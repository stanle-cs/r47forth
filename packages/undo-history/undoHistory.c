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

// One resident pool block, allocated at RESET time and never freed — the
// forth-core gdict pattern. The allocation point is load-bearing, not
// style: doFnReset() arms the ring right after the free list is rebuilt,
// so the block sits at the pool's LOW EDGE next to the boot structures and
// cannot split the pool's contiguous middle. A mid-session allocation of
// the very same block lands between live churn and measurably breaks
// upstream's 14x14 eigenvalue test (matrix.txt RCL58), whose QR workspace
// requests one contiguous chunk of essentially the pool's entire free
// space (29820 blocks vs 29825 free, measured) — as do a scattered
// register-bank layout and a yield-under-pressure retry, both tried and
// measured red before this shape (see DESIGN-HISTORY).
static uint8_t  *historyRing = NULL;      // HISTORY_RING_BYTES once armed at reset
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
  // Deliberately NO display formatting here. The display path is not
  // side-effect-free in a capture context: its validation failures call
  // displayBugScreen(), which silently switches calcMode mid-operation
  // (invisible headless — it surfaced as a wrong SPIRAL program result
  // three test files later), ROUND/RSD do real math through the global
  // displayValueX, and part of the family assumes TMP_STR_LENGTH buffers
  // without checking. The browser formats entries lazily at render time,
  // in display context, instead.

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
  // Armed at reset (undoHistoryReset), NEVER lazily: a mid-session
  // allocation loses the low-edge placement — see the storage comment.
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
    // The dropped forward levels never happened: rewind the seq counter so
    // the next capture numbers consecutively after the surviving top (a
    // jump with no ~ would read as a gap the view never marked).
    historySeq = historySeqOf(historyEntryCount - 1);
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
  // RESET path (doFnReset, right after the free list is rebuilt) and the
  // end of a state restore: the machine state the entries described is
  // gone, and so is the pool the old ring block lived in — forget the
  // pointer without freeing, then re-arm from the current pool. At reset
  // the pool is empty, which pins the block to the low edge (see the
  // storage comment); after a state restore the placement is whatever the
  // restored layout allows.
  historyRing = NULL;
  if(isMemoryBlockAvailable(HISTORY_RING_SIZE_IN_BLOCKS, 1, 0.25f)) {
    historyRing = allocC47Blocks(HISTORY_RING_SIZE_IN_BLOCKS);
  }
  historyUsedBytes = 0;
  historyEntryCount = 0;
  historyCursor = HISTORY_CURSOR_NONE;
  historySeq = 0;
  historyLastCaptureSeq = 0;
  historyGapPending = false;
  historyPendingItem = 0;
}



uint8_t undoHistoryDepth(void) {
  return historyEntryCount;
}

int16_t undoHistoryCursorIndex(void) {
  return historyCursor;
}

bool_t undoHistoryLevelInfo(uint8_t logical, uint16_t *seq, int16_t *labelItem, uint8_t *flags) {
  historyEntryHeader_t h;
  if(historyRing == NULL || logical >= historyEntryCount) {
    return false;
  }
  historyHeaderOf(logical, &h);
  *seq       = h.seq;
  *labelItem = h.labelItem;
  *flags     = h.flags;
  return true;
}

bool_t undoHistoryStagePreview(uint8_t logical) {
  historyRegRecord_t rec;
  uint32_t offset;
  uint8_t ecSave;
  if(historyRing == NULL || logical >= historyEntryCount) {
    return false;
  }
  // The X register is the first record of every entry (slots serialize in
  // bank order, X first).
  offset = historyEntryOffset[logical] + historyRound8(sizeof(historyEntryHeader_t));
  xcopy(&rec, historyRing + offset, sizeof(rec));
  offset += sizeof(rec);
  ecSave = lastErrorCode;
  lastErrorCode = ERROR_NONE;
  reallocateRegister(TEMP_REGISTER_1, rec.dataType, rec.allocParam, amNone);
  if(lastErrorCode == ERROR_RAM_FULL) {
    lastErrorCode = ecSave;              // a preview is best-effort, never an error
    return false;
  }
  lastErrorCode = ecSave;
  xcopy(getRegisterDataPointer(TEMP_REGISTER_1), historyRing + offset, rec.payloadBytes);
  setRegisterTag(TEMP_REGISTER_1, rec.tag);
  return true;
}

