// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyPrint.h
 * Pretty-print package: natural (textbook) display of calculations —
 * everything that DRAWS. Register values (inline lines, PSHOW), the
 * 2D equation surfaces (strip, EQSHW, the equation-language big
 * operators), and the VISUAL program walker. Public surface only; the
 * internals are in prettyInternal.h. The capture engine, the formula
 * history and its views live in the separate pretty-print-extra
 * package (prettyExtra.h), which requires this package.
 *
 * c47.h includes this header near the end of its include block, so
 * every c47 type is already visible. This header must not include c47
 * headers itself.
 *
 * Binding contract: every pretty entry point is a try-function. It
 * paints the pretty form and returns true, or it paints nothing and
 * returns false. On false the upstream rendering runs unchanged.
 */

#if !defined(PRETTYPRINT_H)
#define PRETTYPRINT_H

// Natural-display master toggle (ITM_PPON, item row 460), persisted in
// FLAG_PRETTYP, default ON.
void   fnPrettyToggle(uint16_t unusedButMandatoryParameter);
bool_t prettyEnabled (void);

// PTLIN (ITM_PTLIN, row 462): opt-in live formula on the T register
// line, persisted in FLAG_PTLINE, default OFF. The command and both
// flags are this package's; the rendering is pretty-print-extra's,
// through ppTlineExtension below.
void fnPrettyTlineToggle(uint16_t unusedButMandatoryParameter);

// Inline stack-line surface, hooked from _refreshRegisterLine ahead of
// the FLAG_FRACT arm. On success *lineWidth carries the painted width.
bool_t prettyTryRegisterLine(calcRegister_t regist, int16_t baseY, int16_t *lineWidth);

// PSHOW (ITM_PSHOW, item row 459): full-screen pretty view of X on the
// fnPixel manual-paint protocol. Falls back to fnC47Show.
void fnPrettyShow(uint16_t unusedButMandatoryParameter);

// EQN strip 2D rendering, hooked at showEquation's paint site (never
// while editing). False -> upstream's linear showString runs.
bool_t prettyTryEquation(const char *src, int16_t xLeft);

// EQSHW (ITM_EQSHW, row 461): full-screen equation view. In the
// interactive integrate solver the integrand is framed by a big ∫.
void fnPrettyEqShow(uint16_t unusedButMandatoryParameter);

// VISUAL (ITM_VISUAL, row 984): draws a stored RPN program as the
// mathematics it computes, without running it. Takes a global program
// label, builds the expression tree, and paints it in the Z/T window,
// or in the full band when the drawing is too tall. Declines (raises
// an error, paints nothing) on anything a static walk cannot express.
void fnPrettyVisual(uint16_t label);

// the generated menu_SYSFL row count, from the softmenu table; the
// flag-browser override bounds its walk with this
int16_t prettySysflRows(void);

// Factory reset (doFnReset only — the lazy first-use paths must NOT
// restore flag defaults, PP15): restores FLAG_PRETTYP/FLAG_PTLINE
// defaults and runs the extra package's re-arm through
// ppResetExtension below.
void prettyReset(void);

/* Extension points the pretty-print-extra package fills in at its own
 * lazy init (ppcInit). Both stay NULL when that package is absent or
 * has not run yet, and a NULL extension is simply skipped:
 * - ppTlineExtension: called by prettyTryRegisterLine for REGISTER_T
 *   before the value rendering; draws the live formula and returns
 *   true, or returns false to fall through. Registration timing is
 *   sound because no formula can exist before the first capture hook
 *   runs, and that hook registers first.
 * - ppResetExtension: called by prettyReset before the flag defaults;
 *   re-arms the capture engine. NULL means the engine never ran, so
 *   there is nothing to re-arm. */
extern bool_t (*ppTlineExtension)(int16_t baseY, int16_t bandTop,
                                  int16_t bandBottom, int16_t *lineWidth);
extern void   (*ppResetExtension)(void);

// Function-name resolution, shared by the renderer, the walker and
// the evaluator so all three agree on what a function name is. Takes
// a bare NUL-terminated name, no '(' check. Defined in
// solver/equation.c beside the alias table it mirrors.
int16_t ppEqFunctionItem(const char *name);

static inline bool_t ppEqConstructIs(const char *s, const char *name, uint8_t len) {
  if(s[len] != '(') {
    return false;
  }
  const bool_t lower = (s[0] == (char)(name[0] + 32));
  if(!lower && s[0] != name[0]) {
    return false;
  }
  for(uint8_t i = 1; i < len; i++) {
    if(s[i] != (lower ? (char)(name[i] + 32) : name[i])) {
      return false;
    }
  }
  return true;
}

// testSuite coverage drivers (prettyTest.c, PC_BUILD only, registered
// in funcTestNoParam with coverageDriver = 1)
void prettyTestMeasure (uint16_t unusedButMandatoryParameter);
void prettyTestPixels  (uint16_t unusedButMandatoryParameter);
void prettyTestFallback(uint16_t unusedButMandatoryParameter);
void prettyTestShow    (uint16_t unusedButMandatoryParameter);
void prettyTestEquation(uint16_t unusedButMandatoryParameter);
void prettyTestVisual  (uint16_t unusedButMandatoryParameter);
void prettyTestReal    (uint16_t unusedButMandatoryParameter);

#endif // !PRETTYPRINT_H
