MODEL: GPT-5

The principal defect is that the canvas view cannot be closed with EXIT. A second, independent defect allows upstream mode changes to abandon the view without closing it, after which an ordinary repaint destroys the drawing.

## Findings

### 1. EXIT never closes the canvas view — high severity

**Where:** `fnKeyExit`, `case CM_GRAPHICS_CANVAS`.

**Reaching input:** Open the view with `PVIEW 2`, `PVIEW 6`, or `ERASE`, then press and release EXIT.

**What happens:** EXIT has its own earlier key-processing case, so it never reaches the general canvas guard. On release, `fnKeyExit` executes, but its canvas case only does:

```c
case CM_GRAPHICS_CANVAS: {
  break;
}
```

It never calls `pgCloseView()`. The tail merely changes refresh bookkeeping and returns. Consequently:

- `calcMode` remains 21.
- `canvas.region` remains 2 or 6.
- `canvas.prevCalcMode` remains saved.
- The drawing remains displayed.
- Repeated EXIT presses do nothing.
- Softkeys remain disabled, and ordinary direct keys are intended to be suppressed.
- The owner can continue a program with R/S, but cannot leave the view through its documented exit path. If no runnable program provides another route, rebooting is the practical escape.

This also makes the sequence “EXIT, then PVIEW again” not an actual close/reopen. If another PVIEW is eventually executed, it runs while `calcMode == 21`, preserves the old `prevCalcMode`, and clears/reconfigures the existing canvas.

**Violated contract:**  
“`EXIT in the canvas view: restore the previous mode and repaint (DESIGN.md §3.6).`”

**Confidence:** Very high. The key routing and complete canvas arm of `fnKeyExit` are provided, while no call to `pgCloseView` exists in the package’s non-test functions.

---

### 2. Upstream mode changes abandon the canvas and can erase the drawing — high severity

**Where:** The split state formed by global `calcMode` and `canvas.region`, together with the 26 direct `calcMode = CM_NORMAL` sites and 18 `calcModeNormal()` callers.

**Reaching call path:** Open and draw in the canvas, then let any such upstream path execute while `calcMode == 21`:

```text
PVIEW → drawing → upstream calcMode = CM_NORMAL → normal refresh
```

**What happens:**

1. The upstream site changes only `calcMode`.
2. `canvas.region` remains 2 or 6, even though its documented meaning says a nonzero value means the view is open.
3. The pixels initially remain visible because assigning the variable does not itself repaint.
4. At the next normal refresh, `_refreshNormalScreen()` repaints the register lines. That destroys the drawing in rows 20–170; normal menu painting can replace the lower band as well.
5. `pgCloseView()` cannot repair the state because it returns immediately when `calcMode != 21`.
6. EXIT now follows the upstream mode’s behavior rather than restoring `canvas.prevCalcMode`.

Subsequent operations behave as follows:

- **Next PVIEW:** sees `calcMode != 21`, overwrites `prevCalcMode` with the current mode, clears the requested region, and re-enters successfully.
- **Next ERASE:** treats the view as closed, calls `fnPview(2)`, clears region 2, and re-enters successfully.
- **Next EXIT:** is handled as CM_NORMAL and does not clear the stale `canvas.region`.
- Thus the owner can re-enter, but the previous drawing is unrecoverable and the logical view state was never closed coherently.

This is also a composition defect: every upstream mode-changing site must know not to run during this package’s view, but nothing enforces that agreement.

**Violated contracts:**

- `region: “0 = view closed, else 2 or 6”`
- “`the canvas keeps its pixels`”
- “`pgCloseView`: restore the previous mode and repaint”

**Reachability gap:** The packet establishes exactly what happens *if* one of these sites runs, but does not include a specific site’s enclosing function and input path. Therefore I cannot identify which ordinary key sequence reaches such a site while the canvas is open. Finding 3 makes accidental upstream key-function execution structurally possible, but its omitted bodies prevent connecting a particular key to a particular assignment.

**Confidence:** High for the state corruption and drawing loss once such a site runs; medium for normal-user reachability from this packet alone.

---

### 3. The “every other key does nothing” guard does not cover several direct keys — medium design risk, bounded impact unknown

**Where:** `processKeyAction` canvas guard.

**Reaching input:** While the canvas is open, press ENTER, BACKSPACE, UP, DOWN, or `.d`.

**What happens:** These keys have earlier switch cases. They do not reach:

```c
else if(calcMode == CM_GRAPHICS_CANVAS && item != ITM_SNAP)
```

Their unchanged upstream item functions execute on release. Therefore the claim that every key except R/S and SNAP does nothing is false at the routing level. Those upstream functions were not designed for calcMode 21.

The concrete observable result of each function cannot be established from the packet: their bodies and calcMode handling are absent. Possible consequences such as switching to CM_NORMAL, displaying a bug screen, editing state, or truly doing nothing would be guesses.

EXIT is also outside the guard, but its definite consequence is covered separately in finding 1.

**Violated contract:**  
“`inside the canvas view R/S continues the program and every other key does nothing. SNAP falls through...`”

**Confidence:** Very high that the functions execute; indeterminate regarding their subsequent functional effects.

## Requested path walk

