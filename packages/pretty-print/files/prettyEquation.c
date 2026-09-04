// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyEquation.c
 * Two-dimensional equation strip rendering.
 *
 * A recursive-descent parser reads the display string from showEquation.
 * It renders '/' as stacked fractions and adds vinculums over roots
 * within the 23-pixel softmenu strip.
 *
 * If the parser encounters unsupported syntax, it declines.
 * When it declines, upstream code draws the linear string.
 * This renderer remains inactive during equation editing.
 *
 * Grammar by glyph class:
 *   equation := expr ('=' expr)?
 *   expr     := ['+'|'-'] term (('+'|'-') term)*
 *   term     := factor (('×'|'·'|'/') factor)*    '/' -> stacked FRAC
 *   factor   := primary [sup-digit run]
 *   primary  := number | name | '(' expr ')' | '√' primary
 *
 * A fraction bar or vinculum scopes enclosed terms, so the parser
 * removes redundant parentheses.
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

/* Read the character at current position without advancing parser offset.
 * Returns single-byte ASCII directly and advances the next pointer by one.
 * Packs two-byte font glyphs into big-endian 16-bit integers and advances by two.
 * Sets the failure flag when a two-byte glyph is truncated. */
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

/* Advance parser position past spaces.
 * Skips standard ASCII spaces and C47 special spacing glyphs (0xa000 to 0xa00f).
 * Stops when the parser reaches a non-space character or string end. */
static void ppqSkipSpace(ppqCtx_t *c) {
  int16_t next;
  while(c->pos < c->len && PPQ_IS_SPACE(ppqPeek(c, &next))) {
    c->pos = next;
  }
}

static uint8_t ppqExpr(ppqCtx_t *c, uint8_t font, uint8_t tinyF);

/* Parse a numeric literal token.
 * Reads decimal digits, radix marks, plus scientific notation components.
 * Consumes optional base-10 product markers and superscript exponents verbatim.
 * Creates a text run (PP_RUN) from the consumed characters.
 * Returns PP_NONE when no digits match, or fails when memory is full. */
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
  {
    uint8_t n = ppNewRun(c->s + start, (uint16_t)(c->pos - start), font);
    if(n == PP_NONE) {
      c->failed = true;   // digits were consumed, so a failed run must fail the parse
    }
    return n;
  }
}

/* Parse a variable identifier token.
 * Reads initial letters followed by optional subscript digits.
 * Maps single uppercase variable X to lowercase x to follow mathematical style.
 * Retains original letters for all other variable names.
 * Returns a text run (PP_RUN), or PP_NONE when no letters match. */
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

/* Wrap a big operator node in parentheses when used as an operand.
 * Big operators (such as sums or integrals) extend horizontally to the right.
 * Placing them next to other operators without parentheses creates visual ambiguity.
 * If the node is a big operator (PP_BIGOP), wraps it in parentheses.
 * Returns the node unmodified for other node types. */
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

/* Remove outer parentheses from a node when an enclosing bar scopes the expression.
 * Horizontal fraction bars and root vinculums visually group their contents.
 * Strips the outer PP_PAREN container and returns its inner child node.
 * Returns the original node unchanged when no outer parentheses exist. */
uint8_t ppqUnwrapParen(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd != NULL && nd->kind == PP_PAREN) {
    return nd->firstChild;   // the bar/vinculum scopes: drop the parens
  }
  return n;
}

/* Convert a fixed symbol string into a 2D layout text node.
 * The layout engine cannot place raw C text strings directly into boxes.
 * It must convert symbols into measured graphic nodes (PP_RUN).
 * This helper calculates string length and creates the node in the requested font.
 * Returns the node index, or PP_NONE when node memory is full. */
static uint8_t ppqRun(const char *s, uint8_t fontId) {
  return ppNewRun(s, (uint16_t)strlen(s), fontId);
}

/* Wrap a big operator body in parentheses when it contains additions or subtractions.
 * In mathematical notation, big operators bind tightly to their immediate term.
 * An expression like PROD 1+x reads as (PROD 1)+x without parentheses.
 * Inspects child runs for plus or minus signs.
 * Wraps the body in parentheses if any addition or subtraction exists. */
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

/* Assemble a 2D layout structure for a big operator.
 * Builds notation for summations, products, integrals, plus derivatives.
 * For summations and products, index limits use the small font (varTiny).
 * For integrals, the differential variable uses the context font (varCtx).
 * For derivatives, the denominator uses varCtx while the evaluation subscript uses varTiny.
 * Pass PP_NONE for unused variable nodes.
 * Shared with the program walker to keep uniform mathematical typography. */
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

/* Probe whether input text matches a big operator keyword.
 * Checks for construct names such as SUM, PROD, INTEG, or DERIV.
 * Uses ppEqConstructIs to match the exact keywords accepted by the evaluator.
 * If matched, calculates the character position after the opening parenthesis.
 * Returns true when matched, or false without consuming input characters. */
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

/* Create a text node for an integration or derivative variable.
 * Reads the variable slice between start and end offsets.
 * Maps uppercase variable X to lowercase x to follow mathematical style.
 * Returns the allocated text node (PP_RUN) in the requested font. */
