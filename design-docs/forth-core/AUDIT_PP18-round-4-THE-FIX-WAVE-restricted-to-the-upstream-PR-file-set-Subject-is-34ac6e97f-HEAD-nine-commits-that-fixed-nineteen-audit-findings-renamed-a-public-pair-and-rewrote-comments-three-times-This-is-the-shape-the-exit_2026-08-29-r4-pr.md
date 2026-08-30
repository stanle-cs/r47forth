# Audit — PP18 round 4 (restarted series), THE FIX WAVE, at `6e6c2c0ab`

Subject: `34ac6e97f..HEAD` on `pretty-print/stage-pp18`, tip `6e6c2c0ab`. Nine
commits: three that changed behaviour and closed nineteen findings from rounds
1–3, one mechanical rename of the public `ppfCombine1/2` → `ppfBuildOp1/2`
pair, one workflow commit, one docs commit archiving the six reports behind the
wave, and three consecutive comment rewrites over the same sources. **Reading
restricted to the upstream-PR file set** — `prettyVisual.c`, `prettyLayout.c`,
`prettyFormula.c`, `prettyEquation.c`, `prettyInternal.h`, `prettyPrint.h`,
`prettyTest.c`, `screen.c`.

This is the shape the exit criterion names — the fix commits themselves, not the
stage they fixed — and the shape this project's own record says bites: r2 was 4
of 7 findings from r1's fixes, r3 4 of 4, r5 9 of 12. **It bit again, and
hardest at the one fix that was written to be structural.** `PP18RR1-11` moved
the stack-lift clear out of seventeen switch arms into a `ppvStep` epilogue "no
item can skip", mirroring upstream's dispatch epilogue. One of those arms —
`ITM_XEQ` — does not contain a step; it contains the callee's entire execution.
Its early `return` was load-bearing, and the epilogue now fires after the
subroutine has run, erasing a lift latch the callee armed. A subroutine ending
in ENTER makes VISUAL draw a formula that computes a different number than XEQ
returns, silently, with no decline — which is the exact failure `327ec4811`'s
own commit message says the wave existed to remove, and a property restarted
round 1 had examined and cleared as faithful before the fix reversed it.

**Fourteen CONFIRMED defects, one PLAUSIBLE, six REFUTED.** Thirty findings
were raised across eight in-family dimensions; six were killed by the
refutation pass, six fell beyond the verification cap, and the eighteen
surviving entries collapse to fourteen distinct defects — three of the fourteen
were derived independently by two or three dimensions, which is the fan-out
working. **Eleven of the fourteen are backed by a probe or mutation** applied,
built through the package's own gate, observed in
`build.sim/meson-logs/testlog.txt` and reverted.

Eight of the fourteen are the fix wave's own work: two wrong drawings it
created (`-1`, `-5`), one contract it deleted the only written statement of
(`-3`), three comments it wrote that are false about the code they guard
(`-9`, `-10`, `-14`), one clamp applied at one of a function's two writes
(`-12`), and one new pin that goes vacuous on an ambient flag it never sets
(`-6`). Three more are fixes that shipped with no assertion at all, or with an
assertion that does not reach the changed code (`-7`, `-8`, `-13`). The
remaining three are seams the wave opened and walked only one side of (`-2`,
`-4`) or narrowed by one pixel (`-11`).

Nothing was fixed. Every probe and mutation was applied inside an isolated
worktree, observed, and reverted; the main tree is clean at start and finish
and the gate is green on it.

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).

### Subject

