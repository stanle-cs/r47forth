# Defects — Forth data-stack semantics vs R47

Owner ruling (2026-07-25): **anything that behaves differently from R47 is a
bug.** This file records two findings against that standard, plus one parked
design question. Both defects were found while building the second showcase
program (`test_savings_program`), not by review.

---

## D1 — A native item does not lift the stack after a Forth push

**Severity: bug. Silent wrong answers, no error code. FIXED 2026-07-25.**

### Symptom

```
1000 RCL 19        →  X = <R19>,  Y = 0
```

R47 keeps 1000 in Y. Forth destroys it. Verified on the simulator: after
`XEQ 'CLSTK' 1000 RCL 19` with R19=7, `X=7 (dtLongInteger)` and
`Y=0 (dtReal34)`. Forth-to-Forth pushes are unaffected — `1000 2 3` correctly
leaves `Z=1000` — so the divergence is specifically at the Forth→native
boundary.

### Cause

Upstream `liftStack()` (`src/c47/stack.c:20`) is conditional:

```c
void liftStack(void) {
  if(getSystemFlag(FLAG_ASLIFT)) {   /* lift: shift X..T up */ }
  else                              { freeRegisterData(REGISTER_X); }  /* overwrite X */
}
```

`forthPushReal34`/`forthPushInt32` (`packages/forth-core/forth_inner.c:45-68`)
force the flag on for their own lift and then **clear it**:

```c
setSystemFlag(FLAG_ASLIFT);
liftStack();
clearSystemFlag(FLAG_ASLIFT);      /* <-- leaves auto-lift DISABLED */
```

Every Forth push therefore leaves the machine in "lift disabled" state. The next
native item that calls `liftStack()` — `RCL`, and anything else that pushes a
result — takes the `else` branch and overwrites X instead of lifting above it.

The clear is unnecessary for Forth's own pushes, since each push sets the flag
itself on entry. Its only effect is to break interop with native items.

### Consequence

A working value cannot survive across an `RCL` in compiled Forth. This is not
theoretical: the first draft of the `SAVE` showcase program kept a running
balance on the stack across `RCL`, stored six zeros into R00..R05, and finished
with `lastErrorCode = 0`. Nothing reported a problem.

### Fix applied

The scrub was broader than the report: it cleared `FLAG_ASLIFT` after **every**
push *and* every primitive, at six sites across `forth_inner.c` and
`forth_compile.c`. Upstream does the opposite — `reallyRunFunction()`'s epilogue
*sets* the flag after any item carrying `SLS_ENABLED`, and every prim-equivalent
item (`fnAdd`, `fnDrop`, `fnSwapXY`, `fnMultiply`) carries it.

The rule is now uniform: every dispatch that leaves a value in X sets
`FLAG_ASLIFT`. Pushes leave it set, primitives set it, and the definition marks
(`GLOBAL`/`IMMEDIATE`) touch no stack so they leave it alone (`SLS_UNCHANGED`).

Pinned by `test_native_lift_after_forth`: `1000 RCL 19` leaves 1000 in Y, and
`5 3 + RCL 19` leaves 8 in Y — the second covering the case where the value in X
came from a primitive rather than a literal. DESIGN.md's ASLIFT section, which
had asserted the scrub "is correct and unchanged", is corrected.

Visible consequence: the `SAVE` showcase program now carries its running balance
on the stack across `RCL`, which was impossible before and is what the register
workaround in its first draft existed to avoid.

---

## D2 — Deep recursion silently returns wrong results

**Severity: bug by the same ruling, but see D3 — the depth itself is a design
consequence, the silence is the defect. FIXED 2026-07-25.**

### Symptom

```
5 FACT  →  120        6 FACT  →  720          (correct)
7 FACT  →  4320       8 FACT  →  25920        (5040 and 40320 expected)
9 FACT  →  155520     12 FACT →  33592320
```

with `: FACT DUP IF DUP 1 - RECURSE * ELSE DROP 1 THEN ;`. Each level past 6
multiplies by 6 again: the deepest stack entry is lost to stack-lift
displacement and the recursion keeps reusing whatever survived. `lastErrorCode`
stays 0 throughout.

Nesting makes it bite sooner. `6 2 NCR` returned 1 instead of 15, because
`FACT` ran with two values already underneath it on the stack.

