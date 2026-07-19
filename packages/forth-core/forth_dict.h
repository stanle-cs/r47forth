/*
 * forth_dict.h -- Forth dictionary control block and API
 * Per DESIGN.md §1.1-1.2
 */

#ifndef FORTH_DICT_H
#define FORTH_DICT_H

#include <stdbool.h>
#include <stdint.h>

/* ---- §1.1 In-RAM header layout ---- */

#define FORTH_NULL      0xFFFFu     /* end-of-chain sentinel (region-relative) */
#define FORTH_PRIM_NONE ((uint16_t)0xFFFFu)  /* forthFindPrim miss sentinel (C-3, §3.3) */
#define FORTH_NAME_MAX  31
#define FORTH_TOKEN_MAX 63  /* §3.3.3 tokenizer max token length */

#define FF_IMMEDIATE    0x01        /* execute even in compile state */
#define FF_SMUDGE       0x02        /* hidden: definition in progress / incomplete */
#define FF_DEFMARK      0x04        /* primitive flag: definition-completing marker (GLOBAL/IMMEDIATE) */
#define FF_RESERVED     0xF8        /* must be 0 */
#define FORTH_OWNER_INTERACTIVE 0xFFFFu
#define FORTH_OWNER_GLOBAL      0xFFFEu
#ifndef FTOK_EXIT
  #define FTOK_EXIT     0x0000u     /* end of colon definition body */
#endif
#ifndef FTOK_XEQN
  #define FTOK_XEQN     0x7F05u     /* XEQ by name: [kind][len][name][pad] */
#endif

typedef struct {                    /* stored in ram[], NEVER dereferenced as-is */
  uint16_t link;                    /* region-relative offset of previous header, or FORTH_NULL */
  uint8_t  flags;                   /* FF_* bits */
  uint8_t  nameLen;                 /* 1..31, byte length of name */
  uint16_t owner;                   /* owner tag: FORTH_OWNER_* */
} forthHeader_t;                    /* fixed prefix = 6 bytes */

/* Token type for threaded code (§2.2) */
typedef uint16_t ftoken_t;

/* ---- §1.2 Dictionary control block ---- */

typedef struct {
  uint8_t *base;        /* PCMEMPTR of region start (from allocC47Blocks); may move on grow */
  uint16_t sizeBlocks;  /* current region size in 4-byte blocks */
  uint16_t here;        /* region-relative byte offset of next free byte (bump ptr) */
  uint16_t latest;      /* region-relative byte offset of newest header, or FORTH_NULL */
  uint16_t count;       /* number of defined words (== next dictionary index) */
} forthDict_t;

extern forthDict_t fdict;
extern forthDict_t gdict;

/* ---- §3.3.3 Word refs and token space ---- */

#define FORTH_GCALL_BASE  0x7000u
#define FORTH_REF_GLOBAL  0x8000u

static inline ftoken_t forthTokenFromRef(uint16_t ref) {
  return (ref & FORTH_REF_GLOBAL) ? (ftoken_t)(FORTH_GCALL_BASE + (ref & 0x7FFFu))
                                  : (ftoken_t)(0x1000u + ref);
}
static inline uint16_t forthRefFromToken(ftoken_t tok) {
  return (tok >= FORTH_GCALL_BASE) ? (uint16_t)(FORTH_REF_GLOBAL | (tok - FORTH_GCALL_BASE))
                                   : (uint16_t)(tok - 0x1000u);
}

/* ---- API prototypes ---- */

/* Init / lifecycle */
void forthDictInit(void);
void forthDictClear(void);

/* H5: sanity-check gdict after restoreCalc; resets to empty on corruption. */
void forthGDictValidateRestored(void);

/* Test-only: override initial block count to force early realloc (DESIGN.md §7) */
#if defined(PC_BUILD)
  void forthDictSetTestInitialBlocks(uint16_t blocks);
#endif

/* Ensure at least `bytes` free; may realloc the region. Returns true on success. */
bool forthDictEnsure(uint16_t bytes);

