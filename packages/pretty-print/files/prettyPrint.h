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

// testSuite coverage drivers (prettyTest.c, PC_BUILD only; registered in
// funcTestNoParam with coverageDriver = 1).
void prettyTestMeasure (uint16_t unusedButMandatoryParameter);
void prettyTestPixels  (uint16_t unusedButMandatoryParameter);
void prettyTestFallback(uint16_t unusedButMandatoryParameter);

#endif // !PRETTYPRINT_H
