// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyLayout.c
 * The pretty-print box-model engine: fixed pools, a measure pass and a
 * paint pass. Glyph rendering stays on the upstream pipeline (showString);
 * the only pixels this file paints itself are fraction bars, so the pretty
 * output inherits every font behaviour upstream has.
 *
 * All metrics are derived from the live font data at first use, not
 * hardcoded: the box ascent comes from the digit '0' glyph, and the
 * fraction bar is placed to cover the top rows of the minus sign's ink —
 * a pretty fraction bar sits exactly where a minus sign would, per font.
 */

#include "c47.h"
#include "prettyInternal.h"

static ppNode_t ppPool[PP_POOL_NODES];
static char     ppText[PP_TEXT_BYTES];
static uint8_t  ppNodeCount = 0;
static uint16_t ppTextLen   = 0;

typedef struct {
  const font_t *font;
  int16_t boxAscent;   ///< rowsAbove + rowsGlyph of '0': showString y = baseline - boxAscent
  int16_t barTopRel;   ///< minus-sign ink top relative to baseline (negative)
  int16_t barThick;
  int16_t fracGap;     ///< clear rows between bar and numerator/denominator ink
  int16_t overhang;    ///< bar horizontal overhang past the wider child, each side
  int16_t vincThick;   ///< radical vinculum thickness
  int16_t radGap;      ///< clear rows between radicand ink and the vinculum
  int16_t supDrop;     ///< superscript baseline raise above the base baseline
  int16_t radAbove;    ///< radical-sign glyph: ink rows above its baseline
  int16_t radInk;      ///< radical-sign glyph: total ink rows
  int16_t radAdvance;  ///< radical-sign glyph: advance width (leading cols dropped)
  int16_t parAbove;    ///< '(' glyph: rowsAboveGlyph
  int16_t parInk;      ///< '(' glyph: ink rows
  int16_t parAdvance;  ///< '(' glyph advance (leading cols dropped)
} ppMetrics_t;

static ppMetrics_t ppMet[3];
static bool_t ppMetReady = false;
static bool_t ppMetOk    = false;

static const glyph_t *ppGlyphOf(const font_t *font, uint16_t charCode) {
  int16_t gi = findGlyph(font, charCode);
  return gi < 0 ? NULL : &font->glyphs[gi];
}

static void ppMetricsInit(void) {
  const font_t *f[3];
  f[PP_FONT_NUMERIC]  = &numericFont;
  f[PP_FONT_STANDARD] = &standardFont;
  f[PP_FONT_TINY]     = &tinyFont;

  ppMetOk = true;
  for(int i = 0; i < 3; i++) {
    const glyph_t *zero  = ppGlyphOf(f[i], '0');
    const glyph_t *minus = ppGlyphOf(f[i], '-');
    ppMet[i].font = f[i];
    if(zero == NULL || minus == NULL) {
      ppMetOk = false;
      continue;
    }
    ppMet[i].boxAscent = zero->rowsAboveGlyph + zero->rowsGlyph;
    ppMet[i].barTopRel = -(ppMet[i].boxAscent - minus->rowsAboveGlyph);
    ppMet[i].barThick  = (i == PP_FONT_NUMERIC) ? 2 : 1;
    ppMet[i].fracGap   = (i == PP_FONT_NUMERIC) ? 2 : 1;
    ppMet[i].overhang  = (i == PP_FONT_NUMERIC) ? 2 : 1;
    ppMet[i].vincThick = (i == PP_FONT_TINY) ? 1 : 2;   // matches the sign's stroke weight
    ppMet[i].radGap    = (i == PP_FONT_TINY) ? 0 : 1;
    ppMet[i].supDrop   = (i == PP_FONT_NUMERIC) ? 10 : (i == PP_FONT_STANDARD ? 6 : 4);
    {
      // 0xa21a = STD_SQUARE_ROOT
      const glyph_t *rad = ppGlyphOf(f[i], 0xa21a);
      const glyph_t *par = ppGlyphOf(f[i], '(');
      if(rad == NULL || par == NULL) {
        ppMetOk = false;
      }
      else {
        ppMet[i].radAbove   = rad->rowsAboveGlyph;
        ppMet[i].radInk     = rad->rowsGlyph;
        ppMet[i].radAdvance = rad->colsGlyph + rad->colsAfterGlyph;
        ppMet[i].parAbove   = par->rowsAboveGlyph;
        ppMet[i].parInk     = par->rowsGlyph;
        ppMet[i].parAdvance = par->colsGlyph + par->colsAfterGlyph;
      }
    }
  }
  ppMetReady = true;
}

static void ppBigopBox(const ppNode_t *body, const ppMetrics_t *m,
                       int16_t *ga, int16_t *gd, int16_t *gw);