**Tip.** `6e6c2c0ab` ("pkg: inline comments capped at three lines; longer
explanations move to the doc"). Range `34ac6e97f..HEAD`: **nine commits**, 41
files, +10,468 / −1,060 — of which 7,892 insertions are the six archived audit
reports and 243 are the workflow commit, so the code delta is small and dense.

| commit | subject | what it is |
|---|---|---|
| `327ec4811` | VISUAL reads the machine's mode flags, and the lift rule stops being per-arm | `PP18RR1-2` (eRPN ENTER), `PP18RR1-3` (SSIZE4 + T replication), `PP18RR1-11` (the epilogue refactor), `PP18RR3-2` (the menu freeze) |
| `5ce3985b2` | the drawing engine's acceptors stop being narrower than its own producers | `PP18RR2-1`, `-13`, `PP18RR3-6`, `-11`, `PP18RR1-1`, `PP18RR3-1`, `-5`, `-3`, `PP18RR2-OOF-1` |
| `e81677309` | the shadow stops describing registers it no longer mirrors | `PP18RR2-3`, `-4`, `-2`, `-5`, `-6`, `-10` (mostly `prettyCapture.c`, out of the PR file set; its `PPN_VAL2` half reaches in) |
| `da8fa57b4` | `ppfCombine1/2` become `ppfBuildOp1/2` | rename, 18 insertions / 18 deletions |
| `c719b1a9c` | the out-of-family pass stops being skippable in silence | workflow + skill, no package code |
| `c1d809e69` | the six audit reports behind the nineteen fixes | docs only |
| `7d3c0af31` | the VISUAL set's comments say what the code does | comment-only, 3 sources |
| `9fdf90ce3` | the rest of the comments stop telling the story of their own bug | comment-only, whole package |
| `6e6c2c0ab` | inline comments capped at three lines | comment-only, 8 sources |

**In-scope file set and its delta over the range:**

| file | +/− | lines at tip |
|---|---|---|
| `prettyVisual.c` | +161 / −145 | 1,639 |
| `prettyTest.c` | +543 / −61 | 5,711 |
| `prettyLayout.c` | +69 / −47 | 869 |
| `prettyEquation.c` | +69 / −9 | 1,035 |
| `prettyFormula.c` | +26 / −42 | 760 |
| `screen.c` (override) | +23 / −11 | — |
| `prettyInternal.h` | +19 / −18 | 186 |
| `prettyPrint.h` | +6 / −0 | 131 |

**Out of the PR file set by instruction**, read only where a call path from an
in-scope fix led into them: `prettyCapture.c` (+225/−0 in range — the largest
code change in the wave, and the producer half of `PP18RR4-2`), `prettyValue.c`
(+56/−0), `keyboard.c`, `solver/equation.c`, `browsers/prettyBrowser.c`. **A
reader restricted to the PR file set cannot audit `e81677309`**, which is where
six of the nineteen fixes landed; that is stated as a coverage hole, not as a
clearance.

**KNOWN, excluded from re-reporting** (verified still open, then fenced):
`PP18RR3-1..15` and `-P1`; `PP18RR3-OOF-*`; `PP18RR2-1..17` and `-OOF-1`;
`PP18RR1-1..12` and `-P1`; `PP18R4-1..11`; the pre-restart `PP18-1..16` and
their rulings. Where a finding below is adjacent to a known one it says so.
Nineteen of those were closed by this wave; this round's subject is the
closures, not the openings.

**Numbering.** `PP18RR4-1`–`PP18RR4-14`, plausible `PP18RR4-P1`, design
observations `PP18RR4-D1`–`D6`. `grep -rn PP18RR4` over the repository returned
nothing before this file was written. In-code tags `AUDIT R1-*`…`R4-*` quoted
below are the *package's own* series, not this one — and note that the three
comment commits deleted almost all of them.

### Coverage (union across the eight in-family dimensions)

**Read in full, by three or more dimensions:** `prettyVisual.c` (all 1,639 —
`ppvAlloc`/`ppvIntern`/`ppvLeaf`, `ppvLiveStackSlots`, `ppvPush`/
`ppvPushLifting`/`ppvPop`, `ppvOp1`/`ppvOp2`, `ppvBody`, `ppvConstruct`,
`ppvIntegral`, the name-scoping cluster, `ppvDerivVariable`/`ppvDerivative`,
`ppvLiftNeutral`, `ppvRefillFromT`, `ppvStep`/`ppvStepArm` every arm, `ppvWalk`,
`ppvAstToNodes`, both paint surfaces, `fnPrettyVisual`); `prettyLayout.c`'s
changed regions plus every `ppMeasure`/`ppPaint` pair that touches them;
`prettyFormula.c` (the renamed pair, `ppfWrapIf`, `ppfFromCaptureNode`,
`ppfBuildEntry`, `ppfBuildRow`, `ppfStageValFields`); `prettyEquation.c`
(`PPQ_IS_SUP`, `ppqNumber`, `ppqFitWithEllipsis`, `ppqShowRender`); both
headers; both `screen.c` hunks with their enclosing upstream functions.

**Read for the pins:** all eight new or changed pins (M6's fixture correction,
M8, EQ4b, EQ4c, EQ9b, V-MODE, V-MODE4, V36b) plus the five capture-side pins
the wave added in `prettyTest.c` (T20b, T22b, T23b, T23c, T24b) and the older
pins they lean on (M5, P1–P6, FV8, FV9, EQ9, V33, V38, V65, V72/V75/V76).

**Upstream verified by execution path, not assumed:** `items.c` — the SLS
epilogue at `:604-610` and the SLS/PTP rows of every item the walker dispatches;
`programming/lblGtoXeq.c` — `executeOneStep`'s PTP table, `fnExecute` under
`PGM_RUNNING`, `_putLiteral`, `fnReturn`; `keyboard.c` `fnKeyEnter`'s eRPN
branch and `btnReleased`'s direct `refreshRegisterLine`; `stack.c`
`liftStack`/`_Drop`/`fnFillStack`; `defines.h` `getStackTop`, the `SCRUPD_*`
bits, `SHOWMODE`, `FLAG_BOLD`; `flags.c` `systemFlagAction` and
`refreshStateFlags[]`; `radioButtonCatalog.c` `fnRefreshState`; `statusBar.c`
`refreshStatusBar`'s self-guard; `softmenus.c` `showSoftmenu`/`popSoftmenu`/
`showSoftmenuCurrentPart`/`setScreenUpdateFromMenu`; `screen.c`
`_refreshNormalScreen`, `RETURN_NORMAL`, `refreshScreen`, `_selectiveClearScreen`,
`_refreshPemScreen`; `display.c` `fnC47Show`'s matrix arm; `charString.c`
`_calculateStringWidth`; `ui/tam.c` `_tamProcessInput` and
`leaveTamModeIfEnabled`; `c47.c`'s main loop `lcd_refresh_dma` block;
`browsers/fontBrowser.c`'s own `FLAG_BOLD` save/restore idiom.

**Measured artifacts, not read:** all 441 `numericFont` glyphs parsed out of
`build.sim/src/ttf2RasterFonts/rasterFontsData.c` for the NIM degree-glyph
arithmetic in `PP18RR4-11`; `build.sim/custom_pkg_shadow/*` checked for every
mutation, to prove the probe reached the compiler and not just the flat working
area.

### Not reached, and it matters where

- **`e81677309`'s six fixes are mostly outside the PR file set.** The
  `prettyCapture.c` half (+225 lines: the FILL STAGE arm, the STO retiming, the
  SST invalidation, `ppcIsSlotRegister`, `ppcShadowInvalidate`, the `PPN_VAL2`
  producer) was read only at the seams. `PP18RR4-2` is what one of those seams
  produced when followed into the in-scope file; the rest of that commit is
  unaudited this round.
- **`prettyTest.c`'s 5,711 lines were sampled, not swept.** The eight new pins
  and their neighbours were read; the T- and B-series, EQ26–EQ35 and the P-series
  beyond P6 were not. Four of this round's findings are pin defects, all found in
  the sampled set, which says nothing about the unsampled one.
- **No simulator ran and no LCD photograph backs any finding.**
  `PP18RR4-5`'s consequence is a pixel-sum measurement inside the harness, not a
  screen; `PP18RR4-11` and `PP18RR4-12`'s device halves rest on the DMCP SDK
  declaration and upstream's own field note.
- **`design-audit.sh` is forth-core's**; there is still no pretty-print
  equivalent, so no override-budget check ran. §2's churn scan is the substitute
  and this round it has something to say.
- **Flash was not measured.** Two of the three code commits record no
  `make dmcp5r47` delta (§2), and this round did not build the device target to
  supply one.

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Fourth consecutive round with no out-of-family reader**, and the first one
run after `c719b1a9c` — a commit inside this very subject range whose entire
purpose is to stop that from happening silently. It did what it promised: the
skip is recorded in a banner rather than a footnote. It did not stop the skip.

---

## 2. Mechanical results

**Gate: GREEN.** `./packages/pretty-print/build-test.sh --solo` run by this
synthesis pass on the main tree at `6e6c2c0ab`: `testSuite EXIT STATUS: 0`,
`PRETTY-PRINT GATE GREEN`, `grep -c "test FAIL" build.sim/meson-logs/testlog.txt`
= **0**. Eleven verifier worktrees ran it to completion independently (163–220 s)
and each re-ran it after reverting its probe. No new compiler warning. `git
status --porcelain` empty before and after; no `AUDIT-PROBE` marker survives
anywhere in `packages/`.

**The comment-only claim holds, and this is its fourth independent check.**
Three finder dimensions verified it with their own comment strippers; this pass
re-ran it a fourth time, preprocessing every `.c`/`.h` file each of the three
commits touched with `gcc -fpreprocessed -dD -E -P`, collapsing all whitespace,
and comparing hashes against each commit's parent. **37 file/commit pairs, all
identical.** Nothing semantic is hidden in the sweep. `7d3c0af31` (3 sources, 6
pairs with their `files/` twins), `9fdf90ce3` (10 sources, 17 pairs, including
the `screen.c`, `keyboard.c` and `solver/equation.c` overrides), `6e6c2c0ab`
(7 sources, 14 pairs). That claim is the wave's own and it is true.

**Upstream churn: the one place the gate does not look, and it moved.**
`patch_churn_scan.py` over all thirteen pretty-print patches now **exits 1**
with **9** mechanical findings, against **1** at `34ac6e97f`.

| patch | at `34ac6e97f` | at `6e6c2c0ab` |
|---|---|---|
| `010-screen.c.patch` | 13 adds / 0 dels / **2** hunks | 36 / **11** / **4** |
| `010-keyboard.c.patch` | 76 / 3 / 11 | 64 / 3 / **13** |
| `010-solver__equation.c.patch` | 619 / 1 / 5 | 608 / 1 / 5 |
| churn findings | 1 (`[WS-ONLY]`, pre-existing, catalogued) | 9 |

All eight new `[WS-ONLY]` hits are in `010-screen.c.patch` and all eight are the
same shape: `setPixel(x1, y1)`, `setPixel(x1+1, y1)`, `setPixel(x2, y1)`,
`if(rep_enlarge) {`, `setPixel(x1, y2)`, `setPixel(x2, y2)` and two closing
braces, re-indented one level because the `PP18RR3-3` clamp wraps them in an
`if` instead of using the no-reindent wrap this same file already has a landed
example of. Eleven upstream lines in `showGlyphCode`'s inner draw loop are now
MODIFIED rather than merely adjacent. **The two comment commits paid for it in
the other direction** — `keyboard.c` 76→64 added lines, `solver/equation.c`
619→608, which is what `9fdf90ce3`'s message claims — but `keyboard.c`'s hunk
count went 11→**13** while its adds fell, because removing comments split hunks.
Added lines are the cheap metric; modified lines and hunk count are the ones a
rebase pays, and both went up.

**`showGlyphCode` is now edited by two packages.** `packages/forth-core/patches/
010-screen.c.patch` has a hunk at `:1159` (the function's own declaration line);
pretty-print's new hunk is at `:1272`, inside the same function. Pretty-print's
other new hunk (`:5872`) sits between forth-core's `:5672` and `:5927`. This is
`PP18RR4-D5`.

**Flash and RAM accounting is one commit out of three.** `e81677309` records
"Flash +504 B measured (R47.pg5, package build, against the same build of the
pre-wave tree). RAM unchanged at 11,188 of 16,384" and "Costs two of 24 arena
nodes at runtime; no static RAM." `327ec4811` (+480 insertions) and `5ce3985b2`
(+778) record neither. CLAUDE.md's standing rule is that flash increases are
fine when justified and that the measured `make dmcp5r47` delta is recorded in
the stage commit; two thirds of this wave's code did not.

**The design record was not updated.** `git log 34ac6e97f..HEAD --
design-docs/pretty-print/` returns exactly one commit, `da8fa57b4`, and its
whole delta is the `ppfCombine` → `ppfBuildOp` rename plus a DESIGN-HISTORY
entry about the rename. Nineteen fixes, several of them new binding behaviour —
the mode-dependent stack model, T replication and the `saturated` latch, the
eRPN ENTER rule, the BOLD-must-still-display ruling, `PPN_VAL2`'s two-node
value leaf, the EQSHW ellipsis — landed with no DESIGN.md or DESIGN-HISTORY.md
entry at all. `grep -rni "saturat\|replicat\|SSIZE\|PPN_VAL2" design-docs/pretty-print/`
returns nothing. This is `PP18RR4-D4`.

**Probes and mutations this round** — all applied in isolated worktrees, built
through the real refresh (presence verified in `build.sim/custom_pkg_shadow/*`
or the regenerated `files/` twin), observed in `testlog.txt`, and reverted.

| probe / mutation | observed result | finding |
|---|---|---|
| fixtures `VXC = {LBL; RCL b; ENTER; END}`, `VXR = {RCL a; XEQ VXC; RCL c; +; +}` + `ppvTestExpect("a+(b+c)")` | `FAIL … (expected 'a+(b+c)', actual 'b+(b+c)')` — sole failure in the battery | **`PP18RR4-1`** |
| `ppcTestType("3"); ppcTestOp(ITM_CUBE); ppcTestOp(ITM_CUBE)` → `ppfBuildCurrent` and `ppfBuildEntry`, pinned to a sentinel | live `S(S(3\|3)\|3)`, history `[S(S(3\|3)\|3) = 19 683]`; walker's own V-pin is `S(P(S(a\|2))\|2)` | **`PP18RR4-3`** |
| T22b's own complex fixture rendered through `ppfBuildCurrent` and through `ppfBuildEntry` side by side | live `[¨H2.·+\x01\x010 …]`, filed `[2.·+3.· …]` — same formula, two surfaces, different number | **`PP18RR4-2`** |
| `doRefreshSoftMenu = false` around M8's BOLD render, then assert it stayed false | `FAIL … pretty paint under FLAG_BOLD raised doRefreshSoftMenu` — sole failure | **`PP18RR4-10`** |
| full-screen VISUAL (`VQAD`, a 4-deep integral) + `refreshScreen`, band ink sums | shipped: `scrupd=10 drsm=1`, bottom blank at paint, then `169-216=37.6M 217-239=24.4M` after refresh. Guard disabled: both bands stay **0** | **`PP18RR4-5`** |
| `ppSuppressBold` neutered, M8 fixture unchanged | gate **RED**, `M8 BOLD changed the pretty output (expected 196, actual 290)` — the only pin in the battery that notices | control |
| same, plus one line `clearSystemFlag(FLAG_PROPFR)` (fixture value untouched) | gate **GREEN** | **`PP18RR4-6`** |
| `prettyLayout.c:290-296` (the whole `idxDesc` block) deleted | gate **GREEN**; a probe on `8 ENTER 1 ENTER 2 / XTHROOT` measures index `relBase −2 / descent 8` → `idxDesc 6` against radicand descent 0 | **`PP18RR4-7`** |
| `ppqShowRender`'s fallback reverted to the pre-wave one-liner (the `ppqFitWithEllipsis` call site deleted) | gate **GREEN**, `Fail: 0`; EQ9b passes | **`PP18RR4-8`** |
| `prettyCapture.c:766` guard reverted to `(t - REGISTER_X) <= ppcTopSlot()` | gate **RED**, `T23c slot 4 still holds its old tree …` — sole failure | control |
| `ppcShiftUpForLift` clamped so slot 4 is never populated under SSIZE8 | gate **GREEN**, T23c passes asserting nothing | **`PP18RR4-13`** |
| `ppqNumber`'s ·₁₀ sup loop mutated so the plus form parses to a different shape | gate **RED** on EQ4c's *third* assertion alone | **REFUTES** the EQ4c finding |
| VMEN / VM4 executed on the machine under all four flag settings | `err=0 dtype=0` and X = **7, 8, 15, 16** — exactly the pins' pictures | **REFUTES** the V-MODE oracle finding |
| `if(0)` in place of the new `SCRUPD_MANUAL_MENU` test | only V36b reddens, for BOTH VISUAL arms identically | corroborates **`PP18RR4-5`** |

Two of those mutations deleted a shipped fix outright and left the gate fully
green (`PP18RR4-7`, `PP18RR4-8`); a third made a live pin vacuous with one
ambient flag (`PP18RR4-6`); a fourth made a live pin vacuous with one clamp
(`PP18RR4-13`). That is four of the wave's own assertions, and it is
`PP18RR4-D3`.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner, not by how clever it is. A silently
wrong formula on the stage's headline feature outranks a wrong number on an
always-on line, which outranks a wrong drawing nobody's ordinary keystrokes
reach, which outranks a refusal to draw a program that runs, which outranks ink
in a band that was cleared for something else, which outranks a pin that can go
vacuous, which outranks a comment that cannot re-derive its own code, which
outranks one pixel column.

Where the refutation pass corrected the finder, the correction is stated
**inside** the finding. Five of the fourteen carry one, and two of those
materially change the reaching input.

Each carries: `file:line`, what breaks, the concrete reaching input, the
violated contract quoted, the bug class, and the class-level test. **No patches.**

---

### PP18RR4-1 — `ppvStep`'s epilogue runs after `ITM_XEQ`'s arm has walked the whole callee, so a subroutine ending in ENTER has its lift latch erased and the caller draws the wrong operand

`packages/pretty-print/prettyVisual.c:851` (the epilogue's clear), against the
`ITM_XEQ` arm at `:994-1004`. **Introduced by `327ec4811` (`PP18RR1-11`).**

**What breaks.** `ppvStep` is `ppvStepArm(...)` followed by
`if(!ppvLiftNeutral(op)) stk->liftDisabled = false;`. `ITM_XEQ` is not in
`ppvLiftNeutral`'s list, and its arm does not execute one step — it calls
`ppvWalk` for the callee on the shared stack and returns. So the epilogue fires
*after the entire subroutine has run* and clears a latch the callee's trailing
ENTER armed (`:878`, `liftDisabled = !getSystemFlag(FLAG_ERPN)`, which survives
its own epilogue because ENTER *is* lift-neutral). The caller's next lifting
read then goes through `ppvPushLifting` (`:218-231`) with `liftDisabled` false
and **pushes where the machine overwrites X**.

Upstream does the opposite, and the timing is the whole point: `ITM_XEQ` is
`SLS_ENABLED` (`items.c:1821`) but the SLS epilogue lives inside
`reallyRunFunction` (`items.c:604-610`), and under `PGM_RUNNING` `fnExecute`
only pushes a subroutine level and calls `fnGoto`
(`programming/lblGtoXeq.c:167-190`) — so the lift enable lands at the XEQ step,
**before** the callee's first step. `fnKeyEnter` clears `FLAG_ASLIFT` in classic
RPN (`keyboard.c:3437-3442`); `RTN` (`items.c:1822`) and `END` (`:3310`) are both
`SLS_UNCHANGED`, so the clear survives the return.

**Reaching input** (built as a fixture and measured):

```
LBL 'VXR' :  RCL a ; XEQ 'VXC' ; RCL c ; + ; + ; END
LBL 'VXC' :  RCL b ; ENTER ; END
VISUAL 'VXR'
```

Machine: `a+(b+c)`. Walker: **`b+(b+c)`** — the drawing names `b` where the
machine used `a`, with no decline and no D-number. A second dimension reached
the identical class from the other end with literals: `LBL A: 1 2 XEQ B 5 + +`
against `LBL B: 4 ENTER`, which returns 11 and draws `4+(4+5)` = 13.

**Evidence.** `build.sim/meson-logs/testlog.txt:488` —
`prettyPrint test FAIL: AUDIT-PROBE R4 callee trailing ENTER (expected 'a+(b+c)', actual 'b+(b+c)')`
— the only prettyPrint failure in the run, which also shows no existing pin
covers this direction.

**It is a regression, not a pre-existing defect.** At `34ac6e97f` the clear was
a trailing statement of the single switch function
(`34ac6e97f:prettyVisual.c:992`, "every op above finishes with lift enabled")
and the `ITM_XEQ` arm returned before reaching it (`:950-965`), so the callee's
latch propagated correctly. Splitting into `ppvStepArm` + a `ppvStep` wrapper
turned a load-bearing skip into an unconditional post-arm clear. **The property
had already been examined and passed:** restarted round 1's report cleared "a
trailing ENTER's latch surviving a subroutine return (upstream RTN is not in
the SLS-clearing set — faithful)". The fix reversed a ruling.

**Violated.** The arm's own comment, `prettyVisual.c:995-997`: *"Order, not
reach: this runs BEFORE the nested walk so a callee never inherits an ENTER
latch armed in its caller. The epilogue runs after the arm returns and so cannot
do it."* The epilogue does not merely fail to do it — it **undoes** what the
callee did. And `ppvLiftNeutral`'s banner, `:803-806`: *"Upstream clears stack
lift in a dispatch epilogue no item can skip, and this mirrors that"* — upstream
runs XEQ's epilogue before the callee; this one runs it after. And
`327ec4811`'s own message: *"the invariant held only because each of those
[seventeen returning arms] happened to be lift-neutral"* — false for `ITM_XEQ`
in the callee-exit direction, which is the one direction the arms' `return` was
protecting.

**Bug class.** An epilogue relocated across a nesting boundary: a per-step rule
made universal at a site whose "step" is a whole subtree. The same class as
`PP18-5` with the sign flipped — that finding was "the latch survived where it
must not", this one is "the latch dies where it must live".

**Class-level test.** Enumerate the arms that call `ppvWalk` (today: `ITM_XEQ`)
and pin **both** directions for each: a caller's armed latch must not be
inherited by the callee (V72/V75/V76 already pin this), and a callee's armed
latch must survive the return (nothing pins this). Drive the enumeration from
the arms, not from example programs — every existing fixture ends its callee
with an operator, which is why five rounds and the fix's own pin set missed it.

---

### PP18RR4-2 — the live formula surface never learned the `PPN_VAL2` continuation the same commit added, so a captured complex draws a garbage imaginary part on the T line and correctly in history

`packages/pretty-print/prettyFormula.c:404-409` (the `PPN_VAL` arm of
`ppfFromCaptureNode`). **Half-introduced by `e81677309` (`PP18RR2-10`).**

**What breaks.** `PP18RR2-10` made an oversized value leaf continue into a
`PPN_VAL2` node on `child[0]` — `prettyCapture.c:249-258` sets `pad[1] = 32` for
a `dtComplex34` against a `uint8_t payload[16]`. The **serializer** was taught
to reassemble both halves (`prettyCapture.c:369-381`, with a comment naming the
hazard: *"copying `bytes` straight out would read past the array"*). The
**live builder** was not: `ppfFromCaptureNode`'s `PPN_VAL` arm passes
`nd->payload` and `nd->pad[1]` straight into `ppfStageValFields`, which does
`xcopy(getRegisterDataPointer(TEMP_REGISTER_1), payload, bytes)` — 32 bytes out
of a 16-byte array, so bytes 16..31 are the *next arena node's* header and the
continuation's first payload bytes. `grep -n "PPN_VAL2\|PPN_LIT2"
prettyFormula.c` returns exactly one hit, line 397, the `LIT2` walk.
`git show --stat e81677309` does not list `prettyFormula.c` at all.

**Reaching input.** Pretty print on, T-line formula on (`FLAG_PTLINE`) or PHIST
open (row 0 is the live formula). Complex in X (`2 ENTER 3` `CPX`) with the
shadow slot UNKNOWN (cold start, or after any `ppcShadowInvalidate` — a browser
recall does it), type `4` (lifts: X=4, Y=2+3i), press `×`. STAGE's
`ppcEnsureKnown(1)` snapshots Y through `ppcValLeafFromRegister`, minting the
`PPN_VAL` + `PPN_VAL2` pair; `prettyValue.c:826` → `ppfBuildCurrent` →
`ppfFromCaptureNode` then reads only the head. **This is the wave's own new
fixture T22b.**

**Evidence.** Both builders driven over T22b's own state, printed verbatim:

```
AUDIT-PROBE R4 live T-line sig: '[¨H2.· +\x01\x010· · ¡H ·\x80· 4]'
AUDIT-PROBE R4 filed entry sig: '[2.· +3.· · ¡H ·\x80· 4]'
```

The filed history entry draws `2. + 3.i × 4`; the live T line draws a corrupted
real head and an imaginary part of `\x01\x010`. Two surfaces over one encoding,
one taught. **T22b stayed green throughout** — it asserts only that
`ppcCurrentFormulaRoot() != PPC_NIL`, so the wave's own pin for this fix agrees
with the bug.

**Violated.** `prettyInternal.h:83-86`, added by this wave: *"PPN_VAL2: a VAL
whose payload exceeds one node continues into a continuation on child[0], the
shape PPN_LIT/PPN_LIT2 already use."* `ppfFromCaptureNode` implements that
shape for `PPN_LIT` twelve lines above and not for `PPN_VAL`. And
`DESIGN.md:167-174`, BINDING: *"the slot degrades to a value leaf snapshotted
from its register (truthful by construction) … The display never lies"*, with
*"value leaves store raw register payloads ≤16 B (complex via a two-child
header), formatted only at display time by staging into TEMP_REGISTER_1"*.

**Bug class.** An encoding widened with two readers and one of them taught —
the same class as `PP18RR4-4` and `PP18RR4-12`, and the one `d5b61ab8c`'s own
commit message names as *"the class had three producers"*.

**Class-level test.** For each capture node kind that can carry a continuation
(`PPN_LIT`/`PPN_LIT2`, `PPN_VAL`/`PPN_VAL2`, and whatever comes next), assert
that **every** consumer reproduces the same rendered text: the live
`ppfBuildCurrent` path, the `ppfBuildEntry` history decoder, and the serializer
round trip. Drive it from a table of register types spanning the capacity
boundary (`dtLongInteger` short, `dtReal34` at 16 B, `dtComplex34` at 32 B, an
oversized long integer that must stay OPAQUE, which T26 already pins). One
table, three columns, and this defect is a single red cell.

---

### PP18RR4-3 — `ppfBuildOp1`'s "the caller brackets a stacked power" precondition is met by the walker and by neither capture call site, and the wave deleted the only place it was written down

`packages/pretty-print/prettyFormula.c:234` (the `SQUARE`/`CUBE` arm) reached
from `:417-423` (`ppfFromCaptureNode`) and `:539-553` (`ppfBuildEntry`), against
`packages/pretty-print/prettyInternal.h:119`.

**What breaks.** `ppfBuildOp1` sets `*outPrec = PPF_PREC_ATOM` at entry and the
`SQUARE`/`CUBE` arm never lowers it, then wraps its base with
`ppfWrapIf(a, aPrec, PPF_PREC_ATOM)` — and `ppfWrapIf` brackets only when
`prec < minPrec`, so `ATOM < ATOM` is false and a stacked power gets no
parentheses. The walker compensates locally (`prettyVisual.c:1113-1120` forces
`PPF_PREC_MUL` when SQUARE/CUBE sits under SQUARE/CUBE); the two capture call
sites pass the inner precedence straight through.

**Reaching input.** `3`, then EXP softmenu `x³` (`softmenus.c:80`, `menu_EXP`
slot 0 = `ITM_CUBE`), then `x³` again. X = 19683. Capture mirrors it as
`OP1(CUBE, OP1(CUBE, LIT "3"))` — `ITM_CUBE` is in the monadic set
(`prettyCapture.c:543`) and the STAGE arm does not supersede, because the slot
IS the current node. Now read the live formula on the T line or open PHIST.

**Evidence.** Both capture front ends printed the same unbracketed tree —
live `S(S(3|3)|3)`, history `[S(S(3|3)|3) = 19 683]` — with no `PP_PAREN` node
anywhere, against the walker's own pinned expectation for the identical
operator pair, `prettyTest.c:5561`: `cases[5].sig = "S(P(S(a|2))|2)"`. The
capture surfaces draw `3³³` for a recorded result of 3⁹.

**Correction against the finder.** `PP_SUP` places the outer exponent at the
same `-supDrop` as the inner one (`prettyLayout.c:405-425`), so the picture is a
flat `3³³` reading most naturally as 3^33, not the finding's 3^(3³). Either
reading contradicts 19683.

**What the fix wave did.** The unbracketed capture behaviour predates
`34ac6e97f` — the walker's guard was the PP18 addition. What this wave
contributed is deleting the precondition's only written statement:
`git show 34ac6e97f:packages/pretty-print/prettyInternal.h` carried *"A caller
stacking powers must bracket the base itself"* on the declaration block, and
`6e6c2c0ab` (the three-line comment cap) removed it. `git log -S` confirms
that commit is the sole deleter.

**Violated.** `DESIGN.md:571-574`: *"a stacked power DOES need its base
bracketed, which the walker does locally because `ppfBuildOp` deliberately has
no POW level — a `PP_SUP` normally scopes itself, and adding a level would
change the contract underneath the capture engine (V51)."* The doc asserts the
capture engine has a precondition; it never asserts that the capture engine
satisfies it. Also `prettyInternal.h:117-119` as it now stands, which claims the
builders exist *"so precedence is decided in one place"* while two of the three
callers must fix up the result themselves.

**Adjacent, and not separately filed:** `ppfBuildOp2`'s `ITM_YX` arm
(`prettyFormula.c:108-116`) wraps its base on the same `< PPF_PREC_ATOM` test,
and the walker's guard covers only OP1-under-OP1, so `a² yˣ b` is unbracketed
on **every** front end including VISUAL. Same defect, wider blast radius,
found while verifying this one.

**Bug class.** A precondition enforced at one of three call sites, with the
written statement of it deleted by a comment sweep — a contract that now exists
only in one caller's behaviour.

**Class-level test.** For each `ppf*` builder that has a documented
precondition, drive the same operator sequence through **all three** front ends
(walker, live capture, history decode) and assert the three signatures are
equal. `x² x²`, `x³ x³`, `x² x³` and `a² yˣ b` are four rows; today the walker
column and the two capture columns disagree on three of them.

---

### PP18RR4-4 — `ITM_FILL` is the one arm that fills the stack without going through `ppvPush`, so the new `saturated` latch never arms and VISUAL refuses to draw a program that runs

`packages/pretty-print/prettyVisual.c:910-921` (the arm), against `:210-212`
(the latch's only setter) and `:836-838` (`ppvRefillFromT`'s head). **Left
standing by `327ec4811` (`PP18RR1-3`).** Reached independently by three
dimensions (contracts, lifecycle, arithmetic).

**What breaks.** `PP18RR1-3` introduced `bool_t saturated` and
`ppvLiveStackSlots()`, set the latch inside `ppvPush`, and taught
`ppvRefillFromT` to replicate T only once the model has actually held a full
stack. `ITM_FILL` writes `stk->ast[0..7] = t` and `stk->depth =
PPV_STACK_SLOTS` **directly** and returns via `break`. So `saturated` stays
false, `ppvRefillFromT` returns at its head for the rest of the walk, and
because every consuming op is net-negative the depth only ever falls —
`ppvPush`'s `if(depth >= slots)` is never re-reached either. `git show
327ec4811 -- prettyVisual.c` adds the field, the helper and the `ppvBody`
initialiser and touches **no arm of the switch**; `git log -L` shows the FILL
arm last changed at `47f6e609b`, before the invariant it now violates existed.
`ppvBody` (`:429-441`) performs semantically the same operation — one node on
every level — and does it through `ppvPush` in a loop, so it latches correctly.
The construct path got the invariant; the user-facing arm did not.

**Reaching input.** Under SSIZE8, `LBL 'VF' : 2 ; FILL ; × × × × × × × × ; END`
(eight multiplies), then `VISUAL 'VF'`. Hardware: `fnFillStack`
(`stack.c:203-220`) copies X into Y..`getStackTop()`, every `×` pulls T down
into itself (`_Drop`, `stack.c:41-59`), XEQ returns 512 and never underflows.
Walker: depth walks 8,7,…,1 and the eighth `×` hits `ppvPop` at depth 0 →
**`PPV_D_UNDERFLOW` (D10)**, "cannot be drawn", no picture. The DROP form is the
same shape one step earlier: `2 FILL DROP×8` declines `PPV_D_EMPTY` (D17,
"nothing to show") for a program that leaves 2 in X. FILL is programmable and
on a menu — `items.c:1860` gives it `CAT_FNCT | SLS_ENABLED | US_ENABLED`, and
it is `menu_STK` slot 5 (`softmenus.c:392`).

**Second half, same arm.** `stk->depth = PPV_STACK_SLOTS` is the **array
width**, not `ppvLiveStackSlots()`. Under SSIZE4 the mirror holds eight entries
where the machine holds four, breaking the `depth <= slots` invariant every
other site now maintains. I could not turn that into a wrong drawing — all
eight slots hold the same shared node and the next lifting push renormalises —
so it is filed as the second half of this finding rather than as a third one.

**Violated.** `prettyVisual.c:88`, the field's own contract: *"saturated — the
model has held a full stack, so T replicates on every drop"*, and
`ppvRefillFromT`'s banner at `:830-832`: *"This only applies once the model has
actually held a full stack. Before that the deeper registers hold whatever
preceded the program, which a static walk cannot know, so the underflow decline
is the honest answer."* FILL is precisely the operation that makes the whole
stack known, so the stated *reason* for the gate is exactly what FILL removes.
And `ppvLiveStackSlots`' banner at `:185-188`: *"the mirror drops its bottom
entry where the hardware does"* — FILL does not consult it.

**Nothing rules it deliberate.** The arm carries no comment and never has
(checked at `7d3c0af31^`, so nothing was lost in the sweep). `grep -rn
"saturat\|replicat\|ppvRefillFromT\|ppvLiveStackSlots"` over
`design-docs/pretty-print/` returns nothing; FILL appears in DESIGN.md exactly
twice (`:201` the capture classifier's coverage table, `:609` the opaque-taint
mover list), neither about the walker. DESIGN.md's "Not in v1, deliberately"
list (`:735-740`) names SOLVE/PGMSLV, CLX, `BINARY_REAL34` literals and dyadic
functions, and does not name FILL. The fail-closed ruling that sanctions
declines is about opcodes the dispatch table does not name; FILL is named.
`grep -n FILL prettyTest.c` returns only T20b, which is `ppcTestOp(ITM_FILL)` —
the capture engine. **The walker's FILL arm is unpinned entirely.**

**Bug class.** A new invariant installed at one producer with a second producer
left standing — the recurrence shape `d5b61ab8c` records as *"the class had
three producers"*, at the wave that recorded it.

**Class-level test.** Enumerate every arm that writes `stk->depth` without
calling `ppvPush` (today `ITM_FILL` is the only one) and assert for each that
after the arm `saturated` matches "the model has held `ppvLiveStackSlots()`
entries" and `depth <= ppvLiveStackSlots()`. Pair it with a behavioural pin:
`2 FILL` followed by more consuming steps than the stack is deep must draw, not
decline, under both SSIZE settings. `PP18RR1-3`'s own prescribed test — "a unit
pin that the walker's drop threshold tracks `getStackTop()` rather than a
constant" — is still unwritten, and this arm is why it matters.

---

### PP18RR4-5 — the new menu guard tests `SCRUPD_MANUAL_MENU` one frame after `popSoftmenu` cleared it, so the full-screen VISUAL gets the softkey row painted back into the picture it cleared

`packages/pretty-print/screen.c:5878`. **Introduced by `327ec4811`
(`PP18RR3-2`), comment reworded by `9fdf90ce3`.**

**What breaks.** The two VISUAL surfaces deliberately claim different amounts of
screen: the stack-window arm sets `SCRUPD_MANUAL_STACK` alone
(`prettyVisual.c:1622`) because it wants the menu back; the full-screen arm
clears y=16..239, draws frame lines at 20 and 168, and sets
`SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS`
(`prettyVisual.c:1543-1550`) because it does not. The new guard reads
`MANUAL_MENU` *after* TAM teardown: `leaveTamModeIfEnabled` → `popSoftmenu`,
whose **first statement** is `screenUpdatingMode &= ~(SCRUPD_MANUAL_MENU |
SCRUPD_SKIP_MENU_ONE_TIME)` (`softmenus.c:3716`). The bit the full-screen arm
set is gone before the guard runs, the guard sees it clear, and
`showSoftmenuCurrentPart()` + `refreshStatusBar()` paint into the cleared band.

**Reaching input.** Press VISUAL and give the label of a program whose formula
is taller than the 72-row Z/T band — a 4-deep nested integral does it (the
3-deep `VTRP` at 78/71 still fits shrunk, which is why the existing fixtures do
not reach this arm).

**Evidence (measured, real typed VISUAL path).**

```
shipped:  frame20=1 frame168=1 ink21_167=17164870 sum169_216=0 sum217_239=0 scrupd=10 drsm=1
          after refresh:                         sum169_216=37620973 sum217_239=24441977
guard off: after refresh:                        sum169_216=0        sum217_239=0
```

`scrupd=10` is `0x0A` = `MANUAL_STACK|MANUAL_SHIFT_STATUS` — `MANUAL_MENU`
(0x04) is already stripped by the time `refreshScreen` runs. The full-screen arm
is genuinely taken (both frame rows fully lit, 17.1M ink units between them),
its bottom is blank at paint time, and the next refresh inks 217..239 (the
softkey band) plus 169..216. Disabling only the new guard restores the blank
bottom exactly.

**Correction against the finder.** The clear is `popSoftmenu`'s own first
statement, not `screen.c:6222`'s `doRefreshSoftMenu` path — that one is a no-op
here because the bit is already gone. The mechanism and consequence stand.

**Violated.** The hunk's own comment, `screen.c:5875-5877`: *"This return is
TOTAL: it skips the menu and status bar as well as the stack. A caller that did
not set `SCRUPD_MANUAL_MENU` never asked to manage the menu."* The full-screen
arm **did** set it; the bit is cleared out from under it by routine menu
bookkeeping, so its absence is not evidence of consent. And `DESIGN.md` §6,
which specifies the full-screen surface's protocol verbatim as
`SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS` — a
designed, documented surface, not a corner case.

**V36b cannot see it.** Under `if(0)` in place of the guard, V36b is the only
pin that reddens — and it reddens for **both** arms identically, because it
drives `VDBL`, which fits the Z/T band. No V-series pin asserts anything about
the full-screen surface; the three `168` assertions in `prettyTest.c` (`:586`,
`:2279`, `:3014`) belong to PSHOW, FV5 and EQ7.

**Bug class.** A guard whose predicate is a flag another subsystem clears as
routine bookkeeping — absence of a request read as presence of consent, tested
one frame too late.

**Class-level test.** For every package surface that arms the manual-paint
protocol (PSHOW, PHIST/browser, EQSHW, VISUAL stack-window, VISUAL full-screen),
drive it through its real entry path, call `refreshScreen`, and assert which of
the three bands repaint. Five rows, three columns; the two VISUAL arms are
required to differ in the menu column, which is the assertion nothing makes
today.

---

### PP18RR4-6 — M8, the only pin that certifies "pretty print's output does not depend on `FLAG_BOLD`", goes vacuous on one ambient flag it never sets

`packages/pretty-print/prettyTest.c:295-345`, anchor `:338`. **New with
`5ce3985b2` (`PP18RR3-1`).**

**What breaks.** M8's part 2 renders the inline register line twice, with
`FLAG_BOLD` clear and set, and compares lit pixels. That only tests anything if
the drawing contains at least one `PP_FONT_NUMERIC` run, because
`showGlyphCode`'s bold substitution is gated on `font == &numericFont`
(`screen.c:1195`). `ppInlineRungs[0]` is `{PP_FONT_NUMERIC, PP_FONT_STANDARD}`
(`prettyValue.c:786`), so only a mixed number's **head** is numeric;
`ppParseFraction` builds numerator and denominator with `childFont` and skips
the head entirely when `headLen == 0` (`prettyValue.c:181-220`). M8 saves and
restores `FLAG_FRACT` and `FLAG_BOLD` and never touches `FLAG_PROPFR` — an
ordinary user display setting (`items.c:3744` `SetSetting PROPFR`;
`display.c:1736` selects the mixed "a b/c" form only when it is set).

**Reaching input (measured).** Baseline gate green. Neuter `ppSuppressBold`
(delete the fix): gate **RED**, `M8 BOLD changed the pretty output (lit pixels)
(expected 196, actual 290)`, and `grep -c "test FAIL"` = 1 — **M8 is the only
pin in the battery that notices**. Now add one line, `clearSystemFlag(FLAG_PROPFR)`,
leaving M8's fixture value at `1.5`: gate **GREEN**, exit 0. The fixture renders
`3/2`, both runs are standardFont-only, `litBold == litPlain`, and the pin
certifies nothing. The `litPlain == 0` guard does not catch it because the
standardFont fraction digits supply plenty of ink. Changing the fixture to
`0.75` — the value the sibling P1–P3 pixel pins already use — has the same
effect with no flag at all.

**Its stated tripwire watches the wrong glyph.** Part 1 (`:276-285`) probes
`findGlyph(&numericFont,'0')` against `findGlyphExact(&numericFontBold,'0')`.
The fixture is `1.5`, so the only numericFont glyph it draws is the head `1`.
Convergence of `'1'` alone leaves part 1 silent while part 2 goes vacuous;
convergence of `'0'` alone fires a false all-clear inviting removal of a
still-needed suppression.

**Violated.** M8's own comment: *"Part 1 pins WHY the suppression is needed,
against the live font tables rather than a remembered number — if a future font
makes the two tables agree, this assertion fires and the suppression can go."*
The tripwire does not watch the glyph the pin depends on. And the fixture rule
this file states three times, most plainly at `:1902`: *"A fixture must prove it
reached the state it claims to test."*

**Consequence if it goes quiet.** A BOLD owner gets glyphs painted from
`numericFontBold` (rowsAbove 1 / rows 29) inside a box measured from
`numericFont` (2 / 26) — ink one row above and two below the cleared box on the
always-on X line, which is `PP18RR3-1` returning with the gate green.

**Bug class.** A pin with an unstated, unchecked precondition — it asserts a
property of a drawing without asserting that the drawing contains the thing the
property is about.

**Class-level test.** Assert the precondition rather than freeze the fixture:
fail M8 if the built tree contains no `PP_FONT_NUMERIC` run (or if no painted
glyph came from `&numericFont`), and point part 1's tripwire at the glyphs the
fixture actually draws. Generalise it — every pixel-comparison pin should state
which font/rung it depends on and fail when that dependency is absent.

---

### PP18RR4-7 — `PP18RR1-1`, the PP_RAD index descent dual, shipped with no assertion anywhere in the suite

`packages/pretty-print/prettyLayout.c:290-296`, anchor `:293`. **New with
`5ce3985b2`.**

**What breaks.** Nothing today — the fix is correct. What is missing is any pin
that would notice its removal. Delete the whole `idxDesc` block and run
`./packages/pretty-print/build-test.sh --solo`: **GREEN**, exit 0, `testSuite
OK`. The mutation was verified present in `build.sim/custom_pkg_shadow/
prettyLayout.c` and the file recompiled, and the gate run covers both
pretty-print test lists (`pretty_print` and `pretty_visual_real`).

**The three radical pins, checked individually.** M5 (`:139-151`) builds a
`PP_RAD` with **one** child and asserts ascent 30 / descent 0 — it never enters
the `if(index != PP_NONE)` arm. FV8 (`:2380-2394`) is the only indexed root in
the file and its sole assertion is the signature string `"R(27;3)"`;
`ppfTestSigNode`'s `PP_RAD` arm (`:2122-2131`) emits `"R("`, children, `";"`,
`")"` and reads no ascent, descent, relX or relBase. P6 (`:495-525`) builds an
unindexed radical. `grep -n descent prettyTest.c` returns 15 hits and none
belongs to an indexed `PP_RAD`; `XTHROOT` appears exactly once in the file and
`CUBEROOT` not at all.

**The guarded branch is reachable in five keystrokes, and was measured.**
`8 ENTER 1 ENTER 2 / XTHROOT` builds `R(8;F(1|2))` through
`ppfBuildOp2`'s `ITM_XTHROOT` arm (`prettyFormula.c:120`, `idx = b`, an
arbitrary operand subtree appended with no wrap). Probe output at the real
caller's fonts (`PP_FONT_STANDARD`, as `ppfBuildRow` and the T-line rungs use):
radicand `8` ascent 12 / descent 0; index `FRAC(1,2)` ascent 20 / descent 8 /
relBase −2; `idxDesc = 6`; `PP_RAD` descent **6** with the block, **0** without.
So the guard fires on ordinary input and it is the sole reason the measured box
covers the index's ink.

**Violated.** The fix's own comment, `prettyLayout.c:290-292`: *"the index tucks
above the baseline, but its ink bottom at relBase + descent can fall below the
box, and every band check downstream trusts both edges."* Nothing asserts that
edge. And the project's standing rule (Stan, 2026-08-04): every fix lands with a
reproducer, a named bug class, and a class-level test where the class is
enumerable. This class is enumerable — it is "every `ppMeasure` arm that grows a
box from a child's metrics".

**Note from the file's own history.** FV9, immediately below FV8
(`:2396-2424`), carries a comment recording that this exact mistake was already
made once: its earlier root-descent assertion *"was satisfied by 'log's own
descender — MUT-25 stayed green"*, and it was repaired for `PP_SUB` by pinning
the child's `relBase`. `PP_RAD`'s index never got the same treatment.

**Bug class.** A metrics fix pinned by a signature test, which by construction
reads no metrics.

**Class-level test.** For every `ppMeasure` arm that widens a box from a child
(`PP_RAD` index and radicand, `PP_SUP`/`PP_SUB` script, `PP_FRAC` numerator and
denominator, `PP_BIGOP` limits), assert `box.ascent >= -child.relBase +
child.ascent` **and** `box.descent >= child.relBase + child.descent`, over a
table of children that carry descent (a fraction, a parenthesised sum, a
`log`). The `R(8;F(1|2))` shape above is one row and pins 6 against the pre-fix
0.

---

### PP18RR4-8 — EQ9b pins `ppqFitWithEllipsis` in isolation; nothing in the suite drives the `ppqShowRender` branch `PP18RR3-5` actually changed

`packages/pretty-print/prettyTest.c:2938-2960`, anchor `:2946`, against
`packages/pretty-print/prettyEquation.c:992-1007`. **New with `5ce3985b2`.**

**What breaks.** `PP18RR3-5` closed "EQSHW truncates a wide equation with no
marker" by inserting a `ppqFitWithEllipsis` call into `ppqShowRender`'s
non-pretty fallback. EQ9b calls the **helper** directly and asserts its return
value. Revert `prettyEquation.c:992-1007` to the pre-wave one-liner —
`int16_t x = (w < SCREEN_WIDTH - 4) ? centred : 2; showString(src, &standardFont,
x, 94 - 8, vmNormal, false, true);`, deleting the call site entirely — and the
gate is **GREEN**: `1/1 testSuite OK`, `Fail: 0`, `PRETTY-PRINT GATE GREEN`. The
mutation was verified in the built shadow (`build.sim/custom_pkg_shadow/
prettyEquation.c:993`, and the shadow's `ppqFitWithEllipsis` occurrence count
dropped to 1 — the definition alone).

**Why nothing else catches it.** `ppqShowRender` has exactly four drivers:
`prettyTest.c:3010` and `:3046` take the pretty arm; `prettyEquation.c:1034`
(`fnPrettyEqShow`) is driven only by FV20, whose fixture `1/(X+2)` is a
fraction and also takes the pretty arm; and EQ9 (`:3070`) is the only pin on the
fallback, with the fixture `"A+B+"` — roughly 30 px against a 396 px threshold,
so it takes the centred `w < SCREEN_WIDTH - 4` branch and never reaches the
else. The changed branch has zero drivers.

**Violated.** EQ9b's own comment: *"if it is wider than the screen,
showString's NO_LF arm paints past the edge and bitblt24 drops the tail
silently … the honest answer is a marker."* The pin asserts the marker on the
helper's return value; it never asserts that the surface which paints the line
calls the helper. And the round-3 report's own prescribed class test for
`PP18RR3-5`, which this wave did not implement: *"For each surface that paints a
string it did not width-test, assert that the painted string's stringWidth is
≤ the band, or that its last glyph is the ellipsis. Drive it from a table of
stored equations spanning the decline reasons (no fraction, unparseable glyph,
over-wide 2D tree)."* Both the prescribed table and the prescribed surface are
absent. `5ce3985b2`'s claim *"Every pin verified red by mutation"* is false for
exactly the mutation that matters — reverting the fix.

**Consequence.** EQSHW on a long fraction-free equation goes back to painting
past the right edge with `bitblt24` dropping the tail: a truncated equation that
reads as complete, no marker, and nothing goes red.

**Bug class.** A fix split into a helper and a call site, with the pin on the
helper — the wiring is what regressed and the wiring is what is unpinned.

**Class-level test.** The round-3 report already specified it and it is still
the right one: a table of stored equations spanning the decline reasons, driven
through `ppqShowRender` itself, asserting the painted string fits or ends in
`STD_ELLIPSIS`. Add a second row asserting the ellipsis appears in the pixels of
the band, not just in a returned buffer.

---

### PP18RR4-9 — `ppvLiftNeutral`'s written membership test is `SLS_UNCHANGED`, which is false for two of its six members, so the list cannot be re-derived from its own documentation

`packages/pretty-print/prettyVisual.c:809-810` (the stated rule) against the
list at `:812-820`. **Written by `327ec4811`, carried through `7d3c0af31` and
`6e6c2c0ab`.**

**What breaks.** Nothing at runtime — the membership is correct today. The
defect is the rule the wave wrote down to replace an emergent property. Checked
against the item table the package actually builds
(`packages/pretty-print/items.c`, identical to upstream on these rows):
`ITM_NULL` (`:1816`), `ITM_MVAR` (`:3376`), `ITM_PAUSE` (`:1856`) and `ITM_SNAP`
(`:3257`) are `SLS_UNCHANGED`; **`ITM_LBL` (`:1819`) and `ITM_REM` (`:3406`) are
`SLS_ENABLED`.** They are nevertheless lift-neutral in a program because
`executeOneStep` returns before `runFunction` for both —
`lblGtoXeq.c:825` `case PTP_DECLARE_LABEL: return 1;` and `:866-871`
`case PTP_REM: … // just ignore it … return 1;` — so the SLS epilogue at
`items.c:603-611` never fires on them.

**Why it costs something.** `327ec4811`'s stated purpose was to replace an
accident of control flow with a **named list** whose membership a reader can
check. The written test gives the wrong answer for a third of the list it
governs, and it misclassifies a non-empty class: `ITM_42STRING` and
`ITM_42APPEND` take the same early-returning `PTP_REM` arm and are not on the
list.

**Correction against the finder, and it matters.** The finding's proposed
replacement rule — "`executeOneStep` never dispatches it, or its SLS bit is
UNCHANGED" — is also wrong. The full set of PTP arms that return before
`runFunction` is `{PTP_DECLARE_LABEL, PTP_LITERAL, PTP_REM, PTP_DISABLED}`, and
`PTP_LITERAL` (`ITM_LITERAL`, `items.c:1932`, `SLS_ENABLED`) reaches
`_putLiteral`, which does `liftStack(); setSystemFlag(FLAG_ASLIFT);` in every
arm (`lblGtoXeq.c:574-575` et seq.). A literal is emphatically **not**
lift-neutral. The correct rule is narrower than either statement: *the item's
execution, by whatever path, leaves `FLAG_ASLIFT` unchanged.* The comment still
needs correcting; so does the proposed correction.

**Violated.** `prettyVisual.c:809-811`: *"The exceptions are the declaration and
display items, which upstream marks SLS_UNCHANGED and which must leave a pending
lift alone, and ENTER, which is the item that ARMS the latch."* Contradicted by
`items.c:1819` and `:3406`.

**Bug class.** A hand-maintained exception list documented by an oracle that
does not select it — the list is right, the rule that would let the next
maintainer extend it is not.

**Class-level test.** A table pin: for every item in `ppvLiftNeutral`, and for
every item the walker dispatches that is *not* in it, assert the model's
post-step latch matches the machine's `FLAG_ASLIFT` after running that item as a
program step. That derives the list from behaviour instead of from a
paraphrase, and it would have caught `PP18RR4-1` as a side effect.

---

### PP18RR4-10 — `ppSuppressBold`'s justification is false: `FLAG_BOLD` is in `refreshStateFlags[]`, so every pretty paint calls `fnRefreshState()` twice and raises `doRefreshSoftMenu`

`packages/pretty-print/prettyLayout.c:822-824`. **Introduced by `5ce3985b2`,
rewritten by `7d3c0af31`.** Reached independently by two dimensions.

**What breaks.** The comment tells the next reader the flag flip is inert. It is
not. `clearSystemFlag` calls `systemFlagAction` unconditionally
(`flags.c:273-276`), and `systemFlagAction` runs the `refreshStateFlags` loop
**first** (`:77-82`), before the `doInteractionFlags` switch the comment appeals
to. `FLAG_BOLD` is literally an element of that array (`flags.c:65`, between
`FLAG_FGGR` and `FLAG_SIGZEROS`), so the loop matches and calls
`fnRefreshState()` → `doRefreshSoftMenu = true`
(`radioButtonCatalog.c:544-546`). `ppRestoreBold` repeats it via
`setSystemFlag`. No package overrides `flags.c`, and the package's `defines.h`
keeps `FLAG_BOLD` at `0x8069`, so there is no build-level escape.

**Reaching input (measured).** A probe in M8's own BOLD render:
`doRefreshSoftMenu = false;` → `prettyTryRegisterLine(REGISTER_X, …)` → assert
it stayed false. Gate **RED**, one failure, mine:
`AUDIT-PROBE R4: pretty paint under FLAG_BOLD raised doRefreshSoftMenu`
(`testlog.txt:310`), probe verified present in the built shadow at
`custom_pkg_shadow/prettyTest.c:321`.

**Consequence, stated honestly.** For a paint that happens **inside**
`refreshScreen` the bit is inert: `screen.c:6222` is evaluated before
`_refreshNormalScreen` paints and `:6304` clears the flag afterwards, and the
package's full-screen surfaces set `TI_SHOWNOTHING`, which makes `SHOWMODE` true
and blocks the `:6222` consumer. The path that escapes is a **direct**
`refreshRegisterLine` outside `refreshScreen`, and the cleanest one is upstream's
own: `packages/pretty-print/keyboard.c:2135-2141` in `btnReleased` sets
`SCRUPD_MANUAL_MENU` (explicitly asking the menu not to be repainted), then
forces `temporaryInformation = TI_NO_INFO` and calls
`refreshRegisterLine(REGISTER_T)` under a guard that already pins
`calcMode == CM_NORMAL && !SHOWMODE` — exactly the state
`prettyTryRegisterLine` requires. With BOLD set, that paint raises
`doRefreshSoftMenu`, it survives to the next `refreshScreen`, and `:6222`
revokes the `SCRUPD_MANUAL_MENU` the caller had just set: a softkey-band clear
and full menu repaint the caller asked to suppress. That is flicker and wasted
refresh time, not a wrong result, which is why this is filed low.

**Correction against the finder.** Its four cited `keyboard.c` escape sites are
all NIM contexts, where `prettyTryRegisterLine` bails at `calcMode != CM_NORMAL`.
The conclusion is right for a reason the finder did not identify.

**Violated.** `prettyLayout.c:822-824`: *"Restored on the single exit path of
each wrapper; there is no early return between save and restore, and
systemFlagAction has no FLAG_BOLD arm, so this is a bit flip with no side
effect."* The first two clauses are correct and were verified. The third is
contradicted by `flags.c:65` and `flags.c:77-82`.

**What is NOT wrong.** The save/clear/restore shape itself is upstream's own
convention — `browsers/fontBrowser.c:117-128` does the identical dance and pays
the identical cost — and the coverage claim on the line above ("every glyph goes
through here or `ppRenderRightAligned`") was checked and holds. The defect is the
justification, which is what a future reader will rely on when adding a third
paint entry or a second suppressed flag. `doRefreshSoftMenu` is the one bit that
**overrides** `SCRUPD_MANUAL_MENU` (`screen.c:5813`, `:5844`, `:6222`), which is
the protection every self-painted surface in this package uses.

**Bug class.** A comment that clears a side effect by naming the one mechanism
it checked, when an earlier mechanism on the same call path fires first.

**Class-level test.** Not a pin on this comment — a pin on the class: for every
system flag the package writes during a paint, assert `doRefreshSoftMenu`,
`screenUpdatingMode` and `temporaryInformation` are unchanged across the paint.
One assertion, and it goes red today.

---

### PP18RR4-11 — the new `x1` clamp gates the doubled glyph's left twin, so a column landing exactly on `SCREEN_WIDTH` loses its on-screen pixel at 399

`packages/pretty-print/screen.c:1278`. **Introduced by `5ce3985b2`
(`PP18RR3-3`).**

**What breaks.** `PP18RR3-3` wrapped four `setPixel` calls in
`if(x1 < SCREEN_WIDTH)`. `x2` is `x1` decremented when non-zero
(`screen.c:1269-1273`), so `x1 == 400` gives `x2 == 399` — on screen, and now
unreachable, because the whole block is skipped. The comment scopes the guard to
the wrapped-negative case, and for that case `x2 = x1-1` is equally huge and
equally excluded; the only state the outer guard changes beyond its own scope is
`x1 == SCREEN_WIDTH` with `x2 == SCREEN_WIDTH-1`.

**Reaching input (constructed against the parsed font tables).** Not the
register line — right alignment pins the string's end at 400 and every HP digit
has truncation deficit 0, so the rightmost ink column sits at 389. The reachable
path is the **NIM line's angular-unit glyph**: `displayNim`'s fit test reserves
16 px for the cursor and nothing for the unit glyph it then draws at
`xCursor + 16` (`screen.c:5535`, `:5544`). Type `123 456 789.123 45` in degrees
mode with a real in X: measured width 373 ≤ 384, so the numericFont branch is
taken, `xCursor = 373`, and `STD_DEGREE` (cb 4 / cg 10 / ca 2) is drawn at 389.
Its column 2 gives `x1 = 389 + ((15*6)>>3) = 400`, `x2 = 399`; column 1 gives
398/397. `numDouble` is true (numericFont + checkHP + `TI_NO_INFO`), so pre-wave
the on-screen lit columns were {395,396,397,398,399} and post-wave they are
{395,396,397,398} — and the glyph's own 30 px pre-clear at 389 blanks 399 first,
so it goes dark. A sweep over near-default NIM configurations found 81 hits of
this shape, so it is not a knife-edge coincidence.

**Consequence.** One pixel column at the right edge of a glyph that straddles
x=400, in HP layout mode, on both the simulator and the device. Cosmetic. I
could not make it visible on a real screen and am reporting it as the off-by-one
it is rather than as an observed defect.

**Violated.** The fix's own comment, `screen.c:1275-1277`: *"y is recovered from
its wrap and clamped two lines up; x is not. A negative x is a huge uint32,
which the simulator HALs reject and the device ROM's bitblt24 does not."* The
guard for `x2` is already written separately two lines down, which is the shape
the whole block should have had.

**Two dead tests at the same site, folded in here rather than filed
separately.** `x2 <= x1` always, so both added `&& x2 < SCREEN_WIDTH` conjuncts
(`:1283`, `:1288`) are unfalsifiable inside `if(x1 < SCREEN_WIDTH)`. They cost
nothing at runtime and two modified upstream lines at rebase time (§2). The
`&& x1 + 1 < SCREEN_WIDTH` conjunct at `:1281` is **not** dead and is correct.

**Bug class.** A bounds guard hoisted to cover a block whose members have
different bounds.

**Class-level test.** Enumerate the ink primitives' operands (`x1`, `x1+1`,
`x2`) and assert each is guarded on **its own** value, by construction — a
source-level check, since no pixel test can distinguish "clipped correctly" from
"clipped one column too far" on a HAL that discards out-of-range writes anyway.

---

### PP18RR4-12 — the `PP18RR3-3` clamp was applied to `showGlyphCode`'s `setPixel` exit and not to the glyph pre-clear on the same wrapped `x`, three statements above

`packages/pretty-print/screen.c:1238-1240` (the `lcd_fill_rect` pre-clear),
against the clamp at `:1278`. **Incomplete fix from `5ce3985b2`.**

**What breaks.** `showGlyphCode` has two writes that take the caller's `x`. The
wave clamped one. The pre-clear at `:1239` runs
`lcd_fill_rect(x, max(0, yy), …)` with `x = (uint32_t)(int32_t)` of a possibly
negative coordinate, whenever `noPreClear` is false — which `_doShowString`
(`screen.c:1398`) always passes for `showString`.

**Reaching input.** A history row containing a short radicand (the raised-glyph
form, `synth` false — any formula with `√2` in it) that is wider than the
screen. PHIST, select it, press `.d` to pan until the radical scrolls off the
left: `browsers/prettyBrowser.c:113` sets `x = (int16_t)(8 - pbPan)`, negative
once `pbPan > 8`; `prettyLayout.c:666` then paints the radical sign with
`showString("\xa2\x1a", m->font, signX, …)` where `signX = x + relX - signW`.
Every other painting primitive in `prettyLayout.c` screens this: `ppFillVal`
clips all four edges (`:502-523`, the `R3-11` fix), `ppDrawLine` clips both axes
(`:578-582`), and `ppShowRun` passes `noPreClear = TRUE` precisely to avoid this
call. The radical-sign `showString` is the one that does not.

**Consequence.** On both simulator HALs `lcd_fill_rect` early-returns
(`endX > SCREEN_WIDTH`), so the gate cannot see it — the same blindness
`PP18RR3-3` was filed under. On device `lcd_fill_rect` is the DMCP ROM entry
with no bounds contract, so the pre-clear rectangle lands at a wrapped byte
index: corrupted rows while panning a history row that contains a radical.
**Device-only, unverifiable here**, and stated as such.

**Violated.** The fix's own comment, quoted in full under `PP18RR4-11`, states
the rationale as a property of the function — *"Clamp x where y is already
clamped"* — not of one of its two writes. Nothing in DESIGN.md, DESIGN-HISTORY
or the six archived reports rules the pre-clear exempt: the design's statements
about this `showString` call are about clearing **extent** (`DESIGN.md:106-116`,
the radical-sign exception) and paint **order** (`:58-76`), not coordinate wrap.
The one negative-x clearance on record (DESIGN-HISTORY `R3-13`, *"Negative x and
y are upstream's own convention at this call … verified against both simulator
HALs"*) is scoped to `ppShowRun`, which passes `noPreClear = TRUE` and never
reaches the pre-clear — and is the exact reasoning `PP18RR3-3` overturned.

**Bug class.** A class fixed at the site where it was noticed, with the sibling
site in the same function left standing. Third consecutive round to produce this
shape; the round-3 verdict named it as the pattern to carry.

**Class-level test.** Enumerate every call in `screen.c`'s glyph path that
consumes the caller's `x` as a `uint32_t` (`lcd_fill_rect` at `:1239`, the four
`setPixel`s at `:1279-1290`) and assert each is reached only with an in-range
value — a source-level enumeration, plus the existing T29 pan driver
instrumented with a counter on both HAL entry points, which is how `PP18RR3-3`
was measured in the first place.

---

### PP18RR4-13 — T23c never asserts that slot 4 was known before the STO, and `ppcTestSlotRaw` returns `PPC_NIL` out of range — which the pin accepts as "degraded"

`packages/pretty-print/prettyTest.c:1333-1365`, anchor `:1360`, against
`packages/pretty-print/prettyCapture.c:1270-1272`. **New with `e81677309`
(`PP18RR2-6`).**

**What breaks.** The pin's condition is
`if(slot4 != PPC_UNKNOWN && slot4 != PPC_NIL) fail`, and `ppcTestSlotRaw`
returns `PPC_NIL` for any index past the array. Nothing between
`ppcTestType("5")` (`:1350`) and the `ppcTestOpParam(ITM_STO, REGISTER_A)`
(`:1354`) checks that slot 4 held a tree. So the pin passes both when the guard
works and when the slot was never populated at all.

**Measured, both directions.** Reintroducing the defect
(`prettyCapture.c:766` reverted to the pre-fix `(t - REGISTER_X) <=
ppcTopSlot()`) turns the gate **RED** with T23c as the **sole** failure — so it
is live today and it is the only detector of `PP18RR2-6`. Then clamping
`ppcShiftUpForLift` so slot 4 is never populated under SSIZE8 — the exact
regression the pin's own headline forbids, *"a slot must be maintained wherever
its register is writable"* — leaves the gate **GREEN**, T23c passing with
`slot4 == PPC_UNKNOWN` and nothing else in the battery noticing that A..D
stopped being shadowed.

**The `PPC_NIL` disjunct can only ever mean "slot 4 does not exist".** Every
in-range write to `ppcSlot` normalises NIL to UNKNOWN (`prettyCapture.c:282`,
`:636`, `:647`, `:782`, `:964`, `:1064`), so no live slot can read back
`PPC_NIL`; the value is reachable only through the out-of-range arm of
`ppcTestSlotRaw`. The comment's *"(or emptied)"* corresponds to no real state.

**Scope correction against the finder.** Its consequence is overstated. The
stale-tree-after-STO defect cannot come back unseen, because reintroducing it
requires slot 4 to be populated, and the mutation shows T23c then reddens alone.
What goes unseen is the weaker half — the mirror silently ceasing to maintain
A..D — which `DESIGN.md:167-168` already tolerates (*"over-invalidation only
costs history granularity"*). Test hygiene, not a live correctness hole.

**Violated.** The file's own repeatedly-stated fixture rule, written at B10:
*"A fixture must prove it reached the state it claims to test."* Its **sibling
in the same commit** does exactly that — `prettyTest.c:1282`: *"T22b setup: Y is
not complex after the lift — fixture cannot reach the defect"* — and
`e81677309`'s message records that T22b needed two such corrections before it
could fail. T23c got none. `grep -rn T23c` across the repo hits only
`prettyTest.c`; no ruling exempts it.

**Bug class.** A pin whose acceptance set includes both "the property holds" and
"the property is not applicable", with no setup assertion to separate them.

**Class-level test.** Every capture-side slot pin should open with a positive
setup assertion (`ppcTestSlotRaw(k) != PPC_UNKNOWN && != PPC_NIL`) before the
operation under test, and `ppcTestSlotRaw`'s out-of-range return should be a
distinct sentinel from any legal slot value so a pin cannot silently accept it.

---

### PP18RR4-14 — `PPV_STACK_SLOTS` still documents itself as "SSIZE8 regardless of the flag", which this wave's own SSIZE4 fix falsified

`packages/pretty-print/prettyVisual.c:49`.

**What breaks.** A reader's model. `#define PPV_STACK_SLOTS 8 ///< SSIZE8
simulated regardless of the flag` is directly contradicted 136 lines below by
`ppvLiveStackSlots`' banner, added by this wave: *"The array is always eight
wide; only the effective depth follows the flag, so the mirror drops its bottom
entry where the hardware does"* (`:185-188`). `ppvPush` (`:201-215`) and
`ppvRefillFromT` (`:836-847`) both read the live count.

**Git makes the attribution exact.** `git log -S 'SSIZE8 simulated regardless of
the flag'` returns `80e6d276e` only — the pre-wave stage commit — and the line
has never been edited. `git log -S 'ppvLiveStackSlots'` returns `327ec4811`,
inside this range, whose own message names *"The drop threshold followed a
compile-time 8 rather than the live stack depth"* as the bug. And the sweep did
not consider and keep it: `git show 7d3c0af31 -- prettyVisual.c | grep '^@@'`
starts its first hunk at line 80, so line 49 was never inside any hunk of any of
the three comment commits. Missed, not ruled.

**The file already contains a site written to the false comment.** `ITM_FILL`
(`:916-919`) still expands `PPV_STACK_SLOTS` as the model **depth**, with no
`ppvLiveStackSlots` clamp — which is the second half of `PP18RR4-4`. The next
reader deciding whether that is right is told by line 49 that the flag does not
matter.

**Violated.** CLAUDE.md's comment discipline (*"cut narration … keep
invariants"*) and the three sweep commits' own stated purpose, *"the VISUAL
set's comments say what the code does"*. The comment states a retired invariant
as a current one.

**Bug class.** A constant whose two meanings (array width, model depth) were
split by a behaviour change, with the doc comment left on the retired meaning.

**Class-level test.** Not a test — a grep. When a fix splits a constant's
meaning, the constant's own comment is part of the fix's blast radius; the
mechanical form is `git log -S` on every identifier a behaviour change touches,
run before the comment sweep rather than after.

---

## 4. PLAUSIBLE

Survived scrutiny; nobody constructed the reaching input, or the two readers who
tried produced opposite traces. One this round.

### PP18RR4-P1 — the new menu arm may fire for upstream's own self-painted screens, because "did not set `SCRUPD_MANUAL_MENU`" is not consent when `showSoftmenu` clears that bit as bookkeeping

`packages/pretty-print/screen.c:5878`, the same line as `PP18RR4-5`, but a wider
claim: that the arm reaches **upstream** surfaces that never heard of
pretty-print.

**The candidate path.** Put a real matrix in X and press SHOW. `fnC47Show`'s
NOPARAM arm calls `showSoftmenu(-MNU_SHOW)` (`src/c47/display.c:3421`), and
`showSoftmenu` does `screenUpdatingMode &= ~(SCRUPD_MANUAL_MENU |
SCRUPD_SKIP_MENU_ONE_TIME)` (`packages/pretty-print/softmenus.c:3971`). The
`dtReal34Matrix` arm then clears the softkey area, draws the matrix full-screen,
draws the separator at `Y_POSITION_OF_REGISTER_T_LINE-4`, and sets
`temporaryInformation = TI_SHOWNOTHING` (`display.c:3948-3952`). The next
`refreshScreen` enters `_refreshNormalScreen` with `CM_NORMAL`, mode ≠ AUTO,
`TI_SHOWNOTHING` and `MANUAL_MENU` **clear** — the new arm's three conjuncts —
and would paint the SHOW softkey row and the status bar over a screen upstream
drew itself.

**Why it is not CONFIRMED.** Two dimensions traced it to opposite conclusions
and neither measured it. The contracts reader cleared it: `RETURN_NORMAL`
(`screen.c:6054-6055`) ORs `MANUAL_MENU` back in on every pass, so by the time
the matrix screen is refreshed the bit is set and the arm does not fire. The
upstream reader raised it: `showSoftmenu` clears the bit **after** that, on the
SHOW keypress itself, so the ordering the clearance depends on may not hold. I
verified both mechanisms exist and could not settle which one wins without
running it — and `PP18RR4-5` is the cautionary precedent, because there the
"`RETURN_NORMAL` re-ORs it" argument was the one that failed, for exactly this
reason (`popSoftmenu` clears the bit later than the re-OR).

**What would settle it.** One probe, and the harness can drive it: put a
`dtReal34Matrix` in X, run `fnC47Show`, then `refreshScreen`, and sum ink in
rows 217..239 and 0..19 — the same instrument `PP18RR4-5` used, which produced
unambiguous numbers there. If the bands ink, the arm's predicate must be
replaced by a package-owned condition (`screenHoldsDrawnPixels`, or a
pretty-print latch) so it diverts only the package's own case, which is the
"early-return divert" idiom the upstream-diff-review skill names.

### Raised, never refuted — where each went

Six findings fell beyond the verification cap. None is filed as CONFIRMED and
none is lost:

- **`ppvLiftNeutral` as a hand-maintained duplicate of the skip-arm list.** Same
  site and same facts as `PP18RR4-9`, raised independently by the design
  dimension; folded in, and its `1 2 ENTER REM 3 + +` trace is the reason
  `PP18RR4-9`'s class-level test is written as a table over items.
- **The `showGlyphCode` clamp re-indents eight upstream lines** (12/0/2 → 36/11/4).
  Not a runtime defect and not this audit's discovery — the churn scanner
  reports it, so it is in §2 and `PP18RR4-D5`, per the rule against re-reporting
  the mechanical half.
- **Two of the three genuinely-modified upstream lines carry a dead condition.**
  Verified directly (`x2 <= x1` always) and folded into `PP18RR4-11`, whose site
  it is.
- **The new menu paint omits upstream's paired `lcd_refresh_dma()`.** REFUTED by
  the refutation pass on a stronger trace than the finder had; see §6.
- **DESIGN.md's §6 adjacency table still says `screen.c` is ONE hunk.** Verified
  directly by this pass (four hunks, and forth-core edits the same function);
  reported in §2 and filed as `PP18RR4-D5` rather than as a finding.
- **The matrix-SHOW reach**, above, as `PP18RR4-P1`.

---

## 5. Design observations (D7)

Shape, not defects. Six; the first is why this round exists and the second is
the one that will still matter in six months.

**`PP18RR4-D1` — the fix-regression statistic held, and it concentrated in the
fix that was written to be structural.** Eight of fourteen findings are the
wave's own work. But the distribution is not uniform: `327ec4811`, the one
commit that changed a *shape* rather than a line, produced `PP18RR4-1`
(wrong drawing), `-4` (the invariant installed at one of two producers), `-5`
(the guard tested one frame late), `-9` and `-14` (the rules it wrote down being
false). `5ce3985b2`, which fixed eight findings one line each, produced `-6`,
`-7`, `-8`, `-10`, `-11` and `-12` — all pins and clamps, none a wrong drawing.
The project's memory says "relocating state is the most dangerous fix shape";
this round says relocating *control flow* is worse, because the arms that were
relocated all looked alike and one of them was not a step.

**`PP18RR4-D2` — when a fix replaces an emergent property with a written rule,
the rule becomes the artifact under audit, and this wave's rules are less
reliable than its code.** `327ec4811`'s stated achievement is that "the
exceptions are a named list rather than an emergent property of control flow".
The list is correct; the rule that selects it is false for two of six members
(`PP18RR4-9`). The bold suppression is correct; its stated justification is
false (`PP18RR4-10`). The stacked-power precondition was true and written on the
declaration; the comment cap deleted the sentence and left two of three callers
violating an unstated contract (`PP18RR4-3`). The stack model became
flag-dependent; the constant's doc comment still says it is not (`PP18RR4-14`).
And all three comment commits are **mechanically perfect** — I verified the
comment-only claim a fourth time over 37 file/commit pairs and every one is
byte-identical after stripping. That is the observation: "comment-only" proves
the compiler sees no change, and says nothing about whether the comments are
true. A comment sweep is a semantic edit to the only machine-unreadable part of
the source, and it needs a review pass of its own — which this wave did not get,
because the mechanical proof reads like one.

**`PP18RR4-D3` — four of the wave's own assertions are one edit from vacuity,
and two of its fixes have no assertion at all.** Measured, not inferred:
deleting the `PP_RAD` descent dual leaves the gate green; reverting the EQSHW
fallback to its pre-wave form leaves the gate green; one ambient flag makes M8
certify nothing; one clamp makes T23c certify nothing. `5ce3985b2` states "Every
pin verified red by mutation" and `327ec4811` states "All three verified red by
mutation before being trusted" — both are true of the mutation the author ran
(disable the fix's *effect*) and false of the mutation that matters (revert the
fix's *call site*, or perturb the fixture's ambient state). The protocol this
project already has — pair every green mutation with a run proving the harness
*can* go red — needs its dual: pair every red-verified pin with a run proving it
is red for the right reason.

**`PP18RR4-D4` — nineteen fixes, zero design entries.** The only
`design-docs/pretty-print/` change in nine commits is the `ppfCombine` →
`ppfBuildOp` rename. The mode-dependent stack model, T replication, the
`saturated` latch, the eRPN ENTER rule, the ruling that BOLD must still display,
`PPN_VAL2`'s two-node value leaf and the EQSHW ellipsis are all new binding
behaviour, and all of them live only in commit messages and code comments —
which the same wave then rewrote three times. `grep -rni
"saturat\|replicat\|SSIZE\|PPN_VAL2" design-docs/pretty-print/` returns nothing.
Two of this round's findings (`-4`, `-14`) exist in the gap that leaves: there
is no document to check the FILL arm against.

**`PP18RR4-D5` — the upstream surface roughly tripled and the authoritative
inventory did not move.** `010-screen.c.patch` went 13/0/2 → 36/11/4 and the
churn scanner from 1 finding to 9. `DESIGN.md:783` still reads *"`screen.c` |
ONE hunk: the §6 inline arm at :3936 | forth-core's hunks (:3, :814-:934, :1159,
:5662, :5927) and undo-history's are all far away."* There are four hunks;
pretty-print's new `:1272` hunk is **inside `showGlyphCode`**, the function
forth-core patches at `:1159`, so two packages now modify one hot render
function; the new `:5872` hunk sits between forth-core's `:5672` and `:5927`;
and the pre-existing `:6178` hunk is three lines from undo-history's `:6152`,
which "all far away" has never described. This is the same C-4 class the
2026-08-27 minimality review called "the most consequential item in this
review", against the same table, one wave later.

**`PP18RR4-D6` — every fixture the wave added agrees with the bug it did not
find.** V72/V75/V76 put ENTER before the XEQ, never at the end of the callee
(`PP18RR4-1`). T22b asserts the tree is not poisoned, never that the number is
right (`PP18RR4-2`). V36b drives a program that fits the Z/T band, so it cannot
distinguish the two VISUAL arms (`PP18RR4-5`). M8 uses a mixed number, so it
depends on a flag it never sets (`PP18RR4-6`). EQ9b calls the helper, not the
surface (`PP18RR4-8`). T23c omits the setup assertion its sibling in the same
commit has (`PP18RR4-13`). This is the same-level coverage lesson the project
recorded after PP18 stage work — *a pin written from the example that produced
the bug agrees with the bug's neighbours* — and the remedy is the one every
class-level test above is written in: enumerate the class from the code (the
arms that nest, the nodes that continue, the surfaces that arm the protocol, the
builders with preconditions) and assert over the enumeration, rather than
writing one more good fixture.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. Mandatory section, and this round it carries most of the good
news: the two fixes the brief named as the highest-value targets — the eRPN
ENTER rule and the SSIZE4 / T-replication model — were re-derived from upstream
by four dimensions independently and by direct execution, and they are exact.

### Killed by the refutation pass

**"The copied `showSoftmenuCurrentPart()` omits the DMCP `lcd_refresh_dma()`
upstream pairs it with at the only other menu-paint site in the same
function"** (`screen.c:5879`). **REFUTED.** The asymmetry is real — the new
block genuinely lacks the inline push, and the sibling at `:6032` has it with
upstream's own "presses are missed. Not sure why." comment — but the consequence
is not what the device does. The package ships no `c47.c` override, and the C47
main loop calls `lcd_refresh_dma()` unconditionally for every key event
(`src/c47/c47.c:1201-1203`, `if(key >= 0) { lcd_refresh_dma(); }`) a few returns
after `btnPressed`/`btnReleased` → `refreshScreen` → `_refreshNormalScreen`
returns, in the same loop iteration, before the next key is read. VISUAL is an
item, so `key >= 0` holds. For non-key triggers the periodic block
(`c47.c:1246-1253`) pushes within `SCREEN_REFRESH_PERIOD` (160 ms), which caps
the wait. The "presses may be missed" half is wrong mechanically: softkey
dispatch reads the softmenu stack in RAM, not pixels. And the regression
direction is inverted — upstream at this site paints **nothing** and jumps
straight to `RETURN_NORMAL`, so a merely-delayed push is strictly more than the
baseline it replaced.

**"`ppqFitWithEllipsis` has no guard for a trailing truncated multi-byte glyph,
unlike `ppqPeek` in the same file"** (`prettyEquation.c:947`). **REFUTED**, and
on four independent grounds. Granting the malformed input, the read past the end
is already committed by upstream code on the same bytes before the guard could
run: the caller's own `stringWidth(src, …)` one statement earlier
(`prettyEquation.c:993`) goes through `_calculateStringWidth`
(`charString.c:250`), which does `charCode = (charCode<<8) | str[ch++]` with no
length check. The claimed consequence is wrong in the likeliest sub-case —
`pos` lands one past the terminator, the loop exits, `if(src[pos] != 0)` is
false, and the ellipsis is **omitted**, not planted mid-string. For any
well-formed string the loop provably cannot reach the terminator at all (the
per-glyph width sum is ≥ the whole-string width, and the budget is strictly
less, so the exit is always the budget test) — the guard is dead on the
production path by construction. And there is no producer: equation text is
stored verbatim from `aimBuffer`, whose edits are glyph-aligned at every site
(`keyboard.c:4283`, `:4326`, `bufferize.c:2754`, `addons.c:1005`). The
sibling-inconsistency argument does not carry either: `ppqPeek` guards because it
is a length-delimited acceptor whose contract is to **decline**, and it does
decline this input before the fallback runs; `ppqFitWithEllipsis` reproduces
upstream's own packer shape (`solver/equation.c:657`,
`strPtr += ((*strPtr) & 0x80) ? 2 : 1;` inside `while((*strPtr) != 0)`), which
carries the same non-guard.