/* gdict lifecycle & ensure */
void forthGDictInit(void);
void forthGDictClear(void);
bool forthGDictEnsure(uint16_t bytes);

/* Allocate a new entry: header (4 bytes) + nameLen bytes + padding to 4-byte boundary.
   Returns region-relative offset of the new header, or FORTH_NULL on failure. */
uint16_t forthDictAllocate(uint8_t nameLen, uint16_t bodyBytes);

/* Write name into the header at region-relative offset `hdrOff`. */
void forthDictWriteName(uint16_t hdrOff, const char *name, uint8_t nameLen);

/* Emit a 16-bit token at fdict.here; grows region if needed (§3.3.7). */
bool forthDictEmit(ftoken_t tok);

/* Emit raw bytes (nBytes must be even) at fdict.here; grows region if needed (§3.3.7). */
bool forthDictEmitBytes(const void *src, uint16_t nBytes);

/* Clear FF_SMUDGE at entryOff, block-round fdict.here (§3.3.7). */
bool forthDictFinishDef(uint16_t entryOff);

/* Start a colon-word definition (§3.3.7). */
bool startDefinition(const char *name);

/* Finish current definition: emit FTOK_EXIT, clear smudge, block-round (§3.3.7). */
bool finishDefinition(void);

/* Abort current definition; idempotent (§3.3.7). */
void abortDefinition(void);

/* Open-definition snapshot for nested interprets (§3.2 re-entrancy) */
typedef struct { uint16_t here, latest, count, entryOff; bool open; } forthDefState_t;
void forthDefStateSave(forthDefState_t *out);
void forthDefStateRestore(const forthDefState_t *in);

/* Returns true if a definition is currently open (smudged). */
bool isDefinitionOpen(void);

/* F1-4: index of the definition under construction (== openDef.count;
   the entry is smudged and invisible to forthFindColon until ';').
   Returns false when no definition is open. */
bool forthOpenDefinitionIndex(uint16_t *idx);

/* Copy the name of the open definition into buf (up to bufSize-1 chars).
   Returns true if a name was copied, false if no open definition. */
bool openDefinitionName(char *buf, int bufSize);

/* F3-5: compile-time control flow (forth_compile.c) */
void forthCtlIf(void);
void forthCtlThen(void);
void forthCtlElse(void);
void forthCtlBegin(void);
void forthCtlUntil(void);
void forthCtlAgain(void);
void forthCtlWhile(void);
void forthCtlRepeat(void);

/* Lookup: §4.1 resolution order */
/* Scan forthPrims[] for name; returns index or 0xFFFF if not found. */
uint16_t forthFindPrim(const char *name);

/* Walk fdict.latest chain for name; skips FF_SMUDGE entries.
   Returns true on hit, sets *ref to word ref (bit 15 = global). */
bool forthFindColon(const char *name, uint16_t *ref);

/* Same as forthFindColon but also reports hdr->flags. */
bool forthFindColonRef(const char *name, uint16_t *ref, uint8_t *flags);

/* Forward (Forth-source) C47 item lookup: CAT_FNCT + PTP_NONE only (§4.1 step 4). */
bool forthFindItem(const char *name, uint16_t *itemId);

/* CAT_FNCT items whose PTP class is a parameter class (1<<9 .. 12<<9).
   PTP_NONE/PTP_LITERAL/PTP_REM/PTP_DISABLED are OUTSIDE the set:
   ITM_FORTH (PTP_REM) must keep resolving through the reverse path. */
bool forthFindItemParameterized(const char *name, uint16_t *itemId);

/* Reverse lookup: §4.2 resolution order (label > item > colon) */
typedef enum {
  FORTH_XEQ_NONE = 0,
  FORTH_XEQ_LABEL = 1,
  FORTH_XEQ_COLON = 2,
  FORTH_XEQ_ITEM  = 3
} forthXEQType_t;

/* Resolve name for XEQ: C47 label first, C47 item second, Forth colon last (§4.2).
   Sets *param to label ID (LABEL), item ID (ITEM), or dictionary index (COLON). */
