# PACKET L1-5C — the interactive close-path sweep (C2.1)

**Stage L final packet, session C.** Parent: `PACKET_L1_5_acceptance.md`
(C2.1). **Prerequisite: session B landed and green** (commit
`L1-5: the stage story, part 2 — ...`). Adds `test_interactive_close_sweep`
and registers it. **Transcribe the code EXACTLY**; judgment is limited to
placement, gate, report. STOP with `[SOL DEBUGGER HANDOFF]` on any
contradiction with the tree. NO mutations in this session.

## Why this test exists (context, not instructions)

The landed close tests measure two other axes: the PEM four
(`test_capture_close_paths_reset_tuple` — a hardwired CM_PEM switch) and
the fold's seven (`test_fold_close_paths`). The interactive axis has its
own enumeration — SEVEN paths — and until now only three of them were
asserted, on a partial tuple (state, origin, FLAG_ALPHA; keysMode and
foldMode unasserted). This sweep drives the four driveable paths against
the FULL close tuple (state, keysMode, origin, foldMode) and reports the
three that sit behind gestures the harness cannot model:

1. EXIT ladder rung 3 (fnKeyExit, CM_AIM, interactive) — DRIVEN, incl.
   the rung-1 keys-mode unwind and the rung-3 history push.
2. fnKeyUp's closeAim arm — DRIVEN (non-alpha, non-scrolling menu on top).
3. fnKeyDown's closeAim arm — DRIVEN (mirror).
4. `forthCapPowerReset()` — DRIVEN (the dictionary-seam entry itself).
5. executeFunction's ITM_INTEGRAL/ITM_INTEGRAL_YX CM_AIM arm — REPORTED
   (identical `_forthCapCloseIfInteractive(); closeAim();` pair as 2-3).
6. executeFunction's generic non-alpha-item arm — REPORTED (ditto).
7. processKeyAction's BST/SST longpress arm — REPORTED (ditto).

At the closeAim-family sites (2, 3, 5-7) the native `closeAim()` commits
a non-empty line to X as a dtString — the line is PRESERVED IN X, not
pushed to history; that is the KEEP disposition L1-2 recorded (native
behaviour stays native outside the ladder). At rung 3 the line is pushed
to FHIST. At `forthCapPowerReset()` the line is DROPPED — the reset runs
at the dictionary init/restore seams where transient UI state never
survives (the §8 A5 analogue). The sweep asserts each of those three
dispositions where driveable.

## Implementer contract

As PACKET_L1_5A rev 2: only `packages/forth-core/test_capture.part.h`
and `packages/forth-core/test_dict_reloc.c`; todo
`/tmp/qwen-l1-5c-todo.md`; gate log `/tmp/qwen-l1-5c-gate.log` (these
names OVERRIDE any remembered form); green iff
`FORTH SELF-TEST: ALL PASSED` and `BUILD + SELF-TEST GREEN`; quote the
`[DEBUG] running test_interactive_close_sweep...` line, all FIVE PASS
lines, the REPORT line, and the `FORTH ARENA` line verbatim; one commit,
exact message.

## EXECUTION GATE (STOP on mismatch)

```
grep -c "static int test_interactive_close_sweep" packages/forth-core/test_capture.part.h  # expect 0
grep -c "fail |= test_interactive_acceptance();" packages/forth-core/test_dict_reloc.c     # expect 1
grep -c "uint16_t forthCapHistoryIndex" packages/forth-core/forth_capture.h                # expect 1
grep -c "forthCapFoldModeRaw(void);" packages/forth-core/forth_capture.h                   # expect 1
git status --short | wc -l                                                                  # expect 0
git log --oneline -3 | grep -c "the stage story, part 2"                                   # expect 1
```

## Edit 1 — the test body (APPEND at the very end of `packages/forth-core/test_capture.part.h`)

