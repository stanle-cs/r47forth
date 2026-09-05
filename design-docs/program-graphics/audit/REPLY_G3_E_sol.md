MODEL: GPT-5

I found one functional defect. The ordinary pixel/window boundary arithmetic is otherwise consistent with the stated formula and with `screenWindowRatio`.

## Finding 1 — Distinct long-integer range endpoints can collapse to an “equal” range

**Severity:** Medium — a valid `XRNG`/`YRNG` command is rejected, the previous window silently remains active, and subsequent drawings use the wrong coordinate system.

**Where:**

- `pgReadReal`, at:
  ```c
  convertLongIntegerRegisterToReal(regist, &r, &ctxtReal39);
  realToReal34(&r, out);
  ```
- `pgRange`, at:
  ```c
  realSubtract(&b, &a, &a, &ctxtReal39);
  if(realIsZero(&a)) {
  ```

**Concrete reaching input:**

For `XRNG`:

- `Y` = long integer `10^39`
- `X` = long integer `10^39 + 10^5`

Both are 40-digit integers, and their actual difference is `100000`.

Processing:

1. Both fit in the 39-digit working conversion.
2. Conversion to 34-digit `real34_t` rounds both to `1e39`; the difference is below the real34 unit at that magnitude.
3. `pgRange` consequently computes:
   ```text
   b - a = 1e39 - 1e39 = 0
   ```
