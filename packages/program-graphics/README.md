# program-graphics

This archive contains the `program-graphics` package. It lets a user program draw on the screen of the calculator.

The package adds a canvas view, ten 2D drawing commands, a coordinate window, and a 3D stage. The 3D commands are in the section "3D" of this file.

The package runs alone. It has no dependency on a sibling package.

## The canvas view

A program opens the canvas view with `PVIEW`. The drawing then stays on the screen when the program stops. Without the view, the next screen refresh erases the drawing, as it does for `PIXEL` today.

| Command | Parameter or stack | Effect |
| --- | --- | --- |
| `PVIEW 2` | a step parameter | Opens the view over the register area, rows 20 to 170. The softmenu stays visible. |
| `PVIEW 6` | a step parameter | Opens the view over the register area and the softmenu area, rows 20 to 239. |
| `ERASE` | none | Clears the canvas to white and resets the clip rectangle. When the view is closed, `ERASE` opens it over the register area. |

The status bar stays live in the view. `EXIT` closes the view and shows the stack again. `R/S` runs or stops the program as usual. Every other key does nothing in the view. `VIEW` and `AVIEW` do nothing visible inside the view, and the program continues.

An error inside the view shows on the top line of the canvas, over rows 20 to 39 of the drawing.

## 2D commands

The origin is the bottom left corner of the screen. x goes to the right, y goes up. The screen has 400 columns and 240 rows.

| Command | Stack | Effect |
| --- | --- | --- |
| `LINE` | x1 in X, y1 in Y, x2 in Z, y2 in T | Draws a line from point 1 to point 2. |
| `BOX` | same as `LINE` | Draws the outline of the rectangle with corners at point 1 and point 2. |
| `FBOX` | same as `LINE` | Fills that rectangle. |
| `CIRCLE` | cx in X, cy in Y, r in Z | Draws the outline of a circle. |
| `FCIRCL` | same as `CIRCLE` | Fills a circle. |
| `ARC` | center as a complex number in T, r in Z, a1 in Y, a2 in X | Draws an arc counterclockwise from a1 to a2 in the current angle unit. A span of 360 degrees or more draws the full circle. |
| `TEXTOUT` | x in X, y in Y, a string in Z | Draws the string with the standard font. The point is the top left corner of the text. |
| `DISP n` | n = 1 to 11, a step parameter, a string in X | Draws the string on canvas line n, from the top. Each line is 20 rows tall. |
| `GMODE n` | n = 0, 1, or 2, a step parameter | Sets the draw mode: 0 sets pixels, 1 clears pixels, 2 inverts pixels. |
| `GCLIP` | same as `LINE` | Sets the clip rectangle. Later commands draw inside it only. `ERASE` and `PVIEW` reset it. |
| `XRNG` | xmin in Y, xmax in X | Sets the x range of the window for real coordinates. |
| `YRNG` | ymin in Y, ymax in X | Sets the y range of the window for real coordinates. |

The commands do not consume their arguments. The stack stays as it was.

The commands also work when the view is closed. They then draw on the whole screen, and the next refresh erases the drawing.

## Coordinates

A long integer is a pixel. A real goes through the window. Without `XRNG` and `YRNG`, a real is a pixel rounded half away from zero. With a range set, the real maps onto the 400 columns or the 240 rows of the screen, as a plot does.

A complex number is a point. The real part is x and the imaginary part is y. `LINE`, `BOX`, `FBOX`, and `GCLIP` accept two complex points, the first in Y and the second in X.

A radius is always in pixels.

A point off the screen is not an error. The command draws the part that is on the screen. A coordinate above 32767 pixels is `Out of range`. Equal ends for `XRNG` or `YRNG` are `Argument exceeds function domain`. A string where a coordinate is expected is `Invalid data type for operation`.

## 3D

The 3D commands draw a view volume seen from an eye point, as the HP 48 `WIREFRAME` plot does. x is width, y is depth into the screen, and z is height. The eye must be in front of the near face: its y must be below the low end of `YVOL`.

| Command | Stack | Effect |
| --- | --- | --- |
| `EYEPT` | x in Z, y in Y, z in X | Sets the eye point. The default is (0, -3, 0). |
| `XVOL`, `YVOL`, `ZVOL` | low in Y, high in X | Set the view volume. The default is -1 to 1 on each axis. High must be above low. |
| `NUMX`, `NUMY` | n in X, from 2 to 100 | Set the grid counts of `WIREFRAME`. The defaults are 10 and 8. |
| `WIREFRAME` | the label of a program, a step parameter | Runs the program at every grid point with x in X and y in Y, reads z from X, and draws the mesh. |
| `PT3D` | x in Z, y in Y, z in X | Sets the current 3D point. |
| `LINE3D` | x in Z, y in Y, z in X | Draws a line from the current point to this point. This point becomes the current point. Without a current point it acts as `PT3D`. |

