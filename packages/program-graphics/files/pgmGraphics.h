// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.h
 * program-graphics package: drawing commands for user programs.
 */
#if !defined(PGMGRAPHICS_H)
  #define PGMGRAPHICS_H

  #include <stdint.h>

  #define PG_TOP_ROW              20   // First row below status bar
  #define PG_REGISTER_BOTTOM_ROW 170   // Last row of register region (region 2)
  #define PG_REGION_REGISTERS      2   // Register area view
  #define PG_REGION_FULL           6   // Full screen view (registers and softmenu)

  /** State of the canvas view. Invariant: all fields are zero at boot. */
  typedef struct {
    uint8_t   region;        // 0 = closed, 2 = register region, 6 = full region
    uint8_t   prevCalcMode;  // Calculation mode to restore on EXIT
    uint8_t   drawMode;      // 0 = set, 1 = clear, 2 = invert
    uint8_t   errorShown;    // 1 when error text is shown on canvas line 1
    int16_t   clipX0, clipY0, clipX1, clipY1;   // Inclusive screen coordinates (top-left origin)
    uint32_t  lastRefreshMs;
  } pgCanvas_t;

  // screen.h declares commands and view hooks beside the PIXEL family.

  // Test drivers for headless suite. Each writes its failure count to register X.
  void pgTestSmoke   (uint16_t unusedButMandatoryParameter);
  void pgTestBaseline(uint16_t unusedButMandatoryParameter);
  void pgTestView    (uint16_t unusedButMandatoryParameter);
  void pgTestKeys    (uint16_t unusedButMandatoryParameter);
  void pgTestDraw2D  (uint16_t unusedButMandatoryParameter);
  void pgTestShowcase2D(uint16_t unusedButMandatoryParameter);
  void pgTestDraw3D  (uint16_t unusedButMandatoryParameter);
  void pgTestShowcase3D(uint16_t unusedButMandatoryParameter);

#endif // !PGMGRAPHICS_H
