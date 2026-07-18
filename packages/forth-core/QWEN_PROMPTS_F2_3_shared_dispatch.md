# Stage F2-3 — shared direct dispatch; FTOK_C47 re-routed (bounded implementation prompt)

Origin: DESIGN §10.2 via the stage trace in `QWEN_PROMPTS_F2_core.md` — the
Forth runtime's private parameter checks/dispatch are replaced by the same
semantic tail native execution uses, "ending at `reallyRunFunction()`".
Authored 2026-07-17 against the post-F2-2 target tree; the gate below fails
closed on any drift.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -8` contains commits whose subjects are
   `forth-core: F2-1 — extract the native parameter core into param_core`
   and
   `forth-core: F2-2 — execution-core name reads take an explicit end bound`.
3. `grep -n "paramCoreValidateDirect\|paramCoreDispatchDirect"
   packages/forth-core/programming/param_core.h` returns nothing (this
   task introduces both).
4. In `packages/forth-core/forth_inner.c`,
   `grep -n "case FTOK_C47" packages/forth-core/forth_inner.c` matches, and
   the arm still contains its own PTP switch with a
   `default:` → `ERROR_OPERATION_UNDEFINED` branch (the private decoder
   this task replaces).
5. `grep -n "#define ITM_SDL " packages/forth-core/items.h` shows 423.
6. `grep -n "test_c47_param_shared_dispatch" packages/forth-core/test_dict_reloc.c`
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
   `/tmp/forth-f2-3-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f2-3-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f2-3-gate.log | head -n 30` or a grep
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
   (`packages/forth-core/QWEN_PROMPTS_F2_3_shared_dispatch.md`), the todo
   file `/tmp/forth-f2-3-todo.md`, and `git status --short` / `git diff`
   are the ONLY durable truth about task state. If your context is
   compacted or summarized, or you are unsure what you have already done:
   STOP the current step, re-read this packet file and the todo file, run
   `git status --short` and `git diff --stat`, and check for an unrestored
   `MUTATION APPLIED` marker before doing anything else. Never reconstruct
   packet text, anchors, or code blocks from memory — re-read them from
   this file every time you need them.

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

## F2-3 — One semantic tail for direct parameters, native and Forth

### Authority carried by this packet

Decided architecture (stage trace; no open choices):

- `param_core.h`/`.c` gain the shared direct-parameter seam:

  ```c
  /* F2-3 (§10.2): the semantic tail for DIRECT (non-indirect, non-name)
   * parameters, shared by native _executeOp arms and Forth's FTOK_C47.
   * Validate mirrors the traced native range checks EXACTLY, including
   * their traced silence: an out-of-range direct parameter is a no-op
   * that sets no error (native parity), so validate returning false
   * means "do nothing", not "raise". */
  bool paramCoreValidateDirect(uint16_t op, uint16_t ptpClass, uint16_t value);
  void paramCoreDispatchDirect(uint16_t op, uint16_t value);
  ```

  - `paramCoreValidateDirect`: `PTP_NONE` → always true;
    `PTP_NUMBER_8` (and `PTP_KEYG_KEYX`, which native maps to the same
    arm) → `value <= (indexOfItems[op].tamMinMax & TAM_MAX_MASK)`;
    `PTP_NUMBER_16` → always true for `isFunctionOldParam16(op)` items
    (native applies no range check there) and
    `value <= (indexOfItems[op].tamMinMax & TAM_MAX_MASK)` otherwise —
    IF the targeted read of the native PARAM_NUMBER_16 arm shows different
    check logic, STOP and report; the native arm is the truth.
    Any other ptpClass → false (callers own their own rejection).
  - `paramCoreDispatchDirect`: `reallyRunFunction((int16_t)op, value);` —
    nothing else.
- `paramCoreExecuteOp`'s direct branches for PARAM_NUMBER_8 and
  PARAM_NUMBER_16 are rewritten to
  `if (paramCoreValidateDirect(...)) { paramCoreDispatchDirect(...); } else { <the existing sprintf-only arm> }`
  — observable behavior identical (the sprintf text stays as is).
  PTP_NONE keeps its native path (executeOneStep calls `runFunction`
  directly; do not touch it).
- `forth_inner.c`'s `FTOK_C47` arm KEEPS its byte decode (`boundedRead`,
  itemId bounds check, cell reads — all unchanged) and REPLACES its
  dispatch tail: after decoding `param`, the NONE case dispatches via
  `paramCoreDispatchDirect(itemId, NOPARAM)`-equivalent (see below), and
  the NUMBER_8/NUMBER_16 cases become

  ```c
  if (paramCoreValidateDirect(itemId, ptpClass, param)) {
    paramCoreDispatchDirect(itemId, param);
  }
  /* else: native-parity silence — no error, no dispatch */
  ```

  For PTP_NONE, dispatch with `NOPARAM` as the value (matching the
  pre-F2-3 code's call with no parameter — read the arm's existing
  dispatch lines and preserve their exact `reallyRunFunction` argument
  shape through the new seam; if the existing arm passes anything other
  than NOPARAM for the no-param case, STOP and report).
  The arm's `default → ERROR_OPERATION_UNDEFINED` branch and every
  `boundedRead`/`INNER_LEAVE` line stay byte-identical.
- Include `programming/param_core.h` from `forth_inner.c` the same way
  other cross-directory package includes are done (grep an existing
  `#include` in `forth_inner.c` and mirror the style).
