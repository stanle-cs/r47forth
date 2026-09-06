# program-graphics: TESTING.md

Status: version 0, stage G0, 2026-09-04.

## 1. Harness

The package registers test drivers in the test suite of the headless
simulator, as pretty-print does (`packages/pretty-print/testSuite/testSuite.c`).
A driver is a C function `pgTest<Name>(uint16_t)` that draws, reads pixels
back with `lcd_buffer_pixel_on`, and reports a failure with the suite's
failure path. The test file is `testSuite/tests/program_graphics.txt`. It
is listed in `testSuiteList.txt` under the touching-line rule.

## 2. Pin rules

1. A pin sets the state it asserts and restores it. The mode a gesture runs
   in is part of the pin.
2. A pin must be red under one named mutation. The mutation list is the
   table in section 4. A pin that cannot be made red by any mutation is
   not a pin.
3. A pin drives the real gesture: the item through `runFunction`, the key
   through both key paths.
4. No fixture that cannot reach its own state.

## 3. Gate

Solo and combined, in one run:

    ./packages/program-graphics/build-test.sh

The script refreshes the package first, then runs the solo pass and the
combined pass with forth-core, undo-history, pretty-print and
pretty-print-extra. `--solo` and `--combined` run one pass.

The refresh runs first, because `pkg_build` refreshes after its suite.

## 4. Pins per stage

