# Stage F15-1 — end-to-end run-lifecycle acceptance (bounded implementation prompt)

Origin: DESIGN §8.9 items 1, 7(a,b), 9(a,b) as reconciled 2026-07-17
(`6345f6c64`), pinning the landed F1 lifecycle (§8.3). Stage ledger:
`QWEN_PROMPTS_F15_harness.md`. Authored 2026-07-17 against the post-F1 tree
with every anchor grep-verified and every payload length machine-verified.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -8` contains commits whose subjects include
   `F1-5 — restored threaded code is validated or orphan-cleared` and
   `reconcile §8.3/§8.9 to the landed F1 lifecycle`.
3. `grep -n "F1-2: sole lifetime signal" packages/forth-core/programming/lblGtoXeq.c`
   matches exactly one line: the `if(!nestedEngine)` gate around
   `forthRunGenBump()` at the top of `runProgram`.
4. `grep -n "op == ITM_FORTH" packages/forth-core/programming/lblGtoXeq.c`
   shows the §8.2 arm (`else if(op == ITM_FORTH)`) with a
   `forthProgramStep(step);` call 1-4 lines below it.
5. `grep -n "stepsToBeAdvanced = executeOneStep" packages/forth-core/programming/lblGtoXeq.c`
   matches exactly once, and the IMMEDIATELY following line is
   `if(lastErrorCode == ERROR_NONE) {` (the run-loop halt guard).
6. `grep -n "#define ITM_STOP" packages/forth-core/items.h` shows value
   `70` (fixture byte `0x46`).
7. `grep -n "test_pending_reset_lifetime\|test_validate_restored_bodies"
   packages/forth-core/test_dict_reloc.c` shows both registered, and
   `grep -n "test_accept_run_lifecycle" packages/forth-core/test_dict_reloc.c`
   returns nothing (this task adds it).

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
   `/tmp/forth-f15-1-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f15-1-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f15-1-gate.log | head -n 30` or a grep
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
   (`design-docs/forth-core/QWEN_PROMPTS_F15_1_run_lifecycle.md`), the todo file
   `/tmp/forth-f15-1-todo.md`, and `git status --short` / `git diff` are the
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

## F15-1 — The real run engine proves the F1 lifecycle end to end

### Authority carried by this packet

The landed F1 lifecycle (already implemented and unit-pinned; this task adds
the END-TO-END drives through the real `XEQ`/`R/S` engine, no production
changes):

- The sole lifetime signal is `runProgram` entry, gated `!nestedEngine`.
  Every non-nested entry — interactive `XEQ`, R/S start, R/S **resume**,
  SST, menu, solver — requests a fresh lifetime.
- The first safe `forthProgramStep` entry consumes the pending reset: it
  clears the dictionary (interactive words included) and the first-touch
  scan records, then the touched program is pre-scanned (`DEFS_ONLY`),
  re-deriving its definitions.
- Therefore: R/S resume of a self-contained program works (its words are
  re-derived), while a word defined interactively during the pause is
  dropped. Both halves are normative.
- Pre-scan compiles definitions only; interpret-state (non-definition)
  source text is not executed during the scan. Execution happens when the
  step itself runs (`SKIP_DEFS`). If the targeted reads below contradict
  this, STOP and report.
- An unterminated definition in a source step surfaces
  `ERROR_INVALID_NAME` and aborts the open definition (no smudged leak) —
  pinned at unit level by `test_unterminated_def_errors`.
- The run loop halts at a step whose execution left `lastErrorCode` set
  (the guard after `executeOneStep` in `runProgram`); `currentStep` stays
  at the failing step and later steps do not run.
- Fixture byte facts (all verified against the tree): marker step =
  `0x8B 0x1A 0xFD 0x00`; source step = `0x8B 0x1A 0xFD <len> <bytes>`;
  `LBL 'XYZ'` = `0x01 0xFD 0x03 'X' 'Y' 'Z'`; `RTN` = `0x04`; `STOP` =
  `0x46` (ITM_STOP 70, single byte). Payload lengths are machine-verified:
  `": SQ DUP * ;"` = 12 (`0x0C`), `"3 SQ"` = 4, `": SQ DUP *"` = 10,
  `"3 SQX"` = 5, `"7"` = 1. Transcribe them exactly (AGENTS.md: literals
  are law).

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

Read-only (and mutation-only, always restored):

- `packages/forth-core/programming/lblGtoXeq.c`

### Targeted reads

1. In `test_dict_reloc.c`, grep
   `test_run_entry_lifetime_signaling\|test_marker_parity\|writeTestProgram\|cleanupTestProgram\|x_is_longint`
   and read only: the whole of `test_run_entry_lifetime_signaling` (THE
   drive model — `programRunStop` save/restore, `lastErrorCode` pre-clear,
   `dynamicMenuItem = -1` before every `fnExecute`, label resolution via
   `findNamedLabel(..., GLOBAL_LABELS)`, cleanup on every path), the
   fixture array of `test_marker_parity`, the `writeTestProgram` /
   `cleanupTestProgram` helpers, `x_is_longint`, and the registration lines
   after `test_validate_restored_bodies`.
2. In `programming/lblGtoXeq.c`, grep
   `F1-2: sole lifetime signal\|op == ITM_FORTH\|stepsToBeAdvanced = executeOneStep\|void fnRunProgram`
   and read ≤10 lines around each hit only (mutation anchors and the
   `fnRunProgram` body — note it sets `dynamicMenuItem = -1` itself).
3. In `forth_compile.c`, grep `FORTH_OUTER_DEFS_ONLY\|FORTH_OUTER_SKIP_DEFS`
   and read ≤15 lines around the mode dispatch to confirm the
   scan-vs-execute contract stated above. No edits to this file.

### Change 1 — the focused acceptance test

Add `static int test_accept_run_lifecycle(void)`, registered immediately
after `test_validate_restored_bodies` (same `printf("  [DEBUG] running
...")` + `fail |= ...();` + `forthDictClear();` pattern as its neighbors).
Five independently reported subcases, one PASS line each. Fixture
discipline throughout, copied from `test_run_entry_lifetime_signaling`:
save `programRunStop` at entry and restore it on EVERY exit path;
`lastErrorCode = ERROR_NONE` and `dynamicMenuItem = -1` before every
`fnExecute`; resolve each label with `findNamedLabel("...", GLOBAL_LABELS)`
and FAIL-cleanup-return if `INVALID_VARIABLE`; `forthDictClear()` +
`cleanupTestProgram()` between subcases and on every failure path.

**Subcase 1 — §8.9 item 1: define-and-use in one program.**

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '5', 'A',                       /* LBL 'F5A'   */
    0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH      */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',       /* : SQ DUP * ; */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'S', 'Q',            /* 3 SQ        */
    0x8B, 0x1A, 0xFD, 0x00,                                /* FORTH«      */
    0x04                                                   /* RTN         */
  };
```

Drive: `programRunStop = PGM_STOPPED;` then `fnExecute(lbl)`. Require no
error and `x_is_longint(9)` (this checks `dtLongInteger` AND the value —
§8.9 item 1 verbatim). Keep the program in place for subcase 2.

**Subcase 2 — §8.9 item 9(a): second run identical, no accumulation.**
After subcase 1, record `uint16_t count1 = fdict.count;`. Drive the same
label again (same discipline). Require no error, `x_is_longint(9)`, and
`fdict.count == count1`. Then clean up this program.

**Subcase 3 — §8.9 item 9(b): R/S resume is a fresh lifetime that
re-derives.** New program (label `F5B`), STOP between definition and use:

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '5', 'B',                       /* LBL 'F5B'   */
    0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH      */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',       /* : SQ DUP * ; */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x46,                                                  /* STOP        */
    0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'S', 'Q',            /* 3 SQ        */
    0x8B, 0x1A, 0xFD, 0x00,                                /* FORTH«      */
    0x04                                                   /* RTN         */
  };
```

Drive `fnExecute(lbl)`; require no error and
`programRunStop == PGM_STOPPED` (the STOP halted the run). During the
pause: `forthOuterInterpret(": PZW 5 ;")`, require `PZW` present. Resume
with `lastErrorCode = ERROR_NONE; fnRunProgram(0);` (it sets
`dynamicMenuItem` itself). Require: no error, `x_is_longint(9)` (the
program is self-contained — `SQ` was re-derived by the fresh lifetime's
first-touch pre-scan), `forthFindColon("SQ", &idx)` true, and
`forthFindColon("PZW", &idx)` FALSE — the pause-defined word is dropped.
Both halves are the F1 pin; assert them independently with clear FAIL
messages. Clean up.

**Subcase 4 — §8.9 item 7(a): unterminated definition halts the run,
no smudged leak.** New program (label `F5C`):

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '5', 'C',                       /* LBL 'F5C'   */
    0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH      */
    0x8B, 0x1A, 0xFD, 0x0A, ':', ' ', 'S', 'Q', ' ',       /* : SQ DUP *  */
    'D', 'U', 'P', ' ', '*',
    0x8B, 0x1A, 0xFD, 0x01, '7',                           /* 7           */
    0x04                                                   /* RTN         */
  };
