/*
 * forth_dict.c -- Managed-region dictionary core
 * Per DESIGN.md §1.2 and §5.2
 */

#include <string.h>

#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"

#define FORTH_INITIAL_BLOCKS  64
#define FORTH_GDICT_INITIAL_BLOCKS 16

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

forthDict_t gdict = {
  .base       = NULL,
  .sizeBlocks = 0,
  .here       = 0,
  .latest     = FORTH_NULL,
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
  forthScanTrackReset();
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
  forthScanTrackReset();
}

void forthGDictInit(void)
{
  gdict.base = NULL;
  gdict.sizeBlocks = 0;
  gdict.here = 0;
  gdict.latest = FORTH_NULL;
  gdict.count = 0;
}

void forthGDictClear(void)
{
  if (gdict.base) {
    freeC47Blocks(gdict.base, gdict.sizeBlocks);
  }
  gdict.base = NULL;
  gdict.sizeBlocks = 0;
  gdict.here = 0;
  gdict.latest = FORTH_NULL;
  gdict.count = 0;
}

/* ---- §2.2 token constants (mirror forth_inner.c) — F1-5 validator ---- */
#define FTOK_CALL_BASE    0x1000
#define FTOK_LIT          0x7F00
#define FTOK_ILIT         0x7F01
#define FTOK_BR           0x7F02
#define FTOK_0BR          0x7F03
#define FTOK_C47          0x7F04

/* F1-5: validate one restored body in gdict, or (checkTarget != FORTH_NULL)
 * prove that checkTarget is a token boundary of this body at or before EXIT.
 * limit is exclusive. Restore-time only; the per-branch boundary sub-walk
 * is O(body^2) and deliberately unoptimized. */
static bool vBodyWalk(uint16_t bodyStart, uint16_t limit, uint16_t entryIdx,
                      uint16_t checkTarget)
{
  uint16_t pos = bodyStart;
  for (;;) {
    if (checkTarget != FORTH_NULL && pos == checkTarget) {
      return true;
    }
    if ((uint32_t)pos + 2u > limit) {
      return false;                       /* ran out without EXIT / target */
    }
    ftoken_t tok;
    memcpy(&tok, gdict.base + pos, 2);
    pos += 2;

    if (tok == FTOK_EXIT) {
      return checkTarget == FORTH_NULL;   /* end of body; target missed */
    }
    else if (tok >= 0x0001 && tok <= 0x0FFF) {
      if ((uint16_t)(tok - 1) >= forthPrimCount) return false;
    }
    else if (tok < FORTH_GCALL_BASE) {
      /* Transient call token in global body — illegal */
      return false;
    }
    else if (tok <= 0x7EFF) {             /* FORTH_GCALL_BASE..0x7EFF: global FTOK_CALL */
      if ((uint16_t)(tok - FORTH_GCALL_BASE) > entryIdx) return false;
    }
    else if (tok == FTOK_LIT) {
      if ((uint32_t)pos + 16u > limit) return false;
      pos += 16;
    }
    else if (tok == FTOK_ILIT) {
      if ((uint32_t)pos + 4u > limit) return false;
      pos += 4;
    }
    else if (tok == FTOK_BR || tok == FTOK_0BR) {
      if ((uint32_t)pos + 2u > limit) return false;
      int16_t delta;
      memcpy(&delta, gdict.base + pos, 2);
      pos += 2;
      if (checkTarget == FORTH_NULL) {
        int32_t target = (int32_t)pos + (int32_t)delta * 2;
        if (target < (int32_t)bodyStart || target >= (int32_t)limit) return false;
        if (!vBodyWalk(bodyStart, limit, entryIdx, (uint16_t)target)) return false;
      }
    }
    else if (tok == FTOK_C47) {
      if ((uint32_t)pos + 2u > limit) return false;
      uint16_t itemId;
      memcpy(&itemId, gdict.base + pos, 2);
      pos += 2;
      if (itemId == 0 || itemId >= LAST_ITEM) return false;
      uint16_t ptp = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);
      if (ptp == PTP_NONE) {
        /* no inline param */
      }
      else if (ptp == PTP_NUMBER_8) {
        if ((uint32_t)pos + 2u > limit) return false;
        if (gdict.base[pos + 1] != 0) return false;   /* padded cell */
        pos += 2;
      }
      else       if (ptp == PTP_NUMBER_16) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else if (ptp == PTP_NUMBER_8_16) {
        if ((uint32_t)pos + 2u > limit) return false;
        uint8_t b0 = gdict.base[pos];
        if (!(b0 <= 249 && gdict.base[pos + 1] == 0) && b0 != 250) return false;
        pos += 2;
      }
      else if (ptp == PTP_REGISTER) {
        if ((uint32_t)pos + 2u > limit) return false;
        if (gdict.base[pos + 1] != 0) return false;  /* pad byte */
        if (gdict.base[pos] > 224) return false;     /* byte legality */
        pos += 2;
      }
      else if (ptp == PTP_FLAG) {
        if ((uint32_t)pos + 2u > limit) return false;
        if (gdict.base[pos + 1] != 0) return false;  /* pad byte */
        { uint8_t b0 = gdict.base[pos];
          if (!(b0 <= 143 || (211 <= b0 && b0 <= 224))) return false; }
        pos += 2;
      }
      else if (ptp == PTP_SHUFFLE) {
        if ((uint32_t)pos + 2u > limit) return false;
        if (gdict.base[pos + 1] != 0) return false;  /* pad byte */
        /* any byte is legal */
        pos += 2;
      }
      else {
        return false;
      }
    }
    else if (tok == FTOK_XEQN) {
      /* F3-6: XEQN inline [kind][len][name][pad] */
      if ((uint32_t)pos + 2u > limit) return false;
      uint8_t xkind = gdict.base[pos];
      uint8_t xlen = gdict.base[pos + 1];
      if (xkind != STRING_LABEL_VARIABLE && xkind != LOCAL_LABEL_VARIABLE) return false;
      if (xlen < 1 || xlen > FORTH_NAME_MAX) return false;
      uint16_t xinline = (uint16_t)(2 + xlen);
      uint16_t xpadded = (uint16_t)((xinline + 1) & ~1u);
      if ((uint32_t)pos + xpadded > limit) return false;
      if (xpadded > xinline && gdict.base[pos + xinline] != 0) return false;
      pos += xpadded;
    }
    else {
      return false;   /* 0x7F06..0xFFFF reserved */
    }
  }
}