static bool_t ppMetricsOk(void) {
  if(!ppMetReady) {
    ppMetricsInit();
  }
  return ppMetOk;
}


void ppReset(void) {
  ppNodeCount = 0;
  ppTextLen   = 0;
}

uint8_t ppNewBox(uint8_t kind, uint8_t fontId) {
  if(ppNodeCount >= PP_POOL_NODES) {
    return PP_NONE;
  }
  ppNode_t *n = &ppPool[ppNodeCount];
  memset(n, 0, sizeof(*n));
  n->kind        = kind;
  n->fontId      = fontId;
  n->firstChild  = PP_NONE;
  n->nextSibling = PP_NONE;
  return ppNodeCount++;
}

uint8_t ppNewRun(const char *bytes, uint16_t len, uint8_t fontId) {
  if(ppTextLen + len + 1 > PP_TEXT_BYTES) {
    return PP_NONE;
  }
  uint8_t n = ppNewBox(PP_RUN, fontId);
  if(n == PP_NONE) {
    return PP_NONE;
  }
  ppPool[n].textOff = ppTextLen;
  xcopy(ppText + ppTextLen, bytes, len);
  ppTextLen += len;
  ppText[ppTextLen++] = 0;
  return n;
}

void ppAppendChild(uint8_t parent, uint8_t child) {
  if(parent == PP_NONE || child == PP_NONE) {
    return;
  }
  if(ppPool[parent].firstChild == PP_NONE) {
    ppPool[parent].firstChild = child;
    return;
  }
  uint8_t c = ppPool[parent].firstChild;
  while(ppPool[c].nextSibling != PP_NONE) {
    c = ppPool[c].nextSibling;
  }
  ppPool[c].nextSibling = child;
}

void ppSetBoxTag(uint8_t n, uint16_t tag) {
  if(n < ppNodeCount) {
    ppPool[n].textOff = tag;
  }
}

const ppNode_t *ppNodeAt(uint8_t n) {
  return (n < ppNodeCount) ? &ppPool[n] : NULL;
}

const char *ppTextAt(uint16_t off) {
  return (off < ppTextLen) ? ppText + off : "";
}

int16_t ppPreferredBase(int16_t baseY) {
  return baseY + (ppMetricsOk() ? ppMet[PP_FONT_NUMERIC].boxAscent : 0);
}


/* Tight ink extents of a glyph run relative to the run font's baseline.
 * Space-class glyphs (no ink rows) contribute nothing; an unknown glyph
 * fails the run — a not-found box would have metrics we never audited. */
static bool_t ppRunInk(const char *s, const ppMetrics_t *m, int16_t *asc, int16_t *desc) {
  int16_t pos = 0;
  *asc  = 0;
  *desc = 0;
  while(s[pos] != 0) {
    uint16_t code = (uint8_t)s[pos];
    if(code >= 0x80) {
      if(s[pos + 1] == 0) {
        return false;
      }
      code = (uint16_t)((code << 8) | (uint8_t)s[pos + 1]);
      pos += 2;
    }
    else {
      pos += 1;
    }
    if(code == 0x0001) {   // STD_NOCHAR: showGlyphCode paints nothing for it
      continue;
    }
    const glyph_t *g = ppGlyphOf(m->font, code);
    if(g == NULL) {
      return false;
    }
    if(g->rowsGlyph == 0) {
      continue;
    }
    int16_t a = m->boxAscent - g->rowsAboveGlyph;
    int16_t d = (g->rowsAboveGlyph + g->rowsGlyph) - m->boxAscent;
    if(a > *asc)  *asc  = a;
    if(d > *desc) *desc = d;
  }
  return true;
}


