# D3-3 — user-native boundary rule: named message + tests — Part A

Origin: DESIGN.md §11; D3-2 (`540977271`) landed an interim loud stop in
`forthDataDepthResync` (error + reset when the spill is non-empty). This
packet finishes the rule: the error explains itself, and the boundary is
pinned by tests from both sides. Authored per runbook §4a.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` clean.
2. `grep -n "if (forthSpillCount() > 0)" packages/forth-core/forth_inner.c`
   → exactly TWO matches (LeaveOuter's line-end contract and the resync
   interim stop; this packet edits ONLY the one inside
   `forthDataDepthResync` — read both neighborhoods to identify it).
3. `grep -c "EXTRA_INFO_ON_CALC_ERROR" packages/forth-core/forth_inner.c`
   → at least 1 (the moreInfoOnError pattern exists in this file; if 0,
   grep it in packages/forth-core/forth_compile.c and copy that idiom).
4. `grep -c "static int test_deep_recursion_spill" packages/forth-core/test_dict_reloc.c` → 1.

## Task — three items

**Item 1 — the named message.** Inside `forthDataDepthResync`'s interim
block, after the `displayCalcErrorMessage(ERROR_RAM_FULL, ...)` line, add
the project's extra-info idiom (copy the exact `#if (EXTRA_INFO_ON_CALC_ERROR == 1)`
wrapper shape from the nearest existing use):

```c
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "a native item cannot run while %u Forth value(s) are spilled below the visible stack", (unsigned)forthSpillCount());
      moreInfoOnError("In function forthDataDepthResync:", errorMessage, NULL, NULL);
    #endif
```

NOTE the sprintf must run BEFORE the `forthSpillReset()` call so the
count is still live — if the reset currently precedes the display lines,
reorder so the message forms first and the reset stays last.

**Item 2 — boundary test (blocked side).** Add
`static int test_spill_native_boundary(void)` in the main test file right
after `test_deep_recursion_spill` (declare + call it the same way):

- Build a fixture program with an R47 label (grep `tpLbl` usage in
  `test_deep_recursion_spill`'s neighborhood for the idiom).
- Drive one line that pushes capacity+2 literal values (spill becomes
  non-empty mid-line) and then, IN THE SAME LINE, invokes the R47 label
  by name (the XEQ-by-name form the deep-recursion test's line syntax
  supports — grep how existing tests call a label from Forth source; if
  no existing test does, use the `XEQ` word form documented in the same
  file's fixtures; if neither is findable by grep, STOP and report).
- Assert: `lastErrorCode == ERROR_RAM_FULL`, `forthSpillCount() == 0`
  (the stop resets), and a following ordinary line computes correctly
  (clean state after the error).

**Item 3 — boundary test (allowed side).** In the same function, second
subcase: one line that pushes capacity+2 values and drains them with
primitive arithmetic (`+`) back below capacity, THEN invokes the same
R47 label. Assert: NO error, and the label executed (observable via its
effect — give the label a body that stores a known constant, and assert
that register afterward; use the register-assert idiom from
`test_native_lift_after_forth`).

## Gate

`./packages/forth-core/build-test.sh > /tmp/forth-d3-3-gate.log 2>&1; echo "gate exit: $?"`
Success = exit 0 + both banners + the two new PASS lines + `7 FACT deep
recursion = 5040` still green + arena at baseline. Print those and STOP.
No commit, no mutations.
