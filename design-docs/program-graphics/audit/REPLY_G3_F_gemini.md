MODEL: Gemini 3.1 Pro (High)

### Findings

**1. `ARC` command draws at the wrong vertical position**
*   **Where:** `fnGarc`, at the call to `pgArc(&c, cx, cy, r, ax, ay, bx, by, wide);`
*   **Concrete reaching input:** The `ARC` command with a valid complex center in `T` (e.g., `(200, 100)`), and valid radii/angles in `Z`, `Y`, and `X`. 
*   **Observable consequence:** The arc is drawn flipped vertically. The function passes `cy` directly to `pgArc`, which is the y-coordinate from the bottom of the screen (user space), but the drawing primitive expects a top-down screen row index. (Notice how `pgCircle` and `pgPixel` in the very same function correctly apply `PG_ROW_OF(cy)`).
*   **Violated contract:** "The drawing primitives (pgLine, pgBox, pgCircle, the arc stepper, pgStringCut, showString) ... take int32 screen coordinates"
*   **Confidence:** High. `cy` is clearly a user-space coordinate (origin bottom-left), and skipping `PG_ROW_OF` sends it directly to a primitive requiring screen coordinates.

**2. Type errors in coordinate reading falsely blame `REGISTER_X`**
*   **Where:** `pgReadCoordAxis` (the `default:` case), which calls `pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);`
*   **Concrete reaching input:** A two-point command (like `LINE`) executed with reals in `REGISTER_X` and `REGISTER_Y`, but a complex number in `REGISTER_Z` or `REGISTER_T`.
*   **Observable consequence:** The program halts with a type error, but the calculator UI highlights `REGISTER_X` as the source of the invalid data, even though the offending complex number was in `REGISTER_Z` or `REGISTER_T`. 
*   **Violated contract:** "A complex in X or Y without a complex in the other is ERROR_INVALID_DATA_TYPE_FOR_OP." (Implicitly, the error should point the user to the register actually causing the type violation, but `pgError` hardcodes `displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X);`).
*   **Confidence:** High. The error handler `pgError` lacks a parameter to specify the register, blindly attributing all coordinate type faults to `REGISTER_X`.

***

### Sites Examined and Found Correct

*   **Type matrix of `pgReadTwoPoints`:** Correctly reads complex point pairs or falls back to coordinate-by-coordinate reading, rejecting invalid mix-and-matches.
*   **`CIRCLE` / `FCIRCL` with a real center and radius:** Correctly maps the center through the window while rounding the radius natively to pixels via `PG_AXIS_NONE`.
*   **`ARC` picture under a non-square window:** Correct by design; it draws a circular arc in pixel space (which looks like an ellipse in user space) because the radius is intentionally never mapped through the window.
*   **`TEXTOUT` with a real point:** Correctly maps to a top-left cell corner; a higher user `y` coordinate algebraically results in a smaller screen row index (higher on screen), which perfectly aligns with the font drawing direction.
*   **`GCLIP` reals mixed with `LINE` long integers:** Correctly processes the unmapped long integers as pixels against the window-mapped reals, accommodating two coordinate systems simultaneously.
*   **`XRNG` / `YRNG` execution contexts:** Correctly execute from the keyboard or running programs unconditionally, without altering or popping the stack.
*   **State of the window on `XRNG` failure:** Correctly leaves the window bounds unchanged when inputs are equal or invalid.
*   **Window persistence through `PVIEW`, `ERASE`, and `EXIT`:** Correctly survives via static variables, until a reset intentionally zeroes the state.
*   **Independent axis ranges:** Correctly skips window mapping for an un-set axis, smoothly falling back to the default pixel rounding logic.
*   **Reset behavior vs. surviving registers:** Correctly disables the window (`pgWindow.set = 0`), meaning surviving registers will be drawn as unmapped pixels upon re-run unless `XRNG`/`YRNG` are explicitly invoked again.
*   **39-digit difference test in `pgRange`:** Correct; the 39-digit subtraction of two 34-digit reals operates without precision loss, successfully guaranteeing that `xmax - xmin != 0` in `pgRealToPixel`.
*   **Identity path rounding half away from zero:** Correct; the shift from G2's truncation is explicitly mandated by the G3 design, and off-screen negative results (like -0.5 becoming -1) are safely neutralized by the inclusive clipping logic.
*   **`ERROR_OUT_OF_RANGE` vs off-screen clipping:** Correct; magnitudes up to 32767 are quietly swallowed by the off-screen clip checks, while values genuinely exceeding 32767 safely trigger the documented error.
*   **Design text vs. `W2` pin:** Correct; the formula computes `120` as the un-flipped output ("pixel"), which `PG_ROW_OF` subsequently flips to row `119` for the LCD buffer, satisfying both the documentation and the pin logic.
