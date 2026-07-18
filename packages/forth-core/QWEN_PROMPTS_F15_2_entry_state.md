# Stage F15-2 — derived entry state + power-off round-trip acceptance (bounded implementation prompt)

Origin: DESIGN §8.9 item 2(a-d), against the landed F15-1 tree
(`b773597bd`). Stage ledger: `QWEN_PROMPTS_F15_harness.md`. Authored
2026-07-17 with every anchor grep-verified and the exact fixture byte stream
machine-verified (22 bytes; source payload `"7"` is one byte).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -5` contains a commit whose subject is exactly
   `forth-core: F15-1 — end-to-end run-lifecycle acceptance (§8.9 items 1, 7, 9)`.
3. `grep -n "bool forthEntryStateAtCursor\|bool forthEntryStateAtInsertion"
   packages/forth-core/forth_bridge.c` shows both functions exactly once;
   `forthEntryStateAtInsertion` contains `forthOwningProgramStart`, walks to
   the predecessor of `currentStep`, and ends by deriving RPN/source/marker
   state from that predecessor.
4. `grep -n "forthEntryStateAtInsertion()" packages/forth-core/programming/manage.c`
   shows the E2 printable-key route at the condition containing
   `indexOfItems[func].func == addItemToBuffer`, and that route sets
   `tam.function = ITM_FORTH` then calls `pemAlpha(func)`.
5. `grep -n "/\\*  *542 \\*/" packages/forth-core/items.c` shows the
   `ITM_2` row routed to `addItemToBuffer`; `grep -n "#define ITM_2"
   packages/forth-core/items.h` shows value `542`.
6. `grep -n "void fnGotoDot" packages/forth-core/programming/lblGtoXeq.c`
   shows it calling `goToGlobalStep`; the save and restore halves of
   `packages/forth-core/saveRestoreBackup.c` each contain both
   `"currentStep"`/`"currentStepOffset"` and `"currentLocalStepNumber"`.
7. `grep -n "test_entry_state_derivation\|test_save_restore_roundtrip\|test_accept_run_lifecycle"
   packages/forth-core/test_dict_reloc.c` shows all three tests registered,
   and `grep -n "test_accept_entry_state_roundtrip"
   packages/forth-core/test_dict_reloc.c` returns nothing (this task adds it).

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, identifier, offset, or
fixture byte does not match the tree, STOP and report the mismatch instead of
guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f15-2-todo.md` (outside the repo): one item per file, function,
   acceptance subcase, mutation, final gate, and report. Keep the file updated
   — mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f15-2-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the banners
   and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f15-2-gate.log | head -n 30` or a grep
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
   outside the named mutation. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`packages/forth-core/QWEN_PROMPTS_F15_2_entry_state.md`), the todo file
   `/tmp/forth-f15-2-todo.md`, and `git status --short` / `git diff` are the
   ONLY durable truth about task state. If your context is compacted or
   summarized, or you are unsure what you have already done: STOP the current
   step, re-read this packet file and the todo file, run `git status --short`
   and `git diff --stat`, and check for an unrestored `MUTATION APPLIED` marker
   before doing anything else. Never reconstruct packet text, anchors, or code
   blocks from memory — re-read them from this file every time you need them.

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

## F15-2 — PEM derives keypad state from the landed step, including after power-off

### Authority carried by this packet

This task adds the end-to-end drive for DESIGN §8.9 item 2; production is
already implemented and unit-pinned, so there are no production changes:

- Keypad state is never persisted. `forthEntryStateAtInsertion()` derives it
  from program bytes at the step immediately before the insertion point.
- In PEM with no capture already open, a printable key whose item function is
  `addItemToBuffer` reaches E2. Inside a Forth region E2 sets
  `tam.function = ITM_FORTH` and calls `pemAlpha(func)`, so digit `2` opens
  Forth capture and types text. Outside a Forth region it follows normal RPN
  number entry.
- Real cursor motion for this acceptance is `fnGotoDot(globalStepNumber)`.
  Do not assign `currentStep` to manufacture a landed state.
- Simulator power-off/on is modeled by `saveCalc()` then `restoreCalc()` with
  `loadTestPrograms = false` around the restore, exactly as
  `test_save_restore_roundtrip` does. The backup carries program RAM,
  `currentStep` block+offset, `currentLocalStepNumber`, and the PEM globals;
  derived Forth entry state is intentionally absent.
- Exact fixture facts: `LBL 'E2A'` =
  `0x01 0xFD 0x03 'E' '2' 'A'`; `ITM_sin` = `0x4C`; marker =
  `0x8B 0x1A 0xFD 0x00`; source `"7"` =
  `0x8B 0x1A 0xFD 0x01 '7'`; `RTN` = `0x04`. The complete array below is
  22 bytes. Its global step/offset pairs are: RPN `(2, +6)`, opening marker
  `(3, +7)`, source `(4, +11)`, closing marker `(5, +16)`, trailing RPN
  `(6, +20)`, RTN `(7, +21)`. These literals are normative; transcribe them
  exactly.

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

Read-only (and mutation-only, always restored):

- `packages/forth-core/forth_bridge.c`

### Targeted reads

1. In `test_dict_reloc.c`, grep
   `test_entry_state_derivation\|test_e2_continuation_after_enter\|test_e2_not_inside_rpn_gap\|test_forth_capture_survives_keystroke\|test_save_restore_roundtrip\|test_accept_run_lifecycle\|writeTestProgram\|cleanupTestProgram`.
   Read only those complete functions and the registration lines around
   `test_accept_run_lifecycle`. The E2 tests are the state-save/restore and
   `aimBuffer` assertion models; the save/restore test supplies the exact
   `loadTestPrograms` guard.
2. In `items.c`, grep `void runFunction` and read only from that declaration
   through its `calcMode == CM_PEM` / `addStepInProgram(func)` return. This is
   the real digit-key dispatch used here. Also confirm the item-542 row maps
   `ITM_2` to `addItemToBuffer`.
3. In `programming/manage.c`, grep the E2 condition containing
   `forthEntryStateAtInsertion()` and read only 12 lines around it. In
   `programming/lblGtoXeq.c`, read only `fnGotoDot` and its immediate wrapper.
4. In `saveRestoreBackup.c`, grep `"currentStep"`, `"currentStepOffset"`,
   `"currentLocalStepNumber"`, and read only the save/restore lines for those
   fields. No edits to any file in targeted reads 2-4.

### Change 1 — the focused acceptance test

Add `static int test_accept_entry_state_roundtrip(void)` immediately after
`test_accept_run_lifecycle`, and register it immediately after that test with
the same `printf("  [DEBUG] running ...")`, `fail |= ...();`, and
`forthDictClear();` pattern.

Use this exact fixture afresh for every independent drive; never reuse program
bytes changed by an earlier digit entry:

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'E', '2', 'A',                       /* 1: LBL 'E2A'       (+0)  */
    0x4C,                                                  /* 2: ITM_sin, RPN    (+6)  */
    0x8B, 0x1A, 0xFD, 0x00,                               /* 3: opening »FORTH  (+7)  */
    0x8B, 0x1A, 0xFD, 0x01, '7',                          /* 4: source "7"      (+11) */
    0x8B, 0x1A, 0xFD, 0x00,                               /* 5: closing FORTH«  (+16) */
    0x4C,                                                  /* 6: ITM_sin, RPN    (+20) */
    0x04                                                   /* 7: RTN             (+21) */
  };
```

