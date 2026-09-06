# program-graphics: DESIGN-HISTORY.md

The non-normative amendment trail of DESIGN.md. One entry per stage or
ruling, newest last.

## 2026-09-04: the survey and the plan

Stan opened the package after pretty-print shipped. The survey
(`SURVEY_program-drawing_2026-09-04.md`) found that upstream already has the
HP-42S program graphics `CLLCD`, `PIXEL`, `POINT`, `AGRAPH`, and that a
drawing dies at a normal program stop. The plan (`PLAN_2026-09-04.md`)
records eight rulings and a second round of four. The research
(`RESEARCH_hp-conventions_2026-09-04.md`) fixed the naming rule: Plus42,
then RPL, then Prime.

Two design changes came from Stan's challenges on the same day. First,
speed became a law (DESIGN.md §8) after the reading of the interpreter
path showed that nothing refreshes the LCD between program steps on the
DM42. Second, the kernel writes the screen buffer directly (DESIGN.md
§4.1). The first draft called `bitblt24` per pixel out of caution about
the DMCP ROM's dirty-row protocol. Upstream's own hardware code shows the
row header (`c47.c:618`, `screen.c:695-696`), and the simulator's blitter
mirrors the ROM, so the caution was withdrawn.

## Stage G0: the skeleton and the baseline

Branch `program-graphics/stage-g0`, base af7ad934a. The stage adds no
behaviour. It adds the package working area, two test drivers, the gate
script, and the baseline measurement.

Anchors chosen under the touching-line rule:

| File | Anchor | Distance from the nearest sibling edit |
|---|---|---|
| `testSuite/testSuite.c` prototypes | after `covLoadStateLongLabel` (line 60) | undo-history at 83 |
| `testSuite/testSuite.c` registration | before the `// Statistics` comment (line 309) | pretty-print-extra at 690 |
| `testSuite/tests/testSuiteList.txt` | after `regmgmt_cov` (line 86) | pretty-print at 267 |

The baseline driver runs 20,000 steps of `NOP` and 20,000 steps of
`PIXEL` through `reallyRunFunction` with `programRunStop` set to running.
It does not run `executeOneStep` or the key poll, so it measures the
dispatch cost, not the whole step. Numbers on the simulator:

| Loop | Time for 1,000,000 steps | Per step |
|---|---|---|
| `NOP` through `reallyRunFunction` | 91 ms | 91 ns |
| `PIXEL` through `reallyRunFunction` | 158 ms | 158 ns |
| `PIXEL` body alone (difference) | 67 ms | 67 ns |

The `PIXEL` body costs about three quarters of the dispatch. Both are far
below the LCD refresh cost, which the simulator does not model.

The G0 pin was verified red first. With the two registration rows removed,
the suite reported "Cannot find the function to test" for both `Func:`
lines and 2 failures. With the rows restored, the suite was green again.

Process lesson from the red-first check: after a refresh, a plain `ninja`
does not re-apply the package patches. The resolver materializes them into
the shadow tree at `meson setup --reconfigure` time. The first restored run
still ran the mutant and showed a false red. The rule is refresh, then
reconfigure, then build, then test. The gate script does this. An ad-hoc
loop must do it too.

Firmware size: the stage adds no firmware code. The two drivers compile
only under `TESTSUITE_BUILD`. The flash and BSS deltas are zero by
construction. They were not measured with `make dmcp5r47`. The first
measured delta comes with G1.

The simulator numbers do not transfer to the DM42. The ratio of the
`PIXEL` body to the `NOP` dispatch is the number to watch.

## Stage G1: the canvas view

Implementation choices that DESIGN.md version 0 left open, now fixed in
DESIGN.md §3 and §6:

1. `PVIEW` with a bad parameter raises `ERROR_OUT_OF_RANGE`, not the
   data-type error. The parameter is a number, and the number is out of
   range.
2. The key resolution chain had no free arm position. Undo-history owns
   the chain head, forth-core rewrites the main condition, and
   pretty-print-extra owns the arm before the final else. The fix was the
   registry mechanism: pretty-print-extra's arm became the range form
   `calcMode >= 20 && calcMode <= 23`, and this package carries the same
   bytes. Forth-core already covers the range in its rewrite.
3. R/S in the canvas view goes through upstream's release path. The
   press arm only clears `showFunctionNameItem`. A separate release arm
   sets it to `ITM_RS` for the canvas view without the register line
   paint that the `CM_NORMAL` arm does.