/* H5 (§5.5): sanity-check gdict after a state restore. A torn or corrupt
 * backup must never leave gdict able to read/write out of bounds. */
void forthGDictValidateRestored(void)
{
  if (gdict.base == NULL) {
    /* normalize scalars regardless of what the file said */
    gdict.sizeBlocks = 0;
    gdict.here = 0;
    gdict.latest = FORTH_NULL;
    gdict.count = 0;
    return;
  }

  uint32_t cap = (uint32_t)gdict.sizeBlocks * BYTES_PER_BLOCK;
  bool ok = (gdict.sizeBlocks != 0) && (gdict.here <= cap)
         && (gdict.latest == FORTH_NULL || gdict.latest < gdict.here);

  if (ok) {  /* walk the header chain: offsets must strictly decrease */
    uint16_t off = gdict.latest;
    uint16_t n = 0;
    uint16_t succOff = gdict.here;
     while (off != FORTH_NULL) {
        if ((uint32_t)off + 6 > gdict.here) { ok = false; break; }
        forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
       if (hdr->nameLen == 0 || hdr->nameLen > FORTH_NAME_MAX) { ok = false; break; }
        if ((uint32_t)off + 6u + hdr->nameLen > gdict.here) { ok = false; break; }
       if (hdr->link != FORTH_NULL && hdr->link >= off) { ok = false; break; }
      {
        uint16_t hdrSize = 6 + hdr->nameLen;
        uint16_t alignedHdr = (uint16_t)TO_BLOCKS(hdrSize) * BYTES_PER_BLOCK;
        uint16_t bodyStart = off + alignedHdr;
        uint16_t i;
        if (hdr->flags & (uint8_t)~FF_IMMEDIATE) { ok = false; break; }
        if (hdr->owner != FORTH_OWNER_GLOBAL) { ok = false; break; }
        for (i = off + 6 + hdr->nameLen; i < bodyStart; i++) {
          if (gdict.base[i] != 0) { ok = false; break; }
        }
        if (!ok) break;
        if ((uint32_t)bodyStart + 2u > succOff) { ok = false; break; }
        if (!vBodyWalk(bodyStart, succOff,
                       (uint16_t)(gdict.count - 1 - n), FORTH_NULL)) {
          ok = false;
          break;
        }
      }
      succOff = off;
      off = hdr->link;
      if (++n > gdict.count) { ok = false; break; }
    }
    if (ok && n != gdict.count) {
      ok = false;
    }
  }

  if (!ok) {
#if defined(PC_BUILD)
    printf("forthGDictValidateRestored: inconsistent dictionary in backup, resetting\n");
#endif
    lastErrorCode = ERROR_INVALID_CORRUPTED_DATA;
    /* Deliberate orphan: do NOT freeC47Blocks here — the restored allocation
     * tables are exactly what we just failed to trust (P-4 exception). */
    forthGDictInit();
  }
}

