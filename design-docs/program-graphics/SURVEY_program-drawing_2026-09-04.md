# Program graphics: survey of what exists

Date: 2026-09-04. Status: read-only survey, no code changed.
Scope: what a user-written RPN program can draw on the screen today, and
what code a new package can reuse. Line numbers refer to `src/c47` in this
working tree (upstream line 00.109.04.00b0, merge 3de5b4be0). A path with
no prefix is under `src/c47`. Other paths carry their prefix.

Every claim below carries a file and line. Claims marked "from code" come
from reading the source. No claim in this document was run on the simulator.
"Package" names the new package this survey prepares for. It has no name yet.

## 1. Summary

1. Upstream already has HP-42S style program graphics: `CLLCD`, `CLLCDxy`,
   `PIXEL`, `POINT`, `AGRAPH`, `CLD`, and the `GRAMOD` variable. All are
   programmable. They sit in the P.FN menu, page 1 (`softmenus.c:331-333`).
2. A drawing made with these commands does not survive a normal program
   stop. The stop path repaints the stack over it (`lblGtoXeq.c:1023-1025`,
   `screen.c:6066`). `PAUSE` and `SNAP` are the only ways to show it.
3. Upstream has reusable plot helpers in `plotstat.c`: a Bresenham line
   stepper, real to pixel transforms, tick rounding, marker shapes. A
   program cannot call them.
   They serve the interactive plot views only.
4. There is no circle, arc, polygon, flood fill, image blit, double buffer,
   pan, or trace anywhere in the tree. There is no 3D drawing code.
5. For 3D math, upstream has cross and dot products, vector normalise, a
   real matrix multiply, plus rectangular to spherical conversions. All run
   in decimal `real_t` arithmetic. A `real_t` to `double` bridge exists, and
   the function grapher already uses it for per-sample work.
6. Four package precedents show how to own the screen: an overlay inside an
   existing mode (Forth console), the manual-paint protocol that `PIXEL`
   uses (PSHOW), and a package `calcMode` in the 19 to 23 registry (the two
   browsers).

## 2. What a user program can draw today

### 2.1 The commands

| Item | Row | Function | Programmable | Effect |
|---|---|---|---|---|
| `AGRAPH` 1409 | `items.c:3261` | `fnAGraph` `screen.c:6604` | yes, `PTP_REGISTER` | Writes the bits of a short integer register as a vertical pixel column at (X,Y). `GRAMOD` selects OR, overwrite, clear, or XOR (`screen.c:6629-6633`). Then `INC X`. |
| `CLLCDxy` 1423 | `items.c:3275` | `fnClLcd(CLLCD_XY)` `screen.c:6521` | yes, `PTP_NONE` | Clears from (X,Y) to the bottom right corner. |
| `PIXEL` 1548 | `items.c:3400` | `fnPixel` `screen.c:6552` | yes, `PTP_NONE` | Sets one pixel at (X,Y). Negative X draws a vertical line at |X|. Negative Y draws a horizontal line at |Y|. |
| `POINT` 1551 | `items.c:3403` | `fnPoint` `screen.c:6576` | yes, `PTP_NONE` | Same as `PIXEL` with a 3 by 3 dot and 2 pixel wide lines. |
| `CLLCD` 2772 | `items.c:4666` | `fnClLcd(CLLCD_FULL)` `screen.c:6521` | yes, `PTP_NONE` | Clears the whole screen, status bar included. |
| `CLD` 2771 | `items.c:4665` | `fnClDisplay` `screen.c:6543` | yes, `PTP_NONE` | Clears the temporary information line. Forces a repaint when a program runs. Comment: "same the 42S CLD". |
| `GRAMOD` 1201 | `items.c:3042` | `itemToBeCoded` | no (stub) | The item is a stub. The reserved variable exists (`defines.h:1235`, `registers.c:102`). `STO GRAMOD` accepts 0 to 3 only (`store.c:173-183`). |
| `SNAP` 1405 | `items.c:3257` | `fnSNAP` `screen.c:6293` | yes | Screenshot. Does not repaint before the dump when a program drew the screen (`screen.c:6299`). |