4. The five key functions with a bug-screen default get a no-op case each.
5. The `CANVAS` softmenu row sits after row 180, not at the tail, because
   pretty-print-extra's tail row and a second tail row do not merge.
6. Prototypes go into `screen.h`, because three siblings patch `c47.h`.

Firmware size, `make dmcp5r47`, this package alone against no package:

| Section | Without | With | Delta |
|---|---|---|---|
| flash | 1,090,512 | 1,091,000 | +488 bytes |
| ram | 7,548 | 7,564 | +16 bytes (the `pgCanvas_t` state) |

Red-first results of the G1 pins on the simulator:

| Mutation | Pins that went red |
|---|---|
| No refreshScreen case for the canvas | V8, the status bar repaint. V4 stayed green, because the default arm of the switch paints nothing. The contract now names V8 as the pin of the case. |
| No restore of calcMode on EXIT | V6, K3, and V7 through the leaked mode |
| Region 2 clips at row 239 | V1 |
| No no-op case in `fnKeyEnter` | K2 and K3, through the bug screen that the release raises |

With the sources restored the suite was green: solo 13,020 tests, combined
with the four other packages 13,046 tests.

Two pin lessons from this stage. The softmenu painter clears its band
before it paints, so a pixel in rows 171 to 239 proves nothing about the
region clear. The clip row is the observable. And EXIT, ENTER, BACKSPACE,
UP, DOWN, and .d run their key function on the key release, through the
item function, so a key pin must drive the release too.

## Audit round 1 on G1, 2026-09-04, and its fix wave

Stan ruled the same evening: one round of bug hunting and one fix wave per
stage, then the next stage (PLAN §10).

Readers. The in-family runner's eight finder agents were refused at spawn
by a platform classifier (category "reasoning extraction"); its report
writer read the range itself and produced five traced findings. Sol (GPT-5)
read a self-contained lifecycle packet. Gemini 3.1 Pro read a self-contained
keys packet, after two repository-access forms of the same packet timed
out with no reply. The report and the packets are in `audit/`.

| Finding | Source | Disposition |
|---|---|---|
| EXIT never closes the view | Sol 1 | Packet defect. My extraction matched the ENTER no-op case under the EXIT label. The real case calls `pgCloseView`, pin K3 proves it. Encoded in the packet template as the tenth class. |
| A program step that calls `calcModeNormal` (CLSTK, CLA) abandons the view and the drawing dies at STOP | Sol 2, report G1R1-2 | Confirmed. Fixed: `calcModeNormal` returns at once in mode 21. Pin K4, red first. |
| The guard comment over-claims | Sol 3, Gemini 1 | Comment reworded. The keys with their own case have no-op arms. |
| ENTER and `.d` as program steps inside the view do nothing | report G1R1-1 | Confirmed. Fixed: `pgEffectiveCalcMode()` returns CM_NORMAL for a running program in mode 21, and `fnKeyEnter`, `fnKeyCC`, `fnKeyDotD`, `fnTo_ms` switch on it. Pin K5, red first. New bug class in the catalog. |
| CC and `.ms` show a bug screen inside the view | report G1R1-3 | Confirmed. The bug-screen defaults are eight, not six. Fixed with two no-op cases. Pin K6, red first. |
| An error inside the view has no visible message, and the EXIT press paints the Z line over the canvas | report G1R1-4 | Confirmed. Fixed: the canvas case paints the error text on canvas line 1, and `refreshRegisterLine` returns at once in mode 21. Pin K7, red first. |
| Pin K3 cannot see a press that swallows EXIT | report G1R1-5 | Documented pin limit. `keyActionProcessed` is static. |
| Shift keys never engage in mode 21, so shifted items are unreachable | Gemini 2 | Documented limit. SNAP is a long press of EXIT on the R47. |
| A shift state left active before the view traps R/S and EXIT | Gemini 3 | Refuted by the report's trace: the shift state is set only through `commonShiftProcessing`, which excludes mode 21, and it is reset after every resolution. |

Process lessons. Self-contained packets only, for both outside readers.
A case extracted by a text pattern must be anchored inside its function
and the fence grepped for the promised identifier. The in-family refusal
is recorded in the skill with candidate triggers.

## Stage G2: the 2D commands

Ten commands on rows 2450 to 2459: `LINE`, `BOX`, `FBOX`, `CIRCLE`,
`FCIRCL`, `ARC`, `TEXTOUT`, `DISP`, `GMODE`, `GCLIP`. The kernel writes the
buffer directly (DESIGN.md §4.1). Two names changed from the plan: `RECT`
is upstream's rectangular complex mode, so the filled rectangle is `FBOX`;
and C47 has no alpha register, so `TEXTOUT` takes its string from Z and
`DISP n` from X.

