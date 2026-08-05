# PACKET L1-F2 — the three `tam.c` seams and the `determineItem` fix

**Stage L fold packet 2 of 3.** **Prerequisite: L1-F1 landed and green.**

**Scope.** Wire F1's context into `tam.c` and make TAM keystrokes resolve
correctly. After this packet, pressing `STO` `0` `5` during an interactive
capture types `STO 05 ` into the line. **No `tam.c` commit site is
edited** — that is the point of the mechanism.

## Implementer contract

As PACKET_L1_1. Not repeated.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthFoldEnter" packages/forth-core/forth_capture.h                  # F1 landed
grep -n "else if(calcMode == CM_PEM && forthCapIsOpen())" packages/forth-core/ui/tam.c
grep -n "hourGlassIconEnabled = false;" packages/forth-core/ui/tam.c
grep -n "void tamProcessInput(uint16_t item)" packages/forth-core/ui/tam.c
grep -n "static void _tamProcessInput" packages/forth-core/ui/tam.c
grep -n "case CM_PEM: {" packages/forth-core/ui/tam.c                          # record all
```

## C1 — Seam 1: `tamEnterMode` materialises and suspends

`packages/forth-core/ui/tam.c:1180-1182` today:

```c
    else if(calcMode == CM_PEM && forthCapIsOpen()) {
      forthCaptureSuspend();                /* F6-2: suspend, never close */
    }
```

becomes

```c
    else if(forthCapIsOpen() && (calcMode == CM_PEM || forthCapIsInteractive())) {
      if(calcMode != CM_PEM) { forthFoldEnter(func, tam.mode); }   /* L: interactive */
      forthCaptureSuspend();                                       /* F6-2: unchanged */
    }
```

Ordering is safe: this arm sits after the `CM_NIM` arm (:1163) and before
the two remaining `CM_PEM`-only arms (:1183, :1194), so a `CM_AIM` capture
reaches it and nothing downstream changes. `func` and `tam.mode` are final
here — `tam.mode` is assigned at :1151, `tam.function` at :1153. This is
also why the fold state cannot be derived from `tam.function`
(forth_capture.h:29-33 documents exactly that trap).

**`forthFoldEnter` runs BEFORE `forthCaptureSuspend`** — suspend requires
`currentStep` parked on the capture step, which is what enter leaves.

## C2 — Seam 2: `leaveTamModeIfEnabled` resumes and sweeps

`packages/forth-core/ui/tam.c:1406-1409` today:

```c
    if(calcMode == CM_PEM) {
      hourGlassIconEnabled = false;
      forthCaptureResume();                   /* no-op unless FCAP_SUSPENDED */
    }
```

becomes

```c
    if(calcMode == CM_PEM || forthFoldPending()) {
      hourGlassIconEnabled = false;
      forthCaptureResume();      /* no-op unless FCAP_SUSPENDED — unchanged */
      forthFoldLeave();          /* no-op unless pending */
    }
```

The `forthFoldPending()` disjunct is required for two reasons: PARK never
brackets `calcMode`, and `leaveTamModeIfEnabled` is also reached from
outside `tamProcessInput` (EXIT during TAM, keyboard.c:3785) where the
bracket is off.

`leaveTamModeIfEnabled` is the right exit choke: it early-returns on
`!tam.mode` (ui/tam.c:1364-1366), and every commit and cancel inside
`_tamProcessInput` routes through it. **Enumerate those routes yourself
and report the list** — do not trust this sentence.

## C3 — Seam 3: the `calcMode` bracket

`packages/forth-core/ui/tam.c:1414-1417` today:

```c
  void tamProcessInput(uint16_t item) {
    _tamProcessInput(item);
    _tamUpdateBuffer();
  }
```

becomes

```c
  void tamProcessInput(uint16_t item) {
    /* L-R4 (b): every TAM commit site in this file already has a
     * calcMode == CM_PEM arm that RECORDS a step instead of dispatching.
     * Making that predicate true for the duration of the commit is the
     * whole non-executing-TAM mechanism — no commit site is edited.
     * Narrow by design: a wider bracket would paint _refreshPemScreen
     * (screen.c:6176) and the PEM TAM overlay (screen.c:5637) under the
     * prompt.  Nothing inside the commit path refreshes: _insertInProgram's
     * tail calls scanLabelsAndPrograms + goToGlobalStep (manage.c:770-772),
     * neither of which refreshes (lblGtoXeq.c:101-140). */
    const uint8_t savedMode = calcMode;
    const bool_t  brk       = forthFoldArmed();
    if(brk) { calcMode = CM_PEM; }
    _tamProcessInput(item);
    /* Re-test before restoring: an error raised inside the commit may have
     * changed calcMode, and the epilogue must not clobber that. */
    if(brk && calcMode == CM_PEM) { calcMode = savedMode; }
    _tamUpdateBuffer();
  }
