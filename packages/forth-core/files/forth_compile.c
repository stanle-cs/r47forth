/*
 * forth_compile.c -- Forth outer interpreter + tokenizer + number grammar
 * Per DESIGN.md §3.3.1 (state machine, C-4), §3.3.2 (source, C-5),
 * §3.3.3 (tokenizer, C-6), §3.3.5 (numbers, C-8), §3.3.6 (C47 labels, C-1)
 */

#include <string.h>

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"
#include "forth_capture.h"
#include "programming/param_core.h"

/* ---- §2.2 Token constants (mirror forth_inner.c) ---- */

#define FTOK_PRIM_BASE    0x0001
#define FTOK_CALL_BASE    0x1000
#define FTOK_LIT          0x7F00
#define FTOK_ILIT         0x7F01
#define FTOK_C47          0x7F04
#define FTOK_BR           0x7F02
#define FTOK_0BR          0x7F03

/* ---- §3.3.2 / D-3: per-invocation context; idle BSS = one ptr + 2 bytes ---- */
typedef enum {
  FORTH_OUTER_FULL      = 0,  /* compile and execute (interactive semantics) */
  FORTH_OUTER_DEFS_ONLY = 1,  /* pre-scan: compile definitions, skip ALL interpret-state tokens */
  FORTH_OUTER_SKIP_DEFS = 2,  /* step execution: skip ':'..';' regions, execute the rest */
  FORTH_OUTER_CHECK   = 3     /* grammar check: side-effect-free validation */
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

/* ---- F3-5: control stack for compile-time branching ---- */

#define FORTH_CSTACK_DEPTH 8
#define CTL_ORIG 1
#define CTL_DEST 2
typedef struct { uint16_t pos; uint8_t kind; } forthCtl_t;
static forthCtl_t forthCstack[FORTH_CSTACK_DEPTH];
static uint8_t forthCsp = 0;

/* F5-1: PRIM_* indices from forth_prims.c (append-frozen, not exported) */
#define PRIM_RECURSE   11
#define PRIM_GLOBAL    12
#define PRIM_IMMEDIATE 13
#define PRIM_IF        14
#define PRIM_ELSE      15
#define PRIM_THEN      16
#define PRIM_BEGIN     17
#define PRIM_UNTIL     18
#define PRIM_AGAIN     19
#define PRIM_WHILE     20
#define PRIM_REPEAT    21

static bool ctlPush(uint16_t pos, uint8_t kind) {
  if (forthCsp >= FORTH_CSTACK_DEPTH) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  forthCstack[forthCsp].pos = pos;
  forthCstack[forthCsp].kind = kind;
  forthCsp++;
  return true;
}
static bool ctlPop(uint8_t kind, uint16_t *pos) {
  if (forthCsp == 0 || forthCstack[forthCsp - 1].kind != kind) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  forthCsp--;
  *pos = forthCstack[forthCsp].pos;
  return true;
}
static bool ctlEmitBranch(ftoken_t brTok, uint16_t *deltaPosOut) {
  if (!forthDictEmit(brTok)) return false;
  *deltaPosOut = fdict.here;
  return forthDictEmit((ftoken_t)0);
}
static void ctlPatchTo(uint16_t deltaPos, uint16_t target) {
  int16_t delta = (int16_t)(((int32_t)target - (int32_t)(deltaPos + 2)) / 2);
  memcpy(fdict.base + deltaPos, &delta, 2);
}
static bool ctlEmitBack(ftoken_t brTok, uint16_t dest) {
  if (!forthDictEmit(brTok)) return false;
  {
    uint16_t deltaPos = fdict.here;
    int16_t delta = (int16_t)(((int32_t)dest - (int32_t)(deltaPos + 2)) / 2);
    return forthDictEmit((ftoken_t)(uint16_t)delta);
  }
}

#define CTL_GUARD() do { if (!isDefinitionOpen()) { \
  displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE); \
  return; } } while (0)

void forthCtlIf(void)    { CTL_GUARD(); uint16_t p;
                            if (!ctlEmitBranch(FTOK_0BR, &p)) return;
                            ctlPush(p, CTL_ORIG); }
void forthCtlThen(void)  { CTL_GUARD(); uint16_t p;
                            if (!ctlPop(CTL_ORIG, &p)) return;
                            ctlPatchTo(p, fdict.here); }
void forthCtlElse(void)  { CTL_GUARD(); uint16_t p1, p2;
                            if (!ctlPop(CTL_ORIG, &p1)) return;
                            if (!ctlEmitBranch(FTOK_BR, &p2)) return;
                            ctlPatchTo(p1, fdict.here);
                            ctlPush(p2, CTL_ORIG); }
void forthCtlBegin(void) { CTL_GUARD(); ctlPush(fdict.here, CTL_DEST); }
void forthCtlUntil(void) { CTL_GUARD(); uint16_t d;
                            if (!ctlPop(CTL_DEST, &d)) return;
                            ctlEmitBack(FTOK_0BR, d); }
void forthCtlAgain(void) { CTL_GUARD(); uint16_t d;
                            if (!ctlPop(CTL_DEST, &d)) return;
                            ctlEmitBack(FTOK_BR, d); }
void forthCtlWhile(void) { CTL_GUARD(); uint16_t d, p;
                            if (!ctlPop(CTL_DEST, &d)) return;
                            if (!ctlEmitBranch(FTOK_0BR, &p)) return;
                            if (!ctlPush(p, CTL_ORIG)) return;
                            ctlPush(d, CTL_DEST); }
void forthCtlRepeat(void){ CTL_GUARD(); uint16_t d, o;
                            if (!ctlPop(CTL_DEST, &d)) return;
                            if (!ctlPop(CTL_ORIG, &o)) return;
                            if (!ctlEmitBack(FTOK_BR, d)) return;
                            ctlPatchTo(o, fdict.here); }

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
#if defined(FORTH_DEBUG_SELFTEST)
void forthTestScopeSet(uint16_t scope) { forthCurrentScope = scope; }
#endif

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

/* §10.4 F4-1: Decimal, unsigned, leading-zero-insensitive (TAM parity).
 * Returns false on any non-digit byte, empty token, or accumulated value
 * above TAM_MAX_MASK. */
