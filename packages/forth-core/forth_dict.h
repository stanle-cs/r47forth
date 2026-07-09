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
#define FORTH_NAME_MAX  31

#define FF_IMMEDIATE    0x01        /* execute even in compile state */
#define FF_SMUDGE       0x02        /* hidden: definition in progress / incomplete */
#define FF_RESERVED     0xFC        /* must be 0 */

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

/* Ensure at least `bytes` free; may realloc the region. Returns true on success. */
bool forthDictEnsure(uint16_t bytes);

/* Allocate a new entry: header (4 bytes) + nameLen bytes + padding to 4-byte boundary.
   Returns region-relative offset of the new header, or FORTH_NULL on failure. */
uint16_t forthDictAllocate(uint8_t nameLen, uint16_t bodyBytes);

/* Write name into the header at region-relative offset `hdrOff`. */
void forthDictWriteName(uint16_t hdrOff, const char *name, uint8_t nameLen);

/* Emit a single byte at fdict.here (bumps here). */
void forthDictEmitByte(uint8_t byte);

/* Emit a 16-bit token at fdict.here (LE, bumps here by 2). */
void forthDictEmit(ftoken_t tok);

/* Emit raw bytes at fdict.here. */
void forthDictEmitBytes(const uint8_t *data, uint8_t len);

/* Finish current definition: emit FTOK_EXIT, clear FF_SMUDGE, block-round fdict.here. */
void forthDictFinishDef(void);

/* Lookup: §4.1 resolution order */
/* Scan forthPrims[] for name; returns index or 0xFFFF if not found. */
uint16_t forthFindPrim(const char *name);

/* Walk fdict.latest chain for name; skips FF_SMUDGE entries.
   Returns true on hit, sets *idx to dictionary index. */
bool forthFindColon(const char *name, uint16_t *idx);

/* Inner interpreter entry (§3.2) */
void forthInner(uint16_t entryIndex, bool fromProgram);

#endif /* FORTH_DICT_H */
