# SPEC — the consolidation wave (10 packets), 2026-08-09

Implements every item of the 2026-08-09 minimality review
(`REVIEW_upstream-minimality_2026-08-09.md`) and the follow-up
consolidation assessment, owner-approved the same day ("all of them").
Written for the implementer: zero unstated decisions. Where this spec says
STOP, stop and report — do not improvise past a mismatched anchor.

## Ground rules — binding for every packet

1. **Edit only the flat working area** (`packages/forth-core/<file>` and
   new sources there). NEVER edit `patches/` or `files/` — they are
   generated. After every edit cycle:
   `python3 tools/pkg_patch_refresh.py packages/forth-core`.
2. **Gate** = `./packages/forth-core/build-test.sh` (runs refresh first).
   It must be GREEN at every commit. Where a packet names the upstream
   testSuite, run `make test CUSTOM_PKG=packages/forth-core` too.
3. **One packet = one commit**, in packet order (P1 … P10). Commit message
   names the packet, e.g. `CONSOLIDATE P3: one abort helper for the two
   identical PEM abort sequences`. Record RULE-1 numbers in every commit
   that rebuilds firmware: flash delta from
   `make dmcp5r47 CUSTOM_PKG=packages/forth-core CUSTOM_PKG_RECONFIGURE=1`
   (verify not-stock: `arm-none-eabi-nm ... | grep -c forthConsole` ≈ 19),
   ram, arena (untouched — nothing here changes the dictionary).
4. **Byte-identical discipline** (DESIGN.md:1854): no packet may create a
   NEW modified upstream line beyond those this spec names. After every
   packet:
   ```
   python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
       packages/forth-core/patches/*.patch
   ```
   From P1's commit onward this must exit 0 (no WS-ONLY/COMMENT-ONLY).
5. **Anchors**: quoted code below is the working-area state at
   `88703343f`. If an anchor does not match what you see, STOP and report
   the mismatch. Line references like `manage.c:1002` mean the PACKAGE
   working copy `packages/forth-core/...` unless prefixed `src/c47/`.
6. **Refactor equivalence proof**: P2-P9 are behavior-neutral by intent.
   Proof per packet = gate green + upstream testSuite green + any packet-
   specific mutation named below, run and REPORTED (a mutation that fails
   to redden is reported as a documented gap, not silently dropped).

---

## P1 — churn zeroing (51 mechanical findings → 0)

Fix every WS-ONLY and COMMENT-ONLY hit the scanner reports, plus two
judged NEAR hits. Behavior-neutral by construction; no new symbols.

**Class 1 — wrap-reindent (restore upstream indentation).** Two sites
re-indented upstream lines inside a new `if/else`; restore the enclosed
upstream lines to their exact upstream byte content (indentation
included), keeping the added `if (...) { ... }` / `else {` / `}` lines.
The landed pattern to copy is screen.c's F7 guard (the `else {` line and
closing `}` are added lines; everything between is byte-identical
upstream):

- `keyboard.c`, `determineItem`, the plane-selection body (the block from
  `result = shiftF ? key->fShiftedAim :` through
  `result = -MNU_EIMCATALOG;` `}`): all ~20 upstream lines back to
  upstream indentation. The added `if(...ITM_AIM toggle...) { ... }` arm
  and the `else {` / closing `}` remain added lines.
- `manage.c`, `pemAlpha`, the `addItemToBuffer` arm: same treatment for
  the ~13 upstream lines from `item = numlockReplacements(...)` through
  `T_cursorPos += inputCharLength;` and braces.
- `c47Extensions/keyboardTweak.c`: the `fnExitAllMenus(0);` line back to
  its upstream indentation inside the added guard.
- `items.c`: the two `}` lines the scanner flags.

**Class 2 — comment-attach (move to own added line).** For each, restore
the upstream line byte-identically and put the comment on its own added
line ABOVE it:

- `keyboard.c`: three `leaveTamModeIfEnabled();  /* D7-1 ... */` sites
  (GTOP-catalog arm, SYSFL cancel arm, the CM_PEM-adjacent arm).
- `manage.c`: `pemCloseAlphaInput();`, `defineFirstDisplayedStep();`,
  `_closeAlphaMenus();`, `return;` (the E9 arm), and
  `tam.function = ITM_LITERAL;`. The three ENTER-arm lines share one
  wrapped comment — merge it into one comment block above the first line.
- Note: `//--firstDisplayedLocalStepNumber;` is upstream's own commented
  line — restore byte-identical.

