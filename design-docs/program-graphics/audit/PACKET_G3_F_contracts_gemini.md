Begin your reply with the line `MODEL: <your exact model name>` before
anything else.

## Subject

A personal hobby project: a drawing package (program-graphics) built as an
external package over the open-source C47/R47 firmware for a DM42-class
pocket calculator. A user program draws on the screen with commands such
as LINE and CIRCLE. Stage G3 adds a coordinate window: XRNG and YRNG set
a range per axis, and a real coordinate maps through it. Single-user
handheld. No network stack, no untrusted input, no privilege boundary.
The worst outcome of any bug here is that the calculator reboots and the
owner loses the program they were typing.

Audit for FUNCTIONAL correctness: wrong answers, lost work, stuck states,
crashes. A finding whose impact statement needs an attacker is not a
finding. Report findings, not fixes.

## Orientation

- The screen is 400 columns by 240 rows. The commands take user coordinates with the origin at the bottom left: `PG_ROW_OF(y)` is `SCREEN_HEIGHT - 1 - y`, with `SCREEN_WIDTH` 400 and `SCREEN_HEIGHT` 240. Every internal function works in screen coordinates. The clip rectangle `canvas.clipX0..clipY1` is inclusive; `pgClipNow` gives the clip in force (the canvas clip while the view is open, the whole screen otherwise). The drawing primitives (`pgLine`, `pgBox`, `pgCircle`, the arc stepper, `pgStringCut`, `showString`) are not in this packet; they take int32 screen coordinates and clip every pixel. `pgRefreshMaybe` sends the buffer to the LCD; not the subject.
- `pgWindow` is the new state (the struct below). `pgWindow.set` bit 0 means XRNG was set, bit 1 means YRNG was set. All bytes are zero at boot, and a RESET zeroes them again (the package state is static, not in the memory pool).
- Types and helpers, all upstream: `real34_t` is a 34-digit decimal (IEEE 754-2008 decQuad, 16 bytes); `real_t` is a 75-digit decimal working type; `ctxtReal39` is the 39-digit rounding context. `real34ToReal(src, dst)` converts a real34 to a real_t exactly. `realToReal34(src, dst)` rounds a real_t to 34 digits. `real34Copy(src, dst)` copies 16 bytes. `realSubtract`, `realDivide`, `realMultiply`, `realAdd` take (a, b, result, ctx) and round to the context. `int32ToReal(n, dst)` is exact. `realIsNegative(r)` reads the sign bit (negative zero counts as negative). `realIsZero(r)` is true for any zero. `const_1on2` is the constant 0.5. `real34IsNaN` and `real34IsInfinite` are what they say. `convertLongIntegerRegisterToReal(reg, dst, ctx)` converts a long integer register (arbitrary precision) to a real_t rounded to the context.
- `realToInt32C47(r, &err)` is quoted whole at the end of the code section: it truncates toward zero and sets `err` when the magnitude is above INT32_MAX (or INT32_MAX + 1 for a negative value), or when the value is NaN or infinite. `realToInt` is upstream's decNumber-to-integer helper; its rounding argument `DEC_ROUND_DOWN` means toward zero.
- `screenWindowRatio` in upstream's plotstat.c is quoted whole at the end of the code section as context. The design says the package copies its arithmetic: the ratio in 39 digits, then 0.5 added away from zero, then truncation toward zero (DESIGN.md §5.2). Upstream clamps to +-32767; the package refuses instead (below).
- Registers: `REGISTER_X`, `REGISTER_Y`, `REGISTER_Z`, `REGISTER_T` are the stack. `getRegisterDataType(r)` returns `dtLongInteger`, `dtReal34`, `dtComplex34`, `dtString`, or others. `REGISTER_REAL34_DATA(r)` points at the real34 of a real register and at the real part of a complex register; `REGISTER_IMAG34_DATA(r)` at the imaginary part. The long integer path of `pgReadCoordAxis` is unchanged from stage G2 (audited then): it reads the low limb directly and refuses a magnitude above 32767.
- `displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X)` sets `lastErrorCode` and stops a running program at that step; the package then returns without drawing. Codes used here: `ERROR_OUT_OF_RANGE`, `ERROR_INVALID_DATA_TYPE_FOR_OP`, `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`.
- The contract, quoted from DESIGN.md §5.1 and §5.2 (stage G3): "A long integer is a pixel, also under a window. A real is a user coordinate through the window of its axis. Without a window, a real is a pixel rounded half away from zero. A result beyond 32767 pixels, NaN, or infinity is an ERROR_OUT_OF_RANGE." "A complex is a point: the real part through the x window, the imaginary part through the y window. ARC takes its center this way in T. The two-point commands (LINE, BOX, FBOX, GCLIP) take two complex points, the first in Y and the second in X." "Each coordinate is read by its own type, so a long integer and a real can share one command. A complex in X or Y without a complex in the other is ERROR_INVALID_DATA_TYPE_FOR_OP. A radius is always pixels: a real radius is rounded, never mapped through the window." "The conversion of a real x is pixel = round_half_away((x - xmin) / (xmax - xmin) * (SCREEN_WIDTH - 1)) in 39-digit decimal arithmetic, and the same for y with SCREEN_HEIGHT - 1." "XRNG and YRNG take the minimum in Y and the maximum in X, as long integers or reals. Equal ends raise ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN and leave the window unchanged. A reversed range mirrors the axis. The window survives ERASE and PVIEW; only XRNG, YRNG, and a reset change it. The window maps onto the full pixel grid, so in PVIEW 6 the top 20 rows of the y range lie under the status bar." Every command reads the stack and does not change it.
- Pins that passed (TESTING.md §4), do not report what they prove: W1 without a window 2.5 is pixel 3, 100.49 is 100, and -0.5 draws nothing; W2 XRNG 0 10 and YRNG 0 5 map (5, 2.5) to the pixel (200, 120), the corners to (0, 0) and (399, 239), and a long integer 5 stays column 5; W3 equal ends raise the error and leave the window, XRNG 10 0 mirrors; W4 x = 1000 in a window of 10 raises ERROR_OUT_OF_RANGE, x = 100 draws nothing without an error; W5 two complex points (10, 20) in Y and (50, 20) in X draw the line, a complex in X with a long integer in Y raises the type error; W6 the window survives ERASE. All six went red under a mutation of the code they pin.
- Overrides: pgmGraphics.c is the package's own source file, not an override of an upstream file. The package also overrides upstream items.c (two rows, 2460 XRNG and 2461 YRNG, with the same flags as the G2 rows), items.h, screen.h (the two prototypes), and softmenus.c (two menu entries); none of those change behaviour beyond dispatch and are not in this packet.
- Documented limits, do not report: the top 20 rows of the y range lie under the status bar in PVIEW 6; a real radius is pixels; text is not clipped inside its cell; the DM42 hardware is untested, all evidence is simulator.

