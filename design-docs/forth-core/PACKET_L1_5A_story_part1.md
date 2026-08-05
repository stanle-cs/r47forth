# PACKET L1-5A — the stage story, part 1 (C1 steps 1–7) — rev 2

**Stage L final packet, session A.** Parent packet:
`PACKET_L1_5_acceptance.md` (C1). **Prerequisite: L1-F3 landed and green.**
This session adds `test_interactive_acceptance` (steps 1–7) and registers
it. Session B appends steps 8–10 at the marked seam. NO mutations in this
session — Part B runs them later, in fresh sessions.

**Rev 2 (2026-08-05):** rev 1 asked the implementer to gather the landed
idioms itself; the session spent its lookup budget before writing a line.
The complete function body is now inlined below. **Transcribe it EXACTLY.**
Your judgment is limited to: (a) placement at the named anchors,
(b) running the gate, (c) reporting discrepancies. If the code below
contradicts the tree (a symbol does not exist, an assert fires at gate
time), STOP and report `[SOL DEBUGGER HANDOFF]` — do not adapt, do not
fix, do not substitute your own idiom.

## Implementer contract

- Work ONLY in `packages/forth-core/test_capture.part.h` and
  `packages/forth-core/test_dict_reloc.c`. NEVER edit any production
  file, `patches/`, `files/`, or anything under `src/`.
- Todo file: `/tmp/qwen-l1-5a-todo.md`. Gate log: `/tmp/qwen-l1-5a-gate.log`
  (these names OVERRIDE any remembered form).
- Gate: `./packages/forth-core/build-test.sh > /tmp/qwen-l1-5a-gate.log 2>&1`;
  inspect with bounded `grep -a`. Green iff the log shows
  `FORTH SELF-TEST: ALL PASSED` **and** `BUILD + SELF-TEST GREEN`.
- Literals are law: every item id, string, and number below is normative.
- **Report requirement:** quote the
  `[DEBUG] running test_interactive_acceptance...` line and all seven
  `    [N] PASS:` lines verbatim from the green log, plus the
  `FORTH ARENA` line. The `ALL PASSED` banner alone is not evidence.
- Finish with the commit step at the bottom. One commit, exact message.

## EXECUTION GATE (run these seven, compare counts, STOP on mismatch)

```
grep -c "static int test_interactive_acceptance" packages/forth-core/test_capture.part.h   # expect 0
grep -c "fail |= test_cm_gate_audit();" packages/forth-core/test_dict_reloc.c              # expect 1
grep -c "static int test_cm_gate_audit(void);" packages/forth-core/test_dict_reloc.c       # expect 1
grep -c "forthInteractiveEnter(void);" packages/forth-core/forth_capture.h                 # expect 1
grep -c "runFunction(ITM_COLON);" packages/forth-core/test_capture.part.h                  # expect 3
git status --short | wc -l                                                                  # expect 0
git log --oneline -3 | grep -c "L1-F3: operand-class parity"                               # expect 1
```

## Edit 1 — the test body (APPEND at the very end of `packages/forth-core/test_capture.part.h`)

