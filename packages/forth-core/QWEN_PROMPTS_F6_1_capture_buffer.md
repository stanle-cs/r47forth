# Stage F6-1 — the capture object: Forth capture text moves off aimBuffer

Origin: DESIGN §10.6 via the F6 design pass (`QWEN_PROMPTS_F6_core.md` §2,
decisions 1-5, 10) and the trace evidence of `F6_AUDIT_RESULTS.md` (T1,
T2, T6).  This packet introduces `forth_capture.c/h` — a capture-state
object holding the Forth capture line in a managed, fixed-size allocation
held only while capture is open — and re-points every Forth-capture
read/write of `aimBuffer` onto it.  REM/LITERAL captures, NIM number
entry, TAM name entry, and CM_AIM proper stay on `aimBuffer` untouched.
The per-key incremental step re-commit is preserved bit-for-bit; display
reads step bytes and is untouched (F15-3 is the canary).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F5-2 commit
   `forth-core: F5-2 — E9 commit gate: structural rejects, advisory
   commits`.
2. `grep -n "forthCheckSourceLine(aimBuffer)" packages/forth-core/programming/manage.c`
   → exactly ONE match, inside `pemAlpha`'s `ITM_ENTER` arm (the F5-2
   gate line this packet re-points).
3. `grep -rn "forth_capture\|forthCapOpen\|FCAP_OPEN" packages/forth-core`
   → ZERO matches.
4. Inventory (rule 2): `grep -c aimBuffer packages/forth-core/programming/manage.c`
   → expected 118 (117 on the F3-2 authoring tree + 1 from F5-2's gate
   line).  `grep -c aimBuffer packages/forth-core/keyboard.c` → 77.
   `grep -c aimBuffer packages/forth-core/ui/tam.c` → 23.  Any other
   value: STOP (architect re-verification required — a predecessor
   landed with deviations).
5. `grep -n "bool_t pickerInsertName" packages/forth-core/keyboard.c`
   matches; `grep -n 'strcmp(aimBuffer, "SQ ")' packages/forth-core/test_dict_reloc.c`
   and `grep -n 'strcmp(aimBuffer, "SQ DUP ")' packages/forth-core/test_dict_reloc.c`
   each match exactly ONCE — those two asserts identify the two picker
   tests this packet flips (Change G; the tests' names are whatever the
   tree says — do not expect a particular name).
6. Pre-gate green; arena baseline from the F5-2 commit message.

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
   `/tmp/forth-f6-1-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-1-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f6-1-todo.md`,
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
F15-2 subcase-1 drive block idiom verbatim (targeted read below); never
invent a new drive.

---

## F6-1 — the capture line lives in managed memory

### Authority carried by this packet (no open choices)

1. **New files** `forth_capture.h` / `forth_capture.c` (package-new, flat
   working area).  `forth_capture.h`:

   ```c
   #ifndef FORTH_CAPTURE_H
   #define FORTH_CAPTURE_H

   #include "c47.h"

   #define FORTH_CAP_BYTES 256   /* same 256-byte/196-glyph contract as the
                                    landed aimBuffer capture (manage.c cap) */

   typedef enum { FCAP_CLOSED = 0, FCAP_OPEN = 1, FCAP_SUSPENDED = 2 } forthCapState_t;

   typedef struct {
     uint8_t     state;          /* forthCapState_t */
     uint8_t    *buf;            /* allocC47Blocks'd; NULL unless FCAP_OPEN */
     uint16_t    sizeBlocks;     /* TO_BLOCKS(FORTH_CAP_BYTES) while allocated */
     /* Suspend snapshot — dormant this stage, wired by F6-2: */
     uint16_t    savedCursor;    /* T_cursorPos at suspend */
     uint16_t    savedLocalStep; /* currentLocalStepNumber at suspend */
     uint32_t    savedStepOffset;/* capture step vs beginOfProgramMemory
                                    (offset: program memory may relocate) */
   } forthCap_t;

   void        forthCapOpen(void);       /* alloc+zero; on alloc failure:
                                            displays ERROR_RAM_FULL, state
                                            stays FCAP_CLOSED */
   void        forthCapClose(void);      /* free; state FCAP_CLOSED; safe if
                                            already closed */
   bool_t      forthCapIsOpen(void);     /* state == FCAP_OPEN */
   uint8_t    *forthCapBuf(void);        /* NULL unless FCAP_OPEN */
   bool_t      forthCapTextNonEmpty(void); /* open && buf[0] != 0 */

   #if defined(FORTH_DEBUG_SELFTEST)
   uint8_t     forthTestCapState(void);
   const char *forthTestCapText(void);   /* "" when not open */
   #endif

   #endif
   ```