bool_t ppMeasure(uint8_t n, uint8_t depth) {
  if(n == PP_NONE || n >= ppNodeCount || depth > PP_MAX_DEPTH || !ppMetricsOk()) {
    return false;
  }
  ppNode_t *nd = &ppPool[n];
  const ppMetrics_t *m = &ppMet[nd->fontId];

  switch(nd->kind) {
    case PP_RUN: {
      nd->width = stringWidth(ppText + nd->textOff, m->font, false, true);
      return ppRunInk(ppText + nd->textOff, m, &nd->ascent, &nd->descent);
    }

    case PP_HBOX: {
      int16_t x = 0, asc = 0, desc = 0;
      for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppPool[c].nextSibling) {
        if(!ppMeasure(c, depth + 1)) {
          return false;
        }
        ppPool[c].relX    = x;
        ppPool[c].relBase = 0;
        x += ppPool[c].width;
        if(ppPool[c].ascent  > asc)  asc  = ppPool[c].ascent;
        if(ppPool[c].descent > desc) desc = ppPool[c].descent;
      }
      nd->width   = x;
      nd->ascent  = asc;
      nd->descent = desc;
      return true;
    }

    case PP_RAD: {
      uint8_t child = nd->firstChild;
      if(child == PP_NONE) {
        return false;
      }
      uint8_t index = ppPool[child].nextSibling;   // optional ⁿ√ index
      if(index != PP_NONE && ppPool[index].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(child, depth + 1)) {
        return false;
      }
      if(index != PP_NONE && !ppMeasure(index, depth + 1)) {
        return false;
      }
      // Short radicands use the raised font glyph, taller ones a
      // synthesized stroke. An INDEXED root always synthesizes: the
      // glyph's hook collides with the tucked index. Paint repeats this.
      bool_t synth = (ppPool[child].ascent + ppPool[child].descent > m->radInk + 3)
                     || (index != PP_NONE);
      int16_t signW = synth ? 10 : m->radAdvance;
      int16_t idxW = 0;
      if(index != PP_NONE) {
        // the index tucks above-left; roughly half of it overlaps the sign
        idxW = ppPool[index].width - signW / 2;
        if(idxW < 0) {
          idxW = 0;
        }
      }
      ppPool[child].relX    = (int16_t)(idxW + signW);
      ppPool[child].relBase = 0;
      nd->width   = (int16_t)(idxW + signW + ppPool[child].width + m->overhang);
      // radGap+1 for the same asymmetry the fraction bar has: the
      // radicand's ascent counts strictly above its baseline
      nd->ascent  = ppPool[child].ascent + m->radGap + 1 + m->vincThick;
      nd->descent = ppPool[child].descent;
      if(!synth && m->radInk - nd->ascent > nd->descent) {
        nd->descent = m->radInk - nd->ascent;   // the raised glyph's tail
      }
      if(index != PP_NONE) {
        // index baseline: its ink bottom near the sign's upper third
        int16_t h = ppPool[child].ascent + ppPool[child].descent;
        ppPool[index].relX    = 0;
        ppPool[index].relBase = (int16_t)(-nd->ascent + m->vincThick + h / 3 + ppPool[index].descent);
        int16_t idxAsc = (int16_t)(-ppPool[index].relBase + ppPool[index].ascent);
        if(idxAsc > nd->ascent) {
          nd->ascent = idxAsc;
        }
        /* The descent dual of the line above: the index tucks above the
         * baseline, but its ink bottom at relBase + descent can fall below
         * the box, and every band check downstream trusts both edges. */
        int16_t idxDesc = (int16_t)(ppPool[index].relBase + ppPool[index].descent);
        if(idxDesc > nd->descent) {
          nd->descent = idxDesc;
        }
      }
      return true;
    }

    case PP_SUB: {
      uint8_t base = nd->firstChild;
      if(base == PP_NONE) {
        return false;
      }
      uint8_t script = ppPool[base].nextSibling;
      if(script == PP_NONE || ppPool[script].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(base, depth + 1) || !ppMeasure(script, depth + 1)) {
        return false;
      }
      ppPool[base].relX      = 0;
      ppPool[base].relBase   = 0;
      ppPool[script].relX    = ppPool[base].width + 1;
      ppPool[script].relBase = m->supDrop / 2 + 2;   // lowered, mirror of PP_SUP
      nd->width   = ppPool[base].width + 1 + ppPool[script].width;
      nd->ascent  = ppPool[base].ascent;
      if(ppPool[script].ascent - ppPool[script].relBase > nd->ascent) {
        nd->ascent = ppPool[script].ascent - ppPool[script].relBase;
      }
      nd->descent = ppPool[base].descent;
      if(ppPool[script].relBase + ppPool[script].descent > nd->descent) {
        nd->descent = ppPool[script].relBase + ppPool[script].descent;
      }
      return true;
    }

    case PP_INT: {
      // big integral sign: stroke-drawn, spans the operand's height + hooks
      uint8_t child = nd->firstChild;
      if(child == PP_NONE || ppPool[child].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(child, depth + 1)) {
        return false;
      }
      ppPool[child].relX    = 16;
      ppPool[child].relBase = 0;
      nd->width   = ppPool[child].width + 18;
      nd->ascent  = ppPool[child].ascent + 3;
      nd->descent = ppPool[child].descent + 3;
      return true;
    }

    case PP_BIGOP: {
      // children: body, under-limit, over-limit. The operator glyph is
      // stroke-drawn in a box left of the body; limits stack under/over
      // that box. Paint recomputes the same box from the same inputs.
      uint8_t body = nd->firstChild;
      if(body == PP_NONE) {
        return false;
      }
      uint8_t under = ppPool[body].nextSibling;
      if(under == PP_NONE) {
        return false;
      }
      uint8_t over = ppPool[under].nextSibling;
      if(over == PP_NONE || ppPool[over].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(body, depth + 1) || !ppMeasure(under, depth + 1)
          || !ppMeasure(over, depth + 1)) {
        return false;
      }
      int16_t ga, gd, gw;
      ppBigopBox(&ppPool[body], m, &ga, &gd, &gw);
      int16_t colW = gw;
      if(ppPool[under].width > colW) colW = ppPool[under].width;
      if(ppPool[over].width  > colW) colW = ppPool[over].width;
      ppPool[over].relX     = (int16_t)((colW - ppPool[over].width) / 2);
      ppPool[over].relBase  = (int16_t)(-(ga + 2 + ppPool[over].descent));
      ppPool[under].relX    = (int16_t)((colW - ppPool[under].width) / 2);
      ppPool[under].relBase = (int16_t)(gd + 2 + ppPool[under].ascent);
      ppPool[body].relX     = (int16_t)(colW + 3);
      ppPool[body].relBase  = 0;
      nd->width   = (int16_t)(colW + 3 + ppPool[body].width);
      nd->ascent  = (int16_t)(ga + 2 + ppPool[over].descent + ppPool[over].ascent);
      if(ppPool[body].ascent > nd->ascent) {
        nd->ascent = ppPool[body].ascent;
      }
      nd->descent = (int16_t)(gd + 2 + ppPool[under].ascent + ppPool[under].descent);
      if(ppPool[body].descent > nd->descent) {
        nd->descent = ppPool[body].descent;
      }
      return true;
    }

    case PP_BARS: {
      uint8_t child = nd->firstChild;
      if(child == PP_NONE || ppPool[child].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(child, depth + 1)) {
        return false;
      }
      ppPool[child].relX    = 4;
      ppPool[child].relBase = 0;
      nd->width   = ppPool[child].width + 8;
      nd->ascent  = ppPool[child].ascent + 1;
      nd->descent = ppPool[child].descent + 1;
      return true;
    }

    case PP_SUP: {
      uint8_t base = nd->firstChild;
      if(base == PP_NONE) {
        return false;
      }
      uint8_t exp = ppPool[base].nextSibling;
      if(exp == PP_NONE || ppPool[exp].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(base, depth + 1) || !ppMeasure(exp, depth + 1)) {
        return false;
      }
      ppPool[base].relX    = 0;
      ppPool[base].relBase = 0;
      ppPool[exp].relX     = ppPool[base].width + 1;
      ppPool[exp].relBase  = -m->supDrop;
      nd->width   = ppPool[base].width + 1 + ppPool[exp].width;
      nd->ascent  = ppPool[base].ascent;
      if(m->supDrop + ppPool[exp].ascent > nd->ascent) {
        nd->ascent = m->supDrop + ppPool[exp].ascent;
      }
      nd->descent = ppPool[base].descent;
      if(ppPool[exp].descent - m->supDrop > nd->descent) {
        nd->descent = ppPool[exp].descent - m->supDrop;
      }
      return true;
    }

    case PP_PAREN: {
      uint8_t child = nd->firstChild;
      if(child == PP_NONE || ppPool[child].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(child, depth + 1)) {
        return false;
      }
      int16_t h = ppPool[child].ascent + ppPool[child].descent;
      ppPool[child].relBase = 0;
      if(h <= m->parInk + 2) {
        // glyph parens cover the child; paint recomputes this same test
        ppPool[child].relX = m->parAdvance;
        nd->width   = ppPool[child].width + 2 * m->parAdvance;
        int16_t pAsc  = m->boxAscent - m->parAbove;
        int16_t pDesc = (m->parAbove + m->parInk) - m->boxAscent;
        nd->ascent  = (ppPool[child].ascent  > pAsc)  ? ppPool[child].ascent  : pAsc;
        nd->descent = (ppPool[child].descent > pDesc) ? ppPool[child].descent : pDesc;
      }
      else {
        // synthesized tall parens: 5 px each side, one row over/under
        ppPool[child].relX = 5;
        nd->width   = ppPool[child].width + 10;
        nd->ascent  = ppPool[child].ascent + 1;
        nd->descent = ppPool[child].descent + 1;
      }
      return true;
    }

    case PP_FRAC: {
      uint8_t num = nd->firstChild;
      if(num == PP_NONE) {
        return false;
      }
      uint8_t den = ppPool[num].nextSibling;
      if(den == PP_NONE || ppPool[den].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(num, depth + 1) || !ppMeasure(den, depth + 1)) {
        return false;
      }
      // Bar rows: [B + barTopRel, B + barTopRel + barThick - 1]. Clearance
      // is fracGap+1 both sides: descent counts rows at/below the baseline
      // and ascent strictly above, so the denominator needs the extra row.
      ppPool[num].relBase = m->barTopRel - m->fracGap - ppPool[num].descent;
      ppPool[den].relBase = m->barTopRel + m->barThick + m->fracGap + 1 + ppPool[den].ascent;

      int16_t inner = (ppPool[num].width > ppPool[den].width) ? ppPool[num].width : ppPool[den].width;
      nd->width = inner + 2 * m->overhang;
      ppPool[num].relX = m->overhang + (inner - ppPool[num].width) / 2;
      ppPool[den].relX = m->overhang + (inner - ppPool[den].width) / 2;

      nd->ascent  = -ppPool[num].relBase + ppPool[num].ascent;
      nd->descent =  ppPool[den].relBase + ppPool[den].descent;
      return true;
    }

    default:
      return false;
  }
}