## The code

```c
// the window state (the subject)
// The window of §5.2. Zero at boot: no range set, a real is a pixel. Kept
// apart from pgCanvas_t because the header is read before realType.h.
static struct {
  uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
  real34_t xmin, xmax, ymin, ymax;
} pgWindow;
```

```c
// context, not the subject: the region setter and ERASE
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
}

void fnErase(uint16_t unusedButMandatoryParameter) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    fnPview(PG_REGION_REGISTERS);
    return;
  }
  pgSetRegion(canvas.region);
  if(canvas.region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}
```

```c
// context, not the subject: the clip in force
static void pgClipNow(pgRect_t *c) {
  if(calcMode == CM_GRAPHICS_CANVAS) {
    c->x0 = canvas.clipX0; c->y0 = canvas.clipY0; c->x1 = canvas.clipX1; c->y1 = canvas.clipY1;
  }
  else {
    c->x0 = 0; c->y0 = 0; c->x1 = SCREEN_WIDTH - 1; c->y1 = SCREEN_HEIGHT - 1;
  }
}
```

```c
// the error helper and the axis codes
static void pgError(uint16_t code) {
  displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X);
}

#define PG_AXIS_X    0
#define PG_AXIS_Y    1
#define PG_AXIS_NONE 2   // a radius: pixels, rounded, never through the window
```

