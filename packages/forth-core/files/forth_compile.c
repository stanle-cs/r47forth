/*
 * forth_compile.c -- Forth outer interpreter + tokenizer + number grammar
 * Per DESIGN.md §3.3.1 (state machine, C-4), §3.3.2 (source, C-5),
 * §3.3.3 (tokenizer, C-6), §3.3.5 (numbers, C-8), §3.3.6 (C47 labels, C-1)
 */

#include <string.h>

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"

/* ---- §2.2 Token constants (mirror forth_inner.c) ---- */

#define FTOK_PRIM_BASE    0x0001
#define FTOK_CALL_BASE    0x1000
#define FTOK_LIT          0x7F00
#define FTOK_ILIT         0x7F01
#define FTOK_C47          0x7F04

/* ---- §3.3.2 / D-3: per-invocation context; idle BSS = one ptr + 2 bytes ---- */
typedef enum {
  FORTH_OUTER_FULL      = 0,  /* compile and execute (interactive semantics) */
  FORTH_OUTER_DEFS_ONLY = 1,  /* pre-scan: compile definitions, skip ALL interpret-state tokens */
  FORTH_OUTER_SKIP_DEFS = 2   /* step execution: skip ':'..';' regions, execute the rest */
} forthOuterMode_t;

#define FORTH_SOURCE_MAX 256

typedef struct {
  char            source[FORTH_SOURCE_MAX];
  int16_t         pos;          /* tokenizer position */
  forthDefState_t savedDef;     /* outer level's open-definition snapshot */
  uint16_t        savedScope;   /* outer level's scope snapshot (F3-3) */
  uint16_t        savedLatestClosed; /* F3-4: outer level's tracker snapshot */
} forthOuterCtx_t;

#define FORTH_OUTER_NEST_MAX 2
static forthOuterCtx_t *forthOuterCur   = NULL;
static uint8_t          forthOuterDepth = 0;

/* F3-4: same-line tracker — most recently closed definition on this line */
static uint16_t forthLatestClosedRef = FORTH_NULL;

uint16_t forthLatestClosedRefGet(void) { return forthLatestClosedRef; }
void     forthLatestClosedRefSet(uint16_t ref) { forthLatestClosedRef = ref; }

/* ---- §9.3 Run-generation counter (P-5, F1-1: pending-reset truth) ---- */

static uint16_t forthRunGeneration   = 0;
static uint16_t forthResetGeneration = 0;
static bool     forthResetPending    = false;

void forthRunGenBump(void) {
  forthRunGeneration++;                 /* diagnostic only; wrapping is fine */
  if (!forthInnerIsActive()) {
    forthResetPending = true;
  }
}

/* §9.2 first-touch pre-scan tracking — F1-3 (R4-E1): dynamic records inside
 * the dictionary region. One 8-byte record per scanned program:
 * [uint32 progOffset][uint16 prevOff][uint16 zero], newest at forthScanHead.
 * Records die with the region (clear/init/restore reset the head); capacity
 * failure is ordinary dictionary exhaustion. The two walk guards below are
 * defense-in-depth for a dangling head — declared redundant on every
 * production path (a generation seam precedes every query); do not remove
 * without re-running the mutation analysis. */
static uint16_t forthScanHead = FORTH_NULL;

void forthScanTrackReset(void) {
  forthScanHead = FORTH_NULL;
}

static bool forthScanFindRecord(const uint8_t *progStart, uint16_t *recOff) {
  if (!fdict.base) {
    forthScanHead = FORTH_NULL;
    return false;
  }
  uint32_t key = (uint32_t)(progStart - beginOfProgramMemory);
  uint16_t off = forthScanHead;
  while (off != FORTH_NULL) {
    if ((uint32_t)off + 8u > fdict.here) {   /* dangling head: self-heal */
      forthScanHead = FORTH_NULL;
      return false;
    }
    uint32_t recKey;
    uint16_t prev;
    memcpy(&recKey, fdict.base + off, 4);
    memcpy(&prev, fdict.base + off + 4, 2);
    if (recKey == key) {
      *recOff = off;
      return true;
    }
    if (prev != FORTH_NULL && prev >= off) { /* chain must strictly decrease */
      forthScanHead = FORTH_NULL;
      return false;
    }
    off = prev;
  }
  return false;
}

