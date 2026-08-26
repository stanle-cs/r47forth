// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyValue.c
 * Register value -> layout tree, and the inline stack-line surface.
 *
 * Builder-first invariant (DESIGN.md §2): the upstream display builder runs
 * first and its OUTPUT is parsed into the tree. The pretty form can never
 * disagree with what upstream would have shown, and side effects
 * (displayValueX) happen identically on both paths.
 */

#include "c47.h"
#include "prettyInternal.h"

static char ppScratch[200];

static bool_t ppActive = true;

void fnPrettyToggle(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppActive = !ppActive;
}

bool_t prettyEnabled(void) {
  return ppActive;
}

void prettySetEnabled(bool_t on) {
  ppActive = on;
}


/* fractionToDisplayString's output alphabet (display.c:1684): an optional
 * head (comparison prefix, sign, plain-digit integer part, separators,
 * punctuation space), then sup-digit numerator, '/', sub-digit denominator;
 * group separators may sit inside both digit runs. The improper-case sign
 * is STD_SUP_MINUS immediately before the numerator. */
#define PP_IS_SUP_DIGIT(code) ((code) >= 0xa160 && (code) <= 0xa169)
#define PP_IS_SUB_DIGIT(code) ((code) >= 0xa080 && (code) <= 0xa089)
#define PP_SUP_MINUS_CODE     0xa16b

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


bool_t prettyTryRegisterLine(calcRegister_t regist, int16_t baseY, int16_t *lineWidth) {
  if(!ppActive
      || calcMode != CM_NORMAL
      || temporaryInformation != TI_NO_INFO
      || lastErrorCode != 0
      || checkHP                                      // HP layout doubles glyph rows; our metrics assume it off
      || getSystemFlag(FLAG_SOLVING)
      || getSystemFlag(FLAG_INTING)
      || currentInputVariable != INVALID_VARIABLE) {
    return false;
  }
  // Type/range parity with the upstream FLAG_FRACT arm (screen.c): only
  // what that arm would render as a fraction is ours to re-typeset.
  if(!getSystemFlag(FLAG_FRACT)
      || getRegisterDataType(regist) != dtReal34
      || !(   real34CompareAbsLessThan(REGISTER_REAL34_DATA(regist), const34_1e6)
           || real34IsZero(REGISTER_REAL34_DATA(regist)))) {
    return false;
  }

  fractionToDisplayString(regist, ppScratch);   // same builder, same displayValueX side effect as upstream

  // Non-X lines may not paint below baseY+31: the next line's clear band
  // starts at its own baseY-4 = this baseY+32 and would erase the rows.
  int16_t bandTop    = baseY - 4;
  int16_t bandBottom = baseY + ((regist == REGISTER_X) ? 38 : 31);

  static const uint8_t rung[3][2] = {
    { PP_FONT_NUMERIC,  PP_FONT_STANDARD },
    { PP_FONT_STANDARD, PP_FONT_STANDARD },
    { PP_FONT_STANDARD, PP_FONT_TINY     },
  };
  for(int r = 0; r < 3; r++) {
    uint8_t root;
    ppReset();
    if(!ppParseFraction(ppScratch, rung[r][0], rung[r][1], &root)) {
      return false;
    }
    if(ppRenderRightAligned(root, SCREEN_WIDTH, bandTop, bandBottom, ppPreferredBase(baseY))) {
      *lineWidth = ppNodeAt(root)->width;
      return true;
    }
  }
  return false;
}
