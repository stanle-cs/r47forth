# R3 — PEM entry design findings for the architect

This file separates executed evidence, source-proved findings, and hypotheses.
No firmware or DESIGN.md file was changed during the review.

## A1 — Bare rendering is contextual, not unambiguous

**Design claim.** A Forth source step renders with no name prefix and no quotes,
avoiding confusion with a string-literal step rendered as `'text'`.

**What the code actually does.** `decodeRem()` copies a non-empty Forth payload
verbatim to `tmpString` (`programming/decode.c`, anchor
`getStringLabelOrVariableName(literalAddress + 1)`). This does distinguish it
from a quoted string literal, but it does not make the listing injective. Exact
counterexamples are easy to construct: Forth payloads `SIN`, `123`, `.END.`,
`»FORTH`, and `FORTH«` render identically to an ordinary zero-parameter RPN
instruction, a numeric literal, the end sentinel, or either marker rendering.
The surrounding marker parity may provide context, but a seven-line PEM window
need not show either marker.

`STD_LEFT_SINGLE_QUOTE` inside a Forth payload is not special to this decoder.
It is copied as payload data, with no delimiter or escaping pass, so it does not
truncate or reclassify the Forth step. That part is clean.

**Why it matters.** The owner can be looking at a valid program line and be
unable to tell whether it is Forth source or the RPN step whose display is the
same. Editing/execution uses encoded bytes and remains deterministic; the cost
is a misleading program listing, not privilege or data-boundary risk.

**Suggested correction.** State explicitly whether PEM listing is intentionally
contextual and non-injective. If yes, say that marker context is the only type
cue and accept that it may be off-screen. If no, specify a distinct Forth-source
visual cue. Do not ask Qwen to choose one.

## A2 — Invalid-opcode hardening needs a recovery contract before a code task

**Lead confirmed at source level.** `src/c47/programming/nextStep.c`, anchor
`uint8_t *findKey2ndParam`, constructs a 15-bit `op` directly from program bytes
and indexes `indexOfItems[op]` without checking it against `LAST_ITEM`. The bytes
`0xFD, 0x01` decode to 32001, matching the reported crashing fixture and far
exceeding the stated `LAST_ITEM == 2870`.

The lead is incomplete as a proposed fix. `programming/decode.c`, anchor
`static void _decodeOneStep`, independently indexes `indexOfItems[op]` with the
same unchecked program-derived value. A bounds check that merely makes
`findKey2ndParam()` return `NULL` does not define what PEM should display and
does not make a malformed listing safe: `fnPem()` continues into decode and has
paths that assume `nextStep` is non-NULL.

**Reachability result.** Within the permitted R3 files, the ordinary PEM editor
serializes known item identifiers and length-matched placeholders; I found no
keypad edit path that emits an out-of-range opcode. That is a bounded source
audit, not an executed proof. The known `op == 32001` case came from the broken
test fixture described in the task. Restore/import code was outside R3's
explicit file budget, so truncated-save and older-format reachability remain
**UNVERIFIED**. I will not turn assumptions about those paths into a
`[VERIFIED:]` claim.

**Suggested correction.** First decide and document one malformed-program
policy shared by the walker, scanner, and renderer: reject the save, truncate at
the first malformed step and write a real `.END.`, or retain bytes and render a
specified diagnostic. Then audit only the restore/import entry points to decide
whether owner-produced state can reach it. If those paths validate opcodes and
lengths, drop this lead as test-fixture-only and spend no production bytes. If
they do not, make one complete task covering walker, scan recovery, renderer,
and the exact malformed-save test; do not assign a one-line bounds check.

## A3 — The documented Forth-picker scan limit does not bound its name buffer

**Claim in code tied to the design.** The `MNU_FORTH` builder says its documented
behavioral deviation is a maximum scan of 1000 program steps
(`softmenus.c`, anchor `if (stepCount > 1000) break`).

**What the code actually checks.** Each distinct definition name consumes a
fixed 15-byte slot at `tmpString + 15 * nNames`, but the insertion arm checks
only `nameLen <= 14`; it never checks `nNames` against `TMP_STR_LENGTH / 15`.
Thus the 1000-step limit is not, by itself, a memory bound. A program with one
distinct short `: name` definition per source step can make `nNames` grow once
per scanned step.

**Status.** This is a source-level capacity defect if `TMP_STR_LENGTH < 15000`;
the fresh debug binary must be queried for the actual array size before treating
it as verified. No Qwen task was emitted because the correct user-visible
policy—truncate the picker, increase storage, or page/stream names—is a design
decision and can affect production RAM/flash.

**Suggested correction.** Define a maximum number of picker names that is
derived from the actual scratch-buffer capacity, and specify what the menu does
when it is reached. The smallest likely implementation is to stop accepting new
names once the slot array is full, but the architect must choose that behavior.

## Source claims checked and found clean

- **E3/E5 snapshots:** `wasForth` and `hadText` are both captured before
  `pemCloseAlphaInput()`. The Forth empty path is the only path that deletes the
  placeholder; a non-empty live line has already been rewritten into its step
  by the bottom of `pemAlpha()`. I found no reviewed path with non-empty
  `aimBuffer` that takes the Forth empty-delete branch.
- **E5 state derivation:** after a non-empty close, `pemCloseAlphaInput()` moves
  `currentStep` to the next step. `forthEntryStateAtInsertion()` walks from the
  owning program start and derives from the predecessor, so it examines the
  committed line rather than cached state.
- **E6 ALPHA gesture:** all direct `insertStepInProgram()` calls in the permitted
  R3 file set pass through `addStepInProgram()`. Its pre-move places
  `currentStep` at the insertion point before the `ITM_AIM` arm calls
  `forthEntryStateAtInsertion()`; the function therefore examines the original
  cursor step as predecessor. No direct bypass caller was found in the allowed
  files.
- **`scanLabelsAndPrograms()`:** both passes walk the same validated step prefix.
  The count pass checks a possible final malformed step before validating its
  successor, while the fill pass validates first, so malformed input can make
  the allocation one label too large, never too small. Program increments occur
  after the same successor check in both passes. I found no shape on which fill
  can exceed count.
- **Self-test export:** `FORTH_SELFTEST_EXPORT` occurs only around
  `_closeCatalog()` in `keyboard.c`; the production expansion is `static`, so
  shipped linkage and internal-call optimization are unchanged. The debug
  binary intentionally differs only in visibility. No other static-production
  keyboard helper is exported by this macro.

## Stale evidence citations are code-comment defects, not DESIGN defects

The reset comments in `manage.c` cite `manage.c:1441/1463` and
`manage.c:889-895`; the CAT→FORTH comment in `keyboard.c` cites `:1213-1216`.
Those ranges no longer contain the claimed code. The actual stable anchors are
the `func == ITM_AIM` / `func == ITM_FORTH` arms, the `ITM_BACKSPACE`
empty-buffer arm, and the PEM catalog arm's `runFunction(item)` /
`_closeCatalog()` pair. Qwen task R3-2 repairs the code comments with anchors;
no DESIGN.md edit is needed.