/* Every rule this engine paints — fraction bars, radical vinculums, the
 * |x| strokes, tall parens, the big-operator glyphs — is an
 * lcd_fill_rect taking uint32_t coordinates. The browser's pan paints
 * from a negative origin, and a negative x cast to uint32_t is a huge
 * value, so the rectangle would land nowhere and the rule would vanish
 * while the glyphs beside it still drew. Clip once here rather than at
 * every call site; ppDrawLine screens negatives per pixel already. */
static void ppFillVal(int16_t x, int16_t y, int16_t w, int16_t h, int val) {
  if(w <= 0 || h <= 0) {
    return;
  }
  if(x < 0) {                 // trim what falls off the left edge
    w += x;
    x = 0;
  }
  if(y < 0) {
    h += y;
    y = 0;
  }
  if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || w <= 0 || h <= 0) {
    return;
  }
  if(x + w > SCREEN_WIDTH) {
    w = (int16_t)(SCREEN_WIDTH - x);
  }
  if(y + h > SCREEN_HEIGHT) {
    h = (int16_t)(SCREEN_HEIGHT - y);
  }
  lcd_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, val);
}

static void ppFill(int16_t x, int16_t y, int16_t w, int16_t h) {    // ink
  ppFillVal(x, y, w, h, LCD_EMPTY_VALUE);
}