```c
// the real reader through the window (the subject)
// A real through the window of its axis (§5.2), with the arithmetic of
// upstream's screenWindowRatio (plotstat.c): the ratio in 39 digits, then
// rounded half away from zero. Without a window the real is a pixel,
// rounded the same way. A result beyond 32767 is ERROR_OUT_OF_RANGE.static bool_t pgRealToPixel(const real34_t *v34, uint8_t axis, int32_t *out) {
  real_t t, den;
  bool_t err = false;
  int32_t temp;
  if(real34IsNaN(v34) || real34IsInfinite(v34)) {
    pgError(ERROR_OUT_OF_RANGE);
    return false;
  }
  real34ToReal(v34, &t);
  if(axis != PG_AXIS_NONE && (pgWindow.set & (1u << axis))) {
    real_t mn;
    real34ToReal(axis == PG_AXIS_X ? &pgWindow.xmin : &pgWindow.ymin, &mn);
    real34ToReal(axis == PG_AXIS_X ? &pgWindow.xmax : &pgWindow.ymax, &den);
    realSubtract(&t, &mn, &t, &ctxtReal39);
    realSubtract(&den, &mn, &den, &ctxtReal39);
    realDivide(&t, &den, &t, &ctxtReal39);
    int32ToReal(axis == PG_AXIS_X ? SCREEN_WIDTH - 1 : SCREEN_HEIGHT - 1, &den);
    realMultiply(&t, &den, &t, &ctxtReal39);
  }
  if(realIsNegative(&t)) {
    realSubtract(&t, const_1on2, &t, &ctxtReal39);
  }
  else {
    realAdd(&t, const_1on2, &t, &ctxtReal39);
  }
  temp = realToInt32C47(&t, &err);
  if(err || temp > 32767 || temp < -32767) {
    pgError(ERROR_OUT_OF_RANGE);
    return false;
  }
  *out = temp;
  return true;
}
```

```c
// the coordinate reader (the subject)
// Reads a coordinate from regist into *v: a long integer is a pixel, a real
// goes through the window of the axis. Returns false after an error.static bool_t pgReadCoordAxis(calcRegister_t regist, uint8_t axis, int32_t *v) {
  switch(getRegisterDataType(regist)) {
    case dtLongInteger: {
      const uint8_t *p = REGISTER_LONG_INTEGER_DATA(regist);
      uint32_t bytes = TO_BYTES(getRegisterMaxDataLengthInBlocks(regist));
      uint32_t low = 0, i;
      for(i = 0; i < 4 && i < bytes; i++) {
        low |= (uint32_t)p[i] << (8 * i);
      }
      for(i = 4; i < bytes; i++) {
        if(p[i] != 0) {
          pgError(ERROR_OUT_OF_RANGE);
          return false;
        }
      }
      if(low > 32767u) {
        pgError(ERROR_OUT_OF_RANGE);
        return false;
      }
      switch(getRegisterLongIntegerSign(regist)) {
        case LI_ZERO:     *v = 0;             break;
        case LI_NEGATIVE: *v = -(int32_t)low; break;
        default:          *v = (int32_t)low;  break;
      }
      return true;
    }
    case dtReal34:
      return pgRealToPixel(REGISTER_REAL34_DATA(regist), axis, v);
    default:
      pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
      return false;
  }
}

#define pgReadCoord(regist, v) pgReadCoordAxis((regist), PG_AXIS_NONE, (v))
```

```c
// the complex point reader and the range reader (the subject)
// Reads a complex register as a point: the real part through the x window,
// the imaginary part through the y window. Returns false after an error.static bool_t pgReadComplexPoint(calcRegister_t regist, int32_t *x, int32_t *y) {
  if(getRegisterDataType(regist) != dtComplex34) {
    pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
    return false;
  }
  return pgRealToPixel(REGISTER_REAL34_DATA(regist), PG_AXIS_X, x) && pgRealToPixel(REGISTER_IMAG34_DATA(regist), PG_AXIS_Y, y);
}

// Reads a range end for XRNG and YRNG: a long integer or a real.static bool_t pgReadReal(calcRegister_t regist, real34_t *out) {
  real_t r;
  switch(getRegisterDataType(regist)) {
    case dtLongInteger:
      convertLongIntegerRegisterToReal(regist, &r, &ctxtReal39);
      realToReal34(&r, out);
      return true;
    case dtReal34:
      if(real34IsNaN(REGISTER_REAL34_DATA(regist)) || real34IsInfinite(REGISTER_REAL34_DATA(regist))) {
        pgError(ERROR_OUT_OF_RANGE);
        return false;
      }
      real34Copy(REGISTER_REAL34_DATA(regist), out);
      return true;
    default:
      pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
      return false;
  }
}
```

