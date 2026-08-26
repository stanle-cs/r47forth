// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file browsers/prettyBrowser.c
 * The formula browser, copy-adapting historyBrowser.c's state machine:
 * first call flips calcMode and repaints; later calls are the refresher
 * (the refreshScreen case). Rows come from ppfBuildRow (the same
 * variable-height packing the pager proved); the selected row carries a
 * 3 px marker and pans horizontally when wider than the screen.
 *
 * ENTER stages the selected history entry's TKRES result into X — a
 * real machine mutation, so it runs saveForUndo first (UNDO works) and
 * invalidates the shadow expression stack afterwards (the recall
 * bypasses item dispatch, exactly the class of mutation the claims
 * registry says must not go unnoticed; UNKNOWN slots upgrade truthfully
 * later). The live (now) row has no stored result — its value IS X —
 * so ENTER there just leaves.
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

  // pass 1: which page holds the selection (variable-height packing)
  uint8_t page = 0, selPage = 0;
  int16_t y = 25;
  for(uint8_t row = 0; row < totalRows; row++) {
    if(!ppfBuildRow(row, haveCurrent, &root, &asc, &h)) {
      continue;
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
    if(!ppfBuildRow(row, haveCurrent, &root, &asc, &h)) {
      continue;
    }
    if(y + h - 1 > 163) {
      page++;
      y = 25;
    }
    if(page == selPage) {
      int16_t x = 8;
      if(row == pbSelection) {
        lcd_fill_rect(0, (uint32_t)y, 3, (uint32_t)h, LCD_EMPTY_VALUE);
        const ppNode_t *n = ppNodeAt(root);
        if(n->width > SCREEN_WIDTH - 12 && pbPan > 0) {
          int16_t maxPan = (int16_t)(n->width - (SCREEN_WIDTH - 12));
          if(pbPan > maxPan) {
            pbPan = 0;   // wrap
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
  pbPan = (int16_t)(pbPan + 60);   // paint wraps when past the row's width
}

void prettyBrowserLeave(void) {
  calcMode = pbPreviousCalcMode;
  screenUpdatingMode = SCRUPD_AUTO;
}

/* find the TKRES token of a history entry; returns NULL when absent */
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
  liftStack();   // honours FLAG_ASLIFT exactly like a recall
  reallocateRegister(REGISTER_X, dataType, allocParam, amNone);
  if(lastErrorCode == ERROR_NONE) {
    xcopy(getRegisterDataPointer(REGISTER_X), payload, bytes);
    setRegisterTag(REGISTER_X, tag);
  }
  setSystemFlag(FLAG_ASLIFT);
  // the recall bypassed item dispatch: the shadow must not pretend it
  // followed — wipe to UNKNOWN (truthful upgrades later), keep history
  ppcShadowInvalidate();
  prettyBrowserLeave();
}