Text output from a program goes to the register lines. It does not use pixels: `VIEW`,
`AVIEW`, `42AVIEW`, `MSG`, `SHOW` (`items.c:1919, 3874, 4683, 3348, 3594`).

The stubs for these functions in `items.c:1403-1406` are the test-suite
build variants. The real bodies are in `screen.c`.

### 2.2 Coordinate model of the commands

| Fact | Evidence |
|---|---|
| X register gives x, Y register gives y. Both must be integers. A value outside the open range (-max, max) is an error. | `_getPositionFromRegister` `screen.c:6458-6514`, `getPixelPos` `screen.c:6516` |
| Origin is the bottom left corner. The pixel row is `SCREEN_HEIGHT - y - 1`. | `screen.c:6563` |
| x runs 0 to 399 left to right. y runs 0 to 239 bottom to top. | `SCREEN_WIDTH`, `SCREEN_HEIGHT` `defines.h:1510-1511` |
| Every command sets the manual bits for stack, menu, and shift status, so the normal repaint does not touch those areas. It sets the status bar bit only when the drawing reaches the T line band. | `screen.c:6556-6559` |
| Every command sets `screenHoldsDrawnPixels`. | `screen.c:6534, 6557, 6581, 6621` |

Colour constants have names that invert their effect. `LCD_SET_VALUE` is 0
and clears to white. `LCD_EMPTY_VALUE` is 255 and paints black. The
simulator's fill maps a non-zero value to a bit OR, and a set bit is a dark
pixel (`src/c47-gtk/hal/lcd.c:193`, `:67`; `defines.h:1512-1513`). The header
comment at `hal/lcd.h:36-37` says the opposite. Trust the code: `clearScreen`
uses `LCD_SET_VALUE` (`screen.h:99`), and the screen is white after it.

### 2.3 What happens to the drawing

| Moment | Behaviour | Evidence |
|---|---|---|
| While the program runs | No content repaint between steps. Drawings go straight into the frame buffer. The simulator flushes the buffer about once a second while a program runs. The hardware flush cadence during a run was not established. | `lblGtoXeq.c:997-998`; `items.c:362` (`PC_BUILD` only); `screen.c:1830-1859` |
| After each step | The run loop resets `screenUpdatingMode` to auto. This is harmless while no repaint happens. | `lblGtoXeq.c:997-998` |
| `PAUSE n` | Flushes the buffer once, then waits n times 100 ms. It repaints only if a key aborted the program. | `input.c:196, 242, 281-293` |
| `VIEW`, `AVIEW`, `CLD` inside a program | Force a full repaint at once. They destroy the drawing. | `display.c:3990-3994`; `screen.c:6545-6547` |
| `STOP`, `RTN`, `END`, or run-off at subroutine level 0 | Sets the mode to auto, then the stop path calls `refreshScreen(4)`. That call clears `screenHoldsDrawnPixels` and repaints the stack. The drawing is gone. | `items.c:464-470`; `lblGtoXeq.c:798-802, 1023-1025`; `screen.c:6066` |
| `STOP` inside a subroutine (level above 0) | The manual bits stay set, so the stop path skips the repaint. The drawing stays until the next key. From code only. Not run. | `items.c:466-469`; `lblGtoXeq.c:1023` |
| A run that ends with a plot view open | The stop path switches to `CM_GRAPH` and repaints the plot. The stack stays hidden. | `lblGtoXeq.c:1020-1022`; `screen.c:6234-6253` |
| `SNAP` after the drawing | Dumps the raw screen without a repaint first. | `screen.c:6299` |

Jaco added `screenHoldsDrawnPixels` on 2026-07-30 in upstream commit
09856a1c0 ("Fixes: SNAP during run"). The intended use is: draw, then
`SNAP` or `PAUSE` inside the program.

### 2.4 Programmed plots

A program can also start the interactive plot views.

