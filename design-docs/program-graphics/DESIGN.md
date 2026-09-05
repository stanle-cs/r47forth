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
| `RECT` | same | Fills the rectangle with corners at point 1 and point 2. | Prime |
| `CIRCLE` | cx in X, cy in Y, r in Z | Draws the outline of a circle. | none |
| `FCIRCL` | same | Fills a circle. | none |
| `ARC` | center as a complex number in T, r in Z, a1 in Y, a2 in X | Draws an arc counterclockwise from a1 to a2, in the current angle unit. A span of 360 degrees or more draws a full circle. | RPL |
| `TEXTOUT` | x in X, y in Y, text in the alpha register | Draws the text with the standard font. The point is the top-left corner of the text cell. | Prime |
| `DISP n` | n = 1 to 11, a step parameter; text in the alpha register | Draws the alpha register on canvas line n, from the top. | RPL |
| `GMODE n` | n = 0, 1, or 2, a step parameter | Sets the draw mode: 0 sets pixels, 1 clears pixels, 2 inverts pixels. | RPL `TLINE`, 42S `GRAMOD` |
| `GCLIP` | x1 in X, y1 in Y, x2 in Z, y2 in T | Sets the clip rectangle. Later commands draw inside it only. | none |

### 2.3 User coordinates, stage G3

| Command | Stack | Effect | Precedent |
|---|---|---|---|
| `XRNG` | xmin in Y, xmax in X | Sets the x range of the window. | RPL |
| `YRNG` | ymin in Y, ymax in X | Sets the y range of the window. | RPL |

### 2.4 3D, stage G4

| Command | Stack | Effect | Precedent |
|---|---|---|---|
| `EYEPT` | x in Z, y in Y, z in X | Sets the eye point. | RPL |
| `XVOL`, `YVOL`, `ZVOL` | low in Y, high in X | Set the view volume. | RPL |
| `NUMX`, `NUMY` | n in X | Set the grid counts. | RPL |
| `WIREFRAME` | the label of a user program, a step parameter | Draws z = f(x, y) as a mesh (section 9). | RPL |
| `PT3D` | x in Z, y in Y, z in X | Sets the current 3D point. | none |
| `LINE3D` | x in Z, y in Y, z in X | Draws a 3D line from the current point to this point. This point becomes the current point. | none |

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
| Any softkey, softkey path | Ignored. The package carries the range clause `calcMode < 19 /* package browsers 19-23, claims registry */` on the three softkey functions, byte-identical to the other packages. |
| `VIEW`, `AVIEW` inside the view | The upstream body sets `temporaryInformation` and calls `refreshScreen`. The rule above paints nothing. The program continues. On EXIT, `temporaryInformation` is reset, so nothing shows later. |
| A program step whose item function switches on `calcMode`: `ENTER`, `CC`, `.d`, `.ms` | The function runs as in `CM_NORMAL` while a program runs. The package helper `pgEffectiveCalcMode()` returns `CM_NORMAL` when `calcMode` is 21 and `programRunStop` is running, else `calcMode`. The four functions switch on it. From the keyboard, the same keys do nothing. Audit G1 round 1, finding G1R1-1 and G1R1-3. |
| An error inside the view | Nothing paints at once. The next `refreshScreen` in mode 21 clears canvas line 1 (rows 20 to 39) and writes the error text there. When the error is gone, the next refresh clears the band again. The register line painter `refreshRegisterLine` returns at once in mode 21, so the upstream error line and any other register line never paint over the canvas. Audit G1 round 1, finding G1R1-4. |
| EXIT with an error pending | Upstream consumes the EXIT press to clear the error. The view stays open. A second EXIT closes it. |
| Shifted keys, f and g | The shift keys do not engage in mode 21. Shifted items are not reachable from the keyboard in the view. Documented limit. SNAP on the R47 keyboard is a long press of EXIT. |
| `PAUSE` inside the view | Upstream flushes the buffer once and waits. The canvas stays. |
| `CLLCD` inside the view | Upstream clears the whole screen, status bar included. The view stays open. The status bar repaints at the next refresh. |
| A program step that calls `calcModeNormal()`, such as `CLSTK` or `CLA` | `calcModeNormal` returns at once while the view is open. The view and the drawing stay. The package patches `calcMode.c` for this. |
| A program step that opens a plot view (`Draw`, `PLTf`, `PLSTAT`, `SCATR`, `HPLOT`) or stores a non-finite plot range | The plot takes the screen and sets its own mode. The canvas is abandoned and the next repaint erases it. By design: the program asked for a plot. `canvas.region` stays set, which is harmless, because every reader of it runs in mode 21 only. |
| Sleep or power off | Upstream repaints on wake. The canvas is lost. Documented limit. |

### 3.7 ERASE

    fnErase():
      if canvas.region == 0: fnPview(2); return
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
upstream does. `RECT` in modes 0 and 1 calls `pgRun` per row.

The hardware assumption of this section is: the DMCP ROM's refresh treats
byte 0 of a row as the dirty flag, as the simulator does. Evidence:
upstream's own hardware code writes the header (`screen.c:695-696`) and
the simulator's `bitblt24` was written to mirror the ROM. The package
cannot test this on the DM42 (section 11). The test suite pins that a
direct write and a `bitblt24` write leave the same bytes in the buffer.

### 4.2 Clipping law

Every primitive clips before it touches the buffer. `bitblt24` does not
check the row. `lcd_fill_rect` drops the whole call when any edge is off
screen. An off-screen argument is not an error. It draws nothing.

