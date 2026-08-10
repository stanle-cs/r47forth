/* BEGIN TEMP-LCD-CAPTURE (delete this whole block before commit) */
/* Console-interaction capture driver — proven 2026-08-10 (SAVE program
 * + history scrolling session, 9 shots).  Copy this ENTIRE file into
 * packages/forth-core/test_capture.part.h at file END, register per
 * the pattern below, and DELETE both edits after capture:
 * this never lands in a commit (run-sim skill rule 3).
 *
 * Registration in test_dict_reloc.c:
 *   1. Forward declaration beside the neighbouring statics:
 *        static int tempLcdCapture(void); // TEMP-LCD-CAPTURE
 *   2. Call beside the neighbouring fail |= lines (after the
 *      forthConsoleClear/forthDictClear/forthGDictClear block,
 *      before the stale-list tripwire):
 *        printf("\nTEMP LCD CAPTURE\n"); // TEMP-LCD-CAPTURE
 *        fail |= tempLcdCapture(); // TEMP-LCD-CAPTURE
 *
 * Removal: line-delete every TEMP-LCD-CAPTURE token in
 * test_dict_reloc.c, range-delete the BEGIN/END block in
 * test_capture.part.h, verify `git diff --stat` on both files
 * is EMPTY, delete the BMPs, and re-run the full gate green.
 *
 * Structure:
 *   - Inline helpers (_TLC_RUN, _TLC_SHOT, _TLC_FULL_SHOT) because
 *     _consoleRunLine/N13_RESET/etc live in test_console.part.h which
 *     is included AFTER test_capture.part.h
 *   - Console open + line-by-line word definitions (ADAPT the lines)
 *   - Execution + transcript capture (console-band only)
 *   - Full-screen capture (shows input line, softkey bar, status)
 *   - FHIST history scroll back/forward with full-screen shots
 *   - Save/restore of all touched global state
 *
 * This driver captures TWO kinds of shot:
 *   _TLC_SHOT()      — console transcript band only (no status bar,
 *                       no softkeys, no input line)
 *   _TLC_FULL_SHOT() — full refreshScreen, shows everything including
 *                       the AIM editor input line and FWRD picker
 *
 * Hard-won facts encoded below:
 *   - _consoleRunLine is defined in test_console.part.h (included
 *     after test_capture.part.h), so we inline its body as _TLC_RUN.
 *   - _forthConsoleRender is extern (defined in the main source),
 *     callable from test_capture.part.h via a function pointer.
 *   - History recall changes aimBuffer but NOT the transcript, so
 *     only full-screen shots show the scrolled-to line.
 *   - screenUpdatingMode must be forced to SCRUPD_AUTO before every
 *     shot (the same trap as capture-driver.c).
 *   - Named variables created during the session must be freed in
 *     cleanup or the suite's leak gate fails.
 */

/* Inline helpers — _consoleRunLine/N13_RESET live in test_console.part.h
 * which is included after this file. */
#define _TLC_RUN(src) do { \
  int32_t _n = stringByteLength((char *)(src)); \
  xcopy(aimBuffer, (src), (uint32_t)_n + 1); \
  T_cursorPos = (int16_t)_n; \
  forthInteractiveRun(); \
} while (0)

#define _TLC_SHOT() do { \
  screenUpdatingMode = SCRUPD_AUTO; \
  temporaryInformation = TI_NO_INFO; \
  clearScreen(0); \
  _tlcConsoleRender(); \
  fnScreenDump(0); \
} while (0)

#define _TLC_FULL_SHOT() do { \
  screenUpdatingMode = SCRUPD_AUTO; \
  temporaryInformation = TI_NO_INFO; \
  clearScreen(0); \
  _tlcRefreshScreen(9901); \
  fnScreenDump(0); \
} while (0)

