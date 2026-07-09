/*
 * forth_inner.c -- Forth inner interpreter (fetch-decode-dispatch loop)
 * Per DESIGN.md §2 (token table) and §3.2 (inner interpreter)
 * FTOK_C47: §2.2 decoder (PGM_RUNNING wrap lands in H1)
 */

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"

/* ---- §2.2 Token constants ---- */

#define FTOK_EXIT         0x0000
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
static bool     forthRunning = false;

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

/* ---- Dictionary body lookup: index → region-relative body offset ---- */

static uint16_t bodyOffsetOfIndex(uint16_t idx)
{
  uint16_t off = fdict.latest;
  uint16_t n = 0;

  if (idx >= fdict.count || !fdict.base) {
    return FORTH_NULL;
  }

  while (off != FORTH_NULL) {
    if (fdict.count - 1 - n == idx) {
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      uint16_t hdrSize = 4 + hdr->nameLen;
      uint16_t alignedHdr = (uint16_t)TO_BLOCKS(hdrSize) * BYTES_PER_BLOCK;
      return off + alignedHdr;
    }
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
    off = hdr->link;
    n++;
  }

  return FORTH_NULL;
}

/* ---- Read a little-endian ftoken_t from dictionary ---- */

static inline ftoken_t readToken(uint16_t ip)
{
  uint8_t lo = fdict.base[ip];
  uint8_t hi = fdict.base[ip + 1];
  return (ftoken_t)((hi << 8) | lo);
}

/* ======================================================================
 *  §3.2 Inner interpreter: fetch-decode-dispatch
 * ====================================================================== */

