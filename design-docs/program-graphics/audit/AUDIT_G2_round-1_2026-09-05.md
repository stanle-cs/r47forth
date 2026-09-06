# Audit — program-graphics stage G2, the 2D drawing commands with the round-1 fix wave: round 1, at `0663b2360`

Subject: `af7ad934a..0663b2360` on `program-graphics/stage-g0`, six commits.
The audited delta is G2 (`294f470d2`) and its out-of-family fix wave
(`0663b2360`). G0, G1 and the G1 fix wave sit inside the range. They were the
subject of the G1 round-1 report, and G1-era code appears here only where a
finder reached it.

The kernel holds. Eight in-family readers and two outside readers walked
every mask, clamp, stepper and 64-bit product. The fix wave holds under
mutation: every fix-wave pin fires for the reason its row gives. The round's
yield sits one layer out from the kernel, as G1's yield sat one layer out
from the view. **The ten commands were built for the view. The closed view
is a second surface that nobody modelled.** A `LINE` from the CANVAS menu
with the view closed is erased by the key release that drew it. A `SNAP`
step after `LINE` steps photographs the register lines. The string path
repeats G1's lesson in a smaller key: a private glyph walk under a byte-wise
guard, and upstream's own `x→α` hands it the one string shape it cannot
hold. Five pins carry assertions that no mutation can turn red. And the
authority drifted in four sentences at the commit that implemented it.

> **Filename note.** The dispatched subject string is about 2,400 bytes and
> cannot be a filename (`NAME_MAX` is 255). This file keeps the
> round-9/round-10 convention: the name is cut after the tip, and the full
> subject is stated in §1.

---

## 1. Subject and coverage

### The full dispatched subject

Program-graphics stage G2: the 2D drawing commands, with the round-1 fix
wave. The package lets a user program draw on the screen of the C47
firmware. G2 adds ten commands on item rows 2450 to 2459: `LINE`, `BOX`,
`FBOX`, `CIRCLE`, `FCIRCL`, `ARC`, `TEXTOUT`, `DISP n`, `GMODE n`, `GCLIP`.
The kernel writes the 1-bit screen buffer directly. A row is 52 bytes: byte
0 is the dirty flag, byte 1 is the row number, and the bit order is mirrored
(pixel x is bit `(399-x)&7` of byte `2+((399-x)>>3)`). The clip rectangle
`canvas.clipX0..clipY1` bounds every write. A long integer coordinate is a
pixel with a magnitude up to 32767. A real is converted with `real34ToInt32`
after a range check. The y axis points upward (`PG_ROW_OF(y) = 239 - y`).
Authority: `design-docs/program-graphics/DESIGN.md` §2, §4, §5, §8 and §11.
`TESTING.md` §4 lists the pins D1-D18 and S1. `DESIGN-HISTORY.md` records
G2 and its fix wave. Package working area: `packages/program-graphics/`,
a flat mirror of the upstream paths. The generated `patches/` and `files/`
are the build input. Upstream sources are under `src/c47/`. Siblings:
forth-core, undo-history, pretty-print, pretty-print-extra.

Pre-verified facts, not re-reported here. The round-1 out-of-family readers
found six defects, and the fix wave repaired each with a red-first pin:

- `GCLIP` narrowed an unclamped row into `int16`. Now an intersection with
  an empty-clip sentinel, pin D13.
- The filled-circle square root overflowed `int32` above radius 16384, and
  `4·r²` above 23170. Now `int64`, and the fill loop is limited to the clip
  rows, pin D14.
- `DISP` ignored the clip columns, pin D15.
- The arc direction vectors were scaled by 1024 and collapsed spans under
  0.056 degrees. Now 65536, pin D16.
- The string cap cut inside a two-byte glyph, and the trim loop then stepped
  over the NUL, pins D17 and D17b.
- A NaN or infinite angle was accepted, pin D18.

Two out-of-family claims were refuted by the operator and handed to the
refutation pass: that `TEXTOUT` and `DISP` corrupt `tmpString` for the
program runner, and that `pgError` leaves `canvas.errorShown` stale. A long
integer register is allocated to the exact limb size of its value
(`convertLongIntegerToLongIntegerRegister`), so the fast-path reader sees no
stale bytes. Three claims from a lifecycle finder over the G1 range were
handed to the refutation pass:

- The interactive `ENTER` press paints the T register line over canvas rows
  20 to 59, through `showFunctionName` and `hideFunctionName`.
- The long-press EXIT `SNAP` gesture paints `EXIT`, `SNAP` and the T line
  before the screenshot.
- In the combined build, the undo-history shift-delegation arm lets f and g
  engage inside the view.

Documented limits, not reported: the DM42 hardware is not testable here and
all evidence is from the simulator, `DISP` for lines 8 to 11 in region 2 is
dropped whole, region code 1 is unsupported, and `VIEW`/`AVIEW` inside the
view do nothing. DESIGN.md §10 lists nine limits. Six of them are not among
the dispatched four: sleep and power off (1), text not clipped inside its
cell (3), `GMODE` not applied to text (4), `RESET` inside the view and a
plot step that abandons the canvas (7), shifted keys (8), and the error band
over rows 20 to 39 (9).

### The commits

| commit | role in this range |
|---|---|
| `a0077d62b` | G0: the skeleton, the baseline driver and the design. Audited at G1. |
| `cb2ae56e7` | G1: the canvas view. Audited by the G1 round-1 report. |
| `c452d8796` | The G1 fix wave. Unaudited until this round: K7 (G2R1-13) is in it. |
| `294f470d2` | **G2**: `pgmGraphics.c` +758 lines, `items.c` rows 2450-2459 replaced in place, the `CANVAS` softmenu, `screen.h` prototypes, pins D1-D12 and S1, DESIGN.md +32/−, TESTING.md +22. 21 files, 1,796 insertions. |
| `c9e589b25` | The audit workflow fix (finder output schema). No package code. |
| `0663b2360` | **The fix wave**: `pgmGraphics.c` +200/−45, pins D13-D18, DESIGN.md 34 lines, DESIGN-HISTORY +70, the two packets and the two replies. 10 files, 2,107 insertions. |

The whole range is 55 files and 46,397 insertions. 44,600 of those are the
flat-mirror copies of upstream files, byte-identical except the patched
lines. The audited code is `pgmGraphics.c` (1,264 lines) and `pgmGraphics.h`
(42), the eleven generated patches, and `tests/program_graphics.txt`.

### What was read

Every finder read DESIGN.md, TESTING.md and DESIGN-HISTORY.md in full.
Every finder read `pgmGraphics.c` line by line, from the view hooks through
the kernel, the readers, the ten command bodies and `pgStringCut` to the six
test drivers. The upstream reading, by dimension:

| dimension | upstream sites read |
|---|---|
| contracts | `reallyRunFunction`, `runFunction`, `_executeOp` and the indirect arms, `runProgram` and its stop path, `indirectAddressing`, `real34ToInt32`, `showGlyphCode`, `showString`, `_resetStringMode`, `refreshScreen` head and tail, `_selectiveClearScreen`, `fnSNAP`, the four PIXEL-family flag setters, `_calculateStringWidth`, `displayCalcErrorMessage`, `_view`, `fnPause`, `lcd_fill_rect`, `bitblt24`, the `SCRUPD` flags, `TMP_STR_LENGTH`, `MAX_NUMBER_OF_GLYPHS_IN_STRING`, the softkey row geometry |
| lifecycle | `refreshScreen`, `_refreshNormalScreen`, `clearScreen`, `showSoftmenuCurrentPart`, `refreshLcd` and the cursor blink, `hideCursor`, `btnPressed`, `btnReleased`, `btnFnReleased`, `executeFunction`, `determineItem`, `processKeyAction`, `fnKeyExit`, `fnRunProgram`, `fnStopProgram`, `calcModeNormal` and all 18 callers, `calcModeAim`, `leaveTamModeIfEnabled`, `fnReset`, `fnClrMod`, `flagBrowser`, `saveRestoreCalcState`, `assignToMyAlpha`, undo-history's `setConfirmationMode` |
| arithmetic | `c47-gtk/hal/lcd.c` in full, `hal/lcd.h`, the `lcd_buffer` allocation, `charString.c` (width, `charCodeFromString`, `stringNextGlyph`), `showGlyphCode`, `_doXToAlpha`, `_alphaMid`, `_addString`, the long-integer register layout, `findGlyph`, and the generated standard font (1,373 glyphs, max 18 columns, 20 rows) |
| errorpaths | `error.c`, the `processKeyAction` preamble and the EXIT and BACKSPACE cases, the guard arm, `btnFnPressed`, `commonShiftProcessing` and its mode gate, the `reallyRunFunction` error and undo block, `fnExecute`, `runProgram`, `stopProgram`, the upstream error painter, `refreshLcd`, undo-history's shift gate |
| guards | `lcd_fill_rect`, `showGlyphCode`, `showString`, `runFunction`, `indirectAddressing`, `_executeOp`, the TAM completion paths in `ui/tam.c`, the register accessors, the `STD_` two-byte codes, `_screenFileStep`, the standard font geometry |
| tests | the test suite's `hal/lcd.c` (no-op refresh, so dirty flags never clear inside a test), `showGlyphCode`, the standard font `O` glyph, the error arm and the EXIT arm, the package refresh case and the `refreshRegisterLine` guard, the main tree's testlog |
| design | `charString.c`, `_doShowString`, `fnSNAP`, `bitblt24`, `lcd_fill_rect`, `_addString`, `saveRestoreCalcState.c:1378`, `lblGtoXeq.c:360-411`, all 711 standard-font glyph cells, the `PAUSE` item row, `isFunctionOldParam16` |
| upstream | `fnKeyEnter`, `fnKeyCC`, `fnKeyDotD`, `fnTo_ms`, the `refreshScreen` `last_CM` neighbours and the `lcd_refresh_dma` precedent, `stringLastGlyph`, the `lcd_buffer` init, the row-stride sites, `LCD_LINE_SIZE`, `softmenu[]` rows 170-186, the sibling `softmenus.c` and `keyboard.c` hunks (md5 of the shared lines) |

The verifiers checked out `0663b2360` before any read (every worktree
arrived at `9fdc5753c`, a stale upstream merge that is not an ancestor of the
tip). Eleven of the eighteen findings below carry a mutation or a probe that
was applied, observed and reverted in the same step.

### What the reading did not reach

The bodies of `C47_WP34S_Cvt2RadSinCosTan` and `convertAngleFromTo` (angle
reduction and DMS handling are taken on trust). The DMCP `lcd_refresh_dma`
path and the DM42 ROM. The GTK simulator: no key was pressed in a window,
and every key claim is a read of the two key paths or a headless probe. The
plot commands' own `calcMode` switch (taken from DESIGN.md §3.6). The pixel
width of the longest error message (46 characters) against the 399-pixel
line. `fnScreenDump` and the WP34S sine and cosine. `printing/print.c`'s
`calcMode` tests. The `tam.c` completion site beyond `leaveTamModeIfEnabled`.
DESIGN.md §9 (G4). The DMCP main loop beyond the cursor site. The three
G1-range key claims (U7-U9 in §4.2) were handed to the refutation pass and
sat beyond its cap, so no finder re-examined them.

### Deliberately not audited

