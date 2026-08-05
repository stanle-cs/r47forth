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
grep -n "      if(calcMode == CM_PEM) {" packages/forth-core/items.c       # expect FIVE (670, 699, 711, 719, 766); the load-bearing one is 766
grep -n "else if(calcMode == CM_AIM || (catalog" packages/forth-core/keyboard.c
grep -n "if(calcMode == CM_AIM && !(isAlphabeticSoftmenu()" packages/forth-core/keyboard.c
grep -n "return (calcMode == CM_PEM" packages/forth-core/forth_menu.c
grep -n "progStart = forthOwningProgramStart(currentStep);" packages/forth-core/forth_menu.c
```

## C0 — includes (do this first)

`packages/forth-core/items.c` includes only `c47.h` and `forth_dict.h`
(items.c:4-5), and neither reaches `forth_capture.h` or `forth_menu.h`.
Every symbol this packet adds to that file — `forthCapIsInteractive`,
`forthCapKeysMode`, `forthCapSetKeysMode`, `forthCapInsertName` — would be
implicitly declared `int` and the build would stay green
(`warning_level=2`, no `werror`) with ABI-unspecified returns. Add:

```c
#include "forth_capture.h"
#include "forth_menu.h"
```

after `forth_dict.h`. **Do not** hand-declare them at the top of the file;
items.c:8-9 does exactly that for `fnForthOuter`/`fnForthCall` and it is
the pattern not to repeat. Gate grep:
`grep -c 'forth_capture.h' packages/forth-core/items.c` must be 1 after.

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

**The dynamic-menu hole (T8.4 item 3) — fix it at the DECISION, not the
sink.** `runFunction`'s `ITM_RCL`/`ITM_XEQ` dynamic-menu arms sit **above**
this site (items.c:665-735) and dispatch before the divert is reached. Rev
1 proposed an early-out inside `insertUserItemInProgram` (manage.c:2229) —
**that is unreachable interactively.** All nine sites have the shape
`if(calcMode == CM_PEM) { insertUserItemInProgram(...); } else
{ reallyRunFunction(...); }`, and interactively `calcMode == CM_AIM`, so
the else arm runs and the sink is never entered. An early-out there would
be dead code, and Mutation 6 could not go RED against it.

Add one helper and call it at all nine sites:

```c
/* L1-3: the three-way dispatch for a name resolved from a dynamic menu or
 * a USER key.  PEM records a step, an open interactive capture takes the
 * name as TEXT, everything else executes. */
void forthUserItemDispatch(int16_t item, char *funcParam, int16_t execItem, uint16_t execParam) {
  if(calcMode == CM_PEM)           { insertUserItemInProgram(item, funcParam); }
  else if(forthCapIsInteractive()) { (void)forthCapInsertName(funcParam); }
  else                             { reallyRunFunction(execItem, execParam); }
}
```

The nine sites, each currently an `if(calcMode == CM_PEM){insert}else{exec}`
pair — **verify each against the tree before editing, and report any whose
shape differs**: items.c:670/674, :699/703, :711/715, :719/723;
keyboard.c:2259/2269, :2283/2293; screen.c:818/822, :836/840;
forth_bridge.c:30/34 (`forthDispatchColon`).

`screen.c` and `forth_bridge.c` need the same `forth_capture.h` include
treatment as C0 — check each and report.

## C2 — `determineItem`: the keys-mode column (keyboard.c:1686)

The arm opens `calcMode == CM_AIM || …` as its **first** disjunct,
unconditionally, and only the `CM_PEM` disjunct carries K1's keys-mode
escape. Add the interactive escape to the `CM_AIM` disjunct:

```c
    else if((calcMode == CM_AIM && !(forthCapIsInteractive() && forthCapKeysMode()))
            || (catalog && catalog != CATALOG_MVAR && calcMode != CM_NIM)
            || …unchanged…
```

**This is HALF of an atomic two-part edit. Landing only this half puts a
bug screen on every keypress in interactive keys mode.** Escaping the
`CM_AIM` disjunct drops through `else if(tam.mode)` (false, keyboard.c:1723)
to the normal-column branch at keyboard.c:1726 — which lists `CM_PEM` (that
is why K1 works in PEM) but **does not list `CM_AIM`** — and then to
`else { displayBugScreen(bugScreenItemNotDetermined); }` at keyboard.c:1737.

So also add `CM_AIM` to the normal-column branch, with the **identical**
predicate so the two halves cannot disagree:

```c
    else if(calcMode == CM_NORMAL || … || calcMode == CM_TIMER || calcMode == CM_LISTXY
            || (calcMode == CM_AIM && forthCapIsInteractive() && forthCapKeysMode())) {
```

**Do not** use the broader `calcMode == CM_AIM && !tam.mode`. It would
also change native AIM+TAM behaviour on a path this stage has not traced
(recorded as an observation in T7.4, deliberately not bundled).

**Reachability, not write-set.** Rev 1 argued safety from the write-set —
"the predicate is false everywhere before Stage L, so nothing existing
changes". True, and irrelevant: the defect class is in the paths the
escape **newly opens**, which is exactly what the bug screen above
demonstrates. State the new state explicitly and what it resolves to:
interactive capture + keys mode + `tam.mode == 0` → normal column; every
other state unchanged.

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

**One `popSoftmenu()` is NOT the twin — use a bounded drain.** PEM calls
`_closeAlphaMenus()` (manage.c:776-800), a `SOFTMENU_STACK_SIZE`-bounded
drain over the ALPHAINTL/ALPHAintl/ALPHAMATH/ALPHA_OMEGA/alpha_omega/
ALPHA/MyAlpha family. Interactively an alpha **submenu** can sit above
`-MNU_ALPHA` — `isAlphaSubmenu` counts `-MNU_FORTH` (the FWRD picker) as
one (softmenus.c:3880-3891) — which is exactly the state C6.5 has the
tester create. Specify:

```c
            for(int i = 0; i < SOFTMENU_STACK_SIZE; ++i) {
              if(!isAlphaSubmenu(0) && currentMenu() != -MNU_ALPHA) { break; }
              popSoftmenu();
            }
```

**This is deliberately NOT byte-for-byte PEM parity, and the divergence is
a decision this packet makes.** `_closeAlphaMenus` has no `MNU_FORTH` case
and returns without popping when FWRD is on top, so "match PEM" has no
answer for the FWRD-on-top state. **Interactive choice: drain everything
alpha, FWRD included** — the K-R3 rationale is that the underlying row IS
the mode indicator, and leaving FWRD standing would show an alpha row in
keys mode, which is precisely the confusion K-R3 exists to prevent.
Flag this in your report as a provisional decision for owner review.

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
3. **Keys-mode toggle, two legs.** (a) Drive the ALPHA gesture through
   `determineItem` + `runFunction`; assert `forthCapKeysMode()` flipped
   and the softmenu changed; toggle back and assert `-MNU_ALPHA` is
   current. (b) Open the FWRD picker, THEN toggle to keys mode; assert the
   current menu is neither `-MNU_ALPHA` nor an alpha submenu — the M3
   drain.
3b. **No bug screen in keys mode.** With keys mode on, drive a physical
   key all the way through `determineItem`; assert the resolved item is
   the normal-column one and that `calcMode != CM_BUG_ON_SCREEN`. This is
   the B2 pin.
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
6. Route one of the nine dispatch sites straight to `reallyRunFunction`
   instead of `forthUserItemDispatch`. RED at C6.5's second half.
6b. Add the `CM_AIM` escape at keyboard.c:1686 WITHOUT the companion at
   :1726. RED at C6.3b (bug screen).
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
