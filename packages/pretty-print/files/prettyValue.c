// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyValue.c
 * Register value -> layout tree, the inline stack-line surface, and the
 * PSHOW full-screen surface.
 *
 * Builder-first invariant (DESIGN.md §2): the upstream display builder
 * runs first and its output is parsed into the tree. The pretty form can
 * never disagree with what upstream shows, and side effects
 * (displayValueX) happen identically on both paths. Every parser
 * declines (returns false, paints nothing) on anything outside its
 * verified alphabet. Upstream then renders unchanged.
 */

#include "c47.h"
#include "prettyInternal.h"

static char ppScratch[200];
static char ppSpanA[120];
static char ppSpanB[120];

// Both toggles are system flags (FLAG_PRETTYP bit 50, FLAG_PTLINE bit
// 51, counts reserved by the claims registry). They persist across
// power cycles and answer to SF/CF/FS? and the flag browser. This
// package owns both flags, both commands and both defaults; only the
// T-line rendering lives in pretty-print-extra.

void fnPrettyToggle(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  if(getSystemFlag(FLAG_PRETTYP)) {
    clearSystemFlag(FLAG_PRETTYP);
  }
  else {
    setSystemFlag(FLAG_PRETTYP);
  }
}

// The PTLIN command only flips the core-owned flag, so it lives here.
// The T-line rendering itself is pretty-print-extra's, through
// ppTlineExtension below: in a build without that package the flag
// holds but nothing reads it.
void fnPrettyTlineToggle(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  if(getSystemFlag(FLAG_PTLINE)) {
    clearSystemFlag(FLAG_PTLINE);
  }
  else {
    setSystemFlag(FLAG_PTLINE);
  }
}

void prettySetTline(bool_t on) {
  if(on) {
    setSystemFlag(FLAG_PTLINE);
  }
  else {
    clearSystemFlag(FLAG_PTLINE);
  }
}

// pretty-print-extra's registration slots (prettyPrint.h). NULL until
// that package's lazy init runs; NULL is skipped.
bool_t (*ppTlineExtension)(int16_t baseY, int16_t bandTop,
                           int16_t bandBottom, int16_t *lineWidth) = NULL;
void   (*ppResetExtension)(void) = NULL;

void prettyReset(void) {
  if(ppResetExtension != NULL) {
    (*ppResetExtension)();
  }
  // Factory defaults. A RESET wipes the system flags before this hook
  // runs, so the default-ON flag must be set again here.
  setSystemFlag(FLAG_PRETTYP);
  clearSystemFlag(FLAG_PTLINE);
}

/* The system-flags catalog's generated row count, from the softmenu
 * table. The flag browser bounds its walk with this, and FB1 asserts
 * the same derivation, so the two cannot drift (PP18RR9-4). */
int16_t prettySysflRows(void) {
  for(uint16_t m = 0; softmenu[m].menuItem != 0; m++) {
    if(softmenu[m].menuItem == -MNU_SYSFL) {
      return softmenu[m].numItems;
    }
  }
  return NUMBER_OF_SYSTEM_FLAGS;
}

bool_t prettyEnabled(void) {
  return getSystemFlag(FLAG_PRETTYP);
}

void prettySetEnabled(bool_t on) {
  if(on) {
    setSystemFlag(FLAG_PRETTYP);
  }
  else {
    clearSystemFlag(FLAG_PRETTYP);
  }
}


/* ==== glyph classification ==============================================
 * All codes verified against fonts.h; parsers decline on anything else. */
