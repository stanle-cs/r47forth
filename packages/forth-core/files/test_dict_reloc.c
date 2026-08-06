/*
 * test_dict_reloc.c -- Dictionary relocation self-test
 * Per DESIGN.md §7
 *
 * Forces reallocC47Blocks with a tiny initial region, defines words
 * that exceed the initial size, then confirms all names resolve
 * post-move via region-relative links.
 *
 * Toothed mutation tests (§7 inner interpreter):
 *   stack, branch (fwd/back/0BR), literal, div-by-zero,
 *   rstack overflow, runaway guard, malformed token.
 */

/* FORTH test harness — PC debug build only.
 * Cross-target (DMCP) must not include fork/waitpid/printf or the
 * PC_BUILD-only test helpers (forthDictSetTestInitialBlocks,
 * forthTestSetDepth/GetDepth). */
#if defined(PC_BUILD) && defined(FORTH_DEBUG_SELFTEST)

#include <string.h>
#include <signal.h>   /* SIGALRM: the fork test turns a child hang into a signal */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "c47.h"
#include "forth_dict.h"
#include "forth_capture.h"
#include "forth_console.h"
#include "forth_menu.h"
#include "programming/param_core.h"
#include "saveRestoreBackup.h"

/* ---- Token constants (mirror forth_inner.c §2.2) ---- */

#define T_EXIT         0x0000
#define T_PRIM_BASE    0x0001
#define T_CALL_BASE    0x1000
#define T_LIT          0x7F00
#define T_ILIT         0x7F01
#define T_BR           0x7F02
#define T_0BR          0x7F03
#define T_C47          0x7F04
#define T_XEQN         0x7F05

/* ---- Program-fixture builder types (needed early by test_xeqn) ---- */

#define TP_MAX_BYTES 1536
#define TP_MAX_STEPS 64

typedef enum {
  TP_STEP_LBL,
  TP_STEP_MARKER,
  TP_STEP_SRC,
  TP_STEP_XEQ_NAME,
  TP_STEP_OP1,
  TP_STEP_PARAM,
  TP_STEP_RAW
} tpStepKind_t;

typedef struct testProg {
  uint8_t  bytes[TP_MAX_BYTES];
  uint16_t len;
  uint8_t  stepCount;
  bool     failed;
  uint16_t stepOff[TP_MAX_STEPS];
  tpStepKind_t stepKind[TP_MAX_STEPS];
} testProg_t;

/* Forward declarations for tp* helpers (defined later) */
static void tpInit(testProg_t *);
static int tpLbl(testProg_t *, const char *);
static int tpSrc(testProg_t *, const char *);
static int tpEnd(testProg_t *);
static int tpLblLocal(testProg_t *, const char *);
static int tpXeqLocal(testProg_t *, const char *);
static int tpRtn(testProg_t *);
static bool tpWrite(const testProg_t *);
static uint8_t *tpStepAddr(const testProg_t *, int);
static bool tpSelectStep(const testProg_t *, int);
static int tpStepParam(testProg_t *, uint16_t func, const uint8_t *param, uint16_t nParam);

/* ---- Primitive indices (mirror forth_prims.c) ---- */

enum { P_DUP = 0, P_DROP = 1, P_SWAP = 2, P_OVER = 3,
  P_PLUS = 4, P_MINUS = 5, P_MUL = 6, P_DIV = 7 };

#define PRIM_TOKEN(idx) ((ftoken_t)((idx) + T_PRIM_BASE))

/* ---- Helpers ---- */

static void define_word(const char *name, uint8_t nameLen)
{
  uint16_t off = forthDictAllocate(nameLen, 0);
  if (off == FORTH_NULL) return;
  forthDictWriteName(off, name, nameLen);
  forthDictEmit(T_EXIT);
  forthDictFinishDef(fdict.latest);
}

/* Begin a word; caller emits tokens, then calls end_word(off). */
static uint16_t begin_word(const char *name, uint8_t nameLen)
{
  uint16_t off = forthDictAllocate(nameLen, 0);
  if (off == FORTH_NULL) return FORTH_NULL;
  forthDictWriteName(off, name, nameLen);
  return off;
}

/* Emit EXIT token and finish the word at entryOff. */
static void end_word(uint16_t entryOff)
{
  forthDictEmit(T_EXIT);
  forthDictFinishDef(entryOff);
}

/* Emit a little-endian int32 at fdict.here */
static void emit_int32(int32_t v)
{
  forthDictEmitBytes(&v, 4);
}

/* Emit a little-endian int16 at fdict.here */
static void emit_int16(int16_t v)
{
  forthDictEmitBytes(&v, 2);
}

/* ---- gdict test builders (F3-2) ---- */

static uint16_t gbegin_word(const char *name, uint8_t nameLen)
{
  uint16_t alignedHdr = (uint16_t)TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK;
  if (!forthGDictEnsure(alignedHdr)) return FORTH_NULL;
  uint16_t off = gdict.here;
  forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
  hdr->link = gdict.latest; hdr->flags = 0; hdr->nameLen = nameLen;
  hdr->owner = FORTH_OWNER_GLOBAL;
  memcpy(gdict.base + off + 6, name, nameLen);
  for (uint16_t i = off + 6 + nameLen; i < off + alignedHdr; i++) gdict.base[i] = 0;
  gdict.here = (uint16_t)(off + alignedHdr);
  gdict.latest = off; gdict.count++;
  return off;
}

static bool gemit(ftoken_t t)
{
  if (!forthGDictEnsure(2)) return false;
  memcpy(gdict.base + gdict.here, &t, 2); gdict.here += 2; return true;
}

static bool gemit_bytes(const uint8_t *buf, uint16_t len)
{
  if (!forthGDictEnsure(len)) return false;
  memcpy(gdict.base + gdict.here, buf, len); gdict.here += len; return true;
}

static void gend_word(void)
{
  gemit(T_EXIT);
  gdict.here = (uint16_t)TO_BLOCKS(gdict.here) * BYTES_PER_BLOCK;
}

/* Run a word by name; returns true if lastErrorCode != ERROR_NONE */
static bool run_word(const char *name)
{
  uint16_t idx;
  lastErrorCode = ERROR_NONE;
  if (forthFindColon(name, &idx)) {
    forthInner(idx, false);
  }
  return lastErrorCode != ERROR_NONE;
}

/* Check if X register holds a long integer equal to val; returns 1 if so */
static int x_is_longint(int32_t val)
{
  if (getRegisterDataType(REGISTER_X) != dtLongInteger) return 0;
  longInteger_t li;
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
  int32_t v;
  longIntegerToInt32(li, v);
  longIntegerFree(li);
  return v == val;
}

/* Check if Y register holds a long integer equal to val; returns 1 if so */
static int y_is_longint(int32_t val)
{
  if (getRegisterDataType(REGISTER_Y) != dtLongInteger) return 0;
  longInteger_t li;
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_Y, li);
  int32_t v;
  longIntegerToInt32(li, v);
  longIntegerFree(li);
  return v == val;
}


/* ==================================================================
 * Toothed mutation tests  --  DESIGN.md §7
 * Each test: PASS normally, FAIL under its named mutation.
 * ================================================================== */



















/* ---- R1-2: truncated inline operands ----
 * forthInner read the next token and every inline LIT/ILIT/branch/C47 operand
 * directly from fdict.base with no proof the bytes lie below fdict.here. A
 * restored word whose logical end falls immediately after one of these tokens
 * could read beyond the dictionary instead of raising
 * ERROR_INVALID_CORRUPTED_DATA. Each subcase below constructs a word whose
 * fdict.here ends exactly where the guard should catch it — no forthDictEmit
 * call for the operand, and no end_word (which would append T_EXIT and make
 * the body well-formed again). forthDictFinishDef block-rounds fdict.here, so
 * "immediately after" the token can carry up to BYTES_PER_BLOCK-1 zeroed
 * padding bytes; the guard must still reject when fewer than the required
 * byte count remain even with that rounding. */








/* ==================================================================
 * Sub-phase C acceptance tests  --  DESIGN.md §7.4
 * Exercise forthTestRunFromX (the one-shot interpret-from-X core), not
 * forthOuterInterpret: these cases are written against a source line that
 * ARRIVES IN X and is consumed, so their stack expectations depend on the
 * drop.  Before Stage L this core was fnForthOuter itself; L-R2 made that
 * item entry a capture opener (see PACKET_L1_0).
 * ================================================================== */

/* Helper: store a C string as a dtString in REGISTER_X */
static void x_set_string(const char *s)
{
  int32_t len = (int32_t)strlen(s);
  reallocateRegister(REGISTER_X, dtString, TO_BLOCKS(len + 1), amNone);
  xcopy(REGISTER_STRING_DATA(REGISTER_X), s, len + 1);
}




/* Forward declarations for test-program helpers (used below, defined later) */
static bool writeTestProgram(const uint8_t *bytes, uint16_t n);
static void cleanupTestProgram(void);






/* Forward declarations for test-program helpers (defined later in file) */
static bool writeTestProgram(const uint8_t *bytes, uint16_t n);
static void cleanupTestProgram(void);





/* ==================================================================
 * Main self-test entry point
 * ================================================================== */

/* ==================================================================
 * H1 acceptance tests  --  DESIGN.md §7.1 / §7.5
 * ================================================================== */




/* §7.1 re-entrancy: depth cap fires at FORTH_NEST_MAX and recovers (§3.2)
 * Uses test-only forthTestSetDepth to prime the guard, avoiding
 * reallyRunFunction/display calls that may not be safe in headless mode.
 * Requires FORTH_DEBUG_SELFTEST (meson OPTION). */
#ifdef FORTH_DEBUG_SELFTEST
#endif /* FORTH_DEBUG_SELFTEST */









/* ---- Lifecycle tests: init/reset safety (fix #2 + #11) ----
 * Mutation #2: remove fdict.base guard in forthFindColon -> NULL deref.
 * Mutation #11: remove forthDictInit() from reset -> stale pointers. ---- */





/* ---- Error-handling tests: fix #7 (dict-space / name errors) ----
 * Mutation #7a: remove error display from startDefinition -> silent failure,
 *   ASLIFT set as success.
 * Mutation #7b: remove error display from forthDictEnsure -> silent failure. ---- */







/* ---- Fix #8: prefix-match bug (SQ vs SQUARE) ----
 * Mutation #8: remove queryLen == hdr->nameLen check -> SQUARE matches SQ. ---- */



/* ---- Fix #9: number grammar (signed exponents, reject mantissa-less e) ----
 * Mutation #9a: revert mantissaDigits tracking -> e5/.e5/3e classify as REAL -> NaN.
 * Mutation #9b: reject sign after e/E -> 1e-5 errors as undefined. ---- */








/* ---- Fix #10: UNDO rows US_ENABLED ----
 * Mutation #10: revert US_ENABLED to US_UNCHANGED -> no undo snapshot. ---- */




/* ---- COMMIT 3: Program-step, run-generation, name-by-index tests ---- */
/* (build_payload helper retired with Architecture 2: forthProgramStep's
 * contract requires payloads inside real programs — see the migrated
 * tests below.) */






















/* ---- COMMIT 4: executeOneStep ITM_FORTH arm tests ---- */

/* read_reg_int32 — R2-T4 item 4 helper: type + int32 value of a long-integer
 * stack register, so a marker-noop test can snapshot all four RPN registers
 * without repeating longInteger_t boilerplate four times over. Test-local
 * only; no production counterpart. */
static void read_reg_int32(int reg, uint8_t *type, int32_t *val)
{
  *type = getRegisterDataType(reg);
  if (*type == dtLongInteger) {
    longInteger_t li;
    longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(reg, li);
    longIntegerToInt32(li, *val);
    longIntegerFree(li);
  } else {
    *val = 0;
  }
}

/* seedParamParityState — F2-5: reset RPN stack and execution state
 * to a known baseline before each native/Forth NUMBER_16 dispatch,
 * so the parity comparison starts from identical observable input. */
