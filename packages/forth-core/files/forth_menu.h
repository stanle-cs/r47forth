/*
 * forth_menu.h — Forth capture UI: FWRD word picker + name insertion.
 * Implementation in forth_menu.c (see its header comment for why this
 * is package-owned rather than inlined into softmenus.c/keyboard.c).
 */

#ifndef FORTH_MENU_H
#define FORTH_MENU_H

#include "c47.h"

/* Insert `name` + one trailing space into the open capture line at
 * T_cursorPos.  Same 256-byte/196-glyph cap as typing; false = no room
 * or capture not open.  §9.6 P-H7 discipline, generalized (F6-3). */
bool_t forthCapInsertName(const char *name);

/* Insert the currently selected dynamic-menu label (§9.6 P-H7). */
bool_t pickerInsertName(void);

/* True when a softkey press should be handled as an MNU_FORTH picker
 * pick rather than as its own item (§9.6). */
bool_t forthPickerGuard(int16_t item);

/* AUDIT C2/C3/C4/C8/C9: establish the console's row for the current input
 * sub-mode.  The ONLY function that may change it while an interactive
 * capture is open; see forth_menu.c for why it retargets rather than
 * pushing. */
void forthConsoleShowSurface(void);

/* Re-establish that row after a native item may have destroyed it. */
void forthConsoleRestoreSurface(void);

/* AUDIT C17: frame ownership.  The frame the console relies on is REGISTERED
 * by a sentinel in its own userMenuId (owned = console-created, the close
 * rung pops it; borrowed = the user's row on loan, the close rung releases
 * it).  Ownership rides the frame through every push/pop/reopen/resume.
 *
 * Invariant: at most one BORROWED base and at most one OWNED frame, the
 * owned one above the borrowed one when both exist (the alpha excursion
 * opened over the user's own FWRD row is the two-registration case, and it
 * is correct).  Never two OWNED, never a stamp outliving its capture — the
 * stack is PERSISTED, so the restore seam unstamps too.  See the banner in
 * forth_menu.c. */
bool_t forthConsoleOwnsSlot0(void);     /* slot 0 is console-CREATED */
bool_t forthConsoleStampOnStack(void);  /* a registered frame exists anywhere */
bool_t forthConsoleBaseOnTop(void);     /* EXIT rung 2: fall through, or pop? */
void   forthConsoleRegisterSlot0(bool_t created);  /* open site (forth_compile.c) */
void   forthConsoleUnstampAll(void);    /* close funnel (forthCapClose) only */

/* Scan cut-off for the picker's program-text pass (§9.6 documented
 * deviation): programs with more steps than this before the cursor are not
 * fully scanned. Declared here, not in forth_menu.c, so the pin in the test
 * battery references the same number the builder uses. */
#define FORTH_PICKER_MAX_SCAN_STEPS 1000

/* Build dynamicSoftmenu[menu]'s content for MNU_FORTH (§9.6, F6-5).
 * The caller has already freed the previous menuContent. */
void forthBuildWordPicker(int16_t menu);

#if defined(FORTH_DEBUG_SELFTEST)
/* AUDIT round 4: raw stamp census, so the battery can ASSERT the ownership
 * invariant instead of trusting the banner that stated it wrongly for a
 * whole session. */
uint8_t forthConsoleTestOwnedCount(void);
uint8_t forthConsoleTestBorrowCount(void);
int16_t forthConsoleTestOwnedSlot(void);
int16_t forthConsoleTestBorrowSlot(void);
#endif

#endif /* FORTH_MENU_H */
