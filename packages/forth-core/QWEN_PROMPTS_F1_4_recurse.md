# Stage F1-4 — compile-only `RECURSE` (bounded implementation prompt)

Origin: accepted R4 architecture (DESIGN.md §10.1: "standard compile-only
immediate word; the open definition stays smudged until `;`; `RECURSE` emits
a call to the definition under construction without making its name
visible"), `R6_RESOLUTION_PLAN.md` Step 7. Authored 2026-07-16 against the
post-F1-3 target tree; the execution gate below verifies the tree matches
before any edit is allowed.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

F1-4 executes only after F1-3 is committed green. Verify all of the
following; if any check fails, STOP and report the mismatch — do not adapt.

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -10` contains the F1-1, F1-2, and F1-3 stage commits
   (`pending reset survives wrap`, `sole top-level lifetime signal`,
   `dynamic, arena-backed, cliff-free`).
3. `grep -n "PRIM_COUNT = 11" packages/forth-core/forth_prims.c` matches
   (the table has exactly 11 entries, none immediate). If the count
   differs, STOP.
4. `grep -n "FF_IMMEDIATE" packages/forth-core/forth_compile.c` shows the
   compile-state immediate dispatch
   (`state == STATE_COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE)`).
5. `grep -n "openDef" packages/forth-core/forth_dict.c` shows the private
   `openDef` struct with fields `here, latest, count, entryOff, open`, and
   `startDefinition` saving `openDef.count = fdict.count;` BEFORE calling
   `forthDictAllocate` (which increments `fdict.count`).
6. `grep -n "test_scan_dynamic_no_cliff" packages/forth-core/test_dict_reloc.c`
   shows the F1-3 test and its registration.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, or identifier does not
match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f1-4-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f1-4-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f1-4-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, or `test_dict_reloc.c` in full. Grep the named anchors and
   read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named hook. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`packages/forth-core/QWEN_PROMPTS_F1_4_recurse.md`), the todo file
   `/tmp/forth-f1-4-todo.md`, and `git status --short` / `git diff` are the
   ONLY durable truth about task state. If your context is compacted or
   summarized, or you are unsure what you have already done: STOP the
   current step, re-read this packet file and the todo file, run
   `git status --short` and `git diff --stat`, and check for an unrestored
   `MUTATION APPLIED` marker before doing anything else. Never reconstruct
   packet text, anchors, or code blocks from memory — re-read them from this
   file every time you need them.

**Two-attempt debugger handoff.** After the task implementation first fails a
required command because of your changes, you may make at most two distinct
repair attempts, each followed by the relevant rerun. This does not override
an immediate STOP rule and does not apply to an expected mutation RED. If the
second repair is not green, STOP and report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and relevant verbatim output;
- repair 1 and result;
- repair 2 and result;
- current `git status --short`, `git diff --stat`, and relevant diff;
- remaining hypotheses and surprises.

---

## F1-4 — `RECURSE` emits a self-call; the open name stays invisible

### Authority carried by this packet

Decided semantics (standard Forth, pinned to this engine's structures):

- `RECURSE` is the table's **first immediate primitive** (`FF_IMMEDIATE`).
  In compile state it executes at compile time and emits
  `FTOK_CALL_BASE + index-under-construction` into the open definition's
  body. It never resolves by name and never unsmudges anything.
- The index under construction is `openDef.count` — `startDefinition`
  snapshots `fdict.count` *before* `forthDictAllocate` increments it, so the
  entry being built has exactly that index. Nested definitions are
  impossible (`:` in compile state aborts), so the value is stable while
  open. `startDefinition`'s existing `fdict.count >= 0x6F00` cap guarantees
  the emitted token stays within the call range (`0x1000..0x7EFF`).
- Interpret-state `RECURSE` (no open definition) is an error:
  `ERROR_OPERATION_UNDEFINED`, nothing emitted, dictionary untouched. This
  is the same code used for other "not a legal Forth phrase here"
  rejections.
- Bare self-reference stays what it already is: `forthFindColon` skips
  `FF_SMUDGE`, so inside `: W … W … ;` the inner `W` resolves to a PREVIOUS
  unsmudged `W` (standard redefinition idiom) or errors if none exists.
  `RECURSE` is the only way to call the definition under construction.
- Runtime is untouched: the emitted token is an ordinary `FTOK_CALL`;
  unbounded recursion is already bounded by the return-stack depth
  (`FORTH_RSTACK_DEPTH` 64 → `ERROR_RAM_FULL`) and the runaway cap.
- The pre-scan path needs no special casing: in `DEFS_ONLY` mode compile
  state processes tokens normally (immediates execute); in `SKIP_DEFS` mode
  the `:`…`;` region is consumed as text.

### Files

Modify only:

- `packages/forth-core/forth_prims.c`
- `packages/forth-core/forth_dict.h`
- `packages/forth-core/forth_dict.c`
- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. `forth_prims.c` in full (it is ~52 lines).
2. In `forth_dict.c`, grep `openDef\|startDefinition\|forthDictAllocate` and
   read `startDefinition`, `abortDefinition`, `isDefinitionOpen`, and the
   `openDef` declaration only.
3. In `forth_compile.c`, grep `FF_IMMEDIATE` and read the primitive-lookup
   branch of `forthOuterRun` (~15 lines) only. No edits to this file.
4. In `forth_dict.h`, read the lookup/definition prototype block.
5. In `test_dict_reloc.c`, grep
   `begin_word\|end_word\|forthTestGetDepth\|forthTestGetRsp\|test_scan_dynamic_no_cliff`
   and read only the `begin_word`/`end_word` helpers, one test that uses
   `forthTestGetRsp`, and the F1-3 test's registration lines.

### Change 1 — the open-definition index accessor (forth_dict.h / forth_dict.c)

In `forth_dict.h`, next to `isDefinitionOpen`:

```c
/* F1-4: index of the definition under construction (== openDef.count;
   the entry is smudged and invisible to forthFindColon until ';').
   Returns false when no definition is open. */