The arc's full-turn test converts the span to degrees with upstream's
`convertAngleFromTo`, because the tree has no two-pi constant under the
name the first draft assumed. The arc's span test is an integer cross
product per pixel against two direction vectors computed once with the
WP34S sine and cosine.

The long integer fast path reads the low limb of the register directly,
with the sign tag, and refuses a value above 32767. The real path costs
two decimal compares and one decimal to int32.

Speed on the simulator, 100,000 `LINE` steps of 100 pixels through
`reallyRunFunction`: 47 to 52 ms, that is about 0.5 microseconds per line
and about 5 nanoseconds per pixel above the dispatch. The `NOP` dispatch
stays at about 90 nanoseconds per step.

The showcase screen of TESTING.md §6 has 10,500 lit pixels in rows 20 to
239, recorded as pin S1. A second picture shows region 2 with the `CANVAS`
softmenu below the drawing.

Firmware size, `make dmcp5r47`, this package alone against no package:

| Section | Without | At G1 | At G2 | Delta of G2 |
|---|---|---|---|---|
| flash | 1,090,512 | 1,091,000 | 1,094,552 | +3,552 bytes |
| ram | 7,548 | 7,564 | 7,600 | +36 bytes (the two real34 limits and their flag) |

Red-first results of the G2 pins on the simulator:

| Mutation | Pins that went red |
|---|---|
| The line stepper skips its last pixel | D2, D3, S1 |
| No right clamp in the run writer | The run writes outside the row and the suite aborts. Red by a crash, not by a pin. The clip law pin D8 checks the neighbour rows; a left-clamp mutation is the next check. |
| Invert becomes set | D9, S1 |
| No dirty flag in the pixel writer | D10, after the pin was rebuilt to drive the pixel path on its own rows. The first D10 drew zero-length lines, which the run writer handles, so the pixel writer was never exercised. |
| No type check in the coordinate reader | D12 |
| The arc is left out of the showcase | S1 |

With the sources restored the suite was green: solo 13,022 and combined
13,048 tests.

Two pin lessons. A zero-length line goes through the run writer, so an
equivalence pin that draws points does not test the pixel writer; the pin
must draw vertical segments. And a mutation that removes a clamp can red by
a crash before any assertion runs; the assertion that names the defect is
the neighbour-row check.

## Audit round 1 on G2, 2026-09-04, and its fix wave

Two out-of-family readers read G2 on the day of its commit. Sol (GPT-5)
took the kernel packet, 27.6 KB, self-contained. That packet held the
writers, the steppers for lines, boxes, circles and arcs, the coordinate
reader, and the ten commands, with the numbers of the layout. Gemini 3.1
Pro took the contracts packet, 27.6 KB. That packet held the same code,
with the questions turned to the callers and the shared state. The in-family finders were refused at spawn
by the platform classifier, as on G1. The cause was found the same day
and is recorded in the cross-model-audit skill: the finders' output
schema, not the prompt text. The in-family leg runs on the fixed workflow
after this fix wave, over the range that includes it.

The two readers converged on two sites from different evidence, so the
round counts eight distinct claims, not nine.

