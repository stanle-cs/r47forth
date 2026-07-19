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

void forthPushReal34(const real34_t *r)
{
  setSystemFlag(FLAG_ASLIFT);
  liftStack();
  clearSystemFlag(FLAG_ASLIFT);
  if (lastErrorCode == ERROR_NONE) {
    real34Copy(r, REGISTER_REAL34_DATA(REGISTER_X));
  }
}

void forthPushInt32(int32_t v)
{
  setSystemFlag(FLAG_ASLIFT);
  liftStack();
  clearSystemFlag(FLAG_ASLIFT);
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
       forthPrims[primIdx].fn();
       clearSystemFlag(FLAG_ASLIFT);
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
            if (!boundedRead(curG, ip, 2)) {
              INNER_LEAVE();
            }
            param = (uint16_t)(b[ip] |
                               ((uint16_t)b[ip + 1] << 8));
            ip += 2;
            break;
          default:
            /* PTP_LABEL, PTP_REGISTER, etc. not supported yet (C-1, §3.3.6) */
            lastErrorCode = ERROR_OPERATION_UNDEFINED;
            displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                     ERR_REGISTER_LINE, NIM_REGISTER_LINE);
             INNER_LEAVE();
         }

          /* F2-3: dispatch through shared parameter core (§10.2).
           * PGM_RUNNING save/set/restore wrap around the call (§2.2 resolved issue 2). */
         if (paramCoreValidateDirect(itemId, ptpClass, param)) {
           uint8_t savedRunStop = programRunStop;
           programRunStop = PGM_RUNNING;
           paramCoreDispatchDirect(itemId, param);
           if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
         }
         /* else: native-parity silence — no error, no dispatch */

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
