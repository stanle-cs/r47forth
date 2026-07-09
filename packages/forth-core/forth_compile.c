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

/* ---- §3.3.2 Private source buffer & re-entrancy guard (C-5) ---- */

#define FORTH_SOURCE_MAX 256
static char  forthSource[FORTH_SOURCE_MAX];
static bool  forthOuterActive = false;

/* ---- §3.3.3 Tokenizer state (C-6) ---- */

#define FORTH_TOKEN_MAX 63

static const char *tokenizerSource;
static int16_t     tokenizerPos;

static void forthTokenizerInit(const char *src) {
  tokenizerSource = src;
  tokenizerPos = 0;
}

/*
 * C-6: glyph-wise tokenizer.  Delimiter: exactly 0x20.
 * Returns false at end of line.
 */
static bool nextToken(char buf[FORTH_TOKEN_MAX + 1]) {
  while (tokenizerSource[tokenizerPos] == ' ')
    tokenizerPos = stringNextGlyph(tokenizerSource, tokenizerPos);
  if (tokenizerSource[tokenizerPos] == 0) return false;
  int16_t start = tokenizerPos;
  while (tokenizerSource[tokenizerPos] != 0 && tokenizerSource[tokenizerPos] != ' ')
    tokenizerPos = stringNextGlyph(tokenizerSource, tokenizerPos);
  int16_t len = tokenizerPos - start;
  if (len > FORTH_TOKEN_MAX) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  xcopy(buf, tokenizerSource + start, len);
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
    } else if ((s[i] == '+' || s[i] == '-') && hasExp) {
      /* Sign immediately after e/E — valid per §3.3.5 */
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
 * forthOuterInterpret — core interpret/compile loop.
 * Called by fnForthOuter and by PC tests.
 * Per DESIGN.md §3.3 pseudocode, §3.3.1 (C-4), §3.3.6 (C-1).
 */
void forthOuterInterpret(const char *source) {
  forthState_t state = STATE_INTERPRET;
  bool lineOK = true;
  char buf[FORTH_TOKEN_MAX + 1];

  forthTokenizerInit(source);

  while (lineOK && nextToken(buf)) {
    /* ---- C-4: ':' colon matches the ':' character (B2) ---- */
    if (compareString(buf, ":", CMP_BINARY) == 0) {
      if (state == STATE_COMPILE) {
        abortDefinition();
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
      } else {
        char name[FORTH_TOKEN_MAX + 1];
        if (!nextToken(name)) {
          displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
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
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        lineOK = false;
      } else {
        if (!finishDefinition()) {
          lineOK = false;
        }
        /* M3: state = INTERPRET unconditionally on ';' */
        state = STATE_INTERPRET;
      }
      continue;
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
      uint16_t widx;
      if (forthFindColon(buf, &widx)) {
        if (state == STATE_COMPILE) {
          if (!forthDictEmit((ftoken_t)(FTOK_CALL_BASE + widx))) {
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

    /* ---- §4.1 step 3: number (C-2: number BEFORE label) ---- */
    if (processNumber(buf, state == STATE_COMPILE)) {
      if (lastErrorCode != ERROR_NONE) {
        if (isDefinitionOpen()) abortDefinition();
        lineOK = false;
      }
      continue;
    }

    /* ---- §4.1 step 4: C47 label (§3.3.6, C-1) ---- */
    {
      calcRegister_t label = findNamedLabel(buf);
      if (label != INVALID_VARIABLE) {
        if (state == STATE_COMPILE) {
          displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
          abortDefinition();
          lineOK = false;
        } else {
          uint8_t saved = programRunStop;
          programRunStop = PGM_RUNNING;
          reallyRunFunction(ITM_XEQ, (uint16_t)label);
          if (programRunStop == PGM_RUNNING) programRunStop = saved;
          if (lastErrorCode != ERROR_NONE) {
            if (isDefinitionOpen()) abortDefinition();
            lineOK = false;
          }
        }
        continue;
      }
    }

    /* ---- §4.1 last resort: undefined word ---- */
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    if (isDefinitionOpen()) abortDefinition();
    lineOK = false;
  }

  /* ---- End of line ---- */
  if (state == STATE_COMPILE) {
    /* C-4: unterminated definition — abort first, then error */
    abortDefinition();
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  } else if (lastErrorCode == ERROR_NONE) {
    /* C-7: ASLIFT-set gate checks lastErrorCode == ERROR_NONE */
    setSystemFlag(FLAG_ASLIFT);
  }
}

/* fnForthOuter — ITM_FORTH entry point (§3.3.2) */
void fnForthOuter(uint16_t unused) {
  if (forthOuterActive) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  int32_t len = stringByteLength(REGISTER_STRING_DATA(REGISTER_X));
  if (len + 1 > FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  xcopy(forthSource, REGISTER_STRING_DATA(REGISTER_X), len + 1);
  fnDrop(NOPARAM);
  forthOuterActive = true;
  forthOuterInterpret(forthSource);
  forthOuterActive = false;
}

