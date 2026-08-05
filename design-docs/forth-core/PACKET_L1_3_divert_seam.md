# PACKET L1-3 — the interactive divert seam, catalogs, picker, keys mode

**Stage L packet 3** (evidence: STAGE_L_TRACES.md §T1, §T6, §T8.4, and the
17-row CM-gate audit). **Prerequisite: L1-2 landed and green.**

**Scope.** Direct function items, catalog picks and the FWRD picker insert
their names as TEXT into an open interactive capture instead of executing;
keys mode works interactively. Parameterized items **fall through to TAM
unchanged** — the fold is L1-F*, and under the L-R4 (b) ruling this packet
deliberately does not swallow them.

## Implementer contract

As PACKET_L1_1. Not repeated.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthInteractiveEnter" packages/forth-core/programming/manage.c   # L1-2 landed
grep -n "if(tam.mode == 0 && TM_VALUE <= indexOfItems\[func\].param" packages/forth-core/items.c
grep -n "      if(calcMode == CM_PEM) {" packages/forth-core/items.c       # expect one at ~766
grep -n "else if(calcMode == CM_AIM || (catalog" packages/forth-core/keyboard.c
grep -n "if(calcMode == CM_AIM && !(isAlphabeticSoftmenu()" packages/forth-core/keyboard.c
grep -n "return (calcMode == CM_PEM" packages/forth-core/forth_menu.c
grep -n "progStart = forthOwningProgramStart(currentStep);" packages/forth-core/forth_menu.c
```

## C1 — the divert arm in `runFunction` (items.c)

**Site: immediately before the TAM-entry block** (`if(tam.mode == 0 &&
TM_VALUE <= indexOfItems[func].param …`, items.c:736). Placement is
load-bearing: after it, `tamEnterMode` has already fired and the decision
is gone.

```c
      /* L1-3: the interactive E0-equivalent.  Both physical keys and
       * softkeys converge here — keys via btnPressed -> processKeyAction ->
       * processAimInput (falls through) -> showFunctionNameItem ->
       * btnReleased -> runFunction (keyboard.c:2328); softkeys via
       * executeFunction -> runFunction (keyboard.c:1415).  This is exactly
       * where PEM already diverts, one block below. */
      if(forthCapIsInteractive()) {
        if((indexOfItems[func].status & CAT_STATUS) == CAT_FNCT
           && (indexOfItems[func].status & PTP_STATUS) == PTP_NONE
           && func != ITM_AIM && func != ITM_FORTH
           && func != ITM_ENTER && func != ITM_EXIT1
           && func != ITM_BACKSPACE && func != ITM_RS) {
          (void)forthCapInsertName(indexOfItems[func].itemCatalogName);
          return;
        }
        /* Parameterized items fall THROUGH to the TAM block below —
         * L-R4 (b): the fold makes them type their canonical spelling.
         * Until L1-F* lands they enter TAM and execute, which is the
         * documented interim (record it in your report; it is the one
         * user-visible wart between L1-3 and L1-F1). */
      }
```

The excluded items are the ones L1-2 and L1-1 already own: `ITM_AIM` is
the keys-mode toggle (C4), `ITM_FORTH` re-entry, `ITM_ENTER`/`ITM_RS` run
the line, `ITM_EXIT1` the ladder, `ITM_BACKSPACE` edits. Getting this
exclusion list wrong is the most likely way to break L1-2.

**The dynamic-menu hole (T8.4 item 3).** `runFunction`'s `ITM_RCL` and
`ITM_XEQ` dynamic-menu arms sit **above** this site (items.c:665-735) and
dispatch before the divert is reached; the same shape recurs at
keyboard.c:2259/:2283, screen.c:818/:836 and forth_bridge.c:30. Close it
at the shared sink instead of at six call sites: add to
`insertUserItemInProgram` (manage.c:2229) a first-statement early-out

```c
  if(forthCapIsInteractive()) { (void)forthCapInsertName(name); return; }
```

and **verify by test** (C6.5) that `XEQ 'SOMEWORD'` picked from a dynamic
menu during an interactive capture inserts text rather than executing.
Report whether any of the six call sites still reaches live execution.

## C2 — `determineItem`: the keys-mode column (keyboard.c:1686)

The arm opens `calcMode == CM_AIM || …` as its **first** disjunct,
unconditionally, and only the `CM_PEM` disjunct carries K1's keys-mode
escape. Add the interactive escape to the `CM_AIM` disjunct:

```c
    else if((calcMode == CM_AIM && !(forthCapIsInteractive() && forthCapKeysMode()))
            || (catalog && catalog != CATALOG_MVAR && calcMode != CM_NIM)
            || …unchanged…
```

**Do not** use the broader `calcMode == CM_AIM && !tam.mode`. It would
also change native AIM+TAM behaviour on a path this stage has not traced
(recorded as an observation in T7.4, deliberately not bundled).

Safety is provable from the write-set: `forthCapIsInteractive()` is false
everywhere before Stage L, so the predicate's value is identical to
today's for every execution that exists today.

## C3 — the E10/E11 toggle gesture (keyboard.c:1687-1690)

The landed gate resolves the ALPHA gesture to `ITM_AIM` inside a PEM
capture. Widen its condition to accept the interactive origin:

```c
      if(((calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA)
             && tam.function == ITM_FORTH && forthCapIsOpen())
          || forthCapIsInteractive())
         && shiftF && key->fShifted == ITM_AIM) {
        result = ITM_AIM;
      }
```

Keep it layout-independent (keyed on the row's normal-column `fShifted`,
never a key number) — that is K1's rule and it still holds.

## C4 — the toggle action

PEM's toggle lives in `insertStepInProgram`'s `func == ITM_AIM` arm
(manage.c:1719-1734), which interactive dispatch never reaches. Add the
interactive twin in `runFunction`'s new arm (C1), **before** the
`CAT_FNCT` insert:

```c
        if(func == ITM_AIM) {
          if(forthCapKeysMode()) {
            forthCapSetKeysMode(false);
            showSoftmenu(-MNU_ALPHA);
          }
          else {
            forthCapSetKeysMode(true);
            popSoftmenu();          /* K-R3: the underlying row IS the indicator */
          }
          return;
        }
```

Note the asymmetry with PEM: PEM calls `_closeAlphaMenus()` (a file-static
that clears the whole alpha stack); interactively the ALPHA menu is a
single push from `calcModeAim`, so one `popSoftmenu()` is the twin. **Verify
this by test (C6.3) rather than assuming** — if the stack turns out to hold
more than one alpha level, report it and use a bounded drain loop instead.

## C5 — catalogs and the picker

**Catalog pick must not close AIM.** keyboard.c:1300:

```c
            if(calcMode == CM_AIM && !forthCapIsInteractive()
               && !(isAlphabeticSoftmenu() || isJMAlphaOnlySoftmenu() || item == ITM_KEYMAP)) {
              closeAim();
            }
```

Without this, picking any function from FCNS during an interactive capture
closes the capture, commits the line to X and executes the item.

**The FWRD picker guard.** forth_menu.c:68 is
`calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && tam.function ==
ITM_FORTH && …`. Widen:

```c
  if(softmenu[softmenuStack[0].softmenuId].menuItem != -MNU_FORTH) return false;
  return ((calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && tam.function == ITM_FORTH)
          || forthCapIsInteractive())
      && item == ITM_NOP
      && dynamicMenuItem >= 0
      && dynamicMenuItem < dynamicSoftmenu[softmenuStack[0].softmenuId].numItems;
```

The menu-identity check stays **first** — it is the OOB guard on
`dynamicSoftmenu[]` indexing.

**The picker's re-commit tail.** keyboard.c:987-991 calls
`pemAlpha(ITM_NOP)` after `pickerInsertName()` to re-commit the step.
Interactively there is no step:

```c
        if(forthPickerGuard(item)) {
          if(pickerInsertName()) {
            if(!forthCapIsInteractive()) { pemAlpha(ITM_NOP); }
          }
          return;
        }
```

**The picker's text-scan section.** `forthBuildWordPicker` scans from
`forthOwningProgramStart(currentStep)` (forth_menu.c:107). Interactively
`currentStep` points at whatever the PEM cursor last was — **not NULL** —
so a stale cursor would list that program's definitions with false
provenance. Gate section (a) off:

```c
  progStart = forthCapIsInteractive() ? NULL : forthOwningProgramStart(currentStep);
```

This makes the doc's "interactively it degrades to dictionary sections
only" true by construction rather than by accident.

## C6 — tests (`test_capture_interactive_divert`, new; register per L1-1 C4)

1. **Direct item inserts text.** Open interactive, keys mode on, drive
   `ITM_SIN` through `runFunction`; assert the line is `SIN ` and X is
   unchanged (it did **not** execute).
2. **Token boundary.** Type `42`, then `ITM_SIN`; assert `42 SIN ` — the
   K2 leading-separator rule via `forthCapInsertName` (forth_menu.c:42).
3. **Keys-mode toggle.** Drive the ALPHA gesture through `determineItem`
   + `runFunction`; assert `forthCapKeysMode()` flipped and the softmenu
   changed; toggle back and assert `-MNU_ALPHA` is current.
4. **Catalog pick inserts, does not close.** Open FCNS, pick an item;
   assert the capture is still OPEN, `calcMode == CM_AIM`, and the name
   landed in the line.
5. **Dynamic-menu XEQ inserts.** Define a Forth word, open the FWRD
   picker, pick it; assert text inserted, nothing executed. Then drive the
   `ITM_XEQ` dynamic-menu arm (items.c:699) directly and assert the same —
   this is the T8.4 hole.
6. **Picker sections interactively.** With a program containing `: FOO`
   definitions and `currentStep` inside it, open an interactive capture and
   build the picker; assert `FOO` is **absent** (section (a) gated off) and
   dictionary words are present.
7. **PEM is untouched.** Re-run the landed F6-3/F6-5 picker tests; they
   must pass unchanged.
8. **Parameterized falls through.** Drive `ITM_STO` in interactive keys
   mode; assert `tam.mode != 0` (TAM was entered), i.e. the divert did NOT
   swallow it. This pins the L-R4 (b) contract that L1-F* builds on.

## Mutations

1. Move the divert arm below the TAM block. RED at C6.1 (SIN executes).
2. Drop `func != ITM_ENTER` from the exclusion list. RED at L1-2's C5.1.
3. Drop the `determineItem` escape. RED at C6.1 (the key resolves to a
   letter, not `ITM_SIN`).
4. Remove the `closeAim` guard. RED at C6.4.
5. Leave the picker's text-scan ungated. RED at C6.6.
6. Drop the `insertUserItemInProgram` early-out. RED at C6.5's second half.
7. Keep `pemAlpha(ITM_NOP)` on the interactive picker path. RED — report
   what it does (it will operate on whatever `currentStep` points at); if
   it is silently harmless, say so with evidence rather than deleting the
   mutation.

## Out of scope

- The fold / non-executing TAM — L1-F*. C6.8 pins the fall-through the
  fold hooks.
- History and recall — L1-H.
- The full CM-gate sweep as a class test — L1-5.

## Acceptance

- Gate green; landed F6/K suite unchanged; PASS lines quoted.
- Report which of the six `insertUserItemInProgram` call sites still reach
  live execution, if any.
- Seven mutations RED-then-reverted (or reported with evidence).
- Flash + arena reported.
- **Sim:** in an interactive capture, ALPHA-toggle to keys mode, press SIN,
  see `SIN ` appear in the line; press STO and observe TAM open (the
  documented interim). Capture via `run-sim`, copy-adapting
  `references/capture-driver.c`.
