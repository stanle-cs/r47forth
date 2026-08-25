# undo-history v0.1 — verified fact sheet (for Stan to write from)

Every figure re-checked against source this session. No prose here on
purpose, write it your way. Bracketed notes are context, not suggested
wording.

## What it is
- Package name: undo-history. First release.
- Multi-level undo + a full-screen history browser for R47/C47.
- Stock firmware undo = 1 level. Package: repeated UNDO presses keep
  stepping back.

## The user-facing pieces
- UNDO: press 1 = stock undo; presses 2..n = deeper levels (package).
- REDO (item 428): steps forward again. A new operation after undoing
  drops the redo trail.
- U.HIST (item 427): opens the browser. UP/DOWN move selection, ENTER
  restores the selected level and leaves, EXIT/BACKSPACE leave.
- HCLR (item 429): empties the history. The stock 1-level undo is NOT
  touched by it.
- All three items: STK menu (3 free slots filled), FCNS catalog,
  assignable to keys.
- UHIST system flag (SYSFL, settable with SF/CF, saved with the other
  flags, visible in flag browser): when set, f-shifted UP in normal mode
  opens the browser instead of BST. BST unchanged everywhere else (PEM
  included). Default off.

## What a level holds
- Whole stack (X..T, or X..D with SSIZE8) + LASTx + system flags +
  lrSelection/lrChosen + statistical sums when present.
- Restore = complete state comes back (verified: matrix in T + 3
  coefficients restored together, screenshot 3).
- Labels: operations dispatched normally show their catalog name (+, SLVQ,
  [M]^T). Plain value entries show "(val)". "(now)" = the pre-undo anchor
  level. "*" = where undo currently stands. Inverted row = selection.
  (Was "-" until 2026-08-25: your read of the draft caught that a plain
  dash is ambiguous against subtraction's catalog name "-" — a negative
  value entry rendered "- -3.". Fixed red-first with a namespace class
  test, B8; both synthetic labels are parenthesized now.)

## Numbers (all measured)
- History buffer: 4 KB, one block from the C47 memory pool, allocated at
  RESET. Visible in FLGS free-RAM line.
- Why 4 KB: measured pool headroom is ~5.6 KB before upstream's own
  matrix.txt 14x14 eigenvalue test (RCL58) fails; its QR step asks for one
  contiguous 116.5 KB chunk with ~119 KB free.
- Per-level cap: 1 KB. Bigger states (large matrix, long string) are
  skipped; next stored level shows "~". Numbering stays consecutive.
  Undo works across the gap; the skipped state cannot be returned to.
- Depth: ~25 levels of plain reals at stack size 4; ~16 at SSIZE8; hard
  cap 48.
- Flash cost: +3904 bytes (1090504 -> 1094408, make dmcp5r47, re-measured after the (val) label fix).
- SRAM4 statics: +124 bytes.
- Zip: 44 KB.

## Behavior rules worth stating
- Session-only. RESET and restoring a saved state clear the history.
- Solver/integrator internals never enter the history (they call the undo
  machinery internally; gated out). One SLV/SLVQ call = one level.
- Running programs do not capture (same rule as stock undo).
- Undo restores system flags with the stack, exactly like stock undo
  always did (includes the UHIST flag itself).

## Install / build (verified commands)
- Unzip into packages/undo-history.
- make sim CUSTOM_PKG=packages/undo-history
- With forth: make sim CUSTOM_PKG=packages/forth-core,packages/undo-history
- Device: make dmcp5r47 CUSTOM_PKG=... (same variable).

## Testing statement (true as of this session)
- Upstream full suite green with package active: solo AND combined with
  forth-core (13024+ cases).
- ~30 package test cases (undo semantics, browser, gap, gates).
- valgrind memcheck over the battery: 0 errors.
- Simulator only. NOT yet run on a real DM42n.

## Boilerplate the post must carry (forum/DESIGN.md tiers)
- Target: R47/C47 on DM42n (DMCP5).
- Base: r47forth commit faf9d698c  [NOTE: on an unpushed branch — push or
  restate before posting].
- Dependency: none. Composes with forth-core.
- Zip attached; COPYING inside (pkg_build adds it). GPL-3.0-only,
  inherited from c43.
- Backup + flash-at-own-risk line.

## Attachments
- pkg_dist/undo-history.zip
- forum/screenshots/undo-attach-1-history-view.png  [13-level view:
  (now) anchor w/ complex preview, * cursor, ~ gap, SLVQ label, [3x2]]
- forum/screenshots/undo-attach-2-gap.png  [5 levels, ~ on newest,
  numbering consecutive]
- forum/screenshots/undo-attach-3-restored.png  [stack after ENTER on the
  SLVQ level: 3x2 matrix, 1., -3., 2]