static void seedParamParityState(void)
{
  forthPushInt32(11);
  forthPushInt32(22);
  forthPushInt32(33);
  forthPushInt32(44);
  lastErrorCode = ERROR_NONE;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
}




/* probeListPtrs — silent tripwire (was a temporary debug probe):
 * For each of labelList and programList, if non-NULL, check whether
 * TO_C47MEMPTR(ptr) falls inside any freeMemoryRegions[] entry — i.e. a
 * stale list pointer left dangling into freed arena space, the harness
 * precondition violation class this suite once suffered from. Prints
 * NOTHING when the invariant holds; on violation it prints the offending
 * pointer/region and latches probeListPtrsViolation, which fails the
 * suite in forthDictSelfTest. */
static int probeListPtrsViolation = 0;

static void probeListPtrs(const char *tag)
{
  uint16_t lblAddr = 0, prgAddr = 0;
  bool_t lblInFree = false, prgInFree = false;
  int32_t lblRegion = -1, prgRegion = -1;

  if (labelList != NULL) {
    lblAddr = TO_C47MEMPTR(labelList);
    for (int32_t i = 0; i < numberOfFreeMemoryRegions; i++) {
      uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
      uint32_t rEnd = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
      if ((uint32_t)lblAddr >= rStart && (uint32_t)lblAddr < rEnd) {
        lblInFree = true;
        lblRegion = i;
        break;
      }
    }
  }

  if (programList != NULL) {
    prgAddr = TO_C47MEMPTR(programList);
    for (int32_t i = 0; i < numberOfFreeMemoryRegions; i++) {
      uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
      uint32_t rEnd = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
      if ((uint32_t)prgAddr >= rStart && (uint32_t)prgAddr < rEnd) {
        prgInFree = true;
        prgRegion = i;
        break;
      }
    }
  }

  if (lblInFree) {
    printf("  FAIL: [PROBE %s] labelList=%p addr=%u IN FREE REGION %d [%u..%u)\n",
    tag, (void *)labelList, (unsigned)lblAddr, (int)lblRegion,
    (unsigned)freeMemoryRegions[lblRegion].blockAddress,
    (unsigned)(freeMemoryRegions[lblRegion].blockAddress + freeMemoryRegions[lblRegion].sizeInBlocks));
    probeListPtrsViolation = 1;
  }
  if (prgInFree) {
    printf("  FAIL: [PROBE %s] programList=%p addr=%u IN FREE REGION %d [%u..%u)\n",
    tag, (void *)programList, (unsigned)prgAddr, (int)prgRegion,
    (unsigned)freeMemoryRegions[prgRegion].blockAddress,
    (unsigned)(freeMemoryRegions[prgRegion].blockAddress + freeMemoryRegions[prgRegion].sizeInBlocks));
    probeListPtrsViolation = 1;
  }
}

/* ---- COMMIT 5: §8.4 derived-state helpers + test-program infrastructure ---- */

/* writeTestProgram: expand program memory if needed, write bytes, append
 * .END. sentinel, fix bookkeeping, re-scan. Returns true on success.
 * restoreTestProgram: restore the pristine empty program at the original
 * beginOfProgramMemory location and re-scan.
 *
 * Reset-time program memory layout (INVESTIGATION RESULTS):
 *   beginOfProgramMemory = ram + (RAM_SIZE_IN_BLOCKS - 1)  [last block of RAM]
 *   Minimal valid program: [ITM_END_hi|0x80][ITM_END_lo][0xFF][0xFF]
 *     = [0x85][0xB2][0xFF][0xFF]  (ITM_END=1458=0x05B2)
 *   firstFreeProgramByte = beginOfProgramMemory + 2  (points to .END.)
 *   freeProgramBytes = 0  (set by config.c before scan)
 *   After scanLabelsAndPrograms: firstFreeProgramByte recomputed to .END.,
 *   freeProgramBytes = (ram+RAM_SIZE_IN_BLOCKS - firstFreeProgramByte) - 2
 *   programList[0] = { step:1, instructionPointer: beginOfProgramMemory }
 *   numberOfPrograms = 1
 *   Default program memory = 1 block (4 bytes) — too small for test programs,
 *   so writeTestProgram expands by moving beginOfProgramMemory backwards into
 *   the free-memory region, and adjusts freeMemoryRegions[0] accordingly.
 */

static uint8_t *testProgOrigBegin;   /* saved for restoreTestProgram */
static uint16_t testProgOrigFreeSize; /* saved freeMemoryRegions[0].sizeInBlocks */

/* FIX-6: Production-API restore — avoids hand-editing freeMemoryRegions[]
 * which caused 49 overlap warnings and 7 accounting errors per run.
 *
 * Mutation target (kept in #if 0 for test): the old region-surgery approach
 * below directly manipulates freeMemoryRegions[0].sizeInBlocks and compacts
 * the array, creating overlaps when intermediate free regions from test
 * allocations fall inside the restored region.  Re-enabling this block
 * causes test_freelist_consistent to FAIL and freeList.c diagnostics to
 * reappear in the log. */
#if 0
static void restoreTestProgram_OLD_REGION_SURGERY(void)
{
  *(testProgOrigBegin + 0) = (ITM_END >> 8) | 0x80;
  *(testProgOrigBegin + 1) =  ITM_END       & 0xff;
  *(testProgOrigBegin + 2) = 0xFF;
  *(testProgOrigBegin + 3) = 0xFF;

  beginOfProgramMemory = testProgOrigBegin;
  currentStep = beginOfProgramMemory;
  firstFreeProgramByte = beginOfProgramMemory + 2;
  freeProgramBytes = 0;
  freeMemoryRegions[0].sizeInBlocks =
  TO_C47MEMPTR(testProgOrigBegin) - freeMemoryRegions[0].blockAddress;
  { uint32_t end0 = (uint32_t)freeMemoryRegions[0].blockAddress +
    (uint32_t)freeMemoryRegions[0].sizeInBlocks;
    int32_t keep = 1;
    for (int32_t _i = 1; _i < numberOfFreeMemoryRegions; _i++) {
      uint32_t rStart = (uint32_t)freeMemoryRegions[_i].blockAddress;
      if (rStart >= end0) {
        freeMemoryRegions[keep] = freeMemoryRegions[_i];
        keep++;
      }
    }
    numberOfFreeMemoryRegions = keep;
  }
  scanLabelsAndPrograms();
}
#endif

static void restoreTestProgram(void)
{
  if (!testProgOrigBegin) {
    scanLabelsAndPrograms();
    return;
  }

  /* Write pristine 4-byte empty program at current beginOfProgramMemory */
  beginOfProgramMemory[0] = (ITM_END >> 8) | 0x80;
  beginOfProgramMemory[1] =  ITM_END       & 0xff;
  beginOfProgramMemory[2] = 0xFF;
  beginOfProgramMemory[3] = 0xFF;

  /* Shrink program memory to minimal 1 block via production API.
   * This moves beginOfProgramMemory forward and adds the delta to the
   * last free region — keeps the free list consistent. currentStep is set
   * AFTER the resize: beginOfProgramMemory moves, so setting it earlier
   * left currentStep dangling at the old (now-freed) location until the
   * caller happened to overwrite it. */
  resizeProgramMemory(1);
  currentStep = beginOfProgramMemory;

  /* Re-scan labels and programs; recomputes firstFreeProgramByte and
   * freeProgramBytes from the program bytes (manage.c:184-185). */
  probeListPtrs("restoreTestProgram");
  scanLabelsAndPrograms();
}

static void cleanupTestProgram(void)
{
  /* Free dict BEFORE restoring the program region.
   * If dict is freed after restoreTestProgram collapses free
   * regions, the dict's allocation falls inside the restored
   * region 0 and triggers a false-positive overlap warning. */
  if (fdict.base) {
    freeC47Blocks(fdict.base, fdict.sizeBlocks);
  }
  fdict.base = NULL;
  fdict.sizeBlocks = 0;
  fdict.here = 0;
  fdict.latest = FORTH_NULL;
  fdict.count = 0;
  forthScanTrackReset();

  restoreTestProgram();

  /* Reset saved state so a future writeTestProgram re-captures */
  testProgOrigBegin = NULL;
  testProgOrigFreeSize = 0;
}

static bool writeTestProgram(const uint8_t *bytes, uint16_t n)
{
  if (!bytes || n == 0) return false;

  uint16_t neededBytes = n + 2;  /* program + .END. */
  uint16_t neededBlocks = TO_BLOCKS(neededBytes);

  uint16_t currentSizeBlocks =
  RAM_SIZE_IN_BLOCKS - TO_C47MEMPTR(beginOfProgramMemory);

  /* Save original state for restore */
  testProgOrigBegin = beginOfProgramMemory;
  testProgOrigFreeSize = freeMemoryRegions[0].sizeInBlocks;

  if (neededBlocks > currentSizeBlocks) {
    /* Safety: don't overlap data region */
    uint16_t maxBlocks = RAM_SIZE_IN_BLOCKS - (uint16_t)freeMemoryRegions[0].blockAddress - 1;
    if (neededBlocks > maxBlocks) {
      return false;
    }
    /* Use the production API — keeps the free list consistent */
    resizeProgramMemory(neededBlocks);
  }

  memcpy(beginOfProgramMemory, bytes, n);
  beginOfProgramMemory[n]     = 0xFF;
  beginOfProgramMemory[n + 1] = 0xFF;
  firstFreeProgramByte = beginOfProgramMemory + n;
  freeProgramBytes = ((uint8_t *)(ram + RAM_SIZE_IN_BLOCKS) - firstFreeProgramByte) - 2;
  scanLabelsAndPrograms();
  return true;
}

/* ---- Program-fixture builder (2026-07-18) ------------------------------
 * Hand-computed byte offsets in fixtures caused five defect cycles (the
 * F1-1 +3/+4 pointer, the F1-5 P0 payload length, the F15-4 expected
 * image, the F2-1 fixture drive, the F2-2 +24-for-+26 segfault). Fixtures
 * are now BUILT: each step append records its offset, payload lengths come
 * from strlen, and step addresses are QUERIED via tpStepAddr — never
 * computed by hand. tpRaw() is the sole escape hatch for a deliberate
 * malformation or exact encoding assertion. New tests must use this builder
 * (packet authoring rule); existing tests are not migrated opportunistically. */

static const uint8_t tpForthPrefix[3] = {
  0x8B, 0x1A, STRING_LABEL_VARIABLE
};

static void tpInit(testProg_t *p)
{
  memset(p, 0, sizeof(*p));
}

static int tpReject(testProg_t *p, const char *reason)
{
  p->failed = true;
  printf("    FIXTURE BUG: %s\n", reason);
  return -1;
}

static int tpAppend(testProg_t *p, const uint8_t *b, uint16_t n,
                    tpStepKind_t kind)
{
  if (p->failed) {
    return -1;
  }
  if (b == NULL || n == 0 || n > TP_MAX_BYTES - p->len ||
      p->stepCount >= TP_MAX_STEPS) {
    return tpReject(p, "tp append rejected");
  }
  p->stepOff[p->stepCount] = p->len;
  p->stepKind[p->stepCount] = kind;
  memcpy(p->bytes + p->len, b, n);
  p->len += n;
  return p->stepCount++;
}

static int tpLbl(testProg_t *p, const char *name)          /* LBL 'name' */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpLbl name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpLbl name"); }
  s[0] = ITM_LBL; s[1] = STRING_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_LBL);
}

static int tpMarker(testProg_t *p)                         /* »FORTH / FORTH« */
{
  uint8_t m[4];
  memcpy(m, tpForthPrefix, sizeof(tpForthPrefix));
  m[3] = 0x00;
  return tpAppend(p, m, 4, TP_STEP_MARKER);
}