- This task deliberately CHANGES one Forth behavior: an out-of-range
  direct parameter in a hand-assembled `FTOK_C47` call previously reached
  `reallyRunFunction` unchecked; now it is silently skipped — exactly what
  the native engine does with the same bytes. That is the drift being
  closed (§10.2). If an existing test pins the old unchecked behavior,
  STOP and report (rule 6) — architect decision, not a test edit.

### Files

Modify only:

- `packages/forth-core/programming/param_core.h`
- `packages/forth-core/programming/param_core.c`
- `packages/forth-core/forth_inner.c`
- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. In `programming/param_core.c`, grep
   `PARAM_NUMBER_8\|PARAM_NUMBER_16\|isFunctionOldParam16\|TAM_MAX_MASK`
   and read those arms in full (the range logic being factored — the
   native truth).
2. In `forth_inner.c`, grep `case FTOK_C47` and read the whole arm (~60
   lines): the byte decode to keep, the dispatch tail to replace.
3. In `test_dict_reloc.c`, grep
   `begin_word\|end_word\|T_EXIT\|ITM_FCALL, PTP_NUMBER_16\|test_param_core_bounded_names`
   and read the hand-assembly helpers, the existing hand-assembled
   C47+param test (the fixture model), and the newest registration lines.

### Change 1 — the seam (param_core.h/.c)

Add the two functions exactly as specified, implemented from the arms read
in targeted read 1. Rewrite the two direct branches of
`paramCoreExecuteOp` onto them (identical observable behavior — the
existing suite is the oracle).

### Change 2 — the re-route (forth_inner.c)

As specified in the authority section. After the change,
`grep -n "tamMinMax" packages/forth-core/forth_inner.c` must return
nothing (no private range logic remains) and the arm contains exactly one
`paramCoreDispatchDirect` call per decoded-parameter path.

### Change 3 — focused parity test

Add `test_c47_param_shared_dispatch`, registered after
`test_param_core_bounded_names`. Fixture discipline as usual; hand-build
Forth words with `begin_word`/`end_word` (4-glyph names, the established
convention). Self-verifying configuration guard first: FAIL (do not skip)
if `(indexOfItems[ITM_SDL].status & PTP_STATUS) != PTP_NUMBER_8`, printing
the actual class — the fixture item must be re-chosen by the architect if
this ever moves. Three subcases:

1. **In-range NUMBER_8 parity.** Native: program
   `LBL 'F2H'` + SDL 03 step (`0x81, 0xA7, 0x03` — ITM_SDL 423 = 0x1A7,
   two-byte opcode + param) + RTN; set X to long-integer 1 first; drive
   `fnExecute`; record outcome A (X value + `lastErrorCode`). Forth:
   fresh X = 1, hand-build `SDL3` emitting
   `FTOK_C47`, cell `423`, cell `3`, then EXIT; execute it via
   `forthOuterInterpret("SDL3")`; record outcome B. Require A == B on both
   X (compare via the long-integer conversion pattern) and error code —
   and require the shared value (shift-left-3 of 1 → 1000) explicitly.
2. **Out-of-range NUMBER_8 parity (the closed drift).** Same pair with
   parameter `(indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK) + 1`
   (compute in-test; emit that cell value on the Forth side and patch the
   native step's param byte likewise — if the computed value exceeds 255,
   FAIL with a config message instead of truncating). Require: both sides
   leave X UNCHANGED and `lastErrorCode == ERROR_NONE` on both — the
   traced native silence, now shared.
3. **PTP_NONE dispatch still green through the seam.** Hand-build a word
   emitting `FTOK_C47` + the itemId of a known PTP_NONE item already used
   by the suite's step-4 tests (grep `forthFindItem` tests for the item
   they use and reuse it; FAIL-config if none found); execute and require
   its known effect (assert whatever that existing test asserts, locally
   restated).

### Existing tests and comments

All existing tests stay green unchanged EXCEPT any test that pins the old
unchecked out-of-range Forth dispatch — if one reddens, STOP and report
(rule 6; the authority section anticipates this as an architect decision).

### Non-goals / STOP boundaries

- No byte-encoding changes (validator and save format untouched).
- No compile-side emit changes (`forthFindItem` stays PTP_NONE-only; F4
  owns textual parameters).
- No indirect/name/label work in the Forth arm (still rejected by its
  `default` branch until a later stage rules them).
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue
only when the named subcase goes RED for the named reason:

1. In `paramCoreValidateDirect`, drop the NUMBER_8 range check (return
   true). Subcase 2 must go RED: the Forth side dispatches the
   out-of-range parameter (X changes or an error appears) while the
   native side stays silent — parity broken.
2. In the re-routed FTOK_C47 arm, replace the `paramCoreDispatchDirect`
   call for NUMBER_8 with a direct `reallyRunFunction(itemId, param)`
   bypassing validation. Subcase 2 must go RED for the same reason.
3. In `paramCoreDispatchDirect`, swap the argument order (`value, op`) —
   compile permitting via casts — or if it does not compile, change the
   call to `reallyRunFunction((int16_t)op, NOPARAM)`. Subcase 1 must go
   RED (SDL executes with the wrong parameter; X != 1000).

After all mutations, grep for `MUTATION F2-3` (no match), run the full
gate green again, and record: all three PASS lines; both success banners
and exit 0; the `FORTH ARENA` line; `git diff --check`; byte equality
between each flat file and its generated `files/` counterpart.

### Commit

After the final green gate, `git status --short` may contain only the four
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F2-3 — FTOK_C47 dispatches through the shared parameter core
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the seam functions as landed, the FTOK_C47 arm's diff shape, all
three PASS lines, each mutation's RED symptom, the final gate and arena
lines, commit hash, and anything surprising — explicitly including any
existing test that pinned the old unchecked dispatch.
