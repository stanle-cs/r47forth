// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyFormula.c
 * The formula view: turns capture trees / history token streams into 2D
 * infix layouts (DIV becomes a stacked fraction, powers raise, roots get
 * vinculums, precedence inserts parens), and the PHIST pager surface.
 *
 * PHIST deliberately reuses the PSHOW manual-paint protocol instead of a
 * browser calcMode: repeated PHIST presses page through the history, any
 * other key releases the screen. Zero keyboard.c/defines.h churn;
 * calcMode 20 stays reserved (DESIGN.md §7) for a full browser upgrade.
 *
 * Value leaves format ONLY here, in display context: staged into
 * TEMP_REGISTER_1 by the undo-history recipe and run through the
 * standard display builders.
 */

#include "c47.h"
#include "prettyInternal.h"

static char ppfValBuf[96];

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
      complex34ToDisplayString(REGISTER_COMPLEX34_DATA(TEMP_REGISTER_1), buf, &standardFont,
                               180, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC,
                               (uint16_t)getRegisterTag(TEMP_REGISTER_1), false);
      break;
    case dtShortInteger:
      shortIntegerToDisplayString(TEMP_REGISTER_1, buf, false, 0);
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


/* ==== infix construction ================================================
 * Precedence: ADD/SUB 1, MULT 2; FRAC/SUP/RAD/atoms 3 (visually scoped).
 * ppfBuildOp* build the layout for one operator from already-built child
 * layouts + their precedences — shared by the tree walker and the token
 * decoder so both paths typeset identically. */

static uint8_t ppfParen(uint8_t inner, uint8_t fontId) {
  uint8_t p = ppNewBox(PP_PAREN, fontId);
  if(p == PP_NONE || inner == PP_NONE) {
    return PP_NONE;
  }
  ppAppendChild(p, inner);
  return p;
}

uint8_t ppfWrapIf(uint8_t node, int prec, int minPrec, uint8_t fontId) {
  if(node == PP_NONE) {
    return PP_NONE;
  }
  return (prec < minPrec) ? ppfParen(node, fontId) : node;
}

uint8_t ppfRun(const char *s, uint8_t fontId) {
  return ppNewRun(s, (uint16_t)strlen(s), fontId);
}

