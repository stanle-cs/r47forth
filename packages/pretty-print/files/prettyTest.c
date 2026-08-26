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

  // F2: real X with FLAG_FRACT clear — the gate declines, identity holds.
  clearSystemFlag(FLAG_FRACT);
  ppTestSetRealX("0.75");
  prettySetEnabled(true);
  ppTestRenderX();
  ppTestCapture(0);
  prettySetEnabled(false);
  ppTestRenderX();
  ppTestCapture(1);
  prettySetEnabled(true);
  if(!ppTestSnapsEqual()) ppTestFail("F2 real band differs");

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

#endif // PC_BUILD
