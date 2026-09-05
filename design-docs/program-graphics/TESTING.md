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
| G1 | `PVIEW 2` sets calcMode 21 and clears rows 20 to 170 only. | Clear to row 239. |
| G1 | `PVIEW 6` clears rows 20 to 239. | Clear to row 170. |
| G1 | A program draws with `PIXEL`, runs `PVIEW 2`, stops. The pixels are lit after the stop. | Remove the refreshScreen case for mode 21. |
| G1 | A program without `PVIEW` loses its drawing at the stop, as upstream. | None needed. This pin guards upstream behaviour; it is red when the case leaks into `CM_NORMAL`. |
| G1 | A softkey press in the view changes nothing. | Remove the range clause from one of the three softkey functions. |
| G1 | A direct key other than EXIT and R/S in the view changes nothing. | Remove the containment arm. |
| G1 | EXIT restores the previous mode and repaints. | Skip the calcMode restore. |
| G1 | `VIEW` inside the view paints nothing. | Let the mode 21 case call `_refreshNormalScreen`. |
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