| Claim | Reader | Verdict | Where it went |
|---|---|---|---|
| `GCLIP` clamps only the bottom row. Two rows below the region narrow into int16 and the clip starts at a negative row. A later `FBOX` writes before the buffer. | Sol 1 | Confirmed. The worst finding of the round. | Fixed: the clip is the intersection of the rectangle and the region, with an empty sentinel (x0 = 1, x1 = 0) when nothing is left. Pin D13 on all four sides. |
| The filled circle overflows int32: the square root above radius 16384, and 4 r squared above 23170. Radius 23170 far off screen paints a full row. | Sol 2, Gemini 3 | Confirmed by both, with different consequences (a full row, a single column). | Fixed: 64-bit square root and product, and the fill loop is limited to the rows of the clip. Pin D14. |
| `DISP` ignores the clip columns: it clears the full width and starts the text at column 1. | Sol 3, Gemini 4 | Confirmed by both. | Fixed: the clear and the text stay between the clip columns. Pin D15. |
| The arc direction vectors are scaled by 1024, so a span under 0.056 degrees collapses to one pixel. | Sol 4 | Confirmed. | Fixed: scale 65536, cross products already in 64 bits. Pin D16. |
| The string cap can cut inside a two-byte glyph. The glyph-trim loop then steps over the NUL and reads beyond the string. | Gemini 1, and Sol named the cap as a gap | Confirmed. | Fixed: the cap backs up before a lead byte, and the trim loop stops at a lead byte followed by NUL. Pins D17 and D17b. |
| `pgReadAngle` accepts NaN and infinity. | Sol, named as a gap | Confirmed. | Fixed: `ERROR_OUT_OF_RANGE`. Pin D18. |
| `TEXTOUT` and `DISP` overwrite `tmpString`, which the program runner can keep across a step. | Gemini 2 | Refuted by the operator. Upstream item functions write `tmpString` inside a step (stringFuncs.c 290-305, factorial.c 21) and the runner writes it fresh when it shows a step (nextStep.c 280). No runner state lives there. | Handed to the in-family refutation pass as an extra finding. |
| `pgError` paints the error and never sets `errorShown`. | Gemini 5 | Refuted by the operator. `pgError` only raises the error. The paint on canvas line 1 happens in `pgRefreshCanvasView`, which sets the flag when it paints and clears both together (pin K7). | Handed to the in-family refutation pass as an extra finding. |

Sol also asked whether a long integer register can carry stale bytes
above its value. It cannot. `convertLongIntegerToLongIntegerRegister`
reallocates the register to the exact limb size of the value before the
copy. As a result, the fast-path reader sees only the bytes of the value.

Two lessons for the packets. A packet that supplies the numbers of the
layout gets findings with numbers back. Here the numbers were the rows of
52 bytes, the mirrored bit order, and the int16 clip fields, and Sol
computed the exact narrowed row. Two readers on the same code with
different questions converge on the same defects from different
consequences. That convergence is the corroboration that the process
wants. It is also the dedup work that the operator owes before a number
is minted.

Red-first results of the fix-wave pins on the simulator:

| Mutation | Pins that went red |
|---|---|
| `GCLIP` with the one-sided clamps of the first G2 code | D13: a stored clip edge outside the region, and a full-screen `FBOX` through the empty clip |
| The square root in 32 bits | D14: the off-screen circle of radius 23170 painted a row |
| 4 r squared narrowed to 32 bits | D14: the circle of radius 32767 left a corner clear |
| `DISP` clears the full width and starts at column 1 | D15, all three checks |
| Arc vectors scaled by 1024 | D16 |
| The cap cuts inside a glyph | D17 |
| A NaN angle accepted | D18, both checks: the error was not raised, and the arc drew |

One mutation stayed green by its own design. The first form of the
"4 r squared" mutation narrowed r squared minus dy squared to 32 bits.
That value fits 32 bits for every legal radius. The overflow of the first
G2 code was in the product by four. The mutation was rewritten to narrow
that product. The trim-loop guard (a lone lead byte before the NUL) has
no mutation. Its failure is a hang, so pin D17b documents the guard and
does not falsify it.

Firmware size after the fix wave, `make dmcp5r47`, R47.elf, text plus
data for flash and data plus BSS for RAM, without and with the package in
one build session:

| Section | Without | With G2 and its fix wave | Delta |
|---|---|---|---|
| flash | 1,090,512 | 1,094,824 | +4,312 bytes (+272 against the G2 commit) |
| ram | 7,564 | 7,616 | +52 bytes (the 64-bit square root and the sentinel clip) |

## Stage G3: the window, 2026-09-05

Two commands on rows 2460 and 2461: `XRNG` and `YRNG`, the minimum in
Y and the maximum in X. A real coordinate now goes through the window of
its axis with the arithmetic of upstream's `screenWindowRatio` in
plotstat.c: the ratio in 39 digits, then rounded half away from zero.
Without a range set, a real is a pixel with the same rounding, so 2.5 is
pixel 3 where G2 truncated it to 2. A long integer stays a pixel under
any window. A radius is always pixels.

Three rulings made during the stage, all recorded in DESIGN.md §5:

- A result beyond 32767 pixels is `ERROR_OUT_OF_RANGE`, where upstream's
  plots clamp. A clamped endpoint changes the slope of a line, and a
  refused command draws nothing wrong.
- Equal range ends are `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` and leave the
  window as it was. A reversed range mirrors the axis.
