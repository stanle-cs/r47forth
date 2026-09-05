// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.c
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#include "pgmGraphics.h"
#include "c47.h"

static pgCanvas_t canvas;

// Sends the changed rows to the LCD and stamps the time (DESIGN.md §8.4).
static void pgRefreshNow(void) {
  canvas.lastRefreshMs = getUptimeMs();
  #if defined(DMCP_BUILD)
    lcd_refresh_dma();
  #else // !DMCP_BUILD
    lcd_refresh();
  #endif // DMCP_BUILD
}

// Sets the region, the clip rectangle, and clears the region to white.
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
}

// PVIEW n: opens the canvas view over region n (DESIGN.md §3.5).
void fnPview(uint16_t region) {
  if(region != PG_REGION_REGISTERS && region != PG_REGION_FULL) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }
  if(calcMode != CM_GRAPHICS_CANVAS) {
    canvas.prevCalcMode = calcMode;
  }
  pgSetRegion((uint8_t)region);
  calcMode = CM_GRAPHICS_CANVAS;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
  if(region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}

// ERASE: clears the canvas region. Opens the view over region 2 when closed (DESIGN.md §3.7).
void fnErase(uint16_t unusedButMandatoryParameter) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    fnPview(PG_REGION_REGISTERS);
    return;
  }
  pgSetRegion(canvas.region);
  if(canvas.region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}

// The refreshScreen case for the canvas view: the status bar stays live,
// the softmenu is painted for region 2, nothing else is painted.
void pgRefreshCanvasView(void) {
  refreshStatusBar();
  if(canvas.region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
}

// EXIT in the canvas view: restore the previous mode and repaint (DESIGN.md §3.6).
void pgCloseView(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    return;
  }
  calcMode = canvas.prevCalcMode;
  canvas.region = 0;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(197);
}

