// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyExtraTest.c
 * Coverage drivers for the pretty-print-extra package test suite:
 * prettyTestCapture and prettyTestFormula. testSuite.c registers each
 * driver in funcTestNoParam with coverageDriver = 1.
 * testSuite/tests/pretty_extra.txt drives them. This file builds only
 * under PC_BUILD.
 * Each driver writes its failure count into X as a long integer.
 * The shared scaffolding — the ppTest* helpers, the program-fixture
 * loader and the layout-signature decoders — lives in the pretty-print
 * core package (prettyTest.c, declared in prettyInternal.h).
 */

#include "c47.h"
#include "prettyInternal.h"
#include "prettyExtraInternal.h"

#if defined(PC_BUILD)

#include <stdio.h>

extern bool_t nimWhenButtonPressed;   // keyboard.c file scope, non-static

/* ==== prettyTestCapture =================================================
 * Drives the real interactive paths. Digits go through
 * addItemToNimBuffer, which opens NIM from CM_NORMAL. Operator keys
 * close NIM at the addItemToNimBuffer tail, the same as a keypress.
 * The item then runs through runFunction and reallyRunFunction, where
 * the STAGE and DONE hooks live. Expected signatures come from
 * indexOfItems catalog names at runtime, so font or name changes never
 * turn these tests red. */

// The layout-sig helpers live with prettyTestFormula below. Capture
// traces decode history entries through them.
static void ppfTestFiledMatchesLive(const char *what);
static const char *ppfTestFirstRunText(uint8_t n);

static void ppcTestSigNode(uint8_t n, char *out, size_t cap) {
  size_t len = strlen(out);
  if(len + 24 >= cap) {
    return;
  }
  const ppcNode_t *nd = ppcNodeAt(n);
  if(n == PPC_UNKNOWN) {
    // "~" for UNKNOWN, "#" for a value leaf: the signature must tell
    // them apart.
    strcat(out, "~");
    return;
  }
  if(n == PPC_NIL || nd == NULL) {
    strcat(out, "?");
    return;
  }
  switch(nd->kind) {
    case PPN_OP2:
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, " ");
      ppcTestSigNode(nd->child[1], out, cap);
      strcat(out, " ");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
    case PPN_OP1:
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, " ");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
    case PPN_LIT: {
      char text[32];
      uint8_t l = nd->aux > 15 ? 15 : nd->aux;
      xcopy(text, nd->payload, l);
      text[l] = 0;
      if(nd->child[0] != PPC_NIL && ppcNodeAt(nd->child[0]) != NULL
          && ppcNodeAt(nd->child[0])->kind == PPN_LIT2) {
        const ppcNode_t *c = ppcNodeAt(nd->child[0]);
        uint8_t cl = c->aux > 15 ? 15 : c->aux;
        xcopy(text + l, c->payload, cl);
        text[l + cl] = 0;
      }
      strcat(out, text);
      break;
    }
    case PPN_VAL:    strcat(out, "#"); break;
    case PPN_RCL: {
      char rname[8];
      sprintf(rname, "R%02u", (unsigned)nd->item);
      strcat(out, rname);
      break;
    }
    case PPN_CONST:  strncat(out, indexOfItems[nd->item].itemCatalogName, 15); break;
    case PPN_BIGOP:
      strcat(out, "{");
      ppcTestSigNode(nd->child[0], out, cap);
      strcat(out, ",");
      ppcTestSigNode(nd->child[1], out, cap);
      strcat(out, "}");
      strncat(out, indexOfItems[nd->item].itemCatalogName, 15);
      break;
    case PPN_OPAQUE: strcat(out, "!"); break;
    default:         strcat(out, "?"); break;
  }
}

static void ppcTestSig(char *out, size_t cap) {
  out[0] = 0;
  uint8_t root = ppcCurrentFormulaRoot();
  if(root == PPC_NIL) {
    strcpy(out, "-");
    return;
  }
  ppcTestSigNode(root, out, cap);
}

static void ppcTestReset(void) {
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  programRunStop = PGM_STOPPED;
  clearSystemFlag(FLAG_SOLVING);
  clearSystemFlag(FLAG_INTING);
  clearSystemFlag(FLAG_ERPN);
  setSystemFlag(FLAG_ASLIFT);
  aimBuffer[0] = 0;      // NIM typing residue must not leak into later
  nimNumberPart = NP_EMPTY;   // suite blocks (fn42Alpha asserts an empty buffer)
  lastIntegerBase = 0;   // an entry MODE is residue too: a leaked base
                         // makes later typed integers short integers
  prettyReset();
}

static void ppcTestType(const char *s) {
  for(const char *p = s; *p; p++) {
    if(*p >= '0' && *p <= '9') {
      addItemToNimBuffer(ITM_0 + (*p - '0'));
    }
    else if(*p == '.') {
      addItemToNimBuffer(ITM_PERIOD);
    }
    else if(*p == '<') {
      addItemToNimBuffer(ITM_BACKSPACE);
    }
  }
}

static void ppcTestOp(int16_t item) {
  if(calcMode == CM_NIM) {
    addItemToNimBuffer(item);   // the keypress closes NIM before the run
  }
  runFunction(item);
}

static void ppcTestOpParam(int16_t item, uint16_t param) {
  if(calcMode == CM_NIM) {
    closeNim();   // a TAM-parameter key closes NIM before the entry cycle
  }
  reallyRunFunction(item, param);
}

static void ppcTestExpectSig(const char *what, const char *expected) {
  char sig[128];
  ppcTestSig(sig, sizeof(sig));
  if(strcmp(sig, expected) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (expected '%s', actual '%s')\n", what, expected, sig);
  }
}

/* Does the measured tree hold a run with exactly this text? For pins
 * that care only that a decode reached the picture. */

/* Copy-adapted from testSuite.c covWriteAndLoadPgm: write a program in
 * the program-file format and import it through the official loader,
 * which appends it and registers the global label. The Test-suffixed
 * name is the one the test HAL maps ioPathLoadProgram to. */
/* Fixture labels must be unique across this file: findNamedLabel
 * returns the first match, and nothing clears program memory between
 * fixtures. Identical bytes under the same name are accepted as a
 * re-run. */
static void ppcTestExpectHist(const char *what, uint8_t expected) {
  if(ppcHistoryCount() != expected) {
    ppTestFailInt(what, expected, ppcHistoryCount());
  }
}

