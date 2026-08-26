// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyTest.c
 * testSuite coverage drivers for the pretty-print package. Registered in
 * testSuite.c's funcTestNoParam with coverageDriver = 1 and driven by
 * testSuite/tests/pretty_print.txt. Test-only code: never built for the
 * device. Every driver writes its failure count into X as a long integer.
 */

#include "c47.h"
#include "prettyInternal.h"

#if defined(PC_BUILD)

#include <stdio.h>

extern bool_t nimWhenButtonPressed;   // keyboard.c file scope, non-static

static uint32_t ppTestFailures;

static void ppTestFail(const char *what) {
  ppTestFailures++;
  printf("prettyPrint test FAIL: %s\n", what);
}

static void ppTestFailInt(const char *what, int32_t expected, int32_t actual) {
  ppTestFailures++;
  printf("prettyPrint test FAIL: %s (expected %d, actual %d)\n", what, expected, actual);
}

static void ppTestWriteLonI(calcRegister_t regist, uint32_t value) {
  longInteger_t li;
  longIntegerInit(li);
  uInt32ToLongInteger(value, li);
  convertLongIntegerToLongIntegerRegister(li, regist);
  longIntegerFree(li);
}

static void ppTestSetRealX(const char *value) {
  reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
  stringToReal34(value, REGISTER_REAL34_DATA(REGISTER_X));
}


/* ==== prettyTestMeasure ================================================= */