#if defined(TESTSUITE_BUILD)
  #include <stdio.h>

  static uint32_t pgTestFailures;

  static void pgTestFail(const char *what) {
    pgTestFailures++;
    printf("program-graphics test FAIL: %s\n", what);
  }

  // Writes value into regist as a long integer.
  static void pgTestWriteLonI(calcRegister_t regist, uint32_t value) {
    longInteger_t li;
    longIntegerInit(li);
    uInt32ToLongInteger(value, li);
    convertLongIntegerToLongIntegerRegister(li, regist);
    longIntegerFree(li);
  }

  void pgTestSmoke(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Runs count steps of item through reallyRunFunction with the program
  // state set to running, and returns the elapsed milliseconds.
  static uint32_t pgTestRunSteps(int16_t item, uint32_t count) {
    const uint8_t savedRunStop = programRunStop;
    uint32_t start, end;
    programRunStop = PGM_RUNNING;
    start = getUptimeMs();
    for(uint32_t i = 0; i < count; i++) {
      reallyRunFunction(item, NOPARAM);
    }
    end = getUptimeMs();
    programRunStop = savedRunStop;
    return end - start;
  }

  void pgTestBaseline(uint16_t unusedButMandatoryParameter) {
    const uint32_t count = 1000000;
    uint32_t nopMs, pixelMs;
    pgTestFailures = 0;
    pgTestWriteLonI(REGISTER_X, 200);   // PIXEL reads x from X
    pgTestWriteLonI(REGISTER_Y, 100);   // and y from Y
    nopMs   = pgTestRunSteps(ITM_NOP,   count);
    pixelMs = pgTestRunSteps(ITM_PIXEL, count);
    if(lastErrorCode != ERROR_NONE) {
      pgTestFail("baseline: an error code is set after the PIXEL loop");
      lastErrorCode = ERROR_NONE;
    }
    if(!lcd_buffer_pixel_on(200, SCREEN_HEIGHT - 100 - 1)) {
      pgTestFail("baseline: PIXEL 200,100 did not light row 139 column 200");
    }
    printf("program-graphics baseline: %u steps, NOP %u ms, PIXEL %u ms, PIXEL body %u ms\n",
           count, nopMs, pixelMs, pixelMs > nopMs ? pixelMs - nopMs : 0);
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Stage G1 pins of TESTING.md §4: the canvas view.
  void pgTestView(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;

    // V1: PVIEW 2 enters mode 21 and clears rows 20 to 170 only.
    setBlackPixel(10, 100);
    setBlackPixel(10, 200);
    fnPview(2);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V1 PVIEW 2 did not enter the canvas view");
    if(lcd_buffer_pixel_on(10, 100))     pgTestFail("V1 PVIEW 2 left a pixel lit in the register region");
    if(canvas.clipY1 != PG_REGISTER_BOTTOM_ROW) pgTestFail("V1 PVIEW 2 did not clip at row 170");
    // The softmenu painter owns rows 171 to 239 in region 2 and repaints
    // them, so a pixel there proves nothing. The clip row is the pin.

    // V2: PVIEW 6 clears rows 20 to 239.
    setBlackPixel(10, 200);
    fnPview(6);
    if(lcd_buffer_pixel_on(10, 200))     pgTestFail("V2 PVIEW 6 left a pixel lit in the softmenu region");
    if(canvas.clipY1 != SCREEN_HEIGHT - 1) pgTestFail("V2 PVIEW 6 did not clip at row 239");

    // V3: PVIEW 3 is an error and changes nothing.
    fnPview(3);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("V3 PVIEW 3 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V3 PVIEW 3 changed the mode");

    // V4: the stop path repaint keeps the canvas.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(!lcd_buffer_pixel_on(50, 100))    pgTestFail("V4 refreshScreen in the canvas view erased the drawing");

    // V8: the same repaint keeps the status bar live. The band is cleared
    // first, so only a repaint can light it again.
    lcd_fill_rect(0, 0, SCREEN_WIDTH, PG_TOP_ROW, LCD_SET_VALUE);
    refreshScreen(4);
    {
      bool_t lit = false;
      for(uint32_t x = 0; x < SCREEN_WIDTH && !lit; x++) {
        for(uint32_t y = 0; y < PG_TOP_ROW && !lit; y++) {
          lit = lcd_buffer_pixel_on(x, y);
        }
      }
      if(!lit)                           pgTestFail("V8 refreshScreen in the canvas view did not repaint the status bar");
    }

    // V5: VIEW inside the view paints nothing over the canvas.
    {
      const uint8_t savedRunStop = programRunStop;
      programRunStop = PGM_RUNNING;
      fnView(REGISTER_X);
      programRunStop = savedRunStop;
    }
    if(!lcd_buffer_pixel_on(50, 100))    pgTestFail("V5 VIEW inside the canvas view erased the drawing");
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V5 VIEW changed the mode");

    // V6: EXIT restores the previous mode.
    pgCloseView();
    if(calcMode != CM_NORMAL)            pgTestFail("V6 pgCloseView did not restore CM_NORMAL");

    // V7: without PVIEW, the normal repaint erases a drawing, as upstream does today.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(lcd_buffer_pixel_on(50, 100))     pgTestFail("V7 a drawing without PVIEW survived the normal repaint");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Stage G1 pins of TESTING.md §4: the keys in the canvas view.
  void pgTestKeys(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    fnPview(2);

    // K1: the mode sits inside the range that the three softkey gates block.
    if(!(CM_GRAPHICS_CANVAS >= 19 && CM_GRAPHICS_CANVAS <= 23)) {
      pgTestFail("K1 CM_GRAPHICS_CANVAS is outside the package browser range 19 to 23");
    }

    // K2: direct keys other than EXIT and R/S change nothing. Each key is
    // pressed, then released through its item function, as the keyboard
    // does. The guard arm marks a digit processed at the press. ENTER,
    // BACKSPACE, UP, DOWN, and .d reach their key function at the release.
    setBlackPixel(60, 100);
    processKeyAction(ITM_ENTER);     runFunction(ITM_ENTER);
    processKeyAction(ITM_1);
    processKeyAction(ITM_UP1);       runFunction(ITM_UP1);
    processKeyAction(ITM_DOWN1);     runFunction(ITM_DOWN1);
    processKeyAction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
    processKeyAction(ITM_dotD);      runFunction(ITM_dotD);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K2 a key left the canvas view");
    if(!lcd_buffer_pixel_on(60, 100))    pgTestFail("K2 a key erased the canvas");
    if(lastErrorCode != ERROR_NONE)      pgTestFail("K2 a key raised an error");
    lastErrorCode = ERROR_NONE;

    // K3: EXIT closes the view. The press does nothing for this mode. The
    // release runs the EXIT item, whose function is fnKeyExit.
    processKeyAction(ITM_EXIT1);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K3 the EXIT press alone left the canvas view");
    runFunction(ITM_EXIT1);
    if(calcMode != CM_NORMAL)            pgTestFail("K3 EXIT did not close the canvas view");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }
#endif // TESTSUITE_BUILD
