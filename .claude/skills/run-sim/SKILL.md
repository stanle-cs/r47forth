---
name: run-sim
description: Launch the C47/R47 simulator and capture what is actually on the LCD. Use when asked to run the app, show a screen, screenshot the calculator, or confirm a UI change renders for real rather than only passing a test. Covers the GTK window, the headless binary, the app's own BMP screenshot path, and the traps that make a naive attempt segfault or silently measure nothing.
---

# Running the C47/R47 simulator and seeing the screen

Two binaries are built from the same sources and both matter:

| Binary | Built by | Use it for |
|---|---|---|
| `build.sim/src/c47-gtk/c47` | `./packages/forth-core/build-test.sh` | the app; interactive window, or `--headless` to run the forth self-test battery |
| `build.sim/src/testSuite/testSuite` | same | upstream's own suite under the package overlay |

Build with the gate, never by hand — see AGENTS.md "The gate". A bare
`ninja` compiles the previous patch content.

## Interactive window

```bash
./build.sim/src/c47-gtk/c47
```

A display is present under WSLg (`DISPLAY=:0`, `/tmp/.X11-unix/X0`) and the
window opens. **You cannot drive it**: this box has no `xdotool`, `import`,
`scrot`, `xwd`, `convert`, or `grim`. Launching proves the entrypoint
resolves and nothing more. To *see* a specific screen, use the screenshot
path below instead of trying to click.

Do not `apt-get install` automation tooling to work around this without
asking — the screenshot path already gives a faithful image.

## The screenshot path (this is the one you want)

The calculator screenshots itself. `fnScreenDump(0)`
([src/c47/screen.c:6307](../../../src/c47/screen.c)) writes a 1-bit
400x240 BMP named `YYYYMMDD-HHMMSSnn.bmp` into the current working
directory, built from `lcd_buffer_pixel_on()`. It is the same read-back
upstream pins its plot regressions on (a SHA-256 of a SNAP capture,
`graphs_cov.txt`).

Three facts that make this work headless:

- `lcd_buffer` is filled by the software blitter whether or not a window
  exists. The `headlessMode` guard in
  [src/c47-gtk/hal/lcd.c](../../../src/c47-gtk/hal/lcd.c) skips only
  `gtk_widget_queue_draw_area`.
- `lcd_buffer_pixel_on()` is declared in `src/c47/hal/lcd.h` for every
  non-DMCP build and implemented in **both** HALs (`src/c47-gtk/hal/lcd.c`,
  `src/testSuite/hal/lcd.c`), so it links in either binary.
- `lcd_clear_buf()` is **c47-gtk-only** and absent from the testSuite HAL.
  `packages/forth-core/test_dict_reloc.c` compiles into BOTH binaries, so a
  driver placed there may not call it — you will get an undefined-reference
  link failure in the testSuite build, not the sim. Order your renders and
  assert a relative change instead of clearing.

### Recipe

Start from `references/capture-driver.c` in this skill — the proven
2026-08-03 driver (forum/screenshots/, commit `4022c5657`) with the
save/restore machinery, all three render idioms (PEM listing, FWRD
picker, normal screen), and marker comments for clean removal. Adapt its
fixture; keep the machinery. To hand a capture job to the local
implementer instead, instantiate
`design-docs/forth-core/QWEN_TEMPLATE_LCD_CAPTURE.md` — it wraps the
same driver in execution gates, a numeric blank-frame check (the
implementer cannot see images), marker-anchored removal, and the
post-removal full gate.

1. Append a temporary driver to `packages/forth-core/test_capture.part.h`
   and register it in `packages/forth-core/test_dict_reloc.c` (a forward
   declaration beside the neighbouring `static int test_*(void);` lines, and
   a `fail |= yourDriver();` beside the neighbouring calls).
2. In the driver, reach the state **through a sequence that already works**
   in a landed test. For a softmenu:

   ```c
   extern void showSoftmenu(int16_t menu);
   extern void showSoftmenuCurrentPart(void);
   extern void fnScreenDump(uint16_t unused);

   currentProgramNumber = 1;
   currentStep = beginOfProgramMemory + progLen - 4;   /* closing marker */
   showSoftmenu(-MNU_FORTH);
   softmenuStack[0].firstItem = 0;
   showSoftmenuCurrentPart();
   fnScreenDump(0);
   ```

3. Build and run:

   ```bash
   ./packages/forth-core/build-test.sh --build && ./build.sim/src/c47-gtk/c47 --headless > /tmp/shot.log 2>&1
   ```

