# REM from the Functions catalog leaves the CATALOG softmenu up during alpha entry

**Version:** `00.109.03.03b0` (`VERSION1`, src/c47/defines.h:12)
**Build:** GTK simulator (`build.sim`), Linux x86-64. Target is DM42-class.

## Preconditions (all three are required)

1. **You must be in program entry mode (PEM).** Outside PEM, `REM` is `fnNop`
   (items.c row 1554) and does nothing at all — no alpha, no step. It is a
   program-comment item; it is only live while editing a program.
2. The REM must be **reached through the catalog**: CAT → FCNS → REM.
3. There must therefore be an **MNU_CATALOG left buried** on the softmenu stack,
   which CAT → FCNS produces naturally.

Miss any one of these and the behaviour is correct — see the controls below.

## Symptom

In PEM, insert a REM via the catalog: **CAT → FCNS → REM**.

Alpha entry switches on correctly — `FLAG_ALPHA` is set and the text cursor
appears — but the softmenu row still shows **CATALOG**, not **ALPHA**. You are
typing alpha text with the catalog keys under your fingers and no alpha menu.

```
observed:  currentMenu() == -MNU_CATALOG (-1318),  FLAG_ALPHA == 1
expected:  currentMenu() == -MNU_ALPHA   (-1922),  FLAG_ALPHA == 1
```

## Cause — two predicates disagree about "are we in the catalog"

1. **The REM arm pops one menu, top-of-stack**
   (src/c47/programming/manage.c:1391-1394, in `insertStepInProgram`):

   ```c
   if(catalog) {      // If called from a catalog such as FNCS, exit catalog and Asm Mode
     leaveAsmMode();
     popSoftmenu();
   }
   ```

   Reached from CAT → FCNS the stack is `[FCNS, CATALOG, ..]`. This pops FCNS
   and leaves **CATALOG buried**.

2. `pemAlpha()` then pushes the alpha menu (manage.c:822) → `[ALPHA, CATALOG, ..]`
   and sets `FLAG_ALPHA`.

3. `executeFunction` calls `_closeCatalog()` immediately after `runFunction()`
   (src/c47/keyboard.c:1164 then :1167). `_closeCatalog` decides `inCatalog` by
   scanning the **entire** softmenu stack (keyboard.c:439-444), so the buried
   CATALOG still counts. `currentMenu()` is now `MNU_ALPHA`, which is not one of
   the TAM cases, so it falls through to `default:` → `closeAllCatalogMenus()`
   (keyboard.c:461-464).

4. `closeAllCatalogMenus()` pops `currentMenu()` if it appears in
   `CatalogMenus[]` (keyboard.c:425-434) — and **`MNU_ALPHA` is in that list**
   (keyboard.c:402). So it pops the alpha menu that was just pushed, re-exposing
   CATALOG.

The arm's cleanup is *top-of-stack*; `_closeCatalog`'s detection is
*stack-wide*. The alpha menu is collateral damage between the two.

## How this was verified

Simulator only, driving the real dispatch pair from the built-in test harness —
`runFunction(ITM_REM)` followed by `_closeCatalog()`, which is exactly what
`executeFunction` does at keyboard.c:1164/1167 — with the softmenu stack built
by real `showSoftmenu(-MNU_CATALOG)` / `showSoftmenu(-MNU_FCNS)` calls rather
than hand-poked, and `fnKeyInCatalog = 1` as keyboard.c:1129 sets it on that
branch.

Observed stack at each step (`top`, then stack slots 0..3):

```
-- A: PEM, CAT -> FCNS -> REM                                    <-- the bug
  after CAT        top=-1318  [-1318,-1349,-1394,-1394] ALPHA=0 catalog=0
  after FCNS       top=-1330  [-1330,-1318,-1349,-1394] ALPHA=0 catalog=2
  after REM        top=-1922  [-1922,-1318,-1349,-1394] ALPHA=1 catalog=0
  after _closeCat  top=-1318  [-1318,-1349,-1394,-1394] ALPHA=1 catalog=0
```

### Controls

```
-- B: PEM, REM, no catalog anywhere on the stack                 <-- correct
  after REM        top=-1922  [-1922,-1349,-1349,-1349] ALPHA=1 catalog=0
  after _closeCat  top=-1922  [-1922,-1349,-1349,-1349] ALPHA=1 catalog=0

-- C: NOT in PEM (CM_NORMAL), CAT -> FCNS -> REM                 <-- no-op
  after FCNS       top=-1330  [-1330,-1318,-1349,-1349] ALPHA=0 catalog=2
  after REM        top=-1330  [-1330,-1318,-1349,-1349] ALPHA=0 catalog=2
  after _closeCat  top=-1318  [-1318,-1349,-1349,-1349] ALPHA=0 catalog=0
```

**B** shows the alpha menu survives when no CATALOG is buried — so the buried
CATALOG is the necessary ingredient, not REM itself. **C** shows that outside
PEM, REM does nothing whatsoever: `runFunction` falls past the `CM_PEM` block to
`reallyRunFunction(ITM_REM, NOPARAM)` → `fnNop`. If you try to reproduce this
outside program entry you will see no alpha mode at all, which looks like "the
bug isn't there" but is really precondition 1 not being met.

I have **not** reproduced this by physical keypress. The equivalent end state
(alpha entry on, catalog menu still displayed) was originally seen on real R47
hardware via a different catalog item, which is what led me here.

## Notes

- Not REM-specific in principle: any arm that pops once and then pushes a menu
  listed in `CatalogMenus[]` should land in the same state.
- `closeAllCatalogMenus()` carries the note *"Option to recurse and close more
  than one menu level until all the CAT related menus are out"*
  (keyboard.c:430). Recursing there would also pop the alpha menu, so enabling
  it is not the fix on its own.
- The comment above `CatalogMenus[]` (keyboard.c:395-397) shows the stack-wide
  detection is deliberate and already known to have edge cases: *"The previous
  system closes a normal menu if the CAT main menu happens to be in one of the
  older menu slots in the menu stack"*.
- Suggested direction (untested upstream): make the arm's cleanup use the same
  stack-wide notion as `_closeCatalog` — drain until no `MNU_CATALOG` remains —
  rather than popping once. Changing `_closeCatalog` itself looks riskier: it has
  six other call sites.