```c
// XRNG and YRNG (the subject)
// XRNG and YRNG: the minimum in Y, the maximum in X (§2.3). Equal ends are
// ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN and leave the window unchanged. A reversed range
// mirrors the axis. The window survives ERASE and PVIEW.static void pgRange(uint8_t axis) {
  real34_t mn, mx;
  real_t a, b;
  if(!pgReadReal(REGISTER_Y, &mn) || !pgReadReal(REGISTER_X, &mx)) return;
  real34ToReal(&mn, &a);
  real34ToReal(&mx, &b);
  realSubtract(&b, &a, &a, &ctxtReal39);
  if(realIsZero(&a)) {
    pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN);
    return;
  }
  if(axis == PG_AXIS_X) {
    real34Copy(&mn, &pgWindow.xmin);
    real34Copy(&mx, &pgWindow.xmax);
  }
  else {
    real34Copy(&mn, &pgWindow.ymin);
    real34Copy(&mx, &pgWindow.ymax);
  }
  pgWindow.set |= (uint8_t)(1u << axis);
}

void fnXrng(uint16_t unusedButMandatoryParameter) {
  pgRange(PG_AXIS_X);
}

void fnYrng(uint16_t unusedButMandatoryParameter) {
  pgRange(PG_AXIS_Y);
}
```

```c
// the two-point reader with the complex form (the subject), and LINE as its caller
// Reads the two points of a two-point command, y converted to rows: (X, Y)
// and (Z, T), or two complex points, the first in Y and the second in X
// (§5.1). A complex in one of X and Y without the other is a type error.static bool_t pgReadTwoPoints(int32_t *x0, int32_t *r0, int32_t *x1, int32_t *r1) {
  int32_t y0, y1;
  bool_t cx = getRegisterDataType(REGISTER_X) == dtComplex34, cy = getRegisterDataType(REGISTER_Y) == dtComplex34;
  if(cx || cy) {
    if(!cx || !cy) {
      pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
      return false;
    }
    if(!pgReadComplexPoint(REGISTER_Y, x0, &y0) || !pgReadComplexPoint(REGISTER_X, x1, &y1)) {
      return false;
    }
  }
  else if(!pgReadCoordAxis(REGISTER_X, PG_AXIS_X, x0) || !pgReadCoordAxis(REGISTER_Y, PG_AXIS_Y, &y0) ||
          !pgReadCoordAxis(REGISTER_Z, PG_AXIS_X, x1) || !pgReadCoordAxis(REGISTER_T, PG_AXIS_Y, &y1)) {
    return false;
  }
  *r0 = PG_ROW_OF(y0);
  *r1 = PG_ROW_OF(y1);
  return true;
}

void fnGline(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  pgRect_t c;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  pgClipNow(&c);
  pgLine(&c, x0, r0, x1, r1);
  pgRefreshMaybe();
}
```