bool forthOpenDefinitionIndex(uint16_t *idx);
```

In `forth_dict.c`, next to `isDefinitionOpen`:

```c
bool forthOpenDefinitionIndex(uint16_t *idx)
{
  if (!openDef.open) {
    return false;
  }
  *idx = openDef.count;
  return true;
}
```

### Change 2 — the primitive (forth_prims.c)

Add `#include "forth_dict.h"` after the existing includes, and mirror the
call-token base the way the other engine files do:

```c
#define FTOK_CALL_BASE 0x1000   /* mirror forth_compile.c / forth_inner.c */
```

Add the wrapper with the other wrapper stubs:

```c
/* F1-4: compile-only immediate. Emits a call to the definition under
 * construction; the smudged name itself stays invisible until ';'. */
static void pRecurse(void)
{
  uint16_t idx;
  if (!forthOpenDefinitionIndex(&idx)) {
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if (!forthDictEmit((ftoken_t)(FTOK_CALL_BASE + idx))) {
    if (lastErrorCode == ERROR_NONE) {
      displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
  }
}
```

Extend the table append-only:

- enum: `PRIM_RECURSE = 11,` and `PRIM_COUNT = 12`.
- row: `[PRIM_RECURSE] = { "RECURSE", FF_IMMEDIATE, pRecurse },`

Do not reorder existing rows; the table is index-stable.

### Change 3 — focused test

Add `test_recurse_compile_only`, registered immediately after
`test_scan_dynamic_no_cliff`. Fixture discipline as usual (`forthDictClear()`
before and after; `lastErrorCode` pre-cleared per subcase; save/restore
`programRunStop`, `PGM_RUNNING` only around `forthProgramStep`;
`cleanupTestProgram()` on every path of the program subcase). Five
independently reported subcases, one PASS line each:

1. **Body shape.** `forthOuterInterpret(": SELFW RECURSE ;")` — no error,
   `forthFindColon("SELFW", &idx)` true with `idx == fdict.count - 1`.
   Compute `bodyOff = fdict.latest + (uint16_t)TO_BLOCKS(4 + 5) * BYTES_PER_BLOCK`
   (name length 5) and read two tokens from `fdict.base + bodyOff` via
   `memcpy`: token 0 must equal `(ftoken_t)(0x1000 + idx)` (the self-call,
   NOT the RECURSE prim token `0x000C`), token 1 must equal `FTOK_EXIT`.
