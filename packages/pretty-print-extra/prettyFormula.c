// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyFormula.c
 * The formula view: turns capture trees / history token streams into 2D
 * infix layouts (DIV becomes a stacked fraction, powers raise, roots get
 * vinculums, precedence inserts parens), and the PHIST pager surface.
 *
 * PHIST opens the formula browser (calcMode 20). The manual-paint pager
 * in this file stays as the non-browser fallback surface.
 *
 * Value leaves format only here, in display context: staged into
 * TEMP_REGISTER_1 and run through the standard display builders.
 */

#include "c47.h"
#include "prettyInternal.h"
#include "prettyExtraInternal.h"

// Sized to ppfFormatStaged's own budget. The widest reachable spelling
// is 160 bytes: base 2 at WSIZE 64 with digit grouping on.
static char ppfValBuf[200];

/* ==== value-leaf formatting (display context only) ====================== */

static bool_t ppfStageValFields(uint8_t dataType, uint8_t tag, uint16_t allocParam,
                                uint8_t bytes, const uint8_t *payload) {
  reallocateRegister(TEMP_REGISTER_1, dataType, allocParam, amNone);
  if(lastErrorCode != ERROR_NONE) {
    lastErrorCode = 0;
    return false;
  }
  xcopy(getRegisterDataPointer(TEMP_REGISTER_1), payload, bytes);
  setRegisterTag(TEMP_REGISTER_1, tag);
  return true;
}

static bool_t ppfFormatStaged(char *dest, size_t destSize) {
  char buf[200];
  buf[0] = 0;
  switch(getRegisterDataType(TEMP_REGISTER_1)) {
    case dtLongInteger:
      longIntegerRegisterToDisplayString(TEMP_REGISTER_1, buf, sizeof(buf), 180, 40, false);
      break;
    case dtReal34:
      real34ToDisplayString(REGISTER_REAL34_DATA(TEMP_REGISTER_1), getRegisterTag(TEMP_REGISTER_1),
                            buf, &standardFont, 180, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC);
      break;
    case dtComplex34:
      // the staged tag carries the polar bit; a literal false here redrew
      // every polar value in rectangular form (audit PP18RR8-5)
      complex34ToDisplayString(REGISTER_COMPLEX34_DATA(TEMP_REGISTER_1), buf, &standardFont,
                               180, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC,
                               getComplexRegisterAngularMode(TEMP_REGISTER_1),
                               getComplexRegisterPolarMode(TEMP_REGISTER_1) == amPolar);
      break;
    case dtShortInteger:
      /* This builder does not write from the front: it lays digits out
       * from displayString[ERROR_MESSAGE_LENGTH / 2] upward as scratch,
       * so its buffer must be at least ERROR_MESSAGE_LENGTH bytes (buf
       * is too small). tmpString is what every upstream caller passes,
       * and nothing runs between the call and the copy below. */
      _Static_assert(TMP_STR_LENGTH >= ERROR_MESSAGE_LENGTH,
                     "shortIntegerToDisplayString writes from "
                     "ERROR_MESSAGE_LENGTH/2 upward, so its buffer must be "
                     "at least ERROR_MESSAGE_LENGTH bytes");
      shortIntegerToDisplayString(TEMP_REGISTER_1, tmpString, false, 0);
      if(strlen(tmpString) >= sizeof(buf)) {
        return false;
      }
      strcpy(buf, tmpString);
      break;
    default:
      return false;
  }
  if(strlen(buf) >= destSize) {
    return false;
  }
  strcpy(dest, buf);
  return true;
}