void forthInner(uint16_t entryIndex, bool fromProgram)
{
  uint32_t dispatches = 0;

  /* Re-entrancy guard (§3.2): nested entry destroys outer rstack */
  if (forthRunning) {
    lastErrorCode = ERROR_OPERATION_UNDEFINED;
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  forthRunning = true;

  /* Resolve body start */
  uint16_t ip = bodyOffsetOfIndex(entryIndex);
  if (ip == FORTH_NULL) {
    lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
    displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    forthRunning = false;
    return;
  }

  rsp = 0;

  for (;;) {
    /* Cooperative break: async stop from dispatched item */
    if (fromProgram && programRunStop != PGM_RUNNING) {
      break;
    }

    /* Key poll — primary interrupt on DMCP hardware (§3.2) */
#if defined(DMCP_BUILD)
    if (pollProgramInterrupt()) {
      forthRunning = false;
      return;
    }
#endif

    /* Runaway backstop (§2.2) */
    if (++dispatches >= RUNAWAY_CAP) {
      lastErrorCode = ERROR_RAM_FULL;
      displayCalcErrorMessage(ERROR_RAM_FULL,
                              ERR_REGISTER_LINE, NIM_REGISTER_LINE);
      forthRunning = false;
      return;
    }

    /* ---- FETCH ---- */
    ftoken_t tok = readToken(ip);
    ip += 2;

    /* ---- DECODE / DISPATCH ---- */

    if (tok == FTOK_EXIT) {
      if (rsp == 0) {
        /* Normal exit: set ASLIFT (C47 convention, §3.2) */
        setSystemFlag(FLAG_ASLIFT);
        forthRunning = false;
        return;
      }
      ip = rstack[--rsp];
      continue;
    }

    if (tok >= FTOK_PRIM_BASE && tok <= 0x0FFF) {
      /* FTOK_PRIM: token = index + 1 (§2.2) */
      uint16_t primIdx = (uint16_t)(tok - 1);
      if (primIdx >= forthPrimCount) {
        lastErrorCode = ERROR_OPERATION_UNDEFINED;
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        forthRunning = false;
        return;
      }
       forthPrims[primIdx].fn();
       clearSystemFlag(FLAG_ASLIFT);
       if (lastErrorCode != ERROR_NONE) {
         forthRunning = false;
         return;
       }
       continue;
    }

    if (tok <= 0x7EFF) {
      /* FTOK_CALL: colon def index = tok - 0x1000 */
      if (rsp >= FORTH_RSTACK_DEPTH) {
        lastErrorCode = ERROR_RAM_FULL;
        displayCalcErrorMessage(ERROR_RAM_FULL,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        forthRunning = false;
        return;
      }
      rstack[rsp++] = ip;
      ip = bodyOffsetOfIndex((uint16_t)(tok - FTOK_CALL_BASE));
      if (ip == FORTH_NULL) {
        rsp--;
        lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        forthRunning = false;
        return;
      }
      continue;
    }

    switch (tok) {
      case FTOK_LIT: {
        /* Push 16-byte real34 literal (§2.2) */
        real34_t litVal;
        memcpy(&litVal, fdict.base + ip, sizeof(real34_t));
        ip += (uint16_t)sizeof(real34_t);
        forthPushReal34(&litVal);
        if (lastErrorCode != ERROR_NONE) {
          forthRunning = false;
          return;
        }
        break;
      }

        case FTOK_ILIT: {
          /* Push 4-byte int32 as dtLongInteger (§2.2, §3.3.5)
           * memcpy avoids sign-extension bug on byte 0 (fix #1). */
          int32_t v;
          memcpy(&v, fdict.base + ip, 4);
          ip += 4;
          forthPushInt32(v);
         if (lastErrorCode != ERROR_NONE) {
           forthRunning = false;
           return;
         }
         break;
       }

       case FTOK_BR: {
         /* Unconditional branch: signed int16 delta in cells (§2.2)
          * memcpy avoids sign-extension bug on byte 0 (fix #16a). */
         int16_t delta;
         memcpy(&delta, fdict.base + ip, 2);
         ip += 2;
          ip += (int32_t)delta * 2;
          break;
        }

        case FTOK_0BR: {
          /* Conditional branch: pop X, branch if zero/false (§2.2, §3.2)
           * memcpy avoids sign-extension bug on byte 0 (fix #16b). */
          int16_t delta;
          memcpy(&delta, fdict.base + ip, 2);
          ip += 2;
          if (popIsFalse()) {
            ip += (int32_t)delta * 2;
          }
         if (lastErrorCode != ERROR_NONE) {
           forthRunning = false;
           return;
         }
         break;
       }

      case FTOK_C47: {
        /* §2.2: decode itemId, param per PTP class, dispatch to C47 handler */
        uint16_t itemId = (uint16_t)(fdict.base[ip] |
                                     ((uint16_t)fdict.base[ip + 1] << 8));
        ip += 2;

        /* Bounds-check: itemId must be a valid indexOfItems entry */
        if (itemId >= LAST_ITEM) {
          lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
          displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                  ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          forthRunning = false;
          return;
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
            param = (uint16_t)fdict.base[ip];
            ip += 2;
            break;
          case PTP_NUMBER_16:
            param = (uint16_t)(fdict.base[ip] |
                               ((uint16_t)fdict.base[ip + 1] << 8));
            ip += 2;
            break;
          default:
            /* PTP_LABEL, PTP_REGISTER, etc. not supported yet (C-1, §3.3.6) */
            lastErrorCode = ERROR_OPERATION_UNDEFINED;
            displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                    ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            forthRunning = false;
            return;
        }

        /* H1: PGM_RUNNING save/set/restore wrap around this call (§2.2 resolved issue 2)
         * Pattern: save current state, mark RUNNING, execute, restore only if
         * unchanged.  If reallyRunFunction modifies programRunStop (e.g., keypress
         * aborts execution), the restore is intentionally skipped — the new value
         * (e.g., PGM_STOPPED) is preserved.  Safe under single-threaded assumption. */
        {
          uint8_t savedRunStop = programRunStop;
          programRunStop = PGM_RUNNING;
          reallyRunFunction((int16_t)itemId, param);
          if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
        }

        if (lastErrorCode != ERROR_NONE) {
          forthRunning = false;
          return;
        }
        break;
      }

      default:
        lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        forthRunning = false;
        return;
    }
  }

  forthRunning = false;
}

/* Test-only: prime forthRunning for re-entrancy guard test (§3.2) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestSetRunning(bool val) { forthRunning = val; }
bool forthTestIsRunning(void) { return forthRunning; }
#endif