2. `forth_capture.c` implements exactly:

   ```c
   #include "forth_capture.h"

   static forthCap_t forthCap;   /* zero-initialized: FCAP_CLOSED */

   void forthCapOpen(void) {
     if (forthCap.state == FCAP_OPEN) {
       forthCap.buf[0] = 0;                    /* reopen = fresh line */
       return;
     }
     forthCap.sizeBlocks = TO_BLOCKS(FORTH_CAP_BYTES);
     forthCap.buf = allocC47Blocks(forthCap.sizeBlocks);
     if (forthCap.buf == NULL) {
       forthCap.sizeBlocks = 0;
       displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return;
     }
     forthCap.buf[0] = 0;
     forthCap.state = FCAP_OPEN;
   }

   void forthCapClose(void) {
     if (forthCap.buf != NULL) {
       freeC47Blocks(forthCap.buf, forthCap.sizeBlocks);
       forthCap.buf = NULL;
     }
     forthCap.sizeBlocks = 0;
     forthCap.state = FCAP_CLOSED;
   }

   bool_t forthCapIsOpen(void)  { return forthCap.state == FCAP_OPEN; }
   uint8_t *forthCapBuf(void)   { return forthCap.state == FCAP_OPEN ? forthCap.buf : NULL; }
   bool_t forthCapTextNonEmpty(void) {
     return forthCap.state == FCAP_OPEN && forthCap.buf[0] != 0;
   }

   #if defined(FORTH_DEBUG_SELFTEST)
   uint8_t forthTestCapState(void) { return forthCap.state; }
   const char *forthTestCapText(void) {
     return forthCap.state == FCAP_OPEN ? (const char *)forthCap.buf : "";
   }
   #endif
   ```

   If `freeC47Blocks` is not the free counterpart used by the dictionary
   (check forth_dict.c's realloc/free discipline in the targeted read), use
   the counterpart the dictionary uses and report the substitution.
3. **Routing predicate**: every edit below keys on `forthCapIsOpen()` —
   never on `tam.function` conjunctions — so F6-2 can key suspension on
   the state alone.
4. **The incremental re-commit is untouched in WHEN it runs**; only the
   source of the payload bytes changes (Change D).
5. No behavior change for REM/LITERAL/NIM/TAM/CM_AIM `aimBuffer` users.
   The inventory delta at the end of the task must equal the sites this
   packet names — nothing else.

### Files

Modify only: `forth_capture.h` (new), `forth_capture.c` (new),
`programming/manage.c`, `keyboard.c`, `ui/tam.c`, `test_dict_reloc.c` —
all under `packages/forth-core/`.  (No screen.c change: no screen.c site
reads aimBuffer during PEM capture — display renders the committed step.)

### Targeted reads

1. manage.c: `pemAlpha` in slices — the `ITM_EDIT` FORTH sub-arm (grep
   `// FORTH` / `aimFunc == ITM_FORTH`), the open-capture block (grep
   `showSoftmenu(-MNU_ALPHA)`), the glyph arm (grep
   `func == addItemToBuffer`), the `ITM_BACKSPACE` and `ITM_ENTER` arms,
   the fall-through re-commit tail (grep `// rem or 42str`), and
   `pemCloseAlphaInput` in full (small).
2. forth_dict.c: the alloc/realloc/free discipline (grep `allocC47Blocks`).
3. keyboard.c: `pickerInsertName` (top of file) only.
4. ui/tam.c: `tamEnterMode`'s `calcMode == CM_PEM` arms only (grep
   `pemCloseAlphaInput`).
5. test_dict_reloc.c: the F15-2 subcase-1 capture-open drive block; the
   two picker tests located by the Change G asserts; tp helpers;
   registration lines after the newest test.

### Change A/B — the new module (quoted in full above)

Add both files exactly as §Authority items 1-2.  Add
`#include "forth_capture.h"` where each edited file includes forth
headers today (grep the existing `forth_dict.h` include in each).

### Change C — manage.c pemAlpha FORTH sites

Each site quotes the current line(s) (rule 3); STOP if a quote is absent.

