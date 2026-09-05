Begin your reply with the line `MODEL: <your exact model name>` before
anything else.

## Subject

A personal hobby project: a drawing package (program-graphics) built as an
external package over the open-source C47/R47 firmware for a DM42-class
pocket calculator. A user program draws on the screen. Stage G4 adds 3D:
a view volume seen from an eye point as on the HP 48, a WIREFRAME mesh of
z = f(x, y) from a user program, PT3D and LINE3D, and keys that turn the
retained drawing. Single-user handheld. No network stack, no untrusted
input, no privilege boundary. The worst outcome of any bug here is that
the calculator reboots and the owner loses the program they were typing.

Audit for FUNCTIONAL correctness: wrong answers, lost work, stuck states,
crashes. A finding whose impact statement needs an attacker is not a
finding. Report findings, not fixes.

## Orientation

- The screen is 400 by 240, one bit per pixel. `pgLine(clip, x0, row0, x1, row1)` draws a clipped line in screen coordinates (row 0 at the top) with the draw mode in force; `lcd_fill_rect(x, row, w, h, LCD_SET_VALUE)` clears a rectangle to white; `pgRefreshNow` and `pgRefreshMaybe` send the buffer to the LCD; `pgClipNow(&c)` gives the clip rectangle in force. None of these is the subject.
- `canvas` is the view state: `canvas.region` is 0 when the view is closed, else 2 (rows 20 to 170) or 6 (rows 20 to 239); `canvas.drawMode` is 0 set, 1 clear, 2 invert; `PG_TOP_ROW` is 20, `PG_REGISTER_BOTTOM_ROW` 170, `PG_REGION_REGISTERS` 2. `pgWindow` is the G3 window: `set` bit 0 means XRNG was set (`xmin`, `xmax` real34), bit 1 means YRNG (`ymin`, `ymax`). `pgSetRegion` runs for `ERASE` and `PVIEW`; `pgCloseView` runs at EXIT from the view.
- Memory: `allocC47Blocks(n)` returns n blocks of 4 bytes from the calculator's pool or NULL; `freeC47Blocks(p, n)` returns them and ignores NULL; `TO_BLOCKS(bytes)` rounds up. `c47MemInBlocks` counts blocks in use. A RESET zeroes the whole pool and rebuilds it as one region; a pointer into the pool is dead after that, and a free of it inserts a bogus region. `pgReset` is called from the package's one-line hook in upstream `config.c`, right after `histElementXorY = -1;` in `doFnReset`, which also runs at boot.
- The program runner: `execProgram(label)` (upstream lblGtoXeq.c) runs the labelled program synchronously and returns; inside a running program the body runs only when `FLAG_SOLVING` (or `FLAG_INTING`, or the grapher status) is set, else the call only pushes a subroutine level and returns at once. From the keyboard state `runProgram` sets `programRunStop` to running and back to stopped, and leaves the program pointer at the end of the label's program; `execProgram` restores `currentProgramNumber`, `currentLocalStepNumber`, `currentStep` only in the in-program arm. `FLAG_SOLVING` also stops the per-item undo snapshot and the screen writes of the runner. `engineNestingRefused(true)` is the plot engine's guard: it refuses inside any engine (`engineNestingDepth != 0`), sets `programRunStop = PGM_WAITING` and `engineNestingWasRefused`, and returns true. `saveForUndo()` copies the stack, LASTX, the system flags and the stats into the undo image and sets `thereIsSomethingToUndo`; `fnUndo(0)` restores them when there is something to undo and clears that flag. `exitKeyWaiting()` polls the EXIT key. `dynamicMenuItem = -1` is what the sum engine sets before a run. The sum engine's protocol is: `currentKeyCode = 255; ++currentSolverNestingDepth; setSystemFlag(FLAG_SOLVING)` before, the reverse after; the plot engine adds `++engineNestingDepth; ++plotEngineActive`.
- Registers: `REGISTER_X..T` are the stack; `convertDoubleToReal34Register(d, r)` stores a real; `fnToReal(NOPARAM)` converts X to a real (error for a type it cannot convert); `REGISTER_REAL34_DATA(r)`; `real34ToReal`, `realToFloat`, `convertLongIntegerRegisterToReal`, `realToInt32C47(r, &err)` (truncates toward zero, err beyond int32, NaN, infinity), `realIsAnInteger`. `letteredRegisterName(r)` gives the letter of a stack register; `findNamedLabel(name, GLOBAL_LABELS)` gives a label code or `INVALID_VARIABLE`; a label code lies in `FIRST_LABEL..LAST_LABEL`. Errors: `pgError(code)` and `displayCalcErrorMessage(code, ERR_REGISTER_LINE, reg)` set `lastErrorCode` and stop a running program at that step. `NIM_REGISTER_LINE` is the line upstream uses for `ERROR_RAM_FULL`.
- Keys: `pg3dKey(item)` is called from the package's `fnKeyUp` and `fnKeyDown` cases for `ITM_UP1` and `ITM_DOWN1`, and from the guard arm of `processKeyAction` for `ITM_BST` (f-UP), `ITM_SST` (f-DOWN), `ITM_RBR` (g-UP), `ITM_FLGSV` (g-DOWN), `ITM_ADD`, `ITM_SUB`, `ITM_4`, `ITM_5`, `ITM_6`. On the PC build a key reaches the package only while no program runs.
- The contract, DESIGN.md §9 (2026-09-05): the projection plane is y = eye y plus one unit and moves with the eye; a point projects along the ray from the eye; the three rotations are integer step counts of 10 degrees about the volume center in the fixed order Rz Ry Rx; one zoom step multiplies the magnification of the near face by 1.25 by moving the eye along y; the plane coordinates go through the G3 window; a point at or behind the eye, or nearer than one 1024th of the volume depth, is not drawable and every line that touches it is skipped; rounding is half up; a result is clamped to plus or minus 32000 before the kernel clips. The retained block is 2048 bytes: a 64-byte header, grid bytes up from the header (row major), line records of six bytes down from the end; one byte per value spans a range in 254 steps, 255 marks a hole (NaN or infinite); a finite value outside the range clamps to 0 or 254; the block is taken by the first 3D command inside the view, emptied by ERASE and PVIEW, freed at EXIT, forgotten without a free at a reset; a grid that does not fit draws once and is not retained; a line past the free bytes draws once and is not retained. The volume and the eye freeze at the first record; XVOL, YVOL, ZVOL, EYEPT after that take effect after ERASE, PVIEW or EXIT; the window is not frozen. WIREFRAME runs the label with X = x, Y = y, Z = x, T = y and reads z from X; a failed sample is a hole; when every sample fails the last error shows; an abort keeps the partial drawing and retains no grid; the stack comes back as it was; the program pointers come back on both arms. A zoom press re-runs the program once when one recorded z step is wider than a pixel at the near face, or when the visible z range reaches outside the recorded range; a failed re-run drops the grid and keeps the lines. A key with nothing retained changes nothing. The redraw clears the region and draws in mode 0 with the clip in force.
- Pins that passed, do not report what they prove: P1 the eight unit-cube corners project to (0,239) (399,239) (0,0) (399,0) (100,179) (299,179) (100,60) (299,60); P2 a 2 by 2 plane lights 798 pixels; P3 the block is 512 blocks taken by the first 3D command in the view, returned at EXIT, never taken outside the view; P5 the encoding at 0, 254, NaN and the clamps; P9 and P26 every key moves its counter both ways, 5 resets, 4 does nothing; P10 36 UP presses return the canvas byte for byte, and so do 36 f-UP and 36 g-UP presses and six zoom steps in and out; P12 330 lines fill the block and the 331st draws unretained with the header unchanged; P16 the stack survives WIREFRAME; P18 the reset hook forgets the block and restores the HP defaults; P19 ERASE empties the content; P20 NUMX 1, 101, 2.5 and a string are refused, XVOL with equal ends is refused; P23 LINE3D without a current point sets it and draws nothing.
- Pre-verified facts: with the package's test drivers placed before the suite's equation coverage files, the first formula integration later fails with a syntax error in the upstream parser's word reader (`_parseWord` under `parseEquation` under the integrator, a word longer than seven glyphs in a formula that reads "X"); it needs both 3D drivers in one run, goes away when the engine's `saveForUndo`/`fnUndo` pair is removed, and thirty-four probed globals are equal across the drivers. The drivers now run last in the suite. Not settled. If you can name a mechanism in the code below that leaves a pool block, a saved register, or the undo image in a state the equation parser can trip on, that is the finding of the round.
- Documented limits, do not report: a rotation clears 2D content on the canvas; the block holds 44 by 44 at most; the block plus the undo-history ring exceeds the RCL58 slack of the pool; the zoom test ignores the rotation; 3D commands outside the view draw once and retain nothing; the DM42 key repeat is untested; every error names register X.