void prettyTestMeasure(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  // M1: FRAC(3,4), numeric context, standard children — the exact numbers
  // the inline X-line pin (P1-P3) is derived from.
  ppReset();
  uint8_t frac = ppNewBox(PP_FRAC, PP_FONT_NUMERIC);
  uint8_t num  = ppNewRun("3", 1, PP_FONT_STANDARD);
  uint8_t den  = ppNewRun("4", 1, PP_FONT_STANDARD);
  ppAppendChild(frac, num);
  ppAppendChild(frac, den);
  if(frac == PP_NONE || num == PP_NONE || den == PP_NONE || !ppMeasure(frac, 0)) {
    ppTestFail("M1 build/measure");
  }
  else {
    const ppNode_t *f = ppNodeAt(frac);
    if(f->width   != 12) ppTestFail("M1 width != 12");
    if(f->ascent  != 25) ppTestFail("M1 ascent != 25");
    if(f->descent !=  5) ppTestFail("M1 descent != 5");
  }

  // M2: HBOX[RUN("1", numeric), FRAC(3,4)] — mixed-number shape.
  ppReset();
  uint8_t box = ppNewBox(PP_HBOX, PP_FONT_NUMERIC);
  uint8_t one = ppNewRun("1", 1, PP_FONT_NUMERIC);
  frac = ppNewBox(PP_FRAC, PP_FONT_NUMERIC);
  num  = ppNewRun("3", 1, PP_FONT_STANDARD);
  den  = ppNewRun("4", 1, PP_FONT_STANDARD);
  ppAppendChild(frac, num);
  ppAppendChild(frac, den);
  ppAppendChild(box, one);
  ppAppendChild(box, frac);
  if(box == PP_NONE || one == PP_NONE || !ppMeasure(box, 0)) {
    ppTestFail("M2 build/measure");
  }
  else {
    const ppNode_t *b = ppNodeAt(box);
    // 26, not 28: stringWidth with showLeadingCols=false drops the '1'
    // glyph's leading empty columns (numericFont '1' measures 14).
    if(b->width   != 26) ppTestFailInt("M2 width",   26, b->width);
    if(b->ascent  != 26) ppTestFailInt("M2 ascent",  26, b->ascent);
    if(b->descent !=  5) ppTestFailInt("M2 descent",  5, b->descent);
  }

  // M3: parser round-trip on the improper negative form the builder emits:
  // sup-minus, sup-3, '/', sub-4.
  ppReset();
  uint8_t root;
  static const char improper[] = "\xa1\x6b" "\xa1\x63" "/" "\xa0\x84";
  if(!ppParseFraction(improper, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
    ppTestFail("M3 parse");
  }
  else {
    const ppNode_t *r = ppNodeAt(root);
    uint8_t minus = (r != NULL && r->kind == PP_HBOX) ? r->firstChild : PP_NONE;
    uint8_t fr    = (minus != PP_NONE) ? ppNodeAt(minus)->nextSibling : PP_NONE;
    if(minus == PP_NONE || ppNodeAt(minus)->kind != PP_RUN
        || strcmp(ppTextAt(ppNodeAt(minus)->textOff), "-") != 0) {
      ppTestFail("M3 minus run");
    }
    if(fr == PP_NONE || ppNodeAt(fr)->kind != PP_FRAC) {
      ppTestFail("M3 frac node");
    }
    else {
      uint8_t n = ppNodeAt(fr)->firstChild;
      uint8_t d = (n != PP_NONE) ? ppNodeAt(n)->nextSibling : PP_NONE;
      if(n == PP_NONE || strcmp(ppTextAt(ppNodeAt(n)->textOff), "3") != 0) {
        ppTestFail("M3 numerator != 3");
      }
      if(d == PP_NONE || strcmp(ppTextAt(ppNodeAt(d)->textOff), "4") != 0) {
        ppTestFail("M3 denominator != 4");
      }
    }
  }

  // M5: radical over a numeric digit — the standalone-√2 shape.
  ppReset();
  {
    uint8_t rad = ppNewBox(PP_RAD, PP_FONT_NUMERIC);
    uint8_t arg = ppNewRun("2", 1, PP_FONT_NUMERIC);
    ppAppendChild(rad, arg);
    if(rad == PP_NONE || arg == PP_NONE || !ppMeasure(rad, 0)) {
      ppTestFail("M5 build/measure");
    }
    else {
      const ppNode_t *r = ppNodeAt(rad);
      if(r->ascent  != 29) ppTestFailInt("M5 ascent",  29, r->ascent);   // 26 + radGap 1 + vinc 2
      if(r->descent !=  0) ppTestFailInt("M5 descent",  0, r->descent);
    }
  }

  // M6: exponent form — mantissa ·₁₀⁴⁰ becomes SUP(base "…·10", exp "40").
  ppReset();
  {
    static const char expForm[] = "1.5" "\x80\xb7" "\xa4\x7d" "\xa1\x64" "\xa1\x60";
    static const char expBase[] = "1.5" "\x80\xb7" "10";
    if(!ppParseExponent(expForm, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
      ppTestFail("M6 parse");
    }
    else {
      const ppNode_t *s = ppNodeAt(root);
      uint8_t base = (s != NULL && s->kind == PP_SUP) ? s->firstChild : PP_NONE;
      uint8_t exp  = (base != PP_NONE) ? ppNodeAt(base)->nextSibling : PP_NONE;
      if(base == PP_NONE || strcmp(ppTextAt(ppNodeAt(base)->textOff), expBase) != 0) {
        ppTestFail("M6 base text");
      }
      if(exp == PP_NONE || strcmp(ppTextAt(ppNodeAt(exp)->textOff), "40") != 0) {
        ppTestFail("M6 exponent text");
      }
    }
  }

  // M7: IRFRAC √3/2 — RAD inside the numerator of a FRAC.
  ppReset();
  {
    static const char rt3over2[] = "\xa2\x1a" "\xa0\x83" "/" "\xa0\x82";
    if(!ppParseIrfrac(rt3over2, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
      ppTestFail("M7 parse");
    }
    else {
      const ppNode_t *r = ppNodeAt(root);
      uint8_t fr = (r != NULL && r->kind == PP_HBOX) ? r->firstChild : PP_NONE;
      if(fr == PP_NONE || ppNodeAt(fr)->kind != PP_FRAC) {
        ppTestFail("M7 frac node");
      }
      else {
        uint8_t numBox = ppNodeAt(fr)->firstChild;
        uint8_t den    = (numBox != PP_NONE) ? ppNodeAt(numBox)->nextSibling : PP_NONE;
        uint8_t rad    = (numBox != PP_NONE) ? ppNodeAt(numBox)->firstChild : PP_NONE;
        if(rad == PP_NONE || ppNodeAt(rad)->kind != PP_RAD) {
          ppTestFail("M7 radical in numerator");
        }
        else if(strcmp(ppTextAt(ppNodeAt(ppNodeAt(rad)->firstChild)->textOff), "3") != 0) {
          ppTestFail("M7 radicand != 3");
        }
        if(den == PP_NONE || strcmp(ppTextAt(ppNodeAt(den)->textOff), "2") != 0) {
          ppTestFail("M7 denominator != 2");
        }
      }
    }
  }

  // M8: IRFRAC 3×π/4 accepted; paren-power and bare-name forms decline.
  ppReset();
  {
    static const char threePiOver4[] = "3" "\x80\xd7" "\x83\xc0" "/" "\xa0\x84";
    if(!ppParseIrfrac(threePiOver4, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
      ppTestFail("M8 3xpi/4 parse");
    }
  }
  ppReset();
  {
    static const char piSquared[] = "(" "\x83\xc0" "\xa1\x62" "\xa0\x0a" "\xa0\x0a" ")";
    if(ppParseIrfrac(piSquared, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
      ppTestFail("M8 paren form accepted");
    }
  }
  ppReset();
  {
    static const char barePi[] = "\x83\xc0";
    if(ppParseIrfrac(barePi, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
      ppTestFail("M8 bare name accepted");
    }
  }

  // M4: parser rejections.
  ppReset();
  if(ppParseFraction("3/4", PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
    ppTestFail("M4 plain digits accepted");
  }
  ppReset();
  if(ppParseFraction("", PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
    ppTestFail("M4 empty accepted");
  }
  ppReset();
  static const char twoSlash[] = "\xa1\x63" "/" "\xa0\x84" "/";
  if(ppParseFraction(twoSlash, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
    ppTestFail("M4 two slashes accepted");
  }
  ppReset();
  static const char supAfter[] = "\xa1\x63" "/" "\xa1\x63";
  if(ppParseFraction(supAfter, PP_FONT_NUMERIC, PP_FONT_STANDARD, &root)) {
    ppTestFail("M4 sup after slash accepted");
  }

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestPixels ================================================== */

// X-line band: rows baseY-4 .. baseY+38 (the X line's own clear extent).
#define PPT_BAND_TOP   (Y_POSITION_OF_REGISTER_X_LINE - 4)
#define PPT_BAND_ROWS  43

static void ppTestClearBand(void) {
  lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
}

static bool_t ppTestRowAllLit(uint32_t row, uint32_t x0, uint32_t x1) {
  for(uint32_t x = x0; x <= x1; x++) {
    if(!lcd_buffer_pixel_on(x, row)) {
      return false;
    }
  }
  return true;
}

static bool_t ppTestRowAnyLit(uint32_t row, uint32_t x0, uint32_t x1) {
  for(uint32_t x = x0; x <= x1; x++) {
    if(lcd_buffer_pixel_on(x, row)) {
      return true;
    }
  }
  return false;
}

static bool_t ppTestRectAnyLit(uint32_t r0, uint32_t r1, uint32_t x0, uint32_t x1) {
  for(uint32_t r = r0; r <= r1; r++) {
    if(ppTestRowAnyLit(r, x0, x1)) {
      return true;
    }
  }
  return false;
}

void prettyTestPixels(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  bool_t hadFract = getSystemFlag(FLAG_FRACT);
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  setSystemFlag(FLAG_FRACT);
  prettySetEnabled(true);
  ppTestSetRealX("0.75");

  ppTestClearBand();
  refreshRegisterLine(REGISTER_X);

  // Geometry pinned by TESTING.md: baseline 160, bar rows 149-150,
  // fraction span x = 388..399 (width 12, right-aligned).
  const uint32_t x0 = SCREEN_WIDTH - 12, x1 = SCREEN_WIDTH - 1;
  uint32_t failuresBefore = ppTestFailures;
  if(!ppTestRowAllLit(149, x0, x1)) ppTestFail("P1 bar row 149 not lit");
  if(!ppTestRowAllLit(150, x0, x1)) ppTestFail("P1 bar row 150 not lit");
  if(ppTestRowAnyLit(148, x0, x1))  ppTestFail("P1 row 148 not clear");
  if(ppTestRowAnyLit(151, x0, x1))  ppTestFail("P1 row 151 not clear");

  if(ppTestRowAnyLit(147, x0, x1))  ppTestFail("P2 gap row 147 not clear");
  if(!ppTestRectAnyLit(135, 146, x0, x1)) ppTestFail("P2 numerator ink missing");

  if(ppTestRowAnyLit(152, x0, x1))  ppTestFail("P3 gap row 152 not clear");
  if(!ppTestRectAnyLit(153, 164, x0, x1)) ppTestFail("P3 denominator ink missing");

  if(ppTestFailures != failuresBefore) {
    // one-line diagnosis on failure: where the render actually landed
    uint32_t top = 0, bottom = 0, left = 0, right = 0;
    bool_t any = false;
    for(uint32_t r = PPT_BAND_TOP; r < PPT_BAND_TOP + PPT_BAND_ROWS; r++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, r)) {
          if(!any) { top = r; left = x; right = x; any = true; }
          bottom = r;
          if(x < left)  left  = x;
          if(x > right) right = x;
        }
      }
    }
    printf("prettyPrint pixels probe: lit rows %u..%u cols %u..%u (any=%d)\n",
           top, bottom, left, right, any);
  }

  // P5: with the package off, upstream's diagonal form paints — row 149
  // must NOT be a full lit run across the pretty span.
  prettySetEnabled(false);
  ppTestClearBand();
  refreshRegisterLine(REGISTER_X);
  if(ppTestRowAllLit(149, x0, x1)) ppTestFail("P5 upstream form identical to bar");
  prettySetEnabled(true);

  // P4 (PP2): √2 via IRFRAC — vinculum at rows 131-132 (baseline 160,
  // numeric radicand ink top 134, radGap 1, vincThick 2), gap row 133.
  {
    bool_t hadIrfrac = getSystemFlag(FLAG_IRFRAC);
    uint8_t hadIrStatus = IrFractionsCurrentStatus;
    clearSystemFlag(FLAG_FRACT);
    setSystemFlag(FLAG_IRFRAC);
    IrFractionsCurrentStatus = CF_NORMAL;
    ppTestSetRealX("1.414213562373095048801688724209698");
    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);

    // find the vinculum: a lit run of >= 10 px at row 131 in the right half
    uint32_t runLen = 0, bestLen = 0, bestEnd = 0;
    for(uint32_t x = 200; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, 131)) {
        runLen++;
        if(runLen > bestLen) { bestLen = runLen; bestEnd = x; }
      }
      else {
        runLen = 0;
      }
    }
    if(bestLen < 10) {
      ppTestFail("P4 vinculum row 131 missing");
    }
    else {
      uint32_t vx0 = bestEnd - bestLen + 1, vx1 = bestEnd;
      if(!ppTestRowAllLit(132, vx0, vx1)) ppTestFail("P4 vinculum row 132 missing");
      // gap/ink probes stay in the radicand's columns (right end of the
      // run) — the sign's own diagonal legitimately crosses row 133 on
      // the left
      if(ppTestRowAnyLit(133, vx1 - 8, vx1)) ppTestFail("P4 gap row 133 not clear");
      if(!ppTestRectAnyLit(134, 159, vx1 - 10, vx1)) ppTestFail("P4 radicand ink missing");
    }
    if(!hadIrfrac) {
      clearSystemFlag(FLAG_IRFRAC);
    }
    IrFractionsCurrentStatus = hadIrStatus;
  }

  if(hadFract) {
    setSystemFlag(FLAG_FRACT);
  }
  else {
    clearSystemFlag(FLAG_FRACT);
  }
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestShow ==================================================== */

void prettyTestShow(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  bool_t hadFract = getSystemFlag(FLAG_FRACT);
  int16_t hadScrUpd = screenUpdatingMode;
  bool_t hadHolds = screenHoldsDrawnPixels;
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  setSystemFlag(FLAG_FRACT);
  prettySetEnabled(true);
  ppTestSetRealX("0.75");

  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  fnPrettyShow(NOPARAM);

  // frame lines at 20 and 168, full width
  if(!ppTestRowAllLit(20, 0, SCREEN_WIDTH - 1))  ppTestFail("S1 frame line 20 missing");
  if(!ppTestRowAllLit(168, 0, SCREEN_WIDTH - 1)) ppTestFail("S1 frame line 168 missing");

  // centered 3/4 at the numeric/numeric rung: ascent 39, descent 19,
  // baseline (21+167-58)/2 + 39 = 104, bar rows 93-94
  {
    uint32_t runLen = 0, bestLen = 0, bestEnd = 0;
    for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, 93)) {
        runLen++;
        if(runLen > bestLen) { bestLen = runLen; bestEnd = x; }
      }
      else {
        runLen = 0;
      }
    }
    if(bestLen < 10) {
      ppTestFail("S2 bar row 93 missing");
    }
    else {
      uint32_t bx0 = bestEnd - bestLen + 1, bx1 = bestEnd;
      if(!ppTestRowAllLit(94, bx0, bx1)) ppTestFail("S2 bar row 94 missing");
      // roughly centered: the bar's midpoint within 40 px of screen center
      int32_t mid = (int32_t)(bx0 + bx1) / 2;
      if(mid < SCREEN_WIDTH / 2 - 40 || mid > SCREEN_WIDTH / 2 + 40) {
        ppTestFail("S2 bar not centered");
      }
    }
  }

  // manual-paint protocol armed
  if(!(screenUpdatingMode & SCRUPD_MANUAL_STACK)) ppTestFail("S3 manual stack bit not set");
  if(!screenHoldsDrawnPixels)                     ppTestFail("S3 screenHoldsDrawnPixels not set");

  // fallback: unsupported type must reach fnC47Show (TI changes)
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  reallocateRegister(REGISTER_X, dtString, TO_BLOCKS(4), amNone);
  strcpy(REGISTER_STRING_DATA(REGISTER_X), "abc");
  temporaryInformation = TI_NO_INFO;
  fnPrettyShow(NOPARAM);
  if(temporaryInformation == TI_NO_INFO) ppTestFail("S4 fallback did not reach SHOW");

  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = hadScrUpd;
  screenHoldsDrawnPixels = hadHolds;
  if(!hadFract) {
    clearSystemFlag(FLAG_FRACT);
  }
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestFallback ================================================ */

static uint8_t ppTestSnap[2][PPT_BAND_ROWS][SCREEN_WIDTH / 8];

static void ppTestCapture(int which) {
  memset(ppTestSnap[which], 0, sizeof(ppTestSnap[which]));
  for(uint32_t r = 0; r < PPT_BAND_ROWS; r++) {
    for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, PPT_BAND_TOP + r)) {
        ppTestSnap[which][r][x / 8] |= (uint8_t)(1u << (x % 8));
      }
    }
  }
}

