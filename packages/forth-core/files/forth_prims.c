// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

#include "c47.h"
#include "forth_prims.h"
#include "forth_dict.h"
#include "forth_console.h"

/* ---------- wrapper stubs (C47 handlers take uint16_t, primitives take void) ---------- */

static void pDup(void)   { fnDupN(1); }
static void pDrop(void)  { fnDrop(NOPARAM); }
static void pSwap(void)  { fnSwapXY(NOPARAM); }
static void pOver(void)  { fnRecall(REGISTER_Y); }
static void pPlus(void)  { fnAdd(NOPARAM); }
static void pMinus(void) { fnSubtract(NOPARAM); }
static void pMul(void)   { fnMultiply(NOPARAM); }
static void pDiv(void)   { fnDivide(NOPARAM); }

#define FTOK_CALL_BASE 0x1000   /* mirror forth_compile.c / forth_inner.c */

/* Compile-only immediate. Emits a call to the definition under
 * construction; the smudged name itself stays invisible until ';'. */
static void pRecurse(void)
{
  uint16_t idx;
  if (!forthOpenDefinitionIndex(&idx)) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (!forthDictEmit((ftoken_t)(FTOK_CALL_BASE + idx))) {
     if (lastErrorCode == ERROR_NONE) {
       displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     }
   }
 }

/* GLOBAL — move the latest closed definition to gdict */
static void pGlobal(void)
{
  uint16_t ref = forthLatestClosedRefGet();
  if (ref == FORTH_NULL) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (ref & FORTH_REF_GLOBAL) {
    return;                       /* already global: idempotent no-op */
  }
  if (forthInnerIsActive()) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  {
    uint16_t gref;
    if (forthDictMakeLatestGlobal(ref, &gref)) {
      forthLatestClosedRefSet(gref);
    }
  }
}

/* IMMEDIATE — set FF_IMMEDIATE on the latest closed definition */
static void pImmediate(void)
{
  uint16_t ref = forthLatestClosedRefGet();
  if (ref == FORTH_NULL) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (!forthDictSetImmediateByRef(ref)) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  }
}

/* ---------- the console output words ----------
 *
 * §1.3 bans duplicating CALCULATOR operations reachable as CAT_FNCT items;
 * these are LANGUAGE surface instead, like IF, BEGIN, GLOBAL and RECURSE,
 * because there is no item-table equivalent of "print the top of stack".
 *
 * All seven write the ring wherever they run — interactive, a key press,
 * a program step — and none of them paints; a ring append is legal from
 * interpret, program-step and nested contexts alike (§3.3.2, nesting <= 2).
 *
 * `.$` is NOT the Forth-83 TYPE: TYPE is a landed CAT_FNCT/PTP_NONE item
 * (fnGetType) that already resolves from a Forth line, and a prim of that
 * name would silently change an existing meaning. */

/* `.` — X per the current display mode, then a separating space, then DROP.
 * The trailing space is what makes `1 . 2 . 3 .` read as "1 2 3 " rather
 * than "123". */
static void pPrint(void)
{
  char shown[FORTH_CONSOLE_FMT_MAX];
  forthConsoleFormatRegister(REGISTER_X, shown, (int16_t)sizeof(shown));
  forthConsoleAppend(shown);
  forthConsoleAppend(" ");
  fnDrop(NOPARAM);
}

/* `.S` — a one-line, depth-prefixed picture of the live stack, NON-
 * destructive.  The visible window is what displayStack says (4 or 8); the
 * D3 spill region can hold more, so the count is reported and the levels
 * are shown top-down from X.  Truncation is the view's job (the transcript
 * row ellipsis), not this word's — but the DEPTH is written first so the
 * part that must never be truncated away cannot be. */
