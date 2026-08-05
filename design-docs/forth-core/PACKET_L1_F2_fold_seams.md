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
      if(forthCapIsInteractive()) { forthFoldEnter(func, tam.mode); }
      forthCaptureSuspend();                                       /* F6-2: unchanged */
    }
```

**The guard is `forthCapIsInteractive()`, NOT `calcMode != CM_PEM`.** Rev 1
used the latter, which discriminates on a value **C3's own bracket
forges**. `tamEnterMode` is re-entered from inside a bracketed
`_tamProcessInput` — e.g. ui/tam.c:980 `leaveTamModeIfEnabled()` then :987
`runFunction(i)` → items.c:736 → :744 `tamEnterMode(func)` — and at that
point `calcMode == CM_PEM` is still true, so rev 1's guard would suspend
with **no fold armed**; `forthFoldPending()` would then be false, C3's
epilogue would restore `CM_AIM`, and the capture would be stuck
`FCAP_SUSPENDED` with no path back. The same shape recurs at ui/tam.c:1130
and :1139-1143.

**Contract on L1-1 this depends on:** `forthCapIsInteractive()` must stay
true across a suspension. L1-1 C1 already brackets `origin` through the
resume re-open; cite it, and add a C5 subcase driving the re-entry chain
that asserts the capture is OPEN (not SUSPENDED) after the bracket drops.

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
    }
```

**`forthFoldLeave()` does NOT go here — it goes in C3's epilogue.** Rev 1
put it here on the claim that `leaveTamModeIfEnabled` is the exit choke
for every commit and cancel. **That claim is false**, and it is the same
claim T7 made. The enumerated routes (all `packages/forth-core/ui/tam.c`):

- *commit-then-leave*: 223, 560, 595, 614, 626, 794, 920, 924, 931, 1041,
  1139, 1143
- *leave-then-**dispatch***: 303, 494, 508, 520, 536, 572, 913, 934, 980,
  996, 1130 — these call `leaveTamModeIfEnabled()` and **then** dispatch,
  so a `forthFoldLeave()` hung on the leave would unwind the fold *before*
  the work it is bracketing
- *cancel/EXIT*: 238, 317, 321, 431
- *from outside `tamProcessInput`*: keyboard.c:3785, ui/tam.c:1168

And several sites dispatch or navigate with **no `calcMode == CM_PEM` arm
at all** — the blanket "every commit site records a step" claim does not
hold: ui/tam.c:566-573 (`leaveTamModeIfEnabled(); runFunction(tamOperation());`
— reachable from this packet's own headline gesture, since `menu_TamSto`
carries `ITM_dddVEL`/`ITM_dddIX`/`ITM_dddVEL1..3`), :303-304, :888-899,
:771-796, :975-999, and :1119-1124 (where `ITM_GTOP` and `ITM_DELP` are
tested **before** the `else if(calcMode == CM_PEM) { /* already done */ }`
at :1125).

`ITM_GTOP` and `ITM_DELP` are already PARKed by F1's `_forthFoldAdmits`.
For the rest, the fix is **not** a longer exclusion list — it is to put
the unwind where it cannot be bypassed: **the bracket epilogue in C3**,
which runs on every path out of `_tamProcessInput` regardless of which
internal route was taken.

**Verify the route list yourself and report any site it misses.** Also
report, for each no-CM_PEM-arm site above, whether F1's admit set covers
it or whether it can fold — a site that dispatches live inside an armed
fold is a finding.

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
    /* L-R4 (b): the RECORDING commit sites in this file (ui/tam.c:217, 552,
     * 587, 605, 618, 907, 929, 1102) each have a calcMode == CM_PEM arm that
     * records a step instead of dispatching.  Making that predicate true for
     * the duration of the commit is the non-executing-TAM mechanism, and no
     * commit site is edited.  It is NOT true of every site — see C2 for the
     * ones that dispatch or navigate with no CM_PEM arm; those are covered by
     * F1's admit set (PARK) or by the unwind in this epilogue, not by the
     * bracket.
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
    /* The fold unwinds HERE, not in leaveTamModeIfEnabled — that function is
     * not the exit choke (see C2): eleven sites call it and THEN dispatch, so
     * an unwind hung on it would fire before the work it brackets.  This
     * epilogue runs on every path out of _tamProcessInput. */
    if(forthFoldPending()) { forthFoldLeave(); }
    _tamUpdateBuffer();
  }
```

**Ordering within the epilogue is load-bearing:** `calcMode` is restored
**before** `forthFoldLeave()`, because leave calls `goToPgmStep` and the
cursor restore must land with the machine back in its real mode.

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

**State the reachability, not the write-set.** It is true that
`forthFoldPending()` is false everywhere before Stage L, so no existing
execution changes — and that is not the interesting question. The defect
class is in the path the clause **newly opens**. That path is exactly one
state: interactive capture + `tam.mode != 0` + fold pending. In it,
`tam.alpha` false → falls to `key->primaryTam`, the same column a PEM TAM
gets; `tam.alpha` true → the `tam.alpha` disjunct on the same line still
fires → AIM column → letters, again as PEM. Parity both ways, and C5.3
drives it through the real resolution layer rather than asserting it.

(L1-3 C2 carries the companion edit at keyboard.c:1726 that keeps keys
mode off the bug screen. **This clause is an extension of L1-3's, not a
replacement** — both halves must be present, and if L1-3 shipped without
its companion, F2's state lands in the same `displayBugScreen` at
keyboard.c:1737. Gate-grep for L1-3's edit before making this one.)

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
3. Move `forthFoldLeave()` from the C3 epilogue back into Seam 2. RED at a
   new C5 subcase driving a leave-then-dispatch site (ui/tam.c:566 via a
   `menu_TamSto` softkey such as `ITM_dddVEL`) — the fold unwinds before
   the dispatch it brackets.
3b. Drop `forthFoldPending()` from Seam 2. RED at C5.4 (EXIT-during-TAM
   leaves the capture suspended).
4. Drop the `determineItem` clause. RED at C5.3.
5. Call `forthFoldEnter` after `forthCaptureSuspend` in Seam 1. RED at
   C5.1.
6. Change Seam 1's guard back to `calcMode != CM_PEM`. RED at the new
   re-entry subcase (capture stuck SUSPENDED).

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