static int tpSrc(testProg_t *p, const char *src)           /* ITM_FORTH source step */
{
  uint8_t s[4 + 64];
  size_t n;
  if (src == NULL) { return tpReject(p, "tpSrc payload"); }
  n = strlen(src);
  if (n == 0 || n > 64) { return tpReject(p, "tpSrc payload"); }
  memcpy(s, tpForthPrefix, sizeof(tpForthPrefix));
  s[3] = (uint8_t)n;
  memcpy(s + 4, src, n);
  return tpAppend(p, s, (uint16_t)(4 + n), TP_STEP_SRC);
}

static int tpXeqName(testProg_t *p, const char *name)      /* XEQ 'name' step */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpXeqName name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpXeqName name"); }
  s[0] = ITM_XEQ; s[1] = STRING_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_XEQ_NAME);
}

static int tpOp1(testProg_t *p, uint8_t opByte)            /* one-byte opcode; new callers use named ITM_* constants */
{
  return tpAppend(p, &opByte, 1, TP_STEP_OP1);
}

/* Native parameterized step: opcode bytes (1 or 2 per the func<128 rule)
 * followed by the given parameter bytes verbatim. */
static int tpStepParam(testProg_t *p, uint16_t func,
                       const uint8_t *param, uint16_t nParam)
{
  uint8_t s[2 + 40];
  uint16_t n = 0;
  if (param == NULL || nParam == 0 || nParam > 40) {
    return tpReject(p, "tpStepParam param");
  }
  if (func < 128) {
    s[n++] = (uint8_t)func;
  } else {
    s[n++] = (uint8_t)((func >> 8) | 0x80);
    s[n++] = (uint8_t)(func & 0xff);
  }
  memcpy(s + n, param, nParam);
  return tpAppend(p, s, (uint16_t)(n + nParam), TP_STEP_PARAM);
}

static int tpEnd(testProg_t *p)                            /* ITM_END separator */
{
  uint8_t s[2];
  s[0] = (ITM_END >> 8) | 0x80;
  s[1] =  ITM_END       & 0xff;
  return tpAppend(p, s, 2, TP_STEP_OP1);
}

static int tpLblLocal(testProg_t *p, const char *name)     /* LBL :name: */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpLblLocal name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpLblLocal name"); }
  s[0] = ITM_LBL; s[1] = LOCAL_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_LBL);
}

static int tpXeqLocal(testProg_t *p, const char *name)     /* XEQ :name: */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpXeqLocal name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpXeqLocal name"); }
  s[0] = ITM_XEQ; s[1] = LOCAL_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_XEQ_NAME);
}

static int tpRtn(testProg_t *p)                           /* ITM_RTN = 0x04 */
{
  const uint8_t op = 0x04;
  return tpAppend(p, &op, 1, TP_STEP_OP1);
}

static int tpRaw(testProg_t *p, const uint8_t *b, uint16_t n) /* deliberate malformation or encoding assertion ONLY */
{
  return tpAppend(p, b, n, TP_STEP_RAW);
}

static bool tpWrite(const testProg_t *p)
{
  if (p->failed || p->len == 0 || p->stepCount == 0) {
    printf("    FIXTURE BUG: refusing to write invalid fixture\n");
    return false;
  }
  return writeTestProgram(p->bytes, p->len);
}

static void tpUseAuthoredEnd(const testProg_t *p)
{
  /* scanLabelsAndPrograms truncates firstFreeProgramByte at a malformed
   * tail. Tests of bounded consumers need the builder's authored end as
   * their exclusive bound, while retaining the scan's valid prefix data. */
  firstFreeProgramByte = beginOfProgramMemory + p->len;
  freeProgramBytes = ((uint8_t *)(ram + RAM_SIZE_IN_BLOCKS) - firstFreeProgramByte) - 2;
}

static uint8_t *tpStepAddr(const testProg_t *p, int idx)   /* valid after tpWrite */
{
  if (idx < 0 || idx >= p->stepCount) {
    printf("    FIXTURE BUG: tpStepAddr(%d) out of range\n", idx);
    return NULL;
  }
  return beginOfProgramMemory + p->stepOff[idx];
}

static bool tpSelectStep(const testProg_t *p, int idx)
{
  uint8_t *addr = tpStepAddr(p, idx);
  if (addr == NULL) {
    printf("    FIXTURE BUG: tpSelectStep could not locate captured step\n");
    return false;
  }
  currentStep = addr;
  defineCurrentProgramFromCurrentStep();
  uint8_t *cur = beginOfCurrentProgram;
  uint16_t stepNum = 0;
  while (cur != NULL && cur < addr) {
    cur = findNextStep(cur);
    stepNum++;
  }
  if (cur == NULL || cur != addr) {
    printf("    FIXTURE BUG: tpSelectStep could not locate captured step\n");
    return false;
  }
  currentLocalStepNumber = stepNum + 1;
  return true;
}

static uint8_t *tpSrcPayload(const testProg_t *p, int idx) /* -> the LENGTH byte (the forthProgramStep +3 contract) */
{
  uint8_t *step = tpStepAddr(p, idx);
  if (step == NULL) {
    return NULL;
  }
  if (p->stepKind[idx] != TP_STEP_SRC) {
    printf("    FIXTURE BUG: tpSrcPayload(%d) is not a source step\n", idx);
    return NULL;
  }
  return step + 3;
}



/* ---- COMMIT 6: manage.c override — toggle, in-region, FCALL redirect ---- */






/* ---- COMMIT 7: manage.c slice 2 — E3 empty-commit, E5 EDIT, cursor ---- */






/* COMMIT 10: MNU_FORTH registered as dynamic softmenu #22, NUMBER_OF_DYNAMIC_SOFTMENUS == 23.
 * Escaping mutation: bump NUMBER_OF_DYNAMIC_SOFTMENUS without inserting the softmenu[] and
 * dynamicSoftmenu[] rows — TAMFLAG (index 22) is misclassified as dynamic, renders empty. */
_Static_assert(NUMBER_OF_DYNAMIC_SOFTMENUS == 23, "NUMBER_OF_DYNAMIC_SOFTMENUS must be 23 (P-H6)");



/* ---- COMMIT 11: softmenus.c slice 2 — the : NAME picker builder ---- */

extern void testInitVariableSoftmenu(int16_t menu);






/* ---- COMMIT 12: keyboard.c — picker insert at cursor ---- */












/* FIX-3 tests: restrict Forth fallback to XEQ/XEQ.SKP only */









/* FIX-6: free-list integrity check */
static int test_freelist_consistent(void);

/* FIX-6: double-free guard tests (range-overlap guard in freeListFree) */
static int test_freelist_double_free_guarded(void);
static int test_freelist_interior_double_free(void);
static int test_freelist_no_mutation_on_oversize_free(void);

/* F5: alpha menu presentation tests */
static int test_alpha_menu_on_top_during_capture(void);
static int test_alpha_menu_contains_fwrd(void);

/* A8: real-keyboard-path regression tests */
static int test_forth_toggle_from_catalog_leaves_alpha_menu(void);
static int test_forth_drain_clears_buried_catalog(void);
static int test_forth_capture_survives_keystroke(void);
static int test_forth_alpha_gesture_resumes_forth(void);

/* C-13: end-of-line error precedence tests */
static int test_unterminated_def_errors(void);
static int test_overlong_token_in_def_keeps_error(void);

/* F2-1: parameter core extraction test */
static int test_param_core_extraction(void);

/* F2-2: bounded name reader test */
static int test_param_core_bounded_names(void);

/* F2-3: shared direct dispatch parity test */
static int test_c47_param_shared_dispatch(void);

/* F2-4: native/Forth parameter parity acceptance sweep */
static int test_param_parity_sweep(void);

/* F4-1: parameter classification + direct numeric parameters */
static int test_param_textual_numeric(void);
static int test_param_register_flag(void);
/* F4-3: named, system-flag, and indirect parameter forms */
static int test_param_named_indirect(void);
/* F4-4: Series C error table and native/Forth parity acceptance */
static int test_param_series_c_acceptance(void);
/* F5-1: check mode — the tokenizer validates its own grammar */
static int test_check_source_line(void);
/* F5-2: E9 commit gate — structural rejects at ENTER, advisory commits */
static int test_commit_gate(void);
/* F6-1: managed capture buffer behind the capture object */
static int test_capture_buffer(void);
/* F6-2: TAM suspend/resume keeps capture alive */
static int test_capture_suspend(void);
/* F6-3: catalogs and menus during capture */
static int test_capture_menus(void);
/* F6-4: parameter entry emits canonical text */
static int test_capture_param_text(void);
/* F6-5: the dictionary-backed word catalog */
static int test_word_catalog(void);
/* F6-6: capture acceptance battery */
static int test_capture_acceptance(void);
/* SB-1: sim bench, capture mechanics + cancel edges (charter A2-A6, F1, F2) */
static int test_sim_bench_capture(void);
static int test_sim_bench_nesting(void);
/* code-audit: dynamic-menu XEQ of a Forth word/colon must insert in PEM, not execute live */
static int test_pem_xeq_dynmenu_no_live_exec(void);
/* code-audit (adversarial): edit an existing Forth line, MODIFY it, re-commit via ENTER */
static int test_forth_edit_modify_commit(void);
/* code-audit: PEM Up/Down must commit the managed Forth sink before moving */
static int test_forth_capture_navigation(void);
/* complete user-facing language showcase from FORTH_SHOWCASE_PROGRAM.txt */
static int test_showcase_program(void);
static int test_savings_program(void);
static int test_native_lift_after_forth(void);
static int test_data_stack_overflow_guard(void);
static int test_deep_recursion_spill(void);
static int test_spill_native_boundary(void);
static int test_spill_window_parity(void);
static int test_spill_region(void);

/* ---- Pillar 1 (H5) backup-file helpers ---- */
#define TEST_BACKUP_NAME (CALCMODEL == USER_C47 ? "backup.cfg" : "backupR47.cfg")

static char *savedBackupBytes = NULL;
static long  savedBackupLen   = -1;   /* -1: file did not exist */

static void preserveBackupFile(void)
{
  FILE *f = fopen(TEST_BACKUP_NAME, "rb");
  if (!f) { savedBackupLen = -1; return; }
  fseek(f, 0, SEEK_END);
  savedBackupLen = ftell(f);
  fseek(f, 0, SEEK_SET);
  savedBackupBytes = malloc((size_t)savedBackupLen);
  if (savedBackupBytes) {
    if (fread(savedBackupBytes, 1, (size_t)savedBackupLen, f) != (size_t)savedBackupLen) {
      free(savedBackupBytes); savedBackupBytes = NULL; savedBackupLen = -1;
    }
  }
  fclose(f);
}

static void restoreBackupFile(void)
{
  if (savedBackupLen < 0) { remove(TEST_BACKUP_NAME); return; }
  FILE *f = fopen(TEST_BACKUP_NAME, "wb");
  if (f) {
    fwrite(savedBackupBytes, 1, (size_t)savedBackupLen, f);
    fclose(f);
  }
  free(savedBackupBytes); savedBackupBytes = NULL; savedBackupLen = -1;
}

/* returns 1 if the backup file contains a line starting with `prefix` */
static int backupFileContains(const char *prefix)
{
  FILE *f = fopen(TEST_BACKUP_NAME, "r");
  char line[256];
  int found = 0;
  if (!f) return 0;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, prefix, strlen(prefix)) == 0) { found = 1; break; }
  }
  fclose(f);
  return found;
}

/* Rewrite the backup file: drop lines starting with dropPrefix (may be NULL),
 * and/or replace the whole line starting with replPrefix by replLine (may be
 * NULL). Returns 0 on success. */
static int editBackupFile(const char *dropPrefix, const char *replPrefix, const char *replLine)
{
  FILE *in = fopen(TEST_BACKUP_NAME, "r");
  FILE *out = in ? fopen("backup.tmp.cfg", "w") : NULL;
  char line[4096];
  if (!in || !out) { if (in) fclose(in); return 1; }
  while (fgets(line, sizeof(line), in)) {
    if (dropPrefix && strncmp(line, dropPrefix, strlen(dropPrefix)) == 0) continue;
    if (replPrefix && strncmp(line, replPrefix, strlen(replPrefix)) == 0) {
      fputs(replLine, out);
      continue;
    }
    fputs(line, out);
  }
  fclose(in); fclose(out);
  remove(TEST_BACKUP_NAME);
  return rename("backup.tmp.cfg", TEST_BACKUP_NAME);
}








