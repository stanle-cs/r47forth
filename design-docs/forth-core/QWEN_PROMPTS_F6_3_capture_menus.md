# Stage F6-3 — catalogs and menus during capture

Origin: DESIGN §10.6 via `QWEN_PROMPTS_F6_core.md` §1/§2 decision 9 and
the T3/T4/T7 traces of `F6_AUDIT_RESULTS.md`.  Today a catalog pick of a
function item during Forth capture is INERT: `insertStepInProgram`'s
alpha arm routes every capture-mode key into `pemAlpha`, where a
non-glyph item falls through the arms to the re-commit tail and changes
nothing.  This packet gives capture the PEM-shaped behavior §10.6
promises: picking a callable item inserts its CATALOG NAME as text at
the cursor (the same name-faithful discipline as the landed §9.6 picker
and the F15-5 tam hook), and the EXIT/menu ladder around the picker is
pinned with capture-object assertions.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F6-2 commit
   `forth-core: F6-2 — TAM suspend/resume keeps capture alive`.
2. `grep -n "bool_t pickerInsertName" packages/forth-core/keyboard.c`
   matches, and its body reads the capture buffer (F6-1 Change D landed:
   grep `forthCapBuf` inside it).
3. `grep -rn "forthCapInsertName" packages/forth-core` → ZERO matches
   (this packet adds it).
4. `grep -n "isAlphaSubmenu" packages/forth-core/softmenus.c` shows
   `-MNU_FORTH` inside the disjunction (the landed ALPHA-submenu
   classification).
5. `grep -n "case MNU_FORTH" packages/forth-core/softmenus.c` matches
   (the landed picker builder — UNTOUCHED by this packet; F6-5 replaces
   its content source).
