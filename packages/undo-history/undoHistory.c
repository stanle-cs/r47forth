// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file undoHistory.c
 * Multi-level undo ring. See undoHistory.h for the model. The ring reuses
 * the single-level machinery at both ends: captures serialize the SAVED_*
 * area that saveForUndo() just committed, and every restore stages an entry
 * back into the SAVED_* area and then runs the existing undo() — the SIGMA,
 * solver-flag and entry-status reconciliation there stays the single source
 * of truth.
 */

#include "c47.h"

#define HISTORY_RING_BYTES        ((uint32_t)TO_BYTES(HISTORY_RING_SIZE_IN_BLOCKS))
// A single state larger than this is not captured: it would evict most of
// the ring for one entry. Skipping is safe (entries are complete states).
#define HISTORY_ENTRY_MAX_BYTES   (HISTORY_RING_BYTES / 4)
#define HISTORY_SUMS_BYTES        (NUMBER_OF_STATISTICAL_SUMS * REAL_SIZE_IN_BYTES(75))
#define HISTORY_CURSOR_NONE       (-1)
#define HISTORY_SLOT_L            8   // SAVED_REGISTER_X + 8 == SAVED_REGISTER_L

typedef struct {
  uint16_t totalBytes;                    ///< header + records (+ sums), rounded up to 8
  uint16_t seq;                           ///< capture sequence number, never 0
  int16_t  labelItem;                     ///< item that ran after this state, 0 = unknown
  uint8_t  flags;                         ///< HISTORY_ENTRY_* bits
  uint8_t  regCount;                      ///< register records that follow (stack + L)
  uint64_t sysFlags0, sysFlags1;
  uint16_t lrSel, lrCho;
} historyEntryHeader_t;

typedef struct {
  uint8_t  slot;                          ///< 0..7 = REGISTER_X offset, 8 = L
  uint8_t  dataType;
  uint8_t  tag;
  uint8_t  unused;
  uint16_t allocParam;                    ///< third argument for reallocateRegister()
  uint16_t payloadBytes;                  ///< bytes copied from/to the register data pointer
} historyRegRecord_t;

// The ring is malloc'd once and never freed — the same heap the C47 pool
// itself comes from (SRAM3 on DMCP5), without entering the pool's BLOCK
// allocator: a resident allocC47Blocks block fragments the pool's contiguous
// space and measurably broke upstream's own 14x14 eigenvalue test (RCL58),
// whose workspace growth is calibrated to the full pool; a
// yield-on-allocation-failure hook was tried and rejected (the freed hole
// does not coalesce with the main free region). A static array is out too:
// on DMCP5 globals live in the 16 KiB SRAM4, which the ring overflows.
static uint8_t  *historyRing = NULL;      // HISTORY_RING_BYTES once armed
static uint32_t  historyUsedBytes = 0;
static uint16_t  historyEntryOffset[HISTORY_MAX_ENTRIES];
static uint8_t   historyEntryCount = 0;
static int16_t   historyCursor = HISTORY_CURSOR_NONE;
static uint16_t  historySeq = 0;
static uint16_t  historyLastCaptureSeq = 0;  // seq of the entry mirroring SAVED_*, 0 = none in ring
static bool_t    historyGapPending = false;  // a capture was skipped since the last stored entry
static int16_t   historyPendingItem = 0;

static uint32_t historyRound4(uint32_t n) {
  return (n + 3u) & ~3u;
}

static uint32_t historyRound8(uint32_t n) {
  return (n + 7u) & ~7u;
}

static void historyHeaderOf(uint8_t index, historyEntryHeader_t *h) {
  xcopy(h, historyRing + historyEntryOffset[index], sizeof(historyEntryHeader_t));
}

static uint16_t historySeqOf(uint8_t index) {
  historyEntryHeader_t h;
  historyHeaderOf(index, &h);
  return h.seq;
}

