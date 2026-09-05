// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.c
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#include "pgmGraphics.h"
#include "c47.h"

static pgCanvas_t canvas;

// The window of §5.2. Zero at boot: no range set, a real is a pixel. Kept
// apart from pgCanvas_t because the header is read before realType.h.
static struct {
  uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
  real34_t xmin, xmax, ymin, ymax;
} pgWindow;

// Sends the changed rows to the LCD and stamps the time (DESIGN.md §8.4).
static void pgRefreshNow(void) {
  canvas.lastRefreshMs = getUptimeMs();
  #if defined(DMCP_BUILD)
    lcd_refresh_dma();
  #else // !DMCP_BUILD
    lcd_refresh();
  #endif // DMCP_BUILD
}

// Sets the region, the clip rectangle, and clears the region to white.
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
}

// PVIEW n: opens the canvas view over region n (DESIGN.md §3.5).
void fnPview(uint16_t region) {
  if(region != PG_REGION_REGISTERS && region != PG_REGION_FULL) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }
  if(calcMode != CM_GRAPHICS_CANVAS) {
    canvas.prevCalcMode = calcMode;
  }
  if(calcMode == CM_AIM) {   // the prologue of upstream's browsers: no cursor blinks into the canvas
    hideCursor();
    cursorEnabled = false;
  }
  clearSystemFlag(FLAG_ALPHA);
  pgSetRegion((uint8_t)region);
  calcMode = CM_GRAPHICS_CANVAS;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
  if(region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}

// ERASE: clears the canvas region. Opens the view over region 2 when closed (DESIGN.md §3.7).
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

// A program step inside the canvas view runs as in CM_NORMAL. From the
// keyboard the same items do nothing (DESIGN.md §3.6). The item functions
// that switch on calcMode switch on this value instead.
uint8_t pgEffectiveCalcMode(void) {
  if(calcMode == CM_GRAPHICS_CANVAS && programRunStop == PGM_RUNNING) {
    return CM_NORMAL;
  }
  return calcMode;
}

// The refreshScreen case for the canvas view: the status bar stays live,
// the softmenu is painted for region 2, an error shows on canvas line 1,
// nothing else is painted.
void pgRefreshCanvasView(void) {
  refreshStatusBar();
  if(canvas.region == PG_REGION_REGISTERS) {
    // The painter clears its band only when it paints; a blank base paints
    // nothing, so the band is cleared here first.
    lcd_fill_rect(0, PG_REGISTER_BOTTOM_ROW + 1, SCREEN_WIDTH, SCREEN_HEIGHT - PG_REGISTER_BOTTOM_ROW - 1, LCD_SET_VALUE);
    showSoftmenuCurrentPart();
  }
  if(lastErrorCode != ERROR_NONE) {
    lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, 20, LCD_SET_VALUE);
    showString(errorMessageOf(lastErrorCode), &standardFont, 1, PG_TOP_ROW, vmNormal, true, true);
    canvas.errorShown = 1;
  }
  else if(canvas.errorShown) {
    lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, 20, LCD_SET_VALUE);
    canvas.errorShown = 0;
  }
}

// EXIT in the canvas view: restore the previous mode and repaint (DESIGN.md §3.6).
void pgCloseView(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    return;
  }
  calcMode = canvas.prevCalcMode;
  canvas.region = 0;
  if(calcMode == CM_AIM) {   // the view took the cursor at PVIEW; alpha input gets it back
    setSystemFlag(FLAG_ALPHA);
    cursorEnabled = true;
  }
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(197);
}


// ---------------------------------------------------------------------------
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

// Integer square root in 64 bits: the fill of a radius up to 32767 needs 4 r^2,
// which is above INT32_MAX from r = 23171 on (audit G2 round 1, Sol 2).
static int64_t pgIsqrt(int64_t v) {
  int64_t r = 0, bit;
  if(v <= 0) return 0;
  for(bit = (int64_t)1 << 31; bit > 0; bit >>= 1) {
    if((r + bit) * (r + bit) <= v) r += bit;
  }
  return r;
}