```c
// the other callers of the readers: circles, ARC, TEXTOUT, GCLIP
static void pgCircleCommand(bool_t filled) {
  int32_t cx, cy, r;
  pgRect_t c;
  if(!pgReadCoordAxis(REGISTER_X, PG_AXIS_X, &cx) || !pgReadCoordAxis(REGISTER_Y, PG_AXIS_Y, &cy) || !pgReadCoord(REGISTER_Z, &r)) return;
  pgClipNow(&c);
  pgCircle(&c, cx, PG_ROW_OF(cy), r, filled);
  pgRefreshMaybe();
}

// ARC: center as a complex number in T, radius in Z, start angle in Y, end angle in X (§2.2).void fnGarc(uint16_t unusedButMandatoryParameter) {
  int32_t cx, cy, r, ax, ay, bx, by;
  real_t a1, a2, s, co, t, d, full;
  float f;
  bool_t wide, fullCircle;
  pgRect_t c;
  if(!pgReadComplexPoint(REGISTER_T, &cx, &cy)) return;
  if(!pgReadCoord(REGISTER_Z, &r) || !pgReadAngle(REGISTER_Y, &a1) || !pgReadAngle(REGISTER_X, &a2)) return;
  // A span of 360 degrees or more is a full circle (§2.2): compare the span in degrees.
  realSubtract(&a2, &a1, &d, &ctxtReal39);
  convertAngleFromTo(&d, currentAngularMode, amDegree, &ctxtReal39);
  realSetPositiveSign(&d);
  int32ToReal(360, &full);
  fullCircle = realCompareGreaterEqual(&d, &full);
  C47_WP34S_Cvt2RadSinCosTan(&a1, currentAngularMode, &s, &co, &t, &ctxtReal39);
  realToFloat(&co, &f); ax = (int32_t)(f * 65536.0f);
  realToFloat(&s,  &f); ay = (int32_t)(f * 65536.0f);
  C47_WP34S_Cvt2RadSinCosTan(&a2, currentAngularMode, &s, &co, &t, &ctxtReal39);
  realToFloat(&co, &f); bx = (int32_t)(f * 65536.0f);
  realToFloat(&s,  &f); by = (int32_t)(f * 65536.0f);
  pgClipNow(&c);
  if(fullCircle) {
    pgCircle(&c, cx, PG_ROW_OF(cy), r, false);
  }
  else {
    int64_t cross = (int64_t)ax * by - (int64_t)ay * bx;
    int64_t dot   = (int64_t)ax * bx + (int64_t)ay * by;
    if(cross == 0 && dot > 0) {
      pgPixel(&c, cx + (int32_t)(((int64_t)ax * r) / 65536), PG_ROW_OF(cy + (int32_t)(((int64_t)ay * r) / 65536)));
    }
    else {
      wide = cross < 0;
      pgArc(&c, cx, cy, r, ax, ay, bx, by, wide);
    }
  }
  pgRefreshMaybe();
}

// TEXTOUT: x in X, y in Y (the top-left corner of the cell, y upward), a string in Z (§4.6).void fnGtextout(uint16_t unusedButMandatoryParameter) {
  int32_t x, y, row;
  pgRect_t c;
  if(!pgReadCoordAxis(REGISTER_X, PG_AXIS_X, &x) || !pgReadCoordAxis(REGISTER_Y, PG_AXIS_Y, &y)) return;
  pgClipNow(&c);
  row = PG_ROW_OF(y);
  if(x < c.x0 || x > c.x1 || row < c.y0 || row + 19 > c.y1) {
    return;
  }
  if(!pgStringCut(REGISTER_Z, (uint32_t)(c.x1 - x + 1))) return;
  showString(tmpString, &standardFont, (uint32_t)x, (uint32_t)row, vmNormal, true, true);
  pgRefreshMaybe();
}

void fnGclip(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  int32_t regionBottom = (canvas.region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  if(x0 > x1) { int32_t t = x0; x0 = x1; x1 = t; }
  if(r0 > r1) { int32_t t = r0; r0 = r1; r1 = t; }
  // The clip is the intersection of the rectangle and the region. A
  // rectangle wholly outside the region gives the empty clip (x0 > x1):
  // every clip test fails and nothing draws. Only values that fit int16
  // reach the stores (audit G2 round 1, Sol 1: one unclamped edge narrowed).
  if(x0 < 0) x0 = 0;
  if(x1 > SCREEN_WIDTH - 1) x1 = SCREEN_WIDTH - 1;
  if(r0 < PG_TOP_ROW) r0 = PG_TOP_ROW;
  if(r1 > regionBottom) r1 = regionBottom;
  if(x0 > x1 || r0 > r1) {
    x0 = 1; x1 = 0;
    r0 = PG_TOP_ROW; r1 = PG_TOP_ROW;
  }
  canvas.clipX0 = (int16_t)x0; canvas.clipY0 = (int16_t)r0;
  canvas.clipX1 = (int16_t)x1; canvas.clipY1 = (int16_t)r1;
}
```