#define PP_IS_SUP_DIGIT(code) ((code) >= 0xa160 && (code) <= 0xa169)
#define PP_IS_SUB_DIGIT(code) ((code) >= 0xa080 && (code) <= 0xa089)
#define PP_SUP_MINUS_CODE     0xa16b
#define PP_SUB10_CODE         0xa47d   /* STD_SUB_10 */
#define PP_RAD_CODE           0xa21a   /* STD_SQUARE_ROOT */
#define PP_PI_CODE            0x83c0   /* STD_pi */
#define PP_E_CODE             0xa147   /* STD_EulerE */
#define PP_PHI_CODE           0x83d5   /* STD_phi_m */
#define PP_ANGLE_CODE         0xa221   /* STD_MEASURED_ANGLE (polar) */
#define PP_UNIT_I_CODE        0xa148   /* STD_op_i */
#define PP_UNIT_J_CODE        0xa149   /* STD_op_j */
#define PP_IS_PRODUCT(code)   ((code) == 0x80b7 /*STD_DOT*/ || (code) == 0x80d7 /*STD_CROSS*/)
#define PP_IS_SPACE(code)     (((code) >= 0xa000 && (code) <= 0xa00f) || (code) == ' ')
#define PP_IS_CONST_NAME(code) ((code) == PP_PI_CODE || (code) == PP_E_CODE || (code) == PP_PHI_CODE)

static uint16_t ppGlyphAt(const char *s, int16_t pos, int16_t *next) {
  uint16_t code = (uint8_t)s[pos];
  if(code >= 0x80) {
    code = (uint16_t)((code << 8) | (uint8_t)s[pos + 1]);
    *next = pos + 2;
  }
  else {
    *next = pos + 1;
  }
  return code;
}

/* Map a digit-run span to plain digits: sup/sub digits become '0'+d, any
 * other glyph (a configured group separator) is kept verbatim. */
static bool_t ppMapDigits(const char *s, int16_t from, int16_t to, bool_t sup,
                          char *out, uint16_t outSize) {
  uint16_t o = 0;
  int16_t pos = from, next;
  while(pos < to) {
    uint16_t code = ppGlyphAt(s, pos, &next);
    if(o + 2 >= outSize) {
      return false;
    }
    if(sup ? PP_IS_SUP_DIGIT(code) : PP_IS_SUB_DIGIT(code)) {
      out[o++] = (char)('0' + (code & 0x000F));
    }
    else {
      while(pos < next) {
        out[o++] = s[pos++];
      }
    }
    pos = next;
  }
  out[o] = 0;
  return true;
}


/* ==== fraction form: head + sup-num '/' sub-den ========================= */

bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  int16_t len = (int16_t)strlen(src);
  int16_t slashOff = -1, numStart = -1, supMinusOff = -1;
  bool_t  subSeen = false;

  int16_t pos = 0, next;
  while(pos < len) {
    if((uint8_t)src[pos] >= 0x80 && pos + 1 >= len) {
      return false;   // torn 2-byte glyph
    }
    uint16_t code = ppGlyphAt(src, pos, &next);
    if(code == '/') {
      if(slashOff >= 0 || numStart < 0) {
        return false;
      }
      slashOff = pos;
    }
    else if(PP_IS_SUP_DIGIT(code)) {
      if(slashOff >= 0) {
        return false;
      }
      if(numStart < 0) {
        numStart = pos;
      }
    }
    else if(code == PP_SUP_MINUS_CODE) {
      if(numStart >= 0 || slashOff >= 0 || supMinusOff >= 0) {
        return false;
      }
      supMinusOff = pos;
    }
    else if(PP_IS_SUB_DIGIT(code)) {
      if(slashOff < 0) {
        return false;
      }
      subSeen = true;
    }
    pos = next;
  }
  if(slashOff < 0 || numStart < 0 || !subSeen) {
    return false;
  }
  if(supMinusOff >= 0 && supMinusOff + 2 != numStart) {
    return false;   // the builder writes the sup-minus glyph flush against the numerator
  }

  int16_t headLen = (supMinusOff >= 0) ? supMinusOff : numStart;

  char mapped[64];
  uint8_t root = ppNewBox(PP_HBOX, ctxFont);
  if(root == PP_NONE) {
    return false;
  }
  if(headLen > 0) {
    uint8_t head = ppNewRun(src, (uint16_t)headLen, ctxFont);
    if(head == PP_NONE) {
      return false;
    }
    ppAppendChild(root, head);
  }
  if(supMinusOff >= 0) {
    uint8_t minus = ppNewRun("-", 1, ctxFont);
    if(minus == PP_NONE) {
      return false;
    }
    ppAppendChild(root, minus);
  }

  uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
  if(frac == PP_NONE) {
    return false;
  }
  if(!ppMapDigits(src, numStart, slashOff, true, mapped, sizeof(mapped))) {
    return false;
  }
  uint8_t num = ppNewRun(mapped, (uint16_t)strlen(mapped), childFont);
  if(!ppMapDigits(src, slashOff + 1, len, false, mapped, sizeof(mapped))) {
    return false;
  }
  uint8_t den = ppNewRun(mapped, (uint16_t)strlen(mapped), childFont);
  if(num == PP_NONE || den == PP_NONE) {
    return false;
  }
  ppAppendChild(frac, num);
  ppAppendChild(frac, den);
  ppAppendChild(root, frac);

  *rootOut = root;
  return true;
}