forthXEQType_t forthResolveXEQ(const char *name, uint16_t *param);

/* F3-6: XEQN dispatch result */
typedef enum { FORTH_XEQN_DONE, FORTH_XEQN_COLON, FORTH_XEQN_ERR } forthXeqnResult_t;

/* F3-6: shared XEQN dispatch (kind-faithful, B2 chain, B4 matrix).
   kind: 253 (STRING_LABEL_VARIABLE) or 249 (LOCAL_LABEL_VARIABLE).
   On COLON, sets *colonRef and returns — the caller dispatches. */
forthXeqnResult_t forthXeqnDispatch(const char *name, uint8_t kind, uint16_t *colonRef);

/* Inner interpreter entry (§3.2) */
void forthInner(uint16_t entryIndex, bool fromProgram);

/* Push helpers (§3.3.4) — shared by inner interpreter and outer interpreter */
void forthPushReal34(const real34_t *val);
void forthPushInt32(int32_t val);

/* Bridge functions (§6) */
void fnForthCall(uint16_t param);
void fnForthOuter(uint16_t param);

/* Program-step entry (P-2, §3.3.2) */
void forthProgramStep(const uint8_t *payload);

/* Scope variable (F3-3) */
uint16_t forthCurrentScopeGet(void);
uint16_t forthScopeEnterProgramStep(const uint8_t *anyPtrInProgram);
void forthScopeRestore(uint16_t prev);

/* Run-generation (§9.3) */
void forthRunGenBump(void);

/* F1-3: drop all first-touch scan records (state lives in forth_compile.c). */
void forthScanTrackReset(void);

/* Active-frame predicate (§9.3, F1-1) */
bool forthInnerIsActive(void);

#if defined(PC_BUILD)
void forthSetTestInnerDepth(uint8_t depth);
#endif

/* Ref → name reverse lookup (for FCALL redirect, C6) */
bool forthDictNameByRef(uint16_t ref, char *buf, int bufSize);

/* F3-4: same-line tracker (implemented in forth_compile.c) */
uint16_t forthLatestClosedRefGet(void);
void     forthLatestClosedRefSet(uint16_t ref);

/* F3-4: GLOBAL — move transient word to gdict */
bool forthDictMakeLatestGlobal(uint16_t tref, uint16_t *grefOut);

/* F3-4: IMMEDIATE — set FF_IMMEDIATE flag on a word */
bool forthDictSetImmediateByRef(uint16_t ref);

/* F3-4: FORGET — truncate gdict at named word */
bool forthGDictForget(const char *name);

/* Outer interpreter (§3.3) */
void forthOuterInterpret(const char *source);

/* §9.4 derived-state helpers (read-only, no persisted flag) */
bool forthStepPayload(const uint8_t *step, uint8_t *lenOut);
bool forthMarkerTurnsOn(const uint8_t *markerStep);
bool forthEntryStateAtCursor(void);
bool forthEntryStateAtInsertion(void);

/* §9.2 owning-program helpers (P2 pre-scan + §9.4 refactor) */
uint8_t *forthOwningProgramStart(const uint8_t *ptr);
uint8_t *forthNextProgramStart(const uint8_t *progStart);

/* Self-test harness (DESIGN.md §7) */
int forthDictSelfTest(void);

/* Inner interpreter nesting limit (§3.2) */
#define FORTH_NEST_MAX 4

/* Test-only: prime/read forthInner nesting depth (§3.2) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestSetDepth(uint8_t d);
uint8_t forthTestGetDepth(void);
uint8_t forthTestGetRsp(void);
#endif

/* Test-only: outer-interpreter nesting introspection (D-3) */
#ifdef FORTH_DEBUG_SELFTEST
void *forthTestOuterCur(void);
uint8_t forthTestOuterDepth(void);
void forthTestSetOuterDepth(uint8_t d);
#endif

/* Test-only: program-step entry counter (F3-7) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestProgramStepCountReset(void);
uint32_t forthTestProgramStepCountGet(void);
#endif

#endif /* FORTH_DICT_H */
