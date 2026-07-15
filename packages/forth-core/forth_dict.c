/*
 * forth_dict.c -- Managed-region dictionary core
 * Per DESIGN.md §1.2 and §5.2
 */

#include <string.h>

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"

#define FORTH_INITIAL_BLOCKS  64

/* ---- §1.2 Dictionary control block (package BSS, NOT in the arena) ---- */

/* Static initialization ensures fdict is well-formed even if production never
 * calls forthDictInit() before a lookup. On hardware there is no
 * FORTH_DEBUG_SELFTEST to trigger the self-test (which is what calls
 * forthDictInit on PC), so base must be NULL and latest must be the
 * end-of-chain sentinel from the very first instruction.
 * Field order per DESIGN.md §1.2: base, sizeBlocks, here, latest, count.
 */
forthDict_t fdict = {
  .base       = NULL,
  .sizeBlocks = 0,
  .here       = 0,
  .latest     = FORTH_NULL,   // 0xFFFF end-of-chain sentinel
  .count      = 0,
};

/* Test-only: override initial block count to force early realloc (DESIGN.md §7) */
#if defined(PC_BUILD)
  static uint16_t testInitialBlocks = 0;
  void forthDictSetTestInitialBlocks(uint16_t blocks) { testInitialBlocks = blocks; }
#endif

/* ---- Init / lifecycle ---- */

void forthDictInit(void)
{
  fdict.base = NULL;
  fdict.sizeBlocks = 0;
  fdict.here = 0;
  fdict.latest = FORTH_NULL;
  fdict.count = 0;
}

void forthDictClear(void)
{
  if (fdict.base) {
    freeC47Blocks(fdict.base, fdict.sizeBlocks);
  }
  fdict.base = NULL;
  fdict.sizeBlocks = 0;
  fdict.here = 0;
  fdict.latest = FORTH_NULL;
  fdict.count = 0;
}

/* H5 (§5.5): sanity-check fdict after a state restore. A torn or corrupt
 * backup must never leave fdict able to read/write out of bounds. */
void forthDictValidateRestored(void)
{
  if (fdict.base == NULL) {
    /* normalize scalars regardless of what the file said */
    fdict.sizeBlocks = 0;
    fdict.here = 0;
    fdict.latest = FORTH_NULL;
    fdict.count = 0;
    return;
  }

  uint32_t cap = (uint32_t)fdict.sizeBlocks * BYTES_PER_BLOCK;
  /* Pinned by tests: sizeBlocks!=0 (T1.3b V1), here<=cap (T1.3 v1),
   * nameLen bounds (T1.3b V2), n==count (T1.3 v2).
   * Declared redundant (termination/robustness, shadowed by the checks
   * above — do not remove without re-running the mutation analysis):
   * latest<here (shadowed by walk's off+4 bound), off+4>here vs OOB reads,
   * link strictly-decreasing (cycles are bounded by the n>count cap). */
  bool ok = (fdict.sizeBlocks != 0) && (fdict.here <= cap)
         && (fdict.latest == FORTH_NULL || fdict.latest < fdict.here);

  if (ok) {  /* walk the header chain: offsets must strictly decrease */
    uint16_t off = fdict.latest;
    uint16_t n = 0;
     while (off != FORTH_NULL) {
       if ((uint32_t)off + 4 > fdict.here) { ok = false; break; }
        forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
       if (hdr->nameLen == 0 || hdr->nameLen > FORTH_NAME_MAX) { ok = false; break; }
       /* R4-3: off+4<=here (checked above) proves the HEADER fits, not the
        * name that follows it. Probed: a valid ": VX 1 ;" entry with here
        * force-set to latest+4 survived validation — the header fit, but its
        * name read past the logical dictionary end. */
       if ((uint32_t)off + 4u + hdr->nameLen > fdict.here) { ok = false; break; }
       if (hdr->link != FORTH_NULL && hdr->link >= off) { ok = false; break; }
      off = hdr->link;
      if (++n > fdict.count) { ok = false; break; }
    }
    if (ok && n != fdict.count) {
      ok = false;
    }
  }

  if (!ok) {
#if defined(PC_BUILD)
    printf("forthDictValidateRestored: inconsistent dictionary in backup, resetting\n");
#endif
    /* Deliberate orphan: do NOT freeC47Blocks here — the restored allocation
     * tables are exactly what we just failed to trust (P-4 exception). */
    forthDictInit();
  }
}

/* ---- Region grow (§5.2) ---- */

