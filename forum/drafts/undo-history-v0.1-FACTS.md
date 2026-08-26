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

- 4th screenshot (2026-08-25, his request): the SYSFL picker page holding
  UHIST with the flag set (filled radiobutton), undo-attach-4-sysfl-uhist
  .png. UHIST is index 100 of the GENERATED menu_SYSFL catalog (113
  entries, built from the CAT_SYFL item rows at build time, sort order
  checked by upstream's own suite) — the "set it with SF in SYSFL" claim
  is backed by the generator, not by a hand-added menu entry.
- Attachment indices in the final post (3=picker, 2=history view,
  0=restored, 1=gap) encode a phpBB upload order — YOUR upload order
  decides the real numbers; renumber at post time if it differs.
- The PR-offer line is factual: the package was designed as an upstream
  feature patch from the first ruling (UPSTREAMING.md in
  design-docs/undo-history/ carries the submission notes).

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
- Flash cost: +4264 bytes (1090512 -> 1094776, make dmcp5r47 CUSTOM_PKG=packages/undo-history, matched-pair re-measured 2026-08-26 after audit round 5; vanilla itself drifts a few bytes with the tip's version string, so only same-day pairs count). NOTE: 4,264 B = 4.2 KiB — the post's "grows by 3.9 KB" line needs its number changed to 4.2 KB before posting (it was 4.1 through round 4; round 5 added 24 B).
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

## Final post attachment plan (2026-08-25, three-slot forum limit)
- Slots: zip (referenced as "the attachment" in Install), picker shot
  [attachment=1], history view [attachment=2]. Gap + restored shots are
  DROPPED from the post (remain in forum/screenshots/ and in the
  battery pins). Upload order for phpBB newest-first indexing: view
  first, then picker, then zip (zip lands at 0, unreferenced inline).
- The package-manager thread URL in Install
  (viewtopic.php?f=46&t=4876) is Stan's own thread; external fetch
  returns 403 to non-browsers, so it is author-verified only.

## ENTER-in-browser fix (2026-08-25, your bug report)
- "why can't I press enter to select the undo level in browsing mode?"
  was a real defect: processKeyAction's ITM_ENTER browser ignore lump
  (upstream browsers all ignore ENTER) swallowed the key before
  dispatch; the fnKeyEnter case was dead code on the real path and the
  battery, calling historyBrowserEnter() directly, stayed green.
  Fixed by removing CM_HIST_BROWSER from that one lump; B9 now drives
  the real btnPressed/btnReleased chain for DOWN, ENTER, EXIT.
  The zip is rebuilt with the fix — re-attach it if already uploaded.
- REDO paths (both true in the post as written): the REDO item on the
  STK menu / catalog / assignable key, and now ENTER on a level above
  the * in the view.

## Live-state ENTER mints (now) (2026-08-25, your third find)
- ENTER-restore from the view with NO undo in progress now saves the
  departure state as the (now) anchor first (same rule as the first
  UNDO press): the jump is redoable and (now) appears. Before the fix
  the pre-restore state was silently lost. Pinned by B10. Zip rebuilt
  again — re-attach if uploaded.

## Override numbering + browser key blanket (2026-08-25, your 4th report)
- Override after an undo: the replacing level now takes the dead redo
  tail's number — numbering stays consecutive, no unmarked hole (was:
  rows like 05 03 02 01). B11 + re-pinned R4.
- Keys without a browser meaning (digits, shifted functions) are now
  ignored while the view is shown — they no longer execute against the
  machine underneath (upstream's own browser blanket; the probable
  path behind the firmware error you hit; B12 drives it through the
  real key chain).
- The (now)-on-ENTER fix was already in the newest zip when you
  tested; grab pkg_dist/undo-history.zip fresh — it now carries all
  of today's fixes.

## Phantom ENTER captures (2026-08-25, your 5th report — one cause, three symptoms)
- ENTER dispatched into the browser was captured by upstream's
  pre-dispatch undo-save (ENTER is US_ENABLED): a phantom ENTER level
  from an empty history, the fresh (now) anchor merged away on a
  choose, the trail truncated on a second choose. One exclusion in the
  save block fixes all three; B13/B14 pin it through the real keys.
  (now) now appears on every browser restore from live. Zip rebuilt.

## Audit round 1 (2026-08-25): three fixes after your "more bugs" call
- A failed restore can no longer destroy the level it refused (fit
  pre-check before the anchor mint commits anything).
- First UNDO across an oversized skip now lands on the last stored
  level with the ~ on the anchor (never forward; redo returns).
- f-UP no longer opens the view while a command prompt (TAM) is
  pending. Zip rebuilt again — attach the latest.

## Audit rounds 2-5 (2026-08-26): the engine's corner semantics, settled
- Rounds 2-4 fixed how the ~ (skipped-capture gap) composes with
  merges, restores and the undo walk: the gap mark points the right
  way after a skip, equal states on either side of a gap stay separate
  levels, choosing a level then failing no longer confuses the cursor,
  and no stale ~ appears on captures after you navigate away.
- Round 5 (failure side): if memory is FULL and choosing a level
  fails mid-restore, the single-level undo buffer could be left half
  overwritten — pressing UNDO after that installed a state that never
  existed. Now a failed restore either leaves everything untouched or
  retires the single-level buffer (UNDO then walks the history ring
  instead); a retry after freeing memory works. Two new tests drive
  the calculator to genuine RAM-full to pin this.
- Round 6 came back functionally clean (one comment reworded, no code
  change). Zip rebuilt: pkg_dist/undo-history.zip is now 52432 B —
  attach the LATEST zip; every earlier one lacks these fixes.