```

Baseline: `forthOuterInterpret("42")` and require `x_is_longint(42)`.
Drive `fnExecute(lbl)`. Require `lastErrorCode == ERROR_INVALID_NAME` and
`x_is_longint(42)` still — the run halted at the bad step; the later `7`
step never executed. Then `lastErrorCode = ERROR_NONE;
forthOuterInterpret(": SQ 2 ;")` — require no error and `SQ` found (the
aborted definition left no smudge). Clean up.

**Subcase 5 — §8.9 item 7(b): undefined word halts at its step.** New
program (label `F5D`):

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'F', '5', 'D',                       /* LBL 'F5D'   */
    0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH      */
    0x8B, 0x1A, 0xFD, 0x05, '3', ' ', 'S', 'Q', 'X',       /* 3 SQX       */
    0x8B, 0x1A, 0xFD, 0x01, '7',                           /* 7           */
    0x04                                                   /* RTN         */
  };
```

Drive `fnExecute(lbl)`. Require `lastErrorCode == ERROR_FUNCTION_NOT_FOUND`
and `x_is_longint(3)` — `3` was pushed, `SQX` failed, the run halted, the
`7` step never executed. Reset the error. Clean up.

The pointer contract does not arise in this task: every drive goes through
the real engine by label; never call `forthProgramStep` directly here.

