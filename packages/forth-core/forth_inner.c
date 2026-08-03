/*
 * forth_inner.c -- Forth inner interpreter (fetch-decode-dispatch loop)
 * Per DESIGN.md §2 (token table) and §3.2 (inner interpreter)
 * FTOK_C47: §2.2 decoder (PGM_RUNNING wrap lands in H1)
 */

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"
#include "programming/param_core.h"

/* ---- §2.2 Token constants ---- */

#define FTOK_PRIM_BASE    0x0001
#define FTOK_CALL_BASE    0x1000
#define FTOK_LIT          0x7F00
#define FTOK_ILIT         0x7F01
#define FTOK_BR           0x7F02
#define FTOK_0BR          0x7F03
#define FTOK_C47          0x7F04

/* ---- §3.2 Return stack & guards ---- */

#define FORTH_RSTACK_DEPTH  64
#define RUNAWAY_CAP         4096

static uint16_t rstack[FORTH_RSTACK_DEPTH];
static uint8_t  rsp;
static uint64_t rstackRegionBits;   /* bit i = region of rstack[i]'s ip (1 = gdict) */
static uint8_t forthDepth = 0;   /* nested forthInner invocations */

/* Active-frame predicate (§9.3, F1-1) */
bool forthInnerIsActive(void) {
  return forthDepth != 0;
}

#if defined(PC_BUILD)
void forthSetTestInnerDepth(uint8_t depth) {
  forthDepth = depth;
}
#endif

/* ---- Push helpers (stack discipline per §3.2) ---- */

/* ---- D2: data-stack overflow guard ----
 * The Forth data stack IS the C47 RPN stack (4 or 8 levels, FLAG_SSIZE8), so a
 * push past the top silently discards the bottom-most entry. For a user keying
 * values that is ordinary RPN behaviour, but a recursive word overrunning its
 * OWN operands is silent corruption: 7 FACT used to return 4320 instead of
 * 5040, with lastErrorCode 0 throughout.
 *
 * forthDataDepth counts values Forth has pushed and not yet consumed since the
 * current line began. It is only ever <= the true depth, so the guard can fail
 * to fire but can never fire falsely -- a spurious "stack full" on a correct
 * program would be worse than the silence it replaces. */
static int16_t forthDataDepth   = 0;
static bool_t  forthOuterActive = false;

/* D3-1 spill region (DESIGN.md §11): arena-backed, per-execution, LIFO.
 * Unused by product paths until D3-2 wires forthDataDepthApply to it.
 * Slot: [uint32 dataType][uint16 sizeInBlocks][payload]. */
static void    *forthSpillBase   = NULL;   /* arena block, or NULL */
static uint16_t forthSpillBlocks = 0;      /* allocated size in blocks */
static uint32_t forthSpillTop    = 0;      /* byte offset one past last slot */
static uint16_t forthSpillSlots  = 0;      /* live slot count */

uint16_t forthSpillCount(void) { return forthSpillSlots; }

void forthSpillReset(void)
{
  if (forthSpillBase) {
    freeC47Blocks(forthSpillBase, forthSpillBlocks);
  }
  forthSpillBase = NULL; forthSpillBlocks = 0;
  forthSpillTop = 0; forthSpillSlots = 0;
}

bool_t forthSpillCatch(calcRegister_t reg)
{
  uint32_t type   = getRegisterDataType(reg);
  uint16_t blocks = getRegisterFullSizeInBlocks(reg);
  uint32_t need   = forthSpillTop + 6u + (uint32_t)blocks * 4u;
  if (forthSpillBase == NULL || need > (uint32_t)forthSpillBlocks * 4u) {
    uint16_t newBlocks = (uint16_t)((need + 63u) / 4u + 16u);
    void *nb = forthSpillBase
      ? reallocC47Blocks(forthSpillBase, forthSpillBlocks, newBlocks)
      : allocC47Blocks(newBlocks);
    if (nb == NULL) { return false; }          /* arena exhausted: caller errors */
    forthSpillBase = nb; forthSpillBlocks = newBlocks;
  }
  { uint8_t *p = (uint8_t *)forthSpillBase + forthSpillTop;
    xcopy(p, &type, 4);
    xcopy(p + 4, &blocks, 2);
    xcopy(p + 6, getRegisterDataPointer(reg), (uint32_t)blocks * 4u);
  }
  forthSpillTop += 6u + (uint32_t)blocks * 4u;
  forthSpillSlots++;
  return true;
}

