# D3-2A — amendment: forthPrimInvoke wrapper + line-end contract

Origin: the first D3-2 gate exposed two ARCHITECT design gaps (recorded
in DESIGN.md §11, amended 2026-08-03): primitives are invoked from FOUR
sites, not one, so refills never ran on the inner path (`7 FACT` came
back 4320 — the D2-era silent wrong answer); and a line ending with a
non-empty spill silently discarded values at the LeaveOuter reset. Your
D3-2 Items 1-4 are correct and stay. This packet finishes the wiring.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`.
   The tree is DIRTY with D3-2's work — that is expected; do NOT reset.
2. `grep -c "forthPrims\[.*\]\.fn()" packages/forth-core/forth_inner.c packages/forth-core/forth_compile.c | head -2`
   → the two files report 2 and 2 (four sites total).
3. `grep -c "void forthSpillSettle" packages/forth-core/forth_inner.c` → 1.
4. `grep -n "fnDrop(NOPARAM);" packages/forth-core/forth_inner.c | head -1`
   matches (the 0BR consume).

## Task — four items

**Item 1 — the wrapper (forth_inner.c).** Immediately after
`forthSpillSettle`'s definition, add EXACTLY:

```c
/* D3-2A (DESIGN.md §11): the ONLY way to invoke a primitive. Applies the
 * declared stack effect (catching overflow into the spill), runs it,
 * restores ASLIFT convention, then refills vacated slots. Returns false
 * when the depth/spill accounting refused the invocation; callers keep
 * their own lastErrorCode handling after fn(). */
bool_t forthPrimInvoke(uint16_t idx)
{
  if (!forthDataDepthApply(forthPrims[idx].stackEffect)) {
    return false;
  }
  forthPrims[idx].fn();
  setSystemFlag(FLAG_ASLIFT);
  forthSpillSettle();
  return true;
}
```

Declare it in `packages/forth-core/forth_dict.h` next to
`forthSpillSettle`. If `forthPrims` is not visible in forth_inner.c at
that point (grep `forthPrims` in the file first), STOP and report.

**Item 2 — convert all four invocation sites.** At each of the four
`forthPrims[...].fn()` sites (two in forth_inner.c, two in
forth_compile.c): the existing shape is an `if (!forthDataDepthApply(
forthPrims[X].stackEffect)) { <error handling> }` followed (in an else or
below) by `forthPrims[X].fn(); setSystemFlag(FLAG_ASLIFT);` and in one
case an existing `forthSpillSettle();` line (your D3-2 Item 3 — remove
that line as part of this conversion). Replace each pattern with
`if (!forthPrimInvoke(X)) { <the SAME error handling> }` and keep every
line of surrounding control flow (the lastErrorCode checks after)
untouched. One site at a time; re-grep after each to confirm zero
remaining `forthPrims[...].fn()` calls when done:
`grep -rc "forthPrims\[.*\]\.fn()" packages/forth-core/forth_inner.c packages/forth-core/forth_compile.c` → 0 and 0.

**Item 3 — settle the two direct consumes.**
- forth_inner.c 0BR: after its `fnDrop(NOPARAM); (void)forthDataDepthApply(-1);`
  pair, add `forthSpillSettle();`.
- forth_compile.c string consume (`fnDrop(NOPARAM);   /* copy MUST precede
  drop...`): read 5 lines around it; if an `forthDataDepthApply(-1)` is
  beside it, add `forthSpillSettle();` after the pair; if there is NO
  Apply(-1) there, STOP and report what you found instead.

**Item 4 — line-end contract (forth_inner.c).** In
`forthDataDepthLeaveOuter`, BEFORE the existing `forthSpillReset()` call,
add:

```c
  if (forthSpillCount() > 0) {
    /* D3-2A (§11): a completed line may not leave values beyond the
     * visible stack — loud stop, then the reset below discards. */
    lastErrorCode = ERROR_RAM_FULL;
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
  }
```

**Item 5 — fix the re-pinned overflow test.** Your re-pin asserted
`forthSpillCount()` AFTER the line completed — the LeaveOuter reset
zeroes it, which is why it read 0. Restructure to the amended contract:

- Case (a): one line that pushes capacity+3 values AND consumes them back
  down IN THE SAME LINE (e.g. eleven literals then ten `+` — total drains
  to one value); assert no error and X equals the correct sum (the
  literal values are yours to choose; state the expected sum in the
  assert).
- Case (b): one line that pushes capacity+3 and ENDS; assert
  `lastErrorCode == ERROR_RAM_FULL` after the outer returns (the line-end
  contract) and that a following, ordinary line still works (error path
  left clean state).
- Keep `test_deep_recursion_spill` exactly as it is — with the wrapper it
  must now report 5040.

## Gate

`./packages/forth-core/build-test.sh > /tmp/forth-d3-2-gate.log 2>&1; echo "gate exit: $?"`
Success = exit 0 + both banners + PASS lines for the restructured
overflow test (both cases), `7 FACT deep recursion = 5040`, and the arena
line at baseline. Print those and STOP. No commit, no mutations.
