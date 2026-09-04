# Audit — PP18 restarted round 10: the round-9 fix wave, at `80533eac4`

Subject: `2ae202759..80533eac4` on `pretty-print/stage-pp18`, two commits.

Axis, as dispatched: **(a) THE PLACEHOLDER'S NEIGHBOURS** — selection,
ENTER-recall, pan and page-count paths in the browser and the pager now meet
rows that never used to occupy space; what assumes a built root where a
placeholder sits? **(b) THE RECORDED-DECLINE TABLE** — T33's accepted cells are
a list; what spelling, mode or tag is in NO cell, and can the record rot
silently? Plus the standing fix-regression axis over all nine repairs.

The nine repairs are correct at the sites where they were typed. Both axes were
run to ground, and both came back nearly clean: the placeholder's neighbours
hold at every consumer, and the decline table's recorded cells are accurate.
The round's yield is one layer out from where it was pointed. **The surfaces
these repairs were made on are not reachable in the shapes the design states.**
The pager the placeholder was written for cannot be raised by any keypress —
ruled, and not re-reported. The browser's pan key raises a firmware bug screen
in the package's own solo build and does nothing in the composed one. And the
two pins the wave added to close round 9's coverage findings assert less than
their comments claim: FB1 compares a function against a copy of its own body,
and T33 calls nine hand-written cells an enumeration.

> **Filename note.** The dispatched subject string is 705 bytes and cannot be a
> filename (255-byte limit). This file keeps round 9's convention: truncated
> after the tip, with the full subject stated in §1 and the fenced ids in §8.

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does
> not count toward the exit criterion's two clean rounds and its verdicts carry
> one family's blind spots (SKILL.md, Exit criterion).

### The two commits

| commit | role in the wave |
|---|---|
| `b14a52f6f` | docs: the round-9 report (1,092 lines), the DESIGN-HISTORY close entry, and the `CAP` raise from 24 to 32 in `audit-workflow.js`. No package code |
| `80533eac4` | the repair wave: all nine `PP18RR9` repairs. 16 files, 687 insertions, 53 deletions. `prettyTest.c` gains 226 lines; `prettyFormula.c` changes 77; `browsers/flagBrowser.c` loses its inline scan for a call; `browsers/prettyBrowser.c` changes one string; `DESIGN.md` changes one line (the §7 A8 row); the sibling report is new |

The full dispatched subject, recorded here because the filename cannot hold it:
the round-9 fix wave on `pretty-print/stage-pp18`, audited tip `80533eac4`, two
commits — `b14a52f6f` (docs: round-9 report, CAP 24 to 32) and `80533eac4` (all
nine PP18RR9 repairs: the pager placeholder for undrawable rows, the honest
browser message, T33's per-font alphabet enumeration with recorded RAD/GRAD tiny
declines, the hex and binary atom-window completion with T32b's fifteen-base
loop, the restored A8 handoff plus the paste-ready sibling report,
`prettySysflRows()` as the browser bound's single source with FB1 asserting it,
T31's degree-magnitude assert, T30's filing control, EQ37's direct text-pool
bound, and the `fnOpenMenu` citation).

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line | findings raised |
|---|---|---|---|---|
| sol / gpt | — | — | — | **not run** (`outOfFamily: 'pending'`) |
| gemini | — | — | — | **not run** (`outOfFamily: 'pending'`) |

Neither out-of-family pass ran. The banner above states what that costs.

### In-family coverage

Eight dimensions, blind to each other: contracts, lifecycle, arithmetic,
error/refusal paths, guard reachability, tests-that-cannot-fail, design flaws,
upstream discipline. Every filing then went to a reader who did not produce it,
under one of the three lenses (reachability, correctness, intent).

**Read across the dimensions.** The whole diff `2ae202759..80533eac4`;
`prettyFormula.c` in full (the value-leaf formatter, `ppfTextIsAtom`, both
decode arms, `ppfBuildRow`, `fnPrettyHist`'s two passes, the new
`prettySysflRows` and the PC_BUILD test seam); `browsers/prettyBrowser.c` in
full; `browsers/flagBrowser.c`'s one hunk and its SYSFL walk; the
`prettyInternal.h` / `prettyPrint.h` deltas; every added `prettyTest.c` block
(T32b, T32c, T33, T34, the T30/T31/FB1 clauses, EQ37) with its neighbours.
`DESIGN.md` in full (841 lines), `DESIGN-HISTORY.md`'s round-8 and round-9
entries, `TESTING.md`'s MUT-76 entry, the round-9 report's nine sections, the
round-3 report's containment and pager sections, the PP1-PP16 audit's
not-a-defect list, and `REVIEW_upstream-minimality_2026-08-27.md`.

**Traced outside the package**, because that is where the contracts land:
`keyboard.c`'s `determineItem`, `btnPressed`, `processKeyAction` end to end and
`btnReleased`, against `src/c47/keyboard.c` and `packages/undo-history/keyboard.c`
to separate solo from composed behaviour; `screen.c`'s CM 20 dispatch;
`registers.c` (`reallocateRegister`, `copySourceRegisterToDestRegister`, the
size table); `recall.c`; `items.c` (`reallyRunFunction`); `error.c`;
`display.c` (`real34ToDisplayString`, `angle34ToDisplayString2`,
`shortIntegerToDisplayString`, `longIntegerToDisplayString`,
`exponentToDisplayString`); `bufferize.c`'s NIM emitters; `dateTime.c`,
`addition.c`, `toPolar.c`, `conversionAngles.c`.

**Measured rather than assumed.** Per-font glyph inventories extracted from the
generated `rasterFontsData.c` for every code point a value leaf can emit
(`0xa20e`, `0xa027`, `0x82b3`, `0x9d4d`, `0xac66`, `0x80b0`, `0x80b7`, `0x80d7`,
`0xa221`, `0xa47d`, `0xa160`-`0xa16b`, `0xa008`, and the whole base-subscript
window `0xa461`-`0xa46f`). The gate build's `menu_SYSFL` row count (114) against
`NUMBER_OF_SYSTEM_FLAGS` (115). Seven verification mutations, each applied,
observed and reverted by inverse edit inside its own step, with the probe
verified in `build.sim/custom_pkg_shadow/` or in the generated patch before the
result was believed. A real-key driver, copy-adapted from undo-history's B9,
drove `btnPressed`/`btnReleased` for the g shift and key 24 in both the solo and
the combined gate pass.

**What the budget did not reach.** The capture/fold state machine
(`ppcOpen`/suspend/close) beyond `ppcValLeafFromRegister` and
`ppcTreeHasOpaque`; `prettyValue.c`, `prettyVisual.c`, `prettyLayout.c` and
`prettyEquation.c` beyond the call sites the repairs touch; roughly 6,400 of
`prettyTest.c`'s 6,695 lines; `solver/equation.c`'s five hunks and
`keyboard.c`'s thirteen re-read against current upstream (untouched by this
wave; the 2026-08-27 review read them); the generated `files/` mirrors and the
refresh manifest, which are output. **`PP18RR10-2` is a static trace and was
never executed** — see §4. No device or DMCP build was measured; every runtime
number is the PC gate build.

---

## 2. Mechanical results

**Gate: GREEN** at `80533eac4`, refresh-first, in this session, in the solo and
the combined pass. Every verification mutation below was run against that
baseline and reverted; every worktree finished with `git status --porcelain`
empty.

**Churn scan: exit 0, zero findings.** Re-run for this report rather than taken
from the dispatch:

```
patch                                             adds  dels hunks
010-browsers__flagBrowser.c.patch                    8     1     1
010-bufferize.c.patch                                2     0     2
010-c47.h.patch                                      2     0     1
010-calcMode.c.patch                                 2     0     1
010-config.c.patch                                   4     0     1
010-defines.h.patch                                  4     1     2
010-items.c.patch                                   19    10     5
010-items.h.patch                                    8     6     2
010-keyboard.c.patch                                54     3    13
010-screen.c.patch                                  38     4     5
010-softmenus.c.patch                               19     2     4
010-solver__equation.c.patch                       578     0     5
010-testSuite__testSuite.c.patch                    11     0     1
010-testSuite__tests__testSuiteList.txt.patch        8     0     2
```

Fourteen patch files, 45 hunks, 757 added lines. **The dispatch says fifteen
patches; the directory holds fourteen at this tip.** The count matters to
`PP18RR10-9`, so it is stated rather than carried.

**`design-audit.sh`**: not run for this round. It is a forth-core drift script
and has no pretty-print target; the equivalent mechanical check for this
package's documentation gap is `PP18RR10-9`'s class test, which does not exist
yet.

**Compiler warnings**: none new. Every mutation build compiled clean except
where a probe forced one (one verifier added `(void)sysflRows;` to keep an
unused-variable error from masking its own experiment, and said so).

