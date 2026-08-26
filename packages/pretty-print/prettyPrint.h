// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyPrint.h
 * Pretty-print package: natural (textbook) display of calculations.
 * Public surface only; the engine internals are in prettyInternal.h.
 *
 * Included from c47.h near the end of its include block, so every c47 type
 * is already visible; this header must not include c47 headers itself.
 *
 * The one binding contract callers rely on: every pretty entry point is a
 * try-function. It either paints the pretty form and returns true, or
 * paints NOTHING and returns false — in which case the upstream rendering
 * runs unchanged. There is no error path, only decline.
 */

#if !defined(PRETTYPRINT_H)
#define PRETTYPRINT_H

// Package toggle (ITM_PPON, item row 460). Not a system flag: two packages
// cannot both edit the NUMBER_OF_SYSTEM_FLAGS line (see DESIGN.md §7), so
// the toggle is package state, default ON, not persisted.
void   fnPrettyToggle(uint16_t unusedButMandatoryParameter);
bool_t prettyEnabled (void);

// Inline stack-line surface, hooked from _refreshRegisterLine ahead of the
// FLAG_FRACT arm. On success *lineWidth carries the painted width (the arm
// contract every upstream branch honours via `lineWidth = w`).
bool_t prettyTryRegisterLine(calcRegister_t regist, int16_t baseY, int16_t *lineWidth);

// PSHOW (ITM_PSHOW, item row 459): full-screen pretty view of X on the
// fnPixel manual-paint protocol; falls back to fnC47Show.
void fnPrettyShow(uint16_t unusedButMandatoryParameter);

// PHIST (ITM_PHIST, row 462): opens the formula BROWSER (calcMode 20 —
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

// EQSHW (ITM_EQSHW, row 216): full-screen equation view; in the
// interactive integrate solver the integrand is framed by a big ∫.
void fnPrettyEqShow(uint16_t unusedButMandatoryParameter);

// PTLIN (ITM_PTLIN, row 215): opt-in live formula on the T register
// line — DEFAULT OFF; falls through to T's value when no formula fits.
void fnPrettyTlineToggle(uint16_t unusedButMandatoryParameter);

// Capture-engine hooks (prettyCapture.c), called from small upstream
// patches. STAGE/DONE bracket the item dispatch in reallyRunFunction;
// the NIM trio mirrors number entry at the closeNim funnel with the
// lift decision latched at calcModeNim; prettyReset re-arms at doFnReset.
void prettyNoteFunction    (int16_t func, uint16_t param);
void prettyNoteFunctionDone(void);
void prettyNoteNimOpen     (void);
void prettyNoteNimText     (const char *aim);
void prettyNoteNumberCommit(void);
void prettyReset           (void);

// testSuite coverage drivers (prettyTest.c, PC_BUILD only; registered in
// funcTestNoParam with coverageDriver = 1).
void prettyTestMeasure (uint16_t unusedButMandatoryParameter);
void prettyTestPixels  (uint16_t unusedButMandatoryParameter);
void prettyTestFallback(uint16_t unusedButMandatoryParameter);
void prettyTestShow    (uint16_t unusedButMandatoryParameter);
void prettyTestCapture (uint16_t unusedButMandatoryParameter);
void prettyTestFormula (uint16_t unusedButMandatoryParameter);
void prettyTestEquation(uint16_t unusedButMandatoryParameter);

#endif // !PRETTYPRINT_H
