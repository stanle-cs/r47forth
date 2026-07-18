# Upstream reports — three findings verified at b8f79e486

**Version:** `00.109.03.04a0.int` (`VERSION1`, src/c47/defines.h:12), commit
`b8f79e486`. All line numbers below are that tree. Verified against the
pristine sources on 2026-07-16 (GTK simulator build; findings are
code-inspection + simulator evidence, none are simulator-only).

Each section is self-contained — paste as its own issue.

---

## 1. `XEQ :name:` falls through to the function-name scan when the local label is missing

**File:** `src/c47/ui/tam.c`, `_tamProcessInput`.

The named-local-label feature resolves TAM label input kind-faithfully:

- tam.c:936 — `findNamedLabelWithDuplicate(buffer, dupNum,
  (tam.colon ? LOCAL_LABELS : GLOBAL_LABELS))`.

But the XEQ fallback that runs when no label is found —

- tam.c:962-977 — `if(value == INVALID_VARIABLE && (tam.function == ITM_XEQ
  || tam.function == ITM_XEQP1))` → scan `indexOfItems[].itemCatalogName`
  for a `CAT_FNCT` match and `runFunction(i)` —

is gated only on `!tam.indirect`. It never checks `tam.colon`. So an
**explicitly local** request whose label does not exist in the current
program falls through to the function table:

- Interactive: `XEQ` → `:` (sets `tam.colon`) → type `SIN` → ENTER. No local
  label `:SIN:` exists → the scan matches the item `SIN` and **executes
  sine** instead of reporting the label as not found.
- In PEM the same entry path (tam.c:967-973) records a **`SIN` function
  step** — the user asked for a local-label call and silently got a
  different instruction.

Expected: when `tam.colon` is set, a resolution miss is a label-not-found
error (the surface at tam.c:986). That is the contract the rest of the
feature honors — committed local-label steps encode `LOCAL_LABEL_VARIABLE`
and never re-resolve as anything else at execution time; only this entry-time
fallback breaks kind-faithfulness.

**Suggested fix (one line):** add `&& !tam.colon` to the fallback condition
at tam.c:962 (or hoist `if(tam.colon)` straight to the error path), so a
colon request either resolves a local label or fails.

---

## 2. `_decodeOneStep` indexes `indexOfItems[op]` without an upper bound

**File:** `src/c47/programming/decode.c`, `_decodeOneStep` (:846).

The opcode is decoded from raw program bytes; the two-byte form yields
`op` up to `0x7EFF` (32511):

- decode.c:847-852 — `op = *(step++); if(op & 0x80) { op &= 0x7f;
  op <<= 8; op |= *(step++); }`
- decode.c:854 — only `0x7fff` (`.END.`) is special-cased;
- decode.c:860 — `switch(indexOfItems[op].status & PTP_STATUS)` indexes the
  table immediately, followed by `.itemCatalogName` / `.itemSoftmenuName`
  dereferences (:865-867, :882, :912-925) and
  `decodeOp(..., indexOfItems[op].tamMinMax ...)` (:929).

`LAST_ITEM` is 2870 (src/c47/items.h:2989). Any opcode in
`2870..0x7EFE` reads far past the end of `indexOfItems[]`, and the
subsequent `strcpy`/`stringCopy` of an out-of-bounds `.itemCatalogName`
pointer dereferences a wild pointer — a crash or garbage render, on device
too.

Malformed opcodes are not reachable from normal key entry, but the renderer
consumes the same untrusted bytes as the step walker, and the walker was
recently hardened (`programBytesAvailable()` bounds checks in
`findKey2ndParam`, including the computed-end check) — a damaged or
hand-edited state file that gets past import still reaches the *listing*
renderer with no guard. Same class, one consumer left unguarded.

**Suggested fix (two lines):** after the `.END.` check in `_decodeOneStep`,
reject out-of-range opcodes: `if(op >= LAST_ITEM) { render a fixed "???"
marker (or the byte value); return; }`.

---

## 3. `freeListFree` accepts overlapping/double frees and corrupts the free list silently on device builds

**File:** `src/c47/core/freeList.c`, `freeListFree`.

`freeListFree` has no validation that the range being freed is disjoint from
the existing free regions before it mutates the list:

- The "Memory freeing A/B" diagnostics (region bookkeeping) are wrapped in
  `#if !defined(DMCP_BUILD)` — **compiled out entirely on hardware** — and
  even on PC builds they only print: after reporting, the function falls
  through and mutates the free list anyway.
- The merge/insert logic below matches exact adjacency
  (`freeMemoryRegions[i].blockAddress == addr`, or region end ==
  `C47RamPtr`) and otherwise inserts a new region. A double free or an
  overlapping range therefore lands as a duplicate/overlapping free region;
  subsequent allocations can hand the same blocks out twice, corrupting
  whatever lives there (registers, programs, the stack — they share this
  arena).

That this class is live — not theoretical — is shown by this cycle's own
history: two *caller-side* double frees were found and fixed recently
(e.g. "Fix memory issue - double free": `DELall` freeing the saved
statistical sums block, config.c). Each such caller bug silently corrupts
devices in the field until someone traces it; an allocator-side guard turns
the entire class into a loud, non-corrupting reject at the moment of the
bad call.

**Suggested fix:** an unconditional range-overlap rejection at the top of
`freeListFree` — never mutate the list when the incoming range intersects an
existing free region; report on PC builds, silently refuse on DMCP:

```c
for(i = 0; i < numberOfFreeMemoryRegions; i++) {
  uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
  uint32_t rEnd   = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
  if((uint32_t)C47RamPtr < rEnd &&
     (uint32_t)C47RamPtr + (uint32_t)sizeInBlocks > rStart) {
    /* PC builds: errorf + backtrace here */
    return;   /* a double free is a caller bug; the list must survive it */
  }
}
```

We have been running exactly this guard in production in our fork (it also
carries PC-side diagnostics with a backtrace) with no regressions; happy to
submit it as an MR if you want it.