C1. EDIT FORTH sub-arm — current:

```c
      xcopy(aimBuffer, tmpString, ll);   // bare render: no name prefix, no quotes
      aimBuffer[ll] = 0;
      T_cursorPos = stringLastGlyph(aimBuffer) + 1;
```

becomes:

```c
      forthCapOpen();
      if(!forthCapIsOpen()) { aimBuffer[0] = 0; return; }   /* RAM_FULL shown */
      xcopy(forthCapBuf(), tmpString, ll);   // bare render: no name prefix, no quotes
      forthCapBuf()[ll] = 0;
      T_cursorPos = stringLastGlyph((char *)forthCapBuf()) + 1;
```

C2. Open-capture block — at the TOP of the `if(!getSystemFlag(FLAG_ALPHA))`
block (current first line `resetShiftState();`), insert BEFORE it:

```c
      if(tam.function == ITM_FORTH && !forthCapIsOpen()) {
        forthCapOpen();
        if(!forthCapIsOpen()) { return; }   /* RAM_FULL shown; nothing mutated */
      }
```

C3. Glyph arm — current cap/insert pair:

```c
        if(len < (256 - inputCharLength) && stringGlyphLength(aimBuffer) < 196) {
          xcopy(aimBuffer + T_cursorPos + inputCharLength, aimBuffer + T_cursorPos, stringByteLength(aimBuffer + T_cursorPos) + 1);
          xcopy(aimBuffer + T_cursorPos, indexOfItems[item].itemSoftmenuName, inputCharLength);
          T_cursorPos += inputCharLength;
        }
```

becomes (`sink` declared at the top of the arm beside the existing `len`
local, which must also read the sink):

```c
        char *sink = forthCapIsOpen() ? (char *)forthCapBuf() : aimBuffer;
        /* len local above: int32_t len = stringByteLength(sink); */
        if(len < (256 - inputCharLength) && stringGlyphLength(sink) < 196) {
          xcopy(sink + T_cursorPos + inputCharLength, sink + T_cursorPos, stringByteLength(sink + T_cursorPos) + 1);
          xcopy(sink + T_cursorPos, indexOfItems[item].itemSoftmenuName, inputCharLength);
          T_cursorPos += inputCharLength;
        }
```

(The arm's existing `int32_t len = stringByteLength(aimBuffer);` line is
re-pointed to `sink`; declare `sink` above it.)

C4. BACKSPACE arm — the empty test `if(aimBuffer[0] == 0) {` becomes
`if((forthCapIsOpen() ? forthCapBuf()[0] : aimBuffer[0]) == 0) {`, and
inside that branch add `forthCapClose();` immediately before the existing
`tam.function = 0;`.  The glyph-delete branch's five buffer lines
(`char cursorByte = aimBuffer[T_cursorPos];` …
`T_cursorPos = lastGlyphPos;`) re-point through a `sink` local declared
the same way as C3.

C5. ENTER arm — current:

```c
      bool_t hadText  = (aimBuffer[0] != 0);           // E5 locks on a NON-EMPTY line
```

becomes:

```c
      bool_t hadText  = (forthCapIsOpen() ? forthCapTextNonEmpty()
                                          : (aimBuffer[0] != 0));  // E5 locks on a NON-EMPTY line
```

and the F5-2 gate line `!forthCheckSourceLine(aimBuffer)` becomes
`!forthCheckSourceLine(forthCapIsOpen() ? (const char *)forthCapBuf() : aimBuffer)`.

C6. Fall-through re-commit — every `aimBuffer` read in the tail's two
encode branches (each branch's `stringByteLength`/`xcopy`/
`_insertInProgram` triplet — the rem-42str branch starting
`tmpString[3] = stringByteLength(aimBuffer);` and the literal branch's
equivalents) re-points through a `sink` local
(`char *sink = forthCapIsOpen() ? (char *)forthCapBuf() : aimBuffer;`)
declared once before the `deleteStepsFromTo` call of that tail.  BOTH
branches (literal and rem-42str) use `sink` — a LITERAL/REM capture has
`forthCapIsOpen()` false, so their behavior is unchanged by construction.

C7. `pemCloseAlphaInput` — current opening condition
`if(tam.function == ITM_FORTH && aimBuffer[0] == 0) {` becomes
`if(tam.function == ITM_FORTH && !forthCapTextNonEmpty()) {`; add
`forthCapClose();` in that branch immediately before its
`tam.function = 0;`, and in the commit path add `forthCapClose();`
immediately after the existing `aimBuffer[0] = 0;` line.
(The multi-line ENTER relock then re-allocs via C2's open call — uniform
alloc-on-open/free-on-close; no carve-out.)

