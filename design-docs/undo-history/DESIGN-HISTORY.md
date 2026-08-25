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
