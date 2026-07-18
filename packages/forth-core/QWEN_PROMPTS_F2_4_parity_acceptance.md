# Stage F2-4 — parameter parity acceptance sweep (bounded implementation prompt)

Origin: DESIGN §10.2's acceptance obligation via the stage trace in
`QWEN_PROMPTS_F2_core.md` — after F2-1..F2-3, drift between native and
Forth parameter semantics must be a loud test failure, permanently.
Authored 2026-07-17 against the post-F2-3 target tree; the gate below fails
closed on any drift.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -10` contains the F2-1, F2-2, and F2-3 stage commits
   (`extract the native parameter core`, `explicit end bound`,
   `dispatches through the shared parameter core`).
3. `grep -n "paramCoreValidateDirect\|paramCoreDispatchDirect"
   packages/forth-core/programming/param_core.h` shows both prototypes,
   and `grep -c "tamMinMax" packages/forth-core/forth_inner.c` prints `0`.
4. `grep -n "test_c47_param_shared_dispatch" packages/forth-core/test_dict_reloc.c`
   shows the F2-3 test registered.
5. `grep -n "test_param_parity_sweep" packages/forth-core/test_dict_reloc.c`
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
   `/tmp/forth-f2-4-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f2-4-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f2-4-gate.log | head -n 30` or a grep
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
   (`packages/forth-core/QWEN_PROMPTS_F2_4_parity_acceptance.md`), the todo
   file `/tmp/forth-f2-4-todo.md`, and `git status --short` / `git diff`
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

## F2-4 — Parity is pinned: same item, same parameter, same outcome, forever

### Authority carried by this packet

Decided contract (stage trace + F2-3 as landed):

- Parity means: for every PTP class Forth can carry (`PTP_NONE`,
  `PTP_NUMBER_8`, `PTP_NUMBER_16`), a native program step and a
  hand-assembled `FTOK_C47` call with the same item and same parameter
  value produce IDENTICAL `X`, identical `lastErrorCode`, and identical
  no-op-ness (the traced silent out-of-range behavior included).
- The sweep discovers its NUMBER_16 fixtures from the live item table at
  run time (self-verifying, no hardcoded item beyond the guards): find the
  first item with `PTP_NUMBER_16` and `isFunctionOldParam16(i)` true, and
  the first with `PTP_NUMBER_16` and `isFunctionOldParam16(i)` false. If
  either search fails, FAIL with a config message naming the missing
  class — architect feedback, not a skip. Items whose execution needs
  interactive state (TAM, menus, alpha) make the OUTCOME comparison the
  assertion: the test never asserts a specific effect for discovered
  items, only that both worlds did the SAME thing (X, error code, and for
  register-writing items the written register when cheaply probeable).
- Corruption asymmetry is deliberate and pinned: an `itemId >= LAST_ITEM`
  cannot be encoded in a native step (the step encoder cannot produce it)
  but can appear in a corrupted dictionary — Forth's runtime must keep
  rejecting it with `ERROR_INVALID_CORRUPTED_DATA` (runtime layer) and
  the F1-5 validator must keep rejecting it at restore. Both already
  exist; this sweep pins the runtime half stays after the F2-3 re-route.
- §5.4 arena reporting is mandatory; this stage commit additionally
  records the RULE-1 flash delta (`make dmcp5r47`, run by the owner —
  note it in the report as pending if not available in this session).

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. In `test_dict_reloc.c`, grep
   `test_c47_param_shared_dispatch\|begin_word\|end_word\|writeTestProgram`
   and read the F2-3 test in full (this sweep generalizes its pattern),
   the helpers, and the newest registration lines.
2. In `programming/param_core.c`, grep
   `paramCoreValidateDirect\|isFunctionOldParam16` and read the validate
   function only (the range semantics being pinned).
3. In `forth_inner.c`, grep `ERROR_INVALID_CORRUPTED_DATA` and read 8
   lines around the itemId bounds check (the corruption reject being
   pinned).

### Change 1 — the sweep test

Add `test_param_parity_sweep`, registered after
`test_c47_param_shared_dispatch`. Fixture discipline as usual. Build one
helper INSIDE the test file scope if useful (e.g., a static
`driveParityPair(itemId, param, …)` that runs the native step then the
hand-assembled Forth call from identical starting state and compares X +
`lastErrorCode`), modeled on the F2-3 test. Native step encoding for a
discovered item: one-byte opcode `id` if `id < 128`, else
`0x80 | (id >> 8), id & 0xFF`, followed by the parameter byte(s) in the
class's native form (NUMBER_8: one byte; NUMBER_16 oldParam16:
little-endian two bytes; NUMBER_16 new-form: one byte < 128 for the
values this sweep uses — use parameter values ≤ 99 so both 16-bit forms
encode in their simple shapes; if the targeted read of the native
PARAM_NUMBER_16 arm shows the new-form single-byte encoding differs, STOP
and report). Four independently reported subcases:

1. **NUMBER_8 in-range + out-of-range parity** (reuse ITM_SDL with the
   F2-3 config guard; params 3 and max+1).
2. **NUMBER_16 oldParam16 parity** (discovered item; param 5).
3. **NUMBER_16 new-form parity** (discovered item; param 5).
4. **Corrupted itemId still rejected at runtime.** Hand-build a word
   emitting `FTOK_C47` + cell `LAST_ITEM` (an illegal id) + EXIT via the
   raw emit path the F1-5 tests use; execute; require
   `lastErrorCode == ERROR_INVALID_CORRUPTED_DATA` and depth/rsp unwound
   (`forthTestGetDepth() == 0`).

Every subcase prints one PASS line naming the pinned property.

### Existing tests and comments

All existing tests stay green unchanged. If any reddens, STOP (rule 6).

### Non-goals / STOP boundaries

- No production edits at all in this packet (mutations excepted, restored).
- No new PTP classes for Forth (later stages), no compile-side changes.
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue
only when the named subcase goes RED for the named reason:

1. In `paramCoreValidateDirect`, change the NUMBER_8 comparison from `<=`
   to `<`. Subcase 1 must go RED at the exact boundary (max is in-range
   natively — the sweep must drive param == max in its in-range half to
   make this mutation observable; include that value).
2. In the FTOK_C47 arm, change the itemId bounds check from
   `>= LAST_ITEM` to `> LAST_ITEM`. Subcase 4 must go RED (the illegal id
   `LAST_ITEM` dispatches or crashes; record the symptom).

After all mutations, grep for `MUTATION F2-4` (no match), run the full
gate green again, and record: all four PASS lines; both success banners
and exit 0; the `FORTH ARENA` line (mandatory §5.4 report — compare
against the pre-task baseline captured in your todo file); `git diff
--check`; byte equality between the flat file and its generated `files/`
counterpart.

### Commit

After the final green gate, `git status --short` may contain only
`packages/forth-core/test_dict_reloc.c`, its generated `files/`
counterpart, and `packages/forth-core/.refresh-manifest.json`. Stage those
exact paths only and commit:

```text
forth-core: F2-4 — native/Forth parameter parity is pinned by sweep
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the discovered NUMBER_16 fixture items (ids + names), all four
PASS lines, each mutation's RED symptom, the final gate and arena lines
(with the high-water delta), the RULE-1 flash-delta status, commit hash,
and anything surprising.