- The complex two-point form: `LINE`, `BOX`, `FBOX`, and `GCLIP` accept
  two complex points, the first in Y and the second in X, as RPL `LINE`
  takes them from levels 2 and 1. A complex in one of X and Y without the
  other is the type error. `ARC` reads its complex center through the
  window the same way.

The window lives in its own static struct next to the canvas state at
the top of the file, because the package header is read before
`realType.h` and cannot name `real34_t`. The window survives `ERASE` and `PVIEW`. Only `XRNG`, `YRNG`,
and a reset change it. In `PVIEW 6` the top 20 rows of the y range lie
under the status bar, because the window maps onto the full pixel grid.

Two build lessons. The catalog generator compiles items.c with a stub per
function, and a new item needs its stub in the package's stub block or the
generator fails before the firmware compiles. The equal-ends error was
first written with an error name that does not exist in defines.h; the
compiler found it.

The showcase gained a sine curve through the window on columns 130 to
390, rows 92 to 108: 26 segments of real endpoints. The recorded count
S1 moved from 10,500 to 10,760 lit pixels.

Red-first results of the G3 pins on the simulator:

| Mutation | Pins that went red |
|---|---|
| Round toward zero instead of half away | W1 both checks, and W2, W3, W5, W6, S1 with it |
| A scale of 398 instead of 399 | W2 both checks, and W3, W6, S1 |
| No equal-ends check | W3 both checks |
| Clamp at 32767 instead of the error | W4 |
| Real and imaginary parts swapped | W5, and D6, D16 through the shared arc center reader |
| `ERASE` clears the window | W6, on the second form of the mutation |

The first form of the `ERASE` mutation did not compile: the window struct
was declared after `pgSetRegion`, so the mutation could not name it. The
struct now sits next to the canvas state at the top of the file, and the
mutation was rerun on that form.

Firmware size, `make dmcp5r47`, R47.elf, text plus data for flash and
data plus BSS for RAM:

| Section | Without | With G3 | Delta against the G2 fix wave |
|---|---|---|---|
| flash | 1,090,512 | 1,095,288 | +464 bytes |
| ram | 7,564 | 7,648 | +32 bytes (the window struct, less the two real34 limits it replaced) |

## Audit round 1 on G2, the in-family half, and its fix wave, 2026-09-05

The in-family finders ran for the first time on this package after the
refusal fix of `c9e589b25`: eight finders, three-lens refutation, one
report, `design-docs/forth-core/AUDIT_program-graphics-stage-G2-...tip-0663b2360_2026-09-05.md`.
The range was the whole package, `af7ad934a..0663b2360`, with both
outside replies and five extra claims fed into the refutation pass.
Eighteen distinct findings survived. Under the one-round ruling this is
the one fix wave for G2, applied on the G3 branch.

| Finding | Disposition |
|---|---|
| G2R1-1 With the view closed, the commands skip the PIXEL flag protocol; a `LINE` from the menu is erased by its own key release. | Fixed: `pgRefreshMaybe` sets the manual flags and `screenHoldsDrawnPixels` outside the view. Pin D20 over four commands. |
| G2R1-2 A string that ends in a lone lead byte is not trimmed when it fits; `showString` paints stale bytes. | Fixed: the cut lands on a glyph boundary found by a walk, whatever the width. Pin D17d. |
| G2R1-3 `ARC` collapses a span just under a full turn to one pixel. | Fixed: the same-direction arm consults the exact span, 180 degrees or more draws the circle. Pin D19. |
| G2R1-4 The view opened from alpha input mode keeps the cursor and `FLAG_ALPHA`. | Fixed: the prologue of upstream's browsers in `fnPview`, and the reverse on EXIT. Pin K8. |
| G2R1-5 In region 2 a menu popped to a blank base leaves its labels. | Fixed: the band is cleared before the painter runs. Pin V9. |
| G2R1-6 A mixed long-integer and real pair is accepted, against §5.1. | Ruled at G3: each coordinate is read by its own type. §5.1 amended in the G3 commit. |
| G2R1-7 The cap guard cannot tell a lead byte from a trail byte with bit 7 set. | Fixed by the same walk. Pin D17c. |
| G2R1-8 The DISP band clear has no pin. | Pin D15b. |
| G2R1-9 D8 reads a row the refused command never targets. | D8 rewritten. |
| G2R1-10 D17b claims a property the code does not have. | D17b rewritten around the canary, and the code now has the property. |
| G2R1-11 K7's band assertion is a conjunction with the flag. | K7 split. |
| G2R1-12 K1 checks a constant; no pin presses a softkey. | Pin K10 drives a real softkey press with the `CANVAS` menu pushed. |
| G2R1-13 §5.1 said 31 bits where the code says 32767. | Amended in the G3 commit. |
| G2R1-14 The filled rectangle was specified three ways under the dead name `RECT`. | §4.1, §4.4 and §8.3 now give one recipe under `FBOX`. |
| G2R1-15 Two readers of `canvas.region` disagree with §3.6 and §3.7. | §3.6 names the readers, §3.7 tests `calcMode`, as the code does. |
| G2R1-16 `fnGarc` carried a second copy of the real reader. | Removed in the G3 commit: `pgReadComplexPoint`. |
| G2R1-17 `pgArc` works in the user frame against §3.4. | Recorded as the one exception in §4.5. |
| G2R1-18 `GMODE` out of range raises `ERROR_OUT_OF_RANGE`, §4.7 said the type error. | §4.7 amended to the code, which follows `PVIEW` and `DISP`. |
| U7 An interactive ENTER paints the T line over the canvas through `showFunctionName` and `hideFunctionName`. | Fixed: both functions carry a mode-21 arm in the screen.c mirror; the item still arms and runs on release. Pin K9. |
| U8 The long-press SNAP gesture paints the same way. | The same two arms cover it. Not executed: the long-press timer path is not driven by the suite. |
| U9 In the combined build undo-history's shift gate lets f and g engage. | Recorded in §10 limit 8. G4 carries that line itself. |