4. Convert and look at it (PIL is installed; ImageMagick is not):

   ```bash
   python3 -c "from PIL import Image; im=Image.open('SHOT.bmp').convert('RGB'); im.resize((im.width*2,im.height*2), Image.NEAREST).save('/tmp/shot.png')"
   ```

   **Look at the PNG.** A blank or all-background frame means the draw never
   reached `lcd_buffer` — that is a failure, not a pass.

5. **Revert the driver** — `git checkout -- packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c`
   — delete the BMPs, and re-run the full gate green. A temporary driver
   that survives into a commit is a defect.

## Traps, each one paid for

- **`screenUpdatingMode` can silently blank a normal-screen shot.** An
  earlier battery test leaves it at 7 (manual statusbar/stack/menu), and
  `refreshScreen()` then draws nothing but the date — a technically
  successful dump of an empty frame. Force `screenUpdatingMode =
  SCRUPD_AUTO;` (and `temporaryInformation = TI_NO_INFO;`) before the
  refresh, and save/restore both. Cost a blank-frame debugging round on
  2026-08-03.
- **`refreshScreen()` on hand-built PEM state segfaults.** The full-screen
  redraw walks program memory through `currentStep` /
  `currentLocalStepNumber`; if you set those by hand and they disagree, it
  dereferences its way off the end. Either drive the real entry path that
  establishes them, or redraw only the band you need
  (`showSoftmenuCurrentPart()`) and accept that the rest of the frame holds
  whatever the previous test left there.
- **The self-test battery may run more than once** in a single headless
  invocation (save/restore round-trips re-enter it), so a driver that dumps
  unconditionally writes several identical BMPs. Take the newest, or
  compare them — `cmp -s` — before assuming you captured two different
  moments.
- **`_ioFileNameOverride[]`** (`src/testSuite/hal/io.c`) sets the next dump's
  filename, but check it links in the binary you are running before relying
  on it. Letting the timestamp name the file and picking the newest is
  simpler.
- Pixel counts are **not** stable pins. Upstream owns the font and the cell
  geometry. Assert relative ordering (longer label lights more pixels than
  a shorter one, which lights more than an empty cell), never a literal
  count — see `test_picker_renders_labels` in `test_capture.part.h`.

## Geometry reference

Screen is 400x240. Softkey rows are `y1 = 217 - SOFTMENU_HEIGHT * row` with
`SOFTMENU_HEIGHT` 23 ([src/c47/softmenus.c](../../../src/c47/softmenus.c)),
so the three rows span `y >= 171`.

**Cell borders come from `KEY_X`, not from arithmetic.**
`const int KEY_X[7] = {-1, 66, 133, 200, 267, 333, 400}`
([src/c47/c47.c:32](../../../src/c47/c47.c)) — cell `c` is
`[KEY_X[c], KEY_X[c+1])`, clamping `KEY_X[0]`'s `-1` to 0. Dividing
`SCREEN_WIDTH` by six gives 66 where the real border is 133, and that
one-pixel disagreement puts a neighbouring cell's frame column in the wrong
cell. Cost a debugging round in G4.

**A visible page is 18 items, not 6.** The draw loop is
`for(y=0; y<3; y++) for(x=0; x<6; x++)` guarded by
`x + 6*y + currentFirstItem < numberOfItems`, and `numberOfItems <= 18` is
the renderer's own "fits on one screen" test. A test that pages by 6 changes
the item COUNT on screen, not just which items — so a pixel comparison
between "pages" measures the count, not the paging. Page by 18.

**An empty cell next to a live one is not blank.** A live key draws a dotted
divider down its right-hand edge at `x == KEY_X[n]`, 12 px on alternate
rows — which by the convention above belongs to the NEXT cell's window. An
empty cell further out reads exactly 0. Assert on a cell's interior
(`KEY_X[c]+1` onward) when you mean "no label here", or compare only cells
with no live neighbour.

## Appendix — the implementer's copy (restore after a fresh clone)

`AGENTS.md` is the local model's always-loaded instruction file. **It is
gitignored by upstream policy** (`cf90554fd`, "Ignore AGENTS.md", alongside
`opencode.json`), so it does not survive a clone and this skill is the
tracked source of truth for its screen-capture section. After cloning,
append the block below to `AGENTS.md` verbatim.

<!-- BEGIN AGENTS.md BLOCK -->
## Seeing the screen (added 2026-08-03; only when a packet asks for it)
Do NOT do this on your own initiative. Only when a packet explicitly says
to capture or inspect the LCD.

Capture packets are instantiated from
`design-docs/forth-core/QWEN_TEMPLATE_LCD_CAPTURE.md` and start from the
proven driver at `.claude/skills/run-sim/references/capture-driver.c`;
the packet tells you exactly what to copy and edit. Everything below
still binds.

