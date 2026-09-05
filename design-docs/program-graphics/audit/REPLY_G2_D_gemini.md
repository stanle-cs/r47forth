MODEL: Gemini 3.1 Pro (High)

Here is the audit of the `program-graphics` kernel.

### Findings

**1. Out-of-bounds read and potential crash in `TEXTOUT` string truncation**
*   **Where:** `pgStringCut`, line 286 (`i += ((uint8_t)tmpString[i] & 0x80) ? 2 : 1;`)
*   **Reaching input:** `TEXTOUT` with a string longer than `TMP_STR_LENGTH - 1` bytes, where the byte at index `TMP_STR_LENGTH - 3` is the start of a two-byte glyph (i.e., $\ge$ 0x80).
*   **Observable consequence:** The initial `memcpy` caps the copy at `TMP_STR_LENGTH - 2`, placing a NUL terminator there and cleanly slicing the two-byte glyph in half. When the inner `while` loop reaches this severed byte, it evaluates the `>= 0x80` condition as true, increments `i` by 2, and cleanly jumps *over* the NUL terminator. The loop proceeds to read uninitialized or out-of-bounds memory looking for the next NUL, which will hang the calculator or trigger a hardware fault.
*   **Violated contract:** "The worst outcome of any bug here is that the calculator reboots" and "TEXTOUT ... cuts the string".
*   **Confidence:** High.

**2. Global `tmpString` corruption crashes the execution engine**
*   **Where:** `pgStringCut`, line 278 (`memcpy(tmpString, s, n);`)
*   **Reaching input:** Any successful execution of `TEXTOUT` or `DISP`.
*   **Observable consequence:** `TEXTOUT` and `DISP` destructively overwrite the global `tmpString`. Because the caller of this package (the upstream calculator execution engine) shares this exact scratch buffer, if the engine relies on `tmpString` to retain context across command dispatches (such as storing the currently executing program line or expression), the drawing command will irrevocably corrupt it, causing the program runner to crash or enter a stuck state upon return.
*   **Violated contract:** "Audit for FUNCTIONAL correctness: ... stuck states, crashes." (Implied directly by the explicit requirement to evaluate "the shared scratch buffer tmpString being overwritten by a caller that still needs it").
*   **Confidence:** High.

**3. Integer overflow in `FCIRCL` collapses massive circles**
*   **Where:** `pgCircle` (filled variant), line 112 (`int32_t w = (pgIsqrt(4 * (r * r - dy * dy)) + 1) / 2;`)
*   **Reaching input:** `FCIRCL` with a mathematically valid radius $\ge$ 23,171.
*   **Observable consequence:** The argument `4 * (r * r - dy * dy)` overflows the signed `int32_t` boundary (since $4 \times 23171^2 > 2^{31}-1$) and becomes a negative number. `pgIsqrt` correctly spots a negative input and immediately returns 0. Instead of filling the screen, the enormous filled circle silently renders as a single 1-pixel-wide vertical line down the center.
*   **Violated contract:** "a coordinate is a long integer (pixel, magnitude up to 32767)... an off-screen point is not an error, it is clipped".
*   **Confidence:** High.

**4. `DISP` violates horizontal clipping boundaries**
*   **Where:** `fnGdisp`, line 316 (`lcd_fill_rect(0, (uint32_t)row, SCREEN_WIDTH, 20, LCD_SET_VALUE);` and the subsequent `showString` call).
*   **Reaching input:** The program sets a narrow horizontal clip via `GCLIP` (e.g., from X=100 to Z=200), and then calls `DISP 1`.
*   **Observable consequence:** `DISP` wholly ignores `canvas.clipX0` and `canvas.clipX1`. `lcd_fill_rect` clears the entire 400-pixel width of the row to white, destructively erasing any existing graphics outside the clip region. `showString` compounds this by unconditionally starting at $X=1$.
*   **Violated contract:** "canvas.clipX0..clipY1 is the clip rectangle ... set by ... GCLIP."
*   **Confidence:** High.

**5. `errorShown` state desynchronization**
*   **Where:** `pgError`, line 178.
*   **Reaching input:** Any command supplied with an invalid argument, such as `GLINE` with a string.
*   **Observable consequence:** `pgError` calls `displayCalcErrorMessage`, which physically paints the error message onto canvas line 1 and halts the program. However, `pgError` fails to update `canvas.errorShown = 1`. The package's internal state desynchronizes from the physical screen, meaning subsequent logic or view hooks will not know an error is currently occupying line 1.
*   **Violated contract:** `errorShown` comment explicitly states: "1 while an error message is painted on canvas line 1".
*   **Confidence:** High.

### Considered and deliberately did not flag

*   **`TEXTOUT` with a point on the right edge of the clip:** Did not flag. If $x == c.x1$, the computed string width budget is 1. The loop correctly chops the string down until it is entirely empty, safely drawing nothing.
*   **`GCLIP` with a rectangle wholly outside the region:** Did not flag. This creates an inverted clip rectangle (e.g., `clipX0 > clipX1`). The primitives (`pgRun`, `pgPixel`, `TEXTOUT`) elegantly handle inverted bounds by strictly rejecting all pixels, meaning it safely behaves as an empty clip region.
*   **`DISP` for lines 8 to 11 in Region 2:** Did not flag. Because the upstream `showString` lacks vertical clipping, dropping the intersecting line entirely via `if(row + 19 > c.y1) return;` is a functionally correct, defensive choice to prevent drawing off-screen.
*   **`DISP` with the view closed:** Did not flag. Drawing outside the canvas view is explicitly intended behavior; `pgClipNow` purposefully sets the clip to the full 400x240 screen when `calcMode != CM_GRAPHICS_CANVAS`. Overwriting the standard calculator UI in the background is the designed behavior.
*   **`pgReadCoord` when maximum data length is 0:** Did not flag. The `for` loops safely bypass execution when `bytes == 0`, defaulting `low` to 0. This correctly yields a 0 coordinate.
*   **Mixed angle types in `ARC`:** Did not flag. `pgReadAngle` safely unboxes both `dtLongInteger` and `dtReal34` to a local `real_t` without cross-contamination.
*   **Refresh cadence uptime wrapping:** Did not flag. Unsigned 32-bit arithmetic inherently handles the wrap-around correctly; `0x00000005 - 0xFFFFFFF0` securely evaluates to the correct delta of `21`.