**"The new early-return menu repaint drops the `SCRUPD_SKIP_MENU_ONE_TIME`
conjunct that every other menu gate in the file carries"** (`screen.c:5878`).
**REFUTED**, and the refutation is stronger than "the bit is never set". The bit
*does* have a live producer the finder missed — `screenUpdatingMode =
~SCRUPD_AUTO` at `softmenus.c:3803/3808/3833/3838`, which on a `uint8_t` stores
`0xFF` — but that same store also sets `SCRUPD_MANUAL_MENU`, so the new guard is
already false and the early return behaves byte-for-byte like upstream's
compound gate. Every `0x04`-clearing site reachable between that producer and
line 5878 either clears `0x40` in the same statement or is gated on a `calcMode`
the enclosing `CM_NORMAL` test excludes, and both exits of
`_refreshNormalScreen` re-set `0x04` at `RETURN_NORMAL`. The 0x40-set /
0x04-clear combination the finding needs cannot be produced.

**"EQ4c's 'same shape' assertion compares only signature LENGTHS, which are
equal by construction whenever both forms parse"** (`prettyTest.c:2927`).
**REFUTED by mutation, and this is the round's cleanest kill.** A change that
lets both forms parse but gives the plus form a different tree shape (mutating
`ppqNumber`'s ·₁₀ tail loop) turned EQ4c **red on that assertion alone** — the
two parse assertions stayed silent, because the signature grew from 9 chars to
12 when the exponent left the number run and became a separate HBOX child. So
the third line is a live shape-parity pin, not dead weight. The residual half
of the finding — that a regression rewriting the plus glyph to a minus would
pass — is true only of code that does not exist and cannot naturally arise:
`ppqNumber` takes no per-glyph decision, it hands `c->s + start` and a byte
count to `ppNewRun`, which `xcopy`s the source bytes verbatim; there is no
"minus arm" to reuse; and `0xa16a`/`0xa16b` appear nowhere in the parser except
side by side inside `PPQ_IS_SUP`. The finding's own prescribed remedy is
self-defeating: EQ4b-parity means "normalise the one legitimately-different
character, then `strcmp`", and here that character *is* the sign, so the
normalisation maps the hypothesised regression onto an identical string and
passes too.

**"V-MODE and V-MODE4 call themselves oracles and quote the machine's return
values, but never run the program"** (`prettyTest.c:4082`). **REFUTED by
execution.** Both programs were run on the machine under all four flag
settings inside the harness: `err=0`, and X = **7, 8, 15, 16** — exactly the
values the two pins' comments claim and exactly the pictures they assert. The
finding's prescribed remedy is also wrong: VMEN and VM4 are all-long-integer
arithmetic and leave X as `dtLongInteger`, so adding them to V65's evaluation
list trips V65's first guard (`getRegisterDataType(REGISTER_X) != dtReal34` →
fail). And its consequence has no constructible input: `ppvTestExpect` `strcmp`s
the transpiled string, so a byte-identical string is a same-meaning string, and
the finding's own nominated mutation (`ppvRefillFromT` shifting `ast[1]`) turns
the pin red, which it concedes. What survives is a "could be stronger"
observation, not a defect.