4. It raises `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and leaves the old window unchanged.

An even closer pair, `10^39` and `10^39 + 1`, already collapses during the 39-digit conversion and has the same result.

The mixed-type case also fails:

- `Y` = real34 `1e39`
- `X` = long integer `10^39 + 1`

The long rounds to `1e39`, so the unequal original endpoints are again diagnosed as equal.

**Observable consequence:** A numerically nonzero range cannot be installed. Because the old window remains unchanged by design, later drawing commands can produce coordinates according to an earlier range rather than the one the owner just entered.

**Violated contract:**

> “XRNG and YRNG take the minimum in Y and the maximum in X, as long integers or reals. Equal ends raise ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN…”

The supplied long-integer ends are not equal.

**Confidence:** High. The precision loss and equality result follow directly from the stated 39-digit and 34-digit conversions.

There is a related unresolvable gap: the packet does not specify the exponent range or overflow behavior of `convertLongIntegerRegisterToReal` and `realToReal34`. Therefore I cannot determine whether sufficiently enormous long endpoints become infinity, nor the exact resulting persistent-window behavior.

## Boundary walk

### No window

`pgWindow.set` has no applicable axis bit, so only ±0.5 and truncation occur.

| Input | After ±0.5 | Conversion/result |
|---:|---:|---:|
| `2.5` | `3.0` | `3` |
| `-2.5` | `-3.0` | `-3` |
| `0.5` | `1.0` | `1` |
| `-0.5` | `-1.0` | `-1` |
| `32767.4` | `32767.9` | truncates to `32767`, accepted |
| `32767.5` | `32768.0` | `32768`, `ERROR_OUT_OF_RANGE` |
| `-32767.5` | `-32768.0` | `-32768`, `ERROR_OUT_OF_RANGE` |
| `1e30` | approximately `1e30 + 0.5` | outside `int32`, `ERROR_OUT_OF_RANGE` |
| negative zero | `-0.5` | truncates to integer `0`, accepted |
| NaN | not evaluated | immediate `ERROR_OUT_OF_RANGE` |

Negative zero follows the negative branch because `realIsNegative` reads its sign bit, but truncating `-0.5` toward zero still produces zero.

### `XRNG 0 10`

The formula is `(x / 10) × 399`.

- `x = 5`:
  ```text
  5 / 10 × 399 = 199.5
  199.5 + 0.5 = 200
  ```
  Result: `200`.

- `x = 10`:
  ```text
  10 / 10 × 399 = 399
  399 + 0.5 = 399.5
  ```
  Truncation gives `399`.

- `x = 10.0000000001`:
  ```text
  10.0000000001 / 10 = 1.00000000001
  × 399 = 399.00000000399
  + 0.5 = 399.50000000399
  ```
  Truncation gives `399`, not `400`.

- `x = -1e-40`:
  ```text
  -1e-40 / 10 × 399 = -3.99e-39
  subtract 0.5 ≈ -0.500000000000000000000000000000000000004
  ```
  Truncation toward zero gives `0`.

### Very narrow and very wide ranges

Width alone is insufficient to determine an output; the endpoints and input are also needed. For canonical zero-based ranges:

- `XRNG 0 1e-30`, `x = 5e-31`:
  ```text
  5e-31 / 1e-30 × 399 = 199.5 → 200
  ```

- `XRNG 0 1e30`, `x = 5e29`:
  ```text
  5e29 / 1e30 × 399 = 199.5 → 200
  ```

Both widths are within ordinary decimal exponent ranges and introduce no special boundary failure.

A width of `1e-30` superimposed on a sufficiently large nonzero minimum may be unrepresentable in real34—for example, `1e30` and `1e30 + 1e-30` cannot remain distinct. That is the endpoint-collapse defect above, not a division defect.

### Reversed `XRNG 10 0`

The formula is `((x - 10) / -10) × 399`.

| `x` | Unrounded pixel | Result |
|---:|---:|---:|
| `0` | `399` | `399` |
| `5` | `199.5` | `200` |
| `10` | signed zero | `0` |
| `11` | `-39.9` | `-40` |

The reversal therefore mirrors the axis correctly. Values outside the range remain permissible until their rounded pixel magnitude exceeds `32767`; drawing primitives then clip them.

### `XRNG -32767 32767`, `x = 32767`

```text
(x - xmin) = 65534
(xmax - xmin) = 65534
ratio = 1
pixel = 399
399 + 0.5 = 399.5
```

Result: `399`.

### Exact half cases and upstream agreement

For an exact positive `n + 0.5`, such as `2.5`:

```text
2.5 + 0.5 = 3
truncate → 3
```

For a negative half case, write it as `n + 0.5` with `n ≤ -1`; for example, `n = -3` gives `-2.5`:

```text
-2.5 - 0.5 = -3
truncate → -3
```

Thus positive ties go to `n + 1`, while negative ties go to `n`: both are away from zero. This is exactly the sequence in `screenWindowRatio`. The package and upstream agree wherever upstream does not replace the result with its clamp.

### `realToInt32C47` after the rounding step

- If `t` is exactly `32767.5`, truncation gives `32767`; there is no int32 error, and the package accepts it.
- If `t` is exactly `2147483647.5`, its magnitude exceeds `INT32_MAX`, so according to the supplied helper contract `err` is set; `pgRealToPixel` returns `ERROR_OUT_OF_RANGE`.

The first case is why an original unrounded pixel value of exactly `32767` remains valid: adding `0.5` produces `32767.5`, which truncates back to `32767`.

## Examined and found correct

- `pgRealToPixel`: NaN and infinity are rejected before arithmetic; finite values are rounded half away from zero.
- `pgRealToPixel`: the ±32767 package limit is checked after rounding, as required.
- `pgRealToPixel`: X and Y select bits `1` and `2` respectively; with only `YRNG` set, a real X remains a pixel.
- `pgRealToPixel`: the 39-digit arithmetic and rounding sequence matches `screenWindowRatio`; refusing overflow instead of clamping is the documented difference.
- `pgReadReal`: a real34 negative zero is accepted and copied with its sign; paired with positive zero it is correctly treated as an equal endpoint.
- `pgRange`: if Y is a string or another nonnumeric type, it raises `ERROR_INVALID_DATA_TYPE_FOR_OP`, does not inspect X because of short-circuiting, and does not change the window.
- `pgRange`: one long and one real endpoint work when their converted real34 values remain distinct; the defect is the precision-collapse case above.
- `pgReadComplexPoint`: with a valid real part and NaN imaginary part, the X temporary may be assigned, but the result is `ERROR_OUT_OF_RANGE` and the caller performs no drawing, so nothing is half-drawn.
- `pgReadTwoPoints`: when X and Y are both complex, Z and T are not read; strings in Z and T are irrelevant.
- `pgReadTwoPoints`: one complex among X and Y raises `ERROR_INVALID_DATA_TYPE_FOR_OP`.
- `fnGclip`: user coordinates are mapped before row conversion, ordering, and intersection. For `XRNG 0 10`, `YRNG 0 5`, Y=`(2.5,1)` and X=`(7.5,4)`, the mapped points are `(100,48)` and `(299,191)` in bottom-left coordinates, producing screen rows `191` and `48`, hence clip columns `100..299` and rows `48..191`.
- `fnGclip`: mapped rectangles wholly outside the drawable region produce the intended empty clip.