/* ==== exponent form: mantissa ·₁₀ⁿ -> mantissa·10 with raised n ========= */

bool_t ppParseExponent(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  int16_t len = (int16_t)strlen(src);
  int16_t markOff = -1, expOff = -1, expEnd = -1;
  bool_t expDigitSeen = false;

  int16_t pos = 0, next;
  while(pos < len) {
    if((uint8_t)src[pos] >= 0x80 && pos + 1 >= len) {
      return false;
    }
    uint16_t code = ppGlyphAt(src, pos, &next);
    if(markOff < 0) {
      if(PP_IS_PRODUCT(code) && next + 1 < len) {
        int16_t after;
        if(ppGlyphAt(src, next, &after) == PP_SUB10_CODE) {
          markOff = pos;
          expOff  = after;
          pos = after;
          continue;
        }
      }
      if(PP_IS_SUP_DIGIT(code) || PP_IS_SUB_DIGIT(code) || code == PP_SUP_MINUS_CODE
          || code == PP_RAD_CODE || PP_IS_CONST_NAME(code)) {
        return false;   // not a plain mantissa: some other form
      }
    }
    else {
      if(PP_IS_SUP_DIGIT(code)) {
        if(expEnd >= 0) {
          return false;   // a digit after the terminator is not this shape
        }
        expDigitSeen = true;
      }
      else if(code == PP_SUP_MINUS_CODE && !expDigitSeen) {
        // leading exponent sign, fine
      }
      /* Every exponent string the builder produces ends with a hair
       * space (supNumberToDisplayString). Tolerate a trailing space run
       * and remember where it starts: the digit extraction below stops
       * before it. */
      else if(PP_IS_SPACE(code) && expDigitSeen) {
        if(expEnd < 0) {
          expEnd = pos;
        }
      }
      else {
        return false;   // second marker or stray glyph after the exponent
      }
    }
    pos = next;
  }
  if(markOff < 0 || !expDigitSeen) {
    return false;
  }
  if(expEnd < 0) {
    expEnd = len;
  }

  int16_t baseLen = markOff + 2;
  if(baseLen + 3 > (int16_t)sizeof(ppSpanA)) {
    return false;
  }
  /* Read the exponent BEFORE writing ppSpanA: ppParseComplex passes its
   * own ppSpanA in as src, and the base rebuild below lands '1','0','\0'
   * exactly on expOff. */
  // exponent digits, sup -> plain ('⁻' -> '-'). The copy stops before
  // the trailing space run.
  char expd[24];
  uint16_t o = 0;
  pos = expOff;
  while(pos < expEnd && (size_t)(o + 1) < sizeof(expd)) {
    uint16_t code = ppGlyphAt(src, pos, &next);
    expd[o++] = (code == PP_SUP_MINUS_CODE) ? '-' : (char)('0' + (code & 0x000F));
    pos = next;
  }
  expd[o] = 0;

  // base = mantissa + the original product glyph + plain "10"
  xcopy(ppSpanA, src, baseLen);
  ppSpanA[baseLen]     = '1';
  ppSpanA[baseLen + 1] = '0';
  ppSpanA[baseLen + 2] = 0;

  uint8_t sup = ppNewBox(PP_SUP, ctxFont);
  uint8_t base = ppNewRun(ppSpanA, (uint16_t)strlen(ppSpanA), ctxFont);
  uint8_t exp = ppNewRun(expd, (uint16_t)strlen(expd), childFont);
  if(sup == PP_NONE || base == PP_NONE || exp == PP_NONE) {
    return false;
  }
  ppAppendChild(sup, base);
  ppAppendChild(sup, exp);
  *rootOut = sup;
  return true;
}