static void historyEvictOldest(void) {
  historyEntryHeader_t h;
  if(historyEntryCount == 0) {
    return;
  }
  historyHeaderOf(0, &h);
  memmove(historyRing, historyRing + h.totalBytes, historyUsedBytes - h.totalBytes);
  historyUsedBytes -= h.totalBytes;
  for(uint8_t i = 1; i < historyEntryCount; i++) {
    historyEntryOffset[i - 1] = (uint16_t)(historyEntryOffset[i] - h.totalBytes);
  }
  historyEntryCount--;
  if(historyCursor == 0) {
    historyCursor = HISTORY_CURSOR_NONE;
  }
  else if(historyCursor > 0) {
    historyCursor--;
  }
}

static void historyTruncateAbove(int16_t index) {
  historyEntryHeader_t h;
  if(index < 0 || index >= historyEntryCount - 1) {
    return;
  }
  historyHeaderOf((uint8_t)index, &h);
  historyEntryCount = (uint8_t)(index + 1);
  historyUsedBytes = historyEntryOffset[index] + h.totalBytes;
}


static calcRegister_t historySourceRegister(uint8_t slot, bool_t fromSaved) {
  if(fromSaved) {
    return SAVED_REGISTER_X + slot;   // slot 8 lands on SAVED_REGISTER_L
  }
  return slot == HISTORY_SLOT_L ? REGISTER_L : REGISTER_X + slot;
}

static uint16_t historyAllocParamOf(calcRegister_t regist) {
  switch(getRegisterDataType(regist)) {
    case dtLongInteger:     return REGISTER_LONG_INTEGER_HEADER(regist)->dataMaxLengthInBlocks;
    case dtString:          return REGISTER_STRING_HEADER(regist)->dataMaxLengthInBlocks;
    case dtReal34Matrix:    return (uint16_t)TO_BLOCKS((REGISTER_MATRIX_HEADER(regist)->matrixRows * REGISTER_MATRIX_HEADER(regist)->matrixColumns) * REAL34_SIZE_IN_BYTES);
    case dtComplex34Matrix: return (uint16_t)TO_BLOCKS((REGISTER_MATRIX_HEADER(regist)->matrixRows * REGISTER_MATRIX_HEADER(regist)->matrixColumns) * COMPLEX34_SIZE_IN_BYTES);
    default:                return 0;
  }
}

static uint32_t historyPayloadBytesOf(calcRegister_t regist) {
  switch(getRegisterDataType(regist)) {
    case dtReal34Matrix:    return sizeof(matrixHeader_t) + (REGISTER_MATRIX_HEADER(regist)->matrixRows * REGISTER_MATRIX_HEADER(regist)->matrixColumns) * REAL34_SIZE_IN_BYTES;
    case dtComplex34Matrix: return sizeof(matrixHeader_t) + (REGISTER_MATRIX_HEADER(regist)->matrixRows * REGISTER_MATRIX_HEADER(regist)->matrixColumns) * COMPLEX34_SIZE_IN_BYTES;
    default:                return TO_BYTES((uint32_t)getRegisterFullSizeInBlocks(regist));
  }
}

/**
 * Serializes one complete state into the ring. Source is either the SAVED_*
 * area (capture path) or the live machine (LIVEANCHOR at first UNDO).
 *
 * \return 0 = skipped (too large / cannot allocate), 1 = pushed, 2 = merged
 *         into the identical top entry
 */