| Command | Row | What it plots | Mode it sets | Evidence |
|---|---|---|---|---|
| `PGMSLV lbl` then `PLTf` | `items.c:3399, 4621` | The named program, run once per x sample through `execProgram`. | `CM_GRAPH` | `solve.c:56-75, 211-268, 369`; `graphs.c:292-322` |
| `Draw`, `Draw↑↓` | `items.c:4231-4232` | The stored equation, or the program under `SOLVER_STATUS_RPN_GRAPHER`. | `CM_GRAPH` | `solver/graph.c:2754` |
| `PLSTAT` | `items.c:3896` | The Σ+ data. | `CM_GRAPH` | `solver/graph.c:1961-1985` |
| `SCATR`, `CENTRL`, `HPLOT`, `HNORM`, `ZOOM` | `items.c:3401, 3608, 3644-3645, 3614` | The Σ+ data, histogram, zoom. | `CM_PLOT_STAT` | `plotstat.c:1909, 1992, 2006` |

Sampling is adaptive. It starts near 40 samples across the width and refines
on curvature (`solver/graph.c:1140-1148, 488`). Samples go into a named
matrix register `DrwMX` (`solver/graph.c:194-241`). The pixels are drawn
later, on every refresh in `CM_GRAPH` (`graphs.c:823`).

The tools inside those views are keyboard only. `LINE`, `BOX`, `CROSS`,
`X.AXIS`, `Y.AXIS`, `-ZOOM`, `+ZOOM`, `CXPLT`, `IMPLT`, `ASSESS`, `NXTFIT`
are `CAT_NONE` or `PTP_DISABLED` (`items.c:3865-3871, 3890-3891, 4243, 4622,
3611-3613`). The program editor stores nothing for them
(`programming/manage.c:1518-1682`).

## 3. The primitives underneath

### 3.1 Frame buffer

| Fact | Evidence |
|---|---|
| 400 by 240 pixels, 1 bit per pixel, one shared buffer for hardware and simulator. | `defines.h:1510-1511`; `c47.h:240`; `c47.c:137` |
| One row is 2 header bytes plus 50 data bytes. Byte 0 is a dirty flag. | `hal/lcd.h:34-35`; `dmcp.h:111-112` |
| The bit order is mirrored in x. Pixel 399 is byte 0 bit 0. | `src/c47-gtk/hal/lcd.c:129, 46` |
| No second buffer and no page flip exist. | search of `src/c47*` for backBuffer, doubleBuffer, pageFlip: none |

### 3.2 Pixel and fill primitives

| Function | Where | Clipping |
|---|---|---|
| `setBlackPixel`, `setWhitePixel`, `flipPixel` | `hal/lcd.h:121, 131, 141` | Through `bitblt24`. The simulator and test builds check x only. They do not check y (`src/c47-gtk/hal/lcd.c:119-170`; `src/testSuite/hal/lcd.c:22-58`). The hardware body is in the DMCP ROM and is unknown. |
| `bitblt24(x, dx, y, val, op, fill)` | `dmcp.h:87`; sim `src/c47-gtk/hal/lcd.c:119` | Same. |
| `lcd_fill_rect(x, y, dx, dy, val)` | `dmcp.h:101`; sim `src/c47-gtk/hal/lcd.c:174` | Rejects the whole call when any edge is off screen. It does not draw the part that fits. Hardware unknown. |
| `lcd_buffer_pixel_on(x, y)` | `src/c47-gtk/hal/lcd.c:41`; `src/testSuite/hal/lcd.c:74` | Read back one pixel. Checks both axes. The only read primitive. |
| `placePixel`, `removePixel` | `plotstat.c:249, 256` | Guarded on both axes. Plot code only. |

### 3.3 Text

| Function | Where |
|---|---|
| `showGlyphCode(code, font, x, y, videoMode, leadCols, endCols, noPreClear)` | `screen.c:1159` |
| `showGlyph(ch, font, x, y, ...)` | `screen.c:1300` |
| `showString(s, font, x, y, videoMode, leadCols, endCols)` | `screen.c:1440` |
| `showStringC47(s, mode, comp, x, y, ...)` with modes `stdNoEnlarge`, `stdEnlarge`, `stdnumEnlarge`, `numSmall`, `numHalf` | `screen.c:1481`; modes `screen.h:120-124` |
| `stringWidthC47`, `getStringBounds` | `screen.c:1548, 1336` |

