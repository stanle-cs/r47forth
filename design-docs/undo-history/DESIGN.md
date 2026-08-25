# undo-history — DESIGN.md

Authoritative for the `packages/undo-history/` package: multi-level undo plus
(stage U2) a history browser, written as an upstream-submittable feature
patch. `DESIGN-HISTORY.md` is the non-normative amendment trail; `TESTING.md`
covers the test harness; `UPSTREAMING.md` lists what an upstream MR adds
beyond the package. The upstream conventions ruling (Stan, 2026-08-24) binds
everything here: real upstream file layout and naming, no package-side idioms
in patched code, submission-ready patch content.

## 1. Model

Upstream undo is a single SAVED_* buffer (`saveForUndo()` / `undo()` in
stack.c) and one bool. This package layers a **ring of complete
pre-operation states** on top, reusing the single-level machinery at both
ends:

- **Capture**: `saveForUndo()` calls `undoHistoryCapture()` on its success
  tail. The entry is serialized **from the SAVED_* area** the function just
  committed — live registers are never read, and capture contexts are
  inherited from upstream's own gates (saveForUndo's mode guard,
  reallyRunFunction's US_ENABLED / not-PGM_RUNNING / not-SOLVING/INTING
  gate). The package adds no capture policy of its own.
- **Restore**: every navigation (deeper UNDO, REDO, browser restore) stages
  the chosen entry back into SAVED_* (`reallocateRegister` + payload copy +
  flags/lr/sums staging), sets `thereIsSomethingToUndo`, and calls the
  existing `undo()` — SIGMA fixups, solver-flag reconciliation and
  entry-status bits stay upstream's single source of truth.

Entries are **complete states** (stack X..top per the entry's own SSIZE
flag, L, systemFlags, lrSelection/lrChosen, optional statistical sums, a
label item). A skipped capture (oversized state, see
§3) therefore only removes granularity — navigation across the gap restores
a complete, correct state.

## 2. Cursor algebra

`historyCursor` = NONE while live, else the index of the entry most recently
made live by navigation.

- `fnUndo`, single-level branch (`thereIsSomethingToUndo`):
  `undoHistoryNoteFirstUndo()` pushes the **live** state as a LIVEANCHOR
  entry (so REDO can return to the pre-undo point), then upstream `undo()`
  runs. Cursor lands on the entry mirroring SAVED_* if it is in the ring
  (`seq == historyLastCaptureSeq`), else NONE.
- `fnUndo`, ring branch (nothing in the single-level buffer):
  `undoHistoryStepBack()` restores cursor−1 (or the newest entry when
  cursor is NONE). At the oldest entry it holds position.
- `fnRedo`: restores cursor+1; holds position at the top. A REDO that lands
  on the anchor is "back to now".
- New capture while cursor is active: entries newer than the cursor are
  truncated (the redo tail dies — standard undo-branch semantics), then the
  new entry is pushed. The capture immediately after a restore reproduces
  the cursor entry byte-for-byte and **merges** (dedupe) instead of pushing;
  a merged anchor loses its LIVEANCHOR flag and becomes a plain state.

## 3. Storage

- **One resident `allocC47Blocks` block, armed at RESET, never freed**
  (`historyRing`, 1024 blocks = 4 KiB on new hardware, 256 = 1 KiB on old;
  `RAM_SIZE_IN_BLOCKS == RAM_SIZE_IN_BLOCKS_NEW_HW` discriminates, DM42
  stays best-effort per the 2026-07-15 ruling) — pool-native per the
  2026-08-25 convention ruling, the forth-core gdict shape. Two measured
  laws govern it (matrix.txt RCL58, whose QR workspace requests one
  contiguous chunk of 29,820 blocks against ~31,236 available vanilla —
  free-list dump in DESIGN-HISTORY):
  1. **Total resident size is the law**: every resident block anywhere
     below the pool's top run shrinks that run one-for-one, so the
     firmware-wide budget for ALL resident pool allocations is the
     ~1,400-block (5.6 KiB) vanilla slack. The ring takes 1,024 of it;
     8 KiB variants of every shape (slab, register banks, per-level
     blocks, with and without a yield-under-pressure retry in memory.c)
     were built and measured red.
  2. **Arming time shapes the scraps**: the reset-time allocation (right
     after doFnReset rebuilds the free list) sits at the pool's low edge
     beside the boot structures; a mid-session allocation lands between
     live churn. Lazy arming is therefore forbidden (pin S1) and a state
     restore re-arms via the saveRestoreBackup tail hook.
  Arena-arming failure (a nearly-full restored pool) just disables capture
  until the next reset — history is best-effort.
- Linear compacting layout: entries contiguous ascending, oldest evicted by
  memmove; an offset directory (`historyEntryOffset[48]`) gives indexed
  access. Entries are 8-byte rounded; register payload slack is zeroed so
  dedupe can byte-compare.
- Per-entry cap: arena/4. Larger states are **skipped**; the next stored
  entry carries GAPBEFORE (the browser renders a gap mark).
- Serialization recipe mirrors `copySourceRegisterToDestRegister`: per
  register record = slot, dataType, tag, the `reallocateRegister` size
  argument, payload length, payload bytes. Restore is
  `reallocateRegister(SAVED_*, …)` + copy + `setRegisterTag`, aborting
  cleanly on ERROR_RAM_FULL with the ring untouched.
