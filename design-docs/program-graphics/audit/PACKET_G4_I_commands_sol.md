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
// the setting commands, PT3D and LINE3D (the subject)
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
  if(!(a < b)) { pgError(ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN); return; }
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

Your one question: **do the setting commands, PT3D, and LINE3D keep their contract with the block and the view?** Walk these: `pg3dEnsure` from the keyboard with the view closed (nothing allocated) followed by `PVIEW` inside a program (is the block then taken by the first command, and what did the earlier `EYEPT` store?); `pg3dReadFloat` for a long integer beyond float range, for 1e-50, for negative zero; `pg3dReadCount` for 2.0 as a real, for 100.5, for -3; `pg3dRange` when only the first read fails; `fnEyept` after the view froze (the value lands in `pg3d` and not in the header: when does it take effect, and is that the contract?); `fnLine3d` when `pg3dRecordView` refuses (the current point is kept: is that right after a partial record?), with a current point outside the volume (the clamp changes the direction: is the drawn line the record's line?), with the block full, with the view closed (nothing retained, drawn where?), with `haveCur` set from before `ERASE` (`pg3dEmpty` clears it: and `PVIEW`?); the record bytes against `pg3dDecode` for the same point (does the drawn endpoint equal the retained one?); `pgCloseView` freeing the block while `pg3d.haveCur` stays set (the next `LINE3D` in the next view uses which current point?); `pgReset` restoring `pgWindow.set` (the G3 window: does a RESET inside a program surprise the next `LINE`?); and the pre-verified fact of the Orientation.


## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing. A named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where (the function and the line of
the excerpt), the concrete reaching input (register values, program steps,
or key presses), the observable consequence, the violated contract quoted,
and your confidence. Rank by what the defect costs the owner. End with the
sites that you examined and found correct, one line each.