static bool parseParamDigits(const char *tok, uint16_t *out)
{
  uint32_t v = 0;
  int16_t i = 0;
  if (tok[0] == 0) return false;
  for (i = 0; tok[i] != 0; i++) {
    if (tok[i] < '0' || tok[i] > '9') return false;
    v = v * 10u + (uint32_t)(tok[i] - '0');
    if (v > TAM_MAX_MASK) return false;
  }
  *out = (uint16_t)v;
  return true;
}

/* F4-3: indirection prefix — the two-byte glyph STD_RIGHT_ARROW only.
 * No ASCII "->" and no IND spelling (V4): the typeable surface is exactly
 * the arrow glyph and the ASCII quote. Returns the remainder or NULL. */
static const char *checkArrowPrefix(const char *tok)
{
  if ((uint8_t)tok[0] == 0xa1 && (uint8_t)tok[1] == 0x92) return tok + 2;
  return NULL;
}

/* FIX-7: quote delimiters are ASCII 0x27 (the typeable canonical spelling,
 * V4) OR the directional glyph pair STD_LEFT/RIGHT_SINGLE_QUOTE
 * (\xa0\x18 / \xa0\x19) that decodeOneStep renders and the F6-4 fold
 * writes into capture lines — before this, the fold emitted committed
 * source its own compiler refused (D-C1). Open/close are matched
 * independently (decode always emits the glyph pair, typing always
 * produces 0x27; a mixed pair is harmless). Content bytes pass through
 * raw either way; a mid-token right-glyph stays content — only the LAST
 * glyph closes. No number-grammar collision: any byte >= 0x80 already
 * disqualifies a token as a number (classifyNumber). */
static uint8_t quoteOpenLen(const char *tok)   /* 0 = not an opening quote */
{
  if (tok[0] == 0x27) return 1;
  if ((uint8_t)tok[0] == 0xa0 && (uint8_t)tok[1] == 0x18) return 2;
  return 0;
}
static uint8_t quoteCloseLen(const char *tok)  /* close AND last glyph, else 0 */
{
  if (tok[0] == 0x27 && tok[1] == 0) return 1;
  if ((uint8_t)tok[0] == 0xa0 && (uint8_t)tok[1] == 0x19 && tok[2] == 0) return 2;
  return 0;
}

/* F4-3: 'NAME' — delimiters per quoteOpenLen/quoteCloseLen (FIX-7); the
 * closing quote must be the LAST GLYPH (a two-byte glyph's second byte that
 * equals 0x27 is not a close).  Twin of forthParseXeqForm's quote arm
 * (F3-6); kept separate so the landed XEQ parser stays untouched. */
static bool parseQuotedName(const char *tok, char *name, uint8_t *lenOut)
{
  uint8_t len = 0;
  uint8_t open = quoteOpenLen(tok);
  bool closed = false;
  if (open == 0) return false;
  tok += open;
  while (*tok) {
    if (quoteCloseLen(tok)) {
      closed = true;
      break;
    }
    if (*tok == 0x27) return false;  /* mid-token ASCII quote: malformed (unchanged) */
    if (len >= FORTH_NAME_MAX) return false;
    name[len++] = *tok++;
  }
  if (!closed || len == 0) return false;
  name[len] = 0;                 /* callers compare it as a C string */
  *lenOut = len;
  return true;
}

/* F4-2: KS letter table (26 entries) — uppercase only (native itemSoftmenuName parity).
 * Note: W (224) parses natively but dispatches as a silent no-op for flags;
 * this quirk is parity, do not fix it. */
static const struct { char c; uint8_t ks; } paramLetterKS[26] = {
  {'X',100},{'Y',101},{'Z',102},{'T',103},{'A',104},{'B',105},
  {'C',106},{'D',107},{'L',108},{'I',109},{'J',110},{'K',111},
  {'M',211},{'N',212},{'P',213},{'Q',214},{'R',215},{'S',216},
  {'E',217},{'F',218},{'G',219},{'H',220},{'O',221},{'U',222},
  {'V',223},{'W',224},
};

static bool paramLetterToKS(const char *tok, uint8_t *ks)
{
  int i;
  if (tok[0] == 0 || tok[1] != 0) return false;
  for (i = 0; i < 26; i++) {
    if (paramLetterKS[i].c == tok[0]) { *ks = paramLetterKS[i].ks; return true; }
  }
  return false;
}

/* F4-3: the F4-2 direct register shapes (NN, .NN, letter), reused as the
 * target of the indirection arrow. */
static bool parseDirectRegisterKS(const char *tok, uint8_t *ks)
{
  uint16_t v;
  if (parseParamDigits(tok, &v)) {
    if (v > 99) return false;
    *ks = (uint8_t)v;
    return true;
  }
  if (tok[0] == '.' && parseParamDigits(tok + 1, &v)) {
    if (v > 98) return false;
    *ks = (uint8_t)(112 + v);
    return true;
  }
  return paramLetterToKS(tok, ks);
}

/* F4-3: system-flag reverse map — b = 0..63 over indexOfItems[b + SFL_TDM24],
 * b = 64..127 over indexOfItems[(b & 0x3f) + SFL_MONIT] (the same two ranges
 * the native PARAM_FLAG arm decodes). compareString returns 0 on equal. */
static bool parseSystemFlagName(const char *name, uint8_t *idx)
{
  uint16_t b;
  for (b = 0; b < 64; b++) {
    if (compareString(indexOfItems[b + SFL_TDM24].itemSoftmenuName, name, CMP_BINARY) == 0) {
      *idx = (uint8_t)b;
      return true;
    }
  }
  for (b = 64; b < 128; b++) {
    if (compareString(indexOfItems[(b & 0x3f) + SFL_MONIT].itemSoftmenuName, name, CMP_BINARY) == 0) {
      *idx = (uint8_t)b;
      return true;
    }
  }
  return false;
}

/* F4-3: parse the marker parameter forms `ptpClass` accepts — 'NAME' (253),
 * system-flag names (250), →register (254), →'NAME' (255). Legality comes
 * from forthParamMarkerMask, the one table the runtime decode and the
 * validator walks also read. Fills nbuf[0..*used-1]; false = not a marker
 * form for this class (the caller raises ERROR_INVALID_NAME). */