static int tempLcdCapture(void)
{
  extern void fnScreenDump(uint16_t unused);
  extern void refreshScreen(uint16_t source);
  extern void _forthConsoleRender(void);
  void (*_tlcConsoleRender)(void) = _forthConsoleRender;
  void (*_tlcRefreshScreen)(uint16_t) = refreshScreen;

  int fail = 0;
  char putLine[96];

  int16_t savedScreenUpdatingMode = screenUpdatingMode;
  int16_t savedTempInfo = temporaryInformation;
  uint8_t savedCalcMode = calcMode;
  uint8_t savedRS = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCachedDynamicMenu = cachedDynamicMenu;
  uint16_t savedNamedVars = numberOfNamedVariables;
  bool_t savedFlag10 = getFlag(10);
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  forthDictInit();
  forthGDictInit();

  /* ---- FIXTURE (adapt per packet) — this copy: the SAVE showcase ---- */
  snprintf(putLine, sizeof(putLine), ": PUT STO %s20 ;", STD_RIGHT_ARROW);

  /* Reset and open the console */
  calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0;
  programRunStop = PGM_STOPPED; dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE; forthCapClose(); forthConsoleClear();

  fnForthOuter(NOPARAM);
  forthCapSetKeysMode(false);
  forthConsoleClear();

  /* Key in the SAVE program, line by line */
  _TLC_RUN(": GROW 1.05 * ; GLOBAL");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: GROW definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  _TLC_RUN(putLine);
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: PUT definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  _TLC_RUN(": BUMP RCL 20 1 + STO 20 DROP ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: BUMP definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  _TLC_RUN(": TALLY RCL 19 1 - STO 19 DROP ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: TALLY definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  _TLC_RUN(": STEP GROW PUT BUMP TALLY ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: STEP definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  _TLC_RUN(": RUN BEGIN RCL 19 WHILE STEP REPEAT ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: RUN definition error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  /* SHOT 1: console after all definitions */
  _TLC_SHOT();
  printf("    SHOT 1: console after SAVE word definitions\n");

  /* Initialize registers and run */
  _TLC_RUN("XEQ 'CLSTK'");
  _TLC_RUN("0 STO 20 DROP");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: STO 20 error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }
  _TLC_RUN("6 STO 19 DROP");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: STO 19 error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }
  _TLC_RUN("1000 RUN STO 22");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: 1000 RUN error %d\n", lastErrorCode);
    fail = 1; goto cleanup;
  }

  /* SHOT 2: console after running */
  _TLC_SHOT();
  printf("    SHOT 2: console after SAVE program execution\n");

  /* Print stack with .S */
  _TLC_RUN(".S");

  /* SHOT 3: console showing .S */
  _TLC_SHOT();
  printf("    SHOT 3: console after .S\n");

  /* Recall R22 */
  _TLC_RUN("RCL 22");

  /* SHOT 4: full screen showing RCL 22 result */
  _TLC_FULL_SHOT();
  printf("    SHOT 4: full screen showing RCL 22 result\n");

  {
    uint8_t tX;
    int32_t vX;
    read_reg_int32(REGISTER_X, &tX, &vX);
    printf("    R22 (X): type=%d\n", (int)tX);
  }

  /* ---- History scrolling (full screen to see the input line) ---- */
  forthHistoryRecall(-1);
  _TLC_FULL_SHOT();
  printf("    SHOT 5: history recall -1, aimBuffer=\"%s\"\n", aimBuffer);

  forthHistoryRecall(-1);
  _TLC_FULL_SHOT();
  printf("    SHOT 6: history recall -2, aimBuffer=\"%s\"\n", aimBuffer);

  forthHistoryRecall(-1);
  _TLC_FULL_SHOT();
  printf("    SHOT 7: history recall -3, aimBuffer=\"%s\"\n", aimBuffer);

  forthHistoryRecall(+1);
  _TLC_FULL_SHOT();
  printf("    SHOT 8: history recall +1 (forward), aimBuffer=\"%s\"\n", aimBuffer);

  forthHistoryRecall(+1);
  _TLC_FULL_SHOT();
  printf("    SHOT 9: history recall +2 (forward), aimBuffer=\"%s\"\n", aimBuffer);

  printf("    PASS: all shots captured\n");

cleanup:
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  forthConsoleClear();
  forthDictClear();
  forthGDictClear();
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
  if (savedFlag10) fnSetFlag(10); else fnClearFlag(10);
  calcMode = savedCalcMode;
  programRunStop = savedRS;
  dynamicMenuItem = savedDynamicMenu;
  screenUpdatingMode = savedScreenUpdatingMode;
  temporaryInformation = savedTempInfo;
  cachedDynamicMenu = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (!fail) {
    printf("    PASS: LCD captures dumped\n");
  }
  return fail;
}
/* END TEMP-LCD-CAPTURE */