bool_t forthSpillRefill(calcRegister_t reg)
{
  if (forthSpillSlots == 0) { return false; }
  { /* walk from the base to find the LAST slot's offset */
    uint32_t off = 0, prev = 0; uint16_t n = forthSpillSlots;
    while (n-- > 0) {
      uint16_t blocks; prev = off;
      xcopy(&blocks, (uint8_t *)forthSpillBase + off + 4, 2);
      off += 6u + (uint32_t)blocks * 4u;
    }
    { uint8_t *p = (uint8_t *)forthSpillBase + prev;
      uint32_t type; uint16_t blocks;
      xcopy(&type, p, 4);
      xcopy(&blocks, p + 4, 2);
      freeRegisterData(reg);
      setRegisterDataPointer(reg, allocC47Blocks(blocks));
      if (getRegisterDataPointer(reg) == NULL) { return false; }
      setRegisterDataType(reg, (uint16_t)type, amNone);
      xcopy(getRegisterDataPointer(reg), p + 6, (uint32_t)blocks * 4u);
      forthSpillTop = prev;
      forthSpillSlots--;
    }
  }
  return true;
}

static int16_t forthStackCapacity(void)
{
  return (int16_t)(getStackTop() - REGISTER_X + 1);
}

void forthDataDepthEnterOuter(void)
{
  forthSpillReset();
  forthOuterActive = true;
  forthDataDepth   = 0;
}

void forthDataDepthLeaveOuter(void)
{
  forthSpillReset();
  forthOuterActive = false;
}

/* A native item ran and its stack effect is not knowable from here. Restart the
 * count from a conservative floor rather than abandoning it: 0 is never ABOVE
 * the true depth, so the guard can only fire late, never falsely. Abandoning it
 * (the first design) left the guard disabled for the rest of any line
 * containing a native item -- including the usual `XEQ 'CLSTK'` prefix, after
 * which 0 happens to be exactly right. */
void forthDataDepthResync(void)
{
  forthDataDepth = 0;
}

/* Apply a known net effect. Consumption never fails; growth is refused when it
 * would push a live Forth value off the top. */