static bool parseMarkerForm(const char *tok, uint16_t ptpClass,
                            uint8_t *nbuf, uint16_t *used)
{
  uint8_t mask = forthParamMarkerMask(ptpClass);
  char qname[FORTH_NAME_MAX + 1];
  uint8_t qlen, ks, sfIdx;
  const char *rem;

  if (mask == 0) return false;              /* PTP_NUMBER_16: excluded */
  rem = checkArrowPrefix(tok);
  if (rem) {
    if ((mask & FORTH_MK_IND_VAR) && parseQuotedName(rem, qname, &qlen)) {
      nbuf[0] = INDIRECT_VARIABLE;
      nbuf[1] = qlen;
      memcpy(nbuf + 2, qname, qlen);
      *used = (uint16_t)(2 + qlen);
      return true;
    }
    if ((mask & FORTH_MK_IND_REG) && parseDirectRegisterKS(rem, &ks)) {
      nbuf[0] = INDIRECT_REGISTER;
      nbuf[1] = ks;
      *used = 2;
      return true;
    }
    return false;
  }
  if (!parseQuotedName(tok, qname, &qlen)) return false;
  if (mask & FORTH_MK_NAME) {               /* REGISTER, MENU: 'NAME' */
    nbuf[0] = STRING_LABEL_VARIABLE;
    nbuf[1] = qlen;
    memcpy(nbuf + 2, qname, qlen);
    *used = (uint16_t)(2 + qlen);
    return true;
  }
  if ((mask & FORTH_MK_SYSFLAG) && parseSystemFlagName(qname, &sfIdx)) {
    nbuf[0] = SYSTEM_FLAG_NUMBER;
    nbuf[1] = sfIdx;
    *used = 2;
    return true;
  }
  return false;
}

/* F4-3: land a parsed marker form. Compile: FTOK_C47 + itemId + the bytes
 * zero-padded to whole cells. Interpret: the ONE bounded-core dispatch body
 * (forthParamMarkerDispatch), so compiled and interpreted forms cannot
 * drift. False = error already displayed and the definition aborted. */
