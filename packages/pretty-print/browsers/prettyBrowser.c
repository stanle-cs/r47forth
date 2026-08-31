// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file browsers/prettyBrowser.c
 * The formula browser, copy-adapted from historyBrowser.c's state
 * machine: the first call flips calcMode and repaints, and later calls
 * are the refresher. Rows come from ppfBuildRow. The selected row
 * carries a 3 px marker and pans horizontally when wider than the
 * screen.
 *
 * ENTER stages the selected entry's stored result into X: it runs
 * saveForUndo first and invalidates the shadow expression stack after,
 * because the recall bypasses item dispatch. The live (top) row has no
 * stored result: its value is X already, so ENTER there just leaves.
 */

#include "c47.h"
#include "prettyInternal.h"
#include "browsers/prettyBrowser.h"

static uint8_t pbSelection = 0;
static int16_t pbPan = 0;
static uint8_t pbPreviousCalcMode = CM_NORMAL;

static uint8_t pbTotalRows(void) {
  return (uint8_t)(ppcHistoryCount() + ((ppcCurrentFormulaRoot() != PPC_NIL) ? 1 : 0));
}

static void pbPaint(void) {
  uint8_t haveCurrent = (ppcCurrentFormulaRoot() != PPC_NIL) ? 1 : 0;
  uint8_t totalRows = pbTotalRows();
  uint8_t root;
  int16_t asc, h;

  lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
  drawSinglePixelFullWidthLine(20);
  drawSinglePixelFullWidthLine(168);

  if(totalRows == 0) {
    showString("no formulas", &standardFont, 8, 90, vmNormal, false, true);
    return;
  }

  /* A row that cannot be drawn still exists: both passes reserve a
   * fixed-height placeholder for it, so every row pages, selects and
   * marks like any other. */
  #define PB_UNSHOWN_H 20

  // pass 1: which page holds the selection (variable-height packing)
  uint8_t page = 0, selPage = 0;
  int16_t y = 25;
  for(uint8_t row = 0; row < totalRows; row++) {
    if(!ppfBuildRow(row, haveCurrent, true, &root, &asc, &h)) {
      h = PB_UNSHOWN_H;
    }
    if(y + h - 1 > 163) {
      page++;
      y = 25;
    }
    if(row == pbSelection) {
      selPage = page;
    }
    y = (int16_t)(y + h + 5);
  }

  // pass 2: paint the selection's page
  page = 0;
  y = 25;
  for(uint8_t row = 0; row < totalRows && page <= selPage; row++) {
    bool_t built = ppfBuildRow(row, haveCurrent, true, &root, &asc, &h);
    if(!built) {
      h = PB_UNSHOWN_H;
    }
    if(y + h - 1 > 163) {
      page++;
      y = 25;
    }
    if(page == selPage) {
      int16_t x = 8;
      if(row == pbSelection) {
        lcd_fill_rect(0, (uint32_t)y, 3, (uint32_t)h, LCD_EMPTY_VALUE);
      }
      if(!built) {
        // width is one reason of several: a glyph the font lacks also
        // lands here, so the message must not claim to know (PP18RR9-1)
        showString("(cannot draw)", &standardFont, 8, (uint32_t)y,
                   vmNormal, false, true);
        y = (int16_t)(y + h + 5);
        continue;
      }
      if(row == pbSelection) {
        const ppNode_t *n = ppNodeAt(root);
        int16_t visible = (int16_t)(SCREEN_WIDTH - 12);
        if(n->width > visible) {
          int16_t maxPan = (int16_t)(n->width - visible);
          if(pbPan > maxPan) {
            pbPan = maxPan;   // clamp at the right edge
          }
          x = (int16_t)(8 - pbPan);
        }
      }
      ppPaintAt(root, x, (int16_t)(y + asc));
    }
    y = (int16_t)(y + h + 5);
  }
}