static uint8_t ppqVarRun(ppqCtx_t *c, int16_t start, int16_t end, uint8_t font) {
  if(end - start == 1 && c->s[start] == 'X') {
    char lc = 'x';
    return ppNewRun(&lc, 1, font);
  }
  return ppNewRun(c->s + start, (uint16_t)(end - start), font);
}

/* Consume an expected delimiter character from the parser stream.
 * Skips leading whitespace before it tests the next character.
 * Advances the parser position when the character matches the expectation.
 * Sets the failure flag and returns false when the character does not match. */
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

/* Parse a big operator construct from the input stream.
 * Matches keywords SUM, PROD, DERIV, or INTEG.
 * Reads semicolon-separated arguments for the expression body and index variable.
 * Marks fracSeen to activate 2D rendering.
 * Calls ppqBuildBigop to assemble the complete 2D mathematical structure. */
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

/* Build a 2D layout box for a single-argument function call.
 * Constructs the layout shape name(arg) with scalable parentheses.
 * Allocates a horizontal box containing the function name and argument container.
 * Shared with the program walker for uniform function formatting.
 * Returns the box index, or PP_NONE when memory is exhausted. */
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

/* Parse a named function call such as sin(x) or ln(x).
 * Reads letters and verifies an opening parenthesis follows immediately.
 * Validates the identifier against the catalog function table using ppEqFunctionItem.
 * Parses the enclosed argument expression and closing parenthesis.
 * Returns PP_NONE without advancing input if the identifier is not a known function. */
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

/* Parse a primary expression term.
 * Primaries include numbers and variable names alongside parenthesized groups and square roots.
 * Also probes for big operator constructs and known function calls.
 * For square roots, strips redundant parentheses under the vinculum and marks fracSeen.
 * Sets the failure flag and returns PP_NONE when no valid primary matches. */
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

/* Parse a factor expression with optional exponentiation.
 * Reads a primary base followed by caret powers (x^y) or superscript glyphs.
 * For caret powers, scopes the base.
 * It also creates a 2D superscript box (PP_SUP).
 * Recursively parses the exponent to support right-associative power chains.
 * Marks fracSeen when a 2D superscript is created. */
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

/* Parse a term expression joined by multiplication or division.
 * Loops through factors separated by division slashes or multiplication dots.
 * Converts division slashes into stacked vertical fraction boxes (PP_FRAC).
 * Unwraps redundant parentheses under the fraction bar and re-fonts children to tiny.
 * Formats multiplication symbols with standard raised dots and marks fracSeen. */
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

/* Parse a complete algebraic expression joined by addition and subtraction.
 * Consumes optional leading plus or minus signs before the initial term.
 * Loops through terms separated by plus and minus operators.
 * Combines terms and operators horizontally into layout boxes.
 * Returns the expression root node, or PP_NONE when parsing fails. */
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

/* Parse an equation string into a 2D layout tree.
 * Strips optional equation name prefixes such as NAME: before parsing.
 * Parses the left-hand expression.
 * It also parses an optional equal sign with a right-hand expression.
 * Declines and returns false if unparsed trailing text remains.
 * Declines and returns false if no 2D structures were gained (fracSeen is false).
 * Returns true and writes the root node when 2D formatting succeeds. */
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

/* Attempt to render an equation as a 2D layout in the softmenu strip.
 * Checks if pretty-printing is enabled before resetting node memory.
 * Parses the text and measures total pixel width alongside vertical height.
 * Declines and returns false if dimensions exceed the 23-pixel softmenu band.
 * Centers the equation vertically and paints it directly into the screen buffer.
 * Returns true on success, or false to let upstream draw linear text. */
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

/* Frame an equation with integral notation for the interactive solver.
 * Reads lower and upper limits from solver registers when interactive mode is active.
 * Combines the equation with a differential variable to form the integrand.
 * Attaches formatted limit values to the integral sign (PP_BIGOP).
 * Falls back to a bare integral symbol (PP_INT) when limits are inactive. */
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

/* Frame an equation with derivative prefix notation.
 * Retrieves the active solver variable name.
 * Builds a differential fraction (d/dx or d2/dx2) based on derivative order.
 * Wraps the equation in parentheses adjacent to the differential fraction.
 * Returns the assembled horizontal container, or the original node on failure. */
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

/* Truncate a text string to fit screen width and append an ellipsis.
 * Calculates available pixel width budget based on screen width and ellipsis size.
 * Copies characters until the next character exceeds the budget.
 * Appends an ellipsis glyph when the string must be truncated.
 * Handles both single-byte ASCII characters and two-byte font glyphs. */
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

/* Render a full-screen equation view inside the display area.
 * Clears the screen and draws horizontal boundary lines at rows 20 and 168.
 * Attempts 2D rendering with solver framing for integrals or derivatives.
 * Centers the 2D layout vertically and horizontally when it fits the display band.
 * Falls back to centered linear text or ellipsized truncation when 2D layout fails.
 * Configures manual screen update modes to allow clean view dismissal. */
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
      // An unclipped drawing omits glyphs past the screen boundary.
      // Add an ellipsis to show the truncation.
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

/* Execute the full-screen equation view command (EQSHW).
 * Verifies that formulas exist in catalog memory and checks error state.
 * Retrieves stored formula text for the active equation index.
 * Invokes ppqShowRender to display the complete formula on the LCD.
 * Reads stored data directly to bypass menu strip display truncation. */
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
