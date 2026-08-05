# PACKET L1-F1 — the fold context: materialise, arm, sweep, restore

**Stage L fold packet 1 of 3** (ruling L-R4 (b); mechanism: STAGE_L_TRACES.md
§T7). **Prerequisites: L1-3 and L1-H landed and green.** L1-H owns the
FHIST program this packet parks its transient step in.

**Scope.** The enter/leave pair and its context struct, with **no wiring
into `tam.c`** — this packet is inert in production and is proven by a
self-test that drives it directly. F2 wires it; F3 proves parity.

**The mechanism, in one sentence:** materialise a real `ITM_FORTH` capture
step in the FHIST program, then (F2) set `calcMode = CM_PEM` for the
duration of `_tamProcessInput` only, so the landed F6-2/F6-4 machinery
runs unmodified on a real step and the interactive line gets the same text
by the same code.

## Implementer contract

As PACKET_L1_1. Not repeated.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthHistoryEnsure" packages/forth-core/programming/manage.c   # L1-H landed
grep -n "void forthCaptureSuspend" packages/forth-core/programming/manage.c
grep -n "forthCapSetOrigin(originWas)" packages/forth-core/programming/manage.c  # L1-1 bracket
grep -n "static uint16_t _forthCapBuildStep" packages/forth-core/programming/manage.c
grep -n "static void _insertInProgram" packages/forth-core/programming/manage.c
grep -c "forthFold" packages/forth-core/programming/manage.c            # expect 0
```

## C1 — the context (BSS, `programming/manage.c`)

```c
typedef struct {
  uint16_t savedProgram;         /* currentProgramNumber — NOT a global step
                                    number: program boundaries are themselves
                                    global step numbers and all shift when
                                    FHIST grows or evicts (see L1-H C2) */
  uint16_t savedLocalStep;       /* currentLocalStepNumber */
  uint16_t savedFirstDisplayed;  /* firstDisplayedLocalStepNumber */
  uint16_t entryStepCount;       /* getNumberOfSteps() in FHIST, BEFORE the insert */
  uint32_t capStepOffset;        /* capture step vs beginOfProgramMemory —
                                    program memory may relocate */
  /* NOTE: forthHistoryGotoLastStep() is L1-H's export; if L1-H did not
     declare it, that is a STOP, not something to re-implement here. */
  uint8_t  savedZerothStep;      /* pemCursorIsZerothStep */
  uint8_t  pad;
} forthFoldCtx_t;                /* one static instance */
```

Plus one byte of state on the capture object, in `forth_capture.h` beside
`origin`:

```c
  uint8_t     foldMode;       /* 0 = none, 1 = FOLD (bracket armed),
                                 2 = PARK (materialised, bracket NOT armed).
                                 OWNED BY forthFoldEnter/forthFoldLeave, and
                                 by forthCapPowerReset() ONLY.  forthCapOpen,
                                 forthCapClose and forthCapAbandonSuspended
                                 MUST NOT touch it — see below. */
