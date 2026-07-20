# Stage F6-2 — TAM suspend/resume keeps capture alive

Origin: DESIGN §10.6 via `QWEN_PROMPTS_F6_core.md` §2 decision 6 and the
T2 trace of `F6_AUDIT_RESULTS.md`.  Landed behavior (preserved verbatim
through F6-1's interim guard): entering TAM with a non-empty capture line
COMMITS AND CLOSES it (tam.c CM_PEM arm), and the empty-line case lets
TAM open over the still-open capture.  This packet replaces BOTH with one
uniform contract: any TAM entry while capture is open SUSPENDS the
capture (buffer freed — the committed step is the single source of
truth) and every TAM exit RESUMES it (refill from the step, restore
cursor/position/mode).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F6-1 commit
   `forth-core: F6-1 — managed capture buffer behind the capture object`.
2. `grep -n "FCAP_SUSPENDED" packages/forth-core/forth_capture.h` matches
   the enum; `grep -rn "forthCapSuspendState\|forthCaptureSuspend\|forthCaptureResume" packages/forth-core`
   → ZERO matches (this packet adds them).
3. `grep -n "aimBuffer\[0\] != 0 || forthCapTextNonEmpty()" packages/forth-core/ui/tam.c`
   → exactly ONE match (F6-1's interim guard — this packet's edit target).
4. `grep -n "savedStepOffset" packages/forth-core/forth_capture.h` matches
   (the dormant F6-1 fields).
5. `grep -n "void leaveTamModeIfEnabled" packages/forth-core/ui/tam.c`
   matches; `grep -n "hourGlassIconEnabled = false" packages/forth-core/ui/tam.c`
   locates its CM_PEM tail (the resume hook site).
6. Pre-gate green; arena baseline from the F6-1 commit message.

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
   `/tmp/forth-f6-2-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-2-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `manage.c`,
   `keyboard.c`, `ui/tam.c`, `screen.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f6-2-todo.md`,
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
F15-2 / F6-1 drive idioms verbatim; never invent a new drive.

---

## F6-2 — suspend on TAM entry, resume on TAM exit

### Authority carried by this packet (no open choices)

1. **State ops** (forth_capture.c; prototypes in forth_capture.h):

   ```c
   void forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset) {
     forthCap.savedCursor     = cursor;
     forthCap.savedLocalStep  = localStep;
     forthCap.savedStepOffset = stepOffset;
     if (forthCap.buf != NULL) {
       freeC47Blocks(forthCap.buf, forthCap.sizeBlocks);
       forthCap.buf = NULL;
     }
     forthCap.sizeBlocks = 0;
     forthCap.state = FCAP_SUSPENDED;
   }
   bool_t   forthCapIsSuspended(void)     { return forthCap.state == FCAP_SUSPENDED; }
   uint16_t forthCapSavedCursor(void)     { return forthCap.savedCursor; }
   uint16_t forthCapSavedLocalStep(void)  { return forthCap.savedLocalStep; }
   uint32_t forthCapSavedStepOffset(void) { return forthCap.savedStepOffset; }
   void     forthCapAbandonSuspended(void){ if (forthCap.state == FCAP_SUSPENDED) forthCap.state = FCAP_CLOSED; }
   ```

   Additionally, `forthCapOpen`'s first line gains the orphan guard:
   `if (forthCap.state == FCAP_SUSPENDED) { forthCap.state = FCAP_CLOSED; }`
   (belt for exotic flows that bypass the resume choke point; the state
   holds no allocation while suspended, so closing is a pure flip).
2. **Orchestrators** (programming/manage.c — they need the file-static
   `_closeAlphaMenus`; declare both in forth_capture.h):

   ```c
   void forthCaptureSuspend(void) {
     if (!forthCapIsOpen()) { return; }
     uint16_t cursor    = T_cursorPos;
     uint16_t localStep = currentLocalStepNumber;
     uint32_t stepOff   = (uint32_t)(currentStep - beginOfProgramMemory);
     /* currentStep stays ON the capture step: the landed commit-and-close
      * nets to exactly that (pemCloseAlphaInput steps forward, the tam.c
      * arm steps back), and TAM commits insert via
      * addStepInProgram(tamOperation()), whose pre-move already places
      * the new step AFTER the current one.  Moving here would shift the
      * TAM insert one step too late.
      * tam.function is NOT touched: tamEnterMode assigned the incoming
      * TAM function before this seam; zeroing it would break the TAM
      * session (the landed close's unconditional reset is the very
      * behavior suspend replaces). */
     clearSystemFlag(FLAG_ALPHA);
     calcModeNormalGui();
     _closeAlphaMenus();
     forthCapSuspendState(cursor, localStep, stepOff);
   }

   void forthCaptureResume(void) {
     if (!forthCapIsSuspended()) { return; }
     uint8_t *p = beginOfProgramMemory + forthCapSavedStepOffset();
     if (!(p < firstFreeProgramByte
           && checkOpCodeOfStep(p, ITM_FORTH)
           && p[2] == (uint8_t)STRING_LABEL_VARIABLE)) {
       forthCapAbandonSuspended();             /* defensive canary — see test 5 */
       #if defined(FORTH_DEBUG_SELFTEST)
       printf("FORTH CANARY: suspended capture step falsified; suspension abandoned\n");
       #endif
       return;
     }
     forthCapOpen();                           /* SUSPENDED → orphan-flip → alloc */
     if (!forthCapIsOpen()) { return; }        /* RAM_FULL shown; capture lost */
     { uint8_t len = p[3];                     /* len 0 = empty line, legal */
       if (len > 0) { xcopy(forthCapBuf(), p + 4, len); }
       forthCapBuf()[len] = 0;
       T_cursorPos = forthCapSavedCursor();
       if (T_cursorPos > len) { T_cursorPos = len; }
     }
     currentLocalStepNumber = forthCapSavedLocalStep();
     currentStep = p;
     tam.function = ITM_FORTH;                 /* capture-era tam is exactly
                                                  {mode 0, function ITM_FORTH} */
     resetShiftState();                        /* fresh-open parity */
     setSystemFlag(FLAG_ALPHA);
     calcModeAimGui();
     showSoftmenu(-MNU_ALPHA);
     pemCursorIsZerothStep = false;
   }
   ```

   Place both directly after `pemCloseAlphaInput` in manage.c.
3. **Suspend hook** (ui/tam.c) — F6-1's interim guard line

   ```c
       else if(calcMode == CM_PEM && (aimBuffer[0] != 0 || forthCapTextNonEmpty())) {
   ```

   is REPLACED by a capture-first split:

   ```c
       else if(calcMode == CM_PEM && forthCapIsOpen()) {
         forthCaptureSuspend();                /* F6-2: suspend, never close */
       }
       else if(calcMode == CM_PEM && aimBuffer[0] != 0) {
   ```

   with the original arm's body (the `pemCloseAlphaInput`/
   `pemCloseNumberInput` branch, buffer zero, step-back) kept VERBATIM
   under the second condition — it now serves REM/LITERAL/NIM captures
   only.  The uniform `forthCapIsOpen()` condition intentionally covers
   the EMPTY capture line too, superseding the landed TAM-over-open-
   capture edge (core ledger decision 6).
4. **Resume hook** (ui/tam.c) — in `leaveTamModeIfEnabled`, the existing
   tail

   ```c
     if(calcMode == CM_PEM) {
       hourGlassIconEnabled = false;
     }
   ```

   becomes

   ```c
     if(calcMode == CM_PEM) {
       hourGlassIconEnabled = false;
       forthCaptureResume();                   /* no-op unless FCAP_SUSPENDED */
     }
   ```

   This is the SINGLE resume choke point; the EXIT-cancel arm
   (keyboard.c) already funnels through `leaveTamModeIfEnabled` and needs
   no edit — its PEM `aimBuffer[0] = 0;` line is harmless (the capture
   text no longer lives there).
5. No other seams.  `pemCloseAlphaInput`, the ENTER arm, the E9 gate, the
   picker, and all F6-1 sites are untouched.

### Files

Modify only: `forth_capture.h`, `forth_capture.c`,
`programming/manage.c`, `ui/tam.c`, `test_dict_reloc.c` — all under
`packages/forth-core/`.

### Targeted reads

1. forth_capture.c/h in full (small).
2. manage.c: `pemCloseAlphaInput` and 10 lines after it (insertion
   point); `_closeAlphaMenus` (grep, read the function).
3. ui/tam.c: the interim-guard arm and 15 lines around it; the
   `leaveTamModeIfEnabled` tail (grep anchors from the EXECUTION GATE).
4. test_dict_reloc.c: `test_capture_buffer` (the F6-1 test — reuse its
   drive helpers and subcase-7 TAM idiom), tp helpers, registration
   lines.

### Change T — named test rework (rule 6 authorization)

`test_capture_buffer` subcase 7 (`[7] PASS: TAM entry commits-and-closes
the capture (F6-1 interim)`) pinned the INTERIM behavior this packet
deletes.  Rework it to assert the new contract minimally (the deep pins
live in the new test): after `tamEnterMode(ITM_STO)` from an open capture
with text `9`, require `forthTestCapState() == FCAP_SUSPENDED` while
`tam.mode != 0`; after the cancel (`fnKeyExit(NOPARAM)`), require capture
OPEN with text `9` intact.  New
PASS line: `[7] PASS: TAM entry suspends the capture (F6-2)`.

### Change U — new test `test_capture_suspend` (register after the newest test)

Fixture: `tpLbl("F62")` + `tpMarker`, `tpWrite`; record
`freeBefore = getFreeRamMemory();` before opening.  Drive per the
CAPTURE-DRIVE CONTRACT; reuse the F6-1 typing/TAM/EXIT idioms.

1. **Commit round-trip.**  Open capture, type `5 DUP`.  Record
   `stepNumBefore = currentLocalStepNumber;` then enter TAM directly:
   `tamEnterMode(ITM_STO);` (the landed idiom — the tree's TAM tests call
   `tamEnterMode(ITM_XEQ)` directly), then digits via
   `tamProcessInput(ITM_0); tamProcessInput(ITM_5);` (the landed TAM
   commit auto-fires at two digits; if digit input routes through a
   different landed call, STOP and report).  WHILE `tam.mode != 0`
   (probe after `tamEnterMode`, before the digits): require
   `forthTestCapState() == FCAP_SUSPENDED` and `FLAG_ALPHA` clear.  After
   the commit completes: require capture OPEN; `forthTestCapText()` ==
   `"5 DUP"`; `T_cursorPos` restored to its pre-suspend value;
   `currentLocalStepNumber == stepNumBefore` (cursor back ON the capture
   line); `tam.mode == 0 && tam.function == ITM_FORTH`; and the committed
   `STO 05` step (byte image `0x2C 0x05`, an encoding assertion) sits
   at `findNextStep` of the capture step — AFTER the capture line.  PASS:
   `[1] PASS: TAM commit suspends, inserts after the line, and resumes`
2. **Cancel round-trip.**  Same entry (`tamEnterMode(ITM_STO)`), then
   cancel via `fnKeyExit(NOPARAM)` before any digit.  Require: capture
   OPEN, text intact, NO new step after the capture step,
   `tam.mode == 0`.  PASS:
   `[2] PASS: TAM cancel resumes with no inserted step`
3. **tam.colon no-leak.**  From the open capture enter
   `tamEnterMode(ITM_XEQ)`, then the local-label `:` gesture (sets
   `tam.colon` — reuse the landed `test_tam_colon_never_falls_to_forth`
   drive), then cancel back via `fnKeyExit(NOPARAM)` (repeat until
   `tam.mode == 0` if the first press only pops a TAM menu).
   Require: capture OPEN, `tam.colon == false`, then type `X` and require
   it appended to the capture text.  PASS:
   `[3] PASS: nested tam.colon state does not leak into resumed capture`
4. **Empty-line suspension (edge unified).**  ENTER to a fresh empty
   line, enter `tamEnterMode(ITM_STO)`, probe `FCAP_SUSPENDED` (the
   landed code let TAM open OVER an empty capture; now it suspends
   uniformly), cancel via `fnKeyExit(NOPARAM)`.
   Require: capture OPEN, empty text, placeholder step still present.
   PASS: `[4] PASS: empty-line TAM entry suspends uniformly`
5. **Falsified-step canary.**  Suspend (`tamEnterMode(ITM_STO)`, no
   digits).  Deliberate
   malformation: overwrite the saved capture step's first byte in
   program memory with `0x04` (ITM_RTN) via a direct write — comment it
   as the deliberate falsification of this subcase.  Cancel via
   `fnKeyExit(NOPARAM)`.
   Require: NO crash, capture CLOSED (`FCAP_CLOSED`), `tam.mode == 0`.
   Restore program memory via `cleanupTestProgram()` at subcase end.
   PASS: `[5] PASS: falsified suspension abandons safely`
6. **Arena equality.**  After a fresh fixture: open → type `1` →
   suspend (`tamEnterMode(ITM_STO)`) → cancel-resume (`fnKeyExit`) →
   suspend → cancel-resume → BACKSPACE-abort to close (empty the line
   first).  Require `getFreeRamMemory() == freeBefore`.  Escape valve:
   if the only delta matches one program-memory resize quantum, STOP
   and report (program-region growth, not a capture leak).  PASS:
   `[6] PASS: suspend/resume cycles leave zero arena residue`

Cleanup per subcase: close capture, `lastErrorCode = ERROR_NONE`,
restore mode globals, `cleanupTestProgram()`.

### Existing tests

All stay green except the NAMED subcase-7 rework (Change T).  F15-2/3/4
and the F5 commit-gate suites are the canaries that ordinary capture,
display, and ENTER behavior are untouched.

### Non-goals / STOP boundaries

- No parameter-entry TEXT emission (F6-4): a TAM commit still inserts
  its native STEP (asserted by test 1 — that IS this stage's contract).
- No catalog behavior change (F6-3/F6-5).
- No power-off handling change (F6-6 pins it).

### Gate and required mutations

Full gate green first (six new PASS lines + reworked [7] + every legacy
banner).  Mutations, each separately, logs `/tmp/forth-f6-2-mut1..5.log`;
co-reds beyond the named RED are expected; post-restore gate green
between mutations.

1. Delete the `forthCaptureResume();` call in `leaveTamModeIfEnabled`.
   Test U subcase 1 MUST go RED (capture not reopened after commit).
2. Delete the two position-restore lines in `forthCaptureResume`
   (`currentLocalStepNumber = forthCapSavedLocalStep();` and
   `currentStep = p;`).  Subcase 1 MUST go RED
   (`currentLocalStepNumber != stepNumBefore` — the cursor is left where
   TAM's insert put it, not on the capture line).
3. Delete `tam.function = ITM_FORTH;` in `forthCaptureResume`.  Subcase 1
   MUST go RED (`tam.function != ITM_FORTH` after resume).
4. Delete the structural-validation `if` (keep the abandon call
   unreachable) in `forthCaptureResume` — i.e. force the refill path.
   Subcase 5 MUST go RED (resume proceeds on the falsified step).
5. Revert the tam.c suspend hook to F6-1's interim single condition
   (quote: the Change 3 "REPLACED by" block reversed).  Subcase 2 MUST
   go RED (capture closed-and-committed instead of suspended-and-resumed).

Report: six PASS lines + reworked [7], both banners, exit 0, arena line
vs baseline, `git diff --check`, generated-mirror equality, five
mutation REDs.  RULE-1: negligible flash delta expected; note the
measured delta.

### Commit

```text
forth-core: F6-2 — TAM suspend/resume keeps capture alive
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
