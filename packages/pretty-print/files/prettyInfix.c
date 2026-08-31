// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyInfix.c
 * The shared 2D infix builders: the layout for one operator from
 * already-built child layouts and their precedences, the bracket
 * rules, and the display-time name decodes. Every producer of a 2D
 * form calls through here — the equation renderer and the program
 * walker in this package, and the capture viewer in
 * pretty-print-extra — so precedence is decided in one place.
 */

#include "c47.h"
#include "prettyInternal.h"

/* ==== infix construction ================================================
 * Precedence: ADD/SUB 1, MULT 2, FRAC/SUP/RAD/atoms 3 (visually
 * scoped). ppfBuildOp* build the layout for one operator from
 * already-built child layouts and their precedences, shared by the tree
 * walker and the token decoder so both paths typeset identically. */

static uint8_t ppfParen(uint8_t inner, uint8_t fontId) {
  uint8_t p = ppNewBox(PP_PAREN, fontId);
  if(p == PP_NONE || inner == PP_NONE) {
    return PP_NONE;
  }
  ppAppendChild(p, inner);
  return p;
}

uint8_t ppfWrapIf(uint8_t node, int prec, int minPrec, uint8_t fontId) {
  if(node == PP_NONE) {
    return PP_NONE;
  }
  return (prec < minPrec) ? ppfParen(node, fontId) : node;
}

uint8_t ppfRun(const char *s, uint8_t fontId) {
  return ppNewRun(s, (uint16_t)strlen(s), fontId);
}

/* Is this run text a visual atom: something a raised exponent or a
 * neighboring operator can sit against without brackets? Only digits —
 * including the uppercase hex digits A..F and the wide-spelling binary
 * glyphs — the radix mark, the digit-group spaces and a base subscript
 * are. A based integer is one numeral in every base and every spelling
 * (ruled, PP18RR8-6; domain completed, PP18RR9-2). Anything else reads
 * as a term, so the leaf reports PPF_PREC_ADD and the bracket rules
 * handle it. Lowercase letters stay rejected: the integer builder
 * emits uppercase only, and a typed exponent's 'e' must read as a
 * term. */
bool_t ppfTextIsAtom(const char *s, uint16_t len) {
  if(s == NULL) {
    return true;
  }
  for(uint16_t i = 0; i < len && s[i] != 0; ) {
    uint8_t c = (uint8_t)s[i];
    if(c < 0x80) {
      if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
           || c == '.' || c == ',' || c == ' ')) {
        return false;
      }
      i++;
    }
    else {
      if(i + 1 >= len || s[i + 1] == 0) {
        return false;   // a truncated glyph is not an atom
      }
      uint16_t code = (uint16_t)(((uint16_t)c << 8) | (uint8_t)s[i + 1]);
      if(!((code >= 0xa000 && code <= 0xa00f)          // digit-group spaces
           || (code >= 0xa461 && code <= 0xa46f)       // base subscripts, bases 2..16
           || code == 0xa20e || code == 0xa027)) {     // wide base-2 digit glyphs
        return false;
      }
      i = (uint16_t)(i + 2);
    }
  }
  return true;
}

/* The precedence of a built run, for producers that do not know what
 * they formatted. Only a run that spells a number is judged: a name is
 * an atom whatever its glyphs. */
int ppfRunPrec(uint8_t n) {
  const ppNode_t *nd = (n != PP_NONE) ? ppNodeAt(n) : NULL;
  if(nd == NULL || nd->kind != PP_RUN) {
    return PPF_PREC_ATOM;
  }
  const char *t = ppTextAt(nd->textOff);
  if(t == NULL || !((t[0] >= '0' && t[0] <= '9')
                    || t[0] == '.' || t[0] == ',' || t[0] == '-')) {
    return PPF_PREC_ATOM;
  }
  return ppfTextIsAtom(t, (uint16_t)strlen(t)) ? PPF_PREC_ATOM : PPF_PREC_ADD;
}

/* Brackets a power's base. A base that is itself a PP_SUP needs parens,
 * and ppfWrapIf cannot see that (both report ATOM). Every producer of
 * a PP_SUP must call this. */
uint8_t ppfPowBase(uint8_t a, int aPrec, uint8_t fontId) {
  const ppNode_t *nd = (a != PP_NONE) ? ppNodeAt(a) : NULL;
  return (nd != NULL && nd->kind == PP_SUP)
           ? ppfParen(a, fontId)
           : ppfWrapIf(a, aPrec, PPF_PREC_ATOM, fontId);
}