void prettyTestCapture(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  char expect[128];
  const char *nADD  = indexOfItems[ITM_ADD].itemCatalogName;
  const char *nMULT = indexOfItems[ITM_MULT].itemCatalogName;
  const char *nSUB  = indexOfItems[ITM_SUB].itemCatalogName;
  const char *nSIN  = indexOfItems[ITM_sin].itemCatalogName;
  const char *n1ONX = indexOfItems[ITM_1ONX].itemCatalogName;

  // T1: 2 ENTER 3 + 4 x, one formula. A consumed root continues it.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s 4 %s", nADD, nMULT);
  ppcTestExpectSig("T1 chained sig", expect);
  ppcTestExpectHist("T1 hist", 0);

  // T2: supersession, a new root not consuming (2+3) emits it
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 6 %s", nADD);
  ppcTestExpectSig("T2 new formula", expect);
  ppcTestExpectHist("T2 hist", 1);

  // T3: monadic through the NIM funnel
  ppcTestReset();
  ppcTestType("12");
  ppcTestOp(ITM_sin);
  sprintf(expect, "12 %s", nSIN);
  ppcTestExpectSig("T3 monadic", expect);

  // T4: ENTER dup mirrored as a deep copy
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestOp(ITM_ENTER);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 2 %s", nMULT);
  ppcTestExpectSig("T4 dup", expect);

  // T5: monadic result consumed by a dyadic, one formula
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_1ONX);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 %s 3 %s", n1ONX, nADD);
  ppcTestExpectSig("T5 chain through monadic", expect);
  ppcTestExpectHist("T5 hist", 0);

  // T6: CLX displaces, the natural explicit terminator
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_CLX);
  ppcTestType("7");
  ppcTestOp(ITM_ENTER);
  ppcTestType("8");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "7 8 %s", nMULT);
  ppcTestExpectSig("T6 after CLX", expect);
  ppcTestExpectHist("T6 hist", 1);

  // T7: as-typed literal survives
  ppcTestReset();
  ppcTestType("2.50");
  ppcTestOp(ITM_ENTER);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2.50 4 %s", nMULT);
  ppcTestExpectSig("T7 as-typed", expect);

  // T8: NIM abort by backspace leaves no ghost (deferred lift pays off)
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3<");     // type 3, then backspace to empty = abort
  ppcTestType("4");
  ppcTestOp(ITM_ADD);
  sprintf(expect, "5 4 %s", nADD);
  ppcTestExpectSig("T8 abort", expect);

  // T9: swap mirrored, operand order flips
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_SUB);
  sprintf(expect, "3 2 %s", nSUB);
  ppcTestExpectSig("T9 swap", expect);

  // T10: UNDO discards the current formula, history untouched
  ppcTestReset();
  ppcTestType("3");
  ppcTestOp(ITM_ENTER);
  ppcTestType("4");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_UNDO);
  ppcTestExpectSig("T10 after UNDO", "-");
  ppcTestExpectHist("T10 hist", 0);

  // T11: an erroring dispatch invalidates (DONE-on-error)
  ppcTestReset();
  clearSystemFlag(FLAG_SPCRES);
  ppcTestType("1");
  ppcTestOp(ITM_ENTER);
  ppcTestType("0");
  ppcTestOp(ITM_DIV);
  if(lastErrorCode == ERROR_NONE) {
    ppTestFail("T11 division by zero did not error");
  }
  lastErrorCode = 0;
  ppcTestExpectSig("T11 after error", "-");

  // T12: arena exhaustion invalidates mid-chain, then the engine
  // rebuilds from value-leaf upgrades. The tail of the chain reads
  // "# 1 + 1 +", with # for register Y's live value.
  ppcTestReset();
  ppcTestType("1");
  ppcTestOp(ITM_ENTER);
  for(int i = 0; i < 14; i++) {
    ppcTestType("1");
    ppcTestOp(ITM_ADD);
  }
  sprintf(expect, "# 1 %s 1 %s", nADD, nADD);
  ppcTestExpectSig("T12 exhaustion recovery", expect);

  // T13: LASTx returns as a truthful value leaf
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_LASTX);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s # %s", nADD, nMULT);
  ppcTestExpectSig("T13 LASTx", expect);

  // T14: unknown undo-enabled item (MIN, deliberately unclassified)
  // emits the current formula, then invalidates
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("9");
  ppcTestOp(ITM_MIN);
  ppcTestExpectSig("T14 after unknown", "-");
  ppcTestExpectHist("T14 hist", 1);

  // T15: eRPN ENTER does not dup, the consumed Y upgrades to a value.
  // nimWhenButtonPressed is keyboard-owned. The driver mimics the real
  // keypress (a NIM was open when ENTER went down), so fnKeyEnter's
  // eRPN condition, and the shadow's mirror of it, sees the true state.
  ppcTestReset();
  setSystemFlag(FLAG_ERPN);
  ppcTestType("5");
  nimWhenButtonPressed = true;
  ppcTestOp(ITM_ENTER);
  nimWhenButtonPressed = false;
  ppcTestOp(ITM_MULT);
  sprintf(expect, "# 5 %s", nMULT);
  ppcTestExpectSig("T15 eRPN", expect);
  clearSystemFlag(FLAG_ERPN);

  // T17: a numbered-register recall keeps its NAME in the chain
  ppcTestReset();
  ppcTestType("3");
  ppcTestOpParam(ITM_STO, 5);
  ppcTestType("2");
  ppcTestOpParam(ITM_RCL, 5);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 R05 %s", nMULT);
  ppcTestExpectSig("T17 RCL name", expect);
  ppcTestExpectHist("T17 hist", 0);

  // T18: recalling a STACK register deep-copies its tree. Using the
  // copy supersedes (emits) the original still sitting higher up
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOpParam(ITM_RCL, REGISTER_Y);
  ppcTestOp(ITM_MULT);
  sprintf(expect, "4 2 3 %s %s", nADD, nMULT);
  ppcTestExpectSig("T18 RCL stack copy", expect);
  ppcTestExpectHist("T18 hist", 1);

  // T19: RCL+ builds a dyadic node with the plain operator
  ppcTestReset();
  ppcTestType("10");
  ppcTestOpParam(ITM_STO, 7);
  ppcTestType("5");
  ppcTestOpParam(ITM_RCLADD, 7);
  sprintf(expect, "5 R07 %s", nADD);
  ppcTestExpectSig("T19 RCL-arith", expect);

  // T20: x<>reg emits the departing tree and leaves a truthful
  // value leaf for the register's old content
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOpParam(ITM_Xex, 9);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "# 4 %s", nMULT);
  ppcTestExpectSig("T20 x<>reg", expect);
  ppcTestExpectHist("T20 hist", 1);

  /* T20b: FILL displaces, it does not delete. FILL overwrites Y..top
   * with X: displacement under DESIGN.md's segmentation rule.
   * A finished formula in a slot >= 1 must be filed, the same as the
   * CLSTK wipe site files it. The CLSTK control below checks the same
   * shadow state through a wipe site that does displace. */
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);      // 2+3 finished, root now rides up on the next lift
  ppcTestType("9");        // lifts the root into slot 1
  ppcTestOp(ITM_FILL);
  ppcTestExpectHist("T20b FILL files the displaced formula", 1);

  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("9");
  ppcTestOp(ITM_CLSTK);
  ppcTestExpectHist("T20b control: CLSTK files it", 1);

  // T21: RCL-arith against a slot that is UNKNOWN.
  // ppcDeepCopy returns PPC_UNKNOWN unchanged. The guard beside it
  // tests only PPC_NIL, so the sentinel is stored as a child. Every
  // consumer screens PPC_UNKNOWN, so no crash follows and nothing
  // claims to be a formula. The four asserts below check that, in
  // order, through the raw accessor.
  ppcTestReset();
  ppcTestType("5");
  ppcTestOpParam(ITM_RCLADD, (uint16_t)REGISTER_Z);   // slot 2 is UNKNOWN
  {
    char sig[128];
    ppcTestSig(sig, sizeof(sig));                     // must not crash

    // (1) the operation was classified and built a tree
    uint8_t raw = ppcTestCurrentRaw();
    if(raw == PPC_NIL) {
      ppTestFail("T21 RCL-arith built no tree at all — fixture never reached the state under test");
    }
    else {
      // (2) the sentinel is stored as a child, exactly as designed
      const ppcNode_t *nd = ppcNodeAt(raw);
      if(nd == NULL || nd->child[1] != PPC_UNKNOWN) {
        ppTestFail("T21 the UNKNOWN operand is not the right-hand child");
      }
      // (3) and the display path withholds it
      if(ppcCurrentFormulaRoot() != PPC_NIL) {
        ppTestFail("T21 a tree with an UNKNOWN operand was offered for display");
      }
      uint8_t built;
      ppReset();
      if(ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &built)) {
        ppTestFail("T21 a tree with an UNKNOWN operand was rendered as a formula");
      }
    }
    // (4) nothing was filed into history
    ppcTestExpectHist("T21 nothing emitted", 0);
  }

  // T22 (class test): the recorded "= result" must be the value the
  // formula actually had. ppcTestExpectHist compares counts only and
  // cannot see this class. The PPC_INVALIDATE emit RAN at DONE then,
  // after dispatch had overwritten the register it read. It now emits
  // at STAGE, while the register still holds the formula's value.
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3.7");
    ppcTestOp(ITM_ADD);                 // X = 5.7, formula 2 + 3.7
    ppcTestOp(ITM_IP);                  // unmodelled US_ENABLED -> invalidate
    uint16_t elen, eseq;
    const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
    if(e == NULL) {
      ppTestFail("T22 the superseded formula was not filed at all");
    }
    else {
      uint8_t root;
      ppReset();
      // withResult=true: if a result was recorded, it becomes an
      // " = x" tail. The correct outcomes are "no result recorded" or
      // "= 5.7". The post-dispatch value 5 must not appear.
      if(ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(root, sig, sizeof(sig));
        if(strstr(sig, "=") != NULL && strstr(sig, "5.7") == NULL) {
          ppTestFailures++;
          printf("prettyPrint test FAIL: T22 filed a result the formula never had ('%s')\n", sig);
        }
      }
    }
    lastErrorCode = 0;
  }

  /* T22b: a complex operand must be captured. A
   * complex34 is 32 bytes against a 16-byte node. PPN_VAL2 is the
   * two-child header DESIGN.md section 3 defines for it.
   *
   * Fixture: put a complex in Y, leave the shadow UNKNOWN there, then
   * multiply. STAGE's ppcEnsureKnown(1) must snapshot the complex. */
  {
    ppcTestReset();
    /* The complex goes in X, not Y. Typing the next literal lifts, so
     * the complex ends up in Y. The multiply's STAGE must snapshot it
     * there. */
    reallocateRegister(REGISTER_X, dtComplex34, 0, amNone);
    int32ToReal34(2, REGISTER_REAL34_DATA(REGISTER_X));
    int32ToReal34(3, REGISTER_IMAG34_DATA(REGISTER_X));
    ppcShadowInvalidate();          // both slots UNKNOWN
    ppcTestType("4");               // lifts: X=4, Y=the complex
    if(getRegisterDataType(REGISTER_Y) != dtComplex34) {
      ppTestFail("T22b setup: Y is not complex after the lift — fixture cannot reach the defect");
    }
    ppcTestOp(ITM_MULT);
    /* The T-line half: the tree must not be poisoned by an opaque leaf. */
    if(ppcCurrentFormulaRoot() == PPC_NIL) {
      ppTestFail("T22b a complex operand still withholds the whole formula");
    }
    /* The LIVE half is not pinned. ppfFromCaptureNode reassembles the
     * PPN_VAL2 continuation, which avoids a 16-byte overread past
     * nd->payload. Asserting the drawn text needs C47's complex
     * formatting spelled out, and a guessed expectation only
     * enshrines whatever ppfFormatStaged emits. Pin it against the
     * formatter's real output when available. */
    {
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T22b the live formula does not build with a complex operand");
      }
    }

    /* The history half. A formula files on displacement, so the wipe
     * is part of the fixture. */
    ppcTestOp(ITM_CLSTK);
    ppcTestExpectHist("T22b the complex formula files", 1);
  }

  /* T23b: the filed "= result" must stay the formula's own value
   * after a later STO. The pin reads the filed entry. */
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                             // X=5, the formula 2+3
    ppcTestType("9");                               // lifts: Y=5, X=9
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);  // Y := 9, displacing the formula
    ppcTestExpectHist("T23b the displaced formula files", 1);

    uint8_t filed = PP_NONE;
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                      PP_FONT_STANDARD, true, &filed)) {
      ppTestFail("T23b the filed entry does not decode");
    }
    else {
      if(!ppTreeHasRun(filed, "5")) {
        ppTestFail("T23b the filed result is not 5, the value the formula had");
      }
      if(ppTreeHasRun(filed, "9")) {
        ppTestFail("T23b the filed result is 9 — the value STO wrote, read after the store");
      }
    }
  }

  /* T24c: a stacked power must bracket its base on the capture
   * surfaces too. PP_SUP puts the outer exponent at the same height as
   * the inner one, so an unbracketed 3 cubed cubed draws flat and
   * reads as 3^33 for a value of 3^9. The builder brackets the base,
   * so no call site needs its own guard. */
  {
    ppcTestReset();
    ppcTestType("3");
    ppcTestOp(ITM_CUBE);
    ppcTestOp(ITM_CUBE);
    uint8_t pw = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &pw)) {
      ppTestFail("T24c the stacked power does not build");
    }
    else {
      ppfTestExpect("T24c stacked power brackets its base", pw, "S(P(S(3|3))|3)");
    }
  }

  /* T26: a short integer through ppfBuildEntry. dtShortInteger
   * formatting uses tmpString, sized against ERROR_MESSAGE_LENGTH and
   * checked by a _Static_assert in prettyFormula.c.
   * The operand's spelling depends on the base, so the assertion is
   * only that the entry decodes to text at all. */
  {
    ppcTestReset();
    ppcTestType("10");
    ppcTestOpParam(ITM_toINT, 16);   // integer mode, base 16
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T26 the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcTestType("5");
      ppcTestOp(ITM_ADD);
      ppcTestOp(ITM_CLSTK);          // displacing the formula files it
      ppcTestExpectHist("T26 the integer formula files", 1);
      uint8_t filed = PP_NONE;
      ppReset();
      if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                        PP_FONT_STANDARD, true, &filed)) {
        ppTestFail("T26 the filed integer entry does not decode");
      }
      else if(ppfTestFirstRunText(filed) == NULL
              || ppfTestFirstRunText(filed)[0] == 0) {
        ppTestFail("T26 the filed integer formula has no operand text");
      }
    }
    ppcTestReset();
  }

  /* T25: the class, over every producer of a PP_SUP. Rows are (steps,
   * expected signature). ppfTestPowersScoped checks the property over
   * each row's whole tree, which catches a nested producer inside a
   * shape a row already types. It does not reach a new producer that
   * no row drives, a fourth PP_SUP arm still needs its own row here. */
  {
    static const struct { const char *what; uint16_t op1; const char *lit; uint16_t op2; const char *sig; } powRows[] = {
      { "T25 x2 over x2",  ITM_SQUARE, NULL, ITM_SQUARE, "S(P(S(3|2))|2)" },
      { "T25 x3 over x3",  ITM_CUBE,   NULL, ITM_CUBE,   "S(P(S(3|3))|3)" },
      { "T25 yx over x2",  ITM_SQUARE, "2",  ITM_YX,     "S(P(S(3|2))|2)" },
    };
    for(uint8_t r = 0; r < sizeof(powRows) / sizeof(powRows[0]); r++) {
      ppcTestReset();
      ppcTestType("3");
      ppcTestOp((int16_t)powRows[r].op1);
      if(powRows[r].lit != NULL) {
        ppcTestType(powRows[r].lit);
      }
      ppcTestOp((int16_t)powRows[r].op2);
      uint8_t pw = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &pw)) {
        ppTestFail(powRows[r].what);
      }
      else {
        ppfTestExpect(powRows[r].what, pw, powRows[r].sig);
        if(!ppfTestPowersScoped(pw)) {
          ppTestFail("T25 a power's base is itself a power, unbracketed");
        }
        // the same picture must survive filing (PP18RR7-1's class)
        ppfTestFiledMatchesLive(powRows[r].what);
      }
    }

    /* yx over yx: two operands, so the base is built by the OP2 arm
     * from a node the OP2 arm built. It reaches the same builder on
     * both capture surfaces: the live tree here, the filed entry
     * below through the token decoder. */
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_YX);
    ppcTestType("2");
    ppcTestOp(ITM_YX);
    uint8_t yx = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &yx)) {
      ppTestFail("T25 yx over yx does not build");
    }
    else {
      ppfTestExpect("T25 yx over yx brackets its base", yx, "S(P(S(2|3))|2)");
    }

    ppcTestOp(ITM_CLSTK);   // displacing the formula files it
    ppcTestExpectHist("T25 the stacked power files", 1);
    uint8_t filed = PP_NONE;
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                      PP_FONT_STANDARD, false, &filed)) {
      ppTestFail("T25 the filed stacked power does not decode");
    }
    else if(!ppfTestPowersScoped(filed)) {
      ppTestFail("T25 the FILED power's base is itself a power, unbracketed");
    }

    /* The base need not be a PP_SUP node. A leaf read from a
     * register is formatted for display. A large value arrives as one
     * flat run that ends in its own exponent's superscript digits,
     * and a kind test reads (1x10^50) squared as an exponent of 502.
     * A typed literal keeps its typed text and never has the tail, so
     * this row uses RCL.
     * The SUB-10 glyph is the reach check: without it the row passes
     * while testing nothing. */
    /* The class is not "already a power", it is "the base run is not
     * a visual atom". A typed negative reads as a term. Both the
     * capture leaf and the walker ask ppfTextIsAtom, so they cannot
     * disagree. */
    ppcTestReset();
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_SQUARE);
    uint8_t neg = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &neg)) {
      ppTestFail("T25 the negated square does not build");
    }
    else {
      const char *nleaf = ppfTestFirstRunText(neg);
      if(nleaf == NULL || nleaf[0] != '-') {
        ppTestFail("T25 the literal is not negative, so the row tests nothing");
      }
      else {
        ppfTestExpect("T25 a signed numeral brackets as a term", neg, "S(P(-5)|2)");
        ppfTestFiledMatchesLive("T25 filed signed numeral");
      }
    }

    /* The typed form of the same value is the other half of the class.
     * It keeps the owner's text, so its exponent is ASCII (1.e+50),
     * and the glyph test cannot see it. */
    ppcTestReset();
    ppcTestType("1");
    addItemToNimBuffer(ITM_EXPONENT);
    ppcTestType("50");
    ppcTestOp(ITM_SQUARE);
    uint8_t typed = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &typed)) {
      ppTestFail("T25 the typed scientific power does not build");
    }
    else {
      const char *tleaf = ppfTestFirstRunText(typed);
      const ppNode_t *troot = ppNodeAt(typed);
      const ppNode_t *tbase = (troot != NULL) ? ppNodeAt(troot->firstChild) : NULL;
      if(tleaf == NULL || strstr(tleaf, "e+") == NULL) {
        ppTestFail("T25 the typed value carries no ASCII exponent, so the row tests nothing");
      }
      else if(troot == NULL || troot->kind != PP_SUP) {
        ppTestFail("T25 the typed scientific value is not a power");
      }
      else if(tbase == NULL || tbase->kind != PP_PAREN) {
        ppTestFail("T25 a squared typed scientific value draws its exponent against the owner's");
      }
      ppfTestFiledMatchesLive("T25 filed typed scientific power");
    }

    ppcTestReset();
    ppcTestType("1");
    addItemToNimBuffer(ITM_EXPONENT);
    ppcTestType("50");
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);
    ppcTestOpParam(ITM_RCL, (uint16_t)REGISTER_Y);
    ppcTestOp(ITM_SQUARE);
    uint8_t sci = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &sci)) {
      ppTestFail("T25 the scientific-form power does not build");
    }
    else {
      const char *leaf = ppfTestFirstRunText(sci);
      const ppNode_t *root = ppNodeAt(sci);
      const ppNode_t *base = (root != NULL) ? ppNodeAt(root->firstChild) : NULL;
      if(leaf == NULL || strstr(leaf, STD_SUB_10) == NULL) {
        ppTestFail("T25 the value never reached scientific form, so the row tests nothing");
      }
      else if(root == NULL || root->kind != PP_SUP) {
        ppTestFail("T25 the squared scientific value is not a power");
      }
      else if(base == NULL || base->kind != PP_PAREN) {
        // a structural check
        ppTestFail("T25 a squared scientific value draws its two exponents as one");
      }
      ppfTestFiledMatchesLive("T25 filed scientific power");
    }
  }

  /* T27 (PP18RR7-1): the filed picture equals the live one for the
   * bracket-bearing operand shapes under MULT and SUB. */
  {
    ppcTestReset();
    ppcTestType("3");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_MULT);
    ppfTestFiledMatchesLive("T27 filed MULT keeps the signed bracket");

    ppcTestReset();
    ppcTestType("7");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    addItemToNimBuffer(ITM_CHS);
    ppcTestOp(ITM_SUB);
    ppfTestFiledMatchesLive("T27 filed SUB keeps the signed bracket");
    ppcTestReset();
  }

  /* T28 (PP18RR7-2): for every base, the widest word must decode on
   * the filed surface. Upstream draws each of these on one line. */
  {
    static const uint16_t bases[] = { 2, 4, 8, 10, 16 };
    for(size_t b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
      ppcTestReset();
      ppcTestType("0");
      ppcTestOpParam(ITM_toINT, bases[b]);
      ppcTestOp(ITM_ENTER);
      ppcTestType("1");
      ppcTestOpParam(ITM_toINT, bases[b]);
      ppcTestOp(ITM_SUB);
      if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
        ppTestFailInt("T28 the value is not a short integer, so the row tests nothing",
                      (int)bases[b], (int)getRegisterDataType(REGISTER_X));
        continue;
      }
      ppcTestOp(ITM_CLSTK);
      uint8_t filed = PP_NONE;
      ppReset();
      if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                        PP_FONT_STANDARD, true, &filed)) {
        ppTestFailInt("T28 the widest word does not decode in this base",
                      (int)bases[b], 0);
      }
    }
    ppcTestReset();
  }

  /* T32 (PP18RR8-6, ruled option A): a based integer is one numeral —
   * the base subscript is part of its spelling, like the group spaces —
   * so an integer value leaf draws without brackets, on both surfaces.
   * 0xa469 is the base-10 subscript (0xa461 + base - 2). */
  {
    ppcTestReset();
    ppcTestType("10");
    ppcTestOpParam(ITM_toINT, 10);
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T32 the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcTestType("2");
      ppcTestOp(ITM_MULT);
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T32 the based product does not build, so the row tests nothing");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(live, sig, sizeof(sig));
        if(strstr(sig, "\xa4\x69") == NULL) {
          ppTestFail("T32 the leaf lost its base subscript, so the row tests nothing");
        }
        else if(strstr(sig, "P(") != NULL) {
          ppTestFail("T32 a based numeral drew bracketed");
        }
        else {
          ppfTestFiledMatchesLive("T32 filed based numeral");
        }
      }
    }
    ppcTestReset();
  }

  /* T32b (PP18RR9-2, -7): the ruling covers every base and every digit.
   * One row per base 2..16 with the value 2*base-1, whose spelling is
   * '1' plus the base's highest digit plus the base subscript — so the
   * loop drives both window edges (0xa461, 0xa46f) and the hex letters
   * A..F. No row brackets. */
  {
    for(uint16_t b = 2; b <= 16; b++) {
      ppcTestReset();
      // the multiplier types FIRST: after →INT the entry mode is base
      // b, and '2' is not a digit there for b == 2
      ppcTestType("2");
      ppcTestOp(ITM_ENTER);
      char dec[4];
      snprintf(dec, sizeof(dec), "%u", (unsigned)(2 * b - 1));
      ppcTestType(dec);
      ppcTestOpParam(ITM_toINT, b);
      if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
        ppTestFailInt("T32b the value is not a short integer, so the row tests nothing",
                      (int)b, (int)getRegisterDataType(REGISTER_X));
        continue;
      }
      ppcTestOp(ITM_MULT);
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFailInt("T32b the based product does not build", (int)b, 0);
        continue;
      }
      char sig[192];
      sig[0] = 0;
      ppfTestSigNode(live, sig, sizeof(sig));
      char sub[3] = { (char)0xa4, (char)(0x61 + b - 2), 0 };
      if(strstr(sig, sub) == NULL) {
        ppTestFailInt("T32b the leaf lost its base subscript, so the row tests nothing",
                      (int)b, 0);
      }
      else if(strstr(sig, "P(") != NULL) {
        ppTestFailInt("T32b a based numeral drew bracketed", (int)b, 1);
      }
    }
    ppcTestReset();
  }

  /* T32c (PP18RR9-2, the wide half): past the plain-digit width limit
   * the builder re-spells base 2 with the binary glyphs 0xa20e/0xa027.
   * That spelling is still one numeral and draws bare. */
  {
    ppcTestReset();
    ppcTestType("0");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_SUB);          // all 64 bits set at the default WSIZE
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T32c the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcShadowInvalidate();   // forget the 0-1 formula: the product's
                               // left side must be a VALUE leaf
      lastIntegerBase = 0;     // leave base-2 entry mode so '2' types
      ppcTestType("2");
      ppcTestOp(ITM_MULT);
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T32c the wide binary product does not build");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(live, sig, sizeof(sig));
        char bin[3] = { (char)0xa2, (char)0x0e, 0 };
        if(strstr(sig, bin) == NULL) {
          ppTestFail("T32c the spelling never reached the binary glyphs, so the row tests nothing");
        }
        else if(strstr(sig, "P(") != NULL) {
          ppTestFail("T32c a wide binary numeral drew bracketed");
        }
      }
    }
    ppcTestReset();
  }

  /* T34 (PP18RR9-1): a filed row the fonts cannot draw still holds its
   * place in the PHIST pager as a placeholder line. The fixture is the
   * round-9 probe: a RAD polar complex, too wide for the standard rung,
   * with the 0x82b3 unit suffix that tinyFont lacks. */
  {
    ppcTestReset();
    ppcTestOp(ITM_PCLR);                   // an empty history isolates the row
    reallocateRegister(REGISTER_X, dtComplex34, 0, amNone);
    int32ToReal34(3, REGISTER_REAL34_DATA(REGISTER_X));
    int32ToReal34(4, REGISTER_IMAG34_DATA(REGISTER_X));
    setComplexRegisterAngularMode(REGISTER_X, amRadian);
    setComplexRegisterPolarMode(REGISTER_X, amPolar);
    ppcShadowInvalidate();
    ppcTestType("123456789012345678901234567890");
    ppcTestOp(ITM_MULT);
    ppcTestOp(ITM_CLSTK);                  // files the one undrawable row
    ppcTestExpectHist("T34 the RAD polar row files", 1);
    uint8_t probeRoot;
    int16_t probeAsc, probeH;
    if(ppfBuildRow(0, 0, false, &probeRoot, &probeAsc, &probeH)) {
      ppTestFail("T34 the row builds after all, so the fixture tests nothing");
    }
    else {
      calcMode = CM_PRETTY_BROWSER;   // the pager paints only in-mode;
                                      // any other mode routes to the browser
      lastErrorCode = 0;
      lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
      fnPrettyHist(NOPARAM);
      calcMode = CM_NORMAL;
      if(ppvSumRows(25, 163) == 0) {
        ppTestFail("T34 an undrawable filed row vanished from the pager with no placeholder");
      }
      screenHoldsDrawnPixels = false;
    }
    ppcTestOp(ITM_PCLR);
    ppcTestReset();
  }

  /* T33 (PP18RR9-1): every spelling a value leaf can emit, enumerated
   * against the three fonts. A cell marked noTiny is an ACCEPTED
   * decline: the tiny rung refuses that spelling and the pager shows
   * the placeholder line. Every other cell must resolve in all three
   * fonts, so a formatter or font change that widens a fatal alphabet
   * reddens here instead of shipping a vanished row. */
  {
    real34_t r;
    struct { real34_t re, im; } cp;
    uint64_t sword = 0x2aULL;
    int32ToReal34(5, &r);
    int32ToReal34(3, &cp.re);
    int32ToReal34(4, &cp.im);
    static const struct { const char *what; uint8_t dt; uint8_t tag; uint8_t noTiny; } cells[] = {
      { "real none",         dtReal34,       amNone,                        0 },
      { "real degree",       dtReal34,       amDegree,                      0 },
      { "real radian",       dtReal34,       amRadian,                      1 },
      { "real grad",         dtReal34,       amGrad,                        1 },
      { "real dms",          dtReal34,       amDMS,                         0 },
      { "complex rect",      dtComplex34,    amNone,                        0 },
      { "complex polar deg", dtComplex34,    (uint8_t)(amDegree | amPolar), 0 },
      { "complex polar rad", dtComplex34,    (uint8_t)(amRadian | amPolar), 1 },
      { "shortint base 16",  dtShortInteger, 16,                            0 },
    };
    const font_t *fonts[3] = { &numericFont, &standardFont, &tinyFont };
    for(size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
      const uint8_t *pay;
      uint8_t bytes;
      if(cells[i].dt == dtReal34)         { pay = (const uint8_t *)&r;     bytes = 16; }
      else if(cells[i].dt == dtComplex34) { pay = (const uint8_t *)&cp;    bytes = 32; }
      else                                { pay = (const uint8_t *)&sword; bytes = 8;  }
      char spell[200];
      if(!ppfTestStagedSpelling(cells[i].dt, cells[i].tag, pay, bytes, spell, sizeof(spell))) {
        ppTestFailInt("T33 a cell does not format, so it records nothing", (int)i, 0);
        continue;
      }
      for(int f = 0; f < 3; f++) {
        uint16_t missing = 0;
        for(uint16_t p2 = 0; spell[p2] != 0; ) {
          uint16_t code = (uint8_t)spell[p2];
          if(code >= 0x80) {
            code = (uint16_t)((code << 8) | (uint8_t)spell[p2 + 1]);
            p2 = (uint16_t)(p2 + 2);
          }
          else {
            p2++;
          }
          if(code == 0x0001 || (code >= 0xa000 && code <= 0xa00f)) {
            continue;   // space-class: no glyph needed
          }
          if(findGlyphExact(fonts[f], code) < 0) {
            missing = code;
          }
        }
        bool_t expectOk = (f != 2) || !cells[i].noTiny;
        if(expectOk && missing != 0) {
          printf("prettyPrint test FAIL: T33 %s: code 0x%04x missing from font %d\n",
                 cells[i].what, (unsigned)missing, f);
          ppTestFailures++;
        }
        else if(!expectOk && missing == 0) {
          printf("prettyPrint test FAIL: T33 %s: recorded as a tiny decline, and tiny now draws it — update the record\n",
                 cells[i].what);
          ppTestFailures++;
        }
      }
    }
  }

  /* T29 (PP18RR8-1): an unknown glyph fails the run in EVERY font.
   * findGlyph's id-based fallback reports a tinyFont miss as glyph 0,
   * so the eˣ catalog name (0xa147 0x82e3) measured and painted as
   * blanks. Digit-group spaces stay measurable: they are space-class
   * in every font. */
  {
    ppReset();
    uint8_t bad = ppNewRun("\xa1\x47", 2, PP_FONT_TINY);
    if(bad == PP_NONE) {
      ppTestFail("T29 the run does not build, so the row tests nothing");
    }
    else if(ppMeasure(bad, 0)) {
      ppTestFail("T29 a glyph tinyFont lacks still measures");
    }
    ppReset();
    uint8_t sep = ppNewRun("1\xa0\x08" "2", 4, PP_FONT_TINY);
    if(sep == PP_NONE || !ppMeasure(sep, 0)) {
      ppTestFail("T29 a digit-group space must stay measurable in the tiny font");
    }
    ppReset();
    uint8_t badStd = ppNewRun("\xff\xfe", 2, PP_FONT_STANDARD);
    if(badStd == PP_NONE || ppMeasure(badStd, 0)) {
      ppTestFail("T29 an unknown glyph measures in the standard font");
    }
  }

  /* T30 (PP18RR8-3): when the text pool cannot hold the result run,
   * the entry DECLINES rather than paint without its "= result" tail.
   * LEAD.0 in base 2 at WSIZE 64 spells each value at 160 bytes, so
   * three value leaves and two operators leave no room for the tail
   * (the recall path still finds the TKRES and pushes a number the
   * row never showed). */
  {
    setSystemFlag(FLAG_LEAD0);
    ppcTestReset();
    ppcTestType("0");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_toINT, 2);
    ppcTestOp(ITM_SUB);
    if(getRegisterDataType(REGISTER_X) != dtShortInteger) {
      ppTestFail("T30 the value is not a short integer, so the row tests nothing");
    }
    else {
      ppcTestOp(ITM_ENTER);
      ppcTestOp(ITM_ENTER);
      ppcTestOpParam(ITM_toINT, 2);   // unclassified: the slots go UNKNOWN
      ppcTestOp(ITM_ADD);             // each ADD mints a PPN_VAL leaf
      ppcTestOp(ITM_ADD);
      ppcTestOp(ITM_CLSTK);           // displacing the formula files it
      // two entries: the second ADD displaces the first ADD's formula,
      // which files it, and CLSTK files the second
      ppcTestExpectHist("T30 the wide integer formula files", 2);
      uint8_t filed = PP_NONE;
      ppReset();
      if(ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                       PP_FONT_STANDARD, true, &filed)) {
        ppTestFail("T30 a filed entry whose result run cannot fit painted without its tail");
      }
    }
    clearSystemFlag(FLAG_LEAD0);
    ppcTestReset();
  }

  /* T31 (PP18RR8-5): a polar-tagged complex keeps its polar spelling
   * on the formula view. The staged formatter passed a literal false
   * for tagPolar, so the leaf redrew as a+ib while the stack line
   * showed the polar form. 0xa221 is the measured-angle glyph. */
  {
    ppcTestReset();
    /* The pin asserts the DEGREE magnitude, so it sets degrees by its
     * own hand: an amNone staged tag displays in the ambient angular
     * mode, and inheriting it makes the assertion order-dependent
     * (found at PP19, when this driver moved to a later suite slot). */
    angularMode_t amWasT31 = currentAngularMode;
    currentAngularMode = amDegree;
    reallocateRegister(REGISTER_X, dtComplex34, 0, amNone);
    int32ToReal34(3, REGISTER_REAL34_DATA(REGISTER_X));
    int32ToReal34(4, REGISTER_IMAG34_DATA(REGISTER_X));
    setComplexRegisterPolarMode(REGISTER_X, amPolar);
    if(getComplexRegisterPolarMode(REGISTER_X) != amPolar) {
      ppTestFail("T31 the tag is not polar, so the row tests nothing");
    }
    else {
      ppcShadowInvalidate();
      ppcTestType("2");
      ppcTestOp(ITM_MULT);
      uint8_t live = PP_NONE;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
        ppTestFail("T31 the polar product does not build, so the row tests nothing");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(live, sig, sizeof(sig));
        if(strstr(sig, "\xa2\x21") == NULL) {
          ppTestFail("T31 a polar-tagged value leaf redrew in rectangular form");
        }
        else if(strstr(sig, "53.130") == NULL) {
          // the angle half of the two-argument fix: a regressed tag
          // prints the radian magnitude under a degree sign
          ppTestFail("T31 the angle is not the degree magnitude");
        }
      }
    }
    currentAngularMode = amWasT31;
    ppcTestReset();
  }

  /* FB1 (PP18RR8-2): the flag browser walks the catalog this build
   * actually generated. The row derives the row count the same way the
   * fixed browser does, then drives both system-flag screens. */
  {
    int16_t rows = -1;
    for(uint16_t m = 0; softmenu[m].menuItem != 0; m++) {
      if(softmenu[m].menuItem == -MNU_SYSFL) {
        rows = softmenu[m].numItems;
        break;
      }
    }
    if(rows < 61) {
      ppTestFailInt("FB1 the SYSFL catalog is too short to fill two screens", 61, (int)rows);
    }
    else if(prettySysflRows() != rows) {
      // the browser's bound and this row derive from the same table;
      // a drift between them is the walk reading past the array
      ppTestFailInt("FB1 the browser's bound disagrees with the catalog",
                    (int)rows, (int)prettySysflRows());
    }
    else {
      lastErrorCode = 0;
      flagBrowser(SYSTEM_FLAGS_SCREEN_1);
      currentFlgScr = SYSTEM_FLAGS_SCREEN_2;
      flagBrowser(SYSTEM_FLAGS_SCREEN_2);
      if(lastErrorCode != ERROR_NONE) {
        ppTestFailInt("FB1 the flag browser raised an error", 0, (int)lastErrorCode);
      }
      calcMode = CM_NORMAL;
      currentFlgScr = 0;
    }
    ppcTestReset();
  }

  /* T23c: a slot must be maintained wherever its register is
   * writable, even where the live stack does not reach. Fixture: fill
   * the slots under SSIZE8, switch to SSIZE4, store over A. The
   * shadow must no longer claim to know slot 4. */
  {
    bool_t ss8Was = getSystemFlag(FLAG_SSIZE8);
    setSystemFlag(FLAG_SSIZE8);
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ENTER);
    ppcTestType("4");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");                                 // slots 0..4 known

    /* Establish the state the assertion needs: slot 4 must actually hold a
     * tree before the store, or the pin proves nothing. */
    const uint8_t s4 = ppcTestSlotRaw(4);
    const bool_t slot4WasKnown = (s4 != PPC_UNKNOWN && s4 != PPC_NIL);

    clearSystemFlag(FLAG_SSIZE8);                     // A..D leave the stack, stay writable
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_A);    // upstream writes A = 5
    setSystemFlag(FLAG_SSIZE8);                       // and A is a stack register again

    /* Degraded means UNKNOWN. What it must not be is the tree it held
     * before the store. PPC_NIL is not accepted as degraded: it is
     * also what ppcTestSlotRaw returns out of range, so accepting it
     * lets the pin pass on a slot that was never populated. */
    if(!slot4WasKnown) {
      ppTestFail("T23c setup: slot 4 never held a tree, so the guard below cannot be seen");
    }
    else {
      const uint8_t slot4 = ppcTestSlotRaw(4);
      if(slot4 != PPC_UNKNOWN) {
        ppTestFail("T23c slot 4 was not degraded after a STO to A the guard ignored");
      }
    }
    if(ss8Was) { setSystemFlag(FLAG_SSIZE8); } else { clearSystemFlag(FLAG_SSIZE8); }
  }

  // T23: STO to a STACK register changes a value the shadow
  // claims. 7 ENTER 2 ENTER 3 + STO Y x: the display shows 7·(2+3)=25.
  {
    ppcTestReset();
    ppcTestType("7");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                          // X=5, Y=7
    ppcTestOpParam(ITM_STO, (uint16_t)REGISTER_Y);  // Y := 5
    ppcTestOp(ITM_MULT);                         // X = 5*5 = 25
    {
      // Assert the whole truthful shape: the overwritten Y degraded to
      // a value leaf, times the live (2+3).
      char expect23[64];
      sprintf(expect23, "# 2 3 %s %s",
              indexOfItems[ITM_ADD].itemCatalogName,
              indexOfItems[ITM_MULT].itemCatalogName);
      ppcTestExpectSig("T23 STO Y leaves a truthful value leaf, not the 7", expect23);

      char sig[128];
      ppcTestSig(sig, sizeof(sig));
      if(strstr(sig, "7") != NULL) {
        // sanitize: a signature carrying glyph bytes makes grep treat the
        // whole log as binary and swallow the FAIL line (TESTING.md trap)
        char safe[128];
        uint16_t si = 0;
        for(; sig[si] && si < sizeof(safe) - 1; si++) {
          safe[si] = ((uint8_t)sig[si] >= 32 && (uint8_t)sig[si] < 127) ? sig[si] : '?';
        }
        safe[si] = 0;
        ppTestFailures++;
        printf("prettyPrint test FAIL: T23 the shadow kept the overwritten register ('%s')\n", safe);
      }
    }
  }

  // T25: a formula too wide for the screen must still be in the
  // browser and pannable. Height stays a hard limit, width does not.
  {
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestOp(ITM_CLX);                 // file it
    ppcTestExpectHist("T25 the wide formula was filed", 1);
    {
      uint8_t root;
      int16_t asc, h;
      if(!ppfBuildRow(0, 0, true, &root, &asc, &h)) {
        ppTestFail("T25 a wide row is still dropped instead of panned");
      }
      else {
        const ppNode_t *n = ppNodeAt(root);
        if(n->width <= SCREEN_WIDTH - 8) {
          ppTestFail("T25 fixture is not actually wide enough to exercise panning");
        }
      }
    }
  }

  // T26: the literal-length boundary. The leaf holds two 15-byte
  // payloads, 30 characters. 30 must round-trip exactly. 31 must
  // withhold the formula.
  {
    static const char d30[] = "123456789012345678901234567890";
    static const char d31[] = "1234567890123456789012345678901";
    char expect26[64];
    const char *nADD26 = indexOfItems[ITM_ADD].itemCatalogName;

    ppcTestReset();
    ppcTestType(d30);
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ADD);
    sprintf(expect26, "%s 2 %s", d30, nADD26);
    ppcTestExpectSig("T26 a 30-character literal must round-trip", expect26);

    ppcTestReset();
    ppcTestType(d31);
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_ADD);
    {
      // The formula is withheld, so the signature is empty.
      ppcTestExpectSig("T26 a 31-character literal withholds the formula", "-");

      char sig[128];
      ppcTestSig(sig, sizeof(sig));
      if(strstr(sig, d30) != NULL) {
        ppTestFail("T26 a 31-character literal was truncated to 30 and shown as fact");
      }
    }
  }

  // T27: a superseded formula must still be recallable.
  // The emit happens at STAGE, where the register still holds the
  // value.
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3.7");
    ppcTestOp(ITM_ADD);                 // X = 5.7
    ppcTestOp(ITM_IP);                  // unmodelled -> invalidate + supersede
    uint16_t elen, eseq;
    const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
    if(e == NULL) {
      ppTestFail("T27 the superseded formula was not filed");
    }
    else {
      uint8_t root;
      ppReset();
      if(!ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
        ppTestFail("T27 the filed entry does not decode");
      }
      else {
        char sig[192];
        sig[0] = 0;
        ppfTestSigNode(root, sig, sizeof(sig));
        // it must carry a result, and that result must be the true one
        if(strstr(sig, "=") == NULL) {
          ppTestFail("T27 the filed formula has no result and can never be recalled");
        }
        else if(strstr(sig, "5.7") == NULL) {
          ppTestFail("T27 the filed result is not the value the formula had");
        }
      }
    }
    lastErrorCode = 0;
  }

  // T28: ppfBuildRow has two callers, and only one can pan. The same
  // wide row must be accepted for the panning caller and refused for
  // the one that cannot.
  {
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestOp(ITM_CLX);
    uint8_t root;
    int16_t asc, h;
    bool_t panning = ppfBuildRow(0, 0, true,  &root, &asc, &h);
    bool_t fixed   = ppfBuildRow(0, 0, false, &root, &asc, &h);
    if(!panning) {
      ppTestFail("T28 the panning caller lost the wide row again");
    }
    if(fixed) {
      ppTestFail("T28 the non-panning caller would paint a clipped formula");
    }
  }

  // T29: the browser's pan must reach the code, and the paint must
  // survive a negative origin. lcd_fill_rect takes uint32_t
  // coordinates, so a negative x wraps to a huge one and drops the
  // whole rule, while glyph ink still clips.
  {
    int16_t hadScrUpd = screenUpdatingMode;
    uint16_t hadMode = calcMode;
    screenUpdatingMode = SCRUPD_AUTO;
    temporaryInformation = TI_NO_INFO;
    ppcTestReset();
    ppcTestType("1234567890123456");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2345678901234567");
    ppcTestOp(ITM_ADD);
    ppcTestType("3456789012345678");
    ppcTestOp(ITM_ADD);
    ppcTestType("7");
    ppcTestOp(ITM_DIV);          // wide numerator under a long fill-drawn bar
    ppcTestOp(ITM_CLX);
    prettyBrowser(NOPARAM);
    {
      uint8_t rr; int16_t aa, hh;
      if(!ppfBuildRow(0, 0, true, &rr, &aa, &hh)
          || ppNodeAt(rr)->width <= SCREEN_WIDTH - 12) {
        ppTestFail("T29 fixture is not wide enough to pan; the pin is not exercising anything");
      }
    }
    refreshScreen(200);
    uint32_t runBefore = 0;
    for(uint32_t y = 21; y <= 167 && runBefore == 0; y++) {
      uint32_t run = 0;
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        run = lcd_buffer_pixel_on(x, y) ? run + 1 : 0;
        if(run >= 20) { runBefore = run; break; }
      }
    }
    prettyBrowserPan();
    refreshScreen(201);
    uint32_t runAfter = 0;
    for(uint32_t y = 21; y <= 167 && runAfter == 0; y++) {
      uint32_t run = 0;
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        run = lcd_buffer_pixel_on(x, y) ? run + 1 : 0;
        if(run >= 20) { runAfter = run; break; }
      }
    }
    if(runBefore == 0) {
      ppTestFail("T29 the unpanned row has no fill-drawn rule to test");
    }
    else if(runAfter == 0) {
      ppTestFail("T29 panning dropped the fill-drawn rule instead of clipping it");
    }
    prettyBrowserLeave();
    calcMode = hadMode;
    screenUpdatingMode = hadScrUpd;
    lastErrorCode = 0;
  }

  // P13: repainting the X line must not leave the previous value's
  // ink behind. This is safe only because the register line's own
  // refresh clears the band first. Upstream's clearRegisterLine()
  // calls are commented out at their call sites, so this dependency
  // matters. Measured: a 35-digit value, then a 3-glyph one repainted
  // over it, leaves 467 lit pixels. A short value on a freshly
  // cleared band paints the same count.
  {
    ppTestSetRealX("0.16666666666666666666666666666666667");
    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);

    ppTestSetRealX("0.75");
    refreshRegisterLine(REGISTER_X);       // deliberately NOT cleared first
    uint32_t over = 0;
    for(uint32_t y = PPT_BAND_TOP; y < PPT_BAND_TOP + PPT_BAND_ROWS; y++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, y)) over++;
      }
    }

    ppTestClearBand();
    refreshRegisterLine(REGISTER_X);
    uint32_t clean = 0;
    for(uint32_t y = PPT_BAND_TOP; y < PPT_BAND_TOP + PPT_BAND_ROWS; y++) {
      for(uint32_t x = 0; x < SCREEN_WIDTH; x++) {
        if(lcd_buffer_pixel_on(x, y)) clean++;
      }
    }

    if(clean == 0) {
      ppTestFail("P13 fixture painted nothing; the pin is not exercising anything");
    }
    else if(over != clean) {
      ppTestFailInt("P13 a repaint left the previous value's ink behind", (int32_t)clean, (int32_t)over);
    }
  }

  // P12: a fraction's numerator ink must not depend on which glyph
  // the denominator is. Nothing about FRAC layout gives the
  // denominator any say over the rows above the bar. num.relBase is
  // barTopRel - fracGap - num.descent, none of which reads the
  // denominator. The same numerator over three denominators of very
  // different ink height must light exactly the same pixels. Measured
  // over the numerator's own columns, so re-centering cannot flatter
  // it.
  {
    const char *dens[3] = { "8", "x", "." };
    uint32_t ink[3] = { 0, 0, 0 };

    for(int c = 0; c < 3; c++) {
      ppReset();
      uint8_t fr = ppNewBox(PP_FRAC, PP_FONT_STANDARD);
      uint8_t nn = ppNewRun("8", 1, PP_FONT_STANDARD);
      uint8_t dd = ppNewRun(dens[c], 1, PP_FONT_STANDARD);
      if(fr == PP_NONE || nn == PP_NONE || dd == PP_NONE) { ppTestFail("P12 build"); break; }
      ppAppendChild(fr, nn);
      ppAppendChild(fr, dd);
      if(!ppMeasure(fr, 0)) { ppTestFail("P12 measure"); break; }

      lcd_fill_rect(0, 60, SCREEN_WIDTH, 100, LCD_SET_VALUE);
      ppPaintAt(fr, 40, 120);

      // the numerator's own measured ink box, x and y both
      const ppNode_t *n = ppNodeAt(nn);
      const uint32_t nx0 = (uint32_t)(40 + n->relX);
      const uint32_t nx1 = nx0 + (uint32_t)n->width;
      const uint32_t ytop = (uint32_t)(120 + n->relBase - n->ascent);
      const uint32_t ybot = (uint32_t)(120 + n->relBase + n->descent);
      for(uint32_t y = ytop; y < ybot; y++) {
        for(uint32_t x = nx0; x < nx1; x++) {
          if(lcd_buffer_pixel_on(x, y)) ink[c]++;
        }
      }
    }

    if(ink[0] == 0) {
      ppTestFail("P12 numerator ink missing entirely");
    }
    else if(ink[1] != ink[0] || ink[2] != ink[0]) {
      printf("prettyPrint P12 probe: numerator ink over 8/8=%u 8/x=%u 8/.=%u\n",
             ink[0], ink[1], ink[2]);
      ppTestFail("P12 denominator glyph ate the numerator");
    }
  }

  // T16: abort while ASLIFT is set (straight after an operator
  // result). The deferred-lift design absorbs the upstream undo() for
  // free. A shadow that lifts at NIM open strands the tree one slot
  // up.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5<");     // open with ASLIFT set, then abort
  ppcTestType("6");
  ppcTestOp(ITM_MULT);
  sprintf(expect, "2 3 %s 6 %s", nADD, nMULT);
  ppcTestExpectSig("T16 abort under lift", expect);
  ppcTestExpectHist("T16 hist", 0);

  /* ==== Big operators ================================================== */

  // the label program: LBL "P" / x^2 / END (the pgmT shape from upstream's
  // covProgramFlow, with a package-local label name)
  {
    static const uint8_t pgmP[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 1, 'P',
      ITM_SQUARE,
      (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
    };
    ppcTestWriteAndLoadPgm(pgmP, sizeof(pgmP));
  }
  calcRegister_t bigLbl = findNamedLabel("P", GLOBAL_LABELS);
  if(bigLbl == INVALID_VARIABLE) {
    ppTestFail("B0 label P not registered");
  }
  else {
    // The integral/sum dispatches retarget the solver at label P and
    // clear SOLVER_STATUS_USES_FORMULA. Later suite files (deriv_cov)
    // assume the status they inherited. Drivers restore what they
    // touch, the same rule that covers aimBuffer.
    uint16_t savedSolverStatus   = currentSolverStatus;
    uint16_t savedSolverProgram  = currentSolverProgram;
    uint16_t savedSolverVariable = currentSolverVariable;
    calcRegister_t savedMvarLabel = currentMvarLabel;

    const char *nSIG = indexOfItems[ITM_SIGMAn].itemCatalogName;
    const char *nINT = indexOfItems[ITM_INTEGRAL_YX].itemCatalogName;

    // B1: 1 ENTER 10 ENTER 1 Sigma_n -> a BIGOP root whose value is the
    // result the dispatch left in X (sum of n^2, n=1..10 = 385)
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("10");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    sprintf(expect, "{#,#}%s", nSIG);
    ppcTestExpectSig("B1 sigma captured", expect);
    ppcTestExpectHist("B1 no early emission", 0);
    if(getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("B1 X not real34");
    }
    else {
      real34_t want;
      int32ToReal34(385, &want);
      if(!real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("B1 X != 385");
      }
    }

    // B2/B3: CLX displaces the BIGOP root -> it emits like any op root
    ppcTestOp(ITM_CLX);
    ppcTestExpectHist("B3 emitted on displacement", 1);

    // B4: decode the history entry through the layout builder. The
    // sig pins limit ORDER (under=from, over=to), the label-name
    // decode, and the node shape. withResult=false: the B1 value
    // check already pins the real34 result.
    {
      uint16_t elen, eseq;
      const uint8_t *e = ppcHistoryEntry(0, &elen, &eseq);
      uint8_t root;
      ppReset();
      if(e == NULL || !ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_TINY, false, &root)) {
        ppTestFail("B4 history entry decode");
      }
      else {
        // the HBOX sig joiner space-separates children: [n= 1]
        ppfTestExpect("B4 layout", root, "B(P(n)|[n= 1]|10)");
        if(!ppMeasure(root, 0)) {
          ppTestFail("B4 measure");
        }
        else {
          // B8: pixel pin for the stroke-drawn operator. Probe rows
          // [base-12, base-2], cols [x, x+22]. The glyph box is at
          // most colW wide (about 13 here), and the body run starts
          // past colW + 3. Body ink shares these ROWS, so the narrow
          // column bound is what excludes it. A wider probe once
          // masked a stroke deletion.
          lcd_fill_rect(0, 60, SCREEN_WIDTH, 84, LCD_SET_VALUE);
          ppPaintAt(root, 10, 120);
          if(!ppTestRectAnyLit(108, 118, 10, 22)) {
            ppTestFail("B8 operator strokes missing");
          }
        }
      }
    }

    // B5: the BIGOP result chains like any operand, no early emission
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestType("2");
    ppcTestOp(ITM_MULT);
    sprintf(expect, "{#,#}%s 2 %s", nSIG, nMULT);
    ppcTestExpectSig("B5 sigma chains", expect);
    ppcTestExpectHist("B5 hist", 0);   // the reset above cleared the ring
    ppcTestOp(ITM_CLX);
    ppcTestExpectHist("B5 chained formula emitted", 1);

    // B10: a big operator whose program fails partway through must
    // leave no formula behind. The hooks nest: a dispatch runs a
    // program whose every step re-enters them.
    {
      static const uint8_t pgmE[] = {
        ITM_LBL, STRING_LABEL_VARIABLE, 1, 'E',
        ITM_LITERAL, STRING_REAL34, 1, '6',
        ITM_SUB,                                   // n - 6
        ITM_1ONX,                                  // 1/(n-6): divides by zero at n=6
        (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
      };
      ppcTestWriteAndLoadPgm(pgmE, sizeof(pgmE));
      calcRegister_t eLbl = findNamedLabel("E", GLOBAL_LABELS);
      if(eLbl != INVALID_VARIABLE) {
        ppcTestReset();
        ppcTestType("4");
        ppcTestOp(ITM_ENTER);
        ppcTestType("8");
        ppcTestOp(ITM_ENTER);
        ppcTestType("1");
        ppcTestOpParam(ITM_SIGMAn, (uint16_t)eLbl);
        // The only pin for the dispatch-depth pairing. Assert that we
        // reached the failure.
        if(lastErrorCode == ERROR_NONE) {
          ppTestFail("B10 fixture no longer fails mid-loop; the depth pin is not being exercised");
        }
        else {
          // the run failed: nothing claims to describe the register
          ppcTestExpectSig("B10 failed sum left a formula behind", "-");
        }
        lastErrorCode = 0;
      }
    }

    // B11: the browser must be able to recall a formula containing a
    // big operator. Two decoders read one token stream.
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);   // X = 55
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);                             // supersede -> files the Sigma WITH its result
    if(ppcHistoryCount() < 1) {
      ppTestFail("B11 the sigma formula was not filed");
    }
    else {
      uint16_t hadMode = calcMode;
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
      int32ToReal34(-1, REGISTER_REAL34_DATA(REGISTER_X));   // a value the recall must replace
      prettyBrowser(NOPARAM);
      prettyBrowserDown();          // off the live row, onto the filed sigma
      prettyBrowserEnter();
      real34_t want55;
      int32ToReal34(55, &want55);
      if(getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want55)) {
        ppTestFail("B11 ENTER did not recall the big-operator result into X");
      }
      calcMode = hadMode;
      lastErrorCode = 0;
    }

    #if defined(OPTION_INFSUMS)
    // B9: the early-stop sum captures like any other sum. It reads the
    // same three stack levels, so its node carries the real limits the
    // user gave it.
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAnINF, (uint16_t)bigLbl);
    // A fixture must prove it reached the state it claims to test.
    // Assert the run succeeded, then assert what it produced.
    if(lastErrorCode != ERROR_NONE) {
      ppTestFailInt("B9 the infinite sum did not run", 0, (int32_t)lastErrorCode);
      lastErrorCode = 0;
    }
    else {
      sprintf(expect, "{#,#}%s", indexOfItems[ITM_SIGMAnINF].itemCatalogName);
      ppcTestExpectSig("B9 infinite sum captured", expect);
    }
    #endif // OPTION_INFSUMS

    // B6: the dispatch that actually integrates: PGMINT preselects the
    // label program, and the INTEGRAL_YX param is the integration
    // VARIABLE (the covIntegratePgm currency). ACC=0 -> default
    // tolerance, as the upstream fixture does.
    ppcTestReset();
    currentSolverStatus = 0;
    reallocateRegister(RESERVED_VARIABLE_ACC, dtReal34, 0, amNone);
    int32ToReal34(0, REGISTER_REAL34_DATA(RESERVED_VARIABLE_ACC));
    ppcTestOpParam(ITM_PGMINT, (uint16_t)bigLbl);
    ppcTestType("0");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_INTEGRAL_YX, findOrAllocateNamedVariable("X"));
    sprintf(expect, "{#,#}%s", nINT);
    ppcTestExpectSig("B6 integral captured", expect);
    if(getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("B6 X not real34");
    }
    else {
      // integral of x^2 over [0,1] = 1/3: pin |3X - 1| < 1e-6
      real34_t three, one, diff, tol;
      int32ToReal34(3, &three);
      int32ToReal34(1, &one);
      real34Multiply(REGISTER_REAL34_DATA(REGISTER_X), &three, &diff);
      real34Subtract(&diff, &one, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail("B6 X != 1/3");
      }
    }

    // B6b: the label-param form is setup only. No result
    // exists, so no node claims one. It still consumed X and Y as
    // limits, so the shadow invalidates.
    ppcTestReset();
    ppcTestType("5");
    ppcTestOp(ITM_ENTER);
    ppcTestType("7");
    ppcTestOpParam(ITM_INTEGRAL_YX, (uint16_t)bigLbl);
    ppcTestExpectSig("B6b setup form does not lie", "-");

    // B7: a non-unit step is visible in the under-limit, or the
    // display lies: 1 ENTER 9 ENTER 2 -> n=1,(delta)2 under, 9 over
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("9");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestOp(ITM_CLX);
    {
      uint16_t elen, eseq;
      const uint8_t *e = ppcHistoryEntry((uint8_t)(ppcHistoryCount() - 1), &elen, &eseq);
      uint8_t root;
      ppReset();
      if(e == NULL || !ppfBuildEntry(e, PP_FONT_STANDARD, PP_FONT_TINY, false, &root)) {
        ppTestFail("B7 step entry decode");
      }
      else {
        // the step travels as a real34 (upstream's fnToReal currency), so
        // it renders with the real marker: (delta)2.
        sprintf(expect, "B(P(n)|[n= 1 ," "\x83\x94" "2.]|9)");
        ppfTestExpect("B7 step visible", root, expect);
      }
    }

    /* B10: the captured big operator used as an operand, built
     * through the capture engine's own precedence threading. A big
     * operator's body is drawn to
     * the right of the stroke, so a factor beside it binds into the
     * body unless it brackets. Every PSHOW and PHIST of a programmed
     * sum reaches the same builder. Its own capture, so it disturbs
     * nothing above it. */
    ppcTestReset();
    ppcTestType("1");
    ppcTestOp(ITM_ENTER);
    ppcTestType("10");
    ppcTestOp(ITM_ENTER);
    ppcTestType("1");
    ppcTestOpParam(ITM_SIGMAn, (uint16_t)bigLbl);
    ppcTestType("2");
    ppcTestOp(ITM_MULT);
    {
      uint8_t rootB10;
      ppReset();
      if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_TINY, &rootB10)) {
        ppTestFail("B10 the captured product did not build");
      }
      else {
        char wantB10[96];
        sprintf(wantB10, "[P(B(P(n)|[n= 1]|10)) %s 2]", STD_DOT);
        ppfTestExpect("B10 a captured sum brackets as an operand", rootB10, wantB10);
      }
    }

    currentSolverStatus   = savedSolverStatus;
    currentSolverProgram  = savedSolverProgram;
    currentSolverVariable = savedSolverVariable;
    currentMvarLabel      = savedMvarLabel;
  }

  // T24: R/S resumes a stopped program that then rewrites the stack
  // with every step out of scope, so nothing tells the shadow. XEQ is
  // the same operation by the other key and is US_ENABLED, so the
  // default rule covers it. R/S is US_UNCHANGED, so the default rule
  // alone ignores it. Pins the classification: after R/S nothing
  // still claims to describe a register.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  {
    // R/S drives the program runner, so it moves machine state a test
    // driver owns: put every piece of it back (the aimBuffer rule,
    // applied to the runner).
    uint16_t hadRunStop = programRunStop;
    uint16_t hadPgm = currentProgramNumber;
    ppcTestOp(ITM_RS);
    ppcTestExpectSig("T24 R/S left the shadow describing stale registers", "-");
    programRunStop = hadRunStop;
    currentProgramNumber = hadPgm;
    lastErrorCode = 0;
    ppcTestReset();
  }

  /* T24b: SST. fnSst only sets PGM_SINGLE_STEP. The key handler then
   * runs one program step with PGM_RUNNING, so every nested hook fails
   * ppcScopeOk and returns without mirroring or invalidating. The
   * step's stack motion is recorded nowhere. */
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  {
    uint16_t hadRunStop = programRunStop;
    uint16_t hadPgm = currentProgramNumber;
    ppcTestOp(ITM_SST);
    ppcTestExpectSig("T24b SST left the shadow describing stale registers", "-");
    programRunStop = hadRunStop;
    currentProgramNumber = hadPgm;
    lastErrorCode = 0;
    ppcTestReset();
  }

  ppcTestReset();
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestFormula =================================================
 * Layout signatures: RUN -> its text with spaces stripped. HBOX ->
 * [children space-joined]. FRAC -> F(a|b). SUP -> S(a|b). RAD -> R(a).
 * PAREN -> P(a). Expected strings build from catalog names at runtime. */