/* showString paints each glyph through showGlyphCode with noPreClear
 * false, and that pre-clear covers the glyph's whole FONT box — above,
 * glyph and below — not the ink this engine measured. A short-inked
 * denominator sits with its baseline pushed up so the ink still clears
 * the bar, and its font box then reaches across the bar into the
 * numerator, which is painted first and so gets erased. The same
 * overshoot lets a run at the band edge clear frame rows its measured
 * ink never touches.
 *
 * Clear the MEASURED box here and paint the glyphs with noPreClear, so
 * paint covers exactly what measure promised. The denominator's clear
 * now stops at barTopRel + barThick + fracGap + 1 — below the bar for
 * every denominator, by construction. Ordering rules and stretched
 * parens still paint after their glyph runs; that rule is unchanged.
 *
 * slc/sec reproduce _doShowString for the showLeadingCols=false,
 * showEndingCols=true call this replaces, so the advance stays the one
 * stringWidth() measured. Negative x and y are upstream's own convention
 * here: showGlyphCode recovers a wrapped y, and column positions wrap
 * back into range unsigned. */
static void ppShowRun(const char *s, const ppMetrics_t *m, int16_t x, int16_t baseline,
                      int16_t asc, int16_t desc) {
  ppFillVal(x, (int16_t)(baseline - asc), (int16_t)stringWidth(s, m->font, false, true),
            (int16_t)(asc + desc), LCD_SET_VALUE);

  uint16_t ch = 0;
  uint32_t px = (uint32_t)(int32_t)x;
  const uint32_t py = (uint32_t)(int32_t)(baseline - m->boxAscent);

  bool_t slc = false;   // first glyph takes showLeadingCols; the rest take
  while(s[ch] != 0) {   // true, and sec is showEndingCols or true — both true
    const uint16_t code = charCodeFromString(s, &ch);
    px  = showGlyphCode(code, m->font, px, py, vmNormal, slc, true, true);
    slc = true;
  }
}

// integer Bresenham over setBlackPixel — the synthesized radical sign
static void ppDrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
  int16_t dy = (int16_t)(y1 > y0 ? y1 - y0 : y0 - y1);
  int16_t sx = (int16_t)(x0 < x1 ? 1 : -1);
  int16_t sy = (int16_t)(y0 < y1 ? 1 : -1);
  int16_t err = (int16_t)(dx - dy);
  for(;;) {
    // setBlackPixel is a thin bitblt24 wrapper with no bounds check of
    // its own, so a panned or oversized stroke would write past the
    // frame buffer. Screen both ends of both axes.
    if(x0 >= 0 && y0 >= 0 && x0 < SCREEN_WIDTH && y0 < SCREEN_HEIGHT) {
      setBlackPixel((uint32_t)x0, (uint32_t)y0);
    }
    if(x0 == x1 && y0 == y1) {
      break;
    }
    int16_t e2 = (int16_t)(2 * err);
    if(e2 > -dy) {
      err = (int16_t)(err - dy);
      x0 = (int16_t)(x0 + sx);
    }
    if(e2 < dx) {
      err = (int16_t)(err + dx);
      y0 = (int16_t)(y0 + sy);
    }
  }
}

