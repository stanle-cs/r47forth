# program-graphics: DESIGN.md

Status: version 0, stage G0, 2026-09-04. This document is the authority for
the package. DESIGN-HISTORY.md holds the amendment trail. TESTING.md holds
the test contract. PLAN_2026-09-04.md holds the rulings and the stages.

Writing rules of this document: one fact per sentence, every stateful rule
as pseudocode, no unstated decision. A later stage that changes a rule
amends this file and records the reason in DESIGN-HISTORY.md.

## 1. Scope

The package lets a user program draw on the screen. The drawing stays on the
screen when the program stops. The package adds commands in the style of
the upstream `PIXEL` family. Speed is a law (section 8). Tests run on the
simulator only (section 11).

The package follows the package system of this repository: a working area
under `packages/program-graphics/` that mirrors upstream paths, generated
patches and files, a gate that must be green alone and combined with
forth-core, undo-history, pretty-print, and pretty-print-extra.

## 2. Vocabulary

Names follow one rule: the Plus42 name when one exists, then the RPL name,
then the HP Prime name. A name with no precedent is marked "none". Every
command is programmable. Every command reads the stack and does not change
it. Every command is `US_UNCHANGED` and `SLS_UNCHANGED` (section 8).

### 2.1 The canvas view

| Command | Parameter or stack | Effect | Precedent |
|---|---|---|---|
| `PVIEW n` | n = 2 or 6, a step parameter | Opens the canvas view over region n (section 3). | RPL `PVIEW` and `FREEZE` |
| `ERASE` | none | Clears the canvas region to white and resets the clip rectangle. Opens the view over region 2 when the view is closed. | RPL `ERASE` |

### 2.2 2D drawing, stage G2

| Command | Stack | Effect | Precedent |
|---|---|---|---|
| `LINE` | x1 in X, y1 in Y, x2 in Z, y2 in T | Draws a line from point 1 to point 2. | Plus42, RPL |
| `BOX` | same | Draws the outline of the rectangle with corners at point 1 and point 2. | RPL |
| `FBOX` | same | Fills the rectangle with corners at point 1 and point 2. Upstream already has a programmable `RECT`, the rectangular complex mode, so the Prime name is not available. | none |
| `CIRCLE` | cx in X, cy in Y, r in Z | Draws the outline of a circle. | none |
| `FCIRCL` | same | Fills a circle. | none |
| `ARC` | center as a complex number in T, r in Z, a1 in Y, a2 in X | Draws an arc counterclockwise from a1 to a2, in the current angle unit. A span of 360 degrees or more draws a full circle. | RPL |
| `TEXTOUT` | x in X, y in Y, a string in Z | Draws the string with the standard font. The point is the top-left corner of the text cell. C47 has no alpha register; strings live in registers. | Prime |
| `DISP n` | n = 1 to 11, a step parameter; a string in X | Draws the string on canvas line n, from the top. | RPL |
| `GMODE n` | n = 0, 1, or 2, a step parameter | Sets the draw mode: 0 sets pixels, 1 clears pixels, 2 inverts pixels. | RPL `TLINE`, 42S `GRAMOD` |
| `GCLIP` | x1 in X, y1 in Y, x2 in Z, y2 in T | Sets the clip rectangle. Later commands draw inside it only. `ERASE` and `PVIEW` reset it to the region. | none |

### 2.3 User coordinates, stage G3

| Command | Stack | Effect | Precedent |
|---|---|---|---|
| `XRNG` | xmin in Y, xmax in X | Sets the x range of the window. | RPL |
| `YRNG` | ymin in Y, ymax in X | Sets the y range of the window. | RPL |

### 2.4 3D, stage G4

| Command | Stack | Effect | Precedent |
|---|---|---|---|
| `EYEPT` | x in Z, y in Y, z in X | Sets the eye point. Takes effect at the next record after `ERASE`, `PVIEW`, or EXIT (section 9.2.5). | RPL |
| `XVOL`, `YVOL`, `ZVOL` | low in Y, high in X | Set the view volume. High must be above low, else `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`. | RPL |
| `NUMX`, `NUMY` | n in X, an integer from 2 to 100 | Set the grid counts. Else `ERROR_OUT_OF_RANGE`. | RPL |
| `WIREFRAME` | the label of a user program, a step parameter | Runs the program at every grid point with X = x, Y = y, Z = x, T = y, reads z from X, draws the mesh, and retains the z bytes (section 9.4). | RPL |
| `PT3D` | x in Z, y in Y, z in X | Sets the current 3D point. | none |
| `LINE3D` | x in Z, y in Y, z in X | Draws a 3D line from the current point to this point and retains it. This point becomes the current point. Without a current point it acts as `PT3D`. | none |

The keys of the view are in section 9.6. UP and DOWN turn about x. f-UP
and f-DOWN turn about y. g-UP and g-DOWN turn about z. Plus and minus
zoom. The key 5 returns to the `EYEPT` view.

### 2.5 Upstream commands that do not change

`PIXEL`, `POINT`, `AGRAPH`, `CLLCD`, `CLLCDxy`, `CLD`, `SNAP`. Inside the
canvas view, `VIEW` and `AVIEW` do nothing visible, and the program
continues (section 3.6).

## 3. The canvas view

### 3.1 Mode value

The canvas view is `calcMode` value 21. The package registry reserves 19 to
23 for packages. Undo-history uses 19. Pretty-print-extra uses 20. The
`defines.h` line follows the format of those two packages, byte for byte
except the name and the value:

    #define CM_GRAPHICS_CANVAS                        21 // program-graphics canvas view (package browsers 19-23, claims registry)

### 3.2 Regions

| Region code | Rows | Content |
|---|---|---|
| 2 | 20 to 170 | The four register lines. |
| 6 | 20 to 239 | The register lines and the softmenu. |

The status bar, rows 0 to 19, stays live in both regions. Region code 1 is
not supported. `PVIEW` with a parameter other than 2 or 6 raises
`ERROR_OUT_OF_RANGE` and does nothing else.

### 3.3 State

One static struct in `pgmGraphics.c`. All fields are zero at boot.

    typedef struct {
      uint8_t   region;        // 0 = view closed, else 2 or 6
      uint8_t   prevCalcMode;  // the calcMode to restore on EXIT
      uint8_t   drawMode;      // GMODE: 0 set, 1 clear, 2 invert
      uint8_t   errorShown;    // 1 while an error message is painted on canvas line 1
      int16_t   clipX0, clipY0, clipX1, clipY1;   // screen coordinates, top-left origin, inclusive
      uint32_t  lastRefreshMs;                     // section 8.4
    } pgCanvas_t;

Stage G3 adds the window (section 5.2). Stage G4 adds the 3D state
(section 9.1). The struct is not saved in a backup. A restart closes the
view.

### 3.4 Coordinate frames

Commands read user-facing coordinates: origin bottom left, x to the right,
y upward. The screen buffer uses the top-left origin with y downward. The
conversion is one subtraction:

    row = SCREEN_HEIGHT - 1 - y

Every internal function of the package works in screen coordinates. The
clip rectangle is stored in screen coordinates.

### 3.5 PVIEW

    fnPview(n):
      if n != 2 and n != 6: error ERROR_OUT_OF_RANGE; return
      if calcMode != CM_GRAPHICS_CANVAS:
        canvas.prevCalcMode = calcMode
      canvas.region = n
      canvas.clipX0 = 0; canvas.clipX1 = SCREEN_WIDTH - 1
      canvas.clipY0 = 20
      canvas.clipY1 = (n == 2) ? 170 : SCREEN_HEIGHT - 1
      clear rows 20 to clipY1 to white with lcd_fill_rect
      calcMode = CM_GRAPHICS_CANVAS
      temporaryInformation = TI_NO_INFO
      screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS
      screenHoldsDrawnPixels = true
      if n == 2: showSoftmenuCurrentPart()
      pgRefreshNow()

A second `PVIEW` while the view is open changes the region and clears the
new region. It does not change `prevCalcMode`.

### 3.6 Behaviour of the view

| Event | Rule |
|---|---|
| `refreshScreen` runs while `calcMode == CM_GRAPHICS_CANVAS` | The case for the mode calls `refreshStatusBar()`, then `showSoftmenuCurrentPart()` when region is 2, then `force_refresh(force)`. It does not clear or paint any other row. |
| A program stops | The stop path calls `refreshScreen(4)`. The rule above keeps the canvas. |
| R/S, direct key path | The press arm sets `showFunctionNameItem = 0` and marks the key processed. The release path sets `showFunctionNameItem = ITM_RS` for the canvas view, as it does for `CM_NORMAL` but without the register line paint. Upstream then runs `fnRunProgram`. The view stays open. |
| EXIT, direct key path | `fnKeyExit` has a case for the mode that calls `pgCloseView()`: `calcMode = canvas.prevCalcMode`; `canvas.region = 0`; `temporaryInformation = TI_NO_INFO`; `screenUpdatingMode = SCRUPD_AUTO`; `refreshScreen(197)`. |
| Any other key, direct key path | Ignored. A guard arm in `processKeyAction` before the SNAP arm marks every key except SNAP as processed. `fnKeyEnter`, `fnKeyBackspace`, `fnKeyUp`, `fnKeyDown`, and `fnKeyDotD` have a no-op case for the mode, because their default arm shows a bug screen. |
| A key with its own case above the guard arm (ENTER, CC, EXIT) | The press reaches `showFunctionName`. In mode 21 that function records the item and paints nothing. The release reaches `hideFunctionName`, which in mode 21 clears the item and repaints no register line. Both arms live in the package's screen.c mirror. Audit G2 round 1, U7. |
| Any softkey, softkey path | Ignored. The package carries the range clause `calcMode < 19 /* package browsers 19-23, claims registry */` on the three softkey functions, byte-identical to the other packages. |
| `VIEW`, `AVIEW` inside the view | The upstream body sets `temporaryInformation` and calls `refreshScreen`. The rule above paints nothing. The program continues. On EXIT, `temporaryInformation` is reset, so nothing shows later. |
| A program step whose item function switches on `calcMode`: `ENTER`, `CC`, `.d`, `.ms` | The function runs as in `CM_NORMAL` while a program runs. The package helper `pgEffectiveCalcMode()` returns `CM_NORMAL` when `calcMode` is 21 and `programRunStop` is running, else `calcMode`. The four functions switch on it. From the keyboard, the same keys do nothing. Audit G1 round 1, finding G1R1-1 and G1R1-3. |
| An error inside the view | Nothing paints at once. The next `refreshScreen` in mode 21 clears canvas line 1 (rows 20 to 39) and writes the error text there. When the error is gone, the next refresh clears the band again. The register line painter `refreshRegisterLine` returns at once in mode 21, so the upstream error line and any other register line never paint over the canvas. Audit G1 round 1, finding G1R1-4. |
| EXIT with an error pending | Upstream consumes the EXIT press to clear the error. The view stays open. A second EXIT closes it. |
| Shifted keys, f and g | From G4 on the shift keys engage in mode 21. f UP, f DOWN, g UP and g DOWN turn the 3D drawing (section 9.6). SNAP works. Every other shifted item does nothing in the view. SNAP on the R47 keyboard is a long press of EXIT. |
| `PAUSE` inside the view | Upstream flushes the buffer once and waits. The canvas stays. |
| `CLLCD` inside the view | Upstream clears the whole screen, status bar included. The view stays open. The status bar repaints at the next refresh. |
| A program step that calls `calcModeNormal()`, such as `CLSTK` or `CLA` | `calcModeNormal` returns at once while the view is open. The view and the drawing stay. The package patches `calcMode.c` for this. |
| A program step that opens a plot view (`Draw`, `PLTf`, `PLSTAT`, `SCATR`, `HPLOT`) or stores a non-finite plot range | The plot takes the screen and sets its own mode. The canvas is abandoned and the next repaint erases it. By design: the program asked for a plot. `canvas.region` stays set. Its readers outside mode 21 are `fnGclip`, which sizes a clip that `pgClipNow` ignores while the view is closed, and `pgRefreshCanvasView`, which runs in mode 21 only. `fnErase` tests `calcMode`, not the region, so an `ERASE` after a plot step reopens the view over region 2 with `prevCalcMode` set to the plot mode. Audit G2 round 1, in-family 15. |
| Sleep or power off | Upstream repaints on wake. The canvas is lost. Documented limit. |

### 3.7 ERASE

    fnErase():
      if calcMode != CM_GRAPHICS_CANVAS: fnPview(2); return
      clear rows 20 to canvas.clipY1 (of the region) to white
      reset the clip rectangle to the region, as in fnPview
      if canvas.region == 2: showSoftmenuCurrentPart()
      pgRefreshNow()

## 4. The drawing kernel, stage G2

### 4.1 Pixel access

The kernel writes the screen buffer directly. It does not call `bitblt24`
per pixel. Reason: `bitblt24` is a function call with a range check, a mask
computation, and a byte loop per pixel (`src/c47-gtk/hal/lcd.c:119-170`).
A direct write is one address computation and one byte operation. For
horizontal runs the kernel writes whole bytes, eight pixels at a time.