**Class 3 — alignment/paren churn (revert exactly):**

- `manage.c`: `if((aimBuffer[0]) == 0)` → `if(aimBuffer[0] == 0)`;
  the re-indented `char cursorByte = aimBuffer[T_cursorPos];` back to
  upstream's `        char cursorByte ...`.
- `softmenus.c`: `menu_ALPHA`'s `-MNU_MyAlpha ...` continuation row back
  to upstream's exact spacing (the row content is unchanged; only your
  added fourth row and the modified `CHR_case` row terminator are edits).
- `softmenus.c`: revert the `/* 022 */`→`/* 023 */` comment renumber on
  the `-MNU_TAMFLAG` row — the row keeps its upstream comment number; the
  inserted `-MNU_FORTH` row keeps `/* 022 */` (positional comments are
  fiction after a mid-table insert; do not renumber anything).

**Verify**: scanner exits 0; gate green; upstream testSuite green.
Expected effect: added/deleted line counts drop; no shadow-tree behavior
change.

---

## P2 — one insert-name eligibility predicate

**New function** in `packages/forth-core/forth_capture.c`, declared in
`forth_capture.h` (put the declaration beside `forthCapInsertName`):

```c
/* CONSOLIDATE P2: the one spelling of "can this item be inserted into a
 * capture as text".  Two sites had forked copies: pemAlpha's PEM arm and
 * runFunction's interactive divert, whose exclusion lists differed
 * because pemAlpha's earlier arms consume ENTER/BACKSPACE/EXIT/R-S
 * before its copy ran.  That coupling was implicit and cross-file; it is
 * now this function.  item > 0 is LOAD-BEARING at both sites:
 * determineItem returns negative softmenu ids and
 * indexOfItems[negative] is out of bounds (the L1-2 _forthCapAtCap
 * precedent).  interactive == the runFunction divert (raw key stream);
 * false == the PEM pemAlpha arm (console keys already consumed). */
bool_t forthCapNameInsertEligible(int16_t item, bool_t interactive) {
  if(item <= 0)                                              { return false; }
  if((indexOfItems[item].status & CAT_STATUS) != CAT_FNCT)   { return false; }
  if((indexOfItems[item].status & PTP_STATUS) != PTP_NONE)   { return false; }
  if(item == ITM_AIM || item == ITM_FORTH)                   { return false; }
  if(interactive && (item == ITM_ENTER || item == ITM_EXIT1
                     || item == ITM_BACKSPACE || item == ITM_RS)) {
    return false;
  }
  return true;
}
```

**Call site 1**, `manage.c` `pemAlpha` (current form):
```c
    else if(forthCapIsOpen()
            && (indexOfItems[item].status & CAT_STATUS) == CAT_FNCT
            && (indexOfItems[item].status & PTP_STATUS) == PTP_NONE
            && item != ITM_AIM && item != ITM_FORTH) {
```
becomes:
```c
    else if(forthCapIsOpen() && forthCapNameInsertEligible(item, false)) {
```
(keep the arm's body and its fall-through comment unchanged). The
`item <= 0` conjunct is NEW hardening at this site — state that in the
commit message.

**Call site 2**, `items.c` `runFunction` divert (current form):
```c
        if((indexOfItems[func].status & CAT_STATUS) == CAT_FNCT
           && (indexOfItems[func].status & PTP_STATUS) == PTP_NONE
           && func != ITM_AIM && func != ITM_FORTH
           && func != ITM_ENTER && func != ITM_EXIT1
           && func != ITM_BACKSPACE && func != ITM_RS) {
```
becomes:
```c
        if(forthCapNameInsertEligible(func, true)) {
```
The enclosing `if(forthCapInteractiveLive() && func > 0)` stays unchanged
(its `func > 0` also guards the ITM_AIM toggle arm above the insert arm).

**Test**: add a truth-table subcase to the capture battery
(`test_capture.part.h`, beside the existing insert-name cases) calling
the predicate directly (it is public): eligible(a known
CAT_FNCT/PTP_NONE item, both modes) == true; ITM_ENTER: false
interactive, true non-interactive (artifact of the site split — say so in
the comment); ITM_AIM/ITM_FORTH: false both; a negative id: false both.
**Mutation** (run, report, revert): drop the `interactive &&` exclusions —
the truth-table subcase must redden.

---

## P3 — one abort sequence in manage.c

The backspace-to-empty arm and `pemCloseAlphaInput`'s early arm contain
the identical 6-statement sequence. Add one file-static helper in the
manage.c override, placed directly above `pemAlpha`:

```c
/* CONSOLIDATE P3: the empty-capture PEM abort — delete the §8.1
 * placeholder, drop ALPHA, restore the normal GUI, close the capture.
 * tam.function reset rationale/citations: see the ITM_BACKSPACE arm's
 * original comment (idle value 0 matches the zero-initialized boot
 * state; tam.mode, not tam.function, is the "in TAM" gate). */
static void _forthCapAbortPemInput(void) {
  deleteStepsFromTo(currentStep, findNextStep(currentStep));
  clearSystemFlag(FLAG_ALPHA);
  calcModeNormalGui();
  _closeAlphaMenus();
  forthCapClose();
  tam.function = 0;
}
```

Replace both sequences with `_forthCapAbortPemInput(); return;` — in the
`ITM_BACKSPACE` `aimBuffer[0] == 0` arm, and in `pemCloseAlphaInput`'s
`tam.function == ITM_FORTH && !forthCapTextNonEmpty()` arm. Keep each
site's one-line pointer comment; the long citations live once, at the
helper. `pemCloseAlphaInput`'s TAIL (commit path: `forthCapClose()` +
conditional `tam.function = 0`) is NOT one of the two — leave it.

