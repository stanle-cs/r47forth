# Audit — PP18 round 3 (restarted series), whole stage, at `34ac6e97f`

Subject: `pretty-print/stage-pp17..HEAD` on `pretty-print/stage-pp18`, tip
`34ac6e97f`. **No fix wave exists.** The tree is byte-identical to the one
restarted round 1 and round 2 read — three rounds, one tree. What rotates is
the question. This round runs the axis **both** halves of round 2 put at the
top of their own priority lists:

- **(a) the acceptance-parity oracle** across `ppqParse`, the equation
  evaluator (`ppEqBigopIntercept` and `_parseWord`) and the VISUAL walker —
  three acceptors of one construct language that must agree on what is
  drawable. `PP18RR1-D3` asked for it; neither half of round 2 reached it.
- **(b) `prettyFormula.c` and `prettyLayout.c` internals** — the last two
  package sources with no full pass, and the pair `PP18RR2-12` sits between.

Eight in-family finder dimensions ran blind to each other over that scope;
every raised finding went to an independent refutation pass with one assigned
lens (reachability, correctness, intent), instructed to default to REFUTED and
to prove coverage claims by mutation.

**Fifteen CONFIRMED findings, one PLAUSIBLE, six REFUTED, one dropped as a
known duplicate.** Twenty-three were raised; seventeen survived refutation. Of
those seventeen, one is a fourth independent re-derivation of a ruled item
(`PP18-6`) and is not re-filed, and one could not have its reaching input
constructed and is filed PLAUSIBLE rather than promoted. Nine of the fifteen
are backed by a probe or mutation applied, built through the package's own
gate, observed in `testlog.txt` and reverted; two more rest on measured build
artifacts — the shipped font tables and the disassembled ARM object.

**Axis (a)'s answer is mostly good news, and it is the most useful thing in
this report.** The two acceptance *tests* that matter are genuinely
single-sourced — `ppEqConstructIs` decides construct spelling for the parser
and the evaluator, `ppEqFunctionItem` decides function names for the parser and
the walker — and four dimensions tried and failed to break either. Every
parity break found this round except one runs in the **safe** direction: the
renderer is stricter, so the picture declines and upstream's linear line
remains. The single dangerous-direction break is `PP18RR3-4`, where the
evaluator's own MVAR pass refuses to enumerate a variable the renderer draws
happily — a picture that will not compute, which is precisely the harm
`DESIGN.md`'s shared-spelling rule was written to prevent.

**Axis (b) found less than axis (a), and that is also a result.** Every
measure/paint pair in `prettyLayout.c` was re-derived numerically by two
dimensions and the two passes agree; the history ring's decode arithmetic is
bounded at every token; `prettyFormula.c`'s pool guards hold. What the pass did
find in those two files is not arithmetic at all: the layout engine measures
from `numericFont` and upstream paints from `numericFontBold`
(`PP18RR3-1`), and the browser's pan is the one negative-x producer in the
package with the only ink primitive that was never hardened for it
(`PP18RR3-3`).

Nothing was fixed. Every probe and mutation was applied, built through the
package's own gate, observed in `build.sim/meson-logs/testlog.txt` and
reverted inside an isolated worktree; the main tree is clean at start and
finish (`git status --porcelain packages/` empty, `grep -rn AUDIT-PROBE
packages/` empty).

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).

### Subject