static int historySerializePush(bool_t fromSaved, int16_t labelItem, uint8_t extraFlags) {
  historyEntryHeader_t h;
  uint8_t regCount, slot;
  calcRegister_t regTop = getStackTop();
  const void *sumsSource = fromSaved ? (const void *)savedStatisticalSumsPointer : (const void *)statisticalSumsPointer;
  uint32_t total, offset;

  regCount = (uint8_t)(regTop - REGISTER_X + 1) + 1;   // stack + L

  // Size pass.
  total = historyRound8(sizeof(historyEntryHeader_t));
  for(uint8_t i = 0; i < regCount; i++) {
    slot = i == regCount - 1 ? HISTORY_SLOT_L : i;
    total += sizeof(historyRegRecord_t) + historyRound4(historyPayloadBytesOf(historySourceRegister(slot, fromSaved)));
  }
  if(sumsSource != NULL) {
    total += HISTORY_SUMS_BYTES;
  }
  total = historyRound8(total);

  if(total > HISTORY_ENTRY_MAX_BYTES || total > 0xffff) {
    return 0;
  }
  while(historyEntryCount >= HISTORY_MAX_ENTRIES || historyUsedBytes + total > HISTORY_RING_BYTES) {
    historyEvictOldest();
  }

  // Write pass, into the free tail. The entry is not committed until the
  // dedupe check below decides it is genuinely new.
  memset(&h, 0, sizeof(h));
  h.totalBytes = (uint16_t)total;
  h.seq        = 0;                       // patched at commit
  h.labelItem  = labelItem;
  h.flags      = (uint8_t)(extraFlags | (sumsSource != NULL ? HISTORY_ENTRY_HAS_SUMS : 0));
  h.regCount   = regCount;
  h.sysFlags0  = fromSaved ? savedSystemFlags0 : systemFlags0;
  h.sysFlags1  = fromSaved ? savedSystemFlags1 : systemFlags1;
  h.lrSel      = fromSaved ? lrSelectionUndo : lrSelection;
  h.lrCho      = sumsSource != NULL ? (fromSaved ? lrChosenUndo : lrChosen) : 0;
  // Deliberately NO display formatting here: ROUND/RSD compute by rendering
  // X into displayValueX and re-parsing it, so the display pipeline is part
  // of upstream's math path — re-entering it mid-capture perturbs live
  // state (surfaced as a wrong SPIRAL program result three test files
  // later). The browser formats entries lazily at render time instead.

  offset = historyUsedBytes + historyRound8(sizeof(historyEntryHeader_t));
  for(uint8_t i = 0; i < regCount; i++) {
    historyRegRecord_t rec;
    calcRegister_t src;
    slot = i == regCount - 1 ? HISTORY_SLOT_L : i;
    src  = historySourceRegister(slot, fromSaved);
    rec.slot         = slot;
    rec.dataType     = (uint8_t)getRegisterDataType(src);
    rec.tag          = (uint8_t)getRegisterTag(src);
    rec.unused       = 0;
    rec.allocParam   = historyAllocParamOf(src);
    rec.payloadBytes = (uint16_t)historyPayloadBytesOf(src);
    xcopy(historyRing + offset, &rec, sizeof(rec));
    offset += sizeof(rec);
    xcopy(historyRing + offset, getRegisterDataPointer(src), rec.payloadBytes);
    // Zero the rounding slack so the byte-level dedupe below never compares
    // stale ring garbage.
    if(historyRound4(rec.payloadBytes) > rec.payloadBytes) {
      memset(historyRing + offset + rec.payloadBytes, 0, historyRound4(rec.payloadBytes) - rec.payloadBytes);
    }
    offset += historyRound4(rec.payloadBytes);
  }
  if(sumsSource != NULL) {
    xcopy(historyRing + offset, sumsSource, HISTORY_SUMS_BYTES);
    offset += HISTORY_SUMS_BYTES;
  }
  if(historyRound8(offset - historyUsedBytes) > offset - historyUsedBytes) {
    memset(historyRing + offset, 0, historyRound8(offset - historyUsedBytes) - (offset - historyUsedBytes));
  }
  xcopy(historyRing + historyUsedBytes, &h, sizeof(h));
  if(historyRound8(sizeof(historyEntryHeader_t)) > sizeof(historyEntryHeader_t)) {
    memset(historyRing + historyUsedBytes + sizeof(historyEntryHeader_t), 0, historyRound8(sizeof(historyEntryHeader_t)) - sizeof(historyEntryHeader_t));
  }

  // Dedupe against the top entry: identical machine state, possibly captured
  // twice (upstream saves again inside some functions, and the capture right
  // after a ring restore reproduces the restored entry). Identity is
  // everything except totalBytes/seq/label and the provenance flags.
  if(historyEntryCount > 0) {
    historyEntryHeader_t top;
    historyHeaderOf(historyEntryCount - 1, &top);
    if(top.totalBytes == h.totalBytes &&
       top.regCount == h.regCount &&
       top.sysFlags0 == h.sysFlags0 && top.sysFlags1 == h.sysFlags1 &&
       top.lrSel == h.lrSel && top.lrCho == h.lrCho &&
       (top.flags & HISTORY_ENTRY_HAS_SUMS) == (h.flags & HISTORY_ENTRY_HAS_SUMS) &&
       memcmp(historyRing + historyEntryOffset[historyEntryCount - 1] + historyRound8(sizeof(historyEntryHeader_t)),
              historyRing + historyUsedBytes + historyRound8(sizeof(historyEntryHeader_t)),
              h.totalBytes - historyRound8(sizeof(historyEntryHeader_t))) == 0) {
      if(top.labelItem == 0 && labelItem != 0) {
        top.labelItem = labelItem;
      }
      top.flags &= (uint8_t)~HISTORY_ENTRY_LIVEANCHOR;   // a merged anchor is a plain state again
      xcopy(historyRing + historyEntryOffset[historyEntryCount - 1], &top, sizeof(top));
      return 2;
    }
  }

  historySeq++;
  if(historySeq == 0) {
    historySeq = 1;
  }
  h.seq = historySeq;
  xcopy(historyRing + historyUsedBytes, &h, sizeof(h));
  historyEntryOffset[historyEntryCount] = (uint16_t)historyUsedBytes;
  historyEntryCount++;
  historyUsedBytes += total;
  return 1;
}

