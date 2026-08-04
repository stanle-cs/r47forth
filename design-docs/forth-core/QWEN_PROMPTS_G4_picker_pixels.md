# G4 — what the FWRD picker LOOKS like — Part A

Origin: G3 (`00c5cf2d3`) established that the picker's label reaches
`lcd_buffer` and that the read-back works headless. It pinned one cell
with one name. Three rendering properties that a user would notice
immediately are still unpinned:

- that turning the page changes what is drawn, not only what
  `dynmenuGetLabel()` returns;
- that the empty cells of a partial last page are actually empty — the
  same cells whose key-press G1 subcase 5 pins as a refusal;
- that a maximal 14-byte name stays inside its own cell instead of
  bleeding into its neighbour.

**Read `.claude/skills/run-sim/SKILL.md` before you start.** It carries
the read-back facts, the two traps that will otherwise cost you a
session, and the geometry constants this packet uses. This is a
TESTS-ONLY packet: nothing under `packages/forth-core/` except
`test_capture.part.h` and `test_dict_reloc.c` may change. No production
file, no header, no `src/`.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` → `upstream/migrate-2026-08-03`.
   `git status --short` shows only the two `.opencode-*` files.
2. `git log --oneline -1` names `docs: G4 packet — pin what the FWRD
   picker looks like` (this packet's own commit).
3. `grep -c "lcd_buffer_pixel_on" packages/forth-core/test_capture.part.h`
   → exactly 2, at lines 6270 (a mention in G3's header comment) and 6365
   (G3's counting loop). You are adding the third occurrence.
4. `grep -c "test_picker_renders_labels" packages/forth-core/test_dict_reloc.c`
   → exactly 3, at lines 1092 (declaration), 1752 (the `[DEBUG]` printf)
   and 1753 (the call). Both registration anchors below are among them.
5. `ls .claude/skills/run-sim/SKILL.md` → exists.

## The donor — copy it, do not invent

`test_picker_renders_labels` in
`packages/forth-core/test_capture.part.h` is the landed G3 test and the
donor for everything mechanical here: the save/restore block, the
program-fixture bytes (`0x8B 0x1A 0xFD <len>` header, `": <NAME> 1 ;"`
body), `writeTestProgram`/`cleanupTestProgram`, the
`showSoftmenu(-MNU_FORTH)` + `showSoftmenuCurrentPart()` render, and the
pixel-counting loop. Read it first and copy those parts verbatim.

In particular the fixture is built by `writeTestProgram(prog, progLen)`
on a malloc'd byte array — the donor's exact idiom, and it works for any
number of definitions. Do NOT hand-roll `firstFreeProgramByte`,
`freeProgramBytes` or `scanLabelsAndPrograms()`, and do not reach for the
`tp*`/`testProg_t` builders: if your fixture reports the wrong name
count, the cause is the render ordering below, not the byte-array
approach.

**Geometry (do not re-derive):** the softkey band is `y >= 171`. The six
cells divide `SCREEN_WIDTH` (400), so cell `c` (0-based, left to right)
spans `x` in `[c * SCREEN_WIDTH / 6, (c + 1) * SCREEN_WIDTH / 6)`.

Write ONE helper at the top of the new test and use it for every count:

```c
  /* Lit pixels inside softkey cell `cell` (0..5) of the menu band. */
  static int32_t g4CellPixels(int cell) {
    int32_t lit = 0;
    for(uint32_t y = 171; y < SCREEN_HEIGHT; y++) {
      for(uint32_t x = (uint32_t)(cell * (SCREEN_WIDTH / 6));
          x < (uint32_t)((cell + 1) * (SCREEN_WIDTH / 6)); x++) {
        if(lcd_buffer_pixel_on(x, y)) { lit++; }
      }
    }
    return lit;
  }