static void ppTestRenderX(void) {
  ppTestClearBand();
  refreshRegisterLine(REGISTER_X);
}

static bool_t ppTestSnapsEqual(void) {
  return memcmp(ppTestSnap[0], ppTestSnap[1], sizeof(ppTestSnap[0])) == 0;
}

void prettyTestFallback(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  bool_t hadFract = getSystemFlag(FLAG_FRACT);
  bool_t hadUpd   = updateDisplayValueX;
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;

  // F1: unsupported type (string in X) — enabled and disabled renders must
  // be bit-identical.
  setSystemFlag(FLAG_FRACT);
  reallocateRegister(REGISTER_X, dtString, TO_BLOCKS(4), amNone);
  strcpy(REGISTER_STRING_DATA(REGISTER_X), "abc");
  prettySetEnabled(true);
  ppTestRenderX();
  ppTestCapture(0);
  prettySetEnabled(false);
  ppTestRenderX();
  ppTestCapture(1);
  prettySetEnabled(true);
  if(!ppTestSnapsEqual()) ppTestFail("F1 string band differs");

  // F2: plain real (FRACT and IRFRAC off, no exponent form) — every
  // parser declines, identity holds.
  {
    bool_t hadIrfrac = getSystemFlag(FLAG_IRFRAC);
    clearSystemFlag(FLAG_FRACT);
    clearSystemFlag(FLAG_IRFRAC);
    ppTestSetRealX("1.234567");
    prettySetEnabled(true);
    ppTestRenderX();
    ppTestCapture(0);
    prettySetEnabled(false);
    ppTestRenderX();
    ppTestCapture(1);
    prettySetEnabled(true);
    if(!ppTestSnapsEqual()) ppTestFail("F2 real band differs");
    if(hadIrfrac) {
      setSystemFlag(FLAG_IRFRAC);
    }
  }

  // F3: displayValueX parity — the pretty path runs the same builder, so
  // the ASCII mirror must match upstream's byte for byte.
  char dvOn[DISPLAY_VALUE_LEN];
  setSystemFlag(FLAG_FRACT);
  updateDisplayValueX = true;
  prettySetEnabled(true);
  ppTestRenderX();
  strcpy(dvOn, displayValueX);
  prettySetEnabled(false);
  ppTestRenderX();
  if(strcmp(dvOn, displayValueX) != 0) ppTestFail("F3 displayValueX differs");
  prettySetEnabled(true);

  updateDisplayValueX = hadUpd;
  if(!hadFract) {
    clearSystemFlag(FLAG_FRACT);
  }
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}