static bool forthScanIsRecorded(const uint8_t *progStart) {
  uint16_t t; return forthScanFindRecord(progStart, &t);
}

static bool forthScanRecord(const uint8_t *progStart) {
  uint8_t rec[8];
  uint32_t key = (uint32_t)(progStart - beginOfProgramMemory);
  uint16_t newOff = fdict.here;
  memcpy(rec, &key, 4);
  memcpy(rec + 4, &forthScanHead, 2);
  rec[6] = 0;
  rec[7] = 0;
  if (!forthDictEmitBytes(rec, 8)) {
    return false;
  }
  forthScanHead = newOff;
  return true;
}

/* F3-3: scope variable -- current owner for definition stamping and filtered lookup */
static uint16_t forthCurrentScope = FORTH_OWNER_INTERACTIVE;
uint16_t forthCurrentScopeGet(void) { return forthCurrentScope; }

static void forthRunGenCheckReset(void) {
  if (!forthResetPending || forthInnerIsActive()) {
    return;
  }
  forthDictClear();
  forthResetGeneration = forthRunGeneration;  /* diagnostic sample only */
  forthResetPending = false;                  /* consume only after clear */
}

/* ---- §3.3.3 Tokenizer state (C-6) ---- */

static void forthTokenizerInit(void) {
  forthOuterCur->pos = 0;
}

/*
 * C-6: glyph-wise tokenizer.  Delimiter: exactly 0x20.
 * Returns false at end of line.
 */
