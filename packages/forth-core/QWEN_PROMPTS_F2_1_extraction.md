# Stage F2-1 — extract the native parameter core (bounded implementation prompt)

Origin: DESIGN §10.2 via the stage trace in `QWEN_PROMPTS_F2_core.md`.
Authored 2026-07-17 against the post-F1.5 target tree; the gate below fails
closed on any drift.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -10` contains a commit whose subject is
   `forth-core: F15-5 — PEM XEQ-by-name records a name step (§8.9 item 10)`.
3. `grep -n "static void _executeOp" packages/forth-core/programming/lblGtoXeq.c`
   matches exactly once, and
   `grep -n "static void _executeWithIndirectRegister\|static void _executeWithIndirectVariable"
   packages/forth-core/programming/lblGtoXeq.c` shows both statics.
4. `grep -rn "param_core" packages/forth-core/ --include="*.c" --include="*.h"`
   returns nothing (this task introduces the pair).
5. `grep -n "#define ITM_STO \|#define ITM_RCL " packages/forth-core/items.h`
   shows values 44 and 51.
6. `grep -n "forthResolveXEQ" packages/forth-core/programming/lblGtoXeq.c`
   matches inside `_executeOp`'s PARAM_LABEL arm (the Forth fallback this
   task relocates verbatim).
7. `grep -n "test_param_core_extraction" packages/forth-core/test_dict_reloc.c`
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
   `/tmp/forth-f2-1-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f2-1-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f2-1-gate.log | head -n 30` or a grep
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
   (`packages/forth-core/QWEN_PROMPTS_F2_1_extraction.md`), the todo file
   `/tmp/forth-f2-1-todo.md`, and `git status --short` / `git diff` are the
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

## F2-1 — `_executeOp` becomes the public, package-owned parameter core

### Authority carried by this packet

Decided architecture (stage trace; no open choices):

- This task is a PURE EXTRACTION — behavior must be byte-identical. The
  three statics `_executeOp`, `_executeWithIndirectRegister`,
  `_executeWithIndirectVariable` move VERBATIM from
  `programming/lblGtoXeq.c` into a new package-new pair
  `programming/param_core.h` / `programming/param_core.c`. `_executeOp` is
  renamed `paramCoreExecuteOp` and exported; the two helpers stay static in
  the new file. Their dependency closure is global APIs only (verified in
  the trace); no other lblGtoXeq.c static is referenced EXCEPT
  `_putLiteral`.
- `_putLiteral` STAYS in lblGtoXeq.c. `param_core.h` declares
  `void paramCorePutLiteral(uint8_t *literalAddress);` and lblGtoXeq.c
  implements it directly below `_putLiteral` as a one-line forwarder.
  Inside the moved `paramCoreExecuteOp`, replace any `_putLiteral(`
  call with `paramCorePutLiteral(`.
- Every `_executeOp(` call site in lblGtoXeq.c becomes
  `paramCoreExecuteOp(` with identical arguments (the trace found the
  SOLVE case, the PTP_KEYG_KEYX case, and the general default arm — if you
  find a different set, STOP and report; do not adapt).
- The package system auto-includes new package files: create the two flat
  files and nothing else — no build registration exists.
- The relocated PARAM_LABEL arm carries the Forth XEQ fallback
  (`forthResolveXEQ` → `ITM_FCALL` / item / `ERROR_LABEL_NOT_FOUND`) —
  move it byte-identically; its label-kind comment block moves with it.
- Native step-byte facts for the test (machine-verified): `STO 05` step =
  `0x2C 0x05` (ITM_STO 44), `LBL 'XYZ'` = `0x01 0xFD 0x03 X Y Z`, `RTN` =
  `0x04`, marker = `0x8B 0x1A 0xFD 0x00`, source step =
  `0x8B 0x1A 0xFD <len> <bytes>`, XEQ-name step = `0x03 0xFD <len> <glyphs>`.
  Payload `": W7 7 ;"` is 8 bytes; name `"W7"` is 2.
- The subcase-2 fixture is 32 bytes. Its source step begins at
  `beginOfProgramMemory + 10`; its XEQ-name step begins at
  `beginOfProgramMemory + 26`.

### Files

Modify only:

- `packages/forth-core/programming/param_core.h` (new)
- `packages/forth-core/programming/param_core.c` (new)
- `packages/forth-core/programming/lblGtoXeq.c`
- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. In `programming/lblGtoXeq.c`, grep
   `static void _executeOp\|static void _executeWithIndirect\|_putLiteral\|_executeOp(`
   and read: the three functions in full (they are the payload being moved
   — read them once, move them, never reconstruct from memory), the
   `_putLiteral` definition's first 5 lines, and every `_executeOp(` call
   site with 3 lines of context.
2. In `test_dict_reloc.c`, grep
   `test_accept_run_lifecycle\|test_xeq_word_still_calls\|writeTestProgram\|x_is_longint\|y_is_longint`
   and read the F15-1 test's subcase-1 fixture/cleanup discipline, the
   existing one-step XEQ-to-Forth regression's `executeOneStep` drive, the
   helpers, and the registration lines after the newest test.
3. `head -n 25` of another package-new pair for header style: grep
   `#ifndef\|#include` in `forth_dict.h` and mirror its guard/include
   idiom in `param_core.h`.

### Change 1 — the new pair

`param_core.h`: header guard, the two prototypes
(`void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode);`
and `void paramCorePutLiteral(uint8_t *literalAddress);`), a 4-6 line
header comment naming §10.2 and the byte-identical-extraction contract.

`param_core.c`: the include set it needs (start from lblGtoXeq.c's
includes minus what the moved code does not use; the gate's compile step is
the oracle), then the two static helpers and `paramCoreExecuteOp`, all
moved verbatim (rename + the `paramCorePutLiteral` substitution are the
ONLY textual changes).

### Change 2 — lblGtoXeq.c

Delete the three moved functions; add `#include "param_core.h"` next to
the file's local includes; add the `paramCorePutLiteral` forwarder below
`_putLiteral`; rewrite every `_executeOp(` call site to
`paramCoreExecuteOp(`. After the change,
`grep -c "_executeOp" packages/forth-core/programming/lblGtoXeq.c` must
print `0`.

### Change 3 — focused extraction test

Add `test_param_core_extraction`, registered after the newest existing
test, F15-1 fixture discipline (save/restore `programRunStop`,
`lastErrorCode = ERROR_NONE` + `dynamicMenuItem = -1` before each drive,
label resolution FAIL-guard, cleanup on every path). Two independently
reported subcases:

1. **Direct register parameter through the moved core.** Program:

   ```c
   uint8_t prog[] = {
     0x01, 0xFD, 0x03, 'F', '2', 'E',   /* LBL 'F2E' */
     0x2C, 0x05,                        /* STO 05    */
     0x04                               /* RTN       */
   };
   ```

   Set X first via `forthOuterInterpret("42")` (require `x_is_longint(42)`),
   drive `fnExecute(lbl)`, require no error, `x_is_longint(42)` still, and
   register 05 now holds long-integer 42 (convert register 5 with the same
   `convertLongIntegerRegisterToLongInteger` pattern `x_is_longint` uses on
   REGISTER_X).
2. **The relocated Forth XEQ fallback still resolves.** Program:

   ```c
   uint8_t prog[] = {
     0x01, 0xFD, 0x03, 'F', '2', 'F',                       /* LBL 'F2F' */
     0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH    */
     0x8B, 0x1A, 0xFD, 0x08, ':', ' ', 'W', '7', ' ',       /* : W7 7 ;  */
     '7', ' ', ';',
     0x8B, 0x1A, 0xFD, 0x00,                                /* FORTH«    */
     0x03, 0xFD, 0x02, 'W', '7',                            /* XEQ 'W7'  */
     0x04                                                   /* RTN       */
   };
   ```

   Do **not** drive this fixture with `fnExecute(lbl)`: native
   `executeOneStep` returns `-1` unconditionally for `ITM_XEQ` (its legacy
   "the callee moved `currentStep`" contract), while the synchronous Forth
   fallback correctly leaves `currentStep` on the XEQ instruction. The
   outer loop therefore repeats that instruction forever. Fixing that
   pre-existing control-flow behavior is outside this PURE EXTRACTION.

   Instead, exercise both relevant instructions once through the real
   decoder. Set `programRunStop = PGM_STOPPED`, clear `lastErrorCode`, set
   `dynamicMenuItem = -1`, and call `forthRunGenBump()` to model the
   top-level lifetime. Then set `programRunStop = PGM_RUNNING`, set
   `currentStep = beginOfProgramMemory + 10`, and call
   `executeOneStep(currentStep)` so the source step first-touch pre-scan
   compiles `W7`. If that remains error-free, set
   `currentStep = beginOfProgramMemory + 26` and call
   `executeOneStep(currentStep)` exactly once. Require no error and
   `x_is_longint(7)` — the XEQ-name step went through the relocated
   PARAM_LABEL fallback to the Forth word defined by that fixture's
   pre-scan.

### Existing tests and comments

Every existing test must stay green unchanged — this is the extraction's
real oracle. If any reddens, STOP (rule 6): the move was not verbatim.

### Non-goals / STOP boundaries

- No bounded reader (F2-2), no shared direct-dispatch seam or forth_inner
  changes (F2-3), no behavior change of any kind.
- No edits to `decode.c`, `forth_inner.c`, `forth_compile.c`, upstream
  `src/`.
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue
only when the named subcase goes RED for the named reason:

1. In `param_core.c`'s PARAM_REGISTER arm, comment out the direct-branch
   `reallyRunFunction(op, opParam);` call. Subcase 1 must go RED (register
   05 never written).
2. In the relocated PARAM_LABEL fallback, delete the `FORTH_XEQ_COLON`
   branch. Subcase 2 must go RED (`ERROR_LABEL_NOT_FOUND` instead of X==7).

After all mutations, grep for `MUTATION F2-1` (no match), run the full gate
green again, and record: both PASS lines; both success banners and exit 0;
the `FORTH ARENA` line; `git diff --check`; byte equality between each flat
file and its generated `files/` counterpart (the two new files gain
`files/programming/param_core.*` copies).

### Commit

After the final green gate, `git status --short` may contain only the four
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F2-1 — extract the native parameter core into param_core
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the moved-function inventory (names + old/new locations), the
call-site rewrite count, both PASS lines, each mutation's RED symptom, the
final gate and arena lines, commit hash, and anything surprising.
