# Stage F2-5 — close the F2 acceptance review findings (bounded implementation prompt)

Origin: post-F2 review of the landed F2-2/F2-4 output.  The final F2-4 gate
was green, but required mutation 1 (`<=` to `<` in
`paramCoreValidateDirect`) also stayed green because native and Forth both
used the same mutated validator.  Review also found that the NUMBER_16 sweep
selected the last matching item and did not reseed identical RPN input state,
and that `paramCoreReadName` read the length byte before checking the exclusive
end and narrowed a potentially wider remaining-byte count to `uint8_t`.

This packet is the only F2 follow-up.  It corrects those review findings and
grows independent teeth for every repaired contract.  F3 remains gate-locked
until this packet is committed green.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -8` contains all four landed F2 commits, ending with:
   - `6f0ffca4b` — F2-1 extraction;
   - `69e594c71` — F2-2 bounded names;
   - `06ce84b5a` — F2-3 shared dispatch;
   - `176e0be0f` — F2-4 parity sweep.
3. In `programming/param_core.c`, the `paramCoreReadName` slice still reads
   `*(uint8_t *)(stringAddress++)` before its `stringAddress >= end` check and
   still compares through `(uint8_t)(end - stringAddress)`.  If either defect
   is already absent, STOP and report the landed slice; do not reapply it.
4. `test_param_core_bounded_names` has exactly the two landed F2-2 subcases,
   and `test_param_parity_sweep` is registered immediately after
   `test_c47_param_shared_dispatch`.
5. Both NUMBER_16 discovery loops assign their candidate without an immediate
   `break`, and neither NUMBER_16 native/Forth pair reseeds all four RPN stack
   registers before both halves.  If this does not match, STOP and report.
6. `grep -R "paramCoreDebugNameLengthReads" packages/forth-core/programming
   packages/forth-core/test_dict_reloc.c` returns no match.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`.  You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions.  If a quoted anchor, function, test, branch, literal, or identifier
does not match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`.  The tree must be clean before any edit.  Otherwise
   STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f2-5-todo.md` (outside the repo): one item per file, helper,
   test subcase, discovery repair, state-seeding repair, mutation, final gate,
   parity check, and report.  Keep it updated; mark each item in progress and
   completed as you work, and append `MUTATION APPLIED: <n>` /
   `MUTATION RESTORED: <n>` immediately.  Do not report success with an open
   item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`.  Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.`  Never
   invoke meson or ninja directly.  Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f2-5-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `items.c`,
   `config.c`, `lblGtoXeq.c`, `forth_inner.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f2-5-todo.md`,
   `git status --short`, and `git diff` are the durable task state.  After any
   compaction or uncertainty, STOP the current step and re-read those sources;
   never reconstruct packet text from memory.

**Two-attempt debugger handoff.** After the implementation first fails a
required command because of your changes, make at most two distinct repair
attempts, each followed by the relevant rerun.  This does not override any
immediate STOP rule and does not apply to an expected mutation RED.  If the
second repair is not green, STOP and report `[SOL DEBUGGER HANDOFF]` with the
command, bounded failure output, both repairs/results, current status/diff,
and remaining hypotheses.

---

## F2-5 — F2 acceptance must be bounded, isolated, first-match, and toothed

### Authority carried by this packet

The following contracts are decided; there are no open choices:

1. `paramCoreReadName(address, end)` treats `end` as exclusive.  It performs
   ZERO length-byte reads when `address >= end`, always NUL-terminates, and
   computes the copied count as the mathematical
   `min(lengthByte, end - nameBytes)` without narrowing the available-byte
   count before the comparison.
2. A self-test-only counter records actual length-byte reads.  It exists only
   under `FORTH_DEBUG_SELFTEST`; it is not a production API or persistent
   state.  A logical end-bound fixture uses a real padding byte so the test is
   deterministic without ASan or a guard-page crash.
3. The F2-4 NUMBER_8 boundary has an independent semantic oracle:
   `paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, sdlMax)` MUST be true before
   the native/Forth parity drive.  The parity comparison remains, but is not
   allowed to validate the shared function against itself.
4. NUMBER_16 discovery returns the FIRST live item in ascending item-id order
   for each class, exactly as F2-4 specified.  Each result is independently
   checked to prove no earlier matching id exists.
5. Before EACH native and Forth NUMBER_16 half, the test reseeds the complete
   RPN stack to `T=11, Z=22, Y=33, X=44` by calling `forthPushInt32` in the
   exact order `11, 22, 33, 44`, then sets `lastErrorCode = ERROR_NONE`,
   `programRunStop = PGM_STOPPED`, and `dynamicMenuItem = -1`.  Thus the two
   dispatches start from identical observable input state.  The existing
   cleanup between native program and Forth dictionary fixtures remains.
6. Parity still compares the resulting X value and `lastErrorCode`; the
   corrupted-item runtime rejection remains byte-for-byte unchanged.

### Files

Modify only:

- `packages/forth-core/programming/param_core.h`
- `packages/forth-core/programming/param_core.c`
- `packages/forth-core/test_dict_reloc.c`

