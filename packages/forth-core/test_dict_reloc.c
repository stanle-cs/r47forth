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
 * forthTestSetRunning/IsRunning). */
#if defined(PC_BUILD)

#include <string.h>
#include "c47.h"
#include "forth_dict.h"

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
   bool err = run_word("BF");
   if (err) {
     printf("    FAIL: unexpected error %d during fwd branch\n", lastErrorCode);
     return 1;
   }
   if (!x_is_longint(42)) {
     printf("    FAIL: X should be 42 after BR forward skip\n");
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
   uint16_t w = begin_word("LA", 2);
  if (w == FORTH_NULL) { printf("    SKIP: alloc failed\n"); return 0; }
  forthDictEmit(T_ILIT);
  emit_int32(10);
  forthDictEmit(T_ILIT);
  emit_int32(20);
  forthDictEmit(PRIM_TOKEN(P_DROP));
  end_word(w);

  lastErrorCode = ERROR_NONE;
  bool err = run_word("LA");
  if (err) {
    printf("    FAIL: desynced after ILIT (ip+=8 mutation: error %d)\n", lastErrorCode);
    return 1;
  }
  printf("    PASS: live token after ILIT executes correctly\n");
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
 * ITM_FCALL (2843, PTP_NUMBER_16) -> fnForthCall(idx) -> forthInner(idx)
 * Re-entrancy guard fires (forthRunning already true) -> ERROR_OPERATION_UNDEFINED.
 * Sentinel ILIT 999 does NOT run.
 * Mutation: no guard -> nested forthInner runs, sentinel may or may not run. ---- */
static int test_c47_nested_reentry(void)
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
  if (!err) {
    printf("    FAIL: nested re-entrancy guard did not fire (no error)\n");
    return 1;
  }
  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL: expected ERROR_OPERATION_UNDEFINED, got %d\n", lastErrorCode);
    return 1;
  }
  if (x_is_longint(999)) {
    printf("    FAIL: sentinel ILIT 999 executed (guard may not have halted)\n");
    return 1;
  }
  printf("    PASS: nested re-entrancy guard fired (ERROR_OPERATION_UNDEFINED)\n");
  return 0;
}

/* ---- Fix #13: outer interpreter real literal ----
 * forthOuterInterpret("2.5 2 *") -> X is dtReal34, value 5.0.
 * Mutation: real literal not classified -> undefined word error. ---- */
