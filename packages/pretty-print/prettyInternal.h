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

enum { PP_RUN = 0, PP_HBOX = 1, PP_FRAC = 2, PP_RAD = 3, PP_SUP = 4, PP_PAREN = 5,
       PP_SUB = 6, PP_BARS = 7, PP_INT = 8, PP_BIGOP = 9 };
// PP_BIGOP (PP12): children = body, under-limit, over-limit (chain of 3);
// textOff stores the operator ITM id (no other free field on box nodes) —
// the paint pass picks the stroke glyph (Σ/∏/∫) from it.
// PP_RAD children: radicand, then an OPTIONAL second child = the index
// (ⁿ√), tucked above-left of the sign. PP_SUB mirrors PP_SUP downward
// (log_b). PP_BARS wraps its child in |absolute-value| strokes.
enum { PP_FONT_NUMERIC = 0, PP_FONT_STANDARD = 1, PP_FONT_TINY = 2 };

#define PP_NONE        0xFF
#define PP_POOL_NODES  72
#define PP_TEXT_BYTES  512
// 12, not 6: an integral wrapping a derivative of a big-operator
// fraction legitimately nests 8-9 boxes deep; the node pool is the
// real capacity bound, the depth cap only stops runaway recursion
#define PP_MAX_DEPTH   12

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
void            ppSetBoxTag(uint8_t n, uint16_t tag);   ///< PP_BIGOP: stores the operator ITM id in textOff
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
// A literal leaf stores its text as a head payload plus one LIT2
// continuation, 15 bytes each. Anything longer degrades to a value leaf.
#define PPC_LIT_CAPACITY 30

enum { PPN_FREE = 0, PPN_OP1, PPN_OP2, PPN_LIT, PPN_LIT2, PPN_VAL,
       PPN_RCL, PPN_CONST, PPN_OPAQUE, PPN_BIGOP };
// PPN_BIGOP (PP12): a captured Σₙ/∏ₙ/∫YX dispatch. item = the ITM id,
// pad[0..1] = the label param (LE), payload = the step real34 (sums;
// zeros for ∫), child[0] = from-limit VAL, child[1] = to-limit VAL.
// Its VALUE is the result the dispatch left in X — chains continue.

// postfix stream tokens (history entries: 6-byte header {totalBytes u16,
// seq u16, nTokens u8, flags u8} then tokens; TKRES trails when present)
enum { PPT_TKL = 1,    // literal: len u8, text bytes (as typed)
       PPT_TKV,        // value: dataType u8, tag u8, allocParam u16, len u8, payload
       PPT_TKR,        // register/variable leaf: param u16
       PPT_TKC,        // constant: item u16
       PPT_TKO1,       // monadic op: item u16
       PPT_TKO2,       // dyadic op: item u16
       PPT_TKRES,      // result snapshot: same shape as TKV
       PPT_TKBIG };    // big operator: item u16, label u16, step real34(16B); pops from,to

typedef struct {
  uint8_t  kind;        ///< PPN_*
  uint8_t  aux;         ///< LIT/LIT2: text length; VAL: dataType; OP: PPA_* flags
  uint16_t item;        ///< OP/CONST: item id; RCL: param; VAL: allocParam
  uint8_t  child[2];    ///< arena indices; LIT: child[0] = continuation; free list via child[0]
  uint8_t  pad[2];      ///< VAL: tag, payloadBytes
  uint8_t  payload[16]; ///< LIT text / VAL raw register payload
} ppcNode_t;

// prettyEquation.c — EQN display-string -> 2D strip layout
bool_t ppqParse(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppqShowRender(const char *src);
uint8_t ppqFrameIntegral(uint8_t eq);                    ///< PP13: ∫ with real ULIM/LLIM limits + d<var>; bare PP_INT without them
uint8_t ppqFrameDerivative(uint8_t eq, bool_t second);   ///< PP13: d/dx (d²/dx²) framing

/* prettyFormula.c — the shared 2D node builders. These were the capture
 * engine's private back end until PP18; the program walker now feeds them
 * too, so that precedence and parenthesisation are decided in ONE place
 * instead of once per front-end. There is deliberately no POW level: a
 * PP_SUP scopes itself geometrically, so only ADD and MUL ever need
 * brackets. A caller stacking powers must bracket the base itself. */
#define PPF_PREC_ADD  1
#define PPF_PREC_MUL  2
#define PPF_PREC_ATOM 3

uint8_t ppfRun    (const char *s, uint8_t fontId);
uint8_t ppfWrapIf (uint8_t node, int prec, int minPrec, uint8_t fontId);   ///< parens iff prec < minPrec
uint8_t ppfCombine1(uint16_t item, uint8_t a, int aPrec,
                    uint8_t ctxFont, uint8_t childFont, int *outPrec);
uint8_t ppfCombine2(uint16_t item, uint8_t a, int aPrec, uint8_t b, int bPrec,
                    uint8_t ctxFont, uint8_t childFont, int *outPrec);

// prettyEquation.c — node assembly, shared with the walker (PP18). Both
// keep the shapes the EQ pins fix; only the PARSING stayed behind.
enum { PPQ_BIG_SUM = 0, PPQ_BIG_PROD = 1, PPQ_BIG_DERIV = 2, PPQ_BIG_INTEG = 3 };
uint8_t ppqUnwrapParen(uint8_t n);
uint8_t ppqBuildBigop (uint8_t kind, uint16_t tag, uint8_t body,
                       uint8_t varTiny, uint8_t varCtx,
                       uint8_t fromN, uint8_t toN, uint8_t stepN,
                       bool_t secondOrder, uint8_t ctxFont);
uint8_t ppqBuildCall  (const char *name, uint16_t len, uint8_t arg, uint8_t font);

// prettyVisual.c — the walker's TEST seam: serializes the expression
// tree it built to equation-language text, so a pin can assert something
// readable and one pin can evaluate it. Not in the device build — the
// drawing path builds nodes and never makes text.
#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)
bool_t ppvTranspile(uint16_t labelIdx, char *out, uint16_t cap,
                    uint8_t *reasonOut, uint16_t *stepOut);
bool_t ppvTestBuildNodes(uint16_t labelIdx, uint8_t ctxFont, uint8_t childFont,
                         uint8_t *rootOut);   ///< the tree the product paints
#endif // PC_BUILD || TESTSUITE_BUILD

// prettyFormula.c — display-time name decodes (best-effort, fall back)
void ppfVariableName(uint16_t varId, char *out);         ///< out cap >= 17; falls back to "x"

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
void             ppcShadowInvalidate(void);   ///< dispatch-bypassing mutations (browser recall)
uint8_t          ppcTestCurrentRaw(void);   ///< test only: the current root BEFORE the opaque screen, so a pin can tell a truthful degradation from a total invalidation
void             ppcTestDeinit(void);         ///< test only: re-arm the cold-start path so FV16 can prove lazy init leaves the FLAGS alone

// prettyFormula.c — the browser reuses the pager's packed row builder
// canPan: the BROWSER can scroll a row sideways, so it accepts any
// width; the full-screen pager cannot, and for it an over-wide row must
// be refused rather than silently clipped (AUDIT R2-2).
bool_t ppfBuildRow(uint8_t row, uint8_t haveCurrent, bool_t canPan, uint8_t *rootOut, int16_t *ascOut, int16_t *hOut);

// prettyValue.c — converters and the toggle's test hook
bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseExponent(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseIrfrac  (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseRealAny (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseComplex (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
void   prettySetEnabled(bool_t on);
void   prettySetTline(bool_t on);

#endif // !PRETTYINTERNAL_H