Out-of-family round 1 on G3 ran on the same day. Sol (GPT-5) found that
two range ends that differ only beyond 34 digits are refused as equal
ends: a documented limit (§5.2, §10 item 11), because the ends are stored
as real34 values and a range of that magnitude has no use on a 400-pixel
screen. Gemini 3.1 Pro read `pgArc`'s user-frame center as a defect; the
packet did not carry `pgArc`, and §4.5 now records the exception. Its
second note, that every error names register X, is §10 item 10.

Red-first results of the wave on the simulator:

| Mutation | Pins that went red |
|---|---|
| No PIXEL flags outside the view | D20 for all four commands, both checks each |
| The old bit-7 cap guard instead of the glyph walk | D17c, D17d |
| No 180-degree test in the same-direction arm | D19 |
| No alpha prologue in `fnPview` | K8, both checks |
| No band clear in region 2 | V9, on the second form of the pin |
| No `DISP` band fill | D15b |
| No `showFunctionName` arm | K9, both checks |
| No `hideFunctionName` arm | K9 |
| No range clause in `executeFunction` | K10, on the second form of the pin |

Two pins needed a second form. V9 first ran with the base flags as the
suite left them, and the mutation stayed green; the pin now clears
`FLAG_BASE_HOME` and `FLAG_BASE_MYM` itself. K10 first pressed and
released the softkey and stayed green under the mutation, because the
release only arms the double-tap timer; the pin now calls the click that
the timer's timeout calls. The trim walk's lone-lead-byte stop became
unreachable once the boundary cut runs first, so it was removed rather
than kept as a guard nothing can test; D17b now pins the cut through
its canary, red under the old cap guard.

Firmware size: measured at the G4 commit, not per fix wave.

## Stage G4: 3D, 2026-09-05

Nine commands on the upstream CONV spare rows 2864 to 2872: `EYEPT`,
`XVOL`, `YVOL`, `ZVOL`, `NUMX`, `NUMY`, `WIREFRAME`, `PT3D`, `LINE3D`.
The specification came from a research pass on the same day: five
readers on the HP projection, the program runner, the memory pool, the
key routes, and the animation tooling, then one writer. Every choice the
research left open is marked DECISION in DESIGN.md §9 for Stan to rule
on. The upstream facts the specification cites were verified by hand
before the code was written: the engine globals, the label helpers, the
shift macros, the reset anchor, the free item rows, and the key items.

What the stage does. The projection is the HP 48 rule: the plane sits
one unit in front of the eye and moves with it, so the near face draws
at scale 1 when the eye is one unit before it. The three rotations are
integer step counts of 10 degrees with a 36-entry sine table, so a full
turn returns the canvas byte for byte and no float trigonometry enters
the flash. The zoom moves the eye so the near face scales by exactly
1.25 per press. The retained content lives in one 2 KB block of the pool:
a 64-byte header, the grid bytes up from the header, the line records
down from the end, one byte per value in 254 steps with 255 as the hole.
The block is taken by the first 3D command inside the view, emptied by
`ERASE` and `PVIEW`, freed at EXIT, and forgotten without a free at a
reset through a one-line `config.c` hook. `WIREFRAME` runs the label
with the engine protocol of the sum and plot engines, `FLAG_SOLVING`
included, so the body runs inside a program; the stack comes back
through the undo image. The keys reach the package through the existing
`fnKeyUp` and `fnKeyDown` cases and the guard arm; the shift keys engage
because the package now carries undo-history's shift gate line byte for
byte, and the shift glyph stays in the status bar through two macro
edits in `defines.h`.