/* ==== prettyTestCapture =================================================
 * Drives the REAL interactive paths: digits through addItemToNimBuffer
 * (which itself opens NIM from CM_NORMAL), operator keys close NIM at the
 * addItemToNimBuffer tail exactly as a keypress does, then the item runs
 * through runFunction -> reallyRunFunction where the STAGE/DONE hooks
 * live. Expected signatures are built from indexOfItems catalog names at
 * runtime so font/name changes never turn these red. */

static void ppcTestSigNode(uint8_t n, char *out, size_t cap) {
  size_t len = strlen(out);
  if(len + 24 >= cap) {
    return;
  }
  const ppcNode_t *nd = ppcNodeAt(n);
  if(n == PPC_UNKNOWN) {
    strcat(out, "#");
    return;
  }
  if(n == PPC_NIL || nd == NULL) {
    strcat(out, "?");
    return;
  }
  switch(nd->kind) {
    case PPN_OP2:
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, " ");
      ppcTestSigNode(nd->child[1], out, cap);
      strcat(out, " ");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
    case PPN_OP1:
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, " ");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
    case PPN_LIT: {
      char text[32];
      uint8_t l = nd->aux > 15 ? 15 : nd->aux;
      xcopy(text, nd->payload, l);
      text[l] = 0;
      if(nd->child[0] != PPC_NIL && ppcNodeAt(nd->child[0]) != NULL
          && ppcNodeAt(nd->child[0])->kind == PPN_LIT2) {
        const ppcNode_t *c = ppcNodeAt(nd->child[0]);
        uint8_t cl = c->aux > 15 ? 15 : c->aux;
        xcopy(text + l, c->payload, cl);
        text[l + cl] = 0;
      }
      strcat(out, text);
      break;
    }
    case PPN_VAL:    strcat(out, "#"); break;
    case PPN_CONST:  strncat(out, indexOfItems[nd->item].itemCatalogName, 15); break;
    case PPN_OPAQUE: strcat(out, "!"); break;
    default:         strcat(out, "?"); break;
  }
}

