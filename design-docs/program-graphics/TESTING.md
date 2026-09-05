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
| G2 | Each primitive: endpoint pixels lit, one pixel outside each edge clear. | Off-by-one in each endpoint. |
| G2 | Clip: a line that crosses the clip edge stops at the edge. | Remove the clip test in `pgPixel`. |
| G2 | Off-screen arguments leave the buffer unchanged. | Remove the clamp in `pgRun`. |
| G2 | `GMODE 2` twice restores the buffer. | Use set instead of flip. |
| G2 | For 1,000 random pixels, the direct write and `bitblt24` leave the same 52-byte row. | Change the bit order or skip the dirty flag. |
| G3 | `XRNG`, `YRNG`: a real maps to the pixel that `screenWindowRatio` gives. | Off-by-one in the scale. |
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
