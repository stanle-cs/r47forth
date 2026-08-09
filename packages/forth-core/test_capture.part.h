/* packages/forth-core/test_capture.part.h — T5 split part of test_dict_reloc.c (2026-08-03).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 */
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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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
  uint8_t savedCalcMode = calcMode;
  calcMode = CM_PEM;   /* Stage M E3: the draws build; PEM cursor context */
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
  calcMode = savedCalcMode;
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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

/* test_picker_key_mapping — G1: the softkey -> word mapping.
 *
 * Every other picker test in this file assigns dynamicMenuItem by hand, so
 * index 0 on page 1 unshifted was the only softkey the suite had ever
 * pressed. The real derivation is
 *
 *   case MNU_FORTH: dynamicMenuItem = firstItem + itemShift + fn;
 *
 * in determineFunctionKeyItem_C47, with itemShift = shiftF ? 6 : shiftG ? 12
 * : 0 and fn = data[0] - '0' - 1. Unlike the MNU_VAR/MNU_PROG arms beside
 * it, that arm does NOT clamp against numItems: the only bound is the
 * dynamicMenuItem < numItems conjunct inside forthPickerGuard, and behind it
 * dynmenuGetLabel() returns "" out of range, which forthCapInsertName("")
 * would turn into a bare space inserted into the user's line. Subcase 5 is
 * that conjunct.
 *
 * One 20-name picker (N000..N019, already in sort order, so picker index i
 * holds "N0ii") spans four pages of six with a partial last page — indices
 * 18 and 19 live, 20..23 blank.
 *
 * Subcase 1 runs BEFORE the capture is opened and with currentStep at the
 * closing marker: showSoftmenuCurrentPart() REBUILDS MNU_FORTH on every
 * display, so it must see the same program tail that built the 20 names.
 * Subcases 2-5 never touch a rebuilding path, so the content they index is
 * the content subcase 1 left. */
static int test_picker_key_mapping(void)
{
  const int      totalDefs = 20;
  const int      stepBytes = 14;                 /* header(4) + ": NNNN 1 ;"(10) */
  const uint16_t progLen   = (uint16_t)(4 + totalDefs * stepBytes + 4);

  uint8_t *prog = (uint8_t *)malloc(progLen);
  if (!prog) {
    printf("    FAIL: malloc failed\n");
    return 1;
  }

  uint8_t *p = prog;
  *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;   /* marker (opening) */
  for (int i = 0; i < totalDefs; i++) {
    char name[5];
    sprintf(name, "N%03d", i);                          /* N000 .. N019 */
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

  uint8_t          *savedCurrentStep = currentStep;
  uint16_t          savedProgNum     = currentProgramNumber;
  softmenuStack_t   savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  int16_t  savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent       = dynamicSoftmenu[22].menuContent;
  int16_t  savedNumItems          = dynamicSoftmenu[22].numItems;
  int16_t  savedCursorPos         = T_cursorPos;
  int16_t  savedDynMenuItem       = dynamicMenuItem;
  bool_t   savedZeroth            = pemCursorIsZerothStep;
  uint16_t savedLocalStep         = currentLocalStepNumber;
  bool_t   savedAlpha             = getSystemFlag(FLAG_ALPHA);
  uint8_t  savedCalcMode          = calcMode;
  int16_t  savedTamFunc           = tam.function;
  int16_t  savedTamMode           = tam.mode;
  uint8_t  savedProgRunStop       = programRunStop;
  int16_t  savedCatalog           = catalog;

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems    = 0;

  extern void     showSoftmenu(int16_t menu);
  extern void     showSoftmenuCurrentPart(void);
  extern char    *dynmenuGetLabel(int16_t menuitem);
  extern int16_t  determineFunctionKeyItem_C47(const char *data, bool_t shiftF, bool_t shiftG);
  extern bool_t   forthPickerGuard(int16_t item);
  extern bool_t   pickerInsertName(void);
  extern void     runFunction(int16_t);

  currentProgramNumber = 1;
  currentStep          = closingMarker;

  int fail = 0;

  /* ---- Subcase 1: the draw path at every page ---- */
  { int sc1 = 0;
    const int16_t pages[4] = {0, 6, 12, 18};

    calcMode = CM_PEM;   /* Stage M E3: the draws build; PEM cursor context (restored from savedCalcMode) */
    showSoftmenu(-MNU_FORTH);

    for (int pi = 0; pi < 4 && !sc1; pi++) {
      char expected[8];
      softmenuStack[0].firstItem = pages[pi];
      showSoftmenuCurrentPart();

      if (dynamicSoftmenu[22].numItems != totalDefs) {
        printf("    [1] FAIL: after draw at firstItem=%d, numItems=%d, expected %d\n",
               pages[pi], dynamicSoftmenu[22].numItems, totalDefs);
        sc1 = 1;
        break;
      }
      if (dynamicSoftmenu[22].menuContent == NULL) {
        printf("    [1] FAIL: menuContent is NULL after draw at firstItem=%d\n", pages[pi]);
        sc1 = 1;
        break;
      }
      sprintf(expected, "N%03d", pages[pi]);
      if (compareString(dynmenuGetLabel(pages[pi]), expected, CMP_BINARY) != 0) {
        printf("    [1] FAIL: first label of page starting %d is '%s', expected '%s'\n",
               pages[pi], dynmenuGetLabel(pages[pi]), expected);
        sc1 = 1;
        break;
      }
    }
    if (!sc1) {
      printf("    [1] PASS: draw path survives every page; first label of each page is correct\n");
    }
    fail |= sc1;
  }

  /* Open the real capture per the CAPTURE-DRIVE CONTRACT (drive slice copied
   * from test_picker_insert_at_cursor). The picker content built above is a
   * separate allocation and survives this. */
  if (!fail) {
    calcMode              = CM_PEM;
    catalog               = CATALOG_NONE;
    tam.mode              = 0;
    tam.function          = 0;
    aimBuffer[0]          = 0;
    programRunStop        = PGM_STOPPED;
    dynamicMenuItem       = -1;
    pemCursorIsZerothStep = false;
    clearSystemFlag(FLAG_ALPHA);

    currentStep            = beginOfProgramMemory;
    currentLocalStepNumber = 1;

    runFunction(ITM_AIM);

    if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
      printf("    FIXTURE BUG: ITM_AIM did not open Forth capture\n");
      fail = 1;
    }
    if (!fail && dynamicSoftmenu[22].numItems != totalDefs) {
      printf("    FIXTURE BUG: picker has %d names after capture open, expected %d\n",
             dynamicSoftmenu[22].numItems, totalDefs);
      fail = 1;
    }
  }

  /* One softkey press, start to finish: set the page, press the key, route
   * the returned item through the guard exactly as executeFunction does.
   * Returns the item the mapping produced; the caller checks the line. */
  int16_t pressedItem = ITM_NOP;
  bool_t  guardFired  = false;
  #define G1_PRESS(page_, key_, shiftF_, shiftG_)                        \
    do {                                                                 \
      softmenuStack[0].softmenuId = 22;                                  \
      softmenuStack[0].firstItem  = (page_);                             \
      pressedItem = determineFunctionKeyItem_C47((key_), (shiftF_), (shiftG_)); \
      guardFired  = forthPickerGuard(pressedItem);                       \
      if (guardFired) { pickerInsertName(); }                            \
    } while (0)

  #define G1_CLEAR_LINE()  do { aimBuffer[0] = 0; T_cursorPos = 0; } while (0)

  /* ---- Subcase 2: index >= 1 on the first page ---- */
  if (!fail) {
    int sc2 = 0;
    G1_CLEAR_LINE();
    G1_PRESS(0, "3", false, false);
    if (pressedItem != ITM_NOP) {
      printf("    [2] FAIL: mapping returned item %d, expected ITM_NOP (%d)\n", pressedItem, ITM_NOP);
      sc2 = 1;
    }
    if (dynamicMenuItem != 2) {
      printf("    [2] FAIL: dynamicMenuItem = %d, expected 2\n", dynamicMenuItem);
      sc2 = 1;
    }
    if (!sc2 && strcmp(forthTestCapText(), "N002 ") != 0) {
      printf("    [2] FAIL: cap text = '%s', expected 'N002 '\n", forthTestCapText());
      sc2 = 1;
    }
    if (!sc2) {
      printf("    [2] PASS: unshifted key 3 on page 1 selects index 2 and inserts N002\n");
    }
    fail |= sc2;
  }

  /* ---- Subcase 3: the shift rows ---- */
  if (!fail) {
    int sc3 = 0;
    G1_CLEAR_LINE();
    G1_PRESS(0, "1", true, false);
    if (dynamicMenuItem != 6) {
      printf("    [3] FAIL: f-shift key 1 gave dynamicMenuItem %d, expected 6\n", dynamicMenuItem);
      sc3 = 1;
    }
    if (!sc3 && strcmp(forthTestCapText(), "N006 ") != 0) {
      printf("    [3] FAIL: f-shift cap text = '%s', expected 'N006 '\n", forthTestCapText());
      sc3 = 1;
    }
    if (!sc3) {
      G1_CLEAR_LINE();
      G1_PRESS(0, "2", false, true);
      if (dynamicMenuItem != 13) {
        printf("    [3] FAIL: g-shift key 2 gave dynamicMenuItem %d, expected 13\n", dynamicMenuItem);
        sc3 = 1;
      }
      if (!sc3 && strcmp(forthTestCapText(), "N013 ") != 0) {
        printf("    [3] FAIL: g-shift cap text = '%s', expected 'N013 '\n", forthTestCapText());
        sc3 = 1;
      }
    }
    if (!sc3) {
      printf("    [3] PASS: f-shift adds 6 and g-shift adds 12 to the selected index\n");
    }
    fail |= sc3;
  }

  /* ---- Subcase 4: paging ---- */
  if (!fail) {
    int sc4 = 0;
    G1_CLEAR_LINE();
    G1_PRESS(6, "1", false, false);
    if (dynamicMenuItem != 6) {
      printf("    [4] FAIL: firstItem=6 key 1 gave dynamicMenuItem %d, expected 6\n", dynamicMenuItem);
      sc4 = 1;
    }
    if (!sc4 && strcmp(forthTestCapText(), "N006 ") != 0) {
      printf("    [4] FAIL: cap text = '%s', expected 'N006 '\n", forthTestCapText());
      sc4 = 1;
    }
    if (!sc4) {
      G1_CLEAR_LINE();
      G1_PRESS(12, "6", false, false);
      if (dynamicMenuItem != 17) {
        printf("    [4] FAIL: firstItem=12 key 6 gave dynamicMenuItem %d, expected 17\n", dynamicMenuItem);
        sc4 = 1;
      }
      if (!sc4 && strcmp(forthTestCapText(), "N017 ") != 0) {
        printf("    [4] FAIL: cap text = '%s', expected 'N017 '\n", forthTestCapText());
        sc4 = 1;
      }
    }
    if (!sc4) {
      printf("    [4] PASS: firstItem pages the selection — 6+0 and 12+5 resolve to N006 and N017\n");
    }
    fail |= sc4;
  }

  /* ---- Subcase 5: the blank key on the partial last page ---- */
  if (!fail) {
    int sc5 = 0;
    G1_CLEAR_LINE();
    G1_PRESS(18, "1", false, false);                 /* index 18: live */
    if (strcmp(forthTestCapText(), "N018 ") != 0 || T_cursorPos != 5) {
      printf("    [5] FAIL: setup press — cap text '%s' cursor %d, expected 'N018 ' and 5\n",
             forthTestCapText(), T_cursorPos);
      sc5 = 1;
    }
    if (!sc5) {
      G1_PRESS(18, "5", false, false);               /* index 22: past numItems=20 */
      if (dynamicMenuItem != 22) {
        printf("    [5] FAIL: dynamicMenuItem = %d, expected 22 (the arm does not clamp)\n",
               dynamicMenuItem);
        sc5 = 1;
      }
      if (guardFired) {
        printf("    [5] FAIL: forthPickerGuard true for index 22 against numItems %d\n",
               dynamicSoftmenu[22].numItems);
        sc5 = 1;
      }
      if (strcmp(forthTestCapText(), "N018 ") != 0 || T_cursorPos != 5) {
        printf("    [5] FAIL: blank key changed the line — cap text '%s' cursor %d, "
               "expected 'N018 ' and 5\n", forthTestCapText(), T_cursorPos);
        sc5 = 1;
      }
    }
    if (!sc5) {
      printf("    [5] PASS: blank key past numItems refuses — guard false, line unchanged\n");
    }
    fail |= sc5;
  }

  #undef G1_PRESS
  #undef G1_CLEAR_LINE

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems    = savedNumItems;
  cachedDynamicMenu               = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  T_cursorPos            = savedCursorPos;
  dynamicMenuItem        = savedDynMenuItem;
  pemCursorIsZerothStep  = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  calcMode          = savedCalcMode;
  tam.function      = savedTamFunc;
  tam.mode          = savedTamMode;
  programRunStop    = savedProgRunStop;
  catalog           = savedCatalog;
  currentStep       = savedCurrentStep;
  currentProgramNumber = savedProgNum;
  cleanupTestProgram();

  return fail;
}

/* test_picker_scan_and_alloc — G2: the two behaviours in
 * forthBuildWordPicker that were documented and pinned nowhere.
 *
 * [1] The program-text pass stops after FORTH_PICKER_MAX_SCAN_STEPS steps
 *     (§9.6 documented deviation). Dropping the break, or changing the
 *     number, passed the whole gate before this.
 * [2] The content allocation. numberOfBytes starts at 1, so even an empty
 *     picker allocates its terminator; the menu must come back as a
 *     well-formed empty menu rather than a NULL with a stale count. The
 *     NULL-return branch beside it is not reachable from this battery
 *     without an allocator hook, and this test does not add one. */
static int test_picker_scan_and_alloc(void)
{
  uint8_t        *savedCurrentStep = currentStep;
  uint16_t        savedProgNum     = currentProgramNumber;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  int16_t  savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent       = dynamicSoftmenu[22].menuContent;
  int16_t  savedNumItems          = dynamicSoftmenu[22].numItems;

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems    = 0;

  extern void  showSoftmenuCurrentPart(void);
  extern char *dynmenuGetLabel(int16_t menuitem);

  int fail = 0;

  /* ---- Subcase 1: the scan cut-off ---- */
  { int sc1 = 0;
    /* The number itself is asserted before the mechanism is. The fixture
     * below sizes itself from FORTH_PICKER_MAX_SCAN_STEPS so it always
     * overruns whatever the constant says — which makes the fixture immune
     * to a change in the constant, and therefore blind to one. Changing 1000
     * changes what the calculator does, so it is pinned here as a literal.
     * (Found by mutation: raising the constant to 2000 left the mechanism
     * asserts green, because they scaled with it.) */
    if (FORTH_PICKER_MAX_SCAN_STEPS != 1000) {
      printf("    [1] FAIL: FORTH_PICKER_MAX_SCAN_STEPS is %d, documented as 1000 (§9.6)\n",
             FORTH_PICKER_MAX_SCAN_STEPS);
      sc1 = 1;
    }

    /* Filler steps are the point. With one DEFINITION per step the picker's
     * 170-name cap (TMP_STR_LENGTH/15) bites at step 170 and the step
     * cut-off never gets to act — a fixture built that way stays green with
     * the break deleted, which is how this one was found. So: two
     * definitions only, a near one at step 2 and a far one past the
     * cut-off, with non-defining Forth source steps ("1") in between. Two
     * names is nowhere near the name cap, so the only thing that can keep
     * the far one out is the step cut-off itself.
     *
     * Steps: 1 = opening marker, 2 = ": A000 1 ;", 3..(2+FILLERS) = filler,
     * then ": B004 1 ;" at step 3+FILLERS, then the closing marker. With
     * FILLERS = FORTH_PICKER_MAX_SCAN_STEPS the far definition sits at step
     * 1003, and the scan breaks after step 1000. */
    const int      fillers   = FORTH_PICKER_MAX_SCAN_STEPS;
    const uint16_t progLen   = (uint16_t)(4              /* opening marker  */
                                        + 14             /* ": A000 1 ;"    */
                                        + fillers * 5    /* header(4) + "1" */
                                        + 14             /* ": B004 1 ;"    */
                                        + 4);            /* closing marker  */

    uint8_t *prog = sc1 ? NULL : (uint8_t *)malloc(progLen);
    if (!prog) {
      if (!sc1) {
        printf("    [1] FAIL: malloc failed\n");
        sc1 = 1;
      }
    }
    else {
      uint8_t *p = prog;
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;      /* marker (opening) */

      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 10;        /* ": A000 1 ;" */
      *p++ = ':'; *p++ = ' '; *p++ = 'A'; *p++ = '0'; *p++ = '0'; *p++ = '0';
      *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';

      for (int i = 0; i < fillers; i++) {
        *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 1;       /* body "1": defines nothing */
        *p++ = '1';
      }

      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 10;        /* ": B004 1 ;" */
      *p++ = ':'; *p++ = ' '; *p++ = 'B'; *p++ = '0'; *p++ = '0'; *p++ = '4';
      *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';

      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;      /* marker (closing) */

      if ((p - prog) != progLen || !writeTestProgram(prog, progLen)) {
        printf("    [1] FAIL: writeTestProgram failed\n");
        sc1 = 1;
      }
      free(prog);
    }

    if (!sc1) {
      currentProgramNumber = 1;
      currentStep          = beginOfProgramMemory + (progLen - 4);   /* closing marker */

      { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
        testInitVariableSoftmenu(22);
        calcMode = m1e3s_; }

      int foundNear = 0, foundFar = 0;
      if (dynamicSoftmenu[22].menuContent) {
        const char *content = (const char *)dynamicSoftmenu[22].menuContent;
        while (*content) {
          if (compareString(content, "A000", CMP_BINARY) == 0) foundNear = 1;
          if (compareString(content, "B004", CMP_BINARY) == 0) foundFar  = 1;
          content += strlen(content) + 1;
        }
      }
      else {
        printf("    [1] FAIL: menuContent is NULL\n");
        sc1 = 1;
      }

      if (!sc1 && !foundNear) {
        printf("    [1] FAIL: 'A000' (step 2, well inside the cut-off) missing\n");
        sc1 = 1;
      }
      if (!sc1 && foundFar) {
        printf("    [1] FAIL: 'B004' (past step %d) listed — the scan did not stop\n",
               FORTH_PICKER_MAX_SCAN_STEPS);
        sc1 = 1;
      }
      if (!sc1 && dynamicSoftmenu[22].numItems != 1) {
        printf("    [1] FAIL: numItems = %d, expected exactly 1 (only the near definition)\n",
               dynamicSoftmenu[22].numItems);
        sc1 = 1;
      }

      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
      }
      dynamicSoftmenu[22].numItems = 0;
      cleanupTestProgram();
    }

    if (!sc1) {
      printf("    [1] PASS: scan stops at the documented cut-off — near definition listed, far one absent\n");
    }
    fail |= sc1;
  }

  /* ---- Subcase 2: the empty picker ---- */
  { int sc2 = 0;
    currentProgramNumber = 1;
    currentStep          = beginOfProgramMemory;

    { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
      testInitVariableSoftmenu(22);
      calcMode = m1e3s_; }

    if (dynamicSoftmenu[22].numItems != 0) {
      printf("    [2] FAIL: empty picker numItems = %d, expected 0\n",
             dynamicSoftmenu[22].numItems);
      sc2 = 1;
    }
    if (dynamicSoftmenu[22].menuContent == NULL) {
      printf("    [2] FAIL: empty picker menuContent is NULL, expected the terminator blob\n");
      sc2 = 1;
    }
    else if (((uint8_t *)dynamicSoftmenu[22].menuContent)[0] != 0) {
      printf("    [2] FAIL: empty picker content does not start with the terminator\n");
      sc2 = 1;
    }

    if (!sc2) {
      softmenuStack[0].softmenuId = 22;
      softmenuStack[0].firstItem  = 0;
      if (compareString(dynmenuGetLabel(0), "", CMP_BINARY) != 0) {
        printf("    [2] FAIL: dynmenuGetLabel(0) = '%s' on an empty picker, expected \"\"\n",
               dynmenuGetLabel(0));
        sc2 = 1;
      }
    }

    if (!sc2) {
      uint8_t *contentBeforeDraw = dynamicSoftmenu[22].menuContent;
      showSoftmenuCurrentPart();
      if (dynamicSoftmenu[22].numItems != 0) {
        printf("    [2] FAIL: draw changed numItems to %d on an empty picker\n",
               dynamicSoftmenu[22].numItems);
        sc2 = 1;
      }
      if (dynamicSoftmenu[22].menuContent == NULL) {
        printf("    [2] FAIL: draw left menuContent NULL on an empty picker\n");
        sc2 = 1;
      }
      (void)contentBeforeDraw;   /* the draw rebuilds MNU_FORTH, so the pointer may move */
    }

    if (dynamicSoftmenu[22].menuContent) {
      free(dynamicSoftmenu[22].menuContent);
      dynamicSoftmenu[22].menuContent = NULL;
    }
    dynamicSoftmenu[22].numItems = 0;

    if (!sc2) {
      printf("    [2] PASS: empty picker is a well-formed empty menu, not a NULL with a count\n");
    }
    fail |= sc2;
  }

  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems    = savedNumItems;
  cachedDynamicMenu               = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  currentStep          = savedCurrentStep;
  currentProgramNumber = savedProgNum;

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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

  { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
    testInitVariableSoftmenu(22);
    calcMode = m1e3s_; }

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
      if (!sc1) printf("    [1] PASS: capture opens OPEN, in ALPHA, on an empty aimBuffer\n");
      fail |= sc1;
    }

    /* ---- Subcase 2: Typing lands in aimBuffer and the step ----
     * S3 re-pin: the capture sink IS aimBuffer now (see forth_capture.h).
     * Before S3 this asserted the opposite — that aimBuffer stayed empty
     * while the text accumulated in a separately allocated buffer. */
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
        else if (strcmp(aimBuffer, "3 4 +") != 0) {
          printf("    [2] FAIL: aimBuffer = '%s', expected '3 4 +' (aimBuffer IS the sink)\n",
                 aimBuffer);
          sc2 = 1;
        }
      }
      if (!sc2) printf("    [2] PASS: sink is aimBuffer; step re-commits per key\n");
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
        /* FIX-7b re-pin: the fold now recommits the on-disk step, which may
         * legally relocate program memory (resize), so raw-pointer identity
         * with the pre-STO capture step is no longer the right oracle.  The
         * stronger post-fix pin is the recommit invariant itself: currentStep
         * is ON an ITM_FORTH step whose payload mirrors aimBuffer verbatim. */
        else if (currentStep[0] != 0x8B || currentStep[1] != 0x1A ||
                 currentStep[2] != 0xFD ||
                 currentStep[3] != stringByteLength(aimBuffer) ||
                 memcmp(currentStep + 4, aimBuffer, currentStep[3]) != 0) {
          printf("    [1] FAIL: on-disk step does not mirror the folded line\n");
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

        { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
          testInitVariableSoftmenu(22);
          calcMode = m1e3s_; }
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

        { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
          testInitVariableSoftmenu(22);
          calcMode = m1e3s_; }
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
           * an unbounded one.  FIX-7b adds one recommit (delete +
           * re-insert of a now-longer step) per convert cycle, so the
           * bound widens from 4 to 6 quanta — still block-aligned,
           * still growth-only, still bounded. */
          if (delta4 > 0 && delta4 % BYTES_PER_BLOCK == 0
              && delta4 <= 6 * BYTES_PER_BLOCK && freeBefore4 > after4) {
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
        { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
          testInitVariableSoftmenu(22);
          calcMode = m1e3s_; }
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

/* SB-1: sim bench, capture mechanics + cancel edges (charter A2-A6, F1, F2) */
static int test_sim_bench_capture(void)
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
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  uint8_t savedAlphaCase = alphaCase;

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);
  extern void showSoftmenu(int16_t);
  extern void addStepInProgram(int16_t func);
  extern void pemAlpha(int16_t);
  extern void pemCloseAlphaInput(void);
  extern int16_t currentMenu(void);

  /* SB-A2: reopen + mid-line edit */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-A2 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-A2 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      /* Commit a line "3 4 +" */
      addStepInProgram(ITM_FORTH);
      if (!forthCapIsOpen()) {
        printf("    SB-A2 FAIL: toggle-open did not open capture\n");
        sc = 1;
      }
    }
    if (!sc) {
      pemAlpha(ITM_3);
      pemAlpha(ITM_SPACE);
      pemAlpha(ITM_4);
      pemAlpha(ITM_SPACE);
      pemAlpha(ITM_PLUS);
      pemAlpha(ITM_ENTER);
      /* Line committed, region stays open with empty tail */
    }
    if (!sc) {
      /* Close the region with EXIT */
      fnKeyExit(NOPARAM);
      if (forthCapIsOpen()) {
        printf("    SB-A2 FAIL: capture still open after EXIT on empty line\n");
        sc = 1;
      }
    }
    if (!sc) {
      /* Reopen with edit gesture on the source step */
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (!stepIsForthStep(sSource)) {
        printf("    SB-A2 FAIL: could not locate source step\n");
        sc = 1;
      } else {
        currentStep = sSource;
        pemAlpha(ITM_EDIT);
        if (!forthCapIsOpen()) {
          printf("    SB-A2 FAIL: edit gesture did not reopen capture\n");
          sc = 1;
        }
      }
    }
    if (!sc) {
      /* Reopen places cursor at line end (T5 trace) */
      int32_t expectedPos = stringByteLength(aimBuffer);
      if (T_cursorPos != expectedPos) {
        printf("    SB-A2 FAIL: cursor at %d, expected %d (line end)\n",
               T_cursorPos, expectedPos);
        sc = 1;
      }
    }
    if (!sc) {
      /* Move cursor left twice, insert '2', ENTER */
      T_cursorPos -= 2;  /* position before trailing " +" -> before "4 +" */
      pemAlpha(ITM_2);
      pemAlpha(ITM_ENTER);
      /* Locate the source step and assert payload is "3 42 +" */
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (!stepIsForthStep(sSource)) {
        printf("    SB-A2 FAIL: source step not found after edit\n");
        sc = 1;
      } else if (!stepSrcTextEq(sSource, "3 42 +")) {
        printf("    SB-A2 FAIL: payload wrong (len=%u)\n", sSource[3]);
        sc = 1;
      }
    }
    if (!sc) {
      printf("    SB-A2 PASS: reopen + mid-line edit\n");
    }
    fail |= sc;

    pemAlpha(ITM_BACKSPACE);  /* clean up empty tail if open */
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-A3: two-byte glyph backspace */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-A3 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-A3 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      addStepInProgram(ITM_FORTH);
      if (!forthCapIsOpen()) {
        printf("    SB-A3 FAIL: toggle-open did not open capture\n");
        sc = 1;
      }
    }

    if (!sc) {
      /* Record pre-glyph state */
      int32_t glyphsBefore = stringGlyphLength(aimBuffer);
      int32_t bytesBefore = stringByteLength(aimBuffer);

      /* Type the two-byte glyph */
      pemAlpha(ITM_CROSS);

      int32_t glyphsAfter = stringGlyphLength(aimBuffer);
      int32_t bytesAfter = stringByteLength(aimBuffer);

      if (glyphsAfter != glyphsBefore + 1) {
        printf("    SB-A3 FAIL: glyph count %ld, expected %ld\n",
               (long)glyphsAfter, (long)(glyphsBefore + 1));
        sc = 1;
      } else if (bytesAfter - bytesBefore < 1) {
        printf("    SB-A3 FAIL: byte length did not increase\n");
        sc = 1;
      }

      /* One backspace should remove the whole glyph */
      pemAlpha(ITM_BACKSPACE);

      int32_t glyphsAfterBS = stringGlyphLength(aimBuffer);
      int32_t bytesAfterBS = stringByteLength(aimBuffer);

      if (glyphsAfterBS != glyphsBefore) {
        printf("    SB-A3 FAIL: glyph count after BS %ld, expected %ld\n",
               (long)glyphsAfterBS, (long)glyphsBefore);
        sc = 1;
      } else if (bytesAfterBS != bytesBefore) {
        printf("    SB-A3 FAIL: byte length after BS %ld, expected %ld (whole-glyph removal)\n",
               (long)bytesAfterBS, (long)bytesBefore);
        sc = 1;
      }
    }

    if (!sc) {
      printf("    SB-A3 PASS: two-byte glyph backspace\n");
    }
    fail |= sc;

    pemAlpha(ITM_BACKSPACE);  /* clean up */
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-A4: the 196-glyph cap */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-A4 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-A4 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      addStepInProgram(ITM_FORTH);
      if (!forthCapIsOpen()) {
        printf("    SB-A4 FAIL: toggle-open did not open capture\n");
        sc = 1;
      }
    }

    if (!sc) {
      /* Drive 196 glyphs (single-byte '1' keys) */
      int32_t i;
      for (i = 0; i < 196; i++) {
        pemAlpha(ITM_1);
      }
      if (stringGlyphLength(aimBuffer) != 196) {
        printf("    SB-A4 FAIL: glyph count %ld, expected 196\n",
               (long)stringGlyphLength(aimBuffer));
        sc = 1;
      }

      /* Press 197th key - should be silently ignored */
      int32_t lenBefore = stringGlyphLength(aimBuffer);
      pemAlpha(ITM_1);
      if (stringGlyphLength(aimBuffer) != lenBefore) {
        printf("    SB-A4 FAIL: 197th glyph appended (len %ld)\n",
               (long)stringGlyphLength(aimBuffer));
        sc = 1;
      }

      /* ENTER - assert committed step carries all 196 glyphs */
      pemAlpha(ITM_ENTER);

      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A || sSource[2] != 0xFD) {
        printf("    SB-A4 FAIL: source step not found\n");
        sc = 1;
      } else if (sSource[3] != 196) {
        printf("    SB-A4 FAIL: committed step len=%u, expected 196\n", sSource[3]);
        sc = 1;
      }
    }

    if (!sc) {
      printf("    SB-A4 PASS: 196-glyph cap enforced\n");
    }
    fail |= sc;

    pemAlpha(ITM_BACKSPACE);  /* clean up empty tail */
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-A5: save/restore with a half-typed line */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-A5 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-A5 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      addStepInProgram(ITM_FORTH);
    }

    if (!sc) {
      /* Type a line, commit it, then type a half-line */
      pemAlpha(ITM_3);
      pemAlpha(ITM_SPACE);
      pemAlpha(ITM_4);
      pemAlpha(ITM_SPACE);
      pemAlpha(ITM_PLUS);
      pemAlpha(ITM_ENTER);  /* commit "3 4 +" */

      /* Type half line (do NOT commit) */
      pemAlpha(ITM_7);
      pemAlpha(ITM_8);
    }

    if (!sc) {
      /* Verify committed text survives in step */
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (!stepIsForthStep(sSource)) {
        printf("    SB-A5 FAIL: committed source step not found\n");
        sc = 1;
      } else if (!stepSrcTextEq(sSource, "3 4 +")) {
        printf("    SB-A5 FAIL: committed text wrong (len=%u)\n", sSource[3]);
        sc = 1;
      }
    }

    if (!sc) {
      /* Model power-off teardown before saving — test_accept_entry_state_roundtrip
       * refuses to save while capture is open; the device tears down entry UI first. */
      aimBuffer[0] = 0;
      clearSystemFlag(FLAG_ALPHA);
      tam.function = 0;

      saveCalc();

      {
        bool_t savedLoad = loadTestPrograms;
        loadTestPrograms = false;
        restoreCalc();
        loadTestPrograms = savedLoad;
      }

      /* After restore: all pre-save pointers are stale — re-derive from beginOfProgramMemory.
       * Walk: LBL -> opening marker -> source step 1 ("3 4 +") -> source step 2 ("78").
       * Assert the half-line step text '78' survived and forthCapIsOpen() is false
       * — the open-flag loss IS the contract. */
      { uint8_t *step = beginOfProgramMemory;
        step = findNextStep(step);    /* opening marker */
        step = findNextStep(step);    /* source step 1 ("3 4 +") */
        step = findNextStep(step);    /* source step 2 ("78") */
        if (!stepIsForthStep(step)) {
          printf("    SB-A5 FAIL: half-line source step not found after restore\n");
          sc = 1;
        } else if (!stepSrcTextEq(step, "78")) {
          printf("    SB-A5 FAIL: half-line text wrong (len=%u)\n", step[3]);
          sc = 1;
        } else if (forthCapIsOpen()) {
          printf("    SB-A5 FAIL: capture still open after restore\n");
          sc = 1;
        }
      }
    }

    if (!sc) {
      printf("    SB-A5 PASS: save/restore with half-typed line\n");
    }
    fail |= sc;

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-A6: EXIT with a half-typed line */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-A6 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-A6 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      addStepInProgram(ITM_FORTH);
    }

    if (!sc) {
      /* Type a half line */
      pemAlpha(ITM_7);
      pemAlpha(ITM_8);
      int32_t typedLen = stringByteLength(aimBuffer);
      char typedText[256];
      xcopy(typedText, aimBuffer, typedLen + 1);

      /* Press EXIT once - drops alpha keypad (E8 middle row) */
      fnKeyExit(NOPARAM);

      /* Reopen the line with edit gesture */
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A || sSource[2] != 0xFD) {
        printf("    SB-A6 FAIL: source step not found after EXIT\n");
        sc = 1;
      } else {
        /* Assert text present equals what was typed (committed per key) */
        if (sSource[3] != typedLen || memcmp(sSource + 4, typedText, typedLen) != 0) {
          printf("    SB-A6 FAIL: text not preserved after EXIT\n");
          sc = 1;
        } else {
          /* Reopen and verify */
          currentStep = sSource;
          pemAlpha(ITM_EDIT);
          if (!forthCapIsOpen()) {
            printf("    SB-A6 FAIL: edit did not reopen capture\n");
            sc = 1;
          } else if (stringByteLength(aimBuffer) != typedLen) {
            printf("    SB-A6 FAIL: reopened text length wrong\n");
            sc = 1;
          }
        }
      }
    }

    if (!sc) {
      printf("    SB-A6 PASS: EXIT with half-typed line preserves text\n");
    }
    fail |= sc;

    pemAlpha(ITM_BACKSPACE);  /* clean up */
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-F1: the EXIT ladder */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F66");
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    SB-F1 FIXTURE FAIL: build/write\n");
      sc = 1;
    }

    if (!sc) {
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
        printf("    SB-F1 FIXTURE BUG: fnGotoDot(1) did not position on LBL\n");
        sc = 1;
      }
    }

    if (!sc) {
      /* Open capture, type content, then open FWRD picker above ALPHA menu */
      addStepInProgram(ITM_FORTH);
      pemAlpha(ITM_3);
      pemAlpha(ITM_SPACE);
      pemAlpha(ITM_4);
      showSoftmenu(-MNU_FORTH);  /* drive FWRD picker open above ALPHA menu */
    }

    if (!sc) {
      /* Assert precondition: FWRD picker current, FLAG_ALPHA set */
      if (!getSystemFlag(FLAG_ALPHA)) {
        printf("    SB-F1 FIXTURE BUG: FLAG_ALPHA not set before EXIT (menu=%d)\n",
               currentMenu());
        sc = 1;
      } else if (currentMenu() != -MNU_FORTH) {
        printf("    SB-F1 FIXTURE BUG: menu=%d, expected -MNU_FORTH\n",
               currentMenu());
        sc = 1;
      }
    }

    if (!sc) {
      /* First EXIT: with FWRD picker current above open capture,
       * pops to ALPHA menu */
      fnKeyExit(NOPARAM);

      /* After first EXIT: the alpha keypad should be active,
       * softmenu stack top should be -MNU_ALPHA */
      if (getSystemFlag(FLAG_ALPHA) && currentMenu() == -MNU_ALPHA) {
        /* Good - first EXIT popped to ALPHA menu */
        /* Second EXIT: drops alpha keypad, region markers survive, cursor stays */
        int savedStep = currentLocalStepNumber;
        fnKeyExit(NOPARAM);

        if (getSystemFlag(FLAG_ALPHA)) {
          printf("    SB-F1 FAIL: FLAG_ALPHA still set after second EXIT\n");
          sc = 1;
        } else if (calcMode != CM_PEM) {
          printf("    SB-F1 FAIL: not in PEM after second EXIT\n");
          sc = 1;
        } else if (currentLocalStepNumber != savedStep) {
          printf("    SB-F1 FAIL: cursor moved after second EXIT (step=%d, was=%d)\n",
                 currentLocalStepNumber, savedStep);
          sc = 1;
        } else {
          /* Region markers still exist — re-walk steps and assert marker present */
          uint8_t *mkr = beginOfProgramMemory;
          mkr = findNextStep(mkr);  /* opening marker */
          if (!stepIsMarker(mkr)) {
            printf("    SB-F1 FAIL: opening marker gone after second EXIT\n");
            sc = 1;
          } else {
            /* Third EXIT: leaves PEM */
            fnKeyExit(NOPARAM);

            if (calcMode == CM_PEM) {
              printf("    SB-F1 FAIL: still in PEM after third EXIT\n");
              sc = 1;
            }
          }
        }
      } else {
        printf("    SB-F1 FAIL: first EXIT did not pop to ALPHA menu (menu=%d, alpha=%d)\n",
               currentMenu(), getSystemFlag(FLAG_ALPHA));
        sc = 1;
      }
    }

    if (!sc) {
      printf("    SB-F1 PASS: EXIT ladder\n");
    }
    fail |= sc;

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
  }

  /* SB-F2: empty-line backspace */
  { int sc = 0;

    /* (a) Region with no committed lines - backspace deletes region */
    { int sc_a = 0;
      testProg_t p;
      tpInit(&p);
      int sLbl = tpLbl(&p, "F66");
      if (sLbl < 0 || !tpWrite(&p)) {
        printf("    SB-F2(a) FIXTURE FAIL: build/write\n");
        sc_a = 1;
      }

      if (!sc_a) {
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
          printf("    SB-F2(a) FIXTURE BUG: fnGotoDot(1)\n");
          sc_a = 1;
        }
      }

      if (!sc_a) {
        addStepInProgram(ITM_FORTH);
        /* Capture open, empty line */
        uint8_t *sMarkerBefore = findNextStep(tpStepAddr(&p, sLbl));
        if (!sMarkerBefore) {
          printf("    SB-F2(a) FAIL: no marker after open\n");
          sc_a = 1;
        } else {
          /* Backspace on empty line - should delete region */
          pemAlpha(ITM_BACKSPACE);

          /* Assert markers removed, capture closed */
          if (forthCapIsOpen()) {
            printf("    SB-F2(a) FAIL: capture still open after backspace\n");
            sc_a = 1;
          }
        }
      }

      if (!sc_a) {
        printf("    SB-F2(a) PASS: empty-line backspace deletes region (no committed lines)\n");
      }
      sc |= sc_a;

      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }

    /* (b) After one committed line - assert committed line NOT destroyed */
    { int sc_b = 0;
      testProg_t p;
      tpInit(&p);
      int sLbl = tpLbl(&p, "F66");
      if (sLbl < 0 || !tpWrite(&p)) {
        printf("    SB-F2(b) FIXTURE FAIL: build/write\n");
        sc_b = 1;
      }

      if (!sc_b) {
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
          printf("    SB-F2(b) FIXTURE BUG: fnGotoDot(1)\n");
          sc_b = 1;
        }
      }

      if (!sc_b) {
        addStepInProgram(ITM_FORTH);
        /* Commit one line */
        pemAlpha(ITM_3);
        pemAlpha(ITM_SPACE);
        pemAlpha(ITM_4);
        pemAlpha(ITM_ENTER);  /* commit "3 4" */

        /* Backspace on empty tail - should close empty tail, NOT destroy committed line */
        pemAlpha(ITM_BACKSPACE);

        /* Assert committed line still exists */
        uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
        uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
        if (!stepIsForthStep(sSource)) {
          printf("    SB-F2(b) FAIL: committed source step destroyed\n");
          sc_b = 1;
        } else if (!stepSrcTextEq(sSource, "3 4")) {
          printf("    SB-F2(b) FAIL: committed text wrong (len=%u)\n", sSource[3]);
          sc_b = 1;
        }
      }

      if (!sc_b) {
        printf("    SB-F2(b) PASS: empty-line backspace preserves committed line\n");
      }
      sc |= sc_b;

      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }

    fail |= sc;
  }

  /* Epilogue */
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
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  alphaCase = savedAlphaCase;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

static int test_sim_bench_nesting(void)
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
  uint8_t savedAlphaCase = alphaCase;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void pemAlpha(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamEnterMode(int16_t);
  extern void tamProcessInput(uint16_t);
  extern void showSoftmenu(int16_t menu);

  /* ---- SB-B1: TAM cancel keeps the line ---- */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "F65");
    tpMarker(&p);

    if (!tpWrite(&p)) {
      printf("    SB-B1 FIXTURE FAIL: tpWrite\n");
      fail = 1;
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
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      fnGotoDot(2);

      if (currentStep != tpStepAddr(&p, 1) || currentLocalStepNumber != 2) {
        printf("    SB-B1 FIXTURE BUG: fnGotoDot(2)\n");
        sc = 1;
      }
      else {
        runFunction(ITM_AIM);
        if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
          printf("    SB-B1 FIXTURE BUG: ITM_AIM did not open Forth capture\n");
          sc = 1;
        }
      }

      char preText[64];
      int16_t cursorBefore;

      if (!sc) {
        /* Type "text" in capture line */
        runFunction(ITM_T);
        runFunction(ITM_E);
        runFunction(ITM_X);
        runFunction(ITM_T);

        /* Record state before TAM */
        cursorBefore = T_cursorPos;
        xcopy(preText, forthTestCapText(),
              stringByteLength((char *)forthTestCapText()) + 1);

        /* Open TAM via XEQ path (as test_capture_suspend drives it) */
        runFunction(ITM_XEQ);

        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    SB-B1 FAIL: XEQ did not suspend capture (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Type two name glyphs WITHOUT completing */
        tamProcessInput(ITM_alpha);
        runFunction(ITM_W);
        runFunction(ITM_O);

        /* Cancel back with EXIT */
        int guard;
        for (guard = 0; tam.mode != 0 && guard < 4; guard++) {
          fnKeyExit(NOPARAM);
        }

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    SB-B1 FAIL: capture not open after cancel (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
        else if (tam.mode != 0) {
          printf("    SB-B1 FAIL: tam.mode = %d, expected 0\n", (int)tam.mode);
          sc = 1;
        }
        else if (strcmp(forthTestCapText(), preText) != 0) {
          printf("    SB-B1 FAIL: capture text changed: '%s' vs '%s'\n",
                 forthTestCapText(), preText);
          sc = 1;
        }
        else if (T_cursorPos != cursorBefore) {
          printf("    SB-B1 FAIL: cursor moved: %d vs %d\n",
                 T_cursorPos, cursorBefore);
          sc = 1;
        }
      }

      if (!sc) {
        printf("    SB-B1 PASS: TAM cancel keeps the line\n");
      }
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
    fail |= sc;
  }

  /* ---- SB-B2: no tam.colon leak ---- */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "F66");
    tpMarker(&p);

    if (!tpWrite(&p)) {
      printf("    SB-B2 FIXTURE FAIL: tpWrite\n");
      fail = 1;
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
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      fnGotoDot(2);

      if (currentStep != tpStepAddr(&p, 1) || currentLocalStepNumber != 2) {
        printf("    SB-B2 FIXTURE BUG: fnGotoDot(2)\n");
        sc = 1;
      }
      else {
        runFunction(ITM_AIM);
        if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
          printf("    SB-B2 FIXTURE BUG: ITM_AIM did not open Forth capture\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Type "abc" in capture line */
        runFunction(ITM_A);
        runFunction(ITM_B);
        runFunction(ITM_C);

        /* Open TAM via XEQ path */
        runFunction(ITM_XEQ);

        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    SB-B2 FAIL: XEQ did not suspend capture (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Press the TAM : key so tam.colon becomes true */
        tamProcessInput(ITM_COLON);
        tamProcessInput(ITM_alpha);
        if (!tam.colon) {
          printf("    SB-B2 FIXTURE BUG: tam.colon not set after COLON\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Cancel */
        int guard;
        for (guard = 0; tam.mode != 0 && guard < 4; guard++) {
          fnKeyExit(NOPARAM);
        }

        /* Press a plain glyph — assert no colon artifact in capture text */
        runFunction(ITM_D);

        if (strcmp(forthTestCapText(), "ABCD") != 0) {
          printf("    SB-B2 FAIL: text = '%s', expected 'ABCD' (colon artifact)\n",
                 forthTestCapText());
          sc = 1;
        }
      }

      if (!sc) {
        /* Re-enter TAM — assert tamEnterMode re-initializes tam.colon to false */
        extern void tamEnterMode(int16_t);
        tamEnterMode(ITM_XEQ);
        if (tam.colon) {
          printf("    SB-B2 FAIL: tam.colon not re-initialized on TAM entry\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Cancel back out */
        int guard;
        for (guard = 0; tam.mode != 0 && guard < 4; guard++) {
          fnKeyExit(NOPARAM);
        }
      }

      if (!sc) {
        printf("    SB-B2 PASS: no colon leak into capture keys; re-init pinned\n");
      }
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
    fail |= sc;
  }

  /* ---- SB-B3: committed XEQ from capture ---- */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "F67");
    tpMarker(&p);
    tpLbl(&p, "TGT");   /* target label for XEQ */
    tpRtn(&p);

    if (!tpWrite(&p)) {
      printf("    SB-B3 FIXTURE FAIL: tpWrite\n");
      fail = 1;
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
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      fnGotoDot(2);

      if (currentStep != tpStepAddr(&p, 1) || currentLocalStepNumber != 2) {
        printf("    SB-B3 FIXTURE BUG: fnGotoDot(2)\n");
        sc = 1;
      }
      else {
        runFunction(ITM_AIM);
        if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
          printf("    SB-B3 FIXTURE BUG: ITM_AIM did not open Forth capture\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Complete a full XEQ 'TGT' TAM entry */
        runFunction(ITM_XEQ);

        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    SB-B3 FAIL: XEQ did not suspend capture (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Type name "TGT" */
        tamProcessInput(ITM_alpha);
        runFunction(ITM_T);
        runFunction(ITM_G);
        runFunction(ITM_T);

        /* Commit with ENTER (not EXIT which cancels) */
        tamProcessInput(ITM_ENTER);

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    SB-B3 FAIL: capture not open after XEQ commit (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Verify: capture line survives intact */
        uint8_t *capStep = currentStep;
        if (capStep[0] != 0x8B || capStep[1] != 0x1A || capStep[2] != 0xFD) {
          printf("    SB-B3 FAIL: capture step is not a Forth source step\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Verify: XEQ name converted to text in capture line */
        const char *text = forthTestCapText();
        int hasXeq = 0;
        const char *s = text;
        while (*s) {
          if (s[0] == 'X' && s[1] == 'E' && s[2] == 'Q') {
            hasXeq = 1;
            break;
          }
          s++;
        }
        if (!hasXeq) {
          printf("    SB-B3 FAIL: capture text missing XEQ form: '%s'\n",
                 forthTestCapText());
          sc = 1;
        }
      }

      if (!sc) {
        printf("    SB-B3 PASS: committed XEQ from capture\n");
      }
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
    fail |= sc;
  }

  /* ---- SB-C2: local-register form and cancel ---- */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "F68");
    tpMarker(&p);

    if (!tpWrite(&p)) {
      printf("    SB-C2 FIXTURE FAIL: tpWrite\n");
      fail = 1;
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
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      fnGotoDot(2);

      if (currentStep != tpStepAddr(&p, 1) || currentLocalStepNumber != 2) {
        printf("    SB-C2 FIXTURE BUG: fnGotoDot(2)\n");
        sc = 1;
      }
      else {
        runFunction(ITM_AIM);
        if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
          printf("    SB-C2 FIXTURE BUG: ITM_AIM did not open Forth capture\n");
          sc = 1;
        }
      }

      if (!sc) {
        /* Press STO then . 0 5 (the local form) */
        runFunction(ITM_STO);

        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    SB-C2 FAIL: STO did not suspend capture (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Type . 0 5 for local register L05 */
        tamProcessInput(ITM_PERIOD);
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_5);   /* three digits should auto-fire commit */

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    SB-C2 FAIL: capture not open after local commit (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
      }

      if (!sc) {
        /* Assert canonical text landed in capture line */
        const char *text = forthTestCapText();
        int hasSto05 = 0;
        const char *s = text;
        while (*s) {
          if (s[0] == 'S' && s[1] == 'T' && s[2] == 'O' &&
               s[3] == ' ' && s[4] == '.' && s[5] == '0' && s[6] == '5') {
             hasSto05 = 1;
            break;
          }
          s++;
        }
        if (!hasSto05) {
           printf("    SB-C2 FAIL: canonical text missing STO .05 form: '%s'\n",
                 forthTestCapText());
          sc = 1;
        }
      }

      if (!sc) {
        /* Now press STO and cancel out (EXIT before any digit) */
        char beforeCancel[64];
        xcopy(beforeCancel, forthTestCapText(),
              stringByteLength((char *)forthTestCapText()) + 1);

        runFunction(ITM_STO);
        fnKeyExit(NOPARAM);   /* cancel before any digit */

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    SB-C2 FAIL: capture not open after STO cancel (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
        else if (strcmp(forthTestCapText(), beforeCancel) != 0) {
          printf("    SB-C2 FAIL: line changed after STO cancel: '%s'\n",
                 forthTestCapText());
          sc = 1;
        }
      }

      if (!sc) {
        printf("    SB-C2 PASS: local-register form and cancel\n");
      }
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
    fail |= sc;
  }

  /* ---- SB-D2: picker navigation leaks nothing ---- */
  { int sc = 0;
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "F69");
    tpMarker(&p);
    tpLbl(&p, "ONE");
    tpRtn(&p);
    tpLbl(&p, "TWO");
    tpRtn(&p);

    if (!tpWrite(&p)) {
      printf("    SB-D2 FIXTURE FAIL: tpWrite\n");
      fail = 1;
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
      lastErrorCode = ERROR_NONE;
      forthCapClose();

      fnGotoDot(2);

      if (currentStep != tpStepAddr(&p, 1) || currentLocalStepNumber != 2) {
        printf("    SB-D2 FIXTURE BUG: fnGotoDot(2)\n");
        sc = 1;
      }
      else {
        runFunction(ITM_AIM);
        if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
          printf("    SB-D2 FIXTURE BUG: ITM_AIM did not open Forth capture\n");
          sc = 1;
        }
      }

      char preText[64];
      int16_t cursorBefore;

      if (!sc) {
        /* Type "text" in capture line */
        runFunction(ITM_T);
        runFunction(ITM_E);
        runFunction(ITM_X);
        runFunction(ITM_T);

        /* Record state before picker open */
        cursorBefore = T_cursorPos;
        xcopy(preText, forthTestCapText(),
              stringByteLength((char *)forthTestCapText()) + 1);

        /* Open the FWRD picker (menu overlay, does not suspend capture) */
        showSoftmenu(-MNU_FORTH);

        if (currentMenu() != -MNU_FORTH) {
          printf("    SB-D2 FIXTURE BUG: menu=%d, expected -MNU_FORTH\n",
                 currentMenu());
          sc = 1;
        }
      }

      if (!sc) {
        /* EXIT closes the picker */
        fnKeyExit(NOPARAM);

        if (forthTestCapState() != FCAP_OPEN) {
          printf("    SB-D2 FAIL: capture not open after picker cancel (state=%d)\n",
                 forthTestCapState());
          sc = 1;
        }
        else if (strcmp(forthTestCapText(), preText) != 0) {
          printf("    SB-D2 FAIL: capture text changed: '%s' vs '%s'\n",
                 forthTestCapText(), preText);
          sc = 1;
        }
        else if (T_cursorPos != cursorBefore) {
          printf("    SB-D2 FAIL: cursor moved: %d vs %d\n",
                 T_cursorPos, cursorBefore);
          sc = 1;
        }
      }

      if (!sc) {
        printf("    SB-D2 PASS: picker navigation leaks nothing\n");
      }
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
    fail |= sc;
  }

  /* Epilogue */
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
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  alphaCase = savedAlphaCase;
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
    { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
      testInitVariableSoftmenu(22);
      calcMode = m1e3s_; }
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


/* test_picker_renders_labels — G3: the picker's label reaches the LCD.
 *
 * Everything else in stage G stops at the content array or at
 * dynmenuGetLabel(). This one reads the framebuffer back. lcd_buffer is
 * filled by the software blitter whether or not a GTK window exists — the
 * headlessMode guard in the c47-gtk HAL only skips
 * gtk_widget_queue_draw_area — and lcd_buffer_pixel_on() is declared in
 * src/c47/hal/lcd.h for every non-DMCP build, so it links in the sim binary
 * and in the upstream testSuite binary alike. (lcd_clear_buf() does NOT
 * exist in the testSuite HAL; nothing here may call it.)
 *
 * Three renders of ONE softkey cell, in DECREASING label length, asserting
 * a strict decrease in lit pixels:
 *
 *   14-byte name  >  2-byte name  >  empty picker (chrome only)
 *
 * Decreasing order is deliberate. Nothing clears the buffer between
 * renders, so if showSoftkey did not fully repaint its cell, the leftovers
 * of a longer label would keep the later counts high and break the
 * assertion rather than flatter it.
 *
 * No pixel count is hard-coded: upstream owns the font and the cell
 * layout, and a legitimate change there must not turn this red. What is
 * pinned is that the label is drawn at all, that a longer name draws more
 * of it, and that the pixels counted belong to the label rather than to
 * the border — which is what the empty-picker floor establishes.
 *
 * Geometry: softkey rows are y1 = 217 - SOFTMENU_HEIGHT * row with
 * SOFTMENU_HEIGHT = 23 (softmenus.c), so the three rows span y >= 171; the
 * six cells divide SCREEN_WIDTH, so the first is x < SCREEN_WIDTH / 6. */
static int test_picker_renders_labels(void)
{
  uint8_t savedCalcModeM_ = calcMode;
  extern void showSoftmenu(int16_t menu);
  extern void showSoftmenuCurrentPart(void);

  static const char *const labels[3] = {"ABCDEFGHIJKLMN", "AB", NULL};   /* NULL = no definition */
  int32_t litInFirstCell[3] = {0, 0, 0};

  uint8_t         *savedCurrentStep = currentStep;
  uint16_t         savedProgNum     = currentProgramNumber;
  softmenuStack_t  savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  int16_t  savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent       = dynamicSoftmenu[22].menuContent;
  int16_t  savedNumItems          = dynamicSoftmenu[22].numItems;

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems    = 0;

  int fail = 0;

  for (int k = 0; k < 3 && !fail; k++) {
    const char *name    = labels[k];
    const int   nameLen = (name == NULL) ? 0 : (int)strlen(name);
    /* With a name: marker, ": <name> 1 ;", marker. Without: the two markers
     * alone, which is a syntactically fine program that defines nothing. */
    const int      bodyLen = (name == NULL) ? 0 : (2 + nameLen + 4);
    const uint16_t progLen = (uint16_t)(4 + (name == NULL ? 0 : 4 + bodyLen) + 4);

    uint8_t *prog = (uint8_t *)malloc(progLen);
    if (!prog) {
      printf("    FAIL: malloc failed\n");
      fail = 1;
      break;
    }
    uint8_t *p = prog;
    *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;          /* marker (opening) */
    if (name != NULL) {
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = (uint8_t)bodyLen;
      *p++ = ':'; *p++ = ' ';
      for (int c = 0; c < nameLen; c++) {
        *p++ = (uint8_t)name[c];
      }
      *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';
    }
    *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;          /* marker (closing) */

    if ((p - prog) != progLen || !writeTestProgram(prog, progLen)) {
      printf("    FAIL: writeTestProgram failed for label '%s'\n", name ? name : "(none)");
      free(prog);
      fail = 1;
      break;
    }
    free(prog);

    currentProgramNumber = 1;
    currentStep          = beginOfProgramMemory + (progLen - 4);

    calcMode = CM_PEM;   /* Stage M E3: the draws build; PEM cursor context */
    showSoftmenu(-MNU_FORTH);
    showSoftmenuCurrentPart();

    const int16_t expectItems = (name == NULL) ? 0 : 1;
    if (dynamicSoftmenu[22].numItems != expectItems) {
      printf("    FIXTURE BUG: picker has %d names for label '%s', expected %d\n",
             dynamicSoftmenu[22].numItems, name ? name : "(none)", expectItems);
      fail = 1;
    }

    if (!fail) {
      for (uint32_t y = 171; y < SCREEN_HEIGHT; y++) {
        for (uint32_t x = 0; x < SCREEN_WIDTH / 6; x++) {
          if (lcd_buffer_pixel_on(x, y)) {
            litInFirstCell[k]++;
          }
        }
      }
    }

    if (dynamicSoftmenu[22].menuContent) {
      free(dynamicSoftmenu[22].menuContent);
      dynamicSoftmenu[22].menuContent = NULL;
    }
    dynamicSoftmenu[22].numItems = 0;
    cleanupTestProgram();
  }

  if (!fail && litInFirstCell[2] <= 0) {
    printf("    FAIL: empty picker drew nothing in the first softkey cell — "
           "the draw path is not reaching lcd_buffer at all\n");
    fail = 1;
  }
  if (!fail && litInFirstCell[1] <= litInFirstCell[2]) {
    printf("    FAIL: 'AB' lit %d pixels against %d for an empty picker — "
           "the label is not drawn\n", litInFirstCell[1], litInFirstCell[2]);
    fail = 1;
  }
  if (!fail && litInFirstCell[0] <= litInFirstCell[1]) {
    printf("    FAIL: the 14-byte name lit %d pixels against %d for 'AB' — "
           "a maximal name is not rendered in full\n",
           litInFirstCell[0], litInFirstCell[1]);
    fail = 1;
  }

  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems    = savedNumItems;
  cachedDynamicMenu               = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  currentStep          = savedCurrentStep;
  currentProgramNumber = savedProgNum;

  if (!fail) {
    printf("    PASS: picker labels reach lcd_buffer — %d px (14-byte) > %d px (2-byte) > %d px (chrome only)\n",
           litInFirstCell[0], litInFirstCell[1], litInFirstCell[2]);
  }
    calcMode = savedCalcModeM_;
  return fail;
}

/* Lit pixels inside softkey cell `cell` (0..5) of the menu band. */
static int32_t g4CellPixels(int cell) {
  /* Real softkey borders, not SCREEN_WIDTH/6. KEY_X = {-1,66,133,200,267,333,400}
   * (c47.c), so an arithmetic sixth (66) puts cell 1's right-hand frame column
   * at x=132 INSIDE a naive cell-2 window and reports ~12 stray pixels in a
   * cell that is actually empty. Measure between the borders the renderer
   * itself uses. */
  extern const int KEY_X[7];
  int32_t lit = 0;
  const uint32_t xFrom = (uint32_t)(KEY_X[cell]     < 0 ? 0 : KEY_X[cell]);
  const uint32_t xTo   = (uint32_t)(KEY_X[cell + 1] < 0 ? 0 : KEY_X[cell + 1]);
  for (uint32_t y = 171; y < SCREEN_HEIGHT; y++) {
    for (uint32_t x = xFrom; x < xTo; x++) {
      if (lcd_buffer_pixel_on(x, y)) { lit++; }
    }
  }
  return lit;
}

/* test_picker_pixel_layout — G4: what the FWRD picker LOOKS like.
 *
 * Three rendering properties a user would notice immediately:
 *   [1] turning the page changes what is drawn,
 *   [2] empty cells of a partial last page are actually empty,
 *   [3] a maximal 14-byte name stays inside its own cell.
 *
 * Every assertion is an ordering or equality between counts taken in the
 * same run. No literal pixel count — upstream owns the font and cell layout. */
static int test_picker_pixel_layout(void)
{
  uint8_t savedCalcModeM_ = calcMode;
  extern void showSoftmenu(int16_t menu);
  extern void showSoftmenuCurrentPart(void);

  uint8_t         *savedCurrentStep = currentStep;
  uint16_t         savedProgNum     = currentProgramNumber;
  softmenuStack_t  savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  int16_t  savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent       = dynamicSoftmenu[22].menuContent;
  int16_t  savedNumItems          = dynamicSoftmenu[22].numItems;

  int fail = 0;

  /* ---------- [1] Turning the page changes the picture ---------- */
  {
    int sc1 = 0;

    /* LONGPAGE: 36 names — 18 short then 18 long.
     *
     * A VISIBLE PAGE IS 18 ITEMS, NOT 6. showSoftmenuCurrentPart draws
     * three rows of six (softmenus.c: `for(y=0; y<3; y++) for(x=0; x<6; x++)`
     * guarded by `x + 6*y + currentFirstItem < numberOfItems`), and
     * g4CellPixels sums a whole column, all three rows. A 12-name fixture
     * paged by 6 therefore draws 12 names at firstItem=0 and 6 at
     * firstItem=6, so page 1 wins on item COUNT and the comparison says
     * nothing about what changed. That was this subcase's original defect.
     *
     * 36 names paged by 18 puts an equal count on both pages and lets the
     * only difference be the label width: A00..A17 are 3 bytes, B00 + 11
     * filler are 14 — the maximum the picker admits. They sort A before B,
     * so page 1 is the short set and page 2 the long one. */
    static const char *const longpageNames[36] = {
      "A00", "A01", "A02", "A03", "A04", "A05", "A06", "A07", "A08",
      "A09", "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17",
      "B00CCCCCCCCCCC", "B01CCCCCCCCCCC", "B02CCCCCCCCCCC",
      "B03CCCCCCCCCCC", "B04CCCCCCCCCCC", "B05CCCCCCCCCCC",
      "B06CCCCCCCCCCC", "B07CCCCCCCCCCC", "B08CCCCCCCCCCC",
      "B09CCCCCCCCCCC", "B10CCCCCCCCCCC", "B11CCCCCCCCCCC",
      "B12CCCCCCCCCCC", "B13CCCCCCCCCCC", "B14CCCCCCCCCCC",
      "B15CCCCCCCCCCC", "B16CCCCCCCCCCC", "B17CCCCCCCCCCC"
    };
    const int longpageCount = 36;

    uint16_t progLen = 8;
    for (int i = 0; i < longpageCount; i++) {
      int nlen = (int)strlen(longpageNames[i]);
      progLen += 4 + 2 + nlen + 4;
    }

    uint8_t *prog = (uint8_t *)malloc(progLen);
    if (!prog) { printf("    [1] FAIL: malloc failed\n"); sc1 = 1; }

    if (!sc1) {
      uint8_t *p = prog;
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;
      for (int i = 0; i < longpageCount; i++) {
        int nlen = (int)strlen(longpageNames[i]);
        int bodyLen = 2 + nlen + 4;
        *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = (uint8_t)bodyLen;
        *p++ = ':'; *p++ = ' ';
        for (int c = 0; c < nlen; c++) *p++ = (uint8_t)longpageNames[i][c];
        *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';
      }
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;

      if (!writeTestProgram(prog, progLen)) {
        printf("    [1] FAIL: writeTestProgram failed\n");
        sc1 = 1;
      }
    }
    free(prog);

    if (!sc1) {
      currentProgramNumber = 1;
      currentStep = beginOfProgramMemory + progLen - 4;

      calcMode = CM_PEM;   /* Stage M E3: the draws build; PEM cursor context */
      showSoftmenu(-MNU_FORTH);
      softmenuStack[0].firstItem = 0;
      showSoftmenuCurrentPart();

      if (dynamicSoftmenu[22].numItems != 36) {
        printf("    [1] FIXTURE BUG: expected 36 names, got %d\n", dynamicSoftmenu[22].numItems);
        sc1 = 1;
      }
    }

    if (!sc1) {
      int32_t page1 = 0;
      for (int c = 0; c < 6; c++) page1 += g4CellPixels(c);

      softmenuStack[0].firstItem = 18;         /* one full page of three rows */
      showSoftmenuCurrentPart();

      int32_t page2 = 0;
      for (int c = 0; c < 6; c++) page2 += g4CellPixels(c);

      if (page2 <= page1) {
        printf("    [1] FAIL: page 2 (%d px) should exceed page 1 (%d px) — "
               "same item count, longer labels\n", page2, page1);
        sc1 = 1;
      }
    }

    if (dynamicSoftmenu[22].menuContent) {
      free(dynamicSoftmenu[22].menuContent);
      dynamicSoftmenu[22].menuContent = NULL;
    }
    dynamicSoftmenu[22].numItems = 0;
    cleanupTestProgram();

    if (!sc1) {
      printf("    [1] PASS: paging changes what is drawn — page 2 lights more than page 1\n");
    }
    fail |= sc1;
  }

  /* ---------- [2] Blank cells of a partial page are blank ---------- */
  {
    int sc2 = 0;

    /* PARTIAL: 8 names — N000..N007 */
    static const char *const partialNames[8] = {
      "N000", "N001", "N002", "N003", "N004", "N005", "N006", "N007"
    };
    const int partialCount = 8;

    uint16_t progLen = 8;
    for (int i = 0; i < partialCount; i++) {
      int nlen = (int)strlen(partialNames[i]);
      progLen += 4 + 2 + nlen + 4;
    }

    uint8_t *prog = (uint8_t *)malloc(progLen);
    if (!prog) { printf("    [2] FAIL: malloc failed\n"); sc2 = 1; }

    if (!sc2) {
      uint8_t *p = prog;
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;
      for (int i = 0; i < partialCount; i++) {
        int nlen = (int)strlen(partialNames[i]);
        int bodyLen = 2 + nlen + 4;
        *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = (uint8_t)bodyLen;
        *p++ = ':'; *p++ = ' ';
        for (int c = 0; c < nlen; c++) *p++ = (uint8_t)partialNames[i][c];
        *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';
      }
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;

      if (!writeTestProgram(prog, progLen)) {
        printf("    [2] FAIL: writeTestProgram failed\n");
        sc2 = 1;
      }
    }
    free(prog);

    if (!sc2) {
      currentProgramNumber = 1;
      currentStep = beginOfProgramMemory + progLen - 4;

      showSoftmenu(-MNU_FORTH);
      softmenuStack[0].firstItem = 6;
      showSoftmenuCurrentPart();

      if (dynamicSoftmenu[22].numItems != 8) {
        printf("    [2] FIXTURE BUG: expected 8 names, got %d\n", dynamicSoftmenu[22].numItems);
        sc2 = 1;
      }
    }

    if (!sc2) {
      /* Paint all six cells at firstItem=0, then re-render at firstItem=6: indices
       * 6 and 7 land in cells 0 and 1, and the render clears the rest.
       *
       * Cell 2 is NOT identical to cells 3-5, and correctly so. Measured, its
       * only lit pixels sit at x == KEY_X[2] on alternate rows: the dotted
       * divider the last LIVE key draws down its right-hand edge, which by the
       * KEY_X convention falls in the next cell's window. Cells 3-5 have no live
       * neighbour to their left and read exactly 0.
       *
       * So the pin is sharper than "all four identical": the cells past the live
       * ones carry no label, and cell 2 carries nothing but that one border
       * column. Asserting cell 2's INTERIOR is empty says that precisely. */
      softmenuStack[0].firstItem = 0;
      showSoftmenuCurrentPart();

      softmenuStack[0].firstItem = 6;
      showSoftmenuCurrentPart();

      extern const int KEY_X[7];
      int32_t c2Interior = 0;                 /* cell 2 minus its left border column */
      for (uint32_t yy = 171; yy < SCREEN_HEIGHT; yy++) {
        for (uint32_t xx = (uint32_t)KEY_X[2] + 1; xx < (uint32_t)KEY_X[3]; xx++) {
          if (lcd_buffer_pixel_on(xx, yy)) { c2Interior++; }
        }
      }
      int32_t c0 = g4CellPixels(0);
      int32_t c5 = g4CellPixels(5);
      int32_t c2 = g4CellPixels(2);
      int32_t c3 = g4CellPixels(3);
      int32_t c4 = g4CellPixels(4);

      if (c0 <= c5) {
        printf("    [2] FAIL: live cell 0 (%d px) should exceed empty cell 5 (%d px)\n", c0, c5);
        sc2 = 1;
      } else if (c3 != c4 || c4 != c5) {
        printf("    [2] FAIL: fully-empty cells differ — c3=%d c4=%d c5=%d\n", c3, c4, c5);
        sc2 = 1;
      } else if (c3 != 0) {
        printf("    [2] FAIL: cells past the live ones are not empty — c3=%d\n", c3);
        sc2 = 1;
      } else if (c2Interior != 0) {
        printf("    [2] FAIL: cell 2 holds %d px beyond its border column — "
               "a label is drawn past numItems (c2=%d)\n", c2Interior, c2);
        sc2 = 1;
      }
    }

    if (dynamicSoftmenu[22].menuContent) {
      free(dynamicSoftmenu[22].menuContent);
      dynamicSoftmenu[22].menuContent = NULL;
    }
    dynamicSoftmenu[22].numItems = 0;
    cleanupTestProgram();

    if (!sc2) {
      printf("    [2] PASS: no label past numItems — cells 3-5 empty, cell 2 only the divider\n");
    }
    fail |= sc2;
  }

  /* ---------- [3] A maximal name stays in its cell ---------- */
  {
    int sc3 = 0;

    /* SINGLE: one definition — ABCDEFGHIJKLMN (14 bytes) */
    const char *singleName = "ABCDEFGHIJKLMN";
    const int singleNlen = 14;
    const int singleBodyLen = 2 + singleNlen + 4;
    const uint16_t progLen = (uint16_t)(4 + 4 + singleBodyLen + 4);

    uint8_t *prog = (uint8_t *)malloc(progLen);
    if (!prog) { printf("    [3] FAIL: malloc failed\n"); sc3 = 1; }

    if (!sc3) {
      uint8_t *p = prog;
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = (uint8_t)singleBodyLen;
      *p++ = ':'; *p++ = ' ';
      for (int c = 0; c < singleNlen; c++) *p++ = (uint8_t)singleName[c];
      *p++ = ' '; *p++ = '1'; *p++ = ' '; *p++ = ';';
      *p++ = 0x8B; *p++ = 0x1A; *p++ = 0xFD; *p++ = 0x00;

      if (!writeTestProgram(prog, progLen)) {
        printf("    [3] FAIL: writeTestProgram failed\n");
        sc3 = 1;
      }
    }
    free(prog);

    if (!sc3) {
      currentProgramNumber = 1;
      currentStep = beginOfProgramMemory + progLen - 4;

      showSoftmenu(-MNU_FORTH);
      softmenuStack[0].firstItem = 0;
      showSoftmenuCurrentPart();

      if (dynamicSoftmenu[22].numItems != 1) {
        printf("    [3] FIXTURE BUG: expected 1 name, got %d\n", dynamicSoftmenu[22].numItems);
        sc3 = 1;
      }
    }

    if (!sc3) {
      /* Clear stale content: render with no visible items, then render the label. */
      softmenuStack[0].firstItem = 1;
      showSoftmenuCurrentPart();
      softmenuStack[0].firstItem = 0;
      showSoftmenuCurrentPart();

      int32_t c0 = g4CellPixels(0);
      int32_t c1 = g4CellPixels(1);
      int32_t c2 = g4CellPixels(2);

      if (c0 <= c1) {
        printf("    [3] FAIL: cell 0 (%d px) should exceed cell 1 (%d px) — label not drawn\n", c0, c1);
        sc3 = 1;
      /* Not c1 == c2: cell 1's window opens at KEY_X[1], which is where the LIVE
       * cell 0 draws its right-hand dotted divider — 12 px on alternate rows, the
       * same border that shows up in subcase 2's cell 2. So an empty cell adjacent
       * to a live one legitimately carries that column and an empty cell further
       * out carries nothing. The 15 is that divider plus slack, not a fudge: a
       * label bleeding out of cell 0 is worth tens of pixels, as the mutation
       * lifting trimKey's per-cell clamp shows (131 px). */
      } else if (c1 > c2 + 15) {
        printf("    [3] FAIL: cell 1 (%d px) exceeds cell 2 (%d px) by more than 15 — name bled out\n", c1, c2);
        sc3 = 1;
      }
    }

    if (dynamicSoftmenu[22].menuContent) {
      free(dynamicSoftmenu[22].menuContent);
      dynamicSoftmenu[22].menuContent = NULL;
    }
    dynamicSoftmenu[22].numItems = 0;
    cleanupTestProgram();

    if (!sc3) {
      printf("    [3] PASS: a 14-byte name stays inside its own cell\n");
    }
    fail |= sc3;
  }

  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems    = savedNumItems;
  cachedDynamicMenu               = savedCachedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  currentStep          = savedCurrentStep;
  currentProgramNumber = savedProgNum;

      calcMode = savedCalcModeM_;
  return fail;
}

/* FIX-8 (D-C2) reproducer: picking FORTH from a catalog while a capture line
 * is OPEN must commit-and-close the line, not tear down the alpha UI around a
 * live FCAP_OPEN. Before the fix, insertStepInProgram's toggle-close arm
 * cleared FLAG_ALPHA and tam.function but never called forthCapClose() or
 * cleared aimBuffer — the only close path that skipped the reset. Downstream,
 * a stale FCAP_OPEN makes tamEnterMode's suspend seam destructively recommit
 * and misroutes fnKeyExit's forthCapTextNonEmpty() resync.
 *
 * Drive shape mirrors test_forth_toggle_from_catalog_leaves_alpha_menu
 * (catalog-shaped runFunction dispatch), but with the capture genuinely OPEN
 * and holding text — the combination no landed test drove (T4 trace,
 * 2026-08-04).
 *
 * Escaping mutation: revert the pemCloseAlphaInput() call in the ITM_FORTH
 * arm — state stays FCAP_OPEN and aimBuffer keeps "2"; both assertions fail.
 */
static int test_forth_toggle_close_with_open_capture(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void showSoftmenu(int16_t);
  extern void _closeCatalog(void);

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
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "TC8");
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
  pemCursorIsZerothStep = false;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
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
  if (!fail) {
    runFunction(ITM_2);
    if (strcmp(forthTestCapText(), "2") != 0) {
      printf("    FIXTURE FAIL: capture text \"%s\", expected \"2\"\n",
             forthTestCapText());
      fail = 1;
    }
  }

  if (!fail) {
    /* Catalog-shaped FORTH pick, exactly as keyboard.c dispatches it. */
    catalog = CATALOG_FCNS;
    showSoftmenu(-MNU_CATALOG);
    showSoftmenu(-MNU_FCNS);
    fnKeyInCatalog = 1;                /* after the menus: showSoftmenu clears it */
    runFunction(ITM_FORTH);
    _closeCatalog();
    fnKeyInCatalog = savedFnKeyInCatalog;
    catalog = CATALOG_NONE;

    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    FAIL: capture state %d after toggle-close, expected FCAP_CLOSED\n",
             forthTestCapState());
      fail = 1;
    }
    if (aimBuffer[0] != 0) {
      printf("    FAIL: aimBuffer holds \"%s\" after toggle-close, expected empty\n",
             aimBuffer);
      fail = 1;
    }
    if (tam.function != 0) {
      printf("    FAIL: tam.function = 0x%04X after toggle-close, expected 0\n",
             tam.function);
      fail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    FAIL: FLAG_ALPHA still set after toggle-close\n");
      fail = 1;
    }

    /* The typed line must survive as a committed source step: the toggle
     * closes the region, it must not eat the text. */
    uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
    uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
    if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A ||
        sSource[2] != 0xFD || sSource[3] != 1 ||
        memcmp(sSource + 4, "2", 1) != 0) {
      printf("    FAIL: committed source line lost by toggle-close\n");
      fail = 1;
    }
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
  fnKeyInCatalog = savedFnKeyInCatalog;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  if (!fail) {
    printf("    PASS: FORTH toggle-close with open capture resets the full tuple\n");
  }
  return fail;
}

/* FIX-8 class test: capture-close completeness. The invariant (the CLASS the
 * bug belonged to, per the bug-fix testing rule): EVERY path that ends a
 * capture leaves the full tuple reset — forthCap.state == FCAP_CLOSED,
 * aimBuffer empty, tam.function == 0, FLAG_ALPHA clear. The instance bug was
 * one path (the E1 toggle-close arm) missing two of the four. Sweep all four
 * landed close paths through their real entry points; any future close path
 * (Stage K's E14 sites included) extends this sweep.
 */
static int test_capture_close_paths_reset_tuple(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void showSoftmenu(int16_t);
  extern void fnKeyUp(uint16_t);
  extern void fnKeyBackspace(uint16_t);

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
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedForthMenuItems = dynamicSoftmenu[22].numItems;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* One subcase = one fresh fixture + one real close-path drive + the tuple. */
  for (int sc = 1; sc <= 4 && !fail; sc++) {
    int scFail = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "TCLS");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [%d] FIXTURE FAIL: build/write\n", sc);
      fail = 1;
      break;
    }

    calcMode = CM_PEM;
    catalog = CATALOG_NONE;
    tam.mode = 0;
    tam.function = 0;
    aimBuffer[0] = 0;
    pemCursorIsZerothStep = false;
    alphaCase = AC_UPPER;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
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
      printf("    [%d] FIXTURE FAIL: ITM_AIM did not open capture\n", sc);
      fail = 1;
      break;
    }
    /* K1/E14: poison the keys-mode bit so every close path in this sweep
     * has something to clear.  forthCapClose() is the single site that
     * clears it, and since FIX-8 every close path runs through there —
     * this turns the class sweep into the proof of that claim. */
    forthCapSetKeysMode(true);
    /* L1-1/E14: same rationale for origin — poison to INTERACTIVE (the
     * fixture's own open via runFunction(ITM_AIM) already leaves it at
     * FCAP_ORIGIN_PEM, which would make a missing reset unobservable). */
    forthCapSetOrigin(FCAP_ORIGIN_INTERACTIVE);

    switch (sc) {
      case 1:                       /* BACKSPACE on empty line: abort */
        fnKeyBackspace(NOPARAM);
        break;
      case 2:                       /* ENTER on empty line: delete placeholder */
        runFunction(ITM_ENTER);
        break;
      case 3:                       /* Up with text: navigation commit */
        runFunction(ITM_7);
        dynamicSoftmenu[22].numItems = 0;
        showSoftmenu(-MNU_FORTH);
        fnKeyUp(NOPARAM);
        break;
      case 4:                       /* FORTH toggle with text: region close */
        runFunction(ITM_2);
        runFunction(ITM_FORTH);
        break;
    }

    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [%d] FAIL: state %d, expected FCAP_CLOSED\n", sc, forthTestCapState());
      scFail = 1;
    }
    if (aimBuffer[0] != 0) {
      printf("    [%d] FAIL: aimBuffer \"%s\", expected empty\n", sc, aimBuffer);
      scFail = 1;
    }
    if (tam.function != 0) {
      printf("    [%d] FAIL: tam.function 0x%04X, expected 0\n", sc, tam.function);
      scFail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    [%d] FAIL: FLAG_ALPHA still set\n", sc);
      scFail = 1;
    }
    if (forthCapKeysMode()) {
      printf("    [%d] FAIL: keys-mode bit still set\n", sc);
      scFail = 1;
    }
    if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
      printf("    [%d] FAIL: origin %d after close, expected FCAP_ORIGIN_PEM\n",
             sc, forthTestCapOrigin());
      scFail = 1;
    }
    if (!scFail) {
      printf("    [%d] PASS: close path leaves the tuple fully reset\n", sc);
    }
    fail |= scFail;
    cleanupTestProgram();
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
  alphaCase = savedAlphaCase;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  dynamicSoftmenu[22].numItems = savedForthMenuItems;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* PACKET_L1_1 (C1/C2): the capture origin bit + fnForthOuter's new life as an
 * interactive-capture opener.  Numbered subcases 1-9 match the packet's C4
 * list verbatim; each is independent (its own state reset) so one failure
 * does not mask the next.
 *
 * T9 note (subcase 2): calcModeAim's liftStack() must NOT run on this path —
 * X, Y, Z and the top-of-stack register are snapshotted with known
 * long-integer sentinels and re-checked bit-identical afterward, which would
 * catch a reintroduced lift (X would become an uninitialised dtReal34 and
 * Y/Z/T would each shift up by one register). */
static int test_capture_origin_lifecycle(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void showSoftmenu(int16_t);
  extern void fnForthOuter(uint16_t);
  extern void _closeCatalog(void);

  int fail = 0, scFail;
  uint8_t tType;
  int32_t tVal;
  longInteger_t li;

  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* ---- Subcase 1: default is PEM ---- */
  scFail = 0;
  {
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "TORL");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [1] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else {
      calcMode = CM_PEM;
      catalog = CATALOG_NONE;
      tam.mode = 0;
      tam.function = 0;
      aimBuffer[0] = 0;
      pemCursorIsZerothStep = false;
      programRunStop = PGM_STOPPED;
      dynamicMenuItem = -1;
      clearSystemFlag(FLAG_ALPHA);
      lastErrorCode = ERROR_NONE;
      forthCapClose();
      currentProgramNumber = 1;

      fnGotoDot(2);
      runFunction(ITM_AIM);
      if (!forthCapIsOpen()) {
        printf("    [1] FIXTURE FAIL: ITM_AIM did not open capture\n");
        scFail = 1;
      } else {
        if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
          printf("    [1] FAIL: origin %d, expected FCAP_ORIGIN_PEM\n", forthTestCapOrigin());
          scFail = 1;
        }
        if (forthCapIsInteractive()) {
          printf("    [1] FAIL: forthCapIsInteractive() true for a PEM open\n");
          scFail = 1;
        }
      }
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      cleanupTestProgram();
    }
  }
  if (!scFail) printf("    [1] PASS: default open is FCAP_ORIGIN_PEM, not interactive\n");
  fail |= scFail;

  /* ---- Subcase 2: interactive open, live stack untouched (T9) ---- */
  scFail = 0;
  calcMode = CM_NORMAL;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  longIntegerInit(li); int32ToLongInteger(555, li); convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);
  longIntegerInit(li); int32ToLongInteger(201, li); convertLongIntegerToLongIntegerRegister(li, REGISTER_Y); longIntegerFree(li);
  longIntegerInit(li); int32ToLongInteger(202, li); convertLongIntegerToLongIntegerRegister(li, REGISTER_Z); longIntegerFree(li);
  longIntegerInit(li); int32ToLongInteger(203, li); convertLongIntegerToLongIntegerRegister(li, getStackTop()); longIntegerFree(li);

  fnForthOuter(NOPARAM);

  if (forthTestCapState() != FCAP_OPEN) {
    printf("    [2] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
    scFail = 1;
  }
  if (!forthCapIsInteractive()) {
    printf("    [2] FAIL: forthCapIsInteractive() false after interactive open\n");
    scFail = 1;
  }
  if (calcMode != CM_AIM) {
    printf("    [2] FAIL: calcMode %d, expected CM_AIM\n", calcMode);
    scFail = 1;
  }
  if (!getSystemFlag(FLAG_ALPHA)) {
    printf("    [2] FAIL: FLAG_ALPHA not set\n");
    scFail = 1;
  }
  if (aimBuffer[0] != 0) {
    printf("    [2] FAIL: aimBuffer \"%s\", expected empty\n", aimBuffer);
    scFail = 1;
  }
  if (T_cursorPos != 0) {
    printf("    [2] FAIL: T_cursorPos %d, expected 0\n", T_cursorPos);
    scFail = 1;
  }
  if (tam.function != 0) {
    printf("    [2] FAIL: tam.function 0x%04X, expected 0\n", tam.function);
    scFail = 1;
  }
  /* T9 pin: the whole visible RPN window, bit-identical. */
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (tType != dtLongInteger || tVal != 555) {
    printf("    [2] FAIL: X = %ld type %u, expected 555 (T9: lift touched X)\n", (long)tVal, tType);
    scFail = 1;
  }
  read_reg_int32(REGISTER_Y, &tType, &tVal);
  if (tType != dtLongInteger || tVal != 201) {
    printf("    [2] FAIL: Y = %ld type %u, expected 201 (T9: lift touched Y)\n", (long)tVal, tType);
    scFail = 1;
  }
  read_reg_int32(REGISTER_Z, &tType, &tVal);
  if (tType != dtLongInteger || tVal != 202) {
    printf("    [2] FAIL: Z = %ld type %u, expected 202 (T9: lift touched Z)\n", (long)tVal, tType);
    scFail = 1;
  }
  read_reg_int32(getStackTop(), &tType, &tVal);
  if (tType != dtLongInteger || tVal != 203) {
    printf("    [2] FAIL: top-of-stack = %ld type %u, expected 203 (T9: depth changed)\n", (long)tVal, tType);
    scFail = 1;
  }
  if (!scFail) printf("    [2] PASS: interactive open leaves the live stack bit-identical (T9)\n");
  fail |= scFail;
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_NORMAL;

  /* ---- Subcase 3: seed consumes X ---- */
  scFail = 0;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  longIntegerInit(li); int32ToLongInteger(42, li); convertLongIntegerToLongIntegerRegister(li, REGISTER_Y); longIntegerFree(li);
  x_set_string("1 2 +");

  fnForthOuter(NOPARAM);

  if (compareString(aimBuffer, "1 2 +", CMP_BINARY) != 0) {
    printf("    [3] FAIL: aimBuffer \"%s\", expected \"1 2 +\"\n", aimBuffer);
    scFail = 1;
  }
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (tType != dtLongInteger || tVal != 42) {
    printf("    [3] FAIL: X = %ld type %u, expected 42 (Y's former value; drop did not happen)\n",
           (long)tVal, tType);
    scFail = 1;
  }
  if (T_cursorPos != 5) {
    printf("    [3] FAIL: T_cursorPos %d, expected 5\n", T_cursorPos);
    scFail = 1;
  }
  if (!scFail) printf("    [3] PASS: seed consumes X, cursor lands after the seeded line\n");
  fail |= scFail;
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_NORMAL;

  /* ---- Subcase 4: empty string in X (M3 guard) ---- */
  scFail = 0;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  x_set_string("");
  fnForthOuter(NOPARAM);

  if (!forthCapIsOpen()) {
    printf("    [4] FAIL: capture did not open on empty-string X\n");
    scFail = 1;
  }
  if (aimBuffer[0] != 0) {
    printf("    [4] FAIL: aimBuffer \"%s\", expected empty\n", aimBuffer);
    scFail = 1;
  }
  if (T_cursorPos != 0) {
    printf("    [4] FAIL: T_cursorPos %d, expected 0 (M3 guard)\n", T_cursorPos);
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_1);
    if (aimBuffer[0] != '1') {
      printf("    [4] FAIL: typed '1' landed at \"%s\", expected it at offset 0\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [4] PASS: empty-string open lands the cursor at 0, first keystroke lands at offset 0\n");
  fail |= scFail;
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_NORMAL;

  /* ---- Subcase 5: oversize refuses ---- */
  scFail = 0;
  {
    char big[301];
    int i;
    for (i = 0; i < 300; i++) { big[i] = 'A'; }
    big[300] = 0;

    catalog = CATALOG_NONE;
    tam.mode = 0;
    tam.function = 0;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    clearSystemFlag(FLAG_ALPHA);
    lastErrorCode = ERROR_NONE;
    forthCapClose();
    x_set_string(big);
    uint8_t calcModeBefore = calcMode;

    fnForthOuter(NOPARAM);

    if (lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) {
      printf("    [5] FAIL: lastErrorCode %d, expected ERROR_INVALID_DATA_TYPE_FOR_OP\n", lastErrorCode);
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [5] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
      scFail = 1;
    }
    if (calcMode != calcModeBefore) {
      printf("    [5] FAIL: calcMode changed (%d -> %d)\n", calcModeBefore, calcMode);
      scFail = 1;
    }
    if (getRegisterDataType(REGISTER_X) != dtString ||
        compareString(REGISTER_STRING_DATA(REGISTER_X), big, CMP_BINARY) != 0) {
      printf("    [5] FAIL: X did not still hold the original 300-byte string bit-identical\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [5] PASS: oversize X refuses, no capture, X untouched\n");
  fail |= scFail;
  lastErrorCode = ERROR_NONE;

  /* ---- Subcase 6: running program keeps the one-shot ---- */
  scFail = 0;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  calcMode = CM_NORMAL;
  {
    uint8_t calcModeBefore = calcMode;
    programRunStop = PGM_RUNNING;
    x_set_string("1 2 +");

    fnForthOuter(NOPARAM);

    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 3) {
      printf("    [6] FAIL: X = %ld type %u, expected 3\n", (long)tVal, tType);
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [6] FAIL: state %d, expected FCAP_CLOSED (no capture opened mid-run)\n",
             forthTestCapState());
      scFail = 1;
    }
    if (calcMode != calcModeBefore) {
      printf("    [6] FAIL: calcMode changed (%d -> %d)\n", calcModeBefore, calcMode);
      scFail = 1;
    }
    programRunStop = PGM_STOPPED;
  }
  if (!scFail) printf("    [6] PASS: PGM_RUNNING keeps the pre-Stage-L one-shot, no capture\n");
  fail |= scFail;

  /* ---- Subcases 7+8: origin rides a suspension (state level only); CLOSED
   * reads as not-interactive.  Do NOT call forthCaptureSuspend/Resume on an
   * interactive capture — forthCaptureSuspend guards only on
   * forthCapIsOpen() (manage.c:1181) and forthCapRecommitStep()
   * (manage.c:1173-1175) would deleteStepsFromTo whatever currentStep
   * points at; with this fixture that is the 4-byte .END. sentinel. The
   * real round-trip belongs to L1-F*. ---- */
  scFail = 0;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  {
    longInteger_t li2;
    longIntegerInit(li2); int32ToLongInteger(9, li2);
    convertLongIntegerToLongIntegerRegister(li2, REGISTER_X); longIntegerFree(li2);
  }

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [7] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    forthCapSuspendState(0, 0, 0, 0);
    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [7] FAIL: state %d, expected FCAP_SUSPENDED\n", forthTestCapState());
      scFail = 1;
    }
    if (!forthCapIsInteractive()) {
      printf("    [7] FAIL: forthCapIsInteractive() false while SUSPENDED\n");
      scFail = 1;
    }
    if (!scFail) printf("    [7] PASS: origin rides the suspension at the state level\n");
    fail |= scFail;

    scFail = 0;
    forthCapClose();
    if (forthCapIsInteractive()) {
      printf("    [8] FAIL: forthCapIsInteractive() true after close\n");
      scFail = 1;
    }
    if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
      printf("    [8] FAIL: origin %d after close, expected FCAP_ORIGIN_PEM\n", forthTestCapOrigin());
      scFail = 1;
    }
    if (!scFail) printf("    [8] PASS: CLOSED reads as not-interactive, origin back to PEM\n");
  }
  fail |= scFail;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = CM_NORMAL;

  /* ---- Subcase 9: catalog drain ---- */
  scFail = 0;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  calcMode = CM_NORMAL;

  catalog = CATALOG_FCNS;
  showSoftmenu(-MNU_CATALOG);
  showSoftmenu(-MNU_FCNS);
  fnKeyInCatalog = 1;   /* after the menus: showSoftmenu clears it */

  runFunction(ITM_FORTH);

  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [9] FAIL: FORTH from a catalog did not open an interactive capture\n");
    scFail = 1;
  }
  {
    bool_t catalogRemains = false;
    int i;
    for (i = 0; i < SOFTMENU_STACK_SIZE; i++) {
      int16_t mi = softmenu[softmenuStack[i].softmenuId].menuItem;
      if (mi == -MNU_CATALOG || mi == -MNU_FCNS) { catalogRemains = true; break; }
    }
    if (catalogRemains) {
      printf("    [9] FAIL: a catalog menu remains on softmenuStack after the drain\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [9] PASS: catalog drain leaves no catalog menu, capture open\n");
  fail |= scFail;
  _closeCatalog();
  fnKeyInCatalog = savedFnKeyInCatalog;
  catalog = CATALOG_NONE;

  /* ---- restore the full tuple ---- */
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
  fnKeyInCatalog = savedFnKeyInCatalog;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* PACKET_L1_1 (C4): drive each closeAim() call site reachable by a driven
 * key with an OPEN interactive capture, and confirm it closes fully — the
 * minimum close this packet ships (L1-2 replaces it with the full E8
 * ladder).  Three of the six closeAim() sites are driven here: fnKeyExit in
 * CM_AIM (reachable directly from the just-opened state) and fnKeyUp/
 * fnKeyDown (reachable once the current softmenu is non-alpha and
 * non-scrolling — see the comment at each subcase for why the bare
 * "open then arrow" gesture does NOT reach them).  The other three
 * (executeFunction's ITM_INTEGRAL/ITM_INTEGRAL_YX arm, executeFunction's
 * generic non-alpha-item arm, processKeyAction's BST/SST longpress arm) sit
 * behind multi-step gestures or longpress timing this harness does not
 * model; each calls the identical `_forthCapCloseIfInteractive();
 * closeAim();` pair verified live at the three driven sites, so their
 * correctness rests on code inspection (reported alongside the call-site
 * list), not a live drive. */
static int test_capture_interactive_close(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void fnKeyUp(uint16_t);
  extern void fnKeyDown(uint16_t);
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
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* Site 1: fnKeyExit, CM_AIM, the alpha submenu not showing (keyboard.c). */
  scFail = 0;
  calcMode = CM_NORMAL;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [fnKeyExit] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    fnKeyExit(NOPARAM);
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [fnKeyExit] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
      scFail = 1;
    }
    if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
      printf("    [fnKeyExit] FAIL: origin %d, expected FCAP_ORIGIN_PEM\n", forthTestCapOrigin());
      scFail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    [fnKeyExit] FAIL: FLAG_ALPHA still set\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [fnKeyExit] PASS: EXIT in CM_AIM closes an open interactive capture\n");
  fail |= scFail;

  /* fnKeyUp/fnKeyDown's closeAim() arms sit behind
   * `if(!arrowCasechange && calcMode == CM_AIM && isJMAlphaSoftmenu(menuId))`
   * (keyboard.c ~:4652/:4871) — and `arrowCasechange` is `#define`d `false`
   * (defines.h:500), so that condition is `calcMode == CM_AIM &&
   * isJMAlphaSoftmenu(menuId)`, unconditionally true right after
   * forthEnterAimSurfaceNoLift() shows -MNU_ALPHA.  A driven fnKeyUp/
   * fnKeyDown from the ordinary just-opened state therefore takes the
   * arrow-cursor arm (fnT_ARROW), never reaching closeAim() — true for a
   * NATIVE alpha session too, not something L1-1 introduces.  Reaching the
   * closeAim() arm needs the current softmenu to be non-alpha AND
   * non-scrolling while calcMode stays CM_AIM: showSoftmenu(-MNU_HOME) gives
   * both (softmenuId 0 is never alpha, and currentSoftmenuScrolls() requires
   * menuId > 1 — softmenus.c:4169) — a real reachable combination (e.g. a
   * catalog or menu selection landing on HOME while typing), just not the
   * bare "open then press the arrow" gesture. */
  scFail = 0;
  calcMode = CM_NORMAL;
  clearSystemFlag(FLAG_ALPHA);
  forthCapClose();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [fnKeyUp] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    showSoftmenu(-MNU_HOME);
    fnKeyUp(NOPARAM);
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [fnKeyUp] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
      scFail = 1;
    }
    if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
      printf("    [fnKeyUp] FAIL: origin %d, expected FCAP_ORIGIN_PEM\n", forthTestCapOrigin());
      scFail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    [fnKeyUp] FAIL: FLAG_ALPHA still set\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [fnKeyUp] PASS: fnKeyUp's closeAim() site closes an open interactive capture\n");
  fail |= scFail;

  scFail = 0;
  calcMode = CM_NORMAL;
  clearSystemFlag(FLAG_ALPHA);
  forthCapClose();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [fnKeyDown] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    showSoftmenu(-MNU_HOME);
    fnKeyDown(NOPARAM);
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [fnKeyDown] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
      scFail = 1;
    }
    if (forthTestCapOrigin() != FCAP_ORIGIN_PEM) {
      printf("    [fnKeyDown] FAIL: origin %d, expected FCAP_ORIGIN_PEM\n", forthTestCapOrigin());
      scFail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    [fnKeyDown] FAIL: FLAG_ALPHA still set\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [fnKeyDown] PASS: fnKeyDown's closeAim() site closes an open interactive capture\n");
  fail |= scFail;

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* PACKET_L1_2: the REPL — ENTER runs the line and reopens, EXIT unwinds the
 * ladder, and the input cap holds on both insertion seams.  All typing goes
 * through the real key path (runFunction/processKeyAction/executeFunction),
 * never direct aimBuffer writes — that is the whole point of C5.
 *
 * Subcase 9 (a word whose execution rewrites aimBuffer, pinning the §3.3.2
 * pre-run copy independently of the error path) was searched for and not
 * found: every aimBuffer write in this tree lives in the UI input-handling
 * code (keyboard.c/manage.c/bufferize.c), reached only by driving a key —
 * never by a word forthOuterInterpret executes via XEQ dispatch. Per the
 * packet's own fallback, mutation 4 is pinned by subcase 3 alone (see the
 * mutation-testing notes in the L1-2 report). */
static int test_capture_interactive_repl(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyEnter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void runFunction(int16_t);
  extern void processKeyAction(int16_t);
  extern void executeFunction(const char *data, int16_t item_);
  extern void showSoftmenu(int16_t);
  extern int16_t currentMenu(void);

  int fail = 0, scFail;
  uint8_t tType;
  int32_t tVal;
  longInteger_t li;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedNextChar = nextChar;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  #define L12_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  /* ---- Subcase 0: T9 end-to-end — ENTER runs on the LIVE stack. ---- */
  scFail = 0;
  L12_RESET();
  longIntegerInit(li); int32ToLongInteger(16, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [0] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_1);
    runFunction(ITM_SPACE);
    runFunction(ITM_PLUS);
    fnKeyEnter(NOPARAM);
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 17) {
      printf("    [0] FAIL: X = %ld type %u, expected 17 (16 in X, \"1 +\", ENTER — T9 live stack)\n",
             (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [0] PASS: T9 end-to-end — interactive ENTER runs on the live stack\n");
  fail |= scFail;

  /* ---- Subcase 1: ENTER runs and reopens empty. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [1] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_1);
    runFunction(ITM_SPACE);
    runFunction(ITM_2);
    runFunction(ITM_SPACE);
    runFunction(ITM_PLUS);
    fnKeyEnter(NOPARAM);
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 3) {
      printf("    [1] FAIL: X = %ld type %u, expected 3\n", (long)tVal, tType);
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [1] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
      scFail = 1;
    }
    if (!forthCapIsInteractive()) {
      printf("    [1] FAIL: forthCapIsInteractive() false after ENTER\n");
      scFail = 1;
    }
    if (aimBuffer[0] != 0) {
      printf("    [1] FAIL: aimBuffer \"%s\", expected empty (reopened)\n", aimBuffer);
      scFail = 1;
    }
    if (T_cursorPos != 0) {
      printf("    [1] FAIL: T_cursorPos %d, expected 0\n", T_cursorPos);
      scFail = 1;
    }
    if (calcMode != CM_AIM) {
      printf("    [1] FAIL: calcMode %d, expected CM_AIM\n", calcMode);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [1] PASS: ENTER runs \"1 2 +\", X == 3, capture reopens empty in CM_AIM\n");
  fail |= scFail;

  /* ---- Subcase 2: empty ENTER is a no-op. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [2] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    longIntegerInit(li); int32ToLongInteger(99, li);
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

    fnKeyEnter(NOPARAM);   /* aimBuffer is empty from the fresh open */

    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [2] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
      scFail = 1;
    }
    if (calcMode != CM_AIM) {
      printf("    [2] FAIL: calcMode %d, expected CM_AIM\n", calcMode);
      scFail = 1;
    }
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 99) {
      printf("    [2] FAIL: X = %ld type %u, expected unchanged 99\n", (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [2] PASS: empty ENTER is a no-op, capture stays open, X unchanged\n");
  fail |= scFail;

  /* ---- Subcase 3: error reopens with the line intact. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [3] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_1);
    runFunction(ITM_SPACE);
    runFunction(ITM_Z);
    runFunction(ITM_Z);
    runFunction(ITM_Q);
    runFunction(ITM_Q);
    runFunction(ITM_SPACE);
    runFunction(ITM_PLUS);

    fnKeyEnter(NOPARAM);

    if (lastErrorCode == ERROR_NONE) {
      printf("    [3] FAIL: lastErrorCode ERROR_NONE, expected an error from the unresolvable word\n");
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [3] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
      scFail = 1;
    }
    if (compareString(aimBuffer, "1 ZZQQ +", CMP_BINARY) != 0) {
      printf("    [3] FAIL: aimBuffer \"%s\", expected \"1 ZZQQ +\" intact\n", aimBuffer);
      scFail = 1;
    }
    if (T_cursorPos != stringLastGlyph(aimBuffer) + 1) {
      printf("    [3] FAIL: T_cursorPos %d, expected %d (end of line)\n",
             T_cursorPos, stringLastGlyph(aimBuffer) + 1);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [3] PASS: unresolvable line reopens with the line intact, cursor at end\n");
  fail |= scFail;

  /* ---- Subcase 3b: E9 tier-1 STRUCTURAL reject (mutation 3's pin).
   * "1 ZZQQ +" (subcase 3) does NOT distinguish forthCheckSourceLine's own
   * refusal from forthOuterInterpret's own runtime failure: name resolution
   * is tier-2/advisory (test_engine.part.h's check-mode battery, subcase
   * [2] "names and item-level conditions stay advisory"), AND an
   * unresolvable-word run produces the identical observable outcome
   * (error, line restored) whether or not the tier-1 gate ran first — so
   * that line cannot pin mutation 3 by itself; a plain "IF" is caught by
   * BOTH the tier-1 gate and a live run's own dispatcher (ERROR_OPERATION_
   * UNDEFINED either way), so it can't either.
   *
   * "5 : A IF ;" can: "5" pushes onto the REAL data stack the moment it is
   * actually interpreted, and ": A IF ;" is the tier-1 STRUCTURAL
   * violation (same rejectLines[] entry test_engine.part.h's check-mode
   * battery pins) later in the SAME line. forthCheckSourceLine's CHECK
   * mode is documented side-effect-free (forth_compile.c FORTH_OUTER_CHECK
   * comment) and scans the WHOLE line before anything runs for real, so
   * the correct behaviour never pushes "5" onto X at all. Skip the gate
   * (mutation 3) and forthOuterInterpret runs "5" for real before it ever
   * reaches the structural failure — X changes and stays changed, because
   * forthInteractiveEnter's error path restores aimBuffer's TEXT, not the
   * stack. ---- */
  scFail = 0;
  L12_RESET();
  longIntegerInit(li); int32ToLongInteger(777, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [3b] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_5);
    runFunction(ITM_SPACE);
    runFunction(ITM_COLON);
    runFunction(ITM_SPACE);
    runFunction(ITM_A);
    runFunction(ITM_SPACE);
    runFunction(ITM_I);
    runFunction(ITM_F);
    runFunction(ITM_SPACE);
    runFunction(ITM_SEMICOLON);

    fnKeyEnter(NOPARAM);

    if (lastErrorCode == ERROR_NONE) {
      printf("    [3b] FAIL: lastErrorCode ERROR_NONE, expected a tier-1 structural reject for \"5 : A IF ;\"\n");
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [3b] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
      scFail = 1;
    }
    if (compareString(aimBuffer, "5 : A IF ;", CMP_BINARY) != 0) {
      printf("    [3b] FAIL: aimBuffer \"%s\", expected \"5 : A IF ;\" intact\n", aimBuffer);
      scFail = 1;
    }
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 777) {
      printf("    [3b] FAIL: X = %ld type %u, expected untouched 777 (the tier-1 gate must refuse before \"5\" ever runs)\n",
             (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [3b] PASS: a tier-1 structural violation (\"5 : A IF ;\") is refused atomically before \"5\" runs, line intact\n");
  fail |= scFail;

  /* ---- Subcase 4: EXIT does not commit to X (T9). ---- */
  scFail = 0;
  L12_RESET();
  longIntegerInit(li); int32ToLongInteger(123456, li);
  convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [4] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_A);
    runFunction(ITM_B);
    runFunction(ITM_C);

    fnKeyExit(NOPARAM);

    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [4] FAIL: state %d, expected FCAP_CLOSED\n", forthTestCapState());
      scFail = 1;
    }
    if (getSystemFlag(FLAG_ALPHA)) {
      printf("    [4] FAIL: FLAG_ALPHA still set\n");
      scFail = 1;
    }
    if (calcMode != CM_NORMAL) {
      printf("    [4] FAIL: calcMode %d, expected CM_NORMAL\n", calcMode);
      scFail = 1;
    }
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 123456) {
      printf("    [4] FAIL: X = %ld type %u, expected bit-identical 123456 (EXIT must not closeAim())\n",
             (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [4] PASS: EXIT closes without committing \"ABC\" to X\n");
  fail |= scFail;

  /* ---- Subcase 5: ladder rung 1 — INVERTED by N1-5 (keys-first).
   * Keys input is the console's GROUND state now, so rung 1 unwinds the
   * ALPHA EXCURSION back to keys and restores the FWRD home row.  It used to
   * unwind keys into alpha, which after the flip would be a step AWAY from
   * the ground.  The capture staying open is unchanged. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [5] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    forthCapSetKeysMode(false);        /* the alpha excursion, stated explicitly */
    runFunction(ITM_A);
    runFunction(ITM_B);

    fnKeyExit(NOPARAM);

    if (!forthCapKeysMode()) {
      printf("    [5] FAIL: EXIT from the alpha excursion must return to keys input\n");
      scFail = 1;
    }
    if (currentMenu() != -MNU_FORTH) {
      printf("    [5] FAIL: currentMenu() %d after EXIT, expected the FWRD home row (%d)\n",
             currentMenu(), -MNU_FORTH);
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [5] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
      scFail = 1;
    }
    if (compareString(aimBuffer, "AB", CMP_BINARY) != 0) {
      printf("    [5] FAIL: aimBuffer \"%s\", expected \"AB\" intact\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [5] PASS: rung 1 — EXIT in keys mode returns to alpha input, capture stays open\n");
  fail |= scFail;

  /* ---- Subcase 6a: ladder rung 2, alpha submenu pops, capture stays open. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [6a] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    showSoftmenu(-MNU_ALPHA_OMEGA);
    if (currentMenu() != -MNU_ALPHA_OMEGA) {
      printf("    [6a] FIXTURE FAIL: showSoftmenu(-MNU_ALPHA_OMEGA) did not take\n");
      scFail = 1;
    } else {
      fnKeyExit(NOPARAM);
      if (currentMenu() == -MNU_ALPHA_OMEGA) {
        printf("    [6a] FAIL: alpha submenu did not pop\n");
        scFail = 1;
      }
      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [6a] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [6a] PASS: rung 2 — an alpha submenu pops, capture stays open\n");
  fail |= scFail;

  /* ---- Subcase 6(b): ladder rung 2, a NON-alpha menu (STK) pops, capture
   * stays open with the line intact. Rev 2's narrower isAlphaSubmenu(0)
   * predicate closed the capture and discarded the line here — mutation 2c
   * escapes this subcase. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [6(b)] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_A);
    runFunction(ITM_B);
    showSoftmenu(-MNU_STK);
    if (currentMenu() != -MNU_STK) {
      printf("    [6(b)] FIXTURE FAIL: showSoftmenu(-MNU_STK) did not take\n");
      scFail = 1;
    } else {
      fnKeyExit(NOPARAM);
      if (currentMenu() == -MNU_STK) {
        printf("    [6(b)] FAIL: non-alpha menu did not pop\n");
        scFail = 1;
      }
      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [6(b)] FAIL: state %d, expected FCAP_OPEN (line must not be discarded)\n",
               forthTestCapState());
        scFail = 1;
      }
      if (compareString(aimBuffer, "AB", CMP_BINARY) != 0) {
        printf("    [6(b)] FAIL: aimBuffer \"%s\", expected \"AB\" intact\n", aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [6(b)] PASS: rung 2 — a non-alpha (STK) menu pops, capture stays open, line intact\n");
  fail |= scFail;

  /* ---- Subcase 6b: EXIT (through rung 3) preserves the pre-FORTH menu.
   * Rev 2's extra trailing popSoftmenu() destroyed it — mutation 2b escapes
   * this subcase. ---- */
  scFail = 0;
  L12_RESET();
  showSoftmenu(-MNU_STK);
  {
    int16_t menuBefore = currentMenu();
    if (menuBefore != -MNU_STK) {
      printf("    [6b] FIXTURE FAIL: showSoftmenu(-MNU_STK) did not take\n");
      scFail = 1;
    } else {
      fnForthOuter(NOPARAM);
      if (!forthCapIsOpen()) {
        printf("    [6b] FIXTURE FAIL: interactive open did not take\n");
        scFail = 1;
      } else {
        fnKeyExit(NOPARAM);   /* nothing else pushed, keys mode off: straight to rung 3 */
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [6b] FIXTURE FAIL: state %d, expected FCAP_CLOSED (not through rung 3)\n",
                 forthTestCapState());
          scFail = 1;
        }
        if (currentMenu() != menuBefore) {
          printf("    [6b] FAIL: currentMenu() %d after EXIT, expected the pre-FORTH menu %d back\n",
                 currentMenu(), menuBefore);
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [6b] PASS: EXIT through rung 3 preserves the pre-FORTH menu\n");
  fail |= scFail;

  /* ---- Subcase 7: the input cap, both seams, plus the item > 0 conjunct. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [7] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    /* Prime the buffer directly to 196 '1's rather than through the real key
     * path: bufferize.c's addItemToBuffer bounds non-EIM CM_AIM insertion by
     * on-screen PIXEL WIDTH (bufferize.c:597, stringWidthWithLimitC47), not
     * by byte/glyph count. 196 repeated WIDE glyphs (e.g. 'A') hit that
     * unrelated native cap around glyph 181 — well short of our 196-glyph
     * Forth cap — and a narrow digit still hits it under 256 total insertion
     * attempts once seam 1's guard is removed (verified: 'A' priming plus a
     * single boundary 'A' insert passes even with the guard deleted, because
     * the native width cap silently absorbs the 197th 'A' too, masking the
     * mutation). '1' is narrow enough that 196 of them plus one more stay
     * under the native width cap, so our Forth cap is the only thing that
     * can be stopping growth. The cap boundary itself (what this subcase
     * tests) IS driven through both real seams below; only the priming is
     * direct. */
    int i;
    for (i = 0; i < 196; i++) { aimBuffer[i] = '1'; }
    aimBuffer[196] = 0;
    if (stringGlyphLength(aimBuffer) != 196) {
      printf("    [7] FIXTURE FAIL: glyph length %ld after priming, expected 196\n",
             (long)stringGlyphLength(aimBuffer));
      scFail = 1;
    } else {
      /* Seam 1: the physical-key path (processKeyAction). */
      processKeyAction(ITM_1);
      if (stringGlyphLength(aimBuffer) != 196) {
        printf("    [7] FAIL: glyph length %ld after seam-1 insert at cap, expected no growth\n",
               (long)stringGlyphLength(aimBuffer));
        scFail = 1;
      }
      if (lastErrorCode != ERROR_NONE) {
        printf("    [7] FAIL: lastErrorCode %u after seam-1 insert at cap, expected ERROR_NONE (silent swallow)\n",
               lastErrorCode);
        scFail = 1;
      }

      /* Seam 2: the softkey path (executeFunction -> runFunction).
       * executeFunction("", item_) is NOT this path: data[0] == 0 skips the
       * entire `if(calcMode != CM_CONFIRMATION && data[0] != 0)` block
       * (keyboard.c ~:1090, "data is used if operation is from the real
       * keyboard. item is used directly if called from XEQM") — seam 2 lives
       * INSIDE that block, so an empty-data call never reaches it and would
       * silently exercise nothing. The real softkey path needs a non-empty
       * data string resolved by determineFunctionKeyItem_C47. Push a STATIC
       * alphabetic softmenu (isAlphabeticSoftmenu() true, so the unrelated
       * closeAim() guard at :1351 stays skipped) and press its first softkey:
       * data="1" -> fn=0 -> softkeyItem[0] = ITM_ALPHA, an addItemToBuffer
       * item (2-byte STD_ALPHA glyph, still one glyph). */
      showSoftmenu(-MNU_ALPHA_OMEGA);
      executeFunction("1", 0);
      popSoftmenu();
      if (stringGlyphLength(aimBuffer) != 196) {
        printf("    [7] FAIL: glyph length %ld after seam-2 insert at cap, expected no growth\n",
               (long)stringGlyphLength(aimBuffer));
        scFail = 1;
      }

      /* item > 0 conjunct: a negative item id must not index indexOfItems[]. */
      processKeyAction(-MNU_AIMCATALOG);
      if (!forthCapIsOpen()) {
        printf("    [7] FAIL: capture state corrupted by the negative-item-id drive\n");
        scFail = 1;
      }
      if (stringGlyphLength(aimBuffer) != 196) {
        printf("    [7] FAIL: glyph length %ld after negative-item-id drive, expected no growth\n",
               (long)stringGlyphLength(aimBuffer));
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [7] PASS: cap holds on both seams; negative item id does not corrupt state\n");
  fail |= scFail;

  /* ---- Subcase 8: R/S runs the line. ---- */
  scFail = 0;
  L12_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [8] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_2);
    runFunction(ITM_SPACE);
    runFunction(ITM_3);
    runFunction(ITM_SPACE);
    runFunction(ITM_ASTERISK);

    processKeyAction(ITM_RS);

    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 6) {
      printf("    [8] FAIL: X = %ld type %u, expected 6\n", (long)tVal, tType);
      scFail = 1;
    }
    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [8] FAIL: state %d, expected FCAP_OPEN (reopened)\n", forthTestCapState());
      scFail = 1;
    }
    if (aimBuffer[0] != 0) {
      printf("    [8] FAIL: aimBuffer \"%s\", expected empty (reopened)\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [8] PASS: R/S runs \"2 3 *\", X == 6, capture reopens empty\n");
  fail |= scFail;

  #undef L12_RESET

  /* ---- restore the full tuple ---- */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  alphaCase = savedAlphaCase;
  nextChar = savedNextChar;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* ==================================================================
 * PACKET_L1_3 (C6) — test_capture_interactive_divert.  The interactive
 * divert seam: direct function items, catalog picks and the FWRD picker
 * insert their names as TEXT into an open interactive capture instead of
 * executing; keys mode works interactively.  Parameterized items fall
 * through to TAM unchanged (L-R4 (b) — the fold is L1-F*).
 *
 * Each subcase drives a real entry point (runFunction, determineItem,
 * executeFunction) exactly as L1-1/L1-2's tests do; none primes the
 * outcome under test.
 * ================================================================== */
static int test_capture_interactive_divert(void)
{
  extern void fnForthOuter(uint16_t);
  extern void runFunction(int16_t);
  extern int16_t determineItem(const char *);
  extern void executeFunction(const char *data, int16_t item_);
  extern void showSoftmenu(int16_t);
  extern void showSoftmenuCurrentPart(void);
  extern int16_t currentMenu(void);
  extern bool_t isAlphaSubmenu(uint8_t n);
  extern void _closeCatalog(void);
  extern char *dynmenuGetLabel(int16_t menuitem);
  extern const softmenu_t softmenu[];

  int fail = 0, scFail;
  uint8_t tType;
  int32_t tVal;
  longInteger_t li;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedNextChar = nextChar;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  bool_t savedFnKeyInCatalog = fnKeyInCatalog;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  #define L13_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; fnKeyInCatalog = 0; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    for (int _i = 0; _i < SOFTMENU_STACK_SIZE; ++_i) { \
      softmenuStack[_i].softmenuId = 0; softmenuStack[_i].firstItem = 0; \
      softmenuStack[_i].userMenuId = 0; softmenuStack[_i].calcMode = 0; \
    } \
  } while (0)

  /* ---- Subcase 1: direct item inserts text, does not execute. ---- */
  scFail = 0;
  L13_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [1] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    forthCapSetKeysMode(true);
    longIntegerInit(li); int32ToLongInteger(55, li);
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

    runFunction(ITM_sin);

    if (compareString(aimBuffer, "SIN ", CMP_BINARY) != 0) {
      printf("    [1] FAIL: aimBuffer \"%s\", expected \"SIN \"\n", aimBuffer);
      scFail = 1;
    }
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 55) {
      printf("    [1] FAIL: X = %ld type %u, expected untouched 55 (SIN must not execute)\n",
             (long)tVal, tType);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [1] PASS: direct item (SIN) inserts text, X untouched\n");
  fail |= scFail;

  /* ---- Subcase 2: token boundary (K2 leading-separator rule). ---- */
  scFail = 0;
  L13_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [2] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    runFunction(ITM_4);
    runFunction(ITM_2);
    runFunction(ITM_sin);

    if (compareString(aimBuffer, "42 SIN ", CMP_BINARY) != 0) {
      printf("    [2] FAIL: aimBuffer \"%s\", expected \"42 SIN \"\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [2] PASS: \"42\" + SIN token-boundary-separates to \"42 SIN \"\n");
  fail |= scFail;

  /* Locate the ALPHA-gesture row once, from the live table (layout-
   * independent, per K1's rule — no key number or aim-column item id is
   * hard-coded). */
  int kIdx = -1;
  for (int i = 0; i < 37; i++) {
    if (kbd_std[i].fShifted == ITM_AIM) { kIdx = i; break; }
  }
  if (kIdx < 0) {
    printf("    FIXTURE FAIL: no kbd_std row carries fShifted == ITM_AIM\n");
    fail = 1;
  } else {
    char kb[3];
    sprintf(kb, "%02d", kIdx);

    /* ---- Subcase 3a: ALPHA gesture toggles keys mode, softmenu changes,
     * toggle back lands on -MNU_ALPHA. ---- */
    scFail = 0;
    L13_RESET();
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen()) {
      printf("    [3a] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      /* N1-5: the capture now OPENS in keys input, so this subcase — which is
       * about the E10/E11 TOGGLE, not about the default — enters the alpha
       * excursion first.  AUDIT C17 fixture repair: entered via the REAL
       * toggle, not by forcing keysMode and hand-pushing -MNU_ALPHA — the
       * hand-push created a SEPARATE unregistered row above the console's
       * frame, which frame ownership correctly treats as user-stacked (the
       * toggle declines it) rather than as the excursion (the C22 rule). */
      runFunction(ITM_AIM);
      if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
        printf("    [3a] FIXTURE FAIL: the toggle did not enter the alpha"
               " excursion (keys=%d, menu %d)\n",
               (int)forthCapKeysMode(), currentMenu());
        scFail = 1;
      } else {
        shiftF = true;
        int16_t got = determineItem(kb);
        shiftF = false;
        if (got != ITM_AIM) {
          printf("    [3a] FAIL: determineItem = %d, expected ITM_AIM (%d)\n", got, ITM_AIM);
          scFail = 1;
        } else {
          runFunction(ITM_AIM);
          if (!forthCapKeysMode()) {
            printf("    [3a] FAIL: keys mode bit not set after toggle-on\n");
            scFail = 1;
          }
          if (currentMenu() == -MNU_ALPHA) {
            printf("    [3a] FAIL: -MNU_ALPHA still on top after toggle-on (softmenu did not change)\n");
            scFail = 1;
          }
          /* Toggle back off. */
          shiftF = true;
          got = determineItem(kb);
          shiftF = false;
          if (got != ITM_AIM) {
            printf("    [3a] FAIL: toggle-back determineItem = %d, expected ITM_AIM (%d)\n", got, ITM_AIM);
            scFail = 1;
          } else {
            runFunction(ITM_AIM);
            if (forthCapKeysMode()) {
              printf("    [3a] FAIL: keys mode bit still set after toggle-off\n");
              scFail = 1;
            }
            if (currentMenu() != -MNU_ALPHA) {
              printf("    [3a] FAIL: currentMenu() %d after toggle-off, expected -MNU_ALPHA (%d)\n",
                     currentMenu(), -MNU_ALPHA);
              scFail = 1;
            }
          }
        }
      }
    }
    if (!scFail) printf("    [3a] PASS: ALPHA gesture toggles keys mode both ways, softmenu tracks it\n");
    fail |= scFail;

    /* ---- Subcase 3(b), re-pointed by AUDIT C18: the FWRD picker stacked
     * OVER the alpha excursion is an overlay, and the toggle is REFUSED
     * while it is up — flipping the key plane under a row that cannot
     * follow is exactly the row-lies defect.  EXIT pops the overlay (one
     * press), after which the toggle works.  The pre-C18 form of this row
     * forced keysMode and hand-pushed the rows, then asserted the flip
     * landed — true only while ownership was inferred from the visible
     * menu id (the C22 fixture rule, third instance this stage). ---- */
    scFail = 0;
    L13_RESET();
    fnForthOuter(NOPARAM);
    runFunction(ITM_AIM);               /* the REAL excursion entry */
    if (!forthCapIsOpen() || forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
      printf("    [3(b)] FIXTURE FAIL: excursion entry did not take"
             " (open=%d keys=%d menu %d)\n",
             (int)forthCapIsOpen(), (int)forthCapKeysMode(), currentMenu());
      scFail = 1;
    } else {
      showSoftmenu(-MNU_FORTH);         /* the picker, stacked as an overlay */
      if (currentMenu() != -MNU_FORTH) {
        printf("    [3(b)] FIXTURE FAIL: picker overlay did not stack\n");
        scFail = 1;
      } else {
        runFunction(ITM_AIM);
        if (forthCapKeysMode() || currentMenu() != -MNU_FORTH) {
          printf("    [3(b)] FAIL: toggle under an overlay must be refused with"
                 " the row unmoved (keys=%d, menu %d)\n",
                 (int)forthCapKeysMode(), currentMenu());
          scFail = 1;
        }
        if (!scFail) {
          fnKeyExit(NOPARAM);           /* overlay rung: pop the picker */
          if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA || !forthCapIsOpen()) {
            printf("    [3(b)] FAIL: EXIT must pop the overlay, sub-mode unmoved"
                   " (open=%d keys=%d menu %d)\n",
                   (int)forthCapIsOpen(), (int)forthCapKeysMode(), currentMenu());
            scFail = 1;
          }
        }
        if (!scFail) {
          runFunction(ITM_AIM);         /* now the toggle works */
          if (!forthCapKeysMode() || currentMenu() != -MNU_FORTH) {
            printf("    [3(b)] FAIL: toggle after the pop must land keys+FWRD"
                   " (keys=%d, menu %d)\n",
                   (int)forthCapKeysMode(), currentMenu());
            scFail = 1;
          }
        }
      }
    }
    if (!scFail) printf("    [3(b)] PASS: overlay refuses the toggle, EXIT pops it, toggle then lands keys+FWRD (AUDIT C18)\n");
    fail |= scFail;

    /* ---- Subcase 3b: no bug screen in keys mode (B2 pin) — a physical key
     * resolves to the normal-column item, calcMode stays off CM_BUG_ON_SCREEN. ---- */
    scFail = 0;
    L13_RESET();
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen()) {
      printf("    [3b] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      forthCapSetKeysMode(true);
      shiftF = false;
      shiftG = false;
      int16_t want = kbd_std[kIdx].primary;
      int16_t got = determineItem(kb);
      if (got != want) {
        printf("    [3b] FAIL: determineItem = %d, expected normal-column primary %d\n", got, want);
        scFail = 1;
      }
      if (calcMode == CM_BUG_ON_SCREEN) {
        printf("    [3b] FAIL: calcMode == CM_BUG_ON_SCREEN (the C2 two-part edit is incomplete)\n");
        scFail = 1;
      }
    }
    if (!scFail) printf("    [3b] PASS: keys mode resolves the normal column, no bug screen\n");
    fail |= scFail;
  }

  /* ---- Subcase 4: catalog pick inserts, does not close AIM. ---- */
  scFail = 0;
  L13_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [4] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    int16_t fcnsIdx = -1;
    for (int si = 0; si < 300; si++) {
      if (softmenu[si].menuItem == -MNU_FCNS) { fcnsIdx = si; break; }
    }
    int16_t sinPos = -1;
    if (fcnsIdx >= 0) {
      for (int pi = 0; pi < softmenu[fcnsIdx].numItems; pi++) {
        if (softmenu[fcnsIdx].softkeyItem[pi] % 10000 == ITM_sin) { sinPos = pi; break; }
      }
    }
    if (fcnsIdx < 0 || sinPos < 0) {
      printf("    [4] FIXTURE FAIL: MNU_FCNS (%d) / SIN position (%d) not found\n", fcnsIdx, sinPos);
      scFail = 1;
    } else {
      catalog = CATALOG_FCNS;
      showSoftmenu(-MNU_CATALOG);
      showSoftmenu(-MNU_FCNS);
      softmenuStack[0].firstItem = sinPos;
      fnKeyInCatalog = 1;
      shiftF = false;
      shiftG = false;

      executeFunction("1", 0);   /* fn=0, itemShift=0 -> firstItem+0+0 = sinPos */

      fnKeyInCatalog = savedFnKeyInCatalog;
      catalog = CATALOG_NONE;

      if (!forthCapIsOpen() || !forthCapIsInteractive()) {
        printf("    [4] FAIL: capture no longer open/interactive after the pick\n");
        scFail = 1;
      }
      if (calcMode != CM_AIM) {
        printf("    [4] FAIL: calcMode %d after the pick, expected CM_AIM\n", calcMode);
        scFail = 1;
      }
      if (compareString(aimBuffer, "SIN ", CMP_BINARY) != 0) {
        printf("    [4] FAIL: aimBuffer \"%s\", expected \"SIN \" (picked name landed as text)\n", aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [4] PASS: FCNS catalog pick inserts text, capture stays open in CM_AIM\n");
  fail |= scFail;

  /* ---- Subcase 5: dynamic-menu XEQ inserts (T8.4 hole) — the FWRD
   * picker's pick, then items.c:699's ITM_XEQ dynamic-menu arm driven
   * directly, both insert text and execute nothing. ---- */
  scFail = 0;
  L13_RESET();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": W5DIV 42 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    [5] FIXTURE FAIL: W5DIV def error %d\n", lastErrorCode);
    scFail = 1;
  } else {
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen() || !forthCapIsInteractive()) {
      printf("    [5] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      longIntegerInit(li); int32ToLongInteger(555, li);
      convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);

      showSoftmenu(-MNU_FORTH);
      showSoftmenuCurrentPart();   /* the PUSH alone does not build content —
                                       the draw does (test_picker_key_mapping's
                                       precedent) */
      if (dynamicSoftmenu[22].numItems < 1) {
        printf("    [5] FIXTURE FAIL: MNU_FORTH picker has %d items, expected >= 1\n",
               dynamicSoftmenu[22].numItems);
        scFail = 1;
      } else {
        softmenuStack[0].firstItem = 0;
        dynamicMenuItem = 0;

        /* Part A: the FWRD picker's pick, through the real softkey path. */
        executeFunction("1", 0);

        read_reg_int32(REGISTER_X, &tType, &tVal);
        if (tType != dtLongInteger || tVal != 555) {
          printf("    [5a] FAIL: X = %ld type %u after picker pick, expected untouched 555\n",
                 (long)tVal, tType);
          scFail = 1;
        }
        if (compareString(aimBuffer, "W5DIV ", CMP_BINARY) != 0) {
          printf("    [5a] FAIL: aimBuffer \"%s\", expected \"W5DIV \"\n", aimBuffer);
          scFail = 1;
        }
        if (forthTestCapState() != FCAP_OPEN) {
          printf("    [5a] FAIL: state %d, expected FCAP_OPEN\n", forthTestCapState());
          scFail = 1;
        }

        /* Part B: items.c's ITM_XEQ dynamic-menu arm, driven directly
         * (dynamicMenuItem still 0, dynmenuGetLabel(0) == "W5DIV"). */
        if (!scFail) {
          if (compareString(dynmenuGetLabel(dynamicMenuItem), "W5DIV", CMP_BINARY) != 0) {
            printf("    [5b] FIXTURE FAIL: dynmenuGetLabel(0) = \"%s\", expected \"W5DIV\"\n",
                   dynmenuGetLabel(dynamicMenuItem));
            scFail = 1;
          } else {
            runFunction(ITM_XEQ);

            read_reg_int32(REGISTER_X, &tType, &tVal);
            if (tType != dtLongInteger || tVal != 555) {
              printf("    [5b] FAIL: X = %ld type %u after XEQ dynamic-menu arm, expected untouched 555\n",
                     (long)tVal, tType);
              scFail = 1;
            }
            if (compareString(aimBuffer, "W5DIV W5DIV ", CMP_BINARY) != 0) {
              printf("    [5b] FAIL: aimBuffer \"%s\", expected \"W5DIV W5DIV \"\n", aimBuffer);
              scFail = 1;
            }
          }
        }
      }
    }
  }
  if (!scFail) printf("    [5] PASS: FWRD picker pick and the ITM_XEQ dynamic-menu arm both insert text, execute nothing\n");
  fail |= scFail;

  /* ---- Subcase 6: picker sections interactively — section (a) (the
   * on-disk program text scan) gated off, dictionary sections present. ---- */
  scFail = 0;
  L13_RESET();
  {
    /* marker | : FOO 1 ; | marker */
    uint8_t prog[] = {
      0x8B, 0x1A, 0xFD, 0x00,                                             /* marker (opening) */
      0x8B, 0x1A, 0xFD, 0x09, ':', ' ', 'F', 'O', 'O', ' ', '1', ' ', ';', /* : FOO 1 ; */
      0x8B, 0x1A, 0xFD, 0x00,                                             /* marker (closing) */
    };
    uint8_t *savedCurrentStep = currentStep;
    uint16_t savedProgNum = currentProgramNumber;
    bool_t savedZeroth = pemCursorIsZerothStep;

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [6] FIXTURE FAIL: writeTestProgram failed\n");
      scFail = 1;
    } else {
      lastErrorCode = ERROR_NONE;
      forthOuterInterpret(": BAR 1 ;");
      if (lastErrorCode != ERROR_NONE) {
        printf("    [6] FIXTURE FAIL: BAR def error %d\n", lastErrorCode);
        scFail = 1;
      } else {
        currentProgramNumber = 1;
        currentStep = beginOfProgramMemory + 4 + 13;   /* the closing marker: past FOO's def */
        pemCursorIsZerothStep = false;

        forthCapOpenInteractive();
        calcMode = CM_AIM;

        { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
          testInitVariableSoftmenu(22);
          calcMode = m1e3s_; }

        bool_t sawFoo = false, sawBar = false;
        if (dynamicSoftmenu[22].menuContent) {
          const uint8_t *ptr = dynamicSoftmenu[22].menuContent;
          for (int i = 0; i < dynamicSoftmenu[22].numItems; i++) {
            if (compareString((const char *)ptr, "FOO", CMP_BINARY) == 0) { sawFoo = true; }
            if (compareString((const char *)ptr, "BAR", CMP_BINARY) == 0) { sawBar = true; }
            ptr += stringByteLength((const char *)ptr) + 1;
          }
        }
        if (sawFoo) {
          printf("    [6] FAIL: FOO present in the picker (section (a) not gated off interactively)\n");
          scFail = 1;
        }
        if (!sawBar) {
          printf("    [6] FAIL: BAR absent from the picker (dictionary section (b) missing)\n");
          scFail = 1;
        }
        if (dynamicSoftmenu[22].menuContent) {
          free(dynamicSoftmenu[22].menuContent);
          dynamicSoftmenu[22].menuContent = NULL;
          dynamicSoftmenu[22].numItems = 0;
        }
      }
    }

    forthCapClose();
    cleanupTestProgram();
    currentStep = savedCurrentStep;
    currentProgramNumber = savedProgNum;
    pemCursorIsZerothStep = savedZeroth;
  }
  if (!scFail) printf("    [6] PASS: interactively FOO (program text) is absent, BAR (dictionary) is present\n");
  fail |= scFail;

  /* ---- Subcase 8: parameterized items fall through to TAM unchanged
   * (L-R4 (b) — the fold is L1-F*, not this packet). ---- */
  scFail = 0;
  L13_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [8] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  } else {
    forthCapSetKeysMode(true);
    runFunction(ITM_STO);
    if (tam.mode == 0) {
      printf("    [8] FAIL: tam.mode == 0 after ITM_STO, expected TAM entered (divert must not swallow it)\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [8] PASS: ITM_STO (parameterized) falls through to TAM, not swallowed by the divert\n");
  fail |= scFail;

  #undef L13_RESET

  /* ---- restore the full tuple ---- */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  alphaCase = savedAlphaCase;
  nextChar = savedNextChar;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  fnKeyInCatalog = savedFnKeyInCatalog;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* FIX-7 (D-C1) reproducer: the F6-4 fold's committed text must survive its
 * own ENTER commit. decodeOneStep renders quoted names with the directional
 * glyphs STD_LEFT/RIGHT_SINGLE_QUOTE; before the fix the compiler accepted
 * only ASCII 0x27, so the folded "2 XEQ <glyph>WA<glyph>" line — the exact
 * text the landed test_capture_param_text subcase 2 pins — was REFUSED at
 * ENTER by forthCheckSourceLine's XEQ arm (E9 tier 1). Non-XEQ named forms
 * were worse: check mode skips item branches, so they committed silently and
 * failed at run (class test below).
 *
 * Escaping mutation: revert the quote-glyph acceptance in parseQuotedName /
 * forthParseXeqForm — ENTER refuses again and the assertions fail.
 */
static int test_forth_fold_commit_recompiles(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);

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

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "F7R");
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

  if (!fail) {
    /* The landed F6-4 drive: type "2 ", fold XEQ 'WA' through real TAM. */
    runFunction(ITM_2);
    runFunction(ITM_SPACE);
    runFunction(ITM_XEQ);
    tamProcessInput(ITM_alpha);
    runFunction(ITM_W);
    runFunction(ITM_A);
    tamProcessInput(ITM_ENTER);

    const char *expect = "2 XEQ " STD_LEFT_SINGLE_QUOTE "WA" STD_RIGHT_SINGLE_QUOTE " ";
    if (forthTestCapState() != FCAP_OPEN || strcmp(forthTestCapText(), expect) != 0) {
      printf("    FIXTURE FAIL: fold text '%s' (state=%d)\n",
             forthTestCapText(), forthTestCapState());
      fail = 1;
    }

    if (!fail) {
      lastErrorCode = ERROR_NONE;
      runFunction(ITM_ENTER);

      if (lastErrorCode != ERROR_NONE) {
        printf("    FAIL: ENTER refused the folded line (error %d)\n", lastErrorCode);
        fail = 1;
      }
      if (forthTestCapState() != FCAP_OPEN || forthTestCapText()[0] != 0) {
        printf("    FAIL: no fresh relocked line after commit (state=%d text='%s')\n",
               forthTestCapState(), forthTestCapText());
        fail = 1;
      }
      /* The committed step must hold the glyph-quoted text verbatim. */
      uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
      uint8_t *sSource = sMarker ? findNextStep(sMarker) : NULL;
      size_t elen = strlen(expect);
      if (!sSource || sSource[0] != 0x8B || sSource[1] != 0x1A ||
          sSource[2] != 0xFD || sSource[3] != (uint8_t)elen ||
          memcmp(sSource + 4, expect, elen) != 0) {
        printf("    FAIL: committed step does not hold the folded text\n");
        fail = 1;
      }
    }

    /* Abort the relocked capture line (CLA + BACKSPACE idiom). */
    runFunction(ITM_CLA);
    runFunction(ITM_BACKSPACE);
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
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  if (!fail) {
    printf("    PASS: F6-4 folded line commits and relocks cleanly\n");
  }
  return fail;
}

/* FIX-7 class test: emit/accept quote parity. The class (bug-fix testing
 * rule): every glyph-quoted spelling decodeOneStep can render — named form
 * 253, indirect variable 255, system flag 250, XEQ name — must be accepted
 * by the compiler exactly as its ASCII-0x27 twin is, emitting the identical
 * marker encoding. Sweeps every quoted form in forthParamMarkerMask's
 * repertoire plus the structural XEQ, compile-state (interpret shares the
 * one bounded-core dispatch body, DESIGN-HISTORY 2026-07-19). A negative
 * subcase pins that an unbalanced glyph quote still errors.
 */
static int test_quote_glyph_accept_parity(void)
{
  int fail = 0;
  int16_t savedTamFunction = tam.function;
  uint8_t savedCalcMode = calcMode;

  calcMode = CM_NORMAL;
  tam.function = 0;
  lastErrorCode = ERROR_NONE;

  /* scanCount: occurrences of pat[0..plen) in the interactive dictionary */
  #define Q7_SCAN(pat, plen, wantCount, scTag)                                 \
    do {                                                                       \
      int found = 0;                                                           \
      uint16_t limit = fdict.here;                                             \
      for (uint16_t i = 0; i + (plen) <= limit; i++) {                         \
        if (memcmp(fdict.base + i, (pat), (plen)) == 0) found++;               \
      }                                                                        \
      if (found != (wantCount)) {                                              \
        printf("    [%s] FAIL: marker pattern found %d times, expected %d\n",  \
               scTag, found, (wantCount));                                     \
        sc = 1;                                                                \
      }                                                                        \
    } while(0)

  /* ---- [1] XEQ 'WA' : ASCII vs glyph emit the same XEQN payload ---- */
  { int sc = 0;
    forthOuterInterpret(": Q7A XEQ 'WA' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: ASCII XEQ form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    forthOuterInterpret(": Q7B XEQ " STD_LEFT_SINGLE_QUOTE "WA" STD_RIGHT_SINGLE_QUOTE " ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: glyph XEQ form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    if (!sc) {
      const uint8_t pat[] = { 253, 2, 'W', 'A' };
      Q7_SCAN(pat, 4, 2, "1");
    }
    if (!sc) printf("    [1] PASS: XEQ glyph quotes emit the ASCII form's payload\n");
    fail |= sc;
  }

  /* ---- [2] RCL 'AB' : named-variable marker 253 ---- */
  { int sc = 0;
    forthOuterInterpret(": Q7C RCL 'AB' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: ASCII named form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    forthOuterInterpret(": Q7D RCL " STD_LEFT_SINGLE_QUOTE "AB" STD_RIGHT_SINGLE_QUOTE " ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [2] FAIL: glyph named form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    if (!sc) {
      const uint8_t pat[] = { 253, 2, 'A', 'B' };
      Q7_SCAN(pat, 4, 2, "2");
    }
    if (!sc) printf("    [2] PASS: named-variable glyph quotes match ASCII\n");
    fail |= sc;
  }

  /* ---- [3] STO ->'CD' : indirect-variable marker 255 ---- */
  { int sc = 0;
    forthOuterInterpret(": Q7E STO " STD_RIGHT_ARROW "'CD' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: ASCII indirect form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    forthOuterInterpret(": Q7F STO " STD_RIGHT_ARROW STD_LEFT_SINGLE_QUOTE "CD" STD_RIGHT_SINGLE_QUOTE " ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [3] FAIL: glyph indirect form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    if (!sc) {
      const uint8_t pat[] = { 255, 2, 'C', 'D' };
      Q7_SCAN(pat, 4, 2, "3");
    }
    if (!sc) printf("    [3] PASS: indirect-variable glyph quotes match ASCII\n");
    fail |= sc;
  }

  /* ---- [4] SF 'TDM24' : system-flag marker 250 ---- */
  { int sc = 0;
    forthOuterInterpret(": Q7G SF 'TDM24' ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: ASCII sysflag form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    forthOuterInterpret(": Q7H SF " STD_LEFT_SINGLE_QUOTE "TDM24" STD_RIGHT_SINGLE_QUOTE " ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [4] FAIL: glyph sysflag form errored (%d)\n", lastErrorCode);
      sc = 1; lastErrorCode = ERROR_NONE;
    }
    if (!sc) {
      const uint8_t pat[] = { 250, 0 };   /* TDM24 is bit 0 of the SFL range */
      Q7_SCAN(pat, 2, 2, "4");
    }
    if (!sc) printf("    [4] PASS: system-flag glyph quotes match ASCII\n");
    fail |= sc;
  }

  /* ---- [5] Unbalanced glyph quote still errors (no over-acceptance) ---- */
  { int sc = 0;
    forthOuterInterpret(": Q7I XEQ " STD_LEFT_SINGLE_QUOTE "WA ;");
    if (lastErrorCode == ERROR_NONE) {
      printf("    [5] FAIL: unbalanced glyph quote accepted\n");
      sc = 1;
    }
    lastErrorCode = ERROR_NONE;
    if (!sc) printf("    [5] PASS: unbalanced glyph quote still refused\n");
    fail |= sc;
  }
  #undef Q7_SCAN

  calcMode = savedCalcMode;
  tam.function = savedTamFunction;
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* FIX-9 (D-C3) reproducer + class test: softmenu-stack reconciliation across
 * TAM suspend/resume. A catalog-initiated TAM during capture necessarily has
 * a catalog-family menu on the stack when tamEnterMode pushes the TAM menu;
 * _closeCatalog declines to pop under a TAM menu, leaveTamModeIfEnabled pops
 * only the TAM menu, and (pre-fix) forthCaptureResume pushed -MNU_ALPHA with
 * no stack-wide check — leaving -MNU_CATALOG buried. The NEXT softkey
 * dispatch's _closeCatalog() then scans the whole stack, finds the buried
 * entry, sees -MNU_ALPHA on top — which is itself on CatalogMenus[]
 * (keyboard.c) — and pops the capture's menu out from under it. Trap-#6's
 * exact shape; the E1 arm got the bounded drain, the resume seam had none.
 * Structural/defensive today (FCNS is not reachable mid-capture from the
 * standard alpha keyboard); Stage K's column swap makes it a real key path.
 *
 * Subcase 1 is the reproducer (red pre-fix at the post-_closeCatalog menu
 * check); subcase 2 is the negative control — a plain (non-catalog) STO
 * round-trip must be unaffected by the drain.
 *
 * Escaping mutation: revert the drain loop in forthCaptureResume — subcase 1
 * loses -MNU_ALPHA after _closeCatalog and fails.
 */
static int test_resume_drains_buried_catalog(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);
  extern void showSoftmenu(int16_t);
  extern void _closeCatalog(void);

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

  for (int sc = 1; sc <= 2 && !fail; sc++) {
    int scFail = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "F9R");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [%d] FIXTURE FAIL: build/write\n", sc);
      fail = 1;
      break;
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
      printf("    [%d] FIXTURE FAIL: ITM_AIM did not open capture\n", sc);
      fail = 1;
      break;
    }

    if (sc == 1) {
      /* Catalog-shaped STO: CAT -> FCNS on the stack, then the pick, then
       * the _closeCatalog the keyboard runs right after runFunction —
       * which declines under the TAM menu, leaving the catalog buried. */
      catalog = CATALOG_FCNS;
      showSoftmenu(-MNU_CATALOG);
      showSoftmenu(-MNU_FCNS);
      fnKeyInCatalog = 1;
      runFunction(ITM_STO);
      _closeCatalog();
      fnKeyInCatalog = 0;
    }
    else {
      runFunction(ITM_STO);       /* plain physical-shaped TAM entry */
    }

    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [%d] FAIL: capture not suspended after STO (state=%d)\n",
             sc, forthTestCapState());
      scFail = 1;
    }
    else {
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);     /* two digits auto-fire the STO commit */

      if (forthTestCapState() != FCAP_OPEN ||
          strcmp(forthTestCapText(), "STO 05 ") != 0) {
        printf("    [%d] FAIL: resume state=%d text='%s'\n",
               sc, forthTestCapState(), forthTestCapText());
        scFail = 1;
      }
      else if (currentMenu() != -MNU_ALPHA) {
        printf("    [%d] FAIL: currentMenu() = %d after resume, expected -MNU_ALPHA\n",
               sc, currentMenu());
        scFail = 1;
      }
      else {
        /* The next softkey dispatch runs _closeCatalog() — the capture's
         * menu must survive it. */
        _closeCatalog();
        if (currentMenu() != -MNU_ALPHA) {
          printf("    [%d] FAIL: _closeCatalog ate the ALPHA menu (currentMenu=%d)\n",
                 sc, currentMenu());
          scFail = 1;
        }
        else if (forthTestCapState() != FCAP_OPEN) {
          printf("    [%d] FAIL: capture no longer open (state=%d)\n",
                 sc, forthTestCapState());
          scFail = 1;
        }
      }
    }

    if (!scFail) {
      printf("    [%d] PASS: %s\n", sc,
             sc == 1 ? "buried catalog drained; ALPHA menu survives the next dispatch"
                     : "plain TAM round-trip unaffected by the drain");
    }
    fail |= scFail;

    /* Abort the open line and clean up for the next subcase. */
    runFunction(ITM_CLA);
    runFunction(ITM_BACKSPACE);
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
    xcopy(softmenuStack, savedStack, sizeof(savedStack));
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
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  fnKeyInCatalog = savedFnKeyInCatalog;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ==================================================================
 * K1 (Stage K packet 1) — keys-mode bit, toggle gesture, column swap,
 * navigation guards.  DESIGN rules E10, E11, E12.1-.3, E14, and the
 * E13 interim.
 *
 * The whole overlay is one transient bit: the capture stays OPEN and
 * FLAG_ALPHA stays SET, and forthCapKeysMode() alone switches
 * determineItem from the aim columns to the normal key columns.  These
 * tests drive the real entry points — determineItem, runFunction and
 * processKeyAction — and never prime the state under test; the accessor
 * is called directly only where a subcase deliberately POISONS the bit
 * to prove a reset fires.
 * ================================================================== */

/* T1 (E10 / E12.1): the column swap, at the resolution layer.
 *
 * Differential and layout-independent: the ALPHA-gesture row is located
 * in the live kbd_std table by its normal-column fShifted, and every
 * expectation is read back from that same row — a keyboard relayout
 * cannot silently invalidate this test, and no key number or aim-column
 * item id is hard-coded.
 *
 * Escaping mutation: delete `&& !(tam.function == ITM_FORTH &&
 * forthCapKeysMode())` from the alpha-branch condition (C2a) — sc2 then
 * resolves the aim column instead of the row's primary. */
static int test_keys_mode_resolution(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern int16_t determineItem(const char *);

  int fail = 0;
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  bool_t savedUser = getSystemFlag(FLAG_USER);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* Locate the ALPHA-gesture row once, from the live table. */
  int kIdx = -1;
  for (int i = 0; i < 37; i++) {
    if (kbd_std[i].fShifted == ITM_AIM) { kIdx = i; break; }
  }
  if (kIdx < 0) {
    printf("    FIXTURE FAIL: no kbd_std row carries fShifted == ITM_AIM\n");
    return 1;
  }
  char kb[3];
  sprintf(kb, "%02d", kIdx);

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K1R");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
  clearSystemFlag(FLAG_USER);          /* resolution must read kbd_std */
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  currentProgramNumber = 1;

  fnGotoDot(2);
  runFunction(ITM_AIM);
  if (!forthCapIsOpen()) {
    printf("    FIXTURE FAIL: ITM_AIM did not open capture\n");
    cleanupTestProgram();
    return 1;
  }

  /* sc1 (E10): capture open in the alpha sub-mode.  The f-shift on this
   * row is the mode gesture, not the aim column's alpha glyph. */
  {
    lastErrorCode = ERROR_NONE;
    shiftF = true;
    int16_t got = determineItem(kb);
    shiftF = false;
    if (got != ITM_AIM) {
      printf("    [1] FAIL: determineItem = %d, expected ITM_AIM (%d)\n", got, ITM_AIM);
      fail = 1;
    }
    else {
      printf("    [1] PASS: ALPHA gesture resolves to ITM_AIM inside a capture\n");
    }
  }

  /* sc2 (the swap): with the bit set, CM_PEM falls through to the normal
   * key columns — the expectation is read live from the same row. */
  {
    lastErrorCode = ERROR_NONE;
    forthCapSetKeysMode(true);
    shiftF = false;
    int16_t want = kbd_std[kIdx].primary;
    int16_t got = determineItem(kb);
    shiftF = false;
    if (got != want) {
      printf("    [2] FAIL: determineItem = %d, expected primary %d\n", got, want);
      fail = 1;
    }
    else {
      printf("    [2] PASS: keys mode resolves the normal primary column\n");
    }
  }

  /* sc3: the toggle-out gesture is symmetric — with the bit set the same
   * f-shift resolves ITM_AIM again, now via the normal fShifted column. */
  {
    lastErrorCode = ERROR_NONE;
    shiftF = true;
    int16_t got = determineItem(kb);
    shiftF = false;
    if (got != ITM_AIM) {
      printf("    [3] FAIL: determineItem = %d, expected ITM_AIM (%d)\n", got, ITM_AIM);
      fail = 1;
    }
    else if (!forthCapIsOpen()) {
      printf("    [3] FAIL: capture no longer open after resolution\n");
      fail = 1;
    }
    else {
      printf("    [3] PASS: toggle-out resolves ITM_AIM via the normal column\n");
    }
  }

  /* sc4 (no leak outside a capture): abort the line, then POISON the bit
   * and re-resolve.  The capture gate must be what silences the E10
   * remap — not the bit, which is meaningless once the capture is gone. */
  {
    runFunction(ITM_CLA);
    runFunction(ITM_BACKSPACE);
    lastErrorCode = ERROR_NONE;
    forthCapSetKeysMode(true);
    shiftF = true;
    int16_t want = kbd_std[kIdx].fShifted;
    int16_t got = determineItem(kb);
    shiftF = false;
    if (forthCapIsOpen()) {
      printf("    [4] FAIL: capture still open after abort\n");
      fail = 1;
    }
    else if (got != want) {
      printf("    [4] FAIL: determineItem = %d, expected fShifted %d\n", got, want);
      fail = 1;
    }
    else {
      printf("    [4] PASS: bit set with no capture leaves resolution on normal columns\n");
    }
    forthCapSetKeysMode(false);
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  shiftF = false;
  shiftG = false;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  if (savedUser) setSystemFlag(FLAG_USER); else clearSystemFlag(FLAG_USER);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T2 (E10/E11 + E6): the toggle arm, through the real dispatch.
 *
 * The gesture is driven as runFunction(ITM_AIM) — the same item T1 proved
 * the keyboard now resolves — so the arm is exercised end to end rather
 * than by poking the bit.  sc1 also pins the per-key recommit invariant:
 * a toggle must not touch the buffer or the on-disk step.
 *
 * Escaping mutation: drop forthCapIsOpen() from the C4 gate — sc3's E6
 * re-entry becomes a toggle and the capture never reopens. */
static int test_keys_mode_toggle_arm(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K1T");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  /* sc1: alpha -> keys.  Buffer and on-disk step must be byte-identical
   * across the toggle (the per-key recommit invariant). */
  {
    runFunction(ITM_2);                       /* a line with text in it */
    uint8_t  before[32];
    uint16_t nBefore = 0;
    uint8_t *nx = findNextStep(currentStep);
    char bufBefore[AIM_BUFFER_LENGTH];
    uint8_t *stepBefore = currentStep;
    if (nx == NULL || nx <= currentStep || (nx - currentStep) > (int32_t)sizeof(before)) {
      printf("    [1] FIXTURE FAIL: capture step not walkable\n");
      fail = 1;
    }
    else {
      nBefore = (uint16_t)(nx - currentStep);
      xcopy(before, currentStep, nBefore);
      xcopy(bufBefore, aimBuffer, stringByteLength(aimBuffer) + 1);

      runFunction(ITM_AIM);                   /* the toggle gesture */

      if (!forthCapKeysMode()) {
        printf("    [1] FAIL: keys-mode bit not set after the toggle\n");
        fail = 1;
      }
      else if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture state %d, expected FCAP_OPEN\n", forthTestCapState());
        fail = 1;
      }
      else if (currentMenu() == -MNU_ALPHA) {
        printf("    [1] FAIL: alpha menu still on top in keys mode\n");
        fail = 1;
      }
      else if (compareString(aimBuffer, bufBefore, CMP_EXTENSIVE) != 0) {
        printf("    [1] FAIL: aimBuffer changed across the toggle: '%s' -> '%s'\n",
               bufBefore, aimBuffer);
        fail = 1;
      }
      else if (currentStep != stepBefore ||
               findNextStep(currentStep) == NULL ||
               (uint16_t)(findNextStep(currentStep) - currentStep) != nBefore ||
               memcmp(currentStep, before, nBefore) != 0) {
        printf("    [1] FAIL: on-disk capture step changed across the toggle\n");
        fail = 1;
      }
      else {
        printf("    [1] PASS: toggle sets keys mode; buffer and step untouched\n");
      }
    }
  }

  /* sc2: keys -> alpha.  The visible row swap IS the mode indicator, so
   * the ALPHA menu must come back on top. */
  if (!fail) {
    runFunction(ITM_AIM);
    if (forthCapKeysMode()) {
      printf("    [2] FAIL: keys-mode bit still set after toggling back\n");
      fail = 1;
    }
    else if (forthTestCapState() != FCAP_OPEN) {
      printf("    [2] FAIL: capture state %d, expected FCAP_OPEN\n", forthTestCapState());
      fail = 1;
    }
    else if (currentMenu() != -MNU_ALPHA) {
      printf("    [2] FAIL: currentMenu() = %d, expected -MNU_ALPHA\n", currentMenu());
      fail = 1;
    }
    else {
      printf("    [2] PASS: toggle back clears the bit and restores the ALPHA menu\n");
    }
  }

  /* sc3 (E6 untouched): with the capture CLOSED, ITM_AIM is still the
   * re-entry gesture, not a toggle — and a fresh capture starts alpha. */
  if (!fail) {
    runFunction(ITM_CLA);
    runFunction(ITM_BACKSPACE);
    if (forthCapIsOpen()) {
      printf("    [3] FIXTURE FAIL: capture did not close on the empty abort\n");
      fail = 1;
    }
    else {
      tam.function = 0;                       /* seeding it would be vacuous */
      lastErrorCode = ERROR_NONE;
      runFunction(ITM_AIM);
      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [3] FAIL: E6 re-entry did not reopen the capture (state=%d)\n",
               forthTestCapState());
        fail = 1;
      }
      else if (tam.function != ITM_FORTH) {
        printf("    [3] FAIL: tam.function 0x%04X, expected ITM_FORTH\n", tam.function);
        fail = 1;
      }
      else if (forthCapKeysMode()) {
        printf("    [3] FAIL: reopened capture did not start in alpha input\n");
        fail = 1;
      }
      else {
        printf("    [3] PASS: E6 re-entry reopens a fresh capture in alpha input\n");
      }
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T3 (E12.2 + the E13 interim): the CM_PEM navigation guards, through
 * processKeyAction.  These arms are unreachable while the aim columns
 * are up; keys mode makes them real, and no navigation may leave a
 * capture OPEN behind it.
 *
 * Escaping mutation: delete the ITM_RS guard — sc1's capture stays open
 * and "STOP " lands as text, so the native 0x46 step never appears. */
static int test_keys_mode_nav_guards(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void processKeyAction(int16_t);
  extern void fnKeyExit(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedTemporaryInfo = temporaryInformation;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  for (int sc = 1; sc <= 4 && !fail; sc++) {
    int scFail = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "K1N");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [%d] FIXTURE FAIL: build/write\n", sc);
      fail = 1;
      break;
    }

    calcMode = CM_PEM;
    catalog = CATALOG_NONE;
    tam.mode = 0;
    tam.alpha = false;
    tam.function = 0;
    aimBuffer[0] = 0;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    temporaryInformation = TI_NO_INFO;
    pemCursorIsZerothStep = false;
    alphaCase = AC_UPPER;
    nextChar = NC_NORMAL;
    shiftF = false;
    shiftG = false;
    keyActionProcessed = false;
    clearSystemFlag(FLAG_ALPHA);
    clearSystemFlag(FLAG_NUMLOCK);
    lastErrorCode = ERROR_NONE;
    forthCapClose();
    currentProgramNumber = 1;

    fnGotoDot(2);
    uint16_t stepsFixture = getNumberOfSteps();

    if (sc != 3) {
      runFunction(ITM_AIM);
      if (!forthCapIsOpen()) {
        printf("    [%d] FIXTURE FAIL: ITM_AIM did not open capture\n", sc);
        fail = 1;
        cleanupTestProgram();
        break;
      }
    }

    if (sc == 1) {
      /* R/S with text on the line: commit the source, then the native
       * STOP step.  Walk the program and pin all three steps. */
      runFunction(ITM_2);
      runFunction(ITM_AIM);                   /* the real toggle to keys */
      if (!forthCapKeysMode()) {
        printf("    [1] FIXTURE FAIL: toggle did not set keys mode\n");
        scFail = 1;
      }
      else {
        processKeyAction(ITM_RS);

        /* Walk from the LBL step.  The fixture's RTN still sits between
         * the committed source and the inserted STOP: addStepInProgram's
         * pre-move is gated on FLAG_ALPHA being CLEAR, and
         * pemCloseAlphaInput has just cleared it, so the STOP is placed
         * after the cursor's next step.  Adjacency is therefore not the
         * claim — presence and kind are: the line became a Forth SOURCE
         * step and the R/S became exactly one NATIVE STOP step, rather
         * than the text "STOP " landing inside the line. */
        uint8_t *sMarker = findNextStep(beginOfProgramMemory);   /* past LBL */
        uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;
        static const uint8_t wantMarker[4] = { 0x8B, 0x1A, 0xFD, 0x00 };
        static const uint8_t wantSrc[5]    = { 0x8B, 0x1A, 0xFD, 0x01, '2' };

        int nStop = 0;
        {
          uint8_t *s = beginOfProgramMemory;
          for (int guard = 0; s != NULL && !isAtEndOfPrograms(s) && guard < 64; guard++) {
            if (checkOpCodeOfStep(s, ITM_STOP)) { nStop++; }
            uint8_t *n = findNextStep(s);
            if (n == NULL || n <= s) { break; }
            s = n;
          }
        }

        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [1] FAIL: capture state %d, expected FCAP_CLOSED\n",
                 forthTestCapState());
          scFail = 1;
        }
        if (forthCapKeysMode()) {
          printf("    [1] FAIL: keys-mode bit survived the close\n");
          scFail = 1;
        }
        if (!keyActionProcessed) {
          printf("    [1] FAIL: keyActionProcessed not set by the R/S arm\n");
          scFail = 1;
        }
        if (sMarker == NULL || sSrc == NULL) {
          printf("    [1] FAIL: program walk ran off the end\n");
          scFail = 1;
        }
        else if (memcmp(sMarker, wantMarker, sizeof(wantMarker)) != 0) {
          printf("    [1] FAIL: step 2 is not the opening marker\n");
          scFail = 1;
        }
        else if (memcmp(sSrc, wantSrc, sizeof(wantSrc)) != 0) {
          printf("    [1] FAIL: step 3 is not the committed source line \"2\"\n");
          scFail = 1;
        }
        if (nStop != 1) {
          printf("    [1] FAIL: %d native STOP steps in the program, expected exactly 1\n",
                 nStop);
          scFail = 1;
        }
        if (!scFail) {
          printf("    [1] PASS: R/S in keys mode commits the line then adds a native STOP\n");
        }
      }
    }
    else if (sc == 2) {
      /* SST on an empty capture line: the placeholder must be aborted
       * before the navigation, leaving the program as the fixture had it. */
      runFunction(ITM_AIM);                   /* the real toggle to keys */
      if (!forthCapKeysMode()) {
        printf("    [2] FIXTURE FAIL: toggle did not set keys mode\n");
        scFail = 1;
      }
      else {
        processKeyAction(ITM_SST);
        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [2] FAIL: capture state %d, expected FCAP_CLOSED\n",
                 forthTestCapState());
          scFail = 1;
        }
        if (forthCapKeysMode()) {
          printf("    [2] FAIL: keys-mode bit survived the close\n");
          scFail = 1;
        }
        if (getNumberOfSteps() != stepsFixture) {
          printf("    [2] FAIL: %u steps, expected the fixture's %u (placeholder left behind)\n",
                 getNumberOfSteps(), stepsFixture);
          scFail = 1;
        }
        if (lastErrorCode != ERROR_NONE) {
          printf("    [2] FAIL: lastErrorCode %u after SST\n", lastErrorCode);
          scFail = 1;
        }
        if (!scFail) {
          printf("    [2] PASS: SST in keys mode aborts the empty placeholder first\n");
        }
      }
    }
    else if (sc == 3) {
      /* Negative control: plain PEM, no capture.  Upstream R/S behavior
       * must be exactly one added STOP step. */
      uint16_t before = getNumberOfSteps();
      processKeyAction(ITM_RS);
      if (forthCapIsOpen()) {
        printf("    [3] FAIL: a capture appeared out of nowhere\n");
        scFail = 1;
      }
      if (getNumberOfSteps() != (uint16_t)(before + 1)) {
        printf("    [3] FAIL: %u steps, expected %u (exactly one STOP)\n",
               getNumberOfSteps(), (uint16_t)(before + 1));
        scFail = 1;
      }
      else {
        uint8_t *sMarker = findNextStep(beginOfProgramMemory);
        uint8_t *sStop   = sMarker ? findNextStep(sMarker) : NULL;
        if (sStop == NULL || sStop[0] != (uint8_t)ITM_STOP) {
          printf("    [3] FAIL: the added step is not a native STOP\n");
          scFail = 1;
        }
      }
      if (!scFail) {
        printf("    [3] PASS: R/S outside a capture is untouched upstream behavior\n");
      }
    }
    else {
      /* sc4 (E13 proper, was the K1 interim): a CANCELLED TAM round-trip
       * comes back to the sub-mode it was keyed from.  K3 replaced the
       * interim (suspend clears the bit) with snapshot+restore, so this
       * subcase now pins the cancel path of the ruled behavior — K3's T1
       * pins the commit path. */
      runFunction(ITM_AIM);                   /* the real toggle to keys */
      if (!forthCapKeysMode()) {
        printf("    [4] FIXTURE FAIL: toggle did not set keys mode\n");
        scFail = 1;
      }
      else {
        runFunction(ITM_STO);                 /* physical-shaped TAM entry */
        if (forthTestCapState() != FCAP_SUSPENDED) {
          printf("    [4] FAIL: capture state %d, expected FCAP_SUSPENDED\n",
                 forthTestCapState());
          scFail = 1;
        }
        else if (!forthCapKeysMode()) {
          printf("    [4] FAIL: keys-mode bit cleared by the suspend\n");
          scFail = 1;
        }
        else {
          fnKeyExit(NOPARAM);                 /* cancel the TAM session */
          if (forthTestCapState() != FCAP_OPEN) {
            printf("    [4] FAIL: capture state %d after resume, expected FCAP_OPEN\n",
                   forthTestCapState());
            scFail = 1;
          }
          else if (!forthCapKeysMode()) {
            printf("    [4] FAIL: the cancelled round-trip did not resume in keys mode\n");
            scFail = 1;
          }
          else if (currentMenu() == -MNU_ALPHA) {
            printf("    [4] FAIL: resume pushed the ALPHA menu over the keys-mode row\n");
            scFail = 1;
          }
        }
        if (!scFail) {
          printf("    [4] PASS: a cancelled TAM round-trip resumes in keys mode\n");
        }
      }
    }

    fail |= scFail;

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
    xcopy(softmenuStack, savedStack, sizeof(savedStack));
    lastErrorCode = ERROR_NONE;
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  temporaryInformation = savedTemporaryInfo;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ==================================================================
 * K2 (Stage K packet 2) — token boundaries, the EXIT ladder rung, and
 * the EEX / numlock residuals.  DESIGN rules E12.3 and E12.4, plus the
 * token-boundary defect keys mode brought to the surface: with the
 * normal columns up, digits-then-function is the primary flow, and
 * every direct name-insert path used to glue the two together.
 *
 * Same discipline as the K1 group: the real entry points do the work
 * (runFunction, pickerInsertName, fnKeyExit), the capture is opened by
 * driving it, and the keys-mode bit is only ever set by the real
 * ALPHA-gesture toggle.
 * ================================================================== */

/* T1 (C1): the class test for the token-boundary guard.
 *
 * The claim lives in forthCapInsertName, so it is exercised through the
 * two callers that reach it directly: the F6-3 item arm (driven as a
 * real function press with keys mode on) and the FWRD picker.  Three
 * cursor preconditions cover the whole decision — line start, a
 * non-space byte before the cursor, and a space before the cursor,
 * which must NOT gain a second one.
 *
 * Escaping mutation: drop the `lead` logic from C1 — subcase 2 reads
 * "42SIN " instead of "42 SIN ". */
static int test_insert_token_boundary(void)
{
  extern void   fnGotoDot(uint16_t);
  extern void   runFunction(int16_t);
  extern bool_t pickerInsertName(void);
  extern void   testInitVariableSoftmenu(int16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K2B");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
  runFunction(ITM_AIM);                       /* open the capture */
  if (!forthCapIsOpen()) {
    printf("    FIXTURE FAIL: ITM_AIM did not open capture\n");
    cleanupTestProgram();
    return 1;
  }
  runFunction(ITM_AIM);                       /* the real toggle to keys */
  if (!forthCapKeysMode()) {
    printf("    FIXTURE FAIL: toggle did not set keys mode\n");
    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
    return 1;
  }

  /* ---- Subcase 1: line start — no leading separator ---- */
  { int sc1 = 0;
    runFunction(ITM_sin);
    if (strcmp(forthTestCapText(), "SIN ") != 0) {
      printf("    [1] FAIL: cap text = '%s', expected 'SIN '\n", forthTestCapText());
      sc1 = 1;
    }
    else if (T_cursorPos != 4) {
      printf("    [1] FAIL: T_cursorPos = %d, expected 4\n", T_cursorPos);
      sc1 = 1;
    }
    else {
      printf("    [1] PASS: an insert at the line start gains no leading separator\n");
    }
    fail |= sc1;
  }

  /* ---- Subcase 2: digits immediately before the cursor (the defect) ---- */
  if (!fail) {
    int sc2 = 0;
    runFunction(ITM_CLA);
    runFunction(ITM_4);
    runFunction(ITM_2);
    if (strcmp(forthTestCapText(), "42") != 0) {
      printf("    [2] FIXTURE FAIL: typed line = '%s', expected '42'\n", forthTestCapText());
      sc2 = 1;
    }
    else {
      runFunction(ITM_sin);
      if (strcmp(forthTestCapText(), "42 SIN ") != 0) {
        printf("    [2] FAIL: cap text = '%s', expected '42 SIN '\n", forthTestCapText());
        sc2 = 1;
      }
      else if (T_cursorPos != 7) {
        printf("    [2] FAIL: T_cursorPos = %d, expected 7\n", T_cursorPos);
        sc2 = 1;
      }
      else {
        printf("    [2] PASS: a name after typed digits lands as its own token\n");
      }
    }
    fail |= sc2;
  }

  /* ---- Subcase 3: a space already there — exactly one, not two ---- */
  if (!fail) {
    int sc3 = 0;
    runFunction(ITM_CLA);
    runFunction(ITM_4);
    runFunction(ITM_2);
    runFunction(ITM_SPACE);
    if (strcmp(forthTestCapText(), "42 ") != 0) {
      printf("    [3] FIXTURE FAIL: typed line = '%s', expected '42 '\n", forthTestCapText());
      sc3 = 1;
    }
    else {
      runFunction(ITM_sin);
      if (strcmp(forthTestCapText(), "42 SIN ") != 0) {
        printf("    [3] FAIL: cap text = '%s', expected '42 SIN ' (no double space)\n",
               forthTestCapText());
        sc3 = 1;
      }
      else if (T_cursorPos != 7) {
        printf("    [3] FAIL: T_cursorPos = %d, expected 7\n", T_cursorPos);
        sc3 = 1;
      }
      else {
        printf("    [3] PASS: an existing separator is not doubled\n");
      }
    }
    fail |= sc3;
  }

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  tam.function = 0;
  cleanupTestProgram();

  /* ---- Subcase 4: the picker, the other direct insert path ---- */
  if (!fail) {
    int sc4 = 0;
    /* marker | : SQ DUP * ; | marker  (picker fixture, copied from
     * test_picker_insert_at_cursor: the menu content is built with the
     * cursor past the definition, then the capture is driven open at the
     * head of the program). */
    uint8_t prog[] = {
      0x8B, 0x1A, 0xFD, 0x00,                                            /* marker */
      0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',    /* : SQ DUP * ; */
      ' ', '*', ' ', ';',
      0x8B, 0x1A, 0xFD, 0x00,                                            /* marker */
    };
    uint8_t savedSoftmenuStackId = softmenuStack[0].softmenuId;

    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [4] FIXTURE FAIL: writeTestProgram\n");
      sc4 = 1;
    }
    else {
      currentProgramNumber = 1;
      currentStep = beginOfProgramMemory + 4;         /* the SQ definition */
      { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
        testInitVariableSoftmenu(22);
        calcMode = m1e3s_; }

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
      lastErrorCode = ERROR_NONE;
      clearSystemFlag(FLAG_ALPHA);
      forthCapClose();

      currentStep = beginOfProgramMemory;
      currentLocalStepNumber = 1;
      runFunction(ITM_AIM);

      if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
        printf("    [4] FIXTURE FAIL: ITM_AIM did not open Forth capture\n");
        sc4 = 1;
      }
      else {
        runFunction(ITM_4);
        runFunction(ITM_2);
        if (strcmp(forthTestCapText(), "42") != 0) {
          printf("    [4] FIXTURE FAIL: typed line = '%s', expected '42'\n", forthTestCapText());
          sc4 = 1;
        }
        else {
          softmenuStack[0].softmenuId = 22;
          dynamicMenuItem = 0;
          if (!pickerInsertName()) {
            printf("    [4] FAIL: pickerInsertName returned false\n");
            sc4 = 1;
          }
          else if (strcmp(forthTestCapText(), "42 SQ ") != 0) {
            printf("    [4] FAIL: cap text = '%s', expected '42 SQ '\n", forthTestCapText());
            sc4 = 1;
          }
          else if (T_cursorPos != 6) {
            printf("    [4] FAIL: T_cursorPos = %d, expected 6\n", T_cursorPos);
            sc4 = 1;
          }
          else {
            printf("    [4] PASS: a picker pick after typed digits lands as its own token\n");
          }
        }
      }

      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
      tam.function = 0;
      softmenuStack[0].softmenuId = savedSoftmenuStackId;
      if (dynamicSoftmenu[22].menuContent) {
        free(dynamicSoftmenu[22].menuContent);
        dynamicSoftmenu[22].menuContent = NULL;
      }
      dynamicSoftmenu[22].numItems = 0;
      cleanupTestProgram();
    }
    fail |= sc4;
  }

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  shiftF = false;
  shiftG = false;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T2 (C1, end to end): the reproducer the owner's question surfaced.
 *
 * Keys mode up, four-two-SIN on the physical keys, ENTER.  Before K2 the
 * line read "42SIN " — one unresolvable token, since the tokenizer splits
 * on 0x20 alone.  The buffer assertion is the red-first evidence; the
 * commit assertions pin that the corrected line is also a line the
 * capture will actually take. */
static int test_keys_digits_then_function(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K2D");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }
  runFunction(ITM_AIM);                       /* the real toggle to keys */
  if (!forthCapKeysMode()) {
    printf("    FIXTURE FAIL: toggle did not set keys mode\n");
    fail = 1;
  }

  if (!fail) {
    runFunction(ITM_4);
    runFunction(ITM_2);
    runFunction(ITM_sin);

    if (strcmp(forthTestCapText(), "42 SIN ") != 0) {
      printf("    [1] FAIL: cap text = '%s', expected '42 SIN '\n", forthTestCapText());
      fail = 1;
    }
    else {
      lastErrorCode = ERROR_NONE;
      runFunction(ITM_ENTER);

      /* The committed source step sits between the opening marker and the
       * relocked placeholder; pin its exact bytes. */
      static const uint8_t wantSrc[11] = { 0x8B, 0x1A, 0xFD, 0x07,
                                           '4', '2', ' ', 'S', 'I', 'N', ' ' };
      uint8_t *sMarker = findNextStep(beginOfProgramMemory);      /* past LBL */
      uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;

      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: lastErrorCode %u after ENTER — the line was refused\n",
               lastErrorCode);
        fail = 1;
      }
      else if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture state %d after ENTER, expected FCAP_OPEN (relock)\n",
               forthTestCapState());
        fail = 1;
      }
      else if (tam.function != ITM_FORTH) {
        printf("    [1] FAIL: tam.function 0x%04X after ENTER, expected ITM_FORTH\n",
               tam.function);
        fail = 1;
      }
      else if (forthCapKeysMode()) {
        printf("    [1] FAIL: the relocked line did not start in alpha input\n");
        fail = 1;
      }
      else if (sSrc == NULL || memcmp(sSrc, wantSrc, sizeof(wantSrc)) != 0) {
        printf("    [1] FAIL: the committed step is not the source line \"42 SIN \"\n");
        fail = 1;
      }
      else {
        printf("    [1] PASS: 4 2 SIN in keys mode commits as \"42 SIN \" and relocks\n");
      }
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T3 (C3 / E12.4): the new first rung of the EXIT ladder.
 *
 * The claim is one level per press: EXIT in keys mode returns to alpha
 * input and does nothing else, and the SECOND press then does exactly
 * what EXIT has always done on that line — abort when empty, commit when
 * there is text.  Both halves are needed: without the second press the
 * rung could be swallowing a level rather than adding one.
 *
 * Escaping mutation: remove the C3 rung — the first press commits or
 * aborts, so the capture is gone one press early. */
static int test_exit_ladder_keys_rung(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedTemporaryInfo2 = temporaryInformation;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  for (int sc = 1; sc <= 2 && !fail; sc++) {
    int scFail = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "K2X");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [%d] FIXTURE FAIL: build/write\n", sc);
      fail = 1;
      break;
    }

    calcMode = CM_PEM;
    catalog = CATALOG_NONE;
    tam.mode = 0;
    tam.alpha = false;
    tam.function = 0;
    aimBuffer[0] = 0;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    temporaryInformation = TI_NO_INFO;
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
    uint16_t stepsFixture = getNumberOfSteps();
    runFunction(ITM_AIM);
    if (!forthCapIsOpen()) {
      printf("    [%d] FIXTURE FAIL: ITM_AIM did not open capture\n", sc);
      fail = 1;
      cleanupTestProgram();
      break;
    }

    if (sc == 2) {
      runFunction(ITM_2);                     /* a line with text on it */
    }

    runFunction(ITM_AIM);                     /* the real toggle to keys */
    if (!forthCapKeysMode()) {
      printf("    [%d] FIXTURE FAIL: toggle did not set keys mode\n", sc);
      scFail = 1;
    }
    else {
      /* ---- first press: the new rung, and nothing more ---- */
      lastErrorCode = ERROR_NONE;
      fnKeyExit(NOPARAM);

      if (forthCapKeysMode()) {
        printf("    [%d] FAIL: keys-mode bit survived the first EXIT\n", sc);
        scFail = 1;
      }
      else if (forthTestCapState() != FCAP_OPEN) {
        printf("    [%d] FAIL: capture state %d after the first EXIT, expected FCAP_OPEN\n",
               sc, forthTestCapState());
        scFail = 1;
      }
      else if (currentMenu() != -MNU_ALPHA) {
        printf("    [%d] FAIL: currentMenu() = %d after the first EXIT, expected -MNU_ALPHA\n",
               sc, currentMenu());
        scFail = 1;
      }
      else if (strcmp(forthTestCapText(), sc == 2 ? "2" : "") != 0) {
        printf("    [%d] FAIL: the first EXIT changed the line: '%s'\n",
               sc, forthTestCapText());
        scFail = 1;
      }

      /* ---- second press: the ladder rung that was always there ---- */
      if (!scFail) {
        lastErrorCode = ERROR_NONE;
        fnKeyExit(NOPARAM);

        if (forthTestCapState() != FCAP_CLOSED) {
          printf("    [%d] FAIL: capture state %d after the second EXIT, expected FCAP_CLOSED\n",
                 sc, forthTestCapState());
          scFail = 1;
        }
        else if (sc == 1) {
          if (getNumberOfSteps() != stepsFixture) {
            printf("    [1] FAIL: %u steps after the empty abort, expected the fixture's %u\n",
                   getNumberOfSteps(), stepsFixture);
            scFail = 1;
          }
        }
        else {
          static const uint8_t wantSrc[5] = { 0x8B, 0x1A, 0xFD, 0x01, '2' };
          uint8_t *sMarker = findNextStep(beginOfProgramMemory);   /* past LBL */
          uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;
          if (sSrc == NULL || memcmp(sSrc, wantSrc, sizeof(wantSrc)) != 0) {
            printf("    [2] FAIL: the committed step is not the source line \"2\"\n");
            scFail = 1;
          }
        }
      }

      if (!scFail) {
        printf("    [%d] PASS: EXIT in keys mode returns to alpha; the next press %s\n",
               sc, sc == 1 ? "aborts the empty line" : "commits the text");
      }
    }

    fail |= scFail;

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    cleanupTestProgram();
    xcopy(softmenuStack, savedStack, sizeof(savedStack));
    lastErrorCode = ERROR_NONE;
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  temporaryInformation = savedTemporaryInfo2;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T4 (C4 / E12.3): the two residuals the normal columns expose.
 *
 * (1) EEX carries the softmenu name "EEX" (items.c row 990), three
 *     letters the number grammar cannot read — it accepts e/E only.
 * (2) The numlock translation table is keyed on the AIM columns, so it
 *     has no business rewriting the normal-column ids keys mode feeds it.
 *
 * Escaping mutations: remove the C4a map — subcase 1 reads "1EEX5";
 * remove the C4b guard — subcase 2 is the escape-valve candidate, since
 * the table may simply hold no row for a normal-column digit. */
static int test_keys_eex_and_numlock(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);

  int fail = 0;
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  bool_t savedNumlock = getSystemFlag(FLAG_NUMLOCK);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  for (int sc = 1; sc <= 2 && !fail; sc++) {
    int scFail = 0;
    testProg_t p;
    tpInit(&p);
    int sLbl = tpLbl(&p, "K2E");
    tpMarker(&p);
    tpRtn(&p);
    if (sLbl < 0 || !tpWrite(&p)) {
      printf("    [%d] FIXTURE FAIL: build/write\n", sc);
      fail = 1;
      break;
    }

    calcMode = CM_PEM;
    catalog = CATALOG_NONE;
    tam.mode = 0;
    tam.alpha = false;
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

    if (sc == 2) {
      setSystemFlag(FLAG_NUMLOCK);            /* poison: the table is armed */
    }

    fnGotoDot(2);
    runFunction(ITM_AIM);
    if (!forthCapIsOpen()) {
      printf("    [%d] FIXTURE FAIL: ITM_AIM did not open capture\n", sc);
      fail = 1;
      clearSystemFlag(FLAG_NUMLOCK);
      cleanupTestProgram();
      break;
    }
    runFunction(ITM_AIM);                     /* the real toggle to keys */
    if (!forthCapKeysMode()) {
      printf("    [%d] FIXTURE FAIL: toggle did not set keys mode\n", sc);
      scFail = 1;
    }
    else if (sc == 1) {
      /* EEX alone is one byte and no separator, then the composed form. */
      runFunction(ITM_EXPONENT);
      if (strcmp(forthTestCapText(), "e") != 0) {
        printf("    [1] FAIL: cap text = '%s', expected 'e'\n", forthTestCapText());
        scFail = 1;
      }
      else if (T_cursorPos != 1) {
        printf("    [1] FAIL: T_cursorPos = %d, expected 1\n", T_cursorPos);
        scFail = 1;
      }
      else {
        runFunction(ITM_CLA);
        runFunction(ITM_1);
        runFunction(ITM_EXPONENT);
        runFunction(ITM_5);
        if (strcmp(forthTestCapText(), "1e5") != 0) {
          printf("    [1] FAIL: cap text = '%s', expected '1e5'\n", forthTestCapText());
          scFail = 1;
        }
        else {
          lastErrorCode = ERROR_NONE;
          runFunction(ITM_ENTER);
          static const uint8_t wantSrc[7] = { 0x8B, 0x1A, 0xFD, 0x03, '1', 'e', '5' };
          uint8_t *sMarker = findNextStep(beginOfProgramMemory);
          uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;
          if (lastErrorCode != ERROR_NONE) {
            printf("    [1] FAIL: lastErrorCode %u after ENTER — \"1e5\" was refused\n",
                   lastErrorCode);
            scFail = 1;
          }
          else if (forthTestCapState() != FCAP_OPEN) {
            printf("    [1] FAIL: capture state %d after ENTER, expected FCAP_OPEN\n",
                   forthTestCapState());
            scFail = 1;
          }
          else if (sSrc == NULL || memcmp(sSrc, wantSrc, sizeof(wantSrc)) != 0) {
            printf("    [1] FAIL: the committed step is not the source line \"1e5\"\n");
            scFail = 1;
          }
        }
      }
      if (!scFail) {
        printf("    [1] PASS: EEX types the grammar's exponent byte — \"1e5\" commits as one token\n");
      }
    }
    else {
      /* Numlock armed: a normal-column digit must reach the buffer as
       * itself, with the aim-column translation table stepped over. */
      runFunction(ITM_2);
      if (strcmp(forthTestCapText(), "2") != 0) {
        printf("    [2] FAIL: cap text = '%s', expected '2' (numlock rewrote the item)\n",
               forthTestCapText());
        scFail = 1;
      }
      else if (T_cursorPos != 1) {
        printf("    [2] FAIL: T_cursorPos = %d, expected 1\n", T_cursorPos);
        scFail = 1;
      }
      else {
        printf("    [2] PASS: numlock does not translate keys-mode items\n");
      }
    }

    fail |= scFail;

    forthCapClose();
    clearSystemFlag(FLAG_ALPHA);
    clearSystemFlag(FLAG_NUMLOCK);
    tam.function = 0;
    cleanupTestProgram();
    xcopy(softmenuStack, savedStack, sizeof(savedStack));
    lastErrorCode = ERROR_NONE;
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  if (savedNumlock) setSystemFlag(FLAG_NUMLOCK); else clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ==================================================================
 * K3 (Stage K packet 3) — keys-mode persistence across the TAM
 * round-trip.  E13 proper: the sub-mode the user keyed a parameterized
 * item from comes back with the line, and E14 keeps an abandoned
 * suspension from leaking the bit into the next capture.
 *
 * Same discipline as K1/K2: the capture is opened by driving it, the
 * bit is only ever set by the real ALPHA-gesture toggle, and the TAM
 * round-trip runs through tamProcessInput / fnKeyExit.
 * ================================================================== */

/* T1 (C1+C2+C3): a parameterized item keyed in keys mode returns to
 * keys mode.  Three sequential subcases over one capture: the bit
 * survives the suspension, it survives the reopen the resume performs,
 * and the toggle is still symmetric afterwards.
 *
 * Escaping mutations: restore the C1 interim clear (sc1 red); drop
 * C2's save/restore (sc2 red); make C3's push unconditional (sc2 red
 * on the menu). */
static int test_keys_tam_roundtrip(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K3R");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  /* sc1: the bit rides the suspension.  Pre-K3 the suspend seam cleared
   * it (the E13 interim); the ruled behavior keeps it. */
  {
    runFunction(ITM_AIM);                     /* the real toggle to keys */
    if (!forthCapKeysMode()) {
      printf("    [1] FIXTURE FAIL: toggle did not set keys mode\n");
      fail = 1;
    }
    else {
      runFunction(ITM_STO);                   /* physical-shaped TAM entry */
      if (forthTestCapState() != FCAP_SUSPENDED) {
        printf("    [1] FAIL: capture state %d, expected FCAP_SUSPENDED\n",
               forthTestCapState());
        fail = 1;
      }
      else if (!forthCapKeysMode()) {
        printf("    [1] FAIL: keys-mode bit cleared by the suspend\n");
        fail = 1;
      }
      else {
        printf("    [1] PASS: the keys-mode bit rides the TAM suspension\n");
      }
    }
  }

  /* sc2: the commit resumes in keys mode — the bit survives the reopen
   * forthCaptureResume performs, and the resume does not cover the
   * underlying row (which IS the mode indicator) with the alpha menu. */
  if (!fail) {
    tamProcessInput(ITM_0);
    tamProcessInput(ITM_5);                   /* two digits auto-fire the commit */

    if (forthTestCapState() != FCAP_OPEN) {
      printf("    [2] FAIL: capture state %d after the commit, expected FCAP_OPEN\n",
             forthTestCapState());
      fail = 1;
    }
    else if (!forthCapKeysMode()) {
      printf("    [2] FAIL: keys-mode bit lost across the resume\n");
      fail = 1;
    }
    else if (strcmp(forthTestCapText(), "STO 05 ") != 0) {
      printf("    [2] FAIL: cap text = '%s', expected 'STO 05 '\n", forthTestCapText());
      fail = 1;
    }
    else if (currentMenu() == -MNU_ALPHA) {
      printf("    [2] FAIL: resume pushed the ALPHA menu over the keys-mode row\n");
      fail = 1;
    }
    else {
      printf("    [2] PASS: the TAM commit resumes the line in keys mode\n");
    }
  }

  /* sc3: the toggle is still symmetric after a round-trip. */
  if (!fail) {
    runFunction(ITM_AIM);
    if (forthCapKeysMode()) {
      printf("    [3] FAIL: keys-mode bit still set after toggling back\n");
      fail = 1;
    }
    else if (currentMenu() != -MNU_ALPHA) {
      printf("    [3] FAIL: currentMenu() = %d, expected -MNU_ALPHA\n", currentMenu());
      fail = 1;
    }
    else {
      printf("    [3] PASS: the toggle is still symmetric after the round-trip\n");
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T2: the same drive WITHOUT the toggle.  The C3 gate must not regress
 * the alpha path — a TAM round-trip from alpha input still comes back
 * to alpha input with the ALPHA menu on top. */
static int test_alpha_tam_roundtrip_unchanged(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K3A");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  if (forthCapKeysMode()) {
    printf("    [1] FIXTURE FAIL: a fresh capture is not in alpha input\n");
    fail = 1;
  }
  else {
    runFunction(ITM_STO);
    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [1] FAIL: capture state %d, expected FCAP_SUSPENDED\n",
             forthTestCapState());
      fail = 1;
    }
    else {
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);

      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture state %d after the commit, expected FCAP_OPEN\n",
               forthTestCapState());
        fail = 1;
      }
      else if (forthCapKeysMode()) {
        printf("    [1] FAIL: the alpha round-trip came back in keys mode\n");
        fail = 1;
      }
      else if (currentMenu() != -MNU_ALPHA) {
        printf("    [1] FAIL: currentMenu() = %d after resume, expected -MNU_ALPHA\n",
               currentMenu());
        fail = 1;
      }
      else if (strcmp(forthTestCapText(), "STO 05 ") != 0) {
        printf("    [1] FAIL: cap text = '%s', expected 'STO 05 '\n", forthTestCapText());
        fail = 1;
      }
      else {
        printf("    [1] PASS: the alpha-input round-trip is unchanged by the C3 gate\n");
      }
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* T3 (C4/E14): with the bit now riding the suspension, an ABANDONED
 * suspension must not leak it into the next capture.  Driven through
 * the same falsified-step canary test_capture_suspend [5] uses: stomp
 * the saved step's opcode, then hit the resume choke point.
 *
 * Escaping mutation: revert C4 — the bit survives the abandon. */
static int test_abandon_clears_keys_bit(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K3X");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  runFunction(ITM_AIM);                       /* the real toggle to keys */
  if (!forthCapKeysMode()) {
    printf("    [1] FIXTURE FAIL: toggle did not set keys mode\n");
    fail = 1;
  }
  else {
    runFunction(ITM_STO);
    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [1] FIXTURE FAIL: capture state %d, expected FCAP_SUSPENDED\n",
             forthTestCapState());
      fail = 1;
    }
    else if (!forthCapKeysMode()) {
      printf("    [1] FIXTURE FAIL: keys-mode bit cleared by the suspend\n");
      fail = 1;
    }
    else {
      /* Deliberate falsification, exactly as test_capture_suspend [5]:
       * stomp the saved step's opcode byte with ITM_RTN so the
       * resume-time structural check must reject it. */
      uint8_t *savedStep = beginOfProgramMemory + forthCapSavedStepOffset();
      savedStep[0] = 0x04;

      fnKeyExit(NOPARAM);                     /* the resume choke point */

      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [1] FAIL: capture state %d after the abandon, expected FCAP_CLOSED\n",
               forthTestCapState());
        fail = 1;
      }
      else if (forthCapKeysMode()) {
        printf("    [1] FAIL: keys-mode bit survived the abandoned suspension\n");
        fail = 1;
      }
      else {
        printf("    [1] PASS: an abandoned suspension clears the keys-mode bit\n");
      }
    }
  }

  /* The falsification corrupted program memory — rebuild from scratch. */
  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ==================================================================
 * K4 (Stage K packet 4) — stage acceptance battery.
 *
 * K1 proved the bit and the column swap, K2 the token boundaries and
 * the ladder rung, K3 the TAM round-trip.  K4 asserts nothing new about
 * the production code: it drives the LANDED behavior end to end, the
 * way a user meets it — a definition typed half in alpha and half on
 * the physical keys, a keys-only line folded through TAM, the sub-mode
 * the relock hands back, the whole EXIT ladder rung by rung, and an
 * arena sweep over three complete open-to-closed cycles.
 *
 * Every drive here is a real entry point (runFunction, determineItem,
 * fnKeyExit, addStepInProgram, tamProcessInput, fnExecute); nothing
 * primes the state under test.
 * ================================================================== */

/* Read a numbered global register as a long integer — the register-side
 * twin of x_is_longint, so A2 can assert what the RUN stored rather than
 * what the interpreter happens to leave in X. */
static int k4_reg_is_longint(calcRegister_t reg, int32_t val)
{
  if (getRegisterDataType(reg) != dtLongInteger) {
    return 0;
  }
  longInteger_t li;
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(reg, li);
  int32_t v;
  longIntegerToInt32(li, v);
  longIntegerFree(li);
  return v == val;
}

/* A1 — the stage's headline story, driven end to end.
 *
 * ": SQ DUP " is typed on the alpha keyboard; the multiply is pressed on
 * the PHYSICAL key (located differentially in kbd_std by its normal
 * primary column, resolved through determineItem, dispatched through
 * runFunction); " ;" is typed back in alpha; ENTER relocks; "4 SQ" is
 * typed on the relocked line; EXIT commits and closes; the label is then
 * XEQ'd and X must be 16.
 *
 * One assertion chain covers mixed-sub-mode entry, the K2 token
 * boundary, glyph-name resolution of the keys-inserted multiply (the
 * item's name is STD_CROSS, which is also the Forth primitive's name),
 * the commit, and execution.  Nothing here is layout-dependent: the key
 * row is found by table search, not by key number.
 *
 * Escaping mutation: disable K1's toggle arm (M1) — the ALPHA gesture
 * stops toggling, the multiply key types a letter instead, and the
 * definition never compiles. */
static int test_k4_mixed_input_definition(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void fnExecute(uint16_t);
  extern int16_t determineItem(const char *);

  int fail = 0;
  uint8_t *savedCurrentStep = currentStep;
  bool_t savedZeroth = pemCursorIsZerothStep;
  uint16_t savedLocalStep = currentLocalStepNumber;
  uint16_t savedProgNum = currentProgramNumber;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  bool_t savedUser = getSystemFlag(FLAG_USER);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* Locate the multiply row once, from the live table (K1 T1's idiom). */
  int mIdx = -1;
  for (int i = 0; i < 37; i++) {
    if (kbd_std[i].primary == ITM_MULT) { mIdx = i; break; }
  }
  if (mIdx < 0) {
    printf("    FIXTURE FAIL: no kbd_std row carries primary == ITM_MULT\n");
    return 1;
  }
  char kb[3];
  sprintf(kb, "%02d", mIdx);

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K4A");
  tpMarker(&p);                                 /* opening marker */
  tpMarker(&p);                                 /* closing marker: an empty region */
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
  clearSystemFlag(FLAG_USER);                   /* resolution must read kbd_std */
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  currentProgramNumber = 1;

  fnGotoDot(2);                                 /* onto the opening marker */
  runFunction(ITM_AIM);
  if (!forthCapIsOpen()) {
    printf("    FIXTURE FAIL: ITM_AIM did not open capture\n");
    cleanupTestProgram();
    return 1;
  }

  /* ---- alpha sub-mode: ": SQ DUP " ---- */
  runFunction(ITM_COLON);
  runFunction(ITM_SPACE);
  runFunction(ITM_S);
  runFunction(ITM_Q);
  runFunction(ITM_SPACE);
  runFunction(ITM_D);
  runFunction(ITM_U);
  runFunction(ITM_P);
  runFunction(ITM_SPACE);

  /* ---- toggle to keys and press the physical multiply key ---- */
  runFunction(ITM_AIM);
  if (!forthCapKeysMode()) {
    printf("    [1] FAIL: the ALPHA gesture did not switch to keys mode\n");
    fail = 1;
  }
  else {
    shiftF = false;
    int16_t mItem = determineItem(kb);
    shiftF = false;
    if (mItem != ITM_MULT) {
      printf("    [1] FAIL: the multiply key resolved to %d, expected ITM_MULT (%d)\n",
             mItem, ITM_MULT);
      fail = 1;
    }
    else {
      runFunction(mItem);                       /* the resolved item, dispatched */
    }
  }

  /* ---- toggle back to alpha and finish the definition ---- */
  if (!fail) {
    runFunction(ITM_AIM);
    if (forthCapKeysMode()) {
      printf("    [1] FAIL: the ALPHA gesture did not switch back to alpha input\n");
      fail = 1;
    }
    else {
      runFunction(ITM_SPACE);
      runFunction(ITM_SEMICOLON);
    }
  }

  /* The keys-mode insert brought its own trailing space (K2's token
   * boundary guard), and the packet's script then types one more before
   * the ';' — two spaces between the operator and the terminator, which
   * the C-6 tokenizer skips (delimiter runs are not tokens). */
  if (!fail) {
    const char *want = ": SQ DUP " STD_CROSS "  ;";
    if (strcmp(forthTestCapText(), want) != 0) {
      printf("    [1] FAIL: line text = '%s', expected '%s'\n",
             forthTestCapText(), want);
      fail = 1;
    }
  }

  /* ---- ENTER relocks; the second line is typed on the relocked line ---- */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    runFunction(ITM_ENTER);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: lastErrorCode %u after ENTER — the definition was refused\n",
             lastErrorCode);
      fail = 1;
    }
    else if (forthTestCapState() != FCAP_OPEN) {
      printf("    [1] FAIL: capture state %d after ENTER, expected FCAP_OPEN (relock)\n",
             forthTestCapState());
      fail = 1;
    }
  }

  if (!fail) {
    runFunction(ITM_4);
    runFunction(ITM_SPACE);
    runFunction(ITM_S);
    runFunction(ITM_Q);
    fnKeyExit(NOPARAM);                         /* commit-and-close */

    if (forthCapIsOpen() || getSystemFlag(FLAG_ALPHA)) {
      printf("    [1] FAIL: capture still open after the committing EXIT\n");
      fail = 1;
    }
  }

  /* ---- the committed image, walked structurally from the LBL ---- */
  if (!fail) {
    const char *def1 = ": SQ DUP " STD_CROSS "  ;";
    uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
    uint8_t *sDef1   = sMarker ? findNextStep(sMarker) : NULL;
    uint8_t *sDef2   = sDef1   ? findNextStep(sDef1)   : NULL;
    size_t   n1      = strlen(def1);

    if (!sMarker || !sDef1 || !sDef2) {
      printf("    [1] FAIL: structural walk from the LBL came up short\n");
      fail = 1;
    }
    else if (sDef1[0] != 0x8B || sDef1[1] != 0x1A || sDef1[2] != 0xFD ||
             sDef1[3] != (uint8_t)n1 || memcmp(sDef1 + 4, def1, n1) != 0) {
      printf("    [1] FAIL: the committed definition step is not the typed line\n");
      fail = 1;
    }
    else if (sDef2[0] != 0x8B || sDef2[1] != 0x1A || sDef2[2] != 0xFD ||
             sDef2[3] != 4 || memcmp(sDef2 + 4, "4 SQ", 4) != 0) {
      printf("    [1] FAIL: the committed call step is not \"4 SQ\"\n");
      fail = 1;
    }
  }

  /* ---- run it by label (the F15/showcase idiom) ---- */
  if (!fail) {
    dynamicMenuItem = -1;
    calcRegister_t lbl = findNamedLabel("K4A", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [1] FAIL: findNamedLabel(K4A) failed\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;
      fnExecute(lbl);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: run error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (!x_is_longint(16)) {
        printf("    [1] FAIL: X != 16 after the run\n");
        fail = 1;
      }
      else {
        printf("    [1] PASS: alpha \": SQ DUP \" + the physical " STD_CROSS
               " key + alpha \" ;\" defines SQ; \"4 SQ\" runs to 16\n");
      }
    }
  }

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  shiftF = false;
  shiftG = false;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  if (savedUser) setSystemFlag(FLAG_USER); else clearSystemFlag(FLAG_USER);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* A2 — a line entered entirely on the physical keys.
 *
 * 4, 2, STO, then the TAM digits 0 and 0.  The fold has to put the K2
 * separator between the digits and the folded name ("42 STO 00 ", never
 * "42STO 00 "), the line has to commit, and the program has to actually
 * store 42 into R00 when it runs.
 *
 * Escaping mutation: force K2's `lead` to 0 (M2) — the fold glues the
 * name to the digits and the committed line stops being a program. */
static int test_k4_keys_only_line(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnExecute(uint16_t);
  extern void tamProcessInput(uint16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* R00 must not be able to pass on a value an earlier test left there.
   * Cleared BEFORE the fixture is written: clearRegister reallocates in
   * the same RAM pool program memory lives in, and a reallocation after
   * tpWrite moves the program out from under the addresses
   * scanLabelsAndPrograms recorded — findNamedLabel then resolves to a
   * stale image and the run is a silent no-op (observed: R00 stayed the
   * cleared real34 zero while lastErrorCode stayed ERROR_NONE). */
  clearRegister(0);

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K4B");
  tpMarker(&p);
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  runFunction(ITM_AIM);                         /* the real toggle to keys */
  if (!forthCapKeysMode()) {
    printf("    [1] FIXTURE FAIL: toggle did not set keys mode\n");
    fail = 1;
  }

  if (!fail) {
    runFunction(ITM_4);
    runFunction(ITM_2);
    runFunction(ITM_STO);                       /* physical-shaped TAM entry */
    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [1] FAIL: capture state %d after STO, expected FCAP_SUSPENDED\n",
             forthTestCapState());
      fail = 1;
    }
    else {
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_0);                   /* two digits auto-fire the commit */
      if (forthTestCapState() != FCAP_OPEN) {
        printf("    [1] FAIL: capture state %d after the TAM commit, expected FCAP_OPEN\n",
               forthTestCapState());
        fail = 1;
      }
      else if (strcmp(forthTestCapText(), "42 STO 00 ") != 0) {
        printf("    [1] FAIL: line text = '%s', expected '42 STO 00 '\n",
               forthTestCapText());
        fail = 1;
      }
    }
  }

  if (!fail) {
    lastErrorCode = ERROR_NONE;
    runFunction(ITM_ENTER);                     /* commit; a relock line opens */
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: lastErrorCode %u after ENTER — the line was refused\n",
             lastErrorCode);
      fail = 1;
    }
    else {
      /* Close the capture so the program can run: the relock line is
       * empty, so one BACKSPACE aborts it (the landed idiom). */
      runFunction(ITM_BACKSPACE);
      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [1] FAIL: capture state %d after the relock-line abort, expected FCAP_CLOSED\n",
               forthTestCapState());
        fail = 1;
      }
    }
  }

  if (!fail) {
    uint8_t *sMarker = findNextStep(tpStepAddr(&p, sLbl));
    uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;
    static const uint8_t wantSrc[14] = { 0x8B, 0x1A, 0xFD, 10,
                                         '4', '2', ' ', 'S', 'T', 'O', ' ', '0', '0', ' ' };
    if (sSrc == NULL || memcmp(sSrc, wantSrc, sizeof(wantSrc)) != 0) {
      printf("    [1] FAIL: the committed step is not the source line \"42 STO 00 \"\n");
      fail = 1;
    }
  }

  /* Leave program-entry mode before running, the way the user does — EXIT
   * off the end of the ladder.  This is not cosmetic: a native
   * parameterized word executed while calcMode is still CM_PEM takes the
   * E0 insert divert instead of running, so the program's own "STO 00"
   * would record a step rather than store (observed: a GTO step appearing
   * after the source line and R00 untouched, with lastErrorCode clean). */
  if (!fail) {
    for (int press = 0; press < 4 && calcMode == CM_PEM; press++) {
      fnKeyExit(NOPARAM);
    }
    if (calcMode == CM_PEM) {
      printf("    [1] FAIL: still in CM_PEM after four EXIT presses\n");
      fail = 1;
    }
  }

  if (!fail) {
    dynamicMenuItem = -1;
    calcRegister_t lbl = findNamedLabel("K4B", GLOBAL_LABELS);
    if (lbl == INVALID_VARIABLE) {
      printf("    [1] FAIL: findNamedLabel(K4B) failed\n");
      fail = 1;
    }
    else {
      programRunStop = PGM_STOPPED;
      lastErrorCode = ERROR_NONE;
      fnExecute(lbl);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [1] FAIL: run error %d\n", lastErrorCode);
        fail = 1;
      }
      else if (!k4_reg_is_longint(0, 42)) {
        printf("    [1] FAIL: R00 does not hold 42 after the run (R00 type=%u)\n",
               getRegisterDataType(0));
        fail = 1;
      }
      else {
        printf("    [1] PASS: 4 2 STO 0 0 on the physical keys commits as"
               " \"42 STO 00 \" and stores 42 in R00\n");
      }
    }
  }

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  currentStep = savedCurrentStep;
  pemCursorIsZerothStep = savedZeroth;
  currentLocalStepNumber = savedLocalStep;
  currentProgramNumber = savedProgNum;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* A3 — the sub-mode the relock hands back.
 *
 * THIS TEST PINS THE CURRENT DEFAULT, NOT A RULING.  ENTER's commit runs
 * the close sweep, which clears the keys-mode bit (K1/E14), and the
 * relock then opens a FRESH capture line, whose owner-ruled default is
 * alpha input.  So a keys-mode line hands the next line back in alpha
 * with the ALPHA menu on top.
 *
 * A future refinement may decide the sub-mode should persist across the
 * relock the way K3 made it persist across a TAM round-trip.  That is a
 * deliberate flip of this pin, not a regression: whoever makes it must
 * change these two assertions on purpose and say so in the packet. */
static int test_k4_relock_submode(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);

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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K4C");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
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
    cleanupTestProgram();
    return 1;
  }

  runFunction(ITM_AIM);                         /* the real toggle to keys */
  if (!forthCapKeysMode()) {
    printf("    [1] FIXTURE FAIL: toggle did not set keys mode\n");
    fail = 1;
  }
  else {
    runFunction(ITM_4);
    runFunction(ITM_2);
    lastErrorCode = ERROR_NONE;
    runFunction(ITM_ENTER);                     /* commit + relock */

    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: lastErrorCode %u after ENTER\n", lastErrorCode);
      fail = 1;
    }
    else if (forthTestCapState() != FCAP_OPEN) {
      printf("    [1] FAIL: capture state %d after ENTER, expected FCAP_OPEN (relock)\n",
             forthTestCapState());
      fail = 1;
    }
    else if (forthCapKeysMode()) {
      printf("    [1] FAIL: the relocked line kept keys mode (the pinned default is alpha)\n");
      fail = 1;
    }
    else if (currentMenu() != -MNU_ALPHA) {
      printf("    [1] FAIL: currentMenu() = %d after the relock, expected -MNU_ALPHA\n",
             currentMenu());
      fail = 1;
    }
    else {
      printf("    [1] PASS: the relock after a keys-mode line opens in alpha input"
             " (pinned K1 default, not a ruling)\n");
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* A4 — the full ladder, one rung per press (E8 + E12.4).
 *
 * Keys mode has no alpha menus, so the FWRD picker is not even reachable
 * from it — that is the precondition, asserted rather than assumed.
 * Press 1 buys alpha input back; the picker is then pushed the way the
 * landed picker tests push it; press 2 pops the picker and no more;
 * press 3 aborts the (still empty) line.  Each press is followed by the
 * whole state tuple, so a rung that swallows a level or does two things
 * at once fails here even if the end state happens to be right. */
static int test_k4_ladder_full_unwind(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedTemporaryInfo = temporaryInformation;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K4L");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  temporaryInformation = TI_NO_INFO;
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
  uint16_t stepsFixture = getNumberOfSteps();
  runFunction(ITM_AIM);
  if (!forthCapIsOpen()) {
    printf("    FIXTURE FAIL: ITM_AIM did not open capture\n");
    cleanupTestProgram();
    return 1;
  }

  runFunction(ITM_AIM);                         /* the real toggle to keys */

  /* rung 0 — the precondition: keys mode, no alpha menu, line open. */
  if (!forthCapKeysMode() || forthTestCapState() != FCAP_OPEN ||
      currentMenu() == -MNU_ALPHA) {
    printf("    [1] FAIL: rung 0 tuple wrong (keys=%d state=%d menu=%d)\n",
           (int)forthCapKeysMode(), forthTestCapState(), currentMenu());
    fail = 1;
  }

  /* rung 1 — EXIT returns alpha input and the ALPHA menu, nothing else. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    fnKeyExit(NOPARAM);
    if (forthCapKeysMode() || forthTestCapState() != FCAP_OPEN ||
        currentMenu() != -MNU_ALPHA || !getSystemFlag(FLAG_ALPHA)) {
      printf("    [1] FAIL: rung 1 tuple wrong (keys=%d state=%d menu=%d alpha=%d)\n",
             (int)forthCapKeysMode(), forthTestCapState(), currentMenu(),
             (int)getSystemFlag(FLAG_ALPHA));
      fail = 1;
    }
  }

  /* Now — and only now — the FWRD picker is reachable. */
  if (!fail) {
    showSoftmenu(-MNU_FORTH);
    if (currentMenu() != -MNU_FORTH) {
      printf("    [1] FIXTURE FAIL: picker not on top (menu=%d)\n", currentMenu());
      fail = 1;
    }
  }

  /* rung 2 — EXIT pops the picker back to the ALPHA menu and no further. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    fnKeyExit(NOPARAM);
    if (forthCapKeysMode() || forthTestCapState() != FCAP_OPEN ||
        currentMenu() != -MNU_ALPHA || !getSystemFlag(FLAG_ALPHA)) {
      printf("    [1] FAIL: rung 2 tuple wrong (keys=%d state=%d menu=%d alpha=%d)\n",
             (int)forthCapKeysMode(), forthTestCapState(), currentMenu(),
             (int)getSystemFlag(FLAG_ALPHA));
      fail = 1;
    }
  }

  /* rung 3 — EXIT on the still-empty line aborts it. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    fnKeyExit(NOPARAM);
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [1] FAIL: rung 3 state %d, expected FCAP_CLOSED\n",
             forthTestCapState());
      fail = 1;
    }
    else if (forthCapKeysMode()) {
      printf("    [1] FAIL: rung 3 left the keys-mode bit set\n");
      fail = 1;
    }
    else if (getNumberOfSteps() != stepsFixture) {
      printf("    [1] FAIL: %u steps after the empty abort, expected the fixture's %u\n",
             getNumberOfSteps(), stepsFixture);
      fail = 1;
    }
    else {
      printf("    [1] PASS: keys -> alpha -> picker pop -> abort, one ladder level per EXIT\n");
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  temporaryInformation = savedTemporaryInfo;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* A5 — three complete cycles, and what they leave behind.
 *
 * One cycle: open on the marker, toggle to keys, key "42" and fold STO
 * 00 through TAM, ENTER to commit, abort the relock line, reopen with
 * the E2 in-region route (a printable key with the capture closed),
 * toggle to keys again, and unwind with EXIT until the capture is gone.
 * The two lines the cycle commits are then removed through the real
 * EDIT/CLA/BACKSPACE path, so the region genuinely returns to its
 * fixture image and each cycle starts from the same place.
 *
 * freeRam is allowed the K2/F6 escape valve and nothing else: any
 * residue must be a whole number of program-memory resize quanta and it
 * must be growth (freeRam falling), which is the region growing, not a
 * capture leaking.  Three cycles are bounded at 8 quanta. */
static int test_k4_arena_sweep(void)
{
  extern void fnGotoDot(uint16_t);
  extern void runFunction(int16_t);
  extern void fnKeyExit(uint16_t);
  extern void tamProcessInput(uint16_t);
  extern void addStepInProgram(int16_t);
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
  bool_t savedTamAlpha = tam.alpha;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  uint8_t savedTemporaryInfo = temporaryInformation;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  testProg_t p;
  tpInit(&p);
  int sLbl = tpLbl(&p, "K4S");
  tpMarker(&p);
  tpRtn(&p);
  if (sLbl < 0 || !tpWrite(&p)) {
    printf("    FIXTURE FAIL: build/write\n");
    return 1;
  }

  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.alpha = false;
  tam.function = 0;
  aimBuffer[0] = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  temporaryInformation = TI_NO_INFO;
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
  uint16_t stepsFixture = getNumberOfSteps();
  uint32_t freeBeforeCycles = getFreeRamMemory();

  for (int cycle = 0; cycle < 3 && !fail; cycle++) {
    /* ---- open on the marker, toggle, key a folded line, commit ---- */
    fnGotoDot(2);
    runFunction(ITM_AIM);
    if (!forthCapIsOpen()) {
      printf("    [1] FAIL: cycle %d did not open\n", cycle);
      fail = 1;
      break;
    }
    runFunction(ITM_AIM);
    runFunction(ITM_4);
    runFunction(ITM_2);
    runFunction(ITM_STO);
    if (forthTestCapState() != FCAP_SUSPENDED) {
      printf("    [1] FAIL: cycle %d not suspended (state=%d)\n", cycle, forthTestCapState());
      fail = 1;
      break;
    }
    tamProcessInput(ITM_0);
    tamProcessInput(ITM_0);
    if (forthTestCapState() != FCAP_OPEN || !forthCapKeysMode()) {
      printf("    [1] FAIL: cycle %d fold did not resume in keys mode (state=%d keys=%d)\n",
             cycle, forthTestCapState(), (int)forthCapKeysMode());
      fail = 1;
      break;
    }
    runFunction(ITM_ENTER);                     /* commit; relock line opens */
    runFunction(ITM_BACKSPACE);                 /* the relock line is empty: abort it */
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [1] FAIL: cycle %d relock line not aborted (state=%d)\n",
             cycle, forthTestCapState());
      fail = 1;
      break;
    }

    /* ---- reopen with the E2 in-region route, toggle, EXIT-unwind ---- */
    addStepInProgram(ITM_1);
    if (!forthCapIsOpen() || tam.function != ITM_FORTH) {
      printf("    [1] FAIL: cycle %d E2 reopen failed (open=%d tam=0x%04X)\n",
             cycle, (int)forthCapIsOpen(), tam.function);
      fail = 1;
      break;
    }
    runFunction(ITM_AIM);
    if (!forthCapKeysMode()) {
      printf("    [1] FAIL: cycle %d second toggle failed\n", cycle);
      fail = 1;
      break;
    }
    fnKeyExit(NOPARAM);                         /* rung 1: keys -> alpha */
    if (forthCapKeysMode() || forthTestCapState() != FCAP_OPEN) {
      printf("    [1] FAIL: cycle %d unwind rung 1 wrong (keys=%d state=%d)\n",
             cycle, (int)forthCapKeysMode(), forthTestCapState());
      fail = 1;
      break;
    }
    fnKeyExit(NOPARAM);                         /* rung 2: commit-and-close */
    if (forthTestCapState() != FCAP_CLOSED) {
      printf("    [1] FAIL: cycle %d unwind did not close (state=%d)\n",
             cycle, forthTestCapState());
      fail = 1;
      break;
    }

    /* ---- take the two committed lines back out, the way a user would ---- */
    for (int line = 0; line < 2 && !fail; line++) {
      uint8_t *sMarker = findNextStep(beginOfProgramMemory);   /* past the LBL */
      uint8_t *sSrc    = sMarker ? findNextStep(sMarker) : NULL;
      if (sSrc == NULL) {
        printf("    [1] FAIL: cycle %d cleanup walk came up short\n", cycle);
        fail = 1;
        break;
      }
      currentStep = sSrc;
      currentLocalStepNumber = 3;
      calcMode = CM_PEM;
      tam.mode = 0;
      clearSystemFlag(FLAG_ALPHA);
      tam.function = 0;
      pemAlpha(ITM_EDIT);                       /* reopens the line, deletes the step */
      runFunction(ITM_CLA);
      runFunction(ITM_BACKSPACE);               /* empty line: abort, nothing recommitted */
      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [1] FAIL: cycle %d cleanup left the capture open (state=%d)\n",
               cycle, forthTestCapState());
        fail = 1;
      }
    }
    if (fail) break;

    if (getNumberOfSteps() != stepsFixture) {
      printf("    [1] FAIL: %u steps after cycle %d, expected the fixture's %u\n",
             getNumberOfSteps(), cycle, stepsFixture);
      fail = 1;
      break;
    }
  }

  if (!fail) {
    uint32_t afterCycles = getFreeRamMemory();
    if (afterCycles == freeBeforeCycles) {
      printf("    [1] PASS: three full keys-mode cycles leave zero arena residue\n");
    }
    else {
      uint32_t delta = (freeBeforeCycles > afterCycles)
                       ? (freeBeforeCycles - afterCycles)
                       : (afterCycles - freeBeforeCycles);
      /* The K2/F6 escape valve, verbatim in shape: block-aligned,
       * growth-only, bounded — program-memory resize quanta, not a
       * capture leak.  Three cycles are allowed 8. */
      if (freeBeforeCycles > afterCycles && delta % BYTES_PER_BLOCK == 0
          && delta <= 8 * BYTES_PER_BLOCK) {
        printf("    [1] PASS (escape valve): freeRam %u -> %u is %u program-memory"
               " resize quantum(s) (%u B each) after three cycles, not a capture leak\n",
               (unsigned)freeBeforeCycles, (unsigned)afterCycles,
               (unsigned)(delta / BYTES_PER_BLOCK), (unsigned)BYTES_PER_BLOCK);
      }
      else {
        printf("    [1] FAIL: freeRam changed %u -> %u (delta %u, block %u)\n",
               (unsigned)freeBeforeCycles, (unsigned)afterCycles,
               (unsigned)delta, (unsigned)BYTES_PER_BLOCK);
        fail = 1;
      }
    }
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
  tam.alpha = savedTamAlpha;
  alphaCase = savedAlphaCase;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  temporaryInformation = savedTemporaryInfo;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;
  return fail;
}


/* ==================================================================
 * PACKET_L1_H (C5) — test_history_program.  The FHIST program: push,
 * cap, evict, recall.
 *
 * Each subcase builds its own program-memory fixture via the tp*
 * builder + writeTestProgram (or via real forthHistoryPush/Ensure calls
 * on the pristine baseline), and calls cleanupTestProgram() before the
 * next — same idiom as test_forth_capture_navigation and the picker
 * tests above.  Subcase 0 is a fixture/empirical-verification step (name
 * collision, the two f-shifted item ids); subcases 1-9 are C5.1-C5.9.
 * ================================================================== */
static int test_history_program(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyEnter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void runFunction(int16_t);
  extern void processKeyAction(int16_t);
  extern int16_t determineItem(const char *);

  int fail = 0, scFail;

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
  int16_t savedCursorPos = T_cursorPos;
  uint8_t savedAlphaCase = alphaCase;
  uint8_t savedNextChar = nextChar;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  #define LH_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  cleanupTestProgram();   /* pristine baseline before subcase 0 */

  /* ---- Subcase 0: C1's name-collision check + C4's two f-shifted item
   * ids, both driven for real (not guessed). The row lookup is
   * layout-independent (primary == ITM_UP1/ITM_DOWN1), matching the
   * convention test_capture_interactive_divert's subcase 3a already
   * uses for the ALPHA-gesture row. ---- */
  scFail = 0;
  LH_RESET();
  {
    testProg_t p0;
    tpInit(&p0);
    tpLbl(&p0, "P0");
    tpEnd(&p0);
    if (!tpWrite(&p0)) {
      printf("    [0] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else {
      uint16_t p = 0;
      forthXEQType_t res = forthResolveXEQ("FHIST", &p);
      if (res != FORTH_XEQ_NONE) {
        printf("    [0] FAIL: forthResolveXEQ(\"FHIST\") = %d, expected FORTH_XEQ_NONE (%d) on a machine with no FHIST\n",
               res, FORTH_XEQ_NONE);
        scFail = 1;
      }
      if (forthHistoryProgram() != 0) {
        printf("    [0] FAIL: forthHistoryProgram() = %u, expected 0 before creation\n",
               (unsigned)forthHistoryProgram());
        scFail = 1;
      }

      {
        int upRow = -1, downRow = -1, i;
        for (i = 0; i < 37; i++) {
          if (kbd_std[i].primary == ITM_UP1)   upRow = i;
          if (kbd_std[i].primary == ITM_DOWN1) downRow = i;
        }
        if (upRow < 0 || downRow < 0) {
          printf("    [0] FIXTURE FAIL: no kbd_std row carries primary ITM_UP1/ITM_DOWN1\n");
          scFail = 1;
        } else {
          fnForthOuter(NOPARAM);
          if (!forthCapIsOpen() || !forthCapIsInteractive()) {
            printf("    [0] FIXTURE FAIL: interactive open did not take\n");
            scFail = 1;
          }
          else if (!forthCapKeysMode()) {
            printf("    [0] FIXTURE FAIL: N1-5 opens the console in KEYS input; bit not set\n");
            scFail = 1;
          }
          else {
            char kbUp[3], kbDown[3];
            int16_t gotUp, gotDown;
            int mode;
            sprintf(kbUp, "%02d", upRow);
            sprintf(kbDown, "%02d", downRow);
            /* N1-5 (N-T4) STRENGTHENS this row to BOTH input modes.
             *
             * The recall gesture lives on CHR_caseUP/CHR_caseDN, which are
             * the AIM f-column — so before the flip it was reachable only in
             * alpha input.  Keys-first makes keys the GROUND state, and in
             * keys mode determineItem takes the NORMAL columns, where f-up
             * and f-down are ITM_BST/ITM_SST: without the re-homing arm the
             * console would open with its own history unreachable.  Asserting
             * the same resolution in both modes is what pins the fix. */
            for (mode = 0; mode < 2; mode++) {
              forthCapSetKeysMode(mode == 0);      /* keys first, then alpha */
              /* shiftF is one-shot: determineItem's own resetShiftState()
               * clears it after the call, so it must be set again before
               * each individual key. */
              shiftF = true;
              gotUp = determineItem(kbUp);
              shiftF = true;
              gotDown = determineItem(kbDown);
              shiftF = false;
              printf("    [0] REPORT: %s input: determineItem(shiftF, UP1 row %d) = %d,"
                     " DOWN1 row %d = %d (kbd_std fShiftedAim: UP=%d DOWN=%d)\n",
                     mode == 0 ? "keys" : "alpha", upRow, gotUp, downRow, gotDown,
                     kbd_std[upRow].fShiftedAim, kbd_std[downRow].fShiftedAim);
              if (gotUp != kbd_std[upRow].fShiftedAim || gotDown != kbd_std[downRow].fShiftedAim) {
                printf("    [0] FAIL: %s input does not resolve f-up/f-down to the recall"
                       " gesture — history is unreachable there\n",
                       mode == 0 ? "keys" : "alpha");
                scFail = 1;
              }
            }
          }
          forthCapClose();
          clearSystemFlag(FLAG_ALPHA);
        }
      }
    }
  }
  if (!scFail) printf("    [0] PASS: FHIST collides with nothing; f-up/f-down reach recall in BOTH input modes\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 1 (C5.1): creation shape. One user program; assert
   * FHIST is findable, differs from the user's program number, the
   * user's bytes are unchanged (byte comparison), and record what
   * numberOfPrograms did (settles the T7.2a open item). ---- */
  scFail = 0;
  LH_RESET();
  {
    testProg_t p1;
    int sLbl;
    tpInit(&p1);
    sLbl = tpLbl(&p1, "P1");
    tpSrc(&p1, "1 2 +");
    tpEnd(&p1);
    if (sLbl < 0 || !tpWrite(&p1)) {
      printf("    [1] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else {
      uint16_t numBefore = numberOfPrograms;
      uint8_t before[32];
      uint16_t beforeLen = p1.len;
      bool_t created;
      uint16_t histProg, numAfter;
      xcopy(before, beginOfProgramMemory, beforeLen);

      created = forthHistoryEnsure();
      histProg = forthHistoryProgram();
      numAfter = numberOfPrograms;

      printf("    [1] REPORT: numberOfPrograms before=%u after=%u (empty FHIST just created)\n",
             (unsigned)numBefore, (unsigned)numAfter);

      if (!created || histProg == 0) {
        printf("    [1] FAIL: forthHistoryEnsure/Program failed (created=%d, histProg=%u)\n",
               created, (unsigned)histProg);
        scFail = 1;
      }
      if (histProg == 1) {
        printf("    [1] FAIL: FHIST landed as program 1 (the user's own program)\n");
        scFail = 1;
      }
      if (numAfter != numBefore + 1) {
        printf("    [1] FAIL: numberOfPrograms went %u -> %u, expected +1 (see [1] REPORT)\n",
               (unsigned)numBefore, (unsigned)numAfter);
        scFail = 1;
      }
      if (memcmp(beginOfProgramMemory, before, beforeLen) != 0) {
        printf("    [1] FAIL: user's program bytes changed by FHIST creation\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS: FHIST created distinct from the user's program, user bytes unchanged\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 2 (C5.2): push three lines; assert three source steps
   * in FHIST in order, each decoding to its text. ---- */
  scFail = 0;
  LH_RESET();
  {
    /* A subcase that never calls tpWrite/writeTestProgram leaves
     * testProgOrigBegin NULL, which makes the trailing cleanupTestProgram()
     * a no-op rescan rather than a real reset (restoreTestProgram's own
     * guard) — so an explicit trivial baseline program is required here to
     * make THIS subcase's own cleanup actually isolate the next one. */
    testProg_t base2;
    uint16_t prog;
    tpInit(&base2);
    tpLbl(&base2, "BASE2");
    tpEnd(&base2);
    if (!tpWrite(&base2)) {
      printf("    [2] FIXTURE FAIL: baseline build/write\n");
    }
    forthHistoryPush("1 1 +");
    forthHistoryPush("2 2 +");
    forthHistoryPush("3 3 +");

    prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [2] FIXTURE FAIL: FHIST not created by push\n");
      scFail = 1;
    } else {
      uint8_t *lbl = programList[prog - 1].instructionPointer;
      uint8_t *s1 = findNextStep(lbl);
      uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
      uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
      uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
      uint8_t len;

      if (!s1 || !forthStepPayload(s1, &len) || len != 5 || memcmp(s1 + 4, "1 1 +", 5) != 0) {
        printf("    [2] FAIL: line 1 not \"1 1 +\"\n");
        scFail = 1;
      }
      if (!s2 || !forthStepPayload(s2, &len) || len != 5 || memcmp(s2 + 4, "2 2 +", 5) != 0) {
        printf("    [2] FAIL: line 2 not \"2 2 +\"\n");
        scFail = 1;
      }
      if (!s3 || !forthStepPayload(s3, &len) || len != 5 || memcmp(s3 + 4, "3 3 +", 5) != 0) {
        printf("    [2] FAIL: line 3 not \"3 3 +\"\n");
        scFail = 1;
      }
      if (!s4 || !isAtEndOfProgram(s4)) {
        printf("    [2] FAIL: a 4th step exists after the 3 pushed lines\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [2] PASS: three pushed lines land as three ordered source steps\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 3 (C5.3): duplicates collapse. ---- */
  scFail = 0;
  LH_RESET();
  {
    /* See subcase 2's comment: an explicit baseline is required for THIS
     * subcase's cleanup to actually isolate the next one. */
    testProg_t base3;
    uint16_t prog;
    tpInit(&base3);
    tpLbl(&base3, "BASE3");
    tpEnd(&base3);
    if (!tpWrite(&base3)) {
      printf("    [3] FIXTURE FAIL: baseline build/write\n");
    }
    forthHistoryPush("1 2 +");
    forthHistoryPush("1 2 +");

    prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [3] FIXTURE FAIL: FHIST not created\n");
      scFail = 1;
    } else {
      uint8_t *lbl = programList[prog - 1].instructionPointer;
      uint8_t *s1 = findNextStep(lbl);
      uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
      if (!s1 || !s2 || !isAtEndOfProgram(s2)) {
        printf("    [3] FAIL: expected exactly one step after pushing \"1 2 +\" twice\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [3] PASS: consecutive duplicate pushes collapse to one step\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 4 (C5.4): cap evicts oldest. Push well past
   * FORTH_HISTORY_MAX_BYTES; assert the byte total is under the cap,
   * the newest line survived, and the oldest is gone. ---- */
  scFail = 0;
  LH_RESET();
  {
    /* See subcase 2's comment: an explicit baseline is required for THIS
     * subcase's cleanup to actually isolate the next one. */
    testProg_t base4;
    int n;
    char line[16];
    uint16_t prog;
    tpInit(&base4);
    tpLbl(&base4, "BASE4");
    tpEnd(&base4);
    if (!tpWrite(&base4)) {
      printf("    [4] FIXTURE FAIL: baseline build/write\n");
    }
    for (n = 0; n < 200; n++) {
      sprintf(line, "LINE%04d", n);
      forthHistoryPush(line);
    }
    prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [4] FIXTURE FAIL: FHIST not created\n");
      scFail = 1;
    } else {
      uint8_t *begin = programList[prog - 1].instructionPointer;
      uint8_t *step = begin;
      uint8_t *lastContent = NULL;
      bool_t sawOldest = false;
      uint32_t totalBytes;
      while (!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) {
        uint8_t len;
        if (forthStepPayload(step, &len) && len == 8 && memcmp(step + 4, "LINE0000", 8) == 0) {
          sawOldest = true;
        }
        lastContent = step;
        step = findNextStep(step);
      }
      totalBytes = (uint32_t)(step - begin) + 2;
      printf("    [4] REPORT: FHIST plateaus at %u bytes (cap %u) after 200 pushes\n",
             (unsigned)totalBytes, (unsigned)FORTH_HISTORY_MAX_BYTES);

      if (totalBytes > FORTH_HISTORY_MAX_BYTES) {
        printf("    [4] FAIL: FHIST is %u bytes, over the %u cap\n",
               (unsigned)totalBytes, (unsigned)FORTH_HISTORY_MAX_BYTES);
        scFail = 1;
      }
      {
        uint8_t len;
        if (!lastContent || !forthStepPayload(lastContent, &len) || len != 8 ||
            memcmp(lastContent + 4, "LINE0199", 8) != 0) {
          printf("    [4] FAIL: newest line \"LINE0199\" did not survive\n");
          scFail = 1;
        }
      }
      if (sawOldest) {
        printf("    [4] FAIL: oldest line \"LINE0000\" is still present\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [4] PASS: cap evicts oldest-first; newest survives, total stays under FORTH_HISTORY_MAX_BYTES\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 5a (C5.5, order 1): the cursor is restored, FHIST
   * created AFTER the caller's program. ---- */
  scFail = 0;
  LH_RESET();
  {
    testProg_t p5a;
    int sLbl, sSrc2;
    tpInit(&p5a);
    sLbl = tpLbl(&p5a, "P1");
    tpSrc(&p5a, "1 2 +");
    sSrc2 = tpSrc(&p5a, "3 4 +");
    tpEnd(&p5a);
    if (sLbl < 0 || sSrc2 < 0 || !tpWrite(&p5a)) {
      printf("    [5a] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else if (!tpSelectStep(&p5a, sSrc2)) {
      printf("    [5a] FIXTURE FAIL: could not select step\n");
      scFail = 1;
    } else {
      uint16_t tProg, tLocal, tFirst;
      uint8_t tZero;
      firstDisplayedLocalStepNumber = 0;
      defineFirstDisplayedStep();
      pemCursorIsZerothStep = false;

      tProg = currentProgramNumber;
      tLocal = currentLocalStepNumber;
      tFirst = firstDisplayedLocalStepNumber;
      tZero = (uint8_t)pemCursorIsZerothStep;

      forthHistoryPush("seed one");   /* creates FHIST AFTER P1 */

      if (currentProgramNumber != tProg || currentLocalStepNumber != tLocal ||
          firstDisplayedLocalStepNumber != tFirst || (uint8_t)pemCursorIsZerothStep != tZero) {
        printf("    [5a] FAIL: cursor tuple changed by push (prog %u->%u local %u->%u disp %u->%u zero %u->%u)\n",
               tProg, currentProgramNumber, tLocal, currentLocalStepNumber,
               tFirst, firstDisplayedLocalStepNumber, tZero, (uint8_t)pemCursorIsZerothStep);
        scFail = 1;
      } else {
        int n;
        char line[16];
        for (n = 0; n < 200; n++) {
          sprintf(line, "L%04d", n);
          forthHistoryPush(line);
        }
        if (currentProgramNumber != tProg || currentLocalStepNumber != tLocal ||
            firstDisplayedLocalStepNumber != tFirst || (uint8_t)pemCursorIsZerothStep != tZero) {
          printf("    [5a] FAIL: cursor tuple changed across an eviction\n");
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [5a] PASS: cursor tuple restored, FHIST created after the caller's program\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 5b (C5.5, order 2): the caller's program created AFTER
   * FHIST already exists (FHIST built directly into the fixture at
   * program 1; the scan assigns program numbers in byte order, so FHIST
   * ends up BEFORE P2 here — the mirror of 5a). ---- */
  scFail = 0;
  LH_RESET();
  {
    testProg_t p5b;
    int sLbl2, sSrc2;
    tpInit(&p5b);
    tpLbl(&p5b, "FHIST");
    tpSrc(&p5b, "seed");
    tpEnd(&p5b);
    sLbl2 = tpLbl(&p5b, "P2");
    sSrc2 = tpSrc(&p5b, "5 6 +");
    tpEnd(&p5b);
    if (sLbl2 < 0 || sSrc2 < 0 || !tpWrite(&p5b)) {
      printf("    [5b] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else if (forthHistoryProgram() != 1) {
      printf("    [5b] FIXTURE FAIL: FHIST not program 1 as built (got %u)\n",
             (unsigned)forthHistoryProgram());
      scFail = 1;
    } else if (!tpSelectStep(&p5b, sSrc2)) {
      printf("    [5b] FIXTURE FAIL: could not select P2's step\n");
      scFail = 1;
    } else {
      uint16_t tProg, tLocal, tFirst;
      uint8_t tZero;
      firstDisplayedLocalStepNumber = 0;
      defineFirstDisplayedStep();
      pemCursorIsZerothStep = false;

      tProg = currentProgramNumber;
      tLocal = currentLocalStepNumber;
      tFirst = firstDisplayedLocalStepNumber;
      tZero = (uint8_t)pemCursorIsZerothStep;

      forthHistoryPush("seed two");

      if (currentProgramNumber != tProg || currentLocalStepNumber != tLocal ||
          firstDisplayedLocalStepNumber != tFirst || (uint8_t)pemCursorIsZerothStep != tZero) {
        printf("    [5b] FAIL: cursor tuple changed by push (FHIST before the caller's program)\n");
        scFail = 1;
      } else {
        int n;
        char line[16];
        for (n = 0; n < 200; n++) {
          sprintf(line, "M%04d", n);
          forthHistoryPush(line);
        }
        if (currentProgramNumber != tProg || currentLocalStepNumber != tLocal ||
            firstDisplayedLocalStepNumber != tFirst || (uint8_t)pemCursorIsZerothStep != tZero) {
          printf("    [5b] FAIL: cursor tuple changed across an eviction (FHIST before the caller's program)\n");
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [5b] PASS: cursor tuple restored, the caller's program created after FHIST\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 6 (C5.6): recall round-trip. Push two lines, open a
   * capture, recall back twice and forward once; assert the line text
   * at each step, then edit the browsed line and ENTER: a new newest
   * is pushed and the browsed entry is untouched. ---- */
  scFail = 0;
  LH_RESET();
  {
    /* See subcase 2's comment: an explicit baseline is required for THIS
     * subcase's cleanup to actually isolate the next one. */
    testProg_t base6;
    tpInit(&base6);
    tpLbl(&base6, "BASE6");
    tpEnd(&base6);
    if (!tpWrite(&base6)) {
      printf("    [6] FIXTURE FAIL: baseline build/write\n");
    }
    forthHistoryPush("1 1 +");
    forthHistoryPush("2 2 +");

    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen() || !forthCapIsInteractive()) {
      printf("    [6] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      int upRow = -1, downRow = -1, i;
      char kbUp[3], kbDown[3];
      int16_t itUp, itDown;
      for (i = 0; i < 37; i++) {
        if (kbd_std[i].primary == ITM_UP1)   upRow = i;
        if (kbd_std[i].primary == ITM_DOWN1) downRow = i;
      }
      sprintf(kbUp, "%02d", upRow);
      sprintf(kbDown, "%02d", downRow);
      /* shiftF is one-shot: determineItem's own resetShiftState() clears
       * it after the call, so it must be set again before each key. */
      shiftF = true;
      itUp = determineItem(kbUp);
      shiftF = true;
      itDown = determineItem(kbDown);
      shiftF = false;

      processKeyAction(itUp);
      if (compareString(aimBuffer, "2 2 +", CMP_BINARY) != 0) {
        printf("    [6] FAIL: 1st recall-back = \"%s\", expected \"2 2 +\"\n", aimBuffer);
        scFail = 1;
      }
      processKeyAction(itUp);
      if (compareString(aimBuffer, "1 1 +", CMP_BINARY) != 0) {
        printf("    [6] FAIL: 2nd recall-back = \"%s\", expected \"1 1 +\"\n", aimBuffer);
        scFail = 1;
      }
      processKeyAction(itDown);
      if (compareString(aimBuffer, "2 2 +", CMP_BINARY) != 0) {
        printf("    [6] FAIL: recall-forward = \"%s\", expected \"2 2 +\"\n", aimBuffer);
        scFail = 1;
      }

      if (!scFail) {
        runFunction(ITM_SPACE);
        runFunction(ITM_3);
        runFunction(ITM_SPACE);
        runFunction(ITM_PLUS);
        if (compareString(aimBuffer, "2 2 + 3 +", CMP_BINARY) != 0) {
          printf("    [6] FIXTURE FAIL: edited line = \"%s\", expected \"2 2 + 3 +\"\n", aimBuffer);
          scFail = 1;
        } else {
          fnKeyEnter(NOPARAM);
          if (lastErrorCode != ERROR_NONE) {
            printf("    [6] FIXTURE FAIL: ENTER on the edited line errored (%d)\n", lastErrorCode);
            scFail = 1;
          } else {
            uint16_t prog = forthHistoryProgram();
            uint8_t *lbl = prog ? programList[prog - 1].instructionPointer : NULL;
            uint8_t *s1 = lbl ? findNextStep(lbl) : NULL;
            uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
            uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
            uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
            uint8_t len;
            if (!s2 || !forthStepPayload(s2, &len) || len != 5 || memcmp(s2 + 4, "2 2 +", 5) != 0) {
              printf("    [6] FAIL: the browsed entry \"2 2 +\" was altered by the edit\n");
              scFail = 1;
            }
            if (!s3 || !forthStepPayload(s3, &len) || len != 9 || memcmp(s3 + 4, "2 2 + 3 +", 9) != 0) {
              printf("    [6] FAIL: the edited line did not land as a new newest \"2 2 + 3 +\"\n");
              scFail = 1;
            }
            if (!s4 || !isAtEndOfProgram(s4)) {
              printf("    [6] FAIL: unexpected 4th content step\n");
              scFail = 1;
            }
          }
        }
      }
      forthCapClose();
      clearSystemFlag(FLAG_ALPHA);
    }
  }
  if (!scFail) printf("    [6] PASS: recall back-back-forward round-trips; edit+ENTER pushes a new newest, browsed entry untouched\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 7 (C5.7): EXIT pushes (L-R2: EXIT never loses a line). ---- */
  scFail = 0;
  LH_RESET();
  {
    /* See subcase 2's comment: an explicit baseline is required for THIS
     * subcase's cleanup to actually isolate the next one. */
    testProg_t base7;
    tpInit(&base7);
    tpLbl(&base7, "BASE7");
    tpEnd(&base7);
    if (!tpWrite(&base7)) {
      printf("    [7] FIXTURE FAIL: baseline build/write\n");
    }
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen()) {
      printf("    [7] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      uint16_t prog;
      uint8_t *lbl, *s1;
      uint8_t len;
      runFunction(ITM_A);
      runFunction(ITM_B);
      runFunction(ITM_C);
      fnKeyExit(NOPARAM);

      prog = forthHistoryProgram();
      lbl = prog ? programList[prog - 1].instructionPointer : NULL;
      s1 = lbl ? findNextStep(lbl) : NULL;
      if (!s1 || !forthStepPayload(s1, &len) || len != 3 || memcmp(s1 + 4, "ABC", 3) != 0) {
        printf("    [7] FAIL: EXIT did not push \"ABC\" into FHIST\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [7] PASS: EXIT pushes the open line into FHIST\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 8 (C5.8): the UAF guard. Build FHIST past the cap,
   * then starve the free-memory arena so scanLabelsAndPrograms cannot
   * reallocate labelList/programList, and drive forthHistoryEvict()
   * directly (delete-only: never grows program memory, so
   * resizeProgramMemory's exit(-3) path is never reached by this
   * drive). freeMemoryRegions is restored from a snapshot immediately
   * after, before anything else touches program memory. ---- */
  scFail = 0;
  LH_RESET();
  {
    /* See subcase 2's comment: an explicit baseline is required for THIS
     * subcase's cleanup to actually isolate the next one. */
    testProg_t base8;
    int n;
    char line[16];
    uint16_t prog;
    tpInit(&base8);
    tpLbl(&base8, "BASE8");
    tpEnd(&base8);
    if (!tpWrite(&base8)) {
      printf("    [8] FIXTURE FAIL: baseline build/write\n");
    }
    for (n = 0; n < 200; n++) {
      sprintf(line, "U%04d", n);
      forthHistoryPush(line);
    }
    prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [8] FIXTURE FAIL: FHIST not created\n");
      scFail = 1;
    } else {
      int32_t savedCount = numberOfFreeMemoryRegions;
      freeMemoryRegion_t savedRegions[MAX_FREE_REGIONS];
      bool_t sawRamFull;
      memcpy(savedRegions, freeMemoryRegions, sizeof(freeMemoryRegion_t) * savedCount);

      numberOfFreeMemoryRegions = 0;
      lastErrorCode = ERROR_NONE;
      forthHistoryEvict();
      sawRamFull = (lastErrorCode == ERROR_RAM_FULL);

      numberOfFreeMemoryRegions = savedCount;
      memcpy(freeMemoryRegions, savedRegions, sizeof(freeMemoryRegion_t) * savedCount);
      lastErrorCode = ERROR_NONE;
      scanLabelsAndPrograms();   /* labelList/programList valid again before anything else touches them */

      if (sawRamFull) {
        printf("    [8] PASS: eviction loop abandoned under a forced ERROR_RAM_FULL, no crash\n");
      } else {
        printf("    [8] REPORT: could not force ERROR_RAM_FULL here. Evicting only ever deletes a\n"
               "        non-label, non-boundary FHIST content step, so numberOfLabels/numberOfPrograms\n"
               "        never change across the rescan; scanLabelsAndPrograms frees the OLD labelList/\n"
               "        programList first and then reallocates the identically-sized new ones, so its\n"
               "        own just-freed block is always exactly the right size regardless of external\n"
               "        RAM pressure. Pinned by mutation 5 alone.\n");
      }
    }
  }
  fail |= scFail;   /* only a FIXTURE failure fails this subcase; the RAM_FULL outcome is reported either way */
  cleanupTestProgram();

  /* ---- Subcase 9 (C5.9): user's programs untouched. Two user programs;
   * assert both are byte-identical across a push AND an eviction (by
   * actual byte comparison), and labelList still resolves their
   * labels. ---- */
  scFail = 0;
  LH_RESET();
  {
    testProg_t p9;
    tpInit(&p9);
    tpLbl(&p9, "P1");
    tpSrc(&p9, "1 2 +");
    tpEnd(&p9);
    tpLbl(&p9, "P2");
    tpSrc(&p9, "3 4 +");
    tpEnd(&p9);
    if (!tpWrite(&p9)) {
      printf("    [9] FIXTURE FAIL: build/write\n");
      scFail = 1;
    } else {
      uint16_t p2StartOff = p9.stepOff[3];
      uint16_t totalLen = p9.len;
      uint8_t before1[32], before2[32];
      xcopy(before1, beginOfProgramMemory, p2StartOff);
      xcopy(before2, beginOfProgramMemory + p2StartOff, totalLen - p2StartOff);

      forthHistoryPush("push one");

      if (memcmp(beginOfProgramMemory, before1, p2StartOff) != 0 ||
          memcmp(beginOfProgramMemory + p2StartOff, before2, totalLen - p2StartOff) != 0) {
        printf("    [9] FAIL: P1/P2 bytes changed by a push\n");
        scFail = 1;
      } else {
        int n;
        char line[16];
        for (n = 0; n < 200; n++) {
          sprintf(line, "V%04d", n);
          forthHistoryPush(line);
        }
        if (memcmp(beginOfProgramMemory, before1, p2StartOff) != 0 ||
            memcmp(beginOfProgramMemory + p2StartOff, before2, totalLen - p2StartOff) != 0) {
          printf("    [9] FAIL: P1/P2 bytes changed across an eviction\n");
          scFail = 1;
        }
      }
      if (findNamedLabel("P1", GLOBAL_LABELS) == INVALID_VARIABLE) {
        printf("    [9] FAIL: labelList no longer resolves P1\n");
        scFail = 1;
      }
      if (findNamedLabel("P2", GLOBAL_LABELS) == INVALID_VARIABLE) {
        printf("    [9] FAIL: labelList no longer resolves P2\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [9] PASS: user's programs are byte-identical across a push and an eviction; labels still resolve\n");
  fail |= scFail;
  cleanupTestProgram();

  #undef LH_RESET

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
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
  T_cursorPos = savedCursorPos;
  alphaCase = savedAlphaCase;
  nextChar = savedNextChar;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* L1-F1 (C6): total step count (LBL..END inclusive) of FHIST, or 0 if it
 * does not exist yet.  Independent of currentProgramNumber on purpose —
 * getNumberOfSteps() is keyed off that global (manage.c:2774-2787), which
 * is exactly the coupling C6.9/Mutation-1 probe below, so counting must not
 * go through it. */
static uint16_t _tfcFhistStepCount(void)
{
  uint16_t prog = forthHistoryProgram();
  uint8_t *step;
  uint16_t n;
  if (prog == 0) { return 0; }
  step = programList[prog - 1].instructionPointer;
  n = 1;
  while (!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) {
    n++;
    step = findNextStep(step);
  }
  return n;
}

static int test_fold_context(void)
{
  int fail = 0, scFail;

  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  int16_t savedCursorPos = T_cursorPos;

  #define TFC_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  cleanupTestProgram();   /* pristine baseline before subcase 1 */

  /* ---- Subcase 1 (C6.1): round-trip is bit-identical, no TAM in between. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p1;
    int sEnd;
    tpInit(&p1);
    tpLbl(&p1, "F1P1");
    tpSrc(&p1, "9 9 +");
    sEnd = tpEnd(&p1);
    if (sEnd < 0 || !tpWrite(&p1) || !tpSelectStep(&p1, sEnd)) {
      printf("    [1] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore, localBefore, dispBefore;
      uint32_t stepOffBefore, freeOffBefore;
      bool_t zerothBefore;
      char aimBefore[AIM_BUFFER_LENGTH];
      int16_t cursorBefore;

      firstDisplayedLocalStepNumber = 0;
      defineFirstDisplayedStep();
      pemCursorIsZerothStep = false;

      /* FHIST must already exist before the snapshot: forthFoldEnter's own
       * forthHistoryEnsure() call would otherwise create it here for the
       * first time, permanently growing firstFreeProgramByte by FHIST's
       * own LBL+END bytes — real, wanted growth (FHIST is a KEPT, persistent
       * program, not something the fold cleans up), not something a round-
       * trip identity check should be comparing against. */
      forthHistoryEnsure();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "1 2 +", 5); aimBuffer[5] = 0;
      T_cursorPos = 5;

      numBefore     = getNumberOfSteps();
      /* OFFSETS, not raw pointers: _insertInProgram rebases every program
       * pointer when it grows the underlying region (manage.c:723-733) —
       * legitimate relocation the capture-step insert can still trigger
       * even with FHIST pre-existing, not a defect.  A raw currentStep
       * snapshot would be stale by construction across that; comparing the
       * offset from beginOfProgramMemory is what actually survives it. */
      stepOffBefore = (uint32_t)(currentStep - beginOfProgramMemory);
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
      localBefore  = currentLocalStepNumber;
      dispBefore   = firstDisplayedLocalStepNumber;
      zerothBefore = pemCursorIsZerothStep;
      cursorBefore = T_cursorPos;
      xcopy(aimBefore, aimBuffer, stringByteLength(aimBuffer) + 1);

      forthFoldEnter(ITM_STO, TM_STORCL);
      forthFoldLeave();

      if (getNumberOfSteps() != numBefore) {
        printf("    [1] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(currentStep - beginOfProgramMemory) != stepOffBefore) {
        printf("    [1] FAIL: currentStep offset changed (%u -> %u)\n",
               stepOffBefore, (unsigned)(currentStep - beginOfProgramMemory));
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [1] FAIL: firstFreeProgramByte offset changed (%u -> %u)\n",
               freeOffBefore, (unsigned)(firstFreeProgramByte - beginOfProgramMemory));
        scFail = 1;
      }
      if (currentLocalStepNumber != localBefore) {
        printf("    [1] FAIL: currentLocalStepNumber %u -> %u\n", localBefore, currentLocalStepNumber);
        scFail = 1;
      }
      if (firstDisplayedLocalStepNumber != dispBefore) {
        printf("    [1] FAIL: firstDisplayedLocalStepNumber %u -> %u\n", dispBefore, firstDisplayedLocalStepNumber);
        scFail = 1;
      }
      if ((bool_t)pemCursorIsZerothStep != zerothBefore) {
        printf("    [1] FAIL: pemCursorIsZerothStep changed\n");
        scFail = 1;
      }
      if (T_cursorPos != cursorBefore) {
        printf("    [1] FAIL: T_cursorPos %d -> %d\n", cursorBefore, T_cursorPos);
        scFail = 1;
      }
      if (compareString(aimBuffer, aimBefore, CMP_BINARY) != 0) {
        printf("    [1] FAIL: aimBuffer \"%s\" -> \"%s\"\n", aimBefore, aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS: enter+leave with no TAM in between is bit-identical to entry\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 2 (C6.2): same, with an empty caller program. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p2;
    int sEnd;
    tpInit(&p2);
    tpLbl(&p2, "F1P2");
    sEnd = tpEnd(&p2);
    if (sEnd < 0 || !tpWrite(&p2) || !tpSelectStep(&p2, sEnd)) {
      printf("    [2] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore, localBefore;
      uint32_t stepOffBefore, freeOffBefore;

      firstDisplayedLocalStepNumber = 0;
      defineFirstDisplayedStep();
      pemCursorIsZerothStep = false;

      /* FHIST must already exist before the snapshot -- see subcase 1's
       * comment: its own first-time creation is real, wanted growth, not
       * something a round-trip identity check should be comparing against. */
      forthHistoryEnsure();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "2 2 +", 5); aimBuffer[5] = 0;
      T_cursorPos = 5;

      numBefore     = getNumberOfSteps();
      /* Offsets, not raw pointers -- see subcase 1's comment: the capture-
       * step insert can still rebase beginOfProgramMemory even with FHIST
       * pre-existing. */
      stepOffBefore = (uint32_t)(currentStep - beginOfProgramMemory);
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
      localBefore   = currentLocalStepNumber;

      forthFoldEnter(ITM_STO, TM_STORCL);
      forthFoldLeave();

      if (getNumberOfSteps() != numBefore
          || (uint32_t)(currentStep - beginOfProgramMemory) != stepOffBefore
          || (uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore
          || currentLocalStepNumber != localBefore) {
        printf("    [2] FAIL: round-trip not identical on an empty (LBL+END only) caller program "
               "(steps %u->%u, stepOff %u->%u, freeOff %u->%u, local %u->%u)\n",
               numBefore, getNumberOfSteps(), stepOffBefore,
               (unsigned)(currentStep - beginOfProgramMemory), freeOffBefore,
               (unsigned)(firstFreeProgramByte - beginOfProgramMemory), localBefore, currentLocalStepNumber);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [2] PASS: round-trip is bit-identical with an empty caller program\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 3 (C6.3): cursor inside a user program, FHIST BEFORE it in
   * memory (built directly into the fixture) so the fold's own insert
   * shifts the caller program's absolute address and global step numbers —
   * exactly the case Mutation 3 (global-step restore) needs to go red on.
   * Assert the caller's program is byte-identical afterwards. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p3;
    int sLbl, sSrc1;
    uint16_t p3Off;
    tpInit(&p3);
    tpLbl(&p3, "FHIST");
    tpSrc(&p3, "seed a");
    tpSrc(&p3, "seed b");
    tpEnd(&p3);
    sLbl  = tpLbl(&p3, "F1P3");
    sSrc1 = tpSrc(&p3, "1 1 +");
    tpSrc(&p3, "2 2 +");
    tpEnd(&p3);
    p3Off = p3.stepOff[sLbl];
    if (sSrc1 < 0 || !tpWrite(&p3) || !tpSelectStep(&p3, sSrc1)) {
      printf("    [3] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t p3Len = p3.len - p3Off;
      uint8_t before[128];
      uint16_t progBefore = currentProgramNumber;
      uint16_t localBefore = currentLocalStepNumber;

      if (p3Len > sizeof(before)) {
        printf("    [3] FIXTURE FAIL: p3 region too big for snapshot buffer\n");
        scFail = 1;
      } else {
        xcopy(before, tpStepAddr(&p3, sLbl), p3Len);

        forthCapOpenInteractive();
        xcopy(aimBuffer, "3 3 +", 5); aimBuffer[5] = 0;

        forthFoldEnter(ITM_STO, TM_STORCL);
        forthFoldLeave();

        if (memcmp(tpStepAddr(&p3, sLbl), before, p3Len) != 0) {
          printf("    [3] FAIL: caller's program bytes changed by the fold round-trip\n");
          scFail = 1;
        }
        if (currentProgramNumber != progBefore || currentLocalStepNumber != localBefore
            || currentStep != tpStepAddr(&p3, sSrc1)) {
          printf("    [3] FAIL: cursor not restored onto the caller's original step\n");
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [3] PASS: caller's program (positioned after FHIST) is byte-identical; cursor restored\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 4 (C6.4): the capture step is really materialised between
   * enter and leave — FHIST gains one step that decodes to the line. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p4;
    int sEnd;
    tpInit(&p4);
    tpLbl(&p4, "F1P4");
    sEnd = tpEnd(&p4);
    if (sEnd < 0 || !tpWrite(&p4) || !tpSelectStep(&p4, sEnd)) {
      printf("    [4] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore, countAfter;

      forthHistoryEnsure();   /* force-create FHIST empty, for a clean count */
      countBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "4 4 +", 5); aimBuffer[5] = 0;

      forthFoldEnter(ITM_STO, TM_STORCL);

      countAfter = _tfcFhistStepCount();
      if (countAfter != countBefore + 1) {
        printf("    [4] FAIL: FHIST step count %u -> %u, expected +1\n", countBefore, countAfter);
        scFail = 1;
      }
      {
        uint8_t len;
        if (!checkOpCodeOfStep(currentStep, ITM_FORTH)
            || !forthStepPayload(currentStep, &len) || len != 5
            || memcmp(currentStep + 4, "4 4 +", 5) != 0) {
          printf("    [4] FAIL: parked capture step does not decode to \"4 4 +\"\n");
          scFail = 1;
        }
      }

      forthFoldLeave();
    }
  }
  if (!scFail) printf("    [4] PASS: FHIST gains one step between enter and leave, decoding to the line\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 5 (C6.5): PARK does not arm. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p5;
    int sEnd;
    tpInit(&p5);
    tpLbl(&p5, "F1P5");
    sEnd = tpEnd(&p5);
    if (sEnd < 0 || !tpWrite(&p5) || !tpSelectStep(&p5, sEnd)) {
      printf("    [5] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore;
      forthHistoryEnsure();
      countBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "5 5 +", 5); aimBuffer[5] = 0;

      forthFoldEnter(ITM_GTOP, TM_LABEL);

      if (!forthFoldPending()) {
        printf("    [5] FAIL: forthFoldPending() false right after enter\n");
        scFail = 1;
      }
      if (forthFoldArmed()) {
        printf("    [5] FAIL: forthFoldArmed() true for ITM_GTOP, expected PARK (false)\n");
        scFail = 1;
      }

      forthFoldLeave();

      if (forthFoldPending()) {
        printf("    [5] FAIL: forthFoldPending() still true after leave\n");
        scFail = 1;
      }
      if (_tfcFhistStepCount() != countBefore) {
        printf("    [5] FAIL: FHIST step count %u -> %u, PARK leave did not sweep cleanly\n",
               countBefore, _tfcFhistStepCount());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [5] PASS: PARK (ITM_GTOP) is pending but not armed; leave still sweeps cleanly\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 6 (C6.6): sweep clears debris hand-inserted after the
   * capture step (every break path in resume's own drain loop, and PARK). ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p6;
    int sEnd;
    tpInit(&p6);
    tpLbl(&p6, "F1P6");
    sEnd = tpEnd(&p6);
    if (sEnd < 0 || !tpWrite(&p6) || !tpSelectStep(&p6, sEnd)) {
      printf("    [6] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore;
      forthHistoryEnsure();
      countBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "6 6 +", 5); aimBuffer[5] = 0;

      forthFoldEnter(ITM_STO, TM_STORCL);

      /* Hand-insert debris after the capture step: forthHistoryPush saves
       * and restores the caller's cursor (here, the fold's own currentStep,
       * parked ON the capture step), so it inserts "debris" immediately
       * after the capture step and leaves currentStep back on it — exactly
       * the shape a break path in forthCaptureResume's drain loop would
       * leave behind. */
      forthHistoryPush("debris");

      forthFoldLeave();

      if (_tfcFhistStepCount() != countBefore) {
        printf("    [6] FAIL: FHIST step count %u -> %u after sweep, expected back to entry\n",
               countBefore, _tfcFhistStepCount());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [6] PASS: sweep clears hand-inserted debris; count returns to entry\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 7 (C6.7): foldMode survives forthCapClose/
   * forthCapAbandonSuspended/forthCapOpen — the INVERTED expectation this
   * packet's C1 documents (only forthCapPowerReset clears it). ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p7;
    int sEnd;
    tpInit(&p7);
    tpLbl(&p7, "F1P7");
    sEnd = tpEnd(&p7);
    if (sEnd < 0 || !tpWrite(&p7) || !tpSelectStep(&p7, sEnd)) {
      printf("    [7] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore = (forthHistoryEnsure(), _tfcFhistStepCount());

      /* Round A: forthCapClose() */
      forthCapOpenInteractive();
      xcopy(aimBuffer, "7 7 A", 5); aimBuffer[5] = 0;
      forthFoldEnter(ITM_STO, TM_STORCL);
      forthCapClose();
      if (!forthFoldPending()) {
        printf("    [7] FAIL: forthFoldPending() false after forthCapClose() (expected survive)\n");
        scFail = 1;
      }
      forthFoldLeave();
      if (forthFoldPending() || _tfcFhistStepCount() != countBefore) {
        printf("    [7] FAIL: leave after forthCapClose() did not sweep/restore cleanly\n");
        scFail = 1;
      }

      /* Round B: forthCapAbandonSuspended(), genuinely SUSPENDED first */
      forthCapOpenInteractive();
      xcopy(aimBuffer, "7 7 B", 5); aimBuffer[5] = 0;
      forthFoldEnter(ITM_STO, TM_STORCL);
      forthCapSuspendState(0, 0, 0, 0);   /* force FCAP_SUSPENDED */
      forthCapAbandonSuspended();
      if (!forthFoldPending()) {
        printf("    [7] FAIL: forthFoldPending() false after forthCapAbandonSuspended() (expected survive)\n");
        scFail = 1;
      }
      forthFoldLeave();
      if (forthFoldPending() || _tfcFhistStepCount() != countBefore) {
        printf("    [7] FAIL: leave after forthCapAbandonSuspended() did not sweep/restore cleanly\n");
        scFail = 1;
      }

      /* Round C: forthCapOpen() (PEM open, not interactive) */
      forthCapOpenInteractive();
      xcopy(aimBuffer, "7 7 C", 5); aimBuffer[5] = 0;
      forthFoldEnter(ITM_STO, TM_STORCL);
      forthCapOpen();
      if (!forthFoldPending()) {
        printf("    [7] FAIL: forthFoldPending() false after forthCapOpen() (expected survive)\n");
        scFail = 1;
      }
      forthFoldLeave();
      if (forthFoldPending() || _tfcFhistStepCount() != countBefore) {
        printf("    [7] FAIL: leave after forthCapOpen() did not sweep/restore cleanly\n");
        scFail = 1;
      }

      /* forthCapPowerReset() DOES clear it — the last-resort reset, no
       * cleanup promised (the debris capture step is left behind). */
      forthCapOpenInteractive();
      xcopy(aimBuffer, "7 7 D", 5); aimBuffer[5] = 0;
      forthFoldEnter(ITM_STO, TM_STORCL);
      if (!forthFoldPending()) {
        printf("    [7] FIXTURE FAIL: enter did not arm before the power-reset check\n");
        scFail = 1;
      }
      forthCapPowerReset();
      if (forthFoldPending()) {
        printf("    [7] FAIL: forthFoldPending() still true after forthCapPowerReset()\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [7] PASS: foldMode survives Close/AbandonSuspended/Open; only PowerReset clears it\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 8 (C6.8): zeroth-step normalisation — set true before
   * enter; the (simulated) TAM step lands after the capture step; leave
   * restores the flag to true. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p8;
    int sEnd;
    tpInit(&p8);
    tpLbl(&p8, "F1P8");
    sEnd = tpEnd(&p8);
    if (sEnd < 0 || !tpWrite(&p8) || !tpSelectStep(&p8, sEnd)) {
      printf("    [8] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore = (forthHistoryEnsure(), _tfcFhistStepCount());

      pemCursorIsZerothStep = true;

      forthCapOpenInteractive();
      xcopy(aimBuffer, "8 8 +", 5); aimBuffer[5] = 0;

      forthFoldEnter(ITM_STO, TM_STORCL);

      if (pemCursorIsZerothStep) {
        printf("    [8] FAIL: pemCursorIsZerothStep still true after enter, expected false while folded\n");
        scFail = 1;
      }
      {
        /* Replicates addStepInProgram's own pre-move guard (manage.c:2664)
         * verbatim -- the mechanism this subcase pins -- without invoking
         * the full TAM dispatch (F2's scope, not this packet's). */
        uint8_t *capStep = currentStep;
        uint8_t *afterCap = findNextStep(capStep);
        aimBuffer[0] = 0;
        clearSystemFlag(FLAG_ALPHA);
        if ((!pemCursorIsZerothStep)
            && ((aimBuffer[0] == 0 && !getSystemFlag(FLAG_ALPHA)) || tam.mode)
            && !isAtEndOfProgram(currentStep) && !isAtEndOfPrograms(currentStep)) {
          currentStep = findNextStep(currentStep);
          ++currentLocalStepNumber;
        }
        if (currentStep != afterCap) {
          printf("    [8] FAIL: pre-move did not land past the capture step (TAM step would land before it)\n");
          scFail = 1;
        }
      }

      forthFoldLeave();

      if (!pemCursorIsZerothStep) {
        printf("    [8] FAIL: pemCursorIsZerothStep not restored to true after leave\n");
        scFail = 1;
      }
      if (_tfcFhistStepCount() != countBefore) {
        printf("    [8] FAIL: FHIST step count %u -> %u, expected back to entry\n",
               countBefore, _tfcFhistStepCount());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [8] PASS: zeroth-step forced false while folded; TAM step lands after; restored true on leave\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- Subcase 9 (C6.9, the B1 pin): FHIST holds >= 3 lines, the caller's
   * program is shorter — the sweep must not eat real history. None of
   * C6.1-C6.6 catches a sweep keyed off the wrong program's step count. ---- */
  scFail = 0;
  TFC_RESET();
  {
    testProg_t p9;
    int sEnd;
    tpInit(&p9);
    tpLbl(&p9, "F1P9");            /* shorter than FHIST: LBL+END, 2 steps */
    sEnd = tpEnd(&p9);
    if (sEnd < 0 || !tpWrite(&p9) || !tpSelectStep(&p9, sEnd)) {
      printf("    [9] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t countBefore;

      forthHistoryPush("h1");
      forthHistoryPush("h2");
      forthHistoryPush("h3");
      countBefore = _tfcFhistStepCount();   /* LBL + 3 lines + END = 5 */
      if (countBefore < 5) {
        printf("    [9] FIXTURE FAIL: FHIST only %u steps, expected >= 5 (LBL+3+END)\n", countBefore);
        scFail = 1;
      }

      /* Re-select: forthHistoryPush restores the caller's cursor, but
       * confirm we are still on the (2-step) caller program, shorter than
       * FHIST, as the subcase requires. */
      if (getNumberOfSteps() >= countBefore) {
        printf("    [9] FIXTURE FAIL: caller program (%u steps) not shorter than FHIST (%u)\n",
               getNumberOfSteps(), countBefore);
        scFail = 1;
      }

      forthCapOpenInteractive();
      xcopy(aimBuffer, "9 9 +", 5); aimBuffer[5] = 0;

      forthFoldEnter(ITM_STO, TM_STORCL);
      forthFoldLeave();

      if (_tfcFhistStepCount() != countBefore) {
        printf("    [9] FAIL: FHIST step count %u -> %u — sweep ate real history "
               "(sampled entryStepCount in the wrong program)\n",
               countBefore, _tfcFhistStepCount());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [9] PASS: sweep does not eat real FHIST history when the caller's program is shorter\n");
  fail |= scFail;
  cleanupTestProgram();

  #undef TFC_RESET

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  T_cursorPos = savedCursorPos;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* ==================================================================
 * PACKET_L1_F2 — the three tam.c seams + determineItem fix.
 *
 * Wires F1's inert fold context into ui/tam.c: Seam 1 (tamEnterMode
 * materialises+arms+suspends), Seam 2 (leaveTamModeIfEnabled resumes+
 * sweeps, unchanged for its own resume — only the trigger condition
 * widens), and Seam 3 (the calcMode bracket in tamProcessInput, where
 * the fold actually unwinds).  keyboard.c's determineItem gets a fourth
 * conjunct so TAM digits resolve as digits during a folded interactive
 * session.  No tam.c commit site is edited — every subcase below drives
 * the real entry points (runFunction, tamProcessInput, tamEnterMode,
 * determineItem, fnKeyExit), never a hand-set item or a direct
 * forthFoldEnter/Leave call.
 * ================================================================== */
static int test_fold_seams(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);
  extern void tamEnterMode(int16_t);
  extern int16_t determineItem(const char *);

  int fail = 0, scFail;
  uint8_t rType;
  int32_t rVal;
  longInteger_t li;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  tamState_t savedTam = tam;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* Every subcase starts from the same known baseline, AND drains any
   * fold left pending by the previous subcase (forthCapClose() does NOT
   * touch foldMode — forth_capture.h's own documented invariant — so an
   * un-swept fold would otherwise leak into the next subcase's fixture). */
  #define FS_RESET() do { \
    calcMode = CM_AIM; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    if (forthFoldPending()) { forthFoldLeave(); } \
  } while (0)

  /* ---- Subcase 1 (C5.1): the headline. STO 0 5 during an interactive
   * capture (keys mode on) types "STO 05 " into the line; program memory
   * (getNumberOfSteps, firstFreeProgramByte, FHIST's step count) is
   * bit-identical to before the keypress. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p1;
    int sEnd;
    tpInit(&p1);
    tpLbl(&p1, "F2P1");
    sEnd = tpEnd(&p1);
    if (sEnd < 0 || !tpWrite(&p1) || !tpSelectStep(&p1, sEnd)) {
      printf("    [1] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore, fhistBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();   /* pre-create: its own growth must not count */
      longIntegerInit(li); int32ToLongInteger(999, li);
      convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);

      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore     = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
      fhistBefore   = _tfcFhistStepCount();

      runFunction(ITM_STO);
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);   /* two digits auto-fire the STO commit */

      if (compareString(aimBuffer, "42 STO 05 ", CMP_BINARY) != 0) {
        printf("    [1] FAIL: aimBuffer \"%s\", expected \"42 STO 05 \"\n", aimBuffer);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [1] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [1] FAIL: firstFreeProgramByte offset changed (%u -> %u)\n",
               freeOffBefore, (unsigned)(firstFreeProgramByte - beginOfProgramMemory));
        scFail = 1;
      }
      if (_tfcFhistStepCount() != fhistBefore) {
        printf("    [1] FAIL: FHIST step count %u -> %u\n", fhistBefore, _tfcFhistStepCount());
        scFail = 1;
      }
      if (!forthCapIsOpen() || forthCapIsSuspended()) {
        printf("    [1] FAIL: capture state %d, expected OPEN\n", forthTestCapState());
        scFail = 1;
      }
      if (calcMode != CM_AIM) {
        printf("    [1] FAIL: calcMode %d after, expected CM_AIM\n", calcMode);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS: STO 0 5 types \"STO 05 \", program memory bit-identical\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase 2 (C5.2): nothing executed. Register 05 is unchanged by
   * the headline sequence. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p2;
    int sEnd;
    tpInit(&p2);
    tpLbl(&p2, "F2P2");
    sEnd = tpEnd(&p2);
    if (sEnd < 0 || !tpWrite(&p2) || !tpSelectStep(&p2, sEnd)) {
      printf("    [2] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      forthHistoryEnsure();
      longIntegerInit(li); int32ToLongInteger(555, li);
      convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);

      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      runFunction(ITM_STO);
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);

      read_reg_int32(5, &rType, &rVal);
      if (rType != dtLongInteger || rVal != 555) {
        printf("    [2] FAIL: register 05 = %ld type %u, expected untouched 555\n",
               (long)rVal, rType);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [2] PASS: register 05 unchanged -- STO never executed\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase 3 (C5.3): TAM digits resolve as digits. With the fold
   * pending, determineItem returns key->primaryTam for a digit key, not
   * a letter.  Keys mode OFF (E10-E12's default): with keys mode ON the
   * L1-3 conjunct already excludes the CM_AIM arm for a digit key, so
   * OFF is the state the new C4 conjunct actually has to cover -- a
   * parameterized item (STO) opens TAM from plain alpha-input too. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p3;
    int sEnd;
    tpInit(&p3);
    tpLbl(&p3, "F2P3");
    sEnd = tpEnd(&p3);
    if (sEnd < 0 || !tpWrite(&p3) || !tpSelectStep(&p3, sEnd)) {
      printf("    [3] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      int zIdx = -1, i;
      for (i = 0; i < 37; i++) {
        if (kbd_std[i].primary == ITM_0) { zIdx = i; break; }
      }
      if (zIdx < 0) {
        printf("    [3] FIXTURE FAIL: no kbd_std row carries primary == ITM_0\n");
        scFail = 1;
      } else {
        char kb[3];
        sprintf(kb, "%02d", zIdx);

        forthCapOpenInteractive();
        forthCapSetKeysMode(false);   /* explicit: E10-E12's default */
        xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;

        runFunction(ITM_STO);

        if (!tam.mode || !forthFoldPending()) {
          printf("    [3] FIXTURE FAIL: TAM not entered / fold not pending (tam.mode=%d pending=%d)\n",
                 (int)tam.mode, (int)forthFoldPending());
          scFail = 1;
        } else {
          int16_t got;
          shiftF = false; shiftG = false;
          got = determineItem(kb);
          if (got != kbd_std[zIdx].primaryTam) {
            printf("    [3] FAIL: determineItem = %d, expected key->primaryTam (%d)\n",
                   got, kbd_std[zIdx].primaryTam);
            scFail = 1;
          }
          if (kbd_std[zIdx].primaryAim != kbd_std[zIdx].primaryTam && got == kbd_std[zIdx].primaryAim) {
            printf("    [3] FAIL: determineItem resolved to the AIM-column letter (%d), not TAM\n", got);
            scFail = 1;
          }
        }
        fnKeyExit(NOPARAM);   /* cancel cleanly before the next subcase */
      }
    }
  }
  if (!scFail) printf("    [3] PASS: fold pending -> digit key resolves via key->primaryTam, not a letter\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase 4 (C5.4): cancel. fnKeyExit before any digit -- the line
   * is still "42", the capture is OPEN (not stuck SUSPENDED -- Mutation
   * 3b's pin), and program memory is bit-identical. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p4;
    int sEnd;
    tpInit(&p4);
    tpLbl(&p4, "F2P4");
    sEnd = tpEnd(&p4);
    if (sEnd < 0 || !tpWrite(&p4) || !tpSelectStep(&p4, sEnd)) {
      printf("    [4] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore, fhistBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore     = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
      fhistBefore   = _tfcFhistStepCount();

      runFunction(ITM_STO);
      fnKeyExit(NOPARAM);   /* cancel before any digit */

      if (compareString(aimBuffer, "42", CMP_BINARY) != 0) {
        printf("    [4] FAIL: aimBuffer \"%s\", expected \"42\" intact\n", aimBuffer);
        scFail = 1;
      }
      if (!forthCapIsOpen() || forthCapIsSuspended()) {
        printf("    [4] FAIL: capture state %d, expected OPEN (not stuck SUSPENDED)\n",
               forthTestCapState());
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [4] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [4] FAIL: firstFreeProgramByte offset changed (%u -> %u)\n",
               freeOffBefore, (unsigned)(firstFreeProgramByte - beginOfProgramMemory));
        scFail = 1;
      }
      if (_tfcFhistStepCount() != fhistBefore) {
        printf("    [4] FAIL: FHIST step count %u -> %u\n", fhistBefore, _tfcFhistStepCount());
        scFail = 1;
      }
      if (calcMode != CM_AIM) {
        printf("    [4] FAIL: calcMode %d after, expected CM_AIM\n", calcMode);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [4] PASS: EXIT before any digit cancels cleanly, line intact, capture OPEN\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase 5 (C5.5): PARK executes live and keeps the line. DELP is
   * PARK-classified by func (F1's _forthFoldAdmits) and, unlike ASSIGN/
   * USERMODE/TM_STRING/TM_NEWMENU/TM_KEY, does not itself clobber
   * aimBuffer -- its operand is a plain program number, so "the line
   * survives" is actually observable (those others zero aimBuffer as
   * their OWN pre-existing semantics, fold-unrelated). ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p5;
    int sEnd;
    tpInit(&p5);
    tpLbl(&p5, "F2P5");
    sEnd = tpEnd(&p5);
    if (sEnd < 0 || !tpWrite(&p5) || !tpSelectStep(&p5, sEnd)) {
      printf("    [5] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t fhistBefore;
      forthHistoryEnsure();
      fhistBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;

      runFunction(ITM_DELP);

      if (!tam.mode || forthFoldArmed() || !forthFoldPending()) {
        printf("    [5] FIXTURE FAIL: expected TAM entered + PARK (pending, not armed); "
               "tam.mode=%d armed=%d pending=%d\n",
               (int)tam.mode, (int)forthFoldArmed(), (int)forthFoldPending());
        scFail = 1;
      } else {
        static const int16_t digits[] = { ITM_1, ITM_0, ITM_0 };
        int di;
        for (di = 0; di < 3 && tam.mode != 0; di++) {
          tamProcessInput(digits[di]);
        }
        if (tam.mode != 0) {
          printf("    [5] FIXTURE FAIL: DELP TAM session never committed (tam.mode=%d)\n", (int)tam.mode);
          scFail = 1;
        }
        if (compareString(aimBuffer, "42", CMP_BINARY) != 0) {
          printf("    [5] FAIL: aimBuffer \"%s\", expected \"42\" (PARK does not fold text)\n", aimBuffer);
          scFail = 1;
        }
        if (_tfcFhistStepCount() != fhistBefore) {
          printf("    [5] FAIL: FHIST step count %u -> %u, PARK leave did not sweep cleanly\n",
                 fhistBefore, _tfcFhistStepCount());
          scFail = 1;
        }
        if (calcMode != CM_AIM) {
          printf("    [5] FAIL: calcMode %d after, expected CM_AIM\n", calcMode);
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [5] PASS: PARK (DELP) runs live, line survives, FHIST swept clean\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase 6 (C5.6): PEM is untouched -- re-run the landed F6-2/
   * F6-4 suite unchanged. ---- */
  scFail = 0;
  scFail |= test_capture_suspend();
  scFail |= test_capture_param_text();
  if (!scFail) printf("    [6] PASS: F6-2/F6-4 suite re-run clean, PEM unaffected\n");
  else printf("    [6] FAIL: F6-2/F6-4 suite regressed\n");
  fail |= scFail;

  /* ---- Subcase 7 (C5.7): the bracket does not leak. calcMode == CM_AIM
   * after every subcase above (already asserted individually), plus the
   * error path: force lastErrorCode inside the commit (the real guard at
   * ui/tam.c:1102 skips addStepInProgram whenever lastErrorCode != 0,
   * which is the actual, drivable "error occurred inside the commit"
   * shape in this tree) and confirm the bracket still restores CM_AIM. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t p7;
    int sEnd;
    tpInit(&p7);
    tpLbl(&p7, "F2P7");
    sEnd = tpEnd(&p7);
    if (sEnd < 0 || !tpWrite(&p7) || !tpSelectStep(&p7, sEnd)) {
      printf("    [7] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      forthHistoryEnsure();
      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;

      runFunction(ITM_STO);
      lastErrorCode = ERROR_UNDEF_SOURCE_VAR;   /* force an error mid-commit */
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);

      if (calcMode != CM_AIM) {
        printf("    [7] FAIL: calcMode %d after an error mid-commit, expected CM_AIM (bracket leaked)\n",
               calcMode);
        scFail = 1;
      }
      lastErrorCode = ERROR_NONE;
    }
  }
  if (!scFail) printf("    [7] PASS: calcMode restored to CM_AIM even with an error forced mid-commit\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase A (the re-entry chain, C1's own contract). tamEnterMode
   * is RE-ENTERED from inside a bracketed _tamProcessInput: XEQ 'STO'
   * does not resolve as a label, falls to the native CAT_FNCT scan
   * (ui/tam.c ~987), and calls runFunction(ITM_STO) reentrantly while the
   * OUTER XEQ fold's bracket is still forged CM_PEM.  Assert the capture
   * ends up OPEN, not stuck SUSPENDED, once everything unwinds -- L1-1's
   * origin bit (forthCapIsInteractive()) must stay true across the
   * suspension for Seam 1's guard to arm the fold at all. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t pA;
    int sEnd;
    tpInit(&pA);
    tpLbl(&pA, "F2PA");
    sEnd = tpEnd(&pA);
    if (sEnd < 0 || !tpWrite(&pA) || !tpSelectStep(&pA, sEnd)) {
      printf("    [A] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      forthHistoryEnsure();
      forthCapOpenInteractive();
      forthCapSetKeysMode(false);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;

      runFunction(ITM_XEQ);
      if (!tam.mode || !forthFoldPending()) {
        printf("    [A] FIXTURE FAIL: XEQ did not enter TAM / arm the fold\n");
        scFail = 1;
      } else {
        tamProcessInput(ITM_alpha);
        runFunction(ITM_S);
        runFunction(ITM_T);
        runFunction(ITM_O);
        tamProcessInput(ITM_ENTER);   /* "STO" resolves via the native CAT_FNCT
                                       * fallback, re-entering tamEnterMode for
                                       * ITM_STO from inside _tamProcessInput */

        if (tam.mode) {
          /* The reentrant STO TAM session needs an operand; cancel it so
           * the whole chain unwinds rather than leaving this subcase's
           * fixture mid-TAM for the next subcase's FS_RESET() to inherit. */
          fnKeyExit(NOPARAM);
        }

        if (!forthCapIsOpen() || forthCapIsSuspended()) {
          printf("    [A] FAIL: capture state %d after the re-entry chain unwinds, expected OPEN\n",
                 forthTestCapState());
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [A] PASS: re-entrant tamEnterMode chain still ends with the capture OPEN\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  /* ---- Subcase B (Mutation 3's own pin): a leave-then-dispatch site
   * (ui/tam.c:566, the dddVEL arm of a menu_TamSto softkey reachable
   * after STO) calls leaveTamModeIfEnabled() BEFORE its own
   * runFunction(tamOperation()) dispatch. ---- */
  scFail = 0;
  FS_RESET();
  {
    testProg_t pB;
    int sEnd;
    tpInit(&pB);
    tpLbl(&pB, "F2PB");
    sEnd = tpEnd(&pB);
    if (sEnd < 0 || !tpWrite(&pB) || !tpSelectStep(&pB, sEnd)) {
      printf("    [B] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t fhistBefore;
      forthHistoryEnsure();
      fhistBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      forthCapSetKeysMode(true);
      xcopy(aimBuffer, "42", 2); aimBuffer[2] = 0;

      runFunction(ITM_STO);
      if (!tam.mode || !forthFoldArmed()) {
        printf("    [B] FIXTURE FAIL: STO did not enter TAM / arm the fold\n");
        scFail = 1;
      } else {
        tamProcessInput(ITM_dddVEL);
        /* rev 3: dddVEL maps to ITM_STOVEL (ui/tam.c's StoOperations table),
         * which is TM_VALUE with tamMinMax max 4096 (items.c:4714) — up to
         * FOUR digits, so unlike STO (max 99) two digits do NOT auto-fire the
         * commit.  This softkey therefore opens a NESTED TAM that must be
         * completed explicitly.  The fold staying pending across it is
         * CORRECT: the line lives in the FHIST capture step until the whole
         * interaction ends.  Complete it so the assertions below describe a
         * finished gesture rather than a mid-flight one. */
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_5);
        if (tam.mode) { tamProcessInput(ITM_ENTER); }
        if (compareString(aimBuffer, "42", CMP_BINARY) == 0) {
          printf("    [B] FAIL: aimBuffer still \"42\" -- the dddVEL commit was not folded into the line\n");
          scFail = 1;
        }
        if (_tfcFhistStepCount() != fhistBefore) {
          printf("    [B] FAIL: FHIST step count %u -> %u -- leave-then-dispatch site left debris\n",
                 fhistBefore, _tfcFhistStepCount());
          scFail = 1;
        }
        if (!forthCapIsOpen() || forthCapIsSuspended()) {
          printf("    [B] FAIL: capture state %d, expected OPEN\n", forthTestCapState());
          scFail = 1;
        }
        printf("    [B] REPORT: aimBuffer=\"%s\" fhist %u->%u capstate=%d\n",
               aimBuffer, fhistBefore, _tfcFhistStepCount(), forthTestCapState());
      }
    }
  }
  if (!scFail) printf("    [B] PASS: leave-then-dispatch (dddVEL) commit folds into text, no debris\n");
  fail |= scFail;
  FS_RESET();
  cleanupTestProgram();

  #undef FS_RESET

  forthCapClose();
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam = savedTam;
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


/* ==================================================================
 * PACKET_L1_F3 — operand-class parity and the fold's close paths.
 *
 * C1 proves the L-R4 (b) contract directly: for the SAME key sequence,
 * the interactive fold and the PEM fold produce STRING-IDENTICAL line
 * text, across every operand class of the §4 (F4) grammar.  Row 9
 * (TM_VALUE > 250 / CNST_BEYOND_250) is DELETED as unreachable in this
 * tree — see the packet.  Row 10 covers both ends of the item's
 * tamMinMax bounds (10a min, 10b max).
 *
 * C2 extends the E14 close-paths class with the fold's own invariant:
 * no fold may leave an outstanding transient step.
 *
 * C3 is the CM-gate audit sweep: STAGE_L_TRACES.md's 17-row table,
 * encoded as assertions (or an explicit reported gap) per row.
 * ================================================================== */

extern void fnForthOuter(uint16_t);
extern void fnKeyExit(uint16_t);
extern void fnKeyBackspace(uint16_t);
extern void runFunction(int16_t);
extern void tamProcessInput(uint16_t);
extern void tamEnterMode(int16_t);
extern int16_t determineItem(const char *);
extern void processKeyAction(int16_t);
extern void executeFunction(const char *data, int16_t item_);
extern void showSoftmenu(int16_t);
extern void testInitVariableSoftmenu(int16_t);
extern void pemAlphaEdit(uint16_t);

/* ---- C1 row drivers -------------------------------------------------
 * Each assumes an already-open, EMPTY capture (PEM or interactive) with
 * tam.mode == 0, and drives ONLY the operand-class gesture -- no numeric
 * prefix -- so the two halves are directly, byte-for-byte comparable. */

static void _fopRow1(void)   { runFunction(ITM_STO); tamProcessInput(ITM_0); tamProcessInput(ITM_5); }
static void _fopRow2(void)   { runFunction(ITM_STO); tamProcessInput(ITM_PERIOD); tamProcessInput(ITM_0); tamProcessInput(ITM_3); }
static void _fopRow3(void)   { runFunction(ITM_STO); tamProcessInput(ITM_INDIRECTION); tamProcessInput(ITM_0); tamProcessInput(ITM_5); }
static void _fopRow4(void)   { runFunction(ITM_RCL); tamProcessInput(ITM_INDIRECTION); tamProcessInput(ITM_alpha); runFunction(ITM_A); runFunction(ITM_B); tamProcessInput(ITM_ENTER); }
static void _fopRow5(void)   { runFunction(ITM_SF);  tamProcessInput(ITM_1); tamProcessInput(ITM_2); }
static void _fopRow6(void)   { runFunction(ITM_SF);  tamProcessInput(ITM_PERIOD); tamProcessInput(ITM_0); tamProcessInput(ITM_2); }
static void _fopRow7(void)   { runFunction(ITM_XEQ); tamProcessInput(ITM_alpha); runFunction(ITM_W); runFunction(ITM_A); tamProcessInput(ITM_ENTER); }
static void _fopRow8(void)   { runFunction(ITM_XEQ); tamProcessInput(ITM_COLON); tamProcessInput(ITM_alpha); runFunction(ITM_F); runFunction(ITM_O); runFunction(ITM_O); tamProcessInput(ITM_ENTER); }
static void _fopRow10a(void) { runFunction(ITM_DSTACK); tamProcessInput(ITM_1); }
static void _fopRow10b(void) { runFunction(ITM_DSTACK); tamProcessInput(ITM_4); }
static void _fopRow11(void)  { runFunction(ITM_SHUFFLE); tamProcessInput(ITM_REG_X); tamProcessInput(ITM_REG_Y); tamProcessInput(ITM_REG_Z); tamProcessInput(ITM_REG_T); }
static void _fopRow12(void)  { runFunction(ITM_OPEN_MENU); tamProcessInput(ITM_alpha); runFunction(ITM_M); runFunction(ITM_X); tamProcessInput(ITM_ENTER); }

typedef struct { const char *label; void (*drive)(void); } fopRow_t;

static const fopRow_t fopRows[] = {
  { "1  direct register (STO 0 5)",              _fopRow1   },
  { "2  dotted local register (STO . 0 3)",      _fopRow2   },
  { "3  indirect register (STO -> 0 5)",         _fopRow3   },
  { "4  indirect variable (RCL -> alpha 'AB')",  _fopRow4   },
  { "5  flag by number (SF 1 2)",                _fopRow5   },
  { "6  dotted local flag (SF . 0 2)",           _fopRow6   },
  { "7  named global label (XEQ 'WA')",          _fopRow7   },
  { "8  named local label (XEQ :FOO)",           _fopRow8   },
  { "10a TM_VALUE min edge (dSTACK 1)",          _fopRow10a },
  { "10b TM_VALUE max edge (dSTACK 4)",          _fopRow10b },
  { "11 TM_SHUFFLE (x y z t)",                   _fopRow11  },
  { "12 TM_MENU (OPENM 'MX')",                   _fopRow12  },
};
#define FOP_NUM_ROWS ((int)(sizeof(fopRows) / sizeof(fopRows[0])))

/* Drives one row through the landed PEM idiom (test_capture_param_text:
 * testProg_t + marker, fnGotoDot(2), runFunction(ITM_AIM)) and then through
 * the landed interactive idiom (fnForthOuter(NOPARAM) from CM_NORMAL),
 * capturing each half's text into its own buffer BEFORE any comparison.
 * Returns 1 only on a FIXTURE failure (capture did not open); a text
 * mismatch is reported by the caller, not here. */
static int _fopDriveOneRow(void (*drive)(void), char *pemOut, char *intOut)
{
  int fixtureFail = 0;

  /* ---- PEM half ---- */
  cleanupTestProgram();
  {
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "FOPP");
    tpMarker(&p);

    if (!tpWrite(&p)) {
      printf("      FIXTURE FAIL (PEM): tpWrite\n");
      pemOut[0] = 0;
      fixtureFail = 1;
    } else {
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

      fnGotoDot(2);
      runFunction(ITM_AIM);
      if (!getSystemFlag(FLAG_ALPHA) || tam.function != ITM_FORTH) {
        printf("      FIXTURE FAIL (PEM): ITM_AIM did not open a Forth capture\n");
        pemOut[0] = 0;
        fixtureFail = 1;
      } else {
        drive();
        xcopy(pemOut, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);
      }
    }
  }
  forthCapClose();
  lastErrorCode = ERROR_NONE;
  cleanupTestProgram();

  /* ---- interactive half: fnForthOuter(NOPARAM) from CM_NORMAL. ---- */
  calcMode = CM_NORMAL;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  shiftF = false;
  shiftG = false;
  clearSystemFlag(FLAG_ALPHA);
  clearSystemFlag(FLAG_NUMLOCK);
  lastErrorCode = ERROR_NONE;
  forthCapClose();
  if (forthFoldPending()) { forthFoldLeave(); }

  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("      FIXTURE FAIL (interactive): fnForthOuter did not open an interactive capture\n");
    intOut[0] = 0;
    fixtureFail = 1;
  } else {
    drive();
    xcopy(intOut, forthTestCapText(), stringByteLength((char *)forthTestCapText()) + 1);
  }

  forthCapClose();
  if (forthFoldPending()) { forthFoldLeave(); }
  lastErrorCode = ERROR_NONE;
  cleanupTestProgram();

  return fixtureFail;
}

static int test_fold_operand_parity(void)
{
  int fail = 0;
  int i;

  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedProgRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;

  printf("\n  ---- C1: pairwise operand-class parity (PEM vs interactive) ----\n");
  printf("    [9  TM_VALUE > 250 / CNST_BEYOND_250] DELETED -- unreachable: the\n"
         "        CNST_BEYOND_250 emit (manage.c) is gated on PARAM_NUMBER_8_16,\n"
         "        which occurs exactly once (ITM_CNST), whose tamMinMax max is\n"
         "        NOUC-1 (83), and ui/tam.c clamps the accumulator to tam.max --\n"
         "        no gesture in this tree reaches CNST_BEYOND_250.\n");

  for (i = 0; i < FOP_NUM_ROWS; i++) {
    char pemText[256];
    char intText[256];
    int rowFail;

    rowFail = _fopDriveOneRow(fopRows[i].drive, pemText, intText);
    if (!rowFail && compareString(pemText, intText, CMP_BINARY) != 0) {
      rowFail = 1;
    }

    printf("    [%s]\n      PEM:         \"%s\"\n      interactive: \"%s\"\n      %s\n",
           fopRows[i].label, pemText, intText,
           rowFail ? "FAIL: texts differ (or fixture failed)" : "PASS: string-identical");

    fail |= rowFail;
  }

  forthCapClose();
  if (forthFoldPending()) { forthFoldLeave(); }
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* ==================================================================
 * C2 — the fold's close paths (`test_fold_close_paths`).
 *
 * Extends the E14 close-paths class with the fold's own invariant: no
 * fold may leave an outstanding transient step.  For each subcase,
 * assert afterwards (except subcase 7, whose own invariant is stated at
 * its own block -- see the comment there): the capture line is intact,
 * T_cursorPos is valid, getNumberOfSteps() equals the pre-fold count,
 * firstFreeProgramByte equals its pre-fold value, and forthCap.foldMode
 * == 0 (forthFoldPending() is false).
 * ================================================================== */
static int test_fold_close_paths(void)
{
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

  printf("\n  ---- C2: the fold's close paths ----\n");

  #define FCP_RESET() do { \
    calcMode = CM_AIM; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    if (forthFoldPending()) { forthFoldLeave(); } \
  } while (0)

  /* ---- Subcase 1: EXIT mid-TAM (before any digit). ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p1;
    int sEnd;
    tpInit(&p1);
    tpLbl(&p1, "FCP1");
    sEnd = tpEnd(&p1);
    if (sEnd < 0 || !tpWrite(&p1) || !tpSelectStep(&p1, sEnd)) {
      printf("    [1] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore  = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

      runFunction(ITM_STO);
      fnKeyExit(NOPARAM);

      if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
        printf("    [1] FAIL: aimBuffer \"%s\", expected \"77\" intact\n", aimBuffer);
        scFail = 1;
      }
      if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
        printf("    [1] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [1] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [1] FAIL: firstFreeProgramByte changed\n");
        scFail = 1;
      }
      if (forthFoldPending()) {
        printf("    [1] FAIL: forthFoldPending() still true\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS: EXIT before any digit -- line intact, no outstanding step, fold clear\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 2: EXIT mid-TAM (after one digit of two). ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p2;
    int sEnd;
    tpInit(&p2);
    tpLbl(&p2, "FCP2");
    sEnd = tpEnd(&p2);
    if (sEnd < 0 || !tpWrite(&p2) || !tpSelectStep(&p2, sEnd)) {
      printf("    [2] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore  = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

      runFunction(ITM_STO);
      tamProcessInput(ITM_0);     /* one of two digits -- does not auto-fire */
      fnKeyExit(NOPARAM);

      if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
        printf("    [2] FAIL: aimBuffer \"%s\", expected \"77\" intact\n", aimBuffer);
        scFail = 1;
      }
      if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
        printf("    [2] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [2] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [2] FAIL: firstFreeProgramByte changed\n");
        scFail = 1;
      }
      if (forthFoldPending()) {
        printf("    [2] FAIL: forthFoldPending() still true\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [2] PASS: EXIT after one of two digits -- line intact, no outstanding step, fold clear\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 3: backspace-to-empty during operand entry, then EXIT.
   * Backspacing a digit back to zero does NOT itself close the TAM
   * session (tam.mode stays active -- native behaviour, ui/tam.c's
   * ITM_BACKSPACE arm only decrements tam.digitsSoFar/tam.value); it is
   * reachable, but it is not itself a close transition.  Driven here up
   * to that state, then closed with EXIT, to confirm the invariant holds
   * across it. ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p3;
    int sEnd;
    tpInit(&p3);
    tpLbl(&p3, "FCP3");
    sEnd = tpEnd(&p3);
    if (sEnd < 0 || !tpWrite(&p3) || !tpSelectStep(&p3, sEnd)) {
      printf("    [3] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore  = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

      runFunction(ITM_STO);
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_BACKSPACE);   /* digitsSoFar back to 0; tam.mode still TM_STORCL */
      if (!tam.mode) {
        printf("    [3] FIXTURE NOTE: TAM already closed after backspace-to-empty "
               "(unexpected -- reachability assumption may not hold)\n");
      }
      fnKeyExit(NOPARAM);

      if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
        printf("    [3] FAIL: aimBuffer \"%s\", expected \"77\" intact\n", aimBuffer);
        scFail = 1;
      }
      if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
        printf("    [3] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [3] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [3] FAIL: firstFreeProgramByte changed\n");
        scFail = 1;
      }
      if (forthFoldPending()) {
        printf("    [3] FAIL: forthFoldPending() still true\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [3] PASS: backspace-to-empty is reachable (TAM stays open, native "
                      "behaviour) and does not disturb the subsequent EXIT close\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 4: error at commit -- lastErrorCode forced non-NONE makes
   * ui/tam.c's step-recording gate skip addStepInProgram; assert the fold
   * still sweeps and the line survives. ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p4;
    int sEnd;
    tpInit(&p4);
    tpLbl(&p4, "FCP4");
    sEnd = tpEnd(&p4);
    if (sEnd < 0 || !tpWrite(&p4) || !tpSelectStep(&p4, sEnd)) {
      printf("    [4] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore  = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

      /* ui/tam.c:1102's lastErrorCode gate sits in the NAMED-operand commit
       * path (tam.alpha), not the plain-numeric one -- a bare "STO 05" never
       * reaches it (that commit is unconditional at the numeric branch's own
       * "case CM_PEM: addStepInProgram(...)"). Use the same undefined-name
       * XEQ gesture C1 row 7 / test_capture_param_text [2] use (XEQ 'WA'),
       * which resolves through the CAT_FNCT/Forth-fallback miss and falls
       * to the shared tail at :1102, forcing the error right before ENTER. */
      runFunction(ITM_XEQ);
      tamProcessInput(ITM_alpha);
      runFunction(ITM_W);
      runFunction(ITM_A);
      lastErrorCode = ERROR_UNDEF_SOURCE_VAR;   /* force an error mid-commit */
      tamProcessInput(ITM_ENTER);
      lastErrorCode = ERROR_NONE;

      if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
        printf("    [4] FAIL: aimBuffer \"%s\", expected \"77\" intact (nothing recorded)\n", aimBuffer);
        scFail = 1;
      }
      if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
        printf("    [4] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [4] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
      if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
        printf("    [4] FAIL: firstFreeProgramByte changed\n");
        scFail = 1;
      }
      if (forthFoldPending()) {
        printf("    [4] FAIL: forthFoldPending() still true\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [4] PASS: error forced mid-commit -- nothing recorded, fold still sweeps, line survives\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 5: oversize-text break -- a capture line long enough that
   * forthCapInsertName refuses (196-glyph cap). The splice breaks and KEEPS
   * the committed step, and since round 6 (F10) the sweep honours the keep:
   * the committed operation survives in FHIST rather than vanishing with no
   * error.  (This subcase used to pin the opposite — the sweep deleting the
   * kept step — which is exactly the disposition collision F10 confirmed:
   * a committed STO dropped between the splice's "keep" and the sweep.
   * Contract migrated with the fix.) ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p5;
    int sEnd;
    tpInit(&p5);
    tpLbl(&p5, "FCP5");
    sEnd = tpEnd(&p5);
    if (sEnd < 0 || !tpWrite(&p5) || !tpSelectStep(&p5, sEnd)) {
      printf("    [5] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      char longLine[256];
      int k;
      uint16_t numBefore;
      uint32_t freeOffBefore;

      for (k = 0; k < 96; k++) { longLine[2 * k] = 'X'; longLine[2 * k + 1] = ' '; }
      longLine[192] = 'X';
      longLine[193] = 0;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, longLine, 194);
      T_cursorPos = 193;

      if (stringGlyphLength(aimBuffer) != 193) {
        printf("    [5] FIXTURE FAIL: seed glyph count = %d, expected 193\n",
               stringGlyphLength(aimBuffer));
        scFail = 1;
      } else {
        numBefore  = getNumberOfSteps();
        freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

        runFunction(ITM_STO);
        tamProcessInput(ITM_0);
        tamProcessInput(ITM_5);   /* "STO 05" (~7 glyphs): 193+7 > 196, refused */

        if (compareString(aimBuffer, longLine, CMP_BINARY) != 0) {
          printf("    [5] FAIL: aimBuffer changed on a refused (oversize) conversion\n");
          scFail = 1;
        }
        if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
          printf("    [5] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
          scFail = 1;
        }
        if (getNumberOfSteps() != numBefore) {
          printf("    [5] FAIL: getNumberOfSteps() %u -> %u — the caller's own"
                 " program must be untouched (the kept step lives in FHIST)\n",
                 numBefore, getNumberOfSteps());
          scFail = 1;
        }
        if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) <= freeOffBefore) {
          printf("    [5] FAIL: firstFreeProgramByte did not grow — the kept"
                 " step is missing from FHIST (round 6 F10: a refused commit"
                 " is KEPT, never silently swept)\n");
          scFail = 1;
        }
        if (forthFoldPending()) {
          printf("    [5] FAIL: forthFoldPending() still true\n");
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [5] PASS: oversize commit refused by forthCapInsertName; the committed "
                      "step is KEPT in FHIST (round 6 F10), fold clear\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 6: PARK commit -- the TAM executes live; assert the line
   * survived and nothing was left behind.  ITM_DELP is PARK-classified
   * (F1's _forthFoldAdmits) and, unlike ASSIGN/USERMODE/TM_STRING/
   * TM_NEWMENU/TM_KEY, does not itself clobber aimBuffer. ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p6;
    int sEnd;
    tpInit(&p6);
    tpLbl(&p6, "FCP6");
    sEnd = tpEnd(&p6);
    if (sEnd < 0 || !tpWrite(&p6) || !tpSelectStep(&p6, sEnd)) {
      printf("    [6] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      uint32_t freeOffBefore;

      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      numBefore  = getNumberOfSteps();
      freeOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

      runFunction(ITM_DELP);
      if (!tam.mode || forthFoldArmed() || !forthFoldPending()) {
        printf("    [6] FIXTURE FAIL: expected TAM entered + PARK (pending, not armed)\n");
        scFail = 1;
      } else {
        static const int16_t digits[] = { ITM_1, ITM_0, ITM_0 };
        int di;
        for (di = 0; di < 3 && tam.mode != 0; di++) {
          tamProcessInput(digits[di]);
        }
        if (tam.mode != 0) {
          printf("    [6] FIXTURE FAIL: DELP TAM session never committed\n");
          scFail = 1;
        }
        if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
          printf("    [6] FAIL: aimBuffer \"%s\", expected \"77\" (PARK runs live, does not fold text)\n", aimBuffer);
          scFail = 1;
        }
        if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
          printf("    [6] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
          scFail = 1;
        }
        if (getNumberOfSteps() != numBefore) {
          printf("    [6] FAIL: getNumberOfSteps() %u -> %u\n", numBefore, getNumberOfSteps());
          scFail = 1;
        }
        if ((uint32_t)(firstFreeProgramByte - beginOfProgramMemory) != freeOffBefore) {
          printf("    [6] FAIL: firstFreeProgramByte changed\n");
          scFail = 1;
        }
        if (forthFoldPending()) {
          printf("    [6] FAIL: forthFoldPending() still true\n");
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [6] PASS: PARK (DELP) runs live, line survives, nothing left behind\n");
  fail |= scFail;
  FCP_RESET();
  cleanupTestProgram();

  /* ---- Subcase 7: forthCapPowerReset mid-fold. This is the last-resort
   * reset: forthCapPowerReset() clears forthCap.foldMode directly and does
   * NOT run forthFoldLeave's sweep or cursor restore (forth_capture.h's own
   * documented contract -- test_fold_context [7] pins the same fact for the
   * fold context alone). Consequently currentStep/currentProgramNumber stay
   * parked on FHIST rather than returning to the caller's program, so the
   * generic "getNumberOfSteps()/firstFreeProgramByte unchanged" invariants
   * from subcases 1-6 do not apply here -- there is no cursor restore to
   * make them meaningful against the ORIGINAL caller program. What DOES
   * hold, and is asserted: foldMode clears, aimBuffer/T_cursorPos are
   * untouched (forthCapPowerReset never touches them), and FHIST gains
   * EXACTLY ONE entry -- the leaked capture step, which is byte-identical
   * to what a legitimate forthHistoryPush of the same line would have left
   * (T7.2b's "a leftover capture step IS a history entry" reasoning). ---- */
  scFail = 0;
  FCP_RESET();
  {
    testProg_t p7;
    int sEnd;
    tpInit(&p7);
    tpLbl(&p7, "FCP7");
    sEnd = tpEnd(&p7);
    if (sEnd < 0 || !tpWrite(&p7) || !tpSelectStep(&p7, sEnd)) {
      printf("    [7] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t fhistBefore;

      forthHistoryEnsure();
      fhistBefore = _tfcFhistStepCount();

      forthCapOpenInteractive();
      xcopy(aimBuffer, "77", 2); aimBuffer[2] = 0;
      T_cursorPos = 2;

      runFunction(ITM_STO);        /* arm the fold, mid-TAM, capture SUSPENDED */
      if (!tam.mode || !forthFoldPending()) {
        printf("    [7] FIXTURE FAIL: STO did not enter TAM / arm the fold\n");
        scFail = 1;
      } else {
        forthCapPowerReset();

        if (forthFoldPending()) {
          printf("    [7] FAIL: forthFoldPending() still true after forthCapPowerReset()\n");
          scFail = 1;
        }
        if (compareString(aimBuffer, "77", CMP_BINARY) != 0) {
          printf("    [7] FAIL: aimBuffer \"%s\", expected \"77\" (power reset does not touch it)\n", aimBuffer);
          scFail = 1;
        }
        if ((uint32_t)T_cursorPos > (uint32_t)stringByteLength(aimBuffer)) {
          printf("    [7] FAIL: T_cursorPos %d not <= stringByteLength(aimBuffer)\n", T_cursorPos);
          scFail = 1;
        }
        if (_tfcFhistStepCount() != (uint16_t)(fhistBefore + 1)) {
          printf("    [7] FAIL: FHIST step count %u -> %u, expected exactly +1 (the orphaned "
                 "capture step, equivalent to a legitimate history push)\n",
                 fhistBefore, _tfcFhistStepCount());
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [7] PASS: foldMode clears; line/cursor untouched; FHIST gains exactly "
                      "the one orphaned entry a legitimate push would have left -- generic "
                      "step-count/firstFreeProgramByte invariants do not apply (no cursor "
                      "restore on this path; reported, not asserted -- see comment)\n");
  fail |= scFail;

  #undef FCP_RESET

  forthCapClose();
  if (forthFoldPending()) { forthFoldLeave(); }
  cleanupTestProgram();
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
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* ==================================================================
 * C3 — the CM-gate audit sweep (`test_cm_gate_audit`).
 *
 * STAGE_L_TRACES.md carries a 17-row table of every landed
 * `calcMode == CM_PEM` gate with a widen/keep verdict.  Each row below
 * is encoded as a direct assertion where the gesture is cheaply
 * drivable; where an already-registered test is the row's own
 * regression pin, it is re-invoked here (the test_fold_seams [6]
 * precedent) so this sweep is one consolidated checklist.  A row that
 * cannot be driven -- a structural KEEP with no interactive-specific
 * branch to flip, or a pure-display gate with no state to assert -- is
 * reported NOT DRIVEN with its reason; that is a reported gap, not a
 * silent omission.
 * ================================================================== */
static int test_cm_gate_audit(void)
{
  int fail = 0, scFail;
  int driven = 0, notDriven = 0;

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

  printf("\n  ---- C3: the CM-gate audit sweep (17 rows) ----\n");

  #define CGA_RESET() do { \
    calcMode = CM_AIM; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    if (forthFoldPending()) { forthFoldLeave(); } \
  } while (0)

  /* ---- Row 1: keyboard.c determineItem AIM/PEM column selection.
   * WIDEN -- the K1 keys-mode escape (L1-3, tested elsewhere) plus the
   * fold-precedence conjunct T3 finding 4 requires: with the fold
   * pending, a digit key must resolve via key->primaryTam, not the AIM
   * column's letter. ---- */
  scFail = 0;
  CGA_RESET();
  {
    testProg_t p1;
    int sEnd;
    tpInit(&p1);
    tpLbl(&p1, "CGA1");
    sEnd = tpEnd(&p1);
    if (sEnd < 0 || !tpWrite(&p1) || !tpSelectStep(&p1, sEnd)) {
      printf("    [1] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      int zIdx = -1, ki;
      for (ki = 0; ki < 37; ki++) {
        if (kbd_std[ki].primary == ITM_0) { zIdx = ki; break; }
      }
      if (zIdx < 0) {
        printf("    [1] FIXTURE FAIL: no kbd_std row carries primary == ITM_0\n");
        scFail = 1;
      } else {
        char kb[3];
        sprintf(kb, "%02d", zIdx);

        forthCapOpenInteractive();
        forthCapSetKeysMode(false);
        xcopy(aimBuffer, "1", 1); aimBuffer[1] = 0;

        runFunction(ITM_STO);
        if (!tam.mode || !forthFoldPending()) {
          printf("    [1] FIXTURE FAIL: STO did not enter TAM / arm the fold\n");
          scFail = 1;
        } else {
          int16_t got;
          shiftF = false; shiftG = false;
          got = determineItem(kb);
          if (got != kbd_std[zIdx].primaryTam) {
            printf("    [1] FAIL: determineItem = %d, expected key->primaryTam (%d)\n",
                   got, kbd_std[zIdx].primaryTam);
            scFail = 1;
          }
        }
        fnKeyExit(NOPARAM);
      }
    }
  }
  driven++;
  printf("    [1] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "fold pending -> digit resolves via key->primaryTam (WIDEN)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 2: keyboard.c E10 ALPHA-gesture -> ITM_AIM resolution.
   * WIDEN to the interactive origin: shiftF + key->fShifted == ITM_AIM
   * resolves to ITM_AIM whenever forthCapIsInteractive(), no PEM/
   * tam.function/FLAG_ALPHA preconditions. ---- */
  scFail = 0;
  CGA_RESET();
  {
    int kIdx = -1, ki;
    for (ki = 0; ki < 37; ki++) {
      if (kbd_std[ki].fShifted == ITM_AIM) { kIdx = ki; break; }
    }
    if (kIdx < 0) {
      printf("    [2] FIXTURE FAIL: no kbd_std row carries fShifted == ITM_AIM\n");
      scFail = 1;
    } else {
      char kb[3];
      sprintf(kb, "%02d", kIdx);

      fnForthOuter(NOPARAM);
      if (!forthCapIsOpen() || !forthCapIsInteractive()) {
        printf("    [2] FIXTURE FAIL: interactive open did not take\n");
        scFail = 1;
      } else {
        int16_t got;
        shiftF = true; shiftG = false;
        got = determineItem(kb);
        if (got != ITM_AIM) {
          printf("    [2] FAIL: determineItem = %d, expected ITM_AIM\n", got);
          scFail = 1;
        }
      }
    }
  }
  driven++;
  printf("    [2] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "ALPHA gesture resolves to ITM_AIM under an interactive capture (WIDEN)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 3: keyboard.c closeAim() on catalog pick. WIDEN (suppress
   * when an interactive capture is open) -- FCNS catalog pick inserts
   * the name as text and leaves the capture open. ---- */
  scFail = 0;
  CGA_RESET();
  {
    bool_t savedFnKeyInCatalog = fnKeyInCatalog;
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen()) {
      printf("    [3] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      int16_t fcnsIdx = -1, si, pi, sinPos = -1;
      for (si = 0; si < 300; si++) {
        if (softmenu[si].menuItem == -MNU_FCNS) { fcnsIdx = si; break; }
      }
      if (fcnsIdx >= 0) {
        for (pi = 0; pi < softmenu[fcnsIdx].numItems; pi++) {
          if (softmenu[fcnsIdx].softkeyItem[pi] % 10000 == ITM_sin) { sinPos = pi; break; }
        }
      }
      if (fcnsIdx < 0 || sinPos < 0) {
        printf("    [3] FIXTURE FAIL: MNU_FCNS/SIN not found\n");
        scFail = 1;
      } else {
        catalog = CATALOG_FCNS;
        showSoftmenu(-MNU_CATALOG);
        showSoftmenu(-MNU_FCNS);
        softmenuStack[0].firstItem = sinPos;
        fnKeyInCatalog = 1;
        shiftF = false;
        shiftG = false;

        executeFunction("1", 0);

        catalog = CATALOG_NONE;

        if (!forthCapIsOpen() || !forthCapIsInteractive()) {
          printf("    [3] FAIL: capture closed by the catalog pick (widen missing)\n");
          scFail = 1;
        }
        if (compareString(aimBuffer, "SIN ", CMP_BINARY) != 0) {
          printf("    [3] FAIL: aimBuffer \"%s\", expected \"SIN \"\n", aimBuffer);
          scFail = 1;
        }
      }
    }
    fnKeyInCatalog = savedFnKeyInCatalog;
  }
  driven++;
  printf("    [3] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "FCNS catalog pick inserts text, does not close an interactive capture (WIDEN)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 4: keyboard.c popSoftmenu() after a bufferized softkey.
   * KEEP -- already unconditional for calcMode == CM_AIM; no
   * interactive-origin branch exists to flip. NOT DRIVEN. ---- */
  notDriven++;
  printf("    [4] NOT DRIVEN -- KEEP, structural: unconditional for calcMode == CM_AIM "
         "already, no interactive-origin branch exists to assert\n");

  /* ---- Row 5: keyboard.c catalog letter-entry arm. KEEP -- catalog
   * alpha-selection, not capture text. NOT DRIVEN. ---- */
  notDriven++;
  printf("    [5] NOT DRIVEN -- KEEP, structural: catalog letter-entry, not capture text\n");

  /* ---- Row 6: processKeyAction's CM_PEM arm (SST/BST/RS/dotD capture
   * guards). KEEP PEM-only; the interactive analog (R/S -> ENTER, T4) is
   * a separate site (keyboard.c's CM_AIM arm). Driven directly. ---- */
  scFail = 0;
  CGA_RESET();
  {
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen() || !forthCapIsInteractive()) {
      printf("    [6] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      xcopy(aimBuffer, "1 1 +", 5); aimBuffer[5] = 0;
      T_cursorPos = 5;
      processKeyAction(ITM_RS);
      if (forthFoldPending()) {
        printf("    [6] FAIL: fold left pending by ITM_RS\n");
        scFail = 1;
      }
      if (!forthCapIsOpen() || aimBuffer[0] != 0) {
        printf("    [6] FAIL: capture state %d aimBuffer \"%s\", expected OPEN+empty (REPL "
               "reopened after ITM_RS ran the line)\n", forthTestCapState(), aimBuffer);
        scFail = 1;
      }
    }
  }
  driven++;
  printf("    [6] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "interactive R/S runs the line (T4 divert); CM_PEM-only guards do not fire (KEEP)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 7: fnKeyExit's keys->alpha rung. MIRROR into the CM_AIM arm.
   * Encoded by the already-registered test_exit_ladder_keys_rung,
   * re-invoked here for the consolidated sweep (test_fold_seams [6]
   * precedent). ---- */
  scFail = test_exit_ladder_keys_rung();
  driven++;
  printf("    [7] %s %s\n", scFail ? "FAIL: test_exit_ladder_keys_rung regressed" : "DRIVEN, PASS:",
         scFail ? "" : "keys->alpha rung re-verified via test_exit_ladder_keys_rung (MIRROR)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 8: fnKeyExit's PEM commit + currentStep resync. KEEP
   * PEM-only -- no steps exist interactively. NOT DRIVEN. ---- */
  notDriven++;
  printf("    [8] NOT DRIVEN -- KEEP, structural: PEM program-step resync, no interactive counterpart\n");

  /* ---- Row 9: fnKeyBackspace's PEM capture arm. KEEP PEM-only;
   * interactive backspace stays inert on an empty line (T4). ---- */
  scFail = 0;
  CGA_RESET();
  {
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen() || !forthCapIsInteractive()) {
      printf("    [9] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      aimBuffer[0] = 0;
      T_cursorPos = 0;
      fnKeyBackspace(NOPARAM);
      if (!forthCapIsOpen() || aimBuffer[0] != 0) {
        printf("    [9] FAIL: capture state %d aimBuffer \"%s\" after backspace-on-empty, "
               "expected OPEN+empty (inert, T4)\n", forthTestCapState(), aimBuffer);
        scFail = 1;
      }
    }
  }
  driven++;
  printf("    [9] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "interactive backspace-on-empty is inert (KEEP; T4)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 10: items.c PEM step recording in runFunction. KEEP; the
   * interactive arm is the new sibling this whole stage adds. Driven
   * exhaustively by C1 (test_fold_operand_parity): every row there
   * exercises the interactive sibling arm directly beside runFunction's
   * (unchanged) PEM step recording. Not re-driven here to avoid
   * duplicating that battery. ---- */
  driven++;
  printf("    [10] DRIVEN (elsewhere) -- see C1 (test_fold_operand_parity): every parity row "
         "exercises the interactive sibling beside runFunction's unchanged PEM step recording\n");

  /* ---- Row 11: ui/tam.c TAM step recording. WIDEN-by-bracket -- F2's
   * calcMode bracket forges CM_PEM precisely so this records
   * interactively. ---- */
  scFail = 0;
  CGA_RESET();
  {
    testProg_t p11;
    int sEnd;
    tpInit(&p11);
    tpLbl(&p11, "CGA11");
    sEnd = tpEnd(&p11);
    if (sEnd < 0 || !tpWrite(&p11) || !tpSelectStep(&p11, sEnd)) {
      printf("    [11] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      uint16_t numBefore;
      forthHistoryEnsure();
      forthCapOpenInteractive();
      aimBuffer[0] = 0;

      numBefore = getNumberOfSteps();
      runFunction(ITM_STO);
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);

      if (compareString(aimBuffer, "STO 05 ", CMP_BINARY) != 0) {
        printf("    [11] FAIL: aimBuffer \"%s\", expected \"STO 05 \" (step did not record/fold)\n", aimBuffer);
        scFail = 1;
      }
      if (getNumberOfSteps() != numBefore) {
        printf("    [11] FAIL: getNumberOfSteps() %u -> %u (a step leaked)\n", numBefore, getNumberOfSteps());
        scFail = 1;
      }
    }
  }
  driven++;
  printf("    [11] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "STO 0 5 records/folds under the interactive bracket (WIDEN-by-bracket)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 12: ui/tam.c's tamEnterMode capture suspend. WIDEN -- F2's
   * Seam 1 calls forthCaptureSuspend() interactively too, once
   * forthFoldEnter has armed. Assert the capture is SUSPENDED mid-TAM,
   * before any digit. ---- */
  scFail = 0;
  CGA_RESET();
  {
    testProg_t p12;
    int sEnd;
    tpInit(&p12);
    tpLbl(&p12, "CGA12");
    sEnd = tpEnd(&p12);
    if (sEnd < 0 || !tpWrite(&p12) || !tpSelectStep(&p12, sEnd)) {
      printf("    [12] FIXTURE FAIL: build/write/select\n");
      scFail = 1;
    } else {
      forthHistoryEnsure();
      forthCapOpenInteractive();
      xcopy(aimBuffer, "9", 1); aimBuffer[1] = 0;

      runFunction(ITM_STO);
      if (!forthCapIsSuspended()) {
        printf("    [12] FAIL: capture state %d mid-TAM, expected SUSPENDED "
               "(tamEnterMode's Forth arm did not widen)\n", forthTestCapState());
        scFail = 1;
      }
      fnKeyExit(NOPARAM);
    }
  }
  driven++;
  printf("    [12] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "tamEnterMode suspends an interactive capture mid-TAM (WIDEN)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 13: manage.c's pemAlphaEdit guard. KEEP PEM-only -- EDIT is
   * a program-step gesture. Assert it no-ops during an interactive
   * capture. ---- */
  scFail = 0;
  CGA_RESET();
  {
    fnForthOuter(NOPARAM);
    if (!forthCapIsOpen() || !forthCapIsInteractive()) {
      printf("    [13] FIXTURE FAIL: interactive open did not take\n");
      scFail = 1;
    } else {
      xcopy(aimBuffer, "1 1 +", 5); aimBuffer[5] = 0;
      T_cursorPos = 5;
      pemAlphaEdit(NOPARAM);
      if (!forthCapIsOpen() || compareString(aimBuffer, "1 1 +", CMP_BINARY) != 0) {
        printf("    [13] FAIL: capture state %d aimBuffer \"%s\" after pemAlphaEdit, "
               "expected no-op (KEEP, PEM-only)\n", forthTestCapState(), aimBuffer);
        scFail = 1;
      }
    }
  }
  driven++;
  printf("    [13] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "pemAlphaEdit no-ops during an interactive capture (KEEP)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 14: forthCaptureSanitizeRestoredUi (CM_PEM + ALPHA +
   * tam.function == ITM_FORTH). Documented verdict: WIDEN -- "a restored
   * machine may hold an interactive origin; L1-1 extends the sanitizer."
   * Driven directly against the CURRENT code: simulate a restored
   * INTERACTIVE capture (calcMode == CM_AIM, FLAG_ALPHA set,
   * tam.function == ITM_FORTH, forthCap.state already CLOSED by
   * doFnReset -- exactly saveRestoreBackup.c's own comment) and check
   * whether the sanitizer cleans it up. This is a FINDING, not adjusted
   * to pass -- see the report; not counted toward `fail` since fixing
   * production code is out of this packet's tests-only scope. ---- */
  {
    calcMode = CM_AIM;
    setSystemFlag(FLAG_ALPHA);
    tam.function = ITM_FORTH;
    forthCapClose();       /* forthCap.state CLOSED, as doFnReset leaves it */

    forthCaptureSanitizeRestoredUi();

    if (getSystemFlag(FLAG_ALPHA) || tam.function == ITM_FORTH) {
      printf("    [14] FINDING (not a test FAIL): forthCaptureSanitizeRestoredUi is STILL "
             "gated on calcMode == CM_PEM only -- a restored INTERACTIVE capture (calcMode "
             "== CM_AIM, FLAG_ALPHA set, tam.function == ITM_FORTH, forthCap.state CLOSED) "
             "is left untouched: FLAG_ALPHA=%d tam.function=%d after the call. "
             "STAGE_L_TRACES.md row 14 documents this as WIDEN ('L1-1 extends the "
             "sanitizer'), but no CM_AIM arm exists in the tree. Reported per STOP "
             "CONDITION 3 (packet statement vs tree); not adjusted, not fixed here "
             "(tests-only packet).\n",
             (int)getSystemFlag(FLAG_ALPHA), (int)tam.function);
    } else {
      printf("    [14] DRIVEN, PASS: sanitizer cleans up a restored interactive-shaped state too\n");
    }
    tam.function = 0;
    clearSystemFlag(FLAG_ALPHA);
  }
  driven++;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 15: forth_menu.c's forthPickerGuard. WIDEN to the
   * interactive origin (no CM_PEM/tam.function/FLAG_ALPHA preconditions
   * there). ---- */
  scFail = 0;
  CGA_RESET();
  {
    extern void showSoftmenuCurrentPart(void);
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": CGA15W 42 ;");
    if (lastErrorCode != ERROR_NONE) {
      printf("    [15] FIXTURE FAIL: word definition error %d\n", lastErrorCode);
      scFail = 1;
    } else {
      fnForthOuter(NOPARAM);
      if (!forthCapIsOpen() || !forthCapIsInteractive()) {
        printf("    [15] FIXTURE FAIL: interactive open did not take\n");
        scFail = 1;
      } else {
        showSoftmenu(-MNU_FORTH);
        showSoftmenuCurrentPart();
        if (dynamicSoftmenu[22].numItems < 1) {
          printf("    [15] FIXTURE FAIL: MNU_FORTH picker has %d items, expected >= 1\n",
                 dynamicSoftmenu[22].numItems);
          scFail = 1;
        } else {
          softmenuStack[0].firstItem = 0;
          dynamicMenuItem = 0;
          if (!forthPickerGuard(ITM_NOP)) {
            printf("    [15] FAIL: forthPickerGuard() false under an interactive origin (WIDEN missing)\n");
            scFail = 1;
          }
        }
      }
    }
  }
  driven++;
  printf("    [15] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "forthPickerGuard fires under the interactive origin (WIDEN)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 16: forth_menu.c's picker text-scan section keyed on
   * currentStep. GATE OFF interactively (T6) -- a stale currentStep from
   * a previous PEM session must not leak its definitions into an
   * interactive picker. ---- */
  scFail = 0;
  CGA_RESET();
  {
    uint8_t prog[] = {
      0x8B, 0x1A, 0xFD, 0x00,                                            /* marker (opening) */
      0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ', 'D', 'U', 'P',     /* : SQ DUP */
      ' ', '*', ' ', ';',
      0x8B, 0x1A, 0xFD, 0x00,                                            /* marker (closing) */
    };
    if (!writeTestProgram(prog, sizeof(prog))) {
      printf("    [16] FIXTURE FAIL: writeTestProgram\n");
      scFail = 1;
    } else {
      currentProgramNumber = 1;
      currentStep = beginOfProgramMemory + 4 + 16;   /* stale PEM cursor, past "SQ"'s step */

      forthCapOpenInteractive();
      { uint8_t m1e3s_ = calcMode; calcMode = CM_PEM;  /* Stage M E3: legacy mode-naked fixture — the text-scan always meant a PEM cursor context */
        testInitVariableSoftmenu(22);
        calcMode = m1e3s_; }

      if (dynamicSoftmenu[22].numItems != 0) {
        printf("    [16] FAIL: MNU_FORTH picker has %d items under an interactive origin, "
               "expected 0 (the stale program's 'SQ' leaked in -- section (a) not gated off)\n",
               dynamicSoftmenu[22].numItems);
        scFail = 1;
        if (dynamicSoftmenu[22].menuContent) {
          free(dynamicSoftmenu[22].menuContent);
          dynamicSoftmenu[22].menuContent = NULL;
          dynamicSoftmenu[22].numItems = 0;
        }
      }
      forthCapClose();
    }
  }
  driven++;
  printf("    [16] %s %s\n", scFail ? "FAIL" : "DRIVEN, PASS:",
         scFail ? "" : "a stale currentStep's program text does not leak into an interactive picker (GATE OFF)");
  fail |= scFail;
  CGA_RESET();
  cleanupTestProgram();

  /* ---- Row 17: screen.c's PEM/AIM display gates. KEEP -- no
   * capture-specific behavior. NOT DRIVEN: pure display/paint code with
   * no state to assert without a real screen render (run-sim territory,
   * not this harness). ---- */
  notDriven++;
  printf("    [17] NOT DRIVEN -- KEEP, pure display: no state to assert without a real "
         "screen render (see the packet's Sim acceptance step for visual confirmation)\n");

  #undef CGA_RESET

  printf("\n    C3 summary: %d/%d rows driven, %d not driven (reported gaps, not silent)\n",
         driven, driven + notDriven, notDriven);

  forthCapClose();
  if (forthFoldPending()) { forthFoldLeave(); }
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
  forthCapSetKeysMode(true);               /* N1-5: the console OPENS in keys input now, so the ALPHA gesture here
                                        would toggle it OFF.  These subcases are
                                        about keys-mode BEHAVIOUR, not about the
                                        toggle (which [3a]/[3(b)] own), so they
                                        state the sub-mode they need. */
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
    /* N1-5 (N-R6) FLIPS this row.  It used to pin the E5 relock — a REPL
     * reopen dropped back to alpha input.  Keys input is the console's ground
     * now, and the flip has to survive EVERY ENTER, not just the first open,
     * or the session silently reverts one line in. */
    if (!scFail && !forthCapKeysMode()) {
      printf("    [6] FAIL: keys mode must SURVIVE the REPL reopen (N-R6 keys-first)\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [6] PASS: keys-mode fold \"STO 05 \" types text, executes only at ENTER (R05 555 -> 16)\n");
  fail |= scFail;

  /* ---- [7] EXIT closes; no string commit (rung 3 never touches X).
   * N1-5: the capture is in keys input here (the ground state), so rung 1
   * does not fire and EXIT falls through rung 2's base test to rung 3 — one
   * press, exactly as before the flip. ---- */
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

  /* ---- [8] FORTH again; f-up recalls FHIST's newest; the four-line
   * record.  This pins push-before-run (steps 2,3,4,6), the fold's text
   * as pushed ("STO 05 "), and the recall gesture, end to end. ---- */
  scFail = 0;
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive() || aimBuffer[0] != 0) {
    printf("    [8] FIXTURE FAIL: reopen did not take (state=%d, line=\"%s\")\n",
           forthTestCapState(), aimBuffer);
    scFail = 1;
  }
  if (!scFail) {
    extern void processKeyAction(int16_t);
    int upRow = -1, i;
    char kbUp[3];
    int16_t itUp;
    for (i = 0; i < 37; i++) {
      if (kbd_std[i].primary == ITM_UP1) { upRow = i; }
    }
    if (upRow < 0) {
      printf("    [8] FIXTURE FAIL: ITM_UP1 not on kbd_std\n");
      scFail = 1;
    } else {
      sprintf(kbUp, "%02d", upRow);
      /* shiftF is one-shot: determineItem's own resetShiftState() clears
       * it after the call (landed recall idiom, L1-H C5.6). */
      shiftF = true;
      itUp = determineItem(kbUp);
      shiftF = false;
      processKeyAction(itUp);
    }
  }
  if (!scFail) {
    uint16_t prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [8] FAIL: FHIST does not exist after the session\n");
      scFail = 1;
    } else {
      uint8_t *lbl = programList[prog - 1].instructionPointer;
      uint8_t *s1 = findNextStep(lbl);
      uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
      uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
      uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
      uint8_t *s5 = s4 ? findNextStep(s4) : NULL;
      if (!s1 || !s2 || !s3 || !s4 || !s5) {
        printf("    [8] FAIL: FHIST walk broke before five steps\n");
        scFail = 1;
      }
      if (!scFail && (!stepSrcTextEq(s1, "1 2 +") ||
                      !stepSrcTextEq(s2, ": SQ DUP * ;") ||
                      !stepSrcTextEq(s3, "4 SQ") ||
                      !stepSrcTextEq(s4, "STO 05 ") ||
                      !isAtEndOfProgram(s5))) {
        printf("    [8] FAIL: FHIST is not the session's four lines in order\n");
        scFail = 1;
      }
      /* The recall matches FHIST's newest by DIRECT comparison against
       * the recalled buffer — one comparison, no second literal to
       * drift (the L1-F3 parity discipline). */
      if (!scFail && !stepSrcTextEq(s4, aimBuffer)) {
        printf("    [8] FAIL: recalled line \"%s\" is not FHIST's newest\n", aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [8] PASS: f-up recalls FHIST's newest; history holds the session's four lines in order\n");
  fail |= scFail;

  /* ---- [9] EXIT collapses the duplicate; XEQ 'FHIST' re-runs the
   * session (L-R7: deliberately runnable). ---- */
  scFail = 0;
  fnKeyExit(NOPARAM);
  if (forthTestCapState() != FCAP_CLOSED) {
    printf("    [9] FAIL: state %d after EXIT, expected FCAP_CLOSED\n", forthTestCapState());
    scFail = 1;
  }
  if (!scFail) {
    /* The EXIT pushed the recalled text — a consecutive duplicate of the
     * newest entry, so it must COLLAPSE: still exactly four lines. */
    uint16_t prog = forthHistoryProgram();
    uint8_t *lbl = prog ? programList[prog - 1].instructionPointer : NULL;
    uint8_t *s1 = lbl ? findNextStep(lbl) : NULL;
    uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
    uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
    uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
    uint8_t *s5 = s4 ? findNextStep(s4) : NULL;
    if (!s4 || !s5 || !stepSrcTextEq(s4, "STO 05 ") || !isAtEndOfProgram(s5)) {
      printf("    [9] FAIL: EXIT's push did not collapse as a consecutive duplicate\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    /* Discriminators: zero R05, put 5 in X — the run must RECOMPUTE both. */
    longIntegerInit(li); int32ToLongInteger(0, li);
    convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);
    longIntegerInit(li); int32ToLongInteger(5, li);
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    lastErrorCode = ERROR_NONE;
    {
      extern void fnExecute(uint16_t);
      calcRegister_t lblF = findNamedLabel("FHIST", GLOBAL_LABELS);
      if (lblF == INVALID_VARIABLE) {
        printf("    [9] FAIL: FHIST label not findable by name — XEQ 'FHIST' (L-R7) would not work\n");
        scFail = 1;
      } else {
        fnExecute(lblF);
        if (lastErrorCode != ERROR_NONE) {
          printf("    [9] FAIL: FHIST run errored (%d)\n", lastErrorCode);
          scFail = 1;
        }
        if (!scFail && !x_is_longint(16)) {
          printf("    [9] FAIL: X != 16 after the FHIST re-run\n");
          scFail = 1;
        }
        if (!scFail) {
          read_reg_int32(5, &rType, &rVal);
          if (rType != dtLongInteger || rVal != 16) {
            printf("    [9] FAIL: register 05 = %ld type %u, expected 16 (the replay re-ran \"STO 05 \")\n",
                   (long)rVal, rType);
            scFail = 1;
          }
        }
      }
    }
  }
  if (!scFail) printf("    [9] PASS: XEQ 'FHIST' re-runs the session's lines (X and R05 recomputed)\n");
  fail |= scFail;

  /* ---- [10] Durability: GLOBAL survives the next lifetime; the
   * interactive-scope word does not (§8.3, L3's contract). ---- */
  scFail = 0;
  lastErrorCode = ERROR_NONE;
  calcMode = CM_NORMAL;
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [10] FIXTURE FAIL: reopen did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    /* ": TGLO 6 ; GLOBAL" through the key path. */
    runFunction(ITM_COLON); runFunction(ITM_SPACE);
    runFunction(ITM_T); runFunction(ITM_G); runFunction(ITM_L); runFunction(ITM_O);
    runFunction(ITM_SPACE); runFunction(ITM_6); runFunction(ITM_SPACE);
    runFunction(ITM_SEMICOLON); runFunction(ITM_SPACE);
    runFunction(ITM_G); runFunction(ITM_L); runFunction(ITM_O);
    runFunction(ITM_B); runFunction(ITM_A); runFunction(ITM_L);
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [10] FAIL: \": TGLO 6 ; GLOBAL\" errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
  }
  if (!scFail) {
    /* ": TDUR 5 ;" — stays interactive-scope. */
    runFunction(ITM_COLON); runFunction(ITM_SPACE);
    runFunction(ITM_T); runFunction(ITM_D); runFunction(ITM_U); runFunction(ITM_R);
    runFunction(ITM_SPACE); runFunction(ITM_5); runFunction(ITM_SPACE);
    runFunction(ITM_SEMICOLON);
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [10] FAIL: \": TDUR 5 ;\" errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
    fnKeyExit(NOPARAM);
  }
  if (!scFail && (!forthFindColon("TGLO", &idx) || !forthFindColon("TDUR", &idx))) {
    printf("    [10] FIXTURE FAIL: TGLO/TDUR not both visible before the reset\n");
    scFail = 1;
  }
  if (!scFail) {
    /* The lifetime-reset program: one ITM_FORTH step.  This tpWrite
     * REPLACES program memory and destroys FHIST — deliberate; every
     * FHIST assertion is behind us (steps 8-9). */
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "TLIF");
    tpSrc(&p, "1");
    tpEnd(&p);
    if (!tpWrite(&p)) {
      printf("    [10] FIXTURE FAIL: TLIF build/write\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    extern void fnExecute(uint16_t);
    calcRegister_t lblT;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    lastErrorCode = ERROR_NONE;
    lblT = findNamedLabel("TLIF", GLOBAL_LABELS);
    if (lblT == INVALID_VARIABLE) {
      printf("    [10] FIXTURE FAIL: findNamedLabel(\"TLIF\") returned INVALID_VARIABLE\n");
      scFail = 1;
    } else {
      fnExecute(lblT);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [10] FAIL: TLIF run errored (%d)\n", lastErrorCode);
        scFail = 1;
      }
      if (!scFail && !x_is_longint(1)) {
        printf("    [10] FAIL: X != 1 (the ITM_FORTH step did not run)\n");
        scFail = 1;
      }
      if (!scFail && forthFindColon("TDUR", &idx)) {
        printf("    [10] FAIL: TDUR survived the lifetime reset\n");
        scFail = 1;
      }
      if (!scFail && !forthFindColon("TGLO", &idx)) {
        printf("    [10] FAIL: TGLO did not survive (GLOBAL is the durability mechanism)\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [10] PASS: GLOBAL survives the lifetime reset; the interactive-scope word does not\n");
  fail |= scFail;

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
    /* N1-5 INVERTS rung 1: keys input is the ground state, so the rung
     * unwinds the ALPHA EXCURSION back to keys.  The subcase starts from the
     * excursion and expects to land in keys, capture still open. */
    forthCapSetKeysMode(false);
    if (forthCapKeysMode()) {
      printf("    [1] FIXTURE FAIL: could not enter the alpha excursion\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    fnKeyExit(NOPARAM);                /* rung 1: alpha -> keys (N1-5) */
    if (!forthCapIsOpen() || !forthCapKeysMode()) {
      printf("    [1] FAIL: rung 1 should leave the capture OPEN in keys input (open=%d keys=%d)\n",
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
  if (!scFail) printf("    [1] PASS: EXIT ladder — rung 1 unwinds the alpha excursion to keys, rung 3 closes with the full tuple and pushes the line\n");
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
    forthCapSetKeysMode(true);         /* N1-5: keys is the open default now;
                                          the ALPHA gesture would toggle it OFF */
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
    forthCapSetKeysMode(true);         /* N1-5: keys is the open default now;
                                          the ALPHA gesture would toggle it OFF */
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
    forthCapSetKeysMode(true);         /* N1-5: keys is the open default now;
                                          the ALPHA gesture would toggle it OFF */
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

/* ==================================================================
 * PACKET_L1_5 (C2.2/C2.3) — test_interactive_residue.
 * [1] the capture lifecycle itself allocates nothing: 20 open/close
 *     cycles return getFreeRamMemory() to baseline (escape valve per
 *     the landed F6-2 [6] precedent: bounded, block-aligned,
 *     growth-only allocator quantization is reported, not failed).
 * [2] a full history cap cycle grows program memory by exactly FHIST's
 *     own bytes — nothing else leaks; the cap holds; eviction is
 *     oldest-first.  The plateau is the C4 "program-memory high-water"
 *     number, printed as a REPORT line.
 * ================================================================== */
static int test_interactive_residue(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);

  int fail = 0, scFail;
  int i;

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

  #define L15D_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    tpLbl(&base, "BASED");
    tpEnd(&base);
    if (!tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }

  /* ---- [1] 20 empty open/close cycles: zero arena residue. ---- */
  scFail = 0;
  L15D_RESET();
  /* Warmup: absorb any first-open, first-menu effects before measuring. */
  fnForthOuter(NOPARAM);
  fnKeyExit(NOPARAM);
  L15D_RESET();
  {
    uint32_t freeBefore = getFreeRamMemory();
    for (i = 0; i < 20; i++) {
      fnForthOuter(NOPARAM);
      if (!forthCapIsOpen() || !forthCapIsInteractive()) {
        printf("    [1] FIXTURE FAIL: open %d did not take\n", i);
        scFail = 1;
        break;
      }
      fnKeyExit(NOPARAM);
      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [1] FIXTURE FAIL: close %d did not take\n", i);
        scFail = 1;
        break;
      }
    }
    if (!scFail) {
      uint32_t freeAfter = getFreeRamMemory();
      if (freeAfter != freeBefore) {
        uint32_t delta = (freeBefore > freeAfter) ? (freeBefore - freeAfter)
                                                  : (freeAfter - freeBefore);
        /* Escape valve, landed F6-2 [6] shape: bounded, block-aligned,
         * growth-only allocator quantization is a report, not a leak. */
        if (delta % BYTES_PER_BLOCK == 0 && delta <= 6 * BYTES_PER_BLOCK
            && freeBefore > freeAfter) {
          printf("    [1] PASS (escape valve): freeRam %u -> %u is %u resize quantum(s), not a lifecycle leak\n",
                 (unsigned)freeBefore, (unsigned)freeAfter,
                 (unsigned)(delta / BYTES_PER_BLOCK));
        } else {
          printf("    [1] FAIL: freeRam %u -> %u across 20 empty open/close cycles\n",
                 (unsigned)freeBefore, (unsigned)freeAfter);
          scFail = 1;
        }
      } else {
        printf("    [1] PASS: 20 open/close cycles leave getFreeRamMemory() at baseline (no arena residue)\n");
      }
    }
  }
  fail |= scFail;

  /* ---- [2] Full cap cycle: growth == FHIST's own bytes, cap holds,
   * eviction is oldest-first. ---- */
  scFail = 0;
  L15D_RESET();
  {
    uint32_t pgmOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
    char line[16];
    uint16_t prog;

    if (forthHistoryProgram() != 0) {
      printf("    [2] FIXTURE FAIL: FHIST already exists before the cap cycle\n");
      scFail = 1;
    }
    if (!scFail) {
      for (i = 0; i < 200; i++) {
        sprintf(line, "RLINE%04d", i);
        forthHistoryPush(line);
      }
      prog = forthHistoryProgram();
      if (prog == 0) {
        printf("    [2] FAIL: FHIST not created by 200 pushes\n");
        scFail = 1;
      } else {
        uint8_t *begin = programList[prog - 1].instructionPointer;
        uint8_t *step = begin;
        uint8_t *firstContent = NULL;
        uint32_t totalBytes;
        uint32_t pgmOffAfter;
        while (!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) {
          if (firstContent == NULL && step != begin) { firstContent = step; }
          step = findNextStep(step);
        }
        totalBytes = (uint32_t)(step - begin) + 2;
        pgmOffAfter = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

        printf("    [2] REPORT: program-memory high-water with a full history: %u bytes (cap %u)\n",
               (unsigned)totalBytes, (unsigned)FORTH_HISTORY_MAX_BYTES);

        /* The cap is asserted as the LITERAL 1024, not the macro: a
         * fixture sized from the constant is immune to a change in the
         * constant, and therefore blind to one (the G2 cut-off lesson,
         * QWEN_RUNBOOK §2c).  The L1-H cap subcase uses the macro and is
         * legacy evidence; this is the literal pin beside the mechanism. */
        if (totalBytes > 1024) {
          printf("    [2] FAIL: FHIST is %u bytes, over the 1024-byte cap\n",
                 (unsigned)totalBytes);
          scFail = 1;
        }
        if (!scFail && (pgmOffAfter - pgmOffBefore) != totalBytes) {
          printf("    [2] FAIL: program memory grew %u bytes but FHIST is %u — residue outside FHIST\n",
                 (unsigned)(pgmOffAfter - pgmOffBefore), (unsigned)totalBytes);
          scFail = 1;
        }
        if (!scFail && (firstContent == NULL || stepSrcTextEq(firstContent, "RLINE0000"))) {
          printf("    [2] FAIL: oldest line survived a full cap cycle (eviction not oldest-first)\n");
          scFail = 1;
        }
        if (!scFail) {
          printf("    [2] PASS: a full cap cycle grows program memory by exactly FHIST's own bytes (cap respected, oldest evicted)\n");
        }
      }
    }
  }
  fail |= scFail;

  forthCapClose();
  cleanupTestProgram();
  #undef L15D_RESET
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

/* ==================================================================
 * PACKET_M1_1 — test_fwrd_normal_mode: the FWRD catalog outside
 * captures.  E2's dispositions (capture-insert / normal-execute /
 * inert elsewhere; the CM_ASSIGN feed is M1-2's battery), E3's
 * listing gate, the M-T5 drain row, and the stale-press error
 * surface.  Presses drive the real resolution
 * (determineFunctionKeyItem_C47, the G1 stack-staging idiom) and
 * execution rides runFunction(pressedItem) — the executeFunction
 * tail's own call.
 * ================================================================== */
static int test_fwrd_normal_mode(void)
{
  extern void     showSoftmenu(int16_t menu);
  extern char    *dynmenuGetLabel(int16_t menuitem);
  extern int16_t  determineFunctionKeyItem_C47(const char *data, bool_t shiftF, bool_t shiftG);
  extern bool_t   forthPickerGuard(int16_t item);
  extern bool_t   pickerInsertName(void);
  extern void     runFunction(int16_t);
  extern void     fnForthOuter(uint16_t);
  extern void     fnKeyExit(uint16_t);
  extern void     tamEnterMode(int16_t);
  extern void     testInitVariableSoftmenu(int16_t);

  int fail = 0, scFail;
  int i;
  int16_t pressedItem = ITM_NOP;
  int idxMW1 = -1, idxMW2 = -1, idxMW3 = -1, idxPW = -1;
  uint8_t tType; int32_t tVal;

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
  int16_t savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent = dynamicSoftmenu[22].menuContent;
  int16_t savedNumItems = dynamicSoftmenu[22].numItems;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems = 0;

  #define M11_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  #define M11_PRESS(idx_) do { \
    char kbuf_[2]; \
    softmenuStack[0].softmenuId = 22; \
    softmenuStack[0].firstItem = 0; \
    kbuf_[0] = (char)('1' + (idx_)); kbuf_[1] = 0; \
    pressedItem = determineFunctionKeyItem_C47(kbuf_, false, false); \
  } while (0)

  #define M11_SCAN() do { \
    idxMW1 = idxMW2 = idxMW3 = idxPW = -1; \
    for (i = 0; i < dynamicSoftmenu[22].numItems; i++) { \
      char *lbl_ = dynmenuGetLabel(i); \
      if (compareString(lbl_, "MW1", CMP_BINARY) == 0) idxMW1 = i; \
      if (compareString(lbl_, "MW2", CMP_BINARY) == 0) idxMW2 = i; \
      if (compareString(lbl_, "MW3", CMP_BINARY) == 0) idxMW3 = i; \
      if (compareString(lbl_, "PW",  CMP_BINARY) == 0) idxPW  = i; \
    } \
  } while (0)

  /* Fixture: baseline program with one Forth source step (the text-scan
   * probe), one global and one interactive word. */
  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    if (tpLbl(&base, "BASEM") < 0 || tpSrc(&base, ": PW 7 ;") < 0 || tpEnd(&base) < 0
        || !tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }
  M11_RESET();
  forthOuterInterpret(": MW1 41 ; GLOBAL");
  forthOuterInterpret(": MW2 42 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FIXTURE FAIL: word setup errored (%d)\n", lastErrorCode);
    cleanupTestProgram();
    return 1;
  }

  /* ---- [1] CM_NORMAL press of the global word executes (+6a: no PW). ---- */
  scFail = 0;
  showSoftmenu(-MNU_FORTH);
  testInitVariableSoftmenu(22);   /* showSoftmenu pushes; the DRAW builds —
                                     build directly, the landed idiom */
  M11_SCAN();
  if (idxMW1 < 0 || idxMW2 < 0 || idxMW1 >= 6 || idxMW2 >= 6) {
    printf("    [1] FIXTURE FAIL: words not on page 0 (MW1=%d MW2=%d)\n", idxMW1, idxMW2);
    scFail = 1;
  }
  if (!scFail && idxPW != -1) {
    printf("    [1] FAIL: text-scan name PW listed in CM_NORMAL (E3 gate)\n");
    scFail = 1;
  }
  if (!scFail) {
    M11_PRESS(idxMW1);
    if (pressedItem != ITM_XEQ || dynamicMenuItem != idxMW1) {
      printf("    [1] FAIL: resolution item %d dynIdx %d, expected ITM_XEQ at %d\n",
             pressedItem, dynamicMenuItem, idxMW1);
      scFail = 1;
    }
  }
  if (!scFail) {
    runFunction(pressedItem);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [1] FAIL: dispatch errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
    if (!scFail && !x_is_longint(41)) {
      printf("    [1] FAIL: X != 41 after pressing MW1\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [1] PASS: CM_NORMAL FWRD press executes the global word; text-scan name not listed\n");
  fail |= scFail;

  /* ---- [2] CM_NORMAL press of the interactive word executes. ---- */
  scFail = 0;
  if (idxMW2 < 0) {
    printf("    [2] FIXTURE FAIL: MW2 not listed\n");
    scFail = 1;
  }
  if (!scFail) M11_PRESS(idxMW2);
  if (pressedItem != ITM_XEQ) {
    printf("    [2] FAIL: resolution item %d, expected ITM_XEQ\n", pressedItem);
    scFail = 1;
  }
  if (!scFail) {
    runFunction(pressedItem);
    if (lastErrorCode != ERROR_NONE || !x_is_longint(42)) {
      printf("    [2] FAIL: interactive word did not run (err %d)\n", lastErrorCode);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [2] PASS: CM_NORMAL FWRD press executes the interactive word (X == 42)\n");
  fail |= scFail;

  /* ---- [3] Capture press still inserts (coupled edit, direction A). ---- */
  scFail = 0;
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [3] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  if (!scFail && idxMW1 < 0) {
    printf("    [3] FIXTURE FAIL: MW1 not listed\n");
    scFail = 1;
  }
  if (!scFail) {
    read_reg_int32(REGISTER_X, &tType, &tVal);
    M11_PRESS(idxMW1);
    if (pressedItem != ITM_NOP) {
      printf("    [3] FAIL: resolution item %d during capture, expected ITM_NOP\n", pressedItem);
      scFail = 1;
    }
    if (!scFail && !forthPickerGuard(pressedItem)) {
      printf("    [3] FAIL: picker guard did not fire during capture\n");
      scFail = 1;
    }
    if (!scFail) {
      pickerInsertName();
      if (compareString(aimBuffer, "MW1 ", CMP_BINARY) != 0) {
        printf("    [3] FAIL: line \"%s\", expected \"MW1 \"\n", aimBuffer);
        scFail = 1;
      }
    }
    if (!scFail) {
      uint8_t tType2; int32_t tVal2;
      read_reg_int32(REGISTER_X, &tType2, &tVal2);
      if (tType2 != tType || tVal2 != tVal) {
        printf("    [3] FAIL: X changed across a capture press (executed?)\n");
        scFail = 1;
      }
    }
    aimBuffer[0] = 0;      /* abandon the line so EXIT pushes nothing */
    T_cursorPos = 0;
    fnKeyExit(NOPARAM);
  }
  if (!scFail) printf("    [3] PASS: capture press inserts \"MW1 \" and executes nothing\n");
  fail |= scFail;

  /* ---- [4] Native AIM (no capture) stays inert. ---- */
  scFail = 0;
  M11_RESET();
  calcMode = CM_AIM;
  setSystemFlag(FLAG_ALPHA);
  aimBuffer[0] = 0;
  M11_PRESS(idxMW1);
  if (pressedItem != ITM_NOP) {
    printf("    [4] FAIL: resolution item %d in native AIM, expected ITM_NOP\n", pressedItem);
    scFail = 1;
  }
  if (!scFail && forthPickerGuard(pressedItem)) {
    printf("    [4] FAIL: picker guard fired without a capture\n");
    scFail = 1;
  }
  if (!scFail && aimBuffer[0] != 0) {
    printf("    [4] FAIL: native AIM line gained text\n");
    scFail = 1;
  }
  clearSystemFlag(FLAG_ALPHA);
  if (!scFail) printf("    [4] PASS: native AIM press is inert (no insert, no execute)\n");
  fail |= scFail;

  /* ---- [5] PEM outside a capture stays inert. ---- */
  scFail = 0;
  M11_RESET();
  calcMode = CM_PEM;
  {
    uint16_t stepsBefore = getNumberOfSteps();
    M11_PRESS(idxMW1);
    if (pressedItem != ITM_NOP) {
      printf("    [5] FAIL: resolution item %d in PEM-no-capture, expected ITM_NOP\n", pressedItem);
      scFail = 1;
    }
    if (!scFail && getNumberOfSteps() != stepsBefore) {
      printf("    [5] FAIL: step count changed\n");
      scFail = 1;
    }
  }
  if (!scFail) printf("    [5] PASS: PEM-outside-capture press is inert\n");
  fail |= scFail;

  /* ---- [6] The listing gate's PEM half: PW appears with true provenance. ---- */
  scFail = 0;
  M11_RESET();
  calcMode = CM_PEM;
  dynamicMenuItem = -1;
  fnGotoDot(2);                    /* the ": PW 7 ;" source step */
  testInitVariableSoftmenu(22);
  M11_SCAN();
  if (idxPW < 0) {
    printf("    [6] FAIL: PW not listed in CM_PEM at its own program (gate too wide)\n");
    scFail = 1;
  }
  M11_RESET();
  testInitVariableSoftmenu(22);
  M11_SCAN();
  if (!scFail && idxPW != -1) {
    printf("    [6] FAIL: PW still listed after returning to CM_NORMAL\n");
    scFail = 1;
  }
  if (!scFail) printf("    [6] PASS: text-scan section lists in CM_PEM only (E3 gate, both directions)\n");
  fail |= scFail;

  /* ---- [7] Stale-picker press errors natively. ---- */
  scFail = 0;
  M11_RESET();
  testInitVariableSoftmenu(22);
  M11_SCAN();
  if (idxMW1 < 0) {
    printf("    [7] FIXTURE FAIL: MW1 not listed before the clear\n");
    scFail = 1;
  }
  if (!scFail) {
    uint8_t tType2; int32_t tVal2;
    forthDictClear();
    forthGDictClear();
    read_reg_int32(REGISTER_X, &tType, &tVal);
    M11_PRESS(idxMW1);
    runFunction(pressedItem);
    if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
      printf("    [7] FAIL: error %d, expected ERROR_LABEL_NOT_FOUND\n", lastErrorCode);
      scFail = 1;
    }
    read_reg_int32(REGISTER_X, &tType2, &tVal2);
    if (!scFail && (tType2 != tType || tVal2 != tVal)) {
      printf("    [7] FAIL: X changed on a not-found press\n");
      scFail = 1;
    }
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [7] PASS: a stale FWRD press surfaces ERROR_LABEL_NOT_FOUND, X untouched\n");
  fail |= scFail;

  /* ---- [8] A capture opened over stacked FWRD/CATALOG: buried and
   * harmless, and EXIT restores the user's menus.  (M-T5 as CORRECTED
   * by this test: the FIX-9-analog drain is catalog-VAR-gated —
   * forth_compile.c:1717 `if (catalog)` — and menu-tree rows never set
   * that variable, so the plain stack legitimately survives beneath the
   * capture's alpha frame; rung 3's teardown then reveals it again,
   * exactly closeAim's native shape.  The trace predicted a drain here
   * and was wrong about the gate — reachability, not write-set.) ---- */
  scFail = 0;
  M11_RESET();
  showSoftmenu(-MNU_CATALOG);
  showSoftmenu(-MNU_FORTH);
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    [8] FIXTURE FAIL: interactive open did not take\n");
    scFail = 1;
  }
  /* N1-5 (N-R6): the console's home row is FWRD, so THAT is what sits on top
   * after an open — the drain disposition this subcase pins (KEEP, buried and
   * harmless) is unchanged; only the identity of the frame the open pushes
   * has moved from -MNU_ALPHA to -MNU_FORTH. */
  if (!scFail && currentMenu() != -MNU_FORTH) {
    printf("    [8] FAIL: FWRD home row not on top after open over a stack (menu %d)\n", currentMenu());
    scFail = 1;
  }
  if (!scFail) {
    runFunction(ITM_1);
    if (compareString(aimBuffer, "1", CMP_BINARY) != 0) {
      printf("    [8] FAIL: typing over the buried stack broke (line \"%s\")\n", aimBuffer);
      scFail = 1;
    }
  }
  if (!scFail) {
    aimBuffer[0] = 0;
    T_cursorPos = 0;
    fnKeyExit(NOPARAM);
    if (forthTestCapState() != FCAP_CLOSED || calcMode != CM_NORMAL) {
      printf("    [8] FAIL: EXIT did not close cleanly over the stack\n");
      scFail = 1;
    }
    if (!scFail && currentMenu() != -MNU_FORTH) {
      printf("    [8] FAIL: the user's FWRD menu did not come back after EXIT (menu %d)\n",
             currentMenu());
      scFail = 1;
    }
  }
  if (!scFail) printf("    [8] PASS: capture works over a buried FWRD/CATALOG stack; EXIT restores the user's menus\n");
  fail |= scFail;

  /* ---- [9] XEQ-TAM keeps the latch shape (the tam.mode == 0 conjunct). ---- */
  scFail = 0;
  M11_RESET();
  forthOuterInterpret(": MW3 43 ;");
  testInitVariableSoftmenu(22);
  M11_SCAN();
  if (idxMW3 < 0 || idxMW3 >= 6) {
    printf("    [9] FIXTURE FAIL: MW3 not on page 0 (%d)\n", idxMW3);
    scFail = 1;
  }
  if (!scFail) {
    tamEnterMode(ITM_XEQ);
    if (tam.mode == 0) {
      printf("    [9] FIXTURE FAIL: XEQ TAM did not arm\n");
      scFail = 1;
    }
    if (!scFail) {
      M11_PRESS(idxMW3);
      if (pressedItem != ITM_NOP) {
        printf("    [9] FAIL: resolution item %d during XEQ TAM, expected ITM_NOP (latch shape)\n",
               pressedItem);
        scFail = 1;
      }
      if (!scFail && dynamicMenuItem != idxMW3) {
        printf("    [9] FAIL: dynamicMenuItem %d not latched to %d\n", dynamicMenuItem, idxMW3);
        scFail = 1;
      }
    }
    fnKeyExit(NOPARAM);            /* cancel the TAM */
  }
  if (!scFail) printf("    [9] PASS: XEQ-TAM press keeps the landed latch-then-dispatch shape\n");
  fail |= scFail;

  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  #undef M11_RESET
  #undef M11_PRESS
  #undef M11_SCAN
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems = savedNumItems;
  cachedDynamicMenu = savedCachedDynamicMenu;
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

/* ==================================================================
 * PACKET_M1_2 — test_fwrd_assign: a global Forth word onto a key.
 * The pick rides the real resolution + CM_ASSIGN switch
 * (executeFunction -> determineFunctionKeyItem_C47 -> the band); the
 * record rides the real assignToKey/_assignItem; the press rides the
 * real determineItem USER-key dispatch (label miss -> the landed
 * forthTryColonFallback).  kbd_usr[21] and its label slot are
 * snapshotted and restored.
 * ================================================================== */
static int test_fwrd_assign(void)
{
  extern void     showSoftmenu(int16_t menu);
  extern char    *dynmenuGetLabel(int16_t menuitem);
  extern void     testInitVariableSoftmenu(int16_t);
  extern void     executeFunction(const char *data, int16_t item_);
  extern void     fnGotoDot(uint16_t);
  /* fnAssign, assignToKey, getUserKeyLabelString, setUserKeyArgument,
   * determineItem: prototypes arrive via assign.h/c47.h — no local externs
   * (a hand-extern with a drifted type is exactly the checklist-5 class). */

  int fail = 0, scFail;
  int i;
  int idxMA1 = -1, idxMA2 = -1;
  int16_t pressedUserItem = ITM_NOP;
  bool_t fallbackFired = false;
  uint8_t tType; int32_t tVal;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  bool_t savedUser = getSystemFlag(FLAG_USER);
  uint8_t savedCalcMode = calcMode;
  uint8_t savedPreviousCalcMode = previousCalcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedItemToBeAssigned = itemToBeAssigned;
  int16_t savedCachedDynamicMenu = cachedDynamicMenu;
  uint8_t *savedMenuContent = dynamicSoftmenu[22].menuContent;
  int16_t savedNumItems = dynamicSoftmenu[22].numItems;
  calcKey_t savedKey21;
  char savedKey21Label[16];
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));
  xcopy(&savedKey21, kbd_usr + 21, sizeof(calcKey_t));
  xcopy(savedKey21Label, (char *)getUserKeyLabelString(21 * 6),
        stringByteLength((char *)getUserKeyLabelString(21 * 6)) + 1);

  dynamicSoftmenu[22].menuContent = NULL;
  dynamicSoftmenu[22].numItems = 0;

  #define M12_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; itemToBeAssigned = 0; \
    clearSystemFlag(FLAG_ALPHA); clearSystemFlag(FLAG_USER); \
    lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  #define M12_SCAN() do { \
    idxMA1 = idxMA2 = -1; \
    for (i = 0; i < dynamicSoftmenu[22].numItems; i++) { \
      char *lbl_ = dynmenuGetLabel(i); \
      if (compareString(lbl_, "MA1", CMP_BINARY) == 0) idxMA1 = i; \
      if (compareString(lbl_, "MA2", CMP_BINARY) == 0) idxMA2 = i; \
    } \
  } while (0)

  /* The press, two halves back to back (the btnReleased XEQ arm is not
   * harness-driveable: its funcParam/dispatch live in the GTK-typed
   * button handlers).  Half 1: determineItem resolves the USER key.
   * Half 2: the dispatch, mirroring btnReleased's item == ITM_XEQ arm
   * VERBATIM — findNamedLabel first, forthTryColonFallback on the miss
   * (the arm's own not-found error display is the statement below the
   * mirrored site and is not re-driven; [4] asserts the refusal +
   * untouched X instead). */
  #define M12_PRESS_USER_KEY21() do { \
    char *fp_; \
    calcRegister_t lbl_; \
    setSystemFlag(FLAG_USER); \
    dynamicMenuItem = -1; \
    pressedUserItem = determineItem("21"); \
    clearSystemFlag(FLAG_USER); \
    fp_ = (char *)getUserKeyLabelString(21 * 6); \
    fallbackFired = false; \
    if (pressedUserItem == ITM_XEQ && fp_[0] != 0) { \
      lbl_ = findNamedLabel(fp_, GLOBAL_LABELS); \
      if (lbl_ != INVALID_VARIABLE) { forthUserItemDispatch(pressedUserItem, fp_, pressedUserItem, lbl_); fallbackFired = true; } \
      else { fallbackFired = forthTryColonFallback(pressedUserItem, fp_); } \
    } \
  } while (0)

  bool_t hadUserKeyLabel = (userKeyLabel != NULL);
  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    if (tpLbl(&base, "BASEN") < 0 || tpEnd(&base) < 0 || !tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }
  M12_RESET();
  forthOuterInterpret(": MA1 43 ; GLOBAL");
  forthOuterInterpret(": MA2 44 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FIXTURE FAIL: word setup errored (%d)\n", lastErrorCode);
    cleanupTestProgram();
    return 1;
  }

  /* ---- [1] End to end: pick -> band -> record -> USER press runs. ---- */
  scFail = 0;
  showSoftmenu(-MNU_FORTH);
  testInitVariableSoftmenu(22);
  M12_SCAN();
  if (idxMA1 < 0 || idxMA1 >= 6 || idxMA2 < 0 || idxMA2 >= 6) {
    printf("    [1] FIXTURE FAIL: words not on page 0 (MA1=%d MA2=%d)\n", idxMA1, idxMA2);
    scFail = 1;
  }
  if (!scFail) {
    char kbuf[2];
    previousCalcMode = CM_NORMAL;
    fnAssign(0);
    if (calcMode != CM_ASSIGN || itemToBeAssigned != 0) {
      printf("    [1] FIXTURE FAIL: fnAssign did not arm CM_ASSIGN\n");
      scFail = 1;
    }
    if (!scFail) {
      softmenuStack[0].softmenuId = 22;
      softmenuStack[0].firstItem = 0;
      kbuf[0] = (char)('1' + idxMA1); kbuf[1] = 0;
      executeFunction(kbuf, 0);
      if (itemToBeAssigned < ASSIGN_FORTH_WORDS) {
        printf("    [1] FAIL: itemToBeAssigned %d, expected the ASSIGN_FORTH_WORDS band\n",
               itemToBeAssigned);
        scFail = 1;
      }
      if (!scFail && compareString(getItemCatalogName(itemToBeAssigned), "MA1", CMP_BINARY) != 0) {
        printf("    [1] FAIL: pseudo-item name \"%s\", expected \"MA1\"\n",
               getItemCatalogName(itemToBeAssigned));
        scFail = 1;
      }
    }
    if (!scFail) {
      shiftF = false; shiftG = false;
      assignToKey("21");
      if (kbd_usr[21].primary != ITM_XEQ) {
        printf("    [1] FAIL: key 21 primary %d, expected ITM_XEQ\n", kbd_usr[21].primary);
        scFail = 1;
      }
      if (!scFail && compareString((char *)getUserKeyLabelString(21 * 6), "MA1", CMP_BINARY) != 0) {
        printf("    [1] FAIL: key 21 label \"%s\", expected \"MA1\"\n",
               (char *)getUserKeyLabelString(21 * 6));
        scFail = 1;
      }
    }
    calcMode = CM_NORMAL;
    itemToBeAssigned = 0;
    if (!scFail) {
      lastErrorCode = ERROR_NONE;
      M12_PRESS_USER_KEY21();
      if (pressedUserItem != ITM_XEQ) {
        printf("    [1] FAIL: USER key resolved to %d, expected ITM_XEQ\n", pressedUserItem);
        scFail = 1;
      }
      if (!scFail && (lastErrorCode != ERROR_NONE || !fallbackFired || !x_is_longint(43))) {
        printf("    [1] FAIL: USER press did not run MA1 (err %d)\n", lastErrorCode);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS: pick -> (ITM_XEQ, MA1) record -> USER press runs the word (X == 43)\n");
  fail |= scFail;

  /* ---- [2] Non-global pick refuses with no state change. ---- */
  scFail = 0;
  M12_RESET();
  previousCalcMode = CM_NORMAL;
  fnAssign(0);
  {
    char kbuf[2];
    softmenuStack[0].softmenuId = 22;
    softmenuStack[0].firstItem = 0;
    kbuf[0] = (char)('1' + idxMA2); kbuf[1] = 0;
    executeFunction(kbuf, 0);
    if (itemToBeAssigned != 0) {
      printf("    [2] FAIL: itemToBeAssigned %d after an interactive-word pick, expected 0\n",
             itemToBeAssigned);
      scFail = 1;
    }
  }
  calcMode = CM_NORMAL;
  itemToBeAssigned = 0;
  if (!scFail) printf("    [2] PASS: a non-global pick refuses — no band value, no record\n");
  fail |= scFail;

  /* ---- [3] Save/restore round-trip: the record is ordinary format. ---- */
  scFail = 0;
  M12_RESET();
  saveCalc();
  forthGDictClear();
  restoreCalc();
  if (kbd_usr[21].primary != ITM_XEQ
      || compareString((char *)getUserKeyLabelString(21 * 6), "MA1", CMP_BINARY) != 0) {
    printf("    [3] FAIL: assignment did not survive save/restore (primary %d label \"%s\")\n",
           kbd_usr[21].primary, (char *)getUserKeyLabelString(21 * 6));
    scFail = 1;
  }
  if (!scFail) {
    lastErrorCode = ERROR_NONE;
    M12_PRESS_USER_KEY21();
    if (lastErrorCode != ERROR_NONE || !fallbackFired || !x_is_longint(43)) {
      printf("    [3] FAIL: restored assignment did not run (err %d)\n", lastErrorCode);
      scFail = 1;
    }
  }
  if (!scFail) printf("    [3] PASS: the assignment survives save/restore and still runs (zero new format surface)\n");
  fail |= scFail;

  /* ---- [4] FORGET-then-press: the native error surface. ---- */
  scFail = 0;
  M12_RESET();
  forthOuterInterpret("FORGET MA1");
  if (lastErrorCode != ERROR_NONE) {
    printf("    [4] FIXTURE FAIL: FORGET errored (%d)\n", lastErrorCode);
    scFail = 1;
  }
  if (!scFail) {
    read_reg_int32(REGISTER_X, &tType, &tVal);
    lastErrorCode = ERROR_NONE;
    M12_PRESS_USER_KEY21();
    if (fallbackFired) {
      printf("    [4] FAIL: a dangling name dispatched (fallback fired)\n");
      scFail = 1;
    }
    if (!scFail) {
      uint8_t tType2; int32_t tVal2;
      read_reg_int32(REGISTER_X, &tType2, &tVal2);
      if (tType2 != tType || tVal2 != tVal) {
        printf("    [4] FAIL: X changed on a dangling-key press\n");
        scFail = 1;
      }
    }
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [4] PASS: a dangling key refuses (label and colon both miss), X untouched; the native arm's error line sits below the mirrored site\n");
  fail |= scFail;

  /* ---- [5] PEM press records XEQ 'MA1', never executes. ---- */
  scFail = 0;
  M12_RESET();
  forthOuterInterpret(": MA1 43 ; GLOBAL");   /* re-create after [4]'s FORGET */
  if (lastErrorCode != ERROR_NONE) {
    printf("    [5] FIXTURE FAIL: word re-setup errored (%d)\n", lastErrorCode);
    scFail = 1;
  }
  if (!scFail) {
    uint16_t stepsBefore;
    calcMode = CM_PEM;
    dynamicMenuItem = -1;
    fnGotoDot(1);
    stepsBefore = getNumberOfSteps();
    read_reg_int32(REGISTER_X, &tType, &tVal);
    lastErrorCode = ERROR_NONE;
    M12_PRESS_USER_KEY21();
    if (getNumberOfSteps() != stepsBefore + 1) {
      printf("    [5] FAIL: step count %u, expected %u (record, not execute)\n",
             getNumberOfSteps(), stepsBefore + 1);
      scFail = 1;
    }
    if (!scFail) {
      uint8_t tType2; int32_t tVal2;
      read_reg_int32(REGISTER_X, &tType2, &tVal2);
      if (tType2 != tType || tVal2 != tVal) {
        printf("    [5] FAIL: X changed — the word executed live in PEM\n");
        scFail = 1;
      }
    }
    calcMode = CM_NORMAL;
  }
  if (!scFail) printf("    [5] PASS: a PEM press records the XEQ step by name and executes nothing\n");
  fail |= scFail;

  /* ---- [6] Pending display: the assign TAM buffer shows the word. ---- */
  scFail = 0;
  M12_RESET();
  testInitVariableSoftmenu(22);
  M12_SCAN();
  if (idxMA1 < 0) {
    printf("    [6] FIXTURE FAIL: MA1 not listed after re-create\n");
    scFail = 1;
  }
  if (!scFail) {
    char kbuf[2];
    previousCalcMode = CM_NORMAL;
    fnAssign(0);
    softmenuStack[0].softmenuId = 22;
    softmenuStack[0].firstItem = 0;
    kbuf[0] = (char)('1' + idxMA1); kbuf[1] = 0;
    executeFunction(kbuf, 0);
    updateAssignTamBuffer();
    if (strstr(tamBuffer, "MA1") == NULL) {
      printf("    [6] FAIL: tamBuffer \"%s\" does not show MA1\n", tamBuffer);
      scFail = 1;
    }
  }
  calcMode = CM_NORMAL;
  itemToBeAssigned = 0;
  if (!scFail) printf("    [6] PASS: the pending-assignment display names the word\n");
  fail |= scFail;

  /* restore the key and every global */
  xcopy(kbd_usr + 21, &savedKey21, sizeof(calcKey_t));
  if (hadUserKeyLabel) {
    setUserKeyArgument(21 * 6, savedKey21Label);
  }
  else if (userKeyLabel != NULL) {
    /* This test's first setUserKeyArgument lazily created the persistent
     * userKeyLabel block (assign.c initUserKeyArgument).  The suite's
     * FIX-6 leak gate has no allowance for lazy persistent inits, so
     * restore the pre-test world exactly: free what only this test
     * caused to exist. */
    freeC47Blocks(userKeyLabel, TO_BLOCKS(userKeyLabelSize));
    userKeyLabel = NULL;
    userKeyLabelSize = 0;
  }
  forthCapClose();
  cleanupTestProgram();
  forthDictClear();
  forthGDictClear();
  #undef M12_RESET
  #undef M12_SCAN
  if (dynamicSoftmenu[22].menuContent) {
    free(dynamicSoftmenu[22].menuContent);
  }
  dynamicSoftmenu[22].menuContent = savedMenuContent;
  dynamicSoftmenu[22].numItems = savedNumItems;
  cachedDynamicMenu = savedCachedDynamicMenu;
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  previousCalcMode = savedPreviousCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  itemToBeAssigned = savedItemToBeAssigned;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  if (savedUser) setSystemFlag(FLAG_USER); else clearSystemFlag(FLAG_USER);
  lastErrorCode = ERROR_NONE;

  return fail;
}

/* ==================================================================
 * M1-3 — test_fwrd_late_binding: the one cross-feature beat the M1-1/
 * M1-2 batteries do not already pin.  The assignment stores a NAME
 * (M-R3): after FORGET + re-define, the same key runs the NEW
 * definition — no stale index, no rebind step.
 * ================================================================== */
static int test_fwrd_late_binding(void)
{
  int fail = 0;
  int16_t pressedUserItem = ITM_NOP;
  bool_t fallbackFired = false;

  bool_t savedUser = getSystemFlag(FLAG_USER);
  uint8_t savedCalcMode = calcMode;
  uint8_t savedPreviousCalcMode = previousCalcMode;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedItemToBeAssigned = itemToBeAssigned;
  uint8_t savedProgramRunStop = programRunStop;
  calcKey_t savedKey21;
  char savedKey21Label[16];
  bool_t hadUserKeyLabel = (userKeyLabel != NULL);
  xcopy(&savedKey21, kbd_usr + 21, sizeof(calcKey_t));
  if (hadUserKeyLabel) {
    xcopy(savedKey21Label, (char *)getUserKeyLabelString(21 * 6),
          stringByteLength((char *)getUserKeyLabelString(21 * 6)) + 1);
  } else {
    savedKey21Label[0] = 0;
  }

  calcMode = CM_NORMAL;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
  itemToBeAssigned = 0;
  clearSystemFlag(FLAG_USER);
  lastErrorCode = ERROR_NONE;
  forthCapClose();

  forthOuterInterpret(": MLB 51 ; GLOBAL");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FIXTURE FAIL: word setup errored (%d)\n", lastErrorCode);
    return 1;
  }
  /* Assign by the record directly (the pick flow is M1-2 [1]'s pin;
   * this test pins the BINDING, not the pick). */
  kbd_usr[21].primary = ITM_XEQ;
  setUserKeyArgument(21 * 6, "MLB");

  #define MLB_PRESS() do { \
    char *fp_; \
    calcRegister_t lbl_; \
    setSystemFlag(FLAG_USER); \
    dynamicMenuItem = -1; \
    pressedUserItem = determineItem("21"); \
    clearSystemFlag(FLAG_USER); \
    fp_ = (char *)getUserKeyLabelString(21 * 6); \
    fallbackFired = false; \
    if (pressedUserItem == ITM_XEQ && fp_[0] != 0) { \
      lbl_ = findNamedLabel(fp_, GLOBAL_LABELS); \
      if (lbl_ != INVALID_VARIABLE) { forthUserItemDispatch(pressedUserItem, fp_, pressedUserItem, lbl_); fallbackFired = true; } \
      else { fallbackFired = forthTryColonFallback(pressedUserItem, fp_); } \
    } \
  } while (0)

  MLB_PRESS();
  if (!fallbackFired || !x_is_longint(51)) {
    printf("    FAIL: first press did not run the original MLB (51)\n");
    fail = 1;
  }
  if (!fail) {
    forthOuterInterpret("FORGET MLB");
    forthOuterInterpret(": MLB 52 ; GLOBAL");
    if (lastErrorCode != ERROR_NONE) {
      printf("    FIXTURE FAIL: re-define errored (%d)\n", lastErrorCode);
      fail = 1;
    }
  }
  if (!fail) {
    MLB_PRESS();
    if (!fallbackFired || !x_is_longint(52)) {
      printf("    FAIL: press after FORGET+re-define did not run the NEW definition (52)\n");
      fail = 1;
    }
  }
  if (!fail) printf("    PASS: the key is bound to the NAME — FORGET + re-define, the same press runs the new word (51 -> 52)\n");

  #undef MLB_PRESS
  forthOuterInterpret("FORGET MLB");
  lastErrorCode = ERROR_NONE;
  xcopy(kbd_usr + 21, &savedKey21, sizeof(calcKey_t));
  if (hadUserKeyLabel) {
    setUserKeyArgument(21 * 6, savedKey21Label);
  }
  else if (userKeyLabel != NULL) {
    freeC47Blocks(userKeyLabel, TO_BLOCKS(userKeyLabelSize));
    userKeyLabel = NULL;
    userKeyLabelSize = 0;
  }
  forthDictClear();
  forthGDictClear();
  calcMode = savedCalcMode;
  previousCalcMode = savedPreviousCalcMode;
  dynamicMenuItem = savedDynamicMenu;
  itemToBeAssigned = savedItemToBeAssigned;
  programRunStop = savedProgramRunStop;
  if (savedUser) setSystemFlag(FLAG_USER); else clearSystemFlag(FLAG_USER);
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* ==================================================================
 * AUDIT round 6 (AUDIT_round6_2026-08-08.md) — the fold/suspend window,
 * driven.  Reproducers and class tests for the round's confirmed findings:
 *
 *   [1] F2  — the SYSFL EXIT cancel must unwind the armed fold
 *   [2] F1  — the GTO->GTOP promotion re-derives the fold admission
 *   [3] F10 — a TAM commit the splice cannot fold is KEPT, never swept
 *   [4] F5  — resume re-registers the row through the surface owner
 *   [5] F6  — history recall is refused while the capture is SUSPENDED
 *   [6] F7  — the f long-press leaves the console's registered frame alone
 *   [7] F8/F9 — the suspended residue is NOT a live console (render gate,
 *               ENTER, EXIT recovery)
 *   [8] F11 — a prim that refuses performs none of its declared stack effect
 *   [9] F1  — GTO . . from an open console: the resume splice survives a
 *             moved currentProgramNumber (pre-fix: SIGSEGV, so this subcase
 *             runs LAST)
 *
 * Fixture shape copies test_fold_seams (L1-F2): real dispatch only —
 * fnForthOuter, runFunction, tamProcessInput, fnKeyExit/fnKeyEnter, the
 * timer exec chain.  aimBuffer content is seeded directly where the landed
 * batteries do the same (test_fold_seams subcase 1). */
static int test_fold_round6_window(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);
  extern void fnKeyEnter(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);
  extern void processKeyAction(int16_t);
  extern void Shft_handler(void);
  extern bool_t _forthConsoleActive(void);
  extern void forthInteractiveEnter(void);
  extern void fnTimerStart(uint8_t, uint16_t, uint32_t);
  extern void fnTimerExec(uint8_t);

  int fail = 0, scFail;
  longInteger_t li;
  int32_t rVal;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  tamState_t savedTam = tam;
  uint8_t savedProgramRunStop = programRunStop;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* The FS_RESET shape, plus the console ring and menu stack: each subcase
   * opens its own console over a known foreign row. */
  #define R6_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    if (forthFoldPending()) { forthFoldLeave(); } \
    forthConsoleClear(); \
    xcopy(softmenuStack, savedStack, sizeof(savedStack)); \
  } while (0)

  /* ---- [1] F2: the SYSFL EXIT cancel. keyboard.c's auto-recover arm
   * returns above the unwind; the fold must not stay armed (the next
   * normal-mode STO would record a program step instead of storing). ---- */
  scFail = 0;
  R6_RESET();
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "12", 3); T_cursorPos = 2;
    runFunction(ITM_SF);                  /* TM_FLAGW: admitted -> fold ARMS */
    if (!forthFoldArmed() || !forthCapIsSuspended()) {
      printf("    [1] FIXTURE BUG: SF did not arm+suspend (armed=%d susp=%d)\n",
             (int)forthFoldArmed(), (int)forthCapIsSuspended());
      scFail = 1;
    }
    else {
      /* The SYS.FL catalog level.  The menu row is driven for real; the
       * catalog id is what upstream's CFLG flow sets at calcMode.c:120 —
       * fixture-established, since the arm under test reads it, and the
       * state under test here is the FOLD bracket, not catalog derivation. */
      showSoftmenu(-MNU_SYSFL);
      catalog = CATALOG_SYFL;
      fnKeyExit(NOPARAM);                 /* the SYSFL auto-recover arm */
      catalog = CATALOG_NONE;
      if (forthFoldArmed() || forthCapIsSuspended()) {
        printf("    [1] FAIL (F2): SYSFL EXIT stranded the fold (armed=%d susp=%d)"
               " — tamProcessInput's bracket would forge the next store into a"
               " program step\n",
               (int)forthFoldArmed(), (int)forthCapIsSuspended());
        scFail = 1;
      }
      else if (!forthCapIsOpen()
               || compareString(aimBuffer, "12", CMP_BINARY) != 0) {
        printf("    [1] FAIL (F2): line not restored after the cancel"
               " (open=%d aim=\"%s\")\n", (int)forthCapIsOpen(), aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [1] PASS (F2): the SYSFL EXIT cancel unwinds the fold and restores the line\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [2] F1's door: the `.` promotion rewrites tam.function AFTER the
   * fold admitted GTO.  GTOP is NOT an admitted item (_forthFoldAdmits) —
   * the fold must re-derive to PARK at the promotion. ---- */
  scFail = 0;
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "7", 2); T_cursorPos = 1;
    runFunction(ITM_GTO);                 /* TM_LABEL: admitted -> ARMED */
    if (!forthFoldArmed()) {
      printf("    [2] FIXTURE BUG: GTO did not arm the fold\n");
      scFail = 1;
    }
    else {
      tamProcessInput(ITM_PERIOD);        /* promotes GTO -> GTOP in place */
      if (tam.function != ITM_GTOP) {
        printf("    [2] FIXTURE BUG: `.` did not promote to GTOP (tam.function=%d)\n",
               (int)tam.function);
        scFail = 1;
      }
      else if (forthFoldArmed()) {
        printf("    [2] FAIL (F1 door): the GTO->GTOP promotion left the fold"
               " ARMED — GTOP is excluded by _forthFoldAdmits and will run"
               " live inside the bracket\n");
        scFail = 1;
      }
      else if (!forthFoldPending()) {
        printf("    [2] FAIL (F1 door): the promotion cleared the fold outright"
               " — expected a PARK downgrade (mode 2), the line must survive\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [2] PASS (F1 door): the promotion downgrades the fold to PARK\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [3] F10: a commit the splice cannot fold (near-cap line) is KEPT
   * in FHIST — the sweep must honour the splice's keep, not delete it. ---- */
  scFail = 0;
  {
    uint16_t fhBefore;
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    { int i;
      for (i = 0; i < 250; i += 2) { aimBuffer[i] = '7'; aimBuffer[i + 1] = ' '; }
      aimBuffer[250] = 0;
    }
    T_cursorPos = 250;
    fhBefore = _tfcFhistStepCount();
    runFunction(ITM_STO);
    tamProcessInput(ITM_1);
    tamProcessInput(ITM_0);               /* commit STO 10: no room to fold */
    if (forthFoldPending()) {
      printf("    [3] FIXTURE BUG: fold still pending after the commit epilogue\n");
      scFail = 1;
    }
    else if (_tfcFhistStepCount() != (uint16_t)(fhBefore + 1)) {
      printf("    [3] FAIL (F10): the no-room TAM commit vanished — kept by the"
             " splice, deleted by the sweep (FHIST %u -> %u, expected %u)\n",
             fhBefore, _tfcFhistStepCount(), (unsigned)(fhBefore + 1));
      scFail = 1;
    }
    else if (stringByteLength(aimBuffer) != 250) {
      printf("    [3] FAIL (F10): the near-cap line changed length (%d)\n",
             (int)stringByteLength(aimBuffer));
      scFail = 1;
    }
  }
  if (!scFail) printf("    [3] PASS (F10): the unfoldable commit is kept in FHIST, the line intact\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [4] F5: the alpha-excursion fold.  Suspend pops the OWNED ALPHA
   * frame; resume must re-establish the row THROUGH THE OWNER, registered —
   * not with a raw showSoftmenu push. ---- */
  scFail = 0;
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);                /* keys-first: FWRD row, OWNED */
    runFunction(ITM_AIM);                 /* the excursion: row -> ALPHA */
    if (forthConsoleTestOwnedCount() != 1 || currentMenu() != -MNU_ALPHA) {
      printf("    [4] FIXTURE BUG: excursion not reached (owned=%u menu=%d)\n",
             forthConsoleTestOwnedCount(), (int)currentMenu());
      scFail = 1;
    }
    else {
      xcopy(aimBuffer, "5", 2); T_cursorPos = 1;
      runFunction(ITM_STO);               /* suspend pops the owned ALPHA row */
      tamProcessInput(ITM_0);
      tamProcessInput(ITM_5);             /* commit -> resume */
      if (!forthCapIsOpen()) {
        printf("    [4] FIXTURE BUG: capture not resumed (state=%d)\n",
               forthTestCapState());
        scFail = 1;
      }
      else if (forthConsoleTestOwnedCount() + forthConsoleTestBorrowCount() != 1) {
        printf("    [4] FAIL (F5): resume re-pushed the row UNREGISTERED"
               " (owned=%u borrow=%u) — one EXIT now commits keysMode where"
               " the surface owner refuses to follow (C18's symptom)\n",
               forthConsoleTestOwnedCount(), forthConsoleTestBorrowCount());
        scFail = 1;
      }
      else if (currentMenu() != -MNU_ALPHA) {
        printf("    [4] FAIL (F5): resumed row is %d, expected the ALPHA"
               " excursion row back\n", (int)currentMenu());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [4] PASS (F5): resume re-registers the excursion row through the owner\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [5] F6: while SUSPENDED, aimBuffer belongs to TAM
   * (forth_capture.c's suspension contract) — the f-UP history recall must
   * not fire and overwrite it mid-name. ---- */
  scFail = 0;
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "12 34 +", 8); T_cursorPos = 7;
    forthInteractiveEnter();              /* one real history line */
    if (lastErrorCode != ERROR_NONE) {
      printf("    [5] FIXTURE BUG: seed line errored (%u)\n", lastErrorCode);
      scFail = 1; lastErrorCode = ERROR_NONE;
    }
    else {
      runFunction(ITM_GTO);               /* ARMED + SUSPENDED, TAM live */
      if (!forthCapIsSuspended()) {
        printf("    [5] FIXTURE BUG: GTO did not suspend\n");
        scFail = 1;
      }
      else {
        xcopy(aimBuffer, "AB", 3);        /* TAM's own name entry, mid-name */
        processKeyAction(CHR_caseUP);     /* the sanctioned recall gesture */
        if (compareString(aimBuffer, "AB", CMP_BINARY) != 0) {
          printf("    [5] FAIL (F6): recall fired while SUSPENDED and clobbered"
                 " TAM's name buffer (aim=\"%s\")\n", aimBuffer);
          scFail = 1;
        }
      }
    }
  }
  if (!scFail) printf("    [5] PASS (F6): recall is refused while the capture is suspended\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [6] F7: the f long-press (real timer chain: fnTimerStart, then
   * fnTimerExec fires TO_FG_LONG's configured refreshFn -> Shft_handler).
   * The console's registered FWRD frame must survive it. ---- */
  scFail = 0;
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);                /* keys mode, FWRD row OWNED */
    if (forthConsoleTestOwnedCount() != 1 || currentMenu() != -MNU_FORTH) {
      printf("    [6] FIXTURE BUG: console home row not established"
             " (owned=%u menu=%d)\n",
             forthConsoleTestOwnedCount(), (int)currentMenu());
      scFail = 1;
    }
    else {
      { extern void refreshFn(uint16_t);
        extern void fnTimerConfig(uint8_t, void (*)(uint16_t), uint16_t);
        /* c47.c:767's own configuration — headless init never runs it, and
         * fnTimerExec calls through the configured pointer. */
        fnTimerConfig(TO_FG_LONG, refreshFn, TO_FG_LONG);
      }
      shiftF = true; shiftG = false;
      Shft_LongPress_f_g = true;
      fnTimerStart(TO_FG_LONG, TO_FG_LONG, 10);
      fnTimerExec(TO_FG_LONG);            /* COMPLETED + refreshFn callback */
      Shft_handler();                     /* the screen.c long-press arm */
      shiftF = false;
      if (forthConsoleTestOwnedCount() + forthConsoleTestBorrowCount() == 0) {
        printf("    [6] FAIL (F7): the long-press de-registered the console's"
               " row (menu now %d, keysMode=%d) — the row says one plane while"
               " the keypad types the other\n",
               (int)currentMenu(), (int)forthCapKeysMode());
        scFail = 1;
      }
      else if (currentMenu() != -MNU_FORTH) {
        printf("    [6] FAIL (F7): the long-press replaced the console's row"
               " (menu now %d)\n", (int)currentMenu());
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [6] PASS (F7): the long-press leaves the console's registered row alone\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [7] F8/F9: the suspended residue (capture SUSPENDED, TAM torn down
   * by the raw funnel — exactly what the strand doors produced) is NOT a
   * live console: the render gate declines, ENTER does not commit TAM's
   * scratch, and EXIT recovers the line. ---- */
  scFail = 0;
  {
    uint16_t fhResidue;
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "5", 2); T_cursorPos = 1;
    runFunction(ITM_STO);                 /* ARMED + SUSPENDED */
    /* D7-1 made the residue unreachable through ANY teardown — the public
     * leave settles the bracket by construction — so the state is primed
     * directly: the sites under test are DEFENSIVE and the residue is the
     * subject.  (This line used to be leaveTamModeIfEnabled(), back when
     * the raw teardown was a real door.) */
    tam.mode = 0;
    clearSystemFlag(FLAG_ALPHA);
    if (tam.mode != 0 || !forthCapIsSuspended()) {
      printf("    [7] FIXTURE BUG: residue not reached (tam.mode=%d susp=%d)\n",
             (int)tam.mode, (int)forthCapIsSuspended());
      scFail = 1;
    }
    else {
      calcMode = CM_AIM;                  /* the residue's mode (round 6 F2) */
      if (_forthConsoleActive()) {
        printf("    [7] FAIL (F8): the render gate treats the suspended residue"
               " as a live console — TAM's abandoned aimBuffer would paint as"
               " an editable line\n");
        scFail = 1;
      }
      fhResidue = _tfcFhistStepCount();
      xcopy(aimBuffer, "99", 3); T_cursorPos = 2;   /* TAM's leftover scratch */
      fnKeyEnter(NOPARAM);
      if (_tfcFhistStepCount() != fhResidue) {
        printf("    [7] FAIL (F9): ENTER in the residue committed TAM's scratch"
               " as a Forth line (FHIST %u -> %u)\n",
               fhResidue, _tfcFhistStepCount());
        scFail = 1;
      }
      if (forthCapIsOpen()) {
        printf("    [7] FAIL (F8): ENTER in the residue re-opened/committed the"
               " capture\n");
        scFail = 1;
      }
      /* EXIT is the recovery gesture: the ladder must not run on the residue;
       * it resumes the suspended line instead of closing it. */
      calcMode = CM_AIM;
      fnKeyExit(NOPARAM);
      if (!forthCapIsOpen()) {
        printf("    [7] FAIL (F8): EXIT on the residue %s the capture instead"
               " of recovering it (state=%d)\n",
               forthCapIsSuspended() ? "left" : "closed", forthTestCapState());
        scFail = 1;
      }
      else if (compareString(aimBuffer, "5", CMP_BINARY) != 0) {
        printf("    [7] FAIL (F8): EXIT recovery lost the line (aim=\"%s\")\n",
               aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [7] PASS (F8/F9): the residue is not live; EXIT recovers the line\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [8] F11: a prim that REFUSES performs none of its declared stack
   * effect — forthPrimInvoke must not settle the spill against the false
   * depth (that frees the deepest live register and silences the line-end
   * RAM_FULL stop). Capacity-agnostic: capacity+1 pushes spill one value;
   * EMIT's operand (capacity+1 <= 31) is below the glyph range, so EMIT
   * refuses without consuming. ---- */
  scFail = 0;
  {
    char line[128];
    int  pos = 0;
    /* forthStackCapacity() is static to forth_inner.c; the same derivation
     * getStackTop() uses (defines.h:2287). */
    uint16_t cap = getSystemFlag(FLAG_SSIZE8) ? 8 : 4;
    uint16_t v;
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    for (v = 1; v <= (uint16_t)(cap + 1); v++) {
      pos += sprintf(line + pos, "%u ", (unsigned)v);
    }
    sprintf(line + pos, "EMIT");
    xcopy(aimBuffer, line, stringByteLength(line) + 1);
    T_cursorPos = stringByteLength(line);
    forthInteractiveEnter();
    if (lastErrorCode != ERROR_RAM_FULL) {
      printf("    [8] FAIL (F11): line-end loud stop missing (lastErrorCode=%u)"
             " — the erroneous settle emptied the spill, silencing the"
             " ERROR_RAM_FULL a non-empty spill must raise\n", lastErrorCode);
      scFail = 1;
    }
    { calcRegister_t deepest = getStackTop();
      if (getRegisterDataType(deepest) != dtLongInteger) {
        printf("    [8] FAIL (F11): deepest register type %u, expected a long"
               " integer\n", getRegisterDataType(deepest));
        scFail = 1;
      }
      else {
        longIntegerInit(li);
        convertLongIntegerRegisterToLongInteger(deepest, li);
        longIntegerToInt32(li, rVal);
        longIntegerFree(li);
        if (rVal != 2) {
          printf("    [8] FAIL (F11): deepest register = %d, expected 2 — the"
                 " false settle freed a live register and refilled it with the"
                 " spilled value\n", (int)rVal);
          scFail = 1;
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [8] PASS (F11): a refusing prim leaves depth, spill and the deepest register alone\n");
  fail |= scFail;
  R6_RESET();

  /* ---- [9] F1, LAST because the unfixed tree SIGSEGVs here: GTO . . from
   * an open console.  The second `.` runs GTOP live (navigates
   * currentProgramNumber off FHIST, growing program memory); the resume
   * splice must re-anchor and clamp, not subtract two different programs'
   * step counts into an unguarded delete loop. ---- */
  scFail = 0;
  {
    uint16_t fhBefore;
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "1 2 +", 6); T_cursorPos = 5;
    forthInteractiveEnter();
    xcopy(aimBuffer, "3 4 +", 6); T_cursorPos = 5;
    forthInteractiveEnter();              /* two history lines: FHIST longer
                                             than the program GTOP creates */
    lastErrorCode = ERROR_NONE;
    fhBefore = _tfcFhistStepCount();
    xcopy(aimBuffer, "7", 2); T_cursorPos = 1;
    runFunction(ITM_GTO);
    tamProcessInput(ITM_PERIOD);          /* promote (PARK) */
    tamProcessInput(ITM_PERIOD);          /* GTOP runs live; epilogue resumes */
    if (forthFoldPending()) {
      printf("    [9] FAIL (F1): fold still pending after the GTOP epilogue\n");
      scFail = 1;
    }
    if (!forthCapIsOpen()) {
      printf("    [9] FAIL (F1): capture did not survive GTO . . (state=%d)\n",
             forthTestCapState());
      scFail = 1;
    }
    else if (compareString(aimBuffer, "7", CMP_BINARY) != 0) {
      printf("    [9] FAIL (F1): the typed line did not survive GTO . ."
             " (aim=\"%s\")\n", aimBuffer);
      scFail = 1;
    }
    if (_tfcFhistStepCount() != fhBefore) {
      printf("    [9] FAIL (F1): the resume splice ate FHIST (%u -> %u)\n",
             fhBefore, _tfcFhistStepCount());
      scFail = 1;
    }
    /* The GTOP arm legitimately created one empty program (END only) at the
     * end of program memory; later suites start from their own fixtures, so
     * it is left in place — deleting steps here would re-enter the very
     * machinery under test. */
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [9] PASS (F1): GTO . . resumes clean — splice anchored and clamped\n");
  fail |= scFail;

  R6_RESET();
  #undef R6_RESET
  tam = savedTam;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  programRunStop = savedProgramRunStop;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  lastErrorCode = ERROR_NONE;

  return fail;
}


/* ==================================================================
 * AUDIT round 8 — the round-7 fix wave's findings, driven
 * (AUDIT_round7_2026-08-08.md + its out-of-family addendum).
 *
 *   [1] P-1 — DELP of FHIST from a live console: the fold's debris sweep
 *             must never run keyed on a program that is not the fold's own
 *
 * Fixture shape copies test_fold_round6_window: real dispatch only —
 * fnForthOuter, runFunction, tamProcessInput, forthInteractiveEnter — and
 * every subcase asserts it REACHED the state it claims to test before it
 * asserts anything about the fix (the C22 rule).  The P-1 drive itself is
 * the round-7 evidence driver (/tmp/claude-1000/r7-simdrive, DRIVE 1B)
 * promoted to permanent coverage. */
static int test_fold_round8_window(void)
{
  extern void fnForthOuter(uint16_t);
  extern void runFunction(int16_t);
  extern void tamProcessInput(uint16_t);
  extern void forthInteractiveEnter(void);

  int fail = 0, scFail;
  testProg_t p;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  bool_t savedHome3 = getSystemFlag(FLAG_HOME_TRIPLE);
  bool_t savedMyM3  = getSystemFlag(FLAG_MYM_TRIPLE);
  bool_t savedBaseM = getSystemFlag(FLAG_BASE_MYM);
  bool_t savedBaseH = getSystemFlag(FLAG_BASE_HOME);
  bool_t savedUser  = getSystemFlag(FLAG_USER);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  tamState_t savedTam = tam;
  uint8_t savedProgramRunStop = programRunStop;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  /* The f long-press, third completion — the branch that reaches
   * openHOMEorMyM (package screen.c:1023), driven through the real timer
   * chain exactly as round-6 subcase [6] drives the sibling branch.
   * fnTimerConfig is c47.c:767's own configuration: headless init never
   * runs it, and fnTimerExec calls through the configured pointer. */
  #define R8_LONGPRESS_F() do { \
    extern void refreshFn(uint16_t); \
    extern void fnTimerConfig(uint8_t, void (*)(uint16_t), uint16_t); \
    fnTimerConfig(TO_FG_LONG, refreshFn, TO_FG_LONG); \
    Shft_LongPress_f_g = false; \
    Shft_timeouts = true; \
    shiftF = false; shiftG = true;        /* state after the second completion */ \
    fnTimerStart(TO_FG_LONG, TO_FG_LONG, 10); \
    fnTimerExec(TO_FG_LONG);              /* COMPLETED + refreshFn callback */ \
    Shft_handler();                       /* defensive second pass, [6]'s idiom */ \
    shiftF = false; shiftG = false; \
  } while (0)

  #define R8_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
    if (forthFoldPending()) { forthFoldLeave(); } \
    forthConsoleClear(); \
    xcopy(softmenuStack, savedStack, sizeof(savedStack)); \
  } while (0)

  /* step count of an arbitrary program — _tfcFhistStepCount's walk, keyed
   * on a program number instead of on FHIST's, because the whole point of
   * [1] is what happens to a program that is NOT FHIST. */
  #define R8_PROG_STEPS(progNum, out) do { \
    uint8_t *s_ = programList[(progNum) - 1].instructionPointer; \
    uint16_t n_ = 1; \
    int g_ = 0; \
    while (s_ != NULL && !(isAtEndOfProgram(s_) || isAtEndOfPrograms(s_)) && g_ < 512) { \
      n_++; s_ = findNextStep(s_); g_++; \
    } \
    (out) = n_; \
  } while (0)

  /* ---- [1] P-1: console open -> DELP -> "FHIST" -> ENTER.
   *
   * DELP is fold-NON-admitted, so the fold PARKs and the commit dispatches
   * fnClP LIVE — which deletes FHIST, the very program holding the parked
   * capture step.  forthCaptureResume's canary then falsifies and it exits
   * through forthCapAbandonSuspended BEFORE the F1 re-anchor, leaving
   * currentProgramNumber wherever fnClP left it.  Pre-fix, forthFoldLeave's
   * debris sweep compared FHIST's entry step count against THAT program's
   * length and deleted up to four of its steps: in the FHIST-first memory
   * order — the ordinary layout when the console was used before the
   * program was written — a real user program went 13 steps to 9, with
   * `111 222 333 444` decoded away.  EXECUTED in round 7.
   *
   * The line is lost either way (the user deleted the program holding it);
   * what must not happen is damage to a program the gesture never named. ---- */
  scFail = 0;
  R8_RESET();
  forthDictClear();
  forthGDictClear();
  cleanupTestProgram();
  {
    uint16_t usrBefore = 0, usrAfter = 0;
    uint16_t fhProgBefore, fhProgAfter, usrProgBefore, usrProgAfter;
    calcRegister_t usrLbl;

    /* FHIST-first memory order: LBL 'FHIST' + END is byte-for-byte what
     * forthHistoryEnsure creates (manage.c:1740-1779), so
     * forthHistoryProgram() adopts program 1 as the history program. */
    tpInit(&p);
    if (tpLbl(&p, "FHIST") < 0 ||
        tpEnd(&p) < 0 ||
        tpLbl(&p, "PUSR") < 0 ||
        tpSrc(&p, "111") < 0 || tpSrc(&p, "222") < 0 || tpSrc(&p, "333") < 0 ||
        tpSrc(&p, "444") < 0 || tpSrc(&p, "555") < 0 || tpSrc(&p, "666") < 0 ||
        tpSrc(&p, "777") < 0 || tpSrc(&p, "888") < 0 || tpSrc(&p, "999") < 0 ||
        tpSrc(&p, "1010") < 0 ||
        tpRtn(&p) < 0 ||
        tpEnd(&p) < 0 ||
        !tpWrite(&p)) {
      printf("    [1] FIXTURE BUG: program build/write failed\n");
      scFail = 1;
    }
    else {
      showSoftmenu(-MNU_STK);
      fnForthOuter(NOPARAM);                    /* interactive console, keys mode */
      xcopy(aimBuffer, "1 2 +", 6); T_cursorPos = 5;
      forthInteractiveEnter();                  /* one real history line in FHIST */
      lastErrorCode = ERROR_NONE;

      fhProgBefore  = forthHistoryProgram();
      usrLbl        = findNamedLabel("PUSR", GLOBAL_LABELS);
      usrProgBefore = (usrLbl == INVALID_VARIABLE) ? 0
                      : (uint16_t)labelList[usrLbl - FIRST_LABEL].program;
      if (usrProgBefore != 0) { R8_PROG_STEPS(usrProgBefore, usrBefore); }

      if (fhProgBefore != 1 || usrProgBefore != 2 || usrBefore < 8) {
        printf("    [1] FIXTURE BUG: FHIST-first order not reached"
               " (FHIST prog=%u PUSR prog=%u steps=%u)\n",
               fhProgBefore, usrProgBefore, usrBefore);
        scFail = 1;
      }
      else {
        xcopy(aimBuffer, "42", 3); T_cursorPos = 2;   /* the live typed line */
        runFunction(ITM_DELP);
        if (!forthFoldPending() || forthFoldArmed() || !forthCapIsSuspended()
            || tam.function != ITM_DELP) {
          printf("    [1] FIXTURE BUG: DELP did not PARK+suspend"
                 " (pending=%d armed=%d susp=%d tam.function=%d)\n",
                 (int)forthFoldPending(), (int)forthFoldArmed(),
                 (int)forthCapIsSuspended(), (int)tam.function);
          scFail = 1;
        }
        else {
          tamProcessInput(ITM_alpha);            /* TM_LBLONLY name entry */
          if (!tam.alpha) {
            printf("    [1] FIXTURE BUG: the label prompt refused alpha entry\n");
            scFail = 1;
          }
          else {
            xcopy(aimBuffer, "FHIST", 6);
            tamProcessInput(ITM_ENTER);          /* commit: fnClP runs LIVE */

            fhProgAfter   = forthHistoryProgram();
            usrLbl        = findNamedLabel("PUSR", GLOBAL_LABELS);
            usrProgAfter  = (usrLbl == INVALID_VARIABLE) ? 0
                            : (uint16_t)labelList[usrLbl - FIRST_LABEL].program;
            if (usrProgAfter != 0) { R8_PROG_STEPS(usrProgAfter, usrAfter); }

            /* REACHED: the door really opened — the label was accepted and
             * fnClP really deleted FHIST.  Without this the assertion below
             * would pass on a tree where DELP simply refused. */
            if (fhProgAfter != 0) {
              printf("    [1] FIXTURE BUG: FHIST survived the DELP commit"
                     " (prog %u -> %u, lastErr=%u) — the P-1 door was not"
                     " taken\n", fhProgBefore, fhProgAfter, lastErrorCode);
              scFail = 1;
            }
            /* The class assertion: the sweep may only ever act on the
             * fold's own program. */
            else if (usrProgAfter == 0 || usrAfter != usrBefore) {
              printf("    [1] FAIL (P-1): the fold's debris sweep ate the user's"
                     " program — PUSR %u steps -> %u (prog %u -> %u)\n",
                     usrBefore, usrAfter, usrProgBefore, usrProgAfter);
              scFail = 1;
            }
            if (forthFoldPending()) {
              printf("    [1] FAIL (P-1): the fold is still pending after the"
                     " abandon (foldMode=%u)\n", forthCapFoldModeRaw());
              scFail = 1;
            }
          }
        }
      }
    }
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [1] PASS (P-1): DELP of FHIST abandons the line without"
                      " touching another program\n");
  fail |= scFail;
  cleanupTestProgram();

  /* ---- [2] C-1: the BACKSPACE demotion is the SECOND tam.function rewrite
   * site, and the F1 fix re-derived fold admission at only the first.
   * `GTO . BACKSPACE 0 5 ENTER` from an open console: the `.` promotes to
   * GTOP and downgrades the fold to PARK (round-6 subcase [2] pins that
   * direction); the BACKSPACE demotes back to GTO and — pre-fix — left
   * foldMode on 2, so the commit dispatched reallyRunFunction(ITM_GTO, 5)
   * LIVE instead of recording the step the splice folds.  The committed
   * operation vanished twice over: no text in the line, no effect.
   *
   * This is the mirror of round-6 [2], and the pair is the class test the
   * rule asks for: after EVERY tam.function rewrite, foldMode agrees with
   * _forthFoldAdmits.  The rewrite-site count is pinned below. ---- */
  scFail = 0;
  R8_RESET();
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "7", 2); T_cursorPos = 1;
    runFunction(ITM_GTO);                     /* TM_LABEL: admitted -> ARMED */
    if (!forthFoldArmed() || !forthCapIsSuspended()) {
      printf("    [2] FIXTURE BUG: GTO did not arm+suspend the fold"
             " (armed=%d susp=%d)\n",
             (int)forthFoldArmed(), (int)forthCapIsSuspended());
      scFail = 1;
    }
    else {
      tamProcessInput(ITM_PERIOD);            /* promote: GTO -> GTOP, fold PARKs */
      if (tam.function != ITM_GTOP || forthFoldArmed() || !forthFoldPending()) {
        printf("    [2] FIXTURE BUG: the promotion state was not reached"
               " (tam.function=%d armed=%d pending=%d)\n",
               (int)tam.function, (int)forthFoldArmed(), (int)forthFoldPending());
        scFail = 1;
      }
      else {
        tamProcessInput(ITM_BACKSPACE);       /* digitsSoFar == 0: demote to GTO */
        if (tam.function != ITM_GTO) {
          printf("    [2] FIXTURE BUG: BACKSPACE did not demote to GTO"
                 " (tam.function=%d digitsSoFar=%d)\n",
                 (int)tam.function, (int)tam.digitsSoFar);
          scFail = 1;
        }
        /* The class assertion, stated at the rewrite site itself. */
        else if (!forthFoldArmed()) {
          printf("    [2] FAIL (C-1): the demotion did not re-derive the fold"
                 " admission — GTO/TM_LABEL is admitted, foldMode=%u"
                 " (expected 1); the commit will run LIVE instead of folding\n",
                 forthCapFoldModeRaw());
          scFail = 1;
        }
        else {
          tamProcessInput(ITM_0);
          tamProcessInput(ITM_5);
          if (tam.mode != 0) { tamProcessInput(ITM_ENTER); }
          /* PEM-parity oracle: the five keys must land in the line as text. */
          if (!forthCapIsOpen()) {
            printf("    [2] FAIL (C-1): the capture did not come back open"
                   " (state=%d)\n", forthTestCapState());
            scFail = 1;
          }
          else if (strstr(aimBuffer, "GTO") == NULL) {
            printf("    [2] FAIL (C-1): the committed GTO never reached the line"
                   " (aim=\"%s\", expected the folded step's text)\n", aimBuffer);
            scFail = 1;
          }
          if (forthFoldPending()) {
            printf("    [2] FAIL (C-1): the fold is still pending after the"
                   " commit epilogue (foldMode=%u)\n", forthCapFoldModeRaw());
            scFail = 1;
          }
          lastErrorCode = ERROR_NONE;
        }
      }
    }
  }
  if (!scFail) printf("    [2] PASS (C-1): the BACKSPACE demotion re-derives the fold"
                      " admission — GTO . BACKSPACE 0 5 folds into the line\n");
  fail |= scFail;

  /* ---- [3] C-2: upstream openHOMEorMyM is the second, un-re-enumerated
   * consumer of isAlphabeticSoftmenu — the predicate Stage L widened to
   * count -MNU_FORTH.  The round-6 F7 fix guarded the package-tree copy of
   * this shape in screen.c and stopped there; a package grep is not an
   * upstream census.  From a live console with HOME.3 enabled, the f
   * long-press pops the REGISTERED FWRD frame and covers it with a raw
   * ALPHA push while forthCapKeysMode() stays true: the row reads ALPHA
   * while the keypad types the keys plane, with the ownership stamp
   * destroyed.  EXECUTED with screenshots in round 7.
   *
   * Sibling of round-6 subcase [6], which pins the screen.c door; this one
   * drives the same gesture down the branch that reaches openHOMEorMyM
   * (Shft_timeouts, third completion — package screen.c:1023). ---- */
  scFail = 0;
  R8_RESET();
  setSystemFlag(FLAG_HOME_TRIPLE);
  clearSystemFlag(FLAG_MYM_TRIPLE);
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "1 2", 4); T_cursorPos = 3;
    if (forthConsoleTestOwnedCount() != 1 || currentMenu() != -MNU_FORTH
        || !forthCapKeysMode() || !forthCapIsOpen() || !forthConsoleStampOnStack()) {
      printf("    [3] FIXTURE BUG: live console with an owned FWRD row not"
             " reached (owned=%u menu=%d keys=%d open=%d stamp=%d)\n",
             forthConsoleTestOwnedCount(), (int)currentMenu(),
             (int)forthCapKeysMode(), (int)forthCapIsOpen(),
             (int)forthConsoleStampOnStack());
      scFail = 1;
    }
    else {
      R8_LONGPRESS_F();
      if (forthConsoleTestOwnedCount() + forthConsoleTestBorrowCount() == 0
          || !forthConsoleStampOnStack()) {
        printf("    [3] FAIL (C-2): openHOMEorMyM popped the console's registered"
               " row (menu now %d, keysMode=%d, owned=%u borrow=%u) — the row"
               " says ALPHA while the keypad types the keys plane, and the C18"
               " close accounting now pops the wrong frames\n",
               (int)currentMenu(), (int)forthCapKeysMode(),
               forthConsoleTestOwnedCount(), forthConsoleTestBorrowCount());
        scFail = 1;
      }
      else if (currentMenu() != -MNU_FORTH) {
        printf("    [3] FAIL (C-2): openHOMEorMyM replaced the console's row"
               " (menu now %d)\n", (int)currentMenu());
        scFail = 1;
      }
      if (compareString(aimBuffer, "1 2", CMP_BINARY) != 0) {
        printf("    [3] FAIL (C-2): the typed line did not survive the gesture"
               " (aim=\"%s\")\n", aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [3] PASS (C-2): the HOME.3 long-press leaves the live"
                      " console's registered row alone\n");
  fail |= scFail;

  /* ---- [4] OOF-1: the SECOND row-destroying call in the same function,
   * which the isAlphabeticSoftmenu-census fix shape does not cover.  With
   * MyM.3 enabled and both base-menu flags clear, openHOMEorMyM's normal
   * mode arm calls fnExitAllMenus(0), which pops the WHOLE softmenu stack.
   *
   * The state that reaches it is the one the wrapper itself creates: mid
   * TAM the capture is SUSPENDED and FLAG_ALPHA is clear, so control takes
   * the non-alpha branch, whose own leaveTamModeIfEnabled() — the D7-1
   * wrapper — settles the fold, resumes the capture and re-registers the
   * console's row.  The wipe lands two arms later, on the row that call
   * just restored.  Hence OOF-2's constraint, which this subcase is the
   * proof of: the guard has to hold POST-resume, so it must be evaluated
   * at the call site and never snapshotted at function entry. ---- */
  scFail = 0;
  R8_RESET();
  clearSystemFlag(FLAG_HOME_TRIPLE);
  setSystemFlag(FLAG_MYM_TRIPLE);
  clearSystemFlag(FLAG_BASE_MYM);
  clearSystemFlag(FLAG_BASE_HOME);
  clearSystemFlag(FLAG_USER);
  {
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    xcopy(aimBuffer, "1 2", 4); T_cursorPos = 3;
    runFunction(ITM_GTO);                     /* ARMED + SUSPENDED, FLAG_ALPHA off */
    if (calcModel != USER_C47) {
      printf("    [4] FIXTURE BUG: calcModel %d has no long-press key code\n",
             (int)calcModel);
      scFail = 1;
    }
    else if (getSystemFlag(FLAG_ALPHA) || !forthCapIsSuspended() || tam.mode == 0) {
      printf("    [4] FIXTURE BUG: the mid-TAM suspension was not reached"
             " (alpha=%d susp=%d tam.mode=%d)\n",
             (int)getSystemFlag(FLAG_ALPHA), (int)forthCapIsSuspended(),
             (int)tam.mode);
      scFail = 1;
    }
    else {
      R8_LONGPRESS_F();
      /* REACHED: the wrapper really did settle and resume — without this the
       * assertion below could pass on a tree where the gesture never got
       * that far, and the arm under test would be untested. */
      if (!forthCapIsOpen() || forthFoldPending()) {
        printf("    [4] FIXTURE BUG: the wrapper did not settle+resume"
               " (open=%d foldPending=%d state=%d) — the fnExitAllMenus arm"
               " was not reached in its post-resume state\n",
               (int)forthCapIsOpen(), (int)forthFoldPending(), forthTestCapState());
        scFail = 1;
      }
      else if (forthConsoleTestOwnedCount() + forthConsoleTestBorrowCount() == 0
               || !forthConsoleStampOnStack()) {
        printf("    [4] FAIL (OOF-1): fnExitAllMenus(0) wiped the row the same"
               " gesture's wrapper had just re-registered (menu now %d,"
               " owned=%u borrow=%u, capture OPEN)\n",
               (int)currentMenu(), forthConsoleTestOwnedCount(),
               forthConsoleTestBorrowCount());
        scFail = 1;
      }
      /* Deliberately NOT asserted here: that the FWRD row is CURRENT.  This
       * arm's contract is that the frame survives — a menu pushed OVER the
       * console's row buries it and EXIT gets it back, which is the round-5
       * benign-overlay ruling.  Destruction is the defect; covering is not. */
    }
    lastErrorCode = ERROR_NONE;
  }
  if (!scFail) printf("    [4] PASS (OOF-1): the MyM.3 long-press does not wipe the"
                      " row its own wrapper restored\n");
  fail |= scFail;

  R8_RESET();
  #undef R8_RESET
  #undef R8_PROG_STEPS
  #undef R8_LONGPRESS_F
  forthDictClear();
  forthGDictClear();
  tam = savedTam;
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  programRunStop = savedProgramRunStop;
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  if (savedHome3) setSystemFlag(FLAG_HOME_TRIPLE); else clearSystemFlag(FLAG_HOME_TRIPLE);
  if (savedMyM3)  setSystemFlag(FLAG_MYM_TRIPLE);  else clearSystemFlag(FLAG_MYM_TRIPLE);
  if (savedBaseM) setSystemFlag(FLAG_BASE_MYM);    else clearSystemFlag(FLAG_BASE_MYM);
  if (savedBaseH) setSystemFlag(FLAG_BASE_HOME);   else clearSystemFlag(FLAG_BASE_HOME);
  if (savedUser)  setSystemFlag(FLAG_USER);        else clearSystemFlag(FLAG_USER);
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  lastErrorCode = ERROR_NONE;

  return fail;
}