/* ---- Region grow (§5.2) ---- */

static bool dictEnsureOn(forthDict_t *d, uint16_t bytes, uint16_t initialBlocks)
{
  /* 64 KB offset wrap (§3.3.8 C-10): reject before growing */
  if ((uint32_t)d->here + bytes > 0xFFFEu) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  /* Lazy initial allocation (§5.2). */
  if (!d->base) {
    uint32_t need = (uint32_t)d->here + bytes;
    uint32_t minBlocks = TO_BLOCKS(need);
    uint16_t ib = initialBlocks;
    if (minBlocks > ib) {
      ib = (uint16_t)minBlocks;
    }
    d->base = allocC47Blocks(ib);
    if (!d->base) {
      return false;
    }
    d->sizeBlocks = ib;
    return true;
  }

  /* Grow if needed (§5.2): max(sizeBlocks*2, TO_BLOCKS(here+bytes)) */
  if (d->here + bytes > d->sizeBlocks * BYTES_PER_BLOCK) {
    uint32_t need = (uint32_t)d->here + bytes;
    uint32_t newSize = d->sizeBlocks * 2;
    uint32_t minSize = TO_BLOCKS(need);
    if (minSize > newSize) {
      newSize = minSize;
    }
    void *newBase = reallocC47Blocks(d->base, d->sizeBlocks, (size_t)newSize);
    if (!newBase) {
      return false;
    }
    d->base = newBase;
    d->sizeBlocks = (uint16_t)newSize;
  }

  return true;
}

bool forthDictEnsure(uint16_t bytes)
{
  uint16_t initBlocks = FORTH_INITIAL_BLOCKS;
#if defined(PC_BUILD)
  if (testInitialBlocks > 0) initBlocks = testInitialBlocks;
#endif
  return dictEnsureOn(&fdict, bytes, initBlocks);
}

bool forthGDictEnsure(uint16_t bytes)
{
  return dictEnsureOn(&gdict, bytes, FORTH_GDICT_INITIAL_BLOCKS);
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
  uint32_t hdrSize = 6u + nameLen;
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
    hdr->owner = FORTH_OWNER_INTERACTIVE;
    {
      uint32_t padFrom = (uint32_t)off + 6u + nameLen;
      for (uint32_t i = padFrom; i < (uint32_t)off + alignedHdr; i++) {
        fdict.base[i] = 0;
      }
    }
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
    memcpy(fdict.base + hdrOff + 6, name, (size_t)copyLen);
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

bool forthFindColonRef(const char *name, uint16_t *ref, uint8_t *flags)
{
  size_t queryLen = strlen(name);

  /* Walk fdict newest-first, filtered by owner (F3-3). */
  if (fdict.base) {
    uint16_t off = fdict.latest;
    uint16_t n = 0;
    while (off != FORTH_NULL) {
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
       if (!(hdr->flags & FF_SMUDGE) &&
           hdr->owner == forthCurrentScopeGet()) {
        if (hdr->nameLen > 0 &&
            queryLen == hdr->nameLen &&
            memcmp(fdict.base + off + 6, name, (size_t)hdr->nameLen) == 0) {
          *ref = fdict.count - 1 - n;
          if (flags) *flags = hdr->flags;
          return true;
        }
      }
      off = hdr->link;
      n++;
    }
  }

  /* Walk gdict newest-first. */
  if (gdict.base) {
    uint16_t off = gdict.latest;
    uint16_t n = 0;
    while (off != FORTH_NULL) {
      forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
      if (!(hdr->flags & FF_SMUDGE)) {
        if (hdr->nameLen > 0 &&
            queryLen == hdr->nameLen &&
            memcmp(gdict.base + off + 6, name, (size_t)hdr->nameLen) == 0) {
          *ref = (uint16_t)(FORTH_REF_GLOBAL | (gdict.count - 1 - n));
          if (flags) *flags = hdr->flags;
          return true;
        }
      }
      off = hdr->link;
      n++;
    }
  }

  return false;
}

bool forthFindColon(const char *name, uint16_t *ref)
{
  return forthFindColonRef(name, ref, NULL);
}

/* §4.1 step 4: forward (Forth-source) C47 item lookup.
 * Only matches CAT_FNCT + PTP_NONE items.  Parameterized items
 * (PTP_REGISTER, PTP_NUMBER_8/16) are excluded — stage F4. */
bool forthFindItem(const char *name, uint16_t *itemId)
{
  uint16_t i;
  for (i = 1; i < LAST_ITEM; i++) {
    if ((indexOfItems[i].status & CAT_STATUS) == CAT_FNCT &&
        (indexOfItems[i].status & PTP_STATUS) == PTP_NONE &&
        compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME) == 0) {
      *itemId = i;
      return true;
    }
  }
  return false;
}

