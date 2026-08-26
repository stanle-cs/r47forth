# undo-history — DESIGN-HISTORY.md (non-normative amendment trail)

- **2026-08-24 (U1, pre-commit).** Three shapes were tried and settled
  during bring-up, all now normative in DESIGN.md:
  1. **Preview buffer**: `char buf[200]` on the stack was smashed by
     `shortIntegerToDisplayString` (locals of the serializer frame
     overwritten with the ShoI display string; caught as a SIGSEGV with
     ASCII soup in saved registers). The `*ToDisplayString` family assumes
     TMP_STR_LENGTH headroom → static TMP_STR_LENGTH scratch.
  2. **Ring storage**: pool-resident ring (allocC47Blocks, the
     savedStatisticalSumsPointer pattern) broke matrix.txt RCL58 (14×14
     eigenvalues, "OUT OF MEMORY … Fragmented free memory") with 224 KB
     free at op start — fragmentation, not exhaustion. A memory.c
     yield-and-retry hook was implemented and measured insufficient (freed
     hole does not coalesce). Static arena adopted; memory.c patch dropped.
     Diagnosis chain preserved in the U1 stage notes: capture no-opped under
     gdb → 182/182, ring armed → 181/182. The static arena then failed the
     DMCP5 link (".bss will not fit in region SRAM4", overflow 2048 B —
     device globals get 16 KiB), settling storage at its final shape: one
     malloc at first capture, never freed, from the same heap the pool
     lives in (SRAM3).
  3. **saveRestoreBackup.c patch dropped**: restoreCalc calls doFnReset
     (saveRestoreBackup.c:831/948), so the config.c doFnReset hook already
     covers state restore.
- **2026-08-24 (U1, capture purity).** Capture-time preview formatting
  removed wholesale after a wrong SPIRAL program result three test files
  downstream of ulp.txt captures. Ruling: capture never re-enters the
  display pipeline; the U2 browser formats lazily at render time via
  TEMP_REGISTER_1 staging (registerBrowser precedent). Also dropped the
  TMP_STR_LENGTH scratch and the per-entry 28-byte preview field.
- **2026-08-25 (root cause corrected).** The first causal story for the
  SPIRAL failure (ROUND/RSD computing through `displayValueX`) was wrong —
  plausible, consistent with the bisects, and unproven. A config-matrix
  rerun (formatter × buffer location × malloc size × tail writes) plus a
  `displayBugScreen` breakpoint found the true chain: the tail-buffer
  preview passed `sizeof(buf)` where `buf` was a **pointer** → strLg = 8 →
  a 48-digit long-integer preview tripped `longIntegerToAllocatedString`'s
  validation → `displayBugScreen` → `calcMode = CM_BUG_ON_SCREEN`,
  **silently** (LCD-only, headless-invisible) → SPIRAL later computed under
  bug-screen mode. Two upstream findings extracted (silent headless bug
  screens; uneven formatter buffer-contract enforcement — the ShoI member
  smashes a 200-byte stack buffer with no check at all, which was the
  earlier SIGSEGV). The capture-purity ruling stands, now for proven
  reasons; R9 asserts calcMode — the global the first diagnosis never
  diffed — and pin P1 reproduces the exact historical slip against it.
  The ROUND/RSD `displayValueX` architecture is real and stays listed as a
  reason display code is never side-effect-free, but it was NOT the SPIRAL
  mechanism.