static bool_t historyEnsureRing(void) {
  if(historyRing == NULL) {
    historyRing = malloc(HISTORY_RING_BYTES);
  }
  return historyRing != NULL;
}

void undoHistoryNoteFunction(int16_t item) {
  historyPendingItem = item;
}

void undoHistoryCapture(void) {
  int16_t label = historyPendingItem;
  historyPendingItem = 0;
  if(!historyEnsureRing()) {
    // The state about to change is not captured: same bookkeeping as an
    // oversized skip, and any navigation cursor is stale now.
    historyGapPending = true;
    historyLastCaptureSeq = 0;
    historyCursor = HISTORY_CURSOR_NONE;
    return;
  }
  if(historyCursor != HISTORY_CURSOR_NONE) {
    historyTruncateAbove(historyCursor);
  }
  if(historySerializePush(true, label, historyGapPending ? HISTORY_ENTRY_GAPBEFORE : 0) == 0) {
    historyGapPending = true;
    historyLastCaptureSeq = 0;
  }
  else {
    historyGapPending = false;
    historyLastCaptureSeq = historySeqOf(historyEntryCount - 1);
  }
  historyCursor = HISTORY_CURSOR_NONE;
}

bool_t undoHistoryUserContext(void) {
  return programRunStop != PGM_RUNNING && !getSystemFlag(FLAG_SOLVING) && !getSystemFlag(FLAG_INTING);
}

void undoHistoryNoteFirstUndo(void) {
  int result;
  if(!undoHistoryUserContext() || lastErrorCode != ERROR_NONE) {
    return;   // upstream error-rollback transaction, not a user UNDO
  }
  if(historyRing == NULL || historyEntryCount == 0) {
    return;
  }
  historyCursor = HISTORY_CURSOR_NONE;
  result = historySerializePush(false, 0, HISTORY_ENTRY_LIVEANCHOR);
  if(result == 1) {
    historyCursor = (historyEntryCount >= 2 && historyLastCaptureSeq != 0 && historySeqOf(historyEntryCount - 2) == historyLastCaptureSeq)
                    ? historyEntryCount - 2 : HISTORY_CURSOR_NONE;
  }
  else if(result == 2) {
    historyCursor = (historyLastCaptureSeq != 0 && historySeqOf(historyEntryCount - 1) == historyLastCaptureSeq)
                    ? historyEntryCount - 1 : HISTORY_CURSOR_NONE;
  }
}

/**
 * Stages entry \p index into the SAVED_* area and runs the single-level
 * undo() to make it live.
 *
 * \return true on success
 */
