// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyTest.c
 * Test drivers for the pretty-print package.
 *
 * Drivers include prettyTestMeasure, prettyTestPixels, prettyTestFallback,
 * prettyTestShow, prettyTestEquation, prettyTestVisual, plus prettyTestReal.
 * testSuite.c registers each driver with coverageDriver = 1.
 * Test scripts pretty_print.txt and pretty_visual_real.txt run these tests.
 * This file compiles only when PC_BUILD is active.
 *
 * Each driver writes its failure count to register X as a long integer.
 * Specific tests make sure that pretty-print declines invalid inputs.
 * A decline returns false and draws nothing. The program walker decline
 * also reports an error code and step number.
 *
 * Helper functions ppTest* provide shared test scaffolding.
 * The pretty-print-extra package links against these helpers.
 */

#include "c47.h"
#include "prettyInternal.h"

#if defined(PC_BUILD)

#include <stdio.h>

uint32_t ppTestFailures;

void ppTestFail(const char *what) {
  ppTestFailures++;
  printf("prettyPrint test FAIL: %s\n", what);
}

void ppTestFailInt(const char *what, int32_t expected, int32_t actual) {
  ppTestFailures++;
  printf("prettyPrint test FAIL: %s (expected %d, actual %d)\n", what, expected, actual);
}

void ppTestWriteLonI(calcRegister_t regist, uint32_t value) {
  longInteger_t li;
  longIntegerInit(li);
  uInt32ToLongInteger(value, li);
  convertLongIntegerToLongIntegerRegister(li, regist);
  longIntegerFree(li);
}

void ppTestSetRealX(const char *value) {
  reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
  stringToReal34(value, REGISTER_REAL34_DATA(REGISTER_X));
}

