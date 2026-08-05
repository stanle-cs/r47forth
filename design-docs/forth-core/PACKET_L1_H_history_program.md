# PACKET L1-H — the FHIST program: push, cap, evict, recall

**Stage L packet** (ruling: L-R7, superseding L-R5; evidence:
STAGE_L_TRACES.md §T7.2a, §T7.2b, §T4). **Prerequisite: L1-2 landed and
green** (it calls `forthHistoryPush` as a stub).

**Scope.** Interactive lines accumulate as `ITM_FORTH` steps in one kept,
named, runnable program; the ring is capped with oldest-first eviction;
f-shifted up/down recall lines into the open capture. This packet also
creates the program the fold will later borrow (T7.2a), so L1-F* depends
on it.

## Implementer contract

As PACKET_L1_1. Not repeated.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthHistoryPush" packages/forth-core/programming/manage.c   # L1-2 stub present
grep -n "static uint16_t _forthCapBuildStep" packages/forth-core/programming/manage.c
grep -n "void scanLabelsAndPrograms" packages/forth-core/programming/manage.c
grep -n "numberOfPrograms++" src/c47/programming/manage.c             # the END rule
grep -n "forthStepPayload" packages/forth-core/forth_bridge.c   # expect :60 (def) + call sites
grep -n "void fnKeyUp" packages/forth-core/keyboard.c
```

## C1 — the program and its identity

**Name: `FHIST`.** It must NOT be `FORTH`: `forthResolveXEQ` tries labels
before items (forth_dict.c, the label arm precedes the item arm), so a
label named `FORTH` would shadow the `FORTH` item for `XEQ 'FORTH'`.
**Verify before landing** that `FHIST` collides with nothing:

```
grep -rn "\"FHIST\"" packages/forth-core/items.c src/c47/items.c   # expect no rows
```

and drive `forthResolveXEQ("FHIST", &p)` in a test asserting
`FORTH_XEQ_NONE` on a machine with no FHIST program. If it collides, STOP
and report — the name is a spec decision, not an implementer choice.

**Locate-or-create**, in `programming/manage.c`:

```
forthHistoryProgram() -> uint16_t   /* program number, or 0 if absent */
    scan labelList for a global label "FHIST"; return its .program, else 0

forthHistoryEnsure() -> bool_t      /* creates it if absent; false on failure */
    if forthHistoryProgram() != 0: return true
    save the caller's cursor tuple (see C2)
    position currentStep ON THE .END. STEP  (test with isAtEndOfPrograms)
    insert:  LBL 'FHIST'            (global named label; landed
                                     STRING_LABEL_VARIABLE emit shape)
    insert:  END
    restore the caller's cursor tuple
    return forthHistoryProgram() != 0
```

**Position and order are load-bearing — rev 1 deferred them and rev 1 was
wrong.** `_insertInProgram` shifts bytes up and writes **before** the step
`currentStep` points at (manage.c:735 shift, :759-767 write), and
`scanLabelsAndPrograms` assigns a label to the program number current **at
the LBL's position** (src/c47/programming/manage.c:177). So parking on the
last user program's `END` and inserting there would splice FHIST's label
**into the user's last program** — silent corruption of their code.

Correct sequence, with `currentStep` on the `.END.` step for BOTH inserts:

```
  [ …user progs… END ][ .END. ]          currentStep -> .END.
  insert LBL 'FHIST'
  [ …user progs… END ][ LBL ][ .END. ]   currentStep -> .END. (advanced past LBL)
  insert END
  [ …user progs… END ][ LBL ][ END ][ .END. ]
