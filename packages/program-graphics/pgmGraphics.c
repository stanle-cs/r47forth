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
static void pg3dEmpty(void);
static void pg3dFreeBlock(void);

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
  pg3dEmpty();   // §9.2.4: ERASE and PVIEW drop the retained 3D content
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
  pg3dFreeBlock();   // §9.2.4: the retained 3D block goes with the view
  calcMode = canvas.prevCalcMode;
  canvas.region = 0;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = SCRUPD_AUTO;
  if(calcMode == CM_AIM) {   // the view took the cursor at PVIEW; alpha input gets it back
    setSystemFlag(FLAG_ALPHA);
  }
  refreshScreen(197);
  if(calcMode == CM_AIM) {   // after the refresh, which can take the cursor away
    cursorEnabled = true;
  }
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
  while(tmpString[0] != 0 && (uint32_t)stringWidth(tmpString, &standardFont, true, true) > width) {
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


// ---------------------------------------------------------------------------
// Stage G4: 3D (DESIGN.md §9). The state, the retained block, the
// projection, the keys, and the redraw.
// ---------------------------------------------------------------------------

#define PG3D_BLOCK_BYTES    2048
#define PG3D_BLOCKS          512   // TO_BLOCKS(2048)
#define PG3D_HEADER_BYTES     64
#define PG3D_PAYLOAD_BYTES  1984
#define PG3D_LINE_BYTES        6
#define PG3D_MAX_LINES       330
#define PG3D_STEPS           254   // byte values 0 to 254 span a range
#define PG3D_HOLE            255   // a missing sample
#define PG3D_NOPIX        -32768   // a projected point that is not drawable

typedef struct {
  float    eyeX, eyeY, eyeZ;
  float    xlo, xhi, ylo, yhi, zlo, zhi;
  float    curX, curY, curZ;
  uint8_t  numX, numY;
  uint8_t  haveCur;
  uint8_t  angX, angY, angZ;   // step counts, 0 to 35, one step is 10 degrees
  int8_t   zoomStep;           // -8 to 8
  uint8_t  reserved;
  uint8_t *block;              // the retained block, NULL when none
} pg3d_t;

typedef struct {
  uint8_t  numX, numY;     // the grid, 0 = none
  uint8_t  gridValid;      // 1 when every grid byte came from a complete run
  uint8_t  frozen;         // 1 after the first record
  uint16_t lineCount;
  uint16_t label;          // the label of the last WIREFRAME, 0 = none
  float    xlo, xhi, ylo, yhi, zlo, zhi;
  float    zRecLo, zRecHi; // the z range the grid bytes span
  float    eyeX, eyeY, eyeZ;
  uint8_t  reserved[12];
} pg3dHeader_t;

typedef struct {
  float eyeX, eyeY, eyeZ;
  float xlo, xhi, ylo, yhi, zlo, zhi;
  float zRecLo, zRecHi;
} pg3dView_t;

typedef struct {
  pg3dView_t v;
  float      M[9];
  float      cx, cy, cz;
  float      eyeYz;
  float      eps;
  float      wxmin, wxs, wymin, wys;
} pg3dSetup_t;

typedef struct { int16_t col, row; } pg3dPix_t;

static pg3d_t   pg3d;
static uint16_t pg3dResetCount;
static uint32_t pg3dRunCount;
static uint16_t pg3dLastPointError;

#define PG3D_HDR() ((pg3dHeader_t *)pg3d.block)
#define PG3D_GRID() (pg3d.block + PG3D_HEADER_BYTES)

// The reset hook: the pool is rebuilt, so the block is forgotten without a
// free, and the HP VPAR defaults return (§9.1.1).
void pgReset(void) {
  pg3d.block = NULL;
  pg3dResetCount++;
  pg3d.eyeX = 0.0f; pg3d.eyeY = -3.0f; pg3d.eyeZ = 0.0f;
  pg3d.xlo = pg3d.ylo = pg3d.zlo = -1.0f;
  pg3d.xhi = pg3d.yhi = pg3d.zhi = 1.0f;
  pg3d.numX = 10; pg3d.numY = 8;
  pg3d.curX = pg3d.curY = pg3d.curZ = 0.0f; pg3d.haveCur = 0;
  pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0;
  pgWindow.set = 0;
}

static uint32_t pg3dFreeBytes(const pg3dHeader_t *h) {
  uint32_t used = (uint32_t)h->numX * h->numY + PG3D_LINE_BYTES * (uint32_t)h->lineCount;
  return used > PG3D_PAYLOAD_BYTES ? 0 : PG3D_PAYLOAD_BYTES - used;
}

static uint8_t pg3dEncode(float v, float lo, float hi) {
  float t;
  if(v != v || v - v != 0.0f) return PG3D_HOLE;
  t = (v - lo) * (254.0f / (hi - lo));
  if(t <= 0.0f) return 0;
  if(t >= 254.0f) return 254;
  return (uint8_t)(t + 0.5f);
}

static float pg3dDecode(uint8_t b, float lo, float hi) {
  return lo + (float)b * ((hi - lo) / 254.0f);
}

// The block: allocated by the first 3D command inside the view (§9.2.4).
static bool_t pg3dEnsure(void) {
  if(canvas.region == 0) return true;
  if(pg3d.block != NULL) return true;
  pg3d.block = allocC47Blocks(PG3D_BLOCKS);
  if(pg3d.block == NULL) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  memset(pg3d.block, 0, PG3D_BLOCK_BYTES);
  return true;
}

static void pg3dEmpty(void) {   // content gone, allocation kept
  if(pg3d.block == NULL) return;
  memset(pg3d.block, 0, PG3D_HEADER_BYTES);
  pg3d.haveCur = 0;
}

static void pg3dFreeBlock(void) {
  freeC47Blocks(pg3d.block, PG3D_BLOCKS);
  pg3d.block = NULL;
  pg3d.haveCur = 0;
}

static bool_t pg3dViewValid(const pg3dView_t *v) {
  return v->xlo < v->xhi && v->ylo < v->yhi && v->zlo < v->zhi && v->eyeY < v->ylo;
}

// The frozen view (§9.2.5): the header keeps the volume and the eye of the
// content it stores from the first record on.
static bool_t pg3dRecordView(pg3dView_t *out) {
  pg3dHeader_t *h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(h != NULL && h->frozen) {
    out->eyeX = h->eyeX; out->eyeY = h->eyeY; out->eyeZ = h->eyeZ;
    out->xlo = h->xlo; out->xhi = h->xhi; out->ylo = h->ylo; out->yhi = h->yhi; out->zlo = h->zlo; out->zhi = h->zhi;
    out->zRecLo = h->zRecLo; out->zRecHi = h->zRecHi;
    return true;
  }
  out->eyeX = pg3d.eyeX; out->eyeY = pg3d.eyeY; out->eyeZ = pg3d.eyeZ;
  out->xlo = pg3d.xlo; out->xhi = pg3d.xhi; out->ylo = pg3d.ylo; out->yhi = pg3d.yhi; out->zlo = pg3d.zlo; out->zhi = pg3d.zhi;
  out->zRecLo = pg3d.zlo; out->zRecHi = pg3d.zhi;
  if(!pg3dViewValid(out)) {
    displayCalcErrorMessage(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN, ERR_REGISTER_LINE, REGISTER_X);
    return false;
  }
  if(h != NULL) {
    h->eyeX = out->eyeX; h->eyeY = out->eyeY; h->eyeZ = out->eyeZ;
    h->xlo = out->xlo; h->xhi = out->xhi; h->ylo = out->ylo; h->yhi = out->yhi; h->zlo = out->zlo; h->zhi = out->zhi;
    h->zRecLo = out->zRecLo; h->zRecHi = out->zRecHi;
    h->frozen = 1;
  }
  return true;
}

// The rotation (§9.3.2): integer step counts, a 36-entry sine table.
static const float pg3dSin[36] = {
  0.0f, 0.173648178f, 0.342020143f, 0.5f, 0.64278761f, 0.766044443f,
  0.866025404f, 0.939692621f, 0.984807753f, 1.0f, 0.984807753f, 0.939692621f,
  0.866025404f, 0.766044443f, 0.64278761f, 0.5f, 0.342020143f, 0.173648178f,
  0.0f, -0.173648178f, -0.342020143f, -0.5f, -0.64278761f, -0.766044443f,
  -0.866025404f, -0.939692621f, -0.984807753f, -1.0f, -0.984807753f, -0.939692621f,
  -0.866025404f, -0.766044443f, -0.64278761f, -0.5f, -0.342020143f, -0.173648178f
};
#define PG3D_SIN(k) pg3dSin[(k) % 36]
#define PG3D_COS(k) pg3dSin[((k) + 9) % 36]

static const float pg3dZoom[17] = {   // 1.25 to the power k, k from -8 to 8
  0.16777216f, 0.2097152f, 0.262144f, 0.32768f, 0.4096f, 0.512f, 0.64f, 0.8f,
  1.0f, 1.25f, 1.5625f, 1.953125f, 2.44140625f, 3.0517578125f,
  3.814697265625f, 4.76837158203125f, 5.9604644775390625f
};

static void pg3dMul3(const float *A, const float *B, float *out) {   // out = A * B, row major
  int i, j, k;
  for(i = 0; i < 3; i++) for(j = 0; j < 3; j++) {
    float s = 0.0f;
    for(k = 0; k < 3; k++) s += A[i * 3 + k] * B[k * 3 + j];
    out[i * 3 + j] = s;
  }
}

static void pg3dMatrix(float *M, uint8_t ax, uint8_t ay, uint8_t az) {
  float sx = PG3D_SIN(ax), cx = PG3D_COS(ax), sy = PG3D_SIN(ay), cy = PG3D_COS(ay), sz = PG3D_SIN(az), cz = PG3D_COS(az);
  float A[9] = { 1, 0, 0,  0, cx, -sx,  0, sx, cx };
  float B[9] = { cy, 0, sy,  0, 1, 0,  -sy, 0, cy };
  float C[9] = { cz, -sz, 0,  sz, cz, 0,  0, 0, 1 };
  float T[9];
  pg3dMul3(B, A, T);
  pg3dMul3(C, T, M);
}

static float pg3dReal34ToFloat(const real34_t *r34) {
  real_t r; float f;
  real34ToReal(r34, &r);
  realToFloat(&r, &f);
  return f;
}

static void pg3dSetup(pg3dSetup_t *s, const pg3dView_t *view) {
  s->v = *view;
  pg3dMatrix(s->M, pg3d.angX, pg3d.angY, pg3d.angZ);
  s->cx = (view->xlo + view->xhi) * 0.5f; s->cy = (view->ylo + view->yhi) * 0.5f; s->cz = (view->zlo + view->zhi) * 0.5f;
  s->eyeYz = view->ylo - (view->ylo - view->eyeY) / pg3dZoom[pg3d.zoomStep + 8];
  s->eps = (view->yhi - view->ylo) * (1.0f / 1024.0f);
  if(pgWindow.set & 1) {
    s->wxmin = pg3dReal34ToFloat(&pgWindow.xmin);
    s->wxs = 399.0f / (pg3dReal34ToFloat(&pgWindow.xmax) - s->wxmin);
  }
  else { s->wxmin = 0.0f; s->wxs = 1.0f; }
  if(pgWindow.set & 2) {
    s->wymin = pg3dReal34ToFloat(&pgWindow.ymin);
    s->wys = 239.0f / (pg3dReal34ToFloat(&pgWindow.ymax) - s->wymin);
  }
  else { s->wymin = 0.0f; s->wys = 1.0f; }
}

static int32_t pg3dRound(float f) {   // round half up, clamped, no libm
  if(!(f > -32000.0f)) f = -32000.0f;
  if(f > 32000.0f) f = 32000.0f;
  return (int32_t)(f + 32768.5f) - 32768;
}

// One point through the rotation, the perspective, and the window (§9.3.5).
static bool_t pg3dProject(const pg3dSetup_t *s, float x, float y, float z, int32_t *col, int32_t *row) {
  float px = x - s->cx, py = y - s->cy, pz = z - s->cz;
  float rx = s->M[0] * px + s->M[1] * py + s->M[2] * pz + s->cx;
  float ry = s->M[3] * px + s->M[4] * py + s->M[5] * pz + s->cy;
  float rz = s->M[6] * px + s->M[7] * py + s->M[8] * pz + s->cz;
  float dy = ry - s->eyeYz, inv, u, v;
  if(!(dy >= s->eps)) return false;   // nearer than eps is not drawable; eps itself is
  inv = 1.0f / dy;
  u = s->v.eyeX + (rx - s->v.eyeX) * inv;
  v = s->v.eyeZ + (rz - s->v.eyeZ) * inv;
  *col = pg3dRound((u - s->wxmin) * s->wxs);
  *row = SCREEN_HEIGHT - 1 - pg3dRound((v - s->wymin) * s->wys);
  if(*row > 32000) *row = 32000;     // the clamp holds after the row flip as well
  if(*row < -32000) *row = -32000;
  return true;
}

static void pg3dDrawRecord(const pg3dSetup_t *s, const uint8_t *rec, const pgRect_t *clip) {
  float x0 = pg3dDecode(rec[0], s->v.xlo, s->v.xhi), y0 = pg3dDecode(rec[1], s->v.ylo, s->v.yhi), z0 = pg3dDecode(rec[2], s->v.zlo, s->v.zhi);
  float x1 = pg3dDecode(rec[3], s->v.xlo, s->v.xhi), y1 = pg3dDecode(rec[4], s->v.ylo, s->v.yhi), z1 = pg3dDecode(rec[5], s->v.zlo, s->v.zhi);
  int32_t c0, r0, c1, r1;
  if(pg3dProject(s, x0, y0, z0, &c0, &r0) && pg3dProject(s, x1, y1, z1, &c1, &r1)) {
    pgLine(clip, c0, r0, c1, r1);
  }
}

// One mesh point: the lines to the previous column and the previous row (§9.4.5).
static void pg3dMeshPoint(const pg3dSetup_t *s, pg3dPix_t *rows, uint32_t numX, uint32_t i, uint32_t j, float x, float y, float z, const pgRect_t *clip) {
  pg3dPix_t *cur = rows + (j & 1) * numX, *prev = rows + ((j + 1) & 1) * numX;
  int32_t col = 0, row = 0;
  bool_t ok = (z == z) && pg3dProject(s, x, y, z, &col, &row);
  cur[i].col = ok ? (int16_t)col : PG3D_NOPIX; cur[i].row = ok ? (int16_t)row : 0;
  if(!ok) return;
  if(i > 0 && cur[i - 1].col != PG3D_NOPIX) pgLine(clip, cur[i - 1].col, cur[i - 1].row, col, row);
  if(j > 0 && prev[i].col != PG3D_NOPIX)    pgLine(clip, prev[i].col, prev[i].row, col, row);
}

// ---- WIREFRAME (§9.4) ----

#define PG3D_RUN_OK       0
#define PG3D_RUN_ABORTED  1
#define PG3D_RUN_ALLHOLES 2

// One sample: the label runs with X = x, Y = y, Z = x, T = y and leaves z in X.
static float pg3dSample(uint16_t label, float x, float y, uint16_t *err) {
  real_t r; float z;
  convertDoubleToReal34Register((double)y, REGISTER_T);
  convertDoubleToReal34Register((double)x, REGISTER_Z);
  convertDoubleToReal34Register((double)y, REGISTER_Y);
  convertDoubleToReal34Register((double)x, REGISTER_X);
  dynamicMenuItem = -1;
  pg3dRunCount++;
  execProgram(label);
  if(lastErrorCode != ERROR_NONE) {
    *err = lastErrorCode;
    if(lastErrorCode != ERROR_SOLVER_ABORT) lastErrorCode = ERROR_NONE;
    return 0.0f / 0.0f;
  }
  fnToReal(NOPARAM);
  if(lastErrorCode != ERROR_NONE) {
    *err = lastErrorCode; lastErrorCode = ERROR_NONE;
    return 0.0f / 0.0f;
  }
  real34ToReal(REGISTER_REAL34_DATA(REGISTER_X), &r);
  realToFloat(&r, &z);
  *err = ERROR_NONE;
  return z;
}

// The grid loop (§9.4.3): records the bytes when retain, draws when draw.
static int pg3dRunGrid(const pg3dSetup_t *s, pg3dHeader_t *h, pg3dPix_t *rows, uint32_t numX, uint32_t numY, bool_t retain, uint16_t label, bool_t draw, const pgRect_t *clip) {
  uint32_t i, j, holes = 0;
  pg3dLastPointError = ERROR_NONE;
  for(j = 0; j < numY; j++) {
    float y = s->v.ylo + (float)j * ((s->v.yhi - s->v.ylo) / (float)(numY - 1));
    for(i = 0; i < numX; i++) {
      float x = s->v.xlo + (float)i * ((s->v.xhi - s->v.xlo) / (float)(numX - 1));
      float z; uint16_t err; uint8_t b;
      if(lastErrorCode == ERROR_SOLVER_ABORT || programRunStop == PGM_WAITING || exitKeyWaiting()) {
        lastErrorCode = engineNestingWasRefused ? ERROR_NESTING_TOO_DEEP : ERROR_SOLVER_ABORT;
        if(programRunStop == PGM_RUNNING) programRunStop = PGM_WAITING;
        return PG3D_RUN_ABORTED;
      }
      z = pg3dSample(label, x, y, &err);
      if(err != ERROR_NONE) { holes++; pg3dLastPointError = err; }
      if(pg3d.block == NULL || (h != NULL && h->frozen == 0)) retain = false;   // the body emptied or reset the block
      b = pg3dEncode(z, s->v.zRecLo, s->v.zRecHi);
      if(retain) PG3D_GRID()[j * numX + i] = b;
      if(draw) {
        float zq = (b == PG3D_HOLE) ? (0.0f / 0.0f) : pg3dDecode(b, s->v.zRecLo, s->v.zRecHi);
        pg3dMeshPoint(s, rows, numX, i, j, x, y, zq, clip);
      }
    }
    if(draw) pgRefreshMaybe();
  }
  if(lastErrorCode == ERROR_SOLVER_ABORT) return PG3D_RUN_ABORTED;
  if(holes == numX * numY) return PG3D_RUN_ALLHOLES;
  return PG3D_RUN_OK;
}

// The engine protocol around a run: the shape of the sum engine and the
// plot engine (sumprod.c 76-123, graph.c 2935-2936, solve.c 233-248).
typedef struct {
  uint16_t program, local, resets;
  uint8_t *step;
} pg3dEngineSave_t;

static void pg3dEngineEnter(pg3dEngineSave_t *sv) {
  currentKeyCode = 255;
  saveForUndo();
  ++engineNestingDepth;
  ++plotEngineActive;
  ++currentSolverNestingDepth;
  setSystemFlag(FLAG_SOLVING);
  sv->program = currentProgramNumber;
  sv->local = currentLocalStepNumber;
  sv->step = currentStep;
  sv->resets = pg3dResetCount;
}

static void pg3dEngineLeave(const pg3dEngineSave_t *sv) {
  currentProgramNumber = sv->program;
  currentLocalStepNumber = sv->local;
  currentStep = sv->step;
  if(--currentSolverNestingDepth == 0) clearSystemFlag(FLAG_SOLVING);
  --plotEngineActive;
  --engineNestingDepth;
  temporaryInformation = TI_NO_INFO;
  fnUndo(0);   // the undo image is consumed here, as after PLTf; nothing re-arms it
}

void fnWireframe(uint16_t label) {
  pg3dHeader_t *h;
  pg3dView_t view;
  pg3dSetup_t s;
  pgRect_t clip;
  pg3dPix_t *rows;
  pg3dEngineSave_t sv;
  uint32_t numX, numY;
  bool_t retain;
  int result;
  if(REGISTER_X <= label && label <= REGISTER_T) {   // the shape of _checkArgument in sumprod.c
    char buf[2];
    buf[0] = letteredRegisterName(label); buf[1] = 0;
    label = findNamedLabel(buf, GLOBAL_LABELS);
    if(label == INVALID_VARIABLE) {
      displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
      return;
    }
  }
  else if(!(FIRST_LABEL <= label && label <= LAST_LABEL)) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }
  if(engineNestingRefused(true)) return;
  if(!pg3dEnsure()) return;
  if(!pg3dRecordView(&view)) return;
  numX = pg3d.numX; numY = pg3d.numY;
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(h != NULL) {
    h->gridValid = 0; h->numX = 0; h->numY = 0;
    retain = (numX * numY <= pg3dFreeBytes(h));
    if(retain) { h->numX = (uint8_t)numX; h->numY = (uint8_t)numY; }
  }
  else {
    retain = false;
  }
  rows = allocC47Blocks(TO_BLOCKS(2 * numX * sizeof(pg3dPix_t)));
  if(rows == NULL) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  pg3dEngineEnter(&sv);
  pg3dSetup(&s, &view);
  pgClipNow(&clip);
  result = pg3dRunGrid(&s, h, rows, numX, numY, retain, label, true, &clip);
  if(pg3dResetCount == sv.resets) freeC47Blocks(rows, TO_BLOCKS(2 * numX * sizeof(pg3dPix_t)));
  pg3dEngineLeave(&sv);
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(result == PG3D_RUN_ALLHOLES) {
    displayCalcErrorMessage(pg3dLastPointError, ERR_REGISTER_LINE, REGISTER_X);
  }
  else if(result == PG3D_RUN_OK && retain && h != NULL && h->frozen && h->numX == numX && h->numY == numY) {   // a body that emptied the block leaves no valid grid
    h->gridValid = 1; h->label = label;
  }
  pgRefreshNow();
}

