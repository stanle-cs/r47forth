# Stage F6-4 — parameter entry emits canonical text

Origin: DESIGN §10.6 via `QWEN_PROMPTS_F6_core.md` §2 decision 7.  After
F6-2, a TAM flow initiated during capture (e.g. STO picked from a catalog
— the physical keys are alpha glyphs during capture, T7) suspends the
capture, inserts its NATIVE STEP after the capture line, and resumes.
This packet converts that commit into the §10.6 sink contract: the
committed step is rendered to canonical text through the LANDED decoder
(the same renderer PEM listings use — F4 parity by construction), the
text is inserted at the capture cursor, and the step is removed.  A TAM
cancel still inserts nothing.  Legality is the runtime's business:
converted text that names a Forth-rejected native step (e.g. flow steps
per F4) fails at RUN exactly as if typed by hand — no special cases.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F6-3 commit
   `forth-core: F6-3 — catalogs and menus during capture`.
2. `grep -n "forthCaptureResume" packages/forth-core/programming/manage.c`
   shows the F6-2 orchestrator (the extension point).
3. `grep -rn "savedStepCount\|forthCapSavedStepCount" packages/forth-core`
   → ZERO matches (this packet adds the snapshot field).
4. `grep -n "forthCapInsertName" packages/forth-core/forth_capture.h`
   matches (the F6-3 inserter this packet reuses).
5. `grep -n "void decodeOneStep" packages/forth-core/programming/decode.c`
   matches (the canonical renderer).
6. Pre-gate green; arena baseline from the F6-3 commit message.

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
   `/tmp/forth-f6-4-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-4-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `manage.c`,
   `keyboard.c`, `ui/tam.c`, `programming/decode.c`, or `test_dict_reloc.c`
   in full.  Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f6-4-todo.md`,
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

**PROGRAM-FIXTURE AUTHORING RULE (mandatory)**