**Verification mutations run this round**, all reverted:

| mutation | result | finding |
|---|---|---|
| `flagBrowser.c:283` reverted to `NUMBER_OF_SYSTEM_FLAGS` | gate **GREEN**, solo and combined | `PP18RR10-4` |
| the same, plus `lastErrorCode = 1` at `f+fOffset == 114` | gate **RED** (FB1's own text) | positive control for the above |
| `prettySysflRows()` neutered to `return NUMBER_OF_SYSTEM_FLAGS` | gate **RED** ("expected 114, actual 115") | the one door the repair did close |
| pass-1 `h = 20;` reverted to `continue;` | gate **GREEN**, 13025/13025 | `PP18RR10-7` |
| `tinyFont` made to lack `0xac66` and `0xa47d` | gate **RED** (T29 ×2), 14 probe hits, **all `0xa47d`, none `0xac66`** | `PP18RR10-5` |
| a tiny-missing glyph appended to the `dtLongInteger` arm | gate **RED**, 23 failures over 7 tests | refutes the dtLongInteger half |
| `0xa20e`/`0xa027` deleted from `ppfTextIsAtom` (the DESIGN.md-faithful predicate) | gate **RED** (T32c, sole failure) | `PP18RR10-8` |
| `ITM_dotD` driven through the real key chain in an open browser | solo `cmAfter=10`; combined `panCalls=0` | `PP18RR10-1` |
| `ppfBuildRow` instrumented on the T34 RAD fixture | rung 0 `built=1 meas=1 w=587 h=18`, discarded | `PP18RR10-3` |

---

## 3. CONFIRMED findings

Nine ids, worst first. Twelve filings survived refutation and merge to these
nine: three dimensions filed the FB1 clause independently (`PP18RR10-4`) and two
filed the pan key by different mechanisms (`PP18RR10-1`).

### PP18RR10-1 — the browser's pan key is dead in both builds, and raises the firmware bug screen in the solo one

**Where.** `packages/pretty-print/keyboard.c:2793` (the `.d` exemption), with
`:1604` (the shift whitelist) as the solo-build door and `:2797-3336` (the
`switch(calcMode)` with no arm for 20) as the destination.

**What breaks.** `prettyBrowserPan()` is unreachable from the keyboard in both
maintained configurations. Its only live caller is `prettyTest.c:2276`, which
calls it directly.

**Reaching input, solo build** (pretty-print alone — the package's own gate
configuration): DISP → PP → PHIST opens the browser, `calcMode` 20. Press g.
`determineItem`'s shift block at `:1604` lists CM_NORMAL, AIM, NIM, MIM, EIM,
PEM, PLOT_STAT, GRAPH, ASSIGN, ASN_BROWSER, REGISTER_BROWSER, FLAG_BROWSER,
FONT_BROWSER and TIMER — every upstream browser, and not CM_PRETTY_BROWSER — so
`commonShiftProcessing()` is never called and `shiftG` stays false. Key 24 then
resolves to `ITM_LOG10` (`assign.c:13`: primary/fShifted/gShifted =
`ITM_LOG10`/`ITM_10x`/`ITM_dotD`, in all ten layout tables), which the
containment arm at `:2793` swallows. **Measured with a real-key driver:
`gArmed=0 panCalls=0`.** If `ITM_dotD` is produced anyway — the owner ASSIGNs
`.d` to an unshifted key, or `shiftG` is forced — the exemption at `:2793` is
false by construction for `.d`, so it falls into the `switch(calcMode)` at
`:2797`, which has no `case 20` and no bare `case CM_PRETTY_BROWSER`, and lands
on `default:` at `:3333`: `displayBugScreen("In function processKeyAction: 20 is
an unexpected value while processing calcMode!")`. **Measured: `calcMode` 20 →
10 (CM_BUG_ON_SCREEN), `previousCalcMode` 20.**

**Reaching input, combined build** (pretty-print + undo-history — the shipping
composition): undo-history's keyboard patch adds `(calcMode >= 19 && calcMode <=
23)` to that same whitelist line, so g does arm and `ITM_dotD` is produced. The
exemption lets it through, and undo-history's `case 20:` (its `keyboard.c:3115`,
sharing a body with the upstream browsers) sets `keyActionProcessed = true`.
`btnPressed:1946` then never calls `showFunctionName`, `showFunctionNameItem`
stays 0, and `btnReleased:2152` never runs the item, so `fnKeyDotD`'s `case
CM_PRETTY_BROWSER: prettyBrowserPan()` (`keyboard.c:4980`) is never entered.
**Measured: `gArmed=1 panCalls=0`.** The whole suite stayed green with the probe
in — 13025 solo, 13041 combined, zero prettyPrint failures.

Alternate producers of `ITM_dotD` in mode 20 were ruled out: `Check_Norm_Key_00_Assigned`
and `Check_MultiPresses` (`keyboardTweak.c:320, 368, 385, 396`) are gated on
CM_NORMAL/NIM/AIM/EIM/PEM/TIMER/ASSIGN, so long-press and double-press cannot
mint `.d` in the browser either.

**Contract violated.** The guard's own comment, `keyboard.c:2791-2792`: *"The .d
exemption keeps the pan key reachable: .d has no case of its own earlier in this
chain."* It has no case later in the chain either, which is the half the comment
does not say. Also `DESIGN-HISTORY.md:924`, *".d pans a too-wide selected row
(wraps)"*, and `TESTING.md:248`, MUT-76's replacement trace: *"Verified by trace
instead: UP/DOWN are matched earlier in the same chain …, ENTER/EXIT/BACKSPACE
have their own `case ITM_...` upstream, and `.d` has neither — which is why it
needs the exemption."* The trace establishes why the exemption is needed and
stops there. The same entry also misplaces the guard — it says *"The guard lives
in `executeFunction`"*, and this guard is in `processKeyAction`.

**What it costs the owner.** Solo: a full-screen *"This is most likely a bug in
the firmware!"* panel on a documented gesture. It is recoverable — EXIT restores
`calcMode` from `previousCalcMode` (20) and returns to the browser — and the row
never pans. (One correction to the filing: after the bug screen
`keyActionProcessed` is still false, so the release does fire
`runFunction(ITM_dotD)`, but by then `calcMode` is 10, `fnKeyDotD` has no case
for it, and `displayBugScreen` early-returns. The mechanism is one step longer;
the outcome is the same.) Combined: nothing happens at all. In both, a row wider
than the screen can never be read past its left edge — and this is not a 4-pixel
curiosity, because `ppfBuildRow:810` admits arbitrarily wide rows when `canPan`
is set, which only the browser sets. Pan is the sole way to read them.

**Bug class.** Two, interlocked. *"Never rely on a sibling for containment"*
(DESIGN.md §1, R5-3) with the polarity inverted — in the combined build the
sibling's `case 20:` is the only thing between the owner and the bug screen.
And *same-level coverage*: CM_PRETTY_BROWSER was added to `determineItem`'s key
**resolution** arm (`:1694`) and not to the shift **processing** list one screen
above it.

**Class-level test.** Drive every key of the browser's documented key list
(`prettyPrint.h:36-38`) through `btnPressed`/`btnReleased` in both the solo and
the combined build, and assert the handler ran — a counter inside each of
`prettyBrowserUp`/`Down`/`Enter`/`Leave`/`Pan`, not a direct call to it. The
suite today proves the browser's keyboard arms by calling the handlers (FV12),
which is exactly why a key that cannot be pressed passes.

---

### PP18RR10-2 — capture admits three data types the formatter cannot spell, so ordinary date arithmetic files a row that reads "(cannot draw)" forever

**Where.** `packages/pretty-print/prettyFormula.c:66` (`ppfFormatStaged`'s
`default: return false`) against `prettyCapture.c:196-206`
(`ppcValLeafFromRegister`'s admission test).

**Reaching input.** Press `Date→DT` on the CLK menu (`items.c` row 1438,
`fnDate`, `dateTime.c:929`): X becomes today's date, `dtDate`, and the shadow
invalidates. Type `5`, press `+`. `+` is a classified dyadic
(`prettyCapture.c:501`, `PPC_DY`), so STAGE calls `ppcEnsureKnown(1)` →
`ppcValLeafFromRegister(REGISTER_Y)` with no type gate. `dtDate` is not a matrix,
and it is exactly `REAL34_SIZE_IN_BLOCKS` = 16 bytes (`registers.c:1225-1227`)
against a 16-byte payload cap, so `bytes > cap` is false and it becomes a
`PPN_VAL` leaf with `aux = dtDate`. `addDateReal` (`addition.c:383`) succeeds,
so DONE builds `OP2(ADD, VAL(date), LIT("5"))`. Press `CLSTK`: displacement
files the formula, and `ppcTreeHasOpaque` (`prettyCapture.c:425`) has no opaque
node to catch. Press `PHIST`. The browser row calls `ppfBuildEntry` → the
`PPN_VAL` arm at `prettyFormula.c:500` → `ppfFormatStaged` → the `switch` on
`dtDate` falls to `default: return false` → `PP_NONE` → `ppfBuildRow` false at
**both** rungs → the placeholder.

The same holds for `dtTime` (also 16 bytes) and for a short `dtString`
(`registers.c:1229` — a 1-block string is 8 bytes total, well inside the cap).

**Contract violated.** `DESIGN.md:184`, §3, in the binding capture-engine
paragraph: *"Matrix/string/oversized payloads become `PPN_OPAQUE`, which poisons
the containing tree into never-being-shown."* `ppcValLeafFromRegister` opaques
only `dtReal34Matrix`, `dtComplex34Matrix` and oversized payloads. A short
string is admitted **against the doc by name**, and dates and times were never
considered on this path at all.

Three candidate rulings were checked and none covers it. `DESIGN.md:166` (§2's
*"Long integers, strings, matrices, short integers, dates, times, configs: never
pretty — immediate false"*) is the register-line converter's contract, where a
decline leaves upstream's own arm drawing the date correctly — and
`ppfFormatStaged` contradicts that list anyway by handling `dtLongInteger` and
`dtShortInteger`. §1's fallback rule is scoped to *"upstream's own arm renders
unchanged"*, which the PHIST surfaces do not have. `DESIGN.md:624`'s
OPAQUE-taint rule belongs to the PP18 VISUAL walker. `grep -rn "dtDate|dtString|dtTime"`
over `design-docs/pretty-print/` returns nothing.

**What it costs the owner.** A history row for ordinary date arithmetic that
reads "(cannot draw)", in every font, on the surface a PHIST press actually
raises, permanently. Nothing about the entry is too wide or too tall. Before
this commit the row vanished silently; the round-9 placeholder converts a silent
drop into a visible permanent artefact, which is why the mismatch matters now
rather than at PP10.

**Bug class.** *Two enumerations of one set, in two files, with nothing forcing
them to agree* — the same shape as the SYSFL count PP18RR8-2 fixed. Capture's
admission set and the formatter's four `switch` arms are written independently
and no pin compares them.

**Class-level test.** For every `dataType` `ppcValLeafFromRegister` admits as
`PPN_VAL`, assert `ppfFormatStaged` returns true. The loop must be generated
from the admission predicate, not hand-written — a hand list is what T33 already
is. T33's seam is the right vehicle, and `PP18RR10-6` is what stops it working
today.

**Evidentiary status.** Every link was read; no reader executed the keystrokes.
See §4.

---

### PP18RR10-3 — a wide row the browser could pan is discarded at rung 0, so the browser pays a decline that rung 0 plus its own pan would have avoided

**Where.** `packages/pretty-print/prettyFormula.c:810`.

**Reaching input.** RAD mode, a polar complex in X — `→POLAR` (`items.c` entry
1849, `toPolar.c` `fnToPolar2`, which sets `amPolar` then
`currentAngularMode`), or `FLAG_POLAR` on any ordinary conversion
(`registerValueConversions.c:657-658`) — multiplied by a 30-digit literal (the
documented maximum, T26), then CLSTK to file it and PHIST to select the row.
`pbPaint` calls `ppfBuildRow(row, haveCurrent, canPan=true, …)`.

**Instrumented at the audited tip**, on round 9's own T34 fixture:

```
AUDIT-PROBE R10 rung0 built=1 meas=1 w=587 h=18
AUDIT-PROBE R10 rung1 built=1 meas=0 w=-1
AUDIT-PROBE R10 buildRow pan=0 nopan=0
```

Rung 0 builds **and measures** — width 587 against a pan clamp of 587 − 380 =
207, height 18 against a band of 139 rows — and is discarded solely by `(rung ==
0 || !canPan)`. Rung 1 re-fonts deep to tiny and dies in `ppMeasure`, not on
width, because the polar/RAD spelling carries `STD_SUP_BOLD_r` `0x82b3`, which
`tinyFont` lacks (verified in the generated font table: one occurrence in
`numericFont`, one in `standardFont`, none after the `tinyFont` start). The
panning caller gets `false`, identical to the non-panning one, and
`prettyBrowser.c:87` paints "(cannot draw)".

**Contract violated.** The guard's own comment, `prettyFormula.c:806-809`: *"At
the last rung, width depends on the caller. The browser pans sideways, so it
takes the row at any width. The pager cannot pan, so it omits an over-wide
row."* The browser takes the row at any width **only at the tiny rung**, and
there is no path back to a rung that already succeeded. `DESIGN.md:119` states
the ladder as *"`PP_SURF_BAND` (browser row): standard/standard → standard/tiny
→ fail"* with no width qualifier on the first rung. In practice, for any row
wider than 392 px, the browser's ladder is one rung and inherits every glyph gap
`tinyFont` has.

**What this is not.** Not the RAD/GRAD tiny decline, which is ruled accepted and
is fenced. Not the pager placeholder. The narrow claim: the ruling's recorded
surface is *the pager*, `PP18RR10-1` shows the pager cannot be raised, so the
accepted decline is in fact paid on the **browser** — where a fully built,
fully measured, band-fitting standard-font layout was thrown away one rung
earlier by a caller that implements panning.

**Stated uncertainty.** If the owner ruled the ladder shape deliberately —
prefer shrinking over panning, with no fallback to an earlier rung — this is not
a defect. No such ruling is in `DESIGN.md` or `DESIGN-HISTORY.md`. The nearest
counter-evidence is T28, which pins the panning caller receiving a wide row from
the **tiny** rung; that pin does not distinguish "tiny first" from "tiny only",
and `prettyInternal.h:175-178` documents `canPan` as a width relaxation without
saying what happens when the tiny rung fails for another reason.

**Bug class.** *A fallback ladder with no path back to a rung that already
succeeded.*

**Class-level test.** A fixture that measures at rung 0 and fails at rung 1 —
the T34 fixture in RAD is one — with the assertion that
`ppfBuildRow(canPan=true)` returns true and returns the rung-0 root. The fixture
exists; the assertion does not.

---

### PP18RR10-4 — PP18RR9-4 does not close its own reproducer: FB1 compares `prettySysflRows()` with a copy of its own body

**Where.** `packages/pretty-print/prettyTest.c:2017` (the clause),
`prettyFormula.c:749` (the helper), `browsers/flagBrowser.c:283` (the call site
the pin is supposed to observe).

**Reaching input.** The mutation round 9 named, re-run on this tip: revert
`flagBrowser.c:283` to `int16_t sysflRows = NUMBER_OF_SYSTEM_FLAGS;` (or `:285`
to `f+fOffset > NUMBER_OF_SYSTEM_FLAGS - 1`), refresh, run the gate.
**Measured three times, independently, in three worktrees — solo and once
combined: GREEN, `Ok 1 / Fail 0`, `PRETTY-PRINT GATE GREEN`.** FB1's `rows`
(`prettyTest.c:2005-2013`) and `prettySysflRows()` are the same scan of the same
global `softmenu[]`, evaluated microseconds apart in the same call, so both
return 114; `rows < 61` is false; `114 != 114` is false; the walk then evaluates
`menu_SYSFL[114]` — one past a 114-row array — feeds the garbage to
`indexOfItems[]`, and sets no error code, so the `lastErrorCode` arm is false
too.

**Positive control, so this is an oracle gap and not dead code.** With the
mutation retained, injecting `lastErrorCode = 1` at `f+fOffset == 114` turns the
gate **RED** with FB1's own text (`testlog.txt:496`, *"FB1 the flag browser
raised an error (expected 0, actual 1)"*). Index 114 is reached, FB1's
else-branch runs, its oracle is live. The green is silence, not absence.

**Second control, and the finding's own concession.** `if(1) return
NUMBER_OF_SYSTEM_FLAGS;` inside `prettySysflRows` reds FB1 with *"expected 114,
actual 115"*. The repair therefore does close round 9's **second** door —
neutering the helper — and only that one. What it cannot see is the browser
losing or reverting its call, which is the refresh/rebase/merge scenario the
one-hunk override exists to survive.

**Contract violated.** `DESIGN-HISTORY.md:1664` and the commit message: *"the
walk bound lives in `prettySysflRows()` and FB1 asserts the browser's own
derivation."* FB1 asserts the **test's** re-derivation. The clause's own comment
claims more than it does: *"the browser's bound and this row derive from the
same table; a drift between them is the walk reading past the array"* — two
evaluations of one pure scan of one global cannot drift. Round 9 prescribed the
other shape in terms: *"require the browser's `sysflRows` to equal the generated
`menu_SYSFL` length … or equivalently that the walk's highest index stays inside
the array"*, and named the class it did not want: *"Fixture sized from the
constant under test (G2 — both sides move together)"*.

**The numbers.** `defines.h:1021` is `64+51` = 115; `softmenus.c:1067` sizes
`-MNU_SYSFL`'s `numItems` as `sizeof(menu_SYSFL)/sizeof(int16_t)` = 114 solo;
`DESIGN.md:786` records the same arithmetic (*"Solo is 115 declared / 114
supplied — flag id 112 is undo-history's reserved UHIST, a hole with no catalog
row"*). Screen 2 uses `fOffset` 60 with `f` to 59, so a 115 bound reaches index
114, one past the last valid 113.

**What it costs the owner.** The package's newest upstream override still has no
drift guard, while `DESIGN-HISTORY` and the commit message both say one exists.
A later refresh, rebase or revert that restores the count-based bound
reintroduces the A8 out-of-bounds read on a solo build — the last tile of
system-flags screen 2 painted from memory past the array — with the gate green.
The commit message's evidence paragraph lists marked mutations for exactly two
repairs (the placeholder revert and the window revert); `PP18RR9-4` is not among
them, which is consistent with it never having been mutation-checked.

**Bug class.** *Gate consuming output its producer never emits* (r10 R10-3/4/5),
the r4 vacuous-pin family, and round 9's own G2.

**Class-level test.** The pin must read the browser, not the helper: expose the
walk's highest reached index and assert it is inside the generated array length,
or have `flagBrowser` record its bound in a package variable FB1 reads. A pin
whose two operands are the same expression cannot fail, and that is checkable
mechanically.

---

### PP18RR10-5 — T33's alphabet record has no cell for `amMultPi`, and no cell for complex-polar GRAD

**Where.** `packages/pretty-print/prettyTest.c:1843` (`cells[]`), against the
banner at `:1830-1837`.

**Uncovered and reachable — `amMultPi`.** Set MULπ (`items.c` row 1523,
`fnAngularMode` `amMultPi`; `conversionAngles.c:214` sets the tag), produce an
angle-tagged real, file it, PHIST. `ppfFormatStaged`'s real34 arm calls
`real34ToDisplayString(…, getRegisterTag(TEMP_REGISTER_1), …)`, which routes any
tag other than `amNone` to `angle34ToDisplayString2` (`display.c:253`), which
appends `STD_SUP_pir` `0xac66` (`display.c:1882-1885`). Value leaves carry the
tag — `ppcValLeafFromRegister` stores `getRegisterTag` in `pad[0]`, and the
filed-entry decoder re-stages dataType/tag/allocParam (`prettyFormula.c:598-604`)
— so both TKV and TKRES reach this arm. `cells[]` holds `amNone`, `amDegree`,
`amRadian`, `amGrad`, `amDMS` and three complex tags. No `amMultPi`.

**Mutation-proof of the coverage claim.** A `findGlyphExact` probe that makes
`tinyFont` lack `0xac66` and `0xa47d` was consulted for `0xa47d` **fourteen
times** over the whole suite and for `0xac66` **zero times**. Nothing in the
suite spells, measures or draws a MULπ-tagged leaf in any font, so removing that
glyph from `tinyFont` cannot redden anything.

**Uncovered and reachable — complex polar GRAD.** `cells[]` records `real
radian`, `real grad` and `complex polar rad` as `noTiny` declines. Complex polar
**grad** is absent, and it emits `STD_SUP_BOLD_g` `0x9d4d`, which the generated
`tinyFont` lacks. The decline itself sits inside the ruled RAD/GRAD class and is
not re-reported; the missing **cell** is the finding.

**Two halves of the original filing are refuted, and are recorded here so the
record narrows honestly.** `dtLongInteger` and the exponent form **are** pinned,
just not by T33. With `tinyFont` made to lack `STD_SUB_10` `0xa47d`, the gate
goes RED at T29 twice — T25/T28's filed entries stage a long-integer result
whose spelling must measure in tiny, and `longIntegerToDisplayString` emits the
same exponent tail past its width cap. A separate mutation that appended a
tiny-missing glyph to the `dtLongInteger` arm reddened 23 assertions over seven
tests. So *"the suite does not see it"* is false for those two. Only `amMultPi`
is invisible to the whole suite.

**Contract violated.** The test's own banner, `prettyTest.c:1830-1837`: *"every
spelling a value leaf can emit, enumerated against the three fonts … Every other
cell must resolve in all three fonts, so a formatter or font change that widens
a fatal alphabet reddens here instead of shipping a vanished row."* And
`DESIGN-HISTORY.md:1656-1657`, *"T33 enumerates every value-leaf spelling
against the three fonts."* Nine hand-written cells over three of
`ppfFormatStaged`'s four arms, at a single magnitude, are a sample.

**What it costs the owner.** Nothing today. Both code points were checked in the
generated font tables and behave as recorded. The cost is that the record cannot
rot loudly in the one direction that matters: a formatter or font change that
makes a MULπ-tagged or polar-GRAD row fatal in `tinyFont` adds a fresh "(cannot
draw)" line the owner sees and the suite does not — the exact wave PP18RR9-1 was
raised for. The reverse direction **is** covered: a `noTiny` cell that starts
resolving fails with *"recorded as a tiny decline, and tiny now draws it"*
(`prettyTest.c:1889-1892`).

**Bug class.** *Serialisation that claims byte-fidelity and omits a header
field* (r11 R11-IF-1) — *"round-trip EVERY header field … and pick fixtures the
defect can reach"*.

**Class-level test.** Generate the cells from the domain: every value of
`angularMode_t` crossed with every `dataType` `ppfFormatStaged` handles, plus one
exponent-magnitude value per numeric type. A hand list cannot carry a
completeness claim, and `_Static_assert` on the enum's size is what stops the
list going stale when a tag is added.

---

### PP18RR10-6 — the T33 seam hard-codes `allocParam = 0`, so the cell PP18RR10-5 asks for writes past the register block

**Where.** `packages/pretty-print/prettyFormula.c:730`.

**Reaching input.** Unreached today: all nine cells use fixed-size types. It is
reached by the cell `PP18RR10-5` asks for — add `{ "long integer",
dtLongInteger, LI_POSITIVE, 0 }` with a long-integer payload and `bytes > 0`.
`ppfTestStagedSpelling` → `ppfStageValFields(dataType, tag, **0**, bytes,
payload)` → `reallocateRegister(TEMP_REGISTER_1, dtLongInteger, 0, amNone)`.
`registers.c:2055-2066` overrides `dataSizeWithoutDataLenBlocks` only for
`dtComplex34`/`dtReal34`/`dtTime`/`dtDate`/`dtShortInteger`/`dtConfig`;
`:2077-2082` **keeps the caller's value** for `dtLongInteger`, `dtString` and the
matrix types, so 0 allocates `0 + TO_BLOCKS(sizeof(strLgIntHeader_t))` — one
4-byte block (`BYTES_PER_BLOCK` = 4, `sizeof(strLgIntHeader_t)` = 4).
`REGISTER_LONG_INTEGER_HEADER(a)` **is** `getRegisterDataPointer(a)`
(`registers.h:54`), so the whole allocation is that header, and
`ppfStageValFields`' `xcopy(getRegisterDataPointer(TEMP_REGISTER_1), payload,
bytes)` overruns at the first limb byte.

**Contract violated.** The seam's banner, `prettyFormula.c:723-726`: *"Test
seam: stage a raw register payload and return its display spelling."* No
restriction on `dataType` is stated, and neither the signature nor the body
guards one. `ppfStageValFields` takes `allocParam` **because**
`reallocateRegister` needs it for the variable-length types, and both production
callers supply the real value (`:500` passes `nd->item`, `:603` the entry's
stored `allocParam`). The design treats the parameter as load-bearing:
`ppcAllocParamOf` (`prettyCapture.c:186-192`) exists only to preserve
`dataMaxLengthInBlocks` for `dtLongInteger` and `dtString`, and
`prettyCapture.c:196-206` makes only the matrix types and oversized payloads
opaque, so a small long integer is an ordinary `PPN_VAL`.

**What it costs the owner.** Heap corruption in the PC test build, at the moment
someone honours round 9's own accepted class-test prescription for PP18RR9-1:
*"for every `dataType` a value leaf can carry, crossed with every register
tag"*. The failure presents as unrelated later-test garbage or a crash, not as a
T33 red — which is the worst possible shape for a test-only defect.

**Bug class.** *Correct only by callers' good behavior* — correctness resting on
an unstated invariant that nothing asserts.

**Class-level test.** The seam takes `allocParam` from its caller, and one cell
per variable-length type proves it: stage, spell, then re-read the register's
allocated size and assert it covers the payload. A `_Static_assert` cannot see
this.

---

### PP18RR10-7 — the pass-1 half of the pager placeholder has no test

**Where.** `packages/pretty-print/prettyFormula.c:859`.

**Reaching input.** Revert only pass 1's `h = 20;` to the pre-fix `continue;`,
leave pass 2's placeholder intact, refresh, run the pretty-print gate.
**Measured: 4/4 meson tests OK, 13025 passed, 0 failed**, with both
`pretty_print.txt` and `pretty_visual_real.txt` executed and the probe verified
in `build.sim/custom_pkg_shadow/prettyFormula.c:859`. T34's fixture dispatches
`ITM_PCLR` first, so `histN == 1`; the walk yields `pages == 1` under either
pass-1 shape; `ppfPage = 0 % 1 = 0`; pass 2 paints the placeholder at
`PPF_BAND_TOP` and `ppvSumRows(25, 163)` is non-zero. T34 passes.

T34 is not vacuous — it calls `ppTestFail("T34 the row builds after all, so the
fixture tests nothing")` if `ppfBuildRow` succeeds, and that branch did not fire
— it simply cannot distinguish the two pass-1 shapes at one row. **No other test
reaches the pager at all**: `fnPrettyHist:829` routes to `prettyBrowser` unless
`calcMode == CM_PRETTY_BROWSER`, and `prettyTest.c:1815` is the only assignment
of that mode before an `fnPrettyHist` call. FV5, FV6 and FV12 all arrive in
CM_NORMAL, and FV5 asserts exactly that.

**Contract violated.** The invariant the repair states in its own code,
`prettyFormula.c:875-876`: *"Pass 2: paint the selected page with the same
packing walk. The placeholder height must match pass 1 or the pages drift."* The
literal `20` appears twice (`:859`, `:884`) with no shared constant and nothing
tying them.

**What it costs the owner, stated conservatively.** Less than the filing
claimed. The pager arm is production-dead by ruling (§6), so the structural
consequence — pass 1 undercounting pages, `ppfPage % pages` clamping below the
index pass 2 reaches, and rows becoming unreachable — lives inside a dormant
body. What the wave chose to repair, it chose to pin, and it pinned half of it.

**Bug class.** *One fixture, one row, one page* — a packing walk pinned by a
fixture that has no packing in it.

**Class-level test.** A multi-page fixture with an undrawable row in the middle,
asserting that pass 1's page count equals the number of pages pass 2 can paint.
Two rows and a forced band overflow are enough.

---

### PP18RR10-8 — DESIGN.md's atom-window sentence still describes the pre-RR9-2 window, and implementing it as written reds a shipped pin

**Where.** `design-docs/pretty-print/DESIGN.md:590-591`.

**The text.** *"The predicate's accepted two-byte window is the digit-group
spaces plus the fifteen base subscripts."* The code accepts `0xa000`-`0xa00f`,
`0xa461`-`0xa46f`, **plus `0xa20e` and `0xa027`** (`prettyFormula.c:138-140`),
and on the ASCII side `'A'`..`'F'`.

**Reaching input.** A read path, and a mutation settles it. Applying the
DESIGN.md-faithful predicate — deleting `|| code == 0xa20e || code == 0xa027` —
turns the gate **RED** with a single new failure: *"T32c a wide binary numeral
drew bracketed"* (`testlog.txt:491`). The two codes are load-bearing
(`fonts.h:490` `STD_BINARY_ONE`, `:557` `STD_BINARY_ZERO`, emitted by
`display.c:2304-2309` for wide base-2 numerals), so the normative sentence is
materially false rather than merely loose.

**Contract violated.** `DESIGN.md:3`, *"This document is authoritative for
`packages/pretty-print/`"*, against `DESIGN-HISTORY.md:3`, *"Non-normative
amendment trail"*, where the widened window is recorded (`:1659-1662`).
`git show 80533eac4 -- design-docs/pretty-print/DESIGN.md` is a one-line diff
and it is the §7 A8 claims row; the atom paragraph was not touched. The
"amendments live in DESIGN-HISTORY" defence fails on the project's own practice
— this same commit folded a round-9 amendment inline into DESIGN.md's A8 row,
and the atom paragraph already carries its own inline ruling cite.

**Narrowing.** The filing's consequence clause over-reaches on *"the hex
letters"*. The sentence constrains the two-byte window only; ASCII `A`..`F` is
absent from DESIGN.md rather than contradicted by it. The title and the violated
claim, which are about the two wide binary glyphs, are exact.

**What it costs the owner.** A later reader re-deriving the window from the
authoritative document drops `STD_BINARY_ZERO`/`STD_BINARY_ONE` and reintroduces
PP18RR9-2 for every base-2 numeral wide enough to reach the substituted
spelling.

**Bug class.** *Normative text left behind by its own amendment.*

**Class-level test.** None is possible for prose. The mutation above is the
check, and it is cheap: implement the documented window and watch T32c.

---

### PP18RR10-9 — §7's hook inventory covers 9 of the 14 patched files, and the two densest override surfaces have no adjacency row

**Where.** `design-docs/pretty-print/DESIGN.md:794-809`.

**Measured at this tip.** `ls packages/pretty-print/patches/*.patch` returns 14,
and the refresh manifest's map holds the same 14. The table at `:796-806` has
eight rows covering nine files (`c47.h`, `screen.c`, `items.c`, `bufferize.c`,
`calcMode.c`, `browsers/flagBrowser.c`, `config.c`, `testSuite.c` +
`testSuiteList.txt`). Absent: **`keyboard.c` (13 hunks, 54 adds)**,
**`solver/equation.c` (5 hunks, 578 adds)**, `softmenus.c` (4 hunks), `items.h`
(2 hunks — one of which *is* recorded at `:784`), and `defines.h` (2 hunks, with
the count claim at `:786` but no adjacency row).

**Reaching input.** The reader the table exists for. §7 is headed *"Composition
claims (BINDING for other packages)"* and the table *"Upstream files hooked,
with verified adjacency to sibling packages' hunks"*. A rebaser asks whether
pretty-print touches `keyboard.c` and finds no row. Reading on in the same
document, `DESIGN.md:353` states *"Zero keyboard.c/defines.h churn"* against 13
and 2 measured hunks, and `:356` calls `keyboard.c` *"the project's riskiest
three-package composition surface"*. `grep -n minimality DESIGN.md` returns
nothing, so the authoritative document never cites
`REVIEW_upstream-minimality_2026-08-27.md`, the only artifact where that
adjacency was ever measured.

**Why it is not pedantry.** Three of the 13 `keyboard.c` hunks are
byte-identical shared edits with undo-history, unified by 3-way merge. That is
the exact identical-edit mechanism `defines.h`'s `NUMBER_OF_SYSTEM_FLAGS` row
gets a written claim for at `:786`. It is the edit class most likely to surprise
a rebaser, on the file the same document calls the riskiest surface, and it is
recorded nowhere in DESIGN.md.

**Two sub-claims of the filing are REFUTED, and are stated so the owner does not
act on them.** (1) *"`:808` denies patches that shipped"* is wrong: *"No patches
to `stack.c`, `defines.h`, `keyboard.c`, `softmenus.c`, `statusBar.c` until PP4
(the browser stage)"* is a forward-looking staging constraint, and the cited
review says so verbatim (*"legitimate post-PP4 additions the sentence
anticipates"*). (2) *"`:835` says one hunk at `solver/equation.c` against 5
measured"* is wrong: §9 is a **staging** table, and PP5's tree at `888f46343`
has exactly 1 hunk; it grew to 4 at PP14 and 5 now. Also minor: the filing calls
this "§6"; at this tip the table is under `## §7` (line 777), and line 808 is
correct.

**What it costs the owner.** At the next upstream merge or the next composing
package, the two largest override surfaces must have their adjacency re-derived
from scratch — the per-file analysis this table exists to hold.

**Bug class.** *An inventory that is a claim, kept by hand.*

**Class-level test.** `design-audit.sh` compares the §7 table's file list
against `ls patches/*.patch` and fails on a difference. This one is
mechanisable, which is the argument for doing it rather than re-typing the
table.

---

## 4. PLAUSIBLE

**None this round.** Every filing that survived refutation carries a
constructed, and in eight of nine cases an executed, reaching path. Two
evidentiary residues belong here rather than in a fourth-section finding, and
they are stated so the record does not overclaim.

**`PP18RR10-2` is the one CONFIRMED finding with no execution behind it.** Every
link was read at the audited tip — the classification of `+` as a dyadic, the
16-byte size of `dtDate`, the payload cap, `addDateReal`'s success, the absence
of an opaque node, and the formatter's `default: return false` — but no reader
pressed `Date→DT`, `5`, `+`, `CLSTK`, `PHIST` on the simulator. **What would
settle it:** that keystroke sequence under `run-sim`, with the browser row
captured. It is a ten-minute check and it should be done before the finding is
acted on.

**`PP18RR10-6` is unreached by any input that exists today.** Its mechanism was
confirmed statically end to end — the size table, the block arithmetic, the
header macro — so it is not refutable as impossible; it is a trap laid for the
next person who closes `PP18RR10-5`. It is filed CONFIRMED rather than PLAUSIBLE
because the defect is present in the code, not conditional on a hypothesis.
**What would settle it:** add the `dtLongInteger` cell under ASAN and watch the
allocation.

---

## 5. Design observations

**D1. The wave repairs by second copy, not by single source.** The placeholder
rule now exists twice — the pager's bare literal `20` at `prettyFormula.c:859`
and `:884`, and the browser's named `PB_UNSHOWN_H` at `prettyBrowser.c:48`. The
SYSFL bound was extracted into `prettySysflRows()` and then **re-duplicated
inside the test written to stop it drifting**. Both copies agree today. Every
future repair to the history surface must be made twice, and in the pager's case
the copy that ships is the unpinned one.

**D2. The wave's evidence splits cleanly along one line.** The two repairs that
predate their pins — the placeholder and the atom window — are proved by marked
mutations and hold under re-derivation. The two pins written to close round 9's
coverage findings — FB1's clause and T33's table — assert less than their
comments say. The commit message lists mutations for exactly the first pair. The
rule this suggests for the next wave is one line: **for each new pin, delete the
production line it names and check the pin goes red.**

**D3. Half the headline repair went into a body no keypress can raise.** The
pager's unreachability is ruled and is not a defect (§6). The owner should still
know that the owner-visible half of PP18RR9-1 is the browser's message string,
not the pager's placeholder line.

**D4. The browser's key path has never been observed.** The suite proves the
browser's keyboard arms by calling the handlers directly (FV12). Where a
mutation was judged impossible, `TESTING.md` substituted a trace (MUT-76) — and
that trace both stopped one branch short and named the wrong function.
`PP18RR10-1` is what that costs.

**D5. Sibling polarity is inverted at `keyboard.c:2793`.** R5-3 rules that a
package must never rely on a sibling for containment. Here the sibling's `case
20:` is the only thing that keeps `.d` off the bug screen, and the package's own
**solo** gate — the configuration it ships alone in — is the one where the
defect is worst.

**D6. T33's cell schema cannot express a `numericFont`-only absence.** `noTiny`
is its only escape flag. The wide base-2 glyphs this wave promoted to atoms are
absent from `numericFont` and present in the other two. The cost is nil today
(`numericFont` never draws a `ppfFormatStaged` spelling, because
`ppfBuildCurrent`/`ppfBuildEntry` are only ever called with `ctxFont =
PP_FONT_STANDARD`), but the schema cannot record a direction that exists.

**D7. `pbFindResult` duplicates `ppfBuildEntry`'s token table**, and the code
says so: *"this decoder must know every token `ppfBuildEntry` knows, or an entry
renders but refuses to recall"*. It predates this range and its failure mode is
a clean `prettyBrowserLeave()`, not a wrong number. Worth a pin some day.

**D8. `prettySysflRows()`'s fallback reinstates the quantity the function exists
to avoid** (`return NUMBER_OF_SYSTEM_FLAGS`, `prettyFormula.c:755`). It is
unreachable while `-MNU_SYSFL` sits in the static `softmenu[]` table, and FB1's
`rows < 61` arm would fire loudly if it were not. `0` would be the safer
default.

**D9. The LIT/VAL spelling split stands, and is worth watching.** A literal leaf
holds the as-typed `FF#16` and a value leaf holds `FF₁₆`, so the same number can
bracket differently across the two leaf kinds. Ruled in r9 §5, re-confirmed by
refutation this round. It is the thing to revisit if option A is ever restated
as *"the leaf's TYPE decides"*.

**D10. Two surfaces, one walk, no shared code.** The pager and the browser hold
independent copies of the packing walk. Round 3 filed the divergence when only
the browser copy had a placeholder; round 9 closed it by copying the fix. The
next divergence is a matter of time, and only one copy has a test.

---

## 6. Deliberately not flagged

### 6.1 Filings that the refutation pass killed

**T34 leaks `screenUpdatingMode` into every later row.** REFUTED by instrumented
probe. `fnPrettyHist` does set the three manual bits and T34 does not restore
them — but T34 dispatches `ppcTestOp(ITM_PCLR)` two statements later, and
upstream's `reallyRunFunction` ends its non-program path with an unconditional
`screenUpdatingMode = SCRUPD_AUTO` (`items.c:353`). Probes: 14 after
`fnPrettyHist`, 14 at the end of the block, **0 after PCLR**; the T29 snapshot
reads 0 and its restore writes 0. The five cited convention sites bracket blocks
that paint directly with no trailing item dispatch — the property T34 does not
share.

**The PHIST pager is unreachable, so the placeholder is dead code** (filed twice,
by two dimensions). REFUTED as a re-report. The PP1-PP16 audit's not-a-defect
list rules the retention deliberate (*"`fnPrettyHist`'s pager body is
unreachable through the keyboard and is retained deliberately … the non-browser
fallback surface and for the packing reference"*) and records the mutation that
proves the code is dead. Round 3 received the same claim almost verbatim and
returned REFUTED as a code defect, splitting off only the coverage half
(`PP18RR3-14`, which T34 now closes) and the documentary half (`PP18RR3-15`) —
both KNOWN ids. `git log -S` shows the browser-divert guard and the containment
arm landed in the same commit `c6f85a267`, so the pager was never
reachable-then-broken. The consequence clause also fails: the owner-visible
placeholder is the browser's, and PHIST does raise the browser.

**PP18RR9-2 missed the as-typed `21#16` spelling.** REFUTED as fenced. Ruled
three times: r7 declined it as cosmetic at the same call site, r8's PP18RR8-6
filing explicitly fenced that path out (*"that note names a different call
site"*), and r9 §5 restated it as a design observation. `DESIGN.md:588-590`
governs the two-byte **subscript** window; `21#16` carries an ASCII `#` and no
subscript. Inside the run's stated fence ("all PP18RR7/RR8 dispositions").

**ENTER-recall half-completes when `reallocateRegister` refuses.** REFUTED on
intent, with the convention cited. Upstream's own recall does exactly this:
`fnRecall` (`recall.c:28-33`) commits `liftStack()`, then
`copySourceRegisterToDestRegister` (`registers.c:1532-1538`) returns on
`ERROR_RAM_FULL` with the copy skipped, the lift kept, no unlift and no error
clear. The package's `lastErrorCode == ERROR_NONE` guard is the same guard
written inverted and **wider**. The 2026-08-24 upstream-conventions ruling and
the standing *"a package-side workaround is correct only when no upstream
convention covers the case"* make a rollback the forbidden idiom here.
`DESIGN.md:191-193` already rules that a failed operation *"may have partially
moved the stack"* and that the answer is to invalidate the shadow, which runs
(`prettyBrowser.c:230`). The `screenUpdatingMode` sub-claim is simply wrong —
`prettyBrowserLeave()` sets `SCRUPD_AUTO` on every exit.

**T33 omits `dtLongInteger`, the commonest leaf type.** REFUTED by mutation. A
tiny-missing glyph appended to the `dtLongInteger` arm reddens 23 assertions
over seven tests (T25, T28, T23b, FV3, B4 measure, B4 layout, B7, B10, and T32b
fifteen times), because `ppfBuildRow`'s tiny rung re-fonts the whole tree and
T25/T28's filed entries stage a long-integer result. The type is covered, by
other pins. What survived is `amMultPi` (`PP18RR10-5`).

**T33 covers three of four types and one magnitude, so three alphabets can go
fatal silently.** REFUTED in two of its three parts, by the same probe: with
`tinyFont` made to lack `STD_SUB_10`, the gate reds at T29 twice, so the
exponent form and the long-integer arm are pinned. Only the `amMultPi` third
survived, and it is `PP18RR10-5`.

**`ppfBuildRow` should accept the wide row at rung 0 when `canPan`.** REFUTED in
its wide form. `prettyInternal.h:175-178` documents `canPan` as a **width**
relaxation at the last rung, `DESIGN.md:119` terminates the ladder at `fail`,
and T28 pins the panning caller receiving the wide row from the **tiny** rung —
so accepting at rung 0 inverts a pinned order. What survived is the narrow
claim, `PP18RR10-3`: the decline whose recorded surface is the pager is being
paid on the browser.

### 6.2 Axis (a) — the placeholder's neighbours, cleared

This is the axis's honest answer, and it is that the axis is clean.

- **The browser already had the placeholder before this wave.** The diff to
  `browsers/prettyBrowser.c` is the message string alone; `PB_UNSHOWN_H` and its
  comment (*"both passes reserve a fixed-height placeholder for it, so every row
  pages, selects and marks like any other"*) predate `80533eac4`. Only the
  **pager** met new rows, and the pager has no selection, no ENTER-recall and no
  pan.
- **Selection still marks an undrawable row**: the 3 px marker is drawn at
  `prettyBrowser.c:81-83`, before the built/unbuilt split.
- **ENTER still recalls from an undrawable row**: `pbFindResult` walks the token
  stream and never needs a built root, and the live-row short-circuit does not
  depend on drawability. An undrawable row still has a stored TKRES.
- **No caller reads the stale root.** `ppfBuildRow` calls `ppReset()` before it
  fails, so the retained index is dangling — but `built` guards `ppPaintAt` in
  both surfaces' pass 2, and `h` is the only field the placeholder path reads.
  Checked on the write-set, not on the shape.
- **Page counts cannot drift from the placeholder alone**: both passes of both
  surfaces substitute the same height, and the pager's two passes use the same
  literal and the same break test.
- **`pages = page + 1` cannot wrap and `ppfPage % pages` cannot divide by
  zero**: `PPC_HIST_MAX` is 12, so `totalRows <= 13` and `page <= 12`.
- **The `page <= ppfPage` loop condition** builds one row past the painted page
  and paints nothing. Harmless.
- **The 20 px reservation fits its own string**: `standardFont`'s `(` is 2 + 16
  = 18 rows from the top, and the last row on a page satisfies `y + h - 1 <=
  163`, so ink stops well above the frame line at 168 and cannot reach the next
  row's band.
- **`pbPan` grows by 60 with no clamp on a placeholder or narrow row** — the
  clamp sits inside both `if(built)` and `if(width > visible)`. It is never read
  in those states, Up and Down zero it, and `int16_t` overflow needs about 546
  consecutive presses with nothing visible either way. Pre-existing, and moot
  given `PP18RR10-1`.
- **`prettyBrowserPan`'s clamp comment** (*"the paint clamps this to the row's
  width"*) is false for a placeholder row, for the same reason and with the same
  absence of consequence.
- **The pager paints an empty framed screen when `totalRows == 0`** while the
  browser says "no formulas". A real divergence with zero impact, inside the
  unreachable arm.
- **Two direct `showString` calls from paint**, against §1's clearing-extent
  rule. Not a defect: the rule governs measured layout **nodes** clearing each
  other, and the placeholder is not a node. The pre-existing browser message did
  the same thing.

### 6.3 Axis (b) — the alphabets, cleared to the font tables

- **Every code point a value leaf can emit was checked against the generated
  font tables.** All fifteen base subscripts `0xa461`-`0xa46f` resolve in
  `tinyFont`, so no base other than 16 hides a decline. `0xac66` (`amMultPi`)
  and `0x80b0` (degree) resolve in `tinyFont`, so round 9's suspected further
  members of the PP18RR9-1 class are **not** fatal. Only `0x82b3` and `0x9d4d`
  are missing from `tinyFont` — exactly the recorded pair. **The record is
  accurate. It is incomplete, which is `PP18RR10-5`.**
- **The wide base-2 glyphs `0xa20e`/`0xa027`** resolve in `standardFont` and
  `tinyFont` and are absent only from `numericFont`, which costs at most one
  rung on `PP_SURF_INLINE` and never a decline.
- **T33's rot direction is two-sided**: a `noTiny` cell that starts resolving
  fails with its own message (`prettyTest.c:1889-1892`). That half cannot rot
  silently.
- **T33's `numericFont` column is over-strict rather than wrong.**
  `numericFont` never draws a `ppfFormatStaged` spelling; `PP_FONT_NUMERIC`
  appears only in the register-line rungs, which feed a different formatter that
  refuses angular-tagged reals, polar complexes and short integers outright. The
  column costs nothing and passes.
- **T33's space-class exemption mirrors `ppRunInk` exactly**
  (`prettyLayout.c:194-200`). `STD_SPACE_PUNCTUATION` `0xa008` is genuinely
  absent from `tinyFont`; the exemption is what makes that safe, at both sites,
  consistently. The font-aware version of this claim was killed in refutation
  for round 9 (a font-aware window turns five pins red).
- **The widened atom window was re-checked at every consumer** — the class this
  catalog records as coming back short twice. `prettyFormula.c:479` (PPN_LIT),
  `:504` and `:620` (`ppfValBuf`), `:591` (PPT_TKL), `:162` (`ppfRunPrec`),
  `prettyVisual.c:982` (PPA_LIT). The walker site is inert: `ppvLiteral` refuses
  any byte that is not a digit or `.`, so no hex text can arrive there.
- **No non-based spelling can sneak through the new `A`..`F` conjunct.** The NIM
  buffer spells exponents with lowercase `e+` (`bufferize.c:1231`, `:1245`); the
  display exponent is `PRODUCT_SIGN` + `STD_SUB_10` + superscript digits, all
  two-byte and still rejected; `shortIntegerToDisplayString(…, determineFont =
  false)` cuts off the wide-numeric `NUM_0_b` alphabet; the rejected `-` sign and
  the overflow string both fall to `PPF_PREC_ADD`, which brackets — the
  conservative direction.
- **`ppfRunPrec`'s first-char gate agrees with the widened predicate**: bases 2,
  4, 8 and 16 force `sign = 0` (`display.c:2027`), so a based numeral never
  begins with `-`, and a leading hex letter short-circuits to ATOM either way.

### 6.4 Arithmetic and bounds, cleared

- **EQ37 is sound in both directions.** After `ppReset`, `0 + 511 + 1 > 512` is
  false and the fill is accepted, taking `ppTextLen` to exactly 512; the second,
  empty run computes `512 + 0 + 1 > 512`, true, and is refused. Both arms are
  reachable and each flips on a one-byte change to `ppNewRun`'s guard. The
  512-byte static fill is inside `#if defined(PC_BUILD)` and costs no device RAM.
- **T32c is not vacuous.** `0 − 1` at the default WSIZE should be all ones,
  which would emit only `STD_BINARY_ONE` and make the `0xa20e` probe
  unsatisfiable. The dumped signature is a mixed `0xa027`/`0xa20e` run, so the
  fixture reaches both wide glyphs. The inline comment *"all 64 bits set at the
  default WSIZE"* is inaccurate about the value; the pin it guards is sound.
- **T32b's oracle is real.** `"P("` is the actual `PP_PAREN` marker in
  `ppfTestSigNode`, the subscript guard means a lost leaf reddens rather than
  skipping, both `continue`-after-fail arms call `ppTestFailInt` first, and
  `ppcTestReset` clears `lastIntegerBase` so iteration *b* is not refused by
  iteration *b−1*'s base mode — one of the three recorded fixture traps, handled.
- **T34's oracle direction is right.** `LCD_SET_VALUE` is 0, which selects
  `BLT_ANDN` and therefore clears, so the pre-fill is a clear and the `!= 0`
  test is live. The band 25..163 is exactly `PPF_BAND_TOP`..`PPF_BAND_BOTTOM`,
  and the frame lines at 20 and 168 sit outside it.
- **The new flag-browser bound cannot wrap.** `f + fOffset > sysflRows - 1` is
  int arithmetic on both sides after promotion, and `prettySysflRows()`'s return
  type matches `softmenu_t.numItems`. The sibling report's arithmetic is right:
  upstream `64+48`, package `64+51`, break at `> 114`, 113 supplied rows, two
  past.
- **`ppfValBuf`'s 160-byte worst case holds**: base 2 at WSIZE 64 is 64×2 digits
  + 15 group glyphs ×2 + 2 subscript. Every copy into it is length-guarded, and
  the `dtShortInteger` detour through `tmpString` carries its own
  `_Static_assert`.
- **`ppfBuildEntry`/`ppfFromCaptureNode` stay in bounds**: `text[32]` with `l <=
  15` and `cl <= 15`; `stackNode[8]` guarded by `sp >= 8` before every push and
  `sp < 1`/`sp < 2` before every pop; `PPC_LIT_CAPACITY` 30 into
  `ppcNimText[32]` with room for the NUL.
- **T33's two-byte decoder** advances by 2 on any byte `>= 0x80` without
  checking that the second byte is not the terminator, unlike `ppRunInk`. No
  formatter on this path emits an orphan trailing high byte; every spelling
  dumped was complete. Unreached, and test-only.
- **`ppfTestSigNode` strips ASCII `0x20` from run text**, the r5 R5/R6 shape. No
  glyph in the reachable value alphabet has `0x20` as either byte, so no glyph
  is orphaned.

### 6.5 Upstream discipline, cleared

- **The override change is the right direction**: eight inline lines in an
  upstream file became one call, the patch stayed at one hunk on a virgin file,
  and the marker comment follows the package's convention. The `fnOpenMenu`
  citation is accurate as a **shape** citation — `softmenus.h` exposes no helper
  to call instead, so upstream-convention-first cannot close it by reuse.
- **Placement is correct**: `files/` carries `prettyBrowser.c` (package-owned)
  and not `flagBrowser.c` (an override, shipped as a patch), and the manifest
  hashes moved for exactly the six touched files.
- **No target link exposure.** `ppfTestStagedSpelling` is declared unguarded in
  `prettyInternal.h` against a PC_BUILD-only definition; its only caller sits
  inside `prettyTest.c`'s own PC_BUILD block, and the neighbouring test seams are
  declared the same way. `prettySysflRows`'s declaration entering
  `prettyPrint.h` widens the shared namespace, but the name is package-prefixed
  and the include already existed.
- **`solver/equation.c`'s 578 adds are the package's largest drift risk**, and
  the extraction carries a recorded timing decision — *"rebase-adjacent stage
  work, never mid-audit. Relocating state is this project's most dangerous fix
  shape."* Only the documentation half is reported (`PP18RR10-9`), which has no
  timing bar.
- **`keyboard.c`'s hunks** were ruled *"hunk-dense but irreducible … No
  finding"* by the 2026-08-27 minimality review, and this wave does not touch
  them.
- **One incidental upstream observation, not raised**:
  `src/generateTestPgms/generateTestPgms.c:2675` emits a program step for flag
  `NUMBER_OF_SYSTEM_FLAGS - 1`, which the package's `64+51` turns into flag 114
  = `FLAG_PTLINE`. That generator is a host tool outside the package's override
  set, the gate is green over it, and no owner-visible consequence could be
  constructed.

### 6.6 Housekeeping named, not argued

The bare literal `20` twice in the pager against the browser's named
`PB_UNSHOWN_H` (D1 covers the substance). A system-flags helper living in
`prettyFormula.c`. T30's assert-failure oracle, which the new
`ppcTestExpectHist(…, 2)` control — exactly what PP18RR9-6 asked for — now backs
with a positive filing check. And T31's new `"53.130"` needle, which hard-codes
a `.` radix and an 8-significant-digit spelling and would redden under FIX 2:
that is a **false-red** risk, not a cannot-fail one, and no row in the suite
leaves the radix or display format changed ahead of it, so it is named here
rather than filed.

---

## 7. Verdict

The nine repairs are correct at the sites where they were typed. Each was
re-derived rather than accepted from the commit message: the pager placeholder
in both passes, the browser's honest message, T33's seam and its cell mechanics,
the widened atom window at every one of its six consumers, T32b's fifteen-base
loop, T32c's substituted alphabet, the extracted SYSFL bound, T31's magnitude
assert, T30's filing control, EQ37's pool bound and the `fnOpenMenu` citation.
The functional half of the wave holds. Axis (a) is clean at every consumer I
could reach, and axis (b)'s recorded cells are accurate.

**I would not ship this tip as closing round 9.**

Where it breaks first is the browser's pan key. In the package's own solo build
the owner presses g and `.d` on a row too wide to read and gets a full-screen
firmware-bug panel; in the composed build the key does nothing. `prettyBrowserPan()`
cannot be entered from real input in either, so a wide history row can never be
read past its left edge — and this wave's own `ppfBuildRow` is what lets those
rows into the browser at any width.

Second is a filed date-arithmetic row. Capture admits `dtDate`, `dtTime` and
short `dtString` as ordinary value leaves; the formatter has arms for four other
types and declines these; and the round-9 placeholder turns what used to be a
silent drop into a permanent "(cannot draw)" line the owner will see every time
they open the browser.

Third is the flag-browser override, which still has no drift guard while
`DESIGN-HISTORY` and the commit message both record that it has one. That is the
worst kind of gap: the gate is green, the record says covered, and the mutation
that defines the defect still passes.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.** `PP18RR10-7` — the pass-1 pin guards a body no keypress can
raise, and the two literals are eight lines apart in one function. `PP18RR10-9`'s
table rows: the reader-cost is real but the fix is re-typing prose that will go
stale again, so it is worth doing when `design-audit.sh` is next opened and the
comparison can be mechanised, not before. `PP18RR10-6` unless `PP18RR10-5` is
acted on — they are one task, and the seam is only dangerous the moment the cell
is added. `PP18RR10-8` I would fix, because it is one sentence and the mutation
proves the sentence is false.

**What I would act on today**, in order: `PP18RR10-1`, and with it the class
test that observes handlers instead of calling them — that single test is what
would have caught the pan key, and it is the test shape this package has been
missing since PP10. Then `PP18RR10-2`, after ten minutes on the simulator to
turn its static trace into an observed row. Then `PP18RR10-4`, because it is the
only repair in the wave whose class is memory safety and whose pin cannot fail.

---

## 8. Round and exit state

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does
> not count toward the exit criterion's two clean rounds and its verdicts carry
> one family's blind spots (SKILL.md, Exit criterion).

**Round 10, in-family only.** Subject `2ae202759..80533eac4`, two commits,
audited tip `80533eac4`, branch `pretty-print/stage-pp18`. Readers: eight
in-family dimensions (contracts, lifecycle, arithmetic, error paths, guards,
tests, design, upstream), blind to each other, followed by an independent
refutation pass over every filing under the reachability, correctness and intent
lenses.

| reader | filings | reached a verdict | survived |
|---|---|---|---|
| in-family, 8 dimensions | 29 | 19 | 12 |
| sol / gpt | — | — | not run |
| gemini | — | — | not run |

**Result.** Twelve survivors merge to **nine ids**, `PP18RR10-1` … `PP18RR10-9`.
Three dimensions filed the FB1 clause independently and two filed the pan key by
different mechanisms; independent agreement across blind dimensions is why those
two rank where they do. Seven filings were killed by refutation. No PLAUSIBLE
residue (§4). Ten design observations (§5). Seven killed filings and about forty
cleared items (§6).

**Accounting gap, for the operator.** The eight dimensions report 29 findings
between them in their own verdict lines, and 19 distinct claims reached the
refutation pass. I cannot account for the missing ten from the material handed to
this report — they are presumably duplicates merged before verification, but
three FB1 filings and two pan-key filings arrived unmerged, so the de-duplication
was not uniform. The next run should carry the filing-to-verdict map.

**Fenced ids, from the dispatch, not re-reported.** The PP18RR8-6 ruling and its
PP18RR9-2 domain completion; the blank-to-decline change; the RAD and GRAD tiny
declines RECORDED as accepted, with the pager placeholder as their surface; all
PP18RR7 and PP18RR8 dispositions. All `PP18RR1..RR9` and `PP18R4-1..11` ids are
KNOWN. `PP18RR10-3` and `PP18RR10-5` sit next to fenced items and each states in
its own text which claim it is **not** making.

**Pre-verified facts, carried and used rather than assumed.** The gate is GREEN
at this tip, refresh-first, in this session — every mutation in §2 was run
against that baseline and reverted. The churn scan exits 0; it was re-run for
this report and the table is in §2, and it shows **fourteen** patch files where
the dispatch says fifteen. The red-first and mutation evidence in `80533eac4`'s
message was re-derived: the placeholder revert and the window revert do red T34
and T32c, and the message claims no mutation for PP18RR9-4, which §3 confirms.
All three recorded fixture traps were used — base-*b* entry mode refusing a
foreign digit key (T32b's per-iteration reset), a live `0−1` formula not being a
value leaf, and `fnPrettyHist` routing to the browser unless `calcMode` is
CM_PRETTY_BROWSER (which is `PP18RR10-7`'s coverage argument).

**Exit criterion: NOT met.** The rule is two consecutive rounds with no new
CONFIRMED finding, at least one of them out-of-family. This round has nine
CONFIRMED findings, so a real finding resets the count; and it has no
out-of-family reader, so it could not have counted toward the two even if it had
been clean. The counter stays at zero. The base is still unpushed.

**Runner defects, carried forward.** Every worktree spawned at `e21af8d28`, 150
commits behind the audited tip and **not an ancestor of it** — the eighth
consecutive round. Every reader ran `git log --oneline -1` first and checked out
`80533eac4` before its first read, so no verdict in this report rests on the
wrong tree, but the trap is now costing a step per reader per round. Two further
traps recorded this round: `./packages/forth-core/build-test.sh` does **not**
compile this package, and one reader's first probe run produced zero probe lines
because of it (the gate is `./packages/pretty-print/build-test.sh`, and
`prettyTestCapture`'s output lands in `build.sim/meson-logs/testlog.txt`, not on
the console); and two verifiers wrote to the same `/tmp/baseline_solo.log`
concurrently, interleaving a sibling worktree's ninja output into one reader's
log — that run was discarded and repeated with a private log inside the reader's
own worktree. Verification logs must be worktree-local.