```c

/* ==================================================================
 * PACKET_L1_5 (C2.1) — test_interactive_close_sweep: the interactive
 * close-path axis, full tuple.  Seven paths on this axis (see the
 * keyboard.c banner over _forthCapCloseIfInteractive): four driven
 * below, three reported (multi-step gestures / longpress timing the
 * harness does not model; each calls the identical
 * `_forthCapCloseIfInteractive(); closeAim();` pair the driven sites
 * verify live).  Reported separately from the PEM four
 * (test_capture_close_paths_reset_tuple) and the fold seven
 * (test_fold_close_paths), which own their axes.
 * ================================================================== */
static int test_interactive_close_sweep(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void fnKeyUp(uint16_t);
  extern void fnKeyDown(uint16_t);
  extern void runFunction(int16_t);
  extern void showSoftmenu(int16_t);

  int fail = 0, scFail;

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

  /* The full close tuple, one place.  Returns 0 when fully reset. */
  #define L15C_TUPLE_BAD(tag) ( \
    (forthTestCapState() != FCAP_CLOSED ? (printf("    [%s] FAIL: state %d, expected FCAP_CLOSED\n", tag, forthTestCapState()), 1) : 0) | \
    (forthCapKeysMode()                 ? (printf("    [%s] FAIL: keysMode still set\n", tag), 1) : 0) | \
    (forthTestCapOrigin() != FCAP_ORIGIN_PEM ? (printf("    [%s] FAIL: origin %d, expected FCAP_ORIGIN_PEM\n", tag, forthTestCapOrigin()), 1) : 0) | \
    (forthCapFoldModeRaw() != 0         ? (printf("    [%s] FAIL: foldMode %d, expected 0\n", tag, forthCapFoldModeRaw()), 1) : 0) )

  #define L15C_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  /* Baseline program so FHIST (created by the rung-3 push) lands after a
   * real program and the final cleanup isolates the next test. */
  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    tpLbl(&base, "BASEC");
    tpEnd(&base);
    if (!tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }

  /* ---- [1] EXIT ladder: rung 1 unwinds keys mode (still open), rung 3
   * closes and PUSHES the non-empty line (EXIT never loses a line). ---- */
  scFail = 0;
  L15C_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [1] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_1);                /* the line: "1" */
    runFunction(ITM_AIM);              /* arm keys mode */
    if (!forthCapKeysMode()) {
      printf("    [1] FIXTURE FAIL: keys mode did not arm\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    fnKeyExit(NOPARAM);                /* rung 1: keys -> alpha */
    if (!forthCapIsOpen() || forthCapKeysMode()) {
      printf("    [1] FAIL: rung 1 should leave the capture OPEN in alpha (open=%d keys=%d)\n",
             forthCapIsOpen(), forthCapKeysMode());
      scFail = 1;
    }
  }
  if (!scFail) {
    fnKeyExit(NOPARAM);                /* rung 3: close, push "1" */
    if (L15C_TUPLE_BAD("1")) { scFail = 1; }
    if (!scFail && getSystemFlag(FLAG_ALPHA)) {
      printf("    [1] FAIL: FLAG_ALPHA still set after rung 3\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    uint16_t prog = forthHistoryProgram();
    uint8_t *lbl = prog ? programList[prog - 1].instructionPointer : NULL;
    uint8_t *s1 = lbl ? findNextStep(lbl) : NULL;
    uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
    if (prog == 0 || !s1 || !s2 || !stepSrcTextEq(s1, "1") || !isAtEndOfProgram(s2)) {
      printf("    [1] FAIL: rung 3 did not push the abandoned line onto FHIST\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [1] PASS: EXIT ladder — rung 1 unwinds keys mode, rung 3 closes with the full tuple and pushes the line\n");
  fail |= scFail;

  /* ---- [2] fnKeyUp's closeAim arm: full tuple; the native commit
   * PRESERVES the line in X as a string (KEEP disposition, L1-2). ---- */
  scFail = 0;
  L15C_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [2] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_2);                /* the line: "2" */
    runFunction(ITM_AIM);              /* arm keys mode (tuple must reset it) */
    if (!forthCapKeysMode()) {
      printf("    [2] FIXTURE FAIL: keys mode did not arm\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    showSoftmenu(-MNU_HOME);           /* non-alpha, non-scrolling top (landed
                                          precondition from
                                          test_capture_interactive_close) */
    fnKeyUp(NOPARAM);
    if (L15C_TUPLE_BAD("2")) { scFail = 1; }
    if (!scFail && getRegisterDataType(REGISTER_X) != dtString) {
      printf("    [2] FAIL: X type %u, expected dtString (closeAim's native commit preserves the line)\n",
             getRegisterDataType(REGISTER_X));
      scFail = 1;
    }
  }
  if (!scFail) printf("    [2] PASS: fnKeyUp's closeAim arm resets the full tuple; the line lands in X natively\n");
  fail |= scFail;

  /* ---- [3] fnKeyDown's closeAim arm: mirror of [2]. ---- */
  scFail = 0;
  L15C_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [3] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_3);
    runFunction(ITM_AIM);
    if (!forthCapKeysMode()) {
      printf("    [3] FIXTURE FAIL: keys mode did not arm\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    showSoftmenu(-MNU_HOME);
    fnKeyDown(NOPARAM);
    if (L15C_TUPLE_BAD("3")) { scFail = 1; }
    if (!scFail && getRegisterDataType(REGISTER_X) != dtString) {
      printf("    [3] FAIL: X type %u, expected dtString (closeAim's native commit preserves the line)\n",
             getRegisterDataType(REGISTER_X));
      scFail = 1;
    }
  }
  if (!scFail) printf("    [3] PASS: fnKeyDown's closeAim arm resets the full tuple; the line lands in X natively\n");
  fail |= scFail;

  /* ---- [4] forthCapPowerReset(): the dictionary-seam reset.  The line
   * is DROPPED (transient UI state never survives the seams — the §8 A5
   * analogue); the browse index resets with it. ---- */
  scFail = 0;
  L15C_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [4] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_4);
    runFunction(ITM_AIM);
    if (!forthCapKeysMode()) {
      printf("    [4] FIXTURE FAIL: keys mode did not arm\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    forthCapPowerReset();
    if (L15C_TUPLE_BAD("4")) { scFail = 1; }
    if (!scFail && forthCapHistoryIndex() != FORTH_HIST_BROWSE_NONE) {
      printf("    [4] FAIL: historyIndex %u, expected FORTH_HIST_BROWSE_NONE\n",
             forthCapHistoryIndex());
      scFail = 1;
    }
  }
  if (!scFail) printf("    [4] PASS: forthCapPowerReset resets the full tuple and the browse index\n");
  fail |= scFail;

  /* ---- [5] The three inspection-only paths, reported not driven. ---- */
  printf("    [5] REPORT: 3 further interactive close paths sit behind gestures this harness\n"
         "        does not model and are verified by inspection (identical\n"
         "        _forthCapCloseIfInteractive(); closeAim(); pair as [2]/[3]):\n"
         "        executeFunction ITM_INTEGRAL/ITM_INTEGRAL_YX arm; executeFunction\n"
         "        generic non-alpha-item arm; processKeyAction BST/SST longpress arm.\n");
  printf("    [5] REPORT: interactive close-path axis: 7 paths — 4 driven above, 3 by\n"
         "        inspection.  Separate axes: the PEM four\n"
         "        (test_capture_close_paths_reset_tuple), the fold seven\n"
         "        (test_fold_close_paths).\n");
  printf("    [5] PASS: axis enumerated — 4 driven with the full tuple, 3 inspection-only, counted apart from the PEM four\n");

  forthCapClose();
  cleanupTestProgram();
  #undef L15C_TUPLE_BAD
  #undef L15C_RESET
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

## Edit 2 — forward declaration (`test_dict_reloc.c`)

Insert immediately after
`static int test_interactive_acceptance(void);                 /* L1-5 */`:

```c
static int test_interactive_close_sweep(void);                /* L1-5 */
```

## Edit 3 — invocation (`test_dict_reloc.c`)

Find the block

```c
  printf("  [DEBUG] running test_interactive_acceptance...\n");
  fail |= test_interactive_acceptance();
  forthDictClear();
  forthGDictClear();
```

and insert immediately AFTER it:

```c

  forthDictInit();
  printf("  [DEBUG] running test_interactive_close_sweep...\n");
  fail |= test_interactive_close_sweep();
  forthDictClear();
  forthGDictClear();
```

## Gate, report, commit

Gate green required; STOP on any red outside this session's writes or
any FIXTURE FAIL. Report the `[DEBUG]` line, PASS lines `[1]`-`[5]`, both
REPORT lines, and the `FORTH ARENA` line verbatim. Commit exactly:

```
git add packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
git commit -m "L1-5: the interactive close-path sweep — 7 paths, full tuple (C2.1)"
```