`test_dict_reloc.c` program fixtures are structural, not hand-addressed.
Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
the returned step handle when a test must execute or inspect that step, and
resolve it with `tpStepAddr`; abort the subcase if fixture construction,
`tpWrite`, or address lookup fails.

Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
`tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
authors must identify steps by role and must not publish a calculated byte
offset as a normative literal. If a packet contains such an offset, stop
with `[SOL DEBUGGER HANDOFF]` and report the packet defect; do not repair
its arithmetic locally.

Use a typed builder accessor such as `tpSrcPayload` for an internal field.
If the needed step or field helper does not exist, extend the central fixture
builder first; do not introduce local pointer arithmetic in the test.

Prefer named opcode/parameter constants in builder helpers. An exact byte
array may remain as the expected value of an encoding assertion. Raw bytes
inserted into the program fixture are allowed only for the encoding under
test or a deliberate malformation; they must enter through `tpRaw`, carry an
adjacent comment naming that purpose, and still use the returned handle and
builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
behavior fixture.

This rule is prospective. Do not widen the task by converting untouched
legacy fixtures.

**CAPTURE-DRIVE CONTRACT (binding rule 8).**  Every fixture that types into
a Forth region drives `runFunction(ITM_AIM)` FIRST with the cursor ON the
opening marker and `pemCursorIsZerothStep` fixture-owned.  Reuse the landed
F15-2 / F6-1 / F6-2 drive idioms verbatim; never invent a new drive.

---

## F6-4 — the suspended commit becomes text

### Authority carried by this packet (no open choices)

1. **Snapshot field** (forth_capture.h/.c): add `uint16_t savedStepCount;`
   to `forthCap_t`; `forthCapSuspendState` gains a fourth parameter
   `uint16_t stepCount` stored there; accessor
   `uint16_t forthCapSavedStepCount(void)`.  `forthCaptureSuspend`
   passes `getNumberOfSteps()`.
2. **Conversion block** (programming/manage.c, `forthCaptureResume`) —
   inserted AFTER the buffer refill/cursor restore and BEFORE the
   FLAG_ALPHA/GUI re-establishment:

   ```c
   /* F6-4: steps the suspended TAM committed become canonical text.
    * n is 0 (cancel) or 1 (one commit) today; the loop is defensive. */
   { uint16_t n = getNumberOfSteps() - forthCapSavedStepCount();
     while (n > 0) {
       uint8_t *ins = findNextStep(currentStep);   /* first inserted step */
       decodeOneStep(ins);                          /* canonical text → tmpString */
       if (stringByteLength(tmpString) > 255) {
         break;   /* defensive: keep the step rather than truncate text */
       }
       { char conv[258]; char *t = conv;            /* 1 + 255 + NUL */
         if (T_cursorPos > 0 && forthCapBuf()[T_cursorPos - 1] != ' ') {
           *t++ = ' ';        /* word separator when mid-text */
         }
         xcopy(t, tmpString, stringByteLength(tmpString) + 1);
         if (!forthCapInsertName(conv)) {
           break;   /* no room: keep this and later steps after the line */
         }
       }
       deleteStepsFromTo(ins, findNextStep(ins));
       --n;
     }
   }
   ```

   Ruled: `decodeOneStep` is the SAME renderer PEM listings display, so
   the inserted spelling is F4's canonical spelling by construction;
   `forthCapInsertName` appends the standard trailing space and enforces
   the 256/196 cap; on no-room the step(s) stay after the capture line —
   degraded but lossless.  If `decodeOneStep`'s output for a plain step
   carries a prefix (step number or mode glyph) rather than the bare
   canonical text, STOP and report with the observed string — the
   architect supplies the correct render entry point; do not trim
   heuristically.
3. `forthCaptureResume`'s currentStep/localStep restore already ran
   before the block (F6-2), so `currentStep` IS the capture step, and
   `findNextStep(currentStep)` is the insertion site: suspend leaves the
   cursor ON the capture line and TAM commits insert AFTER it through
   `addStepInProgram`'s pre-move (F6-2's traced contract).
4. No dispatch changes: how TAM gets entered (catalog picks, direct
   `runFunction`) is F6-2/F6-3 territory and untouched.

### Files

Modify only: `forth_capture.h`, `forth_capture.c`,
`programming/manage.c`, `test_dict_reloc.c` — all under
`packages/forth-core/`.

### Targeted reads

1. manage.c: `forthCaptureSuspend`/`forthCaptureResume` in full (small).
2. forth_capture.c/h in full (small).
3. decode.c: `decodeOneStep`'s head (what lands in `tmpString` for a
   two-byte-parameter step — 10 lines).
4. test_dict_reloc.c: `test_capture_suspend` (drive idioms to reuse),
   registration lines.

### Change A/B/C — as Authority items 1-2 (exact bodies above)

### Change D — named test rework (rule 6 authorization)

`test_capture_suspend` subcase 1 pinned the F6-2 interim ("STO 05 step
sits after the capture line").  Rework its tail: after resume require
`forthTestCapText()` == `"5 DUP STO 05 "` (the converted text appended at
the end-of-line cursor), NO step after the capture step, and
`getNumberOfSteps()` equal to its pre-STO value.  New PASS line:
`[1] PASS: TAM commit suspends, converts to text, and resumes`.
ESCAPE VALVE: if the decode spelling of the `0x2C 0x05` step is not
exactly `STO 05`, STOP and report the observed spelling — the architect
reconciles the literal against the landed F4 grammar table; never adapt
the expected string locally.

### Change E — new test `test_capture_param_text` (register after the newest test)

Fixture per the drive contract (`tpLbl("F64")` + `tpMarker`).

1. **Cancel still inserts nothing.**  Open capture, type `1`, enter
   `tamEnterMode(ITM_STO)` directly (the landed TAM-test idiom), cancel
   via `fnKeyExit(NOPARAM)`.  Require text exactly `"1"`, step count
   unchanged.  PASS: `[1] PASS: TAM cancel converts nothing`
2. **Name commit converts.**  With text `2 ` and cursor at end, enter
   `tamEnterMode(ITM_XEQ)`, then the landed chain's keys
   (`tamProcessInput(ITM_alpha)`, `runFunction(ITM_W)`,
   `runFunction(ITM_A)`, commit `tamProcessInput(ITM_ENTER)` — mirror
   the landed `test_tam_colon_never_falls_to_forth` chain).  Require text
   `"2 XEQ 'WA' "` (escape valve as Change D if the decode spelling of
   the name step differs), no residual step, step count unchanged.  PASS:
   `[2] PASS: XEQ name commit becomes source text`
3. **No-room keeps the step.**  Fill the line to within 3 glyphs of the
   196 cap (fixture loop of alternating `X`/space keys), then
   `tamEnterMode(ITM_STO)`, `tamProcessInput(ITM_0)`,
   `tamProcessInput(ITM_5)`.  Require: text unchanged
   (insert refused), the `0x2C 0x05` step PRESENT after the capture
   step, no crash; then BACKSPACE-abort and delete the stray step via
   the PEM idiom (or `cleanupTestProgram()`).  PASS:
   `[3] PASS: no-room conversion keeps the committed step`
4. **Hygiene across cycles.**  Two convert cycles then abort-close:
   `getFreeRamMemory()` equality vs pre-open.  PASS:
   `[4] PASS: conversion cycles leave zero residue`

### Existing tests

All stay green except the NAMED subcase rework (Change D).
`test_capture_buffer`, `test_capture_menus`, F15 and F5 suites are the
canaries.

### Non-goals / STOP boundaries

- No new TAM entry points; no keyboard remaps.
- No legality filtering of converted text (runtime owns it).
- No catalog content change (F6-5).

### Gate and required mutations

Full gate green first (four new PASS lines + reworked suspend [1] +
every legacy banner).  Mutations, each separately, logs
`/tmp/forth-f6-4-mut1..3.log`; co-reds beyond the named RED are
expected; post-restore gate green between mutations.

1. Delete the whole conversion block.  Reworked `test_capture_suspend`
   [1] MUST go RED (step remains, text lacks `STO 05`).
2. Delete the `deleteStepsFromTo` line only.  Reworked [1] MUST go RED
   (text AND step both present — step count mismatch).
3. Move `deleteStepsFromTo(ins, findNextStep(ins));` ABOVE the compound
   insert block (delete-before-insert).  Test E subcase 3 MUST go RED
   (no-room now loses the step).

Report: PASS lines, both banners, exit 0, arena line vs baseline,
`git diff --check`, generated-mirror equality, three mutation REDs, any
escape-valve outcome.  RULE-1: negligible flash delta expected; note the
measured delta.

### Commit

```text
forth-core: F6-4 — parameter entry emits canonical text
```

---

## REGRESSION + ENTRY-POINT RULES (binding, added 2026-07-19 after the F5-2 debug)

These cost a full session at F5-2, where a correct six-line change was
blamed for four red tests it never touched. They apply to this packet
whether or not the body above repeats them.

1. **A red outside your diff is an immediate STOP — zero repair attempts.**
   The two-attempt allowance applies only to code or tests THIS packet
   authored. If a test you did not write reddens, stop and report
   `[SOL DEBUGGER HANDOFF]` at once. Do not first try to decide whether your
   change could have caused it: "my change cannot have caused this" is the
   most common wrong conclusion, and deciding it is the debugger's job.
2. **Name the blast radius by diffing PASS sets, not by reading failures.**
   Keep the pre-gate log. Then:
   `diff <(grep -o "PASS: .*" /tmp/<pre>.log | sort) <(grep -o "PASS: .*" /tmp/<gate>.log | sort)`
   Newly-missing PASS lines in untouched tests are the report.
3. **Entry-point contract pre-flight.** Before wiring an existing function
   into a new call site, prove it saves/restores the process-global state its
   siblings do — grep the other entry points for the fields the shared
   epilogue restores and compare. A function correct in isolation can be
   wrong the moment a second caller exists. A mismatch is a packet defect:
   STOP and report, do not patch around it.
4. **Pin the contract, not just the verdict.** If this packet adds an entry
   point whose spec claims state neutrality ("mutates no live state",
   "restores the mode", "leaves the buffer untouched"), pin that claim
   directly: set the state to a NON-default value, drive both the accepting
   and the rejecting path, assert the state came back. See
   `test_check_source_line` subcase 6 and `poisonAutoFrame()` for the landed
   shape — the poison makes an uninitialized restore deterministic instead of
   luck-of-the-stack.
