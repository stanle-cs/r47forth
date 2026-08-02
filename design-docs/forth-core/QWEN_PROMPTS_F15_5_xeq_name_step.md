# Stage F15-5 — PEM XEQ-by-name records a name step (bounded implementation prompt)

Origin: DESIGN §8.9 item 10 (mechanism verified against §4.2's landed
name-redirect bridge), stage ledger `QWEN_PROMPTS_F15_harness.md`. Authored
2026-07-17 against the post-F15-4 target tree per the owner's same-day
pacing instruction (author ahead, gate-locked); every anchor below was
grep-verified on the F15-3 tree, and F15-4 (forth_prims.c + test file only)
does not touch any of them — the gate re-verifies regardless.

> **AMENDMENT (2026-07-18, executed as `546aa8b6c` — the landed test and
> the run report are normative).** The mutation's stated subcase-2
> consequence was falsified in execution: `insertStepInProgram`'s own
> `ITM_FCALL` arm (programming/manage.c ~1642) is a SECOND
> name-faithfulness guard — it resolves the index back to the name
> (`forthDictNameByIndex`) and records an `ITM_FORTH` source step, so
> `0x8B 0x1B` never reaches program memory even under the re-route. The
> mutation IS detected — subcase 1 goes RED (the XEQ name step
> `0x03 0xFD 0x02 'S' 'Q'` is absent) — and subcase 2's probe is
> defense-in-depth, declared redundant on every production path. DESIGN
> §8.9 item 10 carries the reconciled mutation text.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -6` contains a commit whose subject is
   `forth-core: F15-4 — glyph operators + literal type parity (§8.9 items 5, 6)`.
3. `grep -n "forth-core H-hook" packages/forth-core/ui/tam.c` matches, and
   within 12 lines below it there are BOTH branches:
   `insertUserItemInProgram(tam.function, buffer);` under `calcMode == CM_PEM`,
   and `reallyRunFunction(ITM_FCALL, widx);` on the else path.
4. `grep -n "void insertUserItemInProgram" packages/forth-core/programming/manage.c`
   matches with signature `(int16_t func, char *funcParam)`.
5. `grep -n "#define ITM_XEQ \|#define ITM_FCALL\|#define ITM_Q \|#define ITM_S "
   packages/forth-core/items.h` shows values 3, 2843, 566, 568.
6. `grep -n "test_tam_colon_never_falls_to_forth\|test_accept_entry_state_roundtrip"
   packages/forth-core/test_dict_reloc.c` shows both registered (the TAM
   drive model and the PEM fixture model), and
   `grep -n "test_accept_xeq_name_step" packages/forth-core/test_dict_reloc.c`
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
   `/tmp/forth-f15-5-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f15-5-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f15-5-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, `tam.c`, `manage.c`, or `test_dict_reloc.c` in full. Grep
   the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named hook. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`design-docs/forth-core/QWEN_PROMPTS_F15_5_xeq_name_step.md`), the todo file
   `/tmp/forth-f15-5-todo.md`, and `git status --short` / `git diff` are the
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

## F15-5 — In PEM, XEQ + alpha name of a Forth word records the NAME, never an index

### Authority carried by this packet

§8.9 item 10, mechanism as landed (§4.2 name-redirect bridge):

- In TAM `XEQ` alpha entry, after a global-label miss and a
  function-catalog miss, the forth-core H-hook (`ui/tam.c`) resolves the
  buffer against the Forth dictionary. Interactively it dispatches
  `reallyRunFunction(ITM_FCALL, widx)`; **in PEM (`calcMode == CM_PEM`) it
  records the step via `insertUserItemInProgram(tam.function, buffer)`** —
  the recorded bytes carry the NAME STRING, re-resolved at run time.
  No `ITM_FCALL` opcode and no dictionary index ever reach program memory;
  a recorded index would go stale after `CLEAR FORTH` + redefinition in a
  different order.
- Recorded-step byte shape (verified): an XEQ name step is
  `0x03 0xFD <len> <glyphs>` (ITM_XEQ = 3, string marker `0xFD`), exactly
  like `LBL`'s `0x01 0xFD <len> <name>`. The `ITM_FCALL` opcode would
  encode as the two-byte pair `0x8B 0x1B` (item 2843 = 0x0B1B).
- The public TAM drive chain (already exercised green by
  `test_tam_colon_never_falls_to_forth`): `tamEnterMode(ITM_XEQ)` →
  `tamProcessInput(ITM_alpha)` → `runFunction(ITM_S)` / `runFunction(ITM_Q)`
  (letter items append to `aimBuffer`) → `tamProcessInput(ITM_ENTER)`
  commits. `"SQ"` is 2 bytes (machine-verified); its recorded payload is
  `0x03 0xFD 0x02 'S' 'Q'`.
- PEM fixture state (mirror the landed `test_accept_entry_state_roundtrip`
  setup block EXACTLY): `programRunStop = PGM_STOPPED; calcMode = CM_PEM;
  catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; aimBuffer[0] = 0;
  dynamicMenuItem = -1; pemCursorIsZerothStep = false; lastErrorCode =
  ERROR_NONE; clearSystemFlag(FLAG_ALPHA);` then position with
  `fnGotoDot(n)` and verify `currentLocalStepNumber == n`.

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

