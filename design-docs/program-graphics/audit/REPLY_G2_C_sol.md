MODEL: GPT-5

No—the ordinary pixel, run, line, and small-circle boundaries are correct, but four functional defects remain. The most serious lets a legal `GCLIP` followed by a drawing command write far before `lcd_buffer`.

## Findings

### 1. Critical — `GCLIP` can overflow its stored row bound, enabling out-of-buffer writes

**Where:** `fnGclip`, especially:

```c
if(r1 > regionBottom) r1 = regionBottom;
canvas.clipY0 = (int16_t)r0;
canvas.clipY1 = (int16_t)r1;
```

Only `r1` is clamped downward. When both requested rows lie below the region, `r0` can remain greater than 32767 before being narrowed to `int16_t`.

**Concrete reaching input:**

1. Enter `PVIEW 6`.
2. Execute `GCLIP` with:

   - X = 0
   - Y = −32767
   - Z = 1
   - T = −32766

The converted screen rows are:

- `PG_ROW_OF(-32767) = 239 - (-32767) = 33006`
- `PG_ROW_OF(-32766) = 239 - (-32766) = 33005`

After sorting: `r0 = 33005`, `r1 = 33006`.

For region 6, `regionBottom = 239`, so only `r1` changes:

- `r0 = 33005`
- `r1 = 239`

On the DM42’s ordinary two’s-complement narrowing, storing 33005 in `int16_t` produces:

- `33005 - 65536 = -32531`

The effective clip therefore becomes `(0, -32531)` through `(1, 239)` rather than an empty intersection.

Now execute `FBOX` with:

- X = 0
- Y = 32767
- Z = 1
- T = 239

Its screen rows are:

- `PG_ROW_OF(32767) = 239 - 32767 = -32528`
- `PG_ROW_OF(239) = 0`

`pgBox` iterates from row −32528 through row 0. Those negative rows pass the corrupted clip test, so `pgRun` evaluates:

```c
pgRowPtr(-32528) = lcd_buffer - 32528 * 52
```

and writes far before the framebuffer.

**Observable consequence:** memory corruption, calculator reboot, stuck state, or loss of the program being entered. The exact result depends on the surrounding memory map.

**Violated contracts:**

- “`GCLIP` … keeps it inside the region.”
- “An off-screen point is not an error, it is clipped.”

**Confidence:** High. Conversion of an out-of-range integer to `int16_t` is formally implementation-defined in C, but the stated target’s conventional result directly exposes the write.

---

### 2. High — `pgIsqrt` has signed overflow for valid large filled-circle radii

**Where:** `pgIsqrt`, at:

```c
if((r + bit) * (r + bit) <= v) r += bit;
```

**Concrete reaching input:** `FCIRCL` with:

- X = −25000
- Y = 100
- Z = 23170

At the center scan line, `dy = 0` and the caller computes:

- `r² = 23170² = 536,848,900`
- `4r² = 2,147,395,600`

This is still below `INT32_MAX = 2,147,483,647`, so the caller’s multiplication has not yet overflowed.

Inside `pgIsqrt(2,147,395,600)`:

- Candidate 32768: `32768² = 1,073,741,824`, accepted.
- Next candidate 49152: `49152² = 2,415,919,104`.

The latter exceeds `INT32_MAX`, so the signed multiplication has undefined behavior. With ordinary 32-bit wrapping it becomes −1,879,048,192, is incorrectly considered `<= v`, and the later overflowing candidates are also accepted; the routine commonly returns 65535 instead of the correct 46340.

That gives:

- Erroneous `w = (65535 + 1) / 2 = 32768`
- Correct `w = (46340 + 1) / 2 = 23170`

For the supplied center:

- Correct center run: `[-48170, -1830]`, wholly off-screen.
- Erroneous run: `[-57768, 7768]`, which clamps to columns 0 through 399.

**Observable consequence:** a filled circle that should not touch the display can paint an entire row. Because the operation invokes signed-overflow undefined behavior, compiler-dependent crashes or other corruption are also possible.

There is a second overflow boundary in the caller:

```c
4 * (r * r - dy * dy)
```

For `r = 23171`, `dy = 0`:

- `r² = 536,895,241`
- `4r² = 2,147,580,964 > INT32_MAX`

Thus radii 23171 through 32767 also overflow before `pgIsqrt` is called.

**Violated contract:** “`FCIRCL` … reads the center in X and Y and the radius in Z” and draws the corresponding filled circle; a legal radius up to magnitude 32767 must not produce a different figure or undefined execution.

**Confidence:** High.

---

### 3. Medium — `DISP` ignores the horizontal `GCLIP` bounds and erases pixels outside the clip

**Where:** `fnGdisp`:

```c
if(!pgStringCut(REGISTER_X, SCREEN_WIDTH - 1)) return;
lcd_fill_rect(0, (uint32_t)row, SCREEN_WIDTH, 20, LCD_SET_VALUE);
showString(tmpString, &standardFont, 1, (uint32_t)row, ...);
```