The calculator screenshots itself. `fnScreenDump(0)` writes a 1-bit
400x240 BMP named `YYYYMMDD-HHMMSSnn.bmp` into the current directory,
built from `lcd_buffer_pixel_on()`. That read-back works headless:
`lcd_buffer` is filled by the software blitter whether or not a window
exists, and the `headlessMode` guard skips only the GTK redraw.

There is NO screenshot or input tooling on this machine — no `xdotool`,
`import`, `scrot`, `convert`. Do not `apt-get` any. Launching the GTK
binary proves nothing you need; use the dump.

Rules, all five binding:

1. **`lcd_clear_buf()` is off limits in `test_dict_reloc.c`.** It exists
   only in the c47-gtk HAL, and that file compiles into the testSuite
   binary too. Calling it gives an undefined-reference LINK failure in a
   binary you were not looking at. Order your renders and assert a
   relative change instead of clearing the buffer.
2. **Never call `refreshScreen()` on state you set by hand.** It walks
   program memory through `currentStep`/`currentLocalStepNumber`, and
   hand-set values that disagree segfault it. Reach the state through a
   sequence copied from a LANDED test, and redraw only what you need
   (`showSoftmenu(-MNU_X); showSoftmenuCurrentPart();`).
3. **A capture driver is temporary and you remove it by its markers.**
   The driver and its registration lines all carry the `TEMP-LCD-CAPTURE`
   token: range-delete the BEGIN/END block in `test_capture.part.h`,
   line-delete the token in `test_dict_reloc.c`, verify
   `git diff --stat` on both files is EMPTY, delete the BMPs, and re-run
   the full gate green before reporting. Never remove it with
   `git checkout`/`git restore` — with uncommitted work in the file that
   destroys the stage (standing rule, 2026-08-03 near-miss). A driver
   left in a commit is a defect of the same class as editing `src/`.
4. **Never assert a literal pixel count.** Upstream owns the font and the
   cell geometry; a legitimate change there must not turn a test red.
   Assert ordering only — longer label lights more pixels than a shorter
   one, which lights more than an empty cell. Copy the shape from
   `test_picker_renders_labels` in `test_capture.part.h`.
5. **A normal-screen shot forces `screenUpdatingMode = SCRUPD_AUTO`
   first** (and `temporaryInformation = TI_NO_INFO`), saving and
   restoring both. An earlier battery test often leaves the mode manual
   (7), and `refreshScreen()` then draws nothing but the date — the dump
   succeeds and the frame is blank.

Convert with PIL (installed; ImageMagick is not):
`python3 -c "from PIL import Image; im=Image.open('X.bmp').convert('RGB'); im.resize((im.width*2,im.height*2), Image.NEAREST).save('/tmp/shot.png')"`

Note the battery can run more than once per headless invocation, so an
unconditional dump writes several identical BMPs. Take the newest.

Geometry: softkey rows are `y1 = 217 - SOFTMENU_HEIGHT * row`,
`SOFTMENU_HEIGHT` 23, so the three rows span `y >= 171`; six cells divide
`SCREEN_WIDTH` 400, so the first cell is `x < SCREEN_WIDTH / 6`.

The architect's fuller version of this recipe, including the interactive
window and the traps behind each rule, is `.claude/skills/run-sim/SKILL.md`.
<!-- END AGENTS.md BLOCK -->

## Traps added 2026-08-04 (README-verification session)

- **`--build` can hand you a stale shadow.** `build-test.sh --build`
  refreshed `files/` but the binary still ran the previous runner — the
  same stamp trap as `build.dmcp5`, now confirmed for `build.sim`. If
  your driver's printf does not appear in the log, the binary predates
  your edit; run the FULL gate before debugging anything else.
- **Silence is not-run, not pass.** A driver (or any moved test group)
  is only proven in by its banner in the log. This session found the
  entire K4 group sitting unreachable inside the suite's `if (fail)`
  verdict branch — landed green, never executed. Check the banner FIRST.
- **Letter case is item identity.** `ITM_X` and `ITM_x` are different
  items (uppercase inserts `X` regardless of alphaCase); superscript
  conversion runs off `nextChar` via `convertItemToSubOrSup`
  (bufferize.c) inside pemAlpha, so priming `nextChar = NC_SUPERSCRIPT`
  before a digit item types the superscript digit.
- **An empty-abort strands the cursor past the closing marker** (the
  E2/E6 pre-move fires once FLAG_ALPHA drops), so a reopen without
  `fnGotoDot` back onto the opening marker silently gives you a LITERAL
  capture, not a Forth one — the driver reads an empty forthTestCapText
  and every later assert lies. Reposition before every reopen.