static bool_t historyRestoreToIndex(uint8_t index) {
  historyEntryHeader_t h;
  uint32_t offset;

  if(historyRing == NULL || index >= historyEntryCount) {
    return false;
  }
  historyHeaderOf(index, &h);
  offset = historyEntryOffset[index] + historyRound8(sizeof(historyEntryHeader_t));
  for(uint8_t i = 0; i < h.regCount; i++) {
    historyRegRecord_t rec;
    calcRegister_t dest;
    xcopy(&rec, historyRing + offset, sizeof(rec));
    offset += sizeof(rec);
    dest = SAVED_REGISTER_X + rec.slot;
    reallocateRegister(dest, rec.dataType, rec.allocParam, amNone);
    if(lastErrorCode == ERROR_RAM_FULL) {
      return false;   // ring untouched; nothing consumed the partial staging
    }
    xcopy(getRegisterDataPointer(dest), historyRing + offset, rec.payloadBytes);
    setRegisterTag(dest, rec.tag);
    offset += historyRound4(rec.payloadBytes);
  }

  savedSystemFlags0 = h.sysFlags0;
  savedSystemFlags1 = h.sysFlags1;
  lrSelectionUndo   = h.lrSel;
  lrChosenUndo      = h.lrCho;

  if(h.flags & HISTORY_ENTRY_HAS_SUMS) {
    if(savedStatisticalSumsPointer == NULL) {
      savedStatisticalSumsPointer = allocC47Blocks(NUMBER_OF_STATISTICAL_SUMS * REAL_SIZE_IN_BLOCKS(75));
    }
    if(savedStatisticalSumsPointer == NULL) {
      lastErrorCode = ERROR_RAM_FULL;
      return false;
    }
    xcopy(savedStatisticalSumsPointer, historyRing + offset, HISTORY_SUMS_BYTES);
  }
  else if(savedStatisticalSumsPointer != NULL) {
    freeC47Blocks(savedStatisticalSumsPointer, NUMBER_OF_STATISTICAL_SUMS * REAL_SIZE_IN_BLOCKS(75));
    savedStatisticalSumsPointer = NULL;
  }

  // A cleared TEMP_REGISTER_2_SAVED_STATS makes the recallStatsMatrix() call
  // at the top of undo() a no-op; ring entries never carry a pending
  // matrix-editor stats save.
  clearRegister(TEMP_REGISTER_2_SAVED_STATS);
  SAVED_SIGMA_lastAddRem = SIGMA_NONE;
  thereIsSomethingToUndo = true;
  undo();
  historyCursor = index;
  return true;
}

bool_t undoHistoryCanStepBack(void) {
  if(historyRing == NULL || historyEntryCount == 0) {
    return false;
  }
  return historyCursor == HISTORY_CURSOR_NONE || historyCursor > 0;
}

void undoHistoryStepBack(void) {
  int16_t target;
  if(historyRing == NULL || historyEntryCount == 0) {
    return;
  }
  target = historyCursor == HISTORY_CURSOR_NONE ? historyEntryCount - 1 : historyCursor - 1;
  if(target < 0) {
    return;
  }
  historyRestoreToIndex((uint8_t)target);
}

void fnRedo(uint16_t unusedButMandatoryParameter) {
  if(!undoHistoryUserContext()) {
    return;
  }
  if(historyRing == NULL || historyCursor == HISTORY_CURSOR_NONE || historyCursor >= historyEntryCount - 1) {
    return;
  }
  historyRestoreToIndex((uint8_t)(historyCursor + 1));
}

void fnHistoryClear(uint16_t unusedButMandatoryParameter) {
  historyUsedBytes = 0;
  historyEntryCount = 0;
  historyCursor = HISTORY_CURSOR_NONE;
  historySeq = 0;
  historyLastCaptureSeq = 0;
  historyGapPending = false;
  historyPendingItem = 0;
}

void undoHistoryReset(void) {
  // RESET and state-restore path (restoreCalc funnels through doFnReset):
  // the machine state the entries describe is gone wholesale. The malloc
  // block survives (the heap is not part of the wiped pool) and is reused.
  historyUsedBytes = 0;
  historyEntryCount = 0;
  historyCursor = HISTORY_CURSOR_NONE;
  historySeq = 0;
  historyLastCaptureSeq = 0;
  historyGapPending = false;
  historyPendingItem = 0;
}


#if defined(PC_BUILD)
/* === testSuite coverage drivers ==========================================
 * Registered in testSuite.c's funcTestNoParam with coverageDriver = 1 and
 * driven by testSuite/tests/undo_history.txt. Test-only code: never built
 * for the device. Both drivers start from a cleared ring so every .txt
 * block is self-contained. */

