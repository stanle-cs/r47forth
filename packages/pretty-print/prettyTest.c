// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyTest.c
 * Coverage drivers for the pretty-print package test suite.
 * testSuite.c registers each driver in funcTestNoParam with
 * coverageDriver = 1. testSuite/tests/pretty_print.txt drives them.
 * This file builds only under PC_BUILD.
 * Each driver writes its failure count into X as a long integer.
 * Some drivers check that pretty-print declines to render an input.
 * A decline returns false and draws nothing. Only the program
 * walker's decline also carries a reason code and a step number.
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

static bool_t ppTestIsLonI(calcRegister_t regist, uint32_t expected) {
  longInteger_t li;
  bool_t equal;
  if(getRegisterDataType(regist) != dtLongInteger) {
    return false;
  }
  longIntegerInit(li);
  convertLongIntegerRegisterToLongInteger(regist, li);
  equal = longIntegerCompareUInt(li, expected) == 0;
  longIntegerFree(li);
  return equal;
}


/* ==== prettyTestMeasure ================================================= */

void prettyTestMeasure(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  // M1: FRAC(3,4), numeric context, standard children. Sets the exact
  // numbers the inline X-line pins P1-P3 use.
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
    if(f->descent !=  6) ppTestFail("M1 descent != 6");   // fracGap+1 below the bar (symmetric clearance)
  }

  // M2: HBOX[RUN("1", numeric), FRAC(3,4)], mixed-number shape.
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
    if(b->descent !=  6) ppTestFailInt("M2 descent",  6, b->descent);   // fracGap+1 below the bar
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

  /* M9: the PP_RAD index descent.
   *
   * Radicand 8 has ascent 12 and descent 0. The FRAC index has ascent
   * 20 and descent 8, at relBase -2, so its ink bottom sits 6 rows
   * below the baseline. The box descent must cover that. */
  ppReset();
  {
    uint8_t rad = ppNewBox(PP_RAD, PP_FONT_STANDARD);
    uint8_t arg = ppNewRun("8", 1, PP_FONT_STANDARD);
    uint8_t idx = ppNewBox(PP_FRAC, PP_FONT_STANDARD);
    uint8_t n1  = ppNewRun("1", 1, PP_FONT_STANDARD);
    uint8_t d2  = ppNewRun("2", 1, PP_FONT_STANDARD);
    ppAppendChild(idx, n1);
    ppAppendChild(idx, d2);
    ppAppendChild(rad, arg);
    ppAppendChild(rad, idx);
    if(rad == PP_NONE || arg == PP_NONE || idx == PP_NONE || !ppMeasure(rad, 0)) {
      ppTestFail("M9 build/measure");
    }
    else {
      const ppNode_t *r = ppNodeAt(rad);
      const ppNode_t *i = ppNodeAt(idx);
      if(i->descent == 0) {
        ppTestFail("M9 setup: the index has no descent, so this pin cannot see the defect");
      }
      int16_t idxBottom = (int16_t)(i->relBase + i->descent);
      if(r->descent < idxBottom) {
        ppTestFailInt("M9 the box does not cover the index's ink bottom",
                      idxBottom, r->descent);
      }
    }
  }

  // M5: radical over a numeric digit, the standalone-√2 shape.
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
      if(r->ascent  != 30) ppTestFailInt("M5 ascent",  30, r->ascent);   // 26 + radGap 1 + clearance 1 + vinc 2
      if(r->descent !=  0) ppTestFailInt("M5 descent",  0, r->descent);
    }
  }

  // M6: exponent form, mantissa ·₁₀⁴⁰ becomes SUP(base "…·10", exp "40").
  ppReset();
  {
    /* supNumberToDisplayString always appends STD_SPACE_HAIR (a0 0a).
     * The fixture carries that trailing hair space, the same as the
     * sibling ppParseIrfrac fixture. */
    static const char expForm[] = "1.5" "\x80\xb7" "\xa4\x7d" "\xa1\x64" "\xa1\x60" "\xa0\x0a";
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

  // M7: IRFRAC √3/2, RAD inside the numerator of a FRAC.
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

  // M8: IRFRAC 3×π/4 accepted, paren-power and bare-name forms decline.
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

  /* M8: the substituted-glyph class. The engine derives every metric
   * from &numericFont, clears exactly the box it measured, and paints
   * with noPreClear. Upstream's showGlyphCode can substitute a glyph
   * from another table for the same code. If the substitute's row
   * metrics differ, ink can land outside the cleared box.
   *
   * For every substitution the fonts can undergo, the package must
   * either decline, or the measured box must contain the substitute's
   * ink. FLAG_BOLD keeps displaying, so the paint entries suppress the
   * substitution and use the plain face.
   *
   * Part 1 checks the live font tables for the substitution the
   * suppression exists for. Part 2 checks that drawn output is
   * identical with FLAG_BOLD set or clear. */
  {
    // The same probes showGlyphCode uses: findGlyph for the measured
    // font, findGlyphExact for the bold substitution. A miss returns
    // -1, so it can never alias glyph index 0.
    int16_t pi = findGlyph(&numericFont, '0');
    int16_t bi = findGlyphExact(&numericFontBold, '0');
    const glyph_t *plain = (pi < 0) ? NULL : &numericFont.glyphs[pi];
    const glyph_t *bold  = (bi < 0) ? NULL : &numericFontBold.glyphs[bi];
    if(plain == NULL || bold == NULL) {
      ppTestFail("M8 '0' missing from a numeric font table");
    }
    else if(plain->rowsAboveGlyph == bold->rowsAboveGlyph
            && plain->rowsGlyph == bold->rowsGlyph) {
      ppTestFail("M8 bold and plain row metrics now agree — the FLAG_BOLD suppression is obsolete, re-derive the class");
    }

    /* Pretty-print output does not depend on FLAG_BOLD. Paint
     * suppresses the substitution, so paint stays on the same font
     * table as the metrics. The test compares the two renderings and
     * requires identical pixels. */
    bool_t  hadFract = getSystemFlag(FLAG_FRACT);
    bool_t  boldWas  = getSystemFlag(FLAG_BOLD);
    uint8_t modeWas  = calcMode;
    calcMode             = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode        = 0;
    setSystemFlag(FLAG_FRACT);
    /* The bold substitution triggers only when font == &numericFont.
     * Only a MIXED number's head is numeric: ppParseFraction skips the
     * head when there is none. Without PROPFR the drawing has no
     * numeric run, and the pin compares two identical pictures. */
    bool_t hadPropfr = getSystemFlag(FLAG_PROPFR);
    setSystemFlag(FLAG_PROPFR);
    prettySetEnabled(true);
    ppTestSetRealX("1.5");

    const uint32_t bandTop = (uint32_t)(Y_POSITION_OF_REGISTER_X_LINE - 4);
    uint32_t sumPlain = 0, sumBold = 0, litPlain = 0, litBold = 0;
    int16_t w = 0;
    bool_t drewPlain, drewBold;

    clearSystemFlag(FLAG_BOLD);
    lcd_fill_rect(0, bandTop, SCREEN_WIDTH, 43, LCD_SET_VALUE);
    drewPlain = prettyTryRegisterLine(REGISTER_X, Y_POSITION_OF_REGISTER_X_LINE, &w);
    for(uint32_t r = 0; r < 43; r++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, bandTop + r)) { litPlain++; sumPlain += r * 401 + x; }
      }
    }

    setSystemFlag(FLAG_BOLD);
    lcd_fill_rect(0, bandTop, SCREEN_WIDTH, 43, LCD_SET_VALUE);
    drewBold = prettyTryRegisterLine(REGISTER_X, Y_POSITION_OF_REGISTER_X_LINE, &w);
    for(uint32_t r = 0; r < 43; r++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, bandTop + r)) { litBold++; sumBold += r * 401 + x; }
      }
    }

    if(!drewPlain) {
      ppTestFail("M8 setup: the inline surface declines with BOLD off — pin cannot reach its state");
    }
    else if(!drewBold) {
      ppTestFail("M8 the inline surface declined under FLAG_BOLD; the 2026-08-29 ruling is that BOLD must still display");
    }
    else if(litPlain == 0) {
      ppTestFail("M8 setup: nothing was painted with BOLD off — pin proves nothing");
    }
    else if(litBold != litPlain || sumBold != sumPlain) {
      ppTestFailInt("M8 BOLD changed the pretty output (lit pixels)", (int32_t)litPlain, (int32_t)litBold);
    }

    if(!boldWas)   clearSystemFlag(FLAG_BOLD);
    if(!hadFract)  clearSystemFlag(FLAG_FRACT);
    if(!hadPropfr) clearSystemFlag(FLAG_PROPFR);
    calcMode = modeWas;
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

  // P5: with the package off, upstream's diagonal form paints. Row 149
  // must not be a full lit run across the pretty span.
  prettySetEnabled(false);
  ppTestClearBand();
  refreshRegisterLine(REGISTER_X);
  if(ppTestRowAllLit(149, x0, x1)) ppTestFail("P5 upstream form identical to bar");
  prettySetEnabled(true);

  // P4: √2 via IRFRAC. Vinculum at rows 131-132 (baseline 160, numeric
  // radicand ink top 134, radGap 1, vincThick 2), gap row 133.
  {
    bool_t hadIrfrac = getSystemFlag(FLAG_IRFRAC);
    uint8_t hadIrStatus = IrFractionsCurrentStatus;
    clearSystemFlag(FLAG_FRACT);
    setSystemFlag(FLAG_IRFRAC);
    IrFractionsCurrentStatus = CF_NORMAL;
    ppTestSetRealX("1.414213562373095048801688724209698");
    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);

    // Find the vinculum: a lit run of at least 10 px at row 130, checked
    // again at row 131 across the same span.
    uint32_t runLen = 0, bestLen = 0, bestEnd = 0;
    for(uint32_t x = 200; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, 130)) {
        runLen++;
        if(runLen > bestLen) { bestLen = runLen; bestEnd = x; }
      }
      else {
        runLen = 0;
      }
    }
    if(bestLen < 10) {
      ppTestFail("P4 vinculum row 130 missing");
    }
    else {
      uint32_t vx0 = bestEnd - bestLen + 1, vx1 = bestEnd;
      if(!ppTestRowAllLit(131, vx0, vx1)) ppTestFail("P4 vinculum row 131 missing");
      // Gap and ink probes stay in the radicand's columns, the right end
      // of the run. The root sign's diagonal can cross the gap rows on
      // the left. radGap+1 gives two clear rows.
      if(ppTestRowAnyLit(132, vx1 - 8, vx1)) ppTestFail("P4 gap row 132 not clear");
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

  // P6 (polish): the vinculum matches the root sign's stroke weight,
  // two lit rows over a standard-font radicand (the glyph's stroke is
  // 2 px).
  {
    ppReset();
    uint8_t arg = ppNewRun("2", 1, PP_FONT_STANDARD);
    uint8_t rad = ppNewBox(PP_RAD, PP_FONT_STANDARD);
    if(arg == PP_NONE || rad == PP_NONE) {
      ppTestFail("P6 build");
    }
    else {
      ppAppendChild(rad, arg);
      if(!ppMeasure(rad, 0)) {
        ppTestFail("P6 measure");
      }
      else {
        const ppNode_t *n = ppNodeAt(rad);
        lcd_fill_rect(0, 60, SCREEN_WIDTH, 80, LCD_SET_VALUE);
        ppPaintAt(rad, 30, 120);
        // Vinculum rows: the top two rows of the node's ink, probed at
        // the vinculum's right end. The root sign's hook lights columns
        // near its own column, so a probe there gives a false read.
        uint32_t vtop = (uint32_t)(120 - n->ascent);
        uint32_t rx = (uint32_t)(30 + n->width - 2);
        if(!ppTestRowAnyLit(vtop, rx - 2, rx)) {
          ppTestFail("P6 vinculum row 1 missing");
        }
        if(!ppTestRowAnyLit(vtop + 1, rx - 2, rx)) {
          ppTestFail("P6 vinculum row 2 missing");
        }
      }
    }
  }

  // P5 (polish): the integral sign's hooks reach sideways from the
  // spine, the top hook right, the bottom hook left. A bare vertical
  // bar with no hooks fails both reach probes.
  {
    ppReset();
    uint8_t body = ppNewRun("1", 1, PP_FONT_STANDARD);
    uint8_t big = ppNewBox(PP_INT, PP_FONT_STANDARD);
    if(body == PP_NONE || big == PP_NONE) {
      ppTestFail("P5 build");
    }
    else {
      ppAppendChild(big, body);
      if(!ppMeasure(big, 0)) {
        ppTestFail("P5 measure");
      }
      else {
        const ppNode_t *n = ppNodeAt(big);
        lcd_fill_rect(0, 60, SCREEN_WIDTH, 120, LCD_SET_VALUE);
        ppPaintAt(big, 30, 120);
        uint32_t top = (uint32_t)(120 - n->ascent);
        uint32_t bot = (uint32_t)(120 + n->descent - 1);
        // The spine column is x+7. The hook tips reach at least 3 px
        // sideways.
        if(!ppTestRowAnyLit(top, 30 + 7 + 3, 30 + 7 + 8)) {
          ppTestFail("P5 top hook does not reach right");
        }
        if(!ppTestRowAnyLit(bot, 30 + 7 - 6, 30 + 7 - 3)) {
          ppTestFail("P5 bottom hook does not reach left");
        }
        if(!ppTestRowAnyLit((top + bot) / 2, 30 + 7, 30 + 8)) {
          ppTestFail("P5 spine missing");
        }
      }
    }
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
      if(lcd_buffer_pixel_on(x, 92)) {
        runLen++;
        if(runLen > bestLen) { bestLen = runLen; bestEnd = x; }
      }
      else {
        runLen = 0;
      }
    }
    if(bestLen < 10) {
      ppTestFail("S2 bar row 92 missing");
    }
    else {
      uint32_t bx0 = bestEnd - bestLen + 1, bx1 = bestEnd;
      if(!ppTestRowAllLit(93, bx0, bx1)) ppTestFail("S2 bar row 93 missing");
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

static void ppTestCaptureBand(int which, uint32_t top, uint32_t rows) {
  memset(ppTestSnap[which], 0, sizeof(ppTestSnap[which]));
  if(rows > PPT_BAND_ROWS) {
    rows = PPT_BAND_ROWS;
  }
  for(uint32_t r = 0; r < rows; r++) {
    for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, top + r)) {
        ppTestSnap[which][r][x / 8] |= (uint8_t)(1u << (x % 8));
      }
    }
  }
}

static void ppTestCapture(int which) {
  ppTestCaptureBand(which, PPT_BAND_TOP, PPT_BAND_ROWS);
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

  // F1: unsupported type, string in X. Enabled and disabled renders must
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

  // F2: plain real (FRACT and IRFRAC off, no exponent form). Every
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

  // F3: displayValueX parity. The pretty path runs the same builder, so
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
 * Drives the real interactive paths. Digits go through
 * addItemToNimBuffer, which opens NIM from CM_NORMAL. Operator keys
 * close NIM at the addItemToNimBuffer tail, the same as a keypress.
 * The item then runs through runFunction and reallyRunFunction, where
 * the STAGE and DONE hooks live. Expected signatures come from
 * indexOfItems catalog names at runtime, so font or name changes never
 * turn these tests red. */

// The layout-sig helpers live with prettyTestFormula below. Capture
// traces decode history entries through them.
static void ppfTestSigNode(uint8_t n, char *out, size_t cap);
static void ppfTestExpect(const char *what, uint8_t root, const char *expected);
static void ppfTestFiledMatchesLive(const char *what);
static bool_t ppfTestPowersScoped(uint8_t n);
static const char *ppfTestFirstRunText(uint8_t n);

static void ppcTestSigNode(uint8_t n, char *out, size_t cap) {
  size_t len = strlen(out);
  if(len + 24 >= cap) {
    return;
  }
  const ppcNode_t *nd = ppcNodeAt(n);
  if(n == PPC_UNKNOWN) {
    // "~" for UNKNOWN, "#" for a value leaf: the signature must tell
    // them apart.
    strcat(out, "~");
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
    case PPN_RCL: {
      char rname[8];
      sprintf(rname, "R%02u", (unsigned)nd->item);
      strcat(out, rname);
      break;
    }
    case PPN_CONST:  strncat(out, indexOfItems[nd->item].itemCatalogName, 15); break;
    case PPN_BIGOP:
      strcat(out, "{");
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, ",");
      ppcTestSigNode(nd->child[1], out, cap);
      strcat(out, "}");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
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
  lastIntegerBase = 0;   // an entry MODE is residue too: a leaked base
                         // makes later typed integers short integers
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

static void ppcTestOpParam(int16_t item, uint16_t param) {
  if(calcMode == CM_NIM) {
    closeNim();   // a TAM-parameter key closes NIM before the entry cycle
  }
  reallyRunFunction(item, param);
}

static void ppcTestExpectSig(const char *what, const char *expected) {
  char sig[128];
  ppcTestSig(sig, sizeof(sig));
  if(strcmp(sig, expected) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected '%s', actual '%s')\n", what, expected, sig);
  }
}

/* Does the measured tree hold a run with exactly this text? For pins
 * that care only that a decode reached the picture. */
static bool_t ppTreeHasRun(uint8_t n, const char *text) {
  if(n == PP_NONE) {
    return false;
  }
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL) {
    return false;
  }
  if(nd->kind == PP_RUN && strcmp(ppTextAt(nd->textOff), text) == 0) {
    return true;
  }
  for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppNodeAt(c)->nextSibling) {
    if(ppTreeHasRun(c, text)) {
      return true;
    }
  }
  return false;
}

/* Copy-adapted from testSuite.c covWriteAndLoadPgm: write a program in
 * the program-file format and import it through the official loader,
 * which appends it and registers the global label. The Test-suffixed
 * name is the one the test HAL maps ioPathLoadProgram to. */
/* Fixture labels must be unique across this file: findNamedLabel
 * returns the first match, and nothing clears program memory between
 * fixtures. Identical bytes under the same name are accepted as a
 * re-run. */
static void ppcTestNoteLabel(const uint8_t *pgm, size_t n) {
  /* Sized against the suite (83 distinct labels today) with headroom.
   * The overflow arm fails on purpose: a registry that silently stops
   * registering reports the same green as one that works. */
  #define PPC_LABEL_SEEN_MAX 160
  static char     seenName[PPC_LABEL_SEEN_MAX][8];
  static uint32_t seenSum[PPC_LABEL_SEEN_MAX];
  static uint16_t seenCount = 0;

  if(n < 4 || pgm[0] != ITM_LBL || pgm[1] != STRING_LABEL_VARIABLE) {
    return;
  }
  uint8_t len = pgm[2];
  if(len == 0 || len > 7 || (size_t)(3 + len) > n) {
    return;
  }
  char name[8];
  for(uint8_t i = 0; i < len; i++) {
    name[i] = (char)pgm[3 + i];
  }
  name[len] = 0;

  uint32_t sum = (uint32_t)n;
  for(size_t i = 0; i < n; i++) {
    sum = sum * 31u + pgm[i];
  }
  for(uint16_t i = 0; i < seenCount; i++) {
    if(strcmp(seenName[i], name) == 0) {
      if(seenSum[i] != sum) {
        char msg[64];
        snprintf(msg, sizeof(msg), "label %s is defined by two different fixtures", name);
        ppTestFail(msg);
      }
      return;
    }
  }
  if(seenCount < PPC_LABEL_SEEN_MAX) {
    strcpy(seenName[seenCount], name);
    seenSum[seenCount] = sum;
    seenCount++;
  }
  else {
    char msg[64];
    snprintf(msg, sizeof(msg), "label table full at %s: collisions past here are unchecked", name);
    ppTestFail(msg);
  }
}