/* ==== IRFRAC symbolic form ==============================================
 * Template over checkForAndChange's common output:
 *   [spaces] [sign] [multiple: digits·× | digits | sup-digits] name
 *   [/ denominator: sub-digits|digits] [spaces]
 * with name = √(sub-digits|π) or π|e|φ. Anything else (mixed-number
 * constant forms, the (π²)-family, second constants) declines to
 * upstream's linear rendering. */

bool_t ppParseIrfrac(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  int16_t len = (int16_t)strlen(src);
  int16_t pos = 0, next;

  // token spans, discovered in one forward pass
  int16_t signOff = -1;            bool_t signIsSup = false;
  int16_t multStart = -1, multEnd = -1;  bool_t multIsSup = false;
  int16_t prodOff = -1;
  int16_t nameOff = -1;            uint16_t nameCode = 0;
  int16_t radArgStart = -1, radArgEnd = -1;  bool_t radArgIsPi = false;
  int16_t denStart = -1, denEnd = -1;        bool_t denIsSub = false;
  bool_t slashSeen = false;

  enum { S_LEAD, S_AFTER_SIGN, S_AFTER_MULT, S_AFTER_NAME, S_AFTER_SLASH, S_TAIL } st = S_LEAD;

  while(pos < len) {
    if((uint8_t)src[pos] >= 0x80 && pos + 1 >= len) {
      return false;
    }
    uint16_t code = ppGlyphAt(src, pos, &next);

    if(PP_IS_SPACE(code)) {
      if(st == S_AFTER_SLASH || (st == S_AFTER_NAME && denStart >= 0)) {
        return false;
      }
      if(st == S_AFTER_NAME || st == S_TAIL) {
        st = S_TAIL;
      }
      pos = next;
      continue;
    }
    if(st == S_TAIL) {
      return false;   // content after trailing spaces
    }

    switch(st) {
      case S_LEAD:
        if(code == '+' || code == '-') {
          signOff = pos;
          st = S_AFTER_SIGN;
        }
        else if(code == PP_SUP_MINUS_CODE) {
          signOff = pos;
          signIsSup = true;
          st = S_AFTER_SIGN;
        }
        else {
          st = S_AFTER_SIGN;
          continue;   // re-dispatch this glyph below
        }
        break;

      case S_AFTER_SIGN:
        if(code >= '0' && code <= '9') {
          if(multStart < 0) { multStart = pos; }
          multEnd = next;
        }
        else if(PP_IS_SUP_DIGIT(code)) {
          if(multStart >= 0 && !multIsSup) {
            return false;
          }
          if(multStart < 0) { multStart = pos; multIsSup = true; }
          multEnd = next;
        }
        else if(PP_IS_PRODUCT(code)) {
          if(multStart < 0 || multIsSup || prodOff >= 0) {
            return false;
          }
          prodOff = pos;
        }
        else if(code == PP_RAD_CODE || PP_IS_CONST_NAME(code)) {
          nameOff = pos;
          nameCode = code;
          st = S_AFTER_NAME;
          if(code == PP_RAD_CODE) {
            // radicand: sub-digit run or π, immediately following
            if(next >= len) {
              return false;
            }
            int16_t p2 = next, n2;
            uint16_t c2 = ppGlyphAt(src, p2, &n2);
            if(c2 == PP_PI_CODE) {
              radArgStart = p2;
              radArgEnd = n2;
              radArgIsPi = true;
              next = n2;
            }
            else if(PP_IS_SUB_DIGIT(c2)) {
              radArgStart = p2;
              while(p2 < len && PP_IS_SUB_DIGIT(ppGlyphAt(src, p2, &n2))) {
                radArgEnd = n2;
                p2 = n2;
              }
              next = radArgEnd;
            }
            else {
              return false;
            }
          }
        }
        else {
          return false;
        }
        break;

      case S_AFTER_NAME:
        if(code == '/') {
          if(slashSeen) {
            return false;
          }
          slashSeen = true;
          st = S_AFTER_SLASH;
        }
        else {
          return false;
        }
        break;

      case S_AFTER_SLASH:
        if(PP_IS_SUB_DIGIT(code)) {
          if(denStart >= 0 && !denIsSub) {
            return false;
          }
          if(denStart < 0) { denStart = pos; denIsSub = true; }
          denEnd = next;
          st = S_AFTER_SLASH;
        }
        else if(code >= '0' && code <= '9') {
          if(denStart >= 0 && denIsSub) {
            return false;
          }
          if(denStart < 0) { denStart = pos; }
          denEnd = next;
        }
        else {
          return false;
        }
        // once at least one den glyph is in, further glyphs keep
        // appending. A space or the end closes the form.
        if(denStart >= 0) {
          st = S_AFTER_SLASH;
        }
        break;

      default:
        return false;
    }
    pos = next;
  }

  if(nameOff < 0) {
    return false;
  }
  if(slashSeen && denStart < 0) {
    return false;
  }
  // a bare constant name renders the same upstream: decline
  if(!slashSeen && nameCode != PP_RAD_CODE && !multIsSup) {
    return false;
  }

  bool_t hasDen = (denStart >= 0);
  uint8_t innerFont = hasDen ? childFont : ctxFont;
  char mapped[32];

  uint8_t root = ppNewBox(PP_HBOX, ctxFont);
  if(root == PP_NONE) {
    return false;
  }
  if(signOff >= 0) {
    uint8_t sign = ppNewRun((signIsSup || src[signOff] == '-') ? "-" : "+", 1, ctxFont);
    if(sign == PP_NONE) {
      return false;
    }
    ppAppendChild(root, sign);
  }

  // numerator content: [digits ×?] name
  uint8_t numBox = ppNewBox(PP_HBOX, innerFont);
  if(numBox == PP_NONE) {
    return false;
  }
  if(multStart >= 0) {
    uint16_t o = 0;
    int16_t p = multStart, n;
    while(p < multEnd && (size_t)(o + 1) < sizeof(mapped)) {
      uint16_t c = ppGlyphAt(src, p, &n);
      mapped[o++] = PP_IS_SUP_DIGIT(c) ? (char)('0' + (c & 0x000F)) : src[p];
      p = n;
    }
    if(prodOff >= 0 && (size_t)(o + 2) < sizeof(mapped)) {
      mapped[o++] = src[prodOff];
      mapped[o++] = src[prodOff + 1];
    }
    mapped[o] = 0;
    uint8_t mult = ppNewRun(mapped, o, innerFont);
    if(mult == PP_NONE) {
      return false;
    }
    ppAppendChild(numBox, mult);
  }
  if(nameCode == PP_RAD_CODE) {
    uint8_t rad = ppNewBox(PP_RAD, innerFont);
    uint8_t arg;
    if(radArgIsPi) {
      arg = ppNewRun(src + radArgStart, (uint16_t)(radArgEnd - radArgStart), innerFont);
    }
    else {
      if(!ppMapDigits(src, radArgStart, radArgEnd, false, mapped, sizeof(mapped))) {
        return false;
      }
      arg = ppNewRun(mapped, (uint16_t)strlen(mapped), innerFont);
    }
    if(rad == PP_NONE || arg == PP_NONE) {
      return false;
    }
    ppAppendChild(rad, arg);
    ppAppendChild(numBox, rad);
  }
  else {
    uint8_t name = ppNewRun(src + nameOff, 2, innerFont);
    if(name == PP_NONE) {
      return false;
    }
    ppAppendChild(numBox, name);
  }

  if(hasDen) {
    uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
    if(frac == PP_NONE) {
      return false;
    }
    if(!ppMapDigits(src, denStart, denEnd, false, mapped, sizeof(mapped))) {
      return false;
    }
    uint8_t den = ppNewRun(mapped, (uint16_t)strlen(mapped), childFont);
    if(den == PP_NONE) {
      return false;
    }
    ppAppendChild(frac, numBox);
    ppAppendChild(frac, den);
    ppAppendChild(root, frac);
  }
  else {
    ppAppendChild(root, numBox);
  }

  *rootOut = root;
  return true;
}