bool forthDictEnsure(uint16_t bytes)
{
  /* 64 KB offset wrap (§3.3.8 C-10): reject before growing */
  if ((uint32_t)fdict.here + bytes > 0xFFFEu) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  /* Lazy initial allocation (§5.2).
   * R1-1 / R4-2 item 1 (reconciled — both named this branch): the configured
   * initial block count was allocated unconditionally and reported success
   * even when it did not cover `bytes`, so a first request larger than the
   * initial region was reported safe while the caller could write past the
   * allocation. fdict.base==NULL implies fdict.here==0 in every reachable
   * path (forthDictInit, forthDictClear, forthDictValidateRestored's
   * normalize branch), so `here + bytes` and `bytes` are the same quantity
   * here; using here+bytes anyway costs nothing and does not depend on that
   * invariant continuing to hold. */
  if (!fdict.base) {
    uint16_t initBlocks = FORTH_INITIAL_BLOCKS;
#if defined(PC_BUILD)
    if (testInitialBlocks > 0) initBlocks = testInitialBlocks;
#endif
    uint32_t need = (uint32_t)fdict.here + bytes;
    uint32_t minBlocks = TO_BLOCKS(need);
    if (minBlocks > initBlocks) {
      initBlocks = (uint16_t)minBlocks;
    }
    fdict.base = allocC47Blocks(initBlocks);
    if (!fdict.base) {
      return false;
    }
    fdict.sizeBlocks = initBlocks;
    return true;
  }

  /* Grow if needed (§5.2): max(sizeBlocks*2, TO_BLOCKS(here+bytes)) */
  if (fdict.here + bytes > fdict.sizeBlocks * BYTES_PER_BLOCK) {
    uint32_t need = (uint32_t)fdict.here + bytes;
    uint32_t newSize = fdict.sizeBlocks * 2;
    uint32_t minSize = TO_BLOCKS(need);
    if (minSize > newSize) {
      newSize = minSize;
    }
    void *newBase = reallocC47Blocks(fdict.base, fdict.sizeBlocks, (size_t)newSize);
    if (!newBase) {
      return false;
    }
    fdict.base = newBase;
    fdict.sizeBlocks = (uint16_t)newSize;
  }

  return true;
}

/* ---- Allocate (§1.2, §5.2) ---- */

uint16_t forthDictAllocate(uint8_t nameLen, uint16_t bodyBytes)
{
  /* R4-2 item 2: widened to uint32_t. The uint16_t form wraps a large
   * bodyBytes request silently (probed: forthDictAllocate(31, 0xFFF0)
   * wrapped and returned offset 0 with no error). The compiler today only
   * calls this with bodyBytes==0, so the wrap is unreachable in practice —
   * but the helper's own contract is checked here, not left as a false
   * promise for the first caller that uses bodyBytes for real. */
  uint32_t hdrSize = 4u + nameLen;
  uint32_t alignedHdr = (uint32_t)TO_BLOCKS(hdrSize) * BYTES_PER_BLOCK;
  uint32_t total = alignedHdr + bodyBytes;

  if ((uint32_t)fdict.here + total > 0xFFFEu) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return FORTH_NULL;
  }

  if (!forthDictEnsure((uint16_t)total)) {
    return FORTH_NULL;
  }

  uint16_t off = fdict.here;

  fdict.here = (uint16_t)(off + (uint16_t)alignedHdr);
  /* Chain: new header's link -> previous latest */
  if (fdict.base) {
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
    hdr->link = fdict.latest;
    hdr->flags = FF_SMUDGE;
    hdr->nameLen = nameLen;
  }
  fdict.latest = off;
  fdict.count++;

  return off;
}

/* ---- Write name (§1.2) ---- */

void forthDictWriteName(uint16_t hdrOff, const char *name, uint8_t nameLen)
{
  if (!fdict.base) return;
  forthHeader_t *hdr = (forthHeader_t *)(fdict.base + hdrOff);
  uint8_t copyLen = nameLen;
  if (copyLen > hdr->nameLen) {
    copyLen = hdr->nameLen;
  }
  if (copyLen > 0) {
    memcpy(fdict.base + hdrOff + 4, name, (size_t)copyLen);
  }
}

/* ---- Lookup: §4.1 resolution order ---- */

uint16_t forthFindPrim(const char *name)
{
  uint16_t i;
  for (i = 0; i < forthPrimCount; i++) {
    if (compareString(forthPrims[i].name, name, CMP_BINARY) == 0) {
      return i;
    }
  }
  return FORTH_PRIM_NONE;
}

bool forthFindColon(const char *name, uint16_t *idx)
{
  /* Guard against uninitialized dict (e.g., production hw path). */
  if (!fdict.base) return false;

  uint16_t off = fdict.latest;
  uint16_t n = 0;
  size_t queryLen = strlen(name);

  while (off != FORTH_NULL) {
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);

    if (!(hdr->flags & FF_SMUDGE)) {
      if (hdr->nameLen > 0 &&
          queryLen == hdr->nameLen &&
          memcmp(fdict.base + off + 4, name, (size_t)hdr->nameLen) == 0) {
        *idx = fdict.count - 1 - n;
        return true;
      }
    }

    off = hdr->link;
    n++;
  }

  return false;
}

/* ---- Dict-emit API (§3.3.7) ---- */

static struct { uint16_t here, latest, count, entryOff; bool open; } openDef;

/* §3.2 re-entrancy (D-3): snapshot/restore openDef so a nested interpret can
 * never finish or abort the outer line's definition. */
void forthDefStateSave(forthDefState_t *out)
{
  out->here = openDef.here;
  out->latest = openDef.latest;
  out->count = openDef.count;
  out->entryOff = openDef.entryOff;
  out->open = openDef.open;
}