```

**`foldMode` does NOT join the E14 sweep.** `keysMode` is cleared at four
sites — `forthCapOpen` (forth_capture.c:10), `forthCapClose` (:16),
`forthCapAbandonSuspended` (:38), `forthCapPowerReset` (:61) — and
`forthCaptureResume` calls **both** `forthCapOpen()` (manage.c:1224) and,
on the canary path, `forthCapAbandonSuspended()` (manage.c:1214). Since
F2 runs the resume immediately before the leave, clearing `foldMode` at
those sites would make `forthFoldLeave()`'s `if(foldMode == 0) return;`
fire every time: **the transient step would stay in FHIST forever and the
PEM cursor globals would stay pointing into it.** The invariant:
`forthFoldLeave()` must be able to sweep and restore even after a resume
that abandoned the suspension. Only `forthCapPowerReset()` clears it, as
the last-resort reset.

**Report `sizeof(forthCap_t)` before and after.** `state`/`keysMode`/
`origin`/`foldMode` are four `uint8_t`s ahead of a `uint16_t`, so it should
be unchanged; if not, STOP.

API in `forth_capture.h`:

```c
void   forthFoldEnter(int16_t func, uint16_t mode);
void   forthFoldLeave(void);
bool_t forthFoldArmed(void);     /* foldMode == 1 */
bool_t forthFoldPending(void);   /* foldMode != 0 */
```

## C2 — admission: FOLD vs PARK

Out-of-scope TAM classes take **PARK**: the capture is still materialised
and suspended (so the line survives), the bracket is not armed, and the
TAM executes live. PARK is option (c) applied to the minority — it never
refuses the key and never loses the line.

```c
static bool_t _forthFoldAdmits(int16_t func, uint16_t mode) {
  if(func == ITM_GTOP)   { return false; }  /* navigates the program pointer via
                                               unguarded fnGoto/goToPgmStep,
                                               ui/tam.c:888-899 — not an operand */
  if(func == ITM_ASSIGN || func == ITM_USERMODE) { return false; }  /* zeroes
                                               aimBuffer, ui/tam.c:1198-1200 */
  if(func == ITM_DELP)   { return false; }  /* already excluded by the PEM commit's
                                               own guard, ui/tam.c:1102 */
  switch(mode) {
    case TM_NEWMENU:                         /* sets FLAG_ALPHA + zeroes aimBuffer */
    case TM_STRING:                          /* same */
    case TM_KEY:                             /* half-buffer swap */
      return false;
    default: return true;
  }
}
```

**Verify each exclusion's anchor before landing** and report any that does
not say what this list claims. `TM_MENU` is admitted — but T7.7 item 6
flags it as unverified (`manage.c:2191-2195` may read a `tmpString` prefix
the fold does not supply, against the inbound snapshot at
`manage.c:1919-1920`). **Trace it; if unsafe, move `TM_MENU` to PARK and
say so.**

## C3 — `forthFoldEnter` (exact; no unstated steps)

```
forthFoldEnter(func, mode):
  if !forthHistoryEnsure(): forthCap.foldMode = 0; return   /* no program, no fold */

  if currentProgramNumber < 1: goToGlobalStep(1)            /* guard programList[-1] */
  forthFoldCtx.savedProgram        = currentProgramNumber
  forthFoldCtx.savedLocalStep      = currentLocalStepNumber
  forthFoldCtx.savedFirstDisplayed = firstDisplayedLocalStepNumber
  forthFoldCtx.savedZerothStep     = pemCursorIsZerothStep
  pemCursorIsZerothStep = false     /* MUST: a parked capture step is a real
                                       step, never the zeroth-step pseudo-
                                       position.  addStepInProgram's pre-move
                                       (manage.c:2264) is gated on this being
                                       false; left true, the TAM step commits
                                       BEFORE the capture step, resume's
                                       offset-derived pointer (manage.c:1210-1213)
                                       reads the TAM step, the canary
                                       falsifies and the capture is abandoned.
                                       It is a persistent global with no reset
                                       on leaving PEM. */

  forthHistoryGotoLastStep()        /* L1-H's helper; park on FHIST's last
                                       step, before its END */

  forthFoldCtx.entryStepCount      = getNumberOfSteps()   /* AFTER the reposition:
                                       getNumberOfSteps() is keyed entirely on
                                       currentProgramNumber (manage.c:2374-2387),
                                       so sampling it in the CALLER's program and
                                       comparing against FHIST's count in
                                       forthFoldLeave would make the sweep eat
                                       real history whenever FHIST is longer. */

  /* Materialise the capture step, seeded with the LIVE line.  This is
   * manage.c:941-952's shape verbatim, with aimBuffer instead of "". */
  _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, aimBuffer))
  --currentLocalStepNumber
  currentStep = findPreviousStep(currentStep)     /* park ON the capture step —
                                                    the state forthCaptureSuspend
                                                    documents at manage.c:1192-1197 */
  forthFoldCtx.capStepOffset = (uint32_t)(currentStep - beginOfProgramMemory)

  forthCap.foldMode = _forthFoldAdmits(func, mode) ? 1 : 2
```

**`entryStepCount` is sampled BEFORE the insert** — `forthFoldLeave`'s
sweep compares against it plus one (the capture step).

## C4 — `forthFoldLeave` (exact)

```
forthFoldLeave():
  if forthCap.foldMode == 0: return

  /* Debris sweep.  Normally zero iterations: forthCaptureResume already
   * deleted the folded step (manage.c:1253).  This covers every break path
   * in that loop (manage.c:1246 oversize, :1251 no room) and the PARK case.
   * BOUNDED and guarded — deleteStepsFromTo is a silent no-op when from==to
   * (manage.c:221-227), so an unbounded while can spin; findNextStep can
   * return NULL (src/c47/programming/nextStep.c:151-157); and lastErrorCode
   * may already be set on entry.  Landed precedent for the bound is in the
   * same file, manage.c:1279-1282 ("so never spin on the predicate"). */
  savedErr = lastErrorCode; lastErrorCode = ERROR_NONE
  for i in 0 .. 3:
      if getNumberOfSteps() <= forthFoldCtx.entryStepCount + 1: break
      victim = findNextStep(currentStep)
      if victim == NULL or isAtEndOfProgram(victim) or isAtEndOfPrograms(victim): break
      deleteStepsFromTo(victim, findNextStep(victim))
      if lastErrorCode != ERROR_NONE: break        /* L1-H's UAF guard */
  if lastErrorCode == ERROR_NONE: lastErrorCode = savedErr

  /* The capture step, re-derived from an OFFSET with a canary — currentStep
   * has survived an arbitrary TAM, and _insertInProgram rebases every
   * program pointer whenever it grows the region (manage.c:723-733).  This
   * is the landed pattern: forth_capture.h:44-48 keys the suspend snapshot
   * off an offset for exactly this reason. */
  cap = beginOfProgramMemory + forthFoldCtx.capStepOffset
  if cap < firstFreeProgramByte
     and checkOpCodeOfStep(cap, ITM_FORTH)
     and cap[2] == STRING_LABEL_VARIABLE:
      deleteStepsFromTo(cap, findNextStep(cap))
  /* else: canary failed — skip the delete, but STILL restore the cursor and
     STILL clear foldMode below.  Report it if a test ever reaches here. */

  goToPgmStep(forthFoldCtx.savedProgram, forthFoldCtx.savedLocalStep)
  firstDisplayedLocalStepNumber = forthFoldCtx.savedFirstDisplayed
  defineFirstDisplayedStep()
  pemCursorIsZerothStep = forthFoldCtx.savedZerothStep

  forthCap.foldMode = 0