| Path | Screen afterward | Remaining state |
|---|---|---|
| `PVIEW`, draw, `STOP` at level 0 | Drawing remains. The halt refresh dispatches to the canvas case: status bar is repainted, and region 2’s softmenu is cleared/repainted. Register pixels are not repainted. | `calcMode=21`; region unchanged; program stopped. Stop control explicitly makes `screenUpdatingMode=SCRUPD_AUTO`, so the tail refresh runs. `screenHoldsDrawnPixels` is cleared by `refreshScreen`, although the pixels remain. |
| Same, `STOP` at level 1 | Same retained drawing and canvas refresh. | The level-1 post-dispatch block does not assign AUTO, but the completed preceding step left `AUTO | SKIP_STATUSBAR_ONE_TIME`; the halt tail clears that skip bit, leaving AUTO and permitting the refresh. |
| Fall off `.END.` | Drawing remains; canvas halt refresh paints only status/menu as above. | After the preceding completed step, the loop has installed `AUTO | SKIP_STATUSBAR_ONE_TIME`; the halt tail removes the skip bit and refreshes. This assumes “fall off” reaches the shown `stopProgram` tail normally, as the question states; the actual end-detection branch is not included. |
| PAUSE, no R/S abort | The drawing remains. The provided end-of-PAUSE code does not refresh when execution returns to `PGM_RUNNING`; the program then continues. | `calcMode=21`, region unchanged. The earlier PAUSE body is missing, so what is painted *during* the timed pause cannot be determined. |
| PAUSE with R/S abort | On the stipulated abort, PAUSE’s non-running branch calls `refreshScreen(13)` and then an LCD refresh. The canvas dispatcher preserves the drawing and refreshes status/menu. | Program remains non-running; `calcMode=21`, region unchanged. R/S is correctly routed through the canvas press/release handling. |
| VIEW inside the view | The requested register is not shown; the existing drawing remains. Status and, for region 2, menu are refreshed. This non-display is a documented limit. | `temporaryInformation=TI_VIEW_REGISTER`; `currentViewRegister` is set. At halt, the run tail adds `SCRUPD_SKIP_STACK_ONE_TIME`, preventing its final refresh because the mask is no longer exactly AUTO. R/S or `pgCloseView` would clear the temporary indication, but normal EXIT does not close the view. |
| Drawing command raises an error | The drawing already on screen remains; the program stops in canvas mode. If the halt reaches `refreshScreen` with AUTO, only status/menu are repainted. | `lastErrorCode` is set; `calcMode=21`, region unchanged. The exact error-message pixels and any screen-mode changes made by `displayCalcErrorMessage` cannot be established because its body is absent. |
| Keyboard PVIEW with `calcMode=CM_NIM` at the call | The requested region is cleared and displayed as a canvas. Region 2 also gets a fresh softmenu. | `prevCalcMode=CM_NIM`, `calcMode=21`, `temporaryInformation=TI_NO_INFO`. If `pgCloseView` were called directly, it would restore CM_NIM as specified. Normal EXIT does not call it. There is a packet-level tension here: orientation also says a function key closes NIM, but the requested path stipulates CM_NIM at the PVIEW call; I used the stipulated call state. |
| ERASE while closed | Opens region 2 and clears rows 20–170; softmenu is repainted. | Saves the current mode, enters mode 21, region 2. |
| ERASE while open | Clears the current region. For region 2, it then repaints the menu; for region 6, rows 20–239 stay blank. | Remains in mode 21 with the same region and saved previous mode. |
| EXIT, then PVIEW again | EXIT leaves the first canvas unchanged. A subsequently executed PVIEW clears the newly requested region rather than reopening from a closed state. | Remains mode 21 throughout; `prevCalcMode` is not updated by the second PVIEW. |
| Upstream assignment to CM_NORMAL | Pixels remain momentarily, then a normal refresh overwrites the drawing. | `calcMode=CM_NORMAL` with stale nonzero `canvas.region`; detailed in finding 2. |
| Two PVIEW calls with different regions | The second call intentionally clears its new region. `2→6` clears all rows 20–239. `6→2` clears rows 20–170 and menu painting clears/replaces rows 171–239, so none of the former full-region drawing survives. | Mode remains 21; `prevCalcMode` remains the value saved by the first PVIEW; region changes to the second argument. This follows the stated “sets the region … and clears the region” contract, so it is not a defect. |

## Considered and deliberately not flagged

- The per-step reset of `screenUpdatingMode` does not lose the drawing: while mode 21 remains active, all final refreshes dispatch to `pgRefreshCanvasView`, which never repaints register pixels.
- STOP at subroutine level 1 still reaches an AUTO mask under the shown sequence; the missing level-0 assignment alone does not make the view stick.
- `screenHoldsDrawnPixels` being cleared by a canvas refresh does not itself erase anything. SNAP’s resulting pre-refresh also dispatches through the canvas case while mode 21 remains active.
- VIEW/AVIEW not displaying their register is explicitly documented as a limit.
- Repeated PVIEW and ERASE destroy pixels intentionally because their contract says they clear the selected region.
- Restoring CM_NIM via `prevCalcMode` is unusual but matches the stated “restore the previous mode” contract; the packet does not establish corrupted NIM auxiliary state.
- The redundant `calcMode != CM_REGISTER_BROWSER` conjunct in an arm already requiring `calcMode == CM_NORMAL` is dead logic, but it has no functional consequence for the drawing/view question.
- Sleep/wake persistence, region 1, DMA verification, and future drawing primitives were excluded explicitly.