bool_t ppParseRealAny(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  // The third alternative catches IRFRAC's pure-fraction output (constant
  // = 1): the same sup-num '/' sub-den alphabet the FRACT builder emits.
  return ppParseExponent(src, ctxFont, childFont, rootOut)
      || ppParseIrfrac(src, ctxFont, childFont, rootOut)
      || ppParseFraction(src, ctxFont, childFont, rootOut);
}


/* ==== complex (rectangular only) ========================================
 * Assembly (complex34ToDisplayString2): re ± [i·im | im␣␣i]. The first
 * top-level plain sign after position 0 separates the parts. Polar forms
 * (∠) decline. Each part re-parses through ppParseRealAny, and the whole
 * is pretty only if at least one part is. */

bool_t ppParseComplex(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  int16_t len = (int16_t)strlen(src);
  int16_t pos = 0, next;
  int16_t sepOff = -1;

  while(pos < len) {
    if((uint8_t)src[pos] >= 0x80 && pos + 1 >= len) {
      return false;
    }
    uint16_t code = ppGlyphAt(src, pos, &next);
    if(code == PP_ANGLE_CODE) {
      return false;   // polar stays linear
    }
    if(sepOff < 0 && pos > 0 && (code == '+' || code == '-')) {
      sepOff = pos;
    }
    pos = next;
  }
  if(sepOff < 0) {
    return false;
  }

  // shape A: sign i · im      shape B: sign im ␣ ␣ i
  int16_t afterSep;
  uint16_t sepNext = ppGlyphAt(src, sepOff, &afterSep);
  (void)sepNext;
  int16_t imStart, imEnd, midEnd, tailStart = -1;
  if(afterSep < len) {
    int16_t n2;
    uint16_t c2 = ppGlyphAt(src, afterSep, &n2);
    if(c2 == PP_UNIT_I_CODE || c2 == PP_UNIT_J_CODE) {
      midEnd = n2;
      if(n2 < len) {
        int16_t n3;
        if(PP_IS_PRODUCT(ppGlyphAt(src, n2, &n3))) {
          midEnd = n3;
        }
      }
      imStart = midEnd;
      imEnd = len;
    }
    else {
      // shape B: trailing [spaces] unit
      int16_t p = len;
      // walk back one glyph: find the unit at the very end
      // (2-byte glyphs: scan forward recording glyph starts)
      int16_t last = -1, prev = -1, scan = 0, nx;
      while(scan < len) {
        prev = last;
        last = scan;
        ppGlyphAt(src, scan, &nx);
        scan = nx;
      }
      if(last < 0) {
        return false;
      }
      uint16_t lastCode = ppGlyphAt(src, last, &nx);
      if(lastCode != PP_UNIT_I_CODE && lastCode != PP_UNIT_J_CODE) {
        return false;
      }
      // strip spaces before the unit
      p = last;
      while(prev >= 0) {
        int16_t n4;
        uint16_t c4 = ppGlyphAt(src, prev, &n4);
        if(!PP_IS_SPACE(c4) || prev <= sepOff) {
          break;
        }
        p = prev;
        // find the glyph before prev
        int16_t s2 = 0, l2 = -1, pr2 = -1, nx2;
        while(s2 < prev) {
          pr2 = l2;
          l2 = s2;
          ppGlyphAt(src, s2, &nx2);
          s2 = nx2;
        }
        prev = l2 == prev ? pr2 : l2;
      }
      imStart = afterSep;
      imEnd = p;
      midEnd = afterSep;
      tailStart = p;
    }
  }
  else {
    return false;
  }
  if(imEnd <= imStart || sepOff <= 0) {
    return false;
  }
  if(imEnd - imStart >= (int16_t)sizeof(ppSpanB) || sepOff >= (int16_t)sizeof(ppSpanA)) {
    return false;
  }

  xcopy(ppSpanA, src, sepOff);
  ppSpanA[sepOff] = 0;
  xcopy(ppSpanB, src + imStart, imEnd - imStart);
  ppSpanB[imEnd - imStart] = 0;

  uint8_t root = ppNewBox(PP_HBOX, ctxFont);
  if(root == PP_NONE) {
    return false;
  }
  uint8_t reNode, imNode;
  bool_t rePretty = ppParseRealAny(ppSpanA, ctxFont, childFont, &reNode);
  if(!rePretty) {
    reNode = ppNewRun(ppSpanA, (uint16_t)strlen(ppSpanA), ctxFont);
  }
  ppAppendChild(root, reNode);

  uint8_t mid = ppNewRun(src + sepOff, (uint16_t)(midEnd - sepOff), ctxFont);
  if(mid == PP_NONE || reNode == PP_NONE) {
    return false;
  }
  ppAppendChild(root, mid);

  bool_t imPretty = ppParseRealAny(ppSpanB, ctxFont, childFont, &imNode);
  if(!imPretty) {
    imNode = ppNewRun(ppSpanB, (uint16_t)strlen(ppSpanB), ctxFont);
  }
  if(imNode == PP_NONE) {
    return false;
  }
  ppAppendChild(root, imNode);

  if(tailStart >= 0) {
    uint8_t tail = ppNewRun(src + tailStart, (uint16_t)(len - tailStart), ctxFont);
    if(tail == PP_NONE) {
      return false;
    }
    ppAppendChild(root, tail);
  }

  if(!rePretty && !imPretty) {
    return false;   // nothing gained: upstream renders identically
  }
  *rootOut = root;
  return true;
}