static void ppcTestWriteAndLoadPgm(const uint8_t *pgm, size_t n) {
  ppcTestNoteLabel(pgm, n);
  FILE *f = fopen("c47programTest.bin", "wb");
  if(f == NULL) {
    ppTestFail("cannot open c47programTest.bin");
    return;
  }
  fprintf(f, "PROGRAM_FILE_FORMAT\n0\nC47_program_file_version\n1\nPROGRAM\n%u\n", (unsigned)n);
  for(size_t i = 0; i < n; ++i) {
    fprintf(f, "%u\n", pgm[i]);
  }
  fclose(f);
  fnLoadProgram(NOPARAM);
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

  // T1: 2 ENTER 3 + 4 x, one formula. A consumed root continues it.
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

  // T2: supersession, a new root not consuming (2+3) emits it
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

  // T5: monadic result consumed by a dyadic, one formula
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_1ONX);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 %s 3 %s", n1ONX, nADD);
  ppcTestExpectSig("T5 chain through monadic", expect);
  ppcTestExpectHist("T5 hist", 0);

  // T6: CLX displaces, the natural explicit terminator
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

  // T9: swap mirrored, operand order flips
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

  // T12: arena exhaustion invalidates mid-chain, then the engine
  // rebuilds from value-leaf upgrades. The tail of the chain reads
  // "# 1 + 1 +", with # for register Y's live value.
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

  // T15: eRPN ENTER does not dup, the consumed Y upgrades to a value.
  // nimWhenButtonPressed is keyboard-owned. The driver mimics the real
  // keypress (a NIM was open when ENTER went down), so fnKeyEnter's
  // eRPN condition, and the shadow's mirror of it, sees the true state.
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

  // T17: a numbered-register recall keeps its NAME in the chain
  ppcTestReset();
  ppcTestType("3");
  ppcTestOpParam(ITM_STO, 5);
  ppcTestType("2");
  ppcTestOpParam(ITM_RCL, 5);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 R05 %s", nMULT);
  ppcTestExpectSig("T17 RCL name", expect);
  ppcTestExpectHist("T17 hist", 0);

  // T18: recalling a STACK register deep-copies its tree. Using the
  // copy supersedes (emits) the original still sitting higher up
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOpParam(ITM_RCL, REGISTER_Y);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "4 2 3 %s %s", nADD, nMULT);
  ppcTestExpectSig("T18 RCL stack copy", expect);
  ppcTestExpectHist("T18 hist", 1);

  // T19: RCL+ builds a dyadic node with the plain operator
  ppcTestReset();
  ppcTestType("10");
  ppcTestOpParam(ITM_STO, 7);
  ppcTestType("5");
  ppcTestOpParam(ITM_RCLADD, 7);
  sprintf(expect, "5 R07 %s", nADD);
  ppcTestExpectSig("T19 RCL-arith", expect);

  // T20: x<>reg emits the departing tree and leaves a truthful
  // value leaf for the register's old content
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOpParam(ITM_Xex, 9);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "# 4 %s", nMULT);
  ppcTestExpectSig("T20 x<>reg", expect);
  ppcTestExpectHist("T20 hist", 1);

  /* T20b: FILL displaces, it does not delete. FILL overwrites Y..top
   * with X: displacement under DESIGN.md's segmentation rule.
   * A finished formula in a slot >= 1 must be filed, the same as the
   * CLSTK wipe site files it. The CLSTK control below checks the same
   * shadow state through a wipe site that does displace. */
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);      // 2+3 finished, root now rides up on the next lift
  ppcTestType("9");        // lifts the root into slot 1
  ppcTestOp(ITM_FILL);
  ppcTestExpectHist("T20b FILL files the displaced formula", 1);

  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("9");
  ppcTestOp(ITM_CLSTK);
  ppcTestExpectHist("T20b control: CLSTK files it", 1);

  // T21: RCL-arith against a slot that is UNKNOWN.
  // ppcDeepCopy returns PPC_UNKNOWN unchanged. The guard beside it
  // tests only PPC_NIL, so the sentinel is stored as a child. Every
  // consumer screens PPC_UNKNOWN, so no crash follows and nothing
  // claims to be a formula. The four asserts below check that, in
  // order, through the raw accessor.
  ppcTestReset();
  ppcTestType("5");
  ppcTestOpParam(ITM_RCLADD, (uint16_t)REGISTER_Z);   // slot 2 is UNKNOWN
  {
    char sig[128];
    ppcTestSig(sig, sizeof(sig));                     // must not crash

    // (1) the operation was classified and built a tree
    uint8_t raw = ppcTestCurrentRaw();
    if(raw == PPC_NIL) {
      ppTestFail("T21 RCL-arith built no tree at all — fixture never reached the state under test");
    }
    else {
      // (2) the sentinel is stored as a child, exactly as designed
      const ppcNode_t *nd = ppcNodeAt(raw);
      if(nd == NULL || nd->child[1] != PPC_UNKNOWN) {
        ppTestFail("T21 the UNKNOWN operand is not the right-hand child");
      }
      // (3) and the display path withholds it
      if(ppcCurrentFormulaRoot() != PPC_NIL) {
        ppTestFail("T21 a tree with an UNKNOWN operand was offered for display");
      }
      uint8_t built;
      ppReset();
      if(ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &built)) {
        ppTestFail("T21 a tree with an UNKNOWN operand was rendered as a formula");
      }
    }
    // (4) nothing was filed into history
    ppcTestExpectHist("T21 nothing emitted", 0);
  }

  // T22 (class test): the recorded "= result" must be the value the
  // formula actually had. ppcTestExpectHist compares counts only and
  // cannot see this class. The PPC_INVALIDATE emit RAN at DONE then,
  // after dispatch had overwritten the register it read. It now emits
  // at STAGE, while the register still holds the formula's value.
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3.7");
    ppcTestOp(ITM_ADD);                 // X = 5.7, formula 2 + 3.7
    ppcTestOp(ITM_IP);                  // unmodelled US_ENABLED -> invalidate
    uint16_t elen, eseq;
    const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
    if(e == NULL) {
      ppTestFail("T22 the superseded formula was not filed at all");
    }
    else {
      uint8_t root;
      ppReset();
      // withResult=true: if a result was recorded, it becomes an
      // " = x" tail. The correct outcomes are "no result recorded" or
      // "= 5.7". The post-dispatch value 5 must not appear.
      if(ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(root, sig, sizeof(sig));
        if(strstr(sig, "=") != NULL && strstr(sig, "5.7") == NULL) {
          ppTestFailures++;
          printf("prettyPrint test FAIL: T22 filed a result the formula never had ('%s')\n", sig);
        }
      }
    }
    lastErrorCode = 0;
  }

  /* T22b: a complex operand must be captured. A
   * complex34 is 32 bytes against a 16-byte node. PPN_VAL2 is the
   * two-child header DESIGN.md section 3 defines for it.
   *
   * Fixture: put a complex in Y, leave the shadow UNKNOWN there, then
   * multiply. STAGE's ppcEnsureKnown(1) must snapshot the complex. */
  {
    ppcTestReset();
    /* The complex goes in X, not Y. Typing the next literal lifts, so
     * the complex ends up in Y. The multiply's STAGE must snapshot it
     * there. */
    reallocateRegister(REGISTER_X, dtComplex34, 0, amNone);
    int32ToReal34(2, REGISTER_REAL34_DATA(REGISTER_X));
    int32ToReal34(3, REGISTER_IMAG34_DATA(REGISTER_X));
    ppcShadowInvalidate();          // both slots UNKNOWN
    ppcTestType("4");               // lifts: X=4, Y=the complex
    if(getRegisterDataType(REGISTER_Y) != dtComplex34) {
      ppTestFail("T22b setup: Y is not complex after the lift — fixture cannot reach the defect");
    }
    ppcTestOp(ITM_MULT);
    /* The T-line half: the tree must not be poisoned by an opaque leaf. */
    if(ppcCurrentFormulaRoot() == PPC_NIL) {
      ppTestFail("T22b a complex operand still withholds the whole formula");
    }
    /* The LIVE half is not pinned. ppfFromCaptureNode reassembles the
     * PPN_VAL2 continuation, which avoids a 16-byte overread past
     * nd->payload. Asserting the drawn text needs C47's complex
     * formatting spelled out, and a guessed expectation only
     * enshrines whatever ppfFormatStaged emits. Pin it against the
     * formatter's real output when available. */
    {
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T22b the live formula does not build with a complex operand");
      }
    }

    /* The history half. A formula files on displacement, so the wipe
     * is part of the fixture. */
    ppcTestOp(ITM_CLSTK);
    ppcTestExpectHist("T22b the complex formula files", 1);
  }

  /* T23b: the filed "= result" must stay the formula's own value
   * after a later STO. The pin reads the filed entry. */
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                             // X=5, the formula 2+3
    ppcTestType("9");                               // lifts: Y=5, X=9
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);  // Y := 9, displacing the formula
    ppcTestExpectHist("T23b the displaced formula files", 1);

    uint8_t filed = PP_NONE;
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                      PP_FONT_STANDARD, true, &filed)) {
      ppTestFail("T23b the filed entry does not decode");
    }
    else {
      if(!ppTreeHasRun(filed, "5")) {
        ppTestFail("T23b the filed result is not 5, the value the formula had");
      }
      if(ppTreeHasRun(filed, "9")) {
        ppTestFail("T23b the filed result is 9 — the value STO wrote, read after the store");
      }
    }
  }

  /* T24c: a stacked power must bracket its base on the capture
   * surfaces too. PP_SUP puts the outer exponent at the same height as
   * the inner one, so an unbracketed 3 cubed cubed draws flat and
   * reads as 3^33 for a value of 3^9. The builder brackets the base,
   * so no call site needs its own guard. */
  {
    ppcTestReset();
    ppcTestType("3");
    ppcTestOp(ITM_CUBE);
    ppcTestOp(ITM_CUBE);
    uint8_t pw = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &pw)) {
      ppTestFail("T24c the stacked power does not build");
    }
    else {
      ppfTestExpect("T24c stacked power brackets its base", pw, "S(P(S(3|3))|3)");
    }
  }

  /* T26: a short integer through ppfBuildEntry. dtShortInteger
   * formatting uses tmpString, sized against ERROR_MESSAGE_LENGTH and
   * checked by a _Static_assert in prettyFormula.c.
   * The operand's spelling depends on the base, so the assertion is
   * only that the entry decodes to text at all. */
  {
    ppcTestReset();
    ppcTestType("10");
    ppcTestOpParam(ITM_toINT, 16);   // integer mode, base 16
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T26 the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcTestType("5");
      ppcTestOp(ITM_ADD);
      ppcTestOp(ITM_CLSTK);          // displacing the formula files it
      ppcTestExpectHist("T26 the integer formula files", 1);
      uint8_t filed = PP_NONE;
      ppReset();
      if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                        PP_FONT_STANDARD, true, &filed)) {
        ppTestFail("T26 the filed integer entry does not decode");
      }
      else if(ppfTestFirstRunText(filed) == NULL
              || ppfTestFirstRunText(filed)[0] == 0) {
        ppTestFail("T26 the filed integer formula has no operand text");
      }
    }
    ppcTestReset();
  }

  /* T25: the class, over every producer of a PP_SUP. Rows are (steps,
   * expected signature). ppfTestPowersScoped checks the property over
   * each row's whole tree, which catches a nested producer inside a
   * shape a row already types. It does not reach a new producer that
   * no row drives, a fourth PP_SUP arm still needs its own row here. */
  {
    static const struct { const char *what; uint16_t op1; const char *lit; uint16_t op2; const char *sig; } powRows[] = {
      { "T25 x2 over x2",  ITM_SQUARE, NULL, ITM_SQUARE, "S(P(S(3|2))|2)" },
      { "T25 x3 over x3",  ITM_CUBE,   NULL, ITM_CUBE,   "S(P(S(3|3))|3)" },
      { "T25 yx over x2",  ITM_SQUARE, "2",  ITM_YX,     "S(P(S(3|2))|2)" },
    };
    for(uint8_t r = 0; r < sizeof(powRows) / sizeof(powRows[0]); r++) {
      ppcTestReset();
      ppcTestType("3");
      ppcTestOp((int16_t)powRows[r].op1);
      if(powRows[r].lit != NULL) {
        ppcTestType(powRows[r].lit);
      }
      ppcTestOp((int16_t)powRows[r].op2);
      uint8_t pw = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &pw)) {
        ppTestFail(powRows[r].what);
      }
      else {
        ppfTestExpect(powRows[r].what, pw, powRows[r].sig);
        if(!ppfTestPowersScoped(pw)) {
          ppTestFail("T25 a power's base is itself a power, unbracketed");
        }
        // the same picture must survive filing (PP18RR7-1's class)
        ppfTestFiledMatchesLive(powRows[r].what);
      }
    }

    /* yx over yx: two operands, so the base is built by the OP2 arm
     * from a node the OP2 arm built. It reaches the same builder on
     * both capture surfaces: the live tree here, the filed entry
     * below through the token decoder. */
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_YX);
    ppcTestType("2");
    ppcTestOp(ITM_YX);
    uint8_t yx = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &yx)) {
      ppTestFail("T25 yx over yx does not build");
    }
    else {
      ppfTestExpect("T25 yx over yx brackets its base", yx, "S(P(S(2|3))|2)");
    }

    ppcTestOp(ITM_CLSTK);   // displacing the formula files it
    ppcTestExpectHist("T25 the stacked power files", 1);
    uint8_t filed = PP_NONE;
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                      PP_FONT_STANDARD, false, &filed)) {
      ppTestFail("T25 the filed stacked power does not decode");
    }
    else if(!ppfTestPowersScoped(filed)) {
      ppTestFail("T25 the FILED power's base is itself a power, unbracketed");
    }

    /* The base need not be a PP_SUP node. A leaf read from a
     * register is formatted for display. A large value arrives as one
     * flat run that ends in its own exponent's superscript digits,
     * and a kind test reads (1x10^50) squared as an exponent of 502.
     * A typed literal keeps its typed text and never has the tail, so
     * this row uses RCL.
     * The SUB-10 glyph is the reach check: without it the row passes
     * while testing nothing. */
    /* The class is not "already a power", it is "the base run is not
     * a visual atom". A typed negative reads as a term. Both the
     * capture leaf and the walker ask ppfTextIsAtom, so they cannot
     * disagree. */
    ppcTestReset();
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_SQUARE);
    uint8_t neg = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &neg)) {
      ppTestFail("T25 the negated square does not build");
    }
    else {
      const char *nleaf = ppfTestFirstRunText(neg);
      if(nleaf == NULL || nleaf[0] != '-') {
        ppTestFail("T25 the literal is not negative, so the row tests nothing");
      }
      else {
        ppfTestExpect("T25 a signed numeral brackets as a term", neg, "S(P(-5)|2)");
        ppfTestFiledMatchesLive("T25 filed signed numeral");
      }
    }

    /* The typed form of the same value is the other half of the class.
     * It keeps the owner's text, so its exponent is ASCII (1.e+50),
     * and the glyph test cannot see it. */
    ppcTestReset();
    ppcTestType("1");
    addItemToNimBuffer(ITM_EXPONENT);
    ppcTestType("50");
    ppcTestOp(ITM_SQUARE);
    uint8_t typed = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &typed)) {
      ppTestFail("T25 the typed scientific power does not build");
    }
    else {
      const char *tleaf = ppfTestFirstRunText(typed);
      const ppNode_t *troot = ppNodeAt(typed);
      const ppNode_t *tbase = (troot != NULL) ? ppNodeAt(troot->firstChild) : NULL;
      if(tleaf == NULL || strstr(tleaf, "e+") == NULL) {
        ppTestFail("T25 the typed value carries no ASCII exponent, so the row tests nothing");
      }
      else if(troot == NULL || troot->kind != PP_SUP) {
        ppTestFail("T25 the typed scientific value is not a power");
      }
      else if(tbase == NULL || tbase->kind != PP_PAREN) {
        ppTestFail("T25 a squared typed scientific value draws its exponent against the owner's");
      }
      ppfTestFiledMatchesLive("T25 filed typed scientific power");
    }

    ppcTestReset();
    ppcTestType("1");
    addItemToNimBuffer(ITM_EXPONENT);
    ppcTestType("50");
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);
    ppcTestOpParam(ITM_RCL, (uint16_t)REGISTER_Y);
    ppcTestOp(ITM_SQUARE);
    uint8_t sci = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &sci)) {
      ppTestFail("T25 the scientific-form power does not build");
    }
    else {
      const char *leaf = ppfTestFirstRunText(sci);
      const ppNode_t *root = ppNodeAt(sci);
      const ppNode_t *base = (root != NULL) ? ppNodeAt(root->firstChild) : NULL;
      if(leaf == NULL || strstr(leaf, STD_SUB_10) == NULL) {
        ppTestFail("T25 the value never reached scientific form, so the row tests nothing");
      }
      else if(root == NULL || root->kind != PP_SUP) {
        ppTestFail("T25 the squared scientific value is not a power");
      }
      else if(base == NULL || base->kind != PP_PAREN) {
        // a structural check
        ppTestFail("T25 a squared scientific value draws its two exponents as one");
      }
      ppfTestFiledMatchesLive("T25 filed scientific power");
    }
  }

  /* T27 (PP18RR7-1): the filed picture equals the live one for the
   * bracket-bearing operand shapes under MULT and SUB. */
  {
    ppcTestReset();
    ppcTestType("3");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_MULT);
    ppfTestFiledMatchesLive("T27 filed MULT keeps the signed bracket");

    ppcTestReset();
    ppcTestType("7");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_SUB);
    ppfTestFiledMatchesLive("T27 filed SUB keeps the signed bracket");
    ppcTestReset();
  }

  /* T28 (PP18RR7-2): for every base, the widest word must decode on
   * the filed surface. Upstream draws each of these on one line. */
  {
    static const uint16_t bases[] = { 2, 4, 8, 10, 16 };
    for(size_t b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
      ppcTestReset();
      ppcTestType("0");
      ppcTestOpParam(ITM_toINT, bases[b]);
      ppcTestOp(ITM_ENTER);
      ppcTestType("1");
      ppcTestOpParam(ITM_toINT, bases[b]);
      ppcTestOp(ITM_SUB);
      if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
        ppTestFailInt("T28 the value is not a short integer, so the row tests nothing",
                      (int)bases[b], (int)getRegisterDataType(REGISTER_X));
        continue;
      }
      ppcTestOp(ITM_CLSTK);
      uint8_t filed = PP_NONE;
      ppReset();
      if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                        PP_FONT_STANDARD, true, &filed)) {
        ppTestFailInt("T28 the widest word does not decode in this base",
                      (int)bases[b], 0);
      }
    }
    ppcTestReset();
  }

  /* T29 (PP18RR8-1): an unknown glyph fails the run in EVERY font.
   * findGlyph's id-based fallback reports a tinyFont miss as glyph 0,
   * so the eˣ catalog name (0xa147 0x82e3) measured and painted as
   * blanks. Digit-group spaces stay measurable: they are space-class
   * in every font. */
  {
    ppReset();
    uint8_t bad = ppNewRun("\xa1\x47", 2, PP_FONT_TINY);
    if(bad == PP_NONE) {
      ppTestFail("T29 the run does not build, so the row tests nothing");
    }
    else if(ppMeasure(bad, 0)) {
      ppTestFail("T29 a glyph tinyFont lacks still measures");
    }
    ppReset();
    uint8_t sep = ppNewRun("1\xa0\x08" "2", 4, PP_FONT_TINY);
    if(sep == PP_NONE || !ppMeasure(sep, 0)) {
      ppTestFail("T29 a digit-group space must stay measurable in the tiny font");
    }
    ppReset();
    uint8_t badStd = ppNewRun("\xff\xfe", 2, PP_FONT_STANDARD);
    if(badStd == PP_NONE || ppMeasure(badStd, 0)) {
      ppTestFail("T29 an unknown glyph measures in the standard font");
    }
  }

  /* T30 (PP18RR8-3): when the text pool cannot hold the result run,
   * the entry DECLINES rather than paint without its "= result" tail.
   * LEAD.0 in base 2 at WSIZE 64 spells each value at 160 bytes, so
   * three value leaves and two operators leave no room for the tail
   * (the recall path would still find the TKRES and push a number the
   * row never showed). */
  {
    setSystemFlag(FLAG_LEAD0);
    ppcTestReset();
    ppcTestType("0");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_SUB);
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T30 the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcTestOp(ITM_ENTER);
      ppcTestOpParam(ITM_toINT, 2);   // unclassified: the slots go UNKNOWN
      ppcTestOp(ITM_ADD);             // each ADD mints a PPN_VAL leaf
      ppcTestOp(ITM_ADD);
      ppcTestOp(ITM_CLSTK);           // displacing the formula files it
      uint8_t filed = PP_NONE;
      ppReset();
      if(ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                       PP_FONT_STANDARD, true, &filed)) {
        ppTestFail("T30 a filed entry whose result run cannot fit painted without its tail");
      }
    }
    clearSystemFlag(FLAG_LEAD0);
    ppcTestReset();
  }

  /* T31 (PP18RR8-5): a polar-tagged complex keeps its polar spelling
   * on the formula view. The staged formatter passed a literal false
   * for tagPolar, so the leaf redrew as a+ib while the stack line
   * showed the polar form. 0xa221 is the measured-angle glyph. */
  {
    ppcTestReset();
    reallocateRegister(REGISTER_X, dtComplex34, 0, amNone);
    int32ToReal34(3, REGISTER_REAL34_DATA(REGISTER_X));
    int32ToReal34(4, REGISTER_IMAG34_DATA(REGISTER_X));
    setComplexRegisterPolarMode(REGISTER_X, amPolar);
    if(getComplexRegisterPolarMode(REGISTER_X) != amPolar) {
      ppTestFail("T31 the tag is not polar, so the row tests nothing");
    }
    else {
      ppcShadowInvalidate();
      ppcTestType("2");
      ppcTestOp(ITM_MULT);
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T31 the polar product does not build, so the row tests nothing");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(live, sig, sizeof(sig));
        if(strstr(sig, "\xa2\x21") == NULL) {
          ppTestFail("T31 a polar-tagged value leaf redrew in rectangular form");
        }
      }
    }
    ppcTestReset();
  }

  /* FB1 (PP18RR8-2): the flag browser walks the catalog this build
   * actually generated. The row derives the row count the same way the
   * fixed browser does, then drives both system-flag screens. */
  {
    int16_t rows = -1;
    for(uint16_t m = 0; softmenu[m].menuItem != 0; m++) {
      if(softmenu[m].menuItem == -MNU_SYSFL) {
        rows = softmenu[m].numItems;
        break;
      }
    }
    if(rows < 61) {
      ppTestFailInt("FB1 the SYSFL catalog is too short to fill two screens", 61, (int)rows);
    }
    else {
      lastErrorCode = 0;
      flagBrowser(SYSTEM_FLAGS_SCREEN_1);
      currentFlgScr = SYSTEM_FLAGS_SCREEN_2;
      flagBrowser(SYSTEM_FLAGS_SCREEN_2);
      if(lastErrorCode != ERROR_NONE) {
        ppTestFailInt("FB1 the flag browser raised an error", 0, (int)lastErrorCode);
      }
      calcMode = CM_NORMAL;
      currentFlgScr = 0;
    }
    ppcTestReset();
  }

  /* T23c: a slot must be maintained wherever its register is
   * writable, even where the live stack does not reach. Fixture: fill
   * the slots under SSIZE8, switch to SSIZE4, store over A. The
   * shadow must no longer claim to know slot 4. */
  {
    bool_t ss8Was = getSystemFlag(FLAG_SSIZE8);
    setSystemFlag(FLAG_SSIZE8);
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ENTER);
    ppcTestType("4");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");                                 // slots 0..4 known

    /* Establish the state the assertion needs: slot 4 must actually hold a
     * tree before the store, or the pin proves nothing. */
    const uint8_t s4 = ppcTestSlotRaw(4);
    const bool_t slot4WasKnown = (s4 != PPC_UNKNOWN && s4 != PPC_NIL);

    clearSystemFlag(FLAG_SSIZE8);                     // A..D leave the stack, stay writable
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_A);    // upstream writes A = 5
    setSystemFlag(FLAG_SSIZE8);                       // and A is a stack register again

    /* Degraded means UNKNOWN. What it must not be is the tree it held
     * before the store. PPC_NIL is not accepted as degraded: it is
     * also what ppcTestSlotRaw returns out of range, so accepting it
     * lets the pin pass on a slot that was never populated. */
    if(!slot4WasKnown) {
      ppTestFail("T23c setup: slot 4 never held a tree, so the guard below cannot be seen");
    }
    else {
      const uint8_t slot4 = ppcTestSlotRaw(4);
      if(slot4 != PPC_UNKNOWN) {
        ppTestFail("T23c slot 4 was not degraded after a STO to A the guard ignored");
      }
    }
    if(ss8Was) { setSystemFlag(FLAG_SSIZE8); } else { clearSystemFlag(FLAG_SSIZE8); }
  }

  // T23: STO to a STACK register changes a value the shadow
  // claims. 7 ENTER 2 ENTER 3 + STO Y x: the display shows 7·(2+3)=25.
  {
    ppcTestReset();
    ppcTestType("7");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                          // X=5, Y=7
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);  // Y := 5
    ppcTestOp(ITM_MULT);                         // X = 5*5 = 25
    {
      // Assert the whole truthful shape: the overwritten Y degraded to
      // a value leaf, times the live (2+3).
      char expect23[64];
      sprintf(expect23, "# 2 3 %s %s",
              indexOfItems[ITM_ADD].itemCatalogName,
              indexOfItems[ITM_MULT].itemCatalogName);
      ppcTestExpectSig("T23 STO Y leaves a truthful value leaf, not the 7", expect23);

      char sig[128];
      ppcTestSig(sig, sizeof(sig));
      if(strstr(sig, "7") != NULL) {
        // sanitize: a signature carrying glyph bytes makes grep treat the
        // whole log as binary and swallow the FAIL line (TESTING.md trap)
        char safe[128];
        uint16_t si = 0;
        for(; sig[si] && si < sizeof(safe) - 1; si++) {
          safe[si] = ((uint8_t)sig[si] >= 32 && (uint8_t)sig[si] < 127) ? sig[si] : '?';
        }
        safe[si] = 0;
        ppTestFailures++;
        printf("prettyPrint test FAIL: T23 the shadow kept the overwritten register ('%s')\n", safe);
      }
    }
  }

  // T25: a formula too wide for the screen must still be in the
  // browser and pannable. Height stays a hard limit, width does not.
  {
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestOp(ITM_CLX);                 // file it
    ppcTestExpectHist("T25 the wide formula was filed", 1);
    {
      uint8_t root;
      int16_t asc, h;
      if(!ppfBuildRow(0, 0, true, &root, &asc, &h)) {
        ppTestFail("T25 a wide row is still dropped instead of panned");
      }
      else {
        const ppNode_t *n = ppNodeAt(root);
        if(n->width <= SCREEN_WIDTH - 8) {
          ppTestFail("T25 fixture is not actually wide enough to exercise panning");
        }
      }
    }
  }

  // T26: the literal-length boundary. The leaf holds two 15-byte
  // payloads, 30 characters. 30 must round-trip exactly. 31 must
  // withhold the formula.
  {
    static const char d30[] = "123456789012345678901234567890";
    static const char d31[] = "1234567890123456789012345678901";
    char expect26[64];
    const char *nADD26 = indexOfItems[ITM_ADD].itemCatalogName;

    ppcTestReset();
    ppcTestType(d30);
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ADD);
    sprintf(expect26, "%s 2 %s", d30, nADD26);
    ppcTestExpectSig("T26 a 30-character literal must round-trip", expect26);

    ppcTestReset();
    ppcTestType(d31);
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ADD);
    {
      // The formula is withheld, so the signature is empty.
      ppcTestExpectSig("T26 a 31-character literal withholds the formula", "-");

      char sig[128];
      ppcTestSig(sig, sizeof(sig));
      if(strstr(sig, d30) != NULL) {
        ppTestFail("T26 a 31-character literal was truncated to 30 and shown as fact");
      }
    }
  }

  // T27: a superseded formula must still be recallable.
  // The emit happens at STAGE, where the register still holds the
  // value.
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3.7");
    ppcTestOp(ITM_ADD);                 // X = 5.7
    ppcTestOp(ITM_IP);                  // unmodelled -> invalidate + supersede
    uint16_t elen, eseq;
    const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
    if(e == NULL) {
      ppTestFail("T27 the superseded formula was not filed");
    }
    else {
      uint8_t root;
      ppReset();
      if(!ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
        ppTestFail("T27 the filed entry does not decode");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(root, sig, sizeof(sig));
        // it must carry a result, and that result must be the true one
        if(strstr(sig, "=") == NULL) {
          ppTestFail("T27 the filed formula has no result and can never be recalled");
        }
        else if(strstr(sig, "5.7") == NULL) {
          ppTestFail("T27 the filed result is not the value the formula had");
        }
      }
    }
    lastErrorCode = 0;
  }

  // T28: ppfBuildRow has two callers, and only one can pan. The same
  // wide row must be accepted for the panning caller and refused for
  // the one that cannot.
  {
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestOp(ITM_CLX);
    uint8_t root;
    int16_t asc, h;
    bool_t panning = ppfBuildRow(0, 0, true,  &root, &asc, &h);
    bool_t fixed   = ppfBuildRow(0, 0, false, &root, &asc, &h);
    if(!panning) {
      ppTestFail("T28 the panning caller lost the wide row again");
    }
    if(fixed) {
      ppTestFail("T28 the non-panning caller would paint a clipped formula");
    }
  }

  // T29: the browser's pan must reach the code, and the paint must
  // survive a negative origin. lcd_fill_rect takes uint32_t
  // coordinates, so a negative x wraps to a huge one and drops the
  // whole rule, while glyph ink still clips.
  {
    int16_t hadScrUpd = screenUpdatingMode;
    uint16_t hadMode = calcMode;
    screenUpdatingMode = SCRUPD_AUTO;
    temporaryInformation = TI_NO_INFO;
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestType("7");
    ppcTestOp(ITM_DIV);          // wide numerator under a long fill-drawn bar
    ppcTestOp(ITM_CLX);
    prettyBrowser(NOPARAM);
    {
      uint8_t rr; int16_t aa, hh;
      if(!ppfBuildRow(0, 0, true, &rr, &aa, &hh)
          || ppNodeAt(rr)->width <= SCREEN_WIDTH - 12) {
        ppTestFail("T29 fixture is not wide enough to pan; the pin is not exercising anything");
      }
    }
    refreshScreen(200);
    uint32_t runBefore = 0;
    for(uint32_t y = 21; y <= 167 && runBefore == 0; y++) {
      uint32_t run = 0;
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        run = lcd_buffer_pixel_on(x, y) ? run + 1 : 0;
        if(run >= 20) { runBefore = run; break; }
      }
    }
    prettyBrowserPan();
    refreshScreen(201);
    uint32_t runAfter = 0;
    for(uint32_t y = 21; y <= 167 && runAfter == 0; y++) {
      uint32_t run = 0;
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        run = lcd_buffer_pixel_on(x, y) ? run + 1 : 0;
        if(run >= 20) { runAfter = run; break; }
      }
    }
    if(runBefore == 0) {
      ppTestFail("T29 the unpanned row has no fill-drawn rule to test");
    }
    else if(runAfter == 0) {
      ppTestFail("T29 panning dropped the fill-drawn rule instead of clipping it");
    }
    prettyBrowserLeave();
    calcMode = hadMode;
    screenUpdatingMode = hadScrUpd;
    lastErrorCode = 0;
  }

  // P13: repainting the X line must not leave the previous value's
  // ink behind. This is safe only because the register line's own
  // refresh clears the band first. Upstream's clearRegisterLine()
  // calls are commented out at their call sites, so this dependency
  // matters. Measured: a 35-digit value, then a 3-glyph one repainted
  // over it, leaves 467 lit pixels. A short value on a freshly
  // cleared band paints the same count.
  {
    ppTestSetRealX("0.16666666666666666666666666666666667");
    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);

    ppTestSetRealX("0.75");
    refreshRegisterLine(REGISTER_X);       // deliberately NOT cleared first
    uint32_t over = 0;
    for(uint32_t y = PPT_BAND_TOP; y < PPT_BAND_TOP + PPT_BAND_ROWS; y++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, y)) over++;
      }
    }

    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);
    uint32_t clean = 0;
    for(uint32_t y = PPT_BAND_TOP; y < PPT_BAND_TOP + PPT_BAND_ROWS; y++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, y)) clean++;
      }
    }

    if(clean == 0) {
      ppTestFail("P13 fixture painted nothing; the pin is not exercising anything");
    }
    else if(over != clean) {
      ppTestFailInt("P13 a repaint left the previous value's ink behind", (int32_t)clean, (int32_t)over);
    }
  }

  // P12: a fraction's numerator ink must not depend on which glyph
  // the denominator is. Nothing about FRAC layout gives the
  // denominator any say over the rows above the bar. num.relBase is
  // barTopRel - fracGap - num.descent, none of which reads the
  // denominator. The same numerator over three denominators of very
  // different ink height must light exactly the same pixels. Measured
  // over the numerator's own columns, so re-centering cannot flatter
  // it.
  {
    const char *dens[3] = { "8", "x", "." };
    uint32_t ink[3] = { 0, 0, 0 };

    for(int c = 0; c < 3; c++) {
      ppReset();
      uint8_t fr = ppNewBox(PP_FRAC, PP_FONT_STANDARD);
      uint8_t nn = ppNewRun("8", 1, PP_FONT_STANDARD);
      uint8_t dd = ppNewRun(dens[c], 1, PP_FONT_STANDARD);
      if(fr == PP_NONE || nn == PP_NONE || dd == PP_NONE) { ppTestFail("P12 build"); break; }
      ppAppendChild(fr, nn);
      ppAppendChild(fr, dd);
      if(!ppMeasure(fr, 0)) { ppTestFail("P12 measure"); break; }

      lcd_fill_rect(0, 60, SCREEN_WIDTH, 100, LCD_SET_VALUE);
      ppPaintAt(fr, 40, 120);

      // the numerator's own measured ink box, x and y both
      const ppNode_t *n = ppNodeAt(nn);
      const uint32_t nx0 = (uint32_t)(40 + n->relX);
      const uint32_t nx1 = nx0 + (uint32_t)n->width;
      const uint32_t ytop = (uint32_t)(120 + n->relBase - n->ascent);
      const uint32_t ybot = (uint32_t)(120 + n->relBase + n->descent);
      for(uint32_t y = ytop; y < ybot; y++) {
        for(uint32_t x = nx0; x < nx1; x++) {
          if(lcd_buffer_pixel_on(x, y)) ink[c]++;
        }
      }
    }

    if(ink[0] == 0) {
      ppTestFail("P12 numerator ink missing entirely");
    }
    else if(ink[1] != ink[0] || ink[2] != ink[0]) {
      printf("prettyPrint P12 probe: numerator ink over 8/8=%u 8/x=%u 8/.=%u\n",
             ink[0], ink[1], ink[2]);
      ppTestFail("P12 denominator glyph ate the numerator");
    }
  }

  // T16: abort while ASLIFT is set (straight after an operator
  // result). The deferred-lift design absorbs the upstream undo() for
  // free. A shadow that lifts at NIM open strands the tree one slot
  // up.
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

  /* ==== Big operators ================================================== */

  // the label program: LBL "P" / x^2 / END (the pgmT shape from upstream's
  // covProgramFlow, with a package-local label name)
  {
    static const uint8_t pgmP[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 1, 'P',
      ITM_SQUARE,
      (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
    };
    ppcTestWriteAndLoadPgm(pgmP, sizeof(pgmP));
  }
  calcRegister_t bigLbl = findNamedLabel("P", GLOBAL_LABELS);
  if(bigLbl == INVALID_VARIABLE) {
    ppTestFail("B0 label P not registered");
  }
  else {
    // The integral/sum dispatches retarget the solver at label P and
    // clear SOLVER_STATUS_USES_FORMULA. Later suite files (deriv_cov)
    // assume the status they inherited. Drivers restore what they
    // touch, the same rule that covers aimBuffer.
    uint16_t savedSolverStatus   = currentSolverStatus;
    uint16_t savedSolverProgram  = currentSolverProgram;
    uint16_t savedSolverVariable = currentSolverVariable;
    calcRegister_t savedMvarLabel = currentMvarLabel;

    const char *nSIG = indexOfItems[ITM_SIGMAn].itemCatalogName;
    const char *nINT = indexOfItems[ITM_INTEGRAL_YX].itemCatalogName;

    // B1: 1 ENTER 10 ENTER 1 Sigma_n -> a BIGOP root whose value is the
    // result the dispatch left in X (sum of n^2, n=1..10 = 385)
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("10");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    sprintf(expect, "{#,#}%s", nSIG);
    ppcTestExpectSig("B1 sigma captured", expect);
    ppcTestExpectHist("B1 no early emission", 0);
    if(getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("B1 X not real34");
    }
    else {
      real34_t want;
      int32ToReal34(385, &want);
      if(!real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("B1 X != 385");
      }
    }

    // B2/B3: CLX displaces the BIGOP root -> it emits like any op root
    ppcTestOp(ITM_CLX);
    ppcTestExpectHist("B3 emitted on displacement", 1);

    // B4: decode the history entry through the layout builder. The
    // sig pins limit ORDER (under=from, over=to), the label-name
    // decode, and the node shape. withResult=false: the B1 value
    // check already pins the real34 result.
    {
      uint16_t elen, eseq;
      const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
      uint8_t root;
      ppReset();
      if(e == NULL || !ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_TINY, false, &root)) {
        ppTestFail("B4 history entry decode");
      }
      else {
        // the HBOX sig joiner space-separates children: [n= 1]
        ppfTestExpect("B4 layout", root, "B(P(n)|[n= 1]|10)");
        if(!ppMeasure(root, 0)) {
          ppTestFail("B4 measure");
        }
        else {
          // B8: pixel pin for the stroke-drawn operator. Probe rows
          // [base-12, base-2], cols [x, x+22]. The glyph box is at
          // most colW wide (about 13 here), and the body run starts
          // past colW + 3. Body ink shares these ROWS, so the narrow
          // column bound is what excludes it. A wider probe once
          // masked a stroke deletion.
          lcd_fill_rect(0, 60, SCREEN_WIDTH, 84, LCD_SET_VALUE);
          ppPaintAt(root, 10, 120);
          if(!ppTestRectAnyLit(108, 118, 10, 22)) {
            ppTestFail("B8 operator strokes missing");
          }
        }
      }
    }

    // B5: the BIGOP result chains like any operand, no early emission
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestType("2");
    ppcTestOp(ITM_MULT);
    sprintf(expect, "{#,#}%s 2 %s", nSIG, nMULT);
    ppcTestExpectSig("B5 sigma chains", expect);
    ppcTestExpectHist("B5 hist", 0);   // the reset above cleared the ring
    ppcTestOp(ITM_CLX);
    ppcTestExpectHist("B5 chained formula emitted", 1);

    // B10: a big operator whose program fails partway through must
    // leave no formula behind. The hooks nest: a dispatch runs a
    // program whose every step re-enters them.
    {
      static const uint8_t pgmE[] = {
        ITM_LBL, STRING_LABEL_VARIABLE, 1, 'E',
        ITM_LITERAL, STRING_REAL34, 1, '6',
        ITM_SUB,                                   // n - 6
        ITM_1ONX,                                  // 1/(n-6): divides by zero at n=6
        (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
      };
      ppcTestWriteAndLoadPgm(pgmE, sizeof(pgmE));
      calcRegister_t eLbl = findNamedLabel("E", GLOBAL_LABELS);
      if(eLbl != INVALID_VARIABLE) {
        ppcTestReset();
        ppcTestType("4");
        ppcTestOp(ITM_ENTER);
        ppcTestType("8");
        ppcTestOp(ITM_ENTER);
        ppcTestType("1");
        ppcTestOpParam(ITM_SIGMAn, (uint16_t)eLbl);
        // The only pin for the dispatch-depth pairing. Assert that we
        // reached the failure.
        if(lastErrorCode == ERROR_NONE) {
          ppTestFail("B10 fixture no longer fails mid-loop; the depth pin is not being exercised");
        }
        else {
          // the run failed: nothing claims to describe the register
          ppcTestExpectSig("B10 failed sum left a formula behind", "-");
        }
        lastErrorCode = 0;
      }
    }

    // B11: the browser must be able to recall a formula containing a
    // big operator. Two decoders read one token stream.
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);   // X = 55
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                             // supersede -> files the Sigma WITH its result
    if(ppcHistoryCount() < 1) {
      ppTestFail("B11 the sigma formula was not filed");
    }
    else {
      uint16_t hadMode = calcMode;
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
      int32ToReal34(-1, REGISTER_REAL34_DATA(REGISTER_X));   // a value the recall must replace
      prettyBrowser(NOPARAM);
      prettyBrowserDown();          // off the live row, onto the filed sigma
      prettyBrowserEnter();
      real34_t want55;
      int32ToReal34(55, &want55);
      if(getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want55)) {
        ppTestFail("B11 ENTER did not recall the big-operator result into X");
      }
      calcMode = hadMode;
      lastErrorCode = 0;
    }

    #if defined(OPTION_INFSUMS)
    // B9: the early-stop sum captures like any other sum. It reads the
    // same three stack levels, so its node carries the real limits the
    // user gave it.
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAnINF, (uint16_t)bigLbl);
    // A fixture must prove it reached the state it claims to test.
    // Assert the run succeeded, then assert what it produced.
    if(lastErrorCode != ERROR_NONE) {
      ppTestFailInt("B9 the infinite sum did not run", 0, (int32_t)lastErrorCode);
      lastErrorCode = 0;
    }
    else {
      sprintf(expect, "{#,#}%s", indexOfItems[ITM_SIGMAnINF].itemCatalogName);
      ppcTestExpectSig("B9 infinite sum captured", expect);
    }
    #endif // OPTION_INFSUMS

    // B6: the dispatch that actually integrates: PGMINT preselects the
    // label program, and the INTEGRAL_YX param is the integration
    // VARIABLE (the covIntegratePgm currency). ACC=0 -> default
    // tolerance, as the upstream fixture does.
    ppcTestReset();
    currentSolverStatus = 0;
    reallocateRegister(RESERVED_VARIABLE_ACC, dtReal34, 0, amNone);
    int32ToReal34(0, REGISTER_REAL34_DATA(RESERVED_VARIABLE_ACC));
    ppcTestOpParam(ITM_PGMINT, (uint16_t)bigLbl);
    ppcTestType("0");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_INTEGRAL_YX, findOrAllocateNamedVariable("X"));
    sprintf(expect, "{#,#}%s", nINT);
    ppcTestExpectSig("B6 integral captured", expect);
    if(getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("B6 X not real34");
    }
    else {
      // integral of x^2 over [0,1] = 1/3: pin |3X - 1| < 1e-6
      real34_t three, one, diff, tol;
      int32ToReal34(3, &three);
      int32ToReal34(1, &one);
      real34Multiply(REGISTER_REAL34_DATA(REGISTER_X), &three, &diff);
      real34Subtract(&diff, &one, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail("B6 X != 1/3");
      }
    }

    // B6b: the label-param form is setup only. No result
    // exists, so no node claims one. It still consumed X and Y as
    // limits, so the shadow invalidates.
    ppcTestReset();
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("7");
    ppcTestOpParam(ITM_INTEGRAL_YX, (uint16_t)bigLbl);
    ppcTestExpectSig("B6b setup form does not lie", "-");

    // B7: a non-unit step is visible in the under-limit, or the
    // display lies: 1 ENTER 9 ENTER 2 -> n=1,(delta)2 under, 9 over
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("9");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestOp(ITM_CLX);
    {
      uint16_t elen, eseq;
      const uint8_t *e = ppcHistoryEntry((uint8_t)(ppcHistoryCount() - 1), &elen, &eseq);
      uint8_t root;
      ppReset();
      if(e == NULL || !ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_TINY, false, &root)) {
        ppTestFail("B7 step entry decode");
      }
      else {
        // the step travels as a real34 (upstream's fnToReal currency), so
        // it renders with the real marker: (delta)2.
        sprintf(expect, "B(P(n)|[n= 1 ," "\x83\x94" "2.]|9)");
        ppfTestExpect("B7 step visible", root, expect);
      }
    }

    /* B10: the captured big operator used as an operand, built
     * through the capture engine's own precedence threading. A big
     * operator's body is drawn to
     * the right of the stroke, so a factor beside it binds into the
     * body unless it brackets. Every PSHOW and PHIST of a programmed
     * sum reaches the same builder. Its own capture, so it disturbs
     * nothing above it. */
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("10");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestType("2");
    ppcTestOp(ITM_MULT);
    {
      uint8_t rootB10;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_TINY, &rootB10)) {
        ppTestFail("B10 the captured product did not build");
      }
      else {
        char wantB10[96];
        sprintf(wantB10, "[P(B(P(n)|[n= 1]|10)) %s 2]", STD_DOT);
        ppfTestExpect("B10 a captured sum brackets as an operand", rootB10, wantB10);
      }
    }

    currentSolverStatus   = savedSolverStatus;
    currentSolverProgram  = savedSolverProgram;
    currentSolverVariable = savedSolverVariable;
    currentMvarLabel      = savedMvarLabel;
  }

  // T24: R/S resumes a stopped program that then rewrites the stack
  // with every step out of scope, so nothing tells the shadow. XEQ is
  // the same operation by the other key and is US_ENABLED, so the
  // default rule covers it. R/S is US_UNCHANGED, so the default rule
  // alone ignores it. Pins the classification: after R/S nothing
  // still claims to describe a register.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  {
    // R/S drives the program runner, so it moves machine state a test
    // driver owns: put every piece of it back (the aimBuffer rule,
    // applied to the runner).
    uint16_t hadRunStop = programRunStop;
    uint16_t hadPgm = currentProgramNumber;
    ppcTestOp(ITM_RS);
    ppcTestExpectSig("T24 R/S left the shadow describing stale registers", "-");
    programRunStop = hadRunStop;
    currentProgramNumber = hadPgm;
    lastErrorCode = 0;
    ppcTestReset();
  }

  /* T24b: SST. fnSst only sets PGM_SINGLE_STEP. The key handler then
   * runs one program step with PGM_RUNNING, so every nested hook fails
   * ppcScopeOk and returns without mirroring or invalidating. The
   * step's stack motion is recorded nowhere. */
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  {
    uint16_t hadRunStop = programRunStop;
    uint16_t hadPgm = currentProgramNumber;
    ppcTestOp(ITM_SST);
    ppcTestExpectSig("T24b SST left the shadow describing stale registers", "-");
    programRunStop = hadRunStop;
    currentProgramNumber = hadPgm;
    lastErrorCode = 0;
    ppcTestReset();
  }

  ppcTestReset();
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestFormula =================================================
 * Layout signatures: RUN -> its text with spaces stripped. HBOX ->
 * [children space-joined]. FRAC -> F(a|b). SUP -> S(a|b). RAD -> R(a).
 * PAREN -> P(a). Expected strings build from catalog names at runtime. */