/* The big-operator glyph box: ga/gd rows above/below the baseline and
 * a width that scales with the height at the font's own Σ proportions
 * (the numeric Σ measures 14x25, w/h ≈ 0.56) — a fixed width left tall
 * operators pinched. Measure and paint call this same function. */
static void ppBigopBox(const ppNode_t *body, const ppMetrics_t *m,
                       int16_t *ga, int16_t *gd, int16_t *gw) {
  *ga = (int16_t)(body->ascent + 2);
  if(*ga < m->boxAscent) {
    *ga = m->boxAscent;
  }
  *gd = (int16_t)(body->descent + 2);
  if(*gd < 4) {
    *gd = 4;
  }
  *gw = (int16_t)(((*ga + *gd) * 9) / 16);
  if(*gw < 12) *gw = 12;
  if(*gw > 28) *gw = 28;
}

/* The integral sign, stroke-drawn with curved hooks: a 2 px vertical
 * spine whose top bends right and bottom bends left along a quadratic
 * offset — the straight-line hooks read as a slash (found by review).
 * cx is the spine's left column; rows span [top, bot]. */
static void ppDrawIntegralSign(int16_t cx, int16_t top, int16_t bot) {
  int16_t h = (int16_t)(bot - top + 1);
  int16_t hh = (int16_t)(h / 4);
  if(hh > 7) hh = 7;
  if(hh < 3) hh = 3;
  for(int16_t y = top; y <= bot; y++) {
    int16_t dxo = 0;
    if(y < top + hh) {
      int16_t t = (int16_t)(top + hh - 1 - y);        // 0 at hook base, hh-1 at tip
      dxo = (int16_t)((5 * t * t) / ((hh - 1) * (hh - 1) > 0 ? (hh - 1) * (hh - 1) : 1));
    }
    else if(y > bot - hh) {
      int16_t t = (int16_t)(y - (bot - hh + 1));
      dxo = (int16_t)(-(5 * t * t) / ((hh - 1) * (hh - 1) > 0 ? (hh - 1) * (hh - 1) : 1));
    }
    ppFill((int16_t)((cx + dxo)), (int16_t)(y), (int16_t)(2), (int16_t)(1));
  }
  // terminal dots thicken the hook tips
  ppFill((int16_t)((cx + 5)), (int16_t)((top + 1)), (int16_t)(2), (int16_t)(1));
  ppFill((int16_t)((cx - 5)), (int16_t)((bot - 1)), (int16_t)(2), (int16_t)(1));
}