**"The new menu repaint calls a function that resets `screenUpdatingMode`, so
VISUAL's drawing can be erased on the next refresh"** (`screen.c:5878`).
**REFUTED.** The path exists — `showSoftmenuCurrentPart` →
`setScreenUpdateFromMenu` assigns `screenUpdatingMode = SCRUPD_AUTO` for six
menu ids — but the conclusion does not follow: the repaint is followed
immediately by `goto RETURN_NORMAL`, and that label unconditionally ORs
`SCRUPD_MANUAL_STATUSBAR | SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU` back in
before `_refreshNormalScreen` returns. `MANUAL_STACK` is restored microseconds
later in the same call, the mode is `0x17` on exit, and the next refresh takes
the same early return. Nothing between the AUTO assignment and the label can
repaint rows 20..91 either: `showSoftmenuCurrentPart`'s only clear is the
softkey band and `refreshStatusBar` paints rows 0..19. The finding's secondary
half is inert for a stronger reason than "unreached": `refreshStatusBar`
self-gates, returning immediately in `CM_NORMAL` whenever
`SCRUPD_MANUAL_STATUSBAR` is set (`statusBar.c:783-802`), so a future
status-bar-owning surface is already protected regardless of which bit the call
site tests.

### The wave's fixes, re-derived rather than trusted

