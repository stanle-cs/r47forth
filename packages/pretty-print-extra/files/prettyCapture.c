// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyCapture.c
 * The shadow expression stack. It turns chained RPN operations into
 * expression trees, decides where a formula ends, and keeps a bounded
 * ring of finished formulas as postfix token streams. The rules are in
 * DESIGN.md §1-§3.
 *
 * At quiescence, slot k always holds an expression whose value equals
 * register REGISTER_X + k. When a transform cannot keep this true, the
 * slot degrades to a plain value, or the whole shadow invalidates.
 *
 * STAGE (prettyNoteFunction) runs before the item dispatch and reads
 * only pre-op registers. DONE (prettyNoteFunctionDone) applies the
 * staged transform only when the dispatch left no error.
 *
 * No display formatter runs here. Value leaves store raw register
 * payloads. The viewer formats them at display time.
 */

#include "c47.h"
#include "prettyInternal.h"
#include "prettyExtraInternal.h"

extern bool_t nimWhenButtonPressed;   // keyboard.c file scope, non-static

/* ==== arena ============================================================= */

static ppcNode_t ppcArena[PPC_NODES];
static uint8_t  ppcFreeHead;
static uint8_t  ppcSlot[8];
static uint8_t  ppcSlotL;
static uint8_t  ppcCurrent;          // open formula root (a slot root) or PPC_NIL
static bool_t   ppcPendingLift;
static char     ppcNimText[32];
static bool_t   ppcNimTextValid;
static bool_t   ppcInited = false;

/* The hooks nest: a dispatch can run a program whose steps come back
 * through them. DONE must consume only the stage of its own dispatch. */
static uint16_t ppcDispatchDepth = 0;

static struct {
  int16_t  func;
  uint16_t param;
  uint8_t  cls;
  uint8_t  stagedMode;
  bool_t   lifted;      // ASLIFT latched at STAGE (LASTX/CONST classes)
  bool_t   valid;
  uint16_t depth;       // the dispatch nesting level this was staged at
  uint8_t  stagedFrom;  // BIGOP classes: pre-op limit VAL leaves
  uint8_t  stagedTo;
  uint8_t  stepBytes[16];   // BIGOP sums: the step real34, raw
} ppcStage;

static uint8_t  ppcHist[PPC_HIST_BYTES];
static uint16_t ppcHistOffset[PPC_HIST_MAX];
static uint16_t ppcHistUsed;
static uint8_t  ppcHistCount;
static uint16_t ppcHistSeq;

enum {
  PPC_DY, PPC_MO, PPC_ENTER, PPC_SWAP, PPC_RUP, PPC_RDOWN, PPC_CLX,
  PPC_DROP, PPC_CLSTK, PPC_FILL, PPC_LASTX, PPC_CONSTCLS, PPC_STO_NOP,
  PPC_RCLCLS, PPC_RCLARITH, PPC_XSWAPREG, PPC_DROPY,
  PPC_BIGOPSUM, PPC_BIGOPINT,
  PPC_INVALIDATE, PPC_DISCARD, PPC_IGNORE
};

static uint8_t ppcTopSlot(void) {
  return (uint8_t)(getStackTop() - REGISTER_X);
}

/* ppcTopSlot bounds stack iteration (lifts, drops, wipes). Do not use
 * it to test slot membership: upstream keeps A..D writable under
 * SSIZE4, so ppcIsSlotRegister must cover all eight slots. */
static bool_t ppcIsSlotRegister(uint16_t r) {
  return r >= (uint16_t)REGISTER_X
      && r < (uint16_t)REGISTER_X + (uint16_t)(sizeof(ppcSlot) / sizeof(ppcSlot[0]));
}

static void ppxReset(void);

/* Initializes package data only. It must not set the system flags:
 * only the core's prettyReset restores the factory defaults. It also
 * fills the core package's extension slots: every capture entry point
 * runs through here first, and no formula can exist before one does,
 * so the T-line extension is always registered before it can draw. */
static void ppcInit(void) {
  for(uint8_t i = 0; i < PPC_NODES; i++) {
    ppcArena[i].kind = PPN_FREE;
    ppcArena[i].child[0] = (uint8_t)(i + 1 < PPC_NODES ? i + 1 : PPC_NIL);
  }
  ppcFreeHead = 0;
  for(int i = 0; i < 8; i++) {
    ppcSlot[i] = PPC_UNKNOWN;
  }
  ppcSlotL = PPC_UNKNOWN;
  ppcCurrent = PPC_NIL;
  ppcPendingLift = false;
  ppcNimTextValid = false;
  ppcStage.valid = false;
  ppcHistUsed = 0;
  ppcHistCount = 0;
  ppcHistSeq = 0;
  ppcInited = true;
  ppTlineExtension = ppfTlineTry;
  ppResetExtension = ppxReset;
}

void ppcTestDeinit(void) {
  ppcInited = false;   // the next dispatch takes the cold-start path
}

/* The core's prettyReset calls this through ppResetExtension. The
 * flag defaults are the core's own job there. */
static void ppxReset(void) {
  ppcInit();
}

static uint8_t ppcAlloc(uint8_t kind) {
  if(ppcFreeHead == PPC_NIL) {
    return PPC_NIL;
  }
  uint8_t n = ppcFreeHead;
  ppcFreeHead = ppcArena[n].child[0];
  memset(&ppcArena[n], 0, sizeof(ppcNode_t));
  ppcArena[n].kind = kind;
  ppcArena[n].child[0] = PPC_NIL;
  ppcArena[n].child[1] = PPC_NIL;
  return n;
}

static void ppcFreeTree(uint8_t n) {
  if(n == PPC_NIL || n == PPC_UNKNOWN) {
    return;
  }
  if(ppcArena[n].kind == PPN_FREE) {
    return;   // double free guard
  }
  ppcFreeTree(ppcArena[n].child[0]);
  ppcFreeTree(ppcArena[n].child[1]);
  ppcArena[n].kind = PPN_FREE;
  ppcArena[n].child[0] = ppcFreeHead;
  ppcFreeHead = n;
  if(ppcCurrent == n) {
    ppcCurrent = PPC_NIL;
  }
}

