# PACKET K1 — keys-mode bit, toggle gesture, column swap, navigation guards

**Stage K packet 1 of 4** (design: STAGE_K_KEYS_MODE.md, owner-ruled
2026-08-04; rules E10, E11, E12.1-.3, E14, and the E13 interim). Implements
the `forthKeysMode` overlay: capture stays OPEN, FLAG_ALPHA stays SET, one
transient bit switches `determineItem` to the normal key columns so physical
function keys reach the landed F6-3/F6-4 text sinks.

## Implementer contract (Claude-subagent edition)

- You are in an isolated git worktree of the repo. Work ONLY through the
  package working area `packages/forth-core/` — never edit `patches/` or
  `files/` (generated), never edit `src/c47/` (upstream).
- The gate is `./packages/forth-core/build-test.sh` (it refreshes the
  generated outputs first). Run it with output redirected to a log file and
  inspect via bounded greps (`grep -a` — the log contains control bytes).
  A run ends green iff the log tail shows `FORTH SELF-TEST: ALL PASSED` and
  `BUILD + SELF-TEST GREEN`.
- **STOP conditions (report back, do not adapt):** a red test this packet
  did not write; any anchor in the EXECUTION GATE not matching; any spec
  statement here that contradicts what you find in the tree. On STOP,
  return a report naming the mismatch with file:line evidence.
- Fixture rules (binding, from the F-series ledger): clear `lastErrorCode`
  per subcase; `dynamicMenuItem = -1` and `programRunStop = PGM_STOPPED`
  before `fnGotoDot`; never prime the state under test (drive the real
  entry point); `compareString` returns 0 on equal; a capture is opened by
  DRIVING it (`runFunction(ITM_AIM)` with the cursor ON the opening
  marker), never by assigning FLAG_ALPHA/tam.function.
- Every mutation below must be applied, shown RED at the named assertion,
  and reverted (`git diff` clean of mutation residue afterward). Record
  each RED line verbatim in your report.
- Report the `FORTH ARENA` lines from your final green gate log (§5.4
  discipline).

## EXECUTION GATE (verify before any edit; STOP on mismatch)

```
grep -n "forthCapRecommitStep" packages/forth-core/programming/manage.c   # helper exists (FIX-7b)
grep -n "FIX-9" packages/forth-core/programming/manage.c                  # resume drain exists
grep -n "pemCloseAlphaInput();" packages/forth-core/programming/manage.c | head -1   # FIX-8 in the ITM_FORTH arm
grep -n "FORTH_SELFTEST_EXPORT void _closeCatalog" packages/forth-core/keyboard.c
grep -n "test_capture_close_paths_reset_tuple" packages/forth-core/test_dict_reloc.c
grep -c "keysMode" packages/forth-core/forth_capture.h                    # must be 0 (not yet implemented)
```

## C1 — forth_capture.{h,c}: the bit and its lifecycle

In `forth_capture.h`, add to `forthCap_t` after `state`:

```c
  uint8_t     keysMode;       /* K1 (E10-E12): 0 = alpha input, 1 = keys.
                                 Transient UI state, NEVER persisted;
                                 meaningful only while state == FCAP_OPEN. */
```

Add prototypes next to `forthCapIsOpen`:

```c
bool_t      forthCapKeysMode(void);        /* K1: keys-mode bit */
void        forthCapSetKeysMode(bool_t on);
```

In `forth_capture.c`: implement both as trivial accessor/mutator on the
struct field. Clear the bit (`= 0`) in `forthCapOpen()` (a fresh capture
starts in alpha input — owner default), in `forthCapClose()` (E14: since
FIX-8, EVERY close path calls forthCapClose, so this one site covers the
whole close sweep), and in `forthCapPowerReset()`.

In `programming/manage.c`, `forthCaptureSuspend()`: add
`forthCapSetKeysMode(false);` immediately before `forthCapSuspendState(...)`.
This is the **E13 interim** (packet K3 replaces it with snapshot+restore):
a TAM round-trip returns to alpha input, keeping bit and UI coherent.

