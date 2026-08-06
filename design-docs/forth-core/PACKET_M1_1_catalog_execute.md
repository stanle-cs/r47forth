# PACKET M1-1 — the CATALOG row, the execute resolution, the listing gate

**Stage M packet 1** (design: STAGE_M_BROWSE_ASSIGN.md M-R1/R4/R5;
evidence: STAGE_M_TRACES.md M-T1/M-T5). **Prerequisite: the Stage M
traces commit, green tree.** Architect-implemented (G1-G3 precedent);
this packet is the authored record.

## Production edits (three, all package files)

**E1 — the CATALOG row** (`softmenus.c`, `menu_CATALOG[]` ~:470): the
second row's first `ITM_NULL` becomes `-MNU_FORTH`. One table cell; the
row is a plain submenu push, the same shape as `-MNU_PROGS` beside it.

**E2 — the resolution case** (`keyboard.c` ~:132), replacing the
always-NOP body. **Rev 2, simplified by mutation A's finding:** the
first shape carried an explicit capture branch; mutation A proved it
unreachable — captures live in `CM_PEM`/`CM_AIM`, which the execute
condition already excludes — so the case is one condition, and the
picker-insert guard's `ITM_NOP` precondition holds by mode arithmetic
rather than by a coupled edit:

```c
case MNU_FORTH: {
  dynamicMenuItem = firstItem + itemShift + fn;
  /* M1 (Stage M): CM_NORMAL executes and CM_ASSIGN feeds the assign
   * pick switch, both via the PROGS shape (ITM_XEQ + dynamicMenuItem
   * -> the landed dynamic-XEQ dispatch, PEM guard and error surface
   * included).  Every other state resolves ITM_NOP — captures live
   * in CM_PEM/CM_AIM, so the picker-insert guard's precondition
   * (item == ITM_NOP) holds by mode arithmetic; the M1-1 packet's
   * mutation A proved an explicit capture branch unreachable, and
   * the [3]/[4]/[5]/[9] battery pins every NOP disposition. */
  item = ((calcMode == CM_NORMAL || calcMode == CM_ASSIGN) && tam.mode == 0
          && dynamicMenuItem < dynamicSoftmenu[menuId].numItems)
           ? ITM_XEQ : ITM_NOP;
  break;
}
```

**E3 — the listing gate** (`forth_menu.c`, the `progStart` line ~:110),
**rev 2: ADDITIVE.** The first shape ("scan only in CM_PEM") turned
twelve landed text-scan tests red — the builder is deliberately
mode-blind everywhere the pre-M surfaces reach it, and those tests
encode that. The landed gate keeps its L1-3 interactive conjunct and the
two surfaces Stage M opens join it; every other mode keeps its landed
behaviour byte for byte:

```c
progStart = (forthCapIsInteractive()
             || calcMode == CM_NORMAL || calcMode == CM_ASSIGN)
              ? NULL : forthOwningProgramStart(currentStep);
```

CM_NORMAL/CM_ASSIGN pickers list dictionary sections only — a
normal-mode press of a text-scan name would resolve to nothing and
error, so the name must not be offered there (L1-3's rationale on the
new surfaces; the checklist's "state the new state explicitly" applied
literally after the first shape violated it).

## Tests — `test_fwrd_normal_mode` (test_capture.part.h, registered per the landed pattern)

Fixture: baseline program via tp* (`BASEM` + one `": PW 7 ;"` source
step + END); `forthOuterInterpret(": MW1 41 ; GLOBAL")` and
`": MW2 42 ;"` build one global and one interactive word; picker built
via `showSoftmenu(-MNU_FORTH)` after each mode change; presses driven
through `determineFunctionKeyItem_C47` with the G1 stack-staging idiom;
execution routed by `runFunction(pressedItem)` when non-NOP (the
executeFunction tail's own call).

1. **CM_NORMAL press of the global word executes.** Resolution returns
   `ITM_XEQ`; after `runFunction`: `x_is_longint(41)`, no error.
2. **CM_NORMAL press of the interactive word executes** (listing =
   current scopes, M-R4): `x_is_longint(42)`.
3. **Capture press still inserts** (coupled-edit direction A): open an
   interactive capture, press the word — resolution is `ITM_NOP`, the
   guard fires, the line reads `MW1 `, nothing executes.
4. **Native AIM stays inert** (direction B): native alpha via the
   non-capture AIM surface, FWRD up, press — `ITM_NOP`, `aimBuffer`
   unchanged, X unchanged.
5. **PEM outside a capture stays inert:** `CM_PEM` on the fixture
   program, FWRD up, press — `ITM_NOP`, step count unchanged, X
   unchanged.
6. **The listing gate:** in CM_NORMAL with the cursor on the fixture
   program, the picker contains the dict words and NOT `PW`; in CM_PEM
   at the same cursor it contains `PW`.
7. **Stale-picker press errors natively:** build the picker over the
   dict words, `forthDictClear()`, press — `ERROR_LABEL_NOT_FOUND`, X
   untouched.
8. **Stacked menus under a capture (M-T5 as corrected):** CATALOG then
   FWRD stacked in CM_NORMAL, then `fnForthOuter(NOPARAM)` — the capture
   opens with the alpha row on top, typing works over the buried stack,
   and EXIT restores the user's FWRD menu. (Rev 2 of this packet asserted
   a drain here; the M1-1 battery falsified the trace's claim — the
   FIX-9-analog drain is gated `if (catalog)` at forth_compile.c:1717
   and menu-tree rows never set that variable. The stack-and-unwind
   behaviour is native closeAim shape and correct. STAGE_M_TRACES.md
   carries the dated correction.)
9. **XEQ-TAM keeps the latch shape:** `tamEnterMode(ITM_XEQ)`, FWRD
   press — resolution `ITM_NOP` (the `tam.mode == 0` conjunct),
   `dynamicMenuItem` latched.

## Mutations (each applied, shown RED, reverted — with one retirement)

- **A** — drop E2's capture branch: **stayed GREEN, and the finding was
  about the CODE** (checklist item 8, outcome "fix the code"): captures
  live in `CM_PEM`/`CM_AIM`, which the execute condition already
  excludes, so the explicit capture branch was unreachable by mode
  arithmetic. E2 rev 2 is the simplified single-condition case (NORMAL
  or ASSIGN, no TAM, in bounds → `ITM_XEQ`; else `ITM_NOP`); the
  [3]/[4]/[5]/[9] battery pins every NOP disposition, and the
  coupled-edit worry dissolves — the guard's `ITM_NOP` precondition
  holds structurally. Mutation retired with the branch.
- **B** — E2's condition forced false (revert M1): RED at [1].
- **C** — E3 reverted to the interactive-only gate: RED at [6] (`PW`
  listed in CM_NORMAL; [1]'s no-PW half reds with it).
- **D** — E2's `tam.mode == 0` conjunct dropped: RED at [9].

## Acceptance

Gate green (battery + upstream); PASS lines [1]-[9] quoted; all four
mutations RED and reverted; flash delta recorded at stage close
(RULE-1); arena untouched (no dictionary change).
