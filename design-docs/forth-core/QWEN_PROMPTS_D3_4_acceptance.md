# D3-4 — stage acceptance: visible-window parity pin — Part A

Origin: DESIGN.md §11; D3-1..D3-3 landed. This packet adds the stage's
acceptance pin: spill activity must be INVISIBLE to the native window.
The docs fold is architect work and not part of this packet. Authored
per runbook §4a.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` clean.
2. `grep -c "static int test_spill_native_boundary" packages/forth-core/test_dict_reloc.c` → 1.
3. `grep -c "PASS: 7 FACT deep recursion" packages/forth-core/test_dict_reloc.c` → at least 1.

## Task — one test function, two subcases

Add `static int test_spill_window_parity(void)` right after
`test_spill_native_boundary` (declare + call it the same way). Each
subcase prints one PASS/FAIL line; end with `cleanupTestProgram();` and
restore every global you touch (copy the neighbouring prologue/epilogue).

- **WP-1 same computation, spilled vs unspilled.** Compute the same
  arithmetic twice in separate lines: once staying under capacity
  (e.g. `1 2 3 4 + + +` → 10) and once padded past capacity so the spill
  engages and drains within the line (e.g. eight extra literals below,
  consumed by extra `+`s, arranged so the SAME final value results —
  state your arithmetic in a comment with the expected value). Assert
  both lines leave identical X (type and value) and no error. This pins:
  spilling changes nothing observable about the result.
- **WP-2 visible window depth.** After a line that pushes capacity+2 and
  drains back to exactly 3 values, assert X, Y, Z hold those 3 values in
  correct order (compare via the register idiom used by
  `test_native_lift_after_forth`) and `forthSpillCount() == 0`. This
  pins: the window's contents and order match what an 8-deep-only R47
  would show for the same net program.

## Gate

`./packages/forth-core/build-test.sh > /tmp/forth-d3-4-gate.log 2>&1; echo "gate exit: $?"`
Success = exit 0 + both banners + the two WP PASS lines + the 5040 and
boundary lines still green + arena at baseline. Print those and STOP.
No commit, no mutations.