/* ==== the surfaces ====================================================== */

// Builds the pretty tree for a register at one font rung. Runs the same
// upstream builder as the matching _refreshRegisterLine arm, with
// identical arguments (builder-first invariant).
static bool_t ppBuildRegister(calcRegister_t regist, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  uint32_t dt = getRegisterDataType(regist);

  if(dt == dtReal34 && getSystemFlag(FLAG_FRACT)
      && (   real34CompareAbsLessThan(REGISTER_REAL34_DATA(regist), const34_1e6)
          || real34IsZero(REGISTER_REAL34_DATA(regist)))) {
    fractionToDisplayString(regist, ppScratch);
    return ppParseFraction(ppScratch, ctxFont, childFont, rootOut);
  }

  if(dt == dtReal34) {
    if(getRegisterAngularMode(regist) != amNone) {
      return false;   // angle forms carry a separate suffix-glyph pass
    }
    real34ToDisplayString(REGISTER_REAL34_DATA(regist), getRegisterAngularMode(regist),
                          ppScratch, &numericFont, SCREEN_WIDTH, NUMBER_OF_DISPLAY_DIGITS,
                          LIMITEXP, FRONTSPACE, FULLIRFRAC);
    return ppParseRealAny(ppScratch, ctxFont, childFont, rootOut);
  }

  if(dt == dtComplex34) {
    if(getComplexRegisterPolarMode(regist) == amPolar) {
      return false;
    }
    complex34ToDisplayString(REGISTER_COMPLEX34_DATA(regist), ppScratch, &numericFont,
                             SCREEN_WIDTH, NUMBER_OF_DISPLAY_DIGITS, LIMITEXP, FRONTSPACE,
                             FULLIRFRAC, getComplexRegisterAngularMode(regist),
                             getComplexRegisterPolarMode(regist) == amPolar);
    return ppParseComplex(ppScratch, ctxFont, childFont, rootOut);
  }

  return false;
}