Fonts: `numericFont` (32 pixel cell), `numericFontBold`, `standardFont` (20
pixel cell), `tinyFont` (8 pixel cell) (`c47.h:276`; glyph data is
generated at build time from `res/fonts/*.ttf`). "Mini" and "enlarge" are
runtime modes of `showGlyphCode` (`screen.c:1458`). They are not separate fonts.

### 3.4 Refresh model

| Fact | Evidence |
|---|---|
| `refreshScreen(source)` dispatches on `calcMode`. Each full-screen view has its own case and paints its own content. The stack repaint does not run. | `screen.c:6065, 6146-6277` |
| `screenUpdatingMode` bits suppress the clear and repaint of the status bar, stack, menu, and shift status. | `defines.h:2027-2036`; `screen.c:5665` |
| Many places reset the mode to auto. Examples: key release, program start, program stop, menu change, mode change. | 100 sites, for example `keyboard.c:969`, `lblGtoXeq.c:911, 997`, `softmenus.c:3794` |
| The simulator flushes the buffer through a throttle. The hardware flushes through `lcd_refresh` and its variants. | `screen.c:1830-1872`; `dmcp.h:94-98` |

### 3.5 Absent primitives

| Missing | Evidence |
|---|---|
| Circle, arc, polygon, flood fill | Search of `src/c47`, `src/c47-gtk`, `src/testSuite` for circle, arc, polygon: only glyph names, the `arcsin` family, plus one hand-coded 11 by 11 dot table (`addons.c:3459-3574`). |
| Image blit | `lcd_draw_img`, `lcd_draw_img_direct`, `lcd_draw_img_part` are declared (`dmcp.h:105-107`) and never called. The simulator does not define them. |
| Filled rectangle outline other than `lcd_fill_rect` | `plotrect` draws four lines (`plotstat.c:294`). |
| DMCP text console | `lcd_setXY`, `lcd_print`, `lcd_fillLine` are unused outside one debug branch (`c47.c:621`). |

## 4. The plot subsystem: what a package can reuse

The subsystem lives in `plotstat.c`, `c47Extensions/graphs.c`, and
`solver/graph.c`. The plot area is the right 240 by 240 square, from x 160
(`plotstat.h:44-46` and inline `SCREEN_WIDTH - SCREEN_HEIGHT_GRAPH`).

### 4.1 Reusable as they are

| Function | Where | Why |
|---|---|---|
| `pixelline(xo, yo, xn, yn, vmNormal)` | `plotstat.c:540` | Integer Bresenham stepper. Calls `placePixel` or `removePixel` per step. No globals. |
| `plotline1`, `plotline2` | `plotstat.c:316, 321` | 1 pixel and 3 pass lines over `pixelline`. |
| `plotcross`, `plotplus`, `plotbox`, `plotbox_fat`, `plotrect` | `plotstat.c:270-310` | Markers and an outline rectangle. Pixel coordinates only. |
| `screenWindowRatio`, `screen_window_x_r`, `_screen_window_y_r` and float wrappers | `plotstat.c:130-245` | Real to pixel affine map with clamping. |
| `auto_tick(double)` | `plotstat.c:765` | Rounds a spacing to 1, 1.5, 2, 5, 7.5 times a power of ten. |
| `graphRangeGuard(lo, hi)` | `solver/graph.c:2722` | Swaps a reversed range and widens a degenerate one. |

### 4.2 Reusable with changes

| Function | Where | What binds it |
|---|---|---|
| `graphAxisDraw`, `graph_axis` | `plotstat.c:574, 831` | Hard-coded plot square and global bounds. |
| `graph_plotmem`, `graphPlotstat` | `graphs.c:823`; `plotstat.c:1241` | Good draw loops, tied to `calcMode`, `plotStatMx`, and menus. |
| `graph_Include0` | `graphs.c:604` | Range shaping on globals. |
| `graph_eqn` | `solver/graph.c:1056` | Adaptive sampler that writes into a matrix register. |
| `fnPMzoom`, `fnPlotZoom` | `graphs.c:130, 173` | Zoom about the centre. There is no pan. |

