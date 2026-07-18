# Stage F1-2 — top-level run/SST lifetime signaling (bounded implementation prompt)

Origin: accepted R4 architecture (lifetime rulings 1-4), DESIGN.md §10.1,
`R6_RESOLUTION_PLAN.md` Step 7, and the F1-1 packet in
`QWEN_PROMPTS_F1_1_pending_reset.md`. Authored 2026-07-16 against the
post-F1-1 target tree; the execution gate below verifies the tree matches
before any edit is allowed.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

F1-2 executes only after F1-1 is committed green. Verify all of the
following; if any check fails, STOP and report the mismatch — do not adapt.

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -5` contains a commit whose subject is
   `forth-core: F1-1 — pending reset survives wrap and active frames`.
3. `grep -n "forthResetPending" packages/forth-core/forth_compile.c` shows a
   private `static bool forthResetPending = false;` plus uses inside
   `forthRunGenBump` and `forthRunGenCheckReset`.
4. `grep -n "forthInnerIsActive" packages/forth-core/forth_inner.c
   packages/forth-core/forth_dict.h` shows the implementation and prototype.
5. `grep -n "forthRunGenBump" packages/forth-core/programming/lblGtoXeq.c`
   shows exactly two production call sites: one in `fnExecute` (gated
   `programRunStop != PGM_RUNNING`) and one at the top of `runProgram` (gated
   `!nestedEngine && !singleStep && menuLabel != INVALID_VARIABLE`).
6. `grep -n "test_pending_reset_lifetime" packages/forth-core/test_dict_reloc.c`
   shows the F1-1 test and its registration.

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
   `/tmp/forth-f1-2-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f1-2-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f1-2-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, `keyboard.c`, or `test_dict_reloc.c` in full. Grep the
   named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named hook. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`packages/forth-core/QWEN_PROMPTS_F1_2_run_signaling.md`), the todo file
   `/tmp/forth-f1-2-todo.md`, and `git status --short` / `git diff` are the
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

## F1-2 — Every top-level engine entry signals a fresh Forth lifetime

### Authority carried by this packet

The accepted contract (R4 lifetime rulings; DESIGN.md §10.1; decided, not
open for interpretation):

- The single production signal site is the top of `runProgram()`, gated on
  `!nestedEngine` and nothing else. Every non-nested entry into the run
  engine — interactive `XEQ`, R/S start, R/S **resume**, run-mode single
  step (SST), programmable-menu start, solver-driven start — requests a
  fresh Forth lifetime.
- Run-mode SST is a fresh lifetime. The old `!singleStep` exclusion is
  retired (R4 lifetime ruling 3).
- R/S resume is a fresh lifetime. This deliberately changes the old
  "resume keeps the generation" behavior: the first-touch pre-scan
  re-derives every program-defined word in the new lifetime, so a
  self-contained program resumes correctly; only words defined
  *interactively during the pause* are dropped, which matches the standing
  "programs are self-contained" rule. GTO-then-R/S cold starts stop
  inheriting stale generations as a side effect.
- A nested engine entry (`programRunStop == PGM_RUNNING` at `runProgram`
  entry) must not signal. A launch made from an active `forthInner` frame is
  additionally protected inside `forthRunGenBump()` itself (F1-1's
  active-frame guard) — do not touch that function.
- `fnExecute`'s own bump (old site A) is retired: its interactive branch
  reaches `runProgram` and is signaled there; its `PGM_RUNNING` branch is a
  nested launch and must not signal. Marking the pending event is
  idempotent, so overlapping top-level paths (e.g. `execProgram`) are
  harmless.
- `forthRunGenBump`, `forthRunGenCheckReset`, and the F1-1 pending-reset
  machinery in `forth_compile.c` are correct as landed. F1-2 changes call
  sites only.

### Files

Modify only:

- `packages/forth-core/programming/lblGtoXeq.c`
- `packages/forth-core/test_dict_reloc.c`

