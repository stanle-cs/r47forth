# Fix wave for the G3/G4 round-1 findings: the plan before the edits

Date: 2026-09-05. Tree: `program-graphics/stage-g0` at `840fe1c92`, plus the
uncommitted work of the afternoon (the restored sentinel row, the comment
pass). Inputs: the audit report
`audit/AUDIT_G3-G4_round-1_2026-09-05.md` (18 findings), the design review
`audit/FIX_DESIGN_REVIEW_G3-G4_2026-09-05.md` (ChatGPT, from the seven
cached designer outputs), and Stan's four rulings of 2026-09-05.

Status: executed on 2026-09-05. The red-first table, the sizes and the deviations are in DESIGN-HISTORY.md, entry "G3 and G4 round-1 fix wave". This document is the plan as it stood before the edits. Section 4 gives
each fix as pseudocode with its pins. Section 5 is the protocol that every
fix must pass before the commit.

## 1. What could go wrong, and the guard for each risk

1. **A fix relocates state.** Relocating state is the most dangerous fix
   shape (audit-fix-regression rate: most findings of a round come from
   the previous wave). Guard: no fix in this wave moves a field. The
   ownership fixes add a test on `calcMode`; they do not move `canvas` or
   `pg3d`.
2. **A new hunk lands on a sibling's hunk.** The new override of
   `saveRestoreBackup.c` shares the file with forth-core (hunks at lines
   8, 528, 926, 949, 1465) and undo-history (1478, 1502). Guard: the new
   hunk sits at line 268, inside `saveCalc`, far from every sibling hunk.
   The combined gate proves the composition.
3. **A fix changes a behaviour that an existing pin depends on.** The
   go-home reset runs inside `PVIEW` and `ERASE`. Guard: every G4 pin
   starts from `pgTestUnitCubeView`, which opens the view, so every pin
   starts at home already. P19 and S3 are read again after the change.
4. **The pool layout changes and `integrate_cov` fails again.** The leak
   fix returns 512 blocks per driver run. The upstream over-read in
   `parseEquation` depends on the bytes next to the formula block. Guard:
   the suite order stays (`program_graphics` after `integrate_cov`). If an
   equation test fails, the pool tiling checker of the audit runs before
   anyone names the cause. The upstream report is separate work.
5. **The gate is blind to the simulator and to the backup.** The headless
   suite never runs the item-table sentinel check or a backup round trip.
   Guard: the protocol launches the GTK simulator once and does one
   round trip through the backup file.
6. **A pin passes for the wrong reason.** Every new pin has a named
   mutation and the mutation runs. A repair pin must be red before the
   repair. A coverage pin must be green before and red under the mutation.
7. **A check tool fails silently.** My comment-strip check on 2026-09-05
   compared two empty files and reported "identical". Guard: every
   comparison in the protocol prints the sizes of what it compares.
8. **Speed.** Guard: no fix adds work to a 2D per-pixel path. The window
   guard runs once per 3D command, not per point. The baseline driver
   prints the NOP, PIXEL and LINE times before and after.
9. **Flash and RAM.** Guard: the `make dmcp5r47` delta is measured and
   written into the commit. The test-only switch is under
   `TESTSUITE_BUILD`, so the device gains no byte of RAM for it.

## 2. Rulings