### Change D — keyboard.c pickerInsertName

Re-point the whole body onto the capture buffer and refuse when closed —
current body lines quoting `aimBuffer` (six occurrences) become the same
expressions over `cap`, with:

```c
  uint8_t *cap = forthCapBuf();
  if(cap == NULL) { return false; }
```

inserted after the `pickName`/`nameLen` locals.  (`forthPickerGuard`
already restricts the product path to open capture; the guard makes the
direct-call contract explicit.)

### Change D2 — keyboard.c fnKeyExit CM_PEM sink reads

The EXIT ladder distinguishes empty/non-empty capture by `aimBuffer[0]`;
both reads must follow the sink or EXIT-with-text regresses from
commit-and-close to a one-glyph delete.  Current lines (quote-verify):

```c
        if(getSystemFlag(FLAG_ALPHA) && aimBuffer[0] == 0 && !tam.mode) {
```

becomes

```c
        if(getSystemFlag(FLAG_ALPHA)
           && (forthCapIsOpen() ? !forthCapTextNonEmpty() : aimBuffer[0] == 0)
           && !tam.mode) {
```

and

```c
        if(aimBuffer[0] != 0 && !tam.mode) {
```

becomes

```c
        if((aimBuffer[0] != 0 || forthCapTextNonEmpty()) && !tam.mode) {
```

The bodies are untouched: the first arm's `pemAlpha(ITM_BACKSPACE)` and
the second arm's `pemCloseAlphaInput()` are already cap-aware via C4/C7;
the second arm's `aimBuffer[0] = 0;` line stays (harmless).

### Change E — ui/tam.c interim guard (F6-2 replaces this exact line)

Current:

```c
    else if(calcMode == CM_PEM && aimBuffer[0] != 0) {
```

becomes:

```c
    else if(calcMode == CM_PEM && (aimBuffer[0] != 0 || forthCapTextNonEmpty())) {
```

Nothing else in the arm changes: `pemCloseAlphaInput()` (now cap-aware via
C7) commits-and-closes exactly as the landed behavior did.  The
empty-capture-line TAM edge (landed: TAM opens over the open capture)
stays byte-identical by construction — `forthCapTextNonEmpty()` is false
then, mirroring the landed `aimBuffer[0] != 0`.

### Change F — new test `test_capture_buffer` (register after the newest test)

Fixture: one program, `tpLbl("F61")` + `tpMarker` (opening »FORTH),
`tpWrite`; open capture per the CAPTURE-DRIVE CONTRACT (cursor on the
marker, `runFunction(ITM_AIM)` first — reuse the F15-2 idiom).  Idioms
fixed for every subcase: the EXIT idiom is a direct `fnKeyExit(NOPARAM)`
call; the TAM idiom is a direct `tamEnterMode(...)` call (the landed TAM
tests call `tamEnterMode(ITM_XEQ)` directly).  Any subcase that needs an
open capture and follows one that closed it REOPENS per the drive
contract and notes the reopen in the todo.  Subcases:

1. **Open state.**  After the open drive: `forthTestCapState() == FCAP_OPEN`
   (use the enum via the header), `getSystemFlag(FLAG_ALPHA)` set, and
   `aimBuffer[0] == 0`.  PASS:
   `[1] PASS: capture opens with a managed buffer and an empty aimBuffer`
2. **Typing lands in the buffer and the step, never aimBuffer.**  Type
   `1 2 +` via the landed key idiom.  Require `strcmp(forthTestCapText(), "1 2 +") == 0`;
   `aimBuffer[0] == 0` STILL; and the current step's byte image equals
   `0x8B 0x1A 0xFD 0x05 '1' ' ' '2' ' ' '+'` (read at `currentStep`,
   an encoding assertion).  (`printf '%s' "1 2 +" | wc -c` = 5.)  PASS:
   `[2] PASS: sink is the managed buffer; step re-commits per key`
3. **ENTER multi-line relock.**  Press ENTER via the idiom.  Require: no
   error; `findPreviousStep(currentStep)` is an ITM_FORTH source step
   whose payload is `1 2 +` (len 5 — the committed line now precedes the
   fresh placeholder); capture is OPEN again
   (`forthTestCapState() == FCAP_OPEN`) with
   `forthTestCapText()[0] == 0` (fresh line).  PASS:
   `[3] PASS: ENTER commits and relocks a fresh managed line`