The nine documented limits above. The flat-mirror copies. The G0 baseline
driver. The G1 view code except where a finder reached it (G2R1-3, -5, -14,
-17). The audit packets and replies as prose (their claims are in §6.1).

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `design-docs/program-graphics/audit/PACKET_G2_C_kernel_sol.md` (606 lines, axis: the kernel and its boundary arithmetic) | `design-docs/program-graphics/audit/REPLY_G2_C_sol.md` (375 lines) | `MODEL: GPT-5` | 4 numbered (`GCLIP` int16 narrowing, `pgIsqrt` overflow, `DISP` clip columns, arc quantization at 1024), plus 2 gaps named in its cleared section that the fix wave treated as claims (the string cap inside a glyph, `pgReadAngle` NaN) |
| gemini | `design-docs/program-graphics/audit/PACKET_G2_D_contracts_gemini.md` (606 lines, axis: the callers and the shared state) | `design-docs/program-graphics/audit/REPLY_G2_D_gemini.md` (50 lines) | `MODEL: Gemini 3.1 Pro (High)` | 5 numbered (the string cap over the NUL, `tmpString` corruption, `FCIRCL` overflow, `DISP` clip, `errorShown` desync) |

Both replies exist, both open with a `MODEL:` line, and neither is empty.
No `.err` file exists for the G2 replies, so the codex session header that
the G1 report cross-checked (`gpt-5.6-sol`) is not on disk for this round.
Sol's line reads `GPT-5`, not an exact model name. Recorded, not
re-dispatched. Both readers read the G2 commit `294f470d2`. The fix wave is
the repair of their findings, and no outside reader has read it.

### In-family coverage