- **`PP18RR1-2`, the eRPN ENTER arm.** Checked line by line against
  `fnKeyEnter` (`keyboard.c:3417-3442`) by four dimensions: under `PGM_RUNNING`
  the dup branch is taken in **both** modes (the third disjunct), then eRPN sets
  `FLAG_ASLIFT` and classic clears it. `stk->liftDisabled =
  !getSystemFlag(FLAG_ERPN)` after an unconditional `ppvPush` is exactly that.
  V-MODE's oracle pair (7 vs 8) is the right pair and the machine returns it.
- **`PP18RR1-3`, the SSIZE4 model and T replication** — the brief's named
  highest-value target. `ppvRefillFromT`'s shift direction was re-derived from
  `_Drop` (`stack.c:41-59`) independently by three dimensions: `ast[0]` is T,
  the loop shifts up from `i = depth` and leaves `ast[0]` duplicated, which
  reproduces "the top register keeps its value while everything below pulls
  down" exactly. `1 2 3 4 5 + + + +` under SSIZE4 was hand-traced to
  `2+(2+(3+(4+5))) = 16` and executed to 16; the eight-deep chain and
  DROP/DROPy were traced too (`[T,Z,Y,X] --DROP--> [T,T,Z,Y]`,
  `--DROPy--> [T,T,Z,X]`, both matching hardware). `ppvLiveStackSlots` reads
  `getStackTop()` live (`defines.h:2284` is a macro over the flag), so V-MODE4
  is not vacuous on that axis. The one hole is the arm that bypasses it
  (`PP18RR4-4`).