4. **Backspace + mid-line edit.**  Type `AB`, one BACKSPACE, then `C`.
   Require `forthTestCapText()` == `"AC"` and the step image payload
   `'A' 'C'` with length 2.  PASS:
   `[4] PASS: glyph edits operate on the managed buffer`
5. **Empty-line BACKSPACE closes and frees.**  Close any open line
   first (EXIT idiom), record `freeBefore5 = getFreeRamMemory();`, then
   REOPEN a fresh capture line (drive contract), type nothing, and press
   BACKSPACE once (empty-line abort).  Require: capture CLOSED
   (`forthTestCapState() == FCAP_CLOSED`), `FLAG_ALPHA` clear, placeholder
   gone (step count back to pre-reopen), and
   `getFreeRamMemory() == freeBefore5` (buffer freed, zero residue —
   the placeholder insert+delete nets zero program bytes).  Escape
   valve: if the only delta matches one program-memory resize quantum,
   STOP and report (program-region growth, not a capture leak).  PASS:
   `[5] PASS: abort closes capture and frees the buffer`
6. **EDIT reopen refills from the step.**  Fresh line: type `AB CD`,
   ENTER (commit + relock), then EXIT idiom on the empty relock line
   (aborts it; capture closed).  Position the cursor ON the committed
   `AB CD` source step (the fnGotoDot/step-walk idiom of the
   neighboring F15 tests), fixture-own `calcMode = CM_PEM`,
   `tam.mode = 0`, FLAG_ALPHA clear, then call `pemAlphaEdit(0)`
   directly (its own guards require exactly that state).  Require
   capture OPEN, `forthTestCapText()` == `"AB CD"` (`printf '%s' "AB CD"
   | wc -c` = 5), and cursor at end
   (`T_cursorPos == stringLastGlyph(forthTestCapText()) + 1`).  Close
   via the empty-abort idiom (BACKSPACE to empty, then once more).
   PASS: `[6] PASS: EDIT refills the managed buffer from the step`
7. **Interim TAM guard preserves commit-and-close.**  With capture open
   and text `9` typed: call `tamEnterMode(ITM_STO)` directly (the landed
   TAM-test idiom).  Require: capture CLOSED, `tam.mode != 0` (TAM
   open), the `9` line committed as a source step.  Cancel TAM
   (`fnKeyExit(NOPARAM)`, repeated until `tam.mode == 0` if the first
   press only pops a TAM menu) and require plain PEM.  PASS:
   `[7] PASS: TAM entry commits-and-closes the capture (F6-1 interim)`
8. **196-glyph cap.**  In a fixture loop type 98 repetitions of `X`
   then space (alternating keys; 98 × 2 = exactly 196 glyphs of valid
   one-glyph words — a single 196-glyph token could trip the E9
   structural tier, which is not what this subcase pins); require the
   197th press (one more `X`) is ignored
   (`stringGlyphLength(forthTestCapText()) == 196` after it), then ENTER
   commits without error.  PASS:
   `[8] PASS: 196-glyph cap holds on the managed buffer`
9. **E9 composition.**  In a fresh capture line type `: A IF ;` (the F5-1
   tier-1 malformed row), press ENTER.  Require: refusal (error per the
   landed F5-2 contract), capture still OPEN, and
   `strcmp(forthTestCapText(), ": A IF ;") == 0` (line intact for
   correction).  Clear the error; BACKSPACE-abort to clean up.  PASS:
   `[9] PASS: E9 refusal leaves the managed line intact`

10. **EXIT with text commits and closes (ladder parity).**  Record
    `freeBefore10 = getFreeRamMemory();`, reopen a fresh capture line,
    type `7`, call the EXIT idiom once.  Require: capture CLOSED,
    `FLAG_ALPHA` clear, the `7` line committed as a source step (payload
    `'7'`, length 1), and `getFreeRamMemory() == freeBefore10` minus
    nothing (equality — the commit-close freed the buffer; same resize
    escape valve as subcase 5).  PASS:
    `[10] PASS: EXIT with text commits and closes (landed ladder)`
