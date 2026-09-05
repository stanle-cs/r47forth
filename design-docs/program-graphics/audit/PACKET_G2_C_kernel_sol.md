Begin your reply with the line `MODEL: <your exact model name>` before
anything else.

## Subject

A personal hobby project: a drawing package (program-graphics) built as an
external package over the open-source C47/R47 firmware for a DM42-class
pocket calculator. A user program draws on the screen with commands such
as LINE and CIRCLE. Single-user handheld. No network stack, no untrusted
input, no privilege boundary. The worst outcome of any bug here is that
the calculator reboots and the owner loses the program they were typing.

Audit for FUNCTIONAL correctness: wrong answers, lost work, stuck states,
crashes. A finding whose impact statement needs an attacker is not a
finding. Report findings, not fixes.

## Orientation

- The screen is 400 columns by 240 rows, one bit per pixel. `lcd_buffer` points at 240 rows of 52 bytes: byte 0 is the dirty flag (nonzero means the row goes to the LCD at the next refresh), byte 1 is the row number, bytes 2 to 51 hold the 400 pixels. The bit order is mirrored: pixel column x sits at bit `(399 - x) & 7` of byte `2 + ((399 - x) >> 3)`. A set bit is a black pixel. This layout is the same on the DM42 and on the simulator (`c47.c:618`, `lcd_buffer = lcd_line_addr(0) - 2`). The upstream blitter `bitblt24` uses the same mapping and sets byte 0 to 1 after a write (simulator `c47-gtk/hal/lcd.c:119-170`).
- Row 0 is the top of the screen. The commands take user coordinates with the origin at the bottom left: `PG_ROW_OF(y)` is `SCREEN_HEIGHT - 1 - y`. `SCREEN_WIDTH` is 400, `SCREEN_HEIGHT` is 240, `PG_TOP_ROW` is 20, `PG_REGISTER_BOTTOM_ROW` is 170.
- `canvas` is the package state (the struct in pgmGraphics.h below). `canvas.clipX0..clipY1` is the clip rectangle in screen coordinates, inclusive, set by `PVIEW` (rows 20 to 170 for region 2, rows 20 to 239 for region 6) and by `GCLIP`. `calcMode` is the calculator's mode; `CM_GRAPHICS_CANVAS` (21) is the canvas view. Outside the view the clip is the whole screen (`pgClipNow`).
- `canvas.drawMode` is 0 (set), 1 (clear) or 2 (invert), set by `GMODE n`. `GMODE` and the other parameter commands receive their parameter as the function argument; the item table limits `GMODE` to 0..2 and `DISP` to 1..11 before the function runs.
- Registers: `REGISTER_X`, `REGISTER_Y`, `REGISTER_Z`, `REGISTER_T` are the stack. `getRegisterDataType(r)` returns `dtLongInteger`, `dtReal34`, `dtComplex34`, `dtString`, or others. A long integer register holds GMP limbs, little-endian, at `REGISTER_LONG_INTEGER_DATA(r)`, `TO_BYTES(getRegisterMaxDataLengthInBlocks(r))` bytes long (a multiple of 4), with the sign in the register tag: `getRegisterLongIntegerSign(r)` is `LI_ZERO`, `LI_NEGATIVE` or `LI_POSITIVE`. Upstream's own reader is `convertLongIntegerRegisterToLongInteger` (registerValueConversions.c:19), which copies the bytes into an mpz and trims trailing zero limbs. A real34 register holds a decQuad at `REGISTER_REAL34_DATA(r)`; `real34ToInt32` is `decQuadToInt32` with `DEC_ROUND_DOWN` (truncation toward zero); `real34CompareLessThan(a, b)` is a < b. A complex register holds two real34 at `REGISTER_REAL34_DATA(r)` and `REGISTER_IMAG34_DATA(r)`. A string register holds a NUL-terminated string at `REGISTER_STRING_DATA(r)` in the calculator's own encoding: bytes below 0x80 are one-byte glyphs, a byte at or above 0x80 starts a two-byte glyph.
- `displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X)` sets `lastErrorCode` and stops a running program at that step; the package then returns without drawing. `ERROR_OUT_OF_RANGE` and `ERROR_INVALID_DATA_TYPE_FOR_OP` are upstream codes.
- Angles: `currentAngularMode` is `amDegree`, `amRadian`, `amGrad` or `amMultPi`. `C47_WP34S_Cvt2RadSinCosTan(angle, mode, sin, cos, tan, ctx)` converts the angle from the mode to radians and returns sine, cosine and tangent as `real_t`. `convertAngleFromTo(angle, from, to, ctx)` converts in place. `realToFloat(real, float*)` converts a `real_t` to a C float. `int32ToReal(i, real)` builds a real from an int. `realCompareGreaterEqual(a, b)` is a >= b. `realSetPositiveSign` takes the absolute value.
- `showString(s, &standardFont, x, y, vmNormal, true, true)` paints s with the 20-pixel standard font at column x, top row y; it does not clip, so the caller must keep the text on screen. `stringWidth(s, &standardFont, true, true)` returns the width in pixels. `tmpString` is a shared scratch buffer of `TMP_STR_LENGTH` bytes.
- `lcd_fill_rect(x, y, w, h, LCD_SET_VALUE)` clears a rectangle to white. `LCD_SET_VALUE` is 0 and clears; the names are inverted relative to their effect.
- `getUptimeMs()` is the millisecond clock. `programRunStop == PGM_RUNNING` while a program runs. `lcd_refresh()` sends the dirty rows to the LCD on the simulator; `lcd_refresh_dma()` does it on the DM42.
- Contract of the commands (DESIGN.md §2.2, §4, §5): every command reads the stack and does not change it; a coordinate is a long integer (pixel, magnitude up to 32767) or a real (pixel after truncation, magnitude below 32768); other types are `ERROR_INVALID_DATA_TYPE_FOR_OP`; an off-screen point is not an error, it is clipped; `LINE`, `BOX`, `FBOX` read x1 in X, y1 in Y, x2 in Z, y2 in T; `CIRCLE` and `FCIRCL` read the center in X and Y and the radius in Z; `ARC` reads the center as a complex in T, the radius in Z, the start angle in Y and the end angle in X, draws counterclockwise, and a span of 360 degrees or more is a full circle; `TEXTOUT` reads x in X, y in Y (the top-left of the text cell, y upward) and the string in Z, and cuts the string at the right edge of the clip; `DISP n` draws the string of X on canvas line n (row 20 + 20 (n - 1)); `GCLIP` reads a rectangle from (X, Y) to (Z, T) and keeps it inside the region.
- Pins that passed (TESTING.md §4): D1 to D12 and S1, listed at the end of this packet. Do not report what a pin already proves; do report what no pin covers.
- Documented limits, do not report: text is not clipped inside its cell beyond the string cut; `GMODE` does not apply to text; the DM42 refresh path is untested; user coordinates through a window come in a later stage.