// ---- the setting commands, PT3D and LINE3D (§9.5) ----

static bool_t pg3dReadFloat(calcRegister_t regist, float *f) {
  real_t r;
  switch(getRegisterDataType(regist)) {
    case dtLongInteger: convertLongIntegerRegisterToReal(regist, &r, &ctxtReal39); break;
    case dtReal34:
      if(real34IsNaN(REGISTER_REAL34_DATA(regist)) || real34IsInfinite(REGISTER_REAL34_DATA(regist))) { pgError(ERROR_OUT_OF_RANGE); return false; }
      real34ToReal(REGISTER_REAL34_DATA(regist), &r);
      break;
    default: pgError(ERROR_INVALID_DATA_TYPE_FOR_OP); return false;
  }
  realToFloat(&r, f);
  if(*f - *f != 0.0f) { pgError(ERROR_OUT_OF_RANGE); return false; }
  return true;
}

static bool_t pg3dReadPoint(float *x, float *y, float *z) {   // x in Z, y in Y, z in X
  return pg3dReadFloat(REGISTER_Z, x) && pg3dReadFloat(REGISTER_Y, y) && pg3dReadFloat(REGISTER_X, z);
}

static bool_t pg3dReadCount(uint8_t *n) {   // an integer from 2 to 100 in X
  real_t r; int32_t v; bool_t err = false;
  switch(getRegisterDataType(REGISTER_X)) {
    case dtLongInteger: convertLongIntegerRegisterToReal(REGISTER_X, &r, &ctxtReal39); break;
    case dtReal34:      real34ToReal(REGISTER_REAL34_DATA(REGISTER_X), &r); break;
    default: pgError(ERROR_INVALID_DATA_TYPE_FOR_OP); return false;
  }
  v = realToInt32C47(&r, &err);
  if(err || v < 2 || v > 100 || !realIsAnInteger(&r)) { pgError(ERROR_OUT_OF_RANGE); return false; }
  *n = (uint8_t)v;
  return true;
}

