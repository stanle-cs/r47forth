// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyEquation.c
 * EQN strip 2D rendering: a strict recursive-descent parser over
 * showEquation's display string. It stacks '/' terms as fractions and
 * puts vinculums over √, inside the 23 px softmenu-strip row. Anything
 * the grammar does not fully recognize declines, and upstream's linear
 * rendering runs unchanged. Never active while the equation is edited.
 *
 * Grammar over glyph classes:
 *   equation := expr ('=' expr)?
 *   expr     := ['+'|'-'] term (('+'|'-') term)*
 *   term     := factor (('×'|'·'|'/') factor)*    '/' -> stacked FRAC
 *   factor   := primary [sup-digit run]
 *   primary  := number | name | '(' expr ')' | '√' primary
 * Parenthesized factors unwrap under a fraction bar or a vinculum: the
 * bar scopes, textbook style.
 */

#include "c47.h"
#include "prettyInternal.h"

typedef struct {
  const char *s;
  int16_t pos, len;
  bool_t fracSeen;   // set when the parse gains a 2D form. No gain -> decline
  bool_t failed;
} ppqCtx_t;

// sup digits plus STD_SUP_PLUS (0xa16a) and STD_SUP_MINUS (0xa16b),
// which _showExponent emits
#define PPQ_IS_SUP(code)  (((code) >= 0xa160 && (code) <= 0xa169) \
                           || (code) == 0xa16a || (code) == 0xa16b)
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