## C2 — keyboard.c: resolution layer

`determineItem` is file-static. Change its definition to use the existing
`FORTH_SELFTEST_EXPORT` macro (defined above `_closeCatalog`) so the suite
can drive it; same for `processKeyAction` (needed by T3). Add matching
`extern` declarations inside the new tests only.

**C2a — column swap.** In the alpha-mode branch condition (the long
`else if` currently reading

```c
    else if(calcMode == CM_AIM || (catalog && catalog != CATALOG_MVAR && calcMode != CM_NIM) || calcMode == CM_EIM || tam.alpha || (calcMode == CM_ASSIGN && (previousCalcMode == CM_AIM || previousCalcMode == CM_EIM)) || (calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA))) {
```

), change the final disjunct to

```c
(calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && !(tam.function == ITM_FORTH && forthCapKeysMode()))
```

With the bit set, CM_PEM falls through to the existing normal-columns
branch (`calcMode == CM_PEM` is already in its mode list) — zero new
resolution logic, exactly the design's load-bearing claim. The `catalog`
disjunct is deliberately untouched: an open catalog owns the keyboard in
both sub-modes (F6-3 behavior, unchanged).

**C2b — E10 gesture remap.** Inside that branch, wrap the existing body
(the `result = shiftF ? key->fShiftedAim : ...` assignment through the
`-MNU_EIMCATALOG` remap) in an `else { ... }` preceded by:

```c
      if(calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA)
         && tam.function == ITM_FORTH && forthCapIsOpen()
         && shiftF && key->fShifted == ITM_AIM) {
        /* K1/E10: inside a Forth capture the ALPHA gesture is the keys-mode
         * toggle — resolve to ITM_AIM instead of the aim-column ITM_alpha.
         * Layout-independent: keyed on the row's normal-column fShifted,
         * not on a key number.  Falls through to the shared function tail
         * so shift state is consumed normally (no early return). */
        result = ITM_AIM;
      }
      else {
        ...existing body verbatim...
      }
```

Do NOT early-return: the function tail (`resetShiftState()` etc.) must run.
Preserve every byte of the existing body inside the `else`.

## C3 — keyboard.c: processKeyAction CM_PEM guards (E12.2)

In the `case CM_PEM:` switch arms:

- `ITM_SST` and `ITM_BST` arms — insert before the `fnSst(NOPARAM)` /
  `fnBst(NOPARAM)` call:

```c
                  if(forthCapIsOpen() && aimBuffer[0] == 0) {
                    pemAlpha(ITM_BACKSPACE);  /* K1/E12.2: abort the empty
                       placeholder first — no navigation may leave
                       FCAP_OPEN behind (fnSst/fnBst's own close branch
                       only fires on non-empty aimBuffer). */
                  }
```

- `ITM_RS` arm — insert before `addStepInProgram(ITM_STOP)`:

```c
                  if(forthCapIsOpen()) {
                    pemCloseAlphaInput();     /* K1/E12.2: commit the line,
                       then the native STOP step.  Without this, the E0
                       alpha divert would type the text "STOP " into the
                       line — a flow-reject word that cannot compile. */
                  }
```

These arms are unreachable during capture today (alpha columns hide the
items); keys mode makes them real. Non-capture PEM behavior is unchanged
(`forthCapIsOpen()` false).

## C4 — programming/manage.c: the toggle arm (E10/E11)

At the TOP of `insertStepInProgram`'s first arm (before the
`forthEntryStateAtInsertion()` / `tam.function` block):

```c
    if(func == ITM_AIM && forthCapIsOpen()) {
      /* K1/E10-E11: the ALPHA gesture toggles alpha<->keys while a capture
       * line is open.  Gated on forthCapIsOpen() so E6 (ITM_AIM re-entry
       * with the capture CLOSED) is untouched.  K-R3: keys mode shows the
       * underlying menus — the visible row swap IS the mode indicator. */
      if(forthCapKeysMode()) {
        forthCapSetKeysMode(false);
        showSoftmenu(-MNU_ALPHA);
      }
      else {
        forthCapSetKeysMode(true);
        _closeAlphaMenus();
      }
      pemCursorIsZerothStep = false;
      return;
    }
```