void prettyBrowser(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  if(calcMode != CM_PRETTY_BROWSER) {
    pbPreviousCalcMode = calcMode;
    calcMode = CM_PRETTY_BROWSER;
    pbSelection = 0;
    pbPan = 0;
    clearSystemFlag(FLAG_ALPHA);
    cursorEnabled = false;
    refreshScreen(196);
    return;
  }
  pbPaint();
}

void prettyBrowserUp(void) {
  if(pbSelection > 0) {
    pbSelection--;
    pbPan = 0;
  }
}

void prettyBrowserDown(void) {
  if((uint8_t)(pbSelection + 1) < pbTotalRows()) {
    pbSelection++;
    pbPan = 0;
  }
}

void prettyBrowserPan(void) {
  pbPan = (int16_t)(pbPan + 60);   // the paint clamps this to the row's width
}

void prettyBrowserLeave(void) {
  calcMode = pbPreviousCalcMode;
  screenUpdatingMode = SCRUPD_AUTO;
}

/* Find the TKRES token of a history entry. Returns NULL when absent. */
static const uint8_t *pbFindResult(const uint8_t *entry, uint8_t *dataType, uint8_t *tag,
                                   uint16_t *allocParam, uint8_t *bytes) {
  if(entry == NULL) {
    return NULL;
  }
  uint16_t total;
  xcopy(&total, entry, 2);
  uint16_t off = 6;
  while(off < total) {
    uint8_t tok = entry[off++];
    switch(tok) {
      case PPT_TKL: {
        uint8_t len = entry[off++];
        off = (uint16_t)(off + len);
        break;
      }
      case PPT_TKV:
      case PPT_TKRES: {
        uint8_t dt = entry[off++];
        uint8_t tg = entry[off++];
        uint16_t ap;
        xcopy(&ap, entry + off, 2);
        off += 2;
        uint8_t by = entry[off++];
        if(tok == PPT_TKRES) {
          *dataType = dt;
          *tag = tg;
          *allocParam = ap;
          *bytes = by;
          return entry + off;
        }
        off = (uint16_t)(off + by);
        break;
      }
      case PPT_TKC:
      case PPT_TKR:
      case PPT_TKO1:
      case PPT_TKO2:
        off += 2;
        break;
      // this decoder must know every token ppfBuildEntry knows, or an
      // entry renders but refuses to recall
      case PPT_TKBIG:
        off = (uint16_t)(off + 4 + 16);   // item u16, label u16, payload 16
        break;
      default:
        return NULL;
    }
  }
  return NULL;
}

void prettyBrowserEnter(void) {
  uint8_t haveCurrent = (ppcCurrentFormulaRoot() != PPC_NIL) ? 1 : 0;
  if(haveCurrent && pbSelection == 0) {
    prettyBrowserLeave();   // the live formula's value IS X already
    return;
  }
  uint8_t dataType, tag, bytes;
  uint16_t allocParam;
  const uint8_t *payload = pbFindResult(
      ppcHistoryEntry((uint8_t)(pbSelection - haveCurrent), NULL, NULL),
      &dataType, &tag, &allocParam, &bytes);
  if(payload == NULL) {
    prettyBrowserLeave();   // entry without a stored result: nothing to recall
    return;
  }

  saveForUndo();
  if(lastErrorCode == ERROR_RAM_FULL) {
    lastErrorCode = 0;
    prettyBrowserLeave();
    return;
  }
  liftStack();   // honors FLAG_ASLIFT exactly like a recall
  reallocateRegister(REGISTER_X, dataType, allocParam, amNone);
  if(lastErrorCode == ERROR_NONE) {
    xcopy(getRegisterDataPointer(REGISTER_X), payload, bytes);
    setRegisterTag(REGISTER_X, tag);
  }
  setSystemFlag(FLAG_ASLIFT);
  // the recall bypassed item dispatch: wipe the shadow to UNKNOWN and
  // keep the history
  ppcShadowInvalidate();
  prettyBrowserLeave();
}