The buffer layout is the same on the DM42 and on the simulator
(`c47.c:618`: `lcd_buffer = lcd_line_addr(0) - 2`). One row is 52 bytes:
byte 0 is the dirty flag, byte 1 is the row number, bytes 2 to 51 hold
400 pixels. The bit order is mirrored: pixel x sits at bit
`(SCREEN_WIDTH - 1 - x) & 7` of byte `2 + ((SCREEN_WIDTH - 1 - x) >> 3)`.
A set bit is a black pixel. The refresh pushes a row when its flag byte is
not zero (`src/c47-gtk/hal/lcd.c:89-100`), and `bitblt24` sets the flag to 1
after a write (`lcd.c:169`). The kernel does the same.

    pgRowPtr(row) = lcd_buffer + row * 52
    pgMark(row):   pgRowPtr(row)[0] = 1

    pgPixel(col, row):
      if col < canvas.clipX0 or col > canvas.clipX1: return
      if row < canvas.clipY0 or row > canvas.clipY1: return
      xm = SCREEN_WIDTH - 1 - col
      p = pgRowPtr(row) + 2 + (xm >> 3)
      bit = 1 << (xm & 7)
      switch canvas.drawMode:
        0: *p |= bit
        1: *p &= ~bit
        2: *p ^= bit
      pgMark(row)

    pgRun(col0, col1, row):        // horizontal run, col0 <= col1
      clamp col0, col1 to the clip rectangle; if empty return
      if row outside the clip rectangle: return
      convert to mirrored bit positions; split into a head byte mask, whole bytes, a tail byte mask
      apply the mode to the head mask, the whole bytes (0xFF), and the tail mask
      pgMark(row)

`ERASE` and `PVIEW` clear the region with one `lcd_fill_rect` call, as
upstream does. `FBOX` calls `pgRun` per row in every mode.

The hardware assumption of this section is: the DMCP ROM's refresh treats
byte 0 of a row as the dirty flag, as the simulator does. Evidence:
upstream's own hardware code writes the header (`screen.c:695-696`) and
the simulator's `bitblt24` was written to mirror the ROM. The package
cannot test this on the DM42 (section 11). The test suite pins that a
direct write and a `bitblt24` write leave the same bytes in the buffer.

### 4.2 Clipping law

The clip rectangle in force is the canvas clip while the view is open. While
the view is closed, the drawing commands still work, as `PIXEL` does, and
the clip is the whole screen, rows 0 to 239.

Every primitive clips before it touches the buffer. `bitblt24` does not
check the row. `lcd_fill_rect` drops the whole call when any edge is off
screen. An off-screen argument is not an error. It draws nothing.

`GCLIP` stores the intersection of its rectangle and the region. When the
intersection is empty, the clip is empty (clipX0 = 1, clipX1 = 0) and
nothing draws until the next `GCLIP`, `ERASE`, or `PVIEW`. Only values
inside the region reach the int16 clip fields.

### 4.3 Lines

Integer Bresenham, one `pgPixel` per step. Horizontal lines call `pgRun`
once. Vertical lines call `pgPixel` per row. The endpoints are inclusive.

### 4.4 Rectangles

`BOX` draws four lines with `pgRun` for the top and bottom rows and
`pgPixel` for the side columns. `FBOX` clamps the rectangle to the clip
rectangle and calls `pgRun` per row in every mode. `lcd_fill_rect` is not
used for a drawing command, because it drops the whole call when an edge
is off the screen.

### 4.5 Circles and arcs

`CIRCLE` uses the midpoint algorithm with `pgPixel`. `FCIRCL` uses the
same algorithm and calls `pgRun` for each row of the clip that the circle
crosses. Its half-width per row is the rounded integer square root of
4 (r squared minus dy squared), computed in 64 bits, because the product
by four exceeds int32 from radius 23171 on. `ARC` reads the start and end
angles in the current angular mode. A NaN or infinite angle raises
`ERROR_OUT_OF_RANGE`. The command then steps the circle with the midpoint
algorithm and draws the pixels whose direction lies in the span. The span
test is two integer cross products against the two direction vectors of
the angles, each scaled by 65536, computed once with the WP34S sine and
cosine. The resolution of the span is about 0.001 degrees. A span of 360
degrees or more draws the full circle. When the two vectors coincide at
that scale, the exact span in degrees decides: 180 degrees or more draws
the full circle, less draws one pixel. The arc stepper `pgArc` is the one
internal function that takes its center row in the user frame and
converts at each plot; `pgCircle` takes a screen row. Audit G2 round 1,
in-family 3 and 17.

### 4.6 Text

`TEXTOUT` draws the string of Z with `showString` and the standard font,
after it cuts the string at the last glyph that fits between the point and
the right edge of the clip rectangle. A cell whose top row or bottom row
is outside the clip rectangle is not drawn. `GMODE` does not apply to
text. `DISP n` draws the string of X the same way at the cell:

    row = 20 + (n - 1) * 20
    col = max(1, clipX0)
    if row < clipY0 or row + 19 > clipY1 or col > clipX1: return
    clear the band rows row..row+19, cols clipX0..clipX1 to white
    showString(the string of X, cut to clipX1 - col + 1, &standardFont, col, row, vmNormal, true, true)

The cut never splits a two-byte glyph, at the width and at the cap of the
scratch buffer. The cut position is found by a walk from the start of the
string, because a second byte can carry bit 7 as well. A lone lead byte at
the end is cut away before the width is measured, whatever the width.

Region 2 has lines 1 to 7. Region 6 has lines 1 to 11. `n` outside the
region draws nothing.

### 4.7 Draw mode

`GMODE n` with n outside 0 to 2 raises `ERROR_OUT_OF_RANGE`, as `PVIEW`
and `DISP` do for a parameter outside their range.

## 5. Coordinates

### 5.1 Argument types, stage G2 and G3

A drawing command reads each coordinate register by type:

| Register type | Meaning | Path |
|---|---|---|
| long integer | pixel, also under a window | The fast path. The low 32 bits of the first limb are read directly from the register, with the sign tag. A magnitude above 32767 is an `ERROR_OUT_OF_RANGE`. No GMP call. |
| real | user coordinate through the window of its axis (G3). Without a window, a real is a pixel rounded half away from zero. A result beyond 32767 pixels, NaN, or infinity is an `ERROR_OUT_OF_RANGE`. | The slow path: the arithmetic of upstream's `screenWindowRatio`, in 39 digits. |
| complex | a point: the real part through the x window, the imaginary part through the y window. `ARC` takes its center this way in T. The two-point commands (`LINE`, `BOX`, `FBOX`, `GCLIP`) take two complex points, the first in Y and the second in X (G3). | The slow path, twice. |
| any other type | error `ERROR_INVALID_DATA_TYPE_FOR_OP` | |

Each coordinate is read by its own type, so a long integer and a real can
share one command. A complex in X or Y without a complex in the other is
`ERROR_INVALID_DATA_TYPE_FOR_OP`. A radius is always pixels: a real radius
is rounded, never mapped through the window.

### 5.2 The window, stage G3

    static struct {
      uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
      real34_t xmin, xmax, ymin, ymax;
    } pgWindow;

This struct sits next to `pgCanvas_t` in pgmGraphics.c from G3 on, and
not inside it, because the package header is read before `realType.h`. Without `XRNG` a real x
is a pixel, and without `YRNG` a real y is a pixel, each rounded half
away from zero. With a range set, the conversion of a real x is the one
of upstream's `screenWindowRatio` in plotstat.c:

    pixel = round_half_away((x - xmin) / (xmax - xmin) * (SCREEN_WIDTH - 1))