| Stage | Pin | Mutation that must red it |
|---|---|---|
| G0 | The driver registers and runs. | Remove the registration row. |
| G1 | `PVIEW 2` sets calcMode 21, clears the register region, and clips at row 170. The softmenu painter repaints rows 171 to 239, so the clip row is the observable. | Clip at row 239 for region 2. |
| G1 | `PVIEW 6` clears rows 20 to 239 and clips at row 239. | Clip at row 170 for region 6. |
| G1 | A program draws with `PIXEL`, runs `PVIEW 2`, stops. The pixels are lit after the stop. | Route the mode 21 case to `_refreshNormalScreen`. Removing the case does not red this pin, because the default arm paints nothing. |
| G1 | The same repaint keeps the status bar live: the band is cleared, the repaint lights it again. | Remove the refreshScreen case for mode 21. |
| G1 | A program without `PVIEW` loses its drawing at the stop, as upstream. | None needed. This pin guards upstream behaviour; it is red when the case leaks into `CM_NORMAL`. |
| G1 | A softkey press in the view changes nothing. | Remove the range clause from one of the three softkey functions. |
| G1 | A direct key other than EXIT and R/S in the view changes nothing. The pin presses each key and releases it through its item function. | Remove the no-op case of `fnKeyEnter`, so the release shows a bug screen. |
| G1 | EXIT restores the previous mode and repaints. The pin drives the press, then the release through `runFunction(ITM_EXIT1)`, as the keyboard does. | Skip the calcMode restore. |
| G1 | `VIEW` inside the view paints nothing. | Let the mode 21 case call `_refreshNormalScreen`. |
| G1 fix wave | `CLSTK` inside the view leaves it open and the drawing intact at the next repaint. | Remove the guard in `calcModeNormal`. |
| G1 fix wave | `ENTER` as a program step inside the view lifts the stack: Y equals X afterwards. | Switch `fnKeyEnter` on `calcMode` instead of `pgEffectiveCalcMode()`. |
| G1 fix wave | `CC` and `.ms` from the keyboard inside the view do nothing and show no bug screen. | Remove the no-op case in `fnKeyCC`. |
| G1 fix wave | An error inside the view paints its text on canvas line 1 at the next refresh, and the EXIT press that clears it leaves the Z line band untouched. | Remove the guard at the top of `refreshRegisterLine`. |
| G1 fix wave, limit | Pin K3 drives the EXIT release directly, so it cannot see a press that swallowed EXIT. `keyActionProcessed` is static in keyboard.c. Documented pin limit. | none |
| G2 | D1 to D6: `LINE`, `BOX`, `FBOX`, `CIRCLE`, `FCIRCL`, `ARC`: endpoint and edge pixels lit, one pixel outside clear, interior clear or filled. | Off-by-one in the endpoint of the line stepper; skip the last row of the fill. |
| G2 | D7: `GCLIP`: a line that crosses the clip edge stops at the edge. | Remove the clip test in `pgPixel`. |
| G2 | D8: a line from column 0 to 5000 draws the on-screen part with no error; a coordinate of 40000 raises `ERROR_OUT_OF_RANGE` and draws nothing. | Remove the clamp in `pgRun`; skip the limit check. |
| G2 | D8c: a line from column -20 is clamped at the left edge, and the row below keeps its bytes and its dirty flag. | Remove the left clamp in `pgRun`. |
| G2 | D9: `GMODE 2` twice restores the buffer, for a filled box. | Use set instead of invert in `pgRun`. |
| G2 | D10: for 1,000 pseudo-random pixels, the direct write and `bitblt24` leave the same bytes in every row, dirty flags included. | Change the bit order or skip the dirty flag. |
| G2 | D11: `DISP 2` lights pixels in rows 40 to 59 only; `TEXTOUT` at a point lights pixels in its cell. | Shift the line row. |
| G2 | D12: a string in X for `LINE` raises `ERROR_INVALID_DATA_TYPE_FOR_OP` and draws nothing. | Skip the type check. |
| G2 fix wave | D13: a clip rectangle wholly outside the region, on each of the four sides, gives an empty clip. A full-screen `FBOX` then draws nothing, and the stored clip stays inside the region. | Restore the one-sided clamps of the first G2 code. |
| G2 fix wave | D14: `FCIRCL` with radius 23170 far off the screen paints nothing on the screen. Radius 32767 around an on-screen center paints the four corners. | Compute the square root, or 4 r squared, in 32 bits. |
| G2 fix wave | D15: `DISP` inside a clip of columns 200 to 399 keeps a pixel at column 10, writes inside the clip, and writes nothing left of the clip. | Clear the full width and start the text at column 1. |
| G2 fix wave | D16: an arc from 0 to 0.05 degrees at radius 5000 lights the pixels at rows 130 and 133 of column 300, and none at rows 120 or 140. | Scale the arc vectors by 1024. |
| G2 fix wave | D17: a string longer than the scratch buffer, with a two-byte glyph at the cap, is cut before the glyph. | Cut at the cap without the glyph check. |
| G2 fix wave | D17b, first form, retired by the rewritten row below. | Retired. |
| G2 fix wave | D18: a NaN angle for `ARC` raises `ERROR_OUT_OF_RANGE` and draws nothing. | Accept NaN in the angle reader. |
| G2 | S1: the showcase screen has the recorded count of lit pixels. | Any change to a primitive. |
| G3 | W1: without a window, a real is a pixel rounded half away from zero (2.5 is 3, -0.5 is off the screen). | Round toward zero. |
| G3 | W2: `XRNG` 0 10 and `YRNG` 0 5 map (5, 2.5) to the pixel (200, 120), the corners to (0, 0) and (399, 239), and a long integer stays a pixel. | A scale of 398. |
| G3 | W3: equal ends raise `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and leave the window; a reversed range mirrors the axis. | Skip the equal-ends check. |
| G3 | W4: a real that maps beyond 32767 pixels raises `ERROR_OUT_OF_RANGE`; one that maps to 3990 is clipped without an error. | Clamp instead of the error. |
| G3 | W5: two complex points, the first in Y and the second in X, draw the line; a complex with a long integer is the type error. | Swap the real and imaginary parts. |
| G3 | W6: the window survives `ERASE`. | Clear the window in `pgSetRegion`. |
| G3 | S1: the showcase count includes the sine curve through the window. | Any change to a primitive. |
| G2 audit, in-family wave | D20: outside the view, `LINE`, `FBOX`, `FCIRCL`, and `TEXTOUT` set the PIXEL flags and their drawing survives a refresh. | Skip the flag writes. |
| G2 audit, in-family wave | D8, rewritten: the refused line targets a row nothing has lit, and that row stays clear. | Raise the error and still draw. |
| G2 audit, in-family wave | D15b: `DISP` clears its band inside the clip before it writes; the row above stays. | Skip the band fill. |
| G2 audit, in-family wave | D17b, rewritten: the canary bytes after the NUL survive the trim walk. | Restore the old cap guard in place of `pgGlyphBoundary`: the walk reads past the NUL and the canary changes. |
| G2 audit, in-family wave | D17c: the cap keeps a glyph whose second byte has bit 7 set. | Test bit 7 of the byte before the cap instead of walking. |
| G2 audit, in-family wave | D17d: a lone lead byte at the end is cut when the width fits, and the canary survives. | Cut only at the cap. |
| G2 audit, in-family wave | D19: an arc of 359.9995 degrees draws almost the full circle; an arc of 0.0002 degrees draws a few pixels. | Drop the 180-degree test of the same-direction arm. |
| G2 audit, in-family wave | K7, split: the band is clear, and separately the flag is reset. | Reset the flag without the clear. |
| G2 audit, in-family wave | K8: the view opened from alpha input mode takes the cursor and clears `FLAG_ALPHA`; EXIT gives them back. | Skip the prologue. |
| G2 audit, in-family wave | K9: the function name paint records the item and paints nothing in the view; the hide clears the item and repaints nothing. | Remove either arm in the screen.c mirror. |
| G2 audit, in-family wave | K10: a real softkey press with the `CANVAS` menu pushed changes nothing in the view. | Remove the range clause of `executeFunction`. |
| G2 audit, in-family wave | V9: in region 2 the softmenu band is cleared before the painter runs. | Skip the band clear. |
| G4 | P1: the eight corners of the unit cube project to the recorded pixels (DESIGN.md §9.3.6). | Any change in the projection. |
| G4 | P2: `WIREFRAME` of the plane z = 0 on a 2 by 2 grid lights 798 pixels. | Skip the row lines. |
| G4 | P3: the block is 512 pool blocks, taken by the first 3D command in the view, returned at EXIT, never taken outside the view. | Skip the free in `pgCloseView`. |
| G4 | P5: the byte encoding: NaN is 255, the range ends are 0 and 254, values outside clamp. | Return 255 for a clamped value. |
| G4 | P9, P26: each key changes its counter in both directions; 5 resets; 4 does nothing. | Drop a key from the guard arm. |
| G4 | P10: 36 UP presses return the canvas byte for byte. | Count the steps modulo 37. |
| G4 | P16: the stack survives `WIREFRAME`. | Skip `fnUndo(0)`. |
| G4 | P18: the reset hook forgets the block without a free and restores the HP defaults. | Free the block in `pgReset`. |
| G4 | P19: `ERASE` empties the retained content; a key press then draws nothing. | Skip `pg3dEmpty` in `pgSetRegion`. |
| G4 | P20: `NUMX` with 1, 101, 2.5 and a string, and `XVOL` with equal ends, are refused and change nothing. | Accept 1. |
| G4 | P23: `LINE3D` without a current point sets it and draws nothing. | Draw from (0, 0, 0). |
| G4 | P12: 330 lines fill the block. | Skip the free-bytes test. |
| G4 | S3, R1, R1y, R1z, R2: the showcase count, and the canvas returns after each full turn and after six zoom steps in and out. | Any change to a primitive or to the projection. |
| G4 fix wave | P31 (P27 until the G3/G4 wave): a point exactly one 1024th of the depth in front of the eye is drawable; nearer is not. | Make the eps test exclusive again. |
| G4 fix wave | P32 (P28 until the G3/G4 wave): the far corner of an extreme window projects to (32000, 32000): the clamp holds on the final row. | Drop the row clamp after the flip. |
| G4 fix wave | P20b: `XVOL -2e38 2e38`, a span that overflows float, is refused. | Drop the finite-span check. |
| G4 fix wave | P29: a body that calls `ERASE`, `PT3D`, and `LINE3D` under `WIREFRAME` leaves no valid grid. | Drop the counts check before `gridValid`. |
| G4 fix wave | P3, second mutation: the block is not freed inside `pg3dFreeBlock`. | The first form did not compile: it named the 3D state above its declaration. |
| G3/G4 audit wave | S0: the last item row is upstream's sentinel. | Put the WIREFRAME row back at the last index. |
| G3/G4 audit wave | L1: `pgTestDraw3D` returns every pool block it takes. | Remove the free before P31's `pgReset()`. |
| G3/G4 audit wave | W3, corrected: the probe pixel is clear before the refused `XRNG`. | Store the range before the equal-ends test. |
| G3/G4 audit wave | The header size is a compile-time assert. | `reserved[13]`, or `PG3D_HEADER_BYTES` 68: the build fails. |
| G3/G4 audit wave | O1: a plot step abandons the view; the next `EYEPT` returns the block and the region. | Restore the `canvas.region == 0` test in `pg3dEnsure`. |
| G3/G4 audit wave | B1, B2: `pgBeforeSave()` closes an open view and releases an abandoned one. | Empty `pgBeforeSave`. |
| G3/G4 audit wave | E1, E2: a failed undo save refuses `WIREFRAME` and the zoom re-run before a sample runs. | Remove the `ERROR_RAM_FULL` test in `pg3dEngineEnter`. |
| G3/G4 audit wave | H1: a body that erases once and records a line per sample keeps every record intact. | Restore the frozen-only retention test. |
| G3/G4 audit wave | H2: a re-run whose body erases writes no z range into the empty header. | Write the z range on `PG3D_RUN_OK` alone. |
| G3/G4 audit wave | V1: a volume span whose byte scale overflows float is refused. | Drop the scale test in `pg3dSpanUsable`. |
| G3/G4 audit wave | Z1: a zoom re-run over a slice too thin for the byte scale is skipped. | The same. |
| G3/G4 audit wave | W7, W9: a window outside float range is refused by `WIREFRAME` and by the keys; the picture stays. | Remove the window test in `fnWireframe` or in `pg3dKey`. |
| G3/G4 audit wave | W8: a mirrored window is legal in 3D. | Add `mn < mx` to `pg3dWindowUsable`. |
| G3/G4 audit wave | T1 to T3: `ERASE`, `PVIEW` and EXIT return to the home view. | Skip the reset in `pgSetRegion` or in `pgCloseView`. |
| G3/G4 audit wave | T4: a `PT3D` outside the view does not survive into the view. | Keep the NULL test first in `pg3dEmpty`. |
| G3/G4 audit wave | P6: a key press leaves an over-cap mesh as it is. | Remove the nothing-retained return in `pg3dKey`. |
| G3/G4 audit wave | P4, P7, P8, P11, P13, P14, P15, P21, P22, P25, P27 as DESIGN.md section 9.8 specifies. P7 and P8 select the R47 f/g layout for the pin. | Each row's mutation. |
| G3/G4 audit wave, limit | P17: the program pointers are equal before and after `WIREFRAME`. | None reddens it: `execProgram` restores the pointers itself. The pin guards upstream's behaviour. |
| G3/G4 audit wave | B3: `saveCalc()` from the view leaves the previous mode, no block, region 0. | Remove the `pgBeforeSave` call in `saveRestoreBackup.c`. |
| G3/G4 audit wave | R0: the still picture equals the first home redraw. | Change one divisor in the redraw copy. |
| G3/G4 audit wave | S3h: the home redraw count, recorded once. | Any change to the redraw. |

## 5. Baseline measurement, stage G0

A test driver runs three programs of 1,000 steps each through the
interpreter: `NOP` steps, `PIXEL` steps, and, from G2, `LINE` steps. The
driver reads `getUptimeMs()` before and after each run and prints the
three times. The numbers go into the stage commit message and into
DESIGN-HISTORY.md. The difference between the `PIXEL` run and the `NOP`
run is the cost of the `PIXEL` body. A package command must not exceed
the `NOP` run's per-step time.

## 6. Showcase screenshots, stages G2 and G4

Stan asked on 2026-09-04 for one screen that shows every new 2D command and
one screen that shows every new 3D command, with screenshots.

Two drivers do this in the headless suite. `pgTestShowcase2D` opens
`PVIEW 6`, draws one example of every 2D command on the same canvas
(`LINE`, `BOX`, `FBOX`, `CIRCLE`, `FCIRCL`, `ARC`, `TEXTOUT`, `DISP`,
`GCLIP`, `GMODE 2` over an earlier shape, and a `XRNG`/`YRNG` curve when
G3 has landed), then writes the screen to a BMP file with the same path as
`fnScreenDump`. `pgTestShowcase3D` does the same with `EYEPT`, the view
volume, `NUMX`, `NUMY`, `WIREFRAME` of a saddle surface, and a cube from
`PT3D` and `LINE3D`.

Each driver also pins the picture: it counts the lit pixels of the whole
canvas and compares the count with a value recorded once. A change in any
primitive moves the count. The BMP files go to Stan as PNG images at the
end of G2 and G4, and the GTK simulator gives a second screenshot with the
status bar and the softmenu, through the run-sim skill.

Stan asked on 2026-09-04 for an animation as well: at G4, `pgTestShowcase3D`
also presses the rotation keys itself through the key functions and the
item functions (UP and DOWN, then the items BST and SST for f-UP and f-DOWN,
then RBR and FLGSV for g-UP and g-DOWN, then plus and minus for the
distance), writes one BMP per step, and pins the lit-pixel count of the
first and the last frame. The frames become one GIF of the cube and the
surface rotating about the three axes and zooming, delivered to Stan with
the still pictures.
