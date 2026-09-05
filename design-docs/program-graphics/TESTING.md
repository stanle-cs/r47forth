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

Solo:

    tools/pkg_patch_refresh.py
    make pkg_build PKG=packages/program-graphics

Combined:

    make pkg_build PKG=packages/program-graphics PKG_TEST_WITH=packages/forth-core,packages/undo-history,packages/pretty-print,packages/pretty-print-extra

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
| G2 fix wave | D17b: a string that ends in a lone lead byte is trimmed to a width of one pixel without a read beyond its NUL. | None. The failure of the guard is a hang, so this pin documents the guard and does not falsify it. |
| G2 fix wave | D18: a NaN angle for `ARC` raises `ERROR_OUT_OF_RANGE` and draws nothing. | Accept NaN in the angle reader. |
| G2 | S1: the showcase screen has the recorded count of lit pixels. | Any change to a primitive. |
| G3 | W1: without a window, a real is a pixel rounded half away from zero (2.5 is 3, -0.5 is off the screen). | Round toward zero. |
| G3 | W2: `XRNG` 0 10 and `YRNG` 0 5 map (5, 2.5) to the pixel (200, 120), the corners to (0, 0) and (399, 239), and a long integer stays a pixel. | A scale of 398. |
| G3 | W3: equal ends raise `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and leave the window; a reversed range mirrors the axis. | Skip the equal-ends check. |
| G3 | W4: a real that maps beyond 32767 pixels raises `ERROR_OUT_OF_RANGE`; one that maps to 3990 is clipped without an error. | Clamp instead of the error. |
| G3 | W5: two complex points, the first in Y and the second in X, draw the line; a complex with a long integer is the type error. | Swap the real and imaginary parts. |
| G3 | W6: the window survives `ERASE`. | Clear the window in `pgSetRegion`. |
| G3 | S1: the showcase count includes the sine curve through the window. | Any change to a primitive. |
| G4 | A unit cube from a fixed eye point projects to fixed pixels, recorded once. | Any change in the projection. |
| G4 | `WIREFRAME` of a plane draws a mesh with a known pixel count. | Skip the row lines. |

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
(`LINE`, `BOX`, `RECT`, `CIRCLE`, `FCIRCL`, `ARC`, `TEXTOUT`, `DISP`,
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
