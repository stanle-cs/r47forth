MODEL: GPT-5

I found two definite boundary defects and one probable retained-line defect. None of the shown code establishes a crash, stuck state, memory leak, or loss of the user’s program.

## Findings

### 1. A retained line containing a hole byte is redrawn as a real line

Where: `pg3dDrawRecord`, at the six unconditional `pg3dDecode(rec[n], ...)` calls.

Concrete input: establish a finite current point, then execute `LINE3D` with an endpoint containing a real NaN or infinity, for example X = NaN, Y = 0, Z = 0. `pg3dEncode(NaN, -1, 1)` returns 255. On redraw:

```text
pg3dDecode(255, -1, 1)
= -1 + 255 × (2 / 254)
= 1.0078740157…
```

Thus byte 255 becomes a finite coordinate slightly beyond the high face. Unlike the grid path, `pg3dDrawRecord` never recognizes `PG3D_HOLE`.

Observable consequence: after a rotation or zoom redraw, a line whose endpoint was a hole can appear as a phantom line ending just outside the volume. Its redraw can also differ from its original one-shot rendering.

Violated contract: “255 marks a hole (NaN or infinite).”

Confidence: medium. The decoding defect is definite, but the supplied packet omits the PT3D/LINE3D argument-reading and record-writing code. If that code rejects every nonfinite coordinate before calling `pg3dEncode`, the malformed record is unreachable.

### 2. A point exactly one 1024th of the volume depth in front of the eye is incorrectly rejected

Where: `pg3dProject`, at:

```c
if(!(dy > s->eps)) return false;
```

Concrete input:

```text
YVOL = [0, 1024]
EYEPT y = -1
zoomStep = 0
point y = 0
```

Then:

```text
eyeYz = 0 - (0 - (-1)) / 1 = -1
eps    = (1024 - 0) / 1024 = 1
dy     = 0 - (-1) = 1
```

Because `1 > 1` is false, the point is rejected. For example, a 2×2 WIREFRAME of constant z = 0 loses both first-row samples, that row’s horizontal edge, and both vertical edges touching it.

Observable consequence: valid boundary geometry disappears.

Violated contract: “a point … nearer than one 1024th of the volume depth is not drawable.” Exactly one 1024th is not nearer.

Confidence: high.

### 3. Vertical projection can pass 32239 to the line kernel despite the ±32000 clamp

Where: `pg3dProject`, at:

```c
*row = SCREEN_HEIGHT - 1 - pg3dRound(...);
```

The window-space ordinate is clamped before the `239 - ordinate` transformation, rather than the final row being clamped.

Concrete retained example:

```text
XVOL = [-2, 2]
YVOL = [-1, 1]
ZVOL = [-2, 2]
EYEPT = (0, -3, 0)
XRNG = [0, 399/40000]
YRNG = [-239/40239, 0]
```

A retained line from `(0, -1, 0)` to `(2, -1, -2)` decodes exactly at those volume coordinates. Projection gives:

```text
first point:
u = 0
v = 0
mapped y = 239
row = 239 - 239 = 0

second point:
u = 1
v = -1
mapped x = 40000  → pg3dRound = 32000
mapped y = -40000 → pg3dRound = -32000
row = 239 - (-32000) = 32239
```

The kernel therefore receives `(32000, 32239)`, outside the promised limit. If the final row were clamped, it would receive `(32000, 32000)`. Against a line starting at `(0,0)`, the bottom-edge intersection changes from approximately:

```text
x = 32000 × 239 / 32239 = 237.23
```

to:

```text
x = 32000 × 239 / 32000 = 239
```

Observable consequence: an extreme clipped line can meet the screen boundary roughly two columns away from the contractually specified result.

Violated contract: “a result is clamped to plus or minus 32000 before the kernel clips.”

Confidence: high.

## Material gaps

The shown packet cannot settle the previously observed equation-parser corruption. `pg3dEngineEnter`, `pg3dEngineLeave`, `pg3dRunGrid`, `pg3dEngineSave_t`, `pg3dEmpty`, and the actual line-record writer are absent. Those are precisely the sites needed to determine whether a saved register, undo image, or pool block survives incorrectly.

As printed, the packet also defines the anonymous `pgWindow` object twice. Literal C source would fail compilation because the two anonymous structure types conflict. The passing test pins prove that the compiled source cannot literally contain both definitions, so I treated this as a packet duplication rather than a runtime finding.

## Examined and correct

- `pg3dEncode(253.5, 0, 254)` computes `t = 253.5`, adds `0.5`, and returns 254: correct half-up behavior.
- `pg3dEncode(-0.0, 0, 1)` reaches `t <= 0` and returns 0; `pg3dRound(-0.0)` returns 0.
- With `lo == hi` and `v == lo`, encoding forms `0 × infinity`, then converts NaN to `uint8_t`, which is undefined; however, no supplied valid path can create equal `zRecLo/zRecHi`.
- Initial recorded z bounds are ordered by `zlo < zhi`; zoom reruns require `zNewLo < zNewHi` and install them only after success, so equal or reversed recorded bounds are not reachable in the shown logic.
- Reversed decode bounds are algebraically consistent: with `lo=1`, `hi=-1`, bytes 0, 127, 254 decode to 1, 0, -1.
- `pg3dFreeBytes` safely saturates: `255×255 = 65025 > 1984`, so it returns 0 without unsigned underflow.
- A 44×44 grid occupies offsets 64–1999; its eight fitting records occupy 2000–2047, with no overlap.
- A 24×24 grid occupies offsets 64–639; 234 records occupy 644–2047, leaving offsets 640–643 free. Record 235 would begin at 638 and therefore must not be retained.
- After `pg3dEmpty` clears `frozen`, the next record copies and freezes the then-current volume and eye, as required.
- At x rotation step 9, `sin=1` and `cos=0`; `(x,y,z)` becomes `(x,-z,y)` about the center, matching a 90° x rotation with the eye left fixed.
- A mirrored Y window works numerically: for `[1,-1]`, `wys=-119.5`, computed `wymax=-1`, and the visible endpoints are subsequently ordered by the swap.
- A descending X window similarly produces a negative `wxs` and a mirror; there is no division fault merely because `xmax < xmin`.
- `pg3dRound(32000.4)` returns 32000; `pg3dRound(-32000.6)` returns -32000; NaN is caught by the first comparison and also becomes -32000.
- On mesh row zero, stale `prev` entries are harmless because the vertical-edge test is guarded by `j > 0`.
- `numX == 1` and `numY == 1` would cause invalid redraw assumptions, including division by zero, but they are not reachable through the stated count validation, reset, allocation, and zero-initialization paths.
- An eye above the volume cannot reach zoom rerun through a valid frozen header because recording requires `eyeY < ylo`.
- At `zoomStep == 8`, ADD returns unchanged; at `-8`, SUB returns unchanged.
- For angle 35, the cosine index is `(35+9)%36 = 8`, giving `sin(80°) = 0.984807753`, the correct cosine of 350°.
- Arithmetic operands undergo integer promotion before `% 36`; for example DOWN from zero computes `(0+35)%36 = 35`, without `uint8_t` wrap.