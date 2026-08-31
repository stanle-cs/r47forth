// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyPrint.h
 * Pretty-print package: natural (textbook) display of calculations.
 * Public surface only. The engine internals are in prettyInternal.h.
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

// Inline stack-line surface, hooked from _refreshRegisterLine ahead of
// the FLAG_FRACT arm. On success *lineWidth carries the painted width.
bool_t prettyTryRegisterLine(calcRegister_t regist, int16_t baseY, int16_t *lineWidth);

// PSHOW (ITM_PSHOW, item row 459): full-screen pretty view of X on the
// fnPixel manual-paint protocol. Falls back to fnC47Show.
void fnPrettyShow(uint16_t unusedButMandatoryParameter);

// PHIST (ITM_PHIST, row 462): opens the formula browser (calcMode 20:
// UP/DOWN select, .d pans a wide row, ENTER recalls the result to X,
// EXIT leaves). PCLR (ITM_PCLR, row 461) clears the formula history.
void fnPrettyHist     (uint16_t unusedButMandatoryParameter);
void fnPrettyHistClear(uint16_t unusedButMandatoryParameter);

// browser handlers, called from the keyboard.c / screen.c hooks
void prettyBrowser     (uint16_t unusedButMandatoryParameter);
void prettyBrowserUp   (void);
void prettyBrowserDown (void);
void prettyBrowserPan  (void);
void prettyBrowserEnter(void);
void prettyBrowserLeave(void);

// EQN strip 2D rendering, hooked at showEquation's paint site (never
// while editing). False -> upstream's linear showString runs.
bool_t prettyTryEquation(const char *src, int16_t xLeft);

// EQSHW (ITM_EQSHW, row 216): full-screen equation view. In the
// interactive integrate solver the integrand is framed by a big ∫.
void fnPrettyEqShow(uint16_t unusedButMandatoryParameter);

// VISUAL (ITM_VISUAL, row 984): draws a stored RPN program as the
// mathematics it computes, without running it. Takes a global program
// label, builds the expression tree, and paints it in the Z/T window,
// or in the full band when the drawing is too tall. Declines (raises
// an error, paints nothing) on anything a static walk cannot express.
void fnPrettyVisual(uint16_t label);

// PTLIN (ITM_PTLIN, row 215): opt-in live formula on the T register
// line, default OFF. Falls through to T's value when no formula fits.
void fnPrettyTlineToggle(uint16_t unusedButMandatoryParameter);

// Capture-engine hooks (prettyCapture.c), called from small upstream
// patches. STAGE/DONE bracket the item dispatch in reallyRunFunction.
// The NIM trio mirrors number entry at the closeNim funnel, with the
// lift decision latched at calcModeNim. prettyReset re-arms at
// doFnReset.
void prettyNoteFunction    (int16_t func, uint16_t param);
void prettyNoteFunctionDone(void);
/* For upstream sites that mutate registers without item dispatch, so
 * neither hook above runs: wipe the shadow to UNKNOWN without touching
 * the history ring. The direct fnRecall calls in keyboard.c are the
 * callers this is public for. */
void ppcShadowInvalidate   (void);
void prettyNoteNimOpen     (void);
void prettyNoteNimText     (const char *aim);
void prettyNoteNumberCommit(void);
void prettyReset           (void);

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
void prettyTestCapture (uint16_t unusedButMandatoryParameter);
void prettyTestFormula (uint16_t unusedButMandatoryParameter);
void prettyTestEquation(uint16_t unusedButMandatoryParameter);
void prettyTestVisual  (uint16_t unusedButMandatoryParameter);
void prettyTestReal    (uint16_t unusedButMandatoryParameter);

#endif // !PRETTYPRINT_H
