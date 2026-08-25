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
  independence (ring address outside the pool, capture costs the pool
  nothing), **R9** capture purity (display/math/mode surface byte-identical
  across a capture, calcMode included — the window opens BEFORE
  saveForUndo so a one-shot latch cannot escape it), **R10** the
  TMP_STR_LENGTH render contract for the formatters U2's lazy preview will
  call (sentinel-guarded worst cases; complex34ToDisplayString deliberately
  excluded — display context only). X returns the failure count; the .txt
  asserts `RX=LonI:"0"`.
- `historyTestSequence` — script-from-X sequencer: unsigned integer tokens
  enter values with the closeNim ordering (saveForUndo, lift, write), any
  other token executes by item catalog name through `reallyRunFunction`,
  which is the real capture/label path. Cases cover single UNDO, REDO via
  the live anchor, redo-tail truncation ("UNDO 5 + REDO" must hold, not
  resurrect the dropped anchor), three-deep walk both ways, HCLR leaving
  the single-level buffer intact, and SS=8.

Both drivers are `PC_BUILD`-only and start from `historyTestBaseline()` +
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
| S1 | `malloc` → `allocC47Blocks` in `historyEnsureRing` | ring battery R8 (ring address inside the pool) |
| P1 | reintroduce a capture-time preview with the historical slip — `sizeof` of a pointer passed as the formatter's string budget | ring battery R9, locally: "wrote into errorMessage" + "switched calcMode" (the silent displayBugScreen class caught at the capture) |

R10 carries no mutation pin: it pins an **upstream** contract, not package
code — if it ever goes red, TMP_STR_LENGTH stopped being enough and the U2
render plan needs revisiting, which is precisely the alarm it exists to
raise.

Run results are recorded in the stage commit message, not here.
