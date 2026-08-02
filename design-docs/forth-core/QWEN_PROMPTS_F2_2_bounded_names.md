# Stage F2-2 — bounded string-name reader in the parameter core (bounded implementation prompt)

Origin: DESIGN §10.2 ("a generalized string-name reader takes explicit
start/end bounds") via the stage trace in `QWEN_PROMPTS_F2_core.md`.
Authored 2026-07-17 against the post-F2-1 target tree; the gate below fails
closed on any drift.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -6` contains a commit whose subject is
   `forth-core: F2-1 — extract the native parameter core into param_core`.
3. `grep -n "void paramCoreExecuteOp" packages/forth-core/programming/param_core.c`
   matches, and `grep -c "_executeOp"
   packages/forth-core/programming/lblGtoXeq.c` prints `0`.
4. `grep -n "getStringLabelOrVariableName" packages/forth-core/programming/param_core.c`
   shows the unbounded calls this task replaces (PARAM_LABEL arm, the
   indirect-variable helper, and any further name arms — count and record
   them), and `grep -n "Bounded" packages/forth-core/programming/param_core.c`
   returns nothing.
5. `grep -n "void getStringLabelOrVariableName" packages/forth-core/programming/decode.c`
   matches (the contract source to copy).
6. `grep -n "test_param_core_bounded_names" packages/forth-core/test_dict_reloc.c`
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
   `/tmp/forth-f2-2-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f2-2-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f2-2-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, `decode.c`, or `test_dict_reloc.c` in full. Grep the
   named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named hook. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`design-docs/forth-core/QWEN_PROMPTS_F2_2_bounded_names.md`), the todo file
   `/tmp/forth-f2-2-todo.md`, and `git status --short` / `git diff` are the
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

## F2-2 — Name reads in the execution core take an explicit end bound

### Authority carried by this packet

Decided architecture (stage trace; no open choices):

- `param_core.c` gains ONE static bounded reader:

  ```c
  /* F2-2 (§10.2): bounded variant of decode.c's getStringLabelOrVariableName.
   * end is EXCLUSIVE. A name that would read past end is clamped to the
   * available bytes; the caller's normal not-found path then reports it.
   * The unbounded decode.c reader remains for display paths only. */
  static void paramCoreReadName(const uint8_t *stringAddress, const uint8_t *end);
  ```

  Its body copies the decode.c reader's contract exactly (same output
  buffer `tmpStringLabelOrVariableName`, same length-prefix format: first
  byte = glyph-string byte length, then the bytes), with the single
  addition: the copied byte count is `min(lengthByte, end - (stringAddress
  + 1))` (and zero if `stringAddress + 1 > end`), always
  NUL-terminating the output buffer.
- Every `getStringLabelOrVariableName(` call inside `param_core.c` becomes
  `paramCoreReadName(` with `firstFreeProgramByte` as the end bound —
  program-memory execution paths can never legally read past the last
  program byte. Call sites outside `param_core.c` (decode.c display paths,
  tam paths) are OUT OF SCOPE and must not be touched.
- Behavior on all well-formed programs is unchanged (the clamp can only
  engage on malformed bytes) — the whole existing suite is the
  no-regression oracle.
- `writeTestProgram` writes a `0xFF 0xFF` `.END.` sentinel immediately at
  `firstFreeProgramByte`. The malformed fixture therefore has a
  deterministic differential oracle: the bounded reader sees exactly
  `"W7"`, while the unbounded 127-byte copy necessarily includes the
  non-NUL sentinel and cannot resolve as `"W7"`. No sanitizer assumption
  is needed.

### Files

Modify only:

- `packages/forth-core/programming/param_core.c`
- `packages/forth-core/test_dict_reloc.c`

Read-only:

- `packages/forth-core/programming/decode.c` — the reader being mirrored;
  do not edit.

### Targeted reads

1. In `programming/decode.c`, grep `void getStringLabelOrVariableName` and
   read that function only (it is short); transcribe its copy loop
   faithfully before adding the clamp.
2. In `programming/param_core.c`, grep `getStringLabelOrVariableName` and
   read 6 lines around every hit (the call sites being switched).
3. In `test_dict_reloc.c`, grep
   `test_param_core_extraction\|writeTestProgram\|firstFreeProgramByte` and
   read the F2-1 test (fixture model) and the helpers.

### Change 1 — the reader and the switch