### 4.3 Limits of the plot views

No pan. No trace cursor. Interaction is six softkeys plus BST and SST
(`keyboard.c:2440, 4556-4559`). Curve-fit sampling caps at 50 intervals and
2000 iterations (`plotstat.h:14`; `plotstat.c:1668-1675`). `graph_dx` and
`graph_dy` are dead (`plotstat.c:833-847`). The plot code mixes `double`
and `float` with `real_t` in shipped code (`graphs.c:194-227`;
`plotstat.c:14-24`; `solver/graph.c:326`).

Upstream commit e989d752b (2026-07-30) removed double precision `pow`,
`exp`, `log`, `log10`, and `sqrt` from the link for size. The plot code now
uses `sqrtf` and multiplication. A package that pulls those functions back
in pays their flash cost. Measure it.

## 5. Other screen owners and the package precedents

### 5.1 Upstream views that own the screen

| View | Paint function | Mode |
|---|---|---|
| Register browser | `registerBrowser` `browsers/registerBrowser.c:160` | `CM_REGISTER_BROWSER` 5 |
| Flag browser | `flagBrowser` `browsers/flagBrowser.c:57` | `CM_FLAG_BROWSER` 6 |
| Font browser | `fontBrowser` `browsers/fontBrowser.c:94` | `CM_FONT_BROWSER` 7 |
| Assign browser | `fnAsnDisplay` `browsers/asnBrowser.c:15` | `CM_ASN_BROWSER` 17 |
| Matrix editor | `showMatrixEditor` `ui/matrixEditor.c:489` | `CM_MIM` 12 |
| Plot views | `graph_plotmem`, `graphPlotstat` | `CM_GRAPH` 15, `CM_PLOT_STAT` 8 |
| Bug screen | `displayBugScreen` `error.c:352` | `CM_BUG_ON_SCREEN` |
| Timer | `fnShowTimerApp` `timer.c:523` | `CM_TIMER` 14 |

Each mode has its own case in `refreshScreen` and in the key dispatch
(`screen.c:6146-6277`; `keyboard.c:3568-3578, 3930-3989`).

### 5.2 Package precedents

| Package | Way to own the screen | Evidence |
|---|---|---|
| forth-core console | Overlay inside `CM_AIM`, gated on five conditions. Paints the T, Z, Y band and leaves X live. Falls through to the stock paint when the gate fails. | `forth_console_view.c:123-188`; `packages/forth-core/screen.c:5940-5972` |
| pretty-print PSHOW | The `PIXEL` protocol: manual bits plus `screenHoldsDrawnPixels`, plus `TI_SHOWNOTHING` so the stock EXIT path closes it. No `calcMode`. Not programmable (`PTP_DISABLED`). | `prettyValue.c:881`; `packages/pretty-print/items.c:2295`; `design-docs/pretty-print/DESIGN.md:238-255` |
| pretty-print-extra browser | Own `calcMode` 20 in the package registry 19 to 23. Own case in `refreshScreen`. Restores the previous mode on exit. | `packages/pretty-print-extra/defines.h:1714`; `screen.c:6163-6168`; `prettyBrowser.c:110-144` |
| undo-history browser | Same pattern, `CM_HIST_BROWSER`. | `packages/undo-history/defines.h:1723`; `historyBrowser.c:94` |

The registry rule is recorded in `design-docs/pretty-print/DESIGN-HISTORY.md:912-916`
and enforced as `calcMode < 19 /* package browsers 19-23 */` in the key
paths (`packages/pretty-print-extra/keyboard.c:690, 832, 940`).

## 6. 3D: nothing drawn, some math to reuse