/* CAT_FNCT items whose PTP class is a parameter class (1<<9 .. 12<<9).
   PTP_NONE/PTP_LITERAL/PTP_REM/PTP_DISABLED are OUTSIDE the set:
   ITM_FORTH (PTP_REM) must keep resolving through the reverse path. */
bool forthFindItemParameterized(const char *name, uint16_t *itemId)
{
  uint16_t i;
  for (i = 1; i < LAST_ITEM; i++) {
    uint16_t ptp = (uint16_t)(indexOfItems[i].status & PTP_STATUS);
    if ((indexOfItems[i].status & CAT_STATUS) == CAT_FNCT &&
        ptp >= PTP_DECLARE_LABEL && ptp <= PTP_MENU &&
        compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME) == 0) {
      *itemId = i;
      return true;
    }
  }
  return false;
}

/* §10.4: control/declarative steps are not Forth-callable.  The PTP_NONE
 * subset is upstream's own funcIsProgramStopControl set (items.c);
 * CASE is flow inside PTP_REGISTER; FCALL's parameter is a Forth
 * dictionary index (names-only invariant).  Class rejects: label
 * declaration/target, step-relative jumps, skip-on-compare, key
 * declarations. */
bool forthItemIsFlowReject(uint16_t itemId)
{
  uint16_t ptp = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);
  if (itemId == ITM_END || itemId == ITM_RTN || itemId == ITM_STOP ||
      itemId == ITM_RTNP1 || itemId == ITM_CASE || itemId == ITM_FCALL) {
    return true;
  }
  return ptp == PTP_DECLARE_LABEL || ptp == PTP_LABEL ||
         ptp == PTP_SKIP_BACK || ptp == PTP_COMPARE ||
         ptp == PTP_KEYG_KEYX;
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
  if (fdict.count >= 0x6000) {
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
  ((forthHeader_t *)(fdict.base + off))->owner = forthCurrentScopeGet();
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

bool forthOpenDefinitionIndex(uint16_t *idx)
{
  if (!openDef.open) {
    return false;
  }
  *idx = openDef.count;
  return true;
}

bool openDefinitionName(char *buf, int bufSize)
{
  if (!openDef.open || openDef.entryOff == FORTH_NULL || !fdict.base || bufSize <= 0) return false;
  forthHeader_t *hdr = (forthHeader_t *)(fdict.base + openDef.entryOff);
  uint8_t len = hdr->nameLen;
  if (len >= (uint8_t)bufSize) len = (uint8_t)(bufSize - 1);
  memcpy(buf, fdict.base + openDef.entryOff + 6, (size_t)len);
  buf[len] = '\0';
  return len > 0;
}

/* ---- Ref → name reverse lookup (for FCALL redirect, C6) ---- */

bool forthDictNameByRef(uint16_t ref, char *buf, int bufSize)
{
  if (!buf || bufSize <= 0) return false;

  if (ref & FORTH_REF_GLOBAL) {
    /* Global region */
    uint16_t idx = ref & 0x7FFFu;
    uint16_t off = gdict.latest;
    uint16_t n = 0;
    if (idx >= gdict.count || !gdict.base) return false;
    while (off != FORTH_NULL) {
      if (gdict.count - 1 - n == idx) {
        forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
        if (hdr->flags & FF_SMUDGE) return false;
        uint8_t len = hdr->nameLen;
        if (len >= (uint8_t)bufSize) len = (uint8_t)(bufSize - 1);
        memcpy(buf, gdict.base + off + 6, (size_t)len);
        buf[len] = '\0';
        return true;
      }
      forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
      off = hdr->link;
      n++;
    }
  } else {
    /* Transient region */
    uint16_t idx = ref;
    uint16_t off = fdict.latest;
    uint16_t n = 0;
    if (idx >= fdict.count || !fdict.base) return false;
    while (off != FORTH_NULL) {
      if (fdict.count - 1 - n == idx) {
        forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
        if (hdr->flags & FF_SMUDGE) return false;
        uint8_t len = hdr->nameLen;
        if (len >= (uint8_t)bufSize) len = (uint8_t)(bufSize - 1);
        memcpy(buf, fdict.base + off + 6, (size_t)len);
        buf[len] = '\0';
        return true;
      }
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      off = hdr->link;
      n++;
    }
  }

  return false;
}

/* ---- F3-4: GLOBAL — move transient word to gdict ---- */

static bool validateWalkOn(const uint8_t *base, uint16_t bodyStart, uint16_t limit,
                           uint16_t selfTok, uint16_t trefIdx, uint16_t gcount)
{
  uint16_t pos = bodyStart;
  for (;;) {
    if ((uint32_t)pos + 2u > limit) return false;
    ftoken_t tok;
    memcpy(&tok, base + pos, 2);
    pos += 2;
    if (tok == FTOK_EXIT) return true;
    else if (tok >= 0x0001 && tok <= 0x0FFF) {
      if ((uint16_t)(tok - 1) >= forthPrimCount) return false;
    }
    else if (tok >= FTOK_CALL_BASE && tok < FORTH_GCALL_BASE) {
      /* Transient-space call */
      if (tok != selfTok) return false;  /* call to another transient — illegal */
    }
    else if (tok >= FORTH_GCALL_BASE && tok <= 0x7EFF) {
      if ((uint16_t)(tok - FORTH_GCALL_BASE) >= gcount) return false;
    }
    else if (tok == FTOK_XEQN) {
      /* F3-6: XEQN — names resolve fresh at runtime; accept and advance */
      if ((uint32_t)pos + 2u > limit) return false;
      uint8_t vk = base[pos];
      uint8_t vl = base[pos + 1];
      if (vk != STRING_LABEL_VARIABLE && vk != LOCAL_LABEL_VARIABLE) return false;
      if (vl < 1 || vl > FORTH_NAME_MAX) return false;
      uint16_t vi = (uint16_t)(2 + vl);
      uint16_t vp = (uint16_t)((vi + 1) & ~1u);
      if ((uint32_t)pos + vp > limit) return false;
      if (vp > vi && base[pos + vi] != 0) return false;
      pos += vp;
    }
    else if (tok >= 0x7F06) return false;  /* reserved */
    else if (tok == FTOK_LIT) {
      if ((uint32_t)pos + 16u > limit) return false;
      pos += 16;
    }
    else if (tok == FTOK_ILIT) {
      if ((uint32_t)pos + 4u > limit) return false;
      pos += 4;
    }
    else if (tok == FTOK_BR || tok == FTOK_0BR) {
      if ((uint32_t)pos + 2u > limit) return false;
      pos += 2;
    }
    else if (tok == FTOK_C47) {
      if ((uint32_t)pos + 2u > limit) return false;
      uint16_t itemId;
      memcpy(&itemId, base + pos, 2);
      pos += 2;
      if (itemId == 0 || itemId >= LAST_ITEM) return false;
      uint16_t ptp = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);
      if (ptp == PTP_NONE) {
        /* no inline param */
      }
      else if (ptp == PTP_NUMBER_8) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else if (ptp == PTP_NUMBER_16) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else if (ptp == PTP_NUMBER_8_16) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else if (ptp == PTP_REGISTER || ptp == PTP_FLAG || ptp == PTP_SHUFFLE) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else return false;
    }
    else return false;
  }
}