## The code

```c
// packages/program-graphics/pgmGraphics.h (whole file; the struct is the subject, the rest is context)
// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.h
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#if !defined(PGMGRAPHICS_H)
  #define PGMGRAPHICS_H

  #include <stdint.h>

  #define PG_TOP_ROW              20   // first row below the status bar
  #define PG_REGISTER_BOTTOM_ROW 170   // last row of the register lines (region 2)
  #define PG_REGION_REGISTERS      2   // PVIEW 2: the register lines
  #define PG_REGION_FULL           6   // PVIEW 6: the register lines and the softmenu

  /** State of the canvas view. All fields are zero at boot (DESIGN.md §3.3). */
  typedef struct {
    uint8_t   region;        // 0 = view closed, else 2 or 6
    uint8_t   prevCalcMode;  // the calcMode to restore on EXIT
    uint8_t   drawMode;      // 0 set, 1 clear, 2 invert
    uint8_t   errorShown;    // 1 while an error message is painted on canvas line 1
    int16_t   clipX0, clipY0, clipX1, clipY1;   // screen coordinates, top-left origin, inclusive
    uint32_t  lastRefreshMs;
  } pgCanvas_t;

  // The commands and the view hooks are declared in screen.h, next to the
  // upstream PIXEL family, so that items.c, keyboard.c, and screen.c see
  // them through c47.h.

  // Test drivers for the headless suite (TESTING.md §1). Each writes its
  // failure count into X as a long integer.
  void pgTestSmoke   (uint16_t unusedButMandatoryParameter);
  void pgTestBaseline(uint16_t unusedButMandatoryParameter);
  void pgTestView    (uint16_t unusedButMandatoryParameter);
  void pgTestKeys    (uint16_t unusedButMandatoryParameter);
  void pgTestDraw2D  (uint16_t unusedButMandatoryParameter);
  void pgTestShowcase2D(uint16_t unusedButMandatoryParameter);

#endif // !PGMGRAPHICS_H

```

