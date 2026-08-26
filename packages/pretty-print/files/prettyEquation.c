// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyEquation.c
 * EQN strip 2D rendering: a strict recursive-descent parser over
 * showEquation's DISPLAY string (superscript exponents are already
 * glyphs there) that stacks '/' terms as tiny fractions and puts
 * vinculums over √, inside the one 23 px softmenu-strip row the
 * equation owns. Anything the grammar does not fully recognize — an
 * ellipsis-truncated string, an unknown glyph, a dangling operator —
 * declines, and upstream's linear rendering runs unchanged. Never
 * active while the equation is being edited (the cursor path).
 *
 * Grammar over glyph classes:
 *   equation := expr ('=' expr)?
 *   expr     := ['+'|'-'] term (('+'|'-') term)*
 *   term     := factor (('×'|'·'|'/') factor)*    '/' -> stacked FRAC
 *   factor   := primary [sup-digit run]
 *   primary  := number | name | '(' expr ')' | '√' primary
 * Parenthesized factors unwrap under a fraction bar or a vinculum —
 * the bar scopes, textbook style.
 */

#include "c47.h"
#include "prettyInternal.h"

typedef struct {
  const char *s;
  int16_t pos, len;
  bool_t fracSeen;   // pretty-worthiness: no FRAC/RAD -> decline
  bool_t failed;
} ppqCtx_t;

#define PPQ_IS_SUP(code)  (((code) >= 0xa160 && (code) <= 0xa169) || (code) == 0xa16b)
#define PPQ_IS_SUB(code)  ((code) >= 0xa080 && (code) <= 0xa089)
#define PPQ_IS_PROD(code) ((code) == 0x80b7 || (code) == 0x80d7)
#define PPQ_IS_SPACE(code) (((code) >= 0xa000 && (code) <= 0xa00f) || (code) == ' ')
#define PPQ_SUB10 0xa47d
#define PPQ_RAD   0xa21a

static uint16_t ppqPeek(ppqCtx_t *c, int16_t *next) {
  if(c->pos >= c->len) {
    *next = c->pos;
    return 0;
  }
  uint16_t code = (uint8_t)c->s[c->pos];
  if(code >= 0x80) {
    if(c->pos + 1 >= c->len) {
      c->failed = true;
      *next = c->pos;
      return 0;
    }
    code = (uint16_t)((code << 8) | (uint8_t)c->s[c->pos + 1]);
    *next = c->pos + 2;
  }
  else {
    *next = c->pos + 1;
  }
  return code;
}

static void ppqSkipSpace(ppqCtx_t *c) {
  int16_t next;
  while(c->pos < c->len && PPQ_IS_SPACE(ppqPeek(c, &next))) {
    c->pos = next;
  }
}

static uint8_t ppqExpr(ppqCtx_t *c, uint8_t font, uint8_t tinyF);

// number: digits/'.'/group separators, optionally ·₁₀ + sup exponent —
// copied verbatim (the glyphs already render right in a run)
static uint8_t ppqNumber(ppqCtx_t *c, uint8_t font) {
  int16_t start = c->pos, next;
  bool_t any = false;
  while(c->pos < c->len) {
    uint16_t code = ppqPeek(c, &next);
    if((code >= '0' && code <= '9') || code == '.') {
      any = true;
      c->pos = next;
    }
    else if(any && PPQ_IS_PROD(code)) {
      // possible ·₁₀ⁿ tail: only if the next glyph is ₁₀
      int16_t n2;
      ppqCtx_t probe = *c;
      probe.pos = next;
      if(ppqPeek(&probe, &n2) == PPQ_SUB10) {
        c->pos = n2;
        while(c->pos < c->len && PPQ_IS_SUP(ppqPeek(c, &next))) {
          c->pos = next;
        }
        break;
      }
      break;
    }
    else {
      break;
    }
  }
  if(!any) {
    return PP_NONE;
  }
  return ppNewRun(c->s + start, (uint16_t)(c->pos - start), font);
}

// name: ASCII letters plus subscript digits (X₁ etc.)
static uint8_t ppqName(ppqCtx_t *c, uint8_t font) {
  int16_t start = c->pos, next;
  bool_t any = false;
  while(c->pos < c->len) {
    uint16_t code = ppqPeek(c, &next);
    if((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z') || (any && PPQ_IS_SUB(code))) {
      any = true;
      c->pos = next;
    }
    else {
      break;
    }
  }
  if(!any) {
    return PP_NONE;
  }
  return ppNewRun(c->s + start, (uint16_t)(c->pos - start), font);
}

static uint8_t ppqUnwrapParen(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd != NULL && nd->kind == PP_PAREN) {
    return nd->firstChild;   // the bar/vinculum scopes: drop the parens
  }
  return n;
}

