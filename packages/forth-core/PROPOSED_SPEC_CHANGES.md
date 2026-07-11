# Proposed spec changes (forth-core)

Non-authoritative. DESIGN.md is read-only for the agent; anything below is a
proposal for a human maintainer to fold into DESIGN.md (or upstream), not a
change already ratified there.

---

## [PROPOSED] Range-overlap double-free guard in `freeListFree` (upstream-MR-ready)

**Affected sections:** DESIGN.md §5 (Memory arena plan), §5.1 (The arena),
§5.4 (Budget & high-water reporting) — the arena/free-list plan describes the
allocator but says nothing about double-free/invalid-free handling; this
proposal fills that gap. Not Forth-specific: `freeListFree` is the shared
C47 allocator used by GMP reals, config.c, register data, and the Forth
dictionary/label-list machinery alike.

**Where it lives today:** `packages/forth-core/core/freeList.c` overrides
upstream `src/c47/core/freeList.c`. The override is currently the minimum
diff possible: one hunk in `freeListFree`, immediately after
`C47RamPtr = TO_C47MEMPTR(pcMemPtr);` and before the existing
`#if !defined(DMCP_BUILD)` diagnostic block. [VERIFIED:
packages/forth-core/core/freeList.c:205-234]

**Problem:** upstream `freeListFree` has no defense against a double free or
an invalid free. On PC-simulator builds a double free of an *exact* address
already produced misleading diagnostics ("Memory freeing B: never allocated
at this address") because the address had already been removed from
`allocatedMemoryRegions[]` by the first free — but the function still went
on to *insert the region into the free list a second time*, corrupting it
(two overlapping or duplicate `freeMemoryRegions[]` entries). On DMCP
(device) builds, all of that diagnostic code is compiled out
(`#if !defined(DMCP_BUILD)`), so the same call corrupts the free list with
zero warning — silent heap corruption on the calculator itself, the exact
class of bug a flash/RAM-constrained device build can least afford.

An interim fix constrained the standing overlap detector at the bottom of
`freeListFree`/`freeListReduce` from `>=` to `>` to suppress its own noise,
which weakened it from "any adjacency between free regions is an error"
(correct — regions can't be legitimately adjacent post-coalesce) to "only
strict overlap is an error," silencing the exact signal double-frees
produce. That interim guard also only matched an exact
`blockAddress == C47RamPtr`, missing a double free of an address that had
since coalesced into the *interior* of a larger free region, and on a match
it could *grow* the matched free region to the double-freed size — turning
a detected error into region growth over whatever adjacent memory (possibly
still allocated) that growth covered.

**Proposal:** replace the guard with a range-overlap check that:
1. Runs unconditionally (before any `#if !defined(DMCP_BUILD)` gate), so
   device builds get the same protection as the simulator.
2. Checks `[C47RamPtr, C47RamPtr+sizeInBlocks)` against every existing
   `freeMemoryRegions[]` entry for interval overlap (not exact-address
   match), so a double free of an address that has since coalesced into a
   larger region is still caught.
3. Never mutates `freeMemoryRegions[]` on a hit — just logs (PC builds only,
   via `errorf`/`fprintf(stderr, ...)` plus a backtrace) and returns. A
   double free is always a caller bug; the free list must survive it
   unchanged rather than have the caller's mistake "absorbed" by growing or
   duplicating a region.
4. Restores the overlap detector at the bottom of `freeListFree` and
   `freeListReduce` to `>=` (its original upstream form/comment), since
   adjacent-but-not-yet-coalesced free regions can no longer occur once the
   guard above prevents the duplicate insertion that caused them.

Current override diff (guard hunk only — `diff src/c47/core/freeList.c
packages/forth-core/core/freeList.c` shows nothing else):

```c
  C47RamPtr = TO_C47MEMPTR(pcMemPtr);

  // Double-free / invalid-free guard (FIX-6): reject any free whose range
  // overlaps an existing free region. Runs unconditionally so device builds
  // (DMCP, diagnostics compiled out) cannot insert a duplicate/overlapping
  // region and corrupt the list. Never mutates the list: a double free is
  // always a caller bug and the free list must survive it unchanged.
  for(i=0; i<numberOfFreeMemoryRegions; i++) {
    uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
    uint32_t rEnd   = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
    if((uint32_t)C47RamPtr < rEnd && (uint32_t)C47RamPtr + (uint32_t)sizeInBlocks > rStart) {
      #if !defined(DMCP_BUILD)
        errorf("---->Memory freeing C (double/invalid free):");
        fprintf(stderr, "%zd blocks at address %" PRIu16 " overlap free region [%" PRIu32 "..%" PRIu32 ")\n",
                sizeInBlocks, C47RamPtr, rStart, rEnd);
        #if !defined(WIN32)
          { void *callstack[128];
            int frames = backtrace(callstack, 128);
            char **strs = backtrace_symbols(callstack, frames);
            printf("%30s%42s%s\n\n\n", "", "freeListFree called from: ", strs[1]);
            for(int f = 1; f < frames; f++) {
              printf("%30s%42d: %s\n", "", f, strs[f]);
            }
            free(strs);
          }
        #endif
        fflush(stderr);
      #endif
      return;
    }
  }
```

**Test coverage** (`packages/forth-core/test_dict_reloc.c`, FIX-6 section):
`test_freelist_double_free_guarded` (exact-address double free),
`test_freelist_interior_double_free` (double free of an address coalesced
into the interior of a larger region), `test_freelist_no_mutation_on_oversize_free`
(double free with a larger size than originally allocated must not grow the
region). All three assert the free list is byte-for-byte unchanged and that
`test_freelist_consistent()` still passes afterward; each was verified to
FAIL under its stated escaping mutation and PASS once reverted.

**Root-cause note (Step 4 of this task):** rerunning the full self-test
suite with the corrected guard surfaced one real (forth-core-only, not
upstream) double free: `test_xeq_precedence()` in `test_dict_reloc.c`
restored a pre-`reallocC47Blocks` snapshot of `labelList`/`numberOfLabels`
after the realloc had already freed that snapshot's block internally
(`freeListRealloc` frees the old pointer unconditionally on success —
[VERIFIED: packages/forth-core/core/freeList.c:90]), so the next
`scanLabelsAndPrograms()` call freed it a second time. Fixed by rescanning
from (unmodified) program memory instead of restoring the stale
pointer/count — see `test_dict_reloc.c:1319-1333` (failure branch) and
`:1339-1353` (success path). This does
**not** implicate upstream: the bug was entirely in a forth-core test
helper's own restore logic, not in any upstream code path.

---

## [PROPOSED] Corrected file:line citations invalidated by Prompts 1-4

DESIGN.md is read-only for this agent, so these corrections are staged here
for a human/design-side edit rather than applied directly. All four sites
below cite `packages/forth-core/` files; every citation to an upstream
`src/c47/` file was checked and is unaffected (Prompts 1-4 never touch
upstream, so no upstream line number can have drifted).

Method: grepped DESIGN.md for every `packages/forth-core/*.c:NNNN` /
`test_dict_reloc.c:NNNN` citation, then diffed the cited range's expected
content (from the citing prose) against the file's current content at that
line range. Four citations, spread across three DESIGN.md locations, no
longer match; all four independently point at the same two tests, both in
`test_dict_reloc.c`, which have moved because of the many tests Prompts 1-4
added earlier in that file. No other `packages/forth-core/` citation in
DESIGN.md was found to be stale — in particular, none of Prompts 1-4's edits
to `programming/manage.c`, `core/freeList.c`, `keyboard.c`, or `softmenus.c`
invalidate anything, because DESIGN.md never cites those override files by
line number (it cites their `src/c47/` upstream counterparts instead, or the
override at all — see item 4 below, which cites no line number today).

1. **DESIGN.md:620** (§3.2 ASLIFT-on-exit discussion): currently reads
   `` `test_dict_reloc.c` (~310-325) currently asserts clear-on-exit ``.
   Lines 310-325 are now mid-way through an unrelated 0BR test. The ASLIFT
   stack test (`test_stack_aslift`) is now at
   **[VERIFIED: test_dict_reloc.c:124-148]**. Correction: replace
   `(~310-325)` with `(~124-148)`.

2. **DESIGN.md:1563** (§3.3-C amendment table, row C2): currently cites
   `test_dict_reloc.c:958-980` for "hand-assembled self-test body updated in
   the same commit" (the `FTOK_C47`/`PTP_NUMBER_8` padded-dispatch test).
   Lines 958-980 are now inside `test_ilit_compile_interpret_parity`, an
   unrelated test. The actual test (`test_c47_ptp_number8_padded`) is now at
   **[VERIFIED: test_dict_reloc.c:460-489]**. Correction: replace
   `test_dict_reloc.c:958-980` with `test_dict_reloc.c:460-489`.
   (`forth_inner.c:162-163` in the same row is untouched by Prompts 1-4 and
   remains accurate.)

3. **DESIGN.md:1565** (§3.3-C amendment table, row C4): currently cites
   `test_dict_reloc.c:310-325` for "flip the clear-on-exit assertion in stack
   test c" — the same stale range as item 1, same corrected range
   **[VERIFIED: test_dict_reloc.c:124-148]**. Correction: replace
   `test_dict_reloc.c:310-325` with `test_dict_reloc.c:124-148`.
   (`forth_inner.c:78-82` in the same row is untouched and remains accurate.)

4. **DESIGN.md:2086-2092** (§9.10 item 4, F4 resolution): the current text
   (ratified 2026-07-11) says the fix landed "in the overridden
   `packages/forth-core/programming/manage.c`" but cites no line number — the
   prior wording's `:1863` citation was dropped when this item was reworded
   and never replaced. The fix (`func & 0xff`, with the `audit F4` tag
   comment) is at **[VERIFIED: packages/forth-core/programming/manage.c:1879]**
   today. Correction: append `:1879` to that sentence (e.g. "...in the
   overridden packages/forth-core/programming/manage.c:1879.").
   Line-number citations into an actively-edited override file are inherently
   fragile — worth flagging that this is the second time this exact citation
   has drifted (1863 → dropped → 1879); a future edit to
   `insertUserItemInProgram` will invalidate it again. Citing the anchor
   comment (`audit F4`) alongside the line number, as the code itself already
   does, would survive re-numbering better than the line number alone.