Read-only (and mutation-only, always restored):

- `packages/forth-core/ui/tam.c`

### Targeted reads

1. In `test_dict_reloc.c`, grep
   `test_tam_colon_never_falls_to_forth\|test_accept_entry_state_roundtrip\|writeTestProgram\|cleanupTestProgram`
   and read only: the whole of `test_tam_colon_never_falls_to_forth` (the
   TAM drive + state save/restore model: `savedCalcMode`,
   `savedLastError`, `savedRunStop`, `savedAimBuffer`, `savedTam`), the
   PEM setup block of `test_accept_entry_state_roundtrip` (first ~40
   lines), the helpers, and the registration lines after the F15-4 test.
2. In `ui/tam.c`, grep `forth-core H-hook` and read ≤15 lines around it
   only (the mutation anchor).

### Change 1 — the focused acceptance test

Add `static int test_accept_xeq_name_step(void)`, registered immediately
after the F15-4 test's registration lines (same three-line pattern as its
neighbors). Save and restore ALL of: `calcMode`, `lastErrorCode`,
`programRunStop`, `aimBuffer` (full copy), `tam` (struct copy) — exactly as
`test_tam_colon_never_falls_to_forth` does — plus cleanup
(`forthDictClear()` + `cleanupTestProgram()`) on every path.

Setup:

1. Fixture program (label then return; machine-verified bytes):

   ```c
   uint8_t prog[] = {
     0x01, 0xFD, 0x03, 'F', '5', 'E',   /* LBL 'F5E' */
     0x04                               /* RTN       */
   };
   ```

   `writeTestProgram(prog, sizeof(prog))`, FAIL-return if it fails.
2. Define the Forth word interactively:
   `forthOuterInterpret(": SQ DUP * ;")` — require no error and
   `forthFindColon("SQ", &idx)` true.
3. Enter the PEM fixture state (the exact block quoted in the authority
   section), then `fnGotoDot(1);` and require
   `currentLocalStepNumber == 1` (cursor on the LBL step; the recorded
   step will be inserted after it).

Drive (the real public TAM chain):

```c
  tamEnterMode(ITM_XEQ);
  tamProcessInput(ITM_alpha);
  runFunction(ITM_S);
  runFunction(ITM_Q);
  /* require strcmp(aimBuffer, "SQ") == 0 before commit; FAIL otherwise */
  tamProcessInput(ITM_ENTER);
```

Require `lastErrorCode == ERROR_NONE` after the commit. Then two
independently reported subcases, one PASS line each:

1. **The name step was recorded.** Scan program memory
   `[beginOfProgramMemory, firstFreeProgramByte)` with `memcmp` for the
   5-byte sequence `{0x03, 0xFD, 0x02, 'S', 'Q'}`. Require exactly one
   match. PASS line names the recorded name-string XEQ step.
2. **No ITM_FCALL opcode, no index.** Scan the same range for the 2-byte
   pair `{0x8B, 0x1B}`. Require zero matches. PASS line names the absent
   FCALL opcode.

Cleanup as saved (restore `tam`, `aimBuffer`, `calcMode`, `programRunStop`,
`lastErrorCode`, then `forthDictClear()` + `cleanupTestProgram()`).

### Existing tests and comments

All existing tests stay green unchanged — this task adds one test and
touches no production code. If any existing test reddens, STOP (rule 6).

### Non-goals / STOP boundaries

- No edits to `ui/tam.c` outside the single mutation (restored).
- No interactive-dispatch leg (already pinned by
  `test_tam_colon_never_falls_to_forth`), no run-time re-resolution work,
  no capture/F6 work.
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run the mutation using the full
sanctioned gate, manually restore the hunk afterward, and continue only
when the named subcases go RED for the named reason:

1. In the `ui/tam.c` H-hook PEM branch, replace
   `insertUserItemInProgram(tam.function, buffer);` with
   `tam.value = widx; insertStepInProgram(ITM_FCALL);` (the §8.9 item-10
   index-recording defect; `insertStepInProgram` is already called
   elsewhere in tam.c, so this compiles). Subcase 1 must go RED (no name
   step) AND subcase 2 must go RED (the `0x8B 0x1B` pair appears in
   program memory). Record both symptoms, then restore the hunk.

After the mutation, grep for `MUTATION F15-5` (there must be no match) and
require `git diff` restricted to `packages/forth-core/ui/` to be empty.
Run the full gate green again and record:

- both PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=...` (§5.4 arena rule);
- `git diff --check`;
- byte equality between `test_dict_reloc.c` and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only
`packages/forth-core/test_dict_reloc.c`, its generated `files/`
counterpart, and `packages/forth-core/.refresh-manifest.json`. Stage those
exact paths only and commit:

```text
forth-core: F15-5 — PEM XEQ-by-name records a name step (§8.9 item 10)
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the two PASS lines, the mutation's dual RED symptom, the final gate
and arena lines, commit hash, and anything surprising — explicitly
including any TAM/PEM state this packet's fixture block did not account
for (that is architect feedback, not something to adapt around).