static void pPrintStack(void)
{
  char shown[FORTH_CONSOLE_FMT_MAX];
  char head[32];
  /* The LIVE stack, not the display-line count: displayStack counts
   * DISPLAY LINES and every writer caps it at 4, so using it here would
   * silently drop the top four levels under SSIZE8.
   *
   * The Forth data stack on this machine IS the calculator stack — X up to
   * getStackTop(), which FLAG_SSIZE8 makes 4 or 8 — plus the D3 spill
   * region below it. Both terms are read live, from the same expressions
   * the engine's own capacity check uses, so this cannot fall out of step
   * with the stack. */
  uint16_t levels = (uint16_t)(getStackTop() - REGISTER_X + 1);
  uint16_t depth  = (uint16_t)(levels + forthSpillCount());
  uint16_t i;

  /* "depth first, then levels until the width runs out" — the depth
   * is the part that must never be truncated away. */
  snprintf(head, sizeof(head), "<%u> ", (unsigned)depth);
  forthConsoleAppend(head);
  for(i = 0; i < levels; i++) {
    /* REGISTER_X..REGISTER_T are consecutive; above T the visible window
     * continues into the spare registers the 8-level display already
     * shows. */
    forthConsoleFormatRegister((calcRegister_t)(REGISTER_X + i), shown, (int16_t)sizeof(shown));
    forthConsoleAppend(shown);
    if(i + 1 < levels) { forthConsoleAppend(" "); }
  }
  forthConsoleNewline();
}

static void pCr(void)    { forthConsoleNewline(); }
static void pSpace(void) { forthConsoleAppend(" "); }
static void pPage(void)  { forthConsoleClear(); }   /* the VIEW only — FHIST is
                                                       untouched; history surgery
                                                       is not a display act */

/* `EMIT` — X as a C47 glyph code, then DROP.
 *
 * One byte below 0x80, two bytes (high first) at 0x8000 and above — the
 * encoding the painter decodes. A bare 0x80..0xFF is a TRUNCATED glyph,
 * not a character, and is refused: writing it would put a lone high byte
 * in the ring for the painter to pair with whatever follows. */