static void ppPaint(uint8_t n, int16_t x, int16_t baseline) {
  const ppNode_t *nd = &ppPool[n];
  const ppMetrics_t *m = &ppMet[nd->fontId];

  switch(nd->kind) {
    case PP_RUN:
      ppShowRun(ppText + nd->textOff, m, x, baseline, nd->ascent, nd->descent);
      return;

    case PP_RAD: {
      // Index, sign, radicand, then the vinculum LAST — the binding
      // paint-order rule: glyph-box pre-clears wipe any rule painted
      // before their glyphs.
      uint8_t child = nd->firstChild;
      uint8_t index = ppPool[child].nextSibling;
      bool_t synth = (ppPool[child].ascent + ppPool[child].descent > m->radInk + 3)
                     || (index != PP_NONE);
      int16_t signW = synth ? 10 : m->radAdvance;
      int16_t signX = (int16_t)(x + ppPool[child].relX - signW);
      int16_t vincTop = baseline - ppPool[child].ascent - m->radGap - 1 - m->vincThick;
      if(index != PP_NONE) {
        ppPaint(index, x + ppPool[index].relX, baseline + ppPool[index].relBase);
      }
      if(!synth) {
        showString("\xa2\x1a", m->font, signX, vincTop - m->radAbove, vmNormal, false, true);
      }
      else {
        int16_t yBot = baseline + ppPool[child].descent - 1;
        int16_t yMid = (int16_t)((vincTop + yBot) / 2);
        for(int16_t t = 0; t < ((m->vincThick > 1) ? 2 : 1); t++) {
          ppDrawLine((int16_t)(signX + t), yMid, (int16_t)(signX + 3 + t), yBot);
          ppDrawLine((int16_t)(signX + 3 + t), yBot, (int16_t)(signX + signW - 1 + t), vincTop);
        }
      }
      ppPaint(child, x + ppPool[child].relX, baseline);
      ppFill((int16_t)((x + ppPool[child].relX - 1)), (int16_t)(vincTop), (int16_t)((ppPool[child].width + m->overhang + 1)), (int16_t)(m->vincThick));
      return;
    }

    case PP_INT: {
      uint8_t child = nd->firstChild;
      ppPaint(child, x + ppPool[child].relX, baseline);
      int16_t top = baseline - nd->ascent;
      int16_t bot = baseline + nd->descent - 1;
      ppDrawIntegralSign((int16_t)(x + 7), top, bot);
      return;
    }

    case PP_BIGOP: {
      // children first, strokes last — the binding paint-order rule
      uint8_t body  = nd->firstChild;
      uint8_t under = ppPool[body].nextSibling;
      uint8_t over  = ppPool[under].nextSibling;
      ppPaint(body,  x + ppPool[body].relX,  baseline);
      ppPaint(under, x + ppPool[under].relX, baseline + ppPool[under].relBase);
      ppPaint(over,  x + ppPool[over].relX,  baseline + ppPool[over].relBase);
      int16_t ga, gd, gw;
      ppBigopBox(&ppPool[body], m, &ga, &gd, &gw);
      int16_t colW = gw;
      if(ppPool[under].width > colW) colW = ppPool[under].width;
      if(ppPool[over].width  > colW) colW = ppPool[over].width;
      int16_t gx  = (int16_t)(x + (colW - gw) / 2);
      int16_t top = (int16_t)(baseline - ga);
      int16_t bot = (int16_t)(baseline + gd - 1);
      uint16_t op = nd->textOff;
      if(op == ITM_INTEGRAL_YX) {
        ppDrawIntegralSign((int16_t)(gx + 7), top, bot);
      }
      else if(op == ITM_PIn || op == ITM_iPIn) {
        // the ∏, the font's own design scaled: an overhanging top bar
        // and inset legs (numeric glyph: bar 16 wide, legs 3, inset 1)
        int16_t barT = (int16_t)((bot - top + 1) / 10);
        if(barT < 2) barT = 2;
        int16_t legW = (int16_t)(gw / 5);
        if(legW < 2) legW = 2;
        ppFill((int16_t)(gx), (int16_t)(top), (int16_t)(gw), (int16_t)(barT));
        ppFill((int16_t)((gx + 1)), (int16_t)((top + barT)), (int16_t)(legW), (int16_t)((bot - top + 1 - barT)));
        ppFill((int16_t)((gx + gw - 1 - legW)), (int16_t)((top + barT)), (int16_t)(legW), (int16_t)((bot - top + 1 - barT)));
      }
      else {
        // the Σ, the font's own design scaled: full-width bars whose
        // thickness follows the height, thick diagonals meeting at an
        // apex at ~40% width (numeric glyph: 14x25, apex col 5 of 14)
        int16_t mid = (int16_t)((top + bot) / 2);
        int16_t barT = (int16_t)((bot - top + 1) / 10);
        if(barT < 2) barT = 2;
        int16_t dt = (int16_t)(gw / 5);
        if(dt < 2) dt = 2;
        int16_t apexX = (int16_t)(gx + (gw * 2) / 5);
        ppFill((int16_t)(gx), (int16_t)(top), (int16_t)(gw), (int16_t)(barT));
        ppFill((int16_t)(gx), (int16_t)((bot - barT + 1)), (int16_t)(gw), (int16_t)(barT));
        for(int16_t t = 0; t < dt; t++) {
          ppDrawLine((int16_t)(gx + 1 + t), (int16_t)(top + barT), (int16_t)(apexX + t), mid);
          ppDrawLine((int16_t)(apexX + t), mid, (int16_t)(gx + 1 + t), (int16_t)(bot - barT));
        }
      }
      return;
    }

    case PP_BARS: {
      uint8_t child = nd->firstChild;
      ppPaint(child, x + ppPool[child].relX, baseline);
      int16_t top = baseline - nd->ascent;
      int16_t hh = nd->ascent + nd->descent;
      ppFill((int16_t)((x + 1)), (int16_t)(top), (int16_t)(2), (int16_t)(hh));
      ppFill((int16_t)((x + nd->width - 3)), (int16_t)(top), (int16_t)(2), (int16_t)(hh));
      return;
    }

    case PP_PAREN: {
      uint8_t child = nd->firstChild;
      ppPaint(child, x + ppPool[child].relX, baseline + ppPool[child].relBase);
      int16_t h = ppPool[child].ascent + ppPool[child].descent;
      if(h <= m->parInk + 2) {
        // pAsc/pDesc are measure's own glyph-paren extents (see PP_PAREN
        // in ppMeasure); the child is already painted, so a font-box
        // clear here would eat its ink
        const int16_t pAsc  = (int16_t)(m->boxAscent - m->parAbove);
        const int16_t pDesc = (int16_t)((m->parAbove + m->parInk) - m->boxAscent);
        ppShowRun("(", m, x, baseline, pAsc, pDesc);
        ppShowRun(")", m, (int16_t)(x + nd->width - m->parAdvance), baseline, pAsc, pDesc);
      }
      else {
        int16_t top = baseline - nd->ascent;
        int16_t hh  = nd->ascent + nd->descent;
        int16_t xr  = (int16_t)(x + nd->width - 5);
        ppFill((int16_t)(x + 1), (int16_t)(top + 2), (int16_t)(2), (int16_t)(hh - 4));
        ppFill((int16_t)(x + 2), (int16_t)(top), (int16_t)(2), (int16_t)(2));
        ppFill((int16_t)(x + 2), (int16_t)(top + hh - 2), (int16_t)(2), (int16_t)(2));
        ppFill((int16_t)(xr + 2), (int16_t)(top + 2), (int16_t)(2), (int16_t)(hh - 4));
        ppFill((int16_t)(xr), (int16_t)(top), (int16_t)(2), (int16_t)(2));
        ppFill((int16_t)(xr), (int16_t)(top + hh - 2), (int16_t)(2), (int16_t)(2));
      }
      return;
    }

    case PP_SUB:
    case PP_SUP:
    case PP_FRAC:
    case PP_HBOX:
      for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppPool[c].nextSibling) {
        ppPaint(c, x + ppPool[c].relX, baseline + ppPool[c].relBase);
      }
      if(nd->kind == PP_FRAC) {
        // The children clear only their MEASURED boxes, which stop
        // fracGap+1 above the bar and start fracGap+2 below it, so for a
        // fraction the paint order is not load-bearing. It is kept
        // because the rule still binds the one run painted with a
        // font-box pre-clear: the radical sign glyph.
        // Ink is LCD_EMPTY_VALUE — the same lcd_fill_rect
        // drawSinglePixelFullWidthLine makes for its visible rules.
        ppFill((int16_t)(x), (int16_t)(baseline + m->barTopRel), (int16_t)(nd->width), (int16_t)(m->barThick));
      }
      return;

    default:
      return;
  }
}


