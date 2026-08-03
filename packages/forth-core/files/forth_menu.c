/*
 * forth_menu.c — Forth capture UI: the FWRD word picker and the
 * name-insertion helpers shared by the picker and the catalogs.
 *
 * S2 (simplification pass): this code previously lived inside the
 * upstream files it is called from — the MNU_FORTH builder inside
 * softmenus.c's initVariableSoftmenu(), the insert/guard helpers at the
 * top of keyboard.c.  Nothing here is upstream logic, so carrying it as
 * ~210 lines of patch against two large, actively-maintained upstream
 * files bought nothing but rebase surface.  Moved verbatim; the upstream
 * patches are now the call sites alone.
 */

#include "c47.h"
#include "forth_dict.h"
#include "forth_capture.h"
#include "forth_menu.h"

/* Name-slot comparator.  softmenus.c's own sortMenu is file-static, so
 * the picker carries its own — same comparison, same 15-byte slot
 * stride used by every other dynamic-menu name list. */
static int forthSortMenu(void const *a, void const *b) {
  return compareString(a, b, CMP_EXTENSIVE);
}

/* Insert name + one trailing space into the open capture line at
 * T_cursorPos.  Same 256-byte/196-glyph cap as typing; false = no room
 * or capture not open.  §9.6 P-H7 discipline, generalized (F6-3). */
bool_t forthCapInsertName(const char *name)
{
  int32_t nameLen = stringByteLength(name);
  if(!forthCapIsOpen()) { return false; }
  int32_t bufLen = stringByteLength(aimBuffer);
  if(bufLen + nameLen + 1 < 256 && stringGlyphLength(aimBuffer) + nameLen + 1 <= 196) {
    xcopy(aimBuffer + T_cursorPos + nameLen + 1, aimBuffer + T_cursorPos,
          stringByteLength(aimBuffer + T_cursorPos) + 1);
    xcopy(aimBuffer + T_cursorPos, name, nameLen);
    aimBuffer[T_cursorPos + nameLen] = ' ';
    T_cursorPos += nameLen + 1;
    return true;
  }
  return false;
}

/* Insert the currently selected dynamic-menu label (§9.6 P-H7). */
bool_t pickerInsertName(void)
{
  return forthCapInsertName(dynmenuGetLabel(dynamicMenuItem));
}

/* forthPickerGuard — guard for MNU_FORTH picker action in executeFunction (§9.6).
 * Returns true only when all Forth-capture conjuncts hold AND the current
 * softmenu is actually MNU_FORTH (menu-identity check, placed before any
 * dynamicSoftmenu[] indexing to prevent OOB access on wrong menu). */
bool_t forthPickerGuard(int16_t item)
{
  if(softmenu[softmenuStack[0].softmenuId].menuItem != -MNU_FORTH) return false;
  return (calcMode == CM_PEM
      && getSystemFlag(FLAG_ALPHA)
      && tam.function == ITM_FORTH
      && item == ITM_NOP
      && dynamicMenuItem >= 0
      && dynamicMenuItem < dynamicSoftmenu[softmenuStack[0].softmenuId].numItems);
}

/* MNU_FORTH content builder (§9.6, F6-5): the UNION of the edited
 * program's own ': NAME' definitions before the cursor (text scan — no
 * execution needed), the interactive-scope dictionary, and the global
 * dictionary, each section sorted independently.
 *
 * Called from initVariableSoftmenu()'s MNU_FORTH arm, which has already
 * freed the previous menuContent; this function only allocates the new
 * one and sets numItems. */