At function entry save every global the existing E2 tests save, including
`currentStep`, `pemCursorIsZerothStep`, `currentLocalStepNumber`,
`currentProgramNumber`, `FLAG_ALPHA`, `calcMode`, `catalog`, `tam.function`,
`tam.mode`, `programRunStop`, `dynamicMenuItem`, the current softmenu, and the
whole `aimBuffer`. Restore them on every exit. Between subcases, close any
open Forth capture with `pemAlpha(ITM_ENTER)` or close RPN number entry with
`pemCloseNumberInput()` as appropriate, then call `cleanupTestProgram()` and
restore the saved transient UI/input state before writing the next fresh
fixture. Do not leave a placeholder, number entry, menu, or backup-derived
state live for the next suite test.

The common pre-drive setup after each `writeTestProgram` is:

- `programRunStop = PGM_STOPPED`, `calcMode = CM_PEM`,
  `catalog = CATALOG_NONE`, `tam.mode = 0`, `tam.function = 0`,
  `aimBuffer[0] = 0`, `dynamicMenuItem = -1`,
  `pemCursorIsZerothStep = false`, `lastErrorCode = ERROR_NONE`, and
  `clearSystemFlag(FLAG_ALPHA)`;
- call `fnGotoDot` with the exact step number named below and assert both
  `currentLocalStepNumber` and the exact `currentStep` offset before pressing
  the digit;
- drive the digit through `runFunction(ITM_2)`, not by calling
  `forthEntryStateAtInsertion`, `addStepInProgram`, or `pemAlpha` directly.

Report four independently accumulated subcases, one PASS line each. Apart
from an unrecoverable `writeTestProgram` failure, do not return early: a
mutation must be able to show every affected subcase before cleanup.

**Subcase 1 — §8.9 item 2(a): RPN step keeps RPN number entry.**

Fresh fixture; `fnGotoDot(2)`. Require step 2 and
`currentStep == beginOfProgramMemory + 6`. Drive `runFunction(ITM_2)`.
Require `lastErrorCode == ERROR_NONE`, `FLAG_ALPHA` clear,
`tam.function != ITM_FORTH`, and RPN number entry bytes
`aimBuffer[0] == '+'`, `aimBuffer[1] == '2'`. PASS text must contain:

`[1] PASS: RPN landing routes digit 2 to number entry`

**Subcase 2 — §8.9 item 2(b): source step opens Forth text capture.**