void ppSetFontDeep(uint8_t n, uint8_t fontId) {
  if(n == PP_NONE || n >= ppNodeCount) {
    return;
  }
  ppPool[n].fontId = fontId;
  for(uint8_t c = ppPool[n].firstChild; c != PP_NONE; c = ppPool[c].nextSibling) {
    ppSetFontDeep(c, fontId);
  }
}

/* Every glyph goes through here or ppRenderRightAligned, so this is the
 * one place the bold substitution can be suppressed. showGlyphCode swaps
 * in a numericFontBold glyph whenever FLAG_BOLD is set and the font is
 * &numericFont, and advances by the BOLD glyph's rows — while the
 * metrics, stringWidth and ppRunInk all read the PLAIN table and the node
 * has already cleared the box those metrics promised. Suppressing the flag
 * for the paint keeps both on one table, so a BOLD owner still gets a
 * drawing, in the plain numeric face.
 *
 * Restored on the single exit path of each wrapper; there is no early
 * return between save and restore. It is NOT side-effect free: FLAG_BOLD
 * is in refreshStateFlags[], so each flip runs fnRefreshState and raises
 * doRefreshSoftMenu — twice per paint. Harmless today (a menu repaint
 * request the next refresh consumes) but real, and the reason to prefer
 * bold-aware metrics if this surface ever grows a cost it cannot pay. */
static bool_t ppSuppressBold(void) {
  bool_t was = getSystemFlag(FLAG_BOLD);
  if(was) {
    clearSystemFlag(FLAG_BOLD);
  }
  return was;
}

static void ppRestoreBold(bool_t was) {
  if(was) {
    setSystemFlag(FLAG_BOLD);
  }
}

void ppPaintAt(uint8_t root, int16_t x, int16_t baseline) {
  if(root < ppNodeCount && ppMetricsOk()) {
    bool_t boldWas = ppSuppressBold();
    ppPaint(root, x, baseline);
    ppRestoreBold(boldWas);
  }
}

bool_t ppRenderRightAligned(uint8_t root, int16_t xRight,
                            int16_t bandTop, int16_t bandBottom,
                            int16_t preferredBase) {
  if(!ppMetricsOk() || !ppMeasure(root, 0)) {
    return false;
  }
  const ppNode_t *r = &ppPool[root];
  if(r->width <= 0 || r->width > xRight) {
    return false;
  }
  int16_t lo = bandTop + r->ascent;
  int16_t hi = bandBottom - r->descent + 1;
  if(lo > hi) {
    return false;
  }
  int16_t base = preferredBase;
  if(base < lo) base = lo;
  if(base > hi) base = hi;
  bool_t boldWas = ppSuppressBold();
  ppPaint(root, xRight - r->width, base);
  ppRestoreBold(boldWas);
  return true;
}