bool forthDictMakeLatestGlobal(uint16_t tref, uint16_t *grefOut)
{
  uint16_t idx = tref;

  /* Step 1: validate preconditions */
  if (!fdict.base || fdict.latest == FORTH_NULL ||
      idx != (uint16_t)(fdict.count - 1)) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  uint16_t off = fdict.latest;
  forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);

  /* Step 2: defensive smudge check + compute body start */
  if (hdr->flags & FF_SMUDGE) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  uint16_t bodyStart = off + (uint16_t)TO_BLOCKS(6 + hdr->nameLen) * BYTES_PER_BLOCK;
  uint16_t selfTok = (ftoken_t)(0x1000 + idx);

  /* Step 3: validate walk */
  if (!validateWalkOn(fdict.base, bodyStart, fdict.here, selfTok, idx, gdict.count)) {
    displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  /* Recompute end from walk */
  {
    uint16_t pos = bodyStart;
    for (;;) {
      ftoken_t tok;
      memcpy(&tok, fdict.base + pos, 2);
      pos += 2;
      if (tok == FTOK_EXIT) break;
      else if (tok == FTOK_LIT) pos += 16;
      else if (tok == FTOK_ILIT) pos += 4;
      else if (tok == FTOK_BR || tok == FTOK_0BR) pos += 2;
      else if (tok == FTOK_C47) {
        uint16_t itemId2;
        memcpy(&itemId2, fdict.base + pos, 2);
        pos += 2;
        uint16_t ptp2 = (uint16_t)(indexOfItems[itemId2].status & PTP_STATUS);
        if (ptp2 == PTP_NUMBER_8 || ptp2 == PTP_NUMBER_16 || ptp2 == PTP_NUMBER_8_16 ||
            ptp2 == PTP_REGISTER || ptp2 == PTP_FLAG || ptp2 == PTP_SHUFFLE) pos += 2;
      }
      else if (tok == FTOK_XEQN) {
        uint8_t xk2 = fdict.base[pos];
        uint8_t xl2 = fdict.base[pos + 1];
        uint16_t xi2 = (uint16_t)(2 + xl2);
        uint16_t xp2 = (uint16_t)((xi2 + 1) & ~1u);
        pos += xp2;
      }
    }
    uint16_t entryBytes = pos - off;

    /* Step 4: ensure gdict space */
    if (!forthGDictEnsure(entryBytes)) return false;

    /* Re-derive hdr for hygiene */
    hdr = (forthHeader_t *)(fdict.base + off);

    /* Step 5: copy to gdict */
    uint16_t goff = gdict.here;
    memcpy(gdict.base + goff, fdict.base + off, entryBytes);

    /* Patch link and owner in the copy */
    uint16_t link = gdict.latest;
    ((forthHeader_t *)(gdict.base + goff))->link = link;
    ((forthHeader_t *)(gdict.base + goff))->owner = FORTH_OWNER_GLOBAL;

    /* Step 6: rewrite walk on the COPY — rewrite selfTok */
    {
      ftoken_t newTok = (ftoken_t)(FORTH_GCALL_BASE + gdict.count);
      uint8_t newTokBytes[2];
      memcpy(newTokBytes, &newTok, 2);
      uint16_t gBodyStart = goff + (uint16_t)TO_BLOCKS(6 + hdr->nameLen) * BYTES_PER_BLOCK;
      uint16_t pos = gBodyStart;
      for (;;) {
        ftoken_t tok;
        memcpy(&tok, gdict.base + pos, 2);
        pos += 2;
        if (tok == FTOK_EXIT) break;
        if (tok == selfTok) {
          memcpy(gdict.base + (pos - 2), newTokBytes, 2);
        }
        else if (tok == FTOK_LIT) pos += 16;
        else if (tok == FTOK_ILIT) pos += 4;
        else if (tok == FTOK_BR || tok == FTOK_0BR) pos += 2;
        else if (tok == FTOK_C47) {
          uint16_t itemId3;
          memcpy(&itemId3, gdict.base + pos, 2);
          pos += 2;
          uint16_t ptp3 = (uint16_t)(indexOfItems[itemId3].status & PTP_STATUS);
          if (ptp3 == PTP_NUMBER_8 || ptp3 == PTP_NUMBER_16 || ptp3 == PTP_NUMBER_8_16 ||
              ptp3 == PTP_REGISTER || ptp3 == PTP_FLAG || ptp3 == PTP_SHUFFLE) pos += 2;
        }
        else if (tok == FTOK_XEQN) {
          uint8_t xk3 = gdict.base[pos];
          uint8_t xl3 = gdict.base[pos + 1];
          uint16_t xi3 = (uint16_t)(2 + xl3);
          uint16_t xp3 = (uint16_t)((xi3 + 1) & ~1u);
          pos += xp3;
        }
      }
    }

    /* Step 7: commit gdict */
    gdict.latest = goff;
    gdict.here = (uint16_t)TO_BLOCKS(goff + entryBytes) * BYTES_PER_BLOCK;
    gdict.count++;

    /* Step 8: roll off fdict */
    uint16_t savedLink = hdr->link;
    fdict.here = off;
    fdict.latest = savedLink;
    fdict.count--;

    /* Step 9: return global ref */
    *grefOut = (uint16_t)(FORTH_REF_GLOBAL | (gdict.count - 1));
    return true;
  }
}