- **`PP18RR1-1`, the PP_RAD descent dual.** Arithmetic checked and consistent
  with the ascent line two lines above (same sign convention, can only grow the
  box), and it cannot regress the paint, because `ppPaint`'s `PP_RAD` arm
  computes `yBot` from the **child's** descent, not the node's. Correct as
  written; only its coverage is a finding.
- **`PP18RR3-6`, the comma radix.** Upstream's tokenizer counts `,` toward
  `numericCount` beside the digits and `.` (`solver/equation.c:1543-1545`) and
  `_parseWord` rewrites every `,` in a numeric token before `stringToReal34`
  (`:1191`), so `1,5` **is** 1.5 and the widening matches the producer. Checked
  for a competing meaning: bigop separators are `;` and `ppqFunctionCall` takes
  one argument, so there is no separator role for `,` in this grammar.
- **`PP18RR3-11`, `PPQ_IS_SUP` admitting `STD_SUP_PLUS`.** Both consumers
  (`prettyEquation.c:104`, `:625`) only advance `c->pos` and copy the matched
  span verbatim into a `PP_RUN`; nothing subtracts `0xa160` to recover a digit,
  so the widened acceptor cannot mint an out-of-range index. The arms that *do*
  decode a superscript to a digit live in `prettyValue.c` and keep their own
  narrower `PP_IS_SUP_DIGIT`. The classic "acceptor widened, decoder not" shape
  does not apply.