/* T5-2 reader-side step accessors (PROGRAM-FIXTURE RULE, inspection
 * clause): tests never hand-index step bytes — layout facts live here
 * and in tpSrcPayload only. */
static bool_t stepIsForthStep(const uint8_t *step) {
  return step && step[0] == 0x8B && step[1] == 0x1A && step[2] == 0xFD;
}

static bool_t stepIsMarker(const uint8_t *step) {
  return stepIsForthStep(step) && step[3] == 0;
}

static bool_t stepSrcTextEq(const uint8_t *step, const char *expected) {
  uint8_t len = (uint8_t)strlen(expected);
  return stepIsForthStep(step) && step[3] == len
      && memcmp(step + 4, expected, len) == 0;
}



/* T5 split: forward declarations for the tests that now live in the
 * .part.h include-parts (see the parts' banner comments). */
static int test_literal_after_lit(void);
static int test_c47_ptp_number8_padded(void);
static int test_nested_preserves_outer_rstack(void);
static int test_outer_real_literal(void);
static int test_outer_simple_expr(void);
static int test_outer_compile_invoke(void);
static int test_outer_item_lookup(void);
static int test_outer_nonstring_x(void);
static int test_outer_glyph_cross(void);
static int test_outer_glyph_dot(void);
static int test_outer_glyph_divide(void);
static int test_outer_nesting_tokenizer(void);
static int test_outer_depth_cap(void);
static int test_outer_ctx_at_rest(void);
static int test_tam_dispatcher(void);
static int test_tam_colon_never_falls_to_forth(void);
static int test_number_then_no_label_fallthrough(void);
static int test_number_1e_minus_5(void);
static int test_number_bad_e5(void);
static int test_number_bad_dot_e5(void);
static int test_number_bad_3e(void);
static int test_number_bad_exponent_sign_position(void);
static int test_number_bad_lone_dot(void);
static int test_dynamic_menu_registration(void);
static int test_static_menu_integrity(void);
static int test_picker_scan_basic(void);
static int test_picker_omits_long_names(void);
static int test_picker_rebuilds_same_menu(void);
static int test_picker_capacity_boundary(void);
static int test_picker_dedupes(void);
static int test_picker_insert_at_cursor(void);
static int test_picker_key_mapping(void);
static int test_picker_scan_and_alloc(void);
static int test_picker_renders_labels(void);
static int test_picker_pixel_layout(void);
static int test_picker_insert_mid_line(void);
static int test_picker_trailing_space(void);
static int test_picker_guard_menu_identity(void);
static int test_picker_glyph_tokenize(void);
static int test_picker_long_token_skipped(void);
static int test_softmenu_trailing_null(void);
static int test_tam_function_cleared_after_abort(void);
static int test_restore_missing_params_defaults(void);
static int test_alpha_menu_on_top_during_capture(void);
static int test_alpha_menu_contains_fwrd(void);
static int test_forth_toggle_from_catalog_leaves_alpha_menu(void);
static int test_forth_capture_survives_keystroke(void);
static int test_forth_alpha_gesture_resumes_forth(void);
static int test_param_core_extraction(void);
static int test_param_core_bounded_names(void);
static int test_c47_param_shared_dispatch(void);
static int test_param_parity_sweep(void);
static int test_param_textual_numeric(void);
static int test_param_register_flag(void);
static int test_param_named_indirect(void);
static int test_param_series_c_acceptance(void);
static int test_capture_buffer(void);
static int test_capture_suspend(void);
static int test_capture_menus(void);
static int test_capture_param_text(void);
static int test_capture_acceptance(void);
static int test_forth_capture_navigation(void);
static int test_sim_bench_capture(void);
static int test_sim_bench_nesting(void);
static int test_pem_xeq_dynmenu_no_live_exec(void);
static int test_forth_toggle_close_with_open_capture(void);   /* FIX-8 */
static int test_capture_close_paths_reset_tuple(void);        /* FIX-8 class */
static int test_capture_origin_lifecycle(void);               /* L1-1 */
static int test_capture_interactive_close(void);              /* L1-1 */
static int test_capture_interactive_repl(void);                /* L1-2 */
static int test_capture_interactive_divert(void);              /* L1-3 */
static int test_history_program(void);                        /* L1-H */
static int test_fold_context(void);                            /* L1-F1 */
static int test_fold_seams(void);                              /* L1-F2 */
static int test_fold_operand_parity(void);                     /* L1-F3 */
static int test_fold_close_paths(void);                        /* L1-F3 */
static int test_cm_gate_audit(void);                           /* L1-F3 */
static int test_interactive_acceptance(void);                 /* L1-5 */
static int test_interactive_close_sweep(void);                /* L1-5 */
static int test_interactive_residue(void);                    /* L1-5 */
static int test_fwrd_normal_mode(void);                       /* M1-1 */
static int test_fwrd_assign(void);                            /* M1-2 */
static int test_fwrd_late_binding(void);                      /* M1-3 */
static int test_forth_fold_commit_recompiles(void);           /* FIX-7 */
static int test_quote_glyph_accept_parity(void);              /* FIX-7 class */
static int test_resume_drains_buried_catalog(void);           /* FIX-9 */
static int test_keys_mode_resolution(void);                   /* K1 */
static int test_keys_mode_toggle_arm(void);                   /* K1 */
static int test_keys_mode_nav_guards(void);                   /* K1 */
static int test_insert_token_boundary(void);                  /* K2 */
static int test_keys_digits_then_function(void);              /* K2 */
static int test_exit_ladder_keys_rung(void);                  /* K2 */
static int test_keys_eex_and_numlock(void);                   /* K2 */
static int test_keys_tam_roundtrip(void);                     /* K3 */
static int test_alpha_tam_roundtrip_unchanged(void);          /* K3 */
static int test_abandon_clears_keys_bit(void);                /* K3 */
static int test_k4_mixed_input_definition(void);              /* K4 */
static int test_k4_keys_only_line(void);                      /* K4 */
static int test_k4_relock_submode(void);                      /* K4 */
static int test_k4_ladder_full_unwind(void);                  /* K4 */
static int test_k4_arena_sweep(void);                         /* K4 */


/* T5 split: forward declarations for the tests that now live in the
 * .part.h include-parts (see the parts' banner comments). */
static int test_stack_aslift(void);
static int test_branch_fwd(void);
static int test_branch_back(void);
static int test_0br_longint(void);
static int test_0br_consumes(void);
static int test_0br_longint_taken_branch(void);
static int test_lit_roundtrip(void);
static int test_c47_ptp_none(void);
static int test_c47_bad_ptp(void);
static int test_c47_nested_call_succeeds(void);
static int test_nested_error_unwinds_rsp(void);
static int test_div_zero_halt(void);
static int test_rstack_overflow(void);
static int test_runaway_guard(void);
static int test_truncated_token_fetch(void);
static int test_truncated_inline_operand(void);
static int test_truncated_c47_item_id(void);
static int test_malformed_token(void);
static int test_ilit_sign_extend(void);
static int test_ilit_arithmetic_divergence(void);
static int test_br_delta_sign_extend(void);
static int test_ilit_compile_interpret_parity(void);
static int test_fnforthcall_executes_colon_by_index(void);
static int test_reentrancy(void);
static int test_xeq_precedence(void);
static int test_xeq_item_lookup(void);
static int test_xeqn(void);
static int test_xeqn_acceptance(void);
static int test_fnforthcall_interactive(void);
static int test_lblq_forth_name_not_local_label(void);
static int test_lifecycle_pre_init(void);
static int test_lifecycle_reset(void);
static int test_lifecycle_real_reset_hook(void);
static int test_dict_name_too_long(void);
static int test_dict_space_full(void);
static int test_dict_first_ensure_capacity(void);
static int test_dict_capacity_arithmetic(void);
static int test_dict_name_by_index(void);
static int test_prefix_no_match(void);
static int test_undo_rows_us_enabled(void);
static int test_forth_step_ptp_rem(void);
static int test_forth_step_sizing(void);
static int test_program_step_define_and_use(void);
static int test_program_step_gen_reset(void);
static int test_pending_reset_lifetime(void);
static int test_run_entry_lifetime_signaling(void);
static int test_prescan_forward_reference(void);
static int test_prescan_no_early_tail(void);
static int test_prescan_no_recompile(void);
static int test_prescan_owning_scope(void);
static int test_owning_program_start_bounds(void);
static int test_owning_program_start_max_not_last(void);
static int test_prescan_generation_rearm(void);
static int test_prescan_error_halts(void);
static int test_prescan_error_rolls_back_prior_defs(void);
static int test_prescan_last_step_visible(void);
static int test_prescan_two_programs_first_touch(void);
static int test_scan_dynamic_no_cliff(void);
static int test_recurse_compile_only(void);
static int test_validate_restored_bodies(void);
static int test_accept_run_lifecycle(void);
static int test_accept_entry_state_roundtrip(void);
static int test_accept_display_parity(void);
static int test_accept_glyph_type_parity(void);
static int test_accept_xeq_name_step(void);
static int test_exec_step_marker_noop(void);
static int test_exec_step_source_runs(void);
static int test_exec_step_halts_on_error(void);
static int test_marker_parity(void);
static int test_placeholder_never_marker(void);   /* §8.1 class test, 2026-08-04 */
static int test_entry_state_derivation(void);
static int test_toggle_inserts_marker(void);
static int test_forth_toggle_close_resets_sentinel(void);
static int test_fcall_redirect_records_name(void);
static int test_fcall_redirect_rejects_stale(void);
static int test_forth_empty_enter_leaves_no_step(void);
static int test_forth_edit_extracts_source(void);
static int test_decode_marker_directions(void);
static int test_decode_source_bare(void);
static int test_mnu_forth_row(void);
static int test_program_memory_no_overlap(void);
static int test_cleanup_no_overlap(void);
static int test_e2_continuation_after_enter(void);
static int test_e2_not_inside_rpn_gap(void);
static int test_gto_word_errors(void);
static int test_gto_item_errors(void);
static int test_xeq_word_still_calls(void);
static int test_useritem_xeqp1_opcode(void);
static int test_useritem_xeqp1_decodes(void);
static int test_e1_direction_mid_program(void);
static int test_forth_multiline_lock_holds(void);
static int test_save_restore_roundtrip(void);
static int test_restore_validation_clamps(void);
static int test_validate_direct_corruption(void);
static int test_scope_isolation(void);
static int test_global_marks(void);
static int test_control_flow(void);
static int test_spill_region(void);
static int test_forth_drain_clears_buried_catalog(void);
static int test_freelist_consistent(void);
static int test_freelist_double_free_guarded(void);
static int test_freelist_interior_double_free(void);
static int test_freelist_no_mutation_on_oversize_free(void);
static int test_unterminated_def_errors(void);
static int test_overlong_token_in_def_keeps_error(void);
static int test_commit_gate(void);
static int test_check_source_line(void);
static int test_word_catalog(void);
static int test_forth_edit_modify_commit(void);
static int test_showcase_program(void);
static int test_data_stack_overflow_guard(void);
static int test_deep_recursion_spill(void);
static int test_spill_native_boundary(void);
static int test_spill_window_parity(void);
static int test_forth_run_from_x_brackets(void);
static int test_native_lift_after_forth(void);
static int test_savings_program(void);
/* N1-1: the console view ring */
static int test_console_ring_basic(void);
static int test_console_ring_partial(void);
static int test_console_ring_evict(void);
static int test_console_ring_linecap(void);
static int test_console_ring_glyph(void);
static int test_console_ring_clear(void);
static int test_console_ring_view(void);
static int test_console_ring_reset_seam(void);
static int test_console_ring_hammer(void);