---

## P4 — one SST/BST empty-placeholder abort in keyboard.c

Two identical guarded blocks before `fnSst(NOPARAM)` / `fnBst(NOPARAM)`.
Add beside `_forthCapAtCap` (top of keyboard.c override):

```c
/* K1/E12.2 (CONSOLIDATE P4): abort the empty placeholder before SST/BST
 * navigation — no navigation may leave FCAP_OPEN behind (fnSst/fnBst's
 * own close branch only fires on non-empty aimBuffer). */
static void _forthCapAbortEmptyPlaceholder(void) {
  if(forthCapIsOpen() && aimBuffer[0] == 0) {
    pemAlpha(ITM_BACKSPACE);
  }
}
```

Both sites become a bare `_forthCapAbortEmptyPlaceholder();` above the
existing `fnSst(...)`/`fnBst(...)` lines.

---

## P5 — one SELFTEST_EXPORT macro, in the package header

1. Add to `packages/forth-core/forth_capture.h` (near the top, after the
   includes guard):
   ```c
   /* CONSOLIDATE P5: file-statics exported to the self-test build only;
    * production linkage unchanged.  One definition — keyboard.c and
    * screen.c each carried a private copy of this idiom. */
   #if defined(FORTH_DEBUG_SELFTEST)
     #define FORTH_SELFTEST_EXPORT
   #else
     #define FORTH_SELFTEST_EXPORT static
   #endif
   ```
2. Delete the 5-line `#if/#define/#else/#define/#endif` block from the
   keyboard.c override (keep its explanatory comment, trimmed of the
   "defined up here" sentence) and the equivalent
   `FORTH_CONSOLE_SELFTEST_EXPORT` block from the screen.c override.
3. In screen.c, replace every `FORTH_CONSOLE_SELFTEST_EXPORT` with
   `FORTH_SELFTEST_EXPORT`. Verify completeness:
   `grep -rn FORTH_CONSOLE_SELFTEST_EXPORT packages/ src/` → 0 hits.
   Both overrides already include `forth_capture.h`.

---

## P6 — the closeAim teardown funnel (new override: bufferize.c)

The D7-1 shape applied to the leaked-FCAP_OPEN class: the guard moves
from 4 enumerated call sites into `closeAim()` itself, so every future
upstream caller in any file is correct by default. Override count goes
18 → 19 (budget 16) — record it in the commit as this packet's cost.

1. Materialize:
   `python3 tools/pkg_patch_refresh.py packages/forth-core --materialize bufferize.c`
2. In `packages/forth-core/bufferize.c`, `closeAim()` currently begins
   (src/c47/bufferize.c:2693):
   ```c
   void closeAim(void) {
     calcModeNormal();
     popSoftmenu();
   ```
   Insert as the FIRST statements of the body, before `calcModeNormal()`:
   ```c
     /* forth-core L1-1 (CONSOLIDATE P6): nothing upstream closes an
      * INTERACTIVE capture, and a leaked FCAP_OPEN is not inert (the next
      * PEM ALPHA press toggles keys mode instead of opening literal
      * input).  This guard lived at each closeAim() call site in
      * keyboard.c (the L1-1 disposition table); it now runs in the one
      * teardown itself, so a future upstream closeAim() caller in any
      * file is correct by default — the D7-1 argument, applied to this
      * class.  The interactive EXIT ladder never calls closeAim (its
      * teardown is calcModeNormal+popSoftmenu minus the string commit),
      * so this cannot fire there. */
     if(forthCapIsInteractive()) {
       forthCapClose();
     }
   ```
   Add `#include "forth_capture.h"` after `#include "c47.h"`. Everything
   else in the file stays byte-identical.
