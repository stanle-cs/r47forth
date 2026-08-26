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
    ppMet[i].vincThick = (i == PP_FONT_NUMERIC) ? 2 : 1;
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

    case PP_RAD: {
      uint8_t child = nd->firstChild;
      if(child == PP_NONE || ppPool[child].nextSibling != PP_NONE) {
        return false;
      }
      if(!ppMeasure(child, depth + 1)) {
        return false;
      }
      // The raised sign glyph must cover the radicand's height; taller
      // radicands would need a synthesized sign (deferred past PP2).
      if(ppPool[child].ascent + ppPool[child].descent > m->radInk + 3) {
        return false;
      }
      ppPool[child].relX    = m->radAdvance;
      ppPool[child].relBase = 0;
      nd->width   = m->radAdvance + ppPool[child].width + m->overhang;
      nd->ascent  = ppPool[child].ascent + m->radGap + m->vincThick;
      nd->descent = ppPool[child].descent;
      if(m->radInk - nd->ascent > nd->descent) {
        nd->descent = m->radInk - nd->ascent;   // the raised sign's tail
      }
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

    case PP_RAD: {
      // Sign first (own columns), then the radicand, then the vinculum
      // LAST — the binding paint-order rule: glyph-box pre-clears wipe
      // any rule painted before their glyphs.
      int16_t vincTop = baseline - ppPool[nd->firstChild].ascent - m->radGap - m->vincThick;
      showString("\xa2\x1a", m->font, x, vincTop - m->radAbove, vmNormal, false, true);
      ppPaint(nd->firstChild, x + m->radAdvance, baseline);
      lcd_fill_rect(x + m->radAdvance - 1, vincTop,
                    nd->width - m->radAdvance + 1, m->vincThick, LCD_EMPTY_VALUE);
      return;
    }

    case PP_PAREN: {
      uint8_t child = nd->firstChild;
      ppPaint(child, x + ppPool[child].relX, baseline + ppPool[child].relBase);
      int16_t h = ppPool[child].ascent + ppPool[child].descent;
      if(h <= m->parInk + 2) {
        showString("(", m->font, x, baseline - m->boxAscent, vmNormal, false, true);
        showString(")", m->font, (int16_t)(x + nd->width - m->parAdvance),
                   baseline - m->boxAscent, vmNormal, false, true);
      }
      else {
        int16_t top = baseline - nd->ascent;
        int16_t hh  = nd->ascent + nd->descent;
        int16_t xr  = (int16_t)(x + nd->width - 5);
        lcd_fill_rect(x + 1,  top + 2, 2, hh - 4, LCD_EMPTY_VALUE);
        lcd_fill_rect(x + 2,  top,     2, 2,      LCD_EMPTY_VALUE);
        lcd_fill_rect(x + 2,  top + hh - 2, 2, 2, LCD_EMPTY_VALUE);
        lcd_fill_rect(xr + 2, top + 2, 2, hh - 4, LCD_EMPTY_VALUE);
        lcd_fill_rect(xr,     top,     2, 2,      LCD_EMPTY_VALUE);
        lcd_fill_rect(xr,     top + hh - 2, 2, 2, LCD_EMPTY_VALUE);
      }
      return;
    }

    case PP_SUP:
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


void ppSetFontDeep(uint8_t n, uint8_t fontId) {
  if(n == PP_NONE || n >= ppNodeCount) {
    return;
  }
  ppPool[n].fontId = fontId;
  for(uint8_t c = ppPool[n].firstChild; c != PP_NONE; c = ppPool[c].nextSibling) {
    ppSetFontDeep(c, fontId);
  }
}

void ppPaintAt(uint8_t root, int16_t x, int16_t baseline) {
  if(root < ppNodeCount && ppMetricsOk()) {
    ppPaint(root, x, baseline);
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