static uint8_t ppqPrimary(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  int16_t next;
  ppqSkipSpace(c);
  uint16_t code = ppqPeek(c, &next);

  if(code == '(') {
    c->pos = next;
    uint8_t inner = ppqExpr(c, font, tinyF);
    ppqSkipSpace(c);
    if(c->failed || inner == PP_NONE || ppqPeek(c, &next) != ')') {
      c->failed = true;
      return PP_NONE;
    }
    c->pos = next;
    uint8_t p = ppNewBox(PP_PAREN, font);
    if(p == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(p, inner);
    return p;
  }
  if(code == PPQ_RAD) {
    c->pos = next;
    uint8_t arg = ppqPrimary(c, font, tinyF);
    if(c->failed || arg == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    uint8_t rad = ppNewBox(PP_RAD, font);
    if(rad == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(rad, ppqUnwrapParen(arg));
    c->fracSeen = true;
    return rad;
  }
  uint8_t n = ppqNumber(c, font);
  if(n != PP_NONE) {
    return n;
  }
  n = ppqName(c, font);
  if(n != PP_NONE) {
    return n;
  }
  c->failed = true;
  return PP_NONE;
}

static uint8_t ppqFactor(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  uint8_t n = ppqPrimary(c, font, tinyF);
  if(c->failed || n == PP_NONE) {
    return PP_NONE;
  }
  // attach an already-superscript exponent run verbatim
  int16_t start = c->pos, next;
  while(c->pos < c->len && PPQ_IS_SUP(ppqPeek(c, &next))) {
    c->pos = next;
  }
  if(c->pos > start) {
    uint8_t box = ppNewBox(PP_HBOX, font);
    uint8_t sup = ppNewRun(c->s + start, (uint16_t)(c->pos - start), font);
    if(box == PP_NONE || sup == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(box, n);
    ppAppendChild(box, sup);
    return box;
  }
  return n;
}

static uint8_t ppqTerm(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  uint8_t n = ppqFactor(c, font, tinyF);
  while(!c->failed && n != PP_NONE) {
    int16_t next;
    ppqSkipSpace(c);
    uint16_t code = ppqPeek(c, &next);
    if(code == '/') {
      c->pos = next;
      uint8_t den = ppqFactor(c, font, tinyF);
      if(c->failed || den == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      uint8_t frac = ppNewBox(PP_FRAC, font);
      if(frac == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      // children re-font to tiny so the stack fits the 23 px strip row;
      // parens unwrap under the bar (it scopes, textbook style)
      uint8_t num = ppqUnwrapParen(n);
      uint8_t dn = ppqUnwrapParen(den);
      ppSetFontDeep(num, tinyF);
      ppSetFontDeep(dn, tinyF);
      ppAppendChild(frac, num);
      ppAppendChild(frac, dn);
      c->fracSeen = true;
      n = frac;
    }
    else if(PPQ_IS_PROD(code)) {
      c->pos = next;
      uint8_t rhs = ppqFactor(c, font, tinyF);
      if(c->failed || rhs == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      uint8_t box = ppNewBox(PP_HBOX, font);
      uint8_t opRun = ppNewRun((code == 0x80d7) ? "\x80\xd7" : "\x80\xb7", 2, font);
      if(box == PP_NONE || opRun == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      ppAppendChild(box, n);
      ppAppendChild(box, opRun);
      ppAppendChild(box, rhs);
      n = box;
    }
    else {
      break;
    }
  }
  return n;
}

static uint8_t ppqExpr(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  ppqSkipSpace(c);
  uint8_t box = PP_NONE;
  int16_t next;

  // optional leading sign
  uint16_t code = ppqPeek(c, &next);
  uint8_t lead = PP_NONE;
  if(code == '+' || code == '-') {
    lead = ppNewRun(c->s + c->pos, 1, font);
    c->pos = next;
  }

  uint8_t n = ppqTerm(c, font, tinyF);
  if(c->failed || n == PP_NONE) {
    c->failed = true;
    return PP_NONE;
  }
  if(lead != PP_NONE) {
    box = ppNewBox(PP_HBOX, font);
    if(box == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(box, lead);
    ppAppendChild(box, n);
    n = box;
  }

  while(!c->failed) {
    ppqSkipSpace(c);
    code = ppqPeek(c, &next);
    if(code != '+' && code != '-') {
      break;
    }
    uint8_t op = ppNewRun(c->s + c->pos, 1, font);
    c->pos = next;
    uint8_t rhs = ppqTerm(c, font, tinyF);
    if(c->failed || rhs == PP_NONE || op == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    uint8_t b2 = ppNewBox(PP_HBOX, font);
    if(b2 == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(b2, n);
    ppAppendChild(b2, op);
    ppAppendChild(b2, rhs);
    n = b2;
  }
  return n;
}

bool_t ppqParse(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  ppqCtx_t c;
  c.s = src;
  c.pos = 0;
  c.len = (int16_t)strlen(src);
  c.fracSeen = false;
  c.failed = false;

  uint8_t n = ppqExpr(&c, ctxFont, childFont);
  if(c.failed || n == PP_NONE) {
    return false;
  }
  ppqSkipSpace(&c);
  int16_t next;
  uint16_t code = ppqPeek(&c, &next);
  if(code == '=') {
    uint8_t eq = ppNewRun("=", 1, ctxFont);
    c.pos = next;
    uint8_t rhs = ppqExpr(&c, ctxFont, childFont);
    ppqSkipSpace(&c);
    if(c.failed || rhs == PP_NONE || eq == PP_NONE || c.pos != c.len) {
      return false;
    }
    uint8_t box = ppNewBox(PP_HBOX, ctxFont);
    if(box == PP_NONE) {
      return false;
    }
    ppAppendChild(box, n);
    ppAppendChild(box, eq);
    ppAppendChild(box, rhs);
    n = box;
  }
  else if(c.pos != c.len) {
    return false;   // trailing content the grammar did not consume
  }
  if(!c.fracSeen) {
    return false;   // nothing 2D gained: upstream's linear line is identical
  }
  *rootOut = n;
  return true;
}

/* The hook (solver/equation.c paint site): try the 2D form in the
 * equation's own strip row; false -> upstream's showString runs. */
bool_t prettyTryEquation(const char *src, int16_t xLeft) {
  if(!prettyEnabled()) {
    return false;
  }
  uint8_t root;
  ppReset();
  if(!ppqParse(src, PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
    return false;
  }
  if(!ppMeasure(root, 0)) {
    return false;
  }
  const ppNode_t *n = ppNodeAt(root);
  // the equation's strip row: rows 171..193 (SOFTMENU_HEIGHT band)
  if(n->width > SCREEN_WIDTH - 2 - xLeft || n->ascent + n->descent > 23) {
    return false;
  }
  int16_t base = 171 + n->ascent + (23 - (n->ascent + n->descent)) / 2;
  ppPaintAt(root, xLeft, base);
  return true;
}


/* ==== EQSHW — full-screen equation view (PP7) ===========================
 * Room the 23 px strip lacks: nested fractions render at full size in
 * the 21..167 band. In the interactive integrate solver the equation is
 * the integrand, framed by a stroke-drawn big ∫ (PP_INT) — the equation
 * LANGUAGE has no Σ/∏/∫ constructs (verified against the parser's
 * function aliases), so this solver mode is the one honest input a big
 * operator has; a Σ template would be dead code and was skipped. */

bool_t ppqShowRender(const char *src) {
  lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
  drawSinglePixelFullWidthLine(20);
  drawSinglePixelFullWidthLine(168);

  uint8_t root;
  bool_t pretty = false;
  ppReset();
  if(ppqParse(src, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    if((currentSolverStatus & SOLVER_STATUS_EQUATION_MODE) == SOLVER_STATUS_EQUATION_INTEGRATE) {
      uint8_t big = ppNewBox(PP_INT, PP_FONT_STANDARD);
      if(big != PP_NONE) {
        ppAppendChild(big, root);
        root = big;
      }
    }
    if(ppMeasure(root, 0)) {
      const ppNode_t *n = ppNodeAt(root);
      if(n->width <= SCREEN_WIDTH - 4 && n->ascent + n->descent <= 167 - 21 + 1) {
        int16_t base = (int16_t)((21 + 167 - (n->ascent + n->descent)) / 2 + n->ascent);
        ppPaintAt(root, (int16_t)((SCREEN_WIDTH - n->width) / 2), base);
        pretty = true;
      }
    }
  }
  if(!pretty) {
    // always show SOMETHING: the linear line, centered-ish
    int16_t w = stringWidth(src, &standardFont, false, true);
    int16_t x = (w < SCREEN_WIDTH - 4) ? (int16_t)((SCREEN_WIDTH - w) / 2) : 2;
    showString(src, &standardFont, x, 94 - 8, vmNormal, false, true);
  }

  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
  return pretty;
}

void fnPrettyEqShow(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  if(lastErrorCode != ERROR_NONE) {
    return;
  }
  if(numberOfFormulae == 0 || currentFormula >= numberOfFormulae) {
    return;   // nothing stored to show
  }
  bool_t cursorShown, rightEllipsis;
  showEquation(currentFormula, 0, EQUATION_NO_CURSOR, true, &cursorShown, &rightEllipsis);
  ppqShowRender(tmpString);   // dryRun above filled tmpString (read-only here)
}
