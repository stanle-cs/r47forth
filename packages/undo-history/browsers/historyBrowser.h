// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file historyBrowser.h
 */
#if !defined(HISTORYBROWSER_H)
  #define HISTORYBROWSER_H

  /**
   * The undo history browser application: item handler (first call enters
   * CM_HIST_BROWSER) and screen refresher thereafter, like the other
   * browsers.
   *
   * \param[in] unusedButMandatoryParameter uint16_t
   */
  void    historyBrowser        (uint16_t unusedButMandatoryParameter);

  void    historyBrowserUp      (void);   ///< selection toward the newest level
  void    historyBrowserDown    (void);   ///< selection toward the oldest level
  void    historyBrowserEnter   (void);   ///< restore the selected level and leave
  void    historyBrowserLeave   (void);   ///< leave without restoring
  int16_t historyBrowserSelection(void);  ///< current selection (logical level), for the test driver
#endif // !HISTORYBROWSER_H
