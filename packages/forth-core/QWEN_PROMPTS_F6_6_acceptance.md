# Stage F6-6 — capture acceptance battery

Origin: DESIGN §10.6 via `QWEN_PROMPTS_F6_core.md` §1/§2 decision 10 and
the deferred-bench register of `F6_AUDIT_RESULTS.md`.  This packet is the
stage's end-to-end pin: a full type→commit→run session through the real
toggle and key paths, the EXIT ladder walked rung by rung, the
empty-line/marker rules, the power-off/restore contract for the capture
object, cap round-trips, and an arena-residue sweep.  One implementation
change rides along: the capture object joins the dictionary's lifecycle
seams so a RESTORE (or fresh init) can never resurrect or leak a capture.
When this packet lands, the stage-exit HARDWARE bench (charter Blocks
A-F, deferred by the 2026-07-18 owner ruling) is the only F6 work left.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F6-5 commit
   `forth-core: F6-5 — the dictionary-backed word catalog`.
2. `grep -rn "forthCapPowerReset" packages/forth-core` → ZERO matches
   (this packet adds it).
3. `grep -n "forthScanTrackReset" packages/forth-core/forth_dict.c` →
   exactly TWO call sites (the F1-3 lifecycle seams this packet extends;
   record their enclosing function names in the report).
4. `grep -n "power-off round-trip re-derives" packages/forth-core/test_dict_reloc.c`
   matches (the landed F15-2 power-off idiom to reuse);
   `grep -n "addStepInProgram(ITM_FORTH)" packages/forth-core/test_dict_reloc.c`
   matches (the landed toggle-drive idiom to reuse).
5. `grep -n "forthCapIsSuspended" packages/forth-core/forth_capture.h`
   matches (F6-2 landed).
6. Pre-gate green; arena baseline from the F6-5 commit message.

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
   `/tmp/forth-f6-6-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-6-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `manage.c`,
   `keyboard.c`, `forth_dict.c`, or `test_dict_reloc.c` in full.  Grep the
   named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f6-6-todo.md`,
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
opening marker and `pemCursorIsZerothStep` fixture-owned — EXCEPT the
subcase-1/3 toggle drives, which open/close the region through the real
toggle via `addStepInProgram(ITM_FORTH)` (the landed toggle-test idiom;
that path opens its own placeholder — the §8.4 E1 arm is the very thing
under test).  Reuse the landed F15-2 / F6-1..F6-4 idioms verbatim; never
invent a new drive.

---

## F6-6 — the stage pins itself end to end

### Authority carried by this packet (no open choices)

1. **Lifecycle reset** (forth_capture.h/.c):

   ```c
   /* F6-6: capture cannot outlive the dictionary lifecycle.  Called at
    * the same seams as forthScanTrackReset (init / clear / restore
    * validation): a restored or re-initialized machine starts with the
    * capture CLOSED and the buffer freed.  Deep-sleep wake on hardware
    * does NOT run these seams — a sleeping capture legitimately
    * survives, matching the landed FLAG_ALPHA behavior. */
   void forthCapPowerReset(void) {
     forthCapClose();              /* frees if open; flips SUSPENDED too */
     forthCapAbandonSuspended();   /* explicit for the suspended state */
   }
   ```

   If `forthCapClose` does not already flip a SUSPENDED state to CLOSED
   (check the F6-1/F6-2 landed bodies), the two calls together must —
   report which call covered which state.
