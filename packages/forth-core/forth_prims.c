// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

#include "c47.h"
#include "forth_prims.h"
#include "forth_dict.h"

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

/* F1-4: compile-only immediate. Emits a call to the definition under
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

/* F3-4: GLOBAL — move the latest closed definition to gdict */
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

/* F3-4: IMMEDIATE — set FF_IMMEDIATE on the latest closed definition */
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
  PRIM_COUNT     = 22
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
};

const uint16_t forthPrimCount = PRIM_COUNT;

_Static_assert(PRIM_COUNT <= 0x0FFF, "forthPrimCount exceeds FTOK_PRIM range");
