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

static uint8_t ppqRun(const char *s, uint8_t fontId) {
  return ppNewRun(s, (uint16_t)strlen(s), fontId);
}

// PP14: the equation-language big operators render as their 2D shapes.
// A probe that does not match consumes nothing; a matched construct that
// is malformed fails the whole parse (strict declines).
static bool_t ppqMatchName(ppqCtx_t *c, const char *name, int16_t *after) {
  int16_t l = (int16_t)strlen(name);
  if(c->pos + l >= c->len) {
    return false;
  }
  if(strncmp(c->s + c->pos, name, (size_t)l) != 0 || c->s[c->pos + l] != '(') {
    return false;
  }
  *after = (int16_t)(c->pos + l + 1);
  return true;
}

static bool_t ppqEat(ppqCtx_t *c, char ch) {
  int16_t next;
  ppqSkipSpace(c);
  if(ppqPeek(c, &next) != (uint16_t)ch) {
    c->failed = true;
    return false;
  }
  c->pos = next;
  return true;
}

static uint8_t ppqBigopConstruct(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  int16_t after;
  uint16_t tag;
  uint8_t kind;   // 0 sum, 1 prod, 2 deriv, 3 integ
  if(ppqMatchName(c, "SUM", &after))        { kind = 0; tag = ITM_SIGMAn; }
  else if(ppqMatchName(c, "PROD", &after))  { kind = 1; tag = ITM_PIn; }
  else if(ppqMatchName(c, "DERIV", &after)) { kind = 2; tag = 0; }
  else if(ppqMatchName(c, "INTEG", &after)) { kind = 3; tag = ITM_INTEGRAL_YX; }
  else {
    return PP_NONE;
  }
  c->pos = after;

  uint8_t body = ppqExpr(c, font, tinyF);
  if(c->failed || body == PP_NONE || !ppqEat(c, ';')) {
    c->failed = true;
    return PP_NONE;
  }
  ppqSkipSpace(c);
  int16_t varStart = c->pos;
  uint8_t varRun = ppqName(c, tinyF);
  int16_t varEnd = c->pos;
  if(c->failed || varRun == PP_NONE || !ppqEat(c, ';')) {
    c->failed = true;
    return PP_NONE;
  }
  uint8_t fromN = ppqExpr(c, tinyF, tinyF);
  if(c->failed || fromN == PP_NONE) {
    c->failed = true;
    return PP_NONE;
  }
  uint8_t toN = PP_NONE, stepN = PP_NONE;
  if(kind != 2) {
    if(!ppqEat(c, ';')) {
      return PP_NONE;
    }
    toN = ppqExpr(c, tinyF, tinyF);
    if(c->failed || toN == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
  }
  {
    int16_t next;
    ppqSkipSpace(c);
    if(ppqPeek(c, &next) == ';') {
      c->pos = next;
      stepN = ppqExpr(c, tinyF, tinyF);   // step (sums) / order (deriv)
      if(c->failed || stepN == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
    }
  }
  if(!ppqEat(c, ')')) {
    return PP_NONE;
  }

  c->fracSeen = true;
  if(kind == 0 || kind == 1) {
    uint8_t big = ppNewBox(PP_BIGOP, font);
    uint8_t under = ppNewBox(PP_HBOX, tinyF);
    uint8_t eqRun = ppqRun("=", tinyF);
    if(big == PP_NONE || under == PP_NONE || eqRun == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppSetBoxTag(big, tag);
    ppSetFontDeep(fromN, tinyF);
    ppSetFontDeep(toN, tinyF);
    ppAppendChild(under, varRun);
    ppAppendChild(under, eqRun);
    ppAppendChild(under, fromN);
    if(stepN != PP_NONE) {
      // a non-unit step is part of the range: show it
      uint8_t dRun = ppqRun("," STD_DELTA, tinyF);
      if(dRun == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      ppSetFontDeep(stepN, tinyF);
      ppAppendChild(under, dRun);
      ppAppendChild(under, stepN);
    }
    ppAppendChild(big, body);
    ppAppendChild(big, under);
    ppAppendChild(big, toN);
    return big;
  }
  if(kind == 3) {
    uint8_t big = ppNewBox(PP_BIGOP, font);
    uint8_t hb = ppNewBox(PP_HBOX, font);
    uint8_t dRun = ppqRun(" d", font);
    uint8_t varRun2 = ppNewRun(c->s + varStart, (uint16_t)(varEnd - varStart), font);
    if(big == PP_NONE || hb == PP_NONE || dRun == PP_NONE || varRun2 == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppSetBoxTag(big, tag);
    ppSetFontDeep(fromN, tinyF);
    ppSetFontDeep(toN, tinyF);
    ppAppendChild(hb, body);
    ppAppendChild(hb, dRun);
    ppAppendChild(hb, varRun2);
    ppAppendChild(big, hb);
    ppAppendChild(big, fromN);
    ppAppendChild(big, toN);
    return big;
  }
  // DERIV: d/dvar (body) with var=at as a subscript suffix; an order-2
  // argument raises the superscript-2 glyphs
  {
    bool_t second = false;
    if(stepN != PP_NONE) {
      const ppNode_t *sn = ppNodeAt(stepN);
      second = (sn != NULL && sn->kind == PP_RUN && strcmp(ppTextAt(sn->textOff), "2") == 0);
      if(!second) {
        const ppNode_t *sn1 = ppNodeAt(stepN);
        if(sn1 == NULL || sn1->kind != PP_RUN || strcmp(ppTextAt(sn1->textOff), "1") != 0) {
          c->failed = true;   // only literal order 1 or 2 renders
          return PP_NONE;
        }
      }
    }
    uint8_t hb = ppNewBox(PP_HBOX, font);
    uint8_t frac = ppNewBox(PP_FRAC, font);
    uint8_t num = ppqRun(second ? "d" "\xa1\x62" : "d", font);
    uint8_t denBox = ppNewBox(PP_HBOX, font);
    uint8_t dRun = ppqRun("d", font);
    uint8_t varRun2 = ppNewRun(c->s + varStart, (uint16_t)(varEnd - varStart), font);
    uint8_t par = ppNewBox(PP_PAREN, font);
    uint8_t sub = ppNewBox(PP_SUB, font);
    uint8_t script = ppNewBox(PP_HBOX, tinyF);
    uint8_t eqRun = ppqRun("=", tinyF);
    if(hb == PP_NONE || frac == PP_NONE || num == PP_NONE || denBox == PP_NONE
        || dRun == PP_NONE || varRun2 == PP_NONE || par == PP_NONE
        || sub == PP_NONE || script == PP_NONE || eqRun == PP_NONE) {
      c->failed = true;
      return PP_NONE;
    }
    ppAppendChild(denBox, dRun);
    ppAppendChild(denBox, varRun2);
    if(second) {
      uint8_t s2 = ppqRun("\xa1\x62", font);
      if(s2 == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      ppAppendChild(denBox, s2);
    }
    ppAppendChild(frac, num);
    ppAppendChild(frac, denBox);
    ppAppendChild(par, body);
    ppSetFontDeep(varRun, tinyF);
    ppSetFontDeep(fromN, tinyF);
    ppAppendChild(script, varRun);
    ppAppendChild(script, eqRun);
    ppAppendChild(script, fromN);
    ppAppendChild(sub, par);
    ppAppendChild(sub, script);
    ppAppendChild(hb, frac);
    ppAppendChild(hb, sub);
    return hb;
  }
}

static uint8_t ppqPrimary(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  int16_t next;
  ppqSkipSpace(c);
  uint16_t code = ppqPeek(c, &next);

  if(code >= 'A' && code <= 'Z') {
    uint8_t big = ppqBigopConstruct(c, font, tinyF);
    if(c->failed) {
      return PP_NONE;
    }
    if(big != PP_NONE) {
      return big;
    }
  }

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

/* PP13: the interactive solver's own numbers frame the equation. The
 * integral shows its REAL limits (RESERVED_VARIABLE_ULIM/LLIM) and the
 * d-variable when the session is interactive; the derivative modes get
 * the d/dx (d²/dx²) prefix. Solve framing (f(x)=0) is SKIPPED:
 * SOLVER_STATUS_EQUATION_SOLVER is the zero value, indistinguishable
 * from no session at all — a stale INTERACTIVE bit would frame a plain
 * view with an = 0 the user never asked for. */

uint8_t ppqFrameIntegral(uint8_t eq) {
  if(eq == PP_NONE) {
    return PP_NONE;
  }
  if((currentSolverStatus & SOLVER_STATUS_INTERACTIVE)
      && getRegisterDataType(RESERVED_VARIABLE_LLIM) == dtReal34
      && getRegisterDataType(RESERVED_VARIABLE_ULIM) == dtReal34) {
    char dv[20], lo[48], hi[48], dtext[24];
    ppfVariableName(currentSolverVariable, dv);
    real34ToDisplayString(REGISTER_REAL34_DATA(RESERVED_VARIABLE_LLIM), amNone, lo,
                          &standardFont, 110, 6, LIMITEXP, !FRONTSPACE, NOIRFRAC);
    real34ToDisplayString(REGISTER_REAL34_DATA(RESERVED_VARIABLE_ULIM), amNone, hi,
                          &standardFont, 110, 6, LIMITEXP, !FRONTSPACE, NOIRFRAC);
    sprintf(dtext, " d%s", dv);
    uint8_t big   = ppNewBox(PP_BIGOP, PP_FONT_STANDARD);
    uint8_t body  = ppNewBox(PP_HBOX, PP_FONT_STANDARD);
    uint8_t dRun  = ppNewRun(dtext, (uint16_t)strlen(dtext), PP_FONT_STANDARD);
    uint8_t under = ppNewRun(lo, (uint16_t)strlen(lo), PP_FONT_TINY);
    uint8_t over  = ppNewRun(hi, (uint16_t)strlen(hi), PP_FONT_TINY);
    if(big != PP_NONE && body != PP_NONE && dRun != PP_NONE
        && under != PP_NONE && over != PP_NONE) {
      ppSetBoxTag(big, ITM_INTEGRAL_YX);
      ppAppendChild(body, eq);
      ppAppendChild(body, dRun);
      ppAppendChild(big, body);
      ppAppendChild(big, under);
      ppAppendChild(big, over);
      return big;
    }
  }
  // no live limits to show: the bare stroke ∫, as PP7 shipped it
  uint8_t bare = ppNewBox(PP_INT, PP_FONT_STANDARD);
  if(bare != PP_NONE) {
    ppAppendChild(bare, eq);
    return bare;
  }
  return eq;
}

uint8_t ppqFrameDerivative(uint8_t eq, bool_t second) {
  if(eq == PP_NONE) {
    return PP_NONE;
  }
  char dv[20], num[8], den[28];
  ppfVariableName(currentSolverVariable, dv);
  // 0xa162 is the superscript-2 glyph
  strcpy(num, second ? "d" "\xa1\x62" : "d");
  sprintf(den, second ? "d%s" "\xa1\x62" : "d%s", dv);
  uint8_t hb   = ppNewBox(PP_HBOX, PP_FONT_STANDARD);
  uint8_t frac = ppNewBox(PP_FRAC, PP_FONT_STANDARD);
  uint8_t nRun = ppNewRun(num, (uint16_t)strlen(num), PP_FONT_STANDARD);
  uint8_t dRun = ppNewRun(den, (uint16_t)strlen(den), PP_FONT_STANDARD);
  uint8_t par  = ppNewBox(PP_PAREN, PP_FONT_STANDARD);
  if(hb == PP_NONE || frac == PP_NONE || nRun == PP_NONE
      || dRun == PP_NONE || par == PP_NONE) {
    return eq;
  }
  ppAppendChild(frac, nRun);
  ppAppendChild(frac, dRun);
  ppAppendChild(par, eq);
  ppAppendChild(hb, frac);
  ppAppendChild(hb, par);
  return hb;
}

bool_t ppqShowRender(const char *src) {
  lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
  drawSinglePixelFullWidthLine(20);
  drawSinglePixelFullWidthLine(168);

  uint8_t root;
  bool_t pretty = false;
  ppReset();
  if(ppqParse(src, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    uint16_t eqMode = (uint16_t)(currentSolverStatus & SOLVER_STATUS_EQUATION_MODE);
    if(eqMode == SOLVER_STATUS_EQUATION_INTEGRATE) {
      root = ppqFrameIntegral(root);
    }
    else if(eqMode == SOLVER_STATUS_EQUATION_1ST_DERIVATIVE
         || eqMode == SOLVER_STATUS_EQUATION_2ND_DERIVATIVE) {
      root = ppqFrameDerivative(root, eqMode == SOLVER_STATUS_EQUATION_2ND_DERIVATIVE);
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
