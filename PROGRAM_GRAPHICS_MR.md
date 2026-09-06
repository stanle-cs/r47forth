## Summary

This merge request adds program-driven drawing to C47.

It introduces a dedicated, persistent canvas view (`CM_GRAPHICS_CANVAS`) that prevents screen refreshes and program termination (`STOP`) from wiping out drawn graphics. The engine provides ten 2D drawing primitives, automatic coordinate scaling, and an interactive HP 48-style 3D wireframe pipeline with real-time viewport rotation and zoom.

## Core capabilities

### 1. Canvas lifecycle and view control
- `PVIEW n`: Opens the canvas view.
  - `PVIEW 2`: Opens over the register area (rows 20–170), keeping softkey menus visible.
  - `PVIEW 6`: Opens across the full display (rows 20–239) below the status bar.
- `ERASE`: Clears the canvas to white and resets the clip rectangle. Automatically opens `PVIEW 2` if called while closed.
- `EXIT`: Closes the canvas and returns to the previous calculation mode.
- `R/S`: Starts or pauses program execution normally while inside the canvas.
- **Background isolation**: When programs execute inside the canvas view, `pgEffectiveCalcMode()` returns `CM_NORMAL` so standard instructions (like `ENTER` or `.d`) execute properly without triggering canvas UI handlers.

### 2. 2D drawing primitives
- `LINE`, `BOX`, `FBOX`: Draws lines, rectangle outlines, and solid black rectangles.
- `CIRCLE`, `FCIRCL`, `ARC`: Draws circle outlines, filled circles, and circular arc segments between specified angles.
- `TEXTOUT`, `DISP n`: Renders text at an exact `(x, y)` pixel coordinate or aligned to text line `n` (1 to 11).
- `GMODE n`: Sets the pixel drawing mode: `0` (Set / Black), `1` (Clear / White), `2` (Invert / XOR). Invert mode enables white text or cutouts inside filled black shapes.
- `GCLIP`: Sets a rectangular clipping boundary. Primitives automatically clip to this window without throwing out-of-bounds errors.
- **Stack preservation**: Commands inspect stack arguments non-destructively without popping the stack, enabling coordinate reuse across multiple drawing calls.

### 3. Coordinate windows and scaling
- The origin sits at the bottom-left corner (400 × 240 pixels), with $x$ extending right and $y$ extending up.
- `XRNG`, `YRNG`: Define user-coordinate plotting windows for mathematical functions.
- **Type-directed routing**:
  - Long integers represent direct screen pixels, bypassing floating-point arithmetic.
  - Real numbers automatically scale through the active `XRNG` and `YRNG` window.
  - Complex numbers represent `(x, y)` points in stack registers `(Y, X)`.

![2D Drawing Commands Demo](/uploads/b48a7a2b198ef36354b7b758ddf60bf0/pg-attach-1-2d-commands.png)
*2D drawing commands showing sine wave plotted via XRNG/YRNG, ARC, outline BOX, solid FBOX with inverted GMODE 2 cutout, and clipped quarter circle.*

### 4. 3D wireframe engine (HP 48 heritage)
- `EYEPT`: Sets the camera position in `(x, y, z)` space.
- `XVOL`, `YVOL`, `ZVOL`: Defines the 3D bounding volume box.
- `NUMX`, `NUMY`: Sets the grid evaluation density.
- `WIREFRAME <label>`: Evaluates a user program $z = f(x, y)$ across the grid and renders the connected wireframe mesh.
- `PT3D`, `LINE3D`: Sets 3D point anchors and plots custom 3D vectors.
- **Interactive rotation and zoom**:
  - `UP` / `DOWN`: Rotates around the x-axis.
  - `f-UP` / `f-DOWN`: Rotates around the y-axis.
  - `g-UP` / `g-DOWN`: Rotates around the z-axis.
  - `+` / `-`: Zooms the viewport in and out.
  - `5`: Resets to the default home orientation.
- **Fast point caching**: Evaluated 3D vertices cache in a temporary 2 KB block, allowing instant viewport rotation without re-evaluating the user program.

![3D Interactive Wireframe Demo](/uploads/8886c2297a4062a08e110a4434d8c479/pg-attach-2-3d-cube-400x240.gif)
*3D saddle z = x² - y² evaluated on a 24×24 mesh inside a volume cube, demonstrating real-time interactive rotation and zooming.*

## Architecture and resource impact

- **Display writing**: Primitives write directly to `lcd_buffer`.
- **Throttling**: Screen flushes throttle to at most every 40 ms while `PGM_RUNNING` (`pgRefreshMaybe()`), keeping computational loops fast while animating smoothly.
- **State safety**: `pgBeforeSave()` cleanly closes the view before calculator state serialization, ensuring mode 21 never leaks into backup files.
- **Flash footprint**: +11.2 KB firmware.
- **Static RAM**: 156 bytes.
- **Dynamic memory**: 0 pool bytes in 2D mode. 3D mode borrows a 2 KB block from the memory pool while active and releases it immediately upon closing or reset.

## File changes

### New files added
- `src/c47/pgmGraphics.h`: Public canvas API, state structures, and test driver declarations.
- `src/c47/pgmGraphics.c`: 2D raster kernel, 3D projection engine, coordinate mapping, and interactive navigation handlers.
- `src/c47/testSuite/tests/program_graphics.txt`: Headless test battery.

### Upstream files modified
- `src/c47/items.h` & `src/c47/items.c`: Registers item IDs and tokens for all 23 commands and menus.
- `src/c47/calcMode.c`: Adds `CM_GRAPHICS_CANVAS` (mode 21) handling.
- `src/c47/keyboard.c`: Routes navigation keys, function keys, and shifts in the canvas mode.
- `src/c47/screen.h` & `src/c47/screen.c`: Integrates canvas refresh routines and suppresses stack repainting during canvas mode.
- `src/c47/softmenus.c`: Adds the `CANVAS` softmenu under `PFN`.
- `src/c47/saveRestoreBackup.c`: Hooks `pgBeforeSave()` before state export.

## Verification

- Built and verified against `r47forth` base commit `af7ad934a`.
- All headless test batteries pass with 100% assertions green:
  - Standalone: 13,024 assertions passing.
  - Composed 5-package build (`forth-core`, `undo-history`, `pretty-print`, `pretty-print-extra`, `program-graphics`): 13,050 assertions passing.
- Memory leak checks: 0 blocks leaked across repeated open, rotate, zoom, and reset cycles.