| Finding | Ruling (Stan, 2026-09-05) |
|---|---|
| G34R1-4 | Go home. `ERASE`, `PVIEW` and EXIT set the three angles to 0 and the zoom to 0, as they already drop the frozen eye. |
| G34R1-5 | Test-only switch. A `TESTSUITE_BUILD` flag makes the next undo save fail. No pool-filling fixture for this pin. |
| G34R1-8 | Refuse. A 3D command whose window does not convert to usable floats shows `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and keeps the picture. The keys refuse before they clear the canvas. A mirrored window stays legal. |
| G34R1-9 | Do nothing. A rotation key with nothing retained leaves the canvas as it is. P6 and the design text are corrected to say so. |
| Test names | My call: the pins keep the P, D, W, K, V, S letters. The code's P27 and P28 become P31 and P32. The table's P27 is written. |
| Item rows | My call: the nine G4 rows stay where they are. Relocation is a compatibility decision outside this wave. |
| Backup | My call: the fix is on the save side. A backup written by the G4 version with mode 21 is not handled. The package is unreleased and Stan's own backup holds mode 0. |
| Comments | Stan: comments carry no contracts when the work is done. The comment pass stands. New code gets one-sentence what-it-does comments only. |

## 3. Order

1. Test-contract repairs. They change the pool layout that every later
   measurement runs in: G34R1-10, -14, -17, -18, -16, and the message of
   the sentinel pin (G34R1-1).
2. Ownership: G34R1-3 and G34R1-2. Both answer "who owns the block".
3. Engine: G34R1-5 and G34R1-7.
4. Arithmetic: G34R1-6 and G34R1-8.
5. Rulings on behaviour: G34R1-4 and G34R1-9.
6. Coverage: G34R1-11, -12, -13.
7. Documents: G34R1-15, the §9.8 table, TESTING.md §4, DESIGN-HISTORY.
8. The protocol of section 5, then one commit.

Each step ends with a green gate before the next starts.

## 4. The fixes one by one

Line numbers are for the working tree after the comment pass. Names are
the anchors; the numbers help the reader find them.

### 4.1 G34R1-1, the sentinel row (done, keep it)

The row at `items.c:4809` is upstream's row again, byte for byte. The
smoke driver tests `indexOfItems[LAST_ITEM]` for the name `"Last item"`
and the function `itemToBeCoded`. Change: the pin must call
`pgTestFail("S0 the last item row is not upstream's sentinel")` instead of
a silent `pgTestFailures++`. Mutation: put the WIREFRAME row back at the
last index. Red-first: the pin was red at `840fe1c92` (the simulator
message proves the state).

### 4.2 G34R1-10, the test drivers leak the block

Cause: `pgTestUnitCubeView` (2185) and P27 (2351) call `pgReset()` with a
live block. `pgReset()` forgets the block without a free, by design.

Change in `pgTestUnitCubeView`:

    if(calcMode == CM_GRAPHICS_CANVAS) pgCloseView();   // frees the block
    pg3dFreeBlock();                                     // NULL-safe
    calcMode = CM_NORMAL; lastErrorCode = ERROR_NONE;
    pgReset();
    fnPview(6); ...

Change in P27 (new P31) and P28 (new P32): `pg3dFreeBlock(); pgReset();`
at the start. The `pgReset()` at their end stays (the block is NULL there).
P18 keeps its own free, and the pin now asserts that `held` is not NULL
before the reset (today `held` can be NULL and the pin proves nothing).

New pin L1, pool balance, in `pgTestDraw3D`:

    // at the start, after the three programs are loaded
    X, Y, Z, T = long integer 0; saveForUndo();
    before = c47MemInBlocks;
    ... the pins ...
    // at the end, after pgCloseView()
    X, Y, Z, T = long integer 0; saveForUndo();
    if(c47MemInBlocks != before) fail("L1 the driver leaks N blocks")

The two `saveForUndo()` calls make the undo image the same size at both
ends. Expected: red at the tip (512 blocks), green after the fixture fix.
Mutation: remove the `pg3dFreeBlock()` before P31's `pgReset()`: red.

### 4.3 G34R1-14, W3 reads a pixel that W2 lit

Change at W3 (2070): before the equal-ends `XRNG`, `fnErase(NOPARAM)`, then
assert `!pgTestLit(200, 120)`. The rest of W3 stays. Coverage pin: green
before and after. Mutation: in `pgRange`, store the range before the
equal-ends test: red.

### 4.4 G34R1-17, the compile-time size assert

After the `pg3dHeader_t` typedef (790):

    _Static_assert(sizeof(pg3dHeader_t) == PG3D_HEADER_BYTES, "pg3dHeader_t must be PG3D_HEADER_BYTES");

Evidence: change `reserved[12]` to `reserved[13]`, the build fails. Then
change `PG3D_HEADER_BYTES` to 68, the build fails. Both restored.

### 4.5 G34R1-18, the S3 message prints 0

Change at 2432: one constant `PG_S3_COUNT 8656u` used in the test and in
the message. No pin; the message is the fix. S3h (section 4.16) adds the
second recorded count the same way.

### 4.6 G34R1-16, the D17b mutation row

TESTING.md row 70 (D17b, first form) says the mutation is "None". Row 83
names "Remove the lone-lead-byte stop of the walk", which no longer
exists. Change: row 70 is retired (struck from the table, kept in
DESIGN-HISTORY); row 83 names the real mutation: "Restore the old cap
guard in place of `pgGlyphBoundary`: the walk reads past the NUL and the
canary changes." The mutation runs once in the protocol.

### 4.7 G34R1-3, the block can be taken outside the view

Cause: `pg3dEnsure` (848) tests `canvas.region == 0`. A plot step leaves
`calcMode` and keeps `canvas.region` at 2 or 6, so a later `EYEPT` takes
the block with no view to free it. `fnGclip` reads the same stale region
for its bottom row.

New static helper, used by 4.7 and 4.8:

    static void pgReleaseAbandoned(void) {
      if(calcMode != CM_GRAPHICS_CANVAS && canvas.region != 0) {
        canvas.region = 0;
        pg3dFreeBlock();
      }
    }

Change in `pg3dEnsure`:

    pgReleaseAbandoned();
    if(calcMode != CM_GRAPHICS_CANVAS) return true;
    if(pg3d.block != NULL) return true;
    ... as today

Change in `fnPview`, first line: `pgReleaseAbandoned();` so a new view
after an abandoned one starts from a released block.

Pin O1: open `PVIEW 6`, `EYEPT` (block taken), then leave the view the way
a plot does. The real gesture is `fnPlotStat(PLOT_START)` with two
statistics points entered through `fnSigma`. If that gesture cannot be
driven from the suite in one hour of work, the pin sets
`calcMode = CM_PLOT_STAT` and records that shortcut in TESTING.md as a pin
limit. Then `EYEPT` outside the view: `c47MemInBlocks` returns to its
value before the first `EYEPT`, `pg3d.block` is NULL, `canvas.region` is
0. Then `PVIEW 6` again: the block count rises by 512 at the next 3D
command, not before. Mutation: restore the `canvas.region == 0` test: red.

### 4.8 G34R1-2, a backup saved in the view restores into a mode nobody handles

Cause: `saveCalc` writes `calcMode` 21; `restoreCalc` has no arm for 21
and shows the bug screen (`saveRestoreBackup.c:1531`). Upstream already
normalises one mode at save time: `CM_CONFIRMATION` (268-271). The fix
copies that convention.

New public function in `pgmGraphics.c`, declared in `screen.h` next to
`pgCloseView`:

    void pgBeforeSave(void) {
      if(calcMode == CM_GRAPHICS_CANVAS) pgCloseView();   // restores the mode, frees the block, repaints
      pgReleaseAbandoned();
    }

New override `saveRestoreBackup.c`, made with
`python3 tools/pkg_patch_refresh.py packages/program-graphics --materialize saveRestoreBackup.c`,
one hunk after the `CM_CONFIRMATION` block at 268:

    if(calcMode == CM_CONFIRMATION) { ... }        // upstream, unchanged
    pgBeforeSave();                                  // program-graphics: the canvas view does not survive a save.

Pins B1 and B2 drive `pgBeforeSave()` from the suite (`saveCalc` writes a
file and is not driven from the suite):
B1: in `PVIEW 6` with a block, `pgBeforeSave()` gives `calcMode` equal to
the mode before `PVIEW`, `pg3d.block` NULL, `canvas.region` 0.
B2: the abandoned state of O1, then `pgBeforeSave()`: the same three
results. Mutation: empty `pgBeforeSave`: red.
The call site in `saveCalc` is proven by the round trip of section 5,
step 4, not by a pin.

DESIGN.md §3 gets one sentence: the view closes when the state is saved,
so a backup never holds mode 21. README's limit "The canvas does not
survive sleep or power off" is now enforced, and stays.

### 4.9 G34R1-5, the engine ignores a failed undo save

Cause: `pg3dEngineEnter` (1077) calls `saveForUndo()` and goes on. With
`ERROR_RAM_FULL` there is no image, `fnUndo(0)` at leave restores
nothing, and X, Y, Z, T hold the last sample. Upstream's convention is
`reallyRunFunction` (`items.c:300-310`): test `lastErrorCode` for
`ERROR_RAM_FULL` right after the save, show the error, return.

Change, entry returns success:

    #if defined(TESTSUITE_BUILD)
      static bool_t pgTestFailNextUndoSave;
    #endif

    static bool_t pg3dEngineEnter(pg3dEngineSave_t *sv) {
      currentKeyCode = 255;
      #if defined(TESTSUITE_BUILD)
        if(pgTestFailNextUndoSave) { pgTestFailNextUndoSave = false; lastErrorCode = ERROR_RAM_FULL; }
        else
      #endif
      saveForUndo();
      if(lastErrorCode == ERROR_RAM_FULL) {
        displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        return false;
      }
      ++engineNestingDepth; ... as today
      return true;
    }

Change in `fnWireframe`: the header is written after a successful entry,
so a refused run keeps the old grid.

    retain = (h != NULL) && (numX * numY <= pg3dFreeBytes(h) + (uint32_t)h->numX * h->numY);
    rows = allocC47Blocks(...); if(rows == NULL) { error; return; }
    if(!pg3dEngineEnter(&sv)) { freeC47Blocks(rows, ...); return; }
    if(h != NULL) { h->gridValid = 0; h->numX = 0; h->numY = 0; if(retain) { h->numX = numX; h->numY = numY; } }
    ... as today

The cap formula is the one DESIGN.md §9.4 gives: the old grid does not
count against the new one. The result is the same as today's two-step
write.

Change in `pg3dRerun`: `if(!pg3dEngineEnter(&sv)) return;` before any
write to `view` or `h`. The zoom step itself stays applied; the redraw
uses the old bytes at the new zoom, and the error shows.

Pin E1: X, Y, Z, T = 1, 2, 3, 4; a valid grid from `PLNE` is retained;
`pgTestFailNextUndoSave = true`; `fnWireframe(SADL)`. Expect:
`lastErrorCode == ERROR_RAM_FULL`; `pg3dRunCount` unchanged; X, Y, Z, T
unchanged; `engineNestingDepth`, `plotEngineActive`,
`currentSolverNestingDepth` unchanged; `FLAG_SOLVING` clear;
`c47MemInBlocks` unchanged (the rows were freed); the header still says
`gridValid` 1 with the `PLNE` label and its counts. Red-first: red at the
tip (the count rises, X changes). Mutation: remove the `ERROR_RAM_FULL`
test: red.
Pin E2: from the same grid, press `ITM_ADD` until `pg3dRunCount` rises
(the control), then set the flag and press again: the count does not
rise, the error shows, `gridValid` stays 1, `zRecLo`/`zRecHi` unchanged.
Mutation: remove the entry test in `pg3dRerun`: red.

### 4.10 G34R1-7, grid bytes are written after the body changed the header

Cause: `pg3dRunGrid` (1057) drops retention only when the block is gone
or the header is not frozen. A body that runs `ERASE` at one sample and
`LINE3D` at every sample freezes the header again with `numX` 0, so the
grid bytes go on landing in the payload, where the line records of the
body accumulate from the top. `pg3dRerun` (1305) writes the new z range
on `PG3D_RUN_OK` alone.

New static predicate, used in three places:

    static bool_t pg3dGridIntact(const pg3dHeader_t *h, uint32_t numX, uint32_t numY) {
      return pg3d.block != NULL && h == PG3D_HDR() && h->frozen && h->numX == numX && h->numY == numY;
    }

In `pg3dRunGrid`, after the sample: `if(retain && !pg3dGridIntact(h, numX, numY)) retain = false;`
(`retain` never returns to true). In `fnWireframe`, the final test becomes
`result == PG3D_RUN_OK && retain && pg3dGridIntact(h, numX, numY)`. In
`pg3dRerun`: `if(result == PG3D_RUN_OK && pg3dGridIntact(h, numX, numY)) { zRec update } else if(h != NULL) h->gridValid = 0;`.

Pin H1, the collision case. Program `CND`: `LBL "CND", FS?C 00, XEQ "ERS",
PT3D, LINE3D, CLX, END` and `LBL "ERS", ERASE, RTN`. The pin sets flag 00,
`NUMX 17`, `NUMY 17`, unit-cube view, `fnWireframe(CND)`. The first sample
erases and freezes the header again through `LINE3D`; every sample
records one zero-length line, 289 records in all, filling the payload
from the top down to offset 314. The grid bytes of samples 286 to 288
land at offsets 350 to 352, inside record 282. Expect after the fix:
`lineCount` 289, `gridValid` 0, and every record k equals its six expected
bytes (the encoded sample point, twice). Red-first: red at the tip
(record 282 differs). Mutation: restore the frozen-only test: red.
Pin H2: `pg3dRerun` after a body that erased: the `ERAS` program of P29
under a zoom press with `gridValid` 1 set by hand is not a real gesture,
so H2 is: valid `PLNE` grid, then `fnWireframe(ERAS)` (P29), then press
`ITM_ADD` three times: `gridValid` stays 0 and `pg3dRunCount` does not
rise (no re-run without a valid grid). Coverage pin. Mutation: drop the
`gridValid == 0` return in `pg3dZoomRerun`: red.

### 4.11 G34R1-6, a narrow volume span overflows the byte scale

Cause: `pg3dRange` (1192) tests the span for finite and positive, and
`pg3dEncode` divides 254 by it. A span under about 7.5e-37 gives an
infinite scale. The zoom re-run derives a second span in
`pg3dZoomRerun` (1330) with the same hole.

New static predicate:

    static bool_t pg3dSpanUsable(float lo, float hi) {
      float d = hi - lo, sc;
      if(!(lo < hi) || !(d - d == 0.0f)) return false;
      sc = 254.0f / d;
      return sc - sc == 0.0f;
    }

`pg3dRange` uses it in place of its two tests. `pg3dZoomRerun` tests
`pg3dSpanUsable(zNewLo, zNewHi)` where it tests `zNewLo < zNewHi` today,
and returns without a re-run when it fails.

Pin V1: `XVOL 0 7.4e-37` raises `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and
leaves `xlo`, `xhi`; `XVOL 0 7.6e-37` is accepted; the same for `YVOL`
and `ZVOL`. The implementer confirms the two boundary values with the
float arithmetic before pinning them. Red-first: red at the tip (7.4e-37
accepted). Mutation: drop the scale test: red.
Pin Z1: `YRNG 0 8e-37` (usable as a window: 239 / 8e-37 is finite),
`YVOL -1 1`, eye y -3, a valid `PLNE` grid. Press `ITM_ADD` repeatedly.
The visible z slice shrinks with the zoom, so the re-run runs at the first
presses and is refused from the press where 254 / slice overflows. The
implementer computes that press number, then pins: the count rises at the
press before it (control), and does not rise at it (guard). Mutation:
replace `pg3dSpanUsable` with `zNewLo < zNewHi` in `pg3dZoomRerun`: red.