Only the vertical clip is checked. The clear always covers columns 0–399, and the text always starts at column 1.

**Concrete reaching input:**

1. Enter `PVIEW 6`.
2. Draw existing content on screen row 40 outside columns 100–200.
3. Set `GCLIP` with:

   - X = 100, Y = 219 (`PG_ROW_OF(219) = 20`)
   - Z = 200, T = 0 (`PG_ROW_OF(0) = 239`)

4. Put a string in X and execute `DISP 2`.

`DISP 2` selects:

- `row = 20 + 20(2 - 1) = 40`

Although the clip is columns 100–200, `lcd_fill_rect(0, 40, 400, 20, 0)` clears columns 0–399, and text is drawn beginning at column 1.

**Observable consequence:** existing drawing outside the selected clip is erased, and new text is painted outside it.

**Violated contract:** “`canvas.clipX0..clipY1` is the clip rectangle … set … by `GCLIP`.”

Pin D11 used the normal full-width clip and therefore does not cover this case.

**Confidence:** High.

---

### 4. Medium — arc endpoint quantization can collapse a visibly nonzero arc to one pixel

**Where:** `fnGarc`, at the conversion to 1024-scaled integer vectors and the same-direction branch:

```c
realToFloat(&co, &f); ax = (int32_t)(f * 1024.0f);
...
if(cross == 0 && dot > 0) {
  pgPixel(...);
}
```

**Concrete reaching input in degree mode:**

- T = complex(−32368, 100)
- Z = 32767
- Y = 0°
- X = 0.05°

For 0°:

- `A = (1024, 0)`

For 0.05°:

- `sin(0.05°) × 1024 ≈ 0.89361`, truncated to 0
- `cos(0.05°) × 1024 ≈ 1023.99961`, truncated to 1023
- `B = (1023, 0)`

Consequently:

- `cross = 1024×0 - 0×1023 = 0`
- `dot = 1024×1023 = 1,047,552 > 0`

The implementation treats the endpoints as identical and plots only the start point:

- Column: `−32368 + 32767 = 399`
- User y: 100
- Screen row: `239 - 100 = 139`

But the requested endpoint has an upward displacement of:

- `32767 × sin(0.05°) ≈ 28.59` pixels

The intended rasterized arc therefore extends approximately from row 139 toward row 111 near the right edge, rather than being a single pixel.

The angular collapse begins whenever the scaled sine is below 1:

- `sin(δ) × 1024 < 1`
- approximately `|δ| < asin(1/1024) ≈ 0.05595°`

**Observable consequence:** sufficiently large-radius, small-angle arcs lose tens of visible pixels.

**Violated contract:** “`ARC` … draws counterclockwise” from the supplied start angle to the supplied end angle.

**Confidence:** High.

## Boundary arithmetic that is correct

### Mirrored pixel mapping

The mapping is `xm = 399 - x`, data byte `2 + (xm >> 3)`, bit `xm & 7`:

| Column | `xm` | Pixel-byte index | Bit |
|---:|---:|---:|---:|
| 0 | 399 | 49 | 7 |
| 7 | 392 | 49 | 0 |
| 8 | 391 | 48 | 7 |
| 392 | 7 | 0 | 7 |
| 399 | 0 | 0 | 0 |

Including the two header bytes, columns 0 and 399 address row bytes 51 and 2 respectively. This agrees with the stated layout and D10.

### `pgRun` masks

For columns 394–397:

- `a = 399 - 397 = 2`
- `b = 399 - 394 = 5`
- Both are in byte 0.
- Left mask: `0xFF << 2 = 0xFC`
- Right mask: `0xFF >> (7 - 5) = 0x3F`
- Intersection: `0xFC & 0x3F = 0x3C`

Thus exactly bits 2–5 are affected.

For an adjacent-byte run, columns 391–392:

- `a = 399 - 392 = 7`, byte 0, giving `0x80`
- `b = 399 - 391 = 8`, byte 1 bit 0, giving `0x01`

For columns 390–394:

- First byte starts at bit 5: `0xE0`
- Last byte ends at bit 1: `0x03`

These masks are correct for set, clear, and invert modes.

### Run and box clipping

For a clip of columns 10–20:

- Run −5..12 clamps to 10..12.
- Run 18..25 clamps to 18..20.
- Run −5..5 becomes 10..5 and returns.
- Run 25..30 becomes 25..20 and returns.

For vertical clip rows 20–170:

- Filled box rows −10..25 iterates 20..25.
- Rows −10..10 become 20..10 and do not iterate.
- Rows 180..200 become 180..170 and do not iterate.

The outline path likewise sends its horizontal edges and vertical-edge pixels through the clip checks. This is correct while the clip itself is valid; Finding 1 breaks that prerequisite.

### Bresenham line stepper

For `(0,0)` to `(2,5)`:

- `dx = 2`, `dy = -5`, `err = -3`
- Pixels are `(0,0), (0,1), (1,2), (1,3), (2,4), (2,5)`

For the reverse direction `(2,5)` to `(0,0)`:

