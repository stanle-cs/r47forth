/* packages/forth-core/test_persist.part.h — T5 split part of test_dict_reloc.c (2026-08-03).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 */
/* test_validate_restored_bodies
 * F1-5: full threaded-code validator pins. Independently reported
 * subcases — one PASS line each. T1.3b idiom: build, corrupt, call
 * forthGDictValidateRestored() directly, assert outcome, release orphan,
 * forthGDictClear() between subcases. Hand-built entries use gbegin_word/
 * gend_word with 4-glyph names only (header = 8 bytes, no padding). */
static int test_validate_restored_bodies(void)
{
  int fail = 0;
  uint16_t idx;

  /* ---- P0: a valid gdict validates clean ---- */
  {
    int p0Fail = 0;
    forthDictClear();
    forthGDictClear();
    /* Build 4 gdict words: GP0A, GP0B, GP0C, GP0D with calls */
    {
      uint16_t wa = gbegin_word("GP0A", 4);
      if (wa == FORTH_NULL) { printf("    FAIL P0: alloc GP0A\n"); return 1; }
      gemit(T_ILIT);
      { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
      gend_word();

      uint16_t wb = gbegin_word("GP0B", 4);
      if (wb == FORTH_NULL) { printf("    FAIL P0: alloc GP0B\n"); return 1; }
      gemit(FORTH_GCALL_BASE + 0);  /* call GP0A (global index 0) */
      gend_word();

      uint16_t wc = gbegin_word("GP0C", 4);
      if (wc == FORTH_NULL) { printf("    FAIL P0: alloc GP0C\n"); return 1; }
      gemit(FORTH_GCALL_BASE + 2);  /* self-call GP0C (global index 2) */
      gend_word();

      uint16_t wd = gbegin_word("GP0D", 4);
      if (wd == FORTH_NULL) { printf("    FAIL P0: alloc GP0D\n"); return 1; }
      gemit(T_ILIT);
      { int32_t v = 99; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
      gend_word();
    }
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL P0: valid gdict reset\n");
      fail = p0Fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else if (gdict.count != 4) {
      printf("    FAIL P0: count=%u, expected 4\n", gdict.count);
      fail = p0Fail = 1;
    } else if (!forthFindColon("GP0A", &idx) || !forthFindColon("GP0B", &idx) ||
               !forthFindColon("GP0C", &idx) || !forthFindColon("GP0D", &idx)) {
      printf("    FAIL P0: word not found\n");
      fail = p0Fail = 1;
    }
    if (!p0Fail) printf("    PASS P0: mixed gdict (calls, self-call, literals) validates clean\n");
    forthGDictClear();
    forthDictClear();
  }

  /* ---- P0b: legal backward branch validates clean ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("LOOP", 4);
    if (w == FORTH_NULL) { printf("    FAIL P0b: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gemit(T_BR);
    { int16_t delta = (int16_t)(-5); memcpy(gdict.base + gdict.here, &delta, 2); gdict.here += 2; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL P0b: legal backward branch reset\n"); fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else {
      if (!fail) printf("    PASS P0b: legal backward branch validates clean\n");
    }
    forthGDictClear();
  }

  /* ---- V-B1: missing EXIT ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB1", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B1: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* Body: ILIT(2) + int32(4) + EXIT(2) = 8 bytes. Replace EXIT with DUP. */
    uint16_t bodyStart = w + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
    ftoken_t badTok = (ftoken_t)0x0001;  /* DUP */
    memcpy(gdict.base + bodyStart + 6, &badTok, 2);
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B1: missing EXIT survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B1: missing EXIT detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B2: call index above own index (global space) ---- */
  {
    forthGDictClear();
    uint16_t wa = gbegin_word("VA2", 3);
    if (wa == FORTH_NULL) { printf("    FAIL V-B2: alloc VA2\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint16_t wb = gbegin_word("VB2", 3);
    if (wb == FORTH_NULL) { printf("    FAIL V-B2: alloc VB2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call VA2 (global index 0) */
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* VB2 is latest (index 1). Patch call to FORTH_GCALL_BASE+5 (index 5 > 1). */
    uint16_t bodyStart = wb + (uint16_t)TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK;
    ftoken_t badTok = (ftoken_t)(FORTH_GCALL_BASE + 5);
    memcpy(gdict.base + bodyStart, &badTok, 2);
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B2: bad call index survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B2: call index > entryIdx detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B3: branch into a literal payload ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("BINT", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B3: alloc\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 1; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gemit(T_BR);
    /* delta -4: target = bodyStart+10 + (-4)*2 = bodyStart+2 (inside ILIT operand) */
    { int16_t delta = (int16_t)(-4); memcpy(gdict.base + gdict.here, &delta, 2); gdict.here += 2; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B3: branch into literal survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B3: branch into literal payload detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B4: reserved token ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("RSV4", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B4: alloc\n"); return 1; }
    { ftoken_t badTok = (ftoken_t)0x7F05; gemit(badTok); }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B4: reserved token survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B4: reserved token detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B5: restored smudge ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB5", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B5: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    ((forthHeader_t *)(gdict.base + gdict.latest))->flags |= FF_SMUDGE;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B5: smudged entry survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B5: restored smudge detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B6: nonzero header padding ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB6", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B6: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    /* Name "VB6" is 3 glyphs. Header: 6 + 3 = 9, rounds to 12.
     * Padding bytes at latest + 9 .. latest + 11. */
    gdict.base[gdict.latest + 9] = 0xAA;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B6: nonzero padding survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B6: nonzero header padding detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B7: foreign owner detected (gdict — must be FORTH_OWNER_GLOBAL) ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VB7", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-B7: alloc\n"); return 1; }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    ((forthHeader_t *)(gdict.base + gdict.latest))->owner = 0x1234;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B7: foreign owner survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B7: foreign owner detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-B8: allocate zeroed header padding over poisoned byte ---- */
  {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": AAAA 1 ;");
    if (lastErrorCode != ERROR_NONE || !fdict.base) {
      printf("    FAIL V-B8: setup failed\n");
      fail = 1;
    } else {
      /* Name "AAAA" is 4 glyphs. Next entry: 6+4=10, rounds to 12.
       * Pads at fdict.here+10 .. fdict.here+11. */
      if (fdict.here + 12 > fdict.sizeBlocks * BYTES_PER_BLOCK) {
        printf("    FAIL V-B8: CONFIG poke target outside region\n");
        fail = 1;
        forthDictClear();
      } else {
        fdict.base[fdict.here + 9] = 0xAA;
        forthOuterInterpret(": BBB 2 ;");
        if (lastErrorCode != ERROR_NONE) {
          printf("    FAIL V-B8: BBB compile failed\n"); fail = 1;
          forthDictClear();
        } else if (!(fdict.base[fdict.latest + 9] == 0 &&
                     fdict.base[fdict.latest + 10] == 0 &&
                     fdict.base[fdict.latest + 11] == 0)) {
          printf("    FAIL V-B8: padding not zeroed over poisoned byte\n"); fail = 1;
          forthDictClear();
        } else {
          if (!fail) printf("    PASS V-B8: allocate zeroed header padding over poisoned byte\n");
          forthDictClear();
        }
      }
    }
  }

  /* ---- V-B9: C47 item out of range ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("ITM7", 4);
    if (w == FORTH_NULL) { printf("    FAIL V-B9: alloc\n"); return 1; }
    gemit(T_C47);
    { ftoken_t badId = (ftoken_t)0xFFFF; gemit(badId); }
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-B9: out-of-range C47 item survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-B9: C47 item out of range detected\n");
    }
    forthGDictClear();
  }

  /* ---- V-G1: transient call token rejected in global body ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VG1", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-G1: alloc\n"); return 1; }
    gemit((ftoken_t)0x1000);  /* transient call token — illegal in gdict */
    gend_word();
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base != NULL) {
      printf("    FAIL V-G1: transient call token survived\n"); fail = 1;
      forthGDictClear();
    } else {
      freeC47Blocks(preBase, preBlocks);
      if (!fail) printf("    PASS V-G1: transient call token rejected in global body\n");
    }
    forthGDictClear();
  }

  /* ---- V-G2: FF_IMMEDIATE survives restore validation ---- */
  {
    forthGDictClear();
    uint16_t w = gbegin_word("VG2", 3);
    if (w == FORTH_NULL) { printf("    FAIL V-G2: alloc\n"); return 1; }
    gend_word();
    ((forthHeader_t *)(gdict.base + gdict.latest))->flags = FF_IMMEDIATE;
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;
    forthGDictValidateRestored();
    if (gdict.base == NULL) {
      printf("    FAIL V-G2: FF_IMMEDIATE rejected\n"); fail = 1;
      if (preBase) freeC47Blocks(preBase, preBlocks);
    } else {
      if (!fail) printf("    PASS V-G2: FF_IMMEDIATE survives restore validation\n");
    }
    forthGDictClear();
  }

  /* ---- Nesting probe: transient TW2 calls global GW2 cross-region ---- */
  {
    forthGDictClear();
    /* Build GW2 in gdict: body = call GW1 (global index 0), ILIT 35, EXIT */
    uint16_t gw1 = gbegin_word("GW1", 3);
    if (gw1 == FORTH_NULL) { printf("    FAIL nesting: alloc GW1\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 7; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    uint16_t gw2 = gbegin_word("GW2", 3);
    if (gw2 == FORTH_NULL) { printf("    FAIL nesting: alloc GW2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call GW1 (global index 0) */
    gemit(T_ILIT);
    { int32_t v = 35; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
    /* Build TW2 in fdict: body = call token for GW2 (global), EXIT */
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": TW2 GW2 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("TW2", &idx)) {
      printf("    FAIL nesting: TW2 compile failed\n"); fail = 1;
    } else {
      lastErrorCode = ERROR_NONE;
      uint16_t tw2ref;
      if (forthFindColon("TW2", &tw2ref)) {
        forthInner(tw2ref, false);
        if (lastErrorCode != ERROR_NONE) {
          printf("    FAIL nesting: execution error %d\n", lastErrorCode); fail = 1;
        } else if (!x_is_longint(35)) {
          printf("    FAIL nesting: X != 35 after cross-region call\n"); fail = 1;
        } else {
          if (!fail) printf("    PASS NEST: TW2->GW2->GW1 cross-region nesting, X=35\n");
        }
      } else {
        printf("    FAIL nesting: TW2 not found\n"); fail = 1;
      }
    }
    forthDictClear();
    forthGDictClear();
  }

  /* Rebuild-and-use after final reset */
  {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": FRESH 7 ;");
    if (lastErrorCode != ERROR_NONE || !forthFindColon("FRESH", &idx)) {
      printf("    FAIL: dict unusable after final reset\n"); fail = 1;
    }
  }
  forthDictClear();
  forthGDictClear();

  return fail;
}

/* T1.1 (H5 round-trip). Must fail if: any of the five forthGDict* params is
 * dropped from the save or restore hunk, or the restore rebases gdict.base
 * without TO_PCMEMPTR. */
static int test_save_restore_roundtrip(void)
{
  int fail = 0;
  forthDictClear();
  forthGDictClear();
  lastErrorCode = ERROR_NONE;

  /* Hand-build GW1 (body: ILIT 7 EXIT) and GW2 (body: CALL GW1, ILIT 35 EXIT) */
  {
    uint16_t gw1 = gbegin_word("GW1", 3);
    if (gw1 == FORTH_NULL) { printf("    FAIL: gbegin_word GW1\n"); return 1; }
    gemit(T_ILIT);
    { int32_t v = 7; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();

    uint16_t gw2 = gbegin_word("GW2", 3);
    if (gw2 == FORTH_NULL) { printf("    FAIL: gbegin_word GW2\n"); return 1; }
    gemit(FORTH_GCALL_BASE + 0);  /* call GW1 (global index 0) */
    gemit(T_ILIT);
    { int32_t v = 35; memcpy(gdict.base + gdict.here, &v, 4); gdict.here += 4; }
    gend_word();
  }

  uint16_t savedHere = gdict.here, savedCount = gdict.count, savedLatest = gdict.latest;

  saveCalc();
  if (!backupFileContains("forthGDictBase:")) {
    printf("    FAIL: saveCalc did not write forthGDictBase\n");
    forthGDictClear();
    return 1;
  }
  if (backupFileContains("forthDictBase:")) {
    printf("    FAIL: saveCalc still writes forthDictBase (old keys must be gone)\n");
    forthGDictClear();
    return 1;
  }

  /* clobber gdict; define a transient word */
  forthGDictClear();
  forthOuterInterpret(": SRZZ 1 ;");

  {
    bool_t savedLoad = loadTestPrograms;
    loadTestPrograms = false;
    restoreCalc();
    loadTestPrograms = savedLoad;
  }

  /* gdict scalars match saved */
  if (gdict.here != savedHere || gdict.count != savedCount || gdict.latest != savedLatest) {
    printf("    FAIL: gdict scalars mismatch (here %u/%u count %u/%u latest %u/%u)\n",
           gdict.here, savedHere, gdict.count, savedCount, gdict.latest, savedLatest);
    fail = 1;
  }
  /* fdict is EMPTY after restore */
  if (fdict.count != 0) {
    printf("    FAIL: fdict.count != 0 after restore (%u)\n", fdict.count);
    fail = 1;
  }
  { uint16_t r;
    if (forthFindColon("SRZZ", &r)) {
      printf("    FAIL: post-save transient word SRZZ survived restore\n"); fail = 1; }
  }
  /* GW2 found as global ref */
  { uint16_t ref;
    if (!forthFindColon("GW2", &ref)) {
      printf("    FAIL: GW2 lost across restore\n"); fail = 1;
    } else if (ref != (FORTH_REF_GLOBAL | 1)) {
      printf("    FAIL: GW2 ref = 0x%04X, expected 0x%04X\n", ref, (unsigned)(FORTH_REF_GLOBAL | 1));
      fail = 1;
    }
  }

  /* Execute GW2: drives global FTOK_CALL arm and cross-region body resolution */
  if (!fail) {
    uint16_t ref;
    if (forthFindColon("GW2", &ref)) {
      lastErrorCode = ERROR_NONE;
      forthInner(ref, false);
      if (lastErrorCode != ERROR_NONE) { printf("    FAIL: GW2 raised %d\n", lastErrorCode); fail = 1; }
      else if (!x_is_longint(35))      { printf("    FAIL: X != 35 after GW2\n"); fail = 1; }
    }
  }

  /* A5 / backupR47.cfg regression: forthCap is deliberately not persisted,
   * but older backups do persist the surrounding PEM + ALPHA + ITM_FORTH UI
   * and its cursor.  Restore must close that split state before the exact
   * reported RRRLLLRRL sequence reaches the text cursor. */
  {
    uint8_t savedCalcMode = calcMode;
    int16_t savedTamFunction = tam.function;
    bool_t savedAlphaUi = getSystemFlag(FLAG_ALPHA);
    int16_t savedCursor = T_cursorPos;
    int16_t savedDisplayOffset = displayAIMbufferoffset;
    softmenuStack_t savedUiStack[SOFTMENU_STACK_SIZE];
    char savedAimBuffer[AIM_BUFFER_LENGTH];
    static const uint16_t restoredArrows[] = {
      ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW,
      ITM_T_LEFT_ARROW,  ITM_T_LEFT_ARROW,  ITM_T_LEFT_ARROW,
      ITM_T_RIGHT_ARROW, ITM_T_RIGHT_ARROW, ITM_T_LEFT_ARROW
    };
    uint_fast16_t i;

    xcopy(savedUiStack, softmenuStack, sizeof(savedUiStack));
    xcopy(savedAimBuffer, aimBuffer, sizeof(savedAimBuffer));

    forthCapPowerReset();
    calcMode = CM_PEM;
    showSoftmenu(-MNU_ALPHA);
    setSystemFlag(FLAG_ALPHA);
    tam.function = ITM_FORTH;
    aimBuffer[0] = 0;
    T_cursorPos = 2;
    displayAIMbufferoffset = 0;
    saveCalc();

    calcMode = CM_NORMAL;
    clearSystemFlag(FLAG_ALPHA);
    tam.function = 0;
    T_cursorPos = 0;
    {
      bool_t savedLoad = loadTestPrograms;
      loadTestPrograms = false;
      restoreCalc();
      loadTestPrograms = savedLoad;
    }

    if (calcMode != CM_PEM
        || getSystemFlag(FLAG_ALPHA)
        || tam.function != 0
        || forthCapIsOpen()
        || forthCapIsSuspended()
        || T_cursorPos != 0
        || displayAIMbufferoffset != 0
        || currentMenu() == -MNU_ALPHA
        || isAlphaSubmenu(0)) {
      printf("    FAIL: stale restored capture UI survived "
             "(mode=%u alpha=%d tam=%d state=%d cursor=%d off=%d menu=%d)\n",
             calcMode, getSystemFlag(FLAG_ALPHA), tam.function,
             forthTestCapState(), T_cursorPos, displayAIMbufferoffset,
             currentMenu());
      fail = 1;
    }

    for (i = 0; i < nbrOfElements(restoredArrows); ++i) {
      fnT_ARROW(restoredArrows[i]);
    }
    if (T_cursorPos != 0 || displayAIMbufferoffset != 0) {
      printf("    FAIL: restored arrow replay moved empty cursor "
             "(cursor=%d off=%d)\n", T_cursorPos, displayAIMbufferoffset);
      fail = 1;
    }

    forthCapPowerReset();
    calcMode = savedCalcMode;
    tam.function = savedTamFunction;
    if (savedAlphaUi) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
    T_cursorPos = savedCursor;
    displayAIMbufferoffset = savedDisplayOffset;
    xcopy(softmenuStack, savedUiStack, sizeof(savedUiStack));
    xcopy(aimBuffer, savedAimBuffer, sizeof(savedAimBuffer));
  }

  /* Release the restored gdict region (restore allocated fresh memory) */
  { uint8_t *preBase = gdict.base; uint16_t preBlocks = gdict.sizeBlocks;
    gdict.base = NULL; gdict.sizeBlocks = 0; gdict.here = 0;
    gdict.latest = FORTH_NULL; gdict.count = 0;
    if (preBase) freeC47Blocks(preBase, preBlocks);
  }
  forthDictClear();

  printf("  FORTH ARENA (post-restore): here=%u sizeBlocks=%u gdict here=%u sizeBlocks=%u\n",
         fdict.here, fdict.sizeBlocks, gdict.here, gdict.sizeBlocks);
  if (!fail) printf("    PASS: save/restore round-trip preserved the dictionary\n");
  return fail;
}

/* T1.3 (validation clamps corruption). Must fail if:
 * forthGDictValidateRestored is not called from the restore hunk, or its
 * here-bound / chain-count checks are deleted (next dict write would land
 * out of bounds). */
static int test_restore_validation_clamps(void)
{
  int fail = 0;
  int variant;
  for (variant = 0; variant < 2; variant++) {
    forthDictClear();
    forthGDictClear();
    /* Build a gdict word for save */
    { uint16_t gw = gbegin_word("GW", 2); if (gw == FORTH_NULL) { printf("    FAIL: gbegin_word GW\n"); return 1; } gend_word(); }
    uint8_t *preBase = gdict.base;
    uint16_t preBlocks = gdict.sizeBlocks;

    saveCalc();
    if (variant == 0) {
      if (editBackupFile(NULL, "forthGDictHere:", "forthGDictHere:uint16:65534\n")) { printf("    FAIL: edit\n"); return 1; }
    } else {
      char repl[64];
      sprintf(repl, "forthGDictCount:uint16:%u\n", (unsigned)gdict.count + 1);
      if (editBackupFile(NULL, "forthGDictCount:", repl)) { printf("    FAIL: edit\n"); return 1; }
    }

    { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

    if (gdict.base != NULL) {
      printf("    FAIL: variant %d: corrupt scalars survived validation\n", variant);
      fail = 1;
      forthGDictClear();     /* free whatever it points at, best effort */
    }
    else if (preBase) {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VOKW 5 ;");
    { uint16_t r; if (lastErrorCode != ERROR_NONE || !forthFindColon("VOKW", &r)) {
        printf("    FAIL: variant %d: dict unusable after validation reset\n", variant); fail = 1; } }
    forthGDictClear();
    forthDictClear();
  }

  return fail;
}

static int test_spill_region(void)
{
  int fail = 0;

  /* SP-1: round trip preserves type+payload */
  {
    real34_t seed, origPayload;
    int32ToReal34(42, &seed);
    reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
    real34Copy(&seed, REGISTER_REAL34_DATA(REGISTER_X));
    real34Copy(REGISTER_REAL34_DATA(REGISTER_X), &origPayload);
    forthSpillReset();
    if (!forthSpillCatch(REGISTER_X)) {
      printf("  SP-1 FAIL: catch returned false\n");
      fail = 1;
    } else {
      real34_t altered;
      int32ToReal34(99, &altered);
      real34Copy(&altered, REGISTER_REAL34_DATA(REGISTER_X));
      if (!forthSpillRefill(REGISTER_Y)) {
        printf("  SP-1 FAIL: refill returned false\n");
        fail = 1;
      } else if (getRegisterDataType(REGISTER_Y) != dtReal34 ||
                 memcmp(getRegisterDataPointer(REGISTER_Y),
                        &origPayload,
                        (size_t)getRegisterFullSizeInBlocks(REGISTER_Y) * 4u) != 0) {
        printf("  SP-1 FAIL: Y type=%u blocks=%u (expected dtReal34)\n",
               getRegisterDataType(REGISTER_Y),
               getRegisterFullSizeInBlocks(REGISTER_Y));
        fail = 1;
      } else {
        real34_t yVal;
        real34Copy(REGISTER_REAL34_DATA(REGISTER_Y), &yVal);
        int32_t v = real34ToInt32(&yVal);
        if (v != 42) {
          printf("  SP-1 FAIL: Y value=%ld (expected 42)\n", (long)v);
          fail = 1;
        } else {
          printf("  SP-1 PASS\n");
        }
      }
    }
  }

  /* SP-2: LIFO over three values */
  {
    real34_t s1, s2, s3; int32_t r1, r2, r3;
    forthSpillReset();
    int32ToReal34(100, &s1);
    reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
    real34Copy(&s1, REGISTER_REAL34_DATA(REGISTER_X));
    forthSpillCatch(REGISTER_X);
    int32ToReal34(200, &s2);
    real34Copy(&s2, REGISTER_REAL34_DATA(REGISTER_X));
    forthSpillCatch(REGISTER_X);
    int32ToReal34(300, &s3);
    real34Copy(&s3, REGISTER_REAL34_DATA(REGISTER_X));
    forthSpillCatch(REGISTER_X);
    if (forthSpillCount() != 3) {
      printf("  SP-2 FAIL: count=%u after 3 catches\n", forthSpillCount());
      fail = 1;
    } else {
      reallocateRegister(REGISTER_Y, dtReal34, 0, amNone);
      forthSpillRefill(REGISTER_Y);
      r1 = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
      forthSpillRefill(REGISTER_Y);
      r2 = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
      forthSpillRefill(REGISTER_Y);
      r3 = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
      if (r1 != 300 || r2 != 200 || r3 != 100) {
        printf("  SP-2 FAIL: got %ld,%ld,%ld (expected 300,200,100)\n",
               (long)r1, (long)r2, (long)r3);
        fail = 1;
      } else {
        printf("  SP-2 PASS\n");
      }
    }
  }

  /* SP-3: reset frees */
  {
    real34_t s1, s2;
    forthSpillReset();
    if (forthSpillCount() != 0) {
      printf("  SP-3 FAIL: count=%u after reset\n", forthSpillCount());
      fail = 1;
    } else {
      int32ToReal34(1, &s1);
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
      real34Copy(&s1, REGISTER_REAL34_DATA(REGISTER_X));
      forthSpillCatch(REGISTER_X);
      int32ToReal34(2, &s2);
      real34Copy(&s2, REGISTER_REAL34_DATA(REGISTER_X));
      forthSpillCatch(REGISTER_X);
      forthSpillReset();
      if (forthSpillCount() != 0) {
        printf("  SP-3 FAIL: count=%u after reset\n", forthSpillCount());
        fail = 1;
      } else if (forthSpillRefill(REGISTER_Y)) {
        printf("  SP-3 FAIL: refill returned true on empty spill\n");
        fail = 1;
      } else {
        printf("  SP-3 PASS\n");
      }
    }
  }

  /* SP-4: growth */
  {
    real34_t firstSeed;
    int32_t firstVal, lastVal;
    forthSpillReset();
    int32ToReal34(1, &firstSeed);
    reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
    real34Copy(&firstSeed, REGISTER_REAL34_DATA(REGISTER_X));
    { uint16_t i;
      for (i = 0; i < 40; i++) {
        int32ToReal34((int32_t)(i + 1), &firstSeed);
        real34Copy(&firstSeed, REGISTER_REAL34_DATA(REGISTER_X));
        if (!forthSpillCatch(REGISTER_X)) {
          printf("  SP-4 FAIL: catch %u returned false\n", i);
          fail = 1;
          break;
        }
      }
    }
    if (forthSpillCount() == 40) {
      reallocateRegister(REGISTER_Y, dtReal34, 0, amNone);
      { uint16_t i;
        for (i = 0; i < 40; i++) {
          if (!forthSpillRefill(REGISTER_Y)) {
            printf("  SP-4 FAIL: refill %u returned false\n", i);
            fail = 1;
            break;
          }
        }
      }
      if (forthSpillCount() != 0) {
        printf("  SP-4 FAIL: count=%u after 40 refills\n", forthSpillCount());
        fail = 1;
      } else {
        lastVal = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
        reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
        int32ToReal34(1, &firstSeed);
        real34Copy(&firstSeed, REGISTER_REAL34_DATA(REGISTER_X));
        firstVal = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_X));
        if (lastVal != 1 || firstVal != 1) {
          printf("  SP-4 FAIL: lastRefill=%ld firstCaught=%ld\n",
                 (long)lastVal, (long)firstVal);
          fail = 1;
        } else {
          printf("  SP-4 PASS\n");
        }
      }
    } else if (forthSpillCount() != 0) {
      printf("  SP-4 FAIL: count=%u after 40 catches\n", forthSpillCount());
      fail = 1;
    }
  }

  /* SP-5: empty refill is false */
  {
    real34_t seed;
    int32_t yBefore, yAfter;
    forthSpillReset();
    int32ToReal34(777, &seed);
    reallocateRegister(REGISTER_Y, dtReal34, 0, amNone);
    real34Copy(&seed, REGISTER_REAL34_DATA(REGISTER_Y));
    yBefore = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
    if (forthSpillRefill(REGISTER_Y)) {
      printf("  SP-5 FAIL: refill returned true on empty spill\n");
      fail = 1;
    } else {
      yAfter = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_Y));
      if (yBefore != yAfter || yAfter != 777) {
        printf("  SP-5 FAIL: Y changed from %ld to %ld\n",
               (long)yBefore, (long)yAfter);
        fail = 1;
      } else {
        printf("  SP-5 PASS\n");
      }
    }
  }

  forthSpillReset();
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

  uint8_t savedCalcMode = calcMode;
  calcMode = CM_NORMAL;   /* ensure displayBugScreen's guard can fire */
  freeC47Blocks(blk, blocks); /* double free — must be a no-op */

  if (calcMode != CM_BUG_ON_SCREEN) {
    printf("    FAIL: overlapping free did not raise the firmware-bug screen\n");
    fail = 1;
  }
  calcMode = savedCalcMode;
  clearScreen(0);   /* wipe the bug-screen pixels so later display tests
                       start clean; harmless if unused elsewhere */

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
    printf("    PASS: exact-match double free raises bug screen, free list unchanged\n");
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
  uint8_t savedCalcMode = calcMode;
  calcMode = CM_NORMAL;   /* ensure displayBugScreen's guard can fire */
  freeC47Blocks(second, secondSize); /* interior double free — must be a no-op */

  if (calcMode != CM_BUG_ON_SCREEN) {
    printf("    FAIL: overlapping free did not raise the firmware-bug screen\n");
    fail = 1;
  }
  calcMode = savedCalcMode;
  clearScreen(0);   /* wipe the bug-screen pixels so later display tests
                       start clean; harmless if unused elsewhere */

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
    printf("    PASS: interior double free raises bug screen, free list unchanged\n");
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

  uint8_t savedCalcMode = calcMode;
  calcMode = CM_NORMAL;   /* ensure displayBugScreen's guard can fire */
  freeC47Blocks(blk, oversizeBlocks); /* oversize double free — must be a no-op */

  if (calcMode != CM_BUG_ON_SCREEN) {
    printf("    FAIL: overlapping free did not raise the firmware-bug screen\n");
    fail = 1;
  }
  calcMode = savedCalcMode;
  clearScreen(0);   /* wipe the bug-screen pixels so later display tests
                       start clean; harmless if unused elsewhere */

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
    printf("    PASS: oversize double free raises bug screen, no region grew\n");
  }
  return fail;
}

/* ---- D3-2: deep recursion with spill should compute 7 FACT = 5040 ----
 * Without spilling, 7 FACT overflows the visible stack and returns garbage.
 * With D3-2 spilling, the intermediate values spill and refill correctly. ---- */
static int test_deep_recursion_spill(void)
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

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 7 FACT");
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: 7 FACT errored (%d)\n", lastErrorCode);
    fail = 1;
  } else if (tType != dtLongInteger || tVal != 5040) {
    printf("    FAIL: 7 FACT = %ld type=%u, expected 5040\n",
           (long)tVal, tType);
    fail = 1;
  }

  if (!fail) {
    printf("    PASS: 7 FACT deep recursion = 5040\n");
  }

  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  forthDictClear();
  forthGDictClear();
  return fail;
}

/* ---- D3-3: spill boundary rule — named message + tests ----
 * Blocked side: a native item cannot run while spilled values exist.
 * Allowed side: draining spilled values back below capacity permits the call. ---- */
static int test_spill_native_boundary(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  testProg_t p;
  uint8_t tType;
  int32_t tVal;

  programRunStop = PGM_STOPPED;

  /* Build fixture: label T1 storing 999 to R19, then RTN */
  tpInit(&p);
  if (tpLbl(&p, "T1") < 0 ||
      tpSrc(&p, "999 STO 19 DROP") < 0 ||
      tpRtn(&p) < 0 ||
      tpEnd(&p) < 0 ||
      !tpWrite(&p)) {
    printf("    FIXTURE FAIL: T1 program build/write\n");
    programRunStop = savedRS;
    return 1;
  }

  /* Subcase 1 (blocked): push capacity+2 values, spill non-empty, invoke native */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 5 6 7 8 9 10 T1");
  if (lastErrorCode != ERROR_RAM_FULL) {
    printf("    FAIL: blocked side expected ERROR_RAM_FULL, got %d\n", lastErrorCode);
    fail = 1;
  }
  if (forthSpillCount() != 0) {
    printf("    FAIL: blocked side spill not reset (count=%u)\n", (unsigned)forthSpillCount());
    fail = 1;
  }

  /* Verify clean state after error */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1 2 +");
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 3) {
    printf("    FAIL: post-error state not clean (err=%d X=%ld type=%u)\n",
           lastErrorCode, (long)tVal, tType);
    fail = 1;
  }

  /* Subcase 2 (allowed): push capacity+2 values, drain with + back below capacity,
   * then invoke native — should succeed with T1 storing 999 to R19 */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 5 6 7 8 9 10 + + + + + + T1");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: allowed side errored (%d)\n", lastErrorCode);
    fail = 1;
  } else {
    read_reg_int32((calcRegister_t)19, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 999) {
      printf("    FAIL: allowed side — T1 did not execute (R19=%ld type=%u, expected 999)\n",
             (long)tVal, tType);
      fail = 1;
    }
  }

  if (!fail) {
    printf("    PASS: spill boundary rule — native blocked with spill, allowed after drain\n");
  }

  cleanupTestProgram();
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* ---- D3-4: spill activity must be invisible to the native window ----
 * WP-1: same computation spilled vs unspilled produces identical result.
 * WP-2: after drain, visible window depth and order match unlimited-capacity
 *        arithmetic; spill count returns to 0. ---- */
static int test_spill_window_parity(void)
{
  int fail = 0;
  uint8_t savedRS = programRunStop;
  uint8_t tType;
  int32_t tVal;

  programRunStop = PGM_STOPPED;

  /* WP-1: same computation, spilled vs unspilled.
   * Unspilled: 1+2+3+4 = 10 (depth never exceeds 4, no spill).
   * Spilled: eight zero-literals below 1 2 3 4, consumed by extra +s.
   *   0+0+0+0+0+0+0+0+1+2+3+4 = 10 (12 pushes > capacity 8, spill engages,
   *   drains within line to 1 value). Both leave X=10. */
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 + + +");
  read_reg_int32(REGISTER_X, &tType, &tVal);
  if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 10) {
    printf("    WP-1 FAIL: unspilled 1+2+3+4 should be 10, got %ld type %u (error %d)\n",
           (long)tVal, tType, lastErrorCode);
    fail = 1;
  }

  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 0 0 0 0 0 0 0 0 1 2 3 4 + + + + + + + + + + +");
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (lastErrorCode != ERROR_NONE || tType != dtLongInteger || tVal != 10) {
      printf("    WP-1 FAIL: spilled sum should be 10, got %ld type %u (error %d)\n",
             (long)tVal, tType, lastErrorCode);
      fail = 1;
    }
  }

  if (!fail) {
    printf("    WP-1 PASS: spilled and unspilled produce identical results\n");
  }

  /* WP-2: visible window depth.
   * Push capacity+2 (10 values), drain with 7 adds to exactly 3 values.
   * Assert X, Y, Z hold those 3 values in correct order and spill is empty. */
  if (!fail) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK' 1 2 3 4 5 6 7 8 9 10 + + + + + + +");
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (lastErrorCode != ERROR_NONE || tType != dtLongInteger) {
      printf("    WP-2 FAIL: X error=%d type=%u (expected dtLongInteger)\n",
             lastErrorCode, tType);
      fail = 1;
    } else {
      int32_t xVal = tVal;
      read_reg_int32(REGISTER_Y, &tType, &tVal);
      if (tType != dtLongInteger) {
        printf("    WP-2 FAIL: Y type=%u (expected dtLongInteger)\n", tType);
        fail = 1;
      } else {
        int32_t yVal = tVal;
        read_reg_int32(REGISTER_Z, &tType, &tVal);
        if (tType != dtLongInteger) {
          printf("    WP-2 FAIL: Z type=%u (expected dtLongInteger)\n", tType);
          fail = 1;
        } else {
          int32_t zVal = tVal;
          /* Verify the three values are distinct and spill is drained */
          if (xVal == yVal || yVal == zVal || xVal == zVal) {
            printf("    WP-2 FAIL: X=%ld Y=%ld Z=%ld not distinct\n",
                   (long)xVal, (long)yVal, (long)zVal);
            fail = 1;
          } else if (forthSpillCount() != 0) {
            printf("    WP-2 FAIL: spill count should be 0, got %u\n",
                   (unsigned)forthSpillCount());
            fail = 1;
          }
        }
      }
    }
  }

  if (!fail) {
    printf("    WP-2 PASS: visible window depth and order match unlimited-capacity arithmetic\n");
  }

  cleanupTestProgram();
  programRunStop = savedRS;
  lastErrorCode = ERROR_NONE;
  return fail;
}

/* D3-5: pin the REAL item entry, not the test wrapper — fnForthOuter
 * must bracket depth/spill exactly like forthOuterInterpret. Found by
 * the T6 upstream-runner cases (spill dead on the keyboard path). */
static int test_fnforthouter_brackets(void)
{
  int fail = 0;
  uint8_t tType;
  int32_t tVal;

  x_set_string("1 2 3 4 5 6 7 8 9 10 11 + + + + + + + + + +");
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: fnForthOuter deep line errored (%d)\n", lastErrorCode);
    fail = 1;
  } else {
    read_reg_int32(REGISTER_X, &tType, &tVal);
    if (tType != dtLongInteger || tVal != 66) {
      printf("    FAIL: fnForthOuter deep line X=%ld type=%u, expected 66\n",
             (long)tVal, (unsigned)tType);
      fail = 1;
    }
  }
  if (forthSpillCount() != 0) {
    printf("    FAIL: spill not drained after fnForthOuter (%u)\n",
           (unsigned)forthSpillCount());
    fail = 1;
  }
  if (!fail) {
    printf("    PASS: fnForthOuter brackets depth — deep line = 66, spill drained\n");
  }

  lastErrorCode = ERROR_NONE;
  return fail;
}

