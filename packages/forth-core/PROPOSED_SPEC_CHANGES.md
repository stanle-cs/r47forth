# Proposed spec changes (forth-core)

Non-authoritative. DESIGN.md is read-only for the agent; anything below is a
proposal for a human maintainer to fold into DESIGN.md (or upstream), not a
change already ratified there.

---

## Range-overlap double-free guard in `freeListFree`

**RATIFIED** — promoted to DESIGN.md §5.6 and §6 hook H10 (COMMIT 12).
Implementation lives in `packages/forth-core/core/freeList.c` (already
registered in `pkg_override_sources`). Tests in
`packages/forth-core/test_dict_reloc.c` (FIX-6 section).

**Pending:** upstream MR to c47 firmware repository.

---

## §3.3.6 (C-1) label-arm dispatch: replace the PGM_RUNNING wrap with direct fnExecute

**RATIFIED** — folded into DESIGN.md §3.3.6 (see the DESIGN-HISTORY 2026-07-13
"C-1 label-arm dispatch" entry); the body below is the historical record.
Found during P3/T3.5, 2026-07-13. DESIGN.md §3.3.6 originally prescribed
dispatching interpret-state C47-label calls via `reallyRunFunction(ITM_XEQ,
label)` inside the same PGM_RUNNING save/set/restore wrap as the FTOK_C47 arm.
That DECIDED text is defective for ITM_XEQ specifically — first exercised
end-to-end by test T3.5:

1. **The program never ran.** Under a forced PGM_RUNNING, `fnExecute` takes
   its nested branch: it allocates a 3-block subroutine level, `fnGoto`s, and
   defers stepping to an *enclosing* `runProgram` loop. From an interactive
   Forth line no such loop exists — the XEQ was a silent no-op that leaked
   the subroutine level (3 blocks per call).
2. **§9.3 bump site A was suppressed.** Forcing PGM_RUNNING made
   `fnExecute` skip the run-generation bump, so a program run started from a
   Forth line carried a stale dictionary generation.
3. The §2.2 livelock the wrap defends against lives in items.c's NORMAL-MODE
   dispatch (`refreshStatusBar()` → GTK pump), which calling `fnExecute`
   directly bypasses entirely.

**Implemented in the package** (forth_compile.c label arm): clear
`dynamicMenuItem = -1` (its >= 0 menu branch in `fnGoto` reinterprets the
label ID as a global step number), then call `fnExecute(label)` directly.
Interactively this is the keyboard-XEQ path (fnGoto + runProgram, bump site A
fires); from a program-context Forth step (programRunStop == PGM_RUNNING) the
nested branch is taken unchanged.

**Also documented:** outer-interpreter nesting deeper than 2 is unreachable
by construction — label XEQ from a program-context Forth step is
continuation-style (level push + fnGoto, stepping resumed by the enclosing
runProgram loop), so interpreter frames never stack past
line→program-step. FORTH_OUTER_NEST_MAX=2 is a safety backstop, pinned by a
hook-primed test (T3.6 phase A).