static uint32_t historyTestFailures;

static void historyTestFail(const char *what) {
  historyTestFailures++;
  printf("undoHistory test FAIL: %s\n", what);
}

static void historyTestWriteLonI(calcRegister_t regist, uint32_t value) {
  longInteger_t li;
  longIntegerInit(li);
  uInt32ToLongInteger(value, li);
  convertLongIntegerToLongIntegerRegister(li, regist);
  longIntegerFree(li);
}

static bool_t historyTestIsLonI(calcRegister_t regist, uint32_t expected) {
  longInteger_t li;
  bool_t equal;
  if(getRegisterDataType(regist) != dtLongInteger) {
    return false;
  }
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(regist, li);
  equal = longIntegerCompareUInt(li, expected) == 0;
  longIntegerFree(li);
  return equal;
}

// One known machine baseline for the battery: no pending single-level undo,
// no stale sigma/matrix leftovers from earlier list entries, user context.
static void historyTestBaseline(void) {
  calcMode = CM_NORMAL;
  programRunStop = PGM_STOPPED;
  lastErrorCode = ERROR_NONE;
  thereIsSomethingToUndo = false;
  SAVED_SIGMA_lastAddRem = SIGMA_NONE;
  clearRegister(TEMP_REGISTER_2_SAVED_STATS);
  if(savedStatisticalSumsPointer != NULL) {
    freeC47Blocks(savedStatisticalSumsPointer, NUMBER_OF_STATISTICAL_SUMS * REAL_SIZE_IN_BLOCKS(75));
    savedStatisticalSumsPointer = NULL;
  }
  clearSystemFlag(FLAG_SOLVING);
  clearSystemFlag(FLAG_INTING);
  fnHistoryClear(NOPARAM);
}

