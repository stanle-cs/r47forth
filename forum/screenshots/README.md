# Showcase screenshots (sim, 2026-08-03)

Four LCD captures for the Forth post, taken in the GTK simulator at the
stage-G tree (base 26ec91634). Real 400x240 framebuffer dumps via the
calculator's own `fnScreenDump`, scaled 2x. Not mockups: every screen
came from the real render path (PEM listing via the F15-3 idiom, picker
via the G3/G4 idiom, normal-mode refresh), driven by a temporary test
driver that was reverted after capture.

Both demo programs are permanently tested in the sim on every gate run:

- `test_showcase_program` builds FDEMO+NATADD with the fixture builder,
  runs FDEMO through the real XEQ engine, and asserts every documented
  result: registers 00-16, flag 10, named variables DEMO and PTR, the
  GLOBAL/IMMEDIATE/FORGET effects, and both XEQ bridge directions.
- `test_savings_program` runs SAVE and asserts the six-period schedule
  in registers 00-05 and 22, the drained counters, and GROW surviving
  as a global word.

The captures reproduce those runs step for step, so what the screens
show is what the tests pin.

## The shots

1. `shot1-pem-listing.png` — PEM listing at the top of FDEMO: the
   program header (813 bytes / 43 steps), `LBL 'FDEMO'`, the opening
   `»FORTH` marker, and the first colon definitions as ordinary program
   steps. Cursor on step 0003.
2. `shot2-picker.png` — the tail of FDEMO (FORGET GONE1, the late
   `: LATER 2 * ;` definition, cursor on the closing `FORTH«`) with the
   FWRD picker open below: 13 words on the softkeys. Long names are
   clamped by the renderer, same as on hardware.
3. `shot3-fdemo-results.png` — normal screen after XEQ 'FDEMO':
   Z=17 (CALLWORD through the global PLUS10), Y=42 (named variable
   DEMO), X=15 (5 SUMDOWN, the recursive sum).
4. `shot4-save-balance.png` — after XEQ 'SAVE': Y=1050 (first period,
   R00), X=1340.095640625 (final balance, R22) from six compounding
   periods at 5% on 1000.
5. `shot5-save-listing.png` — PEM listing at the top of SAVE (262
   bytes / 14 steps): `LBL 'SAVE'`, `»FORTH`, and the GROW/PUT/BUMP/
   TALLY definitions, including the indirect `STO →20` glyph.
6. `shot6-save-picker.png` — SAVE's tail (STEP, the RUN loop, the
   setup lines, cursor on `FORTH«`) with the FWRD picker showing all
   six words, each fitting its softkey cell untruncated.

## Stage K-N shots (sim, 2026-08-05 and 2026-08-10)

Captured with the console capture driver
(`.claude/skills/run-sim/references/console-capture-driver.c`), reverted
after each session. All 400x240 native dumps via `fnScreenDump`.

- `stage-n-1-console-dialogue.png` — console dialogue: `7 SQ .` printing
  49, `1 2 3 .S` printing `<4> 3 2 1 5`, input line mid-composition.
  Dialogue behaviour pinned by the N1-3 tests.
- `stage-n-2-console-rolled.png` — rolled transcript: `: SQ DUP * ;
  GLOBAL`, the result line, `7 SQ .` printing 49. Roll pinned by N1-2.
- `stage-n-3-spill-separator.png` — `.S` on a six-deep stack:
  `<6> 6 5 4 3 | 2 1`, the `|` at the spill boundary. Pinned by the
  `.S` spill test (b5636f6c9).
- `stage-n-4-countdown.png` — `: COUNT-DOWN DUP IF DUP . 1 - RECURSE
  ELSE DROP THEN ;` then `5 COUNT-DOWN` printing `5 4 3 2 1`. The word
  itself was sim-verified against a stack canary the same day.
- `stage-n-5-error-line.png` — the dialogue's error arm: `WRONG` echoed,
  `No such function` under it (N1-3).
- `stage-n-6-history-recall.png` — full screen after
  `forthHistoryRecall(-1)`: the last line back in the input editor, the
  FWRD home row on the softkeys (L1-H, N1-5).
