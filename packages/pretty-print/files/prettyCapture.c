// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyCapture.c
 * The shadow expression stack: turns chained interactive RPN operations
 * into expression trees, decides where a formula ends (DESIGN.md §4:
 * liveness + new-root supersession), and keeps a bounded ring of finished
 * formulas as postfix token streams.
 *
 * THE INVARIANT (binding, DESIGN.md §3): shadow slot k always holds an
 * expression whose value equals register REGISTER_X + k at quiescence.
 * When a transform cannot maintain that, the slot degrades to a value
 * leaf snapshotted from its register — truthful by construction — or the
 * whole shadow invalidates. The display never lies; over-invalidation
 * costs only history granularity.
 *
 * Two-phase mirroring: STAGE (prettyNoteFunction) runs before the item
 * dispatch and reads only pre-op registers; DONE (prettyNoteFunctionDone)
 * applies the staged transform only when the dispatch left no error.
 * Number entry commits at the closeNim funnel; the lift decision latched
 * at NIM open is applied there, so an aborted NIM (upstream undo())
 * needs no shadow rollback — nothing was applied.
 *
 * No display formatter ever runs here (the binding lazy rule): value
 * leaves store raw register payloads (undo-history's proven
 * dataType/tag/allocParam/payload recipe) and are formatted only at
 * display time by the viewer.
 */

#include "c47.h"
#include "prettyInternal.h"

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

/* AUDIT R1-3. The two hooks wrap ONE dispatch, but a dispatch can run a
 * user program whose every step comes back through them — so the hooks
 * nest. STAGE was made re-entrancy-safe in PP12; DONE never was, and it
 * consumed the stage on the FIRST nested step instead of on its own
 * dispatch. The end state usually coincided (which is why every test
 * passed), but the error check then ran at step 1 rather than at the
 * end, so a failure later in the program left the shadow claiming a
 * finished formula the register did not hold. This counter pairs each
 * DONE with its own STAGE. */
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

/* Initialising our own data and restoring the user's factory defaults
 * are DIFFERENT operations, and conflating them was a persistence bug
 * (found by PP15's FV14 while adding the second flag): ppcInit runs
 * lazily on the first dispatch after a cold start, and if it also set
 * the flags it would silently overwrite a preference the user had
 * saved. Only doFnReset restores defaults. */
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
}

void ppcTestDeinit(void) {
  ppcInited = false;   // the next dispatch takes the cold-start path
}

void prettyReset(void) {
  ppcInit();
  // Factory defaults. A RESET wipes the system flags before this hook
  // runs (the config.c call sits after Sett(_Reset)), so the T line's
  // default-OFF is already in place and is cleared here only so that
  // prettyReset means the same thing wherever it is called from; the
  // natural-display default-ON has to be re-established.
  setSystemFlag(FLAG_PRETTYP);
  clearSystemFlag(FLAG_PTLINE);
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
  // PPA_EMITTED lives in aux for OP nodes only — for LIT/VAL nodes aux is
  // a LENGTH and masking it would truncate the payload (caught by T4)
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

/* Value leaf from a live register — undo-history's generic capture recipe
 * (dataType / tag / allocParam / payload), viewer restores via
 * reallocateRegister + copy + setRegisterTag. Payloads over 16 bytes (or
 * matrix/complex-matrix types) become PPN_OPAQUE, which poisons any tree
 * that contains them into never-being-shown. */
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
  if(bytes > sizeof(((ppcNode_t *)0)->payload)) {
    return ppcAlloc(PPN_OPAQUE);
  }
  uint8_t n = ppcAlloc(PPN_VAL);
  if(n == PPC_NIL) {
    return PPC_NIL;
  }
  ppcArena[n].aux = (uint8_t)dt;
  ppcArena[n].pad[0] = (uint8_t)getRegisterTag(regist);
  ppcArena[n].pad[1] = (uint8_t)bytes;
  ppcArena[n].item = ppcAllocParamOf(regist);
  xcopy(ppcArena[n].payload, getRegisterDataPointer(regist), bytes);
  return n;
}

// RCL operand leaf: numbered registers keep their NAME (R05 stays R05
// even if the register is later overwritten — names are truthful);
// lettered/named registers become value leaves (their display names are
// not item ids); stack registers are handled by the caller via deepCopy.
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