static uint8_t ppfBigop(uint16_t item, uint16_t label, const uint8_t *stepBytes,
                        uint8_t fromN, uint8_t toN,
                        uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  /* A big operator is not an atom: its body extends right of the
   * stroke. ADD brackets it under x, / and ^, and leaves it bare
   * beside a +. */
  *outPrec = PPF_PREC_ADD;
  bool_t isInt = (item == ITM_INTEGRAL_YX);
  uint8_t big = ppNewBox(PP_BIGOP, ctxFont);
  if(big == PP_NONE || fromN == PP_NONE || toN == PP_NONE) {
    return PP_NONE;
  }
  ppSetBoxTag(big, item);
  ppSetFontDeep(fromN, childFont);
  ppSetFontDeep(toN, childFont);

  // worst case 66 bytes: lbl 23 + '(' + dv 19 + ')d' + dv 19 + NUL
  char lbl[24], text[96];
  ppfLabelName(label, lbl);
  if(isInt) {
    // the d-variable rides in the step payload. Its name decodes
    // display-time best-effort, like the label.
    char dv[20];
    ppfVariableName((uint16_t)(stepBytes[0] | ((uint16_t)stepBytes[1] << 8)), dv);
    snprintf(text, sizeof(text), "%s(%s)d%s", lbl, dv, dv);
  }
  else {
    // sums iterate the label program over the counter n
    snprintf(text, sizeof(text), "%s(n)", lbl);
  }
  uint8_t body = ppfRun(text, ctxFont);
  if(body == PP_NONE) {
    return PP_NONE;
  }

  uint8_t under;
  if(isInt) {
    under = fromN;
  }
  else {
    uint8_t hb  = ppNewBox(PP_HBOX, childFont);
    uint8_t pre = ppfRun("n=", childFont);
    if(hb == PP_NONE || pre == PP_NONE) {
      return PP_NONE;
    }
    ppAppendChild(hb, pre);
    ppAppendChild(hb, fromN);
    // a non-unit step must be visible or the display lies
    real34_t step;
    xcopy(&step, stepBytes, 16);
    if(!real34CompareEqual(&step, const34_1)) {
      char sb[48], stext[52];
      real34ToDisplayString(&step, amNone, sb, &standardFont, 60, 6, LIMITEXP, !FRONTSPACE, NOIRFRAC);
      snprintf(stext, sizeof(stext), "," STD_DELTA "%s", sb);
      uint8_t st = ppfRun(stext, childFont);
      if(st == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(hb, st);
    }
    under = hb;
  }
  ppAppendChild(big, body);
  ppAppendChild(big, under);
  ppAppendChild(big, toN);
  return big;
}


/* ==== capture tree -> layout ============================================ */

static uint8_t ppfFromCaptureNode(uint8_t cap, uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  const ppcNode_t *nd = ppcNodeAt(cap);
  if(cap == PPC_NIL || cap == PPC_UNKNOWN || nd == NULL) {
    return PP_NONE;
  }
  switch(nd->kind) {
    case PPN_LIT: {
      char text[32];
      uint8_t l = nd->aux > 15 ? 15 : nd->aux;
      xcopy(text, nd->payload, l);
      text[l] = 0;
      const ppcNode_t *c = (nd->child[0] != PPC_NIL) ? ppcNodeAt(nd->child[0]) : NULL;
      if(c != NULL && c->kind == PPN_LIT2) {
        uint8_t cl = c->aux > 15 ? 15 : c->aux;
        xcopy(text + l, c->payload, cl);
        text[l + cl] = 0;
      }
      *outPrec = ppfTextIsAtom(text, (uint16_t)strlen(text))
                   ? PPF_PREC_ATOM : PPF_PREC_ADD;
      return ppfRun(text, ctxFont);
    }
    case PPN_VAL: {
      /* A payload wider than one node continues into a PPN_VAL2 on
       * child[0], the same shape PPN_LIT uses above. The two parts must
       * be joined here: nd->payload alone is 16 bytes. */
      const uint8_t head  = (uint8_t)sizeof(nd->payload);
      const uint8_t bytes = nd->pad[1];
      const uint8_t *src  = nd->payload;
      uint8_t full[PPC_VAL_CAPACITY];
      if(bytes > head) {
        const ppcNode_t *c = (nd->child[0] != PPC_NIL) ? ppcNodeAt(nd->child[0]) : NULL;
        if(c == NULL || c->kind != PPN_VAL2 || bytes > (uint8_t)sizeof(full)) {
          return PP_NONE;   // a split value with no continuation is not drawable
        }
        xcopy(full, nd->payload, head);
        xcopy(full + head, c->payload, (uint8_t)(bytes - head));
        src = full;
      }
      if(!ppfStageValFields(nd->aux, nd->pad[0], nd->item, bytes, src)
          || !ppfFormatStaged(ppfValBuf, sizeof(ppfValBuf))) {
        return PP_NONE;
      }
      *outPrec = ppfTextIsAtom(ppfValBuf, (uint16_t)strlen(ppfValBuf))
                   ? PPF_PREC_ATOM : PPF_PREC_ADD;
      return ppfRun(ppfValBuf, ctxFont);
    }
    case PPN_CONST:
      return ppfRun(indexOfItems[nd->item].itemCatalogName, ctxFont);
    case PPN_RCL: {
      char rname[8];
      snprintf(rname, sizeof(rname), "R%02u", (unsigned)nd->item);
      return ppfRun(rname, ctxFont);
    }
    case PPN_OP1: {
      int p;
      uint8_t a = ppfFromCaptureNode(nd->child[0], ctxFont, childFont, &p);
      if(a == PP_NONE) {
        return PP_NONE;
      }
      return ppfBuildOp1(nd->item, a, p, ctxFont, childFont, outPrec);
    }
    case PPN_OP2: {
      int pa, pb;
      uint8_t a = ppfFromCaptureNode(nd->child[0], ctxFont, childFont, &pa);
      uint8_t b = ppfFromCaptureNode(nd->child[1], ctxFont, childFont, &pb);
      if(a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      return ppfBuildOp2(nd->item, a, pa, b, pb, ctxFont, childFont, outPrec);
    }
    case PPN_BIGOP: {
      int pf, pt;
      uint8_t f = ppfFromCaptureNode(nd->child[0], childFont, childFont, &pf);
      uint8_t t = ppfFromCaptureNode(nd->child[1], childFont, childFont, &pt);
      if(f == PP_NONE || t == PP_NONE) {
        return PP_NONE;
      }
      uint16_t label = (uint16_t)(nd->pad[0] | ((uint16_t)nd->pad[1] << 8));
      return ppfBigop(nd->item, label, nd->payload, f, t, ctxFont, childFont, outPrec);
    }
    default:
      return PP_NONE;
  }
}

bool_t ppfBuildCurrent(uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut) {
  uint8_t cap = ppcCurrentFormulaRoot();
  if(cap == PPC_NIL) {
    return false;
  }
  int prec;
  uint8_t n = ppfFromCaptureNode(cap, ctxFont, childFont, &prec);
  if(n == PP_NONE) {
    return false;
  }
  *rootOut = n;
  return true;
}


/* ==== history token stream -> layout ==================================== */

bool_t ppfBuildEntry(const uint8_t *entry, uint8_t ctxFont, uint8_t childFont,
                     bool_t withResult, uint8_t *rootOut) {
  if(entry == NULL) {
    return false;
  }
  uint16_t total;
  xcopy(&total, entry, 2);
  uint16_t off = 6;

  uint8_t stackNode[8];
  int     stackPrec[8];
  int     sp = 0;
  uint8_t resultRun = PP_NONE;

  while(off < total) {
    uint8_t tok = entry[off++];
    switch(tok) {
      case PPT_TKL: {
        uint8_t len = entry[off++];
        char text[32];
        if(len >= sizeof(text) || sp >= 8) {
          return false;
        }
        xcopy(text, entry + off, len);
        text[len] = 0;
        off = (uint16_t)(off + len);
        stackNode[sp] = ppfRun(text, ctxFont);
        stackPrec[sp++] = ppfTextIsAtom(text, (uint16_t)strlen(text))
                            ? PPF_PREC_ATOM : PPF_PREC_ADD;
        break;
      }
      case PPT_TKV:
      case PPT_TKRES: {
        uint8_t dataType = entry[off++];
        uint8_t tag = entry[off++];
        uint16_t allocParam;
        xcopy(&allocParam, entry + off, 2);
        off += 2;
        uint8_t bytes = entry[off++];
        if(!ppfStageValFields(dataType, tag, allocParam, bytes, entry + off)
            || !ppfFormatStaged(ppfValBuf, sizeof(ppfValBuf))) {
          return false;
        }
        off = (uint16_t)(off + bytes);
        if(tok == PPT_TKRES) {
          resultRun = ppfRun(ppfValBuf, ctxFont);
          if(resultRun == PP_NONE) {
            return false;   // a lost "= result" tail must decline, not degrade:
                            // the recall path still finds the TKRES
          }
        }
        else {
          if(sp >= 8) {
            return false;
          }
          stackNode[sp] = ppfRun(ppfValBuf, ctxFont);
          stackPrec[sp++] = ppfTextIsAtom(ppfValBuf, (uint16_t)strlen(ppfValBuf))
                              ? PPF_PREC_ATOM : PPF_PREC_ADD;
        }
        break;
      }
      case PPT_TKC:
      case PPT_TKR: {
        uint16_t item;
        xcopy(&item, entry + off, 2);
        off += 2;
        if(sp >= 8) {
          return false;
        }
        if(tok == PPT_TKR) {
          char rname[8];
          snprintf(rname, sizeof(rname), "R%02u", (unsigned)item);
          stackNode[sp] = ppfRun(rname, ctxFont);
        }
        else {
          stackNode[sp] = ppfRun(indexOfItems[item].itemCatalogName, ctxFont);
        }
        stackPrec[sp++] = PPF_PREC_ATOM;
        break;
      }
      case PPT_TKO1: {
        uint16_t item;
        xcopy(&item, entry + off, 2);
        off += 2;
        if(sp < 1) {
          return false;
        }
        int p;
        uint8_t n = ppfBuildOp1(item, stackNode[sp - 1], stackPrec[sp - 1], ctxFont, childFont, &p);
        if(n == PP_NONE) {
          return false;
        }
        stackNode[sp - 1] = n;
        stackPrec[sp - 1] = p;
        break;
      }
      case PPT_TKO2: {
        uint16_t item;
        xcopy(&item, entry + off, 2);
        off += 2;
        if(sp < 2) {
          return false;
        }
        int p;
        uint8_t n = ppfBuildOp2(item, stackNode[sp - 2], stackPrec[sp - 2],
                                stackNode[sp - 1], stackPrec[sp - 1], ctxFont, childFont, &p);
        if(n == PP_NONE) {
          return false;
        }
        sp--;
        stackNode[sp - 1] = n;
        stackPrec[sp - 1] = p;
        break;
      }
      case PPT_TKBIG: {
        uint16_t item, label;
        xcopy(&item, entry + off, 2);
        off += 2;
        xcopy(&label, entry + off, 2);
        off += 2;
        const uint8_t *step = entry + off;
        off = (uint16_t)(off + 16);
        if(sp < 2) {
          return false;
        }
        int p;
        uint8_t n = ppfBigop(item, label, step, stackNode[sp - 2], stackNode[sp - 1],
                             ctxFont, childFont, &p);
        if(n == PP_NONE) {
          return false;
        }
        sp--;
        stackNode[sp - 1] = n;
        stackPrec[sp - 1] = p;
        break;
      }
      default:
        return false;
    }
  }
  if(sp != 1) {
    return false;
  }
  if(withResult && resultRun != PP_NONE) {
    uint8_t box = ppNewBox(PP_HBOX, ctxFont);
    uint8_t eq = ppfRun(" = ", ctxFont);
    if(box == PP_NONE || eq == PP_NONE) {
      return false;
    }
    ppAppendChild(box, stackNode[0]);
    ppAppendChild(box, eq);
    ppAppendChild(box, resultRun);
    *rootOut = box;
    return true;
  }
  *rootOut = stackNode[0];
  return true;
}


#if defined(PC_BUILD)
/* Test seam: stage a raw register payload and return its display
 * spelling. The alphabet pin (T33) enumerates spellings against the
 * fonts without a keypad fixture. */
bool_t ppfTestStagedSpelling(uint8_t dataType, uint8_t tag, const uint8_t *payload,
                             uint8_t bytes, char *out, size_t cap) {
  if(!ppfStageValFields(dataType, tag, 0, bytes, payload)) {
    return false;
  }
  char buf[200];
  if(!ppfFormatStaged(buf, sizeof(buf))) {
    return false;
  }
  if(strlen(buf) >= cap) {
    return false;
  }
  strcpy(out, buf);
  return true;
}
#endif // PC_BUILD


/* ==== the T-line live formula (core extension point) ==================== */

/* Registered into the core's ppTlineExtension slot at ppcInit
 * (prettyCapture.c). prettyTryRegisterLine calls it for REGISTER_T
 * before the value rendering: while a formula is open, the T line
 * shows the formula. No formula or no fit returns false, and the
 * ordinary value rendering runs. */
bool_t ppfTlineTry(int16_t baseY, int16_t bandTop, int16_t bandBottom,
                   int16_t *lineWidth) {
  if(!getSystemFlag(FLAG_PTLINE)) {
    return false;
  }
  static const uint8_t tRungs[2][2] = {
    { PP_FONT_STANDARD, PP_FONT_STANDARD },
    { PP_FONT_STANDARD, PP_FONT_TINY     },
  };
  for(int r = 0; r < 2; r++) {
    uint8_t root;
    ppReset();
    if(!ppfBuildCurrent(tRungs[r][0], tRungs[r][1], &root)) {
      break;
    }
    if(r == 1) {
      ppSetFontDeep(root, PP_FONT_TINY);   // whole-tree shrink, as in the pager
    }
    if(ppRenderRightAligned(root, SCREEN_WIDTH, bandTop, bandBottom, ppPreferredBase(baseY))) {
      *lineWidth = ppNodeAt(root)->width;
      return true;
    }
  }
  return false;
}


/* ==== PHIST: the history pager ========================================== */

// Inset 4 px from the frame lines at 20/168: plain clearance between a
// row's ink and the frame. The rows are laid out against it.
#define PPF_BAND_TOP    25
#define PPF_BAND_BOTTOM 163
#define PPF_ROW_GAP     5

static uint8_t ppfPage = 0;

void fnPrettyHistClear(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppcHistoryClear();
}

// Build + measure one pager row (row 0 = the current formula when
// open). Standard rung first, then the whole tree re-fonted tiny:
// fraction children are built in the context font, so childFont alone
// cannot shrink a nested stack.
bool_t ppfBuildRow(uint8_t row, uint8_t haveCurrent, bool_t canPan, uint8_t *rootOut, int16_t *ascOut, int16_t *hOut) {
  for(int rung = 0; rung < 2; rung++) {
    uint8_t cf = (rung == 0) ? PP_FONT_STANDARD : PP_FONT_TINY;
    uint8_t root;
    bool_t built;
    ppReset();
    if(haveCurrent && row == 0) {
      built = ppfBuildCurrent(PP_FONT_STANDARD, cf, &root);
    }
    else {
      const uint8_t *e = ppcHistoryEntry((uint8_t)(row - haveCurrent), NULL, NULL);
      built = ppfBuildEntry(e, PP_FONT_STANDARD, cf, true, &root);
    }
    if(!built) {
      return false;
    }
    if(rung == 1) {
      ppSetFontDeep(root, PP_FONT_TINY);
    }
    if(!ppMeasure(root, 0)) {
      continue;
    }
    const ppNode_t *n = ppNodeAt(root);
    int16_t h = (int16_t)(n->ascent + n->descent);
    // Height is a hard limit: a row taller than the band cannot be
    // shown at all.
    if(h > PPF_BAND_BOTTOM - PPF_BAND_TOP + 1) {
      continue;   // too tall even here: try tiny, then give up
    }
    // At the last rung, width depends on the caller. The browser pans
    // sideways, so it takes the row at any width. The pager cannot
    // pan, so it omits an over-wide row.
    if(n->width > SCREEN_WIDTH - 8 && (rung == 0 || !canPan)) {
      continue;
    }
    *rootOut = root;
    *ascOut = n->ascent;
    *hOut = h;
    return true;
  }
  return false;
}

void fnPrettyHist(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  if(lastErrorCode != ERROR_NONE) {
    return;
  }

  // PHIST opens the formula browser (calcMode 20). The manual-paint
  // pager below stays as the non-browser fallback surface.
  if(calcMode != CM_PRETTY_BROWSER) {
    prettyBrowser(NOPARAM);
    return;
  }

  // repeated PHIST presses (screen still held) page forward. Any other
  // key released the screen, so a fresh press starts at page 0.
  if(screenHoldsDrawnPixels) {
    ppfPage++;
  }
  else {
    ppfPage = 0;
  }

  uint8_t histN = ppcHistoryCount();
  uint8_t haveCurrent = (ppcCurrentFormulaRoot() != PPC_NIL) ? 1 : 0;
  uint8_t totalRows = (uint8_t)(histN + haveCurrent);
  uint8_t root;
  int16_t asc, h;

  // Pass 1: variable-height packing to count pages. Rows pack until
  // the band fills. A row that cannot build keeps its place as a
  // fixed-height placeholder line: a filed entry must not vanish from
  // the page (PP18RR9-1).
  uint8_t pages = 1;
  {
    uint8_t page = 0;
    int16_t y = PPF_BAND_TOP;
    for(uint8_t row = 0; row < totalRows; row++) {
      if(!ppfBuildRow(row, haveCurrent, false, &root, &asc, &h)) {
        h = 20;
      }
      if(y + h - 1 > PPF_BAND_BOTTOM) {
        page++;
        y = PPF_BAND_TOP;
      }
      y = (int16_t)(y + h + PPF_ROW_GAP);
    }
    pages = (uint8_t)(page + 1);
  }
  ppfPage = (uint8_t)(ppfPage % pages);

  lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
  drawSinglePixelFullWidthLine(20);
  drawSinglePixelFullWidthLine(168);

  // Pass 2: paint the selected page with the same packing walk. The
  // placeholder height must match pass 1 or the pages drift.
  {
    uint8_t page = 0;
    int16_t y = PPF_BAND_TOP;
    for(uint8_t row = 0; row < totalRows && page <= ppfPage; row++) {
      bool_t built = ppfBuildRow(row, haveCurrent, false, &root, &asc, &h);
      if(!built) {
        h = 20;
      }
      if(y + h - 1 > PPF_BAND_BOTTOM) {
        page++;
        y = PPF_BAND_TOP;
      }
      if(page == ppfPage) {
        if(built) {
          ppPaintAt(root, 4, (int16_t)(y + asc));
        }
        else {
          showString("(cannot draw)", &standardFont, 4, (uint32_t)y,
                     vmNormal, false, true);
        }
      }
      y = (int16_t)(y + h + PPF_ROW_GAP);
    }
  }

  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
}
