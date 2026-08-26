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

enum { PP_RUN = 0, PP_HBOX = 1, PP_FRAC = 2, PP_RAD = 3, PP_SUP = 4, PP_PAREN = 5 };
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
void            ppPaintAt(uint8_t root, int16_t x, int16_t baseline);
void            ppSetFontDeep(uint8_t n, uint8_t fontId);
int16_t         ppPreferredBase(int16_t baseY);   ///< baseY + numericFont box ascent

/* ==== capture engine (prettyCapture.c) ==================================
 * The shadow expression stack's arena, node kinds, and the postfix token
 * stream finished formulas serialize into. Shared with the viewer (PP4)
 * and the test drivers only. */

#define PPC_NODES      24
#define PPC_NIL        0xFF
#define PPC_UNKNOWN    0xFE
#define PPC_HIST_BYTES 640
#define PPC_HIST_MAX   12
#define PPA_EMITTED    0x01

enum { PPN_FREE = 0, PPN_OP1, PPN_OP2, PPN_LIT, PPN_LIT2, PPN_VAL,
       PPN_RCL, PPN_CONST, PPN_OPAQUE };

// postfix stream tokens (history entries: 6-byte header {totalBytes u16,
// seq u16, nTokens u8, flags u8} then tokens; TKRES trails when present)
enum { PPT_TKL = 1,    // literal: len u8, text bytes (as typed)
       PPT_TKV,        // value: dataType u8, tag u8, allocParam u16, len u8, payload
       PPT_TKR,        // register/variable leaf: param u16
       PPT_TKC,        // constant: item u16
       PPT_TKO1,       // monadic op: item u16
       PPT_TKO2,       // dyadic op: item u16
       PPT_TKRES };    // result snapshot: same shape as TKV

typedef struct {
  uint8_t  kind;        ///< PPN_*
  uint8_t  aux;         ///< LIT/LIT2: text length; VAL: dataType; OP: PPA_* flags
  uint16_t item;        ///< OP/CONST: item id; RCL: param; VAL: allocParam
  uint8_t  child[2];    ///< arena indices; LIT: child[0] = continuation; free list via child[0]
  uint8_t  pad[2];      ///< VAL: tag, payloadBytes
  uint8_t  payload[16]; ///< LIT text / VAL raw register payload
} ppcNode_t;

// prettyEquation.c — EQN display-string -> 2D strip layout
bool_t ppqParse(const char *src, uint8_t *rootOut);

// prettyFormula.c — capture tree / token stream -> infix layout
bool_t ppfBuildCurrent(uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppfBuildEntry(const uint8_t *entry, uint8_t ctxFont, uint8_t childFont,
                     bool_t withResult, uint8_t *rootOut);

// viewer/test API — all lazy, no formatter runs at capture time
const ppcNode_t *ppcNodeAt(uint8_t n);
uint8_t          ppcCurrentFormulaRoot(void);
uint8_t          ppcHistoryCount(void);
const uint8_t   *ppcHistoryEntry(uint8_t idx, uint16_t *lenOut, uint16_t *seqOut);
void             ppcHistoryClear(void);

// prettyValue.c — converters and the toggle's test hook
bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseExponent(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseIrfrac  (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseRealAny (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseComplex (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
void   prettySetEnabled(bool_t on);

#endif // !PRETTYINTERNAL_H