Eight finders, one per dimension (contracts, lifecycle, arithmetic,
errorpaths, guards, tests, design, upstream), each blind to the others.
Every finding then went to three verifiers with distinct lenses
(reachability, correctness, intent), each instructed to refute. Twenty-four
verifier-confirmed entries came back, eight refuted entries, and ten claims
sat beyond the verification cap (§4.2). This is the first in-family leg of
this package that the platform classifier admitted (`c9e589b25` fixed the
finders' output schema). G1's in-family finders were refused.

---

## 2. Mechanical results

| check | result |
|---|---|
| `tools/pkg_patch_refresh.py packages/program-graphics` at the tip | idempotent: `git status --porcelain packages/program-graphics/` is empty after the refresh (the upstream finder, and every verifier that mutated and reverted) |
| solo gate, `build-test.sh --solo`, re-run for this report in the clean verifier worktree `wf_8420fae9-eb2-10` at `0663b2360` | **GREEN**, `13022 TESTS PASSED SUCCESSFULLY`, 0 failed. `program_graphics.txt` performed. Showcase S1: `10500 lit pixels in rows 20 to 239`. Baseline line: 1,000,000 steps, NOP 99 ms, PIXEL 168 ms, PIXEL body 69 ms |
| combined gate, `build-test.sh --combined` (the four siblings), re-run in `wf_8420fae9-eb2-9` at the tip | **GREEN**, `13048 TESTS PASSED SUCCESSFULLY`, 0 failed. S1 10500. Matches both stage commits' records |
| compiler warnings, solo build | 7. **One in a package file**: `pgmGraphics.c:557:80` `-Wsign-compare` (`int16_t` against `uint32_t`), once per target (c47, r47, testSuite). The rest are upstream: `c47.h:10` `_XOPEN_SOURCE` redefined (2), `testSuite.c:5501` and `:5509` `-Wformat-overflow` (2) |
| compiler warnings, combined build | 5: the same `pgmGraphics.c:557` warning (3) and `c47.h:10` (2). The `testSuite.c` pair does not appear under the combined stack |
| `design-audit.sh` | forth-core's drift script (`PKG="packages/forth-core"`). Ran in the main tree: exit 1, one finding group, two `WS-ONLY` hits in **forth-core's** `010-softmenus.c.patch` (a re-indented `showKey`/`diagonalsOnTop` pair inside a wrapped block, from `b5c4020af`, 2026-08-10). Outside this subject, pre-existing, not this package's. No equivalent script exists for program-graphics |
| `patch_churn_scan.py` over the eleven patches (upstream finder) | exit 0. adds 157, dels 21, hunks 30. Zero `WS-ONLY`, zero `COMMENT-ONLY`. Modified upstream lines: `keyboard.c` 6, `items.c` 13 (the sanctioned CAT_FREE row fills), `softmenus.c` 1, `addons.c` 1 |
| firmware size (DESIGN-HISTORY, `make dmcp5r47`, R47.elf) | flash +4,312 bytes for the package at the fix wave (+272 against the G2 commit), RAM +52 bytes (the 64-bit square root and the sentinel clip). Not re-measured here |
| tree at the end | both gate worktrees clean at `0663b2360` after their two showcase BMPs were removed. Every verifier reported an empty `git status --porcelain` after its probe. The main working tree is on `program-graphics/stage-g0` at the tip with unrelated forum modifications that are not this audit's. This pass wrote only this file |

**The warning at line 557.** `while(tmpString[0] != 0 && stringWidth(...) >
width)` compares the `int16_t` that `stringWidth` returns with the `uint32_t`
parameter of `pgStringCut`. The arithmetic finder traced the comparison
before the warning was collected. A width above 32,767 pixels wraps
negative, the promotion makes it a large unsigned value, and the loop keeps
trimming. A wrap to a small positive value needs 65,536 pixels, above the
2,558 × 18 = 46,044 maximum, so the loop cannot under-trim. Cleared in §6.3.
An `int32_t` accumulator or one cast silences it.

Two harness notes from the probes, neither a finding. A probe that called
`fnLoadProgram` left `aimBuffer` holding `PROGRAM` and failed
`string_cov.txt` line 474 in that run only. The session scratchpad is shared
across verifier worktrees, and two verifiers' log files carried each other's
names. Each verdict rests on its own `build.sim` (the shadow file, the ninja
log and the testlog), not on the shared logs.

---

## 3. CONFIRMED findings

Twenty-four verifier-confirmed entries collapse to eighteen distinct
findings. Four entries were the same defect seen from two or three
dimensions: the flag protocol (contracts, lifecycle), the mixed-type pair
(contracts, errorpaths, tests), the 31-bit sentence (arithmetic,
errorpaths), and pin D17b (arithmetic, guards, tests). The last finding,
G2R1-18, survived one lens and was refuted by four sibling verifiers on the
same claim. It is kept, at the bottom, as a doc residue and the split is
recorded. Tally: 4 wrong-result, 8 latent, 6 design-flaw, 0
crash-or-data-loss. Ranked by cost to the owner. Line numbers are
`packages/program-graphics/pgmGraphics.c` at `0663b2360` unless stated.

### G2R1-1 — With the view closed, the ten commands skip the PIXEL flag protocol. A `LINE` from the CANVAS menu is erased by its own key release, and `SNAP` after `LINE` steps photographs the register lines

**Where.** `pgmGraphics.c:416-424` (`pgRefreshMaybe`, `lcd_refresh` only) and
`:437-444` (`fnGline`) with its nine siblings. The only package writes of
`screenUpdatingMode` and `screenHoldsDrawnPixels` are in `fnPview`
(`:46-47`), for the view.

**What breaks.** Two forms. Keyboard: in `CM_NORMAL`, `10 ENTER 10 ENTER
100 ENTER 100`, then P.FN page 2 → CANVAS → LINE. `btnFnReleased` →
`executeFunction` → `runFunction(ITM_GLINE)` → `fnGline` → `pgLine` writes
the buffer and `pgRefreshMaybe` flushes it. The tail of `btnFnReleased`
(`src/c47/keyboard.c:1442`) then calls `refreshScreen(114)`.
`_selectiveClearScreen` (`screen.c:5668-5700`) clears the stack band because
`SCRUPD_MANUAL_STACK` is clear, and the register lines repaint over the
rows. The owner presses LINE and sees no line. `PIXEL` from its own menu
stays on screen until the next key, because `fnPixel` sets the three
`SCRUPD_MANUAL_*` bits and `screenHoldsDrawnPixels` (`screen.c:6556-6557`).
`fnCllcd`, `fnPoint` and `fnAGraph` do the same. Program: `LBL A, LINE,
SNAP, END` with no `PVIEW`, `CLLCD` or `PIXEL` before it. `fnSNAP`
(`screen.c:6294-6299`) reads `screenHoldsDrawnPixels`, finds it false, sets
`SCRUPD_AUTO` and calls `refreshScreen(80)` before `fnScreenDump`. The
screenshot shows the register lines, not the drawing.

**Measured.** An `AUDIT-PROBE` in `pgTestSmoke` at the tip, loaded with
`fnLoadProgram` and run through `fnExecute`, then reverted. Step level:
`LINE` leaves `screenUpdatingMode = 16` and `hold = 0`. `PIXEL` sets
`mode = 30`, `hold = 1`. `LINE, SNAP`: 3,012 set bits, 1,232 of them in rows
60-179 (the register lines), 0 of 91 hits on the (10,10)-(100,100) diagonal.
`PIXEL, SNAP`: 52 set bits, the pixel lit, no register band. Keyboard form
through `reallyRunFunction` then `refreshScreen(0)`: the `LINE` pixel
erased, the `PIXEL` pixel kept.

**One narrowing.** The `AVIEW` half of the finder's claim is not
PIXEL-divergent. `runProgram` resets `screenUpdatingMode` to `SCRUPD_AUTO`
after every step (`lblGtoXeq.c:1005-1006`), so `_view`'s `refreshScreen(151)`
erases a `PIXEL` drawing too. The probe showed both `LINE, AVIEW` and
`PIXEL, AVIEW` erased. The finding stands on the key release and on `SNAP`.

**Why wrong.** DESIGN.md §4.2: "While the view is closed, the drawing
commands still work, as PIXEL does, and the clip is the whole screen, rows 0
to 239." PLAN §2 ruling 1: "The upstream flag protocol (option B) stays
usable without the view." PLAN §3.2, row "A drawing command runs, view
closed": "The upstream flag protocol applies, as for PIXEL today. The
drawing lives until the next repaint." `c47.h:234` defines
`screenHoldsDrawnPixels` as "CLLCD, PIXEL, POINT or AGRAPH painted the screen
and no refresh has repainted over it since", and `fnSNAP` reads it to keep
"a screen a program drew". The SURVEY's table of the PIXEL family defines
"as PIXEL does" as the manual bits plus the flag. No document, comment or
history entry rules the omission deliberate. Pin V7 covers a raw
`setBlackPixel` under `SCRUPD_AUTO` and never runs a G2 command.

**Bug class.** A protocol inherited by name, not by mechanism: the code
copies PIXEL's write and not PIXEL's flags. Also the second-surface class:
a command family built for one mode, unmodelled in the other.

**Class-level test.** The ten commands are enumerable. For each, in
`CM_NORMAL` with the view closed: run the command through `runFunction`,
call `refreshScreen(0)` as the key release does, and assert the drawn pixel
is lit and `screenHoldsDrawnPixels` is true. Then the program form `LINE,
SNAP` against `PIXEL, SNAP`, comparing the dumped buffers. Red today for all
ten.

### G2R1-2 — A string that ends in a lone lead byte is trimmed only when it is too wide. `TEXTOUT` and `DISP` then paint stale `tmpString` bytes after it

**Where.** `pgmGraphics.c:557`. The lone-lead-byte trim lives inside
`while(stringWidth(tmpString, ...) > width)`, so it runs only when the
string is too wide.

**Reaching input, from the keyboard.** Put `"HELLO WORLD"` in X and run
`DISP 1` (`tmpString` now holds that text). Then `256 x→α` (item 2785,
STRINGS menu): `stringFuncs.c:399-403` builds `char1 = 0x81, char2 = 0x00`
and stores the one-byte string `"\x81"` (the zero second byte is dropped at
`:427-437`). Roll it to Z, put 100 in Y and 100 in X, `PVIEW 6`, `TEXTOUT`.
`DISP 2` with the string in X reaches the same code. `pgStringCut` copies
`"\x81\0"` into `tmpString` and calls `stringWidth`. `_calculateStringWidth`
(`charString.c:252-254`) reads the NUL as the second byte of the glyph,
advances past it, and its `while(str[ch] != 0)` keeps reading `"LLO WORLD"`
from the stale buffer. The measured width is small, the trim loop never
runs, and `showString` (`_doShowString` via `charCodeFromString`, same
stepping) paints the not-found glyph for `0x8100` and then the stale text.

**Measured.** Probe in `pgTestDraw2D`, gate green with the probe (it only
prints), reverted. The register after `x→α`: `type=5 bytelen=1 b0=81
b1=00`. After `DISP`: `tmp=81 00 4c 4c 4f`, `tmp+2="LLO WORLD"`. The
rightmost lit column of the `DISP 1` band is 98, against 9 for a control
one-glyph two-byte string under the same stale buffer.

**What the owner sees.** A hexadecimal not-found glyph followed by the tail
of an earlier command's text: the last `DISP`, a formatted number, whatever
`tmpString` held. The garbage varies with the session's history, so the
same program draws different text on different runs.

**Why wrong.** DESIGN.md §4.6: "The cut never splits a two-byte glyph, at the
width and at the cap of the scratch buffer. A string that ends in a lone
lead byte is trimmed at that byte." The sentence is unconditional. Pin D17b
uses width 1, which forces the trim loop to run, so it never exercises the
fits-the-width branch. Upstream stores a malformed string here (a one-byte
lead), and the package's contract says it handles that shape.

**Bug class.** Orphan byte at an atomic boundary. A byte-wise idiom on
glyph-wise content. A guard placed inside the loop it must precede.

**Class-level test.** Take every register string that ends in a lone lead
byte (`k·256` through `x→α`, and the D17b fixture string) and the widths 1,
50 and 399. After `pgStringCut`, assert that `tmpString` holds no lone lead
byte. Assert that `stringWidth(tmpString)` equals the width of the same
bytes with a zeroed tail. The second assertion is the over-read oracle from
the D17b probe (G2R1-10).

### G2R1-3 — The view opens from alpha input mode without upstream's AIM prologue. The blinking cursor then paints and wipes the canvas

**Where.** `pgmGraphics.c:36-52` (`fnPview`, G1 code), reached through
`fnErase` (`:56`).

**Reaching input.** Put `ERASE` (or `PVIEW`) into MyAlpha once: `ASSIGN` in
alpha mode, `assign.c:848` `assignToMyAlpha`. `ITM_ASSIGN` sits on the
`MNU_ALPHA` menu (`softmenus.c:1015`), the ASSIGN arm stores any item
(`keyboard.c:1365`) and pushes MyAlpha (`:1367-1370`), and the package's
`CANVAS` softmenu offers `ERASE` and `PVIEW` as softkeys to pick. Then in
alpha input mode press that MyAlpha softkey. `keyboard.c:1270` skips
`closeAim()` because `isAlphabeticSoftmenu()` is true for MyAlpha
(`softmenus.c:3866-3874`, `4174`). `executeFunction` runs
`runFunction(item)` for any item in AIM (`:1382`) → `fnErase` → `fnPview(2)`:
`prevCalcMode = CM_AIM`, `calcMode = 21`, and `cursorEnabled` and
`FLAG_ALPHA` are left as they were. The 100 ms timer `refreshLcd`
(`screen.c:517-527`, `555-565`) then runs `showGlyph(STD_CURSOR, ...)` and
`hideCursor` (`:1988`, a 6 × 6 white `lcd_fill_rect` at `(xCursor, yCursor +
10)`) at the alpha line, rows 148-153, inside the canvas.

**Measured.** Probe in `pgTestKeys` through the real key entry points
`btnFnPressed`/`btnFnReleased` with the double-tap timer callback
`execFnTimeout(38)` (the release arms `TO_FN_EXEC`, `FLAG_G_DOUBLETAP` is on
by default): `post: calcMode=21 cursorEnabled=1 FLAG_ALPHA=1 region=2
prevCalcMode=1 yCursor=138`; canvas pixel (3,150) before `hideCursor` 1,
after 0. Gate green with the probe, reverted.

**What the owner sees.** A cursor blinks inside the canvas and each blink
wipes a 6 × 6 patch of the drawing. The status bar keeps the alpha
indicator while the view is open. A program step that calls
`calcModeNormal` (`CLSTK`, `CLA`) no longer clears `FLAG_ALPHA` or the
cursor either, because the package guard (`010-calcMode.c.patch`) returns
before those lines.

**Why wrong.** DESIGN.md §3.6, the `refreshScreen` row: the canvas case
"does not clear or paint any other row". Upstream's own convention for a
mode entered from alpha input is the prologue of `flagBrowser`: `if(calcMode
== CM_AIM) { hideCursor(); cursorEnabled = false; } previousCalcMode =
calcMode; ... clearSystemFlag(FLAG_ALPHA);`. The sibling undo-history does
the same in `setConfirmationMode` (`packages/undo-history/config.c:1066-
1069`). `fnPview` has neither line.

**Bug class.** An exemption inherited without its paired clear. The G1 audit
gave `calcModeNormal` a guard for the view and the guard also skips the
alpha clears, which nothing else does for mode 21.

**Class-level test.** Take each entry into mode 21 (`fnPview` through
`runFunction`, `fnErase` with the view closed) from each of `CM_AIM` and
`CM_NORMAL`. Assert `cursorEnabled` false and `FLAG_ALPHA` clear after the
entry. Assert that a canvas pixel inside the cursor rectangle survives one
`hideCursor`. The pin must drive `btnFnPressed`/`btnFnReleased` and the
timer callback, as the probe did. Add the `CM_NIM` case as a negative
assertion: `executeFunction` closes NIM before `runFunction`, so
`prevCalcMode` must never be `CM_NIM`.

### G2R1-4 — `ARC` collapses a span just under a full turn to one pixel

**Where.** `pgmGraphics.c:524-530`, the `cross == 0 && dot > 0` single-pixel
arm, and `:513-516`, `fullCircle = d >= 360`.

**Reaching input.** DEG mode. `T = 200+100i`, `Z = 50`, `Y = 0`, `X =
359.9995`, `ARC`. `ax = 65536, ay = 0`; `by = (int32)(sin(359.9995°) ·
65536) = (int32)(−0.572) = 0`; `cos` rounds to `1.0f`, so `bx = 65536`.
`cross = 0`, `dot > 0`, the single-pixel arm runs. `d = 359.9995 < 360`, so
the full-circle test does not catch it. The same for `Y = 90, X = 89.9995`,
which §2.2 defines as a counterclockwise arc of 359.9995°. The collapse band
is `|sin δ| < 1/65536`, about 0.00087°, on either side of a full turn.

**What the owner sees.** One pixel at angle a1 instead of an almost complete
circle. The arc vanishes.

**Why wrong.** DESIGN.md §2.2: "Draws an arc counterclockwise from a1 to a2
... A span of 360 degrees or more draws a full circle." §4.5: "The resolution
of the span is about 0.001 degrees." A resolution limit explains a tiny span
rounding to a point, not a near-full turn rounding to a point. The code
computes `d` exactly in degrees and tests only `d >= 360`. It then hands the
decision to the quantized vectors, where B equals A both for a zero span and
for a near-full turn. The round-1 fix (Sol 4) changed only the scale. Sol's
own sweep checked 359°, above the band, and never the band below 360.
Nothing in DESIGN.md, TESTING.md, DESIGN-HISTORY.md, the fix-wave message or
the code comments names the same-direction arm.

**Bug class.** A quantized predicate decides a case the exact value already
decided. `d` is in hand and unused at the arm.

**Class-level test.** For ε in {0.0001, 0.0005, 0.0008} and spans `360 − ε`
at radius 50, in each of DEG, RAD, GRAD and MULTPI: assert the pixel at
`a1 + 180°` is lit. For `a2 = a1 − ε` assert the same (the long way round).
For ε = 0 assert the full circle. Red today for every ε in the band.

### G2R1-5 — In region 2, a menu popped by a program step to a blank base keeps its old labels painted

**Where.** `pgmGraphics.c:80-95` (`pgRefreshCanvasView` →
`showSoftmenuCurrentPart`, G1 code).

**Reaching input.** `FLAG_BASE_HOME` and `FLAG_BASE_MYM` both clear (a blank
base menu). `CM_NORMAL` with the P.FN menu up. Program: `PVIEW 2, LINE ...,
EXITALL, STOP`. `fnExitAllMenus` (`softmenus.c:4250`) has no `calcMode` gate
and pops to `softmenuId 0`. `IS_BASEBLANK_(0)` (`defines.h:2426`) is true,
so `showSoftmenuCurrentPart` (`softmenus.c:3118-3121`) skips both its
`clearScreenOld(false, false, true)` and its paint. The mode-21
`refreshScreen` case (package `screen.c:6227`) calls only
`pgRefreshCanvasView` and never `_selectiveClearScreen`, which is what clears
the band in `CM_NORMAL`. The stop path reaches this refresh: `stopProgram`
calls `refreshScreen(4)` (`lblGtoXeq.c:1024`).

**Measured.** Probe in `pgTestView`: lit pixels in rows 171-239 before
`EXITALL` 7,702, after the refresh 7,702, after `pgCloseView` 0. Gate green
with the probe, reverted.

**Correction to the finder's input.** "(or CLMENU)" is wrong. `fnClearMenu`
(`programmableMenu.c:135`) clears the programmable menu contents and pops
nothing. `EXITALL`, or any pop that lands on id 0 under a blank base, is the
trigger.

**What the owner sees.** After the program stops, the P.FN labels are still
on the screen with an empty stack. The softkeys are dead in the view, so the
owner sees a menu that is not there until EXIT repaints everything.

**Why wrong.** DESIGN.md §3.6, `refreshScreen` row: the case calls
`showSoftmenuCurrentPart()` when region is 2, which the design reads as "the
band shows the current menu". DESIGN-HISTORY line 114: "the softmenu painter
clears its band before it paints". That sentence is false for the blank
base.

**Bug class.** A painter's precondition assumed by a caller. The painter
clears only when it paints, and the caller skipped the clear the painter
relies on.

**Class-level test.** The pops that can land on a blank base inside the view
are enumerable by a grep of `popSoftmenu` callers without a `calcMode` gate.
For each, with both base flags clear: after `refreshScreen(4)`, rows 171-239
hold zero lit pixels.

### G2R1-6 — A mixed long-integer/real point pair is accepted and drawn. DESIGN.md §5.1 says it is `ERROR_INVALID_DATA_TYPE_FOR_OP`

**Where.** `pgmGraphics.c:427-435` (`pgReadTwoPoints`) and `:354-398`
(`pgReadCoord`). Callers: `fnGline :437`, `fnGbox :449`, `fnGfbox :458`,
`fnGclip :627`. Three dimensions found it, and three verifiers confirmed it.

**Reaching input.** `X = 10` (long integer), `Y = 20.5` (real), `Z = 100`,
`T = 100`, `LINE`, from a program or the CANVAS menu. `pgReadCoord` reads
each register by its own type and records nothing. `pgReadTwoPoints` calls
it four times with a short-circuit OR and compares nothing. The line draws
with the truncated real and no error.

**Measured.** Two probe pins asserting the §5.1 rule (a within-point mix and
a cross-point mix) turned the solo gate red on both: "did not raise the data
type error" and "the mixed pair drew". Reverted.

**Consequence.** In G2 a real is a pixel after truncation, so the picture is
what the user meant. From G3 on (§5.2), a real goes through the window and
a long integer stays a pixel. The same program then draws one endpoint in
pixels and the other in user units, silently.

**Why wrong.** DESIGN.md §5.1, line 308, under the heading "Argument types,
stage G2 and G3": "Both points of one command must use the same type. A
mixed pair is the same error." The sentence dates from G0 and the G2 commit
rewrote the two table rows above it and kept it. No entry in §10,
DESIGN-HISTORY.md, TESTING.md or the round-1 records scopes it to G3. The
only type pin, D12, puts a string in X and passes with a per-register check.
One over-read in the finders' text: the sentence speaks of points, so the
`CIRCLE` radius and the `ARC` angle are not covered by it.

**Bug class.** A rule stated and not enforced, with no pin.

**Class-level test.** For each of the four `pgReadTwoPoints` callers, the
fourteen non-uniform patterns over {long integer, real} for four registers
raise `ERROR_INVALID_DATA_TYPE_FOR_OP` and draw nothing, and the two uniform
patterns draw. This finding needs a ruling before a fix: if the rule is
meant for G3 only, §5.1 gets the stage qualifier and the code stays. Either
way one of the two sides changes.

### G2R1-7 — `fnGarc` carries a second copy of the real-coordinate reader instead of calling it

**Where.** `pgmGraphics.c:493-507` against `:380-392`. `fnGarc` repeats the
lazy limit init, the NaN/Inf/±32768 guard and `real34ToInt32` on the two
halves of the complex in T, byte for byte. It calls `pgReadCoord` only for
the radius in Z (`:508`).

**Reaching input.** None today. Both copies agree, so no input gives a wrong
pixel. The next touch is G3 (§5.2, line 321): the real arm of `pgReadCoord`
becomes `pixel = int32(round((x − xmin) · xscale))`. Nothing forces the
`fnGarc` copy to follow. The G3 plan row (PLAN line 209) names only
"transform pins against screenWindowRatio", and D6 pins today's mapping.
After `XRNG`/`YRNG`, `ARC` with center `0.5+0.5i` in T lands at pixel (0,0)
while a `LINE` endpoint of reals `0.5, 0.5` maps through the window, with no
error.

**Why wrong.** DESIGN.md §5.1, line 305: "complex, for `ARC` center ... | two
reals | The slow path, twice." The doc says one path applied twice. The code
has two paths. A register-taking `pgReadCoord` cannot read the imaginary
half, which explains the copy and does not justify it.

**Bug class.** Structural rule spelled per-site (`bug-classes.md:63`). Two
places that must agree with nothing forcing them to.

**Class-level test.** Today, a source-level pin: `real34ToInt32` appears once
in `pgmGraphics.c`. At G3, a pin that draws `ARC` with a complex center and
`LINE` with real endpoints at the same user point and asserts the same
pixel. The fix shape is a `real34`-taking helper called from both sites.

### G2R1-8 — `pgArc` works in the user frame while §3.4 rules that every internal function works in screen coordinates

**Where.** `pgmGraphics.c:302-334` (`pgArcPoint` and `pgArc` take `cyUser`
and convert with `PG_ROW_OF` at every plot). `pgCircle` (`:246`) takes a
screen row. `fnGarc` calls `pgCircle` with `PG_ROW_OF(cy)` at `:523` and
`pgArc` with the raw `cy` at `:533`, two same-shaped calls in opposite frames
ten lines apart.

**Reaching input.** None today: both calls are correct. The next touch that
treats the two calls alike (the G3 window, or a refactor) passes
`PG_ROW_OF(cy)` to `pgArc`, and the arc's center lands on row `cy` instead
of row `239 − cy`. The center is mirrored about row 119.5 and the arc is
translated with it.

**Measured.** That mis-call, applied as a probe, turned D6, D16 and S1 red.
The latent error is fenced today. Reverted.

**Why wrong.** DESIGN.md §3.4, line 123: "Every internal function of the
package works in screen coordinates." §4.5 says nothing about a frame. The
exception lives only in the comment at `:299-300`.

**Bug class.** An exception recorded in a comment and not in the authority.

**Class-level test.** D6, D16 and S1 already fence the mis-call. The action
is either an authority record of the exception in §3.4 and §4.5 or a
refactor of `pgArc` to take the screen row and negate `dy` at the span test.

### G2R1-9 — The string-cap guard cannot tell a lead byte from a trail byte with bit 7 set

**Where.** `pgmGraphics.c:549-553`: `if(n >= TMP_STR_LENGTH - 1) { n =
TMP_STR_LENGTH - 2; if(n > 0 && ((uint8_t)s[n - 1] & 0x80)) n--; }`. The
guard tests only bit 7 of `s[n − 1]`. A trail byte carries bit 7 whenever
the glyph's low byte is at or above 0x80: `STD_e_ACUTE` is `"\x80\xe9"`,
`STD_DEGREE` `"\x80\xb0"`, `STD_SUB_1` `"\xa0\x81"`, `STD_ALPHA` `"\x83\x91"`.

**Reaching input.** The finder called this UNREACHED, because item functions
cap register strings at 508 glyphs (`defines.h:2097`, `stringFuncs.c:419,
458`, `addition.c:433`). The verifier found the path the cap does not cover.
`LOAD` or `LOADR` (`items.c:3361-3363` → `doLoad` → `restoreOneSection`,
`saveRestoreCalcState.c:1618-1637`) reads a `"Stri"` value line of up to
2,559 bytes with `read2Lines(..., tmpString, TMP_STR_LENGTH)`. It decodes
the line with `utf8ToString` into `errorMessage` (4,096 bytes, no overflow
on the way). It then allocates the register to the full length
(`:1374-1379`) with no glyph cap. A hand-written state file whose R00 line is 2,556 `'A'`, then
UTF-8 `C3 A9` (U+00E9, internal `80 E9`), then `'A'` gives a 2,559-byte
register string. `RCL 00`, two coordinates, `TEXTOUT` (or `DISP n`): `n =
2559 ≥ TMP_STR_LENGTH − 1`, the guard sees `s[2557] = 0xE9`, backs up one
byte, and leaves the lone lead byte `0x80` at `tmpString[2556]`, the exact
split it exists to prevent.

**Consequence.** `stringWidth` → `_calculateStringWidth` consumes the NUL as
the trail byte and walks `tmpString[2558]`, `[2559]` and past the 2,560-byte
buffer until a zero byte (`malloc`'d on the PC, DMCP `aux_buf` on the
calculator). The trim loop's lone-lead stop then repairs the string, so the
drawn text is right. The effect is the over-read. Pin D17 uses `'B'` as the
trail byte and cannot see this case. Severity latent, as the finder said.

**Why wrong.** DESIGN.md §4.6: "The cut never splits a two-byte glyph, at the
width and at the cap of the scratch buffer." The comment at `:551`: "do not
cut inside a two-byte glyph". Upstream hardened `readLine` and `read2Lines`
against over-long lines on purpose (`saveRestoreCalcState.c:985-987`,
`1010-1012`), so a state file is a supported input.

**Bug class.** The same class as G2R1-2: a byte-wise guard on glyph-wise
content. Bit 7 does not separate lead from trail. The only correct way to
find a glyph boundary from the end is a forward walk (`stringLastGlyph`,
`charString.c:453-481`, U3 in §4.2).

**Class-level test.** The two-byte glyphs whose trail byte has bit 7 set are
enumerable from `fonts.h`. For each, build a register string of 2,557 bytes
plus that glyph and cut it through `pgStringCut` at width 399. Assert that
`tmpString` ends with a whole glyph or before it. Assert that
`stringWidth(tmpString)` equals the width of the same bytes with a zeroed
tail.

### G2R1-10 — Pin D17b claims a property the code does not have, plants a canary it never reads, and the guard it documents can be deleted with the pin green

**Where.** `pgmGraphics.c:1175-1182` and `design-docs/program-graphics/
TESTING.md:70`. Three verifiers from three dimensions confirmed three
faces of the same pin.

**Face 1: the row's claim is false.** The row reads "a string that ends in a
lone lead byte is trimmed to a width of one pixel without a read beyond its
NUL." The first call inside the trim loop is `stringWidth(tmpString)`.
`_calculateStringWidth` (`charString.c:250-254`) reads `str[ch++]` after
every lead byte with no NUL check. It takes `[9]` (the NUL) as the second
byte of glyph `0x8000`. It then walks `[10]` and `[11]` (the two `'Y'` the
pin planted) and stops at the NUL the pin put at `[12]`. Only after that does
the guarded walk (`:563`) see the lone lead byte. Measured: `stringWidth` of
the same ten bytes with `'YY'` after the NUL is 114, with zeros 94, and a
probe assertion on that difference turned the gate red.

**Face 2: the canary is never read.** With the guard at `:563` deleted, the
trim loop walks `i = 8 → 10 → 11 → 12`. Pass 1 writes `tmpString[11] = 0`.
Pass 2 writes `[10] = 0`. Pass 3 stops at the now-NUL `[10]` and writes
`[8] = 0`. The string is `"ABCDEFGH"`, trimmed to empty, and the only
assertion (`tmpString[0] == 0`, `:1182`) passes. Measured: gate GREEN with
the guard removed, no hang. The assertion that reds it exists and is not
written: after the cut, `tmpString[10] == 'Y' && tmpString[11] == 'Y'`.
Measured RED without the guard, GREEN with it.

**Face 3: the no-mutation reason is false.** TESTING.md:70 and
DESIGN-HISTORY.md:262 say "None. The failure of the guard is a hang". Each
outer pass moves the written NUL backward and the pin's own `[12] = 0`
bounds the walk, so the loop always terminates on this fixture.

**Why wrong.** TESTING.md §2 rule 2: "A pin that cannot be made red by any
mutation is not a pin." The production guard at `:560-569` is present and
correct. Only the pin is inert, and the row records a property of the whole
call that only the trim walk has. Severity latent.

**Bug class.** Oracle where the mechanism cannot reach. A canary planted and
not read. Message/body mismatch.

**Class-level test.** The canary assertion of face 2, plus the over-read
oracle of face 1 once the trim runs before the measure (the fix of G2R1-2).

### G2R1-11 — The `DISP` band clear has no pin. Every `DISP` in the battery runs on a freshly erased band

**Where.** `pgmGraphics.c:609`, `lcd_fill_rect(c.x0, row, c.x1 - c.x0 + 1,
20, LCD_SET_VALUE)`.

**Measured.** The clear disabled (`if(0) lcd_fill_rect(...)`), refreshed,
solo gate at the tip: GREEN, 0 failed, S1 stayed at exactly 10,500. The
probe reached the compiled shadow file. Reverted.

**Why the battery is blind.** D11 (`:1057`) runs after `fnErase`. D15
(`:1125`) lights only x 5..15 on row 49, outside the clip columns 200..399,
so nothing inside the band-and-clip intersection is lit before the call. S1
issues `DISP 1` right after `fnPview(6)` and `fnPview(2)`, and `fnPview`
fills the canvas rows. `showGlyphCode` (`screen.c:1238`) pre-clears only
each glyph's own cell, so without line 609 a shorter second `DISP` on the
same line leaves the tail of the first string standing. D15's comment
claims "DISP clears and writes only between the clip columns" and its body
observes only the write and the survival of a pixel outside the clip.

**Why wrong.** DESIGN.md §4.6, `DISP` pseudocode: "clear the band rows
row..row+19, cols clipX0..clipX1 to white" before `showString`.

**Bug class.** A pin on a pre-satisfied state. Message/body mismatch.

**Class-level test.** Light a pixel inside the band and inside the clip
before `DISP`, and one inside the band outside the clip. `DISP` a shorter
string. Assert the first is white and the second survives.

### G2R1-12 — D8's "the refused command drew" half reads a row the refused command never targets

**Where.** `pgmGraphics.c:995`. The fixture writes `X=0, Y=30, Z=5000, T=30`
and lights row 30 end to end (`:986-989`), then writes only `Z=40000`
(`:991`) and reads pixel `(200, 31)`, buffer row 208. The refused `LINE`,
with or without its limit check, targets only row 209 (y = 30), which the
first line already lit.

**Measured.** Mutation A, limit check disabled: red only through "D8 a
coordinate of 40000 did not raise ERROR_OUT_OF_RANGE". Mutation B, the
finding's hypothetical defect (`pgError` raised and the value still
returned with true): gate GREEN. The "draws nothing" half cannot fire from
this fixture. The red-first table (DESIGN-HISTORY.md:185-193) records no run
of "skip the limit check" at all.

