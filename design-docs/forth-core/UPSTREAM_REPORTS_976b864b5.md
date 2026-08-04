# Upstream report — one finding verified at 976b864b5

**Version:** `00.109.03.04a0.int` (`VERSION1`, src/c47/defines.h:12), commit
`976b864b5` (forth-core branch — the finding is in general save/restore
machinery, not Forth-specific code, so it should reproduce on any tree with
an ephemeral, block-allocator-backed buffer that is intentionally excluded
from the persisted save-file state; see "Reproduction" below for how this
was actually exercised). Verified against the GTK simulator build,
2026-07-20. Code-inspection + simulator evidence (including a decoded
`addr2line` backtrace pinning the exact call site).

Paste as its own issue.

---

## `restoreCalc()`'s wholesale allocator-bookkeeping restore can silently
## leak any allocation whose lifetime is intentionally shorter than a
## save/restore cycle

**Files:** `src/c47/saveRestoreBackup.c` (`restoreCalc`), interacting with
`src/c47/core/freeList.c` (`freeListFree`, `freeListAlloc`).

### The mechanism

`restoreCalc()` restores the block allocator's own bookkeeping — the
free-region and allocated-region tracking arrays — **wholesale** from the
backup file, independent of anything else happening in the same restore:

- saveRestoreBackup.c:832 — `restoreStateValue(ram, TO_BYTES(RAM_SIZE_IN_BLOCKS), "ram", "hexDump")` overwrites the entire RAM arena's raw bytes with the file's saved snapshot.
- saveRestoreBackup.c:835-836 — `restoreStateValue(&numberOfAllocatedMemoryRegions, ...)` and `restoreStateValue(allocatedMemoryRegions, ...)` overwrite the allocator's own "what's allocated" bookkeeping with the file's saved snapshot, taken at the moment `saveCalc()` last ran.

This assumes every live allocation at save time is meant to persist across
the restore. That assumption is not universally true: a module can hold a
block-allocator-backed buffer that is *intentionally* not part of the
persisted state at all — no `saveStateValue`/`restoreStateValue` calls for
it anywhere — because its lifetime is meant to be strictly shorter than a
save/restore cycle (e.g., freed unconditionally by a reset hook before any
restore-visible state is touched). When that's the case, the wholesale
bookkeeping restore reintroduces the module's block as "allocated" in the
freshly-restored tracking arrays — even though the module itself has
already forgotten the pointer (having freed and NULLed it, correctly, per
its own lifecycle rules) and will never ask to free it again. The block is
permanently orphaned: still counted as allocated, forever, with nothing
left anywhere that could free it short of a full RESET.

This is not a double-free and does not trip `freeListFree`'s (existing)
overlap guard — nothing ever attempts a second free of that address. It is
a silent, wholesale-restore-shaped bookkeeping leak: the file's stale
snapshot simply overrides the correct, already-cleaned-up live state.

### Reproduction

Verified via a small Forth-specific module (`packages/forth-core/`) that
happens to have exactly this shape: a 64-block scratch buffer
(`forthCap`, `packages/forth-core/forth_capture.c`) allocated on demand
and explicitly guaranteed closed at two lifecycle seams
(`forthDictInit()`/`forthDictClear()`), deliberately never included in any
`saveStateValue`/`restoreStateValue` pair — because the design intends it
to never survive a real save/restore (see `forth_capture.c`'s own doc
comment above `forthCapPowerReset()`).

Steps: with the buffer allocated and open, call `saveCalc()`, then
`restoreCalc()` (a direct, in-process backup-file round-trip, no power
cycle involved — same file-based mechanism a real power-off/power-on uses).
Before the restore's own bookkeeping-array overwrite runs, the module's
reset hook (invoked from `doFnReset()`, which `restoreCalc()` calls
unconditionally as its first statement) correctly frees the buffer and
NULLs its own pointer. Immediately after, `restoreCalc()`'s file overlay
(lines 832-836 above) restores `numberOfAllocatedMemoryRegions`/
`allocatedMemoryRegions[]` from the file — which still shows the buffer's
64 blocks as allocated, because that was true at `saveCalc()` time. Result,
measured via `getFreeRamMemory()` before opening the buffer vs. after the
full round-trip: **256 bytes (64 blocks) permanently missing**, every
single time, regardless of any other fix applied to the module's own
reset-hook timing (a separate, narrower issue — a reset-hook-ordering fix
that eliminates a related false-positive double-free diagnostic during the
same sequence has already landed locally and does *not* affect this
number; see below).

