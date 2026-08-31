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
// PP_BIGOP children: body, under-limit, over-limit (chain of 3).
// textOff stores the operator ITM id (no other free field on box
// nodes), and the paint pass picks the stroke glyph (Σ/∏/∫) from it.
// PP_RAD children: radicand, then an optional second child = the index
// (ⁿ√), above-left of the sign. PP_SUB mirrors PP_SUP downward
// (log_b). PP_BARS wraps its child in |absolute-value| strokes.
enum { PP_FONT_NUMERIC = 0, PP_FONT_STANDARD = 1, PP_FONT_TINY = 2 };

#define PP_NONE        0xFF
#define PP_POOL_NODES  72
#define PP_TEXT_BYTES  512
// The node pool is the real capacity bound. The depth cap only stops
// runaway recursion: legitimate shapes nest 8-9 boxes deep.
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

// prettyLayout.c: pools, metrics, measure, paint
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
 * stream finished formulas serialize into. Shared with the viewer and
 * the test drivers only. */

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
       PPN_RCL, PPN_CONST, PPN_OPAQUE, PPN_BIGOP, PPN_VAL2 };
// PPN_VAL2: a VAL whose payload exceeds one node continues into a
// continuation on child[0], the shape PPN_LIT/PPN_LIT2 already use.
// A complex34 is 32 bytes, so it fits in 16 + 16.
#define PPC_VAL_CAPACITY 32
// PPN_BIGOP: item = ITM id, pad[0..1] = label param (LE), payload = the
// step real34 (zeros for the integral), child[0]/child[1] = the limits.
// Its VALUE is the result the dispatch left in X, so chains continue.

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

// prettyEquation.c: EQN display-string -> 2D strip layout
bool_t ppqParse(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppqShowRender(const char *src);
void   ppqFitWithEllipsis(const char *src, char *out, uint16_t cap);   ///< fit a line to the screen, marking any cut
uint8_t ppqFrameIntegral(uint8_t eq);                    ///< ∫ with real ULIM/LLIM limits + d<var>, or bare PP_INT without them
uint8_t ppqFrameDerivative(uint8_t eq, bool_t second);   ///< d/dx (d²/dx²) framing

/* prettyFormula.c: the shared 2D node builders, used by the capture
 * viewer and the walker so precedence is decided in one place. Only
 * ADD and MUL need brackets: PP_SUP scopes itself. Call ppfPowBase for
 * every PP_SUP base. */
#define PPF_PREC_ADD  1
#define PPF_PREC_MUL  2
#define PPF_PREC_ATOM 3

uint8_t ppfRun    (const char *s, uint8_t fontId);
uint8_t ppfWrapIf (uint8_t node, int prec, int minPrec, uint8_t fontId);   ///< parens iff prec < minPrec
uint8_t ppfPowBase(uint8_t a, int aPrec, uint8_t fontId);   ///< a power's base: parens iff the base node is itself a PP_SUP
bool_t  ppfTextIsAtom(const char *s, uint16_t len);   ///< for NUMERIC leaf text only. A name is always an atom: judge a run that can hold one with ppfRunPrec
int     ppfRunPrec(uint8_t n);                        ///< PPF_PREC_ATOM or _ADD for a built PP_RUN, from its text
uint8_t ppfBuildOp1(uint16_t item, uint8_t a, int aPrec,
                    uint8_t ctxFont, uint8_t childFont, int *outPrec);
uint8_t ppfBuildOp2(uint16_t item, uint8_t a, int aPrec, uint8_t b, int bPrec,
                    uint8_t ctxFont, uint8_t childFont, int *outPrec);

// prettyEquation.c: node assembly, shared with the walker
enum { PPQ_BIG_SUM = 0, PPQ_BIG_PROD = 1, PPQ_BIG_DERIV = 2, PPQ_BIG_INTEG = 3 };
uint8_t ppqUnwrapParen(uint8_t n);
uint8_t ppqBuildBigop (uint8_t kind, uint16_t tag, uint8_t body,
                       uint8_t varTiny, uint8_t varCtx,
                       uint8_t fromN, uint8_t toN, uint8_t stepN,
                       bool_t secondOrder, uint8_t ctxFont);
uint8_t ppqBuildCall  (const char *name, uint16_t len, uint8_t arg, uint8_t font);

// prettyVisual.c: the walker's test seam: serializes the tree to
// equation-language text so a test can read or evaluate it. Not in the
// device build. The drawing path builds nodes.
#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)
bool_t ppvTranspile(uint16_t labelIdx, char *out, uint16_t cap,
                    uint8_t *reasonOut, uint16_t *stepOut);
bool_t ppvTestBuildNodes(uint16_t labelIdx, uint8_t ctxFont, uint8_t childFont,
                         uint8_t *rootOut, uint32_t *visitsOut);   ///< the tree the product paints
#endif // PC_BUILD || TESTSUITE_BUILD

// prettyFormula.c: display-time name decodes (best-effort, fall back)
void ppfVariableName(uint16_t varId, char *out);         ///< out cap >= 17; falls back to "x"

// prettyFormula.c: capture tree / token stream -> infix layout
bool_t ppfBuildCurrent(uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppfBuildEntry(const uint8_t *entry, uint8_t ctxFont, uint8_t childFont,
                     bool_t withResult, uint8_t *rootOut);

// viewer/test API: all lazy, no formatter runs at capture time
const ppcNode_t *ppcNodeAt(uint8_t n);
uint8_t          ppcCurrentFormulaRoot(void);
uint8_t          ppcHistoryCount(void);
const uint8_t   *ppcHistoryEntry(uint8_t idx, uint16_t *lenOut, uint16_t *seqOut);
void             ppcHistoryClear(void);
void             ppcShadowInvalidate(void);   ///< dispatch-bypassing mutations (browser recall)
uint8_t          ppcTestCurrentRaw(void);   ///< test only: the current root before the opaque screen
uint8_t          ppcTestSlotRaw(uint8_t k);   ///< test only: slot k's raw arena index
void             ppcTestDeinit(void);         ///< test only: re-arm the cold-start path
bool_t           ppfTestStagedSpelling(uint8_t dataType, uint8_t tag, const uint8_t *payload,
                                       uint8_t bytes, char *out, size_t cap);   ///< test only: a staged value's display spelling

// prettyFormula.c: the browser reuses the pager's packed row builder.
// canPan: the browser can scroll a row sideways, so it accepts any
// width. The pager cannot, so it refuses an over-wide row.
bool_t ppfBuildRow(uint8_t row, uint8_t haveCurrent, bool_t canPan, uint8_t *rootOut, int16_t *ascOut, int16_t *hOut);

// prettyValue.c: converters and the toggle's test hook
bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseExponent(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseIrfrac  (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseRealAny (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseComplex (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
void   prettySetEnabled(bool_t on);
void   prettySetTline(bool_t on);

#endif // !PRETTYINTERNAL_H