### 4.12 G34R1-8, a window the 3D code cannot use (ruling: refuse)

Cause: `pg3dSetup` (944) converts the 34-digit window to floats and
divides 399 and 239 by the span. `XRNG 0 1e39` gives an infinite end and
a zero scale; every point lands on column 0 or off screen with no error.

New static predicate:

    static bool_t pg3dWindowUsable(void) {
      if(pgWindow.set & 1) {
        float mn = toFloat(xmin), mx = toFloat(xmax), d = mx - mn, sc;
        if(!(mn - mn == 0.0f) || !(mx - mx == 0.0f) || !(d - d == 0.0f) || d == 0.0f) return false;
        sc = 399.0f / d; if(!(sc - sc == 0.0f)) return false;
      }
      the same for axis 2 with 239.0f
      return true;
    }

There is no `mn < mx` test: a mirrored window is legal. Call sites, each
before any state changes: `fnWireframe` before `pg3dEnsure`; `fnLine3d`
after the no-current-point branch and before `pg3dRecordView`; `pg3dKey`
after the nothing-retained return and before the angle change. `fnPt3d`
has no projection and no call. On failure: `pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN)`
and return. `pg3dRedraw` and `pg3dZoomRerun` are reached only through
`pg3dKey`, so they are covered.

Pin W7: `XRNG 0 1e39` (reals), then `fnWireframe(PLNE)`: error 1, no lit
pixel, `pg3dRunCount` unchanged, header unchanged. Red-first: red at the
tip (a vertical line is drawn). Mutation: remove the call in
`fnWireframe`: red.
Pin W8: `XRNG 1 0` (mirrored) with the plane of P2: no error, 798 lit
pixels (the plane is symmetric). Coverage pin. Mutation: add `mn < mx` to
the predicate: red.
Pin W9: a valid `PLNE` grid, then `XRNG 0 1e39`, snapshot the canvas,
press `ITM_UP1`: error 1, canvas bytes identical, `angX` 0. Red-first: red
at the tip (the picture collapses). Mutation: remove the call in
`pg3dKey`: red.

