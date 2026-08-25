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
  removed wholesale after a second display-pipeline defect class: with
  previews on, ulp.txt's fifth capture (a complex34) left state that turned
  programs.txt SPIRAL (which uses RND) wrong three files later — ROUND/RSD
  compute *through* the display pipeline (format to `displayValueX`,
  re-parse), so the pipeline is math, not just paint. Bisected to the exact
  preview call under gdb (file bisect → call-index bisect → per-type
  no-op). Ruling: capture never re-enters the display pipeline; the U2
  browser formats lazily at render time via TEMP_REGISTER_1 staging
  (registerBrowser precedent). This also dropped the TMP_STR_LENGTH scratch
  and the per-entry 28-byte preview field.
- **2026-08-24.** Catalog name "HIST" found taken by upstream item 1401
  (CAT_MENU); U2 browser item renamed U.HIST, rows 427-429 kept.