### Existing tests and comments

All existing tests stay green unchanged — this task adds one test and
touches no production code. If any existing test reddens, STOP (rule 6).

### Non-goals / STOP boundaries

- No edits to `lblGtoXeq.c` outside the three mutations (each restored).
- No entry-state, display-parity, glyph, type-parity, or XEQ-name work
  (F15-2..F15-5 own those §8.9 items).
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue only
when the named subcase goes RED for the named reason:

1. In the §8.2 arm of `executeOneStep` (`else if(op == ITM_FORTH)`),
   comment out only the `forthProgramStep(step);` line. Subcase 1 must go
   RED: source steps became no-ops, X is not 9. (Subcases 3-5 may redden
   collaterally; record whatever shows.)
2. Change the run-loop halt guard `if(lastErrorCode == ERROR_NONE) {`
   (the line after `stepsToBeAdvanced = executeOneStep(currentStep);`) to
   `if(true) {`. Subcases 4 and 5 must go RED: the run continues past the
   failing step (X becomes 7, or the error state differs — record the
   actual symptom).
3. Change the sole signal gate to
   `if(!nestedEngine && menuLabel != INVALID_VARIABLE)` (the pre-F1-2
   menu-key gate). Subcase 3 must go RED: the R/S resume no longer signals
   a fresh lifetime and `PZW` survives the resume.

After all mutations, grep for `MUTATION F15-1` (there must be no match) and
require `git diff` restricted to `packages/forth-core/programming/` to be
empty (all mutations reversed). Run the full gate green again and record:

- all five PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=...` (the §5.4 acceptance-run
  arena report is mandatory for this stage);
- `git diff --check`;
- byte equality between `test_dict_reloc.c` and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only
`packages/forth-core/test_dict_reloc.c`, its generated `files/`
counterpart, and `packages/forth-core/.refresh-manifest.json`. Stage those
exact paths only and commit:

```text
forth-core: F15-1 — end-to-end run-lifecycle acceptance (§8.9 items 1, 7, 9)
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the new test's five PASS lines, each mutation's RED symptom, the
final gate and arena lines, commit hash, and anything surprising —
explicitly including any assertion in this packet that did not match
observed engine behavior (that is architect feedback, not something to
adapt around).