```c
// packages/program-graphics/pgmGraphics.c, the stage G2 part: the kernel, the reader, the commands (whole functions, the subject)
// Stage G2: the drawing kernel (DESIGN.md §4) and the argument reader (§5.1)
// ---------------------------------------------------------------------------

#define PG_ROW_BYTES 52   // 2 header bytes plus 50 pixel bytes per row

typedef struct {
  int32_t x0, y0, x1, y1;   // screen coordinates, top-left origin, inclusive
} pgRect_t;

// The clip rectangle in force: the canvas clip in the view, the whole screen outside it (§4.2).
static void pgClipNow(pgRect_t *c) {
  if(calcMode == CM_GRAPHICS_CANVAS) {
    c->x0 = canvas.clipX0; c->y0 = canvas.clipY0; c->x1 = canvas.clipX1; c->y1 = canvas.clipY1;
  }
  else {
    c->x0 = 0; c->y0 = 0; c->x1 = SCREEN_WIDTH - 1; c->y1 = SCREEN_HEIGHT - 1;
  }
}

static inline uint8_t *pgRowPtr(int32_t row) {
  return lcd_buffer + row * PG_ROW_BYTES;
}

static inline void pgApply(uint8_t *p, uint8_t mask) {
  switch(canvas.drawMode) {
    case 1:  *p &= (uint8_t)~mask; break;
    case 2:  *p ^= mask;           break;
    default: *p |= mask;           break;
  }
}

// One pixel at screen column col, screen row row (§4.1).
static void pgPixel(const pgRect_t *c, int32_t col, int32_t row) {
  uint32_t xm;
  if(col < c->x0 || col > c->x1 || row < c->y0 || row > c->y1) {
    return;
  }
  xm = (uint32_t)(SCREEN_WIDTH - 1 - col);
  pgApply(pgRowPtr(row) + 2 + (xm >> 3), (uint8_t)(1u << (xm & 7)));
  pgRowPtr(row)[0] = 1u;
}

// A horizontal run from col0 to col1 on one row, whole bytes at a time (§4.1).
static void pgRun(const pgRect_t *c, int32_t col0, int32_t col1, int32_t row) {
  uint32_t a, b, byteA, byteB;
  uint8_t *p;
  if(row < c->y0 || row > c->y1) {
    return;
  }
  if(col0 > col1) { int32_t t = col0; col0 = col1; col1 = t; }
  if(col0 < c->x0) col0 = c->x0;
  if(col1 > c->x1) col1 = c->x1;
  if(col0 > col1) {
    return;
  }
  a = (uint32_t)(SCREEN_WIDTH - 1 - col1);   // mirrored bit positions, a <= b
  b = (uint32_t)(SCREEN_WIDTH - 1 - col0);
  byteA = a >> 3;
  byteB = b >> 3;
  p = pgRowPtr(row) + 2;
  if(byteA == byteB) {
    pgApply(p + byteA, (uint8_t)((0xFFu << (a & 7)) & (0xFFu >> (7 - (b & 7)))));
  }
  else {
    uint32_t i;
    pgApply(p + byteA, (uint8_t)(0xFFu << (a & 7)));
    for(i = byteA + 1; i < byteB; i++) {
      pgApply(p + i, 0xFFu);
    }
    pgApply(p + byteB, (uint8_t)(0xFFu >> (7 - (b & 7))));
  }
  pgRowPtr(row)[0] = 1u;
}

// Integer Bresenham line, endpoints inclusive (§4.3).
static void pgLine(const pgRect_t *c, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  int32_t dx, dy, sx, sy, err;
  if(y0 == y1) {
    pgRun(c, x0, x1, y0);
    return;
  }
  dx = x1 > x0 ? x1 - x0 : x0 - x1;
  dy = y1 > y0 ? y0 - y1 : y1 - y0;   // negative
  sx = x0 < x1 ? 1 : -1;
  sy = y0 < y1 ? 1 : -1;
  err = dx + dy;
  for(;;) {
    int32_t e2;
    pgPixel(c, x0, y0);
    if(x0 == x1 && y0 == y1) {
      break;
    }
    e2 = 2 * err;
    if(e2 >= dy) { err += dy; x0 += sx; }
    if(e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void pgBox(const pgRect_t *c, int32_t x0, int32_t y0, int32_t x1, int32_t y1, bool_t filled) {
  int32_t row, r0, r1;
  if(x0 > x1) { int32_t t = x0; x0 = x1; x1 = t; }
  if(y0 > y1) { int32_t t = y0; y0 = y1; y1 = t; }
  if(filled) {
    r0 = y0 < c->y0 ? c->y0 : y0;
    r1 = y1 > c->y1 ? c->y1 : y1;
    for(row = r0; row <= r1; row++) {
      pgRun(c, x0, x1, row);
    }
    return;
  }
  pgRun(c, x0, x1, y0);
  if(y1 != y0) {
    pgRun(c, x0, x1, y1);
  }
  r0 = (y0 + 1) < c->y0 ? c->y0 : y0 + 1;
  r1 = (y1 - 1) > c->y1 ? c->y1 : y1 - 1;
  for(row = r0; row <= r1; row++) {
    pgPixel(c, x0, row);
    if(x1 != x0) {
      pgPixel(c, x1, row);
    }
  }
}

static int32_t pgIsqrt(int32_t v) {
  int32_t r = 0, bit;
  if(v <= 0) return 0;
  for(bit = 1 << 15; bit > 0; bit >>= 1) {
    if((r + bit) * (r + bit) <= v) r += bit;
  }
  return r;
}

// Circle outline or fill. (cx, cy) and the radius in screen coordinates (§4.5).
static void pgCircle(const pgRect_t *c, int32_t cx, int32_t cy, int32_t r, bool_t filled) {
  int32_t x, y, err;
  if(r < 0) r = -r;
  if(filled) {
    int32_t dy;
    for(dy = -r; dy <= r; dy++) {
      int32_t w = (pgIsqrt(4 * (r * r - dy * dy)) + 1) / 2;   // rounded half-width, matches the midpoint outline
      pgRun(c, cx - w, cx + w, cy + dy);
    }
    return;
  }
  if(r == 0) {
    pgPixel(c, cx, cy);
    return;
  }
  x = r; y = 0; err = 1 - r;
  while(x >= y) {
    if(y == 0) {
      pgPixel(c, cx + x, cy); pgPixel(c, cx - x, cy); pgPixel(c, cx, cy + x); pgPixel(c, cx, cy - x);
    }
    else if(x == y) {
      pgPixel(c, cx + x, cy + y); pgPixel(c, cx - x, cy + y); pgPixel(c, cx + x, cy - y); pgPixel(c, cx - x, cy - y);
    }
    else {
      pgPixel(c, cx + x, cy + y); pgPixel(c, cx - x, cy + y); pgPixel(c, cx + x, cy - y); pgPixel(c, cx - x, cy - y);
      pgPixel(c, cx + y, cy + x); pgPixel(c, cx - y, cy + x); pgPixel(c, cx + y, cy - x); pgPixel(c, cx - y, cy - x);
    }
    y++;
    if(err < 0) {
      err += 2 * y + 1;
    }
    else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

// Arc test: is the direction (dx, dy), y upward, inside the counterclockwise
// span from A to B? A and B are direction vectors scaled by 1024. wide is
// true when the span exceeds 180 degrees (§4.5).
static bool_t pgInSpan(int32_t ax, int32_t ay, int32_t bx, int32_t by, bool_t wide, int32_t dx, int32_t dy) {
  int64_t ca = (int64_t)ax * dy - (int64_t)ay * dx;   // cross(A, P)
  int64_t cb = (int64_t)dx * by - (int64_t)dy * bx;   // cross(P, B)
  if(wide) {
    return !(ca < 0 && cb < 0);
  }
  return ca >= 0 && cb >= 0;
}

// Arc outline: the circle stepper with the span test. Coordinates in user
// frame (y upward); rowOf converts at the plot.
#define PG_ROW_OF(yUser) (SCREEN_HEIGHT - 1 - (yUser))
static void pgArcPoint(const pgRect_t *c, int32_t cx, int32_t cyUser, int32_t dx, int32_t dy,
                       int32_t ax, int32_t ay, int32_t bx, int32_t by, bool_t wide) {
  if(pgInSpan(ax, ay, bx, by, wide, dx, dy)) {
    pgPixel(c, cx + dx, PG_ROW_OF(cyUser + dy));
  }
}

static void pgArc(const pgRect_t *c, int32_t cx, int32_t cyUser, int32_t r,
                  int32_t ax, int32_t ay, int32_t bx, int32_t by, bool_t wide) {
  int32_t x, y, err;
  if(r < 0) r = -r;
  if(r == 0) {
    pgPixel(c, cx, PG_ROW_OF(cyUser));
    return;
  }
  x = r; y = 0; err = 1 - r;
  while(x >= y) {
    if(y == 0) {
      pgArcPoint(c, cx, cyUser,  x,  0, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -x,  0, ax, ay, bx, by, wide);
      pgArcPoint(c, cx, cyUser,  0,  x, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser,  0, -x, ax, ay, bx, by, wide);
    }
    else if(x == y) {
      pgArcPoint(c, cx, cyUser,  x,  y, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -x,  y, ax, ay, bx, by, wide);
      pgArcPoint(c, cx, cyUser,  x, -y, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -x, -y, ax, ay, bx, by, wide);
    }
    else {
      pgArcPoint(c, cx, cyUser,  x,  y, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -x,  y, ax, ay, bx, by, wide);
      pgArcPoint(c, cx, cyUser,  x, -y, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -x, -y, ax, ay, bx, by, wide);
      pgArcPoint(c, cx, cyUser,  y,  x, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -y,  x, ax, ay, bx, by, wide);
      pgArcPoint(c, cx, cyUser,  y, -x, ax, ay, bx, by, wide); pgArcPoint(c, cx, cyUser, -y, -x, ax, ay, bx, by, wide);
    }
    y++;
    if(err < 0) {
      err += 2 * y + 1;
    }
    else {
      x--;
      err += 2 * (y - x) + 1;
    }
  }
}

// ---- the argument reader (§5.1) ----

static real34_t pgLimitPos, pgLimitNeg;   // +32768 and -32768, built once
static bool_t   pgLimitsReady;

static void pgError(uint16_t code) {
  displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X);
}

// Reads a pixel coordinate from regist into *v. Returns false after an error.
static bool_t pgReadCoord(calcRegister_t regist, int32_t *v) {
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
    case dtReal34: {
      const real34_t *x = REGISTER_REAL34_DATA(regist);
      if(!pgLimitsReady) {
        int32ToReal34(32768, &pgLimitPos);
        int32ToReal34(-32768, &pgLimitNeg);
        pgLimitsReady = true;
      }
      if(real34IsNaN(x) || real34IsInfinite(x) || !real34CompareLessThan(x, &pgLimitPos) || !real34CompareLessThan(&pgLimitNeg, x)) {
        pgError(ERROR_OUT_OF_RANGE);
        return false;
      }
      *v = real34ToInt32(x);
      return true;
    }
    default:
      pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
      return false;
  }
}

// Reads an angle from regist into a real, in the current angular mode.
static bool_t pgReadAngle(calcRegister_t regist, real_t *angle) {
  switch(getRegisterDataType(regist)) {
    case dtLongInteger: convertLongIntegerRegisterToReal(regist, angle, &ctxtReal39); return true;
    case dtReal34:      real34ToReal(REGISTER_REAL34_DATA(regist), angle);            return true;
    default:            pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);                      return false;
  }
}

// Refresh cadence (§8.4).
static void pgRefreshMaybe(void) {
  if(programRunStop != PGM_RUNNING) {
    pgRefreshNow();
    return;
  }
  if(getUptimeMs() - canvas.lastRefreshMs >= 40) {
    pgRefreshNow();
  }
}

// Reads the two points of a two-point command: (X, Y) and (Z, T), y converted to rows.
static bool_t pgReadTwoPoints(int32_t *x0, int32_t *r0, int32_t *x1, int32_t *r1) {
  int32_t y0, y1;
  if(!pgReadCoord(REGISTER_X, x0) || !pgReadCoord(REGISTER_Y, &y0) || !pgReadCoord(REGISTER_Z, x1) || !pgReadCoord(REGISTER_T, &y1)) {
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

void fnGbox(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  pgRect_t c;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  pgClipNow(&c);
  pgBox(&c, x0, r0, x1, r1, false);
  pgRefreshMaybe();
}

void fnGfbox(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  pgRect_t c;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  pgClipNow(&c);
  pgBox(&c, x0, r0, x1, r1, true);
  pgRefreshMaybe();
}

static void pgCircleCommand(bool_t filled) {
  int32_t cx, cy, r;
  pgRect_t c;
  if(!pgReadCoord(REGISTER_X, &cx) || !pgReadCoord(REGISTER_Y, &cy) || !pgReadCoord(REGISTER_Z, &r)) return;
  pgClipNow(&c);
  pgCircle(&c, cx, PG_ROW_OF(cy), r, filled);
  pgRefreshMaybe();
}

void fnGcircle(uint16_t unusedButMandatoryParameter) {
  pgCircleCommand(false);
}

void fnGfcircle(uint16_t unusedButMandatoryParameter) {
  pgCircleCommand(true);
}

// ARC: center as a complex number in T, radius in Z, start angle in Y, end angle in X (§2.2).
void fnGarc(uint16_t unusedButMandatoryParameter) {
  int32_t cx, cy, r, ax, ay, bx, by;
  real_t a1, a2, s, co, t, d, full;
  float f;
  bool_t wide, fullCircle;
  pgRect_t c;
  if(getRegisterDataType(REGISTER_T) != dtComplex34) {
    pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
    return;
  }
  {
    const real34_t *re = REGISTER_REAL34_DATA(REGISTER_T);
    const real34_t *im = REGISTER_IMAG34_DATA(REGISTER_T);
    if(!pgLimitsReady) {
      int32ToReal34(32768, &pgLimitPos);
      int32ToReal34(-32768, &pgLimitNeg);
      pgLimitsReady = true;
    }
    if(real34IsNaN(re) || real34IsInfinite(re) || !real34CompareLessThan(re, &pgLimitPos) || !real34CompareLessThan(&pgLimitNeg, re) ||
       real34IsNaN(im) || real34IsInfinite(im) || !real34CompareLessThan(im, &pgLimitPos) || !real34CompareLessThan(&pgLimitNeg, im)) {
      pgError(ERROR_OUT_OF_RANGE);
      return;
    }
    cx = real34ToInt32(re);
    cy = real34ToInt32(im);
  }
  if(!pgReadCoord(REGISTER_Z, &r) || !pgReadAngle(REGISTER_Y, &a1) || !pgReadAngle(REGISTER_X, &a2)) return;
  // A span of 360 degrees or more is a full circle (§2.2): compare the span in degrees.
  realSubtract(&a2, &a1, &d, &ctxtReal39);
  convertAngleFromTo(&d, currentAngularMode, amDegree, &ctxtReal39);
  realSetPositiveSign(&d);
  int32ToReal(360, &full);
  fullCircle = realCompareGreaterEqual(&d, &full);
  C47_WP34S_Cvt2RadSinCosTan(&a1, currentAngularMode, &s, &co, &t, &ctxtReal39);
  realToFloat(&co, &f); ax = (int32_t)(f * 1024.0f);
  realToFloat(&s,  &f); ay = (int32_t)(f * 1024.0f);
  C47_WP34S_Cvt2RadSinCosTan(&a2, currentAngularMode, &s, &co, &t, &ctxtReal39);
  realToFloat(&co, &f); bx = (int32_t)(f * 1024.0f);
  realToFloat(&s,  &f); by = (int32_t)(f * 1024.0f);
  pgClipNow(&c);
  if(fullCircle) {
    pgCircle(&c, cx, PG_ROW_OF(cy), r, false);
  }
  else {
    int64_t cross = (int64_t)ax * by - (int64_t)ay * bx;
    int64_t dot   = (int64_t)ax * bx + (int64_t)ay * by;
    if(cross == 0 && dot > 0) {
      pgPixel(&c, cx + (int32_t)(((int64_t)ax * r) / 1024), PG_ROW_OF(cy + (int32_t)(((int64_t)ay * r) / 1024)));
    }
    else {
      wide = cross < 0;
      pgArc(&c, cx, cy, r, ax, ay, bx, by, wide);
    }
  }
  pgRefreshMaybe();
}

// Copies the string of regist into tmpString, cut to at most width pixels of the standard font.
static bool_t pgStringCut(calcRegister_t regist, uint32_t width) {
  const char *s;
  size_t n;
  if(getRegisterDataType(regist) != dtString) {
    pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
    return false;
  }
  s = REGISTER_STRING_DATA(regist);
  n = strlen(s);
  if(n >= TMP_STR_LENGTH - 1) {
    n = TMP_STR_LENGTH - 2;
  }
  memcpy(tmpString, s, n);
  tmpString[n] = 0;
  while(tmpString[0] != 0 && stringWidth(tmpString, &standardFont, true, true) > width) {
    // remove the last glyph: a byte at or above 0x80 starts a two-byte glyph
    size_t i = 0, last = 0;
    while(tmpString[i] != 0) {
      last = i;
      i += ((uint8_t)tmpString[i] & 0x80) ? 2 : 1;
    }
    tmpString[last] = 0;
  }
  return true;
}

// TEXTOUT: x in X, y in Y (the top-left corner of the cell, y upward), a string in Z (§4.6).
void fnGtextout(uint16_t unusedButMandatoryParameter) {
  int32_t x, y, row;
  pgRect_t c;
  if(!pgReadCoord(REGISTER_X, &x) || !pgReadCoord(REGISTER_Y, &y)) return;
  pgClipNow(&c);
  row = PG_ROW_OF(y);
  if(x < c.x0 || x > c.x1 || row < c.y0 || row + 19 > c.y1) {
    return;
  }
  if(!pgStringCut(REGISTER_Z, (uint32_t)(c.x1 - x + 1))) return;
  showString(tmpString, &standardFont, (uint32_t)x, (uint32_t)row, vmNormal, true, true);
  pgRefreshMaybe();
}

// DISP n: the string of X on canvas line n, from the top (§4.6).
void fnGdisp(uint16_t line) {
  int32_t row;
  pgRect_t c;
  if(line < 1 || line > 11) {
    pgError(ERROR_OUT_OF_RANGE);
    return;
  }
  pgClipNow(&c);
  row = PG_TOP_ROW + ((int32_t)line - 1) * 20;
  if(row < c.y0 || row + 19 > c.y1) {
    return;
  }
  if(!pgStringCut(REGISTER_X, SCREEN_WIDTH - 1)) return;
  lcd_fill_rect(0, (uint32_t)row, SCREEN_WIDTH, 20, LCD_SET_VALUE);
  showString(tmpString, &standardFont, 1, (uint32_t)row, vmNormal, true, true);
  pgRefreshMaybe();
}

void fnGmode(uint16_t mode) {
  if(mode > 2) {
    pgError(ERROR_OUT_OF_RANGE);
    return;
  }
  canvas.drawMode = (uint8_t)mode;
}

// GCLIP: the clip rectangle from (X, Y) to (Z, T), kept inside the region (§2.2).
void fnGclip(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  int32_t regionBottom = (canvas.region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  if(x0 > x1) { int32_t t = x0; x0 = x1; x1 = t; }
  if(r0 > r1) { int32_t t = r0; r0 = r1; r1 = t; }
  if(x0 < 0) x0 = 0;
  if(x1 > SCREEN_WIDTH - 1) x1 = SCREEN_WIDTH - 1;
  if(r0 < PG_TOP_ROW) r0 = PG_TOP_ROW;
  if(r1 > regionBottom) r1 = regionBottom;
  canvas.clipX0 = (int16_t)x0; canvas.clipY0 = (int16_t)r0;
  canvas.clipX1 = (int16_t)x1; canvas.clipY1 = (int16_t)r1;
}
```