/* ---- F3-4: IMMEDIATE — set FF_IMMEDIATE by ref ---- */

bool forthDictSetImmediateByRef(uint16_t ref)
{
  if (ref & FORTH_REF_GLOBAL) {
    uint16_t idx = ref & 0x7FFFu;
    if (!gdict.base || idx >= gdict.count) return false;
    uint16_t off = gdict.latest;
    uint16_t n = 0;
    while (off != FORTH_NULL) {
      if (gdict.count - 1 - n == idx) {
        forthHeader_t *h = (forthHeader_t *)(gdict.base + off);
        if (h->flags & FF_SMUDGE) return false;
        h->flags |= FF_IMMEDIATE;
        return true;
      }
      forthHeader_t *h = (forthHeader_t *)(gdict.base + off);
      off = h->link;
      n++;
    }
  } else {
    uint16_t idx = ref;
    if (!fdict.base || idx >= fdict.count) return false;
    uint16_t off = fdict.latest;
    uint16_t n = 0;
    while (off != FORTH_NULL) {
      if (fdict.count - 1 - n == idx) {
        forthHeader_t *h = (forthHeader_t *)(fdict.base + off);
        if (h->flags & FF_SMUDGE) return false;
        h->flags |= FF_IMMEDIATE;
        return true;
      }
      forthHeader_t *h = (forthHeader_t *)(fdict.base + off);
      off = h->link;
      n++;
    }
  }
  return false;
}