2. **Seam wiring** (forth_dict.c): add `forthCapPowerReset();`
   immediately AFTER each of the exactly-two `forthScanTrackReset();`
   call sites (the F1-3 lifecycle seams — init and restore-validation
   paths).  No other seam: ordinary `forthDictClear()` from tests is not
   a power event (the gate's site count pins this).
3. **No other product changes.**  Everything else in this packet is
   test-side.

### Files

Modify only: `forth_capture.h`, `forth_capture.c`, `forth_dict.c`,
`test_dict_reloc.c` — all under `packages/forth-core/`.

### Targeted reads

1. forth_dict.c: the two `forthScanTrackReset` call sites ±5 lines.
2. forth_capture.c/h in full (small).
3. test_dict_reloc.c: the F15-2 power-off round-trip subcase (grep the
   gate's anchor string, read that subcase only) — its save/restore
   idiom is reused verbatim; the landed toggle test (grep
   `addStepInProgram(ITM_FORTH)`, read one open/close drive block); the
   F1-5 validator tests' validate-restored drive (grep
   `forthDictValidateRestored`, read one drive block); the F6-1..F6-4
   capture drive idioms; the landed picker-population idiom (for the
   ladder subcase); registration lines.

### Change A/B — as Authority items 1-2

### Change C — new test `test_capture_acceptance` (register after the newest test)

Record `freeBefore = getFreeRamMemory();` before each subcase's fixture
build; every subcase restores mode globals and cleans up per the F15
discipline.

1. **Toggle → type → run.**  Fresh program `tpLbl("F66")` + `tpWrite`
   (NO marker — the toggle inserts it).  In PEM with the cursor on the
   LBL step: `addStepInProgram(ITM_FORTH)` (the landed toggle-test
   idiom — region opens, capture opens per §8.4 E1), type
   `: SQ DUP * ;`, ENTER, type `3 SQ`, then `fnKeyExit(NOPARAM)`
   (commit-and-close; the EXIT idiom for every subcase here).  Then run the program by label
   (`dynamicMenuItem = -1; fnExecute(findNamedLabel("F66", GLOBAL_LABELS));`
   with the F15-1 run discipline).  Require `x_is_longint(9)` and the
   program bytes to contain, in order: the LBL, the opening marker
   `0x8B 0x1A 0xFD 0x00`, the two source steps (payloads
   `: SQ DUP * ;` len 12 and `3 SQ` len 4 — encoding assertions;
   `printf '%s' ": SQ DUP * ;" | wc -c` = 12, `printf '%s' "3 SQ" | wc -c`
   = 4).  PASS:
   `[1] PASS: toggle-open, two-line capture, EXIT, and label run yield 9`
2. **EXIT ladder walk.**  Reopen capture on the program (drive
   contract), text present, picker pushed (landed population idiom).
   `fnKeyExit` #1: picker popped, capture OPEN, text intact.
   `fnKeyExit` #2: capture CLOSED with the line committed.  Then fresh
   EMPTY line: one `fnKeyExit` aborts the placeholder.  PASS:
   `[2] PASS: EXIT ladder — picker pop, commit-with-text, abort-when-empty`
3. **Marker rules.**  After subcase 2's abort: the opening marker
   REMAINS (region open behind it — E3 semantics).  Position the cursor
   ON the opening marker step (the fnGotoDot/step-walk idiom — a marker
   establishes Forth-side entry state per §8.4, so `wasOn` is true),
   then `addStepInProgram(ITM_FORTH)` and require the CLOSING marker
   committed (a second `0x8B 0x1A 0xFD 0x00` occurrence) with capture
   CLOSED.
   PASS: `[3] PASS: abort keeps the region; toggle-off commits the closing marker`
4. **Restore seam closes capture (differential — the restore path has
   its own inherent free-RAM footprint, so never compare against a
   pre-restore baseline).**
   Phase 0: run the landed F15-2 power-off round-trip idiom once with NO
   capture; record `freeBase = getFreeRamMemory();` after it.
   Phase 1: open capture, type `4 4`, run the same idiom.  Require after
   restore: `forthTestCapState() == FCAP_CLOSED`, `FLAG_ALPHA` clear,
   the `4 4` line present as a committed source step (the incremental
   re-commit guarantees it), and `getFreeRamMemory() == freeBase` (the
   capture added ZERO residue on top of the restore's own footprint).
   Phase 2 (suspended state): reopen capture, type `5`, enter
   `tamEnterMode(ITM_STO)` (suspends — buffer already freed).  Drive the
   F1-5 validate-restored idiom directly (the restore-validation path
   the reset seam hooks — reuse the F1-5 validator tests' drive,
   targeted read).  Require `forthTestCapState() == FCAP_CLOSED` while
   `tam.mode != 0`; then cancel TAM (`fnKeyExit(NOPARAM)`) and require
   `tam.mode == 0`, no crash, plain PEM (the resume hook is a no-op —
   nothing is suspended).  PASS:
   `[4] PASS: restore lifecycle closes open and suspended captures leak-free`
5. **Cap round-trip.**  Fresh line; fixture loop types 98 repetitions
   of `X` then space (196 glyphs of valid one-glyph words — a single
   196-glyph token could trip the E9 structural tier); ENTER; close the
   empty relock line (`fnKeyExit`); reopen via `pemAlphaEdit(0)` on the
   committed step (the F6-1 subcase-6 idiom); require the reopened text
   byte-equal to the committed payload and glyph length 196.  PASS:
   `[5] PASS: 196-glyph line round-trips commit and reopen`
6. **Arena sweep.**  Three full cycles of open → type `1` → suspend
   (`tamEnterMode(ITM_STO)`) → cancel-resume (`fnKeyExit`) →
   BACKSPACE-abort (empty the line first); then require
   `getFreeRamMemory()` equality vs pre-cycle and
   `forthTestCapState() == FCAP_CLOSED`.  Escape valve: if the only
   delta matches one program-memory resize quantum, STOP and report
   (program-region growth, not a capture leak).  PASS:
   `[6] PASS: capture cycles leave zero arena residue`

### Existing tests

All stay green untouched.  This battery ADDS pins; it replaces nothing.

### Non-goals / STOP boundaries

- No hardware assumptions: everything runs on the PC-build self-test.
  The DM42n bench (charter Blocks A-F) happens AFTER this packet lands
  and is not Qwen work.
- No new capture behavior beyond the lifecycle reset; if a subcase
  cannot pass without a product change this packet does not name, STOP
  and report the defect.

### Gate and required mutations

Full gate green first (six PASS lines + every legacy banner).  One
mutation (acceptance packets pin; the glue gets the mutation), log
`/tmp/forth-f6-6-mut1.log`:

1. Delete BOTH added `forthCapPowerReset();` seam calls.  Subcase 4
   MUST go RED (phase 1: capture not CLOSED after the restore round-trip
   or `getFreeRamMemory() != freeBase`; phase 2: state not CLOSED under
   TAM).  Restore; post-restore gate green.

Report: six PASS lines, both banners, exit 0, arena line vs baseline
(this stage's line is the F6 closeout arena record), `git diff --check`,
generated-mirror equality, the mutation RED, and the enclosing function
names of the two seam sites.  RULE-1: record the measured
`make dmcp5r47` flash delta for the WHOLE F6 series in this stage
commit.

### Commit

```text
forth-core: F6-6 — capture acceptance battery
```

---

## FIXTURE RULES carried forward from the F4-2/F4-3 debug (binding, 2026-07-19)

Each of these cost a red gate in stage F4. They apply to every test this
packet adds, whether or not the body above repeats them.

1. **Seeded stacks and `x_set_string` are incompatible.** `x_set_string`
   overwrites REGISTER_X with the source string and `fnForthOuter` drops
   it, shifting anything pushed beforehand one level down. A fixture that
   needs values on the stack runs its source through
   `forthOuterInterpret(...)`; `x_set_string` + `fnForthOuter` is for
   compile-only fixtures that need no seeded stack.
2. **`forthFindColon` returns a REF INDEX, not a byte offset.** Byte-image
   assertions walk from
   `fdict.latest + TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK`. Using the ref
   as an offset happens to work for the first word defined after a clear
   (both are 0) and silently corrupts every later image.
3. **Every subcase opens with `lastErrorCode = ERROR_NONE;`.** Neighbouring
   tests deliberately end on an error code; an unset read reports a phantom
   failure that looks like a code defect.
4. **Anything a fixture allocates, the fixture frees** — named variables in
   particular (data block per variable plus the header table, back to the
   pre-test count; see `test_param_named_indirect`'s cleanup). The suite's
   end-of-run `numberOfAllocatedMemoryRegions` gate reddens on a leak, and
   assigning that counter to hide the growth is a packet violation.
5. **`compareString` is a C-string comparison returning 0 on equal.** A
   truthy test (`if (compareString(a, b, CMP_BINARY))`) reads as "not
   equal"; and any buffer you hand it must be NUL-terminated.

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

**Working-area files (added 2026-07-19).** Every upstream file this packet
names is already materialized in `packages/forth-core/` — edit it in place.
Never `cp` a file out of `src/c47/`: patches are diffed against upstream **at
the package's recorded base commit**, so a copy from the current tree bakes
any post-base upstream change into your patch, and `refresh` does not detect
that drift. If a file this packet names is missing from the working area,
STOP and report a packet defect rather than materializing or copying on your
own initiative. Brand-new files (no upstream counterpart) are the exception:
create them in the working area and `refresh` classifies them into `files/`.