```

Put it at file scope beside the new test, not nested inside it.

**Never call `lcd_clear_buf()`** — it does not exist in the testSuite
HAL and `test_dict_reloc.c` compiles into that binary too. You would get
an undefined-reference link failure in a binary you were not looking at.
Nothing in this packet needs it: every assertion compares counts taken
from renders that each fully repaint their own cells.

**Never call `refreshScreen()`.** It walks program memory through
`currentStep`/`currentLocalStepNumber` and segfaults on hand-set state.
`showSoftmenuCurrentPart()` is the only draw call this packet uses.

**Never assert a literal pixel count.** Upstream owns the font and the
cell layout; a legitimate change there must not turn this red. Every
assertion below is an ordering or an equality between two counts taken
in the same run.

## Fixtures — two programs, both built the donor's way

**Fixture LONGPAGE — 12 names, six short then six long.** Names sort, and
these are chosen so the sort puts all six short ones on page 1 and all
six long ones on page 2:

- six names `A1`, `A2`, `A3`, `A4`, `A5`, `A6` (2 bytes each);
- six names `B1CCCCCCCCCCCC`, `B2CCCCCCCCCCCC`, … `B6CCCCCCCCCCCC`
  (14 bytes each — the maximum the picker admits).

One definition step per name, in that order.

**Ordering — this is the trap.** `dynamicSoftmenu[22].numItems` is 0 until
a render builds the content: `showSoftmenu(-MNU_FORTH)` +
`showSoftmenuCurrentPart()` is what calls the builder. So writing the
program does NOT populate it. Every subcase runs in exactly this order:

1. build the fixture program (`writeTestProgram`, then set
   `currentProgramNumber = 1` and `currentStep` to the closing marker);
2. render once at the `firstItem` the subcase starts from;
3. **then** assert the expected `numItems`;
4. then measure pixels.

If the count is wrong at step 3 the fixture is wrong — print
`FIXTURE BUG:` and STOP the subcase. Do not adjust anything to make the
number come out. If it reads 0, you asserted before the render.

**Fixture PARTIAL — 8 names**, `N000`..`N007`, one definition step each,
exactly as the donor builds them.

## Subcases — three

**[1] Turning the page changes the picture.** Fixture LONGPAGE.
Render with `softmenuStack[0].firstItem = 0`, assert `numItems == 12`,
then sum `g4CellPixels(c)` over `c = 0..5` into `page1`. Render again
with `firstItem = 6`, sum into `page2`. Assert `page2 > page1` — page 2 holds six 14-byte names against
page 1's six 2-byte names, so it must light strictly more.
PASS line, exact text:
`    [1] PASS: paging changes what is drawn — page 2 lights more than page 1`

**[2] The blank cells of a partial page are blank.** Fixture PARTIAL,
`firstItem = 6`, so indices 6 and 7 are live (cells 0 and 1) and cells
2..5 have no item behind them. Render, assert `numItems == 8`, then:
- assert `g4CellPixels(0) > g4CellPixels(5)` — a live cell against an
  empty one;
- assert `g4CellPixels(2) == g4CellPixels(3)` and
  `g4CellPixels(3) == g4CellPixels(4)` and
  `g4CellPixels(4) == g4CellPixels(5)` — the four empty cells are
  identical to each other, which is what "chrome only, no label" means
  without naming a number.
PASS line:
`    [2] PASS: cells past numItems draw chrome only, all four identical`

**[3] A maximal name stays in its cell.** Build a THIRD fixture: one
definition only, the 14-byte name `ABCDEFGHIJKLMN`. Render at
`firstItem = 0`, assert `numItems == 1`. Cell 0 is the only one with an
item.
- assert `g4CellPixels(0) > g4CellPixels(1)` — the label is drawn;
- assert `g4CellPixels(1) == g4CellPixels(2)` — cell 1 is no different
  from a cell further away, so nothing bled out of cell 0 into it.
PASS line:
`    [3] PASS: a 14-byte name stays inside its own cell`

## Structure and registration

All three subcases live in ONE function
`test_picker_pixel_layout(void)` in
`packages/forth-core/test_capture.part.h`, placed immediately after
`test_picker_renders_labels`'s closing brace, in the same `sc1`/`sc2`/`sc3`
accumulation style (each subcase sets its own `scN`, prints its own PASS,
`fail |= scN`). Restore every global you touch, exactly as the donor does.

Registration in `packages/forth-core/test_dict_reloc.c`, anchored on the
G3 lines (unique strings):

1. After `static int test_picker_renders_labels(void);`
   add `static int test_picker_pixel_layout(void);`
2. After the pair
   ```c
   printf("  [DEBUG] running test_picker_renders_labels...\n");
   fail |= test_picker_renders_labels();
   ```
   add
   ```c
   printf("  [DEBUG] running test_picker_pixel_layout...\n");
   fail |= test_picker_pixel_layout();
   ```

## Gate

```
./packages/forth-core/build-test.sh > /tmp/forth-g4-gate.log 2>&1; echo $?; tail -n 12 /tmp/forth-g4-gate.log
```

Log path is `/tmp/forth-g4-gate.log` for this packet and overrides any
remembered form. Green means exit 0 AND all three `[N] PASS:` lines in
the log. Report the `FORTH ARENA:` line; it must read
`freeRamDelta=128` as before, since this packet adds tests only.

If a subcase goes red, report the exact FAIL line and STOP. Do not
"fix" it by loosening the assertion — an ordering that does not hold is
a finding about the renderer or about your fixture, and either way it
comes back to the architect.

## Commit

One commit, message exactly:

```
forth-core: G4 — pin what the FWRD picker looks like

G3 pinned that one label reaches lcd_buffer. This pins three properties
a user would notice: that turning the page changes the picture, that the
empty cells of a partial last page really are empty — the same cells
whose key-press G1 subcase 5 pins as a refusal — and that a maximal
14-byte name stays inside its own cell instead of bleeding into its
neighbour.

Every assertion is an ordering or an equality between counts taken in
the same run. No literal pixel count appears, because upstream owns the
font and the cell layout.
```

## Part B

NOT in this file. The architect derives the mutations after this lands
green. Do not invent mutations, and do not edit the renderer to "check"
a subcase.
