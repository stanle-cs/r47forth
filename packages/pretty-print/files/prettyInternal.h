// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyInternal.h
 * Pretty-print engine internals, shared between the package's own
 * translation units (prettyLayout.c, prettyValue.c, prettyTest.c).
 * The pretty-print-extra package's sources include this header too:
 * the layout model below is the contract its builders target.
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
// Only the extra package's builders produce PP_SUB, PP_BARS, PP_INT
// and PP_BIGOP; the paint arms for them live here in the engine.
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

// prettyValue.c: converters and the toggles' test hooks
bool_t ppParseFraction(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseExponent(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseIrfrac  (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseRealAny (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppParseComplex (const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
void   prettySetEnabled(bool_t on);
void   prettySetTline(bool_t on);

/* prettyInfix.c: the shared 2D node builders, used by the equation
 * renderer, the program walker and pretty-print-extra's capture viewer
 * so precedence is decided in one place. Only ADD and MUL need
 * brackets: PP_SUP scopes itself. Call ppfPowBase for every PP_SUP
 * base. */
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

// prettyInfix.c: display-time name decodes (best-effort, fall back)
void ppfVariableName(uint16_t varId, char *out);         ///< out cap >= 17; falls back to "x"
void ppfLabelName(uint16_t param, char *out);            ///< out cap >= 17; falls back to "LBL nn"

// prettyEquation.c: EQN display-string -> 2D strip layout
bool_t ppqParse(const char *src, uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppqShowRender(const char *src);
void   ppqFitWithEllipsis(const char *src, char *out, uint16_t cap);   ///< fit a line to the screen, marking any cut
uint8_t ppqFrameIntegral(uint8_t eq);                    ///< ∫ with real ULIM/LLIM limits + d<var>, or bare PP_INT without them
uint8_t ppqFrameDerivative(uint8_t eq, bool_t second);   ///< d/dx (d²/dx²) framing

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

#if defined(PC_BUILD)
/* Shared test scaffolding, defined in prettyTest.c. The extra
 * package's test driver (prettyExtraTest.c) links against these, so
 * they are non-static and declared here rather than per-file. */
// the X-line band ppTestCapture snapshots
#define PPT_BAND_TOP   (Y_POSITION_OF_REGISTER_X_LINE - 4)
#define PPT_BAND_ROWS  43
extern uint32_t ppTestFailures;
void   ppTestFail(const char *what);
void   ppTestFailInt(const char *what, int32_t expected, int32_t actual);
void   ppTestWriteLonI(calcRegister_t regist, uint32_t value);
void   ppTestSetRealX(const char *value);
bool_t ppTestIsLonI(calcRegister_t regist, uint32_t expected);
void   ppTestClearBand(void);
bool_t ppTestRowAllLit(uint32_t row, uint32_t x0, uint32_t x1);
bool_t ppTestRowAnyLit(uint32_t row, uint32_t x0, uint32_t x1);
bool_t ppTestRectAnyLit(uint32_t r0, uint32_t r1, uint32_t x0, uint32_t x1);
void   ppTestCaptureBand(int which, uint32_t top, uint32_t rows);
void   ppTestCapture(int which);
bool_t ppTestSnapsEqual(void);
// program-fixture loader + layout-signature decoders, shared the same
// way (the prefixes name their subject, not their home)
void        ppcTestWriteAndLoadPgm(const uint8_t *pgm, size_t n);
uint32_t    ppvSumRows(int16_t top, int16_t bottom);
void        ppfTestSigNode(uint8_t n, char *out, size_t cap);
void        ppfTestExpect(const char *what, uint8_t root, const char *expected);
bool_t      ppfTestPowersScoped(uint8_t n);
bool_t      ppfTestRunEndsSup(uint8_t n);
bool_t      ppTreeHasRun(uint8_t n, const char *text);
#endif // PC_BUILD

#endif // !PRETTYINTERNAL_H
