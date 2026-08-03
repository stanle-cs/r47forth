# D3-5 — depth bracket moves into forthOuterRun (T6-found defect) — Part A

Origin: T6's upstream-runner cases exposed that `fnForthOuter` — the
REAL item entry — never brackets depth accounting: D2's guard and D3's
spill were dead on the keyboard path (the sim battery drives the
bracketed `forthOuterInterpret` wrapper instead; the paths diverge).
Fix: the bracket lives INSIDE `forthOuterRun`, nesting-aware, covering
every mode and every caller. Authored per runbook §4a.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`.
   The tree is dirty with T6 content + architect probes — expected.
2. `grep -c "forthOuterRun(" packages/forth-core/forth_compile.c` → at
   least 5 (the definition + 4 callers).
3. `grep -n "\[APPLY\]\|\[SPILL\]" packages/forth-core/forth_inner.c | head -3`
   → matches exist (the architect's probes; Step 3 removes them).
4. `grep -c "forthDataDepthEnterOuter();" packages/forth-core/forth_compile.c` → exactly 1
   (inside forthOuterInterpret; Step 1 relocates it).

## Task — four steps

**Step 1 — the choke point (forth_compile.c).** Rename the existing
`static void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode)`
definition to `static void forthOuterRunInner(...)` (definition line
only — do NOT touch its body or its callers), then add immediately
after its closing brace:

```c
/* D3-5: the depth/spill bracket lives at the single choke point so
 * EVERY outer execution accounts — fnForthOuter, CHECK, DEFS_ONLY,
 * SKIP_DEFS and the interpret wrapper alike. Nesting-aware: only the
 * outermost run brackets (a Forth line can XEQ into another line). */
static int16_t forthOuterRunNesting = 0;

static void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode)
{
  if (forthOuterRunNesting++ == 0) {
    forthDataDepthEnterOuter();
  }
  forthOuterRunInner(ctx, mode);
  if (--forthOuterRunNesting == 0) {
    forthDataDepthLeaveOuter();
  }
}
```

**Step 2 — deduplicate the wrapper.** In `forthOuterInterpret`, DELETE
the `forthDataDepthEnterOuter();` and `forthDataDepthLeaveOuter();`
lines (the choke point now brackets its forthOuterRun call).

**Step 3 — remove the architect probes.** In
`packages/forth-core/forth_inner.c`, delete the three probe blocks: the
`#if defined(TESTSUITE_BUILD)` printf blocks printing `[SPILL] catch`,
`[SPILL] refill`, and `[APPLY]` (each is a 3-line guarded block; remove
guard lines and printf together, nothing else).

**Step 4 — the C-battery pin for the REAL path.** In
`packages/forth-core/test_persist.part.h`, right after
`test_spill_native_boundary`'s closing brace, add (and declare + call it
after that test's call in the runner, in the main file):

```c
/* D3-5: pin the REAL item entry, not the test wrapper — fnForthOuter
 * must bracket depth/spill exactly like forthOuterInterpret. Found by
 * the T6 upstream-runner cases (spill dead on the keyboard path). */
static int test_fnforthouter_brackets(void)
{
  int fail = 0;
  x_set_string("1 2 3 4 5 6 7 8 9 10 11 + + + + + + + + + +");
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: fnForthOuter deep line errored (%d)\n", lastErrorCode);
    fail = 1;
  } else {
    longInteger_t v; int32_t got = -1;
    if (getRegisterDataType(REGISTER_X) == dtLongInteger) {
      convertLongIntegerRegisterToLongInteger(REGISTER_X, v);
      longIntegerToInt(v, got);
      longIntegerFree(v);
    }
    if (got != 66) {
      printf("    FAIL: fnForthOuter deep line X=%ld, expected 66\n", (long)got);
      fail = 1;
    }
  }
  if (forthSpillCount() != 0) {
    printf("    FAIL: spill not drained after fnForthOuter (%u)\n",
           (unsigned)forthSpillCount());
    fail = 1;
  }
  if (!fail) {
    printf("    PASS: fnForthOuter brackets depth — deep line = 66, spill drained\n");
  }
  return fail;
}
```

If `x_set_string`, `convertLongIntegerRegisterToLongInteger` or
`longIntegerToInt` do not exist under those names (grep each), copy the
X-compare idiom from `test_native_lift_after_forth` instead and STOP
only if no comparable idiom exists.

## Gate

`./packages/forth-core/build-test.sh > /tmp/forth-d3-5-gate.log 2>&1; echo "gate exit: $?"`
Success = exit 0 + both banners + the new PASS line + `7 FACT` and the
WP/boundary lines still green + the upstream suite step now reporting
**12078 TESTS PASSED** (12,076 prior + the two forth_interp spill cases
turning green). Print those and STOP. No commit, no mutations.
