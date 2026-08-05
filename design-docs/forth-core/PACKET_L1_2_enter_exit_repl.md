# PACKET L1-2 — ENTER runs the line, EXIT ladder, input cap

**Stage L packet 2** (design: STAGE_L_INTERACTIVE.md, rulings L-R3, L-R5→L-R7,
L-R8; evidence: STAGE_L_TRACES.md §T2, §T4, §T8.4). **Prerequisite: L1-1
landed and green.**

**Scope.** The interactive capture becomes a REPL: ENTER interprets the
line and reopens empty; EXIT unwinds a ladder and leaves; the 256-byte /
196-glyph cap is enforced on the interactive alpha path. This packet
**replaces** L1-1's `_forthCapCloseIfInteractive` stopgap with the real
ladder. It does NOT add the dispatch divert (L1-3), the history program
(L1-H), or the fold (L1-F*).

## Implementer contract

Identical to PACKET_L1_1's contract — same worktree rule, same gate, same
STOP conditions, same fixture rules, same reporting (`FORTH ARENA`, flash
delta, verbatim PASS lines, mutations RED-then-reverted). Re-read it there;
it is not repeated.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthCapIsInteractive" packages/forth-core/forth_capture.h    # L1-1 landed
grep -n "_forthCapCloseIfInteractive" packages/forth-core/keyboard.c   # L1-1 stopgap present
grep -n "case CM_AIM: {" packages/forth-core/keyboard.c                # record all
grep -n "void forthOuterInterpret" packages/forth-core/forth_compile.c
grep -n "stringGlyphLength(aimBuffer) < 196" packages/forth-core/programming/manage.c
grep -n "forthCapSetKeysMode(false)" packages/forth-core/keyboard.c    # the PEM E12.4 rung
```

## C1 — ENTER (`fnKeyEnter`, `case CM_AIM`, keyboard.c:3513)

Today that arm pops softmenus, calls `calcModeNormal()`, and on a
non-empty `aimBuffer` reallocates X as a `dtString`, copies the buffer in,
and lifts the stack. **None of that is wanted.** Divert as the first
statement of the arm:

```c
      case CM_AIM: {
        if(forthCapIsInteractive()) {
          forthInteractiveEnter();
          break;
        }
        /* … landed native-AIM body unchanged … */
```

`forthInteractiveEnter()` is new, in `programming/manage.c` beside the
other capture orchestrators (it needs no PEM statics, but keeping the
capture orchestrators in one file is the landed convention — F6-2's
`forthCaptureSuspend`/`Resume` live there for the same reason). Declared
in `forth_capture.h`.

```
forthInteractiveEnter():
  if aimBuffer[0] == 0:
      /* Empty ENTER is a no-op, NOT a close.  EXIT is the documented
       * close gesture (C2); an empty line has nothing to run and nothing
       * to keep.  Mirrors E3's empty-commit rule in spirit without its
       * step deletion, of which there is none here. */
      return

  /* E9 tier 1: refuse the commit atomically, capture stays open with the
   * line intact for correction, error already displayed.  Same gate the
   * PEM ENTER arm uses (manage.c:1025). */
  if !forthCheckSourceLine(aimBuffer):
      return

  /* L1-H fills this in; until then it is an empty inline function.
   * Push BEFORE the run: an executed word can rewrite aimBuffer
   * (it is also the NIM buffer, §3.3.2), so the text must be captured
   * while it is still the user's line. */
  forthHistoryPush(aimBuffer)

  /* The copy is mandatory and already exists: forthOuterInterpret
   * memcpy's into ctx.source before running (forth_compile.c:1591-1603 (the FORTH_SOURCE_MAX check is at :1597, the memcpy at :1601)).
   * Do NOT hand it aimBuffer expecting it to be stable. */
  forthOuterInterpret(aimBuffer)

  if lastErrorCode != ERROR_NONE:
      /* L5: reopen with the line intact so the user edits rather than
       * retypes.  lastErrorCode / the message surface as today (§8.7,
       * S1 ruling: no on-screen token display).  aimBuffer may have been
       * rewritten by a partially-executed line, so restore from the
       * history entry L1-H just pushed — until L1-H lands, from a local
       * 256-byte copy taken before the run. */
      restore aimBuffer from the pre-run copy
      T_cursorPos = stringLastGlyph(aimBuffer) + 1   /* non-empty by construction */
      return                                          /* capture stays OPEN */

  /* L-R3: REPL.  Reopen empty, stay in CM_AIM.  forthCapOpenInteractive
   * clears aimBuffer and resets keysMode (E14/K1: a fresh capture opens in
   * alpha input, matching the PEM E5 relock). */
  forthCapOpenInteractive()
  T_cursorPos = 0
  displayAIMbufferoffset = 0
```

**The pre-run copy is not optional and not a nicety.** §3.3.2 is
normative: executed words can rewrite `aimBuffer` mid-line because it is
also the NIM buffer (src/c47/c47.c:132). The error path reads the line
back, so it must read it from somewhere the run could not touch.

**`calcMode` stays `CM_AIM` throughout.** Do not call `calcModeNormal()`,
`closeAim()`, or `popSoftmenu()` on this path.

## C2 — EXIT (`fnKeyExit`, `case CM_AIM`, keyboard.c:3868)

The landed arm has two rungs already: alpha-submenu → `popSoftmenu();
stayInAIM();`, else `closeAim()` + `saveForUndo()`. The interactive
ladder inserts above them and is the E8 analog (T4), extending K's E12.4
rung shape (keyboard.c:3926-3933 is the PEM original — copy its shape,
do not share code, the surrounding teardown differs):

```c
      case CM_AIM: {
        if(forthCapIsInteractive()) {
          /* Rung 1 (E12.4 analog): keys mode -> alpha input. */
          if(forthCapKeysMode()) {
            forthCapSetKeysMode(false);
            showSoftmenu(-MNU_ALPHA);
            break;
          }
          /* Rung 2: anything stacked above the base pops and the capture
           * stays open.  This is the NATIVE CM_AIM test (keyboard.c:3869-3873,
           * 3884-3886) adopted verbatim, INCLUDING its pre-normalisation —
           * rev 2 used isAlphaSubmenu(0), which is narrower, so a non-alpha
           * menu (STK, FIN, a catalog) stacked over the capture fell through
           * to rung 3 and silently discarded the line.  The pre-normalisation
           * is what makes -MNU_ALPHA-on-top (the ordinary capture state) fall
           * THROUGH to rung 3 instead of popping: it retargets slot 0 to
           * MyAlpha so the predicate reads "base menu displayed". */
          if(currentMenu() == -MNU_ALPHA) { softmenuStack[0].softmenuId = 1; }
          if(!(softmenuStack[0].softmenuId <= 1 && menu(1) != -MNU_ALPHA)) {
            popSoftmenu();
            stayInAIM();                     /* native pair, keyboard.c:3885-3886 */
            break;
          }
          /* Rung 3: close.  L-R2 consequence + L-R7: a non-empty line is
           * pushed to history BEFORE the close, so EXIT never loses it. */
          if(aimBuffer[0] != 0) {
            forthHistoryPush(aimBuffer);     /* L1-H fills this in */
          }
          forthCapClose();
          aimBuffer[0] = 0;
          T_cursorPos = 0;
          displayAIMbufferoffset = 0;
          calcModeNormal();
          popSoftmenu();                     /* see below — rev 2 removed this
                                                and rev 3 puts it back */
          break;
        }
        /* … landed native-AIM body unchanged … */
```

**Rung 3 must NOT call `closeAim()`** — that commits `aimBuffer` to X as a
`dtString` (src/c47/bufferize.c:2693-2712), which is exactly the native
behaviour the Forth capture exists to avoid. Confirm by test (C5.4).

**The teardown is `calcModeNormal()` FOLLOWED BY `popSoftmenu()`** —
exactly `closeAim()`'s own shape (src/c47/bufferize.c:2693-2695) minus its
string commit. **Rev 2 removed the pop and rev 2 was wrong**; the
implementing pass caught it with C5.6b red. The sequence:

- Rung 2's pre-normalisation renames slot 0 to id 1 **in place** — it does
  not pop. And `softmenu[1].menuItem` is `-MNU_MyAlpha`
  (src/c47/softmenus.c:1039), **not** `-MNU_ALPHA`.
- So `calcModeNormal()`'s own pop, guarded on
  `softmenu[softmenuStack[0].softmenuId].menuItem == -MNU_ALPHA`
  (src/c47/calcMode.c:45), can never fire here. Only its second check
  (id 1 → 0) runs — another in-place rename.
- Two renames, zero pops: the `-MNU_ALPHA` frame pushed at open survives,
  and the user's pre-FORTH menu stays buried under it.

Rev 2's reasoning ("`calcModeNormal` already pops") was true in isolation
and false in sequence, because the same revision *also* added the
pre-normalisation that falsifies its guard. Two individually defensible
edits, jointly wrong. `clearSystemFlag(FLAG_ALPHA)` does stay deleted —
`calcModeNormal` clears it at src/c47/calcMode.c:53.

**No `undo()`, no `saveForUndo()`, no `updateMatrixHeightCache()`** — the
native arm runs those (keyboard.c:3874-3879) because `closeAim` either
commits the buffer to X or `undo()`s the placeholder that `calcModeAim`'s
`liftStack()` created. **T9 removes that placeholder entirely**: the
interactive open does not lift (PACKET_L1_1 C2b), so X is untouched from
FORTH-press to EXIT and there is nothing to resolve. Calling `undo()` here
would be actively wrong — it would roll back whatever the user's ENTER'd
lines did to the stack. Say so in a comment at the site, so a later reader
comparing against the native arm does not "restore" the omission.

**Delete `_forthCapCloseIfInteractive` and its call sites** from L1-1 —
the ladder supersedes it. **But first** enumerate L1-1's reported
`closeAim()` sites and, for each one the ladder does NOT cover (BST/SST at
keyboard.c:2889, the catalog pick at :1300, `fnKeyUp`/`fnKeyDown` at
:4653/:4871), decide explicitly and record the decision in your report:
either keep a guarded close there, or state why that path cannot be
reached with an interactive capture open. **An unreported deletion of a
close site is a STOP condition** — L1-1's whole rationale was that a
leaked `FCAP_OPEN` corrupts the next PEM ALPHA press (manage.c:1719-1734).

## C3 — R/S runs the line

`ITM_RS` in `CM_AIM` reaches `processAimInput` (keyboard.c:498), falls
through every arm, and would land in L1-3's divert as a `CAT_FNCT` item —
typing the text `R/S`, an unresolvable token. K's E12 rule (commit, then
record a native STOP step, keyboard.c:3222-3229) has no interactive
analog: there is no step.

In `processKeyAction`'s `case CM_AIM` (keyboard.c:2886-2898), before the
`processAimInput` call:

```c
          if(forthCapIsInteractive() && item == ITM_RS) {
            forthInteractiveEnter();     /* the closest honest analog */
            keyActionProcessed = true;
            break;
          }
```

## C4 — the input cap

PEM's capture insert enforces `len < 256 - inputCharLength &&
stringGlyphLength(aimBuffer) < 196` (manage.c:983). `addItemToBuffer`
does not — it bounds on `AIM_BUFFER_LENGTH` (1024,
src/c47/bufferize.c:466). So an interactive line would accept up to 1024
bytes and then be refused by `forthOuterInterpret`'s
`n >= FORTH_SOURCE_MAX` check (forth_compile.c:1595) **at ENTER** — a
silent-until-ENTER failure and a divergence from PEM.

Guard at the call sites rather than inside `addItemToBuffer` (which is
upstream and not overridden). **There are TWO seams, not one** — the
physical-key path and the softkey path — and a guard on only the first
leaves softkey character insertion uncapped (keyboard.c:1443-1445 is the
live `CM_AIM` softkey-character tail, and `keyboard.c:1300-1302` confirms
alphabetic softmenu presses in `CM_AIM` do not `closeAim()` and fall
through to `runFunction`). Factor one helper and call it at both:

```c
/* True when the interactive capture cannot take another character.
 * item > 0 is LOAD-BEARING, not defensive: determineItem returns NEGATIVE
 * softmenu ids in CM_AIM (e.g. -MNU_AIMCATALOG for the f-shifted catalog
 * gesture, src/c47/assign.c:46), they reach processKeyAction's default arm
 * and then case CM_AIM, and indexOfItems[negative] is out of bounds.
 * (The landed keyboard.c:2794 tests `... || item < 0` for the same reason;
 * that site's shape is a latent wart, not the pattern to copy.) */
static bool_t _forthCapAtCap(int16_t item) {
  if(!forthCapIsInteractive() || item <= 0) { return false; }
  if(indexOfItems[item].func != addItemToBuffer) { return false; }
  return !(stringByteLength(aimBuffer)
             + stringByteLength(indexOfItems[item].itemSoftmenuName) < 256
           && stringGlyphLength(aimBuffer) < 196);
}
```

Seam 1 — `processKeyAction`'s `case CM_AIM`, before `processAimInput(item)`:

```c
          if(_forthCapAtCap(item)) {
            keyActionProcessed = true;    /* full: swallow the key, no error */
            break;
          }
```

Seam 2 — `executeFunction`, immediately before `runFunction(item)`
(keyboard.c:1415):

```c
                if(calcMode == CM_AIM && _forthCapAtCap(item)) { goto noMoreToDo; }
```

Use whatever the surrounding control flow there actually permits — if
`goto noMoreToDo` is not reachable from that point, report it and use the
equivalent skip. **C5.7 must drive the cap through BOTH seams.**

Silently swallowing matches PEM, which simply does not insert when the cap
is hit (manage.c:983's `if` has no else). Do not add an error.

## C5 — tests (`test_capture_interactive_repl`, new; register per L1-1 C4)

0. **T9 end-to-end (moved here from L1-1, which cannot run ENTER).** Put
   `16` in X, open interactive, type `1 +` through the real key path,
   ENTER; assert **X == 17**. This is the assertion that proves the
   interactive line operates on the live stack. L1-1 pins the open half by
   register snapshotting; this closes it.
1. **ENTER runs and reopens empty.** Open interactive, type `1 2 +`
   through the real key path, ENTER via `fnKeyEnter(NOPARAM)`. Assert
   X == 3, capture still `FCAP_OPEN`, `forthCapIsInteractive()`,
   `aimBuffer[0] == 0`, `T_cursorPos == 0`, `calcMode == CM_AIM`.
2. **Empty ENTER is a no-op.** With an empty line, ENTER; assert still
   open, `calcMode == CM_AIM`, X unchanged.
3. **Error reopens with the line intact.** ENTER `1 ZZQQ +`; assert
   `lastErrorCode != ERROR_NONE`, capture open, and `aimBuffer` still
   holds `1 ZZQQ +` with `T_cursorPos` at the end.
4. **EXIT does not commit to X.** Put a known long integer in X. Open
   interactive, type `ABC`, EXIT via `fnKeyExit(NOPARAM)`; assert
   `FCAP_CLOSED`, `!getSystemFlag(FLAG_ALPHA)`, `calcMode == CM_NORMAL`,
   and **X is bit-identical to its pre-FORTH value** — same type, same
   value, read back through the landed `read_reg_int32` idiom, not merely
   "not a string". With T9's non-lifting open this must hold exactly; if
   it does not, the open is still lifting and that is a STOP.
5. **Ladder rung 1.** Open interactive, set keys mode via
   `forthCapSetKeysMode(true)`, EXIT; assert keys mode off, capture still
   OPEN, line intact.
6. **Ladder rung 2, two cases.** (a) Push an alpha submenu, EXIT; assert
   it popped and the capture is still open. (b) Push a **non-alpha** menu
   (e.g. STK), EXIT; assert it popped and the capture is **still open with
   the line intact** — rev 2's narrower `isAlphaSubmenu(0)` predicate
   closed the capture and discarded the line here.
6b. **EXIT preserves the pre-FORTH menu.** With a non-default menu open,
   press FORTH, then EXIT through rung 3; assert the original menu is
   current again (rev 2's extra `popSoftmenu()` destroyed it).
7. **Cap, both seams.** Drive 196 glyphs in, then one more via the
   physical-key seam; assert no growth, no error. Repeat the last insert
   via the **softkey** seam (`executeFunction` → `runFunction`) and assert
   the same. Then drive a negative item id (the f-shifted AIM-catalog
   gesture) into `processKeyAction` with a capture open and assert no
   out-of-bounds read — the `item > 0` conjunct.
8. **R/S runs the line.** Type `2 3 *`, drive `ITM_RS` through
   `processKeyAction`; assert X == 6 and the capture reopened empty.
9. **A word that rewrites aimBuffer.** Run a line whose execution writes
   `aimBuffer` (any item using the NIM buffer), then force the error path,
   and assert the restored line is the user's original — this is the
   §3.3.2 pin. If you cannot find such an item, report that and pin the
   copy by mutation 4 alone.

## Mutations

1. Delete the `forthCapIsInteractive()` divert in `fnKeyEnter`. RED at C5.4
   (X gets the string) and C5.1.
2. Call `closeAim()` instead of the rung-3 teardown. RED at C5.4 (X gets
   the string instead of its pre-FORTH value).
2b. **REMOVE** the trailing `popSoftmenu()` from rung 3. RED at C5.6b —
   the pre-FORTH menu stays buried. (Rev 2 had this mutation inverted,
   which is how the defect survived review: the packet predicted red for
   the correct code.)
2c. Narrow rung 2 back to `isAlphaSubmenu(0)`. RED at C5.6(b).
2d. Drop the `item > 0` conjunct from `_forthCapAtCap`. RED at C5.7's
   negative-id case (or a sanitiser/ASAN report — if the harness cannot
   observe the out-of-bounds read, say so rather than deleting it).
3. Skip `forthCheckSourceLine`. RED at a new subcase driving a line the
   E9 tier-1 check rejects.
4. Use `aimBuffer` directly on the error path instead of the pre-run copy.
   RED at C5.9; if C5.9 could not be written, report this mutation as
   unpinned rather than deleting it.
5. Remove the cap guard from seam 1. RED at C5.7's physical-key case.
5b. Remove it from seam 2 only. RED at C5.7's softkey case — this is the
   mutation that proves the second seam is real.
6. Make empty ENTER close the capture. RED at C5.2.

## Out of scope

- The dispatch divert / keys-mode routing — L1-3. (C3's R/S guard and
  C4's cap guard sit in the same `case CM_AIM` block L1-3 will extend;
  that is deliberate and not a conflict.)
- `forthHistoryPush` beyond an empty stub — L1-H.
- Any fold/TAM behaviour — L1-F*.
- Up/down recall — L1-H (gesture ruled: f-shifted arrows, T4).

## Acceptance

- Gate green, landed F6/K suite unchanged, PASS lines quoted.
- The `closeAim()` site disposition table from C2 reported.
- Six mutations RED-then-reverted (or reported unpinned with evidence).
- Flash + arena reported.
- **Sim:** FORTH, type `1 2 +`, ENTER, see 3 on the stack and an empty
  line; EXIT returns to normal with X still 3. Capture via `run-sim`,
  copy-adapting `references/capture-driver.c`.

  **Settle this open item while you are there (L1-1, 2026-08-05):** L1-1's
  sim pass confirmed the alpha menu appears on FORTH and that EXIT
  returns to the normal screen, but **the typed glyphs did not visibly
  render** in the headless capture even though `aimBuffer` and
  `T_cursorPos` were programmatically correct at that instant. Not root-
  caused; not an L1-1 regression (that packet changed no display code).
  Two candidates: a `screenUpdatingMode` capture trap — the `run-sim`
  skill's rule 5 documents exactly this, an earlier battery test leaving
  the mode manual so `refreshScreen()` draws nothing — or a real gap in
  T5's "zero new display code" claim. **T5 is the stage's justification
  for hosting on AIM at all, so this must be settled, not deferred
  again.** If it is the trap, say so with the mode value; if the line
  genuinely does not render, STOP and report — that reopens L-R8.