static bool nextToken(char buf[FORTH_TOKEN_MAX + 1]) {
  while (forthOuterCur->source[forthOuterCur->pos] == ' ')
    forthOuterCur->pos = stringNextGlyph(forthOuterCur->source, forthOuterCur->pos);
  if (forthOuterCur->source[forthOuterCur->pos] == 0) return false;
  int16_t start = forthOuterCur->pos;
  while (forthOuterCur->source[forthOuterCur->pos] != 0 && forthOuterCur->source[forthOuterCur->pos] != ' ')
    forthOuterCur->pos = stringNextGlyph(forthOuterCur->source, forthOuterCur->pos);
  int16_t len = forthOuterCur->pos - start;
  if (len > FORTH_TOKEN_MAX) {
    displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  xcopy(buf, forthOuterCur->source + start, len);
  buf[len] = 0;
  return true;
}

/* ---- §3.3.5 Number grammar (C-8) ---- */

typedef enum {
  FORTH_NUM_NONE = 0,
  FORTH_NUM_INT  = 1,
  FORTH_NUM_REAL = 2
} forthNumType_t;

/* Classify token against C-8 grammar. */
static forthNumType_t classifyNumber(const char *s) {
  int16_t i = 0;
  int16_t len = 0;

  while (s[len]) {
    if ((unsigned char)s[len] >= 0x80) return FORTH_NUM_NONE;
    len++;
  }
  if (len == 0) return FORTH_NUM_NONE;

  if (s[i] == '+' || s[i] == '-') i++;
  if (i >= len) return FORTH_NUM_NONE;

  bool hasDot = false;
  bool hasExp = false;
  int16_t mantissaDigits = 0;
  int16_t expDigits = 0;

  while (i < len) {
    if (s[i] >= '0' && s[i] <= '9') {
      if (hasExp) {
        expDigits++;
      } else {
        mantissaDigits++;
      }
      i++;
    } else if (s[i] == '.' && !hasDot && !hasExp) {
      hasDot = true;
      i++;
    } else if ((s[i] == 'e' || s[i] == 'E') && !hasExp) {
      hasExp = true;
      i++;
    } else if ((s[i] == '+' || s[i] == '-') && hasExp && expDigits == 0
               && i > 0 && (s[i - 1] == 'e' || s[i - 1] == 'E')) {
      /* R4-1: grammar is [eE][+-]?digit+ — a sign is legal ONLY as the first
       * byte immediately after e/E, and only one. The old clause accepted a
       * sign anywhere after an exponent marker (e.g. "1e2-3" tokenized as a
       * valid number instead of being rejected as an undefined word).
       * Probed: forthOuterInterpret("1e2-3 7") left lastErrorCode ==
       * ERROR_NONE. */
      i++;
    } else {
      return FORTH_NUM_NONE;
    }
  }

  if (hasExp && expDigits == 0) return FORTH_NUM_NONE;
  if (mantissaDigits == 0) return FORTH_NUM_NONE;
  if (hasDot || hasExp) return FORTH_NUM_REAL;
  return FORTH_NUM_INT;
}

/* Parse integer: skip '+', mpz_set_str, range-check int32. */
static bool parseNumberAsInt32(const char *buf, int32_t *out) {
  const char *p = buf;
  if (*p == '+') p++;

  longInteger_t li;
  longIntegerInit(li);

  if (stringToLongInteger(p, 10, li) != 0) {
    longIntegerFree(li);
    return false;
  }

  if (longIntegerCompareInt(li, INT32_MAX) <= 0 &&
      longIntegerCompareInt(li, INT32_MIN) >= 0) {
    int32_t v;
    longIntegerToInt32(li, v);
    *out = v;
    longIntegerFree(li);
    return true;
  }

  longIntegerFree(li);
  return false;
}

static decQuad *parseNumberAsReal34(const char *buf, real34_t *out) {
  return stringToReal34(buf, out);
}

/*
 * Process a number token in either state.
 * Compile: emit FTOK_ILIT or FTOK_LIT.
 * Interpret: push via forthPushInt32 / forthPushReal34.
 * Returns true on success, false on parse failure.
 */
static bool processNumber(const char *buf, bool compile) {
  forthNumType_t type = classifyNumber(buf);

  if (type == FORTH_NUM_INT) {
    int32_t v;
    if (parseNumberAsInt32(buf, &v)) {
      if (compile) {
        if (!forthDictEmit(FTOK_ILIT)) return false;
        if (!forthDictEmitBytes(&v, 4)) return false;
      } else {
        forthPushInt32(v);
      }
      return true;
    }
    /* Out of range int -> real34 fallback (C-8, both states) */
    real34_t r;
    if (!parseNumberAsReal34(buf, &r)) return false;
    if (compile) {
      if (!forthDictEmit(FTOK_LIT)) return false;
      if (!forthDictEmitBytes(&r, sizeof(r))) return false;
    } else {
      forthPushReal34(&r);
    }
    return true;
  }

  if (type == FORTH_NUM_REAL) {
    real34_t r;
    if (!parseNumberAsReal34(buf, &r)) return false;
    if (compile) {
      if (!forthDictEmit(FTOK_LIT)) return false;
      if (!forthDictEmitBytes(&r, sizeof(r))) return false;
    } else {
      forthPushReal34(&r);
    }
    return true;
  }

  return false;
}

/* ---- §3.3.1 State machine / outer interpreter (C-4) ---- */

typedef enum {
  STATE_INTERPRET = 0,
  STATE_COMPILE   = 1
} forthState_t;

/*
 * forthOuterRun — core interpret/compile loop.
 * Called by forthOuterInterpret, fnForthOuter, and forthProgramStep.
 * Per DESIGN.md §3.3 pseudocode, §3.3.1 (C-4), §3.3.6 (C-1).
 */
static void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode) {
  if (forthOuterDepth >= FORTH_OUTER_NEST_MAX) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  /* PRECONDITION (R4-E3, accepted): a nested outer interpretation must begin
   * with no open definition. Nested error paths call abortDefinition(),
   * which observes the OUTER invocation's openDef — the epilogue restores
   * openDef.open but not the dictionary bytes an inner abort rolled back.
   * Unreachable from natural stage-C paths (compile state executes nothing);
   * revisit only if an EVALUATE-like or immediate source word lands. */
  #ifdef FORTH_DEBUG_SELFTEST
  if (forthOuterDepth > 0 && isDefinitionOpen()) {
    printf("FORTH CANARY: nested outer interpret entered with an open definition (R4-E3 precondition violated)\n");
  }
  #endif
  forthOuterCtx_t *prevCtx = forthOuterCur;
  forthOuterCur = ctx;
  forthOuterDepth++;
  forthDefStateSave(&ctx->savedDef);
  ctx->savedLatestClosed = forthLatestClosedRef;
  forthLatestClosedRef = FORTH_NULL;
  forthTokenizerInit();

  forthState_t state = STATE_INTERPRET;
  bool lineOK = true;
  char buf[FORTH_TOKEN_MAX + 1];

  while (lineOK && nextToken(buf)) {
    /* ---- C-4: ':' colon matches the ':' character (B2) ---- */
    if (compareString(buf, ":", CMP_BINARY) == 0) {
      if (mode == FORTH_OUTER_SKIP_DEFS) {
        /* SKIP_DEFS (D-2b): definition was compiled by the pre-scan — consume
         * ':' <name> ... ';' without touching the dictionary. */
        char name[FORTH_TOKEN_MAX + 1];
        if (!nextToken(name)) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        } else {
          bool closed = false;
          while (nextToken(buf)) {
            if (strcmp(buf, ";") == 0) { closed = true; break; }
          }
          if (!closed) {   /* defensive: pre-scan already errored such a step */
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            lineOK = false;
          } else {
            /* F3-4: resolve consumed name for tracker (pre-scan may have moved to gdict) */
            { uint16_t ref; uint8_t fl;
              if (forthFindColonRef(name, &ref, &fl)) {
                forthLatestClosedRef = ref;
              }
            }
          }
        }
        continue;
      }
      if (state == STATE_COMPILE) {
         abortDefinition();
         displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
         lineOK = false;
       } else {
         char name[FORTH_TOKEN_MAX + 1];
         if (!nextToken(name)) {
           displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
           lineOK = false;
        } else if (!startDefinition(name)) {
          lineOK = false;
        } else {
          state = STATE_COMPILE;
        }
      }
      continue;
    }

    /* ---- C-4: ';' ---- */
    if (strcmp(buf, ";") == 0) {
      if (state == STATE_INTERPRET) {
         if (mode == FORTH_OUTER_DEFS_ONLY) {
           continue;   /* stray ';' is an execution-time error, not a pre-scan one */
         }
         displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
         lineOK = false;
      } else {
        if (!finishDefinition()) {
          lineOK = false;
        }
        /* F3-4: set tracker for same-line marks (GLOBAL/IMMEDIATE) */
        if (lineOK) {
          forthLatestClosedRef = (uint16_t)(fdict.count - 1);
        }
        /* M3: state = INTERPRET unconditionally on ';' */
        state = STATE_INTERPRET;
      }
      continue;
    }

    /* F3-4: FORGET — structural, gdict-only */
    if (compareString(buf, "FORGET", CMP_BINARY) == 0) {
      char fname[FORTH_TOKEN_MAX + 1];
      if (state == STATE_COMPILE) {
        abortDefinition();
        displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
        continue;
      }
      if (!nextToken(fname)) {
        displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
        continue;
      }
      if (mode == FORTH_OUTER_DEFS_ONLY) {
        continue;                     /* behavior, not a mark: skipped in pre-scan */
      }
      if (forthInnerIsActive()) {
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
        continue;
      }
      if (!forthGDictForget(fname)) {
        lineOK = false;               /* error already displayed */
      }
      continue;
    }

    if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
      /* F3-4: allow GLOBAL/IMMEDIATE marks through the pre-scan so that
       * subsequent definitions on the same line see the mark immediately.
       * On the execution pass the marks re-apply as idempotent no-ops. */
      { uint16_t pidx = forthFindPrim(buf);
        if (pidx != FORTH_PRIM_NONE && (forthPrims[pidx].flags & FF_DEFMARK)) {
          forthPrims[pidx].fn();
          clearSystemFlag(FLAG_ASLIFT);
          if (lastErrorCode != ERROR_NONE) {
            lineOK = false;
          }
          continue;
        }
      }
      continue;   /* D-2a: pre-scan must not execute tail code */
    }

    /* ---- §4.1 step 1: primitive lookup ---- */
    {
      uint16_t idx = forthFindPrim(buf);
      if (idx != FORTH_PRIM_NONE) {
        if (state == STATE_COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE)) {
          if (!forthDictEmit((ftoken_t)(idx + FTOK_PRIM_BASE))) {
            abortDefinition();
            lineOK = false;
          }
        } else {
          forthPrims[idx].fn();
          clearSystemFlag(FLAG_ASLIFT);
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* ---- §4.1 step 2: colon-def lookup ---- */
    {
      uint16_t widx; uint8_t wflags;
      if (forthFindColonRef(buf, &widx, &wflags)) {
        if (state == STATE_COMPILE && !(wflags & FF_IMMEDIATE)) {
          if (!forthDictEmit(forthTokenFromRef(widx))) {
            abortDefinition();
            lineOK = false;
          }
        } else {
          forthInner(widx, programRunStop == PGM_RUNNING);
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* ---- §4.1 step 3: number (C-2: number BEFORE label, C-8 classify-gate) ---- */
    if (classifyNumber(buf) != FORTH_NUM_NONE) {
      if (!processNumber(buf, state == STATE_COMPILE)) {
        /* Classified as number but processing failed — do NOT fall through */
        if (isDefinitionOpen()) abortDefinition();
        lineOK = false;
        break;
      }
      if (lastErrorCode != ERROR_NONE) {
        if (isDefinitionOpen()) abortDefinition();
        lineOK = false;
        break;
      }
      continue;
    }

    /* ---- §4.1 step 4: C47 item (§4.1, forward lookup: CAT_FNCT + PTP_NONE only) ---- */
    {
      uint16_t itemId;
      if (forthFindItem(buf, &itemId)) {
        if (state == STATE_COMPILE) {
          if (!forthDictEmit(FTOK_C47)) {
            abortDefinition();
            lineOK = false;
          } else if (!forthDictEmit((ftoken_t)itemId)) {
            abortDefinition();
            lineOK = false;
          }
        } else {
          uint8_t savedRunStop = programRunStop;
          programRunStop = PGM_RUNNING;
          reallyRunFunction((int16_t)itemId, NOPARAM);
          if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* ---- §4.1 step 5: C47 label (§3.3.6, C-1) ---- */
    {
      /* GLOBAL_LABELS (upstream rebase to b8f79e486): see forth_dict.c's
       * forthResolveXEQ for the same note — Forth's bare-name label lookup
       * has only ever meant global labels. */
      calcRegister_t label = findNamedLabel(buf, GLOBAL_LABELS);
      if (label != INVALID_VARIABLE) {
         if (state == STATE_COMPILE) {
           displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
           abortDefinition();
           lineOK = false;
         } else {
           /* C-1 amendment (proposed, see PROPOSED_SPEC_CHANGES.md): dispatch
            * via fnExecute directly, NOT reallyRunFunction under a forced
            * PGM_RUNNING wrap. ITM_XEQ is unlike ordinary items: under
            * PGM_RUNNING, fnExecute only pushes a subroutine level and defers
            * stepping to an enclosing runProgram loop — from an interactive
            * Forth line no such loop exists, so the program never ran and the
            * level leaked 3 blocks per call (found by T3.5). The §2.2
            * livelock lives in items.c's normal-mode dispatch
            * (refreshStatusBar pump), which a direct fnExecute call bypasses.
            * Interactively this takes the same fnGoto+runProgram path as a
            * keyboard XEQ and fires §9.3 bump site A (a run start must bump —
            * the old wrap wrongly suppressed it); from a program-context
            * Forth step (programRunStop == PGM_RUNNING) the nested branch is
            * taken unchanged (continuation semantics, level popped by RTN).
            * dynamicMenuItem must be cleared FIRST: fnGoto's
            * dynamicMenuItem >= 0 branch reinterprets the label ID as a
            * global step number (menu-launch semantics); leftover menu state
            * (e.g. 0 after the reset path shows MyMenu) sent goToGlobalStep
            * off the end of program memory. fnExecute itself resets it only
            * AFTER fnGoto — too late for a name-resolved, non-menu call. */
           dynamicMenuItem = -1;
           fnExecute((uint16_t)label);
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* ---- §4.1 last resort: undefined word ---- */
    {
      char defName[FORTH_TOKEN_MAX + 1];
      if (isDefinitionOpen() && openDefinitionName(defName, sizeof(defName))) {
        snprintf(errorMessage, ERROR_MESSAGE_LENGTH, "%s (in %s)", buf, defName);
      } else {
        snprintf(errorMessage, ERROR_MESSAGE_LENGTH, "%s", buf);
      }
    }
    displayCalcErrorMessage(ERROR_FUNCTION_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    if (isDefinitionOpen()) abortDefinition();
    lineOK = false;
  }

  /* ---- End of line ---- */
  if (state == STATE_COMPILE && isDefinitionOpen()) {
    /* C-4: truly unterminated — abort always; display INVALID_NAME only if
     * no prior error was shown (never mask e.g. ERROR_INPUT_TOO_LONG). */
    abortDefinition();
    if (lastErrorCode == ERROR_NONE) {
      displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
  } else if (lastErrorCode == ERROR_NONE) {
    /* C-7: ASLIFT-set gate checks lastErrorCode == ERROR_NONE */
    setSystemFlag(FLAG_ASLIFT);
  }

  forthCurrentScope = ctx->savedScope;
  forthDefStateRestore(&ctx->savedDef);
  forthLatestClosedRef = ctx->savedLatestClosed;
  forthOuterDepth--;
  forthOuterCur = prevCtx;
}

/* ---- Public wrapper: same signature as before ---- */
void forthOuterInterpret(const char *source)
{
  forthOuterCtx_t ctx;
  ctx.savedScope = forthCurrentScope;
  size_t n = strlen(source);
  if (n >= FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  memcpy(ctx.source, source, n + 1);
  forthOuterRun(&ctx, FORTH_OUTER_FULL);
}

/* fnForthOuter — ITM_FORTH entry point (§3.3.2) */
void fnForthOuter(uint16_t unused) {
  if (getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  int32_t len = stringByteLength(REGISTER_STRING_DATA(REGISTER_X));
  if (len + 1 > FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  forthOuterCtx_t ctx;
  ctx.savedScope = forthCurrentScope;
  xcopy(ctx.source, REGISTER_STRING_DATA(REGISTER_X), len + 1);
  fnDrop(NOPARAM);   /* copy MUST precede drop: drop invalidates the string */
  forthOuterRun(&ctx, FORTH_OUTER_FULL);
}

/* ---- P-2: Program-step entry point (§3.3.2, §9.2) ---- */

/* §9.2 Architecture 2: first-touch pre-scan of the owning program.
 * DEFS_ONLY-compiles every Forth source step so forward references from any
 * step's tail resolve. D-2: (a) no tail execution, (b) no recompile,
 * (c) owning program only. */
static void forthPreScanOwningProgram(const uint8_t *anyPtrInProgram)
{
  uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
  if (!progStart) {
    return;
  }
  if (forthScanIsRecorded(progStart)) {
    return;   /* first touch already done this generation */
  }

  /* Snapshot for rollback (R4-4 policy unchanged: base/sizeBlocks are
   * deliberately NOT restored). The record participates in the snapshot:
   * appended first, trimmed with everything else if the scan errors. */
  uint16_t scanHere   = fdict.here;
  uint16_t scanLatest = fdict.latest;
  uint16_t scanCount  = fdict.count;
  uint16_t scanHead   = forthScanHead;

  if (!forthScanRecord(progStart)) {
    /* Ordinary dictionary exhaustion (R4-E1): surface it, halt the step. */
    if (lastErrorCode == ERROR_NONE) {
      displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
    fdict.here   = scanHere;
    fdict.latest = scanLatest;
    fdict.count  = scanCount;
    forthScanHead = scanHead;
    return;
  }

  /* F3-3: set scope to the new record's offset for definition stamping */
  uint16_t savedScope = forthCurrentScope;
  forthCurrentScope = forthScanHead;

  uint8_t *nextStart = forthNextProgramStart(progStart);
  forthOuterCtx_t ctx;
  ctx.savedScope = forthCurrentScope;
  uint8_t *step = progStart;
  while (step && (nextStart == NULL || step < nextStart)) {
    uint8_t len;
    if (forthStepPayload(step, &len) && len > 0) {   /* markers (len==0) skipped */
      xcopy(ctx.source, step + 4, len);
      ctx.source[len] = 0;
      forthOuterRun(&ctx, FORTH_OUTER_DEFS_ONLY);
       if (lastErrorCode != ERROR_NONE) {
         /* Roll back this pre-scan's definitions AND its record. */
         fdict.here   = scanHere;
         fdict.latest = scanLatest;
         fdict.count  = scanCount;
         forthScanHead = scanHead;
         forthCurrentScope = savedScope;
         return;
       }
    }
    uint8_t *next = findNextStep(step);
    if (!next || next <= step) {
      break;      /* defensive, mirrors forthMarkerTurnsOn */
    }
    step = next;
  }
  /* Success: the record is already in place. */
  forthCurrentScope = savedScope;
}

/* F3-3A: single scope-entry primitive for every scope-sensitive step arm
 * (the ITM_FORTH source-step handler below; the XEQ/XEQP1 name fallback
 * in param_core.c).  Generation check + first-touch pre-scan, then select
 * the owning program's scope.  Returns the previous scope for
 * forthScopeRestore.  On pre-scan error the scope is left unchanged and
 * the caller halts its step. */
uint16_t forthScopeEnterProgramStep(const uint8_t *anyPtrInProgram)
{
  uint16_t prev = forthCurrentScope;
  forthRunGenCheckReset();
  forthPreScanOwningProgram(anyPtrInProgram);
  if (lastErrorCode != ERROR_NONE) {
    return prev;
  }
  {
    uint16_t recOff;
    uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
    if (progStart && forthScanFindRecord(progStart, &recOff)) {
      forthCurrentScope = recOff;
    } else {
      forthCurrentScope = FORTH_OWNER_INTERACTIVE;
    }
  }
  return prev;
}

void forthScopeRestore(uint16_t prev) { forthCurrentScope = prev; }

void forthProgramStep(const uint8_t *payload) {
  uint16_t prevScope = forthScopeEnterProgramStep(payload);
  if (lastErrorCode != ERROR_NONE) {
    return;                                 /* pre-scan error halts before executing this step */
  }
  forthOuterCtx_t ctx;
  ctx.savedScope = prevScope;
  uint8_t len = *payload;
  xcopy(ctx.source, payload + 1, len);
  ctx.source[len] = 0;
  forthOuterRun(&ctx, FORTH_OUTER_SKIP_DEFS);
}

/* Test-only: outer-interpreter nesting introspection (D-3) */
#ifdef FORTH_DEBUG_SELFTEST
void *forthTestOuterCur(void) { return (void *)forthOuterCur; }
uint8_t forthTestOuterDepth(void) { return forthOuterDepth; }
void forthTestSetOuterDepth(uint8_t d) { forthOuterDepth = d; }
#endif