/* One check over a whole tree, so one function serves every surface.
 * A PP_SUP whose base already carries an exponent draws both
 * exponents at the same height and reads as one number. Every
 * producer of a PP_SUP must bracket such a base. A base carries an
 * exponent when it is a PP_SUP or a run whose text ends in
 * superscript glyphs. */
static bool_t ppfTestRunEndsSup(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL || nd->kind != PP_RUN) {
    return false;
  }
  const char *t = ppTextAt(nd->textOff);
  uint16_t last = 0;
  for(uint16_t i = 0; t != NULL && t[i] != 0; ) {
    uint8_t c = (uint8_t)t[i];
    uint16_t code;
    if(c < 0x80) {
      code = c;
      i++;
    }
    else if(t[i + 1] == 0) {
      break;
    }
    else {
      code = (uint16_t)(((uint16_t)c << 8) | (uint8_t)t[i + 1]);
      i = (uint16_t)(i + 2);
    }
    if(code != ' ' && !(code >= 0xa000 && code <= 0xa00f)) {
      last = code;   // the formatter pads with a trailing space glyph
    }
  }
  return last >= 0xa160 && last <= 0xa16b;
}

static const char *ppfTestFirstRunText(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL) {
    return NULL;
  }
  if(nd->kind == PP_RUN) {
    return ppTextAt(nd->textOff);
  }
  for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppNodeAt(c)->nextSibling) {
    const char *t = ppfTestFirstRunText(c);
    if(t != NULL) {
      return t;
    }
  }
  return NULL;
}

static bool_t ppfTestPowersScoped(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL) {
    return true;
  }
  if(nd->kind == PP_SUP) {
    const ppNode_t *b = ppNodeAt(nd->firstChild);
    if(b != NULL && (b->kind == PP_SUP || ppfTestRunEndsSup(nd->firstChild))) {
      return false;
    }
  }
  for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppNodeAt(c)->nextSibling) {
    if(!ppfTestPowersScoped(c)) {
      return false;
    }
  }
  return true;
}

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
    case PP_INT:
      strcat(out, "I(");
      ppfTestSigNode(nd->firstChild, out, cap);
      strcat(out, ")");
      break;
    case PP_BIGOP: {
      uint8_t body  = nd->firstChild;
      uint8_t under = ppNodeAt(body)->nextSibling;
      uint8_t over  = ppNodeAt(under)->nextSibling;
      strcat(out, "B(");
      ppfTestSigNode(body, out, cap);
      strcat(out, "|");
      ppfTestSigNode(under, out, cap);
      strcat(out, "|");
      ppfTestSigNode(over, out, cap);
      strcat(out, ")");
      break;
    }
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

/* File the live formula with CLSTK, decode the filed entry, and
 * require the same layout signature on both surfaces. Call with the
 * formula still current, before any reset. */
static void ppfTestFiledMatchesLive(const char *what) {
  uint8_t live = PP_NONE, filed = PP_NONE;
  char liveSig[192], filedSig[192], msg[256];
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
    snprintf(msg, sizeof(msg), "%s: the live formula does not build, so the row tests nothing", what);
    ppTestFail(msg);
    return;
  }
  liveSig[0] = 0;
  ppfTestSigNode(live, liveSig, sizeof(liveSig));
  ppcTestOp(ITM_CLSTK);
  ppReset();
  if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                    PP_FONT_STANDARD, false, &filed)) {
    snprintf(msg, sizeof(msg), "%s: the filed entry does not decode", what);
    ppTestFail(msg);
    return;
  }
  filedSig[0] = 0;
  ppfTestSigNode(filed, filedSig, sizeof(filedSig));
  if(strcmp(liveSig, filedSig) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (live '%s', filed '%s')\n", what, liveSig, filedSig);
  }
}

