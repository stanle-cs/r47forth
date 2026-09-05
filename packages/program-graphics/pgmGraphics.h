// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.h
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#if !defined(PGMGRAPHICS_H)
  #define PGMGRAPHICS_H

  #include <stdint.h>

  #define PG_TOP_ROW              20   // first row below the status bar
  #define PG_REGISTER_BOTTOM_ROW 170   // last row of the register lines (region 2)
  #define PG_REGION_REGISTERS      2   // PVIEW 2: the register lines
  #define PG_REGION_FULL           6   // PVIEW 6: the register lines and the softmenu

  /** State of the canvas view. All fields are zero at boot (DESIGN.md §3.3). */
  typedef struct {
    uint8_t   region;        // 0 = view closed, else 2 or 6
    uint8_t   prevCalcMode;  // the calcMode to restore on EXIT
    uint8_t   drawMode;      // 0 set, 1 clear, 2 invert
    uint8_t   errorShown;    // 1 while an error message is painted on canvas line 1
    int16_t   clipX0, clipY0, clipX1, clipY1;   // screen coordinates, top-left origin, inclusive
    uint32_t  lastRefreshMs;
  } pgCanvas_t;

  // The commands and the view hooks are declared in screen.h, next to the
  // upstream PIXEL family, so that items.c, keyboard.c, and screen.c see
  // them through c47.h.

  // Test drivers for the headless suite (TESTING.md §1). Each writes its
  // failure count into X as a long integer.
  void pgTestSmoke   (uint16_t unusedButMandatoryParameter);
  void pgTestBaseline(uint16_t unusedButMandatoryParameter);
  void pgTestView    (uint16_t unusedButMandatoryParameter);
  void pgTestKeys    (uint16_t unusedButMandatoryParameter);
  void pgTestDraw2D  (uint16_t unusedButMandatoryParameter);
  void pgTestShowcase2D(uint16_t unusedButMandatoryParameter);
  void pgTestDraw3D  (uint16_t unusedButMandatoryParameter);
  void pgTestShowcase3D(uint16_t unusedButMandatoryParameter);

#endif // !PGMGRAPHICS_H