static bool emitOrRunMarkerForm(bool compiling, uint16_t itemId, uint16_t ptpClass,
                                uint8_t *nbuf, uint16_t used)
{
  if (compiling) {
    uint16_t padded = (uint16_t)((used + 1) & ~1u);
    if (padded > used) nbuf[used] = 0;
    if (!forthDictEmit(FTOK_C47) ||
        !forthDictEmit((ftoken_t)itemId) ||
        !forthDictEmitBytes(nbuf, padded)) {
      abortDefinition();
      return false;
    }
    return true;
  }
  forthParamMarkerDispatch(itemId, ptpClass, nbuf, used);
  forthDataDepthResync();   /* native item: resync the count (D2) */
  if (lastErrorCode != ERROR_NONE) {
    if (isDefinitionOpen()) abortDefinition();
    return false;
  }
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
static bool forthParseXeqForm(const char *, uint8_t *, char *, uint8_t *);
static bool emitXeqn(uint8_t, const char *, uint8_t);
static void forthOuterRunInner(forthOuterCtx_t *ctx, forthOuterMode_t mode) {
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

  const bool checking = (mode == FORTH_OUTER_CHECK);

  /* F5-1: simulation state — live only in check mode */
  bool simOpen = false;
  bool simClosedThisLine = false;
  uint8_t simCsp = 0;
  uint8_t simKind[FORTH_CSTACK_DEPTH];

  forthState_t state = STATE_INTERPRET;
  bool lineOK = true;
  char buf[FORTH_TOKEN_MAX + 1];

  while (lineOK && nextToken(buf)) {
    bool colonHit = false;   /* F5-1: for §2(f) number suppression in check mode */
    /* ---- C-4: ':' colon matches the ':' character (B2) ---- */
    if (compareString(buf, ":", CMP_BINARY) == 0) {
       /* F5-1: check mode — structural simulation, no dictionary touch */
       if (checking) {
         if (simOpen) {
           displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
           lineOK = false;
         } else {
           char name[FORTH_TOKEN_MAX + 1];
           if (!nextToken(name)) {
             displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
             lineOK = false;
           } else {
             uint8_t nlen = (uint8_t)strlen(name);
             if (nlen == 0 || nlen > FORTH_NAME_MAX) {
               displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
               lineOK = false;
             } else {
               simOpen = true;
               simCsp = 0;
             }
           }
         }
         continue;
       }
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
            forthCsp = 0;
          }
       }
       continue;
     }

      /* ---- C-4: ';' ---- */
      if (strcmp(buf, ";") == 0) {
        /* F5-1: check mode — structural close simulation */
        if (checking) {
          if (!simOpen) {
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            lineOK = false;
          } else if (simCsp != 0) {
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            lineOK = false;
          } else {
            simOpen = false;
            simClosedThisLine = true;
          }
          continue;
        }
        if (state == STATE_INTERPRET) {
          if (mode == FORTH_OUTER_DEFS_ONLY) {
            continue;   /* stray ';' is an execution-time error, not a pre-scan one */
          }
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        } else {
          if (forthCsp != 0) {
            abortDefinition();
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            forthCsp = 0;
            lineOK = false;
          } else if (!finishDefinition()) {
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
      /* F5-1: check mode — consume name, no gdict consult (tier 2) */
      if (checking) {
        if (simOpen) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        } else {
          char fname[FORTH_TOKEN_MAX + 1];
          if (!nextToken(fname)) {
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            lineOK = false;
          }
          /* else: name consumed, continue (tier 2 — no gdict consult) */
        }
        continue;
      }
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

    /* F3-6: XEQ — structural, deliberately unshadowable */
    if (compareString(buf, "XEQ", CMP_BINARY) == 0) {
      /* F5-1: check mode — parse form, no emit/dispatch (tier 2) */
      if (checking) {
        char xtok[FORTH_TOKEN_MAX + 1];
        char xname[FORTH_NAME_MAX + 1];
        uint8_t xkind, xlen;
        if (!nextToken(xtok)) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        } else if (!forthParseXeqForm(xtok, &xkind, xname, &xlen)) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        }
        /* else: parsed OK, continue (no emit, no dispatch — resolution is tier 2) */
        continue;
      }
      char xtok[FORTH_TOKEN_MAX + 1];
      char xname[FORTH_NAME_MAX + 1];
      uint8_t xkind, xlen;
      if (!nextToken(xtok)) {
        if (isDefinitionOpen()) abortDefinition();
        displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
        continue;
      }
      if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
        continue;                     /* tail XEQ is execution, not a mark */
      }
      if (!forthParseXeqForm(xtok, &xkind, xname, &xlen)) {
        if (isDefinitionOpen()) abortDefinition();
        displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;               /* B3: only the canonical spellings exist */
        continue;
      }
      if (state == STATE_COMPILE) {
        if (!emitXeqn(xkind, xname, xlen)) {
          abortDefinition();
          lineOK = false;
        }
      } else {
        uint16_t colonRef;
        forthXeqnResult_t r = forthXeqnDispatch(xname, xkind, &colonRef);
        if (r == FORTH_XEQN_COLON) {
          forthInner(colonRef, programRunStop == PGM_RUNNING);
        }
        if (lastErrorCode != ERROR_NONE) {
          lineOK = false;
        }
      }
      continue;
    }

    if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
      /* F3-4: allow GLOBAL/IMMEDIATE marks through the pre-scan so that
       * subsequent definitions on the same line see the mark immediately.
       * On the execution pass the marks re-apply as idempotent no-ops. */
      { uint16_t pidx = forthFindPrim(buf);
        if (pidx != FORTH_PRIM_NONE && (forthPrims[pidx].flags & FF_DEFMARK)) {
          if (!forthPrimInvoke(pidx)) {
            lineOK = false;
          }
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
        /* F5-1: check mode — control-structure simulation, no emit/execute */
        if (checking) {
          bool stop = false;
          if (idx >= PRIM_IF && idx <= PRIM_REPEAT) {
            /* Control prim */
            if (!simOpen) {
              displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              stop = true;
            } else {
              switch (idx) {
              case PRIM_IF:
                if (simCsp >= FORTH_CSTACK_DEPTH) {
                  displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simKind[simCsp] = CTL_ORIG;
                  simCsp++;
                }
                break;
              case PRIM_THEN:
                if (simCsp == 0 || simKind[simCsp - 1] != CTL_ORIG) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simCsp--;
                }
                break;
              case PRIM_ELSE:
                if (simCsp == 0 || simKind[simCsp - 1] != CTL_ORIG) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else if (simCsp >= FORTH_CSTACK_DEPTH) {
                  displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simCsp--;
                  simKind[simCsp] = CTL_ORIG;
                  simCsp++;
                }
                break;
              case PRIM_BEGIN:
                if (simCsp >= FORTH_CSTACK_DEPTH) {
                  displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simKind[simCsp] = CTL_DEST;
                  simCsp++;
                }
                break;
              case PRIM_UNTIL:
              case PRIM_AGAIN:
                if (simCsp == 0 || simKind[simCsp - 1] != CTL_DEST) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simCsp--;
                }
                break;
              case PRIM_WHILE:
                if (simCsp == 0 || simKind[simCsp - 1] != CTL_DEST) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else if (simCsp + 1 > FORTH_CSTACK_DEPTH) {
                  displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simCsp--;
                  simKind[simCsp] = CTL_ORIG;
                  simCsp++;
                  simKind[simCsp] = CTL_DEST;
                  simCsp++;
                }
                break;
              case PRIM_REPEAT:
                if (simCsp < 2) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else if (simKind[simCsp - 1] != CTL_DEST || simKind[simCsp - 2] != CTL_ORIG) {
                  displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                  stop = true;
                } else {
                  simCsp -= 2;
                }
                break;
            }
            }
          } else if (idx == PRIM_RECURSE) {
            if (!simOpen) {
              displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              stop = true;
            }
          } else if (idx == PRIM_GLOBAL || idx == PRIM_IMMEDIATE) {
            if (!simClosedThisLine) {
              displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              stop = true;
            }
          }
          /* every other prim: continue (legal in both states; nothing runs) */
          if (stop) lineOK = false;
          continue;
        }
        if (state == STATE_COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE)) {
          if (!forthDictEmit((ftoken_t)(idx + FTOK_PRIM_BASE))) {
            abortDefinition();
            lineOK = false;
          }
        } else if (!forthPrimInvoke(idx)) {
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
        }
        if (lastErrorCode != ERROR_NONE) {
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
        }
        continue;
      }
    }

    /* ---- §4.1 step 2: colon-def lookup ---- */
    {
      uint16_t widx; uint8_t wflags;
      colonHit = forthFindColonRef(buf, &widx, &wflags);
      if (checking) {
        /* F5-1: check mode — continue on hit or miss (tier 2), but colonHit
         * is needed by the number branch for §2(f) suppression */
        /* Fall through to number branch for suppression check */
      } else if (colonHit) {
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
      /* colonHit not set and not checking: fall through to number */
    }

    /* ---- §4.1 step 3: number (C-2: number BEFORE label, C-8 classify-gate) ---- */
    /* F5-1: check mode — parse into locals, no push/emit; suppressed by colonHit (§2f) */
    if (checking) {
      forthNumType_t numType = classifyNumber(buf);
      if (numType != FORTH_NUM_NONE && !colonHit) {
        /* Parse into locals — no push, no emit */
        bool numOK = false;
        if (numType == FORTH_NUM_INT) {
          int32_t v;
          if (parseNumberAsInt32(buf, &v)) {
            numOK = true;
          } else {
            /* Out of range int -> real34 fallback */
            real34_t r;
            if (parseNumberAsReal34(buf, &r)) {
              numOK = true;
            }
          }
        } else if (numType == FORTH_NUM_REAL) {
          real34_t r;
          if (parseNumberAsReal34(buf, &r)) {
            numOK = true;
          }
        }
        if (!numOK) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          lineOK = false;
        }
      }
      /* Success or suppressed -> continue */
    } else {
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
    }

    /* F5-1: check mode — item, parameterized-item, and label branches are
     * SKIPPED ENTIRELY (tier 2). The parameterized-item's parameter token is
     * NOT consumed: an unshadowed run would consume it, a shadowed run would
     * not; consuming in check could mask a tier-1 violation in the next token. */
    if (checking)
      continue;

    /* ---- §4.1 step 4: C47 item (§4.1, forward lookup: CAT_FNCT + PTP_NONE only) ---- */
    {
      uint16_t itemId;
      if (forthFindItem(buf, &itemId)) {
        if (forthItemIsFlowReject(itemId)) {
          displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
          continue;
        }
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
          forthDataDepthResync();   /* native item: resync the count (D2) */
          if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* F4-1: Series-C parameter-grammar entry (replaces F3-6 blanket reject).
     * DEFS_ONLY: in a tail, the interpret-state gate already skipped the item
     * token before this arm; inside a definition the pre-scan compiles normally. */
    {
      uint16_t paramItemId;
      if (forthFindItemParameterized(buf, &paramItemId)) {
        if (forthItemIsFlowReject(paramItemId)) {
          displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
          continue;
        }
        char ptok[FORTH_TOKEN_MAX + 1] = {0};
        if (!nextToken(ptok)) {
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
          continue;
        }
        uint16_t ptpClass = (uint16_t)(indexOfItems[paramItemId].status & PTP_STATUS);
        if (ptpClass == PTP_NUMBER_8 || ptpClass == PTP_NUMBER_16 || ptpClass == PTP_NUMBER_8_16) {
          uint16_t value;
          if (!parseParamDigits(ptok, &value)) {
            /* F4-3: not digits — the indirection forms are the only other
             * shape these classes take (NUMBER_16 has an empty mask). */
            uint8_t mbuf[2 + FORTH_NAME_MAX + 1];
            uint16_t mused;
            if (parseMarkerForm(ptok, ptpClass, mbuf, &mused)) {
              if (!emitOrRunMarkerForm(state == STATE_COMPILE, paramItemId,
                                       ptpClass, mbuf, mused)) {
                lineOK = false;
              }
              continue;
            }
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          }
          uint16_t min = (uint16_t)(indexOfItems[paramItemId].tamMinMax >> TAM_MAX_BITS);
          uint16_t max = (uint16_t)(indexOfItems[paramItemId].tamMinMax & TAM_MAX_MASK);
          if (value < min || value > max) {
            displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          }
          if (state == STATE_COMPILE) {
            if (!forthDictEmit(FTOK_C47)) {
              abortDefinition();
              lineOK = false;
              continue;
            }
            if (!forthDictEmit((ftoken_t)paramItemId)) {
              abortDefinition();
              lineOK = false;
              continue;
            }
            if (ptpClass == PTP_NUMBER_8) {
              if (value > 0xFF) {
                displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                abortDefinition();
                lineOK = false;
                continue;
              }
              if (!forthDictEmit((ftoken_t)value)) {
                abortDefinition();
                lineOK = false;
                continue;
              }
            } else if (ptpClass == PTP_NUMBER_16) {
              if (!forthDictEmit((ftoken_t)value)) {
                abortDefinition();
                lineOK = false;
                continue;
              }
            } else {
              /* PTP_NUMBER_8_16: value <= 249 -> [value][0]; 250..505 -> [250][value-250] */
              if (value <= 249) {
                if (!forthDictEmit((ftoken_t)value)) {
                  abortDefinition();
                  lineOK = false;
                  continue;
                }
              } else {
                uint16_t extCell = (uint16_t)(250 | ((uint16_t)(value - 250) << 8));
                if (!forthDictEmit((ftoken_t)extCell)) {
                  abortDefinition();
                  lineOK = false;
                  continue;
                }
              }
            }
          } else {
            if (paramCoreValidateDirect(paramItemId, ptpClass, value)) {
              uint8_t savedRunStop = programRunStop;
              programRunStop = PGM_RUNNING;
              paramCoreDispatchDirect(paramItemId, ptpClass, value);
              forthDataDepthResync();   /* native item: resync the count (D2) */
              if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
            }
            if (lastErrorCode != ERROR_NONE) {
              if (isDefinitionOpen()) abortDefinition();
              lineOK = false;
              continue;
            }
          }
          continue;
        } else if (ptpClass == PTP_REGISTER) {
          /* F4-2: register direct forms — number, dot, letter */
          uint16_t regValue;
          uint8_t regKS;
          if (parseParamDigits(ptok, &regValue) && regValue <= 99) {
            regKS = (uint8_t)regValue;
          } else if (ptok[0] == '.' && parseParamDigits(ptok + 1, &regValue) && regValue <= 98) {
            regKS = 112 + (uint8_t)regValue;
          } else if (ptok[0] >= '0' && ptok[0] <= '9' && parseParamDigits(ptok, &regValue)) {
            /* all digits but > 99 */
            displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          } else if (paramLetterToKS(ptok, &regKS)) {
            /* single letter */
          } else {
            /* F4-3: named, system-flag, and indirect forms */
            uint8_t mbuf[2 + FORTH_NAME_MAX + 1];
            uint16_t mused;
            if (parseMarkerForm(ptok, ptpClass, mbuf, &mused)) {
              if (!emitOrRunMarkerForm(state == STATE_COMPILE, paramItemId,
                                       ptpClass, mbuf, mused)) {
                lineOK = false;
              }
              continue;
            }
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          }
          if (state == STATE_COMPILE) {
            if (!forthDictEmit(FTOK_C47)) { abortDefinition(); lineOK = false; continue; }
            if (!forthDictEmit((ftoken_t)paramItemId)) { abortDefinition(); lineOK = false; continue; }
            if (!forthDictEmit((ftoken_t)regKS)) { abortDefinition(); lineOK = false; continue; }
          } else {
            if (paramCoreValidateDirect(paramItemId, ptpClass, regKS)) {
              uint8_t savedRunStop = programRunStop;
              programRunStop = PGM_RUNNING;
              paramCoreDispatchDirect(paramItemId, ptpClass, regKS);
              forthDataDepthResync();   /* native item: resync the count (D2) */
              if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
            }
            if (lastErrorCode != ERROR_NONE) {
              if (isDefinitionOpen()) abortDefinition();
              lineOK = false;
              continue;
            }
          }
          continue;
        } else if (ptpClass == PTP_FLAG) {
          /* F4-2: flag direct forms — number, dot, letter */
          uint16_t flagValue;
          uint8_t flagByte;
          if (parseParamDigits(ptok, &flagValue) && flagValue <= 99) {
            flagByte = (uint8_t)flagValue;
          } else if (ptok[0] == '.' && parseParamDigits(ptok + 1, &flagValue) && flagValue <= 31) {
            flagByte = 112 + (uint8_t)flagValue;
          } else if (ptok[0] == '.' && parseParamDigits(ptok + 1, &flagValue) && flagValue <= 98) {
            displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          } else if (ptok[0] >= '0' && ptok[0] <= '9' && parseParamDigits(ptok, &flagValue)) {
            displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          } else if (paramLetterToKS(ptok, &flagByte)) {
            /* single letter — quirk: W (224) parses natively but dispatches as no-op */
          } else {
            /* F4-3: named, system-flag, and indirect forms */
            uint8_t mbuf[2 + FORTH_NAME_MAX + 1];
            uint16_t mused;
            if (parseMarkerForm(ptok, ptpClass, mbuf, &mused)) {
              if (!emitOrRunMarkerForm(state == STATE_COMPILE, paramItemId,
                                       ptpClass, mbuf, mused)) {
                lineOK = false;
              }
              continue;
            }
            displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
            continue;
          }
          if (state == STATE_COMPILE) {
            if (!forthDictEmit(FTOK_C47)) { abortDefinition(); lineOK = false; continue; }
            if (!forthDictEmit((ftoken_t)paramItemId)) { abortDefinition(); lineOK = false; continue; }
            if (!forthDictEmit((ftoken_t)flagByte)) { abortDefinition(); lineOK = false; continue; }
          } else {
            if (paramCoreValidateDirect(paramItemId, ptpClass, flagByte)) {
              uint8_t savedRunStop = programRunStop;
              programRunStop = PGM_RUNNING;
              paramCoreDispatchDirect(paramItemId, ptpClass, flagByte);
              forthDataDepthResync();   /* native item: resync the count (D2) */
              if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
            }
            if (lastErrorCode != ERROR_NONE) {
              if (isDefinitionOpen()) abortDefinition();
              lineOK = false;
              continue;
            }
          }
          continue;
        } else if (ptpClass == PTP_SHUFFLE) {
          /* F4-2: shuffle — exactly 4 lowercase chars from {x,y,z,t} */
          {
            const char *shuffleReg = "xyzt";
            uint8_t packed = 0, ci;
            int si;
            if (ptok[4] != 0) {
              displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
              if (isDefinitionOpen()) abortDefinition();
              lineOK = false;
              continue;
            }
            for (si = 0; si < 4; si++) {
              ci = 0;
              while (ci < 4 && shuffleReg[ci] != ptok[si]) ci++;
              if (ci == 4) {
                displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
                if (isDefinitionOpen()) abortDefinition();
                lineOK = false;
                goto shuffle_done;
              }
              packed |= (uint8_t)(ci << (si * 2));
            }
            if (state == STATE_COMPILE) {
              if (!forthDictEmit(FTOK_C47)) { abortDefinition(); lineOK = false; continue; }
              if (!forthDictEmit((ftoken_t)paramItemId)) { abortDefinition(); lineOK = false; continue; }
              if (!forthDictEmit((ftoken_t)packed)) { abortDefinition(); lineOK = false; continue; }
            } else {
              if (paramCoreValidateDirect(paramItemId, ptpClass, packed)) {
                uint8_t savedRunStop = programRunStop;
                programRunStop = PGM_RUNNING;
                paramCoreDispatchDirect(paramItemId, ptpClass, packed);
              forthDataDepthResync();   /* native item: resync the count (D2) */
                if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
              }
              if (lastErrorCode != ERROR_NONE) {
                if (isDefinitionOpen()) abortDefinition();
                lineOK = false;
                continue;
              }
            }
          }
          shuffle_done:
          continue;
        } else {
          /* F4-3: named, system-flag, and indirect forms (MENU and any other
           * marker-capable class); PTP_NUMBER_16 has an empty mask, so the
           * arrow there falls straight through to ERROR_INVALID_NAME. */
          {
            uint8_t mbuf[2 + FORTH_NAME_MAX + 1];
            uint16_t mused;
            if (parseMarkerForm(ptok, ptpClass, mbuf, &mused)) {
              if (!emitOrRunMarkerForm(state == STATE_COMPILE, paramItemId,
                                       ptpClass, mbuf, mused)) {
                lineOK = false;
              }
              continue;
            }
          }
          displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          if (isDefinitionOpen()) abortDefinition();
          lineOK = false;
          continue;
        }
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
             forthDataDepthResync();   /* R47 label body: resync the count (D2) */
            if (lastErrorCode != ERROR_NONE) {
             if (isDefinitionOpen()) abortDefinition();
             lineOK = false;
           }
         }
         continue;
       }
     }

    /* ---- §4.1 last resort: undefined word ---- */
    /* F5-1: check mode — unknown = tier 2, continue */
    if (!checking) {
      char defName[FORTH_TOKEN_MAX + 1];
      if (isDefinitionOpen() && openDefinitionName(defName, sizeof(defName))) {
        snprintf(errorMessage, ERROR_MESSAGE_LENGTH, "%s (in %s)", buf, defName);
      } else {
        snprintf(errorMessage, ERROR_MESSAGE_LENGTH, "%s", buf);
      }
    }
    if (!checking) {
      displayCalcErrorMessage(ERROR_FUNCTION_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
      if (isDefinitionOpen()) abortDefinition();
      lineOK = false;
    }
  }

  /* ---- End of line ---- */
  if (checking) {
    /* F5-1: check mode — simOpen means unterminated definition; no abort
     * (nothing was actually opened); NO ASLIFT write */
    if (simOpen && lastErrorCode == ERROR_NONE) {
      displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
  } else if (state == STATE_COMPILE && isDefinitionOpen()) {
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

/* D3-5: the depth/spill bracket lives at the single choke point so
 * EVERY outer execution accounts — fnForthOuter, CHECK, DEFS_ONLY,
 * SKIP_DEFS and the interpret wrapper alike. Nesting-aware: only the
 * outermost run brackets (a Forth line can XEQ into another line). */
static int16_t forthOuterRunNesting = 0;

static void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode)
{
  if (forthOuterRunNesting++ == 0) {
    forthDataDepthEnterOuter();
  }
  forthOuterRunInner(ctx, mode);
  if (--forthOuterRunNesting == 0) {
    forthDataDepthLeaveOuter();
  }
}

/* ---- F5-1: check mode public API ---- */

bool forthCheckSourceLine(const char *source)
{
  forthOuterCtx_t ctx;
  size_t n = strlen(source);
  if (n >= FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  memcpy(ctx.source, source, n + 1);
  /* forthOuterRun's epilogue restores forthCurrentScope FROM ctx.savedScope;
   * the prologue fills savedDef/savedLatestClosed but NOT this field — every
   * entry point must snapshot it, or the epilogue writes stack garbage into
   * the live scope. Latent while only the F5-1 test called check mode (it
   * never observed scope); poisoned the whole suite the moment F5-2 wired
   * check mode into pemAlpha's commit seam. */
  ctx.savedScope = forthCurrentScope;
  lastErrorCode = ERROR_NONE;
  forthOuterRun(&ctx, FORTH_OUTER_CHECK);
  return lastErrorCode == ERROR_NONE;
}

/* ---- F3-6: XEQ source-form helpers ---- */

/* 'NAME' -> kind 253; :NAME: -> kind 249.  The closing delimiter must be
 * the LAST GLYPH (a two-byte glyph whose second byte merely equals the
 * delimiter is not a close).  Name bytes pass through raw (C47 glyphs
 * legal; 0x20 is inexpressible in a token by construction).  FIX-7: the
 * quote spelling also accepts the directional glyph pair the F6-4 fold
 * emits (see quoteOpenLen/quoteCloseLen above); :NAME: is unchanged. */
static bool forthParseXeqForm(const char *tok, uint8_t *kind,
                              char *name, uint8_t *lenOut)
{
  int16_t len = (int16_t)strlen(tok);
  bool isColon = (tok[0] == ':');
  uint8_t open = isColon ? 1 : quoteOpenLen(tok);
  int16_t closeLen, last;
  if (open == 0 || len < open + 2) return false;
  {
    int16_t pp = 0;
    last = 0;
    while (tok[pp] != 0) { last = pp; pp = stringNextGlyph((char *)tok, pp); }
  }
  if (last < open) return false;             /* close must not be the open */
  if (isColon) {
    if (tok[last] != ':' || last != len - 1) return false;
    closeLen = 1;
  }
  else if (tok[last] == 0x27 && last == len - 1) {
    closeLen = 1;
  }
  else if ((uint8_t)tok[last] == 0xa0 && (uint8_t)tok[last + 1] == 0x19 &&
           last == len - 2) {
    closeLen = 2;
  }
  else {
    return false;
  }
  {
    int16_t nlen = len - open - closeLen;
    if (nlen < 1 || nlen > FORTH_NAME_MAX) return false;
    memcpy(name, tok + open, (size_t)nlen);
    name[nlen] = 0;
    *lenOut = (uint8_t)nlen;
  }
  *kind = isColon ? LOCAL_LABEL_VARIABLE : STRING_LABEL_VARIABLE;
  return true;
}

static bool emitXeqn(uint8_t kind, const char *name, uint8_t len)
{
  uint8_t buf[2 + FORTH_NAME_MAX + 1];
  uint16_t inlineBytes = (uint16_t)(2 + len);
  uint16_t padded = (uint16_t)((inlineBytes + 1) & ~1u);
    buf[0] = kind;
    buf[1] = len;
  memcpy(buf + 2, name, len);
  if (padded > inlineBytes) buf[inlineBytes] = 0;
  if (!forthDictEmit((ftoken_t)FTOK_XEQN)) return false;
  return forthDictEmitBytes(buf, padded);
}

/* ---- Public wrapper: same signature as before ---- */
void forthOuterInterpret(const char *source)
{
  lastErrorCode = ERROR_NONE;
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

/* L1-1 (C2): the shared "take the source line out of X" core.  Used by
 * fnForthOuter's C2a one-shot arm, fnForthOuter's interactive seed, and
 * forthTestRunFromX (rewritten below to call this so the two cannot drift).
 *
 * Returns false with the documented error displayed and X untouched when X
 * is not a string or the line is oversize.  On true, dst holds the
 * NUL-terminated line and X HAS BEEN DROPPED.  Copy MUST precede drop:
 * drop invalidates the string (§3.3.2). */
static bool_t forthTakeSourceFromX(char *dst) {
  if (getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  int32_t len = stringByteLength(REGISTER_STRING_DATA(REGISTER_X));
  if (len + 1 > FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  xcopy(dst, REGISTER_STRING_DATA(REGISTER_X), len + 1);
  fnDrop(NOPARAM);   /* copy MUST precede drop: drop invalidates the string */
  return true;
}

/* T9: calcModeAim's setup (src/c47/calcMode.c:62-92) WITHOUT its
 * liftStack().  An interactive Forth line operates on the LIVE stack — L-R2
 * already drops a seeded string precisely "so interpreted words see a clean
 * stack" — so a lifting open would replace X with a fresh uninitialised
 * dtReal34 before the line ever ran: "16 in X, type 1 +, ENTER" would
 * compute garbage + 1.  fnAim() cannot be used and calcModeAim cannot be
 * repaired after the fact (the repair is conditional on FLAG_ASLIFT, and
 * with it clear the old X is already freed).  Every line below is
 * calcModeAim's; the only intentional omission is the lift.  If calcModeAim
 * gains a statement upstream, this must gain it too — the rebase discipline
 * for this function is "diff it against calcModeAim".
 *
 * C2b verification against src/c47/calcMode.c:62-92 (reported, not silently
 * dropped, per the packet):
 *   - The PC_BUILD-only jm_show_comment() debug scaffold at the top of
 *     calcModeAim is omitted here: cosmetic GTK-debug output, irrelevant to
 *     DMCP/PC behaviour either way.
 *   - calcModeAim guards its calcMode/liftStack/cursor block on
 *     `!tam.mode && calcMode != CM_ASSIGN && calcMode != CM_PEM &&
 *     calcMode != CM_ASN_BROWSER`, and separately guards its
 *     `showSoftmenu(-MNU_ALPHA)` on `!tam.mode` alone.  Both are omitted
 *     below (unconditional instead).  Reachability check for fnForthOuter,
 *     the only caller: CM_PEM is intercepted before reallyRunFunction is
 *     ever reached, for both a direct FORTH keypress and a catalog pick —
 *     items.c's runFunction() (~:734-772) records the step and returns
 *     whenever `calcMode == CM_PEM`, and by the time that check runs,
 *     tam.mode already reads 0 on every path that gets there (either it was
 *     never set, or the enclosing keyboard.c branch — :1104-1300 — consumed
 *     the tam.mode!=0 case itself via addItemToBuffer/leaveTamModeIfEnabled
 *     before ever falling through to runFunction/reallyRunFunction).
 *     CM_ASSIGN never reaches runFunction for a function item either:
 *     keyboard.c's CM_ASSIGN arms (~:1044,:1049,:1314,:1373) capture the
 *     pressed item as `itemToBeAssigned` or route it through
 *     processAimInput, never runFunction.  CM_ASN_BROWSER is excluded
 *     outright: keyboard.c:940 gates the ENTIRE executeFunction dispatch
 *     block (including its terminal runFunction() call) on calcMode being
 *     none of CM_REGISTER_BROWSER/CM_FLAG_BROWSER/CM_ASN_BROWSER/
 *     CM_FONT_BROWSER.  So none of the four excluded states can co-occur
 *     with a live fnForthOuter call through today's dispatch (the
 *     PGM_RUNNING program-step path is separately guarded by C2a below, not
 *     by this function) — the guards are left out, matching the packet's
 *     literal text. This finding is dispatch-shaped and belongs to L1-3's
 *     surface, not this packet's; revisit if dispatch changes. */
static void forthEnterAimSurfaceNoLift(void) {
  alphaCase = CAPS_AIM_DEFAULT;
  nextChar  = NC_NORMAL;
  clearSystemFlag(FLAG_NUMLOCK);
  scrLock   = NC_NORMAL;

  calcMode = CM_AIM;
  /* NO liftStack() — T9 */
  clearRegisterLine(AIM_REGISTER_LINE, true, true);
  xCursor = 1;
  yCursor = Y_POSITION_OF_AIM_LINE + 6;
  cursorFont = &standardFont;
  cursorEnabled = true;

  showSoftmenu(-MNU_ALPHA);
  if (softmenuStack[0].softmenuId == 0) { softmenuStack[0].softmenuId = 1; }
  setSystemFlag(FLAG_ALPHA);
  calcModeAimGui();
}

/* fnForthOuter — ITM_FORTH entry point (§3.3.2).  L1-1 (L-R2): outside a
 * running program this now OPENS AN INTERACTIVE CAPTURE instead of
 * interpreting X.  ENTER semantics, the REPL loop and the dispatch divert
 * are L1-2/L1-3 — out of scope here. */
void fnForthOuter(uint16_t unused) {
  /* C2a: ITM_FORTH is PTP_REM (items.c:4771) and forthResolveXEQ deliberately
   * keeps resolving it, so a program step `XEQ 'FORTH'` reaches this
   * function while a program runs.  Unguarded, L-R2 would open an
   * interactive capture mid-run; preserve the pre-Stage-L one-shot
   * interpret-from-X behaviour for that case instead. */
  if (programRunStop == PGM_RUNNING) {
    forthOuterCtx_t ctx;
    ctx.savedScope = forthCurrentScope;
    if (!forthTakeSourceFromX(ctx.source)) { return; }
    forthOuterRun(&ctx, FORTH_OUTER_FULL);
    return;
  }

  bool_t seeded = false;
  char seed[FORTH_SOURCE_MAX];
  if (getRegisterDataType(REGISTER_X) == dtString) {
    if (!forthTakeSourceFromX(seed)) { return; }   /* oversize: error, NO capture */
    seeded = true;
  }

  if (catalog) {   /* T6: FIX-9 analog — drain a buried/on-top catalog menu */
    leaveAsmMode();
    for (int i = 0; i < SOFTMENU_STACK_SIZE; i++) {
      if (!(forthCatalogMenuOnTop() || forthCatalogBuriedOnStack())) break;
      popSoftmenu();
    }
  }

  forthEnterAimSurfaceNoLift();                   /* see above — NOT fnAim */
  forthCapOpenInteractive();                      /* clears aimBuffer; cannot fail */
  T_cursorPos = 0;
  displayAIMbufferoffset = 0;

  if (seeded) {
    xcopy(aimBuffer, seed, stringByteLength(seed) + 1);
    /* Empty-line guard, copied from the landed PEM idiom (manage.c:900-904):
     * an EMPTY string in X is a valid dtString and passes the size check, and
     * stringLastGlyph("") + 1 == 1 would put the cursor one past the NUL and
     * silently eat every keystroke. */
    T_cursorPos = (aimBuffer[0] == 0) ? 0 : stringLastGlyph(aimBuffer) + 1;
  }
}

#if defined(FORTH_DEBUG_SELFTEST)
/* L1-0: the battery's "interpret the string in X" entry.
 *
 * Until Stage L this WAS fnForthOuter: ITM_FORTH outside PEM required a
 * string in X, interpreted it, and consumed it.  L-R2 rules that FORTH
 * always opens an interactive capture instead, so the item entry stops
 * interpreting and the sites that drove it for its interpret semantics
 * need those semantics under their own name.
 *
 * L1-1: rewritten to call the shared forthTakeSourceFromX() core instead of
 * duplicating fnForthOuter's old body, so the two cannot drift.  Same two
 * error codes, same copy-before-drop ordering, same FORTH_OUTER_FULL run —
 * every existing assertion keeps its exact stack expectation.  Do not
 * "improve" it: forthOuterInterpret() is NOT a substitute (it never touches
 * X, so the drop these tests' stack expectations are written against would
 * not happen, and it clears lastErrorCode on entry where this does not).
 *
 * Self-test builds only; production never calls it. */
void forthTestRunFromX(uint16_t unusedButMandatoryParameter) {
  (void)unusedButMandatoryParameter;   /* L1-0: the signature exists so this
                                          entry is dispatch-table compatible —
                                          testSuite.c's table is
                                          {name, void(*)(uint16_t)} and
                                          forth_interp.txt drives it by name. */
  forthOuterCtx_t ctx;
  ctx.savedScope = forthCurrentScope;
  if (!forthTakeSourceFromX(ctx.source)) { return; }
  forthOuterRun(&ctx, FORTH_OUTER_FULL);
}
#endif

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

/* Test-only: program-step entry counter (F3-7) */
#ifdef FORTH_DEBUG_SELFTEST
static uint32_t forthTestProgramStepCount = 0;
#endif

void forthProgramStep(const uint8_t *payload) {
  #ifdef FORTH_DEBUG_SELFTEST
  forthTestProgramStepCount++;
  #endif
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

/* Test-only: program-step entry counter (F3-7) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestProgramStepCountReset(void) { forthTestProgramStepCount = 0; }
uint32_t forthTestProgramStepCountGet(void) { return forthTestProgramStepCount; }
#endif
