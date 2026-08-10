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
#define FORTH_PRIM_NONE ((uint16_t)0xFFFFu)  /* forthFindPrim miss sentinel (§3.3) */
#define FORTH_NAME_MAX  31
#define FORTH_TOKEN_MAX 63  /* §3.3.3 tokenizer max token length */
#define FORTH_HISTORY_MAX_BYTES 1024  /* FHIST program-memory cap (bytes),
                                          oldest-first eviction beyond this */

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

/* Sanity-check gdict after restoreCalc; resets to empty on corruption. */
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

/* Index of the definition under construction (== openDef.count;
   the entry is smudged and invisible to forthFindColon until ';').
   Returns false when no definition is open. */
bool forthOpenDefinitionIndex(uint16_t *idx);

/* Copy the name of the open definition into buf (up to bufSize-1 chars).
   Returns true if a name was copied, false if no open definition. */
bool openDefinitionName(char *buf, int bufSize);

/* Compile-time control flow (forth_compile.c) */
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

/* §4.2 executable-name resolution (forth_bridge.c).  forthTryColonFallback
 * is the whole "resolve then act" fallback used after a native label miss;
 * forthDispatchColon is just its action half, for callers that must do
 * something (leaveTamModeIfEnabled) between the two. */
void   forthDispatchColon(int16_t item, char *name, uint16_t widx);
bool_t forthTryColonFallback(int16_t item, char *name);

/* Browse surface: copy the name of the n-th listable entry
 * (newest-first) into out (>= 15 bytes, NUL-terminated).  Listable =
 * not FF_SMUDGE, nameLen 1..14, and (fdict variant) owner matches.
 * Returns false when fewer than n+1 listable entries exist.  This is
 * a BROWSE surface: it reads owners directly and never enters a
 * program-step scope (scope entry is for execution only). */
bool_t forthDictBrowseName(uint16_t n, uint16_t owner, char *out);
bool_t forthGDictBrowseName(uint16_t n, char *out);

#if defined(FORTH_DEBUG_SELFTEST)
/* Test hook: newest-first fdict walk, FIRST name match regardless
 * of owner or smudge state; set/clear FF_SMUDGE on that header.  No
 * product surface. */
void forthTestSmudgeSet(const char *name, bool_t on);
#endif

/* Forward (Forth-source) C47 item lookup: CAT_FNCT + PTP_NONE only (§4.1 step 4). */
bool forthFindItem(const char *name, uint16_t *itemId);

/* CAT_FNCT items whose PTP class is a parameter class (1<<9 .. 12<<9).
   PTP_NONE/PTP_LITERAL/PTP_REM/PTP_DISABLED are OUTSIDE the set:
   ITM_FORTH (PTP_REM) must keep resolving through the reverse path. */
bool forthFindItemParameterized(const char *name, uint16_t *itemId);

/* §10.4: control/declarative steps are not Forth-callable.  The PTP_NONE
 * subset is upstream's own funcIsProgramStopControl set; CASE is flow
 * inside PTP_REGISTER; FCALL's parameter is a Forth dictionary index
 * (names-only invariant).  Class rejects: label declaration/target,
 * step-relative jumps, skip-on-compare, key declarations. */
bool forthItemIsFlowReject(uint16_t itemId);

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

/* XEQN dispatch result */
typedef enum { FORTH_XEQN_DONE, FORTH_XEQN_COLON, FORTH_XEQN_ERR } forthXeqnResult_t;

/* Shared XEQN dispatch (kind-faithful).
   kind: 253 (STRING_LABEL_VARIABLE) or 249 (LOCAL_LABEL_VARIABLE).
   On COLON, sets *colonRef and returns — the caller dispatches. */
forthXeqnResult_t forthXeqnDispatch(const char *name, uint8_t kind, uint16_t *colonRef);

/* Shared marker-cell dispatch (253/250/254/255).
   nbuf: marker byte + len/param byte + name bytes (for 253/255) or
         marker byte + param byte (for 254/250).
   used: byte count of the meaningful portion of nbuf.
   Dispatches through paramCoreExecuteOpBounded(nbuf, nbuf+used, op, paramMode). */
void forthParamMarkerDispatch(uint16_t op, uint16_t ptpClass, uint8_t *nbuf, uint16_t used);

/* Which marker bytes a PTP class accepts. ONE table (forth_dict.c) —
   the compiler, the runtime decode, and all three walks read it, so a class
   can never accept a form in one place and reject it in another. */
#define FORTH_MK_NAME     0x01u   /* 253 STRING_LABEL_VARIABLE: 'NAME'    */
#define FORTH_MK_SYSFLAG  0x02u   /* 250 SYSTEM_FLAG_NUMBER               */
#define FORTH_MK_IND_REG  0x04u   /* 254 INDIRECT_REGISTER: →NN           */
#define FORTH_MK_IND_VAR  0x08u   /* 255 INDIRECT_VARIABLE: →'NAME'       */
uint8_t forthParamMarkerMask(uint16_t ptpClass);
uint8_t forthParamMarkerBit(uint8_t firstParamByte);

