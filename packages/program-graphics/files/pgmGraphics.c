// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.c
 * program-graphics package: drawing commands for user programs.
 */
#include "pgmGraphics.h"
#include "c47.h"

static pgCanvas_t canvas;
static void pg3dEmpty(void);
static void pg3dFreeBlock(void);
static void pgReleaseAbandoned(void);
static void pg3dHome(void);

// World coordinate window. When unset, a user coordinate maps to one pixel.
static struct {
  uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
  real34_t xmin, xmax, ymin, ymax;
} pgWindow;

// Flushes changed rows to the LCD and records the refresh timestamp.
static void pgRefreshNow(void) {
  canvas.lastRefreshMs = getUptimeMs();
  #if defined(DMCP_BUILD)
    lcd_refresh_dma();
  #else // !DMCP_BUILD
    lcd_refresh();
  #endif // DMCP_BUILD
}

// Sets the canvas region and clipping rectangle, then clears the area to white.
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
  pg3dEmpty();   // Clears retained 3D geometry.
  pg3dHome();    // a cleared canvas starts from the home view
}

// PVIEW: opens the canvas view over region 2 (registers) or 6 (full screen).
void fnPview(uint16_t region) {
  pgReleaseAbandoned();
  if(region != PG_REGION_REGISTERS && region != PG_REGION_FULL) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }
  if(calcMode != CM_GRAPHICS_CANVAS) {
    canvas.prevCalcMode = calcMode;
  }
  if(calcMode == CM_AIM) {   // Hide cursor so it does not blink over the canvas.
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

// ERASE: clears the canvas region. Opens region 2 view if currently closed.
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

// Returns CM_NORMAL for a running program inside the canvas view.
uint8_t pgEffectiveCalcMode(void) {
  if(calcMode == CM_GRAPHICS_CANVAS && programRunStop == PGM_RUNNING) {
    return CM_NORMAL;
  }
  return calcMode;
}

// Redraws the status bar, the softmenu for region 2, and any active error message.
void pgRefreshCanvasView(void) {
  refreshStatusBar();
  if(canvas.region == PG_REGION_REGISTERS) {
    // Clear softmenu band before redrawing.
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

// Closes the canvas view and restores the previous calculation mode.
void pgCloseView(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    return;
  }
  pg3dFreeBlock();   // Release the retained 3D memory block.
  pg3dHome();        // the next view starts from the home view
  calcMode = canvas.prevCalcMode;
  canvas.region = 0;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = SCRUPD_AUTO;
  if(calcMode == CM_AIM) {   // Restore cursor for alpha input.
    setSystemFlag(FLAG_ALPHA);
  }
  refreshScreen(197);
  if(calcMode == CM_AIM) {   // Re-enable cursor after screen refresh.
    cursorEnabled = true;
  }
}

// Called before the calculator state is saved: the canvas view does not survive a save.
void pgBeforeSave(void) {
  if(calcMode == CM_GRAPHICS_CANVAS) pgCloseView();
  pgReleaseAbandoned();
}


// ---------------------------------------------------------------------------
// 2D drawing kernel and argument parsing
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

// Draws one pixel at the specified column and row.
static void pgPixel(const pgRect_t *c, int32_t col, int32_t row) {
  uint32_t xm;
  if(col < c->x0 || col > c->x1 || row < c->y0 || row > c->y1) {
    return;
  }
  xm = (uint32_t)(SCREEN_WIDTH - 1 - col);
  pgApply(pgRowPtr(row) + 2 + (xm >> 3), (uint8_t)(1u << (xm & 7)));
  pgRowPtr(row)[0] = 1u;
}

// Draws a horizontal line between col0 and col1 on the specified row.
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

// Draws a line between two points with the Bresenham algorithm.
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

// 64-bit integer square root. Radius fill needs 4*r^2, which exceeds INT32_MAX for large r.
static int64_t pgIsqrt(int64_t v) {
  int64_t r = 0, bit;
  if(v <= 0) return 0;
  for(bit = (int64_t)1 << 31; bit > 0; bit >>= 1) {
    if((r + bit) * (r + bit) <= v) r += bit;
  }
  return r;
}

// Draws a circle outline or filled circle using screen coordinates.
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

// Tests if direction (dx, dy) falls within the counterclockwise span from A to B.
static bool_t pgInSpan(int32_t ax, int32_t ay, int32_t bx, int32_t by, bool_t wide, int32_t dx, int32_t dy) {
  int64_t ca = (int64_t)ax * dy - (int64_t)ay * dx;   // cross(A, P)
  int64_t cb = (int64_t)dx * by - (int64_t)dy * bx;   // cross(P, B)
  if(wide) {
    return !(ca < 0 && cb < 0);
  }
  return ca >= 0 && cb >= 0;
}

// Draws an arc outline in user coordinates (y upward).
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

// ---- Argument parsing ----

static void pgError(uint16_t code) {
  displayCalcErrorMessage(code, ERR_REGISTER_LINE, REGISTER_X);
}

#define PG_AXIS_X    0
#define PG_AXIS_Y    1
#define PG_AXIS_NONE 2   // Radius in pixels (not transformed by window)

// Converts a real coordinate to screen pixels. Rounds half away from zero.
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

// Reads a coordinate from regist into *v (long integer = pixel, real = window mapped).
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

// Reads complex register as point (real part = X, imaginary part = Y).
static bool_t pgReadComplexPoint(calcRegister_t regist, int32_t *x, int32_t *y) {
  if(getRegisterDataType(regist) != dtComplex34) {
    pgError(ERROR_INVALID_DATA_TYPE_FOR_OP);
    return false;
  }
  return pgRealToPixel(REGISTER_REAL34_DATA(regist), PG_AXIS_X, x) && pgRealToPixel(REGISTER_IMAG34_DATA(regist), PG_AXIS_Y, y);
}

// Reads a range boundary for XRNG and YRNG from the specified register.
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

// Sets axis range from Y (minimum) and X (maximum). Equal values cause an error.
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

// Reads an angle from regist into a real in the current angular mode.
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

// Throttles screen refreshes during program execution to a 40 ms cadence.
static void pgRefreshMaybe(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    // Prevent automatic screen refreshes from overwriting drawn pixels.
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

// Reads two points from (X, Y) and (Z, T), or two complex points from Y and X.
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

// Draws an arc: center in T (complex), radius in Z, start angle in Y, end angle in X.
void fnGarc(uint16_t unusedButMandatoryParameter) {
  int32_t cx, cy, r, ax, ay, bx, by;
  real_t a1, a2, s, co, t, d, full;
  float f;
  bool_t wide, fullCircle;
  pgRect_t c;
  if(!pgReadComplexPoint(REGISTER_T, &cx, &cy)) return;
  if(!pgReadCoord(REGISTER_Z, &r) || !pgReadAngle(REGISTER_Y, &a1) || !pgReadAngle(REGISTER_X, &a2)) return;
  // Draw a full circle if the angular span is 360 degrees or more.
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
      // Distinguish near-zero span from near-full circle when scaled directions match.
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

// Finds largest glyph boundary at or below n. Bytes >= 0x80 start two-byte glyphs.
static size_t pgGlyphBoundary(const char *s, size_t n) {
  size_t i = 0, last = 0;
  while(i < n && s[i] != 0) {
    last = i;
    i += ((uint8_t)s[i] & 0x80) ? 2 : 1;
  }
  return (i == n) ? n : last;
}

// Copies string from regist into tmpString, truncated to at most width pixels.
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
    // Remove the last multi-byte or single-byte glyph.
    size_t i = 0, last = 0;
    while(tmpString[i] != 0) {
      last = i;
      i += ((uint8_t)tmpString[i] & 0x80) ? 2 : 1;
    }
    tmpString[last] = 0;
  }
  return true;
}

// Draws text at coordinate (X, Y) with string in Z.
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

// Draws string from X on canvas line n (lines 1 to 11).
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

// Sets clipping rectangle between (X, Y) and (Z, T), bounded by the canvas region.
void fnGclip(uint16_t unusedButMandatoryParameter) {
  int32_t x0, r0, x1, r1;
  int32_t regionBottom = (canvas.region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  if(!pgReadTwoPoints(&x0, &r0, &x1, &r1)) return;
  if(x0 > x1) { int32_t t = x0; x0 = x1; x1 = t; }
  if(r0 > r1) { int32_t t = r0; r0 = r1; r1 = t; }
  // Intersects rectangle with canvas region. Out-of-bounds rectangle disables drawing.
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
// 3D drawing: state, projection, retained block, and interactive keys
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
  uint8_t  angX, angY, angZ;   // Step counts (0 to 35, one step is 10 degrees)
  int8_t   zoomStep;           // Zoom level (-8 to 8)
  uint8_t  reserved;
  uint8_t *block;              // Retained 3D block (NULL when none)
} pg3d_t;

typedef struct {
  uint8_t  numX, numY;     // Grid sample counts (0 = no grid)
  uint8_t  gridValid;      // 1 when all grid points were evaluated
  uint8_t  frozen;         // 1 after first record is stored
  uint16_t lineCount;
  uint16_t label;          // Target label of last WIREFRAME run (0 = none)
  float    xlo, xhi, ylo, yhi, zlo, zhi;
  float    zRecLo, zRecHi; // Z range spanned by grid samples
  float    eyeX, eyeY, eyeZ;
  uint8_t  reserved[12];
} pg3dHeader_t;
_Static_assert(sizeof(pg3dHeader_t) == PG3D_HEADER_BYTES, "pg3dHeader_t must be PG3D_HEADER_BYTES bytes");

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
#if defined(TESTSUITE_BUILD)
  static bool_t pgTestFailNextUndoSave;   // test seam: the next undo save reports ERROR_RAM_FULL
#endif

#define PG3D_HDR() ((pg3dHeader_t *)pg3d.block)
#define PG3D_GRID() (pg3d.block + PG3D_HEADER_BYTES)

// Resets 3D state and parameters to defaults. Does not free memory block.
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

// True while the header h still describes the grid of numX by numY that a run writes.
static bool_t pg3dGridIntact(const pg3dHeader_t *h, uint32_t numX, uint32_t numY) {
  return pg3d.block != NULL && h == PG3D_HDR() && h->frozen && h->numX == numX && h->numY == numY;
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

// Allocates the 3D retained memory block on first use.
static bool_t pg3dEnsure(void) {
  pgReleaseAbandoned();
  if(calcMode != CM_GRAPHICS_CANVAS) return true;
  if(pg3d.block != NULL) return true;
  pg3d.block = allocC47Blocks(PG3D_BLOCKS);
  if(pg3d.block == NULL) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  memset(pg3d.block, 0, PG3D_BLOCK_BYTES);
  return true;
}

static void pg3dEmpty(void) {   // Clears stored 3D content while keeping memory block.
  pg3d.haveCur = 0;
  if(pg3d.block == NULL) return;
  memset(pg3d.block, 0, PG3D_HEADER_BYTES);
}

static void pg3dFreeBlock(void) {
  freeC47Blocks(pg3d.block, PG3D_BLOCKS);
  pg3d.block = NULL;
  pg3d.haveCur = 0;
}

// Returns the angles and the zoom to the home view.
static void pg3dHome(void) {
  pg3d.angX = pg3d.angY = pg3d.angZ = 0; pg3d.zoomStep = 0;
}

// A plot step leaves the view without EXIT. Releases the region and the block that view still holds.
static void pgReleaseAbandoned(void) {
  if(calcMode != CM_GRAPHICS_CANVAS && canvas.region != 0) {
    canvas.region = 0;
    pg3dFreeBlock();
  }
}

static bool_t pg3dViewValid(const pg3dView_t *v) {
  return v->xlo < v->xhi && v->ylo < v->yhi && v->zlo < v->zhi && v->eyeY < v->ylo;
}

// Copies or freezes the 3D volume and eye point for retained drawing.
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

// 36-entry sine table for 10-degree rotation steps.
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

static const float pg3dZoom[17] = {   // Zoom scale factors: 1.25^k for k from -8 to 8
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

// True when lo < hi and the byte scale 254 / (hi - lo) is a finite float.
static bool_t pg3dSpanUsable(float lo, float hi) {
  float d = hi - lo, sc;
  if(!(lo < hi) || !(d - d == 0.0f)) return false;
  sc = 254.0f / d;
  return sc - sc == 0.0f;
}

// True when each set window axis converts to finite floats with a finite, non-zero pixel scale. A mirrored window passes.
static bool_t pg3dWindowUsable(void) {
  if(pgWindow.set & 1) {
    float mn = pg3dReal34ToFloat(&pgWindow.xmin), mx = pg3dReal34ToFloat(&pgWindow.xmax), d = mx - mn, sc;
    if(!(mn - mn == 0.0f) || !(mx - mx == 0.0f) || !(d - d == 0.0f) || d == 0.0f) return false;
    sc = 399.0f / d;
    if(!(sc - sc == 0.0f)) return false;
  }
  if(pgWindow.set & 2) {
    float mn = pg3dReal34ToFloat(&pgWindow.ymin), mx = pg3dReal34ToFloat(&pgWindow.ymax), d = mx - mn, sc;
    if(!(mn - mn == 0.0f) || !(mx - mx == 0.0f) || !(d - d == 0.0f) || d == 0.0f) return false;
    sc = 239.0f / d;
    if(!(sc - sc == 0.0f)) return false;
  }
  return true;
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

// Projects a 3D point to screen coordinates through rotation and perspective.
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

// Draws mesh lines from point (i, j) to neighbors (i-1, j) and (i, j-1).
static void pg3dMeshPoint(const pg3dSetup_t *s, pg3dPix_t *rows, uint32_t numX, uint32_t i, uint32_t j, float x, float y, float z, const pgRect_t *clip) {
  pg3dPix_t *cur = rows + (j & 1) * numX, *prev = rows + ((j + 1) & 1) * numX;
  int32_t col = 0, row = 0;
  bool_t ok = (z == z) && pg3dProject(s, x, y, z, &col, &row);
  cur[i].col = ok ? (int16_t)col : PG3D_NOPIX; cur[i].row = ok ? (int16_t)row : 0;
  if(!ok) return;
  if(i > 0 && cur[i - 1].col != PG3D_NOPIX) pgLine(clip, cur[i - 1].col, cur[i - 1].row, col, row);
  if(j > 0 && prev[i].col != PG3D_NOPIX)    pgLine(clip, prev[i].col, prev[i].row, col, row);
}

// ---- WIREFRAME ----

#define PG3D_RUN_OK       0
#define PG3D_RUN_ABORTED  1
#define PG3D_RUN_ALLHOLES 2

// Evaluates user program at (x, y) with inputs in X/Y/Z/T. Returns z.
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

// Grid coordinate i of n between lo and hi. The still picture and the redraw both use it.
static float pg3dGridCoord(float lo, float hi, uint32_t i, uint32_t n) {
  return lo + (float)i * ((hi - lo) / (float)(n - 1));
}

// Evaluates surface grid. Stores samples if retain is true; draws if draw is true.
static int pg3dRunGrid(const pg3dSetup_t *s, pg3dHeader_t *h, pg3dPix_t *rows, uint32_t numX, uint32_t numY, bool_t retain, uint16_t label, bool_t draw, const pgRect_t *clip) {
  uint32_t i, j, holes = 0;
  pg3dLastPointError = ERROR_NONE;
  for(j = 0; j < numY; j++) {
    float y = pg3dGridCoord(s->v.ylo, s->v.yhi, j, numY);
    for(i = 0; i < numX; i++) {
      float x = pg3dGridCoord(s->v.xlo, s->v.xhi, i, numX);
      float z; uint16_t err; uint8_t b;
      if(lastErrorCode == ERROR_SOLVER_ABORT || programRunStop == PGM_WAITING || exitKeyWaiting()) {
        lastErrorCode = engineNestingWasRefused ? ERROR_NESTING_TOO_DEEP : ERROR_SOLVER_ABORT;
        if(programRunStop == PGM_RUNNING) programRunStop = PGM_WAITING;
        return PG3D_RUN_ABORTED;
      }
      z = pg3dSample(label, x, y, &err);
      if(err != ERROR_NONE) { holes++; pg3dLastPointError = err; }
      if(retain && !pg3dGridIntact(h, numX, numY)) retain = false;   // the body emptied, reset, or refroze the block
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

// Saves calculator engine state and undo image before running user program.
typedef struct {
  uint16_t program, local, resets;
  uint8_t *step;
} pg3dEngineSave_t;

// Returns false when the undo image could not be saved; nothing is changed then.
static bool_t pg3dEngineEnter(pg3dEngineSave_t *sv) {
  currentKeyCode = 255;
  #if defined(TESTSUITE_BUILD)
    if(pgTestFailNextUndoSave) { pgTestFailNextUndoSave = false; lastErrorCode = ERROR_RAM_FULL; }
    else
  #endif
  saveForUndo();
  if(lastErrorCode == ERROR_RAM_FULL) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  ++engineNestingDepth;
  ++plotEngineActive;
  ++currentSolverNestingDepth;
  setSystemFlag(FLAG_SOLVING);
  sv->program = currentProgramNumber;
  sv->local = currentLocalStepNumber;
  sv->step = currentStep;
  sv->resets = pg3dResetCount;
  return true;
}

static void pg3dEngineLeave(const pg3dEngineSave_t *sv) {
  currentProgramNumber = sv->program;
  currentLocalStepNumber = sv->local;
  currentStep = sv->step;
  if(--currentSolverNestingDepth == 0) clearSystemFlag(FLAG_SOLVING);
  --plotEngineActive;
  --engineNestingDepth;
  temporaryInformation = TI_NO_INFO;
  fnUndo(0);   // Consumes the undo image to restore user state.
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
  if(REGISTER_X <= label && label <= REGISTER_T) {   // Resolve named label from register.
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
  if(!pg3dWindowUsable()) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }
  if(!pg3dEnsure()) return;
  if(!pg3dRecordView(&view)) return;
  numX = pg3d.numX; numY = pg3d.numY;
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  retain = (h != NULL) && (numX * numY <= pg3dFreeBytes(h) + (uint32_t)h->numX * h->numY);   // the old grid does not count
  rows = allocC47Blocks(TO_BLOCKS(2 * numX * sizeof(pg3dPix_t)));
  if(rows == NULL) {
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  if(!pg3dEngineEnter(&sv)) {
    freeC47Blocks(rows, TO_BLOCKS(2 * numX * sizeof(pg3dPix_t)));
    return;
  }
  if(h != NULL) {
    h->gridValid = 0; h->numX = 0; h->numY = 0;
    if(retain) { h->numX = (uint8_t)numX; h->numY = (uint8_t)numY; }
  }
  pg3dSetup(&s, &view);
  pgClipNow(&clip);
  result = pg3dRunGrid(&s, h, rows, numX, numY, retain, label, true, &clip);
  if(pg3dResetCount == sv.resets) freeC47Blocks(rows, TO_BLOCKS(2 * numX * sizeof(pg3dPix_t)));
  pg3dEngineLeave(&sv);
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(result == PG3D_RUN_ALLHOLES) {
    displayCalcErrorMessage(pg3dLastPointError, ERR_REGISTER_LINE, REGISTER_X);
  }
  else if(result == PG3D_RUN_OK && retain && pg3dGridIntact(h, numX, numY)) {   // a body that emptied the block leaves no valid grid
    h->gridValid = 1; h->label = label;
  }
  pgRefreshNow();
}

// ---- 3D setup, PT3D, and LINE3D ----

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

static bool_t pg3dReadPoint(float *x, float *y, float *z) {   // Reads (x, y, z) from registers (Z, Y, X).
  return pg3dReadFloat(REGISTER_Z, x) && pg3dReadFloat(REGISTER_Y, y) && pg3dReadFloat(REGISTER_X, z);
}

static bool_t pg3dReadCount(uint8_t *n) {   // Reads an integer count (2 to 100) from register X.
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

static void pg3dRange(float *lo, float *hi) {   // Reads range from Y (low) and X (high).
  float a, b;
  if(!pg3dReadFloat(REGISTER_Y, &a) || !pg3dReadFloat(REGISTER_X, &b)) return;
  if(!pg3dSpanUsable(a, b)) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }   // the span and the byte scale must be finite
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
  if(!pg3dWindowUsable()) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }
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

// ---- Interactive keys, redraw, and zoom recalculation ----

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
        float y = pg3dGridCoord(view.ylo, view.yhi, j, h->numY);
        for(i = 0; i < h->numX; i++) {
          float x = pg3dGridCoord(view.xlo, view.xhi, i, h->numX);
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

// Re-evaluates grid samples across visible Z range after a zoom change.
static void pg3dRerun(pg3dHeader_t *h, pg3dView_t *view, float zNewLo, float zNewHi) {
  pg3dEngineSave_t sv;
  pg3dSetup_t s;
  uint32_t numX = h->numX, numY = h->numY;
  uint16_t label = h->label;
  int result;
  if(engineNestingRefused(true)) return;
  if(!pg3dEngineEnter(&sv)) return;
  view->zRecLo = zNewLo; view->zRecHi = zNewHi;
  pg3dSetup(&s, view);
  result = pg3dRunGrid(&s, h, NULL, numX, numY, true, label, false, NULL);
  pg3dEngineLeave(&sv);
  h = (pg3d.block != NULL) ? PG3D_HDR() : NULL;
  if(h == NULL) return;
  if(result == PG3D_RUN_OK && pg3dGridIntact(h, numX, numY)) { h->zRecLo = zNewLo; h->zRecHi = zNewHi; }
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
  pg3dSetup(&s, &view);
  zoom = pg3dZoom[pg3d.zoomStep + 8];
  dNear = (view.ylo - view.eyeY) / zoom;
  wymax = s.wymin + 239.0f / s.wys;
  zVisLo = view.eyeZ + (s.wymin - view.eyeZ) * dNear;
  zVisHi = view.eyeZ + (wymax - view.eyeZ) * dNear;
  if(zVisLo > zVisHi) { float t = zVisLo; zVisLo = zVisHi; zVisHi = t; }
  zNewLo = (zVisLo > view.zlo) ? zVisLo : view.zlo;
  zNewHi = (zVisHi < view.zhi) ? zVisHi : view.zhi;
  if(!pg3dSpanUsable(zNewLo, zNewHi)) return;
  pps = (h->zRecHi - h->zRecLo) * (1.0f / 254.0f) * s.wys / dNear;
  if(pps < 0.0f) pps = -pps;
  wider = (zNewLo < h->zRecLo) || (zNewHi > h->zRecHi);
  if(!(pps > 1.0f) && !wider) return;
  pg3dRerun(h, &view, zNewLo, zNewHi);
}

// Handles interactive 3D navigation keys (rotations and zoom).
void pg3dKey(int16_t item) {

  pg3dHeader_t *h;
  if(pg3d.block == NULL) return;
  h = PG3D_HDR();
  if(h->gridValid == 0 && h->lineCount == 0) return;
  if(!pg3dWindowUsable()) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }
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
    if(strcmp(indexOfItems[LAST_ITEM].itemSoftmenuName, "Last item") != 0 ||
       indexOfItems[LAST_ITEM].func != itemToBeCoded) {
      pgTestFail("S0 the last item row is not upstream's sentinel");
    }
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
      // Measures 100,000 steps of 100-pixel LINE execution time.
      uint32_t lineMs;
      pgTestWriteLonI(REGISTER_X, 0);   pgTestWriteLonI(REGISTER_Y, 0);
      pgTestWriteLonI(REGISTER_Z, 100); pgTestWriteLonI(REGISTER_T, 50);
      lineMs = pgTestRunSteps(ITM_GLINE, 100000);
      printf("program-graphics baseline: 100000 LINE steps of 100 pixels, %u ms\n", lineMs);
    }
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // Tests canvas view mode, clipping, and screen refresh.
  void pgTestView(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;

    // V1: PVIEW 2 enters canvas view and clears rows 20 to 170 only.
    setBlackPixel(10, 100);
    setBlackPixel(10, 200);
    fnPview(2);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V1 PVIEW 2 did not enter the canvas view");
    if(lcd_buffer_pixel_on(10, 100))     pgTestFail("V1 PVIEW 2 left a pixel lit in the register region");
    if(canvas.clipY1 != PG_REGISTER_BOTTOM_ROW) pgTestFail("V1 PVIEW 2 did not clip at row 170");
    // Rows 171 to 239 belong to softmenu region 2. Clip row verifies boundary.

    // V2: PVIEW 6 clears rows 20 to 239.
    setBlackPixel(10, 200);
    fnPview(6);
    if(lcd_buffer_pixel_on(10, 200))     pgTestFail("V2 PVIEW 6 left a pixel lit in the softmenu region");
    if(canvas.clipY1 != SCREEN_HEIGHT - 1) pgTestFail("V2 PVIEW 6 did not clip at row 239");

    // V3: PVIEW 3 raises an error and changes no state.
    fnPview(3);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("V3 PVIEW 3 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("V3 PVIEW 3 changed the mode");

    // V4: stop path repaint preserves the canvas.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(!lcd_buffer_pixel_on(50, 100))    pgTestFail("V4 refreshScreen in the canvas view erased the drawing");

    // V8: repaint restores the status bar after clearing.
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

    // V5: VIEW inside the canvas view does not overwrite the canvas.
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

    // V7: without PVIEW, normal repaint erases the drawing.
    setBlackPixel(50, 100);
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(lcd_buffer_pixel_on(50, 100))     pgTestFail("V7 a drawing without PVIEW survived the normal repaint");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    // V9: softmenu band clears before painting so blank base leaves no labels.
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

  // Key handling tests in canvas view.
  void pgTestKeys(uint16_t unusedButMandatoryParameter) {
    pgTestFailures = 0;
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    fnPview(2);

    // K1: mode is in the range blocked by the three softkey gates.
    if(!(CM_GRAPHICS_CANVAS >= 19 && CM_GRAPHICS_CANVAS <= 23)) {
      pgTestFail("K1 CM_GRAPHICS_CANVAS is outside the package browser range 19 to 23");
    }

    // K2: non-exit keys do not alter canvas. Tests press and release paths.
    // ENTER, BACKSPACE, UP, DOWN, and .d reach their key function at release.
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

    // K4: CLSTK leaves the view open and the drawing intact at repaint.
    runFunction(ITM_CLSTK);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K4 CLSTK closed the canvas view");
    screenUpdatingMode = SCRUPD_AUTO;
    refreshScreen(4);
    if(!lcd_buffer_pixel_on(60, 100))    pgTestFail("K4 the repaint after CLSTK erased the canvas");
    lastErrorCode = ERROR_NONE;

    // K5: ENTER as a program step inside the view lifts the stack.
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

    // K6: CC and .ms inside the view do nothing and show no error display.
    runFunction(ITM_CC);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K6 CC from the keyboard left the canvas view");
    runFunction(ITM_ms);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K6 .ms from the keyboard left the canvas view");
    lastErrorCode = ERROR_NONE;

    // K7: error displays on line 1; EXIT clears error without redrawing Z line.
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

    // K3: EXIT closes the view on release.
    processKeyAction(ITM_EXIT1);
    if(calcMode != CM_GRAPHICS_CANVAS)   pgTestFail("K3 the EXIT press alone left the canvas view");
    runFunction(ITM_EXIT1);
    if(calcMode != CM_NORMAL)            pgTestFail("K3 EXIT did not close the canvas view");

    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    // K8: alpha input mode disables cursor and FLAG_ALPHA; EXIT restores them.
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

    // K9: key press shows no function name and release redraws no register line.
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

    // K10: softkey press inside view causes no action.
    showSoftmenu(-MNU_CANVAS);
    setBlackPixel(100, 100);
    {
      GdkEventButton ev;
      memset(&ev, 0, sizeof(ev));
      ev.type = GDK_BUTTON_PRESS;
      btnFnPressed(NULL, (GdkEvent *)&ev, "2");
      ev.type = GDK_BUTTON_RELEASE;
      btnFnReleased(NULL, (GdkEvent *)&ev, "2");
      btnFnClicked(NULL, "2");   // Simulate double-tap timeout.
    }
    if(calcMode != CM_GRAPHICS_CANVAS) pgTestFail("K10 a softkey press left the view");
    if(!lcd_buffer_pixel_on(100, 100)) pgTestFail("K10 a softkey press ran its function in the view");
    if(lastErrorCode != ERROR_NONE) { pgTestFail("K10 a softkey press raised an error"); lastErrorCode = ERROR_NONE; }
    pgCloseView();
    popSoftmenu();
    calcMode = CM_NORMAL;

    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // 2D drawing primitive tests.

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

  static void pgTestPoints(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {   // User coordinates.
    pgTestWriteLonI(REGISTER_X, (uint32_t)x0);  // Writer takes unsigned values.
    pgTestWriteLonI(REGISTER_Y, (uint32_t)y0);
    pgTestWriteLonI(REGISTER_Z, (uint32_t)x1);
    pgTestWriteLonI(REGISTER_T, (uint32_t)y1);
  }

  static bool_t pgTestLit(int32_t x, int32_t yUser) {
    return lcd_buffer_pixel_on((uint32_t)x, (uint32_t)PG_ROW_OF(yUser));
  }

  // Verifies that closed-view drawing survives a screen refresh.
  static bool_t pgTestAnyLit(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {   // User coordinates, inclusive box.
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
    pgTestClosedView(fnGtextout, NOPARAM, 100, 131, 119, 150, "TEXTOUT");   // 20-row glyph cell.
    lastErrorCode = ERROR_NONE;
    fnPview(6);
    fnGmode(0);

    // D1: horizontal line, endpoints inclusive.
    pgTestPoints(10, 10, 50, 10);
    fnGline(NOPARAM);
    if(!pgTestLit(10, 10) || !pgTestLit(50, 10) || !pgTestLit(30, 10)) pgTestFail("D1 the horizontal line misses a pixel");
    if(pgTestLit(9, 10) || pgTestLit(51, 10) || pgTestLit(30, 11) || pgTestLit(30, 9)) pgTestFail("D1 the horizontal line has a pixel beyond an endpoint or off its row");

    // D2: vertical line, endpoints inclusive.
    pgTestPoints(70, 20, 70, 60);
    fnGline(NOPARAM);
    if(!pgTestLit(70, 20) || !pgTestLit(70, 60) || !pgTestLit(70, 40)) pgTestFail("D2 the vertical line misses a pixel");
    if(pgTestLit(70, 19) || pgTestLit(70, 61) || pgTestLit(71, 40) || pgTestLit(69, 40)) pgTestFail("D2 the vertical line has a pixel beyond an endpoint or off its column");

    // D3: diagonal line endpoints and midpoint.
    pgTestPoints(100, 100, 140, 120);
    fnGline(NOPARAM);
    if(!pgTestLit(100, 100) || !pgTestLit(140, 120) || !pgTestLit(120, 110)) pgTestFail("D3 the diagonal line misses an endpoint or its midpoint");

    // D4: box outline and filled box.
    pgTestPoints(200, 50, 260, 90);
    fnGbox(NOPARAM);
    if(!pgTestLit(200, 50) || !pgTestLit(260, 90) || !pgTestLit(230, 50) || !pgTestLit(200, 70)) pgTestFail("D4 the box outline misses a corner or an edge");
    if(pgTestLit(230, 70)) pgTestFail("D4 the box outline lit its interior");
    fnGfbox(NOPARAM);
    if(!pgTestLit(230, 70) || !pgTestLit(201, 51) || !pgTestLit(259, 89)) pgTestFail("D4 the filled box left the interior clear");

    // D5: circle outline and filled circle.
    pgTestWriteLonI(REGISTER_X, 300); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 20);
    fnGcircle(NOPARAM);
    if(!pgTestLit(320, 100) || !pgTestLit(280, 100) || !pgTestLit(300, 120) || !pgTestLit(300, 80)) pgTestFail("D5 the circle misses a cardinal point");
    if(pgTestLit(300, 100) || pgTestLit(321, 100)) pgTestFail("D5 the circle lit its center or a pixel beyond its radius");
    fnGfcircle(NOPARAM);
    if(!pgTestLit(300, 100) || !pgTestLit(310, 105)) pgTestFail("D5 the filled circle left the interior clear");

    // D6: arc from 0 to 90 degrees around complex center.
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

    // D7: clip rectangle clips line at boundary.
    pgTestPoints(0, 0, 199, 239);
    fnGclip(NOPARAM);
    pgTestPoints(100, 30, 300, 30);
    fnGline(NOPARAM);
    if(!pgTestLit(150, 30)) pgTestFail("D7 the clipped line lost a pixel inside the clip rectangle");
    if(pgTestLit(250, 30)) pgTestFail("D7 the line crossed the clip edge");
    fnErase(NOPARAM);   // Resets clip boundary to region.

    // D8: off-screen endpoints clip to screen. Values above 32767 raise error.
    pgTestWriteLonI(REGISTER_X, 0); pgTestWriteLonI(REGISTER_Y, 30); pgTestWriteLonI(REGISTER_Z, 5000); pgTestWriteLonI(REGISTER_T, 30);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D8 a coordinate of 5000 raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 30) || !pgTestLit(399, 30)) pgTestFail("D8 the line to column 5000 misses an edge pixel");
    if(pgTestLit(399, 29) || pgTestLit(390, 29) || pgTestLit(0, 31)) pgTestFail("D8 the run spilled into a neighbour row");
    pgTestWriteLonI(REGISTER_Y, 31); pgTestWriteLonI(REGISTER_Z, 40000); pgTestWriteLonI(REGISTER_T, 31);   // Unmodified row.
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("D8 a coordinate of 40000 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    if(pgTestLit(200, 31) || pgTestLit(0, 31)) pgTestFail("D8 the refused command drew");
    // D8c: negative column clamps at left edge with no memory spill.
    pgTestWriteLonISigned(REGISTER_X, -20); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 50); pgTestWriteLonI(REGISTER_T, 100);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D8c a negative column raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 100) || !pgTestLit(50, 100)) pgTestFail("D8c the clamped line misses an edge pixel");
    if(pgTestLit(399, 99) || pgTestLit(392, 99) || pgTestLit(399, 101)) pgTestFail("D8c the run spilled into the row below or above");
    if(pgRowPtr(PG_ROW_OF(99))[0] > 1) pgTestFail("D8c the row below has a corrupted dirty flag");

    // D9: two GMODE 2 XOR operations restore original buffer content.
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

    // D10: direct write matches bitblt24 output for 1000 test coordinates.
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
        y = 4 + (seed >> 20) % 210;                       // Rows 25 to 235.
        pgTestWriteLonI(REGISTER_X, x); pgTestWriteLonI(REGISTER_Y, y); pgTestWriteLonI(REGISTER_Z, x); pgTestWriteLonI(REGISTER_T, y + 1);
        fnGline(NOPARAM);                                  // Vertical line.
        pgTestWriteLonI(REGISTER_Y, y - 3); pgTestWriteLonI(REGISTER_Z, x + 2); pgTestWriteLonI(REGISTER_T, y - 3);
        fnGline(NOPARAM);                                  // Horizontal run.
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

    // D12: string coordinate raises data type error and draws nothing.
    pgTestSetString(REGISTER_X, "X");
    pgTestWriteLonI(REGISTER_Y, 200); pgTestWriteLonI(REGISTER_Z, 300); pgTestWriteLonI(REGISTER_T, 200);
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_INVALID_DATA_TYPE_FOR_OP) pgTestFail("D12 a string coordinate did not raise the data type error");
    lastErrorCode = ERROR_NONE;
    if(pgTestLit(300, 200)) pgTestFail("D12 the command drew after the error");

    // D13: clip rectangle wholly outside region results in empty clip.
    {
      static const int32_t outside[4][4] = {
        { 0, 32766, 399, 32767 },       // Above region.
        { 0, -32767, 399, -32766 },     // Below region.
        { -32767, 0, -32766, 219 },     // Left of screen.
        { 32766, 0, 32767, 219 },       // Right of screen.
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

    // D14: large radius circles clip correctly.
    pgTestWriteLonISigned(REGISTER_X, -30000); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 23170);
    fnGfcircle(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D14 a radius of 23170 raised an error"); lastErrorCode = ERROR_NONE; }
    if(pgTestLit(0, 100) || pgTestLit(200, 100) || pgTestLit(399, 100)) pgTestFail("D14 the off-screen circle of radius 23170 painted an on-screen row");
    pgTestWriteLonI(REGISTER_X, 200); pgTestWriteLonI(REGISTER_Y, 100); pgTestWriteLonI(REGISTER_Z, 32767);
    fnGfcircle(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("D14 a radius of 32767 raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(0, 0) || !pgTestLit(399, 219) || !pgTestLit(0, 219) || !pgTestLit(399, 0)) pgTestFail("D14 the circle of radius 32767 left a corner clear");
    fnErase(NOPARAM);

    // D15: DISP clears and writes only between clip columns.
    pgTestPoints(5, 190, 15, 190);   // Inside band of line 2.
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
    // D15b: DISP clears band inside clip before writing.
    pgTestPoints(100, 170, 100, 170); fnGline(NOPARAM);   // Inside band of line 3.
    pgTestPoints(100, 180, 100, 180); fnGline(NOPARAM);   // Row above band.
    pgTestSetString(REGISTER_X, "X");
    fnGdisp(3);
    if(pgTestLit(100, 170))  pgTestFail("D15b DISP did not clear its band");
    if(!pgTestLit(100, 180)) pgTestFail("D15b DISP cleared the row above its band");
    fnErase(NOPARAM);

    // D16: small angular arc at large radius preserves correct span.
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

    // D17: string cap does not cut inside a two-byte glyph.
    {
      static char longString[TMP_STR_LENGTH + 64];
      size_t i;
      for(i = 0; i < TMP_STR_LENGTH - 3; i++) longString[i] = 'A';
      longString[TMP_STR_LENGTH - 3] = (char)0x80;   // Two-byte glyph starts at cap.
      longString[TMP_STR_LENGTH - 2] = 'B';
      for(i = TMP_STR_LENGTH - 1; i < TMP_STR_LENGTH + 30; i++) longString[i] = 'C';
      longString[TMP_STR_LENGTH + 30] = 0;
      pgTestSetString(REGISTER_X, longString);
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17 the string cut refused a long string");
      if(strlen(tmpString) != TMP_STR_LENGTH - 3 || (uint8_t)tmpString[TMP_STR_LENGTH - 4] != 'A') pgTestFail("D17 the string cap cut inside a two-byte glyph");
      // D17b: lone lead byte at end is removed without overwriting buffer tail.
      tmpString[10] = 'Y'; tmpString[11] = 'Y'; tmpString[12] = 0;
      pgTestSetString(REGISTER_X, "ABCDEFGH\x80");
      if(!pgStringCut(REGISTER_X, 1)) pgTestFail("D17b the trim refused a short string");
      if(tmpString[0] != 0) pgTestFail("D17b the trim did not empty a string wider than one pixel");
      if(tmpString[10] != 'Y' || tmpString[11] != 'Y') pgTestFail("D17b the walk wrote beyond the NUL");
      // D17c: cap retains two-byte glyph when second byte has bit 7 set.
      for(i = 0; i < TMP_STR_LENGTH - 4; i++) longString[i] = 'A';
      longString[TMP_STR_LENGTH - 4] = (char)0x80; longString[TMP_STR_LENGTH - 3] = (char)0xE9;   // Glyph ends at cap.
      for(i = TMP_STR_LENGTH - 2; i < TMP_STR_LENGTH + 30; i++) longString[i] = 'B';
      longString[TMP_STR_LENGTH + 30] = 0;
      pgTestSetString(REGISTER_X, longString);
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17c the string cut refused a long string");
      if(strlen(tmpString) != TMP_STR_LENGTH - 2 || (uint8_t)tmpString[TMP_STR_LENGTH - 4] != 0x80 || (uint8_t)tmpString[TMP_STR_LENGTH - 3] != 0xE9) pgTestFail("D17c the cap split a glyph whose second byte has bit 7 set");
      // D17d: lone lead byte is removed when width fits.
      tmpString[10] = 'Y'; tmpString[11] = 'Y'; tmpString[12] = 0;
      pgTestSetString(REGISTER_X, "ABCDEFGH\x80");
      if(!pgStringCut(REGISTER_X, 0xFFFFFFFFu)) pgTestFail("D17d the cut refused a short string");
      if(strcmp(tmpString, "ABCDEFGH") != 0) pgTestFail("D17d a lone lead byte survived a cut that fits the width");
      if(tmpString[10] != 'Y' || tmpString[11] != 'Y') pgTestFail("D17d the cut wrote beyond the NUL");
    }

    // D19: near-full and near-zero angle arcs draw expected pixel ranges.
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

    // D18: NaN angle raises ERROR_OUT_OF_RANGE and draws nothing.
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

    // W1: real coordinate rounds half away from zero when no window is active.
    fnErase(NOPARAM);
    pgWindow.set = 0;
    pgTestWriteReal(REGISTER_X, "2.5"); pgTestWriteReal(REGISTER_Y, "100.49"); pgTestWriteReal(REGISTER_Z, "2.5"); pgTestWriteReal(REGISTER_T, "100.49");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W1 a real coordinate raised an error"); lastErrorCode = ERROR_NONE; }
    if(!pgTestLit(3, 100) || pgTestLit(2, 100) || pgTestLit(3, 101)) pgTestFail("W1 a real without a window is not rounded half away from zero");
    pgTestWriteReal(REGISTER_X, "-0.5"); pgTestWriteReal(REGISTER_Y, "50"); pgTestWriteReal(REGISTER_Z, "-0.5"); pgTestWriteReal(REGISTER_T, "50");
    fnGline(NOPARAM);
    if(pgTestLit(0, 50)) pgTestFail("W1 a real of -0.5 was rounded toward zero onto the screen");

    // W2: XRNG and YRNG map user coordinates to pixels; integers stay pixels.
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

    // W3: equal range limits raise error; reversed range mirrors the axis.
    fnErase(NOPARAM);
    if(pgTestLit(200, 120)) pgTestFail("W3 the probe pixel is lit before the refused XRNG");
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

    // W4: coordinate beyond 32767 pixels raises ERROR_OUT_OF_RANGE.
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 10); fnXrng(NOPARAM);
    pgTestWriteReal(REGISTER_X, "1000"); pgTestWriteReal(REGISTER_Y, "2.5"); pgTestWriteReal(REGISTER_Z, "1000"); pgTestWriteReal(REGISTER_T, "2.5");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_OUT_OF_RANGE) pgTestFail("W4 a user x of 1000 in a window of 10 did not raise ERROR_OUT_OF_RANGE");
    lastErrorCode = ERROR_NONE;
    pgTestWriteReal(REGISTER_X, "100"); pgTestWriteReal(REGISTER_Z, "100");
    fnGline(NOPARAM);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("W4 a user x of 100 raised an error"); lastErrorCode = ERROR_NONE; }
    if(pgTestLit(399, 120)) pgTestFail("W4 an off-screen real drew on the screen");

    // W5: complex coordinates define endpoints in Y and X.
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

    // W6: window settings persist across ERASE.
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


  // 3D wireframe and projection tests.

  // Loads a program from bytecode into memory.
  static void pgTestLoadProgram(const uint8_t *pgm, size_t n, const char *label) {
    FILE *f;
    size_t i;
    if(findNamedLabel(label, GLOBAL_LABELS) != INVALID_VARIABLE) return;   // Avoid duplicate global labels.
    f = fopen("c47programTest.bin", "wb");
    if(f == NULL) { pgTestFail("cannot write c47programTest.bin"); return; }
    fprintf(f, "PROGRAM_FILE_FORMAT\n0\nC47_program_file_version\n1\nPROGRAM\n%u\n", (unsigned)n);
    for(i = 0; i < n; i++) fprintf(f, "%u\n", pgm[i]);
    fclose(f);
    fnLoadProgram(NOPARAM);
    remove("c47programTest.bin");   // Remove temporary file.
    aimBuffer[0] = 0;               // Clear alpha input buffer.
  }

  static const uint8_t pgTestPgmSaddle[] = {
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'S', 'A', 'D', 'L',
    ITM_SQUARE, ITM_XexY, ITM_SQUARE, ITM_SUB,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmCnd[] = {   // erases once (flag 00 set), then records one line per sample
    ITM_LBL, STRING_LABEL_VARIABLE, 3, 'C', 'N', 'D',
    (uint8_t)((ITM_FSC >> 8) | 0x80), (uint8_t)(ITM_FSC & 0xff), 0,
    ITM_XEQ, STRING_LABEL_VARIABLE, 3, 'E', 'R', 'S',
    (uint8_t)((ITM_PT3D >> 8) | 0x80), (uint8_t)(ITM_PT3D & 0xff),
    (uint8_t)((ITM_LINE3D >> 8) | 0x80), (uint8_t)(ITM_LINE3D & 0xff),
    ITM_CLX,
    ITM_RTN,
    ITM_LBL, STRING_LABEL_VARIABLE, 3, 'E', 'R', 'S',
    (uint8_t)((ITM_ERASE >> 8) | 0x80), (uint8_t)(ITM_ERASE & 0xff),
    ITM_RTN,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmCne[] = {   // erases once (flag 00 set) and records nothing
    ITM_LBL, STRING_LABEL_VARIABLE, 3, 'C', 'N', 'E',
    (uint8_t)((ITM_FSC >> 8) | 0x80), (uint8_t)(ITM_FSC & 0xff), 0,
    ITM_XEQ, STRING_LABEL_VARIABLE, 3, 'E', 'R', 'S',
    ITM_CLX,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmHole[] = {   // square root: a hole for every negative x
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'H', 'O', 'L', 'E',
    ITM_SQUAREROOTX,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmStop[] = {   // stops at the first sample
    ITM_LBL, STRING_LABEL_VARIABLE, 3, 'S', 'T', 'P',
    ITM_STOP,
    (uint8_t)((ITM_END >> 8) | 0x80), (uint8_t)(ITM_END & 0xff),
  };
  static const uint8_t pgTestPgmErase[] = {   // Program that clears 3D retained block during run.
    ITM_LBL, STRING_LABEL_VARIABLE, 4, 'E', 'R', 'A', 'S',
    (uint8_t)((ITM_ERASE >> 8) | 0x80), (uint8_t)(ITM_ERASE & 0xff),
    (uint8_t)((ITM_PT3D >> 8) | 0x80), (uint8_t)(ITM_PT3D & 0xff),       // Sample point as current point.
    (uint8_t)((ITM_LINE3D >> 8) | 0x80), (uint8_t)(ITM_LINE3D & 0xff),   // Zero-length line.
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

  static void pgTestSet3(const char *x, const char *y, const char *z) {   // Sets x in Z, y in Y, z in X.
    pgTestWriteFloat(REGISTER_Z, x); pgTestWriteFloat(REGISTER_Y, y); pgTestWriteFloat(REGISTER_X, z);
  }

  static uint32_t pgTestLitCanvas(void) {
    uint32_t lit = 0, x, yy;
    for(x = 0; x < SCREEN_WIDTH; x++) for(yy = PG_TOP_ROW; yy < SCREEN_HEIGHT; yy++) if(lcd_buffer_pixel_on(x, yy)) lit++;
    return lit;
  }

  static uint8_t pgTestCanvasCopy[SCREEN_HEIGHT * 50];
  static void pgTestSnapCanvas(void) {   // Saves bytes 2 to 51 for rows 20 to 239.
    uint32_t r;
    for(r = PG_TOP_ROW; r < SCREEN_HEIGHT; r++) memcpy(pgTestCanvasCopy + r * 50, pgRowPtr((int32_t)r) + 2, 50);
  }
  static uint32_t pgTestCanvasDiff(void) {
    uint32_t r, d = 0, i;
    for(r = PG_TOP_ROW; r < SCREEN_HEIGHT; r++) for(i = 0; i < 50; i++) if(pgTestCanvasCopy[r * 50 + i] != pgRowPtr((int32_t)r)[2 + i]) d++;
    return d;
  }

  // Configures unit-cube view inside PVIEW 6.
  static void pgTestUnitCubeView(void) {
    if(calcMode == CM_GRAPHICS_CANVAS) pgCloseView();
    pg3dFreeBlock();
    calcMode = CM_NORMAL; lastErrorCode = ERROR_NONE;
    pgReset();
    fnPview(6); fnGmode(0);
    pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 1); fnXrng(NOPARAM); fnYrng(NOPARAM);
    fnXvol(NOPARAM); fnYvol(NOPARAM); fnZvol(NOPARAM);
    pgTestSet3("0.5", "-1", "0.5"); fnEyept(NOPARAM);
  }

  // Leaves the view the way a plot step does: two statistics points, then PLSTAT.
  static void pgTestAbandonView(void) {
    fnClSigma(0);
    pgTestWriteReal(REGISTER_Y, "1"); pgTestWriteReal(REGISTER_X, "2"); fnSigmaAddRem(1);
    pgTestWriteReal(REGISTER_Y, "3"); pgTestWriteReal(REGISTER_X, "4"); fnSigmaAddRem(1);
    fnPlotStat(PLOT_START);
    if(calcMode != CM_PLOT_STAT) pgTestFail("the plot step did not leave the view");
  }

  // Leaves the plot as EXIT does, then clears the statistics.
  static void pgTestLeavePlot(void) {
    processKeyAction(ITM_EXIT1); runFunction(ITM_EXIT1);
    fnClSigma(0);
    if(calcMode != CM_NORMAL) pgTestFail("EXIT did not leave the plot");
    lastErrorCode = ERROR_NONE;
  }

  void pgTestDraw3D(uint16_t unusedButMandatoryParameter) {
    uint32_t poolBefore;
    pgTestFailures = 0;
    pgTestLoadProgram(pgTestPgmSaddle, sizeof(pgTestPgmSaddle), "SADL");
    pgTestLoadProgram(pgTestPgmPlane, sizeof(pgTestPgmPlane), "PLNE");
    pgTestLoadProgram(pgTestPgmErase, sizeof(pgTestPgmErase), "ERAS");
    pgTestLoadProgram(pgTestPgmCnd, sizeof(pgTestPgmCnd), "CND");
    pgTestLoadProgram(pgTestPgmCne, sizeof(pgTestPgmCne), "CNE");
    pgTestLoadProgram(pgTestPgmHole, sizeof(pgTestPgmHole), "HOLE");
    pgTestLoadProgram(pgTestPgmStop, sizeof(pgTestPgmStop), "STP");
    // L1: the driver returns every pool block it takes. The statistics, the stack and the undo image are normalised at both ends.
    fnClSigma(0);
    pgTestWriteLonI(REGISTER_X, 0); pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_Z, 0); pgTestWriteLonI(REGISTER_T, 0);
    saveForUndo();
    poolBefore = c47MemInBlocks;

    // P1: eight unit-cube corners project to expected pixel coordinates.
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

    // P2: WIREFRAME of plane z = 0 with 2x2 grid lights expected pixel count.
    {
      uint32_t lit;
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("P2 WIREFRAME of the plane raised an error"); lastErrorCode = ERROR_NONE; }
      lit = pgTestLitCanvas();
      if(lit != 798) { printf("program-graphics test FAIL: P2 the plane mesh lit %u pixels, expected 798\n", lit); pgTestFailures++; }
    }

    // P3: first 3D command allocates 512 blocks; EXIT frees them.
    {
      uint32_t before;
      pgCloseView(); calcMode = CM_NORMAL;
      pgTestSet3("0", "-3", "0");   // Write registers before measuring memory.
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

    // O1: a plot step leaves the view without EXIT. The next 3D command releases the block and the region.
    {
      uint32_t mid;
      pgTestUnitCubeView();
      if(pg3d.block == NULL) pgTestFail("O1 the unit-cube view did not take the block");
      pgTestAbandonView();
      pgTestSet3("0", "-3", "0");
      mid = c47MemInBlocks;
      fnEyept(NOPARAM);
      if(pg3d.block != NULL) pgTestFail("O1 EYEPT after the abandoned view kept the block");
      if(canvas.region != 0) pgTestFail("O1 the abandoned view kept its region");
      if(c47MemInBlocks != mid - PG3D_BLOCKS) pgTestFail("O1 EYEPT after the abandoned view did not return the block");
      if(lastErrorCode != ERROR_NONE) { pgTestFail("O1 EYEPT after the abandoned view raised an error"); lastErrorCode = ERROR_NONE; }
      pgTestLeavePlot();
    }

    // B1: the state save closes the view: mode restored, block returned, region 0.
    {
      uint32_t mid;
      pgTestUnitCubeView();
      if(pg3d.block == NULL) pgTestFail("B1 the view has no block");
      mid = c47MemInBlocks;
      pgBeforeSave();
      if(calcMode != CM_NORMAL) pgTestFail("B1 pgBeforeSave did not restore the mode");
      if(pg3d.block != NULL)    pgTestFail("B1 pgBeforeSave kept the block");
      if(canvas.region != 0)    pgTestFail("B1 pgBeforeSave kept the region");
      if(c47MemInBlocks != mid - PG3D_BLOCKS) pgTestFail("B1 pgBeforeSave did not return the block");
    }

    // B2: the same after a plot step abandoned the view.
    {
      uint32_t mid;
      pgTestUnitCubeView();
      pgTestAbandonView();
      mid = c47MemInBlocks;
      pgBeforeSave();
      if(calcMode != CM_PLOT_STAT) pgTestFail("B2 pgBeforeSave changed the plot mode");
      if(pg3d.block != NULL)      pgTestFail("B2 pgBeforeSave kept the abandoned block");
      if(canvas.region != 0)      pgTestFail("B2 pgBeforeSave kept the abandoned region");
      if(c47MemInBlocks != mid - PG3D_BLOCKS) pgTestFail("B2 pgBeforeSave did not return the abandoned block");
      pgTestLeavePlot();
    }

    // B3: the real save closes the view. saveCalc() writes the test backup file and leaves the previous mode, no block, region 0.
    {
      uint8_t prev;
      pgTestUnitCubeView();
      prev = canvas.prevCalcMode;
      saveCalc();
      if(calcMode != prev)   pgTestFail("B3 saveCalc did not close the view");
      if(pg3d.block != NULL) pgTestFail("B3 saveCalc kept the block");
      if(canvas.region != 0) pgTestFail("B3 saveCalc kept the region");
      lastErrorCode = ERROR_NONE;
    }

    // P5: byte encoding and decoding.
    if(pg3dEncode(0.0f / 0.0f, 0, 1) != 255 || pg3dEncode(0, 0, 1) != 0 || pg3dEncode(1, 0, 1) != 254 || pg3dEncode(7, 0, 1) != 254 || pg3dEncode(-7, 0, 1) != 0) pgTestFail("P5 the byte encoding is wrong");
    if(pg3dDecode(254, 0, 1) != 1.0f || pg3dDecode(0, 0, 1) != 0.0f) pgTestFail("P5 the byte decoding is wrong");

    // P9 and P26: rotation and zoom keys update state in both directions.
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

    // P10: 36 UP key presses complete a 360-degree rotation.
    {
      int k; uint32_t d;
      processKeyAction(ITM_5);
      pgTestSnapCanvas();
      for(k = 0; k < 36; k++) processKeyAction(ITM_UP1);
      d = pgTestCanvasDiff();
      if(pg3d.angX != 0) pgTestFail("P10 36 presses did not close the turn");
      if(d != 0) { printf("program-graphics test FAIL: P10 %u canvas bytes differ after a full turn\n", d); pgTestFailures++; }
    }

    // P29: ERASE inside program body invalidates retained grid.
    fnErase(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe((uint16_t)findNamedLabel("ERAS", GLOBAL_LABELS));
    lastErrorCode = ERROR_NONE;
    if(pg3d.block != NULL && PG3D_HDR()->gridValid != 0) pgTestFail("P29 a body that emptied the block left a valid grid");
    pgTestUnitCubeView();
    pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));

    // P16: stack registers persist across WIREFRAME.
    pgTestWriteLonI(REGISTER_X, 1); pgTestWriteLonI(REGISTER_Y, 2); pgTestWriteLonI(REGISTER_Z, 3); pgTestWriteLonI(REGISTER_T, 4);
    fnWireframe((uint16_t)findNamedLabel("SADL", GLOBAL_LABELS));
    {
      int32_t v;
      if(!pgReadCoord(REGISTER_X, &v) || v != 1) pgTestFail("P16 X changed across WIREFRAME");
      if(!pgReadCoord(REGISTER_T, &v) || v != 4) pgTestFail("P16 T changed across WIREFRAME");
    }
    lastErrorCode = ERROR_NONE;

    // E1: a failed undo save refuses the run before a sample runs; the stack and the old grid stay.
    {
      uint32_t runsBefore, blocksBefore, depth = engineNestingDepth, plot = plotEngineActive, solver = currentSolverNestingDepth;
      uint8_t hdr[PG3D_HEADER_BYTES];
      int32_t v;
      fnErase(NOPARAM);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(PG3D_HDR()->gridValid != 1) pgTestFail("E1 the plane grid is not valid before the refused run");
      memcpy(hdr, pg3d.block, PG3D_HEADER_BYTES);
      pgTestWriteLonI(REGISTER_X, 1); pgTestWriteLonI(REGISTER_Y, 2); pgTestWriteLonI(REGISTER_Z, 3); pgTestWriteLonI(REGISTER_T, 4);
      runsBefore = pg3dRunCount; blocksBefore = c47MemInBlocks;
      pgTestFailNextUndoSave = true;
      fnWireframe((uint16_t)findNamedLabel("SADL", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_RAM_FULL) pgTestFail("E1 the refused run did not raise ERROR_RAM_FULL");
      lastErrorCode = ERROR_NONE;
      if(pg3dRunCount != runsBefore) pgTestFail("E1 a sample ran after the refused undo save");
      if(c47MemInBlocks != blocksBefore) pgTestFail("E1 the refused run kept its rows");
      if(memcmp(hdr, pg3d.block, PG3D_HEADER_BYTES) != 0) pgTestFail("E1 the refused run changed the header");
      if(engineNestingDepth != depth || plotEngineActive != plot || currentSolverNestingDepth != solver) pgTestFail("E1 the refused run changed an engine counter");
      if(getSystemFlag(FLAG_SOLVING)) pgTestFail("E1 the refused run left FLAG_SOLVING set");
      if(!pgReadCoord(REGISTER_X, &v) || v != 1) pgTestFail("E1 X changed across the refused run");
      if(!pgReadCoord(REGISTER_T, &v) || v != 4) pgTestFail("E1 T changed across the refused run");
    }

    // E2: the zoom re-run is refused the same way, and the grid stays valid.
    {
      uint32_t runsBefore = pg3dRunCount; int k; float lo, hi;
      for(k = 0; k < 8 && pg3dRunCount == runsBefore; k++) processKeyAction(ITM_ADD);
      if(pg3dRunCount == runsBefore) pgTestFail("E2 no zoom press re-ran the program (control)");
      if(pg3d.zoomStep >= 8) pgTestFail("E2 no zoom step is left for the refused press");
      lo = PG3D_HDR()->zRecLo; hi = PG3D_HDR()->zRecHi; runsBefore = pg3dRunCount;
      pgTestFailNextUndoSave = true;
      processKeyAction(ITM_ADD);
      if(lastErrorCode != ERROR_RAM_FULL) pgTestFail("E2 the refused re-run did not raise ERROR_RAM_FULL");
      lastErrorCode = ERROR_NONE;
      if(pg3dRunCount != runsBefore) pgTestFail("E2 a sample ran after the refused undo save");
      if(PG3D_HDR()->gridValid != 1) pgTestFail("E2 the refused re-run invalidated the grid");
      if(PG3D_HDR()->zRecLo != lo || PG3D_HDR()->zRecHi != hi) pgTestFail("E2 the refused re-run changed the recorded z range");
      processKeyAction(ITM_5);
      lastErrorCode = ERROR_NONE;
    }

    // H1: a body that erases at the first sample and records one line at every sample. The grid bytes stop; the records stay intact.
    {
      uint32_t k; pg3dHeader_t *hh;
      pgTestUnitCubeView();
      fnSetFlag(0);
      pgTestWriteLonI(REGISTER_X, 17); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("CND", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("H1 WIREFRAME of CND raised an error"); lastErrorCode = ERROR_NONE; }
      hh = PG3D_HDR();
      if(hh->gridValid != 0) pgTestFail("H1 the erased run left a valid grid");
      if(hh->lineCount != 289) { printf("program-graphics test FAIL: H1 lineCount is %u, expected 289\n", (unsigned)hh->lineCount); pgTestFailures++; }
      for(k = 0; k < hh->lineCount && k < 289; k++) {
        uint32_t i = k % 17, j = k / 17;
        uint8_t ex = pg3dEncode((float)i * (1.0f / 16.0f), 0.0f, 1.0f), ey = pg3dEncode((float)j * (1.0f / 16.0f), 0.0f, 1.0f);
        const uint8_t *rec = pg3d.block + PG3D_BLOCK_BYTES - PG3D_LINE_BYTES * (k + 1);
        if(rec[0] != ex || rec[1] != ey || rec[2] != ex || rec[3] != ex || rec[4] != ey || rec[5] != ex) {
          printf("program-graphics test FAIL: H1 record %u is corrupt: %u %u %u %u %u %u, expected %u %u %u\n", (unsigned)k, rec[0], rec[1], rec[2], rec[3], rec[4], rec[5], ex, ey, ex);
          pgTestFailures++;
          break;
        }
      }
      fnClearFlag(0);
    }

    // H2: a re-run whose body erases leaves an empty header: no z range is written into it.
    {
      pg3dHeader_t *hh; uint32_t runs;
      pgTestUnitCubeView();
      fnClearFlag(0);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("CNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("H2 WIREFRAME of CNE raised an error"); lastErrorCode = ERROR_NONE; }
      if(PG3D_HDR()->gridValid != 1) pgTestFail("H2 the first run of CNE left no valid grid");
      fnSetFlag(0);
      runs = pg3dRunCount;
      pg3d.zoomStep = 1;   // the re-run alone: the key's redraw would freeze the empty header again and hide the write
      pg3dZoomRerun();
      if(pg3dRunCount == runs) pgTestFail("H2 the zoom re-run did not run the program");
      hh = PG3D_HDR();
      if(hh->gridValid != 0 || hh->frozen != 0) pgTestFail("H2 the erased re-run left a valid or frozen header");
      if(hh->zRecLo != 0.0f || hh->zRecHi != 0.0f) pgTestFail("H2 the erased re-run wrote a z range into the empty header");
      fnClearFlag(0);
      pg3d.zoomStep = 0;
      lastErrorCode = ERROR_NONE;
    }

    // P19: ERASE empties retained content; redraw key draws nothing.
    fnErase(NOPARAM);
    if(PG3D_HDR()->lineCount != 0 || PG3D_HDR()->gridValid != 0) pgTestFail("P19 ERASE kept the retained content");
    processKeyAction(ITM_UP1);
    if(pgTestLitCanvas() != 0) pgTestFail("P19 a key press after ERASE drew");

    // P4: with no run of 512 free blocks left, EYEPT inside the view raises ERROR_RAM_FULL, takes no block, and leaves the eye.
    {
      static void *chunk[256]; uint32_t n = 0; float ey;
      pgTestUnitCubeView();
      pgCloseView(); calcMode = CM_NORMAL;
      fnPview(6);
      pgTestSet3("0", "-6", "0");
      ey = pg3d.eyeY;
      while(n < 256 && (chunk[n] = allocC47Blocks(PG3D_BLOCKS)) != NULL) n++;
      if(n == 256) pgTestFail("P4 the chunk array is too small to exhaust the pool");
      fnEyept(NOPARAM);
      if(lastErrorCode != ERROR_RAM_FULL) pgTestFail("P4 EYEPT with the pool exhausted did not raise ERROR_RAM_FULL");
      lastErrorCode = ERROR_NONE;
      if(pg3d.block != NULL) pgTestFail("P4 EYEPT with the pool exhausted took a block");
      if(pg3d.eyeY != ey) pgTestFail("P4 the refused EYEPT changed the eye");
      while(n > 0) { n--; freeC47Blocks(chunk[n], PG3D_BLOCKS); }
    }

    // P7 and P8: real key presses in the view on the R47 f/g layout. UP turns about x; f UP about y; g UP about z; the shift does not stick.
    {
      int up = -1, fk = -1, gk = -1, i; char key[4];
      uint8_t savedModel = calcModel;
      calcModel = USER_R47f_g;
      for(i = 0; i < 37; i++) {
        if(kbd_std[i].primary == ITM_UP1)    up = i;
        if(kbd_std[i].primary == ITM_SHIFTf) fk = i;
        if(kbd_std[i].primary == ITM_SHIFTg) gk = i;
      }
      if(up < 0 || fk < 0 || gk < 0) pgTestFail("P7 the keyboard table has no UP, f or g key");
      else {
        pgTestUnitCubeView();
        pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
        fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
        pgTestSnapCanvas();
        sprintf(key, "%02d", up); btnClicked(NULL, key);
        if(pg3d.angX != 1) pgTestFail("P7 a real UP press did not turn about x once");
        if(pgTestCanvasDiff() == 0) pgTestFail("P7 the UP press did not change the canvas");
        sprintf(key, "%02d", fk); btnClicked(NULL, key);
        sprintf(key, "%02d", up); btnClicked(NULL, key);
        if(pg3d.angY != 1 || pg3d.angX != 1) pgTestFail("P8 f UP did not turn about y once");
        if(shiftF) pgTestFail("P8 the f shift stayed engaged");
        sprintf(key, "%02d", gk); btnClicked(NULL, key);
        sprintf(key, "%02d", up); btnClicked(NULL, key);
        if(pg3d.angZ != 1) pgTestFail("P8 g UP did not turn about z once");
        if(shiftG) pgTestFail("P8 the g shift stayed engaged");
        if(lastErrorCode != ERROR_NONE) { pgTestFail("P7 a real key press raised an error"); lastErrorCode = ERROR_NONE; }
      }
      calcModel = savedModel;
    }

    // P13: two of four samples are holes. Only the column line between the valid samples is drawn, and no error is raised.
    {
      bool_t cpx = getSystemFlag(FLAG_CPXRES); uint32_t lit, ref; pg3dView_t view; pg3dSetup_t s; pgRect_t clip; int32_t c0, r0, c1, r1;
      clearSystemFlag(FLAG_CPXRES);
      pgTestUnitCubeView();
      pgTestWriteReal(REGISTER_Y, "-1"); pgTestWriteReal(REGISTER_X, "1"); fnXvol(NOPARAM);
      pgTestSet3("0", "-1", "0.5"); fnEyept(NOPARAM);
      pgTestWriteReal(REGISTER_Y, "-1"); pgTestWriteReal(REGISTER_X, "1"); fnXrng(NOPARAM);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("HOLE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("P13 two holes raised an error"); lastErrorCode = ERROR_NONE; }
      if(PG3D_GRID()[0] != PG3D_HOLE || PG3D_GRID()[2] != PG3D_HOLE || PG3D_GRID()[1] != 254 || PG3D_GRID()[3] != 254) pgTestFail("P13 the grid bytes are not hole, 254, hole, 254");
      lit = pgTestLitCanvas();
      pg3dRecordView(&view); pg3dSetup(&s, &view); pgClipNow(&clip);
      lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, SCREEN_HEIGHT - PG_TOP_ROW, LCD_SET_VALUE);
      if(!pg3dProject(&s, pg3dGridCoord(-1.0f, 1.0f, 1, 2), pg3dGridCoord(0.0f, 1.0f, 0, 2), pg3dDecode(254, 0.0f, 1.0f), &c0, &r0) ||
         !pg3dProject(&s, pg3dGridCoord(-1.0f, 1.0f, 1, 2), pg3dGridCoord(0.0f, 1.0f, 1, 2), pg3dDecode(254, 0.0f, 1.0f), &c1, &r1)) pgTestFail("P13 a valid sample does not project");
      pgLine(&clip, c0, r0, c1, r1);
      ref = pgTestLitCanvas();
      if(lit != ref || lit == 0) { printf("program-graphics test FAIL: P13 the holed mesh lit %u pixels, the column line alone %u\n", lit, ref); pgTestFailures++; }
      if(cpx) setSystemFlag(FLAG_CPXRES);
    }

    // P14: STOP in the body aborts the run: ERROR_SOLVER_ABORT, no valid grid, the program is not running.
    {
      uint8_t savedRun = programRunStop;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("STP", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_SOLVER_ABORT) { printf("program-graphics test FAIL: P14 STOP in the body gave error %u, expected %u\n", (unsigned)lastErrorCode, (unsigned)ERROR_SOLVER_ABORT); pgTestFailures++; }
      lastErrorCode = ERROR_NONE;
      if(PG3D_HDR()->gridValid != 0) pgTestFail("P14 the aborted run left a valid grid");
      if(programRunStop == PGM_RUNNING) pgTestFail("P14 the program is still running after the abort");
      programRunStop = savedRun;
    }

    // P15: a nested WIREFRAME is refused: the program waits, the refusal is recorded, nothing is drawn, no block moves.
    {
      uint8_t savedRun = programRunStop; uint32_t before;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      before = c47MemInBlocks;
      engineNestingDepth = 1;
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      engineNestingDepth = 0;
      if(programRunStop != PGM_WAITING) pgTestFail("P15 the refused nested run did not stop the program");
      if(!engineNestingWasRefused) pgTestFail("P15 the refusal was not recorded");
      if(pgTestLitCanvas() != 0) pgTestFail("P15 the refused nested run drew");
      if(c47MemInBlocks != before) pgTestFail("P15 the refused nested run moved pool blocks");
      engineNestingWasRefused = false; programRunStop = savedRun; lastErrorCode = ERROR_NONE;
    }

    // P17: the program pointers are the same before and after WIREFRAME.
    {
      uint16_t pn = currentProgramNumber, ls = currentLocalStepNumber; uint8_t *st = currentStep; uint8_t savedRun = programRunStop;
      programRunStop = PGM_STOPPED;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(currentProgramNumber != pn || currentLocalStepNumber != ls || currentStep != st) pgTestFail("P17 WIREFRAME moved the program pointers");
      programRunStop = savedRun;
    }

    // P21: in the view the shift glyph stays in the status bar: Y_SHIFT 0 and X_SHIFT at the right. In normal mode Y_SHIFT is 24.
    {
      bool_t d = getSystemFlag(FLAG_SBdate), t = getSystemFlag(FLAG_SBtime), r = getSystemFlag(FLAG_SBshfR);
      setSystemFlag(FLAG_SBdate); setSystemFlag(FLAG_SBtime); clearSystemFlag(FLAG_SBshfR);
      pgTestUnitCubeView();
      if(Y_SHIFT != 0) pgTestFail("P21 Y_SHIFT is not 0 in the view");
      if(X_SHIFT != X_SHIFT_R) pgTestFail("P21 X_SHIFT is not X_SHIFT_R in the view");
      pgCloseView(); calcMode = CM_NORMAL;
      if(Y_SHIFT != 24) pgTestFail("P21 Y_SHIFT is not 24 in normal mode");
      if(d) setSystemFlag(FLAG_SBdate); else clearSystemFlag(FLAG_SBdate);
      if(t) setSystemFlag(FLAG_SBtime); else clearSystemFlag(FLAG_SBtime);
      if(r) setSystemFlag(FLAG_SBshfR); else clearSystemFlag(FLAG_SBshfR);
    }

    // P22: the eye is frozen with the picture. EYEPT after the first record changes no redraw until the next clear.
    {
      uint32_t d;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      processKeyAction(ITM_UP1);
      pgTestSnapCanvas();
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      pgTestSet3("0.5", "-6", "0.5"); fnEyept(NOPARAM);
      processKeyAction(ITM_UP1);
      d = pgTestCanvasDiff();
      if(d != 0) { printf("program-graphics test FAIL: P22 EYEPT after the record changed the redraw in %u bytes\n", d); pgTestFailures++; }
      if(lastErrorCode != ERROR_NONE) { pgTestFail("P22 EYEPT or the key raised an error"); lastErrorCode = ERROR_NONE; }
    }

    // P25: every sample errors: the error is shown, nothing is lit, no grid.
    {
      bool_t cpx = getSystemFlag(FLAG_CPXRES);
      clearSystemFlag(FLAG_CPXRES);
      pgTestUnitCubeView();
      pgTestWriteReal(REGISTER_Y, "-2"); pgTestWriteReal(REGISTER_X, "-1"); fnXvol(NOPARAM);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("HOLE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) { printf("program-graphics test FAIL: P25 all holes gave error %u, expected %u\n", (unsigned)lastErrorCode, (unsigned)ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); pgTestFailures++; }
      lastErrorCode = ERROR_NONE;
      if(pgTestLitCanvas() != 0) pgTestFail("P25 the all-holes run drew");
      if(PG3D_HDR()->gridValid != 0) pgTestFail("P25 the all-holes run left a valid grid");
      if(cpx) setSystemFlag(FLAG_CPXRES);
    }

    // P27: WIREFRAME in GMODE 2 twice over the same mesh restores the canvas.
    {
      pgTestUnitCubeView();
      fnGmode(2);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(pgTestLitCanvas() == 0) pgTestFail("P27 the first inverted mesh lit nothing");
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(pgTestLitCanvas() != 0) pgTestFail("P27 the second inverted mesh did not restore the canvas");
      fnGmode(0);
      lastErrorCode = ERROR_NONE;
    }

    // T1 to T3: ERASE, PVIEW and EXIT return the turn and the zoom to the home view. The next drawing equals a fresh home drawing.
    {
      uint32_t d; int way;
      for(way = 1; way <= 3; way++) {
        pgTestUnitCubeView();
        pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
        fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
        pgTestSnapCanvas();
        processKeyAction(ITM_UP1); processKeyAction(ITM_ADD);
        if(pg3d.angX != 1 || pg3d.zoomStep != 1) pgTestFail("T the keys did not turn and zoom");
        if(way == 1) fnErase(NOPARAM);
        else if(way == 2) fnPview(6);
        else { pgCloseView(); fnPview(6); }
        if(pg3d.angX != 0 || pg3d.zoomStep != 0) { printf("program-graphics test FAIL: T%d the clear did not return to the home view\n", way); pgTestFailures++; }
        pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
        fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
        d = pgTestCanvasDiff();
        if(d != 0) { printf("program-graphics test FAIL: T%d the drawing after the clear differs from the home drawing in %u bytes\n", way, d); pgTestFailures++; }
        if(lastErrorCode != ERROR_NONE) { pgTestFail("T a clear or a drawing raised an error"); lastErrorCode = ERROR_NONE; }
      }
    }

    // T4: a PT3D given outside the view does not survive into the view: the first LINE3D there sets the point and draws nothing.
    {
      pgTestUnitCubeView();
      pgCloseView(); calcMode = CM_NORMAL;
      pgTestSet3("0.2", "0.2", "0.2"); fnPt3d(NOPARAM);
      fnPview(6);
      pgTestSet3("0.8", "0.8", "0.8"); fnLine3d(NOPARAM);
      if(pgTestLitCanvas() != 0) pgTestFail("T4 LINE3D drew from a point set outside the view");
      if(!pg3d.haveCur) pgTestFail("T4 LINE3D did not set the current point");
      lastErrorCode = ERROR_NONE;
    }

    // P6: a mesh above the cap draws once and does not turn. A key press leaves the canvas as it is.
    {
      uint32_t d;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 45); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("P6 the over-cap WIREFRAME raised an error"); lastErrorCode = ERROR_NONE; }
      if(pgTestLitCanvas() == 0) pgTestFail("P6 the over-cap mesh did not draw");
      if(PG3D_HDR()->gridValid != 0 || PG3D_HDR()->numX != 0) pgTestFail("P6 the over-cap mesh was retained");
      pgTestSnapCanvas();
      processKeyAction(ITM_UP1);
      d = pgTestCanvasDiff();
      if(d != 0) { printf("program-graphics test FAIL: P6 the key press changed %u canvas bytes of an over-cap mesh\n", d); pgTestFailures++; }
      if(pg3d.angX != 0) pgTestFail("P6 the key press turned the view with nothing retained");
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
    }

    // P20: invalid counts and ranges raise errors.
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

    // W7: a window that does not fit in floats is refused by WIREFRAME, and nothing is drawn.
    {
      uint32_t runs; uint8_t hdr[PG3D_HEADER_BYTES];
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      memcpy(hdr, pg3d.block, PG3D_HEADER_BYTES);
      fnErase(NOPARAM);
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "1e39"); fnXrng(NOPARAM);
      if(lastErrorCode != ERROR_NONE) { pgTestFail("W7 XRNG 0 1e39 was refused by the 2D window"); lastErrorCode = ERROR_NONE; }
      runs = pg3dRunCount;
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("W7 WIREFRAME under XRNG 0 1e39 was not refused");
      lastErrorCode = ERROR_NONE;
      if(pgTestLitCanvas() != 0) pgTestFail("W7 the refused WIREFRAME drew");
      if(pg3dRunCount != runs) pgTestFail("W7 the refused WIREFRAME ran a sample");
      (void)hdr;
    }

    // W8: a mirrored window is legal in 3D: the plane of P2 lights the same 798 pixels.
    {
      uint32_t lit;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_Y, 1); pgTestWriteLonI(REGISTER_X, 0); fnXrng(NOPARAM);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("W8 WIREFRAME under a mirrored XRNG raised an error"); lastErrorCode = ERROR_NONE; }
      lit = pgTestLitCanvas();
      if(lit != 798) { printf("program-graphics test FAIL: W8 the mirrored plane lit %u pixels, expected 798\n", lit); pgTestFailures++; }
    }

    // W9: a key press under an unusable window is refused before the canvas is cleared.
    {
      uint32_t d;
      pgTestUnitCubeView();
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "1e39"); fnXrng(NOPARAM);
      lastErrorCode = ERROR_NONE;
      pgTestSnapCanvas();
      processKeyAction(ITM_UP1);
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("W9 UP under XRNG 0 1e39 was not refused");
      lastErrorCode = ERROR_NONE;
      { uint32_t r; d = 0; for(r = 40; r < SCREEN_HEIGHT; r++) if(memcmp(pgTestCanvasCopy + r * 50, pgRowPtr((int32_t)r) + 2, 50) != 0) d++; }   // rows 20 to 39 hold the error text
      if(d != 0) { printf("program-graphics test FAIL: W9 the refused key press changed %u canvas rows below the error band\n", d); pgTestFailures++; }
      if(pg3d.angX != 0) pgTestFail("W9 the refused key press turned the view");
      pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 1); fnXrng(NOPARAM);
    }

    // P23: LINE3D without current point sets position and draws nothing.
    fnErase(NOPARAM);
    pg3d.haveCur = 0;
    pgTestSet3("0.2", "0.2", "0.2"); fnLine3d(NOPARAM);
    if(pgTestLitCanvas() != 0) pgTestFail("P23 LINE3D without a current point drew");
    if(!pg3d.haveCur) pgTestFail("P23 LINE3D without a current point did not set it");

    // P12: retained buffer holds 330 lines; subsequent lines draw without caching.
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
      lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, SCREEN_HEIGHT - PG_TOP_ROW, LCD_SET_VALUE);   // Clear canvas.
      litBefore = pgTestLitCanvas();
      pgTestSet3("0.5", "0.5", "0.5"); fnLine3d(NOPARAM);
      if(pgTestLitCanvas() <= litBefore) pgTestFail("P12 the 331st line did not draw");
      if(PG3D_HDR()->lineCount != 330) pgTestFail("P12 the 331st line was retained past the block");
      if(memcmp(header, pg3d.block, PG3D_HEADER_BYTES) != 0) pgTestFail("P12 the 331st line changed the header");
    }

    // P31: point at 1/1024 depth in front of eye projects successfully.
    {
      pg3dView_t view; pg3dSetup_t s; int32_t col, row;
      pg3dFreeBlock(); pgReset();
      pg3d.ylo = 0.0f; pg3d.yhi = 1024.0f; pg3d.eyeY = -1.0f;
      if(!pg3dRecordView(&view)) pgTestFail("P31 the view is not valid");
      pg3dSetup(&s, &view);
      if(!pg3dProject(&s, 0.0f, 0.0f, 0.0f, &col, &row)) pgTestFail("P31 a point at exactly eps was rejected");
      if(pg3dProject(&s, 0.0f, -0.5f, 0.0f, &col, &row)) pgTestFail("P31 a point nearer than eps was drawn");
      pgReset();
    }

    // P32: coordinate clamp holds at display edge.
    {
      pg3dView_t view; pg3dSetup_t s; int32_t col, row;
      pg3dFreeBlock(); pgReset();
      pg3d.xlo = -2.0f; pg3d.xhi = 2.0f; pg3d.ylo = -1.0f; pg3d.yhi = 1.0f; pg3d.zlo = -2.0f; pg3d.zhi = 2.0f;
      pg3d.eyeX = 0.0f; pg3d.eyeY = -3.0f; pg3d.eyeZ = 0.0f;
      pgTestWriteReal(REGISTER_Y, "0");           pgTestWriteReal(REGISTER_X, "0.009975");  fnXrng(NOPARAM);   // Ratio 399 / 40000.
      pgTestWriteReal(REGISTER_Y, "-0.005939511"); pgTestWriteReal(REGISTER_X, "0");         fnYrng(NOPARAM);   // Ratio -239 / 40239.
      if(!pg3dRecordView(&view)) pgTestFail("P32 the view is not valid");
      pg3dSetup(&s, &view);
      if(!pg3dProject(&s, 2.0f, -1.0f, -2.0f, &col, &row)) pgTestFail("P32 the far corner was rejected");
      if(col != 32000 || row != 32000) { printf("program-graphics test FAIL: P32 the far corner projects to (%d, %d), expected (32000, 32000)\n", (int)col, (int)row); pgTestFailures++; }
      pgWindow.set = 0;
      pgReset();
    }

    // P20b: volume span that overflows float raises error.
    pgTestWriteReal(REGISTER_Y, "-2e38"); pgTestWriteReal(REGISTER_X, "2e38"); fnXvol(NOPARAM);
    if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("P20b XVOL -2e38 2e38 was accepted");
    lastErrorCode = ERROR_NONE;

    // V1: a volume span whose byte scale overflows float is refused; one just inside is accepted.
    {
      float xlo = pg3d.xlo, xhi = pg3d.xhi;
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "7.4e-37"); fnXvol(NOPARAM);
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("V1 XVOL 0 7.4e-37 was accepted");
      lastErrorCode = ERROR_NONE;
      if(pg3d.xlo != xlo || pg3d.xhi != xhi) pgTestFail("V1 the refused XVOL changed the volume");
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "7.6e-37"); fnXvol(NOPARAM);
      if(lastErrorCode != ERROR_NONE) { pgTestFail("V1 XVOL 0 7.6e-37 was refused"); lastErrorCode = ERROR_NONE; }
      if(pg3d.xhi != 7.6e-37f) pgTestFail("V1 XVOL 0 7.6e-37 did not set the volume");
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "7.4e-37"); fnYvol(NOPARAM);
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("V1 YVOL 0 7.4e-37 was accepted");
      lastErrorCode = ERROR_NONE;
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "7.4e-37"); fnZvol(NOPARAM);
      if(lastErrorCode != ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN) pgTestFail("V1 ZVOL 0 7.4e-37 was accepted");
      lastErrorCode = ERROR_NONE;
      pg3d.xlo = xlo; pg3d.xhi = xhi;
    }

    // Z1: the zoom re-run is skipped when the visible z slice is too thin for the byte scale. Eye at z 0, YRNG 0 2e-36:
    // presses 1 to 4 re-run, press 5 is refused (computed in float32; the plan records the arithmetic).
    {
      uint32_t runs; int k;
      pgTestUnitCubeView();
      pgTestSet3("0.5", "-1", "0"); fnEyept(NOPARAM);
      pgTestWriteReal(REGISTER_Y, "-1"); pgTestWriteReal(REGISTER_X, "1"); fnZvol(NOPARAM);
      pgTestWriteReal(REGISTER_Y, "0"); pgTestWriteReal(REGISTER_X, "2e-36"); fnYrng(NOPARAM);
      pgTestWriteLonI(REGISTER_X, 2); fnNumx(NOPARAM); fnNumy(NOPARAM);
      fnWireframe((uint16_t)findNamedLabel("PLNE", GLOBAL_LABELS));
      if(lastErrorCode != ERROR_NONE) { pgTestFail("Z1 WIREFRAME under the thin window raised an error"); lastErrorCode = ERROR_NONE; }
      if(PG3D_HDR()->gridValid != 1) pgTestFail("Z1 no valid grid before the zoom presses");
      for(k = 1; k <= 3; k++) processKeyAction(ITM_ADD);
      runs = pg3dRunCount;
      processKeyAction(ITM_ADD);
      if(pg3dRunCount == runs) pgTestFail("Z1 press 4 did not re-run the program (control)");
      runs = pg3dRunCount;
      processKeyAction(ITM_ADD);
      if(pg3dRunCount != runs) pgTestFail("Z1 press 5 re-ran the program over a slice too thin for the byte scale");
      if(lastErrorCode != ERROR_NONE) { pgTestFail("Z1 a zoom press raised an error"); lastErrorCode = ERROR_NONE; }
      pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_X, 1); fnYrng(NOPARAM);
      processKeyAction(ITM_5);
    }

    // P18: reset restores defaults and clears block pointer.
    {
      uint32_t before = c47MemInBlocks;
      uint8_t *held = pg3d.block;
      if(held == NULL) pgTestFail("P18 no block is live before the reset");
      pgReset();
      if(pg3d.block != NULL) pgTestFail("P18 reset kept the block pointer");
      if(c47MemInBlocks != before) pgTestFail("P18 reset freed the block");
      if(pg3d.eyeY != -3.0f || pg3d.xlo != -1.0f || pg3d.numX != 10 || pg3d.numY != 8) pgTestFail("P18 reset did not restore the HP defaults");
      freeC47Blocks(held, PG3D_BLOCKS);   // Free test allocation.
    }

    pgCloseView();
    calcMode = CM_NORMAL;
    lastErrorCode = ERROR_NONE;
    fnClSigma(0);
    pgTestWriteLonI(REGISTER_X, 0); pgTestWriteLonI(REGISTER_Y, 0); pgTestWriteLonI(REGISTER_Z, 0); pgTestWriteLonI(REGISTER_T, 0);
    saveForUndo();
    if(c47MemInBlocks != poolBefore) { printf("program-graphics test FAIL: L1 the driver leaks %d pool blocks\n", (int)c47MemInBlocks - (int)poolBefore); pgTestFailures++; }
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // 3D showcase and animation frames.
  #define PG_S3_COUNT  8656u   // lit pixels of the still picture, recorded at the first green run
  #define PG_S3H_COUNT 7325u   // lit pixels of the home redraw (frame 001), recorded at the first green run of the wave
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
    // R0: the still picture equals the first redraw at the home view (UP, then DOWN).
    pgTestSnapCanvas();
    processKeyAction(ITM_UP1); processKeyAction(ITM_DOWN1);
    { uint32_t d = pgTestCanvasDiff(); if(d != 0) { printf("program-graphics test FAIL: R0 the home redraw differs from the still picture in %u bytes\n", d); pgTestFailures++; } }
    pgTestSetString(REGISTER_X, "program-graphics G4: EYEPT XVOL YVOL ZVOL NUMX NUMY WIREFRAME PT3D LINE3D");
    fnGdisp(1);
    lit = pgTestLitCanvas();
    printf("program-graphics showcase 3D: %u lit pixels in rows 20 to 239\n", lit);
    if(lit != PG_S3_COUNT) { printf("program-graphics test FAIL: S3 the showcase count is %u, recorded %u\n", lit, (unsigned)PG_S3_COUNT); pgTestFailures++; }
    strcpy(_ioFileNameOverride, "pg3d_000.bmp"); fnScreenDump(0);
    processKeyAction(ITM_5);   // Reset to home view.
    pg3dRedraw();
    strcpy(_ioFileNameOverride, "pg3d_001.bmp"); fnScreenDump(0);
    lit = pgTestLitCanvas();
    printf("program-graphics showcase 3D: home redraw %u lit pixels\n", lit);
    if(lit != PG_S3H_COUNT) { printf("program-graphics test FAIL: S3h the home redraw count is %u, recorded %u\n", lit, (unsigned)PG_S3H_COUNT); pgTestFailures++; }
    pgTestSnapCanvas();
    k = 2;
    #define PG_FRAME(item) do { processKeyAction(item); sprintf(name, "pg3d_%03u.bmp", (unsigned)k++); strcpy(_ioFileNameOverride, name); fnScreenDump(0); } while(0)
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_UP1); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1 a full turn about x did not return the canvas");
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_BST); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1y a full turn about y did not return the canvas");
    { int i; for(i = 0; i < 36; i++) PG_FRAME(ITM_RBR); }
    if(pgTestCanvasDiff() != 0) pgTestFail("R1z a full turn about z did not return the canvas");
    // P11: five of the six zoom-in presses re-run the 24 by 24 program; every zoom-out press widens the slice and re-runs.
    { int i; uint32_t r0 = pg3dRunCount;
      for(i = 0; i < 6; i++) PG_FRAME(ITM_ADD);
      if(pg3dRunCount - r0 != 5u * 576u) { printf("program-graphics test FAIL: P11 six zoom-in presses ran the program %u times, expected 2880\n", (unsigned)(pg3dRunCount - r0)); pgTestFailures++; }
      r0 = pg3dRunCount;
      for(i = 0; i < 6; i++) PG_FRAME(ITM_SUB);
      if(pg3dRunCount - r0 != 6u * 576u) { printf("program-graphics test FAIL: P11 six zoom-out presses ran the program %u times, expected 3456\n", (unsigned)(pg3dRunCount - r0)); pgTestFailures++; }
    }
    if(pgTestCanvasDiff() != 0) pgTestFail("R2 six zoom steps in and out did not return the canvas");
    #undef PG_FRAME
    printf("program-graphics showcase 3D: %u frames, %u program runs\n", (unsigned)k, (unsigned)pg3dRunCount);
    if(lastErrorCode != ERROR_NONE) { pgTestFail("S3 an error was raised during the animation"); lastErrorCode = ERROR_NONE; }
    pgCloseView();
    calcMode = CM_NORMAL;
    pgTestWriteLonI(REGISTER_X, pgTestFailures);
  }

  // 2D showcase with all 2D commands. Writes pg_showcase_2d.bmp.
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
    pgTestPoints(10, 10, 390, 10);   fnGline(NOPARAM);           // Baseline.
    pgTestPoints(10, 10, 10, 195);   fnGline(NOPARAM);           // Left axis.
    pgTestPoints(10, 10, 120, 190);  fnGline(NOPARAM);           // Diagonal.
    pgTestPoints(40, 40, 110, 90);   fnGbox(NOPARAM);            // Outline box.
    pgTestPoints(130, 40, 200, 90);  fnGfbox(NOPARAM);           // Filled box.
    pgTestWriteLonI(REGISTER_X, 250); pgTestWriteLonI(REGISTER_Y, 65); pgTestWriteLonI(REGISTER_Z, 25); fnGcircle(NOPARAM);
    pgTestWriteLonI(REGISTER_X, 320); pgTestWriteLonI(REGISTER_Y, 65); pgTestWriteLonI(REGISTER_Z, 25); fnGfcircle(NOPARAM);
    pgTestSetComplex(REGISTER_T, 250, 150); pgTestWriteLonI(REGISTER_Z, 40); pgTestWriteLonI(REGISTER_Y, 30); pgTestWriteLonI(REGISTER_X, 300); fnGarc(NOPARAM);
    pgTestSetString(REGISTER_Z, "TEXTOUT at 150,130");
    pgTestWriteLonI(REGISTER_X, 150); pgTestWriteLonI(REGISTER_Y, 130); fnGtextout(NOPARAM);
    pgTestPoints(300, 110, 390, 180); fnGclip(NOPARAM);          // Clip and disc at corner.
    pgTestWriteLonI(REGISTER_X, 390); pgTestWriteLonI(REGISTER_Y, 180); pgTestWriteLonI(REGISTER_Z, 45); fnGfcircle(NOPARAM);
    pgTestPoints(300, 110, 390, 180); fnGbox(NOPARAM);           // Clip rectangle outline.
    pgTestPoints(0, 0, 399, 239); fnGclip(NOPARAM);
    fnGmode(2);
    pgTestPoints(150, 55, 180, 75); fnGfbox(NOPARAM);            // Invert window in filled box.
    fnGmode(0);
    {
      // Draws sine curve through window across 26 segments.
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

    // Region 2 with CANVAS softmenu visible below drawing.
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