- **2026-08-25 (storage, final).** The malloc arena was rejected by ruling
  ("straying from upstream convention, like not using allocC47Blocks, makes
  the design not good"). The pool-native ladder, each rung built and
  measured against matrix.txt RCL58:
  1. 13 register banks in the 137..255 enum gap (copySource-native, the
     serializer deleted) — red: banks pin ~117 register-class blocks, and
     even their cleared stubs (4 blocks each) stay resident.
  2. Zero-floor variant, one pool block per level (the
     savedStatisticalSumsPointer shape) + a memory.c yield-and-retry seam
     that frees every package block on allocation failure — still red.
  3. The free-list dump at the OOM gave the real law: RCL58's QR
     decomposition requests 29,820 contiguous blocks in ONE allocation
     (matrix.c:5413); the pool's top run was 29,188 with a 2,048-block
     arena resident; vanilla has ~31,236 — about 1,400 blocks (5.6 KiB) of
     TOTAL slack for the entire firmware. Every resident block anywhere
     below the top run shrinks it one-for-one; placement only shapes the
     scraps. (The U1 "freed hole does not coalesce" reading of the
     yield-hook failure was incomplete — hole location never mattered, the
     arithmetic did.)
  4. Final shape: the U1 ring engine on one reset-armed allocC47Blocks
     block sized INSIDE the slack (1,024 blocks new HW, 256 old), the
     forth-core gdict pattern — RCL58 green, suite green. Reset-time
     arming is load-bearing (pin S1 pins it); the saveRestoreBackup
     restore-tail hook re-arms after a state load, and the suite proves
     that path itself: undo_history.txt runs after serialize_cov's
     backup-restore cycle and R8 asserts the ring is armed.
- **2026-08-25 (U2).** The browser landed per the original browser-mode
  ruling (CM_HIST_BROWSER, the CM_ASN_BROWSER precedent). One measured
  composition lesson: the first combined gate failed because our
  CM_HIST_BROWSER addition edited determineItem's combined key-resolution
  list — the exact line forth-core's patch rewrites; an insertion adjacent
  to it conflicted too (three-way merges treat insertion-next-to-
  modification as overlap). Resolved by giving the mode its own branch at
  the head of that chain, in the clear gap between forth-core's hunks —
  now a binding composition claim in DESIGN.md §6. Render verified on real
  pixels via the run-sim dump path (marker-removed temporary capture in
  the battery driver; status bar, newest-first rows, inverted selection,
  cursor mark all present).
- **2026-08-25 (FLAG_UHIST).** The shifted-UP shortcut landed as a real
  system flag (bit 112, the first free one) with its SYSFL item in row
  2299 — which the SFL_MONIT offset arithmetic dictates, not a choice; the
  pre-named `SFL_2299` spare confirmed upstream planned the slot. The
  determineItem hook reuses the package's existing chain-head insertion
  (no new keyboard.c conflict surface vs forth-core), with the predicate
  package-side for testability. The key reroute itself cannot be driven
  headless (determineItem is static, the harness has no key injection) —
  the predicate is fully unit-tested/pinned and the resolution line is
  one-line-by-construction; sim keypress verification is manual.
- **2026-08-24.** Catalog name "HIST" found taken by upstream item 1401
  (CAT_MENU); U2 browser item renamed U.HIST, rows 427-429 kept.
- **2026-08-25.** Label collision found by Stan reading the announcement
  draft: unlabeled captures rendered `-`, byte-identical to ITM_SUB's
  catalog name (items.c row 96), so a value-entry row and a subtraction
  row were indistinguishable — a value entry of a negative number read
  `- -3.`. Fixed red-first: B8 scans the whole item namespace against
  both synthetic labels (fails on the shipped `-`), the placeholder is
  now `(val)` — parenthesized like `(now)` so the meta-label namespace
  is disjoint from item names by construction, and 5 glyphs so the
  label column keeps its gap before the preview at HB_PREVIEW_X
  (`(entry)` at 7 glyphs touched it). All three post screenshots
  re-captured with the recovered marker-block drivers, removed again
  after capture, full gate green.
- **2026-08-25 (later).** Stan, using the build: "why can't I press
  enter to select the undo level in browsing mode?" Real: the
  CM_ASN_BROWSER precedent sweep had added CM_HIST_BROWSER to
  processKeyAction's ITM_ENTER browser ignore lump — correct for
  upstream browsers, which all ignore ENTER, fatal for the one browser
  that acts on it. The fnKeyEnter case was unreachable dead code on the
  real path; the battery stayed green because it called
  historyBrowserEnter() directly ("silence is not-run" class). Fixed
  red-first: B9 drives the real btnPressed/btnReleased chain (key
  numbers resolved from kbd_std at runtime, GdkEvent per the
  btnClickedP idiom) for DOWN, ENTER, EXIT — red on the swallow, green
  after removing CM_HIST_BROWSER from that one lump. Two detours worth
  their lines. (1) The first fix attempt tested red WRONGLY because
  pkg_patch_refresh + bare ninja does not re-materialize PATCHED
  upstream files into the build shadow (only +files ride ninja); meson
  --reconfigure is required — the stale-shadow trap in its patched-file
  form. (2) B9 passed isolated and failed under the full suite: a
  latched SHOW state from an earlier test (btnPressed's own first act
  is `showScreenDismissed = (SHOWMODE || currentMenu() == -MNU_SHOW)`
  followed by closeShowMenu(), which resets calcMode) ate the first
  press — found with a gdb hardware watchpoint on calcMode after two
  wrong hypotheses (ambient kbd_usr assignments, a stale TO_FN_EXEC
  queue; both normalizations kept as the pin's defined-context
  preamble). On device the browser cannot coexist with SHOW — the
  entry keypress dismisses it before the browser opens — so the
  dismissal is test-context pinning, not a firmware change.
- **2026-08-25 (third use-report).** Stan: "(now) did not trigger if we
  use the ENTER key." Real gap, sharper than the first read of it: the
  anchor was minted only in fnUndo's first-undo branch, so a browser
  ENTER from the LIVE state (no undo pressed) jumped away without
  saving the departure state — unredoable, and no (now) row. Fixed in
  undoHistoryRestoreLevel: a live-cursor restore runs
  undoHistoryNoteFirstUndo() first (same gates), re-finding the target
  by seq because the anchor push can evict or dedupe-merge; a push that
  evicts the very target makes the restore refuse rather than land
  wrong. B10 pins both halves red-first (anchor present + redo reaches
  the pre-restore state). The (now) lifecycle explanation given for the
  second report stands; this closes the one entry path that skipped it.
- **2026-08-25 (fourth use-report, three items).** Stan: level count
  wrong after an override-after-undo; a firmware error on exit with the
  browser shown; (now) still missing on ENTER-choose. Probed as one
  sequence with full state dumps. (a) REAL: the override dropped the
  forward levels but historySeq kept counting — rows read 05 03 02 01,
  an unmarked hole. Ruled: seq is display numbering, unique within the
  ring at any instant, NOT a forever-identity; truncate rewinds the
  counter to the surviving top, the replacing capture takes the dead
  tail's number. B11 red-first; R4's old assert pinned the overturned
  ruling (never-reuse) and was re-pinned to the new one. (b) REAL
  CLASS, exact trigger unconfirmed: unhandled items (digits, shifted
  functions) fell through processKeyAction's mode switch — no
  CM_HIST_BROWSER case in the browser blanket — and EXECUTED against
  the machine under the browser; B12's red run showed a digit press
  flipping calcMode out of the browser. Joined the FLAG/FONT blanket.
  The direct EXIT and app-exit/restore paths probed clean
  (previousCalcMode rides the backup; the restore fixup group already
  carries CM_HIST_BROWSER). (c) Stale build: the live-restore anchor
  fix (B10) shipped in the zip minutes before the report; reproduces
  on the prior build only.
- **2026-08-25 (fifth use-report, three items, ONE cause).** Stan:
  ENTER on an empty history inserted an ENTER level; choosing a second
  level rewrote the list; (now) never appeared from the browser. The
  real-key probe dumped all three: 01[ENTER] from empty,
  05[ENTER] with no N after choose A, a 3-row list after choose B.
  Root cause upstream of every earlier fix: ENTER is US_ENABLED, so
  reallyRunFunction ran its note+saveForUndo BEFORE dispatching into
  the browser — the capture (labeled ENTER) dedupe-merged the
  just-minted anchor away on a choose (a merged anchor is a plain
  state again), truncated the trail on a mid-trail choose, and pushed
  a phantom level from empty. The B10 mint was correct and invisible:
  its push merged into the phantom. Fix: CM_HIST_BROWSER joins the
  save block's exclusions (the CM_GRAPH/CM_NO_UNDO shape). B13/B14
  red-first through the real key chain. Lesson logged: the ENTER
  dispatch fix (B9) opened a path upstream browsers never exercise,
  and its side effects landed three reports later — when a fix opens
  a new dispatch path, audit what upstream runs on EITHER side of the
  handler, not just the handler.
- **2026-08-25 audit round 1** (cross-model, out-of-family only —
  Claude-authored package, family exclusion): three CONFIRMED (A1
  failed-restore corrupts cursor and destroys the selected level; A2
  anchor mint bypasses gap bookkeeping, second UNDO walks forward; A3
  browser opens mid-TAM and TAM eats its keys), three refuted (S3
  correct-by-construction, G1 empirically dead, G3 wrong item
  identity). Report: AUDIT_round1_2026-08-25.md. Findings, not fixes:
  the tree ends the round as it began; fixes are the next work, each
  red-first with its class pin.
- **2026-08-25 audit round 1 fixes.** A1/A2/A3 landed red-first (B15,
  R12, B7-TAM: seven asserts red on the shipped code, green after).
  A2's shape: the mint now owns gap+cursor bookkeeping and fnUndo takes
  a package step for the gap case instead of upstream undo(), consuming
  thereIsSomethingToUndo so the press cannot re-mint. Process note paid
  for twice today: a python edit heredoc that asserts inside a
  BACKGROUND task dies silently and the gate then runs the unfixed
  tree — edits land foreground with a landed-sentinel, gates run
  separately. Round 2 on these fix commits is owed before the audit can
  close.
- **2026-08-26 audit round 2** (on round 1's fixes; the fix trap held —
  4 of 4 confirmed findings were in the fixes). Convergent R2-1: the
  dedupe-merge path ignored a pending gap and the fallback landed on
  the skipped state; fixed in push (merge keeps GAPBEFORE) + the mint's
  result-2 branch (step-over). R2-2 ruled: a restore-from-live abandons
  the live branch's pending gap. R2-3: restore failure now repairs the
  cursor to the pushed anchor. R2-4: B15 made self-pinning and
  mutation-proven. Pins R13/R14; gate green; round 3 owed.
- **2026-08-26 audit round 3** (on round 2's fixes; trap 3-for-3).
  Convergent ruling: equal states separated by a gap are distinct
  occurrences — dedupe suppressed when GAPBEFORE is incoming; the
  anchor pushes with the ~ pointing the right way and undo lands on
  the last recorded state. R13 v1 had pinned the buggy merge (lesson:
  pins encode semantics, not fix echoes). Restore-failure push
  detection moved from entry-count to top-seq; gap-abandon extended to
  every successful restore. Round 4 owed.
- **2026-08-26 audit round 4** (on round 3's deltas; 4-for-4). One
  convergent finding: the gap-abandon rule was per-site — stepBack and
  fnRedo restores leaked the pending gap. Moved into the
  historyRestoreToIndex funnel (bug class: structural rule spelled
  per-site); caller clears removed; R15 red-first. Rounds 5+6 owed.
- **2026-08-26 audit round 5** (on round 4's delta; both readers
  confirmed the delta itself CLEAN — the funnel clear is right at all
  four call sites, and gap-pending with a non-live cursor is
  structurally unreachable). The round rotated its question to the
  FAILURE side and the fix-trap streak ended at four: R5-2 is
  U1-original. R5-1 (coverage): the failure returns of
  historyRestoreToIndex were unreachable by any test — pool-hoard
  helpers added, R16 pins the slot-0 failure contract (gap, cursor,
  live state, armed buffer all survive; the freed-memory retry
  succeeds and abandons the gap), which also gives R2-3's trace-only
  cursor repair executable coverage and retires TESTING.md's
  "RAM_FULL mid-restore is not batteries-enumerable" ruling. R5-2
  (functional, cross-refuted by Sol after Gemini raised it): a
  mid-staging failure tears SAVED_* while the browser leaves
  thereIsSomethingToUndo armed — the next plain undo() installs a
  state that never existed. Fixed in the funnel: any failure after
  the first mutated slot (or at the sums step) retires the buffer;
  slot-0 failures keep it. Transactional staging rejected (no
  upstream convention — upstream undo() tears the live state the
  same way; and rebuild-the-bank is the relocating-state fix shape).
  R17 red-first. Rounds 6+7 owed.
- **2026-08-26 r5 addendum (pre-round-6): the sums arithmetic.** Writing
  the sums-site retirement pin exposed that the scenario is
  unreachable by construction: HISTORY_SUMS_BYTES (28 sums x
  REAL_SIZE_IN_BYTES(75) = 28 x 60 = 1,680 B) exceeds the per-entry cap
  (1,024 B)
  on both ring sizes, so HAS_SUMS entries cannot exist — stats
  sessions always skip through the gap machinery (coherent: the
  single-level undo still operates, restoring a pre-stats level
  correctly drops live sums via undo()'s own branch). Ruled a
  by-construction skip, not a defect: documented in DESIGN.md, pinned
  by R18 (real sums block through saveForUndo -> skip + gap + ~ on
  the next sums-free capture) with a tripwire that reddens if the
  constants ever let sums fit — the day the dormant sums restore and
  retirement paths need real pins. Round-5 packet lesson encoded in
  the template: constants that gate reachability (sizing arithmetic)
  belong in the packet — neither reader could have caught this
  without HISTORY_SUMS_BYTES vs the cap in front of them.
- **2026-08-26 audit round 6** (on the r5 deltas + sums addendum):
  FUNCTIONALLY CLEAN — the first round with zero functional findings
  on its subject. One documentation finding (Sol): the retirement
  comment's absolute claim dies to a coherent-by-luck construction (a
  staged slot writing bytes equal to the pre-op bank; the retired
  buffer was the only path back to an oversized never-ringed state).
  Ruled conservative-by-design instead of reworked: exact-change
  tracking is failure-path code whose own defect silently KEEPS a
  torn bank — over-retire loses one undo level in a RAM-full corner,
  under-retire re-opens R5-2; the asymmetry is the ruling (comment +
  DESIGN.md + TESTING.md future-pin demand). Gemini's forward-landing
  claim refuted on version attribution: every constructible variant
  has the buffer already spent, making the retirement a no-op and the
  walk identical pre-r5; the armed+NONE+old-live conjunction is
  unconstructible. Both readers independently re-confirmed the
  ordinary-use invariance, retry idempotence, pin-per-site coverage,
  helper soundness, and the sums-session walk. Rounds 7 (prose-only
  delta) and 8 close the audit if clean.
- **2026-08-26 engine comment trim (Stan's zip-size question):** eleven
  audit-provenance tags ("audit r3/r4/r5/r6", "audit A1/A2", pin
  cross-refs) removed from engine comments per the standing 2026-08-09
  comment ruling and this package's submission-ready rule — an
  upstream reviewer cannot resolve them. Invariant sentences kept
  verbatim; every ruling stays traceable here and in the AUDIT_round*
  reports. The A1 comment's old-code narration ("the old refuse path
  minted, evicted the target") rewritten as the invariant it taught.
  Engine comment density measured 20% before the trim — the tag CLASS
  was the violation, not the volume.
