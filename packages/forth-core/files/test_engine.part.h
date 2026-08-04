/* packages/forth-core/test_engine.part.h — T5 split part of test_dict_reloc.c (2026-08-03).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 */
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

/* test_program_memory_no_overlap
 * Mutation: writeTestProgram directly manipulates freeMemoryRegions[0].sizeInBlocks
 * instead of using resizeProgramMemory, creating free-list fragments that overlap
 * with region 0 and trigger the firmware-bug screen in freeListFree() (FIX-6B).
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
 * the firmware-bug screen in freeListFree() (FIX-6B).
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

/* ---- D3-2: recursion past the data stack spills, drains in order ----
 * The Forth data stack is the RPN stack (8 levels here), and a recursive word
 * holds one live operand per level. FACT used to run off the top silently and
 * return 720*6 = 4320 for 7!, with lastErrorCode 0. It must now refuse.
 * Mutation: drop the forthDataDepthApply() call at the FTOK_PRIM dispatch ->
 * 7 FACT returns 4320 again and subcase 2 fails. ---- */
static int test_data_stack_overflow_guard(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint8_t tType;
  int32_t tVal;

  programRunStop = PGM_STOPPED;
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": FACT DUP IF DUP 1 - RECURSE * ELSE DROP 1 THEN ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: could not define FACT (%d)\n", lastErrorCode);
    programRunStop = savedRS;
    return 1;
  }

  /* Subcase 1: what fits must still compute exactly. */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 6 FACT");
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 720) {
    printf("    FAIL: 6 FACT should be 720, got %ld type %u (error %d)\n",
           (long)tVal, tType, lastErrorCode);
    fail = 1;
  }

  /* Subcase 2a: D3-2A — push capacity+3 values and consume back in the same line.
   * Eleven literals then ten + drains to one value: sum(1..11) = 66. */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 5 6 7 8 9 10 11 + + + + + + + + + +");
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 66) {
      printf("    FAIL: same-line drain should give 66, got %ld type %u (error %d)\n",
             (long)tVal, tType, lastErrorCode);
      fail = 1;
    }
  }

  /* Subcase 2b: D3-2A — push capacity+3 and end line: line-end contract triggers
   * ERROR_RAM_FULL. Then verify a following ordinary line still works. */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 5 6 7 8 9 10 11");
    if (lastErrorCode != ERROR_RAM_FULL) {
      printf("    FAIL: line-end spill should give ERROR_RAM_FULL, got error %d\n",
             lastErrorCode);
      fail = 1;
    }

    /* Recovery: next ordinary line must work */
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 42");
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 42) {
      printf("    FAIL: post-error line should give 42, got %ld type %u (error %d)\n",
             (long)tVal, tType, lastErrorCode);
      fail = 1;
    }
  }

  /* Subcase 3: shallow computation must still work. */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 +");
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 45) {
    printf("    FAIL: shallow chained adds should give 45, got %ld type %u (error %d)\n",
           (long)tVal, tType, lastErrorCode);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: deep push spills, drains back in order\n");
  }

  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  forthDictClear();      /* FACT was defined here: release its region */
  forthGDictClear();
  return fail;
}

/* ---- D1: a native item after a Forth value must LIFT, not clobber X ----
 * Upstream's dispatcher epilogue sets FLAG_ASLIFT after every SLS_ENABLED item,
 * and every prim-equivalent (fnAdd, fnDrop, fnSwapXY, fnMultiply) carries it.
 * forth-core used to scrub the flag after each push and each prim, so the next
 * native item took liftStack()'s else-branch and overwrote X.
 * Mutation: restore clearSystemFlag(FLAG_ASLIFT) at either site -> Y is not the
 * Forth value and both subcases fail. ---- */