void prettyTestFormula(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  char expect[192];
  uint8_t root;
  const char *nADD  = indexOfItems[ITM_ADD].itemCatalogName;

  // FV1: precedence parens, (2+3)×4
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
    // multiplication typesets as the raised dot in the layout
    sprintf(expect, "[P([2 %s 3]) " STD_DOT " 4]", nADD);
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

  // FV4: sqrt scopes without parens, square wraps in SUP
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

  // FV5: PHIST pager paints frames and arms the protocol. PCLR empties
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
  if(calcMode != CM_PRETTY_BROWSER) ppTestFail("FV5 browser mode not entered");
  prettyBrowserLeave();
  if(calcMode == CM_PRETTY_BROWSER) ppTestFail("FV5 leave did not restore mode");
  fnPrettyHistClear(NOPARAM);
  if(ppcHistoryCount() != 0) ppTestFailInt("FV5 PCLR", 0, ppcHistoryCount());
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV6: a formula too tall for the pager's standard rung must still
  // show. The tiny rung re-fonts the whole tree. Build the 3-level
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
  prettyBrowserLeave();
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV12: selection clamps at the last row, and ENTER recalls the
  // selected entry's result into X, restoring the mode and wiping the
  // shadow. The recall bypasses item dispatch.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  fnPrettyHist(NOPARAM);
  if(calcMode != CM_PRETTY_BROWSER) ppTestFail("FV12 browser not entered");
  prettyBrowserDown();
  prettyBrowserDown();   // over-navigation must clamp at the last row
  prettyBrowserDown();
  prettyBrowserEnter();
  if(calcMode == CM_PRETTY_BROWSER) ppTestFail("FV12 recall did not leave the browser");
  if(!ppTestIsLonI(REGISTER_X, 5)) ppTestFail("FV12 recalled result not in X");
  if(ppcCurrentFormulaRoot() != PPC_NIL) ppTestFail("FV12 shadow not invalidated after recall");

  // FV7: sqrt over a fraction, the synthesized tall sign. Measure must
  // succeed, and the sign strokes must leave ink left of the vinculum.
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
        // Columns 10..17 hold only the stroke sign. The vinculum
        // starts at column 19 (child relX-1).
        if(!ppTestRectAnyLit((uint32_t)(100 - n7->ascent), (uint32_t)(100 + n7->descent - 1), 10, 17)) {
          ppTestFail("FV7 synthesized sign missing");
        }
        lcd_fill_rect(0, 60, 120, 80, LCD_SET_VALUE);
      }
    }
  }

  // FV8: xth-root carries its index at the crook
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

  // FV9: log with a subscript base. The script is lowered: a SUB
  // node's descent grows, unlike a SUP node's.
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
        // Assert the script node itself is lowered: relBase must be
        // positive.
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

  // FV10: absolute-value bars
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

  // FV11: the T-line live formula is off by default (identity with
  // forced-off), shows the open formula when toggled on, and never
  // hijacks the X line
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    clearSystemFlag(FLAG_FRACT);
    clearSystemFlag(FLAG_IRFRAC);
    prettySetEnabled(true);

    // T band: baseY 24 -> rows 20..55
    #define PPT_T_TOP 20
    #define PPT_T_ROWS 36
    // default state (fresh driver, toggle untouched): must equal forced-off
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    ppTestCaptureBand(0, PPT_T_TOP, PPT_T_ROWS);
    prettySetTline(false);
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    ppTestCaptureBand(1, PPT_T_TOP, PPT_T_ROWS);
    if(!ppTestSnapsEqual()) {
      ppTestFail("FV11 T-line not OFF by default");
    }
    // toggled ON: the T line must DIFFER from the value render,
    // because the formula "2+3" paints there
    prettySetTline(true);
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    // "2+3" right-aligned: some ink in the T band
    if(!ppTestRectAnyLit(PPT_T_TOP + 1, PPT_T_TOP + PPT_T_ROWS - 1, 300, SCREEN_WIDTH - 1)) {
      ppTestFail("FV11 T-line formula missing when enabled");
    }
    // and the X line stays a VALUE with the toggle on: X band identical
    // on/off (the branch must be T-only)
    lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_X);
    ppTestCapture(0);
    prettySetTline(false);
    lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_X);
    ppTestCapture(1);
    if(!ppTestSnapsEqual()) {
      ppTestFail("FV11 X line affected by the T-line toggle");
    }
    prettySetTline(false);
  }

  // FV13: the master toggle is the real system flag. The toggle item
  // flips it, and a reset restores the default-ON.
  {
    bool_t before = getSystemFlag(FLAG_PRETTYP);
    fnPrettyToggle(NOPARAM);
    if(getSystemFlag(FLAG_PRETTYP) == before) {
      ppTestFail("FV13 toggle does not flip FLAG_PRETTYP");
    }
    fnPrettyToggle(NOPARAM);
    prettySetEnabled(false);
    prettyReset();
    if(!getSystemFlag(FLAG_PRETTYP)) {
      ppTestFail("FV13 reset does not restore default-ON");
    }
    if(!prettyEnabled()) {
      ppTestFail("FV13 prettyEnabled does not read the flag");
    }
  }

  // FV14: the T line is a real flag too, and its default is reached
  // by the opposite route to the master toggle's. A reset wipes the
  // flags, and OFF is already the T line's default, so it must not be
  // re-set afterward the way FLAG_PRETTYP is.
  {
    prettySetTline(false);
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 prettySetTline(false) leaves the flag set");
    }
    fnPrettyTlineToggle(NOPARAM);
    if(!getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 toggle does not set FLAG_PTLINE");
    }
    fnPrettyTlineToggle(NOPARAM);
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 toggle does not clear FLAG_PTLINE");
    }
    setSystemFlag(FLAG_PTLINE);
    prettyReset();
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 reset does not leave the T line OFF");
    }
  }

  // FV17: the two toggles in our own menu show their state. The
  // generic checkbox path only fires for fnGetSystemFlag items inside
  // one particular menu, so ours is a package-local branch. This pins
  // it by rendering the softkey row twice with the flag opposite and
  // comparing ink. A filled box carries more ink than an outline. Per
  // the harness rule the assert is on ordering.
  {
    int16_t hadScrUpd = screenUpdatingMode;
    bool_t hadP = getSystemFlag(FLAG_PRETTYP);
    uint16_t hadMode = calcMode;
    screenUpdatingMode = SCRUPD_AUTO;
    temporaryInformation = TI_NO_INFO;
    calcMode = CM_NORMAL;
    showSoftmenu(-MNU_PP);
    softmenuStack[0].firstItem = 0;

    // the PPON softkey is cell 4 of 6: KEY_X[4]..KEY_X[5]
    uint32_t x0 = 268, x1 = 332, y0 = 218, y1 = 239;
    uint32_t inkOn = 0, inkOff = 0;
    setSystemFlag(FLAG_PRETTYP);
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    showSoftmenuCurrentPart();
    for(uint32_t yy = y0; yy <= y1; yy++) {
      for(uint32_t xx = x0; xx <= x1; xx++) {
        if(lcd_buffer_pixel_on(xx, yy)) inkOn++;
      }
    }
    clearSystemFlag(FLAG_PRETTYP);
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    showSoftmenuCurrentPart();
    for(uint32_t yy = y0; yy <= y1; yy++) {
      for(uint32_t xx = x0; xx <= x1; xx++) {
        if(lcd_buffer_pixel_on(xx, yy)) inkOff++;
      }
    }
    if(inkOn == 0 || inkOff == 0) {
      ppTestFail("FV17 the PPON softkey did not render at all");
    }
    else if(inkOn < inkOff + 8) {
      // Measured margin is 33 px (291 filled vs 258 outline). 8 is a
      // floor that still catches "the indicator stopped moving" without
      // pinning a literal count the font owns.
      ppTestFailInt("FV17 the state indicator does not track the flag",
                    (int32_t)inkOff + 8, (int32_t)inkOn);
    }
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    if(hadP) setSystemFlag(FLAG_PRETTYP); else clearSystemFlag(FLAG_PRETTYP);
    screenUpdatingMode = hadScrUpd;
    calcMode = hadMode;
  }

  // FV16: the cold-start path initializes our data without touching
  // the user's flags.
  {
    clearSystemFlag(FLAG_PRETTYP);   // a user who turned it OFF
    setSystemFlag(FLAG_PTLINE);      // and turned the T line ON
    ppcTestDeinit();                 // ... then cold-starts
    ppcTestOp(ITM_ENTER);            // first dispatch: lazy init runs
    if(getSystemFlag(FLAG_PRETTYP)) {
      ppTestFail("FV16 cold start overwrote the user's PPRTY setting");
    }
    if(!getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV16 cold start overwrote the user's PTLINE setting");
    }
    prettyReset();                   // back to defaults for later tests
  }

  // FV18: the builder must never report success with a child
  // silently missing. ppfParen allocates, and PP_HBOX is the one
  // variadic container. ppMeasure checks arity for
  // FRAC/SUP/SUB/RAD/BARS/PAREN/BIGOP but cannot for HBOX, so an
  // unchecked append there measures and paints as a finished formula
  // with an operand absent. LOGXY is the shape: "log2" with no
  // argument, reported true.
  //
  // The fixture starves the pool by one node, measured at runtime,
  // and asserts both halves: with the measured node count the build
  // succeeds, and one node short it must fail.
  {
    ppcTestReset();
    ppcTestType("8");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_LOGXY);

    uint8_t root = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
      ppTestFail("FV18 fixture never built at all — LOGXY was not captured");
    }
    else {
      uint8_t used = 0;
      while(ppNodeAt(used) != NULL) {
        used++;
      }
      if(used < 2 || used > PP_POOL_NODES) {
        ppTestFailInt("FV18 implausible node count", PP_POOL_NODES, used);
      }
      else {
        // one node short: the last allocation in ppfBuildOp2 is ppfParen
        ppReset();
        for(uint8_t i = 0; i < (uint8_t)(PP_POOL_NODES - (used - 1)); i++) {
          if(ppNewBox(PP_HBOX, PP_FONT_STANDARD) == PP_NONE) {
            ppTestFail("FV18 could not starve the pool");
            break;
          }
        }
        uint8_t starved = PP_NONE;
        if(ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &starved)) {
          ppTestFail("FV18 the builder reported success one node short — a formula is missing an operand");
        }
      }
    }
    ppReset();
  }

  // FV19: the browser's softkey containment is a range, and this pins
  // our calcMode inside it. Upstream's three softkey gates
  // (btnFnPressed, btnFnReleased, executeFunction) enumerate its own
  // browsers by name. A package browser is covered only by the shared
  // `calcMode < 19 /* package browsers 19-23, claims registry */`
  // clause both sibling packages carry byte-identically. Renumber
  // CM_PRETTY_BROWSER below 19 and softkeys silently run underneath
  // the browser again. Our own PCLR then wipes the browsed history.
  // Nothing else goes red.
  if(CM_PRETTY_BROWSER < 19 || CM_PRETTY_BROWSER > 23) {
    ppTestFailInt("FV19 calcMode is outside the package-browser range the softkey gates protect",
                  20, (int32_t)CM_PRETTY_BROWSER);
  }

  // FV15: the softmenu claims are actually wired. The package's own
  // menu resolves with its six entries, and both parent slots hold
  // what the claims registry says they hold.
  {
    const int16_t *ppItems = NULL;
    int16_t ppCount = 0;
    for(int16_t m = 0; softmenu[m].menuItem != 0; m++) {
      if(softmenu[m].menuItem == -MNU_PP) {
        ppItems = softmenu[m].softkeyItem;
        ppCount = softmenu[m].numItems;
        break;
      }
    }
    if(ppItems == NULL) {
      ppTestFail("FV15 MNU_PP is not registered in the softmenu table");
    }
    else {
      // 12, not 6: VISUAL sits on the f-shifted row (a softmenu's slots
      // 6-11 ARE the f row), and upstream menus are padded to a multiple
      // of six
      static const int16_t want[12] = { ITM_PSHOW,  ITM_PHIST, ITM_PCLR,
                                        ITM_EQSHW,  ITM_PPON,  ITM_PTLIN,
                                        ITM_VISUAL, ITM_NULL,  ITM_NULL,
                                        ITM_NULL,   ITM_NULL,  ITM_NULL };
      if(ppCount != 12) {
        ppTestFailInt("FV15 MNU_PP size", 12, ppCount);
      }
      else {
        for(int i = 0; i < 12; i++) {
          if(ppItems[i] != want[i]) {
            ppTestFailInt("FV15 MNU_PP slot", want[i], ppItems[i]);
          }
        }
      }
    }
    // the two parent slots
    bool_t inDisp = false, inEqn = false;
    for(int16_t m = 0; softmenu[m].menuItem != 0; m++) {
      const int16_t *it = softmenu[m].softkeyItem;
      if(it == NULL) {
        continue;
      }
      for(int16_t k = 0; k < softmenu[m].numItems; k++) {
        if(softmenu[m].menuItem == -MNU_DISP && it[k] == -MNU_PP)  inDisp = true;
        if(softmenu[m].menuItem == -MNU_EQN  && it[k] == ITM_EQSHW) inEqn = true;
      }
    }
    if(!inDisp) {
      ppTestFail("FV15 MNU_PP is not in the DISP menu");
    }
    if(!inEqn) {
      ppTestFail("FV15 EQSHW is not in the EQN menu");
    }
  }

  // FV20: both full-screen surfaces hold their pixels with the
  // fnPixel protocol. Upstream's EXIT arm (keyboard.c:2533) dismisses
  // a held screen only when
  //   temporaryInformation != TI_NO_INFO || showScreenDismissed
  // showScreenDismissed is latched from SHOWMODE at btnPressed
  // (keyboard.c:1808).
  //
  // The EXIT arm lives in keyboard.c's static executeFunction,
  // reachable only from GTK button events the testSuite binary has no
  // path to raise. This pin asserts the state the surface must leave
  // behind for that arm to fire.
  {
    ppcTestReset();
    calcMode = CM_NORMAL;
    bool_t hadFract = getSystemFlag(FLAG_FRACT);
    setSystemFlag(FLAG_FRACT);
    prettySetEnabled(true);
    ppTestSetRealX("0.75");     // a LonI never pretties: it falls
                                // back to SHOW and proves nothing

    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    fnPrettyShow(NOPARAM);
    if(!screenHoldsDrawnPixels) {
      ppTestFail("FV20 PSHOW did not reach the held-pixel surface");
    }
    else if(temporaryInformation == TI_NO_INFO) {
      ppTestFail("FV20 PSHOW leaves no temporaryInformation — EXIT cannot dismiss it");
    }
    else if(!SHOWMODE) {
      ppTestFail("FV20 PSHOW screen is not a SHOWMODE screen — no showScreenDismissed latch");
    }

    // setEquation dereferences allFormulae, NULL until a slot
    // exists. This is the covDerivEq idiom.
    uint16_t hadMode = calcMode;
    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    setEquation(currentFormula, "1/(X+2)");
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    fnPrettyEqShow(NOPARAM);
    if(!screenHoldsDrawnPixels) {
      ppTestFail("FV20 EQSHW did not reach the held-pixel surface");
    }
    else if(temporaryInformation == TI_NO_INFO) {
      ppTestFail("FV20 EQSHW leaves no temporaryInformation — EXIT cannot dismiss it");
    }
    else if(!SHOWMODE) {
      ppTestFail("FV20 EQSHW screen is not a SHOWMODE screen — no showScreenDismissed latch");
    }

    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode = SCRUPD_AUTO;
    calcMode = hadMode;
    if(!hadFract) {
      clearSystemFlag(FLAG_FRACT);
    }
    lastErrorCode = ERROR_NONE;
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
  if(!ppqParse("1/X+2", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
    ppTestFail("EQ1 parse");
  }
  else {
    ppfTestExpect("EQ1 precedence", root, "[F(1|x) + 2]");
  }

  // EQ2: parens unwrap under the bar
  ppReset();
  if(!ppqParse("(A+B)/C", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
    ppTestFail("EQ2 parse");
  }
  else {
    ppfTestExpect("EQ2 unwrap", root, "F([A + B]|C)");
  }

  // EQ3: vinculum over a parenthesized radicand
  ppReset();
  if(!ppqParse("\xa2\x1a" "(X+1)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
    ppTestFail("EQ3 parse");
  }
  else {
    ppfTestExpect("EQ3 radical", root, "R([x + 1])");
  }

  // EQ4: declines: no 2D gain, dangling operator, ellipsis, unknown glyph
  ppReset();
  if(ppqParse("A+B", PP_FONT_STANDARD, PP_FONT_TINY, &root))            ppTestFail("EQ4 no-frac accepted");
  ppReset();
  if(ppqParse("1/X+", PP_FONT_STANDARD, PP_FONT_TINY, &root))           ppTestFail("EQ4 dangling accepted");
  ppReset();
  if(ppqParse("1/X" "\xa0\x1b", PP_FONT_STANDARD, PP_FONT_TINY, &root)) ppTestFail("EQ4 ellipsis accepted");
  ppReset();
  if(ppqParse("\x83\xc0" "/2", PP_FONT_STANDARD, PP_FONT_TINY, &root))  ppTestFail("EQ4 unknown glyph accepted");

  /* EQ36 (PP18RR7-3, hardened per PP18RR8-8): a numeral whose run does
   * not fit the text pool must fail the parse, not vanish from the
   * drawing. The fixture arithmetic derives from PP_TEXT_BYTES and a
   * fit-exactly control must PARSE, so a drift in the pool size or the
   * preamble cost breaks this block loudly instead of silencing the
   * overflow rows. Costs: the "1+" preamble is ten runs at two pool
   * bytes each (20), the numeral run is N+1, the denominator run is 2. */
  {
    static char big[PP_TEXT_BYTES + 24];
    const int fit  = PP_TEXT_BYTES - 23;   // 20 + (fit+1) + 2 == PP_TEXT_BYTES
    const int over = PP_TEXT_BYTES - 20;   // the numeral run alone overflows
    int p = 0;
    for(int i = 0; i < 5; i++) { big[p++] = '1'; big[p++] = '+'; }
    for(int i = 0; i < fit; i++) { big[p++] = '7'; }
    big[p++] = '/'; big[p++] = '2'; big[p] = 0;
    ppReset();
    if(!ppqParse(big, PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
      ppTestFail("EQ36 the fit-exactly control does not parse: the pool or preamble arithmetic drifted");
    }
    p = 10;
    for(int i = 0; i < over; i++) { big[p++] = '7'; }
    big[p++] = '/'; big[p++] = '2'; big[p] = 0;
    ppReset();
    if(ppqParse(big, PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
      ppTestFail("EQ36 a numeral one byte past the pool still parsed");
    }
    p = 10 + over;
    big[p++] = 'a'; big[p++] = 'b'; big[p++] = 'c';
    big[p++] = '/'; big[p++] = '2'; big[p] = 0;
    ppReset();
    if(ppqParse(big, PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
      ppTestFail("EQ36 a numeral vanished into the text pool and the parse still succeeded");
    }
  }

  /* EQ4b: radix parity. The evaluator treats ',' and '.' as the same
   * radix mark, so an acceptor whose verdict changes between the two
   * for identical mathematics is a defect by definition. Both forms
   * must parse and draw the same picture. The dot form is the
   * control, so a regression that breaks both still reddens this. */
  {
    char sigDot[192], sigComma[192];
    uint8_t dotRoot = PP_NONE, commaRoot = PP_NONE;
    sigDot[0] = sigComma[0] = 0;

    ppReset();
    if(!ppqParse("1.5/X", PP_FONT_STANDARD, PP_FONT_TINY, &dotRoot)) {
      ppTestFail("EQ4b control: the dot radix form does not parse at all");
    }
    else {
      ppfTestSigNode(dotRoot, sigDot, sizeof(sigDot));
    }
    ppReset();
    if(!ppqParse("1,5/X", PP_FONT_STANDARD, PP_FONT_TINY, &commaRoot)) {
      ppTestFail("EQ4b the comma radix form declined; the evaluator reads 1,5 as exactly 1.5");
    }
    else {
      ppfTestSigNode(commaRoot, sigComma, sizeof(sigComma));
    }

    /* The drawing keeps the owner's own radix spelling. Normalize
     * that one character, and the two pictures must be identical. */
    for(char *q = sigComma; *q != 0; q++) {
      if(*q == ',') {
        *q = '.';
      }
    }
    if(sigDot[0] == 0 || strcmp(sigDot, sigComma) != 0) {
      ppTestFailures++;
      printf("prettyPrint test FAIL: EQ4b radix parity (dot '%s' vs comma-normalised '%s')\n",
             sigDot, sigComma);
    }
  }

  /* EQ4c: exponent-sign parity. The equation builder emits both signs
   * (_showExponent writes STD_SUP_PLUS for an explicit '+'), and
   * PPQ_IS_SUP admits both STD_SUP_PLUS and STD_SUP_MINUS. Same class
   * as EQ4b's radix: the acceptor's alphabet must match the
   * producer's emit-set. */
  {
    char sigMinus[192], sigPlus[192];
    uint8_t mRoot = PP_NONE, pRoot = PP_NONE;
    sigMinus[0] = sigPlus[0] = 0;

    ppReset();
    if(!ppqParse("1" "\x80\xb7" "\xa4\x7d" "\xa1\x6b" "\xa1\x65" "/X",
                 PP_FONT_STANDARD, PP_FONT_TINY, &mRoot)) {
      ppTestFail("EQ4c control: the sup-MINUS exponent form does not parse");
    }
    else {
      ppfTestSigNode(mRoot, sigMinus, sizeof(sigMinus));
    }
    ppReset();
    if(!ppqParse("1" "\x80\xb7" "\xa4\x7d" "\xa1\x6a" "\xa1\x65" "/X",
                 PP_FONT_STANDARD, PP_FONT_TINY, &pRoot)) {
      ppTestFail("EQ4c the sup-PLUS exponent form declined; the builder emits it too");
    }
    else {
      ppfTestSigNode(pRoot, sigPlus, sizeof(sigPlus));
    }
    if(sigMinus[0] == 0 || sigPlus[0] == 0 || strlen(sigPlus) != strlen(sigMinus)) {
      ppTestFail("EQ4c the two signed-exponent forms do not draw the same shape");
    }
  }

  /* EQ4d: the EQN grammar is a producer of a PP_SUP, so it carries the
   * same rule. A base that already ends in exponent glyphs must be
   * bracketed, or 1x10^5 raised to 2 draws its two exponents at one
   * height. The number scanner swallows the SUB-10 tail into the run,
   * so the kind test alone cannot see it. ppfPowBase reads the run. */
  {
    ppReset();
    uint8_t eRoot = PP_NONE;
    if(!ppqParse("1" "\x80\xb7" "\xa4\x7d" "\xa1\x65" "^2/X",
                 PP_FONT_STANDARD, PP_FONT_TINY, &eRoot)) {
      ppTestFail("EQ4d the exponent-tail power form does not parse");
    }
    else if(!ppfTestPowersScoped(eRoot)) {
      ppTestFail("EQ4d a power over a scientific-form number draws two exponents as one");
    }
  }

  /* EQ9b: the linear fallback must not lie by truncation. An equation
   * with no fraction, radical, or script is refused by the 2D grammar
   * by construction, so it lands on the fallback. If it is wider than
   * the screen, showString's NO_LF arm paints past the edge, and
   * bitblt24 drops the tail silently. The sibling pager refuses that
   * shape outright, but this surface must show something, so it
   * paints a marker. Asserted on the fit. */
  {
    static const char wide[] = "AREA=LENGTH+WIDTH+CORRECTION+OFFSET+FACTOR+MARGIN";
    char cut[256];
    if(stringWidth(wide, &standardFont, false, true) <= SCREEN_WIDTH - 4) {
      ppTestFail("EQ9b setup: the fixture is not wider than the screen — pin proves nothing");
    }
    ppqFitWithEllipsis(wide, cut, sizeof(cut));
    if(stringWidth(cut, &standardFont, false, true) > SCREEN_WIDTH - 4) {
      ppTestFail("EQ9b the fitted line still overruns the screen");
    }
    if(strlen(cut) >= strlen(wide)) {
      ppTestFail("EQ9b the fitted line was not shortened");
    }
    if(strstr(cut, STD_ELLIPSIS) == NULL) {
      ppTestFail("EQ9b the truncation carries no marker — a lie by truncation");
    }
    /* A line that already fits must be returned whole and unmarked. */
    char keep[256];
    ppqFitWithEllipsis("A+B", keep, sizeof(keep));
    if(strcmp(keep, "A+B") != 0) {
      ppTestFail("EQ9b a line that fits was altered");
    }
  }

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

  // EQ6: the nested equation that must decline in the strip parses
  // and fits the full view at standard fonts, and stays full-size (a
  // strip-font mutation shrinks the outer stack below 25 px)
  ppReset();
  {
    uint8_t root6;
    if(!ppqParse("1/(2+3/4)", PP_FONT_STANDARD, PP_FONT_STANDARD, &root6)) {
      ppTestFail("EQ6 full-view parse");
    }
    else if(!ppMeasure(root6, 0)) {
      ppTestFail("EQ6 measure");
    }
    else {
      const ppNode_t *n6 = ppNodeAt(root6);
      if(n6->ascent + n6->descent > 147) ppTestFailInt("EQ6 too tall", 147, n6->ascent + n6->descent);
      if(n6->ascent + n6->descent < 25)  ppTestFailInt("EQ6 shrunk to strip size", 25, n6->ascent + n6->descent);
    }
  }

  // EQ7: EQSHW renders the nested equation between its frames and
  // arms the manual-paint protocol
  {
    int16_t hadScrUpd = screenUpdatingMode;
    bool_t hadHolds = screenHoldsDrawnPixels;
    screenUpdatingMode = SCRUPD_AUTO;
    screenHoldsDrawnPixels = false;
    if(!ppqShowRender("1/(2+3/4)")) {
      ppTestFail("EQ7 full view declined");
    }
    if(!ppTestRowAllLit(20, 0, SCREEN_WIDTH - 1))  ppTestFail("EQ7 frame 20");
    if(!ppTestRowAllLit(168, 0, SCREEN_WIDTH - 1)) ppTestFail("EQ7 frame 168");
    if(!ppTestRectAnyLit(21, 167, 0, SCREEN_WIDTH - 1)) ppTestFail("EQ7 no content");
    {
      // full-size pin: the nested stack spans >= 30 rows at standard
      // fonts (a strip-font mutation shrinks it below that)
      uint32_t topRow = 0, botRow = 0;
      bool_t seen = false;
      for(uint32_t r = 21; r <= 167; r++) {
        if(ppTestRowAnyLit(r, 0, SCREEN_WIDTH - 1)) {
          if(!seen) { topRow = r; seen = true; }
          botRow = r;
        }
      }
      if(!seen || botRow - topRow + 1 < 30) {
        ppTestFailInt("EQ7 render not full-size", 30, seen ? (int32_t)(botRow - topRow + 1) : 0);
      }
    }
    if(!(screenUpdatingMode & SCRUPD_MANUAL_STACK)) ppTestFail("EQ7 protocol not armed");
    if(!screenHoldsDrawnPixels)                     ppTestFail("EQ7 pixels not held");
    screenUpdatingMode = hadScrUpd;
    screenHoldsDrawnPixels = hadHolds;
  }

  // EQ8: interactive integrate mode frames the integrand with the
  // stroke-drawn big ∫. Ink appears left of the plain render's block.
  {
    uint16_t hadStatus = currentSolverStatus;
    int16_t hadScrUpd = screenUpdatingMode;
    bool_t hadHolds = screenHoldsDrawnPixels;
    screenUpdatingMode = SCRUPD_AUTO;
    screenHoldsDrawnPixels = false;
    currentSolverStatus = (uint16_t)((currentSolverStatus & ~SOLVER_STATUS_EQUATION_MODE) | SOLVER_STATUS_EQUATION_INTEGRATE);
    if(!ppqShowRender("1/X")) {
      ppTestFail("EQ8 integrate view declined");
    }
    else {
      bool_t any8 = false;
      for(uint32_t xx = 170; xx < 188 && !any8; xx++) {   // left of the operand's columns
        for(uint32_t yy = 40; yy < 150 && !any8; yy++) {
          any8 = lcd_buffer_pixel_on(xx, yy);
        }
      }
      if(!any8) ppTestFail("EQ8 integral sign missing");
    }
    currentSolverStatus = hadStatus;
    screenUpdatingMode = hadScrUpd;
    screenHoldsDrawnPixels = hadHolds;
  }

  // EQ9: an unparseable equation still shows its linear line in the
  // full view, the always-show-something fallback
  {
    int16_t hadScrUpd = screenUpdatingMode;
    bool_t hadHolds = screenHoldsDrawnPixels;
    screenUpdatingMode = SCRUPD_AUTO;
    screenHoldsDrawnPixels = false;
    if(ppqShowRender("A+B+")) {
      ppTestFail("EQ9 unparseable accepted as pretty");
    }
    if(!ppTestRectAnyLit(21, 167, 0, SCREEN_WIDTH - 1)) {
      ppTestFail("EQ9 fallback text missing");
    }
    screenUpdatingMode = hadScrUpd;
    screenHoldsDrawnPixels = hadHolds;
  }

  /* ==== Solver-surface frames ========================================== */
  {
    uint16_t hadStatus = currentSolverStatus;
    uint16_t hadVar = currentSolverVariable;
    uint8_t eq;

    // EQ10: interactive integrate frames with the REAL limits and d<var>
    currentSolverVariable = findOrAllocateNamedVariable("X");
    currentSolverStatus = (uint16_t)(SOLVER_STATUS_INTERACTIVE | SOLVER_STATUS_EQUATION_INTEGRATE);
    reallocateRegister(RESERVED_VARIABLE_LLIM, dtReal34, 0, amNone);
    reallocateRegister(RESERVED_VARIABLE_ULIM, dtReal34, 0, amNone);
    int32ToReal34(0, REGISTER_REAL34_DATA(RESERVED_VARIABLE_LLIM));
    int32ToReal34(1, REGISTER_REAL34_DATA(RESERVED_VARIABLE_ULIM));
    ppReset();
    if(!ppqParse("1/X", PP_FONT_STANDARD, PP_FONT_TINY, &eq)) {
      ppTestFail("EQ10 parse");
    }
    else {
      ppfTestExpect("EQ10 integral frame", ppqFrameIntegral(eq), "B([F(1|x) dx]|0.|1.)");
    }

    // EQ11: without INTERACTIVE the limits are not the session's, so
    // it falls back to the bare stroke integral.
    currentSolverStatus = SOLVER_STATUS_EQUATION_INTEGRATE;
    ppReset();
    if(!ppqParse("1/X", PP_FONT_STANDARD, PP_FONT_TINY, &eq)) {
      ppTestFail("EQ11 parse");
    }
    else {
      ppfTestExpect("EQ11 bare fallback", ppqFrameIntegral(eq), "I(F(1|x))");
    }

    // EQ12: first derivative frames d/dX (var name decoded live)
    currentSolverStatus = (uint16_t)(SOLVER_STATUS_INTERACTIVE | SOLVER_STATUS_EQUATION_1ST_DERIVATIVE);
    ppReset();
    if(!ppqParse("1/X", PP_FONT_STANDARD, PP_FONT_TINY, &eq)) {
      ppTestFail("EQ12 parse");
    }
    else {
      ppfTestExpect("EQ12 d/dx", ppqFrameDerivative(eq, false), "[F(d|dx) P(F(1|x))]");
    }

    // EQ13: second derivative carries the superscript-2 glyphs
    ppReset();
    if(!ppqParse("1/X", PP_FONT_STANDARD, PP_FONT_TINY, &eq)) {
      ppTestFail("EQ13 parse");
    }
    else {
      char expect13[64];
      sprintf(expect13, "[F(d" "\xa1\x62" "|dx" "\xa1\x62" ") P(F(1|x))]");
      ppfTestExpect("EQ13 d2/dx2", ppqFrameDerivative(eq, true), expect13);
    }

    currentSolverStatus = hadStatus;
    currentSolverVariable = hadVar;
  }

  /* ==== Equation-language big operators ================================ */
  {
    uint16_t hadStatus = currentSolverStatus;
    uint16_t hadVar = currentSolverVariable;
    uint16_t hadProgram = currentSolverProgram;

    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);          // the covDerivEq idiom, leaves EIM state
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;

    calcRegister_t varX = findOrAllocateNamedVariable("X");

    // EQ14: SUM evaluates through the real fnEqCalc path, and the bound
    // variable is put back the way it was found
    reallocateRegister(varX, dtReal34, 0, amNone);
    int32ToReal34(99, REGISTER_REAL34_DATA(varX));
    setEquation(currentFormula, "SUM(X^2;X;1;10)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(385, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ14 SUM != 385");
      }
      int32ToReal34(99, &want);
      if(getRegisterDataType(varX) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(varX), &want)) {
        ppTestFail("EQ14 bound variable not restored");
      }
    }

    // EQ15: PROD seeds with one
    setEquation(currentFormula, "PROD(X;X;1;5)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(120, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ15 PROD != 120");
      }
    }

    // EQ16: DERIV delegates to the upstream engine, exact for a cubic
    // (deriv_cov's own pin), both orders
    setEquation(currentFormula, "DERIV(X^3;X;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(12, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ16 DERIV != 12");
      }
    }
    setEquation(currentFormula, "DERIV(X^3;X;3;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(18, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ16 DERIV order 2 != 18");
      }
    }

    // EQ17: INTEG delegates to the double-exponential integrator
    setEquation(currentFormula, "INTEG(X^2;X;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t three, one, diff, tol;
      int32ToReal34(3, &three);
      int32ToReal34(1, &one);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ17 INTEG errored");
      }
      else {
        real34Multiply(REGISTER_REAL34_DATA(REGISTER_X), &three, &diff);
        real34Subtract(&diff, &one, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-6", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ17 INTEG != 1/3");
        }
      }
    }

    // EQ18: constructs nest. The argument slicer must honor paren depth
    setEquation(currentFormula, "SUM(SUM(Y;Y;1;X);X;1;3)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(10, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ18 nested SUM != 10");
      }
    }

    // EQ19: a wrong argument count raises the equation's own error
    setEquation(currentFormula, "SUM(X;X;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    if(lastErrorCode == ERROR_NONE) {
      ppTestFail("EQ19 malformed SUM accepted");
    }
    lastErrorCode = 0;

    // EQ20/EQ21: the constructs render as their 2D shapes (strict parse)
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("SUM(X" "\xa1\x62" ";X;1;10)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ20 parse");
      }
      else {
        // HBOX sig children are space-joined
        ppfTestExpect("EQ20 sum shape", root, "B([x \xa1\x62]|[x = 1]|10)");
      }
      ppReset();
      if(!ppqParse("DERIV(X;X;2)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ21 parse");
      }
      else {
        ppfTestExpect("EQ21 deriv shape", root, "[F(d|[d x]) U(P(x)|[x = 2])]");
      }
    }

    // EQ22 (capacity): the ultimate nesting, an integral wrapping a
    // second derivative of [a Σ-with-√-fraction over a multiplication,
    // times a ∏ with a nested power fraction]. Pins the raised
    // PP_MAX_DEPTH and the 64-node pool: it must parse, measure, use
    // most of the pool, and fit the EQSHW band at full size.
    //
    // The fixture is the string a user can actually type, and EQ33
    // evaluates this same expression. The root is a function alias
    // that needs parentheses (functionAlias[], beside log10), and the
    // power operator is '^'. Indices are lowercase and distinct from
    // the outer variable. The constructs shadow correctly (same-
    // variable and distinct-variable forms agree to 34 digits), and
    // distinct indices let a reader see that.
    {
      uint8_t root;
      ppReset();
      static const char ultimate[] =
        "INTEG(DERIV("
          "SUM(" "\xa2\x1a" "(n)/(n+1);n;1;10)/(x" "\x80\xd7" "(x+1))"
          "\x80\xd7" "PROD(1+1/(2+m^2);m;1;5)"
        ";x;2;2);x;0;1)";
      ppReset();
      if(!ppqParse(ultimate, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ22 ultimate parse");
      }
      else if(!ppMeasure(root, 0)) {
        ppTestFail("EQ22 ultimate measure");
      }
      else {
        const ppNode_t *n = ppNodeAt(root);
        int16_t h = (int16_t)(n->ascent + n->descent);
        if(h < 80 || h > 147) {
          ppTestFailInt("EQ22 height out of band", 147, h);
        }
        if(n->width > 396) {
          ppTestFailInt("EQ22 too wide", 396, n->width);
        }
        uint8_t used = 0;
        while(used < PP_POOL_NODES && ppNodeAt(used) != NULL) {
          used++;
        }
        if(used < 45) {
          ppTestFailInt("EQ22 not the big tree", 45, used);
        }
      }
    }

    // EQ23-EQ25: the stored-alphabet arms EQSHW reads. The display
    // string truncates long equations for the strip.
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("X^2/(X+1)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ23 stored-form parse");
      }
      else {
        ppfTestExpect("EQ23 caret exponent", root, "F(S(x|2)|[x + 1])");
      }
      ppReset();
      if(!ppqParse("F:1/X", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ24 labeled parse");
      }
      else {
        ppfTestExpect("EQ24 label skipped", root, "F(1|x)");
      }
      ppReset();
      if(!ppqParse("SUM(1+X;X;1;5)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ25 parse");
      }
      else {
        // an additive body scopes in parens or the operator misreads
        ppfTestExpect("EQ25 additive body scoped", root, "B(P([1 + x])|[x = 1]|5)");
      }
    }

    // EQ29: the whole user journey for a construct, through the real
    // key path. Types SUM(X;X;1;3) one softkey at a time into the
    // equation editor (the ';' lives in the ALPHA punctuation menu).
    // The commit runs setEquation plus the MVAR variable-hunting
    // parse, then evaluates. 1+2+3 = 6.
    {
      uint16_t hadMode = calcMode;
      bool_t hadKIC = fnKeyInCatalog;
      fnEqNew(NOPARAM);                      // -> CM_EIM on a fresh slot
      aimBuffer[0] = 0;
      xCursor = 0;
      showSoftmenu(-MNU_ALPHAMISC);          // where ';' lives
      fnKeyInCatalog = true;                 // as a softkey press sets it
      static const int16_t keys[] = {
        ITM_S, ITM_U, ITM_M, ITM_LEFT_PARENTHESIS,
        ITM_X, ITM_SEMICOLON, ITM_X, ITM_SEMICOLON,
        ITM_1, ITM_SEMICOLON, ITM_3, ITM_RIGHT_PARENTHESIS
      };
      for(uint8_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        addItemToBuffer(keys[k]);
      }
      if(strcmp(aimBuffer, "SUM(X;X;1;3)") != 0) {
        ppTestFailures++;
        printf("prettyPrint test FAIL: EQ29 typed text (got '%s')\n", aimBuffer);
      }
      // the commit ENTER performs (keyboard.c CM_EIM case)
      lastErrorCode = 0;
      setEquation(currentFormula, aimBuffer);
      parseEquation(currentFormula, EQUATION_PARSER_MVAR, aimBuffer, tmpString);
      if(lastErrorCode != ERROR_NONE) {
        ppTestFailInt("EQ29 commit rejected the typed equation", 0, (int32_t)lastErrorCode);
        lastErrorCode = 0;
      }
      fnKeyInCatalog = hadKIC;
      calcMode = CM_NORMAL;
      aimBuffer[0] = 0;
      nimNumberPart = NP_EMPTY;
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      {
        real34_t want;
        int32ToReal34(6, &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
            || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ29 typed equation != 6");
        }
      }
      calcMode = hadMode;
      lastErrorCode = 0;
    }

    // EQ30: a construct whose body is complex accumulates complex, on
    // upstream's own terms. Its _programmableSumProd latches over the
    // moment a term has an imaginary part, but only if FL_CPXRES
    // allows. SUM(X*i;X;1;3) = (1+2+3)i = 6i.
    {
      bool_t hadCpx = getSystemFlag(FLAG_CPXRES);
      setSystemFlag(FLAG_CPXRES);
      setEquation(currentFormula, "SUM(X" STD_CROSS "i;X;1;3)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtComplex34) {
        ppTestFailInt("EQ30 complex SUM did not stay complex", dtComplex34,
                      (int32_t)getRegisterDataType(REGISTER_X));
      }
      else {
        real34_t want;
        int32ToReal34(6, &want);
        if(!real34IsZero(REGISTER_REAL34_DATA(REGISTER_X))
            || !real34CompareEqual(REGISTER_IMAG34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ30 complex SUM != 6i");
        }
      }
      lastErrorCode = 0;
      // and with complex results refused, the same body is a domain
      // error
      clearSystemFlag(FLAG_CPXRES);
      setEquation(currentFormula, "SUM(X" STD_CROSS "i;X;1;3)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(lastErrorCode == ERROR_NONE) {
        ppTestFail("EQ30 complex term accepted while CPXRES is clear");
      }
      lastErrorCode = 0;
      if(hadCpx) {
        setSystemFlag(FLAG_CPXRES);
      }
      else {
        clearSystemFlag(FLAG_CPXRES);
      }
    }

    // EQ31: a construct must never destroy the value the loop
    // variable already held. Class test: every type
    // saveRegisterSnapshot covers must survive a construct.
    {
      calcRegister_t vA = findOrAllocateNamedVariable("A");
      // complex value
      reallocateRegister(vA, dtComplex34, 0, amNone);
      int32ToReal34(7, REGISTER_REAL34_DATA(vA));
      int32ToReal34(4, REGISTER_IMAG34_DATA(vA));
      setEquation(currentFormula, "SUM(A;A;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      // Prove the sum ran: 1+2 = 3.
      {
        real34_t want3;
        int32ToReal34(3, &want3);
        if(lastErrorCode != ERROR_NONE) {
          ppTestFailInt("EQ31 the sum refused instead of running", 0, (int32_t)lastErrorCode);
        }
        else if(getRegisterDataType(REGISTER_X) != dtReal34
                 || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want3)) {
          ppTestFail("EQ31 the sum did not produce 3, so the restore is untested");
        }
      }
      lastErrorCode = 0;
      if(getRegisterDataType(vA) != dtComplex34) {
        ppTestFailInt("EQ31 complex loop variable destroyed", dtComplex34,
                      (int32_t)getRegisterDataType(vA));
      }
      else {
        real34_t w7, w4;
        int32ToReal34(7, &w7);
        int32ToReal34(4, &w4);
        if(!real34CompareEqual(REGISTER_REAL34_DATA(vA), &w7)
            || !real34CompareEqual(REGISTER_IMAG34_DATA(vA), &w4)) {
          ppTestFail("EQ31 complex loop variable not restored to 7+4i");
        }
      }
      // long integer: this branch must still work
      reallocateRegister(vA, dtLongInteger, 0, amNone);
      ppTestWriteLonI(vA, 12345);
      setEquation(currentFormula, "SUM(A;A;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      lastErrorCode = 0;
      if(!ppTestIsLonI(vA, 12345)) {
        ppTestFail("EQ31 long-integer loop variable not restored");
      }
      reallocateRegister(vA, dtReal34, 0, amNone);
      int32ToReal34(0, REGISTER_REAL34_DATA(vA));
    }

    // EQ32: the refusal verdict must be about this call. Nothing on
    // the derivative path clears engineNestingWasRefused (solve.c
    // holds the tree's only `= false`). Same shape for a pre-existing
    // PGM_WAITING.
    {
      engineNestingWasRefused = true;          // as an earlier refusal leaves it
      setEquation(currentFormula, "DERIV(X^3;X;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      real34_t want12;
      int32ToReal34(12, &want12);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want12)) {
        ppTestFail("EQ32 a stale refusal flag discarded a correct derivative");
      }
      engineNestingWasRefused = false;
      lastErrorCode = 0;

      // The caller's run state is the caller's. A pre-existing
      // PGM_WAITING means an abort is genuinely in flight (the engine
      // itself raises SOLVER_ABORT there, differentiate.c:402), so the
      // construct failing is correct. The state must not be silently
      // rewritten to PGM_STOPPED on the way out.
      programRunStop = PGM_WAITING;            // simulates a pre-existing WAITING
      setEquation(currentFormula, "DERIV(X^3;X;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(programRunStop != PGM_WAITING) {
        ppTestFailInt("EQ32 the caller's run state was overwritten",
                      PGM_WAITING, (int32_t)programRunStop);
      }
      programRunStop = PGM_STOPPED;
      lastErrorCode = 0;
    }

    // EQ26: the render/eval parity ruling, an integral of a numeric
    // second derivative whose body holds a construct, over limits away
    // from zero. A limit at zero collapses the relative-step stencil
    // at the integrator's endpoint-clustered nodes. Upstream's
    // interactive d²/dx² at 1E-24 fails the same way (documented
    // caveat).
    // g = SUM(X;X;1;3)/(X+2) = 6/(x+2). Integral of g'' over [1,2]
    // = g'(2)-g'(1) = 7/24.
    setEquation(currentFormula, "INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;1;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("0.2916666666666666666666666666666667", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ26 tower eval errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-10", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ26 tower != 7/24");
        }
      }
    }
    lastErrorCode = 0;

    // EQ33: the capacity expression EQ22 renders must also evaluate.
    // Hand-checked factor by factor: the sum is 3.759490707, the
    // product 1.857588228, and the second derivative of 1/(x(x+1)) at
    // x=2 is 0.175925926. The integral of that constant over [0,1]
    // leaves it unchanged.
    setEquation(currentFormula,
      "INTEG(DERIV("
        "SUM(" "\xa2\x1a" "(n)/(n+1);n;1;10)/(x" "\x80\xd7" "(x+1))"
        "\x80\xd7" "PROD(1+1/(2+m^2);m;1;5)"
      ";x;2;2);x;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("1.228593777031159439372254772764558", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ33 the capacity expression did not evaluate");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-30", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ33 capacity expression value moved");
        }
      }
    }
    lastErrorCode = 0;

    // EQ34: the construct names must accept lowercase. Upstream's own
    // convention for a name a user types into an equation is to carry
    // both spellings in the alias table. functionAlias[] holds "sinh"
    // beside "SINH" and "asinh" beside "ASINH" (equation.c:41-44),
    // because CMP_NAME folds superscripts and struck forms but not
    // case. Variables are not affected, so lowercase indices in the
    // capacity expression (EQ33) always work.
    {
      static const char * const eq34[] = {
        "sum(X;X;1;3)", "SUM(X;X;1;3)",
      };
      for(unsigned i = 0; i < sizeof(eq34) / sizeof(eq34[0]); i++) {
        setEquation(currentFormula, eq34[i]);
        lastErrorCode = 0;
        fnEqCalc(NOPARAM);
        real34_t want;
        int32ToReal34(6, &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
            || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ34 a construct spelling did not evaluate to 6");
        }
        lastErrorCode = 0;
      }
      // and the same name mixed-case inside a nest, with a lowercase
      // index, so the whole intercept path is exercised
      setEquation(currentFormula, "integ(deriv(sum(X;X;1;3)/(X+2);X;X;2);X;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      {
        real34_t want, diff, tol;
        stringToReal34("0.2916666666666666666666666666666667", &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
          ppTestFail("EQ34 the lowercase tower did not evaluate");
        }
        else {
          real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
          real34SetPositiveSign(&diff);
          stringToReal34("1e-10", &tol);
          if(!real34CompareLessThan(&diff, &tol)) {
            ppTestFail("EQ34 lowercase tower != 7/24");
          }
        }
      }
      lastErrorCode = 0;
      // the renderer must agree with the evaluator: same text, drawn
      ppReset();
      if(!ppqParse("sum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 lowercase construct does not render");
      }
      // a variable ENDING in a construct name still must not match
      ppReset();
      if(ppqParse("mysum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 a name ending in a construct name matched");
      }
      // MIXED case is upstream's own no: functionAlias[] carries SINH
      // and sinh, never Sinh.
      ppReset();
      if(ppqParse("Sum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 mixed case matched, which upstream's aliases do not");
      }
    }

    /* EQ35: the EQN parser is a producer of the big-operator-as-operand
     * defect. SUM(X;X;1;3)^2 is 36. Drawn without brackets it is the
     * picture of 1^2+2^2+3^2 = 14. This parser has no precedence value
     * to correct, so the node kind decides. */
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("SUM(X;X;1;3)^2", PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ35 a construct under a power did not parse");
      }
      else {
        ppfTestExpect("EQ35 the EQN parser brackets a construct operand",
                      root, "S(P(B(x|[x = 1]|3))|2)");
      }
      ppReset();
      if(!ppqParse("SUM(X;X;1;3)" STD_CROSS "2", PP_FONT_STANDARD,
                   PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ35 a construct times two did not parse");
      }
      else {
        char w[96];
        sprintf(w, "[P(B(x|[x = 1]|3)) %s 2]", STD_DOT);
        ppfTestExpect("EQ35 the EQN parser brackets a product operand", root, w);
      }
    }

    // EQ27: an integral nests inside an integral. The
    // double-exponential path never increments the engine counter, so
    // nothing upstream refuses it. The package's stack guard is the
    // bound. Integral over y of (integral of x over [0,1]) = 0.5.
    setEquation(currentFormula, "INTEG(INTEG(X;X;0;1);X;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("0.5", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ27 nested integral errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-10", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ27 nested integral != 0.5");
        }
      }
    }
    lastErrorCode = 0;

    // EQ28: the upstream second-derivative defect near zero, and its
    // upstream-native remedy. The same integral over [0,1], a range
    // whose endpoint samples land in the failing band, is garbage
    // with the default relative step and exact once the derivative's
    // own step variable is set. Pins the remedy so the DESIGN.md
    // guidance cannot rot.
    {
      calcRegister_t vd = findOrAllocateNamedVariable(STD_delta STD_SUB_d);
      reallocateRegister(vd, dtReal34, 0, amNone);
      stringToReal34("0.001", REGISTER_REAL34_DATA(vd));
      setEquation(currentFormula, "INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;0;1)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      real34_t want, diff, tol;
      stringToReal34("0.8333333333333333333333333333333333", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ28 stepped derivative errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-25", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ28 stepped derivative != 5/6");
        }
      }
      // zero disables it again: the engine ignores a zero step
      reallocateRegister(vd, dtReal34, 0, amNone);
      int32ToReal34(0, REGISTER_REAL34_DATA(vd));
      lastErrorCode = 0;
    }

    currentSolverStatus = hadStatus;
    currentSolverVariable = hadVar;
    currentSolverProgram = hadProgram;
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    temporaryInformation = TI_NO_INFO;
  }

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== VISUAL, the RPN-program walker ====================================
 * Most pins here assert the transpiled string. That string is not the
 * product: the product is a node tree, and the text back end exists
 * only under PC_BUILD as a test seam. The string is asserted because
 * it is readable and because it is derived from the same AST as the
 * drawing. It cannot catch a fault in the layout pass. The node-shape
 * pins V46-V51, V56, V57, V68, V69, V73, and V74 are for that.
 *
 * V18 and V65 close the loop from the other side: they evaluate the
 * walker's own output and require it to agree with what the program
 * actually computes.
 *
 * Fixtures are the appnote-22 chain (docs/appnotes/sources/AN0022),
 * with package-local label names so no other driver's labels
 * collide. V58 loads the real file. */