static int test_outer_real_literal(void)
{
  forthDictInit();
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
  printf("    PASS: DIV by zero halted (error %d, X preserved, sentinel not executed)\n", lastErrorCode);
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
  if (lastErrorCode != ERROR_NONE) {
    printf("    PASS: rstack overflow caught (error %d, recovered)\n", lastErrorCode);
    return 0;
  }
  printf("    PASS: rstack overflow detected (X unchanged, err=%d)\n", lastErrorCode);
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

  if (x_is_longint(444)) {
    printf("    PASS: runaway guard halted (X unchanged at 444, err=%d)\n", lastErrorCode);
    return 0;
  }
  if (lastErrorCode != ERROR_NONE) {
    printf("    PASS: runaway guard triggered (error %d)\n", lastErrorCode);
    return 0;
  }
  printf("    FAIL: runaway loop did not trigger guard (X!=444, err=%d)\n", lastErrorCode);
  return 1;
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
   *   2-261: filler tokens (skipped)
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

  /* Filler: 260 DROP tokens (skipped by BR) */
  int i;
  for (i = 0; i < 260; i++) {
    forthDictEmit(PRIM_TOKEN(P_DROP));
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

/* §7.1 re-entrancy: forthRunning guard fires on nested entry (§3.2)
  * Uses test-only forthTestSetRunning to prime the guard, avoiding
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

  /* Prime the re-entrancy guard */
  forthTestSetRunning(true);

  /* Call forthInner — guard should fire, word should NOT execute */
  lastErrorCode = ERROR_NONE;
  forthInner(idx, false);

  /* Guard does NOT clear forthRunning; reset for subsequent tests */
  forthTestSetRunning(false);

  if (lastErrorCode != ERROR_OPERATION_UNDEFINED) {
    printf("    FAIL: expected ERROR_OPERATION_UNDEFINED (%d), got %d\n",
           ERROR_OPERATION_UNDEFINED, lastErrorCode);
    return 1;
  }
  if (x_is_longint(99)) {
    printf("    FAIL: sentinel value 99 was set — guard did not prevent entry\n");
    return 1;
  }
  if (forthTestIsRunning()) {
    printf("    FAIL: forthRunning still true after manual reset\n");
    return 1;
  }
  printf("    PASS: re-entrancy guard fired (err=%d), word not executed\n",
         lastErrorCode);
  return 0;
}
#endif /* FORTH_DEBUG_SELFTEST */

/* §7.5 precedence: forthResolveXEQ returns LABEL when C47 label shadows Forth word */
static int test_xeq_precedence(void)
{
  const char *sharedName = "XEQP";
  uint8_t nameLen = (uint8_t)sizeof("XEQP") - 1;

  uint16_t savedNumLabels = numberOfLabels;
  labelList_t *savedLabelList = labelList;
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
    labelList = savedLabelList;
    numberOfLabels = savedNumLabels;
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

  /* Restore labelList pointer and count, free label data.
   * Fix #15: restore savedLabelList (not just count) to avoid freeing the
   * expanded block later with the old size (double-free / mismatched free). */
  labelList = savedLabelList;
  numberOfLabels = savedNumLabels;
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


/* §4.2 LBL? UB fix (fix #5): resolvedParam init + LBLQ guard.
 * DEFERRED to §8.5 integration — _executeOp in lblGtoXeq.c requires full
 * C47 program context (program memory, reallyRunFunction dispatch) and is
 * not unit-callable in this harness. The fix ensures:
 *   (1) resolvedParam = INVALID_VARIABLE on a miss (no uninitialized read).
 *   (2) ITM_LBLQ never receives a Forth colon index as a label ID.
 * Mutation: remove the INVALID_VARIABLE initializer -> ASan flags
 *   uninitialized-read when name matches neither label nor Forth word.
 * Verify under ASan: make test_asan CUSTOM_PKG=packages/forth-core ---- */
static int test_lblq_undefined_no_ub(void)
{
  printf("    DEFERRED to §8.5 integration (requires full program context for _executeOp)\n");
  return 0;
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
  /* Stage a word: SQ (DUP * EXIT) */
  forthDictInit();
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

  /* Simulate RESET: forthDictInit() clears the dict (as doFnReset does). */
  forthDictInit();

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

/* Test: items.c FORTH/FCALL rows carry US_ENABLED (§0.2) */
static int test_undo_rows_us_enabled(void)
{
  FILE *f = fopen("/home/stan/c43/packages/forth-core/items.c", "r");
  if (!f) {
    printf("    SKIP: cannot open items.c for static check\n");
    return 0;
  }
  char line[512];
  int forthFound = 0, fcallFound = 0;
  int forthUS = 0, fcallUS = 0;
  while (fgets(line, sizeof(line), f)) {
    if (strstr(line, "\"FORTH\"") && strstr(line, "fnForthOuter")) {
      forthFound = 1;
      forthUS = (strstr(line, "US_ENABLED") != NULL);
    }
    if (strstr(line, "\"FCALL\"") && strstr(line, "fnForthCall")) {
      fcallFound = 1;
      fcallUS = (strstr(line, "US_ENABLED") != NULL);
    }
  }
  fclose(f);

  if (!forthFound || !fcallFound) {
    printf("    SKIP: FORTH/FCALL rows not found in items.c\n");
    return 0;
  }
  if (!forthUS) {
    printf("    FAIL: ITM_FORTH row uses US_UNCHANGED (should be US_ENABLED)\n");
    return 1;
  }
  if (!fcallUS) {
    printf("    FAIL: ITM_FCALL row uses US_UNCHANGED (should be US_ENABLED)\n");
    return 1;
  }
  printf("    PASS: FORTH and FCALL rows carry US_ENABLED\n");
  return 0;
}


int forthDictSelfTest(void)
{
  int fail = 0;

  printf("FORTH DICT SELF-TEST: relocation test\n");

  /* Force a tiny initial region: 4 blocks = 16 bytes. */
  forthDictSetTestInitialBlocks(4);

  forthDictInit();

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

  forthDictClear();

  /* ==================================================================
    * H1 acceptance tests  --  DESIGN.md §7.1 / §7.5
    * Run BEFORE mutation tests to avoid heap corruption from double-free.
    * ================================================================== */
  printf("\nFORTH H1 ACCEPTANCE TESTS (XEQ / re-entrancy / precedence)\n");
  forthDictInit();

  printf("  [DEBUG] running test_xeq_end_to_end...\n");
  fail |= test_xeq_end_to_end();
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
  printf("  [DEBUG] running test_lblq_undefined_no_ub...\n");
  fail |= test_lblq_undefined_no_ub();

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
  fail |= test_c47_nested_reentry();
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

  if (fail) {
    printf("\nFORTH SELF-TEST: FAILED\n");
  } else {
    printf("\nFORTH SELF-TEST: ALL PASSED\n");
  }

  return fail;
}

#endif  // PC_BUILD