**Tip.** `34ac6e97f` ("docs: skill defect handoff — the out-of-family pass can
be skipped silently"). Range `pretty-print/stage-pp17..HEAD`: fourteen
commits, 26 files, +8,668 / −859 — identical to rounds 1 and 2, because no
commit has landed since either report. The *reading* scope is different and is
stated per axis above. Primary subject: `prettyFormula.c` (776 lines) and
`prettyLayout.c` (847), plus the three-acceptor seam —
`prettyEquation.c` (975), `packages/pretty-print/solver/equation.c`'s PP14
evaluator block, and `prettyVisual.c`'s layout half.

**KNOWN, excluded from re-reporting** (verified still present, then fenced):
`PP18RR2-1..17` and `PP18RR2-OOF-1`; `PP18RR1-1..12` and `PP18RR1-P1`;
`PP18R4-1..11` and round 4's plausible carry; the pre-restart series
`PP18-1..16` and its rulings. Where a new finding sits adjacent to a known one
it states the distinction in the finding.

**One raised finding was dropped as a duplicate, not filed.** The walker
double-parenthesising an additive `DERIV` body (`prettyVisual.c:1124` wraps at
`PPF_PREC_MUL`, then `ppqBuildBigop`'s DERIV arm wraps again at
`prettyEquation.c:298`) is **`PP18-6`**, ruled leave-alone by the 2026-08-28
round 1, re-affirmed by round 2, and re-derived and re-fenced by restarted
round 1. Two dimensions reached it independently this round and one verifier
*measured* it for the first time — walker signature `P(P([x + 3]))` against
parser `P([x + 3])` for the same mathematics — but the standing ruling's
condition for revisiting is explicit ("fix it when the height consequence is
measured and shown to push a real formula out of the Z/T band"), and this
round did not measure that. The measurement is recorded in §6; the ruling
stands. That is three independent re-derivations of one ruled item, which is
itself a data point about the ruling's discoverability.

**Numbering.** This round's findings are **`PP18RR3-1`–`PP18RR3-16`**, its
plausible **`PP18RR3-P1`**, its design observations
**`PP18RR3-D1`–`D6`**. `grep -rn PP18RR3` over the repository returned nothing
before this file was written. In-code comment tags `AUDIT R1-*`, `R2-*`,
`R3-*`, `R4-*` quoted below are the *package's own* audit series, not this one.

### Coverage (union across the eight in-family dimensions)

**Read at line level, in full, by more than one dimension:**
`prettyFormula.c` (all 776 — value staging, `ppfCombine1/2`, `ppfBigop`,
`ppfFromCaptureNode`, the `ppfBuildEntry` token decoder, `ppfBuildRow`, the
`fnPrettyHist` pager) by six; `prettyLayout.c` (all 847 — pools, metrics init,
`ppRunInk`, every `ppMeasure` arm, `ppFillVal`/`ppShowRun`/`ppDrawLine`/
`ppBigopBox`/`ppDrawIntegralSign`, every `ppPaint` arm, `ppSetFontDeep`,
`ppPaintAt`, `ppRenderRightAligned`) by six; `prettyEquation.c` (all 975 —
the whole `ppq*` cluster, `ppqBuildBigop`/`ppqBuildCall`, both frame builders,
`ppqShowRender`, `fnPrettyEqShow`) by five; `prettyVisual.c` (all 1,623) by
four; `browsers/prettyBrowser.c` (all 251) by four; `prettyInternal.h` and
`prettyPrint.h` in full.

**Read for the parity axis:** `packages/pretty-print/solver/equation.c` —
`ppEqFunctionItem` (:217-244), `_parseWord`'s MVAR / XEQ / FUNCTION arms
(:1055-1309), `_operatorPriority` and the operator stack (:741-1070),
`parseEquation`'s scan loop, label-prefix scan and the intercept hook
(:1311-1520), `showEquation`'s display-string builder including
`_showExponent`/`_checkExponent`, and the whole PP14 block (:1714-2274) — by
five dimensions, with only the SUM/PROD accumulator walk (:2130-2274) read by
fewer than two.

**Read at the seams:** `prettyValue.c` at the two numeric-font surfaces
(:740-890); `prettyCapture.c` at `ppcValLeafFromRegister`, `ppcSerializeNode`,
`ppcEmit`, `ppcHistEvictOldest`, `ppcHistoryEntry` and the DONE hook;
`prettyTest.c` — `prettyTestFormula` in full, `prettyTestEquation`
`EQ1`–`EQ25`, the visual fixtures and pins, the signature printer, the pixel
probes; `keyboard.c` at `determineItem`'s calcMode-20 arm, the containment
guard, the six browser key arms, the shift list and the `ITM_CC`/`op_j` case.

**Upstream verified by execution path, not assumed:** `src/c47/screen.c`
`showGlyphCode` (:1159-1290), `_doShowString` (:1355-1420),
`_refreshNormalScreen`'s head (:5869) and `RETURN_NORMAL` (:6042),
`refreshScreen` (:6169-6292); `src/c47/charString.c` `_calculateStringWidth`;
`src/c47/display.c` `real34ToDisplayString`/`…2`'s FIX and SCI arms;
`src/c47/ui/tam.c` `tamEnterMode`, `_tamProcessInput`'s label arm,
`leaveTamModeIfEnabled`; `src/c47/items.c` `runFunction`/`reallyRunFunction`
ordering; `src/c47/config.c`'s HP-35 preset row; `src/c47/registers.c`
`allocateNamedVariableOnMiss`; `src/c47/memory.c`'s stack-watermark field note;
both simulator HAL `bitblt24` implementations and `src/c47/hal/lcd.h`
`setBlackPixel`; `dep/DMCP5_SDK/dmcp/dmcp.h`'s ROM `bitblt24` declaration.

**Measured artifacts, not read:** `build.sim/src/ttf2RasterFonts/
rasterFontsData.c` — all 128 glyph codes shared between `numericFont` and
`numericFontBold`, compared field by field; and the shipped ARM object
`build.dmcp5/…/prettyEquation.c.o`, disassembled for the four `ppq*` prologue
frame sizes.

### Not reached, and it matters where

- **`ppqParse`'s interior was not audited for pin vacuity.** The tests
  dimension reached `prettyEquation.c:1-840` only through its pins and its two
  frame builders. That is half of axis (a): the `EQ2x`/`EQ3x` pins over the
  parser's own grammar rules are unaudited for whether they can fail.
- **`prettyCapture.c` and `prettyValue.c` as wholes** were not re-read; round 2
  owns them and its seventeen findings there are open.
- **`prettyTest.c` lines 823-1819** (`prettyTestCapture`'s `T`- and `B`-series)
  read only in spots, and `prettyTestEquation` `EQ26`–`EQ35` not read at all —
  which includes `EQ29`, the pin the MVAR ruling rests on.
- **The evaluator's SUM/PROD accumulator arithmetic** (`equation.c:2130-2274`)
  was read for override shape and control flow, not for numerical correctness.
- **No simulator ran and no LCD photograph backs any finding.** `PP18RR3-1`'s
  pixel claim is derived from the shipped font tables; the device-side half of
  `PP18RR3-3` and `PP18RR3-7` rests on the DMCP SDK's declarations and
  upstream's own field note, not on hardware.
- **`design-audit.sh` is forth-core's**; there is still no pretty-print
  equivalent, so no override-budget check ran. The substitute is §2's churn
  scan.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh --solo` is green at
`34ac6e97f`. Seven verifiers ran it to completion in isolated worktrees —
clean baselines at 183–265 s, `PRETTY-PRINT GATE GREEN`, testSuite OK — and
each re-ran it after reverting its probe. No new compiler warning. This is the
baseline every mutation below is measured against, not a discovery.

**The governing gate is the package's own.** The round-3/4 warning stands:
`./packages/forth-core/build-test.sh` refreshes only `packages/forth-core`, so
it returns a meaningless green for a pretty-print mutation. Two verifiers
re-derived this independently before choosing a runner.

**Probes and mutations this round ran** — all applied, built through the real
refresh (presence verified in `build.sim/custom_pkg_shadow/*` or in the
regenerated `files/` twin), observed in `build.sim/meson-logs/testlog.txt`,
and reverted.

| probe / mutation | observed result | finding |
|---|---|---|
| counters in both HAL `bitblt24` + a negative-`px` counter in `ppShowRun`, around T29's existing `prettyBrowserPan()` | `unpanned-oob=56 huge=0 \| panned-oob=39 huge=39 minhuge=4294967279(=-17) negGlyphs=3` | **PP18RR3-3** |
| state dump + softkey-band ink probe inserted into V36 after `ppvTestKeyIn("VDBL")` | `menu=-1378 su=2 ti=93 drsm=1`; post-refresh softkey ink **0**, `su=7 drsm=0`; after replaying keyboard.c's EXIT clear ink **0**; clearing `SCRUPD_MANUAL_MENU` alone ink **1179** | **PP18RR3-2** |
| `setEquation("SUM(A×X;X;1;3)")` + MVAR parse + `fnEqCalc`, beside EQ29 | `A before = 2199 (INVALID)`; `MVAR err=0 vars: (end)`; `A after MVAR = 2199`; `fnEqCalc err=8 type=1` | **PP18RR3-4** |
| HP-35 conjuncts + `DF_FIX`/19, `real34ToDisplayString(1e10, …, 110, 6, LIMITEXP, …)` into a 48-byte field with a 192-byte `0x7e` canary | `checkHP=1 gwl=3 gwr=3 seplen=2`; `firstWriteLen=49 bytesNeeded=50`; `canaryBytesClobberedPast48=2` | **PP18RR3-7** |
| `ppqParse` + `ppqFrameDerivative` on a 70-node fitting equation, then `ppqShowRender` twice (derivative mode vs plain) with row capture | `nodesUsed=70 pool=72`; `frameChanged=0`; `meas=1 w=364 h=29 wcap=396 hcap=147`; `prettyDeriv=1 prettyPlain=1 pixelsIdentical=1` | **PP18RR3-10** |
| `ppqParse` on the two display strings differing only in the exponent sign glyph | PLUS form never printed (declined); `AUDIT-PROBE R3 MINUS-exponent ACCEPTED` — one failure in the battery | **PP18RR3-11** |
| `ppqParse("Y = 1.5 / X")` and `ppqParse("Y = 1,5 / X")` beside EQ4 | dot control parses; `AUDIT-PROBE R3 comma form DECLINED` — one failure in the battery | **PP18RR3-6** |
| `prettyLayout.c:384` PP_BARS measure arm → `return false` unconditionally | gate **GREEN** (mutation confirmed in `custom_pkg_shadow/prettyLayout.c`) | **PP18RR3-13** |
| same, plus a temporary `else ppTestFail` on FV10's guard | gate **RED**, exactly one failure: `FV10 measure declined` | **PP18RR3-13** |
| bare `return;` inserted after `fnPrettyHist`'s browser-divert guard (the whole manual pager deleted) | gate **GREEN** | **PP18RR3-14** |
| walker vs parser node-signature dump for `d/dx(x+3)` | walker `P(P([x + 3]))`, parser `P([x + 3])` | **PP18-6** (known, §6) |
| `prettyVisual.c:1601` solver-status mask deleted (V19's own named mutation) | gate **GREEN** | **REFUTES** the V19-vacuity finding |
| same, plus a downstream `currentSolverStatus` read in `ppvPaintStackWindow` | gate **RED**, exactly one failure: `V19 a stale solver session changed the drawing` | **REFUTES** it conclusively |

**Two findings rest on measured artifacts rather than a probe.**
`PP18RR3-1` on a field-by-field comparison of all 128 codes shared between
`numericFont` and `numericFontBold` in
`build.sim/src/ttf2RasterFonts/rasterFontsData.c` — 109 differ, 94 of them in
row metrics, and the total `rowsAbove+rows+rowsBelow` is identical in both
fonts for every one of the 128 (which is why upstream's own clear is safe and
the package's measured-box clear is not). `PP18RR3-8` on
`arm-none-eabi-objdump` of the shipped `prettyEquation.c.o`: `ppqPrimary`
128 B, `ppqFactor` 48, `ppqTerm` 48, `ppqExpr` 40 — all four real,
non-inlined symbols, 264 B per parenthesis level.

**Upstream churn.** `patch_churn_scan.py` re-run over all 13 pretty-print
patches at HEAD: standing churn count **1** — `[WS-ONLY]`, the `showString`
wrap-reindent in `010-solver__equation.c.patch` — pre-existing, catalogued,
owned by `REVIEW_upstream-minimality_2026-08-27.md`, not re-reported. Totals
unchanged from round 2: `010-keyboard.c.patch` 76/3/11,
`010-solver__equation.c.patch` 619/1/5, every other patch ≤ 21 adds.

**One mechanical fact worth stating plainly.** Two mutations this round
deleted a production behaviour outright and left the gate fully green — the
PP_BARS measure arm, and the entire `fnPrettyHist` pager. Two others went red
with exactly one useful failure each. That ratio is §3's two test findings and
it is `PP18RR3-D6`; it is also the second consecutive round to produce it.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner, not by how clever it is. Ink
painted wrongly on the always-on surface for a whole population of owners
outranks a stuck menu on one feature's primary path, which outranks display
corruption that only the device can see, which outranks a picture that will
not compute, which outranks a truncation that reads as complete, which
outranks a crash nobody's ordinary use reaches, which outranks a pin that
cannot go red, which outranks a stale sentence in the authoritative doc.

Where the refutation pass corrected a finding's own claim, the correction is
stated **inside** the finding — five of them are corrections against the
finder, and two of those materially narrow what is being claimed.

For each: `file:line`, what breaks, the concrete reaching input, the violated
contract quoted, the bug class, and the class-level test. **No patches.**

---

### PP18RR3-1 — the engine measures with `numericFont` and paints with `numericFontBold`; `FLAG_BOLD` has no guard though its twin `checkHP` has one at both numeric-font surfaces

`packages/pretty-print/prettyLayout.c:567` (the `showGlyphCode` call with
`noPreClear` TRUE), against `packages/pretty-print/screen.c:1195`.

**What breaks.** `ppMetricsInit` caches every metric from `&numericFont`
(prettyLayout.c:52-91); `ppMeasure`'s PP_RUN arm sizes the box with
`stringWidth(…, m->font, …)` and derives ascent/descent from the *plain*
glyph via `ppRunInk`; `ppShowRun` then clears exactly that box and paints with
`noPreClear = TRUE`. But `showGlyphCode` substitutes a glyph from
`numericFontBold` whenever `getSystemFlag(FLAG_BOLD) && font == &numericFont`,
and advances `y` by the **bold** glyph's `rowsAboveGlyph` for the **bold**
glyph's `rowsGlyph`. Neither `stringWidth` nor `ppRunInk` ever consults the
bold table.

**Reaching input.** `SF 42` — `FLAG_BOLD` is `0x8069`, system flag 42, and it
is also the BOLD checkbox in the settings radio-button catalog
(`radioButtonCatalog.c:119`, `ITM_BOLD` dispatching `SetSetting`). Then any
value in numeric context: with FRACT on, `1 ENTER 3 /` on the X line (inline
rung 0 is numeric context, so the integer part and any sign run are
`numericFont`), or `PSHOW` of `1.5e30` (`ppFullRungs[0]` is numeric/numeric,
so mantissa *and* exponent runs are `numericFont`).

**Evidence.** Parsed field by field out of
`build.sim/src/ttf2RasterFonts/rasterFontsData.c`. `'0'` plain is
`rowsAbove 2 / rows 26`, bold `1 / 29`; `'-'` plain `17 / 3`, bold `16 / 4`;
of 128 shared codes, 109 differ and 94 differ in row metrics. So
`boxAscent = 2+26 = 28`, `ppRunInk` gives `asc 26 / desc 0`, the cleared box is
`baseline-26 .. baseline-1`, `py = baseline-28`, and the bold ink runs
`py+1 .. py+29 = baseline-27 .. baseline+1` — **one row above and two rows
below the box the node just cleared**, uncovered because `noPreClear` is true.
The fraction bar goes with it: `barTopRel = -(28-17) = -11` from the plain
`'-'` puts the bar on `B-11..B-10` while the painted bold `'-'` inks
`B-12..B-9`, so the bar and the minus sign stop coinciding, and the designed
2-row numerator clearance drops to 1.

**Corrections against the finder, both from the refutation pass.** (i) The
horizontal half of the claim is **wrong as stated**. Total advance
(`colsBefore+colsGlyph+colsAfter`) is identical in both fonts for all 128
shared codes, and `showGlyphCode` returns the advance from the bold glyph, so
non-first glyphs advance exactly as measured. Twelve codes do differ in
`colsBeforeGlyph`, but the finding's named example cannot occur —
`prettyValue.c:288` rewrites superscript exponent digits to plain ASCII before
building the run, and no `ppNewRun` in the package emits a numeric run
*starting* with one of the twelve. The 1-px first-glyph asymmetry is real in
the tables and unreachable in this code. (ii) "Upstream has the same mismatch"
is **false**, and the reason matters: upstream's `showString` passes
`noPreClear = false`, and `screen.c:1237` pre-clears
`rowsAbove+rowsGlyph+rowsBelow` of the *substituted* glyph — a total that is
identical in both fonts for every shared code. Upstream's clear always covers
its own bold ink. The package's measured-box clear is the only one that does
not, so the defect is package-owned, not inherited.

**Violated.** `prettyLayout.c:11-15`, the file's own §1 statement: *"All
metrics are derived from the live font data at first use … the fraction bar is
placed to cover the top rows of the minus sign's ink — a pretty fraction bar
sits exactly where a minus sign would, per font"*; and the BINDING
clearing-extent rule at `prettyLayout.c:539-541`: *"a node clears exactly the
box it measured, and never more … Clear the MEASURED box here and paint the
glyphs with `noPreClear`, so paint covers exactly what measure promised."*
The precedent for this exact hazard is in the package already —
`prettyValue.c:780`, `|| checkHP   // HP layout doubles glyph rows; our
metrics assume it off`, written for `AUDIT R1-13`: *"HP layout DOUBLES glyph
rows and our metrics assume it off; this surface used numericFont through the
same engine and had no such guard … the owner got a garbled screen."*
`FLAG_BOLD` is read five lines from `charCodeHPReplacement` inside the same
upstream function and has no guard at either numeric-font surface
(`prettyTryRegisterLine`, `fnPrettyShow`). `grep FLAG_BOLD packages/pretty-print/pretty*.c`
is empty.

**Bug class.** Metrics derived from one font while the paint layer may
substitute another — the R1-13 class, fixed at the flag it was found under and
not at its sibling.

**Class-level test.** Enumerate every upstream substitution `showGlyphCode`
can make under the fonts this engine measures with (`checkHP`'s
`charCodeHPReplacement`, `FLAG_BOLD`'s `numericFontBold`, and the `maxiC`/
`stdnumEnlarge` arms), and for each: assert either that the package declines
(a guard), or that the measured box contains the substituted glyph's ink for
every code in the shared set. A table pin over the font data, not a pixel
probe — the tables are in the build tree and can be read at test time.

---

### PP18RR3-2 — VISUAL's stack-window arm sets the exact two conditions that make `_refreshNormalScreen` paint nothing, so its own TAM softmenu pop is swallowed and DESIGN.md's "the menu and status bar keep working" is false

`packages/pretty-print/prettyVisual.c:1605`, against `src/c47/screen.c:5869`.

**What breaks.** `fnPrettyVisual`'s stack-window arm sets
`screenUpdatingMode |= SCRUPD_MANUAL_STACK` (0x02), `screenHoldsDrawnPixels`
and `temporaryInformation = TI_SHOWNOTHING`. Those are exactly the three
conjuncts of `_refreshNormalScreen`'s first statement:
`if(calcMode == CM_NORMAL && screenUpdatingMode != SCRUPD_AUTO &&
temporaryInformation == TI_SHOWNOTHING) goto RETURN_NORMAL;` —
`SCRUPD_AUTO` is 0x00, so any manual bit satisfies the second. The jump skips
`showSoftmenuCurrentPart()` (:6019) *and* `refreshStatusBar()` (:6032). The
suspension is not partial; it is total.

**Reaching input.** The feature's own primary path. From the PP softmenu press
VISUAL — item 984 carries `TM_LBLONLY` (`packages/pretty-print/items.c:2831`),
so `runFunction` routes into `tamEnterMode`, which pushes `-MNU_TAMLBLONLY`
(`src/c47/ui/tam.c:1273`). Type a label, press ENTER: `_tamProcessInput`'s
label arm is `reallyRunFunction(tamOperation(), value); leaveTamModeIfEnabled();`
(`ui/tam.c:1027-1029`) — the function runs **before** the TAM menus are
popped. `fnPrettyVisual` draws into the Z/T band and arms the three flags;
control returns; `leaveTamModeIfEnabled` calls `popSoftmenu`
(`packages/pretty-print/softmenus.c:3716-3721`), which clears
`SCRUPD_MANUAL_MENU` and sets `doRefreshSoftMenu = true`; the key handler
calls `refreshScreen(117)`; the early return fires; `RETURN_NORMAL` re-sets
all three MANUAL bits and `refreshScreen`'s tail clears `doRefreshSoftMenu`.
The one repaint request the pop raised is consumed and dropped. The dismissing
EXIT does not recover it: `keyboard.c:2552` clears only
`(SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_STATUSBAR)`.

**Evidence (measured).** A probe in the existing V36 pin, immediately after
`ppvTestKeyIn("VDBL")`:

```
AUDITPROBE0 menu=-1378 su=2 ti=93 drsm=1            <- MANUAL_STACK only, TI_SHOWNOTHING, request pending, real menu on the stack
AUDITPROBE1 postPopRefresh ink=0 su=7 ti=93 drsm=0  <- softkey row painted NOTHING; request consumed and dropped
AUDITPROBE2 afterExit ink=0 su=7 ti=0               <- replaying keyboard.c:2550-2552 exactly still paints no menu
AUDITPROBE3 control ink=1179                        <- clearing SCRUPD_MANUAL_MENU alone repaints the row
```

The control is the load-bearing line: the menu was ready to paint and only the
flag suppressed it.

**Two corrections against the finder, neither changing the verdict.** The
coverage claim ("all pins call `fnPrettyVisual(label)` directly, so no test
ever traverses the TAM entry path") is **false** — `ppvTestKeyIn`
(`prettyTest.c:3471`) drives `runFunction(ITM_VISUAL)` → `ITM_alpha` →
letters → `ITM_ENTER`, and V36 uses it. What no pin does is call
`refreshScreen` afterwards; the hole is one step narrower than reported. And
the claim that `screen.c:6214` clears `MANUAL_MENU` again is false —
`SHOWMODE` includes `TI_SHOWNOTHING`, so that clear never runs. Moot:
`popSoftmenu` already cleared the bit and the early return skips the paint
either way.

**Consequence.** CERTAIN from the measurement: after a VISUAL that lands in the
Z/T window, no menu, status-bar or shift-annunciator repaint runs until
something else clears `SCRUPD_MANUAL_MENU`. INFERRED, not observed on a
simulator: the softkey row keeps showing `MNU_TAMLBLONLY`'s label-entry keys
after that menu has been popped, and keeps showing them past the EXIT that
dismisses the drawing.

**Violated.** `design-docs/pretty-print/DESIGN.md:689-691`: *"Only the stack
refresh is suspended (`SCRUPD_MANUAL_STACK`), so the menu and status bar keep
working and X keeps its value"* — restated verbatim in the code on the two
lines immediately above the assignment (`prettyVisual.c:1603-1604`).

**Bug class.** A partial-suspension contract implemented with a flag pair the
consumer treats as total. The design named one bit; the consumer tests
`!= SCRUPD_AUTO`.

**Class-level test.** For every surface that arms the manual-paint protocol
(`PSHOW`, `PHIST`/browser, `EQSHW`, `VISUAL`), drive it through its *real*
entry path and then call `refreshScreen`, asserting which of the three bands
repaint: stack suspended, softkey row and status bar painted. That pin is a
band-ink assertion, and `AUDITPROBE3` shows it goes red today.

---

### PP18RR3-3 — the browser's pan hands `showGlyphCode` a wrapped-around negative x that nothing clips, in the one function that clips its own rectangle for that exact reason

`packages/pretty-print/prettyLayout.c:561` (`px = (uint32_t)(int32_t)x`, fed
straight into `showGlyphCode`), against `packages/pretty-print/screen.c:1271`.

**What breaks.** `ppShowRun` clips its **rectangle** — its first statement is
the `AUDIT R3-11`-clipped `ppFillVal` — and then hands its **glyphs** an
unclipped `px`. `screen.c:1226` recovers a wrapped `y` (`int32_t yy =
(int32_t)y;`) and :1272-1273 clamp `y1`/`y2` against `SCREEN_HEIGHT`, but
:1271 is `uint32_t x1 = x + ((((doubling*(xGlyph+col)))>>miniC)>>3);` with no
x recovery and no clamp, passed straight to `setPixel`. `_doShowString` clips
nothing in x either.

**Reaching input.** A history row wider than the visible band, then PHIST,
select it, press `.d` once. `prettyBrowser.c:152` does `pbPan += 60`; :113
computes `x = 8 - pbPan`; :116 calls `ppPaintAt(root, x, …)`. Every PP_RUN at
`relX < pbPan-8` reaches `ppShowRun` with a negative absolute x, and
`(uint32_t)(int32_t)x` is a value near 2^32 that only wraps back into range
once `colOffset` reaches `pbPan-8`. The clamp at :107-111 bounds the pan at
the right edge; it does not bound x below.

**Evidence (measured, on the package's own T29 pan fixture).** Counters in
both HAL `bitblt24` implementations, a negative-`px` counter inside
`ppShowRun`'s loop, and a print bracketing T29's existing `prettyBrowserPan()`:

```
AUDITPROBE R3: unpanned-oob=56 huge=0 | panned-oob=39 huge=39 minhuge=4294967279(=-17) negGlyphs=3
```

One pan plus repaint added 39 `bitblt24` calls the HAL rejected, **all** of
them with a wrapped-negative x, from 3 glyphs painted at negative `px`. Before
the pan, zero wrapped-negative calls had occurred in the entire suite, so this
is the pan's own contribution.

**Why the gate is green, and why the device is not protected.** Both simulator
HALs early-return on `x >= SCREEN_WIDTH || x + dx > SCREEN_WIDTH`
(`src/testSuite/hal/lcd.c:26`, `src/c47-gtk/hal/lcd.c:124`) — and the GTK one
`printf`s a diagnostic, i.e. upstream itself classifies an out-of-range x as a
defect. On device, `setBlackPixel` is a `static inline` wrapper living
*outside* the `#if defined(DMCP_BUILD)` block, resolving to the DMCP ROM
`bitblt24` (`dep/DMCP5_SDK/dmcp/dmcp.h:87`) with no bounds contract, and
`find packages -path "*hal*" -name "lcd*"` returns nothing — no package-side
guard exists on device.

**Severity correction against the finder, from the refutation pass.** "Far
outside the frame buffer" and "reboot" are **overstated**. Both HALs implement
the mirror `x' = SCREEN_WIDTH - dx - x` in `uint32`, which wraps a negative x
back to just past the *right* edge: T29's `x = -17` gives `x' = 416`, byte
index 52 against 50 data bytes per line — two bytes into the next line's
prefix region; a 724-px row panned to `x = -328` gives byte 90, under two
lines further down. The browser paints rows 25..163 of a 240-line buffer, so
these land **inside** the buffer. The realistic device symptom is a corrupted
display and scrambled LCD line-prefix bytes, not a wild write. Severity is
therefore display corruption, not crash-or-data-loss — and the finding should
still be acted on, because it is invisible to every test the project can run.

**Violated.** The same function's own two siblings.
`prettyLayout.c:494-501` (`AUDIT R3-11`): *"The browser's pan paints from a
NEGATIVE origin, and a negative x cast to uint32_t becomes a huge value …
Clip here, once, rather than at nineteen call sites."* `prettyLayout.c:581-585`
(`AUDIT R3-12`): *"setBlackPixel is a thin bitblt24 wrapper with no bounds
check of its own — a panned or oversized stroke wrote past the frame buffer.
Both ends, both axes."* The glyph path's stated justification
(`prettyLayout.c:552-554`) is only half true: *"showGlyphCode recovers a
wrapped y at screen.c:1226, and column positions wrap back into range
unsigned"* — there is no x-side counterpart to the y recovery, and columns
wrap back into range only at and to the right of the pan origin.

**The written record is narrower than the comment, and that is how the gap
became invisible.** `design-docs/pretty-print/DESIGN-HISTORY.md:1250-1255`
says the same thing *with a qualifier*: *"… verified against both simulator
HALs, which reject an out-of-range x in bitblt24 and never index the buffer
with it."* Simulator-only, by its own words. The source comment dropped the
qualifier.

**Bug class.** A class fixed at the sites where it was noticed
(rectangle, line) and not at the third member (glyph), with the safety
argument for the third generalised past the evidence that supported it.

**Class-level test.** A HAL-level assertion, not a pixel probe: build the
testSuite with `bitblt24` failing the run on any call whose x is out of range
rather than early-returning, and drive every panning and manual-paint surface.
That converts the simulator's silent rejection — the thing that hides the
defect — into the oracle.

---

### PP18RR3-4 — in MVAR mode the construct hook swallows the whole `NAME(...)` span, so a free variable used only inside a construct body is never enumerated or allocated: the equation draws and cannot be computed

`packages/pretty-print/solver/equation.c:1985-2003`, against
`equation.c:1093-1133`.

**What breaks.** `ppEqBigopIntercept` fires for every `parseMode`
(`equation.c:1371`). When `parseMode != EQUATION_PARSER_XEQ` it walks to the
matching `')'` and returns the length of the **whole span**, so `strPtr` jumps
past `SUM(A×X;X;1;3)` in one step. `_parseWord`'s MVAR arm — the only place a
name is added to `mvarBuffer` and the only equation-side caller of
`findOrAllocateNamedVariable` — never sees `A`. The MVAR menu comes back with
zero variables (CALC still appears, because upstream's pad-to-4 loop at
:1658-1666 appends it regardless). Pressing CALC runs the XEQ parse, which
slices `A×X` into a hidden slot, evaluates it, and does
`reallyRunFunction(ITM_RCL, findNamedVariable("A"))` on a variable the MVAR
pass never allocated.

**Reaching input.** Store `SUM(A×X;X;1;3)` — typeable one softkey at a time
exactly as `EQ29` types `SUM(X;X;1;3)`, with `A` from the ALPHA menu. ENTER
commits and runs `parseEquation(currentFormula, EQUATION_PARSER_MVAR, …)`
(`keyboard.c:3545`).

**Evidence (measured).** A probe beside EQ29:

```
AUDIT-PROBE R3: A before = 2199 (INVALID=2199)
AUDIT-PROBE R3: MVAR err=0 vars: (end)      <- ZERO variables enumerated, no error
AUDIT-PROBE R3: A after MVAR = 2199         <- still unallocated
AUDIT-PROBE R3: fnEqCalc err=8 type=1       <- ERROR_OUT_OF_RANGE
```

Correction against the finder: the failure code is `ERROR_OUT_OF_RANGE` (8),
not `ERROR_UNDEF_SOURCE_VAR` — `findNamedVariable` returns
`INVALID_VARIABLE` = 2199, which falls into `regInRange`'s generic branch. The
user-visible outcome is as reported: the equation draws, the MVAR menu has no
`A`, CALC errors.

**This finding survived a partial refutation, and what survived is narrower
than what was filed.** A second verifier refuted the finding's *stated
contract*: `DESIGN.md:478-481` ("In MVAR mode it consumes ONLY the name and
`(`") is **stale prose that the same section supersedes thirty lines
earlier**. `DESIGN.md:445-451` records the current behaviour, its
justification and its pin: *"EQ29 types `SUM(X;X;1;3)` one softkey at a time,
runs the commit ENTER runs … MUT-56 (the Bug 1 shape) turns it red with syntax
error 45, which is also proof that Bug 1 would have made these constructs
impossible to SAVE from the keyboard."* `TESTING.md:246` carries MUT-56;
`DESIGN-HISTORY.md:635-638` carries the 2026-08-26 ruling. **The whole-span
consumption must not be reverted** — under the name-only behaviour the
finding's own reaching input cannot be stored at all, because the first
top-level `;` is a hard error in the base grammar and ENTER bounces the user
back into the editor.

What is left after that refutation is the finding, and it is unruled: the
ruling's stated rationale, *"Construct-internal variables are not enumerated;
the constructs bind their own"* (quoted verbatim at `equation.c:1988-1989`),
is true of the **loop** variable and false of a **free** variable in the body.
Nothing binds `A`. `ppEqEvalSlice`/`ppEqEvalSlot` parse slices under
`EQUATION_PARSER_XEQ` only, so no on-demand allocation exists either. So the
remedy is not "revert" but "enumerate the argument slices in MVAR mode without
letting `;` reach the base grammar" — and DESIGN.md must be made to say one
thing about MVAR mode rather than two.

**Violated.** The BINDING spelling rule, `DESIGN.md:460`: *"a spelling one
accepts and the other declines gives the user a number with no picture, or a
picture that will not compute"* — here between the MVAR acceptor and the XEQ
acceptor rather than between the renderer and the evaluator, which is the same
harm through a different pair. And `DESIGN.md:478-481`, which still states the
opposite of the shipped code inside its own section.

**Calibration.** "Stuck state" is slightly strong: the owner can still allocate
and set `A` through the ordinary `STO`-with-ALPHA-name path. What is genuinely
unavailable is supplying the parameter from the equation's own MVAR surface.

**Bug class.** A fix that closed one parse-mode failure by deleting the pass's
visibility into a region, with the region's other contents unaccounted for —
and a design document left holding both the old rule and the new one.

**Class-level test.** For a table of construct strings each containing a free
body variable, assert three things after the commit-ENTER path: the MVAR parse
raises no error, the free variable **is** in `mvarBuffer`, and `fnEqCalc`
returns 0 with the expected value. MUT-56 stays as it is — the pin for the
opposite direction — and the new pin is its complement.

---

### PP18RR3-5 — `ppqShowRender`'s linear line of last resort measures the string and then paints it unclipped, so EQSHW silently truncates any equation wider than the screen: no ellipsis, no marker

`packages/pretty-print/prettyEquation.c:942-946`.

**What breaks.** The fallback is
`int16_t w = stringWidth(src, &standardFont, false, true);
int16_t x = (w < SCREEN_WIDTH - 4) ? centered : 2;
showString(src, &standardFont, x, 94 - 8, vmNormal, false, true);` —
`w` is computed and then used only to pick `x`. `showString` passes `NO_LF`
(`false`), so the auto-LF arm never runs and x simply grows past 400;
`showGlyphCode`'s pre-clear `lcd_fill_rect` bails and every ink pixel goes to
`bitblt24`, which returns silently once `x >= SCREEN_WIDTH`. Nothing repaints
afterwards: the band is already cleared and the manual-paint flags are armed.

**Reaching input.** Any equation the 2D grammar declines that is wider than the
screen — and the cheapest instance is an equation with no `/`, `√` or `^` at
all, which `prettyEquation.c:803` refuses **by construction**
(`if(!c.fracSeen) return false; // nothing 2D gained`). Example:
`AREA=LENGTH+WIDTH+CORRECTION+OFFSET+FACTOR+MARGIN`. `fnPrettyEqShow:970`
hands `ppqShowRender` the **stored** text, deliberately (:963-969), so no
ellipsis has been applied. The same fallback is reached by an equation that
parses and then fails the width gate at :935.

**Evidence.** Replaying `_calculateStringWidth` against the shipped
`standardFont` metrics, that string measures **490 px**, not the ~430 the
finder estimated. Painted from `x = 2`, the last glyph starting on screen is
index 39: the owner sees `AREA=LENGTH+WIDTH+CORRECTION+OFFSET+FAC` and the
rest is dropped column by column with nothing marking the cut. (Static
derivation; no probe was run for this one.)

**The history makes it stronger than filed.** At PP7 the fallback string came
from `showEquation`'s dry run (`git show a85e60acb:packages/pretty-print/
prettyEquation.c`), which is capped at `SCREEN_WIDTH-2-X_OFF` and ends in
`STD_ELLIPSIS` — it always fit and always carried a marker. Commit
`e29e7a019` switched the input to the stored text specifically to *escape*
that ellipsis, and the unguarded `showString` kept its shape. No ruling covers
the post-change state, and the `DESIGN-HISTORY.md:686-694` paragraph that
records the switch is now stale.

**Violated.** The package's own `AUDIT R2-2` ruling, in the sibling file at
`prettyFormula.c:684-691`: *"an over-wide row there would be painted CLIPPED —
showing `12345678 + 98` for `12345678 + 98765432`, which is a lie by
truncation, and worse than the honest omission it replaced."* The pager
refuses an over-wide row on that ruling; this fallback computes the identical
width and paints anyway. The surface EQSHW exists to improve on is more honest
than EQSHW: `solver/equation.c:640-650` packs to
`(strWidth + glyphWidth) <= (SCREEN_WIDTH - 2 - X_OFF)`, writes
`STD_ELLIPSIS` and sets `*rightEllipsis`. Upstream's convention at the same
shape is to width-test before `showString` (`screen.c:3784`).

**Note for whoever fixes it.** R2-2's remedy — omit the row — does **not**
transfer. EQSHW's always-show-something fallback exists to avoid a blank band
and is pinned by EQ9 (`prettyTest.c:2700-2716`). The honest fix is a marker,
the way `equation.c:645-649` does it, or `_showStringWithLimit`.

**Bug class.** A width measured and then discarded; a "show something" fallback
that shows something untrue.

**Class-level test.** For each surface that paints a string it did not
width-test, assert that the painted string's `stringWidth` is `<=` the band,
or that its last glyph is the ellipsis. Drive it from a table of stored
equations spanning the decline reasons (no fraction, unparseable glyph,
over-wide 2D tree).

---

### PP18RR3-6 — `ppqNumber` rejects the comma radix mark its own comment says it accepts, so `1,5` — which the evaluator reads as 1.5 — declines the whole strip

`packages/pretty-print/prettyEquation.c:79`.

**What breaks.** `ppqNumber`'s loop accepts only
`(code >= '0' && code <= '9') || code == '.'`. The equation parser accepts a
comma as a radix mark and rewrites it: `solver/equation.c:1599` counts `','`
toward `numericCount`, so `PARSER_HINT_REGULAR` types `"1,5"` as NUMERIC, and
:1225-1233 rewrites every `','` to `'.'` before `stringToReal34`. `1,5` **is**
1.5. `showEquation` copies the comma verbatim into the display string
(:450, and the Numbers arm at :582 only NUL-terminates), and that string is
what reaches `prettyTryEquation` at :692. `ppqNumber` stops after the `1`,
`ppqTerm`/`ppqExpr` break on the comma, and `ppqParse` fails the
full-consumption test at :800.

**Reaching input, and it is not exotic.** `keyboard.c:1665-1671` rewrites the
radix key to `ITM_COMMA` in `CM_EIM` whenever the owner's radix mark is a
comma (`RADIX34_MARK_DEC_ITM`). For a comma-radix owner, `1,5` is the ordinary
way to type a decimal into an equation — so **every equation containing a
decimal number loses its 2D form**, silently, for that whole population. The
punctuation softmenu route (`ITM_COMMA` in `menu_alphaMisc`) is the minor of
the two. `EIM_DISABLED` on `ITM_COMMA` is not a gate: `bufferize.c:509` uses
`EIM_STATUS` only to decide whether to append `"()"` to a function name, and
`ITM_PERIOD` carries the same flag.

**Evidence (measured).** A probe beside EQ4 asserting both forms parse:

```
prettyPrint test FAIL: AUDIT-PROBE R3 comma form DECLINED
```

with **no** failure for the dot control, in the same run — one test failed in
the whole battery. The radix mark is the sole cause.

**Violated.** `prettyEquation.c:72`, `ppqNumber`'s own banner: *"number:
digits/'.'/group separators"* — the code accepts no separator of any kind. And
the finding of fact `DESIGN.md`'s PP14 section quotes as evidence: *"the parser
rewrites every comma inside a number to `.`, so `1,5` IS the number 1.5"* (the
rewrite is at `solver/equation.c:1229` in this tree). The acceptor and the
evaluator disagree about what a numeral is.

**Scope correction against the finder.** The divergence is not confined to the
strip: `fnPrettyEqShow` feeds the **stored** text to the same `ppqParse`, so
EQSHW declines a comma equation identically.

**Severity.** Latent, deliberately: the failure mode is the designed safe
fallback to the linear line, not a wrong picture. It is ranked here rather than
lower because of who it hits and how often — a configuration setting, not an
unusual input.

**Bug class.** Acceptor alphabet narrower than the producer's emit-set, with
the producer being the package's own evaluator. Same class as `PP18RR2-1`, one
seam over.

**Class-level test.** Drive the parity table (`PP18RR3-D1`) once per radix
setting. Any acceptor whose verdict changes with `RADIX34_MARK_CHAR` for the
same mathematics is a defect by definition.

---

### PP18RR3-7 — `ppqFrameIntegral`'s 48-byte limit buffers are smaller than `real34ToDisplayString`'s worst-case output, and the width retry that would shrink it runs only after the write

`packages/pretty-print/prettyEquation.c:858` (`char dv[20], lo[48], hi[48],
dtext[24];`), and the same shape at `packages/pretty-print/prettyFormula.c:369`
(`char sb[48]`).

**What breaks.** `real34ToDisplayString` takes no size argument, and its
width-shrink loop is a `do { … } while(stringWidth(displayString, …) >
maxWidth)` — the string is written **before** it is measured, so the retry can
only follow an oversized write, never prevent it. The two calls at :860-863
run unconditionally once the status/`dtReal34` gate opens, before any
`ppNewBox`, so no allocation failure can steer around them.

**Reaching input.** Config menu → the HP-35 preset, which sets `SDIGS 16`,
`DSTACK 1`, `RNG 99`, `InputDefaultDataType ID_DP` — **exactly the conjunct
set of `checkHP`** (`defines.h:2375`) in one menu action. Then `FIX 19`
(`DSP_MAX` is 19 and `fnDisplayFormatFix` has no HP-mode cap). Then the
equation integrate solver on any stored equation with `LLIM 0`,
`ULIM 1E10`. Then EQSHW. The `ID_DP` that `checkHP` itself demands is also
what guarantees the limits arrive as `dtReal34`, so `ppqFrameIntegral`'s own
gate opens.

**Why the FIX arm is taken.** `display.c:964`'s SCI escape is
`exponent >= (checkHP ? 10+1 : displayHasNDigits)`; with `checkHP` the escape
is 11 and the exponent is 10, so the `displayHasNDigits = 6` the package
passes is **inert**. `fixLoopEnd = firstDigit + exponent +
displayFormatDigits_Active` emits 11 integer + 19 fraction digits; separators
are `xcopy(…, SEPARATOR_(digitCount), 2)` — a hardcoded 2 bytes, and
`SEPARATOR_LEFT/RIGHT` resolve to `STD_SPACE_PUNCTUATION`, 2 bytes each — nine
of them at the default grouping, plus the radix mark.

**Evidence (measured).**

```
AUDIT-PROBE R3: checkHP=1 gwl=3 gwr=3 seplen=2
AUDIT-PROBE R3: firstWriteLen=49 bytesNeeded=50 text=[10 000 000 000.000 000 000 000 000 000 0]
AUDIT-PROBE R3: canaryBytesClobberedPast48=2
```

The third line used the package's exact call shape, `maxWidth 110` included,
writing into `struct { char buf[48]; char canary[192]; }` pre-filled with
`0x7e`. Two bytes past the 48-byte field were overwritten **despite** the
width limit — which is the `do/while` ordering, demonstrated.

**Severity correction against the finder.** The overrun is 2 bytes, not an
arbitrary-length smash. The realistic damage is clobbering the neighbouring
local (`lo[48]`, `dv[20]` or `dtext[24]` in the same frame) or tripping a
stack-protector abort — not free-form corruption.

**Violated.** `prettyFormula.c:333-334`, the package's own sizing rule for this
family of buffers: *"sized so neither overflow NOR truncation is possible"*.
That reasoning was applied to `lbl`/`text`/`dv`, whose producers have hard
length caps (`snprintf`, 17-byte variable names), and skipped for the two
whose producer is upstream's unbounded formatter. `DESIGN.md`'s fallback rule
("Any failure … paints nothing and returns false") cannot cover a write that
has already happened.

**Exposure this audit could not bound.** `GROUPWIDTH_LEFT`/`RIGHT` are user
settings held in `uint8_t`. If either can be set below 3, the separator count
grows and the margin shrinks further. Nobody traced the config menu's accepted
range; the measurement above is at the **default** grouping of 3, and does not
depend on that question.

**Bug class.** A fixed stack buffer sized against an assumed producer rather
than the producer's contract, where the producer's contract is "no bound".

**Class-level test.** A worst-case sweep: for every package call to
`real34ToDisplayString`/`real34ToDisplayString2`, drive the formatter across
the display-mode × `checkHP` × `displayFormatDigits` × exponent grid with a
canary past the destination and assert no byte past the declared size is
touched. It is a table over four settings, and it retires the "sized so
neither overflow nor truncation is possible" comment as the authority.

---

### PP18RR3-8 — `ppqParse`'s recursive descent has no depth cap; nesting depth is bounded only by the length of the equation text, at a measured 264 bytes of ARM stack per level

`packages/pretty-print/prettyEquation.c:534` (and :551, :589).

**What breaks.** `ppqCtx_t` is `{const char *s; int16_t pos, len; bool_t
fracSeen; bool_t failed;}` — no depth field, no depth parameter on
`ppqExpr`/`ppqTerm`/`ppqFactor`/`ppqPrimary`. At :531-546 the `'('` arm sets
`c->pos = next` and calls `ppqExpr` **before** allocating anything; the
`PP_PAREN` box is not created until :541, after the recursive call returns. So
neither the 72-node pool nor the 512-byte text pool bounds the descent. Same
shape on the radical arm (:551) and the `'^'` arm (:589). Malformed input
recurses fully too: `ppqPeek` returns 0 past the end and `ppqPrimary` sets
`c->failed` at the **bottom**. `ppMeasure`'s `PP_MAX_DEPTH` runs only after
`ppqParse` returns, so it cannot bound anything.

**Reaching input.** In the equation editor, type `(` a few hundred times
(`AIM_BUFFER_LENGTH` 1024, `MAX_NUMBER_OF_GLYPHS_IN_STRING` 508), ENTER to
store, then EQSHW. `fnPrettyEqShow:974` hands `ppqShowRender` the stored text
deliberately.

**The refutation that mattered, and failed.** "The input cannot be stored."
`keyboard.c:3541-3543` (CM_EIM ENTER) is `setEquation(currentFormula,
aimBuffer); parseEquation(currentFormula, EQUATION_PARSER_MVAR, …);`.
`setEquation` has no length or syntax check. In `_parseWord` the MVAR body is
wholly inside `if(parserHint == PARSER_HINT_VARIABLE)`; the parenthesis path
calls `_parseWord` with `PARSER_HINT_OPERATOR`, which in MVAR mode does
nothing; and `_processOperator` — the only raiser of
`ERROR_EQUATION_TOO_COMPLEX` on operator-stack overflow — runs under
`EQUATION_PARSER_XEQ` only. So `((((…X…))))` at 300 deep **stores cleanly**,
exits EIM with no error, and sits in `allFormulae` waiting for EQSHW.

**Evidence.** `arm-none-eabi-objdump` on the shipped
`build.dmcp5/…/prettyEquation.c.o`: all four are real, non-inlined symbols —
`ppqPrimary` pushes 9 registers and subtracts 92 (128 B), `ppqFactor` and
`ppqTerm` push 12 each (48 B), `ppqExpr` pushes 10 (40 B). **264 bytes per
parenthesis level**, 128 per radical, 176 per `'^'`.

**And the stack is not big enough.** Upstream's own field note,
`src/c47/memory.c:225-229`: *"How far down to mark cannot be derived, since
neither DMCP nor either linker script says how far the stack extends… Too far
writes outside the stack and the calculator faults on the next reset. The
largest depth confirmed as actually written is 16384, on the DM42n… a DM42n
run asking for 55000 crashed on the next reset."* At 264 B/level a sub-55 KB
program stack is exhausted somewhere around 76–200 nested parens — an order of
magnitude inside the storable range.

**Residual doubt, stated rather than papered over.** The exact fault threshold
is unnamed: the 55000 datapoint is a marker write, not a call-frame chain, and
the DMCP program stack size is not in `dep/DMCP5_SDK` or
`src/c47-dmcp5/stm32_program.ld`. What is certain is that the consumption is
unbounded in the input.

**Violated.** Every other recursion in this package is capped, and the design
says why: `PP_MAX_DEPTH` 12 in `ppMeasure` (*"the depth cap only stops runaway
recursion"*), `PPV_MAX_DEPTH` 5 in the walker, and for the evaluator's own
construct nesting *"the device stack is a scarce OS-provided resource … A
breach refuses the construct cleanly; it never dives on hope"* with
`PPEQ_STACK_ALLOWANCE` 8000 (`equation.c:1741-1754`). `ppqParse` is the one
acceptor of the same construct language with neither a depth counter nor a
stack probe, and it is the one fed directly from user-typed text.

**Extra, beyond the finding.** `fnPrettyEqShow` never consults
`prettyEnabled()`, so EQSHW recurses even with the pretty-print flag off. A
second acceptor, `prettyTryEquation`, is reached from `solver/equation.c:692`
on the strip's display string — shallower, because that string is truncated to
about a screen width, but likewise uncapped.

**Bug class.** Unbounded recursion over attacker-length input in the one
member of a family where every sibling is capped.

**Class-level test.** A depth-sweep pin: for each recursive acceptor
(`ppqParse`, the walker, `ppEqBigopIntercept`), feed nesting at
cap−1/cap/cap+1 and assert a clean decline at cap+1 rather than a completed
parse. Today `ppqParse` has no cap to sweep, which is the finding.

---

### PP18RR3-9 — the solo-build browser-containment census is two sites short; the complex-key case runs its item under the pretty browser and lands on the bug screen

`packages/pretty-print/keyboard.c:2589` (the `case ITM_op_j_pol/ITM_op_j/
ITM_CC:` calcMode list), and `:1604` (the shift-processing list).

**What breaks, structurally.** Brace-depth analysis of the `switch(item)`
starting at keyboard.c:2462 puts `case ITM_CC:` at **depth 1** while the
`CM_PRETTY_BROWSER` containment guard sits at **depth 3**, inside that same
switch's `default:` arm. The guard cannot fire for any item that owns a `case`
label. The case's own list at :2589 covers `CM_REGISTER_BROWSER`,
`CM_FLAG_BROWSER`, `CM_ASN_BROWSER`, `CM_FONT_BROWSER` and `CM_TIMER` — and
not 20. So `keyActionProcessed` stays false, `btnPressed` sets
`showFunctionNameItem`, and `btnReleased` reaches `runFunction(item)`.

**Reaching input.** `FLAG_USER` on (`determineItem` selects `kbd_usr`
regardless of calcMode, :1552), assign `ITM_CC` to a key from the catalog
(`CAT_FNCT`, assignable by name), then PSHOW or PHIST to enter
`CM_PRETTY_BROWSER` and press that key. `prettyBrowser()` never clears
`FLAG_USER`. `runFunction(ITM_CC)` calls `fnKeyCC`, whose `switch(calcMode)`
has no arm for 20 and falls to
`default: displayBugScreen(bugMsgCalcModeWhileProcKey)`, which sets
`calcMode = CM_BUG_ON_SCREEN`, overwrites `previousCalcMode` and paints over
the browser.

**Correction against the finder, and it matters for the write-up.** The
finding's chosen example, `ITM_op_j`, does **not** map to `fnKeyCC`. Item 1830
is `fn_cnst_op_j`, which forwards to `fnKeyCC` only in `CM_NIM`/`CM_MIM` and
otherwise runs `cpxToStk(const_0, const_1, …)` — silently lifting the stack
and pushing `i` underneath the browser. That is still the class the package's
own comment names ("ran its item UNDERNEATH the browser"), but it is not the
bug screen. The bug screen is reached through `ITM_CC`, which the finding also
enumerates. Note also that `KEY_COMPLEX` (1848) is **not** `ITM_CC` (1730), so
the stock f-shifted COMPLEX key lands in `default:` and *is* contained.

**Routes checked and closed** (so the finding is not broader than it is): the
Σ+ / `Norm_Key_00` override is gated to `CM_NORMAL`/NIM/PEM/TIMER/ASSIGN; the
longpress f-shift injection is gated to `CM_NORMAL`/NIM; the softkey path is
already contained by `calcMode < 19` at `keyboard.c:690`, `:832` and `:940`.
The open route is USER-mode assignment.

**Why the gate never sees it.** Both sites are covered in the **combined**
build by undo-history's override: `packages/undo-history/patches/
010-keyboard.c.patch` adds `(calcMode >= 19 && calcMode <= 23)` to the shift
list at patch line 37 and to the exact `else if(calcMode ==
CM_REGISTER_BROWSER || …)` line that is pretty-print's :2589 at hunk
`@@ -2571`. The sibling patches upstream lines on pretty-print's behalf.
`packages/pretty-print/build-test.sh --solo` is a maintained pass, and in it
these two gates are absent.

**Violated.** The package's own two comments, which establish solo correctness
as a maintained property and name this exact class. `keyboard.c:1690`: *"Both
gates that admitted calcMode 20 lived in sibling packages … so a solo
pretty-print build fell straight through to the bug screen on every key inside
PHIST."* `keyboard.c:2807` (`AUDIT R3-7`): *"the switch below decides whether
a resolved item may EXECUTE, and with no arm for the browser every key that is
not one of its own ran its item UNDERNEATH the browser in a solo build."* The
`R3-10` note enumerates the exceptions by example — *"ENTER, EXIT and
BACKSPACE have their own `case ITM_…` blocks upstream of it"* — and misses
`ITM_CC`/`ITM_op_j`/`ITM_op_j_pol`.

**Bug class.** Guard enumerates the examples, not the class — the shape this
stage has already paid for once (`PP18RR1`-era, and named in the memory index
as "guard-enumerates-examples-not-class").

**Class-level test.** Mechanical, and it is the only honest form: enumerate
every `case ITM_…` block in `processKeyAction` that carries a calcMode list,
and assert each list either includes the package-browser range or that the
item's handler has an arm for it. That is a source-shape check, not a runtime
pin — and it is exactly the enumeration the two comments substitute prose for.
Whatever the fix, the solo build needs a gate run of its own, since today the
property is asserted in comments and measured nowhere.

---

### PP18RR3-10 — `ppqFrameDerivative`/`ppqFrameIntegral` return the UNFRAMED equation when the node pool is spent, so EQSHW in a derivative or integrate session paints the plain expression and reports success

`packages/pretty-print/prettyEquation.c:906` (the derivative arm's
`return eq;`), and `:887` / `:874-886` (the integral arm's two).

**What breaks.** Both frame builders bind five fresh nodes and check them
together; when the check fails they hand back their own argument. `ppMeasure`
then succeeds on the unframed tree, the fit test passes, and `:934` paints it
with `pretty = true`. The refusal is answered with a different, well-formed
picture instead of a decline.

**Reaching input.** An equation dense enough that `ppqParse` uses 70 of the 72
layout nodes but still measures and fits, then a 1st-derivative session
(EQN → `f'(x)`, `-MNU_1STDERIV`) and EQSHW.

**Evidence (measured, both a corrected and the original input).**

```
Run A  ((1/A+1/B)+(1/C+1/D))+((1/E+1/F)+(1/G+1/H))+((1/I+1/J)+(1/K+1/L))+M
       parse=1 nodesUsed=70 pool=72
       frameChanged=0 meas=1 w=362 h=32 wcap=396 hcap=147
       showrender prettyDeriv=1 prettyPlain=1 pixelsIdentical=1

Run B  ((1/X+1/X)+(1/X+1/X))+((1/X+1/X)+(1/X+1/X))+((1/X+1/X)+(1/X+1/X))+X
       parse=1 nodesUsed=70 pool=72
       frameChanged=0 meas=1 w=364 h=29 wcap=396 hcap=147
       showrender prettyDeriv=1 prettyPlain=1 pixelsIdentical=1
```

`frameChanged=0` is `ppqFrameDerivative` returning its own argument;
`prettyDeriv=1` is the false success; **`pixelsIdentical=1` is the
consequence measured directly** — the derivative-mode EQSHW screen is
bit-for-bit the non-solver screen.

**Correction against the finder.** The literal reaching input carries 13
distinct variables and upstream's softmenu path rejects it with
`ERROR_EQUATION_TOO_COMPLEX` at `numberOfVars > 12` before a session can open.
The correction does not save the finding: the single-variable rewrite (Run B)
gives the identical 70-node count and matching measurements, and
single-variable is precisely the case the derivative session requires in order
to run `fn1stDerivEq` at all — so the corrected input is strictly **more**
reachable than the one reported.

**Consequence.** The owner presses EQSHW in a derivative session and gets a
picture of the plain equation with no `d/dx` prefix and no parentheses round
the body — visually identical to the non-solver view, so nothing on screen
says which mode the session is in. No error, no decline, no linear fallback.
The integral arm loses the `∫` entirely (:887) or its live LLIM/ULIM limits
(:874) by the same mechanism.

**Violated.** `DESIGN.md`'s "Solver-surface frames (PP13)": *"The derivative
modes prefix `d/d<var>` (first) or `d²/d<var>²` (second) with the equation in
tall parens"* — stated unconditionally, and the only documented fallback is
the integral's, *"without `SOLVER_STATUS_INTERACTIVE` (or non-real limit
registers) it falls back to PP7's bare stroke `∫`"*, which is not the
condition that fires here. Also this file's own rule at
`prettyEquation.c:192-197`: *"Every allocation is bound and checked BEFORE any
append … an unchecked append renders a formula with an operand missing and
reports success (audit R4-3)."* These two functions check, and then answer the
refusal with a lie.

**Scope note.** The derivative arm at :906 was executed. The integral arm's
:887 and :874-886 were traced, not executed.

**One earlier clearing is overturned by this measurement.** The guards
dimension cleared exactly this shape as UNREACHED, reasoning that any
equation dense enough to exhaust the pool during framing would be rejected by
the width/height gate first. Runs A and B show both caps pass with margin
(364 px against 396, 29 px against 147). Recorded here because "I could not
construct it" and "it cannot happen" are different claims, and this round
produced an instance of the difference.

**Bug class.** Failure answered with a substitute rather than a decline —
`bool_t` try-function protocol violated by returning `true` with different
content.

**Class-level test.** Pool-starvation sweep, the `FV18` idiom generalised: for
every builder that composes on top of a completed tree (both frame builders,
`ppfCombine1/2`, `ppqBuildBigop`, `ppqBuildCall`), pre-allocate the pool down
to `needed-1` and assert the caller reports **false** and paints nothing.
`FV18` already does this for one site; the sweep is the same code over a list.

---

### PP18RR3-11 — `PPQ_IS_SUP` admits `STD_SUP_MINUS` but not `STD_SUP_PLUS`, so an equation holding a `+`-signed exponent loses its whole 2D strip

`packages/pretty-print/prettyEquation.c:35`.

**What breaks.** `PPQ_IS_SUP` is
`(code >= 0xa160 && code <= 0xa169) || code == 0xa16b`. `0xa16b` is
`STD_SUP_MINUS`; `0xa16a` is `STD_SUP_PLUS` (`fonts.h:530-531`). The builder
emits both: `_checkExponent` (`solver/equation.c:328-364`) has an explicit
`case '+'` that counts the sign, and `_showExponent` (:308-311) writes
`STD_SUP_PLUS`. `ppqNumber`'s exponent tail (:90), `ppqFactor`'s verbatim
sup-run (:609), `ppqTerm` and `ppqExpr` all decline `0xa16a`, so `ppqParse`
fails full consumption at :800 and the **whole equation** falls to upstream's
linear line.

**Reaching input.** Store `Y=1E+5/X` — the equation parser accepts the signed
exponent at `solver/equation.c:1474` — and let the strip repaint with no
cursor. The differential is one glyph: `Y=1E-5/X` renders as a stacked 2D
fraction, `Y=1E+5/X` does not.

**Evidence (measured).** Two `ppqParse` calls beside the EQ4 declines, fed the
exact display strings and reporting only on accept:

```
prettyPrint test FAIL: AUDIT-PROBE R3 MINUS-exponent ACCEPTED
```

— one failure in the entire battery. The PLUS probe never printed, i.e.
`ppqParse` returned false for it. One glyph, opposite outcomes, in the built
artifact. Both `PRODUCT_SIGN` spellings (`STD_DOT` / `STD_CROSS`) are in
`PPQ_IS_PROD`, so the `FLAG_MULTx` setting does not change the outcome.

**Correction against the finder.** The EQSHW aside is loose. EQSHW reads the
**stored** text `1E+5/X`, where `E` is a plain letter; that declines one step
earlier at the `1`/`E` juxtaposition, and would decline for `1E-5/X` too.
EQSHW is a separate pre-existing limitation. The `showEquation` strip is where
this defect bites, and there the finding is exact.

**Violated.** `prettyEquation.c:72-73`, `ppqNumber`'s contract: *"number:
digits/'.'/group separators, optionally ·₁₀ + sup exponent — copied verbatim
(the glyphs already render right in a run)"*. The superscript sign is part of
the exponent `_showExponent` emits and it renders right in a run; the macro
admits one of the two sign glyphs the builder can produce. Nothing in
`DESIGN.md` §6 or the PP5 grammar note distinguishes a positive exponent from
a negative one.

**Bug class.** A character-class macro enumerating the members someone
remembered rather than the members the producer emits — the same shape as
`PP18RR3-6` and `PP18RR2-1`, three instances now.

**Class-level test.** As `PP18RR3-6`: producer-fed, not hand-typed. Every
glyph `_showExponent`, `_showFraction` and the numbers arm of `showEquation`
can write must be accepted by the acceptor that reads their output, asserted
by enumerating the producer's own emit sites.

---

### PP18RR3-12 — the DERIV order guard tests a `uint16_t` truncation of an int32 result, so an order congruent to 1 or 2 mod 65536 passes it and silently computes a derivative

`packages/pretty-print/solver/equation.c:2090`.

**PRIOR ART — this is a re-report, not a discovery.** It is verbatim finding
**A13** of the 2026-08-27 PP1-PP16 audit ("DERIV order is narrowed to
`uint16_t` before the 1-or-2 range gate"), which measured it live —
`DERIV(X^3;X;3;65538)` through `setEquation` + `fnEqCalc` returning `err=0`
with X exactly 18 — and whose own triage put it under *"what I would leave
alone"*: *"needs an order argument congruent to 1 or 2 mod 65536. Nobody types
65538. The class test is worth more than the fix."* It is filed here because it
is still open and unfixed, and because this round's independent search
established that **no ruling sanctions it**.

**What breaks.** `order = (uint16_t)real34ToInt32(&argS);` then
`if(order != 1 && order != 2) ppEqSyntaxError("DERIV order must be 1 or 2");`.
`real34ToInt32` is `decQuadToInt32` with `DEC_ROUND_DOWN`, so an in-int32 value
arrives intact and only the cast narrows: 65537 becomes 1, 131074 becomes 2.

**Reaching input.** `DERIV(X×X;X;3;65537)` evaluated with EQCALC. Meanwhile
`ppqParse` refuses to render it at all (`prettyEquation.c:411-421` accepts the
order only as the literal run `"1"` or `"2"`), so the picture disappears from
the strip while the number quietly changes.

**Intent search, all negative** — this is what the round adds to A13. Grep for
`65536`/`65537`/`truncat`/`(uint16_t)real34` over `design-docs/pretty-print/`
returns nothing on this cast. The PP14 RULED block (`DESIGN.md:366-521`) rules
on nesting depth, the separator choice, slice evaluation, loop binding,
delegation and rendering — never on argument **range**; `DESIGN.md:454` spells
the syntax with order in {1,2} only. `DESIGN-HISTORY.md` has no entry and no
deferral. The one commit body that discusses DERIV's order (`1fd492a48`) rules
on the **renderer** only: *"'Only a literal 1 or 2 renders' is a parse rule;
the parser now resolves it to a flag."* `prettyTest.c` has one DERIV-order pin
(EQ16, order 2) and no out-of-range sweep, so no test encodes the current
behaviour as intended either. And the one repo-wide reference to A13's
disposition, in the PP18 audit at line 634, cites a *different* A13 (a
forth-core register-tag `uint8_t` ruled lossless).

**Violated.** The guard's own error text, `equation.c:2092`: *"DERIV order must
be 1 or 2"*. It is the only thing between the user's argument and
`ppEqDelegate`'s kind/order dispatch, and the truncation defeats it.

**Bug class.** Silent narrowing before a range gate (the PP1-PP16 audit's
class `C20`).

**Class-level test.** The one A13 already asked for, unwritten a month later: a
sweep over every package-side cast that narrows before a validity test, driven
with values that alias into the accepted set. For this site: `order` in
{0, 1, 2, 3, 65536, 65537, 65538, 131074} with the expectation `error` for
everything but 1 and 2.

---

### PP18RR3-13 — FV9 and FV10 hang their only measure/paint assertions on an unchecked `if(ppMeasure(...))`, and for `PP_BARS` that guard is the sole coverage of the arm it guards

`packages/pretty-print/prettyTest.c:2183` (FV10), and `:2156` (FV9).

**What breaks.** FV10 builds `|5|`, asserts the signature `"A(5)"`, then
`if(ppMeasure(root10, 0)) { paint; assert left bar }` with **no `else`**. If
the measure arm the pin exists for fails, the guard is false, the paint and
the bar probe never run, and the driver writes 0 failures.

**Evidence — a three-run mutation ladder, which is what makes this
conclusive.**

| run | tree | result |
|---|---|---|
| 1 | unmutated `34ac6e97f` | `PRETTY-PRINT GATE GREEN` (201.53 s) |
| 2 | `prettyLayout.c:384` PP_BARS measure arm → `return false` unconditionally | `PRETTY-PRINT GATE GREEN` — mutation confirmed present in `build.sim/custom_pkg_shadow/prettyLayout.c` |
| 3 | run 2's mutation **plus** a temporary `else { ppTestFail("FV10 measure declined"); }` | gate **RED**, `Fail: 1`, `prettyPrint test FAIL: FV10 measure declined` |

Run 3 rules out the only alternative explanation — that `prettyTestFormula` is
vacuous or compiled out. The suite is live, executes FV10, and reports through
`REGISTER_X`; run 2's green is a coverage escape caused by the missing `else`
and nothing else.

**Coverage uniqueness, re-verified independently.** `PP_BARS` is produced at
exactly one site (`prettyFormula.c:218`, `ITM_ABS`/`ITM_MAGNITUDE` in
`ppfCombine1`) and consumed at exactly two (`prettyLayout.c:384` measure,
`:746` paint). Across the whole 5,229-line test file the only
`ITM_MAGNITUDE`/`ITM_ABS` drive is `prettyTest.c:2174` and the only `"A("`
expectation is `:2182` — both inside FV10. A `PP_BARS` measure regression has
no other pin anywhere in the package.

**Production reachability of the destroyed arm.** `ITM_ABS`/`ITM_MAGNITUDE`
reach `ppfCombine1`'s builder through the capture engine
(`prettyFormula.c:553`) and the visual walker (`prettyVisual.c:1082`) — the
same path FV10 drives with `ppcTestOp(ITM_MAGNITUDE)`. The owner's `|x|` would
silently stop measuring and painting its strokes with the package's only bars
pin still green.

**Violated.** The pins' own stated purpose. FV10's comment names the thing
checked ("absolute-value bars", then the left-bar ink probe); FV9's says
*"assert the SCRIPT node itself is lowered: relBase must be positive (the
root-descent version was satisfied by `log`'s own descender — MUT-25 stayed
green)"* — i.e. it was written once as a pin that could not fire, tightened,
and then left behind a guard that can disable it wholesale. And the same file
does it correctly twelve lines above: FV7 at `prettyTest.c:2106` is
`if(!ppMeasure(root7, 0)) ppTestFail("FV7 tall radical declined"); else {…}`.

**Bug class.** A pin switched off by exactly the regression it exists to catch.
Adjacent to the KNOWN `PP18RR2-16` ("B10's dispatch-depth pin … wrapped in a
label guard with no else"), and the class genuinely changes: there the guard
was a fixture load, here the guard **is** the production function under test.

**Weaker half, stated as such.** FV9's `PP_SUB` variant was not mutated. Its
escape is partly covered by EQ22 (`prettyTest.c:2944`), which measures a tree
containing a DERIV `PP_SUB` with an explicit `else if(!ppMeasure(…))
ppTestFail`. FV10/`PP_BARS` is the half that is fully proven.

**Class-level test.** Mechanical and cheap: a source-shape check that no
`if(ppMeasure(…))` or `if(ppfBuild…(…))` in `prettyTest.c` lacks an `else`
that fails. Fifteen call sites, one grep, and it retires the whole class.

---

### PP18RR3-14 — FV5 and FV6 are labelled as the PHIST pager's pins, but every `fnPrettyHist` call in the suite enters from `CM_NORMAL` and is diverted to the browser; the pager body has no coverage at all

`packages/pretty-print/prettyTest.c:2008` (FV5), `:2058` (FV6), and
`testSuite/tests/pretty_print.txt:46`.

**What breaks.** `fnPrettyHist`'s first arm is
`if(calcMode != CM_PRETTY_BROWSER) { prettyBrowser(NOPARAM); return; }`
(`prettyFormula.c:711-714`). All three `fnPrettyHist` call sites in the suite
— `:2021` (FV5), `:2057` (FV6), `:2080` (FV12) — are preceded by
`ppcTestReset()` (which sets `calcMode = CM_NORMAL`) or by an explicit
`calcMode = CM_NORMAL`, so all three return before line 719. The frames at
rows 20/168 and the content ink FV5 probes are painted by `pbPaint`
(`browsers/prettyBrowser.c:38-41`), which issues the same `lcd_fill_rect` and
the same two `drawSinglePixelFullWidthLine` calls — the pin agrees with the
bug.

**Evidence (measured).** A bare `return;` inserted immediately after the
divert guard — deleting the whole ~55-line manual pager: page counting, the
`ppfPage % pages` arithmetic, both packing passes, the `SCRUPD_MANUAL_*`
arming and `screenHoldsDrawnPixels = true` — left the gate **GREEN**
(183.56 s, zero tests red). The mutation was verified present in both the
generated `files/prettyFormula.c` and
`build.sim/custom_pkg_shadow/prettyFormula.c`, i.e. in what the compiler read.
No statement in the suite depends on any of it.

**Also unchecked: the other half of FV5's own label.** FV5 writes
`screenUpdatingMode` and `screenHoldsDrawnPixels` at `prettyTest.c:2018-2019`
and `2030-2031` but never reads them back; the only assertions on those two
globals in the whole suite are in test S3 and at `:2510`/`:2534`. The "arms
the protocol" half of the comment is asserted nowhere.

**Violated.** `prettyTest.c:2008` — *"FV5: PHIST pager paints frames and arms
the protocol; PCLR empties"*; `prettyTest.c:2058` — *"FV6 tall formula missing
from the pager"*; `testSuite/tests/pretty_print.txt:46` — *"FV5 the PHIST
pager frames + manual-paint protocol + PCLR"*. None of the three is what the
body exercises. The production comment is inconsistent with its own guard as
well: `prettyFormula.c:708-709` calls the pager *"the non-browser fallback
surface"*, and the guard two lines below sends every non-browser call to
`prettyBrowser` and returns, so the pager is reachable only **from** the
browser.

**On the dead-code half, which this finding does not claim.**
`keyboard.c:2819` swallows every item except `ITM_dotD` in the browser, and
`keyboard.c:690`/`:832`/`:940` gate softkeys on `calcMode < 19`, and
`items.c:2302` carries `PTP_DISABLED` — so a PHIST press cannot reach
`fnPrettyHist` from browser mode either. The pager appears to be unreachable in
production as well. The pager's retention was ruled deliberate by the PP1-PP16
audit and is not re-opened here (§6); what is filed is the labelling and the
zero coverage, and the fact that the same ruling's word "fallback" is now
false.

**Bug class.** A pin whose driver never reaches the code its label names,
passing because a different surface paints the same pixels.

**Class-level test.** For every pin whose label names a specific production
function, assert entry: a counter or a state precondition inside the function
under test, checked by the pin. T28, T29 and B10 already do this (each asserts
its fixture is actually exercising the class); FV5/FV6 are the counterexample.

---

### PP18RR3-15 — DESIGN.md §6 still specifies PHIST as a manual-paint pager with "zero keyboard.c/defines.h churn", which the shipped code, `prettyPrint.h` and §7 and §9 all contradict

`design-docs/pretty-print/DESIGN.md:338-351`.

**What breaks.** Read path, not a keypress. `fnPrettyHist` raises calcMode 20
unconditionally from `CM_NORMAL`; the pager body below is unreachable (three
independent gates, §6); the package owns `patches/010-keyboard.c.patch`
(76 adds, 11 hunks, eight sites) and `patches/010-defines.h.patch` with
`CM_PRETTY_BROWSER` at `defines.h:1716`. `prettyPrint.h:36-38` says so
plainly: *"PHIST … opens the formula BROWSER (calcMode 20 — UP/DOWN select,
`.d` pans a wide row, ENTER recalls the result to X, EXIT leaves)"*.

**Why this is not ordinary doc lag.** The authoritative document contradicts
**itself** about the same stage. `DESIGN.md:816` (§9 staging) gives PP4 as
*"history browser (calcMode 20, the keyboard.c stage)"*; `DESIGN.md:338` gives
PP4 as *"a PAGER, not a browser mode … Repeated PHIST presses page forward …
Zero keyboard.c/defines.h churn; the full CM-mode browser … remains an
explicitly possible upgrade on reserved calcMode 20"*. `DESIGN.md:768` (§7)
heads its own cell *"20 CM_PRETTY_BROWSER, WIRED since PP10"* while the body
of that same cell still explains that the browser was traded away. Three
statements, two of them false, in one authoritative file — and
`DESIGN-HISTORY.md` is its **non-normative** amendment trail, so the trail
cannot adjudicate which of two mutually exclusive normative sentences is live.
The project's own precedent is to repair DESIGN.md, not defer:
`DESIGN-HISTORY.md:1433-1434` records exactly that being done once for the §7
row header, in a pass that left the same cell's body and §6 stale.

**Why it is filed when round 2 dismissed doc drift.** Round 2's "doc drift,
found and not filed" section enumerates §8 BSS, the inline band figure, the
hook signature and `ppLeafScratch` — not this. Its stated bar was to file a
documentary item *when a maintainer acts on it*, and §7 is headed **"Composition
claims (BINDING for other packages)"**: its calcMode cell is the sizing input
another package owner reads. A maintainer sizing a change from §6 will believe
calcMode 20 is free and that PHIST costs no keyboard.c hunks, on the file §7
itself calls *"the project's riskiest three-package composition surface"*.

**The concrete residue is already in the tree.** `AUDIT R4-4`'s placeholder fix
for a row that will not build was applied to the **browser** copy of the
packing walk (`prettyBrowser.c:47-75`, `PB_UNSHOWN_H`) and not to the **pager**
copy (`prettyFormula.c:737-746`), which still `continue`s a declined row; and
the browser re-spells `PPF_BAND_TOP`/`PPF_BAND_BOTTOM`/`PPF_ROW_GAP` as the
literals 25/163/5. Two copies of one walk, one of them fixed. (They agree
numerically today, and the pager copy is unreachable, so this is maintenance
divergence rather than a live bug — which is why it is inside this finding
rather than its own.)

**Corrections against the finder.** The `violated:` clause mis-cited
`CLAUDE.md`; that sentence is scoped to forth-core, and the correct source is
`DESIGN.md:3` itself. And "a maintainer will believe calcMode 20 is still free"
is half overstated, because §7's row **header** does say WIRED — the
contradiction is inside the cell.

**Bug class.** Authoritative document holding both the superseded rule and its
replacement, with no mechanism that makes the superseded one loud. Second
instance this round: `PP18RR3-4` is the same shape in the PP14 section.

**Class-level test.** Not a test — a documentation convention. Every RULED
bullet that a later ruling supersedes gets struck or dated in place rather than
answered elsewhere in the same file. The mechanical half that *is* testable: a
grep-level check that every `calcMode` constant the package defines appears in
§7's table with a state matching `defines.h`.

---

## 4. PLAUSIBLE

Survived refutation; nobody could construct the reaching input. One this round.

### PP18RR3-P1 — `ppfBuildEntry` pushes `ppfRun` results without checking them, so a `PP_NONE` from an exhausted text pool at the TKRES token is read as "this entry has no stored result"

`packages/pretty-print/prettyFormula.c:515` (`resultRun = ppfRun(ppfValBuf,
ctxFont);`) and `:608` (`if(withResult && resultRun != PP_NONE)`).

**The mechanism is real and confirmed.** `ppfRun` is a thin wrapper over
`ppNewRun`, which returns `PP_NONE` on `ppTextLen + len + 1 > PP_TEXT_BYTES`
(512). `ppfFormatStaged` writes into the separate static `ppfValBuf[96]`, so
formatting still succeeds when the shared text pool is full — the TKRES
`ppfRun` at :515 is the sole failure point, and :608 then reads its `PP_NONE`
as "absent". The sibling `ppfRun(" = ")` on the very next line **is** checked
(`eq == PP_NONE` → `return false`), which is what makes :515 read as an
omission rather than a policy. The two surfaces then disagree: the PHIST row
draws the formula with no `= result`, while `pbFindResult`
(`browsers/prettyBrowser.c:159-215`) decodes the TKRES straight out of the byte
stream, never consulting the layout pool, so ENTER on that row still recalls a
value.

**Why it is not CONFIRMED.** Nobody constructed the reaching input, and there
is a real constraint against it: the entry byte stream is hard-capped at
`PPC_HIST_BYTES/2` = 320 B (`prettyCapture.c:410`; `DESIGN.md:290`, *"Oversized
entries (> half the ring) are dropped, not stored"*), while the finding needs
480–500 B of *formatted* text — plausible only because the stream stores raw
payloads that `ppfFormatStaged` expands to 40 digits for a `dtLongInteger`
leaf. The reader who confirmed it was assigned the intent lens and explicitly
did not test that construction.

**And half of the same site was REFUTED, which is why the framing matters.** A
separate reader raised the `:608` guard itself as the defect ("a failed
allocation is reported as a resultless entry"). That was **refuted**: a
resultless entry is a first-class, documented, common state —
`prettyCapture.c:391-394` (*"pass -1 when the value has already left the stack
… which stores the formula without a result"*),
`DESIGN-HISTORY.md:1119-1120` (top-of-stack falloff),
`prettyInternal.h:89` (*"TKRES trails when present"*), and
`prettyTest.c:1116-1118` naming the resultless render a **correct** outcome.
The guard is load-bearing: without it a falloff entry appends `PP_NONE` into
the variadic `PP_HBOX` and paints a dangling `2+3 = `, which is the `R4-3`
defect. So what remains is only the conflation — an allocation failure landing
on a shape the design already declares truthful — with no marker and no
distinguishability specified either way.

**What would settle it.** A pool-pressure construction: file a history entry
whose decoded leaf text approaches 512 B while its stored stream stays under
320 B, and observe whether the TKRES run is the allocation that fails. If it
cannot be built, the finding is unreachable-by-construction and belongs in §6
next round; if it can, it is `PP18RR3-10`'s class at a fourth site.

**Anchor correction for whoever picks it up.** The load-bearing anchors are
`:515` and `:608`; the other leaf pushes are at `:497`, `:521`, `:537`, `:540`,
not the `:506/:519/:539/:544` the finder cited. A separate unchecked
`ppAppendChild(box, stackNode[0])` sits at `:615`.

### Carried forward, still open, not re-examined

For the ledger only: `PP18RR2-1..17` and `PP18RR2-OOF-1`; `PP18RR1-1..12` and
`PP18RR1-P1`; `PP18R4-1..11` and round 4's plausible carry (P1 `MVAR` import,
P2 `PP_MAX_DEPTH` composition); round 2's P1; round 3's P1/P2 (V65 ordering)
and P3 (`ppvAstPrec`'s missing NIL guard); and `PP18-6` (§6).

---

## 5. Design observations (D7)

Shape, not defects. Six; the first is the answer to the question this round was
called for, and the last two are about why the round found what it did.

**PP18RR3-D1 — the acceptance triangle has exactly two shared tests, and five
decisions that are spelled twice.** This is the axis's result, and it is worth
more than the finding list. Construct spelling is single-sourced:
`ppqMatchName` and `ppEqBigopIntercept` both call the same inline
`ppEqConstructIs`, whose `s[len] == '('` requirement means `SUMX(` and a
variable merely *ending* in a construct name cannot match either acceptor, and
both accept exactly all-upper and all-lower. Function names are single-sourced:
`ppqFunctionCall` gates on `ppEqFunctionItem(nm) >= 0` and the walker's
`ppvMonadicName` requires the catalog spelling to round-trip to the same item.
Four dimensions attacked both and neither broke.

What is **not** shared is everything else the two grammars both decide:
argument **arity** (`PP18RR1-6`, INTEG's fifth argument), **operand scoping**
(`PP18R4-3`, left-only), **body scoping** (`PP18-6`, the DERIV double wrap),
**variable names** (`PP18RR1-5` and the 7-glyph
`allocateNamedVariableOnMiss` cap the renderer does not know about), and the
**order literal** (`PP18RR3-12`). Five members of one family, four of them
already open, each finding proposing a class test that covers only its own
cell.

The instrument this stage actually wants is one differential oracle: a table of
construct strings driven through `ppqParse`, through `ppEqBigopIntercept`, and
through `ppvTestBuildNodes` on a program computing the same thing, asserting
that all three agree on accept/refuse **and** that the two node producers agree
on the layout signature. Every finding in §3 that touches the triangle, and
four of the open ones, are single rows in that table. Round 2 asked for this
and could not build it; this round can now say precisely what its columns are.

**PP18RR3-D2 — every parity break but one runs in the safe direction, and the
package has no name for the unsafe one.** The renderer being stricter than the
evaluator produces a *decline*: the picture disappears, upstream's linear line
remains, the arithmetic is untouched. That describes `PP18RR3-6`,
`PP18RR3-11`, the DERIV order literal, an ASCII-digit bound variable, an 8+
glyph function name, and a body the strict grammar cannot parse. The evaluator
being stricter than the renderer produces a *picture that will not compute*,
which is what `DESIGN.md:460` forbids — and this round found exactly two:
`PP18RR3-4` (the MVAR span) and the bound-variable name-length gap it enables.
The design rule names the harm but the code has no way to express which
direction a given divergence runs, so both kinds get reported, argued and
ruled one at a time. A two-column statement in §6 of DESIGN.md — *renderer
stricter is ruled acceptable; evaluator stricter is a defect* — would let
half of these be closed on sight.

**PP18RR3-D3 — the metrics contract names the font it reads and not the font
that will be painted.** `ppMetricsInit` latches from `&numericFont`;
`showGlyphCode` may substitute `numericFontBold` (`FLAG_BOLD`), replace the
character (`checkHP`), or enlarge (`maxiC`, `stdnumEnlarge`). The package
guards one of the four, at two of the surfaces, by hand
(`prettyValue.c:780`, `:848`). The `maxiC`/enlarge pair happens to be
unreachable for this engine, and — this is the good part — `checkHP`'s guard
is correct *by font*, not by luck: `screen.c:1232` makes `numDouble` require
`font == &numericFont`, and those two surfaces are precisely the ones whose
rungs contain `PP_FONT_NUMERIC`. The missing piece is a statement, in §1, of
which upstream substitutions the metrics model tolerates and which it must
decline — a four-row table that would have caught `PP18RR3-1` at review.

**PP18RR3-D4 — solo-build correctness is asserted in comments and measured
nowhere.** Two of this package's own comments (`keyboard.c:1690`, `:2807`)
treat "a solo pretty-print build behaves correctly inside its own browser" as a
maintained property, and `build-test.sh --solo` exists to run it. But every
gate run in every audit round has been the combined build, in which
undo-history's patch supplies two of the containment gates on pretty-print's
behalf (`PP18RR3-9`). The composition is engineered — identical `defines.h`
and `keyboard.c` edits from both packages unify under `git apply -3`, and the
`calcMode < 19` literal must stay a literal for that to work — but the *solo*
half of the claim has no runner in any pipeline. Either the solo build joins
the gate, or the two comments should say the property is aspirational.

**PP18RR3-D5 — the ink primitives were hardened one at a time, and the safety
argument for the third generalised past its evidence.** `prettyLayout.c` has
three ways to put ink on the screen: a rectangle, a line, and a glyph run. The
rectangle was clipped for negative origin by `AUDIT R3-11`, the line by
`R3-12` — both with comments naming the browser pan as the producer — and the
glyph run was left with a *justification* instead of a clip. That
justification exists in two places with different scope:
`DESIGN-HISTORY.md:1250` says the behaviour was *"verified against both
simulator HALs"*, and the source comment repeats the claim with the qualifier
removed. The general shape, and it is the more useful statement: **when a
safety argument depends on a property of the test environment, the source
comment is where that dependency gets lost.** A rule that costs nothing —
if the evidence is "the simulator rejects it", the comment says so — would
have made this finding visible to any of the six readers who read that
function in earlier rounds.

**PP18RR3-D6 — two mutations deleted production behaviour and left the gate
green, and both pins failed for reasons the file already knows how to avoid.**
FV10's guard is the production function under test (`PP18RR3-13`); FV5's
driver never enters the function its label names (`PP18RR3-14`). The
counter-idioms are twelve lines and a few hundred lines away respectively:
FV7's `if(!ppMeasure(…)) ppTestFail(…); else {…}`, and T28/T29/B10's explicit
"this fixture really is exercising the class" preconditions. This is the second
consecutive round to produce this observation (`PP18RR2-D5` named the
existential-probe half), and the two together suggest a short checklist for new
pins rather than a suite rewrite: assert entry, assert the negative branch,
and never let the subject of the pin be its guard.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. Mandatory section, and this round it is most of the value:
`prettyLayout.c`'s geometry and `prettyFormula.c`'s decoders were read end to
end for the first time and came out clean, and the acceptance triangle — the
thing the round was called for — is in better shape than the finding count
suggests.

### Killed by the refutation pass

**"`ppfBuildEntry` uses `resultRun != PP_NONE` to mean the entry carried a
result, so a failed allocation is reported as a resultless entry instead of
failing the row"** (`prettyFormula.c:608`). **REFUTED.** The finding's stated
contract violation is inverted by the design record: a history entry carrying
no result is a first-class, documented, common state, and `withResult &&
resultRun != PP_NONE` is its ruled encoding — `withResult` is the caller's
permission to attach a tail, `resultRun` is whether one was recorded.
`prettyCapture.c:391-394` and `:417-420` give four legitimate causes
(`resultReg = -1`, matrix result, payload > 16 B, entry buffer full);
`DESIGN-HISTORY.md:1119-1120` rules top-of-stack falloff emits without one;
`prettyTest.c:1116-1118` states the semantics in-tree and calls the resultless
render a truthful outcome. The guard is load-bearing: `ppAppendChild` no-ops
on `PP_NONE` and `ppMeasure` has no arity check for `PP_HBOX`, so removing it
paints `2+3 = ` with a dangling equals on every ordinary lift. Nor is this the
`R4-3` mirror — `R4-3` was an *unchecked* append and its ruling recorded an
explicit four-site sweep that correctly excludes this one. The residual
mechanical observation survives as `PP18RR3-P1` and nothing more.

**"`fnPrettyHist`'s pager half runs only inside `CM_PRETTY_BROWSER`, where the
containment guard swallows `ITM_PHIST` — the surface §6 describes cannot be
raised at all"** (`prettyFormula.c:711`). **REFUTED as a code defect.** The
unreachability is a recorded, already-mutation-tested decision: the PP1-PP16
audit's own §6d states *"`fnPrettyHist`'s pager body is unreachable through
the keyboard and is retained deliberately … 'the non-browser fallback surface
and for the packing reference'"*, and §6a records that the mutation was
already run — *"killing it entirely produced no failing pin — but its
retention is exactly what `prettyFormula.c:669-671` rules as deliberate
fallback code."* The containment guard is written as a class rule whose exempt
set is the browser's documented key list, and `prettyPrint.h:36-38` fixes that
list post-PP10 with no paging role: PHIST is contained **by design**, not by
omission. An ordering check settles the last doubt — the audit that ruled it
(`0115b17a3`, 06:36) precedes the commit that wrote the §6 TI_SHOWNOTHING
scope note (`181be2f0e`, 20:49), so the exclusion does not rest on live
paging. What survives is the documentary half, filed as `PP18RR3-15`, and the
zero coverage, filed as `PP18RR3-14`.

**"V19's 'a stale solver session changed the drawing' assertion cannot fire —
nothing in VISUAL's drawing path reads `currentSolverStatus` any more"**
(`prettyTest.c:4839`). **REFUTED by mutation, and this is the cleanest kill of
the round.** The factual observation is true — deleting the mask at
`prettyVisual.c:1601` leaves the gate green — but the conclusion is backwards.
With a downstream frame read reintroduced *and* the mask gone, V19 is the
**only** assertion in the entire battery that fires:
`prettyPrint test FAIL: V19 a stale solver session changed the drawing`. So
V19 is dormant, not vacuous: it is a black-box property pin ("a program's
drawing is insulated from solver session state") whose property is genuinely
unviolated today, which is a true pass. The retention is ruled in place
(`prettyVisual.c:1595-1599` concedes the no-reader state and rules the
save/restore stays *"regardless of who happens to read it today"*), and the
comment's "V19 pins them" takes *them* from the preceding "the save and
restore stay". What survives is smaller: `DESIGN.md:675-678` and
`TESTING.md:257` (MUT-85) still teach `ppqShowRender` as VISUAL's live framing
mechanism, which PP18 removed — the `PP18R2-7` prose class, listed under doc
drift below.

**"The MVAR-mode intercept skips the whole construct span … and DESIGN.md
documents the opposite"** (`solver/equation.c:1998`). **REFUTED as filed**, and
its surviving core is `PP18RR3-4`. The whole-span skip is an explicit, dated,
gate-pinned ruling recorded in DESIGN.md *itself* thirty lines above the stale
bullet the finding cited; the behaviour the finding demanded back is the named
"Bug 1", whose restoration turns EQ29 red with syntax error 45 and which made
constructs impossible to **save** from the keyboard. Under it the finding's own
reaching input cannot exist. The finding's remedy was worse than the defect;
the defect it noticed is real, and is filed with the remedy constraint stated.

**"Browser panning is one-way: `pbPan` only ever increases and is clamped at
the right edge"** (`browsers/prettyBrowser.c:110`). **REFUTED.** The exact
behaviour was ruled on by name in the fix that created it: `f1f77b940`
replaced `pbPan = 0;   // wrap` with
`pbPan = maxPan;   // clamp at the right edge rather than snapping back`, and
its message says *"Pan clamps at the right edge instead of snapping back."*
The finding's own remedy list was "a second direction (or a wrap at maxPan)";
the wrap is what was explicitly removed, and a second key is another
`case` block in the file `DESIGN.md` calls the riskiest three-package
composition surface, which the design ruled against as policy. The finding's
normative anchor was also a miscitation (DESIGN.md:339-340 is the PP4 pager
ruling, not a panning promise; "pans horizontally when wider than the screen"
is `prettyBrowser.c:9-10`, and the row does pan horizontally). Coverage exists
and is non-vacuous: T25 and T29, the latter with a self-check that fails if the
row is not actually wide enough to pan. **What does survive is a two-line
comment defect**, listed under doc drift.

**"`ppEqFunctionItem`'s banner says the evaluator calls it; the evaluator keeps
its own copy of the same three loops, and the copy is already not exact"**
(`solver/equation.c:217`). **REFUTED on reachability, provably.** The claimed
failure — a name the renderer accepts and the evaluator refuses on the 7-glyph
token gate — cannot be constructed, and not by accident: `ppEqFunctionItem`'s
accept set is exactly (alias table) ∪ (`EIM_ENABLED`, `param <= NOPARAM` item
names). Every literal in `functionAlias[]` was extracted and sorted — longest
are `arcsinh`/`arccosh`/`arctanh` at 7 glyphs; all 63 `EIM_ENABLED` rows in
`items.c` were listed — longest are 6, and the package's `items.c` override
adds none (`diff` of the `EIM_ENABLED` rows against upstream returns 0). The
gate triggers at 8. The two acceptors are in exact parity for the function-call
construct, and the gate is dead weight for this input class. The H2 convention
the finding invoked also does not apply: H2 covers *superseded* upstream code,
and the three resolution loops are the live XEQ path, byte-identical to
upstream with no hunk near them — deleting them would *add* override churn.
What survives is doc drift: the banner and `DESIGN.md:715` both say the
evaluator calls `ppEqFunctionItem`, and grep shows two callers, both in the
package.

### The acceptance triangle, checked and matching — this is axis (a)'s result

Every item here was traced by at least two dimensions independently.

- **Construct spelling.** `ppEqConstructIs` is the single test, called by
  `ppqMatchName` and by `ppEqBigopIntercept`. Its `s[len] == '('` requirement
  is bounds-safe in both callers (`ppqMatchName` pre-checks `pos + len <
  len`; the evaluator's string is NUL-terminated). `SUMMER(`, `ASUM(` and a
  variable ending in a construct name are refused by all three acceptors;
  `Sum(` is refused by both. All four names, both spellings.
- **Argument arity.** SUM/PROD 4–5 renderer vs `needMin 4`/`needMax 5`
  evaluator; DERIV 3–4 both. The only mismatch is INTEG's fifth argument,
  which is the KNOWN `PP18RR1-6`.
- **Top-level `;` slicing** agrees on nested constructs
  (`SUM(SUM(X;X;1;2);X;1;3)` slices identically in both), and both walk
  two-byte glyphs the same way.
- **Label-prefix skip.** `ppqParse`'s (:762-774) against `parseEquation`'s
  (:1345-1358): same 7-glyph bound, same `':'`/`'('` terminators, same
  unlabelled-reset semantics.
- **Function names.** `ppqFunctionCall` gates on `ppEqFunctionItem`; the
  walker's `ppvMonadicName` requires a round-trip to the *same* item.
  `ITM_MAGNITUDE`'s catalog name is `"|x|"` and `ITM_ABS`'s is `">ABS<"`, so
  both correctly fail `ppvNameIsDrawable` exactly as DESIGN claims.
- **Body scoping.** The parser sniffs top-level run TEXT for `'+'`/`'-'`;
  the walker asks precedence. No false positive is possible: every run the
  grammar builds is ASCII letters, digits, `'.'`, or two-byte glyphs whose
  low bytes are 0x60–0x6b / 0x80–0x89 / 0x00–0x0f / 0xb7 / 0xd7 — none is
  0x2b or 0x2d. False negatives require a PAREN that already scopes. Checked
  against nested constructs, parenthesised bodies, leading signs, `STD_DOT`
  products and `·₁₀` exponent tails.
- **Name length caps.** Walker `PPV_NAME_MAX` 15, evaluator `varName[16]` —
  so the walker cannot mint a name the evaluator rejects on length.
- **`^` associativity.** `_operatorPriority` returns 7 for
  `PARSER_OPERATOR_ITM_YX`, and odd means right-associative, matching
  `ppqFactor`'s right recursion; unary minus is rewritten `-1 ×` at priority
  8, so `-x^2` is `-(x^2)` in both acceptors. Checked because a mismatch here
  would be a silent wrong picture with a number behind it.

**Parity breaks deliberately not filed, all one-way in the safe direction:**
a DERIV order that is an expression rather than a literal
(`DERIV(X;X;1;1+1)` computes and declines to draw — the code states the choice
at `prettyEquation.c:410-411`, *"DERIV's order is a PARSE question"*); a bound
variable spelled with ASCII digits (`ppqName` admits subscript digits, not
ASCII ones); a bound variable of 8+ glyphs (`ppqName` accepts any length,
`allocateNamedVariableOnMiss` caps at 7 glyphs) — this last one announces
itself with `ERROR_SYNTAX_ERROR_IN_EQUATION` and the text "bad big-operator
variable name", and a divergence that announces itself is not the class
`DESIGN.md:460` forbids. Each of these is a decline, the arithmetic is
untouched, and §6 of DESIGN.md already rules that the renderer's grammar may be
narrower. They are listed because they are rows in `PP18RR3-D1`'s table, not
because they are defects.

### `prettyLayout.c` geometry, re-derived rather than assumed

Two dimensions independently re-derived every measure/paint pair numerically
and found the two passes agree. `PP_RAD`: the vinculum fill spans
`x+child.relX-1 .. x+nd->width-1`, exactly the measured box, and
`vincTop = baseline - nd->ascent` by construction. `PP_BIGOP`:
`over.relBase = -(ga+2+over.descent)` lands the over-limit's ink top on
`baseline - nd->ascent`; `colW` starts at `gw` so both centering divisions are
non-negative; `nd->width = colW+3+body.width` matches `body.relX = colW+3`.
`PP_PAREN` glyph arm: `nd->width = child.width + 2*parAdvance` and the closing
run draws flush at `x + nd->width - parAdvance`. `PP_FRAC`: the bar band sits
`fracGap+1` below the numerator's descent and `fracGap+2` above the
denominator's ascent, which is what `DESIGN.md`'s R5-1 amendment claims.
`ppFillVal` clips both edges on both axes **before** the `uint32_t` cast
(R3-11); `ppDrawLine` screens all four bounds per pixel (R3-12);
`ppDrawIntegralSign` clamps `hh` to [3,7] so the `(hh-1)²` divisor is never
zero. `ppNewRun`'s `ppTextLen + len + 1 > PP_TEXT_BYTES` promotes to int, no
wrap. `PP_NONE` = 0xFF cannot collide with a pool index (72 nodes).

### Fail-closed paths that are actually closed

- Every `ppMeasure` arity precondition (RAD 1–2 children, SUB/SUP/FRAC exactly
  2, INT/BARS/PAREN exactly 1, BIGOP exactly 3) is re-satisfied by the
  corresponding paint arm, and all six `ppPaintAt`/`ppRenderRightAligned` call
  sites are gated on a successful `ppMeasure` of the same root in the same pool
  generation. This is what makes `ppPaint`'s unguarded
  `ppPool[nd->firstChild]` indexing safe; all eight paint entry points were
  enumerated to confirm it.
- `ppfBuildEntry` returning `true` with `*rootOut == PP_NONE` is harmless:
  every consumer goes through `ppMeasure`, which rejects `n >= ppNodeCount`.
  The sibling shape — `stackNode[0] == PP_NONE` reaching the `= result` HBOX —
  is unreachable, because `ppcEmit` refuses a root that is not OP1/OP2/BIGOP,
  so every stored stream ends in an operator token and every combine arm
  checks its operands.
- `ppEqDepth` is incremented after all validation and decremented on every path
  below it (the single `--ppEqDepth` dominates all exits); `ppEqStackBase` is
  re-armed at every depth-0 entry; `ppEqTempAppend`/`ppEqTempDelete` are
  strictly LIFO across nesting and every failure arm deletes, including all
  five `break`s of the SUM/PROD loop. Neither the depth counter nor the stack
  fence can latch a construct off permanently.
- `ppReset` ordering: every pool consumer resets, builds, measures and paints
  before the next reset, and no caller holds a node index across a reset.
- `ppvPaintStackWindow`'s failure leaves the band untouched (`ppvClearBand`
  runs only after the fit is known), so the fall-through to full-screen and
  then to the error cannot leave a half-cleared screen — PP18-2's fix holds at
  both sites.
- The capture hook against upstream's error rollback: `prettyNoteFunctionDone`
  runs ~175 lines before `reallyRunFunction`'s `undo()`, which is the classic
  stale-shadow shape — and `prettyCapture.c:849-859` tests `lastErrorCode`
  first and tears the shadow down (*"a failed function may have partially moved
  the stack"*). Closed.
- All seven pretty items are `US_UNCHANGED`/`SLS_UNCHANGED`, so pressing
  VISUAL/EQSHW/PSHOW does not burn the owner's undo point and a decline does
  not trigger `reallyRunFunction`'s `undo()`.
- `ppfStageValFields` clearing `lastErrorCode` after a failed
  `reallocateRegister` also wipes the `screenUpdatingMode = SCRUPD_AUTO` that
  `displayCalcErrorMessage` assigned — checked and cleared on reachability:
  every caller of the value-staging path is gated on `lastErrorCode == 0`
  before entry or runs inside `CM_PRETTY_BROWSER` where no manual bit is set,
  and `screen.c:3783`'s reader of `errorMessageRegisterLine` is itself gated on
  the code the clear zeroes. The clear is what `prettyPrint.h`'s "there is no
  error path, only decline" demands. Worth a comment, not a finding.
- `prettyBrowserEnter`'s post-`liftStack` allocation failure leaves X holding
  an uninitialised `real34` — which is upstream's own shape at every recall
  site, the error is left set and displayed here, `saveForUndo` has already
  run, and the `lastErrorCode == ERROR_NONE` guard before the `xcopy` is
  exactly what stops a 40-byte long-integer payload landing in a 16-byte
  `real34`.

### Guards whose conjuncts were falsified, or proved load-bearing

Non-falsifiable conjuncts, noted as noise and not filed: `ppMeasure`'s
`n == PP_NONE ||` (subsumed by `n >= ppNodeCount`, since PP_NONE is 0xFF and
the pool is 72); `ppqScopeOperand`'s `n == PP_NONE ||` (subsumed by
`nd == NULL`); `ppvPushLifting`'s `stk->depth > 0`. Dead-but-defensive arity
guards: `ppMeasure`'s three-child rejection for `PP_RAD` and its second-child
rejection for `PP_INT` — no producer builds either shape. Proved load-bearing
rather than removable: the `resultRun != PP_NONE` test above, and
`ppvNameIsDrawable`'s `i >= PPV_NAME_MAX - 1`, which is exactly right for a
15-char name into `char[16]`.

Buffer bounds re-derived rather than assumed and found correct:
`ppfVariableName`'s "out cap >= 17" against all three callers (`dv[20]` each);
`ppfLabelName`'s `snprintf(out, 17, …)` into `lbl[24]`; `ppfBigop`'s
`text[96]` against a 66-byte worst case; `ppfFromCaptureNode`'s LIT+LIT2
30-byte maximum into `text[32]`; `ppqFrameDerivative`'s `den[28]`;
`ppvVarName`/`ppvDerivVariable`/`ppvMonadicName`/`ppvBody` all copying behind a
length check; `ppfFormatStaged`'s `buf[200]` and `prettyValue.c`'s
`ppLeafScratch[200]`, both with ample margin at their producers' digit counts.
Only the two 48-byte buffers of `PP18RR3-7` are tight.

`ppfLabelName`'s `*(p-1)` test mirrors upstream's own idiom at
`softmenus.c:1712` and cannot mis-decode a numeric local label's number as a
length. `ppcSerializeNode`'s BIGOP arm calling the second child before testing
the first's `0xffff` is safe, because every leaf arm tests `off + k > cap` in
int arithmetic and returns `0xffff` before writing. `ppcRclLeaf` only mints a
`PPN_RCL` for `param <= 99`, so `snprintf(rname, 8, "R%02u", item)` can never
truncate. `ppcEmit`'s eviction loop cannot spin (`buf` is 320 B, the test is
640) and `ppcHistEvictOldest`'s compaction reads index 11 of a 12-element array
at worst. `ppfBuildRow`'s height test admits exactly 139 rows and a 139-row row
painted at y=25 occupies 25..163 inclusive — exact fit, no off-by-one.

### Unreached shapes, named so they are findings the day something changes

- `ppqExpr:697-701`'s leading-sign arm allocates `lead` unchecked and advances
  past the sign regardless, so `lead == PP_NONE` at :707 is indistinguishable
  from "there was no sign" — which would draw `X/2` for `-X/2` and report
  success. Unreachable: a 1-glyph run is the cheapest allocation in the file,
  so any pool state that fails it also fails the `ppqTerm` on the next line and
  the parse declines. Recorded because the sibling loop at :727 **does** check
  its `op`.
- `prettyBrowserPan`'s unbounded `pbPan += 60` on a **narrow** row: the clamp
  never runs, `pbPan` grows, and `int16_t` overflow arrives after 546
  consecutive presses on one row. No visible effect, because the
  `x = 8 - pbPan` assignment is inside the `n->width > visible` arm. Signed
  overflow is UB; no consequence the owner sees, so it stays here.
- The `GROUPWIDTH_LEFT`/`RIGHT` configuration exposure named inside
  `PP18RR3-7`.
- `ppqBigopConstruct`'s orphaned `varRun` node for INTEG, allocated and never
  linked; and the 1–2 px overdraw of `ppDrawIntegralSign` past `gx+gw` at the
  minimum stroke width.

**One clearing this round was overturned by measurement, and the distinction is
worth keeping.** The guards dimension cleared `ppqFrameIntegral`/
`ppqFrameDerivative`'s unframed return as UNREACHED, on the reasoning that any
equation dense enough to exhaust the pool during framing would fail the
width/height gate first. The errorpaths dimension's verifier measured it:
70 nodes used, both caps passed with margin, pixels identical.
"I could not construct it" and "it cannot happen" are different claims, and
this round produced an instance of the difference in the same section that
records both.

### Doc drift, found and not filed as defects

- `prettyBrowser.c:152`, *"paint wraps when past the row's width"* — paint
  clamps; `f1f77b940` removed the wrap and the comment 15 lines below it now
  contradicts this one. `DESIGN-HISTORY.md:924-925` (*"`.d` pans a too-wide
  selected row (wraps)"*) is the same staleness in the trail.
- `DESIGN.md:675-678` and `TESTING.md:257` (MUT-85) still teach
  `ppqShowRender` as VISUAL's live framing mechanism; PP18 removed it, and the
  mutation confirms MUT-85 no longer kills.
- `ppEqFunctionItem`'s banner and `DESIGN.md:715` say the evaluator calls it;
  it has two callers, the renderer and the walker.
- `ppqBuildCall`'s banner claims "both callers gate on `ppEqFunctionItem`
  first" when PP18 left it with one caller.
- `DESIGN.md:37` still says `ppNode_t ppPool[48]`; `prettyInternal.h:30`
  defines `PP_POOL_NODES` 72. `prettyTest.c:2933` (EQ22) says "the 64-node
  pool" for the same constant.
- `prettyTest.c:346`'s comment names rows 131-132 for the vinculum where the
  body scans 130 and treats 132 and 133 as gaps; the failure strings match the
  body, so the pin is right and the prose is one row stale.
- `testSuite/tests/pretty_print.txt` describes `prettyTestFormula` as
  "FV1 … FV5" and `prettyTestVisual` as "V1-V20"; both drivers have grown well
  past that.
- `prettyFormula.c:708-709` calls the pager "the non-browser fallback surface"
  while the guard two lines below sends every non-browser call away. (The
  retention ruling stands; the word "fallback" is false.)

### Ruled, known, or below the bar

- **`PP18-6`**, the walker's doubled parentheses on an additive DERIV body:
  ruled leave-alone by round 1, re-affirmed by round 2, re-derived and
  re-fenced by restarted round 1, and re-derived twice more this round. The
  ruling's revisit condition is a measured height consequence pushing a real
  formula out of the Z/T band; that was not measured. What **is** new, and is
  recorded here for whoever eventually takes it: the divergence has now been
  measured directly — walker `[F(d|[d x]) U(P(P([x + 3]))|[x = 3])]` against
  parser `[F(d|[d x]) U(P([x + 3])|[x = 1])]` — and no pin on either front-end
  can see it, because every DERIV fixture in the suite has a multiplicative or
  atomic body, V74 asserts only `ppTreeHasRun(root, "d²")`, and the
  `ppvTestExpect` pins read `ppvSerialize`, which emits no parentheses at all.
  EQ21, which the finder cited as pinning the parser's single paren, actually
  parses `DERIV(X;X;2)` — an atomic body — so it pins `P(x)`, not `P([x + 3])`.
- The KNOWN 42, re-derived independently and not re-reported: `PP18RR1-1`
  (PP_RAD index descent), `-5` (`ppvNameIsDrawable` narrower than `ppqName`),
  `-6` (INTEG's fifth argument), `-7` (three hand-enumerated monadic sets),
  `-9` (paint re-deriving measure textually), `-10` (V28's hand-copied band
  offsets); `PP18R4-1` (`ppvNameInSubtree`'s missing latch), `-3`
  (`ppqScopeOperand` left-only), `-8`/`-9`/`-10`/`-11` (the vacuous-pin set);
  `PP18RR2-12` (the postfix-8 ceiling, re-confirmed as still the only operand
  bound in `ppfBuildEntry`), `-14`, `-15`, `-16`, `-17`. Four of the guards
  dimension's strongest independent derivations turned out to be already-ruled
  items, which is a real signal: the axes that have been rotated are worked
  out, and what is left is in the corners nobody has read.
- `fnPrettyVisual`'s label ladder is byte-for-byte `fnPgmInt`'s including both
  error arms; `ppvSumProd`'s pop order matches `_programmableSumProd`;
  `prettyBrowserLeave` restoring from a private `pbPreviousCalcMode` rather
  than the global `previousCalcMode` is safer than upstream's convention, not
  worse; the browser clearing `FLAG_ALPHA` and `cursorEnabled` on entry and
  restoring neither is byte-for-byte upstream's `registerBrowser`.
  Upstream-convention-first, all four.
- `ppfStageValFields`'s `xcopy` of `bytes` into `TEMP_REGISTER_1` with no size
  cross-check is upstream's own `copyRegister` call, header included, and
  `bytes` is capped at 16 by the payload guard.
- EXIT and ENTER out of the browser both land in `btnReleased` **after**
  `keyboard.c:2139` has set `SCRUPD_MANUAL_MENU`, so `prettyBrowserLeave`'s
  `screenUpdatingMode = SCRUPD_AUTO` is the last write before
  `refreshScreen(117)` and the menu the browser painted over **is** repainted.
  The `CM_PRETTY_BROWSER` omission from the `RELEASE_END` browser list looks
  like a defect and is not — but it is one edit away from becoming one: move
  `prettyBrowserLeave` to press time and the menu band stays blank.
- `FV18`'s pool-starvation arithmetic re-derived and found live, including the
  "implausible node count" plausibility guard, which cannot mask a
  mis-calibration. Its unpinned sibling — `ppfCombine2`'s `default:` arm — is
  unreachable, because `ppcClassify`'s `PPC_DY` set is exactly the seven items
  that each have their own arm.
- `F3`'s `displayValueX` parity pin: checked for the one-shot-flag shape and
  cleared — `updateDisplayValueX` is never cleared by `display.c`, both renders
  write, and F2 leaves a stale FRACT-OFF form that a skipped mirror would
  expose. `V65`, `V66`, `V70`, `V73`, `V74` and `FV20`'s else-if ladder all
  checked for vacuity and cleared with the argument recorded.
- `ppcTestType` silently drops any character that is not a digit, `'.'` or
  `'<'` — a classic silent-no-op fixture. All 19 distinct literals passed to it
  across the file were enumerated; none is being quietly truncated today.
- `EQ4`'s four decline pins each name a distinct reason while `ppqParse`
  returns only a bool, so a pass does not prove the intended reason fired —
  same class as the KNOWN `PP18RR2-17`, and the class does not change: EQ1-EQ3
  assert positively in the same driver, so a globally-declining parser goes
  red.
- `FV19`'s `CM_PRETTY_BROWSER < 19 || > 23` compile-time comparison over
  hand-copied magic numbers is the `PP18RR1-10` shape at a new site, and unlike
  V28 there is no derived quantity to read instead — the constants **are** the
  contract.

### Cross-package composition

The override surface is unusually disciplined and this round found nothing new
in it beyond `PP18RR3-9`. Checked and cleared: the 565-line append to
`equation.c` rather than a new file (it binds to file-static machinery —
`_pushNumericStack`, `_processOperator`, and PARSER_* macros expanding against
a local `mvarBuffer` — and is appended at EOF plus one forward declaration,
the lowest-conflict shape available); the local `_fnIntegrate` declaration
(upstream exports it without a header); `defines.h` and three `keyboard.c`
gate lines edited **byte-identically** by pretty-print and undo-history, which
merge because `pkg_patch_apply.py` uses `git apply -3` with the pre-image blob
seeded — and which is why the added text must stay the literal `calcMode < 19`
rather than a symbolic name; `CM_PRETTY_BROWSER 20` inserted out of numeric
order to keep the hunk count down; `items.c` row 984's
`TM_LBLONLY / |99 / PTP_LABEL` matching upstream's own PGMINT/PGMSLV rows; the
three distinct `funcTestNoParam[]` anchors within an 11-line window; and
`pretty_visual_real` anchored before `graphs_cov` with the reason in the patch
itself. `prettyLayout.c` and `prettyFormula.c` reimplement no upstream
renderer — ink goes through `showGlyphCode`/`showString`/`stringWidth`/
`lcd_fill_rect`, and the one hand-rolled loop (Bresenham over `setBlackPixel`)
exists because upstream has no line primitive and carries its own bounds note.
The single churn hit is the sanctioned guard-wrap artifact; it has no catalog
entry, so `patch_churn_scan.py` exits 1 on this package and every reviewer
re-derives it — worth one line in `deliberate-exceptions.md`.

---

## 7. Verdict

**Would I ship this? No** — and again for a different reason than the previous
two rounds gave. Restarted round 1's blocker was ink outside a measured box;
round 2's was a mirror that keeps describing registers it no longer mirrors.
This round's is that the drawing engine's own calibration is wrong for anybody
who has BOLD on, and that the stage's headline feature freezes the menu row on
its ordinary entry path. Neither needs an unusual machine, a program, or a
mode nobody uses — one is a settings checkbox, the other is how VISUAL is
invoked.

**Where would it break first?** In an owner's hands, in this order:

1. **`SF 42` or the BOLD checkbox, then any value at all.** `PP18RR3-1`: every
   numeric run paints one row above and two rows below the box the engine just
   cleared, and the fraction bar stops coinciding with the minus sign — which
   is the one calibration `DESIGN.md` §1 states as the reason the metrics are
   derived live. Cheapest thing on the list to confirm.
2. **VISUAL from the PP softmenu, on any label.** `PP18RR3-2`: the softkey row
   and status bar stop repainting until something else clears
   `SCRUPD_MANUAL_MENU`, and the row keeps showing the label-entry menu that
   was popped. Measured, not inferred.
3. **PHIST on a wide row, then `.d`, on the device.** `PP18RR3-3`: 39 writes
   per pan with a wrapped-negative x, landing a couple of bytes into following
   LCD lines. Both simulator HALs discard them, which is why five audit rounds
   and every gate run have been blind to it.
4. **Any equation with a comma decimal, for a comma-radix owner.**
   `PP18RR3-6`: the 2D strip never appears. Whole population, every equation.
5. **A parameterised construct in the solver** (`PP18RR3-4`) and **EQSHW on a
   long linear equation** (`PP18RR3-5`) — a picture that will not compute, and
   a truncation that reads as the whole equation.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.** `PP18RR3-12` (the DERIV `uint16_t` narrowing) — the
2026-08-27 audit already ruled this one leave-alone and it was right: nobody
types 65537, the class test is worth more than the fix, and the only thing this
round adds is that no *design* ruling sanctions it. `PP18RR3-8` (uncapped
`ppqParse` recursion) — real, and the ARM frame arithmetic is exact, but it
needs a deliberate few hundred keystrokes; take it when someone is in that file
and the cap is four lines. `PP18RR3-7` (the 48-byte buffers) — the measured
overrun is two bytes and the reaching configuration is a specific preset plus
`FIX 19`; worth fixing with the sweep, not worth a wave on its own.
`PP18RR3-10` (the unframed frame return) needs the pool at 70 of 72, which is
an unusual equation. `PP18RR3-15` and the doc-drift list are one commit
between them. And `PP18RR3-P1` should not be fixed at all until somebody
constructs its input — the sibling framing was refuted precisely because the
"obvious" repair would break a documented, common state.

**What should not wait.** `PP18RR3-1` (a guard the package already knows how to
write, at a surface that is always on); `PP18RR3-2` (the ruling on the line
above the code is false, and the fix is a decision about which bit
`_refreshNormalScreen` should test); `PP18RR3-3` (device-only, invisible to
every runner the project has, and the third member of a class whose other two
were fixed); `PP18RR3-4` — as a **ruling first**, because the obvious repair is
the reverted Bug 1 and the actual question is how MVAR mode enumerates a body
it must not tokenise; and `PP18RR3-6`/`-11`, which are one conjunct each and
are the third and fourth instances of a class this project has now paid for
three times.

**What is genuinely solid, verified rather than assumed.**
`prettyLayout.c`'s geometry — every measure/paint pair re-derived numerically
by two dimensions, and they agree, including the three pieces of arithmetic
(`PP_BIGOP`'s baseline, `PP_FRAC`'s bar band, `PP_RAD`'s vinculum span) that
would have been the natural place for a defect. The history ring's decode
arithmetic is bounded at every token and its literal-length chain is exact at
both ends. Every `ppPaint` arm is protected by a matching `ppMeasure`
precondition, verified across all eight entry points. The evaluator's temp-slot
and depth discipline is LIFO-correct on every exit including five loop
`break`s. `ppEqConstructIs` and `ppEqFunctionItem` really are the single shared
tests they claim to be, and four dimensions failed to break either. And the
package's cross-package composition is engineered rather than lucky — the
identical-edit merge trick in `defines.h` and `keyboard.c` is the correct
answer to a genuinely hard problem.

**The pattern to carry.** Round 2's statistic was that most findings come from
the previous wave's fixes. This round has no fix wave to blame, and it produced
a different pattern worth naming: **three of the top five findings are a class
fixed at the sites where it was noticed, with a sibling left out** — the
metrics-vs-substituted-font guard written for `checkHP` and not `FLAG_BOLD`,
the negative-origin clip written for the rectangle and the line and not the
glyph, and the browser-containment list written from examples rather than from
the switch. In each case the *comment explaining the fix* is present and
correct at the site it was applied, which is exactly why the sibling stayed
invisible: the reader who arrives sees a hazard that has been thought about.
The remedy is `PP18RR3-D1`'s and `PP18RR3-D3`'s shape — enumerate the class
mechanically once (the substitutions `showGlyphCode` can make; the primitives
that put ink on the screen; the `case` blocks with a calcMode list) and assert
over the enumeration, rather than writing a fourth good comment.

---

## 8. Round and exit state

**Round: PP18 round 3 of the restarted series**, same subject
(`pretty-print/stage-pp17..34ac6e97f`), **no fix wave in between** — the tree
is byte-identical to the one rounds 1 and 2 read. Three rounds, one tree,
three axes. This is not a fix-wave audit.

**Readers.** Eight in-family finder dimensions (contracts, lifecycle,
arithmetic, error paths, guards, tests, design, upstream), blind to each other,
scoped to `prettyFormula.c` and `prettyLayout.c` internals plus the
three-acceptor seam; every finding then refuted independently under one
assigned lens (reachability, correctness, intent), default REFUTED, coverage
claims proven by mutation.

**Out-of-family accounting: `pending`.** No packet was built, no reply exists,
no `MODEL:` line can be quoted. The §1 banner states it; this section repeats
it because the exit criterion turns on it:

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Counts.** Twenty-three findings raised; **six refuted**; seventeen survived.
Of the seventeen, one is a re-derivation of a ruled item (`PP18-6`, dropped to
§6), and one could not have its reaching input constructed
(`PP18RR3-P1`). Filed: **fifteen CONFIRMED**, `PP18RR3-1`–`PP18RR3-15`, **one
PLAUSIBLE**, six design observations `PP18RR3-D1`–`D6`. One of the fifteen
(`PP18RR3-12`) is an explicit re-report of the 2026-08-27 audit's `A13`,
labelled as such.

**Independent agreement.** The DERIV double-parenthesis was reached by two
dimensions and previously by two rounds — four independent derivations of one
ruled item. The `fnPrettyHist` pager's unreachability was reached by four
dimensions, which is why three separate aspects of it (the code, the pins, the
doc) could be separated cleanly. `ppqFrameIntegral`/`ppqFrameDerivative` was
cleared as unreached by one dimension and measured as reachable by another,
which is the fan-out working as designed rather than a contradiction.

**Evidence discipline.** **Nine of the fifteen** are backed by a probe or
mutation applied, built through the package gate, observed in
`build.sim/meson-logs/testlog.txt`, and reverted. Two more rest on measured
build artifacts rather than a running probe (the font tables for `PP18RR3-1`,
the disassembled ARM frames for `PP18RR3-8`). The four static traces are
`PP18RR3-5` (the fallback width), `PP18RR3-9` (the containment census, proven
by brace-depth analysis and the item table), `PP18RR3-12` (re-report of a
previously-measured finding) and `PP18RR3-15` (documentary). No simulator ran;
no finding rests on an LCD photograph, and the two device-side claims say so
in their own text. Main tree clean at start and finish
(`git status --porcelain packages/` empty, no `AUDIT-PROBE` marker anywhere in
`packages/`).

**Exit criterion: NOT MET, and this round cannot advance it.** Fifteen new
CONFIRMED findings would reset the count on their own; separately, the round
had no out-of-family reader, and the criterion requires two consecutive clean
rounds with at least one of them out-of-family. The clock stands where
restarted round 1 left it. **Three consecutive rounds have now been run
in-family only.**

**Process items.**

1. **Stale worktrees, now requested by six consecutive rounds.** Every verifier
   worktree again spawned at `e21af8d28` — 111 commits behind, not an ancestor
   of the tip. Every one of them detected it and ran `git checkout 34ac6e97f`
   before reading, which is the only reason this round is usable. The
   `git merge-base --is-ancestor` guard in `audit-workflow.js` is **still
   absent**. This is the seventh consecutive round to report it and the first
   in which every verifier's evidence block opens with the same paragraph.
2. **Clean worktrees this round, unlike round 2.** No verifier reported a
   foreign edit, a shared `/tmp` clobber, or an inherited `build.sim` — one
   noted it had read some files through `/home/stan/c43` by mistake and
   `diff`ed all three against its own checkout to prove the reads were sound.
   Round 2's items 2 and 3 appear to have been absorbed as habits; the
   scratch-path and per-worktree-build fix is still worth making structural.
3. **The out-of-family pass was skipped silently for the third consecutive
   round**, which is the exact defect the tip commit's own handoff describes
   (`HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md`). The workflow still
   does not throw on a missing out-of-family argument and still does not
   enforce the §1 accounting. Compliance here is again a hand-written banner,
   which the handoff says is not enough.
4. **The governing gate still matters.** `./packages/forth-core/build-test.sh`
   refreshes only `packages/forth-core` and returns a meaningless green for a
   pretty-print mutation; `packages/pretty-print/build-test.sh --solo` is the
   only trustworthy runner. Two verifiers re-derived this independently, which
   means the trap is still not written down where a reader trips over it.
5. **Probe liveness checks paid off again.** Every mutation this round that
   produced a green was paired with a run proving the harness *could* go red
   (the FV10 ladder's run 3, V19's mutation B, the comma probe's dot control).
   Round 2 recommended making this part of the mutation protocol; three of this
   round's verdicts would have been wrong without it.
6. **Report filename truncated.** The requested filename is 613 bytes against a
   255-byte filesystem limit; this file's name is the requested one truncated
   after axis (a), with the date and the `-r3` suffix preserved.

**Round 4's axis, in priority order.**

1. **An out-of-family reader, over anything.** Three consecutive rounds have
   been one family's; two of them said so in a banner and moved on. The
   packet with the best return is the acceptance triangle — `ppqParse`,
   `ppEqBigopIntercept` and `ppvAstToNodes` are self-contained enough to
   inline, and the question ("do these three agree about what is drawable?")
   is answerable without the rest of the package.
2. **The fix wave for these three reports, when it lands.** Forty-two open
   findings across three rounds and no wave yet; the fix-regression statistic
   is seven-for-seven, and the shape to hunt is this report's own — a class
   fixed at the sites it was found and not at its siblings, with a correct
   comment at each fixed site.
3. **`ppqParse`'s interior for pin vacuity.** The tests dimension reached
   `prettyEquation.c:1-840` only through its pins, which is half of axis (a)
   left unaudited. Two of this round's parser findings (`-6`, `-11`) are
   one-conjunct alphabet defects that the EQ-series pins did not catch, which
   is the reason to look.
4. **`prettyTest.c` `EQ26`–`EQ35` and the `T`/`B` series**, the last unread
   regions of the harness — including `EQ29`, the pin the entire MVAR ruling
   rests on and which `PP18RR3-4` sits directly beside.