#define PPV2(itm) (uint8_t)(((itm) >> 8) | 0x80), (uint8_t)((itm) & 0xff)

static void ppvTestExpect(const char *what, const char *label, const char *expected) {
  calcRegister_t id = findNamedLabel(label, GLOBAL_LABELS);
  if(id == INVALID_VARIABLE) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (label '%s' not registered)\n", what, label);
    return;
  }
  char out[256];
  uint8_t reason = 0;
  uint16_t atStep = 0;
  if(!ppvTranspile((uint16_t)(id - FIRST_LABEL), out, sizeof(out), &reason, &atStep)) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (declined D%u at step %u, expected '%s')\n",
           what, (unsigned)reason, (unsigned)atStep, expected);
    return;
  }
  if(strcmp(out, expected) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected '%s', actual '%s')\n", what, expected, out);
  }
}

static void ppvTestDecline(const char *what, const char *label, uint8_t wantReason) {
  calcRegister_t id = findNamedLabel(label, GLOBAL_LABELS);
  if(id == INVALID_VARIABLE) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (label '%s' not registered)\n", what, label);
    return;
  }
  char out[256];
  uint8_t reason = 0;
  uint16_t atStep = 0;
  if(ppvTranspile((uint16_t)(id - FIRST_LABEL), out, sizeof(out), &reason, &atStep)) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected a decline, drew '%s')\n", what, out);
    return;
  }
  if(reason != wantReason) {
    ppTestFailInt(what, wantReason, reason);
  }
}

/* Drive VISUAL the way the unit does: the command, ALPHA, the label
 * typed one letter at a time, ENTER. Every other pin calls
 * ppvTranspile or fnPrettyVisual directly, which skips the whole
 * transient-alpha path the item's TM_LBLONLY parameter exists for. */
static void ppvTestKeyIn(const char *label) {
  runFunction(ITM_VISUAL);              // the key press: enters TAM
  tamProcessInput(ITM_alpha);           // ALPHA
  for(const char *p = label; *p != 0; p++) {
    addItemToBuffer((uint16_t)(ITM_A + (*p - 'A')));
  }
  tamProcessInput(ITM_ENTER);           // resolves the name and dispatches
}

/* Key a program in through PEM, the way it is actually entered on the
 * unit: every step arrives as runFunction() in CM_PEM, and a step
 * with a parameter goes through the same transient-alpha machinery a
 * user types. This matters beyond "more coverage": every other
 * fixture here hand-encodes its bytes (ITM_LITERAL,
 * STRING_LONG_INTEGER, 1, '0'), and a hand-encoded literal is only a
 * guess at what PEM writes. If the two disagree, the walker passes
 * every pin and still declines the first program a user types. */
// The global step number of .END., derived by walking, never
// counted. fnGotoDot does not clamp, and a number past the end walks
// currentStep to NULL and takes the suite down with it.
static uint16_t ppvLastGlobalStep(void) {
  uint8_t *st = beginOfProgramMemory;
  uint16_t n = 1;
  while(st != NULL && !isAtEndOfPrograms(st)) {
    uint8_t *nx = findNextStep(st);
    if(nx == NULL || nx <= st) {
      break;
    }
    st = nx;
    n++;
  }
  return n;
}

static void ppvPemBegin(void) {
  programRunStop = PGM_STOPPED;
  calcMode = CM_PEM;
  catalog = CATALOG_NONE;
  tam.mode = 0;
  tam.function = 0;
  aimBuffer[0] = 0;
  dynamicMenuItem = -1;
  pemCursorIsZerothStep = false;
  lastErrorCode = ERROR_NONE;
  clearSystemFlag(FLAG_ALPHA);
  // Go to the last global step (.END.), so the keyed steps land after
  // every program.
  fnGotoDot(ppvLastGlobalStep());
}

