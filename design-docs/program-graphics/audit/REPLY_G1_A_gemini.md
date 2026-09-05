MODEL: Gemini 3.1 Pro (High)

Here is the audit of the program-graphics package, focusing on the key resolution and event handling within `CM_GRAPHICS_CANVAS`. 

### Findings

**1. ENTER, BACKSPACE, UP, DOWN, and .d bypass the canvas guard and act underneath the view**
*   **Where**: `keyboard.c`, `processKeyAction`
*   **Reaching Input**: The user presses `ENTER`, `BACKSPACE`, `UP`, `DOWN`, or `.d` while the canvas view is open.
*   **Consequence**: According to the orientation, these specific keys have an early `case` in the `processKeyAction` switch that bypasses the `calcMode == CM_GRAPHICS_CANVAS` guard arm entirely. As a result, their keypresses are recorded, and on release, they execute their native, unpatched key functions (e.g., `fnKeyEnter`). If these native functions contain a `calcMode` switch with a default bug screen (like upstream `fnKeyExit` does), pressing them will crash the calculator and cause the user to lose their unsaved program. Otherwise, they will silently mutate calculator state underneath the canvas (e.g., `ENTER` duplicating the X register). 
*   **Violated Contract**: "every other key does nothing." and "The worst outcome of any bug here is that the calculator reboots and the owner loses the program they were typing."
*   **Confidence**: High.

**2. Shift keys (f and g) are ignored, making SNAP (and all other shifted keys) impossible to press**
*   **Where**: `keyboard.c`, `determineItem`
*   **Reaching Input**: The user attempts to press a shifted key while the canvas is open, such as `f` then `SNAP` (`+/-`).
*   **Consequence**: At the top of `determineItem`, the `if` block responsible for delegating to `commonShiftProcessing` checks a hardcoded list of modes (`if(calcMode == CM_NORMAL || calcMode == CM_AIM || ...)`). `CM_GRAPHICS_CANVAS` (21) is missing from this list. As a result, the `f` and `g` keys fall down the resolution chain and return as `ITM_SHIFTf` and `ITM_SHIFTg` items. These items hit the canvas guard arm in `processKeyAction` and are swallowed. Because the shift state is never engaged, it is completely impossible for the user to trigger `ITM_SNAP` or any other shifted key.
*   **Violated Contract**: "SNAP falls through to its own arm below" (implying the user should be able to trigger it) and "can a key act... fail to reach the view when it must? ... (4) the shift keys f and g".
*   **Confidence**: High.

**3. An active shift state breaks R/S and EXIT, trapping the user in an inescapable stuck state**
*   **Where**: `keyboard.c`, `determineItem` and `processKeyAction`
*   **Reaching Input**: A shift state (`shiftF` or `shiftG`) becomes active before the canvas opens and the program halts (e.g., the user presses `f` during a normal `PAUSE`). The user then attempts to continue the program with `R/S` or close the view with `EXIT`.
*   **Consequence**: Because the shift state is active, the package's arm in `determineItem` resolves the keystrokes to their shifted variants (e.g., `result = shiftF ? key->fShifted...`). `R/S` resolves to an item like `ITM_PRGM` instead of `ITM_RS`. The guard arm in `processKeyAction` strictly checks `if(item == ITM_RS)`; it will swallow the shifted `R/S` item, failing to continue the program. Similarly, a shifted `EXIT` resolves to `ITM_OFF`, which misses the early `ITM_EXIT` bypass, hits the canvas guard, and is also swallowed, failing to close the view. Since `f` and `g` presses are ignored in this mode (Finding 2), the user cannot clear the shift state to fix this. They are permanently stranded and must pinhole-reset the calculator.
*   **Violated Contract**: "inside the canvas view R/S continues the program", "EXIT in the canvas view: restore the previous mode", and "stuck states".
*   **Confidence**: High.

---

### What I considered and deliberately did not flag:

*   **Dangling `canvas.region` when upstream forces `CM_NORMAL`**: The orientation notes upstream directly assigns `calcMode = CM_NORMAL;` in many places, which would bypass `pgCloseView` and leave `canvas.region` at 2 or 6. This is benign. `fnPview` and `fnErase` overwrite the region safely when called, and `pgRefreshCanvasView` is guarded by `calcMode == 21`, so it won't mistakenly paint the softmenu over a normal screen.
*   **`screenHoldsDrawnPixels` being cleared by `refreshScreen`**: `fnPview` sets this flag to `true` to tell `SNAP` to skip its pre-refresh. However, when the program halts, `stopProgram:` calls `refreshScreen(4)`, which clears the flag on entry. When the user presses `SNAP`, the flag is `false`, so `SNAP` performs a pre-refresh anyway. This is harmless because the pre-refresh simply calls `pgRefreshCanvasView`, which safely repaints only the status bar and softmenu, leaving the canvas pixels intact for the capture.
*   **Bug screen in `determineItem` in the solo build**: I evaluated if the lack of forth-core's patch causes mode 21 to fall into the `default` bug screen. It does not. In the solo build, execution cleanly falls through to the package's own `else if(calcMode >= 20 && calcMode <= 23)` arm, successfully resolving the `key->primary` item.