// Circle outline or fill. (cx, cy) and the radius in screen coordinates (§4.5).
static void pgCircle(const pgRect_t *c, int32_t cx, int32_t cy, int32_t r, bool_t filled) {
  int32_t x, y, err;
  if(r < 0) r = -r;
  if(filled) {
    int32_t dy, dy0 = -r, dy1 = r;
    if(cy + dy0 < c->y0) dy0 = c->y0 - cy;   // only the rows inside the clip
    if(cy + dy1 > c->y1) dy1 = c->y1 - cy;
    for(dy = dy0; dy <= dy1; dy++) {
      int64_t rr = (int64_t)r * r - (int64_t)dy * dy;
      int32_t w = (int32_t)((pgIsqrt(4 * rr) + 1) / 2);   // rounded half-width, matches the midpoint outline
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
// span from A to B? A and B are direction vectors scaled by 65536. wide is
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

static void pgError(uint16_t code) {
  displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X);
}

#define PG_AXIS_X    0
#define PG_AXIS_Y    1
#define PG_AXIS_NONE 2   // a radius: pixels, rounded, never through the window

// A real through the window of its axis (§5.2), with the arithmetic of
// upstream's screenWindowRatio (plotstat.c): the ratio in 39 digits, then
// rounded half away from zero. Without a window the real is a pixel,
// rounded the same way. A result beyond 32767 is ERROR_OUT_OF_RANGE.
static bool_t pgRealToPixel(const real34_t *v34, uint8_t axis, int32_t *out) {
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

// Reads a coordinate from regist into *v: a long integer is a pixel, a real
// goes through the window of the axis. Returns false after an error.
static bool_t pgReadCoordAxis(calcRegister_t regist, uint8_t axis, int32_t *v) {
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

// Reads a complex register as a point: the real part through the x window,
// the imaginary part through the y window. Returns false after an error.
static bool_t pgReadComplexPoint(calcRegister_t regist, int32_t *x, int32_t *y) {
  if(getRegisterDataType(regist) != dtComplex34) {
    pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
    return false;
  }
  return pgRealToPixel(REGISTER_REAL34_DATA(regist), PG_AXIS_X, x) && pgRealToPixel(REGISTER_IMAG34_DATA(regist), PG_AXIS_Y, y);
}

// Reads a range end for XRNG and YRNG: a long integer or a real.
static bool_t pgReadReal(calcRegister_t regist, real34_t *out) {
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

// XRNG and YRNG: the minimum in Y, the maximum in X (§2.3). Equal ends are
// ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN and leave the window unchanged. A reversed range
// mirrors the axis. The window survives ERASE and PVIEW.
static void pgRange(uint8_t axis) {
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

// Reads an angle from regist into a real, in the current angular mode.
static bool_t pgReadAngle(calcRegister_t regist, real_t *angle) {
  switch(getRegisterDataType(regist)) {
    case dtLongInteger: convertLongIntegerRegisterToReal(regist, angle, &ctxtReal39); return true;
    case dtReal34:
      if(real34IsNaN(REGISTER_REAL34_DATA(regist)) || real34IsInfinite(REGISTER_REAL34_DATA(regist))) {
        pgError(ERROR_OUT_OF_RANGE);
        return false;
      }
      real34ToReal(REGISTER_REAL34_DATA(regist), angle);
      return true;
    default:            pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);                      return false;
  }
}

// Refresh cadence (§8.4).
static void pgRefreshMaybe(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    // Outside the view the drawing lives as a PIXEL drawing does (§4.2):
    // the manual flags keep the next refresh from repainting the stack
    // over it, and SNAP dumps the buffer as it is.
    screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
    screenHoldsDrawnPixels = true;
  }
  if(programRunStop != PGM_RUNNING) {
    pgRefreshNow();
    return;
  }
  if(getUptimeMs() - canvas.lastRefreshMs >= 40) {
    pgRefreshNow();
  }
}

// Reads the two points of a two-point command, y converted to rows: (X, Y)
// and (Z, T), or two complex points, the first in Y and the second in X
// (§5.1). A complex in one of X and Y without the other is a type error.
static bool_t pgReadTwoPoints(int32_t *x0, int32_t *r0, int32_t *x1, int32_t *r1) {
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
  if(!pgReadCoordAxis(REGISTER_X, PG_AXIS_X, &cx) || !pgReadCoordAxis(REGISTER_Y, PG_AXIS_Y, &cy) || !pgReadCoord(REGISTER_Z, &r)) return;
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
      // Same direction at the 65536 scale: a span of almost nothing, or of
      // almost a full turn. The exact span in degrees tells them apart.
      int32ToReal(180, &full);
      if(realCompareGreaterEqual(&d, &full)) {
        pgCircle(&c, cx, PG_ROW_OF(cy), r, false);
      }
      else {
        pgPixel(&c, cx + (int32_t)(((int64_t)ax * r) / 65536), PG_ROW_OF(cy + (int32_t)(((int64_t)ay * r) / 65536)));
      }
    }
    else {
      wide = cross < 0;
      pgArc(&c, cx, cy, r, ax, ay, bx, by, wide);
    }
  }
  pgRefreshMaybe();
}

// The largest glyph boundary at or below n, by a walk from the start: a
// byte at or above 0x80 starts a two-byte glyph, whatever its second byte
// holds. A lone lead byte at the end is not a glyph and is cut away.
static size_t pgGlyphBoundary(const char *s, size_t n) {
  size_t i = 0, last = 0;
  while(i < n && s[i] != 0) {
    last = i;
    i += ((uint8_t)s[i] & 0x80) ? 2 : 1;
  }
  return (i == n) ? n : last;
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
  n = pgGlyphBoundary(s, n);
  memcpy(tmpString, s, n);
  tmpString[n] = 0;
  while(tmpString[0] != 0 && stringWidth(tmpString, &standardFont, true, true) > width) {
    // remove the last glyph: a byte at or above 0x80 starts a two-byte
    // glyph, and the boundary cut above guarantees a whole last glyph
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
  {
    int32_t col = c.x0 < 1 ? 1 : c.x0;
    if(col > c.x1) {
      return;
    }
    if(!pgStringCut(REGISTER_X, (uint32_t)(c.x1 - col + 1))) return;
    lcd_fill_rect((uint32_t)c.x0, (uint32_t)row, (uint32_t)(c.x1 - c.x0 + 1), 20, LCD_SET_VALUE);
    showString(tmpString, &standardFont, (uint32_t)col, (uint32_t)row, vmNormal, true, true);
  }
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

#if defined(TESTSUITE_BUILD)
  #include <stdio.h>
  #include <math.h>

  static uint32_t pgTestFailures;

  static void pgTestFail(const char *what) {
    pgTestFailures++;
    printf("program-graphics test FAIL: %s\n", what);
  }

  // Writes value into regist as a long integer.
  static void pgTestWriteLonI(calcRegister_t regist, uint32_t value) {
    longInteger_t li;
    longIntegerInit(li);
    uInt32ToLongInteger(value, li);
    convertLongIntegerToLongIntegerRegister(li, regist);
    longIntegerFree(li);
  }

  void pgTestSmoke(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Runs count steps of item through reallyRunFunction with the program
  // state set to running, and returns the elapsed milliseconds.
  static uint32_t pgTestRunSteps(int16_t item, uint32_t count) {
    const uint8_t savedRunStop = programRunStop;
    uint32_t start, end;
    programRunStop = PGM_RUNNING;
    start = getUptimeMs();
    for(uint32_t i = 0; i < count; i++) {
      reallyRunFunction(item, NOPARAM);
    }
    end = getUptimeMs();
    programRunStop = savedRunStop;
    return end - start;
  }

  void pgTestBaseline(uint16_t unusedButMandatoryParameter) {
    const uint32_t count = 1000000;
    uint32_t nopMs, pixelMs;
    pgTestFailures = 0;
    pgTestWriteLonI(REGISTER_X, 200);   // PIXEL reads x from X
    pgTestWriteLonI(REGISTER_Y, 100);   // and y from Y
    nopMs   = pgTestRunSteps(ITM_NOP,   count);
    pixelMs = pgTestRunSteps(ITM_PIXEL, count);
    if(lastErrorCode != ERROR_NONE) {
      pgTestFail("baseline: an error code is set after the PIXEL loop");
      lastErrorCode = ERROR_NONE;
    }
    if(!lcd_buffer_pixel_on(200, SCREEN_HEIGHT - 100 - 1)) {
      pgTestFail("baseline: PIXEL 200,100 did not light row 139 column 200");
    }
    printf("program-graphics baseline: %u steps, NOP %u ms, PIXEL %u ms, PIXEL body %u ms\n",
           count, nopMs, pixelMs, pixelMs > nopMs ? pixelMs - nopMs : 0);
    {
      // Stage G2: LINE of 100 pixels, 100,000 times (TESTING.md §5).
      uint32_t lineMs;
      pgTestWriteLonI(REGISTER_X, 0);   pgTestWriteLonI(REGISTER_Y, 0);
      pgTestWriteLonI(REGISTER_Z, 100); pgTestWriteLonI(REGISTER_T, 50);
      lineMs = pgTestRunSteps(ITM_GLINE, 100000);
      printf("program-graphics baseline: 100000 LINE steps of 100 pixels, %u ms\n", lineMs);
    }
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Stage G1 pins of TESTING.md §4: the canvas view.
  void pgTestView(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;

    // V1: PVIEW 2 enters mode 21 and clears rows 20 to 170 only.
    setBlackPixel(10, 100);
    setBlackPixel(10, 200);
    fnPview(2);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V1 PVIEW 2 did not enter the canvas view");
    if(lcd_buffer_pixel_on(10, 100))     pgTestFail("V1 PVIEW 2 left a pixel lit in the register region");
    if(canvas.clipY1 != PG_REGISTER_BOTTOM_ROW) pgTestFail("V1 PVIEW 2 did not clip at row 170");
    // The softmenu painter owns rows 171 to 239 in region 2 and repaints
    // them, so a pixel there proves nothing. The clip row is the pin.

    // V2: PVIEW 6 clears rows 20 to 239.
    setBlackPixel(10, 200);
    fnPview(6);
    if(lcd_buffer_pixel_on(10, 200))     pgTestFail("V2 PVIEW 6 left a pixel lit in the softmenu region");
    if(canvas.clipY1 != SCREEN_HEIGHT - 1) pgTestFail("V2 PVIEW 6 did not clip at row 239");

    // V3: PVIEW 3 is an error and changes nothing.
    fnPview(3);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("V3 PVIEW 3 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V3 PVIEW 3 changed the mode");

    // V4: the stop path repaint keeps the canvas.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(!lcd_buffer_pixel_on(50, 100))    pgTestFail("V4 refreshScreen in the canvas view erased the drawing");

    // V8: the same repaint keeps the status bar live. The band is cleared
    // first, so only a repaint can light it again.
    lcd_fill_rect(0, 0, SCREEN_WIDTH, PG_TOP_ROW, LCD_SET_VALUE);
    refreshScreen(4);
    {
      bool_t lit = false;
      for(uint32_t x = 0; x < SCREEN_WIDTH && !lit; x++) {
        for(uint32_t y = 0; y < PG_TOP_ROW && !lit; y++) {
          lit = lcd_buffer_pixel_on(x, y);
        }
      }
      if(!lit)                           pgTestFail("V8 refreshScreen in the canvas view did not repaint the status bar");
    }

    // V5: VIEW inside the view paints nothing over the canvas.
    {
      const uint8_t savedRunStop = programRunStop;
      programRunStop = PGM_RUNNING;
      fnView(REGISTER_X);
      programRunStop = savedRunStop;
    }
    if(!lcd_buffer_pixel_on(50, 100))    pgTestFail("V5 VIEW inside the canvas view erased the drawing");
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V5 VIEW changed the mode");

    // V6: EXIT restores the previous mode.
    pgCloseView();
    if(calcMode != CM_NORMAL)            pgTestFail("V6 pgCloseView did not restore CM_NORMAL");

    // V7: without PVIEW, the normal repaint erases a drawing, as upstream does today.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(lcd_buffer_pixel_on(50, 100))     pgTestFail("V7 a drawing without PVIEW survived the normal repaint");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    // V9: in region 2 the softmenu band is cleared before the painter runs,
    // so a menu popped to a blank base leaves no labels (audit G2 round 1,
    // in-family 5).
    {
      bool_t home = getSystemFlag(FLAG_BASE_HOME), mym = getSystemFlag(FLAG_BASE_MYM);
      clearSystemFlag(FLAG_BASE_HOME); clearSystemFlag(FLAG_BASE_MYM);
      calcMode = CM_NORMAL;
      fnPview(2);
      fnExitAllMenus(NOPARAM);
      setBlackPixel(100, 200);
      pgRefreshCanvasView();
      if(lcd_buffer_pixel_on(100, 200)) pgTestFail("V9 the softmenu band kept its old pixels on a blank base");
      if(home) setSystemFlag(FLAG_BASE_HOME);
      if(mym)  setSystemFlag(FLAG_BASE_MYM);
    }
    pgCloseView();
    calcMode = CM_NORMAL;

    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Stage G1 pins of TESTING.md §4: the keys in the canvas view.
  void pgTestKeys(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    fnPview(2);

    // K1: the mode sits inside the range that the three softkey gates block.
    if(!(CM_GRAPHICS_CANVAS >= 19 && CM_GRAPHICS_CANVAS <= 23)) {
      pgTestFail("K1 CM_GRAPHICS_CANVAS is outside the package browser range 19 to 23");
    }

    // K2: direct keys other than EXIT and R/S change nothing. Each key is
    // pressed, then released through its item function, as the keyboard
    // does. The guard arm marks a digit processed at the press. ENTER,
    // BACKSPACE, UP, DOWN, and .d reach their key function at the release.
    setBlackPixel(60, 100);
    processKeyAction(ITM_ENTER);     runFunction(ITM_ENTER);
    processKeyAction(ITM_1);
    processKeyAction(ITM_UP1);       runFunction(ITM_UP1);
    processKeyAction(ITM_DOWN1);     runFunction(ITM_DOWN1);
    processKeyAction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
    processKeyAction(ITM_dotD);      runFunction(ITM_dotD);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K2 a key left the canvas view");
    if(!lcd_buffer_pixel_on(60, 100))    pgTestFail("K2 a key erased the canvas");
    if(lastErrorCode != ERROR_NONE)      pgTestFail("K2 a key raised an error");
    lastErrorCode = ERROR_NONE;

    // K4: a program step that resets the mode through calcModeNormal, such
    // as CLSTK, leaves the view open and the drawing intact at the next
    // repaint (audit G1 round 1, finding S2).
    runFunction(ITM_CLSTK);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K4 CLSTK closed the canvas view");
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(!lcd_buffer_pixel_on(60, 100))    pgTestFail("K4 the repaint after CLSTK erased the canvas");
    lastErrorCode = ERROR_NONE;

    // K5: ENTER as a program step inside the view lifts the stack (audit
    // G1 round 1, finding G1R1-1).
    {
      const uint8_t savedRunStop = programRunStop;
      longInteger_t li; int32_t y = 0;
      pgTestWriteLonI(REGISTER_X, 7);
      pgTestWriteLonI(REGISTER_Y, 3);
      programRunStop = PGM_RUNNING;
      runFunction(ITM_ENTER);
      programRunStop = savedRunStop;
      if(getRegisterDataType(REGISTER_Y) == dtLongInteger) {
        convertLongIntegerRegisterToLongInteger(REGISTER_Y, li);
        longIntegerToInt32(li, y);
        longIntegerFree(li);
      }
      if(y != 7)                         pgTestFail("K5 ENTER as a program step did not copy X into Y");
      if(calcMode != CM_GRAPHICS_CANVAS) pgTestFail("K5 ENTER as a program step changed the mode");
    }

    // K6: CC and .ms from the keyboard inside the view do nothing and show
    // no bug screen (finding G1R1-3).
    runFunction(ITM_CC);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K6 CC from the keyboard left the canvas view");
    runFunction(ITM_ms);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K6 .ms from the keyboard left the canvas view");
    lastErrorCode = ERROR_NONE;

    // K7: an error inside the view shows on canvas line 1 at the next
    // refresh, and the EXIT press that clears it does not paint the Z line
    // band over the canvas (finding G1R1-4).
    setBlackPixel(80, 80);
    fnPview(3);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("K7 PVIEW 3 did not raise the error");
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    {
      bool_t lit = false;
      for(uint32_t x = 0; x < SCREEN_WIDTH && !lit; x++) {
        for(uint32_t yy = PG_TOP_ROW; yy < PG_TOP_ROW + 20 && !lit; yy++) {
          lit = lcd_buffer_pixel_on(x, yy);
        }
      }
      if(!lit)                           pgTestFail("K7 the error text did not appear on canvas line 1");
    }
    if(!lcd_buffer_pixel_on(80, 80))     pgTestFail("K7 the error refresh erased the canvas");
    processKeyAction(ITM_EXIT1);
    if(lastErrorCode != ERROR_NONE)      pgTestFail("K7 the EXIT press did not clear the error");
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K7 the EXIT press with an error pending closed the view");
    if(!lcd_buffer_pixel_on(80, 80))     pgTestFail("K7 the EXIT press painted the Z line over the canvas");
    if(lcd_buffer_pixel_on(1, PG_TOP_ROW + 10)) pgTestFail("K7 the error band was not cleared");
    if(canvas.errorShown)                     pgTestFail("K7 the error flag was not reset");

    // K3: EXIT closes the view. The press does nothing for this mode. The
    // release runs the EXIT item, whose function is fnKeyExit.
    processKeyAction(ITM_EXIT1);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K3 the EXIT press alone left the canvas view");
    runFunction(ITM_EXIT1);
    if(calcMode != CM_NORMAL)            pgTestFail("K3 EXIT did not close the canvas view");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    // K8: the view opened from alpha input mode takes the cursor and clears
    // FLAG_ALPHA, as upstream's browsers do; EXIT gives them back (audit G2
    // round 1, in-family 4).
    pgCloseView();
    calcMode = CM_NORMAL;
    calcModeAim(NOPARAM);
    fnPview(6);
    if(cursorEnabled)                pgTestFail("K8 the cursor stays enabled in the view");
    if(getSystemFlag(FLAG_ALPHA))    pgTestFail("K8 FLAG_ALPHA stays set in the view");
    if(canvas.prevCalcMode != CM_AIM) pgTestFail("K8 the view did not record alpha input mode");
    pgCloseView();
    if(calcMode != CM_AIM || !cursorEnabled || !getSystemFlag(FLAG_ALPHA)) pgTestFail("K8 EXIT did not give alpha input its cursor back");
    calcModeNormal();
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;

    // K9: a key press in the view paints no function name, and the release
    // repaints no register line over the canvas (audit G2 round 1, U7). The
    // two paint sites are driven directly; the item still arms and disarms.
    fnPview(6);
    lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, 40, LCD_SET_VALUE);
    setBlackPixel(100, 39);
    setBlackPixel(100, 25);
    showFunctionName(ITM_ENTER, 1000, NULL);
    if(showFunctionNameItem != ITM_ENTER) pgTestFail("K9 the press did not arm the item");
    {
      uint32_t lit = 0, x, yy;
      for(x = 0; x < SCREEN_WIDTH; x++) for(yy = PG_TOP_ROW; yy < PG_TOP_ROW + 40; yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
      if(lit != 2) pgTestFail("K9 the press painted a function name over the canvas");
    }
    hideFunctionName();
    if(!lcd_buffer_pixel_on(100, 39) || !lcd_buffer_pixel_on(100, 25)) pgTestFail("K9 the release repainted the register line over the canvas");
    if(showFunctionNameItem != 0) pgTestFail("K9 the release did not disarm the item");

    // K10: a real softkey press in the view changes nothing. The CANVAS
    // menu is pushed so that softkey 2 would run ERASE if the gate let it
    // through (audit G2 round 1, in-family 12).
    showSoftmenu(-MNU_CANVAS);
    setBlackPixel(100, 100);
    {
      GdkEventButton ev;
      memset(&ev, 0, sizeof(ev));
      ev.type = GDK_BUTTON_PRESS;
      btnFnPressed(NULL, (GdkEvent *)&ev, "2");
      ev.type = GDK_BUTTON_RELEASE;
      btnFnReleased(NULL, (GdkEvent *)&ev, "2");
      btnFnClicked(NULL, "2");   // what the double-tap timer does at its timeout (keyboardTweak.c execFnTimeout)
    }
    if(calcMode != CM_GRAPHICS_CANVAS) pgTestFail("K10 a softkey press left the view");
    if(!lcd_buffer_pixel_on(100, 100)) pgTestFail("K10 a softkey press ran its function in the view");
    if(lastErrorCode != ERROR_NONE) { pgTestFail("K10 a softkey press raised an error"); lastErrorCode = ERROR_NONE; }
    pgCloseView();
    popSoftmenu();
    calcMode = CM_NORMAL;

    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // ---- Stage G2 pins (TESTING.md §4, D1 to D12) ----

  // Writes a signed value into regist as a long integer.
  static void pgTestWriteLonISigned(calcRegister_t regist, int32_t value) {
    longInteger_t li;
    longIntegerInit(li);
    int32ToLongInteger(value, li);
    convertLongIntegerToLongIntegerRegister(li, regist);
    longIntegerFree(li);
  }

  static void pgTestWriteReal(calcRegister_t regist, const char *text) {
    reallocateRegister(regist, dtReal34, 0, amNone);
    stringToReal34(text, REGISTER_REAL34_DATA(regist));
  }

  static void pgTestSetString(calcRegister_t regist, const char *s) {
    reallocateRegister(regist, dtString, TO_BLOCKS(strlen(s) + 1), amNone);
    strcpy(REGISTER_STRING_DATA(regist), s);
  }

  static void pgTestSetComplex(calcRegister_t regist, int32_t re, int32_t im) {
    reallocateRegister(regist, dtComplex34, 0, amNone);
    int32ToReal34(re, REGISTER_REAL34_DATA(regist));
    int32ToReal34(im, REGISTER_IMAG34_DATA(regist));
  }

  static void pgTestPoints(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {   // user coordinates
    pgTestWriteLonI(REGISTER_X, (uint32_t)x0);  // the writer takes unsigned; the pins use positive values
    pgTestWriteLonI(REGISTER_Y, (uint32_t)y0);
    pgTestWriteLonI(REGISTER_Z, (uint32_t)x1);
    pgTestWriteLonI(REGISTER_T, (uint32_t)y1);
  }

  static bool_t pgTestLit(int32_t x, int32_t yUser) {
    return lcd_buffer_pixel_on((uint32_t)x, (uint32_t)PG_ROW_OF(yUser));
  }

  // One drawing command with the view closed: the drawing must survive a
  // refresh, as a PIXEL drawing does (audit G2 round 1, in-family 1).
  static bool_t pgTestAnyLit(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {   // user coordinates, inclusive box
    int32_t x, y;
    for(x = x0; x <= x1; x++) for(y = y0; y <= y1; y++) if(pgTestLit(x, y)) return true;
    return false;
  }

  static void pgTestClosedView(void (*command)(uint16_t), uint16_t param, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *what) {
    screenUpdatingMode = SCRUPD_AUTO;
    screenHoldsDrawnPixels = false;
    lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
    command(param);
    if(!screenHoldsDrawnPixels || !(screenUpdatingMode & SCRUPD_MANUAL_STACK)) { printf("program-graphics test FAIL: D20 %s did not set the PIXEL flags outside the view\n", what); pgTestFailures++; }
    if(!pgTestAnyLit(x0, y0, x1, y1)) { printf("program-graphics test FAIL: D20 %s did not draw outside the view\n", what); pgTestFailures++; }
    refreshScreen(0);
    if(!pgTestAnyLit(x0, y0, x1, y1)) { printf("program-graphics test FAIL: D20 the refresh erased the %s drawing outside the view\n", what); pgTestFailures++; }
    screenUpdatingMode = SCRUPD_AUTO;
    screenHoldsDrawnPixels = false;
  }

  void pgTestDraw2D(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    pgWindow.set = 0;
    pgTestPoints(50, 100, 80, 100);
    pgTestClosedView(fnGline, NOPARAM, 60, 100, 60, 100, "LINE");
    pgTestClosedView(fnGfbox, NOPARAM, 60, 100, 60, 100, "FBOX");
    pgTestWriteLonI(REGISTER_X, 200); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 20);
    pgTestClosedView(fnGfcircle, NOPARAM, 200, 100, 200, 100, "FCIRCL");
    pgTestSetString(REGISTER_Z, "W"); pgTestWriteLonI(REGISTER_X, 100); pgTestWriteLonI(REGISTER_Y, 150);
    pgTestClosedView(fnGtextout, NOPARAM, 100, 131, 119, 150, "TEXTOUT");   // the 20-row cell of the glyph
    lastErrorCode = ERROR_NONE;
    fnPview(6);
    fnGmode(0);

    // D1: a horizontal line, endpoints inclusive, nothing beyond them.
    pgTestPoints(10, 10, 50, 10);
    fnGline(NOPARAM);
    if(!pgTestLit(10, 10) || !pgTestLit(50, 10) || !pgTestLit(30, 10)) pgTestFail("D1 the horizontal line misses a pixel");
    if(pgTestLit(9, 10) || pgTestLit(51, 10) || pgTestLit(30, 11) || pgTestLit(30, 9)) pgTestFail("D1 the horizontal line has a pixel beyond an endpoint or off its row");

    // D2: a vertical line.
    pgTestPoints(70, 20, 70, 60);
    fnGline(NOPARAM);
    if(!pgTestLit(70, 20) || !pgTestLit(70, 60) || !pgTestLit(70, 40)) pgTestFail("D2 the vertical line misses a pixel");
    if(pgTestLit(70, 19) || pgTestLit(70, 61) || pgTestLit(71, 40) || pgTestLit(69, 40)) pgTestFail("D2 the vertical line has a pixel beyond an endpoint or off its column");

    // D3: a diagonal line, both endpoints and the midpoint.
    pgTestPoints(100, 100, 140, 120);
    fnGline(NOPARAM);
    if(!pgTestLit(100, 100) || !pgTestLit(140, 120) || !pgTestLit(120, 110)) pgTestFail("D3 the diagonal line misses an endpoint or its midpoint");

    // D4: a box outline, then a filled box.
    pgTestPoints(200, 50, 260, 90);
    fnGbox(NOPARAM);
    if(!pgTestLit(200, 50) || !pgTestLit(260, 90) || !pgTestLit(230, 50) || !pgTestLit(200, 70)) pgTestFail("D4 the box outline misses a corner or an edge");
    if(pgTestLit(230, 70)) pgTestFail("D4 the box outline lit its interior");
    fnGfbox(NOPARAM);
    if(!pgTestLit(230, 70) || !pgTestLit(201, 51) || !pgTestLit(259, 89)) pgTestFail("D4 the filled box left the interior clear");

    // D5: a circle outline, then a filled circle.
    pgTestWriteLonI(REGISTER_X, 300); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 20);
    fnGcircle(NOPARAM);
    if(!pgTestLit(320, 100) || !pgTestLit(280, 100) || !pgTestLit(300, 120) || !pgTestLit(300, 80)) pgTestFail("D5 the circle misses a cardinal point");
    if(pgTestLit(300, 100) || pgTestLit(321, 100)) pgTestFail("D5 the circle lit its center or a pixel beyond its radius");
    fnGfcircle(NOPARAM);
    if(!pgTestLit(300, 100) || !pgTestLit(310, 105)) pgTestFail("D5 the filled circle left the interior clear");

    // D6: an arc from 0 to 90 degrees around a complex center.
    {
      const angularMode_t savedAm = currentAngularMode;
      currentAngularMode = amDegree;
      pgTestSetComplex(REGISTER_T, 200, 150);
      pgTestWriteLonI(REGISTER_Z, 30);
      pgTestWriteLonI(REGISTER_Y, 0);
      pgTestWriteLonI(REGISTER_X, 90);
      fnGarc(NOPARAM);
      currentAngularMode = savedAm;
      if(!pgTestLit(230, 150) || !pgTestLit(200, 180) || !pgTestLit(221, 171)) pgTestFail("D6 the arc misses a point of its quarter");
      if(pgTestLit(200, 120) || pgTestLit(170, 150) || pgTestLit(179, 129)) pgTestFail("D6 the arc drew outside its quarter");
      if(lastErrorCode != ERROR_NONE) { pgTestFail("D6 the arc raised an error"); lastErrorCode = ERROR_NONE; }
    }

    // D7: the clip rectangle stops a line at its edge.
    pgTestPoints(0, 0, 199, 239);
    fnGclip(NOPARAM);
    pgTestPoints(100, 30, 300, 30);
    fnGline(NOPARAM);
    if(!pgTestLit(150, 30)) pgTestFail("D7 the clipped line lost a pixel inside the clip rectangle");
    if(pgTestLit(250, 30)) pgTestFail("D7 the line crossed the clip edge");
    fnErase(NOPARAM);   // resets the clip to the region

    // D8: far off-screen endpoints draw only the on-screen part, without an
    // error, up to the 32767 limit. Beyond the limit the command refuses.
    pgTestWriteLonI(REGISTER_X, 0); pgTestWriteLonI(REGISTER_Y, 30); pgTestWriteLonI(REGISTER_Z, 5000); pgTestWriteLonI(REGISTER_T, 30);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D8 a coordinate of 5000 raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 30) || !pgTestLit(399, 30)) pgTestFail("D8 the line to column 5000 misses an edge pixel");
    if(pgTestLit(399, 29) || pgTestLit(390, 29) || pgTestLit(0, 31)) pgTestFail("D8 the run spilled into a neighbour row");
    pgTestWriteLonI(REGISTER_Y, 31); pgTestWriteLonI(REGISTER_Z, 40000); pgTestWriteLonI(REGISTER_T, 31);   // a row nothing has lit
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("D8 a coordinate of 40000 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    if(pgTestLit(200, 31) || pgTestLit(0, 31)) pgTestFail("D8 the refused command drew");
    // D8c: a negative start column is clamped at the left edge; nothing
    // spills into the next row's bytes (the mirrored layout puts the
    // right end of the row below right after this row's left end).
    pgTestWriteLonISigned(REGISTER_X, -20); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 50); pgTestWriteLonI(REGISTER_T, 100);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D8c a negative column raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 100) || !pgTestLit(50, 100)) pgTestFail("D8c the clamped line misses an edge pixel");
    if(pgTestLit(399, 99) || pgTestLit(392, 99) || pgTestLit(399, 101)) pgTestFail("D8c the run spilled into the row below or above");
    if(pgRowPtr(PG_ROW_OF(99))[0] > 1) pgTestFail("D8c the row below has a corrupted dirty flag");

    // D9: GMODE 2 twice restores the buffer.
    {
      uint8_t before[PG_ROW_BYTES * 3];
      pgTestPoints(50, 150, 120, 152);
      memcpy(before, pgRowPtr(PG_ROW_OF(152)), sizeof(before));
      fnGmode(2);
      fnGfbox(NOPARAM);
      if(!pgTestLit(60, 151)) pgTestFail("D9 the first invert did not light a clear pixel");
      fnGfbox(NOPARAM);
      if(memcmp(before, pgRowPtr(PG_ROW_OF(152)), sizeof(before)) != 0) pgTestFail("D9 two inverts did not restore the three rows");
      fnGmode(0);
    }

    // D10: the direct write and bitblt24 leave the same bytes for 1,000
    // pseudo-random sites, dirty flags included. Each site is a vertical
    // two-pixel line (the pixel path) and a horizontal three-pixel run on
    // another row (the run path).
    {
      static uint8_t mine[SCREEN_HEIGHT * PG_ROW_BYTES];
      uint32_t seed, i;
      lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
      for(i = 0; i < SCREEN_HEIGHT; i++) pgRowPtr((int32_t)i)[0] = 0;
      seed = 12345;
      for(i = 0; i < 1000; i++) {
        uint32_t x, y;
        seed = seed * 1103515245u + 12345u;
        x = (seed >> 8) % (SCREEN_WIDTH - 4);
        y = 4 + (seed >> 20) % 210;                       // rows 25 to 235 as y
        pgTestWriteLonI(REGISTER_X, x); pgTestWriteLonI(REGISTER_Y, y); pgTestWriteLonI(REGISTER_Z, x); pgTestWriteLonI(REGISTER_T, y + 1);
        fnGline(NOPARAM);                                  // vertical: pgPixel twice
        pgTestWriteLonI(REGISTER_Y, y - 3); pgTestWriteLonI(REGISTER_Z, x + 2); pgTestWriteLonI(REGISTER_T, y - 3);
        fnGline(NOPARAM);                                  // horizontal: pgRun once
      }
      memcpy(mine, lcd_buffer, sizeof(mine));
      lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
      for(i = 0; i < SCREEN_HEIGHT; i++) pgRowPtr((int32_t)i)[0] = 0;
      seed = 12345;
      for(i = 0; i < 1000; i++) {
        uint32_t x, y;
        seed = seed * 1103515245u + 12345u;
        x = (seed >> 8) % (SCREEN_WIDTH - 4);
        y = 4 + (seed >> 20) % 210;
        setBlackPixel(x, (uint32_t)PG_ROW_OF((int32_t)y));
        setBlackPixel(x, (uint32_t)PG_ROW_OF((int32_t)y + 1));
        setBlackPixel(x,     (uint32_t)PG_ROW_OF((int32_t)y - 3));
        setBlackPixel(x + 1, (uint32_t)PG_ROW_OF((int32_t)y - 3));
        setBlackPixel(x + 2, (uint32_t)PG_ROW_OF((int32_t)y - 3));
      }
      if(memcmp(mine, lcd_buffer, sizeof(mine)) != 0) pgTestFail("D10 the direct write and bitblt24 differ in the buffer");
    }

    // D11: DISP 2 and TEXTOUT.
    fnErase(NOPARAM);
    pgTestSetString(REGISTER_X, "HELLO");
    fnGdisp(2);
    {
      bool_t band = false, above = false;
      uint32_t x, yy;
      for(x = 0; x < 100 && !band; x++) for(yy = 40; yy < 60 && !band; yy++) band = lcd_buffer_pixel_on(x, yy);
      for(x = 0; x < 100 && !above; x++) for(yy = 20; yy < 40 && !above; yy++) above = lcd_buffer_pixel_on(x, yy);
      if(!band) pgTestFail("D11 DISP 2 did not light its line");
      if(above) pgTestFail("D11 DISP 2 lit the line above");
    }
    pgTestSetString(REGISTER_Z, "Hi");
    pgTestWriteLonI(REGISTER_X, 50); pgTestWriteLonI(REGISTER_Y, 100);
    fnGtextout(NOPARAM);
    {
      bool_t cell = false;
      uint32_t x, yy;
      for(x = 50; x < 80 && !cell; x++) for(yy = 139; yy < 159 && !cell; yy++) cell = lcd_buffer_pixel_on(x, yy);
      if(!cell) pgTestFail("D11 TEXTOUT did not light its cell");
    }

    // D12: a string where a coordinate is expected raises the data type error and draws nothing.
    pgTestSetString(REGISTER_X, "X");
    pgTestWriteLonI(REGISTER_Y, 200); pgTestWriteLonI(REGISTER_Z, 300); pgTestWriteLonI(REGISTER_T, 200);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) pgTestFail("D12 a string coordinate did not raise the data type error");
    lastErrorCode = ERROR_NONE;
    if(pgTestLit(300, 200)) pgTestFail("D12 the command drew after the error");

    // D13: a clip rectangle wholly outside the region is empty, on each of
    // the four sides. A full-screen FBOX then draws nothing, and the stored
    // clip stays inside the region (audit G2 round 1, Sol 1).
    {
      static const int32_t outside[4][4] = {
        { 0, 32766, 399, 32767 },       // above the region
        { 0, -32767, 399, -32766 },     // below the region (Sol's case)
        { -32767, 0, -32766, 219 },     // left of the screen
        { 32766, 0, 32767, 219 },       // right of the screen
      };
      uint32_t side;
      for(side = 0; side < 4; side++) {
        fnErase(NOPARAM);
        pgTestWriteLonISigned(REGISTER_X, outside[side][0]); pgTestWriteLonISigned(REGISTER_Y, outside[side][1]);
        pgTestWriteLonISigned(REGISTER_Z, outside[side][2]); pgTestWriteLonISigned(REGISTER_T, outside[side][3]);
        fnGclip(NOPARAM);
        if(lastErrorCode != ERROR_NONE) { pgTestFail("D13 an outside clip rectangle raised an error"); lastErrorCode = ERROR_NONE; }
        if(canvas.clipX0 < 0 || canvas.clipX1 > SCREEN_WIDTH - 1 || canvas.clipY0 < PG_TOP_ROW || canvas.clipY1 > SCREEN_HEIGHT - 1) pgTestFail("D13 a stored clip edge is outside the region");
        pgTestPoints(0, 0, 399, 219);
        fnGfbox(NOPARAM);
        if(pgTestLit(0, 0) || pgTestLit(200, 100) || pgTestLit(399, 219) || pgTestLit(0, 219) || pgTestLit(399, 0)) pgTestFail("D13 a full-screen FBOX drew through an empty clip");
      }
      fnErase(NOPARAM);
    }

    // D14: filled circles with a large radius (audit G2 round 1, Sol 2).
    // A radius of 23170 far off screen paints nothing on the screen. A
    // radius of 32767 around an on-screen center paints the whole region.
    pgTestWriteLonISigned(REGISTER_X, -30000); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 23170);
    fnGfcircle(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D14 a radius of 23170 raised an error"); lastErrorCode = ERROR_NONE; }
    if(pgTestLit(0, 100) || pgTestLit(200, 100) || pgTestLit(399, 100)) pgTestFail("D14 the off-screen circle of radius 23170 painted an on-screen row");
    pgTestWriteLonI(REGISTER_X, 200); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 32767);
    fnGfcircle(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D14 a radius of 32767 raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 0) || !pgTestLit(399, 219) || !pgTestLit(0, 219) || !pgTestLit(399, 0)) pgTestFail("D14 the circle of radius 32767 left a corner clear");
    fnErase(NOPARAM);

    // D15: DISP clears and writes only between the clip columns (audit G2
    // round 1, Sol 3). A pixel left of the clip survives, and the text
    // starts at the left clip edge, not at column 1.
    pgTestPoints(5, 190, 15, 190);   // buffer row 49, inside the band of line 2
    fnGline(NOPARAM);
    pgTestPoints(200, 0, 399, 219);
    fnGclip(NOPARAM);
    pgTestSetString(REGISTER_X, "HELLO");
    fnGdisp(2);
    {
      bool_t inside = false, left = false;
      uint32_t x, yy;
      for(x = 200; x < 260 && !inside; x++) for(yy = 40; yy < 60 && !inside; yy++) inside = lcd_buffer_pixel_on(x, yy);
      for(x = 20; x < 200 && !left; x++) for(yy = 40; yy < 60 && !left; yy++) left = lcd_buffer_pixel_on(x, yy);
      if(!pgTestLit(10, 190)) pgTestFail("D15 DISP cleared a pixel left of the clip");
      if(!inside) pgTestFail("D15 DISP did not write inside the clip");
      if(left) pgTestFail("D15 DISP wrote left of the clip");
    }
    fnErase(NOPARAM);
    // D15b: DISP clears its band inside the clip before it writes (audit G2
    // round 1, in-family 8). A pixel in the band goes, one row above stays.
    pgTestPoints(100, 170, 100, 170); fnGline(NOPARAM);   // buffer row 69, inside the band of line 3
    pgTestPoints(100, 180, 100, 180); fnGline(NOPARAM);   // buffer row 59, one row above the band
    pgTestSetString(REGISTER_X, "X");
    fnGdisp(3);
    if(pgTestLit(100, 170))  pgTestFail("D15b DISP did not clear its band");
    if(!pgTestLit(100, 180)) pgTestFail("D15b DISP cleared the row above its band");
    fnErase(NOPARAM);

    // D16: an arc of 0.05 degrees at radius 5000 keeps its span (audit G2
    // round 1, Sol 4). The center is off screen so that the arc crosses
    // the screen at column 300, rows 130 to 134.
    {
      const angularMode_t savedAm = currentAngularMode;
      currentAngularMode = amDegree;
      pgTestSetComplex(REGISTER_T, -4700, 130);
      pgTestWriteLonI(REGISTER_Z, 5000);
      pgTestWriteLonI(REGISTER_Y, 0);
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
      stringToReal34("0.05", REGISTER_REAL34_DATA(REGISTER_X));
      fnGarc(NOPARAM);
      currentAngularMode = savedAm;
      if(lastErrorCode != ERROR_NONE) { pgTestFail("D16 the small arc raised an error"); lastErrorCode = ERROR_NONE; }
      if(!pgTestLit(300, 130) || !pgTestLit(300, 133)) pgTestFail("D16 the arc of 0.05 degrees lost its span");
      if(pgTestLit(300, 140) || pgTestLit(300, 120)) pgTestFail("D16 the arc of 0.05 degrees drew outside its span");
    }

    // D17: the string cap never cuts inside a two-byte glyph.
    {
      static char longString[TMP_STR_LENGTH + 64];
      size_t i;
      for(i = 0; i < TMP_STR_LENGTH - 3; i++) longString[i] = 'A';
      longString[TMP_STR_LENGTH - 3] = (char)0x80;   // a two-byte glyph starts at the cap
      longString[TMP_STR_LENGTH - 2] = 'B';
      for(i = TMP_STR_LENGTH - 1; i < TMP_STR_LENGTH + 30; i++) longString[i] = 'C';
      longString[TMP_STR_LENGTH + 30] = 0;
      pgTestSetString(REGISTER_X, longString);
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17 the string cut refused a long string");
      if(strlen(tmpString) != TMP_STR_LENGTH - 3 || (uint8_t)tmpString[TMP_STR_LENGTH - 4] != 'A') pgTestFail("D17 the string cap cut inside a two-byte glyph");
      // D17b: a string that ends in a lone lead byte is trimmed to fit a
      // width of one pixel, and the canary bytes after the NUL survive: the
      // boundary cut removes the lone byte before the walk (audit G2 round
      // 1, in-family 10). Red under the old cap guard.
      tmpString[10] = 'Y'; tmpString[11] = 'Y'; tmpString[12] = 0;
      pgTestSetString(REGISTER_X, "ABCDEFGH\x80");
      if(!pgStringCut(REGISTER_X, 1)) pgTestFail("D17b the trim refused a short string");
      if(tmpString[0] != 0) pgTestFail("D17b the trim did not empty a string wider than one pixel");
      if(tmpString[10] != 'Y' || tmpString[11] != 'Y') pgTestFail("D17b the walk wrote beyond the NUL");
      // D17c: the cap keeps a two-byte glyph whose second byte has bit 7 set
      // (audit G2 round 1, in-family 7).
      for(i = 0; i < TMP_STR_LENGTH - 4; i++) longString[i] = 'A';
      longString[TMP_STR_LENGTH - 4] = (char)0x80; longString[TMP_STR_LENGTH - 3] = (char)0xE9;   // the e acute glyph ends at the cap
      for(i = TMP_STR_LENGTH - 2; i < TMP_STR_LENGTH + 30; i++) longString[i] = 'B';
      longString[TMP_STR_LENGTH + 30] = 0;
      pgTestSetString(REGISTER_X, longString);
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17c the string cut refused a long string");
      if(strlen(tmpString) != TMP_STR_LENGTH - 2 || (uint8_t)tmpString[TMP_STR_LENGTH - 4] != 0x80 || (uint8_t)tmpString[TMP_STR_LENGTH - 3] != 0xE9) pgTestFail("D17c the cap split a glyph whose second byte has bit 7 set");
      // D17d: a lone lead byte at the end is cut even when the width fits
      // (audit G2 round 1, in-family 2), and the canary survives.
      tmpString[10] = 'Y'; tmpString[11] = 'Y'; tmpString[12] = 0;
      pgTestSetString(REGISTER_X, "ABCDEFGH\x80");
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17d the cut refused a short string");
      if(strcmp(tmpString, "ABCDEFGH") != 0) pgTestFail("D17d a lone lead byte survived a cut that fits the width");
      if(tmpString[10] != 'Y' || tmpString[11] != 'Y') pgTestFail("D17d the cut wrote beyond the NUL");
    }

    // D19: an arc of 359.9995 degrees draws almost the full circle, and an
    // arc of 0.0002 degrees draws a few pixels (audit G2 round 1, in-family 3).
    {
      const angularMode_t savedAm = currentAngularMode;
      uint32_t lit, x, yy;
      currentAngularMode = amDegree;
      fnErase(NOPARAM);
      pgTestSetComplex(REGISTER_T, 200, 100);
      pgTestWriteLonI(REGISTER_Z, 50);
      pgTestWriteLonI(REGISTER_Y, 0);
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone); stringToReal34("359.9995", REGISTER_REAL34_DATA(REGISTER_X));
      fnGarc(NOPARAM);
      lit = 0;
      for(x = 140; x <= 260; x++) for(yy = PG_ROW_OF(160); yy <= PG_ROW_OF(40); yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
      if(lit < 250) pgTestFail("D19 the arc just under a full turn collapsed");
      fnErase(NOPARAM);
      stringToReal34("0.0002", REGISTER_REAL34_DATA(REGISTER_X));
      fnGarc(NOPARAM);
      lit = 0;
      for(x = 140; x <= 260; x++) for(yy = PG_ROW_OF(160); yy <= PG_ROW_OF(40); yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
      if(lit > 3) pgTestFail("D19 the tiny arc drew more than a few pixels");
      if(lastErrorCode != ERROR_NONE) { pgTestFail("D19 an arc raised an error"); lastErrorCode = ERROR_NONE; }
      currentAngularMode = savedAm;
    }

    // D18: a NaN angle is refused with ERROR_OUT_OF_RANGE and draws nothing.
    {
      const angularMode_t savedAm = currentAngularMode;
      currentAngularMode = amDegree;
      pgTestSetComplex(REGISTER_T, 200, 100);
      pgTestWriteLonI(REGISTER_Z, 30);
      pgTestWriteLonI(REGISTER_Y, 0);
      reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
      stringToReal34("NaN", REGISTER_REAL34_DATA(REGISTER_X));
      fnGarc(NOPARAM);
      currentAngularMode = savedAm;
      if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("D18 a NaN angle did not raise ERROR_OUT_OF_RANGE");
      lastErrorCode = ERROR_NONE;
      if(pgTestLit(230, 100) || pgTestLit(200, 130)) pgTestFail("D18 the refused arc drew");
    }

    // W1: without a window a real is a pixel, rounded half away from zero:
    // 2.5 is 3, 100.49 is 100, and -0.5 is -1, which is off the screen.
    fnErase(NOPARAM);
    pgWindow.set = 0;
    pgTestWriteReal(REGISTER_X, "2.5"); pgTestWriteReal(REGISTER_Y, "100.49"); pgTestWriteReal(REGISTER_Z, "2.5"); pgTestWriteReal(REGISTER_T, "100.49");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W1 a real coordinate raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(3, 100) || pgTestLit(2, 100) || pgTestLit(3, 101)) pgTestFail("W1 a real without a window is not rounded half away from zero");
    pgTestWriteReal(REGISTER_X, "-0.5"); pgTestWriteReal(REGISTER_Y, "50"); pgTestWriteReal(REGISTER_Z, "-0.5"); pgTestWriteReal(REGISTER_T, "50");
    fnGline(NOPARAM);
    if(pgTestLit(0, 50)) pgTestFail("W1 a real of -0.5 was rounded toward zero onto the screen");

    // W2: XRNG 0 10 and YRNG 0 5 map the user point (5, 2.5) to the pixel
    // (200, 120): 199.5 and 119.5 rounded half away from zero. The corners
    // map to (0, 0) and (399, 239). A long integer stays a pixel.
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 10); fnXrng(NOPARAM);
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 5);  fnYrng(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W2 XRNG or YRNG raised an error"); lastErrorCode = ERROR_NONE; }
    pgTestWriteReal(REGISTER_X, "5"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "5"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(!pgTestLit(200, 120) || pgTestLit(199, 120) || pgTestLit(200, 119) || pgTestLit(201, 120)) pgTestFail("W2 the user point (5, 2.5) did not map to the pixel (200, 120)");
    pgTestWriteReal(REGISTER_X, "0"); pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_Z, "10"); pgTestWriteReal(REGISTER_T, "0");
    fnGline(NOPARAM);
    if(!pgTestLit(0, 0) || !pgTestLit(399, 0) || pgTestLit(0, 1)) pgTestFail("W2 the bottom edge of the window did not map to the bottom row");
    pgTestWriteLonI(REGISTER_X, 5); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 5); pgTestWriteLonI(REGISTER_T, 100);
    fnGline(NOPARAM);
    if(!pgTestLit(5, 100)) pgTestFail("W2 a long integer under a window is not a pixel");

    // W3: equal ends are refused and leave the window; a reversed range mirrors the axis.
    pgTestWriteLonI(REGISTER_Y, 3); pgTestWriteLonI(REGISTER_X, 3); fnXrng(NOPARAM);
    if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("W3 XRNG with equal ends did not raise ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN");
    lastErrorCode = ERROR_NONE;
    pgTestWriteReal(REGISTER_X, "5"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "5"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(!pgTestLit(200, 120)) pgTestFail("W3 a refused XRNG changed the window");
    pgTestWriteLonI(REGISTER_Y, 10); pgTestWriteLonI(REGISTER_X, 0); fnXrng(NOPARAM);
    pgTestWriteReal(REGISTER_X, "10"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "10"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(!pgTestLit(0, 120) || pgTestLit(399, 120)) pgTestFail("W3 a reversed XRNG did not mirror the axis");
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W3 the reversed range raised an error"); lastErrorCode = ERROR_NONE; }

    // W4: a real that maps beyond 32767 pixels is ERROR_OUT_OF_RANGE; one
    // that maps to 3990 is clipped without an error.
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 10); fnXrng(NOPARAM);
    pgTestWriteReal(REGISTER_X, "1000"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "1000"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("W4 a user x of 1000 in a window of 10 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    pgTestWriteReal(REGISTER_X, "100"); pgTestWriteReal(REGISTER_Z, "100");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W4 a user x of 100 raised an error"); lastErrorCode = ERROR_NONE; }
    if(pgTestLit(399, 120)) pgTestFail("W4 an off-screen real drew on the screen");

    // W5: the complex two-point form, the first point in Y and the second in X.
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 399); fnXrng(NOPARAM);
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 239); fnYrng(NOPARAM);
    pgTestSetComplex(REGISTER_Y, 10, 20);
    pgTestSetComplex(REGISTER_X, 50, 20);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W5 the complex form raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(10, 20) || !pgTestLit(50, 20) || !pgTestLit(30, 20) || pgTestLit(30, 21)) pgTestFail("W5 the complex two-point line is wrong");
    pgTestWriteLonI(REGISTER_Y, 60);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) pgTestFail("W5 a complex in X with a long integer in Y did not raise the type error");
    lastErrorCode = ERROR_NONE;
    if(pgTestLit(50, 60)) pgTestFail("W5 the mixed pair drew");

    // W6: the window survives ERASE.
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 10); fnXrng(NOPARAM);
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 5);  fnYrng(NOPARAM);
    fnErase(NOPARAM);
    pgTestWriteReal(REGISTER_X, "5"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "5"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(!pgTestLit(200, 120)) pgTestFail("W6 ERASE reset the window");
    pgWindow.set = 0;

    pgCloseView();
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // S1: the showcase screen with every 2D command (TESTING.md §6). Writes
  // the screen to pg_showcase_2d.bmp and prints the count of lit pixels.
  extern char _ioFileNameOverride[];
  void pgTestShowcase2D(uint16_t unusedButMandatoryParameter) {
    uint32_t lit = 0, x, yy;
    const angularMode_t savedAm = currentAngularMode;
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    fnPview(6);
    fnGmode(0);
    currentAngularMode = amDegree;
    pgWindow.set = 0;
    pgTestSetString(REGISTER_X, "program-graphics G3: LINE BOX FBOX CIRCLE FCIRCL ARC TEXTOUT DISP GMODE GCLIP XRNG YRNG");
    fnGdisp(1);
    pgTestPoints(10, 10, 390, 10);   fnGline(NOPARAM);           // a baseline
    pgTestPoints(10, 10, 10, 195);   fnGline(NOPARAM);           // a left axis
    pgTestPoints(10, 10, 120, 190);  fnGline(NOPARAM);           // a diagonal
    pgTestPoints(40, 40, 110, 90);   fnGbox(NOPARAM);            // an outline box
    pgTestPoints(130, 40, 200, 90);  fnGfbox(NOPARAM);           // a filled box
    pgTestWriteLonI(REGISTER_X, 250); pgTestWriteLonI(REGISTER_Y, 65); pgTestWriteLonI(REGISTER_Z, 25); fnGcircle(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 320); pgTestWriteLonI(REGISTER_Y, 65); pgTestWriteLonI(REGISTER_Z, 25); fnGfcircle(NOPARAM);
    pgTestSetComplex(REGISTER_T, 250, 150); pgTestWriteLonI(REGISTER_Z, 40); pgTestWriteLonI(REGISTER_Y, 30); pgTestWriteLonI(REGISTER_X, 300); fnGarc(NOPARAM);
    pgTestSetString(REGISTER_Z, "TEXTOUT at 150,130");
    pgTestWriteLonI(REGISTER_X, 150); pgTestWriteLonI(REGISTER_Y, 130); fnGtextout(NOPARAM);
    pgTestPoints(300, 110, 390, 180); fnGclip(NOPARAM);          // clip, then a disc centred on the clip corner: a quarter shows
    pgTestWriteLonI(REGISTER_X, 390); pgTestWriteLonI(REGISTER_Y, 180); pgTestWriteLonI(REGISTER_Z, 45); fnGfcircle(NOPARAM);
    pgTestPoints(300, 110, 390, 180); fnGbox(NOPARAM);           // the clip rectangle itself, as an outline
    pgTestPoints(0, 0, 399, 239); fnGclip(NOPARAM);
    fnGmode(2);
    pgTestPoints(150, 55, 180, 75); fnGfbox(NOPARAM);            // an inverted window on the filled box
    fnGmode(0);
    {
      // A sine curve through the window: x from 0 to 2 pi on columns 130
      // to 390, y from -1 to 1 on rows 92 to 108, 26 segments of reals.
      char a[24], b[24];
      int k;
      pgTestWriteReal(REGISTER_Y, "-3.1421"); pgTestWriteReal(REGISTER_X, "6.5016"); fnXrng(NOPARAM);
      pgTestWriteReal(REGISTER_Y, "-12.5");   pgTestWriteReal(REGISTER_X, "17.375"); fnYrng(NOPARAM);
      for(k = 0; k < 26; k++) {
        double x0 = k * 6.283185307 / 26, x1 = (k + 1) * 6.283185307 / 26;
        sprintf(a, "%.6f", x0); pgTestWriteReal(REGISTER_X, a); sprintf(a, "%.6f", sin(x0)); pgTestWriteReal(REGISTER_Y, a);
        sprintf(b, "%.6f", x1); pgTestWriteReal(REGISTER_Z, b); sprintf(b, "%.6f", sin(x1)); pgTestWriteReal(REGISTER_T, b);
        fnGline(NOPARAM);
      }
      pgWindow.set = 0;
    }
    currentAngularMode = savedAm;
    for(x = 0; x < SCREEN_WIDTH; x++) for(yy = PG_TOP_ROW; yy < SCREEN_HEIGHT; yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
    printf("program-graphics showcase 2D: %u lit pixels in rows 20 to 239\n", lit);
    if(lit != 10760) pgTestFail("S1 the showcase count of lit pixels moved from the recorded 10760");
    strcpy(_ioFileNameOverride, "pg_showcase_2d.bmp");
    fnScreenDump(0);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("S1 an error was raised while drawing the showcase"); lastErrorCode = ERROR_NONE; }
    pgCloseView();

    // A second picture: region 2 with the CANVAS softmenu visible below the drawing.
    calcMode = CM_NORMAL;
    showSoftmenu(-MNU_CANVAS);
    fnPview(2);
    pgTestSetString(REGISTER_X, "PVIEW 2: the register lines are the canvas, the softmenu stays");
    fnGdisp(1);
    pgTestPoints(20, 80, 380, 80);   fnGline(NOPARAM);
    pgTestPoints(40, 90, 120, 140);  fnGbox(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 200); pgTestWriteLonI(REGISTER_Y, 115); pgTestWriteLonI(REGISTER_Z, 25); fnGfcircle(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 300); pgTestWriteLonI(REGISTER_Y, 115); pgTestWriteLonI(REGISTER_Z, 25); fnGcircle(NOPARAM);
    strcpy(_ioFileNameOverride, "pg_show2d_menu.bmp");
    fnScreenDump(0);
    pgCloseView();
    popSoftmenu();
    calcMode = CM_NORMAL;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }
#endif // TESTSUITE_BUILD
