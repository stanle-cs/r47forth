// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

#include "c47.h"
#include "forth_prims.h"

/* ---------- wrapper stubs (C47 handlers take uint16_t, primitives take void) ---------- */

static void pDup(void)   { fnDupN(1); }
static void pDrop(void)  { fnDrop(NOPARAM); }
static void pSwap(void)  { fnSwapXY(NOPARAM); }
static void pOver(void)  { fnRecall(REGISTER_Y); }
static void pPlus(void)  { fnAdd(NOPARAM); }
static void pMinus(void) { fnSubtract(NOPARAM); }
static void pMul(void)   { fnMultiply(NOPARAM); }
static void pDiv(void)   { fnDivide(NOPARAM); }

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
  PRIM_COUNT = 8
};

const forthPrimDef_t forthPrims[PRIM_COUNT] = {
  [PRIM_DUP]   = { "DUP",    0, pDup   },
  [PRIM_DROP]  = { "DROP",   0, pDrop  },
  [PRIM_SWAP]  = { "SWAP",   0, pSwap  },
  [PRIM_OVER]  = { "OVER",   0, pOver  },
  [PRIM_PLUS]  = { "+",      0, pPlus  },
  [PRIM_MINUS] = { "-",      0, pMinus },
  [PRIM_MUL]   = { "*",      0, pMul   },
  [PRIM_DIV]   = { "/",      0, pDiv   },
};

const uint16_t forthPrimCount = PRIM_COUNT;

_Static_assert(PRIM_COUNT <= 0x0FFF, "forthPrimCount exceeds FTOK_PRIM range");