- `sx = -1`, `sy = -1`
- Pixels are `(2,5), (2,4), (1,3), (1,2), (0,1), (0,0)`

The minor difference in tie placement is normal Bresenham rasterization. Negative directions terminate correctly. A single point `(x,y)` takes the horizontal fast path and calls `pgRun(x,x,y)`, affecting exactly one pixel.

### Small midpoint circles and invert-mode duplicates

- `r = 1`: four distinct cardinal pixels.
- `r = 2`: four cardinal pixels plus eight `(±2,±1)/(±1,±2)` pixels.
- `r = 3`: four cardinal pixels, eight `(±3,±1)/(±1,±3)` pixels, and four diagonal `(±2,±2)` pixels.

The `y == 0` and `x == y` branches suppress duplicate octant plots. Therefore these boundary pixels are inverted once, not twice.

Filled-circle half-widths agree with those outlines:

- `r=1`: widths by `dy=-1,0,1` are `0,1,0`.
- `r=2`: widths for `|dy|=2,1,0` are `0,2,2`.
- `r=3`: widths for `|dy|=3,2,1,0` are `0,2,3,3`.

For example, `r=3, dy=2` gives:

- `4(9-4)=20`
- `isqrt(20)=4`
- `w=(4+1)/2=2`

This meets the outline at `x=±2`. The agreement stops being reliable at the large-radius overflow described in Finding 2.

### Arc span decisions

Ignoring the separately reported 1024-vector quantization:

- 0°→0°: `cross=0`, positive dot; one endpoint.
- 0°→90°: `cross=1,048,576`, narrow; first quadrant.
- 0°→180°: `cross=0`, dot=−1,048,576; `dy>=0`, the upper semicircle.
- 0°→270°: `cross=−1,048,576`, wide; all except the lower-right quadrant.
- 0°→359°: approximately `B=(1023,−17)`, `cross=−17,408`; a wide span excluding only the final roughly 1° wedge.
- A difference of 360° sets `fullCircle`.
- 0°→−90° has absolute raw difference 90°, and correctly represents the 270° counterclockwise route.
- Differences of ±360° or more become full circles.

Equivalent full rotations are 2π radians, 400 grads, and 2 in multiples-of-π mode; quarter rotations are π/2, 100 grads, and 0.5 respectively. The code converts both the span and endpoint angles using `currentAngularMode`, so that part is structurally correct. Exact transcendental rounding from the conversion helper is not supplied, so claims about its last output bit would require upstream implementation details.

### Integer coordinate reader

For an 8- or 16-byte limb area:

- 32767 has low word `0x00007FFF`, with bytes 4–7 or 4–15 zero: accepted.
- 32768 has low word `0x00008000`: rejected because `32768 > 32767`.
- −32768 has the same magnitude word and `LI_NEGATIVE`: rejected before negation.
- A well-formed `LI_ZERO` value has zero limbs and returns 0.
- Any nonzero byte above byte 3 is rejected.

This matches the stated long-integer magnitude limit. Whether unused capacity bytes are guaranteed zero is not stated; without that register-storage invariant, I cannot determine whether scanning maximum allocation length could reject a valid short integer.

For real coordinates:

- 32767.9 is less than 32768 and truncates to 32767.
- −0.5 is greater than −32768 and truncates toward zero to 0.
- `1e30` fails the `< 32768` comparison.
- NaN is explicitly rejected.
- −32768 is excluded because the lower comparison is strict.

### `pgIsqrt` below its overflow boundary

- `pgIsqrt(0) = 0`
- `pgIsqrt(1) = 1`
- Inputs below `32768² = 1,073,741,824` reject the initial 32768 candidate, after which every candidate is at most 32767 and its square fits in `int32_t`.

Values at or above 1,073,741,824 can reach the overflowing 49152² test described in Finding 2.

## Considered and deliberately not flagged

- I did not flag the mirrored mapping, partial-byte masks, ordinary run/box clipping, Bresenham stepping, or the `r=1..3` circle boundaries; their arithmetic is correct and several are independently supported by D1–D5 and D7–D10.
- I did not flag negative arc spans: under a counterclockwise contract, 0° to −90° legitimately means a 270° counterclockwise arc.
- I did not flag `GMODE` behavior for text, cell-internal text clipping, or the DM42 refresh path because those are documented exclusions.
- `pgStringCut` can truncate immediately after the first byte of a two-byte glyph when its raw `TMP_STR_LENGTH-2` cap falls inside that glyph. Its backward scan then skips the inserted terminator and examines later scratch bytes. I did not promote this to a finding because the packet omits `TMP_STR_LENGTH`, the lifecycle of `tmpString`, and whether the remaining scratch bytes are kept zero; those facts are needed for a concrete observable failure.
- `pgReadAngle` does not explicitly reject NaN or infinity. Such values may eventually be converted from floating NaN to `int32_t`, which would be undefined in C, but the exceptional-value behavior of `C47_WP34S_Cvt2RadSinCosTan` and `realToFloat` is not provided. That is a named audit gap rather than a confident finding.