### 4.13 G34R1-4, the turn and the zoom after a clear (ruling: go home)

Change in `pgSetRegion` (34), after `pg3dEmpty()`, and in `pgCloseView`
(104), after `pg3dFreeBlock()`:

    pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0;

Change in `pg3dEmpty`: `pg3d.haveCur = 0;` moves before the NULL test, so
a `PT3D` given outside the view does not survive into the first view.

Pin T1: valid `PLNE` grid, press `ITM_UP1` and `ITM_ADD` (`angX` 1,
`zoomStep` 1), `fnErase`: `angX` 0, `zoomStep` 0; `fnWireframe(PLNE)`
again gives the canvas of a fresh home drawing (snapshot compared). T2:
the same through `fnPview(6)`. T3: the same through `pgCloseView` then
`fnPview(6)`. T4: `PT3D` with the view closed, `PVIEW 6`, `LINE3D`: no
pixel lit and `haveCur` 1 (no phantom segment). Red-first: T1, T2, T3, T4
red at the tip. Mutations: skip the reset in `pgSetRegion` (T1, T2 red);
skip it in `pgCloseView` (T3 red); keep the NULL test first in
`pg3dEmpty` (T4 red).

DESIGN.md §9.2.5 and §9.6 get the sentence: `ERASE`, `PVIEW` and EXIT
return the angles and the zoom to the home view. README's key table gets
the same sentence under key 5.

