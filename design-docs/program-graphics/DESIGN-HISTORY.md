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