```

**Re-entrancy note (pin it in the comment).** `leaveTamModeIfEnabled`
contains a `PC_BUILD`-only call to `tamProcessInput` guarded by
`forceTamAlpha` (ui/tam.c:1344-1349). `forceTamAlpha` is **never assigned
`true` anywhere in either tree** — only cleared (ui/tam.c:1346,
config.c:1789, src/c47/config.c:1778, src/c47/ui/tam.c:1331) — so it is
dead today. If it were ever enabled it would nest this bracket and the
inner epilogue would restore `CM_PEM`. **Do not assume non-reentrancy:**
add `if(brk && savedMode == CM_PEM) { /* already bracketed — STOP */ }`
as a self-test-only assertion, or document the hazard at the site. Report
which you did.

## C4 — `determineItem`: TAM precedence (keyboard.c:1686)

`calcMode` does not change on TAM entry (`tam.mode != 0` is the gate), so
during an interactive TAM `calcMode` is still `CM_AIM` and the arm's
**first** disjunct fires — before the `else if(tam.mode)` arm that selects
`key->primaryTam`. TAM digits would resolve to letters.

PEM gets its escape for free: `forthCaptureSuspend` clears `FLAG_ALPHA`
(manage.c:1202) and PEM's disjunct is
`calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && …`. `CM_AIM` has no
such conjunct. Extend L1-3's escape:

```c
    else if((calcMode == CM_AIM
             && !(forthCapIsInteractive() && forthCapKeysMode())
             && !(tam.mode && forthFoldPending()))
            || …unchanged…
```

Safety is provable from the write-set: `forthFoldPending()` reads
`forthCap.foldMode`, written only by `forthFoldEnter`/`forthFoldLeave`,
and `forthFoldEnter` has exactly one call site (C1), gated on
`forthCapIsInteractive()`, false everywhere before Stage L. **The
predicate's value is identical to today's for every execution that exists
today.**

After the stage, in the one new state: `tam.alpha` false → falls to
`key->primaryTam`, the same column a PEM TAM gets; `tam.alpha` true → the
`tam.alpha` disjunct on the same line still fires → AIM column → letters,
again as PEM. Parity both ways.

## C5 — tests (`test_fold_seams`, new; register per L1-1 C4)

1. **The headline.** Interactive capture holding `42`, keys mode on;
   drive `runFunction(ITM_STO)`, then `tamProcessInput(ITM_0)`,
   `tamProcessInput(ITM_5)`. Assert `aimBuffer == "42 STO 05 "` and
   **program memory is bit-identical to before the keypress**
   (`getNumberOfSteps()`, `firstFreeProgramByte`, and FHIST's step count).
2. **Nothing executed.** Assert register 05 is unchanged by the above.
3. **TAM digits resolve as digits.** With the fold pending, assert
   `determineItem` returns `key->primaryTam` for a digit key, not a letter.
   Drive the real resolution layer, not a hand-set item.
4. **Cancel.** Same setup, then `fnKeyExit(NOPARAM)` before any digit;
   assert the line is still `42`, the capture is OPEN, and program memory
   is bit-identical.
5. **PARK executes live and keeps the line.** Drive a PARK-classified item
   (per F1's `_forthFoldAdmits`); assert it executed, the line survived,
   and program memory is bit-identical.
6. **PEM is untouched.** Re-run the landed F6-2/F6-4 suite unchanged.
7. **The bracket does not leak.** Assert `calcMode == CM_AIM` after every
   subcase above, including the error path (force `lastErrorCode` inside a
   commit if the harness allows; otherwise report).

## Mutations

1. Widen the bracket to span `tamEnterMode`→`leaveTamModeIfEnabled`.
   Report what renders (this is the design's rejected alternative; a
   sim capture is acceptable evidence).
2. Drop the `calcMode == CM_PEM` re-test in the epilogue. RED at C5.7's
   error subcase, or report unpinned.
3. Drop `forthFoldPending()` from Seam 2. RED at C5.4 (EXIT-during-TAM
   leaves debris).
4. Drop the `determineItem` clause. RED at C5.3.
5. Call `forthFoldEnter` after `forthCaptureSuspend` in Seam 1. RED at
   C5.1.

## Out of scope

- Operand-class parity across the full F4 grammar — F3.
- The catalog-driven TAM commit sites (keyboard.c:1148, :1160), which
  bypass `tamProcessInput` entirely and therefore do not fold
  interactively. **Known v1 limitation** (T7.8) — record it in your report
  and in DESIGN.md at stage close; do not fix it here.

## Acceptance

- Gate green; landed F6/K suite unchanged; PASS lines quoted.
- The `leaveTamModeIfEnabled` route list from C2 reported.
- The re-entrancy decision from C3 reported.
- Five mutations RED-then-reverted (or reported with evidence).
- **Sim:** interactive capture, keys mode, `STO` `0` `5` types `STO 05 `
  and stores nothing. Capture via `run-sim`, copy-adapting
  `references/capture-driver.c`.