/* ---- F3-4: FORGET — truncate gdict at named word ---- */

bool forthGDictForget(const char *name)
{
  if (!gdict.base) {
    xcopy(errorMessage, name, ERROR_MESSAGE_LENGTH);
    displayCalcErrorMessage(ERROR_FUNCTION_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }

  size_t nameLen = strlen(name);
  uint16_t off = gdict.latest;
  uint16_t n = 0;

  while (off != FORTH_NULL) {
    forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
    if (hdr->nameLen > 0 &&
        nameLen == hdr->nameLen &&
        memcmp(gdict.base + off + 6, name, nameLen) == 0) {
      /* Hit at `off` after skipping `n` newer entries */
      gdict.count -= (uint16_t)(n + 1);
      gdict.latest = hdr->link;
      gdict.here = off;
      return true;
    }
    off = hdr->link;
    n++;
  }

  xcopy(errorMessage, name, ERROR_MESSAGE_LENGTH);
  displayCalcErrorMessage(ERROR_FUNCTION_NOT_FOUND, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  return false;
}

/* ---- Reverse lookup: §4.2 resolution order ---- */

forthXEQType_t forthResolveXEQ(const char *name, uint16_t *param)
{
  /* C47 label first (§4.2: preserve existing programs' behavior).
   * GLOBAL_LABELS (upstream rebase to b8f79e486): findNamedLabel gained a
   * labelType selector when upstream added named LOCAL labels. Forth's
   * label lookup has only ever meant global labels (R4 ruling: "native RPN
   * labels retain their established global visibility") — GLOBAL_LABELS
   * preserves that exactly and does not silently adopt local-named lookup. */
  calcRegister_t label = findNamedLabel(name, GLOBAL_LABELS);
  if (label != INVALID_VARIABLE) {
    *param = (uint16_t)label;
    return FORTH_XEQ_LABEL;
  }

  /* C47 item name second (built-in functions like FORTH)
   * B3 reverse: reject parameterized items (PTP_DECLARE_LABEL..PTP_MENU).
   * ITM_FORTH (PTP_REM) keeps resolving; ITM_FCALL (PTP_NUMBER_16) stops. */
  {
    uint16_t i;
    for (i = 1; i < LAST_ITEM; i++) {
      uint16_t ptp = (uint16_t)(indexOfItems[i].status & PTP_STATUS);
      if ((indexOfItems[i].status & CAT_STATUS) == CAT_FNCT &&
          !(ptp >= PTP_DECLARE_LABEL && ptp <= PTP_MENU) &&
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
