// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file browsers/prettyBrowser.h
 * The formula browser (CM_PRETTY_BROWSER, calcMode 20 per the claims
 * registry). Keys: UP/DOWN select, .d pans a too-wide selected row,
 * ENTER recalls the selected formula's result into X and leaves,
 * EXIT/BACKSPACE leave.
 */

#if !defined(PRETTYBROWSER_H)
#define PRETTYBROWSER_H

void prettyBrowser     (uint16_t unusedButMandatoryParameter);
void prettyBrowserUp   (void);
void prettyBrowserDown (void);
void prettyBrowserPan  (void);
void prettyBrowserEnter(void);
void prettyBrowserLeave(void);

#endif // !PRETTYBROWSER_H
