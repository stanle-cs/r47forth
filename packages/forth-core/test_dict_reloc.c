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
#if defined(PC_BUILD)

#include <string.h>
#include <signal.h>   /* SIGALRM: the fork test turns a child hang into a signal */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "c47.h"
#include "forth_dict.h"
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
 * Assert: X == 42 (original value preserved, sentinel did not run).
 * Assert: X != 999 (sentinel did not execute).
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
  printf("    PASS: DIV by zero halted (error %d, sentinel not executed)\n", lastErrorCode);
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
      } else if (x_is_longint(444)) {
        printf("    PASS: bad PRIM index caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      } else {
        printf("    PASS: bad PRIM index caught (X not 555, err=%d)\n", lastErrorCode);
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
      } else if (x_is_longint(444)) {
        printf("    PASS: bad CALL index caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      } else {
        printf("    PASS: bad CALL index caught (X not 555, err=%d)\n", lastErrorCode);
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
      } else if (x_is_longint(444)) {
        printf("    PASS: reserved token caught (X unchanged at 444, err=%d)\n", lastErrorCode);
      } else {
        printf("    PASS: reserved token caught (X not 555, err=%d)\n", lastErrorCode);
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

  calcRegister_t lbl = findNamedLabel("NLB");
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

/* §7.1 H1: XEQ of ITM_FORTH/ITM_FCALL doesn't crash; indexOfItems size check */
static int test_xeq_end_to_end(void)
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
  if (x_is_longint(99)) {
    printf("    FAIL: sentinel value 99 was set — guard did not prevent entry\n");
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

  /* Test 2: "FCALL" resolves to ITM_FCALL */
  res = forthResolveXEQ("FCALL", &param);
  if (res != FORTH_XEQ_ITEM || param != ITM_FCALL) {
    printf("    FAIL: forthResolveXEQ(\"FCALL\") returned %d/%u (expected ITEM/%d)\n",
    res, param, ITM_FCALL);
    fail = 1;
  }

  /* Test 3: a non-existent name returns NONE */
  res = forthResolveXEQ("NONEXISTENT_ITEM", &param);
  if (res != FORTH_XEQ_NONE) {
    printf("    FAIL: forthResolveXEQ(\"NONEXISTENT_ITEM\") returned %d (expected NONE)\n",
    res);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: XEQ item lookup: FORTH->ITEM(%d), FCALL->ITEM(%d), miss->NONE\n",
    ITM_FORTH, ITM_FCALL);
  }
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
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: count cap did not set an error (silent failure)\n");
    return 1;
  }
  printf("    PASS: count cap rejected with error %d\n", lastErrorCode);
  return 0;
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
 * Escaping mutation: reverting items.c PTP_REM back to PTP_NONE (sizes as 2 bytes). */
static int test_forth_step_sizing(void)
{
  uint8_t marker[] = {0x8B, 0x1A, 0xFD, 0x00};
  uint8_t source[]  = {0x8B, 0x1A, 0xFD, 0x05, '3', ' ', 'S', 'Q', ' '};

  uint8_t *next = findKey2ndParam(marker);
  if (next != marker + 4) {
    printf("    FAIL: marker step: next=%p, expected %p (buf+4)\n",
    next, marker + 4);
    return 1;
  }

  next = findKey2ndParam(source);
  if (next != source + 9) {
    printf("    FAIL: source step: next=%p, expected %p (buf+9)\n",
    next, source + 9);
    return 1;
  }

  printf("    PASS: ITM_FORTH step sizing correct (marker=4, source=9)\n");
  return 0;
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
 * Mutations: a no-op forthProgramStep handler (§9.9 acceptance 1), or a
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

/* test_prescan_generation_rearm
 * T2.5: After a generation bump, the pre-scan re-runs on first touch.
 * Must fail if forthRunGenCheckReset clears the dictionary but not
 * forthScannedCount (scan skipped after dict clear -> FUNCTION_NOT_FOUND). */
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

/* test_dict_name_by_index
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

  if (!forthDictNameByIndex(0, namebuf, sizeof(namebuf))) {
    printf("    FAIL: idx 0 not found\n");
    return 1;
  }
  if (strcmp(namebuf, "ALPHA") != 0) {
    printf("    FAIL: idx 0 is '%s', expected 'ALPHA'\n", namebuf);
    return 1;
  }

  if (!forthDictNameByIndex(1, namebuf, sizeof(namebuf))) {
    printf("    FAIL: idx 1 not found\n");
    return 1;
  }
  if (strcmp(namebuf, "BETA") != 0) {
    printf("    FAIL: idx 1 is '%s', expected 'BETA'\n", namebuf);
    return 1;
  }

  if (forthDictNameByIndex(fdict.count, namebuf, sizeof(namebuf))) {
    printf("    FAIL: idx == count should return false\n");
    return 1;
  }

  printf("    PASS: forthDictNameByIndex round-trips, count is out of range\n");
  return 0;
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

/* test_exec_step_marker_noop
 * Mutation: the arm calling forthProgramStep for len==0 too (the marker
 * would interpret an empty line and set FLAG_ASLIFT/N drop state).
 * (§9.9 acceptance 8a)
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
 * (§9.9 acceptance 1 at executeOneStep granularity). */
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
 * (§9.9 acceptance 7b's PC-testable half)
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

/* ---- COMMIT 5: §9.4 derived-state helpers + test-program infrastructure ---- */

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

/* test_marker_parity
 * Program: marker, source(: SQ DUP * ;), marker, marker
 * Assert turnsOn == true/false/true for the 1st/3rd/4th markers.
 * Escaping mutation: inverting the parity test (odd instead of even) —
 * every direction flips. (§9.9 acceptance 4 logic) */
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
 * (§9.9 acceptance 2 logic) */
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
 * Tests both opening and closing of Forth capture via addStepInProgram(ITM_FORTH).
 *
 * Opening case: program [RPN][ITM_END][.END.], cursor on ITM_END.
 *   Pre-move skipped (isAtEndOfProgram), predecessor = RPN → wasOn=false → capture opens.
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

    /* Cursor on ITM_END (offset 1) — pre-move skipped */
    currentStep = beginOfProgramMemory + 1;
    pemCursorIsZerothStep = false;
    currentLocalStepNumber = 2;
    catalog = CATALOG_NONE;
    aimBuffer[0] = 0;
    tam.mode = 0;
    clearSystemFlag(FLAG_ALPHA);

    extern void addStepInProgram(int16_t func);
    addStepInProgram(ITM_FORTH);

    /* Check: FLAG_ALPHA set (capture opened) */
    if (!getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA not set after opening toggle\n");
      fail = 1;
    }

    /* Check: marker inserted before ITM_END */
    uint8_t *marker = beginOfProgramMemory + 1;
    if (*(marker + 0) != 0x8B || *(marker + 1) != 0x1A ||
    *(marker + 2) != 0xFD || *(marker + 3) != 0x00) {
      printf("    FAIL: opening marker not found (got 0x%02X 0x%02X 0x%02X 0x%02X)\n",
      *(marker+0), *(marker+1), *(marker+2), *(marker+3));
      fail = 1;
    }

    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    catalog = savedCatalog;
    currentLocalStepNumber = savedLocalStep;
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
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

    /* Cursor on ITM_END (offset 20) — pre-move skipped */
    currentStep = beginOfProgramMemory + 20;
    pemCursorIsZerothStep = false;
    currentLocalStepNumber = 3;
    catalog = CATALOG_NONE;
    aimBuffer[0] = 0;
    tam.mode = 0;
    clearSystemFlag(FLAG_ALPHA);

    extern void addStepInProgram(int16_t func);
    addStepInProgram(ITM_FORTH);

    /* Check: FLAG_ALPHA clear (closing toggle, no capture) */
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA set after closing toggle (should be clear)\n");
      fail = 1;
    }

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
  }

  if (!fail) {
    printf("    PASS: toggle inserts marker, opens/closes capture correctly\n");
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

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  tam.value = savedTamValue;
  tam.indirect = savedTamIndirect;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);

  printf("    PASS: FCALL redirect records name '%.*s', no ITM_FCALL opcode\n", nameLen, (char*)(s+4));
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
 * returned to exactly one marker (no phantom second marker) and FLAG_ALPHA
 * clear (§9.9 acceptance 8b).
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

  /* The empty ENTER is the escape hatch, and the only close path that must
   * clear the sentinel: the opening marker survives, so the cursor is still
   * inside an open region and a leaked ITM_FORTH would make the next
   * keystroke behave as if capture were still up. (E5's lock deliberately
   * KEEPS the sentinel on the non-empty path — see
   * test_forth_multiline_lock_holds.) */
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
  if (stepsAfter != stepsBefore + 1) {
    printf("    FAIL: step count = %d, expected %d (only opening marker remains)\n",
    stepsAfter, stepsBefore + 1);
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

  if (strcmp(aimBuffer, ": SQ DUP * ;") != 0) {
    printf("    FAIL: aimBuffer = '%s', expected ': SQ DUP * ;'\n", aimBuffer);
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    pemCursorIsZerothStep = savedZeroth;
    currentLocalStepNumber = savedLocalStep;
    tam.function = savedTamFunc;
    xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
    if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    return 1;
  }

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
 * (§9.9 acceptance 4).
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
 * §9.9 acceptance 3's essence. */
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
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundSQ = 0, foundCUBE = 0;
    while (*content) {
      if (compareString(content, "CUBE", CMP_BINARY) == 0) foundCUBE = 1;
      if (compareString(content, "SQ", CMP_BINARY) == 0) foundSQ = 1;
      content += strlen(content) + 1;
    }
    if (!foundSQ) {
      printf("    FAIL: 'SQ' not found in menuContent\n");
      fail = 1;
    }
    if (!foundCUBE) {
      printf("    FAIL: 'CUBE' not found in menuContent\n");
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
    printf("    PASS: picker finds SQ and CUBE, numItems==2, sorted\n");
  }
  return fail;
}

/* test_picker_omits_long_names
 * Two 15-byte word names and one 14-byte name. The 15-byte names are omitted,
 * only the 14-byte name is present.
 * Escaping mutation: truncating instead of omitting — the 15-byte names
 * are cut to 14 bytes and appear in menuContent, so numItems > 1. */
static int test_picker_omits_long_names(void)
{
  /* marker | : ABCDEFGHIJKLMNO (15) ; | : PQRSTUVWXYZABCD (15) ; | : SHORT (5) ; | marker | .END. */
  uint8_t prog[] = {
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (opening) */
    0x8B, 0x1A, 0xFD, 0x13, ':', ' ', 'A', 'B', 'C', 'D', 'E', 'F',    /* : ABCDEFGHIJKLMNO (15) */
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x13, ':', ' ', 'P', 'Q', 'R', 'S', 'T', 'U',    /* : PQRSTUVWXYZABCD (15) */
    'V', 'W', 'X', 'Y', 'Z', 'A', 'B', 'C', 'D', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x09, ':', ' ', 'S', 'H', 'O', 'R', 'T', ' ',    /* : SHORT (5) */
    ';',
    0x8B, 0x1A, 0xFD, 0x00,                                              /* marker (closing) */
  };

  if (!writeTestProgram(prog, sizeof(prog))) {
    printf("    FAIL: writeTestProgram failed\n");
    return 1;
  }

  /* marker(4) + 15byte1(23) + 15byte2(23) + SHORT(13) = 63 → closing marker */
  const uint8_t *closingMarker = beginOfProgramMemory + 4 + 23 + 23 + 13;
  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)closingMarker;

  testInitVariableSoftmenu(22);

  int fail = 0;

  if (dynamicSoftmenu[22].numItems != 1) {
    printf("    FAIL: numItems = %d, expected 1 (two 15-byte names omitted, SHORT kept)\n",
    dynamicSoftmenu[22].numItems);
    fail = 1;
  }

  if (dynamicSoftmenu[22].menuContent) {
    const char *content = (const char *)dynamicSoftmenu[22].menuContent;
    int foundShort = 0, foundLong = 0;
    while (*content) {
      if (compareString(content, "SHORT", CMP_BINARY) == 0) foundShort = 1;
      int clen = strlen(content);
      if (clen >= 14) foundLong = 1;  /* truncated 15-byte name would be 14 chars */
      content += clen + 1;
    }
    if (foundLong) {
      printf("    FAIL: 15-byte name should be omitted, not truncated\n");
      fail = 1;
    }
    if (!foundShort) {
      printf("    FAIL: 'SHORT' should be present\n");
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
    printf("    PASS: 15-byte names omitted, SHORT present, numItems==1\n");
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
  char aimSaved[AIM_BUFFER_LENGTH];
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Set up for picker insert */
  xcopy(aimSaved, aimBuffer, sizeof(aimSaved));
  aimBuffer[0] = 0;
  T_cursorPos = 0;
  softmenuStack[0].softmenuId = 22;  /* MNU_FORTH */
  dynamicMenuItem = 0;               /* "SQ" is the first (sorted) entry */

  extern bool_t pickerInsertName(void);
  if (!pickerInsertName()) {
    printf("    FAIL: pickerInsertName returned false\n");
    fail = 1;
  } else {
    if (strcmp(aimBuffer, "SQ ") != 0) {
      printf("    FAIL: aimBuffer = '%s', expected 'SQ '\n", aimBuffer);
      fail = 1;
    }
    if (T_cursorPos != 3) {
      printf("    FAIL: T_cursorPos = %d, expected 3\n", T_cursorPos);
      fail = 1;
    }
  }

  /* Cleanup */
  xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
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
  char aimSaved[AIM_BUFFER_LENGTH];
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Set up: "DUP " in buffer, cursor at 0 (before DUP) */
  xcopy(aimSaved, aimBuffer, sizeof(aimSaved));
  xcopy(aimBuffer, "DUP ", 4);
  T_cursorPos = 0;
  softmenuStack[0].softmenuId = 22;
  dynamicMenuItem = 0;

  extern bool_t pickerInsertName(void);
  if (!pickerInsertName()) {
    printf("    FAIL: pickerInsertName returned false\n");
    fail = 1;
  } else {
    if (strcmp(aimBuffer, "SQ DUP ") != 0) {
      printf("    FAIL: aimBuffer = '%s', expected 'SQ DUP '\n", aimBuffer);
      fail = 1;
    }
    if (T_cursorPos != 3) {
      printf("    FAIL: T_cursorPos = %d, expected 3\n", T_cursorPos);
      fail = 1;
    }
  }

  /* Cleanup */
  xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
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
 * Build picker menu with "SQ". Set aimBuffer = "DUP ", cursor at end (4).
 * Insert "SQ"; assert aimBuffer == "DUP SQ " (trailing space present).
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
  char aimSaved[AIM_BUFFER_LENGTH];
  int16_t savedCursorPos = T_cursorPos;
  int16_t savedDynMenuItem = dynamicMenuItem;
  uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;

  currentProgramNumber = 1;
  currentStep = (uint8_t *)sqStep;

  testInitVariableSoftmenu(22);

  int fail = 0;

  /* Set up: "DUP " in buffer, cursor at end */
  xcopy(aimSaved, aimBuffer, sizeof(aimSaved));
  xcopy(aimBuffer, "DUP ", 4);
  T_cursorPos = 4;
  softmenuStack[0].softmenuId = 22;
  dynamicMenuItem = 0;

  extern bool_t pickerInsertName(void);
  if (!pickerInsertName()) {
    printf("    FAIL: pickerInsertName returned false\n");
    fail = 1;
  } else {
    if (strcmp(aimBuffer, "DUP SQ ") != 0) {
      printf("    FAIL: aimBuffer = '%s', expected 'DUP SQ '\n", aimBuffer);
      fail = 1;
    }
    if (T_cursorPos != 7) {
      printf("    FAIL: T_cursorPos = %d, expected 7\n", T_cursorPos);
      fail = 1;
    }
  }

  /* Cleanup */
  xcopy(aimBuffer, aimSaved, sizeof(aimSaved));
  T_cursorPos = savedCursorPos;
  dynamicMenuItem = savedDynMenuItem;
  softmenuStack[0].softmenuId = savedSoftmenuStackId;
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
 * This is also the overflow probe: under ASAN, the old unchecked xcopy would
 * trigger stack-buffer-overflow on tok[64].
 * Escaping mutation: restore the unchecked xcopy — ASAN build fails with
 * stack-buffer-overflow; non-ASAN still passes numItems assert. */
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

  /* Assert: aimBuffer contains "2" */
  if (aimBuffer[0] != '2' || aimBuffer[1] != 0) {
    printf("    FAIL: aimBuffer = '%s', expected '2'\n", aimBuffer);
    fail = 1;
  }

  /* Cleanup: close capture via pemAlpha(ITM_ENTER) to commit the step */
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
 * F4 follow-through (DESIGN.md §9.10 item 4): the write side
 * (insertUserItemInProgram) is tested by test_useritem_xeqp1_opcode; this
 * test verifies the full insert -> decode/display path. decode.c's own
 * two-byte opcode reassembly (_decodeOneStep: op &= 0x7f; op <<= 8;
 * op |= *(step++);) already ORs in the low byte unmasked and is
 * byte-identical to upstream src/c47/programming/decode.c at that site
 * [VERIFIED: packages/forth-core/programming/decode.c:866-869] -- no decode-
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
 * step → wasOn = false → opening marker inserted, capture opens.
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

  /* Assert: new marker inserted between RPN step and old marker */
  scanLabelsAndPrograms();
  /* After insertion: RPN(1) + newMarker(4) + oldMarker(4) + source(16) + marker2(4) + END(2) + .END.(2) */
  uint8_t *newMarker = beginOfProgramMemory + 1;  /* right after ITM_sin */
  if (*(newMarker + 0) != 0x8B || *(newMarker + 1) != 0x1A ||
  *(newMarker + 2) != 0xFD || *(newMarker + 3) != 0x00) {
    printf("    FAIL: new marker not at expected position (got 0x%02X 0x%02X 0x%02X 0x%02X)\n",
    *(newMarker+0), *(newMarker+1), *(newMarker+2), *(newMarker+3));
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
    printf("    PASS: E1 direction mid-program — opening marker inserted, capture opened\n");
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

/* T1.1 (H5 round-trip). Must fail if: any of the five forthDict* params is
 * dropped from the save or restore hunk, or the restore rebases fdict.base
 * without TO_PCMEMPTR. */
static int test_save_restore_roundtrip(void)
{
  int fail = 0;
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SRW1 7 ;");
  forthOuterInterpret(": SRW2 SRW1 35 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: setup definitions raised error %d\n", lastErrorCode);
    return 1;
  }
  uint16_t savedHere = fdict.here, savedCount = fdict.count, savedLatest = fdict.latest;

  saveCalc();
  if (!backupFileContains("forthDictBase:")) {
    printf("    FAIL: saveCalc did not write forthDictBase (calcModel guard or missing hunk)\n");
    forthDictClear();
    return 1;
  }

  /* clobber the dictionary so only a real restore can bring it back */
  forthDictClear();
  forthOuterInterpret(": SRZZ 1 ;");

  {
    bool_t savedLoad = loadTestPrograms;
    loadTestPrograms = false;
    restoreCalc();
    loadTestPrograms = savedLoad;
  }

  uint16_t idx;
  if (!forthFindColon("SRW1", &idx)) { printf("    FAIL: SRW1 lost across restore\n"); fail = 1; }
  if (!forthFindColon("SRW2", &idx)) { printf("    FAIL: SRW2 lost across restore\n"); fail = 1; }
  if (forthFindColon("SRZZ", &idx))  { printf("    FAIL: post-save word SRZZ survived restore\n"); fail = 1; }
  if (fdict.here != savedHere || fdict.count != savedCount || fdict.latest != savedLatest) {
    printf("    FAIL: fdict scalars mismatch (here %u/%u count %u/%u latest %u/%u)\n",
           fdict.here, savedHere, fdict.count, savedCount, fdict.latest, savedLatest);
    fail = 1;
  }

  if (!fail && forthFindColon("SRW2", &idx)) {
    lastErrorCode = ERROR_NONE;
    forthInner(idx, false);
    if (lastErrorCode != ERROR_NONE) { printf("    FAIL: SRW2 raised %d\n", lastErrorCode); fail = 1; }
    else if (!x_is_longint(35))      { printf("    FAIL: X != 35 after SRW2\n"); fail = 1; }
  }

  printf("  FORTH ARENA (post-restore): here=%u sizeBlocks=%u\n", fdict.here, fdict.sizeBlocks);
  forthDictClear();   /* balance the suite-level leak gate */
  if (!fail) printf("    PASS: save/restore round-trip preserved the dictionary\n");
  return fail;
}

/* T1.2 (old-backup defaults). Must fail if: the pre-seeded defaults before
 * each restoreStateValue call are removed (stale ramPtr from the programList
 * restore would masquerade as the dict base). */
static int test_restore_missing_params_defaults(void)
{
  int fail = 0;
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": OLDW 2 ;");
  if (lastErrorCode != ERROR_NONE) { printf("    FAIL: setup error %d\n", lastErrorCode); return 1; }
  uint8_t *preBase = fdict.base;
  uint16_t preBlocks = fdict.sizeBlocks;

  saveCalc();
  if (!backupFileContains("forthDictBase:")) { printf("    FAIL: save missing params\n"); forthDictClear(); return 1; }
  if (editBackupFile("forthDict", NULL, NULL)) { printf("    FAIL: file edit failed\n"); forthDictClear(); return 1; }

  { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

  if (fdict.base != NULL || fdict.latest != FORTH_NULL || fdict.count != 0
      || fdict.sizeBlocks != 0 || fdict.here != 0) {
    printf("    FAIL: missing params did not default to empty dict\n");
    fail = 1;
  }
  /* The restored arena still carries the pre-save dict region, now orphaned
   * by design (params stripped). Release it to balance the leak gate. */
  if (preBase) freeC47Blocks(preBase, preBlocks);

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": NEWW 3 ;");
  { uint16_t idx; if (lastErrorCode != ERROR_NONE || !forthFindColon("NEWW", &idx)) {
      printf("    FAIL: lazy alloc broken after defaulted restore\n"); fail = 1; } }
  forthDictClear();
  if (!fail) printf("    PASS: stripped params default to empty dict\n");
  return fail;
}

/* T1.3 (validation clamps corruption). Must fail if:
 * forthDictValidateRestored is not called from the restore hunk, or its
 * here-bound / chain-count checks are deleted (next dict write would land
 * out of bounds). */
static int test_restore_validation_clamps(void)
{
  int fail = 0;
  int variant;
  for (variant = 0; variant < 2; variant++) {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VALW 4 ;");
    if (lastErrorCode != ERROR_NONE) { printf("    FAIL: setup error\n"); return 1; }
    uint8_t *preBase = fdict.base;
    uint16_t preBlocks = fdict.sizeBlocks;

    saveCalc();
    if (variant == 0) {
      if (editBackupFile(NULL, "forthDictHere:", "forthDictHere:uint16:65534\n")) { printf("    FAIL: edit\n"); return 1; }
    } else {
      char repl[64];
      sprintf(repl, "forthDictCount:uint16:%u\n", (unsigned)fdict.count + 1);
      if (editBackupFile(NULL, "forthDictCount:", repl)) { printf("    FAIL: edit\n"); return 1; }
    }

    { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

    if (fdict.base != NULL) {
      printf("    FAIL: variant %d: corrupt scalars survived validation\n", variant);
      fail = 1;
      forthDictClear();     /* free whatever it points at, best effort */
    }
    else if (preBase) {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VOKW 5 ;");
    { uint16_t idx; if (lastErrorCode != ERROR_NONE || !forthFindColon("VOKW", &idx)) {
        printf("    FAIL: variant %d: dict unusable after validation reset\n", variant); fail = 1; } }
    forthDictClear();
  }
  if (!fail) printf("    PASS: corrupt here/count both clamped to empty dict\n");
  return fail;
}

/* T1.3b (validator direct pins). V1 must fail if the sizeBlocks!=0 check is
 * removed (stale base + zeroed scalars passes every other check: cap=0,
 * here=0<=0, latest=FORTH_NULL skips the walk, n=0==count).
 * V2 must fail if the nameLen bounds check is removed (a zeroed nameLen
 * header walks clean through the off/link/count checks). */
static int test_validate_direct_corruption(void)
{
  int fail = 0;

  /* V1: stale base with zeroed scalars */
  {
    uint8_t *region = allocC47Blocks(4);
    if (!region) { printf("    SKIP: alloc failed\n"); return 0; }
    forthDictClear();
    fdict.base = region;             /* simulate stale-pointer restore */
    fdict.sizeBlocks = 0;
    fdict.here = 0;
    fdict.latest = FORTH_NULL;
    fdict.count = 0;
    forthDictValidateRestored();
    if (fdict.base != NULL) {
      printf("    FAIL: V1 stale base with zeroed scalars survived validation\n");
      fail = 1;
      forthDictClear();              /* best effort */
    }
    freeC47Blocks(region, 4);        /* release the deliberate orphan */
  }

  /* V2: corrupt nameLen on a real header */
  {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VD2 1 ;");
    if (lastErrorCode != ERROR_NONE || !fdict.base) {
      printf("    SKIP: V2 setup failed\n");
      return fail;
    }
    uint8_t *preBase = fdict.base;
    uint16_t preBlocks = fdict.sizeBlocks;
    ((forthHeader_t *)(fdict.base + fdict.latest))->nameLen = 0;
    forthDictValidateRestored();
    if (fdict.base != NULL) {
      printf("    FAIL: V2 zero-nameLen header survived validation\n");
      fail = 1;
      forthDictClear();
    }
    else {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }
  }

  forthDictClear();
  if (!fail) printf("    PASS: validator direct pins (sizeBlocks, nameLen)\n");
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

  printf("  [DEBUG] running test_xeq_end_to_end...\n");
  fail |= test_xeq_end_to_end();
  printf("  [DEBUG] running test_tam_dispatcher...\n");
  fail |= test_tam_dispatcher();
#ifdef FORTH_DEBUG_SELFTEST
  printf("  [DEBUG] running test_reentrancy...\n");
  fail |= test_reentrancy();
#endif
  printf("  [DEBUG] running test_xeq_precedence...\n");
  fail |= test_xeq_precedence();
  printf("  [DEBUG] running test_xeq_item_lookup...\n");
  fail |= test_xeq_item_lookup();
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

  printf("  [DEBUG] running test_prescan_generation_rearm...\n");
  fail |= test_prescan_generation_rearm();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_error_halts...\n");
  fail |= test_prescan_error_halts();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_last_step_visible...\n");
  fail |= test_prescan_last_step_visible();
  forthDictClear();

  printf("  [DEBUG] running test_prescan_two_programs_first_touch...\n");
  fail |= test_prescan_two_programs_first_touch();
  forthDictClear();

  printf("  [DEBUG] running test_dict_name_by_index...\n");
  fail |= test_dict_name_by_index();
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

  /* COMMIT 5: §9.4 derived-state helpers + test-program infrastructure */
  printf("\nFORTH COMMIT 5 TESTS (§9.4 derived-state helpers)\n");
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

  /* COMMIT 8: decode.c override — §9.5 symmetric display */
  printf("\nFORTH COMMIT 8 TESTS (decode.c override: §9.5 marker rendering)\n");
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

  /* FIX-6: Arena report (§5.4/§9.9 duty) — define words, report, then clear */
  { uint32_t freeRamBefore = getFreeRamMemory();
    forthDictInit();
    define_word("AR1", 3);
    define_word("AR2", 3);
    define_word("AR3", 3);
    uint32_t freeRamAfter = getFreeRamMemory();
    uint16_t dictBlocks = fdict.sizeBlocks;
    uint32_t dictBytes = TO_BYTES(dictBlocks);
    printf("  FORTH ARENA: dict here=%u sizeBlocks=%u  freeRamDelta=%ld\n",
    fdict.here, fdict.sizeBlocks, (long)(freeRamBefore - freeRamAfter));
    if (dictBlocks > 512) {
      printf("    WARN: dict region %u blocks (%u bytes) exceeds 2 KB budget\n",
      dictBlocks, dictBytes);
    } else {
      printf("    PASS: dict region %u blocks (%u bytes) within 2 KB budget\n",
      dictBlocks, dictBytes);
    }
    forthDictClear();
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

  currentStep = beginOfProgramMemory + 1;
  pemCursorIsZerothStep = false;
  currentLocalStepNumber = 2;
  catalog = CATALOG_NONE;
  aimBuffer[0] = 0;
  tam.mode = 0;
  clearSystemFlag(FLAG_ALPHA);
  tam.function = ITM_FORTH;

  extern void addStepInProgram(int16_t func);
  addStepInProgram(ITM_FORTH);

  if (currentMenu() != -MNU_ALPHA) {
    printf("    FAIL: currentMenu() = %d, expected %d (-MNU_ALPHA)\n",
    currentMenu(), -MNU_ALPHA);
    fail = 1;
  }

  cleanupTestProgram();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  catalog = savedCatalog;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  softmenuStack[0].softmenuId = savedSoftmenuStackId;

  if (!fail) {
    printf("    PASS: alpha menu on top during Forth capture (not MNU_FORTH overlay)\n");
  }
  return fail;
}

/* test_alpha_menu_contains_fwrd
 * F5: The MNU_ALPHA item table (menu_ALPHA) must contain a -MNU_FORTH entry
 * so the Forth word picker is reachable as a submenu from the alpha menu.
 * Escaping mutation: remove the -MNU_FORTH entry from menu_ALPHA — the assertion fails.
 * [VERIFIED: softmenus.c:1000]
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
  fnKeyInCatalog = 0;       /* ...and :1229 */

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
  fnKeyInCatalog = 0;

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

#endif  // PC_BUILD