### 4.14 G34R1-9, the keys after an over-cap mesh (ruling: do nothing)

No code change. Pin P6, written as the ruling says: `NUMX 45`, `NUMY 45`,
`fnWireframe(PLNE)` in the unit-cube view: lit above 0, `gridValid` 0,
`numX` 0; snapshot; `processKeyAction(ITM_UP1)`: canvas bytes identical,
`angX` 0. Coverage pin. Mutation: remove the nothing-retained return in
`pg3dKey`: red. DESIGN.md §9.8 row P6, §9.9/§10 item 13 and README's
limits list say: a grid above 44 by 44 draws once, does not turn, and a
key press leaves it as it is.

### 4.15 G34R1-11, the pins the design lists and the code lacks

Written in this wave, each as its §9.8 row specifies, with these
adjustments: P4 keeps the design's chunk fixture with a bound of 1024
chunks and a checked stop; P7 and P8 find the key index by a scan of the
active keyboard table for `ITM_UP1`, not the literal "22"; P11 asserts
`pg3dRunCount` with the design's numbers after the implementer has
confirmed them on the unchanged code; P13 asserts the recorded hole mask
and the one valid segment, not only "no error"; P21 reads the macros;
P22, P25 as written; P27 (GMODE 2 twice) as written; P28 is covered by
R1y and R1z of the showcase driver and the row says so; the code's P27
and P28 become P31 and P32 with rows added. P14, P15, P17 as written; P14
and P15 record that `exitKeyWaiting` is not reachable from the suite. The
four mutations the audit ran green against existing pins (§G34R1-11 of
the report) run again after the pins exist and must be red.