**Why wrong.** TESTING.md §4 D8: "a coordinate of 40000 raises
ERROR_OUT_OF_RANGE and draws nothing." §2 rule 2. The pin as a whole passes
rule 2 through its error-code half, so the severity is latent.

**Bug class.** Dead assertion: an oracle on an untargeted row.

**Class-level test.** Place the refused line on a fresh row (`Y = T = 31`)
and read a pixel on that row.

### G2R1-13 — K7's band-clear assertion is a conjunction with the flag. A refresh that resets the flag without clearing the band passes

**Where.** `pgmGraphics.c:873`: `lcd_buffer_pixel_on(1, PG_TOP_ROW + 10) &&
canvas.errorShown`. G1 fix-wave code (`c452d8796`).

**Measured.** The fill at `:91` removed and the flag reset kept: gate GREEN,
0 failed. The same build with the assertion in its positive form (pixel
only): RED, "K7 the error band was not cleared". Pixel (1,30) is lit after
the error paint (the `O` glyph's row 6 is `0xC3` with `colsBeforeGlyph` 0),
stays lit under the mutation, and the pin passes only because the flag half
is false. Reverted.

**Why wrong.** DESIGN.md §3.6, line 159: "When the error is gone, the next
refresh clears the band again." DESIGN-HISTORY.md:229 relies on K7 as the
proof that the refresh "clears both together". TESTING.md's one named
mutation for K7 targets the `refreshRegisterLine` guard, the Z-line half,
not the band clear.

**Bug class.** Assertion after the epilogue reset. Message/body mismatch.

**Class-level test.** Two assertions: pixel (1,30) clear after the EXIT
press, and separately `errorShown == 0`.

### G2R1-14 — K1 checks a compile-time constant. No pin presses a softkey, so the three softkey range clauses are unpinned

**Where.** `pgmGraphics.c:793-796`: `if(!(CM_GRAPHICS_CANVAS >= 19 &&
CM_GRAPHICS_CANVAS <= 23))` with `CM_GRAPHICS_CANVAS` a `#define` of 21. G1
code. No driver in the `TESTSUITE_BUILD` block calls `btnFn*`,
`executeFunction` or `btnFnClicked`.

**Measured.** The range clause removed from `executeFunction`
(`keyboard.c:940`), refreshed into the patch, rebuilt: `13022 TESTS PASSED`,
K1 included. Reverted.

**Why wrong.** TESTING.md §4, line 48: "A softkey press in the view changes
nothing | Remove the range clause from one of the three softkey functions."
§2 rules 2 and 3 ("A pin drives the real gesture"). The G1 round-1 report,
§6.4, already named the tautology, and the G1 fix wave left it without a
ruling. The pretty-print-extra sibling carries the same shape (FV19) with a
note that `executeFunction` is unreachable from the test binary. That note
is a sibling's comment, not a program-graphics ruling, and the G2R1-3 probe
drove `btnFnPressed`/`btnFnReleased` from the test binary without trouble.

**Bug class.** A compile-time oracle for a runtime property.

**Class-level test.** Drive `btnFnPressed`/`btnFnReleased` in mode 21 with a
P.FN softkey, as the AIM probe did, and assert `calcMode`, one canvas pixel
and the stack unchanged. Red when any of the three range clauses is removed.

### G2R1-15 — DESIGN.md §5.1 says a long integer coordinate errors only above 31 bits. The code, pin D8 and DESIGN-HISTORY all say 32767

**Where.** `design-docs/program-graphics/DESIGN.md:303` against
`pgmGraphics.c:368-371` (`if(low > 32767u)`), TESTING.md:59 (D8) and
DESIGN-HISTORY.md:165 ("refuses a value above 32767"). Two verifiers
confirmed it from two dimensions.

**Reaching input.** `LINE` with a long integer 40000 in Z raises
`ERROR_OUT_OF_RANGE`. By the sentence at line 303, 40000 fits in 31 bits and
is accepted and clipped. `git log -L` shows both the sentence and the check
landed in `294f470d2`: a spec error at birth, not a later drift. The real row
in the same table (line 304) gives the 32768 limit, so the long-integer row
is inconsistent inside its own table. The G0 text read "clipped to
−32768..32767".

**Cost.** A G3 implementer who trusts §5.1 sizes the window arithmetic for
31-bit pixels while the kernel is built for 15-bit ones (§4.5 sizes the
64-bit root for radius 32767). A local model implements from this document
(CLAUDE.md).

**Bug class.** Doc and code born in the same commit disagree.

**Class-level test.** None for the code: D8 pins it. The doc line changes.

### G2R1-16 — DESIGN.md specifies the filled rectangle three different ways, all under the dead name `RECT`

**Where.** DESIGN.md:219 (§4.1: "`RECT` in modes 0 and 1 calls `pgRun` per
row"), :251-253 (§4.4: "`RECT` clamps the rectangle to the clip rectangle and
calls `lcd_fill_rect` once in modes 0 and 1. In mode 2 it calls `pgRun` per
row."), :387 (§8.3: "Horizontal runs use `lcd_fill_rect`"). DESIGN.md:43 and
DESIGN-HISTORY.md:153-154 say `RECT` was replaced by `FBOX`. TESTING.md:94
still says `RECT`.

**The code.** `pgBox` (`:208-218`, from `fnGfbox :455-462`) calls `pgRun` per
row in every mode and never `lcd_fill_rect` (the only `lcd_fill_rect` sites
are ERASE, PVIEW, DISP and the tests). It matches §4.1 and contradicts §4.4
and §8.3. `lcd_fill_rect` drops the whole call when any edge is off screen
(`c47-gtk/hal/lcd.c:184-191`, `testSuite/hal/lcd.c:62-64`).

**Reaching input.** Not a runtime path. A reader of §8.3 who routes every
horizontal run through `lcd_fill_rect` loses the `LINE` from column −20 of
pin D8c whole. A reader of §4.4 clamps first and does not, which narrows the
runtime story and not the contradiction. The document's own rule (lines
3-9): one fact per sentence, no unstated decision.

**Bug class.** Rule corrected in a subset of its copies.

**Class-level test.** None for the code: D4, D9 and D13 pin `pgBox`. The
three sentences and the TESTING.md name change.

### G2R1-17 — Two readers of `canvas.region` disagree with the design text after a plot step abandons the view

**Where.** `pgmGraphics.c:56` (`fnErase` tests `calcMode !=
CM_GRAPHICS_CANVAS`) against DESIGN.md:168-174 (§3.7: `if canvas.region ==
0: fnPview(2); return`). `pgmGraphics.c:626` (`fnGclip` reads
`canvas.region` in any mode) against DESIGN.md:164 (§3.6: "every reader of
it runs in mode 21 only").

**Reaching input.** `PVIEW 6`, a plot step (`Draw`, `PLTf`, `PLSTAT`), then
`ERASE`. `fnPlotSQ` (`graphs.c:322`) and `fnPlotStat` (`plotstat.c:1992`)
set `CM_GRAPH`/`CM_PLOT_STAT`. Nothing clears `canvas.region` (its only
writers are `pgSetRegion` and `pgCloseView`, and `pgCloseView` runs only
from the mode-21 EXIT arm). The run loop calls no `refreshScreen` between
steps, so the plot mode is in force at the `ERASE` step. The code re-opens
the view over region 2 with `prevCalcMode = CM_GRAPH`, so the next EXIT lands
in the plot view. Under the §3.7 pseudocode, `ERASE` clears rows 20 to 239
under the plot mode without entering mode 21. The stop's `refreshScreen(4)`
then repaints the plot over it. The code's choice is the better one. It is not the
one the authority states. `fnGclip`'s read has no visible effect: `pgClipNow`
ignores the stored clip outside mode 21 and `pgSetRegion` resets it on
every entry.

**Bug class.** Two predicates for "the view is open", one in the authority
and one in the code.

**Class-level test.** `PVIEW 6`, a plot step, `ERASE`: assert `calcMode 21`
and `prevCalcMode CM_GRAPH`. Then EXIT: assert `CM_GRAPH`. The fix is an
amendment of §3.7 and §3.6, not a code change.

### G2R1-18 — `GMODE n` with `n > 2` raises `ERROR_OUT_OF_RANGE`. DESIGN.md §4.7 says `ERROR_INVALID_DATA_TYPE_FOR_OP` (split verdict)

**Where.** `pgmGraphics.c:615-620` against DESIGN.md:293.

**The split.** Five dimensions raised this claim. One verifier (intent lens,
arithmetic dimension) found no ruling and confirmed it. Four verifiers
(contracts, errorpaths, guards, tests) refuted it on two facts that the
confirming verifier did not reach. First, DESIGN-HISTORY.md:77-78, stage G1
item 1: "PVIEW with a bad parameter raises ERROR_OUT_OF_RANGE, not the
data-type error. The parameter is a number, and the number is out of
range." DESIGN.md:349 puts `PVIEW`, `DISP` and `GMODE` on the same
step-parameter mechanism. The G1 commit `cb2ae56e7` rewrote the two `PVIEW`
sentences and did not sweep §4.7. The confirming verifier grepped for
"gmode" only. Second, no product input reaches line 617. The item row caps
TAM at 2 (`tam.c:742, 874`). `indirectAddressing` refuses a value above 2
with `ERROR_OUT_OF_RANGE` before the call (`registers.c:1611, 1627, 1725`).
`_executeOp`'s `PARAM_NUMBER_8` arm drops a stored byte above `tamMax` into
a diagnostic `sprintf` without calling the function (`lblGtoXeq.c:426-440`).
The combined build's `paramCoreValidateDirect` enforces the same. Only the
test harness can call `fnGmode(3)`.

**Net.** A code defect, refuted four to one. A doc residue, real: the
authority contradicts its own G1 ruling in one sentence, and a local model
implements from that sentence. Kept here, last, on that ground. The
disposition is settled by the ruling: DESIGN.md:293 changes to
`ERROR_OUT_OF_RANGE`, the code stays.

**Bug class.** A G0 leftover outside the sweep of the ruling that replaced
it.

**Class-level test.** None. The item row's max is the guard, and the
in-function branch is dead defensive code.

---

## 4. PLAUSIBLE findings

### 4.1 Survived refutation without a reaching input

None. Every finding in §3 carries a concrete input, a mutation, or both.

### 4.2 Beyond the verification cap

Ten claims received no three-lens verdict. They are listed as unverified,
with what settles each.

| id | claim | source | what settles it |
|---|---|---|---|
| U1 | `TEXTOUT` erases the drawing under every glyph cell (`showGlyphCode` pre-clears the 20-row cell with `LCD_SET_VALUE`, `screen.c:1239`) and DESIGN.md §4.6 and §10 do not state it. A label over a drawing punches holes in it. `showGlyphCode` offers `noPreClear`. | design finder | A ruling: is the erase the intended overlay? If yes, one sentence in §10. If no, the `noPreClear` path and a pin that draws a line, then a label over it, and asserts the line survives outside the glyph ink. |
| U2 | `010-keyboard.c.patch:122` rewrites `keyboard.c:3410` in place (`= calcMode;` → `= pgEffectiveCalcMode();`), where upstream's own additive override shape sits two lines below (`effectiveCalcMode = CM_NORMAL` under GRAPHMODE). An added `if(calcMode == CM_GRAPHICS_CANVAS && programRunStop == PGM_RUNNING) effectiveCalcMode = CM_NORMAL;` is behaviour-identical and touches no upstream line. | upstream finder | A read of both shapes under `pkg_patch_refresh.py --rebase-base` against an upstream edit of line 3410. Pin K5 covers the runtime. |
| U3 | `pgStringCut` lines 558-569 re-implement `stringLastGlyph` (`charString.c:453-481`), including its clamp of a trailing lone lead byte (`:468-470`), which is the case D17b had to add by hand. The cap back-up (`:547-549`) re-derives the two-byte rule instead of `stringPrevGlyph`. Three defects (Gemini 1, G2R1-2, G2R1-9) came from this private copy. | upstream finder | A trace that `stringLastGlyph` gives the same cut for the D17, D17b and G2R1-9 fixtures. If it does, the copy goes. |
| U4 | The row stride is the literal 52 (`PG_ROW_BYTES`) while `hal/lcd.h:34` exports `LCD_LINE_SIZE` (50). The simulator's own `lcd.c` mixes both spellings. If upstream changes `LCD_LINE_SIZE`, the package and D10 diverge from `bitblt24` without a compile-time signal. Low. | upstream finder | `#define PG_ROW_BYTES (LCD_LINE_SIZE + 2)` and a green D10. A design note, not a defect today. |
| U5 | `TEXTOUT` and `DISP` overwrite `tmpString`; if the runner keeps state there across a step, the runner crashes or sticks. | Gemini 2 | Refuted by the operator: upstream item functions write `tmpString` inside a step (`stringFuncs.c:290-305`, `factorial.c:21`, `timer.c`) and the runner writes it fresh when it shows a step (`nextStep.c:280`). The contracts finder concurred by trace. A three-lens verdict settles it. |
| U6 | `pgError` paints the error and never sets `canvas.errorShown`. | Gemini 5 | Refuted by the operator: `pgError` only raises `lastErrorCode`; `pgRefreshCanvasView` (`:84-93`) sets the flag when it paints and clears both together, pin K7. The contracts, lifecycle and errorpaths finders concurred by trace. A three-lens verdict settles it. |
| U7 | An interactive `ENTER` press in the view paints the T register line over canvas rows 20 to 59: `case ITM_ENTER` sits above the guard arm and leaves `keyActionProcessed` false for mode 21, so `btnPressed` calls `showFunctionName(ITM_ENTER)` and the release calls `hideFunctionName` → `refreshRegisterLineRestoreT` → `_refreshRegisterLine(REGISTER_T, RESTORE_T)`, below the guarded wrapper. Pin K2 drives `processKeyAction` and `runFunction` and never the two paint calls. | in-family lifecycle finder on the G1 range | **A read at the tip supports the mechanism.** The `ITM_ENTER` case (mirror `keyboard.c:2596-2648`) sets `keyActionProcessed` only for `CM_ASSIGN`, the four browsers, `CM_CONFIRMATION`, `tam.mode` and `CM_NIM`, so mode 21 reaches `break` with it false and `btnPressed` (`:1947-1949`) calls `showFunctionName`. `hideFunctionName` (`screen.c:2174-2183`) calls `refreshRegisterLineRestoreT` (`:5489`), which calls `_refreshRegisterLine` directly. The package guard is on the wrapper `refreshRegisterLine` (`:5486`) only. Not executed. What settles it: a probe in `pgTestKeys` that lights a canvas pixel in rows 20-59, drives `btnPressed`/`btnReleased` with the ENTER key data, and reads the pixel. This is the open item most likely to be the next wrong-result. |
| U8 | The long-press EXIT `SNAP` gesture on the R47 paints `EXIT`, then `SNAP`, then the T line into rows 20-59 before the screenshot. `case ITM_EXIT1` leaves the key unprocessed in mode 21 without an error or a pending `temporaryInformation`, `LongpressKey_handler` paints `SNAP`, the release runs `hideFunctionName`, `refreshScreen(137)` takes the canvas case and repaints nothing, and `fnSNAP` dumps the buffer. | in-family lifecycle finder on the G1 range | The same read supports the first half: the `ITM_EXIT1` case (`:2538-2560`) sets `keyActionProcessed` only for SHOWMODE, `CM_LISTXY`, `CM_PEM`, a pending TI, or an error. On a plain EXIT the T-line paint is invisible because `pgCloseView` repaints everything. The long-press path was not run. What settles it: the U7 probe extended through `execFnTimeout`/`LongpressKey_handler` with `longpressDelayedkey1 = ITM_SNAP`, then a read of the dumped BMP's rows 20-59. |
| U9 | In the combined build, undo-history's `010-keyboard.c.patch:37` adds `(calcMode >= 19 && calcMode <= 23)` to `determineItem`'s shift list, so f and g engage in mode 21 there: `commonShiftProcessing` sets the shift, paints the glyph at `Y_SHIFT` (row 24, inside the canvas), and the next key resolves as `fShifted`/`gShifted`. `f EXIT` (`ITM_OFF`) is swallowed by the guard, a second EXIT closes the view. DESIGN.md §10 limit 8 and the G1 refutation of Gemini 3 are solo-build facts. | in-family lifecycle finder on the G1 range | The combined binary from §2 exists in `wf_8420fae9-eb2-9`. A probe: mode 21, `determineItem` for the f key, assert `shiftF` false and no lit pixel at `Y_SHIFT`. If it fails, §10 limit 8 and §7 item 5 need a build qualifier, or the arm needs `calcMode != CM_GRAPHICS_CANVAS`. No crash on this path. |

---

## 5. Design observations

1. **The closed view is a second surface.** The ten commands share one
   epilogue, `pgRefreshMaybe`. That epilogue is the natural site for the
   flag protocol: set the three manual bits and `screenHoldsDrawnPixels`
   when `calcMode != CM_GRAPHICS_CANVAS`, as upstream's four drawing
   commands do. Upstream-convention-first applies. One helper, ten call
   sites, and the class test of G2R1-1 pins it.
2. **The string path is one class of defect, four times.** Gemini 1, the
   D17b guard, G2R1-2 and G2R1-9 all come from byte-wise reasoning on
   glyph-wise content. The walk is one that upstream already exports
   (`stringLastGlyph`, `stringPrevGlyph`). The order "measure, then
   normalize" is backwards: the trim of a lone lead byte belongs before the
   first `stringWidth`. Upstream's `_calculateStringWidth` and
   `charCodeFromString` never check the trail byte for NUL, so the package
   owes `showString` a well-formed string. That is a contract worth one
   sentence in §4.6.
3. **Five inert assertions share one shape.** D8's second half, D17b, K1,
   K7 and the missing `DISP` clear pin are the five. Each asserts a positive
   property through a conjunction, on a pre-satisfied state, or on a
   constant. TESTING.md rule 1 ("a pin sets the state it asserts") already
   covers the pre-satisfied cases in spirit. A rule 5 makes it explicit: the
   named mutation must run before the row is written, and the red must come
   from the assertion the row names. The DESIGN-HISTORY red-first tables
   record which mutations ran. D8's "skip the limit check" and K1's softkey
   clause never did.
4. **The authority drifted at the commit that implemented it.** Four
   sentences (the 31-bit limit, `RECT` three times, the §3.7 predicate, the
   §3.6 reader claim) and one G0 leftover (§4.7). DESIGN.md changed 32 lines
   at G2 and 34 at the fix wave, and nobody diffed the document against the
   code. A doc-versus-code pass belongs at each stage close, before the
   audit, because a local model implements from this document.
5. **Two frames ten lines apart.** `pgArc` in the user frame, `pgCircle` in
   the screen frame, both called from `fnGarc`. The empty-clip sentinel
   `(x0 = 1, x1 = 0)` is correct by construction because every consumer
   rejects it through its own range test, and no name carries the rule. A
   `pgClipIsEmpty()` predicate and a `pgArc` that takes the screen row make
   both invariants visible.
6. **Two decisions the code makes and the document does not state.**
   `GMODE` persists across `PVIEW`, `ERASE` and EXIT (the §3.5 pseudocode
   does not reset `drawMode`, RPL precedent, by design). `GCLIP` before
   `PVIEW` is discarded by the documented reset and `GCLIP` with the view
   closed is overridden by the documented whole-screen clip (§4.2 and §3.5
   compose, §6.2). Both are right. Both cost one sentence.
7. **The mode-21 refresh case paints nothing and clears nothing.** It relies
   on painters that clear their own band, and one painter does not for a
   blank base (G2R1-5). The "paint only the status bar" rule needs an
   explicit list of what the case must clear itself.
8. **The upstream surface is small and disciplined.** Thirty hunks, 21
   deletions of which 13 are the sanctioned CAT_FREE fills, zero churn, and
   the three registry lines byte-identical with the siblings. One avoidable
   modified line remains (U2), one duplicated constant (U4), and the
   mid-table softmenu row is ruled. The real drift risk lives in `files/`,
   not in a patch: the private glyph walk (U3).
9. **The fix wave was audited this time, and it held better than the
   pattern predicts.** The memory rule says each round's findings come
   mostly from the previous round's fixes. Here two of eighteen distinct
   findings sit in `0663b2360`'s code (G2R1-9, the cap guard, and G2R1-10,
   pin D17b), one in the G1 fix wave (G2R1-13, K7), and the rest in G2 and
   G1 proper. Whatever wave closes §3 is the last unaudited code of this
   stage under the one-round ruling. Its class tests carry the second look.

---

## 6. Deliberately not flagged

### 6.1 The out-of-family claims

| claim | reader | disposition |
|---|---|---|
| `GCLIP` narrows an unclamped row into `int16`; a later `FBOX` writes before the buffer | Sol 1 | Confirmed and fixed in the wave, pin D13. Pre-verified, not re-reported. |
| `pgIsqrt` and `4·r²` overflow `int32` | Sol 2, Gemini 3 | Confirmed by both, fixed, pin D14. Pre-verified. |
| `DISP` ignores the clip columns | Sol 3, Gemini 4 | Confirmed by both, fixed, pin D15. Pre-verified. |
| Arc vectors at 1024 collapse spans under 0.056° | Sol 4 | Confirmed, fixed, pin D16. Pre-verified. The residual band under 360° is G2R1-4, a different defect. |
| The string cap cuts inside a glyph and the trim walks over the NUL | Gemini 1, Sol as a named gap | Confirmed, fixed, pins D17 and D17b. Pre-verified. The guard's lead/trail confusion is G2R1-9 and the pin's inertness is G2R1-10, both new. |
| `pgReadAngle` accepts NaN and infinity | Sol as a named gap | Confirmed, fixed, pin D18. Pre-verified. |
| `TEXTOUT`/`DISP` corrupt `tmpString` for the runner | Gemini 2 | Refuted by the operator. The contracts finder concurred by trace ("written and consumed inside one step"). No three-lens verdict: U5. |
| `pgError` leaves `errorShown` stale | Gemini 5 | Refuted by the operator. The contracts, lifecycle and errorpaths finders concurred by trace. No three-lens verdict: U6. |
| Sol's question on stale limb bytes | Sol | Answered: exact-size allocation, pre-verified. |

Sol's cleared list stands. It covers the mirrored mapping, the partial-byte
masks, run and box clipping, the Bresenham stepper and the r = 1..3 circle
boundaries. It also covers negative arc spans (0° to −90° is a 270°
counterclockwise arc) and the documented exclusions. Gemini's cleared list
stands too. It covers `TEXTOUT` at the right clip edge (the loop empties the
string), an inverted clip rectangle (now the sentinel), and `DISP` for lines
8 to 11 in region 2. It also covers `DISP` with the view closed, a
zero-length limb area, mixed angle types in `ARC` (each unboxed to its own
`real_t`), and the uptime wrap in `pgRefreshMaybe`.

### 6.2 Refuted by the refutation pass

| claim | why cleared |
|---|---|
| `GMODE` code defect (four refutations) | The branch has no product path and the G1 ruling decides the error name. See G2R1-18 for the doc residue that remains. |
| After a `VIEW`, `AVIEW` or `PROMPT` step inside the view, the first EXIT press is swallowed | Documented and upstream-consistent. DESIGN.md §3.6, row "VIEW, AVIEW inside the view": "On EXIT, temporaryInformation is reset, so nothing shows later" describes that press. The G1 round-1 report §6.2 cleared it first ("Same count of presses as upstream"). Upstream keeps `TI_VIEW_REGISTER` through STOP on purpose (the JM comment in `_view`, `display.c:3986-3996`), and in `CM_NORMAL` the same program costs the same two presses. PLAN §9 item 4 records option A. |
| `TEXTOUT` and `DISP` raise the type error for a non-string only when the cell is inside the clip | The position-first order is the specified order. DESIGN.md §4.6's `DISP` pseudocode returns on the clip test before `showString`, and assigns `TEXTOUT` the same shape ("the same way"). That order has stood since `294f470d2`, the same commit as the code. §5.1 governs coordinate registers ("reads each coordinate register by type"), and the coordinates are read before the clip test in both commands (pin D12). The string in Z or X is not a coordinate. |
| `canvas.errorShown` is never reset by `PVIEW`, `ERASE` or `pgCloseView`; a clear of `lastErrorCode` without a mode-21 refresh erases rows 20-39 of a later drawing | The only candidate path fails at its second step. In the combined build the f press reaches `commonShiftProcessing`, which sets `shiftKeyClearsError` before it clears the code (`keyboard.c:1494-1500`). On the release, `preventRefreshAtTheEndOfReleasedKey` is false because of that flag (`:2321-2329`), `refreshScreen(117)` runs, and the mode-21 case clears the band and the flag in the same key cycle. Every other clear of `lastErrorCode` reachable in mode 21 is followed by a mode-21 refresh (`processKeyAction:2375`, the EXIT arm `:2539`, and the `FLAG_IGN1ER` clear happens before any refresh). The flag is stale after `pgCloseView`, and every re-entry runs `pgSetRegion`, which clears the band anyway. Pin K7 covers the paint-then-clear cycle. |
| `GCLIP` with the view closed stores a clip that nothing reads, and the doc does not say so | The behaviour is the composition of two rules DESIGN.md states without exception: §4.2 (the clip is the whole screen while the view is closed) and §3.5 (`PVIEW` resets the clip to the region). The G2 contracts packet asked the outside reader to walk "GCLIP with the view closed" and it raised nothing. A request for a third sentence that names `GCLIP` is a redundancy request, not an unstated decision. Noted in §5 item 6. |

### 6.3 Cleared by the in-family finders

**The kernel.** `pgRun` (`:153-182`): the mirrored positions `a = 399 −
col1 ≤ b = 399 − col0`, the head mask `0xFF << (a & 7)`, the tail mask
`0xFF >> (7 − (b & 7))`, and the same-byte AND of both. The masks were
checked by hand for `a & 7 = 3, b & 7 = 5` (`0x38`) and for the multi-byte
split. Both clamps and
the swap run before any byte is touched, and the sentinel (1,0) makes `col0
> col1` and returns. The dirty flag is set as `bitblt24` does (D10).
`pgPixel` (`:142-150`): all four clip tests precede the write, each with a
falsifying input (column −1 or 400, row 19, row 171 in region 2). `pgLine`
(`:185-206`): Bresenham with inclusive endpoints, `dx ≤ 65534`, `err` and
`2·err` far inside `int32`, the horizontal case through `pgRun`, the
vertical case through the stepper, at most 65,535 steps. `pgBox`
(`:208-232`): outline rows `y0+1..y1−1` and filled rows clamped to the clip,
the degenerate `y0 == y1` and `x0 == x1` cases draw each pixel once, and the
sentinel yields one no-op row. `pgIsqrt` (`:236-243`): `4·32767²` is below
`2^33` and `(r + bit)²` stays below `2^61` for that input, so `int64` never
overflows. `pgCircle` fill (`:249-258`): `dy0`/`dy1` clamped to the clip
rows, an empty loop for a disc wholly above or below, `rr ≥ 0` inside the
range, `w ≤ 32767`, `cx − w ≥ −65534` (D14). `pgCircle` outline
(`:260-285`): the midpoint stepper checked for r = 0, 1, 2, 3, terminates at
`x < y`, `r < 0` made positive. `pgInSpan` and the wide/narrow split
(`:290-297`, `:532`): a span of exactly 180° gives `cross == 0, dot < 0`, the
narrow arm, and the inclusive half plane. Direction vectors of ±65536 fit
`int32` and the cross products are `int64`. `a2 < a1` gives the long
counterclockwise arc as §2.2 states. `fnGarc`'s full-circle test uses
`|a2 − a1|` in degrees: for `a1 = 0, a2 = −370` it draws a full circle where
a strict reading of "counterclockwise from a1 to a2" gives 350°. Ambiguous
in §2.2, RPL reads `|θ2 − θ1| ≥ 2π` the same way, not flagged.
`pgArc`/`pgArcPoint` (`:302-342`): `cyUser + dy ≤ 65534` before `PG_ROW_OF`.

**The readers.** `pgReadCoord` long integer (`:356-379`): little-endian limb
read, bytes 4 and up checked for zero (so 8-byte limbs on the simulator are
covered), `LI_ZERO` with zero limbs gives 0, 32768 refused for both signs
(the magnitude rule), sign applied from the tag, exact allocation
pre-verified. `pgReadCoord` real (`:380-393`): NaN, infinity and `|x| ≥
32768` refused before the conversion, and `real34ToInt32` is
`decQuadToInt32` with `DEC_ROUND_DOWN`, the toward-zero rounding §5.1
promises. `pgReadAngle` (`:401-413`): NaN and infinity refused (D18), the
long-integer path through `convertLongIntegerRegisterToReal`. `fnGarc` reads
angles in `currentAngularMode` and ignores a register's own angle tag:
upstream `SIN` honours the tag, but §4.5 says "in the current angular mode",
a design choice. The complex parts are range-checked strictly inside ±32768
before `real34ToInt32`. `pgRefreshMaybe` (`:416-424`): the unsigned uptime
subtraction is wrap-safe and `lastRefreshMs = 0` at boot refreshes at once.
The `int16_t`/`uint32_t` compare at `:557` (the §2 warning): a wrapped
negative width promotes to a large unsigned value and keeps trimming, and a
wrap to a small positive value needs 65,536 pixels, above the 46,044
maximum. The loop cannot under-trim.

**The commands.** `fnGtextout` (`:576-588`): x in `[x0, x1]` and `row..row +
19` inside the clip before any cast. The standard font is at most 20 rows
and 18 columns (measured over all 711 glyphs, min = max = 20 rows), so the
cell test and the glyph pre-clear stay inside the clip, and
`STANDARD_FONT_HEIGHT` 22 is a font-browser pitch, not a mismatch. The cut
width `c.x1 − x + 1 ≥ 1`. `stringWidth` and `showString` use the same
metrics with leading and ending columns, so the cut string fits. Text that
starts left of the clip is dropped, which §4.6 does not forbid. `fnGdisp`
(`:591-613`): lines 1..11 only, rows 20..220 with `row + 19 ≤ 239`, region 2
lines 8-11 dropped whole (documented), the band fill from `c.x0` with width
≤ 400, `col = max(1, x0)` per §4.6, `col > x1` returns, the sentinel clip
returns before `lcd_fill_rect`, `pgStringCut` runs before the fill so a type
error clears nothing (D15). `fnGmode` (`:615-621`): only 0..2 reaches the
store, text ignores it (limit 10.4). `fnGclip` (`:624-644`): swap, four-sided
intersection with the region, the sentinel (1, 0, 20, 20) when empty, only
clamped values reach the `int16` stores (D13), region 2 with a
softmenu-only rectangle gives `r0 = 171 > r1 = 170` and the sentinel.
`pgClipNow` (`:120-127`): the full screen outside the view is §4.2 by
design. `pgSetRegion` (`:25-32`): region 2 fills 151 rows (20..170), region
6 fills 220 rows (20..239). `showString`'s ambient state: every C47 wrapper
restores `combinationFonts` and resets `maxiC`/`miniC`/`compressString`
(`_resetStringMode`), so a raw `showString` with `standardFont` never
enlarges or compresses. `fnPview` (`:36-52`): a bad region refused before any
state change (V3), `prevCalcMode` kept on a second `PVIEW`, the manual bits,
`temporaryInformation` and `screenHoldsDrawnPixels` set as §3.5 says,
`showSoftmenuCurrentPart` paints rows 171-239 only.

**The view's lifecycle.** `pgCloseView`: restores `prevCalcMode`, clears the
region and `temporaryInformation`, forces `SCRUPD_AUTO`, repaints (V6, K3),
and `_selectiveClearScreen` then clears the full screen, so rows 171-239 of a
region-6 canvas are cleared on close even with a blank base menu. The
close is idempotent. The error band: `displayCalcErrorMessage` sets
`SCRUPD_AUTO`, `runProgram` breaks, `refreshScreen(4)` paints the band, and
any next key clears the code and refreshes in mode 21 (K7). The state file
does not save `calcMode` (§3.3). `RESET` as a step: `calcModeNormal` is
guarded, `temporaryInformation = TI_RESET`, matches §10.7. `CLRMOD` as a
step closes the view through `fnKeyExit` twice, which is the command's
meaning. The `calcModeNormal` callers reachable in the view (`CLSTK`,
`_clearAlpha`, `closeAim`, `closeNim`) are all guarded. Number literal steps
have no `calcMode` dependence. `reallyRunFunction`'s `calcMode ==
CM_NORMAL` branches are cosmetics only. Single step runs under
`PGM_RUNNING`, so `pgEffectiveCalcMode` is right there. R/S while running:
`PGM_WAITING` at the press, the release runs nothing, `fnRunProgram` then
stops, the same as `CM_NORMAL`. `ITM_RS` has no case in the main switch, so
it reaches the guard arm as designed. `SNAP` inside the view refreshes
through the mode-21 case, so the canvas stays (G1 report O5). A softkey from
NIM: `executeFunction` closes NIM first, so `prevCalcMode` is never `CM_NIM`.
TAM leaves before `runFunction`, so `fnPview` and `fnGdisp` run with
`tam.mode == 0`. The catalog closes after the item. `PAUSE` inside a running
program does not call `refreshScreen`. `refreshLcd` has no periodic
`refreshScreen`, so no timer paints over the canvas between steps. Plot
abandonment leaves `region` set: documented in §3.6, only G2R1-17's readers
see it.

**The error paths.** Every command reads all its arguments before it touches
the buffer or the canvas state, so a rejected argument changes nothing (D8,
D12, D13, D18). A partial `fnGclip` read failure stores nothing. The commands
are `US_UNCHANGED`, so `reallyRunFunction`'s undo-on-error does not fire.
EXIT with an error pending clears the code, the guarded
`refreshRegisterLine` returns, `refreshScreen(139)` clears the band and
`errorShown`, and `keyActionProcessed` stops the release. Any other key with
an error pending clears and refreshes in mode 21 before the guard arm. The
reserved-variable-name suffix that upstream appends to an error is raised
only from the equation editor, unreachable as a program step. Every error
message is 46 characters or fewer (the width was not measured). The
program-step parameter path checks direct values against the item max and
indirect values in `indirectAddressing`, so `fnPview`, `fnGdisp` and
`fnGmode` see only in-range values from a program.

**The pins.** D1/D2/D3: endpoint, midpoint and one-beyond pixels on fresh
rows, the true Bresenham midpoint (120,110). D4: the interior (230,70) is
clear before `BOX` and lit after `FBOX`. D5: cardinal points, center clear,
(321,100) beyond r = 20. D6: the 45° point (221,171) is the stepper's `x = y
= 21` pixel and (179,129) is the mirrored pixel that a full circle lights.
D7: the clip rows swap and clamp, a pixel inside and outside on the same
row. D8 first half: (0,30) and (399,30) lit, neighbour rows 208 and 210
clear, the right-clamp mutation crashes before the assertions, recorded.
D8c: −20 clamps to 0, and without the left clamp `byteB = 52` writes the
next row's flag and pixel byte, which (399,99) reads. D9: rows 87-89 clear
before the box, flags 1 on both sides because the suite never clears them,
set-instead-of-invert reds by pixel bytes. D10: two writers on the same
1,000 sites in rows 25..238, the flag bytes compare equal only because the
suite's refresh is a no-op (a harness property, not a false green), the
skip-flag mutation reds. D11: a one-row shift passes (weak, not dead), and
the natural `(line − 1) · 20` mutation reds. D12: (300,200) is clear before
the call and lies on the line that a reader without the type check draws.
D13: all
four outside rectangles reduce to the sentinel, the stored-edge check
catches `int16` narrowing of ±32767, the `FBOX` pixels are on a fresh
erase. D14: 23170 fits `4·r²` at `dy = 0` (2,147,395,600 < `INT32_MAX`), so
the recorded red comes from the 32-bit root, as the history says. 32767
overflows the product. The corners are on a fresh erase. D15: the survivor
pixel (10,190) sits outside both the clip and the left scan. D16: at r =
5000 the stepper keeps `x = 5000` for `dy` 0..4, `sin(3/5000) = 0.034°` and
`sin(4/5000) = 0.046°` fall inside 0.05°, `dy = 5` falls outside, and with
1024 scaling `by = 0` collapses to the single pixel, so (300,133) reds. D17:
`n = TMP_STR_LENGTH − 2` backs to −3 on the 0x80 lead byte, the cap-only
mutation leaves `strlen = TMP_STR_LENGTH − 2`. D18: (230,100) and (200,130)
are clear after D16. S1: an exact count, not an upper bound. V1/V2: the clip
row asserted on the struct, per the recorded lesson that the softmenu
painter owns rows 171-239. V3: `PVIEW 3` passes the TM_VALUE range and
reaches `fnPview`. V4/V5/V7/V8: each has a recorded red. K2: press through
`processKeyAction`, release through `runFunction`, mode, pixel and error
asserted. K3: press then release, mode asserted. K4/K5/K6: state asserted
after the real item call. The `pgTestFail` counter goes to X and the
`RX=LonI:"0"` oracle fails the suite on any nonzero count. `pgRefreshMaybe`
is not testable in the suite (refresh is a no-op), documented in §11.

**The design.** The bit layout and the dirty flag match `lcd.c` and D10.
The literal 52 duplicates the HAL's own literal, which upstream hardcodes in
eight places (U4 keeps the note). `screenHoldsDrawnPixels` set in `fnPview`
and not cleared in `pgCloseView`: `refreshScreen` clears it on the way out
(`screen.c:6066`), so no stale flag reaches `fnSNAP`. The softmenu registry
insertion at 180b shifts rows 181 and up: no code indexes `softmenu[]` by a
literal and `softmenuStack` is not persisted. `ITM_2448..ITM_2459` kept
beside the new names in `items.h`: no `.c` file uses them. The item rows
2448-2462: all `CAT_FNCT`, `SLS_UNCHANGED`, `US_UNCHANGED`, `PVIEW` 2..6,
`DISP` 1..11, `GMODE` 0..2, the duplicated range is the upstream `PAUSE`
convention, `menu_CANVAS` holds 12 items under the 18-slot page. The test
file comment "D1 to D12" is stale after D13-D18: a comment only. `fnGarc`'s
full-turn test converts with `convertAngleFromTo` while the vectors use
`Cvt2RadSinCosTan`: both upstream helpers for the same modes, not traced
further. The DMS borrow edge in the full-turn test is sub-pixel.

**The upstream surface.** The refresh is idempotent. The churn scan is
zero. The three `calcMode < 19 /* package browsers 19-23, claims registry
*/` lines are md5-identical across undo-history, pretty-print-extra and
program-graphics, and the `calcMode >= 20 && calcMode <= 23` arm is
md5-identical with pretty-print-extra. The no-op key cases, the EXIT case
before `CM_TIMER`, the guard arm before SNAP, the R/S release arm, the
`refreshRegisterLine` divert, the `refreshScreen` case (with `last_CM =
calcMode;` matching its neighbours), the `calcModeNormal` guard and the
`CM_GRAPHICS_CANVAS 21` line are additive, each at least three lines from a
sibling hunk. `fnKeyCC`, `fnKeyDotD` and `fnTo_ms` `switch(calcMode)` →
`switch(pgEffectiveCalcMode())` are modified lines with no upstream variable
to override, so inherent. The `items.c` rows are the sanctioned CAT_FREE
fill. `menu_PFN_2`'s `ITM_NULL → -MNU_CANVAS` is one modified line, inherent,
the same shape as `-MNU_PP` and `-MNU_FORTH`. The mid-table row 180b is
ruled in DESIGN.md §6 and DESIGN-HISTORY G1 item 5 (pretty-print-extra owns
the tail, `git apply -3` rejects adjacent insertions). `pgmGraphics.c`
reuses `lcd_fill_rect`, `showString`, `stringWidth`, the WP34S trig,
`convertAngleFromTo`, `real34ToInt32`, the register accessors,
`fnScreenDump` through `_ioFileNameOverride`, `lcd_refresh_dma` under DMCP
per the `screen.c:6016` precedent, and `getUptimeMs`. The fast-path limb
read and the direct buffer writes are ruled (§5.1, §8.1, §4.1 with D10).

### 6.4 Findings to leave alone if the goal is correct code

G2R1-8 (the arc frame): a sentence in §3.4 is enough. The mis-call is
fenced by three pins. G2R1-17: the code's predicate is the better one. The
amendment is the fix. G2R1-15, G2R1-16 and G2R1-18: doc lines, one sitting
each, no code. G2R1-6 needs Stan's ruling before anybody touches it: the
rule is either a G3 rule (then §5.1 gets the stage and the pin waits) or a
G2 rule (then the code and the pin). G2R1-7 can wait for G3, when the
helper is needed anyway, if the source-level pin lands now. Everything else
in §3 is cheap and worth the wave: G2R1-1 to -5 change what the owner sees,
and G2R1-9 to -14 change what the battery can see.

---

## 7. Verdict

Inside the view, the ten commands draw what the design says. The kernel's
masks, clamps, steppers and 64-bit products hold at every boundary that two
outside readers and eight inside readers named, and the fix wave holds
under mutation. Nothing here crashes or loses work.

The closed view is not ready to ship. The owner's first interactive gesture of
the package, `LINE` from the CANVAS menu with no `PVIEW` open, draws a line
that the same key release erases (G2R1-1). That is where it breaks first,
and it breaks against three documents that promise PIXEL's behaviour. The
second break is the string path: `x→α` of a multiple of 256, then `TEXTOUT`,
paints a not-found glyph and the tail of whatever `tmpString` held
(G2R1-2), and the picture changes from run to run. The third is a designed
upstream flow, an assigned `ERASE` pressed from alpha input, which leaves a
blinking cursor eating the canvas (G2R1-3). The rest is a rare arc
(G2R1-4), a cosmetic stale menu (G2R1-5), a rule the code does not enforce
until G3 makes it matter (G2R1-6), two G3 hazards (G2R1-7, -8), one
over-read behind a hand-written state file (G2R1-9), five pins that cannot
see the regressions they name (G2R1-10 to -14), and four doc drifts
(G2R1-15 to -18).

The open item most likely to be the next wrong-result is U7: an `ENTER`
press in the view painting the T register line over the top of the
drawing. It reads as real at the tip, no pin can see it, and the probe that
settles it is the same shape as the one that settled G2R1-3.

Fix-wave order, by cost: G2R1-1 (one epilogue helper, ten sites), G2R1-2
(trim before measure, the `stringLastGlyph` call of U3 if it passes the
three fixtures), G2R1-3 (the AIM prologue, copied from `flagBrowser`),
G2R1-4 (use `d` at the arm), G2R1-5 (one clear in the region-2 arm), then
the five pins, then the doc pass (G2R1-15 to -18 and the §3.4 sentence),
with Stan's ruling on G2R1-6 in the same sitting. Then the U7/U8 probes.

---

## 8. Round and exit state

- **Round:** 1 of the program-graphics G2 audit, subject
  `af7ad934a..0663b2360`, tip `0663b2360` (`pkg: G2 round 1 fix wave — six
  repairs, eight pins, two claims refuted`). This report is the in-family
  leg. The out-of-family leg ran on 2026-09-04 over the G2 commit, and its
  fix wave is the tip.
- **Readers, in-family:** eight finders (contracts, lifecycle, arithmetic,
  errorpaths, guards, tests, design, upstream), then three-lens verifiers
  (reachability, correctness, intent) on every finding. The first in-family
  leg of this package that the platform classifier admitted.
- **Readers, out-of-family:**

  | reader | packet | reply | `MODEL:` | raised | survived refutation |
  |---|---|---|---|---|---|
  | sol / gpt | `design-docs/program-graphics/audit/PACKET_G2_C_kernel_sol.md` | `design-docs/program-graphics/audit/REPLY_G2_C_sol.md` | `MODEL: GPT-5` | 4 numbered, 2 named gaps | 4 of 4 confirmed and fixed (D13, D14, D15, D16). Both gaps confirmed and fixed (D17, D18) |
  | gemini | `design-docs/program-graphics/audit/PACKET_G2_D_contracts_gemini.md` | `design-docs/program-graphics/audit/REPLY_G2_D_gemini.md` | `MODEL: Gemini 3.1 Pro (High)` | 5 | 3 of 5 confirmed and fixed (D14, D15, D17). 2 refuted by the operator with three in-family finders concurring by trace, and **no three-lens verdict** (U5, U6) |

- **Tally:** 24 verifier-confirmed entries → 18 distinct findings in §3 (4
  wrong-result, 8 latent, 6 design-flaw, 0 crash-or-data-loss), one of them
  (G2R1-18) on a 1-to-4 split kept as a doc residue. 8 refuted entries → 5
  distinct claims in §6.2. 10 claims unverified in §4.2, of which U7 reads as
  real at the tip. Both outside families and the in-family leg read the
  actual subject, so the round-1 three-family rule is met and the round
  counter advances to 1.
- **Exit criterion:** not met, and by ruling not applied. CODE_AUDIT.md
  asks for two consecutive clean rounds with one out-of-family, and a real
  finding resets the count. PLAN §10, Stan's ruling of 2026-09-04 evening:
  "one round of bug hunting and fix is enough, then move on." Each stage
  gets one audit round (in-family plus the outside readers), one fix wave
  with red-first pins, and then the next stage starts. The out-of-family
  half of that round and its fix wave are on the tip. This report is the
  in-family half. Its fix wave is not written. Under the ruling, the stage
  advances after one wave for §3, with the ruling G2R1-6 needs, and no
  second round follows unless Stan asks.
- **What the round did not close:** U5 and U6 have no independent verdict.
  U7 and U8 were read at the tip and not executed. U9 was not run on the
  combined binary that §2 built.
- **The fix wave is unaudited code, again.** Whatever wave closes §3 is the
  last unaudited code of this stage under the ruling. The class tests in
  §3 are what stands in for the second look.
- **Housekeeping:** every verifier left its worktree clean at `0663b2360`
  (probes reverted by inverse edit, generated files re-refreshed, build
  byproducts restored or removed). The two gate runs of §2 left
  `wf_8420fae9-eb2-10` and `wf_8420fae9-eb2-9` clean after their showcase
  BMPs were removed. This pass wrote only this file. The working tree is on
  `program-graphics/stage-g0` at the tip with unrelated forum modifications
  that are not this audit's. Filename cut after the tip per the
  round-9/round-10 convention.