void forthDefStateRestore(const forthDefState_t *in)
{
  openDef.here = in->here;
  openDef.latest = in->latest;
  openDef.count = in->count;
  openDef.entryOff = in->entryOff;
  openDef.open = in->open;
}

bool forthDictEmit(ftoken_t tok)
{
  if (!forthDictEnsure(2)) return false;
  memcpy(fdict.base + fdict.here, &tok, 2);
  fdict.here += 2;
  return true;
}

bool forthDictEmitBytes(const void *src, uint16_t nBytes)
{
  if (nBytes % 2 != 0) return false;
  uint16_t i;
  for (i = 0; i < nBytes; i += 2) {
    ftoken_t c;
    memcpy(&c, (const uint8_t *)src + i, 2);
    if (!forthDictEmit(c)) return false;
  }
  return true;
}

bool forthDictFinishDef(uint16_t entryOff)
{
  if (entryOff != FORTH_NULL && fdict.base) {
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + entryOff);
    hdr->flags &= (uint8_t)~FF_SMUDGE;
  }
  fdict.here = (uint16_t)TO_BLOCKS(fdict.here) * BYTES_PER_BLOCK;
  return true;
}

bool startDefinition(const char *name)
{
  size_t nameLen = strlen(name);
  if (nameLen == 0 || nameLen > FORTH_NAME_MAX) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  if (fdict.count >= 0x6F00) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  openDef.here = fdict.here;
  openDef.latest = fdict.latest;
  openDef.count = fdict.count;
  uint16_t off = forthDictAllocate((uint8_t)nameLen, 0);
  if (off == FORTH_NULL) {
    return false;
  }
  forthDictWriteName(off, name, (uint8_t)nameLen);
  {
    uint16_t hdrSize = 4 + (uint16_t)nameLen;
    uint16_t alignedHdr = (uint16_t)TO_BLOCKS(hdrSize) * BYTES_PER_BLOCK;
    uint16_t bodyOff = off + alignedHdr;
    uint16_t i;
    for (i = off + 4 + (uint16_t)nameLen; i < bodyOff; i++) {
      fdict.base[i] = 0;
    }
  }
  openDef.entryOff = off;
  openDef.open = true;
  return true;
}

bool finishDefinition(void)
{
  if (!forthDictEmit(FTOK_EXIT)) {
    abortDefinition();
    return false;
  }
  forthDictFinishDef(openDef.entryOff);
  openDef.open = false;
  return true;
}

void abortDefinition(void)
{
  if (!openDef.open) return;
  fdict.here = openDef.here;
  fdict.latest = openDef.latest;
  fdict.count = openDef.count;
  openDef.open = false;
}

bool isDefinitionOpen(void)
{
  return openDef.open;
}

bool openDefinitionName(char *buf, int bufSize)
{
  if (!openDef.open || openDef.entryOff == FORTH_NULL || !fdict.base || bufSize <= 0) return false;
  forthHeader_t *hdr = (forthHeader_t *)(fdict.base + openDef.entryOff);
  uint8_t len = hdr->nameLen;
  if (len >= (uint8_t)bufSize) len = (uint8_t)(bufSize - 1);
  memcpy(buf, fdict.base + openDef.entryOff + 4, (size_t)len);
  buf[len] = '\0';
  return len > 0;
}

/* ---- Index → name reverse lookup (for FCALL redirect, C6) ---- */

bool forthDictNameByIndex(uint16_t idx, char *buf, int bufSize)
{
  uint16_t off = fdict.latest;
  uint16_t n = 0;

  if (idx >= fdict.count || !fdict.base || !buf || bufSize <= 0) {
    return false;
  }

  while (off != FORTH_NULL) {
    if (fdict.count - 1 - n == idx) {
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      if (hdr->flags & FF_SMUDGE) return false;
      uint8_t len = hdr->nameLen;
      if (len >= (uint8_t)bufSize) len = (uint8_t)(bufSize - 1);
      memcpy(buf, fdict.base + off + 4, (size_t)len);
      buf[len] = '\0';
      return true;
    }
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
    off = hdr->link;
    n++;
  }

  return false;
}

/* ---- Reverse lookup: §4.2 resolution order ---- */

forthXEQType_t forthResolveXEQ(const char *name, uint16_t *param)
{
  /* C47 label first (§4.2: preserve existing programs' behavior) */
  calcRegister_t label = findNamedLabel(name);
  if (label != INVALID_VARIABLE) {
    *param = (uint16_t)label;
    return FORTH_XEQ_LABEL;
  }

  /* C47 item name second (built-in functions like FORTH) */
  {
    uint16_t i;
    for (i = 1; i < LAST_ITEM; i++) {
      if ((indexOfItems[i].status & CAT_STATUS) == CAT_FNCT &&
          compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME) == 0) {
        *param = i;
        return FORTH_XEQ_ITEM;
      }
    }
  }

  /* Forth colon last */
  {
    uint16_t widx;
    if (forthFindColon(name, &widx)) {
      *param = widx;
      return FORTH_XEQ_COLON;
    }
  }

  return FORTH_XEQ_NONE;
}
