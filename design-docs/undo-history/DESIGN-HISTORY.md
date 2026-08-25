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
- **2026-08-24.** Catalog name "HIST" found taken by upstream item 1401
  (CAT_MENU); U2 browser item renamed U.HIST, rows 427-429 kept.