Read-only verification:

- `packages/forth-core/keyboard.c` — the SST dispatch this task's test
  mirrors; do not edit.
- `packages/forth-core/forth_compile.c` — confirm F1-1 shapes; do not edit.

### Targeted reads

1. In `programming/lblGtoXeq.c`, grep `forthRunGenBump` (must be exactly two
   hits) and read `fnExecute` (~lines 168-211), `fnRunProgram` (~305-315),
   and the top of `runProgram` (~927-955) plus its `stopProgram` tail
   (~1013-1030). No other slices.
2. In `keyboard.c`, grep `PGM_SINGLE_STEP` and read only the ~5-line hit
   showing `programRunStop = PGM_STOPPED; runProgram(true, INVALID_VARIABLE);`.
3. In `test_dict_reloc.c`, grep
   `test_pending_reset_lifetime\|test_outer_nesting_tokenizer\|writeTestProgram\|x_is_longint\|fnGotoDot`
   and read only `test_outer_nesting_tokenizer` (the fnExecute-driven fixture
   precedent, ~lines 1556-1598), the `test_pending_reset_lifetime`
   registration line, and the `writeTestProgram`/`cleanupTestProgram` helpers.
4. In `forth_compile.c`, grep `forthRunGenBump\|forthRunGenCheckReset` and
   read only those two functions (verify F1-1 shape; no edit).

### Change 1 — retire bump site A in `fnExecute`

Delete this whole line at the top of `fnExecute` (anchor must match):

```c
  if(programRunStop != PGM_RUNNING) { /* §9.3 bump site A: interactive XEQ */ forthRunGenBump(); }
```

Nothing else in `fnExecute` changes.

### Change 2 — centralize the signal at `runProgram` entry

Replace this line (anchor must match):

```c
  if(!nestedEngine && !singleStep && menuLabel != INVALID_VARIABLE) { /* §9.3 bump site B: menu-key start */ forthRunGenBump(); }
```

with:

```c
  if(!nestedEngine) { /* F1-2: sole lifetime signal — every top-level engine entry (XEQ, R/S, SST, menu, solver). forthRunGenBump() defers while a Forth frame is active. */ forthRunGenBump(); }
```

Position is normative: same statement position (after `nestedEngine` is
computed, before `lastErrorCode = ERROR_NONE;` / `programRunStop =
PGM_RUNNING;`). Do not move, reorder, or duplicate it. After both changes,
`grep -c forthRunGenBump packages/forth-core/programming/lblGtoXeq.c` must
print `1`.

### Change 3 — focused integration test

Add `test_run_entry_lifetime_signaling` to `test_dict_reloc.c`, registered
immediately after `test_pending_reset_lifetime`'s registration lines (same
`printf("  [DEBUG] running ...")` + `fail |= ...();` + `forthDictClear();`
pattern as its neighbors).

Shared fixture — one real program (model: `test_outer_nesting_tokenizer`):

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '2', 'A',            /* step 1: LBL 'F2A' */
    0x8B, 0x1A, 0xFD, 0x01, '1',                /* step 2: ITM_FORTH "1" */
    0x04                                        /* step 3: RTN */
  };