uint8_t ppfBuildOp2(uint16_t item, uint8_t a, int aPrec, uint8_t b, int bPrec,
                           uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  switch(item) {
    case ITM_DIV: {
      uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
      if(frac == PP_NONE || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(frac, a);   // FRAC scopes its children: no parens
      ppAppendChild(frac, b);
      return frac;
    }
    case ITM_YX: {
      uint8_t sup = ppNewBox(PP_SUP, ctxFont);
      uint8_t base = ppfPowBase(a, aPrec, ctxFont);   // (2+3)² keeps its parens
      if(sup == PP_NONE || base == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sup, base);
      ppAppendChild(sup, b);
      return sup;
    }
    case ITM_XTHROOT: {
      // ˣ√y: index = X (right operand b), radicand = Y (left operand a)
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      uint8_t idx = b;
      if(rad == PP_NONE || a == PP_NONE || idx == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);
      ppAppendChild(rad, idx);
      return rad;
    }
    case ITM_LOGXY: {
      // LOGₓy: base = X (right operand b), argument = Y (left operand a)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t sub = ppNewBox(PP_SUB, ctxFont);
      uint8_t name = ppfRun("log", ctxFont);
      if(box == PP_NONE || sub == PP_NONE || name == PP_NONE
          || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      // ppfParen can fail, and ppMeasure has no arity check for the
      // variadic PP_HBOX, so test it here.
      uint8_t par = ppfParen(a, ctxFont);
      if(par == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sub, name);
      ppAppendChild(sub, b);
      ppAppendChild(box, sub);
      ppAppendChild(box, par);
      return box;
    }
    case ITM_ADD: case ITM_SUB: case ITM_MULT: {
      int myPrec = (item == ITM_MULT) ? PPF_PREC_MUL : PPF_PREC_ADD;
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t l = ppfWrapIf(a, aPrec, myPrec, ctxFont);
      // right side of SUB needs parens at equal precedence: a-(b+c)
      uint8_t r = ppfWrapIf(b, bPrec, (item == ITM_SUB) ? myPrec + 1 : myPrec, ctxFont);
      // multiplication typesets as the raised dot (matches the EQN view)
      uint8_t op = ppfRun(item == ITM_MULT ? STD_DOT : indexOfItems[item].itemCatalogName, ctxFont);
      if(box == PP_NONE || l == PP_NONE || r == PP_NONE || op == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, l);
      ppAppendChild(box, op);
      ppAppendChild(box, r);
      *outPrec = myPrec;
      return box;
    }
    default: {
      // function form: name(a, b)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t name = ppfRun(indexOfItems[item].itemCatalogName, ctxFont);
      uint8_t inner = ppNewBox(PP_HBOX, ctxFont);
      uint8_t comma = ppfRun(", ", childFont);
      if(box == PP_NONE || name == PP_NONE || inner == PP_NONE || comma == PP_NONE
          || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      // same shape: ppfParen can fail, so test it here
      uint8_t par = ppfParen(inner, ctxFont);
      if(par == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(inner, a);
      ppAppendChild(inner, comma);
      ppAppendChild(inner, b);
      ppAppendChild(box, name);
      ppAppendChild(box, par);
      return box;
    }
  }
}

uint8_t ppfBuildOp1(uint16_t item, uint8_t a, int aPrec,
                           uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  switch(item) {
    case ITM_SQUAREROOTX: {
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      if(rad == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);   // the vinculum scopes: no parens
      return rad;
    }
    case ITM_CUBEROOT: {
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      uint8_t idx = ppfRun("3", childFont);
      if(rad == PP_NONE || a == PP_NONE || idx == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);
      ppAppendChild(rad, idx);
      return rad;
    }
    case ITM_ABS: case ITM_MAGNITUDE: {
      uint8_t bars = ppNewBox(PP_BARS, ctxFont);
      if(bars == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(bars, a);
      return bars;
    }
    case ITM_1ONX: {
      uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
      uint8_t one = ppfRun("1", childFont);
      if(frac == PP_NONE || one == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(frac, one);
      ppAppendChild(frac, a);
      return frac;
    }
    case ITM_SQUARE: case ITM_CUBE: {
      uint8_t sup = ppNewBox(PP_SUP, ctxFont);
      uint8_t base = ppfPowBase(a, aPrec, ctxFont);
      uint8_t exp = ppfRun(item == ITM_SQUARE ? "2" : "3", childFont);
      if(sup == PP_NONE || base == PP_NONE || exp == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sup, base);
      ppAppendChild(sup, exp);
      return sup;
    }
    case ITM_CHS: {
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t minus = ppfRun("-", ctxFont);
      uint8_t inner = ppfWrapIf(a, aPrec, PPF_PREC_MUL, ctxFont);
      if(box == PP_NONE || minus == PP_NONE || inner == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, minus);
      ppAppendChild(box, inner);
      *outPrec = PPF_PREC_ADD;
      return box;
    }
    default: {
      // function form: name(a)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t name = ppfRun(indexOfItems[item].itemCatalogName, ctxFont);
      uint8_t inner = ppfParen(a, ctxFont);
      if(box == PP_NONE || name == PP_NONE || inner == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, name);
      ppAppendChild(box, inner);
      return box;
    }
  }
}


/* ==== big operators =====================================================
 * A captured Sigma_n/Pi_n/integral-YX dispatch. The label param decodes
 * through labelList (findNamedLabel returns index + FIRST_LABEL). The
 * lookup is display-time best-effort: a program edit between capture
 * and display falls back to the numeric form. */

void ppfVariableName(uint16_t varId, char *out) {
  strcpy(out, "x");
  if(varId >= FIRST_NAMED_VARIABLE
      && (uint32_t)(varId - FIRST_NAMED_VARIABLE) < numberOfNamedVariables) {
    const uint8_t *vn = allNamedVariables[varId - FIRST_NAMED_VARIABLE].variableName;
    if(vn[0] > 0 && vn[0] <= 15) {
      xcopy(out, vn + 1, vn[0]);
      out[vn[0]] = 0;
      // the canonical variable X typesets as the classic lowercase x
      // (the closest form the fonts have to the italic convention).
      // Every other name keeps its own letters.
      if(out[0] == 'X' && out[1] == 0) {
        out[0] = 'x';
      }
    }
  }
}

void ppfLabelName(uint16_t param, char *out) {
  if(param >= FIRST_LABEL && (uint32_t)(param - FIRST_LABEL) < numberOfLabels) {
    const uint8_t *p = labelList[param - FIRST_LABEL].labelPointer;
    if(p != NULL && (*(p - 1) == STRING_LABEL_VARIABLE || *(p - 1) == LOCAL_LABEL_VARIABLE)) {
      uint8_t len = *p;
      if(len > 0 && len <= 15) {
        xcopy(out, p + 1, len);
        out[len] = 0;
        return;
      }
    }
  }
  snprintf(out, 17, "LBL %u", (unsigned)param);
}