/* Byte span of ONE FTOK_C47 inline parameter group starting at `pos`
   (the first parameter cell; `limit` is exclusive). `strict` enforces byte
   legality and the pad-zero rule. Runtime, restore, and promotion walks pass
   true so malformed parameter data cannot become executable or persistent.
   Returns false when the group is malformed or would run past `limit`. */
bool forthParamCellSpan(const uint8_t *base, uint16_t pos, uint16_t limit,
                        uint16_t ptpClass, bool strict, uint16_t *spanOut);

/* Inner interpreter entry (§3.2) */
void forthInner(uint16_t entryIndex, bool fromProgram);

/* Push helpers (§3.3.4) — shared by inner interpreter and outer interpreter */
void forthPushReal34(const real34_t *val);
void forthPushInt32(int32_t val);

/* Data-stack overflow guard (forth_inner.c). The outer interpreter brackets
 * each line so the count starts clean and nested forthInner calls inherit it. */
void    forthDataDepthEnterOuter(void);
void    forthDataDepthLeaveOuter(void);
void    forthDataDepthResync(void);
bool_t  forthDataDepthApply(int16_t net);

/* Spill region (forth_inner.c) */
uint16_t forthSpillCount(void);
#if defined(FORTH_DEBUG_SELFTEST)
/* Per-line high-water mark of the spill region.  forthSpillCount is
 * reset by forthDataDepthLeaveOuter before a test can read it. */
uint16_t forthTestSpillHighWater(void);
#endif
void     forthSpillReset(void);
bool_t   forthSpillCatch(calcRegister_t reg);
bool_t   forthSpillRefill(calcRegister_t reg);
void     forthSpillSettle(void);
bool_t   forthPrimInvoke(uint16_t idx);

/* Bridge functions (§6) */
void fnForthCall(uint16_t param);
void fnForthOuter(uint16_t param);
#if defined(FORTH_DEBUG_SELFTEST)
void forthTestRunFromX(uint16_t unusedButMandatoryParameter);   /* Battery entry for the one-shot
                                   interpret-the-string-in-X semantics
                                   that fnForthOuter carries */
#endif

/* Program-step entry (§3.3.2) */
void forthProgramStep(const uint8_t *payload);

/* Scope variable */
uint16_t forthCurrentScopeGet(void);
uint16_t forthScopeEnterProgramStep(const uint8_t *anyPtrInProgram);
void forthScopeRestore(uint16_t prev);

/* Run-generation (§9.3) */
void forthRunGenBump(void);

/* Drop all first-touch scan records (state lives in forth_compile.c). */
void forthScanTrackReset(void);

/* Active-frame predicate (§9.3) */
bool forthInnerIsActive(void);

#if defined(PC_BUILD)
void forthSetTestInnerDepth(uint8_t depth);
#endif

/* Ref → name reverse lookup (for FCALL redirect) */
bool forthDictNameByRef(uint16_t ref, char *buf, int bufSize);

/* Same-line tracker (implemented in forth_compile.c) */
uint16_t forthLatestClosedRefGet(void);
void     forthLatestClosedRefSet(uint16_t ref);

/* GLOBAL — move transient word to gdict */
bool forthDictMakeLatestGlobal(uint16_t tref, uint16_t *grefOut);

/* IMMEDIATE — set FF_IMMEDIATE flag on a word */
bool forthDictSetImmediateByRef(uint16_t ref);

/* FORGET — truncate gdict at named word */
bool forthGDictForget(const char *name);

/* Outer interpreter (§3.3) */
void forthOuterInterpret(const char *source);
bool forthCheckSourceLine(const char *source);

/* §9.4 derived-state helpers (read-only, no persisted flag) */
bool forthStepPayload(const uint8_t *step, uint8_t *lenOut);
bool forthMarkerTurnsOn(const uint8_t *markerStep);
bool forthEntryStateAtCursor(void);
bool forthEntryStateAtInsertion(void);

/* §9.2 owning-program helpers */
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

/* Test-only: outer-interpreter nesting introspection */
#ifdef FORTH_DEBUG_SELFTEST
void *forthTestOuterCur(void);
uint8_t forthTestOuterDepth(void);
void forthTestSetOuterDepth(uint8_t d);
#endif

/* Test-only: program-step entry counter */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestProgramStepCountReset(void);
uint32_t forthTestProgramStepCountGet(void);
#endif

/* Test-only: scope override — lets a test prove that an outer-run
   entry point restores the scope it found rather than writing whatever its
   uninitialized ctx.savedScope happened to hold. */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestScopeSet(uint16_t scope);
#endif

#endif /* FORTH_DICT_H */
