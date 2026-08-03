# D3-2 — spill goes live in the dispatch bracket; 7 FACT = 5040 — Part A

Origin: DESIGN.md §11; queue QWEN_RUNBOOK §2b; D3-1 (`30d29d7e8`) landed
the region this packet wires. Authored per runbook §4a; mutations are
directed separately after this lands.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` clean.
2. `grep -c "forthDataDepthApply(forthPrims\[idx\].stackEffect)" packages/forth-core/forth_compile.c`
   → exactly 1 (the bracket).
3. `grep -n "ERROR_RAM_FULL" packages/forth-core/forth_inner.c | head -2`
   matches inside `forthDataDepthApply` (the branch this packet replaces).
4. `grep -c "forthSpillCatch\|forthSpillRefill\|forthSpillCount" packages/forth-core/forth_inner.c`
   → at least 6 (D3-1 landed).
5. `grep -c "static int test_data_stack_overflow_guard" packages/forth-core/test_dict_reloc.c`
   → at least 1 (the test this packet re-pins).
6. Stack-register FACTS (§4a-1, do not re-derive): register ids ascend
   from REGISTER_X (shallowest, stack top) to `getStackTop()` (deepest);
   `getStackTop()` is REGISTER_D under SSIZE8, REGISTER_T otherwise;
   upstream `liftStack()` destroys the DEEPEST register. Verify only:
   `grep -c "define getStackTop" src/c47/defines.h` → 1.

## Task — five items

**Item 1 — catch on overflow (forth_inner.c).** In
`forthDataDepthApply`, replace the capacity-error branch

```c
  if (net > 0 && forthDataDepth + net > forthStackCapacity()) {
    lastErrorCode = ERROR_RAM_FULL;
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  forthDataDepth += net;
```

with EXACTLY:

```c
  if (net > 0 && forthDataDepth + net > forthStackCapacity()) {
    /* D3-2 (DESIGN.md §11): the falling values are Forth-owned — catch
     * them into the spill, deepest first, BEFORE the primitive's lifts
     * destroy them. LIFO refill in forthSpillSettle() restores the
     * shallowest spilled value first. Depth saturates at capacity; the
     * spill count carries the excess. Only arena exhaustion errors. */
    int16_t overflow = (int16_t)(forthDataDepth + net - forthStackCapacity());
    int16_t k;
    for (k = 0; k < overflow; k++) {
      if (!forthSpillCatch((calcRegister_t)(getStackTop() - k))) {
        lastErrorCode = ERROR_RAM_FULL;
        displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        return false;
      }
    }
    forthDataDepth = forthStackCapacity();
  } else {
    forthDataDepth += net;
  }
```

**Item 2 — refill after the primitive (forth_inner.c).** Add, right after
`forthDataDepthApply`:

```c
/* D3-2: after a primitive consumed values, pull spilled values back into
 * the vacated deepest slots. Called from the dispatch bracket only. */
void forthSpillSettle(void)
{
  while (forthSpillCount() > 0
         && forthDataDepth < forthStackCapacity()) {
    if (!forthSpillRefill(getStackTop())) {
      break;   /* allocation failure: value stays spilled, count intact */
    }
    forthDataDepth++;
  }
}
```

Declare `forthSpillSettle` in `packages/forth-core/forth_dict.h` next to
the D3-1 declarations.

**Item 3 — the bracket (forth_compile.c).** Immediately after the
`forthPrims[idx].fn();` line and its `setSystemFlag(FLAG_ASLIFT);`
companion inside the dispatch (gate item 2 located it), add one line:

```c
          forthSpillSettle();   /* D3-2: refill vacated slots from the spill */
```

**Item 4 — interim boundary stop (forth_inner.c).** In
`forthDataDepthResync`, add at the top:

```c
  if (forthSpillCount() > 0) {
    /* D3-2 interim, refined by D3-3: an arbitrary native cannot run
     * while Forth values hide below the visible window. Loud stop. */
    lastErrorCode = ERROR_RAM_FULL;
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    forthSpillReset();
  }
```

**Item 5 — re-pin the guard tests (test_dict_reloc.c, main file).**

- `test_data_stack_overflow_guard`: its assertion that pushing past
  capacity raises `ERROR_RAM_FULL` is the RETIRED contract. Read the
  test; re-pin it to the new one: pushing capacity+3 values yields NO
  error, `forthSpillCount() == 3`, and popping everything back returns
  the values in correct stack order with the spill drained to 0. Rename
  its PASS text to `deep push spills, drains back in order`.
- Add `static int test_deep_recursion_spill(void)` right after it,
  called from the runner right after the overflow-guard call: drive the
  interpreter exactly the way neighbouring tests do (grep
  `x_set_string(": ` for the idiom and copy it) to define and run:
  `: FACT DUP 1 > IF DUP 1 - RECURSE * THEN ;` then `7 FACT` — assert X
  is 5040 (long integer compare via the idiom the D1/D2 pins use — grep
  `test_native_lift_after_forth` for the X-compare idiom and reuse it).
  Then `6 2 NCR`-equivalent if an NCR word exists in the tests — if not
  present as a fixture already, SKIP that half and note it in the PASS
  line: `7 FACT deep recursion = 5040` suffices.
- `test_savings_program` and every other existing test stay untouched
  and must stay green — a red there is an immediate STOP with a
  `[SOL DEBUGGER HANDOFF]` report.

## Gate

`./packages/forth-core/build-test.sh > /tmp/forth-d3-2-gate.log 2>&1; echo "gate exit: $?"`
(log per THIS packet). Success = exit 0 + both banners + the re-pinned
PASS line + the new deep-recursion PASS line + arena line at baseline
(`dict here=48 sizeBlocks=16 gdict here=16 sizeBlocks=16
freeRamDelta=128`). Print those lines and STOP. No commit, no mutations.