The showcase: a saddle from a four-step program on a 24 by 24 grid and
the cube of the volume, then the film: one home frame, 36 steps about
each axis, six zoom steps in and six out. The suite writes every frame
as a BMP and the assembly script joins them into a GIF.

Three suite lessons from the first runs. A register write takes pool
blocks of its own, so a pin that counts pool blocks sets its registers
before it reads the count. The program loader `fnLoadProgram` reads the
file's lines into the alpha input buffer and leaves the last word there,
and a later string test expects that buffer empty; the test loader clears
it. A program loaded twice gives a duplicate global label, and a later
equation test then fails with a syntax error; the test loader skips a
label that exists.

One open question, handed to the G4 audit round as a pre-verified fact.
With the package's drivers in their G3 place in the suite list, the first
formula integration of `integrate_cov.txt` later fails with a syntax
error: the parser's word reader sees a word longer than seven glyphs in a
formula that reads "X". A backtrace put the site in `_parseWord` under
`parseEquation` under the integrator. The failure needs both 3D drivers
in the same run, goes away without the engine's undo pair, and does not
change any of thirty-four probed globals across the drivers. The
package's test file now runs right after the equation files, as the
suite's own comment orders those files by the pool state they inherit;
the tail of the list is where every sibling package appends its own
file, and an entry there conflicts in the combined build. Whether a G4
command leaves a pool block dirty, or the upstream parser reads past a
formula, is not settled.

Numbers recorded at the first green run:

| Number | Value |
|---|---|
| P2, the plane z = 0 on a 2 by 2 grid in the unit-cube view | 798 lit pixels, as the specification computed before the first run |
| S3a, the saddle alone, 24 by 24 | 6,083 lit pixels |
| S3, the showcase still with the cube and the caption | 8,656 lit pixels |
| The film | 122 frames; the canvas returns byte for byte after 36 steps about each axis and after six zoom steps in and out |
| Program runs in the showcase | 6,940 across the suite's 3D drivers: the saddle grid, the zoom re-runs (one per press past the threshold, 576 samples each), and the pins |
| `NOP` and `LINE` baselines, unchanged | about 95 ms per million steps, 47 to 49 ms per 100,000 lines of 100 pixels |

Red-first results of the G4 pins on the simulator:

| Mutation | Pins that went red |
|---|---|
| The projection plane one unit farther from the eye | P1, all eight corners |
| No row lines in the mesh | P2 (600 pixels for 798), S3 |
| No free of the block at EXIT (inside `pg3dFreeBlock`) | P3 |
| A clamped value encodes as the hole | P5, S3 |
| RBR dropped from the guard arm | P9, P26 |
| The steps counted modulo 37 | P10 |
| No stack restore after WIREFRAME | P16, both registers |
| The reset hook frees the block | P18 |
| ERASE keeps the retained content | P19, both checks |
| NUMX accepts 1 | P20, both checks |
| LINE3D without a current point draws from the origin | P23 |
| No free-bytes test for a line | P12, both checks |
| The eps test exclusive again | P27 |
| No clamp of the final row | P28: (32000, 32239) for (32000, 32000) |
| No finite-span check | P20b |
| A valid grid without the counts check | P29 |

The first form of the "no free at EXIT" mutation replaced the call in
`pgCloseView` with two assignments that name the 3D state, which is
declared later in the file, so it did not compile; the second form
removes the free inside `pg3dFreeBlock`.

Firmware size, `make dmcp5r47`, R47.elf, text plus data for flash and
data plus BSS for RAM:

| Section | Without | With G4 and its fix wave | Delta against G3 | Delta of the package |
|---|---|---|---|---|
| flash | 1,090,512 | 1,101,144 | +5,856 bytes | +10,632 bytes |
| ram | 7,564 | 7,720 | +72 bytes (the 3D state and two counters) | +156 bytes, plus a 2 KB pool block while a 3D view is open |