- **Capture is pure serialization — no display formatting, ever.** Three
  measured reasons (see DESIGN-HISTORY and the UPSTREAM_REPORTS_* files):
  the `*ToDisplayString` family assumes TMP_STR_LENGTH buffers and one
  member smashes smaller stack buffers without any check; another member
  validates its length but fails through `displayBugScreen`, which
  **silently switches calcMode** in headless builds — a capture-time
  formatting failure therefore changes machine state mid-operation and
  surfaces as wrong results arbitrarily far away (measured: a wrong SPIRAL
  program result three test files after the trigger); and ROUND/RSD
  genuinely compute through the global `displayValueX`, so display code is
  not side-effect-free by design. Battery case R9 fences the enumerable
  surface (calcMode included) around a capture; R10 pins the
  TMP_STR_LENGTH contract the U2 render path will rely on. Entry previews
  for the U2 browser are formatted **lazily at render time**, staging the
  serialized X record into TEMP_REGISTER_1 — registerBrowser's own scratch
  pattern — in display context where these functions are designed to run.

## 4. Gates (load-bearing)

- `undoHistoryUserContext()` = `programRunStop != PGM_RUNNING &&
  !FLAG_SOLVING && !FLAG_INTING`, mirroring reallyRunFunction's capture
  gate. Upstream calls `fnUndo(0)` internally with nothing to undo
  (solve.c:129/248, integrate.c:232, graph.c:2638); without the gate, the
  ring branch would swap states mid-computation.
- `undoHistoryNoteFirstUndo()` additionally requires
  `lastErrorCode == ERROR_NONE`: upstream also uses
  `saveForUndo()`/`fnUndo(0)` as an error-rollback transaction
  (random.c:138 fires right after `displayCalcErrorMessage`), which must
  not mint a live anchor. Residual exposure: an internal rollback with a
  clean error code would still anchor — bounded to one stray entry,
  truncated by the next capture.
- Items REDO (428) and HCLR (429) are `US_UNCHANGED` — navigation and ring
  management must never trigger a capture of their own.

## 5. Lifecycle

Session-local by design (v1): `doFnReset` calls `undoHistoryReset()` —
counters forgotten and the ring block re-armed from the freshly rebuilt
pool (low-edge placement; see §3) — and the saveRestoreBackup restore tail
calls it again so a state load forgets the replaced pool's block and
re-arms from the restored one. HCLR (`fnHistoryClear`) empties the ring
but keeps the block resident and **leaves the single-level buffer intact**
(class-tested). Backup-file persistence of the ring is possible future
work for the upstream MR, not v1.

## 6. Composition claims (binding for other packages)

- Claimed upstream resources: item rows **428 ("REDO"), 429 ("HCLR")**
  (spare CAT_FREE rows, far from forth-core's hunks at 213/2842-2843);
  stage U2 will claim **427 ("U.HIST")** and calcMode **19**
  (CM_HIST_BROWSER).
- testSuiteList.txt entry `undo_history` is anchored after `nested_cov`,
  mid-file — forth-core appends at EOF; sharing that hunk context would be a
  guaranteed apply conflict.
- testSuite.c hunks (driver declarations after `covAmortNext`, table rows
  after `fnSettingsDispFormatGrpR`) sit ≥4 lines from forth-core's hunks at
  the covHashBmp and fnWho anchors for the same reason.
- The combined gate (`build-test.sh`, second pass:
  `CUSTOM_PKG=packages/forth-core,packages/undo-history`) is the proof; any
  drift fails loudly at patch-apply time by design.

### Depth that follows from the budget

4 KiB holds roughly 20 shallow levels (a real34 stack entry serializes to
~176 bytes); matrices and long strings consume more and self-limit through
the per-entry cap (arena/4). This is the honest capacity the RCL58 headroom
allows; widening it is an upstream conversation (bound the QR workspace —
see the eigen report), not a package knob.

## 7. Upstream findings (reported, not patched)

Three defects/fragilities in unrelated upstream code were found and
root-caused during bring-up. Per the 2026-08-25 ruling they are addressed
**without modifying upstream code unrelated to this feature**: each has a
package-side structural/class test plus a paste-ready report —
`UPSTREAM_REPORTS_eigen_pool_fragmentation.md` (guarded by R8 and the
suite's own RCL58), `UPSTREAM_REPORTS_displayBugScreen_headless.md`
(guarded by R9), `UPSTREAM_REPORTS_toDisplayString_buffer_contract.md`
(pinned by R10).

## 8. Upstream patch surface (8 files)

stack.c (capture tail + fnUndo two-branch), items.c (label line, two rows,
two generator stubs), items.h (two renamed spare defines), c47.h (one
include), config.c (doFnReset reset line — which also arms the ring),
saveRestoreBackup.c (one restore-tail line: forget + re-arm from the
restored pool; anchored 13 lines from forth-core's hook there),
testSuite/testSuite.c (driver declarations + two coverageDriver rows),
testSuite/tests/testSuiteList.txt (one anchored line). New files:
undoHistory.c/.h (+ browsers/historyBrowser in U2). All patch content is submission-ready upstream code; comments
explain invariants in upstream's own voice, and no package markers are used.
