// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file historyBrowser.c The undo history browser
 ***********************************************/

#include "c47.h"

#define HB_FIRST_ROW_Y       43   // the flag browser's row grid
#define HB_ROW_HEIGHT        22
#define HB_ROWS               8
#define HB_PREVIEW_X        104   // preview column start, pixels
#define HB_PREVIEW_GLYPHS    15

static int16_t currentHistorySelection = 0;   // logical level, 0 = oldest

// Copies source glyph-wise (high-bit glyphs are two bytes) so a truncated
// preview never ends inside a glyph.
static void historyGlyphTruncate(char *dest, const char *source, uint32_t maxGlyphs) {
  uint32_t s = 0, d = 0, glyphs = 0;
  while(source[s] != 0 && glyphs < maxGlyphs) {
    dest[d++] = source[s];
    if(source[s++] & 0x80) {
      if(source[s] == 0) {
        break;
      }
      dest[d++] = source[s++];
    }
    glyphs++;
  }
  dest[d] = 0;
}

// Formats the level's X register — staged into TEMP_REGISTER_1 by
// undoHistoryStagePreview — through the standard display code. This runs
// in display context only (refreshScreen -> historyBrowser), per the
// capture-purity ruling in DESIGN.md §3.
static void historyPreviewString(uint8_t logical, char *dest) {
  dest[0] = 0;
  if(!undoHistoryStagePreview(logical)) {
    strcpy(dest, "?");
    return;
  }
  tmpString[0] = 0;
  switch(getRegisterDataType(TEMP_REGISTER_1)) {
    case dtLongInteger: {
      longIntegerRegisterToDisplayString(TEMP_REGISTER_1, tmpString, TMP_STR_LENGTH, 130, 50, false);
      break;
    }
    case dtReal34: {
      real34ToDisplayString(REGISTER_REAL34_DATA(TEMP_REGISTER_1), getRegisterTag(TEMP_REGISTER_1), tmpString, &standardFont, 130, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC);
      break;
    }
    case dtComplex34: {
      complex34ToDisplayString(REGISTER_COMPLEX34_DATA(TEMP_REGISTER_1), tmpString, &standardFont, 130, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC, (uint16_t)getRegisterTag(TEMP_REGISTER_1), false);
      break;
    }
    case dtShortInteger: {
      shortIntegerToDisplayString(TEMP_REGISTER_1, tmpString, false, 0);
      break;
    }
    case dtString: {
      historyGlyphTruncate(tmpString, REGISTER_STRING_DATA(TEMP_REGISTER_1), HB_PREVIEW_GLYPHS);
      break;
    }
    case dtTime: {
      timeToDisplayString(TEMP_REGISTER_1, tmpString, true);
      break;
    }
    case dtDate: {
      dateToDisplayString(TEMP_REGISTER_1, tmpString);
      break;
    }
    case dtReal34Matrix:
    case dtComplex34Matrix: {
      sprintf(tmpString, "[%ux%u]", (unsigned)REGISTER_MATRIX_HEADER(TEMP_REGISTER_1)->matrixRows, (unsigned)REGISTER_MATRIX_HEADER(TEMP_REGISTER_1)->matrixColumns);
      break;
    }
    default: {
      strcpy(tmpString, "?");
      break;
    }
  }
  historyGlyphTruncate(dest, tmpString, HB_PREVIEW_GLYPHS);
}

  /********************************************//**
   * \brief The undo history browser application
   *
   * \param[in] unusedButMandatoryParameter uint16_t
   * \return void
   ***********************************************/
  void historyBrowser(uint16_t unusedButMandatoryParameter) {
    uint8_t depth = undoHistoryDepth();
    char line[64], preview[40];
    int16_t dispSel, dispFirst;

    hourGlassIconEnabled = false;

    if(calcMode != CM_HIST_BROWSER) {
      if(calcMode == CM_AIM) {
        hideCursor();
        cursorEnabled = false;
      }
      previousCalcMode = calcMode;
      calcMode = CM_HIST_BROWSER;
      clearSystemFlag(FLAG_ALPHA);
      currentHistorySelection = undoHistoryCursorIndex() >= 0 ? undoHistoryCursorIndex() : (int16_t)depth - 1;
      if(currentHistorySelection < 0) {
        currentHistorySelection = 0;
      }
      refreshScreen(195);        // restart once, cleared, now in the correct mode
    }

    if(depth == 0) {
      showString("Undo history is empty.", &standardFont, 1, HB_FIRST_ROW_Y, vmNormal, true, true);
      showString("EXIT to leave.", &standardFont, 1, HB_FIRST_ROW_Y + HB_ROW_HEIGHT, vmNormal, true, true);
      return;
    }
    if(currentHistorySelection >= depth) {
      currentHistorySelection = depth - 1;
    }

    sprintf(line, "Undo history  %u level%s", (unsigned)depth, depth == 1 ? "" : "s");
    showString(line, &standardFont, 1, 22 - 1, vmNormal, true, true);

    // Newest first. The window keeps the selection visible.
    dispSel = (int16_t)depth - 1 - currentHistorySelection;
    dispFirst = dispSel >= HB_ROWS ? dispSel - (HB_ROWS - 1) : 0;
    for(int16_t row = 0; row < HB_ROWS && dispFirst + row < depth; row++) {
      uint8_t logical = (uint8_t)(depth - 1 - (dispFirst + row));
      uint16_t seq;
      int16_t labelItem;
      uint8_t flags;
      const char *name = "-";                        // unlabeled capture
      videoMode_t vm = logical == currentHistorySelection ? vmReverse : vmNormal;
      int16_t y = HB_FIRST_ROW_Y + row * HB_ROW_HEIGHT;

      if(!undoHistoryLevelInfo(logical, &seq, &labelItem, &flags)) {
        break;
      }
      if(flags & HISTORY_ENTRY_LIVEANCHOR) {
        name = "(now)";
      }
      else if(labelItem > 0 && labelItem <= LAST_ITEM) {
        name = indexOfItems[labelItem].itemCatalogName;
      }
      sprintf(line, "%c%c%02u", undoHistoryCursorIndex() == logical ? '*' : ' ',
                                (flags & HISTORY_ENTRY_GAPBEFORE) ? '~' : ' ',
                                (unsigned)(seq % 100));
      showString(line, &standardFont, 1, y, vm, true, true);
      historyGlyphTruncate(line, name, 8);
      showString(line, &standardFont, 44, y, vm, true, true);
      historyPreviewString(logical, preview);
      showString(preview, &standardFont, HB_PREVIEW_X, y, vm, true, true);
    }
  }

void historyBrowserUp(void) {
  if(undoHistoryDepth() > 0 && currentHistorySelection < (int16_t)undoHistoryDepth() - 1) {
    currentHistorySelection++;
  }
}

void historyBrowserDown(void) {
  if(undoHistoryDepth() > 0 && currentHistorySelection > 0) {
    currentHistorySelection--;
  }
}

void historyBrowserLeave(void) {
  calcMode = previousCalcMode;
  if(calcMode == CM_TIMER) {
    previousCalcMode = CM_NORMAL;
  }
}

void historyBrowserEnter(void) {
  if(undoHistoryDepth() > 0) {
    (void)undoHistoryRestoreLevel((uint8_t)currentHistorySelection);
  }
  historyBrowserLeave();
}

int16_t historyBrowserSelection(void) {
  return currentHistorySelection;
}