int forthDictSelfTest(void)
{
  int fail = 0;
  static int suiteEntryCount = 0;
  suiteEntryCount++;

  printf("FORTH DICT SELF-TEST: relocation test\n");

  /* Force a tiny initial region: 4 blocks = 16 bytes. */
  forthDictSetTestInitialBlocks(4);

  forthDictInit();

  /* FIX-6: snapshot allocated regions at suite start (§freeList diagnostics) */
  int32_t allocRegionsStart = numberOfAllocatedMemoryRegions;

  const char *words[] = { "A", "B", "C", "D", "E" };
  uint8_t     lens[]  = { 1,   1,   1,   1,   1  };
  int numWords = 5;

  uint8_t *baseBefore = NULL;
  uint8_t *baseAfter  = NULL;
  int relocObserved = 0;

  define_word(words[0], lens[0]);
  baseBefore = fdict.base;
  printf("  After '%s': base=%p, here=%u, size=%u blocks, latest=%u\n",
  words[0], (void *)fdict.base, fdict.here, fdict.sizeBlocks, fdict.latest);

  for (int i = 1; i < numWords; i++) {
    define_word(words[i], lens[i]);
    printf("  After '%s': base=%p, here=%u, size=%u blocks, latest=%u\n",
    words[i], (void *)fdict.base, fdict.here, fdict.sizeBlocks, fdict.latest);
  }

  baseAfter = fdict.base;

  if (baseAfter != baseBefore) {
    relocObserved = 1;
    printf("  PASS: relocation detected (base moved from %p to %p)\n",
    (void *)baseBefore, (void *)baseAfter);
  } else {
    printf("  WARN: no relocation observed (base unchanged at %p)\n", (void *)baseBefore);
  }

  /* Fix #14: gate on relocObserved — §7.2 requires "grow across a move". */
  if (!relocObserved) {
    fail = 1;
  }

  printf("  Resolving names post-move:\n");
  for (int i = 0; i < numWords; i++) {
    uint16_t idx;
    if (forthFindColon(words[i], &idx)) {
      printf("    '%s' -> index %u  PASS\n", words[i], idx);
    } else {
      printf("    '%s' -> NOT FOUND  FAIL\n", words[i]);
      fail = 1;
    }
  }

  {
    int chainCount = 0;
    uint16_t off = fdict.latest;
    while (off != FORTH_NULL && chainCount < 100) {
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      if (!(hdr->flags & FF_SMUDGE)) {
        chainCount++;
      }
      off = hdr->link;
    }
    if (chainCount == numWords) {
      printf("  PASS: link chain has %d unsmudged entries (expected %d)\n",
      chainCount, numWords);
    } else {
      printf("  FAIL: link chain has %d unsmudged entries (expected %d)\n",
      chainCount, numWords);
      fail = 1;
    }
  }

  if (fdict.count == (uint16_t)numWords) {
    printf("  PASS: fdict.count = %u (expected %d)\n", fdict.count, numWords);
  } else {
    printf("  FAIL: fdict.count = %u (expected %d)\n", fdict.count, numWords);
    fail = 1;
  }

  if (fail) {
    printf("FORTH DICT SELF-TEST: FAILED\n");
  } else {
    printf("FORTH DICT SELF-TEST: PASSED%s\n",
    relocObserved ? " (with relocation)" : " (no relocation triggered)");
  }

  forthDictClear();

  /* ==================================================================
   * Lifecycle tests  --  fix #2 + #11 (init/reset safety)
   * ================================================================== */
  printf("\nFORTH LIFECYCLE TESTS (init/reset safety)\n");
  printf("  [DEBUG] running test_lifecycle_pre_init...\n");
  fail |= test_lifecycle_pre_init();
  printf("  [DEBUG] running test_lifecycle_reset...\n");
  fail |= test_lifecycle_reset();
  printf("  [DEBUG] running test_lifecycle_real_reset_hook...\n");
  fail |= test_lifecycle_real_reset_hook();

  forthDictClear();

  /* ==================================================================
   * H1 acceptance tests  --  DESIGN.md §7.1 / §7.5
   * Run BEFORE mutation tests to avoid heap corruption from double-free.
   * ================================================================== */
  printf("\nFORTH H1 ACCEPTANCE TESTS (XEQ / re-entrancy / precedence)\n");
  forthDictInit();

  printf("  [DEBUG] running test_fnforthcall_executes_colon_by_index...\n");
  fail |= test_fnforthcall_executes_colon_by_index();
  printf("  [DEBUG] running test_tam_dispatcher...\n");
  fail |= test_tam_dispatcher();
  printf("  [DEBUG] running test_tam_colon_never_falls_to_forth...\n");
  fail |= test_tam_colon_never_falls_to_forth();
#ifdef FORTH_DEBUG_SELFTEST
  printf("  [DEBUG] running test_reentrancy...\n");
  fail |= test_reentrancy();
#endif
  printf("  [DEBUG] running test_xeq_precedence...\n");
  fail |= test_xeq_precedence();
  printf("  [DEBUG] running test_xeq_item_lookup...\n");
  fail |= test_xeq_item_lookup();
  printf("  [DEBUG] running test_xeqn...\n");
  fail |= test_xeqn();
  printf("  [DEBUG] running test_xeqn_acceptance...\n");
  fail |= test_xeqn_acceptance();
  printf("  [DEBUG] running test_fnforthcall_interactive...\n");
  fail |= test_fnforthcall_interactive();
  printf("  [DEBUG] running test_lblq_forth_name_not_local_label...\n");
  fail |= test_lblq_forth_name_not_local_label();

  forthDictClear();

  /* ==================================================================
   * Toothed mutation tests  --  DESIGN.md §7
   * ================================================================== */
  printf("\nFORTH INNER INTERPRETER SELF-TEST (toothed mutation tests)\n");
  forthDictInit();

  fail |= test_stack_aslift();
  fail |= test_branch_fwd();
  fail |= test_branch_back();
  fail |= test_0br_longint();
  fail |= test_0br_consumes();
  fail |= test_0br_longint_taken_branch();
  fail |= test_literal_after_lit();
  fail |= test_lit_roundtrip();
  fail |= test_c47_ptp_none();
  fail |= test_c47_ptp_number8_padded();
  fail |= test_c47_bad_ptp();
  fail |= test_c47_nested_call_succeeds();
  printf("  [DEBUG] running test_nested_preserves_outer_rstack...\n");
  fail |= test_nested_preserves_outer_rstack();
  printf("  [DEBUG] running test_nested_error_unwinds_rsp...\n");
  fail |= test_nested_error_unwinds_rsp();
  fail |= test_div_zero_halt();
  fail |= test_rstack_overflow();
  fail |= test_runaway_guard();
  fail |= test_malformed_token();
  fail |= test_truncated_token_fetch();
  fail |= test_truncated_inline_operand();
  fail |= test_truncated_c47_item_id();
  fail |= test_ilit_sign_extend();
  fail |= test_ilit_arithmetic_divergence();
  fail |= test_br_delta_sign_extend();

  forthDictClear();

  /* ==================================================================
   * Sub-phase C acceptance tests  --  DESIGN.md §7.4
   * ================================================================== */
  printf("\nFORTH SUB-PHASE C ACCEPTANCE TESTS (fnForthOuter)\n");
  forthDictInit();

  printf("  [DEBUG] running test_outer_simple_expr...\n");
  fail |= test_outer_simple_expr();
  printf("  [DEBUG] running test_outer_compile_invoke...\n");
  fail |= test_outer_compile_invoke();
  printf("  [DEBUG] running test_outer_item_lookup...\n");
  fail |= test_outer_item_lookup();
  printf("  [DEBUG] running test_outer_nonstring_x...\n");
  fail |= test_outer_nonstring_x();
  printf("  [DEBUG] running test_ilit_compile_interpret_parity...\n");
  fail |= test_ilit_compile_interpret_parity();
  printf("  [DEBUG] running test_outer_real_literal...\n");
  fail |= test_outer_real_literal();
  printf("  [DEBUG] running test_outer_glyph_cross...\n");
  fail |= test_outer_glyph_cross();
  printf("  [DEBUG] running test_outer_glyph_dot...\n");
  fail |= test_outer_glyph_dot();
  printf("  [DEBUG] running test_outer_glyph_divide...\n");
  fail |= test_outer_glyph_divide();
  printf("  [DEBUG] running test_outer_nesting_tokenizer...\n");
  fail |= test_outer_nesting_tokenizer();
  printf("  [DEBUG] running test_outer_depth_cap...\n");
  fail |= test_outer_depth_cap();
  printf("  [DEBUG] running test_outer_ctx_at_rest...\n");
  fail |= test_outer_ctx_at_rest();

  forthDictClear();

  /* ==================================================================
   * Error-handling + grammar + undo tests  --  fix #7, #8, #9, #10
   * ================================================================== */
  printf("\nFORTH ERROR/GRAMMAR/UNDO TESTS (fix #7-#10)\n");
  forthDictInit();

  printf("  [DEBUG] running test_dict_name_too_long...\n");
  fail |= test_dict_name_too_long();
  printf("  [DEBUG] running test_dict_space_full...\n");
  fail |= test_dict_space_full();

  printf("  [DEBUG] running test_dict_first_ensure_capacity...\n");
  fail |= test_dict_first_ensure_capacity();

  printf("  [DEBUG] running test_dict_capacity_arithmetic...\n");
  fail |= test_dict_capacity_arithmetic();

  printf("  [DEBUG] running test_number_then_no_label_fallthrough...\n");
  fail |= test_number_then_no_label_fallthrough();
  forthDictClear();

  printf("  [DEBUG] running test_prefix_no_match...\n");
  fail |= test_prefix_no_match();
  forthDictClear();

  printf("  [DEBUG] running test_number_1e_minus_5...\n");
  fail |= test_number_1e_minus_5();
  forthDictClear();
  printf("  [DEBUG] running test_number_bad_e5...\n");
  fail |= test_number_bad_e5();
  forthDictClear();
  printf("  [DEBUG] running test_number_bad_dot_e5...\n");
  fail |= test_number_bad_dot_e5();
  forthDictClear();
  printf("  [DEBUG] running test_number_bad_3e...\n");
  fail |= test_number_bad_3e();
  forthDictClear();
  printf("  [DEBUG] running test_number_bad_exponent_sign_position...\n");
  fail |= test_number_bad_exponent_sign_position();
  forthDictClear();
  printf("  [DEBUG] running test_number_bad_lone_dot...\n");
  fail |= test_number_bad_lone_dot();
  forthDictClear();
  printf("  [DEBUG] running test_undo_rows_us_enabled...\n");
  fail |= test_undo_rows_us_enabled();

  /* Fix #15: double-free in test_xeq_precedence is fixed; no need to
   * skip the final free anymore. */
  forthDictClear();

  /* COMMIT 2: P-1 representation — ITM_FORTH is PTP_REM */
  printf("  [DEBUG] running test_forth_step_ptp_rem...\n");
  fail |= test_forth_step_ptp_rem();
  printf("  [DEBUG] running test_forth_step_sizing...\n");
  fail |= test_forth_step_sizing();

  /* COMMIT 3: Program-step entry, run-generation, name-by-index */
  printf("\nFORTH COMMIT 3 TESTS (program-step / run-gen / name-by-index)\n");
  forthDictInit();

  printf("  [DEBUG] running test_program_step_define_and_use...\n");
  fail |= test_program_step_define_and_use();
  forthDictClear();

  printf("  [DEBUG] running test_program_step_gen_reset...\n");
  fail |= test_program_step_gen_reset();
  forthDictClear();

  printf("  [DEBUG] running test_pending_reset_lifetime...\n");
  fail |= test_pending_reset_lifetime();
  forthDictClear();

  printf("  [DEBUG] running test_run_entry_lifetime_signaling...\n");
  fail |= test_run_entry_lifetime_signaling();
  forthDictClear();

  /* P2: Pillar 2 — pre-scan contract tests (T2.1-T2.4) */
  printf("\nFORTH P2 TESTS (pre-scan contract: forward ref, no tail, no recompile, owning scope, gen rearm, error halt, last step, two programs)\n");
  forthDictInit();

  printf("  [DEBUG] running test_prescan_forward_reference...\n");
  fail |= test_prescan_forward_reference();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_no_early_tail...\n");
  fail |= test_prescan_no_early_tail();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_no_recompile...\n");
  fail |= test_prescan_no_recompile();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_owning_scope...\n");
  fail |= test_prescan_owning_scope();
  forthDictClear();

  printf("  [DEBUG] running test_owning_program_start_bounds...\n");
  fail |= test_owning_program_start_bounds();
  forthDictClear();

  printf("  [DEBUG] running test_owning_program_start_max_not_last...\n");
  fail |= test_owning_program_start_max_not_last();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_generation_rearm...\n");
  fail |= test_prescan_generation_rearm();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_error_halts...\n");
  fail |= test_prescan_error_halts();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_error_rolls_back_prior_defs...\n");
  fail |= test_prescan_error_rolls_back_prior_defs();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_last_step_visible...\n");
  fail |= test_prescan_last_step_visible();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_two_programs_first_touch...\n");
  fail |= test_prescan_two_programs_first_touch();
  forthDictClear();

  printf("  [DEBUG] running test_scan_dynamic_no_cliff...\n");
  fail |= test_scan_dynamic_no_cliff();
  forthDictClear();

  printf("  [DEBUG] running test_recurse_compile_only...\n");
  fail |= test_recurse_compile_only();
  forthDictClear();

  printf("  [DEBUG] running test_validate_restored_bodies...\n");
  fail |= test_validate_restored_bodies();
  forthDictClear();

  printf("  [DEBUG] running test_accept_run_lifecycle...\n");
  fail |= test_accept_run_lifecycle();
  forthDictClear();

  printf("  [DEBUG] running test_accept_entry_state_roundtrip...\n");
  fail |= test_accept_entry_state_roundtrip();
  forthDictClear();

  printf("  [DEBUG] running test_accept_display_parity...\n");
  fail |= test_accept_display_parity();
  forthDictClear();

  printf("  [DEBUG] running test_accept_glyph_type_parity...\n");
  fail |= test_accept_glyph_type_parity();
  forthDictClear();

  printf("  [DEBUG] running test_dict_name_by_index...\n");
  fail |= test_dict_name_by_index();
  forthDictClear();

  printf("  [DEBUG] running test_accept_xeq_name_step...\n");
  fail |= test_accept_xeq_name_step();
  forthDictClear();

  printf("  [DEBUG] running test_param_core_extraction...\n");
  fail |= test_param_core_extraction();
  forthDictClear();

  printf("  [DEBUG] running test_param_core_bounded_names...\n");
  fail |= test_param_core_bounded_names();
  forthDictClear();

  printf("  [DEBUG] running test_c47_param_shared_dispatch...\n");
  fail |= test_c47_param_shared_dispatch();
  forthDictClear();

  printf("  [DEBUG] running test_param_parity_sweep...\n");
  fail |= test_param_parity_sweep();
  forthDictClear();

  /* COMMIT 4: executeOneStep ITM_FORTH arm + bump sites */
  printf("\nFORTH COMMIT 4 TESTS (executeOneStep ITM_FORTH arm)\n");
  forthDictInit();

  printf("  [DEBUG] running test_exec_step_marker_noop...\n");
  fail |= test_exec_step_marker_noop();
  forthDictClear();

  printf("  [DEBUG] running test_exec_step_source_runs...\n");
  fail |= test_exec_step_source_runs();
  forthDictClear();

  printf("  [DEBUG] running test_exec_step_halts_on_error...\n");
  fail |= test_exec_step_halts_on_error();
  forthDictClear();

  /* COMMIT 5: §8.4 derived-state helpers + test-program infrastructure */
  printf("\nFORTH COMMIT 5 TESTS (§8.4 derived-state helpers)\n");
  forthDictInit();

  printf("  [DEBUG] running test_marker_parity...\n");
  fail |= test_marker_parity();
  forthDictClear();

  printf("  [DEBUG] running test_placeholder_never_marker...\n");
  fail |= test_placeholder_never_marker();
  forthDictClear();

  printf("  [DEBUG] running test_entry_state_derivation...\n");
  fail |= test_entry_state_derivation();
  forthDictClear();

  /* COMMIT 6: manage.c override — toggle, in-region, FCALL redirect */
  printf("\nFORTH COMMIT 6 TESTS (manage.c override)\n");
  forthDictInit();

  printf("  [DEBUG] running test_toggle_inserts_marker...\n");
  fail |= test_toggle_inserts_marker();
  forthDictClear();

  printf("  [DEBUG] running test_forth_toggle_close_resets_sentinel...\n");
  fail |= test_forth_toggle_close_resets_sentinel();
  forthDictClear();

  printf("  [DEBUG] running test_fcall_redirect_records_name...\n");
  fail |= test_fcall_redirect_records_name();
  forthDictClear();

  printf("  [DEBUG] running test_fcall_redirect_rejects_stale...\n");
  fail |= test_fcall_redirect_rejects_stale();
  forthDictClear();

  /* COMMIT 7: manage.c slice 2 — E3 empty-commit, E5 EDIT, cursor */
  printf("\nFORTH COMMIT 7 TESTS (manage.c slice 2: E3/E5)\n");
  forthDictInit();

  printf("  [DEBUG] running test_forth_empty_enter_leaves_no_step...\n");
  fail |= test_forth_empty_enter_leaves_no_step();
  forthDictClear();

  printf("  [DEBUG] running test_forth_edit_extracts_source...\n");
  fail |= test_forth_edit_extracts_source();
  forthDictClear();

  /* COMMIT 8: decode.c override — §8.5 symmetric display */
  printf("\nFORTH COMMIT 8 TESTS (decode.c override: §8.5 marker rendering)\n");
  forthDictInit();

  printf("  [DEBUG] running test_decode_marker_directions...\n");
  fail |= test_decode_marker_directions();
  forthDictClear();

  printf("  [DEBUG] running test_decode_source_bare...\n");
  fail |= test_decode_source_bare();
  forthDictClear();

  printf("  [DEBUG] running test_mnu_forth_row...\n");
  fail |= test_mnu_forth_row();
  forthDictClear();

  /* COMMIT 10: softmenus.c registration + defines.h */
  printf("\nFORTH COMMIT 10 TESTS (softmenus.c/defines.h: MNU_FORTH registration)\n");
  forthDictInit();

  printf("  [DEBUG] running test_dynamic_menu_registration...\n");
  fail |= test_dynamic_menu_registration();
  forthDictClear();

  printf("  [DEBUG] running test_static_menu_integrity...\n");
  fail |= test_static_menu_integrity();
  forthDictClear();

  /* COMMIT 11: softmenus.c slice 2 — the : NAME picker builder */
  printf("\nFORTH COMMIT 11 TESTS (softmenus.c: MNU_FORTH picker builder)\n");
  forthDictInit();

  printf("  [DEBUG] running test_picker_scan_basic...\n");
  fail |= test_picker_scan_basic();
  forthDictClear();

  printf("  [DEBUG] running test_picker_omits_long_names...\n");
  fail |= test_picker_omits_long_names();
  forthDictClear();

  printf("  [DEBUG] running test_picker_rebuilds_same_menu...\n");
  fail |= test_picker_rebuilds_same_menu();
  forthDictClear();

  printf("  [DEBUG] running test_picker_capacity_boundary...\n");
  fail |= test_picker_capacity_boundary();
  forthDictClear();

  printf("  [DEBUG] running test_picker_dedupes...\n");
  fail |= test_picker_dedupes();
  forthDictClear();

  /* COMMIT 12: picker insert at cursor tests */
  printf("  [DEBUG] running test_picker_insert_at_cursor...\n");
  fail |= test_picker_insert_at_cursor();

  printf("  [DEBUG] running test_picker_key_mapping...\n");
  fail |= test_picker_key_mapping();

  printf("  [DEBUG] running test_picker_scan_and_alloc...\n");
  fail |= test_picker_scan_and_alloc();

  printf("  [DEBUG] running test_picker_renders_labels...\n");
  fail |= test_picker_renders_labels();

  printf("  [DEBUG] running test_picker_pixel_layout...\n");
  fail |= test_picker_pixel_layout();
  forthDictClear();

  printf("  [DEBUG] running test_picker_insert_mid_line...\n");
  fail |= test_picker_insert_mid_line();
  forthDictClear();

  printf("  [DEBUG] running test_picker_trailing_space...\n");
  fail |= test_picker_trailing_space();
  forthDictClear();

  /* COMMIT 13: keyboard.c — F7 picker-guard menu-identity conjunct */
  printf("  [DEBUG] running test_picker_guard_menu_identity...\n");
  fail |= test_picker_guard_menu_identity();
  forthDictClear();

  /* FIX-1: softmenus.c — glyph-wise bounded picker tokenizer */
  printf("  [DEBUG] running test_picker_glyph_tokenize...\n");
  fail |= test_picker_glyph_tokenize();
  forthDictClear();

  printf("  [DEBUG] running test_picker_long_token_skipped...\n");
  fail |= test_picker_long_token_skipped();
  forthDictClear();

  /* Memory refactor: allocator consistency + zero-init */
  printf("\nFORTH MEMORY REFACTOR TESTS (allocator consistency)\n");
  forthDictInit();

  printf("  [DEBUG] running test_program_memory_no_overlap...\n");
  fail |= test_program_memory_no_overlap();
  forthDictClear();

  printf("  [DEBUG] running test_cleanup_no_overlap...\n");
  fail |= test_cleanup_no_overlap();
  forthDictClear();

  printf("  [DEBUG] running test_softmenu_trailing_null...\n");
  fail |= test_softmenu_trailing_null();
  forthDictClear();

  /* FIX-2: derived state at insertion point */
  printf("\nFORTH FIX-2 TESTS (derived state at insertion point)\n");
  forthDictInit();

  printf("  [DEBUG] running test_e2_continuation_after_enter...\n");
  fail |= test_e2_continuation_after_enter();
  forthDictClear();

  printf("  [DEBUG] running test_e2_not_inside_rpn_gap...\n");
  fail |= test_e2_not_inside_rpn_gap();
  forthDictClear();

  printf("  [DEBUG] running test_e1_direction_mid_program...\n");
  fail |= test_e1_direction_mid_program();
  forthDictClear();

  /* Capture lifecycle: tam.function must not survive capture close/abort */
  printf("\nFORTH CAPTURE LIFECYCLE TESTS (tam.function reset on close/abort)\n");
  forthDictInit();

  printf("  [DEBUG] running test_forth_multiline_lock_holds...\n");
  fail |= test_forth_multiline_lock_holds();
  forthDictClear();

  printf("  [DEBUG] running test_tam_function_cleared_after_abort...\n");
  fail |= test_tam_function_cleared_after_abort();
  forthDictClear();

  /* FIX-3: restrict Forth fallback to XEQ/XEQ.SKP only */
  printf("\nFORTH FIX-3 TESTS (PARAM_LABEL fallback restricted to XEQ/XEQ.SKP)\n");
  forthDictInit();

  printf("  [DEBUG] running test_gto_word_errors...\n");
  fail |= test_gto_word_errors();
  forthDictClear();

  printf("  [DEBUG] running test_gto_item_errors...\n");
  fail |= test_gto_item_errors();
  forthDictClear();

  printf("  [DEBUG] running test_xeq_word_still_calls...\n");
  fail |= test_xeq_word_still_calls();
  forthDictClear();

  /* FIX-4: manage.c — insertUserItemInProgram opcode low-byte mask */
  printf("\nFORTH FIX-4 TESTS (manage.c: insertUserItemInProgram low-byte mask)\n");
  forthDictInit();

  printf("  [DEBUG] running test_useritem_xeqp1_opcode...\n");
  fail |= test_useritem_xeqp1_opcode();
  forthDictClear();

  printf("  [DEBUG] running test_useritem_xeqp1_decodes...\n");
  fail |= test_useritem_xeqp1_decodes();
  forthDictClear();

  /* F5: Forth picker is a submenu entry inside MNU_ALPHA, not an overlay */
  printf("\nFORTH F5 TESTS (alpha menu presentation)\n");
  forthDictInit();

  printf("  [DEBUG] running test_alpha_menu_on_top_during_capture...\n");
  fail |= test_alpha_menu_on_top_during_capture();
  forthDictClear();

  printf("  [DEBUG] running test_alpha_menu_contains_fwrd...\n");
  fail |= test_alpha_menu_contains_fwrd();
  forthDictClear();

  /* A8: real-keyboard-path regression tests */
  printf("\nFORTH A8 TESTS (real keyboard dispatch chain)\n");
  forthDictInit();

  printf("  [DEBUG] running test_forth_toggle_from_catalog_leaves_alpha_menu...\n");
  fail |= test_forth_toggle_from_catalog_leaves_alpha_menu();

  printf("  [DEBUG] running test_forth_drain_clears_buried_catalog...\n");
  fail |= test_forth_drain_clears_buried_catalog();
  forthDictClear();

  printf("  [DEBUG] running test_forth_capture_survives_keystroke...\n");
  fail |= test_forth_capture_survives_keystroke();
  forthDictClear();

  printf("  [DEBUG] running test_forth_alpha_gesture_resumes_forth...\n");
  fail |= test_forth_alpha_gesture_resumes_forth();
  forthDictClear();

  /* C-13: end-of-line error precedence tests */
  printf("\nFORTH C-13 TESTS (end-of-line error precedence)\n");
  printf("  [DEBUG] running test_unterminated_def_errors...\n");
  fail |= test_unterminated_def_errors();
  forthDictClear();

  printf("  [DEBUG] running test_overlong_token_in_def_keeps_error...\n");
  fail |= test_overlong_token_in_def_keeps_error();
  forthDictClear();

  /* F3-3: scope isolation */
  printf("\nFORTH F3-3 TESTS (scope isolation)\n");
  forthDictInit();

  printf("  [DEBUG] running test_scope_isolation...\n");
  fail |= test_scope_isolation();
  forthDictClear();

  /* F3-4: GLOBAL/IMMEDIATE/FORGET with same-line mark discipline */
  printf("\nFORTH F3-4 TESTS (global marks)\n");
  forthDictInit();
  forthGDictInit();

  printf("  [DEBUG] running test_global_marks...\n");
  fail |= test_global_marks();
  forthDictClear();
  forthGDictClear();

  /* F3-5: compile-time control flow */
  printf("\nFORTH F3-5 TESTS (compile-time control flow)\n");
  forthDictInit();
  forthGDictInit();

  printf("  [DEBUG] running test_control_flow...\n");
  fail |= test_control_flow();
  forthDictClear();
  forthGDictClear();

  /* F4-1: parameter classification + direct numeric parameters */
  printf("\nFORTH F4-1 TESTS (parameter classification + direct numeric)\n");
  forthDictInit();

  printf("  [DEBUG] running test_param_textual_numeric...\n");
  fail |= test_param_textual_numeric();
  forthDictClear();

  /* F4-2: register, flag, and shuffle direct forms */
  printf("\nFORTH F4-2 TESTS (register, flag, shuffle direct forms)\n");
  forthDictInit();

  printf("  [DEBUG] running test_param_register_flag...\n");
  fail |= test_param_register_flag();
  forthDictClear();
  forthGDictClear();

  /* F4-3: named, system-flag, and indirect parameter forms */
  printf("\nFORTH F4-3 TESTS (named, system-flag, indirect parameter forms)\n");
  forthDictInit();

  printf("  [DEBUG] running test_param_named_indirect...\n");
  fail |= test_param_named_indirect();
  forthDictClear();
  forthGDictClear();

  /* F4-4: Series C error table and native/Forth parity acceptance */
  printf("\nFORTH F4-4 TESTS (Series C error table and native/Forth parity)\n");
  forthDictInit();

  printf("  [DEBUG] running test_param_series_c_acceptance...\n");
  fail |= test_param_series_c_acceptance();
  forthDictClear();
  forthGDictClear();

  /* F5-1: check mode — the tokenizer validates its own grammar */
  printf("\nFORTH F5-1 TESTS (check mode: tokenizer self-validation)\n");
  forthDictInit();

  printf("  [DEBUG] running test_check_source_line...\n");
  fail |= test_check_source_line();
  forthDictClear();
  forthGDictClear();

  /* F5-2: E9 commit gate — structural rejects at ENTER, advisory commits */
  printf("\nFORTH F5-2 TESTS (E9 commit gate)\n");
  forthDictInit();

  printf("  [DEBUG] running test_commit_gate...\n");
  fail |= test_commit_gate();
  forthDictClear();
  forthGDictClear();

  /* F6-1: managed capture buffer behind the capture object */
  printf("\nFORTH F6-1 TESTS (managed capture buffer)\n");
  forthDictInit();

  printf("  [DEBUG] running test_capture_buffer...\n");
  fail |= test_capture_buffer();
  forthDictClear();
  forthGDictClear();

  /* F6-2: TAM suspend/resume keeps capture alive */
  printf("\nFORTH F6-2 TESTS (TAM suspend/resume)\n");
  forthDictInit();

  printf("  [DEBUG] running test_capture_suspend...\n");
  fail |= test_capture_suspend();
  forthDictClear();
  forthGDictClear();

  /* F6-3: catalogs and menus during capture */
  printf("\nFORTH F6-3 TESTS (catalogs and menus during capture)\n");
  forthDictInit();

  printf("  [DEBUG] running test_capture_menus...\n");
  fail |= test_capture_menus();
  forthDictClear();
  forthGDictClear();

  /* F6-4: parameter entry emits canonical text */
  printf("\nFORTH F6-4 TESTS (parameter entry emits canonical text)\n");
  forthDictInit();

  printf("  [DEBUG] running test_capture_param_text...\n");
  fail |= test_capture_param_text();
  forthDictClear();
  forthGDictClear();

  /* F6-5: the dictionary-backed word catalog */
  printf("\nFORTH F6-5 TESTS (dictionary-backed word catalog)\n");
  forthDictInit();

  printf("  [DEBUG] running test_word_catalog...\n");
  fail |= test_word_catalog();
  forthDictClear();
  forthGDictClear();

  /* F6-6: capture acceptance battery */
  printf("\nFORTH F6-6 TESTS (capture acceptance battery)\n");
  forthDictInit();

  printf("  [DEBUG] running test_capture_acceptance...\n");
  fail |= test_capture_acceptance();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_sim_bench_capture...\n");
  fail |= test_sim_bench_capture();
  fail |= test_sim_bench_nesting();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH CODE-AUDIT TESTS (PEM insert-vs-execute regression)\n");
  forthDictInit();
  printf("  [DEBUG] running test_pem_xeq_dynmenu_no_live_exec...\n");
  fail |= test_pem_xeq_dynmenu_no_live_exec();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_forth_edit_modify_commit...\n");
  fail |= test_forth_edit_modify_commit();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_forth_capture_navigation...\n");
  fail |= test_forth_capture_navigation();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH FIX-8 TESTS (capture-close completeness)\n");
  forthDictInit();
  printf("  [DEBUG] running test_forth_toggle_close_with_open_capture...\n");
  fail |= test_forth_toggle_close_with_open_capture();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_capture_close_paths_reset_tuple...\n");
  fail |= test_capture_close_paths_reset_tuple();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-1 TESTS (capture origin bit, interactive open, minimum close)\n");
  forthDictInit();
  printf("  [DEBUG] running test_capture_origin_lifecycle...\n");
  fail |= test_capture_origin_lifecycle();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_capture_interactive_close...\n");
  fail |= test_capture_interactive_close();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-2 TESTS (interactive REPL: ENTER, EXIT ladder, input cap)\n");
  forthDictInit();
  printf("  [DEBUG] running test_capture_interactive_repl...\n");
  fail |= test_capture_interactive_repl();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-3 TESTS (interactive divert seam: catalogs, picker, keys mode)\n");
  forthDictInit();
  printf("  [DEBUG] running test_capture_interactive_divert...\n");
  fail |= test_capture_interactive_divert();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-H TESTS (history program: push, cap, evict, recall)\n");
  forthDictInit();
  printf("  [DEBUG] running test_history_program...\n");
  fail |= test_history_program();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-F1 TESTS (fold context: materialise, arm, sweep, restore)\n");
  forthDictInit();
  printf("  [DEBUG] running test_fold_context...\n");
  fail |= test_fold_context();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-F2 TESTS (the three tam.c seams + determineItem fix)\n");
  forthDictInit();
  printf("  [DEBUG] running test_fold_seams...\n");
  fail |= test_fold_seams();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-F3 TESTS (operand-class parity, fold close paths, CM-gate audit)\n");
  forthDictInit();
  printf("  [DEBUG] running test_fold_operand_parity...\n");
  fail |= test_fold_operand_parity();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_fold_close_paths...\n");
  fail |= test_fold_close_paths();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_cm_gate_audit...\n");
  fail |= test_cm_gate_audit();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH L1-5 TESTS (stage acceptance battery)\n");
  forthDictInit();
  printf("  [DEBUG] running test_interactive_acceptance...\n");
  fail |= test_interactive_acceptance();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_interactive_close_sweep...\n");
  fail |= test_interactive_close_sweep();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_interactive_residue...\n");
  fail |= test_interactive_residue();
  forthDictClear();
  forthGDictClear();



  printf("\nFORTH FIX-7 TESTS (emit/accept quote parity)\n");
  forthDictInit();
  printf("  [DEBUG] running test_forth_fold_commit_recompiles...\n");
  fail |= test_forth_fold_commit_recompiles();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_quote_glyph_accept_parity...\n");
  fail |= test_quote_glyph_accept_parity();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH FIX-9 TESTS (resume drains buried catalog menus)\n");
  forthDictInit();
  printf("  [DEBUG] running test_resume_drains_buried_catalog...\n");
  fail |= test_resume_drains_buried_catalog();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH K1 TESTS (keys-mode toggle)\n");
  forthDictInit();
  printf("  [DEBUG] running test_keys_mode_resolution...\n");
  fail |= test_keys_mode_resolution();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_keys_mode_toggle_arm...\n");
  fail |= test_keys_mode_toggle_arm();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_keys_mode_nav_guards...\n");
  fail |= test_keys_mode_nav_guards();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH K2 TESTS (token boundaries + ladder)\n");
  forthDictInit();
  printf("  [DEBUG] running test_insert_token_boundary...\n");
  fail |= test_insert_token_boundary();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_keys_digits_then_function...\n");
  fail |= test_keys_digits_then_function();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_exit_ladder_keys_rung...\n");
  fail |= test_exit_ladder_keys_rung();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_keys_eex_and_numlock...\n");
  fail |= test_keys_eex_and_numlock();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH K3 TESTS (keys-mode TAM persistence)\n");
  forthDictInit();
  printf("  [DEBUG] running test_keys_tam_roundtrip...\n");
  fail |= test_keys_tam_roundtrip();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_alpha_tam_roundtrip_unchanged...\n");
  fail |= test_alpha_tam_roundtrip_unchanged();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_abandon_clears_keys_bit...\n");
  fail |= test_abandon_clears_keys_bit();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH FIX-6 TESTS (free-list integrity + arena report)\n");
  printf("  [DEBUG] running test_freelist_consistent...\n");
  fail |= test_freelist_consistent();

  printf("  [DEBUG] running test_freelist_double_free_guarded...\n");
  fail |= test_freelist_double_free_guarded();

  printf("  [DEBUG] running test_freelist_interior_double_free...\n");
  fail |= test_freelist_interior_double_free();

  printf("  [DEBUG] running test_freelist_no_mutation_on_oversize_free...\n");
  fail |= test_freelist_no_mutation_on_oversize_free();

  /* FIX-6: Arena report (§5.4/§8.9 duty) — define words, report, then clear */
  { uint32_t freeRamBefore = getFreeRamMemory();
    forthDictInit();
    forthGDictInit();
    define_word("AR1", 3);
    define_word("AR2", 3);
    define_word("AR3", 3);
    {
      uint16_t gw = gbegin_word("GAR", 3);
      if (gw != FORTH_NULL) gend_word();
    }
    uint32_t freeRamAfter = getFreeRamMemory();
    uint16_t dictBlocks = fdict.sizeBlocks + gdict.sizeBlocks;
    uint32_t dictBytes = TO_BYTES(dictBlocks);
    printf("  FORTH ARENA: dict here=%u sizeBlocks=%u gdict here=%u sizeBlocks=%u freeRamDelta=%ld\n",
    fdict.here, fdict.sizeBlocks, gdict.here, gdict.sizeBlocks, (long)(freeRamBefore - freeRamAfter));
    if (dictBlocks > 512) {
      printf("    WARN: dict region %u blocks (%u bytes) exceeds 2 KB budget\n",
      dictBlocks, dictBytes);
    } else {
      printf("    PASS: dict region %u blocks (%u bytes) within 2 KB budget\n",
      dictBlocks, dictBytes);
    }
    forthDictClear();
    forthGDictClear();
  }

  preserveBackupFile();
  printf("  [DEBUG] running test_save_restore_roundtrip...\n");
  fail |= test_save_restore_roundtrip();
  printf("  [DEBUG] running test_restore_missing_params_defaults...\n");
  fail |= test_restore_missing_params_defaults();
  printf("  [DEBUG] running test_restore_validation_clamps...\n");
  fail |= test_restore_validation_clamps();
  printf("  [DEBUG] running test_validate_direct_corruption...\n");
  fail |= test_validate_direct_corruption();
  restoreBackupFile();

  /* FIX-6: allocated regions must return to the start value after all
   * cleanup. A growth here means some test leaked an allocation (the two
   * historical offenders: forthDictInit on a live dict — §6.2 P-4 — and
   * the interior-double-free test's split-free). Gate, don't warn: a WARN
   * was permanent noise nobody failed on. */
  if (numberOfAllocatedMemoryRegions != allocRegionsStart) {
    printf("  FAIL: numberOfAllocatedMemoryRegions = %d (start=%d, expected unchanged)\n",
    numberOfAllocatedMemoryRegions, allocRegionsStart);
    fail = 1;
  }

  /* K4-A: registered AFTER the FIX-6 freelist group — the battery's two
   * program runs otherwise shift the free-list shape (16 regions, no
   * adjacent pair) and push test_freelist_interior_double_free into its
   * defensive SKIP, silently unexercising that assertion. */

  /* M1 (Stage M): registered AFTER the FIX-6 leak gate, the K4-A
   * precedent — the assign battery's save/restore cycle and USER-key
   * table rebuilds legitimately shift the allocator composition (the
   * packed userKeyLabel table relocates on every write), which the
   * region-count gate has no allowance for. */
  printf("\nFORTH M1 TESTS (FWRD catalog outside captures; ASSIGN band)\n");
  forthDictInit();
  printf("  [DEBUG] running test_fwrd_normal_mode...\n");
  fail |= test_fwrd_normal_mode();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_fwrd_assign...\n");
  fail |= test_fwrd_assign();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_fwrd_late_binding...\n");
  fail |= test_fwrd_late_binding();
  forthDictClear();
  forthGDictClear();


  printf("\nFORTH K4 TESTS (stage acceptance)\n");
  forthDictInit();
  printf("  [DEBUG] running test_k4_mixed_input_definition...\n");
  fail |= test_k4_mixed_input_definition();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_k4_keys_only_line...\n");
  fail |= test_k4_keys_only_line();
  forthDictClear();
  forthGDictClear();

  forthDictInit();
  printf("  [DEBUG] running test_k4_relock_submode...\n");
  fail |= test_k4_relock_submode();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_k4_ladder_full_unwind...\n");
  fail |= test_k4_ladder_full_unwind();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_k4_arena_sweep...\n");
  fail |= test_k4_arena_sweep();
  forthDictClear();
  forthGDictClear();

  printf("\nFORTH SHOWCASE PROGRAM (complete user-facing language example)\n");
  forthDictInit();
  forthGDictInit();
  printf("  [DEBUG] running test_showcase_program...\n");
  fail |= test_showcase_program();
  forthDictClear();
  forthGDictClear();

  printf("  [DEBUG] running test_data_stack_overflow_guard...\n");
  fail |= test_data_stack_overflow_guard();

  printf("  [DEBUG] running test_deep_recursion_spill...\n");
  fail |= test_deep_recursion_spill();

  printf("  [DEBUG] running test_spill_native_boundary...\n");
  fail |= test_spill_native_boundary();

  printf("  [DEBUG] running test_spill_window_parity...\n");
  fail |= test_spill_window_parity();

  printf("  [DEBUG] running test_forth_run_from_x_brackets...\n");
  fail |= test_forth_run_from_x_brackets();

  printf("  [DEBUG] running test_spill_region...\n");
  fail |= test_spill_region();

  printf("  [DEBUG] running test_native_lift_after_forth...\n");
  fail |= test_native_lift_after_forth();

  printf("  [DEBUG] running test_savings_program...\n");
  forthDictInit();
  forthGDictInit();
  fail |= test_savings_program();
  forthDictClear();
  forthGDictClear();

  /* N1-1: the console view ring (Stage N).  No dictionary dependency —
   * the ring is BSS and knows nothing about fdict/gdict — but the seam
   * test drives forthDictInit/forthDictClear, so the block owns its own
   * lifecycle exactly like its neighbours. */
  printf("\nFORTH N1-1 TESTS (console view ring)\n");
  forthDictInit();

  printf("  [DEBUG] running test_console_ring_basic...\n");
  fail |= test_console_ring_basic();

  printf("  [DEBUG] running test_console_ring_partial...\n");
  fail |= test_console_ring_partial();

  printf("  [DEBUG] running test_console_ring_evict...\n");
  fail |= test_console_ring_evict();

  printf("  [DEBUG] running test_console_ring_linecap...\n");
  fail |= test_console_ring_linecap();

  printf("  [DEBUG] running test_console_ring_glyph...\n");
  fail |= test_console_ring_glyph();

  printf("  [DEBUG] running test_console_ring_clear...\n");
  fail |= test_console_ring_clear();

  printf("  [DEBUG] running test_console_ring_view...\n");
  fail |= test_console_ring_view();

  printf("  [DEBUG] running test_console_ring_reset_seam...\n");
  fail |= test_console_ring_reset_seam();

  printf("  [DEBUG] running test_console_ring_hammer...\n");
  fail |= test_console_ring_hammer();

  forthConsoleClear();          /* leave no dialogue behind for later tests */
  forthDictClear();
  forthGDictClear();

  /* FIX-6: free-list integrity — LAST test, after all cleanup */

  /* Stale-list tripwire (probeListPtrs): any labelList/programList pointer
   * observed inside a free region during the run fails the suite. */
  if (probeListPtrsViolation) {
    printf("  FAIL: stale labelList/programList pointer observed (see [PROBE] lines)\n");
    fail = 1;
  }

  if (suiteEntryCount != 1) {
    printf("  FAIL: suite entered %d times (run-once guard in config.c broken)\n", suiteEntryCount);
    fail = 1;
  }

  if (fail) {

    printf("\nFORTH SELF-TEST: FAILED\n");
  } else {
    printf("\nFORTH SELF-TEST: ALL PASSED\n");
  }

  return fail;
}



