// a step whose parameter is typed: LBL 'NAME', RCL 'a', PGMINT 'P', ...
static void ppvPemNamed(uint16_t item, const char *name) {
  runFunction(item);
  tamProcessInput(ITM_alpha);
  for(const char *p = name; *p != 0; p++) {
    if(*p >= 'A' && *p <= 'Z') {
      addItemToBuffer((uint16_t)(ITM_A + (*p - 'A')));
    }
    else {
      addItemToBuffer((uint16_t)(ITM_a + (*p - 'a')));
    }
  }
  tamProcessInput(ITM_ENTER);
}

static uint32_t ppvSumRows(int16_t top, int16_t bottom) {
  uint32_t sum = 0;
  for(int16_t y = top; y <= bottom; y++) {
    for(int16_t x = 0; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, y)) {
        sum += (uint32_t)(x + 1) * (uint32_t)(y + 1);
      }
    }
  }
  return sum;
}

static uint32_t ppvBandSum(void) {
  uint32_t sum = 0;
  for(int16_t y = 16; y <= 167; y++) {
    for(int16_t x = 0; x < SCREEN_WIDTH; x++) {
      if(lcd_buffer_pixel_on(x, y)) {
        sum += (uint32_t)(x + 1) * (uint32_t)(y + 1);
      }
    }
  }
  return sum;
}