static int test_native_lift_after_forth(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint8_t tType;
  int32_t tVal;

  programRunStop = PGM_STOPPED;

  /* Subcase 1: literal push then RCL. R47 keeps the literal in Y. */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 7 STO 19");
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1000 RCL 19");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"1000 RCL 19\" errored (%d)\n", lastErrorCode);
    fail = 1;
  }
  else {
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 7) {
      printf("    FAIL: RCL 19 should leave 7 in X, got %ld type %u\n",
             (long)tVal, tType);
      fail = 1;
    }
    read_reg_int32(REGISTER_Y, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 1000) {
      printf("    FAIL: RCL 19 clobbered X instead of lifting; Y=%ld type %u, expected 1000\n",
             (long)tVal, tType);
      fail = 1;
    }
  }

  /* Subcase 2: same, but the value in X came from a primitive, not a literal.
   * fnAdd is SLS_ENABLED upstream, so the following RCL must still lift. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 5 3 + RCL 19");
    if (lastErrorCode != ERROR_NONE) {
      printf("    FAIL: \"5 3 + RCL 19\" errored (%d)\n", lastErrorCode);
      fail = 1;
    }
    else {
      read_reg_int32(REGISTER_Y, &tType, &tVal);
      if (tType != dtLongInteger || tVal != 8) {
        printf("    FAIL: RCL after + clobbered the sum; Y=%ld type %u, expected 8\n",
               (long)tVal, tType);
        fail = 1;
      }
    }
  }

  if (!fail) {
    printf("    PASS: native items lift onto a Forth value instead of overwriting it\n");
  }

  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  return fail;
}

static int test_savings_program(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  testProg_t p;
  calcRegister_t lbl;
  char tickLine[96];
  int i;

  /* Each entry subtracts the documented balance from its register: an exact
   * schedule leaves zero, so a drifted value fails without string compares. */
  static const char *schedule[] = {
    "RCL 00 1050 -",
    "RCL 01 1102.5 -",
    "RCL 02 1157.625 -",
    "RCL 03 1215.50625 -",
    "RCL 04 1276.2815625 -",
    "RCL 05 1340.095640625 -",
    "RCL 22 1340.095640625 -"
  };

  tpInit(&p);
  snprintf(tickLine, sizeof(tickLine),
           ": PUT STO %s20 ;", STD_RIGHT_ARROW);

  if (tpLbl(&p, "SAVE") < 0 ||
      tpMarker(&p) < 0 ||
      tpSrc(&p, ": GROW 1.05 * ; GLOBAL") < 0 ||
      tpSrc(&p, tickLine) < 0 ||
      tpSrc(&p, ": BUMP RCL 20 1 + STO 20 DROP ;") < 0 ||
      tpSrc(&p, ": TALLY RCL 19 1 - STO 19 DROP ;") < 0 ||
      tpSrc(&p, ": STEP GROW PUT BUMP TALLY ;") < 0 ||
      tpSrc(&p, ": RUN BEGIN RCL 19 WHILE STEP REPEAT ;") < 0 ||
      tpSrc(&p, "0 STO 20 DROP") < 0 ||
      tpSrc(&p, "6 STO 19 DROP") < 0 ||
      tpSrc(&p, "1000 RUN STO 22") < 0 ||
      tpMarker(&p) < 0 ||
      tpRtn(&p) < 0 ||
      tpEnd(&p) < 0 ||
      !tpWrite(&p)) {
    printf("    FIXTURE FAIL: SAVE program build/write\n");
    fail = 1;
    goto cleanup;
  }

  lbl = findNamedLabel("SAVE", GLOBAL_LABELS);
  if (lbl == INVALID_VARIABLE) {
    printf("    FIXTURE FAIL: SAVE label not found\n");
    fail = 1;
    goto cleanup;
  }

  programRunStop = PGM_STOPPED;
  lastErrorCode = ERROR_NONE;
  fnExecute(lbl);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: SAVE run error %d\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }

  for (i = 0; i < (int)(sizeof(schedule) / sizeof(schedule[0])); i++) {
    if (!forthExprIsZero(schedule[i])) {
      printf("    FAIL: schedule check \"%s\" did not come out zero (error %d)\n",
             schedule[i], lastErrorCode);
      fail = 1;
    }
  }

  /* The loop ran exactly six times: countdown drained, slot index advanced. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("RCL 19");
    if (lastErrorCode != ERROR_NONE || !x_is_longint(0)) {
      printf("    FAIL: R19 countdown did not reach 0\n");
      fail = 1;
    }
  }
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("RCL 20");
    if (lastErrorCode != ERROR_NONE || !x_is_longint(6)) {
      printf("    FAIL: R20 slot index did not reach 6\n");
      fail = 1;
    }
  }

  /* GROW was promoted, so it survives the program and composes standalone. */
  if (!fail) {
    uint16_t ref;
    if (!forthFindColon("GROW", &ref) || !(ref & FORTH_REF_GLOBAL)) {
      printf("    FAIL: GROW is not global\n");
      fail = 1;
    }
    else if (!forthExprIsZero("XEQ 'CLSTK' 200 GROW GROW 220.5 -")) {
      printf("    FAIL: global GROW GROW on 200 did not give 220.5\n");
      fail = 1;
    }
  }

  if (!fail) {
    printf("    PASS: SAVE wrote the six-period schedule, drained its counters, and left GROW global\n");
  }

cleanup:
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();
  return fail;
}