bool_t undoHistoryKeyReroute(bool_t shiftFActive, int16_t keyPrimary) {
  return shiftFActive && keyPrimary == ITM_UP1 && calcMode == CM_NORMAL && getSystemFlag(FLAG_UHIST);
}

bool_t undoHistoryRestoreLevel(uint8_t logical) {
  if(!undoHistoryUserContext() || historyRing == NULL || logical >= historyEntryCount) {
    return false;
  }
  if(historyCursor == HISTORY_CURSOR_NONE) {
    // A restore from the LIVE state is a jump the user must be able to
    // redo out of: mint the (now) anchor exactly like the first UNDO
    // press. The push can evict oldest levels (or dedupe-merge), so the
    // target is re-found by its seq; if the anchor push evicted it, the
    // restore refuses rather than land on the wrong level.
    uint16_t targetSeq = historySeqOf(logical);
    int16_t found = -1;
    undoHistoryNoteFirstUndo();
    for(uint8_t l = 0; l < historyEntryCount; l++) {
      if(historySeqOf(l) == targetSeq) {
        found = l;
        break;
      }
    }
    if(found < 0) {
      return false;
    }
    logical = (uint8_t)found;
  }
  return historyRestoreToIndex(logical);
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
    // Ruled 2026-08-25 (Stan's report): the replacing capture TAKES the dead
    // tail's number — display numbering stays consecutive across an
    // override, and seqs stay unique within the ring at any instant.
    if(historyEntryCount != 3 || historySeqOf(2) != seqTop) {
      historyTestFail("R4 the replacing capture must take the dead tail's number");
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

  { // R8: storage residency — the ring is ONE pool block armed at reset
    // (the upstream-convention ruling, 2026-08-25: allocC47Blocks, the
    // gdict pattern), it is already armed BEFORE any capture (lazy
    // mid-session arming loses the low-edge placement and breaks RCL58 —
    // measured), and a capture costs the pool nothing.
    const uint8_t *poolLo = (const uint8_t *)ram;
    const uint8_t *poolHi = (const uint8_t *)(ram + RAM_SIZE_IN_BLOCKS);
    historyTestBaseline();
    if(historyRing == NULL) {
      historyTestFail("R8 ring must be armed at reset, before any capture");
    }
    else {
      if((const uint8_t *)historyRing < poolLo || (const uint8_t *)historyRing >= poolHi) {
        historyTestFail("R8 ring block must live inside the C47 pool");
      }
      historyTestWriteLonI(REGISTER_X, 42);
      saveForUndo();
      uint32_t freeBefore = getFreeRamMemory();
      undoHistoryCapture();              // direct: full serialize, dedupe-merge
      if(getFreeRamMemory() != freeBefore) {
        historyTestFail("R8 a capture must not allocate from the pool");
      }
    }
  }

  { // R9: capture purity — serialization must leave the machine surface
    // untouched. The class that broke SPIRAL: a display-formatter call
    // during capture whose validation failure ran displayBugScreen(), which
    // silently flips calcMode (invisible headless; the damage surfaced
    // three test files later). calcMode is asserted here for exactly that
    // reason. This fences the enumerable surface; the suite's own
    // ulp -> programs ordering stays the integration guard for the rest.
    char dvxBefore[DISPLAY_VALUE_LEN];
    realContext_t c34, c39, c75;
    longInteger_t li;
    bool_t udvx;
    uint8_t df, dfd, sd;
    uint32_t am, dm, cm, pcm, ti;
    uint64_t sf0, sf1;
    uint8_t ec;

    historyTestBaseline();
    // Stage the two trigger shapes on the stack: a 49-digit long integer in
    // X (the width that trips longIntegerToAllocatedString's validation
    // when a caller lies about its buffer) and a complex34 in Y.
    longIntegerInit(li);
    uInt32ToLongInteger(9999999u, li);
    longIntegerMultiply(li, li, li);
    longIntegerMultiply(li, li, li);                     // ~28 digits
    longIntegerMultiplyUInt(li, 4000000000u, li);
    longIntegerMultiplyUInt(li, 4000000000u, li);        // ~47-49 digits
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
    longIntegerFree(li);
    reallocateRegister(REGISTER_Y, dtComplex34, 0, amNone);
    stringToReal34("2.5", REGISTER_REAL34_DATA(REGISTER_Y));
    stringToReal34("-1.5", REGISTER_IMAG34_DATA(REGISTER_Y));

    // Snapshot BEFORE the first capture: the window must cover the capture
    // inside saveForUndo() too, or a one-shot state switch there (the
    // displayBugScreen class latches behind its own calcMode guard) escapes
    // the assertions below.
    memset(tmpString, 0x5a, 256);
    memset(errorMessage, 0x5a, 64);
    xcopy(dvxBefore, displayValueX, DISPLAY_VALUE_LEN);
    udvx = updateDisplayValueX;
    df   = displayFormat;
    dfd  = displayFormatDigits;
    sd   = significantDigits;
    am   = (uint32_t)currentAngularMode;
    dm   = denMax;
    sf0  = systemFlags0;
    sf1  = systemFlags1;
    ec   = lastErrorCode;
    cm   = (uint32_t)calcMode;
    pcm  = (uint32_t)previousCalcMode;
    ti   = (uint32_t)temporaryInformation;
    xcopy(&c34, &ctxtReal34, sizeof(realContext_t));
    xcopy(&c39, &ctxtReal39, sizeof(realContext_t));
    xcopy(&c75, &ctxtReal75, sizeof(realContext_t));

    saveForUndo();                       // SAVED_X/SAVED_Y hold the triggers
    undoHistoryCapture();

    for(uint32_t i = 0; i < 256; i++) {
      if(tmpString[i] != 0x5a) {
        historyTestFail("R9 capture wrote into tmpString");
        break;
      }
    }
    for(uint32_t i = 0; i < 64; i++) {
      if(errorMessage[i] != 0x5a) {
        historyTestFail("R9 capture wrote into errorMessage");
        break;
      }
    }
    if(memcmp(dvxBefore, displayValueX, DISPLAY_VALUE_LEN) != 0 || udvx != updateDisplayValueX) {
      historyTestFail("R9 capture touched displayValueX");
    }
    if(df != displayFormat || dfd != displayFormatDigits || sd != significantDigits) {
      historyTestFail("R9 capture touched the display format");
    }
    if(am != (uint32_t)currentAngularMode || dm != denMax) {
      historyTestFail("R9 capture touched angular mode or denMax");
    }
    if(sf0 != systemFlags0 || sf1 != systemFlags1 || ec != lastErrorCode) {
      historyTestFail("R9 capture touched system flags or the error code");
    }
    if(cm != (uint32_t)calcMode || pcm != (uint32_t)previousCalcMode || ti != (uint32_t)temporaryInformation) {
      historyTestFail("R9 capture switched calcMode or temporary information");
    }
    if(memcmp(&c34, &ctxtReal34, sizeof(realContext_t)) != 0 ||
       memcmp(&c39, &ctxtReal39, sizeof(realContext_t)) != 0 ||
       memcmp(&c75, &ctxtReal75, sizeof(realContext_t)) != 0) {
      historyTestFail("R9 capture touched a rounding context");
    }
    errorMessage[0] = 0;                 // drop the sentinels
    tmpString[0] = 0;
  }

  { // R10: the render-path contract stage U2 relies on — worst-case values
    // from the formatters U2's lazy preview will call stay inside a
    // TMP_STR_LENGTH buffer (a 200-byte buffer is smashed regardless of the
    // final string length; see DESIGN-HISTORY). complex34ToDisplayString is
    // deliberately absent: it is the state-perturber (see R9) and only runs
    // in display context, where U2's own tests cover it.
    static char cbuf[TMP_STR_LENGTH + 256];
    longInteger_t li;
    uint8_t df, dfd;
    bool_t guardIntact = true;

    historyTestBaseline();
    updateDisplayValueX = false;
    df  = displayFormat;
    dfd = displayFormatDigits;

    memset(cbuf + TMP_STR_LENGTH, 0x5a, 256);

    reallocateRegister(REGISTER_X, dtShortInteger, 0, 2);     // base 2, WS=64: the widest render
    *REGISTER_SHORT_INTEGER_DATA(REGISTER_X) = 0xffffffffffffffffULL;
    shortIntegerToDisplayString(REGISTER_X, cbuf, false, 0);

    longIntegerInit(li);
    uInt32ToLongInteger(9999999u, li);
    for(int i = 0; i < 40; i++) {
      longIntegerMultiply(li, li, li);   // squares: astronomically many digits capped by maxWidth
      if(longIntegerBits(li) > 4000) {
        break;
      }
    }
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
    longIntegerFree(li);
    longIntegerRegisterToDisplayString(REGISTER_X, cbuf, TMP_STR_LENGTH, 140, 50, false);

    reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
    stringToReal34("1.234567890123456789012345678901234e-6143", REGISTER_REAL34_DATA(REGISTER_X));
    real34ToDisplayString(REGISTER_REAL34_DATA(REGISTER_X), amNone, cbuf, &standardFont, 140, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC);

    for(uint32_t i = 0; i < 256; i++) {
      if(cbuf[TMP_STR_LENGTH + i] != 0x5a) {
        guardIntact = false;
        break;
      }
    }
    if(!guardIntact) {
      historyTestFail("R10 a formatter wrote past TMP_STR_LENGTH");
    }
    if(df != displayFormat || dfd != displayFormatDigits || updateDisplayValueX) {
      historyTestFail("R10 a render-path formatter left display state changed");
    }
  }

  historyTestBaseline();                 // leave no half-navigated state behind
  historyTestWriteLonI(REGISTER_X, historyTestFailures);
}

void historyTestBrowser(uint16_t unusedButMandatoryParameter) {
  historyTestFailures = 0;
  historyTestBaseline();

  { // B1: entry — mode switch, previous mode kept, selection on the newest.
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 3);
    saveForUndo();
    historyBrowser(NOPARAM);
    if(calcMode != CM_HIST_BROWSER || previousCalcMode != CM_NORMAL) {
      historyTestFail("B1 opening must enter CM_HIST_BROWSER from CM_NORMAL");
    }
    if(historyBrowserSelection() != (int16_t)undoHistoryDepth() - 1) {
      historyTestFail("B1 selection must start on the newest level");
    }
  }

  { // B2: navigation clamps at both ends.
    historyBrowserDown();
    historyBrowserDown();
    if(historyBrowserSelection() != 0) {
      historyTestFail("B2 two downs from the top of 3 must reach the oldest");
    }
    historyBrowserDown();
    if(historyBrowserSelection() != 0) {
      historyTestFail("B2 down at the oldest must hold");
    }
    for(int i = 0; i < 5; i++) {
      historyBrowserUp();
    }
    if(historyBrowserSelection() != (int16_t)undoHistoryDepth() - 1) {
      historyTestFail("B2 ups must clamp at the newest");
    }
  }

  { // B3: ENTER restores the selected level and leaves the browser.
    historyBrowserDown();
    historyBrowserDown();                // oldest = pre-{1} capture state
    historyBrowserEnter();
    if(calcMode != CM_NORMAL) {
      historyTestFail("B3 ENTER must leave the browser");
    }
    if(undoHistoryCursorIndex() != 0) {
      historyTestFail("B3 ENTER must move the cursor to the restored level");
    }
  }

  { // B4: reopening starts on the navigation cursor.
    historyBrowser(NOPARAM);
    if(historyBrowserSelection() != 0) {
      historyTestFail("B4 reopening must select the cursor level");
    }
    historyBrowserLeave();
    if(calcMode != CM_NORMAL) {
      historyTestFail("B4 EXIT must return to the previous mode");
    }
  }

  { // B5: the empty browser opens, renders and leaves cleanly.
    historyTestBaseline();
    historyBrowser(NOPARAM);
    if(calcMode != CM_HIST_BROWSER) {
      historyTestFail("B5 empty history must still open the browser");
    }
    historyBrowserEnter();               // nothing to restore: acts as leave
    if(calcMode != CM_NORMAL) {
      historyTestFail("B5 ENTER on empty history must just leave");
    }
  }

  { // B7: the FLAG_UHIST reroute — the SFL offset arithmetic that maps the
    // flag to its SYSFL item (keyboard.c resolves SF/CF menu presses as
    // indexOfItems[(offset & 0x3f) + SFL_MONIT], so the row position IS the
    // flag number), and the key predicate across flag/mode/shift/key.
    bool_t was = getSystemFlag(FLAG_UHIST);
    if(SFL_MONIT + (FLAG_UHIST & 0x3f) != SFL_UHIST || indexOfItems[SFL_UHIST].param != FLAG_UHIST) {
      historyTestFail("B7 FLAG_UHIST must map to its SYSFL row by the SFL_MONIT arithmetic");
    }
    clearSystemFlag(FLAG_UHIST);
    if(undoHistoryKeyReroute(true, ITM_UP1)) {
      historyTestFail("B7 reroute must be off while the flag is clear");
    }
    setSystemFlag(FLAG_UHIST);
    calcMode = CM_NORMAL;
    if(!undoHistoryKeyReroute(true, ITM_UP1)) {
      historyTestFail("B7 flag + f-shift + UP in normal mode must reroute");
    }
    if(undoHistoryKeyReroute(false, ITM_UP1) || undoHistoryKeyReroute(true, ITM_DOWN1)) {
      historyTestFail("B7 reroute must require the f shift and the UP key");
    }
    calcMode = CM_PEM;
    if(undoHistoryKeyReroute(true, ITM_UP1)) {
      historyTestFail("B7 reroute must stay out of program-entry mode");
    }
    calcMode = CM_NORMAL;
    if(was) { setSystemFlag(FLAG_UHIST); } else { clearSystemFlag(FLAG_UHIST); }
  }

  { // B6: the preview staging path delivers the level's X to TEMP_REGISTER_1.
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 77);
    saveForUndo();
    if(!undoHistoryStagePreview(0) || !historyTestIsLonI(TEMP_REGISTER_1, 77)) {
      historyTestFail("B6 preview staging must reproduce the level's X");
    }
  }

  { // B8: the two synthetic labels stay outside the item catalog namespace
    // — an unlabeled row rendering "-" was indistinguishable from ITM_SUB.
    for(uint32_t i = 1; i <= LAST_ITEM; i++) {
      const char *nm = indexOfItems[i].itemCatalogName;
      if(nm != NULL && (strcmp(nm, HISTORY_LABEL_UNLABELED) == 0 ||
                        strcmp(nm, HISTORY_LABEL_ANCHOR) == 0)) {
        historyTestFail("B8 a browser meta label collides with an item catalog name");
        break;
      }
    }
  }

  { // B9: every browser key the post documents must act through the REAL
    // btnPressed/btnReleased chain, not only through direct handler calls —
    // ENTER was wired in fnKeyEnter but swallowed by processKeyAction's
    // browser ignore list, so the battery was green while the key was dead.
    int16_t kEnter = -1, kUp = -1, kDown = -1, kExit = -1;
    for(int16_t k = 0; k < 37; k++) {
      if(kbd_std[k].primary == ITM_ENTER) { kEnter = k; }
      if(kbd_std[k].primary == ITM_UP1)   { kUp    = k; }
      if(kbd_std[k].primary == ITM_DOWN1) { kDown  = k; }
      if(kbd_std[k].primary == ITM_EXIT1) { kExit  = k; }
    }
    if(kEnter < 0 || kUp < 0 || kDown < 0 || kExit < 0) {
      historyTestFail("B9 the active key layout misses a required primary");
    }
    else {
      char kbuf[4];
      GdkEvent ev;                       // the btnClickedP/btnClickedR idiom
      // The chain runs under whatever ~13k earlier suite tests left behind:
      // user key maps, TAM, SHOW, stale temporary info. Pin a defined
      // context, restore what other suites may rely on.
      bool_t hadUser = getSystemFlag(FLAG_USER);
      calcKey_t savedKbd[37];            // earlier assign tests leave kbd_usr
      xcopy(savedKbd, kbd_usr, sizeof(savedKbd));
      xcopy(kbd_usr, kbd_std, sizeof(savedKbd));   // the config.c reset idiom
      ev.button.button = 1;
      historyTestBaseline();
      clearSystemFlag(FLAG_USER);
      tam.mode = 0;
      tam.alpha = false;
      temporaryInformation = TI_NO_INFO;
      lastErrorCode = ERROR_NONE;
      historyTestWriteLonI(REGISTER_X, 1);
      saveForUndo();
      historyTestWriteLonI(REGISTER_X, 2);
      saveForUndo();
      historyTestWriteLonI(REGISTER_X, 3);
      saveForUndo();
      calcMode = CM_NORMAL;
      shiftF = shiftG = false;
      fnTimerExec(TO_FN_EXEC);           // drain a stale queued fn in CM_NORMAL
      if(SHOWMODE || currentMenu() == -MNU_SHOW) {
        closeShowMenu();                 // a latched SHOW screen or menu eats
      }                                  // the first press (btnPressed's own
                                         // dismissal resets calcMode) — on
                                         // device the browser can never
                                         // coexist with SHOW: the entry
                                         // keypress dismisses it first
      historyBrowser(NOPARAM);
      {
        int16_t sel0 = historyBrowserSelection();
        sprintf(kbuf, "%02d", kDown);
        ev.type = 0;
        btnPressed(NULL, &ev, kbuf);
        btnReleased(NULL, &ev, kbuf);
        if(calcMode != CM_HIST_BROWSER || historyBrowserSelection() != sel0 - 1) {
          historyTestFail("B9 DOWN through the real key path must move the selection");
        }
      }
      sprintf(kbuf, "%02d", kEnter);
      ev.type = 0;
      btnPressed(NULL, &ev, kbuf);
      btnReleased(NULL, &ev, kbuf);
      if(calcMode != CM_NORMAL) {
        historyTestFail("B9 ENTER through the real key path must restore and leave");
      }
      if(undoHistoryCursorIndex() != historyBrowserSelection()) {
        historyTestFail("B9 ENTER must move the undo cursor to the restored level");
      }
      historyBrowser(NOPARAM);
      sprintf(kbuf, "%02d", kExit);
      ev.type = 0;
      btnPressed(NULL, &ev, kbuf);
      btnReleased(NULL, &ev, kbuf);
      if(calcMode != CM_NORMAL) {
        historyTestFail("B9 EXIT through the real key path must leave the browser");
      }
      if(hadUser) { setSystemFlag(FLAG_USER); }
      xcopy(kbd_usr, savedKbd, sizeof(savedKbd));
    }
  }

  { // B10: ENTER-restore from the LIVE state must mint the (now) anchor —
    // a browser jump with no undo pressed was unredoable: the pre-restore
    // state was lost and no (now) row appeared.
    uint16_t seq;
    int16_t li;
    uint8_t fl;
    bool_t anchorSeen = false;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 3);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 4);           // live {4}, cursor NONE
    calcMode = CM_NORMAL;
    historyBrowser(NOPARAM);
    historyBrowserDown();                          // an older level
    historyBrowserEnter();                         // restore from LIVE
    for(uint8_t l = 0; l < undoHistoryDepth(); l++) {
      if(undoHistoryLevelInfo(l, &seq, &li, &fl) && (fl & HISTORY_ENTRY_LIVEANCHOR)) {
        anchorSeen = true;
      }
    }
    if(!anchorSeen) {
      historyTestFail("B10 a live-state restore must mint the (now) anchor");
    }
    for(int i = 0; i < 6; i++) {
      fnRedo(NOPARAM);                             // walk back up, extra calls no-op
    }
    if(!historyTestIsLonI(REGISTER_X, 4)) {
      historyTestFail("B10 redo after a live-state restore must reach the pre-restore state");
    }
  }

  { // B11: overriding after an undo keeps the level numbering consecutive —
    // truncate dropped the forward levels but the seq counter kept counting,
    // so the next capture showed a numbering hole with no ~ mark.
    uint16_t seq;
    int16_t li;
    uint8_t fl;
    historyTestBaseline();
    historyTestWriteLonI(REGISTER_X, 1);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 2);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 3);
    saveForUndo();
    historyTestWriteLonI(REGISTER_X, 4);
    fnUndo(NOPARAM);                               // anchor {4}, back on {3}
    historyTestWriteLonI(REGISTER_X, 7);
    reallyRunFunction(ITM_ADD, indexOfItems[ITM_ADD].param);   // the override
    if(undoHistoryDepth() != 4) {
      historyTestFail("B11 override after undo should leave 4 levels");
    }
    else {
      undoHistoryLevelInfo(3, &seq, &li, &fl);     // the fresh top
      uint16_t topSeq = seq;
      undoHistoryLevelInfo(2, &seq, &li, &fl);     // the survivor below it
      if(topSeq != (uint16_t)(seq + 1)) {
        historyTestFail("B11 numbering must stay consecutive across an override");
      }
      if(fl & HISTORY_ENTRY_GAPBEFORE) {
        historyTestFail("B11 an override is not a gap");
      }
    }
  }

  { // B12: unhandled keys are IGNORED while the browser is shown, the
    // upstream browser blanket — a digit or shifted item leaking through
    // executes against the machine under the browser.
    char kbuf[4];
    GdkEvent ev;
    calcKey_t savedKbd2[37];
    int16_t kDigit = -1;
    xcopy(savedKbd2, kbd_usr, sizeof(savedKbd2));
    xcopy(kbd_usr, kbd_std, sizeof(savedKbd2));
    for(int16_t k = 0; k < 37; k++) {
      if(kbd_std[k].primary == ITM_5) { kDigit = k; }
    }
    ev.button.button = 1;
    historyTestBaseline();
    clearSystemFlag(FLAG_USER);
    tam.mode = 0;
    tam.alpha = false;
    temporaryInformation = TI_NO_INFO;
    fnTimerExec(TO_FN_EXEC);
    if(SHOWMODE || currentMenu() == -MNU_SHOW) {
      closeShowMenu();
    }
    historyTestWriteLonI(REGISTER_X, 42);
    saveForUndo();
    calcMode = CM_NORMAL;
    shiftF = shiftG = false;
    historyBrowser(NOPARAM);
    if(kDigit >= 0) {
      sprintf(kbuf, "%02d", kDigit);
      ev.type = 0;
      btnPressed(NULL, &ev, kbuf);
      btnReleased(NULL, &ev, kbuf);
      if(calcMode != CM_HIST_BROWSER) {
        historyTestFail("B12 a digit key must not act while the browser is shown");
      }
      if(!historyTestIsLonI(REGISTER_X, 42)) {
        historyTestFail("B12 a leaked key changed the machine under the browser");
      }
    }
    historyBrowserLeave();
    calcMode = CM_NORMAL;
    xcopy(kbd_usr, savedKbd2, sizeof(savedKbd2));
  }

  historyTestBaseline();
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