uint8_t ppfBuildOp2(uint16_t item, uint8_t a, int aPrec, uint8_t b, int bPrec,
                           uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  switch(item) {
    case ITM_DIV: {
      uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
      if(frac == PP_NONE || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(frac, a);   // FRAC scopes its children: no parens
      ppAppendChild(frac, b);
      return frac;
    }
    case ITM_YX: {
      uint8_t sup = ppNewBox(PP_SUP, ctxFont);
      uint8_t base = ppfWrapIf(a, aPrec, PPF_PREC_ATOM, ctxFont);   // (2+3)² keeps its parens
      if(sup == PP_NONE || base == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sup, base);
      ppAppendChild(sup, b);
      return sup;
    }
    case ITM_XTHROOT: {
      // ˣ√y: index = X (right operand b), radicand = Y (left operand a)
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      uint8_t idx = b;
      if(rad == PP_NONE || a == PP_NONE || idx == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);
      ppAppendChild(rad, idx);
      return rad;
    }
    case ITM_LOGXY: {
      // LOGₓy: base = X (right operand b), argument = Y (left operand a)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t sub = ppNewBox(PP_SUB, ctxFont);
      uint8_t name = ppfRun("log", ctxFont);
      if(box == PP_NONE || sub == PP_NONE || name == PP_NONE
          || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      // ppfParen allocates and can fail, and PP_HBOX is variadic, so
      // ppMeasure has no arity check to catch a missing child: unchecked,
      // this renders "log2" with its argument absent and reports success.
      uint8_t par = ppfParen(a, ctxFont);
      if(par == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sub, name);
      ppAppendChild(sub, b);
      ppAppendChild(box, sub);
      ppAppendChild(box, par);
      return box;
    }
    case ITM_ADD: case ITM_SUB: case ITM_MULT: {
      int myPrec = (item == ITM_MULT) ? PPF_PREC_MUL : PPF_PREC_ADD;
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t l = ppfWrapIf(a, aPrec, myPrec, ctxFont);
      // right side of SUB needs parens at equal precedence: a-(b+c)
      uint8_t r = ppfWrapIf(b, bPrec, (item == ITM_SUB) ? myPrec + 1 : myPrec, ctxFont);
      // multiplication typesets as the raised dot (matches the EQN view)
      uint8_t op = ppfRun(item == ITM_MULT ? STD_DOT : indexOfItems[item].itemCatalogName, ctxFont);
      if(box == PP_NONE || l == PP_NONE || r == PP_NONE || op == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, l);
      ppAppendChild(box, op);
      ppAppendChild(box, r);
      *outPrec = myPrec;
      return box;
    }
    default: {
      // function form: name(a, b)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t name = ppfRun(indexOfItems[item].itemCatalogName, ctxFont);
      uint8_t inner = ppNewBox(PP_HBOX, ctxFont);
      uint8_t comma = ppfRun(", ", childFont);
      if(box == PP_NONE || name == PP_NONE || inner == PP_NONE || comma == PP_NONE
          || a == PP_NONE || b == PP_NONE) {
        return PP_NONE;
      }
      // Same shape: unchecked, this renders the function NAME alone with
      // "(a, b)" absent, and reports success.
      uint8_t par = ppfParen(inner, ctxFont);
      if(par == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(inner, a);
      ppAppendChild(inner, comma);
      ppAppendChild(inner, b);
      ppAppendChild(box, name);
      ppAppendChild(box, par);
      return box;
    }
  }
}

uint8_t ppfBuildOp1(uint16_t item, uint8_t a, int aPrec,
                           uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  switch(item) {
    case ITM_SQUAREROOTX: {
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      if(rad == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);   // the vinculum scopes: no parens
      return rad;
    }
    case ITM_CUBEROOT: {
      uint8_t rad = ppNewBox(PP_RAD, ctxFont);
      uint8_t idx = ppfRun("3", childFont);
      if(rad == PP_NONE || a == PP_NONE || idx == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(rad, a);
      ppAppendChild(rad, idx);
      return rad;
    }
    case ITM_ABS: case ITM_MAGNITUDE: {
      uint8_t bars = ppNewBox(PP_BARS, ctxFont);
      if(bars == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(bars, a);
      return bars;
    }
    case ITM_1ONX: {
      uint8_t frac = ppNewBox(PP_FRAC, ctxFont);
      uint8_t one = ppfRun("1", childFont);
      if(frac == PP_NONE || one == PP_NONE || a == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(frac, one);
      ppAppendChild(frac, a);
      return frac;
    }
    case ITM_SQUARE: case ITM_CUBE: {
      uint8_t sup = ppNewBox(PP_SUP, ctxFont);
      uint8_t base = ppfWrapIf(a, aPrec, PPF_PREC_ATOM, ctxFont);
      uint8_t exp = ppfRun(item == ITM_SQUARE ? "2" : "3", childFont);
      if(sup == PP_NONE || base == PP_NONE || exp == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(sup, base);
      ppAppendChild(sup, exp);
      return sup;
    }
    case ITM_CHS: {
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t minus = ppfRun("-", ctxFont);
      uint8_t inner = ppfWrapIf(a, aPrec, PPF_PREC_MUL, ctxFont);
      if(box == PP_NONE || minus == PP_NONE || inner == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, minus);
      ppAppendChild(box, inner);
      *outPrec = PPF_PREC_ADD;
      return box;
    }
    default: {
      // function form: name(a)
      uint8_t box = ppNewBox(PP_HBOX, ctxFont);
      uint8_t name = ppfRun(indexOfItems[item].itemCatalogName, ctxFont);
      uint8_t inner = ppfParen(a, ctxFont);
      if(box == PP_NONE || name == PP_NONE || inner == PP_NONE) {
        return PP_NONE;
      }
      ppAppendChild(box, name);
      ppAppendChild(box, inner);
      return box;
    }
  }
}


/* ==== big operators (PP12) ==============================================
 * A captured Sigma_n/Pi_n/integral-YX dispatch. The label param decodes
 * through labelList (findNamedLabel returns index + FIRST_LABEL); the
 * lookup is display-time best-effort — a program edit between capture
 * and display falls back to the numeric form. */

void ppfVariableName(uint16_t varId, char *out) {
  strcpy(out, "x");
  if(varId >= FIRST_NAMED_VARIABLE
      && (uint32_t)(varId - FIRST_NAMED_VARIABLE) < numberOfNamedVariables) {
    const uint8_t *vn = allNamedVariables[varId - FIRST_NAMED_VARIABLE].variableName;
    if(vn[0] > 0 && vn[0] <= 15) {
      xcopy(out, vn + 1, vn[0]);
      out[vn[0]] = 0;
      // the canonical variable X typesets as the classic lowercase x
      // (the closest form the fonts have to the italic convention);
      // every other name keeps its own letters
      if(out[0] == 'X' && out[1] == 0) {
        out[0] = 'x';
      }
    }
  }
}

static void ppfLabelName(uint16_t param, char *out) {
  if(param >= FIRST_LABEL && (uint32_t)(param - FIRST_LABEL) < numberOfLabels) {
    const uint8_t *p = labelList[param - FIRST_LABEL].labelPointer;
    if(p != NULL && (*(p - 1) == STRING_LABEL_VARIABLE || *(p - 1) == LOCAL_LABEL_VARIABLE)) {
      uint8_t len = *p;
      if(len > 0 && len <= 15) {
        xcopy(out, p + 1, len);
        out[len] = 0;
        return;
      }
    }
  }
  snprintf(out, 17, "LBL %u", (unsigned)param);
}

static uint8_t ppfBigop(uint16_t item, uint16_t label, const uint8_t *stepBytes,
                        uint8_t fromN, uint8_t toN,
                        uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  /* A big operator is not an atom: its body extends right of the stroke,
   * so a neighbour binds into it. ADD brackets it under x, / and ^, and
   * leaves it bare left of a +, where convention already scopes it. */
  *outPrec = PPF_PREC_ADD;
  bool_t isInt = (item == ITM_INTEGRAL_YX);
  uint8_t big = ppNewBox(PP_BIGOP, ctxFont);
  if(big == PP_NONE || fromN == PP_NONE || toN == PP_NONE) {
    return PP_NONE;
  }
  ppSetBoxTag(big, item);
  ppSetFontDeep(fromN, childFont);
  ppSetFontDeep(toN, childFont);

  // sized so neither overflow NOR truncation is possible: lbl 23 +
  // '(' + dv 19 + ')d' + dv 19 + NUL = 66 worst case
  char lbl[24], text[96];
  ppfLabelName(label, lbl);
  if(isInt) {
    // the d-variable rides in the step payload; decode its name,
    // display-time best-effort like the label
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
      return ppfRun(text, ctxFont);
    }
    case PPN_VAL:
      if(!ppfStageValFields(nd->aux, nd->pad[0], nd->item, nd->pad[1], nd->payload)
          || !ppfFormatStaged(ppfValBuf, sizeof(ppfValBuf))) {
        return PP_NONE;
      }
      return ppfRun(ppfValBuf, ctxFont);
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
        stackPrec[sp++] = PPF_PREC_ATOM;
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
        }
        else {
          if(sp >= 8) {
            return false;
          }
          stackNode[sp] = ppfRun(ppfValBuf, ctxFont);
          stackPrec[sp++] = PPF_PREC_ATOM;
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


/* ==== PHIST — the history pager ========================================= */

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

// Build + measure one pager row (row 0 = the current formula when open).
// Standard rung first, then the whole tree re-fonted tiny — fraction
// children are built in the context font, so childFont alone cannot
// shrink a nested stack (found by the continued-fraction stress test).
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
    // HEIGHT is a hard limit: a row taller than the band cannot be shown
    // at all. WIDTH is not, because the browser pans sideways. So width
    // only sends us down a rung; at the last rung the row is accepted
    // however wide it is, and panning carries it.
    if(h > PPF_BAND_BOTTOM - PPF_BAND_TOP + 1) {
      continue;   // too tall even here: try tiny, then give up
    }
    // At the LAST rung, width depends on the caller. The browser pans
    // sideways, so it takes the row however wide. The full-screen pager
    // cannot pan, and an over-wide row there would paint CLIPPED —
    // showing `12345678 + 98` for `12345678 + 98765432`, a lie by
    // truncation. It omits the row instead.
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

  // PP10: PHIST opens the formula BROWSER (calcMode 20) — selection,
  // panning, recall-to-X. The manual-paint pager below is retained as
  // the non-browser fallback surface and for the packing reference.
  if(calcMode != CM_PRETTY_BROWSER) {
    prettyBrowser(NOPARAM);
    return;
  }

  // repeated PHIST presses (screen still held) page forward; anything
  // else released the screen, so a fresh press starts at page 0
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

  // Pass 1 — variable-height packing to count pages (rows pack until the
  // band fills; heights vary from one text line to a nested stack)
  uint8_t pages = 1;
  {
    uint8_t page = 0;
    int16_t y = PPF_BAND_TOP;
    for(uint8_t row = 0; row < totalRows; row++) {
      if(!ppfBuildRow(row, haveCurrent, false, &root, &asc, &h)) {
        continue;
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

  // Pass 2 — paint the selected page with the same packing walk
  {
    uint8_t page = 0;
    int16_t y = PPF_BAND_TOP;
    for(uint8_t row = 0; row < totalRows && page <= ppfPage; row++) {
      if(!ppfBuildRow(row, haveCurrent, false, &root, &asc, &h)) {
        continue;
      }
      if(y + h - 1 > PPF_BAND_BOTTOM) {
        page++;
        y = PPF_BAND_TOP;
      }
      if(page == ppfPage) {
        ppPaintAt(root, 4, (int16_t)(y + asc));
      }
      y = (int16_t)(y + h + PPF_ROW_GAP);
    }
  }

  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
}