in 39-digit decimal arithmetic, and the same for y with `SCREEN_HEIGHT - 1`.
A result beyond 32767 pixels is an `ERROR_OUT_OF_RANGE`, so a line to a
far point is refused and never drawn with a clamped slope. `XRNG` and
`YRNG` take the minimum in Y and the maximum in X, as long integers or
reals. Equal ends raise `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and leave the window
unchanged. A reversed range mirrors the axis. The window survives `ERASE`
and `PVIEW`; only `XRNG`, `YRNG`, and a reset change it. The ends are
stored as real34 values, so two ends that differ only beyond 34 digits
are equal ends. Audit G3 round 1, Sol 1. The window maps
onto the full pixel grid, so in `PVIEW 6` the top 20 rows of the y range
lie under the status bar.

## 6. Items and menu

The package claims the spare rows 2448 to 2463 of `items.c` (all
`CAT_FREE`, far from every sibling claim) and the 3D rows from 2864 on
when G4 lands.

| Row | Item | Since |
|---|---|---|
| 2448 | `PVIEW` (`TM_VALUE`, `PTP_NUMBER_8`, min 2, max 6) | G1 |
| 2449 | `ERASE` | G1 |
| 2450 `LINE`, 2451 `BOX`, 2452 `FBOX`, 2453 `CIRCLE`, 2454 `FCIRCL`, 2455 `ARC`, 2456 `TEXTOUT`, 2457 `DISP`, 2458 `GMODE`, 2459 `GCLIP` | the 2D commands of §2.2 | G2 |
| 2460 `XRNG`, 2461 `YRNG` | §2.3 | G3 |
| 2864 `EYEPT`, 2865 `XVOL`, 2866 `YVOL`, 2867 `ZVOL`, 2868 `NUMX`, 2869 `NUMY`, 2871 `PT3D`, 2872 `LINE3D` | §2.4, `NOPARAM`, `CAT_FNCT`, `SLS_UNCHANGED`, `US_UNCHANGED`, `PTP_NONE`; upstream CONV spares | G4 |
| 2870 `WIREFRAME` | §2.4, `TM_LBLONLY`, `PTP_LABEL`, the row of section 9.4.1 | G4 |
| 2462 | `CANVAS`, the softmenu (`CAT_MENU`, `MNU_CANVAS`) | G1 |

The `CANVAS` menu array is defined after `menu_PFN_3` in `softmenus.c`.
Its registry row sits after row 180, four rows above the tail, because
pretty-print-extra owns the tail row and two insertions at one line do
not merge. The rows after 180 shift by one in a build with this package.
The menu hangs off the first free slot of the P.FN page 2 menu.

Prototypes of the commands and of the view hooks live in `screen.h`, next
to the upstream `PIXEL` family, because three siblings already patch the
include block of `c47.h`. The catalog stubs of the commands sit after the
`fnPlotStat` stub in the stub block of `items.c`.

Parameters of `PVIEW`, `DISP`, and `GMODE` use the step-parameter
mechanism of `PAUSE nn` (`TM_VALUE`, `PTP_NUMBER_8`). `WIREFRAME` uses
the label mechanism of `PGMSLV`.

## 7. Composition with the other packages

1. The `defines.h` line of section 3.1 sits in the package registry block.
   The value 21 is inside the range clause `calcMode < 19` that the other
   packages carry, so their softkey blocks cover this view without change.
2. The package carries the same range clause lines byte-identically,
   comments included, so that the solo build blocks softkeys too.
3. The package does not edit a row adjacent to another package's row in
   `items.c`, `softmenus.c`, or `testSuiteList.txt`.
4. `NUMBER_OF_SYSTEM_FLAGS` is not touched. The package adds no system
   flag.
5. The key resolution chain of `keyboard.c` has one free arm position,
   and pretty-print-extra owns it. Both packages carry one identical arm
   there: `else if(calcMode >= 20 && calcMode <= 23)` with the same
   comment bytes. Pretty-print-extra was amended to this range form on
   2026-09-04 under the registry mechanism. Forth-core resolves the same
   range in its own rewrite of the condition above the arm, so the three
   compose.
6. Every other keyboard insertion of the package sits at least three
   lines from every sibling hunk: the guard arm before the SNAP arm, the
   release arm after the `CM_NORMAL` R/S block, the EXIT case before
   `CM_TIMER`, and the no-op cases before the `default` arm of each key
   function.
7. The package also patches `calcMode.c` (the guard in `calcModeNormal`),
   `c47Extensions/addons.c` (`fnTo_ms`), and one line each in `fnKeyEnter`,
   `fnKeyCC`, `fnKeyDotD` for `pgEffectiveCalcMode()`, plus the guard at the
   top of `refreshRegisterLine` in `screen.c`. Each sits clear of the
   sibling hunks.

## 8. The speed law

1. Argument conversion uses one path per type (section 5.1). No decimal
   comparison on the fast path.
2. Every command is `US_UNCHANGED` and `SLS_UNCHANGED`.
3. Integer math only in the kernel. Horizontal runs use `pgRun`, one
   masked write per byte of the row.
4. Refresh cadence:

       pgRefreshMaybe():
         if programRunStop != PGM_RUNNING: pgRefreshNow(); return
         now = getUptimeMs()
         if now - canvas.lastRefreshMs >= 40: pgRefreshNow()

       pgRefreshNow():
         canvas.lastRefreshMs = getUptimeMs()
         DMCP_BUILD: lcd_refresh_dma()      // upstream precedent: screen.c:6016
         PC_BUILD:   lcd_refresh()

   Every drawing command ends with `pgRefreshMaybe()`. `PVIEW`, `ERASE`,
   and EXIT call `pgRefreshNow()`.
5. Stage G0 records a baseline on the simulator: `PIXEL` steps per second
   in a program of 1,000 steps. Each later stage records the same number
   for its commands. A command must not cost more than the interpreter's
   own fixed work per step, measured as the difference between a `PIXEL`
   loop and a `NOP` loop.

The DM42 is not tested by the package (section 11). The DMA refresh path
is implemented as upstream uses it and documented as untested.

## 9. 3D, stage G4

The stage was specified on 2026-09-05 from the research of that day; every
line marked DECISION is a choice made by the implementer for the owner to
rule on.

### 9.1 State

One static struct `pg3d` in `pgmGraphics.c`, next to `canvas`.

    typedef struct {
      float    eyeX, eyeY, eyeZ;   // offsets 0, 4, 8: EYEPT, x y z
      float    xlo, xhi;           // 12, 16: XVOL, low and high
      float    ylo, yhi;           // 20, 24: YVOL
      float    zlo, zhi;           // 28, 32: ZVOL
      float    curX, curY, curZ;   // 36, 40, 44: the current point of PT3D and LINE3D
      uint8_t  numX, numY;         // 48, 49: NUMX and NUMY, 2 to 100
      uint8_t  haveCur;            // 50: 1 when curX, curY, curZ hold a point
      uint8_t  angX, angY, angZ;   // 51, 52, 53: rotation step counts, 0 to 35, one step is 10 degrees
      int8_t   zoomStep;           // 54: -8 to 8, magnification 1.25 to the power zoomStep
      uint8_t  reserved;           // 55: zero
      uint8_t *block;              // 56: the retained block of section 9.2, NULL when none
    } pg3d_t;

`sizeof(pg3d_t)` is 64 on the PC build and 60 on the DM42. The pointer
sits last, so every other offset is the same on both builds.

The angles are integer step counts. DECISION: one press is 10 degrees, so
36 presses close one turn exactly. The zoom is an integer step count.
DECISION: one press multiplies the magnification by 1.25. Both counts are
integers so that a pin can demand exact return after a full turn
(section 9.8, P10). A float angle summed per press drifts.

#### 9.1.1 Defaults and the reset hook

`pgReset()` runs from `doFnReset`, so it also runs at boot
(`c47.c:636`). The anchor is the line after the assignment of
`histElementXorY` (`src/c47/config.c:1715`). The package adds a `config.c` mirror for this
one line, in the style of the undo-history hook at package line 1700:

    histElementXorY = -1;
    pgReset();   // program-graphics package: the pool is rebuilt, forget the 3D block without a free

    pgReset():
      pg3d.block = NULL              // never freed here, section 9.2.4
      pg3dResetCount += 1            // a static u16 of the file: WIREFRAME learns that the pool was rebuilt
      pg3d.eyeX = 0.0f; pg3d.eyeY = -3.0f; pg3d.eyeZ = 0.0f
      pg3d.xlo = pg3d.ylo = pg3d.zlo = -1.0f
      pg3d.xhi = pg3d.yhi = pg3d.zhi = 1.0f
      pg3d.numX = 10; pg3d.numY = 8
      pg3d.curX = pg3d.curY = pg3d.curZ = 0.0f; pg3d.haveCur = 0
      pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0
      pgWindow.set = 0               // the G3 window, DESIGN.md section 5.2

The defaults are the HP 48G `VPAR` defaults (48G UG p. 22-15, 50g AUR
p. D-10). The volume is -1 to 1 on every axis. The eye is (0, -3, 0). The
grid is 10 by 8.
DECISION: the package uses these defaults instead of zero. A zero volume
has no size and every projection divides by zero.

`pgReset` does not touch `canvas`. DESIGN.md section 10 item 7 stays: a
reset inside the view keeps the view open with a blank canvas.

Distance from the sibling hooks in `config.c`: undo-history inserts after
upstream line 1699, pretty-print after line 1730. The package inserts
after line 1715. Both distances are above the three-line rule of
section 7.

### 9.2 The retained block

#### 9.2.1 Constants

    #define PG3D_BLOCK_BYTES    2048
    #define PG3D_BLOCKS          512   // TO_BLOCKS(2048), 4 bytes per block (defines.h:2289-2291)
    #define PG3D_HEADER_BYTES     64
    #define PG3D_PAYLOAD_BYTES  1984   // 2048 - 64
    #define PG3D_LINE_BYTES        6
    #define PG3D_MAX_LINES       330   // 1984 / 6, with no grid
    #define PG3D_STEPS           254   // byte values 0 to 254 span a range
    #define PG3D_HOLE            255   // the byte value of a missing sample
    #define PG3D_NOPIX        -32768   // a projected point that is not drawable

#### 9.2.2 Layout

The block is 2048 bytes, 512 blocks of the memory pool. The pool returns
4-byte aligned memory, so the header is a plain C struct with natural
alignment. A compile-time assert pins `sizeof(pg3dHeader_t) == 64`.

    typedef struct {
      uint8_t  numX;        // 0: columns of the grid, 0 = no grid
      uint8_t  numY;        // 1: rows of the grid
      uint8_t  gridValid;   // 2: 1 when every grid byte was written by a complete run
      uint8_t  frozen;      // 3: 1 after the first record, section 9.2.5
      uint16_t lineCount;   // 4: line records stored
      uint16_t label;       // 6: the label code of the last WIREFRAME, 0 = none
      float    xlo, xhi;    // 8, 12
      float    ylo, yhi;    // 16, 20
      float    zlo, zhi;    // 24, 28
      float    zRecLo;      // 32: the z range the grid bytes span
      float    zRecHi;      // 36
      float    eyeX;        // 40
      float    eyeY;        // 44
      float    eyeZ;        // 48
      uint8_t  reserved[12];// 52 to 63: zero
    } pg3dHeader_t;

| Offset | Size | Content |
|---|---|---|
| 0 | 64 | The header above. |
| 64 | numX * numY | Grid bytes, row major. The byte of column i, row j is at `64 + j * numX + i`. |
| 2048 - 6 * (k + 1) | 6 | Line record k, for k from 0 to lineCount - 1. The six bytes are `bx0 by0 bz0 bx1 by1 bz1`. |

The grid grows up from the header. The lines grow down from the end of
the block. DECISION: this layout replaces the fixed `lineCap` of the
research. A later `WIREFRAME` with another grid size then never moves the
line records.

Free bytes at any time:

    pg3dFree(h) = PG3D_PAYLOAD_BYTES - h->numX * h->numY - PG3D_LINE_BYTES * h->lineCount

A grid of n = numX * numY bytes is retained when `n <= 1984 - 6 * lineCount`.
A line is retained when `pg3dFree(h) >= 6`.

Capacities: 44 by 44 (1936 bytes) with up to 8 lines. 45 by 45 needs
2025 bytes and is never retained. 32 by 32 with 160 lines. 24 by 24, the
showcase grid, with 234 lines. No grid with 330 lines. The plan figures
"45 by 45" and "about 340 lines" assumed no header (PLAN section 12).

#### 9.2.3 The byte encoding

    pg3dEncode(v, lo, hi):            // float v, lo < hi
      if v != v or v - v != 0.0f: return PG3D_HOLE     // NaN or infinite, no libm
      t = (v - lo) * (254.0f / (hi - lo))
      if t <= 0.0f: return 0
      if t >= 254.0f: return 254
      return (uint8_t)(t + 0.5f)

    pg3dDecode(b, lo, hi):            // b from 0 to 254
      return lo + (float)b * ((hi - lo) / 254.0f)

DECISION: byte 255 marks a hole. The values 0 to 254 span the range in
254 steps. The ruling said 256 steps. A NaN or infinite z needs one value
that draws nothing, and the ruling gave none. A finite value outside the
range clamps to 0 or 254.

The grid bytes span `zRecLo` to `zRecHi` of the header. A line byte spans
`xlo` to `xhi`, `ylo` to `yhi`, or `zlo` to `zhi` of the header. The grid x
and y are not stored. Column i has `x = xlo + i * (xhi - xlo) / (numX - 1)`
and row j has `y = ylo + j * (yhi - ylo) / (numY - 1)`, from the header.

Every drawing of 3D content goes through the bytes, retained or not. The
still picture of `WIREFRAME` and the redraw after a key press then use
the same decoded values (section 9.8, P10).

#### 9.2.4 Allocation, empty, free, and reset

Every 3D command starts with `pg3dEnsure()` and returns at once when it
gives false.

    pg3dEnsure():
      if canvas.region == 0: return true          // view closed: no block, no retention
      if pg3d.block != NULL: return true
      pg3d.block = allocC47Blocks(PG3D_BLOCKS)   // memory.c:76
      if pg3d.block == NULL:
        displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE)   // the two lines of checkedAllocate2, memory.h:65
        return false
      memset(pg3d.block, 0, PG3D_BLOCK_BYTES)
      return true

DECISION: the block is allocated by the first 3D command that runs while
the view is open. Outside the view the 3D commands draw once and retain
nothing. Reason: only `pgCloseView` frees the block, and `pgCloseView`
runs only from the view. A block allocated outside the view has no owner
until the next reset.

`ERROR_RAM_FULL` is code 11. `displayCalcErrorMessage` sets `lastErrorCode`
and the next refresh in mode 21 paints the text on canvas line 1
(section 3.6).

    pg3dEmpty():                    // content gone, allocation kept
      if pg3d.block == NULL: return
      memset(pg3d.block, 0, PG3D_HEADER_BYTES)
      pg3d.haveCur = 0

`pgSetRegion` calls `pg3dEmpty()` after it clears the rows. `ERASE` and
`PVIEW` both go through `pgSetRegion`. DECISION: `ERASE` and `PVIEW` empty
the retained content. A rotation after `ERASE` must not bring back a
picture the user erased.

    pgCloseView():                  // the existing body follows these lines
      freeC47Blocks(pg3d.block, PG3D_BLOCKS)   // ignores NULL, memory.c:117-119
      pg3d.block = NULL
      pg3d.haveCur = 0

`pgReset()` sets `pg3d.block = NULL` without a free. `doFnReset` zeroes
the pool and rebuilds the free list as one region (`config.c:1547,
1556-1559`). A free after that inserts a bogus region. The simulator's
`restoreCalc` calls `doFnReset` first (`saveRestoreBackup.c:831`), so no
second hook is needed. A state `LOAD` on the DM42 keeps the pool and the
block. Power off and on keeps both.

Budget: the block is 512 resident blocks while the view is open. With the
undo-history ring (1024 blocks) the total is 1536 blocks, above the 1400
block slack of the RCL58 law (undo-history DESIGN.md section 91-101). A
program that runs a 14 by 14 eigenvalue inside the view can hit
`ERROR_RAM_FULL`. Documented limit (section 9.9).

#### 9.2.5 The frozen view

The header holds the volume and the eye of the content it stores. They
freeze at the first record after the block was empty. Later `XVOL`,
`YVOL`, `ZVOL`, and `EYEPT` change `pg3d` only. They take effect after
`ERASE`, `PVIEW`, or EXIT. DECISION: this rule keeps the still picture and
every redraw consistent. The window (`XRNG`, `YRNG`) is not frozen. The
redraw reads the live window. The angles and the zoom are not part of the
frozen view: `ERASE`, `PVIEW` and EXIT reset them to the home view
(ruling 2026-09-05).

    typedef struct {
      float eyeX, eyeY, eyeZ;
      float xlo, xhi, ylo, yhi, zlo, zhi;
      float zRecLo, zRecHi;
    } pg3dView_t;

    pg3dViewValid(v):
      return v->xlo < v->xhi and v->ylo < v->yhi and v->zlo < v->zhi and v->eyeY < v->ylo

    pg3dRecordView(out):            // called by WIREFRAME and by LINE3D before a record
      h = (pg3d.block != NULL) ? header : NULL
      if h != NULL and h->frozen:
        copy h->eye*, h->xlo..zhi, h->zRecLo, h->zRecHi into out
        return true
      copy pg3d.eye*, pg3d.xlo..zhi into out; out->zRecLo = pg3d.zlo; out->zRecHi = pg3d.zhi
      if not pg3dViewValid(out):
        displayCalcErrorMessage(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN, ERR_REGISTER_LINE, REGISTER_X)
        return false
      if h != NULL:
        copy out into h->eye*, h->xlo..zhi, h->zRecLo, h->zRecHi
        h->frozen = 1
      return true

The eye rule `eyeY < ylo` is the HP rule: the eye is in front of the near
face (48G UG p. 23-25, AUR: one unit before ynear). The error code is
`ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` (1), the code G3 uses for a bad range.

The rule "a grid above the cap draws once and does not rotate" is this.
The cap is `pg3dFree(h) + h->numX * h->numY`, so the old grid does not
count. `WIREFRAME` with `numX * numY` above the cap writes no grid bytes.
It sets `h->numX = h->numY = 0`
and `h->gridValid = 0`. It draws the mesh from the encoded bytes of each
sample as it goes. The next key press clears the canvas and redraws the
retained lines only. The same rule holds for a line when
`pg3dFree(h) < 6`: the line draws once and `lineCount` does not change.

### 9.3 Projection

#### 9.3.1 Axes and the plane

x is width (screen horizontal). y is depth (into the screen, positive is
farther). z is height (screen vertical). The eye is at
(eyeX, eyeYz, eyeZ), where `eyeYz` is the eye y after the zoom
(section 9.3.4). The projection plane is `y = eyeYz + 1`. It moves with the
eye (48G UG p. 23-25). A point P projects along the ray from the eye
through P onto the plane. The plane coordinates are u along x and v along
z, in volume units:

    u = eyeX + (px - eyeX) / (py - eyeYz)
    v = eyeZ + (pz - eyeZ) / (py - eyeYz)

A face at depth py shrinks by the factor `1 / (py - eyeYz)`. When the eye
is one unit before the near face, the near face projects at scale 1.

DESIGN.md section 9.2 of version 0 said "the plane y = yhi". That was a
placeholder and this section replaces it.

#### 9.3.2 The rotation

The rotation turns the content about the center of the frozen volume:

    cx = (xlo + xhi) * 0.5f; cy = (ylo + yhi) * 0.5f; cz = (zlo + zhi) * 0.5f
    p' = M * (p - c) + c

The three matrices, with s = sin(a) and c = cos(a), right handed:

    Rx(a) = | 1  0  0 |    Ry(a) = |  c  0  s |    Rz(a) = | c -s  0 |
            | 0  c -s |            |  0  1  0 |            | s  c  0 |
            | 0  s  c |            | -s  0  c |            | 0  0  1 |

    M = Rz(angZ * 10 degrees) * Ry(angY * 10 degrees) * Rx(angX * 10 degrees)

DECISION: the three angles compose in this fixed order. The research
proposed a trackball (`M = Rk(step) * M` per press). A trackball needs a
float matrix that drifts and cannot return exactly after a full turn. The
fixed order is deterministic and matches the ruling table, which names one
axis per key.

The sines come from a table of 36 floats. No libm call is needed for the
keys, so the flash cost of float trigonometry (PLAN section 5) is zero.

    static const float pg3dSin[36] = {
      0.0f, 0.173648178f, 0.342020143f, 0.5f, 0.64278761f, 0.766044443f,
      0.866025404f, 0.939692621f, 0.984807753f, 1.0f, 0.984807753f, 0.939692621f,
      0.866025404f, 0.766044443f, 0.64278761f, 0.5f, 0.342020143f, 0.173648178f,
      0.0f, -0.173648178f, -0.342020143f, -0.5f, -0.64278761f, -0.766044443f,
      -0.866025404f, -0.939692621f, -0.984807753f, -1.0f, -0.984807753f, -0.939692621f,
      -0.866025404f, -0.766044443f, -0.64278761f, -0.5f, -0.342020143f, -0.173648178f
    };
    #define PG3D_SIN(k) pg3dSin[(k) % 36]
    #define PG3D_COS(k) pg3dSin[((k) + 9) % 36]

    pg3dMatrix(M, angX, angY, angZ):
      A = Rx(angX); B = Ry(angY); C = Rz(angZ)     // each from PG3D_SIN and PG3D_COS
      T = B * A                                    // 3 by 3 product, row times column
      M = C * T

#### 9.3.3 The zoom

    static const float pg3dZoom[17] = {   // 1.25 to the power k, k from -8 to 8
      0.16777216f, 0.2097152f, 0.262144f, 0.32768f, 0.4096f, 0.512f, 0.64f, 0.8f,
      1.0f, 1.25f, 1.5625f, 1.953125f, 2.44140625f, 3.0517578125f,
      3.814697265625f, 4.76837158203125f, 5.9604644775390625f
    };
    zoom  = pg3dZoom[pg3d.zoomStep + 8]
    eyeYz = ylo - (ylo - eyeY) / zoom

DECISION: the zoom moves the eye along y so that the near face
`y = ylo` is magnified by exactly `zoom`. The distance from the eye to the
near face is `D0 / zoom` with `D0 = ylo - eyeY`, positive by the eye rule.
The eye never enters the volume. Farther faces grow less than the near
face, so the zoom is true perspective and not a 2D scale. The research
offered the volume center as the reference instead. That form puts the
eye inside the volume at zoom 3 for the HP defaults.

#### 9.3.4 The plane to pixels

The plane coordinates go through the G3 window. `XRNG` maps u and `YRNG`
maps v. Without `XRNG` a u is a pixel column. Without `YRNG` a v is a user
row. Per redraw the window is read once into floats:

    pg3dWindow(w):
      if pgWindow.set & 1:
        w->xmin = float(pgWindow.xmin); w->xs = 399.0f / (float(pgWindow.xmax) - w->xmin)
      else: w->xmin = 0.0f; w->xs = 1.0f
      if pgWindow.set & 2:
        w->ymin = float(pgWindow.ymin); w->ys = 239.0f / (float(pgWindow.ymax) - w->ymin)
      else: w->ymin = 0.0f; w->ys = 1.0f

`float(r34)` is `real34ToReal` then `realToFloat`
(`registerValueConversions.h:98`). A reversed range gives a negative scale
and mirrors the axis, as in G3.

#### 9.3.5 One point

    typedef struct {
      pg3dView_t v;          // the frozen view
      float      M[9];       // row major
      float      cx, cy, cz;
      float      eyeYz;
      float      eps;        // (yhi - ylo) / 1024
      float      wxmin, wxs, wymin, wys;
    } pg3dSetup_t;

    pg3dSetup(s, view):
      s->v = *view
      pg3dMatrix(s->M, pg3d.angX, pg3d.angY, pg3d.angZ)
      s->cx, s->cy, s->cz as in 9.3.2
      s->eyeYz = view->ylo - (view->ylo - view->eyeY) / pg3dZoom[pg3d.zoomStep + 8]
      s->eps = (view->yhi - view->ylo) * (1.0f / 1024.0f)
      pg3dWindow(&s->w)

    pg3dRound(f):                    // round half up, clamped, no libm
      if not (f > -32000.0f): f = -32000.0f     // NaN lands here too
      if f > 32000.0f: f = 32000.0f
      return (int32_t)(f + 32768.5f) - 32768

    pg3dProject(s, x, y, z, &col, &row):        // returns false when the point is not drawable
      px = x - s->cx; py = y - s->cy; pz = z - s->cz
      rx = s->M[0]*px + s->M[1]*py + s->M[2]*pz + s->cx
      ry = s->M[3]*px + s->M[4]*py + s->M[5]*pz + s->cy
      rz = s->M[6]*px + s->M[7]*py + s->M[8]*pz + s->cz
      dy = ry - s->eyeYz
      if not (dy >= s->eps): return false        // nearer than eps, or NaN; exactly eps is drawable (audit G4 round 1)
      inv = 1.0f / dy
      u = s->v.eyeX + (rx - s->v.eyeX) * inv
      v = s->v.eyeZ + (rz - s->v.eyeZ) * inv
      col = pg3dRound((u - s->wxmin) * s->wxs)
      row = 239 - pg3dRound((v - s->wymin) * s->wys)
      clamp row to -32000 .. 32000                 // the flip can carry a clamped value past the limit (audit G4 round 1)
      return true

One division, twelve multiplications, and one branch per point. DECISION:
`eps` is one 1024th of the volume depth. A point nearer to the eye than
that is dropped, and every line that touches it. The clamp at 32000 keeps
the int32 to int16 conversions of the row buffer safe. The kernel clips
the rest (section 4.2).

Rounding is half up, not half away from zero as in G3. The two paths
differ only at an exact half, which the pins avoid.

#### 9.3.6 The worked example

The unit cube, corners (0, 0, 0) to (1, 1, 1). `XVOL 0 1`, `YVOL 0 1`,
`ZVOL 0 1`, `EYEPT 0.5 -1 0.5`, `XRNG 0 1`, `YRNG 0 1`, angles 0, zoom 1.
The plane is y = 0, the near face. xs = 399, ys = 239.

| Corner | u | v | col | yUser | row |
|---|---|---|---|---|---|
| (0, 0, 0) | 0.00 | 0.00 | 0 | 0 | 239 |
| (1, 0, 0) | 1.00 | 0.00 | 399 | 0 | 239 |
| (0, 0, 1) | 0.00 | 1.00 | 0 | 239 | 0 |
| (1, 0, 1) | 1.00 | 1.00 | 399 | 239 | 0 |
| (0, 1, 0) | 0.25 | 0.25 | 100 | 60 | 179 |
| (1, 1, 0) | 0.75 | 0.25 | 299 | 60 | 179 |
| (0, 1, 1) | 0.25 | 0.75 | 100 | 179 | 60 |
| (1, 1, 1) | 0.75 | 0.75 | 299 | 179 | 60 |

The near face fills the screen. Its top edge, row 0, lies under the status
bar and is clipped in the view. The far face is the rectangle columns 100
to 299, rows 60 to 179. The eight pixels are pin P1. The twelve edges
through `pgLine` light 1806 pixels in rows 20 to 239 (computed with the
stepper of section 4.3).

A rotation about y keeps every depth, so a face of constant y turns as a
flat picture. A rotation about x or z moves points nearer and farther, so
parallel edges converge. A rotation of 45 degrees about x brings a corner
of the cube to y = -0.207, still in front of the eye at -1.

### 9.4 WIREFRAME

#### 9.4.1 The item

    /* 2870 */  { fnWireframe, TM_LBLONLY, "WIREFRAME", "WIREFRAME", (0 << TAM_MAX_BITS) | 99, CAT_FNCT | SLS_UNCHANGED | US_UNCHANGED | EIM_DISABLED | PTP_LABEL | HG_ENABLED },  // program-graphics package

The row copies `PGMSLV` (`items.c:3399`) except the two status fields.
`TM_LBLONLY` opens the label TAM. `PTP_LABEL` stores the label in program
memory as opcode, `STRING_LABEL_VARIABLE`, length, name
(`manage.c:1555-1558`). At run time `_executeOp` resolves the name to a
code from `FIRST_LABEL` to `LAST_LABEL` and raises `ERROR_LABEL_NOT_FOUND`
before the item function runs (`lblGtoXeq.c:369-395`).

DECISION: the row stays `US_UNCHANGED` and `SLS_UNCHANGED` under the speed
law. The command restores the stack itself with the `PLTf` pair
`saveForUndo` and `fnUndo(0)` (`solve.c:233, 248`). The research asked for
a ruling on this conflict. With `US_ENABLED` the runner saves the same
image and restores it only on error. The leftovers of the body then stay
on the stack after a good run.

#### 9.4.2 The command

    fnWireframe(label):
      // 1. the label, the shape of _checkArgument (sumprod.c:308-336)
      if REGISTER_X <= label and label <= REGISTER_T:
        buf[0] = letteredRegisterName(label); buf[1] = 0
        label = findNamedLabel(buf, GLOBAL_LABELS)
        if label == INVALID_VARIABLE:
          displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X); return
      else if not (FIRST_LABEL <= label and label <= LAST_LABEL):
        displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X); return
      // 2. the nesting guard, ahead of saveForUndo (solve.c:36-42)
      if engineNestingRefused(true): return
      // 3. the block and the view
      if not pg3dEnsure(): return
      if not pg3dRecordView(&view): return
      // 4. the grid plan
      numX = pg3d.numX; numY = pg3d.numY
      h = (pg3d.block != NULL) ? header : NULL
      if h != NULL:
        h->gridValid = 0; h->numX = 0; h->numY = 0            // the old grid is gone in any case
        retain = (numX * numY <= pg3dFree(h))
        if retain: h->numX = numX; h->numY = numY              // reserved now, so a LINE3D in the body sees it
      else: retain = false
      rows = allocC47Blocks(TO_BLOCKS(2 * numX * 4))
      if rows == NULL:
        displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE); return
      // 5. the engine protocol
      currentKeyCode = 255                                     // sumprod.c:76
      saveForUndo()                                            // solve.c:233
      ++engineNestingDepth; ++plotEngineActive                 // graph.c:2935-2936
      ++currentSolverNestingDepth; setSystemFlag(FLAG_SOLVING) // sumprod.c:122-123
      savedProgram = currentProgramNumber; savedLocal = currentLocalStepNumber
      savedStep = currentStep; resets = pg3dResetCount
      pg3dSetup(&s, &view)
      pgClipNow(&clip)
      result = pg3dRunGrid(&s, h, rows, numX, numY, retain, label, true, &clip)
      // 6. the exit protocol, every path
      if pg3dResetCount == resets: freeC47Blocks(rows, TO_BLOCKS(2 * numX * 4))   // a reset in the body rebuilt the pool: then forget rows
      currentProgramNumber = savedProgram; currentLocalStepNumber = savedLocal; currentStep = savedStep
      if (--currentSolverNestingDepth) == 0: clearSystemFlag(FLAG_SOLVING)
      --plotEngineActive; --engineNestingDepth
      temporaryInformation = TI_NO_INFO
      fnUndo(0)                                                // solve.c:248, after the flags are clear
      h = (pg3d.block != NULL) ? header : NULL
      if result == PG3D_RUN_ALLHOLES:
        displayCalcErrorMessage(lastPointError, ERR_REGISTER_LINE, REGISTER_X)
      else if result == PG3D_RUN_OK and retain and h != NULL and h->frozen and h->numX == numX and h->numY == numY:
        h->gridValid = 1; h->label = label       // a body that emptied the block leaves no valid grid (audit G4 round 1)
      pgRefreshNow()

`engineNestingRefused(true)` refuses `WIREFRAME` inside any engine, as it
refuses `PLOT`: the screen and the block are single. On refusal it sets
`PGM_WAITING` and `engineNestingWasRefused`, and the outer engine names
the error `ERROR_NESTING_TOO_DEEP` (`graph.c:1214`). `WIREFRAME` returns
without its own error, as `fnMvarPlot` does (`solve.c:227-229`). DECISION:
the plot form of the guard, not the solver form.

The program pointer restore covers the keyboard arm of `fnExecute`.
There `runProgram` leaves the pointer at the END of the label's program and
`execProgram` skips its own restore (`lblGtoXeq.c:1038`). No upstream
caller restores it. DECISION: the package restores the three pointers on
both arms. On the program arm the restore repeats what `execProgram` did.

`programRunStop` is left as the run left it. An abort sets `PGM_WAITING`
to halt the outer program, as the solver does (`solve.c:784-788`). A key
press converts `PGM_WAITING` to `PGM_STOPPED` (`keyboard.c:1311-1313`).

#### 9.4.3 The grid loop

    PG3D_RUN_OK 0, PG3D_RUN_ABORTED 1, PG3D_RUN_ALLHOLES 2

    pg3dRunGrid(s, h, rows, numX, numY, retain, label, draw, clip):
      holes = 0; lastPointError = ERROR_NONE
      for j in 0 .. numY - 1:
        y = s->v.ylo + (float)j * ((s->v.yhi - s->v.ylo) / (float)(numY - 1))
        for i in 0 .. numX - 1:
          x = s->v.xlo + (float)i * ((s->v.xhi - s->v.xlo) / (float)(numX - 1))
          // the abort test, differentiate.c:399-406 and graph.c:1213-1217
          if lastErrorCode == ERROR_SOLVER_ABORT or programRunStop == PGM_WAITING or exitKeyWaiting():
            lastErrorCode = engineNestingWasRefused ? ERROR_NESTING_TOO_DEEP : ERROR_SOLVER_ABORT
            if programRunStop == PGM_RUNNING: programRunStop = PGM_WAITING
            return PG3D_RUN_ABORTED
          z = pg3dSample(label, x, y, &err)
          if err != ERROR_NONE: holes += 1; lastPointError = err
          if pg3d.block == NULL or (h != NULL and h->frozen == 0): retain = false   // the body emptied or reset the block
          b = pg3dEncode(z, s->v.zRecLo, s->v.zRecHi)
          if retain: h->grid[j * numX + i] = b
          if draw:
            zq = (b == PG3D_HOLE) ? NaN : pg3dDecode(b, s->v.zRecLo, s->v.zRecHi)
            pg3dMeshPoint(s, rows, numX, i, j, x, y, zq, clip)
        if draw: pgRefreshMaybe()
      if lastErrorCode == ERROR_SOLVER_ABORT: return PG3D_RUN_ABORTED
      if holes == numX * numY: return PG3D_RUN_ALLHOLES
      return PG3D_RUN_OK

`lastPointError` is a static of the file, read by `fnWireframe` after the
loop. DECISION: when every sample failed, `WIREFRAME` raises the error of
the last sample. A body that fails everywhere otherwise draws nothing and
says nothing. A body that fails at some points leaves holes there.

An aborted run keeps the partial drawing on the canvas and retains no
grid: `gridValid` stays 0. The reserved `numX`, `numY` in the header stay
reserved until the next `WIREFRAME` or `ERASE`.

#### 9.4.4 One sample

    pg3dSample(label, x, y, &err):                 // returns z as float, NaN with err set on failure
      convertDoubleToReal34Register((double)y, REGISTER_T)   // registerValueConversions.h:95
      convertDoubleToReal34Register((double)x, REGISTER_Z)
      convertDoubleToReal34Register((double)y, REGISTER_Y)
      convertDoubleToReal34Register((double)x, REGISTER_X)
      dynamicMenuItem = -1                                     // sumprod.c:159, lblGtoXeq.c:16-19
      pg3dRunCount += 1                                        // a static u32 of the file, read by pin P11
      execProgram(label)                                       // lblGtoXeq.c:1032
      if lastErrorCode != ERROR_NONE:
        err = lastErrorCode
        if lastErrorCode != ERROR_SOLVER_ABORT: lastErrorCode = ERROR_NONE   // differentiate.c:324-336, the one error that stays
        return NaN
      fnToReal(NOPARAM)                                        // registers.c:2130, a program may leave any type in X
      if lastErrorCode != ERROR_NONE:
        err = lastErrorCode; lastErrorCode = ERROR_NONE; return NaN
      real34ToReal(REGISTER_REAL34_DATA(REGISTER_X), &r); realToFloat(&r, &z)
      err = ERROR_NONE
      return z

DECISION: the stack at entry of the body is X = x, Y = y, Z = x, T = y. No
upstream caller loads two arguments. With this shape the saddle
`z = x*x - y*y` is the four steps `x², x⇄y, x², -`. Registers A to D of an
8-level stack are not written.

`FLAG_SOLVING` is what lets `execProgram` run the body inside a program
(`lblGtoXeq.c:1038`). It also stops the per-item undo snapshot
(`items.c:294-297`) and the screen writes of `runProgram`
(`lblGtoXeq.c:910-914, 1015-1025`). The canvas keeps its pixels through
the mode 21 refresh arm.

#### 9.4.5 The mesh from the rows

Two rows of `numX` entries, from the transient pool, freed at the end.

    typedef struct { int16_t col, row; } pg3dPix_t;   // col == PG3D_NOPIX: not drawable

    pg3dMeshPoint(s, rows, numX, i, j, x, y, z, clip):
      cur  = rows + (j & 1) * numX
      prev = rows + ((j + 1) & 1) * numX
      ok = (z == z) and pg3dProject(s, x, y, z, &col, &row)
      cur[i].col = ok ? col : PG3D_NOPIX; cur[i].row = ok ? row : 0
      if not ok: return
      if i > 0 and cur[i - 1].col != PG3D_NOPIX: pgLine(clip, cur[i - 1].col, cur[i - 1].row, col, row)
      if j > 0 and prev[i].col != PG3D_NOPIX:    pgLine(clip, prev[i].col, prev[i].row, col, row)

The lines use `pgLine` of section 4.3 with the draw mode in force. No
hidden-line removal. The still picture uses `canvas.drawMode`. The redraw
after a key uses mode 0 (section 9.6.3).

#### 9.4.6 Refresh cadence

`pgRefreshMaybe()` after each grid row. `pgRefreshNow()` at the end of the
command. On the DM42 the user sees the mesh grow row by row, at most
every 40 ms.

#### 9.4.7 Speed

The cost per point is one `execProgram` plus the projection. TESTING.md
section 5 asks for two numbers per stage. The stage records the time of
`WIREFRAME` with the four-step saddle at 24 by 24 on the simulator. It
also records the time of 1,000 `LINE3D` steps.

### 9.5 The setting commands, PT3D, and LINE3D

#### 9.5.1 Reading a float

    pg3dReadFloat(regist, &f):            // returns false after an error
      switch getRegisterDataType(regist):
        dtLongInteger: convertLongIntegerRegisterToReal(regist, &r, &ctxtReal39); break
        dtReal34:
          if real34IsNaN(data) or real34IsInfinite(data): pgError(ERROR_OUT_OF_RANGE); return false
          real34ToReal(data, &r); break
        default: pgError(ERROR_INVALID_DATA_TYPE_FOR_OP); return false
      realToFloat(&r, &f)
      if f - f != 0.0f: pgError(ERROR_OUT_OF_RANGE); return false     // overflowed to infinity
      return true

One path per type, no decimal compare (section 8, law 1).

    pg3dReadPoint(&x, &y, &z):            // x in Z, y in Y, z in X, the EYEPT order
      return pg3dReadFloat(REGISTER_Z, &x) and pg3dReadFloat(REGISTER_Y, &y) and pg3dReadFloat(REGISTER_X, &z)

    pg3dReadCount(&n):                    // NUMX, NUMY: an integer from 2 to 100 in X
      switch getRegisterDataType(REGISTER_X):
        dtLongInteger: convertLongIntegerRegisterToReal(REGISTER_X, &r, &ctxtReal39); break
        dtReal34: real34ToReal(data, &r); break
        default: pgError(ERROR_INVALID_DATA_TYPE_FOR_OP); return false
      v = realToInt32C47(&r, &err)
      if err or v < 2 or v > 100 or not realIsAnInteger(&r): pgError(ERROR_OUT_OF_RANGE); return false
      n = (uint8_t)v
      return true

#### 9.5.2 EYEPT, XVOL, YVOL, ZVOL, NUMX, NUMY

Every one of these starts with `if not pg3dEnsure(): return`. None of them
draws. None of them changes the stack.

    fnEyept():   if not pg3dReadPoint(&x, &y, &z): return;  pg3d.eyeX = x; pg3d.eyeY = y; pg3d.eyeZ = z
    fnXvol():    pg3dRange(&pg3d.xlo, &pg3d.xhi)
    fnYvol():    pg3dRange(&pg3d.ylo, &pg3d.yhi)
    fnZvol():    pg3dRange(&pg3d.zlo, &pg3d.zhi)
    fnNumx():    if pg3dReadCount(&n): pg3d.numX = n
    fnNumy():    if pg3dReadCount(&n): pg3d.numY = n

    pg3dRange(lo, hi):                    // low in Y, high in X
      if not pg3dReadFloat(REGISTER_Y, &a) or not pg3dReadFloat(REGISTER_X, &b): return
      if not (a < b) or (b - a) is not finite: pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return   // HP: ynear must be less than yfar; a span beyond float is refused (audit G4 round 1)
      *lo = a; *hi = b

A reversed or empty volume range is refused. A reversed window range is
allowed (G3). The two rules differ because a volume byte spans low to
high and a mirrored volume has no meaning.

#### 9.5.3 The current point

`PT3D` stores the point as given. `LINE3D` with no current point acts as
`PT3D`. DECISION: no error for a missing current point. A program then
starts a polyline with `LINE3D` alone.

    fnPt3d():
      if not pg3dEnsure(): return
      if not pg3dReadPoint(&x, &y, &z): return
      pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z; pg3d.haveCur = 1

#### 9.5.4 LINE3D

    fnLine3d():
      if not pg3dEnsure(): return
      if not pg3dReadPoint(&x, &y, &z): return
      if not pg3d.haveCur:
        pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z; pg3d.haveCur = 1; return
      if not pg3dRecordView(&view): return           // the current point stays
      x0 = clamp(pg3d.curX, view.xlo, view.xhi); y0 = clamp(pg3d.curY, view.ylo, view.yhi); z0 = clamp(pg3d.curZ, view.zlo, view.zhi)
      x1 = clamp(x, ...); y1 = clamp(y, ...); z1 = clamp(z, ...)
      rec[0] = pg3dEncode(x0, view.xlo, view.xhi); rec[1] = pg3dEncode(y0, view.ylo, view.yhi); rec[2] = pg3dEncode(z0, view.zlo, view.zhi)
      rec[3] = pg3dEncode(x1, ...); rec[4] = pg3dEncode(y1, ...); rec[5] = pg3dEncode(z1, ...)
      h = (pg3d.block != NULL) ? header : NULL
      if h != NULL and pg3dFree(h) >= PG3D_LINE_BYTES:
        memcpy(pg3d.block + PG3D_BLOCK_BYTES - PG3D_LINE_BYTES * (h->lineCount + 1), rec, 6)
        h->lineCount += 1
      pg3dSetup(&s, &view); pgClipNow(&clip)
      pg3dDrawRecord(&s, rec, &clip)
      pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z
      pgRefreshMaybe()

    clamp(v, lo, hi): return v < lo ? lo : (v > hi ? hi : v)

    pg3dDrawRecord(s, rec, clip):
      x0 = pg3dDecode(rec[0], s->v.xlo, s->v.xhi); y0 = pg3dDecode(rec[1], s->v.ylo, s->v.yhi); z0 = pg3dDecode(rec[2], s->v.zlo, s->v.zhi)
      x1, y1, z1 likewise from rec[3], rec[4], rec[5]
      if pg3dProject(s, x0, y0, z0, &c0, &r0) and pg3dProject(s, x1, y1, z1, &c1, &r1):
        pgLine(clip, c0, r0, c1, r1)

DECISION: a coordinate outside the volume clamps to the volume face. The
research left this open. The alternative, `ERROR_OUT_OF_RANGE`, breaks a
program loop at its first point outside. The 2D commands never raise an
error for a point off the screen (section 4.2), and this rule follows
them. The clamp changes the direction of a line that leaves the volume.
Documented limit. The current point keeps the unclamped values.

The line is drawn from the encoded bytes, retained or not, so the still
picture and the redraw agree. A line with an endpoint at or behind the
eye draws nothing.

### 9.6 The keys

#### 9.6.1 The table

The owner ruled the key map (PLAN section 12). The step per press is a
DECISION of section 9.1: 10 degrees, and a zoom factor of 1.25.

| Key in the view | Item that reaches the package | Arrives through | Effect |
|---|---|---|---|
| UP | `ITM_UP1` | the `CM_GRAPHICS_CANVAS` case of `fnKeyUp` | `angX = (angX + 1) % 36` |
| DOWN | `ITM_DOWN1` | the `CM_GRAPHICS_CANVAS` case of `fnKeyDown` | `angX = (angX + 35) % 36` |
| f-UP | `ITM_BST` | the guard arm of `processKeyAction` | `angY = (angY + 1) % 36` |
| f-DOWN | `ITM_SST` | the guard arm | `angY = (angY + 35) % 36` |
| g-UP | `ITM_RBR` | the guard arm | `angZ = (angZ + 1) % 36` |
| g-DOWN | `ITM_FLGSV` | the guard arm | `angZ = (angZ + 35) % 36` |
| plus | `ITM_ADD` | the guard arm | `zoomStep += 1`, at most 8 |
| minus | `ITM_SUB` | the guard arm | `zoomStep -= 1`, at least -8 |
| 5 | `ITM_5` | the guard arm | `angX = angY = angZ = 0; zoomStep = 0` |
| 4, 6 | `ITM_4`, `ITM_6` | the guard arm | none, reserved |

DECISION: the zoom range is -8 to 8 steps, magnification 0.168 to 5.96.
A press beyond either end changes nothing.

Item ids: `ITM_UP1` 1733, `ITM_DOWN1` 1735, `ITM_BST` 1734, `ITM_SST`
1736, `ITM_RBR` 1560, `ITM_FLGSV` 1935, `ITM_ADD` 95, `ITM_SUB` 96,
`ITM_4` 544, `ITM_5` 545, `ITM_6` 546 (`src/c47/items.h`).

    pg3dKey(item):
      if pg3d.block == NULL: return
      h = header
      if h->gridValid == 0 and h->lineCount == 0: return       // nothing retained: no effect
      switch item:
        ITM_UP1:   pg3d.angX = (pg3d.angX + 1) % 36; break
        ITM_DOWN1: pg3d.angX = (pg3d.angX + 35) % 36; break
        ITM_BST:   pg3d.angY = (pg3d.angY + 1) % 36; break
        ITM_SST:   pg3d.angY = (pg3d.angY + 35) % 36; break
        ITM_RBR:   pg3d.angZ = (pg3d.angZ + 1) % 36; break
        ITM_FLGSV: pg3d.angZ = (pg3d.angZ + 35) % 36; break
        ITM_ADD:   if pg3d.zoomStep >= 8: return;  pg3d.zoomStep += 1; pg3dZoomRerun(); break
        ITM_SUB:   if pg3d.zoomStep <= -8: return; pg3d.zoomStep -= 1; pg3dZoomRerun(); break
        ITM_5:     if angX == 0 and angY == 0 and angZ == 0 and zoomStep == 0: return
                   pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0; pg3dZoomRerun(); break
        default:   return
      pg3dRedraw()

DECISION: a key press with no retained content has no effect. The angles
do not change either. The K2 pin of G1, which presses UP and DOWN in an
empty view, stays green. Ruling 2026-09-05 (G34R1-9): this holds for a
mesh above the cap too. The mesh stays on the screen and does not turn;
a key press leaves the canvas byte for byte (pin P6).

Ruling 2026-09-05 (G34R1-4): `ERASE`, `PVIEW` and EXIT return the three
angles and the zoom to the home view, as they drop the frozen eye. The
next drawing starts straight on (pins T1 to T3).

On the PC build a key reaches the package only while no program runs
(`keyboard.c:1887-1907`). On the DM42 a held UP or DOWN repeats
(`c47.c:1010-1014`). The key numbers of that repeat were not checked
against the R47 layout. Untested by the package.

#### 9.6.2 The code arms and their anchors

All line numbers are of `packages/program-graphics/keyboard.c` (PK) and
of upstream `src/c47/keyboard.c` (UP).

(a) `fnKeyUp`, the existing case (PK 4733, anchor between UP 4683 and
4689):

        case CM_GRAPHICS_CANVAS: {   // program-graphics package: stage G4, rotation about x
          pg3dKey(ITM_UP1);
          break;
        }

`fnKeyDown` (PK 4956, anchor UP 4902-4908) is the same with `ITM_DOWN1`.
`processKeyAction` calls `fnKeyUp` at the press (PK 2488-2489). The
release runs no item, because `showFunctionNameItem` is 0 (PK 2164). Pin
P7 drives the real key path once and demands one step.

(b) The guard arm of `processKeyAction`, replacing PK 2787-2792 (anchor:
before `else if(item == ITM_SNAP)`, UP 2766):

        else if(calcMode == CM_GRAPHICS_CANVAS && item != ITM_SNAP) {
          if(item == ITM_RS) {
            showFunctionNameItem = 0;
          }
          else if(item == ITM_BST || item == ITM_SST || item == ITM_RBR || item == ITM_FLGSV
               || item == ITM_ADD || item == ITM_SUB || item == ITM_4 || item == ITM_5 || item == ITM_6) {
            pg3dKey(item);   // program-graphics package, stage G4: rotation, zoom, home view
          }
          keyActionProcessed = true;
        }

These lines exist only in this package. The arm keeps swallowing
`ITM_OFF` (f-EXIT) and `ITM_PR` (f-R/S).

(c) The shift gate line, PK 1604 (UP 1604), replaced byte for byte by
`packages/undo-history/keyboard.c:1604`:

        if((calcMode == CM_NORMAL || calcMode == CM_AIM || calcMode == CM_NIM  || calcMode == CM_MIM || calcMode == CM_EIM || calcMode == CM_PEM || calcMode == CM_PLOT_STAT || calcMode == CM_GRAPH || calcMode == CM_ASSIGN || calcMode == CM_ASN_BROWSER || calcMode == CM_REGISTER_BROWSER || calcMode == CM_FLAG_BROWSER || calcMode == CM_FONT_BROWSER || (calcMode >= 19 && calcMode <= 23) /* package browsers */ || calcMode == CM_TIMER)) {

With this line f and g engage in mode 21. `commonShiftProcessing` toggles
`shiftF` (PK 1503-1505) and the next key resolves through
`key->fShifted` in the package arm (PK 1694-1696). On the `KEY_fg`
layouts g needs two presses (`keyboardTweak.c:297-304`).

(d) `defines.h`, two macro lines (upstream 1583 and 1585), the shift
glyph rule of section 9.6.4.

(e) `screen.h`, after `pgEffectiveCalcMode`:

        void       pg3dKey                          (int16_t item);
        void       pgReset                          (void);

and the nine item functions after `fnGclip`, in the same style.

(f) `pgCloseView`, the free of section 9.2.4.

(g) `config.c`, the reset hook of section 9.1.1.

(h) `items.c`: rows 2864 to 2872 (sections 9.4.1 and 9.5.2), nine catalog stubs after
the `fnPview` stub (package `items.c:1097`), and `items.h` the nine
`ITM_` defines. `softmenus.c`: the nine items appended to `menu_CANVAS`
after `ITM_YRNG`.

#### 9.6.3 Identical-edit constraints

1. PK 1604 must equal undo-history's line 1604 byte for byte, comment
   included. Pretty-print-extra does not edit that line. Patches apply
   with `git apply -3` and identical bytes compose (package-manager
   README section "composition").
2. The package arm `else if(calcMode >= 20 && calcMode <= 23)` at PK
   1693-1697 stays identical to pretty-print-extra's (section 7 item 5).
3. The guard arm (b) is package-only. Pretty-print-extra's guard sits
   after the SNAP arm, a different line.
4. `defines.h` lines 1583 and 1585 are edited by no other package. The
   nearest sibling hunks are forth-core at 1520 and this package at 1705.
5. `config.c` line 1715 is 16 lines below undo-history's insert and 15
   above pretty-print's.
6. PK 2590 is upstream's `ITM_CC` line. The package does not edit it. If
   it ever does, the line must equal undo-history's 2584 byte for byte.

Without (c) the solo build cannot reach `BST`, `SST`, `RBR`, `FLGSV` from
real keys while the combined build can. A test that calls
`processKeyAction(ITM_BST)` passes in both. Pin P8 drives the real path
through `btnClicked`, so the solo build is red without (c).

#### 9.6.4 The shift glyph rule

`showShiftState` (`keyboardTweak.c:79-100`) paints the f or g glyph at
`X_SHIFT`, `Y_SHIFT`. With the date and the time both shown, `Y_SHIFT` is
24, inside the canvas. The status bar never repaints row 24, and the next
key wipes that corner white. Rule: in mode 21 the glyph always paints in
the status bar, at the right-side slot upstream uses for `FLAG_SBshfR`.
DECISION: two macro edits in `defines.h`:

    #define X_SHIFT                                  ((getSystemFlag(FLAG_SBshfR) || calcMode == CM_GRAPHICS_CANVAS) ? X_SHIFT_R : X_SHIFT_L)
    #define Y_SHIFT                                  (((!SBARUPD_Date || !(SBARUPD_Time || SBARUPD_WoY)) && !SBAR_SHIFT) ? 0 : ((SBAR_SHIFT || calcMode == CM_GRAPHICS_CANVAS) ? 0 : Y_SHIFT_LO ))

`calcMode` is visible wherever `getSystemFlag` is, so the macros compile
where they did. The softkey underline of `show_f_jm` stays opt-in through
`FLAG_FGLNFUL` and `FLAG_FGLNLIM`. Documented gap: with either flag set,
the underline paints inside region 6. Section 10 item 8 changes to: shifted
keys engage in the view and drive the 3D keys of section 9.6. Other
shifted items do nothing.

#### 9.6.5 The redraw

    pg3dRedraw():
      h = header; pg3dRecordView(&view)              // the frozen view; frozen views are valid
      bottom = (canvas.region == 2) ? 170 : 239
      lcd_fill_rect(0, 20, 400, bottom - 20 + 1, LCD_SET_VALUE)      // the region, the clip stays
      savedMode = canvas.drawMode; canvas.drawMode = 0
      pg3dSetup(&s, &view); pgClipNow(&clip)
      if h->gridValid:
        rows = allocC47Blocks(TO_BLOCKS(2 * h->numX * 4))
        if rows == NULL: displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE)
        else:
          for j in 0 .. h->numY - 1:
            y = view.ylo + (float)j * ((view.yhi - view.ylo) / (float)(h->numY - 1))
            for i in 0 .. h->numX - 1:
              x = view.xlo + (float)i * ((view.xhi - view.xlo) / (float)(h->numX - 1))
              b = h->grid[j * h->numX + i]
              zq = (b == PG3D_HOLE) ? NaN : pg3dDecode(b, view.zRecLo, view.zRecHi)
              pg3dMeshPoint(&s, rows, h->numX, i, j, x, y, zq, &clip)
          freeC47Blocks(rows, TO_BLOCKS(2 * h->numX * 4))
      for k in 0 .. h->lineCount - 1:
        pg3dDrawRecord(&s, pg3d.block + PG3D_BLOCK_BYTES - 6 * (k + 1), &clip)
      canvas.drawMode = savedMode
      pgRefreshNow()

DECISION: the redraw draws in mode 0 with the clip in force. The rotation
is a view operation and not a drawing command. The canvas is cleared
before the redraw, so mode 2 and mode 0 give the same picture. Mode 1
gives a blank canvas, which is not what the key means. 2D content on the
canvas is lost at the first key press. Documented limit.

#### 9.6.6 The zoom threshold re-run

The ruling: rotation never re-runs the program. A zoom press can cross
the magnification where one z step is more than one pixel. Such a press
runs the program once more, over the z range visible at that
magnification. One pass per press.

    pg3dZoomRerun():
      h = header
      if h->gridValid == 0 or h->label == 0: return
      pg3dRecordView(&view)                       // the frozen view
      pg3dWindow(&w)
      zoom  = pg3dZoom[pg3d.zoomStep + 8]
      dNear = (view.ylo - view.eyeY) / zoom       // eye to near face, positive
      wymax = w.ymin + 239.0f / w.ys
      zVisLo = view.eyeZ + (w.ymin - view.eyeZ) * dNear     // v = eyeZ + (z - eyeZ) / dNear, inverted, at the near face
      zVisHi = view.eyeZ + (wymax - view.eyeZ) * dNear
      if zVisLo > zVisHi: swap(zVisLo, zVisHi)                // a mirrored YRNG
      zNewLo = max(view.zlo, zVisLo); zNewHi = min(view.zhi, zVisHi)
      if not (zNewLo < zNewHi): return                         // nothing of the volume is visible
      pps    = (h->zRecHi - h->zRecLo) * (1.0f / 254.0f) * w.ys / dNear   // pixels per z step at the near face
      wider  = (zNewLo < h->zRecLo) or (zNewHi > h->zRecHi)
      if not (pps > 1.0f) and not wider: return
      pg3dRerun(h, &view, zNewLo, zNewHi)

The exact test, in words. A re-run is due when one z step of the recorded
range covers more than one pixel at the near face. A re-run is also due
when the visible z range reaches outside the recorded range. The visible
z range is computed at the near face with the rotation ignored. DECISION: the rotation is
ignored in this test. With a rotation the function value z is no longer
the screen vertical, and no visible z range exists.

    pg3dRerun(h, view, zNewLo, zNewHi):
      // the WIREFRAME protocol of 9.4.2 steps 2, 5 and 6, without drawing
      if engineNestingRefused(true): return
      rows = NULL; numX = h->numX; numY = h->numY; label = h->label
      view->zRecLo = zNewLo; view->zRecHi = zNewHi
      currentKeyCode = 255; saveForUndo()
      ++engineNestingDepth; ++plotEngineActive; ++currentSolverNestingDepth; setSystemFlag(FLAG_SOLVING)
      save the three program pointers
      pg3dSetup(&s, view)
      result = pg3dRunGrid(&s, h, NULL, numX, numY, true, label, false, NULL)
      restore the three program pointers
      if (--currentSolverNestingDepth) == 0: clearSystemFlag(FLAG_SOLVING)
      --plotEngineActive; --engineNestingDepth
      temporaryInformation = TI_NO_INFO
      fnUndo(0)
      h = (pg3d.block != NULL) ? header : NULL
      if h == NULL: return
      if result == PG3D_RUN_OK: h->zRecLo = zNewLo; h->zRecHi = zNewHi
      else: h->gridValid = 0                       // the grid bytes are half new: drop the grid, keep the lines
      if result == PG3D_RUN_ALLHOLES: displayCalcErrorMessage(lastPointError, ERR_REGISTER_LINE, REGISTER_X)

The re-run writes the grid bytes in place. DECISION: an aborted or failed
re-run drops the grid and keeps the lines. The block has no room for a
copy of the old grid. A label that no longer exists raises
`ERROR_LABEL_NOT_FOUND` from `fnGoto` inside `pg3dSample` at every point.
Every point is then a hole, so the error shows.

The re-run from a key press runs the program from the keyboard arm of
`fnExecute`. The program pointer restore of section 9.4.2 covers it. The
`5` key resets the zoom to 1 and calls the same test. The visible range is
then wider than a narrowed recorded range. One re-run brings the bytes
back to the full `ZVOL` range. For a deterministic program those bytes
equal the first record (pin P11 and oracle R2).

Worked numbers, the showcase view of section 9.7 (eye (0, -3, 0), volume
-1 to 1, `YRNG -0.6 0.6`, so `ys = 199.17` and `D0 = 2`):

| Press | zoom | dNear | visible z | pps before | re-run |
|---|---|---|---|---|---|
| none | 1.0 | 2.0 | -1 to 1 (clamped) | 0.78 | no |
| plus 1 | 1.25 | 1.6 | -0.96 to 0.96 | 0.98 | no |
| plus 2 | 1.5625 | 1.28 | -0.768 to 0.768 | 1.23 | yes |
| plus 3 to 6 | up to 3.81 | down to 0.52 | down to -0.315 to 0.315 | 1.18 each | yes, each |
| minus 1 to 6 | back to 1.0 | back to 2.0 | wider each press | below 1 | yes, each (wider) |

After the first crossing every further press re-runs, because the
visible range shrinks by the same factor the magnification grows. That is
the ruling's cadence, one pass per crossing, at every press past the
threshold. Six plus and six minus presses run the program 11 times, at
576 samples each (pin P11).

### 9.7 The 3D showcase and the animation

#### 9.7.1 The driver

One driver `pgTestShowcase3D` in the `TESTSUITE_BUILD` block after
`pgTestShowcase2D`, registered after it in `testSuite.c` and driven by a
`Func:` line at the end of `program_graphics.txt`.

Setup, in this order, each through the item function:

    calcMode = CM_NORMAL; lastErrorCode = ERROR_NONE; pgWindow.set = 0
    fnPview(6); fnGmode(0)
    XRNG -1 1; YRNG -0.6 0.6                 // 2.0 by 1.2 volume units on 400 by 240: square pixels
    EYEPT 0 -3 0; XVOL -1 1; YVOL -1 1; ZVOL -1 1; NUMX 24; NUMY 24
    load the saddle program and run WIREFRAME on it
    the cube of the volume box, 4 PT3D and 12 LINE3D
    DISP 1 with the caption "program-graphics G4: EYEPT XVOL YVOL ZVOL NUMX NUMY WIREFRAME PT3D LINE3D"

The saddle program, loaded with the pattern of `covWriteAndLoadPgm`
(`src/testSuite/testSuite.c:1167-1183`), which writes
`c47programTest.bin` and calls `fnLoadProgram`:

    static const uint8_t pgmSaddle[] = {
      ITM_LBL, STRING_LABEL_VARIABLE, 4, 'S', 'A', 'D', 'L',    // LBL "SADL"
      ITM_SQUARE,                                                // x²      X = x², Y = y
      ITM_XexY,                                                  // x⇄y     X = y,  Y = x²
      ITM_SQUARE,                                                // x²      X = y², Y = x²
      ITM_SUB,                                                   // -       X = x² - y²
      (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
    };
    label = findNamedLabel("SADL", GLOBAL_LABELS); fnWireframe(label)

The cube is the volume box, with x, y, z each -1 or 1. Its corners are
v0 (-1,-1,-1), v1 (1,-1,-1), v2 (1,1,-1), v3 (-1,1,-1), v4 (-1,-1,1),
v5 (1,-1,1), v6 (1,1,1), v7 (-1,1,1). The drawing order:

    PT3D v0; LINE3D v1, v2, v3, v0, v4, v5, v6, v7, v4     // 9 lines
    PT3D v1; LINE3D v5;  PT3D v2; LINE3D v6;  PT3D v3; LINE3D v7   // 3 lines

Retained bytes: 576 grid bytes and 72 line bytes, 648 of 1984.

A frame is written after each step with the two lines of the 2D driver:

    strcpy(_ioFileNameOverride, name); fnScreenDump(0);

Every name is `pg3d_NNN.bmp`, 12 characters, under the 21-character cap
of `fnScreenDump` (`screen.c:6328`). The files land in the project root,
the workdir of the suite (`src/testSuite/meson.build:35-40`).

#### 9.7.2 The frame plan

122 frames, 12.2 seconds at 10 frames per second.

| Frames | Content | Gesture |
|---|---|---|
| 000 | The still picture with the caption. | none |
| 001 | The home view, a redraw. | `processKeyAction(ITM_5)` |
| 002 to 037 | 36 steps about x. | `processKeyAction(ITM_UP1)` per frame |
| 038 to 073 | 36 steps about y. | `processKeyAction(ITM_BST)` per frame |
| 074 to 109 | 36 steps about z. | `processKeyAction(ITM_RBR)` per frame |
| 110 to 115 | 6 zoom-in steps. | `processKeyAction(ITM_ADD)` per frame |
| 116 to 121 | 6 zoom-out steps, back to 1.0. | `processKeyAction(ITM_SUB)` per frame |

Frame 001 exists because the still picture holds the caption and a redraw
does not. Every redraw compares against frame 001. `DOWN`, `SST`, and
`FLGSV` are not in the film: pin P26 presses each once.

`processKeyAction(ITM_UP1)` runs `fnKeyUp` at the press and no item at the
release, so one call is one step. The items `BST`, `RBR`, `ADD`, `SUB`,
`5` reach the guard arm from `processKeyAction` alone. The shift keys are
not in the film: pin P8 drives them once through `btnClicked`.

#### 9.7.3 The assembly script

`pg3d_gif.py` lives in the scratchpad, next to the frames, not in the
repository. PIL 10.2.0 and imageio 2.37.4 are installed. The script
refuses a frame that is not a 400 by 240 mode-P BMP with the simulator
palette. It prints the lit count per frame with the rule of the C pin. It
reports whether the first and the last redraw frames are byte-identical.

    mkdir -p $SCRATCH/pg3d && mv /home/stan/c43-g3/pg3d_*.bmp $SCRATCH/pg3d/
    python3 $SCRATCH/pg3d_gif.py $SCRATCH/pg3d $SCRATCH/pg3d_showcase.gif --fps 10 --scale 2

    #!/usr/bin/env python3
    import argparse, glob, os, sys
    from PIL import Image
    WIDTH, HEIGHT, TOP_ROW = 400, 240, 20
    PALETTE = [0xDF, 0xF5, 0xCC, 0, 0, 0]
    def load_frame(path):
        im = Image.open(path)
        if im.format != "BMP": sys.exit(f"{path}: not a BMP ({im.format})")
        if im.size != (WIDTH, HEIGHT): sys.exit(f"{path}: size {im.size}")
        if im.mode != "P": sys.exit(f"{path}: mode {im.mode}, expected P")
        if im.getpalette()[:6] != PALETTE: sys.exit(f"{path}: palette")
        return im
    def lit_count(im):
        px = im.load()
        return sum(1 for y in range(TOP_ROW, HEIGHT) for x in range(WIDTH) if px[x, y] == 1)
    def canvas_bits(im):
        return im.crop((0, TOP_ROW, WIDTH, HEIGHT)).tobytes()
    def main():
        ap = argparse.ArgumentParser()
        ap.add_argument("frame_dir"); ap.add_argument("out_gif")
        ap.add_argument("--pattern", default="pg3d_*.bmp")
        ap.add_argument("--fps", type=float, default=10.0)
        ap.add_argument("--scale", type=int, default=1)
        a = ap.parse_args()
        files = sorted(glob.glob(os.path.join(a.frame_dir, a.pattern)))
        if len(files) < 2: sys.exit("fewer than two frames")
        frames, counts = [], []
        for f in files:
            im = load_frame(f); counts.append(lit_count(im))
            rgb = im.convert("RGB")
            if a.scale > 1: rgb = rgb.resize((WIDTH * a.scale, HEIGHT * a.scale), Image.NEAREST)
            frames.append(rgb.convert("P", palette=Image.ADAPTIVE, colors=2))
        ms = int(round(1000.0 / a.fps))
        frames[0].save(a.out_gif, save_all=True, append_images=frames[1:], duration=ms, loop=0, optimize=False)
        g = Image.open(a.out_gif)
        print(f"{a.out_gif}: {g.n_frames} frames, {g.info.get('duration')} ms, loop={g.info.get('loop')}")
        for f, c in zip(files, counts): print(f"{os.path.basename(f)}: {c} lit pixels in rows 20 to 239")
        home, last = load_frame(files[1]), load_frame(files[-1])
        print("frame 001 and the last frame canvas identical:", canvas_bits(home) == canvas_bits(last))
    if __name__ == "__main__": main()

At about 3 KB per frame the GIF is about 400 KB. The frames and the GIF
go to Stan with `SendUserFile`. Nothing is committed: the precedent is the
2D showcase, whose BMP stayed at the worktree root and whose PNG went out
by hand. `pg3d_*.bmp` in `.gitignore` next to `20*.bmp` is Stan's call.

#### 9.7.4 The oracles

Recorded-count oracles, the S1 pattern:

| Oracle | Frame | Assertion |
|---|---|---|
| S3 | 000 | The lit count of rows 20 to 239 equals a value recorded once at the first green run. |
| S3h | 001 | The home redraw has its own recorded count. |
| S3a | before the cube | `WIREFRAME` of the saddle alone, recorded once. |
| S3b | on an erased canvas | The cube alone, recorded once. |

The numbers cannot be derived before the first run. They are recorded as
G2 did (DESIGN-HISTORY.md, stage G2).

Return-to-start oracles:

| Oracle | Assertion |
|---|---|
| R1 | Before frame 002, copy bytes 2 to 51 of rows 20 to 239 (11,000 bytes) into a suite buffer. After frame 037, `memcmp` the same bytes: zero differing bytes. Byte 0 of each row, the dirty flag, is skipped. |
| R1y, R1z | The same after frame 073 and after frame 109. |
| R2 | After frame 121 the canvas bytes equal the frame 001 copy, and the lit count equals S3h. The zoom re-runs at zoom 1.0 record the same bytes for a deterministic program. |

The exact return holds for two reasons. The angles are integer counts
modulo 36. The redraw is a pure function of the block, the angles, the
zoom, and the window.

Structural oracles are the pins P1 and P2 of section 9.8.

### 9.8 Pins

Every pin sets its state and restores it (TESTING.md section 2). "The
unit-cube view" means the setup of section 9.3.6 inside `PVIEW 6`. "The
showcase view" means the setup of section 9.7.1. A program in a pin is
loaded as in section 9.7.1. Item drives use `runFunction` for the
commands and `processKeyAction` for the keys, except where a pin names
`btnClicked`.

| Pin | Assertion | Expected numbers | Mutation that reddens it |
|---|---|---|---|
| P1 | The eight corners of the unit cube through `pg3dProject` in the unit-cube view. | (0,239), (399,239), (0,0), (399,0), (100,179), (299,179), (100,60), (299,60). The pixels (100,179), (299,179), (100,60), (299,60) are lit after the twelve `LINE3D` edges. | Project onto the plane y = yhi (u = x): the far face moves. |
| P2 | `WIREFRAME` of the program `LBL "PLNE", CLX, END` with `NUMX 2`, `NUMY 2` in the unit-cube view. | 798 lit pixels in rows 20 to 239: two rows of 400 and 200, two columns of 101 each, minus 4 shared corners. | Skip the row lines (`i > 0`): 202. |
| P3 | `c47MemInBlocks` rises by exactly 512 at the first `EYEPT` inside the view and falls by 512 at `pgCloseView`. `EYEPT` outside the view changes it by 0. | 512, 512, 0 | Skip the free in `pgCloseView`. |
| P4 | With the pool exhausted (allocate every free block in chunks until `allocC47Blocks(512)` returns NULL), `EYEPT` inside the view raises `ERROR_RAM_FULL`, leaves `pg3d.block` NULL, and leaves the eye unchanged. The chunks are freed after. | error 11 | Skip the NULL test in `pg3dEnsure`. |
| P5 | `pg3dEncode(NaN, 0, 1)` is 255, `pg3dEncode(0, 0, 1)` is 0, `pg3dEncode(1, 0, 1)` is 254, `pg3dEncode(7, 0, 1)` is 254, `pg3dEncode(-7, 0, 1)` is 0, `pg3dDecode(254, 0, 1)` is 1.0f, `pg3dDecode(127, 0, 1)` is within 0.002 of 0.5. | as listed | Scale by 255. |
| P6 | `NUMX 45`, `NUMY 45`, `WIREFRAME` of PLNE in the unit-cube view draws (lit above 0), the header has `gridValid` 0 and `numX` 0. One UP press then leaves the canvas byte for byte and `angX` 0 (ruling 2026-09-05). | above 0, 0, 0, 0 bytes | Remove the nothing-retained return in `pg3dKey`. |
| P7 | The showcase view, then `btnClicked` with the UP key string once (key "22" of `kbd_std_R47f_g`, verified by the pin against `assign.c:370-392`). `angX` is 1 and the lit count differs from S3. | 1 | Call `pg3dKey` in the release path too: 2. |
| P8 | The showcase view, `btnClicked` f then UP. `angY` is 1, `angX` is 0, `shiftF` is false after. Then g then UP: `angZ` is 1. | 1, 0, 1 | Restore upstream's shift gate line: the shift does not engage and `angX` becomes 1. |
| P9 | `processKeyAction` with `ITM_RBR`, `ITM_FLGSV`, `ITM_ADD`, `ITM_SUB`, `ITM_5`, `ITM_4` in turn from the home view. | `angZ` 1 then 0, `zoomStep` 1 then 0, all zero, all zero | Drop `ITM_RBR` from the guard arm. |
| P10 | R1 of section 9.7.4: 36 UP presses give zero differing canvas bytes. | 0 | Count the steps modulo 37. |
| P11 | The showcase view. Six `ITM_ADD` presses call `execProgram` 5 times 576 times (a static counter `pg3dRunCount` in the file). Six `ITM_SUB` presses add 6 times 576. | 2880, then 6336 | Skip the `wider` test: the minus presses add 0. |
| P12 | 330 `LINE3D` calls fill the block. The 331st draws (lit count rises) and `lineCount` stays 330, and the header bytes 0 to 63 are unchanged. | 330 | Skip the free-bytes test: the record overlaps the header. |
| P13 | `WIREFRAME` of `LBL "HOLE", √x, END` with `NUMX 2`, `NUMY 2`, `XVOL -1 1`, `YVOL 0 1`, `ZVOL 0 1`, eye (0, -1, 0.5), `XRNG -1 1`, `YRNG 0 1`. The two samples at x = -1 are holes. `lastErrorCode` is 0 after. | The lit count equals the count of the one column line between the two valid samples, computed by the pin from the projected pixels through `pgLine` into a scratch count. | Treat an error as z = 0: the two row lines appear. |
| P14 | `WIREFRAME` of `LBL "STP", STOP, END` in the unit-cube view with `NUMX 2`, `NUMY 2`. `lastErrorCode` is `ERROR_SOLVER_ABORT` (60), `gridValid` is 0, `programRunStop` is not `PGM_RUNNING`. | 60, 0 | Skip the abort test: the run completes. |
| P15 | With `engineNestingDepth` set to 1, `WIREFRAME` of PLNE returns with `programRunStop == PGM_WAITING`, `engineNestingWasRefused` true, no pixel lit, `c47MemInBlocks` unchanged. The pin restores the depth and `programRunStop`. | 2, true, 0 | Call `engineNestingRefused(false)`: the mesh draws. |
| P16 | X, Y, Z, T hold the long integers 1, 2, 3, 4 before `WIREFRAME` of the saddle. After it they hold 1, 2, 3, 4. | 1, 2, 3, 4 | Skip `fnUndo(0)`. |
| P17 | From the keyboard state (`programRunStop == PGM_STOPPED`), `currentProgramNumber`, `currentLocalStepNumber`, and `currentStep` are equal before and after `WIREFRAME`. | equal | Skip the pointer restore: the pin stays green, because `execProgram` (`lblGtoXeq.c`) restores the three pointers itself. The engine's restore is a belt over upstream's, and the pin guards that upstream behaviour. Documented pin limit. |
| P18 | With a block allocated, `pgReset()` leaves `pg3d.block` NULL, `c47MemInBlocks` unchanged, and the HP defaults in `pg3d`. The pin frees the block itself after. | NULL, unchanged, eye (0, -3, 0), volume -1 to 1, 10 by 8 | Free in `pgReset`: `c47MemInBlocks` falls by 512. |
| P19 | After the showcase and `ERASE`, the header has `lineCount` 0 and `gridValid` 0, and one UP press leaves 0 lit pixels. | 0, 0, 0 | Skip `pg3dEmpty` in `pgSetRegion`: the picture returns. |
| P20 | `NUMX` with 1, 101, 2.5 in X raises `ERROR_OUT_OF_RANGE` (8) and leaves `numX`. A string raises `ERROR_INVALID_DATA_TYPE_FOR_OP` (24). `XVOL` with 1 and 1 raises `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` (1). `EYEPT` with NaN raises 8. `WIREFRAME` with `eyeY` 2 and `ylo` 0 raises 1 and lights nothing. | 8, 8, 8, 24, 1, 8, 1 | Skip the eye rule. |
| P21 | In mode 21 with `FLAG_SBdate` and `FLAG_SBtime` set and `FLAG_SBshfR` clear, `Y_SHIFT` is 0 and `X_SHIFT` is `X_SHIFT_R`. In `CM_NORMAL` with the same flags `Y_SHIFT` is 24. | 0, X_PRINTER - 1, 24 | Restore the upstream macros: 24 in mode 21. |
| P22 | After the saddle in the showcase view, `EYEPT 0 -6 0`, then one UP press. The lit count equals the count after one UP press without the `EYEPT`, measured in a fresh sequence. | equal | Read `pg3d.eye` in the redraw. |
| P23 | With `haveCur` 0, `LINE3D` lights nothing and sets `haveCur` 1 and the current point. | 0 lit, 1 | Draw from (0, 0, 0). |
| P24 | S3, S3h, S3a, S3b of section 9.7.4, recorded once. | recorded | Any change to a primitive or to the projection. |
| P25 | `WIREFRAME` of HOLE with `XVOL -2 -1`: every sample errors. `lastErrorCode` is `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` (1), nothing is lit, `gridValid` is 0. | 1, 0, 0 | Never re-raise: 0. |
| P26 | From the home view, one press each of `ITM_DOWN1`, `ITM_SST`, `ITM_FLGSV`. | `angX` 35, `angY` 35, `angZ` 35 | DOWN adds 1: 1. |
| P27 | The plane P2 with `GMODE 2` twice over the same mesh: the second `WIREFRAME` restores the canvas to 0 lit pixels. | 0 | Draw the still picture in mode 0 regardless of GMODE. |
| P28 | `WIREFRAME` of the saddle, then 36 `ITM_BST` presses, then 36 `ITM_RBR` presses. R1y and R1z of section 9.7.4: zero differing bytes each. | 0, 0 | Compose the matrices in the other order for one of the axes only: still 0. Use `angY % 36` for the sine index and `(angY + 8) % 36` for the cosine: red. | Covered by R1y and R1z of the showcase driver.
| P31 | (The code's former P27.) A point exactly one 1024th of the depth in front of the eye is drawable; a nearer point is not. | drawn, rejected | Make the eps test exclusive again. |
| P32 | (The code's former P28.) The far corner of an extreme window projects to (32000, 32000): the clamp holds on the final row. | 32000, 32000 | Drop the row clamp after the flip. |
| P20b | `XVOL -2e38 2e38`, a span that overflows float, is refused. | 1 | Drop the finite-span test. |
| P29 | A body that calls `ERASE`, `PT3D` and `LINE3D` under `WIREFRAME` leaves no valid grid. | 0 | Drop the counts test before `gridValid`. |
| S0 | `indexOfItems[LAST_ITEM]` is upstream's sentinel row: the name `"Last item"` and the function `itemToBeCoded`. | sentinel | Put the WIREFRAME row back at the last index. |
| L1 | `pgTestDraw3D` returns every pool block it takes: `c47MemInBlocks` is equal at both ends after the statistics, the stack and the undo image are normalised. | 0 | Remove the free before P31's `pgReset()`: 512. |
| O1 | A plot step (`fnSigmaAddRem` twice, `fnPlotStat(PLOT_START)`) leaves the view without EXIT. The next `EYEPT` returns the block and zeroes the region. | NULL, 0, minus 512 | Restore the `canvas.region == 0` test in `pg3dEnsure`. |
| B1 | In the view with a block, `pgBeforeSave()` restores the previous mode, returns the block and zeroes the region. | mode, NULL, 0 | Empty `pgBeforeSave`. |
| B2 | The same after the plot step of O1 abandoned the view; the plot mode stays. | plot mode, NULL, 0 | Empty `pgBeforeSave`. |
| E1 | With the test switch the undo save fails: `WIREFRAME` shows `ERROR_RAM_FULL`, runs no sample, keeps X to T, the engine counters, `FLAG_SOLVING`, the pool count and the old header. | 11, unchanged | Remove the `ERROR_RAM_FULL` test in `pg3dEngineEnter`. |
| E2 | The same at a zoom re-run: the run count does not rise, `gridValid` stays 1, the recorded z range stays. | unchanged | The same. |
| H1 | Program `CND` erases at the first sample and records one line at every sample of a 17 by 17 grid: 289 records, all intact, `gridValid` 0. | 289 | Restore the frozen-only retention test: record 282 is overwritten. |
| H2 | Program `CNE` erases at a zoom re-run: the header stays empty and no z range is written into it. | 0, 0 | Write the z range on `PG3D_RUN_OK` alone. |
| V1 | `XVOL 0 7.4e-37` is refused and leaves the volume; `XVOL 0 7.6e-37` is accepted; `YVOL` and `ZVOL` refuse the same span. | 1, accepted | Drop the scale test in `pg3dSpanUsable`. |
| Z1 | Eye at z 0, `YRNG 0 2e-36`, `ZVOL -1 1`, a valid grid: zoom presses 1 to 4 re-run the program and press 5 is refused, because 254 divided by the visible slice overflows float. | runs at 4, none at 5 | The same. |
| W7 | `XRNG 0 1e39`, then `WIREFRAME`: `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`, nothing lit, no sample run. | 1, 0, 0 | Remove the window test in `fnWireframe`. |
| W8 | `XRNG 1 0` (mirrored) with the plane of P2: 798 lit pixels and no error. | 798 | Add `mn < mx` to `pg3dWindowUsable`. |
| W9 | A valid grid, then `XRNG 0 1e39`, then UP: `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`, the canvas bytes identical, `angX` 0. | 1, 0, 0 | Remove the window test in `pg3dKey`. |
| T1, T2, T3 | After UP and +, `ERASE` (T1), `PVIEW 6` (T2), or EXIT then `PVIEW 6` (T3): angles 0, zoom 0, and the next `WIREFRAME` equals the home drawing byte for byte. | 0, 0, 0 bytes | Skip the reset in `pgSetRegion` (T1, T2) or in `pgCloseView` (T3). |
| T4 | `PT3D` outside the view, `PVIEW 6`, `LINE3D`: nothing lit and `haveCur` 1. | 0, 1 | Keep the NULL test first in `pg3dEmpty`. |
| R0 | The showcase still picture equals the redraw after UP then DOWN, byte for byte. | 0 bytes | Change one divisor in the redraw copy (before `pg3dGridCoord`), or blank the redraw. |
| S3h | The home redraw of the showcase (frame 001) has its own recorded count. | recorded | Any change to the redraw. |

P13 and P25 use `√x` (`ITM_SQUAREROOTX`, 61): a negative real with
`FLAG_CPXRES` clear raises `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`
(`squareRoot.c:37`). The pin clears `FLAG_CPXRES` and restores it. P14
uses `STOP` (`ITM_STOP`, 70). P2 uses `CLX` (`ITM_CLX`, 41). All three are
one-byte opcodes.

Pin limit: the DM42 key repeat and the DMCP `exitKeyWaiting` poll are not
reachable from the suite. Documented.

### 9.9 Limits and documented gaps

The limits are in section 10.

## 10. Limits and documented gaps

1. The canvas does not survive sleep or power off.
2. Region code 1 is not supported.
3. Text is not clipped inside its cell.
4. `GMODE` does not apply to text.
5. The DMA refresh on the DM42 is untested by the package.
6. The dirty-flag protocol of the DM42 ROM is assumed from the simulator
   and from upstream's hardware code (section 4.1). Untested by the package.
7. `RESET` inside the view keeps the view open with a blank canvas. EXIT
   closes it. A program that opens a plot view abandons the canvas.
8. Shifted keys engage in the view from G4 on: the package carries
   undo-history's shift gate line. f-UP, f-DOWN, g-UP, g-DOWN drive the 3D
   rotation. Every other shifted item does nothing.
9. The error message on canvas line 1 covers rows 20 to 39 of the drawing.
10. Every error of a drawing command names register X in its message,
    whichever register held the offending value.
11. Range ends for `XRNG` and `YRNG` that differ only beyond 34 digits
    are refused as equal ends.
12. A rotation clears the canvas. 2D content drawn on the canvas is lost at
   the first 3D key press.
13. The retained block holds 44 by 44 at most. A larger grid draws once
   and does not rotate; a key press leaves it as it is. A line past 330,
   or past the free bytes next to a grid, draws once and does not rotate.
14. The block takes 512 resident pool blocks while the view is open. With
   the undo-history ring the RCL58 slack is exceeded, so a 14 by 14
   eigenvalue inside the view can raise `ERROR_RAM_FULL`.
15. The volume and the eye freeze at the first record. `XVOL`, `YVOL`,
   `ZVOL`, and `EYEPT` after that take effect only after `ERASE`, `PVIEW`,
   or EXIT.
16. A coordinate outside the volume clamps to the volume face, so a line
   that leaves the volume changes direction.
17. The z bytes span 254 steps, so a surface is quantized to 1/254 of its
   range. At the default distance one step is under one pixel. Past the
   zoom threshold the program runs again at every press.
18. The zoom threshold test ignores the rotation.
19. The 3D commands outside the view draw once and retain nothing.
20. A body that calls `ERASE` or `PVIEW` during `WIREFRAME` empties the
   block under the run. The run continues and retains nothing.
21. Float results differ in the last bit between a build that contracts
    `a * b + c` into a fused multiply-add and one that does not. A pixel
    can flip at an exact rounding boundary between the DM42 and the
    simulator. The pins run on the simulator only.
22. The DM42 key repeat for UP and DOWN is untested.
23. With `FLAG_FGLNFUL` or `FLAG_FGLNLIM` set, the shift underline paints
    inside region 6.
24. The default window puts a unit volume into one pixel. A 3D drawing
    needs `XRNG` and `YRNG` in volume units, or the eye and the volume in
    pixel units.
25. No hidden-line removal. No depth sort.
26. The 8-level stack registers A to D are not loaded for the body.
27. The 3D commands and the keys convert `XRNG` and `YRNG` to 32-bit
    floats. Ends that do not give finite floats with a finite, non-zero
    pixel scale are refused with `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`, and
    the picture stays. A mirrored window is legal. Ruling 2026-09-05.
28. `ERASE`, `PVIEW` and EXIT return the angles and the zoom to the home
    view. Ruling 2026-09-05.
29. The canvas view closes before the calculator state is saved, so a
    backup never holds mode 21. A backup written by the G4 build with
    mode 21 is not handled.
30. A `WIREFRAME` or a zoom re-run whose undo save fails with
    `ERROR_RAM_FULL` is refused before a sample runs. The old picture
    stays. In the test build a switch makes the next save fail.

## 11. Test policy

All tests run on the simulator. The pixel oracle is `lcd_buffer_pixel_on`.
The contract is in TESTING.md.
