# undo-history — TESTING.md

## Harness

One harness: **upstream's testSuite**, reused whole. The package registers
two `coverageDriver` functions (no indexOfItems row, dispatched by name —
the forthTestRunFromX pattern) in testSuite.c and ships
`testSuite/tests/undo_history.txt`:

- `historyTestRing` — in-driver battery against the ring internals (same
  translation unit): round-trip restore per type, dedupe, eviction
  offset/size coherence, branch truncation + merge-after-restore, oversized
  skip + GAPBEFORE, the fnUndo/fnRedo cursor walk incl. both no-op ends,
  the two gates (FLAG_SOLVING exclusion; error-rollback fnUndo minting no
  anchor — the RANI# class), and the three finding guards: **R8** storage
  residency (the ring is one pool block armed at reset BEFORE any capture
  — lazy arming breaks RCL58, measured; a capture costs the pool nothing;
  because undo_history.txt runs after serialize_cov's backup-restore
  cycle, the armed-assert also proves the saveRestoreBackup re-arm hook),
  **R9** capture purity (display/math/mode surface byte-identical across a
  capture, calcMode included — the window opens BEFORE saveForUndo so a
  one-shot latch cannot escape it), **R10** the TMP_STR_LENGTH render
  contract for the formatters U2's lazy preview will call (sentinel-guarded
  worst cases; complex34ToDisplayString deliberately excluded — display
  context only). X returns the failure count; the .txt asserts
  `RX=LonI:"0"`. matrix.txt RCL58 is the standing integration gate for the
  resident-size budget.
- `historyTestSequence` — script-from-X sequencer: unsigned integer tokens
  enter values with the closeNim ordering (saveForUndo, lift, write), any
  other token executes by item catalog name through `reallyRunFunction`,
  which is the real capture/label path. Cases cover single UNDO, REDO via
  the live anchor, redo-tail truncation ("UNDO 5 + REDO" must hold, not
  resurrect the dropped anchor), three-deep walk both ways, HCLR leaving
  the single-level buffer intact, and SS=8.

- `historyTestBrowser` — the browser's state machine, driven through the
  same functions the keyboard dispatch calls: entry (mode switch, previous
  mode kept, selection on the newest level), clamped navigation both ways,
  ENTER-restore with cursor placement and browser exit, reopen-at-cursor,
  the empty browser, and preview staging into TEMP_REGISTER_1. The entry
  path runs `refreshScreen` for real, so the render executes headless on
  every suite run; pixel truth comes from the run-sim capture at review.

The drivers are `PC_BUILD`-only and start from `historyTestBaseline()` +
stack clear so every .txt block is self-contained (the suite runs in one
process; ring state must not leak between blocks).

**Gate**: `./packages/undo-history/build-test.sh` — refresh both packages,
then per pass: meson reconfigure, ninja, `meson test testSuite`, status AND
banner both required. Pass 1 solo, pass 2 combined with forth-core
(composition proof). The forth self-test battery is forth-core's own gate,
not this one.

## Mutation pins (red-first discipline, per forth-core TESTING.md)

Each named mutation must go red in the listed case, gate green between
restores:

| # | Mutation (undoHistory.c) | Red case |
|---|--------------------------|----------|
| M1 | drop `historyTruncateAbove` call in `undoHistoryCapture` | sequence "3 4 + UNDO 5 + REDO" (stale anchor {7} resurrected) |
| M2 | drop `lastErrorCode == ERROR_NONE` in `undoHistoryNoteFirstUndo` | ring battery R7 (error-rollback mints an anchor) |
| M3 | drop `undoHistoryUserContext()` from fnUndo's ring branch (stack.c) | ring battery R7 (FLAG_SOLVING walk) |
| M4 | `historyCursor - 1` → `historyCursor + 1` in `undoHistoryStepBack` | ring battery R6 / sequence three-deep walk |
| M5 | drop `setRegisterTag` in `historyRestoreToIndex` | ring battery R1 (tagged real34 round-trip) |
| S1 | lazy arming: allocate the ring in `historyEnsureRing` instead of at reset | ring battery R8 (ring must be armed at reset, before any capture) |
| P1 | reintroduce a capture-time preview with the historical slip — `sizeof` of a pointer passed as the formatter's string budget | ring battery R9, locally: "wrote into errorMessage" + "switched calcMode" (the silent displayBugScreen class caught at the capture) |

| V1 | drop the clamp in `historyBrowserUp` | browser battery B2 (ups must clamp at the newest) |
| V2 | drop the leave in `historyBrowserEnter` | browser battery B3 (ENTER must leave the browser) |
| F1 | wrong flag in the SYSFL row 2299 item | browser battery B7 (the SFL_MONIT offset arithmetic) |
| F2 | drop the flag check in `undoHistoryKeyReroute` | browser battery B7 (reroute must be off while the flag is clear) |

R10 carries no mutation pin: it pins an **upstream** contract, not package
code — if it ever goes red, TMP_STR_LENGTH stopped being enough and the U2
render plan needs revisiting, which is precisely the alarm it exists to
raise.

Run results are recorded in the stage commit message, not here.