```

Write it once with `writeTestProgram`, resolve
`findNamedLabel("F2A", GLOBAL_LABELS)` (STOP-fail the test if
`INVALID_VARIABLE`). Save `programRunStop` at entry and restore it on every
exit path; run each engine drive with the values specified below;
`lastErrorCode = ERROR_NONE` before every drive. Clean up with
`forthDictClear()` + `cleanupTestProgram()` on every path.

Three independently reported subcases (one PASS line each):

1. **Interactive XEQ start is a fresh lifetime.** Set
   `programRunStop = PGM_STOPPED`. Baseline drive:
   `dynamicMenuItem = -1; fnExecute(lbl);` — require no error (this consumes
   any prior pending event; `dynamicMenuItem = -1` is mandatory before every
   `fnExecute`/`fnGotoDot` in this test — leftover menu state reroutes label
   resolution). Then `forthOuterInterpret(": F2X 7 ;")` and require
   `forthFindColon("F2X", &idx)` true. Drive again:
   `dynamicMenuItem = -1; fnExecute(lbl);`. Require no error,
   `x_is_longint(1)`, and `F2X` **absent** — the real XEQ start, through the
   real `runProgram`, consumed the interactive word.
2. **Run-mode SST is a fresh lifetime.** Define `: F2S 8 ;` and require it
   present. Position on the Forth step: `dynamicMenuItem = -1; fnGotoDot(2);`
   then require `currentStep == beginOfProgramMemory + 6` (the LBL step is 6
   bytes; STOP-fail otherwise). Mirror the keyboard SST dispatch exactly:
   `programRunStop = PGM_STOPPED; runProgram(true, INVALID_VARIABLE);`.
   Require no error, `x_is_longint(1)`, and `F2S` absent. This pins the NEW
   contract (the old code excluded `singleStep` from signaling).
3. **A nested engine entry preserves the active lifetime.** Define
   `: F2N 9 ;` and require it present. Reposition:
   `dynamicMenuItem = -1; fnGotoDot(2);`. Set
   `programRunStop = PGM_RUNNING;` (simulated enclosing engine), then
   `runProgram(false, INVALID_VARIABLE);`, then restore `programRunStop`.
   Require no error, `x_is_longint(1)`, and `F2N` **still present** — the
   nested entry made no signal and the step ran in the same lifetime (the
   program was already first-touched this generation, so no re-scan clears
   anything).

A failure path must restore `programRunStop` before returning. Do not call
`forthRunGenBump()` anywhere in this test — every signal must come from the
production `runProgram` site.

### Existing tests and comments

All existing tests must stay green unchanged. In particular
`test_pending_reset_lifetime`, `test_program_step_gen_reset`,
`test_prescan_generation_rearm`, and every other test that calls
`forthRunGenBump()` directly keeps doing so — the function and its F1-1
semantics are untouched; only production call sites moved. If any of them
goes red, STOP (rule 6). Update prose only where a nearby comment names
"bump site A/B" or the `!singleStep`/menu-key gate as current behavior
(the two comments this task's hunks already replace are sufficient; do not
sweep other files).

### Non-goals / STOP boundaries

- No edits to `forth_compile.c`, `forth_inner.c`, `forth_dict.h`,
  `forth_dict.c`, `keyboard.c`, or upstream `src/`.
- No changes to `fnRunProgram`, `fnStopProgram`, `execProgram`, or the
  `stopProgram` tail of `runProgram`.
- No new bump/signal sites anywhere else; no removal of `forthRunGenBump`'s
  prototype.
- No dynamic scan registry (F1-3), no `RECURSE` (F1-4), no validator work
  (F1-5), no DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue only
when the focused test goes RED for the named reason:

1. Delete the `if(!nestedEngine) { ... forthRunGenBump(); }` line in
   `runProgram`. Subcase 1 must go RED because `F2X` survives the second
   XEQ start (no production signal remains anywhere).
2. Change the gate to `if(!nestedEngine && !singleStep)`. Subcase 2 must go
   RED because `F2S` survives the single-step drive.
3. Change the gate to `if(true)` (signal on nested entries too). Subcase 3
   must go RED because `F2N` is cleared during the nested drive.

After all mutations, grep for `MUTATION F1-2` (there must be no match), run
the full gate green again, and record:

- all three PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=... freeRamDelta=...`;
- `git diff --check`;
- byte equality between each flat file and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only the two
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F1-2 — runProgram entry is the sole top-level lifetime signal
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the two removed/replaced production hunks, the
`grep -c forthRunGenBump` count in `lblGtoXeq.c` (must be 1), the three
focused PASS lines, each mutation's RED symptom, the final gate and arena
lines, commit hash, and anything surprising.