static uint8_t ppcDeepCopy(uint8_t n) {
  if(n == PPC_NIL || n == PPC_UNKNOWN) {
    return n;
  }
  uint8_t c = ppcAlloc(ppcArena[n].kind);
  if(c == PPC_NIL) {
    return PPC_NIL;
  }
  uint8_t k0 = ppcDeepCopy(ppcArena[n].child[0]);
  uint8_t k1 = ppcDeepCopy(ppcArena[n].child[1]);
  if((ppcArena[n].child[0] != PPC_NIL && ppcArena[n].child[0] != PPC_UNKNOWN && k0 == PPC_NIL)
      || (ppcArena[n].child[1] != PPC_NIL && ppcArena[n].child[1] != PPC_UNKNOWN && k1 == PPC_NIL)) {
    ppcFreeTree(k0);
    ppcFreeTree(k1);
    ppcArena[c].kind = PPN_FREE;
    ppcArena[c].child[0] = ppcFreeHead;
    ppcFreeHead = c;
    return PPC_NIL;
  }
  uint8_t saved0 = k0, saved1 = k1;
  // PPA_EMITTED lives in aux for OP nodes only. For LIT/VAL nodes aux
  // is a length, and masking it truncates the payload.
  ppcArena[c].aux = ppcArena[n].aux;
  if(ppcArena[n].kind == PPN_OP1 || ppcArena[n].kind == PPN_OP2 || ppcArena[n].kind == PPN_BIGOP) {
    ppcArena[c].aux &= (uint8_t)~PPA_EMITTED;
  }
  ppcArena[c].item = ppcArena[n].item;
  ppcArena[c].pad[0] = ppcArena[n].pad[0];
  ppcArena[c].pad[1] = ppcArena[n].pad[1];
  xcopy(ppcArena[c].payload, ppcArena[n].payload, sizeof(ppcArena[c].payload));
  ppcArena[c].child[0] = saved0;
  ppcArena[c].child[1] = saved1;
  return c;
}

/* Value leaf from a live register (dataType / tag / allocParam /
 * payload). The viewer restores via reallocateRegister + copy +
 * setRegisterTag. Oversized payloads and matrix types become PPN_OPAQUE,
 * and a tree that contains an OPAQUE node is never shown. */
static uint16_t ppcAllocParamOf(calcRegister_t regist) {
  switch(getRegisterDataType(regist)) {
    case dtLongInteger: return REGISTER_LONG_INTEGER_HEADER(regist)->dataMaxLengthInBlocks;
    case dtString:      return REGISTER_STRING_HEADER(regist)->dataMaxLengthInBlocks;
    default:            return 0;
  }
}

static uint8_t ppcValLeafFromRegister(calcRegister_t regist) {
  uint32_t dt = getRegisterDataType(regist);
  if(dt == dtReal34Matrix || dt == dtComplex34Matrix) {
    return ppcAlloc(PPN_OPAQUE);
  }
  uint32_t bytes = TO_BYTES((uint32_t)getRegisterFullSizeInBlocks(regist));
  /* Only complex34 gets the two-node continuation (DESIGN.md §1): an
   * oversized long integer stays opaque. */
  const uint32_t cap = (dt == dtComplex34) ? (uint32_t)PPC_VAL_CAPACITY
                                           : (uint32_t)sizeof(((ppcNode_t *)0)->payload);
  if(bytes > cap) {
    return ppcAlloc(PPN_OPAQUE);
  }
  const uint8_t head = (uint8_t)sizeof(((ppcNode_t *)0)->payload);
  uint8_t n = ppcAlloc(PPN_VAL);
  if(n == PPC_NIL) {
    return PPC_NIL;
  }
  ppcArena[n].aux = (uint8_t)dt;
  ppcArena[n].pad[0] = (uint8_t)getRegisterTag(regist);
  ppcArena[n].pad[1] = (uint8_t)bytes;
  ppcArena[n].item = ppcAllocParamOf(regist);
  xcopy(ppcArena[n].payload, getRegisterDataPointer(regist),
        (bytes > head) ? head : bytes);
  /* A payload wider than one node continues into a PPN_VAL2 on
   * child[0]. If the second node cannot be had, the leaf degrades to
   * OPAQUE: a truncated payload is a wrong number. */
  if(bytes > head) {
    uint8_t cont = ppcAlloc(PPN_VAL2);
    if(cont == PPC_NIL) {
      ppcFreeTree(n);
      return ppcAlloc(PPN_OPAQUE);
    }
    uint8_t rest = (uint8_t)(bytes - head);
    ppcArena[cont].aux = rest;
    xcopy(ppcArena[cont].payload, getRegisterDataPointer(regist) + head, rest);
    ppcArena[n].child[0] = cont;
  }
  return n;
}

// RCL operand leaf. Numbered registers keep their name: R05 stays R05
// even if the register is later overwritten. Lettered and named
// registers become value leaves. The caller handles stack registers
// with a deep copy of the slot tree.
static uint8_t ppcRclLeaf(uint16_t param) {
  if(param <= 99) {
    uint8_t n = ppcAlloc(PPN_RCL);
    if(n != PPC_NIL) {
      ppcArena[n].item = param;
    }
    return n;
  }
  return ppcValLeafFromRegister((calcRegister_t)param);
}

static void ppcEnsureKnown(uint8_t slot) {
  if(ppcSlot[slot] == PPC_UNKNOWN) {
    uint8_t n = ppcValLeafFromRegister(REGISTER_X + slot);
    ppcSlot[slot] = (n == PPC_NIL) ? PPC_UNKNOWN : n;
  }
}


/* ==== emission (DESIGN.md §2/§3) ======================================== */

static bool_t ppcTreeHasOpaque(uint8_t n) {
  if(n == PPC_NIL || n == PPC_UNKNOWN) {
    return n == PPC_UNKNOWN;   // an UNKNOWN child means the tree cannot serialize truthfully
  }
  if(ppcArena[n].kind == PPN_OPAQUE) {
    return true;
  }
  return ppcTreeHasOpaque(ppcArena[n].child[0]) || ppcTreeHasOpaque(ppcArena[n].child[1]);
}