/* ==== emission (DESIGN.md §4/§5) ======================================== */

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
      // gather continuation text
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
      if(off + 7 + bytes > cap) {
        return 0xffff;
      }
      out[off++] = PPT_TKV;
      out[off++] = nd->aux;                       // dataType
      out[off++] = nd->pad[0];                    // tag
      out[off++] = (uint8_t)(nd->item & 0xff);    // allocParam
      out[off++] = (uint8_t)(nd->item >> 8);
      out[off++] = bytes;
      xcopy(out + off, nd->payload, bytes);
      off = (uint16_t)(off + bytes);
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
 * snapshot (the register still holding the value — the invariant is the
 * proof); pass -1 when the value has already left the stack (a tree
 * pushed off the top), which stores the formula without a result. */
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
    if(dt != dtReal34Matrix && dt != dtComplex34Matrix && bytes <= 16 && off + 7 + bytes <= sizeof(buf)) {
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

/* Displacement (§4 rule 1): the tree in `slot` is about to leave the
 * shadow stack unconsumed. Emit if it is an unemitted op root; the
 * register that still holds its value supplies the result. */
static void ppcDisplaced(uint8_t slot, bool_t registerStillLive) {
  uint8_t n = ppcSlot[slot];
  if(n == PPC_NIL || n == PPC_UNKNOWN) {
    return;
  }
  ppcEmit(n, registerStillLive ? (calcRegister_t)(REGISTER_X + slot) : (calcRegister_t)-1);
}

static void ppcInvalidate(bool_t emitCurrent) {
  if(emitCurrent && ppcCurrent != PPC_NIL) {
    // AUDIT R1-5 (bug class: result snapshot taken on the wrong side of
    // the dispatch). Every OTHER emit-with-register site in this file
    // runs at STAGE, before the dispatch, where the register genuinely
    // still holds the formula's value. This one runs at DONE — the
    // PPC_INVALIDATE arm is deliberately deferred there because the
    // dispatch may error — so by now the register holds the NEW item's
    // output. Reading it filed lies permanently: `2 ENTER 3 . 7 +` then
    // IP recorded `2 + 3.7 = 5.` in the history, and the browser
    // recalled that 5.
    //
    // AUDIT R2-1 refined it further. Emitting with -1 is truthful but
    // records NO result, and the browser's ENTER can then never recall
    // the entry — truthful and useless. The classifier's INVALIDATE arm
    // now emits at STAGE instead, where the register genuinely still
    // holds this formula's value, which is where every other
    // emit-with-register in this file already happens. By the time we
    // get here the node is normally already marked EMITTED and ppcEmit
    // refuses it; -1 remains as the truthful last resort for any caller
    // that reaches invalidation without having staged.
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


/* ==== classifier (DESIGN.md §3) ========================================= */

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

    // PP12: the big-operator family — Σₙ/∏ₙ (and the integer variants)
    // consume Z=from, Y=to, X=step and leave the result in X; ∫YX
    // consumes Y=lower, X=upper over a label program.
    case ITM_SIGMAn: case ITM_PIn: case ITM_iSIGMAn: case ITM_iPIn:
      return PPC_BIGOPSUM;
    #if defined(OPTION_INFSUMS)
    // The early-stop sum consumes the SAME three stack levels — `inf`
    // only changes when the loop gives up, not what it reads — so it
    // captures identically. Its limits are the real ones the user
    // supplied (it stops early if the terms converge; "infinity" is the
    // key's name, not its arithmetic), so nothing new to render.
    // GUARDED: without the option, item 2755 is an unimplemented stub
    // that moves no stack, and classifying it would mint a node for an
    // operation that never happened.
    case ITM_SIGMAnINF:
      return PPC_BIGOPSUM;
    #endif // OPTION_INFSUMS
    case ITM_INTEGRAL_YX:
      return PPC_BIGOPINT;

    // stack mutators that are US_UNCHANGED and would otherwise be ignored
    case ITM_UNDO:
      return PPC_DISCARD;   // the user revoked the current formula

    // AUDIT R1-7. R/S resumes a stopped program, which then rewrites the
    // stack with every step out of scope — so nothing tells the shadow.
    // XEQ is the same operation reached by the other key and is
    // US_ENABLED, so the default rule already covers it; R/S is
    // US_UNCHANGED and was not.
    case ITM_RS:
      return PPC_INVALIDATE;
    case 427: case 428:
      // undo-history package composition claims (DESIGN.md §7): U.HIST
      // opens a browser whose restore bypasses item dispatch; REDO
      // rewrites the stack. Both invalidate.
      return PPC_INVALIDATE;

    default:
      break;
  }
  if(indexOfItems[func].func == fnConstant) {
    return PPC_CONSTCLS;
  }
  // The default rule (binding): unknown undo-enabled items moved the
  // stack in ways we did not model — invalidate; unknown non-undo items
  // are display/mode chatter — ignore. Upstream maintains US_STATUS for
  // its own undo correctness, so it maintains our predicate too.
  uint32_t us = indexOfItems[func].status & US_STATUS;
  // AUDIT R1-6. US_CANCEL belongs on the invalidate side, not the ignore
  // side: upstream's own header defines it as "the command cancels the
  // last UNDO data" — the machine moved beyond what undo can describe —
  // against US_UNCHANGED, "leaves the existing UNDO data as is". LOAD
  // and LOADST replace the whole register file under that status, and
  // the shadow went on describing the registers they overwrote.
  if(us == US_ENABLED || us == US_ENABL_XEQ || us == US_CANCEL) {
    return PPC_INVALIDATE;
  }
  return PPC_IGNORE;
}


/* ==== stack motion helpers ============================================== */

static void ppcShiftUpForLift(void) {
  uint8_t top = ppcTopSlot();
  // the top slot's tree is pushed off the stack; its value is gone from
  // the registers by the time we apply, so it emits without a result
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

// STAGE supersession helper: emit the current formula from the slot
// register still holding it, then close it
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
  // AUDIT R2-4. The counter used to saturate at 255 on the way up while
  // decrementing unconditionally on the way down, so a 256-deep nesting
  // would desynchronise it PERMANENTLY and every later DONE would pair
  // with the wrong STAGE. 256 levels of program-within-program is not
  // reachable on this machine, but the asymmetry is free to remove: a
  // wider counter, and a depth we cannot represent invalidates rather
  // than guesses.
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
    // A nested dispatch (a BIGOP's label program runs every step through
    // runFunction under FLAG_SOLVING/PGM_RUNNING) must not clobber the
    // outer stage. valid is only ever true strictly inside a dispatch,
    // so a top-level STAGE out of scope has nothing to clear.
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
      // supersession (§4 rule 2): a new root whose operands do not
      // include the current formula finishes it now, result from the
      // register still holding it
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
      // AUDIT R3-5. x<> to a STACK register swaps X with that slot's
      // register, so the PARTNER slot's tree stops describing it — the
      // same hole R1-8 closed for STO, at the hand exception next door.
      {
        uint16_t xt = param;
        if(xt > REGISTER_X && xt <= (uint16_t)getStackTop()) {
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
      // Z=from, Y=to, X=step consumed by VALUE (fnToReal + copy), never
      // by structure: an op tree in a consumed slot is displaced (§4
      // rule 1), and the current root is superseded regardless of where
      // it sits — the new BIGOP root never contains it.
      if(!(ppcStage.param >= FIRST_LABEL && ppcStage.param <= LAST_LABEL)) {
        // the register-letter form resolves a label indirectly — un-modeled
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
      // The integration only RUNS for a named-variable param over a
      // preselected label program (_fnIntegrate); a label/register param
      // is the interactive SETUP form — it still harvests X,Y into
      // ULIM/LLIM and drops them, but leaves no result to vouch for.
      // Formula targets belong to the EQN surface (PP13), not capture.
      if(!(ppcStage.param >= FIRST_NAMED_VARIABLE && ppcStage.param <= LAST_NAMED_VARIABLE)
          || (currentSolverStatus & SOLVER_STATUS_USES_FORMULA)
          || currentSolverProgram >= numberOfLabels) {
        ppcStage.cls = PPC_INVALIDATE;
        break;
      }
      // Y=lower, X=upper; payload[0..1] = the integration variable (for
      // the d<var> body text), the label comes from the solver target
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
      // applied at DONE (the dispatch may still error out)
      break;
    case PPC_INVALIDATE:
      // AUDIT R2-1. R1-5 stopped this from reading a POST-dispatch
      // register as the formula's result — correct, but emitting with
      // -1 records no result at all, and the browser's ENTER can then
      // never recall the entry: truthful and useless. The register is
      // still truthful HERE, before the dispatch, which is where every
      // other emit-with-register in this file already happens. Emit
      // now; DONE then tears the shadow down without emitting again
      // (ppcEmit refuses an already-EMITTED node, so a dispatch that
      // errors costs at most one early filing of a formula that was
      // true when it was filed).
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
  // only the dispatch that staged it may apply it (AUDIT R1-3)
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
      ppcSlotL = PPC_UNKNOWN;              // L holds old X; upgrade lazily
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
      // mirror fnKeyEnter: the CM_NIM branch always dups after commit;
      // the CM_NORMAL branch dups per the eRPN condition
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
      ppcSlot[0] = PPC_UNKNOWN;   // X now holds 0; upgrades truthfully on consume
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
      // AUDIT R1-8. "register-side only: the stack does not move" is true
      // about MOTION and irrelevant to the invariant, which is about
      // VALUES. `STO Y` overwrites register Y while the shadow goes on
      // showing Y's old tree, so `7 ENTER 2 ENTER 3 + STO Y x` displayed
      // 7·(2+3) = 25 for a product that is really 25 of something else.
      // The hand exception is what created the hole: ITM_STO is
      // undo-enabled, so without it the default rule would have been
      // safe. A stack target degrades that slot to UNKNOWN, which
      // re-materialises truthfully from the register on next use.
      // AUDIT R3-2, R3-3, R3-4 — three corrections to R1-8.
      //  * STO X writes X with what X already holds, so it changes no
      //    value and must not disturb the shadow at all.
      //  * The tree being dropped may be a FINISHED formula. Every other
      //    wipe site in this file displaces it (emits it with the
      //    register that still holds its value) before freeing; this one
      //    freed it, so the owner's formula vanished from the history
      //    instead of being filed.
      //  * REGISTER_L is a shadow slot too — ppcSlotL caches the LASTx
      //    tree — and STO L left it describing the overwritten value.
      uint16_t t = ppcStage.param;
      if(t > REGISTER_X && t <= (uint16_t)getStackTop()) {
        uint8_t k = (uint8_t)(t - REGISTER_X);
        ppcDisplaced(k, true);          // file it before it is lost
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
    case PPC_RCLCLS: {
      // copy-then-lift: a stack-register source is read BEFORE the shift
      uint8_t t;
      uint16_t param = ppcStage.param;
      if(param >= REGISTER_X && param <= getStackTop()) {
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
      if(param >= REGISTER_X && param <= getStackTop()) {
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
      // X now holds the register's OLD value — a fresh value leaf is the
      // only truthful description (naming the register would lie: it
      // holds the swapped-in tree value now)
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
      // The dispatch ran the label program between STAGE and DONE, and a
      // program can touch anything: X holds the result (the BIGOP's
      // value), every other register is somebody else's writing now.
      // Consumed formula slots were displaced at STAGE. UNKNOWN slots
      // re-materialize lazily as truthful VAL leaves.
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
  // latched BEFORE liftStack runs (hook placement in calcModeNim); the
  // shadow lift is deferred to the commit hook, so an aborted NIM
  // (upstream undo()) needs no shadow rollback
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
  // AUDIT R1-14. The gate was sizeof(ppcNimText) = 32, but the LEAF that
  // ultimately stores this text holds two 15-byte payloads = 30. Length
  // 31 was therefore the one value admitted and then silently truncated
  // — `aux` recorded 15, so nothing downstream could tell, and the
  // history copy was wrong too. Gate on what the leaf can hold, not on
  // the size of the staging buffer.
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
    // overwrite: a bare copy (post-ENTER) or an UNKNOWN (post-CLX) —
    // ops set ASLIFT, so an op tree is essentially never overwritten;
    // ppcDisplaced covers the residual case truthfully via no-emit
    // (register X already holds the new number at this point)
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

/* The raw current root, BEFORE the opaque screen below withholds it.
 * Tests only, and it earns its place: every public path into the shadow
 * goes through that screen, so a pin written against them cannot tell a
 * truthful degradation (tree built, one operand PPC_UNKNOWN, withheld
 * from display) from a total invalidation or from the operation never
 * having been classified at all. T21 asserted the second of those for
 * three rounds while believing it asserted the first. */
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