```

`forthFoldLeave` deliberately does **not** touch `calcMode`; F2's bracket
epilogue owns that, using the local it captured before the call.

## C5 — the leak window, stated

The capture step holds the **live line** while it exists, so it is a real
`ITM_FORTH` source step — `_forthCapBuildStep` emits the len=1/NUL
placeholder only for empty text (manage.c:851-855), and
`executeOneStep`'s `ITM_FORTH` arm runs any `len > 0` step through
`forthProgramStep` regardless of markers (lblGtoXeq.c:632-638). A crash
inside the fold therefore leaves a runnable line behind.

It leaves it **in FHIST**, which is exactly where a history entry belongs
(T7.2a) — that is why the fold parks there and not at `currentStep`. Add a
comment saying so at the materialise site, so a later reader does not
"simplify" it back to the caller's program.

## C6 — tests (`test_fold_context`, new; register per L1-1 C4)

1. **Round-trip is bit-identical.** With an interactive capture holding
   known text, call `forthFoldEnter(ITM_STO, TM_STORCL)` then
   `forthFoldLeave()` with no TAM in between. Assert `getNumberOfSteps()`,
   `firstFreeProgramByte`, `currentStep`, `currentLocalStepNumber`,
   `firstDisplayedLocalStepNumber`, `pemCursorIsZerothStep`, `aimBuffer`
   and `T_cursorPos` are all bit-identical to entry.
2. **Same with an empty program** (only `.END.` plus FHIST).
3. **Same with the caller's cursor inside a user program** — assert that
   program is byte-identical afterwards.
4. **The capture step really is materialised.** Between enter and leave,
   assert FHIST gained one step and that it decodes to the line text.
5. **PARK does not arm.** `forthFoldEnter(ITM_GTOP, …)`; assert
   `forthFoldPending()` true and `forthFoldArmed()` **false**, and that
   leave still sweeps cleanly.
6. **Sweep clears debris.** Hand-insert an extra step after the capture
   step, then `forthFoldLeave()`; assert the count returns to entry.
7. **`foldMode` survives the capture-object resets.** Arm a fold, then call
   `forthCapClose()`, and separately `forthCapAbandonSuspended()`, and
   separately `forthCapOpen()`; after each, assert `forthFoldPending()` is
   **still true** and that `forthFoldLeave()` still sweeps and restores.
   Then assert `forthCapPowerReset()` DOES clear it. This is the inverse of
   what rev 1 asked for, and it is the assertion that keeps the fold able
   to unwind after a resume that abandoned the suspension.
8. **Zeroth-step normalisation.** Set `pemCursorIsZerothStep = true` before
   `forthFoldEnter`; assert the TAM step lands **after** the capture step
   and that leave restores the flag to true.
9. **Sweep does not eat history.** With FHIST holding >= 3 steps and the
   caller's program shorter (or empty), run enter+leave and assert FHIST's
   step count is unchanged. This is the B1 pin — none of C6.1-C6.6 catches
   a sweep keyed off the wrong program.

## Mutations

1. Sample `entryStepCount` after the insert. RED at C6.6.
2. Drop the `findPreviousStep` park. RED at C6.4 (the step lands wrong) —
   report which assertion actually fires.
3. Restore the cursor with `goToGlobalStep` keyed off a global step number
   instead of `goToPgmStep(savedProgram, savedLocalStep)`. RED at C6.3 —
   drive it with FHIST holding enough lines that the caller's program
   boundary has shifted.
4. Omit the `lastErrorCode` check in the sweep. Report unpinned with
   evidence if it cannot be driven.
5. Materialise at `currentStep` instead of in FHIST. RED at C6.3.

## Out of scope

- Any `tam.c` edit — F2.
- The `determineItem` TAM-precedence fix — F2.
- Operand-class parity — F3.

## Acceptance

- Gate green; `sizeof(forthCap_t)` reported; PASS lines quoted.
- The `_forthFoldAdmits` anchor verification reported, including the
  `TM_MENU` decision.
- Five mutations RED-then-reverted (or reported with evidence).
- Flash, arena, and program-memory delta across a fold reported (T7.7
  item 5 — this is the measurement that closes it).