static const char *ppfTestFirstRunText(uint8_t n) {
  const ppNode_t *nd = ppNodeAt(n);
  if(nd == NULL) {
    return NULL;
  }
  if(nd->kind == PP_RUN) {
    return ppTextAt(nd->textOff);
  }
  for(uint8_t c = nd->firstChild; c != PP_NONE; c = ppNodeAt(c)->nextSibling) {
    const char *t = ppfTestFirstRunText(c);
    if(t != NULL) {
      return t;
    }
  }
  return NULL;
}

static void ppfTestFiledMatchesLive(const char *what) {
  uint8_t live = PP_NONE, filed = PP_NONE;
  char liveSig[192], filedSig[192], msg[256];
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &live)) {
    snprintf(msg, sizeof(msg), "%s: the live formula does not build, so the row tests nothing", what);
    ppTestFail(msg);
    return;
  }
  liveSig[0] = 0;
  ppfTestSigNode(live, liveSig, sizeof(liveSig));
  ppcTestOp(ITM_CLSTK);
  ppReset();
  if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD,
                    PP_FONT_STANDARD, false, &filed)) {
    snprintf(msg, sizeof(msg), "%s: the filed entry does not decode", what);
    ppTestFail(msg);
    return;
  }
  filedSig[0] = 0;
  ppfTestSigNode(filed, filedSig, sizeof(filedSig));
  if(strcmp(liveSig, filedSig) != 0) {
    ppTestFailures++;
    printf("prettyPrint test FAIL: %s (live '%s', filed '%s')\n", what, liveSig, filedSig);
  }
}