bool_t ppTestIsLonI(calcRegister_t regist, uint32_t expected) {
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
    // Expected width is 26: stringWidth with showLeadingCols=false drops the '1'
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

  /* M8: substituted glyph class.
   * The engine derives metrics from &numericFont. It clears the measured
   * box, then paints with noPreClear. showGlyphCode can substitute a glyph
   * from a different table for the same code. If row metrics differ,
   * ink can fall outside the cleared area.
   *
   * The package must decline or expand the box to contain substitute ink.
   * Because bold mode remains active, paint routines suppress glyph
   * substitution and use the standard font face.
   *
   * Part 1 tests active font tables for glyph substitutions.
   * Part 2 makes sure that output is identical whether FLAG_BOLD is set or clear. */
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
      ppTestFail("M8 bold and plain row metrics now agree: the FLAG_BOLD suppression is obsolete, re-derive the class");
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
      ppTestFail("M8 setup: the inline surface declines with BOLD off, pin cannot reach its state");
    }
    else if(!drewBold) {
      ppTestFail("M8 the inline surface declined under FLAG_BOLD; the 2026-08-29 ruling is that BOLD must still display");
    }
    else if(litPlain == 0) {
      ppTestFail("M8 setup: nothing was painted with BOLD off, pin proves nothing");
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

void ppTestClearBand(void) {
  lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
}

bool_t ppTestRowAllLit(uint32_t row, uint32_t x0, uint32_t x1) {
  for(uint32_t x = x0; x <= x1; x++) {
    if(!lcd_buffer_pixel_on(x, row)) {
      return false;
    }
  }
  return true;
}

bool_t ppTestRowAnyLit(uint32_t row, uint32_t x0, uint32_t x1) {
  for(uint32_t x = x0; x <= x1; x++) {
    if(lcd_buffer_pixel_on(x, row)) {
      return true;
    }
  }
  return false;
}

bool_t ppTestRectAnyLit(uint32_t r0, uint32_t r1, uint32_t x0, uint32_t x1) {
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

void ppTestCaptureBand(int which, uint32_t top, uint32_t rows) {
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

void ppTestCapture(int which) {
  ppTestCaptureBand(which, PPT_BAND_TOP, PPT_BAND_ROWS);
}

static void ppTestRenderX(void) {
  ppTestClearBand();
  refreshRegisterLine(REGISTER_X);
}

bool_t ppTestSnapsEqual(void) {
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



/* ==== shared program-fixture loader and layout-signature decoders ======
 * Non-static: the pretty-print-extra package's capture and formula
 * drivers use these too (declared in prettyInternal.h). */

bool_t ppTreeHasRun(uint8_t n, const char *text) {
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

/* Single validation pass for the complete tree.
 * A PP_SUP node whose base already contains an exponent renders both
 * exponents at the same height. Every PP_SUP generator must enclose
 * such a base in parentheses. A base contains an exponent if it is
 * a PP_SUP node or if its text ends with superscript glyphs. */
bool_t ppfTestRunEndsSup(uint8_t n) {
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


void ppcTestNoteLabel(const uint8_t *pgm, size_t n) {
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

void ppcTestWriteAndLoadPgm(const uint8_t *pgm, size_t n) {
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

bool_t ppfTestPowersScoped(uint8_t n) {
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

void ppfTestSigNode(uint8_t n, char *out, size_t cap) {
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

void ppfTestExpect(const char *what, uint8_t root, const char *expected) {
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

/* ==== prettyTestEquation ================================================ */

void prettyTestEquation(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  uint8_t root;

  // EQ1: '/' binds individual factors before outer terms
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

  /* EQ36: a numeral that exceeds the text pool must fail parse.
   * The numeral must not be omitted silently from the drawing.
   * Buffer calculations use PP_TEXT_BYTES.
   * A control string that fits must parse successfully.
   * Costs: ten runs for '1+' take 20 bytes.
   * The numeral run uses N+1 bytes.
   * The denominator run uses 2 bytes. */
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

  /* EQ37 (PP18RR9-OOF-1): the text pool's bound, asserted directly. A
   * run of PP_TEXT_BYTES-1 bytes fills the pool exactly (cost is the
   * length plus one). A second, empty run must be refused. This pin
   * does not depend on the parser's run accounting, so it closes the
   * one-byte slack EQ36's derivation leaves. */
  {
    static char fill[PP_TEXT_BYTES];
    memset(fill, '7', sizeof(fill) - 1);
    fill[sizeof(fill) - 1] = 0;
    ppReset();
    if(ppNewRun(fill, (uint16_t)(PP_TEXT_BYTES - 1), PP_FONT_STANDARD) == PP_NONE) {
      ppTestFail("EQ37 a run that fills the pool exactly was refused");
    }
    else if(ppNewRun("", 0, PP_FONT_STANDARD) != PP_NONE) {
      ppTestFail("EQ37 the pool accepted one byte past its bound");
    }
    ppReset();
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
      ppTestFail("EQ9b setup: the fixture is not wider than the screen, pin proves nothing");
    }
    ppqFitWithEllipsis(wide, cut, sizeof(cut));
    if(stringWidth(cut, &standardFont, false, true) > SCREEN_WIDTH - 4) {
      ppTestFail("EQ9b the fitted line still overruns the screen");
    }
    if(strlen(cut) >= strlen(wide)) {
      ppTestFail("EQ9b the fitted line was not shortened");
    }
    if(strstr(cut, STD_ELLIPSIS) == NULL) {
      ppTestFail("EQ9b the truncation carries no marker: a lie by truncation");
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

  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== VISUAL, the RPN-program walker ====================================
 * Most pins here test the transpiled string. That string is a test artifact.
 * The real product is a node tree. The text back end compiles only under
 * PC_BUILD as a test interface. The tests read this string because it is
 * readable and derives from the same AST as the drawing.
 * Node-shape pins (such as V46-V51, V56, V57, V68-V69, plus V73-V74)
 * validate node layout directly.
 *
 * Tests V18 and V65 evaluate walker output. Their outputs match program
 * execution. These tests reside in prettyTestEqLang in
 * pretty-print-extra because they use that package's equation language.
 *
 * Fixtures come from docs/appnotes/sources/AN0022. They use package-local
 * labels to prevent label collisions. Test V58 loads the real file. */

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

uint32_t ppvSumRows(int16_t top, int16_t bottom) {
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

  /* V-XEQ: a callee's trailing ENTER must persist after return.
   * ITM_XEQ executes the full subroutine. If the epilogue clears the
   * lift latch after the call, it erases the state set by the callee.
   * Without the latch, the caller pushes the next literal to a new stack level.
   * The lift latch causes the 5 to overwrite the duplicate value.
   * The stack holds [1,2,4,5]. It adds these values to 11 and renders as 2+(4+5). */
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

  /* V-DECL: a declaration item between ENTER and read retains the latch.
   * MVAR represents this declaration group.
   * The walk contains at most four entries, so stack size does not change
   * the output.
   *   1 2 ENTER MVAR z 5 + +  ->  the 5 overwrites the duplicate: 1+(2+5).
   * If the item is missing from the list, the epilogue clears the latch.
   * The 5 then lifts, and the walker outputs 2+(2+5) for a program that
   * returns 8. */
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

  /* V-FILL: FILL fills the stack without ppvPush, so it sets the
   * saturation latch directly. Without the latch, register T never
   * replicates and the walk declines. Eight additions exceed both stack
   * sizes, and replicated register T produces the same nested structure. */
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
    // DROPY removes register Y at level 2. Register X remains unchanged.
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
  /* The lift-latch pins run in classic mode by their own hand: the
   * walker's ENTER arm reads the machine's FLAG_ERPN, so a pin that
   * inherits the ambient mode tests whatever ran before it. V38 under
   * eRPN legitimately DRAWS (the push is real), so without this pin
   * the decline assertion is order-dependent (found at PP19, when the
   * capture driver, whose eRPN trace used to restore the flag first,
   * moved behind these pins). */
  {
    bool_t erpnWasLatch = getSystemFlag(FLAG_ERPN);
    clearSystemFlag(FLAG_ERPN);
    // V33: ENTER latches the lift, so the next literal replaces X
    ppvTestExpect("V33 ENTER then literal", "VLFT", "5+3");
    // V34/V35: stack motions
    ppvTestExpect("V34 x<>y reverses the operands", "VSWP", "b-a");
    ppvTestExpect("V35 DROPY removes the second level", "VDRY", "a+c");
    // V38: the latch seen from underneath. See the fixture's note above.
    ppvTestDecline("V38 the lift latch leaves no phantom copy", "VLF2", 10);
    if(erpnWasLatch) {
      setSystemFlag(FLAG_ERPN);
    }
  }

  /* ---- Named functions ---------------------------------------------- */
  // V40: a function with a name the evaluator resolves is emitted, and
  // the name comes from the item's own catalog spelling
  ppvTestExpect("V40 a named function", "VSIN", "SIN(x)");
  // V41: this test verifies that the fraction remains intact with an internal function.
  // Without the f(x) branch, the strict parser rejects the trailing '('.
  // The complete formula then loses 2D layout.
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
    // If no match occurs, upstream code selects the first declared variable 'x'.
    // The subscript displays 'x' and not parameter 'v'.
    sprintf(want, "DERIV(x%sx;x;3)", STD_CROSS);
    ppvTestExpect("V60 parameter differs, body declares one", "VD2", want);
    // CONTROL: the body RCLs y explicitly while upstream varies x, so
    // the derivative really is 0 and d/dx(y*y) is the correct picture.
    // This one must stay right. A fix that makes every subscript
    // follow the body breaks it.
    sprintf(want, "DERIV(y%sy;x;3)", STD_CROSS);
    ppvTestExpect("V61 body that does not read the sampled variable", "VD3", want);
    // No items match, and the body reads from the stack.
    // Initialization follows upstream selection: the body and the subscript
    // each resolve to y.
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

  // V66: ENTER duplicates operand nodes to create a directed acyclic graph.
  // The layout traversal visits shared children multiple times.
  // When the 72-node pool filled up, traversals previously continued.
  // The layoutFull flag halts the traversal after pool exhaustion.
  //
  // This test validates the traversal visit count.
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
  // V71: the invented counter must not collide with enclosing
  // bindings, free names, or the limit subtrees. A closed sibling's
  // counter is out of scope and its name is reusable (V77 pins that).
  {
    char want[64];
    sprintf(want, "SUM(m%sm;m;1;n)", STD_CROSS);
    ppvTestExpect("V71 counter avoids a name the formula already uses",
                  "VCOL", want);
  }

  // V72: the epilogue clears the ENTER lift latch before dispatch returns
  // from XEQ, PGMINT, or PGMDRV. Otherwise, the callee's first lifting read
  // overwrites the duplicate. At top level, that defect causes an underflow
  // decline. Inside a construct body, it causes an incorrect drawing.
  {
    char want[64];
    sprintf(want, "a%s(a+b)", STD_CROSS);
    ppvTestExpect("V72 the lift latch does not survive XEQ", "VXA", want);
    // the same at the other two arms
    ppvTestExpect("V75 nor PGMINT", "VXI", want);
    ppvTestExpect("V76 nor PGMDRV", "VXD", want);
  }

  // V73: the node signature prints PP_BIGOP as "B(...)" and omits the
  // operator tag. Existing tests (such as V46, V50, or V68) pass if an
  // integral draws with a sum glyph. The tag selects the glyph during
  // paint operations. It is stored in textOff in the box structure.
  // This test validates the tag directly and avoids signature reformatting.
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

  /* V-FULL: full-screen drawing branch.
   * If a drawing exceeds the Z and T registers, it uses the full display
   * band. It adds borders and sets all three chrome control bits.
   * The stack branch sets only one chrome bit.
   *
   * V67 validates that both drawing branches handle tall drawings.
   * A quadruple integral measures 98 pixels in standard font and 91 pixels
   * in small font. Both sizes exceed the 72-row stack window, so the
   * routine uses the 147-row band. */
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
      ppTestFail("V28 a single line now holds a full-size integral");
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

    /* A menu below the TAM one, by the pin's own hand: the repaint
     * needs something to draw, and inheriting whichever menu an
     * earlier pin left up made this assertion order-dependent (the
     * PP19 ambient-state class). */
    showSoftmenu(-MNU_DISP);

    ppvTestKeyIn("VDBL");
    lcd_fill_rect(0, SCREEN_HEIGHT - SOFTMENU_HEIGHT, SCREEN_WIDTH,
                  SOFTMENU_HEIGHT, LCD_SET_VALUE);   // clear the row, then ask for a repaint
    refreshScreen(900);   // a test-owned source id, as the key handler does with 117
    if(ppvSumRows((int16_t)(SCREEN_HEIGHT - SOFTMENU_HEIGHT),
                  (int16_t)(SCREEN_HEIGHT - 1)) == 0) {
      ppTestFail("V36b the softkey row was never repainted after a typed VISUAL");
    }
    popSoftmenu();

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



/* ==== appnote-22 test driver ===========================================
 * Tests in prettyTestVisual use hand-coded fixtures.
 * This driver loads docs/appnotes/sources/AN0022/func.p47 with the standard loader.
 * It transpiles the labels from that file.
 *
 * This test uses a separate driver because it clears program memory.
 * Clearing program memory is necessary because fixtures fill program storage.
 * Labels in func.p47 also conflict with programs in nested_cov.
 *
 * This test is placed before graphs_cov in testSuiteList.txt.
 * Do not anchor at the end of the file, because multiple packages at
 * the tail cause merge conflicts.
 * The test runs after programs.txt. Later tests like graphs_cov, nested_cov,
 * config_cov, plus stack_cov do not require preloaded programs.
 *
 * If the file is missing, the driver reports a failure. */
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