3. In the keyboard.c override, delete the `_forthCapCloseIfInteractive`
   definition AND its L1-1/L1-2 banner (the surviving rationale now lives
   at the funnel), and its 4 call sites — each is the line
   `_forthCapCloseIfInteractive();   /* L1-1 */` plus its site-local
   "L1-2 (C2) disposition: KEEP..." comment block, at: executeFunction's
   ITM_INTEGRAL `case CM_AIM:` arm; the longpress BST/SST arm; fnKeyUp's
   CM_AIM arm; fnKeyDown's CM_AIM arm. The `closeAim();` lines themselves
   are untouched. Expect keyboard.c's patch to shrink by ~40 lines.
   Verify: `grep -c _forthCapCloseIfInteractive packages/forth-core/keyboard.c` → 0.
4. **Mutation** (run, report, revert): comment out the funnel's
   `forthCapClose()` — `test_capture_interactive_close` (or the battery
   the fnKeyUp comment names) must redden. If nothing reddens, STOP and
   report before committing: the class test for this class is then a gap
   that must be filled in the same commit (a subcase driving
   open-console → fnKeyUp → assert `!forthCapIsInteractive()`).
5. Records, same commit: DESIGN-HISTORY entry (this spec, P6, the
   D7-1-shape argument, override 19); new row in
   `.claude/skills/upstream-diff-review/references/deliberate-exceptions.md`
   (shape: "closeAim funnel — guard inside upstream teardown instead of
   call-site enumeration"; citation: this spec + the owner's 2026-08-09
   approval).

---

## P7 — extract the console view from screen.c

New source `packages/forth-core/forth_console_view.c` (working-area
root, no upstream counterpart → auto-classified to `files/`).

**Moves** (byte-identical, keep every name and every comment):
`FORTH_CONSOLE_ROW_PITCH`, `FORTH_CONSOLE_ED_YINCR`,
`FORTH_CONSOLE_ED_CLEAR`, both `_Static_assert`s, `forthConsoleViewRows`,
`_forthConsoleMaxView`, `_forthConsoleClampView`, `_forthConsoleViewBase`,
`forthConsoleRollView`, `_forthConsoleActive`, `_forthConsoleRender`, and
the `/* ==== N1-2 — the console view ... */` banner. File starts with the
SPDX pair + `#include "c47.h"` + `#include "forth_capture.h"` +
`#include "forth_console.h"`.

**Stays in the screen.c override**: `_forthConsoleEditorTop()` with its
C14 derivation comment — it reads `checkHPoffset`, a screen.c-local
macro (src/c47/screen.c:385), which is the one coupling that cannot move.
Change its linkage from `static` to non-static (keep the name), and
declare in `forth_console.h`:
```c
/* Defined in the screen.c override — reads screen.c's checkHPoffset
 * macro, the one coupling the P7 extraction could not move. */
uint16_t _forthConsoleEditorTop(void);
```

**Linkage changes in the moved file**: `_forthConsoleActive` and
`_forthConsoleRender` are called from screen.c's refresh arm — change
them from `FORTH_SELFTEST_EXPORT` to plain non-static (they are now
cross-file API; keep names) and declare both in `forth_console.h`. The
pure helpers (`_forthConsoleMaxView`, `_forthConsoleClampView`) stay
`static`; `_forthConsoleViewBase` keeps `FORTH_SELFTEST_EXPORT` (tests
reference it). Verify test linkage after refresh:
`grep -rn "_forthConsoleViewBase\|_forthConsoleActive\|_forthConsoleRender" packages/forth-core/test_console.part.h`
— every referenced symbol must still resolve in the selftest build (the
gate proves it).

**Reconfigure note**: first build after creating the file needs
`CUSTOM_PKG_RECONFIGURE=1` (new `files/` entry). The gate handles the sim
build; run the dmcp5r47 measure per ground rule 3.

