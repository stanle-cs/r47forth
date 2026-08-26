// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyInternal.h
 * Pretty-print engine internals, shared between the package's own
 * translation units (prettyLayout.c, prettyValue.c, prettyTest.c) only.
 * Include after c47.h. Not part of the public surface.
 *
 * Layout model: a node tree measured then painted. For a node whose
 * baseline is screen row B, ink occupies rows [B - ascent, B + descent - 1];
 * a plain digit has descent 0. Child positions are relative: relX from the
 * parent's left edge, relBase from the parent's baseline.
 */

#if !defined(PRETTYINTERNAL_H)
#define PRETTYINTERNAL_H

enum { PP_RUN = 0, PP_HBOX = 1, PP_FRAC = 2 };
enum { PP_FONT_NUMERIC = 0, PP_FONT_STANDARD = 1, PP_FONT_TINY = 2 };

#define PP_NONE        0xFF
#define PP_POOL_NODES  48
#define PP_TEXT_BYTES  512
#define PP_MAX_DEPTH   6

typedef struct {
  uint8_t  kind;         ///< PP_RUN / PP_HBOX / PP_FRAC
  uint8_t  fontId;       ///< metric context of this node (PP_FONT_*)
  uint8_t  firstChild;   ///< pool index or PP_NONE
  uint8_t  nextSibling;  ///< pool index or PP_NONE
  uint16_t textOff;      ///< PP_RUN only: offset of the NUL-terminated run in the text pool
  int16_t  width;        ///< measure output, px
  int16_t  ascent;       ///< measure output: ink rows strictly above baseline
  int16_t  descent;      ///< measure output: ink rows at/below baseline
  int16_t  relX;         ///< layout output: x offset from parent left edge
  int16_t  relBase;      ///< layout output: child baseline minus parent baseline
} ppNode_t;

// prettyLayout.c — pools, metrics, measure, paint
void            ppReset(void);
uint8_t         ppNewBox(uint8_t kind, uint8_t fontId);
uint8_t         ppNewRun(const char *bytes, uint16_t len, uint8_t fontId);  ///< copies + NUL-terminates; PP_NONE on overflow
void            ppAppendChild(uint8_t parent, uint8_t child);
bool_t          ppMeasure(uint8_t n, uint8_t depth);
const ppNode_t *ppNodeAt(uint8_t n);
const char     *ppTextAt(uint16_t off);
bool_t          ppRenderRightAligned(uint8_t root, int16_t xRight,
                                     int16_t bandTop, int16_t bandBottom,
                                     int16_t preferredBase);
int16_t         ppPreferredBase(int16_t baseY);   ///< baseY + numericFont box ascent

// prettyValue.c — converters and the toggle's test hook
bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
void   prettySetEnabled(bool_t on);

#endif // !PRETTYINTERNAL_H