### 4.3 Lines

Integer Bresenham, one `pgPixel` per step. Horizontal lines call `pgRun`
once. Vertical lines call `pgPixel` per row. The endpoints are inclusive.

### 4.4 Rectangles

`BOX` draws four lines with `pgRun` for the top and bottom rows and
`pgPixel` for the side columns. `RECT` clamps the rectangle to the clip
rectangle and calls `lcd_fill_rect` once in modes 0 and 1. In mode 2 it
calls `pgRun` per row.

### 4.5 Circles and arcs

`CIRCLE` uses the midpoint algorithm with `pgPixel`. `FCIRCL` uses the
same algorithm and calls `pgRun` for each scan line pair. `ARC` computes
the start and end angles in the current angular mode, then steps the
circle with the midpoint algorithm and draws the pixels whose angle lies
in the span. The angle test uses integer octant logic, no trigonometry.
A span of 360 degrees or more draws the full circle. Stage G2 specifies
the octant test in full before implementation.

### 4.6 Text

`TEXTOUT` calls `showString(alpha, &standardFont, col, row, vmNormal,
true, true)` after a clip test of the whole text cell. The text is not
clipped inside the cell. A cell that does not fit the clip rectangle is
not drawn. `GMODE` does not apply to text. `DISP n` computes the cell:

    row = 20 + (n - 1) * 20
    col = 1
    if row + 19 > canvas.clipY1: return
    clear the band rows row..row+19, cols 0..SCREEN_WIDTH-1 to white
    showString(alpha, &standardFont, col, row, vmNormal, true, true)

Region 2 has lines 1 to 7. Region 6 has lines 1 to 11. `n` outside the
region draws nothing.

### 4.7 Draw mode

`GMODE n` with n outside 0 to 2 raises `ERROR_INVALID_DATA_TYPE_FOR_OP`.

## 5. Coordinates

### 5.1 Argument types, stage G2 and G3

A drawing command reads each coordinate register by type:

| Register type | Meaning | Path |
|---|---|---|
| long integer | pixel | The fast path. The value is read as int32. Values outside -32768 to 32767 are clipped to that range. |
| real | user coordinate through the window (G3). Before G3, a real is a pixel after rounding toward zero. | The slow path. |
| complex, for `ARC` center and for the two-point form of `LINE` (G3) | two reals | The slow path, twice. |
| any other type | error `ERROR_INVALID_DATA_TYPE_FOR_OP` | |

Both points of one command must use the same type. A mixed pair is the
same error.

### 5.2 The window, stage G3

    typedef struct {
      real34_t xmin, ymin;
      real34_t xscale, yscale;   // (SCREEN_WIDTH - 1) / (xmax - xmin), (SCREEN_HEIGHT - 1) / (ymax - ymin)
    } pgWindow_t;

The default window is xmin 0, xmax 399, ymin 0, ymax 239, so a real
behaves as a pixel until `XRNG` or `YRNG` change it. `XRNG` with
xmax equal to xmin raises `ERROR_INVALID_DATA_INPUT`. The conversion of a
real x is `pixel = int32(round((x - xmin) * xscale))` in real34 arithmetic.
The window is part of `pgCanvas_t` from G3 on.

## 6. Items and menu

The package claims the spare rows 2448 to 2463 of `items.c` (all
`CAT_FREE`, far from every sibling claim) and the 3D rows from 2864 on
when G4 lands.

| Row | Item | Since |
|---|---|---|
| 2448 | `PVIEW` (`TM_VALUE`, `PTP_NUMBER_8`, min 2, max 6) | G1 |
| 2449 | `ERASE` | G1 |
| 2450 to 2461 | the 2D commands of §2.2 and §2.3 | G2, G3 |
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
3. Integer math only in the kernel. Horizontal runs use `lcd_fill_rect`.
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

### 9.1 State

    typedef struct {
      float eyeX, eyeY, eyeZ;
      float xlo, xhi, ylo, yhi, zlo, zhi;
      uint8_t numX, numY;
      float curX, curY, curZ;    // the current point
      bool_t haveCur;
    } pg3d_t;

### 9.2 Projection

Perspective from the eye point onto the plane y = yhi... Stage G4
specifies the projection with the RPL semantics of `EYEPT` and the view
volume, in float, and records the formulas here before implementation.
The math bridge is `realToFloat` on input and integer pixels on output.

### 9.3 WIREFRAME

    for j in 0..numY-1:
      y = ylo + j * (yhi - ylo) / (numY - 1)
      for i in 0..numX-1:
        x = xlo + i * (xhi - xlo) / (numX - 1)
        store x in the X register and y in the Y register as reals
        execute the labelled program
        read z from the X register as a real; convert to float
        project (x, y, z) to a pixel; store in row[j & 1][i]
        if i > 0: line from row[j & 1][i-1] to row[j & 1][i]
        if j > 0: line from row[(j-1) & 1][i] to row[j & 1][i]

Two rows of `numX` pixel pairs: 2 * numX * 4 bytes, from the transient
pool. `numX` and `numY` are limited to 2 to 100. The program runs through
the same call as `PGMSLV` plus `PLTf` use (`execProgram`).

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
8. Shifted keys are not reachable from the keyboard in the view.
9. The error message on canvas line 1 covers rows 20 to 39 of the drawing.

## 11. Test policy

All tests run on the simulator. The pixel oracle is `lcd_buffer_pixel_on`.
The contract is in TESTING.md.