static void ppcTestSig(char *out, size_t cap) {
  out[0] = 0;
  uint8_t root = ppcCurrentFormulaRoot();
  if(root == PPC_NIL) {
    strcpy(out, "-");
    return;
  }
  ppcTestSigNode(root, out, cap);
}

static void ppcTestReset(void) {
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  programRunStop = PGM_STOPPED;
  clearSystemFlag(FLAG_SOLVING);
  clearSystemFlag(FLAG_INTING);
  clearSystemFlag(FLAG_ERPN);
  setSystemFlag(FLAG_ASLIFT);
  aimBuffer[0] = 0;      // NIM typing residue must not leak into later
  nimNumberPart = NP_EMPTY;   // suite blocks (fn42Alpha asserts an empty buffer)
  prettyReset();
}

static void ppcTestType(const char *s) {
  for(const char *p = s; *p; p++) {
    if(*p >= '0' && *p <= '9') {
      addItemToNimBuffer(ITM_0 + (*p - '0'));
    }
    else if(*p == '.') {
      addItemToNimBuffer(ITM_PERIOD);
    }
    else if(*p == '<') {
      addItemToNimBuffer(ITM_BACKSPACE);
    }
  }
}

static void ppcTestOp(int16_t item) {
  if(calcMode == CM_NIM) {
    addItemToNimBuffer(item);   // the keypress closes NIM before the run
  }
  runFunction(item);
}

static void ppcTestExpectSig(const char *what, const char *expected) {
  char sig[128];
  ppcTestSig(sig, sizeof(sig));
  if(strcmp(sig, expected) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected '%s', actual '%s')\n", what, expected, sig);
  }
}

static void ppcTestExpectHist(const char *what, uint8_t expected) {
  if(ppcHistoryCount() != expected) {
    ppTestFailInt(what, expected, ppcHistoryCount());
  }
}