static void pEmit(void)
{
  int32_t code = 0;
  char g[3];

  if(getRegisterDataType(REGISTER_X) == dtLongInteger) {
    longInteger_t li;
    bool_t inRange;
    longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    /* Test the magnitude BEFORE converting: longIntegerToInt32 is
     * mpz_get_si, which returns the low 32 bits of anything larger, so a
     * value that overflows int32 would silently pass the ASCII gate as a
     * wrong code instead of being refused. */
    inRange = !longIntegerIsNegative(li) && longIntegerCompareUInt(li, 0xFFFF) <= 0;
    if(inRange) {
      longIntegerToInt32(li, code);
    }
    longIntegerFree(li);
    if(!inRange) {
      displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
      return;
    }
  }
  else if(getRegisterDataType(REGISTER_X) == dtReal34) {
    code = real34ToInt32(REGISTER_REAL34_DATA(REGISTER_X));
  }
  else {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }

  if(code >= 0x20 && code <= 0x7E) {
    g[0] = (char)code; g[1] = 0;
  }
  else if(code >= 0x8000 && code <= 0xFFFF && (code & 0xFF) != 0) {
    /* The low byte must not be NUL: a two-byte glyph whose second byte is
     * 0x00 cannot exist as a C string — g would be ONE byte long, the ring
     * would store the lead byte alone, and forthConsoleLineAt would re-pair
     * it with whatever followed. Same refusal as the bare 0x80..0xFF case:
     * a truncated glyph is not a character. */
    g[0] = (char)((code >> 8) & 0xFF); g[1] = (char)(code & 0xFF); g[2] = 0;
  }
  else {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  forthConsoleAppend(g);
  fnDrop(NOPARAM);
}

/* `.$` — the string in X as text, then DROP.  Anything else is the standard
 * type error, unchanged. */
static void pPrintStr(void)
{
  if(getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  { char shown[FORTH_CONSOLE_FMT_MAX];
    forthConsoleFormatRegister(REGISTER_X, shown, (int16_t)sizeof(shown));
    forthConsoleAppend(shown);
  }
  fnDrop(NOPARAM);
}

/* ---------- primitive table (index-stable, append-only) ---------- */

enum {
  PRIM_DUP   = 0,
  PRIM_DROP  = 1,
  PRIM_SWAP  = 2,
  PRIM_OVER  = 3,
  PRIM_PLUS  = 4,
  PRIM_MINUS = 5,
  PRIM_MUL   = 6,
  PRIM_DIV   = 7,
  PRIM_CROSS = 8,
  PRIM_DOT   = 9,
  PRIM_DIVGL   = 10,
  PRIM_RECURSE = 11,
  PRIM_GLOBAL    = 12,
  PRIM_IMMEDIATE = 13,
  PRIM_IF        = 14,
  PRIM_ELSE      = 15,
  PRIM_THEN      = 16,
  PRIM_BEGIN     = 17,
  PRIM_UNTIL     = 18,
  PRIM_AGAIN     = 19,
  PRIM_WHILE     = 20,
  PRIM_REPEAT    = 21,
  /* Appended after PRIM_REPEAT; the identifiers avoid PRIM_DOT, which is
   * already the multiplication dot. */
  PRIM_PRINT     = 22,   /* .   */
  PRIM_PRINTS    = 23,   /* .S  */
  PRIM_CR        = 24,   /* CR  */
  PRIM_EMIT      = 25,   /* EMIT */
  PRIM_SPACE     = 26,   /* SPACE */
  PRIM_PRINTSTR  = 27,   /* .$  */
  PRIM_PAGE      = 28,   /* PAGE */
  PRIM_COUNT     = 29
};

const forthPrimDef_t forthPrims[PRIM_COUNT] = {
  [PRIM_DUP]   = { "DUP",    0, pDup   , +1 },
  [PRIM_DROP]  = { "DROP",   0, pDrop  , -1 },
  [PRIM_SWAP]  = { "SWAP",   0, pSwap  ,  0 },
  [PRIM_OVER]  = { "OVER",   0, pOver  , +1 },   /* fnRecall(Y): lifts and copies */
  [PRIM_PLUS]  = { "+",      0, pPlus  , -1 },
  [PRIM_MINUS] = { "-",      0, pMinus , -1 },
  [PRIM_MUL]   = { "*",      0, pMul   , -1 },
  [PRIM_DIV]   = { "/",      0, pDiv   , -1 },
  [PRIM_CROSS] = { STD_CROSS, 0, pMul  , -1 },
  [PRIM_DOT]   = { STD_DOT,   0, pMul  , -1 },
  [PRIM_DIVGL]   = { STD_DIVIDE, 0, pDiv, -1 },
  [PRIM_RECURSE] = { "RECURSE", FF_IMMEDIATE, pRecurse, 0 },
  [PRIM_GLOBAL]    = { "GLOBAL",    FF_DEFMARK, pGlobal,    0 },
  [PRIM_IMMEDIATE] = { "IMMEDIATE", FF_DEFMARK, pImmediate, 0 },
  [PRIM_IF]        = { "IF",     FF_IMMEDIATE, forthCtlIf    , 0 },
  [PRIM_ELSE]      = { "ELSE",   FF_IMMEDIATE, forthCtlElse  , 0 },
  [PRIM_THEN]      = { "THEN",   FF_IMMEDIATE, forthCtlThen  , 0 },
  [PRIM_BEGIN]     = { "BEGIN",  FF_IMMEDIATE, forthCtlBegin , 0 },
  [PRIM_UNTIL]     = { "UNTIL",  FF_IMMEDIATE, forthCtlUntil , 0 },
  [PRIM_AGAIN]     = { "AGAIN",  FF_IMMEDIATE, forthCtlAgain , 0 },
  [PRIM_WHILE]     = { "WHILE",  FF_IMMEDIATE, forthCtlWhile , 0 },
  [PRIM_REPEAT]    = { "REPEAT", FF_IMMEDIATE, forthCtlRepeat, 0 },
  /* plain prims, no FF_IMMEDIATE — they RUN, they do not compile. */
  [PRIM_PRINT]     = { ".",      0, pPrint,      -1 },
  [PRIM_PRINTS]    = { ".S",     0, pPrintStack,  0 },
  [PRIM_CR]        = { "CR",     0, pCr,          0 },
  [PRIM_EMIT]      = { "EMIT",   0, pEmit,       -1 },
  [PRIM_SPACE]     = { "SPACE",  0, pSpace,       0 },
  [PRIM_PRINTSTR]  = { ".$",     0, pPrintStr,   -1 },
  [PRIM_PAGE]      = { "PAGE",   0, pPage,        0 },
};

const uint16_t forthPrimCount = PRIM_COUNT;

_Static_assert(PRIM_COUNT <= 0x0FFF, "forthPrimCount exceeds FTOK_PRIM range");