### Why this isn't just "that module's bug"

The module in question already does everything correctly on its own terms
— it frees its own buffer at the right time, via the right mechanism, and
never touches restore-owned state directly. The gap is structural: nothing
in the wholesale bookkeeping restore has any way to know that one of the
"allocated" entries it's about to reinstate belongs to state a module has
already, correctly, and intentionally torn down before the restore
completed. Any future module with this same shape — an ephemeral,
allocator-backed scratch buffer explicitly excluded from the save format —
would hit the identical leak, deterministically, on every save/restore
cycle it's open across. On real hardware this means every power-off/
power-on cycle (or explicit backup/restore) performed while such a buffer
is allocated permanently consumes a slice of RAM with no way to reclaim it
short of a full RESET.

### Suggested directions (not proposing a specific patch yet)

Two shapes seem plausible, and picking between them is a genuine design
call rather than a one-line fix, which is why this is being raised for
discussion rather than shipped as an MR outright:

1. **Don't persist ephemeral allocations at all.** If a module's buffer is
   never meant to survive a restore, its blocks shouldn't appear in the
   *saved* bookkeeping snapshot either — e.g., the allocator could support
   a class of allocation that's excluded from `saveCalc()`'s own bookkeeping
   dump, so the file never claims it was allocated in the first place.
2. **Reconcile at restore time.** After the file overlay restores the
   allocated-region array, give modules with a reset hook a *second* pass —
   after the overlay, not just before — so a hook can detect "the restored
   bookkeeping claims a block I no longer own" and correct it. This is more
   invasive (needs a defined ordering/contract for reset hooks relative to
   the bookkeeping restore) but doesn't require touching the save format.

Happy to trace either direction further and submit a concrete patch once
there's a preference — this report is intentionally scoped to the finding
and its mechanism, not a specific fix, since (unlike the FIX-6 finding)
the right shape of fix depends on a call you're better positioned to make
about the save-format/restore-contract tradeoffs involved.

---

## Re-verification at 26ec91634 (2026-08-03, post-migration) — internal status, not part of the paste

**The mechanism is intact.** `restoreCalc()` at `26ec91634` still calls
`doFnReset(CONFIRMED, loadAutoSav)` as its first act (saveRestoreBackup.c:830)
and still overlays `ram`, `freeMemoryRegions`, and `allocatedMemoryRegions`
wholesale from the file afterward (:900-917). The migration window's restore
hardening (`f208a3727` region-count validation, `8b408019a`/`f53b62a06`
bounded hexDump reads, `38605170d` pool-pointer range checks) is all
file-validity work: a well-formed file that honestly records an ephemeral
allocation as live at save time passes every new check and still reinstates
it as allocated after the module's reset hook freed it. Nothing addresses
the stale-bookkeeping shape.

**Our reproducer no longer exists — by our own construction, not by an
upstream fix.** The S3 cleanup (DESIGN-HISTORY 2026-08-02) moved the capture
line back onto `aimBuffer`; forth-core no longer holds any block-allocator
allocation whose lifetime is shorter than a save/restore cycle. Measured
2026-08-03 at `26ec91634`: a save/restore round-trip with a capture open
shows `getFreeRamMemory()` delta 0 (was deterministically −256 B at
`976b864b5`).

**Consequence for filing.** The finding stands as a structural observation
(per the DESIGN-HISTORY 2026-08-02 ruling), but the "Reproduction" section
above is historical: it describes a module shape (`forthCap`'s 64-block
scratch buffer) that has since been redesigned away for unrelated reasons.
If filed, present it as a code-inspection finding whose reproduction was
measured at `976b864b5`, and note that any future module with an ephemeral
allocator-backed buffer recreates the leak deterministically. Owner's call
whether it is worth filing without a live in-tree reproducer.