Do not edit `forth_inner.c`, any F2 packet/ledger, DESIGN/history, generated
package output, or upstream `src/` during implementation.

### Targeted reads

Read one file at a time:

1. In `programming/param_core.h`, read the complete small header.
2. In `programming/param_core.c`, grep `paramCoreReadName` and read only that
   function plus five surrounding lines; separately read only
   `paramCoreValidateDirect`.
3. In `test_dict_reloc.c`, grep and read only:
   - `read_reg_int32` plus its four-push caller (the exact stack seed model);
   - `test_param_core_bounded_names` in full;
   - `test_param_parity_sweep` one subcase at a time;
   - its existing declaration and registration lines.

### Change 1 — make the bounded reader actually bound every read

In `test_dict_reloc.c`, add `#include "programming/param_core.h"` beside the
existing package includes.  This test now calls the public core directly and
reads its debug-only counter; do not add an ad-hoc `extern` declaration.

In `param_core.h`, add exactly this debug-only declaration after the public
parameter-core declarations and before the closing guard:

```c
#if defined(FORTH_DEBUG_SELFTEST)
extern uint32_t paramCoreDebugNameLengthReads;
#endif
```

In `param_core.c`, define the counter once near the includes:

```c
#if defined(FORTH_DEBUG_SELFTEST)
uint32_t paramCoreDebugNameLengthReads = 0;
#endif
```

Replace only the body of `paramCoreReadName` with the following logic.  Match
local brace/spacing style, but do not change the operations, their order, or
the counter placement:

```c
uint8_t stringLength = 0;
const uint8_t *nameBytes = end;
if(stringAddress < end) {
  stringLength = *(uint8_t *)(stringAddress++);
  nameBytes = stringAddress;
  #if defined(FORTH_DEBUG_SELFTEST)
    paramCoreDebugNameLengthReads++;
  #endif
  if((end - stringAddress) < stringLength) {
    stringLength = (uint8_t)(end - stringAddress);
  }
}
xcopy(tmpStringLabelOrVariableName, nameBytes, stringLength);
tmpStringLabelOrVariableName[stringLength] = 0;
```

The cast is legal only after the comparison proves the nonnegative remaining
count is smaller than the `uint8_t` length.  Do not cast the remaining count
inside the comparison.  Do not change any caller or the display-path reader.

### Change 2 — add two bounded-reader regression subcases

Extend `test_param_core_bounded_names` after its landed subcase 2.  Preserve
subcases 1 and 2 byte-for-byte.  Wrap both new subcases in
`#if defined(FORTH_DEBUG_SELFTEST)` / `#endif` so an ordinary PC build that
does not inject the self-test define has no reference to the debug counter.

**Subcase 3: no length byte at the exclusive end.** Use this exact direct-core
fixture:

```c
uint8_t truncatedParam[] = { STRING_LABEL_VARIABLE, 0x7F };
```

Save `firstFreeProgramByte`, then set it to `truncatedParam + 1`, reset
`paramCoreDebugNameLengthReads` to `0`, and call
`paramCoreExecuteOp(truncatedParam, ITM_GTO, PARAM_LABEL)`.  Restore
`firstFreeProgramByte` IMMEDIATELY after the call, before any assertion or
early return.  Require `paramCoreDebugNameLengthReads == 0` and
`tmpStringLabelOrVariableName[0] == 0`.  The `0x7F` byte is accessible padding
beyond the logical end; reading it is a test failure even though it cannot
crash.  Print exactly one success line containing:

`[3] PASS: missing name-length byte performed zero length-byte reads`

**Subcase 4: an available count wider than `uint8_t`.** Use an exact
`uint8_t wideParam[258] = {0};`, then set:

```c
wideParam[0] = STRING_LABEL_VARIABLE;
wideParam[1] = 2;
wideParam[2] = 'W';
wideParam[3] = '7';
```

Save `firstFreeProgramByte`, set it to `wideParam + 258`, reset the debug
counter, and call `paramCoreExecuteOp(wideParam, ITM_GTO, PARAM_LABEL)`.
Restore `firstFreeProgramByte` immediately.  From the byte after the length to
the exclusive end there are EXACTLY `256` available bytes; the old cast wraps
that count to `0`.  Require the debug counter to be `1` and
`strcmp(tmpStringLabelOrVariableName, "W7") == 0`.  Print exactly one success
line containing:

`[4] PASS: 256-byte name remainder preserved 'W7' without uint8_t wrap`

These direct calls are allowed to take the normal label-not-found path; reset
`lastErrorCode` after each because their oracle is the bounded reader, not GTO
resolution.  Do not add a program, dictionary word, label, or richer fixture.

### Change 3 — give the NUMBER_8 boundary an independent oracle

In F2-4 sweep subcase 1, immediately after computing `sdlMax` and before any
native or Forth drive, add a failure if:

```c
!paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, sdlMax)
```

The failure line must state the exact `sdlMax` and that the inclusive boundary
was rejected.  Keep the existing `param == sdlMax` and `param == sdlMax + 1`
native/Forth drives and outcome comparisons unchanged.  The existing single
subcase PASS line remains the only PASS line for this pair.