Expected: screen.c patch drops from ~286 to ~60 added lines; flash delta
≈ 0 (report actual).

---

## P8 — extract the fold/history/capture subsystem from manage.c

New source `packages/forth-core/programming/forth_fold.c` (mirrors the
`files/programming/` precedent set by param_core.c). This moves the
1,281-line hunk. Every public symbol it defines is ALREADY declared in
`forth_capture.h` (verified at spec time) — the API surface does not
change; definitions relocate.

**Seam wrappers** — the block calls exactly two manage.c statics
(`_insertInProgram`, manage.c:698; `_closeAlphaMenus`, manage.c:758).
Add to the manage.c override, directly below `_closeAlphaMenus`'s
definition (the `paramCorePutLiteral` precedent, lblGtoXeq.c):
```c
/* CONSOLIDATE P8: seams for programming/forth_fold.c — the extracted
 * fold/history subsystem needs exactly these two manage.c statics. */
void forthPkgInsertInProgram(const uint8_t *dat, uint16_t size) {
  _insertInProgram(dat, size);
}
void forthPkgCloseAlphaMenus(void) {
  _closeAlphaMenus();
}
```
Declare both in `forth_capture.h` (a short "manage.c seams" section). In
the moved code, rewrite the 5 `_insertInProgram(...)` calls and 1
`_closeAlphaMenus()` call to the wrapper names. (`_getProgramSize` is
public via manage.h — call it directly, no wrapper.)

**Move list** — every definition in the hunk between
`pemCloseAlphaInput`'s closing `}` and the `pemAlphaEdit` hunk, i.e.:
`_forthFoldKeptSteps` (static data), `forthCapRecommitStep` (static; both
callers move with it), `forthCaptureSuspend`, `_forthFoldFindCaptureStep`,
`forthCaptureResume`, `forthInteractiveEnter`, `_forthHistSaveCursor`,
`_forthHistRestoreCursor`, `forthHistoryProgram`,
`_forthHistPositionAtEnd`, `_forthHistFirstLineStep`,
`_forthHistLastLineStep`, `_forthHistLineCount`, `_forthHistLineAt`,
`_forthHistProgramBytes`, `forthHistoryEnsure`,
`forthHistoryGotoLastStep`, `forthHistoryEvict`, `forthHistoryPush`,
`forthHistoryRecall`, `_forthFoldAdmits`, `forthFoldEnter`,
`forthFoldUnwindIfDone`, `_forthFoldResolveCaptureStep`,
`_forthFoldNoteProgramDeleted`, `forthFoldLeave`, `forthFoldArmed`,
`forthFoldPending`, `forthFoldRederiveAdmission`. Comments move with
their functions, byte-identical.