```c

/* ==================================================================
 * PACKET_L1_5 (C1) — test_interactive_acceptance: the stage story.
 * Steps 1-7 here (session A); steps 8-10 appended at the seam below
 * (session B).  ONE function, ONE story: state flows from step to step
 * deliberately — do not isolate the steps from each other.  Every step
 * drives real entry points only (fnForthOuter, runFunction, fnKeyEnter,
 * fnKeyExit, tamProcessInput).
 * ================================================================== */
static int test_interactive_acceptance(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyEnter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);

  int fail = 0, scFail;
  uint8_t tType; int32_t tVal;
  uint8_t rType; int32_t rVal;
  uint16_t idx;
  longInteger_t li;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  #define L15_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  /* Baseline program isolation (the L1-H lesson): FHIST is created by the
   * story's first ENTER and must land AFTER a real program, so the final
   * cleanup isolates the next test.  Nothing global is modified yet, so
   * the abort path needs no restore. */
  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    tpLbl(&base, "BASEA");
    tpEnd(&base);
    if (!tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }

  /* ---- [1] Open from CM_NORMAL: X bit-identical (T9, no lift). ---- */
  scFail = 0;
  L15_RESET();
  longIntegerInit(li); int32ToLongInteger(7, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [1] FAIL: interactive open did not take (state=%d)\n", forthTestCapState());
    scFail = 1;
  }
  if (!scFail && calcMode != CM_AIM) {
    printf("    [1] FAIL: calcMode %u, expected CM_AIM\n", calcMode);
    scFail = 1;
  }
  if (!scFail && !getSystemFlag(FLAG_ALPHA)) {
    printf("    [1] FAIL: FLAG_ALPHA not set\n");
    scFail = 1;
  }
  if (!scFail) {
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 7) {
      printf("    [1] FAIL: X = %ld type %u, expected untouched 7 (no lift)\n", (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [1] PASS: FORTH from CM_NORMAL opens interactive; X bit-identical (no lift)\n");
  fail |= scFail;

  /* ---- [2] "1 2 +", ENTER: live stack, REPL reopens empty. ---- */
  scFail = 0;
  if (!forthCapIsOpen()) {
    printf("    [2] FIXTURE FAIL: capture not open\n");
    scFail = 1;
  } else {
    runFunction(ITM_1); runFunction(ITM_SPACE);
    runFunction(ITM_2); runFunction(ITM_SPACE);
    runFunction(ITM_PLUS);
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: ENTER errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
    if (!scFail && !x_is_longint(3)) {
      printf("    [2] FAIL: X != 3 after \"1 2 +\"\n");
      scFail = 1;
    }
    if (!scFail && aimBuffer[0] != 0) {
      printf("    [2] FAIL: line not empty after ENTER (\"%s\")\n", aimBuffer);
      scFail = 1;
    }
    if (!scFail && (!forthCapIsOpen() || calcMode != CM_AIM)) {
      printf("    [2] FAIL: REPL did not stay open in CM_AIM\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [2] PASS: \"1 2 +\" ENTER computes 3 on the live stack; REPL reopens empty\n");
  fail |= scFail;

  /* ---- [3] ": SQ DUP * ;" through the key path (landed typing drive). ---- */
  scFail = 0;
  runFunction(ITM_COLON); runFunction(ITM_SPACE);
  runFunction(ITM_S); runFunction(ITM_Q); runFunction(ITM_SPACE);
  runFunction(ITM_D); runFunction(ITM_U); runFunction(ITM_P); runFunction(ITM_SPACE);
  runFunction(ITM_ASTERISK); runFunction(ITM_SPACE);
  runFunction(ITM_SEMICOLON);
  fnKeyEnter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    [3] FAIL: ENTER errored (%d)\n", lastErrorCode);
    scFail = 1;
  }
  if (!scFail && !forthFindColon("SQ", &idx)) {
    printf("    [3] FAIL: SQ did not resolve after the definition\n");
    scFail = 1;
  }
  if (!scFail) printf("    [3] PASS: \": SQ DUP * ;\" defines SQ interactively\n");
  fail |= scFail;

  /* ---- [4] "4 SQ": the interactive definition used from a later line. ---- */
  scFail = 0;
  runFunction(ITM_4); runFunction(ITM_SPACE);
  runFunction(ITM_S); runFunction(ITM_Q);
  fnKeyEnter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    [4] FAIL: ENTER errored (%d)\n", lastErrorCode);
    scFail = 1;
  }
  if (!scFail && !x_is_longint(16)) {
    printf("    [4] FAIL: X != 16 after \"4 SQ\"\n");
    scFail = 1;
  }
  if (!scFail) printf("    [4] PASS: \"4 SQ\" uses the interactive definition; X == 16\n");
  fail |= scFail;

  /* ---- [5] Keys mode: SIN inserts its name; backspace edits natively
   * (items.c's divert exclusion list). ---- */
  scFail = 0;
  runFunction(ITM_AIM);                    /* the ALPHA gesture: alpha -> keys */
  if (!forthCapKeysMode()) {
    printf("    [5] FIXTURE FAIL: keys mode did not arm\n");
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_sin);
    if (compareString(aimBuffer, "SIN ", CMP_BINARY) != 0) {
      printf("    [5] FAIL: line \"%s\", expected \"SIN \"\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) {
    runFunction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
    runFunction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
    if (aimBuffer[0] != 0) {
      printf("    [5] FAIL: line \"%s\" after 4 backspaces, expected empty\n", aimBuffer);
      scFail = 1;
    }
    if (!scFail && !forthCapKeysMode()) {
      printf("    [5] FAIL: keys mode dropped by backspace\n");
      scFail = 1;
    }
    if (!scFail && !forthCapIsOpen()) {
      printf("    [5] FAIL: capture closed by backspace\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [5] PASS: keys mode SIN inserts its name; backspaces clear it\n");
  fail |= scFail;

  /* ---- [6] Keys-mode fold: "STO 0 5" types text; ENTER executes it
   * (L-R4 (b): one gesture, one meaning — text, not action). ---- */
  scFail = 0;
  longIntegerInit(li); int32ToLongInteger(555, li);
  convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);
  runFunction(ITM_STO);
  tamProcessInput(ITM_0);
  tamProcessInput(ITM_5);
  if (tam.mode != 0) {
    printf("    [6] FAIL: tam.mode %d after the fold, expected 0\n", tam.mode);
    scFail = 1;
  }
  if (!scFail && compareString(aimBuffer, "STO 05 ", CMP_BINARY) != 0) {
    printf("    [6] FAIL: line \"%s\", expected \"STO 05 \"\n", aimBuffer);
    scFail = 1;
  }
  if (!scFail) {
    read_reg_int32(5, &rType, &rVal);
    if (rType != dtLongInteger || rVal != 555) {
      printf("    [6] FAIL: register 05 = %ld type %u, expected untouched 555 (fold must not execute)\n",
             (long)rVal, rType);
      scFail = 1;
    }
  }
  if (!scFail) {
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [6] FAIL: ENTER errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
    if (!scFail) {
      read_reg_int32(5, &rType, &rVal);
      if (rType != dtLongInteger || rVal != 16) {
        printf("    [6] FAIL: register 05 = %ld type %u, expected 16 after ENTER\n",
               (long)rVal, rType);
        scFail = 1;
      }
    }
    if (!scFail && !x_is_longint(16)) {
      printf("    [6] FAIL: X != 16 after \"STO 05\"\n");
      scFail = 1;
    }
    if (!scFail && aimBuffer[0] != 0) {
      printf("    [6] FAIL: line not empty after ENTER\n");
      scFail = 1;
    }
    if (!scFail && !forthCapIsOpen()) {
      printf("    [6] FAIL: capture not open after ENTER\n");
      scFail = 1;
    }
    if (!scFail && forthCapKeysMode()) {
      printf("    [6] FAIL: keys mode survived the REPL reopen (E5 relock)\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [6] PASS: keys-mode fold \"STO 05 \" types text, executes only at ENTER (R05 555 -> 16)\n");
  fail |= scFail;

  /* ---- [7] EXIT closes; no string commit (rung 3 never touches X). ---- */
  scFail = 0;
  fnKeyExit(NOPARAM);
  if (forthTestCapState() != FCAP_CLOSED) {
    printf("    [7] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
    scFail = 1;
  }
  if (!scFail && calcMode != CM_NORMAL) {
    printf("    [7] FAIL: calcMode %u, expected CM_NORMAL\n", calcMode);
    scFail = 1;
  }
  if (!scFail && getSystemFlag(FLAG_ALPHA)) {
    printf("    [7] FAIL: FLAG_ALPHA still set\n");
    scFail = 1;
  }
  if (!scFail && !x_is_longint(16)) {
    printf("    [7] FAIL: X changed across EXIT (string commit?)\n");
    scFail = 1;
  }
  if (!scFail) printf("    [7] PASS: EXIT closes; CM_NORMAL, FLAG_ALPHA clear, X still 16, no string commit\n");
  fail |= scFail;

  /* L1-5B: steps 8-10 are appended here by session B. */

  forthCapClose();
  cleanupTestProgram();
  #undef L15_RESET
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}
```