void forthBuildWordPicker(int16_t menu)
{
  int16_t i, numberOfBytes;
  uint8_t *ptr;
  uint8_t *progStart = NULL;
  uint8_t *step;
  int16_t nNames = 0;
  /* R2 finding 6, ruled: accepted names are copied into 15-byte slots in
   * this same global tmpString with no capacity check; TMP_STR_LENGTH/15
   * = 170 complete slots fit, and a personal program with more unique
   * definitions before the cursor could write past the workspace.
   * Policy: truncate by scan order. Once the cap is reached, stop
   * RECORDING new names but keep tokenizing normally — nothing else in
   * this loop depends on nNames, and the cap must not desync the
   * tokenizer's position in the line. Sorting applies only to the names
   * actually collected. No error UI: this is ordinary single-user
   * robustness (a reboot/lost-edit risk), not a reportable condition,
   * and 170 unique word definitions before the cursor is far beyond any
   * realistic personal program. */
  const int16_t forthPickerMaxNames = TMP_STR_LENGTH / 15;

  memset(tmpString, 0, TMP_STR_LENGTH);

  progStart = forthOwningProgramStart(currentStep);

  if (progStart) {
    step = progStart;
    int16_t stepCount = 0;
    while (step && step <= currentStep) {
      stepCount++;
      if (stepCount > FORTH_PICKER_MAX_SCAN_STEPS) break; /* §9.6 documented deviation */
      uint8_t *next = findNextStep(step);
      if (checkOpCodeOfStep(step, ITM_FORTH) && step[2] == STRING_LABEL_VARIABLE) {
        uint8_t len = step[3];
        if (len > 0) {
          char line[256];
          xcopy(line, (const char *)(step + 4), len);
          line[len] = 0;
          int16_t pos = 0;
          char tok[FORTH_TOKEN_MAX + 1];

          while (pos < len && line[pos] == ' ')
            pos = stringNextGlyph(line, pos);
          while (pos < len) {
            int16_t start = pos;
            while (pos < len && line[pos] != ' ')
              pos = stringNextGlyph(line, pos);
            int16_t tokLen = pos - start;
            if (tokLen > FORTH_TOKEN_MAX) {
              while (pos < len && line[pos] == ' ')
                pos = stringNextGlyph(line, pos);
              continue;
            }
            xcopy(tok, line + start, tokLen);
            tok[tokLen] = 0;

            if (compareString(tok, ":", CMP_BINARY) == 0) {
              while (pos < len && line[pos] == ' ')
                pos = stringNextGlyph(line, pos);
              if (pos < len) {
                int16_t nameStart = pos;
                while (pos < len && line[pos] != ' ')
                  pos = stringNextGlyph(line, pos);
                int16_t nameLen = pos - nameStart;
                if (nameLen > 0 && nameLen <= 14) {
                  char nameBuf[15];
                  xcopy(nameBuf, line + nameStart, nameLen);
                  nameBuf[nameLen] = 0;
                  int16_t dup = 0;
                  for (int16_t d = 0; d < nNames; d++) {
                    if (compareString(tmpString + 15 * d, nameBuf, CMP_BINARY) == 0) {
                      dup = 1;
                      break;
                    }
                  }
                  if (!dup && nNames < forthPickerMaxNames) {
                    xcopy(tmpString + 15 * nNames, nameBuf, nameLen);
                    nNames++;
                  }
                }
              }
            }
            while (pos < len && line[pos] == ' ')
              pos = stringNextGlyph(line, pos);
          }
        }
      }
      if (!next || next <= step) break;
      step = next;
    }
  }

  if (nNames > 0) {
    qsort(tmpString, nNames, 15, forthSortMenu);
  }

  /* F6-5 section (b): interactive-scope dictionary words. */
  { uint16_t bi = 0;
    int16_t nB = 0;
    char slot[15];
    while (nNames < forthPickerMaxNames && forthDictBrowseName(bi, FORTH_OWNER_INTERACTIVE, slot)) {
      int16_t dup = 0;
      for (int16_t d = 0; d < nB; d++) {
        if (compareString(tmpString + 15 * (nNames - nB + d), slot, CMP_BINARY) == 0) {
          dup = 1;
          break;
        }
      }
      if (!dup) {
        xcopy(tmpString + 15 * nNames, slot, stringByteLength(slot));
        nNames++;
        nB++;
      }
      bi++;
    }
    if (nB > 0) {
      qsort(tmpString + 15 * (nNames - nB), nB, 15, forthSortMenu);
    }
  }

  /* F6-5 section (c): global dictionary words. */
  { uint16_t ci = 0;
    int16_t nC = 0;
    char slot[15];
    while (nNames < forthPickerMaxNames && forthGDictBrowseName(ci, slot)) {
      int16_t dup = 0;
      for (int16_t d = 0; d < nC; d++) {
        if (compareString(tmpString + 15 * (nNames - nC + d), slot, CMP_BINARY) == 0) {
          dup = 1;
          break;
        }
      }
      if (!dup) {
        xcopy(tmpString + 15 * nNames, slot, stringByteLength(slot));
        nNames++;
        nC++;
      }
      ci++;
    }
    if (nC > 0) {
      qsort(tmpString + 15 * (nNames - nC), nC, 15, forthSortMenu);
    }
  }

  numberOfBytes = 1;
  for (i = 0; i < nNames; i++) {
    numberOfBytes += stringByteLength(tmpString + 15 * i) + 1;
  }

  ptr = calloc(1, numberOfBytes);
  if (ptr == NULL) {
    /* Out of heap: leave the menu empty rather than writing through NULL.
     * An empty picker is a menu with no keys, which showSoftmenuCurrentPart
     * already renders (its numberOfItems == 0 arm blanks all six). */
    dynamicSoftmenu[menu].menuContent = NULL;
    dynamicSoftmenu[menu].numItems = 0;
    return;
  }
  dynamicSoftmenu[menu].menuContent = ptr;
  for (i = 0; i < nNames; i++) {
    int16_t len = stringByteLength(tmpString + 15 * i) + 1;
    xcopy(ptr, tmpString + 15 * i, len);
    ptr += len;
  }

  dynamicSoftmenu[menu].numItems = nNames;
}
