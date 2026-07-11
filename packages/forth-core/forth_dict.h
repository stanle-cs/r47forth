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

#define FF_IMMEDIATE    0x01        /* execute even in compile state */
#define FF_SMUDGE       0x02        /* hidden: definition in progress / incomplete */
#define FF_RESERVED     0xFC        /* must be 0 */
#ifndef FTOK_EXIT
  #define FTOK_EXIT     0x0000u     /* end of colon definition body */
#endif

typedef struct {                    /* stored in ram[], NEVER dereferenced as-is */
  uint16_t link;                    /* region-relative offset of previous header, or FORTH_NULL */
  uint8_t  flags;                   /* FF_* bits */
  uint8_t  nameLen;                 /* 1..31, byte length of name */
} forthHeader_t;                    /* fixed prefix = 4 bytes */

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

/* ---- API prototypes ---- */

/* Init / lifecycle */
void forthDictInit(void);
void forthDictClear(void);

/* Test-only: override initial block count to force early realloc (DESIGN.md §7) */
#if defined(PC_BUILD)
  void forthDictSetTestInitialBlocks(uint16_t blocks);
#endif

/* Ensure at least `bytes` free; may realloc the region. Returns true on success. */
bool forthDictEnsure(uint16_t bytes);

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

/* Returns true if a definition is currently open (smudged). */
bool isDefinitionOpen(void);

/* Copy the name of the open definition into buf (up to bufSize-1 chars).
   Returns true if a name was copied, false if no open definition. */
bool openDefinitionName(char *buf, int bufSize);

/* Lookup: §4.1 resolution order */
/* Scan forthPrims[] for name; returns index or 0xFFFF if not found. */
uint16_t forthFindPrim(const char *name);

/* Walk fdict.latest chain for name; skips FF_SMUDGE entries.
   Returns true on hit, sets *idx to dictionary index. */
bool forthFindColon(const char *name, uint16_t *idx);

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

/* Run-generation (§9.3) */
void forthRunGenBump(void);

/* Index → name reverse lookup (for FCALL redirect, C6) */
bool forthDictNameByIndex(uint16_t idx, char *buf, int bufSize);

/* Outer interpreter (§3.3) */
void forthOuterInterpret(const char *source);

/* §9.4 derived-state helpers (read-only, no persisted flag) */
bool forthStepPayload(const uint8_t *step, uint8_t *lenOut);
bool forthMarkerTurnsOn(const uint8_t *markerStep);
bool forthEntryStateAtCursor(void);

/* Self-test harness (DESIGN.md §7) */
int forthDictSelfTest(void);

/* Test-only: expose forthRunning for re-entrancy guard test (§3.2) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestSetRunning(bool val);
bool forthTestIsRunning(void);
#endif

#endif /* FORTH_DICT_H */