- **`PP18RR3-5`'s buffer and width arithmetic.** `o + n + sizeof(STD_ELLIPSIS)
  >= cap` reserves the terminator as well as the marker, so the trailing
  `strcat` cannot pass `cap-1`; both callers pass 256-byte buffers. The width
  side errs safe: per-glyph widths measured with `showLeadingCols = true` sum to
  at least the whole-string width measured without them, so the fit is
  conservative.
- **`PP18RR3-1`'s coverage claim.** *"Every glyph goes through here or
  `ppRenderRightAligned`"* — verified by enumerating every `ppPaint` entry:
  `ppPaint` is static with exactly two external entries, both wrapped, and every
  direct `showString` in the package uses `&standardFont` except the radical-sign
  glyph at `prettyLayout.c:666`, which is *inside* `ppPaint` and therefore inside
  the suppression. `showGlyphCode`'s bold substitution is gated on
  `font == &numericFont`, so the standardFont calls cannot substitute. The claim
  holds; only its side-effect justification does not (`PP18RR4-10`).
- **`PP18RR2-10`'s serializer half.** `prettyCapture.c:369-381` walks `child[0]`
  and rebuilds one flat TKV, with a comment naming the exact hazard — *"copying
  `bytes` straight out would read past the array"*. That half is right, which is
  what makes `PP18RR4-2` an asymmetry rather than a misunderstanding.
- **The rename.** Complete: every call site in `prettyFormula.c`,
  `prettyVisual.c` and `prettyInternal.h` moved together, FV18's comment was
  updated with it, `grep -rn ppfCombine` over the package returns nothing, and
  DESIGN.md was renamed in the same commit while DESIGN-HISTORY and the audit
  reports keep the old name deliberately with a trail entry saying so. No pin
  depends on the identifier.

### Guards and conjuncts, falsified or proved load-bearing

- `ppvLiveStackSlots`' two clamps (`n < 1`, `n > PPV_STACK_SLOTS`) are
  unfalsifiable — `getStackTop()` returns `REGISTER_T` or `REGISTER_D`, i.e.
  exactly 4 or 8. Defensive noise, not a defect. `ppvPush`'s
  `(uint8_t)(slots - 1)` cannot underflow for the same reason.
- `ppvPushLifting`'s `stk->depth > 0` conjunct is also unfalsifiable:
  `liftDisabled` is only armed by ENTER, which requires depth > 0 and pushes.
- `saturated` is initialised at both of the two `ppvStack_t` instances
  (`:430`, `:1207`); there is no uninitialised-latch path. `ppvBody`'s
  `sub.saturated = false` is **not** a third `ITM_FILL` — it seeds through
  `ppvPush` in a loop, so it latches correctly and renormalises under SSIZE4.
- The now-redundant `stk->liftDisabled = false;` in the `ITM_PGMINT` and
  `ITM_PGMDRV` arms is idempotent (both items are `SLS_ENABLED` and neither
  walks a nested program), so the epilogue's second clear is harmless. Their new
  comments say "clear BEFORE the nested walk, see ITM_XEQ" and there is no
  nested walk in either arm — a dead statement plus a pointer at machinery that
  is not there. Noted, below the bar as a defect, and worth deleting when
  someone is in the file.
- `ppvBody`'s five failure returns do not decrement `ctx->bindingCount`. Every
  one sets `ctx->failed` first, which aborts the whole walk before any later
  binding lookup, so the leak is unobservable. Pre-existing.
- `ppvAstToNodes`' `PPA_CONSTRUCT` arm recursing into all four operands before
  checking any is bounded, not exponential: OP1/OP2 latch `ctx->layoutFull` on
  the first failed child, so at most one construct node pays a constant factor,
  and construct nesting is capped at `PPV_MAX_DEPTH`.
- `layoutVisits` is not reset per rung (only `layoutFull` is), and that is
  correct — it is reported to a pin and never compared against a budget, and the
  code says so (*"so a pin can assert the BOUND"*).
- `ppvLeaf`'s `(uint8_t)len` truncation of a `uint16_t`: program literals and
  names are length-prefixed with a single byte, so `len <= 255`;
  `ppvDerivVariable`'s `xcopy` is bounded by the name length plus the
  `PPV_NAME_MAX` break; `ppvVarName`'s `strcpy` is gated by
  `ppvNameIsDrawable`.
- `ppfStageValFields` clearing `lastErrorCode` after a failed
  `reallocateRegister` is untouched by this wave and deliberate — a display-context
  formatter must not leave an error pending.
- `fnPrettyVisual`'s `currentSolverStatus` save/restore has a single exit with
  both paint arms and the error arm between save and restore; the comment's *"a
  surface leaves session state as it found it"* holds.
- `refreshStatusBar()` called unguarded from the new early-return block is
  consistent with upstream, which calls it unguarded at its own normal tail
  (`screen.c:6041-6044`, the `MANUAL_STATUSBAR` guard commented out) — and it
  self-gates anyway.
- `showSoftmenuCurrentPart` clears its own softkey band (`softmenus.c:3120`), so
  the missing `_selectiveClearScreen` before the new call leaves no leftover ink.
- `ppvLiftNeutral`'s **membership** is behaviourally correct for all six
  members, including the two whose stated justification is wrong; and every
  `SLS_DISABLED` item (`CLX`, `Σ+`, `Σ-`, `CLA`) is unhandled by the walker and
  declines, so the epilogue's "clear for everything not listed" cannot mislabel a
  lift-disabling item.
- `screen.c:1281`'s `&& x1 + 1 < SCREEN_WIDTH` is **not** dead (`x1 ==
  SCREEN_WIDTH-1` reaches it) and is correct: `setBlackPixel` is a bare
  `bitblt24` with no bounds check, and dropping the pixel is right where
  clamping would smear ink into the wrong column.

### Pins read for vacuity and found live

- **EQ4b** — a genuine `strcmp` after normalising exactly one character, with
  the `sigDot[0] == 0 ||` control that keeps it red if **both** forms break,
  which is the failure mode this class dies of.
- **EQ9b's setup guard** — the fixture measures ~490 px against a 396 px
  threshold, so the precondition is genuinely false and roughly ten glyphs are
  dropped. Its problem is scope, not vacuity.
- **V-MODE's flag restore** — the asymmetric `if(!erpnWas) clear` is correct
  because the second leg leaves the flag set.
- **V36b** — clears the softkey band, calls `refreshScreen` and asserts ink via
  `ppvSumRows` (which accumulates `(x+1)*(y+1)` per lit pixel, so `== 0` really
  is "no ink"). Live; it simply cannot distinguish the two VISUAL arms.
- **T24b** — `ITM_SST` is `US_UNCHANGED` (`items.c:3588`), so `ppcClassify`'s
  default returns `PPC_IGNORE` and the explicit `case ITM_SST: return
  PPC_INVALIDATE` is load-bearing; remove it and the signature stays `2 3 +`.
  `fnSst`'s `CM_NORMAL` arm sets no error, so the pin is not riding the
  error-invalidation path.
- **M6's fixture correction** is real, not cosmetic: `ppParseExponent` tolerates
  the builder's trailing hair space only through the `PP_IS_SPACE(code) &&
  expDigitSeen` arm (`prettyValue.c:273`) and would otherwise map `a0 0a` to
  `':'`. Noted without filing: no pin now covers the *no*-trailing-space form,
  and `prettyValue.c` is outside the PR file set.
- **M8's part 1** is live today — measured from the generated tables,
  `numericFont '0'` is 2/26 and `numericFontBold '0'` is 1/29, so the equality
  does not hold and would fire if a future font converged them.

### Unreached shapes, named so they are findings the day something changes

- **`SCRUPD_SKIP_MENU_ONE_TIME`** has no live setter anywhere in `src/` or any
  package — the only `|=` is commented out (`c47Extensions/keyboardTweak.c:125`,
  "removed the MENU skip again"). Every consumer that tests it is testing a dead
  bit today.
- **The walker's numeral acceptor** (`prettyVisual.c:396`) admits digits and `.`
  only, which is the `PP18RR3-6` shape one seam over. Unreachable: the NIM writes
  `.` unconditionally (`bufferize.c:874`, `:1131`), so a stored program literal
  can never carry a comma. Cleared on the trace, not on the predicate.
- **`ppqFitWithEllipsis` on a string ending in an unpaired lead byte** — see the
  refutation above; no producer exists, and the read is pre-committed upstream.
- **The `x2 < SCREEN_WIDTH` conjuncts** — dead by construction, folded into
  `PP18RR4-11`.

### Doc drift, found and not filed as defects

- `DESIGN.md`'s fail-closed skip list *"(LBL, MVAR, REM, PAUSE, SNAP)"* omits
  `ITM_NULL`, which the code has always skipped.
- `DESIGN.md` says `ppvBody` seeds "all eight levels"; under SSIZE4 it seeds
  four, which is functionally identical (every level holds the same node and the
  refill replicates it) and makes the sentence loose rather than wrong.
- `DESIGN.md:675-678` and `TESTING.md:257` still teach `ppqShowRender` as
  VISUAL's live framing mechanism, which PP18 removed — carried from round 3,
  still unpaid.
- The §6 hook table is `PP18RR4-D5`.

### Known, ruled, or below the bar

- **`PP18-6`** (the DERIV double parenthesis) was not re-derived this round and
  its ruling stands; the condition for revisiting it (measure a real formula
  pushed out of the Z/T band) is unchanged.
- **The text back end duplicates precedence.** `ppvAstPrec`/`ppvOperand` and
  `ppfBuildOp1/2` are two independent bracketing implementations, and it is the
  **text** one that nearly every V pin asserts — so a bracketing divergence in
  the drawing path would not go red. DESIGN.md names the text form a test back
  end deliberately and this wave did not touch it, so it is recorded as a
  coverage risk rather than a defect of the fix wave. `PP18RR4-3` is what that
  risk looks like when it lands.
- **`ppcShadowInvalidate` declared in both `prettyPrint.h` and
  `prettyInternal.h`** — a divergence would be a compile error, so it is
  excluded by the "what the compiler reports" rule.
- **`files/` sync** — all in-scope sources are byte-identical between the flat
  working area and `files/`, so the gate built what was read.

---

## 7. Verdict

**Would I ship this? No — but the wave is not the problem, one commit in it is.**
Nineteen findings were closed and I could break none of the closures except
where the closure changed a shape. Four of the fixes were re-derived from
upstream by three or four independent dimensions and by direct execution, and
they are exact: the eRPN ENTER rule reproduces `fnKeyEnter` line for line, the
T-replication model reproduces `_Drop` step for step under both stack sizes, the
comma and superscript-plus widenings match their producers with no decoder to
desynchronise, and the EQSHW fit is conservative on both the buffer and the width
axis. That is a good wave.

`327ec4811` is where it goes wrong, and it goes wrong in the way a *structural*
fix does. Moving the lift clear out of seventeen arms into an epilogue "no item
can skip" was the right instinct — upstream's own shape — and it was applied to
a switch in which one arm is not a step. `ITM_XEQ`'s body is the callee's entire
execution, so "after the arm" is a different point in the machine's timeline
than "after the item", and the epilogue now erases a latch the subroutine armed.
VISUAL draws a formula that computes a different number than XEQ returns,
silently, with no decline — the one failure `327ec4811`'s own message says the
commit existed to remove, and a property restarted round 1 had examined and
cleared as faithful before this fix reversed it.

**Where would it break first?** In an owner's hands, in this order:

1. **VISUAL on any program whose subroutine ends in ENTER** (`PP18RR4-1`). Two
   labels, six steps, a silently wrong picture. Measured with a fixture, and no
   pin in the suite looks in that direction.
2. **A complex in a formula, with the T-line formula on or PHIST open**
   (`PP18RR4-2`). `2 ENTER 3 CPX`, `4`, `×`. The live line shows a garbage
   imaginary part; the same formula in history shows the right one. Measured,
   both surfaces printed side by side.
3. **VISUAL on a formula taller than the Z/T band** (`PP18RR4-5`). The
   full-screen view's deliberately cleared bottom gets the softkey row painted
   back into it, because the guard added to protect it reads a bit that TAM
   teardown clears first. Measured in pixel sums; the fix's own pin passes for
   both arms.
4. **`FILL` in a program, then more consuming steps than the stack is deep**
   (`PP18RR4-4`). "Cannot be drawn (D10)" for a program the machine runs to 512.
   The one arm that writes the stack without going through `ppvPush`.
5. **`3 x³ x³`, or `a² yˣ b`** (`PP18RR4-3`). The pager and the T line draw a
   flat `3³³` against a recorded result of 19683, while VISUAL brackets it
   correctly — two front ends over one builder disagreeing about the same
   mathematics, with the precondition's only written statement deleted by the
   comment cap.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.**

- **`PP18RR4-10`. Fix the comment; do not touch the code.** The
  save/clear/restore of `FLAG_BOLD` is upstream's own idiom at
  `fontBrowser.c:117-128`, it pays the identical cost there, and I could not
  make the side effect observable. The defect is a sentence that will mislead
  whoever adds the third paint entry. Deleting the false clause costs one line;
  "fixing" the wrapper would replace an upstream convention with a package
  invention for no measured gain.
- **`PP18RR4-11` and `PP18RR4-12` together, or not at all.** One pixel column in
  HP layout and a device-only pre-clear, both inside `showGlyphCode`, both
  cheap only if the hunk is re-shaped once. Doing either alone means touching
  that function twice and paying the merge tax twice — and the hunk wants
  re-shaping anyway (§2, `PP18RR4-D5`), which is a separate, behaviour-neutral
  change verifiable with `git diff -w` plus the gate.
- **`PP18RR4-9` and `PP18RR4-14`.** Two false comments, one commit between them.
  Real, worth doing, worth nobody's wave. Note that the *proposed* replacement
  rule in `-9` is itself wrong (a literal returns before `runFunction` and is not
  lift-neutral) — this one needs the table test more than it needs a reworded
  sentence.
- **`PP18RR4-13`.** Test hygiene. The defect T23c actually guards is still
  guarded — the mutation proves T23c is its sole detector and it reddens alone.
  What is unguarded is a granularity loss the design already tolerates. Add the
  setup assertion when the next capture pin is written, not before.
- **`PP18RR4-P1`.** Do not fix it; measure it. One probe settles whether the new
  menu arm reaches upstream's matrix SHOW, and the two readers who traced it
  reached opposite answers. Changing the predicate on a guess is how
  `PP18RR4-5` happened.

**What should not wait.** `PP18RR4-1` — it is a silent wrong drawing, it is a
regression this wave introduced, and the repair is a decision (does `ITM_XEQ`
join `ppvLiftNeutral`, or does the epilogue move inside `ppvWalk`?) rather than
a line. `PP18RR4-2` — the wrong half of a seam the same commit got right on the
other side, and the design's "the display never lies" is BINDING.
`PP18RR4-5` — its own comment states a premise that is false, so the fix and the
ruling have to be decided together. `PP18RR4-3` — as a **ruling first**, because
the obvious repair (give `ppfBuildOp` a POW level) is the one DESIGN.md says
would change the contract underneath the capture engine, and the sibling case
`a² yˣ b` is unbracketed on *every* front end including VISUAL, which the
walker's local guard does not cover. And `PP18RR4-6` and `PP18RR4-8`, because
they are ten lines between them and they are the reason a fix wave can ship with
a green gate over a reverted fix.

**What is genuinely solid, verified rather than assumed.** The three comment
commits are comment-only — checked a fourth time here over 37 file/commit pairs,
all byte-identical after stripping. The SSIZE4/SSIZE8 stack model, T
replication, DROP and DROPy all reproduce the hardware exactly, and the eRPN arm
reproduces `fnKeyEnter` exactly; both were the brief's named high-value targets
and both hold. `ppqFitWithEllipsis`'s buffer and width arithmetic is
conservative on both axes and its one theoretical overrun is unreachable and
pre-committed upstream anyway. The two acceptor widenings do not reach any
decoder. `ppSuppressBold`'s coverage claim is true. The rename is complete and
the design doc moved with it. `files/` is in sync with the working area, and the
gate is green on a clean tree.

**The pattern to carry.** Round 3 named it as "a class fixed at the sites where
it was noticed, with a sibling left out", and this round is the same sentence one
level up: **a class fixed by a refactor, at every site that looked like the
others.** `ITM_XEQ` looked like the other sixteen returning arms. `ITM_FILL`
looked like the other stack-writing arms. `PPN_VAL` looked like `PPN_LIT`. The
remedy is the one every class-level test above is written in — enumerate the
class *from the code* (the arms that call `ppvWalk`, the arms that assign
`depth`, the node kinds that continue, the surfaces that arm the manual
protocol) and assert over the enumeration. Every one of those enumerations is
two or three members long and each would have gone red today.

---

## 8. Round and exit state

**Round: PP18 round 4 of the restarted series**, and the first round of the
series with a fix wave to read. Subject `34ac6e97f..6e6c2c0ab`, nine commits,
restricted to the upstream-PR file set. This is a fix-wave audit — the shape the
exit criterion names — and the shape the project's own record predicted: r2 was
4 of 7 from r1's fixes, r3 4 of 4, r5 9 of 12, and **r4 is 8 of 14**.

**Readers.** Eight in-family finder dimensions (contracts, lifecycle,
arithmetic, error paths, guards, tests, design, upstream), blind to each other,
scoped to the eight PR files. Every raised finding then went to an independent
refutation pass with one assigned lens (reachability, correctness, intent),
instructed to default to REFUTED and to prove coverage claims by mutation.

**Out-of-family accounting: `pending`.** No packet was built, no reply exists,
no `MODEL:` line can be quoted. The §1 banner states it; this section repeats it
because the exit criterion turns on it:

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Counts.** Thirty findings raised; **six refuted**; six beyond the verification
cap; eighteen surviving entries collapsing to **fourteen CONFIRMED** distinct
defects (`PP18RR4-1`–`-14`), **one PLAUSIBLE** (`PP18RR4-P1`), and six design
observations (`PP18RR4-D1`–`D6`). Three of the fourteen were derived
independently by two or three dimensions: the XEQ latch (contracts, lifecycle,
error paths), the FILL arm (contracts, lifecycle, arithmetic) and the bold
comment (contracts, error paths). Five findings carry a correction against their
own finder, and two of those corrections replace the reaching input entirely
(`PP18RR4-11`'s register line → the NIM degree glyph; `PP18RR4-5`'s
`doRefreshSoftMenu` → `popSoftmenu`'s own first statement).

**Evidence discipline.** **Eleven of the fourteen** are backed by a probe or
mutation applied in an isolated worktree, built through the package gate with
presence verified in `build.sim/custom_pkg_shadow/*`, observed in
`build.sim/meson-logs/testlog.txt`, and reverted. The three static traces are
`PP18RR4-9` (an item-table comparison), `PP18RR4-12` (device-only, unmeasurable
on any runner this project has, and it says so) and `PP18RR4-14` (a `git log -S`
attribution). No simulator ran; no finding rests on an LCD photograph. Main tree
clean at start and finish, gate green on it, no `AUDIT-PROBE` marker anywhere in
`packages/`.

**Exit criterion: NOT MET, and this round cannot advance it.** Fourteen new
CONFIRMED findings would reset the count on their own; separately, the round had
no out-of-family reader, and the criterion requires two consecutive clean rounds
with at least one of them out-of-family. **Four consecutive rounds have now run
in-family only** — and this one ran immediately after `c719b1a9c`, a commit
inside its own subject range whose entire purpose was to stop that happening
silently. It succeeded at "not silently" and did not change the outcome.

**Process items.**

1. **Stale worktrees: eighth consecutive round, and the mitigation now works.**
   Every verifier worktree again spawned at `e21af8d28` — 120 commits off, and
   this round not even an ancestor of the tip (a divergent README branch), so a
   reader who trusted it would have produced verdicts about a tree where the
   audited code does not exist. Every single verifier detected it and ran
   `git checkout 6e6c2c0ab` before its first read, and every evidence block
   opens with the check. That is `c719b1a9c`'s instruction working at 100%. It
   is still a reader-side workaround for a spawner-side bug: the
   `git merge-base --is-ancestor` guard in `audit-workflow.js` is still absent,
   and the cost of it failing once is a whole round of invalid verdicts.
2. **Worktrees clean this round.** No verifier reported a foreign edit or an
   inherited mutation; round 3's item about a worktree arriving with a sibling's
   live mutation did not recur. Several verifiers had to re-run
   `pkg_patch_refresh.py` after reverting, because the gate's own refresh had
   regenerated `files/` and `.refresh-manifest.json` from their probe — that is
   now a known step and it is worth writing into the mutation protocol
   explicitly rather than rediscovering it per round.
3. **The out-of-family pass was skipped for the fourth consecutive round.** The
   new banner and the workflow's refusal to spawn without `outOfFamily` did what
   they were built to do; what they cannot do is supply a reader. The
   `dispatch.sh` proven range is now 30.3 KB (Gemini) / 28.9 KB (Sol), and the
   packet with the best return is named below.
4. **The governing gate still matters.** `./packages/forth-core/build-test.sh`
   refreshes only `packages/forth-core` and returns a meaningless green for a
   pretty-print mutation; `packages/pretty-print/build-test.sh --solo` is the
   only trustworthy runner. Every verifier used the right one this round without
   being told, which suggests the trap has been absorbed as habit — it is still
   not written down where a new reader trips over it.
5. **"Red-verified by mutation" needs a definition.** Both code commits claim
   every new pin was verified red, and both are telling the truth about the
   mutation they ran: disable the fix's *effect*. Two of this round's findings
   come from the mutation they did not run: revert the fix's *call site*
   (`PP18RR4-8`) and perturb the fixture's ambient state (`PP18RR4-6`). The
   protocol should name all three.
6. **Flash accounting is one commit in three** (§2). Two code commits totalling
   1,258 insertions record no `make dmcp5r47` delta, against CLAUDE.md's
   standing rule.
7. **Report filename truncated.** The requested filename is 450 bytes against a
   255-byte filesystem limit; this file's name is the requested one truncated
   after "…This-is-the-shape-the-exit", with the date and the `-r4-pr` suffix
   preserved.

**Round 5's axis, in priority order.**

1. **An out-of-family reader, over `327ec4811` alone.** Four rounds in one
   family, and this round's worst finding is a control-flow refactor whose
   defect is invisible unless you ask "what is inside this arm?" — exactly the
   question a reader without the author's model asks first. The commit is 480
   insertions across `prettyVisual.c` and `screen.c` and it is self-contained
   enough to inline whole: the walker's stack model, the epilogue split, the
   exception list, and upstream's `items.c` SLS rows plus
   `lblGtoXeq.c`'s `executeOneStep`. The question — "does this model do what the
   machine does, for every arm?" — is answerable without the rest of the package.
2. **`e81677309` and `prettyCapture.c`**, which this round could not read.
   Six of the nineteen fixes landed there, +225 lines, and the one seam that was
   followed out of it into the PR file set produced `PP18RR4-2`. It is the
   largest unaudited surface in the wave.
3. **The two capture front ends against the walker, as one differential
   oracle.** `PP18RR4-3` is a single row of the table round 3's `D1` asked for,
   and `PP18RR4-2` is another: drive the same operator sequence through
   `ppvTestBuildNodes`, `ppfBuildCurrent` and `ppfBuildEntry` and assert all
   three signatures are equal. Four of this round's fourteen and four of round
   3's open findings are rows in that one table.
4. **`prettyTest.c`'s unsampled regions** — the T- and B-series, EQ26–EQ35, and
   the P-series past P6. Four of this round's findings are pin defects and all
   four were found in the sampled quarter.