static void pg3dRange(float *lo, float *hi) {   // low in Y, high in X
  float a, b;
  if(!pg3dReadFloat(REGISTER_Y, &a) || !pg3dReadFloat(REGISTER_X, &b)) return;
  if(!(a < b) || !((b - a) - (b - a) == 0.0f)) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }   // the span must be a finite positive float
  *lo = a; *hi = b;
}

void fnEyept(uint16_t unusedButMandatoryParameter) {
  float x, y, z;
  if(!pg3dEnsure()) return;
  if(!pg3dReadPoint(&x, &y, &z)) return;
  pg3d.eyeX = x; pg3d.eyeY = y; pg3d.eyeZ = z;
}
void fnXvol(uint16_t unusedButMandatoryParameter) { if(pg3dEnsure()) pg3dRange(&pg3d.xlo, &pg3d.xhi); }
void fnYvol(uint16_t unusedButMandatoryParameter) { if(pg3dEnsure()) pg3dRange(&pg3d.ylo, &pg3d.yhi); }
void fnZvol(uint16_t unusedButMandatoryParameter) { if(pg3dEnsure()) pg3dRange(&pg3d.zlo, &pg3d.zhi); }
void fnNumx(uint16_t unusedButMandatoryParameter) { uint8_t n; if(pg3dEnsure() && pg3dReadCount(&n)) pg3d.numX = n; }
void fnNumy(uint16_t unusedButMandatoryParameter) { uint8_t n; if(pg3dEnsure() && pg3dReadCount(&n)) pg3d.numY = n; }

