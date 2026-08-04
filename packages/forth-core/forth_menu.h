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

/* Scan cut-off for the picker's program-text pass (§9.6 documented
 * deviation): programs with more steps than this before the cursor are not
 * fully scanned. Declared here, not in forth_menu.c, so the pin in the test
 * battery references the same number the builder uses. */
#define FORTH_PICKER_MAX_SCAN_STEPS 1000

/* Build dynamicSoftmenu[menu]'s content for MNU_FORTH (§9.6, F6-5).
 * The caller has already freed the previous menuContent. */
void forthBuildWordPicker(int16_t menu);

#endif /* FORTH_MENU_H */