## Edit 2 — forward declaration (`packages/forth-core/test_dict_reloc.c`)

Insert immediately after the line

```c
static int test_cm_gate_audit(void);                           /* L1-F3 */
```

this line:

```c
static int test_interactive_acceptance(void);                 /* L1-5 */
```

## Edit 3 — invocation (`packages/forth-core/test_dict_reloc.c`, outside any `if(fail)`)

Find the block

```c
  forthDictInit();
  printf("  [DEBUG] running test_cm_gate_audit...\n");
  fail |= test_cm_gate_audit();
  forthDictClear();
  forthGDictClear();
```

and insert immediately AFTER it:

```c

  printf("\nFORTH L1-5 TESTS (stage acceptance battery)\n");
  forthDictInit();
  printf("  [DEBUG] running test_interactive_acceptance...\n");
  fail |= test_interactive_acceptance();
  forthDictClear();
  forthGDictClear();
```

## Gate, report, commit

1. Run the gate. Green required. If a test THIS session did not write goes
   red, or any `FIXTURE FAIL` line appears, STOP and report
   `[SOL DEBUGGER HANDOFF]` with the log excerpt — do not adapt.
2. Report: quote the `[DEBUG]` line, all seven PASS lines, and the
   `FORTH ARENA` line verbatim from `/tmp/qwen-l1-5a-gate.log`.
3. Commit exactly:

```
git add packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
git commit -m "L1-5: the stage story, part 1 — open to EXIT (C1 steps 1-7)"
```