11. **EXIT on empty aborts.**  Fresh capture line (empty), EXIT once.
    Require: capture CLOSED, placeholder deleted (step count restored).
    PASS: `[11] PASS: EXIT on empty aborts the placeholder`

Cleanup: capture closed, `forthCapClose()` idempotent call, restore
`programRunStop`/mode globals per the F15 fixture discipline,
`lastErrorCode = ERROR_NONE`, `cleanupTestProgram()`.

### Change G — named legacy flips (rule 6 authorization)

1. **The two picker tests containing the quoted asserts** (locate by
   grep: `strcmp(aimBuffer, "SQ ")` and `strcmp(aimBuffer, "SQ DUP ")`;
   the enclosing test names are whatever the tree says) currently seed
   and assert `aimBuffer` around a direct `pickerInsertName()` call.  Rework each fixture to open a
   REAL capture first (CAPTURE-DRIVE CONTRACT; the fixture program each
   test already builds gains an opening `tpMarker` if it lacks one), type
   the pre-seed text (`DUP ` for the second test) via the key idiom, set
   `T_cursorPos = 0` where the test previously seeded it, keep the direct
   `pickerInsertName()` call, and assert `forthTestCapText()` equals the
   same expected strings (`"SQ "` / `"SQ DUP "`) with the same
   `T_cursorPos == 3` checks.  Close the capture in cleanup
   (BACKSPACE-abort idiom or `forthCapClose()` + flag restore per the
   test's existing restore block).
2. **`test_commit_gate`** (F5-2): every assertion reading `aimBuffer`
   inside it (byte-equality of the typed line and any emptiness check)
   re-points to `forthTestCapText()` with identical expected values.  No
   other logic changes.

If ANY other test reddens by reading `aimBuffer` in a Forth-capture
context, STOP and report — flips beyond these two classes are not
authorized.

### Existing tests

F15-1..F15-5, the F5 suites, and every earlier banner stay green.  The
display-parity suite (F15-3) is the explicit canary that the buffer move
did not touch rendering.

### Non-goals / STOP boundaries

- No suspend/resume: the tam.c edit is the INTERIM guard only (F6-2).
- No catalog/menu changes (F6-3/F6-5); no parameter-entry text (F6-4).
- The dormant snapshot fields stay unwritten (F6-2 wires them).
- aimBuffer users outside the named sites are untouched — the closing
  inventory (below) is the proof.

### Gate and required mutations

Full gate green first (all eleven PASS lines + every legacy banner + the
two reworked picker tests + `test_commit_gate` green).  Closing
inventory (report the numbers): `grep -c aimBuffer` on manage.c,
keyboard.c, ui/tam.c — record the deltas vs the EXECUTION GATE values and
list the sites; they must be exactly this packet's edits.

Mutations, each separately, logs `/tmp/forth-f6-1-mut1..5.log`; co-reds
beyond the named RED are expected mutation fallout (restore, continue);
the post-restore gate must be green before the next mutation.

1. In C3, revert the two `xcopy(sink + ...)` lines to `aimBuffer + ...`
   (keep the `sink` declaration).  Subcase 2 MUST go RED (step image
   empty or aimBuffer sentinel violated).
2. Delete BOTH added `forthCapClose();` calls (C4's empty-abort branch
   and C7's two paths) as ONE mutation.  Subcase 5 MUST go RED
   (free-RAM inequality: the abort leaks the buffer); subcase 10 co-reds
   on its own equality (the commit-close leak).
3. In Change E, revert the condition to `aimBuffer[0] != 0`.  Subcase 7
   MUST go RED (TAM opens while capture is still OPEN:
   `forthTestCapState()` not CLOSED under `tam.mode != 0`).
4. In C5, revert the F5-2 gate argument to `aimBuffer`.  Subcase 9 MUST
   go RED (the malformed line commits — an empty aimBuffer passes the
   check).
5. In D2, revert the second condition to `aimBuffer[0] != 0 && !tam.mode`.
   Subcase 10 MUST go RED (EXIT falls to the empty-arm and deletes a
   glyph instead of committing).

Report: eleven PASS lines, both banners, exit 0, arena line vs baseline
PLUS the capture-buffer free/alloc equality note, closing inventory
deltas, `git diff --check`, generated-mirror equality, five mutation
REDs.  RULE-1: flash delta expected small; record the measured
`make dmcp5r47` delta in the stage commit.

### Commit

```text
forth-core: F6-1 — managed capture buffer behind the capture object
```