6. Pre-gate green; arena baseline from the F6-2 commit message.

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
   `/tmp/forth-f6-3-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-3-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `manage.c`,
   `keyboard.c`, `ui/tam.c`, `softmenus.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f6-3-todo.md`,
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

## F6-3 — a catalog pick is a text insertion

### Authority carried by this packet (no open choices)

1. **Shared inserter** (keyboard.c): factor the landed `pickerInsertName`
   body into

   ```c
   /* Insert name + one trailing space into the open capture line at
    * T_cursorPos.  Same 256-byte/196-glyph cap as typing; false = no room
    * or capture not open.  §9.6 P-H7 discipline, generalized (F6-3). */
   bool_t forthCapInsertName(const char *name)
   ```

   holding the current cap/xcopy/cursor logic verbatim (parameterized on
   `name`), and `pickerInsertName` becomes exactly
   `return forthCapInsertName(dynmenuGetLabel(dynamicMenuItem));`.
   Prototype for `forthCapInsertName` goes in `forth_capture.h` beside
   the other capture entry points (implementation stays in keyboard.c —
   it owns the cap constants' original site).
2. **The item arm** (programming/manage.c, `pemAlpha`): insert a new arm
   AFTER the `fnT_ARROW` arm (the last special-item arm before the
   fall-through tail) and BEFORE the tail's `int16_t aimFunc = ...`
   re-read:

   ```c
   else if(forthCapIsOpen()
           && (indexOfItems[item].status & CAT_STATUS) == CAT_FNCT
           && (indexOfItems[item].status & PTP_STATUS) == PTP_NONE
           && item != ITM_AIM && item != ITM_FORTH) {
     (void)forthCapInsertName(indexOfItems[item].itemCatalogName);
     /* falls through to the re-commit tail: the step tracks the insert */
   }
   ```

   Rationale fixed by this packet: `CAT_FNCT` is the same callable-item
   class the §4.2 XEQ fallback matches — a name the catalog can insert is
   a name the interpreter can resolve — and `PTP_NONE` restricts the arm
   to PARAMETERLESS items (SIN is `CAT_FNCT | PTP_NONE`, traced
   items.c:1879).  PARAMETERIZED items (PTP classes) stay INERT in this
   arm on purpose: their capture UX is the TAM-shaped path — a
   catalog-initiated `tamEnterMode` already suspends via F6-2, and F6-4
   converts the commit to canonical text; inserting a bare `STO ` here
   would create a second, conflicting entry UX for the same item.  B3's
   atomic reject at RUN time is F3-6's landed contract and unaffected.
   The two explicit exclusions keep the AIM gesture and the FORTH toggle
   out of the sink (ITM_FORTH is `PTP_REM`, so it is doubly excluded).
   Items whose `func == addItemToBuffer` never reach this arm (the glyph
   arm above it wins).
3. **No EXIT/menu code changes.**  The ladder is already correct
   (`isAlphaSubmenu` includes `-MNU_FORTH`; `fnKeyExit` arms are F6-1
   D2-aware).  This packet PINS it (tests 3-5).
4. The landed picker builder (`case MNU_FORTH` in softmenus.c) and
   `forthPickerGuard` are untouched.

### Files

Modify only: `forth_capture.h`, `keyboard.c`, `programming/manage.c`,
`test_dict_reloc.c` — all under `packages/forth-core/`.

### Targeted reads

1. keyboard.c: `pickerInsertName` and 10 lines around it.
2. manage.c: `pemAlpha`'s special-item arms — grep `fnT_ARROW` and read
   that arm plus 15 lines after it (the insertion point and the tail's
   first lines).
3. test_dict_reloc.c: the two landed picker tests (fixture idiom for
   populating `-MNU_FORTH` and `dynamicMenuItem`); the F6-1/F6-2 drive
   idioms; registration lines.

### Change A/B — as Authority items 1-2 (exact bodies above)

### Change C — new test `test_capture_menus` (register after the newest test)

Fixture: `tpLbl("F63")` + `tpMarker`, `tpWrite`; open capture per the
drive contract.

1. **Item pick inserts its name.**  With text `1 ` typed and cursor at
   end, drive `runFunction(ITM_SIN)` (routes through the alpha arm into
   `pemAlpha`).  Require `forthTestCapText()` == `"1 SIN "` and the
   step image payload byte-equal to the same text (re-commit tracked).
   (`printf '%s' "1 SIN " | wc -c` = 6.)  PASS:
   `[1] PASS: catalog item inserts its catalog name as text`
2. **Glyph items still type.**  Drive `runFunction(ITM_2)`; require text
   `"1 SIN 2"` — the glyph arm won, not the item arm.  PASS:
   `[2] PASS: glyph keys unaffected by the item arm`
3. **Picker pops to ALPHA, capture survives.**  Populate the `-MNU_FORTH`
   picker exactly as the landed picker tests do (softmenu fixture +
   `initVariableSoftmenu` idiom), push it on the stack; call
   `fnKeyExit(NOPARAM)` once (the EXIT idiom for every subcase here).  Require: top menu is no longer `-MNU_FORTH`; capture
   OPEN; text unchanged.  PASS:
   `[3] PASS: EXIT pops the picker back toward ALPHA`
4. **Picker navigation leaks nothing.**  Re-push the picker; drive the
   menu page-down/page-up keys (the landed navigation idiom from the
   picker tests, if one exists — otherwise a second `fnKeyExit(NOPARAM)`).
   Require text and `T_cursorPos` unchanged.  PASS:
   `[4] PASS: picker navigation leaves the capture line intact`
5. **EXIT ladder end.**  With the menu stack back at the ALPHA level and
   text still present, one `fnKeyExit(NOPARAM)` commits-and-closes (the
   D2 arm — capture CLOSED, line committed); with a FRESH empty line,
   one `fnKeyExit(NOPARAM)` aborts (placeholder gone).  PASS:
   `[5] PASS: EXIT ladder ends in commit-with-text / abort-when-empty`
6. **Insert-at-cursor discipline.**  Fresh line, type `AB`, set
   `T_cursorPos = 0` (fixture-owned), drive `runFunction(ITM_SIN)`.
   Require text `"SIN AB"` and `T_cursorPos == 4`.  PASS:
   `[6] PASS: item insert honors the cursor position`

Cleanup: close capture (BACKSPACE/EXIT idiom), restore softmenu stack
and mode globals per the landed picker tests' restore blocks,
`lastErrorCode = ERROR_NONE`, `cleanupTestProgram()`.

### Existing tests

All stay green untouched — the item arm sits BEFORE the previously-inert
fall-through, and non-capture behavior (`forthCapIsOpen()` false) is
unchanged by construction.  The two landed picker tests and
`test_capture_buffer`/`test_capture_suspend` are the canaries.

### Non-goals / STOP boundaries

- No TAM interplay change (F6-4 owns parameterized-entry UX).
- No picker CONTENT change (F6-5 owns the dictionary-backed catalog).
- No new EXIT semantics — pins only.

### Gate and required mutations

Full gate green first (six PASS lines + every legacy banner).
Mutations, each separately, logs `/tmp/forth-f6-3-mut1..3.log`; co-reds
beyond the named RED are expected; post-restore gate green between
mutations.

1. Delete the new item arm in `pemAlpha`.  Subcase 1 MUST go RED (the
   pick is inert again — text unchanged).
2. In the item arm, replace `itemCatalogName` with `itemSoftmenuName`.
   Subcase 1 MUST go RED if the two spellings differ for SIN; if this
   mutation stays GREEN, STOP and report — the F15-4/F15-5 capture-store
   precedent (softmenu vs catalog name) must be re-checked for SIN and
   the packet's mutation replaced by the architect, never silently
   accepted.
3. In `forthCapInsertName`, drop the trailing-space append.  Subcase 1
   MUST go RED (`"1 SIN"` without the separator).

Report: six PASS lines, both banners, exit 0, arena line vs baseline,
`git diff --check`, generated-mirror equality, mutation REDs (with the
mutation-2 escape valve outcome).  RULE-1: negligible flash delta
expected; note the measured delta.

### Commit

```text
forth-core: F6-3 — catalogs and menus during capture
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

**Working-area files (added 2026-07-19).** Every upstream file this packet
names is already materialized in `packages/forth-core/` — edit it in place.
Never `cp` a file out of `src/c47/`: patches are diffed against upstream **at
the package's recorded base commit**, so a copy from the current tree bakes
any post-base upstream change into your patch, and `refresh` does not detect
that drift. If a file this packet names is missing from the working area,
STOP and report a packet defect rather than materializing or copying on your
own initiative. Brand-new files (no upstream counterpart) are the exception:
create them in the working area and `refresh` classifies them into `files/`.