2. **Runtime self-call is bounded and unwinds.** Execute
   `forthOuterInterpret("SELFW")`: require
   `lastErrorCode == ERROR_RAM_FULL` (return-stack depth exhausts), then
   `forthTestGetDepth() == 0` and `forthTestGetRsp() == 0` (INNER_LEAVE
   unwound). Reset `lastErrorCode`.
3. **Interpret state rejects.** Snapshot `fdict.here` and `fdict.count`;
   `forthOuterInterpret("RECURSE")`; require
   `lastErrorCode == ERROR_OPERATION_UNDEFINED` and both snapshots
   unchanged (nothing emitted, no lazy allocation growth attributable to an
   emit — compare against the snapshots, not against zero).
4. **RECURSE is not the bare name.** `forthOuterInterpret(": WOLD 5 ;")`
   then `forthOuterInterpret(": WOLD WOLD 1 + ;")` — both without error
   (the inner `WOLD` resolves to the first, unsmudged definition). Execute
   `forthOuterInterpret("WOLD")`: newest-first lookup runs the redefinition
   → require `x_is_longint(6)`.
5. **Program pre-scan compiles it; the emitted call really recurses.** One
   real program step `": PRW RECURSE ; PRW"` (payload length 19):

   ```c
   uint8_t prog[] = { 0x8B, 0x1A, 0xFD, 19,
     ':', ' ', 'P', 'R', 'W', ' ',
     'R', 'E', 'C', 'U', 'R', 'S', 'E', ' ', ';', ' ',
     'P', 'R', 'W' };
   ```

   The pointer contract is literal: offsets 0..2 are the `ITM_FORTH` opcode,
   offset 3 is the source-length byte, and offset 4 is the first `':'`.
   `forthProgramStep` takes `payload` at offset 3, never offset 4 or the first
   source character. There is no leading marker step in this fixture.

   After `writeTestProgram` succeeds, set
   `const uint8_t *payload = beginOfProgramMemory + 3;`, then under the
   `PGM_RUNNING` wrap call `forthProgramStep(payload)`. Require
   `lastErrorCode == ERROR_RAM_FULL` (the tail call spun on the real
   self-call until the return stack filled — proving DEFS_ONLY compiled the
   immediate correctly) and `forthFindColon("PRW", &idx)` true (the scan
   succeeded; only the execution errored, so no rollback). Reset the error,
   clean up.

### Existing tests and comments

All existing tests stay green unchanged. `_Static_assert(PRIM_COUNT <=
0x0FFF, ...)` in `forth_prims.c` stays. If any old test pins
`forthPrimCount == 11` or a prim-table byte size, STOP and report (rule 6).

### Non-goals / STOP boundaries

- No control-flow words (`IF`/`BEGIN`/…), no other immediates, no new token
  types — the emitted token is a plain `FTOK_CALL`.
- No changes to `forth_compile.c` or `forth_inner.c`: the immediate dispatch
  path and the runtime already support everything this task needs. If they
  appear not to, STOP and report instead of patching them.
- No exposure of `openDef` beyond the single accessor; do not export the
  struct.
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue only
when the focused test goes RED for the named reason:

1. Change the table row's flags from `FF_IMMEDIATE` to `0`. Subcase 1 must
   go RED: compile state emits the prim token `0x000C` instead of executing
   the immediate, so token 0 is not the self-call.
2. In `forthOpenDefinitionIndex`, return `openDef.count + 1`. Subcase 1
   must go RED (wrong call token); subcase 2 typically also fails with
   `ERROR_INVALID_CORRUPTED_DATA` — record whatever it shows.
3. In `pRecurse`, delete the `!forthOpenDefinitionIndex(&idx)` guard branch
   (always emit, using an uninitialized/zero index). Subcase 3 must go RED:
   no `ERROR_OPERATION_UNDEFINED`, and/or the `here` snapshot moved.

After all mutations, grep for `MUTATION F1-4` (there must be no match), run
the full gate green again, and record:

- all five PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=... freeRamDelta=...`;
- `git diff --check`;
- byte equality between each flat file and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only the four
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F1-4 — compile-only RECURSE calls the open definition by index
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the new prim row and accessor, the five PASS lines, each mutation's
RED symptom, the final gate and arena lines, commit hash, and anything
surprising.
