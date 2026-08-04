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

      testInitVariableSoftmenu(22);

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

    testInitVariableSoftmenu(22);

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
      /* sc4 (E13 interim): a TAM round-trip returns to alpha input.
       * Suspend clears the bit; resume rebuilds the ALPHA menu. */
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
        else if (forthCapKeysMode()) {
          printf("    [4] FAIL: keys-mode bit survived the suspend\n");
          scFail = 1;
        }
        else {
          fnKeyExit(NOPARAM);                 /* cancel the TAM session */
          if (forthTestCapState() != FCAP_OPEN) {
            printf("    [4] FAIL: capture state %d after resume, expected FCAP_OPEN\n",
                   forthTestCapState());
            scFail = 1;
          }
          else if (forthCapKeysMode()) {
            printf("    [4] FAIL: resumed capture is not in alpha input\n");
            scFail = 1;
          }
          else if (currentMenu() != -MNU_ALPHA) {
            printf("    [4] FAIL: currentMenu() = %d after resume, expected -MNU_ALPHA\n",
                   currentMenu());
            scFail = 1;
          }
        }
        if (!scFail) {
          printf("    [4] PASS: a TAM round-trip from keys mode resumes in alpha input\n");
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
      testInitVariableSoftmenu(22);

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