No 3D drawing code exists. Searches of `src/c47` for rotate, rotation,
projection, project, transform, isometric, wireframe, perspective, sinTable,
cosTable, and "fast sin" found only unrelated hits (alpha rotate, bit
rotate, prose). The upstream branch `Vectors-2D-3D-cached-test` is about
vector data types. It does not draw.

| Math | Where | Type | Fit for per-frame 3D |
|---|---|---|---|
| `crossRealVectors` | `mathematics/matrix.c:3266` | `real34` in, `real_t` at 39 digits | Correct, decimal slow. |
| `dotRealVectors` | `mathematics/matrix.c:3253` | same | Correct, decimal slow. |
| `unitVectorRema` | `mathematics/unitVector.c:70` | same | Normalise in place. |
| `multiplyRealMatrices` | `mathematics/matrix.c:3011` | same | Works for 4 by 4 transforms. One-off use only. |
| `convert3DtoSPH`, `convertSPHto3D`, `convert3DtoCYL`, `convertCYLto3D` | `mathematics/matrix.c:9032-9108` | same | The nearest thing to a rotation. |
| `C47_WP34S_Cvt2RadSinCosTan`, `C47_WP34S_Atan2`, `C47_WP34S_Acos` | `mathematics/wp34s.c:431, 1071, 1237` | `real_t`, 75 digits or more | Software decimal. Not for per-frame work. |
| `realToDouble`, `realToFloat` | `registerValueConversions.c:935, 899` | `real_t` to `double` or `float` | The bridge. Convert once, iterate in `double`. |
| `convertDoubleToReal`, `convertDoubleToReal34Register` | `registerValueConversions.c:798, 806` | back | The return path. |

Precedent: `normCoord` in `solver/graph.c:326` converts to `double` once
and iterates there. The comment says "double maths is O(1) at any data
magnitude". A 3D vector is a 1 by 3 or 3 by 1 `real34Matrix_t`
(`defines.h:2459-2460`). There is no vector data type.

## 7. Test and capture facilities

| Facility | Where |
|---|---|
| Headless frame buffer and pixel read back | `src/testSuite/hal/lcd.c:16-74` |
| Pixel oracles: `ppTestRowAllLit`, `ppTestRowAnyLit`, `ppTestRectAnyLit` | `packages/pretty-print/prettyTest.c:391-413` |
| Pixel test driver `prettyTestPixels` | `prettyTest.c:422`; registered `packages/pretty-print/testSuite/testSuite.c:710` |
| BMP hash oracle for plots: `covBmpName`, `covHashBmp` | `src/testSuite/testSuite.c:2639-2668`; `src/testSuite/tests/graphs_cov.txt` |
| Screenshot to BMP | `fnScreenDump` `screen.c:6324` (`PC_BUILD`); `standardScreenDump` `addons.c:1074` (DMCP) |
| Simulator capture drivers | `.claude/skills/run-sim/references/capture-driver.c`, `console-capture-driver.c` |

## 8. What this means for the package

These are observations. Stan decides.

1. The command vocabulary exists. A package can add lines, circles, filled
   shapes, text at a pixel position, and a viewport, in the same style. The
   coordinate convention is set: integers from the stack, bottom left origin.
2. Persistence is the design question. Today a drawing dies at `STOP`. The
   package needs its own way to keep a drawn screen: a package `calcMode`
   in the 19 to 23 registry, or a flag the stop path honours. The `PSHOW`
   protocol shows the manual-bits route and its EXIT hook.
3. The plot subsystem gives a line stepper and transforms for free, but its
   `placePixel` clips to the plot area and its state is global. A package
   should copy the stepper and own its own clip rectangle.
4. Off-screen writes are unsafe. `bitblt24` does not check y in the
   simulator, and `lcd_fill_rect` drops whole calls. The package must clip
   before it calls either.
5. 3D needs `double` math and a one-time conversion, following `normCoord`.
   The flash cost of libm trig must be measured, because upstream removed
   the double `pow`, `exp`, `log`, and `sqrt` from the link on purpose.
6. Pixel tests are already possible with the read-back oracle and the BMP
   hash oracle. No new harness is needed.