// number: digits/'.'/group separators, optionally ·₁₀ + sup exponent,
// copied verbatim
static uint8_t ppqNumber(ppqCtx_t *c, uint8_t font) {
  int16_t start = c->pos, next;
  bool_t any = false;
  while(c->pos < c->len) {
    uint16_t code = ppqPeek(c, &next);
    /* The comma is a radix mark here: upstream rewrites every ',' in
     * a numeric token to '.' before stringToReal34. This grammar's
     * separators are ';' and ')'. */
    if((code >= '0' && code <= '9') || code == '.' || code == ',') {
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

// name: ASCII letters plus subscript digits (X₁ etc.). The canonical
// variable X typesets as lowercase x. Other names keep their letters.
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
  if(c->pos - start == 1 && c->s[start] == 'X') {
    char lc = 'x';
    return ppNewRun(&lc, 1, font);
  }
  return ppNewRun(c->s + start, (uint16_t)(c->pos - start), font);
}

/* A big operator used as an operand needs brackets: its body extends
 * right of the stroke, so a neighbor binds into it. This parser has no
 * precedence values, so the node kind decides. */
static uint8_t ppqScopeOperand(ppqCtx_t *c, uint8_t n, uint8_t font) {
  const ppNode_t *nd = ppNodeAt(n);
  if(n == PP_NONE || nd == NULL || nd->kind != PP_BIGOP) {
    return n;
  }
  uint8_t p = ppNewBox(PP_PAREN, font);
  if(p == PP_NONE) {
    c->failed = true;
    return PP_NONE;
  }
  ppAppendChild(p, n);
  return p;
}

uint8_t ppqUnwrapParen(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd != NULL && nd->kind == PP_PAREN) {
    return nd->firstChild;   // the bar/vinculum scopes: drop the parens
  }
  return n;
}

static uint8_t ppqRun(const char *s, uint8_t fontId) {
  return ppNewRun(s, (uint16_t)strlen(s), fontId);
}

// An additive body under a big operator needs parens (PROD 1+x reads
// as (PROD 1)+x): wrap when the top level carries a +/- joiner.
static uint8_t ppqScopeBody(ppqCtx_t *c, uint8_t body, uint8_t font) {
  const ppNode_t *b = ppNodeAt(body);
  if(b == NULL || b->kind != PP_HBOX) {
    return body;
  }
  for(uint8_t ch = b->firstChild; ch != PP_NONE; ch = ppNodeAt(ch)->nextSibling) {
    const ppNode_t *cn = ppNodeAt(ch);
    if(cn->kind == PP_RUN) {
      const char *t = ppTextAt(cn->textOff);
      if(strchr(t, '+') != NULL || strchr(t, '-') != NULL) {
        uint8_t p = ppNewBox(PP_PAREN, font);
        if(p == PP_NONE) {
          c->failed = true;
          return PP_NONE;
        }
        ppAppendChild(p, body);
        return p;
      }
    }
  }
  return body;
}

/* The node-assembly half of a construct, shared with the program
 * walker. Takes already-built children and touches no parser state.
 *
 * Test every allocation before any append: ppAppendChild silently
 * no-ops on PP_NONE, and ppMeasure cannot arity-check the variadic
 * PP_HBOX.
 *
 * `body` arrives already scoped by the caller.
 *
 * varTiny / varCtx: SUM and PROD use only the tiny run, INTEG only the
 * context-font one, DERIV both. Pass PP_NONE for the unused one. */
uint8_t ppqBuildBigop(uint8_t kind, uint16_t tag, uint8_t body,
                      uint8_t varTiny, uint8_t varCtx,
                      uint8_t fromN, uint8_t toN, uint8_t stepN,
                      bool_t secondOrder, uint8_t ctxFont) {
  if(body == PP_NONE || fromN == PP_NONE) {
    return PP_NONE;
  }
  // limits and scripts always typeset tiny
  if(kind == PPQ_BIG_SUM || kind == PPQ_BIG_PROD) {
    uint8_t big   = ppNewBox(PP_BIGOP, ctxFont);
    uint8_t under = ppNewBox(PP_HBOX, PP_FONT_TINY);
    uint8_t eqRun = ppqRun("=", PP_FONT_TINY);
    if(big == PP_NONE || under == PP_NONE || eqRun == PP_NONE
        || varTiny == PP_NONE || toN == PP_NONE) {
      return PP_NONE;
    }
    ppSetBoxTag(big, tag);
    ppSetFontDeep(varTiny, PP_FONT_TINY);
    ppSetFontDeep(fromN, PP_FONT_TINY);
    ppSetFontDeep(toN, PP_FONT_TINY);
    ppAppendChild(under, varTiny);
    ppAppendChild(under, eqRun);
    ppAppendChild(under, fromN);
    if(stepN != PP_NONE) {
      // a non-unit step is part of the range: show it
      uint8_t dRun = ppqRun("," STD_DELTA, PP_FONT_TINY);
      if(dRun == PP_NONE) {
        return PP_NONE;
      }
      ppSetFontDeep(stepN, PP_FONT_TINY);
      ppAppendChild(under, dRun);
      ppAppendChild(under, stepN);
    }
    ppAppendChild(big, body);
    ppAppendChild(big, under);
    ppAppendChild(big, toN);
    return big;
  }
  if(kind == PPQ_BIG_INTEG) {
    uint8_t big  = ppNewBox(PP_BIGOP, ctxFont);
    uint8_t hb   = ppNewBox(PP_HBOX, ctxFont);
    uint8_t dRun = ppqRun(" d", ctxFont);
    if(big == PP_NONE || hb == PP_NONE || dRun == PP_NONE
        || varCtx == PP_NONE || toN == PP_NONE) {
      return PP_NONE;
    }
    ppSetBoxTag(big, tag);
    ppSetFontDeep(fromN, PP_FONT_TINY);
    ppSetFontDeep(toN, PP_FONT_TINY);
    ppAppendChild(hb, body);
    ppAppendChild(hb, dRun);
    ppAppendChild(hb, varCtx);
    ppAppendChild(big, hb);
    ppAppendChild(big, fromN);
    ppAppendChild(big, toN);
    return big;
  }
  // DERIV: d/dvar (body) with var=at as a subscript suffix. 0xa162 is
  // the superscript-2 glyph.
  {
    uint8_t hb     = ppNewBox(PP_HBOX, ctxFont);
    uint8_t frac   = ppNewBox(PP_FRAC, ctxFont);
    uint8_t num    = ppqRun(secondOrder ? "d" "\xa1\x62" : "d", ctxFont);
    uint8_t denBox = ppNewBox(PP_HBOX, ctxFont);
    uint8_t dRun   = ppqRun("d", ctxFont);
    uint8_t par    = ppNewBox(PP_PAREN, ctxFont);
    uint8_t sub    = ppNewBox(PP_SUB, ctxFont);
    uint8_t script = ppNewBox(PP_HBOX, PP_FONT_TINY);
    uint8_t eqRun  = ppqRun("=", PP_FONT_TINY);
    if(hb == PP_NONE || frac == PP_NONE || num == PP_NONE || denBox == PP_NONE
        || dRun == PP_NONE || varCtx == PP_NONE || par == PP_NONE
        || sub == PP_NONE || script == PP_NONE || eqRun == PP_NONE
        || varTiny == PP_NONE) {
      return PP_NONE;
    }
    ppAppendChild(denBox, dRun);
    ppAppendChild(denBox, varCtx);
    if(secondOrder) {
      uint8_t s2 = ppqRun("\xa1\x62", ctxFont);
      if(s2 == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(denBox, s2);
    }
    ppAppendChild(frac, num);
    ppAppendChild(frac, denBox);
    ppAppendChild(par, body);
    ppSetFontDeep(varTiny, PP_FONT_TINY);
    ppSetFontDeep(fromN, PP_FONT_TINY);
    ppAppendChild(script, varTiny);
    ppAppendChild(script, eqRun);
    ppAppendChild(script, fromN);
    ppAppendChild(sub, par);
    ppAppendChild(sub, script);
    ppAppendChild(hb, frac);
    ppAppendChild(hb, sub);
    return hb;
  }
}

// The equation-language big operators render as their 2D shapes. A
// probe that does not match consumes nothing. A matched construct that
// is malformed fails the whole parse.
static bool_t ppqMatchName(ppqCtx_t *c, const char *name, int16_t *after) {
  int16_t l = (int16_t)strlen(name);
  if(c->pos + l >= c->len) {
    return false;
  }
  // both spellings, per ppEqConstructIs: the renderer must accept
  // exactly what the evaluator accepts
  if(!ppEqConstructIs(c->s + c->pos, name, (uint8_t)l)) {
    return false;
  }
  *after = (int16_t)(c->pos + l + 1);
  return true;
}

// the raw-slice twin of ppqName's X-to-x rule, for the d<var> runs
static uint8_t ppqVarRun(ppqCtx_t *c, int16_t start, int16_t end, uint8_t font) {
  if(end - start == 1 && c->s[start] == 'X') {
    char lc = 'x';
    return ppNewRun(&lc, 1, font);
  }
  return ppNewRun(c->s + start, (uint16_t)(end - start), font);
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

  // only a literal order of 1 or 2 renders: the builder takes a plain
  // flag
  bool_t second = false;
  if(kind == 2 && stepN != PP_NONE) {
    const ppNode_t *sn = ppNodeAt(stepN);
    second = (sn != NULL && sn->kind == PP_RUN && strcmp(ppTextAt(sn->textOff), "2") == 0);
    if(!second && (sn == NULL || sn->kind != PP_RUN
                   || strcmp(ppTextAt(sn->textOff), "1") != 0)) {
      c->failed = true;
      return PP_NONE;
    }
    stepN = PP_NONE;   // consumed into `second`
  }
  // the parser cannot see precedence, so it scopes by sniffing the body's
  // own runs for a +/- joiner
  if(kind != 2) {
    body = ppqScopeBody(c, body, font);
    if(body == PP_NONE) {
      return PP_NONE;
    }
  }
  {
    uint8_t varCtx = (kind == 2 || kind == 3)
                       ? ppqVarRun(c, varStart, varEnd, font) : PP_NONE;
    uint8_t root = ppqBuildBigop((uint8_t)kind, tag, body,
                                 (kind == 3) ? PP_NONE : varRun, varCtx,
                                 fromN, toN, stepN, second, font);
    if(root == PP_NONE) {
      c->failed = true;
    }
    return root;
  }
}

/* The node half of a function application, shared with the walker. The
 * name is not re-checked here: both callers gate on ppEqFunctionItem
 * first. */
uint8_t ppqBuildCall(const char *name, uint16_t len, uint8_t arg, uint8_t font) {
  uint8_t hb  = ppNewBox(PP_HBOX, font);
  uint8_t run = ppNewRun(name, len, font);
  uint8_t par = ppNewBox(PP_PAREN, font);
  if(hb == PP_NONE || run == PP_NONE || par == PP_NONE || arg == PP_NONE) {
    return PP_NONE;
  }
  ppAppendChild(par, arg);
  ppAppendChild(hb, run);
  ppAppendChild(hb, par);
  return hb;
}

/* A function application: sin(x), ln(x). Does not set fracSeen: a
 * drawn sin(x) is the same shape as a linear one. A probe: it consumes
 * nothing unless the name resolves through ppEqFunctionItem. */
static uint8_t ppqFunctionCall(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  int16_t save = c->pos, start = c->pos, next;
  while(c->pos < c->len) {
    uint16_t ch = ppqPeek(c, &next);
    if((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      c->pos = next;
    }
    else {
      break;
    }
  }
  int16_t len = (int16_t)(c->pos - start);
  char nm[24];
  if(len == 0 || len >= (int16_t)sizeof(nm) || c->pos >= c->len || c->s[c->pos] != '(') {
    c->pos = save;
    return PP_NONE;
  }
  xcopy(nm, (void *)(c->s + start), (uint32_t)len);
  nm[len] = 0;
  if(ppEqFunctionItem(nm) < 0) {
    c->pos = save;
    return PP_NONE;
  }
  c->pos++;   // the '('
  uint8_t arg = ppqExpr(c, font, tinyF);
  ppqSkipSpace(c);
  if(c->failed || arg == PP_NONE || ppqPeek(c, &next) != ')') {
    c->failed = true;
    return PP_NONE;
  }
  c->pos = next;
  uint8_t hb = ppqBuildCall(nm, (uint16_t)len, arg, font);
  if(hb == PP_NONE) {
    c->failed = true;
  }
  return hb;
}

static uint8_t ppqPrimary(ppqCtx_t *c, uint8_t font, uint8_t tinyF) {
  int16_t next;
  ppqSkipSpace(c);
  uint16_t code = ppqPeek(c, &next);

  // either spelling reaches the probe, which consumes nothing unless
  // it matches
  if((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z')) {
    uint8_t big = ppqBigopConstruct(c, font, tinyF);
    if(c->failed) {
      return PP_NONE;
    }
    if(big != PP_NONE) {
      return big;
    }
    uint8_t fn = ppqFunctionCall(c, font, tinyF);
    if(c->failed) {
      return PP_NONE;
    }
    if(fn != PP_NONE) {
      return fn;
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
  // the stored form spells exponents with '^': build a real 2D
  // superscript
  {
    int16_t nx;
    if(ppqPeek(c, &nx) == '^') {
      c->pos = nx;
      uint8_t exp = ppqFactor(c, font, tinyF);
      if(c->failed || exp == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      uint8_t sup = ppNewBox(PP_SUP, font);
      if(sup == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      n = ppqScopeOperand(c, n, font);
      n = ppfPowBase(n, ppfRunPrec(n), font);
      if(n == PP_NONE) {
        c->failed = true;   // ppfPowBase allocates and does not set it
        return PP_NONE;
      }
      ppAppendChild(sup, n);
      ppAppendChild(sup, ppqUnwrapParen(exp));   // the raise scopes
      c->fracSeen = true;
      return sup;
    }
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
    n = ppqScopeOperand(c, n, font);
    if(n == PP_NONE) {
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
      // Children re-font to tiny so the stack fits the 23 px strip
      // row, and parens unwrap under the bar. Equal caller fonts
      // (EQSHW) skip the re-font.
      uint8_t num = ppqUnwrapParen(n);
      uint8_t dn = ppqUnwrapParen(den);
      if(tinyF != font) {
        ppSetFontDeep(num, tinyF);
        ppSetFontDeep(dn, tinyF);
      }
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
      // multiplication typesets as the raised dot regardless of how it
      // was typed
      uint8_t opRun = ppNewRun(STD_DOT, 2, font);
      if(box == PP_NONE || opRun == PP_NONE) {
        c->failed = true;
        return PP_NONE;
      }
      n = ppqScopeOperand(c, n, font);
      if(n == PP_NONE) {
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

  // a labeled equation ("NAME:expr") starts after the ':', found with
  // the same bounded scan parseEquation uses
  {
    int16_t p = 0;
    for(int i = 0; i < 7 && p < c.len; i++) {
      p += ((uint8_t)src[p] & 0x80) ? 2 : 1;
      if(p < c.len && src[p] == ':') {
        c.pos = (int16_t)(p + 1);
        break;
      }
      if(p < c.len && src[p] == '(') {
        break;
      }
    }
  }

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
 * equation's own strip row. False -> upstream's showString runs. */
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


/* ==== EQSHW: full-screen equation view ==================================
 * Nested fractions render at full size in the 21..167 band. In the
 * interactive integrate solver the equation is the integrand, framed by
 * a stroke-drawn big ∫ (PP_INT). */

/* The interactive solver's numbers frame the equation: the integral
 * shows its real limits (RESERVED_VARIABLE_ULIM/LLIM) and the
 * d-variable, and the derivative modes get the d/dx (d²/dx²) prefix.
 * Solve framing (f(x)=0) is skipped: SOLVER_STATUS_EQUATION_SOLVER is
 * the zero value, indistinguishable from no session at all. */

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
    snprintf(dtext, sizeof(dtext), " d%s", dv);
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
  // no live limits to show: the bare stroke ∫
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
  snprintf(den, sizeof(den), second ? "d%s" "\xa1\x62" : "d%s", dv);
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

/* Pack glyph by glyph until the next does not fit, then append the
 * ellipsis (upstream's own shape for this problem). */
void ppqFitWithEllipsis(const char *src, char *out, uint16_t cap) {
  const int16_t budget = (int16_t)(SCREEN_WIDTH - 4
                                   - stringWidth(STD_ELLIPSIS, &standardFont, false, true));
  uint16_t o = 0;
  int16_t used = 0, pos = 0;
  out[0] = 0;
  while(src[pos] != 0) {
    const uint16_t n = ((uint8_t)src[pos] >= 0x80) ? 2u : 1u;
    char one[3];
    if((uint16_t)(o + n + sizeof(STD_ELLIPSIS)) >= cap) {
      break;
    }
    one[0] = src[pos];
    one[1] = (n == 2) ? src[pos + 1] : 0;
    one[2] = 0;
    const int16_t gw = stringWidth(one, &standardFont, true, true);
    if(used + gw > budget) {
      break;
    }
    out[o++] = one[0];
    if(n == 2) {
      out[o++] = one[1];
    }
    used = (int16_t)(used + gw);
    pos  = (int16_t)(pos + n);
  }
  out[o] = 0;
  if(src[pos] != 0) {
    strcat(out, STD_ELLIPSIS);   // only when something was actually cut
  }
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
    // always show something: the linear line, centered when it fits
    // and at the left edge when it does not
    int16_t w = stringWidth(src, &standardFont, false, true);
    if(w < SCREEN_WIDTH - 4) {
      showString(src, &standardFont, (int16_t)((SCREEN_WIDTH - w) / 2),
                 94 - 8, vmNormal, false, true);
    }
    else {
      /* An unclipped paint drops every glyph past the edge with
       * nothing marking the cut, so mark it. */
      char cut[256];
      ppqFitWithEllipsis(src, cut, sizeof(cut));
      showString(cut, &standardFont, 2, 94 - 8, vmNormal, false, true);
    }
  }

  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
  // a self-painted screen declares itself one, so EXIT can dismiss it
  temporaryInformation = TI_SHOWNOTHING;
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
  // EQSHW reads the stored text. The display pipeline truncates long
  // equations with an ellipsis, and the strict parser declines that.
  // The stored alphabet differs only in '^' and the label prefix.
  if(allFormulae[currentFormula].pointerToFormulaData == C47_NULL) {
    return;
  }
  ppqShowRender((const char *)TO_PCMEMPTR(allFormulae[currentFormula].pointerToFormulaData));
}