The projection plane sits one unit in front of the eye. Its coordinates go through the window: set `XRNG` and `YRNG` in volume units, for example `XRNG -1 1` and `YRNG -0.6 0.6` for the default volume, or the picture is one pixel wide.

The 3D drawing stays in the view after the program stops, and the keys turn it:

| Key in the view | Effect |
| --- | --- |
| UP, DOWN | Turn about the x axis, 10 degrees per press. |
| f UP, f DOWN | Turn about the y axis. |
| g UP, g DOWN | Turn about the z axis. |
| + , - | Zoom in and out, a factor 1.25 per press, 8 steps each way. |
| 5 | The view as set by `EYEPT`. |

A rotation redraws the 3D content only. 2D drawings on the same canvas are lost at the first press. The eye and the volume freeze at the first 3D drawing; a change takes effect after `ERASE`, `PVIEW`, or `EXIT`. When a zoom step makes one recorded z step wider than a pixel, the program of the last `WIREFRAME` runs again once to record finer values.

The retained 3D content lives in a 2 KB block from the calculator's memory pool while the view is open. It holds a grid of 44 by 44 samples, or a smaller grid and up to 330 lines. A larger grid draws once and does not turn.

### Example: a saddle

Store this program under the global label `SADL`. One step per line.

```text
LBL 'SADL'
x²
x<>y
x²
-
RTN
```

Then run this program:

```text
LBL 'SURF'
PVIEW 6
ERASE
-1
1
XRNG
-0.6
0.6
YRNG
0
-3
0
EYEPT
24
NUMX
24
NUMY
WIREFRAME 'SADL'
STOP
```

The view shows z = x² - y² as a 24 by 24 mesh. Press UP a few times to tilt it. Press 5 to return.

## The menu

The `CANVAS` softmenu holds every command of the package. Open it from the `PFN` menu, or select a command from the function catalog.

## Install

Run these commands from the root of the c43 source tree:

```sh
mkdir -p packages/program-graphics
unzip program-graphics.zip -d packages/program-graphics
```

Build the R47 simulator:

```sh
make simr47 CUSTOM_PKG=packages/program-graphics
```

Build the C47 simulator:

```sh
make sim CUSTOM_PKG=packages/program-graphics
```

Build the R47 image for DMCP5:

```sh
make dmcp5r47 CUSTOM_PKG=packages/program-graphics
```

The package builds together with `forth-core`, `undo-history`, `pretty-print`, and `pretty-print-extra`. Name the packages in one comma-separated `CUSTOM_PKG` value.

## Example: a frame and a diagonal

Store this program under the global label `FRAME`. One step per line.

```text
LBL 'FRAME'
PVIEW 6
ERASE
230
390
10
10
BOX
LINE
STOP
```

Run `FRAME`. The view opens over the full screen. The four numbers are y2, x2, y1, x1, so `BOX` draws the rectangle from (10, 10) to (390, 230). `BOX` leaves the stack as it was, and `LINE` then draws the diagonal between the same two points. The drawing stays on the screen when the program stops. Press `EXIT` to close the view.

## Example: a curve through the window

Store this program under the global label `SINE`. One step per line.

```text
LBL 'SINE'
RAD
PVIEW 6
ERASE
0
6.2832
XRNG
-1.5
1.5
YRNG
0
STO 00
LBL 01
RCL 00
0.1
+
SIN
RCL 00
0.1
+
RCL 00
SIN
RCL 00
LINE
RCL 00
0.1
+
STO 00
6.2832
RCL 00
x<y?
GTO 01
STOP
```

The window maps x from 0 to 6.2832 onto the 400 columns and y from -1.5 to 1.5 onto the 240 rows. Each pass of the loop pushes y2, x2, y1, x1 and draws one segment of the sine curve from x to x + 0.1.

## Limits

- The canvas does not survive sleep or power off.
- Text is not clipped inside its cell, and `GMODE` does not apply to text.
- `RESET` inside the view keeps the view open with a blank canvas.
- A program that opens a plot view abandons the canvas.
- Shifted keys do nothing in the view.
- The DMA refresh of the DM42 is untested by the package. Every number in this file comes from the simulator.

## Compatibility and license

The package targets R47 or C47 on DM42n with DMCP5. It uses c43 commit `af7ad934a0dfeeada96c21136e2ab7084647de01` as its patch base.

The package uses GPL-3.0-only. The archive includes `COPYING`.
