MODEL: Gemini 3.1 Pro (High)

### Findings

**1. Double-free memory corruption via global undo manipulation (Critical / Crash)**
* **Where**: `pg3dEngineEnter` and `pg3dEngineLeave` (inside `fnWireframe`)
* **Concrete reaching input**: Execute `WIREFRAME` when there is an existing undo state (`thereIsSomethingToUndo == true`), such as during a test suite run or after a manual stack operation.
* **Observable consequence**: `fnWireframe` improperly uses the global `saveForUndo()` to save the stack. At the end, `fnUndo(0)` consumes the undo image by transferring its pointers back to the active stack, and clears the undo flag. `pg3dEngineLeave` then forces `thereIsSomethingToUndo = sv.undo` (true), re-activating the consumed undo image. The undo image now holds dangling pointers to memory blocks owned by the active stack. When the active stack is later modified, those blocks are freed. A subsequent command triggering `saveForUndo()` attempts to free the active undo image's contents, double-freeing those blocks. This corrupts the pool allocator, causing a later equation parse to allocate a corrupted block and crash with a syntax error (reading garbage). 
* **Violated contract**: "name a mechanism in this code that leaves the pool, a saved register, or the undo image in a state a later equation parse can trip on" and "the earlier undo point stays valid".
* **Confidence**: High

**2. `ERASE` during `WIREFRAME` leaves a corrupted valid grid (Medium)**
* **Where**: `pg3dRunGrid` and the end of `fnWireframe`
* **Concrete reaching input**: A user program passed to `WIREFRAME` calls `ERASE`, followed by a 3D command that sets the view (like `PT3D` or `LINE3D`). 
* **Observable consequence**: `ERASE` zeroes the block header (via `pg3dEmpty`), setting `numX` and `numY` to 0. This causes `pg3dRunGrid` to set its inner `retain` flag to false, halting grid writes. However, the subsequent 3D command sets `h->frozen = 1`. Upon completion, `fnWireframe` checks its outer `retain` local variable (which is still true) and `h->frozen` (which is 1), and executes `h->gridValid = 1`. This leaves a retained block incorrectly marked as a valid grid, but with 0x0 dimensions.
* **Violated contract**: "A grid that does not fit draws once and is not retained" (a valid retained grid should reflect the completed grid with valid dimensions).
* **Confidence**: High

***

### Audited Sites (Found Correct)

* **Body ends in STOP**: Correct. `programRunStop` becomes `PGM_WAITING`, `pg3dRunGrid` aborts cleanly, and the outer program halts.
* **Raises an error at some points**: Correct. `pg3dSample` records the error, clears it to continue the run, and safely encodes the resulting NaN as a hole.
* **Leaves a string in X**: Correct. `fnToReal` raises a type error which is caught, turned into a hole, and cleared.
* **Calls WIREFRAME (nesting)**: Correct. `engineNestingRefused(true)` cleanly refuses the nested call, returning early and aborting the outer run with `ERROR_NESTING_TOO_DEEP`.
* **User presses EXIT**: Correct. `exitKeyWaiting()` triggers the abort test, halting the grid loop cleanly before the system closes the view.
* **Rows buffer when body resets calculator**: Correct. The `pg3dResetCount == sv.resets` check successfully prevents freeing the `rows` pointer into a rebuilt, dead pool.
* **pgReset from config hook at boot**: Correct. `pg3d.block` is nulled without freeing, which is the correct behavior since the entire pool is rebuilt at boot.