## The code

```c
// the 3D state, the block header, and the constants (the subject)
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
```

```c
// the reset hook, the free-bytes rule, the byte encoding (the subject)
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
```

```c
// the block: ensure, empty, free, and the frozen view (the subject)
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

static void pg3dEmpty(void);
static void pg3dFreeBlock(void);

// The window of §5.2. Zero at boot: no range set, a real is a pixel. Kept
// apart from pgCanvas_t because the header is read before realType.h.
static struct {
  uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
  real34_t xmin, xmax, ymin, ymax;
} pgWindow;

static void pg3dFreeBlock(void);

// The window of §5.2. Zero at boot: no range set, a real is a pixel. Kept
// apart from pgCanvas_t because the header is read before realType.h.
static struct {
  uint8_t  set;            // bit 0: XRNG was set, bit 1: YRNG was set
  real34_t xmin, xmax, ymin, ymax;
} pgWindow;

static bool_t pg3dViewValid(const pg3dView_t *v) {
  return v->xlo < v->xhi && v->ylo < v->yhi && v->zlo < v->zhi && v->eyeY < v->ylo;
}

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
```

```c
// WIREFRAME: the sample, the grid loop, the engine protocol, the command (the subject)
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
  if(!(dy > s->eps)) return false;
  inv = 1.0f / dy;
  u = s->v.eyeX + (rx - s->v.eyeX) * inv;
  v = s->v.eyeZ + (rz - s->v.eyeZ) * inv;
  *col = pg3dRound((u - s->wxmin) * s->wxs);
  *row = SCREEN_HEIGHT - 1 - pg3dRound((v - s->wymin) * s->wys);
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
  bool_t   undo;   // the undo state at entry: the command changes no register, so the earlier undo point stays valid
} pg3dEngineSave_t;

static void pg3dEngineEnter(pg3dEngineSave_t *sv) {
  currentKeyCode = 255;
  sv->undo = thereIsSomethingToUndo;
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
  fnUndo(0);
  thereIsSomethingToUndo = sv->undo;
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
  else if(result == PG3D_RUN_OK && retain && h != NULL && h->frozen) {
    h->gridValid = 1; h->label = label;
  }
  pgRefreshNow();
}
```