void historyTestRing(uint16_t unusedButMandatoryParameter) {
  historyTestFailures = 0;
  historyTestBaseline();

  { // R1: two captures, then step back twice restores the older state.
    real34_t expected;
    historyTestWriteLonI(REGISTER_X, 111);
    reallocateRegister(REGISTER_Y, dtReal34, 0, amNone);
    stringToReal34("2.5", REGISTER_REAL34_DATA(REGISTER_Y));
    setRegisterTag(REGISTER_Y, amDegree);   // tag must survive the round-trip
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 222);
    saveForUndo();
    if(historyEntryCount != 2) {
      historyTestFail("R1 expected 2 entries");
    }
    undoHistoryStepBack();               // cursor NONE -> newest entry {222}
    if(!historyTestIsLonI(REGISTER_X, 222)) {
      historyTestFail("R1 first step-back should land on {222}");
    }
    undoHistoryStepBack();               // -> {111}
    if(!historyTestIsLonI(REGISTER_X, 111)) {
      historyTestFail("R1 second step-back should land on {111}");
    }
    stringToReal34("2.5", &expected);
    if(getRegisterDataType(REGISTER_Y) != dtReal34 || memcmp(REGISTER_REAL34_DATA(REGISTER_Y), &expected, REAL34_SIZE_IN_BYTES) != 0) {
      historyTestFail("R1 Y real34 did not round-trip");
    }
    if(getRegisterTag(REGISTER_Y) != amDegree) {
      historyTestFail("R1 Y angular tag did not round-trip");
    }
    if(historyCursor != 0) {
      historyTestFail("R1 cursor should sit on entry 0");
    }
  }

  { // R2: an identical second capture merges instead of pushing.
    uint8_t before;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 7);
    saveForUndo();
    before = historyEntryCount;
    saveForUndo();                       // same state again
    if(historyEntryCount != before) {
      historyTestFail("R2 dedupe should not grow the ring");
    }
  }

  { // R3: eviction keeps offsets/sizes coherent, oldest gone, ring full.
    uint32_t sum = 0;
    historyTestBaseline();
    for(calcRegister_t regist = REGISTER_X; regist <= getStackTop(); regist++) {
      clearRegister(regist);             // deterministic entry sizes
    }
    clearRegister(REGISTER_L);
    for(uint32_t i = 0; i < HISTORY_MAX_ENTRIES + 12u; i++) {
      historyTestWriteLonI(REGISTER_X, i + 1);
      saveForUndo();
    }
    // Full either by the entry directory or by bytes (entry size depends on
    // register payloads, so the exact count is layout-derived, not pinned).
    if(historyEntryCount != HISTORY_MAX_ENTRIES && historyUsedBytes + 256 <= HISTORY_RING_BYTES) {
      historyTestFail("R3 ring should be full by entries or by bytes");
    }
    if(historySeqOf(0) == 1) {
      historyTestFail("R3 oldest entry should have been evicted");
    }
    for(uint8_t i = 0; i < historyEntryCount; i++) {
      historyEntryHeader_t h;
      if(historyEntryOffset[i] != sum) {
        historyTestFail("R3 offsets must be contiguous");
        break;
      }
      historyHeaderOf(i, &h);
      sum += h.totalBytes;
    }
    if(sum != historyUsedBytes) {
      historyTestFail("R3 sizes must sum to usedBytes");
    }
  }

  { // R4: navigating back then running something new truncates the redo tail;
    // an identical immediate capture merges into the cursor entry instead.
    uint16_t seqTop;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 3);
    saveForUndo();                       // E1{1} E2{2} E3{3}
    seqTop = historySeqOf(2);
    undoHistoryStepBack();               // -> E3
    undoHistoryStepBack();               // -> E2, cursor 1
    if(historyCursor != 1 || !historyTestIsLonI(REGISTER_X, 2)) {
      historyTestFail("R4 setup navigation failed");
    }
    saveForUndo();                       // pre-state == E2 -> truncate + merge
    if(historyEntryCount != 2 || historyCursor != HISTORY_CURSOR_NONE) {
      historyTestFail("R4 capture after navigation should truncate to 2 and drop the cursor");
    }
    historyTestWriteLonI(REGISTER_X, 99);
    saveForUndo();                       // genuinely new -> push
    if(historyEntryCount != 3 || historySeqOf(2) == seqTop) {
      historyTestFail("R4 new capture should replace the dead redo tail");
    }
  }

  { // R5: an oversized state is skipped and the next entry carries GAPBEFORE.
    historyEntryHeader_t h;
    uint8_t before;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    before = historyEntryCount;
    reallocateRegister(REGISTER_X, dtString, TO_BLOCKS(HISTORY_ENTRY_MAX_BYTES + 512), amNone);
    memset(REGISTER_STRING_DATA(REGISTER_X), 'A', HISTORY_ENTRY_MAX_BYTES + 400);
    REGISTER_STRING_DATA(REGISTER_X)[HISTORY_ENTRY_MAX_BYTES + 400] = 0;
    saveForUndo();                       // too large -> skipped
    if(historyEntryCount != before) {
      historyTestFail("R5 oversized capture should be skipped");
    }
    historyTestWriteLonI(REGISTER_X, 5);
    saveForUndo();
    historyHeaderOf(historyEntryCount - 1, &h);
    if(!(h.flags & HISTORY_ENTRY_GAPBEFORE)) {
      historyTestFail("R5 entry after a skip must be flagged GAPBEFORE");
    }
  }

  { // R6: the fnUndo/fnRedo walk — anchor push, cursor algebra, both ends.
    historyEntryHeader_t h;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();                       // E1{1} E2{2}, SAVED_* == E2
    historyTestWriteLonI(REGISTER_X, 3); // live {3}, thereIsSomethingToUndo
    fnUndo(NOPARAM);                     // anchor{3} pushed, single-level undo -> {2}
    if(!historyTestIsLonI(REGISTER_X, 2) || historyEntryCount != 3 || historyCursor != 1) {
      historyTestFail("R6 first UNDO should restore {2} with cursor on E2");
    }
    historyHeaderOf(2, &h);
    if(!(h.flags & HISTORY_ENTRY_LIVEANCHOR)) {
      historyTestFail("R6 top entry must be the live anchor");
    }
    fnUndo(NOPARAM);                     // deeper -> {1}
    if(!historyTestIsLonI(REGISTER_X, 1) || historyCursor != 0) {
      historyTestFail("R6 second UNDO should restore {1}");
    }
    fnUndo(NOPARAM);                     // already oldest -> no-op
    if(!historyTestIsLonI(REGISTER_X, 1)) {
      historyTestFail("R6 UNDO at the oldest entry must hold position");
    }
    fnRedo(NOPARAM);                     // -> {2}
    if(!historyTestIsLonI(REGISTER_X, 2) || historyCursor != 1) {
      historyTestFail("R6 REDO should return to {2}");
    }
    fnRedo(NOPARAM);                     // -> anchor {3}
    if(!historyTestIsLonI(REGISTER_X, 3) || historyCursor != 2) {
      historyTestFail("R6 REDO should return to the pre-undo state");
    }
    fnRedo(NOPARAM);                     // top -> no-op
    if(!historyTestIsLonI(REGISTER_X, 3)) {
      historyTestFail("R6 REDO past the anchor must hold position");
    }
  }

  { // R7: gates. No capture and no ring walk inside SLV context, and the
    // fnUndo(0) error-rollback idiom must not mint an anchor.
    uint8_t before;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();
    before = historyEntryCount;
    thereIsSomethingToUndo = false;
    setSystemFlag(FLAG_SOLVING);
    fnUndo(NOPARAM);                     // solver-internal shape -> must not step into the ring
    fnUndo(NOPARAM);                     // twice: an unguarded ring branch walks deeper on the repeat
    clearSystemFlag(FLAG_SOLVING);
    if(!historyTestIsLonI(REGISTER_X, 2)) {
      historyTestFail("R7 fnUndo under FLAG_SOLVING must not touch the ring");
    }
    historyTestWriteLonI(REGISTER_X, 3);
    thereIsSomethingToUndo = true;       // upstream rollback transaction shape:
    lastErrorCode = ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN;
    fnUndo(NOPARAM);                     // error set -> no anchor, plain single-level undo
    lastErrorCode = ERROR_NONE;
    if(historyEntryCount != before) {
      historyTestFail("R7 error-rollback fnUndo must not mint an anchor");
    }
  }

  historyTestBaseline();                 // leave no half-navigated state behind
  historyTestWriteLonI(REGISTER_X, historyTestFailures);
}