### Change 4 — first-match discovery, proved rather than assumed

In each NUMBER_16 discovery loop, add `break;` immediately after assigning the
matching id.  After the not-found guard and before executing the item, rescan
ids `1` through `discoveredId - 1`.  If an earlier item matches the same
`PTP_NUMBER_16` plus old/new `isFunctionOldParam16` predicate, print a CONFIG
FAIL naming both ids and fail that subcase.  This assertion is deliberately
independent of the discovery loop so removal of the `break` cannot silently
select the last item again.

Keep runtime discovery; do not hardcode `1297`, `1310`, `2236`, `2843`, an
item macro, or an item name.  Parameter value remains exactly `5` for both
NUMBER_16 subcases.

Update each success line to say `first`:

- `[2] PASS: first NUMBER_16 oldParam16 parity pinned (item=%u)`
- `[3] PASS: first NUMBER_16 new-form parity pinned (item=%u)`

### Change 5 — identical input state for both NUMBER_16 halves

Add one test-local helper near `read_reg_int32`:

```c
static void seedParamParityState(void)
{
  forthPushInt32(11);
  forthPushInt32(22);
  forthPushInt32(33);
  forthPushInt32(44);
  lastErrorCode = ERROR_NONE;
  programRunStop = PGM_STOPPED;
  dynamicMenuItem = -1;
}
```

Call it immediately before the native dispatch and again immediately before
the Forth dispatch in BOTH NUMBER_16 subcases.  Do not seed once per pair;
there must be four call sites total.  Do not move cleanup across a call.

Immediately after each call, use `read_reg_int32` to require the exact seeded
types/values `T=11, Z=22, Y=33, X=44` before dispatch.  A seed mismatch fails
the current subcase before either engine runs.  This precondition is what
makes the recorded post-outcomes a real parity comparison.

Within `test_param_parity_sweep` only, remove the unused local `bool err`
captures where the return value is not asserted; call `run_word` directly and
continue comparing `lastErrorCode`.  Keep subcase 4's asserted `err` variable.

### Existing tests and comments

All existing tests, including the landed F2-2 subcases, F2-3 parity test, the
F2-4 NUMBER_8 outcome comparisons, and corrupted-item rejection, must remain
green.  If any old test reddens, STOP before editing it.

### Non-goals / STOP boundaries

- No product behavior change beyond making the existing bounded reader obey
  its already-decided exclusive-end contract.
- No new PTP class, dispatch seam, Forth token, parameter encoding, label
  grammar, or F3 work.
- No change to `forth_inner.c`, display-path name readers, item tables, DESIGN,
  history, generated package files, or upstream sources.
- If the exact fixtures or pointer expressions above do not fit the landed
  function signatures, STOP and report; never adjust a literal to fit.

### Gate and required mutations

Run the full sanctioned gate green first.  Record all four bounded-name PASS
lines, all four parity-sweep PASS lines, the arena line, both success banners,
and exit 0.  Then run each mutation separately with the full sanctioned gate;
restore only its hunk manually before continuing:

1. In `paramCoreValidateDirect`, change the NUMBER_8 comparison from `<=` to
   `<`.  The new independent boundary assertion in parity subcase 1 MUST go
   RED stating that `sdlMax` was rejected.  A green run is an immediate STOP.
2. In `paramCoreReadName`, move the length-byte read AND the debug-counter
   increment before `if(stringAddress < end)`, recreating the reviewed
   read-before-bound ordering.  Bounded-name subcase 3 MUST go RED with read
   count `1` (the padding byte prevents a crash).
3. In the safe reader, replace the remaining-count comparison with the old
   narrowing comparison `(uint8_t)(end - stringAddress)`.  Bounded-name
   subcase 4 MUST go RED because the exact `256` remainder wraps to `0` and the
   captured name is empty.
4. Remove only the oldParam16 discovery loop's new `break`.  Parity subcase 2
   MUST go RED at the independent first-match CONFIG check.
5. Remove only the new-form discovery loop's new `break`.  Parity subcase 3
   MUST go RED at the independent first-match CONFIG check.

Use `/tmp/forth-f2-5-mut1.log` through `/tmp/forth-f2-5-mut5.log`.  After all
mutations, require no `MUTATION F2-5` match, run the full gate again into
`/tmp/forth-f2-5-final.log`, and record:

- the four bounded-name PASS lines;
- the four parity-sweep PASS lines;
- `FORTH SELF-TEST: ALL PASSED`;
- `==> BUILD + SELF-TEST GREEN.`;
- exit 0;
- the `FORTH ARENA` line and comparison to the pre-task baseline;
- `git diff --check` with no output;
- byte equality for each flat file and generated `files/` counterpart.

RULE-1 flash delta is required because `param_core.c` changes.  The owner runs
`make dmcp5r47`; record it as PENDING in the report if unavailable in this
session.  Do not substitute another build command.

### Commit

After final green, `git status --short` may contain only the three named flat
files, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`.  Stage those exact paths only and
commit:

```text
forth-core: F2-5 — close parameter acceptance review findings
```

Report the commit id, all required output, mutation symptoms, arena result,
RULE-1 flash status, and anything surprising.