void prettyTestVisual(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;

  const uint16_t hadStatus  = currentSolverStatus;
  const uint16_t hadVar     = currentSolverVariable;
  const uint16_t hadProgram = currentSolverProgram;
  const uint16_t hadMode    = calcMode;

  /* ---- the appnote-22 integral chain -------------------------------- */
  {
    static const uint8_t pgmHT[] = {          // h(t) = t
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','H','T',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 't',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 't',
      PPV2(ITM_END),
    };
    static const uint8_t pgmIT[] = {          // INT(0..x) t dt
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','I','T',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','H','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 't',
      PPV2(ITM_END),
    };
    static const uint8_t pgmIY[] = {          // INT(0..y) IT dx
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','I','Y',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'y',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'y',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    // DBLINT carries the appnote's own title idiom: a string literal
    // stored to a lettered register and dropped again
    static const uint8_t pgmDBL[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','B','L',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      ITM_LITERAL, STRING_LABEL_VARIABLE, 3, '4','/','3',
      ITM_STO, REGISTER_A_IN_KS_CODE,
      ITM_DROP,
      PPV2(ITM_SNAP),
      PPV2(ITM_END),
    };
    // TRPINT brackets its run with an ACC setting: an exponent literal
    // (unspellable, so opaque) stored to a named variable and dropped
    static const uint8_t pgmTRP[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','T','R','P',
      ITM_LITERAL, STRING_REAL34, 4, '1','e','-','8',
      ITM_STO, STRING_LABEL_VARIABLE, 3, 'A','C','C',
      ITM_DROP,
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','Y',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'y',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_STO, STRING_LABEL_VARIABLE, 3, 'A','C','C',
      ITM_DROP,
      PPV2(ITM_END),
    };
    /* One level deeper than TRPINT, and nothing draws it but the
     * full-screen arm. V28 measures the chain at 38/58/78 px standard
     * and 31/51/71 tiny, so a fourth integral is 98 and 91. That is
     * past the 72-row Z/T band at both rungs and inside the 147-row
     * full band at the first. */
    static const uint8_t pgmIZ[] = {          // INT(0..z) IY dy
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','I','Z',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'z',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','Y',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'z',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'y',
      PPV2(ITM_END),
    };
    static const uint8_t pgmQDL[] = {         // INT(0..2) IZ dz
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','Q','D','L',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','Z',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'z',
      PPV2(ITM_END),
    };
    static const uint8_t pgmFX[] = {          // f(x) = x^2 - p*x - 2
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','F','X',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'p',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_ENTER,
      ITM_MULT,
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'p',
      ITM_MULT,
      ITM_SUB,
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_SUB,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmHT,  sizeof(pgmHT));
    ppcTestWriteAndLoadPgm(pgmIT,  sizeof(pgmIT));
    ppcTestWriteAndLoadPgm(pgmIY,  sizeof(pgmIY));
    ppcTestWriteAndLoadPgm(pgmDBL, sizeof(pgmDBL));
    ppcTestWriteAndLoadPgm(pgmTRP, sizeof(pgmTRP));
    ppcTestWriteAndLoadPgm(pgmIZ,  sizeof(pgmIZ));
    ppcTestWriteAndLoadPgm(pgmQDL, sizeof(pgmQDL));
    ppcTestWriteAndLoadPgm(pgmFX,  sizeof(pgmFX));
  }

  /* V-MODE: the mode-looped oracle. The walker reads both flags.
   * These pins draw the same program under each setting and require
   * the pictures to differ in the way the machine differs.
   *
   * VMEN: 1, 2, ENTER, 3, x, +.
   *   classic: ENTER clears the lift latch, so the 3 overwrites the
   *            dup. Stack [1,2,3] -> 1 + 2x3, and XEQ returns 7.
   *   eRPN:    a running program's ENTER dups and leaves the latch
   *            set, so the 3 lifts. [1,2,2,3] -> 2 + 2x3, and XEQ
   *            returns 8. */
  {
    static const uint8_t pgmMEN[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','M','E','N',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_ENTER),
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_MULT),
      PPV2(ITM_ADD),
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmMEN, sizeof(pgmMEN));

    bool_t erpnWas = getSystemFlag(FLAG_ERPN);
    char want[64];

    clearSystemFlag(FLAG_ERPN);
    sprintf(want, "1+2%s3", STD_CROSS);
    ppvTestExpect("V-MODE classic ENTER overwrites", "VMEN", want);

    setSystemFlag(FLAG_ERPN);
    sprintf(want, "2+2%s3", STD_CROSS);
    ppvTestExpect("V-MODE eRPN ENTER lifts", "VMEN", want);

    if(!erpnWas) {
      clearSystemFlag(FLAG_ERPN);
    }
  }

  /* V-XEQ: a callee's trailing ENTER must survive the return.
   * ITM_XEQ's arm walks the whole subroutine and returns, so an
   * epilogue that clears the lift latch after the arm erases what
   * the callee armed, and the caller's next literal pushes where the
   * machine overwrites.
   *   LBL VZA: 1 2 XEQ VZB 5 + +      LBL VZB: 4 ENTER
   * The armed latch makes the 5 overwrite the dup, so the machine
   * holds [1,2,4,5], adds to 9, adds to 11, and the picture is
   * 2+(4+5). */
  {
    static const uint8_t pgmXB[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','Z','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '4',
      PPV2(ITM_ENTER),
      PPV2(ITM_END),
    };
    static const uint8_t pgmXA[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','Z','A',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_XEQ), STRING_LABEL_VARIABLE, 3, 'V','Z','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      PPV2(ITM_ADD), PPV2(ITM_ADD),
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmXB, sizeof(pgmXB));
    ppcTestWriteAndLoadPgm(pgmXA, sizeof(pgmXA));

    /* The latch's meaning is mode-dependent, so the picture is too.
     * Under eRPN the callee's ENTER leaves the latch set, the 5
     * lifts, and 4+(4+5) = 13 is the right answer.
     * Asserting only one mode leaves the pin true of an ambient this
     * file happens to set. */
    bool_t erpnWasX = getSystemFlag(FLAG_ERPN);

    clearSystemFlag(FLAG_ERPN);
    ppvTestExpect("V-XEQ classic: callee ENTER survives the return (=11)", "VZA", "2+(4+5)");

    setSystemFlag(FLAG_ERPN);
    ppvTestExpect("V-XEQ eRPN: the caller's 5 lifts (=13)", "VZA", "4+(4+5)");

    if(erpnWasX) {
      setSystemFlag(FLAG_ERPN);
    }
    else {
      clearSystemFlag(FLAG_ERPN);
    }
  }

  /* V-DECL: a declaration item between ENTER and the lifting read
   * must leave the latch alone. MVAR stands for the group. The walk
   * never holds more than four entries, so the stack size cannot
   * change the picture.
   *   1 2 ENTER MVAR z 5 + +  ->  the 5 overwrites the dup:
   *   1+(2+5).
   * With the item dropped from the list, the epilogue clears the
   * latch, the 5 lifts, and the walker draws 2+(2+5) for a program
   * returning 8. */
  {
    static const uint8_t pgmDC[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','D','C',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_ENTER),
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'z',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      PPV2(ITM_ADD), PPV2(ITM_ADD),
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmDC, sizeof(pgmDC));

    bool_t erpnWasDC = getSystemFlag(FLAG_ERPN);
    clearSystemFlag(FLAG_ERPN);
    ppvTestExpect("V-DECL the latch survives a declaration item (=8)", "VDC", "1+(2+5)");
    if(erpnWasDC) {
      setSystemFlag(FLAG_ERPN);
    }
  }

  /* V-FILL: FILL is the only arm that fills the stack without
   * ppvPush, so it has to arm the saturation latch itself. Without
   * that, T never replicates, and the walk declines a program the
   * machine runs. Eight adds outlive either stack depth, and the
   * replicated T keeps the same right-nested picture at both, so the
   * assertion is one string and the stack size is the axis. */
  {
    static const uint8_t pgmFL[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','F','L',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '7',
      PPV2(ITM_FILL),
      PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD),
      PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD),
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmFL, sizeof(pgmFL));

    bool_t ss8WasFL = getSystemFlag(FLAG_SSIZE8);

    setSystemFlag(FLAG_SSIZE8);
    ppvTestExpect("V-FILL SSIZE8 saturates, so T replicates (=63)", "VFL",
                  "7+(7+(7+(7+(7+(7+(7+(7+7)))))))");

    clearSystemFlag(FLAG_SSIZE8);
    ppvTestExpect("V-FILL SSIZE4 saturates, so T replicates (=63)", "VFL",
                  "7+(7+(7+(7+(7+(7+(7+(7+7)))))))");

    if(ss8WasFL) {
      setSystemFlag(FLAG_SSIZE8);
    }
    else {
      clearSystemFlag(FLAG_SSIZE8);
    }
  }

  /* V-MODE4: the stack-depth axis of the same oracle.
   * VM4: 1 2 3 4 5 + + + +.
   *   SSIZE8: nothing saturates. All five literals survive, XEQ
   *           returns 15.
   *   SSIZE4: the fifth literal's lift drops the 1, and each
   *           subsequent drop replicates T, so the machine adds 2
   *           twice and returns 16. */
  {
    static const uint8_t pgmVM4[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','M','4',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '4',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD), PPV2(ITM_ADD),
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmVM4, sizeof(pgmVM4));

    bool_t ss8Was = getSystemFlag(FLAG_SSIZE8);

    /* The value is the oracle: the walker keeps the
     * RPN's own right-nesting (4 5 + 3 + really is 3+(4+5)) and does
     * not flatten associative chains, so the pictures are
     * parenthesized. 1+(2+(3+(4+5))) is 15, the value XEQ returns
     * under SSIZE8. 2+(2+(3+(4+5))) is 16, the value it returns under
     * SSIZE4 with T replicated twice. */
    setSystemFlag(FLAG_SSIZE8);
    ppvTestExpect("V-MODE4 SSIZE8 keeps all five (=15)", "VM4", "1+(2+(3+(4+5)))");

    clearSystemFlag(FLAG_SSIZE8);
    ppvTestExpect("V-MODE4 SSIZE4 drops one and replicates T (=16)", "VM4", "2+(2+(3+(4+5)))");

    if(ss8Was) {
      setSystemFlag(FLAG_SSIZE8);
    }
    else {
      clearSystemFlag(FLAG_SSIZE8);
    }
  }

  // V1: the ask itself, a double integral recovered from the chain,
  // limits and d-variables and all, with the title idiom passing
  // through
  ppvTestExpect("V1 DBLINT", "VDBL", "INTEG(INTEG(t;t;0;x);x;0;2)");

  // V2: three coupled levels. Also pins that a STO'd name (ACC) and an
  // exponent literal ride through without reaching the mathematics.
  ppvTestExpect("V2 TRPINT", "VTRP", "INTEG(INTEG(INTEG(t;t;0;x);x;0;y);y;0;2)");

  // V3: a constructed function. SUB takes Y-X, so operand order shows
  // here. ENTER dups. MVAR declares nothing the picture carries. ENTER
  // x is x*x, not x^2.
  {
    char want[64];
    sprintf(want, "x%sx-x%sp-2", STD_CROSS, STD_CROSS);
    ppvTestExpect("V3 FX", "VFX", want);
  }

  /* ---- programmed sums ---------------------------------------------- */
  {
    static const uint8_t pgmBD[] = {          // body: n^2, off the stack
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','D',
      ITM_ENTER,
      ITM_MULT,
      PPV2(ITM_END),
    };
    static const uint8_t pgmS1[] = {          // unit step
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','S','1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','B','D',
      PPV2(ITM_END),
    };
    static const uint8_t pgmS2[] = {          // step 2
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','S','2',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '9',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','B','D',
      PPV2(ITM_END),
    };
    static const uint8_t pgmBN[] = {          // body recalls a REAL 'n'
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','N',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'n',
      PPV2(ITM_END),
    };
    static const uint8_t pgmS4[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','S','4',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','B','N',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmBD, sizeof(pgmBD));
    ppcTestWriteAndLoadPgm(pgmS1, sizeof(pgmS1));
    ppcTestWriteAndLoadPgm(pgmS2, sizeof(pgmS2));
    ppcTestWriteAndLoadPgm(pgmBN, sizeof(pgmBN));
    ppcTestWriteAndLoadPgm(pgmS4, sizeof(pgmS4));
  }
  // V4/V5: the counter has no name in RPN. It arrives in a filled
  // stack, so the text invents one. A unit step is the evaluator's
  // default and is left out.
  {
    char want[64];
    sprintf(want, "SUM(n%sn;n;1;5)", STD_CROSS);
    ppvTestExpect("V4 sum unit step", "VS1", want);
    sprintf(want, "SUM(n%sn;n;1;9;2)", STD_CROSS);
    ppvTestExpect("V5 sum step 2", "VS2", want);
  }
  // V6: the invented name must never shadow a real variable spelled
  // the same way. Upstream reads the variable, the text reads the
  // counter, and nothing on screen says so.
  ppvTestDecline("V6 counter collides with a real variable", "VS4", 12);

  /* ---- declines ------------------------------------------------------ */
  {
    static const uint8_t pgmSLV[] = {         // SOLVE has no construct
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','L','V',
      PPV2(ITM_PGMSLV), STRING_LABEL_VARIABLE, 3, 'V','F','X',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      PPV2(ITM_SOLVE), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    static const uint8_t pgmLOC[] = {         // XEQ of a local label
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','L','O','C',
      ITM_XEQ, 5,
      PPV2(ITM_END),
    };
    static const uint8_t pgmIND[] = {         // indirect XEQ
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','I','N','D',
      ITM_XEQ, INDIRECT_REGISTER, 0,
      PPV2(ITM_END),
    };
    static const uint8_t pgmOPQ[] = {         // a string reaching the maths
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','O','P','Q',
      ITM_LITERAL, STRING_LABEL_VARIABLE, 3, 'a','b','c',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_ADD,
      PPV2(ITM_END),
    };
    static const uint8_t pgmDRT[] = {         // recall of a name a STO changed
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','R','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_STO, STRING_LABEL_VARIABLE, 1, 'q',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'q',
      PPV2(ITM_END),
    };
    static const uint8_t pgmREG[] = {         // recall of a numbered register
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','R','E','G',
      ITM_RCL, 5,
      PPV2(ITM_END),
    };
    static const uint8_t pgmUF[] = {          // consumes what it was never given
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','U','F',
      ITM_ADD,
      PPV2(ITM_END),
    };
    static const uint8_t pgmNOL[] = {         // an integral with no PGMINT
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','N','O','L',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 't',
      PPV2(ITM_END),
    };
    static const uint8_t pgmFLW[] = {         // flow control
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','F','L','W',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_GTO, 5,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmSLV, sizeof(pgmSLV));
    ppcTestWriteAndLoadPgm(pgmLOC, sizeof(pgmLOC));
    ppcTestWriteAndLoadPgm(pgmIND, sizeof(pgmIND));
    ppcTestWriteAndLoadPgm(pgmOPQ, sizeof(pgmOPQ));
    ppcTestWriteAndLoadPgm(pgmDRT, sizeof(pgmDRT));
    ppcTestWriteAndLoadPgm(pgmREG, sizeof(pgmREG));
    ppcTestWriteAndLoadPgm(pgmUF,  sizeof(pgmUF));
    ppcTestWriteAndLoadPgm(pgmNOL, sizeof(pgmNOL));
    ppcTestWriteAndLoadPgm(pgmFLW, sizeof(pgmFLW));
  }
  ppvTestDecline("V7 SOLVE has no construct",        "VSLV",  1);
  ppvTestDecline("V8 local label",                   "VLOC",  3);
  ppvTestDecline("V9 indirect parameter",            "VIND",  2);
  ppvTestDecline("V10 string reaching the maths",    "VOPQ", 11);
  ppvTestDecline("V11 recall of a stored name",      "VDRT",  5);
  ppvTestDecline("V12 numbered-register recall",     "VREG",  7);
  ppvTestDecline("V13 stack underflow",              "VUF",  10);
  ppvTestDecline("V14 integral with no PGMINT",      "VNOL",  6);
  ppvTestDecline("V15 flow control",                 "VFLW",  1);

  /* ---- precedence and the latch ------------------------------------- */
  {
    static const uint8_t pgmPRC[] = {         // a / (b + c)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','P','R','C',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'c',
      ITM_ADD,
      ITM_DIV,
      PPV2(ITM_END),
    };
    static const uint8_t pgmLAT[] = {         // two integrals, one PGMINT
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','L','A','T',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','H','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 't',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 't',
      ITM_ADD,
      PPV2(ITM_END),
    };
    static const uint8_t pgmSLA[] = {         // a - (b + c)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','L','A',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'c',
      ITM_ADD,
      ITM_SUB,
      PPV2(ITM_END),
    };
    static const uint8_t pgmSLM[] = {         // a / (b * c)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','L','M',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'c',
      ITM_MULT,
      ITM_DIV,
      PPV2(ITM_END),
    };
    static const uint8_t pgmIX[] = {          // integrand that integrates over x too
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','I','X',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','H','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    static const uint8_t pgmSHD[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','H','D',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','I','X',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmPRC, sizeof(pgmPRC));
    ppcTestWriteAndLoadPgm(pgmLAT, sizeof(pgmLAT));
    ppcTestWriteAndLoadPgm(pgmSLA, sizeof(pgmSLA));
    ppcTestWriteAndLoadPgm(pgmSLM, sizeof(pgmSLM));
    static const uint8_t pgmMON[] = {         // -1/sqrt(a^2)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','M','O','N',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_SQUARE,
      ITM_SQUAREROOTX,
      ITM_1ONX,
      ITM_CHS,
      PPV2(ITM_END),
    };
    static const uint8_t pgmMOB[] = {         // 1/(a+b)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','M','O','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_ADD,
      ITM_1ONX,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmIX,  sizeof(pgmIX));
    ppcTestWriteAndLoadPgm(pgmSHD, sizeof(pgmSHD));
    static const uint8_t pgmMOP[] = {         // 1/(a*b)
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','M','O','P',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_MULT,
      ITM_1ONX,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmMON, sizeof(pgmMON));
    ppcTestWriteAndLoadPgm(pgmMOB, sizeof(pgmMOB));
    ppcTestWriteAndLoadPgm(pgmMOP, sizeof(pgmMOP));
  }

  /* ---- shapes the appnote-22 set never reaches --------------------- */
  {
    // The appnote's own PLTINTG integrand: a constructed function
    // inside an integral, the case Jaymos described as "a single
    // function ... or nested or serial functions"
    static const uint8_t pgmIG[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','I','G',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','F','X',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '8',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    // serial: two subroutines in a row over one stack
    static const uint8_t pgmAD1[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','A','D','1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_ADD,
      PPV2(ITM_END),
    };
    static const uint8_t pgmDB2[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','B','2',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_MULT,
      PPV2(ITM_END),
    };
    static const uint8_t pgmSER[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','E','R',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_XEQ, STRING_LABEL_VARIABLE, 4, 'V','A','D','1',
      ITM_XEQ, STRING_LABEL_VARIABLE, 4, 'V','D','B','2',
      PPV2(ITM_END),
    };
    // the product arm
    static const uint8_t pgmPRD[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','P','R','D',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '4',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_PIn), STRING_LABEL_VARIABLE, 3, 'V','B','D',
      PPV2(ITM_END),
    };
    // an integration limit that is itself an expression
    static const uint8_t pgmLIM[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','L','I','M',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','H','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'u',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_MULT,
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 't',
      PPV2(ITM_END),
    };
    // ENTER then a LITERAL: the lift latch. 5 ENTER 3 + is 8. A
    // walker that ignores the latch reads 5+5 or 3+3.
    static const uint8_t pgmLFT[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','L','F','T',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '5',
      ITM_ENTER,
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_ADD,
      PPV2(ITM_END),
    };
    // stack motions: x<>y reverses a non-commutative operand order
    static const uint8_t pgmSWP[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','W','P',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_XexY,
      ITM_SUB,
      PPV2(ITM_END),
    };
    // DROPY removes the SECOND level, not the top
    static const uint8_t pgmDRY[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','R','Y',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'c',
      PPV2(ITM_DROPY),
      ITM_ADD,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmIG,  sizeof(pgmIG));
    ppcTestWriteAndLoadPgm(pgmAD1, sizeof(pgmAD1));
    ppcTestWriteAndLoadPgm(pgmDB2, sizeof(pgmDB2));
    ppcTestWriteAndLoadPgm(pgmSER, sizeof(pgmSER));
    ppcTestWriteAndLoadPgm(pgmPRD, sizeof(pgmPRD));
    ppcTestWriteAndLoadPgm(pgmLIM, sizeof(pgmLIM));
    ppcTestWriteAndLoadPgm(pgmLFT, sizeof(pgmLFT));
    // The lift latch's whole effect is on stack depth: ENTER-then-
    // replace and ENTER-then-push agree on the top two values and
    // differ only underneath, so a pin has to reach past them to see
    // it. This one consumes one more than the correct trace provides.
    // It declines. A walker that skips the latch finds a phantom copy
    // waiting and prints an expression instead.
    static const uint8_t pgmLF2[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','L','F','2',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_ENTER,
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_SUB,
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'c',
      ITM_MULT,
      ITM_ADD,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmSWP, sizeof(pgmSWP));
    ppcTestWriteAndLoadPgm(pgmDRY, sizeof(pgmDRY));
    // Functions with a name the evaluator resolves
    static const uint8_t pgmSIN[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','I','N',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_sin,
      PPV2(ITM_END),
    };
    static const uint8_t pgmSF[] = {          // SIN(x)/2, a function inside a fraction
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','S','F',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_sin,
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_DIV,
      PPV2(ITM_END),
    };
    static const uint8_t pgmEXP[] = {         // a name the grammar cannot spell
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','E','X','P',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_EXP,
      PPV2(ITM_END),
    };
    static const uint8_t pgmISN[] = {         // an integral over a function
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','I','S','N',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 4, 'V','S','I','N',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmLF2, sizeof(pgmLF2));
    ppcTestWriteAndLoadPgm(pgmSIN, sizeof(pgmSIN));
    ppcTestWriteAndLoadPgm(pgmSF,  sizeof(pgmSF));
    ppcTestWriteAndLoadPgm(pgmEXP, sizeof(pgmEXP));
    static const uint8_t pgmCUB[] = {         // x^3 has a grammar spelling
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','C','U','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_CUBE,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmISN, sizeof(pgmISN));
    static const uint8_t pgmPOW[] = {         // (a^2)^2, the stacked power
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','P','O','W',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_SQUARE,
      ITM_SQUARE,
      PPV2(ITM_END),
    };
    // The derivative family
    static const uint8_t pgmDB[] = {          // f(x) = x*x
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','D','B',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_ENTER,
      ITM_MULT,
      PPV2(ITM_END),
    };
    static const uint8_t pgmDRV[] = {         // f'(x) at 3
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','R','V',
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','D','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    static const uint8_t pgmDR2[] = {         // f"(x) at 3
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','R','2',
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','D','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F2DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    static const uint8_t pgmDNL[] = {         // a derivative with no PGMDRV
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','N','L',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    static const uint8_t pgmDIN[] = {         // PGMINT's latch must NOT serve f'
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','D','I','N',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','D','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmCUB, sizeof(pgmCUB));
    ppcTestWriteAndLoadPgm(pgmPOW, sizeof(pgmPOW));
    ppcTestWriteAndLoadPgm(pgmDB,  sizeof(pgmDB));
    ppcTestWriteAndLoadPgm(pgmDRV, sizeof(pgmDRV));
    ppcTestWriteAndLoadPgm(pgmDR2, sizeof(pgmDR2));
    ppcTestWriteAndLoadPgm(pgmDNL, sizeof(pgmDNL));
    // Class fixtures: bodies differing only in their MVAR
    // declarations, driven by an f' that does or does not name them
    static const uint8_t pgmB1[] = {          // MVAR 'x'; body reads x
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','1',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'x',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    static const uint8_t pgmB2[] = {          // MVAR 'y','x'; body reads y
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','2',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'y',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'y',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    static const uint8_t pgmB3[] = {          // no MVAR at all
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','3',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'v',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    static const uint8_t pgmB4[] = {          // MVAR 'y','x'; consumes the STACK
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','4',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'y',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'x',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    #define PPVDRV(nm, body, param) \
      ITM_LBL, STRING_LABEL_VARIABLE, 3, nm[0], nm[1], nm[2], \
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, body[0], body[1], body[2], \
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3', \
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, param, \
      PPV2(ITM_END)
    static const uint8_t pgmD1[] = { PPVDRV("VD1", "VB1", 'x') };
    static const uint8_t pgmD2[] = { PPVDRV("VD2", "VB1", 'v') };
    static const uint8_t pgmD3[] = { PPVDRV("VD3", "VB2", 'x') };
    static const uint8_t pgmD4[] = { PPVDRV("VD4", "VB4", 'v') };
    static const uint8_t pgmD5[] = { PPVDRV("VD5", "VB3", 'v') };
    #undef PPVDRV
    ppcTestWriteAndLoadPgm(pgmDIN, sizeof(pgmDIN));
    ppcTestWriteAndLoadPgm(pgmB1, sizeof(pgmB1));
    ppcTestWriteAndLoadPgm(pgmB2, sizeof(pgmB2));
    ppcTestWriteAndLoadPgm(pgmB3, sizeof(pgmB3));
    ppcTestWriteAndLoadPgm(pgmB4, sizeof(pgmB4));
    ppcTestWriteAndLoadPgm(pgmD1, sizeof(pgmD1));
    ppcTestWriteAndLoadPgm(pgmD2, sizeof(pgmD2));
    ppcTestWriteAndLoadPgm(pgmD3, sizeof(pgmD3));
    ppcTestWriteAndLoadPgm(pgmD4, sizeof(pgmD4));
    // DAG and clear-order fixtures
    #define PPV_DUP4 ITM_ENTER, ITM_MULT, ITM_ENTER, ITM_MULT, \
                     ITM_ENTER, ITM_MULT, ITM_ENTER, ITM_MULT
    #define PPV_REC4 ITM_1ONX, ITM_1ONX, ITM_1ONX, ITM_1ONX
    static const uint8_t pgmXP[] = {          // 20 ENTER-x pairs: a DAG
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','X','P',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV_DUP4, PPV_DUP4, PPV_DUP4, PPV_DUP4, PPV_DUP4,
      PPV2(ITM_END),
    };
    static const uint8_t pgmBIG[] = {         // 20 nested reciprocals
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','B','I','G',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      PPV_REC4, PPV_REC4, PPV_REC4, PPV_REC4, PPV_REC4,
      PPV2(ITM_END),
    };
    #undef PPV_DUP4
    #undef PPV_REC4
    // A body that takes its argument off the stack and declares no
    // MVAR, the ordinary RPN function shape
    static const uint8_t pgmB5[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','5',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    static const uint8_t pgmD6[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','D','6',
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','B','5',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    // No MVAR, and the body recalls a real 'n', the name the
    // inventor reaches for first
    static const uint8_t pgmDN[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','D','N',
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','B','N',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'x',
      PPV2(ITM_END),
    };
    // the first declaration is one we cannot draw
    static const uint8_t pgmB6[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','B','6',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 2, 'x','1',
      PPV2(ITM_MVAR), STRING_LABEL_VARIABLE, 1, 'y',
      ITM_ENTER, ITM_MULT, PPV2(ITM_END),
    };
    static const uint8_t pgmD7[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','D','7',
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','B','6',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      PPV2(ITM_F1DRV), STRING_LABEL_VARIABLE, 1, 'z',
      PPV2(ITM_END),
    };
    // A sum whose upper limit is itself a sum. The inner one is
    // closed by the time the outer chooses, but it is drawn inside
    // the outer's limit.
    static const uint8_t pgmNS[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','N','S',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmNS, sizeof(pgmNS));
    ppcTestWriteAndLoadPgm(pgmDN, sizeof(pgmDN));
    ppcTestWriteAndLoadPgm(pgmB6, sizeof(pgmB6));
    ppcTestWriteAndLoadPgm(pgmD7, sizeof(pgmD7));
    ppcTestWriteAndLoadPgm(pgmB5, sizeof(pgmB5));
    ppcTestWriteAndLoadPgm(pgmD6, sizeof(pgmD6));
    ppcTestWriteAndLoadPgm(pgmD5, sizeof(pgmD5));
    // a construct used as an operand. The body is empty, so
    // it returns the seeded counter and the picture stays small enough to
    // read in a signature.
    static const uint8_t pgmNB[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','N','B',
      PPV2(ITM_END),
    };
    static const uint8_t pgmSQ[] = {          // (SUM ...)^2
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','S','Q',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_SQUARE,
      PPV2(ITM_END),
    };
    static const uint8_t pgmPX[] = {          // (PROD ...) x 2
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','P','X',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '3',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_PIn), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      ITM_MULT,
      PPV2(ITM_END),
    };
    // 22 twelve-letter names intern 264 bytes before the
    // construct's own variable, pushing its offset past 255
    static const uint8_t pgmOFF[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','O','F','F',
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','a','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','b','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','c','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','d','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','e','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','f','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','g','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','h','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','i','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','j','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','k','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','l','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','m','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','n','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','o','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','p','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','q','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','r','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','s','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','t','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','u','d','e','f','g','h','i','j','k','l',
      ITM_RCL, STRING_LABEL_VARIABLE, 12, 'q','q','v','d','e','f','g','h','i','j','k','l',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '0',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '2',
      PPV2(ITM_INTEGRAL_YX), STRING_LABEL_VARIABLE, 1, 'w',
      PPV2(ITM_END),
    };
    // the loop count lives in a variable called 'n'
    static const uint8_t pgmCOL[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','C','O','L',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'n',
      ITM_LITERAL, STRING_LONG_INTEGER, 1, '1',
      PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','B','D',
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmOFF, sizeof(pgmOFF));
    // an ENTER before a subroutine call
    static const uint8_t pgmXB[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','X','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_ADD,
      PPV2(ITM_END),
    };
    static const uint8_t pgmXA[] = {          // a x (a+b)
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','X','A',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_ENTER,
      ITM_XEQ, STRING_LABEL_VARIABLE, 3, 'V','X','B',
      ITM_MULT,
      PPV2(ITM_END),
    };
    // The latch clears at three arms. V72 pins one. These are the
    // other two, same shape.
    static const uint8_t pgmXI[] = {          // ENTER then PGMINT
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','X','I',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_ENTER,
      PPV2(ITM_PGMINT), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_ADD,
      ITM_MULT,
      PPV2(ITM_END),
    };
    static const uint8_t pgmXD[] = {          // ENTER then PGMDRV
      ITM_LBL, STRING_LABEL_VARIABLE, 3, 'V','X','D',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'a',
      ITM_ENTER,
      PPV2(ITM_PGMDRV), STRING_LABEL_VARIABLE, 3, 'V','N','B',
      ITM_RCL, STRING_LABEL_VARIABLE, 1, 'b',
      ITM_ADD,
      ITM_MULT,
      PPV2(ITM_END),
    };
    ppcTestWriteAndLoadPgm(pgmXB, sizeof(pgmXB));
    ppcTestWriteAndLoadPgm(pgmXA, sizeof(pgmXA));
    ppcTestWriteAndLoadPgm(pgmXI, sizeof(pgmXI));
    ppcTestWriteAndLoadPgm(pgmXD, sizeof(pgmXD));
    // Five disjoint sums. Each closes before the next opens, so
    // nothing can confuse their counters and all five can be 'n'.
    #define PPV_SUM1 ITM_LITERAL, STRING_LONG_INTEGER, 1, '1', \
                     ITM_LITERAL, STRING_LONG_INTEGER, 1, '3', \
                     ITM_LITERAL, STRING_LONG_INTEGER, 1, '1', \
                     PPV2(ITM_SIGMAn), STRING_LABEL_VARIABLE, 3, 'V','N','B'
    static const uint8_t pgmSIB[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'V','S','I','B',
      PPV_SUM1, PPV_SUM1, ITM_ADD,
      PPV_SUM1, ITM_ADD,
      PPV_SUM1, ITM_ADD,
      PPV_SUM1, ITM_ADD,
      PPV2(ITM_END),
    };
    #undef PPV_SUM1
    ppcTestWriteAndLoadPgm(pgmSIB, sizeof(pgmSIB));
    ppcTestWriteAndLoadPgm(pgmCOL, sizeof(pgmCOL));
    ppcTestWriteAndLoadPgm(pgmNB, sizeof(pgmNB));
    ppcTestWriteAndLoadPgm(pgmSQ, sizeof(pgmSQ));
    ppcTestWriteAndLoadPgm(pgmPX, sizeof(pgmPX));
    ppcTestWriteAndLoadPgm(pgmXP, sizeof(pgmXP));
    ppcTestWriteAndLoadPgm(pgmBIG, sizeof(pgmBIG));
  }
  {
    char want[96];
    // V29: the appnote's own integrand under its own integral
    sprintf(want, "INTEG(x%sx-x%sp-2;x;0;8)", STD_CROSS, STD_CROSS);
    ppvTestExpect("V29 constructed function under an integral", "VIG", want);
    // V30: serial subroutines, the other half of what was asked for
    sprintf(want, "(a+1)%s2", STD_CROSS);
    ppvTestExpect("V30 serial XEQ chain", "VSER", want);
    // V31: the product arm
    sprintf(want, "PROD(n%sn;n;1;4)", STD_CROSS);
    ppvTestExpect("V31 programmed product", "VPRD", want);
    // V32: a limit that is a whole expression
    sprintf(want, "INTEG(t;t;0;u%s2)", STD_CROSS);
    ppvTestExpect("V32 computed integration limit", "VLIM", want);
  }
  // V33: ENTER latches the lift, so the next literal replaces X
  ppvTestExpect("V33 ENTER then literal", "VLFT", "5+3");
  // V34/V35: stack motions
  ppvTestExpect("V34 x<>y reverses the operands", "VSWP", "b-a");
  ppvTestExpect("V35 DROPY removes the second level", "VDRY", "a+c");
  // V38: the latch seen from underneath. See the fixture's note above.
  ppvTestDecline("V38 the lift latch leaves no phantom copy", "VLF2", 10);

  /* ---- Named functions ---------------------------------------------- */
  // V40: a function with a name the evaluator resolves is emitted, and
  // the name comes from the item's own catalog spelling
  ppvTestExpect("V40 a named function", "VSIN", "SIN(x)");
  // V41: this pins that the fraction survives a function inside it.
  // Without the f(x) arm, the strict parser fails on the trailing
  // '(', and the whole formula loses its 2D form, fraction and all.
  {
    uint8_t root;
    ppReset();
    if(!ppqParse("SIN(x)/2", PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
      ppTestFail("V41 a function inside a fraction did not parse");
    }
    else if(ppNodeAt(root)->kind != PP_FRAC) {
      ppTestFailInt("V41 root kind", PP_FRAC, ppNodeAt(root)->kind);
    }
    ppvTestExpect("V41 transpiles to it", "VSF", "SIN(x)/2");
  }
  // V42: a monadic whose catalog spelling is glyphs (e^x) has no name
  // the grammar can carry, so it declines
  ppvTestDecline("V42 a name the grammar cannot spell", "VEXP", 1);
  // V43: the two compose, a function under an integral
  ppvTestExpect("V43 function under an integral", "VISN", "INTEG(SIN(x);x;0;1)");
  // V45: the cube has a grammar spelling, so it stays 2D
  ppvTestExpect("V45 cube", "VCUB", "a^3");

  /* ---- Derivatives -------------------------------------------------- */
  // V52/V53: the point comes off the stack, the program from PGMDRV,
  // and the order is which item was used. Seeding follows the
  // integrator's own rule: differentiate.c fills the stack and stores
  // to the variable, exactly as DEI_xeq_user does.
  {
    char want[64];
    sprintf(want, "DERIV(x%sx;x;3)", STD_CROSS);
    ppvTestExpect("V52 first derivative", "VDRV", want);
    sprintf(want, "DERIV(x%sx;x;3;2)", STD_CROSS);
    ppvTestExpect("V53 second derivative", "VDR2", want);
  }
  // V59-V63: the class. Upstream does not differentiate with respect
  // to the f' parameter. calcDeriv asks deriv_pgm_variable(label),
  // which walks the body's own MVAR declarations and returns the one
  // matching the parameter, else the first declared, else nothing.
  {
    char want[64];
    // CONTROL: parameter matches the declaration
    sprintf(want, "DERIV(x%sx;x;3)", STD_CROSS);
    ppvTestExpect("V59 matching MVAR", "VD1", want);
    // no match -> upstream falls back to the FIRST declared, x. The
    // subscript must say x, not the parameter v.
    sprintf(want, "DERIV(x%sx;x;3)", STD_CROSS);
    ppvTestExpect("V60 parameter differs, body declares one", "VD2", want);
    // CONTROL: the body RCLs y explicitly while upstream varies x, so
    // the derivative really is 0 and d/dx(y*y) is the correct picture.
    // This one must stay right. A fix that makes every subscript
    // follow the body breaks it.
    sprintf(want, "DERIV(y%sy;x;3)", STD_CROSS);
    ppvTestExpect("V61 body that does not read the sampled variable", "VD3", want);
    // None match, and the body consumes the stack. Seeding must
    // follow upstream's choice, so both the body and the subscript
    // become y.
    sprintf(want, "DERIV(y%sy;y;3)", STD_CROSS);
    ppvTestExpect("V62 first when none match, stack-consuming body", "VD4", want);
    // A body declaring no MVAR is still differentiated. fnFillStack
    // is unconditional, so a body reading the stack varies correctly.
    // With no name in the program the picture invents one, exactly as
    // a sum does. VD5's body RCLs 'v', which upstream never varies,
    // so the slope really is 0 and d/dn(v*v) is the correct picture.
    {
      char w2[64];
      sprintf(w2, "DERIV(v%sv;n;3)", STD_CROSS);
      ppvTestExpect("V63 body declares no MVAR, reads a variable", "VD5", w2);
      sprintf(w2, "DERIV(n%sn;n;3)", STD_CROSS);
      ppvTestExpect("V63b body declares no MVAR, reads the stack", "VD6", w2);
    }
  }

  // V66: ENTER shares its operand node, so the tree is a DAG, and the
  // layout pass visits a shared child once per path: 2^(k+1)-1 visits
  // for k dups, measured. Exhausting the 72-node layout pool DID not
  // stop it: a PP_NONE return was a per-call value, not a latch, so
  // the walk kept doubling through failures that cost nothing. 20
  // dups is 2,097,151 visits unlatched. On an 80 MHz DM42n the
  // reachable cases did not return at all. layoutFull is the latch.
  //
  // The pin asserts the visit count.
  {
    calcRegister_t id = findNamedLabel("VXP", GLOBAL_LABELS);
    uint8_t root;
    uint32_t visits = 0;
    if(id == INVALID_VARIABLE) {
      ppTestFail("V66 fixture absent");
    }
    else {
      ppvTestBuildNodes((uint16_t)(id - FIRST_LABEL), PP_FONT_STANDARD,
                        PP_FONT_STANDARD, &root, &visits);
      // the fit does not matter here. The try must be cheap.
      if(visits == 0) {
        ppTestFail("V66 the layout pass never ran");
      }
      else if(visits > 500) {
        ppTestFailInt("V66 layout visits (exponential re-expansion)", 500, (int)visits);
      }
    }
  }
  // V67: when no surface can hold the drawing, the owner gets an
  // error and keeps the answer.
  {
    calcRegister_t id = findNamedLabel("VBIG", GLOBAL_LABELS);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = ERROR_NONE;
    currentSolverStatus = 0;
    screenHoldsDrawnPixels = false;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    lcd_fill_rect(100, Y_POSITION_OF_REGISTER_X_LINE, 40, 12, LCD_EMPTY_VALUE);
    uint32_t xBefore = ppvSumRows(Y_POSITION_OF_REGISTER_X_LINE,
                                  Y_POSITION_OF_REGISTER_X_LINE + 20);
    fnPrettyVisual((uint16_t)id);
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) {
      ppTestFailInt("V67 undrawable program error", ERROR_INVALID_DATA_TYPE_FOR_OP,
                    lastErrorCode);
    }
    if(screenHoldsDrawnPixels) {
      ppTestFail("V67 an undrawable program held a screen");
    }
    if(ppvSumRows(21, 167) != xBefore) {
      ppTestFail("V67 the screen was cleared for a drawing that never came");
    }
    lastErrorCode = ERROR_NONE;
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode &= ~SCRUPD_MANUAL_STACK;
  }

  // V70: the construct's variable name is interned past pool offset
  // 255. The name must still resolve to 'w'.
  ppvTestExpect("V70 variable name interned past offset 255", "VOFF",
                "INTEG(w;w;0;2)");
  // V71: the invented counter must avoid enclosing bindings, free
  // names, and the limit subtrees. A closed sibling's counter is out
  // of scope and its name is reusable (V77 pins that).
  {
    char want[64];
    sprintf(want, "SUM(m%sm;m;1;n)", STD_CROSS);
    ppvTestExpect("V71 counter avoids a name the formula already uses",
                  "VCOL", want);
  }

  // V72: the epilogue that clears the ENTER lift latch must run
  // before XEQ, PGMINT, and PGMDRV return, or the callee's first
  // lifting read overwrites the dup. At top level
  // that produces a false underflow decline for a program that never
  // underflows. Inside a construct body, where the seeded frame
  // supplies a phantom operand, it produces a silent wrong drawing
  // instead.
  {
    char want[64];
    sprintf(want, "a%s(a+b)", STD_CROSS);
    ppvTestExpect("V72 the lift latch does not survive XEQ", "VXA", want);
    // the same at the other two arms
    ppvTestExpect("V75 nor PGMINT", "VXI", want);
    ppvTestExpect("V76 nor PGMDRV", "VXD", want);
  }

  // V73: the node signature prints every PP_BIGOP as "B(...)" and
  // drops the operator tag. V46, V50, and V68 pass unchanged even if
  // an integral is drawn as a sum. The tag is what the paint pass
  // picks the stroke glyph from, so it is the operator. It lives in
  // the box's textOff (prettyInternal.h). Assert it directly rather
  // than reformat every signature in the file.
  {
    struct { const char *label; uint16_t tag; const char *what; } tags[3] = {
      { "VDBL", ITM_INTEGRAL_YX, "V73 an integral draws the integral stroke" },
      { "VS1",  ITM_SIGMAn,      "V73 a sum draws the sum stroke"            },
      { "VPX",  ITM_PIn,         "V73 a product draws the product stroke"    },
    };
    for(unsigned t = 0; t < 3; t++) {
      calcRegister_t id = findNamedLabel(tags[t].label, GLOBAL_LABELS);
      uint8_t root;
      if(id == INVALID_VARIABLE
          || !ppvTestBuildNodes((uint16_t)(id - FIRST_LABEL), PP_FONT_STANDARD,
                                PP_FONT_STANDARD, &root, NULL)) {
        ppTestFail(tags[t].what);
        continue;
      }
      // the outermost PP_BIGOP in the tree
      uint8_t n = root;
      while(n != PP_NONE && ppNodeAt(n)->kind != PP_BIGOP) {
        n = ppNodeAt(n)->firstChild;
      }
      if(n == PP_NONE) {
        ppTestFail(tags[t].what);
      }
      else if(ppNodeAt(n)->textOff != tags[t].tag) {
        ppTestFailInt(tags[t].what, tags[t].tag, ppNodeAt(n)->textOff);
      }
    }
  }
  // V74: the second-order flag's node wiring. The drawn d^2/dx^2 must
  // keep its superscripts.
  {
    calcRegister_t id = findNamedLabel("VDR2", GLOBAL_LABELS);
    uint8_t root;
    if(id == INVALID_VARIABLE
        || !ppvTestBuildNodes((uint16_t)(id - FIRST_LABEL), PP_FONT_STANDARD,
                              PP_FONT_STANDARD, &root, NULL)) {
      ppTestFail("V74 second derivative did not lay out");
    }
    else if(!ppTreeHasRun(root, "d" "\xa1\x62")) {
      ppTestFail("V74 the drawn second derivative lost its superscript");
    }
  }

  /* V65: the differential oracle. Run the program, then evaluate the
   * walker's own drawing, and require the two to agree. No expected
   * string appears here. */
  {
    static const char *const oracle[] = { "VD1", "VD2", "VD6", "VDBL", "VS1" };
    uint8_t savedMode = calcMode;
    for(unsigned k = 0; k < 5; k++) {
      calcRegister_t id = findNamedLabel(oracle[k], GLOBAL_LABELS);
      char produced[256], what[64];
      uint8_t reason = 0;
      uint16_t atStep = 0;
      real34_t viaProgram, viaPicture, diff, tol;
      sprintf(what, "V65 %s: the picture and the program", oracle[k]);
      if(id == INVALID_VARIABLE) {
        ppTestFail(what);
        continue;
      }
      calcMode = CM_NORMAL;
      programRunStop = PGM_STOPPED;
      dynamicMenuItem = -1;
      lastErrorCode = ERROR_NONE;
      fnExecute(id);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        lastErrorCode = ERROR_NONE;
        ppTestFail(what);
        continue;
      }
      real34Copy(REGISTER_REAL34_DATA(REGISTER_X), &viaProgram);
      if(!ppvTranspile((uint16_t)(id - FIRST_LABEL), produced, sizeof(produced),
                       &reason, &atStep)) {
        ppTestFailures++;
        printf("prettyPrint test FAIL: %s (the walker drew nothing: D%u)\n",
               what, (unsigned)reason);
        continue;
      }
      if(numberOfFormulae == 0) {
        fnEqNew(NOPARAM);
      }
      calcMode = CM_NORMAL;
      aimBuffer[0] = 0;
      nimNumberPart = NP_EMPTY;
      setEquation(currentFormula, produced);
      lastErrorCode = ERROR_NONE;
      fnEqCalc(NOPARAM);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        lastErrorCode = ERROR_NONE;
        ppTestFailures++;
        printf("prettyPrint test FAIL: %s (the drawing did not evaluate)\n", what);
        continue;
      }
      real34Copy(REGISTER_REAL34_DATA(REGISTER_X), &viaPicture);
      real34Subtract(&viaPicture, &viaProgram, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail(what);
      }
      lastErrorCode = ERROR_NONE;
    }
    calcMode = savedMode;
  }

  // V77: a closed sibling's counter is out of scope, and its name is
  // reusable. Only free variables and enclosing constructs collide.
  {
    char want[128];
    const char *one = "SUM(n;n;1;3)";
    snprintf(want, sizeof(want), "%s+%s+%s+%s+%s", one, one, one, one, one);
    ppvTestExpect("V77 five disjoint sums may all use n", "VSIB", want);
  }

  // V80: the inner sum is closed when the outer picks its counter,
  // but it is drawn inside the outer's upper limit, so the two must
  // not share a name. The distinction is not "already built" but
  // "about to be drawn inside me".
  ppvTestExpect("V80 a construct inside a limit is not a sibling", "VNS",
                "SUM(m;m;1;SUM(n;n;1;3))");

  // V78: an invented derivative name is synthetic, and that flag is
  // what arms the shadow guard.
  ppvTestDecline("V78 an invented derivative name cannot shadow", "VDN", 12);
  // V79: upstream varies the first declaration whether or not we can
  // draw it.
  ppvTestDecline("V79 an undrawable first declaration declines", "VD7", 18);

  // V54: only a latch set during the walk counts
  ppvTestDecline("V54 derivative with no PGMDRV", "VDNL", 6);
  // V55: PGMDRV is a slot of its own upstream. A derivative must not
  // read whatever PGMINT last pointed at: that draws the wrong
  // function with no sign that it did.
  ppvTestDecline("V55 PGMINT's latch does not serve f'", "VDIN", 6);
  // V44: the emitted spelling must compute: it resolves through the
  // evaluator's own table
  {
    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    setEquation(currentFormula, "LN(1)+2");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("V44 an emitted function name did not compute");
    }
    else {
      real34_t want, diff, tol;
      stringToReal34("2", &want);
      real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-20", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail("V44 LN(1)+2 != 2");
      }
    }
    lastErrorCode = 0;
  }
  // V16: a lower-precedence right operand keeps its brackets.
  // Without them, a/(b+c) silently becomes a/b+c.
  ppvTestExpect("V16 precedence", "VPRC", "a/(b+c)");
  // V21/V22: an equal-precedence operand also keeps its brackets.
  // This is the case the left/right asymmetry exists for: the grammar
  // is left-associative, so a-(b+c) must keep its brackets while
  // a-b+c means something else.
  ppvTestExpect("V21 same-level right operand (additive)", "VSLA", "a-(b+c)");
  {
    char want[64];
    sprintf(want, "a/(b%sc)", STD_CROSS);
    ppvTestExpect("V22 same-level right operand (multiplicative)", "VSLM", want);
  }
  // V23: an inner d-variable spelled like an outer one leaves the
  // outer integral reading the inner's last node.
  ppvTestDecline("V23 shadowed d-variable", "VSHD", 12);
  // V24: the four monadics that have a grammar spelling. The radical
  // brings its own parentheses, so it must not also take precedence
  // brackets. sqrt((a)) parses, and is still wrong to emit.
  {
    char want[80];
    sprintf(want, "-1/%s(a^2)", "\xa2\x1a");
    ppvTestExpect("V24 monadics", "VMON", want);
  }
  // V25/V26: a monadic does bracket an operand that binds no tighter
  // than it does. V26 is the one that matters: 1/a*b is not 1/(a*b),
  // and only a same-level operand distinguishes the two arg levels.
  ppvTestExpect("V25 monadic brackets a sum", "VMOB", "1/(a+b)");
  {
    char want[80];
    sprintf(want, "1/(a%sb)", STD_CROSS);
    ppvTestExpect("V26 monadic brackets a product", "VMOP", want);
  }
  // V17: PGMINT is a persistent latch upstream, so a second integral
  // without a fresh one runs the same integrand
  ppvTestExpect("V17 latch persists", "VLAT", "INTEG(t;t;0;1)+INTEG(t;t;0;1)");

  /* ---- the loop closed: the text computes -------------------------- */
  // V18: a picture that cannot be evaluated is a transpilation that
  // only looks right. The double integral is 4/3.
  {
    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    // The walker's own output. A hand-
    // written expectation and a hand-written pin agree with each
    // other whether or not either is right. 4/3 is a number nobody
    // in this file chose.
    {
      calcRegister_t id = findNamedLabel("VDBL", GLOBAL_LABELS);
      char produced[256];
      uint8_t reason = 0;
      uint16_t atStep = 0;
      if(id == INVALID_VARIABLE
          || !ppvTranspile((uint16_t)(id - FIRST_LABEL), produced, sizeof(produced),
                           &reason, &atStep)) {
        ppTestFail("V18 the walker produced nothing to evaluate");
        strcpy(produced, "0");
      }
      setEquation(currentFormula, produced);
    }
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("V18 transpiled double integral errored");
    }
    else {
      real34_t want, diff, tol;
      stringToReal34("1.33333333333333333333333333333333", &want);
      real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail("V18 transpiled double integral != 4/3");
      }
    }
    lastErrorCode = 0;
  }

  /* ---- the surface --------------------------------------------------- */
  // V19: a stale integrate session must not frame a program's drawing in
  // an integral sign it never asked for, and the session must come back
  // exactly as it was.
  {
    calcRegister_t id = findNamedLabel("VPRC", GLOBAL_LABELS);
    uint32_t clean, stale;
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    currentSolverStatus = 0;
    fnPrettyVisual((uint16_t)id);
    clean = ppvBandSum();

    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    currentSolverStatus = (uint16_t)(SOLVER_STATUS_INTERACTIVE | SOLVER_STATUS_EQUATION_INTEGRATE);
    fnPrettyVisual((uint16_t)id);
    stale = ppvBandSum();

    if(currentSolverStatus != (uint16_t)(SOLVER_STATUS_INTERACTIVE | SOLVER_STATUS_EQUATION_INTEGRATE)) {
      ppTestFail("V19 solver session not restored");
    }
    if(clean == 0) {
      ppTestFail("V19 nothing was painted");
    }
    else if(clean != stale) {
      ppTestFail("V19 a stale solver session changed the drawing");
    }
  }
  /* V-CHROME: the Z/T arm declares which chrome it manages, and the
   * menu-repaint guard reads that declaration. Both bits are set
   * first, or the assertion passes on an ambient that was already
   * clear. V-FULL drives the full-screen arm. */
  {
    calcRegister_t id = findNamedLabel("VPRC", GLOBAL_LABELS);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    currentSolverStatus = 0;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode |= SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
    fnPrettyVisual((uint16_t)id);
    if(lastErrorCode != ERROR_NONE || !screenHoldsDrawnPixels
        || temporaryInformation != TI_SHOWNOTHING) {
      ppTestFailInt("V-CHROME the Z/T arm was never reached, so the row tests nothing",
                    0, (int)lastErrorCode);
    }
    else if(!(screenUpdatingMode & SCRUPD_MANUAL_STACK)) {
      ppTestFail("V-CHROME the Z/T arm did not claim the stack");
    }
    else if(screenUpdatingMode & (SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS)) {
      ppTestFail("V-CHROME the Z/T arm kept chrome it does not manage, so the menu is not repainted");
    }
    screenUpdatingMode = SCRUPD_AUTO;
  }

  // V20: a decline paints nothing. The whole text is composed before
  // a pixel is touched.
  {
    calcRegister_t id = findNamedLabel("VSLV", GLOBAL_LABELS);
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    screenHoldsDrawnPixels = false;
    fnPrettyVisual((uint16_t)id);
    if(screenHoldsDrawnPixels) {
      ppTestFail("V20 a declined program still painted");
    }
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) {
      ppTestFailInt("V20 decline error code", ERROR_INVALID_DATA_TYPE_FOR_OP, lastErrorCode);
    }
    lastErrorCode = 0;
  }

  /* V27: where the drawing goes was part of the request. The formula
   * lands in the Z/T rows, and the X line, which holds the answer the
   * program just computed, is left exactly as it was.
   *
   * The reach is asserted. The call has to come back
   * with no error and with the surface declaring its pixels, so a
   * decline cannot satisfy the ink sums by leaving the screen alone. */
  {
    calcRegister_t id = findNamedLabel("VDBL", GLOBAL_LABELS);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    currentSolverStatus = 0;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    // a stand-in for the answer sitting on the X line
    lcd_fill_rect(100, Y_POSITION_OF_REGISTER_X_LINE, 40, 12, LCD_EMPTY_VALUE);
    uint32_t xBefore = ppvSumRows(Y_POSITION_OF_REGISTER_X_LINE,
                                  Y_POSITION_OF_REGISTER_X_LINE + 20);
    fnPrettyVisual((uint16_t)id);
    // Both halves must carry ink: a double integral is taller than
    // one stack line.
    uint32_t inT = ppvSumRows(Y_POSITION_OF_REGISTER_T_LINE - 4,
                              Y_POSITION_OF_REGISTER_T_LINE + 31);
    uint32_t inZ = ppvSumRows(Y_POSITION_OF_REGISTER_Z_LINE - 4,
                              Y_POSITION_OF_REGISTER_Z_LINE + 31);
    uint32_t inWindow = inT + inZ;
    uint32_t below    = ppvSumRows(Y_POSITION_OF_REGISTER_Y_LINE - 4,
                                   Y_POSITION_OF_REGISTER_Y_LINE + 31);
    uint32_t xAfter   = ppvSumRows(Y_POSITION_OF_REGISTER_X_LINE,
                                   Y_POSITION_OF_REGISTER_X_LINE + 20);
    if(lastErrorCode != ERROR_NONE) {
      ppTestFailInt("V27 the Z/T arm was never reached, so the ink sums test nothing",
                    0, (int)lastErrorCode);
    }
    if(inWindow == 0) {
      ppTestFail("V27 nothing drawn in the Z/T window");
    }
    if(inT == 0 || inZ == 0) {
      ppTestFail("V27 the double integral did not span both stack rows");
    }
    if(below != 0) {
      ppTestFail("V27 the drawing spilled past Z into the Y line");
    }
    if(xAfter != xBefore) {
      ppTestFail("V27 the X line was disturbed");
    }
    if(!screenHoldsDrawnPixels) {
      ppTestFail("V27 the Z/T surface did not declare its pixels");
    }
    if(temporaryInformation != TI_SHOWNOTHING) {
      ppTestFail("V27 the Z/T surface cannot be dismissed by EXIT");
    }
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode &= ~SCRUPD_MANUAL_STACK;
  }

  /* V-FULL: the other paint arm. A drawing too tall for the Z/T
   * window takes the whole band, frames it top and bottom, and claims
   * all three chrome bits. The stack-window arm claims one and clears
   * the other two, so the chrome mask is what tells the two arms
   * apart.
   *
   * V67 pins that both arms can fail. A quadruple integral is 98 px
   * standard and 91 tiny against a 72-row band, so both stack rungs
   * decline it and the full band (147 rows) takes it at the first. */
  {
    calcRegister_t id = findNamedLabel("VQDL", GLOBAL_LABELS);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = ERROR_NONE;
    currentSolverStatus = 0;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode = SCRUPD_AUTO;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    fnPrettyVisual((uint16_t)id);
    if(lastErrorCode != ERROR_NONE || !screenHoldsDrawnPixels) {
      ppTestFailInt("V-FULL the full-screen arm was never reached, so the row tests nothing",
                    0, (int)lastErrorCode);
    }
    else {
      uint16_t litTop = 0, litBottom = 0;
      for(int16_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on((uint32_t)x, 20))  litTop++;
        if(lcd_buffer_pixel_on((uint32_t)x, 168)) litBottom++;
      }
      if(litTop != SCREEN_WIDTH || litBottom != SCREEN_WIDTH) {
        ppTestFailInt("V-FULL the band was not framed top and bottom",
                      SCREEN_WIDTH * 2, (int)(litTop + litBottom));
      }
      if((screenUpdatingMode & (SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU
                                | SCRUPD_MANUAL_SHIFT_STATUS))
          != (SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS)) {
        ppTestFail("V-FULL a full-screen surface did not claim the whole chrome");
      }
      if(temporaryInformation != TI_SHOWNOTHING) {
        ppTestFail("V-FULL the full-screen surface cannot be dismissed by EXIT");
      }
      // and it is the full band: the
      // drawing has to reach past the Z line that bounds the other arm
      if(ppvSumRows((int16_t)(Y_POSITION_OF_REGISTER_Z_LINE + 32), 167) == 0) {
        ppTestFail("V-FULL nothing drawn below the Z/T window");
      }
    }
    lastErrorCode = ERROR_NONE;
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode = SCRUPD_AUTO;
  }
  // V28: the measurement this placement rests on. One stack line is
  // 36 px and holds a single integral only when shrunk. The T and Z
  // bands together are 72, so the double fits.
  {
    struct { const char *text; int16_t stdH; int16_t tinyH; } cases[] = {
      { "INTEG(t;t;0;1)",                            38, 31 },
      { "INTEG(INTEG(t;t;0;x);x;0;2)",               58, 51 },
      { "INTEG(INTEG(INTEG(t;t;0;x);x;0;y);y;0;2)",  78, 71 },
    };
    for(int c = 0; c < 3; c++) {
      for(int rung = 0; rung < 2; rung++) {
        uint8_t root;
        ppReset();
        if(!ppqParse(cases[c].text, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
          ppTestFail("V28 a transpiled integral did not parse");
          continue;
        }
        if(rung == 1) {
          ppSetFontDeep(root, PP_FONT_TINY);
        }
        if(!ppMeasure(root, 0)) {
          ppTestFail("V28 measure failed");
          continue;
        }
        const ppNode_t *n = ppNodeAt(root);
        int16_t want = rung ? cases[c].tinyH : cases[c].stdH;
        if(n->ascent + n->descent != want) {
          ppTestFailInt("V28 height", want, n->ascent + n->descent);
        }
      }
    }
    // and the two claims those numbers are here to hold
    if(!(38 > Y_POSITION_OF_REGISTER_Z_LINE - Y_POSITION_OF_REGISTER_T_LINE)) {
      ppTestFail("V28 a single line would now hold a full-size integral");
    }
    if(!(58 <= (Y_POSITION_OF_REGISTER_Z_LINE + 31) - (Y_POSITION_OF_REGISTER_T_LINE - 4) + 1)) {
      ppTestFail("V28 the Z/T pair no longer holds the double integral");
    }
  }

  /* ---- a program keyed in through PEM ------------------------------ */
  // V39: LBL 'VKEY' / RCL 'a' / ENTER / x / 2 / - keyed through PEM,
  // then transpiled. Nothing here was hand-encoded, so this pin
  // catches the walker reading a literal PEM writes differently from
  // the ones the other fixtures spell out.
  {
    uint8_t savedCalcMode4 = calcMode;
    uint16_t savedProg = currentProgramNumber;
    uint8_t *savedStep = currentStep;

    ppvPemBegin();
    ppvPemNamed(ITM_LBL, "VKEY");
    ppvPemNamed(ITM_RCL, "a");
    runFunction(ITM_ENTER);
    runFunction(ITM_MULT);
    runFunction(ITM_2);       // opens NIM
    runFunction(ITM_SUB);     // commits the literal, then its own step
    runFunction(ITM_RTN);     // terminate it, the way a user does

    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    clearSystemFlag(FLAG_ALPHA);
    tam.mode = 0;
    if(lastErrorCode != ERROR_NONE) {
      ppTestFailInt("V39 keying the program errored", ERROR_NONE, lastErrorCode);
      lastErrorCode = ERROR_NONE;
    }
    {
      char want[64];
      sprintf(want, "a%sa-2", STD_CROSS);
      ppvTestExpect("V39 a PEM-keyed program", "VKEY", want);
    }
    currentProgramNumber = savedProg;
    currentStep = savedStep;
    calcMode = savedCalcMode4;
  }

  /* ---- through the real keys --------------------------------------- */
  // V36: the whole chain, keypress to pixels. TM_LBLONLY has to hand
  // fnPrettyVisual a FIRST_LABEL-based id, the alpha buffer has to
  // resolve the typed name, and the drawing has to land where the
  // direct call put it.
  {
    uint8_t savedCalcMode3 = calcMode;
    bool_t  savedAlpha = getSystemFlag(FLAG_ALPHA);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = ERROR_NONE;
    currentSolverStatus = 0;
    aimBuffer[0] = 0;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);

    ppvTestKeyIn("VDBL");

    uint32_t inT = ppvSumRows(Y_POSITION_OF_REGISTER_T_LINE - 4,
                              Y_POSITION_OF_REGISTER_T_LINE + 31);
    uint32_t inZ = ppvSumRows(Y_POSITION_OF_REGISTER_Z_LINE - 4,
                              Y_POSITION_OF_REGISTER_Z_LINE + 31);
    if(lastErrorCode != ERROR_NONE) {
      ppTestFailInt("V36 typed VISUAL errored", ERROR_NONE, lastErrorCode);
    }
    if(inT == 0 || inZ == 0) {
      ppTestFail("V36 a typed VISUAL did not draw across the Z/T window");
    }
    if(tam.mode != 0) {
      ppTestFail("V36 still in TAM after ENTER");
    }
    lastErrorCode = ERROR_NONE;
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode &= ~SCRUPD_MANUAL_STACK;
    calcMode = savedCalcMode3;
    if(savedAlpha) { setSystemFlag(FLAG_ALPHA); } else { clearSystemFlag(FLAG_ALPHA); }
    aimBuffer[0] = 0;
  }
  // V37: a name that is not a label never reaches the walker at all.
  // TAM refuses it, and nothing is drawn.
  /* V36b: the softkey row must survive a VISUAL. The stack-window arm
   * sets MANUAL_STACK + TI_SHOWNOTHING in CM_NORMAL, which are
   * exactly _refreshNormalScreen's three early-return conjuncts, and
   * that return skips the menu and status bar too. The TAM label menu
   * is popped after fnPrettyVisual returns. DESIGN.md and the code's
   * own comment both claim only the stack is suspended. */
  {
    uint8_t savedCalcMode4 = calcMode;
    bool_t  savedAlpha4 = getSystemFlag(FLAG_ALPHA);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = ERROR_NONE;
    aimBuffer[0] = 0;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    screenHoldsDrawnPixels = false;

    ppvTestKeyIn("VDBL");
    lcd_fill_rect(0, SCREEN_HEIGHT - SOFTMENU_HEIGHT, SCREEN_WIDTH,
                  SOFTMENU_HEIGHT, LCD_SET_VALUE);   // clear the row, then ask for a repaint
    refreshScreen(900);   // a test-owned source id, as the key handler does with 117
    if(ppvSumRows((int16_t)(SCREEN_HEIGHT - SOFTMENU_HEIGHT),
                  (int16_t)(SCREEN_HEIGHT - 1)) == 0) {
      ppTestFail("V36b the softkey row was never repainted after a typed VISUAL");
    }

    lastErrorCode = ERROR_NONE;
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode &= ~SCRUPD_MANUAL_STACK;
    calcMode = savedCalcMode4;
    if(savedAlpha4) { setSystemFlag(FLAG_ALPHA); } else { clearSystemFlag(FLAG_ALPHA); }
  }

  {
    uint8_t savedCalcMode3 = calcMode;
    bool_t  savedAlpha = getSystemFlag(FLAG_ALPHA);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = ERROR_NONE;
    aimBuffer[0] = 0;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    screenHoldsDrawnPixels = false;

    ppvTestKeyIn("VZZQ");

    if(lastErrorCode != ERROR_LABEL_NOT_FOUND) {
      ppTestFailInt("V37 unknown label error", ERROR_LABEL_NOT_FOUND, lastErrorCode);
    }
    if(ppvSumRows(Y_POSITION_OF_REGISTER_T_LINE - 4,
                  Y_POSITION_OF_REGISTER_Z_LINE + 31) != 0) {
      ppTestFail("V37 an unknown label still drew something");
    }
    lastErrorCode = ERROR_NONE;
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode &= ~SCRUPD_MANUAL_STACK;
    calcMode = savedCalcMode3;
    if(savedAlpha) { setSystemFlag(FLAG_ALPHA); } else { clearSystemFlag(FLAG_ALPHA); }
    aimBuffer[0] = 0;
  }

  /* ---- The tree the product actually paints -------------------------- */
  // V46-V51 assert the node shape the product paints. Two cases
  // differ from the text form:
  //   V49: a fraction bar scopes, so a/(b+c) draws without
  //        parentheses where the text needed them
  //   V51: a stacked power does need its base bracketed, and the
  //        layout back end supplies the wrap
  {
    struct { const char *what; const char *label; const char *sig; } cases[10];
    char sMul[64], sSum[64];
    sprintf(sMul, "[[[x %s x] - [x %s p]] - 2]", STD_DOT, STD_DOT);
    sprintf(sSum, "B([n %s n]|[n = 1]|5)", STD_DOT);
    cases[0].what = "V46 nested integral";      cases[0].label = "VDBL";
    cases[0].sig  = "B([B([t d t]|0|x) d x]|0|2)";
    cases[1].what = "V47 binary chain";         cases[1].label = "VFX";
    cases[1].sig  = sMul;
    cases[2].what = "V48 function call";        cases[2].label = "VSIN";
    cases[2].sig  = "[SIN P(x)]";
    cases[3].what = "V49 the bar scopes";       cases[3].label = "VPRC";
    cases[3].sig  = "F(a|[b + c])";
    cases[4].what = "V50 sum with limits";      cases[4].label = "VS1";
    cases[4].sig  = sSum;
    cases[5].what = "V51 stacked power brackets its base";
    cases[5].label = "VPOW";                    cases[5].sig = "S(P(S(a|2))|2)";
    char sDrv[80];
    sprintf(sDrv, "[F(d|[d x]) U(P([x %s x])|[x = 3])]", STD_DOT);

    cases[6].what = "V56 derivative shape";
    cases[6].label = "VDRV";                    cases[6].sig = sDrv;
    // V57: an additive construct body keeps its brackets. The parser
    // gets this by sniffing the body's runs for a +/- joiner because a
    // parse has no precedence to consult. The tree asks the real
    // question instead, and must reach the same answer.
    char sScope[160];
    sprintf(sScope, "B([P([[[x %s x] - [x %s p]] - 2]) d x]|0|8)", STD_DOT, STD_DOT);
    cases[7].what = "V57 additive construct body is scoped";
    cases[7].label = "VIG";                     cases[7].sig = sScope;
    // V68/V69: a big operator is not an atom. Its body extends
    // rightward, so anything multiplied or raised beside it binds
    // into the body unless the construct is bracketed. Two programs
    // whose answers differ by a factor of 2.6 draw the same picture:
    // (1+2+3)^2 = 36 and 1^2+2^2+3^2 = 14.
    char sSq[96], sPx[96];
    sprintf(sSq, "S(P(B(n|[n = 1]|3))|2)");
    cases[8].what = "V68 a construct under a power is bracketed";
    cases[8].label = "VSQ";                     cases[8].sig = sSq;
    sprintf(sPx, "[P(B(n|[n = 1]|3)) %s 2]", STD_DOT);
    cases[9].what = "V69 a construct left of a product is bracketed";
    cases[9].label = "VPX";                     cases[9].sig = sPx;

    for(unsigned c = 0; c < 10; c++) {
      calcRegister_t id = findNamedLabel(cases[c].label, GLOBAL_LABELS);
      uint8_t root;
      if(id == INVALID_VARIABLE
          || !ppvTestBuildNodes((uint16_t)(id - FIRST_LABEL), PP_FONT_STANDARD,
                                PP_FONT_STANDARD, &root, NULL)) {
        ppTestFailures++;
        printf("prettyPrint test FAIL: %s (did not lay out)\n", cases[c].what);
        continue;
      }
      ppfTestExpect(cases[c].what, root, cases[c].sig);
    }
  }

  currentSolverStatus    = hadStatus;
  currentSolverVariable  = hadVar;
  currentSolverProgram   = hadProgram;
  calcMode               = hadMode;
  temporaryInformation   = TI_NO_INFO;
  screenHoldsDrawnPixels = false;
  aimBuffer[0] = 0;
  nimNumberPart = NP_EMPTY;

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}



/* ==== the real appnote-22 file (its own driver, anchored late) =========
 * Every fixture in prettyTestVisual is hand-encoded. This loads
 * docs/appnotes/sources/AN0022/func.p47, Jaymos's actual file, the
 * one his forum message pointed at, through the official loader, and
 * transpiles the labels he named.
 *
 * It lives in its own driver because it clears program memory.
 * Clearing is unavoidable here: our own fixtures have nearly filled
 * program memory, and func.p47's labels (DBLINT, HT, IT, ...) collide
 * with upstream's own nested_cov programs.
 *
 * It is anchored in testSuiteList.txt before graphs_cov. forth-core
 * appends at the tail, and two packages appending at EOF
 * produce the same hunk and a hard conflict. It runs after
 * programs.txt, which the ordering needs, but graphs_cov, nested_cov,
 * config_cov, and stack_cov still run after it. None of those depend
 * on preloaded programs.
 *
 * A missing file fails the driver. */
void prettyTestReal(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  /* ---- the REAL appnote-22 file ------------------------------------ */
  /* V58: every other fixture in this driver is hand-encoded. This
   * loads docs/appnotes/sources/AN0022/func.p47, Jaymos's actual
   * file, the one his forum message pointed at, through the official
   * loader, and transpiles the labels he named. It clears program
   * memory both before and after: the fixtures above have nearly
   * filled it, and a later driver needs a clean slate. It runs late
   * in the list, before the tail. See the driver's own header.
   *
   * A missing file fails the driver. The suite runs from the repo
   * root (meson test -C build.sim). */
  {
    extern void fnClPAll(uint16_t confirmation);
    FILE *in = fopen("docs/appnotes/sources/AN0022/func.p47", "rb");
    if(in == NULL) {
      ppTestFail("V58 cannot open the appnote-22 program file");
    }
    else {
      FILE *outF = fopen("c47programTest.bin", "wb");
      if(outF == NULL) {
        // Both fopen calls must be checked. An unchecked NULL FILE*
        // here crashes the whole suite.
        fclose(in);
        ppTestFail("V58 cannot write the loader's input file");
        outF = NULL;
      }
      else {
        int ch;
        while((ch = fgetc(in)) != EOF) {
          fputc(ch, outF);
        }
        fclose(in);
        fclose(outF);
      fnClPAll(CONFIRMED);
      lastErrorCode = ERROR_NONE;
      fnLoadProgram(NOPARAM);
      if(lastErrorCode != ERROR_NONE) {
        ppTestFailInt("V58 loading func.p47", ERROR_NONE, lastErrorCode);
        lastErrorCode = ERROR_NONE;
      }
      {
        char want[128];
        // the two he named in the forum message, plus the appnote's own
        // integrand-under-integral and its pi
        ppvTestExpect("V58 real DBLINT", "DBLINT", "INTEG(INTEG(t;t;0;x);x;0;2)");
        ppvTestExpect("V58 real TRPINT", "TRPINT",
                      "INTEG(INTEG(INTEG(t;t;0;x);x;0;y);y;0;2)");
        sprintf(want, "INTEG(x%sx-x%sp-2;x;0;8)", STD_CROSS, STD_CROSS);
        ppvTestExpect("V58 real IG", "IG", want);
        sprintf(want, "INTEG(4/(x%sx+1);x;0;1)", STD_CROSS);
        ppvTestExpect("V58 real INTPI", "INTPI", want);
        // and the halves that decline: SOLVE has no construct, PLOT none
        ppvTestDecline("V58 real SLVINT declines", "SLVINT", 1);
        ppvTestDecline("V58 real PLTROOT declines", "PLTROOT", 1);
      }
      fnClPAll(CONFIRMED);
      lastErrorCode = ERROR_NONE;
      }
    }
  }

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


#endif // PC_BUILD
