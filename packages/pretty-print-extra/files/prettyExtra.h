// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyExtra.h
 * Pretty-print-extra package. It contains the calculation-capture
 * engine and formula history. Its views are the pager, browser, and
 * T line. The pretty-print core package supplies the drawing engine.
 * This file is the public surface. The internals are in
 * prettyExtraInternal.h.
 * This package REQUIRES the pretty-print package. It decodes capture
 * trees with the core builders. Its lazy initialization registers the
 * core ppTlineExtension and ppResetExtension hooks.
 *
 * c47.h includes this header inside its include block, so every c47
 * type is already visible. This header must not include c47 headers
 * itself.
 *
 * Binding contract: every pretty entry point is a try-function. It
 * paints the pretty form and returns true, or it paints nothing and
 * returns false. On false the upstream rendering runs unchanged.
 */

#if !defined(PRETTYEXTRA_H)
#define PRETTYEXTRA_H

// PHIST (ITM_PHIST, item row 216): opens the formula browser
// (calcMode 20: UP/DOWN select, .d pans a wide row, ENTER recalls the
// result to X, EXIT leaves). PCLR (ITM_PCLR, row 215) clears the
// formula history.
void fnPrettyHist     (uint16_t unusedButMandatoryParameter);
void fnPrettyHistClear(uint16_t unusedButMandatoryParameter);

// browser handlers, called from the keyboard.c / screen.c hooks
void prettyBrowser     (uint16_t unusedButMandatoryParameter);
void prettyBrowserUp   (void);
void prettyBrowserDown (void);
void prettyBrowserPan  (void);
void prettyBrowserEnter(void);
void prettyBrowserLeave(void);

// Capture-engine hooks (prettyCapture.c), called from small upstream
// patches. STAGE/DONE bracket the item dispatch in reallyRunFunction.
// The NIM trio mirrors number entry at the closeNim funnel, with the
// lift decision latched at calcModeNim.
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

// testSuite coverage drivers (prettyExtraTest.c, PC_BUILD only,
// registered in funcTestNoParam with coverageDriver = 1)
void prettyTestCapture (uint16_t unusedButMandatoryParameter);
void prettyTestFormula (uint16_t unusedButMandatoryParameter);
void prettyTestEqLang  (uint16_t unusedButMandatoryParameter);

#endif // !PRETTYEXTRA_H