- `stage-m-1-catalog-fwrd-row.png` — CATALOG tree with the FWRD row
  beside FCNS (M1-1).
- `stage-m-2-fwrd-normal-mode.png` — FWRD softmenu in CM_NORMAL, one
  global word (MSHOW) on the softkeys (M1-1).
- `stage-m-4-assign-pending.png` — ASSIGN with a global Forth word
  pending on the key wait (M1-2).

Composites for the stage-N forum post (3-attachment limit): 
`forth2-attach-1-console.png` = n-1 + n-2 + n-3, 
`forth2-attach-2-repl.png` = n-4 + n-5 + n-6, 
`forth2-attach-3-catalog.png` = m-1 + m-2 + m-4. 
Stacked at native scale with 4 px white separators.

## Not yet covered

Simulator captures cannot stand in for the hardware checks the post
needs (R/S interrupting SPIN is hardware-only; forum/DESIGN.md §5 has
the full list). These are illustrations, taken where the sim and the
device provably render alike.

# Undo history screenshots (sim, 2026-08-25)

Three LCD captures for the undo-history post, taken at the stage-u2 tree
(package base faf9d698c). Re-captured 2026-08-25 after the label fix:
unlabeled captures now render `(val)` instead of `-` (the dash was
ambiguous against subtraction's catalog name — Stan's catch, pinned by
battery case B8). Same two marker-block drivers, recovered verbatim from
the session transcript, removed again after capture, full gate green. Same mechanism as the Forth set: real 400x240
framebuffer dumps via `fnScreenDump`, scaled 2x, driven by a temporary
marker-wrapped block inside the package's own `historyTestBrowser` battery
driver, removed after capture with the full gate re-run green. The browser
states shown are the ones the battery pins on every gate run: entry,
selection and cursor placement by B1-B4, the gap flag by ring case R5, the
restore path by B3.

1. `undo-attach-1-history-view.png` — the view after a 13-step session
   (arithmetic, a 2x3 matrix transposed, SLVQ on 1/-3/2, an oversized
   string, a complex number) and one UNDO. Shows the (now) anchor with a
   complex preview, the * cursor on the selected row, a ~ gap mark, the
   SLVQ label, and a [3x2] matrix preview.
2. `undo-attach-2-gap.png` — five levels after an oversized state was
   skipped: numbering stays consecutive and the ~ on the newest level is
   the only trace of the missing state.
3. `undo-attach-3-restored.png` — the normal screen after ENTER on the
   SLVQ level: [3x2 Matrix], 1., -3., 2 back on the stack, the complete
   pre-solver state.

4. `undo-attach-4-sysfl-uhist.png` — added 2026-08-25 on Stan's request:
   the SYSFL picker page that holds UHIST, flag set so the radiobutton
   shows filled. UHIST sits at index 100 of the generated menu_SYSFL
   catalog (113 entries); the shot renders the 18-item page starting at
   90 through the real refreshScreen path. Same marker-block driver
   mechanism, removed after capture, full gate green.

## Pretty-print shots (sim, 2026-08-26, moved into this folder 2026-08-27)

Captures for the pretty-print package announcement, taken in the GTK
simulator during the PP6/PP8/PP16 stages at base `70f8b7db7`. Native
400x240 dumps via the calculator's own `fnScreenDump`, scaled 2x for the
forum. Every screen came from the real render path — the package's own
measure-and-paint engine drawing through `showGlyphCode`, on the real
register-line, browser and equation surfaces. No mockups.

Provenance caveat, stated plainly: unlike the Forth showcase shots above,
these were captured during stage work with temporary drivers, not by the
tests that pin the behaviour. The tests below pin what the shots SHOW;
they are not the capture drivers.

The full 21-capture working set is preserved in `pretty-print-archive/`
because it previously lived only in `/tmp`. The forum limit is three
attachments; this post uses TWO, because Stan asked for the shots
collated. Both are built from captures in the archive, and neither crops
or retouches a frame — a full 400x240 screen is always shown whole.