Fresh fixture; `fnGotoDot(4)`. Require step 4 and
`currentStep == beginOfProgramMemory + 11`. Drive `runFunction(ITM_2)`.
Require `lastErrorCode == ERROR_NONE`, `FLAG_ALPHA` set,
`tam.function == ITM_FORTH`, and exact text buffer
`aimBuffer[0] == '2' && aimBuffer[1] == 0`. PASS text must contain:

`[2] PASS: source landing routes digit 2 to Forth capture`

**Subcase 3 — §8.9 item 2(c): opening and closing markers are symmetric.**

Opening half: fresh fixture; `fnGotoDot(3)`. Require step 3 and
`currentStep == beginOfProgramMemory + 7`; drive the digit and require the
same Forth-capture state and exact `"2"` buffer as subcase 2. Close and clean
that drive completely.

Closing half: another fresh fixture; `fnGotoDot(5)`. Require step 5 and
`currentStep == beginOfProgramMemory + 16`; drive the digit and require the
same RPN number-entry state and `aimBuffer[0] == '+'`,
`aimBuffer[1] == '2'` as subcase 1. Only after both halves pass, print:

`[3] PASS: opening marker captures and closing marker restores RPN`

**Subcase 4 — §8.9 item 2(d): save/restore re-derives at the same source step.**

Fresh fixture and common setup; `fnGotoDot(4)`. Require step 4 and
`currentStep == beginOfProgramMemory + 11`, with no capture open. Call
`saveCalc()` at this point. Prove the test is not observing untouched globals:
call `fnGotoDot(2)`, require the cursor moved to step 2 / offset `+6`, clear
`aimBuffer`, and keep `FLAG_ALPHA` clear and `tam.function = 0`.

Then restore exactly as the existing round-trip test does:

```c
  {
    bool_t savedLoad = loadTestPrograms;
    loadTestPrograms = false;
    restoreCalc();
    loadTestPrograms = savedLoad;
  }
```

Require the restored cursor lands at step 4 and
`currentStep == beginOfProgramMemory + 11`, with
`pemCursorIsZerothStep == false`. Now drive `runFunction(ITM_2)` and require
the same no-error, `FLAG_ALPHA`, `tam.function`, and exact `"2"` buffer
assertions as subcase 2. No static or restored Forth-state flag is consulted;
production must re-derive from the restored program bytes. PASS text:

`[4] PASS: power-off round-trip re-derives Forth capture at source step`

### Existing tests and comments

All existing tests stay green unchanged — this task adds one test and touches
no production code. If any existing test reddens, STOP (rule 6).

### Non-goals / STOP boundaries

- No production edits to `forth_bridge.c`, `manage.c`, `items.c`,
  `lblGtoXeq.c`, or `saveRestoreBackup.c` outside the one temporary mutation,
  which is in `forth_bridge.c` and must be restored.
- No direct assignments to `currentStep` for an acceptance drive; only restore
  saved suite state during cleanup may assign it.
- No display-parity, glyph, literal-type, or XEQ-name work (F15-3..F15-5 own
  those §8.9 items).
- No DESIGN or history edits.

### Gate and required mutation

Run the full gate green first. Then apply the single mutation below, run the
full sanctioned gate, inspect the focused bounded output, and manually restore
the exact hunk.

In `forthEntryStateAtInsertion`, replace only its final derivation:

```c
  uint8_t len;
  if (!forthStepPayload(prev, &len)) return false;  /* RPN step: RPN */
  if (len > 0) return true;                         /* source step: Forth */
  return forthMarkerTurnsOn(prev);                  /* marker: its direction */
```

with this deliberately wrong persisted-toggle surrogate:

```c
  static bool_t staleEntryState = false;            /* MUTATION F15-2 */
  uint8_t len;
  if (!forthStepPayload(prev, &len)) return staleEntryState;
  if (len == 0) staleEntryState = !staleEntryState;
  return staleEntryState;
```

This is the §8.9 item-2 escaping mutation: a static bool is changed by marker
toggles instead of state being derived at every landing. Subcase 2 must go RED
because landing directly on a source step did not first toggle the flag;
subcase 4 must also go RED after the save/restore landing for the same reason.
Record the actual focused RED lines; subcase 1 and both marker halves should
still demonstrate why this mutation is not equivalent to a blanket
`return false`.

After the mutation run, restore those four original lines manually. Grep for
`MUTATION F15-2` (there must be no match) and require `git diff` restricted to
`packages/forth-core/forth_bridge.c` to be empty. Run the full gate green again
and record:

- all four PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=...` (the §5.4 acceptance-run arena
  report is mandatory for every F1.5 packet);
- `git diff --check`;
- byte equality between `test_dict_reloc.c` and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only
`packages/forth-core/test_dict_reloc.c`, its generated `files/` counterpart,
and `packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F15-2 — end-to-end entry-state + power-off acceptance (§8.9 item 2)
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the new test's four PASS lines, the mutation's RED symptoms, the final
gate and arena lines, commit hash, and anything surprising — explicitly
including any assertion, offset, cursor step, save/restore field, or entry
route in this packet that did not match observed behavior (that is architect
feedback, not something to adapt around).