void prettyTestCapture(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  char expect[128];
  const char *nADD  = indexOfItems[ITM_ADD].itemCatalogName;
  const char *nMULT = indexOfItems[ITM_MULT].itemCatalogName;
  const char *nSUB  = indexOfItems[ITM_SUB].itemCatalogName;
  const char *nSIN  = indexOfItems[ITM_sin].itemCatalogName;
  const char *n1ONX = indexOfItems[ITM_1ONX].itemCatalogName;

  // T1: 2 ENTER 3 + 4 x — one formula, consuming continues it
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s 4 %s", nADD, nMULT);
  ppcTestExpectSig("T1 chained sig", expect);
  ppcTestExpectHist("T1 hist", 0);

  // T2: supersession — a new root not consuming (2+3) emits it
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 6 %s", nADD);
  ppcTestExpectSig("T2 new formula", expect);
  ppcTestExpectHist("T2 hist", 1);

  // T3: monadic through the NIM funnel
  ppcTestReset();
  ppcTestType("12");
  ppcTestOp(ITM_sin);
  sprintf(expect, "12 %s", nSIN);
  ppcTestExpectSig("T3 monadic", expect);

  // T4: ENTER dup mirrored as a deep copy
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestOp(ITM_ENTER);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 2 %s", nMULT);
  ppcTestExpectSig("T4 dup", expect);

  // T5: monadic result consumed by a dyadic — one formula
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_1ONX);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 %s 3 %s", n1ONX, nADD);
  ppcTestExpectSig("T5 chain through monadic", expect);
  ppcTestExpectHist("T5 hist", 0);

  // T6: CLX displaces — the natural explicit terminator
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_CLX);
  ppcTestType("7");
  ppcTestOp(ITM_ENTER);
  ppcTestType("8");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "7 8 %s", nMULT);
  ppcTestExpectSig("T6 after CLX", expect);
  ppcTestExpectHist("T6 hist", 1);

  // T7: as-typed literal survives
  ppcTestReset();
  ppcTestType("2.50");
  ppcTestOp(ITM_ENTER);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2.50 4 %s", nMULT);
  ppcTestExpectSig("T7 as-typed", expect);

  // T8: NIM abort by backspace leaves no ghost (deferred lift pays off)
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3<");     // type 3, then backspace to empty = abort
  ppcTestType("4");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 4 %s", nADD);
  ppcTestExpectSig("T8 abort", expect);

  // T9: swap mirrored — operand order flips
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_SUB);
  sprintf(expect, "3 2 %s", nSUB);
  ppcTestExpectSig("T9 swap", expect);

  // T10: UNDO discards the current formula, history untouched
  ppcTestReset();
  ppcTestType("3");
  ppcTestOp(ITM_ENTER);
  ppcTestType("4");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_UNDO);
  ppcTestExpectSig("T10 after UNDO", "-");
  ppcTestExpectHist("T10 hist", 0);

  // T11: an erroring dispatch invalidates (DONE-on-error)
  ppcTestReset();
  clearSystemFlag(FLAG_SPCRES);
  ppcTestType("1");
  ppcTestOp(ITM_ENTER);
  ppcTestType("0");
  ppcTestOp(ITM_DIV);
  if(lastErrorCode == ERROR_NONE) {
    ppTestFail("T11 division by zero did not error");
  }
  lastErrorCode = 0;
  ppcTestExpectSig("T11 after error", "-");

  // T12: arena exhaustion invalidates mid-chain, then the engine rebuilds
  // truthfully from value-leaf upgrades — the tail of the chain reads
  // "VAL 1 + 1 +", which is honest (# is register Y's live value)
  ppcTestReset();
  ppcTestType("1");
  ppcTestOp(ITM_ENTER);
  for(int i = 0; i < 14; i++) {
    ppcTestType("1");
    ppcTestOp(ITM_ADD);
  }
  sprintf(expect, "# 1 %s 1 %s", nADD, nADD);
  ppcTestExpectSig("T12 exhaustion recovery", expect);

  // T13: LASTx returns as a truthful value leaf
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_LASTX);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s # %s", nADD, nMULT);
  ppcTestExpectSig("T13 LASTx", expect);

  // T14: unknown undo-enabled item (MIN, deliberately unclassified)
  // emits the current formula, then invalidates
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("9");
  ppcTestOp(ITM_MIN);
  ppcTestExpectSig("T14 after unknown", "-");
  ppcTestExpectHist("T14 hist", 1);

  // T15: eRPN ENTER does not dup — the consumed Y upgrades to a value.
  // nimWhenButtonPressed is keyboard-owned; the driver mimics the real
  // keypress (a NIM was open when ENTER went down) so fnKeyEnter's eRPN
  // condition — and the shadow's mirror of it — sees the true state.
  ppcTestReset();
  setSystemFlag(FLAG_ERPN);
  ppcTestType("5");
  nimWhenButtonPressed = true;
  ppcTestOp(ITM_ENTER);
  nimWhenButtonPressed = false;
  ppcTestOp(ITM_MULT);
  sprintf(expect, "# 5 %s", nMULT);
  ppcTestExpectSig("T15 eRPN", expect);
  clearSystemFlag(FLAG_ERPN);

  // T16: abort while ASLIFT is set (straight after an operator result) —
  // the deferred-lift design absorbs the upstream undo() for free; a
  // shadow that lifts at NIM open strands the tree one slot up
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5<");     // open with ASLIFT set, then abort
  ppcTestType("6");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s 6 %s", nADD, nMULT);
  ppcTestExpectSig("T16 abort under lift", expect);
  ppcTestExpectHist("T16 hist", 0);

  ppcTestReset();
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestFormula =================================================
 * Layout signatures: RUN -> its text with spaces stripped; HBOX ->
 * [children space-joined]; FRAC -> F(a|b); SUP -> S(a|b); RAD -> R(a);
 * PAREN -> P(a). Expected strings build from catalog names at runtime. */