/* ==========================================================================
 * F4-3: Named, system-flag, and indirect parameter forms
 * ========================================================================== */



/* ==========================================================================
 * F5-1: Check mode — the tokenizer validates its own grammar
 * ========================================================================== */


/* ==========================================================================
 * F5-1: Check mode — the tokenizer validates its own grammar
 * ========================================================================== */

/* F5-2A: fill the stack region an about-to-be-called frame will occupy with
 * 0xAA, so that a callee reading an uninitialized local sees a deterministic
 * poison value instead of whatever the previous call left behind. volatile
 * keeps the writes; the array is deliberately larger than the frames under
 * test. */
static void poisonAutoFrame(void)
{
  volatile uint8_t scratch[1024];
  int i;
  for (i = 0; i < (int)sizeof(scratch); i++) {
    scratch[i] = 0xAA;
  }
  (void)scratch[0];
}








/* test_pem_xeq_dynmenu_no_live_exec — code-audit finding, 2026-07-20.
 *
 * items.c's runFunction() dispatches ITM_XEQ picked from a dynamic menu
 * (dynamicMenuItem >= 0): a resolved native LABEL correctly branches on
 * calcMode (insertUserItemInProgram in PEM, reallyRunFunction otherwise),
 * but the forth-core-added FORTH_XEQ_COLON/FORTH_XEQ_ITEM branches used to
 * skip that check entirely and always call reallyRunFunction — so picking
 * a Forth word from the MNU_FORTH picker via XEQ while editing a program
 * (CM_PEM) would execute it live instead of recording an "XEQ 'NAME'"
 * step, corrupting register/stack state mid-edit instead of composing the
 * program (violates DESIGN.md §4.2's "PEM recording of XEQ 'NAME': names
 * persist, never widx" contract). The same missing-check pattern was found
 * and fixed at two more call sites (screen.c's _executeItem, keyboard.c's
 * btnReleased) — both FLAG_USER-key XEQ dispatch, not exercised by this
 * test, fixed by inspection/mirroring this one's shape.
 *
 * Drive: compile W7 interactively (fdict-resident, F6-5's "interactive-
 * scope dictionary words" catalog section), build a real MNU_FORTH picker
 * over a minimal program, select W7 via dynamicMenuItem, call
 * runFunction(ITM_XEQ) with calcMode == CM_PEM. Oracle: a step must be
 * recorded (getNumberOfSteps() increases by exactly 1) and the sentinel
 * left in X must survive untouched (no live execution).
 * Escaping mutation: drop the calcMode == CM_PEM check in the
 * FORTH_XEQ_COLON arm (items.c) — X becomes 7 (the word ran) and the step
 * count stays unchanged, both caught below. */






