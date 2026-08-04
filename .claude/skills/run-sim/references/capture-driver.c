/* BEGIN TEMP-LCD-CAPTURE (delete this whole block before commit) */
/* LCD capture driver — proven 2026-08-03 (forum/screenshots/, commit
 * 4022c5657). Copy this ENTIRE file into
 * packages/forth-core/test_capture.part.h at file END, register per
 * QWEN_TEMPLATE_LCD_CAPTURE.md, and DELETE both edits after capture:
 * this never lands in a commit (run-sim skill rule 3).
 *
 * The BEGIN marker on line 1 and the END marker on the last line are
 * the removal anchors — everything in this file sits between them.
 *
 * Structure:
 *   - save/restore of every display/program global the renders touch
 *     (MACHINERY — keep as is)
 *   - a tp* program fixture (ADAPT — this copy carries the FDEMO+SAVE
 *     showcase; replace with the packet's fixture)
 *   - three render idioms, each ending in fnScreenDump(0):
 *       PEM listing at a step   (F15-3 idiom)
 *       FWRD picker at cursor   (G3/G4 idiom)
 *       normal screen after a line (CM_NORMAL refresh)
 *     (ADAPT the shot list; keep each idiom's line order)
 *
 * Hard-won facts encoded below, do not "simplify" them away:
 *   - screenUpdatingMode: an earlier battery test leaves it at 7
 *     (manual statusbar/stack/menu), which makes refreshScreen draw
 *     NOTHING but the date. Force SCRUPD_AUTO before a normal-screen
 *     shot. Cost a blank-frame debugging round on 2026-08-03.
 *   - lcd_clear_buf() is c47-gtk-only and this file compiles into BOTH
 *     binaries; clearScreen(0) is the clear that links everywhere.
 *   - never refreshScreen() over hand-set PEM state; reach it through
 *     fnGotoDot + defineFirstDisplayedStep + fnPem as below.
 *   - step numbers are never hand-counted: the closing-marker walk
 *     derives the fnGotoDot argument from the tp handle.
 */
static int tempLcdCapture(void)
{
  extern void fnGotoDot(uint16_t globalStepNumber);
  extern void defineFirstDisplayedStep(void);
  extern void fnPem(uint16_t param);
  extern void fnScreenDump(uint16_t unused);
  extern void showSoftmenu(int16_t menu);
  extern void showSoftmenuCurrentPart(void);
  extern void refreshScreen(uint16_t source);

  int fail = 0;
  testProg_t p;
  char indirectRegister[64];
  char indirectNamed[64];
  int hClose = -1;
  calcRegister_t lbl;

  uint8_t *savedCurrentStep = currentStep;
  uint16_t savedProgNum = currentProgramNumber;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint8_t *savedFirstDisplayedStep = firstDisplayedStep;
  uint16_t savedFirstDisplayedLocal = firstDisplayedLocalStepNumber;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint8_t savedCalcMode = calcMode;
  uint8_t savedRS = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint16_t savedNamedVars = numberOfNamedVariables;
  bool_t savedFlag10 = getFlag(10);
  int16_t savedScreenUpdatingMode = screenUpdatingMode;
  int16_t savedTempInfo = temporaryInformation;
  int16_t savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent = dynamicSoftmenu[22].menuContent;
  int16_t savedNumItems = dynamicSoftmenu[22].numItems;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems = 0;

  /* ---- FIXTURE (adapt per packet) — this copy: the FDEMO showcase ---- */
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
      (hClose = tpMarker(&p)) < 0 ||
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
    printf("    SHOT FIXTURE FAIL: build/write\n");
    fail = 1;
    goto cleanup;
  }

  /* ---- Idiom 1: PEM listing at a step (F15-3) ---- */
  clearScreen(0);
  calcMode = CM_PEM;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  clearSystemFlag(FLAG_ALPHA);
  pemCursorIsZerothStep = false;
  programRunStop = PGM_STOPPED;
  lastErrorCode = ERROR_NONE;
  fnGotoDot(3);
  firstDisplayedLocalStepNumber = 0;
  defineFirstDisplayedStep();
  fnPem(NOPARAM);
  fnScreenDump(0);
  printf("    SHOT 1: PEM listing dumped\n");

  /* ---- Idiom 2: FWRD picker at the cursor (G3/G4); step number is
   * DERIVED from the tp handle, never hand-counted ---- */
  {
    uint8_t *closeAddr = tpStepAddr(&p, hClose);
    uint8_t *s = beginOfProgramMemory;
    uint16_t n = 1;
    while (s != NULL && s != closeAddr && n < 2000) { s = findNextStep(s); n++; }
    if (closeAddr == NULL || s != closeAddr) {
      printf("    SHOT 2 SKIP: marker not located by step walk\n");
    } else {
      clearScreen(0);
      fnGotoDot(n);
      firstDisplayedLocalStepNumber = 0;
      defineFirstDisplayedStep();
      fnPem(NOPARAM);
      showSoftmenu(-MNU_FORTH);
      showSoftmenuCurrentPart();
      fnScreenDump(0);
      printf("    SHOT 2: PEM tail + FWRD picker dumped (global step %u, %d words)\n",
             n, dynamicSoftmenu[22].numItems);
    }
  }

  /* ---- Idiom 3: normal screen after running the program ---- */
  calcMode = CM_NORMAL;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  lastErrorCode = ERROR_NONE;
  lbl = findNamedLabel("FDEMO", GLOBAL_LABELS);
  if (lbl == INVALID_VARIABLE) {
    printf("    SHOT FAIL: label not found\n");
    fail = 1;
    goto cleanup;
  }
  fnExecute(lbl);
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: run error %d\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }
  forthOuterInterpret("XEQ 'CLSTK' RCL 05 RCL 06 RCL 00");
  if (lastErrorCode != ERROR_NONE) {
    printf("    SHOT FAIL: result recall error %d\n", lastErrorCode);
    fail = 1;
    goto cleanup;
  }
  /* screenUpdatingMode is often 7 (manual) here from an earlier battery
   * test — with it, refreshScreen draws only the date. Force AUTO. */
  screenUpdatingMode = SCRUPD_AUTO;
  temporaryInformation = TI_NO_INFO;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  cachedDynamicMenu = savedCachedDynamicMenu;
  clearScreen(0);
  refreshScreen(9901);
  fnScreenDump(0);
  printf("    SHOT 3: run results dumped\n");

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
  lastErrorCode = ERROR_NONE;
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems = savedNumItems;
  cachedDynamicMenu = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  calcMode = savedCalcMode;
  programRunStop = savedRS;
  dynamicMenuItem = savedDynamicMenu;
  currentStep = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  currentLocalStepNumber = savedLocalStep;
  firstDisplayedStep = savedFirstDisplayedStep;
  firstDisplayedLocalStepNumber = savedFirstDisplayedLocal;
  pemCursorIsZerothStep = savedZeroth;
  screenUpdatingMode = savedScreenUpdatingMode;
  temporaryInformation = savedTempInfo;
  if (!fail) {
    printf("    PASS: LCD captures dumped\n");
  }
  return fail;
}
/* END TEMP-LCD-CAPTURE */