static const uint8_t ppInlineRungs[3][2] = {
  { PP_FONT_NUMERIC,  PP_FONT_STANDARD },
  { PP_FONT_STANDARD, PP_FONT_STANDARD },
  { PP_FONT_STANDARD, PP_FONT_TINY     },
};

static const uint8_t ppFullRungs[4][2] = {
  { PP_FONT_NUMERIC,  PP_FONT_NUMERIC  },
  { PP_FONT_NUMERIC,  PP_FONT_STANDARD },
  { PP_FONT_STANDARD, PP_FONT_STANDARD },
  { PP_FONT_STANDARD, PP_FONT_TINY     },
};

bool_t prettyTryRegisterLine(calcRegister_t regist, int16_t baseY, int16_t *lineWidth) {
  if(!getSystemFlag(FLAG_PRETTYP)
      || calcMode != CM_NORMAL
      || temporaryInformation != TI_NO_INFO
      || lastErrorCode != 0
      || checkHP                                      // HP layout doubles glyph rows, and the metrics assume it off
      || getSystemFlag(FLAG_SOLVING)
      || getSystemFlag(FLAG_INTING)
      || currentInputVariable != INVALID_VARIABLE) {
    return false;
  }

  // Non-X lines must not paint below baseY+31: the next line's clear
  // band starts at its own baseY-4 = this baseY+32 and erases the rows.
  int16_t bandTop    = baseY - 4;
  int16_t bandBottom = baseY + ((regist == REGISTER_X) ? 38 : 31);

  // T-line live formula (opt-in, pretty-print-extra): while a formula
  // is open, the T line shows the formula. An absent extra package, no
  // formula or no fit falls through to the ordinary value rendering
  // below.
  if(regist == REGISTER_T && ppTlineExtension != NULL
      && (*ppTlineExtension)(baseY, bandTop, bandBottom, lineWidth)) {
    return true;
  }

  for(int r = 0; r < 3; r++) {
    uint8_t root;
    ppReset();
    if(!ppBuildRegister(regist, ppInlineRungs[r][0], ppInlineRungs[r][1], &root)) {
      return false;
    }
    if(ppRenderRightAligned(root, SCREEN_WIDTH, bandTop, bandBottom, ppPreferredBase(baseY))) {
      *lineWidth = ppNodeAt(root)->width;
      return true;
    }
  }
  return false;
}