static void ppfTestSigNode(uint8_t n, char *out, size_t cap) {
  if(strlen(out) + 24 >= cap) {
    return;
  }
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL) {
    strcat(out, "?");
    return;
  }
  switch(nd->kind) {
    case PP_RUN: {
      const char *t = ppTextAt(nd->textOff);
      char *e = out + strlen(out);
      while(*t && (size_t)(e - out) + 2 < cap) {
        if(*t != ' ') {
          *e++ = *t;
        }
        t++;
      }
      *e = 0;
      break;
    }
    case PP_HBOX: {
      strcat(out, "[");
      for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppNodeAt(c)->nextSibling) {
        if(c != nd->firstChild) {
          strcat(out, " ");
        }
        ppfTestSigNode(c, out, cap);
      }
      strcat(out, "]");
      break;
    }
    case PP_FRAC:
      strcat(out, "F(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, "|");
      ppfTestSigNode(ppNodeAt(nd->firstChild)->nextSibling, out, cap);
      strcat(out, ")");
      break;
    case PP_SUP:
      strcat(out, "S(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, "|");
      ppfTestSigNode(ppNodeAt(nd->firstChild)->nextSibling, out, cap);
      strcat(out, ")");
      break;
    case PP_RAD: {
      strcat(out, "R(");
      ppfTestSigNode(nd->firstChild, out, cap);
      uint8_t idx = ppNodeAt(nd->firstChild)->nextSibling;
      if(idx != PP_NONE) {
        strcat(out, ";");
        ppfTestSigNode(idx, out, cap);
      }
      strcat(out, ")");
      break;
    }
    case PP_SUB:
      strcat(out, "U(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, "|");
      ppfTestSigNode(ppNodeAt(nd->firstChild)->nextSibling, out, cap);
      strcat(out, ")");
      break;
    case PP_BARS:
      strcat(out, "A(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, ")");
      break;
    case PP_PAREN:
      strcat(out, "P(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, ")");
      break;
    default:
      strcat(out, "?");
      break;
  }
}

static void ppfTestExpect(const char *what, uint8_t root, const char *expected) {
  char sig[192];
  sig[0] = 0;
  ppfTestSigNode(root, sig, sizeof(sig));
  if(strcmp(sig, expected) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected '%s', actual '%s')\n", what, expected, sig);
  }
}

void prettyTestFormula(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  char expect[192];
  uint8_t root;
  const char *nADD  = indexOfItems[ITM_ADD].itemCatalogName;
  const char *nMULT = indexOfItems[ITM_MULT].itemCatalogName;

  // FV1: precedence parens — (2+3)×4
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV1 build");
  }
  else {
    sprintf(expect, "[P([2 %s 3]) %s 4]", nADD, nMULT);
    ppfTestExpect("FV1 precedence", root, expect);
  }

  // FV2: division becomes a stacked fraction, children unparenthesized
  ppcTestReset();
  ppcTestType("6");
  ppcTestOp(ITM_ENTER);
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_DIV);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV2 build");
  }
  else {
    sprintf(expect, "F(6|[2 %s 3])", nADD);
    ppfTestExpect("FV2 div as fraction", root, expect);
  }

  // FV3: history entry decodes with its result
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  if(ppcHistoryCount() != 1) {
    ppTestFailInt("FV3 hist", 1, ppcHistoryCount());
  }
  else {
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
      ppTestFail("FV3 decode");
    }
    else {
      sprintf(expect, "[[2 %s 3] = 5]", nADD);
      ppfTestExpect("FV3 entry with result", root, expect);
    }
  }

  // FV4: sqrt scopes without parens; square wraps in SUP
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_SQUAREROOTX);
  ppcTestOp(ITM_SQUARE);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV4 build");
  }
  else {
    ppfTestExpect("FV4 sqrt+square", root, "S(R(2)|2)");
  }

  // FV5: PHIST pager paints frames and arms the protocol; PCLR empties
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  temporaryInformation = TI_NO_INFO;
  fnPrettyHist(NOPARAM);
  if(!ppTestRowAllLit(20, 0, SCREEN_WIDTH - 1))  ppTestFail("FV5 frame 20");
  if(!ppTestRowAllLit(168, 0, SCREEN_WIDTH - 1)) ppTestFail("FV5 frame 168");
  if(!ppTestRectAnyLit(21, 167, 0, SCREEN_WIDTH - 1)) ppTestFail("FV5 no content ink");
  if(!(screenUpdatingMode & SCRUPD_MANUAL_STACK)) ppTestFail("FV5 protocol not armed");
  if(!screenHoldsDrawnPixels)                     ppTestFail("FV5 pixels not held");
  fnPrettyHistClear(NOPARAM);
  if(ppcHistoryCount() != 0) ppTestFailInt("FV5 PCLR", 0, ppcHistoryCount());
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV6: a formula too tall for the pager's standard rung must still
  // show — the tiny rung re-fonts the whole tree. Build the 3-level
  // continued fraction 1/(2+3/(4+5/6)) through the real key paths.
  ppcTestReset();
  ppcTestType("6");
  ppcTestOp(ITM_ENTER);
  ppcTestType("5");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  ppcTestType("4");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_ADD);
  ppcTestType("3");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  ppcTestType("2");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_ADD);
  ppcTestType("1");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  temporaryInformation = TI_NO_INFO;
  fnPrettyHist(NOPARAM);
  if(!ppTestRectAnyLit(21, 56, 0, SCREEN_WIDTH - 1)) {
    ppTestFail("FV6 tall formula missing from the pager");
  }
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV7 (PP6): sqrt over a fraction — the synthesized tall sign; measure
  // must succeed and the sign strokes must leave ink left of the vinculum
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_DIV);
  ppcTestOp(ITM_SQUAREROOTX);
  ppReset();
  {
    uint8_t root7;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root7)) {
      ppTestFail("FV7 build");
    }
    else {
      ppfTestExpect("FV7 sqrt of fraction", root7, "R(F(2|3))");
      if(!ppMeasure(root7, 0)) {
        ppTestFail("FV7 tall radical declined");
      }
      else {
        lcd_fill_rect(0, 60, 120, 80, LCD_SET_VALUE);
        const ppNode_t *n7 = ppNodeAt(root7);
        ppPaintAt(root7, 10, 100);
        // columns 10..17 hold ONLY the stroke sign — the vinculum starts
        // at column 19 (child relX-1) and once satisfied this pin by
        // accident (MUT-23 stayed green)
        if(!ppTestRectAnyLit((uint32_t)(100 - n7->ascent), (uint32_t)(100 + n7->descent - 1), 10, 17)) {
          ppTestFail("FV7 synthesized sign missing");
        }
        lcd_fill_rect(0, 60, 120, 80, LCD_SET_VALUE);
      }
    }
  }

  // FV8 (PP6): xth-root carries its index at the crook
  ppcTestReset();
  ppcTestType("27");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_XTHROOT);
  ppReset();
  {
    uint8_t root8;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root8)) {
      ppTestFail("FV8 build");
    }
    else {
      ppfTestExpect("FV8 indexed root", root8, "R(27;3)");
    }
  }

  // FV9 (PP6): log with a subscript base; the script is LOWERED (a SUB
  // node's descent grows — a SUP-flipped mutation zeroes it)
  ppcTestReset();
  ppcTestType("8");
  ppcTestOp(ITM_ENTER);
  ppcTestType("2");
  ppcTestOp(ITM_LOGXY);
  ppReset();
  {
    uint8_t root9;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root9)) {
      ppTestFail("FV9 build");
    }
    else {
      ppfTestExpect("FV9 log base", root9, "[U(log|2) P(8)]");
      if(ppMeasure(root9, 0)) {
        // assert the SCRIPT node itself is lowered: relBase must be
        // positive (the root-descent version was satisfied by 'log's own
        // descender — MUT-25 stayed green)
        const ppNode_t *r9 = ppNodeAt(root9);
        uint8_t sub9 = r9->firstChild;
        uint8_t script9 = (sub9 != PP_NONE) ? ppNodeAt(ppNodeAt(sub9)->firstChild)->nextSibling : PP_NONE;
        if(script9 == PP_NONE || ppNodeAt(script9)->relBase < 3) {
          ppTestFailInt("FV9 subscript not lowered", 3,
                        script9 == PP_NONE ? -99 : ppNodeAt(script9)->relBase);
        }
      }
    }
  }

  // FV10 (PP6): absolute-value bars
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_MAGNITUDE);
  ppReset();
  {
    uint8_t root10;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root10)) {
      ppTestFail("FV10 build");
    }
    else {
      ppfTestExpect("FV10 abs bars", root10, "A(5)");
      if(ppMeasure(root10, 0)) {
        lcd_fill_rect(0, 60, 120, 60, LCD_SET_VALUE);
        const ppNode_t *n10 = ppNodeAt(root10);
        ppPaintAt(root10, 10, 100);
        if(!ppTestRowAllLit((uint32_t)(100 - n10->ascent), 11, 12)) {
          ppTestFail("FV10 left bar missing");
        }
        lcd_fill_rect(0, 60, 120, 60, LCD_SET_VALUE);
      }
    }
  }

  ppcTestReset();
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}