```

The trailing `END` is what makes it a program: `scanLabelsAndPrograms`
counts a program at an `END` whose successor is not `.END.`
(src/c47/programming/manage.c:143-146), so the user's last `END` now
counts, and FHIST's own `END` (successor `.END.`) does not add another.

**Assert the structure, do not assume it** (C5.1): after `forthHistoryEnsure`
on a machine with one user program, assert `forthHistoryProgram()` returns
a number **different from** the user's, and that the user program's bytes
are unchanged — compare a captured byte range before and after, not prose.

**OPEN, settle by test not by reading (T7.2a item 1):** an `END`
immediately followed by `.END.` does **not** increment `numberOfPrograms`
(src/c47/programming/manage.c:144). So an *empty* FHIST may be invisible
to the program count while a *non-empty* one is visible. Write the test
first (C5.1), record what actually happens, and if the empty form is
invisible then `forthHistoryEnsure` must create the program **with its
first line**, not empty. Report which shape you landed.

## C2 — the cursor tuple

Every insert and delete moves the PEM cursor globals. Save and restore
them around every history operation, using the landed
`forthCaptureSuspend`/`forthFoldEnter` shape:

```c
typedef struct {
  uint16_t savedProgram;          /* currentProgramNumber */
  uint16_t savedLocalStep;        /* currentLocalStepNumber */
  uint16_t savedFirstDisplayed;   /* firstDisplayedLocalStepNumber */
  uint8_t  savedZerothStep;       /* pemCursorIsZerothStep */
  uint8_t  pad;
} forthHistCursor_t;              /* 8 bytes, BSS, one instance */
```

Restore via `goToPgmStep(savedProgram, savedLocalStep)` (lblGtoXeq.c:155),
then `firstDisplayedLocalStepNumber` + `defineFirstDisplayedStep()` +
`pemCursorIsZerothStep`.

**A global step number is NOT a stable key here** — rev 1 used one.
Program boundaries are themselves global step numbers
(src/c47/programming/manage.c:392), so every one of them shifts when FHIST
gains or evicts a line; `goToGlobalStep` resolves by counting from the
start (lblGtoXeq.c:120-133) and would land somewhere else. The landed
suspend keys off a **byte offset** for exactly this reason
(manage.c:1191). `(program, localStep)` is stable as long as FHIST is not
inserted at a lower index than the caller's program — **and if the ensure
creates FHIST, remap `savedProgram` accordingly**. C5.5 must run the
restore test with FHIST **before** the caller's program as well as after.

**Do not use `getNumberOfSteps()` to address FHIST.** It reads the
*global* `currentProgramNumber` (manage.c:2374-2387) and is evaluated
before any `goToPgmStep` call, so it counts the program you are leaving.

## C3 — push, cap, evict

```
forthHistoryPush(const char *text):
    if text[0] == 0: return                       /* nothing to keep */
    if !forthHistoryEnsure(): return              /* silent: history is a
                                                     convenience, never an
                                                     error that blocks a run */
    if text equals the newest FHIST line: return  /* L2 rule: consecutive
                                                     duplicates collapse */
    save cursor tuple
    position on FHIST's last step (before its END)
    _insertInProgram(tmpString, _forthCapBuildStep(tmpString, text))
    forthHistoryEvict()
    restore cursor tuple
```

`_forthCapBuildStep` (manage.c:846-859) is the single definition of the
step bytes and **must** be used — it is what makes an empty text emit the
§8.1 len=1/NUL placeholder rather than a marker-aliased `len == 0`. Here
text is non-empty by the guard above, so it emits `len = n`.

```
forthHistoryEvict():
    while bytes(FHIST) > FORTH_HISTORY_MAX_BYTES:
        delete FHIST's FIRST source step (the one after its LBL)
        if lastErrorCode != ERROR_NONE: break      /* see the UAF guard below */
```

`#define FORTH_HISTORY_MAX_BYTES 1024` in `forth_dict.h`, next to the
other budgets. Tunable; pinned by C5.4.

**The upstream use-after-free guard (binding — STAGE_L_TRACES.md §T7.2b).**
`deleteStepsFromTo` calls `scanLabelsAndPrograms`, which frees
`labelList`/`programList` up front (manage.c:129-130) and can early-return
on `ERROR_RAM_FULL` **after** the free without reallocating
(src/c47/programming/manage.c:151-163), leaving both NULL. `leavePem` then
dereferences `programList[...]` via `defineCurrentStep`
(keyboard.c:2404-2409 → src/c47/programming/nextStep.c:532, a file with no
package override). That is an upstream defect and we do **not** patch
upstream (S1 precedent: `UPSTREAM_REPORTS_globalRegister_reset.md`).

So: **check `lastErrorCode` after every `deleteStepsFromTo` in this packet
and abandon the loop on error**, and do not leave a path where `leavePem`
runs with the lists in that state. The guard lives in code this packet
writes. Add a comment naming the upstream defect at the check.

## C4 — recall (f-shifted up/down)

**Unshifted arrows are unavailable** (T4): in `CM_AIM` with the ALPHA menu
up they are the case-change gesture (keyboard.c:4636-4640), with a
scrolling menu they page it (:4644-4646), and otherwise they
`closeAim(); fnBst();` (:4647-4661) — destructive. Empty-line gating does
not save them: case-change on an empty line is exactly when a user reaches
for it.

The f-shifted ids resolve through `determineItem`'s
`shiftF ? key->fShiftedAim : …` (keyboard.c:1702-1704) and therefore do
**not** reach `fnKeyUp`/`fnKeyDown`. Determine the two resolved item ids
empirically — **do not guess**:

```
Report what determineItem returns for the up and down keys with shiftF set
in CM_AIM, and spec the divert against those ids.
```

**The divert site is the item switch, NOT `case CM_AIM`.** Rev 1 said
"beside L1-2's R/S guard" — wrong: `ITM_RS` has no `case` in
`processKeyAction`'s `switch(item)` (keyboard.c:2496-2755), which is why
it falls to `default:` and reaches `case CM_AIM:`; the f-shifted arrows
resolve to `CHR_caseUP`/`CHR_caseDN` (src/c47/assign.c:27, :32, field 7 =
`fShiftedAim`), which **do** have cases (keyboard.c:2682, :2695) that end
`keyActionProcessed = true; break;` and therefore never reach `CM_AIM`.

