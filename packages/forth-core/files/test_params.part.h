/* packages/forth-core/test_params.part.h — T5 split part of test_dict_reloc.c (2026-08-03).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 */
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

/* "3 DUP +" via forthTestRunFromX -> X == 6 */
static int test_outer_simple_expr(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 DUP +");
  forthTestRunFromX(NOPARAM);
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

/* ": SQ2 DUP * ;" then "3 SQ2" via forthTestRunFromX -> X == 9 (§7.4 full acceptance) */
static int test_outer_compile_invoke(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string(": SQ2 DUP * ;");
  forthTestRunFromX(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \": SQ2 DUP * ;\" compile error %d\n", lastErrorCode);
    return 1;
  }

  lastErrorCode = ERROR_NONE;
  x_set_string("3 SQ2");
  forthTestRunFromX(NOPARAM);
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
  forthTestRunFromX(NOPARAM);
  if (lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) {
    printf("    FAIL: non-string X gave error %d (expected %d)\n",
    lastErrorCode, ERROR_INVALID_DATA_TYPE_FOR_OP);
    return 1;
  }
  printf("    PASS: non-string X -> ERROR_INVALID_DATA_TYPE_FOR_OP (%d)\n",
  ERROR_INVALID_DATA_TYPE_FOR_OP);
  return 0;
}

/* Keyboard glyph: "3 4 " STD_CROSS via forthTestRunFromX -> X == 12 */
static int test_outer_glyph_cross(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 4 " STD_CROSS);
  forthTestRunFromX(NOPARAM);
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

/* Keyboard glyph: "3 4 " STD_DOT via forthTestRunFromX -> X == 12 */
static int test_outer_glyph_dot(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("3 4 " STD_DOT);
  forthTestRunFromX(NOPARAM);
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

/* Keyboard glyph: "8 4 " STD_DIVIDE via forthTestRunFromX -> X == 2 */
static int test_outer_glyph_divide(void)
{
  lastErrorCode = ERROR_NONE;
  x_set_string("8 4 " STD_DIVIDE);
  forthTestRunFromX(NOPARAM);
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

/* Test: a dot with NO MANTISSA DIGITS is not a valid number (Axis 4 spec
 * edge case — classifyNumber's `mantissaDigits == 0` rule).
 *
 * RETARGETED by Stage N packet N1-4 (2026-08-06).  The probe used to be a
 * bare ".", with "lastErrorCode != ERROR_NONE" standing in for "not a
 * number".  That proxy died when `.` became a console output prim: prims
 * resolve at §4.1 step 1, so a bare "." now runs and never reaches the
 * number arm at step 3 — the test would have read a legitimate, designed
 * behaviour change as a grammar bug.
 *
 * The CLAIM is unchanged and still worth pinning, so the probe moved to a
 * token that exercises the same grammar rule and is shadowed by nothing:
 * "+." and "-." are a sign followed by a dot and no digits, which
 * classifyNumber must reject for exactly the mantissaDigits reason, and
 * neither is a prim, an item or a label.  This is the user-shadowing hazard
 * N-T3 named, realised against a test rather than against a user's word. */
static int test_number_bad_lone_dot(void)
{
  const char *probes[2] = { "+.", "-." };
  int i;

  for (i = 0; i < 2; i++) {
    forthDictInit();
    lastErrorCode = ERROR_NONE;
    errorMessage[0] = 0;
    uint8_t xTypeBefore = getRegisterDataType(REGISTER_X);
    forthOuterInterpret(probes[i]);
    if (lastErrorCode != ERROR_FUNCTION_NOT_FOUND) {
      printf("    FAIL: '%s' gave error %d, expected ERROR_FUNCTION_NOT_FOUND (%d)"
             " — a dot with no mantissa digits must not parse as a number\n",
             probes[i], lastErrorCode, ERROR_FUNCTION_NOT_FOUND);
      return 1;
    }
    if (getRegisterDataType(REGISTER_X) != xTypeBefore) {
      printf("    FAIL: '%s' modified X\n", probes[i]);
      return 1;
    }
  }
  printf("    PASS: '+.' and '-.' rejected as undefined words, not parsed as numbers\n");
  return 0;
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: compile SR0 error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RR0 RCL 05 ;");
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: compile SA error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RA RCL A ;");
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: compile SM error %d\n", lastErrorCode);
      subFail = 1;
    }
    if (!subFail) {
      x_set_string(": RM RCL M ;");
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
      if (lastErrorCode != ERROR_OUT_OF_RANGE) {
        printf("    [5] FAIL: SF .32 expected ERROR_OUT_OF_RANGE, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      x_set_string(": SFQ SF q ;");
      forthTestRunFromX(NOPARAM);
      if (lastErrorCode != ERROR_INVALID_NAME) {
        printf("    [5] FAIL: SF q expected ERROR_INVALID_NAME, got %d\n", lastErrorCode);
        subFail = 1;
      }
    }
    if (!subFail) {
      lastErrorCode = ERROR_NONE;
      x_set_string(": CF100 CF 100 ;");
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
     * source string and forthTestRunFromX drops it, so anything pushed beforehand is
     * shifted out of place before the shuffle ever runs. */
    if (!subFail) {
      char rbuf[128];
      lastErrorCode = ERROR_NONE;
      sprintf(rbuf, "11 22 33 44 %s", sbuf);
      x_set_string(rbuf);
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
   * ERROR_UNDEF_SOURCE_VAR miss. Note forthOuterInterpret (not forthTestRunFromX):
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
    forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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
      forthTestRunFromX(NOPARAM);
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

