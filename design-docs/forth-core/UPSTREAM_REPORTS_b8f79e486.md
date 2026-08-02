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

---

### §3 — upstream response and resolution (2026-07-19)

**Upstream's ruling (quoted):**

> I disagree with the analysis in point 3 regarding freeListFree. If there
> is an overlap in the region we want to free, there is a bug, either
> directly in the caller or perhaps one that occurred much earlier. So
> whether the list of free blocks survives is really not important, because
> memory will start to become corrupted from that point on anyway, and no
> one knows how it will progress.
>
> What we can do is alert the user to a significant memory management issue
> and ask him to perform a hardware reset, and try to reproduce the problem
> and then report it. Pauli quite wisely said: It's not nice for the user
> but it's better than continuing and getting who knows what trouble later.
>
> All of these memory management issues are easily identified and
> highlighted by the simulator with yellow messages on the console.

**Our position: ACCEPTED.**  The doctrine is correct — once an overlap is
detected an earlier invariant already broke, and preserving the free list
does not un-corrupt anything; continuing risks plausible-looking wrong
results, the worst failure mode for a calculator.  Our "never mutate, keep
going" was defense-in-depth that optimized for allocator survival when the
honest condition is "state untrustworthy, halt."

**Key point of agreement (the substance of the original report survives).**
Upstream's preferred behavior — "alert the user and ask for a hardware
reset" — REQUIRES the detection scan to run ON THE DEVICE.  That is exactly
what §3 flagged as missing: the landed "Memory freeing A/B" diagnostics are
`#if !defined(DMCP_BUILD)`, so today the overlapping region is inserted
SILENTLY on hardware.  So the disagreement is only about the RESPONSE half
(continue vs. halt); the DETECTION half is precisely what upstream's
preferred alert needs.  Detection stays; silent-continue goes; response
upgrades to fail-loud via `displayBugScreen` (upstream's own
internal-fault mechanism — unconditional device + PC, non-blocking).

**Resolution.**  The fork guard is reworked to the fail-loud form:
detection scan unchanged, `displayBugScreen(...)` in place of the
PC-only print block, backtrace block dropped (upstream's simulator
diagnostics cover PC-side triage).  Net ~32 → ~14 lines.  Authored as
`QWEN_PROMPTS_FIX6_bugscreen.md` (FIX-6B); the four affected self-tests
re-pin to the new contract (list still unchanged AND
`calcMode == CM_BUG_ON_SCREEN`).  The Step-8 MR now offers this version.

**Open question deferred to upstream (call-context).**  `freeListFree`
runs inside allocation/restore paths — e.g. the historical `DELall`
double free sits in a config.c restore that may reset `calcMode`
afterward, which could SWALLOW an immediately-raised bug screen.  Upstream
knows the call graph; the MR flags whether an immediate raise is
acceptable everywhere or whether a latched raise (set a fault flag,
surface at the next idle/refresh) is safer.  Our tree implements the
immediate raise (the self-tests call `freeC47Blocks` at top level, where
it is observable); the mechanism choice is upstream's to finalize.

**Pasteable reply (for the MR thread):**

> Agreed, and thanks — you've convinced me on the halt-vs-continue point.
> Preserving the free list after an overlap is detected doesn't buy
> anything real: the corruption already happened upstream of the free, so
> "survive and keep going" just defers an unpredictable failure. Halting
> loudly is the right call.
>
> One thing worth keeping from the original note, though: your preferred
> behaviour — alert the user, ask for a hardware reset — needs the overlap
> *detection* to run on the device, and right now it doesn't. The "Memory
> freeing A/B" diagnostics are all under `#if !defined(DMCP_BUILD)`, so on
> hardware the overlapping region is inserted silently and no alert ever
> fires. So I think we actually want the same thing: keep the detection,
> drop the silent-continue, and make the response your bug screen.
>
> Reworked patch (replaces the version I floated): the unconditional
> range-overlap scan stays, and on a hit it calls
> `displayBugScreen("Memory management fault: an overlapping or double free
> was detected.")` and returns without touching the list — no
> `#if DMCP` wrapper, no backtrace block (your simulator's yellow console
> messages already cover PC-side triage). That's ~14 lines, down from ~32,
> and it runs identically on device and PC.
>
> One open question for you, since you know the call graph better: is an
> immediate `displayBugScreen` from inside `freeListFree` safe in every
> caller context? I'm thinking of paths like the `DELall` restore that
> frees the stats block — if an outer operation resets `calcMode`
> afterwards it could swallow the screen. If that's a concern I'm happy to
> latch a fault flag and raise at the next idle/refresh instead; your call
> on which mechanism fits. I'll send whichever form you prefer as the MR.