/* PSHOW (ITM_PSHOW, item row 459): full-screen pretty view of X on the
 * fnPixel manual-paint protocol: pixels survive refreshes, and the next
 * keypress releases them. Anything the engine cannot pretty falls back
 * to the ordinary SHOW, so the user always gets a result. */
void fnPrettyShow(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  // Order matters: an error takes precedence and returns silently.
  // Then checkHP falls back to the ordinary SHOW, because HP layout
  // doubles glyph rows and the engine's metrics assume it off.
  if(lastErrorCode != ERROR_NONE) {
    return;
  }
  if(checkHP) {
    fnC47Show(NOPARAM);   // the ordinary SHOW, which HP layout handles
    return;
  }

  for(int r = 0; r < 4; r++) {
    uint8_t root;
    ppReset();
    if(!ppBuildRegister(REGISTER_X, ppFullRungs[r][0], ppFullRungs[r][1], &root)) {
      break;
    }
    if(!ppMeasure(root, 0)) {
      break;
    }
    const ppNode_t *n = ppNodeAt(root);
    // interior band: rows 21..167 between the frame lines
    if(n->width > SCREEN_WIDTH - 4 || n->ascent + n->descent > 167 - 21 + 1) {
      continue;   // next rung
    }
    int16_t x = (SCREEN_WIDTH - n->width) / 2;
    int16_t baseline = (21 + 167 - (n->ascent + n->descent)) / 2 + n->ascent;

    lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
    drawSinglePixelFullWidthLine(20);
    drawSinglePixelFullWidthLine(168);
    ppPaintAt(root, x, baseline);

    screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
    screenHoldsDrawnPixels = true;
    // A self-painted screen must declare itself one, as upstream's
    // matrix SHOW does. Without TI_SHOWNOTHING, EXIT falls through to
    // the menu arm and leaves the pixels up.
    temporaryInformation = TI_SHOWNOTHING;
    return;
  }

  fnC47Show(NOPARAM);   // fallback: the user always gets a SHOW
}