static uint16_t ppcSerializeNode(uint8_t n, uint8_t *out, uint16_t off, uint16_t cap, uint8_t *nTokens) {
  if(off == 0xffff || n == PPC_NIL || n == PPC_UNKNOWN) {
    return 0xffff;
  }
  const ppcNode_t *nd = &ppcArena[n];
  switch(nd->kind) {
    case PPN_OP2:
      off = ppcSerializeNode(nd->child[0], out, off, cap, nTokens);
      off = ppcSerializeNode(nd->child[1], out, off, cap, nTokens);
      if(off == 0xffff || off + 3 > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKO2;
      out[off++] = (uint8_t)(nd->item & 0xff);
      out[off++] = (uint8_t)(nd->item >> 8);
      (*nTokens)++;
      return off;
    case PPN_OP1:
      off = ppcSerializeNode(nd->child[0], out, off, cap, nTokens);
      if(off == 0xffff || off + 3 > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKO1;
      out[off++] = (uint8_t)(nd->item & 0xff);
      out[off++] = (uint8_t)(nd->item >> 8);
      (*nTokens)++;
      return off;
    case PPN_LIT: {
      char text[32];
      uint8_t len = nd->aux;
      if(len > 15) {
        len = 15;
      }
      xcopy(text, nd->payload, len);
      uint8_t total = len;
      uint8_t cont = nd->child[0];
      if(cont != PPC_NIL && cont != PPC_UNKNOWN && ppcArena[cont].kind == PPN_LIT2) {
        uint8_t clen = ppcArena[cont].aux;
        if(clen > 15) {
          clen = 15;
        }
        xcopy(text + total, ppcArena[cont].payload, clen);
        total = (uint8_t)(total + clen);
      }
      if(off + 2 + total > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKL;
      out[off++] = total;
      xcopy(out + off, text, total);
      off = (uint16_t)(off + total);
      (*nTokens)++;
      return off;
    }
    case PPN_VAL: {
      uint8_t bytes = nd->pad[1];
      const uint8_t head = (uint8_t)sizeof(nd->payload);
      /* the header below is six bytes: kind, dataType, tag,
       * allocParam(2), len */
      if(off + 6 + bytes > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKV;
      out[off++] = nd->aux;                       // dataType
      out[off++] = nd->pad[0];                    // tag
      out[off++] = (uint8_t)(nd->item & 0xff);    // allocParam
      out[off++] = (uint8_t)(nd->item >> 8);
      out[off++] = bytes;
      /* A split payload continues in the PPN_VAL2 on child[0]. The
       * stream stays one flat TKV, so the reader is unchanged. */
      uint8_t first = (bytes > head) ? head : bytes;
      xcopy(out + off, nd->payload, first);
      off = (uint16_t)(off + first);
      if(bytes > first) {
        uint8_t cont = nd->child[0];
        if(cont == PPC_NIL || cont == PPC_UNKNOWN
            || ppcArena[cont].kind != PPN_VAL2) {
          return 0xffff;   // a split value with no continuation is not filable
        }
        xcopy(out + off, ppcArena[cont].payload, (uint8_t)(bytes - first));
        off = (uint16_t)(off + (bytes - first));
      }
      (*nTokens)++;
      return off;
    }
    case PPN_CONST:
      if(off + 3 > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKC;
      out[off++] = (uint8_t)(nd->item & 0xff);
      out[off++] = (uint8_t)(nd->item >> 8);
      (*nTokens)++;
      return off;
    case PPN_RCL:
      if(off + 3 > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKR;
      out[off++] = (uint8_t)(nd->item & 0xff);
      out[off++] = (uint8_t)(nd->item >> 8);
      (*nTokens)++;
      return off;
    case PPN_BIGOP:
      // postfix: from-VAL, to-VAL, then the operator record
      off = ppcSerializeNode(nd->child[0], out, off, cap, nTokens);
      off = ppcSerializeNode(nd->child[1], out, off, cap, nTokens);
      if(off == 0xffff || off + 21 > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKBIG;
      out[off++] = (uint8_t)(nd->item & 0xff);
      out[off++] = (uint8_t)(nd->item >> 8);
      out[off++] = nd->pad[0];
      out[off++] = nd->pad[1];
      xcopy(out + off, nd->payload, 16);
      off = (uint16_t)(off + 16);
      (*nTokens)++;
      return off;
    default:
      return 0xffff;
  }
}

static void ppcHistEvictOldest(void) {
  if(ppcHistCount == 0) {
    return;
  }
  uint16_t first = ppcHistOffset[0];
  uint16_t firstLen;
  xcopy(&firstLen, ppcHist + first, 2);
  uint16_t tail = (uint16_t)(ppcHistUsed - (first + firstLen));
  memmove(ppcHist, ppcHist + first + firstLen, tail);
  ppcHistUsed = tail;
  ppcHistCount--;
  for(uint8_t i = 0; i < ppcHistCount; i++) {
    ppcHistOffset[i] = (uint16_t)(ppcHistOffset[i + 1] - firstLen);
  }
}

/* Emit a finished formula. resultReg >= 0 supplies the "= result"
 * snapshot and must still hold the formula's value. Pass -1 when the
 * value already left the stack: the formula is stored without a
 * result. */
static void ppcEmit(uint8_t root, calcRegister_t resultReg) {
  if(root == PPC_NIL || root == PPC_UNKNOWN) {
    return;
  }
  ppcNode_t *nd = &ppcArena[root];
  if(nd->kind != PPN_OP1 && nd->kind != PPN_OP2 && nd->kind != PPN_BIGOP) {
    return;   // a bare value is not a formula
  }
  if(nd->aux & PPA_EMITTED) {
    return;
  }
  if(ppcTreeHasOpaque(root)) {
    return;
  }

  uint8_t buf[PPC_HIST_BYTES / 2];
  uint8_t nTokens = 0;
  uint16_t off = ppcSerializeNode(root, buf, 6, sizeof(buf), &nTokens);
  if(off == 0xffff) {
    return;
  }
  // result snapshot
  if(resultReg >= 0) {
    uint32_t dt = getRegisterDataType(resultReg);
    uint32_t bytes = TO_BYTES((uint32_t)getRegisterFullSizeInBlocks(resultReg));
    if(dt != dtReal34Matrix && dt != dtComplex34Matrix
        && bytes <= PPC_VAL_CAPACITY && off + 6 + bytes <= sizeof(buf)) {
      buf[off++] = PPT_TKRES;
      buf[off++] = (uint8_t)dt;
      buf[off++] = (uint8_t)getRegisterTag(resultReg);
      uint16_t ap = ppcAllocParamOf(resultReg);
      buf[off++] = (uint8_t)(ap & 0xff);
      buf[off++] = (uint8_t)(ap >> 8);
      buf[off++] = (uint8_t)bytes;
      xcopy(buf + off, getRegisterDataPointer(resultReg), bytes);
      off = (uint16_t)(off + bytes);
      nTokens++;
    }
  }
  ppcHistSeq++;
  buf[0] = (uint8_t)(off & 0xff);        // totalBytes
  buf[1] = (uint8_t)(off >> 8);
  buf[2] = (uint8_t)(ppcHistSeq & 0xff);
  buf[3] = (uint8_t)(ppcHistSeq >> 8);
  buf[4] = nTokens;
  buf[5] = 0;

  while(ppcHistCount >= PPC_HIST_MAX || ppcHistUsed + off > PPC_HIST_BYTES) {
    ppcHistEvictOldest();
  }
  ppcHistOffset[ppcHistCount++] = ppcHistUsed;
  xcopy(ppcHist + ppcHistUsed, buf, off);
  ppcHistUsed = (uint16_t)(ppcHistUsed + off);
  nd->aux |= PPA_EMITTED;
}

/* The tree in `slot` is about to leave the shadow stack unconsumed
 * (DESIGN.md §2 rule 1). Emit it if it is an unemitted op root. */
static void ppcDisplaced(uint8_t slot, bool_t registerStillLive) {
  uint8_t n = ppcSlot[slot];
  if(n == PPC_NIL || n == PPC_UNKNOWN) {
    return;
  }
  ppcEmit(n, registerStillLive ? (calcRegister_t)(REGISTER_X + slot) : (calcRegister_t)-1);
}

static void ppcInvalidate(bool_t emitCurrent) {
  if(emitCurrent && ppcCurrent != PPC_NIL) {
    // No result snapshot: at DONE the register already holds the new
    // item's output. Callers that can emit do it at STAGE, and ppcEmit
    // then refuses the already-emitted node here.
    ppcEmit(ppcCurrent, (calcRegister_t)-1);
  }
  for(int i = 0; i < 8; i++) {
    ppcFreeTree(ppcSlot[i] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[i]);
    ppcSlot[i] = PPC_UNKNOWN;
  }
  ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
  ppcSlotL = PPC_UNKNOWN;
  ppcCurrent = PPC_NIL;
}


/* ==== classifier (DESIGN.md §1) ========================================= */

static uint8_t ppcClassify(int16_t func) {
  switch(func) {
    case ITM_ADD: case ITM_SUB: case ITM_MULT: case ITM_DIV:
    case ITM_YX: case ITM_XTHROOT: case ITM_LOGXY:
      return PPC_DY;

    case ITM_SQUAREROOTX: case ITM_SQUARE: case ITM_CUBE: case ITM_CUBEROOT:
    case ITM_1ONX: case ITM_CHS: case ITM_LN: case ITM_LOG10: case ITM_EXP:
    case ITM_10x: case ITM_XFACT: case ITM_MAGNITUDE: case ITM_ABS:
    case ITM_EX1: case ITM_LN1X:
    case ITM_sin: case ITM_cos: case ITM_tan:
    case ITM_arcsin: case ITM_arccos: case ITM_arctan:
      return PPC_MO;

    case ITM_ENTER:  return PPC_ENTER;
    case ITM_XexY:   return PPC_SWAP;
    case ITM_Rup:    return PPC_RUP;
    case ITM_Rdown:  return PPC_RDOWN;
    case ITM_CLX:    return PPC_CLX;
    case ITM_DROP:   return PPC_DROP;
    case ITM_CLSTK:  return PPC_CLSTK;
    case ITM_FILL:   return PPC_FILL;
    case ITM_LASTX:  return PPC_LASTX;
    case ITM_CONSTpi: return PPC_CONSTCLS;

    // register-side only: the stack does not move
    case ITM_STO: case ITM_STOADD: case ITM_STOSUB: case ITM_STOMULT: case ITM_STODIV:
      return PPC_STO_NOP;

    case ITM_RCL:   return PPC_RCLCLS;
    case ITM_RCLADD: case ITM_RCLSUB: case ITM_RCLMULT: case ITM_RCLDIV:
      return PPC_RCLARITH;
    case ITM_Xex:   return PPC_XSWAPREG;
    case ITM_DROPY: return PPC_DROPY;

    // Big-operator family: Σₙ/∏ₙ and the integer variants consume
    // Z=from, Y=to, X=step and leave the result in X. ∫YX consumes
    // Y=lower, X=upper over a label program.
    case ITM_SIGMAn: case ITM_PIn: case ITM_iSIGMAn: case ITM_iPIn:
      return PPC_BIGOPSUM;
    #if defined(OPTION_INFSUMS)
    // Captures the same as a plain sum. Guarded: without the option the
    // item is a stub that moves no stack.
    case ITM_SIGMAnINF:
      return PPC_BIGOPSUM;
    #endif // OPTION_INFSUMS
    case ITM_INTEGRAL_YX:
      return PPC_BIGOPINT;

    // US_UNCHANGED stack mutators the default rule alone ignores
    case ITM_UNDO:
      return PPC_DISCARD;   // the user revoked the current formula

    // Both run program steps whose stack motion happens out of scope,
    // so nothing tells the shadow. Both are US_UNCHANGED, so the
    // default rule alone ignores them.
    case ITM_RS: case ITM_SST:
      return PPC_INVALIDATE;
    case 427: case 428:
      // undo-history composition (DESIGN.md §6): the U.HIST restore
      // bypasses item dispatch, and REDO rewrites the stack. Both
      // invalidate.
      return PPC_INVALIDATE;

    default:
      break;
  }
  if(indexOfItems[func].func == fnConstant) {
    return PPC_CONSTCLS;
  }
  // Default rule: an unknown undo-enabled item moved the stack in a
  // way the shadow does not model, so invalidate. An unknown non-undo
  // item is display or mode chatter, so ignore.
  uint32_t us = indexOfItems[func].status & US_STATUS;
  // US_CANCEL also invalidates: LOAD and LOADST replace the whole
  // register file under that status.
  if(us == US_ENABLED || us == US_ENABL_XEQ || us == US_CANCEL) {
    return PPC_INVALIDATE;
  }
  return PPC_IGNORE;
}


/* ==== stack motion helpers ============================================== */

static void ppcShiftUpForLift(void) {
  uint8_t top = ppcTopSlot();
  // the top slot's tree is pushed off the stack, and its value is gone
  // from the registers, so it emits without a result
  ppcDisplaced(top, false);
  ppcFreeTree(ppcSlot[top] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[top]);
  for(uint8_t k = top; k > 0; k--) {
    ppcSlot[k] = ppcSlot[k - 1];
  }
  ppcSlot[0] = PPC_UNKNOWN;
}

static void ppcShiftDownAfterConsume(void) {
  // mirrors upstream: Y..top move down, top keeps (duplicates) its value
  uint8_t top = ppcTopSlot();
  for(uint8_t k = 0; k + 1 <= top; k++) {
    ppcSlot[k] = ppcSlot[k + 1];
  }
  if(top >= 1) {
    uint8_t d = ppcDeepCopy(ppcSlot[top]);
    ppcSlot[top] = (d == PPC_NIL) ? PPC_UNKNOWN : d;
  }
}

static void ppcCurrentRevalidate(void) {
  if(ppcCurrent == PPC_NIL) {
    return;
  }
  for(uint8_t k = 0; k <= ppcTopSlot(); k++) {
    if(ppcSlot[k] == ppcCurrent) {
      return;
    }
  }
  ppcCurrent = PPC_NIL;
}


/* ==== hooks ============================================================= */

static bool_t ppcScopeOk(void) {
  return (programRunStop != PGM_RUNNING)
      && !getSystemFlag(FLAG_SOLVING)
      && !getSystemFlag(FLAG_INTING)
      && (calcMode == CM_NORMAL || calcMode == CM_NIM);
}

// Emit the current formula from the slot register that still holds it,
// then close it.
static void ppcSupersedeCurrent(void) {
  for(uint8_t k = 0; k <= ppcTopSlot(); k++) {
    if(ppcSlot[k] == ppcCurrent) {
      ppcEmit(ppcCurrent, (calcRegister_t)(REGISTER_X + k));
      break;
    }
  }
  ppcCurrent = PPC_NIL;
}

void prettyNoteFunction(int16_t func, uint16_t param) {
  // A depth the counter cannot represent invalidates the shadow.
  if(ppcDispatchDepth < 0xFFFF) {
    ppcDispatchDepth++;
  }
  else {
    ppcInvalidate(false);
    ppcStage.valid = false;
    return;
  }
  if(!ppcInited) {
    ppcInit();
  }
  if(!ppcScopeOk()) {
    // A nested dispatch must not clobber the outer stage. valid is only
    // true strictly inside a dispatch, so a top-level STAGE out of
    // scope has nothing to clear.
    return;
  }
  ppcStage.valid = false;
  uint8_t cls = ppcClassify(func);
  if(cls == PPC_IGNORE) {
    return;
  }
  ppcStage.func = func;
  ppcStage.param = param;
  ppcStage.cls = cls;
  ppcStage.stagedMode = calcMode;
  ppcStage.depth = ppcDispatchDepth;
  ppcStage.lifted = getSystemFlag(FLAG_ASLIFT);
  ppcStage.valid = true;

  // STAGE-side work: everything that must read PRE-op registers
  switch(cls) {
    case PPC_DY:
      ppcEnsureKnown(0);
      ppcEnsureKnown(1);
      // a new root whose operands do not include the current formula
      // finishes it now (DESIGN.md §2 rule 2)
      if(ppcCurrent != PPC_NIL && ppcSlot[0] != ppcCurrent && ppcSlot[1] != ppcCurrent) {
        ppcSupersedeCurrent();
      }
      break;
    case PPC_MO:
      ppcEnsureKnown(0);
      if(ppcCurrent != PPC_NIL && ppcSlot[0] != ppcCurrent) {
        ppcSupersedeCurrent();
      }
      break;
    case PPC_ENTER:
      ppcEnsureKnown(0);
      break;
    case PPC_CLX:
    case PPC_DROP:
      ppcDisplaced(0, true);
      break;
    case PPC_CLSTK:
      for(uint8_t k = 0; k <= ppcTopSlot(); k++) {
        ppcDisplaced(k, true);
      }
      break;
    /* FILL overwrites Y..top with X, which is displacement, so those
     * slots must be filed here while the registers are still pre-op.
     * Slot 0 is the source and keeps its value. */
    case PPC_FILL:
      for(uint8_t k = 1; k <= ppcTopSlot(); k++) {
        ppcDisplaced(k, true);
      }
      break;
    /* Must run at STAGE: ppcEmit requires that resultReg still holds
     * the formula's value, and by DONE fnStore has overwritten it. */
    case PPC_STO_NOP: {
      uint16_t t = param;
      if(t > (uint16_t)REGISTER_X && ppcIsSlotRegister(t)) {
        uint8_t k = (uint8_t)(t - REGISTER_X);
        ppcDisplaced(k, true);          // file it while its register still holds it
        ppcFreeTree(ppcSlot[k] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[k]);
        ppcSlot[k] = PPC_UNKNOWN;
        ppcCurrentRevalidate();
      }
      else if(t == REGISTER_L) {
        ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
        ppcSlotL = PPC_UNKNOWN;
      }
      break;
    }
    case PPC_LASTX:
      if(ppcSlotL == PPC_UNKNOWN) {
        uint8_t n = ppcValLeafFromRegister(REGISTER_L);
        ppcSlotL = (n == PPC_NIL) ? PPC_UNKNOWN : n;
      }
      break;
    case PPC_RCLARITH:
      ppcEnsureKnown(0);
      if(ppcCurrent != PPC_NIL && ppcSlot[0] != ppcCurrent) {
        ppcSupersedeCurrent();
      }
      break;
    case PPC_XSWAPREG:
      // the tree's value still sits in register X at STAGE
      ppcDisplaced(0, true);
      // x<> to a stack register swaps X with that slot's register, so
      // the partner slot's tree stops describing it.
      {
        uint16_t xt = param;
        if(xt > (uint16_t)REGISTER_X && ppcIsSlotRegister(xt)) {
          uint8_t k = (uint8_t)(xt - REGISTER_X);
          ppcDisplaced(k, true);
          ppcFreeTree(ppcSlot[k] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[k]);
          ppcSlot[k] = PPC_UNKNOWN;
        }
        else if(xt == REGISTER_L) {
          ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
          ppcSlotL = PPC_UNKNOWN;
        }
      }
      break;
    case PPC_BIGOPSUM: {
      // Z=from, Y=to, X=step are consumed by value. Consumed slots and
      // the current root are emitted here (DESIGN.md §2).
      if(!(ppcStage.param >= FIRST_LABEL && ppcStage.param <= LAST_LABEL)) {
        // the register-letter form resolves a label indirectly: not modeled
        ppcStage.cls = PPC_INVALIDATE;
        break;
      }
      if(getRegisterDataType(REGISTER_X) == dtReal34) {
        xcopy(ppcStage.stepBytes, REGISTER_REAL34_DATA(REGISTER_X), 16);
      }
      else if(getRegisterDataType(REGISTER_X) == dtLongInteger) {
        real34_t s;
        convertLongIntegerRegisterToReal34(REGISTER_X, &s);
        xcopy(ppcStage.stepBytes, &s, 16);
      }
      else {
        // a step we cannot show truthfully: fall back to the default rule
        ppcStage.cls = PPC_INVALIDATE;
        break;
      }
      ppcDisplaced(0, true);
      ppcDisplaced(1, true);
      ppcDisplaced(2, true);
      if(ppcCurrent != PPC_NIL) {
        ppcSupersedeCurrent();
      }
      ppcStage.stagedFrom = ppcValLeafFromRegister(REGISTER_Z);
      ppcStage.stagedTo   = ppcValLeafFromRegister(REGISTER_Y);
      break;
    }
    case PPC_BIGOPINT: {
      // The integration runs only for a named-variable param over a
      // preselected label program. A label/register param is the
      // interactive setup form: it harvests X,Y into ULIM/LLIM and
      // leaves no result. Formula targets belong to the EQN surface.
      if(!(ppcStage.param >= FIRST_NAMED_VARIABLE && ppcStage.param <= LAST_NAMED_VARIABLE)
          || (currentSolverStatus & SOLVER_STATUS_USES_FORMULA)
          || currentSolverProgram >= numberOfLabels) {
        ppcStage.cls = PPC_INVALIDATE;
        break;
      }
      // Y=lower, X=upper. payload[0..1] = the integration variable (for
      // the d<var> body text). The label comes from the solver target.
      memset(ppcStage.stepBytes, 0, 16);
      ppcStage.stepBytes[0] = (uint8_t)(ppcStage.param & 0xff);
      ppcStage.stepBytes[1] = (uint8_t)(ppcStage.param >> 8);
      ppcStage.param = (uint16_t)(currentSolverProgram + FIRST_LABEL);
      ppcDisplaced(0, true);
      ppcDisplaced(1, true);
      if(ppcCurrent != PPC_NIL) {
        ppcSupersedeCurrent();
      }
      ppcStage.stagedFrom = ppcValLeafFromRegister(REGISTER_Y);
      ppcStage.stagedTo   = ppcValLeafFromRegister(REGISTER_X);
      break;
    }
    case PPC_DROPY:
      ppcDisplaced(1, true);
      break;
    case PPC_DISCARD:
      // applied at DONE (the dispatch can still fail)
      break;
    case PPC_INVALIDATE:
      // Emit now, while the register still holds this formula's value.
      // At DONE the shadow tears down without a second emit.
      if(ppcCurrent != PPC_NIL) {
        ppcSupersedeCurrent();
      }
      break;
    default:
      break;
  }
}

void prettyNoteFunctionDone(void) {
  uint16_t myDepth = ppcDispatchDepth;
  if(ppcDispatchDepth > 0) {
    ppcDispatchDepth--;
  }
  // only the dispatch that staged it may apply it
  if(!ppcStage.valid || ppcStage.depth != myDepth) {
    return;
  }
  ppcStage.valid = false;
  if(lastErrorCode != ERROR_NONE) {
    if(ppcStage.cls == PPC_BIGOPSUM || ppcStage.cls == PPC_BIGOPINT) {
      ppcFreeTree(ppcStage.stagedFrom);
      ppcFreeTree(ppcStage.stagedTo);
    }
    // a failed function may have partially moved the stack
    ppcInvalidate(false);
    return;
  }

  switch(ppcStage.cls) {
    case PPC_DY: {
      uint8_t n = ppcAlloc(PPN_OP2);
      if(n == PPC_NIL) {
        ppcInvalidate(false);
        break;
      }
      ppcArena[n].item = (uint16_t)ppcStage.func;
      ppcArena[n].child[0] = ppcSlot[1];   // left = Y
      ppcArena[n].child[1] = ppcSlot[0];   // right = X
      ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
      ppcSlotL = PPC_UNKNOWN;              // L holds old X and upgrades lazily
      ppcSlot[1] = n;                      // temporary: n sits where Y was
      ppcSlot[0] = PPC_UNKNOWN;
      ppcShiftDownAfterConsume();          // moves n into slot 0
      ppcCurrent = n;
      break;
    }
    case PPC_MO: {
      uint8_t n = ppcAlloc(PPN_OP1);
      if(n == PPC_NIL) {
        ppcInvalidate(false);
        break;
      }
      ppcArena[n].item = (uint16_t)ppcStage.func;
      ppcArena[n].child[0] = ppcSlot[0];
      ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
      ppcSlotL = PPC_UNKNOWN;
      ppcSlot[0] = n;
      ppcCurrent = n;
      break;
    }
    case PPC_ENTER: {
      // mirror fnKeyEnter: the CM_NIM branch always dups after commit,
      // and the CM_NORMAL branch dups per the eRPN condition
      bool_t dup;
      if(ppcStage.stagedMode == CM_NIM) {
        dup = (calcMode != CM_NIM);   // only if closeNim committed
      }
      else {
        dup = (!getSystemFlag(FLAG_ERPN)
               || (!nimWhenButtonPressed && programRunStop != PGM_RUNNING)
               || (getSystemFlag(FLAG_ERPN) && programRunStop == PGM_RUNNING));
      }
      if(dup) {
        ppcShiftUpForLift();
        uint8_t d = ppcDeepCopy(ppcSlot[1]);
        ppcSlot[0] = (d == PPC_NIL) ? PPC_UNKNOWN : d;
      }
      break;
    }
    case PPC_SWAP: {
      uint8_t t = ppcSlot[0];
      ppcSlot[0] = ppcSlot[1];
      ppcSlot[1] = t;
      break;
    }
    case PPC_RDOWN: {
      uint8_t top = ppcTopSlot();
      uint8_t t = ppcSlot[0];
      for(uint8_t k = 0; k < top; k++) {
        ppcSlot[k] = ppcSlot[k + 1];
      }
      ppcSlot[top] = t;
      break;
    }
    case PPC_RUP: {
      uint8_t top = ppcTopSlot();
      uint8_t t = ppcSlot[top];
      for(uint8_t k = top; k > 0; k--) {
        ppcSlot[k] = ppcSlot[k - 1];
      }
      ppcSlot[0] = t;
      break;
    }
    case PPC_CLX:
      ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
      ppcSlot[0] = PPC_UNKNOWN;   // X now holds 0 and upgrades truthfully on consume
      break;
    case PPC_DROP:
      ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
      ppcSlot[0] = PPC_UNKNOWN;
      ppcShiftDownAfterConsume();
      break;
    case PPC_CLSTK:
      ppcInvalidate(false);
      break;
    case PPC_FILL: {
      for(uint8_t k = 1; k <= ppcTopSlot(); k++) {
        ppcFreeTree(ppcSlot[k] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[k]);
        uint8_t d = ppcDeepCopy(ppcSlot[0]);
        ppcSlot[k] = (d == PPC_NIL) ? PPC_UNKNOWN : d;
      }
      break;
    }
    case PPC_LASTX: {
      if(ppcStage.lifted) {
        ppcShiftUpForLift();
      }
      else {
        ppcDisplaced(0, false);   // overwritten; its register is already gone
        ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
        ppcSlot[0] = PPC_UNKNOWN;
      }
      uint8_t d = ppcDeepCopy(ppcSlotL);
      ppcSlot[0] = (d == PPC_NIL) ? PPC_UNKNOWN : d;
      break;
    }
    case PPC_CONSTCLS: {
      if(ppcStage.lifted) {
        ppcShiftUpForLift();
      }
      else {
        ppcDisplaced(0, false);
        ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
        ppcSlot[0] = PPC_UNKNOWN;
      }
      uint8_t n = ppcAlloc(PPN_CONST);
      if(n != PPC_NIL) {
        ppcArena[n].item = (uint16_t)ppcStage.func;
      }
      ppcSlot[0] = (n == PPC_NIL) ? PPC_UNKNOWN : n;
      break;
    }
    case PPC_STO_NOP: {
      // Handled entirely at STAGE, where the target register still
      // holds the formula's value.
      break;
    }
    case PPC_RCLCLS: {
      // copy-then-lift: a stack-register source is read BEFORE the shift
      uint8_t t;
      uint16_t param = ppcStage.param;
      if(ppcIsSlotRegister(param)) {
        t = ppcDeepCopy(ppcSlot[param - REGISTER_X]);
      }
      else {
        t = ppcRclLeaf(param);
      }
      if(ppcStage.lifted) {
        ppcShiftUpForLift();
      }
      else {
        ppcDisplaced(0, false);
        ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
        ppcSlot[0] = PPC_UNKNOWN;
      }
      ppcSlot[0] = (t == PPC_NIL) ? PPC_UNKNOWN : t;
      break;
    }
    case PPC_RCLARITH: {
      uint16_t mapped =
          ppcStage.func == ITM_RCLADD  ? ITM_ADD  :
          ppcStage.func == ITM_RCLSUB  ? ITM_SUB  :
          ppcStage.func == ITM_RCLMULT ? ITM_MULT : ITM_DIV;
      uint16_t param = ppcStage.param;
      uint8_t r;
      if(ppcIsSlotRegister(param)) {
        r = ppcDeepCopy(ppcSlot[param - REGISTER_X]);
      }
      else {
        r = ppcRclLeaf(param);
      }
      uint8_t n = ppcAlloc(PPN_OP2);
      if(n == PPC_NIL || r == PPC_NIL) {
        ppcFreeTree(r == PPC_UNKNOWN ? PPC_NIL : r);
        ppcInvalidate(false);
        break;
      }
      ppcArena[n].item = mapped;
      ppcArena[n].child[0] = ppcSlot[0];   // left = old X
      ppcArena[n].child[1] = r;            // right = the register operand
      ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
      ppcSlotL = PPC_UNKNOWN;
      ppcSlot[0] = n;
      ppcCurrent = n;
      break;
    }
    case PPC_XSWAPREG:
      // X now holds the register's old value: describe it with a fresh
      // value leaf.
      ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
      {
        uint8_t v = ppcValLeafFromRegister(REGISTER_X);
        ppcSlot[0] = (v == PPC_NIL) ? PPC_UNKNOWN : v;
      }
      break;
    case PPC_DROPY: {
      uint8_t top = ppcTopSlot();
      ppcFreeTree(ppcSlot[1] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[1]);
      for(uint8_t k = 1; k + 1 <= top; k++) {
        ppcSlot[k] = ppcSlot[k + 1];
      }
      if(top >= 2) {
        uint8_t d = ppcDeepCopy(ppcSlot[top]);
        ppcSlot[top] = (d == PPC_NIL) ? PPC_UNKNOWN : d;
      }
      break;
    }
    case PPC_BIGOPSUM:
    case PPC_BIGOPINT: {
      uint8_t n = ppcAlloc(PPN_BIGOP);
      if(n == PPC_NIL || ppcStage.stagedFrom == PPC_NIL || ppcStage.stagedTo == PPC_NIL) {
        ppcFreeTree(ppcStage.stagedFrom);
        ppcFreeTree(ppcStage.stagedTo);
        if(n != PPC_NIL) {
          ppcFreeTree(n);
        }
        ppcInvalidate(false);
        break;
      }
      ppcArena[n].item = (uint16_t)ppcStage.func;
      ppcArena[n].pad[0] = (uint8_t)(ppcStage.param & 0xff);
      ppcArena[n].pad[1] = (uint8_t)(ppcStage.param >> 8);
      xcopy(ppcArena[n].payload, ppcStage.stepBytes, 16);
      ppcArena[n].child[0] = ppcStage.stagedFrom;
      ppcArena[n].child[1] = ppcStage.stagedTo;
      // The label program ran between STAGE and DONE and can touch
      // anything, so all slots reset to UNKNOWN. X gets the new BIGOP
      // tree.
      for(int i = 0; i < 8; i++) {
        ppcFreeTree(ppcSlot[i] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[i]);
        ppcSlot[i] = PPC_UNKNOWN;
      }
      ppcFreeTree(ppcSlotL == PPC_UNKNOWN ? PPC_NIL : ppcSlotL);
      ppcSlotL = PPC_UNKNOWN;
      ppcSlot[0] = n;
      ppcCurrent = n;
      break;
    }
    case PPC_INVALIDATE:
      ppcInvalidate(true);
      break;
    case PPC_DISCARD:
      ppcInvalidate(false);   // UNDO: the user revoked the current formula
      break;
    default:
      break;
  }
  ppcCurrentRevalidate();
}

void prettyNoteNimOpen(void) {
  if(!ppcInited) {
    ppcInit();
  }
  if(programRunStop == PGM_RUNNING || getSystemFlag(FLAG_SOLVING) || getSystemFlag(FLAG_INTING)) {
    ppcNimTextValid = false;
    return;
  }
  // Latched before liftStack runs (hook placement in calcModeNim). The
  // shadow lift waits for the commit hook, so an aborted NIM needs no
  // rollback.
  ppcPendingLift = getSystemFlag(FLAG_ASLIFT);
  ppcNimTextValid = false;
}

void prettyNoteNimText(const char *aim) {
  if(!ppcInited) {
    ppcInit();
  }
  const char *s = aim;
  if(*s == '+') {
    s++;
  }
  size_t n = strlen(s);
  // Gate on what the leaf can hold: two 15-byte payloads.
  if(n > PPC_LIT_CAPACITY) {
    ppcNimTextValid = false;   // too long: the leaf will fall back to a value
    ppcNimText[0] = 0;
    return;
  }
  xcopy(ppcNimText, s, n + 1);
  ppcNimTextValid = true;
}

void prettyNoteNumberCommit(void) {
  if(!ppcInited) {
    ppcInit();
  }
  if(calcMode == CM_NIM || lastErrorCode != 0) {
    return;   // not committed (still typing, or the close failed)
  }
  if(programRunStop == PGM_RUNNING || getSystemFlag(FLAG_SOLVING) || getSystemFlag(FLAG_INTING)) {
    return;
  }

  if(ppcPendingLift) {
    ppcShiftUpForLift();
  }
  else {
    // Overwrite: register X already holds the new number, so a
    // displaced op tree is stored without a result.
    ppcDisplaced(0, false);
    ppcFreeTree(ppcSlot[0] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[0]);
    ppcSlot[0] = PPC_UNKNOWN;
  }
  ppcPendingLift = false;

  uint8_t n = PPC_NIL;
  if(ppcNimTextValid) {
    size_t len = strlen(ppcNimText);
    n = ppcAlloc(PPN_LIT);
    if(n != PPC_NIL) {
      uint8_t first = (uint8_t)(len > 15 ? 15 : len);
      ppcArena[n].aux = first;
      xcopy(ppcArena[n].payload, ppcNimText, first);
      if(len > 15) {
        uint8_t cont = ppcAlloc(PPN_LIT2);
        if(cont == PPC_NIL) {
          ppcFreeTree(n);
          n = PPC_NIL;
        }
        else {
          uint8_t rest = (uint8_t)(len - 15 > 15 ? 15 : len - 15);
          ppcArena[cont].aux = rest;
          xcopy(ppcArena[cont].payload, ppcNimText + 15, rest);
          ppcArena[n].child[0] = cont;
        }
      }
    }
  }
  if(n == PPC_NIL) {
    n = ppcValLeafFromRegister(REGISTER_X);   // as-typed unavailable: truthful value
  }
  ppcSlot[0] = (n == PPC_NIL) ? PPC_UNKNOWN : n;
  ppcNimTextValid = false;
}


/* ==== viewer API (PP4) ================================================== */

const ppcNode_t *ppcNodeAt(uint8_t n) {
  return (n < PPC_NODES) ? &ppcArena[n] : NULL;
}

/* Raw shadow state, before the OPAQUE filter below withholds it.
 * Tests only. */
uint8_t ppcTestSlotRaw(uint8_t k) {
  return (k < sizeof(ppcSlot) / sizeof(ppcSlot[0])) ? ppcSlot[k] : PPC_NIL;
}

uint8_t ppcTestCurrentRaw(void) {
  return ppcInited ? ppcCurrent : PPC_NIL;
}

uint8_t ppcCurrentFormulaRoot(void) {
  if(!ppcInited || ppcCurrent == PPC_NIL) {
    return PPC_NIL;
  }
  if(ppcTreeHasOpaque(ppcCurrent)) {
    return PPC_NIL;
  }
  return ppcCurrent;
}

uint8_t ppcHistoryCount(void) {
  return ppcInited ? ppcHistCount : 0;
}

const uint8_t *ppcHistoryEntry(uint8_t idx, uint16_t *lenOut, uint16_t *seqOut) {
  if(!ppcInited || idx >= ppcHistCount) {
    return NULL;
  }
  // newest first
  uint8_t phys = (uint8_t)(ppcHistCount - 1 - idx);
  const uint8_t *e = ppcHist + ppcHistOffset[phys];
  uint16_t len, seq;
  xcopy(&len, e, 2);
  xcopy(&seq, e + 2, 2);
  if(lenOut) {
    *lenOut = len;
  }
  if(seqOut) {
    *seqOut = seq;
  }
  return e;
}

void ppcHistoryClear(void) {
  ppcHistUsed = 0;
  ppcHistCount = 0;
}

// For mutations that bypass item dispatch (the browser's recall-to-X):
// wipe the shadow to UNKNOWN without touching the history ring.
void ppcShadowInvalidate(void) {
  if(!ppcInited) {
    ppcInit();
    return;
  }
  ppcInvalidate(false);
}