```c
// context, not the subject: upstream screenWindowRatio in src/c47/plotstat.c, byte-identical to upstream, the arithmetic the window copies
// (x - x_min) / (x_max - x_min) * scale in real_t, rounded half away from zero like ROUND_F2I, clamped to +-32767 like the float original.
static int16_t screenWindowRatio(const real_t *v_min, const real_t *v, const real_t *v_max, int32_t scale) {
  int32_t temp;
  real_t tempr, den;
  bool_t err = false;

  realSubtract(v, v_min, &tempr, &ctxtReal39);       // tempr = x - x_min
  realSubtract(v_max, v_min, &den, &ctxtReal39);     // den = x_max - x_min
  realDivide(&tempr, &den, &tempr, &ctxtReal39);     // tempr = (x - x_min) / (x_max - x_min)
  int32ToReal(scale, &den);                          // den = scale
  realMultiply(&tempr, &den, &tempr, &ctxtReal39);   // tempr = (x - x_min) / (x_max - x_min) * scale
  if(realIsNegative(&tempr)) {                       // ROUND_F2I equivalent: +-0.5, then truncate towards zero in realToInt32C47
    realSubtract(&tempr, const_1on2, &tempr, &ctxtReal39);
  }
  else {
    realAdd(&tempr, const_1on2, &tempr, &ctxtReal39);
  }
  temp = realToInt32C47(&tempr, &err);               // err is set when tempr is NaN, infinite or beyond int32 range, e.g. after division by zero when v_max == v_min

  if(err) {
    temp = realIsNegative(&tempr) ? -32767 : 32767;
  }
  else if(temp > 32766) {
    temp = 32767;
  }
  else if(temp < -32766) {
    temp = -32767;
  }

  return (int16_t)temp;
}
```

```c
// context, not the subject: upstream realToInt32C47 in src/c47/realType.c, byte-identical to upstream
int32_t realToInt32C47(const real_t *r, bool_t *error) {
  uint64_t magnitudeLimit;
  bool_t sign;
  int64_t value;

  sign = realIsNegative(r);

  magnitudeLimit = (uint64_t)INT32_MAX + (uint64_t)sign; // 2147483647 or 2147483648

  if(error != NULL) {
    *error = true;
  }
  value = (int64_t)realToInt(r, magnitudeLimit, DEC_ROUND_DOWN, error);
  return sign ? (int32_t)(-value) : (int32_t)value;
}
```

## Your task

You are auditing firmware code for bugs and design flaws. Report what you find. Do not fix anything. The design intent is stated in the Orientation; code that contradicts the stated intent is a finding, and code that contradicts your expectations but matches the intent is not.

Your one question: **do the window and the readers keep their contract with the commands and the calculator around them?** Walk these: the type matrix of `pgReadTwoPoints` (X complex and Y complex; X complex and Y real; X real and Y complex; X long integer and Y complex; Z or T complex with X and Y reals) against the quoted contract; `CIRCLE` and `FCIRCL` with a real center under a window and a real radius; `ARC` whose center goes through the window while its radius does not, and what the picture then means under a non-square window; `TEXTOUT` with a real point under a window; `GCLIP` with reals under a window and then a `LINE` with long integers (two coordinate systems in one program); `XRNG` and `YRNG` with the view closed, from the keyboard, and inside a running program; the stack after `XRNG` (the contract says unchanged); `lastErrorCode` after a refused `XRNG` and what the next drawing command does; the window after a refused `XRNG` (the contract says unchanged) and after `PVIEW`, `ERASE`, and `EXIT`; the case where only one axis has a range and the other axis takes a real; the `pgWindow.set` bits after a RESET (the package state is static and zeroed) versus the register contents that survive; the 39-digit difference test in `pgRange` for two distinct real34 values; the identity path (no window) rounding half away from zero against the stage G2 behaviour that truncated toward zero, and whether any G2 pin or documented behaviour depended on truncation; the interplay of `ERROR_OUT_OF_RANGE` for a mapped coordinate beyond 32767 with the clip law that an off-screen point is not an error; and the design text against the code: DESIGN.md §5.2 says the conversion is `round_half_away((x - xmin) / (xmax - xmin) * (SCREEN_WIDTH - 1))`, and the pins give (5, 2.5) in a 0..10 by 0..5 window as pixel (200, 120); check that the code and the pin agree with the text.

## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing. A named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where (the function and the line of
the excerpt), the concrete reaching input (register values and the
command), the observable consequence, the violated contract quoted, and
your confidence. Rank by what the defect costs the owner. End with the
sites that you examined and found correct, one line each.