/* Runs a blank-separated script from X: unsigned integer tokens enter a
 * value (saveForUndo, lift, write — the closeNim ordering), any other token
 * is looked up as an item catalog name and executed through
 * reallyRunFunction(), which is the real capture/label path. */
void historyTestSequence(uint16_t unusedButMandatoryParameter) {
  char script[512], token[48];
  uint32_t s = 0, t;

  script[0] = 0;
  COPY_REGISTER_STRING_TO(script, REGISTER_X);
  historyTestBaseline();
  // Deterministic floor: the script string must not linger on the stack and
  // leak into the first capture, and leftover registers from earlier list
  // entries must not either.
  for(calcRegister_t regist = REGISTER_X; regist <= getStackTop(); regist++) {
    clearRegister(regist);
  }
  clearRegister(REGISTER_L);

  while(true) {
    while(script[s] == ' ') {
      s++;
    }
    if(script[s] == 0) {
      break;
    }
    t = 0;
    while(script[s] != ' ' && script[s] != 0 && t < sizeof(token) - 1) {
      token[t++] = script[s++];
    }
    token[t] = 0;

    char *end;
    unsigned long value = strtoul(token, &end, 10);
    if(*end == 0 && end != token) {
      saveForUndo();
      liftStack();
      historyTestWriteLonI(REGISTER_X, (uint32_t)value);
      setSystemFlag(FLAG_ASLIFT);
      continue;
    }

    int16_t item = 0;
    for(int16_t i = 1; i <= LAST_ITEM; i++) {
      if(strcmp(indexOfItems[i].itemCatalogName, token) == 0) {
        item = i;
        break;
      }
    }
    if(item == 0) {
      displayCalcErrorMessage(ERROR_UNDEF_SOURCE_VAR, ERR_REGISTER_LINE, REGISTER_X);
      return;
    }
    reallyRunFunction(item, indexOfItems[item].param);
  }
}
#endif // PC_BUILD