`_closeAlphaMenus` and `showSoftmenu` are already used in this file. The
buffer and step are untouched — the per-key recommit invariant holds across
a toggle by construction.

## Tests (append to test_capture.part.h; register in test_dict_reloc.c after the FIX-9 group, banner "FORTH K1 TESTS (keys-mode toggle)")

Reuse the landed fixture idiom exactly (testProg_t: tpInit/tpLbl("K1")/
tpMarker/tpRtn/tpWrite; full state reset incl. programRunStop/
dynamicMenuItem/nextChar/shiftF/shiftG; fnGotoDot(2); runFunction(ITM_AIM)
opens the capture; save/restore the full global set incl. softmenuStack —
copy the shape of test_resume_drains_buried_catalog).

**T1 `test_keys_mode_resolution`** — the column swap at determineItem level.
Locate the ALPHA-gesture row once:
`int kIdx = -1; for (int i = 0; i < 37; i++) if (kbd_std[i].fShifted == ITM_AIM) { kIdx = i; break; }`
(STOP if not found). `char kb[3]; sprintf(kb, "%02d", kIdx);`
`extern int16_t determineItem(const char *);` FLAG_USER must be clear.
 - sc1 (E10, alpha sub-mode): capture open, bit off, `shiftF = true;` →
   `determineItem(kb) == ITM_AIM` (pre-K1 this resolves to the aim column's
   ITM_alpha — this subcase is the packet's red-first evidence).
 - sc2 (swap): `forthCapSetKeysMode(true); shiftF = false;` →
   `determineItem(kb) == kbd_std[kIdx].primary` (read the expectation live
   from the table — differential, layout-independent).
 - sc3 (symmetric toggle-out resolution): bit still set, `shiftF = true;`
   → `determineItem(kb) == ITM_AIM` (now via the normal fShifted column).
 - sc4 (no leak outside capture): abort the capture (CLA+BACKSPACE idiom),
   bit forced on via accessor, `shiftF = true;` →
   `determineItem(kb) == kbd_std[kIdx].fShifted` i.e. ITM_AIM is fine, but
   the ALPHA-branch remap must NOT have fired from the capture gate — 
   assert `forthCapIsOpen()` is false and resolution used normal columns
   (CM_PEM + FLAG_ALPHA clear resolves normal anyway); then clear the bit.
 After each determineItem call reset `shiftF = false` (the tail consumed it;
 belt and braces).

**T2 `test_keys_mode_toggle_arm`** — the C4 arm through the real dispatch.
 - sc1: capture open (alpha), `runFunction(ITM_AIM)` → bit set,
   `currentMenu() != -MNU_ALPHA`, capture still FCAP_OPEN, aimBuffer
   unchanged, on-disk step unchanged (recommit invariant: compare step
   bytes before/after toggle).
 - sc2: `runFunction(ITM_AIM)` again → bit clear, `currentMenu() ==
   -MNU_ALPHA`, capture still FCAP_OPEN.
 - sc3 (E6 untouched): close the capture (empty abort), cursor still in
   region, `tam.function = 0`, then `runFunction(ITM_AIM)` → capture
   REOPENS (FCAP_OPEN, tam.function == ITM_FORTH, bit CLEAR — fresh open
   starts alpha).

**T3 `test_keys_mode_nav_guards`** — the C3 guards through processKeyAction.
`extern void processKeyAction(int16_t);`
 - sc1 (R/S with text): capture open, `runFunction(ITM_2)`, set bit via
   the real toggle (`runFunction(ITM_AIM)`), then
   `processKeyAction(ITM_RS)` → capture FCAP_CLOSED, bit clear, and the
   program now holds: marker, source step "2" (0x8B 0x1A 0xFD 0x01 '2'),
   STOP step (single byte 0x46 = ITM_STOP 70) — walk from the LBL step
   with findNextStep and assert all three; `keyActionProcessed` true.
 - sc2 (SST on empty line): fresh fixture; open capture, toggle to keys,
   `processKeyAction(ITM_SST)` → FCAP_CLOSED, bit clear, placeholder gone
   (step count == fixture count), no error.
 - sc3 (negative control): capture CLOSED, plain PEM,
   `processKeyAction(ITM_RS)` → exactly one STOP step added (upstream
   behavior untouched).

**T4 — extend `test_capture_close_paths_reset_tuple`** (the FIX-8 class
sweep, E14): in each subcase, poison `forthCapSetKeysMode(true)` right
after the capture opens, and add a fifth tuple line asserting
`!forthCapKeysMode()` after the close. Also extend
`test_capture_suspend`-style coverage inline in T2 or T3 is NOT needed —
the suspend interim is pinned by adding one assertion to T3 sc1? No:
add **sc4 to T3**: capture open, toggle to keys, `runFunction(ITM_STO)`
(physical-shaped TAM entry) → suspended, assert `!forthCapKeysMode()`
(E13 interim: suspend clears); `fnKeyExit(NOPARAM)` to cancel TAM; resume
returns alpha (`currentMenu() == -MNU_ALPHA`, FCAP_OPEN).

## Mutations (apply, show RED, revert — record the RED lines)

- M1: in C2a, delete `&& !(tam.function == ITM_FORTH && forthCapKeysMode())`
  → T1 sc2 red (aim column resolves instead of primary).
- M2: in C4, change the gate to `if(func == ITM_AIM)` (drop
  `forthCapIsOpen()`) → T2 sc3 red (E6 re-entry becomes a toggle;
  tam.function stays 0 / capture does not reopen). The landed
  `test_forth_alpha_gesture_resumes_forth` should co-red — note if it does.
- M3: in C1, delete the `keysMode = 0` clear from `forthCapClose()` →
  T4 (class sweep, any subcase) red on the fifth tuple line.
- M4: in C3, delete the `ITM_RS` guard → T3 sc1 red (capture stays open
  and/or "STOP " lands as text, no 0x46 step).

## Acceptance

Final gate green (`ALL PASSED` + `BUILD + SELF-TEST GREEN` + upstream
suite line), all four mutations shown red and reverted, PASS-set diff vs
a pre-edit baseline log shows ONLY the new K1 lines, arena line reported.
Deliver: files changed, per-test PASS lines, mutation RED lines, arena
line, and any STOP/deviation notes. Do NOT commit — leave the worktree
dirty for architect review.

---

## AMENDMENT K1-A (2026-08-04, post-implementation — two architect spec defects, both caught independently by BOTH bench implementers)

1. **C2's `processKeyAction` export instruction was wrong and dangerous.**
   `processKeyAction` is NOT file-static — it is declared non-static in
   upstream `keyboard.h:16` and called cross-file (screen.c:900). Wrapping
   it in `FORTH_SELFTEST_EXPORT` would have made it `static` in PRODUCTION
   builds and broken the hardware link — a defect the sim-only gate cannot
   catch (the sim always defines FORTH_DEBUG_SELFTEST). Correct action
   (taken): export `determineItem` only; T3 declares a plain extern.
   Authoring lesson: before instructing an export, grep the upstream header
   for an existing non-static declaration.

2. **T3 sc1's step-adjacency expectation ignored the pre-move regate.**
   After the RS guard's `pemCloseAlphaInput()` clears FLAG_ALPHA and
   empties aimBuffer, `addStepInProgram`'s pre-move regates and the native
   STOP lands AFTER the fixture's RTN — identical to upstream PEM
   commit-then-insert semantics (consistent with ruling K-R4). Layout is
   LBL / marker / src / RTN / STOP. The landed oracle asserts the exact
   source bytes at step 3 plus exactly-one-ITM_STOP by full program walk —
   stronger than the original adjacency check. M4 confirms it kills.

Both implementations (opus 28.4 min / sonnet 40.9 min, both green, 4/4
mutations red, identical defect findings) are recorded in the stage ledger;
the opus diff was adopted (first-pass green, stronger T3 oracle).