void prettyTestFormula(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  char expect[192];
  uint8_t root;
  const char *nADD  = indexOfItems[ITM_ADD].itemCatalogName;

  // FV1: precedence parens, (2+3)×4
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("4");
  ppcTestOp(ITM_MULT);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV1 build");
  }
  else {
    // multiplication typesets as the raised dot in the layout
    sprintf(expect, "[P([2 %s 3]) " STD_DOT " 4]", nADD);
    ppfTestExpect("FV1 precedence", root, expect);
  }

  // FV2: division becomes a stacked fraction, children unparenthesized
  ppcTestReset();
  ppcTestType("6");
  ppcTestOp(ITM_ENTER);
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestOp(ITM_DIV);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV2 build");
  }
  else {
    sprintf(expect, "F(6|[2 %s 3])", nADD);
    ppfTestExpect("FV2 div as fraction", root, expect);
  }

  // FV3: history entry decodes with its result
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  if(ppcHistoryCount() != 1) {
    ppTestFailInt("FV3 hist", 1, ppcHistoryCount());
  }
  else {
    ppReset();
    if(!ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), PP_FONT_STANDARD, PP_FONT_STANDARD, true, &root)) {
      ppTestFail("FV3 decode");
    }
    else {
      sprintf(expect, "[[2 %s 3] = 5]", nADD);
      ppfTestExpect("FV3 entry with result", root, expect);
    }
  }

  // FV4: sqrt scopes without parens, square wraps in SUP
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_SQUAREROOTX);
  ppcTestOp(ITM_SQUARE);
  ppReset();
  if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
    ppTestFail("FV4 build");
  }
  else {
    ppfTestExpect("FV4 sqrt+square", root, "S(R(2)|2)");
  }

  // FV5: PHIST pager paints frames and arms the protocol. PCLR empties
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  temporaryInformation = TI_NO_INFO;
  fnPrettyHist(NOPARAM);
  if(!ppTestRowAllLit(20, 0, SCREEN_WIDTH - 1))  ppTestFail("FV5 frame 20");
  if(!ppTestRowAllLit(168, 0, SCREEN_WIDTH - 1)) ppTestFail("FV5 frame 168");
  if(!ppTestRectAnyLit(21, 167, 0, SCREEN_WIDTH - 1)) ppTestFail("FV5 no content ink");
  if(calcMode != CM_PRETTY_BROWSER) ppTestFail("FV5 browser mode not entered");
  prettyBrowserLeave();
  if(calcMode == CM_PRETTY_BROWSER) ppTestFail("FV5 leave did not restore mode");
  fnPrettyHistClear(NOPARAM);
  if(ppcHistoryCount() != 0) ppTestFailInt("FV5 PCLR", 0, ppcHistoryCount());
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV6: a formula too tall for the pager's standard rung must still
  // show. The tiny rung re-fonts the whole tree. Build the 3-level
  // continued fraction 1/(2+3/(4+5/6)) through the real key paths.
  ppcTestReset();
  ppcTestType("6");
  ppcTestOp(ITM_ENTER);
  ppcTestType("5");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  ppcTestType("4");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_ADD);
  ppcTestType("3");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  ppcTestType("2");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_ADD);
  ppcTestType("1");
  ppcTestOp(ITM_XexY);
  ppcTestOp(ITM_DIV);
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;
  temporaryInformation = TI_NO_INFO;
  fnPrettyHist(NOPARAM);
  if(!ppTestRectAnyLit(21, 56, 0, SCREEN_WIDTH - 1)) {
    ppTestFail("FV6 tall formula missing from the pager");
  }
  prettyBrowserLeave();
  screenUpdatingMode = SCRUPD_AUTO;
  screenHoldsDrawnPixels = false;

  // FV12: selection clamps at the last row, and ENTER recalls the
  // selected entry's result into X, restoring the mode and wiping the
  // shadow. The recall bypasses item dispatch.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_ADD);
  ppcTestType("5");
  ppcTestOp(ITM_ENTER);
  ppcTestType("6");
  ppcTestOp(ITM_ADD);
  calcMode = CM_NORMAL;
  temporaryInformation = TI_NO_INFO;
  lastErrorCode = 0;
  fnPrettyHist(NOPARAM);
  if(calcMode != CM_PRETTY_BROWSER) ppTestFail("FV12 browser not entered");
  prettyBrowserDown();
  prettyBrowserDown();   // over-navigation must clamp at the last row
  prettyBrowserDown();
  prettyBrowserEnter();
  if(calcMode == CM_PRETTY_BROWSER) ppTestFail("FV12 recall did not leave the browser");
  if(!ppTestIsLonI(REGISTER_X, 5)) ppTestFail("FV12 recalled result not in X");
  if(ppcCurrentFormulaRoot() != PPC_NIL) ppTestFail("FV12 shadow not invalidated after recall");

  // FV7: sqrt over a fraction, the synthesized tall sign. Measure must
  // succeed, and the sign strokes must leave ink left of the vinculum.
  ppcTestReset();
  ppcTestType("2");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_DIV);
  ppcTestOp(ITM_SQUAREROOTX);
  ppReset();
  {
    uint8_t root7;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root7)) {
      ppTestFail("FV7 build");
    }
    else {
      ppfTestExpect("FV7 sqrt of fraction", root7, "R(F(2|3))");
      if(!ppMeasure(root7, 0)) {
        ppTestFail("FV7 tall radical declined");
      }
      else {
        lcd_fill_rect(0, 60, 120, 80, LCD_SET_VALUE);
        const ppNode_t *n7 = ppNodeAt(root7);
        ppPaintAt(root7, 10, 100);
        // Columns 10..17 hold only the stroke sign. The vinculum
        // starts at column 19 (child relX-1).
        if(!ppTestRectAnyLit((uint32_t)(100 - n7->ascent), (uint32_t)(100 + n7->descent - 1), 10, 17)) {
          ppTestFail("FV7 synthesized sign missing");
        }
        lcd_fill_rect(0, 60, 120, 80, LCD_SET_VALUE);
      }
    }
  }

  // FV8: xth-root carries its index at the crook
  ppcTestReset();
  ppcTestType("27");
  ppcTestOp(ITM_ENTER);
  ppcTestType("3");
  ppcTestOp(ITM_XTHROOT);
  ppReset();
  {
    uint8_t root8;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root8)) {
      ppTestFail("FV8 build");
    }
    else {
      ppfTestExpect("FV8 indexed root", root8, "R(27;3)");
    }
  }

  // FV9: log with a subscript base. The script is lowered: a SUB
  // node's descent grows, unlike a SUP node's.
  ppcTestReset();
  ppcTestType("8");
  ppcTestOp(ITM_ENTER);
  ppcTestType("2");
  ppcTestOp(ITM_LOGXY);
  ppReset();
  {
    uint8_t root9;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root9)) {
      ppTestFail("FV9 build");
    }
    else {
      ppfTestExpect("FV9 log base", root9, "[U(log|2) P(8)]");
      if(ppMeasure(root9, 0)) {
        // Assert the script node itself is lowered: relBase must be
        // positive.
        const ppNode_t *r9 = ppNodeAt(root9);
        uint8_t sub9 = r9->firstChild;
        uint8_t script9 = (sub9 != PP_NONE) ? ppNodeAt(ppNodeAt(sub9)->firstChild)->nextSibling : PP_NONE;
        if(script9 == PP_NONE || ppNodeAt(script9)->relBase < 3) {
          ppTestFailInt("FV9 subscript not lowered", 3,
                        script9 == PP_NONE ? -99 : ppNodeAt(script9)->relBase);
        }
      }
    }
  }

  // FV10: absolute-value bars
  ppcTestReset();
  ppcTestType("5");
  ppcTestOp(ITM_MAGNITUDE);
  ppReset();
  {
    uint8_t root10;
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root10)) {
      ppTestFail("FV10 build");
    }
    else {
      ppfTestExpect("FV10 abs bars", root10, "A(5)");
      if(ppMeasure(root10, 0)) {
        lcd_fill_rect(0, 60, 120, 60, LCD_SET_VALUE);
        const ppNode_t *n10 = ppNodeAt(root10);
        ppPaintAt(root10, 10, 100);
        if(!ppTestRowAllLit((uint32_t)(100 - n10->ascent), 11, 12)) {
          ppTestFail("FV10 left bar missing");
        }
        lcd_fill_rect(0, 60, 120, 60, LCD_SET_VALUE);
      }
    }
  }

  // FV11: the T-line live formula is off by default (identity with
  // forced-off), shows the open formula when toggled on, and never
  // hijacks the X line
  {
    ppcTestReset();
    ppcTestType("2");
    ppcTestOp(ITM_ENTER);
    ppcTestType("3");
    ppcTestOp(ITM_ADD);
    calcMode = CM_NORMAL;
    temporaryInformation = TI_NO_INFO;
    lastErrorCode = 0;
    clearSystemFlag(FLAG_FRACT);
    clearSystemFlag(FLAG_IRFRAC);
    prettySetEnabled(true);

    // T band: baseY 24 -> rows 20..55
    #define PPT_T_TOP 20
    #define PPT_T_ROWS 36
    // default state (fresh driver, toggle untouched): must equal forced-off
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    ppTestCaptureBand(0, PPT_T_TOP, PPT_T_ROWS);
    prettySetTline(false);
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    ppTestCaptureBand(1, PPT_T_TOP, PPT_T_ROWS);
    if(!ppTestSnapsEqual()) {
      ppTestFail("FV11 T-line not OFF by default");
    }
    // toggled ON: the T line must DIFFER from the value render,
    // because the formula "2+3" paints there
    prettySetTline(true);
    lcd_fill_rect(0, PPT_T_TOP, SCREEN_WIDTH, PPT_T_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_T);
    // "2+3" right-aligned: some ink in the T band
    if(!ppTestRectAnyLit(PPT_T_TOP + 1, PPT_T_TOP + PPT_T_ROWS - 1, 300, SCREEN_WIDTH - 1)) {
      ppTestFail("FV11 T-line formula missing when enabled");
    }
    // and the X line stays a VALUE with the toggle on: X band identical
    // on/off (the branch must be T-only)
    lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_X);
    ppTestCapture(0);
    prettySetTline(false);
    lcd_fill_rect(0, PPT_BAND_TOP, SCREEN_WIDTH, PPT_BAND_ROWS, LCD_SET_VALUE);
    refreshRegisterLine(REGISTER_X);
    ppTestCapture(1);
    if(!ppTestSnapsEqual()) {
      ppTestFail("FV11 X line affected by the T-line toggle");
    }
    prettySetTline(false);
  }

  // FV13: the master toggle is the real system flag. The toggle item
  // flips it, and a reset restores the default-ON.
  {
    bool_t before = getSystemFlag(FLAG_PRETTYP);
    fnPrettyToggle(NOPARAM);
    if(getSystemFlag(FLAG_PRETTYP) == before) {
      ppTestFail("FV13 toggle does not flip FLAG_PRETTYP");
    }
    fnPrettyToggle(NOPARAM);
    prettySetEnabled(false);
    prettyReset();
    if(!getSystemFlag(FLAG_PRETTYP)) {
      ppTestFail("FV13 reset does not restore default-ON");
    }
    if(!prettyEnabled()) {
      ppTestFail("FV13 prettyEnabled does not read the flag");
    }
  }

  // FV14: the T line is a real flag too, and its default is reached
  // by the opposite route to the master toggle's. A reset wipes the
  // flags, and OFF is already the T line's default, so it must not be
  // re-set afterward the way FLAG_PRETTYP is.
  {
    prettySetTline(false);
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 prettySetTline(false) leaves the flag set");
    }
    fnPrettyTlineToggle(NOPARAM);
    if(!getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 toggle does not set FLAG_PTLINE");
    }
    fnPrettyTlineToggle(NOPARAM);
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 toggle does not clear FLAG_PTLINE");
    }
    setSystemFlag(FLAG_PTLINE);
    prettyReset();
    if(getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV14 reset does not leave the T line OFF");
    }
  }

  // FV17: the two toggles in our own menu show their state. The
  // generic checkbox path only fires for fnGetSystemFlag items inside
  // one particular menu, so ours is a package-local branch. This pins
  // it by rendering the softkey row twice with the flag opposite and
  // comparing ink. A filled box carries more ink than an outline. Per
  // the harness rule the assert is on ordering.
  {
    int16_t hadScrUpd = screenUpdatingMode;
    bool_t hadP = getSystemFlag(FLAG_PRETTYP);
    uint16_t hadMode = calcMode;
    screenUpdatingMode = SCRUPD_AUTO;
    temporaryInformation = TI_NO_INFO;
    calcMode = CM_NORMAL;
    showSoftmenu(-MNU_PP);
    softmenuStack[0].firstItem = 0;

    // the PPON softkey is cell 4 of 6: KEY_X[4]..KEY_X[5]
    uint32_t x0 = 268, x1 = 332, y0 = 218, y1 = 239;
    uint32_t inkOn = 0, inkOff = 0;
    setSystemFlag(FLAG_PRETTYP);
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    showSoftmenuCurrentPart();
    for(uint32_t yy = y0; yy <= y1; yy++) {
      for(uint32_t xx = x0; xx <= x1; xx++) {
        if(lcd_buffer_pixel_on(xx, yy)) inkOn++;
      }
    }
    clearSystemFlag(FLAG_PRETTYP);
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    showSoftmenuCurrentPart();
    for(uint32_t yy = y0; yy <= y1; yy++) {
      for(uint32_t xx = x0; xx <= x1; xx++) {
        if(lcd_buffer_pixel_on(xx, yy)) inkOff++;
      }
    }
    if(inkOn == 0 || inkOff == 0) {
      ppTestFail("FV17 the PPON softkey did not render at all");
    }
    else if(inkOn < inkOff + 8) {
      // Measured margin is 33 px (291 filled vs 258 outline). 8 is a
      // floor that still catches "the indicator stopped moving" without
      // pinning a literal count the font owns.
      ppTestFailInt("FV17 the state indicator does not track the flag",
                    (int32_t)inkOff + 8, (int32_t)inkOn);
    }
    lcd_fill_rect(0, 190, SCREEN_WIDTH, 50, LCD_SET_VALUE);
    if(hadP) setSystemFlag(FLAG_PRETTYP); else clearSystemFlag(FLAG_PRETTYP);
    screenUpdatingMode = hadScrUpd;
    calcMode = hadMode;
  }

  // FV16: the cold-start path initializes our data without touching
  // the user's flags.
  {
    clearSystemFlag(FLAG_PRETTYP);   // a user who turned it OFF
    setSystemFlag(FLAG_PTLINE);      // and turned the T line ON
    ppcTestDeinit();                 // ... then cold-starts
    ppcTestOp(ITM_ENTER);            // first dispatch: lazy init runs
    if(getSystemFlag(FLAG_PRETTYP)) {
      ppTestFail("FV16 cold start overwrote the user's PPRTY setting");
    }
    if(!getSystemFlag(FLAG_PTLINE)) {
      ppTestFail("FV16 cold start overwrote the user's PTLINE setting");
    }
    prettyReset();                   // back to defaults for later tests
  }

  // FV18: the builder must never report success with a child
  // silently missing. ppfParen allocates, and PP_HBOX is the one
  // variadic container. ppMeasure checks arity for
  // FRAC/SUP/SUB/RAD/BARS/PAREN/BIGOP but cannot for HBOX, so an
  // unchecked append there measures and paints as a finished formula
  // with an operand absent. LOGXY is the shape: "log2" with no
  // argument, reported true.
  //
  // The fixture starves the pool by one node, measured at runtime,
  // and asserts both halves: with the measured node count the build
  // succeeds, and one node short it must fail.
  {
    ppcTestReset();
    ppcTestType("8");
    ppcTestOp(ITM_ENTER);
    ppcTestType("2");
    ppcTestOp(ITM_LOGXY);

    uint8_t root = PP_NONE;
    ppReset();
    if(!ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
      ppTestFail("FV18 fixture never built at all — LOGXY was not captured");
    }
    else {
      uint8_t used = 0;
      while(ppNodeAt(used) != NULL) {
        used++;
      }
      if(used < 2 || used > PP_POOL_NODES) {
        ppTestFailInt("FV18 implausible node count", PP_POOL_NODES, used);
      }
      else {
        // one node short: the last allocation in ppfBuildOp2 is ppfParen
        ppReset();
        for(uint8_t i = 0; i < (uint8_t)(PP_POOL_NODES - (used - 1)); i++) {
          if(ppNewBox(PP_HBOX, PP_FONT_STANDARD) == PP_NONE) {
            ppTestFail("FV18 could not starve the pool");
            break;
          }
        }
        uint8_t starved = PP_NONE;
        if(ppfBuildCurrent(PP_FONT_STANDARD, PP_FONT_STANDARD, &starved)) {
          ppTestFail("FV18 the builder reported success one node short — a formula is missing an operand");
        }
      }
    }
    ppReset();
  }

  // FV19: the browser's softkey containment is a range, and this pins
  // our calcMode inside it. Upstream's three softkey gates
  // (btnFnPressed, btnFnReleased, executeFunction) enumerate its own
  // browsers by name. A package browser is covered only by the shared
  // `calcMode < 19 /* package browsers 19-23, claims registry */`
  // clause both sibling packages carry byte-identically. Renumber
  // CM_PRETTY_BROWSER below 19 and softkeys silently run underneath
  // the browser again. Our own PCLR then wipes the browsed history.
  // Nothing else goes red.
  if(CM_PRETTY_BROWSER < 19 || CM_PRETTY_BROWSER > 23) {
    ppTestFailInt("FV19 calcMode is outside the package-browser range the softkey gates protect",
                  20, (int32_t)CM_PRETTY_BROWSER);
  }

  // FV15: the softmenu claims are actually wired. The package's own
  // menu resolves with its six entries, and both parent slots hold
  // what the claims registry says they hold.
  {
    const int16_t *ppItems = NULL;
    int16_t ppCount = 0;
    for(int16_t m = 0; softmenu[m].menuItem != 0; m++) {
      if(softmenu[m].menuItem == -MNU_PP) {
        ppItems = softmenu[m].softkeyItem;
        ppCount = softmenu[m].numItems;
        break;
      }
    }
    if(ppItems == NULL) {
      ppTestFail("FV15 MNU_PP is not registered in the softmenu table");
    }
    else {
      // 12, not 6: VISUAL sits on the f-shifted row (a softmenu's slots
      // 6-11 ARE the f row), and upstream menus are padded to a multiple
      // of six
      static const int16_t want[12] = { ITM_PSHOW,  ITM_PHIST, ITM_PCLR,
                                        ITM_EQSHW,  ITM_PPON,  ITM_PTLIN,
                                        ITM_VISUAL, ITM_NULL,  ITM_NULL,
                                        ITM_NULL,   ITM_NULL,  ITM_NULL };
      if(ppCount != 12) {
        ppTestFailInt("FV15 MNU_PP size", 12, ppCount);
      }
      else {
        for(int i = 0; i < 12; i++) {
          if(ppItems[i] != want[i]) {
            ppTestFailInt("FV15 MNU_PP slot", want[i], ppItems[i]);
          }
        }
      }
    }
    // the two parent slots
    bool_t inDisp = false, inEqn = false;
    for(int16_t m = 0; softmenu[m].menuItem != 0; m++) {
      const int16_t *it = softmenu[m].softkeyItem;
      if(it == NULL) {
        continue;
      }
      for(int16_t k = 0; k < softmenu[m].numItems; k++) {
        if(softmenu[m].menuItem == -MNU_DISP && it[k] == -MNU_PP)  inDisp = true;
        if(softmenu[m].menuItem == -MNU_EQN  && it[k] == ITM_EQSHW) inEqn = true;
      }
    }
    if(!inDisp) {
      ppTestFail("FV15 MNU_PP is not in the DISP menu");
    }
    if(!inEqn) {
      ppTestFail("FV15 EQSHW is not in the EQN menu");
    }
  }

  // FV20: both full-screen surfaces hold their pixels with the
  // fnPixel protocol. Upstream's EXIT arm (keyboard.c:2533) dismisses
  // a held screen only when
  //   temporaryInformation != TI_NO_INFO || showScreenDismissed
  // showScreenDismissed is latched from SHOWMODE at btnPressed
  // (keyboard.c:1808).
  //
  // The EXIT arm lives in keyboard.c's static executeFunction,
  // reachable only from GTK button events the testSuite binary has no
  // path to raise. This pin asserts the state the surface must leave
  // behind for that arm to fire.
  {
    ppcTestReset();
    calcMode = CM_NORMAL;
    bool_t hadFract = getSystemFlag(FLAG_FRACT);
    setSystemFlag(FLAG_FRACT);
    prettySetEnabled(true);
    ppTestSetRealX("0.75");     // a LonI never pretties: it falls
                                // back to SHOW and proves nothing

    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    fnPrettyShow(NOPARAM);
    if(!screenHoldsDrawnPixels) {
      ppTestFail("FV20 PSHOW did not reach the held-pixel surface");
    }
    else if(temporaryInformation == TI_NO_INFO) {
      ppTestFail("FV20 PSHOW leaves no temporaryInformation — EXIT cannot dismiss it");
    }
    else if(!SHOWMODE) {
      ppTestFail("FV20 PSHOW screen is not a SHOWMODE screen — no showScreenDismissed latch");
    }

    // setEquation dereferences allFormulae, NULL until a slot
    // exists. This is the covDerivEq idiom.
    uint16_t hadMode = calcMode;
    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    setEquation(currentFormula, "1/(X+2)");
    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    fnPrettyEqShow(NOPARAM);
    if(!screenHoldsDrawnPixels) {
      ppTestFail("FV20 EQSHW did not reach the held-pixel surface");
    }
    else if(temporaryInformation == TI_NO_INFO) {
      ppTestFail("FV20 EQSHW leaves no temporaryInformation — EXIT cannot dismiss it");
    }
    else if(!SHOWMODE) {
      ppTestFail("FV20 EQSHW screen is not a SHOWMODE screen — no showScreenDismissed latch");
    }

    temporaryInformation = TI_NO_INFO;
    screenHoldsDrawnPixels = false;
    screenUpdatingMode = SCRUPD_AUTO;
    calcMode = hadMode;
    if(!hadFract) {
      clearSystemFlag(FLAG_FRACT);
    }
    lastErrorCode = ERROR_NONE;
  }

  ppcTestReset();
  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}


