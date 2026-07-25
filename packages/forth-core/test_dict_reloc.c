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

/* ---- Stack test: ILIT + DUP + arithmetic + ASLIFT on normal exit ----
 * Mutation: ASLIFT not set after return (C4 missing)               ---- */
static int test_stack_aslift(void)
{
  uint16_t w = begin_word("S1", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(5);
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(PRIM_TOKEN(P_PLUS));
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("S1");
  if (err) {
    printf("    FAIL: unexpected error %d during stack test\n", lastErrorCode);
    return 1;
  }
  if (!getSystemFlag(FLAG_ASLIFT)) {
    printf("    FAIL: FLAG_ASLIFT not set after normal word return (C4 mutation)\n");
    return 1;
  }
  if (!x_is_longint(10)) {
    printf("    FAIL: X should be 10 after ILIT 5 DUP + (stack result not computed)\n");
    return 1;
  }
  printf("    PASS: stack ops + ASLIFT on exit\n");
  return 0;
}

/* ---- Branch forward: BR skips over a DROP token ----
 * Mutation: BR delta miscalculated (wrong cell arithmetic)          ---- */
static int test_branch_fwd(void)
{
  /*
   * Layout (cells):
   *   0: BR
   *   1: delta +1  (skip 1 cell = DROP)
   *   2: DROP      (skipped)
   *   3: ILIT
   *   4-5: int32 42
   *   6: EXIT
   */
  uint16_t w = begin_word("BF", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_BR);
  emit_int16(1);           /* skip 1 cell = DROP token */
  forthDictEmit(PRIM_TOKEN(P_DROP));  /* skipped */
  forthDictEmit(T_ILIT);
  emit_int32(42);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  forthPushInt32(123456);  /* Y canary: unskipped DROP would consume it */
  bool err = run_word("BF");
  if (err) {
    printf("    FAIL: unexpected error %d during fwd branch\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(42)) {
    printf("    FAIL: X should be 42 after BR forward skip\n");
    return 1;
  }
  if (!y_is_longint(123456)) {
    printf("    FAIL: AUDIT R2 forward branch consumed the Y canary\n");
    return 1;
  }
  printf("    PASS: BR forward skips DROP\n");
  return 0;
}

/* ---- Branch backward: canonical countdown loop (DESIGN.md §2.2) ----
 * Body: DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT
 * Counter starts at 5, decremented to 0 over 5 iterations, terminates.
 * 0BR CONSUMES the DUP'd copy; the original counter stays for +.
 * Deltas are in CELLS, signed, relative to the cell after the delta.
 * Cell layout:
 *   0:  DUP
 *   1:  0BR
 *   2:  delta +6  (skip ILIT(-1) + + + BR + delta = 6 cells → EXIT)
 *   3:  ILIT
 *   4-5: int32 -1 (2 cells, LE)
 *   6:  +
 *   7:  BR
 *   8:  delta -9  (back to cell 0: ip=9, 9+(-9)=0)
 *   9:  EXIT
 * 0BR: at cell 1, delta at cell 2, ip=3. +6 → ip=9 (EXIT). ✓
 * BR:  at cell 7, delta at cell 8, ip=9. -9 → ip=0 (DUP). ✓
 * Mutation: revert to "DUP ILIT 1 MINUS 0BR BR" → infinite loop (hang).
 * Mutation: 0BR doesn't consume → DUP accumulates, loop hangs.         ---- */
static int test_branch_back(void)
{
  uint16_t w = begin_word("BB", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(T_0BR);
  emit_int16(6);
  forthDictEmit(T_ILIT);
  emit_int32(-1);
  forthDictEmit(PRIM_TOKEN(P_PLUS));
  forthDictEmit(T_BR);
  emit_int16(-9);
  forthDictEmit(T_EXIT);
  end_word(w);

  /* Seed X = 5 (counter) */
  forthPushInt32(5);
  lastErrorCode = ERROR_NONE;
  bool err = run_word("BB");
  if (err) {
    printf("    FAIL: unexpected error %d during back branch loop\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(0)) {
    printf("    FAIL: X should be 0 after countdown loop (got non-zero)\n");
    return 1;
  }
  printf("    PASS: canonical backward loop 5→0 terminates, X==0\n");
  return 0;
}

/* ---- 0BR with dtLongInteger zero — toothed ----
 * Base sentinel (42) below the tested value (0).
 * 0BR pops 0, branches if zero → sentinel exposed in X.
 * If branch NOT taken (raw real34IsZero misreads long-int zero),
 * ILIT 999 executes → X = 999.
 * Cell layout:
 *   0:  ILIT
 *   1-2: int32 42  (base sentinel, 2 cells LE)
 *   3:  ILIT
 *   4-5: int32 0   (2 cells LE)
 *   6:  0BR
 *   7:  delta +3  (ip=8, +3 → ip=11 EXIT)
 *   8:  ILIT
 *   9-10: int32 999 (not-taken sentinel, 2 cells LE)
 *   11: EXIT
 * Mutation: raw real34IsZero on dtLongInteger 0 → no branch → X=999. ---- */
static int test_0br_longint(void)
{
  uint16_t w = begin_word("ZB", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  forthDictEmit(T_ILIT);
  emit_int32(0);
  forthDictEmit(T_0BR);
  emit_int16(3);
  forthDictEmit(T_ILIT);
  emit_int32(999);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("ZB");
  if (err) {
    printf("    FAIL: 0BR error on dtLongInteger zero (T1-1 mutation: error %d)\n",
    lastErrorCode);
    return 1;
  }
  if (!x_is_longint(42)) {
    printf("    FAIL: X should be 42 (base sentinel) after 0BR branches on long-int zero; got X!=42\n");
    return 1;
  }
  printf("    PASS: 0BR branches on dtLongInteger zero, X==42\n");
  return 0;
}

/* ---- 0BR consumes its operand (branch NOT taken) ----
 * Pushes 7 (non-zero), then 0BR should pop it and NOT branch.
 * If 0BR does NOT pop: stack has 7,999 → X=999, Y=7.
 * If 0BR pops correctly: stack has 999 → X=999, Y=0.
 * Cell layout:
 *   0:  ILIT
 *   1-2: int32 7   (non-zero test value, 2 cells LE)
 *   3:  0BR
 *   4:  delta +3  (ip=5, +3 → ip=8 EXIT)
 *   5:  ILIT
 *   6-7: int32 999 (not-taken sentinel, 2 cells LE)
 *   8:  EXIT
 * Mutation: no fnDrop in popIsFalse → Y=7 (operand not consumed).  ---- */
static int test_0br_consumes(void)
{
  uint16_t w = begin_word("ZC", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(7);
  forthDictEmit(T_0BR);
  emit_int16(3);
  forthDictEmit(T_ILIT);
  emit_int32(999);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("ZC");
  if (err) {
    printf("    FAIL: 0BR consume test error (mutation: no fnDrop, error %d)\n",
    lastErrorCode);
    return 1;
  }
  if (!x_is_longint(999)) {
    printf("    FAIL: X should be 999 (branch not taken); got X!=999\n");
    return 1;
  }
  if (y_is_longint(7)) {
    printf("    FAIL: Y should not be 7 (operand not consumed)\n");
    return 1;
  }
  printf("    PASS: 0BR consumes non-zero operand, branch not taken, X==999, Y!=7\n");
  return 0;
}

/* ---- 0BR on long-int zero: branch taken ----
 * X = dtLongInteger 0 must be seen as zero by the type-dispatched test.
 * If branch taken → ILIT 42 executes → X = 42.
 * If branch NOT taken (raw real34IsZero misreads long-int zero) →
 * ILIT 999 executes → EXIT → X = 999.
 * Cell layout:
 *   0:  ILIT
 *   1-2: int32 0   (2 cells LE)
 *   3:  0BR
 *   4:  delta +4  (ip=5, +4 → ip=9 ILIT 42)
 *   5:  ILIT
 *   6-7: int32 999 (not-taken sentinel, 2 cells LE)
 *   8:  EXIT
 *   9:  ILIT
 *   10-11: int32 42 (taken sentinel, 2 cells LE)
 *   12: EXIT
 * Mutation: raw real34IsZero on dtLongInteger 0 → no branch → X=999. ---- */
static int test_0br_longint_taken_branch(void)
{
  uint16_t w = begin_word("ZS", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(0);
  forthDictEmit(T_0BR);
  emit_int16(4);
  forthDictEmit(T_ILIT);
  emit_int32(999);
  forthDictEmit(T_EXIT);
  forthDictEmit(T_ILIT);
  emit_int32(42);
  forthDictEmit(T_EXIT);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("ZS");
  if (err) {
    printf("    FAIL: 0BR taken-branch test error (mutation: raw real34IsZero, error %d)\n",
    lastErrorCode);
    return 1;
  }
  if (!x_is_longint(42)) {
    printf("    FAIL: X should be 42 after 0BR branches on long-int zero; got X!=42\n");
    return 1;
  }
  printf("    PASS: 0BR branches on dtLongInteger zero, branch taken, X==42\n");
  return 0;
}

/* ---- Literal with live token AFTER LIT ----
 * Body: ILIT 10 | ILIT 20 | DROP | EXIT
 * The ILIT after ILIT must advance ip correctly (ip+=4 for int32 payload).
 * Mutation: ip+=8 bug (treating ILIT payload as 8 bytes, desyncs to next token) ---- */
static int test_literal_after_lit(void)
{
  /* Body: ILIT 10 | ILIT 20 | PLUS | EXIT */
  uint16_t w = begin_word("LA", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(10);
  forthDictEmit(T_ILIT);
  emit_int32(20);
  forthDictEmit(PRIM_TOKEN(P_PLUS));
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("LA");
  if (err) {
    printf("    FAIL: desynced after ILIT (ip+=8 mutation: error %d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(30)) {
    printf("    FAIL: X should be 30 after ILIT 10 ILIT 20 PLUS\n");
    return 1;
  }
  printf("    PASS: live token after ILIT executes correctly (10+20=30)\n");
  return 0;
}

/* ---- Fix #13: FTOK_LIT roundtrip (hand-assembled) ----
 * Body: LIT(42.0) | ILIT(777) | EXIT
 * LIT pushes 16-byte real34; ILIT 777 lifts stack and pushes to X.
 * If ip advances 16 bytes past LIT payload, ILIT 777 runs -> X=777.
 * If ip only advances 8 (mutation), ILIT is misread -> X!=777 or error.
 * Mutation: ip += 8 instead of sizeof(real34_t) -> desync. ---- */
static int test_lit_roundtrip(void)
{
  real34_t litVal;
  int32ToReal34(42, &litVal);

  uint16_t w = begin_word("LR", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_LIT);
  forthDictEmitBytes(&litVal, sizeof(real34_t));
  forthDictEmit(T_ILIT);
  emit_int32(777);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("LR");
  if (err) {
    printf("    FAIL: LIT roundtrip error (ip+=8 mutation: desync, error %d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(777)) {
    printf("    FAIL: X should be 777 after LIT+ILIT (ip desynced past 16-byte payload)\n");
    return 1;
  }
  printf("    PASS: LIT roundtrip, ip advanced 16 bytes, trailing ILIT executed\n");
  return 0;
}

/* ---- Fix #13: FTOK_C47 PTP_NONE dispatch (hand-assembled) ----
 * Body: C47(ITM_NULL, NOPARAM) | ILIT(55) | EXIT
 * ITM_NULL (0) has PTP_NONE — no inline param consumed, no-op on stack.
 * ip advances 2 bytes past itemId, then ILIT 55 runs.
 * Mutation: wrong ip advance -> ILIT misread. ---- */
static int test_c47_ptp_none(void)
{
  uint16_t w = begin_word("CN", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_C47);
  { uint16_t itemId = 0; forthDictEmitBytes(&itemId, 2); } /* ITM_NULL, PTP_NONE */
  forthDictEmit(T_ILIT);
  emit_int32(55);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("CN");
  if (err) {
    printf("    FAIL: C47 PTP_NONE dispatch error (%d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(55)) {
    printf("    FAIL: X should be 55 (trailing ILIT ran after C47 PTP_NONE)\n");
    return 1;
  }
  printf("    PASS: C47 PTP_NONE dispatch, ip += 2 past itemId, trailing token ran\n");
  return 0;
}

/* ---- Fix #13: FTOK_C47 PTP_NUMBER_8 padded dispatch (hand-assembled) ----
 * Body: C47(ITM_PAUSE, 0) | ILIT(33) | EXIT
 * ITM_PAUSE (38) has PTP_NUMBER_8 — 1-byte param padded to 2-byte cell.
 * fnPause is a no-op stub; param=0 means no pause.
 * ip advances 2 past itemId, 2 past param cell.
 * Mutation: ip += 1 for param -> desync. ---- */
static int test_c47_ptp_number8_padded(void)
{
  uint16_t w = begin_word("C8", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_C47);
  { uint16_t itemId = 38; forthDictEmitBytes(&itemId, 2); } /* ITM_PAUSE, PTP_NUMBER_8 */
  { uint16_t paramCell = 0; forthDictEmitBytes(&paramCell, 2); } /* param=0, padded to cell */
  forthDictEmit(T_ILIT);
  emit_int32(33);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("C8");
  if (err) {
    printf("    FAIL: C47 PTP_NUMBER_8 dispatch error (%d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(33)) {
    printf("    FAIL: X should be 33 (trailing ILIT ran after C47 PTP_NUMBER_8)\n");
    return 1;
  }
  printf("    PASS: C47 PTP_NUMBER_8 padded dispatch, ip += 4 (2+2), trailing token ran\n");
  return 0;
}

/* ---- Fix #13: FTOK_C47 bad PTP (PTP_LABEL not supported) ----
 * Body: C47(ITM_GTO) | EXIT
 * ITM_GTO (2) has PTP_LABEL — not supported in sub-phase C.
 * Expect ERROR_OPERATION_UNDEFINED, forthInner returns.
 * Mutation: bad PTP silently continues -> no error. ---- */
static int test_c47_bad_ptp(void)
{
  uint16_t w = begin_word("CB", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_C47);
  { uint16_t itemId = 2; forthDictEmitBytes(&itemId, 2); } /* ITM_GTO, PTP_LABEL */
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("CB");
  if (!err) {
    printf("    FAIL: C47 PTP_LABEL should raise error (silently continued)\n");
    return 1;
  }
  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL: expected ERROR_OPERATION_UNDEFINED, got %d\n", lastErrorCode);
    return 1;
  }
  printf("    PASS: C47 bad PTP raises ERROR_OPERATION_UNDEFINED\n");
  return 0;
}

/* ---- Fix #13: FTOK_C47 nested re-entrancy guard ----
 * Define word "NR1": ILIT(42) EXIT
 * Hand-assemble: C47(ITM_FCALL, idx(NR1)) | ILIT(999) | EXIT
 * P3: nested forthInner via ITM_FCALL now succeeds (depth 2 <= FORTH_NEST_MAX).
 * Must fail if the depth upgrade regresses to a single-level guard, or if the
 * nested return corrupts the outer ip. ---- */
static int test_c47_nested_call_succeeds(void)
{
  uint16_t w1 = begin_word("NR1", 3);
  if (w1 == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  end_word(w1);

  uint16_t nr1Idx = fdict.count - 1;

  forthPushInt32(999);
  uint16_t w = begin_word("NR", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_C47);
  { uint16_t itemId = 2843; forthDictEmitBytes(&itemId, 2); } /* ITM_FCALL, PTP_NUMBER_16 */
  forthDictEmitBytes(&nr1Idx, 2);                              /* param = word index */
  forthDictEmit(T_ILIT);
  emit_int32(999);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("NR");
  if (err) {
    printf("    FAIL: nested call should succeed (got error %d)\n", lastErrorCode);
    return 1;
  }
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: expected ERROR_NONE, got %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(999)) {
    printf("    FAIL: sentinel ILIT 999 did not execute (outer ip corrupted by nested return)\n");
    return 1;
  }
  printf("    PASS: nested forthInner succeeded (depth 2), sentinel ILIT 999 executed\n");
  return 0;
}

/* T3.2 (D-3 core): nested forthInner fires while the OUTER level has rsp > 0.
 * Must fail if forthInner still zeroes rsp on entry (outer return chain
 * destroyed: TOP's tail after MID never runs). */
static int test_nested_preserves_outer_rstack(void)
{
  /* WMLEAF: ILIT(1), EXIT */
  uint16_t w = begin_word("WMLEAF", 6);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(1);
  end_word(w);
  uint16_t leafIdx = fdict.count - 1;

  /* WMNEST: ILIT(42), EXIT */
  w = begin_word("WMNEST", 6);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  end_word(w);
  uint16_t nestIdx = fdict.count - 1;

  /* WMMID: CALL(WMLEAF), FTOK_C47+ITM_FCALL+nestIdx, CALL(WMLEAF), EXIT */
  w = begin_word("WMMID", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit((ftoken_t)(T_CALL_BASE + leafIdx));
  forthDictEmit(T_C47);
  { uint16_t itemId = 2843; forthDictEmitBytes(&itemId, 2); }
  forthDictEmitBytes(&nestIdx, 2);
  forthDictEmit((ftoken_t)(T_CALL_BASE + leafIdx));
  end_word(w);
  uint16_t midIdx = fdict.count - 1;

  /* WMTOP: CALL(WMMID), ILIT(9), EXIT */
  w = begin_word("WMTOP", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit((ftoken_t)(T_CALL_BASE + midIdx));
  forthDictEmit(T_ILIT);
  emit_int32(9);
  end_word(w);

  uint8_t savedRunStop = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  bool err = run_word("WMTOP");
  programRunStop = savedRunStop;

  if (err) {
    printf("    FAIL: nested call should succeed (got error %d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(9)) {
    printf("    FAIL: X != 9 (TOP's tail did not run — outer rstack destroyed)\n");
    return 1;
  }
  if (forthTestGetDepth() != 0) {
    printf("    FAIL: forthDepth = %d, expected 0\n", forthTestGetDepth());
    return 1;
  }
  if (forthTestGetRsp() != 0) {
    printf("    FAIL: rsp=%u leaked (success path unbalanced)\n",
           forthTestGetRsp());
    return 1;
  }
  printf("    PASS: nested forthInner preserved outer rstack, TOP's tail executed (X=9)\n");
  return 0;
}

/* T3.4: an error deep in a nest must unwind rsp to each level's watermark.
 * The rsp-at-rest assertion (rsp == 0) is the direct pin; the UWOK follow-up
 * run remains as the behavioral smoke check. */
static int test_nested_error_unwinds_rsp(void)
{
  int fail = 0;
  /* UWBAD: CALL(200) — nonexistent index → ERROR_INVALID_CORRUPTED_DATA at dispatch */
  uint16_t w = begin_word("UWBAD", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit((ftoken_t)(T_CALL_BASE + 200));
  end_word(w);
  uint16_t badIdx = fdict.count - 1;

  /* UWMID: CALL(UWBAD), EXIT — rsp > 0 when the bad call fires */
  w = begin_word("UWMID", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit((ftoken_t)(T_CALL_BASE + badIdx));
  end_word(w);

  /* Run UWMID — expect an error */
  bool err = run_word("UWMID");
  if (!err) {
    printf("    FAIL: UWMID should raise an error\n");
    return 1;
  }

  if (forthTestGetRsp() != 0) {
    printf("    FAIL: rsp=%u leaked after error unwind (watermark restore missing)\n",
           forthTestGetRsp());
    fail = 1;
  }

  /* UWOK: ILIT(6), EXIT — fresh word to verify rsp was restored */
  w = begin_word("UWOK", 4);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(6);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  err = run_word("UWOK");
  if (err) {
    printf("    FAIL: UWOK should succeed (got error %d)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(6)) {
    printf("    FAIL: X != 6 after UWOK (stale rstack entry corrupted ip)\n");
    return 1;
  }
  if (forthTestGetDepth() != 0) {
    printf("    FAIL: forthDepth = %d, expected 0\n", forthTestGetDepth());
    return 1;
  }
  if (fail) return 1;
  printf("    PASS: error unwind restored rsp, follow-up UWOK succeeded (X=6)\n");
  return 0;
}

/* ---- Fix #13: outer interpreter real literal ----
 * forthOuterInterpret("2.5 2 *") -> X is dtReal34, value 5.0.
 * Mutation: real literal not classified -> undefined word error. ---- */
static int test_outer_real_literal(void)
{
  /* forthDictClear, NOT forthDictInit: the sub-phase C section's earlier
   * tests (test_outer_compile_invoke) leave the dict region live, and
   * forthDictInit on a live arena leaks the region + its allocation
   * record (DESIGN.md §6.2 P-4). Clear frees first. */
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("2.5 2 *");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: real literal expression error (%d)\n", lastErrorCode);
    return 1;
  }
  if (getRegisterDataType(REGISTER_X) != dtReal34) {
    printf("    FAIL: X is not dtReal34 (type %u)\n", getRegisterDataType(REGISTER_X));
    return 1;
  }
  int32_t v = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_X));
  if (v != 5) {
    printf("    FAIL: X should be 5.0, got %d\n", v);
    return 1;
  }
  printf("    PASS: outer real literal 2.5 * 2 = 5.0 (dtReal34)\n");
  return 0;
}

/* ---- Divide-by-zero halt (fix #14: de-vacuous) ----
 * Pushes 42 (long int), then DIV by 0.
 * DIV by zero should set lastErrorCode and halt.
 * Sentinel (ILIT 999) after DIV must NOT execute.
 * Assert: lastErrorCode == ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN.
 * Assert: X != 999 (sentinel did not execute).
 * Assert: X is real34 zero — NOT the original longint 42 (test-audit
 * finding 2026-07-20: this comment used to claim X==42, but that was
 * never actually checked or true. Traced via src/c47/mathematics/
 * division.c: divLonILonI sees a zero divisor and converts BOTH X and Y
 * to dtReal34 in place *before* calling divRealReal(), which is where
 * ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN actually gets raised (divRealReal's
 * zero-divisor branch, FLAG_SPCRES clear) — X is left at its
 * already-converted value (real34 0.0), not restored to the pre-op
 * longint. registers.c's adjustResult() calls undo() on a nonzero
 * lastErrorCode, but undo() restores from SAVED_REGISTER_X/saveForUndo(),
 * a checkpoint this Forth-driven primitive call never populates, so it
 * has no effect on the outcome here — confirmed empirically, not by
 * reading undo()'s call sites exhaustively).
 * Mutation: error not checked, sentinel executes -> X=999.             ---- */
static int test_div_zero_halt(void)
{
  /* DZ: ILIT 42 | ILIT 0 | DIV | ILIT 999 | EXIT */
  uint16_t w = begin_word("DZ", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  forthDictEmit(T_ILIT);
  emit_int32(0);
  forthDictEmit(PRIM_TOKEN(P_DIV));
  forthDictEmit(T_ILIT);
  emit_int32(999);
  end_word(w);

  /* Clear special-results flag so div-by-zero produces an error, not infinity. */
  clearSystemFlag(FLAG_SPCRES);
  lastErrorCode = ERROR_NONE;
  run_word("DZ");

  if (lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) {
    printf("    FAIL: expected ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN, got %d\n", lastErrorCode);
    return 1;
  }
  if (x_is_longint(999)) {
    printf("    FAIL: DIV by zero did not halt (sentinel ILIT 999 executed, X=999)\n");
    return 1;
  }
  if (getRegisterDataType(REGISTER_X) != dtReal34 || !real34IsZero(REGISTER_REAL34_DATA(REGISTER_X))) {
    printf("    FAIL: X type=%u (expected dtReal34 zero) after DIV halt\n",
           getRegisterDataType(REGISTER_X));
    return 1;
  }
  printf("    PASS: DIV by zero halted (error %d, X is real34 zero, sentinel not executed)\n", lastErrorCode);
  return 0;
}

/* ---- Rstack overflow (70-deep call chain) ----
 * 70 words: each calls the next; leaf calls sentinel (sets X=777).
 * FORTH_RSTACK_DEPTH = 64; 65th CALL triggers ERROR_RAM_FULL.
 * If chain completes without overflow, sentinel runs and X=777.
 * Mutation: no rstack depth check -> chain completes, X=777.          ---- */
static int test_rstack_overflow(void)
{
  #define RSTACK_N 70
  char name[3];
  int i;

  /* Sentinel: sets X = 777 */
  {
    uint16_t w = begin_word("RS", 2);
    if (w != FORTH_NULL) {
      forthDictEmit(T_ILIT);
      emit_int32(777);
      end_word(w);
    }
  }
  uint16_t sentIdx = fdict.count - 1;

  /* Leaf: CALL(sentinel) */
  {
    name[0] = 'L'; name[1] = '0'; name[2] = '\0';
    uint16_t w = begin_word(name, 2);
    if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
    forthDictEmit((ftoken_t)(T_CALL_BASE + sentIdx));
    end_word(w);
  }

  /* Callers: each calls the previously created word */
  for (i = RSTACK_N - 1; i >= 0; i--) {
    name[0] = (char)('A' + (i / 36));
    int d = i % 36;
    name[1] = (d < 10) ? (char)('0' + d) : (char)('A' + (d - 10));
    name[2] = '\0';

    uint16_t w = begin_word(name, 2);
    if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
    forthDictEmit((ftoken_t)(T_CALL_BASE + (fdict.count - 1)));
    end_word(w);
  }

  /* Run root word "A0" (i=0) */
  lastErrorCode = ERROR_NONE;
  run_word("A0");

  if (x_is_longint(777)) {
    printf("    FAIL: 70-deep chain completed (no rstack overflow guard)\n");
    return 1;
  }
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: expected ERROR_RAM_FULL, got %d\n", lastErrorCode);
    return 1;
  }
  printf("    PASS: rstack overflow caught (error %d)\n", lastErrorCode);
  return 0;
}

/* ---- Runaway guard: BR -2 infinite loop ----
 * Body: BR -2 (infinite loop over BR token + delta, never reaches EXIT).
 * RUNAWAY_CAP = 4096 dispatches; guard must trigger and halt.
 * Use sentinel approach: word "RR1" sets X=444, then "RR2" runs the loop.
 * If guard halts before BR modifies X, X stays 444.
 * Mutation: no runaway cap -> infinite loop (hang).                   ---- */
static int test_runaway_guard(void)
{
  /* Prime word: sets X = 444 */
  {
    uint16_t w = begin_word("RR1", 3);
    if (w != FORTH_NULL) {
      forthDictEmit(T_ILIT);
      emit_int32(444);
      end_word(w);
    }
  }

  /* RR2: BR -2 (infinite loop, never reaches EXIT) */
  uint16_t w = begin_word("RR2", 3);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_BR);
  emit_int16(-2);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  run_word("RR1");       /* X = 444 */
  lastErrorCode = ERROR_NONE;
  run_word("RR2");

  if (!x_is_longint(444)) {
    printf("    FAIL: runaway guard: X should be 444\n");
    return 1;
  }
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: runaway guard: expected ERROR_RAM_FULL, got %d\n", lastErrorCode);
    return 1;
  }
  printf("    PASS: runaway guard halted (X=444, err=%d)\n", lastErrorCode);
  return 0;
}

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

/* Body is empty: zero bytes after the header. The very first token fetch
 * (2 bytes) has nothing to read.
 * Escaping mutation: remove the main-loop boundedRead(ip, 2) call — the read
 * proceeds on whatever garbage sits past fdict.here. */
static int test_truncated_token_fetch(void)
{
  int fail = 0;
  forthDictClear();

  uint16_t w = begin_word("TRTOK", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictFinishDef(w);   /* no tokens emitted at all */

  lastErrorCode = ERROR_NONE;
  forthPushInt32(444);
  bool err = run_word("TRTOK");

  if (!err || lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
    printf("    FAIL: empty body — lastErrorCode = %d, expected ERROR_INVALID_CORRUPTED_DATA (%d)\n",
           lastErrorCode, ERROR_INVALID_CORRUPTED_DATA);
    fail = 1;
  }
  if (!x_is_longint(444)) {
    printf("    FAIL: sentinel X changed after truncated token fetch\n");
    fail = 1;
  }

  forthDictClear();
  if (!fail) printf("    PASS: empty body rejected before the first token fetch, sentinel held\n");
  return fail;
}

/* Body is FTOK_ILIT with no following 4-byte int32.
 * Escaping mutation: remove the boundedRead(ip, 4) call in case FTOK_ILIT —
 * the memcpy reads 4 bytes past fdict.here. */
static int test_truncated_inline_operand(void)
{
  int fail = 0;
  forthDictClear();

  uint16_t w = begin_word("TRILIT", 6);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  forthDictFinishDef(w);   /* no int32 operand, no T_EXIT */

  lastErrorCode = ERROR_NONE;
  forthPushInt32(444);
  bool err = run_word("TRILIT");

  if (!err || lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
    printf("    FAIL: truncated FTOK_ILIT — lastErrorCode = %d, expected ERROR_INVALID_CORRUPTED_DATA (%d)\n",
           lastErrorCode, ERROR_INVALID_CORRUPTED_DATA);
    fail = 1;
  }
  if (!x_is_longint(444)) {
    printf("    FAIL: sentinel X changed after truncated FTOK_ILIT\n");
    fail = 1;
  }

  forthDictClear();
  if (!fail) printf("    PASS: truncated FTOK_ILIT rejected, sentinel X held\n");
  return fail;
}

/* Body is FTOK_C47 with no following 2-byte itemId.
 * The two bytes past fdict.here are block-rounding padding inside the SAME
 * allocated block, not unmapped memory — their content is whatever was last
 * written there. Forced to 0 (ITM_NULL, PTP_NONE) here so the test is
 * deterministic: an uncontrolled garbage value read as itemId=8712 on one
 * run, which the EXISTING downstream `itemId >= LAST_ITEM` check also
 * rejects with the same error code, masking whether the new guard ran at all.
 *
 * NOT independently mutation-isolable from the main fetch guard, verified by
 * trying: with itemId forced to 0/PTP_NONE, removing ONLY this guard still
 * passes, because the itemId "read" (of controlled, in-bounds-looking zero
 * bytes) consumes no further param, ip lands exactly on the now-exhausted
 * fdict.here, and the OUTER LOOP's own boundedRead(ip, 2) — unmutated, one
 * iteration later — catches the truncation instead. That is defense in
 * depth working as intended, not a gap: this guard and its neighbor protect
 * the same hazard from two sides, and removing either alone still gets
 * caught by the other for this specific body shape. The main-fetch guard's
 * own dedicated test (test_truncated_token_fetch) already proves that guard
 * in isolation. */
static int test_truncated_c47_item_id(void)
{
  int fail = 0;
  forthDictClear();

  uint16_t w = begin_word("TRC47", 5);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_C47);
  forthDictFinishDef(w);   /* no itemId bytes, no T_EXIT */
  fdict.base[fdict.here - 2] = 0;   /* force a within-bounds-looking itemId=0 */
  fdict.base[fdict.here - 1] = 0;   /* (ITM_NULL, PTP_NONE) in the padding bytes */

  lastErrorCode = ERROR_NONE;
  forthPushInt32(444);
  bool err = run_word("TRC47");

  if (!err || lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
    printf("    FAIL: truncated FTOK_C47 — lastErrorCode = %d, expected ERROR_INVALID_CORRUPTED_DATA (%d)\n",
           lastErrorCode, ERROR_INVALID_CORRUPTED_DATA);
    fail = 1;
  }
  if (!x_is_longint(444)) {
    printf("    FAIL: sentinel X changed after truncated FTOK_C47\n");
    fail = 1;
  }

  forthDictClear();
  if (!fail) printf("    PASS: truncated FTOK_C47 item ID rejected, sentinel X held\n");
  return fail;
}

/* ---- Malformed token ----
 * (a) Bad PRIM index: token 0x0FFF -> primIdx = 0x0FFE >= forthPrimCount
 * (b) Bad CALL index: CALL to nonexistent colon def
 * (c) Reserved token: 0x7F05 (unrecognized, above T_C47)
 * Each word: [bad token] | ILIT 555 | EXIT
 * ILIT 555 is the sentinel: if bad token is not caught, ILIT 555 executes -> X=555.
 * Prime X=444 before each test; if error caught, X stays 444.
 * Mutation: no bounds check -> sentinel executes, X=555.              ---- */
static int test_malformed_token(void)
{
  int fail = 0;

  /* Prime word: sets X = 444 */
  {
    uint16_t w = begin_word("MRP", 3);
    if (w != FORTH_NULL) {
      forthDictEmit(T_ILIT);
      emit_int32(444);
      end_word(w);
    }
  }

  /* (a) Bad PRIM index: 0x0FFF | ILIT 555 | EXIT */
  {
    uint16_t w = begin_word("MP", 2);
    if (w != FORTH_NULL) {
      forthDictEmit(0x0FFF);
      forthDictEmit(T_ILIT);
      emit_int32(555);
      end_word(w);

      lastErrorCode = ERROR_NONE;
      run_word("MRP");   /* X = 444 */
      lastErrorCode = ERROR_NONE;
      run_word("MP");
      if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
        printf("    FAIL: bad PRIM expected ERROR_OPERATION_UNDEFINED, got %d\n", lastErrorCode);
        fail = 1;
      }
      if (x_is_longint(555)) {
        printf("    FAIL: bad PRIM index not caught (sentinel ILIT 555 executed)\n");
        fail = 1;
      } else if (!x_is_longint(444)) {
        printf("    FAIL: bad PRIM index: X not preserved at 444\n");
        fail = 1;
      } else {
        printf("    PASS: bad PRIM index caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      }
    }
  }

  /* (b) Bad CALL index: CALL(0x6FFF) | ILIT 555 | EXIT */
  {
    uint16_t w = begin_word("MC", 2);
    if (w != FORTH_NULL) {
      forthDictEmit((ftoken_t)(T_CALL_BASE + 0x6FFF));
      forthDictEmit(T_ILIT);
      emit_int32(555);
      end_word(w);

      lastErrorCode = ERROR_NONE;
      run_word("MRP");   /* X = 444 */
      lastErrorCode = ERROR_NONE;
      run_word("MC");
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    FAIL: bad CALL expected ERROR_INVALID_CORRUPTED_DATA, got %d\n", lastErrorCode);
        fail = 1;
      }
      if (x_is_longint(555)) {
        printf("    FAIL: bad CALL index not caught (sentinel ILIT 555 executed)\n");
        fail = 1;
      } else if (!x_is_longint(444)) {
        printf("    FAIL: bad CALL index: X not preserved at 444\n");
        fail = 1;
      } else {
        printf("    PASS: bad CALL index caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      }
    }
  }

  /* (c) Reserved token: 0x7F05 | ILIT 555 | EXIT */
  {
    uint16_t w = begin_word("MR", 2);
    if (w != FORTH_NULL) {
      forthDictEmit(0x7F05);
      forthDictEmit(T_ILIT);
      emit_int32(555);
      end_word(w);

      lastErrorCode = ERROR_NONE;
      run_word("MRP");   /* X = 444 */
      lastErrorCode = ERROR_NONE;
      run_word("MR");
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    FAIL: reserved token expected ERROR_INVALID_CORRUPTED_DATA, got %d\n", lastErrorCode);
        fail = 1;
      }
      if (x_is_longint(555)) {
        printf("    FAIL: reserved token not caught (sentinel ILIT 555 executed)\n");
        fail = 1;
      } else if (!x_is_longint(444)) {
        printf("    FAIL: reserved token: X not preserved at 444\n");
        fail = 1;
      } else {
        printf("    PASS: reserved token caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      }
    }
  }

  return fail;
}
/* ---- ILIT sign-extend fix (fix #1) ----
 * Compiles : TEST128 128 ; and verifies X = 128 (not -128).
 * Mutation: revert to (int8_t) sign-extend on byte 0.               ---- */
static int test_ilit_sign_extend(void)
{
  uint16_t w = begin_word("T128", 4);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(128);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("T128");
  if (err) {
    printf("    FAIL: unexpected error %d during ILIT 128\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(128)) {
    printf("    FAIL: X should be 128 after ILIT 128 (sign-extend bug: got negative)\n");
    return 1;
  }

  /* Also test 300 (high byte nonzero) and -200 (negative, sign-preserved) */
  w = begin_word("T300", 4);
  if (w != FORTH_NULL) {
    forthDictEmit(T_ILIT);
    emit_int32(300);
    end_word(w);
    lastErrorCode = ERROR_NONE;
    err = run_word("T300");
    if (err || !x_is_longint(300)) {
      printf("    FAIL: X should be 300 after ILIT 300\n");
      return 1;
    }
  }

  w = begin_word("TN200", 5);
  if (w != FORTH_NULL) {
    forthDictEmit(T_ILIT);
    emit_int32(-200);
    end_word(w);
    lastErrorCode = ERROR_NONE;
    err = run_word("TN200");
    if (err || !x_is_longint(-200)) {
      printf("    FAIL: X should be -200 after ILIT -200\n");
      return 1;
    }
  }

  printf("    PASS: ILIT sign-extend fixed (128, 300, -200 correct)\n");
  return 0;
}

/* ---- ILIT arithmetic divergence (fix #1) ----
 * ILIT 42 + ILIT 128 + PLUS should produce 170.
 * Mutation: ILIT 128 produces -128 (sign-extend); 42+(-128) = -86 != 170. ---- */
static int test_ilit_arithmetic_divergence(void)
{
  /* Hand-assembled: ILIT 42 | ILIT 128 | PLUS | EXIT */
  uint16_t w = begin_word("WA", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  forthDictEmit(T_ILIT);
  emit_int32(128);
  forthDictEmit(PRIM_TOKEN(P_PLUS));
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("WA");
  if (err) {
    printf("    FAIL: unexpected error %d during ILIT arithmetic\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(170)) {
    printf("    FAIL: X should be 170 (42+128), got wrong value (sign-extend bug?)\n");
    return 1;
  }
  printf("    PASS: ILIT arithmetic correct (42+128=170, no sign-extend)\n");
  return 0;
}

/* ---- BR delta sign-extend fix (fix #16) ----
 * BR with delta whose high byte != 0 must not sign-extend from byte 0.
 * Tests a forward branch with delta = 260 cells (0x0104).
 * With memcpy, delta = +260 cells.  Without memcpy (old buggy code),
 * byte 0 = 0x04 would be read correctly, but byte 1 = 0x01 could be
 * misinterpreted if the code used int8_t sign-extension.
 * Layout (cells, 2 bytes each):
 *   0: BR token
 *   1: delta +260 (skip 260 cells = 520 bytes)
 *   2-261: T_EXIT fillers (skipped; low-byte +4 hits a filler EXIT)
 *   262: ILIT 777
 *   263-264: int32 777
 *   265: EXIT
 */
static int test_br_delta_sign_extend(void)
{
  uint16_t w = begin_word("BDS", 3);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }

  forthDictEmit(T_BR);
  emit_int16(260);  /* skip 260 cells, delta 0x0104 exercises high-byte path */

  /* Filler: 260 T_EXIT tokens (skipped by BR +260; low-byte +4 hits a filler EXIT) */
  int i;
  for (i = 0; i < 260; i++) {
    forthDictEmit(T_EXIT);
  }

  forthDictEmit(T_ILIT);
  emit_int32(777);
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("BDS");
  if (err) {
    printf("    FAIL: unexpected error %d during large BR\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(777)) {
    printf("    FAIL: X should be 777 after BR skips 260 cells\n");
    return 1;
  }
  printf("    PASS: BR delta decoded correctly (memcpy, no sign-extend, delta=260)\n");
  return 0;
}


/* ==================================================================
 * Sub-phase C acceptance tests  --  DESIGN.md §7.4
 * Exercise fnForthOuter (the REAL entry point), not forthOuterInterpret.
 * ================================================================== */

/* Helper: store a C string as a dtString in REGISTER_X */
static void x_set_string(const char *s)
{
  int32_t len = (int32_t)strlen(s);
  reallocateRegister(REGISTER_X, dtString, TO_BLOCKS(len + 1), amNone);
  xcopy(REGISTER_STRING_DATA(REGISTER_X), s, len + 1);
}

/* ---- ILIT compiled vs interpreted parity (fix #1) ----
 * Compiled ": W 128 + ;" with X=42 -> 170.
 * Interpreted "42 128 +" -> 170.
 * Both paths must agree. Mutation: ILIT sign-extend in compiled path
 * gives -86 (42 + -128) instead of 170. ---- */
static int test_ilit_compile_interpret_parity(void)
{
  /* Compiled path: define : W 128 + ; */
  lastErrorCode = ERROR_NONE;
  x_set_string(": W 128 + ;");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: compile \": W 128 + ;\" error %d\n", lastErrorCode);
    return 1;
  }

  /* Seed X = 42, run W -> expect 170 */
  forthPushInt32(42);
  lastErrorCode = ERROR_NONE;
  bool err = run_word("W");
  if (err) {
    printf("    FAIL: compiled W error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(170)) {
    printf("    FAIL: compiled 42 W -> X != 170 (ILIT sign-extend in compiled path?)\n");
    return 1;
  }

  /* Interpreted path: "42 128 +" -> also 170 */
  x_set_string("42 128 +");
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: interpreted \"42 128 +\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(170)) {
    printf("    FAIL: interpreted \"42 128 +\" -> X != 170\n");
    return 1;
  }

  printf("    PASS: compiled vs interpreted parity (42 128 + -> 170 both ways)\n");
  return 0;
}

/* "3 DUP +" via fnForthOuter -> X == 6 */
static int test_outer_simple_expr(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 DUP +");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"3 DUP +\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(6)) {
    printf("    FAIL: \"3 DUP +\" X != 6\n");
    return 1;
  }
  printf("    PASS: \"3 DUP +\" -> X==6\n");
  return 0;
}

/* ": SQ2 DUP * ;" then "3 SQ2" via fnForthOuter -> X == 9 (§7.4 full acceptance) */
static int test_outer_compile_invoke(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string(": SQ2 DUP * ;");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \": SQ2 DUP * ;\" compile error %d\n", lastErrorCode);
    return 1;
  }

  lastErrorCode = ERROR_NONE;
  x_set_string("3 SQ2");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"3 SQ2\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(9)) {
    printf("    FAIL: \"3 SQ2\" X != 9\n");
    return 1;
  }
  printf("    PASS: \": SQ2 DUP * ;  3 SQ2\" -> X==9\n");
  return 0;
}

/* Forward declarations for test-program helpers (used below, defined later) */
static bool writeTestProgram(const uint8_t *bytes, uint16_t n);
static void cleanupTestProgram(void);

/* §4.1 step 4: C47 item lookup in the outer interpreter.
 * Tests forthFindItem and the new outer arm (compile + interpret). */
static int test_outer_item_lookup(void)
{
  uint16_t itemId;
  int fail = 0;

  /* Subcase 1: forthFindItem("SIN") -> true, itemId == ITM_sin (76) */
  if (!forthFindItem("SIN", &itemId)) {
    printf("    FAIL: forthFindItem(\"SIN\") returned false\n");
    fail = 1;
  } else if (itemId != ITM_sin) {
    printf("    FAIL: forthFindItem(\"SIN\") itemId=%u (expected %d)\n", itemId, ITM_sin);
    fail = 1;
  } else {
    printf("    PASS: subcase 1 — forthFindItem(\"SIN\") -> ITM_sin (%d)\n", ITM_sin);
  }

  /* Subcase 2: forthFindItem("STO") -> false (parameterized item) */
  if (forthFindItem("STO", &itemId)) {
    printf("    FAIL: forthFindItem(\"STO\") returned true (expected false — parameterized)\n");
    fail = 1;
  } else {
    printf("    PASS: subcase 2 — parameterized item STO excluded\n");
  }

  /* Subcase 3: forthFindItem("FORTH") -> false, forthFindItem("FCALL") -> false
   * (CAT_FNCT but PTP_REM / PTP_NUMBER_16 — the PTP_NONE filter) */
  int subcase3Failed = 0;
  if (forthFindItem("FORTH", &itemId)) {
    printf("    FAIL: forthFindItem(\"FORTH\") returned true (expected false — PTP_REM)\n");
    fail = 1;
    subcase3Failed = 1;
  }
  if (forthFindItem("FCALL", &itemId)) {
    printf("    FAIL: forthFindItem(\"FCALL\") returned true (expected false — PTP_NUMBER_16)\n");
    fail = 1;
    subcase3Failed = 1;
  }
  if (!subcase3Failed) {
    printf("    PASS: subcase 3 — FORTH/FCALL excluded by PTP_NONE filter\n");
  }

  /* Subcase 4: Compile ": ISIN SIN ;" and byte-probe the body.
   * Expected: T_C47 (0x7F04), itemId (76 = 0x4C), T_EXIT (0x0000) */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": ISIN SIN ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \": ISIN SIN ;\" compile error %d\n", lastErrorCode);
    fail = 1;
  } else {
    /* ISIN is the latest word; body follows header(6) + name(4) = ceil4(10) = 12 */
    uint16_t hdr = fdict.latest;
    uint8_t *body = fdict.base + hdr + 12;
    if (body[0] != 0x04 || body[1] != 0x7F) {
      printf("    FAIL: body[0..1] = 0x%02X%02X (expected 0x047F = T_C47)\n",
             body[1], body[0]);
      fail = 1;
    } else if (body[2] != 0x4C || body[3] != 0x00) {
      printf("    FAIL: body[2..3] = 0x%02X%02X (expected 0x4C00 = ITM_sin)\n",
             body[3], body[2]);
      fail = 1;
    } else if (body[4] != 0x00 || body[5] != 0x00) {
      printf("    FAIL: body[4..5] = 0x%02X%02X (expected 0x0000 = T_EXIT)\n",
             body[5], body[4]);
      fail = 1;
    } else {
      printf("    PASS: subcase 4 — emitted cells 0x7F04 0x004C 0x0000\n");
    }
  }

  /* Subcase 5: Colon-over-item precedence.
   * Define ": SIN 42 ;" (colon word shadows built-in), interpret "SIN", require X=42. */
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SIN 42 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \": SIN 42 ;\" compile error %d\n", lastErrorCode);
    fail = 1;
  } else {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("SIN");
    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: interpret \"SIN\" error %d\n", lastErrorCode);
      fail = 1;
    } else if (!x_is_longint(42)) {
      printf("    FAIL: X != 42 after \"SIN\" (colon should shadow item)\n");
      fail = 1;
    } else {
      printf("    PASS: subcase 5 — colon SIN shadows native item\n");
    }
  }
  forthDictClear();

  /* Subcase 6: Item-over-label precedence.
   * Write a program with a label named SIN, push 0, interpret "SIN".
   * The ITEM should run (sin(0)=0 as real34), not the LABEL. */
  {
    uint8_t prog[] = { 0x01, 0xFD, 3, 'S', 'I', 'N' };
    cleanupTestProgram();
    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    FAIL: writeTestProgram failed\n");
      fail = 1;
    } else {
      forthPushInt32(0);
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SIN");
      if (lastErrorCode != ERROR_NONE) {
        printf("    FAIL: interpret \"SIN\" error %d\n", lastErrorCode);
        fail = 1;
      } else if (getRegisterDataType(REGISTER_X) != dtReal34) {
        printf("    FAIL: X is not dtReal34 (type %u) — LABEL hijacked the name\n",
               getRegisterDataType(REGISTER_X));
        fail = 1;
      } else if (!real34IsZero(REGISTER_REAL34_DATA(REGISTER_X))) {
        printf("    FAIL: X is real34 but not zero (sin(0) should be 0)\n");
        fail = 1;
      } else {
        printf("    PASS: subcase 6 — native SIN dispatch beats global label SIN\n");
      }
    }
    cleanupTestProgram();
  }

  if (!fail) {
    printf("    PASS: outer item lookup: SIN->ITEM(%d), STO/FORTH/FCALL miss, "
           "compile byte-probe OK, colon beats item, item beats label\n", ITM_sin);
  }
  return fail;
}

/* Non-string X -> ERROR_INVALID_DATA_TYPE_FOR_OP */
static int test_outer_nonstring_x(void)
{
  forthPushInt32(42);  /* X is now dtLongInteger, not dtString */
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) {
    printf("    FAIL: non-string X gave error %d (expected %d)\n",
    lastErrorCode, ERROR_INVALID_DATA_TYPE_FOR_OP);
    return 1;
  }
  printf("    PASS: non-string X -> ERROR_INVALID_DATA_TYPE_FOR_OP (%d)\n",
  ERROR_INVALID_DATA_TYPE_FOR_OP);
  return 0;
}

/* Keyboard glyph: "3 4 " STD_CROSS via fnForthOuter -> X == 12 */
static int test_outer_glyph_cross(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 4 " STD_CROSS);
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"3 4 <cross>\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(12)) {
    printf("    FAIL: \"3 4 <cross>\" X != 12\n");
    return 1;
  }
  printf("    PASS: \"3 4 <cross>\" (STD_CROSS) -> X==12\n");
  return 0;
}

/* Keyboard glyph: "3 4 " STD_DOT via fnForthOuter -> X == 12 */
static int test_outer_glyph_dot(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 4 " STD_DOT);
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"3 4 <dot>\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(12)) {
    printf("    FAIL: \"3 4 <dot>\" X != 12\n");
    return 1;
  }
  printf("    PASS: \"3 4 <dot>\" (STD_DOT) -> X==12\n");
  return 0;
}

/* Keyboard glyph: "8 4 " STD_DIVIDE via fnForthOuter -> X == 2 */
static int test_outer_glyph_divide(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("8 4 " STD_DIVIDE);
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"8 4 <divide>\" error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(2)) {
    printf("    FAIL: \"8 4 <divide>\" X != 2\n");
    return 1;
  }
  printf("    PASS: \"8 4 <divide>\" (STD_DIVIDE) -> X==2\n");
  return 0;
}

/* Forward declarations for test-program helpers (defined later in file) */
static bool writeTestProgram(const uint8_t *bytes, uint16_t n);
static void cleanupTestProgram(void);

/* T3.5 (D-3): a Forth line XEQs a label whose program contains a Forth
 * source step (outer-in-outer). The OUTER line's remaining tokens must still
 * be consumed after the nested line. Must fail if tokenizer state is shared
 * statics (nested init clobbers the outer position). */
static int test_outer_nesting_tokenizer(void)
{
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'N', 'L', 'B',                         /* LBL 'NLB' */
    0x8B, 0x1A, 0xFD, 0x01, '3',                              /* ITM_FORTH "3" */
    0x04                                                      /* RTN (ITM_RTN=4, PTP_NONE: single byte) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  calcRegister_t lbl = findNamedLabel("NLB", GLOBAL_LABELS);
  if (lbl == INVALID_VARIABLE) {
    printf("    FAIL: findNamedLabel(\"NLB\") returned INVALID_VARIABLE\n");
    cleanupTestProgram();
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("NLB 5");

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"NLB 5\" raised error %d\n", lastErrorCode);
    cleanupTestProgram();
    return 1;
  }
  if (!x_is_longint(5)) {
    printf("    FAIL: X != 5 (outer tail token not consumed after nested run)\n");
    cleanupTestProgram();
    return 1;
  }
  if (!y_is_longint(3)) {
    printf("    FAIL: Y != 3 (nested Forth step result lost)\n");
    cleanupTestProgram();
    return 1;
  }
  printf("    PASS: outer nesting preserves tokenizer — X=5, Y=3\n");
  cleanupTestProgram();
  return 0;
}

/* T3.6 (rewritten by architect ruling): the outer depth cap is UNREACHABLE
 * by natural construction — a label XEQ from a program-context Forth step is
 * continuation-style (fnExecute's nested branch pushes a level and defers to
 * the enclosing runProgram loop), so interpreter frames never stack past 2.
 * Phase A therefore primes the depth via test hook and pins the cap check
 * itself: must fail if the FORTH_OUTER_NEST_MAX guard in forthOuterRun is
 * removed (the primed line would execute). Phase B keeps the two-program
 * construct as a continuation-XEQ integration test: must fail if fnExecute's
 * nested branch stops working from a Forth step (X != 3) or leaks its
 * subroutine level / leaves outer depth dangling. */
static int test_outer_depth_cap(void)
{
  int fail = 0;

  /* ---- Phase A: hook-primed cap + recovery ---- */
  forthTestSetOuterDepth(2);   /* == FORTH_OUTER_NEST_MAX */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("11");
  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL: primed cap: error = %d, expected ERROR_OPERATION_UNDEFINED\n",
           lastErrorCode);
    fail = 1;
  }
  if (x_is_longint(11)) {
    printf("    FAIL: primed cap: line executed past the depth guard\n");
    fail = 1;
  }
  forthTestSetOuterDepth(0);
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("11");
  if (lastErrorCode != ERROR_NONE || !x_is_longint(11)) {
    printf("    FAIL: recovery line \"11\" failed after cap test\n");
    fail = 1;
  }

  /* ---- Phase B: continuation XEQ across two labels ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'N', 'L', 'B',                         /* LBL 'NLB' */
      0x8B, 0x1A, 0xFD, 0x01, '3',                              /* ITM_FORTH "3" */
      0x04,                                                      /* RTN (ITM_RTN=4, PTP_NONE: single byte) */
      0x01, 0xFD, 0x03, 'N', 'L', '2',                         /* LBL 'NL2' */
      0x8B, 0x1A, 0xFD, 0x03, 'N', 'L', 'B',                    /* ITM_FORTH "NLB" */
      0x04                                                      /* RTN (ITM_RTN=4, PTP_NONE: single byte) */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    FAIL: writeTestProgram failed\n");
      return 1;
    }

    forthRunGenBump();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("NL2");

    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: \"NL2\" raised error %d\n", lastErrorCode);
      fail = 1;
    }
    if (!x_is_longint(3)) {
      printf("    FAIL: X != 3 (continuation XEQ NL2->NLB did not run NLB's step)\n");
      fail = 1;
    }
    if (forthTestOuterDepth() != 0) {
      printf("    FAIL: forthOuterDepth = %u at rest after continuation run\n",
             (unsigned)forthTestOuterDepth());
      fail = 1;
    }
    cleanupTestProgram();
  }

  if (fail) return 1;
  printf("    PASS: outer depth cap pinned via hook; continuation XEQ NL2->NLB -> X=3\n");
  return 0;
}

/* T3.7: after any nesting episode, forthOuterCur must be NULL at rest.
 * Must fail if an exit path restores depth but not the ctx pointer
 * (use-after-return into a dead stack frame on the next line). */
static int test_outer_ctx_at_rest(void)
{
  int fail = 0;

  /* Simple line: no nesting */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("1 2 +");
  if (forthTestOuterCur() != NULL) {
    printf("    FAIL: forthOuterCur != NULL after simple line\n");
    fail = 1;
  }
  if (forthTestOuterDepth() != 0) {
    printf("    FAIL: forthOuterDepth = %u after simple line\n",
           (unsigned)forthTestOuterDepth());
    fail = 1;
  }

  /* Nested scenario (same as T3.5) */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'N', 'L', 'B',                         /* LBL 'NLB' */
      0x8B, 0x1A, 0xFD, 0x01, '3',                              /* ITM_FORTH "3" */
      0x04                                                      /* RTN (ITM_RTN=4, PTP_NONE: single byte) */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    FAIL: writeTestProgram failed\n");
      return 1;
    }

    printf("    [DBG] after writeTestProgram, numberOfPrograms=%u\n", numberOfPrograms);
    forthRunGenBump();
    lastErrorCode = ERROR_NONE;
    printf("    [DBG] before forthOuterInterpret(\"NLB 5\")\n");
    fflush(stdout);
    forthOuterInterpret("NLB 5");
    printf("    [DBG] after forthOuterInterpret(\"NLB 5\"), err=%d\n", lastErrorCode);
    fflush(stdout);

    printf("    [DBG] calling forthTestOuterCur...\n");
    fflush(stdout);
    void *cur = forthTestOuterCur();
    printf("    [DBG] forthTestOuterCur returned %p\n", cur);
    fflush(stdout);
    if (cur != NULL) {
      printf("    FAIL: forthOuterCur != NULL after nested episode\n");
      fail = 1;
    }
    printf("    [DBG] checking forthTestOuterDepth...\n");
    fflush(stdout);
    uint8_t depth = forthTestOuterDepth();
    printf("    [DBG] forthTestOuterDepth returned %u\n", depth);
    fflush(stdout);
    if (depth != 0) {
      printf("    FAIL: forthOuterDepth = %u after nested episode\n", depth);
      fail = 1;
    }

    printf("    [DBG] calling cleanupTestProgram...\n");
    fflush(stdout);
    cleanupTestProgram();
    printf("    [DBG] cleanupTestProgram done\n");
    fflush(stdout);
  }

  if (fail) return 1;
  printf("    PASS: forthOuterCur/forthOuterDepth NULL/0 at rest (simple + nested)\n");
  return 0;
}


/* ==================================================================
 * Main self-test entry point
 * ================================================================== */

/* ==================================================================
 * H1 acceptance tests  --  DESIGN.md §7.1 / §7.5
 * ================================================================== */

/* R2-T7: renamed from test_xeq_end_to_end, which claimed to test XEQ of
 * ITM_FORTH/ITM_FCALL but calls fnForthCall directly — it exercises no XEQ
 * step, ITM_FORTH, or ITM_FCALL dispatch at all. Describes only what it
 * actually calls/asserts: fnForthCall executing a colon word by dictionary
 * index, plus a static indexOfItems bounds check on ITM_FCALL/ITM_FORTH.
 * TODO: missing acceptance coverage: real XEQ execution of ITM_FORTH and
 * ITM_FCALL; fixture deferred pending an independently verified reachable
 * path. */
static int test_fnforthcall_executes_colon_by_index(void)
{
  /* Verify indexOfItems bounds: ITM_FCALL (2843) < LAST_ITEM */
  if (ITM_FCALL >= LAST_ITEM) {
    printf("    FAIL: ITM_FCALL (%d) >= LAST_ITEM (%d)\n", ITM_FCALL, LAST_ITEM);
    return 1;
  }
  if (ITM_FORTH >= LAST_ITEM) {
    printf("    FAIL: ITM_FORTH (%d) >= LAST_ITEM (%d)\n", ITM_FORTH, LAST_ITEM);
    return 1;
  }

  /* Define word "XEQV": ILIT(42) EXIT */
  uint16_t w = begin_word("XEQV", 4);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(T_ILIT);
  emit_int32(42);
  end_word(w);

  /* Get dictionary index */
  uint16_t idx;
  if (!forthFindColon("XEQV", &idx)) {
    printf("    FAIL: cannot find XEQV\n");
    return 1;
  }

  /* Call via fnForthCall (simulates ITM_FCALL dispatch) */
  /* fnForthCall sets fromProgram=true, which checks programRunStop.
   * Must set PGM_RUNNING so forthInner doesn't break immediately. */
  uint8_t savedRunStop = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  fnForthCall(idx);
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: fnForthCall raised error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(42)) {
    printf("    FAIL: X != 42 after fnForthCall\n");
    return 1;
  }
  printf("    PASS: fnForthCall(XEQV) -> X=42, no crash, ITM_FCALL < LAST_ITEM\n");
  return 0;
}

/* §4.2 TAM dispatcher: reallyRunFunction(ITM_FCALL, idx) executes Forth word.
 * Tests the exact dispatch path used by the tam.c H-hook (DESIGN.md §4.2).
 * Mutation: remove H-hook from tam.c -> ERROR_FUNCTION_NOT_FOUND instead of
 *   executing the word. */
static int test_tam_dispatcher(void)
{
  /* Define word "TAMX": ILIT(77) EXIT */
  uint16_t w = begin_word("TAMX", 4);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(T_ILIT);
  emit_int32(77);
  end_word(w);

  /* Get dictionary index */
  uint16_t idx;
  if (!forthFindColon("TAMX", &idx)) {
    printf("    FAIL: cannot find TAMX\n");
    return 1;
  }

  /* Dispatch via reallyRunFunction — the exact path the tam.c H-hook uses:
   *   reallyRunFunction(ITM_FCALL, widx);  (tam.c ~line 970) */
  uint8_t savedRunStop = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  reallyRunFunction(ITM_FCALL, idx);
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: reallyRunFunction(ITM_FCALL) raised error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(77)) {
    printf("    FAIL: X should be 77 after reallyRunFunction(ITM_FCALL, TAMX), got wrong value\n");
    return 1;
  }
  printf("    PASS: reallyRunFunction(ITM_FCALL, TAMX) -> X=77, TAM dispatch path works\n");
  return 0;
}

/* AUD-U1: a tam.colon (LOCAL) request never falls through to Forth vocabulary.
 * Drives the real public TAM chain: tamEnterMode, TAM alpha entry, and the
 * letter items' runFunction -> addItemToBuffer -> tamProcessInput path.
 * Control leg (no colon): global request -> Forth fallback dispatches word.
 * Colon leg: local request -> ERROR_FUNCTION_NOT_FOUND, nothing dispatched. */
static int test_tam_colon_never_falls_to_forth(void)
{
  uint8_t savedCalcMode = calcMode;
  uint8_t savedLastError = lastErrorCode;
  uint8_t savedRunStop = programRunStop;
  char savedAimBuffer[AIM_BUFFER_LENGTH];
  memcpy(savedAimBuffer, aimBuffer, sizeof(savedAimBuffer));
  tamState_t savedTam = tam;
  int fail = 0;

  extern void runFunction(int16_t func);

  forthDictClear();
  /* Define colon word FOO: ILIT(42) EXIT */
  {
    uint16_t w = begin_word("FOO", 3);
    if (w == FORTH_NULL) {
      printf("    SKIP: alloc failed\n");
      return 0;
    }
    forthDictEmit(T_ILIT);
    emit_int32(42);
    end_word(w);
  }

  uint16_t savedFdictCount = fdict.count;

  /* Push sentinel */
  forthPushInt32(31337);

  /* ---- Control leg: global request (no colon) dispatches Forth word ---- */
  calcMode = CM_NORMAL;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  aimBuffer[0] = 0;
  tamEnterMode(ITM_XEQ);
  tamProcessInput(ITM_alpha);
  runFunction(ITM_F);
  runFunction(ITM_O);
  runFunction(ITM_O);
  if (strcmp(aimBuffer, "FOO") != 0) {
    printf("    FAIL(control): TAM letter path produced '%s', expected 'FOO'\n",
           aimBuffer);
    fail = 1;
  }
  tamProcessInput(ITM_ENTER);

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL(control): expected no error, got %d\n", lastErrorCode);
    fail = 1;
  } else if (!x_is_longint(42)) {
    printf("    FAIL(control): X should be 42 (Forth word dispatched), got wrong value\n");
    fail = 1;
  } else {
    printf("    PASS(control): global XEQ FOO -> Forth fallback dispatched FOO, X=42\n");
  }

  /* ---- Reset ---- */
  lastErrorCode = ERROR_NONE;
  aimBuffer[0] = 0;
  forthPushInt32(31337);

  /* ---- Colon leg: local request (:FOO) never falls to Forth ---- */
  tamEnterMode(ITM_XEQ);
  tamProcessInput(ITM_COLON);
  tamProcessInput(ITM_alpha);
  runFunction(ITM_F);
  runFunction(ITM_O);
  runFunction(ITM_O);
  if (strcmp(aimBuffer, "FOO") != 0) {
    printf("    FAIL(colon): TAM letter path produced '%s', expected 'FOO'\n",
           aimBuffer);
    fail = 1;
  }
  tamProcessInput(ITM_ENTER);

  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL(colon): expected ERROR_FUNCTION_NOT_FOUND (%d), got %d\n",
           ERROR_FUNCTION_NOT_FOUND, lastErrorCode);
    fail = 1;
  } else if (!x_is_longint(31337)) {
    printf("    FAIL(colon): X should be 31337 (sentinel, nothing dispatched)\n");
    fail = 1;
  } else if (fdict.count != savedFdictCount) {
    printf("    FAIL(colon): fdict.count changed from %u to %u\n",
           savedFdictCount, fdict.count);
    fail = 1;
  } else {
    printf("    PASS(colon): local XEQ :FOO -> ERROR_FUNCTION_NOT_FOUND, X=31337, fdict unchanged\n");
  }

  /* Cleanup */
  forthDictClear();
  calcMode = savedCalcMode;
  lastErrorCode = savedLastError;
  programRunStop = savedRunStop;
  memcpy(aimBuffer, savedAimBuffer, sizeof(savedAimBuffer));
  tam = savedTam;

  return fail;
}

/* §7.1 re-entrancy: depth cap fires at FORTH_NEST_MAX and recovers (§3.2)
 * Uses test-only forthTestSetDepth to prime the guard, avoiding
 * reallyRunFunction/display calls that may not be safe in headless mode.
 * Requires FORTH_DEBUG_SELFTEST (meson OPTION). */
#ifdef FORTH_DEBUG_SELFTEST
static int test_reentrancy(void)
{
  /* Define word: ILIT(99) EXIT */
  uint16_t w = begin_word("RTEST", 5);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(T_ILIT);
  emit_int32(99);
  end_word(w);

  /* Get dictionary index */
  uint16_t idx;
  if (!forthFindColon("RTEST", &idx)) {
    printf("    FAIL: cannot find RTEST\n");
    return 1;
  }

  /* Prime the depth cap */
  forthTestSetDepth(FORTH_NEST_MAX);

  /* Sentinel distinct from RTEST's own ILIT(99), so the post-call check
   * below proves X was left untouched rather than coincidentally landing
   * on 99 from whatever the previous test left on the stack. */
  forthPushInt32(77);

  /* Call forthInner — guard should fire, word should NOT execute */
  lastErrorCode = ERROR_NONE;
  forthInner(idx, false);

  /* Reset depth for subsequent tests */
  forthTestSetDepth(0);

  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL: expected ERROR_OPERATION_UNDEFINED (%d), got %d\n",
    ERROR_OPERATION_UNDEFINED, lastErrorCode);
    return 1;
  }
  if (!x_is_longint(77)) {
    printf("    FAIL: X changed from sentinel 77 — guard did not prevent entry\n");
    return 1;
  }
  if (forthTestGetDepth() != 0) {
    printf("    FAIL: forthDepth still nonzero after manual reset\n");
    return 1;
  }
  printf("    PASS: depth cap fired (err=%d), word not executed\n",
  lastErrorCode);
  return 0;
}
#endif /* FORTH_DEBUG_SELFTEST */

/* §7.5 precedence: forthResolveXEQ returns LABEL when C47 label shadows Forth word */
static int test_xeq_precedence(void)
{
  const char *sharedName = "XEQP";
  uint8_t nameLen = (uint8_t)sizeof("XEQP") - 1;

  if (numberOfLabels >= 4096) {
    printf("    SKIP: label table full\n");
    return 0;
  }

  /* Label data must be in C47 memory (below firstFreeProgramByte) because
   * boundProgramNameLength rejects addresses >= firstFreeProgramByte. */
  uint8_t *labelBuf = allocC47Blocks(TO_BLOCKS(nameLen + 1));
  if (labelBuf == NULL) {
    printf("    SKIP: alloc labelBuf failed\n");
    return 0;
  }
  labelBuf[0] = nameLen;
  memcpy(labelBuf + 1, sharedName, nameLen);

  /* Expand labelList to accommodate the new entry.
   * After scanLabelsAndPrograms, labelList may be small or empty. */
  size_t oldBlocks = TO_BLOCKS(sizeof(labelList_t)) * numberOfLabels;
  size_t newBlocks = TO_BLOCKS(sizeof(labelList_t)) * (numberOfLabels + 1);
  if (oldBlocks == 0) oldBlocks = 1; /* freeListAlloc normalises 0→1 */
  labelList_t *expanded = reallocC47Blocks(labelList, oldBlocks, newBlocks);
  if (expanded == NULL) {
    freeC47Blocks(labelBuf, TO_BLOCKS(nameLen + 1));
    printf("    SKIP: realloc failed\n");
    return 0;
  }
  labelList = expanded;

  labelList[numberOfLabels].program = 1;
  labelList[numberOfLabels].step = 1;
  labelList[numberOfLabels].labelPointer = labelBuf;
  labelList[numberOfLabels].instructionPointer = labelBuf;
  numberOfLabels++;

  /* Define a Forth word with the same name */
  uint16_t w = begin_word(sharedName, nameLen);
  if (w == FORTH_NULL) {
    /* reallocC47Blocks() above already freed the pre-expansion labelList
     * block (freeListRealloc frees the old pointer on success — core/
     * freeList.c:90 [VERIFIED]); rescanning rebuilds labelList/
     * numberOfLabels from the (unmodified) program memory instead of
     * reinstating a pointer/size pair that a later scanLabelsAndPrograms()
     * would free a second time. */
    scanLabelsAndPrograms();
    freeC47Blocks(labelBuf, TO_BLOCKS(nameLen + 1));
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(T_ILIT);
  emit_int32(77);
  end_word(w);

  /* forthResolveXEQ should return LABEL (C47 label takes precedence) */
  uint16_t param;
  forthXEQType_t res = forthResolveXEQ(sharedName, &param);

  /* Rescan to rebuild labelList/numberOfLabels from the (unmodified) program
   * memory. The old "Fix #15" approach of restoring savedLabelList/
   * savedNumLabels here was itself a double-free: reallocC47Blocks() already
   * freed the pre-expansion labelList block on success (freeListRealloc
   * frees the old pointer unconditionally — core/freeList.c:90 [VERIFIED]),
   * so reinstating that stale pointer left the next scanLabelsAndPrograms()
   * call freeing already-freed memory. */
  scanLabelsAndPrograms();
  freeC47Blocks(labelBuf, TO_BLOCKS(nameLen + 1));

  if (res != FORTH_XEQ_LABEL) {
    printf("    FAIL: forthResolveXEQ returned %d (expected %d = FORTH_XEQ_LABEL)\n",
    res, FORTH_XEQ_LABEL);
    return 1;
  }
  printf("    PASS: forthResolveXEQ returns LABEL when C47 label shadows Forth word\n");
  return 0;
}

/* §4.2 XEQ item lookup: forthResolveXEQ finds C47 built-in items by name.
 * Tests: FORTH -> ITM_FORTH, FCALL -> ITM_FCALL, label shadows item. */
static int test_xeq_item_lookup(void)
{
  uint16_t param;
  forthXEQType_t res;
  int fail = 0;

  /* Test 1: "FORTH" resolves to ITM_FORTH */
  res = forthResolveXEQ("FORTH", &param);
  if (res != FORTH_XEQ_ITEM || param != ITM_FORTH) {
    printf("    FAIL: forthResolveXEQ(\"FORTH\") returned %d/%u (expected ITEM/%d)\n",
    res, param, ITM_FORTH);
    fail = 1;
  }

  /* Test 2: "FCALL" rejected by B3 reverse (PTP_NUMBER_16 is parameterized) */
  res = forthResolveXEQ("FCALL", &param);
  if (res != FORTH_XEQ_NONE) {
    printf("    FAIL: forthResolveXEQ(\"FCALL\") returned %d/%u (expected NONE — B3 reject)\n",
    res, param);
    fail = 1;
  }

  /* Test 3: a non-existent name returns NONE */
  res = forthResolveXEQ("NONEXISTENT_ITEM", &param);
  if (res != FORTH_XEQ_NONE) {
    printf("    FAIL: forthResolveXEQ(\"NONEXISTENT_ITEM\") returned %d (expected NONE)\n",
    res);
    fail = 1;
  }

  /* Test 4 (R2-T7 item 2): item-before-colon precedence. A Forth colon word
   * named SIN must NOT shadow the built-in item ITM_sin — resolver order is
   * label -> item -> colon (forth_dict.c:390-419); test_xeq_precedence
   * already pins label->colon, this pins item->colon. */
  forthDictClear();
  {
    uint16_t w = begin_word("SIN", 3);
    if (w == FORTH_NULL) {
      printf("    SKIP: alloc failed for colon word SIN\n");
    } else {
      forthDictEmit(T_ILIT);
      emit_int32(1);
      end_word(w);

      res = forthResolveXEQ("SIN", &param);
      if (res != FORTH_XEQ_ITEM || param != ITM_sin) {
        printf("    FAIL: forthResolveXEQ(\"SIN\") returned %d/%u (expected ITEM/%d) "
               "— colon word shadowed the built-in item\n", res, param, ITM_sin);
        fail = 1;
      }
    }
  }
  forthDictClear();

  if (!fail) {
    printf("    PASS: XEQ item lookup: FORTH->ITEM(%d), FCALL->NONE (B3), miss->NONE, "
           "item SIN beats colon SIN\n", ITM_FORTH);
  }
  return fail;
}

/* F3-6: XEQ source forms, FTOK_XEQN, kind-faithful end to end */
static int test_xeqn(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* ---- Subcase 1: Compile shape, both kinds ---- */
  {
    forthDictClear();
    forthOuterInterpret(": X1 XEQ 'AB' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: XEQ 'AB' compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t off = fdict.latest;
      uint16_t bodyOff = off + (uint16_t)TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK;
      uint8_t expected1[] = {0x05, 0x7F, 0xFD, 0x02, 0x41, 0x42, 0x00, 0x00};
      int ok = 1;
      for (int i = 0; i < (int)(sizeof(expected1)); i++) {
        if (fdict.base[bodyOff + i] != expected1[i]) {
          printf("    [1] FAIL: byte %d: 0x%02x != 0x%02x\n",
                 i, fdict.base[bodyOff + i], expected1[i]);
          ok = 0; break;
        }
      }
      if (!ok) fail = 1;

      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": X2 XEQ :CDE: ;");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: XEQ :CDE: compile error %d\n", lastErrorCode);
        fail = 1;
      } else {
        off = fdict.latest;
        bodyOff = off + (uint16_t)TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK;
        uint8_t expected2[] = {0x05, 0x7F, 0xF9, 0x03, 0x43, 0x44, 0x45, 0x00, 0x00, 0x00};
        for (int i = 0; i < (int)(sizeof(expected2)); i++) {
          if (fdict.base[bodyOff + i] != expected2[i]) {
            printf("    [1] FAIL: X2 byte %d: 0x%02x != 0x%02x\n",
                   i, fdict.base[bodyOff + i], expected2[i]);
            ok = 0; break;
          }
        }
        if (!ok) fail = 1;
      }
    }
    if (!fail) printf("    [1] PASS: XEQN encodes kind/len/name/pad exactly\n");
  }

  /* ---- Subcase 2: Interpret-state global hit runs the program ---- */
  {
    forthDictClear();
    cleanupTestProgram();
    testProg_t tp;
    tpInit(&tp);
    int sLbl = tpLbl(&tp, "TG");
    int sSrc = tpSrc(&tp, "77");
    (void)tpEnd(&tp);
    if (sLbl < 0 || sSrc < 0 || !tpWrite(&tp)) {
      printf("    [2] FAIL: fixture build/write failed\n");
      fail = 1;
    } else {
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;
      dynamicMenuItem = -1;
      forthRunGenBump();
      programRunStop = PGM_RUNNING;
      currentStep = tpStepAddr(&tp, sSrc);
      executeOneStep(currentStep);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: program setup error %d\n", lastErrorCode);
        fail = 1;
      } else {
        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("XEQ 'TG'");
        if (lastErrorCode != ERROR_NONE) {
          printf("    [2] FAIL: XEQ 'TG' error %d\n", lastErrorCode);
          fail = 1;
        } else if (!x_is_longint(77)) {
          printf("    [2] FAIL: X != 77 after XEQ 'TG'\n");
          fail = 1;
        }
      }
    }
    if (!fail) printf("    [2] PASS: XEQ 'NAME' interpret dispatch reaches the native XEQ path\n");
    cleanupTestProgram();
  }

  /* ---- Subcase 3: Local requests never fall back ---- */
  {
    forthDictClear();
    forthOuterInterpret(": ZZ 8 ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: define ZZ error %d\n", lastErrorCode);
      fail = 1;
    } else {
      forthPushInt32(55);
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("XEQ :ZZ:");
      if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
        printf("    [3] FAIL: expected ERROR_LABEL_NOT_FOUND got %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(55)) {
        printf("    [3] FAIL: X changed from 55\n");
        fail = 1;
      }
    }
    if (!fail) printf("    [3] PASS: kind 249 miss is terminal — no fallback\n");
    lastErrorCode = ERROR_NONE;
  }

  /* ---- Subcase 4: Global-kind fallback chain, prim then colon ---- */
  {
    forthDictClear();
    forthPushInt32(5);
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'DUP'");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: XEQ 'DUP' error %d\n", lastErrorCode);
      fail = 1;
    } else if (!x_is_longint(5) || !y_is_longint(5)) {
      printf("    [4] FAIL: X or Y != 5 after DUP\n");
      fail = 1;
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": CW 9 ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: define CW error %d\n", lastErrorCode);
      fail = 1;
    } else {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("XEQ 'CW'");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: XEQ 'CW' error %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(9)) {
        printf("    [4] FAIL: X != 9 after XEQ 'CW'\n");
        fail = 1;
      }
    }
    if (!fail) printf("    [4] PASS: global-kind miss falls back prim-then-colon\n");
  }

  /* ---- Subcase 5: Compiled XEQN dispatches at run time, from both regions ---- */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": XR XEQ 'CW' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: define XR error %d\n", lastErrorCode);
      fail = 1;
    } else {
      if (run_word("XR")) {
        printf("    [5] FAIL: run XR error %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(9)) {
        printf("    [5] FAIL: X != 9 after run XR\n");
        fail = 1;
      }
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": GX XEQ 'CW' ; GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: define GX GLOBAL error %d\n", lastErrorCode);
      fail = 1;
    } else {
      if (run_word("GX")) {
        printf("    [5] FAIL: run GX error %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(9)) {
        printf("    [5] FAIL: X != 9 after run GX\n");
        fail = 1;
      }
    }
    if (!fail) printf("    [5] PASS: FTOK_XEQN runs from transient and global bodies\n");
  }

  /* ---- Subcase 6: Corrupted inline data rejects ---- */
  {
    /* Runtime: bad kind byte */
    {
      uint16_t w = begin_word("XC", 2);
      if (w == FORTH_NULL) {
        printf("    [6] FAIL: alloc failed for XC\n");
        fail = 1;
      } else {
        forthDictEmit((ftoken_t)T_XEQN);
        uint8_t bad[] = {0xAA, 0x02, 'A', 'B'};
        forthDictEmitBytes(bad, sizeof(bad));
        end_word(w);
        lastErrorCode = ERROR_NONE;
        if (run_word("XC")) {
          if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
            printf("    [6] FAIL: runtime bad kind: expected ERROR_INVALID_CORRUPTED_DATA got %d\n",
                   lastErrorCode);
            fail = 1;
          }
        } else {
          printf("    [6] FAIL: runtime bad kind: no error\n");
          fail = 1;
        }
        lastErrorCode = ERROR_NONE;
      }
    }

    /* Validator: zero length */
    {
      uint16_t w = gbegin_word("XV", 2);
      if (w == FORTH_NULL) {
        printf("    [6] FAIL: galloc failed for XV\n");
        fail = 1;
      } else {
        gemit((ftoken_t)T_XEQN);
        uint8_t zlen[] = {0xFD, 0x00};
        gemit_bytes(zlen, sizeof(zlen));
        gend_word();
        uint8_t *savedBase = gdict.base;
        uint16_t savedBlocks = gdict.sizeBlocks;
        forthGDictValidateRestored();
        if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
          printf("    [6] FAIL: validator zero len: expected ERROR_INVALID_CORRUPTED_DATA got %d\n",
                 lastErrorCode);
          fail = 1;
        }
        lastErrorCode = ERROR_NONE;
        if (savedBase) freeC47Blocks(savedBase, savedBlocks);
      }
    }
    if (!fail) printf("    [6] PASS: bad kind and zero length reject at run and restore\n");
  }

  /* ---- Subcase 7: B3 forward ---- */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("STO");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [7] FAIL: STO: expected ERROR_INVALID_NAME got %d\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;

    forthOuterInterpret(": BX STO ;");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [7] FAIL: STO compile: expected ERROR_INVALID_NAME got %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t r;
      if (forthFindColon("BX", &r)) {
        printf("    [7] FAIL: BX should not exist (atomic abort)\n");
        fail = 1;
      }
    }
    lastErrorCode = ERROR_NONE;

    forthOuterInterpret("XEQ AB");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [7] FAIL: XEQ AB: expected ERROR_INVALID_NAME got %d\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;

    forthOuterInterpret("XEQ");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [7] FAIL: XEQ bare: expected ERROR_INVALID_NAME got %d\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;

    if (!fail) printf("    [7] PASS: bare parameterized items and malformed XEQ forms reject atomically\n");
  }

  forthDictClear();
  cleanupTestProgram();
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* test_xeqn_acceptance
 * F3-7: four independent pins at the native/Forth resolution boundary.
 * No product behavior changes — all instrumentation is FORTH_DEBUG_SELFTEST. */
static int test_xeqn_acceptance(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint32_t savedFirstDisplayedLocalStepNumber = firstDisplayedLocalStepNumber;

  /* ---- Subcase 1: Native/Forth mimicry selects the same NEXT local instance ---- */
  {
    int variant;
    for (variant = 0; variant < 2; variant++) {
      testProg_t tp;
      int sLblMim, sLblT1, sBody1, sRtn1, sLblT2, sBody2, sRtn2, sP, sEnd;
      int ok = 1;

      tpInit(&tp);
      sLblMim = tpLbl(&tp, "MIM");
      if (sLblMim < 0) { ok = 0; }
      sLblT1 = tpLblLocal(&tp, "T");
      if (sLblT1 < 0) { ok = 0; }
      sBody1 = tpSrc(&tp, "11");
      if (sBody1 < 0) { ok = 0; }
      sRtn1 = tpRtn(&tp);
      if (sRtn1 < 0) { ok = 0; }
      if (variant == 0) {
        sP = tpXeqLocal(&tp, "T");
      } else {
        sP = tpSrc(&tp, "XEQ :T:");
      }
      if (sP < 0) { ok = 0; }
      sLblT2 = tpLblLocal(&tp, "T");
      if (sLblT2 < 0) { ok = 0; }
      sBody2 = tpSrc(&tp, "22");
      if (sBody2 < 0) { ok = 0; }
      sRtn2 = tpRtn(&tp);
      if (sRtn2 < 0) { ok = 0; }
      sEnd = tpEnd(&tp);
      (void)sEnd;
      if (sEnd < 0) { ok = 0; }

      if (!ok) {
        printf("    [1] FAIL: fixture build failed (variant %d)\n", variant);
        fail = 1;
        continue;
      }

      if (!tpWrite(&tp)) {
        printf("    [1] FAIL: tpWrite failed (variant %d)\n", variant);
        fail = 1;
        continue;
      }

       firstDisplayedLocalStepNumber = 0;
       forthRunGenBump();
       programRunStop = PGM_RUNNING;
       lastErrorCode = ERROR_NONE;

       if (!tpSelectStep(&tp, sP)) {
        printf("    [1] FAIL: tpSelectStep(sP) failed (variant %d)\n", variant);
        fail = 1;
        cleanupTestProgram();
        continue;
      }

      /* Execute: for native XEQ (variant 0) executeOneStep only performs the
         jump to the label — the subroutine body must be stepped through
         manually.  For Forth XEQ (variant 1) executeOneStep dispatches to
         forthProgramStep which runs the full XEQ chain in one call. */
      {
        int savedSubLevel = currentSubroutineLevel;
        executeOneStep(currentStep);
        /* Continue stepping while in the subroutine (native variant) or until
           the step returns normally (Forth variant returns 1 from
           forthProgramStep). */
        while (lastErrorCode == ERROR_NONE &&
               (currentSubroutineLevel > savedSubLevel ||
                (variant == 0 && currentStep != NULL))) {
          if (variant == 0 && currentSubroutineLevel <= savedSubLevel) {
            /* Native: subroutine returned — stop. */
            break;
          }
          int16_t adv = executeOneStep(currentStep);
          if (adv <= 0) break;
          /* Advance past the step (needed for LBL no-ops, etc.) */
          if (adv > 0) {
            uint8_t *next = findNextStep(currentStep);
            if (!next) break;
            currentStep = next;
          }
        }
      }
      firstDisplayedLocalStepNumber = 0;

      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: error %d (variant %d)\n", lastErrorCode, variant);
        fail = 1;
        cleanupTestProgram();
        continue;
      }
      if (!x_is_longint(22)) {
        printf("    [1] FAIL: X != 22 (variant %d)\n", variant);
        fail = 1;
        cleanupTestProgram();
        continue;
      }
      cleanupTestProgram();
    }
    if (!fail) printf("    [1] PASS: native and Forth local XEQ select the same next label instance\n");
  }

  /* ---- Subcase 2: A local miss is terminal; no word or global label dispatches ---- */
  {
    testProg_t tp;
    int sLbl, sPrime, sMiss, sEnd;
    int ok = 1;

    tpInit(&tp);
    sLbl = tpLbl(&tp, "MISS");
    if (sLbl < 0) { ok = 0; }
    sPrime = tpSrc(&tp, "0");
    if (sPrime < 0) { ok = 0; }
    sMiss = tpSrc(&tp, "XEQ :FOO:");
    if (sMiss < 0) { ok = 0; }
    sEnd = tpEnd(&tp);
    if (sEnd < 0) { ok = 0; }

    if (!ok) {
      printf("    [2] FAIL: fixture build failed\n");
      fail = 1;
    } else if (!tpWrite(&tp)) {
      printf("    [2] FAIL: tpWrite failed\n");
      fail = 1;
    } else {
      firstDisplayedLocalStepNumber = 0;
      forthRunGenBump();
      lastErrorCode = ERROR_NONE;

      /* Consume pre-scan with prime step */
      if (!tpSelectStep(&tp, sPrime)) {
        printf("    [2] FAIL: tpSelectStep(sPrime) failed\n");
        fail = 1;
      } else {
        executeOneStep(currentStep);
      }

      /* Define global Forth word : FOO 88 ; GLOBAL */
      if (!fail) {
        forthOuterInterpret(": FOO 88 ; GLOBAL");
        if (lastErrorCode != ERROR_NONE) {
          printf("    [2] FAIL: define FOO error %d\n", lastErrorCode);
          fail = 1;
        }
      }

      /* Execute the miss step */
      if (!fail) {
        forthPushInt32(55);
        forthTestProgramStepCountReset();
        lastErrorCode = ERROR_NONE;

        if (!tpSelectStep(&tp, sMiss)) {
          printf("    [2] FAIL: tpSelectStep(sMiss) failed\n");
          fail = 1;
        } else {
          executeOneStep(currentStep);

          if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
            printf("    [2] FAIL: expected ERROR_LABEL_NOT_FOUND got %d\n", lastErrorCode);
            fail = 1;
          }
          if (!x_is_longint(55)) {
            printf("    [2] FAIL: X != 55 (global word executed?)\n");
            fail = 1;
          }
          if (forthTestProgramStepCountGet() != 1) {
            printf("    [2] FAIL: step count %u != 1\n", forthTestProgramStepCountGet());
            fail = 1;
          }
        }
      }
    }
    if (!fail) printf("    [2] PASS: local XEQ miss is terminal with no fallback dispatch\n");
    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 3: Kind round-trip and malformed inline data reject before dispatch ---- */
  {
    static const uint8_t quotedBody[8] =
      { 0x05, 0x7F, 0xFD, 0x01, 0x41, 0x00, 0x00, 0x00 };
    static const uint8_t localBody[8] =
      { 0x05, 0x7F, 0xF9, 0x01, 0x41, 0x00, 0x00, 0x00 };
    uint8_t kqBody[8], klBody[8];

    /* Compile : KQ XEQ 'A' ; */
    forthOuterInterpret(": KQ XEQ 'A' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile KQ error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t bodyAddr = fdict.base != NULL
        ? (uint16_t)(fdict.latest + TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK)
        : 0;
      if (fdict.base == NULL || bodyAddr + 8 > fdict.here) {
        printf("    [3] FAIL: KQ body address out of range\n");
        fail = 1;
      } else {
        memcpy(kqBody, fdict.base + bodyAddr, 8);
      }
    }

    /* Compile : KL XEQ :A: ; */
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": KL XEQ :A: ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile KL error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t bodyAddr = fdict.base != NULL
        ? (uint16_t)(fdict.latest + TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK)
        : 0;
      if (fdict.base == NULL || bodyAddr + 8 > fdict.here) {
        printf("    [3] FAIL: KL body address out of range\n");
        fail = 1;
      } else {
        memcpy(klBody, fdict.base + bodyAddr, 8);
      }
    }

    /* Exact body comparisons */
    if (!fail) {
      int i;
      for (i = 0; i < 8; i++) {
        if (kqBody[i] != quotedBody[i]) {
          printf("    [3] FAIL: KQ byte %d: 0x%02x != 0x%02x\n", i, kqBody[i], quotedBody[i]);
          fail = 1;
          break;
        }
      }
    }
    if (!fail) {
      int i;
      for (i = 0; i < 8; i++) {
        if (klBody[i] != localBody[i]) {
          printf("    [3] FAIL: KL byte %d: 0x%02x != 0x%02x\n", i, klBody[i], localBody[i]);
          fail = 1;
          break;
        }
      }
    }

    /* Sole-difference check: index 2 must differ, all others match */
    if (!fail) {
      int i;
      for (i = 0; i < 8; i++) {
        if (i == 2) {
          if (kqBody[i] == klBody[i]) {
            printf("    [3] FAIL: byte 2 should differ (0xFD vs 0xF9)\n");
            fail = 1;
            break;
          }
        } else {
          if (kqBody[i] != klBody[i]) {
            printf("    [3] FAIL: byte %d should match: 0x%02x vs 0x%02x\n", i, kqBody[i], klBody[i]);
            fail = 1;
            break;
          }
        }
      }
    }

     /* Run KQ: expect ERROR_LABEL_NOT_FOUND, X unchanged */
     if (!fail) {
       lastErrorCode = ERROR_NONE;
       forthPushInt32(31);
       lastErrorCode = ERROR_NONE;
       forthOuterInterpret("KQ");
      if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
        printf("    [3] FAIL: KQ run: expected ERROR_LABEL_NOT_FOUND got %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(31)) {
        printf("    [3] FAIL: KQ run: X != 31\n");
        fail = 1;
      }
    }

     /* Run KL: expect ERROR_LABEL_NOT_FOUND, X unchanged */
     if (!fail) {
       lastErrorCode = ERROR_NONE;
       forthPushInt32(31);
       lastErrorCode = ERROR_NONE;
       forthOuterInterpret("KL");
       if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
        printf("    [3] FAIL: KL run: expected ERROR_LABEL_NOT_FOUND got %d\n", lastErrorCode);
        fail = 1;
       } else if (!x_is_longint(31)) {
         printf("    [3] FAIL: KL run: X != 31\n");
         fail = 1;
       }
     }

     /* Malformed XI: FTOK_XEQN + bad kind byte 0xAA */
    if (!fail) {
      uint16_t xiOff = begin_word("XI", 2);
      if (xiOff == FORTH_NULL) {
        printf("    [3] FAIL: begin_word XI failed\n");
        fail = 1;
      } else {
        static const uint8_t xiInline[4] = { 0xAA, 0x01, 0x41, 0x00 };
        forthDictEmit(FTOK_XEQN);
        forthDictEmitBytes(xiInline, 4);
        end_word(xiOff);

         lastErrorCode = ERROR_NONE;
        forthPushInt32(31);
        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("XI");
        if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
          printf("    [3] FAIL: XI: expected ERROR_INVALID_CORRUPTED_DATA got %d\n", lastErrorCode);
          fail = 1;
        } else if (!x_is_longint(31)) {
          printf("    [3] FAIL: XI: X != 31\n");
          fail = 1;
        }
        lastErrorCode = ERROR_NONE;
      }
    }

    /* Malformed XT: FTOK_XEQN + truncated operand (only 1 byte for kind+len) */
    if (!fail) {
      uint16_t truncBody, xtOff;
      xtOff = begin_word("XT", 2);
      if (xtOff == FORTH_NULL) {
        printf("    [3] FAIL: begin_word XT failed\n");
        fail = 1;
      } else {
        truncBody = fdict.here;
        forthDictEmit(FTOK_XEQN);
        forthDictEmit(STRING_LABEL_VARIABLE);
        end_word(xtOff);

        /* Truncate body: token (2 bytes) + 1 operand byte = 3 bytes */
        fdict.here = (uint16_t)(truncBody + 3);

        lastErrorCode = ERROR_NONE;
        forthPushInt32(31);
        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("XT");
        if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
          printf("    [3] FAIL: XT: expected ERROR_INVALID_CORRUPTED_DATA got %d\n", lastErrorCode);
          fail = 1;
        } else if (!x_is_longint(31)) {
          printf("    [3] FAIL: XT: X != 31\n");
          fail = 1;
        }
        lastErrorCode = ERROR_NONE;
      }
    }

    if (!fail) printf("    [3] PASS: XEQN kind round-trip is exact and malformed data cannot dispatch\n");
    forthDictClear();
  }

  /* ---- Subcase 4: Bare names remain GLOBAL_LABELS-only ---- */
  {
    forthGDictClear();
    testProg_t tp;
    int sLbl, sLblLocal, sBody, sRtn, sEnd, sLblFoo, sBodyFoo, sEndFoo;
    int ok = 1;
    uint16_t savedSubLevel = currentSubroutineLevel;
    uint16_t savedAllSubLevel = allSubroutineLevels.numberOfSubroutineLevels;

    tpInit(&tp);
    sLbl = tpLbl(&tp, "PIN");
    if (sLbl < 0) { ok = 0; }
    sLblLocal = tpLblLocal(&tp, "FOO");
    if (sLblLocal < 0) { ok = 0; }
    sBody = tpSrc(&tp, "99");
    if (sBody < 0) { ok = 0; }
    sRtn = tpRtn(&tp);
    if (sRtn < 0) { ok = 0; }
    sEnd = tpEnd(&tp);
    if (sEnd < 0) { ok = 0; }
    sLblFoo = tpLbl(&tp, "FOO");
    if (sLblFoo < 0) { ok = 0; }
    sBodyFoo = tpSrc(&tp, "44");
    if (sBodyFoo < 0) { ok = 0; }
    sEndFoo = tpEnd(&tp);
    (void)sEndFoo;
    if (sEndFoo < 0) { ok = 0; }

    if (!ok) {
      printf("    [4] FAIL: fixture build failed\n");
      fail = 1;
    } else if (!tpWrite(&tp)) {
      printf("    [4] FAIL: tpWrite failed\n");
      fail = 1;
    } else {
      firstDisplayedLocalStepNumber = 0;
      forthRunGenBump();
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;

      /* Bare name FOO should resolve to global label FOO (->44), not local :FOO: (->99) */
      {
        currentSubroutineLevel = 0;
        allSubroutineLevels.numberOfSubroutineLevels = 0;
        forthOuterInterpret("FOO");
        currentSubroutineLevel = savedSubLevel;
        allSubroutineLevels.numberOfSubroutineLevels = savedAllSubLevel;
      }
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: error %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(44)) {
        printf("    [4] FAIL: X != 44 (got wrong label?)\n");
        fail = 1;
      }
    }
    if (!fail) printf("    [4] PASS: bare Forth name lookup ignores a colliding local label\n");
    cleanupTestProgram();
  }

  forthDictClear();
  cleanupTestProgram();
  firstDisplayedLocalStepNumber = savedFirstDisplayedLocalStepNumber;
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* §7.1 fnForthCall interactive (fix #4): fromProgram must reflect programRunStop.
 * Compiles : FIFTEEN 15 ; calls via fnForthCall with PGM_STOPPED.
 * Mutation: revert to hardcoded true -> word exits immediately on break check. ---- */
static int test_fnforthcall_interactive(void)
{
  /* Define word: ILIT(15) EXIT (self-contained, no stack lifting needed) */
  uint16_t w = begin_word("FIFTEEN", 7);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(T_ILIT);
  emit_int32(15);
  end_word(w);

  /* Find the word index */
  uint16_t idx;
  if (!forthFindColon("FIFTEEN", &idx)) {
    printf("    FAIL: cannot find FIFTEEN\n");
    return 1;
  }

  /* Interactive call: programRunStop = PGM_STOPPED (not running).
   * With fix #4: fromProgram = false, word executes fully.
   * Without fix: fromProgram = true, forthInner breaks on PGM_STOPPED check. */
  uint8_t savedRunStop = programRunStop;
  programRunStop = PGM_STOPPED;
  forthPushInt32(999);  /* Sentinel: if word doesn't execute, X stays 999 */
  lastErrorCode = ERROR_NONE;
  fnForthCall(idx);
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: fnForthCall raised error %d\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(15)) {
    printf("    FAIL: X should be 15 after interactive fnForthCall(FIFTEEN), got %s (hardcoded true bug?)\n",
    getRegisterDataType(REGISTER_X) == dtLongInteger ? "wrong longint" : "non-longint");
    return 1;
  }
  printf("    PASS: fnForthCall interactive (PGM_STOPPED) -> X=15, word executed fully\n");
  return 0;
}


/* test_lblq_forth_name_not_local_label
 * §4.2 LBL? UB fix: ordinary LBL? must ask for INVALID_VARIABLE, so a
 * Forth-only colon name is never mistaken for a same-numbered LOCAL label.
 * Program is LBL 00 + LBL?"FW", with FW a colon word at dictionary index 0 —
 * local label 00 and colon index 0 are deliberately the same numeric value, so
 * passing the resolved index instead of INVALID_VARIABLE finds LBL 00.
 *
 * PRECONDITION, and the whole reason the first attempt could not fail:
 * fnCheckLabel (lblGtoXeq.c:1005-1008) opens with
 *   if(dynamicMenuItem >= 0) { label = findNamedLabel(dynmenuGetLabel(...)); }
 * i.e. it DISCARDS its label argument whenever a dynamic menu item is active.
 * dynamicMenuItem is zero-initialized (c47.c:259) and 0 is >= 0, so under the
 * harness the argument under test was never read and NO fixture work could make
 * the mutation observable. Production never executes a step in that state:
 * fnRunProgram sets dynamicMenuItem = -1 immediately before runProgram
 * (lblGtoXeq.c:301), and the fnGoto/XEQ paths do the same (:178, :193).
 * executeOneStep sits below that layer, so this test must establish the
 * precondition production establishes. That is reproducing a documented
 * production invariant, not priming the state under test.
 *
 * Escaping mutation: in _executeOp's ITM_LBLQ arm (lblGtoXeq.c:381), change
 * reallyRunFunction(op, (uint16_t)INVALID_VARIABLE) to
 * reallyRunFunction(op, resolvedParam) — LBL 00 is then found and TI_TRUE.
 * Verified RED with TI=13 (TI_TRUE); correct code gives TI=12 (TI_FALSE).
 */
static int test_lblq_forth_name_not_local_label(void)
{
  int fail = 0;

  forthDictClear();

  /* writeTestProgram calls scanLabelsAndPrograms(), so LBL 00 is registered by
   * the real scanner. Do NOT hand-build labelList: sizing it separately from
   * numberOfLabels desynchronizes the allocation from the cleanup path and
   * corrupts the free list. */
  uint8_t prog[] = {
    0x01, 0x00,                         /* LBL 00 */
    0x85, 0xDF, 0xFD, 2, 'F', 'W'       /* LBL? "FW" */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    forthDictClear();
    return 1;
  }

  /* Colon word "FW" at dictionary index 0 (body = EXIT). */
  uint16_t w = begin_word("FW", 2);
  if (w == FORTH_NULL) {
    printf("    FAIL: could not define colon word FW\n");
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }
  end_word(w);

  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedTI          = temporaryInformation;
  uint8_t savedRunStop     = programRunStop;

  dynamicMenuItem      = -1;            /* as fnRunProgram does, lblGtoXeq.c:301 */
  temporaryInformation = TI_TRUE;       /* must be flipped to TI_FALSE by the arm */
  lastErrorCode        = ERROR_NONE;
  programRunStop       = PGM_RUNNING;

  executeOneStep(beginOfProgramMemory + 2);   /* the LBL?"FW" step */

  programRunStop  = savedRunStop;
  dynamicMenuItem = savedDynMenuItem;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_NONE %d)\n",
           lastErrorCode, ERROR_NONE);
    fail = 1;
  }
  if (temporaryInformation != TI_FALSE) {
    printf("    FAIL: temporaryInformation = %d (expected TI_FALSE %d) — "
           "LBL? treated the Forth name as local label 00\n",
           temporaryInformation, TI_FALSE);
    fail = 1;
  }

  temporaryInformation = savedTI;
  forthDictClear();
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: LBL? with Forth name sets TI_FALSE, not TI_TRUE\n");
  }
  return fail;
}


/* ---- Lifecycle tests: init/reset safety (fix #2 + #11) ----
 * Mutation #2: remove fdict.base guard in forthFindColon -> NULL deref.
 * Mutation #11: remove forthDictInit() from reset -> stale pointers. ---- */

/* Test: pre-init guard — forthFindColon must not deref NULL fdict.base */
static int test_lifecycle_pre_init(void)
{
  /* Simulate hardware: no forthDictInit() yet; fdict is zero-initialized.
   * Set latest=0 (non-FORTH_NULL) so the loop would enter without the guard,
   * forcing a NULL deref of fdict.base.  This makes the test a proper mutation
   * check: removing the guard causes a crash or bogus behaviour. */
  fdict.base = NULL;
  fdict.sizeBlocks = 0;
  fdict.here = 0;
  fdict.latest = 0;      /* non-FORTH_NULL — exercises the guard */
  fdict.count = 0;

  /* Attempt lookup of nonexistent name — should return false, not crash. */
  uint16_t idx;
  bool found = forthFindColon("DOESNOTEXIST", &idx);
  if (found) {
    printf("    FAIL: forthFindColon returned true on uninitialized dict\n");
    return 1;
  }
  if (fdict.base != NULL) {
    printf("    FAIL: fdict.base changed from NULL (should remain uninitialized)\n");
    return 1;
  }
  printf("    PASS: pre-init guard — forthFindColon returns false, no NULL deref\n");
  return 0;
}

/* Test: reset lifecycle — dict cleared after forthDictInit, redefinable afterward */
static int test_lifecycle_reset(void)
{
  /* Stage a word: SQ (DUP * EXIT).
   * Clear (not Init) so a live dict left by an earlier test is freed,
   * not leaked (DESIGN.md §6.2 P-4). */
  forthDictClear();
  uint16_t w = begin_word("SQ", 2);
  if (w == FORTH_NULL) {
    printf("    FAIL: cannot allocate SQ\n");
    return 1;
  }
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(PRIM_TOKEN(P_MUL));
  end_word(w);

  uint16_t idx;
  bool foundPre = forthFindColon("SQ", &idx);
  if (!foundPre) {
    printf("    FAIL: SQ not found after definition\n");
    return 1;
  }

  /* Simulate RESET: forthDictInit() clears the dict (as doFnReset does).
   * In the real reset path the whole arena is rebuilt around this call, so
   * dropping fdict.base without freeC47Blocks is correct there (DESIGN.md
   * §6.2 P-4). Here the arena is NOT rebuilt, so the dropped region would
   * leak (allocation record + blocks) — capture it and release it below,
   * after the semantic assertions, as test-side cleanup. */
  uint8_t *orphanBase = fdict.base;
  uint16_t orphanBlocks = fdict.sizeBlocks;
  forthDictInit();

  /* Release the region the simulated reset orphaned (see above). The
   * assertions below consult only the zeroed control block, never the old
   * region, so freeing it here does not weaken what is being tested. */
  if (orphanBase) {
    freeC47Blocks(orphanBase, orphanBlocks);
  }

  /* After reset, SQ is gone (dict zeroed). */
  bool foundPost = forthFindColon("SQ", &idx);
  if (foundPost) {
    printf("    FAIL: SQ still found after reset (stale pointers)\n");
    return 1;
  }

  /* But we can define again without corruption. */
  w = begin_word("SQ", 2);
  if (w == FORTH_NULL) {
    printf("    FAIL: cannot redefine SQ after reset\n");
    return 1;
  }
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(PRIM_TOKEN(P_MUL));
  end_word(w);

  foundPost = forthFindColon("SQ", &idx);
  if (!foundPost) {
    printf("    FAIL: SQ not found after redefinition\n");
    return 1;
  }
  printf("    PASS: reset lifecycle — dict cleared, redefinable after reset\n");
  return 0;
}

/* Test: production doFnReset actually clears the dictionary.
 * Uses fork() so the destructive reset cannot corrupt the parent
 * test process. The child defines SQ, verifies it exists, calls
 * doFnReset, then checks that SQ is gone and fdict is zeroed. */
static int test_lifecycle_real_reset_hook(void)
{
  pid_t pid = fork();
  if (pid < 0) {
    printf("    FAIL: fork failed (%s)\n", strerror(errno));
    return 1;
  }
  if (pid == 0) {
    /* Child process.
     * alarm() is load-bearing, not belt-and-braces: the mutation this test
     * exists to catch (deleting forthDictInit() from doFnReset) leaves fdict
     * stale after doFnReset's memset(ram,0,...). fdict.base stays non-NULL so
     * forthFindColon's !base guard passes, every link then reads 0, and its
     * walk `while (off != FORTH_NULL) off = hdr->link;` spins on base+0
     * forever (forth_dict.c). Without this alarm the child never exits, the
     * parent blocks in waitpid, and the whole suite hangs instead of going RED
     * — which is exactly what happened. Turn the hang into a signal so the
     * parent can report it. */
    alarm(10);
    forthDictClear();
    uint16_t w = begin_word("SQ", 2);
    if (w == FORTH_NULL) {
      _exit(10);
    }
    forthDictEmit(PRIM_TOKEN(P_DUP));
    forthDictEmit(PRIM_TOKEN(P_MUL));
    end_word(w);

    uint16_t idx;
    bool foundPre = forthFindColon("SQ", &idx);
    if (!foundPre) {
      _exit(20);
    }

    /* Poison the header bits that the field setters do not touch.  Reset must
     * initialize them before saveCalc serializes the whole descriptor. */
    for (uint16_t i = 0; i < NUMBER_OF_GLOBAL_REGISTERS; i++) {
      globalRegister[i].descriptor |= 0xFE000000u;
    }

    doFnReset(CONFIRMED, doNotLoadAutoSav);

    bool foundPost = forthFindColon("SQ", &idx);
    if (foundPost) {
      _exit(30);
    }
    if (fdict.base != NULL) {
      _exit(40);
    }
    if (fdict.here != 0) {
      _exit(50);
    }
    if (fdict.count != 0) {
      _exit(60);
    }
    if (fdict.latest != FORTH_NULL) {
      _exit(70);
    }
    for (uint16_t i = 0; i < NUMBER_OF_GLOBAL_REGISTERS; i++) {
      if ((globalRegister[i].descriptor & 0xFE000000u) != 0) {
        _exit(80);
      }
    }
    _exit(0);
  }

  /* Parent process */
  int status;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    printf("    PASS: real reset hook clears dictionary and zeroes fdict\n");
    return 0;
  }
  if (WIFSIGNALED(status)) {
    printf("    FAIL: real reset hook child killed by signal %d%s\n",
           WTERMSIG(status),
           WTERMSIG(status) == SIGALRM
             ? " (timeout — reset left fdict stale; forthFindColon's unbounded"
               " link walk spun on a zeroed chain)"
             : "");
    return 1;
  }
  printf("    FAIL: real reset hook child exited with code %d\n",
         WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  return 1;
}


/* ---- Error-handling tests: fix #7 (dict-space / name errors) ----
 * Mutation #7a: remove error display from startDefinition -> silent failure,
 *   ASLIFT set as success.
 * Mutation #7b: remove error display from forthDictEnsure -> silent failure. ---- */

/* Test: over-long name triggers error, not silent ASLIFT-as-success */
static int test_dict_name_too_long(void)
{
  char longName[64];
  memset(longName, 'A', 32);
  longName[32] = 0;

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH 1 ;");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: over-long name (32 chars) did not produce an error (silent ASLIFT-as-success)\n");
    return 1;
  }
  uint16_t idx;
  if (forthFindColon(longName, &idx)) {
    printf("    FAIL: over-long name was defined despite error\n");
    return 1;
  }
  printf("    PASS: over-long name rejected with error %d, no silent success\n", lastErrorCode);
  return 0;
}

/* Test: dict count cap triggers ERROR_RAM_FULL */
static int test_dict_space_full(void)
{
  forthDictInit();
  fdict.count = 0x6F00;

  lastErrorCode = ERROR_NONE;
  bool ok = startDefinition("FULLTEST");
  if (ok) {
    abortDefinition();
    printf("    FAIL: startDefinition succeeded at count cap\n");
    return 1;
  }
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: count cap gave error %d, expected ERROR_RAM_FULL (%d)\n",
           lastErrorCode, ERROR_RAM_FULL);
    return 1;
  }
  printf("    PASS: count cap rejected with ERROR_RAM_FULL\n");
  return 0;
}

/* test_dict_first_ensure_capacity
 * R1-1: forthDictEnsure's null-base branch allocated the configured initial
 * block count unconditionally and returned true even when that allocation did
 * not cover the requested bytes — a first request larger than the initial
 * region was reported safe while the caller could write past it.
 * Escaping mutation: drop the minBlocks-to-initBlocks raise (revert to the
 * unconditional `uint16_t initBlocks = FORTH_INITIAL_BLOCKS` / test-override
 * allocation) — TO_BYTES(sizeBlocks) < requested and the capacity assertion
 * fails. */
static int test_dict_first_ensure_capacity(void)
{
  int fail = 0;

  forthDictClear();
  forthDictSetTestInitialBlocks(1);   /* deliberately small: 1 block = BYTES_PER_BLOCK bytes */

  const uint16_t requested = BYTES_PER_BLOCK * 5;   /* well beyond the 1-block initial region */
  bool ok = forthDictEnsure(requested);

  if (!ok) {
    printf("    FAIL: forthDictEnsure(%u) returned false\n", requested);
    fail = 1;
  }
  else if (!fdict.base) {
    printf("    FAIL: forthDictEnsure succeeded but fdict.base is NULL\n");
    fail = 1;
  }
  else {
    uint32_t capacityBytes = TO_BYTES(fdict.sizeBlocks);
    uint32_t needed = (uint32_t)fdict.here + requested;
    if (capacityBytes < needed) {
      printf("    FAIL: capacity %u bytes < needed %u bytes (here=%u sizeBlocks=%u)\n",
             capacityBytes, needed, fdict.here, fdict.sizeBlocks);
      fail = 1;
    }
    else {
      /* The last requested byte must be writable without passing the
       * allocation — not just arithmetically claimed as covered. */
      fdict.base[fdict.here + requested - 1] = 0xA5;
      if (fdict.base[fdict.here + requested - 1] != 0xA5) {
        printf("    FAIL: write to the last requested byte did not persist\n");
        fail = 1;
      }
    }
  }

  forthDictClear();
  forthDictSetTestInitialBlocks(4);   /* restore the suite's baseline override */

  if (!fail) {
    printf("    PASS: first ensure(%u) over a 1-block initial region grew capacity to cover it\n",
           requested);
  }
  return fail;
}

/* test_dict_capacity_arithmetic
 * R4-2: two independent violations of the same capacity contract.
 * Subcase 1 restates R1-1's fix at the forthDictEnsure level (kept light —
 * test_dict_first_ensure_capacity above is the thorough version, proving the
 * fix by actually writing the last requested byte; both target the same
 * null-base branch and would be redundant in full).
 * Subcase 2 is the independent forthDictAllocate defect: hdrSize/alignedHdr/
 * total were uint16_t and wrapped silently. Probed: forthDictAllocate(31,
 * 0xFFF0) wrapped the total and returned offset 0 with no error.
 * Escaping mutation: revert forthDictAllocate's total to uint16_t (drop the
 * widened-arithmetic overflow check) — subcase 2 gets FORTH_NULL/ERROR_NONE
 * become offset-0/no-error instead. */
static int test_dict_capacity_arithmetic(void)
{
  int fail = 0;

  /* Subcase 1: forthDictEnsure over a small initial region. */
  forthDictClear();
  forthDictSetTestInitialBlocks(1);
  uint16_t requested1 = BYTES_PER_BLOCK + 2;
  bool ok = forthDictEnsure(requested1);
  if (!ok || TO_BYTES(fdict.sizeBlocks) < requested1) {
    printf("    FAIL: subcase 1 — ensure(%u) ok=%d sizeBlocks*BPB=%u\n",
           requested1, ok, (unsigned)TO_BYTES(fdict.sizeBlocks));
    fail = 1;
  }

  /* Subcase 2: forthDictAllocate with a bodyBytes large enough to overflow
   * uint16_t arithmetic (31 + aligned-header + 0xFFF0 wraps in 16 bits). */
  forthDictClear();
  forthDictSetTestInitialBlocks(4);
  lastErrorCode = ERROR_NONE;
  uint16_t off = forthDictAllocate(31, 0xFFF0u);
  if (off != FORTH_NULL) {
    printf("    FAIL: subcase 2 — forthDictAllocate(31, 0xFFF0) returned offset %u, expected FORTH_NULL\n",
           off);
    fail = 1;
  }
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: subcase 2 — lastErrorCode = %d, expected ERROR_RAM_FULL (%d)\n",
           lastErrorCode, ERROR_RAM_FULL);
    fail = 1;
  }

  forthDictClear();
  forthDictSetTestInitialBlocks(4);

  if (!fail) {
    printf("    PASS: dict capacity arithmetic is checked and truthful (ensure + allocate)\n");
  }
  return fail;
}

/* test_number_then_no_label_fallthrough
 * C-8 classify-gate: a classified number that fails to emit must NOT fall
 * through to label lookup.  Without the gate, processNumber returns false
 * on emit failure, the token falls through to findNamedLabel, then to
 * ERROR_FUNCTION_NOT_FOUND (overwriting the original ERROR_RAM_FULL).
 * Mutation: revert to single-bool fall-through -> lastErrorCode ends as
 * ERROR_FUNCTION_NOT_FOUND and the test FAILS.
 */
static int test_number_then_no_label_fallthrough(void)
{
  forthDictClear();
  forthDictSetTestInitialBlocks(4);
  forthDictInit();

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": PAD ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: could not define PAD (error %d)\n", lastErrorCode);
    return 1;
  }

  fdict.here = 0xFFF6;

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": W 42 ;");

  if (!fdict.base) {
    printf("    FAIL: fdict.base is NULL (realloc failed)\n");
    return 1;
  }

  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: lastErrorCode = %d (expected %d = ERROR_RAM_FULL)\n",
           lastErrorCode, ERROR_RAM_FULL);
    return 1;
  }

  if (strstr(errorMessage, "42")) {
    printf("    FAIL: errorMessage contains '42' (label lookup occurred: '%s')\n",
           errorMessage);
    return 1;
  }

  {
    uint16_t idx;
    if (forthFindColon("W", &idx)) {
      printf("    FAIL: word W found (definition not aborted)\n");
      return 1;
    }
  }

  printf("    PASS: classified number emit failure does not fall through to label lookup\n");
  return 0;
}


/* ---- Fix #8: prefix-match bug (SQ vs SQUARE) ----
 * Mutation #8: remove queryLen == hdr->nameLen check -> SQUARE matches SQ. ---- */

/* Test: SQ defined, SQUARE lookup returns false */
static int test_prefix_no_match(void)
{
  forthDictClear();
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SQ DUP * ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: could not define SQ (error %d)\n", lastErrorCode);
    return 1;
  }

  uint16_t idx;
  bool found = forthFindColon("SQUARE", &idx);
  if (found) {
    printf("    FAIL: SQUARE matched SQ (prefix match bug)\n");
    return 1;
  }
  found = forthFindColon("SQ", &idx);
  if (!found) {
    printf("    FAIL: SQ not found after definition\n");
    return 1;
  }
  printf("    PASS: SQUARE does not match SQ (exact match enforced)\n");
  return 0;
}


/* ---- Fix #9: number grammar (signed exponents, reject mantissa-less e) ----
 * Mutation #9a: revert mantissaDigits tracking -> e5/.e5/3e classify as REAL -> NaN.
 * Mutation #9b: reject sign after e/E -> 1e-5 errors as undefined. ---- */

/* Test: 1e-5 is a valid real number */
static int test_number_1e_minus_5(void)
{
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("1e-5");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: 1e-5 rejected as undefined (error %d) — sign after e/E not accepted\n", lastErrorCode);
    return 1;
  }
  if (getRegisterDataType(REGISTER_X) != dtReal34) {
    printf("    FAIL: 1e-5 pushed as non-real type %u\n", getRegisterDataType(REGISTER_X));
    return 1;
  }
  printf("    PASS: 1e-5 accepted as real (dtReal34)\n");
  return 0;
}

/* Test: e5 is NOT a valid number (no mantissa) */
static int test_number_bad_e5(void)
{
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  uint8_t xTypeBefore = getRegisterDataType(REGISTER_X);
  forthOuterInterpret("e5");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: e5 accepted (no mantissa digits) — grammar bug\n");
    return 1;
  }
  if (getRegisterDataType(REGISTER_X) != xTypeBefore) {
    printf("    FAIL: e5 modified X register (possible NaN from misclassification)\n");
    return 1;
  }
  printf("    PASS: e5 rejected (error %d, not a number)\n", lastErrorCode);
  return 0;
}

/* Test: .e5 is NOT a valid number (no mantissa digits before e) */
static int test_number_bad_dot_e5(void)
{
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(".e5");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: .e5 accepted (no mantissa digits) — grammar bug\n");
    return 1;
  }
  printf("    PASS: .e5 rejected (error %d, not a number)\n", lastErrorCode);
  return 0;
}

/* Test: 3e is NOT a valid number (no exponent digits) */
static int test_number_bad_3e(void)
{
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("3e");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: 3e accepted (no exponent digits) — grammar bug\n");
    return 1;
  }
  printf("    PASS: 3e rejected (error %d, not a number)\n", lastErrorCode);
  return 0;
}

/* test_number_bad_exponent_sign_position
 * R4-1: the grammar is [eE][+-]?digit+ — a sign is legal only as the FIRST
 * byte immediately after e/E, and only once. "1e2-3" has its '-' after the
 * exponent digit '2', not immediately after 'e', so it must be rejected as
 * an undefined word — not silently accepted as a number, and not silently
 * truncating the line before the tail "7".
 * Escaping mutation: restore the broad clause
 * `(s[i] == '+' || s[i] == '-') && hasExp` — lastErrorCode stays ERROR_NONE. */
static int test_number_bad_exponent_sign_position(void)
{
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  errorMessage[0] = 0;
  forthPushInt32(444);

  forthOuterInterpret("1e2-3 7");

  int fail = 0;
  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d, expected ERROR_FUNCTION_NOT_FOUND (%d)\n",
           lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
    fail = 1;
  }
  if (strcmp(errorMessage, "1e2-3") != 0) {
    printf("    FAIL: errorMessage = '%s', expected '1e2-3'\n", errorMessage);
    fail = 1;
  }
  if (!x_is_longint(444)) {
    printf("    FAIL: X changed — malformed token or tail '7' modified X\n");
    fail = 1;
  }
  if (!fail) {
    printf("    PASS: '1e2-3' rejected as undefined word, tail '7' not processed, X unchanged\n");
  }
  return fail;
}

/* Test: lone . is NOT a valid number (Axis 4 spec edge case) */
static int test_number_bad_lone_dot(void)
{
  forthDictInit();
  lastErrorCode = ERROR_NONE;
  uint8_t xTypeBefore = getRegisterDataType(REGISTER_X);
  forthOuterInterpret(".");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: lone dot accepted — grammar bug\n");
    return 1;
  }
  if (getRegisterDataType(REGISTER_X) != xTypeBefore) {
    printf("    FAIL: lone dot modified X register\n");
    return 1;
  }
  printf("    PASS: lone dot rejected (error %d, not a number)\n", lastErrorCode);
  return 0;
}


/* ---- Fix #10: UNDO rows US_ENABLED ----
 * Mutation #10: revert US_ENABLED to US_UNCHANGED -> no undo snapshot. ---- */

/* Test: items.c FORTH/FCALL rows carry US_ENABLED (§0.2)
 *
 * No runtime mutation check: R2-T3 asked for one ("temporarily clear
 * US_ENABLED from one runtime table entry ... require its exact FAIL label,
 * then restore"), but indexOfItems[] is `const item_t[]` and, even in the PC
 * build where TO_QSPI is empty, lands in a read-only-mapped section.
 * Confirmed empirically: casting away const and writing indexOfItems[
 * ITM_FORTH].status SIGSEGVs (exit 139), reverted. The FAIL branch below is
 * real and correctly labeled — read the two comparisons — but it can only be
 * exercised by editing items.c and rebuilding, not by a runtime probe. This
 * test's value is as a regression tripwire on the real data table if items.c
 * itself changes. */
static int test_undo_rows_us_enabled(void)
{
  uint16_t forthUS = indexOfItems[ITM_FORTH].status & US_STATUS;
  uint16_t fcallUS = indexOfItems[ITM_FCALL].status & US_STATUS;

  if (forthUS != US_ENABLED) {
    printf("    FAIL: ITM_FORTH US_STATUS is 0x%x, expected US_ENABLED (0x%x)\n", forthUS, US_ENABLED);
    return 1;
  }
  if (fcallUS != US_ENABLED) {
    printf("    FAIL: ITM_FCALL US_STATUS is 0x%x, expected US_ENABLED (0x%x)\n", fcallUS, US_ENABLED);
    return 1;
  }
  printf("    PASS: FORTH and FCALL rows carry US_ENABLED\n");
  return 0;
}

/* COMMIT 2: P-1 — ITM_FORTH step is PTP_REM, not PTP_NONE.
 * Escaping mutation: reverting items.c PTP_REM back to PTP_NONE. */
static int test_forth_step_ptp_rem(void)
{
  if ((indexOfItems[ITM_FORTH].status & PTP_STATUS) != PTP_REM) {
    printf("    FAIL: ITM_FORTH PTP is %x, expected PTP_REM (%x)\n",
    indexOfItems[ITM_FORTH].status & PTP_STATUS, PTP_REM);
    return 1;
  }
  printf("    PASS: ITM_FORTH PTP is PTP_REM\n");
  return 0;
}

/* COMMIT 2: P-1 — ITM_FORTH step sizing via findKey2ndParam (PTP_REM path).
 * Escaping mutation: reverting items.c PTP_REM back to PTP_NONE (sizes as 2 bytes).
 *
 * Rebase to b8f79e486: upstream's findKey2ndParam (nextStep.c) gained
 * programBytesAvailable(address, count) bounds-checking — it now rejects any
 * computed "next step" pointer that falls outside [beginOfProgramMemory,
 * firstFreeProgramByte]. The original bare stack-local marker[]/source[]
 * arrays here are outside that range by construction (they were never part
 * of any real program), so upstream's new guard correctly returns NULL for
 * them — the same "stack-local test fixture" defect class as this session's
 * own R2-T2 fix (test_exec_step_halts_on_error), independently found and
 * fixed upstream. Rebuilt via writeTestProgram so both steps live in real,
 * registered program memory. */
static int test_forth_step_sizing(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                    /* marker: 4 bytes */
    0x8B, 0x1A, 0xFD, 0x05, '3', ' ', 'S', 'Q', ' ',            /* source: 9 bytes */
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *marker = beginOfProgramMemory;
  uint8_t *source = beginOfProgramMemory + 4;
  int fail = 0;

  uint8_t *next = findKey2ndParam(marker);
  if (next != marker + 4) {
    printf("    FAIL: marker step: next=%p, expected %p (buf+4)\n",
    (void *)next, (void *)(marker + 4));
    fail = 1;
  }

  next = findKey2ndParam(source);
  if (next != source + 9) {
    printf("    FAIL: source step: next=%p, expected %p (buf+9)\n",
    (void *)next, (void *)(source + 9));
    fail = 1;
  }

  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: ITM_FORTH step sizing correct (marker=4, source=9)\n");
  }
  return fail;
}

/* ---- COMMIT 3: Program-step, run-generation, name-by-index tests ---- */
/* (build_payload helper retired with Architecture 2: forthProgramStep's
 * contract requires payloads inside real programs — see the migrated
 * tests below.) */

/* test_program_step_define_and_use
 * MIGRATED to Architecture 2 (P2 ruling, 2026-07-13): forthProgramStep's
 * contract now requires the payload to reside inside a real program (the
 * first touch pre-scans the owning program, compiling definitions;
 * SKIP_DEFS executes only tails). Stack-buffer payloads encode the retired
 * execute-in-place semantics.
 * Mutations: a no-op forthProgramStep handler (§8.9 acceptance 1), or a
 * no-op pre-scan — either way SQ never compiles and step 2 errors. */
static int test_program_step_define_and_use(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 12, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 4, '3', ' ', 'S', 'Q'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  forthProgramStep(beginOfProgramMemory + 3);        /* define step payload */
  if (lastErrorCode == ERROR_NONE) {
    /* Canary (R2-T4 item 2): a dropped handler must leave -123456 on X, not
     * a stale 9 left over from an earlier test — X==9 alone cannot tell
     * "this step ran" from "a previous test already left X at 9". */
    forthPushInt32(-123456);
    forthProgramStep(beginOfProgramMemory + 16 + 3); /* "3 SQ" payload */
  }
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: program steps raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (getRegisterDataType(REGISTER_X) != dtLongInteger) {
    printf("    FAIL: X is not dtLongInteger (type %u)\n", getRegisterDataType(REGISTER_X));
    fail = 1;
  }
  else if (!x_is_longint(9)) {
    printf("    FAIL: X != 9\n");
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: forthProgramStep (real program) : SQ DUP * ;  3 SQ -> X==9\n");
  return fail;
}

/* test_program_step_gen_reset
 * MIGRATED to Architecture 2 (P2 ruling, 2026-07-13). The OLD expectation
 * (FUNCTION_NOT_FOUND after a generation bump) is exactly what the
 * pre-scan eliminates: the bump clears the dictionary AND re-arms the
 * scan, so a later step regains program-defined words via re-scan. The
 * new observable for the reset is an INTERACTIVE word: it must NOT
 * survive the generation change.
 * Mutation: deleting the forthRunGenCheckReset call in forthProgramStep
 * -> GENX survives the bump (dict never cleared). */
static int test_program_step_gen_reset(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 12, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 4, '3', ' ', 'S', 'Q'
  };
  uint16_t idx;
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Baseline run: pre-scan compiles SQ, tail executes */
  forthRunGenBump();
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  forthProgramStep(beginOfProgramMemory + 3);
  if (lastErrorCode == ERROR_NONE) {
    forthProgramStep(beginOfProgramMemory + 16 + 3);
  }
  programRunStop = savedRS;
  if (lastErrorCode != ERROR_NONE || !x_is_longint(9)) {
    printf("    FAIL: baseline run error %d\n", lastErrorCode);
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  /* Interactive word: must not survive the next generation */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": GENX 1 ;");
  if (lastErrorCode != ERROR_NONE || !forthFindColon("GENX", &idx)) {
    printf("    FAIL: interactive GENX setup failed\n");
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  forthRunGenBump();
  savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  forthProgramStep(beginOfProgramMemory + 16 + 3);   /* "3 SQ" in new generation */
  programRunStop = savedRS;

  if (lastErrorCode != ERROR_NONE || !x_is_longint(9)) {
    printf("    FAIL: post-bump run error %d (re-scan should recompile SQ)\n", lastErrorCode);
    fail = 1;
  }
  if (forthFindColon("GENX", &idx)) {
    printf("    FAIL: interactive word GENX survived the generation bump (checkReset missing)\n");
    fail = 1;
  }
  if (fdict.count != 1) {
    printf("    FAIL: fdict.count = %u after re-scan (expected 1: just SQ)\n", fdict.count);
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: gen bump clears dict, re-scan recompiles SQ -> X==9, GENX gone\n");
  return fail;
}

/* test_pending_reset_lifetime
 * F1-1: pending-reset flag is truth; active frames defer invalidation.
 * Subcase 1: 16-bit wrap cannot cancel a reset (counter equality is NOT truth).
 * Subcase 2: nested launch (active frame) does not request a generation.
 * Subcase 3: pending reset waits for a safe entry (no active frame). */
static int test_pending_reset_lifetime(void)
{
  /* One-step program: ITM_FORTH opcode + length-prefixed source "0" */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 1, '0'
  };
  uint16_t idx;
  int fail = 0;
  uint8_t savedRS = programRunStop;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }
  const uint8_t *payload = beginOfProgramMemory + 3;

  /* Subcase 1: 16-bit wrap cannot cancel a reset */
  {
    /* Consume any prior event with a baseline call */
    lastErrorCode = ERROR_NONE;
    programRunStop = PGM_RUNNING;
    forthProgramStep(payload);
    programRunStop = savedRS;

    /* Define interactive word */
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": WRAP 1 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("WRAP", &idx)) {
      printf("  [1] FAIL: WRAP setup failed\n");
      fail = 1;
      forthSetTestInnerDepth(0);
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    /* 65536 bumps: counter wraps to same value; pending-reset survives */
    forthSetTestInnerDepth(0);
    for (uint32_t i = 0; i < 65536u; i++) {
      forthRunGenBump();
    }

    /* Enter real Forth program step */
    lastErrorCode = ERROR_NONE;
    programRunStop = PGM_RUNNING;
    forthProgramStep(payload);
    programRunStop = savedRS;

    if (lastErrorCode != ERROR_NONE) {
      printf("  [1] FAIL: program step error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (forthFindColon("WRAP", &idx)) {
      printf("  [1] FAIL: WRAP survived 65536-bump wrap (counter equality is truth — WRONG)\n");
      fail = 1;
    }
    else {
      printf("  [1] PASS: 16-bit wrap cannot cancel a reset; WRAP cleared\n");
    }
  }

  /* Subcase 2: nested launch does not request a generation */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": KEEP 2 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("KEEP", &idx)) {
      printf("  [2] FAIL: KEEP setup failed\n");
      fail = 1;
      forthSetTestInnerDepth(0);
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    /* Bump while active frame */
    forthSetTestInnerDepth(1);
    forthRunGenBump();
    forthSetTestInnerDepth(0);

    /* Enter real program step */
    lastErrorCode = ERROR_NONE;
    programRunStop = PGM_RUNNING;
    forthProgramStep(payload);
    programRunStop = savedRS;

    if (lastErrorCode != ERROR_NONE) {
      printf("  [2] FAIL: program step error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!forthFindColon("KEEP", &idx)) {
      printf("  [2] FAIL: KEEP cleared by active-frame bump\n");
      fail = 1;
    }
    else {
      printf("  [2] PASS: nested launch does not request a generation; KEEP retained\n");
    }
  }

  /* Subcase 3: pending reset waits for a safe entry */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": HOLD 3 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("HOLD", &idx)) {
      printf("  [3] FAIL: HOLD setup failed\n");
      fail = 1;
      forthSetTestInnerDepth(0);
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    /* Request reset at depth 0 */
    forthSetTestInnerDepth(0);
    forthRunGenBump();

    /* Enter with active frame: reset deferred, HOLD survives */
    forthSetTestInnerDepth(1);
    lastErrorCode = ERROR_NONE;
    programRunStop = PGM_RUNNING;
    forthProgramStep(payload);
    programRunStop = savedRS;

    if (lastErrorCode != ERROR_NONE) {
      printf("  [3] FAIL: guarded program step error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!forthFindColon("HOLD", &idx)) {
      printf("  [3] FAIL: HOLD cleared during guarded entry\n");
      fail = 1;
    }
    else {
      printf("  [3a] PASS: pending reset deferred by active frame; HOLD retained\n");
    }

    /* Enter again without active frame: reset consumed, HOLD cleared */
    forthSetTestInnerDepth(0);
    lastErrorCode = ERROR_NONE;
    programRunStop = PGM_RUNNING;
    forthProgramStep(payload);
    programRunStop = savedRS;

    if (lastErrorCode != ERROR_NONE) {
      printf("  [3b] FAIL: safe-entry program step error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (forthFindColon("HOLD", &idx)) {
      printf("  [3b] FAIL: HOLD survived safe entry (reset not consumed)\n");
      fail = 1;
    }
    else {
      printf("  [3b] PASS: safe entry consumed pending reset; HOLD cleared\n");
    }
  }

  forthSetTestInnerDepth(0);
  forthDictClear();
  cleanupTestProgram();
  return fail;
}

/* test_run_entry_lifetime_signaling
 * F1-2: Every top-level engine entry signals a fresh Forth lifetime.
 * Subcase 1: Interactive XEQ start is a fresh lifetime.
 * Subcase 2: Run-mode SST is a fresh lifetime (R4 ruling 3).
 * Subcase 3: A nested engine entry preserves the active lifetime. */
static int test_run_entry_lifetime_signaling(void)
{
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '2', 'A',            /* step 1: LBL 'F2A' */
    0x8B, 0x1A, 0xFD, 0x01, '1',                /* step 2: ITM_FORTH "1" */
    0x04                                        /* step 3: RTN */
  };
  uint16_t idx;
  int fail = 0;
  uint8_t savedRS = programRunStop;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  calcRegister_t lbl = findNamedLabel("F2A", GLOBAL_LABELS);
  if (lbl == INVALID_VARIABLE) {
    printf("    FAIL: findNamedLabel(\"F2A\") returned INVALID_VARIABLE\n");
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  /* Subcase 1: Interactive XEQ start is a fresh lifetime */
  {
    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: baseline XEQ error %d\n", lastErrorCode);
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": F2X 7 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("F2X", &idx)) {
      printf("    [1] FAIL: F2X setup failed\n");
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: second XEQ error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(1)) {
      printf("    [1] FAIL: X != 1 after second XEQ\n");
      fail = 1;
    }
    else if (forthFindColon("F2X", &idx)) {
      printf("    [1] FAIL: F2X survived second XEQ start (not a fresh lifetime)\n");
      fail = 1;
    }
    else {
      printf("    [1] PASS: interactive XEQ start is a fresh lifetime\n");
    }
  }

  /* Subcase 2: Run-mode SST is a fresh lifetime */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": F2S 8 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("F2S", &idx)) {
      printf("    [2] FAIL: F2S setup failed\n");
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    dynamicMenuItem = -1;
    fnGotoDot(2);

    if (currentStep != beginOfProgramMemory + 6) {
      printf("    [2] FAIL: currentStep != beginOfProgramMemory + 6\n");
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    runProgram(true, INVALID_VARIABLE);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: SST drive error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(1)) {
      printf("    [2] FAIL: X != 1 after SST\n");
      fail = 1;
    }
    else if (forthFindColon("F2S", &idx)) {
      printf("    [2] FAIL: F2S survived SST (not a fresh lifetime)\n");
      fail = 1;
    }
    else {
      printf("    [2] PASS: run-mode SST is a fresh lifetime\n");
    }
  }

  /* Subcase 3: A nested engine entry preserves the active lifetime */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": F2N 9 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("F2N", &idx)) {
      printf("    [3] FAIL: F2N setup failed\n");
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    dynamicMenuItem = -1;
    fnGotoDot(2);

    programRunStop = PGM_RUNNING;
    lastErrorCode = ERROR_NONE;
    runProgram(false, INVALID_VARIABLE);
    programRunStop = savedRS;

    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: nested drive error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(1)) {
      printf("    [3] FAIL: X != 1 after nested drive\n");
      fail = 1;
    }
    else if (!forthFindColon("F2N", &idx)) {
      printf("    [3] FAIL: F2N cleared by nested entry (should be preserved)\n");
      fail = 1;
    }
    else {
      printf("    [3] PASS: nested entry preserves active lifetime\n");
    }
  }

  programRunStop = savedRS;
  forthDictClear();
  cleanupTestProgram();
  return fail;
}

/* test_prescan_forward_reference
 * T2.1: A definition in step 2 can be called from step 1, because the
 * pre-scan compiles all steps before execution.
 * Must fail if the pre-scan is skipped or only covers steps before the
 * current one (execute-in-place raises ERROR_FUNCTION_NOT_FOUND). */
static int test_prescan_forward_reference(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 10, ':', ' ', 'F', 'W', 'D', ' ', '4', '2', ' ', ';',
    0x8B, 0x1A, 0xFD, 3,  'F', 'W', 'D'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  forthProgramStep(beginOfProgramMemory + 14 + 3);
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step raised error %d (expected no error — pre-scan should compile FWD from step 1)\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(42)) {
    printf("    FAIL: X != 42\n");
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: forward reference resolves — pre-scan compiles def before step executes\n");
  return fail;
}

/* test_prescan_no_early_tail
 * T2.2: DEFS_ONLY must NOT execute tail tokens. The "99" trailing the
 * definition in step 1 must NOT be pushed during the pre-scan.
 * Must fail if DEFS_ONLY executes interpret-state tokens (99 pushed during
 * pre-scan too — sentinel lands one deeper). */
static int test_prescan_no_early_tail(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 11, ':', ' ', 'A', '2', ' ', '1', ' ', ';', ' ', '9', '9',
    0x8B, 0x1A, 0xFD, 2,  'A', '2'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthPushInt32(777);
  forthProgramStep(beginOfProgramMemory + 3);
  if (lastErrorCode == ERROR_NONE) {
    forthProgramStep(beginOfProgramMemory + 15 + 3);
  }
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(1)) {
    printf("    FAIL: X != 1 (got %d)\n", x_is_longint(0) ? 0 : -1);
    fail = 1;
  }
  else {
    fnDrop(NOPARAM);
    fnDrop(NOPARAM);
    if (!x_is_longint(777)) {
      printf("    FAIL: sentinel 777 not where expected (tail executed during pre-scan?)\n");
      fail = 1;
    }
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: DEFS_ONLY does not execute tail — 99 not pushed during pre-scan\n");
  return fail;
}

/* test_prescan_no_recompile
 * T2.3: SKIP_DEFS must not recompile definitions. Passing over the defining
 * step during execution must not add a second entry to the dictionary.
 * Must fail if SKIP_DEFS recompiles the definition when execution passes the
 * defining step (count 2 / here grows). */
static int test_prescan_no_recompile(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 8,  ':', ' ', 'B', '3', ' ', '5', ' ', ';',
    0x8B, 0x1A, 0xFD, 2,  'B', '3'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthProgramStep(beginOfProgramMemory + 3);
  uint16_t countAfter1 = fdict.count;
  uint16_t hereAfter1 = fdict.here;

  if (lastErrorCode == ERROR_NONE) {
    forthProgramStep(beginOfProgramMemory + 12 + 3);
  }
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(5)) {
    printf("    FAIL: X != 5\n");
    fail = 1;
  }
  if (fdict.count != countAfter1) {
    printf("    FAIL: fdict.count changed from %u to %u (recompile detected)\n", countAfter1, fdict.count);
    fail = 1;
  }
  if (fdict.here != hereAfter1) {
    printf("    FAIL: fdict.here changed from %u to %u (recompile detected)\n", hereAfter1, fdict.here);
    fail = 1;
  }
  if (countAfter1 != 1) {
    printf("    FAIL: fdict.count after pre-scan = %u (expected 1)\n", countAfter1);
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: SKIP_DEFS does not recompile — count and here unchanged\n");
  return fail;
}

/* test_prescan_owning_scope
 * T2.4: The pre-scan must only walk the owning program, not all programs.
 * A word defined in program 1 must NOT be visible when executing a step in
 * program 2.
 * Must fail if the pre-scan walks all programs instead of only the owning
 * one (D-2c). */
static int test_prescan_owning_scope(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 11, ':', ' ', 'O', 'N', 'L', 'Y', '1', ' ', '8', ' ', ';',
    0x85, 0xB2,
    0x8B, 0x1A, 0xFD, 5,  'O', 'N', 'L', 'Y', '1'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  if (numberOfPrograms < 2) {
    printf("    FAIL (SKIP): numberOfPrograms = %u, expected >= 2 (harness did not split at ITM_END)\n", numberOfPrograms);
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  const uint8_t *prog2Step = beginOfProgramMemory + 15 + 2;

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  forthProgramStep(prog2Step + 3);
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_FUNCTION_NOT_FOUND=%d — pre-scan leaked across programs)\n",
    lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
    fail = 1;
  }
  if (fdict.count != 0) {
    printf("    FAIL: fdict.count = %u (expected 0 — pre-scan should not have compiled program 1's def)\n", fdict.count);
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: pre-scan scoped to owning program — ONLY1 not visible in program 2\n");
  return fail;
}

/* test_owning_program_start_bounds
 * R2-T2 finding, code-defect track: forthOwningProgramStart's scan had no
 * upper bound — ANY pointer at or past the last program's start resolved to
 * that program, including a pointer outside program memory entirely. That is
 * what made test_exec_step_halts_on_error's original stack-local fixture
 * silently pre-scan a real, unrelated program (probe: a stack address
 * resolved to a real progStart instead of NULL).
 * Three subcases, one program:
 *   1. A pointer genuinely inside the program's payload resolves to progStart.
 *   2. currentStep == firstFreeProgramByte (cursor on the .END. sentinel, a
 *      normal PEM position — several capture tests set exactly this) must
 *      still resolve. Regression guard: the first fix attempt rejected this
 *      and broke test_e2_continuation_after_enter and three sibling tests;
 *      caught by the full gate before landing, not by this test alone.
 *   3. A stack-local buffer's address — nowhere near the C47 RAM arena —
 *      must return NULL.
 * Escaping mutation: delete the arena-membership check; subcase 3 finds a
 * real progStart instead of NULL. */
static int test_owning_program_start_bounds(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 7, ':', ' ', 'G', ' ', '1', ' ', ';',
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  int fail = 0;

  uint8_t *inProgram = beginOfProgramMemory + 3;   /* inside the payload */
  uint8_t *owner1 = forthOwningProgramStart(inProgram);
  if (owner1 != beginOfProgramMemory) {
    printf("    FAIL: in-program pointer resolved to %p, expected progStart %p\n",
           (void *)owner1, (void *)beginOfProgramMemory);
    fail = 1;
  }

  uint8_t *owner2 = forthOwningProgramStart(firstFreeProgramByte);
  if (owner2 != beginOfProgramMemory) {
    printf("    FAIL: firstFreeProgramByte (.END. sentinel position) resolved to %p, "
           "expected progStart %p\n", (void *)owner2, (void *)beginOfProgramMemory);
    fail = 1;
  }

  uint8_t stackLocal[4];
  uint8_t *owner3 = forthOwningProgramStart(stackLocal);
  if (owner3 != NULL) {
    printf("    FAIL: stack-local pointer resolved to %p, expected NULL\n", (void *)owner3);
    fail = 1;
  }

  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: in-program and sentinel pointers resolve; a wild stack pointer does not\n");
  }
  return fail;
}

/* test_owning_program_start_max_not_last
 * R4 accepted ruling (E5): forthOwningProgramStart must compute the greatest
 * qualifying programList entry explicitly, not rely on programList being
 * address-ascending. scanLabelsAndPrograms happens to build it that way today
 * (manage.c:102-129 walks program memory sequentially), so the bug is latent,
 * not currently reachable — this proves the FUNCTION's own contract,
 * independent of that unstated builder invariant.
 * Two programs, then programList[0] and [1] are swapped by hand so array
 * order no longer matches address order. A query inside the higher-address
 * program (P2) must still resolve to P2, not fall back to P1 because P1 now
 * appears LATER in the (deliberately reversed) array.
 * Escaping mutation: revert to unconditional overwrite
 * (`if (ip <= ptr) progStart = ip;`, no `ip > progStart` comparison) — with
 * this swap, the query resolves to P1 (wrong) instead of P2. */
static int test_owning_program_start_max_not_last(void)
{
  uint8_t prog[] = {
    0x4C,                          /* P1: ITM_sin (RPN), 1 byte */
    0x85, 0xB2,                    /* ITM_END separator */
    0x4C,                          /* P2: ITM_sin (RPN), 1 byte */
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  int fail = 0;

  if (numberOfPrograms < 2) {
    printf("    FAIL (SKIP): numberOfPrograms = %u, expected >= 2\n", numberOfPrograms);
    cleanupTestProgram();
    return 1;
  }

  uint8_t *p1Start = beginOfProgramMemory;
  uint8_t *p2Start = beginOfProgramMemory + 1 + 2;   /* past P1(1) + ITM_END(2) */

  /* Locate P1's and P2's real programList entries, then swap array slots 0/1
   * so index order no longer matches address order. */
  int16_t idx1 = -1, idx2 = -1;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    if (programList[i].instructionPointer == p1Start) idx1 = (int16_t)i;
    if (programList[i].instructionPointer == p2Start) idx2 = (int16_t)i;
  }
  if (idx1 < 0 || idx2 < 0) {
    printf("    FAIL (SKIP): could not locate both programs in programList\n");
    cleanupTestProgram();
    return 1;
  }
  programList_t tmp = programList[idx1];
  programList[idx1] = programList[idx2];
  programList[idx2] = tmp;

  uint8_t *owner = forthOwningProgramStart(p2Start);

  if (owner != p2Start) {
    printf("    FAIL: query inside P2 resolved to %p, expected P2 start %p "
           "(array order no longer matches address order)\n",
           (void *)owner, (void *)p2Start);
    fail = 1;
  }

  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: owning-program lookup finds the address maximum, not the array-order last\n");
  }
  return fail;
}

/* test_prescan_generation_rearm
 * T2.5: After a generation bump, the pre-scan re-runs on first touch.
 * Must fail if forthRunGenCheckReset clears the dictionary but not
 * forthScanHead (scan skipped after dict clear -> FUNCTION_NOT_FOUND). */
static int test_prescan_generation_rearm(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 10, ':', ' ', 'F', 'W', 'D', ' ', '4', '2', ' ', ';',
    0x8B, 0x1A, 0xFD, 3,  'F', 'W', 'D'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthProgramStep(beginOfProgramMemory + 3);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step 1 raised error %d\n", lastErrorCode);
    programRunStop = savedRS;
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }
  forthProgramStep(beginOfProgramMemory + 14 + 3);

  uint16_t countAfter = fdict.count;

  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step 2 raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(42)) {
    printf("    FAIL: X != 42 after step 2\n");
    fail = 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  programRunStop = PGM_RUNNING;
  forthProgramStep(beginOfProgramMemory + 3);
  programRunStop = savedRS;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step 1 (new gen) raised error %d (expected no error — re-scan should recompile FWD)\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(42)) {
    printf("    FAIL: X != 42 after step 1 (new gen)\n");
    fail = 1;
  }
  if (fdict.count != countAfter) {
    printf("    FAIL: fdict.count = %u (expected %u — fresh compile, not doubled, not missing)\n",
           fdict.count, countAfter);
    fail = 1;
  }

  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: generation re-arm — re-scan recompiles after bump, count stable\n");
  return fail;
}

/* test_prescan_error_halts
 * T2.6: A pre-scan error must halt execution of the step; tail tokens must
 * not execute.
 * Must fail if pre-scan errors are swallowed and the step executes against
 * a partial dictionary, or if the halt lands after the tail ran. */
static int test_prescan_error_halts(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 20, '7', '7', ' ', ':', ' ', 'C', '6', ' ', 'N', 'O', 'S', 'U', 'C', 'H', 'W', 'O', 'R', 'D', ' ', ';',
    0x8B, 0x1A, 0xFD, 1,  '1'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthPushInt32(555);
  forthProgramStep(beginOfProgramMemory + 3);
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_FUNCTION_NOT_FOUND=%d — pre-scan error should halt)\n",
           lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
    fail = 1;
  }
  if (!x_is_longint(555)) {
    printf("    FAIL: X != 555 sentinel (tail '77' executed — pre-scan did not halt)\n");
    fail = 1;
  }
  if (fdict.count != 0) {
    printf("    FAIL: fdict.count = %u (expected 0 — no definitions compiled on error)\n", fdict.count);
    fail = 1;
  }

  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: pre-scan error halts — tail not executed, dict empty\n");
  return fail;
}

/* test_prescan_error_rolls_back_prior_defs
 * R4-4: a pre-scan is not transactional across its own steps. Unlike
 * test_prescan_error_halts (error in the FIRST definition, so nothing prior
 * ever succeeds), this program has a VALID definition before the failing
 * one: ": G 1 ;" then ": B NOPE ;" (NOPE undefined). Before the fix, the
 * first failed touch left G compiled (fdict.count=1, program unrecorded), and
 * because the program stays unrecorded on error, a retry re-scanned from
 * scratch and compiled a SECOND G (count=2) before failing again — probed
 * exactly this: counts 1 then 2. A calculator owner retrying a program with
 * one typo would consume RAM on every attempt until the dictionary filled.
 * With the fix, both touches roll back to empty: count 0 both times. */
static int test_prescan_error_rolls_back_prior_defs(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 7,  ':', ' ', 'G', ' ', '1', ' ', ';',
    0x8B, 0x1A, 0xFD, 10, ':', ' ', 'B', ' ', 'N', 'O', 'P', 'E', ' ', ';'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  lastErrorCode = ERROR_NONE;
  forthProgramStep(beginOfProgramMemory + 3);
  uint16_t countAfterFirst = fdict.count;

  lastErrorCode = ERROR_NONE;
  forthProgramStep(beginOfProgramMemory + 3);
  uint16_t countAfterSecond = fdict.count;

  programRunStop = savedRS;

  int fail = 0;
  if (countAfterFirst != 0) {
    printf("    FAIL: fdict.count = %u after first touch (expected 0 — G should have rolled back)\n",
           countAfterFirst);
    fail = 1;
  }
  if (countAfterSecond != 0) {
    printf("    FAIL: fdict.count = %u after second touch (expected 0 — each retry rolls back)\n",
           countAfterSecond);
    fail = 1;
  }
  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL: second-touch lastErrorCode = %d, expected ERROR_FUNCTION_NOT_FOUND (%d)\n",
           lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
    fail = 1;
  }

  forthDictClear();
  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: pre-scan error rolls back the prior valid def — count 0 after both touches\n");
  }
  return fail;
}

/* test_prescan_last_step_visible
 * T2.7: A definition in the last step of the last program must be visible
 * to earlier steps in the same program.
 * Must fail if the walk bound drops the final step (exclusive-bound bug)
 * or mishandles the NULL next-program sentinel. */
static int test_prescan_last_step_visible(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 5,  'L', 'A', 'S', 'T', '7',
    0x8B, 0x1A, 0xFD, 11, ':', ' ', 'L', 'A', 'S', 'T', '7', ' ', '3', ' ', ';'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthProgramStep(beginOfProgramMemory + 3);
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: step raised error %d (expected no error — LAST7 should be visible from pre-scan)\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(3)) {
    printf("    FAIL: X != 3\n");
    fail = 1;
  }

  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: last step visible — walk bound includes final step with NULL next-program\n");
  return fail;
}

/* test_prescan_two_programs_first_touch
 * T2.8 / R2-T4 item 1: Two programs in one write, each with a single Forth
 * step that both defines and immediately calls its own word (": P1W 9 ; P1W",
 * ": P2W 4 ; P2W") — one forthProgramStep call does both. Touch P1, then P2,
 * then P1 again, all in ONE generation.
 * Must fail if the scanned-program list is a single slot instead of an array:
 * P2's touch would evict P1's record, so the third P1 touch would re-scan and
 * recompile P1W a second time (fdict.count would read 3, not 2, and fdict.here
 * would grow) instead of just re-running the tail call. */
static int test_prescan_two_programs_first_touch(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 13, ':', ' ', 'P', '1', 'W', ' ', '9', ' ', ';', ' ', 'P', '1', 'W',
    0x85, 0xB2,
    0x8B, 0x1A, 0xFD, 13, ':', ' ', 'P', '2', 'W', ' ', '4', ' ', ';', ' ', 'P', '2', 'W'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  if (numberOfPrograms < 2) {
    printf("    FAIL (SKIP): numberOfPrograms = %u, expected >= 2\n", numberOfPrograms);
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  /* Program 1 step: header(3)+len(1)+payload(13) = 17 bytes, then a 2-byte
   * ITM_END separator. Program 2 step starts at offset 17+2 = 19. */
  const uint8_t *prog1Step = beginOfProgramMemory;
  const uint8_t *prog2Step = beginOfProgramMemory + 17 + 2;

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;

  forthProgramStep(prog1Step + 3);
  if (lastErrorCode != ERROR_NONE || !x_is_longint(9) || fdict.count != 1) {
    printf("    FAIL: program 1 first touch — error %d, X==9? %d, count=%u (expected 0/1/1)\n",
           lastErrorCode, x_is_longint(9), fdict.count);
    programRunStop = savedRS;
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  forthProgramStep(prog2Step + 3);
  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: program 2 first touch raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(4)) {
    printf("    FAIL: X != 4 after program 2 first touch\n");
    fail = 1;
  }
  else if (fdict.count != 2) {
    printf("    FAIL: fdict.count = %u after program 2 (expected 2: P1W, P2W)\n", fdict.count);
    fail = 1;
  }

  if (fail) {
    programRunStop = savedRS;
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  uint16_t hereAfterBothTouches = fdict.here;

  /* Third touch: P1 again, same generation. Must re-run the tail call
   * (X==9) WITHOUT re-scanning/recompiling — count and here must not move. */
  forthProgramStep(prog1Step + 3);
  programRunStop = savedRS;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: program 1 third touch raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(9)) {
    printf("    FAIL: X != 9 after program 1 third touch\n");
    fail = 1;
  }
  if (fdict.count != 2) {
    printf("    FAIL: fdict.count = %u after third P1 touch (expected 2 — no re-scan)\n",
           fdict.count);
    fail = 1;
  }
  if (fdict.here != hereAfterBothTouches) {
    printf("    FAIL: fdict.here moved from %u to %u after third P1 touch (re-scan grew the dict)\n",
           hereAfterBothTouches, fdict.here);
    fail = 1;
  }

  printf("  FORTH ARENA (post-prescan): here=%u sizeBlocks=%u\n", fdict.here, fdict.sizeBlocks);

  forthDictClear();
  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: two programs, three touches — independent scans, no eviction, no re-scan\n");
  }
  return fail;
}

/* test_scan_dynamic_no_cliff
 * F1-3: R4-E1 successor — 9 programs exceed the old 8-slot array cliff.
 * Records live in the dictionary arena; capacity failure is ordinary
 * dictionary exhaustion. Re-touch stability: no re-scan, no recompile. */
static int test_scan_dynamic_no_cliff(void)
{
  uint8_t prog[169];   /* 9 steps x 17 bytes + 8 separators x 2 bytes */
  uint16_t p = 0;
  int i;
  for (i = 1; i <= 9; i++) {
    char src[16];
    int len = snprintf(src, sizeof(src), ": P%dW %d ; P%dW", i, i, i);
    prog[p++] = 0x8B; prog[p++] = 0x1A; prog[p++] = 0xFD;
    prog[p++] = (uint8_t)len;
    memcpy(prog + p, src, (size_t)len);
    p += (uint16_t)len;
    if (i != 9) { prog[p++] = 0x85; prog[p++] = 0xB2; }
  }

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  if (numberOfPrograms < 9) {
    printf("    FAIL (SKIP): numberOfPrograms = %u, expected >= 9\n", numberOfPrograms);
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  forthRunGenBump();
  lastErrorCode = ERROR_NONE;
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  int fail = 0;

  /* Touch programs 1..9 in order */
  for (i = 1; i <= 9; i++) {
    const uint8_t *stepStart = beginOfProgramMemory + (i - 1) * 19;
    forthProgramStep(stepStart + 3);
    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: program %d first touch raised error %d\n", i, lastErrorCode);
      fail = 1;
      break;
    }
    if (!x_is_longint(i)) {
      printf("    FAIL: X != %d after program %d first touch\n", i, i);
      fail = 1;
      break;
    }
    if (fdict.count != (uint16_t)i) {
      printf("    FAIL: fdict.count = %u after program %d (expected %d)\n",
             fdict.count, i, i);
      fail = 1;
      break;
    }
  }

  if (fail) {
    programRunStop = savedRS;
    forthDictClear();
    cleanupTestProgram();
    return 1;
  }

  uint16_t hereAfter = fdict.here;

  /* Re-touch program 1 and program 9 — must NOT re-scan or recompile */
  const uint8_t *step1 = beginOfProgramMemory + 0;
  const uint8_t *step9 = beginOfProgramMemory + 8 * 19;

  forthProgramStep(step1 + 3);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: program 1 re-touch raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(1)) {
    printf("    FAIL: X != 1 after program 1 re-touch\n");
    fail = 1;
  }
  else if (fdict.count != 9) {
    printf("    FAIL: fdict.count = %u after program 1 re-touch (expected 9)\n", fdict.count);
    fail = 1;
  }
  else if (fdict.here != hereAfter) {
    printf("    FAIL: fdict.here moved from %u to %u after program 1 re-touch\n",
           hereAfter, fdict.here);
    fail = 1;
  }

  if (!fail) {
    forthProgramStep(step9 + 3);
    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: program 9 re-touch raised error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(9)) {
      printf("    FAIL: X != 9 after program 9 re-touch\n");
      fail = 1;
    }
    else if (fdict.count != 9) {
      printf("    FAIL: fdict.count = %u after program 9 re-touch (expected 9)\n", fdict.count);
      fail = 1;
    }
    else if (fdict.here != hereAfter) {
      printf("    FAIL: fdict.here moved from %u to %u after program 9 re-touch\n",
             hereAfter, fdict.here);
      fail = 1;
    }
  }

  programRunStop = savedRS;
  forthDictClear();
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: nine programs recorded, re-touch stable (no re-scan, no recompile)\n");
  }
  return fail;
}

/* test_recurse_compile_only
 * F1-4: RECURSE is compile-only immediate; emits call to open definition by index. */
static int test_recurse_compile_only(void)
{
  int fail = 0;
  uint16_t idx;

  /* --- Subcase 1: Body shape --- */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SELFW RECURSE ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL [1]: define SELFW error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!forthFindColon("SELFW", &idx)) {
    printf("    FAIL [1]: SELFW not found\n");
    fail = 1;
  }
  else if (idx != fdict.count - 1) {
    printf("    FAIL [1]: SELFW idx %u != count-1 %u\n", idx, fdict.count - 1);
    fail = 1;
  }
  else {
    uint16_t bodyOff = fdict.latest + (uint16_t)TO_BLOCKS(6 + 5) * BYTES_PER_BLOCK;
    ftoken_t toks[2];
    memcpy(toks, fdict.base + bodyOff, sizeof(toks));
    if (toks[0] != (ftoken_t)(0x1000 + idx)) {
      printf("    FAIL [1]: token 0 = 0x%04X, expected 0x%04X\n", toks[0], 0x1000 + idx);
      fail = 1;
    }
    else if (toks[1] != FTOK_EXIT) {
      printf("    FAIL [1]: token 1 = 0x%04X, expected 0x%04X (EXIT)\n", toks[1], FTOK_EXIT);
      fail = 1;
    }
  }
  if (!fail) {
    printf("    PASS [1]: body shape — self-call token 0x%04X, EXIT\n", (0x1000 + idx));
  }

  /* --- Subcase 2: Runtime self-call is bounded and unwinds --- */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("SELFW");
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL [2]: expected ERROR_RAM_FULL, got %d\n", lastErrorCode);
    fail = 1;
  }
  else if (forthTestGetDepth() != 0) {
    printf("    FAIL [2]: forthDepth = %d, expected 0\n", forthTestGetDepth());
    fail = 1;
  }
  else if (forthTestGetRsp() != 0) {
    printf("    FAIL [2]: forthRsp = %d, expected 0\n", forthTestGetRsp());
    fail = 1;
  }
  if (!fail) {
    printf("    PASS [2]: runtime self-call bounded by rstack (ERROR_RAM_FULL, unwound)\n");
  }
  lastErrorCode = ERROR_NONE;

  /* --- Subcase 3: Interpret state rejects --- */
  uint16_t hereBefore = fdict.here;
  uint16_t countBefore = fdict.count;
  forthOuterInterpret("RECURSE");
  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL [3]: expected ERROR_OPERATION_UNDEFINED, got %d\n", lastErrorCode);
    fail = 1;
  }
  else if (fdict.here != hereBefore) {
    printf("    FAIL [3]: here moved from %u to %u\n", hereBefore, fdict.here);
    fail = 1;
  }
  else if (fdict.count != countBefore) {
    printf("    FAIL [3]: count moved from %u to %u\n", countBefore, fdict.count);
    fail = 1;
  }
  if (!fail) {
    printf("    PASS [3]: interpret-state RECURSE rejected (ERROR_OPERATION_UNDEFINED, dict unchanged)\n");
  }
  lastErrorCode = ERROR_NONE;

  /* --- Subcase 4: RECURSE is not the bare name --- */
  forthOuterInterpret(": WOLD 5 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL [4]: define WOLD (first) error %d\n", lastErrorCode);
    fail = 1;
  }
  else {
    forthOuterInterpret(": WOLD WOLD 1 + ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL [4]: redefine WOLD error %d\n", lastErrorCode);
      fail = 1;
    }
    else {
      forthOuterInterpret("WOLD");
      if (lastErrorCode != ERROR_NONE) {
        printf("    FAIL [4]: execute WOLD error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (!x_is_longint(6)) {
        printf("    FAIL [4]: X != 6 after WOLD\n");
        fail = 1;
      }
    }
  }
  if (!fail) {
    printf("    PASS [4]: bare name redefinition works (WOLD -> 6)\n");
  }
  lastErrorCode = ERROR_NONE;

  /* --- Subcase 5: Program pre-scan compiles it; emitted call really recurses --- */
  uint8_t prog[] = { 0x8B, 0x1A, 0xFD, 19,
    ':', ' ', 'P', 'R', 'W', ' ',
    'R', 'E', 'C', 'U', 'R', 'S', 'E', ' ', ';', ' ',
    'P', 'R', 'W' };
   uint8_t savedRunStop = programRunStop;
  int sub5Fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL [5]: writeTestProgram failed\n");
    sub5Fail = 1;
  }
  else {
    const uint8_t *payload = beginOfProgramMemory + 3;
    programRunStop = PGM_RUNNING;
    lastErrorCode = ERROR_NONE;
    forthProgramStep(payload);
    programRunStop = savedRunStop;

    if (lastErrorCode != ERROR_RAM_FULL) {
      printf("    FAIL [5]: expected ERROR_RAM_FULL, got %d\n", lastErrorCode);
      sub5Fail = 1;
    }
    else if (forthFindColon("PRW", &idx)) {   /* F3-3: program-owned, invisible interactively */
      printf("    FAIL [5]: PRW visible from interactive scope (F3-3 isolation)\n");
      sub5Fail = 1;
    }
  }
  lastErrorCode = ERROR_NONE;
  cleanupTestProgram();

  if (sub5Fail) {
    fail = 1;
  }
  else {
    printf("    PASS [5]: program pre-scan compiled RECURSE; execution recursed to ERROR_RAM_FULL\n");
  }

  forthDictClear();
  return fail;
}

/* test_validate_restored_bodies
 * F1-5: full threaded-code validator pins. Independently reported
 * subcases — one PASS line each. T1.3b idiom: build, corrupt, call
 * forthGDictValidateRestored() directly, assert outcome, release orphan,
 * forthGDictClear() between subcases. Hand-built entries use gbegin_word/
 * gend_word with 4-glyph names only (header = 8 bytes, no padding). */
static int test_validate_restored_bodies(void)
{
  int fail = 0;
  uint16_t idx;

  /* ---- P0: a valid gdict validates clean ---- */
  {
    int p0Fail = 0;
    forthDictClear();
    forthGDictClear();
    /* Build 4 gdict words: GP0A, GP0B, GP0C, GP0D with calls */
    {
      uint16_t wa = gbegin_word("GP0A", 4);
      if (wa == FORTH_NULL) { printf("    FAIL P0: alloc GP0A\n"); return 1; }
      gemit(T_ILIT);
      { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
      gend_word();

      uint16_t wb = gbegin_word("GP0B", 4);
      if (wb == FORTH_NULL) { printf("    FAIL P0: alloc GP0B\n"); return 1; }
      gemit(FORTH_GCALL_BASE + 0);  /* call GP0A (global index 0) */
      gend_word();

      uint16_t wc = gbegin_word("GP0C", 4);
      if (wc == FORTH_NULL) { printf("    FAIL P0: alloc GP0C\n"); return 1; }
      gemit(FORTH_GCALL_BASE + 2);  /* self-call GP0C (global index 2) */
      gend_word();

      uint16_t wd = gbegin_word("GP0D", 4);
      if (wd == FORTH_NULL) { printf("    FAIL P0: alloc GP0D\n"); return 1; }
      gemit(T_ILIT);
      { int32_t v = 99; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
      gend_word();
    }
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL P0: valid gdict reset\n");
      fail = p0Fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else if (gdict.count != 4) {
      printf("    FAIL P0: count=%u, expected 4\n", gdict.count);
      fail = p0Fail = 1;
    } else if (!forthFindColon("GP0A", &idx) || !forthFindColon("GP0B", &idx) ||
               !forthFindColon("GP0C", &idx) || !forthFindColon("GP0D", &idx)) {
      printf("    FAIL P0: word not found\n");
      fail = p0Fail = 1;
    }
    if (!p0Fail) printf("    PASS P0: mixed gdict (calls, self-call, literals) validates clean\n");
    forthGDictClear();
    forthDictClear();
  }

  /* ---- P0b: legal backward branch validates clean ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("LOOP", 4);
    if (w == FORTH_NULL) { printf("    FAIL P0b: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gemit(T_BR);
    { int16_t delta = (int16_t)(-5); memcpy(gdict.base + gdict.here, &delta, 2); gdict.here += 2; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL P0b: legal backward branch reset\n"); fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else {
      if (!fail) printf("    PASS P0b: legal backward branch validates clean\n");
    }
    forthGDictClear();
  }

  /* ---- V-B1: missing EXIT ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB1", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B1: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* Body: ILIT(2) + int32(4) + EXIT(2) = 8 bytes. Replace EXIT with DUP. */
    uint16_t bodyStart = w + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
    ftoken_t badTok = (ftoken_t)0x0001;  /* DUP */
    memcpy(gdict.base + bodyStart + 6, &badTok, 2);
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B1: missing EXIT survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B1: missing EXIT detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B2: call index above own index (global space) ---- */
  {
    forthGDictClear();
    uint16_t wa = gbegin_word("VA2", 3);
    if (wa == FORTH_NULL) { printf("    FAIL V-B2: alloc VA2\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint16_t wb = gbegin_word("VB2", 3);
    if (wb == FORTH_NULL) { printf("    FAIL V-B2: alloc VB2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call VA2 (global index 0) */
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* VB2 is latest (index 1). Patch call to FORTH_GCALL_BASE+5 (index 5 > 1). */
    uint16_t bodyStart = wb + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
    ftoken_t badTok = (ftoken_t)(FORTH_GCALL_BASE + 5);
    memcpy(gdict.base + bodyStart, &badTok, 2);
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B2: bad call index survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B2: call index > entryIdx detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B3: branch into a literal payload ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("BINT", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B3: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gemit(T_BR);
    /* delta -4: target = bodyStart+10 + (-4)*2 = bodyStart+2 (inside ILIT operand) */
    { int16_t delta = (int16_t)(-4); memcpy(gdict.base + gdict.here, &delta, 2); gdict.here += 2; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B3: branch into literal survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B3: branch into literal payload detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B4: reserved token ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("RSV4", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B4: alloc\n"); return 1; }
    { ftoken_t badTok = (ftoken_t)0x7F05; gemit(badTok); }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B4: reserved token survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B4: reserved token detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B5: restored smudge ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB5", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B5: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    ((forthHeader_t *)(gdict.base + gdict.latest))->flags |= FF_SMUDGE;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B5: smudged entry survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B5: restored smudge detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B6: nonzero header padding ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB6", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B6: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* Name "VB6" is 3 glyphs. Header: 6 + 3 = 9, rounds to 12.
     * Padding bytes at latest + 9 .. latest + 11. */
    gdict.base[gdict.latest + 9] = 0xAA;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B6: nonzero padding survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B6: nonzero header padding detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B7: foreign owner detected (gdict — must be FORTH_OWNER_GLOBAL) ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB7", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B7: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    ((forthHeader_t *)(gdict.base + gdict.latest))->owner = 0x1234;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B7: foreign owner survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B7: foreign owner detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B8: allocate zeroed header padding over poisoned byte ---- */
  {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": AAAA 1 ;");
    if (lastErrorCode != ERROR_NONE || !fdict.base) {
      printf("    FAIL V-B8: setup failed\n");
      fail = 1;
    } else {
      /* Name "AAAA" is 4 glyphs. Next entry: 6+4=10, rounds to 12.
       * Pads at fdict.here+10 .. fdict.here+11. */
      if (fdict.here + 12 > fdict.sizeBlocks * BYTES_PER_BLOCK) {
        printf("    FAIL V-B8: CONFIG poke target outside region\n");
        fail = 1;
        forthDictClear();
      } else {
        fdict.base[fdict.here + 9] = 0xAA;
        forthOuterInterpret(": BBB 2 ;");
        if (lastErrorCode != ERROR_NONE) {
          printf("    FAIL V-B8: BBB compile failed\n"); fail = 1;
          forthDictClear();
        } else if (!(fdict.base[fdict.latest + 9] == 0 &&
                     fdict.base[fdict.latest + 10] == 0 &&
                     fdict.base[fdict.latest + 11] == 0)) {
          printf("    FAIL V-B8: padding not zeroed over poisoned byte\n"); fail = 1;
          forthDictClear();
        } else {
          if (!fail) printf("    PASS V-B8: allocate zeroed header padding over poisoned byte\n");
          forthDictClear();
        }
      }
    }
  }

  /* ---- V-B9: C47 item out of range ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("ITM7", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B9: alloc\n"); return 1; }
    gemit(T_C47);
    { ftoken_t badId = (ftoken_t)0xFFFF; gemit(badId); }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B9: out-of-range C47 item survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B9: C47 item out of range detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-G1: transient call token rejected in global body ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VG1", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-G1: alloc\n"); return 1; }
    gemit((ftoken_t)0x1000);  /* transient call token — illegal in gdict */
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-G1: transient call token survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-G1: transient call token rejected in global body\n");
    }
    forthGDictClear();
  }

  /* ---- V-G2: FF_IMMEDIATE survives restore validation ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VG2", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-G2: alloc\n"); return 1; }
    gend_word();
    ((forthHeader_t *)(gdict.base + gdict.latest))->flags = FF_IMMEDIATE;
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL V-G2: FF_IMMEDIATE rejected\n"); fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else {
      if (!fail) printf("    PASS V-G2: FF_IMMEDIATE survives restore validation\n");
    }
    forthGDictClear();
  }

  /* ---- Nesting probe: transient TW2 calls global GW2 cross-region ---- */
  {
    forthGDictClear();
    /* Build GW2 in gdict: body = call GW1 (global index 0), ILIT 35, EXIT */
    uint16_t gw1 = gbegin_word("GW1", 3);
    if (gw1 == FORTH_NULL) { printf("    FAIL nesting: alloc GW1\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 7; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint16_t gw2 = gbegin_word("GW2", 3);
    if (gw2 == FORTH_NULL) { printf("    FAIL nesting: alloc GW2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call GW1 (global index 0) */
    gemit(T_ILIT);
    { int32_t v = 35; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    /* Build TW2 in fdict: body = call token for GW2 (global), EXIT */
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": TW2 GW2 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("TW2", &idx)) {
      printf("    FAIL nesting: TW2 compile failed\n"); fail = 1;
    } else {
      lastErrorCode = ERROR_NONE;
      uint16_t tw2ref;
      if (forthFindColon("TW2", &tw2ref)) {
        forthInner(tw2ref, false);
        if (lastErrorCode != ERROR_NONE) {
          printf("    FAIL nesting: execution error %d\n", lastErrorCode); fail = 1;
        } else if (!x_is_longint(35)) {
          printf("    FAIL nesting: X != 35 after cross-region call\n"); fail = 1;
        } else {
          if (!fail) printf("    PASS NEST: TW2->GW2->GW1 cross-region nesting, X=35\n");
        }
      } else {
        printf("    FAIL nesting: TW2 not found\n"); fail = 1;
      }
    }
    forthDictClear();
    forthGDictClear();
  }

  /* Rebuild-and-use after final reset */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": FRESH 7 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("FRESH", &idx)) {
      printf("    FAIL: dict unusable after final reset\n"); fail = 1;
    }
  }
  forthDictClear();
  forthGDictClear();

  return fail;
}

/* test_accept_run_lifecycle
 * F15-1: End-to-end acceptance of the F1 lifecycle through the real XEQ/R/S
 * engine.  Five subcases covering §8.9 items 1, 7(a,b), 9(a,b). */
static int test_accept_run_lifecycle(void)
{
  uint16_t idx;
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* ---- Subcase 1: §8.9 item 1 — define-and-use in one program ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '5', 'A',
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',
      'D', 'U', 'P', ' ', '*', ' ', ';',
      0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'S', 'Q',
      0x8B, 0x1A, 0xFD, 0x00,
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [1] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F5A", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [1] FAIL: findNamedLabel(\"F5A\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(9)) {
      printf("    [1] FAIL: X != 9 (got %s)\n",
             getRegisterDataType(REGISTER_X) == dtLongInteger ? "longint(!=9)" : "not longint");
      fail = 1;
    }
    else {
      printf("    [1] PASS: define-and-use in one program yields X=9\n");
    }

    /* Record count for subcase 2; keep program in place */
  }

  /* ---- Subcase 2: §8.9 item 9(a) — second run identical, no accumulation ---- */
  {
    uint16_t count1 = fdict.count;

    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    calcRegister_t lbl = findNamedLabel("F5A", GLOBAL_LABELS);
    /* lbl already resolved above, but re-resolve for discipline */
    if (lbl == INVALID_VARIABLE) {
      printf("    [2] FAIL: findNamedLabel(\"F5A\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(9)) {
      printf("    [2] FAIL: X != 9 on second run\n");
      fail = 1;
    }
    else if (fdict.count != count1) {
      printf("    [2] FAIL: fdict.count changed (%u -> %u; expected %u)\n",
             count1, fdict.count, count1);
      fail = 1;
    }
    else {
      printf("    [2] PASS: second run identical, no accumulation\n");
    }

    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 3: §8.9 item 9(b) — R/S resume is a fresh lifetime ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '5', 'B',
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',
      'D', 'U', 'P', ' ', '*', ' ', ';',
      0x46,
      0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'S', 'Q',
      0x8B, 0x1A, 0xFD, 0x00,
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [3] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F5B", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [3] FAIL: findNamedLabel(\"F5B\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: initial run error %d\n", lastErrorCode);
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }
    else if (programRunStop != PGM_WAITING) {
      printf("    [3] FAIL: STOP did not halt (programRunStop=%d, expected PGM_WAITING=2)\n", programRunStop);
      fail = 1;
    }
    else {
      /* During pause: define PZW interactively */
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": PZW 5 ;");
      if (lastErrorCode != ERROR_NONE || !forthFindColon("PZW", &idx)) {
        printf("    [3] FAIL: PZW setup during pause failed\n");
        fail = 1;
        programRunStop = savedRS;
        forthDictClear();
        cleanupTestProgram();
        return 1;
      }

      /* Resume: fnRunProgram sets dynamicMenuItem itself */
      lastErrorCode = ERROR_NONE;
      fnRunProgram(0);

      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: resume error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (!x_is_longint(9)) {
        printf("    [3] FAIL: X != 9 after resume (SQ not re-derived)\n");
        fail = 1;
      }
      else if (forthFindColon("SQ", &idx)) {   /* F3-3: program-owned, invisible interactively */
        printf("    [3] FAIL: SQ visible from interactive scope after resume (F3-3 isolation)\n");
        fail = 1;
      }
      else if (forthFindColon("PZW", &idx)) {
        printf("    [3] FAIL: PZW survived resume (not a fresh lifetime)\n");
        fail = 1;
      }
      else {
        printf("    [3] PASS: R/S resume re-derives program, drops interactive word\n");
      }
    }

    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 4: §8.9 item 7(a) — unterminated definition halts, no smudge ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '5', 'C',
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x0A, ':', ' ', 'S', 'Q', ' ',
      'D', 'U', 'P', ' ', '*',
      0x8B, 0x1A, 0xFD, 0x01, '7',
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [4] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F5C", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [4] FAIL: findNamedLabel(\"F5C\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    /* Baseline: push 42 */
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("42");
    if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
      printf("    [4] FAIL: baseline 42 setup failed\n");
      fail = 1;
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [4] FAIL: expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(42)) {
      printf("    [4] FAIL: X != 42 (later step executed)\n");
      fail = 1;
    }
    else {
      /* Verify no smudge: define SQ cleanly */
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": SQ 2 ;");
      if (lastErrorCode != ERROR_NONE || !forthFindColon("SQ", &idx)) {
        printf("    [4] FAIL: SQ re-define after aborted def failed\n");
        fail = 1;
      }
      else {
        printf("    [4] PASS: unterminated def halts, no smudged leak\n");
      }
    }

    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 5: §8.9 item 7(b) — undefined word halts at its step ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '5', 'D',
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x05, '3', ' ', 'S', 'Q', 'X',
      0x8B, 0x1A, 0xFD, 0x01, '7',
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [5] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F5D", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [5] FAIL: findNamedLabel(\"F5D\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    fnExecute(lbl);

    if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
      printf("    [5] FAIL: expected ERROR_FUNCTION_NOT_FOUND, got %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(3)) {
      printf("    [5] FAIL: X != 3 (3 was pushed, SQX failed, run halted)\n");
      fail = 1;
    }
    else {
      printf("    [5] PASS: undefined word halts at its step, later step skipped\n");
    }

    lastErrorCode = ERROR_NONE;
    forthDictClear();
    cleanupTestProgram();
  }

  programRunStop = savedRS;
  return fail;
}

/* test_accept_entry_state_roundtrip
 * F15-2: §8.9 item 2(a-d) — PEM derives keypad state from the landed step,
 * including after power-off. Four independently accumulated subcases. */
static int test_accept_entry_state_roundtrip(void)
{
  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedMenu = currentMenu();
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void runFunction(int16_t func);
  extern void pemAlpha(int16_t item);
  extern void showSoftmenu(int16_t menu);
  extern void pemCloseNumberInput(void);

  /* ---- Subcase 1: §8.9 item 2(a) — RPN step keeps RPN number entry ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'E', '2', 'A',
      0x4C,
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x01, '7',
      0x8B, 0x1A, 0xFD, 0x00,
      0x4C,
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [1] FAIL: writeTestProgram failed\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      calcMode = CM_PEM;
      catalog = CATALOG_NONE;
      tam.mode = 0;
      tam.function = 0;
      aimBuffer[0] = 0;
      dynamicMenuItem = -1;
      pemCursorIsZerothStep = false;
      lastErrorCode = ERROR_NONE;
      clearSystemFlag(FLAG_ALPHA);

      fnGotoDot(2);

      if (currentLocalStepNumber != 2) {
        printf("    [1] FAIL: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
        fail = 1;
      }
      else if (currentStep != beginOfProgramMemory + 6) {
        printf("    [1] FAIL: currentStep = %p, expected %p (+6)\n",
               (void *)currentStep, (void *)(beginOfProgramMemory + 6));
        fail = 1;
      }
      else {
        runFunction(ITM_2);

        if (lastErrorCode != ERROR_NONE) {
          printf("    [1] FAIL: lastErrorCode = %d\n", lastErrorCode);
          fail = 1;
        }
        else if (getSystemFlag(FLAG_ALPHA)) {
          printf("    [1] FAIL: FLAG_ALPHA set — should be clear for RPN\n");
          fail = 1;
        }
        else if (tam.function == ITM_FORTH) {
          printf("    [1] FAIL: tam.function = ITM_FORTH\n");
          fail = 1;
        }
        else if (aimBuffer[0] != '+' || aimBuffer[1] != '2') {
          printf("    [1] FAIL: aimBuffer[0]='%c'[1]='%c' — expected '+'/'2'\n",
                 aimBuffer[0], aimBuffer[1]);
          fail = 1;
        }
        else {
          printf("    [1] PASS: RPN landing routes digit 2 to number entry\n");
        }
      }

      if (aimBuffer[0] != 0) { pemCloseNumberInput(); aimBuffer[0] = 0; }
    }
    cleanupTestProgram();
  }

  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  tam.mode = 0;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE;

  /* ---- Subcase 2: §8.9 item 2(b) — source step opens Forth text capture ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'E', '2', 'A',
      0x4C,
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x01, '7',
      0x8B, 0x1A, 0xFD, 0x00,
      0x4C,
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [2] FAIL: writeTestProgram failed\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      calcMode = CM_PEM;
      catalog = CATALOG_NONE;
      tam.mode = 0;
      tam.function = 0;
      aimBuffer[0] = 0;
      dynamicMenuItem = -1;
      pemCursorIsZerothStep = false;
      lastErrorCode = ERROR_NONE;
      clearSystemFlag(FLAG_ALPHA);

      fnGotoDot(4);

      if (currentLocalStepNumber != 4) {
        printf("    [2] FAIL: currentLocalStepNumber = %u, expected 4\n", currentLocalStepNumber);
        fail = 1;
      }
      else if (currentStep != beginOfProgramMemory + 11) {
        printf("    [2] FAIL: currentStep = %p, expected %p (+11)\n",
               (void *)currentStep, (void *)(beginOfProgramMemory + 11));
        fail = 1;
      }
      else {
        runFunction(ITM_2);

        if (lastErrorCode != ERROR_NONE) {
          printf("    [2] FAIL: lastErrorCode = %d\n", lastErrorCode);
          fail = 1;
        }
        else if (!getSystemFlag(FLAG_ALPHA)) {
          printf("    [2] FAIL: FLAG_ALPHA not set\n");
          fail = 1;
        }
        else if (tam.function != ITM_FORTH) {
          printf("    [2] FAIL: tam.function = %d, expected ITM_FORTH (%d)\n",
                 (int)tam.function, ITM_FORTH);
          fail = 1;
        }
        else if (strcmp(forthTestCapText(), "2") != 0) {
          printf("    [2] FAIL: cap text = '%s', expected '2'\n", forthTestCapText());
          fail = 1;
        }
        else {
          printf("    [2] PASS: source landing routes digit 2 to Forth capture\n");
        }
      }

      if (getSystemFlag(FLAG_ALPHA)) { pemAlpha(ITM_ENTER); }
      forthCapClose();
    }
    cleanupTestProgram();
  }

  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  tam.mode = 0;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE;

  /* ---- Subcase 3: §8.9 item 2(c) — opening and closing markers symmetric ---- */
  {
    int sc3 = 0;

    /* Opening half */
    {
      uint8_t prog[] = {
        0x01, 0xFD, 0x03, 'E', '2', 'A',
        0x4C,
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x01, '7',
        0x8B, 0x1A, 0xFD, 0x00,
        0x4C,
        0x04
      };

      if (!writeTestProgram(prog, sizeof(prog))) {
        printf("    [3] FAIL: writeTestProgram (opening)\n");
        sc3 = 1;
      }
      else {
        programRunStop = PGM_STOPPED;
        calcMode = CM_PEM;
        catalog = CATALOG_NONE;
        tam.mode = 0;
        tam.function = 0;
        aimBuffer[0] = 0;
        dynamicMenuItem = -1;
        pemCursorIsZerothStep = false;
        lastErrorCode = ERROR_NONE;
        clearSystemFlag(FLAG_ALPHA);

        fnGotoDot(3);

        if (currentLocalStepNumber != 3) {
          printf("    [3] FAIL: opening step = %u, expected 3\n", currentLocalStepNumber);
          sc3 = 1;
        }
        else if (currentStep != beginOfProgramMemory + 7) {
          printf("    [3] FAIL: opening currentStep = %p, expected %p (+7)\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 7));
          sc3 = 1;
        }
        else {
          runFunction(ITM_2);

          if (lastErrorCode != ERROR_NONE) {
            printf("    [3] FAIL: opening error %d\n", lastErrorCode);
            sc3 = 1;
          }
          else if (!getSystemFlag(FLAG_ALPHA)) {
            printf("    [3] FAIL: opening FLAG_ALPHA not set\n");
            sc3 = 1;
          }
          else if (tam.function != ITM_FORTH) {
            printf("    [3] FAIL: opening tam.function = %d\n", (int)tam.function);
            sc3 = 1;
          }
          else if (strcmp(forthTestCapText(), "2") != 0) {
            printf("    [3] FAIL: opening cap text = '%s'\n", forthTestCapText());
            sc3 = 1;
          }
        }

        if (getSystemFlag(FLAG_ALPHA)) { pemAlpha(ITM_ENTER); }
        forthCapClose();
      }
      cleanupTestProgram();
    }

    clearSystemFlag(FLAG_ALPHA);
    tam.function = 0;
    tam.mode = 0;
    catalog = CATALOG_NONE;
    aimBuffer[0] = 0;
    dynamicMenuItem = -1;
    lastErrorCode = ERROR_NONE;

    /* Closing half */
    {
      uint8_t prog[] = {
        0x01, 0xFD, 0x03, 'E', '2', 'A',
        0x4C,
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x01, '7',
        0x8B, 0x1A, 0xFD, 0x00,
        0x4C,
        0x04
      };

      if (!writeTestProgram(prog, sizeof(prog))) {
        printf("    [3] FAIL: writeTestProgram (closing)\n");
        sc3 = 1;
      }
      else {
        programRunStop = PGM_STOPPED;
        calcMode = CM_PEM;
        catalog = CATALOG_NONE;
        tam.mode = 0;
        tam.function = 0;
        aimBuffer[0] = 0;
        dynamicMenuItem = -1;
        pemCursorIsZerothStep = false;
        lastErrorCode = ERROR_NONE;
        clearSystemFlag(FLAG_ALPHA);

        fnGotoDot(5);

        if (currentLocalStepNumber != 5) {
          printf("    [3] FAIL: closing step = %u, expected 5\n", currentLocalStepNumber);
          sc3 = 1;
        }
        else if (currentStep != beginOfProgramMemory + 16) {
          printf("    [3] FAIL: closing currentStep = %p, expected %p (+16)\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 16));
          sc3 = 1;
        }
        else {
          runFunction(ITM_2);

          if (lastErrorCode != ERROR_NONE) {
            printf("    [3] FAIL: closing error %d\n", lastErrorCode);
            sc3 = 1;
          }
          else if (getSystemFlag(FLAG_ALPHA)) {
            printf("    [3] FAIL: closing FLAG_ALPHA set — should be RPN\n");
            sc3 = 1;
          }
          else if (tam.function == ITM_FORTH) {
            printf("    [3] FAIL: closing tam.function = ITM_FORTH\n");
            sc3 = 1;
          }
          else if (aimBuffer[0] != '+' || aimBuffer[1] != '2') {
            printf("    [3] FAIL: closing aimBuffer[0]='%c'[1]='%c'\n",
                   aimBuffer[0], aimBuffer[1]);
            sc3 = 1;
          }
        }

        if (aimBuffer[0] != 0) { pemCloseNumberInput(); aimBuffer[0] = 0; }
      }
      cleanupTestProgram();
    }

    if (sc3) {
      fail = 1;
    }
    else {
      printf("    [3] PASS: opening marker captures and closing marker restores RPN\n");
    }
  }

  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  tam.mode = 0;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE;

  /* ---- Subcase 4: §8.9 item 2(d) — save/restore re-derives at source step ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'E', '2', 'A',
      0x4C,
      0x8B, 0x1A, 0xFD, 0x00,
      0x8B, 0x1A, 0xFD, 0x01, '7',
      0x8B, 0x1A, 0xFD, 0x00,
      0x4C,
      0x04
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [4] FAIL: writeTestProgram failed\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      calcMode = CM_PEM;
      catalog = CATALOG_NONE;
      tam.mode = 0;
      tam.function = 0;
      aimBuffer[0] = 0;
      dynamicMenuItem = -1;
      pemCursorIsZerothStep = false;
      lastErrorCode = ERROR_NONE;
      clearSystemFlag(FLAG_ALPHA);

      fnGotoDot(4);

      if (currentLocalStepNumber != 4) {
        printf("    [4] FAIL: pre-save step = %u, expected 4\n", currentLocalStepNumber);
        fail = 1;
      }
      else if (currentStep != beginOfProgramMemory + 11) {
        printf("    [4] FAIL: pre-save currentStep = %p, expected %p (+11)\n",
               (void *)currentStep, (void *)(beginOfProgramMemory + 11));
        fail = 1;
      }
      else if (getSystemFlag(FLAG_ALPHA)) {
        printf("    [4] FAIL: capture open before saveCalc\n");
        fail = 1;
      }
      else {
        saveCalc();

        fnGotoDot(2);

        if (currentLocalStepNumber != 2) {
          printf("    [4] FAIL: post-goto step = %u, expected 2\n", currentLocalStepNumber);
          fail = 1;
        }
        else if (currentStep != beginOfProgramMemory + 6) {
          printf("    [4] FAIL: post-goto currentStep = %p, expected %p (+6)\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 6));
          fail = 1;
        }

        aimBuffer[0] = 0;
        clearSystemFlag(FLAG_ALPHA);
        tam.function = 0;

        {
          bool_t savedLoad = loadTestPrograms;
          loadTestPrograms = false;
          restoreCalc();
          loadTestPrograms = savedLoad;
        }

        if (currentLocalStepNumber != 4) {
          printf("    [4] FAIL: post-restore step = %u, expected 4\n", currentLocalStepNumber);
          fail = 1;
        }
        else if (currentStep != beginOfProgramMemory + 11) {
          printf("    [4] FAIL: post-restore currentStep = %p, expected %p (+11)\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 11));
          fail = 1;
        }
        else if (pemCursorIsZerothStep) {
          printf("    [4] FAIL: post-restore pemCursorIsZerothStep = true\n");
          fail = 1;
        }
        else {
          runFunction(ITM_2);

          if (lastErrorCode != ERROR_NONE) {
            printf("    [4] FAIL: post-restore error %d\n", lastErrorCode);
            fail = 1;
          }
          else if (!getSystemFlag(FLAG_ALPHA)) {
            printf("    [4] FAIL: post-restore FLAG_ALPHA not set\n");
            fail = 1;
          }
          else if (tam.function != ITM_FORTH) {
            printf("    [4] FAIL: post-restore tam.function = %d\n", (int)tam.function);
            fail = 1;
          }
          else if (strcmp(forthTestCapText(), "2") != 0) {
            printf("    [4] FAIL: post-restore cap text = '%s'\n", forthTestCapText());
            fail = 1;
          }
          else {
            printf("    [4] PASS: power-off round-trip re-derives Forth capture at source step\n");
          }
        }
      }

      if (getSystemFlag(FLAG_ALPHA)) { pemAlpha(ITM_ENTER); }
      forthCapClose();
    }
    cleanupTestProgram();
  }

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  showSoftmenu(savedMenu);
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  return fail;
 }

 /* accept_copy_screen_rect
  * Copies a rectangular region from the calculator LCD buffer into a linear
  * one-byte-per-pixel buffer. No rendering, no global mutation. */
 static void accept_copy_screen_rect(uint8_t *dst, int x, int y, int width, int height)
 {
   int row, col;
   for (row = 0; row < height; row++) {
     for (col = 0; col < width; col++) {
       dst[row * width + col] = lcd_buffer_pixel_on(x + col, y + row) ? 1 : 0;
     }
   }
 }

 /* accept_marker_token_is
  * Returns true only when tmpString holds the exact seven-byte NUL-terminated
  * internal marker token: opening = \x80\xbb"FORTH", closing = "FORTH"\x80\xab. */
 static bool_t accept_marker_token_is(bool_t opening)
 {
   if (strlen(tmpString) != 7) return false;
   if (opening) {
     return (tmpString[0] == (char)0x80 && tmpString[1] == (char)0xBB &&
             memcmp(tmpString + 2, "FORTH", 5) == 0);
   } else {
     return (memcmp(tmpString, "FORTH", 5) == 0 &&
             tmpString[5] == (char)0x80 && tmpString[6] == (char)0xAB);
   }
 }

 /* test_accept_display_parity
  * F15-3: §8.9 item 4 — one real program renders the same marker directions
  * on PEM, SST, and BST surfaces. Three independently accumulated subcases. */
 static int test_accept_display_parity(void)
 {
   int fail = 0;

   /* Save all display/program globals the three drives touch */
   uint8_t *savedCurrentStep = currentStep;
   uint16_t savedProgNum = currentProgramNumber;
   uint16_t savedLocalStep = currentLocalStepNumber;
   uint8_t *savedFirstDisplayedStep = firstDisplayedStep;
   uint16_t savedFirstDisplayedLocal = firstDisplayedLocalStepNumber;
   bool_t savedZeroth = pemCursorIsZerothStep;
   uint8_t savedCalcMode = calcMode;
   int16_t savedScreenUpdatingMode = screenUpdatingMode;
   uint8_t savedProgRunStop = programRunStop;
   int16_t savedTempInfo = temporaryInformation;
   uint16_t savedCurrentInputVar = currentInputVariable;
   bool_t savedProgramListEnd = programListEnd;
   bool_t savedLastProgramListEnd = lastProgramListEnd;
   int16_t savedTamMode = tam.mode;
   int16_t savedTamFunction = tam.function;
   bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
   char aimBufSave[256];
   memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

   extern void fnGotoDot(uint16_t globalStepNumber);
   extern void defineFirstDisplayedStep(void);
   extern void fnPem(uint16_t param);
   extern void fnSst(uint16_t param);
   extern void fnBst(uint16_t param);
   extern int16_t stringWidth(const char *str, const font_t *font, bool_t withLeading, bool_t withEnding);
   extern uint32_t showString(const char *string, const font_t *font, uint32_t x, uint32_t y, videoMode_t videoMode, bool_t showLeading, bool_t showEnding);

   /* Exact fixture — 35 bytes, 6 steps */
   uint8_t prog[] = {
     0x01, 0xFD, 0x03, 'D', '3', 'A',                       /* 1: LBL 'D3A'       (+0)  */
     0x8B, 0x1A, 0xFD, 0x00,                               /* 2: opening »FORTH  (+6)  */
     0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',       /* 3: : SQ DUP * ;    (+10) */
     'D', 'U', 'P', ' ', '*', ' ', ';',
     0x8B, 0x1A, 0xFD, 0x00,                               /* 4: closing FORTH«  (+26) */
     0x8B, 0x1A, 0xFD, 0x00,                               /* 5: opening »FORTH  (+30) */
     0x04                                                   /* 6: RTN             (+34) */
   };

   const char opening[] = STD_RIGHT_DOUBLE_ANGLE "FORTH";
   const char closing[] = "FORTH" STD_LEFT_DOUBLE_ANGLE;
   const int x = 62;
   const int rowPitch = 21;
   const int marker1Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 2;
   const int marker2Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 4;
   const int marker3Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 5;
   const int rectHeight = 21;

   int openingW = stringWidth(opening, &standardFont, false, false);
   int closingW = stringWidth(closing, &standardFont, false, false);

   if (openingW <= 0 || closingW <= 0) {
     printf("    FAIL: stringWidth returned non-positive (opening=%d, closing=%d)\n", openingW, closingW);
     fail = 1;
     goto restore_exit;
   }
   if (x + openingW > SCREEN_WIDTH || x + closingW > SCREEN_WIDTH) {
     printf("    FAIL: marker text exceeds LCD width\n");
     fail = 1;
     goto restore_exit;
   }

   int maxW = (openingW > closingW) ? openingW : closingW;

   /* Allocate expected rectangles and one actual scratch rectangle */
   uint8_t *exp1 = malloc((size_t)openingW * rectHeight);
   uint8_t *exp2 = malloc((size_t)closingW * rectHeight);
   uint8_t *exp3 = malloc((size_t)openingW * rectHeight);
   uint8_t *actual = malloc((size_t)maxW * rectHeight);

   if (!exp1 || !exp2 || !exp3 || !actual) {
     printf("    FAIL: malloc for pixel rectangles\n");
     fail = 1;
     free(exp1); free(exp2); free(exp3); free(actual);
     goto restore_exit;
   }

   /* ---- Render expected tokens into the real calculator LCD buffer ---- */
   /* Opening at marker1Y */
   {
     clearScreen(0);
     showString(opening, &standardFont, x, marker1Y, vmNormal, false, false);
     accept_copy_screen_rect(exp1, x, marker1Y, openingW, rectHeight);
   }
   /* Closing at marker2Y */
   {
     clearScreen(0);
     showString(closing, &standardFont, x, marker2Y, vmNormal, false, false);
     accept_copy_screen_rect(exp2, x, marker2Y, closingW, rectHeight);
   }
   /* Opening at marker3Y */
   {
     clearScreen(0);
     showString(opening, &standardFont, x, marker3Y, vmNormal, false, false);
     accept_copy_screen_rect(exp3, x, marker3Y, openingW, rectHeight);
   }

   /* ---- Subcase 1: real PEM listing ---- */
   {
     int sc1 = 0;

     clearScreen(0);

     if (!writeTestProgram(prog, sizeof(prog))) {
       printf("    [1] FAIL: writeTestProgram\n");
       sc1 = 1;
     }
     else {
       calcMode = CM_PEM;
       tam.mode = 0;
       tam.function = 0;
       aimBuffer[0] = 0;
       clearSystemFlag(FLAG_ALPHA);
       pemCursorIsZerothStep = false;
       programRunStop = PGM_STOPPED;
       lastErrorCode = ERROR_NONE;

       fnGotoDot(2);

       if (currentLocalStepNumber != 2) {
         printf("    [1] FAIL: after fnGotoDot(2) step=%u, expected 2\n", currentLocalStepNumber);
         sc1 = 1;
       }
       else if (currentStep != beginOfProgramMemory + 6) {
         printf("    [1] FAIL: after fnGotoDot(2) currentStep=%p, expected %p (+6)\n",
                (void *)currentStep, (void *)(beginOfProgramMemory + 6));
         sc1 = 1;
       }
       else {
         firstDisplayedLocalStepNumber = 0;
         defineFirstDisplayedStep();
         fnPem(NOPARAM);

         /* Copy actual marker rectangles */
         accept_copy_screen_rect(actual, x, marker1Y, openingW, rectHeight);
         if (memcmp(actual, exp1, (size_t)openingW * rectHeight) != 0) {
           printf("    [1] FAIL: PEM marker 1 pixel mismatch (expected opening)\n");
           sc1 = 1;
         }

         accept_copy_screen_rect(actual, x, marker2Y, closingW, rectHeight);
         if (memcmp(actual, exp2, (size_t)closingW * rectHeight) != 0) {
           printf("    [1] FAIL: PEM marker 2 pixel mismatch (expected closing)\n");
           sc1 = 1;
         }

         accept_copy_screen_rect(actual, x, marker3Y, openingW, rectHeight);
         if (memcmp(actual, exp3, (size_t)openingW * rectHeight) != 0) {
           printf("    [1] FAIL: PEM marker 3 pixel mismatch (expected opening)\n");
           sc1 = 1;
         }
       }
     }
     cleanupTestProgram();

     if (sc1) {
       fail = 1;
     }
     else {
       printf("    [1] PASS: PEM listing renders opening/closing/opening markers\n");
     }
   }

   /* ---- Subcase 2: real SST display ---- */
   {
     int sc2 = 0;
     const uint16_t steps[] = { 2, 4, 5 };
     const uint16_t offsets[] = { 6, 26, 30 };
     const bool_t openingExpected[] = { true, false, true };
     int i;

     if (!writeTestProgram(prog, sizeof(prog))) {
       printf("    [2] FAIL: writeTestProgram\n");
       sc2 = 1;
     }
     else {
       for (i = 0; i < 3; i++) {
         calcMode = CM_NORMAL;
         programRunStop = PGM_STOPPED;
         lastErrorCode = ERROR_NONE;
         dynamicMenuItem = -1;
         tam.mode = 0;
         aimBuffer[0] = 0;
         clearSystemFlag(FLAG_ALPHA);

         fnGotoDot(steps[i]);

         if (currentLocalStepNumber != steps[i]) {
           printf("    [2] FAIL: step %d: localStep=%u, expected %u\n",
                  i, currentLocalStepNumber, steps[i]);
           sc2 = 1;
         }
         else if (currentStep != beginOfProgramMemory + offsets[i]) {
           printf("    [2] FAIL: step %d: currentStep=%p, expected %p (+%d)\n",
                  i, (void *)currentStep, (void *)(beginOfProgramMemory + offsets[i]), offsets[i]);
           sc2 = 1;
         }
         else {
           fnSst(NOPARAM);

           if (lastErrorCode != ERROR_NONE) {
             printf("    [2] FAIL: step %d: lastErrorCode=%d\n", i, lastErrorCode);
             sc2 = 1;
           }
           else if (!accept_marker_token_is(openingExpected[i])) {
             printf("    [2] FAIL: step %d: token mismatch (expected %s)\n",
                    i, openingExpected[i] ? "opening" : "closing");
             sc2 = 1;
           }
         }

         programRunStop = PGM_STOPPED;
       }
     }
     cleanupTestProgram();

     if (sc2) {
       fail = 1;
     }
     else {
       printf("    [2] PASS: SST display matches PEM marker directions\n");
     }
   }

   /* ---- Subcase 3: real BST display ---- */
   {
     int sc3 = 0;
     const uint16_t startSteps[] = { 3, 5, 6 };
     const uint16_t startOffsets[] = { 10, 30, 34 };
     const uint16_t targetSteps[] = { 2, 4, 5 };
     const uint16_t targetOffsets[] = { 6, 26, 30 };
     const bool_t openingExpected[] = { true, false, true };
     int i;

     if (!writeTestProgram(prog, sizeof(prog))) {
       printf("    [3] FAIL: writeTestProgram\n");
       sc3 = 1;
     }
     else {
       for (i = 0; i < 3; i++) {
         calcMode = CM_NORMAL;
         programRunStop = PGM_STOPPED;
         lastErrorCode = ERROR_NONE;
         dynamicMenuItem = -1;
         tam.mode = 0;
         aimBuffer[0] = 0;
         clearSystemFlag(FLAG_ALPHA);

         fnGotoDot(startSteps[i]);

         if (currentLocalStepNumber != startSteps[i]) {
           printf("    [3] FAIL: element %d: start localStep=%u, expected %u\n",
                  i, currentLocalStepNumber, startSteps[i]);
           sc3 = 1;
         }
         else if (currentStep != beginOfProgramMemory + startOffsets[i]) {
           printf("    [3] FAIL: element %d: start currentStep=%p, expected %p (+%d)\n",
                  i, (void *)currentStep, (void *)(beginOfProgramMemory + startOffsets[i]), startOffsets[i]);
           sc3 = 1;
         }
         else {
           fnBst(NOPARAM);

           if (lastErrorCode != ERROR_NONE) {
             printf("    [3] FAIL: element %d: lastErrorCode=%d\n", i, lastErrorCode);
             sc3 = 1;
           }
           else if (currentLocalStepNumber != targetSteps[i]) {
             printf("    [3] FAIL: element %d: target localStep=%u, expected %u\n",
                    i, currentLocalStepNumber, targetSteps[i]);
             sc3 = 1;
           }
           else if (currentStep != beginOfProgramMemory + targetOffsets[i]) {
             printf("    [3] FAIL: element %d: target currentStep=%p, expected %p (+%d)\n",
                    i, (void *)currentStep, (void *)(beginOfProgramMemory + targetOffsets[i]), targetOffsets[i]);
             sc3 = 1;
           }
           else if (!accept_marker_token_is(openingExpected[i])) {
             printf("    [3] FAIL: element %d: token mismatch (expected %s)\n",
                    i, openingExpected[i] ? "opening" : "closing");
             sc3 = 1;
           }
         }
       }
     }
     cleanupTestProgram();

     if (sc3) {
       fail = 1;
     }
     else {
       printf("    [3] PASS: BST display matches PEM marker directions\n");
     }
   }

 free_rects:
   free(exp1); free(exp2); free(exp3); free(actual);
 restore_exit:
   currentStep = savedCurrentStep;
   currentProgramNumber = savedProgNum;
   currentLocalStepNumber = savedLocalStep;
   firstDisplayedStep = savedFirstDisplayedStep;
   firstDisplayedLocalStepNumber = savedFirstDisplayedLocal;
   pemCursorIsZerothStep = savedZeroth;
   calcMode = savedCalcMode;
   screenUpdatingMode = savedScreenUpdatingMode;
   programRunStop = savedProgRunStop;
   temporaryInformation = savedTempInfo;
   currentInputVariable = savedCurrentInputVar;
   programListEnd = savedProgramListEnd;
   lastProgramListEnd = savedLastProgramListEnd;
   tam.mode = savedTamMode;
   tam.function = savedTamFunction;
   if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
   memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

    return fail;
  }

  /* test_accept_glyph_type_parity
   * F15-4: §8.9 items 5 and 6 — alpha-authored glyphs compile and run,
   * and RPN-keypad / Forth-source literals share a data type.
   * Three independently accumulated subcases. */
  static int test_accept_glyph_type_parity(void)
  {
    int fail = 0;

    /* Save all program/input globals */
    uint8_t *savedCurrentStep = currentStep;
    uint16_t savedProgNum = currentProgramNumber;
    uint16_t savedLocalStep = currentLocalStepNumber;
    uint8_t *savedFirstDisplayedStep = firstDisplayedStep;
    uint16_t savedFirstDisplayedLocal = firstDisplayedLocalStepNumber;
    bool_t savedZeroth = pemCursorIsZerothStep;
    uint8_t savedCalcMode = calcMode;
    int16_t savedCatalog = catalog;
    int16_t savedTamMode = tam.mode;
    int16_t savedTamFunction = tam.function;
    bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
    bool_t savedNumin = getSystemFlag(FLAG_NUMIN);
    bool_t savedNumlock = getSystemFlag(FLAG_NUMLOCK);
    uint8_t savedAlphaCase = alphaCase;
    uint8_t savedNextChar = nextChar;
    bool_t savedShiftF = shiftF;
    bool_t savedShiftG = shiftG;
    uint8_t savedProgRunStop = programRunStop;
    int16_t savedDynamicMenuItem = dynamicMenuItem;
    uint8_t savedInputDefault = Input_Default;
    uint8_t savedNimNumberPart = nimNumberPart;
    uint32_t savedLastIntegerBase = lastIntegerBase;
    int16_t savedTCursorPos = T_cursorPos;
    int16_t savedMenu = currentMenu();
    char aimBufSave[256];
    memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

    extern void fnGotoDot(uint16_t globalStepNumber);
    extern void runFunction(int16_t func);
    extern void addItemToNimBuffer(int16_t item);
    extern void closeNim(void);
    extern void setLastintegerBasetoZero(void);

    /* ---- Subcase 1: §8.9 item 5, alpha-authored cross ---- */
    {
      int sc1 = 0;

      uint8_t seedM[] = {
        0x01, 0xFD, 0x03, 'G', '4', 'M',
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x00,
        0x04
      };

      uint8_t expected[] = {
        0x01, 0xFD, 0x03, 'G', '4', 'M',
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x0B,
        ':', ' ', 'M', '2', ' ', '2', ' ', 0x80, 0xD7, ' ', ';',
        0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'M', '2',
        0x8B, 0x1A, 0xFD, 0x00,
        0x04
      };

      const int16_t defItems[] = {
        ITM_COLON, ITM_SPACE, ITM_M, ITM_2, ITM_SPACE, ITM_2,
        ITM_SPACE, ITM_CROSS, ITM_SPACE, ITM_SEMICOLON, ITM_ENTER
      };

      const int16_t useItems[] = {
        ITM_3, ITM_SPACE, ITM_M, ITM_2, ITM_ENTER, ITM_ENTER
      };

      if (!writeTestProgram(seedM, sizeof(seedM))) {
        printf("    [1] FAIL: writeTestProgram\n");
        sc1 = 1;
      }
      else {
        calcMode = CM_PEM;
        catalog = CATALOG_NONE;
        tam.mode = 0;
        tam.function = 0;
        aimBuffer[0] = 0;
        programRunStop = PGM_STOPPED;
        dynamicMenuItem = -1;
        pemCursorIsZerothStep = false;
        alphaCase = AC_UPPER;
        nextChar = NC_NORMAL;
        shiftF = false;
        shiftG = false;
        clearSystemFlag(FLAG_ALPHA);
        clearSystemFlag(FLAG_NUMLOCK);

        /* Cursor ON the OPENING marker (step 2): the ALPHA route pre-moves
         * one step (addStepInProgram) before insertStepInProgram consults
         * forthEntryStateAtInsertion, so the governing predecessor is the
         * »FORTH marker itself and capture opens as Forth. A first key of
         * ':' or a digit consults the bridge WITHOUT the pre-move (LBL
         * predecessor → RPN → number entry), which is exactly the failure
         * this fixture originally hit. */
        fnGotoDot(2);

        if (currentStep != beginOfProgramMemory + 6) {
          printf("    [1] FAIL: currentStep after fnGotoDot(2) = %p, expected %p\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 6));
          sc1 = 1;
        }
        else if (currentLocalStepNumber != 2) {
          printf("    [1] FAIL: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
          sc1 = 1;
        }
        else {
          int i;
          /* Open Forth capture with the ALPHA gesture (the sanctioned route
           * pinned by test_forth_alpha_gesture_resumes_forth) BEFORE any
           * text key. */
          runFunction(ITM_AIM);
          if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
            printf("    [1] FAIL: ALPHA gesture did not open Forth capture (alpha=%d tam.function=%d)\n",
                   (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
            sc1 = 1;
          }
          for (i = 0; sc1 == 0 && i < (int)(sizeof(defItems) / sizeof(defItems[0])); i++) {
            runFunction(defItems[i]);
          }
          for (i = 0; sc1 == 0 && i < (int)(sizeof(useItems) / sizeof(useItems[0])); i++) {
            runFunction(useItems[i]);
          }

          if (sc1 == 0 && memcmp(beginOfProgramMemory, expected, sizeof(expected)) != 0) {
            printf("    [1] FAIL: program bytes mismatch (size %u vs expected %u)\n",
                   (uint16_t)(firstFreeProgramByte - beginOfProgramMemory), (uint16_t)sizeof(expected));
            sc1 = 1;
          }
          else if (sc1 == 0) {
            calcRegister_t lbl = findNamedLabel("G4M", GLOBAL_LABELS);
            if (lbl == INVALID_VARIABLE) {
              printf("    [1] FAIL: findNamedLabel(\"G4M\") returned INVALID_VARIABLE\n");
              sc1 = 1;
            }
            else {
              lastErrorCode = ERROR_NONE;
              dynamicMenuItem = -1;
              programRunStop = PGM_STOPPED;
              fnExecute(lbl);

              if (lastErrorCode != ERROR_NONE) {
                printf("    [1] FAIL: fnExecute error %d\n", lastErrorCode);
                sc1 = 1;
              }
              else if (!x_is_longint(6)) {
                printf("    [1] FAIL: X != 6 (type=%d)\n", getRegisterDataType(REGISTER_X));
                sc1 = 1;
              }
            }
          }
        }
      }

      cleanupTestProgram();
      forthDictClear();

      if (sc1) {
        fail = 1;
      }
      else {
        printf("    [1] PASS: alpha-authored cross bytes run 3 M2 -> 6\n");
      }
    }

    /* ---- Subcase 2: §8.9 item 5, alpha-authored divide (design fixture) ---- */
    {
      int sc2 = 0;

      uint8_t seedD[] = {
        0x01, 0xFD, 0x03, 'G', '4', 'D',
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x00,
        0x04
      };

      uint8_t expected[] = {
        0x01, 0xFD, 0x03, 'G', '4', 'D',
        0x8B, 0x1A, 0xFD, 0x00,
        0x8B, 0x1A, 0xFD, 0x0B,
        ':', ' ', 'D', '2', ' ', '2', ' ', 0x80, 0xF7, ' ', ';',
        0x8B, 0x1A, 0xFD, 0x04, '8', ' ', 'D', '2',
        0x8B, 0x1A, 0xFD, 0x00,
        0x04
      };

      const int16_t defItems[] = {
        ITM_COLON, ITM_SPACE, ITM_D, ITM_2, ITM_SPACE, ITM_2,
        ITM_SPACE, ITM_OBELUS, ITM_SPACE, ITM_SEMICOLON, ITM_ENTER
      };

      const int16_t useItems[] = {
        ITM_8, ITM_SPACE, ITM_D, ITM_2, ITM_ENTER, ITM_ENTER
      };

      if (!writeTestProgram(seedD, sizeof(seedD))) {
        printf("    [2] FAIL: writeTestProgram\n");
        sc2 = 1;
      }
      else {
        calcMode = CM_PEM;
        catalog = CATALOG_NONE;
        tam.mode = 0;
        tam.function = 0;
        aimBuffer[0] = 0;
        programRunStop = PGM_STOPPED;
        dynamicMenuItem = -1;
        pemCursorIsZerothStep = false;
        alphaCase = AC_UPPER;
        nextChar = NC_NORMAL;
        shiftF = false;
        shiftG = false;
        clearSystemFlag(FLAG_ALPHA);
        clearSystemFlag(FLAG_NUMLOCK);

        /* Same contract as subcase 1: cursor ON the opening marker; the
         * ALPHA route's pre-move makes »FORTH the governing predecessor. */
        fnGotoDot(2);

        if (currentStep != beginOfProgramMemory + 6) {
          printf("    [2] FAIL: currentStep after fnGotoDot(2) = %p, expected %p\n",
                 (void *)currentStep, (void *)(beginOfProgramMemory + 6));
          sc2 = 1;
        }
        else if (currentLocalStepNumber != 2) {
          printf("    [2] FAIL: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
          sc2 = 1;
        }
        else {
          int i;
          runFunction(ITM_AIM);
          if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
            printf("    [2] FAIL: ALPHA gesture did not open Forth capture (alpha=%d tam.function=%d)\n",
                   (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
            sc2 = 1;
          }
          for (i = 0; sc2 == 0 && i < (int)(sizeof(defItems) / sizeof(defItems[0])); i++) {
            runFunction(defItems[i]);
          }
          for (i = 0; sc2 == 0 && i < (int)(sizeof(useItems) / sizeof(useItems[0])); i++) {
            runFunction(useItems[i]);
          }

          if (sc2 == 0 && memcmp(beginOfProgramMemory, expected, sizeof(expected)) != 0) {
            printf("    [2] FAIL: program bytes mismatch (size %u vs expected %u)\n",
                   (uint16_t)(firstFreeProgramByte - beginOfProgramMemory), (uint16_t)sizeof(expected));
            sc2 = 1;
          }
          else if (sc2 == 0) {
            calcRegister_t lbl = findNamedLabel("G4D", GLOBAL_LABELS);
            if (lbl == INVALID_VARIABLE) {
              printf("    [2] FAIL: findNamedLabel(\"G4D\") returned INVALID_VARIABLE\n");
              sc2 = 1;
            }
            else {
              lastErrorCode = ERROR_NONE;
              dynamicMenuItem = -1;
              programRunStop = PGM_STOPPED;
              fnExecute(lbl);

              if (lastErrorCode != ERROR_NONE) {
                printf("    [2] FAIL: fnExecute error %d\n", lastErrorCode);
                sc2 = 1;
              }
              else if (!x_is_longint(4)) {
                printf("    [2] FAIL: X != 4 (type=%d)\n", getRegisterDataType(REGISTER_X));
                sc2 = 1;
              }
            }
          }
        }
      }

      cleanupTestProgram();
      forthDictClear();

      if (sc2) {
        fail = 1;
      }
      else {
        printf("    [2] PASS: alpha-authored divide bytes run 8 D2 -> 4\n");
      }
    }

    /* ---- Subcase 3: §8.9 item 6, RPN-keypad / Forth-source type parity ---- */
    {
      int sc3 = 0;

      /* RPN half: addItemToNimBuffer(ITM_7) + closeNim() */
      {
        calcMode = CM_NORMAL;
        Input_Default = ID_LI;
        nimNumberPart = NP_EMPTY;
        aimBuffer[0] = 0;
        setLastintegerBasetoZero();
        lastErrorCode = ERROR_NONE;

        addItemToNimBuffer(ITM_7);
        closeNim();

        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: RPN half error %d\n", lastErrorCode);
          sc3 = 1;
        }
        else if (!x_is_longint(7)) {
          printf("    [3] FAIL: RPN half X != 7 (type=%d)\n", getRegisterDataType(REGISTER_X));
          sc3 = 1;
        }
      }

      /* Seed X as dtReal34 value 42 so Forth half cannot inherit RPN result */
      {
        real34_t seed;
        int32ToReal34(42, &seed);
        reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
        real34Copy(&seed, REGISTER_REAL34_DATA(REGISTER_X));
      }

      /* Forth half: run program with source step "7" */
      {
        uint8_t prog[] = {
          0x01, 0xFD, 0x03, 'T', '4', 'A',
          0x8B, 0x1A, 0xFD, 0x00,
          0x8B, 0x1A, 0xFD, 0x01, '7',
          0x8B, 0x1A, 0xFD, 0x00,
          0x04
        };

        if (!writeTestProgram(prog, sizeof(prog))) {
          printf("    [3] FAIL: writeTestProgram (Forth half)\n");
          sc3 = 1;
        }
        else {
          calcRegister_t lbl = findNamedLabel("T4A", GLOBAL_LABELS);
          if (lbl == INVALID_VARIABLE) {
            printf("    [3] FAIL: findNamedLabel(\"T4A\") returned INVALID_VARIABLE\n");
            sc3 = 1;
          }
          else {
            lastErrorCode = ERROR_NONE;
            dynamicMenuItem = -1;
            programRunStop = PGM_STOPPED;
            fnExecute(lbl);

            if (lastErrorCode != ERROR_NONE) {
              printf("    [3] FAIL: Forth half fnExecute error %d\n", lastErrorCode);
              sc3 = 1;
            }
            else if (!x_is_longint(7)) {
              printf("    [3] FAIL: Forth half X != 7 (type=%d)\n", getRegisterDataType(REGISTER_X));
              sc3 = 1;
            }
          }
        }
        cleanupTestProgram();
        forthDictClear();
      }

      if (sc3) {
        fail = 1;
      }
      else {
        printf("    [3] PASS: RPN-keypad 7 and Forth-source 7 both leave dtLongInteger\n");
      }
    }

  restore_exit:
    currentStep = savedCurrentStep;
    currentProgramNumber = savedProgNum;
    currentLocalStepNumber = savedLocalStep;
    firstDisplayedStep = savedFirstDisplayedStep;
    firstDisplayedLocalStepNumber = savedFirstDisplayedLocal;
    pemCursorIsZerothStep = savedZeroth;
    calcMode = savedCalcMode;
    catalog = savedCatalog;
    tam.mode = savedTamMode;
    tam.function = savedTamFunction;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    if (savedNumin) setSystemFlag(FLAG_NUMIN); else clearSystemFlag(FLAG_NUMIN);
    if (savedNumlock) setSystemFlag(FLAG_NUMLOCK); else clearSystemFlag(FLAG_NUMLOCK);
    alphaCase = savedAlphaCase;
    nextChar = savedNextChar;
    shiftF = savedShiftF;
    shiftG = savedShiftG;
    programRunStop = savedProgRunStop;
    dynamicMenuItem = savedDynamicMenuItem;
    Input_Default = savedInputDefault;
    nimNumberPart = savedNimNumberPart;
    lastIntegerBase = savedLastIntegerBase;
    T_cursorPos = savedTCursorPos;
    showSoftmenu(savedMenu);
    memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

    return fail;
  }

  /* test_dict_name_by_ref
 * Mutation: off-by-one in count-1-n walk (returns wrong word's name). */
static int test_dict_name_by_index(void)
{
  char namebuf[FORTH_NAME_MAX + 1];
  uint16_t idx;

  lastErrorCode = ERROR_NONE;
  x_set_string(": ALPHA 1 + ;");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: define ALPHA error %d\n", lastErrorCode);
    return 1;
  }

  lastErrorCode = ERROR_NONE;
  x_set_string(": BETA 2 + ;");
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: define BETA error %d\n", lastErrorCode);
    return 1;
  }

  if (!forthFindColon("ALPHA", &idx)) {
    printf("    FAIL: ALPHA not found\n");
    return 1;
  }
  if (!forthFindColon("BETA", &idx)) {
    printf("    FAIL: BETA not found\n");
    return 1;
  }

  /* ALPHA is at transient ref 0 (oldest), BETA at ref 1 (newest) */
  if (!forthDictNameByRef(0, namebuf, sizeof(namebuf))) {
    printf("    FAIL: ref 0 not found\n");
    return 1;
  }
  if (strcmp(namebuf, "ALPHA") != 0) {
    printf("    FAIL: ref 0 is '%s', expected 'ALPHA'\n", namebuf);
    return 1;
  }

  if (!forthDictNameByRef(1, namebuf, sizeof(namebuf))) {
    printf("    FAIL: ref 1 not found\n");
    return 1;
  }
  if (strcmp(namebuf, "BETA") != 0) {
    printf("    FAIL: ref 1 is '%s', expected 'BETA'\n", namebuf);
    return 1;
  }

  if (forthDictNameByRef(fdict.count, namebuf, sizeof(namebuf))) {
    printf("    FAIL: ref == count should return false\n");
    return 1;
  }

  printf("    PASS: forthDictNameByRef round-trips, count is out of range\n");
  return 0;
}

/* test_accept_xeq_name_step
 * F15-5: §8.9 item 10 — In PEM, XEQ + alpha name of a Forth word records
 * the NAME (not an index), re-resolved at run time. Two independently
 * reported subcases. */
static int test_accept_xeq_name_step(void)
{
  uint8_t savedCalcMode = calcMode;
  uint8_t savedLastError = lastErrorCode;
  uint8_t savedRunStop = programRunStop;
  char savedAimBuffer[AIM_BUFFER_LENGTH];
  memcpy(savedAimBuffer, aimBuffer, sizeof(savedAimBuffer));
  tamState_t savedTam = tam;
  int fail = 0;

  extern void runFunction(int16_t func);
  extern void fnGotoDot(uint16_t globalStepNumber);

  /* Setup: fixture program (LBL 'F5E' + RTN) */
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '5', 'E',   /* LBL 'F5E' */
    0x04                               /* RTN       */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    fail = 1;
    goto cleanup;
  }

  /* Define the Forth word interactively */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SQ DUP * ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: forthOuterInterpret error %d\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }

  {
    uint16_t idx;
    if (!forthFindColon("SQ", &idx)) {
      printf("    FAIL: SQ not found in dictionary\n");
      fail = 1;
      goto cleanup;
    }
  }

  /* PEM fixture state */
  programRunStop = PGM_STOPPED;
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  lastErrorCode = ERROR_NONE;
  clearSystemFlag(FLAG_ALPHA);

  /* Position cursor on the LBL step */
  fnGotoDot(1);
  if (currentLocalStepNumber != 1) {
    printf("    FAIL: currentLocalStepNumber = %u, expected 1\n", currentLocalStepNumber);
    fail = 1;
    goto cleanup;
  }

  /* Drive: real public TAM chain */
  tamEnterMode(ITM_XEQ);
  tamProcessInput(ITM_alpha);
  runFunction(ITM_S);
  runFunction(ITM_Q);

  if (strcmp(aimBuffer, "SQ") != 0) {
    printf("    FAIL: aimBuffer = '%s', expected 'SQ'\n", aimBuffer);
    fail = 1;
    goto cleanup;
  }

  tamProcessInput(ITM_ENTER);

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: lastErrorCode = %d after commit\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }

  /* Subcase 1: The name step was recorded */
  {
    const uint8_t nameStep[] = {0x03, 0xFD, 0x02, 'S', 'Q'};
    uint8_t *p = beginOfProgramMemory;
    uint8_t *end = firstFreeProgramByte;
    int matches = 0;

    while (p + sizeof(nameStep) <= end) {
      if (memcmp(p, nameStep, sizeof(nameStep)) == 0) {
        matches++;
      }
      p++;
    }

    if (matches != 1) {
      printf("    FAIL(subcase1): expected exactly 1 name-step match, got %d\n", matches);
      fail = 1;
    } else {
      printf("    PASS(subcase1): XEQ name step recorded: 0x03 0xFD 0x02 'S' 'Q'\n");
    }
  }

  /* Subcase 2: No ITM_FCALL opcode, no index */
  {
    const uint8_t fcallOpc[] = {0x8B, 0x1B};
    uint8_t *p = beginOfProgramMemory;
    uint8_t *end = firstFreeProgramByte;
    int matches = 0;

    while (p + sizeof(fcallOpc) <= end) {
      if (memcmp(p, fcallOpc, sizeof(fcallOpc)) == 0) {
        matches++;
      }
      p++;
    }

    if (matches != 0) {
      printf("    FAIL(subcase2): expected 0 FCALL opcode matches, got %d\n", matches);
      fail = 1;
    } else {
      printf("    PASS(subcase2): no ITM_FCALL opcode (0x8B 0x1B) in program memory\n");
    }
  }

cleanup:
  tam = savedTam;
  memcpy(aimBuffer, savedAimBuffer, sizeof(savedAimBuffer));
  calcMode = savedCalcMode;
  programRunStop = savedRunStop;
  lastErrorCode = savedLastError;
  forthDictClear();
  cleanupTestProgram();
  return fail;
}

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

/* test_exec_step_marker_noop
 * Mutation: the arm calling forthProgramStep for len==0 too (the marker
 * would interpret an empty line and set FLAG_ASLIFT/N drop state).
 * (§8.9 acceptance 8a)
 * R2-T4 item 4: the old test read only X and fdict.count. Seeds all four RPN
 * registers with distinct values and also checks fdict.here, so a marker that
 * silently touches Y/Z/T or grows the dict without moving X or count would
 * still be caught. len==0 never reaches forthProgramStep (the arm's guard is
 * `if(*step != 0)`), so no writeTestProgram/program membership is needed
 * here — this is testing that the guard itself holds, not program execution. */
static int test_exec_step_marker_noop(void)
{
  uint8_t step[] = { 0x8B, 0x1A, 0xFD, 0x00 }; /* ITM_FORTH, STRING_LABEL_VARIABLE, len=0 */

  forthPushInt32(11);
  forthPushInt32(22);
  forthPushInt32(33);
  forthPushInt32(44);  /* stack: T=11 Z=22 Y=33 X=44 */

  uint8_t typeXb, typeYb, typeZb, typeTb;
  int32_t xB, yB, zB, tB;
  read_reg_int32(REGISTER_X, &typeXb, &xB);
  read_reg_int32(REGISTER_Y, &typeYb, &yB);
  read_reg_int32(REGISTER_Z, &typeZb, &zB);
  read_reg_int32(REGISTER_T, &typeTb, &tB);

  uint16_t countBefore = fdict.count;
  uint16_t hereBefore  = fdict.here;
  lastErrorCode = ERROR_NONE;

  int16_t ret = executeOneStep(step);

  int fail = 0;
  if (ret != 1) {
    printf("    FAIL: executeOneStep returned %d (expected 1)\n", ret);
    fail = 1;
  }
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_NONE)\n", lastErrorCode);
    fail = 1;
  }

  uint8_t typeXa, typeYa, typeZa, typeTa;
  int32_t xA, yA, zA, tA;
  read_reg_int32(REGISTER_X, &typeXa, &xA);
  read_reg_int32(REGISTER_Y, &typeYa, &yA);
  read_reg_int32(REGISTER_Z, &typeZa, &zA);
  read_reg_int32(REGISTER_T, &typeTa, &tA);

  if (typeXa != typeXb || xA != xB) {
    printf("    FAIL: X changed (type %u->%u, val %d->%d)\n", typeXb, typeXa, xB, xA);
    fail = 1;
  }
  if (typeYa != typeYb || yA != yB) {
    printf("    FAIL: Y changed (type %u->%u, val %d->%d)\n", typeYb, typeYa, yB, yA);
    fail = 1;
  }
  if (typeZa != typeZb || zA != zB) {
    printf("    FAIL: Z changed (type %u->%u, val %d->%d)\n", typeZb, typeZa, zB, zA);
    fail = 1;
  }
  if (typeTa != typeTb || tA != tB) {
    printf("    FAIL: T changed (type %u->%u, val %d->%d)\n", typeTb, typeTa, tB, tA);
    fail = 1;
  }
  if (fdict.count != countBefore) {
    printf("    FAIL: fdict.count changed from %u to %u\n", countBefore, fdict.count);
    fail = 1;
  }
  if (fdict.here != hereBefore) {
    printf("    FAIL: fdict.here changed from %u to %u\n", hereBefore, fdict.here);
    fail = 1;
  }
  if (!fail) {
    printf("    PASS: marker (len==0) is a no-op — X/Y/Z/T and dict unchanged\n");
  }
  return fail;
}

/* test_exec_step_source_runs
 * MIGRATED to Architecture 2 (P2 ruling, 2026-07-13): drives the REAL
 * dispatch arm (executeOneStep -> ITM_FORTH -> forthProgramStep) with
 * steps residing in a real program, as the pre-scan contract requires.
 * Unique coverage: the lblGtoXeq.c arm routing, not just forthProgramStep.
 * Mutation: dropping the forthProgramStep call (arm returns 1 silently)
 * (§8.9 acceptance 1 at executeOneStep granularity). */
static int test_exec_step_source_runs(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 12, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 4, '3', ' ', 'S', 'Q'
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;
  executeOneStep(beginOfProgramMemory);        /* define step */
  if (lastErrorCode == ERROR_NONE) {
    /* Canary (R2-T4 item 2): see test_program_step_define_and_use. */
    forthPushInt32(-123456);
    executeOneStep(beginOfProgramMemory + 16); /* "3 SQ" step */
  }
  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: executeOneStep raised error %d\n", lastErrorCode);
    fail = 1;
  }
  else if (getRegisterDataType(REGISTER_X) != dtLongInteger) {
    printf("    FAIL: X is not dtLongInteger (type %u)\n", getRegisterDataType(REGISTER_X));
    fail = 1;
  }
  else if (!x_is_longint(9)) {
    printf("    FAIL: X != 9\n");
    fail = 1;
  }
  forthDictClear();
  cleanupTestProgram();
  if (!fail) printf("    PASS: executeOneStep (real program) : SQ DUP * ; then 3 SQ -> X==9\n");
  return fail;
}

/* test_exec_step_halts_on_error
 * Mutation: the arm clearing lastErrorCode before returning.
 * (§8.9 acceptance 7b's PC-testable half)
 *
 * R2-T4 item 3 / architect ruling 2026-07-15 (FOR_THE_ARCHITECT_R2.md):
 * standalone step execution is a fixture artifact, not a supported API — the
 * P2 ruling of 2026-07-13 already requires forthProgramStep's payload to live
 * inside a real program. The old fixture had two independent defects: its
 * length byte said 4 for the five bytes "3 SQX" (so it silently executed
 * "3 SQ" and never told the interpreter about the X), and its stack-local
 * step buffer resolved through forthOwningProgramStart to a REAL, unrelated
 * program (that function has no upper bound on its `instructionPointer <=
 * ptr` scan), so the pre-scan compiled whatever that program happened to
 * contain before the step ever ran. Rebuilt via writeTestProgram: no SQ
 * definition anywhere, no stack-local step. */
static int test_exec_step_halts_on_error(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 5, '3', ' ', 'S', 'Q', 'X'   /* 3 SQX — SQX undefined, length 5 */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  forthRunGenBump();
  uint8_t savedRS = programRunStop;
  programRunStop = PGM_RUNNING;
  lastErrorCode = ERROR_NONE;

  executeOneStep(beginOfProgramMemory);

  programRunStop = savedRS;

  int fail = 0;
  if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_FUNCTION_NOT_FOUND=%d)\n",
           lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
    fail = 1;
  }
  /* Identify WHICH word was reported missing, not just that something was.
   * With no SQ definition anywhere in this test, a length-4 truncation
   * ("3 SQ" instead of "3 SQX") would raise the identical
   * ERROR_FUNCTION_NOT_FOUND — SQ is exactly as undefined as SQX — so the
   * error code alone cannot catch that mutation. errorMessage carries the
   * offending token (forth_compile.c:409-414); require it names SQX. */
  else if (!strstr(errorMessage, "SQX")) {
    printf("    FAIL: errorMessage = \"%s\" (expected to name SQX — got a "
           "truncated read of the source, e.g. only \"SQ\")\n", errorMessage);
    fail = 1;
  }

  forthDictClear();
  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: executeOneStep on undefined word (real program) sets "
           "ERROR_FUNCTION_NOT_FOUND naming SQX\n");
  }
  return fail;
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

/* test_marker_parity
 * Program: marker, source(: SQ DUP * ;), marker, marker
 * Assert turnsOn == true/false/true for the 1st/3rd/4th markers.
 * Escaping mutation: inverting the parity test (odd instead of even) —
 * every direction flips. (§8.9 acceptance 4 logic) */
static int test_marker_parity(void)
{
  /* marker | source | marker | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 1 */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',               /* source */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 2 */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 3 */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *marker1 = beginOfProgramMemory;
  const uint8_t *marker2 = beginOfProgramMemory + 4 + 16;  /* after marker(4) + source(16) */
  const uint8_t *marker3 = marker2 + 4;

  int fail = 0;

  /* Marker 1: 0 markers before it → even → opening (true) */
  if (!forthMarkerTurnsOn(marker1)) {
    printf("    FAIL: marker 1 should turn on (opening)\n");
    fail = 1;
  }

  /* Marker 2: 1 marker before it (marker1) → odd → closing (false) */
  if (forthMarkerTurnsOn(marker2)) {
    printf("    FAIL: marker 2 should NOT turn on (closing)\n");
    fail = 1;
  }

  /* Marker 3: 2 markers before it (marker1, marker2) → even → opening (true) */
  if (!forthMarkerTurnsOn(marker3)) {
    printf("    FAIL: marker 3 should turn on (opening)\n");
    fail = 1;
  }

  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: marker parity = true/false/true for 1st/3rd/4th\n");
  }
  return fail;
}

/* test_entry_state_derivation
 * Same program + RPN step (ITM_sin) appended inside the region.
 * Point currentStep at each step and assert:
 *   RPN step → false, source step → true, opening marker → true,
 *   closing marker → false, zeroth-step → false.
 * Escaping mutation: replacing the derivation with a static bool toggled
 * by callers (the persisted-flag bug) — the land-on-step cases regress.
 * (§8.9 acceptance 2 logic) */
static int test_entry_state_derivation(void)
{
  /* marker | source | marker | marker | ITM_sin | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 1 (opening) */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',               /* source */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 2 (closing) */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 3 (opening) */
    0x4C,                                                            /* ITM_sin (76) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *marker1 = beginOfProgramMemory;
  const uint8_t *source  = beginOfProgramMemory + 4;
  const uint8_t *marker2 = beginOfProgramMemory + 4 + 16;
  const uint8_t *marker3 = marker2 + 4;
  const uint8_t *rpnStep = marker3 + 4;

  int fail = 0;

  /* Save original cursor state */
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;

  /* RPN step → false */
  currentStep = (uint8_t *)rpnStep;
  pemCursorIsZerothStep = false;
  if (forthEntryStateAtCursor()) {
    printf("    FAIL: RPN step (ITM_sin) should return false\n");
    fail = 1;
  }

  /* Source step → true */
  currentStep = (uint8_t *)source;
  pemCursorIsZerothStep = false;
  if (!forthEntryStateAtCursor()) {
    printf("    FAIL: source step should return true\n");
    fail = 1;
  }

  /* Opening marker → true */
  currentStep = (uint8_t *)marker1;
  pemCursorIsZerothStep = false;
  if (!forthEntryStateAtCursor()) {
    printf("    FAIL: opening marker should return true\n");
    fail = 1;
  }

  /* Closing marker → false */
  currentStep = (uint8_t *)marker2;
  pemCursorIsZerothStep = false;
  if (forthEntryStateAtCursor()) {
    printf("    FAIL: closing marker should return false\n");
    fail = 1;
  }

  /* Zeroth-step flag → false */
  currentStep = (uint8_t *)source;
  pemCursorIsZerothStep = true;
  if (forthEntryStateAtCursor()) {
    printf("    FAIL: zeroth-step should return false\n");
    fail = 1;
  }

  /* Restore cursor state */
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;

  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: entry state derived from step bytes, not flag\n");
  }
  return fail;
}

/* ---- COMMIT 6: manage.c override — toggle, in-region, FCALL redirect ---- */

/* test_toggle_inserts_marker
 * Tests a new Forth capture's automatic opening/closing marker pair, plus
 * backward-compatible manual closing of an old open-ended region.
 *
 * Opening case: program [RPN][ITM_END][.END.], cursor on ITM_END.
 *   Pre-move skipped (isAtEndOfProgram), predecessor = RPN → wasOn=false →
 *   capture opens on an empty source placeholder between a marker pair.
 *
 * Closing case: program [marker][source(: SQ ...)][ITM_END][.END.], cursor on ITM_END.
 *   Pre-move skipped, predecessor = source step → wasOn=true → capture closes.
 *
 * Escaping mutation: E1 unconditionally entering capture (ignoring wasOn) —
 * the closing assertion fails (FLAG_ALPHA set when it should be clear).
 */
static int test_toggle_inserts_marker(void)
{
  int fail = 0;

  /* ---- Opening case: RPN step → wasOn=false → FLAG_ALPHA set ---- */
  {
    uint8_t prog[] = {
      0x4C,                                                             /* ITM_sin (RPN) */
      0x85, 0xB2,                                                       /* ITM_END */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    FAIL: writeTestProgram (opening) failed\n");
      return 1;
    }

    uint8_t *savedCurrentStep = currentStep;
    bool_t savedZeroth = pemCursorIsZerothStep;
    int16_t savedCatalog = catalog;
    uint16_t savedLocalStep = currentLocalStepNumber;
    bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
    int16_t savedTamFunc = tam.function;
    uint8_t savedCalcMode = calcMode;

    /* Cursor on ITM_END (offset 1) — pre-move skipped */
    currentStep = beginOfProgramMemory + 1;
    pemCursorIsZerothStep = false;
    currentLocalStepNumber = 2;
    catalog = CATALOG_NONE;
    aimBuffer[0] = 0;
    tam.mode = 0;
    tam.function = 0;
    calcMode = CM_PEM;
    clearSystemFlag(FLAG_ALPHA);

    extern void addStepInProgram(int16_t func);
    addStepInProgram(ITM_FORTH);

    /* Check: FLAG_ALPHA set (capture opened) */
    if (!getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA not set after opening toggle\n");
      fail = 1;
    }

    /* Check: the real call derives ITM_FORTH — R2-T6 item 1 */
    if (tam.function != ITM_FORTH) {
      printf("    FAIL: tam.function = %d, expected ITM_FORTH (%d) after opening toggle\n",
             tam.function, ITM_FORTH);
      fail = 1;
    }

    /* Check: opening marker, editable placeholder, and automatic closing
     * marker were inserted before ITM_END. */
    uint8_t *marker = beginOfProgramMemory + 1;
    uint8_t *placeholder = marker + 4;
    uint8_t *autoClose = placeholder + 4;
    if (*(marker + 0) != 0x8B || *(marker + 1) != 0x1A ||
    *(marker + 2) != 0xFD || *(marker + 3) != 0x00) {
      printf("    FAIL: opening marker not found (got 0x%02X 0x%02X 0x%02X 0x%02X)\n",
      *(marker+0), *(marker+1), *(marker+2), *(marker+3));
      fail = 1;
    }
    else if (placeholder[0] != 0x8B || placeholder[1] != 0x1A ||
             placeholder[2] != 0xFD || placeholder[3] != 0x00 ||
             currentStep != placeholder) {
      printf("    FAIL: editable placeholder not between marker pair\n");
      fail = 1;
    }
    else if (autoClose[0] != 0x8B || autoClose[1] != 0x1A ||
             autoClose[2] != 0xFD || autoClose[3] != 0x00) {
      printf("    FAIL: automatic closing marker not after first-line placeholder\n");
      fail = 1;
    }

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    catalog = savedCatalog;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    tam.function = savedTamFunc;
    calcMode = savedCalcMode;
  }

  /* ---- Closing case: source step → wasOn=true → FLAG_ALPHA clear ---- */
  {
    uint8_t prog[] = {
      0x8B, 0x1A, 0xFD, 0x00,                                         /* marker (opening) */
      0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* source: : SQ DUP * ; */
      ' ', '*', ' ', ';',
      0x85, 0xB2,                                                       /* ITM_END */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    FAIL: writeTestProgram (closing) failed\n");
      return 1;
    }

    uint8_t *savedCurrentStep = currentStep;
    bool_t savedZeroth = pemCursorIsZerothStep;
    int16_t savedCatalog = catalog;
    uint16_t savedLocalStep = currentLocalStepNumber;
    bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
    int16_t savedTamFunc = tam.function;

    /* Cursor on ITM_END (offset 20) — pre-move skipped */
    currentStep = beginOfProgramMemory + 20;
    pemCursorIsZerothStep = false;
    currentLocalStepNumber = 3;
    catalog = CATALOG_NONE;
    aimBuffer[0] = 0;
    tam.mode = 0;
    tam.function = 0;
    clearSystemFlag(FLAG_ALPHA);

    extern void addStepInProgram(int16_t func);
    addStepInProgram(ITM_FORTH);

    /* Check: FLAG_ALPHA clear (closing toggle, no capture) */
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA set after closing toggle (should be clear)\n");
      fail = 1;
    }

    /* No tam.function assertion here (R2-T6 item 1): whether E1 must clear
     * the sentinel on close is unresolved; asserting either value would
     * encode an unmade decision. */

    /* Check: closing marker inserted before ITM_END (offset 20) */
    uint8_t *marker2 = beginOfProgramMemory + 20;
    if (*(marker2 + 0) != 0x8B || *(marker2 + 1) != 0x1A ||
    *(marker2 + 2) != 0xFD || *(marker2 + 3) != 0x00) {
      printf("    FAIL: closing marker not found (got 0x%02X 0x%02X 0x%02X 0x%02X)\n",
      *(marker2+0), *(marker2+1), *(marker2+2), *(marker2+3));
      fail = 1;
    }

    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    catalog = savedCatalog;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    tam.function = savedTamFunc;
  }

  if (!fail) {
    printf("    PASS: new capture auto-brackets one line; legacy open region still toggles closed\n");
  }
  return fail;
}

/* test_forth_toggle_close_resets_sentinel
 * The transient alpha state (FLAG_ALPHA, aimBuffer, tam.function) must be
 * cleared when capture closes. A new region now owns its closing marker, so
 * committing its one source line with EXIT must clear that state without a
 * second FORTH toggle.
 * Probed and confirmed live, not just theoretical: after a normal open+close,
 * a SUBSEQUENT, unrelated plain alpha capture (func == ITM_AIM, structurally
 * outside any Forth region) got silently mislabeled — insertStepInProgram's
 * `else if(tam.function != ITM_FORTH) tam.function = ITM_LITERAL;` guard skips
 * the assignment when the sentinel is already (stale-)true, so the new
 * capture inherits ITM_FORTH. That then misroutes R3-1's cursor-offset math,
 * which is keyed on tam.function, not the step's real type.
 * Escaping mutation: remove the non-empty Forth close reset in
 * pemCloseAlphaInput — this test's post-close and post-AIM assertions fail. */
static int test_forth_toggle_close_resets_sentinel(void)
{
  int fail = 0;

  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin (RPN) */
    0x85, 0xB2,                                                       /* ITM_END */
  };
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  int16_t savedTamFunc = tam.function;
  uint8_t savedCalcMode = calcMode;

  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;
  calcMode = CM_PEM;
  clearSystemFlag(FLAG_ALPHA);

  extern void addStepInProgram(int16_t func);
  extern void insertStepInProgram(const int16_t func);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);

  addStepInProgram(ITM_FORTH);   /* open */
  if (tam.function != ITM_FORTH) {
    printf("    FAIL: tam.function = %d after open, expected ITM_FORTH (%d)\n",
           tam.function, ITM_FORTH);
    fail = 1;
  }

  runFunction(ITM_3);
  fnKeyExit(NOPARAM);
  if (forthCapIsOpen() && getSystemFlag(FLAG_ALPHA)) {
    /* A digit can leave an Alpha sub-menu on top.  EXIT intentionally pops
     * that menu first; the next EXIT commits the auto-paired one-line entry. */
    fnKeyExit(NOPARAM);
  }
  uint8_t *marker = beginOfProgramMemory + 1;
  uint8_t *source = marker + 4;
  uint8_t *autoClose = source + 5;
  if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA) || tam.function != 0) {
    printf("    FAIL: one-line EXIT left capture=%d alpha=%d tam.function=%d\n",
           forthCapIsOpen(), getSystemFlag(FLAG_ALPHA), tam.function);
    fail = 1;
  }
  else if (source[0] != 0x8B || source[1] != 0x1A ||
           source[2] != 0xFD || source[3] != 1 || source[4] != '3') {
    printf("    FAIL: first source line was not committed as FORTH \"3\"\n");
    fail = 1;
  }
  else if (autoClose[0] != 0x8B || autoClose[1] != 0x1A ||
           autoClose[2] != 0xFD || autoClose[3] != 0x00) {
    printf("    FAIL: automatic closing marker did not survive one-line EXIT\n");
    fail = 1;
  }

  /* A genuinely unrelated, non-Forth alpha capture must not inherit the
   * (already-fixed, but re-check) sentinel. */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = true;
  aimBuffer[0] = 0;
  clearSystemFlag(FLAG_ALPHA);
  insertStepInProgram(ITM_AIM);
  if (tam.function != ITM_LITERAL) {
    printf("    FAIL: tam.function = %d after unrelated plain AIM capture, "
           "expected ITM_LITERAL (%d) — stale Forth sentinel leaked\n",
           tam.function, ITM_LITERAL);
    fail = 1;
  }

  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  tam.function = savedTamFunc;
  calcMode = savedCalcMode;

  if (!fail) {
    printf("    PASS: auto-paired one-line EXIT resets tam.function; later plain capture is not mislabeled\n");
  }
  return fail;
}

/* test_fcall_redirect_records_name
 * Define SQ in dictionary; set tam.value to its widx;
 * insertStepInProgram(ITM_FCALL); byte-probe: step is
 * 0x8B 0x1A 0xFD 0x02 'S' 'Q' and NO 0x8B 0x1B (ITM_FCALL opcode).
 * Escaping mutation: falling through to PTP_NUMBER_16 arm (recording
 * 0x8B 0x1B + index — the exact names-only violation).
 */
static int test_fcall_redirect_records_name(void)
{
  /* Define SQ */
  define_word("SQ", 2);
  uint16_t sqIdx = fdict.count - 1;

  /* Minimal program */
  uint8_t prog[] = { 0x4C };  /* ITM_sin */
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Save state */
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedTamValue = tam.value;
  bool_t savedTamIndirect = tam.indirect;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);

  /* Setup for FCALL insertion */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.indirect = false;
  tam.value = sqIdx;
  clearSystemFlag(FLAG_ALPHA);

  extern void insertStepInProgram(const int16_t func);
  insertStepInProgram(ITM_FCALL);

  /* Rescan to update pointers */
  probeListPtrs("pre-scan:2502");
  scanLabelsAndPrograms();

  /* Byte-probe: the step at currentStep should be the redirected form */
  uint8_t *s = beginOfProgramMemory;
  if (*s != 0x8B || *(s+1) != 0x1A || *(s+2) != 0xFD) {
    printf("    FAIL: step does not start with ITM_FORTH opcode (got 0x%02X 0x%02X 0x%02X)\n",
    *s, *(s+1), *(s+2));
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.value = savedTamValue;
    tam.indirect = savedTamIndirect;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  uint8_t nameLen = *(s + 3);
  if (nameLen != 2) {
    printf("    FAIL: name length is %d, expected 2\n", nameLen);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.value = savedTamValue;
    tam.indirect = savedTamIndirect;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  if (*(s + 4) != 'S' || *(s + 5) != 'Q') {
    printf("    FAIL: name is '%c%c', expected 'SQ'\n", *(s+4), *(s+5));
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.value = savedTamValue;
    tam.indirect = savedTamIndirect;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  /* Verify NO ITM_FCALL opcode (0x8B 0x1B) anywhere in program */
  uint8_t *p = beginOfProgramMemory;
  uint8_t *end = firstFreeProgramByte;
  while (p < end - 1) {
    if (*p == 0x8B && *(p+1) == 0x1B) {
      printf("    FAIL: ITM_FCALL opcode (0x8B 0x1B) found in program!\n");
      cleanupTestProgram();
      currentStep = savedCurrentStep;
      pemCursorIsZerothStep = savedZeroth;
      currentLocalStepNumber = savedLocalStep;
      tam.value = savedTamValue;
      tam.indirect = savedTamIndirect;
      if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
      return 1;
    }
    p++;
  }

  /* R2-T6 item 5: copy the name out BEFORE cleanupTestProgram() releases/
   * restores the program region `s` points into — the old code dereferenced
   * `s + 4` in the PASS printf AFTER cleanup, a stale-pointer read. */
  char recordedName[3];
  recordedName[0] = *(s + 4);
  recordedName[1] = *(s + 5);
  recordedName[2] = 0;

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  tam.value = savedTamValue;
  tam.indirect = savedTamIndirect;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: FCALL redirect records name '%s', no ITM_FCALL opcode\n", recordedName);
  return 0;
}

/* test_fcall_redirect_rejects_stale
 * tam.value = fdict.count (invalid); call insertStepInProgram(ITM_FCALL);
 * assert ERROR_NON_PROGRAMMABLE_COMMAND and step count unchanged.
 * Escaping mutation: recording a step with empty/garbage name instead of rejecting.
 */
static int test_fcall_redirect_rejects_stale(void)
{
  /* Define a word so fdict.count > 0 */
  define_word("X", 1);

  /* Minimal program */
  uint8_t prog[] = { 0x4C };  /* ITM_sin */
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Save state */
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedTamValue = tam.value;
  bool_t savedTamIndirect = tam.indirect;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t *endBefore = firstFreeProgramByte;

  /* Setup: invalid widx */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.indirect = false;
  tam.value = fdict.count;  /* out of range */
  clearSystemFlag(FLAG_ALPHA);

  lastErrorCode = ERROR_NONE;

  extern void insertStepInProgram(const int16_t func);
  insertStepInProgram(ITM_FCALL);

  /* Check: error was raised */
  if (lastErrorCode != ERROR_NON_PROGRAMMABLE_COMMAND) {
    printf("    FAIL: lastErrorCode = %d, expected %d (ERROR_NON_PROGRAMMABLE_COMMAND)\n",
    lastErrorCode, ERROR_NON_PROGRAMMABLE_COMMAND);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.value = savedTamValue;
    tam.indirect = savedTamIndirect;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  /* Check: program unchanged (no bytes inserted) */
  if (firstFreeProgramByte != endBefore) {
    printf("    FAIL: program memory changed (firstFreeProgramByte moved)\n");
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.value = savedTamValue;
    tam.indirect = savedTamIndirect;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  tam.value = savedTamValue;
  tam.indirect = savedTamIndirect;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: FCALL with stale widx rejected, error raised, program unchanged\n");
  return 0;
}


/* ---- COMMIT 7: manage.c slice 2 — E3 empty-commit, E5 EDIT, cursor ---- */

/* test_forth_empty_enter_leaves_no_step
 * Open capture via insertStepInProgram(ITM_FORTH) (opening toggle), then
 * pemAlpha(ITM_ENTER) with empty aimBuffer; assert program step count
 * returned to exactly the automatic marker pair (no phantom source step) and
 * FLAG_ALPHA clear.
 * Escaping mutation: dropping E3 — the empty placeholder commits and
 * COMMIT 5's test_marker_parity invariant would flip downstream. */
static int test_forth_empty_enter_leaves_no_step(void)
{
  uint8_t prog[] = { 0x4C };  /* ITM_sin */

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint16_t savedTamFunc = tam.function;
  int stepsBefore = getNumberOfSteps();

  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);

  extern void insertStepInProgram(const int16_t func);
  insertStepInProgram(ITM_FORTH);

  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set after opening capture\n");
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }
  if (aimBuffer[0] != 0) {
    printf("    FAIL: aimBuffer not empty after opening capture\n");
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  extern void pemAlpha(int16_t item);
  pemAlpha(ITM_ENTER);

  if (getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA still set after E3 deletion\n");
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    return 1;
  }

  /* The empty ENTER is the escape hatch. The balanced marker pair survives,
   * but the capture sentinel must not: a leaked ITM_FORTH would make the next
   * unrelated keystroke behave as if capture were still up. */
  if (tam.function == ITM_FORTH) {
    printf("    FAIL: tam.function == ITM_FORTH after empty ENTER (stale sentinel "
           "survived the escape hatch)\n");
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    return 1;
  }

  int stepsAfter = getNumberOfSteps();
  if (stepsAfter != stepsBefore + 2) {
    printf("    FAIL: step count = %d, expected %d (only automatic marker pair remains)\n",
    stepsAfter, stepsBefore + 2);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    return 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  tam.function = savedTamFunc;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: empty ENTER deletes placeholder, FLAG_ALPHA clear, step count correct\n");
  return 0;
}

/* test_forth_edit_extracts_source
 * Write a FORTH ': SQ DUP * ;' step, point currentStep at it, call
 * pemAlpha(ITM_EDIT); assert aimBuffer == ": SQ DUP * ;".
 * Escaping mutation: using the REM offset 6 instead of 8 (aimBuffer
 * starts with two garbage bytes from STD_LEFT_SINGLE_QUOTE). */
static int test_forth_edit_extracts_source(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A,  /* ITM_FORTH */
    0xFD,         /* STRING_LABEL_VARIABLE */
    12,           /* length */
    ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', ' ', '*', ' ', ';'
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint16_t savedTamFunc = tam.function;
  char aimSaved[AIM_BUFFER_LENGTH];
  xcopy(aimSaved, aimBuffer, sizeof(aimSaved));

  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  clearSystemFlag(FLAG_ALPHA);

  extern void pemAlpha(int16_t item);
  pemAlpha(ITM_EDIT);

  if (strcmp(forthTestCapText(), ": SQ DUP * ;") != 0) {
    printf("    FAIL: cap text = '%s', expected ': SQ DUP * ;'\n", forthTestCapText());
    forthCapClose();
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  forthCapClose();
  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  tam.function = savedTamFunc;
  xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: FORTH EDIT extracts source correctly (bare, offset 0)\n");
  return 0;
}

/* test_decode_marker_directions
 * Writes marker/source/marker/marker program; decodeOneStep each marker;
 * assert tmpString bytes are \x80\xbbFORTH, FORTH\x80\xab, \x80\xbbFORTH
 * respectively; decode the source step and assert it renders bare
 * (§8.9 acceptance 4).
 * Escaping mutation: inverting the parity (call !forthMarkerTurnsOn) —
 * all three direction assertions fail. */
static int test_decode_marker_directions(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 1 */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',               /* source */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 2 */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 3 */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *marker1 = beginOfProgramMemory;
  const uint8_t *source  = beginOfProgramMemory + 4;
  const uint8_t *marker2 = beginOfProgramMemory + 4 + 16;
  const uint8_t *marker3 = marker2 + 4;

  int fail = 0;

  /* Marker 1: opening → \x80\xbbFORTH (7 bytes: 2 glyph + 5 "FORTH") */
  decodeOneStep((uint8_t *)marker1);
  if (strlen(tmpString) != 7 ||
  tmpString[0] != (char)0x80 || tmpString[1] != (char)0xBB ||
  memcmp(tmpString + 2, "FORTH", 5) != 0) {
    printf("    FAIL: marker 1 tmpString = '%s' (len=%zu), expected \\x80\\xbbFORTH\n",
    tmpString, strlen(tmpString));
    fail = 1;
  }

  /* Marker 2: closing → FORTH\x80\xab */
  decodeOneStep((uint8_t *)marker2);
  if (strlen(tmpString) != 7 ||
  memcmp(tmpString, "FORTH", 5) != 0 ||
  tmpString[5] != (char)0x80 || tmpString[6] != (char)0xAB) {
    printf("    FAIL: marker 2 tmpString = '%s' (len=%zu), expected FORTH\\x80\\xab\n",
    tmpString, strlen(tmpString));
    fail = 1;
  }

  /* Marker 3: opening → \x80\xbbFORTH */
  decodeOneStep((uint8_t *)marker3);
  if (strlen(tmpString) != 7 ||
  tmpString[0] != (char)0x80 || tmpString[1] != (char)0xBB ||
  memcmp(tmpString + 2, "FORTH", 5) != 0) {
    printf("    FAIL: marker 3 tmpString = '%s' (len=%zu), expected \\x80\\xbbFORTH\n",
    tmpString, strlen(tmpString));
    fail = 1;
  }

  /* Source step: renders BARE — no "FORTH" prefix, no quotes. Only the two
   * marker forms carry the word "FORTH" at all. */
  decodeOneStep((uint8_t *)source);
  if (strcmp(tmpString, ": SQ DUP * ;") != 0) {
    printf("    FAIL: source step tmpString = '%s', expected ': SQ DUP * ;'\n",
    tmpString);
    fail = 1;
  }

  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: marker directions = \\x80\\xbbFORTH / FORTH\\x80\\ab / \\x80\\xbbFORTH; source renders bare\n");
  }
  return fail;
}

/* test_decode_source_bare
 * A len > 0 ITM_FORTH step renders its payload BARE: no "FORTH " name prefix
 * and no surrounding quotes. Quoting would be actively harmful — a string
 * literal step already renders 'text' WITH quotes, so a quoted Forth payload
 * would be indistinguishable from one. Bare collides with nothing.
 * Escaping mutation: the len > 0 case falling through to the generic
 * NAME STD_LEFT_SINGLE_QUOTE payload STD_RIGHT_SINGLE_QUOTE path. */
static int test_decode_source_bare(void)
{
  const char srcText[] = "2 2 +";
  uint8_t len = (uint8_t)strlen(srcText);
  /* Build: ITM_FORTH opcode + STRING_LABEL_VARIABLE + len + name bytes */
  uint8_t step[64];
  uint8_t *p = step;
  *p++ = (ITM_FORTH >> 8) | 0x80;  /* 0x8B */
  *p++ =  ITM_FORTH       & 0xFF;  /* 0x1A */
  *p++ = STRING_LABEL_VARIABLE;     /* 0xFD */
  *p++ = len;
  memcpy(p, srcText, len);
  p += len;
  uint16_t stepSize = (uint16_t)(p - step);

  if (!writeTestProgram(step, stepSize)) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  decodeOneStep((uint8_t *)beginOfProgramMemory);

  /* The payload, and nothing but the payload. */
  if (strcmp(tmpString, srcText) != 0) {
    printf("    FAIL: tmpString = '%s', expected '%s'\n", tmpString, srcText);
    cleanupTestProgram();
    return 1;
  }

  /* Pin the collision rationale explicitly: no quote glyph may appear, or a
   * Forth source line would render identically to a string literal step. */
  if (strstr(tmpString, STD_LEFT_SINGLE_QUOTE) != NULL ||
      strstr(tmpString, STD_RIGHT_SINGLE_QUOTE) != NULL) {
    printf("    FAIL: tmpString = '%s' contains a quote glyph; bare render "
           "must not quote (collides with a string literal step)\n", tmpString);
    cleanupTestProgram();
    return 1;
  }

  cleanupTestProgram();
  printf("    PASS: len>0 source step renders bare as '%s' (no prefix, no quotes)\n", srcText);
  return 0;
}

/* COMMIT 9: MNU_FORTH row at slot 213 is CAT_MENU with "FWRD" label.
 * Escaping mutation: reverting slot 213 back to CAT_FREE (botched upstream merge). */
static int test_mnu_forth_row(void)
{
  int fail = 0;

  if ((indexOfItems[MNU_FORTH].status & CAT_STATUS) != CAT_MENU) {
    printf("    FAIL: MNU_FORTH CAT_STATUS is %x, expected CAT_MENU (%x)\n",
    indexOfItems[MNU_FORTH].status & CAT_STATUS, CAT_MENU);
    fail = 1;
  }

  if (compareString(indexOfItems[MNU_FORTH].itemCatalogName, "FWRD", CMP_BINARY) != 0) {
    printf("    FAIL: MNU_FORTH itemCatalogName is '%s', expected 'FWRD'\n",
    indexOfItems[MNU_FORTH].itemCatalogName);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: MNU_FORTH row is CAT_MENU with 'FWRD' label\n");
  }
  return fail;
}

/* COMMIT 10: MNU_FORTH registered as dynamic softmenu #22, NUMBER_OF_DYNAMIC_SOFTMENUS == 23.
 * Escaping mutation: bump NUMBER_OF_DYNAMIC_SOFTMENUS without inserting the softmenu[] and
 * dynamicSoftmenu[] rows — TAMFLAG (index 22) is misclassified as dynamic, renders empty. */
_Static_assert(NUMBER_OF_DYNAMIC_SOFTMENUS == 23, "NUMBER_OF_DYNAMIC_SOFTMENUS must be 23 (P-H6)");

static int test_dynamic_menu_registration(void)
{
  int fail = 0;

  if (dynamicSoftmenu[22].menuItem != -MNU_FORTH) {
    printf("    FAIL: dynamicSoftmenu[22].menuItem = %d, expected %d (-MNU_FORTH)\n",
    dynamicSoftmenu[22].menuItem, -MNU_FORTH);
    fail = 1;
  }

  if (softmenu[22].menuItem != -MNU_FORTH) {
    printf("    FAIL: softmenu[22].menuItem = %d, expected %d (-MNU_FORTH)\n",
    softmenu[22].menuItem, -MNU_FORTH);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: dynamicSoftmenu[22] and softmenu[22] are -MNU_FORTH\n");
  }
  return fail;
}

/* test_static_menu_integrity
 * Escaping mutation: bump NUMBER_OF_DYNAMIC_SOFTMENUS without inserting the softmenu[] and
 * dynamicSoftmenu[] rows — TAMFLAG shifts to index 22, which is now < NUMBER_OF_DYNAMIC_SOFTMENUS,
 * so it is treated as dynamic (empty). This test guards the exact off-by-one. */
static int test_static_menu_integrity(void)
{
  int fail = 0;

  if (softmenu[23].menuItem != -MNU_TAMFLAG) {
    printf("    FAIL: softmenu[23].menuItem = %d, expected %d (-MNU_TAMFLAG)\n",
    softmenu[23].menuItem, -MNU_TAMFLAG);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: softmenu[23] is -MNU_TAMFLAG (static area intact)\n");
  }
  return fail;
}

/* ---- COMMIT 11: softmenus.c slice 2 — the : NAME picker builder ---- */

extern void testInitVariableSoftmenu(int16_t menu);

/* test_picker_scan_basic
 * Program: marker, : SQ DUP * ;, : CUBE DUP DUP * * ;, marker.
 * currentStep on the last marker. Call initVariableSoftmenu(22).
 * Assert menuContent contains "SQ" and "CUBE", numItems == 2, sorted.
 * Escaping mutation: the walk stopping BEFORE currentStep (exclusive bound) —
 * a word defined on the immediately preceding line is missing; this is
 * §8.9 acceptance 3's essence. */
static int test_picker_scan_basic(void)
{
  /* marker | : SQ DUP * ; | : CUBE DUP DUP * * ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x14, ':', ' ', 'C', 'U', 'B', 'E', ' ', 'D',    /* : CUBE DUP DUP */
    'U', 'P', ' ', 'D', 'U', 'P', ' ', '*', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* currentStep on the CUBE step — mutation check: exclusive bound (<)
   * would skip this step, so CUBE would be missing from the menu */
  const uint8_t *cubeStep = beginOfProgramMemory + 4 + 16;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)cubeStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 2) {
    printf("    FAIL: numItems = %d, expected 2\n", dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    /* R2-T5 item 1: exact order, not membership. qsort's comparator is
     * compareString(..., CMP_EXTENSIVE) (softmenus.c sortMenu) — binary
     * alphabetic order puts CUBE ('C') before SQ ('S'). */
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int16_t len0 = strlen(content);
    if (compareString(content, "CUBE", CMP_BINARY) != 0) {
      printf("    FAIL: item 0 is '%s', expected 'CUBE'\n", content);
      fail = 1;
    }
    const char *item1 = content + len0 + 1;
    int16_t len1 = strlen(item1);
    if (compareString(item1, "SQ", CMP_BINARY) != 0) {
      printf("    FAIL: item 1 is '%s', expected 'SQ'\n", item1);
      fail = 1;
    }
    /* One extra NUL follows the last name's own terminator (production
     * allocates numberOfBytes = 1 + sum(len+1)). */
    char afterLast = *(item1 + len1 + 1);
    if (afterLast != '\0') {
      printf("    FAIL: byte after second string terminator = %d, expected 0\n",
             (unsigned char)afterLast);
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: picker order is exactly CUBE, SQ; numItems==2; trailing NUL present\n");
  }
  return fail;
}

/* test_picker_omits_long_names
 * R2-T5 item 2: the old "kept" name was SHORT (5 bytes) — nowhere near the
 * nameLen<=14 boundary (softmenus.c: `if (nameLen > 0 && nameLen <= 14)`), so
 * R2's `<=14 -> <=13` mutation stayed GREEN even though it moved the boundary.
 * Kept name is now exactly 14 bytes (KEEPABCDEFGHIJ) and shares no prefix
 * with either rejected 15-byte name (ABCDEFGHIJKLMNO, PQRSTUVWXYZABCD), so a
 * boundary-off-by-one can't coincidentally still look right.
 * Escaping mutation: truncating instead of omitting — the 15-byte names
 * are cut to 14 bytes and appear in menuContent, so numItems > 1. */
static int test_picker_omits_long_names(void)
{
  /* marker | :ABCDEFGHIJKLMNO(15) | :PQRSTUVWXYZABCD(15) | :KEEPABCDEFGHIJ(14) | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x13, ':', ' ', 'A', 'B', 'C', 'D', 'E', 'F',    /* : ABCDEFGHIJKLMNO (15) */
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x13, ':', ' ', 'P', 'Q', 'R', 'S', 'T', 'U',    /* : PQRSTUVWXYZABCD (15) */
    'V', 'W', 'X', 'Y', 'Z', 'A', 'B', 'C', 'D', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x12, ':', ' ', 'K', 'E', 'E', 'P', 'A', 'B',    /* : KEEPABCDEFGHIJ (14) */
    'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* marker(4) + 15byte1(4+19=23) + 15byte2(23) + 14byte(4+18=22) = 72 → closing marker */
  const uint8_t *closingMarker = beginOfProgramMemory + 4 + 23 + 23 + 22;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)closingMarker;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1 (two 15-byte names omitted, "
           "KEEPABCDEFGHIJ kept)\n", dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    if (compareString(content, "KEEPABCDEFGHIJ", CMP_BINARY) != 0) {
      printf("    FAIL: kept name is '%s', expected exactly 'KEEPABCDEFGHIJ' (14 bytes)\n",
             content);
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: 15-byte names omitted, exactly 14-byte KEEPABCDEFGHIJ kept, numItems==1\n");
  }
  return fail;
}

/* test_picker_rebuilds_same_menu
 * R2-T5 item 3: R2 deleted the special-case term in showSoftmenuCurrentPart's
 * dynamic-menu cache check —
 *   if(softmenu[m].menuItem != cachedDynamicMenu || ... || softmenu[m].menuItem == -MNU_FORTH)
 * (softmenus.c) — and the full suite stayed GREEN, because every existing
 * picker test calls initVariableSoftmenu directly (via testInitVariableSoftmenu),
 * bypassing the cache gate entirely. This test drives the REAL public path —
 * showSoftmenu(-MNU_FORTH) then showSoftmenuCurrentPart(), exactly what the UI
 * calls — twice in a row without ever changing softmenuStack[0] in between, so
 * the second call reaches the gate with cachedDynamicMenu already == -MNU_FORTH.
 * Every other dynamic menu can trust "same identity -> same content"; MNU_FORTH
 * cannot, because its content is derived from live program memory that a user
 * can edit between two views of the same menu (add a word, look at the menu
 * again without leaving it).
 * Escaping mutation: drop the `|| softmenu[m].menuItem == -MNU_FORTH` term —
 * the second showSoftmenuCurrentPart() call sees an identity match and skips
 * the rebuild, so TWO stays absent from the still-cached, now-stale content. */
static int test_picker_rebuilds_same_menu(void)
{
  /* marker | : ONE 1 ; | : TWO 2 ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x09, ':', ' ', 'O', 'N', 'E', ' ', '1', ' ', ';', /* : ONE 1 ; */
    0x8B, 0x1A, 0xFD, 0x09, ':', ' ', 'T', 'W', 'O', ' ', '2', ' ', ';', /* : TWO 2 ; */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* marker(4) + ONE-step(4+9=13) = 17: last byte of the ONE step.
   * TWO step starts at offset 17 (>17 is false, so TWO is excluded when
   * currentStep sits inside ONE's payload). */
  const uint8_t *withinOneStep = beginOfProgramMemory + 4 + 13 - 1;
  /* marker(4) + ONE(13) + TWO(13) = 30: closing marker, TWO now included. */
  const uint8_t *closingMarker = beginOfProgramMemory + 4 + 13 + 13;

  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  int16_t savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent = dynamicSoftmenu[22].menuContent;
  int16_t savedNumItems = dynamicSoftmenu[22].numItems;
  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems = 0;

  extern void showSoftmenu(int16_t menu);
  extern void showSoftmenuCurrentPart(void);

  currentProgramNumber = 1;
  currentStep = (uint8_t *)withinOneStep;

  showSoftmenu(-MNU_FORTH);
  showSoftmenuCurrentPart();

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1 || !dynamicSoftmenu[22].menuContent ||
      compareString((const char *)dynamicSoftmenu[22].menuContent, "ONE", CMP_BINARY) != 0) {
    printf("    FAIL: first build — numItems=%d content='%s' (expected 1, \"ONE\")\n",
           dynamicSoftmenu[22].numItems,
           dynamicSoftmenu[22].menuContent ? (const char *)dynamicSoftmenu[22].menuContent : "(null)");
    fail = 1;
  }

  if (!fail) {
    /* Move currentStep so TWO is now "before" it too — no showSoftmenu()
     * call in between, no menu closed/reopened, cachedDynamicMenu is still
     * -MNU_FORTH from the first build. */
    currentStep = (uint8_t *)closingMarker;
    showSoftmenuCurrentPart();

    int foundTWO = 0;
    if (dynamicSoftmenu[22].menuContent) {
      const char *content = (const char *)dynamicSoftmenu[22].menuContent;
      while (*content) {
        if (compareString(content, "TWO", CMP_BINARY) == 0) foundTWO = 1;
        content += strlen(content) + 1;
      }
    }
    if (!foundTWO) {
      printf("    FAIL: second build (same cached menu) did not include TWO — "
             "numItems=%d (stale cache not rebuilt)\n", dynamicSoftmenu[22].numItems);
      fail = 1;
    }
  }

  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems = savedNumItems;
  cachedDynamicMenu = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: MNU_FORTH rebuilds on every display, even with unchanged cache identity\n");
  }
  return fail;
}

/* test_picker_capacity_boundary
 * R2 finding 6, ruled: the picker's accepted-name buffer is 15-byte slots in
 * the global tmpString (TMP_STR_LENGTH=2560), giving a hard capacity of
 * TMP_STR_LENGTH/15 = 170 names. Policy: truncate by scan order — the cap
 * stops RECORDING new names but the tokenizer keeps running.
 * One program with 171 unique colon definitions (cap+1) proves both edges at
 * once: exactly 170 survive (the "at cap" edge — none of the first 170 are
 * lost to an off-by-one), and the 171st, which is LAST in scan order, is the
 * one dropped (proves truncation is by scan order, not silently keeping an
 * arbitrary later one instead of an earlier one).
 * Escaping mutation: drop the `nNames < forthPickerMaxNames` guard — numItems
 * becomes 171 instead of 170, and (with a fixed build) the 171st slot write
 * lands one 15-byte slot past tmpString's declared length. */
static int test_picker_capacity_boundary(void)
{
  const int totalDefs = 171;   /* cap (170) + 1 */
  const int stepBytes = 14;    /* header(4) + ": N000 1 ;"(10) */
  uint16_t progLen = (uint16_t)(4 + totalDefs * stepBytes + 4);   /* open marker + defs + close marker */
  uint8_t *prog = (uint8_t *)malloc(progLen);
  if (!prog) {
    printf("    FAIL: malloc failed\n");
    return 1;
  }

  uint8_t *p = prog;
  *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;   /* marker (opening) */
  for (int i = 0; i < totalDefs; i++) {
    char name[5];
    sprintf(name, "N%03d", i);   /* N000 .. N170, 4 bytes each, distinct */
    *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 10;   /* len=10: ": NNNN 1 ;" */
    *p++ = ':'; *p++ = ' ';
    *p++ = (uint8_t)name[0]; *p++ = (uint8_t)name[1];
    *p++ = (uint8_t)name[2]; *p++ = (uint8_t)name[3];
    *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';
  }
  *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;   /* marker (closing) */

  if ((p - prog) != progLen || !writeTestProgram(prog, progLen)) {
    printf("    FAIL: writeTestProgram failed\n");
    free(prog);
    return 1;
  }
  free(prog);

  uint8_t *closingMarker = beginOfProgramMemory + (progLen - 4);
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = closingMarker;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 170) {
    printf("    FAIL: numItems = %d, expected exactly 170 (cap; 171st dropped)\n",
           dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundFirst = 0, foundLast = 0;
    while (*content) {
      if (compareString(content, "N000", CMP_BINARY) == 0) foundFirst = 1;
      if (compareString(content, "N170", CMP_BINARY) == 0) foundLast = 1;
      content += strlen(content) + 1;
    }
    if (!foundFirst) {
      printf("    FAIL: 'N000' (first in scan order, well under the cap) missing\n");
      fail = 1;
    }
    if (foundLast) {
      printf("    FAIL: 'N170' (171st, last in scan order) present — should be the one dropped\n");
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: picker capacity boundary — exactly 170 survive, 171st (scan-order-last) dropped\n");
  }
  return fail;
}

/* test_picker_dedupes
 * The same : SQ on two lines yields one entry.
 * Escaping mutation: skipping the dedupe — numItems == 2 for one name. */
static int test_picker_dedupes(void)
{
  /* marker | : SQ DUP * ; | : SQ DUP * ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP (dup) */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *marker2 = beginOfProgramMemory + 4 + 16 + 16;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)marker2;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1 (deduped)\n",
    dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundSQ = 0;
    while (*content) {
      if (compareString(content, "SQ", CMP_BINARY) == 0) foundSQ = 1;
      content += strlen(content) + 1;
    }
    if (!foundSQ) {
      printf("    FAIL: 'SQ' not found in menuContent\n");
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: duplicate : SQ collapsed to one entry\n");
  }
  return fail;
}

/* ---- COMMIT 12: keyboard.c — picker insert at cursor ---- */

/* test_picker_insert_at_cursor
 * Build picker menu with "SQ" from program. Set aimBuffer empty, cursor at 0.
 * Call pickerInsertName; assert aimBuffer == "SQ ", T_cursorPos == 3.
 * Escaping mutation: inserting at aimBuffer end instead of T_cursorPos —
 * with empty buffer end == cursor so this only catches the case when
 * buffer is non-empty (tested by test_picker_insert_mid_line). */
static int test_picker_insert_at_cursor(void)
{
  /* marker | : SQ DUP * ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP * ; */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *sqStep = beginOfProgramMemory + 4;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedCatalog = catalog;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Open real capture per CAPTURE-DRIVE CONTRACT */
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  clearSystemFlag(FLAG_ALPHA);

  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;

  extern void runFunction(int16_t);
  runFunction(ITM_AIM);

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
    printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
    fail = 1;
  }

  if (!fail) {
    /* Set up for picker insert */
    T_cursorPos = 0;
    softmenuStack[0].softmenuId = 22;
    dynamicMenuItem = 0;

    extern bool_t pickerInsertName(void);
    if (!pickerInsertName()) {
      printf("    FAIL: pickerInsertName returned false\n");
      fail = 1;
    } else {
      if (strcmp(forthTestCapText(), "SQ ") != 0) {
        printf("    FAIL: cap text = '%s', expected 'SQ '\n", forthTestCapText());
        fail = 1;
      }
      if (T_cursorPos != 3) {
        printf("    FAIL: T_cursorPos = %d, expected 3\n", T_cursorPos);
        fail = 1;
      }
    }
  }

  /* Cleanup: close capture */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  tam.function = savedTamFunc;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  catalog = savedCatalog;
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
  }
  dynamicSoftmenu[22].numItems = 0;
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: picker inserts 'SQ ' at cursor 0, T_cursorPos == 3\n");
  }
  return fail;
}

/* test_picker_insert_mid_line
 * Build picker menu with "SQ". Set aimBuffer = "DUP ", cursor at position 0.
 * Call pickerInsertName; assert aimBuffer == "SQ DUP " (SQ inserted at
 * cursor, not appended at end).
 * Escaping mutation: inserting at aimBuffer end instead of T_cursorPos —
 * would produce "DUP SQ" instead of "SQ DUP ". */
static int test_picker_insert_mid_line(void)
{
  /* marker | : SQ DUP * ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP * ; */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *sqStep = beginOfProgramMemory + 4;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedCatalog = catalog;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Open real capture per CAPTURE-DRIVE CONTRACT */
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  clearSystemFlag(FLAG_ALPHA);

  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;

  extern void runFunction(int16_t);
  runFunction(ITM_AIM);

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
    printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
    fail = 1;
  }

  if (!fail) {
    /* Type "DUP " via key idiom */
    const int16_t preItems[] = { ITM_D, ITM_U, ITM_P, ITM_SPACE };
    int i;
    for (i = 0; i < (int)(sizeof(preItems) / sizeof(preItems[0])); i++) {
      runFunction(preItems[i]);
    }

    /* Set cursor at 0 (before DUP) */
    T_cursorPos = 0;
    softmenuStack[0].softmenuId = 22;
    dynamicMenuItem = 0;

    extern bool_t pickerInsertName(void);
    if (!pickerInsertName()) {
      printf("    FAIL: pickerInsertName returned false\n");
      fail = 1;
    } else {
      if (strcmp(forthTestCapText(), "SQ DUP ") != 0) {
        printf("    FAIL: cap text = '%s', expected 'SQ DUP '\n", forthTestCapText());
        fail = 1;
      }
      if (T_cursorPos != 3) {
        printf("    FAIL: T_cursorPos = %d, expected 3\n", T_cursorPos);
        fail = 1;
      }
    }
  }

  /* Cleanup: close capture */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  tam.function = savedTamFunc;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  catalog = savedCatalog;
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
  }
  dynamicSoftmenu[22].numItems = 0;
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: mid-line insert at cursor (not end), 'SQ DUP ' correct\n");
  }
  return fail;
}

/* test_picker_trailing_space
 * Build picker menu with "SQ". Open capture, type "DUP ", cursor at end (4).
 * Insert "SQ"; assert cap text == "DUP SQ " (trailing space present).
 * Escaping mutation: omitting trailing space — would produce "DUP SQ" */
static int test_picker_trailing_space(void)
{
  /* marker | : SQ DUP * ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP * ; */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *sqStep = beginOfProgramMemory + 4;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedCatalog = catalog;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Open real capture per CAPTURE-DRIVE CONTRACT */
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  clearSystemFlag(FLAG_ALPHA);

  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;

  extern void runFunction(int16_t);
  runFunction(ITM_AIM);

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
    printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
    fail = 1;
  }

  if (!fail) {
    /* Type "DUP " via key idiom */
    const int16_t preItems[] = { ITM_D, ITM_U, ITM_P, ITM_SPACE };
    int i;
    for (i = 0; i < (int)(sizeof(preItems) / sizeof(preItems[0])); i++) {
      runFunction(preItems[i]);
    }

    /* Set cursor at end */
    T_cursorPos = 4;
    softmenuStack[0].softmenuId = 22;
    dynamicMenuItem = 0;

    extern bool_t pickerInsertName(void);
    if (!pickerInsertName()) {
      printf("    FAIL: pickerInsertName returned false\n");
      fail = 1;
    } else {
      if (strcmp(forthTestCapText(), "DUP SQ ") != 0) {
        printf("    FAIL: cap text = '%s', expected 'DUP SQ '\n", forthTestCapText());
        fail = 1;
      }
      if (T_cursorPos != 7) {
        printf("    FAIL: T_cursorPos = %d, expected 7\n", T_cursorPos);
        fail = 1;
      }
    }
  }

  /* Cleanup: close capture */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  tam.function = savedTamFunc;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  catalog = savedCatalog;
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
  }
  dynamicSoftmenu[22].numItems = 0;
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: trailing space present, 'DUP SQ ' correct\n");
  }
  return fail;
}

/* test_picker_guard_menu_identity
 * ForthPickerGuard must check softmenu[softmenuStack[0].softmenuId].menuItem
 * == -MNU_FORTH BEFORE any dynamicSoftmenu[] indexing. With Forth capture
 * globals set (CM_PEM, FLAG_ALPHA, tam.function=ITM_FORTH, valid dynamicMenuItem),
 * pointing at the MNU_FORTH menu -> guard true; pointing at any other menu
 * -> guard false.
 * Escaping mutation: drop the menu-identity conjunct from forthPickerGuard —
 * the wrong-menu case returns true and the test FAILs. */
static int test_picker_guard_menu_identity(void)
{
  extern bool_t forthPickerGuard(int16_t);
  extern const softmenu_t softmenu[];

  int fail = 0;

  /* Save state */
  uint8_t savedCalcMode = calcMode;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  int16_t savedTamFunction = tam.function;
  int16_t savedDynMenuItem = dynamicMenuItem;
  int16_t savedSoftmenuStackId = softmenuStack[0].softmenuId;

  /* Set up Forth capture globals */
  calcMode = CM_PEM;
  setSystemFlag(FLAG_ALPHA);
  tam.function = ITM_FORTH;
  dynamicMenuItem = 0;  /* valid index */

  /* Find MNU_FORTH index in softmenu[] */
  int16_t forthMenuIdx = -1;
  for (int16_t si = 0; si < 200; si++) {
    if (softmenu[si].menuItem == -MNU_FORTH) {
      forthMenuIdx = si;
      break;
    }
  }
  if (forthMenuIdx < 0) {
    printf("    FAIL: MNU_FORTH not found in softmenu array\n");
    fail = 1;
    goto cleanup_guard;
  }

  /* Save and set numItems so dynamicMenuItem=0 is in range */
  int16_t savedForthNumItems = dynamicSoftmenu[forthMenuIdx].numItems;
  dynamicSoftmenu[forthMenuIdx].numItems = 6;

  /* Find a non-MNU_FORTH dynamic menu (any other entry) */
  int16_t otherMenuIdx = -1;
  for (int16_t si = 0; si < NUMBER_OF_DYNAMIC_SOFTMENUS; si++) {
    if (si != forthMenuIdx) {
      otherMenuIdx = si;
      break;
    }
  }
  if (otherMenuIdx < 0) {
    printf("    FAIL: no other dynamic menu found\n");
    fail = 1;
    goto cleanup_guard_numitems;
  }

  /* Test 1: MNU_FORTH menu -> guard should be true */
  softmenuStack[0].softmenuId = forthMenuIdx;
  if (!forthPickerGuard(ITM_NOP)) {
    printf("    FAIL: forthPickerGuard returned false for MNU_FORTH menu\n");
    fail = 1;
  }

  /* Test 2: non-MNU_FORTH menu -> guard should be false (menu-identity conjunct) */
  softmenuStack[0].softmenuId = otherMenuIdx;
  if (forthPickerGuard(ITM_NOP)) {
    printf("    FAIL: forthPickerGuard returned true for non-MNU_FORTH menu (menu-identity conjunct missing)\n");
    fail = 1;
  }

cleanup_guard_numitems:
  dynamicSoftmenu[forthMenuIdx].numItems = savedForthNumItems;
cleanup_guard:
  /* Restore state */
  calcMode = savedCalcMode;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  tam.function = savedTamFunction;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;

  if (!fail) {
    printf("    PASS: forthPickerGuard true for MNU_FORTH, false for other menu\n");
  }
  return fail;
}

/* test_picker_glyph_tokenize
 * Source step: ": A<a2><20>B DUP ;" — name contains STD_ANGLE ("\xa2\x20").
 * Build the menu; assert menuContent contains the 4-byte name "A\xa2\x20B"
 * and does NOT contain the 2-byte prefix "A\xa2" (which would appear if
 * byte-wise splitting cut the token inside the glyph).
 * Escaping mutation: restore the byte-wise src[pos] != ' ' advance — the
 * name splits at 0x20 and both assertions fail. */
static int test_picker_glyph_tokenize(void)
{
  /* marker | : A<STD_ANGLE>B DUP ; | marker | .END. */
  /* Payload: ": A\xa2\x20B DUP ;" = 12 bytes */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'A', 0xA2, 0x20, 'B',             /* : A<STD_ANGLE>B */
    ' ', 'D', 'U', 'P', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *defStep = beginOfProgramMemory + 4;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)defStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1\n", dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundFull = 0, foundSplit = 0;
    while (*content) {
      /* Check for full 4-byte name "A\xa2\x20B" */
      if (content[0] == 'A' && content[1] == '\xa2' && content[2] == '\x20' && content[3] == 'B' && content[4] == '\0') {
        foundFull = 1;
      }
      /* Check for split 2-byte prefix "A\xa2" (should NOT be present) */
      if (content[0] == 'A' && content[1] == '\xa2' && content[2] == '\0') {
        foundSplit = 1;
      }
      content += strlen(content) + 1;
    }
    if (!foundFull) {
      printf("    FAIL: full 4-byte name not found in menuContent\n");
      fail = 1;
    }
    if (foundSplit) {
      printf("    FAIL: split prefix 'A\xa2' found in menuContent (glyph was split)\n");
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: glyph-wise tokenizer preserves STD_ANGLE in name\n");
  }
  return fail;
}

/* test_picker_long_token_skipped
 * Source step: 200-byte spaceless token followed by " : SQ DUP ;".
 * The 200-byte token exceeds FORTH_TOKEN_MAX (63) and must be skipped.
 * Assert numItems == 1 and "SQ" present (the long token was skipped, not copied).
 *
 * R2-T5 item 4: this test pins SEMANTIC OMISSION ONLY — that an over-length
 * token does not appear in the built menu. It does NOT catch an unchecked
 * copy into a fixed buffer; that claim was false and is retracted here.
 * softmenus.c's token copy (`case MNU_FORTH:`, the `xcopy(tok, line + start,
 * tokLen)` line) is inline in the giant initVariableSoftmenu switch, gated by
 * `if (tokLen > FORTH_TOKEN_MAX) { skip, continue; }` immediately above it —
 * there is no separable helper function to wrap with pre/post canaries under
 * FORTH_DEBUG_SELFTEST. Per this task's own instruction ("If the production
 * code has no separable helper, STOP and report instead of inventing an ASan
 * command or relying on stack corruption"): stopped. An overflow of the
 * length check itself can only be verified under ASan, which is not the
 * sanctioned gate — this remains an accepted, documented gap, not a covered
 * mutation. */
static int test_picker_long_token_skipped(void)
{
  /* Build payload: 200 'X' bytes + " : SQ DUP ;" (11 bytes) = 211 bytes */
  uint8_t payload[211];
  int16_t i;
  for (i = 0; i < 200; i++) {
    payload[i] = 'X';
  }
  const char suffix[] = " : SQ DUP ;";
  for (i = 0; i < 11; i++) {
    payload[200 + i] = (uint8_t)suffix[i];
  }

  /* Build program: marker | ITM_FORTH step | marker | .END. */
  uint8_t header[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0xD3,                                              /* ITM_FORTH, STRING_LABEL_VARIABLE, len=211 */
  };
  uint8_t footer[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  uint16_t progLen = sizeof(header) + sizeof(payload) + sizeof(footer);
  uint8_t *prog = (uint8_t *)malloc(progLen);
  if (!prog) {
    printf("    FAIL: malloc failed\n");
    return 1;
  }
  xcopy(prog, header, sizeof(header));
  xcopy(prog + sizeof(header), payload, sizeof(payload));
  xcopy(prog + sizeof(header) + sizeof(payload), footer, sizeof(footer));

  if (!writeTestProgram(prog, progLen)) {
    printf("    FAIL: writeTestProgram failed\n");
    free(prog);
    return 1;
  }
  free(prog);

  const uint8_t *defStep = beginOfProgramMemory + 4;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)defStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1 (long token should be skipped)\n", dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundSQ = 0;
    while (*content) {
      if (compareString(content, "SQ", CMP_BINARY) == 0) foundSQ = 1;
      content += strlen(content) + 1;
    }
    if (!foundSQ) {
      printf("    FAIL: 'SQ' not found in menuContent\n");
      fail = 1;
    }
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  } else {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: 200-byte token skipped, SQ found, no overflow\n");
  }
  return fail;
}

/* test_program_memory_no_overlap
 * Mutation: writeTestProgram directly manipulates freeMemoryRegions[0].sizeInBlocks
 * instead of using resizeProgramMemory, creating free-list fragments that overlap
 * with region 0 and trigger the overlap warning in freeListFree().
 * (§memory refactor: allocator consistency) */
static int test_program_memory_no_overlap(void)
{
  /* Program large enough to trigger resizeProgramMemory expansion */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Allocate and free some blocks through the proper API to exercise
   * the free list, then verify no overlap exists. */
  { void *blk1 = allocC47Blocks(2);
    void *blk2 = allocC47Blocks(3);
    if (!blk1 || !blk2) {
      printf("    FAIL: allocC47Blocks returned NULL\n");
      fail = 1;
    }
    else {
      freeC47Blocks(blk1, 2);
      freeC47Blocks(blk2, 3);
    }
  }

  /* Check free list for overlap: regions must be strictly ordered */
  { bool overlap = false;
    for (int32_t i = 1; i < numberOfFreeMemoryRegions && !overlap; i++) {
      uint32_t prevEnd = (uint32_t)freeMemoryRegions[i - 1].blockAddress +
      (uint32_t)freeMemoryRegions[i - 1].sizeInBlocks;
      if (prevEnd >= (uint32_t)freeMemoryRegions[i].blockAddress) {
        overlap = true;
      }
    }
    if (overlap) {
      printf("    FAIL: free memory regions overlap detected\n");
      fail = 1;
    }
  }

  cleanupTestProgram();
  if (!fail) {
    printf("    PASS: no free-list overlap after program memory resize + alloc/free\n");
  }
  return fail;
}

/* test_cleanup_no_overlap
 * Mutation: dict is freed AFTER restoreTestProgram collapses free regions,
 * so the dict's allocation falls inside restored region 0 and triggers
 * the overlap warning in freeListFree().
 * (§memory refactor: cleanup order) */
static int test_cleanup_no_overlap(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Allocate and free some blocks to exercise the free list */
  { void *blk1 = allocC47Blocks(2);
    void *blk2 = allocC47Blocks(3);
    if (!blk1 || !blk2) {
      printf("    FAIL: allocC47Blocks returned NULL\n");
      fail = 1;
    }
    else {
      freeC47Blocks(blk1, 2);
      freeC47Blocks(blk2, 3);
    }
  }

  /* cleanupTestProgram frees dict BEFORE restoring program memory.
   * After cleanup, free list should be consistent with no overlap. */
  cleanupTestProgram();

  /* Verify free list integrity */
  { bool overlap = false;
    int32_t n = numberOfFreeMemoryRegions;
    if (n < 1) {
      printf("    FAIL: no free memory regions after cleanup\n");
      fail = 1;
    }
    for (int32_t i = 1; i < n && !overlap; i++) {
      uint32_t prevEnd = (uint32_t)freeMemoryRegions[i - 1].blockAddress +
      (uint32_t)freeMemoryRegions[i - 1].sizeInBlocks;
      if (prevEnd >= (uint32_t)freeMemoryRegions[i].blockAddress) {
        overlap = true;
      }
    }
    if (overlap) {
      printf("    FAIL: free memory regions overlap after cleanup\n");
      fail = 1;
    }
  }

  if (!fail) {
    printf("    PASS: cleanup order produces consistent free list, no overlap\n");
  }
  return fail;
}

/* test_softmenu_trailing_null
 * Mutation: initVariableSoftmenu uses malloc instead of calloc, leaving the
 * trailing byte after the last menu name uninitialized.  Iteration that
 * expects a NUL-terminated string array can read garbage past the last name.
 * (§softmenu: zero-initialized allocation) */
static int test_softmenu_trailing_null(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  const uint8_t *cubeStep = beginOfProgramMemory + 4 + 16;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)cubeStep;

  /* R2-T5 item 5: pollute the heap allocator's same-size bin with non-zero
   * bytes before the builder's calloc(1, 4) (one name "SQ": numberOfBytes =
   * 1 + (2+1) = 4), so a malloc()-instead-of-calloc() mutation cannot pass
   * by accident on a freshly-mapped, already-zero page. */
  for (int p = 0; p < 8; p++) {
    void *junk = malloc(4);
    if (junk) {
      memset(junk, 0xA5, 4);
      free(junk);
    }
  }

  testInitVariableSoftmenu(22);

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1\n", dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  /* Verify the content is properly NUL-terminated: iterate through
   * all names, then confirm the byte immediately after is '\0'. */
  if (dynamicSoftmenu[22].menuContent && !fail) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int count = 0;
    while (*content && count < dynamicSoftmenu[22].numItems) {
      content += strlen(content) + 1;
      count++;
    }
    if (count != dynamicSoftmenu[22].numItems || *content != '\0') {
      printf("    FAIL: menu content not properly NUL-terminated (count=%d, byte=%d)\n",
      count, (unsigned char)*content);
      fail = 1;
    }
  }
  else if (!fail) {
    printf("    FAIL: menuContent is NULL\n");
    fail = 1;
  }

  /* Cleanup */
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
  }
  dynamicSoftmenu[22].numItems = 0;
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  if (!fail) {
    printf("    PASS: softmenu content properly NUL-terminated\n");
  }
  return fail;
}

/* test_e2_continuation_after_enter
 * Program: marker, source step, .END. (appended by writeTestProgram).
 * Cursor placed on .END. (where pemCloseAlphaInput leaves it after ENTER).
 * addStepInProgram(ITM_2) — pre-move skipped (isAtEndOfPrograms true on .END.),
 * predecessor = source step → Forth capture opens.
 * Escaping mutation: swap both call sites back to forthEntryStateAtCursor —
 * .END. derives false (not an ITM_FORTH step), E2 misses, RPN number entry. */
static int test_e2_continuation_after_enter(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',                                              /* * ; */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  /* Setup: cursor on .END. (where pemCloseAlphaInput leaves cursor after ENTER) */
  currentStep = beginOfProgramMemory + sizeof(prog);  /* .END. appended by writeTestProgram */
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 3;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_2);

  /* Assert: FLAG_ALPHA set (Forth capture opened) */
  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set — E2 missed (continuation failed)\n");
    fail = 1;
  }

  /* Assert: tam.function == ITM_FORTH */
  if (tam.function != ITM_FORTH) {
    printf("    FAIL: tam.function = %d, expected ITM_FORTH (%d)\n",
    (int)tam.function, ITM_FORTH);
    fail = 1;
  }

  /* Assert: capture buffer contains "2" */
  if (strcmp(forthTestCapText(), "2") != 0) {
    printf("    FAIL: cap text = '%s', expected '2'\n", forthTestCapText());
    fail = 1;
  }

  /* Cleanup: pemAlpha(ITM_ENTER) commits the step and closes capture itself;
   * closing here first would misroute ENTER into the empty-abort branch and
   * delete the step instead of committing it. */
  if (!fail) {
    extern void pemAlpha(int16_t item);
    pemAlpha(ITM_ENTER);

    /* Verify committed step is an ITM_FORTH source step with payload "2" */
    scanLabelsAndPrograms();
    uint8_t *committedStep = beginOfProgramMemory + 4 + 16;  /* after marker + source */
    if (*(committedStep + 0) != 0x8B || *(committedStep + 1) != 0x1A ||
    *(committedStep + 2) != 0xFD || *(committedStep + 3) != 1 ||
    *(committedStep + 4) != '2') {
      printf("    FAIL: committed step not ITM_FORTH '2' (got 0x%02X 0x%02X 0x%02X 0x%02X '%c')\n",
      *(committedStep+0), *(committedStep+1), *(committedStep+2),
      *(committedStep+3), *(committedStep+4));
      fail = 1;
    }
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  if (!fail) {
    printf("    PASS: E2 continuation after ENTER — Forth capture opened correctly\n");
  }
  return fail;
}

/* test_e2_not_inside_rpn_gap
 * Program: marker, source step, RPN step (ITM_sin), marker, .END.
 * Cursor ON the RPN step; addStepInProgram(ITM_2); pre-move puts currentStep
 * on the closing marker, predecessor = RPN step → no capture (FLAG_ALPHA clear).
 * Escaping mutation: a naive "always derive from predecessor of the PRE-move
 * cursor" (two steps back) — this case flips to capture. */
static int test_e2_not_inside_rpn_gap(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 1 (opening) */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',                                              /* * ; */
    0x4C,                                                             /* ITM_sin (RPN) */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 2 (closing) */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  /* Setup: cursor ON the RPN step (ITM_sin at offset 20) */
  currentStep = beginOfProgramMemory + 4 + 16;  /* ITM_sin */
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 3;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_2);

  /* Assert: FLAG_ALPHA NOT set (RPN step → RPN entry, no Forth capture) */
  if (getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA set — should NOT capture inside RPN gap\n");
    fail = 1;
  }

  /* Assert: number entry began (aimBuffer[0] = '+' sign prefix, aimBuffer[1] = '2' digit) */
  if (aimBuffer[0] != '+' || aimBuffer[1] != '2') {
    printf("    FAIL: aimBuffer[0] = '%c' (0x%02X), aimBuffer[1] = '%c' (0x%02X) — expected '+'/'2' for number entry\n",
    aimBuffer[0], (unsigned char)aimBuffer[0], aimBuffer[1], (unsigned char)aimBuffer[1]);
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  if (!fail) {
    printf("    PASS: E2 does not capture inside RPN gap — RPN entry preserved\n");
  }
  return fail;
}

/* FIX-3 tests: restrict Forth fallback to XEQ/XEQ.SKP only */

/* test_gto_word_errors
 * Mutation: remove the forthFallbackOp conjunct — GTO calls SQ, X changes,
 * no error is raised. */
static int test_gto_word_errors(void)
{
  uint16_t w = begin_word("SQ", 2);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(PRIM_TOKEN(P_MUL));
  end_word(w);

  int32_t xBefore;
  longInteger_t li;
  longIntegerInit(li);
  int32ToLongInteger(5, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
  longIntegerFree(li);
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
  longIntegerToInt32(li, xBefore);
  longIntegerFree(li);

  uint8_t step[] = { ITM_GTO, STRING_LABEL_VARIABLE, 2, 'S', 'Q' };
  lastErrorCode = ERROR_NONE;
  uint8_t savedRunStop = programRunStop;
  uint8_t *savedFirstFree = firstFreeProgramByte;
  programRunStop = PGM_RUNNING;
  firstFreeProgramByte = step + sizeof(step);
  executeOneStep(step);
  firstFreeProgramByte = savedFirstFree;
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_LABEL_NOT_FOUND %d)\n",
    lastErrorCode, ERROR_LABEL_NOT_FOUND);
    return 1;
  }
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
  longIntegerToInt32(li, xBefore);
  longIntegerFree(li);
  if (xBefore != 5) {
    printf("    FAIL: X changed to %d (expected 5, SQ was NOT called)\n", xBefore);
    return 1;
  }
  printf("    PASS: GTO 'SQ' raises ERROR_LABEL_NOT_FOUND, X unchanged\n");
  return 0;
}

/* test_gto_item_errors
 * Mutation: remove the forthFallbackOp conjunct — GTO 'FORTH' executes the
 * FORTH item (ITM_FORTH), no error is raised. */
static int test_gto_item_errors(void)
{
  int32_t xBefore;
  longInteger_t li;
  longIntegerInit(li);
  int32ToLongInteger(-7, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
  longIntegerFree(li);
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
  longIntegerToInt32(li, xBefore);
  longIntegerFree(li);

  uint8_t step[] = { ITM_GTO, STRING_LABEL_VARIABLE, 5, 'F', 'O', 'R', 'T', 'H' };
  lastErrorCode = ERROR_NONE;
  uint8_t savedRunStop = programRunStop;
  uint8_t *savedFirstFree = firstFreeProgramByte;
  programRunStop = PGM_RUNNING;
  firstFreeProgramByte = step + sizeof(step);
  executeOneStep(step);
  firstFreeProgramByte = savedFirstFree;
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_LABEL_NOT_FOUND %d)\n",
    lastErrorCode, ERROR_LABEL_NOT_FOUND);
    return 1;
  }
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
  longIntegerToInt32(li, xBefore);
  longIntegerFree(li);
  if (xBefore != -7) {
    printf("    FAIL: X changed to %d (expected -7, FORTH item was NOT called)\n", xBefore);
    return 1;
  }
  printf("    PASS: GTO 'FORTH' raises ERROR_LABEL_NOT_FOUND, X unchanged\n");
  return 0;
}

/* test_xeq_word_still_calls
 * Regression: XEQ 'SQ' with X=3 must produce X=9.
 * Mutation: over-tighten gate (drop ITM_XEQ) — this test fails. */
static int test_xeq_word_still_calls(void)
{
  uint16_t w = begin_word("SQ", 2);
  if (w == FORTH_NULL) {
    printf("    SKIP: alloc failed\n");
    return 0;
  }
  forthDictEmit(PRIM_TOKEN(P_DUP));
  forthDictEmit(PRIM_TOKEN(P_MUL));
  end_word(w);

  longInteger_t li;
  longIntegerInit(li);
  int32ToLongInteger(3, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
  longIntegerFree(li);

  uint8_t step[] = { ITM_XEQ, STRING_LABEL_VARIABLE, 2, 'S', 'Q' };
  lastErrorCode = ERROR_NONE;
  uint8_t savedRunStop = programRunStop;
  uint8_t *savedFirstFree = firstFreeProgramByte;
  programRunStop = PGM_RUNNING;
  firstFreeProgramByte = step + sizeof(step);
  executeOneStep(step);
  firstFreeProgramByte = savedFirstFree;
  programRunStop = savedRunStop;

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: lastErrorCode = %d (expected ERROR_NONE)\n", lastErrorCode);
    return 1;
  }
  if (!x_is_longint(9)) {
    printf("    FAIL: X != 9 after XEQ 'SQ'\n");
    return 1;
  }
  printf("    PASS: XEQ 'SQ' with X=3 -> X=9, colon word still called\n");
  return 0;
}

/* test_useritem_xeqp1_opcode
 * insertUserItemInProgram(ITM_XEQP1, "SQ2") — byte-probe the inserted step:
 * 0x88 0xAF 0xFD 0x03 'S' 'Q' '2'.
 * Escaping mutation: revert manage.c:1863 to & 0x7f — low byte becomes 0x2F. */
static int test_useritem_xeqp1_opcode(void)
{
  /* Minimal program */
  uint8_t prog[] = { 0x4C };  /* ITM_sin */
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Save state */
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);

  /* Setup for insertUserItemInProgram */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);

  extern void insertUserItemInProgram(int16_t func, char *funcParam);
  insertUserItemInProgram(ITM_XEQP1, "SQ2");

  /* Rescan to update pointers */
  probeListPtrs("pre-scan:4154");
  scanLabelsAndPrograms();

  /* Byte-probe: insertUserItemInProgram advances past ITM_sin (1 byte),
   * so the XEQP1 step starts at offset 1 */
  uint8_t *s = beginOfProgramMemory + 1;
  if (*s != 0x88 || *(s+1) != 0xAF || *(s+2) != 0xFD) {
    printf("    FAIL: step does not start with ITM_XEQP1 opcode (got 0x%02X 0x%02X 0x%02X)\n",
    *s, *(s+1), *(s+2));
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  uint8_t nameLen = *(s + 3);
  if (nameLen != 3) {
    printf("    FAIL: name length is %d, expected 3\n", nameLen);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  if (*(s + 4) != 'S' || *(s + 5) != 'Q' || *(s + 6) != '2') {
    printf("    FAIL: name is '%c%c%c', expected 'SQ2'\n", *(s+4), *(s+5), *(s+6));
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: ITM_XEQP1 opcode 0x88 0xAF preserved (low byte 0xAF, not 0x2F)\n");
  return 0;
}

/* test_useritem_xeqp1_decodes
 * F4 follow-through (DESIGN.md §8.10 item 4): the write side
 * (insertUserItemInProgram) is tested by test_useritem_xeqp1_opcode; this
 * test verifies the full insert -> decode/display path. decode.c's own
 * two-byte opcode reassembly (_decodeOneStep: op &= 0x7f; op <<= 8;
 * op |= *(step++);) already ORs in the low byte unmasked and is
 * byte-identical to upstream src/c47/programming/decode.c at that site
 * [VERIFIED: programming/decode.c, opCode reconstruction from first byte
 * through low-byte OR] -- no decode-
 * side bug exists. This test instead exercises manage.c's write-side fix
 * THROUGH decode: insertUserItemInProgram(ITM_XEQP1, "SQ2") writes the step,
 * decodeOneStep() reconstructs the opcode from those bytes and renders it,
 * and the rendered text must name ITM_XEQP1's catalog entry "XEQ.SKP"
 * [VERIFIED: src/c47/items.c:4033] via the PARAM_LABEL/STRING_LABEL_VARIABLE
 * rendering [VERIFIED: packages/forth-core/programming/decode.c:198-224],
 * i.e. "XEQ.SKP 'SQ2'" -- not the item at the corrupted opcode.
 * Escaping mutation: revert manage.c's mask to & 0x7f (the F4 regression) --
 * the written low byte becomes 0x2F, decode.c faithfully (and correctly)
 * reconstructs opcode 0x082F = 2095, whose catalog entry is the unrelated
 * unit-conversion item "rad/s->" [VERIFIED: src/c47/items.c:3905], so the
 * rendered-text assertion fails. */
static int test_useritem_xeqp1_decodes(void)
{
  /* Minimal program */
  uint8_t prog[] = { 0x4C };  /* ITM_sin */
  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* Save state */
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);

  /* Setup for insertUserItemInProgram (identical to test_useritem_xeqp1_opcode) */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);

  extern void insertUserItemInProgram(int16_t func, char *funcParam);
  insertUserItemInProgram(ITM_XEQP1, "SQ2");

  scanLabelsAndPrograms();

  int fail = 0;

  /* XEQP1 step starts at offset 1 (after ITM_sin) */
  uint8_t *s = beginOfProgramMemory + 1;
  decodeOneStep(s);

  char reference[32];
  sprintf(reference, "XEQ.SKP " STD_LEFT_SINGLE_QUOTE "SQ2" STD_RIGHT_SINGLE_QUOTE);

  if (strcmp(tmpString, reference) != 0) {
    printf("    FAIL: tmpString = '%s', expected '%s' (opcode not reconstructed as ITM_XEQP1=0x%04X)\n",
    tmpString, reference, ITM_XEQP1);
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  if (!fail) {
    printf("    PASS: XEQP1 user item decodes to '%s' (opcode 0x%04X)\n", reference, ITM_XEQP1);
  }
  return fail;
}

/* test_e1_direction_mid_program
 * Program: RPN step, marker(»), source, marker(«), END, .END.
 * Cursor ON the RPN step (predecessor semantics: insertion follows it,
 * before the »). addStepInProgram(ITM_FORTH) — E1 fires, predecessor = RPN
 * step → wasOn = false → a balanced marker pair and placeholder are inserted,
 * with capture open on the placeholder.
 * Escaping mutation: the at-cursor derivation (state from the old » = true)
 * suppresses the capture — assertion fails. */
static int test_e1_direction_mid_program(void)
{
  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin (RPN) */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 1 (») */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',                                              /* * ; */
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker 2 («) */
    0x85, 0xB2,                                                       /* ITM_END */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  /* Setup: cursor ON the RPN step (ITM_sin at offset 0) */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_FORTH);

  /* Assert: FLAG_ALPHA set (opening marker, capture opened) */
  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set — E1 should open capture (wasOn=false from RPN predecessor)\n");
    fail = 1;
  }

  /* Assert: new bracket and placeholder inserted between RPN and old marker. */
  scanLabelsAndPrograms();
  /* After insertion: RPN(1) + newOpen(4) + placeholder(4) + newClose(4)
   * + oldMarker(4) + source(16) + marker2(4) + END(2) + .END.(2). */
  uint8_t *newMarker = beginOfProgramMemory + 1;  /* right after ITM_sin */
  uint8_t *placeholder = newMarker + 4;
  uint8_t *autoClose = placeholder + 4;
  if (*(newMarker + 0) != 0x8B || *(newMarker + 1) != 0x1A ||
  *(newMarker + 2) != 0xFD || *(newMarker + 3) != 0x00) {
    printf("    FAIL: new marker not at expected position (got 0x%02X 0x%02X 0x%02X 0x%02X)\n",
    *(newMarker+0), *(newMarker+1), *(newMarker+2), *(newMarker+3));
    fail = 1;
  }
  else if (currentStep != placeholder || placeholder[0] != 0x8B ||
           placeholder[1] != 0x1A || placeholder[2] != 0xFD ||
           placeholder[3] != 0x00) {
    printf("    FAIL: new bracket placeholder not selected\n");
    fail = 1;
  }
  else if (autoClose[0] != 0x8B || autoClose[1] != 0x1A ||
           autoClose[2] != 0xFD || autoClose[3] != 0x00) {
    printf("    FAIL: automatic close missing before old marker\n");
    fail = 1;
  }

  /* Cleanup: close capture */
  if (!fail) {
    extern void pemAlpha(int16_t item);
    /* Close the Forth capture by entering empty (E3: should delete placeholder) */
    pemAlpha(ITM_ENTER);
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  if (!fail) {
    printf("    PASS: E1 direction mid-program — balanced bracket inserted, capture opened inside it\n");
  }
  return fail;
}

/* test_forth_multiline_lock_holds
 * §8.4 E5: committing a NON-EMPTY Forth source line with ENTER while the
 * cursor is still inside an open region must re-open capture on the next
 * line — FLAG_ALPHA set and tam.function == ITM_FORTH. ENTER drops to the
 * next Forth line; it does not leave the region.
 *
 * This test formerly asserted the opposite (tam.function != ITM_FORTH,
 * "no stale sentinel"). That was the pre-E5 contract, under which the region
 * became unreachable after the first ENTER — the exact hardware defect E5
 * exists to fix. The anti-leak concern it guarded is real but lives on the
 * EMPTY-ENTER escape hatch instead, where the sentinel genuinely must clear;
 * test_forth_empty_enter_leaves_no_step pins that.
 *
 * Escaping mutation: drop the `hadText` term from the E5 condition — the lock
 * then also fires on the empty escape hatch and E3's test fails. */
static int test_forth_multiline_lock_holds(void)
{
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* marker */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P', /* : SQ DUP */
    ' ', '*', ' ', ';',                                              /* * ; */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  /* Setup: cursor on .END. (where pemCloseAlphaInput leaves cursor after ENTER) */
  currentStep = beginOfProgramMemory + sizeof(prog);  /* .END. appended by writeTestProgram */
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 3;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_2);   /* E2 continuation: opens Forth capture, tam.function = ITM_FORTH */

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
    printf("    FAIL: setup did not open Forth capture (FLAG_ALPHA=%d tam.function=%d)\n",
    (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
    fail = 1;
  }

  if (!fail) {
    extern void pemAlpha(int16_t item);
    pemAlpha(ITM_ENTER);   /* commit "2" as a Forth source line, inside the region */

    /* E5: the cursor is still inside the open region, so capture must re-open. */
    if (tam.function != ITM_FORTH) {
      printf("    FAIL: tam.function = %d after committing a non-empty Forth line "
             "inside an open region, expected ITM_FORTH (E5 lock did not hold)\n",
             (int)tam.function);
      fail = 1;
    }
    if (!getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA clear after E5 re-open — the alpha layout is "
             "what makes letter keys reachable; the region would be stranded\n");
      fail = 1;
    }
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  if (!fail) {
    printf("    PASS: E5 multi-line lock holds — non-empty ENTER inside a region "
           "re-opens capture (FLAG_ALPHA set, tam.function == ITM_FORTH)\n");
  }
  return fail;
}

/* test_tam_function_cleared_after_abort
 * The sentinel-clear invariant, but for the
 * abort path: opening the capture (addStepInProgram(ITM_FORTH), as in
 * test_toggle_inserts_marker's opening case) leaves aimBuffer empty, then
 * pemAlpha(ITM_BACKSPACE) with an empty buffer is the abort/EXIT gesture
 * (manage.c:883-897) — it deletes the placeholder step and clears
 * FLAG_ALPHA. Assert tam.function != ITM_FORTH afterward.
 * Escaping mutation: remove the `tam.function = 0;` reset added in the
 * ITM_BACKSPACE abort branch (manage.c:889-896) — the assertion fails. */
static int test_tam_function_cleared_after_abort(void)
{
  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin (RPN) */
    0x85, 0xB2,                                                       /* ITM_END */
  };
  int fail = 0;

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunction = tam.function;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  /* Setup: cursor on ITM_END (offset 1) — pre-move skipped, predecessor RPN
   * step → wasOn=false → capture opens, aimBuffer stays empty */
  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_FORTH);

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH || aimBuffer[0] != 0) {
    printf("    FAIL: setup did not open an empty Forth capture (FLAG_ALPHA=%d tam.function=%d aimBuffer[0]=%d)\n",
    (int)getSystemFlag(FLAG_ALPHA), (int)tam.function, (int)aimBuffer[0]);
    fail = 1;
  }

  if (!fail) {
    extern void pemAlpha(int16_t item);
    pemAlpha(ITM_BACKSPACE);   /* abort/EXIT gesture: empty buffer, deletes placeholder */

    if (tam.function == ITM_FORTH) {
      printf("    FAIL: tam.function == ITM_FORTH after capture abort (stale sentinel survived EXIT)\n");
      fail = 1;
    }
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));

  if (!fail) {
    printf("    PASS: tam.function != ITM_FORTH after capture abort\n");
  }
  return fail;
}

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
/* code-audit: dynamic-menu XEQ of a Forth word/colon must insert in PEM, not execute live */
static int test_pem_xeq_dynmenu_no_live_exec(void);
/* code-audit (adversarial): edit an existing Forth line, MODIFY it, re-commit via ENTER */
static int test_forth_edit_modify_commit(void);
/* code-audit: PEM Up/Down must commit the managed Forth sink before moving */
static int test_forth_capture_navigation(void);
/* complete user-facing language showcase from FORTH_SHOWCASE_PROGRAM.txt */
static int test_showcase_program(void);

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

/* T1.1 (H5 round-trip). Must fail if: any of the five forthGDict* params is
 * dropped from the save or restore hunk, or the restore rebases gdict.base
 * without TO_PCMEMPTR. */
static int test_save_restore_roundtrip(void)
{
  int fail = 0;
  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;

  /* Hand-build GW1 (body: ILIT 7 EXIT) and GW2 (body: CALL GW1, ILIT 35 EXIT) */
  {
    uint16_t gw1 = gbegin_word("GW1", 3);
    if (gw1 == FORTH_NULL) { printf("    FAIL: gbegin_word GW1\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 7; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();

    uint16_t gw2 = gbegin_word("GW2", 3);
    if (gw2 == FORTH_NULL) { printf("    FAIL: gbegin_word GW2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call GW1 (global index 0) */
    gemit(T_ILIT);
    { int32_t v = 35; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
  }

  uint16_t savedHere = gdict.here, savedCount = gdict.count, savedLatest = gdict.latest;

  saveCalc();
  if (!backupFileContains("forthGDictBase:")) {
    printf("    FAIL: saveCalc did not write forthGDictBase\n");
    forthGDictClear();
    return 1;
  }
  if (backupFileContains("forthDictBase:")) {
    printf("    FAIL: saveCalc still writes forthDictBase (old keys must be gone)\n");
    forthGDictClear();
    return 1;
  }

  /* clobber gdict; define a transient word */
  forthGDictClear();
  forthOuterInterpret(": SRZZ 1 ;");

  {
    bool_t savedLoad = loadTestPrograms;
    loadTestPrograms = false;
    restoreCalc();
    loadTestPrograms = savedLoad;
  }

  /* gdict scalars match saved */
  if (gdict.here != savedHere || gdict.count != savedCount || gdict.latest != savedLatest) {
    printf("    FAIL: gdict scalars mismatch (here %u/%u count %u/%u latest %u/%u)\n",
           gdict.here, savedHere, gdict.count, savedCount, gdict.latest, savedLatest);
    fail = 1;
  }
  /* fdict is EMPTY after restore */
  if (fdict.count != 0) {
    printf("    FAIL: fdict.count != 0 after restore (%u)\n", fdict.count);
    fail = 1;
  }
  { uint16_t r;
    if (forthFindColon("SRZZ", &r)) {
      printf("    FAIL: post-save transient word SRZZ survived restore\n"); fail = 1; }
  }
  /* GW2 found as global ref */
  { uint16_t ref;
    if (!forthFindColon("GW2", &ref)) {
      printf("    FAIL: GW2 lost across restore\n"); fail = 1;
    } else if (ref != (FORTH_REF_GLOBAL | 1)) {
      printf("    FAIL: GW2 ref = 0x%04X, expected 0x%04X\n", ref, (unsigned)(FORTH_REF_GLOBAL | 1));
      fail = 1;
    }
  }

  /* Execute GW2: drives global FTOK_CALL arm and cross-region body resolution */
  if (!fail) {
    uint16_t ref;
    if (forthFindColon("GW2", &ref)) {
      lastErrorCode = ERROR_NONE;
      forthInner(ref, false);
      if (lastErrorCode != ERROR_NONE) { printf("    FAIL: GW2 raised %d\n", lastErrorCode); fail = 1; }
      else if (!x_is_longint(35))      { printf("    FAIL: X != 35 after GW2\n"); fail = 1; }
    }
  }

  /* A5 / backupR47.cfg regression: forthCap is deliberately not persisted,
   * but older backups do persist the surrounding PEM + ALPHA + ITM_FORTH UI
   * and its cursor.  Restore must close that split state before the exact
   * reported RRRLLLRRL sequence reaches the text cursor. */
  {
    uint8_t savedCalcMode = calcMode;
    int16_t savedTamFunction = tam.function;
    bool_t savedAlphaUi = getSystemFlag(FLAG_ALPHA);
    int16_t savedCursor = T_cursorPos;
    int16_t savedDisplayOffset = displayAIMbufferoffset;
    softmenuStack_t savedUiStack[SOFTMENU_STACK_SIZE];
    char savedAimBuffer[AIM_BUFFER_LENGTH];
    static const uint16_t restoredArrows[] = {
      ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW,
      ITM_T_LEFT_ARROW,  ITM_T_LEFT_ARROW,  ITM_T_LEFT_ARROW,
      ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW, ITM_T_LEFT_ARROW
    };
    uint_fast16_t i;

    xcopy(savedUiStack, softmenuStack, sizeof(savedUiStack));
    xcopy(savedAimBuffer, aimBuffer, sizeof(savedAimBuffer));

    forthCapPowerReset();
    calcMode = CM_PEM;
    showSoftmenu(-MNU_ALPHA);
    setSystemFlag(FLAG_ALPHA);
    tam.function = ITM_FORTH;
    aimBuffer[0] = 0;
    T_cursorPos = 2;
    displayAIMbufferoffset = 0;
    saveCalc();

    calcMode = CM_NORMAL;
    clearSystemFlag(FLAG_ALPHA);
    tam.function = 0;
    T_cursorPos = 0;
    {
      bool_t savedLoad = loadTestPrograms;
      loadTestPrograms = false;
      restoreCalc();
      loadTestPrograms = savedLoad;
    }

    if (calcMode != CM_PEM
        || getSystemFlag(FLAG_ALPHA)
        || tam.function != 0
        || forthCapIsOpen()
        || forthCapIsSuspended()
        || T_cursorPos != 0
        || displayAIMbufferoffset != 0
        || currentMenu() == -MNU_ALPHA
        || isAlphaSubmenu(0)) {
      printf("    FAIL: stale restored capture UI survived "
             "(mode=%u alpha=%d tam=%d state=%d cursor=%d off=%d menu=%d)\n",
             calcMode, getSystemFlag(FLAG_ALPHA), tam.function,
             forthTestCapState(), T_cursorPos, displayAIMbufferoffset,
             currentMenu());
      fail = 1;
    }

    for (i = 0; i < nbrOfElements(restoredArrows); ++i) {
      fnT_ARROW(restoredArrows[i]);
    }
    if (T_cursorPos != 0 || displayAIMbufferoffset != 0) {
      printf("    FAIL: restored arrow replay moved empty cursor "
             "(cursor=%d off=%d)\n", T_cursorPos, displayAIMbufferoffset);
      fail = 1;
    }

    forthCapPowerReset();
    calcMode = savedCalcMode;
    tam.function = savedTamFunction;
    if (savedAlphaUi) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    T_cursorPos = savedCursor;
    displayAIMbufferoffset = savedDisplayOffset;
    xcopy(softmenuStack, savedUiStack, sizeof(savedUiStack));
    xcopy(aimBuffer, savedAimBuffer, sizeof(savedAimBuffer));
  }

  /* Release the restored gdict region (restore allocated fresh memory) */
  { uint8_t *preBase = gdict.base; uint16_t preBlocks = gdict.sizeBlocks;
    gdict.base = NULL; gdict.sizeBlocks = 0; gdict.here = 0;
    gdict.latest = FORTH_NULL; gdict.count = 0;
    if (preBase) freeC47Blocks(preBase, preBlocks);
  }
  forthDictClear();

  printf("  FORTH ARENA (post-restore): here=%u sizeBlocks=%u gdict here=%u sizeBlocks=%u\n",
         fdict.here, fdict.sizeBlocks, gdict.here, gdict.sizeBlocks);
  if (!fail) printf("    PASS: save/restore round-trip preserved the dictionary\n");
  return fail;
}

/* T1.2 (old-backup defaults). Must fail if: the pre-seeded defaults before
 * each restoreStateValue call are removed (stale ramPtr from the programList
 * restore would masquerade as the gdict base). */
static int test_restore_missing_params_defaults(void)
{
  int fail = 0;
  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;
  /* Build a gdict word for save */
  { uint16_t gw = gbegin_word("GW", 2); if (gw == FORTH_NULL) { printf("    FAIL: gbegin_word GW\n"); return 1; } gend_word(); }
  uint8_t *preBase = gdict.base;
  uint16_t preBlocks = gdict.sizeBlocks;

  saveCalc();
  if (!backupFileContains("forthGDictBase:")) { printf("    FAIL: save missing params\n"); forthGDictClear(); return 1; }
  if (editBackupFile("forthGDict", NULL, NULL)) { printf("    FAIL: file edit failed\n"); forthGDictClear(); return 1; }

  { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

  if (gdict.base != NULL || gdict.latest != FORTH_NULL || gdict.count != 0
      || gdict.sizeBlocks != 0 || gdict.here != 0) {
    printf("    FAIL: missing params did not default to empty gdict\n");
    fail = 1;
  }
  /* fdict also empty after restore */
  if (fdict.count != 0) {
    printf("    FAIL: fdict.count != 0 after restore (%u)\n", fdict.count);
    fail = 1;
  }
  /* The restored arena still carries the pre-save gdict region, now orphaned
   * by design (params stripped). Release it to balance the leak gate. */
  if (preBase) freeC47Blocks(preBase, preBlocks);

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": NEWW 3 ;");
  { uint16_t r; if (lastErrorCode != ERROR_NONE || !forthFindColon("NEWW", &r)) {
      printf("    FAIL: lazy alloc broken after defaulted restore\n"); fail = 1; } }
  forthGDictClear();
  forthDictClear();
  if (!fail) printf("    PASS: stripped params default to empty dict\n");
  return fail;
}

/* T1.3 (validation clamps corruption). Must fail if:
 * forthGDictValidateRestored is not called from the restore hunk, or its
 * here-bound / chain-count checks are deleted (next dict write would land
 * out of bounds). */
static int test_restore_validation_clamps(void)
{
  int fail = 0;
  int variant;
  for (variant = 0; variant < 2; variant++) {
    forthDictClear();
    forthGDictClear();
    /* Build a gdict word for save */
    { uint16_t gw = gbegin_word("GW", 2); if (gw == FORTH_NULL) { printf("    FAIL: gbegin_word GW\n"); return 1; } gend_word(); }
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;

    saveCalc();
    if (variant == 0) {
      if (editBackupFile(NULL, "forthGDictHere:", "forthGDictHere:uint16:65534\n")) { printf("    FAIL: edit\n"); return 1; }
    } else {
      char repl[64];
      sprintf(repl, "forthGDictCount:uint16:%u\n", (unsigned)gdict.count + 1);
      if (editBackupFile(NULL, "forthGDictCount:", repl)) { printf("    FAIL: edit\n"); return 1; }
    }

    { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

    if (gdict.base != NULL) {
      printf("    FAIL: variant %d: corrupt scalars survived validation\n", variant);
      fail = 1;
      forthGDictClear();     /* free whatever it points at, best effort */
    }
    else if (preBase) {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VOKW 5 ;");
    { uint16_t r; if (lastErrorCode != ERROR_NONE || !forthFindColon("VOKW", &r)) {
        printf("    FAIL: variant %d: dict unusable after validation reset\n", variant); fail = 1; } }
    forthGDictClear();
    forthDictClear();
  }

  return fail;
}

/* T1.3b (validator direct pins). V1 must fail if the sizeBlocks!=0 check is
 * removed (stale base + zeroed scalars passes every other check: cap=0,
 * here=0<=0, latest=FORTH_NULL skips the walk, n=0==count).
 * V2 must fail if the nameLen bounds check is removed (a zeroed nameLen
 * header walks clean through the off/link/count checks).
 * V3 (R4-3) must fail if the off+4+nameLen<=here extent check is removed: the
 * validator proved off+4<=here (the HEADER fits) and 1<=nameLen<=31, but never
 * proved the NAME that follows the header also fits inside here. Probed: a
 * valid ": VX 1 ;" entry with here force-set to latest+6 (header fits, name
 * does not) survived validation before this fix. */
static int test_validate_direct_corruption(void)
{
  int fail = 0;

  /* V1: stale base with zeroed scalars */
  {
    uint8_t *region = allocC47Blocks(4);
    if (!region) { printf("    SKIP: alloc failed\n"); return 0; }
    forthGDictClear();
    gdict.base = region;             /* simulate stale-pointer restore */
    gdict.sizeBlocks = 0;
    gdict.here = 0;
    gdict.latest = FORTH_NULL;
    gdict.count = 0;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL: V1 stale base with zeroed scalars survived validation\n");
      fail = 1;
      forthGDictClear();              /* best effort */
    }
    freeC47Blocks(region, 4);        /* release the deliberate orphan */
  }

  /* V2: corrupt nameLen on a real header */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VD2", 3);
    if (w == FORTH_NULL) { printf("    SKIP: V2 setup failed\n"); return fail; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    ((forthHeader_t *)(gdict.base + gdict.latest))->nameLen = 0;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL: V2 zero-nameLen header survived validation\n");
      fail = 1;
      forthGDictClear();
    }
    else {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }
  }

  /* V3: header fits (off+4<=here) but the name after it runs past here */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VD3", 3);
    if (w == FORTH_NULL) { printf("    SKIP: V3 setup failed\n"); return fail; }
    gend_word();
    uint8_t *savedBase = gdict.base;
    uint16_t savedBlocks = gdict.sizeBlocks;
    gdict.here = gdict.latest + 6;   /* header fits; nameLen bytes now run past here */
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL: V3 header name extending past here survived validation\n");
      fail = 1;
      forthGDictClear();
    }
    else {
      freeC47Blocks(savedBase, savedBlocks);  /* release the deliberate orphan */
    }
  }

  forthGDictClear();
  if (!fail) printf("    PASS: validator direct pins (sizeBlocks, nameLen, name-extent)\n");
  return fail;
}

/* test_scope_isolation
 * F3-3: definitions are scope-owned and lookup honors the owner.
 * Five subcases covering scope isolation, cross-program rejection,
 * interactive/program mutual invisibility, scope restore, and global
 * word visibility. */
static int test_scope_isolation(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* Build fixture: program A | program B */
  testProg_t tp;
  tpInit(&tp);
  int sLblA  = tpLbl(&tp, "PA");
  int sDefA  = tpSrc(&tp, ": WA 41 ;");
  int sUseA  = tpSrc(&tp, "WA");
  int sUseWI = tpSrc(&tp, "WI");
  (void)tpEnd(&tp);
  int sLblB  = tpLbl(&tp, "PB");
  int sDefB  = tpSrc(&tp, ": WB 42 ;");
  int sUseB  = tpSrc(&tp, "WA 1 +");
  int sXeqA  = tpXeqName(&tp, "WA");
  if (sLblA < 0 || sDefA < 0 || sUseA < 0 || sUseWI < 0 ||
      sLblB < 0 || sDefB < 0 || sUseB < 0 || sXeqA < 0 || !tpWrite(&tp)) {
    printf("    FAIL: fixture build/write failed\n");
    programRunStop = savedRS;
    return 1;
  }

  /* ---- Subcase 1: Program A derives and uses its own word ---- */
  {
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    programRunStop = PGM_RUNNING;
    forthRunGenBump();
    currentStep = tpStepAddr(&tp, sDefA);
    executeOneStep(currentStep);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: def step error %d\n", lastErrorCode);
      fail = 1;
    } else {
      currentStep = tpStepAddr(&tp, sUseA);
      executeOneStep(currentStep);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: use step error %d\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(41)) {
        printf("    [1] FAIL: X != 41\n");
        fail = 1;
      } else {
        printf("    [1] PASS: program word resolves in its own scope\n");
      }
    }
  }

  /* ---- Subcase 2: Program B cannot see A's word ---- */
  {
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    programRunStop = PGM_RUNNING;
    currentStep = tpStepAddr(&tp, sDefB);
    executeOneStep(currentStep);
    if (lastErrorCode == ERROR_NONE) {
      forthPushInt32(77);
      currentStep = tpStepAddr(&tp, sUseB);
      executeOneStep(currentStep);
      if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
        printf("    [2] FAIL: expected ERROR_FUNCTION_NOT_FOUND, got %d (X=%s)\n",
               lastErrorCode, x_is_longint(77) ? "77" : "changed");
        fail = 1;
      } else if (!x_is_longint(77)) {
        printf("    [2] FAIL: X changed from sentinel 77\n");
        fail = 1;
      } else {
        printf("    [2] PASS: cross-program lookup rejected\n");
      }
    } else {
      printf("    [2] FAIL: defB step error %d\n", lastErrorCode);
      fail = 1;
    }
    if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
      printf("    [2] FAIL: scope not restored after error drive (got %u)\n",
             forthCurrentScopeGet());
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;
  }

  /* ---- Subcase 3: Interactive and program scopes are mutually invisible ---- */
  {
    int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": WI 7 ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: WI definition error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("WA");
      if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
        printf("    [3a] FAIL: interactive saw WA (got %d)\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      bool err = run_word("WI");
      if (err || !x_is_longint(7)) {
        printf("    [3b] FAIL: WI failed or X != 7\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      dynamicMenuItem = -1;
      programRunStop = PGM_RUNNING;
      currentStep = tpStepAddr(&tp, sUseWI);
      executeOneStep(currentStep);
      if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
        printf("    [3c] FAIL: program saw WI (got %d)\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) {
      printf("    [3] PASS: interactive and program scopes are mutually invisible\n");
    } else {
      fail = 1;
    }
  }

  /* ---- Subcase 4: Scope restores to INTERACTIVE after every drive ---- */
  {
    if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
      printf("    [4] FAIL: scope not INTERACTIVE after all drives (got %u)\n",
             forthCurrentScopeGet());
      fail = 1;
    } else {
      printf("    [4] PASS: current scope restored to interactive\n");
    }
  }

  /* ---- Subcase 5: Global word visible and callable from transient scope ---- */
  {
    int subFail = 0;
    uint16_t gw = gbegin_word("GVIS", 4);
    if (gw == FORTH_NULL) {
      printf("    [5] FAIL: gbegin_word GVIS failed\n");
      subFail = 1;
    } else {
      gemit(T_ILIT);
      { int32_t v = 9;
        if (!forthGDictEnsure(4)) {
          printf("    [5] FAIL: forthGDictEnsure for int32\n");
          subFail = 1;
        } else {
          memcpy(gdict.base + gdict.here, &v, 4);
          gdict.here += 4;
        }
      }
      if (!subFail) {
        gemit(T_EXIT);
        gdict.here = (uint16_t)TO_BLOCKS(gdict.here) * BYTES_PER_BLOCK;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("GVIS");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: GVIS error %d\n", lastErrorCode);
        subFail = 1;
      } else if (!x_is_longint(9)) {
        printf("    [5] FAIL: X != 9 after GVIS\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": WG GVIS 1 + ;");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: WG definition error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      bool err = run_word("WG");
      if (err || !x_is_longint(10)) {
        printf("    [5] FAIL: WG failed or X != 10\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [5] PASS: global word visible and callable from transient scope\n");
    } else {
      fail = 1;
    }
  }

  /* [6] cross-program XEQ-name step: B's XEQ 'WA' must reject in B's scope */
  {
    uint8_t savedRS6 = savedRS;
    forthPushInt32(88);
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    programRunStop = PGM_RUNNING;
    currentStep = tpStepAddr(&tp, sXeqA);
    executeOneStep(currentStep);
    programRunStop = savedRS6;
    if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
      printf("    [6] FAIL: expected ERROR_LABEL_NOT_FOUND, got %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(88)) {
      printf("    [6] FAIL: X changed across rejected XEQ\n");
      fail = 1;
    }
    else if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
      printf("    [6] FAIL: scope not restored after rejected XEQ step\n");
      fail = 1;
    }
    else {
      printf("    [6] PASS: cross-program XEQ-name step rejected in the step's scope\n");
    }
    lastErrorCode = ERROR_NONE;
  }

  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();
  lastErrorCode = ERROR_NONE;
  programRunStop = savedRS;
  return fail;
}

/* test_global_marks
 * F3-4: GLOBAL/IMMEDIATE/FORGET with same-line mark discipline.
 * Eleven subcases covering GLOBAL move, same-line discipline, transient-call
 * rejection, RECURSE rewrite, IMMEDIATE compile-time execution, FORGET
 * truncation, pre-scan IMMEDIATE carve-out, global+IMMEDIATE persistence,
 * a variable-width native cell before a promoted self-call, and strict
 * rejection of malformed inline parameter cells, and persistent ITM_NULL. */
static int test_global_marks(void)
{
  int fail = 0;

  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;

  /* [1] GLOBAL moves the same-line definition */
  {
    forthOuterInterpret(": GA 5 ; GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: GLOBAL produced error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t ref;
      if (!forthFindColon("GA", &ref) || ref != (FORTH_REF_GLOBAL | 0)) {
        printf("    [1] FAIL: GA not found or not global (ref=%u)\n", ref);
        fail = 1;
      } else if (fdict.count != 0) {
        printf("    [1] FAIL: fdict.count != 0 (got %u)\n", fdict.count);
        fail = 1;
      } else if (gdict.count != 1) {
        printf("    [1] FAIL: gdict.count != 1 (got %u)\n", gdict.count);
        fail = 1;
      } else {
        lastErrorCode = ERROR_NONE;
        if (run_word("GA") || !x_is_longint(5)) {
          printf("    [1] FAIL: GA did not leave X=5\n");
          fail = 1;
        } else {
          forthOuterInterpret(": TB GA 1 + ;");
          lastErrorCode = ERROR_NONE;
          if (run_word("TB") || !x_is_longint(6)) {
            printf("    [1] FAIL: TB did not leave X=6\n");
            fail = 1;
          } else {
            printf("    [1] PASS: same-line GLOBAL moved the definition to gdict\n");
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [2] Same-line discipline */
  {
    forthOuterInterpret("GLOBAL");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [2] FAIL: bare GLOBAL did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": GC 1 ;");
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("GLOBAL");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [2] FAIL: separate-line GLOBAL did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;
    { uint16_t ref;
      if (forthFindColon("GC", &ref) && (ref & FORTH_REF_GLOBAL)) {
        printf("    [2] FAIL: GC became global despite separate-line GLOBAL\n");
        fail = 1;
      } else {
        printf("    [2] PASS: GLOBAL requires a definition closed on the same line\n");
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [3] A global may not call a transient */
  {
    forthOuterInterpret(": TD 3 ;");
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": GE TD ; GLOBAL");
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [3] FAIL: GE calling transient TD did not error (got %d)\n", lastErrorCode);
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;
    { uint16_t ref;
      if (forthFindColon("GE", &ref)) {
        if (ref & FORTH_REF_GLOBAL) {
          printf("    [3] FAIL: GE became global despite transient call\n");
          fail = 1;
        } else {
          printf("    [3] PASS: transient-calling body refused GLOBAL\n");
        }
      } else {
        printf("    [3] FAIL: GE not found\n");
        fail = 1;
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [4] Self-call rewrite */
  {
    forthOuterInterpret(": GR 1 RECURSE ; GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: GR RECURSE GLOBAL produced error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t off = gdict.latest;
      uint16_t bodyStart = off + (uint16_t)TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK;
      /* Token layout: ILIT(2) payload(4) RECURSE(2) EXIT(2) */
      /* RECURSE token at bodyStart + 2 + 4 */
      ftoken_t tok;
      memcpy(&tok, gdict.base + bodyStart + 6, 2);
      if (tok != (FORTH_GCALL_BASE + 1)) {
        printf("    [4] FAIL: RECURSE not rewritten (tok=0x%04X, expected 0x%04X)\n",
               tok, FORTH_GCALL_BASE + 1);
        fail = 1;
      } else {
        printf("    [4] PASS: RECURSE self-call rewritten to the global index\n");
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [5] IMMEDIATE honored by the compiler */
  {
    forthOuterInterpret(": GI 2 ; IMMEDIATE");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: IMMEDIATE produced error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t ref; uint8_t fl;
      if (!forthFindColonRef("GI", &ref, &fl) || !(fl & FF_IMMEDIATE)) {
        printf("    [5] FAIL: GI not marked IMMEDIATE\n");
        fail = 1;
      } else {
        forthPushInt32(0);
        forthOuterInterpret(": TU GI ;");
        if (lastErrorCode != ERROR_NONE) {
          printf("    [5] FAIL: TU compilation errored (%d)\n", lastErrorCode);
          fail = 1;
        } else if (!x_is_longint(2)) {
          printf("    [5] FAIL: X != 2 after TU compilation\n");
          fail = 1;
        } else {
          /* Verify TU body is empty: first body token == FTOK_EXIT */
          uint16_t tuOff = fdict.latest;
          forthHeader_t *tuHdr = (forthHeader_t *)(fdict.base + tuOff);
          uint16_t tuBody = tuOff + (uint16_t)TO_BLOCKS(6 + tuHdr->nameLen) * BYTES_PER_BLOCK;
          ftoken_t tuTok;
          memcpy(&tuTok, fdict.base + tuBody, 2);
          if (tuTok != FTOK_EXIT) {
            printf("    [5] FAIL: TU body not empty (tok=0x%04X)\n", tuTok);
            fail = 1;
          } else {
            printf("    [5] PASS: immediate colon word executed at compile time\n");
          }
        }
        lastErrorCode = ERROR_NONE;
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [6] FORGET truncates from the named word */
  {
    forthOuterInterpret(": G1 1 ; GLOBAL");
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": G2 2 ; GLOBAL");
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": G3 3 ; GLOBAL");
    lastErrorCode = ERROR_NONE;
    if (gdict.count != 5) {
      printf("    [6] FAIL: gdict.count != 5 before FORGET (got %u)\n", gdict.count);
      fail = 1;
    } else {
      forthOuterInterpret("FORGET G2");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [6] FAIL: FORGET G2 errored (%d)\n", lastErrorCode);
        fail = 1;
      } else if (gdict.count != 3) {
        printf("    [6] FAIL: gdict.count != 3 after FORGET G2 (got %u)\n", gdict.count);
        fail = 1;
      } else {
        uint16_t r;
        bool g2 = forthFindColon("G2", &r);
        bool g3 = forthFindColon("G3", &r);
        bool g1 = forthFindColon("G1", &r);
        if (g2 || g3 || !g1) {
          printf("    [6] FAIL: FORGET lookup wrong (G2=%d G3=%d G1=%d)\n", g2, g3, g1);
          fail = 1;
        } else {
          forthOuterInterpret("FORGET TD");
          if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
            printf("    [6] FAIL: FORGET transient TD did not give ERROR_FUNCTION_NOT_FOUND\n");
            fail = 1;
          }
          lastErrorCode = ERROR_NONE;
          forthOuterInterpret("FORGET ZZQQ");
          if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
            printf("    [6] FAIL: FORGET ZZQQ did not give ERROR_FUNCTION_NOT_FOUND\n");
            fail = 1;
          }
          lastErrorCode = ERROR_NONE;
          if (!fail) {
            printf("    [6] PASS: FORGET truncated the global scope at the named word\n");
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [7] Marks apply on the pre-scan (program context) */
  {
    testProg_t tp; tpInit(&tp);
    int sL   = tpLbl(&tp, "PM");
    int sMi  = tpSrc(&tp, ": MI 3 ; IMMEDIATE");
    int sMu  = tpSrc(&tp, ": MU MI ;");
    int sTail= tpSrc(&tp, "MU");
    if (sL < 0 || sMi < 0 || sMu < 0 || sTail < 0 || !tpWrite(&tp)) {
      printf("    [7] FAIL: fixture build/write failed\n");
      fail = 1;
    } else {
      forthRunGenBump();
      uint8_t *step = tpStepAddr(&tp, sMi);
      executeOneStep(step);
      /* MI compiled and marked immediate; MU compiled with MI executed at compile time */
      forthPushInt32(55);
      step = tpStepAddr(&tp, sTail);
      executeOneStep(step);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [7] FAIL: tail step errored (%d)\n", lastErrorCode);
        fail = 1;
      } else if (!x_is_longint(55)) {
        printf("    [7] FAIL: X != 55 (MU body not empty — X=%d)\n", x_is_longint(0) ? 0 : -1);
        fail = 1;
      } else {
        printf("    [7] PASS: IMMEDIATE applied during the pre-scan pass\n");
      }
      lastErrorCode = ERROR_NONE;
      cleanupTestProgram();
    }
  }

  /* [8] Global persistence with flags */
  {
    forthOuterInterpret(": GJ 4 ; IMMEDIATE GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [8] FAIL: GJ IMMEDIATE GLOBAL errored (%d)\n", lastErrorCode);
      fail = 1;
    } else {
      /* Clear fdict before save so restoreCalc does not restore an orphaned fdict region */
      forthDictClear();
      saveCalc();
      forthGDictClear();
      { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }
      uint16_t ref; uint8_t fl;
      if (!forthFindColonRef("GJ", &ref, &fl)) {
        printf("    [8] FAIL: GJ not found after restore\n");
        fail = 1;
      } else if (!(ref & FORTH_REF_GLOBAL)) {
        printf("    [8] FAIL: GJ not global after restore\n");
        fail = 1;
      } else if (!(fl & FF_IMMEDIATE)) {
        printf("    [8] FAIL: GJ lost IMMEDIATE flag after restore\n");
        fail = 1;
      } else {
        lastErrorCode = ERROR_NONE;
        if (run_word("GJ") || !x_is_longint(4)) {
          printf("    [8] FAIL: GJ did not leave X=4 after restore\n");
          fail = 1;
        } else {
          printf("    [8] PASS: global word and its IMMEDIATE flag survive restore\n");
        }
      }
      lastErrorCode = ERROR_NONE;
      /* Release the restored gdict region (restore allocated fresh memory) */
      { uint8_t *rBase = gdict.base; uint16_t rBlocks = gdict.sizeBlocks;
        gdict.base = NULL; gdict.sizeBlocks = 0; gdict.here = 0;
        gdict.latest = FORTH_NULL; gdict.count = 0;
        if (rBase) freeC47Blocks(rBase, rBlocks);
      }
    }
  }

  /* [9] Promotion walks across a complete variable-width native parameter.
   * The payload is deliberately changed to the FTOK_EXIT byte image: it is
   * data inside the named parameter and must never terminate the token walk. */
  {
    uint16_t ref = 0, gref = 0;
    forthDictClear();
    forthGDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": GP STO 'AA' RECURSE ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("GP", &ref) ||
        (ref & FORTH_REF_GLOBAL)) {
      printf("    [9] FAIL: fixture definition failed (error=%d ref=0x%04X)\n",
             lastErrorCode, ref);
      fail = 1;
    } else {
      uint16_t off = fdict.latest;
      forthHeader_t *h = (forthHeader_t *)(fdict.base + off);
      uint16_t pos = off + (uint16_t)TO_BLOCKS(6 + h->nameLen) * BYTES_PER_BLOCK;
      ftoken_t tok = 0;
      uint16_t item = 0, span = 0;
      memcpy(&tok, fdict.base + pos, 2);
      pos += 2;
      memcpy(&item, fdict.base + pos, 2);
      pos += 2;
      if (tok != T_C47 || item == 0 || item >= LAST_ITEM ||
          !forthParamCellSpan(fdict.base, pos, fdict.here,
                              (uint16_t)(indexOfItems[item].status & PTP_STATUS),
                              true, &span) ||
          span < 4 || fdict.base[pos] != STRING_LABEL_VARIABLE ||
          fdict.base[pos + 1] != 2) {
        printf("    [9] FAIL: fixture native parameter layout is invalid\n");
        fail = 1;
      } else {
        uint16_t selfPos = (uint16_t)(pos + span);
        ftoken_t selfTok = 0, promotedTok = 0;
        memcpy(&selfTok, fdict.base + selfPos, 2);
        fdict.base[pos + 2] = 0;
        fdict.base[pos + 3] = 0;
        lastErrorCode = ERROR_NONE;
        if (selfTok != (ftoken_t)(0x1000u + ref) ||
            !forthDictMakeLatestGlobal(ref, &gref)) {
          printf("    [9] FAIL: promotion failed (self=0x%04X error=%d)\n",
                 selfTok, lastErrorCode);
          fail = 1;
        } else {
          uint16_t gBody = gdict.latest +
              (uint16_t)TO_BLOCKS(6 + h->nameLen) * BYTES_PER_BLOCK;
          memcpy(&promotedTok, gdict.base + gBody + (selfPos -
                 (off + (uint16_t)TO_BLOCKS(6 + h->nameLen) * BYTES_PER_BLOCK)), 2);
          if (promotedTok !=
              (ftoken_t)(FORTH_GCALL_BASE + (gref & 0x7FFFu))) {
            printf("    [9] FAIL: self-call after named parameter not rewritten "
                   "(tok=0x%04X)\n", promotedTok);
            fail = 1;
          } else {
            printf("    [9] PASS: promotion skipped named payload and rewrote "
                   "the following self-call\n");
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [10] Promotion must reject a malformed named-cell pad rather than
   * copying it to persistent gdict and walking its payload as tokens. */
  {
    uint16_t ref = 0, gref = 0;
    forthDictClear();
    forthGDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": GQ STO 'ABC' RECURSE ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("GQ", &ref) ||
        (ref & FORTH_REF_GLOBAL)) {
      printf("    [10] FAIL: fixture definition failed (error=%d ref=0x%04X)\n",
             lastErrorCode, ref);
      fail = 1;
    } else {
      uint16_t off = fdict.latest;
      forthHeader_t *h = (forthHeader_t *)(fdict.base + off);
      uint16_t pos = off + (uint16_t)TO_BLOCKS(6 + h->nameLen) * BYTES_PER_BLOCK;
      uint16_t item = 0, span = 0;
      pos += 2;                              /* FTOK_C47 */
      memcpy(&item, fdict.base + pos, 2);
      pos += 2;
      if (item == 0 || item >= LAST_ITEM ||
          !forthParamCellSpan(fdict.base, pos, fdict.here,
                              (uint16_t)(indexOfItems[item].status & PTP_STATUS),
                              true, &span) ||
          span < 6) {
        printf("    [10] FAIL: fixture named-cell layout is invalid\n");
        fail = 1;
      } else {
        uint16_t oldFCount = fdict.count;
        fdict.base[pos + span - 1] = 1;       /* corrupt the odd-name pad */
        lastErrorCode = ERROR_NONE;
        if (forthDictMakeLatestGlobal(ref, &gref) ||
            lastErrorCode != ERROR_INVALID_NAME ||
            gdict.count != 0 || fdict.count != oldFCount) {
          printf("    [10] FAIL: malformed pad promotion result "
                 "(error=%d fcount=%u gcount=%u)\n",
                 lastErrorCode, fdict.count, gdict.count);
          fail = 1;
        } else {
          printf("    [10] PASS: malformed named-cell pad rejected before promotion\n");
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [11] Item ID zero is ITM_NULL/PTP_NONE, not a corrupt sentinel.  Both
   * promotion and restore validation must accept the same body the runtime
   * has historically executed. */
  {
    uint16_t ref = 0, gref = 0;
    uint16_t w;
    forthDictClear();
    forthGDictClear();
    lastErrorCode = ERROR_NONE;
    w = begin_word("G0", 2);
    if (w == FORTH_NULL) {
      printf("    [11] FAIL: fixture allocation failed\n");
      fail = 1;
    } else {
      uint16_t itemId = ITM_NULL;
      forthDictEmit(T_C47);
      forthDictEmitBytes(&itemId, 2);
      forthDictEmit(T_ILIT);
      emit_int32(55);
      end_word(w);
      if (!forthFindColon("G0", &ref) ||
          !forthDictMakeLatestGlobal(ref, &gref)) {
        printf("    [11] FAIL: ITM_NULL body promotion failed (error=%d)\n",
               lastErrorCode);
        fail = 1;
      } else {
        forthGDictValidateRestored();
        if (lastErrorCode != ERROR_NONE || gdict.base == NULL ||
            !(gref & FORTH_REF_GLOBAL)) {
          printf("    [11] FAIL: ITM_NULL global failed validation (error=%d)\n",
                 lastErrorCode);
          fail = 1;
        } else {
          lastErrorCode = ERROR_NONE;
          if (run_word("G0") || !x_is_longint(55)) {
            printf("    [11] FAIL: validated ITM_NULL global did not run\n");
            fail = 1;
          } else {
            printf("    [11] PASS: ITM_NULL survives promotion and restore validation\n");
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* test_control_flow
 * F3-5: compile-time control flow words: IF/ELSE/THEN, BEGIN/UNTIL/AGAIN/WHILE/REPEAT
 */
static int test_control_flow(void)
{
  int fail = 0;

  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;

  /* [1] IF/ELSE/THEN, both arms + the no-DUP pin */
  {
    forthOuterInterpret(": CF1 IF 10 ELSE 20 THEN ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: CF1 compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      uint16_t ref;
      if (!forthFindColon("CF1", &ref)) {
        printf("    [1] FAIL: CF1 not found\n");
        fail = 1;
      } else {
        /* Body starts after header: name "CF1" = 3 bytes. Header = 6 bytes + nameLen bytes.
         * TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK = TO_BLOCKS(9) * 4 = 2*4 = 8 */
        { uint16_t tok;
          memcpy(&tok, fdict.base + fdict.latest + TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK, 2);
          if (tok != 0x7F03) {
            printf("    [1] FAIL: first body token is 0x%04X, expected 0x7F03 (FTOK_0BR)\n", tok);
            fail = 1;
          } else {
            forthPushInt32(1);
            lastErrorCode = ERROR_NONE;
            if (run_word("CF1") || !x_is_longint(10)) {
              printf("    [1] FAIL: CF1 with truthy flag did not leave X=10\n");
              fail = 1;
            } else {
              forthPushInt32(0);
              lastErrorCode = ERROR_NONE;
              if (run_word("CF1") || !x_is_longint(20)) {
                printf("    [1] FAIL: CF1 with falsy flag did not leave X=20\n");
                fail = 1;
              } else {
                printf("    [1] PASS: IF consumes the flag and selects the correct arm\n");
              }
            }
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [2] BEGIN/WHILE/REPEAT countdown */
  {
    forthOuterInterpret(": CF2 BEGIN DUP WHILE 1 - REPEAT ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: CF2 compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      forthPushInt32(5);
      lastErrorCode = ERROR_NONE;
      if (run_word("CF2") || !x_is_longint(0)) {
        printf("    [2] FAIL: CF2 did not leave X=0 (got error %d)\n", lastErrorCode);
        fail = 1;
      } else {
        printf("    [2] PASS: BEGIN/WHILE/REPEAT loop terminates at zero\n");
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [3] UNTIL loops while false */
  {
    forthOuterInterpret(": CF5 BEGIN 1 - DUP UNTIL DROP ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: CF5 compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      forthPushInt32(99);
      forthPushInt32(1);
      lastErrorCode = ERROR_NONE;
      if (run_word("CF5") || !x_is_longint(99)) {
        printf("    [3] FAIL: CF5 did not leave X=99\n");
        fail = 1;
      } else {
        printf("    [3] PASS: UNTIL branches back on false\n");
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [4] AGAIN is the runaway-bounded infinite loop */
  {
    forthOuterInterpret(": CF3 BEGIN AGAIN ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: CF3 compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      lastErrorCode = ERROR_NONE;
      run_word("CF3");
      if (lastErrorCode != ERROR_RAM_FULL) {
        printf("    [4] FAIL: CF3 did not hit ERROR_RAM_FULL (got %d)\n", lastErrorCode);
        fail = 1;
      } else {
        printf("    [4] PASS: AGAIN loops until the runaway backstop\n");
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [5] Nesting */
  {
    forthOuterInterpret(": CF4 IF 1 IF 30 THEN THEN ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: CF4 compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      forthPushInt32(1);
      lastErrorCode = ERROR_NONE;
      if (run_word("CF4") || !x_is_longint(30)) {
        printf("    [5] FAIL: CF4 with truthy flags did not leave X=30\n");
        fail = 1;
      } else {
        forthPushInt32(7);
        forthPushInt32(0);
        lastErrorCode = ERROR_NONE;
        if (run_word("CF4") || !x_is_longint(7)) {
          printf("    [5] FAIL: CF4 with falsy inner flag did not leave X=7\n");
          fail = 1;
        } else {
          printf("    [5] PASS: nested IF pairs resolve independently\n");
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }

  /* [6] Pairing and placement errors, all atomic */
  {
    int subfail = 0;

    /* THEN with no IF */
    { forthOuterInterpret(": CE1 THEN ;");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: CE1 did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      { uint16_t ref;
        if (forthFindColon("CE1", &ref)) {
          printf("    [6] FAIL: CE1 findable despite error\n");
          subfail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* IF without THEN, unbalanced at ; */
    { forthOuterInterpret(": CE2 IF ;");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: CE2 did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      { uint16_t ref;
        if (forthFindColon("CE2", &ref)) {
          printf("    [6] FAIL: CE2 findable despite error\n");
          subfail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* BEGIN THEN (kind mismatch) */
    { forthOuterInterpret(": CE3 BEGIN THEN ;");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: CE3 did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      { uint16_t ref;
        if (forthFindColon("CE3", &ref)) {
          printf("    [6] FAIL: CE3 findable despite error\n");
          subfail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* BEGIN REPEAT (missing WHILE) */
    { forthOuterInterpret(": CE4 BEGIN REPEAT ;");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: CE4 did not give ERROR_INVALID_NAME (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      { uint16_t ref;
        if (forthFindColon("CE4", &ref)) {
          printf("    [6] FAIL: CE4 findable despite error\n");
          subfail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* interpret-state IF (compile-only guard) */
    { forthOuterInterpret("IF");
      if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
        printf("    [6] FAIL: interpret-state IF did not give ERROR_OPERATION_UNDEFINED (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* 9 IFs overflow FORTH_CSTACK_DEPTH 8 */
    { forthOuterInterpret(": CE5 IF IF IF IF IF IF IF IF IF 1 ;");
      if (lastErrorCode != ERROR_RAM_FULL) {
        printf("    [6] FAIL: CE5 did not give ERROR_RAM_FULL (got %d)\n", lastErrorCode);
        subfail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    if (!subfail) {
      printf("    [6] PASS: unbalanced and misplaced control words reject atomically\n");
    } else {
      fail = 1;
    }
  }

  /* [7] Branches survive GLOBAL + restore */
  {
    forthOuterInterpret(": GLW BEGIN DUP WHILE 1 - REPEAT ; GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [7] FAIL: GLW GLOBAL compile error %d\n", lastErrorCode);
      fail = 1;
    } else {
      forthDictClear();
      saveCalc();
      forthGDictClear();
      { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }
      forthPushInt32(3);
      lastErrorCode = ERROR_NONE;
      if (run_word("GLW") || !x_is_longint(0)) {
        printf("    [7] FAIL: GLW did not leave X=0 after restore\n");
        fail = 1;
      } else {
        printf("    [7] PASS: compiled branches survive GLOBAL and restore validation\n");
      }
      lastErrorCode = ERROR_NONE;
      { uint8_t *rBase = gdict.base; uint16_t rBlocks = gdict.sizeBlocks;
        gdict.base = NULL; gdict.sizeBlocks = 0; gdict.here = 0;
        gdict.latest = FORTH_NULL; gdict.count = 0;
        if (rBase) freeC47Blocks(rBase, rBlocks);
      }
    }
  }

  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;
  return fail;
}

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

  printf("\nFORTH SHOWCASE PROGRAM (complete user-facing language example)\n");
  forthDictInit();
  forthGDictInit();
  printf("  [DEBUG] running test_showcase_program...\n");
  fail |= test_showcase_program();
  forthDictClear();
  forthGDictClear();

  /* FIX-6: free-list integrity — LAST test, after all cleanup */
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

/* test_alpha_menu_on_top_during_capture
 * F5: Opening Forth capture should leave MNU_ALPHA on top of the softmenu stack,
 * not MNU_FORTH. The picker is a submenu entry, not a forced overlay.
 * Escaping mutation: re-add showSoftmenu(-MNU_FORTH) after showSoftmenu(-MNU_ALPHA)
 * in pemAlpha — the assertion fails (top becomes MNU_FORTH instead of MNU_ALPHA).
 */
static int test_alpha_menu_on_top_during_capture(void)
{
  int fail = 0;

  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin (RPN) */
    0x85, 0xB2,                                                       /* ITM_END */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;
  int16_t savedTamFunc = tam.function;

  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  /* R2-T6 item 2: seed 0, not ITM_FORTH — priming the derived value defeats
   * the point of a test that exists to prove the call DERIVES it. */
  tam.function = 0;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_FORTH);

  if (currentMenu() != -MNU_ALPHA) {
    printf("    FAIL: currentMenu() = %d, expected %d (-MNU_ALPHA)\n",
    currentMenu(), -MNU_ALPHA);
    fail = 1;
  }

  if (tam.function != ITM_FORTH) {
    printf("    FAIL: tam.function = %d, expected ITM_FORTH (%d) — not derived\n",
           tam.function, ITM_FORTH);
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
  tam.function = savedTamFunc;

  if (!fail) {
    printf("    PASS: alpha menu on top during Forth capture (not MNU_FORTH overlay), tam.function derived\n");
  }
  return fail;
}

/* test_alpha_menu_contains_fwrd
 * F5: The MNU_ALPHA item table (menu_ALPHA) must contain a -MNU_FORTH entry
 * so the Forth word picker is reachable as a submenu from the alpha menu.
 * Escaping mutation: remove the -MNU_FORTH entry from menu_ALPHA — the assertion fails.
 * [VERIFIED: softmenus.c, menu_ALPHA row containing -MNU_FORTH]
 */
static int test_alpha_menu_contains_fwrd(void)
{
  int fail = 0;

  extern const softmenu_t softmenu[];
  /* Find MNU_ALPHA in the softmenu array (dynamic — index may vary by build) */
  int16_t alphaIdx = -1;
  for (int16_t si = 0; si < 200; si++) {
    if (softmenu[si].menuItem == -MNU_ALPHA) {
      alphaIdx = si;
      break;
    }
  }
  if (alphaIdx < 0) {
    printf("    FAIL: MNU_ALPHA not found in softmenu array\n");
    return 1;
  }
  const int16_t *items = softmenu[alphaIdx].softkeyItem;
  int16_t numItems = softmenu[alphaIdx].numItems;
  bool_t found = false;

  for (int16_t i = 0; i < numItems; i++) {
    if (items[i] == -MNU_FORTH) {
      found = true;
      break;
    }
  }

  if (!found) {
    printf("    FAIL: -MNU_FORTH not found in menu_ALPHA table (%d items)\n", numItems);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: menu_ALPHA contains -MNU_FORTH entry (FWRD submenu)\n");
  }
  return fail;
}

/* test_forth_toggle_from_catalog_leaves_alpha_menu
 * A8: When Forth is toggled from the Functions catalog (CAT → FCNS → FORTH),
 * the real keyboard path must leave MNU_ALPHA on top.  fnKeyInCatalog=1 is what
 * executeFunction sets before calling runFunction in catalog context.
 *
 * The chain under test is keyboard.c:1213-1216 — runFunction(item) followed
 * immediately by _closeCatalog().  _closeCatalog is what eats MNU_ALPHA when the
 * catalog stack is only half torn down (MNU_ALPHA is in CatalogMenus[]), so it is
 * the whole point of the test and must be the REAL one: it is exported for this
 * suite via FORTH_SELFTEST_EXPORT.  Do not hand-roll it as popSoftmenu() calls —
 * the real one is a no-op once A2's teardown has emptied the catalog menus,
 * whereas blind pops eat MNU_ALPHA and fail a correct implementation.
 *
 * Escaping mutation: revert A2 (pop once instead of draining the catalog menus)
 * and _closeCatalog then finds MNU_CATALOG still stacked, pops MNU_ALPHA, and the
 * assertion fails.
 */
static int test_forth_toggle_from_catalog_leaves_alpha_menu(void)
{
  int fail = 0;

  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin */
    0x85, 0xB2,                                                       /* ITM_END */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedMenu = currentMenu();
  int16_t savedTamFunc = tam.function;
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;   /* R2-T6 item 3: incoming value, not 0 */

  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  catalog = CATALOG_FCNS;
  calcMode = CM_PEM;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  tam.function = ITM_FORTH;

  extern void showSoftmenu(int16_t menu);
  showSoftmenu(-MNU_CATALOG);
  showSoftmenu(-MNU_FCNS);

  /* fnKeyInCatalog must be set AFTER the menus are up and immediately before
   * the dispatch — showSoftmenu() clears it. That is the real ordering too
   * (keyboard.c:1190 sets it, :1213 dispatches, :1229 clears it). Setting it
   * before showSoftmenu leaves it false, runFunction's PEM gate
   * (!catalog || catalog == CATALOG_MVAR || fnKeyInCatalog) then fails, and
   * runFunction falls through to reallyRunFunction() — EXECUTING Forth instead
   * of inserting a step, so the arm under test never runs at all. */
  fnKeyInCatalog = 1;

  extern void runFunction(int16_t func);
  extern void _closeCatalog(void);
  runFunction(ITM_FORTH);
  _closeCatalog();          /* exactly what keyboard.c does next, :1216 */
  fnKeyInCatalog = savedFnKeyInCatalog;   /* R2-T6 item 3: restore, don't hardcode 0 */

  if (currentMenu() != -MNU_ALPHA) {
    printf("    FAIL: currentMenu() = %d, expected %d (-MNU_ALPHA)\n",
    currentMenu(), -MNU_ALPHA);
    fail = 1;
  }

  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set after toggle from catalog\n");
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  showSoftmenu(savedMenu);
  tam.function = savedTamFunc;

  if (!fail) {
    printf("    PASS: Forth toggle from catalog leaves MNU_ALPHA on top\n");
  }
  return fail;
}

/* test_forth_drain_clears_buried_catalog
 * R3: the ITM_FORTH catalog drain must use the same stack-wide predicate as
 * _closeCatalog(), which scans the ENTIRE softmenu stack for MNU_CATALOG.
 *
 * STRUCTURAL/DEFENSIVE TEST — this constructs the state rather than reaching it
 * by keypress, and the state is not known to be user-reachable today: ITM_FORTH
 * lives only in the FCNS catalog, and MNU_FCNS *is* in the old top-of-stack
 * list. It is a regression guard on the predicate itself, because the old drain
 * had two latent gaps and both are invisible until something does become
 * reachable:
 *   (a) it tested only the top of the stack, while _closeCatalog() is stack-wide;
 *   (b) its hand-written menu list (CATALOG/FCNS/CONST/CHARS/PROGS/VARS/MENUS)
 *       omits every catalog menu that enterAsmModeIfMenuIsACatalog() also sets
 *       `catalog` for — MNU_SYSFL here, plus the whole VARS family (MNU_REALS,
 *       MNU_MATRS, MNU_DATES, ...) that upstream itself lists in CatalogMenus[]
 *       (keyboard.c:407-419). A list that must be kept in sync by hand is the
 *       defect; the stack-wide test removes the need for one.
 * MNU_SYSFL is used because it sets `catalog` (CATALOG_SYFL, calcMode.c:120),
 * is a static menu, and is absent from the old drain list.
 * Escaping mutation: revert the drain to the old top-of-stack while-loop —
 * MNU_SYSFL is not in its list, so it stops immediately, the buried MNU_CATALOG
 * survives, and _closeCatalog() pops the MNU_ALPHA that pemAlpha() just pushed
 * (MNU_ALPHA is itself listed in CatalogMenus[], keyboard.c:402).
 */
static int test_forth_drain_clears_buried_catalog(void)
{
  int fail = 0;

  uint8_t prog[] = {
    0x4C,                                                             /* ITM_sin */
    0x85, 0xB2,                                                       /* ITM_END */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;   /* R2-T6 item 3: incoming value, not 0 */
  /* The drain pops the whole stack down past MNU_CATALOG, so restoring only
   * currentMenu() would leak a truncated stack into later tests. */
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  calcMode = CM_PEM;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  tam.function = ITM_FORTH;

  extern void showSoftmenu(int16_t menu);
  catalog = CATALOG_NONE;
  showSoftmenu(-MNU_CATALOG);   /* does not set `catalog` (calcMode.c default:) */
  showSoftmenu(-MNU_SYSFL);     /* sets catalog = CATALOG_SYFL; NOT in the old
                                 * drain list; MNU_CATALOG now buried under it */

  if (catalog != CATALOG_SYFL) {
    printf("    FAIL: precondition — catalog = %d, expected CATALOG_SYFL (%d)\n",
           catalog, CATALOG_SYFL);
    fail = 1;
  }
  if (currentMenu() != -MNU_SYSFL) {
    printf("    FAIL: precondition — currentMenu() = %d, expected %d (-MNU_SYSFL)\n",
           currentMenu(), -MNU_SYSFL);
    fail = 1;
  }

  fnKeyInCatalog = 1;           /* after the menus are up — showSoftmenu clears it */

  extern void runFunction(int16_t func);
  extern void _closeCatalog(void);
  runFunction(ITM_FORTH);
  _closeCatalog();              /* exactly what keyboard.c does next */
  fnKeyInCatalog = savedFnKeyInCatalog;   /* R2-T6 item 3: restore, don't hardcode 0 */

  if (currentMenu() != -MNU_ALPHA) {
    printf("    FAIL: currentMenu() = %d, expected %d (-MNU_ALPHA) — buried "
           "MNU_CATALOG defeated the drain\n", currentMenu(), -MNU_ALPHA);
    fail = 1;
  }
  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set\n");
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  tam.function = savedTamFunc;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));

  if (!fail) {
    printf("    PASS: buried MNU_CATALOG drained; MNU_ALPHA survives _closeCatalog\n");
  }
  return fail;
}

/* test_forth_capture_survives_keystroke
 * A8 / A1 regression: with a capture open, a SECOND printable keystroke driven
 * through the real path must leave tam.function == ITM_FORTH. Upstream's first
 * arm of insertStepInProgram set tam.function = ITM_LITERAL unconditionally,
 * clobbering the capture on every key.
 *
 * The capture must be OPENED by driving it, not by assigning tam.function and
 * FLAG_ALPHA: pemAlpha's per-key re-insert path takes the step opcode from
 * currentStep[0] (manage.c:970), not from tam.function. With the cursor parked
 * on an arbitrary step, a hand-primed capture rewrites THAT step into
 * <its opcode> + STRING_LABEL_VARIABLE + payload — e.g. ITM_sin becomes
 * `4c fd 01 41`, a PTP_NONE item carrying a string. findNextStep then steps one
 * byte onto the 0xfd, decodes it as op 0x7d01, and findKey2ndParam indexes
 * indexOfItems[32001] (LAST_ITEM is 2870) — an out-of-bounds read that
 * segfaults. Priming the state under test does not just weaken this test; it
 * corrupts program memory.
 *
 * Escaping mutation: restore the unconditional tam.function = ITM_LITERAL in
 * insertStepInProgram's first arm — the assertion fails.
 */
static int test_forth_capture_survives_keystroke(void)
{
  int fail = 0;

  /* The marker alone. The entry state is derived from the step immediately
   * BEFORE the insertion point (forth_bridge.c:126 — an RPN predecessor means
   * RPN, whatever came before it), and addStepInProgram advances the cursor one
   * step before inserting (manage.c:1920-1923). So the cursor must sit ON the
   * marker for the marker to end up as the predecessor. Parking it after an
   * intervening ITM_sin would make that sin the predecessor and derive RPN —
   * correctly. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* opening marker */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  int16_t savedMenu = currentMenu();

  currentStep = beginOfProgramMemory;      /* ON the marker; insert lands after it */
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  catalog = CATALOG_NONE;
  calcMode = CM_PEM;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;

  /* Open the capture the way the machine does: a printable key with the cursor
   * inside a region is an E2 continuation, and it creates the ITM_FORTH
   * placeholder that the per-key re-insert path keys on. */
  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_2);

  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
    printf("    FAIL: setup did not open Forth capture (FLAG_ALPHA=%d tam.function=%d)\n",
    (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    catalog = savedCatalog;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    calcMode = savedCalcMode;
    tam.function = savedTamFunc;
    return 1;
  }

  /* The key under test. ITM_A (550) is what the ALPHA keyboard produces for
   * 'A'; the ASCII code 'A' (65) is ITM_EXP, a different function entirely. */
  extern void runFunction(int16_t func);
  runFunction(ITM_A);

  if (tam.function != ITM_FORTH) {
    printf("    FAIL: tam.function = 0x%04X, expected ITM_FORTH (0x%04X)\n",
    tam.function, ITM_FORTH);
    fail = 1;
  }

  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA cleared after printable key in capture\n");
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  showSoftmenu(savedMenu);
  tam.function = savedTamFunc;

  if (!fail) {
    printf("    PASS: Forth capture survives printable keystroke\n");
  }
  return fail;
}

/* test_forth_alpha_gesture_resumes_forth
 * A8 / A7 regression: with the cursor inside an open Forth region and the keypad
 * dropped to RPN, pressing ALPHA (ITM_AIM) must resume a FORTH capture, not a
 * string-literal one — this is what makes dropping the keypad survivable.
 *
 * tam.function MUST start at 0 here, not ITM_FORTH. A7's guard reads
 *   if(func == ITM_AIM && forthEntryStateAtInsertion()) tam.function = ITM_FORTH;
 *   else if(tam.function != ITM_FORTH)                  tam.function = ITM_LITERAL;
 * so seeding the sentinel satisfies the else and the assertion holds even with
 * A7 deleted — the test would be vacuous. Starting from 0 means only A7's branch
 * can produce ITM_FORTH.
 *
 * Escaping mutation: delete A7's ITM_AIM branch — tam.function comes back
 * ITM_LITERAL and the assertion fails.
 */
static int test_forth_alpha_gesture_resumes_forth(void)
{
  int fail = 0;

  /* Marker alone — see test_forth_capture_survives_keystroke for why the cursor
   * must sit ON it: the entry state comes from the predecessor of the INSERTION
   * point, and addStepInProgram advances one step before inserting. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                         /* opening marker */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  int16_t savedCatalog = catalog;
  uint16_t savedLocalStep = currentLocalStepNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedTamFunc = tam.function;
  int16_t savedMenu = currentMenu();

  /* Cursor inside open Forth region, keypad at RPN, no capture open. */
  currentStep = beginOfProgramMemory;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 1;
  catalog = CATALOG_NONE;
  calcMode = CM_PEM;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;          /* see header: seeding ITM_FORTH makes this vacuous */

  /* Drive ALPHA (ITM_AIM) through the real keyboard path */
  extern void runFunction(int16_t func);
  runFunction(ITM_AIM);

  if (tam.function != ITM_FORTH) {
    printf("    FAIL: tam.function = 0x%04X, expected ITM_FORTH (0x%04X)\n",
    tam.function, ITM_FORTH);
    fail = 1;
  }

  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: FLAG_ALPHA not set after ALPHA gesture resumes Forth\n");
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  showSoftmenu(savedMenu);
  tam.function = savedTamFunc;

  if (!fail) {
    printf("    PASS: ALPHA gesture resumes Forth (not ITM_LITERAL)\n");
  }
  return fail;
}

/* test_freelist_consistent
 * FIX-6: free-list integrity check — walks freeMemoryRegions[0..n), asserts
 * blockAddress strictly increasing, no overlap (addr+size <= next.addr),
 * and no region overlaps program memory.
 * Escaping mutation: the old restoreTestProgram region surgery (#if 0 block)
 * creates overlapping regions that this test catches. */
static int test_freelist_consistent(void)
{
  int fail = 0;

  if (numberOfFreeMemoryRegions < 1) {
    printf("    FAIL: no free memory regions\n");
    return 1;
  }

  /* Check strictly increasing blockAddress and no overlap */
  for (int32_t i = 1; i < numberOfFreeMemoryRegions; i++) {
    uint32_t prevEnd = (uint32_t)freeMemoryRegions[i - 1].blockAddress +
    (uint32_t)freeMemoryRegions[i - 1].sizeInBlocks;
    uint32_t curAddr = (uint32_t)freeMemoryRegions[i].blockAddress;
    if (prevEnd > curAddr) {
      printf("    FAIL: overlap between regions %ld and %ld "
      "(prev end=%u, cur addr=%u)\n",
      (long)(i - 1), (long)i, prevEnd, curAddr);
      fail = 1;
    }
    if (freeMemoryRegions[i].sizeInBlocks == 0) {
      printf("    FAIL: region %ld has zero size\n", (long)i);
      fail = 1;
    }
  }

  /* Check no region overlaps program memory */
  { uint16_t progStart = TO_C47MEMPTR(beginOfProgramMemory);
    uint16_t progEnd = RAM_SIZE_IN_BLOCKS;
    for (int32_t i = 0; i < numberOfFreeMemoryRegions; i++) {
      uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
      uint32_t rEnd = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
      if (rStart < progEnd && rEnd > progStart) {
        printf("    FAIL: free region %ld [%u..%u) overlaps program [%u..%u)\n",
        (long)i, (uint32_t)progStart, (uint32_t)progEnd, rStart, rEnd);
        fail = 1;
      }
    }
  }

  if (!fail) {
    printf("    PASS: free list consistent (%ld regions, no overlap)\n",
    (long)numberOfFreeMemoryRegions);
  }
  return fail;
}

/* test_freelist_double_free_guarded
 * FIX-6: freeListFree's range-overlap guard must reject a double free of the
 * exact same (pointer, size) pair without mutating the free list at all.
 * Escaping mutation: remove the guard loop in freeListFree (core/freeList.c)
 * — the second free proceeds, inserts a duplicate/overlapping region, and
 * test_freelist_consistent() FAILs (overlap between adjacent regions). */
static int test_freelist_double_free_guarded(void)
{
  int fail = 0;
  const size_t blocks = 4;

  void *blk = allocC47Blocks(blocks);
  if (!blk) {
    printf("    FAIL: allocC47Blocks returned NULL\n");
    return 1;
  }

  freeC47Blocks(blk, blocks); /* legitimate free */

  int32_t countBefore = numberOfFreeMemoryRegions;
  freeMemoryRegion_t snapshot[MAX_FREE_REGIONS];
  memcpy(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t));

  freeC47Blocks(blk, blocks); /* double free — must be a no-op */

  if (numberOfFreeMemoryRegions != countBefore) {
    printf("    FAIL: numberOfFreeMemoryRegions changed %ld -> %ld after double free\n",
    (long)countBefore, (long)numberOfFreeMemoryRegions);
    fail = 1;
  }
  else if (memcmp(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t)) != 0) {
    printf("    FAIL: a free region's blockAddress/sizeInBlocks changed after double free\n");
    fail = 1;
  }

  if (test_freelist_consistent()) {
    printf("    FAIL: test_freelist_consistent failed after double free\n");
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: exact-match double free rejected, free list unchanged\n");
  }
  return fail;
}

/* test_freelist_interior_double_free
 * FIX-6: free two adjacent allocations so they coalesce into one free region,
 * then double-free the SECOND allocation's address — now interior to the
 * coalesced region, not equal to its blockAddress. The range-overlap guard
 * must still catch this.
 * Escaping mutation: revert the guard to the old exact blockAddress==C47RamPtr
 * match — the interior address no longer equals any region's blockAddress, the
 * double free slips through, inserts an overlapping region, and
 * test_freelist_consistent() FAILs. */
static int test_freelist_interior_double_free(void)
{
  int fail = 0;

  /* Build an ADJACENT pair from SEPARATELY TRACKED allocations, so every
   * free below is legitimate against allocatedMemoryRegions[] and produces
   * zero bookkeeping diagnostics. (An earlier revision allocated one
   * 5-block region and split-freed it 2+3, which left a stale allocation
   * record behind and tripped the "Memory freeing A/B" diagnostics on
   * every run — harness-made allocator-accounting staleness, not a real
   * caller pattern.)
   *
   * freeListAlloc is best-fit (exact-size hole first, else the smallest
   * larger region, carved from the front — core/freeList.c:18,35-37,55),
   * so two back-to-back allocations carry no adjacency guarantee. Instead
   * allocate a batch of equal-size chunks: once exact-size holes are
   * exhausted, consecutive chunks are carved contiguously from one region,
   * so a batch of 8 always contains an adjacent pair in practice. SKIP
   * defensively if fragmentation ever defeats that. */
  enum { CHUNKS = 8 };
  const size_t chunkBlocks = 2;             /* 2 blocks per chunk */
  void *chunk[CHUNKS];
  int nAlloc = 0;
  int lo = -1, hi = -1;

  for (int i = 0; i < CHUNKS; i++) {
    chunk[i] = allocC47Blocks(chunkBlocks);
    if (!chunk[i]) {
      break;
    }
    nAlloc++;
  }
  for (int i = 0; i < nAlloc && lo < 0; i++) {
    for (int j = 0; j < nAlloc; j++) {
      if (i != j &&
          (uint8_t *)chunk[j] == (uint8_t *)chunk[i] + TO_BYTES(chunkBlocks)) {
        lo = i;
        hi = j;
        break;
      }
    }
  }
  /* Release every chunk not part of the pair — fully tracked frees. */
  for (int i = 0; i < nAlloc; i++) {
    if (i != lo && i != hi) {
      freeC47Blocks(chunk[i], chunkBlocks);
    }
  }
  if (lo < 0) {
    printf("    SKIP: no adjacent chunk pair found, cannot form interior region\n");
    return 0;
  }

  void *second = chunk[hi];             /* higher address: interior after merge */
  const size_t secondSize = chunkBlocks;
  freeC47Blocks(chunk[lo], chunkBlocks); /* frees [lo, lo+2) */
  freeC47Blocks(second, secondSize);     /* frees [lo+2, lo+4), coalesces */

  int32_t countBefore = numberOfFreeMemoryRegions;
  freeMemoryRegion_t snapshot[MAX_FREE_REGIONS];
  memcpy(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t));

  /* second's address is now interior to the coalesced region, not equal to
   * its blockAddress (which is first's address, or lower after merging
   * with surrounding free space). */
  freeC47Blocks(second, secondSize); /* interior double free — must be a no-op */

  if (numberOfFreeMemoryRegions != countBefore) {
    printf("    FAIL: numberOfFreeMemoryRegions changed %ld -> %ld after interior double free\n",
    (long)countBefore, (long)numberOfFreeMemoryRegions);
    fail = 1;
  }
  else if (memcmp(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t)) != 0) {
    printf("    FAIL: free list changed after interior double free\n");
    fail = 1;
  }

  if (test_freelist_consistent()) {
    printf("    FAIL: test_freelist_consistent failed after interior double free\n");
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: interior double free rejected, free list unchanged\n");
  }
  return fail;
}

/* test_freelist_no_mutation_on_oversize_free
 * FIX-6: double-freeing with a LARGER sizeInBlocks than originally allocated
 * must not grow the free region the address falls in.
 * Escaping mutation: re-add the old size-grow branch (if sizeInBlocks <
 * requested, grow freeMemoryRegions[i].sizeInBlocks to the requested size) —
 * the region grows and the size-unchanged assertion FAILs. */
static int test_freelist_no_mutation_on_oversize_free(void)
{
  int fail = 0;
  const size_t blocks = 4;
  const size_t oversizeBlocks = blocks + 10;

  void *blk = allocC47Blocks(blocks);
  if (!blk) {
    printf("    FAIL: allocC47Blocks returned NULL\n");
    return 1;
  }

  freeC47Blocks(blk, blocks); /* legitimate free */

  int32_t countBefore = numberOfFreeMemoryRegions;
  freeMemoryRegion_t snapshot[MAX_FREE_REGIONS];
  memcpy(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t));

  freeC47Blocks(blk, oversizeBlocks); /* oversize double free — must be a no-op */

  if (numberOfFreeMemoryRegions != countBefore) {
    printf("    FAIL: numberOfFreeMemoryRegions changed %ld -> %ld after oversize double free\n",
    (long)countBefore, (long)numberOfFreeMemoryRegions);
    fail = 1;
  }
  else if (memcmp(snapshot, freeMemoryRegions, (size_t)countBefore * sizeof(freeMemoryRegion_t)) != 0) {
    printf("    FAIL: a free region grew after oversize double free\n");
    fail = 1;
  }

  if (test_freelist_consistent()) {
    printf("    FAIL: test_freelist_consistent failed after oversize double free\n");
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: oversize double free rejected, no region grew\n");
  }
  return fail;
}

/* test_unterminated_def_errors
 * C-4: End of line with state == COMPILE (unterminated definition) must
 * abort the definition and display ERROR_INVALID_NAME. The word must not
 * be visible after the error, and fdict.count must be restored.
 * Escaping mutation: remove or over-tighten the end-of-line block in
 * forth_compile.c — no error is shown and the count grows (smudged leak). */
static int test_unterminated_def_errors(void)
{
  uint16_t countBefore = fdict.count;
  uint16_t idx;

  lastErrorCode = ERROR_NONE;
  x_set_string(": FOO DUP");
  fnForthOuter(NOPARAM);

  if (lastErrorCode != ERROR_INVALID_NAME) {
    printf("    FAIL: lastErrorCode = %d, expected %d (ERROR_INVALID_NAME)\n",
           lastErrorCode, ERROR_INVALID_NAME);
    return 1;
  }

  if (forthFindColon("FOO", &idx)) {
    printf("    FAIL: FOO should not be visible after unterminated def\n");
    return 1;
  }

  if (fdict.count != countBefore) {
    printf("    FAIL: fdict.count changed from %u to %u (smudged leak)\n",
           countBefore, fdict.count);
    return 1;
  }

  printf("    PASS: unterminated def shows INVALID_NAME, word invisible, count restored\n");
  return 0;
}

/* test_overlong_token_in_def_keeps_error
 * C-13: When a token exceeds FORTH_TOKEN_MAX inside a definition, the
 * ERROR_INPUT_TOO_LONG from the tokenizer must NOT be masked by a subsequent
 * ERROR_INVALID_NAME from the end-of-line handler. The definition must be
 * aborted, the word invisible, and count restored.
 * Escaping mutation: revert to unconditional displayCalcErrorMessage(ERROR_INVALID_NAME)
 * in the end-of-line block — INVALID_NAME masks INPUT_TOO_LONG. */
static int test_overlong_token_in_def_keeps_error(void)
{
  uint16_t countBefore = fdict.count;
  uint16_t idx;
  char longSource[256];

  /* Build ": FOO " + a 70-byte spaceless ASCII token */
  snprintf(longSource, sizeof(longSource), ": FOO %s",
           "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst");

  lastErrorCode = ERROR_NONE;
  x_set_string(longSource);
  fnForthOuter(NOPARAM);

  if (lastErrorCode != ERROR_INPUT_TOO_LONG) {
    printf("    FAIL: lastErrorCode = %d, expected %d (ERROR_INPUT_TOO_LONG, not INVALID_NAME %d)\n",
           lastErrorCode, ERROR_INPUT_TOO_LONG, ERROR_INVALID_NAME);
    return 1;
  }

  if (forthFindColon("FOO", &idx)) {
    printf("    FAIL: FOO should not be visible after overlong token error\n");
    return 1;
  }

  if (fdict.count != countBefore) {
    printf("    FAIL: fdict.count changed from %u to %u\n",
           countBefore, fdict.count);
    return 1;
  }

  printf("    PASS: overlong token preserves INPUT_TOO_LONG (not masked by INVALID_NAME), word invisible, count restored\n");
  return 0;
}

/* test_param_core_extraction
 * F2-1: verify the extracted parameter core (param_core.c/h) behaves
 * identically to the original _executeOp in lblGtoXeq.c.  Two
 * independently reported subcases. */
static int test_param_core_extraction(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* ---- Subcase 1: direct register parameter through the moved core ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '2', 'E',   /* LBL 'F2E' */
      0x2C, 0x05,                        /* STO 05    */
      0x04                               /* RTN       */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [1] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F2E", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [1] FAIL: findNamedLabel(\"F2E\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("42");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: forthOuterInterpret(\"42\") error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(42)) {
      printf("    [1] FAIL: X != 42 after interpret\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;
      dynamicMenuItem = -1;
      fnExecute(lbl);

      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: fnExecute error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (!x_is_longint(42)) {
        printf("    [1] FAIL: X != 42 after STO\n");
        fail = 1;
      }
      else {
        int32_t reg5Val = -1;
        bool_t isLongint = (getRegisterDataType(5) == dtLongInteger);
        if (isLongint) {
          longInteger_t li;
          longIntegerInit(li);
          convertLongIntegerRegisterToLongInteger(5, li);
          longIntegerToInt32(li, reg5Val);
          longIntegerFree(li);
        }
        if (!isLongint || reg5Val != 42) {
          printf("    [1] FAIL: register 05 != 42 (type=%s, val=%d)\n",
                 isLongint ? "longint" : "not-longint", reg5Val);
          fail = 1;
        }
        else {
          printf("    [1] PASS: STO 05 through moved core stores 42 in register 05\n");
        }
      }
    }

    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 2: relocated Forth XEQ fallback resolves and advances ---- */
  {
    uint8_t prog[] = {
      0x01, 0xFD, 0x03, 'F', '2', 'F',                       /* LBL 'F2F' */
      0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH    */
      0x8B, 0x1A, 0xFD, 0x08, ':', ' ', 'W', '7', ' ',       /* : W7 7 ;  */
      '7', ' ', ';',
      0x8B, 0x1A, 0xFD, 0x00,                                /* FORTH«    */
      0x03, 0xFD, 0x02, 'W', '7',                            /* XEQ 'W7'  */
      0x04                                                   /* RTN       */
    };

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [2] FAIL: writeTestProgram failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    calcRegister_t lbl = findNamedLabel("F2F", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [2] FAIL: findNamedLabel(\"F2F\") returned INVALID_VARIABLE\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    forthRunGenBump();
    programRunStop = PGM_RUNNING;
    currentStep = beginOfProgramMemory + 10;  /* source step */
    executeOneStep(currentStep);
    int16_t xeqAdvance = -1;
    if (lastErrorCode == ERROR_NONE) {
      currentStep = beginOfProgramMemory + 26;  /* XEQ 'W7' */
      xeqAdvance = executeOneStep(currentStep);
    }

    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: executeOneStep error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (xeqAdvance != 1) {
      printf("    [2] FAIL: synchronous XEQ 'W7' returned advance %d (expected 1)\n",
             xeqAdvance);
      fail = 1;
    }
    else if (!x_is_longint(7)) {
      printf("    [2] FAIL: X != 7 after XEQ 'W7'\n");
      fail = 1;
    }
    else {
      /* F2 audit regression: before executeOneStep distinguished a
       * synchronous Forth-name fallback from a native label branch, it
       * returned -1 here and the real run loop repeated this XEQ forever.
       * The direct return-value guard above keeps that mutation from
       * hanging the suite; now prove the complete engine drive terminates. */
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;
      dynamicMenuItem = -1;
      fnExecute(lbl);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: full fnExecute drive error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (programRunStop != PGM_STOPPED) {
        printf("    [2] FAIL: full fnExecute drive did not stop (state=%u)\n",
               programRunStop);
        fail = 1;
      }
      else if (!x_is_longint(7)) {
        printf("    [2] FAIL: full fnExecute drive did not leave X=7\n");
        fail = 1;
      }
      else {
        printf("    [2] PASS: XEQ 'W7' returns advance 1 and the full run terminates with X=7\n");
      }
    }

    forthDictClear();
    cleanupTestProgram();
  }

  programRunStop = savedRS;
  return fail;
}

/* test_param_core_bounded_names
 * F2-2: verify the bounded name reader (paramCoreReadName) in
 * param_core.c clamps reads to firstFreeProgramByte.
 * F4 audit: fixed-width structural bytes honor the same exclusive bound. */
static int test_param_core_bounded_names(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* ---- Subcase 1: well-formed name step unchanged ---- */
  {
    testProg_t tp;
    int sSrc, sXeq;
    tpInit(&tp);
    tpLbl(&tp, "F2F");
    tpMarker(&tp);
    sSrc = tpSrc(&tp, ": W7 7 ;");
    tpMarker(&tp);
    sXeq = tpXeqName(&tp, "W7");
    tpOp1(&tp, 0x04);                                        /* RTN */

    if (sSrc < 0 || sXeq < 0 || !tpWrite(&tp)) {
      printf("    [1] FAIL: fixture build/write failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    forthRunGenBump();
    programRunStop = PGM_RUNNING;
    currentStep = tpStepAddr(&tp, sSrc);
    executeOneStep(currentStep);
    if (lastErrorCode == ERROR_NONE) {
      currentStep = tpStepAddr(&tp, sXeq);
      executeOneStep(currentStep);
    }

    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: executeOneStep error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(7)) {
      printf("    [1] FAIL: X != 7 after XEQ 'W7'\n");
      fail = 1;
    }
    else {
      printf("    [1] PASS: well-formed XEQ 'W7' through bounded reader yields X=7\n");
    }

    forthDictClear();
    cleanupTestProgram();
  }

  /* ---- Subcase 2: lying length byte at end of program memory ---- */
  {
    static const uint8_t lyingXeq[] = {
      0x03, 0xFD, 0x7F, 'W', '7'         /* XEQ, len=127, only 2 bytes    */
    };
    testProg_t tp;
    int sSrc, sXeq;
    tpInit(&tp);
    tpLbl(&tp, "F2G");
    tpMarker(&tp);
    sSrc = tpSrc(&tp, ": W7 7 ;");
    tpMarker(&tp);
    sXeq = tpRaw(&tp, lyingXeq, sizeof(lyingXeq));           /* deliberately last: the lie crosses firstFreeProgramByte */

    if (sSrc < 0 || sXeq < 0 || !tpWrite(&tp)) {
      printf("    [2] FAIL: fixture build/write failed\n");
      programRunStop = savedRS;
      forthDictClear();
      cleanupTestProgram();
      return 1;
    }
    tpUseAuthoredEnd(&tp);

    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    dynamicMenuItem = -1;
    forthRunGenBump();
    programRunStop = PGM_RUNNING;
    currentStep = tpStepAddr(&tp, sSrc);
    executeOneStep(currentStep);
    if (lastErrorCode == ERROR_NONE) {
      currentStep = tpStepAddr(&tp, sXeq);
      executeOneStep(currentStep);
    }

    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: executeOneStep error %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(7)) {
      printf("    [2] FAIL: X != 7 after bounded XEQ 'W7'\n");
      fail = 1;
    }
    else {
      printf("    [2] PASS: lying length byte clamped, XEQ 'W7' resolved, X=7\n");
    }

    forthDictClear();
    cleanupTestProgram();
  }

#if defined(FORTH_DEBUG_SELFTEST)
  /* ---- Subcase 3: no length byte at the exclusive end ---- */
  {
    uint8_t truncatedParam[] = { STRING_LABEL_VARIABLE, 0x7F };
    uint8_t *savedFirstFree = firstFreeProgramByte;

    firstFreeProgramByte = truncatedParam + 1;
    paramCoreDebugNameLengthReads = 0;
    paramCoreExecuteOp(truncatedParam, ITM_GTO, PARAM_LABEL);
    firstFreeProgramByte = savedFirstFree;

    if (paramCoreDebugNameLengthReads != 0) {
      printf("    [3] FAIL: expected 0 length-byte reads, got %u\n",
             paramCoreDebugNameLengthReads);
      fail = 1;
    }
    else if (tmpStringLabelOrVariableName[0] != 0) {
      printf("    [3] FAIL: expected empty string, got '%s'\n",
             tmpStringLabelOrVariableName);
      fail = 1;
    }
    else {
      printf("    [3] PASS: missing name-length byte performed zero length-byte reads\n");
    }

    lastErrorCode = ERROR_NONE;
  }

  /* ---- Subcase 4: an available count wider than uint8_t ---- */
  {
    uint8_t wideParam[258] = {0};

    wideParam[0] = STRING_LABEL_VARIABLE;
    wideParam[1] = 2;
    wideParam[2] = 'W';
    wideParam[3] = '7';
    uint8_t *savedFirstFree = firstFreeProgramByte;

    firstFreeProgramByte = wideParam + 258;
    paramCoreDebugNameLengthReads = 0;
    paramCoreExecuteOp(wideParam, ITM_GTO, PARAM_LABEL);
    firstFreeProgramByte = savedFirstFree;

    if (paramCoreDebugNameLengthReads != 1) {
      printf("    [4] FAIL: expected 1 length-byte read, got %u\n",
             paramCoreDebugNameLengthReads);
      fail = 1;
    }
    else if (strcmp(tmpStringLabelOrVariableName, "W7") != 0) {
      printf("    [4] FAIL: expected 'W7', got '%s'\n",
             tmpStringLabelOrVariableName);
      fail = 1;
    }
    else {
      printf("    [4] PASS: 256-byte name remainder preserved 'W7' without uint8_t wrap\n");
    }

    lastErrorCode = ERROR_NONE;
  }
#endif

  /* ---- Subcase 5: every fixed-width structural read is bounded ---- */
  {
    uint16_t old16 = LAST_ITEM, new16 = LAST_ITEM;
    uint16_t i;
    uint8_t cell[1];
    int subFail = 0;

    for (i = 1; i < LAST_ITEM; i++) {
      if ((indexOfItems[i].status & PTP_STATUS) == PTP_NUMBER_16) {
        if (old16 == LAST_ITEM && isFunctionOldParam16(i)) old16 = i;
        if (new16 == LAST_ITEM && !isFunctionOldParam16(i)) new16 = i;
      }
    }
    if (old16 == LAST_ITEM || new16 == LAST_ITEM) {
      printf("    [5] CONFIG FAIL: missing old/new NUMBER_16 probe item\n");
      subFail = 1;
    }

    /* No leading parameter byte. */
    if (!subFail) {
      cell[0] = 0;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell, ITM_GTO, PARAM_LABEL);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: empty parameter did not report corrupted data "
               "(got %d)\n", lastErrorCode);
        subFail = 1;
      }
    }

    /* Marker forms whose required second byte is missing. */
    if (!subFail) {
      cell[0] = INDIRECT_REGISTER;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell + 1, ITM_STO, PARAM_REGISTER);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: truncated indirect-register cell got %d\n",
               lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      cell[0] = SYSTEM_FLAG_NUMBER;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell + 1, ITM_SF, PARAM_FLAG);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: truncated system-flag cell got %d\n",
               lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      cell[0] = CNST_BEYOND_250;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell + 1, ITM_CNST, PARAM_NUMBER_8_16);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: truncated extended-number cell got %d\n",
               lastErrorCode);
        subFail = 1;
      }
    }

    /* Both byte orders of PTP_NUMBER_16 require the second byte. */
    if (!subFail) {
      cell[0] = 1;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell + 1, old16, PARAM_NUMBER_16);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: truncated old NUMBER_16 cell got %d\n",
               lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      cell[0] = 1;
      lastErrorCode = ERROR_NONE;
      paramCoreExecuteOpBounded(cell, cell + 1, new16, PARAM_NUMBER_16);
      if (lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
        printf("    [5] FAIL: truncated new NUMBER_16 cell got %d\n",
               lastErrorCode);
        subFail = 1;
      }
    }

    if (!subFail) {
      printf("    [5] PASS: bounded core rejects every truncated fixed-width cell\n");
    } else {
      fail = 1;
    }
    lastErrorCode = ERROR_NONE;
  }

  programRunStop = savedRS;
  return fail;
}

/* test_c47_param_shared_dispatch
 * F2-3: verify that Forth's FTOK_C47 dispatch and the native engine
 * share the same parameter validation/dispatch path, closing the
 * out-of-range direct-parameter drift (§10.2). */
static int test_c47_param_shared_dispatch(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* Self-verifying config guard: ITM_SDL must be PTP_NUMBER_8 */
  {
    uint16_t sdlClass = (uint16_t)(indexOfItems[ITM_SDL].status & PTP_STATUS);
    if (sdlClass != PTP_NUMBER_8) {
      printf("    CONFIG FAIL: ITM_SDL status=0x%04X, ptpClass=0x%04X (expected PTP_NUMBER_8=0x%04X) — re-pick fixture item\n",
             (uint16_t)indexOfItems[ITM_SDL].status, sdlClass, PTP_NUMBER_8);
      programRunStop = savedRS;
      return 1;
    }
  }

  /* ---- Subcase 1: in-range NUMBER_8 parity ---- */
  {
    uint16_t sdlMax = (uint16_t)(indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK);
    uint8_t paramVal = 3;  /* SDL 3: shift left by 3 */
    int subFail = 0;

    /* Native: LBL 'F2H' + SDL 03 + RTN */
    {
      uint8_t prog[] = {
        0x01, 0xFD, 0x03, 'F', '2', 'H',   /* LBL 'F2H' */
        0x81, 0xA7, paramVal,                /* SDL 03    */
        0x04                                 /* RTN       */
      };

      if (!writeTestProgram(prog, sizeof(prog))) {
        printf("    [1] FAIL: writeTestProgram (native) failed\n");
        programRunStop = savedRS;
        return 1;
      }

      calcRegister_t lbl = findNamedLabel("F2H", GLOBAL_LABELS);
      if (lbl == INVALID_VARIABLE) {
        printf("    [1] FAIL: findNamedLabel(\"F2H\") returned INVALID_VARIABLE\n");
        forthDictClear();
        cleanupTestProgram();
        programRunStop = savedRS;
        return 1;
      }

      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("1");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(1)) {
        printf("    [1] FAIL: native setup X=1 failed\n");
        subFail = 1;
      }

      if (!subFail) {
        programRunStop = PGM_STOPPED;
        lastErrorCode = ERROR_NONE;
        dynamicMenuItem = -1;
        fnExecute(lbl);

        int32_t nativeX = 0;
        int nativeErr = lastErrorCode;
        if (x_is_longint(0)) {
          /* capture actual X */
        }
        /* Read X value */
        {
          longInteger_t li;
          longIntegerInit(li);
          convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
          longIntegerToInt32(li, nativeX);
          longIntegerFree(li);
        }

        /* Forth: SDL3 word = FTOK_C47, cell 423, cell 3, EXIT */
        forthDictClear();
        cleanupTestProgram();

        uint16_t w = begin_word("SDL3", 4);
        if (w == FORTH_NULL) {
          printf("    [1] FAIL: begin_word SDL3 failed\n");
          programRunStop = savedRS;
          return 1;
        }
        forthDictEmit(T_C47);
        { uint16_t itemId = ITM_SDL; forthDictEmitBytes(&itemId, 2); }
        { uint16_t p = paramVal; forthDictEmitBytes(&p, 2); }
        end_word(w);

        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("1");
        if (lastErrorCode != ERROR_NONE || !x_is_longint(1)) {
          printf("    [1] FAIL: forth setup X=1 failed\n");
          subFail = 1;
        }

        if (!subFail) {
          bool err = run_word("SDL3");
          int32_t forthX = 0;
          int forthErr = lastErrorCode;
          if (!err) {
            longInteger_t li;
            longIntegerInit(li);
            convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
            longIntegerToInt32(li, forthX);
            longIntegerFree(li);
          }

          if (err) {
            printf("    [1] FAIL: Forth SDL3 error %d\n", lastErrorCode);
            subFail = 1;
          }
          else if (nativeX != forthX) {
            printf("    [1] FAIL: X mismatch — native=%d, forth=%d\n", nativeX, forthX);
            subFail = 1;
          }
          else if (nativeErr != forthErr) {
            printf("    [1] FAIL: error mismatch — native=%d, forth=%d\n", nativeErr, forthErr);
            subFail = 1;
          }
          else if (forthX != 1000) {
            printf("    [1] FAIL: X=%d (expected 1000 = SDL 3 of 1)\n", forthX);
            subFail = 1;
          }
        }
      }

      if (!subFail) {
        printf("    [1] PASS: in-range NUMBER_8 parity — both sides X=1000, error=0\n");
      }
      else {
        fail = 1;
      }

      forthDictClear();
      cleanupTestProgram();
    }
  }

  /* ---- Subcase 2: out-of-range NUMBER_8 parity (closed drift) ---- */
  {
    uint16_t outOfRange = (uint16_t)(indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK) + 1;
    int subFail = 0;

    if (outOfRange > 255) {
      printf("    [2] CONFIG FAIL: computed out-of-range value %u exceeds 255 — re-pick fixture item\n", outOfRange);
      programRunStop = savedRS;
      return 1;
    }

    /* Native: LBL 'F2I' + SDL <outOfRange> + RTN */
    {
      uint8_t prog[] = {
        0x01, 0xFD, 0x03, 'F', '2', 'I',   /* LBL 'F2I' */
        0x81, 0xA7, (uint8_t)outOfRange,    /* SDL <oor> */
        0x04                                 /* RTN       */
      };

      if (!writeTestProgram(prog, sizeof(prog))) {
        printf("    [2] FAIL: writeTestProgram (native) failed\n");
        programRunStop = savedRS;
        return 1;
      }

      calcRegister_t lbl = findNamedLabel("F2I", GLOBAL_LABELS);
      if (lbl == INVALID_VARIABLE) {
        printf("    [2] FAIL: findNamedLabel(\"F2I\") returned INVALID_VARIABLE\n");
        forthDictClear();
        cleanupTestProgram();
        programRunStop = savedRS;
        return 1;
      }

      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("42");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
        printf("    [2] FAIL: native setup X=42 failed\n");
        subFail = 1;
      }

      if (!subFail) {
        programRunStop = PGM_STOPPED;
        lastErrorCode = ERROR_NONE;
        dynamicMenuItem = -1;
        fnExecute(lbl);

        int32_t nativeX = 0;
        longInteger_t li;
        longIntegerInit(li);
        convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
        longIntegerToInt32(li, nativeX);
        longIntegerFree(li);
        int nativeErr = lastErrorCode;

        /* Forth: SDL_OOR word with out-of-range param */
        forthDictClear();
        cleanupTestProgram();

        uint16_t w = begin_word("SOOR", 4);
        if (w == FORTH_NULL) {
          printf("    [2] FAIL: begin_word SOOR failed\n");
          programRunStop = savedRS;
          return 1;
        }
        forthDictEmit(T_C47);
        { uint16_t itemId = ITM_SDL; forthDictEmitBytes(&itemId, 2); }
        { uint16_t p = outOfRange; forthDictEmitBytes(&p, 2); }
        end_word(w);

        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("42");
        if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
          printf("    [2] FAIL: forth setup X=42 failed\n");
          subFail = 1;
        }

        if (!subFail) {
          bool err = run_word("SOOR");
          int32_t forthX = 0;
          longInteger_t li2;
          longIntegerInit(li2);
          convertLongIntegerRegisterToLongInteger(REGISTER_X, li2);
          longIntegerToInt32(li2, forthX);
          longIntegerFree(li2);
          int forthErr = lastErrorCode;

          if (nativeX != 42) {
            printf("    [2] FAIL: native X changed to %d (expected 42 unchanged)\n", nativeX);
            subFail = 1;
          }
          else if (nativeErr != ERROR_NONE) {
            printf("    [2] FAIL: native error %d (expected ERROR_NONE)\n", nativeErr);
            subFail = 1;
          }
          else if (forthX != 42) {
            printf("    [2] FAIL: forth X changed to %d (expected 42 unchanged)\n", forthX);
            subFail = 1;
          }
          else if (forthErr != ERROR_NONE) {
            printf("    [2] FAIL: forth error %d (expected ERROR_NONE)\n", forthErr);
            subFail = 1;
          }
        }
      }

      if (!subFail) {
        printf("    [2] PASS: out-of-range NUMBER_8 parity — both sides silent, X=42 unchanged\n");
      }
      else {
        fail |= 1;
      }

      forthDictClear();
      cleanupTestProgram();
    }
  }

  /* ---- Subcase 3: PTP_NONE dispatch still green through the seam ---- */
  {
    int subFail = 0;

    /* Use ITM_sin (76) — PTP_NONE, used by step-4 forthFindItem tests */
    uint16_t noneClass = (uint16_t)(indexOfItems[ITM_sin].status & PTP_STATUS);
    if (noneClass != PTP_NONE) {
      printf("    [3] CONFIG FAIL: ITM_sin ptpClass=0x%04X (expected PTP_NONE=0x%04X) — re-pick fixture item\n",
             noneClass, PTP_NONE);
      programRunStop = savedRS;
      fail |= 1;
    }
    else {
      uint16_t w = begin_word("SN01", 4);
      if (w == FORTH_NULL) {
        printf("    [3] FAIL: begin_word SN01 failed\n");
        programRunStop = savedRS;
        return 1;
      }
      forthDictEmit(T_C47);
      { uint16_t itemId = ITM_sin; forthDictEmitBytes(&itemId, 2); }
      end_word(w);

      /* Longint seed: sin() always produces a dtReal34 result regardless
       * of value or angular mode, so a type-change check below proves
       * ITM_sin actually dispatched rather than silently no-op'ing —
       * !err && lastErrorCode==ERROR_NONE alone can't distinguish "ran"
       * from "paramCoreValidateDirect's PTP_NONE arm returned true but
       * the seam never called reallyRunFunction." */
      forthPushInt32(0);
      lastErrorCode = ERROR_NONE;
      bool err = run_word("SN01");

      if (err) {
        printf("    [3] FAIL: PTP_NONE dispatch error %d\n", lastErrorCode);
        subFail = 1;
      }
      else if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: PTP_NONE dispatch set error %d\n", lastErrorCode);
        subFail = 1;
      }
      else if (getRegisterDataType(REGISTER_X) != dtReal34) {
        printf("    [3] FAIL: X type %u after ITM_sin, expected dtReal34 (sin did not run)\n",
               getRegisterDataType(REGISTER_X));
        subFail = 1;
      }

      if (!subFail) {
        printf("    [3] PASS: PTP_NONE dispatch through seam — ITM_sin executed, no error\n");
      }
      else {
        fail |= 1;
      }

      forthDictClear();
    }
  }

  programRunStop = savedRS;
  return fail;
}

/* test_param_parity_sweep
 * F2-4: pin native/Forth parameter parity across all PTP classes Forth
 * can carry.  Four independently reported subcases. */
static int test_param_parity_sweep(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;

  /* ---- Subcase 1: NUMBER_8 in-range + out-of-range parity ---- */
  {
    uint16_t sdlClass = (uint16_t)(indexOfItems[ITM_SDL].status & PTP_STATUS);
    if (sdlClass != PTP_NUMBER_8) {
      printf("    [1] CONFIG FAIL: ITM_SDL ptpClass=0x%04X (expected PTP_NUMBER_8=0x%04X)\n",
             sdlClass, PTP_NUMBER_8);
      programRunStop = savedRS;
      return 1;
    }

    uint16_t sdlMax = (uint16_t)(indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK);
    uint16_t sdlOutOfRange = sdlMax + 1;
    int subFail = 0;

    if (!paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, sdlMax)) {
      printf("    [1] CONFIG FAIL: sdlMax %u rejected by paramCoreValidateDirect (inclusive boundary failed)\n",
             sdlMax);
      programRunStop = savedRS;
      return 1;
    }

    /* --- 1a: in-range (param == max) --- */
    {
      uint8_t prog[] = {
        0x01, 0xFD, 0x03, 'F', '4', 'A',   /* LBL 'F4A' */
        0x81, 0xA7, (uint8_t)sdlMax,         /* SDL <max>  */
        0x04                                 /* RTN       */
      };

      if (!writeTestProgram(prog, sizeof(prog))) {
        printf("    [1a] FAIL: writeTestProgram failed\n");
        programRunStop = savedRS;
        return 1;
      }

      calcRegister_t lbl = findNamedLabel("F4A", GLOBAL_LABELS);
      if (lbl == INVALID_VARIABLE) {
        printf("    [1a] FAIL: findNamedLabel F4A failed\n");
        forthDictClear();
        cleanupTestProgram();
        programRunStop = savedRS;
        return 1;
      }

      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("1");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(1)) {
        printf("    [1a] FAIL: native setup X=1 failed\n");
        subFail = 1;
      }

      if (!subFail) {
        programRunStop = PGM_STOPPED;
        lastErrorCode = ERROR_NONE;
        dynamicMenuItem = -1;
        fnExecute(lbl);

        int32_t nativeX = 0;
        int nativeErr = lastErrorCode;
        {
          longInteger_t li;
          longIntegerInit(li);
          convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
          longIntegerToInt32(li, nativeX);
          longIntegerFree(li);
        }

        /* Forth half */
        forthDictClear();
        cleanupTestProgram();

        uint16_t w = begin_word("S1A", 3);
        if (w == FORTH_NULL) {
          printf("    [1a] FAIL: begin_word S1A failed\n");
          programRunStop = savedRS;
          return 1;
        }
        forthDictEmit(T_C47);
        { uint16_t itemId = ITM_SDL; forthDictEmitBytes(&itemId, 2); }
        { uint16_t p = sdlMax; forthDictEmitBytes(&p, 2); }
        end_word(w);

        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("1");
        if (lastErrorCode != ERROR_NONE || !x_is_longint(1)) {
          printf("    [1a] FAIL: forth setup X=1 failed\n");
          subFail = 1;
        }

        if (!subFail) {
          bool err = run_word("S1A");
          int32_t forthX = 0;
          int forthErr = lastErrorCode;
          if (!err) {
            longInteger_t li;
            longIntegerInit(li);
            convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
            longIntegerToInt32(li, forthX);
            longIntegerFree(li);
          }

          if (nativeX != forthX) {
            printf("    [1a] FAIL: in-range X mismatch native=%d forth=%d\n", nativeX, forthX);
            subFail = 1;
          } else if (nativeErr != forthErr) {
            printf("    [1a] FAIL: in-range error mismatch native=%d forth=%d\n", nativeErr, forthErr);
            subFail = 1;
          }
        }
      }

      forthDictClear();
      cleanupTestProgram();
    }

    /* --- 1b: out-of-range (param == max+1) --- */
    {
      if (sdlOutOfRange > 255) {
        printf("    [1b] CONFIG FAIL: out-of-range %u exceeds 255\n", sdlOutOfRange);
        subFail = 1;
      }

      if (!subFail) {
        uint8_t prog[] = {
          0x01, 0xFD, 0x03, 'F', '4', 'B',   /* LBL 'F4B' */
          0x81, 0xA7, (uint8_t)sdlOutOfRange, /* SDL <oor> */
          0x04                                 /* RTN       */
        };

        if (!writeTestProgram(prog, sizeof(prog))) {
          printf("    [1b] FAIL: writeTestProgram failed\n");
          programRunStop = savedRS;
          return 1;
        }

        calcRegister_t lbl = findNamedLabel("F4B", GLOBAL_LABELS);
        if (lbl == INVALID_VARIABLE) {
          printf("    [1b] FAIL: findNamedLabel F4B failed\n");
          forthDictClear();
          cleanupTestProgram();
          programRunStop = savedRS;
          return 1;
        }

        lastErrorCode = ERROR_NONE;
        forthOuterInterpret("42");
        if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
          printf("    [1b] FAIL: native setup X=42 failed\n");
          subFail = 1;
        }

        if (!subFail) {
          programRunStop = PGM_STOPPED;
          lastErrorCode = ERROR_NONE;
          dynamicMenuItem = -1;
          fnExecute(lbl);

          int32_t nativeX = 0;
          int nativeErr = lastErrorCode;
          {
            longInteger_t li;
            longIntegerInit(li);
            convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
            longIntegerToInt32(li, nativeX);
            longIntegerFree(li);
          }

          /* Forth half */
          forthDictClear();
          cleanupTestProgram();

          uint16_t w = begin_word("S1B", 3);
          if (w == FORTH_NULL) {
            printf("    [1b] FAIL: begin_word S1B failed\n");
            programRunStop = savedRS;
            return 1;
          }
          forthDictEmit(T_C47);
          { uint16_t itemId = ITM_SDL; forthDictEmitBytes(&itemId, 2); }
          { uint16_t p = sdlOutOfRange; forthDictEmitBytes(&p, 2); }
          end_word(w);

          lastErrorCode = ERROR_NONE;
          forthOuterInterpret("42");
          if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
            printf("    [1b] FAIL: forth setup X=42 failed\n");
            subFail = 1;
          }

          if (!subFail) {
            run_word("S1B");
            int32_t forthX = 0;
            int forthErr = lastErrorCode;
            {
              longInteger_t li;
              longIntegerInit(li);
              convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
              longIntegerToInt32(li, forthX);
              longIntegerFree(li);
            }

            if (nativeX != forthX) {
              printf("    [1b] FAIL: out-of-range X mismatch native=%d forth=%d\n", nativeX, forthX);
              subFail = 1;
            } else if (nativeErr != forthErr) {
              printf("    [1b] FAIL: out-of-range error mismatch native=%d forth=%d\n", nativeErr, forthErr);
              subFail = 1;
            }
          }
        }
      }

      forthDictClear();
      cleanupTestProgram();
    }

    if (!subFail) {
      printf("    [1] PASS: NUMBER_8 in-range (param=%u) + out-of-range (param=%u) parity pinned\n",
             sdlMax, sdlOutOfRange);
    } else {
      fail = 1;
    }
  }
  /* ---- Subcase 2: NUMBER_16 oldParam16 parity ---- */
  {
    uint16_t old16Id = LAST_ITEM;  /* sentinel: not found */
    int subFail = 0;

    for (uint16_t i = 1; i < LAST_ITEM && !subFail; i++) {
      uint16_t ptp = (uint16_t)(indexOfItems[i].status & PTP_STATUS);
      if (ptp == PTP_NUMBER_16 && isFunctionOldParam16(i)) {
        old16Id = i;
        break;
      }
    }

    if (old16Id >= LAST_ITEM) {
      printf("    [2] CONFIG FAIL: no PTP_NUMBER_16 + isFunctionOldParam16 item found\n");
      fail = 1;
      programRunStop = savedRS;
      return 1;
    }

    /* Prove no earlier id matches (independent of discovery loop) */
    {
      uint16_t earlier = 0;
      for (uint16_t j = 1; j < old16Id; j++) {
        uint16_t ptp = (uint16_t)(indexOfItems[j].status & PTP_STATUS);
        if (ptp == PTP_NUMBER_16 && isFunctionOldParam16(j)) {
          earlier = j;
          break;
        }
      }
      if (earlier) {
        printf("    [2] CONFIG FAIL: earlier matching id %u exists before discovered id %u\n",
               earlier, old16Id);
        fail = 1;
        programRunStop = savedRS;
        return 1;
      }
    }

    printf("    [2] discovered oldParam16 item: id=%u (%s)\n",
           old16Id, indexOfItems[old16Id].itemCatalogName);

    {
      uint16_t param = 5;
      uint8_t hi = (uint8_t)(old16Id >> 8);
      uint8_t lo = (uint8_t)(old16Id & 0xFF);
      char savedStatMx[sizeof(statMx)];
      uint16_t savedLrSelection = lrSelection;
      uint16_t savedLrChosen = lrChosen;
      uint8_t savedTemporaryInformation = temporaryInformation;
      xcopy(savedStatMx, statMx, sizeof(savedStatMx));

      /* Native: opcode bytes + LE param (low, high) */
      uint8_t prog[8];
      uint16_t progLen;
      if (old16Id < 128) {
        prog[0] = (uint8_t)old16Id;
        prog[1] = (uint8_t)(param & 0xFF);
        prog[2] = (uint8_t)(param >> 8);
        progLen = 3;
      } else {
        prog[0] = 0x80 | (hi & 0x7F);
        prog[1] = lo;
        prog[2] = (uint8_t)(param & 0xFF);
        prog[3] = (uint8_t)(param >> 8);
        progLen = 4;
      }
      /* Prepend LBL 'F4C', append RTN */
      uint8_t fullProg[20];
      uint16_t fullLen = 0;
      fullProg[fullLen++] = 0x01;
      fullProg[fullLen++] = 0xFD;
      fullProg[fullLen++] = 0x03;
      fullProg[fullLen++] = 'F';
      fullProg[fullLen++] = '4';
      fullProg[fullLen++] = 'C';
      for (uint16_t k = 0; k < progLen; k++) fullProg[fullLen++] = prog[k];
      fullProg[fullLen++] = 0x04;

      if (!writeTestProgram(fullProg, fullLen)) {
        printf("    [2] FAIL: writeTestProgram failed\n");
        programRunStop = savedRS;
        return 1;
      }

      calcRegister_t lbl = findNamedLabel("F4C", GLOBAL_LABELS);
      if (lbl == INVALID_VARIABLE) {
        printf("    [2] FAIL: findNamedLabel F4C failed\n");
        forthDictClear();
        cleanupTestProgram();
        programRunStop = savedRS;
        return 1;
      }

      strcpy(statMx, "STATS");
      lrSelection = savedLrSelection;
      lrChosen = savedLrChosen;
      temporaryInformation = savedTemporaryInformation;
      seedParamParityState();
      {
        uint8_t tType, zType, yType, xType;
        int32_t tVal, zVal, yVal, xVal;
        read_reg_int32(REGISTER_T, &tType, &tVal);
        read_reg_int32(REGISTER_Z, &zType, &zVal);
        read_reg_int32(REGISTER_Y, &yType, &yVal);
        read_reg_int32(REGISTER_X, &xType, &xVal);
        if (tType != dtLongInteger || tVal != 11 ||
            zType != dtLongInteger || zVal != 22 ||
            yType != dtLongInteger || yVal != 33 ||
            xType != dtLongInteger || xVal != 44) {
          printf("    [2] FAIL: native seed mismatch T=%d Z=%d Y=%d X=%d\n", tVal, zVal, yVal, xVal);
          subFail = 1;
        }
      }

      if (!subFail) {
        fnExecute(lbl);

        int32_t nativeX = 0;
        int nativeErr = lastErrorCode;
        {
          longInteger_t li;
          longIntegerInit(li);
          convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
          longIntegerToInt32(li, nativeX);
          longIntegerFree(li);
        }

        /* Forth half */
        forthDictClear();
        cleanupTestProgram();

        uint16_t w = begin_word("S2O", 3);
        if (w == FORTH_NULL) {
          printf("    [2] FAIL: begin_word S2O failed\n");
          xcopy(statMx, savedStatMx, sizeof(savedStatMx));
          lrSelection = savedLrSelection;
          lrChosen = savedLrChosen;
          temporaryInformation = savedTemporaryInformation;
          programRunStop = savedRS;
          return 1;
        }
        forthDictEmit(T_C47);
        { uint16_t itemId = old16Id; forthDictEmitBytes(&itemId, 2); }
        { uint16_t p = 5; forthDictEmitBytes(&p, 2); }
        end_word(w);

        strcpy(statMx, "STATS");
        lrSelection = savedLrSelection;
        lrChosen = savedLrChosen;
        temporaryInformation = savedTemporaryInformation;
        seedParamParityState();
        {
          uint8_t tType, zType, yType, xType;
          int32_t tVal, zVal, yVal, xVal;
          read_reg_int32(REGISTER_T, &tType, &tVal);
          read_reg_int32(REGISTER_Z, &zType, &zVal);
          read_reg_int32(REGISTER_Y, &yType, &yVal);
          read_reg_int32(REGISTER_X, &xType, &xVal);
          if (tType != dtLongInteger || tVal != 11 ||
              zType != dtLongInteger || zVal != 22 ||
              yType != dtLongInteger || yVal != 33 ||
              xType != dtLongInteger || xVal != 44) {
            printf("    [2] FAIL: forth seed mismatch T=%d Z=%d Y=%d X=%d\n", tVal, zVal, yVal, xVal);
            subFail = 1;
          }
        }

        if (!subFail) {
          run_word("S2O");
          int32_t forthX = 0;
          int forthErr = lastErrorCode;
          {
            longInteger_t li;
            longIntegerInit(li);
            convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
            longIntegerToInt32(li, forthX);
            longIntegerFree(li);
          }

          if (nativeX != forthX) {
            printf("    [2] FAIL: X mismatch native=%d forth=%d\n", nativeX, forthX);
            subFail = 1;
          } else if (nativeErr != forthErr) {
            printf("    [2] FAIL: error mismatch native=%d forth=%d\n", nativeErr, forthErr);
            subFail = 1;
          }
        }

        forthDictClear();
        cleanupTestProgram();
      }

      xcopy(statMx, savedStatMx, sizeof(savedStatMx));
      lrSelection = savedLrSelection;
      lrChosen = savedLrChosen;
      temporaryInformation = savedTemporaryInformation;
    }

    if (!subFail) {
      printf("    [2] PASS: first NUMBER_16 oldParam16 parity pinned (item=%u)\n", old16Id);
    } else {
      fail = 1;
    }
  }
  /* ---- Subcase 3: NUMBER_16 new-form parity ---- */
  {
    uint16_t new16Id = LAST_ITEM;  /* sentinel: not found */
    int subFail = 0;

    for (uint16_t i = 1; i < LAST_ITEM && !subFail; i++) {
      uint16_t ptp = (uint16_t)(indexOfItems[i].status & PTP_STATUS);
      if (ptp == PTP_NUMBER_16 && !isFunctionOldParam16(i)) {
        new16Id = i;
        break;
      }
    }

    if (new16Id >= LAST_ITEM) {
      printf("    [3] CONFIG FAIL: no PTP_NUMBER_16 + !isFunctionOldParam16 item found\n");
      fail |= 1;
    } else {
      /* Prove no earlier id matches (independent of discovery loop) */
      {
        uint16_t earlier = 0;
        for (uint16_t j = 1; j < new16Id; j++) {
          uint16_t ptp = (uint16_t)(indexOfItems[j].status & PTP_STATUS);
          if (ptp == PTP_NUMBER_16 && !isFunctionOldParam16(j)) {
            earlier = j;
            break;
          }
        }
        if (earlier) {
          printf("    [3] CONFIG FAIL: earlier matching id %u exists before discovered id %u\n",
                 earlier, new16Id);
          fail = 1;
          programRunStop = savedRS;
          return 1;
        }
      }

      if (!subFail) {
        printf("    [3] discovered new-form item: id=%u (%s)\n",
               new16Id, indexOfItems[new16Id].itemCatalogName);

      {
        uint16_t param = 5;
        uint8_t hi = (uint8_t)(new16Id >> 8);
        uint8_t lo = (uint8_t)(new16Id & 0xFF);
        char savedStatMx[sizeof(statMx)];
        uint16_t savedLrSelection = lrSelection;
        uint16_t savedLrChosen = lrChosen;
        uint8_t savedTemporaryInformation = temporaryInformation;
        xcopy(savedStatMx, statMx, sizeof(savedStatMx));

        /* Native: opcode bytes + BE param (high, low) */
        uint8_t prog[8];
        uint16_t progLen;
        if (new16Id < 128) {
          prog[0] = (uint8_t)new16Id;
          prog[1] = (uint8_t)(param >> 8);
          prog[2] = (uint8_t)(param & 0xFF);
          progLen = 3;
        } else {
          prog[0] = 0x80 | (hi & 0x7F);
          prog[1] = lo;
          prog[2] = (uint8_t)(param >> 8);
          prog[3] = (uint8_t)(param & 0xFF);
          progLen = 4;
        }
        /* Prepend LBL 'F4D', append RTN */
        uint8_t fullProg[20];
        uint16_t fullLen = 0;
        fullProg[fullLen++] = 0x01;
        fullProg[fullLen++] = 0xFD;
        fullProg[fullLen++] = 0x03;
        fullProg[fullLen++] = 'F';
        fullProg[fullLen++] = '4';
        fullProg[fullLen++] = 'D';
        for (uint16_t k = 0; k < progLen; k++) fullProg[fullLen++] = prog[k];
        fullProg[fullLen++] = 0x04;

        if (!writeTestProgram(fullProg, fullLen)) {
          printf("    [3] FAIL: writeTestProgram failed\n");
          programRunStop = savedRS;
          return 1;
        }

        calcRegister_t lbl = findNamedLabel("F4D", GLOBAL_LABELS);
        if (lbl == INVALID_VARIABLE) {
          printf("    [3] FAIL: findNamedLabel F4D failed\n");
          forthDictClear();
          cleanupTestProgram();
          programRunStop = savedRS;
          return 1;
        }

        strcpy(statMx, "STATS");
        lrSelection = savedLrSelection;
        lrChosen = savedLrChosen;
        temporaryInformation = savedTemporaryInformation;
        seedParamParityState();
        {
          uint8_t tType, zType, yType, xType;
          int32_t tVal, zVal, yVal, xVal;
          read_reg_int32(REGISTER_T, &tType, &tVal);
          read_reg_int32(REGISTER_Z, &zType, &zVal);
          read_reg_int32(REGISTER_Y, &yType, &yVal);
          read_reg_int32(REGISTER_X, &xType, &xVal);
          if (tType != dtLongInteger || tVal != 11 ||
              zType != dtLongInteger || zVal != 22 ||
              yType != dtLongInteger || yVal != 33 ||
              xType != dtLongInteger || xVal != 44) {
            printf("    [3] FAIL: native seed mismatch T=%d Z=%d Y=%d X=%d\n", tVal, zVal, yVal, xVal);
            subFail = 1;
          }
        }

        if (!subFail) {
          fnExecute(lbl);

          int32_t nativeX = 0;
          int nativeErr = lastErrorCode;
          {
            longInteger_t li;
            longIntegerInit(li);
            convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
            longIntegerToInt32(li, nativeX);
            longIntegerFree(li);
          }

          /* Forth half */
          forthDictClear();
          cleanupTestProgram();

          uint16_t w = begin_word("S3N", 3);
          if (w == FORTH_NULL) {
            printf("    [3] FAIL: begin_word S3N failed\n");
            xcopy(statMx, savedStatMx, sizeof(savedStatMx));
            lrSelection = savedLrSelection;
            lrChosen = savedLrChosen;
            temporaryInformation = savedTemporaryInformation;
            programRunStop = savedRS;
            return 1;
          }
          forthDictEmit(T_C47);
          { uint16_t itemId = new16Id; forthDictEmitBytes(&itemId, 2); }
          { uint16_t p = 5; forthDictEmitBytes(&p, 2); }
          end_word(w);

          strcpy(statMx, "STATS");
          lrSelection = savedLrSelection;
          lrChosen = savedLrChosen;
          temporaryInformation = savedTemporaryInformation;
          seedParamParityState();
          {
            uint8_t tType, zType, yType, xType;
            int32_t tVal, zVal, yVal, xVal;
            read_reg_int32(REGISTER_T, &tType, &tVal);
            read_reg_int32(REGISTER_Z, &zType, &zVal);
            read_reg_int32(REGISTER_Y, &yType, &yVal);
            read_reg_int32(REGISTER_X, &xType, &xVal);
            if (tType != dtLongInteger || tVal != 11 ||
                zType != dtLongInteger || zVal != 22 ||
                yType != dtLongInteger || yVal != 33 ||
                xType != dtLongInteger || xVal != 44) {
              printf("    [3] FAIL: forth seed mismatch T=%d Z=%d Y=%d X=%d\n", tVal, zVal, yVal, xVal);
              subFail = 1;
            }
          }

          if (!subFail) {
            run_word("S3N");
            int32_t forthX = 0;
            int forthErr = lastErrorCode;
            {
              longInteger_t li;
              longIntegerInit(li);
              convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
              longIntegerToInt32(li, forthX);
              longIntegerFree(li);
            }

            if (nativeX != forthX) {
              printf("    [3] FAIL: X mismatch native=%d forth=%d\n", nativeX, forthX);
              subFail = 1;
            } else if (nativeErr != forthErr) {
              printf("    [3] FAIL: error mismatch native=%d forth=%d\n", nativeErr, forthErr);
              subFail = 1;
            }
          }

          forthDictClear();
          cleanupTestProgram();
        }

        xcopy(statMx, savedStatMx, sizeof(savedStatMx));
        lrSelection = savedLrSelection;
        lrChosen = savedLrChosen;
        temporaryInformation = savedTemporaryInformation;
      }

      if (!subFail) {
        printf("    [3] PASS: first NUMBER_16 new-form parity pinned (item=%u)\n", new16Id);
      } else {
        fail |= 1;
      }
    }
  }
  }
  /* ---- Subcase 4: Corrupted itemId still rejected at runtime ---- */
  {
    int subFail = 0;

    uint16_t w = begin_word("S4C", 3);
    if (w == FORTH_NULL) {
      printf("    [4] FAIL: begin_word S4C failed\n");
      programRunStop = savedRS;
      return 1;
    }
    forthDictEmit(T_C47);
    { uint16_t itemId = LAST_ITEM; forthDictEmitBytes(&itemId, 2); }
    end_word(w);

    lastErrorCode = ERROR_NONE;
    bool err = run_word("S4C");

    if (!err || lastErrorCode != ERROR_INVALID_CORRUPTED_DATA) {
      printf("    [4] FAIL: expected ERROR_INVALID_CORRUPTED_DATA (%d), got error=%d lastErrorCode=%d\n",
             ERROR_INVALID_CORRUPTED_DATA, err, lastErrorCode);
      subFail = 1;
    }

    if (forthTestGetDepth() != 0) {
      printf("    [4] FAIL: depth not unwound, forthTestGetDepth()=%u\n", forthTestGetDepth());
      subFail = 1;
    }

    if (!subFail) {
      printf("    [4] PASS: corrupted itemId %u rejected with ERROR_INVALID_CORRUPTED_DATA, depth unwound\n",
             LAST_ITEM);
    } else {
      fail |= 1;
    }

    forthDictClear();
  }
  programRunStop = savedRS;
  return fail;
}

/* test_param_textual_numeric
 * F4-1: parameter classification + direct numeric parameters.
 * Subcase 1: NUMBER_8 compile path
 * Subcase 2: NUMBER_8 execute path
 * Subcase 3: NUMBER_16 compile path
 * Subcase 4: NUMBER_8_16 compile path (short + extended)
 * Subcase 5: Range error (value > max)
 * Subcase 6: Invalid token (non-digit)
 * Subcase 7: Flow reject (RTN)
 *
 * Escaping mutation 1: revert step-4 to blanket reject (F3-6) — subcases 1-4 fail.
 * Escaping mutation 2: remove PTP_NUMBER_8_16 from paramCoreValidateDirect — subcase 4 fails.
 * Escaping mutation 3: remove PTP_NUMBER_8_16 from forth_inner.c decode — runtime of W4 errors.
 * Escaping mutation 4: omit forthItemIsFlowReject in step-4 — subcase 7 fails (RTN runs).
 * Escaping mutation 5: remove TAM_MIN/MAX check — subcase 5 fails (no range error).
 */
static int test_param_textual_numeric(void)
{
  int fail = 0;

  /* Subcase 1: NUMBER_8 compile path */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string(": W1 RMODE 3 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: compile RMODE 3 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [1] PASS: NUMBER_8 compile RMODE 3\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 2: NUMBER_8 execute path */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string("RMODE 3");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: execute RMODE 3 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [2] PASS: NUMBER_8 execute RMODE 3\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 3: NUMBER_16 compile path */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string(": W2 BestF 100 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile BestF 100 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [3] PASS: NUMBER_16 compile BestF 100\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 4: NUMBER_8_16 compile path (short) + extended decode + interpret */
  { int subFail = 0;
    /* Short form: compile CNST 10 (value 10 <= 249 -> [10][0]) */
    lastErrorCode = ERROR_NONE;
    x_set_string(": W3 CNST 10 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: compile CNST 10 error %d\n", lastErrorCode);
      subFail = 1;
    }
    /* Interpret path: CNST 10 -> exercises paramCoreValidateDirect(PTP_NUMBER_8_16)
     * X starts as dtLongInteger; CNST should set it to dtReal34.
     * Mutation 2 (remove PTP_NUMBER_8_16 validation) skips dispatch -> X stays dtLongInteger. */
    if (!subFail) {
      forthPushInt32(0);
      lastErrorCode = ERROR_NONE;
      x_set_string("CNST 10");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: interpret CNST 10 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      if (getRegisterDataType(REGISTER_X) != dtReal34) {
        printf("    [4] FAIL: CNST 10 did not set X to dtReal34 (type %u)\n",
               getRegisterDataType(REGISTER_X));
        subFail = 1;
      }
    }
    /* Extended decode: hand-assemble [FTOK_C47][ITM_CNST][250][50][FTOK_EXIT]
     * -> value = 250 + 50 = 300 (bypasses outer-interpreter range check,
     * exercises the inner interpreter PTP_NUMBER_8_16 decoder) */
    uint16_t w4 = 0;
    if (!subFail) {
      w4 = begin_word("W4", 3);
      if (w4 == FORTH_NULL) {
        printf("    [4] FAIL: begin_word W4\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      forthDictEmit(T_C47);
      { uint16_t itemId = 207; forthDictEmitBytes(&itemId, 2); } /* ITM_CNST */
      { uint8_t ext[2] = {250, 50}; forthDictEmitBytes(ext, 2); } /* extended: 250+50=300 */
      end_word(w4);
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      bool err = run_word("W4");
      if (err) {
        printf("    [4] FAIL: extended decode W4 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [4] PASS: NUMBER_8_16 compile CNST 10 (short) + interpret + extended decode [250][50]=300\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 5: Range error (value > max) */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string("RMODE 10");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_OUT_OF_RANGE) {
      printf("    [5] FAIL: expected ERROR_OUT_OF_RANGE, got %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [5] PASS: range error RMODE 10 -> ERROR_OUT_OF_RANGE\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 6: Invalid token (non-digit) */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string("RMODE abc");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [6] FAIL: expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [6] PASS: invalid token RMODE abc -> ERROR_INVALID_NAME\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 7: Flow reject (RTN) */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    x_set_string("RTN");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
      printf("    [7] FAIL: expected ERROR_OPERATION_UNDEFINED, got %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      printf("    [7] PASS: flow reject RTN -> ERROR_OPERATION_UNDEFINED\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  return fail;
}

/* test_param_register_flag
 * F4-2: register, flag, and shuffle direct parameter forms.
 * Subcase 1: STO/RCL numbered round-trip, interpret + compiled
 * Subcase 2: Letter registers
 * Subcase 3: Stat-letter conversion is live
 * Subcase 4: Local dot form encodes and stays silent unallocated
 * Subcase 5: Flag forms
 * Subcase 6: Shuffle
 * Subcase 7: Validator arms
 */
static int test_param_register_flag(void)
{
  int fail = 0;
  char sbuf[64];

  /* Subcase 1: STO/RCL numbered round-trip, compiled */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    x_set_string(": SR0 STO 05 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: compile SR0 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RR0 RCL 05 ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: compile RR0 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(7);
      run_word("SR0");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: STO 05 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      run_word("RR0");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: RCL 05 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail && !x_is_longint(7)) {
      printf("    [1] FAIL: RCL 05 did not return 7\n");
      subFail = 1;
    }
    /* Byte-image pin (mirrors subcase 2/3's pins, for consistency): a
     * dedicated freshly-compiled probe word, since fdict.latest (used
     * below to locate the body without a byte-offset-vs-ref-index mixup)
     * must be the newest header — SR0 no longer is, once RR0 compiles
     * after it. ": PR0 STO 05 ;" body cells are FTOK_C47, ITM_STO,
     * {5,0}, FTOK_EXIT. */
    if (!subFail) {
      x_set_string(": PR0 STO 05 ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: compile PR0 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PR0", &w)) {
        printf("    [1] FAIL: PR0 not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != ITM_STO || cell2 != 5 || cell3 != T_EXIT) {
        printf("    [1] FAIL: byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 0x%04X, 5, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, ITM_STO, T_EXIT);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [1] PASS: STO/RCL 05 round-trips (compiled)\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 2: Letter registers */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    x_set_string(": SA STO A ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: compile SA error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RA RCL A ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: compile RA error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(13);
      run_word("SA");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: STO A error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      run_word("RA");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: RCL A error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail && !x_is_longint(13)) {
      printf("    [2] FAIL: RCL A did not return 13\n");
      subFail = 1;
    }
    /* Byte-image pin: ": PRA STO A ;" body cells are FTOK_C47, ITM_STO, {104,0}, FTOK_EXIT */
    if (!subFail) {
      x_set_string(": PRA STO A ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: compile PRA error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PRA", &w)) {
        printf("    [2] FAIL: PRA not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      /* forthFindColon yields a REF index, not a byte offset; the word under
       * test is always the newest header, so walk from fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != ITM_STO || cell2 != 104 || cell3 != T_EXIT) {
        printf("    [2] FAIL: byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 0x%04X, 104, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, ITM_STO, T_EXIT);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [2] PASS: lettered register A maps to KS 104 and round-trips\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 3: Stat-letter conversion is live */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    x_set_string(": SM STO M ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile SM error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RM RCL M ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: compile RM error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(21);
      run_word("SM");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: STO M error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      run_word("RM");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: RCL M error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail && !x_is_longint(21)) {
      printf("    [3] FAIL: RCL M did not return 21\n");
      subFail = 1;
    }
    /* Byte-image pin (mirrors subcase 2's PRA pin): a round-trip alone
     * can't distinguish REGISTER_M_IN_KS_CODE (211) from a transcription
     * error in paramLetterKS[] that swaps M for another stat letter —
     * STO and RCL would then read the same wrong table row and round-trip
     * clean against the wrong register. ": PRM STO M ;" body cells are
     * FTOK_C47, ITM_STO, {211,0}, FTOK_EXIT. */
    if (!subFail) {
      x_set_string(": PRM STO M ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: compile PRM error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PRM", &w)) {
        printf("    [3] FAIL: PRM not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != ITM_STO || cell2 != 211 || cell3 != T_EXIT) {
        printf("    [3] FAIL: byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 0x%04X, 211, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, ITM_STO, T_EXIT);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [3] PASS: stat register M stores through regKStoC\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 4: Local dot form encodes and stays silent unallocated */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    /* Byte-image: ": PRL STO .05 ;" body cell {117, 0} (112+5) */
    x_set_string(": PRL STO .05 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: compile PRL error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PRL", &w)) {
        printf("    [4] FAIL: PRL not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      /* forthFindColon yields a REF index, not a byte offset; the word under
       * test is always the newest header, so walk from fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != ITM_STO || cell2 != 117 || cell3 != T_EXIT) {
        printf("    [4] FAIL: byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 0x%04X, 117, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, ITM_STO, T_EXIT);
        subFail = 1;
      }
    }
    /* Behavior parity (traced 2026-07-19, corrects the F4-2 packet): the native
     * PARAM_REGISTER arm gates on regInRange(), and regInRange() is NOT a pure
     * predicate — on a miss it calls displayCalcErrorMessage(ERROR_OUT_OF_RANGE)
     * itself (store.c:17-72). So an unallocated local is NOT silent natively:
     * it raises OUT_OF_RANGE and performs no store. We mirror that exactly. */
    if (!subFail) {
      x_set_string(": PRL2 STO .05 ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: compile PRL2 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(31);
      run_word("PRL2");
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [4] FAIL: STO .05 unallocated expected ERROR_OUT_OF_RANGE, got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail && !x_is_longint(31)) {
      printf("    [4] FAIL: X should still be 31 after the rejected STO .05\n");
      subFail = 1;
    }
    if (!subFail) {
      printf("    [4] PASS: .05 encodes KS 117; unallocated locals raise OUT_OF_RANGE and never store\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 5: Flag forms */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    /* Byte-images */
    x_set_string(": PF1 SF 10 ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: compile PF1 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PF1", &w)) {
        printf("    [5] FAIL: PF1 not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      /* forthFindColon yields a REF index, not a byte offset; the word under
       * test is always the newest header, so walk from fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != ITM_SF || cell2 != 10 || cell3 != T_EXIT) {
        printf("    [5] FAIL: PF1 byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 0x%04X, 10, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, ITM_SF, T_EXIT);
        subFail = 1;
      }
    }
    /* PF2: SF .31 -> {143, 0} */
    if (!subFail) {
      x_set_string(": PF2 SF .31 ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: compile PF2 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PF2", &w)) {
        printf("    [5] FAIL: PF2 not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      /* forthFindColon yields a REF index, not a byte offset; the word under
       * test is always the newest header, so walk from fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell2;
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      if (cell2 != 143) {
        printf("    [5] FAIL: PF2 param cell 0x%04X, expected 143\n", cell2);
        subFail = 1;
      }
    }
    /* Errors */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      x_set_string("SF .32");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [5] FAIL: SF .32 expected ERROR_OUT_OF_RANGE, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      x_set_string(": SFQ SF q ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [5] FAIL: SF q expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      x_set_string(": CF100 CF 100 ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [5] FAIL: CF 100 expected ERROR_OUT_OF_RANGE, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [5] PASS: flag forms encode and bound correctly\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 6: Shuffle */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    /* Build source with item's own glyph name */
    if ((indexOfItems[1694].status & PTP_STATUS) != PTP_SHUFFLE) {
      printf("    [6] FAIL: item 1694 does not have PTP_SHUFFLE status\n");
      subFail = 1;
    }
    if (!subFail) {
      sprintf(sbuf, "%s yxzt", indexOfItems[1694].itemCatalogName);
    }
    /* Byte-image via ": PSH <glyph> yxzt ;" */
    if (!subFail) {
      char cbuf[128];
      sprintf(cbuf, ": PSH %s ;", sbuf);
      x_set_string(cbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [6] FAIL: compile PSH error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t w;
      if (!forthFindColon("PSH", &w)) {
        printf("    [6] FAIL: PSH not found\n");
        subFail = 1;
      }
    }
    if (!subFail) {
      /* forthFindColon yields a REF index, not a byte offset; the word under
       * test is always the newest header, so walk from fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint16_t cell0, cell1, cell2, cell3;
      memcpy(&cell0, fdict.base + bodyStart, 2);
      memcpy(&cell1, fdict.base + bodyStart + 2, 2);
      memcpy(&cell2, fdict.base + bodyStart + 4, 2);
      memcpy(&cell3, fdict.base + bodyStart + 6, 2);
      if (cell0 != T_C47 || cell1 != 1694 || cell2 != 0xE1 || cell3 != T_EXIT) {
        printf("    [6] FAIL: byte image {0x%04X, 0x%04X, 0x%04X, 0x%04X} expected {0x%04X, 1694, 0xE1, 0x%04X}\n",
               cell0, cell1, cell2, cell3, T_C47, T_EXIT);
        subFail = 1;
      }
    }
    /* Behavior: seed T=11,Z=22,Y=33,X=44, interpret -> X==33, Y==44, Z==22, T==11.
     * The seed MUST ride in the source line: x_set_string overwrites X with the
     * source string and fnForthOuter drops it, so anything pushed beforehand is
     * shifted out of place before the shuffle ever runs. */
    if (!subFail) {
      char rbuf[128];
      lastErrorCode = ERROR_NONE;
      sprintf(rbuf, "11 22 33 44 %s", sbuf);
      x_set_string(rbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [6] FAIL: shuffle yxzt error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint8_t ttype; int32_t tval;
      read_reg_int32(REGISTER_X, &ttype, &tval);
      if (ttype != dtLongInteger || tval != 33) {
        printf("    [6] FAIL: X = %d expected 33\n", tval);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint8_t ttype; int32_t tval;
      read_reg_int32(REGISTER_Y, &ttype, &tval);
      if (ttype != dtLongInteger || tval != 44) {
        printf("    [6] FAIL: Y = %d expected 44\n", tval);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint8_t ttype; int32_t tval;
      read_reg_int32(REGISTER_Z, &ttype, &tval);
      if (ttype != dtLongInteger || tval != 22) {
        printf("    [6] FAIL: Z = %d expected 22\n", tval);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint8_t ttype; int32_t tval;
      read_reg_int32(REGISTER_T, &ttype, &tval);
      if (ttype != dtLongInteger || tval != 11) {
        printf("    [6] FAIL: T = %d expected 11\n", tval);
        subFail = 1;
      }
    }
    /* Malformed */
    if (!subFail) {
      char bad0[64];
      sprintf(bad0, "%s y", indexOfItems[1694].itemCatalogName);
      lastErrorCode = ERROR_NONE;
      x_set_string(bad0);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: y expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      char bad1[64];
      sprintf(bad1, "%s yxz", indexOfItems[1694].itemCatalogName);
      lastErrorCode = ERROR_NONE;
      x_set_string(bad1);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: yxz expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      char bad2[64];
      sprintf(bad2, "%s yxzq", indexOfItems[1694].itemCatalogName);
      lastErrorCode = ERROR_NONE;
      x_set_string(bad2);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: yxzq expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [6] PASS: shuffle yxzt packs to 0xE1 and swaps X/Y\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 7: Validator arms */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthGDictClear();
    /* REGISTER cell {230, 0} -> RESET (illegal byte) */
    {
      uint16_t w = gbegin_word("VR1", 3);
      if (w == FORTH_NULL) { printf("    [7] FAIL: alloc VR1\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        { uint16_t cell = 230; gemit_bytes((uint8_t[]){cell, cell >> 8}, 2); }
      }
      if (!subFail) gend_word();
      uint8_t *preBase = gdict.base;
      uint16_t preBlocks = gdict.sizeBlocks;
      forthGDictValidateRestored();
      if (gdict.base != NULL) {
        printf("    [7] FAIL: REGISTER {230,0} survived validation\n");
        subFail = 1;
        forthGDictClear();
      } else if (preBase) {
        freeC47Blocks(preBase, preBlocks);
      }
    }
    /* FLAG cell {150, 0} -> RESET (144..210 hole) */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VF1", 3);
      if (w == FORTH_NULL) { printf("    [7] FAIL: alloc VF1\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_SF; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        { uint16_t cell = 150; gemit_bytes((uint8_t[]){cell, cell >> 8}, 2); }
      }
      if (!subFail) gend_word();
      uint8_t *preBase = gdict.base;
      uint16_t preBlocks = gdict.sizeBlocks;
      forthGDictValidateRestored();
      if (gdict.base != NULL) {
        printf("    [7] FAIL: FLAG {150,0} survived validation\n");
        subFail = 1;
        forthGDictClear();
      } else if (preBase) {
        freeC47Blocks(preBase, preBlocks);
      }
    }
    /* SHUFFLE cell {0xE1, 1} -> RESET (pad byte) */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VS1", 3);
      if (w == FORTH_NULL) { printf("    [7] FAIL: alloc VS1\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = 1694; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        { uint16_t cell = 0x01E1; gemit_bytes((uint8_t[]){cell, cell >> 8}, 2); }
      }
      if (!subFail) gend_word();
      uint8_t *preBase = gdict.base;
      uint16_t preBlocks = gdict.sizeBlocks;
      forthGDictValidateRestored();
      if (gdict.base != NULL) {
        printf("    [7] FAIL: SHUFFLE {0xE1,1} survived validation\n");
        subFail = 1;
        forthGDictClear();
      } else if (preBase) {
        freeC47Blocks(preBase, preBlocks);
      }
    }
    /* REGISTER cell {104, 0} -> ACCEPT */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VR2", 3);
      if (w == FORTH_NULL) { printf("    [7] FAIL: alloc VR2\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        { uint16_t cell = 104; gemit_bytes((uint8_t[]){cell, cell >> 8}, 2); }
      }
      if (!subFail) gend_word();
      uint8_t *preBase = gdict.base;
      uint16_t preBlocks = gdict.sizeBlocks;
      forthGDictValidateRestored();
      if (gdict.base == NULL) {
        printf("    [7] FAIL: REGISTER {104,0} reset (should accept)\n");
        subFail = 1;
        if (preBase) freeC47Blocks(preBase, preBlocks);
      } else {
        forthGDictClear();
      }
    }
    if (!subFail) {
      printf("    [7] PASS: walk arms enforce register/flag/shuffle cell legality\n");
    } else {
      fail |= 1;
    }
    forthGDictClear();
  }

  return fail;
}

/* ==========================================================================
 * F4-3: Named, system-flag, and indirect parameter forms
 * ========================================================================== */

static int test_param_named_indirect(void)
{
  int fail = 0;
  char sbuf[96];
  uint16_t savedNamedVars = numberOfNamedVariables;
  uint16_t menuItem = 0, n16Item = 0;

  /* Runtime discovery — never hardcode an item id (F3-core §0). */
  { uint16_t id;
    for (id = 1; id < LAST_ITEM; id++) {
      uint16_t st = indexOfItems[id].status;
      if ((st & CAT_STATUS) != CAT_FNCT) continue;
      if (!menuItem && (st & PTP_STATUS) == PTP_MENU) menuItem = id;
      if (!n16Item && (st & PTP_STATUS) == PTP_NUMBER_16 && !forthItemIsFlowReject(id)) n16Item = id;
    }
  }
  if (!menuItem || !n16Item) {
    printf("    CONFIG FAIL: no PTP_MENU (%u) / PTP_NUMBER_16 (%u) item found\n", menuItem, n16Item);
    return 1;
  }

  /* Subcase 1: named variable create + recall round-trip, and the inherited
   * ERROR_UNDEF_SOURCE_VAR miss. Note forthOuterInterpret (not fnForthOuter):
   * x_set_string would overwrite the seeded X with the source string. */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    forthPushInt32(42);
    forthOuterInterpret("STO 'VZ'");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: STO 'VZ' error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      forthPushInt32(1);
      forthOuterInterpret("RCL 'VZ'");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: RCL 'VZ' error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail && !x_is_longint(42)) {
      printf("    [1] FAIL: RCL 'VZ' did not return 42\n");
      subFail = 1;
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("RCL 'VMISS'");
      if (lastErrorCode != ERROR_UNDEF_SOURCE_VAR) {
        printf("    [1] FAIL: RCL 'VMISS' expected ERROR_UNDEF_SOURCE_VAR, got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) {
      printf("    [1] PASS: named variable creation and UNDEF_SOURCE_VAR inherit from the core\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 2: named encode image — [253][len] + name, odd len zero-padded. */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    x_set_string(": PN1 STO 'VZ' ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: compile PN1 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      /* forthFindColon returns a ref index, not an offset: walk fdict.latest. */
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint8_t expect[10];
      uint16_t t;
      t = T_C47;   memcpy(expect + 0, &t, 2);
      t = ITM_STO; memcpy(expect + 2, &t, 2);
      expect[4] = 253; expect[5] = 2; expect[6] = 'V'; expect[7] = 'Z';
      t = T_EXIT;  memcpy(expect + 8, &t, 2);
      if (memcmp(fdict.base + bodyStart, expect, 10) != 0) {
        printf("    [2] FAIL: PN1 image %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
               fdict.base[bodyStart], fdict.base[bodyStart+1], fdict.base[bodyStart+2],
               fdict.base[bodyStart+3], fdict.base[bodyStart+4], fdict.base[bodyStart+5],
               fdict.base[bodyStart+6], fdict.base[bodyStart+7], fdict.base[bodyStart+8],
               fdict.base[bodyStart+9]);
        subFail = 1;
      }
    }
    if (!subFail) {
      x_set_string(": PN2 RCL 'ABC' ;");
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: compile PN2 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint8_t expect[6];
      expect[0] = 253; expect[1] = 3; expect[2] = 'A'; expect[3] = 'B';
      expect[4] = 'C'; expect[5] = 0;   /* odd len ⇒ one pad byte 0 */
      if (memcmp(fdict.base + bodyStart + 4, expect, 6) != 0) {
        printf("    [2] FAIL: PN2 param bytes %02X %02X %02X %02X %02X %02X\n",
               fdict.base[bodyStart+4], fdict.base[bodyStart+5], fdict.base[bodyStart+6],
               fdict.base[bodyStart+7], fdict.base[bodyStart+8], fdict.base[bodyStart+9]);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [2] PASS: 253 cells carry len, name, and pad exactly\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 3: the compiled 253 form dispatches through the bounded core. */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    x_set_string(": PN1 STO 'VZ' ;");
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile PN1 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      forthPushInt32(77);
      run_word("PN1");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: run PN1 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      forthPushInt32(0);
      forthOuterInterpret("RCL 'VZ'");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(77)) {
        printf("    [3] FAIL: RCL 'VZ' after compiled store (err %d)\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [3] PASS: compiled 253 form dispatches through the bounded core\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 4: →NN indirection, interpreted AND compiled (the compiled drive
   * is what mutation 1 — dropping the 254 marker byte — must redden). */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    sprintf(sbuf, "STO %s05", STD_RIGHT_ARROW);
    forthPushInt32(7);
    forthOuterInterpret("STO 05");            /* register 05 := 7 */
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: seed STO 05 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      forthPushInt32(99);
      forthOuterInterpret(sbuf);              /* → stores 99 into register 07 */
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: STO ->05 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      forthPushInt32(0);
      forthOuterInterpret("RCL 07");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(99)) {
        printf("    [4] FAIL: RCL 07 after indirect store (err %d)\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      sprintf(sbuf, ": PN3 STO %s05 ;", STD_RIGHT_ARROW);
      x_set_string(sbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: compile PN3 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      if (fdict.base[bodyStart + 4] != 254 || fdict.base[bodyStart + 5] != 5) {
        printf("    [4] FAIL: PN3 param cell {%u, %u} expected {254, 5}\n",
               fdict.base[bodyStart + 4], fdict.base[bodyStart + 5]);
        subFail = 1;
      }
    }
    /* Compiled drive: reseed 05 := 7, X := 99, run PN3, register 07 must be 99. */
    if (!subFail) {
      forthPushInt32(7);
      forthOuterInterpret("STO 05");
      forthPushInt32(0);
      forthOuterInterpret("STO 07");          /* clear the target */
      forthPushInt32(99);
      run_word("PN3");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: run PN3 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      forthPushInt32(0);
      forthOuterInterpret("RCL 07");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(99)) {
        printf("    [4] FAIL: compiled ->05 did not reach register 07 (err %d)\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [4] PASS: ->05 resolves through the native indirection helper\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 5: →'NAME' indirection through a named variable. */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    forthPushInt32(3);
    forthOuterInterpret("STO 'VP'");          /* VP := 3, a register NUMBER */
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FAIL: STO 'VP' error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      sprintf(sbuf, "STO %s'VP'", STD_RIGHT_ARROW);
      forthPushInt32(55);
      forthOuterInterpret(sbuf);              /* → stores 55 into register 03 */
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: STO ->'VP' error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      forthPushInt32(0);
      forthOuterInterpret("RCL 03");
      if (lastErrorCode != ERROR_NONE || !x_is_longint(55)) {
        printf("    [5] FAIL: RCL 03 after ->'VP' store (err %d)\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      sprintf(sbuf, ": PN4 STO %s'VP' ;", STD_RIGHT_ARROW);
      x_set_string(sbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: compile PN4 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint8_t expect[4] = { 255, 2, 'V', 'P' };
      if (memcmp(fdict.base + bodyStart + 4, expect, 4) != 0) {
        printf("    [5] FAIL: PN4 param bytes %02X %02X %02X %02X\n",
               fdict.base[bodyStart+4], fdict.base[bodyStart+5],
               fdict.base[bodyStart+6], fdict.base[bodyStart+7]);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [5] PASS: ->'VP' resolves through the native indirect-variable helper\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 6: system-flag reverse map — both index ranges. */
  { int subFail = 0;
    const char *name0 = indexOfItems[SFL_TDM24].itemSoftmenuName;
    const char *name64 = indexOfItems[SFL_MONIT].itemSoftmenuName;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    sprintf(sbuf, ": PF3 SF '%s' ;", name0);
    x_set_string(sbuf);
    fnForthOuter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [6] FAIL: compile SF '%s' error %d\n", name0, lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      if (fdict.base[bodyStart + 4] != 250 || fdict.base[bodyStart + 5] != 0) {
        printf("    [6] FAIL: '%s' cell {%u, %u} expected {250, 0}\n", name0,
               fdict.base[bodyStart + 4], fdict.base[bodyStart + 5]);
        subFail = 1;
      }
    }
    /* Range-two probe (index 64 = the SFL_MONIT range). */
    if (!subFail) {
      sprintf(sbuf, ": PF4 SF '%s' ;", name64);
      x_set_string(sbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [6] FAIL: compile SF '%s' error %d\n", name64, lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      if (fdict.base[bodyStart + 4] != 250 || fdict.base[bodyStart + 5] != 64) {
        printf("    [6] FAIL: '%s' cell {%u, %u} expected {250, 64}\n", name64,
               fdict.base[bodyStart + 4], fdict.base[bodyStart + 5]);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SF 'ZZQQ'");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: SF 'ZZQQ' expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) {
      printf("    [6] PASS: system-flag names map to [250][index] cells\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 7: menu names encode unresolved and miss with UNDEF_MENU. */
  { int subFail = 0;
    const char *mname = indexOfItems[menuItem].itemCatalogName;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    sprintf(sbuf, "%s 'ZZQQ'", mname);
    forthOuterInterpret(sbuf);
    if (lastErrorCode != ERROR_UNDEF_MENU) {
      printf("    [7] FAIL: %s 'ZZQQ' expected ERROR_UNDEF_MENU, got %d\n", mname, lastErrorCode);
      subFail = 1;
    }
    lastErrorCode = ERROR_NONE;
    if (!subFail) {
      sprintf(sbuf, ": PM1 %s 'ZZQQ' ;", mname);
      x_set_string(sbuf);
      fnForthOuter(NOPARAM);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [7] FAIL: compile PM1 error %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      uint16_t bodyStart = fdict.latest + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
      uint8_t expect[6] = { 253, 4, 'Z', 'Z', 'Q', 'Q' };
      if (memcmp(fdict.base + bodyStart + 4, expect, 6) != 0) {
        printf("    [7] FAIL: PM1 param bytes %02X %02X %02X %02X %02X %02X\n",
               fdict.base[bodyStart+4], fdict.base[bodyStart+5], fdict.base[bodyStart+6],
               fdict.base[bodyStart+7], fdict.base[bodyStart+8], fdict.base[bodyStart+9]);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [7] PASS: menu names encode unresolved and miss with UNDEF_MENU\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
  }

  /* Subcase 8: NUMBER_16 indirection excluded (compile-side — the validator
   * cannot police it: for N16 the cell IS a legal little-endian value), plus
   * the marker-cell malformations the walks DO reject. */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    forthGDictClear();
    sprintf(sbuf, "%s %s05", indexOfItems[n16Item].itemCatalogName, STD_RIGHT_ARROW);
    forthOuterInterpret(sbuf);
    if (lastErrorCode != ERROR_INVALID_NAME) {
      printf("    [8] FAIL: N16 indirection expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
      subFail = 1;
    }
    lastErrorCode = ERROR_NONE;

    /* {253, 0}: len below the lower bound → RESET */
    if (!subFail) {
      uint16_t w = gbegin_word("VN1", 3);
      if (w == FORTH_NULL) { printf("    [8] FAIL: alloc VN1\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        gemit_bytes((uint8_t[]){253, 0}, 2);
        gend_word();
      }
      if (!subFail) {
        uint8_t *preBase = gdict.base;
        uint16_t preBlocks = gdict.sizeBlocks;
        forthGDictValidateRestored();
        if (gdict.base != NULL) {
          printf("    [8] FAIL: REGISTER {253,0} survived validation\n");
          subFail = 1;
          forthGDictClear();
        } else if (preBase) {
          freeC47Blocks(preBase, preBlocks);
        }
      }
    }
    /* {253, 3, 'A','B','C', 7}: non-zero pad byte → RESET */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VN2", 3);
      if (w == FORTH_NULL) { printf("    [8] FAIL: alloc VN2\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        gemit_bytes((uint8_t[]){253, 3}, 2);
        gemit_bytes((uint8_t[]){'A', 'B'}, 2);
        gemit_bytes((uint8_t[]){'C', 7}, 2);
        gend_word();
      }
      if (!subFail) {
        uint8_t *preBase = gdict.base;
        uint16_t preBlocks = gdict.sizeBlocks;
        forthGDictValidateRestored();
        if (gdict.base != NULL) {
          printf("    [8] FAIL: REGISTER {253,3,'A','B','C',7} survived validation\n");
          subFail = 1;
          forthGDictClear();
        } else if (preBase) {
          freeC47Blocks(preBase, preBlocks);
        }
      }
    }
    /* {253, 31} with no name bytes: the group runs past the body → RESET */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VN3", 3);
      if (w == FORTH_NULL) { printf("    [8] FAIL: alloc VN3\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        gemit_bytes((uint8_t[]){253, 31}, 2);
        gend_word();
      }
      if (!subFail) {
        uint8_t *preBase = gdict.base;
        uint16_t preBlocks = gdict.sizeBlocks;
        forthGDictValidateRestored();
        if (gdict.base != NULL) {
          printf("    [8] FAIL: REGISTER {253,31} overrun survived validation\n");
          subFail = 1;
          forthGDictClear();
        } else if (preBase) {
          freeC47Blocks(preBase, preBlocks);
        }
      }
    }
    /* {253, 2, 'V','Z'}: well-formed → ACCEPT (keeps the RESETs non-vacuous) */
    if (!subFail) {
      forthGDictClear();
      uint16_t w = gbegin_word("VN4", 3);
      if (w == FORTH_NULL) { printf("    [8] FAIL: alloc VN4\n"); subFail = 1; }
      if (!subFail) {
        gemit(T_C47);
        { uint16_t itemId = ITM_STO; gemit_bytes((uint8_t[]){itemId, itemId >> 8}, 2); }
        gemit_bytes((uint8_t[]){253, 2}, 2);
        gemit_bytes((uint8_t[]){'V', 'Z'}, 2);
        gend_word();
      }
      if (!subFail) {
        uint8_t *preBase = gdict.base;
        uint16_t preBlocks = gdict.sizeBlocks;
        forthGDictValidateRestored();
        if (gdict.base == NULL) {
          printf("    [8] FAIL: REGISTER {253,2,'V','Z'} reset (should accept)\n");
          subFail = 1;
          if (preBase) freeC47Blocks(preBase, preBlocks);
        } else {
          forthGDictClear();
        }
      }
    }
    if (!subFail) {
      printf("    [8] PASS: N16 indirection excluded; marker-cell malformations reject\n");
    } else {
      fail |= 1;
    }
    forthDictClear();
    forthGDictClear();
  }

  /* Cleanup: upstream has no delete-named-variable API, so unwind exactly
   * what allocateNamedVariable created for VZ/VP — each variable's data
   * block plus the header table — back to the pre-test count. Without this
   * the suite's end-of-run region-count gate reddens (and papering over
   * that gate by assigning numberOfAllocatedMemoryRegions is never the
   * answer: the leak is real). */
  if (numberOfNamedVariables > savedNamedVars) {
    uint16_t i;
    for (i = savedNamedVars; i < numberOfNamedVariables; i++) {
      freeRegisterData(FIRST_NAMED_VARIABLE + i);
    }
    if (savedNamedVars == 0) {
      freeC47Blocks(allNamedVariables,
                    TO_BLOCKS(sizeof(namedVariableHeader_t) * numberOfNamedVariables));
      allNamedVariables = NULL;
    } else {
      allNamedVariables = reallocC47Blocks(allNamedVariables,
                            TO_BLOCKS(sizeof(namedVariableHeader_t) * numberOfNamedVariables),
                            TO_BLOCKS(sizeof(namedVariableHeader_t) * savedNamedVars));
    }
    numberOfNamedVariables = savedNamedVars;
  }
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* test_param_series_c_acceptance
 * F4-4: Series C error table and native/Forth parity acceptance sweep.
 * Closes stage F4. */
static int test_param_series_c_acceptance(void)
{
  int fail = 0;
  char sbuf[96];
  uint16_t savedNamedVars = numberOfNamedVariables;
  uint16_t namedVarsAfterCreate = savedNamedVars; /* updated after 'VS' creation in subcase 3 */

  /* seedSweepState — push T=11, Z=22, Y=33, X=44 (each push lifts). */
  /* The seed-assert checks all four registers plus CONFIG state. */
  /* Helper inline: called before each half of a parity pair. */

  /* Subcase 1: REGISTER parity, numbered + letter */
  { int subFail = 0;
    uint8_t rclType; int32_t rclVal;

    /* Pair A: STO 05 / RCL 05 */
    { /* Native half */
      lastErrorCode = ERROR_NONE;
      testProg_t tp; tpInit(&tp);
      int sSto = tpStepParam(&tp, ITM_STO, (uint8_t[]){5}, 1);
      if (sSto < 0 || !tpWrite(&tp)) {
        printf("    [1] FAIL: fixture STO 05\n"); subFail = 1;
      }
      if (!subFail) {
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
         { uint8_t tType; int32_t tVal;
           read_reg_int32(REGISTER_X, &tType, &tVal);
           if (tType != dtLongInteger || tVal != 44 || dynamicMenuItem != -1) {
            printf("    [1] FAIL: seed-assert X=%d type=%d dyn=%d\n", tVal, tType, dynamicMenuItem);
            subFail = 1;
          }
        }
        if (!subFail) {
          uint8_t *step = tpStepAddr(&tp, sSto);
          if (step) { tpSelectStep(&tp, sSto); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) {
            printf("    [1] FAIL: native STO 05 error %d\n", lastErrorCode); subFail = 1;
          }
        }
      }
      /* Native RCL 05 */
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        testProg_t tp2; tpInit(&tp2);
        int sRcl = tpStepParam(&tp2, ITM_RCL, (uint8_t[]){5}, 1);
        if (sRcl < 0 || !tpWrite(&tp2)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(0);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp2, sRcl);
          if (step) { tpSelectStep(&tp2, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) { subFail = 1; }
        }
        if (!subFail) {
          read_reg_int32(REGISTER_X, &rclType, &rclVal);
        }
      }
      int32_t nativeRecall = rclVal;

      /* Forth half */
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        forthOuterInterpret("STO 05");
        if (lastErrorCode != ERROR_NONE) {
          printf("    [1] FAIL: Forth STO 05 error %d\n", lastErrorCode); subFail = 1;
        }
      }
      /* Forth RCL 05 (native step to read back) */
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        testProg_t tp3; tpInit(&tp3);
        int sRcl = tpStepParam(&tp3, ITM_RCL, (uint8_t[]){5}, 1);
        if (sRcl < 0 || !tpWrite(&tp3)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(0);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp3, sRcl);
          if (step) { tpSelectStep(&tp3, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) { subFail = 1; }
        }
        if (!subFail) {
          read_reg_int32(REGISTER_X, &rclType, &rclVal);
        }
      }
      int32_t forthRecall = rclVal;

      if (!subFail) {
        if (nativeRecall != forthRecall) {
          printf("    [1] FAIL: register parity 05 (native=%d forth=%d)\n", nativeRecall, forthRecall);
          subFail = 1;
        }
      }
    }

    /* Pair B: STO A / RCL A */
    if (!subFail) {
      uint8_t rclTypeB; int32_t rclValB;

      /* Native half */
      { lastErrorCode = ERROR_NONE;
        testProg_t tp; tpInit(&tp);
        int sSto = tpStepParam(&tp, ITM_STO, (uint8_t[]){104}, 1);
        if (sSto < 0 || !tpWrite(&tp)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp, sSto);
          if (step) { tpSelectStep(&tp, sSto); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) {
            printf("    [1] FAIL: native STO A error %d\n", lastErrorCode); subFail = 1;
          }
        }
      }
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        testProg_t tp2; tpInit(&tp2);
        int sRcl = tpStepParam(&tp2, ITM_RCL, (uint8_t[]){104}, 1);
        if (sRcl < 0 || !tpWrite(&tp2)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(0);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp2, sRcl);
          if (step) { tpSelectStep(&tp2, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) { subFail = 1; }
        }
        if (!subFail) {
          read_reg_int32(REGISTER_X, &rclTypeB, &rclValB);
        }
      }
      int32_t nativeRecallB = rclValB;

      /* Forth half */
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        forthOuterInterpret("STO A");
        if (lastErrorCode != ERROR_NONE) {
          printf("    [1] FAIL: Forth STO A error %d\n", lastErrorCode); subFail = 1;
        }
      }
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        testProg_t tp3; tpInit(&tp3);
        int sRcl = tpStepParam(&tp3, ITM_RCL, (uint8_t[]){104}, 1);
        if (sRcl < 0 || !tpWrite(&tp3)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(0);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp3, sRcl);
          if (step) { tpSelectStep(&tp3, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
          if (lastErrorCode != ERROR_NONE) { subFail = 1; }
        }
        if (!subFail) {
          read_reg_int32(REGISTER_X, &rclTypeB, &rclValB);
        }
      }
      int32_t forthRecallB = rclValB;

      if (!subFail) {
        if (nativeRecallB != forthRecallB) {
          printf("    [1] FAIL: register parity A (native=%d forth=%d)\n", nativeRecallB, forthRecallB);
          subFail = 1;
        }
      }
    }

    if (!subFail) {
      printf("    [1] PASS: register parameter parity (05, A)\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 2: NUMBER_8 boundary parity + over-range divergence */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    uint16_t sdlMax = (uint16_t)(indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK);

    /* Independent oracle: sdlMax must be accepted FIRST */
    if (!paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, sdlMax)) {
      printf("    [2] CONFIG FAIL: sdlMax %u rejected by paramCoreValidateDirect\n", sdlMax);
      subFail = 1;
    }

    int32_t nativeX = 0, forthX = 0;
    int nativeErr = ERROR_NONE, forthErr = ERROR_NONE;

    if (!subFail) {
      /* Native half */
      { lastErrorCode = ERROR_NONE;
        testProg_t tp; tpInit(&tp);
        uint8_t pval = (uint8_t)sdlMax;
        int sSdl = tpStepParam(&tp, ITM_SDL, &pval, 1);
        if (sSdl < 0 || !tpWrite(&tp)) {
          printf("    [2] FAIL: fixture SDL\n"); subFail = 1;
        }
        if (!subFail) {
          forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp, sSdl);
          if (step) {
            tpSelectStep(&tp, sSdl);
            programRunStop = PGM_RUNNING;
            executeOneStep(step);
          }
          nativeErr = lastErrorCode;
          if (lastErrorCode == ERROR_NONE) {
            uint8_t tType; int32_t tVal;
            read_reg_int32(REGISTER_X, &tType, &tVal);
            nativeX = tVal;
          }
        }
      }
    }

    if (!subFail) {
      /* Forth half */
      lastErrorCode = ERROR_NONE;
      sprintf(sbuf, "SDL %u", sdlMax);
      forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
      lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
      forthOuterInterpret(sbuf);
      forthErr = lastErrorCode;
      if (lastErrorCode == ERROR_NONE) {
        uint8_t tType; int32_t tVal;
        read_reg_int32(REGISTER_X, &tType, &tVal);
        forthX = tVal;
      }
    }

    if (!subFail) {
      if (nativeErr != forthErr || nativeX != forthX) {
        printf("    [2] FAIL: boundary parity (native err=%d X=%d, forth err=%d X=%d)\n",
               nativeErr, nativeX, forthErr, forthX);
        subFail = 1;
      }
    }

    if (!subFail) {
      /* Over-max Forth: must raise ERROR_OUT_OF_RANGE */
      lastErrorCode = ERROR_NONE;
      sprintf(sbuf, "SDL %u", sdlMax + 1);
      forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
      lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
      forthOuterInterpret(sbuf);
      int forthOverErr = lastErrorCode;

      /* Over-max native (corrupt-step): cannot be built by TAM, silence expected */
      lastErrorCode = ERROR_NONE;
      testProg_t tp; tpInit(&tp);
      uint8_t pval = (uint8_t)(sdlMax + 1);
      int sSdl = tpStepParam(&tp, ITM_SDL, &pval, 1);
      if (sSdl < 0 || !tpWrite(&tp)) { subFail = 1; }
      if (!subFail) {
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tp, sSdl);
        if (step) {
          int32_t preX = 44;
          tpSelectStep(&tp, sSdl);
          programRunStop = PGM_RUNNING;
          executeOneStep(step);
        }
        int nativeOverErr = lastErrorCode;
        int32_t nativeOverX = 0;
        { uint8_t tType; int32_t tVal;
          read_reg_int32(REGISTER_X, &tType, &tVal);
          nativeOverX = tVal;
        }
        /* DIVERGENCE: Forth parse reject vs native silence */
        if (forthOverErr != ERROR_OUT_OF_RANGE) {
          printf("    [2] FAIL: Forth over-range expected ERROR_OUT_OF_RANGE got %d\n", forthOverErr);
          subFail = 1;
        }
        /* Native silence: no error, X unchanged */
        if (!subFail && nativeOverErr != ERROR_NONE) {
          printf("    [2] FAIL: native over-range expected silence got error %d\n", nativeOverErr);
          subFail = 1;
        }
        if (!subFail && nativeOverX != 44) {
          printf("    [2] FAIL: native over-range X changed to %d (expected 44)\n", nativeOverX);
          subFail = 1;
        }
      }
    }

    if (!subFail) {
      printf("    [2] PASS: NUMBER_8 boundary agrees; over-range diverges as designed (parse reject vs native silence)\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 3: Named variable parity */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;

    /* Native half: STO 'VS' with X=44 */
    { lastErrorCode = ERROR_NONE;
      forthDictClear();
      testProg_t tp; tpInit(&tp);
      uint8_t pval[4] = {253, 2, 'V', 'S'};
      int sSto = tpStepParam(&tp, ITM_STO, pval, 4);
      if (sSto < 0 || !tpWrite(&tp)) {
        printf("    [3] FAIL: fixture STO 'VS'\n"); subFail = 1;
      }
      if (!subFail) {
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tp, sSto);
        if (step) {
          tpSelectStep(&tp, sSto);
          programRunStop = PGM_RUNNING;
          executeOneStep(step);
        }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: native STO 'VS' error %d\n", lastErrorCode);
          subFail = 1;
        }
        namedVarsAfterCreate = numberOfNamedVariables; /* snapshot after 'VS' created */
      }
    }

    /* RCL 'VS' -> 44 */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      testProg_t tpRcl; tpInit(&tpRcl);
      uint8_t pRcl[4] = {253, 2, 'V', 'S'};
      int sRcl = tpStepParam(&tpRcl, ITM_RCL, pRcl, 4);
      if (sRcl < 0 || !tpWrite(&tpRcl)) { subFail = 1; }
      if (!subFail) {
        forthPushInt32(0);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tpRcl, sRcl);
        if (step) { tpSelectStep(&tpRcl, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: RCL 'VS' after native (err=%d)\n", lastErrorCode);
          subFail = 1;
        } else {
          uint8_t tType; int32_t tVal;
          read_reg_int32(REGISTER_X, &tType, &tVal);
          if (tVal != 44) {
            printf("    [3] FAIL: RCL 'VS' after native X=%d (expected 44)\n", tVal);
            subFail = 1;
          }
        }
      }
    }

    /* Forth half: STO 'VS' with X=99 */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(99);
      lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
      forthOuterInterpret("STO 'VS'");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [3] FAIL: Forth STO 'VS' error %d\n", lastErrorCode);
        subFail = 1;
      }
    }

    /* RCL 'VS' -> 99 */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      testProg_t tpRcl2; tpInit(&tpRcl2);
      uint8_t pRcl2[4] = {253, 2, 'V', 'S'};
      int sRcl2 = tpStepParam(&tpRcl2, ITM_RCL, pRcl2, 4);
      if (sRcl2 < 0 || !tpWrite(&tpRcl2)) { subFail = 1; }
      if (!subFail) {
        forthPushInt32(0);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tpRcl2, sRcl2);
        if (step) { tpSelectStep(&tpRcl2, sRcl2); programRunStop = PGM_RUNNING; executeOneStep(step); }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: RCL 'VS' after Forth (err=%d)\n", lastErrorCode);
          subFail = 1;
        } else {
          uint8_t tType; int32_t tVal;
          read_reg_int32(REGISTER_X, &tType, &tVal);
          if (tVal != 99) {
            printf("    [3] FAIL: RCL 'VS' after Forth X=%d (expected 99)\n", tVal);
            subFail = 1;
          }
        }
      }
    }

    if (!subFail) {
      printf("    [3] PASS: named variable parameter reaches one variable from both engines\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 4: Indirect parity */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;

    /* Native half: STO ->05 with X=99, reg 05 := 7 */
    { lastErrorCode = ERROR_NONE;
      forthPushInt32(7);
      forthOuterInterpret("STO 05");
      if (lastErrorCode != ERROR_NONE) { subFail = 1; }
      if (!subFail) {
        testProg_t tp; tpInit(&tp);
        uint8_t pval[2] = {254, 5};
        int sSto = tpStepParam(&tp, ITM_STO, pval, 2);
        if (sSto < 0 || !tpWrite(&tp)) { subFail = 1; }
        if (!subFail) {
          forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(99);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp, sSto);
          if (step) {
            tpSelectStep(&tp, sSto);
            programRunStop = PGM_RUNNING;
            executeOneStep(step);
          }
          if (lastErrorCode != ERROR_NONE) {
            printf("    [4] FAIL: native STO ->05 error %d\n", lastErrorCode);
            subFail = 1;
          }
        }
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      testProg_t tpRcl; tpInit(&tpRcl);
      int sRcl = tpStepParam(&tpRcl, ITM_RCL, (uint8_t[]){7}, 1);
      if (sRcl < 0 || !tpWrite(&tpRcl)) { subFail = 1; }
      if (!subFail) {
        forthPushInt32(0);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tpRcl, sRcl);
        if (step) { tpSelectStep(&tpRcl, sRcl); programRunStop = PGM_RUNNING; executeOneStep(step); }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [4] FAIL: RCL 07 after native (err=%d)\n", lastErrorCode);
          subFail = 1;
        } else {
          uint8_t tType; int32_t tVal;
          read_reg_int32(REGISTER_X, &tType, &tVal);
          if (tVal != 99) {
            printf("    [4] FAIL: RCL 07 after native X=%d (expected 99)\n", tVal);
            subFail = 1;
          }
        }
      }
    }

    /* Forth half: STO ->05 with X=99, reg 05 := 7 */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthPushInt32(7);
      forthOuterInterpret("STO 05");
      if (lastErrorCode != ERROR_NONE) { subFail = 1; }
      if (!subFail) {
        sprintf(sbuf, "STO %s05", STD_RIGHT_ARROW);
        forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(99);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        forthOuterInterpret(sbuf);
        if (lastErrorCode != ERROR_NONE) {
          printf("    [4] FAIL: Forth STO ->05 error %d\n", lastErrorCode);
          subFail = 1;
        }
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      testProg_t tpRcl2; tpInit(&tpRcl2);
      int sRcl2 = tpStepParam(&tpRcl2, ITM_RCL, (uint8_t[]){7}, 1);
      if (sRcl2 < 0 || !tpWrite(&tpRcl2)) { subFail = 1; }
      if (!subFail) {
        forthPushInt32(0);
        lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
        uint8_t *step = tpStepAddr(&tpRcl2, sRcl2);
        if (step) { tpSelectStep(&tpRcl2, sRcl2); programRunStop = PGM_RUNNING; executeOneStep(step); }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [4] FAIL: RCL 07 after Forth (err=%d)\n", lastErrorCode);
          subFail = 1;
        } else {
          uint8_t tType; int32_t tVal;
          read_reg_int32(REGISTER_X, &tType, &tVal);
          if (tVal != 99) {
            printf("    [4] FAIL: RCL 07 after Forth X=%d (expected 99)\n", tVal);
            subFail = 1;
          }
        }
      }
    }

    if (!subFail) {
      printf("    [4] PASS: indirect register parameter parity\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 5: Flow divergence table */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;

    struct { uint16_t itemId; uint8_t param[4]; uint16_t nParam; const char *name; } flows[] = {
      { ITM_RTN,  {0}, 1, "RTN" },
      { ITM_STOP, {0}, 1, "STOP" },
      { ITM_END,  {0}, 1, "END" },
      { ITM_CASE, {5}, 1, "CASE" },
      { ITM_GTO,  {253, 1, 'Q', 0}, 4, "GTO 'Q'" },
    };
    int nFlows = 5;
    int i;

    for (i = 0; i < nFlows && !subFail; i++) {
      /* Native: must NOT raise ERROR_OPERATION_UNDEFINED */
      { lastErrorCode = ERROR_NONE;
        testProg_t tp; tpInit(&tp);
        int sStep = tpStepParam(&tp, flows[i].itemId, flows[i].param, flows[i].nParam);
        if (sStep < 0 || !tpWrite(&tp)) {
          printf("    [5] FAIL: fixture %s\n", flows[i].name); subFail = 1;
        }
        if (!subFail) {
          forthPushInt32(11); forthPushInt32(22); forthPushInt32(33); forthPushInt32(44);
          lastErrorCode = ERROR_NONE; programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
          uint8_t *step = tpStepAddr(&tp, sStep);
          if (step) {
            tpSelectStep(&tp, sStep);
            programRunStop = PGM_RUNNING;
            executeOneStep(step);
          }
          if (lastErrorCode == ERROR_OPERATION_UNDEFINED) {
            printf("    [5] FAIL: native %s raised OPERATION_UNDEFINED\n", flows[i].name);
            subFail = 1;
          }
        }
      }

      /* Forth: MUST raise ERROR_OPERATION_UNDEFINED */
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        forthOuterInterpret(flows[i].name);
        if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
          printf("    [5] FAIL: Forth %s expected OPERATION_UNDEFINED got %d\n",
                 flows[i].name, lastErrorCode);
          subFail = 1;
        }
        lastErrorCode = ERROR_NONE;
      }
    }

    if (!subFail) {
      printf("    [5] PASS: flow steps stay native-only; names reject with OPERATION UNDEFINED\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 6: Error-table sweep */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;

    /* "RTN" -> OPERATION_UNDEFINED */
    { lastErrorCode = ERROR_NONE;
      forthOuterInterpret("RTN");
      if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
        printf("    [6] FAIL: RTN expected OPERATION_UNDEFINED got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "SDL" -> INVALID_NAME */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SDL");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: SDL expected INVALID_NAME got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "SDL 1X" -> INVALID_NAME */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SDL 1X");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: SDL 1X expected INVALID_NAME got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "SDL 100" -> OUT_OF_RANGE */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SDL 100");
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [6] FAIL: SDL 100 expected OUT_OF_RANGE got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "SF .32" -> OUT_OF_RANGE */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SF .32");
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [6] FAIL: SF .32 expected OUT_OF_RANGE got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "SDL 'X'" -> INVALID_NAME */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("SDL 'X'");
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [6] FAIL: SDL 'X' expected INVALID_NAME got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* N16-item + arrow -> INVALID_NAME */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      uint16_t n16Item = 0;
      { uint16_t id;
        for (id = 1; id < LAST_ITEM; id++) {
          uint16_t st = indexOfItems[id].status;
          if ((st & CAT_STATUS) != CAT_FNCT) continue;
          if ((st & PTP_STATUS) == PTP_NUMBER_16 && !forthItemIsFlowReject(id)) {
            n16Item = id;
            break;
          }
        }
      }
      if (n16Item) {
        sprintf(sbuf, "%s %s05", indexOfItems[n16Item].itemCatalogName, STD_RIGHT_ARROW);
        forthOuterInterpret(sbuf);
        if (lastErrorCode != ERROR_INVALID_NAME) {
          printf("    [6] FAIL: N16-> expected INVALID_NAME got %d\n", lastErrorCode);
          subFail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "RCL 'NOVAR9'" -> UNDEF_SOURCE_VAR */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret("RCL 'NOVAR9'");
      if (lastErrorCode != ERROR_UNDEF_SOURCE_VAR) {
        printf("    [6] FAIL: RCL 'NOVAR9' expected UNDEF_SOURCE_VAR got %d\n", lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }

    /* "OPENM 'NOMENU9'" -> UNDEF_MENU */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      uint16_t menuItem = 0;
      { uint16_t id;
        for (id = 1; id < LAST_ITEM; id++) {
          uint16_t st = indexOfItems[id].status;
          if ((st & CAT_STATUS) != CAT_FNCT) continue;
          if ((st & PTP_STATUS) == PTP_MENU) { menuItem = id; break; }
        }
      }
      if (menuItem) {
        sprintf(sbuf, "%s 'NOMENU9'", indexOfItems[menuItem].itemCatalogName);
        forthOuterInterpret(sbuf);
        if (lastErrorCode != ERROR_UNDEF_MENU) {
          printf("    [6] FAIL: OPENM 'NOMENU9' expected UNDEF_MENU got %d\n", lastErrorCode);
          subFail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    /* Compile-state atomicity: ": EA SDL 100 ;" -> OUT_OF_RANGE, EA unfindable */
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      forthDictClear();
      forthOuterInterpret(": EA SDL 100 ;");
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [6] FAIL: : EA SDL 100 ; expected OUT_OF_RANGE got %d\n", lastErrorCode);
        subFail = 1;
      }
      if (!subFail) {
        uint16_t idx;
        if (forthFindColon("EA", &idx)) {
          printf("    [6] FAIL: EA still findable after compile error\n");
          subFail = 1;
        }
      }
      lastErrorCode = ERROR_NONE;
    }

    if (!subFail) {
      printf("    [6] PASS: the Series C error table holds row by row\n");
    } else {
      fail |= 1;
    }
  }

  /* Subcase 7: Extra-token and stage-wide state hygiene */
  { int subFail = 0;
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("STO 05 7");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [7] FAIL: STO 05 7 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      uint8_t tType7; int32_t tVal7;
      read_reg_int32(REGISTER_X, &tType7, &tVal7);
      if (tVal7 != 7) {
        printf("    [7] FAIL: X != 7 after STO 05 7 (got %d)\n", tVal7);
        subFail = 1;
      }
    }
    if (!subFail) {
      if (forthTestGetRsp() != 0) {
        printf("    [7] FAIL: rsp=%u (expected 0)\n", forthTestGetRsp());
        subFail = 1;
      }
    }
    if (!subFail) {
      if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
        printf("    [7] FAIL: scope=0x%04X (expected 0x%04X)\n",
               forthCurrentScopeGet(), FORTH_OWNER_INTERACTIVE);
        subFail = 1;
      }
    }
    if (!subFail) {
      printf("    [7] PASS: one-token consumption and engine state hygiene hold\n");
    } else {
      fail |= 1;
    }
  }

  /* Cleanup: unwind named variables from subcase 3 */
  if (namedVarsAfterCreate > savedNamedVars) {
    uint16_t i;
    for (i = savedNamedVars; i < namedVarsAfterCreate; i++) {
      freeRegisterData(FIRST_NAMED_VARIABLE + i);
    }
    if (savedNamedVars == 0) {
      freeC47Blocks(allNamedVariables,
                    TO_BLOCKS(sizeof(namedVariableHeader_t) * namedVarsAfterCreate));
      allNamedVariables = NULL;
    } else {
      allNamedVariables = reallocC47Blocks(allNamedVariables,
                        TO_BLOCKS(sizeof(namedVariableHeader_t) * namedVarsAfterCreate),
                        TO_BLOCKS(sizeof(namedVariableHeader_t) * savedNamedVars));
    }
    numberOfNamedVariables = savedNamedVars;
  }
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ==========================================================================
 * F5-1: Check mode — the tokenizer validates its own grammar
 * ========================================================================== */

/* test_commit_gate
 * F5-2: E9 commit gate — structural rejects at ENTER, advisory commits.
 * Five subcases in one capture session. */
static int test_commit_gate(void)
{
  extern void pemAlpha(int16_t);

  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedMenu = currentMenu();
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);

  /* Build fixture: LBL, marker, placeholder source, RTN */
  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F5T");
  tpMarker(&p);
  tpSrc(&p, " ");  /* placeholder — replaced on commit */
  tpRtn(&p);

  if (!tpWrite(&p)) {
    printf("    FIXTURE FAIL: tpWrite\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  /* Position on the opening marker (step 2) */
  fnGotoDot(2);

  if (currentStep != tpStepAddr(&p, 1)) {
    printf("    FIXTURE BUG: fnGotoDot(2) did not position on marker\n");
    fail = 1;
  }
  else if (currentLocalStepNumber != 2) {
    printf("    FIXTURE BUG: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
    fail = 1;
  }
  else {
    /* Open Forth capture with the ALPHA gesture */
    runFunction(ITM_AIM);

    if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
      printf("    FIXTURE BUG: ITM_AIM did not open Forth capture (alpha=%d tam.function=%d)\n",
             (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
      fail = 1;
    }
    else {
      uint16_t stepCountBefore = getNumberOfSteps();

      /* ---- Subcase 1: Malformed line refuses atomically ---- */
      { int sc1 = 0;
        lastErrorCode = ERROR_NONE;
        aimBuffer[0] = 0;

        const int16_t badItems[] = {
          ITM_COLON, ITM_SPACE, ITM_A, ITM_SPACE,
          ITM_I, ITM_F, ITM_SPACE,
          ITM_SEMICOLON, ITM_ENTER
        };
        int i;
        for (i = 0; i < (int)(sizeof(badItems) / sizeof(badItems[0])); i++) {
          runFunction(badItems[i]);
        }

        if (lastErrorCode != ERROR_INVALID_NAME) {
          printf("    [1] FAIL: lastErrorCode = %d, expected ERROR_INVALID_NAME (%d)\n",
                 lastErrorCode, ERROR_INVALID_NAME);
          sc1 = 1;
        }
        else if (!getSystemFlag(FLAG_ALPHA)) {
          printf("    [1] FAIL: FLAG_ALPHA not set — capture should stay open\n");
          sc1 = 1;
        }
        else if (strcmp(forthTestCapText(), ": A IF ;") != 0) {
          printf("    [1] FAIL: cap text = '%s', expected ': A IF ;'\n", forthTestCapText());
          sc1 = 1;
        }
        else if (getNumberOfSteps() != stepCountBefore) {
          printf("    [1] FAIL: step count changed %u -> %u\n",
                 stepCountBefore, getNumberOfSteps());
          sc1 = 1;
        }
        else {
          printf("    [1] PASS: tier-1 line refused; capture and buffer intact\n");
        }
        lastErrorCode = ERROR_NONE;
        fail |= sc1;
      }

      /* ---- Subcase 2: Correction commits ---- */
      { int sc2 = 0;
        lastErrorCode = ERROR_NONE;

        /* Clear buffer and retype via CLA */
        runFunction(ITM_CLA);

        const int16_t goodItems[] = {
          ITM_COLON, ITM_SPACE, ITM_A, ITM_SPACE,
          ITM_1, ITM_SPACE, ITM_I, ITM_F, ITM_SPACE,
          ITM_2, ITM_SPACE,
          ITM_T, ITM_H, ITM_E, ITM_N, ITM_SPACE,
          ITM_SEMICOLON, ITM_ENTER
        };
        int i;
        for (i = 0; i < (int)(sizeof(goodItems) / sizeof(goodItems[0])); i++) {
          runFunction(goodItems[i]);
        }

        if (lastErrorCode != ERROR_NONE) {
          printf("    [2] FAIL: lastErrorCode = %d\n", lastErrorCode);
          sc2 = 1;
        }
        else if (!getSystemFlag(FLAG_ALPHA)) {
          printf("    [2] FAIL: FLAG_ALPHA not set (multi-line lock should hold)\n");
          sc2 = 1;
        }
        else if (getNumberOfSteps() != stepCountBefore + 1) {
          printf("    [2] FAIL: step count %u, expected %u\n",
                 getNumberOfSteps(), stepCountBefore + 1);
          sc2 = 1;
        }
        else {
          printf("    [2] PASS: corrected line commits and the lock advances\n");
        }
        lastErrorCode = ERROR_NONE;
        fail |= sc2;
      }

      /* ---- Subcase 3: Advisory line commits ---- */
      { int sc3 = 0;
        lastErrorCode = ERROR_NONE;

        const int16_t advItems[] = {
          ITM_F, ITM_U, ITM_T, ITM_U, ITM_R, ITM_E,
          ITM_W, ITM_O, ITM_R, ITM_D,
          ITM_SPACE, ITM_9, ITM_SPACE, ITM_PLUS,
          ITM_ENTER
        };
        int i;
        for (i = 0; i < (int)(sizeof(advItems) / sizeof(advItems[0])); i++) {
          runFunction(advItems[i]);
        }

        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: lastErrorCode = %d\n", lastErrorCode);
          sc3 = 1;
        }
        else if (getNumberOfSteps() != stepCountBefore + 2) {
          printf("    [3] FAIL: step count %u, expected %u\n",
                 getNumberOfSteps(), stepCountBefore + 2);
          sc3 = 1;
        }
        else {
          printf("    [3] PASS: unresolved names commit untouched\n");
        }
        lastErrorCode = ERROR_NONE;
        fail |= sc3;
      }

      /* ---- Subcase 4: Empty ENTER keeps E3 ---- */
      { int sc4 = 0;
        lastErrorCode = ERROR_NONE;
        uint16_t stepsBeforeE3 = getNumberOfSteps();

        pemAlpha(ITM_ENTER);

        if (getSystemFlag(FLAG_ALPHA)) {
          printf("    [4] FAIL: FLAG_ALPHA still set after E3\n");
          sc4 = 1;
        }
        else if (tam.function == ITM_FORTH) {
          printf("    [4] FAIL: tam.function == ITM_FORTH after E3\n");
          sc4 = 1;
        }
        else if (getNumberOfSteps() != stepsBeforeE3 - 1) {
          printf("    [4] FAIL: step count %u, expected %u (E3 deletes placeholder)\n",
                 getNumberOfSteps(), stepsBeforeE3 - 1);
          sc4 = 1;
        }
        else {
          printf("    [4] PASS: empty ENTER unchanged (E3)\n");
        }
        lastErrorCode = ERROR_NONE;
        fail |= sc4;
      }

      /* ---- Subcase 5: Run confirms the committed program ---- */
      { int sc5 = 0;
        lastErrorCode = ERROR_NONE;

        /* After E3, program: LBL(1), marker(2), src1(3), src2(4), RTN(5).
         * tpStepAddr is stale — offsets changed after commits rewrote the
         * placeholder. Navigate live from the program start. */
        uint8_t *cur = beginOfProgramMemory;
        uint8_t *sDef = NULL, *sUse = NULL;
        { int stepN = 0;
          while (cur != NULL) {
            stepN++;
            if (stepN == 3) sDef = cur;
            else if (stepN == 4) { sUse = cur; break; }
            cur = findNextStep(cur);
          }
        }

        if (sDef == NULL || sUse == NULL) {
          printf("    [5] FAIL: could not locate committed steps\n");
          sc5 = 1;
        }
        else {
          programRunStop = PGM_RUNNING;

          /* Execute definition step: : A 1 IF 2 THEN ; */
          currentStep = sDef;
          int16_t adv = executeOneStep(currentStep);
          if (adv <= 0) {
            printf("    [5] FAIL: executeOneStep(def) returned %d\n", adv);
            sc5 = 1;
          }
          else if (lastErrorCode != ERROR_NONE) {
            printf("    [5] FAIL: executeOneStep(def) error %d\n", lastErrorCode);
            sc5 = 1;
          }

          if (sc5 == 0) {
            /* Execute use step: FUTUREWORD 9 + */
            currentStep = sUse;
            lastErrorCode = ERROR_NONE;
            adv = executeOneStep(currentStep);
            if (adv <= 0) {
              printf("    [5] FAIL: executeOneStep(use) returned %d\n", adv);
              sc5 = 1;
            }
            /* FUTUREWORD is undefined — error expected, but mechanism must not crash */
          }

          lastErrorCode = ERROR_NONE;

          /* State hygiene */
          if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
            printf("    [5] FAIL: scope = %d, expected FORTH_OWNER_INTERACTIVE (%d)\n",
                   forthCurrentScopeGet(), FORTH_OWNER_INTERACTIVE);
            sc5 = 1;
          }
          else if (forthTestGetRsp() != 0) {
            printf("    [5] FAIL: rsp = %u, expected 0\n", forthTestGetRsp());
            sc5 = 1;
          }
        }

        if (!sc5)
          printf("    [5] PASS: the committed lines execute; state hygiene holds\n");
        lastErrorCode = ERROR_NONE;
        fail |= sc5;
      }
    }
  }

  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  showSoftmenu(savedMenu);
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));
  lastErrorCode = ERROR_NONE;

  return fail;
}

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

static int test_check_source_line(void)
{
  int fail = 0;

  /* Subcase 1: Tier-1 rejects, exact codes */
  { int subFail = 0;
    struct { const char *line; uint8_t code; } cases[] = {
      { ": A : B ;", ERROR_INVALID_NAME },
      { ": ;", ERROR_INVALID_NAME },
      { ";", ERROR_INVALID_NAME },
      { ": A", ERROR_INVALID_NAME },
      { ": TOOLONGNAMETOOLONGNAMETOOLONGNAMEX 1 ;", ERROR_INVALID_NAME },
      { "IF", ERROR_OPERATION_UNDEFINED },
      { ": A THEN ;", ERROR_INVALID_NAME },
      { ": A BEGIN THEN ;", ERROR_INVALID_NAME },
      { ": A IF ;", ERROR_INVALID_NAME },
      { ": CE5 IF IF IF IF IF IF IF IF IF 1 ;", ERROR_RAM_FULL },
      { "RECURSE", ERROR_OPERATION_UNDEFINED },
      { "GLOBAL", ERROR_INVALID_NAME },
      { "FORGET", ERROR_INVALID_NAME },
      { "XEQ", ERROR_INVALID_NAME },
      { "XEQ AB", ERROR_INVALID_NAME },
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
      lastErrorCode = ERROR_NONE;
      forthDictClear();
      forthGDictClear();
      if (forthCheckSourceLine(cases[i].line)) {
        printf("    [1] FAIL: \"%s\" should reject\n", cases[i].line);
        subFail = 1;
      } else if (lastErrorCode != cases[i].code) {
        printf("    [1] FAIL: \"%s\" code %d expected %d\n",
               cases[i].line, lastErrorCode, cases[i].code);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) printf("    [1] PASS: tier-1 structural violations reject with their runtime codes\n");
    fail |= subFail;
  }

  /* Subcase 2: Tier-2 acceptances */
  { int subFail = 0;
    const char *lines[] = {
      "UNKNOWNWORD9",
      "SDL",
      "SDL 100",
      "RTN",
      "STO 'NEVERMADE'",
      "FORGET NOSUCH",
      "XEQ 'NOLABEL'",
      "3 4 +",
      ": D2 2 / ; 8 D2",
    };
    size_t i;
    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
      lastErrorCode = ERROR_NONE;
      forthDictClear();
      forthGDictClear();
      if (!forthCheckSourceLine(lines[i])) {
        printf("    [2] FAIL: \"%s\" should accept (code %d)\n", lines[i], lastErrorCode);
        subFail = 1;
      } else if (lastErrorCode != ERROR_NONE) {
        printf("    [2] FAIL: \"%s\" returned true but code %d\n", lines[i], lastErrorCode);
        subFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
    if (!subFail) printf("    [2] PASS: names and item-level conditions stay advisory\n");
    fail |= subFail;
  }

  /* Subcase 3: Zero side effects */
  { int subFail = 0;
    uint16_t savedFdictHere = fdict.here;
    uint16_t savedFdictCount = fdict.count;
    uint16_t savedFdictLatest = fdict.latest;
    uint16_t savedGdictHere = gdict.here;
    uint16_t savedGdictCount = gdict.count;
    uint16_t savedGdictLatest = gdict.latest;
    uint8_t savedRsp = forthTestGetRsp();
    forthPushInt32(123);

    const char *allLines[] = {
      ": A : B ;", ": ;", ";", ": A",
      ": TOOLONGNAMETOOLONGNAMETOOLONGNAMEX 1 ;",
      "IF", ": A THEN ;", ": A BEGIN THEN ;", ": A IF ;",
      ": CE5 IF IF IF IF IF IF IF IF IF 1 ;",
      "RECURSE", "GLOBAL", "FORGET", "XEQ", "XEQ AB",
      "UNKNOWNWORD9", "SDL", "SDL 100", "RTN",
      "STO 'NEVERMADE'", "FORGET NOSUCH", "XEQ 'NOLABEL'",
      "3 4 +", ": D2 2 / ; 8 D2",
    };
    size_t i;
    for (i = 0; i < sizeof(allLines) / sizeof(allLines[0]); i++) {
      lastErrorCode = ERROR_NONE;
      forthCheckSourceLine(allLines[i]);
      lastErrorCode = ERROR_NONE;
    }

    if (fdict.here != savedFdictHere) {
      printf("    [3] FAIL: fdict.here changed %u->%u\n", savedFdictHere, fdict.here);
      subFail = 1;
    }
    if (fdict.latest != savedFdictLatest) {
      printf("    [3] FAIL: fdict.latest changed %u->%u\n", savedFdictLatest, fdict.latest);
      subFail = 1;
    }
    if (fdict.count != savedFdictCount) {
      printf("    [3] FAIL: fdict.count changed %u->%u\n", savedFdictCount, fdict.count);
      subFail = 1;
    }
    if (gdict.here != savedGdictHere) {
      printf("    [3] FAIL: gdict.here changed %u->%u\n", savedGdictHere, gdict.here);
      subFail = 1;
    }
    if (gdict.count != savedGdictCount) {
      printf("    [3] FAIL: gdict.count changed %u->%u\n", savedGdictCount, gdict.count);
      subFail = 1;
    }
    if (gdict.latest != savedGdictLatest) {
      printf("    [3] FAIL: gdict.latest changed %u->%u\n", savedGdictLatest, gdict.latest);
      subFail = 1;
    }
    if (forthTestGetRsp() != savedRsp) {
      printf("    [3] FAIL: rsp changed %u->%u\n", savedRsp, forthTestGetRsp());
      subFail = 1;
    }
    if (!x_is_longint(123)) {
      printf("    [3] FAIL: X is not 123\n");
      subFail = 1;
    }
    if (!subFail) printf("    [3] PASS: the check mode mutates nothing\n");
    fail |= subFail;
  }

  /* Subcase 4: Soundness battery — every check reject reproduces at execution */
  { int subFail = 0;
    const char *rejectLines[] = {
      ": A : B ;", ": ;", ";", ": A",
      ": TOOLONGNAMETOOLONGNAMETOOLONGNAMEX 1 ;",
      "IF", ": A THEN ;", ": A BEGIN THEN ;", ": A IF ;",
      ": CE5 IF IF IF IF IF IF IF IF IF 1 ;",
      "RECURSE", "GLOBAL", "FORGET", "XEQ", "XEQ AB",
    };
    size_t i;
    for (i = 0; i < sizeof(rejectLines) / sizeof(rejectLines[0]); i++) {
      uint8_t checkCode, runCode;

      lastErrorCode = ERROR_NONE;
      forthDictClear();
      forthGDictClear();
      forthCheckSourceLine(rejectLines[i]);
      checkCode = lastErrorCode;
      lastErrorCode = ERROR_NONE;

      lastErrorCode = ERROR_NONE;
      forthDictClear();
      forthGDictClear();
      forthOuterInterpret(rejectLines[i]);
      runCode = lastErrorCode;
      lastErrorCode = ERROR_NONE;

      if (checkCode != runCode) {
        printf("    [4] FAIL: \"%s\" check=%d run=%d\n",
               rejectLines[i], checkCode, runCode);
        subFail = 1;
      }
    }
    if (!subFail) printf("    [4] PASS: every check reject reproduces at execution with the same code\n");
    fail |= subFail;
  }

  /* Subcase 5: The documented suppression edge */
  { int subFail = 0;
    /* 9999999999999999999999999999999999999999E9999 classifies as real */
    const char *line1 = "9999999999999999999999999999999999999999E9999";
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    forthGDictClear();
    bool checkReject = !forthCheckSourceLine(line1);
    uint8_t rejectCode = lastErrorCode;
    lastErrorCode = ERROR_NONE;

    if (checkReject && rejectCode == ERROR_INVALID_NAME) {
      /* Now define a colon shadow with that name and check suppression */
      if (!startDefinition(line1)) {
        printf("    [5] FAIL: startDefinition for shadow name\n");
        subFail = 1;
      } else {
        if (!finishDefinition()) {
          printf("    [5] FAIL: finishDefinition for shadow\n");
          subFail = 1;
        }
      }
      if (!subFail) {
        lastErrorCode = ERROR_NONE;
        if (!forthCheckSourceLine(line1)) {
          printf("    [5] FAIL: shadow did not suppress (code %d)\n", lastErrorCode);
          subFail = 1;
        }
      }
    } else if (!checkReject) {
      /* parseNumberAsReal34 accepted it — suppression edge untestable */
      printf("    [5] CONFIG: no parse-failing numeric form found — suppression edge untestable, tier-1(f) reachable only via conversion failures\n");
    } else {
      printf("    [5] FAIL: unexpected check result code=%d\n", rejectCode);
      subFail = 1;
    }
    lastErrorCode = ERROR_NONE;
    if (!subFail) printf("    [5] PASS: number tier-1 fires and the live-shadow suppression holds\n");
    fail |= subFail;
  }

  /* Subcase 6 (F5-2A): check mode is state-NEUTRAL. §10.5 says check mode
   * "executes nothing, allocates nothing, mutates no live state", but the
   * landed F5-1 pins only read the verdict — so forthCheckSourceLine could
   * (and did) restore forthCurrentScope from an uninitialized ctx field
   * without any test noticing. It stayed latent until F5-2 called check
   * mode from pemAlpha's commit seam, where the garbage scope poisoned four
   * unrelated tests. This subcase pins the contract itself, from a scope
   * that is NOT the default, over both an accepted and a rejected line.
   * poisonAutoFrame() fills the stack region the callee's frame will occupy
   * with 0xAA, so an uninitialized restore is deterministic (0xAAAA), not
   * luck-of-the-stack. */
  { int subFail = 0;
    const uint16_t probeScope = 0x1234;
    lastErrorCode = ERROR_NONE;
    forthDictClear();
    forthGDictClear();
    { uint16_t rspBefore = forthTestGetRsp();
      forthDefState_t defBefore, defAfter;
      forthDefStateSave(&defBefore);

      forthTestScopeSet(probeScope);
      poisonAutoFrame();
      (void)forthCheckSourceLine("1 2 +");             /* accepted line */
      if (forthCurrentScopeGet() != probeScope) {
        printf("    [6] FAIL: accepted line left scope %u (expected %u)\n",
               forthCurrentScopeGet(), probeScope);
        subFail = 1;
      }
      if (!subFail) {
        poisonAutoFrame();
        (void)forthCheckSourceLine(": A IF ;");        /* rejected line */
        if (forthCurrentScopeGet() != probeScope) {
          printf("    [6] FAIL: rejected line left scope %u (expected %u)\n",
                 forthCurrentScopeGet(), probeScope);
          subFail = 1;
        }
      }
      forthTestScopeSet(FORTH_OWNER_INTERACTIVE);
      lastErrorCode = ERROR_NONE;

      forthDefStateSave(&defAfter);
      if (!subFail && (defBefore.here != defAfter.here ||
                       defBefore.latest != defAfter.latest ||
                       defBefore.count != defAfter.count ||
                       defBefore.entryOff != defAfter.entryOff ||
                       defBefore.open != defAfter.open)) {
        printf("    [6] FAIL: check mode mutated the open-definition state "
               "(h %u/%u l %u/%u c %u/%u e %u/%u o %d/%d)\n",
               defBefore.here, defAfter.here, defBefore.latest, defAfter.latest,
               defBefore.count, defAfter.count, defBefore.entryOff, defAfter.entryOff,
               defBefore.open, defAfter.open);
        subFail = 1;
      }
      if (!subFail && forthTestGetRsp() != rspBefore) {
        printf("    [6] FAIL: check mode moved rsp (%u -> %u)\n", rspBefore, forthTestGetRsp());
        subFail = 1;
      }
      if (!subFail && (fdict.count != 0 || fdict.here != 0)) {
        printf("    [6] FAIL: check mode wrote the dictionary (count=%u here=%u)\n",
               fdict.count, fdict.here);
        subFail = 1;
      }
    }
    if (!subFail) printf("    [6] PASS: check mode restores scope, def state, rsp, and writes nothing\n");
    fail |= subFail;
  }

  lastErrorCode = ERROR_NONE;
  return fail;
}

/* test_capture_buffer
 * F6-1: managed capture buffer — Forth capture text moves off aimBuffer.
 * Eleven subcases verifying the capture object lifecycle. */
static int test_capture_buffer(void)
{
  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedMenu = currentMenu();
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void pemAlpha(int16_t);
  extern void pemCloseAlphaInput(void);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);

  /* Build fixture: LBL, marker, RTN */
  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F61");
  tpMarker(&p);
  tpRtn(&p);

  if (!tpWrite(&p)) {
    printf("    FIXTURE FAIL: tpWrite\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  /* Position on the opening marker (step 2) */
  fnGotoDot(2);

  if (currentStep != tpStepAddr(&p, 1)) {
    printf("    FIXTURE BUG: fnGotoDot(2) did not position on marker\n");
    fail = 1;
  }
  else if (currentLocalStepNumber != 2) {
    printf("    FIXTURE BUG: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
    fail = 1;
  }
  else {
    /* ---- Subcase 1: Open state ---- */
    { int sc1 = 0;
      runFunction(ITM_AIM);
      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture not open (state=%d)\n", forthTestCapState());
        sc1 = 1;
      }
      else if (!getSystemFlag(FLAG_ALPHA)) {
        printf("    [1] FAIL: FLAG_ALPHA not set\n");
        sc1 = 1;
      }
      else if (aimBuffer[0] != 0) {
        printf("    [1] FAIL: aimBuffer not empty\n");
        sc1 = 1;
      }
      if (!sc1) printf("    [1] PASS: capture opens with a managed buffer and an empty aimBuffer\n");
      fail |= sc1;
    }

    /* ---- Subcase 2: Typing lands in the buffer and the step, never aimBuffer ---- */
    { int sc2 = 0;
      if (!fail) {
        const int16_t items[] = {
          ITM_3, ITM_SPACE, ITM_4, ITM_SPACE, ITM_PLUS
        };
        int i;
        for (i = 0; i < (int)(sizeof(items) / sizeof(items[0])); i++) {
          runFunction(items[i]);
        }
        if (strcmp(forthTestCapText(), "3 4 +") != 0) {
          printf("    [2] FAIL: cap text = '%s', expected '3 4 +'\n", forthTestCapText());
          sc2 = 1;
        }
        else if (aimBuffer[0] != 0) {
          printf("    [2] FAIL: aimBuffer not empty (should be 0)\n");
          sc2 = 1;
        }
      }
      if (!sc2) printf("    [2] PASS: sink is the managed buffer; step re-commits per key\n");
      fail |= sc2;
    }

    /* ---- Subcase 3: ENTER multi-line relock ---- */
    { int sc3 = 0;
      if (!fail) {
        lastErrorCode = ERROR_NONE;
        runFunction(ITM_ENTER);
        if (lastErrorCode != ERROR_NONE) {
          printf("    [3] FAIL: ENTER error %d\n", lastErrorCode);
          sc3 = 1;
        }
        else if (forthTestCapState() != FCAP_OPEN) {
          printf("    [3] FAIL: capture not open after ENTER relock (state=%d)\n", forthTestCapState());
          sc3 = 1;
        }
        else if (forthTestCapText()[0] != 0) {
          printf("    [3] FAIL: cap text not empty after relock\n");
          sc3 = 1;
        }
      }
      if (!sc3) printf("    [3] PASS: ENTER commits and relocks a fresh managed line\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc3;
    }

    /* ---- Subcase 4: Backspace + mid-line edit + two-byte glyph ---- */
    { int sc4 = 0;
      if (!fail) {
        runFunction(ITM_A);
        runFunction(ITM_B);
        runFunction(ITM_BACKSPACE);
        runFunction(ITM_C);
        if (strcmp(forthTestCapText(), "AC") != 0) {
          printf("    [4] FAIL: cap text = '%s', expected 'AC'\n", forthTestCapText());
          sc4 = 1;
        }
        if (!sc4) {
          runFunction(ITM_CLA);
          runFunction(ITM_CROSS);
          if (strcmp(forthTestCapText(), STD_CROSS) != 0 ||
              stringByteLength((char *)forthTestCapText()) != 2 ||
              stringGlyphLength((char *)forthTestCapText()) != 1) {
            printf("    [4] FAIL: two-byte cross fixture is not one glyph\n");
            sc4 = 1;
          }
          else {
            runFunction(ITM_BACKSPACE);
            if (forthTestCapText()[0] != 0 || T_cursorPos != 0) {
              printf("    [4] FAIL: one BACKSPACE did not remove the whole cross glyph\n");
              sc4 = 1;
            }
          }
        }
      }
      if (!sc4) printf("    [4] PASS: glyph edits and two-byte BACKSPACE operate on the managed buffer\n");
      fail |= sc4;
    }

    /* ---- Subcase 5: Empty-line BACKSPACE closes and frees ---- */
    { int sc5 = 0;
      uint32_t freeBefore5;
      if (!fail) {
        /* Close any open line: clear buffer, then BACKSPACE to abort */
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
        freeBefore5 = getFreeRamMemory();
        /* Reopen */
        runFunction(ITM_AIM);
        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [5] FAIL: reopen failed\n");
          sc5 = 1;
        }
        else {
          /* Empty-line abort */
          runFunction(ITM_BACKSPACE);
          if (forthTestCapState() != FCAP_CLOSED) {
            printf("    [5] FAIL: capture not closed (state=%d)\n", forthTestCapState());
            sc5 = 1;
          }
          else if (getSystemFlag(FLAG_ALPHA)) {
            printf("    [5] FAIL: FLAG_ALPHA still set\n");
            sc5 = 1;
          }
          else if (getFreeRamMemory() != freeBefore5) {
            printf("    [5] FAIL: freeRam changed %u -> %u\n",
                   (unsigned)freeBefore5, (unsigned)getFreeRamMemory());
            sc5 = 1;
          }
        }
      }
      if (!sc5) printf("    [5] PASS: abort closes capture and frees the buffer\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc5;
    }

    /* ---- Subcase 6: EDIT reopen refills from the step ---- */
    { int sc6 = 0;
      if (!fail) {
        /* Reopen, type AB CD, ENTER, EXIT to close */
        runFunction(ITM_AIM);
        runFunction(ITM_A);
        runFunction(ITM_B);
        runFunction(ITM_SPACE);
        runFunction(ITM_C);
        runFunction(ITM_D);
        runFunction(ITM_ENTER);
        /* Now on fresh relock line; BACKSPACE aborts the empty placeholder,
         * deleting it in place — currentStep/currentLocalStepNumber then
         * name whatever slides into that address (structurally: .END.,
         * since the aborted placeholder sat right before it), one step
         * past the committed AB CD. Step back onto it directly (the same
         * findPreviousStep/decrement pair pemAlpha's own insert paths use
         * throughout this file) rather than a hardcoded fnGotoDot(N),
         * which would require an absolute step count across five prior
         * subcases' mutations — exactly the fragility the fixture-
         * authoring rule forbids. */
        runFunction(ITM_BACKSPACE);
        currentStep = findPreviousStep(currentStep);
        --currentLocalStepNumber;
        calcMode = CM_PEM;
        tam.mode = 0;
        clearSystemFlag(FLAG_ALPHA);
        tam.function = 0;
        pemAlpha(ITM_EDIT);
        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [6] FAIL: capture not open after EDIT\n");
          sc6 = 1;
        }
        else if (strcmp(forthTestCapText(), "AB CD") != 0) {
          printf("    [6] FAIL: cap text = '%s', expected 'AB CD'\n", forthTestCapText());
          sc6 = 1;
        }
        else if (T_cursorPos != stringLastGlyph(forthTestCapText()) + 1) {
          printf("    [6] FAIL: cursor not at end\n");
          sc6 = 1;
        }
        /* Close via empty-abort */
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
      }
      if (!sc6) printf("    [6] PASS: EDIT refills the managed buffer from the step\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc6;
    }

    /* ---- Subcase 7: TAM entry suspends the capture (F6-2) ---- */
    { int sc7 = 0;
      if (!fail) {
        runFunction(ITM_AIM);
        runFunction(ITM_9);
        runFunction(ITM_STO);
        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    [7] FAIL: capture not suspended after TAM entry (state=%d)\n", forthTestCapState());
          sc7 = 1;
        }
        else if (tam.mode == 0) {
          printf("    [7] FAIL: TAM not open\n");
          sc7 = 1;
        }
        /* Cancel TAM */
        fnKeyExit(NOPARAM);
        if (tam.mode != 0) {
          fnKeyExit(NOPARAM);
        }
        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [7] FAIL: capture not resumed after TAM cancel (state=%d)\n", forthTestCapState());
          sc7 = 1;
        }
        else if (strcmp(forthTestCapText(), "9") != 0) {
          printf("    [7] FAIL: cap text = '%s', expected '9'\n", forthTestCapText());
          sc7 = 1;
        }
      }
      if (!sc7) printf("    [7] PASS: TAM entry suspends the capture (F6-2)\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc7;
    }

    /* ---- Subcase 8: 196-glyph cap ---- */
    { int sc8 = 0;
      if (!fail) {
        runFunction(ITM_AIM);
        int i;
        for (i = 0; i < 98; i++) {
          runFunction(ITM_X);
          runFunction(ITM_SPACE);
        }
        if (stringGlyphLength(forthTestCapText()) != 196) {
          printf("    [8] FAIL: glyph count = %d, expected 196\n",
                 stringGlyphLength(forthTestCapText()));
          sc8 = 1;
        }
        else {
          int16_t lenBefore = stringGlyphLength(forthTestCapText());
          runFunction(ITM_X);
          if (stringGlyphLength(forthTestCapText()) != lenBefore) {
            printf("    [8] FAIL: 197th glyph accepted\n");
            sc8 = 1;
          }
        }
        if (!sc8) {
          runFunction(ITM_ENTER);
          if (lastErrorCode != ERROR_NONE) {
            printf("    [8] FAIL: ENTER error %d\n", lastErrorCode);
            sc8 = 1;
          }
        }
      }
      if (!sc8) printf("    [8] PASS: 196-glyph cap holds on the managed buffer\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc8;
    }

    /* ---- Subcase 9: E9 composition ---- */
    { int sc9 = 0;
      if (!fail) {
        lastErrorCode = ERROR_NONE;
        const int16_t badItems[] = {
          ITM_COLON, ITM_SPACE, ITM_A, ITM_SPACE,
          ITM_I, ITM_F, ITM_SPACE,
          ITM_SEMICOLON, ITM_ENTER
        };
        int i;
        for (i = 0; i < (int)(sizeof(badItems) / sizeof(badItems[0])); i++) {
          runFunction(badItems[i]);
        }
        if (lastErrorCode == ERROR_NONE) {
          printf("    [9] FAIL: no error for malformed line\n");
          sc9 = 1;
        }
        else if (forthTestCapState() != FCAP_OPEN) {
          printf("    [9] FAIL: capture not open after refusal (state=%d)\n", forthTestCapState());
          sc9 = 1;
        }
        else if (strcmp(forthTestCapText(), ": A IF ;") != 0) {
          printf("    [9] FAIL: cap text = '%s', expected ': A IF ;'\n", forthTestCapText());
          sc9 = 1;
        }
        lastErrorCode = ERROR_NONE;
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
      }
      if (!sc9) printf("    [9] PASS: E9 refusal leaves the managed line intact\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc9;
    }

    /* ---- Subcase 10: EXIT with text commits and closes (ladder parity) ---- */
    { int sc10 = 0;
      uint32_t freeBefore10;
      if (!fail) {
        freeBefore10 = getFreeRamMemory();
        runFunction(ITM_AIM);
        runFunction(ITM_7);
        fnKeyExit(NOPARAM);
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [10] FAIL: capture not closed (state=%d)\n", forthTestCapState());
          sc10 = 1;
        }
        else if (getSystemFlag(FLAG_ALPHA)) {
          printf("    [10] FAIL: FLAG_ALPHA still set\n");
          sc10 = 1;
        }
        else if (getFreeRamMemory() != freeBefore10) {
          printf("    [10] FAIL: freeRam changed %u -> %u\n",
                 (unsigned)freeBefore10, (unsigned)getFreeRamMemory());
          sc10 = 1;
        }
        else {
          calcMode = CM_PEM;
          tam.mode = 0;
          tam.function = 0;
          clearSystemFlag(FLAG_ALPHA);
          runFunction(ITM_EDIT);
          if (forthTestCapState() != FCAP_OPEN ||
              strcmp(forthTestCapText(), "7") != 0) {
            printf("    [10] FAIL: reopened half-line = '%s', state=%d\n",
                   forthTestCapText(), forthTestCapState());
            sc10 = 1;
          }
          if (forthTestCapState() == FCAP_OPEN) {
            fnKeyExit(NOPARAM);
          }
        }
      }
      if (!sc10) printf("    [10] PASS: EXIT with text commits, closes, and survives reopen\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc10;
    }

    /* ---- Subcase 11: EXIT on empty aborts ---- */
    { int sc11 = 0;
      if (!fail) {
        uint16_t stepsBefore = getNumberOfSteps();
        runFunction(ITM_AIM);
        fnKeyExit(NOPARAM);
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [11] FAIL: capture not closed (state=%d)\n", forthTestCapState());
          sc11 = 1;
        }
        else if (getNumberOfSteps() != stepsBefore) {
          printf("    [11] FAIL: step count changed %u -> %u (placeholder not deleted)\n",
                 stepsBefore, getNumberOfSteps());
          sc11 = 1;
        }
      }
      if (!sc11) printf("    [11] PASS: EXIT on empty aborts the placeholder\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc11;
    }
  }

  /* Cleanup */
  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  showSoftmenu(savedMenu);
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* test_capture_suspend
 * F6-2: TAM suspend/resume keeps capture alive. Six subcases verifying
 * suspend-on-entry, resume-on-exit, tam.colon no-leak, empty-line
 * uniformity, a falsified-step canary, and arena hygiene. */
static int test_capture_suspend(void)
{
  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedMenu = currentMenu();
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void pemAlpha(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);
  extern void tamProcessInput(uint16_t);

  /* Build fixture: LBL, marker (no RTN — the marker's region runs to
   * .END., which isAtEndOfProgram already recognizes without needing
   * the RTN-transparency fix). */
  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F62");
  tpMarker(&p);

  if (!tpWrite(&p)) {
    printf("    FIXTURE FAIL: tpWrite\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  /* Position on the opening marker (step 2) */
  fnGotoDot(2);

  if (currentStep != tpStepAddr(&p, 1)) {
    printf("    FIXTURE BUG: fnGotoDot(2) did not position on marker\n");
    fail = 1;
  }
  else if (currentLocalStepNumber != 2) {
    printf("    FIXTURE BUG: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
    fail = 1;
  }
  else {
    runFunction(ITM_AIM);
    if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
      printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
      fail = 1;
    }
  }

  if (!fail) {
    /* ---- Subcase 1: Commit round-trip ---- */
    { int sc1 = 0;
      const int16_t items[] = { ITM_5, ITM_SPACE, ITM_D, ITM_U, ITM_P };
      int i;
      for (i = 0; i < (int)(sizeof(items) / sizeof(items[0])); i++) {
        runFunction(items[i]);
      }
      uint16_t stepNumBefore = currentLocalStepNumber;
      uint8_t *capStepBefore = currentStep;
      uint8_t *nextStepBefore = findNextStep(currentStep);
      uint16_t stepsBeforeSTO = getNumberOfSteps();

      runFunction(ITM_STO);
      if (forthTestCapState() != FCAP_SUSPENDED) {
        printf("    [1] FAIL: capture not suspended after tamEnterMode (state=%d)\n",
               forthTestCapState());
        sc1 = 1;
      }
      else if (getSystemFlag(FLAG_ALPHA)) {
        printf("    [1] FAIL: FLAG_ALPHA still set while suspended\n");
        sc1 = 1;
      }

      if (!sc1) {
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_5);   /* two digits auto-fire the STO commit */

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [1] FAIL: capture not resumed after commit (state=%d)\n",
                 forthTestCapState());
          sc1 = 1;
        }
        /* F6-4: the suspended TAM commit is converted to canonical text
         * and appended, then the native step is deleted again — no step
         * survives after the capture line. */
        else if (strcmp(forthTestCapText(), "5 DUP STO 05 ") != 0) {
          printf("    [1] FAIL: cap text = '%s', expected '5 DUP STO 05 '\n", forthTestCapText());
          sc1 = 1;
        }
        else if (currentLocalStepNumber != stepNumBefore) {
          printf("    [1] FAIL: currentLocalStepNumber = %u, expected %u\n",
                 currentLocalStepNumber, stepNumBefore);
          sc1 = 1;
        }
        else if (tam.mode != 0 || tam.function != ITM_FORTH) {
          printf("    [1] FAIL: tam.mode=%d tam.function=%d, expected 0/ITM_FORTH\n",
                 (int)tam.mode, (int)tam.function);
          sc1 = 1;
        }
        else if (currentStep != capStepBefore) {
          printf("    [1] FAIL: currentStep moved off the capture line\n");
          sc1 = 1;
        }
        else if (findNextStep(currentStep) != nextStepBefore) {
          printf("    [1] FAIL: a step remains after the capture step\n");
          sc1 = 1;
        }
        else if (getNumberOfSteps() != stepsBeforeSTO) {
          printf("    [1] FAIL: step count = %u, expected %u (pre-STO)\n",
                 getNumberOfSteps(), stepsBeforeSTO);
          sc1 = 1;
        }
      }
      if (!sc1) printf("    [1] PASS: TAM commit suspends, converts to text, and resumes\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc1;
    }

    /* ---- Subcase 2: Cancel round-trip ---- */
    /* Text-loss bug fixed 2026-07-20 (code-audit finding, see
     * DESIGN-HISTORY.md same date): expected text is now "5 DUP STO 05 "
     * — subcase 1's F6-4 fold, preserved — matching the F6-2 packet's
     * "text intact" requirement. forthCaptureSuspend() (manage.c) now
     * recommits the buffer to the on-disk step before snapshotting its
     * offset, so a suspend/resume with no intervening keystroke can no
     * longer read a stale pre-fold snapshot. */
    { int sc2 = 0;
      if (!fail) {
        uint16_t stepsBefore = getNumberOfSteps();

        runFunction(ITM_STO);
        fnKeyExit(NOPARAM);   /* cancel before any digit */

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [2] FAIL: capture not open after cancel (state=%d)\n", forthTestCapState());
          sc2 = 1;
        }
        else if (strcmp(forthTestCapText(), "5 DUP STO 05 ") != 0) {
          printf("    [2] FAIL: cap text = '%s', expected '5 DUP STO 05 '\n", forthTestCapText());
          sc2 = 1;
        }
        else if (getNumberOfSteps() != stepsBefore) {
          printf("    [2] FAIL: step count changed %u -> %u (cancel inserted a step)\n",
                 stepsBefore, getNumberOfSteps());
          sc2 = 1;
        }
        else if (tam.mode != 0) {
          printf("    [2] FAIL: tam.mode = %d, expected 0\n", (int)tam.mode);
          sc2 = 1;
        }
        if (!sc2) {
          char preText[64];
          int16_t cursorBefore = T_cursorPos;
          xcopy(preText, forthTestCapText(),
                stringByteLength((char *)forthTestCapText()) + 1);

          runFunction(ITM_XEQ);
          if (forthTestCapState() != FCAP_SUSPENDED) {
            printf("    [2] FAIL: XEQ did not suspend capture (state=%d)\n",
                   forthTestCapState());
            sc2 = 1;
          }
          else {
            tamProcessInput(ITM_alpha);
            runFunction(ITM_W);
            runFunction(ITM_A);

            int guard;
            for (guard = 0; tam.mode != 0 && guard < 4; guard++) {
              fnKeyExit(NOPARAM);
            }

            if (forthTestCapState() != FCAP_OPEN || tam.mode != 0) {
              printf("    [2] FAIL: named XEQ cancel did not resume capture (state=%d tam.mode=%d)\n",
                     forthTestCapState(), (int)tam.mode);
              sc2 = 1;
            }
            else if (strcmp(forthTestCapText(), preText) != 0 ||
                     T_cursorPos != cursorBefore) {
              printf("    [2] FAIL: named XEQ cancel changed text/cursor ('%s', %d; expected '%s', %d)\n",
                     forthTestCapText(), T_cursorPos, preText, cursorBefore);
              sc2 = 1;
            }
            else if (getNumberOfSteps() != stepsBefore) {
              printf("    [2] FAIL: named XEQ cancel changed step count %u -> %u\n",
                     stepsBefore, getNumberOfSteps());
              sc2 = 1;
            }
          }
        }
      }
      if (!sc2) printf("    [2] PASS: STO and named XEQ cancels resume with text/cursor intact, no inserted step\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc2;
    }

    /* ---- Subcase 3: tam.colon no-leak ---- */
    { int sc3 = 0;
      if (!fail) {
        char preText[64];
        xcopy(preText, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);

        runFunction(ITM_XEQ);
        tamProcessInput(ITM_COLON);   /* the landed local-label gesture: sets tam.colon */

        int guard;
        for (guard = 0; tam.mode != 0 && guard < 4; guard++) {
          fnKeyExit(NOPARAM);
        }

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [3] FAIL: capture not open after cancel (state=%d)\n", forthTestCapState());
          sc3 = 1;
        }
        else if (tam.colon) {
          printf("    [3] FAIL: tam.colon leaked into the resumed capture\n");
          sc3 = 1;
        }
        else {
          runFunction(ITM_X);
          char expected[66];
          xcopy(expected, preText, stringByteLength(preText));
          expected[stringByteLength(preText)] = 'X';
          expected[stringByteLength(preText) + 1] = 0;
          if (strcmp(forthTestCapText(), expected) != 0) {
            printf("    [3] FAIL: cap text = '%s', expected '%s'\n", forthTestCapText(), expected);
            sc3 = 1;
          }
        }
      }
      if (!sc3) printf("    [3] PASS: nested tam.colon state does not leak into resumed capture\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc3;
    }

    /* ---- Subcase 4: Empty-line suspension (edge unified) ---- */
    { int sc4 = 0;
      if (!fail) {
        runFunction(ITM_ENTER);   /* commit + relock a fresh empty line */
        uint16_t stepsBefore = getNumberOfSteps();

        runFunction(ITM_STO);
        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    [4] FAIL: empty-line capture not suspended (state=%d)\n",
                 forthTestCapState());
          sc4 = 1;
        }
        fnKeyExit(NOPARAM);

        if (!sc4 && (forthTestCapState() != FCAP_OPEN ||
                     forthTestCapText()[0] != 0 ||
                     getNumberOfSteps() != stepsBefore)) {
          printf("    [4] FAIL: empty-line STO cancel changed state/text/steps\n");
          sc4 = 1;
        }

        if (!sc4) {
          runFunction(ITM_STO);
          if (forthTestCapState() != FCAP_SUSPENDED) {
            printf("    [4] FAIL: local-form capture not suspended (state=%d)\n",
                   forthTestCapState());
            sc4 = 1;
          }
        }
        if (!sc4) {
          tamProcessInput(ITM_PERIOD);
          tamProcessInput(ITM_0);
          tamProcessInput(ITM_5);
          if (forthTestCapState() != FCAP_OPEN ||
              strcmp(forthTestCapText(), "STO .05 ") != 0) {
            printf("    [4] FAIL: local-form text = '%s', state=%d\n",
                   forthTestCapText(), forthTestCapState());
            sc4 = 1;
          }
          else if (getNumberOfSteps() != stepsBefore) {
            printf("    [4] FAIL: local STO left a residual step (%u -> %u)\n",
                   stepsBefore, getNumberOfSteps());
            sc4 = 1;
          }
        }
      }
      if (!sc4) printf("    [4] PASS: STO .05 converts and empty-line STO cancel resumes uniformly\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc4;
    }

    /* ---- Subcase 5: Falsified-step canary ---- */
    { int sc5 = 0;
      if (!fail) {
        runFunction(ITM_STO);
        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    [5] FAIL: capture not suspended before falsification (state=%d)\n",
                 forthTestCapState());
          sc5 = 1;
        }
        else {
          /* Deliberate falsification: stomp the saved step's opcode byte
           * with ITM_RTN so the resume-time structural check must reject
           * it. */
          uint8_t *savedStep = beginOfProgramMemory + forthCapSavedStepOffset();
          savedStep[0] = 0x04;

          fnKeyExit(NOPARAM);

          if (forthTestCapState() != FCAP_CLOSED) {
            printf("    [5] FAIL: capture not closed after falsified resume (state=%d)\n",
                   forthTestCapState());
            sc5 = 1;
          }
          else if (tam.mode != 0) {
            printf("    [5] FAIL: tam.mode = %d, expected 0\n", (int)tam.mode);
            sc5 = 1;
          }
        }
      }
      if (!sc5) printf("    [5] PASS: falsified suspension abandons safely\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc5;
      /* Restore program memory: the falsification corrupted it. */
      cleanupTestProgram();
      forthDictClear();
      forthGDictClear();
      forthCapClose();
    }
  }

  /* ---- Subcase 6: Arena equality (fresh fixture) ---- */
  { int sc6 = 0;
    testProg_t p6;
    tpInit(&p6);
    tpLbl(&p6, "F62B");
    tpMarker(&p6);
    if (!tpWrite(&p6)) {
      printf("    [6] FAIL: tpWrite\n");
      sc6 = 1;
    }
    else {
      calcMode = CM_PEM;
      catalog = CATALOG_NONE;
      tam.mode = 0;
      tam.function = 0;
      aimBuffer[0] = 0;
      programRunStop = PGM_STOPPED;
      dynamicMenuItem = -1;
      pemCursorIsZerothStep = false;
      clearSystemFlag(FLAG_ALPHA);
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      uint32_t freeBefore6 = getFreeRamMemory();

      fnGotoDot(2);
      runFunction(ITM_AIM);
      runFunction(ITM_1);

      /* suspend -> cancel-resume, twice */
      runFunction(ITM_STO);
      fnKeyExit(NOPARAM);
      runFunction(ITM_STO);
      fnKeyExit(NOPARAM);

      /* BACKSPACE-abort to close (empty the line first) */
      if (forthTestCapState() == FCAP_OPEN) {
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
      }

      bool_t escapeValve = false;
      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [6] FAIL: capture not closed at end (state=%d)\n", forthTestCapState());
        sc6 = 1;
      }
      else if (getFreeRamMemory() != freeBefore6) {
        uint32_t after6 = getFreeRamMemory();
        uint32_t delta = (freeBefore6 > after6) ? (freeBefore6 - after6) : (after6 - freeBefore6);
        if (delta == BYTES_PER_BLOCK && freeBefore6 > after6) {
          /* Escape valve (packet-anticipated): the FIRST ITM_AIM open
           * inserted the capture placeholder before .END. with zero
           * program-memory slack, growing the block allocation by one
           * quantum; deleteStepsFromTo only ever adjusts
           * firstFreeProgramByte/freeProgramBytes bookkeeping, it never
           * calls resizeProgramMemory to shrink back. Pre-existing
           * program-memory behavior, not a capture-buffer leak: the
           * suspend/resume cycles themselves (the thing under test) are
           * a separate 64-block allocation that free/alloc back to the
           * same address every cycle (F6-1 established this). */
          printf("    [6] PASS (escape valve): freeRam %u -> %u is one program-memory"
                 " resize quantum (%u B), not a capture leak\n",
                 (unsigned)freeBefore6, (unsigned)after6, (unsigned)BYTES_PER_BLOCK);
          escapeValve = true;
        } else {
          printf("    [6] FAIL: freeRam changed %u -> %u\n",
                 (unsigned)freeBefore6, (unsigned)after6);
          sc6 = 1;
        }
      }
      if (!sc6 && !escapeValve) printf("    [6] PASS: suspend/resume cycles leave zero arena residue\n");
    }
    lastErrorCode = ERROR_NONE;
    fail |= sc6;
  }

  /* Cleanup */
  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  showSoftmenu(savedMenu);
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* test_capture_menus
 * F6-3: a catalog pick is a text insertion. Six subcases: item-pick
 * insertion, glyph-arm precedence, EXIT-ladder pop/commit/abort, picker
 * navigation, and cursor-position discipline. */
static int test_capture_menus(void)
{
  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void showSoftmenu(int16_t);
  extern void testInitVariableSoftmenu(int16_t);
  extern bool_t isAlphaSubmenu(uint8_t);
  extern void _closeCatalog(void);

  /* Build fixture: LBL, marker (no colon-defs — the F6-5 stage owns
   * section (a) content; this stage only needs a valid, poppable
   * -MNU_FORTH picker on the stack). */
  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F63");
  tpMarker(&p);

  if (!tpWrite(&p)) {
    printf("    FIXTURE FAIL: tpWrite\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  /* Position on the opening marker (step 2) */
  fnGotoDot(2);

  if (currentStep != tpStepAddr(&p, 1)) {
    printf("    FIXTURE BUG: fnGotoDot(2) did not position on marker\n");
    fail = 1;
  }
  else if (currentLocalStepNumber != 2) {
    printf("    FIXTURE BUG: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
    fail = 1;
  }
  else {
    runFunction(ITM_AIM);
    if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
      printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
      fail = 1;
    }
  }

  if (!fail) {
    /* ---- Subcase 1: Item pick inserts its name ---- */
    { int sc1 = 0;
      runFunction(ITM_1);
      runFunction(ITM_SPACE);

      /* Reproduce the keyboard.c catalog-dispatch order: menu setup first,
       * then arm fnKeyInCatalog for the selected FCNS item, dispatch it, and
       * drain the catalog stack. */
      showSoftmenu(-MNU_CATALOG);
      showSoftmenu(-MNU_FCNS);
      fnKeyInCatalog = 1;
      runFunction(ITM_sin);
      _closeCatalog();
      fnKeyInCatalog = savedFnKeyInCatalog;

      if (strcmp(forthTestCapText(), "1 SIN ") != 0) {
        printf("    [1] FAIL: cap text = '%s', expected '1 SIN '\n", forthTestCapText());
        sc1 = 1;
      }
      else if (currentMenu() != -MNU_CATALOG) {
        printf("    [1] FAIL: currentMenu = %d after FCNS pick, expected -MNU_CATALOG (%d)\n",
               currentMenu(), -MNU_CATALOG);
        sc1 = 1;
      }
      else if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture not open after FCNS pick (state=%d)\n",
               forthTestCapState());
        sc1 = 1;
      }
      else {
        uint8_t len = currentStep[3];
        int32_t textLen = stringByteLength((char *)forthTestCapText());
        bool_t bytesMatch = true;
        if (len == textLen) {
          int32_t i;
          for (i = 0; i < textLen; i++) {
            if (currentStep[4 + i] != (uint8_t)forthTestCapText()[i]) { bytesMatch = false; break; }
          }
        } else {
          bytesMatch = false;
        }
        if (!bytesMatch) {
          printf("    [1] FAIL: step image does not track the insert (len=%u, expected %d)\n",
                 len, textLen);
          sc1 = 1;
        }
      }
      /* itemCatalogName/itemSoftmenuName both read "SIN" for ITM_sin
       * (items.c:1859), so a field-swap mutation is silent against SIN
       * alone (F15-4/F15-5 precedent, DESIGN-HISTORY.md 2026-07-18).
       * ARCCOS's two fields diverge ("ARCCOS" catalog vs "ACOS" softmenu,
       * items.c:1864, ITM_arccos=81, same CAT_FNCT|PTP_NONE class) so a
       * field swap is observable here. */
      if (!sc1) {
        runFunction(ITM_arccos);
        if (strcmp(forthTestCapText(), "1 SIN ARCCOS ") != 0) {
          printf("    [1] FAIL: cap text after ARCCOS pick = '%s', expected '1 SIN ARCCOS '\n",
                 forthTestCapText());
          sc1 = 1;
        }
      }
      if (currentMenu() == -MNU_CATALOG) {
        /* Fixture teardown only: after a real FCNS pick the selector closes
         * while its CATALOG root remains displayed.  A physical EXIT here
         * belongs to the capture EXIT ladder, not to B4. */
        popSoftmenu();
      }
      if (!sc1 && (currentMenu() != -MNU_ALPHA ||
                   forthTestCapState() != FCAP_OPEN)) {
        printf("    [1] FAIL: catalog fixture teardown did not restore open ALPHA capture "
               "(menu=%d state=%d)\n", currentMenu(), forthTestCapState());
        sc1 = 1;
      }
      if (!sc1) {
        printf("    [1] PASS: FCNS pick inserts catalog text and returns to CATALOG root\n");
      }
      lastErrorCode = ERROR_NONE;
      fail |= sc1;
    }

    /* ---- Subcase 2: Glyph items still type ---- */
    { int sc2 = 0;
      if (!fail) {
        runFunction(ITM_2);
        if (strcmp(forthTestCapText(), "1 SIN ARCCOS 2") != 0) {
          printf("    [2] FAIL: cap text = '%s', expected '1 SIN ARCCOS 2'\n", forthTestCapText());
          sc2 = 1;
        }
      }
      if (!sc2) printf("    [2] PASS: glyph keys unaffected by the item arm\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc2;
    }

    /* ---- Subcase 3: Picker pops to ALPHA, capture survives ---- */
    { int sc3 = 0;
      if (!fail) {
        char textBefore[64];
        xcopy(textBefore, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);

        testInitVariableSoftmenu(22);
        showSoftmenu(-MNU_FORTH);

        if (currentMenu() != -MNU_FORTH) {
          printf("    [3] FAIL: picker not on top after push (currentMenu=%d)\n", currentMenu());
          sc3 = 1;
        }
        else {
          fnKeyExit(NOPARAM);
          if (currentMenu() != -MNU_ALPHA) {
            printf("    [3] FAIL: currentMenu = %d after EXIT, expected -MNU_ALPHA (%d)\n",
                   currentMenu(), -MNU_ALPHA);
            sc3 = 1;
          }
          else if (forthTestCapState() != FCAP_OPEN) {
            printf("    [3] FAIL: capture not open after EXIT (state=%d)\n", forthTestCapState());
            sc3 = 1;
          }
          else if (strcmp(forthTestCapText(), textBefore) != 0) {
            printf("    [3] FAIL: cap text = '%s', expected '%s'\n", forthTestCapText(), textBefore);
            sc3 = 1;
          }
        }
      }
      if (!sc3) printf("    [3] PASS: EXIT pops the picker back toward ALPHA\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc3;
    }

    /* ---- Subcase 4: Picker navigation leaks nothing ---- */
    { int sc4 = 0;
      if (!fail) {
        char textBefore[64];
        xcopy(textBefore, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);
        int16_t cursorBefore = T_cursorPos;

        testInitVariableSoftmenu(22);
        showSoftmenu(-MNU_FORTH);
        fnKeyExit(NOPARAM);   /* no page-navigation idiom on an empty picker; EXIT stands in */

        if (strcmp(forthTestCapText(), textBefore) != 0) {
          printf("    [4] FAIL: cap text = '%s', expected '%s'\n", forthTestCapText(), textBefore);
          sc4 = 1;
        }
        else if (T_cursorPos != cursorBefore) {
          printf("    [4] FAIL: T_cursorPos = %d, expected %d\n", T_cursorPos, cursorBefore);
          sc4 = 1;
        }
      }
      if (!sc4) printf("    [4] PASS: picker navigation leaves the capture line intact\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc4;
    }

    /* ---- Subcase 5: EXIT ladder end ---- */
    { int sc5 = 0;
      if (!fail) {
        if (forthTestCapState() != FCAP_OPEN || forthTestCapText()[0] == 0) {
          printf("    [5] FAIL: fixture bug — expected an open, non-empty line\n");
          sc5 = 1;
        }
        else {
          fnKeyExit(NOPARAM);   /* commit-with-text */
          if (forthTestCapState() != FCAP_CLOSED) {
            printf("    [5] FAIL: capture not closed after commit-EXIT (state=%d)\n",
                   forthTestCapState());
            sc5 = 1;
          }
          else {
            uint16_t stepsBefore = getNumberOfSteps();
            runFunction(ITM_AIM);   /* fresh empty line */
            fnKeyExit(NOPARAM);     /* abort-when-empty */
            if (forthTestCapState() != FCAP_CLOSED) {
              printf("    [5] FAIL: capture not closed after abort-EXIT (state=%d)\n",
                     forthTestCapState());
              sc5 = 1;
            }
            else if (getNumberOfSteps() != stepsBefore) {
              printf("    [5] FAIL: step count changed %u -> %u (placeholder not deleted)\n",
                     stepsBefore, getNumberOfSteps());
              sc5 = 1;
            }
          }
        }
      }
      if (!sc5) printf("    [5] PASS: EXIT ladder ends in commit-with-text / abort-when-empty\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc5;
    }

    /* ---- Subcase 6: Insert-at-cursor discipline ---- */
    { int sc6 = 0;
      if (!fail) {
        runFunction(ITM_AIM);
        runFunction(ITM_A);
        runFunction(ITM_B);
        T_cursorPos = 0;
        runFunction(ITM_sin);
        if (strcmp(forthTestCapText(), "SIN AB") != 0) {
          printf("    [6] FAIL: cap text = '%s', expected 'SIN AB'\n", forthTestCapText());
          sc6 = 1;
        }
        else if (T_cursorPos != 4) {
          printf("    [6] FAIL: T_cursorPos = %d, expected 4\n", T_cursorPos);
          sc6 = 1;
        }
      }
      if (!sc6) printf("    [6] PASS: item insert honors the cursor position\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc6;
    }
  }

  /* Cleanup */
  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  }

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  fnKeyInCatalog = savedFnKeyInCatalog;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* test_capture_param_text — F6-4: a suspended TAM commit is converted to
 * canonical text (through the landed decoder) and inserted at the
 * capture cursor; a TAM cancel still inserts nothing. */
static int test_capture_param_text(void)
{
  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  char aimBufSave[256];
  memcpy(aimBufSave, aimBuffer, sizeof(aimBufSave));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);
  extern void tamProcessInput(uint16_t);

  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F64");
  tpMarker(&p);

  if (!tpWrite(&p)) {
    printf("    FIXTURE FAIL: tpWrite\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  /* Position on the opening marker (step 2) */
  fnGotoDot(2);

  if (currentStep != tpStepAddr(&p, 1)) {
    printf("    FIXTURE BUG: fnGotoDot(2) did not position on marker\n");
    fail = 1;
  }
  else if (currentLocalStepNumber != 2) {
    printf("    FIXTURE BUG: currentLocalStepNumber = %u, expected 2\n", currentLocalStepNumber);
    fail = 1;
  }
  else {
    runFunction(ITM_AIM);
    if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
      printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
      fail = 1;
    }
  }

  if (!fail) {
    /* ---- Subcase 1: Cancel still inserts nothing ---- */
    { int sc1 = 0;
      uint16_t stepsBefore = getNumberOfSteps();

      runFunction(ITM_1);
      runFunction(ITM_STO);
      fnKeyExit(NOPARAM);   /* cancel before any digit */

      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture not open after cancel (state=%d)\n", forthTestCapState());
        sc1 = 1;
      }
      else if (strcmp(forthTestCapText(), "1") != 0) {
        printf("    [1] FAIL: cap text = '%s', expected '1'\n", forthTestCapText());
        sc1 = 1;
      }
      else if (getNumberOfSteps() != stepsBefore) {
        printf("    [1] FAIL: step count changed %u -> %u\n", stepsBefore, getNumberOfSteps());
        sc1 = 1;
      }
      if (!sc1) printf("    [1] PASS: TAM cancel converts nothing\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc1;
    }

    /* ---- Subcase 2: Name commit converts ---- */
    { int sc2 = 0;
      if (!fail) {
        /* Abort-close idiom (test_capture_suspend [6]): forthCapClose()
         * alone frees the buffer but leaves FLAG_ALPHA/tam.function
         * dangling, so a bare runFunction(ITM_AIM) then reads as the
         * TOGGLE-OFF gesture instead of a fresh open. CLA empties the
         * line, then BACKSPACE-on-empty performs the real exit. */
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
        runFunction(ITM_AIM);
        runFunction(ITM_2);
        runFunction(ITM_SPACE);
        uint16_t stepsBefore = getNumberOfSteps();

        runFunction(ITM_XEQ);
        tamProcessInput(ITM_alpha);
        runFunction(ITM_W);
        runFunction(ITM_A);
        tamProcessInput(ITM_ENTER);

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [2] FAIL: capture not resumed after commit (state=%d)\n",
                 forthTestCapState());
          sc2 = 1;
        }
        /* Escape valve (Change D discipline): if the decode spelling of
         * an XEQ name step ever differs from "XEQ 'WA'" (glyph quotes),
         * this is a packet-literal mismatch, not a local adaptation. */
        else if (strcmp(forthTestCapText(),
                         "2 XEQ " STD_LEFT_SINGLE_QUOTE "WA" STD_RIGHT_SINGLE_QUOTE " ") != 0) {
          printf("    [2] FAIL: cap text = '%s', expected \"2 XEQ 'WA' \"\n", forthTestCapText());
          sc2 = 1;
        }
        else if (getNumberOfSteps() != stepsBefore) {
          printf("    [2] FAIL: step count = %u, expected %u (no residual step)\n",
                 getNumberOfSteps(), stepsBefore);
          sc2 = 1;
        }
      }
      if (!sc2) printf("    [2] PASS: XEQ name commit becomes source text\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc2;
    }

    /* ---- Subcase 3: No-room keeps the step ---- */
    { int sc3 = 0;
      if (!fail) {
        /* Abort-close idiom (test_capture_suspend [6]); see subcase 2. */
        runFunction(ITM_CLA);
        runFunction(ITM_BACKSPACE);
        runFunction(ITM_AIM);
        int i;
        for (i = 0; i < 96; i++) {
          runFunction(ITM_X);
          runFunction(ITM_SPACE);
        }
        runFunction(ITM_X);   /* 193 glyphs: 3 shy of the 196 cap */

        if (stringGlyphLength(forthTestCapText()) != 193) {
          printf("    [3] FAIL: fixture glyph count = %d, expected 193\n",
                 stringGlyphLength(forthTestCapText()));
          sc3 = 1;
        }
        else {
          char textBefore[256];
          xcopy(textBefore, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);
          uint8_t *capStep = currentStep;

          runFunction(ITM_STO);
          tamProcessInput(ITM_0);
          tamProcessInput(ITM_5);   /* two digits auto-fire the STO commit */

          if (forthTestCapState() != FCAP_OPEN) {
            printf("    [3] FAIL: capture not resumed after commit (state=%d)\n",
                   forthTestCapState());
            sc3 = 1;
          }
          else if (strcmp(forthTestCapText(), textBefore) != 0) {
            printf("    [3] FAIL: cap text changed on a no-room conversion\n");
            sc3 = 1;
          }
          else {
            uint8_t *stoStep = findNextStep(capStep);
            if (stoStep[0] != 0x2C || stoStep[1] != 0x05) {
              printf("    [3] FAIL: STO step bytes = 0x%02X 0x%02X, expected 0x2C 0x05"
                     " (the step must remain when there is no room to convert)\n",
                     stoStep[0], stoStep[1]);
              sc3 = 1;
            }
          }
        }
      }
      if (!sc3) printf("    [3] PASS: no-room conversion keeps the committed step\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc3;
      /* Cleanup: this subcase leaves a stray STO step and a 193-glyph
       * capture line; cleanupTestProgram() resets the whole region
       * (sanctioned by the packet as the alternative to a manual
       * backspace-abort + step delete). Subcase 4 rebuilds its own
       * fixture. */
      forthCapClose();
      cleanupTestProgram();
    }

    /* ---- Subcase 4: Hygiene across cycles ---- */
    { int sc4 = 0;
      testProg_t p4;
      tpInit(&p4);
      tpLbl(&p4, "F64B");
      tpMarker(&p4);
      if (!tpWrite(&p4)) {
        printf("    [4] FAIL: fixture rebuild (tpWrite)\n");
        sc4 = 1;
      }
      else {
        calcMode = CM_PEM;
        catalog = CATALOG_NONE;
        tam.mode = 0;
        tam.function = 0;
        aimBuffer[0] = 0;
        programRunStop = PGM_STOPPED;
        dynamicMenuItem = -1;
        pemCursorIsZerothStep = false;
        clearSystemFlag(FLAG_ALPHA);
        lastErrorCode = ERROR_NONE;
        forthCapClose();

        uint32_t freeBefore4 = getFreeRamMemory();
        uint16_t stepsBefore4 = getNumberOfSteps();

        fnGotoDot(2);
        runFunction(ITM_AIM);

        /* Two convert cycles */
        runFunction(ITM_1);
        runFunction(ITM_STO);
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_5);
        runFunction(ITM_2);
        runFunction(ITM_STO);
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_6);

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [4] FAIL: capture not open after two convert cycles (state=%d)\n",
                 forthTestCapState());
          sc4 = 1;
        }

        /* Abort-close idiom (test_capture_suspend [6]): CLA then one BACKSPACE */
        if (forthTestCapState() == FCAP_OPEN) {
          runFunction(ITM_CLA);
          runFunction(ITM_BACKSPACE);
        }

        bool_t escapeValve4 = false;
        if (!sc4 && forthTestCapState() != FCAP_CLOSED) {
          printf("    [4] FAIL: capture not closed after abort (state=%d)\n",
                 forthTestCapState());
          sc4 = 1;
        }
        else if (!sc4 && getNumberOfSteps() != stepsBefore4) {
          printf("    [4] FAIL: step count = %u, expected %u (pre-open)\n",
                 getNumberOfSteps(), stepsBefore4);
          sc4 = 1;
        }
        else if (!sc4 && getFreeRamMemory() != freeBefore4) {
          uint32_t after4 = getFreeRamMemory();
          uint32_t delta4 = (freeBefore4 > after4) ? (freeBefore4 - after4) : (after4 - freeBefore4);
          /* Escape valve (F6-2 [6] precedent, widened for two cycles):
           * each convert cycle temporarily inserts a real native step
           * then deletes it again — deleteStepsFromTo is bookkeeping-
           * only, it never calls resizeProgramMemory to shrink the
           * block-level allocation back down (the same mechanism the
           * F6-2 escape valve documents for a single first-open).  Two
           * insert/delete cycles plus the first-ever AIM open on this
           * zero-slack fixture can each cost up to one quantum.
           * step count and pgmSize (firstFreeProgramByte) both fully
           * restored under direct measurement confirm this is
           * allocator quantization, not a leak, so a bounded,
           * block-aligned, growth-only residue is tolerated instead of
           * an unbounded one. */
          if (delta4 > 0 && delta4 % BYTES_PER_BLOCK == 0
              && delta4 <= 4 * BYTES_PER_BLOCK && freeBefore4 > after4) {
            printf("    [4] PASS (escape valve): freeRam %u -> %u is %u program-memory"
                   " resize quantum(s) (%u B each), not a conversion leak\n",
                   (unsigned)freeBefore4, (unsigned)after4,
                   (unsigned)(delta4 / BYTES_PER_BLOCK), (unsigned)BYTES_PER_BLOCK);
            escapeValve4 = true;
          } else {
            printf("    [4] FAIL: freeRam changed %u -> %u\n",
                   (unsigned)freeBefore4, (unsigned)after4);
            sc4 = 1;
          }
        }
        if (!sc4 && !escapeValve4) printf("    [4] PASS: conversion cycles leave zero residue\n");
      }
      lastErrorCode = ERROR_NONE;
      fail |= sc4;
    }
  }

  /* Cleanup */
  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  memcpy(aimBuffer, aimBufSave, sizeof(aimBufSave));
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* test_word_catalog — F6-5: MNU_FORTH becomes the union catalog of the
 * edited program's words (text scan), interactive-scope dictionary
 * words, and global dictionary words. */
static int test_word_catalog(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  int16_t savedDynamicMenu = dynamicMenuItem;

  dynamicMenuItem = -1;

  testProg_t p;
  tpInit(&p);
  tpLbl(&p, "F65");
  tpMarker(&p);
  int sDef = tpSrc(&p, ": PW 1 ;");
  int sClose = tpMarker(&p);
  if (sDef < 0 || sClose < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    programRunStop = savedRS;
    dynamicMenuItem = savedDynamicMenu;
    return 1;
  }

  /* Drive sDef once (F3-3 drive discipline) so fdict holds a
   * PROGRAM-owned PW entry — section (b) must not list it. */
  lastErrorCode = ERROR_NONE;
  programRunStop = PGM_RUNNING;
  forthRunGenBump();
  currentStep = tpStepAddr(&p, sDef);
  executeOneStep(currentStep);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FIXTURE FAIL: PW def step error %d\n", lastErrorCode);
    fail = 1;
  }
  programRunStop = savedRS;

  /* Interactive WI. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": WI 7 ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    FIXTURE FAIL: WI def error %d\n", lastErrorCode);
      fail = 1;
    }
  }

  /* Global GVIS (F3-3 subcase-5 fixture shape, verbatim: FTOK_ILIT,
   * int32 9, FTOK_EXIT). */
  if (!fail) {
    uint16_t gw = gbegin_word("GVIS", 4);
    if (gw == FORTH_NULL) {
      printf("    FIXTURE FAIL: gbegin_word GVIS\n");
      fail = 1;
    } else {
      gemit(T_ILIT);
      int32_t v = 9;
      if (!forthGDictEnsure(4)) {
        printf("    FIXTURE FAIL: forthGDictEnsure for int32\n");
        fail = 1;
      } else {
        memcpy(gdict.base + gdict.here, &v, 4);
        gdict.here += 4;
        gemit(T_EXIT);
        gdict.here = (uint16_t)TO_BLOCKS(gdict.here) * BYTES_PER_BLOCK;
      }
    }
  }

  if (!fail) {
    currentProgramNumber = 1;
    currentStep = tpStepAddr(&p, sClose);
  }

  if (!fail) {
    /* ---- Subcase 1: Union content and section order ---- */
    { int sc1 = 0;
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 3) {
        printf("    [1] FAIL: numItems = %d, expected 3\n", dynamicSoftmenu[22].numItems);
        sc1 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *item0 = (const char *)dynamicSoftmenu[22].menuContent;
        const char *item1 = item0 + strlen(item0) + 1;
        const char *item2 = item1 + strlen(item1) + 1;
        if (compareString(item0, "PW", CMP_BINARY) != 0) {
          printf("    [1] FAIL: item 0 = '%s', expected 'PW'\n", item0);
          sc1 = 1;
        }
        else if (compareString(item1, "WI", CMP_BINARY) != 0) {
          printf("    [1] FAIL: item 1 = '%s', expected 'WI'\n", item1);
          sc1 = 1;
        }
        else if (compareString(item2, "GVIS", CMP_BINARY) != 0) {
          printf("    [1] FAIL: item 2 = '%s', expected 'GVIS'\n", item2);
          sc1 = 1;
        }
      } else {
        printf("    [1] FAIL: menuContent is NULL\n");
        sc1 = 1;
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }
      if (!sc1) printf("    [1] PASS: catalog lists program, interactive, and global sections\n");
      fail |= sc1;
    }

    /* ---- Subcase 2: Program-owned dict entries stay out of section (b) ---- */
    { int sc2 = 0;
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 3) {
        printf("    [2] FAIL: numItems = %d, expected 3\n", dynamicSoftmenu[22].numItems);
        sc2 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        int pwCount = 0;
        for (int16_t i = 0; i < dynamicSoftmenu[22].numItems; i++) {
          if (compareString(content, "PW", CMP_BINARY) == 0) pwCount++;
          content += strlen(content) + 1;
        }
        if (pwCount != 1) {
          printf("    [2] FAIL: 'PW' appears %d times, expected 1\n", pwCount);
          sc2 = 1;
        }
      } else {
        printf("    [2] FAIL: menuContent is NULL\n");
        sc2 = 1;
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }
      if (!sc2) printf("    [2] PASS: program-owned dictionary entries are not double-listed\n");
      fail |= sc2;
    }

    /* ---- Subcase 3: Smudged entries absent ---- */
    { int sc3 = 0;
      forthTestSmudgeSet("WI", true);
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 2) {
        printf("    [3] FAIL: numItems = %d, expected 2 (WI smudged)\n", dynamicSoftmenu[22].numItems);
        sc3 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        for (int16_t i = 0; i < dynamicSoftmenu[22].numItems; i++) {
          if (compareString(content, "WI", CMP_BINARY) == 0) {
            printf("    [3] FAIL: smudged 'WI' still listed\n");
            sc3 = 1;
          }
          content += strlen(content) + 1;
        }
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }

      forthTestSmudgeSet("WI", false);
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 3) {
        printf("    [3] FAIL: numItems = %d, expected 3 (WI restored)\n", dynamicSoftmenu[22].numItems);
        sc3 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        bool_t foundWI = false;
        for (int16_t i = 0; i < dynamicSoftmenu[22].numItems; i++) {
          if (compareString(content, "WI", CMP_BINARY) == 0) foundWI = true;
          content += strlen(content) + 1;
        }
        if (!foundWI) {
          printf("    [3] FAIL: 'WI' missing after un-smudge\n");
          sc3 = 1;
        }
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }
      if (!sc3) printf("    [3] PASS: smudged entries never list\n");
      fail |= sc3;
    }

    /* ---- Subcase 4: Long names skipped, not truncated ---- */
    { int sc4 = 0;
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": WINTERACTIVELONG 1 ;");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [4] FAIL: WINTERACTIVELONG def error %d\n", lastErrorCode);
        sc4 = 1;
      }
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 3) {
        printf("    [4] FAIL: numItems = %d, expected 3 (unchanged)\n", dynamicSoftmenu[22].numItems);
        sc4 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        bool_t foundPW = false, foundWI = false, foundGVIS = false;
        for (int16_t i = 0; i < dynamicSoftmenu[22].numItems; i++) {
          if (compareString(content, "WINTERACTIVELON", CMP_BINARY) == 0) {
            printf("    [4] FAIL: truncated 'WINTERACTIVELON' entry present\n");
            sc4 = 1;
          }
          if (compareString(content, "PW", CMP_BINARY) == 0) foundPW = true;
          else if (compareString(content, "WI", CMP_BINARY) == 0) foundWI = true;
          else if (compareString(content, "GVIS", CMP_BINARY) == 0) foundGVIS = true;
          content += strlen(content) + 1;
        }
        /* The skip must leave the three pre-existing entries untouched,
         * not just the count coincidentally still 3 — a bug that skips
         * correctly but corrupts a neighboring entry into some other
         * (non-truncated) string would pass the count+truncation checks
         * alone. */
        if (!sc4 && (!foundPW || !foundWI || !foundGVIS)) {
          printf("    [4] FAIL: existing entries disturbed (PW=%d WI=%d GVIS=%d)\n",
                 foundPW, foundWI, foundGVIS);
          sc4 = 1;
        }
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }
      if (!sc4) printf("    [4] PASS: over-long names are skipped, not truncated\n");
      fail |= sc4;
    }

    /* ---- Subcase 5: Cross-section duplicate shows per section ---- */
    { int sc5 = 0;
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": PW 2 ;");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [5] FAIL: interactive PW def error %d\n", lastErrorCode);
        sc5 = 1;
      }
      testInitVariableSoftmenu(22);
      if (dynamicSoftmenu[22].numItems != 4) {
        printf("    [5] FAIL: numItems = %d, expected 4\n", dynamicSoftmenu[22].numItems);
        sc5 = 1;
      }
      else if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        int pwCount = 0;
        bool_t firstIsPW = compareString(content, "PW", CMP_BINARY) == 0;
        for (int16_t i = 0; i < dynamicSoftmenu[22].numItems; i++) {
          if (compareString(content, "PW", CMP_BINARY) == 0) pwCount++;
          content += strlen(content) + 1;
        }
        if (!firstIsPW) {
          printf("    [5] FAIL: section-a position no longer 'PW'\n");
          sc5 = 1;
        }
        else if (pwCount != 2) {
          printf("    [5] FAIL: 'PW' appears %d times, expected 2 (section a + section b)\n", pwCount);
          sc5 = 1;
        }
      }
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
        dynamicSoftmenu[22].numItems = 0;
      }
      if (!sc5) printf("    [5] PASS: cross-section duplicates list once per provenance\n");
      fail |= sc5;
    }
  }

  /* Cleanup */
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  }
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();

  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  programRunStop = savedRS;
  dynamicMenuItem = savedDynamicMenu;
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* test_capture_acceptance — F6-6: the stage's end-to-end pin. A full
 * type -> commit -> run session through the real toggle and key paths,
 * the EXIT ladder rung by rung, marker rules, the power-off/restore
 * contract, a cap round-trip, and an arena-residue sweep. */
static int test_capture_acceptance(void)
{
  int fail = 0;
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);
  extern void showSoftmenu(int16_t);
  extern void testInitVariableSoftmenu(int16_t);
  extern void addStepInProgram(int16_t func);
  extern void pemAlpha(int16_t);
  extern void fnExecute(uint16_t lbl);

  /* Fresh program: LBL only — the toggle inserts the marker (subcase 1). */
  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "F66");
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  currentProgramNumber = 1;

  fnGotoDot(1);
  if (currentStep != tpStepAddr(&p, sLbl) || currentLocalStepNumber != 1) {
    printf("    FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
    fail = 1;
  }

  if (!fail) {
    /* ---- Subcase 1: Toggle -> type -> run ---- */
    { int sc1 = 0;
      addStepInProgram(ITM_FORTH);   /* landed toggle idiom: open, §8.4 E1 */
      if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH || !forthCapIsOpen()) {
        printf("    [1] FAIL: toggle-open did not open Forth capture\n");
        sc1 = 1;
      }
      if (!sc1) {
        runFunction(ITM_COLON);
        runFunction(ITM_SPACE);
        runFunction(ITM_S);
        runFunction(ITM_Q);
        runFunction(ITM_SPACE);
        runFunction(ITM_D);
        runFunction(ITM_U);
        runFunction(ITM_P);
        runFunction(ITM_SPACE);
        runFunction(ITM_ASTERISK);
        runFunction(ITM_SPACE);
        runFunction(ITM_SEMICOLON);
        runFunction(ITM_ENTER);        /* commit line 1, line 2 stays open */
        runFunction(ITM_3);
        runFunction(ITM_SPACE);
        runFunction(ITM_S);
        runFunction(ITM_Q);
        fnKeyExit(NOPARAM);            /* commit-and-close */

        if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) {
          printf("    [1] FAIL: capture still open after EXIT\n");
          sc1 = 1;
        }
      }
      if (!sc1) {
        /* Byte-image assertions BEFORE running: walked structurally (no
         * hardcoded offsets) from the LBL step. Checked here, ahead of
         * fnExecute, because running rewrites the "3 SQ" call site (a
         * name -> label/GTO resolution, out of this subcase's scope) —
         * the packet's len-12/len-4 encoding assertion describes the
         * AUTHORED source text, not its post-run form. */
        uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
        uint8_t *sDef1 = sMarker ? findNextStep(sMarker) : NULL;
        uint8_t *sDef2 = sDef1 ? findNextStep(sDef1) : NULL;
        uint8_t *sClose = sDef2 ? findNextStep(sDef2) : NULL;
        if (!sMarker || !sDef1 || !sDef2 || !sClose) {
          printf("    [1] FAIL: structural walk from LBL came up short\n");
          sc1 = 1;
        }
        else if (sMarker[0] != 0x8B || sMarker[1] != 0x1A || sMarker[2] != 0xFD || sMarker[3] != 0x00) {
          printf("    [1] FAIL: opening marker image wrong\n");
          sc1 = 1;
        }
        else if (sDef1[0] != 0x8B || sDef1[1] != 0x1A || sDef1[2] != 0xFD || sDef1[3] != 12) {
          printf("    [1] FAIL: def1 header wrong (len=%u, expected 12)\n", sDef1[3]);
          sc1 = 1;
        }
        else if (memcmp(sDef1 + 4, ": SQ DUP * ;", 12) != 0) {
          printf("    [1] FAIL: def1 payload mismatch\n");
          sc1 = 1;
        }
        else if (sDef2[0] != 0x8B || sDef2[1] != 0x1A || sDef2[2] != 0xFD || sDef2[3] != 4) {
          printf("    [1] FAIL: def2 header wrong (len=%u, expected 4)\n", sDef2[3]);
          sc1 = 1;
        }
        else if (memcmp(sDef2 + 4, "3 SQ", 4) != 0) {
          printf("    [1] FAIL: def2 payload mismatch\n");
          sc1 = 1;
        }
        else if (sClose[0] != 0x8B || sClose[1] != 0x1A ||
                 sClose[2] != 0xFD || sClose[3] != 0) {
          printf("    [1] FAIL: automatic closing marker missing after explicit second line\n");
          sc1 = 1;
        }
      }
      if (!sc1) {
        dynamicMenuItem = -1;
        /* No rescan here: labelList was already scanned once at
         * tpWrite() time and the LBL step's own address never moves —
         * only content AFTER it changes (marker/typing), never
         * relocating beginOfProgramMemory itself for a growth this
         * small. An explicit scanLabelsAndPrograms() rescan here is
         * simply redundant work; findNamedLabel() below resolves
         * correctly without it. (An earlier hypothesis blamed this
         * rescan for the suite's +1 numberOfAllocatedMemoryRegions
         * leak — disproven by A/B test; the real cause was subcase 4
         * Phase 0's missing forthDictClear() hygiene call, fixed at
         * that call site.) */
        calcRegister_t lbl = findNamedLabel("F66", GLOBAL_LABELS);
        if (lbl == INVALID_VARIABLE) {
          printf("    [1] FAIL: findNamedLabel(F66) failed\n");
          sc1 = 1;
        } else {
          programRunStop = PGM_STOPPED;
          lastErrorCode = ERROR_NONE;
          fnExecute(lbl);
          if (lastErrorCode != ERROR_NONE) {
            printf("    [1] FAIL: run error %d\n", lastErrorCode);
            sc1 = 1;
          }
          else if (!x_is_longint(9)) {
            printf("    [1] FAIL: X != 9 after run\n");
            sc1 = 1;
          }
        }
      }
      if (!sc1) printf("    [1] PASS: toggle-open, two-line capture, EXIT, and label run yield 9\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc1;
    }

    /* ---- Subcase 2: EXIT ladder walk ---- */
    { int sc2 = 0;
      if (!fail) {
        /* Reopen ON the first existing source step (def1) via EDIT — the
         * landed F6-1 subcase-6 reopen mechanism (pemAlpha(ITM_EDIT)),
         * which refills the buffer from the step. ITM_AIM on an
         * already-committed, non-empty step opens plain alpha instead
         * (it is the "start a fresh line" gesture, not "re-edit this
         * one") — confirmed empirically, not the right drive here. */
        fnGotoDot(3);
        if (currentLocalStepNumber != 3) {
          printf("    [2] FAIL: fnGotoDot(3) landed on step %u\n", currentLocalStepNumber);
          sc2 = 1;
        }
        else {
          calcMode = CM_PEM;
          tam.mode = 0;
          clearSystemFlag(FLAG_ALPHA);
          tam.function = 0;
          pemAlpha(ITM_EDIT);
          if (!forthCapIsOpen() || !forthCapTextNonEmpty()) {
            printf("    [2] FAIL: reopen did not yield an open, non-empty capture\n");
            sc2 = 1;
          }
        }
      }
      if (!fail && !sc2) {
        testInitVariableSoftmenu(22);
        showSoftmenu(-MNU_FORTH);
        if (currentMenu() != -MNU_FORTH) {
          printf("    [2] FAIL: picker not on top after push\n");
          sc2 = 1;
        }
      }
      if (!fail && !sc2) {
        char textBefore[64];
        xcopy(textBefore, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);

        fnKeyExit(NOPARAM);   /* #1: pop the picker only */
        if (currentMenu() == -MNU_FORTH) {
          printf("    [2] FAIL: picker still on top after EXIT #1\n");
          sc2 = 1;
        }
        else if (forthTestCapState() != FCAP_OPEN) {
          printf("    [2] FAIL: capture not open after EXIT #1 (state=%d)\n", forthTestCapState());
          sc2 = 1;
        }
        else if (strcmp(forthTestCapText(), textBefore) != 0) {
          printf("    [2] FAIL: text changed after EXIT #1\n");
          sc2 = 1;
        }
      }
      if (!fail && !sc2) {
        fnKeyExit(NOPARAM);   /* #2: commit-with-text */
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [2] FAIL: capture not closed after EXIT #2 (state=%d)\n", forthTestCapState());
          sc2 = 1;
        }
      }
      if (!fail && !sc2) {
        uint16_t stepsBefore = getNumberOfSteps();
        fnGotoDot(2);
        runFunction(ITM_AIM);   /* fresh empty line, on the marker */
        fnKeyExit(NOPARAM);     /* abort-when-empty */
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [2] FAIL: capture not closed after abort (state=%d)\n", forthTestCapState());
          sc2 = 1;
        }
        else if (getNumberOfSteps() != stepsBefore) {
          printf("    [2] FAIL: step count changed %u -> %u (placeholder not deleted)\n",
                 stepsBefore, getNumberOfSteps());
          sc2 = 1;
        }
      }
      if (!fail && !sc2) printf("    [2] PASS: EXIT ladder — picker pop, commit-with-text, abort-when-empty\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc2;
    }

    /* ---- Subcase 3: Automatic marker-pair rules ---- */
    { int sc3 = 0;
      if (!fail) {
        fnGotoDot(2);   /* opening marker; subcase 2's abort kept the bracket */
        if (currentLocalStepNumber != 2) {
          printf("    [3] FAIL: fnGotoDot(2) landed on step %u\n", currentLocalStepNumber);
          sc3 = 1;
        }
        else if (!forthEntryStateAtCursor()) {
          printf("    [3] FAIL: cursor on the opening marker does not read Forth-side\n");
          sc3 = 1;
        }
        else {
          int markerCount = 0;
          uint8_t *closingMarker = NULL;
          uint8_t *walk = tpStepAddr(&p, sLbl);
          while (walk) {
            if (walk[0] == 0x8B && walk[1] == 0x1A &&
                walk[2] == 0xFD && walk[3] == 0x00) {
              markerCount++;
              closingMarker = walk;
            }
            uint8_t *next = findNextStep(walk);
            if (!next || next <= walk) break;
            walk = next;
          }
          if (markerCount != 2 || !closingMarker) {
            printf("    [3] FAIL: automatic marker occurrences = %d, expected 2\n",
                   markerCount);
            sc3 = 1;
          }
          else {
            currentStep = closingMarker;
            pemCursorIsZerothStep = false;
            if (forthEntryStateAtCursor()) {
              printf("    [3] FAIL: automatic closing marker does not restore RPN state\n");
              sc3 = 1;
            }
          }
        }
      }
      if (!fail && !sc3) printf("    [3] PASS: abort keeps the automatic balanced region; close restores RPN\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc3;
    }

    /* ---- Subcase 4: Restore seam closes capture (differential) ---- */
    { int sc4 = 0;
      if (!fail) {
        uint32_t freeBase;

        /* Fresh, minimal fixture for this subcase (F6-4 subcase-4
         * precedent): subcases 1-3's F66 program has been through
         * several insert/delete/toggle cycles, and program memory's
         * block-level allocation never shrinks back down after a
         * delete (the F6-2/F6-4 escape-valve mechanism) — that slack
         * is where an independently-allocated capture buffer can end
         * up sitting. A restoreCalc() resize of THAT program then
         * legitimately reclaims blocks the capture buffer also
         * occupies, and the double-free guard (correctly) rejects the
         * second free — orphaning the buffer for the rest of the run.
         * A fresh fixture with no resize history avoids the conflict;
         * it does not touch the seam being tested. */
        testProg_t p4;
        tpInit(&p4);
        tpLbl(&p4, "F66B");
        tpMarker(&p4);
        if (!tpWrite(&p4)) {
          printf("    [4] FAIL: fixture rebuild (tpWrite)\n");
          sc4 = 1;
        }

        if (!sc4) {
          calcMode = CM_PEM;
          catalog = CATALOG_NONE;
          tam.mode = 0;
          tam.function = 0;
          aimBuffer[0] = 0;
          programRunStop = PGM_STOPPED;
          dynamicMenuItem = -1;
          pemCursorIsZerothStep = false;
          clearSystemFlag(FLAG_ALPHA);
          lastErrorCode = ERROR_NONE;
          forthCapClose();

          /* Phase 0: establish freeBase via the SAME mechanism phase 1
           * measures against below (packet correction, traced during
           * F6-6 authoring: a real saveCalc()/restoreCalc() round-trip
           * here — the packet's literal "F15-2 power-off round-trip
           * idiom" — restores numberOfAllocatedMemoryRegions/
           * allocatedMemoryRegions wholesale from the backup file
           * (saveRestoreBackup.c) independently of anything Forth- or
           * capture-related, and was independently confirmed, by
           * temporarily disabling it, to be the sole source of a
           * +1-region discrepancy even with NO capture ever open in
           * this phase. A second pre-existing saveCalc/restoreCalc
           * issue, distinct from phase 1's — logged for the forth-core
           * code audit alongside it. Establishing freeBase this way
           * instead keeps the comparison meaningful: both sides of the
           * phase 1 equality check now go through the identical
           * forthGDictValidateRestored()/forthDictInit() path). */
          forthDictClear();   /* hygiene: fdict may hold SQ from subcase 1 —
                                * forthDictInit() below nulls fdict.base
                                * without freeing, so a live allocation left
                                * here would leak silently (F6-6 authoring:
                                * this was the suite's actual +1
                                * numberOfAllocatedMemoryRegions source). */
          forthGDictValidateRestored();
          forthDictInit();
          freeBase = getFreeRamMemory();

          /* Phase 1: open capture, type "4 4", drive the restore-
           * validation path directly (forthGDictValidateRestored() +
           * forthDictInit(), mirroring saveRestoreBackup.c's own call
           * pair — the same idiom phase 2 below uses for the suspended
           * case) rather than a full saveCalc()/restoreCalc() file
           * round-trip.
           *
           * Packet correction, discovered and traced during F6-6
           * authoring: restoreCalc() restores numberOfFreeMemoryRegions/
           * freeMemoryRegions/numberOfAllocatedMemoryRegions/
           * allocatedMemoryRegions WHOLESALE from the backup file
           * (saveRestoreBackup.c) before this seam ever runs. With a
           * capture genuinely OPEN at save time — the scenario this
           * phase exists to test; no earlier F-series test ever
           * exercised it — the round-trip leaves the arena in a state
           * where this seam's forthCapClose() free is REJECTED by the
           * double-free guard (freeListFree "Memory freeing C", traced
           * to this exact call site), orphaning one capture buffer's
           * worth of blocks. Reproduced identically across three
           * independent variations (subcases 1-3's edited program,
           * this subcase's fresh fixture, and a pre-inflated program
           * memory footprint) with the byte delta varying between
           * attempts (256-272 B) — a real, deterministic, pre-existing
           * arena/restore interaction, not fixture fragmentation or a
           * bounded quantum this stage's established escape-valve
           * pattern can honestly cover. A full architectural fix is
           * out of scope here ("No other product changes" — Authority)
           * and unsafe to improvise (the orphaned block cannot be
           * safely re-freed without risking a double-free against
           * whatever the arena allocates into that address range
           * next). Logged for the forth-core code audit. This
           * subcase's actual subject — the lifecycle-reset seam
           * closing an OPEN (not just suspended) capture — is still
           * fully exercised and pinned below, just via the same
           * direct-call drive already proven safe for phase 2. */
          forthDictClear();   /* hygiene BEFORE opening: fdict may hold SQ from
                                * subcase 1, and forthDictClear() also runs this
                                * seam — done here, pre-open, so it cannot
                                * prematurely close the capture this phase is
                                * about to open and is actually testing. */
          fnGotoDot(2);
          runFunction(ITM_AIM);
          runFunction(ITM_4);
          runFunction(ITM_SPACE);
          runFunction(ITM_4);
          if (!forthCapIsOpen()) {
            printf("    [4] FAIL: phase 1 capture did not open\n");
            sc4 = 1;
          }
          else {
            forthGDictValidateRestored();   /* mirrors saveRestoreBackup.c's restore call */
            forthDictInit();                /* mirrors saveRestoreBackup.c's restore call — the reset seam */

            if (forthTestCapState() != FCAP_CLOSED) {
              printf("    [4] FAIL: phase 1 capture not closed after restore (state=%d)\n",
                     forthTestCapState());
              sc4 = 1;
            }
            else if (getFreeRamMemory() != freeBase) {
              printf("    [4] FAIL: phase 1 freeRam %u != freeBase %u\n",
                     (unsigned)getFreeRamMemory(), (unsigned)freeBase);
              sc4 = 1;
            }
            else {
              int found44 = 0;
              uint8_t *walk = tpStepAddr(&p4, 0);
              while (walk) {
                if (checkOpCodeOfStep(walk, ITM_FORTH) && walk[2] == (uint8_t)STRING_LABEL_VARIABLE &&
                    walk[3] == 3 && walk[4] == '4' && walk[5] == ' ' && walk[6] == '4') {
                  found44 = 1;
                }
                uint8_t *next = findNextStep(walk);
                if (!next || next <= walk) break;
                walk = next;
              }
              if (!found44) {
                printf("    [4] FAIL: '4 4' source step not found after restore\n");
                sc4 = 1;
              }
            }
          }
        }

        /* Phase 2: suspended state. Fresh fixture — phase 1's marker
         * now has a committed "4 4" step immediately after it, and
         * every earlier F6 subcase's fnGotoDot(marker)+AIM only ever
         * ran against a marker with nothing (meaningful) following it;
         * a clean marker keeps this phase's drive unambiguous rather
         * than probing that untested combination. */
        if (!sc4) {
          /* writeTestProgram() (tpWrite's implementation) records only
           * ONE "original state" snapshot, overwritten on every call —
           * reconcile phase 1's "p4" snapshot via a real restore before
           * building a second fixture, so this fixture's own block-
           * level footprint doesn't end up orphaned the same way
           * behind whatever this one saves next. */
          cleanupTestProgram();

          testProg_t p4b;
          tpInit(&p4b);
          tpLbl(&p4b, "F66C");
          tpMarker(&p4b);
          if (!tpWrite(&p4b)) {
            printf("    [4] FAIL: phase 2 fixture rebuild (tpWrite)\n");
            sc4 = 1;
          }
        }
        if (!sc4) {
          calcMode = CM_PEM;
          catalog = CATALOG_NONE;
          tam.mode = 0;
          tam.function = 0;
          aimBuffer[0] = 0;
          programRunStop = PGM_STOPPED;
          dynamicMenuItem = -1;
          pemCursorIsZerothStep = false;
          clearSystemFlag(FLAG_ALPHA);
          lastErrorCode = ERROR_NONE;
          forthCapClose();

          fnGotoDot(2);
          runFunction(ITM_AIM);
          runFunction(ITM_5);
          runFunction(ITM_STO);    /* suspends — buffer already freed */
          if (!forthCapIsSuspended()) {
            printf("    [4] FAIL: phase 2 capture not suspended before drive\n");
            sc4 = 1;
          }
          else {
            forthDictClear();               /* hygiene: fdict may hold SQ from subcase 1 */
            forthGDictValidateRestored();   /* mirrors saveRestoreBackup.c's restore call */
            forthDictInit();                /* mirrors saveRestoreBackup.c's restore call — the reset seam */

            if (forthTestCapState() != FCAP_CLOSED) {
              printf("    [4] FAIL: phase 2 capture not closed under TAM (state=%d)\n",
                     forthTestCapState());
              sc4 = 1;
            }
            else if (tam.mode == 0) {
              printf("    [4] FAIL: phase 2 tam.mode unexpectedly 0\n");
              sc4 = 1;
            }
            else {
              fnKeyExit(NOPARAM);   /* cancel TAM */
              if (tam.mode != 0) {
                printf("    [4] FAIL: tam.mode = %d after cancel, expected 0\n", (int)tam.mode);
                sc4 = 1;
              }
            }
          }
        }
      }
      if (!fail && !sc4) printf("    [4] PASS: restore lifecycle closes open and suspended captures leak-free\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc4;
    }

    /* ---- Subcase 5: Cap round-trip ---- */
    { int sc5 = 0;
      if (!fail) {
        fnGotoDot(2);   /* opening marker: fresh, empty line */
        runFunction(ITM_AIM);
        if (!forthCapIsOpen()) {
          printf("    [5] FAIL: fresh AIM did not open capture\n");
          sc5 = 1;
        }
        else {
          int i;
          for (i = 0; i < 98; i++) {
            runFunction(ITM_X);
            runFunction(ITM_SPACE);
          }
          if (stringGlyphLength(forthTestCapText()) != 196) {
            printf("    [5] FAIL: glyph count = %d, expected 196\n",
                   stringGlyphLength(forthTestCapText()));
            sc5 = 1;
          }
        }
      }
      if (!fail && !sc5) {
        char committed[258];
        xcopy(committed, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);

        runFunction(ITM_ENTER);       /* commit; a fresh relock line opens */
        runFunction(ITM_BACKSPACE);   /* relock line is already empty: abort it */

        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [5] FAIL: capture not closed after relock-line abort (state=%d)\n",
                 forthTestCapState());
          sc5 = 1;
        }
        else {
          /* Step back onto the committed 196-glyph step (findPreviousStep,
           * the F6-1 subcase-6 pointer-walk idiom — no hardcoded
           * fnGotoDot(N), which would need an absolute count across
           * four prior subcases' mutations). */
          currentStep = findPreviousStep(currentStep);
          --currentLocalStepNumber;
          calcMode = CM_PEM;
          tam.mode = 0;
          clearSystemFlag(FLAG_ALPHA);
          tam.function = 0;
          pemAlpha(ITM_EDIT);   /* landed F6-1 subcase-6 reopen mechanism
                                  * (the packet's own "pemAlphaEdit(0)" name
                                  * does not exist in the tree) */
          if (forthTestCapState() != FCAP_OPEN) {
            printf("    [5] FAIL: capture not open after EDIT\n");
            sc5 = 1;
          }
          else if (strcmp(forthTestCapText(), committed) != 0) {
            printf("    [5] FAIL: reopened text != committed payload\n");
            sc5 = 1;
          }
          else if (stringGlyphLength(forthTestCapText()) != 196) {
            printf("    [5] FAIL: reopened glyph length = %d, expected 196\n",
                   stringGlyphLength(forthTestCapText()));
            sc5 = 1;
          }
          runFunction(ITM_CLA);
          runFunction(ITM_BACKSPACE);
        }
      }
      if (!fail && !sc5) printf("    [5] PASS: 196-glyph line round-trips commit and reopen\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc5;
    }

    /* ---- Subcase 6: Arena sweep ---- */
    { int sc6 = 0;
      if (!fail) {
        uint32_t freeBeforeCycle = getFreeRamMemory();
        int cycle;
        for (cycle = 0; cycle < 3 && !sc6; cycle++) {
          fnGotoDot(2);
          runFunction(ITM_AIM);
          runFunction(ITM_1);
          runFunction(ITM_STO);
          if (forthTestCapState() != FCAP_SUSPENDED) {
            printf("    [6] FAIL: cycle %d not suspended (state=%d)\n", cycle, forthTestCapState());
            sc6 = 1;
            break;
          }
          fnKeyExit(NOPARAM);   /* cancel-resume */
          if (forthTestCapState() != FCAP_OPEN) {
            printf("    [6] FAIL: cycle %d not resumed (state=%d)\n", cycle, forthTestCapState());
            sc6 = 1;
            break;
          }
          /* BACKSPACE-abort: text is a single '1', so one BACKSPACE
           * empties it and a second (on the now-empty line) aborts. */
          runFunction(ITM_BACKSPACE);
          runFunction(ITM_BACKSPACE);
          if (forthTestCapState() != FCAP_CLOSED) {
            printf("    [6] FAIL: cycle %d not closed after abort (state=%d)\n", cycle, forthTestCapState());
            sc6 = 1;
            break;
          }
        }
        if (!sc6) {
          uint32_t afterCycles = getFreeRamMemory();
          if (afterCycles != freeBeforeCycle) {
            uint32_t delta = (freeBeforeCycle > afterCycles) ? (freeBeforeCycle - afterCycles)
                                                              : (afterCycles - freeBeforeCycle);
            if (delta == BYTES_PER_BLOCK && freeBeforeCycle > afterCycles) {
              printf("    [SOL DEBUGGER HANDOFF] subcase 6: freeRam %u -> %u is exactly one"
                     " program-memory resize quantum (%u B) after three arena-sweep cycles —"
                     " packet-anticipated program-region growth, not a capture leak; STOP and"
                     " report per Authority rather than silently widening the tolerance.\n",
                     (unsigned)freeBeforeCycle, (unsigned)afterCycles, (unsigned)BYTES_PER_BLOCK);
              sc6 = 1;
            } else {
              printf("    [6] FAIL: freeRam changed %u -> %u\n",
                     (unsigned)freeBeforeCycle, (unsigned)afterCycles);
              sc6 = 1;
            }
          }
        }
      }
      if (!fail && !sc6) printf("    [6] PASS: capture cycles leave zero arena residue\n");
      lastErrorCode = ERROR_NONE;
      fail |= sc6;
    }
  }

  /* Cleanup */
  forthCapClose();
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
    dynamicSoftmenu[22].menuContent = NULL;
    dynamicSoftmenu[22].numItems = 0;
  }
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();

  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  lastErrorCode = ERROR_NONE;

  return fail;
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
/* test_forth_edit_modify_commit  (adversarial code-audit reproduction)
 * The existing capture tests DEFINE a Forth line via keystrokes, and reopen
 * an existing line via EDIT (subcase 6 / acceptance subcase 2) — but none
 * EDIT an existing line, MODIFY the text, and re-commit it via ENTER through
 * the E9 check gate.  That is exactly the "edit a forth line" gesture.  This
 * drives it end to end and asserts no spurious error and a correct rewrite. */
static int test_forth_edit_modify_commit(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void addStepInProgram(int16_t func);

  int fail = 0;

  uint8_t *savedCurrentStep = currentStep;
  bool_t   savedZeroth      = pemCursorIsZerothStep;
  uint16_t savedLocalStep   = currentLocalStepNumber;
  uint16_t savedProgNum     = currentProgramNumber;
  bool_t   savedAlpha       = getSystemFlag(FLAG_ALPHA);
  uint8_t  savedCalcMode    = calcMode;
  int16_t  savedTamFunction = tam.function;
  int16_t  savedTamMode     = tam.mode;
  uint8_t  savedProgRunStop = programRunStop;
  int16_t  savedDynamicMenu = dynamicMenuItem;

  /* Fresh program: LBL only; the toggle inserts the opening marker. */
  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "EDT");
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  currentProgramNumber = 1;

  fnGotoDot(1);

  /* Build: LBL | <<marker>> | ": SQ DUP * ;" (def1) | "3 SQ" (def2) */
  addStepInProgram(ITM_FORTH);
  if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH || !forthCapIsOpen()) {
    printf("    FAIL: toggle-open did not open Forth capture\n");
    fail = 1;
    goto done;
  }
  { const char *l1 = ": SQ DUP * ;";
    int16_t keys1[] = { ITM_COLON, ITM_SPACE, ITM_S, ITM_Q, ITM_SPACE, ITM_D,
                        ITM_U, ITM_P, ITM_SPACE, ITM_ASTERISK, ITM_SPACE, ITM_SEMICOLON };
    (void)l1;
    for (unsigned i = 0; i < sizeof(keys1)/sizeof(keys1[0]); i++) runFunction(keys1[i]);
  }
  runFunction(ITM_ENTER);          /* commit line 1, line 2 stays open */
  runFunction(ITM_3);
  runFunction(ITM_SPACE);
  runFunction(ITM_S);
  runFunction(ITM_Q);
  fnKeyExit(NOPARAM);              /* commit-and-close */
  if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) {
    printf("    FAIL: capture still open after initial build EXIT\n");
    fail = 1;
    goto done;
  }
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: error %d after initial build\n", lastErrorCode);
    fail = 1;
    goto done;
  }

  /* ---- Scenario A: EDIT def1, ENTER unchanged -> must not error ---- */
  { int sc = 0;
    calcMode = CM_PEM; tam.mode = 0; tam.function = 0;
    clearSystemFlag(FLAG_ALPHA);
    lastErrorCode = ERROR_NONE;
    fnGotoDot(3);                  /* def1 = ": SQ DUP * ;" */
    runFunction(ITM_EDIT);
    if (!forthCapIsOpen()) {
      printf("    [A] FAIL: EDIT did not open capture\n"); sc = 1;
    } else if (strcmp(forthTestCapText(), ": SQ DUP * ;") != 0) {
      printf("    [A] FAIL: EDIT text = '%s'\n", forthTestCapText()); sc = 1;
    } else {
      runFunction(ITM_ENTER);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [A] FAIL: ENTER on unchanged line raised error %d\n", lastErrorCode);
        sc = 1;
      }
    }
    /* ENTER inside a region drops to the next Forth line and reopens; close it. */
    if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) { runFunction(ITM_CLA); runFunction(ITM_BACKSPACE); }
    if (!sc) printf("    [A] PASS: edit + ENTER unchanged does not error\n");
    lastErrorCode = ERROR_NONE;
    fail |= sc;
  }

  /* ---- Scenario B: EDIT def1, LEFT twice, INSERT SPACE, ENTER -> rewrites ---- */
  if (!fail) { int sc = 0;
    calcMode = CM_PEM; tam.mode = 0; tam.function = 0;
    clearSystemFlag(FLAG_ALPHA);
    lastErrorCode = ERROR_NONE;
    fnGotoDot(3);
    runFunction(ITM_EDIT);
    if (!forthCapIsOpen()) {
      printf("    [B] FAIL: EDIT did not open capture\n"); sc = 1;
    } else {
      runFunction(ITM_T_LEFT_ARROW);
      runFunction(ITM_T_LEFT_ARROW);
      if (T_cursorPos != stringByteLength(": SQ DUP * ;") - 2) {
        printf("    [B] FAIL: two LEFT presses left cursor at %u\n", T_cursorPos);
        sc = 1;
      } else {
        runFunction(ITM_SPACE);
      }
      if (!sc && lastErrorCode != ERROR_NONE) {
        printf("    [B] FAIL: typing raised error %d\n", lastErrorCode); sc = 1;
      } else if (!sc && strcmp(forthTestCapText(), ": SQ DUP *  ;") != 0) {
        printf("    [B] FAIL: modified text = '%s'\n", forthTestCapText()); sc = 1;
      } else if (!sc) {
        runFunction(ITM_ENTER);
        if (lastErrorCode != ERROR_NONE) {
          printf("    [B] FAIL: ENTER on modified line raised error %d\n", lastErrorCode);
          sc = 1;
        }
      }
    }
    if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) { runFunction(ITM_CLA); runFunction(ITM_BACKSPACE); }
    /* Verify the on-disk step really carries the new text. */
    if (!sc) {
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sDef1   = sMarker ? findNextStep(sMarker) : NULL;
      if (!sDef1 || sDef1[0] != 0x8B || sDef1[1] != 0x1A || sDef1[2] != 0xFD ||
          sDef1[3] != 13 || memcmp(sDef1 + 4, ": SQ DUP *  ;", 13) != 0) {
        printf("    [B] FAIL: def1 not rewritten (len=%u)\n", sDef1 ? sDef1[3] : 0);
        sc = 1;
      }
    }
    if (!sc) printf("    [B] PASS: edit + two LEFT + mid-line insert + ENTER rewrites\n");
    lastErrorCode = ERROR_NONE;
    fail |= sc;
  }

done:
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* A Forth capture lives in forthCapBuf(), while the inherited PEM
 * navigation paths historically decided whether to close/commit alpha input
 * by looking only at aimBuffer.  On a non-scrolling alpha submenu, Up or
 * Down would therefore backspace the managed line, navigate with capture
 * still open, and leave later edits aimed at a different program step.
 * Exercise both key paths and require the complete text to be committed
 * before navigation. */
static int test_forth_capture_navigation(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyUp(uint16_t);
  extern void fnKeyDown(uint16_t);
  extern void pemAlpha(int16_t);
  extern void showSoftmenu(int16_t);

  int fail = 0;
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedAlphaCase = alphaCase;
  int16_t savedForthMenuItems = dynamicSoftmenu[22].numItems;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "NAV");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  alphaCase = AC_UPPER;
  nextChar = NC_NORMAL;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  currentProgramNumber = 1;

  fnGotoDot(2);
  runFunction(ITM_AIM);
  if (!forthCapIsOpen()) {
    printf("    FIXTURE FAIL: ITM_AIM did not open capture\n");
    fail = 1;
  }

  /* Up from a non-scrolling alpha submenu must commit "7" intact. */
  if (!fail) {
    int sc1 = 0;
    runFunction(ITM_7);
    dynamicSoftmenu[22].numItems = 0;
    showSoftmenu(-MNU_FORTH);
    alphaCase = AC_UPPER;
    fnKeyUp(NOPARAM);

    uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
    uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
    if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) {
      printf("    [1] FAIL: Up navigated with capture still open\n");
      sc1 = 1;
    }
    else if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A ||
             sSource[2] != 0xFD || sSource[3] != 1 ||
             memcmp(sSource + 4, "7", 1) != 0) {
      printf("    [1] FAIL: Up did not commit the complete source line\n");
      sc1 = 1;
    }
    if (!sc1) {
      printf("    [1] PASS: Up commits managed Forth text before navigation\n");
    }
    fail |= sc1;
  }

  /* Re-edit that source; Down must commit the appended "8" intact. */
  if (!fail) {
    int sc2 = 0;
    fnGotoDot(3);
    calcMode = CM_PEM;
    tam.mode = 0;
    tam.function = 0;
    clearSystemFlag(FLAG_ALPHA);
    pemAlpha(ITM_EDIT);
    if (!forthCapIsOpen() || strcmp(forthTestCapText(), "7") != 0) {
      printf("    [2] FAIL: EDIT did not reopen source text \"7\"\n");
      sc2 = 1;
    }
    else {
      runFunction(ITM_8);
      dynamicSoftmenu[22].numItems = 0;
      showSoftmenu(-MNU_FORTH);
      alphaCase = AC_LOWER;
      fnKeyDown(NOPARAM);

      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) {
        printf("    [2] FAIL: Down navigated with capture still open\n");
        sc2 = 1;
      }
      else if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A ||
               sSource[2] != 0xFD || sSource[3] != 2 ||
               memcmp(sSource + 4, "78", 2) != 0) {
        printf("    [2] FAIL: Down did not commit the complete modified line\n");
        sc2 = 1;
      }
    }
    if (!sc2) {
      printf("    [2] PASS: Down commits managed Forth text before navigation\n");
    }
    fail |= sc2;
  }

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  alphaCase = savedAlphaCase;
  dynamicSoftmenu[22].numItems = savedForthMenuItems;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

static int test_pem_xeq_dynmenu_no_live_exec(void)
{
  extern void showSoftmenu(int16_t menu);

  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedCalcMode = calcMode;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  uint8_t *savedMenuContent = dynamicSoftmenu[22].menuContent;
  int16_t savedNumItems = dynamicSoftmenu[22].numItems;

  dynamicMenuItem = -1;
  forthDictClear();

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": W7 7 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FIXTURE FAIL: W7 def error %d\n", lastErrorCode);
    fail = 1;
  }

  testProg_t p;
  int sMarker = -1;
  if (!fail) {
    tpInit(&p);
    tpLbl(&p, "PXF");
    sMarker = tpMarker(&p);
    if (sMarker < 0 || !tpWrite(&p)) {
      printf("    FIXTURE FAIL: build/write\n");
      fail = 1;
    }
  }

  if (!fail) {
    currentProgramNumber = 1;
    currentStep = tpStepAddr(&p, sMarker);
    testInitVariableSoftmenu(22);
    showSoftmenu(-MNU_FORTH);

    if (dynamicSoftmenu[22].numItems < 1 || !dynamicSoftmenu[22].menuContent ||
        compareString((const char *)dynamicSoftmenu[22].menuContent, "W7", CMP_BINARY) != 0) {
      printf("    FIXTURE FAIL: picker content = '%s' (numItems=%d), expected \"W7\" first\n",
             dynamicSoftmenu[22].menuContent ? (const char *)dynamicSoftmenu[22].menuContent : "(null)",
             dynamicSoftmenu[22].numItems);
      fail = 1;
    }
  }

  if (!fail) {
    dynamicMenuItem = 0;   /* "W7" */
    calcMode = CM_PEM;
    programRunStop = PGM_STOPPED;
    lastErrorCode = ERROR_NONE;
    forthPushInt32(999);   /* sentinel: must survive if XEQ only records a step */

    uint16_t stepsBefore = getNumberOfSteps();
    runFunction(ITM_XEQ);

    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: runFunction(ITM_XEQ) errored %d\n", lastErrorCode);
      fail = 1;
    }
    else if (!x_is_longint(999)) {
      printf("    FAIL: X changed — word executed live instead of being recorded\n");
      fail = 1;
    }
    else if (getNumberOfSteps() != stepsBefore + 1) {
      printf("    FAIL: step count %u -> %u, expected +1 (no step recorded)\n",
             (unsigned)stepsBefore, (unsigned)getNumberOfSteps());
      fail = 1;
    }
  }

  if (!fail) {
    printf("    PASS: PEM XEQ of a Forth word from the dynamic menu records a step, does not execute\n");
  }

  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems = savedNumItems;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  calcMode = savedCalcMode;
  dynamicMenuItem = savedDynamicMenu;
  currentStep = savedCurrentStep;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  forthDictClear();
  cleanupTestProgram();
  return fail;
}

/* Exact simulator drive for FORTH_SHOWCASE_PROGRAM.txt. The fixture uses the
 * same program-step encodings as PEM and leaves its results in registers
 * 00..16 so the text example has observable outputs instead of only compiling. */
static int test_showcase_program(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint16_t savedNamedVars = numberOfNamedVariables;
  bool_t savedFlag10 = getFlag(10);
  testProg_t p;
  char indirectRegister[64];
  char indirectNamed[64];
  calcRegister_t lbl;
  int i;

  tpInit(&p);
  snprintf(indirectRegister, sizeof(indirectRegister),
           "18 STO 09 77 STO %s09 RCL 18 STO 10", STD_RIGHT_ARROW);
  snprintf(indirectNamed, sizeof(indirectNamed),
           "19 STO 'PTR' 88 STO %s'PTR' RCL 19 STO 11", STD_RIGHT_ARROW);

  if (tpLbl(&p, "FDEMO") < 0 ||
      tpMarker(&p) < 0 ||
      tpSrc(&p, ": SUMDOWN DUP IF DUP 1 - RECURSE + THEN ;") < 0 ||
      tpSrc(&p, ": COUNTDOWN BEGIN DUP WHILE 1 - REPEAT ;") < 0 ||
      tpSrc(&p, ": UNTIL1 BEGIN 1 - DUP UNTIL DROP ;") < 0 ||
      tpSrc(&p, ": CHOOSE IF 111 ELSE 222 THEN ;") < 0 ||
      tpSrc(&p, ": SPIN BEGIN AGAIN ;") < 0 ||
      tpSrc(&p, ": PLUS10 10 + ; GLOBAL") < 0 ||
      tpSrc(&p, ": GONE1 1 ; GLOBAL") < 0 ||
      tpSrc(&p, ": GONE2 2 ; GLOBAL") < 0 ||
      tpSrc(&p, ": IMM3 3 ; IMMEDIATE") < 0 ||
      tpSrc(&p, ": EMPTY IMM3 ;") < 0 ||
      tpSrc(&p, ": CALLWORD XEQ 'PLUS10' ;") < 0 ||
      tpSrc(&p, ": CLEAR XEQ 'CLSTK' ;") < 0 ||
      tpSrc(&p, "CLEAR") < 0 ||
      tpSrc(&p, "5 SUMDOWN STO 00") < 0 ||
      tpSrc(&p, "5 COUNTDOWN STO 01") < 0 ||
      tpSrc(&p, "99 1 UNTIL1 STO 02") < 0 ||
      tpSrc(&p, "1 CHOOSE STO 03") < 0 ||
      tpSrc(&p, "0 CHOOSE STO 04") < 0 ||
      tpSrc(&p, "7 CALLWORD STO 05") < 0 ||
      tpSrc(&p, "42 STO 'DEMO' RCL 'DEMO' STO 06") < 0 ||
      tpSrc(&p, "CNST 10") < 0 ||
      tpSrc(&p, "10 STO 07") < 0 ||
      tpSrc(&p, "SF 10") < 0 ||
      tpSrc(&p, indirectRegister) < 0 ||
      tpSrc(&p, indirectNamed) < 0 ||
      tpSrc(&p, "XEQ :GET0:") < 0 ||
      tpSrc(&p, "STO 12") < 0 ||
      tpSrc(&p, "2") < 0 ||
      tpSrc(&p, "XEQ 'NATADD'") < 0 ||
      tpSrc(&p, "STO 13") < 0 ||
      tpSrc(&p, "5 LATER STO 14") < 0 ||
      tpSrc(&p, "99 EMPTY STO 15") < 0 ||
      tpSrc(&p, "1 2 CLEAR 9 STO 16") < 0 ||
      tpSrc(&p, "FORGET GONE1") < 0 ||
      tpSrc(&p, ": LATER 2 * ;") < 0 ||
      tpMarker(&p) < 0 ||
      tpRtn(&p) < 0 ||
      tpLblLocal(&p, "GET0") < 0 ||
      tpStepParam(&p, ITM_RCL, (uint8_t[]){0}, 1) < 0 ||
      tpRtn(&p) < 0 ||
      tpEnd(&p) < 0 ||
      tpLbl(&p, "NATADD") < 0 ||
      tpMarker(&p) < 0 ||
      tpSrc(&p, "100 +") < 0 ||
      tpMarker(&p) < 0 ||
      tpRtn(&p) < 0 ||
      tpEnd(&p) < 0 ||
      !tpWrite(&p)) {
    printf("    FIXTURE FAIL: showcase program build/write\n");
    fail = 1;
    goto cleanup;
  }

  lbl = findNamedLabel("FDEMO", GLOBAL_LABELS);
  if (lbl == INVALID_VARIABLE) {
    printf("    FIXTURE FAIL: FDEMO label not found\n");
    fail = 1;
    goto cleanup;
  }

  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE;
  fnExecute(lbl);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: FDEMO run error %d\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }

  {
    static const struct {
      uint8_t reg;
      int32_t expected;
    } results[] = {
      {0, 15}, {1, 0}, {2, 99}, {3, 111}, {4, 222},
      {5, 17}, {6, 42}, {7, 10}, {9, 18}, {10, 77}, {11, 88},
      {12, 15}, {13, 102}, {14, 10}, {15, 99}, {16, 9}
    };
    for (i = 0; i < (int)(sizeof(results) / sizeof(results[0])); i++) {
      char rcl[16];
      snprintf(rcl, sizeof(rcl), "RCL %02u", results[i].reg);
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(rcl);
      if (lastErrorCode != ERROR_NONE || !x_is_longint(results[i].expected)) {
        int32_t actual = 0;
        uint16_t actualType = getRegisterDataType(REGISTER_X);
        if (actualType == dtLongInteger) {
          longInteger_t li;
          longIntegerInit(li);
          convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
          longIntegerToInt32(li, actual);
          longIntegerFree(li);
        }
        printf("    FAIL: R%02u expected %ld, got %ld type %u (error %d)\n",
               results[i].reg, (long)results[i].expected, (long)actual,
               actualType, lastErrorCode);
        fail = 1;
        break;
      }
    }
  }

  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("RCL 'DEMO'");
    if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
      printf("    FAIL: named variable DEMO expected 42 (error %d)\n",
             lastErrorCode);
      fail = 1;
    }
  }
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("RCL 'PTR'");
    if (lastErrorCode != ERROR_NONE || !x_is_longint(19)) {
      printf("    FAIL: named variable PTR expected 19 (error %d)\n",
             lastErrorCode);
      fail = 1;
    }
  }
  if (!fail && !getFlag(10)) {
    printf("    FAIL: flag 10 was not set\n");
    fail = 1;
  }

  if (!fail) {
    uint16_t ref;
    if (!forthFindColon("PLUS10", &ref) || !(ref & FORTH_REF_GLOBAL)) {
      printf("    FAIL: PLUS10 is not global\n");
      fail = 1;
    } else if (forthFindColon("GONE1", &ref) || forthFindColon("GONE2", &ref)) {
      printf("    FAIL: FORGET GONE1 left GONE1 or GONE2 visible\n");
      fail = 1;
    } else {
      forthPushInt32(5);
      lastErrorCode = ERROR_NONE;
      if (run_word("PLUS10") || !x_is_longint(15)) {
        printf("    FAIL: global PLUS10 did not leave X=15\n");
        fail = 1;
      }
    }
  }

  if (!fail) {
    printf("    PASS: FDEMO produced every documented register, flag, named-variable, scope, XEQ, control-flow, and FORGET result\n");
  }

cleanup:
  if (numberOfNamedVariables > savedNamedVars) {
    uint16_t n;
    for (n = savedNamedVars; n < numberOfNamedVariables; n++) {
      freeRegisterData(FIRST_NAMED_VARIABLE + n);
    }
    if (savedNamedVars == 0) {
      freeC47Blocks(allNamedVariables,
                    TO_BLOCKS(sizeof(namedVariableHeader_t) * numberOfNamedVariables));
      allNamedVariables = NULL;
    } else {
      allNamedVariables = reallocC47Blocks(
          allNamedVariables,
          TO_BLOCKS(sizeof(namedVariableHeader_t) * numberOfNamedVariables),
          TO_BLOCKS(sizeof(namedVariableHeader_t) * savedNamedVars));
    }
    numberOfNamedVariables = savedNamedVars;
  }
  if (savedFlag10) {
    fnSetFlag(10);
  } else {
    fnClearFlag(10);
  }
  programRunStop = savedRS;
  dynamicMenuItem = savedDynamicMenu;
  lastErrorCode = ERROR_NONE;
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();
  return fail;
}
#endif  // PC_BUILD && FORTH_DEBUG_SELFTEST