### 4.16 G34R1-12 and G34R1-13, the redraw has no content oracle

Change: one helper `static float pg3dGridCoord(float lo, float hi, uint32_t i, uint32_t n)`
returning `lo + (float)i * ((hi - lo) / (float)(n - 1))`, used by
`pg3dRunGrid` and `pg3dRedraw`. Pin R0: the showcase scene without the
caption (saddle plus cube edges), snapshot the still canvas, press
`ITM_UP1` then `ITM_DOWN1` (two redraws, the second at home): the canvas
equals the still snapshot. Coverage pin. Mutations: blank redraw (drop
both loops): red; change the divisor in the redraw copy only, before the
helper exists: red. S3h: the frame 001 count recorded as a constant and
asserted, as S3 is.

### 4.17 G34R1-15, the documents

README: line 224 becomes "f UP, f DOWN, g UP and g DOWN turn the drawing.
SNAP works. Every other shifted key does nothing in the view." DESIGN.md
§3.6 (166) gets the same rule. §9.9 becomes one line: "The limits are in
section 10." §10 stays canonical and gets the new items (window refusal,
go-home, the save-time close). The dead citation at 1273 ("section
9.10") points to §9.4. TESTING.md §3 names
`./packages/program-graphics/build-test.sh` as the gate. The README passes
`forum/aiaudit.py` and `forum/framescan.py` before the commit.

## 5. Verification protocol

1. **Red-first table.** For every pin of section 4: write the pin, build,
   record red or green; apply the fix, build, record; apply the named
   mutation, build, record red; revert the mutation. The table goes into
   DESIGN-HISTORY with the wave entry.
2. **Gate.** `python3 tools/pkg_patch_refresh.py packages/program-graphics`,
   then `./packages/program-graphics/build-test.sh`. Both passes must print
   GREEN and the success banner. The final run happens on the final tree.
3. **Simulator start.** `./build.sim/src/c47-gtk/c47` opens a window. The
   process must not exit with the sentinel message.
4. **Backup round trip.** In the simulator: `PVIEW 6`, draw one `LINE`,
   close the window (the state is saved), start the simulator again. The
   calculator comes up in normal mode with no bug screen, and
   `backup.cfg` holds `calcMode:uint8:0`.
5. **Churn scan.** `patch_churn_scan.py` on `packages/program-graphics/patches/*.patch`
   reports zero violations.
6. **Diff read.** `git diff` of the package touches only the functions
   section 4 names. Every sizes-and-counts comparison prints what it
   compared.
7. **Pool.** The end-of-suite line "C47 owns N bytes" for the solo pass is
   recorded before and after; the difference is the leak returned.
8. **Speed.** The baseline driver's NOP, PIXEL and LINE times before and
   after, same build type. The 2D times must not move outside their noise.
9. **Flash and RAM.** `make dmcp5r47`: text plus data and the BSS delta,
   written into the commit message.
10. **Equation suite.** If `integrate_cov` or another equation file fails
    after the layout change, the pool tiling checker runs first. The
    cause is named from evidence, not from the history.
11. **Documents.** DESIGN.md §9.8 lists every pin the code has, and only
    those. TESTING.md §4 has one row per pin with its mutation. The
    DESIGN-HISTORY entry records the wave, the rulings, the red-first
    table, the sizes and the pool line.
12. **Commit.** One commit on `program-graphics/stage-g0`. Nothing is
    pushed.

## 6. After the wave

The one-round rule closes G3/G4 after this wave. The regression rate of
past waves says the next findings would come from these fixes. Stan's
call: no further reading, or one out-of-family packet on the wave's diff
alone (findings, not fixes).

## 7. Not in this wave

- Relocation of the nine G4 item rows.
- Restore-side handling of a backup that already holds mode 21.
- The upstream report on the `parseEquation` over-read (separate file).
- A visible message when a mesh is drawn above the cap.
- The DM42 key repeat.