1. `pp-attach-1-stack-and-browser.png` — two screens stacked with a rule
   between them, both retaken 2026-08-27, neither cropped.

   The top screen is the stack with the formula line on, and it sits at
   that line's measured ceiling. The T line carries
   `root(4/9) + (1/2 + 3/4) x 2`: a radical wrapping a stacked fraction,
   two more stacked fractions, synthesized tall parentheses and the
   raised dot. TWO levels of stacking is the limit — measured by
   rendering candidates and checking whether the line drew the formula or
   fell back to T's value, a triple-nested fraction, a fraction over a
   fraction, and a radical over a radical all fall back. The operands are
   chosen so the intermediates stay pretty: root(4/9) is exactly 2/3, so
   Y and Z read as fractions and the answer is 3 1/6. An earlier take
   used root(2/3) and buried the stack in 20-digit decimals.

   The bottom screen is the browser with three different shapes: a plain
   chain `7 x 8 = 56` (selected), `root(2/3)` wrapping a stacked
   fraction, and a captured SIGMA over n = 1 to 10 of a program P,
   divided by `root(4)`. A Sigma does NOT fit the T line — it is taller
   than two levels, so it falls back there, which is why it is shown on
   this surface. PSHOW is no use for it either: that draws the X VALUE,
   not the formula.

   Every frame here was driven through the real key paths, so the shadow
   stack stayed truthful; no register was set behind the capture engine's
   back to make a nicer picture. The T-line default is OFF (FV11 pins the
   default, FV13 pins that turning it on changes the band). The browser
   is driven through `prettyBrowser(NOPARAM)` in the suite; T29 pins a
   wide row panning.

2. `pp-attach-2-nesting.png` — the capacity case: an integral from 0 to
   1 of a second derivative wrapping a Sigma over a root-fraction divided
   by a product, times a Pi with a nested power fraction, evaluated at
   x=2. Retaken 2026-08-27 (so its status bar is a day newer than the
   shot above). The expression is now the one a user can TYPE: the root
   takes brackets, powers use `^`, and the two indices are lowercase and
   distinct from the outer variable. EQ22 pins that it renders and EQ33
   pins that it evaluates, to 1.228593777031159439372254772764558. The
   earlier version of this shot was written in display glyphs, which draw
   the same but are not input syntax, so it could never have been
   computed.

## program-graphics (sim, 2026-09-05)

1. `pg-attach-1-2d-commands.png` — every 2D command of the package on one
   `PVIEW 6` canvas: a baseline and an axis (LINE), a diagonal, an outline
   BOX, a filled FBOX with a GMODE 2 hole, CIRCLE and FCIRCL, a 30 to 300
   degree ARC, TEXTOUT, DISP 1 for the title, a quarter FCIRCL cut by
   GCLIP with the clip rectangle outlined, and a sine curve of 26 LINE
   segments through XRNG and YRNG. Written by the headless test driver
   `pgTestShowcase2D` through the calculator's own `fnScreenDump`, scaled
   2x. Pin S1 records the count of lit pixels (10,760) on every gate run.

2. `pg-attach-2-3d-cube.gif` — the G4 film, 122 frames at 10 per second,
   400 by 240: the saddle z = x² - y² from the four-step program `SADL`
   on a 24 by 24 `WIREFRAME` mesh, and the cube of the volume from `PT3D`
   and `LINE3D`, then one home frame, 36 UP presses (x), 36 f-UP presses
   (y), 36 g-UP presses (z), six plus and six minus presses. Every frame
   is a `fnScreenDump` of the headless suite's driver `pgTestShowcase3D`,
   joined by the assembly script of DESIGN.md §9.7.3. Pins S3 and R1 to
   R2 record the count of the still (8,656 lit pixels) and the byte-exact
   return of the canvas after each full turn and after the zoom.
   The forum attachment `pg-attach-2-3d-cube.gif` is scaled to 200x120
   (244 KB, 122 frames) to meet forum attachment size limits; the native
   400x240 capture is preserved in `pg-attach-2-3d-cube-400x240.gif` (464 KB),
   and `pg-attach-2-3d-cube-120kb.gif` provides a 61-frame (121 KB) fallback.

