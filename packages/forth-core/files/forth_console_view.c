// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

#include "c47.h"
#include "forth_capture.h"
#include "forth_console.h"

/* ================================================================
 * N1-2 — the console view (Stage N, N-R3; geometry from N-T1).
 *
 * While an interactive Forth capture is open the console owns the whole
 * stack area: the native AIM editor draws its line where it always did
 * (refreshRegisterLine(REGISTER_X), untouched) and the transcript fills
 * the rows the editor leaves above it, newest just above the input,
 * older rolling upward.
 * ================================================================ */

#define FORTH_CONSOLE_ROW_PITCH   21   /* the landed fnPem listing pitch
                                          (manage.c:546-551), not
                                          STANDARD_FONT_HEIGHT (22, the font
                                          browser's metric) */

/* CONSOLIDATE P7: extracted from the screen.c override, byte-identical
 * except for the linkage changes named in the P7 commit.  Only
 * _forthConsoleEditorTop() stayed behind — it reads checkHPoffset, a
 * screen.c-local macro, which is the one coupling this move could not
 * carry (it is declared in forth_console.h and called from here).
 * FORTH_CONSOLE_ROW_PITCH is above; the two editor-geometry macros live in
 * forth_console.h because that seam needs them too. */

/* C14's actual enforcement: the band's row counts are what Stage N's
 * N-T1 geometry was designed and tested against (4 short, 2 long).  Tie
 * them to UPSTREAM's names, so that moving a register line or the NIM
 * line upstream breaks this build instead of silently repainting the
 * transcript over the editor.  checkHPoffset is a runtime term and cannot
 * appear here; the default layout (checkHPoffset == 0) is what is
 * asserted, which is the layout every landed test drives. */
_Static_assert((Y_POSITION_OF_NIM_LINE - 3 - FORTH_CONSOLE_ED_CLEAR
                - Y_POSITION_OF_REGISTER_T_LINE) / FORTH_CONSOLE_ROW_PITCH == 4,
               "C14: the short-line band is no longer 4 rows — upstream's "
               "Y_POSITION_OF_NIM_LINE/REGISTER_T_LINE geometry moved, and "
               "N-T1's layout must be re-derived, not re-fitted");
_Static_assert(((FORTH_CONSOLE_ED_YINCR - 1) * 2 - FORTH_CONSOLE_ED_CLEAR
                - Y_POSITION_OF_REGISTER_T_LINE) / FORTH_CONSOLE_ROW_PITCH == 2,
               "C14: the long-line band is no longer 2 rows — showStringEdC47's "
               "yincr or upstream's REGISTER_T_LINE moved");

/* C12 (owner ruling 2026-08-08): rows is a VIEW concept the ring module
 * deliberately does not have, so the view owns both the row count and the
 * roll's upper bound. */
uint16_t forthConsoleViewRows(void) {
  uint16_t editorTop = _forthConsoleEditorTop();
  if(editorTop <= Y_POSITION_OF_REGISTER_T_LINE) { return 0; }
  return (uint16_t)((editorTop - Y_POSITION_OF_REGISTER_T_LINE) / FORTH_CONSOLE_ROW_PITCH);
}

/* C12's bound, in one place: the largest view offset that still fills the
 * band.  AUDIT round 8 (C-3) extracted it because the roll is no longer
 * its only enforcer — see _forthConsoleClampView. */
static uint16_t _forthConsoleMaxView(uint16_t rows, uint16_t count) {
  return (count > rows) ? (uint16_t)(count - rows) : 0;
}

/* AUDIT round 8 (C-3; owner ruling 2026-08-08: enforce at render time).
 * C12 clamped where the offset is WRITTEN, which is correct only while
 * the bound is constant — and it is not: rows is frame-variable, because
 * the editor's long-line state halves the band from 4 rows to 2.  An
 * offset clamped legally at count-2 with a long line becomes illegal the
 * moment BACKSPACE shortens the line, no roll key pressed and nothing to
 * re-clamp it; the renderer's only guard is `view >= count -> continue`,
 * so the top rows painted nothing.  That is C12's own symptom returning
 * through the one variable N-R3 does not govern.
 *
 * SHAPE (revised 2026-08-08 under the standing rule: follow upstream's
 * convention where one exists).  Upstream has this exact class — a
 * display window index that can outrun its content — and solves it in two
 * parts, neither of which is a write during the paint:
 *
 *   - the WRITE clamp lives at the scroll site: scrollPemBackwards and
 *     scrollPemForwards guard before they move
 *     (src/c47/programming/manage.c:432-446);
 *   - the paint TOLERATES an out-of-range index instead of rewriting it:
 *     defineFirstDisplayedStep walks to the window and simply breaks if
 *     the content runs out (src/c47/programming/nextStep.c:548-560).
 *
 * So the write clamp below belongs to forthConsoleRollView — the roll IS
 * this band's scroll site, and C12 already had it in the right place —
 * and the renderer clamps a LOCAL copy for the frame
 * (_forthConsoleViewBase).  The owner's ruling is unchanged: the band is
 * full on every frame.  What changes is that a paint no longer writes
 * state, and a stored offset means "where the owner left the view", not
 * "where the last repaint happened to leave it". */
static void _forthConsoleClampView(uint16_t rows, uint16_t count) {
  uint16_t maxView;
  if(rows == 0 || count == 0) { return; }
  maxView = _forthConsoleMaxView(rows, count);
  if(forthConsoleViewOffset() > maxView) {
    forthConsoleSetViewOffset(maxView);
  }
}

/* The offset THIS frame paints from: the stored one, clamped to the
 * frame's own bound.  Pure — see the shape note above.
 *
 * Exported to the self-test build only (the keyboard.c executeFunction
 * precedent) so N1-2's geometry and roll cases can drive the paint offset
 * directly, without the menus, status bar and screenUpdatingMode arithmetic a
 * full refreshScreen() drags in.  The ARM being wired is proven separately,
 * through refreshScreen(), by test_console_view_arm.  The gate and the paint
 * below need no such macro since P7: screen.c's refresh arm calls them across
 * the file boundary, so they are plain non-static. */
FORTH_SELFTEST_EXPORT uint16_t _forthConsoleViewBase(uint16_t rows, uint16_t count) {
  uint16_t maxView = _forthConsoleMaxView(rows, count);
  uint16_t view    = forthConsoleViewOffset();
  return (view > maxView) ? maxView : view;
}

void forthConsoleRollView(int16_t delta) {
  forthConsoleRoll(delta);
  /* C12: the ring's roll stops at count-1 (its own bound: there is no
   * older line).  The VIEW stops at count-rows, so the band stays full —
   * past that, each press emptied the transcript one row at a time. */
  _forthConsoleClampView(forthConsoleViewRows(), forthConsoleLineCount());
}

/* Every conjunct is load-bearing — see STAGE_N_TRACES.md N-T1:
 *
 *  - tam.mode: a TAM session over the capture keeps calcMode == CM_AIM and
 *    paints its prompt on the T row.  The fold's forged CM_PEM does NOT
 *    protect this arm: that bracket is three statements wide (ui/tam.c:
 *    1459-1465) around code that never refreshes.
 *  - lastErrorCode: the error text paints INSIDE _refreshRegisterLine on
 *    errorMessageRegisterLine (screen.c:3778-3786, REGISTER_Z by default);
 *    an unconditional console would swallow every error display.
 *  - temporaryInformation: worse than a swallow.  Sixteen TI arms call
 *    displayTemporaryInformationOnX, which repaints ALL FOUR register rows
 *    (screen.c:2571-2576) — and they hang off the REGISTER_X paint the
 *    console KEEPS, so a live TI would wipe the transcript from inside the
 *    one call this arm does not own.  Reachable: a console line can execute
 *    any CAT_FNCT/PTP_NONE item, and BATTV/BYTES/WHO/VERS all set one. */
bool_t _forthConsoleActive(void) {
  /* round 6 (F8): LIVE, not origin — the render gate fired on the
   * suspended residue and painted TAM's abandoned aimBuffer as an
   * editable console line. */
  return calcMode == CM_AIM
         && forthCapInteractiveLive()
         && !tam.mode
         && lastErrorCode == 0
         && temporaryInformation == TI_NO_INFO;
}

void _forthConsoleRender(void) {
  /* The first pixel row the editor's own ink or cursor can occupy.
   *
   * Short line (yMultiLineEdOffset == 3): showStringEdC47 draws at
   * Y_POSITION_OF_NIM_LINE - 3 = 129 and its cursor block starts one row
   * above, at 128.  Long line (== 1): the caller's y is overridden to
   * (yincr-1) + 1*(yincr-1) = 68 with yincr 35 (screen.c:1655,1680-1683),
   * cursor from 67.
   *
   * DERIVED from yMultiLineEdOffset, never re-measured from the string:
   * the landed AIM arm below trusts that same global at this same point in
   * the frame, so reading it keeps the console and the editor in step —
   * re-measuring would put them a frame apart on the call where the line
   * crosses the long/short boundary. */
  uint16_t editorTop = _forthConsoleEditorTop();
  uint16_t rows, firstY, r, view, count, viewBase;

  if(editorTop <= Y_POSITION_OF_REGISTER_T_LINE) {
    return;                                    /* graceful floor; unreachable
                                                  with the two states above */
  }
  rows = forthConsoleViewRows();
  count = forthConsoleLineCount();
  if(rows == 0 || count == 0) {
    return;                                    /* an empty console shows an
                                                  empty area, not registers */
  }
  viewBase = _forthConsoleViewBase(rows, count);  /* C-3: rows is
                                                  frame-variable, so the bound
                                                  is re-checked every frame —
                                                  for the paint only, never by
                                                  writing the stored offset */
  /* Bottom-anchor the band to the editor: 4 rows at Y 44/65/86/107 in the
   * short-line state, 2 at Y 25/46 in the long-line state (N-T1). */
  firstY = (uint16_t)(editorTop - rows * FORTH_CONSOLE_ROW_PITCH);

  for(r = 0; r < rows; r++) {
    char line[FORTH_CONSOLE_LINE_MAX + 1];
    /* The BOTTOM row (r == rows-1) shows the line at the roll offset —
     * offset 0 is the newest — and every row above it is one line older. */
    view = (uint16_t)(viewBase + (rows - 1 - r));
    if(view >= count) {
      continue;                                /* fewer lines than rows */
    }
    if(!forthConsoleLineAt(view, line, sizeof(line)) || line[0] == 0) {
      continue;
    }
    /* Truncate with the native ellipsis — the landed idiom at
     * screen.c:4982-4983; 14 px is STD_ELLIPSIS's width.  No wrapping:
     * that is a Stage N non-goal, render-only and additive later. */
    if(stringWidth(line, &standardFont, true, true) > SCREEN_WIDTH - 1) {
      char *cut = stringAfterPixels(line, &standardFont, SCREEN_WIDTH - 14 - 1, true, true);
      xcopy(cut, STD_ELLIPSIS, 3);
    }
    showString(line, &standardFont, 1,
               (uint32_t)(firstY + r * FORTH_CONSOLE_ROW_PITCH), vmNormal, true, true);
  }
}