Out-of-family round 1 on G4, same day, three packets: G (Sol, the
projection and the block arithmetic), H (Gemini, the engine protocol),
I (Sol, the setting commands and the lines). Packet I came back with one
finding: a volume range whose span overflows float (`XVOL -2e38 2e38`)
makes the byte scale zero and the decode NaN. Fixed in the G4 fix wave:
`pg3dRange` refuses a span that is not a finite positive float, pin
P20b. Packet I also noted that the packet text carried the `pgWindow`
struct twice and lacked two helpers it named; a packet defect, not a
code one.

Packet G (Sol, GPT-5) came back with three: a hole byte in a line record
decodes as a coordinate (unreachable: the readers refuse NaN and infinity
before a record is written; recorded, not fixed); a point exactly one
1024th of the depth in front of the eye was rejected where the contract
says "nearer than" (fixed, pin P27); the final row was not clamped after
the flip, so the kernel could receive 32239 where the contract promises
32000 (fixed, pin P28). Packet H (Gemini 3.1 Pro) came back with two: the
restore of the undo flag after the engine's `fnUndo` re-armed a consumed
undo image (removed: the image is consumed as after PLTf); a body that
calls `ERASE` under `WIREFRAME` could leave a valid grid with zero counts
(fixed: a valid grid needs the header's counts intact, pin P29). Gemini
cleared the STOP, error, string, nesting, EXIT, reset and boot paths.
Neither reader could name a mechanism for the open suite question from
the code in its packet.

## Audit round 1 on G3 and G4, 2026-09-05

One round for both stages, under the one-round ruling. The out-of-family
half ran on 2026-09-05 before this entry: packets E and F on G3 (Sol,
Gemini), G, H and I on G4 (Sol, Gemini, Sol). The G4 replies were fixed
on the readers' word inside the G4 commit; this round is the first
reading of that fix wave. The in-family half ran the same day: eight
finders, three-lens refutation of every finding, the nine out-of-family
findings fed through the same refutation. Report:
`design-docs/program-graphics/audit/AUDIT_G3-G4_round-1_2026-09-05.md`.
Findings, not fixes: the tree is as it was at `840fe1c92`.

Three results decide the next wave.

The GTK simulator does not start with the package. The G4 items.c hunk
replaced upstream's sentinel row at index 2885, which carries the same
stale comment `/* 2870 */` as the real row 2870, with a second WIREFRAME
row, and `c47-gtk.c` refuses to run when that row is not `"Last item"`.
Executed: the built simulator exits with the message. The headless gate
has no such check, so it stayed green from `e19769236` on. The run-sim
skill has been unusable for the package since that commit.

The open suite question of the G4 entry is settled, and it is upstream.
`parseEquation` scans up to seven glyphs forward for a label marker and
never tests for the terminator, so the one-glyph formula `"X"` is read
past its NUL into the neighbouring pool block. A `':'` there makes the
parser treat the neighbour's bytes as the formula. The package only
changes the pool layout. The evidence is a pool tiling check that stayed
clean, a hardware watchpoint that saw no write to the formula block, and
a dump of the parser's input at the error. The reorder of
`testSuiteList.txt` in the G4 commit is a workaround for this defect, and
its comment describes the symptom. An upstream report is the next step.

The test drivers leak. `pgTestUnitCubeView` and pin P27 call `pgReset()`
while a 3D block is allocated, and `pgReset()` forgets the block without
a free by design, because a real reset rebuilds the pool. Two blocks of
512 pool blocks leak per run of `pgTestDraw3D`. Five finders found it
independently. It is part of the pool layout that exposed the upstream
defect, and it breaks the first pin rule.

The rest of the round is in the report. The design and the suite
disagree on the G4 pin set: DESIGN.md §9.8 names 28 pins with numbers
and mutations, 12 have no code, and the fix wave reused the numbers P27
and P28 for pins that §9.8 gives other content. The 3D redraw
has no pin with an absolute oracle. `pg3dEnsure` decides "view open" by
`canvas.region` where every other site decides by `calcMode`, so a
plot-abandoned view lets a 3D command take the block outside the view.
The nine G4 rows sit on the head of upstream's CONV growth region.

Process. The finders ran without a refusal for the second time since the
schema fix. Two operator lessons went into the skill: the mechanical half
now launches the built simulator once, because the gate does not run the
sentinel check that `c47-gtk.c` makes at start; and the bug-class catalog
gained the over-read class, with the rule that a test-order dependency on
"the pool state inherited" is a symptom to explain, not a rule to record.