/* ==== prettyTestEqLang ==================================================
 * The package-invented equation language: SUM/PROD/DERIV/INTEG
 * evaluation through the real fnEqCalc path, and the two loop-closing
 * walker pins (V18, V65) that evaluate the walker's own output. The
 * program fixtures load in the core package's prettyTestVisual, which
 * runs earlier in the suite; this driver must run after it and before
 * serialize_cov resets the calculator (the list line's anchor). */

void prettyTestEqLang(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;
  ppTestFailures = 0;
  uint8_t root = PP_NONE;   // layout root for the render-agreement pins
  (void)root;

  /* ==== Equation-language big operators ================================ */
  {
    uint16_t hadStatus = currentSolverStatus;
    uint16_t hadVar = currentSolverVariable;
    uint16_t hadProgram = currentSolverProgram;

    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);          // the covDerivEq idiom, leaves EIM state
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;

    calcRegister_t varX = findOrAllocateNamedVariable("X");

    // EQ14: SUM evaluates through the real fnEqCalc path, and the bound
    // variable is put back the way it was found
    reallocateRegister(varX, dtReal34, 0, amNone);
    int32ToReal34(99, REGISTER_REAL34_DATA(varX));
    setEquation(currentFormula, "SUM(X^2;X;1;10)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(385, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ14 SUM != 385");
      }
      int32ToReal34(99, &want);
      if(getRegisterDataType(varX) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(varX), &want)) {
        ppTestFail("EQ14 bound variable not restored");
      }
    }

    // EQ15: PROD seeds with one
    setEquation(currentFormula, "PROD(X;X;1;5)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(120, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ15 PROD != 120");
      }
    }

    // EQ16: DERIV delegates to the upstream engine, exact for a cubic
    // (deriv_cov's own pin), both orders
    setEquation(currentFormula, "DERIV(X^3;X;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(12, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ16 DERIV != 12");
      }
    }
    setEquation(currentFormula, "DERIV(X^3;X;3;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(18, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ16 DERIV order 2 != 18");
      }
    }

    // EQ17: INTEG delegates to the double-exponential integrator
    setEquation(currentFormula, "INTEG(X^2;X;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t three, one, diff, tol;
      int32ToReal34(3, &three);
      int32ToReal34(1, &one);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ17 INTEG errored");
      }
      else {
        real34Multiply(REGISTER_REAL34_DATA(REGISTER_X), &three, &diff);
        real34Subtract(&diff, &one, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-6", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ17 INTEG != 1/3");
        }
      }
    }

    // EQ18: constructs nest. The argument slicer must honor paren depth
    setEquation(currentFormula, "SUM(SUM(Y;Y;1;X);X;1;3)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want;
      int32ToReal34(10, &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
        ppTestFail("EQ18 nested SUM != 10");
      }
    }

    // EQ19: a wrong argument count raises the equation's own error
    setEquation(currentFormula, "SUM(X;X;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    if(lastErrorCode == ERROR_NONE) {
      ppTestFail("EQ19 malformed SUM accepted");
    }
    lastErrorCode = 0;

    // EQ20/EQ21: the constructs render as their 2D shapes (strict parse)
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("SUM(X" "\xa1\x62" ";X;1;10)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ20 parse");
      }
      else {
        // HBOX sig children are space-joined
        ppfTestExpect("EQ20 sum shape", root, "B([x \xa1\x62]|[x = 1]|10)");
      }
      ppReset();
      if(!ppqParse("DERIV(X;X;2)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ21 parse");
      }
      else {
        ppfTestExpect("EQ21 deriv shape", root, "[F(d|[d x]) U(P(x)|[x = 2])]");
      }
    }

    // EQ22 (capacity): the ultimate nesting, an integral wrapping a
    // second derivative of [a Σ-with-√-fraction over a multiplication,
    // times a ∏ with a nested power fraction]. Pins the raised
    // PP_MAX_DEPTH and the 64-node pool: it must parse, measure, use
    // most of the pool, and fit the EQSHW band at full size.
    //
    // The fixture is the string a user can actually type, and EQ33
    // evaluates this same expression. The root is a function alias
    // that needs parentheses (functionAlias[], beside log10), and the
    // power operator is '^'. Indices are lowercase and distinct from
    // the outer variable. The constructs shadow correctly (same-
    // variable and distinct-variable forms agree to 34 digits), and
    // distinct indices let a reader see that.
    {
      uint8_t root;
      ppReset();
      static const char ultimate[] =
        "INTEG(DERIV("
          "SUM(" "\xa2\x1a" "(n)/(n+1);n;1;10)/(x" "\x80\xd7" "(x+1))"
          "\x80\xd7" "PROD(1+1/(2+m^2);m;1;5)"
        ";x;2;2);x;0;1)";
      ppReset();
      if(!ppqParse(ultimate, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ22 ultimate parse");
      }
      else if(!ppMeasure(root, 0)) {
        ppTestFail("EQ22 ultimate measure");
      }
      else {
        const ppNode_t *n = ppNodeAt(root);
        int16_t h = (int16_t)(n->ascent + n->descent);
        if(h < 80 || h > 147) {
          ppTestFailInt("EQ22 height out of band", 147, h);
        }
        if(n->width > 396) {
          ppTestFailInt("EQ22 too wide", 396, n->width);
        }
        uint8_t used = 0;
        while(used < PP_POOL_NODES && ppNodeAt(used) != NULL) {
          used++;
        }
        if(used < 45) {
          ppTestFailInt("EQ22 not the big tree", 45, used);
        }
      }
    }

    // EQ23-EQ25: the stored-alphabet arms EQSHW reads. The display
    // string truncates long equations for the strip.
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("X^2/(X+1)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ23 stored-form parse");
      }
      else {
        ppfTestExpect("EQ23 caret exponent", root, "F(S(x|2)|[x + 1])");
      }
      ppReset();
      if(!ppqParse("F:1/X", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ24 labeled parse");
      }
      else {
        ppfTestExpect("EQ24 label skipped", root, "F(1|x)");
      }
      ppReset();
      if(!ppqParse("SUM(1+X;X;1;5)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ25 parse");
      }
      else {
        // an additive body scopes in parens or the operator misreads
        ppfTestExpect("EQ25 additive body scoped", root, "B(P([1 + x])|[x = 1]|5)");
      }
    }

    // EQ29: the whole user journey for a construct, through the real
    // key path. Types SUM(X;X;1;3) one softkey at a time into the
    // equation editor (the ';' lives in the ALPHA punctuation menu).
    // The commit runs setEquation plus the MVAR variable-hunting
    // parse, then evaluates. 1+2+3 = 6.
    {
      uint16_t hadMode = calcMode;
      bool_t hadKIC = fnKeyInCatalog;
      fnEqNew(NOPARAM);                      // -> CM_EIM on a fresh slot
      aimBuffer[0] = 0;
      xCursor = 0;
      showSoftmenu(-MNU_ALPHAMISC);          // where ';' lives
      fnKeyInCatalog = true;                 // as a softkey press sets it
      static const int16_t keys[] = {
        ITM_S, ITM_U, ITM_M, ITM_LEFT_PARENTHESIS,
        ITM_X, ITM_SEMICOLON, ITM_X, ITM_SEMICOLON,
        ITM_1, ITM_SEMICOLON, ITM_3, ITM_RIGHT_PARENTHESIS
      };
      for(uint8_t k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        addItemToBuffer(keys[k]);
      }
      if(strcmp(aimBuffer, "SUM(X;X;1;3)") != 0) {
        ppTestFailures++;
        printf("prettyPrint test FAIL: EQ29 typed text (got '%s')\n", aimBuffer);
      }
      // the commit ENTER performs (keyboard.c CM_EIM case)
      lastErrorCode = 0;
      setEquation(currentFormula, aimBuffer);
      parseEquation(currentFormula, EQUATION_PARSER_MVAR, aimBuffer, tmpString);
      if(lastErrorCode != ERROR_NONE) {
        ppTestFailInt("EQ29 commit rejected the typed equation", 0, (int32_t)lastErrorCode);
        lastErrorCode = 0;
      }
      fnKeyInCatalog = hadKIC;
      calcMode = CM_NORMAL;
      aimBuffer[0] = 0;
      nimNumberPart = NP_EMPTY;
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      {
        real34_t want;
        int32ToReal34(6, &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
            || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ29 typed equation != 6");
        }
      }
      calcMode = hadMode;
      lastErrorCode = 0;
    }

    // EQ30: a construct whose body is complex accumulates complex, on
    // upstream's own terms. Its _programmableSumProd latches over the
    // moment a term has an imaginary part, but only if FL_CPXRES
    // allows. SUM(X*i;X;1;3) = (1+2+3)i = 6i.
    {
      bool_t hadCpx = getSystemFlag(FLAG_CPXRES);
      setSystemFlag(FLAG_CPXRES);
      setEquation(currentFormula, "SUM(X" STD_CROSS "i;X;1;3)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtComplex34) {
        ppTestFailInt("EQ30 complex SUM did not stay complex", dtComplex34,
                      (int32_t)getRegisterDataType(REGISTER_X));
      }
      else {
        real34_t want;
        int32ToReal34(6, &want);
        if(!real34IsZero(REGISTER_REAL34_DATA(REGISTER_X))
            || !real34CompareEqual(REGISTER_IMAG34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ30 complex SUM != 6i");
        }
      }
      lastErrorCode = 0;
      // and with complex results refused, the same body is a domain
      // error
      clearSystemFlag(FLAG_CPXRES);
      setEquation(currentFormula, "SUM(X" STD_CROSS "i;X;1;3)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(lastErrorCode == ERROR_NONE) {
        ppTestFail("EQ30 complex term accepted while CPXRES is clear");
      }
      lastErrorCode = 0;
      if(hadCpx) {
        setSystemFlag(FLAG_CPXRES);
      }
      else {
        clearSystemFlag(FLAG_CPXRES);
      }
    }

    // EQ31: a construct must never destroy the value the loop
    // variable already held. Class test: every type
    // saveRegisterSnapshot covers must survive a construct.
    {
      calcRegister_t vA = findOrAllocateNamedVariable("A");
      // complex value
      reallocateRegister(vA, dtComplex34, 0, amNone);
      int32ToReal34(7, REGISTER_REAL34_DATA(vA));
      int32ToReal34(4, REGISTER_IMAG34_DATA(vA));
      setEquation(currentFormula, "SUM(A;A;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      // Prove the sum ran: 1+2 = 3.
      {
        real34_t want3;
        int32ToReal34(3, &want3);
        if(lastErrorCode != ERROR_NONE) {
          ppTestFailInt("EQ31 the sum refused instead of running", 0, (int32_t)lastErrorCode);
        }
        else if(getRegisterDataType(REGISTER_X) != dtReal34
                 || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want3)) {
          ppTestFail("EQ31 the sum did not produce 3, so the restore is untested");
        }
      }
      lastErrorCode = 0;
      if(getRegisterDataType(vA) != dtComplex34) {
        ppTestFailInt("EQ31 complex loop variable destroyed", dtComplex34,
                      (int32_t)getRegisterDataType(vA));
      }
      else {
        real34_t w7, w4;
        int32ToReal34(7, &w7);
        int32ToReal34(4, &w4);
        if(!real34CompareEqual(REGISTER_REAL34_DATA(vA), &w7)
            || !real34CompareEqual(REGISTER_IMAG34_DATA(vA), &w4)) {
          ppTestFail("EQ31 complex loop variable not restored to 7+4i");
        }
      }
      // long integer: this branch must still work
      reallocateRegister(vA, dtLongInteger, 0, amNone);
      ppTestWriteLonI(vA, 12345);
      setEquation(currentFormula, "SUM(A;A;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      lastErrorCode = 0;
      if(!ppTestIsLonI(vA, 12345)) {
        ppTestFail("EQ31 long-integer loop variable not restored");
      }
      reallocateRegister(vA, dtReal34, 0, amNone);
      int32ToReal34(0, REGISTER_REAL34_DATA(vA));
    }

    // EQ32: the refusal verdict must be about this call. Nothing on
    // the derivative path clears engineNestingWasRefused (solve.c
    // holds the tree's only `= false`). Same shape for a pre-existing
    // PGM_WAITING.
    {
      engineNestingWasRefused = true;          // as an earlier refusal leaves it
      setEquation(currentFormula, "DERIV(X^3;X;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      real34_t want12;
      int32ToReal34(12, &want12);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
          || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want12)) {
        ppTestFail("EQ32 a stale refusal flag discarded a correct derivative");
      }
      engineNestingWasRefused = false;
      lastErrorCode = 0;

      // The caller's run state is the caller's. A pre-existing
      // PGM_WAITING means an abort is genuinely in flight (the engine
      // itself raises SOLVER_ABORT there, differentiate.c:402), so the
      // construct failing is correct. The state must not be silently
      // rewritten to PGM_STOPPED on the way out.
      programRunStop = PGM_WAITING;            // simulates a pre-existing WAITING
      setEquation(currentFormula, "DERIV(X^3;X;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      if(programRunStop != PGM_WAITING) {
        ppTestFailInt("EQ32 the caller's run state was overwritten",
                      PGM_WAITING, (int32_t)programRunStop);
      }
      programRunStop = PGM_STOPPED;
      lastErrorCode = 0;
    }

    // EQ26: the render/eval parity ruling, an integral of a numeric
    // second derivative whose body holds a construct, over limits away
    // from zero. A limit at zero collapses the relative-step stencil
    // at the integrator's endpoint-clustered nodes. Upstream's
    // interactive d²/dx² at 1E-24 fails the same way (documented
    // caveat).
    // g = SUM(X;X;1;3)/(X+2) = 6/(x+2). Integral of g'' over [1,2]
    // = g'(2)-g'(1) = 7/24.
    setEquation(currentFormula, "INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;1;2)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("0.2916666666666666666666666666666667", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ26 tower eval errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-10", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ26 tower != 7/24");
        }
      }
    }
    lastErrorCode = 0;

    // EQ33: the capacity expression EQ22 renders must also evaluate.
    // Hand-checked factor by factor: the sum is 3.759490707, the
    // product 1.857588228, and the second derivative of 1/(x(x+1)) at
    // x=2 is 0.175925926. The integral of that constant over [0,1]
    // leaves it unchanged.
    setEquation(currentFormula,
      "INTEG(DERIV("
        "SUM(" "\xa2\x1a" "(n)/(n+1);n;1;10)/(x" "\x80\xd7" "(x+1))"
        "\x80\xd7" "PROD(1+1/(2+m^2);m;1;5)"
      ";x;2;2);x;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("1.228593777031159439372254772764558", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ33 the capacity expression did not evaluate");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-30", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ33 capacity expression value moved");
        }
      }
    }
    lastErrorCode = 0;

    // EQ34: the construct names must accept lowercase. Upstream's own
    // convention for a name a user types into an equation is to carry
    // both spellings in the alias table. functionAlias[] holds "sinh"
    // beside "SINH" and "asinh" beside "ASINH" (equation.c:41-44),
    // because CMP_NAME folds superscripts and struck forms but not
    // case. Variables are not affected, so lowercase indices in the
    // capacity expression (EQ33) always work.
    {
      static const char * const eq34[] = {
        "sum(X;X;1;3)", "SUM(X;X;1;3)",
      };
      for(unsigned i = 0; i < sizeof(eq34) / sizeof(eq34[0]); i++) {
        setEquation(currentFormula, eq34[i]);
        lastErrorCode = 0;
        fnEqCalc(NOPARAM);
        real34_t want;
        int32ToReal34(6, &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34
            || !real34CompareEqual(REGISTER_REAL34_DATA(REGISTER_X), &want)) {
          ppTestFail("EQ34 a construct spelling did not evaluate to 6");
        }
        lastErrorCode = 0;
      }
      // and the same name mixed-case inside a nest, with a lowercase
      // index, so the whole intercept path is exercised
      setEquation(currentFormula, "integ(deriv(sum(X;X;1;3)/(X+2);X;X;2);X;1;2)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      {
        real34_t want, diff, tol;
        stringToReal34("0.2916666666666666666666666666666667", &want);
        if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
          ppTestFail("EQ34 the lowercase tower did not evaluate");
        }
        else {
          real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
          real34SetPositiveSign(&diff);
          stringToReal34("1e-10", &tol);
          if(!real34CompareLessThan(&diff, &tol)) {
            ppTestFail("EQ34 lowercase tower != 7/24");
          }
        }
      }
      lastErrorCode = 0;
      // the renderer must agree with the evaluator: same text, drawn
      ppReset();
      if(!ppqParse("sum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 lowercase construct does not render");
      }
      // a variable ENDING in a construct name still must not match
      ppReset();
      if(ppqParse("mysum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 a name ending in a construct name matched");
      }
      // MIXED case is upstream's own no: functionAlias[] carries SINH
      // and sinh, never Sinh.
      ppReset();
      if(ppqParse("Sum(X;X;1;3)", PP_FONT_STANDARD, PP_FONT_TINY, &root)) {
        ppTestFail("EQ34 mixed case matched, which upstream's aliases do not");
      }
    }

    /* EQ35: the EQN parser is a producer of the big-operator-as-operand
     * defect. SUM(X;X;1;3)^2 is 36. Drawn without brackets it is the
     * picture of 1^2+2^2+3^2 = 14. This parser has no precedence value
     * to correct, so the node kind decides. */
    {
      uint8_t root;
      ppReset();
      if(!ppqParse("SUM(X;X;1;3)^2", PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ35 a construct under a power did not parse");
      }
      else {
        ppfTestExpect("EQ35 the EQN parser brackets a construct operand",
                      root, "S(P(B(x|[x = 1]|3))|2)");
      }
      ppReset();
      if(!ppqParse("SUM(X;X;1;3)" STD_CROSS "2", PP_FONT_STANDARD,
                   PP_FONT_STANDARD, &root)) {
        ppTestFail("EQ35 a construct times two did not parse");
      }
      else {
        char w[96];
        sprintf(w, "[P(B(x|[x = 1]|3)) %s 2]", STD_DOT);
        ppfTestExpect("EQ35 the EQN parser brackets a product operand", root, w);
      }
    }

    // EQ27: an integral nests inside an integral. The
    // double-exponential path never increments the engine counter, so
    // nothing upstream refuses it. The package's stack guard is the
    // bound. Integral over y of (integral of x over [0,1]) = 0.5.
    setEquation(currentFormula, "INTEG(INTEG(X;X;0;1);X;0;1)");
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    {
      real34_t want, diff, tol;
      stringToReal34("0.5", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ27 nested integral errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-10", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ27 nested integral != 0.5");
        }
      }
    }
    lastErrorCode = 0;

    // EQ28: the upstream second-derivative defect near zero, and its
    // upstream-native remedy. The same integral over [0,1], a range
    // whose endpoint samples land in the failing band, is garbage
    // with the default relative step and exact once the derivative's
    // own step variable is set. Pins the remedy so the DESIGN.md
    // guidance cannot rot.
    {
      calcRegister_t vd = findOrAllocateNamedVariable(STD_delta STD_SUB_d);
      reallocateRegister(vd, dtReal34, 0, amNone);
      stringToReal34("0.001", REGISTER_REAL34_DATA(vd));
      setEquation(currentFormula, "INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;0;1)");
      lastErrorCode = 0;
      fnEqCalc(NOPARAM);
      real34_t want, diff, tol;
      stringToReal34("0.8333333333333333333333333333333333", &want);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        ppTestFail("EQ28 stepped derivative errored");
      }
      else {
        real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
        real34SetPositiveSign(&diff);
        stringToReal34("1e-25", &tol);
        if(!real34CompareLessThan(&diff, &tol)) {
          ppTestFail("EQ28 stepped derivative != 5/6");
        }
      }
      // zero disables it again: the engine ignores a zero step
      reallocateRegister(vd, dtReal34, 0, amNone);
      int32ToReal34(0, REGISTER_REAL34_DATA(vd));
      lastErrorCode = 0;
    }

    currentSolverStatus = hadStatus;
    currentSolverVariable = hadVar;
    currentSolverProgram = hadProgram;
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    temporaryInformation = TI_NO_INFO;
  }


  /* V65: the differential oracle. Run the program, then evaluate the
   * walker's own drawing, and require the two to agree. No expected
   * string appears here. */
  {
    static const char *const oracle[] = { "VD1", "VD2", "VD6", "VDBL", "VS1" };
    uint8_t savedMode = calcMode;
    for(unsigned k = 0; k < 5; k++) {
      calcRegister_t id = findNamedLabel(oracle[k], GLOBAL_LABELS);
      char produced[256], what[64];
      uint8_t reason = 0;
      uint16_t atStep = 0;
      real34_t viaProgram, viaPicture, diff, tol;
      sprintf(what, "V65 %s: the picture and the program", oracle[k]);
      if(id == INVALID_VARIABLE) {
        ppTestFail(what);
        continue;
      }
      calcMode = CM_NORMAL;
      programRunStop = PGM_STOPPED;
      dynamicMenuItem = -1;
      lastErrorCode = ERROR_NONE;
      fnExecute(id);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        lastErrorCode = ERROR_NONE;
        ppTestFail(what);
        continue;
      }
      real34Copy(REGISTER_REAL34_DATA(REGISTER_X), &viaProgram);
      if(!ppvTranspile((uint16_t)(id - FIRST_LABEL), produced, sizeof(produced),
                       &reason, &atStep)) {
        ppTestFailures++;
        printf("prettyPrint test FAIL: %s (the walker drew nothing: D%u)\n",
               what, (unsigned)reason);
        continue;
      }
      if(numberOfFormulae == 0) {
        fnEqNew(NOPARAM);
      }
      calcMode = CM_NORMAL;
      aimBuffer[0] = 0;
      nimNumberPart = NP_EMPTY;
      setEquation(currentFormula, produced);
      lastErrorCode = ERROR_NONE;
      fnEqCalc(NOPARAM);
      if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
        lastErrorCode = ERROR_NONE;
        ppTestFailures++;
        printf("prettyPrint test FAIL: %s (the drawing did not evaluate)\n", what);
        continue;
      }
      real34Copy(REGISTER_REAL34_DATA(REGISTER_X), &viaPicture);
      real34Subtract(&viaPicture, &viaProgram, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail(what);
      }
      lastErrorCode = ERROR_NONE;
    }
    calcMode = savedMode;
  }

  /* ---- the loop closed: the text computes -------------------------- */
  // V18: a picture that cannot be evaluated is a transpilation that
  // only looks right. The double integral is 4/3.
  {
    if(numberOfFormulae == 0) {
      fnEqNew(NOPARAM);
    }
    calcMode = CM_NORMAL;
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
    // The walker's own output. A hand-
    // written expectation and a hand-written pin agree with each
    // other whether or not either is right. 4/3 is a number nobody
    // in this file chose.
    {
      calcRegister_t id = findNamedLabel("VDBL", GLOBAL_LABELS);
      char produced[256];
      uint8_t reason = 0;
      uint16_t atStep = 0;
      if(id == INVALID_VARIABLE
          || !ppvTranspile((uint16_t)(id - FIRST_LABEL), produced, sizeof(produced),
                           &reason, &atStep)) {
        ppTestFail("V18 the walker produced nothing to evaluate");
        strcpy(produced, "0");
      }
      setEquation(currentFormula, produced);
    }
    lastErrorCode = 0;
    fnEqCalc(NOPARAM);
    if(lastErrorCode != ERROR_NONE || getRegisterDataType(REGISTER_X) != dtReal34) {
      ppTestFail("V18 transpiled double integral errored");
    }
    else {
      real34_t want, diff, tol;
      stringToReal34("1.33333333333333333333333333333333", &want);
      real34Subtract(REGISTER_REAL34_DATA(REGISTER_X), &want, &diff);
      real34SetPositiveSign(&diff);
      stringToReal34("1e-6", &tol);
      if(!real34CompareLessThan(&diff, &tol)) {
        ppTestFail("V18 transpiled double integral != 4/3");
      }
    }
    lastErrorCode = 0;
  }


  ppTestWriteLonI(REGISTER_X, ppTestFailures);
}

#endif // PC_BUILD