/* ==== prettyTestEquation ================================================ */

void prettyTestEquation(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  uint8_t root;

  // EQ1: '/' binds factors, not the whole expression
  ppReset();
  if(!ppqParse("1/X+2", &root)) {
    ppTestFail("EQ1 parse");
  }
  else {
    ppfTestExpect("EQ1 precedence", root, "[F(1|X) + 2]");
  }

  // EQ2: parens unwrap under the bar
  ppReset();
  if(!ppqParse("(A+B)/C", &root)) {
    ppTestFail("EQ2 parse");
  }
  else {
    ppfTestExpect("EQ2 unwrap", root, "F([A + B]|C)");
  }

  // EQ3: vinculum over a parenthesized radicand
  ppReset();
  if(!ppqParse("\xa2\x1a" "(X+1)", &root)) {
    ppTestFail("EQ3 parse");
  }
  else {
    ppfTestExpect("EQ3 radical", root, "R([X + 1])");
  }

  // EQ4: declines — no 2D gain, dangling operator, ellipsis, unknown glyph
  ppReset();
  if(ppqParse("A+B", &root))            ppTestFail("EQ4 no-frac accepted");
  ppReset();
  if(ppqParse("1/X+", &root))           ppTestFail("EQ4 dangling accepted");
  ppReset();
  if(ppqParse("1/X" "\xa0\x1b", &root)) ppTestFail("EQ4 ellipsis accepted");
  ppReset();
  if(ppqParse("\x83\xc0" "/2", &root))  ppTestFail("EQ4 unknown glyph accepted");

  // EQ5: the strip render paints the bar in the equation's own row
  {
    lcd_fill_rect(0, 171, SCREEN_WIDTH, 23, LCD_SET_VALUE);
    prettySetEnabled(true);
    if(!prettyTryEquation("1/X", 1)) {
      ppTestFail("EQ5 render declined");
    }
    else {
      // FRAC ctx standard, tiny children: ascent 14, descent 3 ->
      // baseline 188, bar row 182 (barTopRel -6, thickness 1)
      bool_t any = false;
      for(uint32_t x = 1; x < 60 && !any; x++) {
        any = lcd_buffer_pixel_on(x, 182);
      }
      if(!any) ppTestFail("EQ5 bar row 182 missing");
      if(ppTestRectAnyLit(194, 200, 0, 60)) ppTestFail("EQ5 ink below the strip");
    }
    lcd_fill_rect(0, 171, SCREEN_WIDTH, 23, LCD_SET_VALUE);
  }

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}



#endif // PC_BUILD
