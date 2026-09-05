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
// the projection (the subject)
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

static void pg3dMeshPoint(const pg3dSetup_t *s, pg3dPix_t *rows, uint32_t numX, uint32_t i, uint32_t j, float x, float y, float z, const pgRect_t *clip) {
  pg3dPix_t *cur = rows + (j & 1) * numX, *prev = rows + ((j + 1) & 1) * numX;
  int32_t col = 0, row = 0;
  bool_t ok = (z == z) && pg3dProject(s, x, y, z, &col, &row);
  cur[i].col = ok ? (int16_t)col : PG3D_NOPIX; cur[i].row = ok ? (int16_t)row : 0;
  if(!ok) return;
  if(i > 0 && cur[i - 1].col != PG3D_NOPIX) pgLine(clip, cur[i - 1].col, cur[i - 1].row, col, row);
  if(j > 0 && prev[i].col != PG3D_NOPIX)    pgLine(clip, prev[i].col, prev[i].row, col, row);
}
```

```c
// the keys, the redraw, and the zoom re-run (the subject)
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

Your one question: **is the retained content and its projection correct at the boundaries?** Walk the numbers: `pg3dEncode` and `pg3dDecode` for lo == hi (a frozen view is valid so lo < hi, but `pg3dRecordView` copies `zRecLo` and `zRecHi` from the header after a re-run: can they be equal or reversed?), for a value exactly at 253.5 steps, for negative zero; `pg3dFreeBytes` when `numX * numY` exceeds the payload; the line record offsets against the grid bytes for a 44 by 44 grid and for a 24 by 24 grid with 234 lines; `pg3dRecordView` when the header is frozen but the block was emptied under it (`frozen` is 0 after `pg3dEmpty`: what does a later `LINE3D` freeze?); `pg3dProject` for a point exactly at the eye depth plus eps, for a rotation of 90 degrees about x with the eye at (0, -3, 0), for a mirrored window (a negative scale), for `wxs` when the window is set with xmax below xmin; `pg3dRound` for 32000.4, -32000.6, NaN; `pg3dMeshPoint` on the first row (prev holds stale entries from an earlier run) and for `numX` 1 (cannot happen? `pg3dReadCount` refuses below 2, but the header's `numX` comes from a block that a reset did not rebuild); `pg3dZoomRerun`'s visible range with `wys` negative and with the eye above the volume; `pg3dKey` at `zoomStep` 8 and -8; the sine table index for `angX` 35 plus 9; `pg3dRedraw` with `gridValid` and `numY` 1 (division by zero); the `%` arithmetic on `uint8_t` step counts. For every arithmetic claim give the numbers.


## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing. A named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where (the function and the line of
the excerpt), the concrete reaching input (register values, program steps,
or key presses), the observable consequence, the violated contract quoted,
and your confidence. Rank by what the defect costs the owner. End with the
sites that you examined and found correct, one line each.