void fnPt3d(uint16_t unusedButMandatoryParameter) {
  float x, y, z;
  if(!pg3dEnsure()) return;
  if(!pg3dReadPoint(&x, &y, &z)) return;
  pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z; pg3d.haveCur = 1;
}

static float pg3dClamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void fnLine3d(uint16_t unusedButMandatoryParameter) {
  float x, y, z;
  pg3dView_t view;
  pg3dSetup_t s;
  pgRect_t clip;
  pg3dHeader_t *h;
  uint8_t rec[6];
  if(!pg3dEnsure()) return;
  if(!pg3dReadPoint(&x, &y, &z)) return;
  if(!pg3d.haveCur) {
    pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z; pg3d.haveCur = 1;
    return;
  }
  if(!pg3dRecordView(&view)) return;
  rec[0] = pg3dEncode(pg3dClamp(pg3d.curX, view.xlo, view.xhi), view.xlo, view.xhi);
  rec[1] = pg3dEncode(pg3dClamp(pg3d.curY, view.ylo, view.yhi), view.ylo, view.yhi);
  rec[2] = pg3dEncode(pg3dClamp(pg3d.curZ, view.zlo, view.zhi), view.zlo, view.zhi);
  rec[3] = pg3dEncode(pg3dClamp(x, view.xlo, view.xhi), view.xlo, view.xhi);
  rec[4] = pg3dEncode(pg3dClamp(y, view.ylo, view.yhi), view.ylo, view.yhi);
  rec[5] = pg3dEncode(pg3dClamp(z, view.zlo, view.zhi), view.zlo, view.zhi);
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(h != NULL && pg3dFreeBytes(h) >= PG3D_LINE_BYTES) {
    memcpy(pg3d.block + PG3D_BLOCK_BYTES - PG3D_LINE_BYTES * (h->lineCount + 1), rec, PG3D_LINE_BYTES);
    h->lineCount++;
  }
  pg3dSetup(&s, &view);
  pgClipNow(&clip);
  pg3dDrawRecord(&s, rec, &clip);
  pg3d.curX = x; pg3d.curY = y; pg3d.curZ = z;
  pgRefreshMaybe();
}

// ---- the keys, the redraw, and the zoom re-run (§9.6) ----

