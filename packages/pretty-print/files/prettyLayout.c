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
  }
  ppMetReady = true;
}

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
      // Bar rows: [B + barTopRel, B + barTopRel + barThick - 1].
      // Numerator ink ends fracGap rows above the bar; denominator ink
      // starts fracGap rows below it.
      ppPool[num].relBase = m->barTopRel - m->fracGap - ppPool[num].descent;
      ppPool[den].relBase = m->barTopRel + m->barThick + m->fracGap + ppPool[den].ascent;

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


static void ppPaint(uint8_t n, int16_t x, int16_t baseline) {
  const ppNode_t *nd = &ppPool[n];
  const ppMetrics_t *m = &ppMet[nd->fontId];

  switch(nd->kind) {
    case PP_RUN:
      showString(ppText + nd->textOff, m->font, x, baseline - m->boxAscent, vmNormal, false, true);
      return;

    case PP_FRAC:
    case PP_HBOX:
      for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppPool[c].nextSibling) {
        ppPaint(c, x + ppPool[c].relX, baseline + ppPool[c].relBase);
      }
      if(nd->kind == PP_FRAC) {
        // The bar goes AFTER the children: showGlyphCode pre-clears each
        // glyph's full box, and a digit box's padding rows reach into the
        // bar band even though its ink honours fracGap. Visible ink is
        // LCD_EMPTY_VALUE in lcd_fill_rect, the same call
        // drawSinglePixelFullWidthLine makes for its visible rules.
        lcd_fill_rect(x, baseline + m->barTopRel, nd->width, m->barThick, LCD_EMPTY_VALUE);
      }
      return;

    default:
      return;
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
  ppPaint(root, xRight - r->width, base);
  return true;
}