/* ---- Second showcase program: SAVE (compound savings schedule) ----
 * Companion to test_showcase_program. Where FDEMO exercises every language
 * feature in isolation, SAVE is a program someone would actually keep: it
 * writes a six-period compound-interest schedule into R00..R05 and leaves the
 * closing balance in R22, using a GLOBAL word (GROW) that stays callable from
 * the keyboard afterwards.
 *
 * Register-based by necessity, and that is the point of the fixture: RCL
 * inside compiled Forth overwrites X rather than lifting the Forth stack, so
 * a working value CANNOT be parked on the stack across an RCL. The schedule
 * therefore carries its state in R19 (countdown), R20 (slot index) and R21
 * (running balance). An earlier draft that kept the balance on the stack
 * stored six zeros and passed nothing. */
static int forthExprIsZero(const char *src)
{
  uint32_t t;
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(src);
  if (lastErrorCode != ERROR_NONE) {
    return 0;
  }
  t = getRegisterDataType(REGISTER_X);
  if (t == dtReal34) {
    return real34IsZero(REGISTER_REAL34_DATA(REGISTER_X)) ? 1 : 0;
  }
  if (t == dtLongInteger) {
    longInteger_t li;
    int isZero;
    longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    isZero = (mpz_cmp_ui(li, 0) == 0);
    longIntegerFree(li);
    return isZero;
  }
  return 0;
}







/* T5 split parts — included last so every helper and file-scope static above is
 * visible to them; forward decls before the runner cover all call sites. */
#include "test_capture.part.h"
#include "test_params.part.h"


/* T5 split parts — included last so every helper and file-scope static above is
 * visible to them; forward decls before the runner cover all call sites. */
#include "test_persist.part.h"
#include "test_engine.part.h"
#include "test_console.part.h"

#endif  // PC_BUILD && FORTH_DEBUG_SELFTEST
