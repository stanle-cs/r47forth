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