**Also moves**: `_forthCapBuildStep` — needed by both halves. It becomes
public `forthCapBuildStep(char *dst, const char *text)` in forth_fold.c
(keep body and §8.1 comment verbatim), declared in `forth_capture.h`.
Update its call sites: the two inside moved code, and the three that stay
in manage.c (`pemAlpha`'s two `_insertInProgram(... _forthCapBuildStep ...)`
arms and `insertStepInProgram`'s arm) to the new name.

**Stays in manage.c** (do NOT move): `forthCaptureSanitizeRestoredUi`
(sits above `pemAlpha`, calls `_closeAlphaMenus` directly — leave it),
the catalog helpers `_forthCatalogBuriedOnStack` / `_forthCatalogMenuOnTop`
and their public wrappers (staying code in `insertStepInProgram`'s hunk
uses them; moved code switches its one use at the
`forthCaptureResume` guard to the PUBLIC wrappers
`forthCatalogMenuOnTop()` / `forthCatalogBuriedOnStack()`, already
declared in forth_capture.h:193-194), all `pemAlpha`/`pemCloseAlphaInput`
/`fnPem`/`insertStepInProgram` hook arms, and the two P3/P2 helpers.

**Cleanups the move enables**: delete the FIX-9 forward declarations of
the catalog helpers (patch's `pemCloseAlphaInput`-adjacent lines) — the
moved code now uses the public wrappers; delete the
`_forthFoldNoteProgramDeleted` forward declaration above `_clearProgram`
and declare it in `forth_capture.h` instead (manage.c already includes
forth_capture.h; keep the R8-1 DELETER comment at the `_clearProgram`
call site).

**Order within the wave**: P8 runs AFTER P1-P3 (their manage.c edits
touch lines this packet relocates or sits beside — rebasing them later
would redo work).

**Verify**: gate + upstream testSuite green; the group I pins in
`design-docs/forth-core/design-audit.sh` all pass — pins that grep
manage.c for fold symbols may need their FILE target updated to
forth_fold.c: update the grep target ONLY, never a count, and list every
pin touched in the commit message. `CUSTOM_PKG_RECONFIGURE=1` on first
build (new files/ entry). Expected: manage.c patch drops from ~1,651 to
~350 added lines. Report flash delta.

---

## P9 — extract the interactive EXIT ladder (F13/U5, owner-deferred until now)

Move fnKeyExit's interactive `CM_AIM` block — everything from the first
`if(forthCapIsInteractive() && lastErrorCode != 0) {` through the close
rung's `break;` — into a new public function in
`packages/forth-core/forth_console.c`:

```c
/* F13/U5 (CONSOLIDATE P9): the interactive EXIT ladder, extracted from
 * upstream's fnKeyExit.  Returns true when it handled the press (the
 * caller breaks); false only when no interactive capture is engaged.
 * Bodies and rung comments are the fnKeyExit block VERBATIM — including
 * the R12 error-dismiss pre-rung and the F8 suspended-residue recovery
 * pre-rung, which run before the three rungs proper. */
bool_t forthConsoleExitLadder(void);
```

Body = the three consecutive `if(forthCapIsInteractive()...)` blocks,
verbatim, with each terminal `break;` becoming `return true;`, and a
final `return false;`. Every comment moves along. The fnKeyExit site
becomes:

```c
      case CM_AIM: {
        /* forth-core F13: the interactive EXIT ladder lives in
         * forth_console.c; true means the press was consumed. */
        if(forthConsoleExitLadder()) {
          break;
        }
```
with the native arm following unchanged. Declaration goes in
`forth_console.h`.

**Decision, stated**: the CM_PEM arm's keys-mode rung (K2/E12.4, 5 lines)
does NOT unify into this function — it belongs to the PEM ladder's
ordering and unifying the two would couple two gesture systems for a
5-line win. Leave it.

**Mutation** (run, report, revert): in the extracted close rung, remove
the `if(popHome)` guard — the M1-1 battery's [8] row (the owned-frame
pop test) must redden, proving the moved code is the live code.

Expected: keyboard.c patch drops by ~155 added lines. This packet is
rebase-adjacent by the round-6 ruling; it is LAST of the code packets so
a mid-wave upstream rebase never has to replay it.

---

## P10 — comment relocation sweep (after P6-P9, re-measured)

P6-P9 already relocate the largest comment masses into package files
where they cost nothing. This packet handles what remains in `patches/`,
under the owner-approved rule:

- **Constraint comments stay inline** — anything a reader needs to not
  break the line below (the "LIVE, not origin" one-liners, load-bearing
  conjunct notes, refusal rationales). When in doubt, it stays.
- **Narrative moves** — audit history (which round found it, what was
  here before, how the refutation went, cross-references to superseded
  revisions). Destination: the finding's existing DESIGN-HISTORY entry
  (append, do not rewrite history); inline remains a one-line pointer:
  `/* F7: the console owns its row — DESIGN-HISTORY 2026-08-08 (F7). */`

Procedure: after P9 lands, list every comment block ≥ 6 lines remaining
in `patches/*.patch` (`awk` over `^\+\s*(/\*|\*|//)` runs), classify each
against the rule, move the narrative class, and put the classification
table (block anchor → stay/move + one-word reason) in the commit message.
The keyboardTweak.c banner (33 lines) is pre-classified: the census table
and disposition list MOVE (narrative), the two-sentence "why forth-core
overrides this file" head STAYS. The manage.c R8-1 DELETER comment at
`_clearProgram` STAYS (constraint). Do not touch comments inside
`files/` sources — they are free.

Verify: scanner still 0; gate green; report the added-line delta of the
four big patches against P9's numbers.

---

## Wave close-out

After P10: run the full review once more per the `upstream-diff-review`
skill (report `REVIEW_upstream-minimality_<date>.md`, counts against the
2026-08-09 baseline: 51 churn findings, 3,066 added lines), update the
skill's `deliberate-exceptions.md` if any packet added a shape, and
record in DESIGN-HISTORY: one entry for the wave with per-packet flash
deltas and the final override count (19). The audit exit criterion is
unaffected — these are refactors, but round 9 should still read this
wave's commits (the regression record does not exempt behavior-neutral
intent).