Add `paramCoreReadName` as specified; switch every in-file call site to it
with end = `firstFreeProgramByte`. After the change,
`grep -c "getStringLabelOrVariableName" packages/forth-core/programming/param_core.c`
must print `0`.

### Change 2 — focused test

Add `test_param_core_bounded_names`, registered after
`test_param_core_extraction`, same fixture discipline. Two subcases:

1. **Well-formed name step unchanged.** Reuse the F2-1 subcase-2 program
   verbatim (LBL 'F2F' / »FORTH / `: W7 7 ;` / FORTH« / XEQ 'W7' / RTN,
   exact bytes in that packet's Change 3). Use the corrected F2-1
   one-step drive: with `programRunStop = PGM_STOPPED`, clear
   `lastErrorCode`, set `dynamicMenuItem = -1`, call `forthRunGenBump()`,
   then set `programRunStop = PGM_RUNNING`. Set
   `currentStep = beginOfProgramMemory + 10` and call
   `executeOneStep(currentStep)` to run the source-step pre-scan; if still
   error-free, set `currentStep = beginOfProgramMemory + 26` and call
   `executeOneStep(currentStep)` exactly once. Require no error and
   `x_is_longint(7)` — the bounded reader is transparent on valid input.
   Do not use `fnExecute`: as recorded in the corrected F2-1 packet, the
   legacy `ITM_XEQ` control return would repeat a synchronous Forth
   fallback forever, and changing that behavior is outside F2-2.
2. **Lying length byte at end of program memory — differential oracle
   (amended 2026-07-18: the original "expect LABEL_NOT_FOUND" oracle could
   not distinguish bounded from unbounded reads, a mutation-escape risk of
   the F15-4 class).** The program DEFINES `W7` first, then ends with a
   malformed XEQ-name step claiming 127 name bytes with only 2 present:

   ```c
   uint8_t prog[] = {
     0x01, 0xFD, 0x03, 'F', '2', 'G',                       /* LBL 'F2G' */
     0x8B, 0x1A, 0xFD, 0x00,                                /* »FORTH    */
     0x8B, 0x1A, 0xFD, 0x08, ':', ' ', 'W', '7', ' ',       /* : W7 7 ;  */
     '7', ' ', ';',
     0x8B, 0x1A, 0xFD, 0x00,                                /* FORTH«    */
     0x03, 0xFD, 0x7F, 'W', '7'         /* XEQ, len=127, only 2 bytes    */
   };
   ```

   (Deliberately no RTN: the malformed step must be the final program
   bytes so the lie crosses `firstFreeProgramByte`.) Use the same one-step
   drive as subcase 1: source at `beginOfProgramMemory + 10`, then the
   malformed XEQ at `beginOfProgramMemory + 26`, with the XEQ executed
   exactly once. Require `lastErrorCode == ERROR_NONE` and
   `x_is_longint(7)` — the bounded reader clamps the name to the 2
   available bytes, reads `"W7"`, and the relocated Forth fallback
   resolves and runs it (memory-safety bound, not validation — commit-time
   validation is F5's). The unbounded reader's 127-byte copy necessarily
   includes `writeTestProgram`'s non-NUL `0xFF 0xFF` sentinel, so it cannot
   produce `"W7"`; the mutation below is guaranteed RED without relying
   on allocator contents or sanitizer instrumentation.

### Existing tests and comments

All existing tests stay green unchanged. If any reddens, STOP (rule 6).

### Non-goals / STOP boundaries

- No changes to decode.c, tam.c, forth_inner.c, or any display path.
- No shared dispatch seam (F2-3), no parity sweep (F2-4).
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run the mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue
only when the named subcase goes RED for the named reason:

1. In the PARAM_LABEL arm only, revert the call to the unbounded
   `getStringLabelOrVariableName(paramAddress)`. Subcase 2 must go RED:
   the 127-byte copy crosses the `0xFF 0xFF` sentinel and cannot yield
   `"W7"`, so the success assertions (no error + X==7) fail. Record the
   resulting error and X symptom.

After the mutation, grep for `MUTATION F2-2` (no match), run the full gate
green again, and record: both PASS lines; both success banners and exit 0;
the `FORTH ARENA` line; `git diff --check`; byte equality between each
flat file and its generated `files/` counterpart.

### Commit

After the final green gate, `git status --short` may contain only the two
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F2-2 — execution-core name reads take an explicit end bound
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the call sites switched (count + arms), both PASS lines, the
mutation's RED symptom verbatim, the final gate and arena lines, commit
hash, and anything surprising.