`SUMDOWN` in the FDEMO showcase is the same shape, and is correct only because
it is driven with 5.

### Cause

The Forth data stack *is* the C47 RPN stack (see D3). It is 4 or 8 levels deep
per `FLAG_SSIZE8`; measured 8 in the self-test build. Pushing past the top frees
the topmost register, exactly as the calculator does natively. Recursion holds
one live value per level, so anything deeper than the stack silently corrupts.

### Fix applied

`forthDataDepth` in `forth_inner.c` counts values Forth has pushed and not yet
consumed since the current line began, driven by a new `stackEffect` column in
`forthPrims[]` (`DUP` +1, `OVER` +1, `DROP` -1, the four arithmetic words and
their glyph aliases -1, `SWAP` 0; the compile-time words are all 0). Growth past
`getStackTop() - REGISTER_X + 1` raises `ERROR_RAM_FULL`, alongside the existing
return-stack guard and runaway cap.

Two properties chosen so it can never fire on a correct program:

- **Only ever an underestimate.** A native item's stack effect is unknowable
  from the dispatcher, so running one *resyncs* the count to 0 instead of
  abandoning it. 0 is never above the true depth, so the guard can fire late but
  never falsely. The first design abandoned the count instead, which left the
  guard disabled for the rest of any line containing a native item — including
  the usual `XEQ 'CLSTK'` prefix, after which 0 is in fact exact.
- **Only while Forth is executing.** `forthPushInt32`/`forthPushReal34` are
  public helpers used to seed the RPN stack outside any Forth line. Counting
  those accumulated a stale depth that refused a later legitimate push; that
  regression was caught by `test_param_series_c_acceptance`, not by review.

Pinned by `test_data_stack_overflow_guard`: `6 FACT` is still exactly 720,
`7 FACT` raises `ERROR_RAM_FULL` instead of returning 4320, and a long-but-
shallow `1 2 + 3 + … 9 +` chain is untouched.

Not fixed, and out of scope by the same reasoning as before: a *user* keying
more values than the stack holds still loses the bottom one silently, because
that is what R47 does.

---

## D3 — RULED 2026-08-03: hybrid spill stack (design: DESIGN.md §11)

**Not a defect. Owner-approved stage; the decided design lives in
DESIGN.md §11 and the queue in QWEN_RUNBOOK §2. The analysis below is
the historical record that scoped it.**

The Forth data stack is not a separate structure — it is the calculator's RPN
stack. The primitives are one-line delegations:

```
pDup  → fnDupN(1)     pPlus  → fnAdd        pDrop → fnDrop
pSwap → fnSwapXY      pMinus → fnSubtract   pOver → ...
```

and pushes go through `liftStack()` into `REGISTER_X`. Depth is therefore
whatever C47 is configured for, 4 or 8, and cannot be raised from inside the
package.

That is a consequence of a deliberate choice, not an oversight. Because the data
stack is the RPN stack:

- `5 SUMDOWN` leaves its answer in X where the user can see it;
- C47 functions take their arguments from X/Y with no marshalling;
- Forth words are XEQ-able like any other function;
- there is no second numeric representation to keep in sync.

All of that follows from "R47 wins", and a private deep Forth stack would give
it up: values would live somewhere the user cannot see and native functions
cannot reach.

### The option worth keeping open

A hybrid. Keep X/Y/Z/T as the visible top of stack, and spill deeper entries
into a package-owned region, refilling on the way back. Depth becomes a package
concern while visibility and native interop are preserved.

Cost, which is why it is parked: every primitive currently operates on the RPN
stack directly, so each would need a push/pop wrapper that knows about the spill
boundary, and the FTOK_C47 dispatch would need to guarantee the native item sees
a correctly-populated X/Y at the moment it runs. That is a stage of its own, and
it should not start before D1 is fixed — D1 is in the same code path and would
otherwise be inherited by the new one.

Until then the practical guidance is that the working value may ride the stack
(D1 makes that safe again), while loop counters and indices are better kept in
registers — not because the stack cannot hold them, but because 8 levels shared
with the user's own stack is not much room, and D2 now stops rather than
corrupts when it runs out.

**Measured cost of the D1+D2 fixes:** `make dmcp5r47` flash 1094536 -> 1094824
(+288 B), self-test 173 checks green.