bool_t forthDataDepthApply(int16_t net)
{
  /* Only meaningful while Forth is executing. forthPushInt32/forthPushReal34
   * are public helpers that callers (and the self-test harness) use to seed the
   * RPN stack outside any Forth line; counting those would accumulate a stale
   * depth and refuse a later legitimate push. */
  if (!forthOuterActive && forthDepth == 0) {
    return true;
  }
  if (net > 0 && forthDataDepth + net > forthStackCapacity()) {
    lastErrorCode = ERROR_RAM_FULL;
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  forthDataDepth += net;
  if (forthDataDepth < 0) {
    forthDataDepth = 0;       /* consumed inherited values, not Forth's own */
  }
  return true;
}

/* ASLIFT is left SET after a push: a value now sits in X, and upstream's
 * SLS_ENABLED convention says whatever runs next must lift onto it rather than
 * overwrite it. Clearing it here made a following native item (RCL, and any
 * other result-producing item) clobber X instead of lifting -- see
 * DEFECTS_stack_semantics.md D1. Each push sets the flag on entry for its own
 * lift, so nothing depends on it being clear. */
void forthPushReal34(const real34_t *r)
{
  if (!forthDataDepthApply(+1)) {
    return;
  }
  setSystemFlag(FLAG_ASLIFT);
  liftStack();
  if (lastErrorCode == ERROR_NONE) {
    real34Copy(r, REGISTER_REAL34_DATA(REGISTER_X));
  }
}

void forthPushInt32(int32_t v)
{
  if (!forthDataDepthApply(+1)) {
    return;
  }
  setSystemFlag(FLAG_ASLIFT);
  liftStack();
  if (lastErrorCode == ERROR_NONE) {
    longInteger_t lgInt;
    longIntegerInit(lgInt);
    int32ToLongInteger(v, lgInt);
    convertLongIntegerToLongIntegerRegister(lgInt, REGISTER_X);
    longIntegerFree(lgInt);
  }
}

/* ---- 0BR: type-dispatched zero test (§3.2, NOT raw real34 read) ---- */

static bool_t popIsFalse(void)
{
  bool_t isZero = false;
  uint32_t dtype = getRegisterDataType(REGISTER_X);

  switch (dtype) {
    case dtReal34:
      isZero = real34IsZero(REGISTER_REAL34_DATA(REGISTER_X));
      break;
    case dtComplex34:
      isZero = (real34IsZero(REGISTER_REAL34_DATA(REGISTER_X)) &&
                real34IsZero(VARIABLE_IMAG34_DATA(getRegisterDataPointer(REGISTER_X))));
      break;
    case dtLongInteger: {
      /* C47 sign tag unreliable for computed zeros (e.g. 1-1=0 keeps LI_POSITIVE).
       * Check actual value via GMP mpz_cmp_ui. */
      longInteger_t li;
      longIntegerInit(li);
      convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
      isZero = (mpz_cmp_ui(li, 0) == 0);
      longIntegerFree(li);
      break;
    }
    case dtShortInteger:
      isZero = (*REGISTER_SHORT_INTEGER_DATA(REGISTER_X) == 0);
      break;
    case dtTime:
      isZero = real34IsZero(REGISTER_REAL34_DATA(REGISTER_X));
      break;
    default:
      /* Non-numeric types: truthy (e.g. non-empty string) */
      isZero = false;
      break;
  }

  /* FTOK_0BR CONSUMES its operand (DESIGN.md §2.2): pops X after the
     zero/false test.  IF compiles a DUP before 0BR precisely because
     0BR pops the tested value. */
  fnDrop(NOPARAM);
  (void)forthDataDepthApply(-1);
  return isZero;
}

/* ---- DMCP key poll: R/S (36) or EXIT (33) interrupt (§3.2) ---- */

#if defined(DMCP_BUILD)
static bool_t pollProgramInterrupt(void)
{
  int key = C47PopKeyNoBuffer(DISPLAY_WAIT_FOR_RELEASE) + 1;
  if (key == 36 || key == 33) {
    programRunStop = PGM_WAITING;
    return true;
  } else if (key > 0) {
    setLastKeyCode(key);
  }
  return false;
}
#endif

/* ---- F3-6: XEQN shared dispatch (kind-faithful, B2 chain, B4 matrix) ---- */

forthXeqnResult_t forthXeqnDispatch(const char *name, uint8_t kind, uint16_t *colonRef)
{
  /* 1. Label lookup with stored kind (position-sensitive inherited) */
  calcRegister_t label = findNamedLabel(name, kind);
  if (label != INVALID_VARIABLE) {
    dynamicMenuItem = -1;
    fnExecute((uint16_t)label);
    forthDataDepthResync();   /* R47 label body: resync the count (D2) */
    if (lastErrorCode != ERROR_NONE) return FORTH_XEQN_ERR;
    return FORTH_XEQN_DONE;
  }

  /* 2. Kind-faithful: local miss is terminal — no fallback */
  if (kind == LOCAL_LABEL_VARIABLE) {
    displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return FORTH_XEQN_ERR;
  }

  /* 3. Global-kind miss: B2 chain — prim, colon, item */
  {
    uint16_t pidx = forthFindPrim(name);
    if (pidx != FORTH_PRIM_NONE) {
      if (!forthDataDepthApply(forthPrims[pidx].stackEffect)) {
        return FORTH_XEQN_ERR;
      }
      forthPrims[pidx].fn();
      setSystemFlag(FLAG_ASLIFT);   /* SLS_ENABLED, as upstream's epilogue does */
      if (lastErrorCode != ERROR_NONE) return FORTH_XEQN_ERR;
      return FORTH_XEQN_DONE;
    }
  }
  {
    uint16_t ref;
    uint8_t fl;
    if (forthFindColonRef(name, &ref, &fl)) {
      *colonRef = ref;
      return FORTH_XEQN_COLON;
    }
  }
  {
    uint16_t itemId;
    if (forthFindItem(name, &itemId)) {
      uint8_t savedRunStop = programRunStop;
      programRunStop = PGM_RUNNING;
      reallyRunFunction((int16_t)itemId, NOPARAM);
      forthDataDepthResync();   /* native item: resync the count (D2) */
      if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
      if (lastErrorCode != ERROR_NONE) return FORTH_XEQN_ERR;
      return FORTH_XEQN_DONE;
    }
  }

  displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  return FORTH_XEQN_ERR;
}

/* ---- Region dispatch helpers (interpreter only) ---- */

static inline uint8_t *innerBase(bool g) { return g ? gdict.base : fdict.base; }
static inline uint16_t innerHere(bool g) { return g ? gdict.here : fdict.here; }

/* ---- Dictionary body lookup: ref → region-relative body offset ---- */

static uint16_t bodyOffsetOfRef(uint16_t ref)
{
  bool g = (ref & FORTH_REF_GLOBAL) != 0;
  uint16_t idx = (uint16_t)(ref & 0x7FFFu);
  forthDict_t *d = g ? &gdict : &fdict;
  uint16_t off = d->latest;
  uint16_t n = 0;

  if (idx >= d->count || !d->base) {
    return FORTH_NULL;
  }

  while (off != FORTH_NULL) {
    if (d->count - 1 - n == idx) {
      forthHeader_t *hdr = (forthHeader_t *)(d->base + off);
      uint16_t hdrSize = 6 + hdr->nameLen;
      uint16_t alignedHdr = (uint16_t)TO_BLOCKS(hdrSize) * BYTES_PER_BLOCK;
      return off + alignedHdr;
    }
    forthHeader_t *hdr = (forthHeader_t *)(d->base + off);
    off = hdr->link;
    n++;
  }

  return FORTH_NULL;
}

/* ---- Read a little-endian ftoken_t from dictionary ---- */

static inline ftoken_t readToken(bool g, uint16_t ip)
{
  uint8_t *b = innerBase(g);
  uint8_t lo = b[ip];
  uint8_t hi = b[ip + 1];
  return (ftoken_t)((hi << 8) | lo);
}

/* R1-2: forthInner read the next token and every inline LIT/ILIT/branch/C47
 * operand directly from the active region base with no proof the bytes lie
 * below the region's here. One guard, checked before every fixed-size
 * inline read; callers exit via INNER_LEAVE() on false so rsp/forthDepth
 * unwind (this function cannot call that macro itself — it is scoped to
 * forthInner's locals). */
static inline bool boundedRead(bool g, uint16_t ip, uint16_t byteCount)
{
  if ((uint32_t)ip + byteCount <= innerHere(g)) {
    return true;
  }
  lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
  displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  return false;
}

/* ======================================================================
 *  §3.2 Inner interpreter: fetch-decode-dispatch (cross-region)
 * ====================================================================== */

/* F4-3: shared marker-cell dispatch — the ONE dispatch body, used by the
 * FTOK_C47 runtime decode below and by forth_compile.c's interpret path.
 * paramMode is the native PARAM_* selector: (status & PTP_STATUS) >> 9. */
void forthParamMarkerDispatch(uint16_t op, uint16_t ptpClass, uint8_t *nbuf, uint16_t used)
{
  uint8_t savedRunStop = programRunStop;
  programRunStop = PGM_RUNNING;
  paramCoreExecuteOpBounded(nbuf, nbuf + used, op, (uint16_t)(ptpClass >> 9));
  if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
}

typedef enum { FORTH_MARK_NONE, FORTH_MARK_OK, FORTH_MARK_BAD } forthMarkResult_t;

/* F4-3: decode one marker parameter cell group at ip into nbuf.
 * NONE = the leading byte is not a marker legal for this class (the caller
 * decodes its own direct form); OK = decoded, *used and *advance set;
 * BAD = malformed encoding, ERROR_INVALID_CORRUPTED_DATA already raised.
 * Legality comes from forthParamMarkerMask, and the cell grammar from
 * forthParamCellSpan — the same two functions the validator walks use, so a
 * body that validates always decodes and vice versa. The caller has already
 * bounded-read the first cell. */
static forthMarkResult_t decodeMarkerCell(bool g, uint16_t ip, uint16_t ptpClass,
                                          uint8_t *nbuf, uint16_t *used, uint16_t *advance)
{
  const uint8_t *b = innerBase(g);
  uint8_t b0 = b[ip];
  uint8_t bit = (uint8_t)(forthParamMarkerBit(b0) & forthParamMarkerMask(ptpClass));
  uint16_t span;

  if (!bit) return FORTH_MARK_NONE;
  /* Native order: for the NUMBER classes a legal direct value wins over the
   * marker reading (param_core.c tries the direct dispatch first). */
  if ((ptpClass == PTP_NUMBER_8 || ptpClass == PTP_NUMBER_8_16) && b0 <= 249) {
    return FORTH_MARK_NONE;
  }
  if (!forthParamCellSpan(b, ip, innerHere(g), ptpClass, true, &span)) {
    lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
    displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return FORTH_MARK_BAD;
  }
  if (!boundedRead(g, ip, span)) return FORTH_MARK_BAD;
  nbuf[0] = b0;
  nbuf[1] = b[ip + 1];
  if (bit == FORTH_MK_NAME || bit == FORTH_MK_IND_VAR) {
    memcpy(nbuf + 2, b + ip + 2, b[ip + 1]);
    *used = (uint16_t)(2 + b[ip + 1]);
  }
  else {
    *used = 2;
  }
  *advance = span;
  return FORTH_MARK_OK;
}

void forthInner(uint16_t wordRef, bool fromProgram)
{
  uint32_t dispatches = 0;
  bool curG = (wordRef & FORTH_REF_GLOBAL) != 0;

  /* Re-entrancy (§3.2, D-3): bounded nesting; rstack shared via watermark */
  if (forthDepth >= FORTH_NEST_MAX) {
    lastErrorCode = ERROR_OPERATION_UNDEFINED;   /* C-12: same code as old guard */
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (forthDepth == 0 && !forthOuterActive) {
    forthDataDepth = 0;       /* direct entry (XEQ, ITM_FCALL, harness) */
  }
  forthDepth++;
  uint8_t rspBase = rsp;   /* watermark: this level's rstack floor */
  #define INNER_LEAVE() do { rsp = rspBase; forthDepth--; return; } while (0)

  /* Resolve body start */
  uint16_t ip = bodyOffsetOfRef(wordRef);
  if (ip == FORTH_NULL) {
    lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                  ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          INNER_LEAVE();
   }

  for (;;) {
    /* Cooperative break: async stop from dispatched item */
    if (fromProgram && programRunStop != PGM_RUNNING) {
      break;
    }

    /* Key poll — primary interrupt on DMCP hardware (§3.2) */
#if defined(DMCP_BUILD)
    if (pollProgramInterrupt()) {
      INNER_LEAVE();
    }
#endif

    /* Runaway backstop (§2.2) */
    if (++dispatches >= RUNAWAY_CAP) {
      lastErrorCode = ERROR_RAM_FULL;
      displayCalcErrorMessage(ERROR_RAM_FULL,
                               ERR_REGISTER_LINE, NIM_REGISTER_LINE);
      INNER_LEAVE();
    }

    /* ---- FETCH ---- */
    if (!boundedRead(curG, ip, 2)) {
      INNER_LEAVE();
    }
    ftoken_t tok = readToken(curG, ip);
    ip += 2;

    /* ---- DECODE / DISPATCH ---- */

    if (tok == FTOK_EXIT) {
      if (rsp == rspBase) {
        /* Normal exit: set ASLIFT (C47 convention, §3.2) */
        setSystemFlag(FLAG_ASLIFT);
        forthDepth--;
        return;
      }
      --rsp;
      ip = rstack[rsp];
      curG = (rstackRegionBits >> rsp) & 1;
      continue;
    }

    if (tok >= FTOK_PRIM_BASE && tok <= 0x0FFF) {
      /* FTOK_PRIM: token = index + 1 (§2.2) */
      uint16_t primIdx = (uint16_t)(tok - 1);
      if (primIdx >= forthPrimCount) {
        lastErrorCode = ERROR_OPERATION_UNDEFINED;
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                 ERR_REGISTER_LINE, NIM_REGISTER_LINE);
         INNER_LEAVE();
       }
       if (!forthDataDepthApply(forthPrims[primIdx].stackEffect)) {
         INNER_LEAVE();
       }
       forthPrims[primIdx].fn();
       /* SLS_ENABLED: every prim-equivalent item upstream (fnAdd, fnDrop,
        * fnSwapXY, fnMultiply ...) carries it, so the epilogue sets ASLIFT.
        * Prims bypass reallyRunFunction(), so mirror it here. D1. */
       setSystemFlag(FLAG_ASLIFT);
       if (lastErrorCode != ERROR_NONE) {
         INNER_LEAVE();
       }
       continue;
    }

    if (tok <= 0x7EFF) {
      /* FTOK_CALL: ref = forthRefFromToken(tok) */
      if (rsp >= FORTH_RSTACK_DEPTH) {
        lastErrorCode = ERROR_RAM_FULL;
        displayCalcErrorMessage(ERROR_RAM_FULL,
                                 ERR_REGISTER_LINE, NIM_REGISTER_LINE);
         INNER_LEAVE();
       }
        uint16_t calleeRef = forthRefFromToken(tok);
       rstack[rsp] = ip;
       if (curG) rstackRegionBits |= (1ull << rsp);
       else rstackRegionBits &= ~(1ull << rsp);
       rsp++;
       curG = (calleeRef & FORTH_REF_GLOBAL) != 0;
       ip = bodyOffsetOfRef(calleeRef);
       if (ip == FORTH_NULL) {
         lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
         displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                   ERR_REGISTER_LINE, NIM_REGISTER_LINE);
           INNER_LEAVE();
         }
       continue;
    }

    switch (tok) {
      case FTOK_LIT: {
        /* Push 16-byte real34 literal (§2.2) */
        if (!boundedRead(curG, ip, (uint16_t)sizeof(real34_t))) {
          INNER_LEAVE();
        }
        real34_t litVal;
        memcpy(&litVal, innerBase(curG) + ip, sizeof(real34_t));
        ip += (uint16_t)sizeof(real34_t);
        forthPushReal34(&litVal);
        if (lastErrorCode != ERROR_NONE) {
          INNER_LEAVE();
        }
        break;
      }

        case FTOK_ILIT: {
          /* Push 4-byte int32 as dtLongInteger (§2.2, §3.3.5)
           * memcpy avoids sign-extension bug on byte 0 (fix #1). */
          if (!boundedRead(curG, ip, 4)) {
            INNER_LEAVE();
          }
          int32_t v;
          memcpy(&v, innerBase(curG) + ip, 4);
          ip += 4;
          forthPushInt32(v);
          if (lastErrorCode != ERROR_NONE) {
            INNER_LEAVE();
          }
          break;
        }

        case FTOK_BR: {
         /* Unconditional branch: signed int16 delta in cells (§2.2)
          * memcpy avoids sign-extension bug on byte 0 (fix #16a). */
         if (!boundedRead(curG, ip, 2)) {
           INNER_LEAVE();
         }
         int16_t delta;
         memcpy(&delta, innerBase(curG) + ip, 2);
         ip += 2;
          ip += (int32_t)delta * 2;
          break;
        }

        case FTOK_0BR: {
          /* Conditional branch: pop X, branch if zero/false (§2.2, §3.2)
           * memcpy avoids sign-extension bug on byte 0 (fix #16b). */
          if (!boundedRead(curG, ip, 2)) {
            INNER_LEAVE();
          }
          int16_t delta;
          memcpy(&delta, innerBase(curG) + ip, 2);
          ip += 2;
          if (popIsFalse()) {
            ip += (int32_t)delta * 2;
          }
          if (lastErrorCode != ERROR_NONE) {
            INNER_LEAVE();
          }
          break;
        }

        case FTOK_C47: {
        /* §2.2: decode itemId, param per PTP class, dispatch to C47 handler */
        if (!boundedRead(curG, ip, 2)) {
          INNER_LEAVE();
        }
        uint8_t *b = innerBase(curG);
        uint16_t itemId = (uint16_t)(b[ip] |
                                     ((uint16_t)b[ip + 1] << 8));
        ip += 2;

        /* Bounds-check: itemId must be a valid indexOfItems entry */
        if (itemId >= LAST_ITEM) {
          lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
          displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                   ERR_REGISTER_LINE, NIM_REGISTER_LINE);
           INNER_LEAVE();
         }

         /* Determine PTP class from item status (bits 9-13) */
        uint16_t ptpClass = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);

        /* Decode inline param per PTP class; unsupported PTP → error */
        uint16_t param = NOPARAM;
        /* F4-3: marker forms first — the mask says which markers this class
         * accepts, and a hit dispatches straight through the bounded native
         * core (names resolve at run time, source-as-truth). */
        {
          uint8_t nbuf[2 + FORTH_NAME_MAX];
          uint16_t nused = 0, nadv = 0;
          if (forthParamMarkerMask(ptpClass) != 0) {
            forthMarkResult_t mk;
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            mk = decodeMarkerCell(curG, ip, ptpClass, nbuf, &nused, &nadv);
            if (mk == FORTH_MARK_BAD) {
              INNER_LEAVE();
            }
            if (mk == FORTH_MARK_OK) {
              ip += nadv;
              forthParamMarkerDispatch(itemId, ptpClass, nbuf, nused);
              if (lastErrorCode != ERROR_NONE) {
                INNER_LEAVE();
              }
              goto c47_done;
            }
          }
        }

        switch (ptpClass) {
          case PTP_NONE:
            /* No inline param */
            break;
          case PTP_NUMBER_8:
            /* 1-byte value padded to a 2-byte cell (§2.2 resolved issue 1) */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            param = (uint16_t)b[ip];
            ip += 2;
            break;
          case PTP_NUMBER_16:
            /* F4-3: no marker forms — a [254][ks] cell is indistinguishable
             * from a legal little-endian value with low byte 254. */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            memcpy(&param, b + ip, 2);
            ip += 2;
            break;
          case PTP_NUMBER_8_16: {
            /* Bounded-read one cell; b0 = byte0; b0 <= 249 -> param = b0;
             * b0 == 250 -> param = 250 + byte1; b0 >= 251 -> error. */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            uint8_t b0 = b[ip];
            uint8_t b1 = b[ip + 1];
            ip += 2;
            if (b0 <= 249) {
              param = (uint16_t)b0;
            } else if (b0 == 250) {
              param = (uint16_t)(250 + b1);
            } else {
              lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
              displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                       ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              INNER_LEAVE();
            }
            break;
          }
          case PTP_REGISTER: {
            /* F4-2: one cell, b0 <= 224 legal; anything else is either a
             * marker (handled above) or a corrupt cell — fail loud. The
             * silence parity is for legal-form out-of-range VALUES. */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            uint8_t b0 = b[ip];
            ip += 2;
            if (b0 <= LAST_SPARE_REGISTERS_IN_KS_CODE) {
              param = (uint16_t)b0;
            } else {
              lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
              displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                       ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              INNER_LEAVE();
            }
            break;
          }
          case PTP_FLAG: {
            /* F4-2: one cell, legal bytes (<=143 or 211..224); 250 and the
             * indirection markers are handled above. */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            uint8_t b0 = b[ip];
            ip += 2;
            if (b0 <= LAST_LOCAL_FLAG || (FLAG_M <= b0 && b0 <= FLAG_W)) {
              param = (uint16_t)b0;
            } else {
              lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
              displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                       ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              INNER_LEAVE();
            }
            break;
          }
          case PTP_SHUFFLE: {
            /* F4-2: one cell, any byte. */
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            param = (uint16_t)b[ip];
            ip += 2;
            break;
          }
          case PTP_MENU:
            /* F4-3: MENU has no direct form — every legal cell is a marker
             * cell, so reaching here means the cell is corrupt. */
            lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
            displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                     ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            INNER_LEAVE();
          default:
            /* PTP_LABEL, etc. not supported yet */
            lastErrorCode = ERROR_OPERATION_UNDEFINED;
            displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                     ERR_REGISTER_LINE, NIM_REGISTER_LINE);
             INNER_LEAVE();
          }

          /* F2-3/F4-2: dispatch through shared parameter core (§10.2).
           * PGM_RUNNING save/set/restore wrap around the call (§2.2 resolved issue 2). */
         if (paramCoreValidateDirect(itemId, ptpClass, param)) {
           uint8_t savedRunStop = programRunStop;
           programRunStop = PGM_RUNNING;
           paramCoreDispatchDirect(itemId, ptpClass, param);
           if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
         }
         /* else: native-parity silence — no error, no dispatch */

        if (lastErrorCode != ERROR_NONE) {
          INNER_LEAVE();
        }
        c47_done:
        forthDataDepthResync();   /* native stack effect is opaque */
        break;
      }

      case FTOK_XEQN: {
        /* F3-6: XEQN runtime dispatch — bounded-read kind/len/name/pad */
        if (!boundedRead(curG, ip, 2)) {
          INNER_LEAVE();
        }
        uint8_t xkind = innerBase(curG)[ip];
        uint8_t xnlen = innerBase(curG)[ip + 1];
        if (xkind != STRING_LABEL_VARIABLE && xkind != LOCAL_LABEL_VARIABLE) {
          lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
          displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                  ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          INNER_LEAVE();
        }
        if (xnlen < 1 || xnlen > FORTH_NAME_MAX) {
          lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
          displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                  ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          INNER_LEAVE();
        }
        uint16_t xinline = (uint16_t)(2 + xnlen);
        uint16_t xpadded = (uint16_t)((xinline + 1) & ~1u);
        if (!boundedRead(curG, ip, xpadded)) {
          INNER_LEAVE();
        }
        if (xpadded > xinline && innerBase(curG)[ip + xinline] != 0) {
          lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
          displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                  ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          INNER_LEAVE();
        }
        char xname[FORTH_NAME_MAX + 1];
        memcpy(xname, innerBase(curG) + ip + 2, xnlen);
        xname[xnlen] = 0;
        ip += xpadded;

        uint16_t colonRef;
        forthXeqnResult_t xr = forthXeqnDispatch(xname, xkind, &colonRef);
        if (xr == FORTH_XEQN_COLON) {
          /* FTOK_CALL dispatch shape */
          if (rsp >= FORTH_RSTACK_DEPTH) {
            lastErrorCode = ERROR_RAM_FULL;
            displayCalcErrorMessage(ERROR_RAM_FULL,
                                    ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            INNER_LEAVE();
          }
          rstack[rsp] = ip;
          if (curG) rstackRegionBits |= (1ull << rsp);
          else rstackRegionBits &= ~(1ull << rsp);
          rsp++;
          curG = (colonRef & FORTH_REF_GLOBAL) != 0;
          ip = bodyOffsetOfRef(colonRef);
          if (ip == FORTH_NULL) {
            lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
            displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                    ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            INNER_LEAVE();
          }
        }
        if (lastErrorCode != ERROR_NONE) {
          INNER_LEAVE();
        }
        break;
      }

      default:
        lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        INNER_LEAVE();
    }
  }

  rsp = rspBase;
  forthDepth--;
  #undef INNER_LEAVE
}

/* Test-only: prime/read nesting depth for guard tests (§3.2) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestSetDepth(uint8_t d) { forthDepth = d; }
uint8_t forthTestGetDepth(void) { return forthDepth; }
uint8_t forthTestGetRsp(void) { return rsp; }
#endif
