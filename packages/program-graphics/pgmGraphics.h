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

  /** State of the canvas view. All fields are zero at boot (DESIGN.md §3.3). */
  typedef struct {
    uint8_t   region;        // 0 = view closed, else 2 or 6
    uint8_t   prevCalcMode;  // the calcMode to restore on EXIT
    uint8_t   drawMode;      // 0 set, 1 clear, 2 invert
    uint8_t   reserved;
    int16_t   clipX0, clipY0, clipX1, clipY1;   // screen coordinates, top-left origin, inclusive
    uint32_t  lastRefreshMs;
  } pgCanvas_t;

  // Test drivers for the headless suite (TESTING.md §1). Each writes its
  // failure count into X as a long integer.
  void pgTestSmoke   (uint16_t unusedButMandatoryParameter);
  void pgTestBaseline(uint16_t unusedButMandatoryParameter);

#endif // !PGMGRAPHICS_H