Pins that passed, in short: D1 horizontal line endpoints inclusive and nothing beyond; D2 vertical line; D3 diagonal endpoints and midpoint; D4 box corners and edges, interior clear, then filled; D5 circle cardinal points, center clear, then filled; D6 arc 0 to 90 degrees around a complex center lights (230,150), (200,180), (221,171) and not (200,120), (170,150), (179,129); D7 a line stops at the clip edge; D8 a line to column 5000 draws columns 0 to 399 with no error and no spill into the neighbour rows, a coordinate of 40000 is ERROR_OUT_OF_RANGE; D9 two inverts of a filled box restore three rows byte for byte; D10 for 1,000 sites the direct write and bitblt24 leave the same bytes, dirty flags included; D11 DISP 2 lights rows 40 to 59 only, TEXTOUT lights its cell; D12 a string coordinate is ERROR_INVALID_DATA_TYPE_FOR_OP and nothing is drawn; S1 the showcase has 10,500 lit pixels.

## Your task

You are auditing firmware code for bugs and design flaws. Report what you find. Do not fix anything. The design intent is stated in the Orientation; code that contradicts the stated intent is a finding, and code that contradicts your expectations but matches the intent is not.

Your one question: **is the kernel correct at its boundaries?** Walk the arithmetic: the mirrored bit mapping in `pgPixel` and `pgRun` for columns 0, 7, 8, 392, 399 and runs that start or end inside a byte; the masks of `pgRun` when `a` and `b` fall in the same byte and in adjacent bytes; the clamps of `pgRun` and `pgBox` for rectangles partly or wholly outside the clip; the Bresenham stepper for lines with |dx| < |dy|, for negative directions, and for a single point; the midpoint circle for r = 1, 2, 3 (double plots at the octant boundaries in invert mode); the filled circle's rounded half-width and whether outline and fill agree; the arc's span test for spans of 0, 90, 180, 270, 359 and 360 degrees, negative spans, and angles given in radians, grads and multiples of pi; `pgIsqrt` for 0, 1, and values near 2^31; the coordinate reader's limb read for a register whose data length is 8 or 16 bytes, for the value 32767, 32768, -32768, and for zero with sign tag LI_ZERO; the real path for 32767.9, -0.5, 1e30, NaN. For every arithmetic claim give the numbers.

## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing — a named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where (the function and the line of
the excerpt), the concrete reaching input (register values and the
command), the observable consequence, the violated contract quoted, and
your confidence. Rank by what the defect costs the owner. End with what you
considered and deliberately did not flag, and why.