```c
// context, not the subject: the view hooks that call the block functions
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
  pg3dEmpty();   // §9.2.4: ERASE and PVIEW drop the retained 3D content
}

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
```

## Your task

You are auditing firmware code for bugs and design flaws. Report what you find. Do not fix anything. The design intent is stated in the Orientation; code that contradicts the stated intent is a finding, and code that contradicts your expectations but matches the intent is not.

Your one question: **does WIREFRAME keep its contract with the calculator around it?** Walk these: `fnWireframe` inside a running program (the in-program arm of `execProgram`: `programRunStop` is running at entry; what does `pg3dRunGrid`'s abort test do on the first sample; what does `runProgram` leave in `programRunStop` for the outer program); a body that ends in `STOP`, that raises an error at some points, that leaves a string in X, that calls `ERASE`, that calls `WIREFRAME` (nesting), and a user who presses EXIT during the run; the engine protocol against the sum and plot engines quoted in the Orientation: what the `saveForUndo`/`fnUndo` pair restores and what it does not (the SAVED registers, `savedSystemFlags`, `thereIsSomethingToUndo`, the stats image), and whether a saved image that holds a string or a matrix in a stack register comes back to the same pool state; the `rows` buffer when the body reset the calculator (`pg3dResetCount`); `pgSetRegion` calling `pg3dEmpty` while the run is on (the body's `ERASE`: `retain` is cleared, but the header's `numX` and `numY` were reserved before the run: who resets them?); `pgReset` from the config hook at boot before any view; and the pre-verified fact of the Orientation: name a mechanism in this code that leaves the pool, a saved register, or the undo image in a state a later equation parse can trip on.


## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing. A named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where (the function and the line of
the excerpt), the concrete reaching input (register values, program steps,
or key presses), the observable consequence, the violated contract quoted,
and your confidence. Rank by what the defect costs the owner. End with the
sites that you examined and found correct, one line each.
