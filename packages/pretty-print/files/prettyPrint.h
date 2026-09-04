// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyPrint.h
 * Pretty-print package: natural (textbook) display of calculations.
 *
 * This package contains all drawing logic. It draws register values for
 * stack lines and PSHOW. It draws two-dimensional equation surfaces for
 * strip and EQSHW. It also runs the VISUAL program walker.
 *
 * This header defines the public interface. Internal definitions are in
 * prettyInternal.h. The separate pretty-print-extra package (prettyExtra.h)
 * provides formula capture and formula history views.
 *
 * c47.h includes this header near the end of its include list. All C47
 * types are visible here. Do not include C47 headers in this file.
 *
 * Design rule: every pretty entry point is a try-function. The function
 * paints the pretty form and returns true. If drawing fails, it returns
 * false without painting. When it returns false, upstream code renders
 * the display.
 */

#if !defined(PRETTYPRINT_H)
#define PRETTYPRINT_H

// Natural-display master toggle (ITM_PPON, item row 460), persisted in
// FLAG_PRETTYP, default ON.
void   fnPrettyToggle(uint16_t unusedButMandatoryParameter);
bool_t prettyEnabled (void);

// PTLIN (ITM_PTLIN, row 462): opt-in live formula on the T register
// line, persisted in FLAG_PTLINE, default OFF. The command and both
// flags are this package's. The rendering is pretty-print-extra's,
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

// VISUAL (ITM_VISUAL, row 984): draws a stored RPN program as
// mathematical notation without program execution.
// The function accepts a global program label and creates an expression
// tree. It draws the tree in the Z and T registers. If the drawing is
// too tall, it draws in the full-screen band.
// The function declines if a static walk cannot represent an instruction.
// On decline, it raises an error without painting.
void fnPrettyVisual(uint16_t label);

// the generated menu_SYSFL row count, from the softmenu table. The
// flag-browser override bounds its walk with this.
int16_t prettySysflRows(void);

// Factory reset (doFnReset only: the lazy first-use paths must NOT
// restore flag defaults, PP15): restores FLAG_PRETTYP/FLAG_PTLINE
// defaults and runs the extra package's re-arm through
// ppResetExtension below.
void prettyReset(void);

/* Extension callbacks for the pretty-print-extra package (set in ppcInit).
 * Both pointers remain NULL when that package is not installed.
 * If a pointer is NULL, the caller skips the callback.
 * - ppTlineExtension: prettyTryRegisterLine calls this callback for
 *   REGISTER_T before value rendering. It draws the live formula and
 *   returns true. If it cannot draw the formula, it returns false.
 * - ppResetExtension: prettyReset calls this callback before it restores
 *   flag defaults. It re-arms the capture engine. A NULL pointer means
 *   the engine has not run yet. */
extern bool_t (*ppTlineExtension)(int16_t baseY, int16_t bandTop,
                                  int16_t bandBottom, int16_t *lineWidth);
extern void   (*ppResetExtension)(void);

// Resolve a function name. The renderer and evaluator share this logic
// with the program walker. Components agree on function names.
// Accepts a NUL-terminated string without an opening parenthesis.
// Defined in solver/equation.c near the function alias table.
int16_t ppEqFunctionItem(const char *name);

/* Test whether a string matches a construct name prefix followed by an open parenthesis.
 * Supports case-insensitive matching for lowercase and uppercase construct names.
 * Returns true if the prefix matches and s[len] is '(', or false otherwise. */
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