Add two guarded arms immediately before keyboard.c:2682, falling through
to the landed case-change body when the guard is false:

```c
            case CHR_caseUP:
              if(forthCapIsInteractive()) {
                forthHistoryRecall(-1);
                keyActionProcessed = true;
                break;
              }
              /* fall through to the landed case-change body */
```

and the mirror for `CHR_caseDN` with `+1`. **Confirm the resolved ids
empirically before editing** — report what `determineItem` returns for the
up and down keys with `shiftF` set in `CM_AIM`, and if it is not
`CHR_caseUP`/`CHR_caseDN`, STOP and report rather than adapting.

```
forthHistoryRecall(delta):
    /* browse index is transient: a uint16_t in forthCap, reset to
       "past the newest" at every capture open and at every push */
    move the browse index by delta, clamped to [0, lineCount]
    if index == lineCount: aimBuffer[0] = 0            /* past newest = empty */
    else: copy that FHIST line's text into aimBuffer
    T_cursorPos = (aimBuffer[0] == 0) ? 0 : stringLastGlyph(aimBuffer) + 1
    displayAIMbufferoffset = 0
```

**Copy the text, do not execute the step.** The payload starts at
`step + 4` and runs for `step[3]` bytes and is **NOT** NUL-terminated
(`_forthCapBuildStep`, manage.c:846-859; `forthStepPayload`,
forth_bridge.c:60-68). Do not use `+3` — that is the length byte. Copy
`step[3]` bytes then terminate.

Editing a recalled line and pressing ENTER runs the edited version and
pushes it as newest, leaving the browsed entry untouched — that falls out
of push being unconditional and needs no extra code, but pin it (C5.6).

## C5 — tests (`test_history_program`, new; register per L1-1 C4)

1. **Creation shape.** Call `forthHistoryEnsure()` on a machine with no
   FHIST; assert the program exists and is findable by
   `forthHistoryProgram()`. **Record and assert whether `numberOfPrograms`
   changed** — this settles the T7.2a open item; write the assertion
   against what you observe and say so in your report.
2. **Push and read back.** Push three lines; assert three source steps in
   FHIST in order, each decoding to its text.
3. **Duplicates collapse.** Push `1 2 +` twice; assert one step.
4. **Cap evicts oldest.** Push lines until over `FORTH_HISTORY_MAX_BYTES`;
   assert the byte total is under the cap, the newest line survived, and
   the oldest is gone.
5. **The cursor is restored, both orders.** Note the full cursor tuple,
   push, assert every field is bit-identical afterwards; repeat across an
   eviction. Run it **twice**: once with FHIST created after the caller's
   program, and once with the caller's program created after FHIST, so the
   `(program, localStep)` key is exercised in both orders.
6. **Recall round-trip.** Push two lines, open a capture, recall back
   twice and forward once; assert the line text at each step and that
   editing + ENTER pushes a new newest without altering the browsed entry.
7. **EXIT pushes.** Open interactive, type a line, EXIT; assert it is in
   FHIST (L-R2 consequence: EXIT never loses a line).
8. **The UAF guard.** If you can force `scanLabelsAndPrograms` to take its
   RAM_FULL arm in the harness, assert the eviction loop abandons. If you
   cannot, say so explicitly and pin the guard by mutation 5 alone.
9. **User's programs untouched.** With two user programs present, run a
   push and an eviction; assert both programs are byte-identical and
   `labelList` still resolves their labels.

## Mutations

1. Skip the duplicate check. RED at C5.3.
2. Skip the cursor restore. RED at C5.5.
3. Use `+3` instead of `+4` for the payload. RED at C5.6.
4. Treat the payload as NUL-terminated (copy with `stringByteLength`
   instead of `step[3]`). RED at C5.6 with a line followed by another step.
5. Remove the `lastErrorCode` check in the eviction loop. RED at C5.8, or
   report unpinned with evidence.
6. Raise `FORTH_HISTORY_MAX_BYTES` so eviction never fires. RED at C5.4.

## Out of scope

- The fold borrowing this program — L1-F* (it will call
  `forthHistoryEnsure` and park its transient step there).
- Any change to how FHIST executes when the user runs it: running it
  re-runs the session, deliberately (L-R7), and no guard is built.

## Acceptance

- Gate green; PASS lines quoted; the C5.1 program-count observation
  reported explicitly.
- The two f-shifted item ids reported.
- Six mutations RED-then-reverted (or reported with evidence).
- Flash, arena, **and** the measured program-memory growth across a
  full-cap history reported (T7.2a item 2).
- **Sim:** type three lines, EXIT, FORTH again, f-up twice to recall the
  first, edit it, ENTER. Capture via `run-sim`, copy-adapting
  `references/capture-driver.c`.