static void pg3dRedraw(void) {
  pg3dHeader_t *h = PG3D_HDR();
  pg3dView_t view;
  pg3dSetup_t s;
  pgRect_t clip;
  int32_t bottom = (canvas.region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  uint8_t savedMode = canvas.drawMode;
  uint32_t k;
  pg3dRecordView(&view);
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, (uint32_t)(bottom - PG_TOP_ROW + 1), LCD_SET_VALUE);
  canvas.drawMode = 0;
  pg3dSetup(&s, &view);
  pgClipNow(&clip);
  if(h->gridValid) {
    pg3dPix_t *rows = allocC47Blocks(TO_BLOCKS(2 * h->numX * sizeof(pg3dPix_t)));
    if(rows == NULL) {
      displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
    else {
      uint32_t i, j;
      for(j = 0; j < h->numY; j++) {
        float y = view.ylo + (float)j * ((view.yhi - view.ylo) / (float)(h->numY - 1));
        for(i = 0; i < h->numX; i++) {
          float x = view.xlo + (float)i * ((view.xhi - view.xlo) / (float)(h->numX - 1));
          uint8_t b = PG3D_GRID()[j * h->numX + i];
          float zq = (b == PG3D_HOLE) ? (0.0f / 0.0f) : pg3dDecode(b, view.zRecLo, view.zRecHi);
          pg3dMeshPoint(&s, rows, h->numX, i, j, x, y, zq, &clip);
        }
      }
      freeC47Blocks(rows, TO_BLOCKS(2 * h->numX * sizeof(pg3dPix_t)));
    }
  }
  for(k = 0; k < h->lineCount; k++) {
    pg3dDrawRecord(&s, pg3d.block + PG3D_BLOCK_BYTES - PG3D_LINE_BYTES * (k + 1), &clip);
  }
  canvas.drawMode = savedMode;
  pgRefreshNow();
}

// The re-run after a zoom step (§9.6.6): the grid bytes are recorded again
// over the z range visible at this magnification, without drawing.
static void pg3dRerun(pg3dHeader_t *h, pg3dView_t *view, float zNewLo, float zNewHi) {
  pg3dEngineSave_t sv;
  pg3dSetup_t s;
  uint32_t numX = h->numX, numY = h->numY;
  uint16_t label = h->label;
  int result;
  if(engineNestingRefused(true)) return;
  view->zRecLo = zNewLo; view->zRecHi = zNewHi;
  pg3dEngineEnter(&sv);
  pg3dSetup(&s, view);
  result = pg3dRunGrid(&s, h, NULL, numX, numY, true, label, false, NULL);
  pg3dEngineLeave(&sv);
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(h == NULL) return;
  if(result == PG3D_RUN_OK) { h->zRecLo = zNewLo; h->zRecHi = zNewHi; }
  else { h->gridValid = 0; }
  if(result == PG3D_RUN_ALLHOLES) displayCalcErrorMessage(pg3dLastPointError, ERR_REGISTER_LINE, REGISTER_X);
}

static void pg3dZoomRerun(void) {
  pg3dHeader_t *h = PG3D_HDR();
  pg3dView_t view;
  pg3dSetup_t s;
  float zoom, dNear, wymax, zVisLo, zVisHi, zNewLo, zNewHi, pps;
  bool_t wider;
  if(h->gridValid == 0 || h->label == 0) return;
  pg3dRecordView(&view);
  pg3dSetup(&s, &view);   // the window floats
  zoom = pg3dZoom[pg3d.zoomStep + 8];
  dNear = (view.ylo - view.eyeY) / zoom;
  wymax = s.wymin + 239.0f / s.wys;
  zVisLo = view.eyeZ + (s.wymin - view.eyeZ) * dNear;
  zVisHi = view.eyeZ + (wymax - view.eyeZ) * dNear;
  if(zVisLo > zVisHi) { float t = zVisLo; zVisLo = zVisHi; zVisHi = t; }
  zNewLo = (zVisLo > view.zlo) ? zVisLo : view.zlo;
  zNewHi = (zVisHi < view.zhi) ? zVisHi : view.zhi;
  if(!(zNewLo < zNewHi)) return;
  pps = (h->zRecHi - h->zRecLo) * (1.0f / 254.0f) * s.wys / dNear;
  if(pps < 0.0f) pps = -pps;
  wider = (zNewLo < h->zRecLo) || (zNewHi > h->zRecHi);
  if(!(pps > 1.0f) && !wider) return;
  pg3dRerun(h, &view, zNewLo, zNewHi);
}

// A key in the view (§9.6.1). Called from fnKeyUp, fnKeyDown, and the guard arm.
void pg3dKey(int16_t item) {
  pg3dHeader_t *h;
  if(pg3d.block == NULL) return;
  h = PG3D_HDR();
  if(h->gridValid == 0 && h->lineCount == 0) return;
  switch(item) {
    case ITM_UP1:   pg3d.angX = (uint8_t)((pg3d.angX + 1) % 36);  break;
    case ITM_DOWN1: pg3d.angX = (uint8_t)((pg3d.angX + 35) % 36); break;
    case ITM_BST:   pg3d.angY = (uint8_t)((pg3d.angY + 1) % 36);  break;
    case ITM_SST:   pg3d.angY = (uint8_t)((pg3d.angY + 35) % 36); break;
    case ITM_RBR:   pg3d.angZ = (uint8_t)((pg3d.angZ + 1) % 36);  break;
    case ITM_FLGSV: pg3d.angZ = (uint8_t)((pg3d.angZ + 35) % 36); break;
    case ITM_ADD:   if(pg3d.zoomStep >= 8) return;  pg3d.zoomStep++; pg3dZoomRerun(); break;
    case ITM_SUB:   if(pg3d.zoomStep <= -8) return; pg3d.zoomStep--; pg3dZoomRerun(); break;
    case ITM_5:
      if(pg3d.angX == 0 && pg3d.angY == 0 && pg3d.angZ == 0 && pg3d.zoomStep == 0) return;
      pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0; pg3dZoomRerun();
      break;
    default: return;
  }
  pg3dRedraw();
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
    if(calcMode != CM_AIM)          pgTestFail("K8 EXIT did not return to alpha input mode");
    if(!cursorEnabled)              pgTestFail("K8 EXIT did not give alpha input its cursor back");
    if(!getSystemFlag(FLAG_ALPHA))  pgTestFail("K8 EXIT did not set FLAG_ALPHA back");
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


  // ---- stage G4: the 3D pins (DESIGN.md §9.8) ----

  // Loads a program from bytes through the official loader, as the suite's
  // own covWriteAndLoadPgm does (testSuite.c 1167-1183).
  static void pgTestLoadProgram(const uint8_t *pgm, size_t n, const char *label) {
    FILE *f;
    size_t i;
    if(findNamedLabel(label, GLOBAL_LABELS) != INVALID_VARIABLE) return;   // loaded by an earlier driver: a duplicate global label breaks a later equation test
    f = fopen("c47programTest.bin", "wb");
    if(f == NULL) { pgTestFail("cannot write c47programTest.bin"); return; }
    fprintf(f, "PROGRAM_FILE_FORMAT\n0\nC47_program_file_version\n1\nPROGRAM\n%u\n", (unsigned)n);
    for(i = 0; i < n; i++) fprintf(f, "%u\n", pgm[i]);
    fclose(f);
    fnLoadProgram(NOPARAM);
    remove("c47programTest.bin");   // a stale file fails a later string test of the suite
    aimBuffer[0] = 0;               // the loader leaves a word in the alpha input buffer; a later string test expects it empty
  }

  static const uint8_t pgTestPgmSaddle[] = {
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'S', 'A', 'D', 'L',
    ITM_SQUARE, ITM_XexY, ITM_SQUARE, ITM_SUB,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmErase[] = {   // a body that empties the block under the run
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'E', 'R', 'A', 'S',
    (uint8_t)((ITM_ERASE >> 8) | 0x80), (uint8_t)(ITM_ERASE & 0xff),
    (uint8_t)((ITM_PT3D >> 8) | 0x80), (uint8_t)(ITM_PT3D & 0xff),       // the sample point as the current point
    (uint8_t)((ITM_LINE3D >> 8) | 0x80), (uint8_t)(ITM_LINE3D & 0xff),   // a zero-length line that freezes the header again
    ITM_CLX,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmPlane[] = {
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'P', 'L', 'N', 'E',
    ITM_CLX,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };

  static void pgTestWriteFloat(calcRegister_t regist, const char *text) {
    pgTestWriteReal(regist, text);
  }

  static void pgTestSet3(const char *x, const char *y, const char *z) {   // x in Z, y in Y, z in X
    pgTestWriteFloat(REGISTER_Z, x); pgTestWriteFloat(REGISTER_Y, y); pgTestWriteFloat(REGISTER_X, z);
  }

  static uint32_t pgTestLitCanvas(void) {
    uint32_t lit = 0, x, yy;
    for(x = 0; x < SCREEN_WIDTH; x++) for(yy = PG_TOP_ROW; yy < SCREEN_HEIGHT; yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
    return lit;
  }

  static uint8_t pgTestCanvasCopy[SCREEN_HEIGHT * 50];
  static void pgTestSnapCanvas(void) {   // bytes 2 to 51 of rows 20 to 239
    uint32_t r;
    for(r = PG_TOP_ROW; r < SCREEN_HEIGHT; r++) memcpy(pgTestCanvasCopy + r * 50, pgRowPtr((int32_t)r) + 2, 50);
  }
  static uint32_t pgTestCanvasDiff(void) {
    uint32_t r, d = 0, i;
    for(r = PG_TOP_ROW; r < SCREEN_HEIGHT; r++) for(i = 0; i < 50; i++) if(pgTestCanvasCopy[r * 50 + i] != pgRowPtr((int32_t)r)[2 + i]) d++;
    return d;
  }

  // The unit-cube view of §9.3.6 inside PVIEW 6.
  static void pgTestUnitCubeView(void) {
    calcMode = CM_NORMAL; lastErrorCode = ERROR_NONE;
    pgReset();
    fnPview(6); fnGmode(0);
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 1); fnXrng(NOPARAM); fnYrng(NOPARAM);
    fnXvol(NOPARAM); fnYvol(NOPARAM); fnZvol(NOPARAM);
    pgTestSet3("0.5", "-1", "0.5"); fnEyept(NOPARAM);
  }

  void pgTestDraw3D(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    pgTestLoadProgram(pgTestPgmSaddle, sizeof(pgTestPgmSaddle), "SADL");
    pgTestLoadProgram(pgTestPgmPlane, sizeof(pgTestPgmPlane), "PLNE");
    pgTestLoadProgram(pgTestPgmErase, sizeof(pgTestPgmErase), "ERAS");

    // P1: the eight corners of the unit cube project to the recorded pixels.
    pgTestUnitCubeView();
    {
      static const float corner[8][3] = { {0,0,0},{1,0,0},{0,0,1},{1,0,1},{0,1,0},{1,1,0},{0,1,1},{1,1,1} };
      static const int32_t want[8][2] = { {0,239},{399,239},{0,0},{399,0},{100,179},{299,179},{100,60},{299,60} };
      pg3dView_t view; pg3dSetup_t s; int k;
      if(!pg3dRecordView(&view)) pgTestFail("P1 the unit-cube view is not valid");
      pg3dSetup(&s, &view);
      for(k = 0; k < 8; k++) {
        int32_t col, row;
        if(!pg3dProject(&s, corner[k][0], corner[k][1], corner[k][2], &col, &row) || col != want[k][0] || row != want[k][1]) {
          printf("program-graphics test FAIL: P1 corner %d projects to (%d, %d), expected (%d, %d)\n", k, (int)col, (int)row, (int)want[k][0], (int)want[k][1]);
          pgTestFailures++;
        }
      }
    }

    // P2: WIREFRAME of the plane z = 0 with a 2 by 2 grid lights the recorded count.
    {
      uint32_t lit;
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("P2 WIREFRAME of the plane raised an error"); lastErrorCode = ERROR_NONE; }
      lit = pgTestLitCanvas();
      if(lit != 798) { printf("program-graphics test FAIL: P2 the plane mesh lit %u pixels, expected 798\n", lit); pgTestFailures++; }
    }

    // P3: the block is 512 pool blocks, taken by the first 3D command in the view and returned at EXIT.
    {
      uint32_t before;
      pgCloseView(); calcMode = CM_NORMAL;
      pgTestSet3("0", "-3", "0");   // the registers first: a register write takes pool blocks of its own
      before = c47MemInBlocks;
      fnEyept(NOPARAM);
      if(c47MemInBlocks != before) pgTestFail("P3 EYEPT outside the view took pool memory");
      fnPview(6);
      before = c47MemInBlocks;
      fnEyept(NOPARAM);
      if(c47MemInBlocks != before + PG3D_BLOCKS) pgTestFail("P3 the first 3D command in the view did not take 512 blocks");
      pgCloseView(); calcMode = CM_NORMAL;
      if(c47MemInBlocks != before) pgTestFail("P3 EXIT did not return the block");
    }

    // P5: the byte encoding.
    if(pg3dEncode(0.0f / 0.0f, 0, 1) != 255 || pg3dEncode(0, 0, 1) != 0 || pg3dEncode(1, 0, 1) != 254 || pg3dEncode(7, 0, 1) != 254 || pg3dEncode(-7, 0, 1) != 0) pgTestFail("P5 the byte encoding is wrong");
    if(pg3dDecode(254, 0, 1) != 1.0f || pg3dDecode(0, 0, 1) != 0.0f) pgTestFail("P5 the byte decoding is wrong");

    // P9 and P26: each key changes its counter, in both directions.
    pgTestUnitCubeView();
    pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
    processKeyAction(ITM_RBR);   if(pg3d.angZ != 1)  pgTestFail("P9 RBR did not turn about z");
    processKeyAction(ITM_FLGSV); if(pg3d.angZ != 0)  pgTestFail("P26 FLGSV did not turn back about z");
    processKeyAction(ITM_BST);   if(pg3d.angY != 1)  pgTestFail("P9 BST did not turn about y");
    processKeyAction(ITM_SST);   if(pg3d.angY != 0)  pgTestFail("P26 SST did not turn back about y");
    processKeyAction(ITM_UP1);   if(pg3d.angX != 1)  pgTestFail("P9 UP did not turn about x");
    processKeyAction(ITM_DOWN1); if(pg3d.angX != 0)  pgTestFail("P26 DOWN did not turn back about x");
    processKeyAction(ITM_ADD);   if(pg3d.zoomStep != 1) pgTestFail("P9 plus did not zoom in");
    processKeyAction(ITM_SUB);   if(pg3d.zoomStep != 0) pgTestFail("P9 minus did not zoom out");
    processKeyAction(ITM_UP1); processKeyAction(ITM_ADD);
    processKeyAction(ITM_5);
    if(pg3d.angX != 0 || pg3d.angY != 0 || pg3d.angZ != 0 || pg3d.zoomStep != 0) pgTestFail("P9 the key 5 did not reset the view");
    processKeyAction(ITM_4);
    if(pg3d.angX != 0 || pg3d.zoomStep != 0) pgTestFail("P9 the key 4 changed the view");
    if(lastErrorCode != ERROR_NONE) { pgTestFail("P9 a key raised an error"); lastErrorCode = ERROR_NONE; }

    // P10: 36 UP presses return the canvas exactly.
    {
      int k; uint32_t d;
      processKeyAction(ITM_5);
      pgTestSnapCanvas();
      for(k = 0; k < 36; k++) processKeyAction(ITM_UP1);
      d = pgTestCanvasDiff();
      if(pg3d.angX != 0) pgTestFail("P10 36 presses did not close the turn");
      if(d != 0) { printf("program-graphics test FAIL: P10 %u canvas bytes differ after a full turn\n", d); pgTestFailures++; }
    }

    // P29: a body that calls ERASE leaves no valid grid (audit G4 round 1, Gemini H 2).
    fnErase(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe((uint16_t)findNamedLabel("ERAS", GLOBAL_LABELS));
    lastErrorCode = ERROR_NONE;
    if(pg3d.block != NULL && PG3D_HDR()->gridValid != 0) pgTestFail("P29 a body that emptied the block left a valid grid");
    pgTestUnitCubeView();
    pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));

    // P16: the stack survives WIREFRAME.
    pgTestWriteLonI(REGISTER_X, 1); pgTestWriteLonI(REGISTER_Y, 2); pgTestWriteLonI(REGISTER_Z, 3); pgTestWriteLonI(REGISTER_T, 4);
    fnWireframe((uint16_t)findNamedLabel("SADL", GLOBAL_LABELS));
    {
      int32_t v;
      if(!pgReadCoord(REGISTER_X, &v) || v != 1) pgTestFail("P16 X changed across WIREFRAME");
      if(!pgReadCoord(REGISTER_T, &v) || v != 4) pgTestFail("P16 T changed across WIREFRAME");
    }
    lastErrorCode = ERROR_NONE;

    // P19: ERASE empties the retained content; a key press then draws nothing.
    fnErase(NOPARAM);
    if(PG3D_HDR()->lineCount != 0 || PG3D_HDR()->gridValid != 0) pgTestFail("P19 ERASE kept the retained content");
    processKeyAction(ITM_UP1);
    if(pgTestLitCanvas() != 0) pgTestFail("P19 a key press after ERASE drew");

    // P20: bad counts and a bad range are refused.
    pgTestWriteLonI(REGISTER_X, 1);   fnNumx(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) { pgTestFail("P20 NUMX 1 was accepted"); }
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_X, 101); fnNumx(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) { pgTestFail("P20 NUMX 101 was accepted"); }
    lastErrorCode = ERROR_NONE;
    pgTestWriteReal(REGISTER_X, "2.5"); fnNumx(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) { pgTestFail("P20 NUMX 2.5 was accepted"); }
    lastErrorCode = ERROR_NONE;
    if(pg3d.numX != 2) { pgTestFail("P20 a refused NUMX changed the count"); }
    pgTestSetString(REGISTER_X, "X"); fnNumx(NOPARAM);
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) { pgTestFail("P20 NUMX with a string did not raise the type error"); }
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_Y, 1); pgTestWriteLonI(REGISTER_X, 1); fnXvol(NOPARAM);
    if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) { pgTestFail("P20 XVOL 1 1 was accepted"); }
    lastErrorCode = ERROR_NONE;

    // P23: LINE3D without a current point sets it and draws nothing.
    fnErase(NOPARAM);
    pg3d.haveCur = 0;
    pgTestSet3("0.2", "0.2", "0.2"); fnLine3d(NOPARAM);
    if(pgTestLitCanvas() != 0) pgTestFail("P23 LINE3D without a current point drew");
    if(!pg3d.haveCur) pgTestFail("P23 LINE3D without a current point did not set it");

    // P12: 330 lines fill the block; the 331st draws and is not retained.
    {
      uint32_t k, litBefore;
      uint8_t header[PG3D_HEADER_BYTES];
      fnErase(NOPARAM);
      pgTestSet3("0", "0", "0"); fnPt3d(NOPARAM);
      for(k = 0; k < 330; k++) {
        char b[16]; sprintf(b, "%.4f", 0.1f + 0.8f * (float)(k % 7) / 7.0f);
        pgTestSet3((k & 1) ? "0.9" : "0.1", b, (k & 2) ? "0.9" : "0.1"); fnLine3d(NOPARAM);
      }
      if(PG3D_HDR()->lineCount != 330) pgTestFail("P12 330 lines did not fill the block");
      memcpy(header, pg3d.block, PG3D_HEADER_BYTES);
      lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, SCREEN_HEIGHT - PG_TOP_ROW, LCD_SET_VALUE);   // a clear canvas, the block untouched
      litBefore = pgTestLitCanvas();
      pgTestSet3("0.5", "0.5", "0.5"); fnLine3d(NOPARAM);
      if(pgTestLitCanvas() <= litBefore) pgTestFail("P12 the 331st line did not draw");
      if(PG3D_HDR()->lineCount != 330) pgTestFail("P12 the 331st line was retained past the block");
      if(memcmp(header, pg3d.block, PG3D_HEADER_BYTES) != 0) pgTestFail("P12 the 331st line changed the header");
    }

    // P27: a point exactly one 1024th of the depth in front of the eye is drawable (audit G4 round 1, Sol G 2).
    {
      pg3dView_t view; pg3dSetup_t s; int32_t col, row;
      pgReset();
      pg3d.ylo = 0.0f; pg3d.yhi = 1024.0f; pg3d.eyeY = -1.0f;
      if(!pg3dRecordView(&view)) pgTestFail("P27 the view is not valid");
      pg3dSetup(&s, &view);
      if(!pg3dProject(&s, 0.0f, 0.0f, 0.0f, &col, &row)) pgTestFail("P27 a point at exactly eps was rejected");
      if(pg3dProject(&s, 0.0f, -0.5f, 0.0f, &col, &row)) pgTestFail("P27 a point nearer than eps was drawn");
      pgReset();
    }

    // P28: the clamp holds on the final row as well (audit G4 round 1, Sol G 3).
    {
      pg3dView_t view; pg3dSetup_t s; int32_t col, row;
      pgReset();
      pg3d.xlo = -2.0f; pg3d.xhi = 2.0f; pg3d.ylo = -1.0f; pg3d.yhi = 1.0f; pg3d.zlo = -2.0f; pg3d.zhi = 2.0f;
      pg3d.eyeX = 0.0f; pg3d.eyeY = -3.0f; pg3d.eyeZ = 0.0f;
      pgTestWriteReal(REGISTER_Y, "0");           pgTestWriteReal(REGISTER_X, "0.009975");  fnXrng(NOPARAM);   // 399 / 40000
      pgTestWriteReal(REGISTER_Y, "-0.005939511"); pgTestWriteReal(REGISTER_X, "0");         fnYrng(NOPARAM);   // -239 / 40239
      if(!pg3dRecordView(&view)) pgTestFail("P28 the view is not valid");
      pg3dSetup(&s, &view);
      if(!pg3dProject(&s, 2.0f, -1.0f, -2.0f, &col, &row)) pgTestFail("P28 the far corner was rejected");
      if(col != 32000 || row != 32000) { printf("program-graphics test FAIL: P28 the far corner projects to (%d, %d), expected (32000, 32000)\n", (int)col, (int)row); pgTestFailures++; }
      pgWindow.set = 0;
      pgReset();
    }

    // P20b: a volume span that overflows float is refused (audit G4 round 1, Sol I 1).
    pgTestWriteReal(REGISTER_Y, "-2e38"); pgTestWriteReal(REGISTER_X, "2e38"); fnXvol(NOPARAM);
    if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("P20b XVOL -2e38 2e38 was accepted");
    lastErrorCode = ERROR_NONE;

    // P18: the reset hook forgets the block without a free and restores the defaults.
    {
      uint32_t before = c47MemInBlocks;
      uint8_t *held = pg3d.block;
      pgReset();
      if(pg3d.block != NULL) pgTestFail("P18 reset kept the block pointer");
      if(c47MemInBlocks != before) pgTestFail("P18 reset freed the block");
      if(pg3d.eyeY != -3.0f || pg3d.xlo != -1.0f || pg3d.numX != 10 || pg3d.numY != 8) pgTestFail("P18 reset did not restore the HP defaults");
      freeC47Blocks(held, PG3D_BLOCKS);   // the pin returns the block the reset forgot
    }

    pgCloseView();
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // S3: the 3D showcase and the animation frames (TESTING.md §6, DESIGN.md §9.7).
  void pgTestShowcase3D(uint16_t unusedButMandatoryParameter) {
    uint32_t lit, k;
    char name[24];
    uint16_t saddle;
    pgTestFailures = 0;
    pgTestLoadProgram(pgTestPgmSaddle, sizeof(pgTestPgmSaddle), "SADL");
    saddle = (uint16_t)findNamedLabel("SADL", GLOBAL_LABELS);
    calcMode = CM_NORMAL; lastErrorCode = ERROR_NONE;
    pgReset();
    fnPview(6); fnGmode(0);
    pgTestWriteReal(REGISTER_Y, "-1");   pgTestWriteReal(REGISTER_X, "1");   fnXrng(NOPARAM);
    pgTestWriteReal(REGISTER_Y, "-0.6"); pgTestWriteReal(REGISTER_X, "0.6"); fnYrng(NOPARAM);
    pgTestSet3("0", "-3", "0"); fnEyept(NOPARAM);
    pgTestWriteReal(REGISTER_Y, "-1"); pgTestWriteReal(REGISTER_X, "1"); fnXvol(NOPARAM); fnYvol(NOPARAM); fnZvol(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 24); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe(saddle);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("S3 WIREFRAME of the saddle raised an error"); lastErrorCode = ERROR_NONE; }
    lit = pgTestLitCanvas();
    printf("program-graphics showcase 3D: saddle alone %u lit pixels\n", lit);
    {
      static const char *v[8][3] = { {"-1","-1","-1"},{"1","-1","-1"},{"1","1","-1"},{"-1","1","-1"},{"-1","-1","1"},{"1","-1","1"},{"1","1","1"},{"-1","1","1"} };
      static const int path[10] = { 0, 1, 2, 3, 0, 4, 5, 6, 7, 4 };
      int i;
      pgTestSet3(v[0][0], v[0][1], v[0][2]); fnPt3d(NOPARAM);
      for(i = 1; i < 10; i++) { pgTestSet3(v[path[i]][0], v[path[i]][1], v[path[i]][2]); fnLine3d(NOPARAM); }
      pgTestSet3(v[1][0], v[1][1], v[1][2]); fnPt3d(NOPARAM); pgTestSet3(v[5][0], v[5][1], v[5][2]); fnLine3d(NOPARAM);
      pgTestSet3(v[2][0], v[2][1], v[2][2]); fnPt3d(NOPARAM); pgTestSet3(v[6][0], v[6][1], v[6][2]); fnLine3d(NOPARAM);
      pgTestSet3(v[3][0], v[3][1], v[3][2]); fnPt3d(NOPARAM); pgTestSet3(v[7][0], v[7][1], v[7][2]); fnLine3d(NOPARAM);
    }
    pgTestSetString(REGISTER_X, "program-graphics G4: EYEPT XVOL YVOL ZVOL NUMX NUMY WIREFRAME PT3D LINE3D");
    fnGdisp(1);
    lit = pgTestLitCanvas();
    printf("program-graphics showcase 3D: %u lit pixels in rows 20 to 239\n", lit);
    if(lit != 8656) { printf("program-graphics test FAIL: S3 the showcase count moved from the recorded %u\n", (unsigned)0u /* recorded at the first green run */); pgTestFailures++; }
    strcpy(_ioFileNameOverride, "pg3d_000.bmp"); fnScreenDump(0);
    processKeyAction(ITM_5);   // frame 001: the home view without the caption; a redraw with nothing to change draws the same picture
    pg3dRedraw();
    strcpy(_ioFileNameOverride, "pg3d_001.bmp"); fnScreenDump(0);
    pgTestSnapCanvas();
    k = 2;
    #define PG_FRAME(item) do { processKeyAction(item); sprintf(name, "pg3d_%03u.bmp", (unsigned)k++); strcpy(_ioFileNameOverride, name); fnScreenDump(0); } while(0)
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_UP1); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1 a full turn about x did not return the canvas");
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_BST); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1y a full turn about y did not return the canvas");
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_RBR); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1z a full turn about z did not return the canvas");
    { int i; for(i = 0; i < 6; i++) PG_FRAME(ITM_ADD); }
    { int i; for(i = 0; i < 6; i++) PG_FRAME(ITM_SUB); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R2 six zoom steps in and out did not return the canvas");
    #undef PG_FRAME
    printf("program-graphics showcase 3D: %u frames, %u program runs\n", (unsigned)k, (unsigned)pg3dRunCount);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("S3 an error was raised during the animation"); lastErrorCode = ERROR_NONE; }
    pgCloseView();
    calcMode = CM_NORMAL;
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
