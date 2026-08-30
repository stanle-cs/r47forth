# Audit — PP18 round 5 (restarted series), THE FIX-OF-THE-FIX WAVE, at `9de22ac7d`

Subject: `6e6c2c0ab..HEAD` on `pretty-print/stage-pp18`, tip `9de22ac7d`. **Seven
commits, all repair**: no feature, no rename, no docs commit, no workflow
commit. They close twelve of round 4's fourteen findings against the wave
before them, leaving `PP18RR4-4` (the `ITM_FILL` arm) and `PP18RR4-8` (the
`ppqShowRender` branch) open by the wave's own statement. Reading restricted to
the upstream-PR file set — `prettyVisual.c`, `prettyLayout.c`, `prettyFormula.c`,
`prettyEquation.c`, `prettyInternal.h`, `prettyPrint.h`, `prettyTest.c`,
`screen.c`.

The question the round was set was whether round 4's rate — eight of fourteen
findings in the wave's own work — repeated when the wave contains nothing *but*
fixes. **It did, and at a higher density: seven CONFIRMED defects against twelve
closures, six of them created by the repairs themselves and the seventh a
pre-existing behaviour the repairs declared closed.** Eleven of the twelve
closures I could not break; four independent dimensions re-derived the lift-latch
exception list against upstream's own dispatch and found it complete, the menu
guard survives every clearer in the tree, the `PPN_VAL2` reassembly is bounded
by construction, and the wrapped-x clamps are correct at every boundary I could
reach.

**The two that cost the owner something are both a rule that was told it was
finished.** `PP18RR4-3` moved the stacked-power bracket out of three call sites
and into `ppfBuildOp1`'s SQUARE/CUBE arm — the right instinct, one structural
question asked where it can be answered — and then wrote into
`prettyInternal.h` that "no caller carries that precondition". `ppfBuildOp2`'s
`ITM_YX` arm, twelve lines above in the same file, builds the same `PP_SUP` and
was not repaired, so `2 ENTER 3 yˣ 2 yˣ` draws `2³²` beside `= 64`. And the pin
the wave added for the one finding it could not reproduce, `V-FILL`, is green
whether that finding is fixed or not: it was measured both ways.

**Seven CONFIRMED, one PLAUSIBLE, five REFUTED.** Twenty-four findings were
raised across eight in-family dimensions; convergence was heavy (six of the
eight independently reached the `ITM_YX` arm), so they collapse to fourteen
distinct claims, all of which went to refutation. Five were killed there.
**Five of the seven CONFIRMED are backed by a mutation** applied inside an
isolated worktree, built through the package's own gate with presence verified
in `build.sim/custom_pkg_shadow/*`, observed in `build.sim/meson-logs/testlog.txt`
and reverted.

Nothing was fixed. The main tree is clean at start and finish and the gate is
green on it.

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).

### Subject

**Tip.** `9de22ac7d` ("pkg: ppSuppressBold's comment stops claiming the flag flip
is inert"). Range `6e6c2c0ab..HEAD`: **seven commits**, 13 files,
+491 / −75 — and unlike round 4's subject, none of that is documentation: the
whole delta is package source and one generated patch.

| commit | subject | closes |
|---|---|---|
| `969d0af78` | four of round 4's findings against the fix wave, including a regression I shipped | `PP18RR4-1`, `-9`, `-2`, `-14` |
| `bbf43534b` | the PP_RAD descent fix gets the assertion it shipped without | `PP18RR4-7` (adds M9) |
| `f69f2749a` | the menu guard stops reading a bit that routine bookkeeping clears | `PP18RR4-5` |
| `1d748e5d3` | two of the wave's own pins stop being satisfiable without testing anything | `PP18RR4-6` (M8), `-13` (T23c) |
| `266a60921` | the wrapped-x clamp screens each write by its own column, and covers both writes | `PP18RR4-11`, `-12` |
| `24cbf7590` | the stacked-power rule moves into the builder, so no caller carries it | `PP18RR4-3` (adds T24c) |
| `9de22ac7d` | ppSuppressBold's comment stops claiming the flag flip is inert | `PP18RR4-10` (comment-only) |

**Left open, by the wave's own statement** (`969d0af78`: *"Four are fixed here;
ten remain open"*, then six more closed by the later commits): `PP18RR4-4`, the
`ITM_FILL` arm, and `PP18RR4-8`, the `ppqShowRender` fallback. `prettyEquation.c`
is untouched in the range, so `-8` is not merely unfixed but unexamined by this
wave. `-4` is discussed in §3 — not re-reported as a defect, but its reaching
input, which round 4 recorded as needing re-derivation, was measured this round
and is stated there.

**In-scope file set and its delta over the range:**

| file | +/− | note |
|---|---|---|
| `prettyTest.c` | +155 / −10 | four new pins (M9, T24c, V-XEQ, V-FILL) plus M8/T23c repairs |
| `screen.c` (override) | +45 / −14 | two hunks rewritten: the glyph clamp and the menu guard |
| `prettyFormula.c` | +34 / −4 | the `PPN_VAL2` reader and the stacked-power kind test |
| `prettyVisual.c` | +18 / −5 | `case ITM_XEQ:` plus two comment blocks |
| `prettyLayout.c` | +7 / −2 | comment-only (`ppSuppressBold`) |
| `prettyInternal.h` | +5 / −2 | the builder banner's new precondition sentence |

`prettyEquation.c`, `prettyPrint.h` and every other package source are unchanged
in the range. The generated `files/` twins are byte-identical to the flat working
area for all six sources (checked; and the gate's own refresh left the tree
clean), so the build read what was read here.

**KNOWN, excluded from re-reporting** (verified still open, then fenced):
`PP18RR4-4`, `-8`, `-P1`, and every finding from `PP18RR3-*`, `PP18RR2-*`,
`PP18RR1-*`, `PP18R4-*` and the pre-restart `PP18-*` series with their rulings.
Where a finding below is adjacent to a known one it says so.

**Numbering.** `PP18RR5-1`–`PP18RR5-7`, plausible `PP18RR5-P1`, design
observations `PP18RR5-D1`–`D6`. `grep -rn PP18RR5` over the repository returned
nothing before this file was written.

### Coverage (union across the eight in-family dimensions)

**Read in full by three or more dimensions:** the entire `git log -p
6e6c2c0ab..HEAD`, all seven messages included; `prettyFormula.c`
(`ppfWrapIf`/`ppfParen`/`ppfRun`, all of `ppfBuildOp1` and `ppfBuildOp2`,
`ppfFromCaptureNode` every arm, `ppfBuildEntry`'s whole token loop,
`ppfStageValFields`/`ppfFormatStaged`, `ppfBuildCurrent`/`ppfBuildRow`);
`prettyVisual.c` (`ppvLiveStackSlots`, `ppvPush`/`ppvPushLifting`/`ppvPop`,
`ppvOp1`/`ppvOp2`, `ppvBody`, `ppvRefillFromT`, `ppvLiftNeutral`, `ppvStep` and
**every arm** of `ppvStepArm`, `ppvWalk`, `ppvAstToNodes`, `ppvAstPrec`, both
paint surfaces, `fnPrettyVisual`); `prettyLayout.c` (`ppSuppressBold`/
`ppRestoreBold` and both wrappers, the `PP_SUP` and `PP_RAD` measure and paint
arms, `ppFillVal`, `ppShowRun`); `prettyInternal.h`; `screen.c`'s two changed
hunks with their enclosing upstream functions (`showGlyphCode` 1159–1320,
`_refreshNormalScreen` 5878–6085 through `RETURN_NORMAL`).

**Read for the pins:** all four new pins and both repaired ones (M9, T24c,
V-XEQ, V-FILL; M8, T23c) plus the pins they lean on or collide with — M2, M5,
FV8, FV9, P1–P6, T22b, T23b, T24b, V-MODE, V-MODE4, V36b, V38, V51, V72/V75/V76 —
and the harness underneath them (`ppvTestExpect`, `ppfTestExpect`/`ppfTestSigNode`,
`ppcTestSlotRaw`, `ppcTestWriteAndLoadPgm`, `ppTestFail`).

**Out of the PR file set, read where a call path led into them:**
`prettyCapture.c` (`ppcClassify`, the `PPC_DY`/`PPC_MO` arms,
`ppcValLeafFromRegister`, `ppcRclLeaf`, `ppcDeepCopy`, the `PPN_VAL` serializer,
`ppcNodeAt`, every `ppcSlot[]` write), `prettyValue.c` (the SCI/exponent
classifiers and the PSHOW protocol lines), `prettyEquation.c` (`ppqFactor`'s
`^` arm only).

**Upstream verified by execution path, not assumed:** `items.c` SLS/PTP rows for
every op the walker dispatches and the SLS epilogue in `reallyRunFunction`;
`programming/lblGtoXeq.c` `executeOneStep` (including the `PTP_DECLARE_LABEL`
and `PTP_REM` early returns, which is what makes LBL and REM lift-neutral in
effect despite being `SLS_ENABLED`), `_putLiteral`, `fnExecute` under
`PGM_RUNNING`; `programming/manage.c` `findNamedLabel`/`findNamedLabelWithDuplicate`;
`flags.c` `refreshStateFlags[]` and `systemFlagAction`; `radioButtonCatalog.c`
`fnRefreshState`; `softmenus.c` `popSoftmenu`/`showSoftmenu`/
`showSoftmenuCurrentPart`; `keyboard.c` `commonShiftProcessing`, `btnPressed`,
the TAM routes and `processAimInput`; `bufferize.c` `addItemToBuffer`;
`screen.c` `RETURN_NORMAL`, `refreshScreen`, `closeShowMenu`; `statusBar.c`
`refreshStatusBar`'s own calcMode self-guard; `display.c`
`exponentToDisplayString`/`supNumberToDisplayString`; `defines.h` `getStackTop`,
the `SCRUPD_*` bits, `SHOWMODE`; `config.c`'s reset table (`FLAG_SSIZE8` is set
in the Reset, JM, RJ and C47 columns and cleared only for HP35); `testSuite.c`'s
`In:` parameter handling; both simulator `lcd_fill_rect` HALs.

**Measured artifacts, not read:** the `numericFont`, `standardFont` and
`tinyFont` glyph tables parsed out of
`build.sim/src/ttf2RasterFonts/rasterFontsData.c` for the `idxDesc` closed form
that refuted the M9 finding.

### Not reached, and it matters where

- **No simulator ran and no LCD photograph backs any finding.** Every picture
  claim is derived from `ppMeasure`/`ppPaint` arithmetic or from the harness's
  own signature strings. `PP18RR5-3` in particular is a layout argument, not a
  rendered capture.
- **`prettyCapture.c` is still the largest unaudited surface in the stage.** It
  is outside the PR file set for the second consecutive round; it was read only
  where a trace from an in-scope site entered it. `PP18RR5-3`'s producer half
  lives there.
- **`prettyTest.c`'s 5,852 lines were sampled, not swept.** The six new or
  changed pins and their neighbours were read; the B-series, EQ26–EQ35 and the
  P-series past P6 were not. Three of this round's seven findings are pin
  defects, all in the sampled set.
- **The `SSIZE8` sweep was spot-checked, not completed.** Flipping the battery's
  own `SS=4` to `SS=8` reddens V-FILL *and* five EQ pins (EQ26, EQ27, EQ28,
  EQ33, EQ34) because A..D become stack registers under the eight-level stack.
  Only the V-FILL failure was traced; the EQ failures are expected fixture
  breakage and were not investigated, so this round cannot say whether the
  SSIZE8 branch holds for the rest of the battery.
- **`design-audit.sh` is forth-core's.** There is still no pretty-print
  equivalent, so no override-budget check ran; §2's churn scan is the substitute.
- **Flash was not measured.** No commit in the range records a `make dmcp5r47`
  delta and this round did not build the device target to supply one.

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Fifth consecutive round with no out-of-family reader.** `c719b1a9c` — the
commit added one wave ago to stop this happening silently — is doing its job:
the skip is a banner, not a footnote. It still cannot supply a reader.

---

## 2. Mechanical results

**Gate: GREEN.** `./packages/pretty-print/build-test.sh --solo`, run by this
synthesis pass on the main tree at `9de22ac7d`: `testSuite EXIT STATUS: 0`,
`1/1 testSuite OK 170.53s`, `PRETTY-PRINT GATE GREEN`,
`grep -c "test FAIL" build.sim/meson-logs/testlog.txt` = **0**. Seven verifier
worktrees ran the same gate independently (170–195 s) and each re-ran it after
reverting its probe. `git status --porcelain` shows only the untracked round-4
report before and after; no `AUDIT-PROBE` marker survives anywhere in
`packages/`.

**Warnings: two, both upstream's own text.**
`build.sim/custom_pkg_shadow/testSuite/testSuite.c:5498` and `:5506`,
`'%s' directive writing up to 1999 bytes into a region of size 404`
`[-Wformat-overflow=]`. The lines are byte-identical to
`src/testSuite/testSuite.c`; the package's override does not touch them. Outside
the subject range and not this wave's.

**Upstream churn: halved, not closed.** `patch_churn_scan.py` over all thirteen
pretty-print patches exits 1 with **5** mechanical findings at `9de22ac7d`,
against **9** at `6e6c2c0ab` (reconstructed from git into a scratch directory and
scanned the same way).

| `010-screen.c.patch` | at `6e6c2c0ab` | at `9de22ac7d` |
|---|---|---|
| adds / dels / hunks | 36 / 11 / 4 | 56 / 8 / 5 |
| `[WS-ONLY]` findings | 8 | 4 |

The four survivors are `setPixel(x1, y1);`, `setPixel(x1+1, y1);`, `}` and
`setPixel(x1, y2);` — upstream lines whose only change is indentation, because
`266a60921` re-indented them into an `if` rather than using the no-reindent wrap
this package's own minimality review prescribes. The fifth finding is the
pre-existing `solver/equation.c` one that review catalogued. That is
`PP18RR5-7`, and the excursion 1 → 9 → 5 appears in no commit message: `git log
6e6c2c0ab..HEAD` mentions churn, minimality, indentation or the scanner exactly
zero times.

**The design record did not move, for the second consecutive wave.**
`git log 6e6c2c0ab..HEAD -- design-docs/` returns **nothing**. Twelve findings
closed, a new structural rule installed in `prettyInternal.h`, and a new
membership rule installed in `prettyVisual.c` — all of it lives only in commit
messages and code comments. `grep -rn "saturat\|SSIZE\|replicat\|PPN_VAL2"
design-docs/pretty-print/` still returns nothing, which is round 4's `D4`
unchanged. `DESIGN.md:571-574` now actively disagrees with the code it describes
(§6, doc drift).

**Flash and RAM accounting: zero of seven commits.** No `make dmcp5r47` delta
anywhere in the range, against CLAUDE.md's standing rule that the measured delta
is recorded in the stage commit. Round 4 reported one commit in three; this wave
reports none in seven. In fairness the whole delta is small, and `24cbf7590`
adds one `ppNodeAt` and one branch — but "small" is a claim, and the rule exists
because the claim is cheap to check and nobody checked it.

**The round-4 report is untracked.** `git log --all --
'design-docs/forth-core/AUDIT_PP18-round-4*'` is empty: the document that
defines `PP18RR4-1`…`-14` has never been committed. Seven commits cite those
numbers as their justification, and a reader with a clean clone cannot resolve
one of them.

**Probes and mutations this round** — all applied in isolated worktrees, built
through the real refresh, observed in `testlog.txt`, and reverted.

| probe / mutation | observed result | finding |
|---|---|---|
| `2 ENTER 3 [ITM_YX] 2 [ITM_YX]` → `ppfBuildCurrent`, expect `S(P(S(2\|3))\|2)` | `FAIL … actual 'S(S(2\|3)\|2)'` | **`PP18RR5-1`** |
| `3 [ITM_SQUARE] 2 [ITM_YX]`, expect `S(P(S(3\|2))\|2)` | `FAIL … actual 'S(S(3\|2)\|2)'` — with the row above, the only two failures in the run | **`PP18RR5-1`** |
| `setSystemFlag(FLAG_SSIZE8)` around **only** the V-FILL expectation, plus a printf of the ambient | `ambient FLAG_SSIZE8 at V-FILL = 0`; `FAIL: V-FILL saturates, so T replicates (=63) (declined D10 at step 10 …)` | **`PP18RR5-2`**, and `PP18RR4-4`'s reaching input |
| `PP18RR4-4`'s own fix applied (`stk->depth = ppvLiveStackSlots(); stk->saturated = true;`) | gate **GREEN**, V-FILL emits the identical string | **`PP18RR5-2`** — the pin cannot distinguish fixed from broken |
| the battery's own `SS=4` → `SS=8` at `pretty_print.txt:8` | V-FILL declines D10; EQ26/27/28/33/34 also break (A..D become stack registers) | corroborates **`PP18RR5-2`** |
| `setSystemFlag(FLAG_ERPN)` around only the V-XEQ expectation | `FAIL: V-XEQ … (expected '2+(4+5)', actual '4+(4+5)')` | **`PP18RR5-6`** |
| V-XEQ's fixture renamed `VZA/VZB` → `VXA/VXB` — the edit its own comment instructs | gate **RED**, sole failure `V72 the lift latch does not survive XEQ (expected 'a×(a+b)', actual '2+(4+5)')` | **`PP18RR5-5`** |
| the six declaration items deleted from `ppvLiftNeutral`, left in `ppvStepArm` | gate **GREEN**, `1/1 testSuite OK 195.14s` | **`PP18RR5-4`** |
| `prettyLayout.c:283-286` (the whole `idxDesc` clamp) → `(void)idxDesc;` | gate **RED**, sole failure `M9 the box does not cover the index's ink bottom (expected 6, actual 0)` | control — **REFUTES** the M9 vacuity finding and confirms `bbf43534b` |

Two of those are the shape this project's protocol keeps asking for and keeps
not getting: a mutation that applies **the fix a pin is named after** and watches
the pin stay green, and a mutation that deletes **half a list** and watches the
whole suite stay green. Both were green. That is `PP18RR5-D3`.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. A silently wrong drawing four
keystrokes away outranks a green pin that certifies an open defect as covered,
which outranks a wrong drawing in a display mode fewer owners sit in, which
outranks a rule that the next edit will break with the gate still green, which
outranks a comment that misdirects, which outranks a pin locked to one mode,
which outranks four upstream lines that a rebase will conflict on for nothing.

There are no crashes and no data loss in this range. The worst thing here draws
the wrong mathematics.

Where the refutation pass corrected the finder, the correction is stated
**inside** the finding. Each carries `file:line`, what breaks, the concrete
reaching input, the violated contract quoted, the bug class, and the class-level
test. **No patches.**

---

### PP18RR5-1 — the stacked-power rule moved into the builder and stopped at `ppfBuildOp1`: `ppfBuildOp2`'s `ITM_YX` arm still leaves a `PP_SUP` base unbracketed

`packages/pretty-print/prettyFormula.c:110`, against the repaired arm at
`:234-252`. **Introduced by `24cbf7590` (`PP18RR4-3`)** — the defect in the
`ITM_YX` arm predates the branch; what this wave contributed is a header
contract stating that it cannot happen.

**What breaks.** `ppfBuildOp1`'s SQUARE/CUBE arm now asks the base node's kind
and calls `ppfParen` itself when it is a `PP_SUP`. `ppfBuildOp2`'s `ITM_YX` arm
builds the identical node kind twelve lines above and still asks only
`ppfWrapIf(a, aPrec, PPF_PREC_ATOM, ctxFont)`. Every builder leaves
`*outPrec = PPF_PREC_ATOM`, and `ppfWrapIf` brackets iff `prec < minPrec`, so
`ATOM < ATOM` is false and nothing is inserted. The result is
`SUP(SUP(2,3),2)` with no `PP_PAREN` anywhere in the tree.

Nothing downstream rescues it, and the layout makes it invisible:
`ppMeasure`'s `PP_SUP` arm (`prettyLayout.c:405-424`) sets `base.relBase = 0`
and `exp.relBase = -m->supDrop` at **both** nesting levels while `ppPaint`
accumulates baselines additively (`:779-784`), so the inner and outer exponents
land at exactly the same height. Unlike the CUBE case there is not even a font
difference to separate them — the walker passes `ctxFont` to both `OP2`
children.

**Reaching input**, built as a fixture and measured:

```
2 ENTER 3 [yˣ] 2 [yˣ]      → X = 64,   drawn 2³²
3 [x²] 2 [yˣ]              → X = 81,   drawn 3²²
```

`ppcClassify(ITM_YX)` returns `PPC_DY` (`prettyCapture.c:540`); the DY arm
(`:918-931`) sets `child[0] = ppcSlot[1]`, so a second `yˣ` nests one `PPN_OP2`
inside another; `ppfFromCaptureNode`'s `PPN_OP2` arm (`prettyFormula.c:452-464`)
hands that to `ppfBuildOp2` with `aPrec = PPF_PREC_ATOM`. **Both capture
surfaces reach the same unguarded builder** — the live T-line/PHIST row 0 via
that arm, and the filed history rows via `PPT_TKO2` (`prettyFormula.c:583-594`),
which passes the two stack precedences straight through and adds no paren of its
own. `ITM_YX` is reachable independently of the `yˣ` key: `softmenus.c:80`
(menu_EXP) and `:974` (menu_HOME) both expose it, so the R47 key remap does not
close the path.

**Evidence.** Two probes driven through the real `runFunction` dispatch by the
same harness T24c uses, in `build.sim/meson-logs/testlog.txt:433` and `:436`:

```
prettyPrint test FAIL: AUDIT-PROBE R5 YX stacked power brackets its base
  (expected 'S(P(S(2|3))|2)', actual 'S(S(2|3)|2)')
prettyPrint test FAIL: AUDIT-PROBE R5b YX over SQUARE brackets its base
  (expected 'S(P(S(3|2))|2)', actual 'S(S(3|2)|2)')
```

These were the **only** two prettyPrint failures in the run. T24c's `ITM_CUBE`
pin stayed green throughout, which is the "the pin agrees with the bug" shape:
the rule is stated generally and pinned for one arm.

**Violated.** `prettyInternal.h:120-123`, written by this wave: *"The one shape
that level would have covered — a power whose base is itself a power — is
handled inside the SQUARE/CUBE arm by inspecting the base node's kind, so no
caller carries that precondition."* It is handled in one of the two arms that
build a `PP_SUP`. And `prettyTest.c:1394-1397`, also written by this wave:
*"a stacked power must bracket its base on the CAPTURE surfaces too … an
unbracketed 3 cubed cubed draws flat and reads as 3^33"* — stated as a rule
over the class, pinned for `ITM_CUBE` alone. And `24cbf7590`'s own message:
*"one structural question asked at the one place that can answer it"* — there
are two places.

**Bug class.** A rule relocated to an owner but not to its scope: the fix is
correct for the shape it was derived from and its written contract is false for
the shape it was not. Round 4's `D1` in its narrowest form, and the same class
`DESIGN-HISTORY.md:143-146` records as `PP18-4` — *"a guard that enumerated its
examples instead of its class"*.

**Class-level test.** Enumerate the producers of `PP_SUP` **from the code**
(`ppfBuildOp1` SQUARE/CUBE, `ppfBuildOp2` `ITM_YX`, and `ppqFactor`'s `^` arm)
and drive one table over three columns — `ppvTestBuildNodes`, `ppfBuildCurrent`,
`ppfBuildEntry` — asserting that every producer with a `PP_SUP` base emits a
`PP_PAREN` and that all three surfaces agree. The four rows are `x² x²`,
`x³ x³`, `yˣ` over `yˣ` and `yˣ` over `x²`; three of them are red today. This is
also the differential oracle round 4's third round-5 axis asked for, and it
subsumes `PP18RR5-3`'s row.

---

### PP18RR5-2 — `V-FILL`, the pin the wave added for the finding it could not reproduce, is green whether that finding is fixed or not, and its verdict is decided by an ambient flag it does not own

`packages/pretty-print/prettyTest.c:4176-4192` (the pin), `:4191` (the
assertion). **Added by `969d0af78`.** Three of the eight dimensions reached this
independently.

**What breaks.** The pin's comment states the mechanism of the still-open
`PP18RR4-4` — *"FILL writes the slots directly instead of going through
ppvPush, so the saturation latch never arms and T replication never runs: the
walk underflows and declines a program the machine runs"* — and then asserts the
fully replicated drawing, which is what the walker produces when that mechanism
does **not** bite. It passes because `packages/pretty-print/testSuite/tests/
pretty_print.txt:8` opens the whole battery with `In: … SS=4 WS=64`, which
`testSuite.c:3403-3410` turns into `clearSystemFlag(FLAG_SSIZE8)` for every
`Func:` block in the file, and `ppvTestExpect` has no `SSIZE` handling of its
own.

Under `SS=4` the pin passes by the opposite mechanism to the one it names:
`ITM_FILL` sets `depth = PPV_STACK_SLOTS` = 8, four more than
`ppvLiveStackSlots()` returns, so the **first** `ADD`'s `ppvPush` takes its
`depth >= slots` drop-the-bottom branch (`prettyVisual.c:202-207`), renormalises
to 4 and sets `saturated = true` as a side effect. FILL never saturates
anything; the over-deep write does.

**Reaching input for the pin's failure.** `setSystemFlag(FLAG_SSIZE8)` — that
is, the configuration the calculator boots into (`config.c:271`, the Reset, JM,
RJ and C47 columns all set `FLAG_SSIZE8`). Measured:

```
AUDIT-PROBE R5: ambient FLAG_SSIZE8 at V-FILL = 0
prettyPrint test FAIL: V-FILL saturates, so T replicates (=63)
  (declined D10 at step 10, expected '7+(7+(7+(7+(7+(7+(7+(7+7)))))))')
```

`D10` is `PPV_D_UNDERFLOW` (`prettyVisual.c:63-68`, tenth enumerator); step 10 is
the eighth `ADD`. The printf firing proves the probe reached the built artifact
and prints the ambient empirically.

**The second measurement is the one that matters.** With the probe reverted and
`PP18RR4-4`'s own repair applied to the `ITM_FILL` arm instead
(`depth = ppvLiveStackSlots(); saturated = true;`), the gate is **GREEN** and
V-FILL emits the identical string. **The pin produces the same output whether
the defect it is named after is present or absent.** It is not merely fragile;
it has no power against its own class.

**Consequence.** The record now carries a green pin captioned with
`PP18RR4-4`'s mechanism, which reads as coverage for a finding that is open, in
a wave whose subject is closing findings. It is also a latent false-red: a
reordered fixture, or any future pin that leaves `FLAG_SSIZE8` set, turns it red
for a reason unrelated to any regression. V-MODE4, thirty lines below, saves,
sets and restores the same flag on both of its arms — the contrast is inside the
same commit's neighbourhood.

**Violated.** The pin's own caption, above. The bug-fix class-test rule (Stan,
2026-08-04): every fix lands with a reproducer, a named bug class and a
class-level test — this pin is attached to a finding with no fix at all. And
`1d748e5d3`'s own statement of the class, written two commits later in this same
range: *"a pin whose assertion is satisfied by a state other than the one it
names … the same class the wave has now hit five times."* Six.

**Correction to the finders.** Two dimensions wrote that V-FILL "asserts the
opposite of the property its caption states". Under `SS=4` the asserted drawing
is genuinely correct for the hardware too, so the pin is mis-captioned rather
than false. The consequence is unchanged.

**Bug class.** A pin written from the configuration the author could reach, for
a defect that only exists in the configuration he could not — the same-level
coverage lesson, applied to a machine mode instead of an expression shape.

**Class-level test.** Round 4 already wrote it: *"a behavioural pin: `2 FILL`
followed by more consuming steps than the stack is deep must draw, not decline,
under BOTH `SSIZE` settings."* Generalise it once and the class is enumerable —
every V fixture whose walk can saturate should run twice, with the flag saved,
set and restored, exactly as V-MODE4 does. The harness makes that cheap and
`ppvTestExpect` is the one place to put it.

---

### `PP18RR4-4`, closed on its reaching input — recorded here, NOT re-reported as a finding

`packages/pretty-print/prettyVisual.c:927`. Round 4 filed the `ITM_FILL` arm and
recorded that its reaching input still needed re-deriving. It is `FLAG_SSIZE8`,
and it was measured this round three separate ways: flipping the flag around the
V-FILL expectation, flipping the battery's own `SS=4` to `SS=8`, and applying
the fix and watching the decline disappear.

`ITM_FILL` (`:918-928`) writes `stk->ast[0..7]` and `stk->depth =
PPV_STACK_SLOTS` directly. It is the only arm that fills the stack without
calling `ppvPush`, and `ppvPush:212` is the only place `stk->saturated` is ever
set true (`:433` and `:1217` set it false at frame init). Under `SSIZE8`,
`slots = 8`: FILL leaves `depth = 8` with `saturated` false, `ppvRefillFromT`
(`:844-847`) returns at its head forever, each `ADD` nets −1, and the eighth
`ADD`'s second `ppvPop` hits `depth == 0` → `PPV_D_UNDERFLOW`. The Z/T window
prints "cannot be drawn (D10)" for a program `XEQ` runs to 63.

It violates `prettyVisual.c:185-188` — *"The array is always eight wide; only
the effective depth follows the flag"* — and the constant comment this wave's
predecessor corrected at `:49`, *"array width; the LIVE depth follows
SSIZE4/8"*. `ppvBody:437-441` shows the intended shape: it seeds every slot
through `ppvPush`, so it arms correctly at either stack size.

Not re-reported because it is a known open finding. Recorded because the report
that owns it says the input was missing, and it no longer is.

---

### PP18RR5-3 — the base-kind test cannot see a power that the value formatter already spelled into glyphs, so a squared scientific-notation value draws its two exponents as one

`packages/pretty-print/prettyFormula.c:244` (the kind test), against the value
leaf built at `:414-441`. **The behaviour predates the wave; what this wave
contributed is the claim at `prettyInternal.h:120-123` that it cannot happen.**

**What breaks.** The repaired arm asks `an->kind == PP_SUP`. A node's kind can
only answer for powers *this layer* built. `ppfFromCaptureNode`'s `PPN_VAL` arm
formats a register through `ppfStageValFields` → `ppfFormatStaged` →
`real34ToDisplayString` and returns `ppfRun(ppfValBuf, ctxFont)` — one flat
`PP_RUN` whose text already ends in `PRODUCT_SIGN + STD_SUB_10 +` superscript
digits, because that is how `exponentToDisplayString` (`src/c47/display.c:127-146`)
spells an exponent. The kind is `PP_RUN`, so line 244 takes the `ppfWrapIf`
branch, `ATOM < ATOM` is false, and no paren is inserted.
`ppMeasure`'s `PP_SUP` arm then places the outer exponent at
`relX = base.width + 1, relBase = -supDrop` — raised, and immediately right of
the run's own raised exponent digits.

**Reaching input.** With X holding a value the formatter renders in scientific
form — any large-exponent result such as `1×10⁵⁰`, or any value while the
display format is SCI — press `[x²]`, then `PHIST` or read the T-line formula.
`RCL A` on a lettered register reaches the same leaf via `ppcRclLeaf`
(`prettyCapture.c:268-277`). The formula row draws `1×₁₀⁵⁰²` where it means
`(1×10⁵⁰)²`: the exponent reads as 502.

**Two producers, not one.** `prettyFormula.c:524-545` (the `PPT_TKV`/`PPT_TKRES`
arms of the filed-history token decoder) pushes the formatted value as a bare
run with `stackPrec = PPF_PREC_ATOM` as well, so PHIST's history rows carry the
same exposure, not just the live row.

**Violated.** The same sentence as `PP18RR5-1` — `prettyInternal.h:120-123`,
*"a power whose base is itself a power … so no caller carries that
precondition"* — and the arm's own comment, *"Ask the node's KIND rather than
adding a POW precedence level"*. Asking the kind is the right call for the
structural case and is structurally blind to this one, and nothing says so.

**What was searched and found absent.** `DESIGN.md`, `DESIGN-HISTORY.md`,
`TESTING.md`, the arm's comment and `24cbf7590`'s message all treat the class as
CLOSED. The one nearby statement about exponent tails inside a run —
`prettyEquation.c:77`, *"copied verbatim (the glyphs already render right in a
run)"* — rules on drawing the number **alone** in the EQN grammar, in a
different producer, not on composing that run under an outer `PP_SUP`. The
closest documented precedent cuts the other way: `DESIGN-HISTORY.md:144` records
`PP18-4`, the same class (a construct reporting ATOM so a square drew its
exponent on the body), as a defect that was fixed.

**Confidence: medium, and here is exactly where.** The chain from the value leaf
to an unbracketed `PP_SUP` over a `PP_RUN` is read from the code and is not in
doubt. What was **not** measured is the picture: no probe rendered a
scientific-form value under `x²` and counted pixels, so the claim that the two
exponent runs read as one number rests on `ppMeasure`'s placement arithmetic
rather than on ink. That is the single check that would promote this to the same
footing as `PP18RR5-1`.

**Bug class.** A structural test over a representation that is sometimes text: a
guard that inspects node kinds cannot see semantics the layer below has already
flattened into glyphs. Related to, but not the same as, `PP18RR5-1` — that one
is a missed site, this one is a missed representation.

**Class-level test.** Enumerate the *value* shapes a leaf can format into and
assert the drawn width of `x²` over each: an integer, a plain real, a real in
SCI/ENG display mode, a value past `LIMITEXP`, and a complex. The property to
assert is not a string but a box: the `PP_SUP`'s base must contain the run's
whole ink, so a base whose text ends in superscript glyphs must be parenthesised
or the exponent must be separated. Round 4's `-2` table over register types is
the same table with a different column, so the two are one fixture.

---

### PP18RR5-4 — `ppvLiftNeutral`'s replacement membership rule still does not select its own list, and its six declaration items are duplicated verbatim into the dispatch switch with nothing linking them

`packages/pretty-print/prettyVisual.c:809` (the rule), `:821-822` and `:871-872`
(the two copies of the list). **Introduced by `969d0af78` (`PP18RR4-9`).**

**What breaks — nothing today, and that is the finding.** Round 4 found the old
membership rule ("upstream marks `SLS_UNCHANGED`") false for two of six members
and supplied a correct one. The wave replaced it with a different rule —
*"Membership is 'the arm decides the latch'"* — which is also not the test that
selects this list. Four arms decide the latch and are not members:
`ITM_LITERAL` (`:942`, whose own comment is *"ppvPushLifting owns the latch"*),
`ITM_RCL` (via the same helper), `ITM_PGMINT` (`:992`) and `ITM_PGMDRV`
(`:1016`), the last two with an explicit `stk->liftDisabled = false;`. Six
members do not decide it: `ITM_NULL`/`LBL`/`MVAR`/`REM`/`PAUSE`/`SNAP` return
without touching it.

**The refutation pass dented the rule clause and confirmed the rest.** Read
together with the headline at `:801` — *"Items whose arm OWNS the lift latch, so
the epilogue must not touch it"* — the exclusions are defensible: those four
arms leave the latch in exactly the state the epilogue would assign
(`ppvPushLifting` ends with `liftDisabled = false` on both branches, `:226` and
`:230`), so the epilogue is a no-op for them. And round 4's proposed replacement
("execution leaves `FLAG_ASLIFT` unchanged") is itself false for ENTER and XEQ,
so the wave had cause not to take it. What is not defensible is that the file
now uses the same word, *owns*, at `:801` to justify membership and at `:942` to
justify non-membership, for the same latch — the next maintainer cannot
re-derive the list from either sentence.

**The duplication was measured, not asserted.** Deleting
`case ITM_NULL: case ITM_LBL: case ITM_MVAR: case ITM_REM: case ITM_PAUSE:
case ITM_SNAP:` from `ppvLiftNeutral` while leaving them in the dispatch arm —
exactly the "forgot the other switch" state a next edit produces — compiles into
the shadow and the whole pretty-print gate stays **GREEN** (`1/1 testSuite OK
195.14s`). No pin distinguishes the correct list from the half list.

**Reaching input for the next edit's defect** (constructed against the code, not
observed): `LBL "VXX"; 2; ENTER; PAUSE; 5; ADD; ADD`. Correct — ENTER arms the
latch (`:886`, `FLAG_ERPN` clear), PAUSE is a member so it survives, the 5
overwrites, `[7]` at depth 1, the second ADD underflows with `saturated` false,
so the walk declines. With PAUSE dropped from the list: the epilogue clears the
latch, the 5 pushes, and the walker draws `2+(2+5)` for a program the machine
cannot compute. That is `PP18RR4-1`'s consequence — a picture for a program that
does something else — reached by an ordinary maintenance edit.

**Why no pin catches it.** The lift-latch fixtures are `pgmLFT`
(`prettyTest.c:4536`, `5 ENTER 3 +`), `pgmLF2` (V38, a decline) and `VMEN`
(`:4112`). None places a declaration item between the ENTER and the lifting
read, so none exercises latch survival across the six-item group at all.

**Violated.** `PP18RR4-9` asked for a rule from which the list can be
re-derived; the wave supplied a rule that selects a different set and left two
comments in one file contradicting each other on the test. And the wave's own
banner at `:803-806`, *"the exceptions are this list rather than a property of
control flow"* — the exceptions are **two** lists, hand-kept in sync.

**Bug class.** A membership list with two copies and no oracle: the correctness
of a fix stored as data that nothing checks. `PP18RR4-1` — the calculator
drawing `b+(b+c)` for a program that computes `a+(b+c)` — was this list being
wrong once already.

**Class-level test.** Not a fixture: a structural assertion. Enumerate the
`ppvStepArm` cases that return without touching `liftDisabled` and assert the
set equals `ppvLiftNeutral`'s membership, in the test driver, over the item
table — the same shape as forth-core's structural ownership test. Failing that,
one behavioural pin per member (`2 ENTER <member> 5 + +` must decline) makes the
deletion above go red; six lines, and the mutation above proves all six are dark
today.

---

### PP18RR5-5 — V-XEQ's comment documents its fixture under labels `VXA`/`VXB`, which are V72's programs, not its own

`packages/pretty-print/prettyTest.c:4152`. **Introduced by `969d0af78`.**

**What breaks.** The comment reads *"LBL VXA: 1 2 XEQ VXB 5 + +      LBL VXB: 4
ENTER"*. The block it heads encodes `VZB` and `VZA` (`:4157`, `:4163`, `:4166`)
and asserts `ppvTestExpect(…, "VZA", "2+(4+5)")`. `VXA` and `VXB` both exist and
belong elsewhere: `:4870` defines `LBL VXB` as `{RCL b, ADD}` and `:4876`
defines `LBL VXA` as `{RCL a, ENTER, XEQ VXB, MULT}`, asserted at `:5130` as
V72 (`want = a×(a+b)`) and reused by V75/V76. `git show 969d0af78` shows the
comment line landing in the same hunk as the `'V','Z','B'` bytes — it shipped
pre-rename and was never updated.

**Reaching input.** A maintainer edits the fixture to match its own comment.
Measured: renaming V-XEQ's three label triples to `VXA`/`VXB` turns the gate
**RED** with exactly one failure, and it is not in V-XEQ:

```
prettyPrint test FAIL: V72 the lift latch does not survive XEQ
  (expected 'a×(a+b)', actual '2+(4+5)')
```

`ppvTestExpect` resolves through `findNamedLabel(label, GLOBAL_LABELS)`
(`prettyTest.c:3878`), which returns the **first** name match in label order
(`programming/manage.c:1881-1887`); `ppcTestWriteAndLoadPgm` appends, V-XEQ's
programs load ~700 lines earlier than V72's, and nothing clears program memory
between them. So V72 silently resolves to V-XEQ's program and fails as a
lift-latch semantic regression, ~960 lines from the edit that caused it.

**This exact failure already happened once, in this range.** `969d0af78`'s own
message: *"V-XEQ's first fixture took the label VXA, which V72 already owns, so
V72 resolved to my program and failed: label collision reading as a semantic
conflict."* The fixture was renamed; the comment was not.

**Violated.** The standard `9de22ac7d` states in this same range: *"a comment
that tells the next reader a call is inert when it is not is worse than no
comment, and this one would have sent them looking in the wrong place."* And
`TESTING.md:542-544`, which requires the V fixtures to carry *"package-local
label names"*, and `TESTING.md:642-647`, which records this pathology biting
before: labels that *"collide by design"* wiped what `programs.txt` expects and
*"six upstream cases failed 300 lines away from the cause, with nothing in the
message pointing back"*.

**Correction to the finder.** "Three thousand lines away" is ~960. Nothing else
changes; the misdirection works in both directions, since anyone grepping `VXA`
out of this comment lands in a fixture whose steps look nothing like the ones
the comment describes.

**Bug class.** A fixture comment that names live symbols belonging to another
pin — a documentation defect whose cost is a real, misattributed test failure.

**Class-level test.** Mechanical and cheap: a driver-side check that every label
triple written by `ppcTestWriteAndLoadPgm` is unique across `prettyTest.c`, and
that a collision fails at load time with the label named. That turns "V72 broke"
into "VXA is defined twice", which is the failure this class actually is. The
comment itself needs no test, only correction — but the uniqueness check is what
stops the next occurrence from costing a debugging session.

---

### PP18RR5-6 — V-XEQ asserts its picture in classic RPN only; under eRPN the same fixture draws `4+(4+5)` and the pin reddens

`packages/pretty-print/prettyTest.c:4173`. **Introduced by `969d0af78`**, in the
same commit as V-FILL and with the same shape.

**What breaks.** `ITM_ENTER`'s arm reads
`stk->liftDisabled = !getSystemFlag(FLAG_ERPN)` (`prettyVisual.c:886`). Under
eRPN a running program's ENTER dups **and** leaves the latch set, so the caller's
`5` lifts instead of overwriting the callee's dup and the picture is
`4+(4+5)`, not the asserted `2+(4+5)`. Measured: wrapping only the V-XEQ
expectation in `setSystemFlag(FLAG_ERPN)` gives

```
prettyPrint test FAIL: V-XEQ callee ENTER survives the return (=11)
  (expected '2+(4+5)', actual '4+(4+5)')
```

The ambient is forced the other way by `ppcTestReset` (`prettyTest.c:872-885`,
`clearSystemFlag(FLAG_ERPN)`), while the machine's own reset table sets
`FLAG_ERPN` (`config.c`, Reset column).

**This is a pin defect only, and that is the ranking.** Under eRPN the drawing
the walker produces is **correct** — the machine returns 13 for that program in
that mode — so nothing an owner sees is wrong. What is wrong is that the pin
guarding `PP18RR4-1`, the wave's worst regression, holds in one keyboard-settable
configuration and goes red in the other for no code reason. V-MODE, thirty lines
above it, saves, sets and restores `FLAG_ERPN` precisely because this axis
changes the answer.

**Violated.** The rule this wave applied twice in its own commits:
`prettyTest.c:342-345` — *"Without PROPFR the drawing has no numeric run and
this pin compares two identical pictures"* — and `prettyTest.c:1436-1437` —
*"Establish the state the assertion needs … or the pin proves nothing."* Also
the V-MODE block's own premise at `:4105-4110`, which claims the suite runs
*"the build default … classic lift and SSIZE8"* — false for this driver file on
both axes, since `pretty_print.txt:8` pins `SS=4` and `ppcTestReset` clears
`FLAG_ERPN`.

**Bug class.** Same as `PP18RR5-2`: an assertion whose truth depends on ambient
machine state the pin neither sets nor names. Two of the wave's four new pins
carry it.

**Class-level test.** The same one `PP18RR5-2` needs, and it is one change, not
two: `ppvTestExpect` should take the mode it asserts under, or the V driver
should run its mode-sensitive fixtures under both settings the way V-MODE4 does.
The enumeration is the two flags the walker reads — `FLAG_ERPN` and
`FLAG_SSIZE8` — and there are only four combinations.

---

### PP18RR5-7 — the `setPixel` clamp re-indents four upstream lines instead of using the no-reindent wrap this package's own minimality review prescribed

`packages/pretty-print/screen.c:1289-1294`, generated into
`packages/pretty-print/patches/010-screen.c.patch`. **Introduced by
`266a60921`.**

**What breaks.** Nothing at runtime. Four upstream lines in the firmware's
hottest glyph loop — `setPixel(x1, y1);`, `setPixel(x1+1, y1);`, `}` and
`setPixel(x1, y2);` — are now MODIFIED rather than merely adjacent, with
indentation as the sole difference. On the next upstream rebase each conflicts,
or silently reverts the guard's indentation, for no behaviour.

**Reaching input.** Mechanical:
`python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py
packages/pretty-print/patches/*.patch` at `9de22ac7d` exits 1 with 5 `[WS-ONLY]`
findings, four of them these lines. At `6e6c2c0ab` the same command reports 9,
eight in `screen.c` — so this commit **halved** the churn it inherited and did
not take it to zero, which the prescribed idiom does: leaving the four enclosed
lines at their original 10/12-space indentation inside an added
`if(x1 < SCREEN_WIDTH) {` … `}` leaves only the two genuinely changed condition
lines modified, and the deleted `}` then maps to a byte-identical added one and
drops out.

**Provenance, which decides whose defect this is.** At `6cc133c76` — the HEAD the
minimality review ran against — `010-screen.c.patch` had **zero** deleted
upstream lines; it was purely additive. All of this file's churn postdates the
review that set the standing count of 1.

**Violated.** `design-docs/pretty-print/REVIEW_upstream-minimality_2026-08-27.md`
§1, which names this exact class and its fix: *"the no-reindent wrap — add the
`if (...) {` and the closing `}` on their own lines and leave the enclosed
upstream line at its ORIGINAL indentation, byte-identical"*, and states of the
catalogued `softmenus.c` guard-wrap exception that *"No such ruling exists
here."* `deliberate-exceptions.md`'s thirteen entries are all forth-core's; the
only `showGlyphCode` entry there is forth-core's warning silencer.

**The weaker half of the charge, stated as such.** The finder also read §5 —
*"The churn gate is not wired beside this package's pins; the next run must
state its own count against this one"* (count: 1) — as an unmet exit criterion.
That sentence most naturally binds the next *upstream-diff review* run, not an
audit fix commit, so "criterion unmet" is a stretch. The churn itself is not.

**Bug class.** Merge-surface churn: a behaviour-neutral edit that converts
adjacent upstream lines into modified ones, in a function two packages already
patch (forth-core has a hunk at `:1159`, inside the same function).

**Class-level test.** Wire the scanner beside the package's pins, as §5 asks:
one gate step running `patch_churn_scan.py` over `packages/pretty-print/patches/`
and failing above a recorded baseline. The count is a single integer, the
scanner is the project's own, and the excursion 1 → 9 → 5 happened entirely
unobserved because nothing runs it.

---

## 4. PLAUSIBLE

Survived scrutiny; nobody constructed the reaching input. One this round.

### PP18RR5-P1 — the Z/T VISUAL arm may inherit a stale `SCRUPD_MANUAL_SHIFT_STATUS` from an earlier full-screen paint and lose the menu repaint the guard exists to give it

`packages/pretty-print/prettyVisual.c:1630` against the repaired guard at
`packages/pretty-print/screen.c:5900`.

**The candidate path.** The Z/T arm sets `screenUpdatingMode |=
SCRUPD_MANUAL_STACK` — an OR, and the package never clears these bits anywhere.
The full-screen arm (`:1558`) sets all three MANUAL bits. If a full-screen
VISUAL (or PSHOW, PHIST, EQSHW — four package surfaces set the bit) leaves
`MANUAL_SHIFT_STATUS` set and a later Z/T paint ORs `MANUAL_STACK` onto it, the
repaired guard's `(MANUAL_MENU | MANUAL_SHIFT_STATUS) == 0` test fails and the
Z/T window loses its menu repaint — which is the exact freeze `PP18RR3-2` fixed,
returned by the door the `PP18RR4-5` repair opened.

**Why it is not CONFIRMED.** The reachability lens killed the general version of
this claim and could not kill this one. `fnPrettyVisual` is `TM_LBLONLY`
(`items.c:2831`), so outside the test suite it is only ever entered through TAM
parameter entry, and **every** keyboard route to that parameter passes through
`addItemToBuffer`, whose first executed statement is
`screenUpdatingMode &= ~(SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_SHIFT_STATUS)`
(`bufferize.c:457`, byte-identical to upstream). So the bit is wiped by the very
keystrokes that name the label. What is not closed is the dismissal path in the
other direction: leaving the full-screen surface goes through `fnKeyExit`'s
`TI_SHOW_REGISTER || SHOWMODE` arm (`keyboard.c:3757`), which sets `TI_NO_INFO`
and routes the next refresh through the full `_refreshNormalScreen` rather than
the guard, and nobody established whether that pass clears
`MANUAL_SHIFT_STATUS` before a second VISUAL invocation. Two readers reached
opposite conclusions on the ordering and neither measured it.

**What would settle it.** One probe, and the harness can drive it: run the
full-screen arm, dismiss it the way a key does, run the Z/T arm, then
`refreshScreen`, and read `screenUpdatingMode` at `screen.c:5900` — the same
instrument `PP18RR4-5` used, which produced unambiguous numbers there
(`scrupd=10`). If the bit survives, the guard's predicate has to become
package-owned state rather than a composition of upstream's bits, which is
`PP18RR5-D5`.

### Raised, never refuted — where each went

No finding fell beyond the verification cap this round: all fourteen distinct
claims were verified. The five that died are in §6, with the traces that killed
them.

---

## 5. Design observations (D7)

Shape, not defects. Six; the second is the one that will still matter after the
findings are closed.

**`PP18RR5-D1` — the fix-regression rate survives the removal of everything
except fixes.** Round 4's wave carried features, a rename and three comment
sweeps, and 8 of its 14 findings were its own work. This wave is nothing but
repair — seven commits, twelve closures, no feature — and **six of seven
findings are defects the repairs created**; the seventh is a pre-existing
behaviour a repair declared closed. That is roughly a defect per two closures,
against a wave that was careful, mutation-verified its pins, and got eleven of
twelve closures exactly right. The lesson is not "the fixes were sloppy" — they
were not — it is that the finding rate is a property of *editing this code*, not
of what the edit is for, and a fix-of-the-fix wave is therefore not a way out of
the loop by itself.

**`PP18RR5-D2` — closing a hole and documenting it closed are separate acts, and
this wave coupled them.** `24cbf7590` did the right structural thing: it moved a
precondition from three callers into the one place that can answer it. Then it
wrote into the header that *no caller carries that precondition*. Before the
wave, `ITM_YX`'s missing bracket was an unstated hole a reader might find; after
it, the same hole is covered by a written guarantee, and a reader who trusts the
header will not look. Both `PP18RR5-1` and `-3` exist in the gap between what the
kind test does and what the sentence claims. **A contract sentence is an
assertion about code the author did not necessarily read**, and unlike a pin it
never runs. If a fix wants a sentence like that, the sentence is what needs the
enumeration — grep for the node kind, list the producers, and either fix them all
or write the exception into the same paragraph.

**`PP18RR5-D3` — the pins written to close pin defects inherited the defect.**
Round 4 found three pin failures (M8 vacuous on an ambient flag, T23c accepting
an out-of-range sentinel, the `PP_RAD` dual with no assertion at all). The wave
fixed all three and added four pins; **three of the four have a defect**. V-FILL
is insensitive to the very fix it is named after — measured both ways. V-XEQ is
locked to one lift mode and documents itself under another pin's labels. T24c
pins one arm of a rule it states over a class. Only M9 is clean, and M9 is the
one that asserts a **property** (`r->descent >= i->relBase + i->descent`) rather
than an example — the refutation pass attacked its guard hard and could not
reach vacuity across any of the three shipped fonts. That is the difference worth
carrying: the three defective pins each assert a *string* produced under an
ambient the fixture does not own; the clean one asserts a *relation* the code
must satisfy in any configuration.

**`PP18RR5-D4` — the harness fences off the configuration the machine boots
into, and the wave discovered that axis and then ignored it twice.**
`config.c:271` sets `FLAG_SSIZE8` in the Reset, JM, RJ and C47 columns and
clears it only for HP35: the eight-level stack is the shipped default.
`pretty_print.txt:8` sets `SS=4` for the whole driver file, and `ppcTestReset`
clears `FLAG_ERPN`. So every V pin except V-MODE/V-MODE4 runs in a machine
configuration the calculator does not start in, and the V-MODE block's own
comment — *"Four audit rounds ran every V fixture under the build default only,
which is classic lift and SSIZE8"* — is wrong about its own driver on both axes.
This is why `PP18RR4-4` could not be reproduced, why V-FILL is green, and why
V-XEQ reddens under eRPN. The fix is not more fixtures; it is that the two flags
the walker reads are a two-bit axis with four settings, and the driver should
name which one each pin asserts under.

**`PP18RR5-D5` — the menu guard is now a composition of three upstream-owned
bits to answer a package-owned question.** The repaired predicate at
`screen.c:5900` asks "which of my two surfaces is on screen?" and answers it
with `calcMode`, `screenUpdatingMode` and `temporaryInformation` — none of which
the package owns, one of which (`MANUAL_MENU`) upstream clears as teardown
bookkeeping, and one of which (`MANUAL_SHIFT_STATUS`) four package surfaces set
and none clears. The repair is correct against every clearer in the tree, and I
could not break it; it is also the second time in two waves that this predicate
has been re-derived from upstream's bits after one of them turned out to mean
something else. A package-owned latch set by the two VISUAL arms would answer
the question directly and would not move when upstream's bookkeeping does. That
is also what would retire `PP18RR5-P1`.

**`PP18RR5-D6` — twelve fixes cite a document that does not exist in the
repository.** `git log --all -- 'design-docs/forth-core/AUDIT_PP18-round-4*'` is
empty; the report defining `PP18RR4-1`…`-14` is untracked in the working tree.
Combined with zero `design-docs/` commits in the range and zero recorded flash
deltas, the wave's entire justification lives in commit messages. Round 4's `D4`
said the design record did not move for nineteen fixes; a wave later it has not
moved for twelve more, and the audit trail those commits point at has never been
committed either.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. This is the substantive half of the round: the wave closed twelve
findings and **eleven of the twelve hold**, several of them re-derived from
upstream independently by three or four dimensions rather than trusted.

### Killed by the refutation pass

**"The menu guard's new bit is not the durable marker its comment claims: four
package surfaces set `MANUAL_SHIFT_STATUS` and no dismissal path clears it"**
(`screen.c:5900`). **REFUTED on reachability.** There is a structural blocker
that makes the bit provably clear when the Z/T arm paints: `fnPrettyVisual` is
`TM_LBLONLY` (`items.c:2831`), so outside the suite it is only entered through
TAM parameter entry, and every keyboard route to that parameter — softkey in TAM
(`keyboard.c:1204-1209`), main keyboard in TAM (`:2754-2772`), alpha letters via
`processAimInput` (`:476`, `:494-511`) — runs `addItemToBuffer`, whose first
statement is `screenUpdatingMode &= ~(SCRUPD_MANUAL_STACK |
SCRUPD_MANUAL_SHIFT_STATUS)` (`bufferize.c:457`). The only direct
`tamProcessInput` calls that bypass it are guarded on `tam.mode == TM_KEY`. So
any bit leaked by PSHOW, PHIST, EQSHW, the full-screen arm, CM_MIM or CM_TIMER
is wiped before `prettyVisual.c:1630` runs — which is also why V36b still passes
with the tightened guard. The comment's prose is loose (it says "the difference
between the two VISUAL surfaces" when four surfaces set the bit) but no drawn
pixel changes on any constructible input. The one residue — the dismissal
ordering — is `PP18RR5-P1`.

**"The new guard ignores `SCRUPD_SKIP_MENU_ONE_TIME`, the bit every other
menu-repaint site in this file tests"** (`screen.c:5901`). **REFUTED on
intent, three ways.** (1) The design ruled on what this predicate must read:
`DESIGN.md:309-310` declares the full-screen surface as
`MANUAL_STACK | MANUAL_MENU | MANUAL_SHIFT_STATUS` and `DESIGN.md:690` declares
the Z/T window as `MANUAL_STACK` alone *"so the menu and status bar keep
working"*. The guard's question is which declaration is on screen; a transient
one-time speedup flag declares no ownership and cannot answer it. (2) The cited
convention does not transfer: upstream's mask at `:6049` guards
`showSoftmenuCurrentPart()` **alone**, while `refreshStatusBar()` nineteen lines
below is unconditional (upstream commented its own `MANUAL_STATUSBAR` guard
out). The package's if-body paints both, so adding the bit would newly suppress a
status-bar repaint upstream never suppresses. (3) `grep -rn
SCRUPD_SKIP_MENU_ONE_TIME src/ packages/` finds only clears and tests — **zero**
`|=` setters anywhere in the tree; the one candidate sits commented out in
`keyboardTweak.c:125` with the regression that removed it stated inline. It is a
hypothetical about a bit no code sets.

**"29 of the 56 lines this package adds to upstream `screen.c` are comments, and
two blocks narrate audit-round fix history"** (`screen.c:1283`). **REFUTED.**
The count is exact (29 comment / 2 blank / 25 code) and the characterisation is
not. Neither block contains a round number, a finding ID, a "this used to say X"
or a `file:line` anchor — the four shapes the 2026-08-09 comment-budget ruling
names for deletion — and that ruling's explicit KEEP category is *"an INVARIANT
a future editor would otherwise break"*, with a stated preference to *"name a
function over citing a line"*, which is exactly what the `:5888` block does.
Every clause of it is a live property of HEAD: `softmenus.c:3697` does clear
`MANUAL_MENU` unconditionally and does not touch `MANUAL_SHIFT_STATUS`;
`prettyVisual.c:1630` does take `MANUAL_STACK` alone and `:1558` all three. It
also replaced a comment that was factually **false**. And the "cost paid at every
rebase" clause inverts the project's own instrument: `patch_churn_scan.py`'s
COMMENT-ONLY tier exists to require that a package comment sit on its own added
line so the upstream line stays byte-identical — the shape both blocks already
have. (The finding's arithmetic was also off: `screen.c` at 56 adds is the third
largest patch, behind `solver/equation.c` 608 and `keyboard.c` 64.)

**"The split value payload now has three hand-written decoders and an unstated
`32 ≤ 2×16` invariant"** (`prettyFormula.c:422`). **REFUTED on both
load-bearing premises.** The invariant is stated, at `prettyInternal.h:85` —
*"A complex34 is 32 bytes, so it fits in 16 + 16"* — one line above the
`PPC_VAL_CAPACITY` definition and inside the range the finding cited. And the
named trigger edit is inert: `bytes` comes from
`TO_BYTES(getRegisterFullSizeInBlocks(regist))`, which for the only dataType the
wider cap admits returns `COMPLEX34_SIZE_IN_BLOCKS` = `TO_BLOCKS(sizeof(complex34_t))`
(`realType.h:39`) — a compile-time 32 with no dependence on `PPC_VAL_CAPACITY` —
while every other dataType is capped at `sizeof(payload)` = 16 by the other arm
of the ternary. Raising the constant alone can never make `bytes` exceed 32, so
neither the serializer's `bytes - first` nor the reader's `bytes - head` can
overread. What survives is true and consequence-free: three sites implement the
split by hand with no shared accessor, and `ppcArena[cont].aux = rest`
(`prettyCapture.c:257`) is written and never read by either VAL reader. A field
no consumer reads cannot make a number wrong. Worth a cleanup, not a finding.

**"M9's setup guard checks the index's descent, not the quantity the assertion
needs"** (`prettyTest.c:165`). **REFUTED on reachability, with arithmetic.**
`i->descent == 0` is indeed a proxy for the precondition the assertion needs, but
no reachable input makes the gap bite. `indexDescent` reduces to
`minus.rowsAboveGlyph − digit.rowsAboveGlyph + barThick + fracGap + 1`, so the
finding's worked example ("8 → 4 at the same standard font") is arithmetically
impossible — at standard font it is 8, by construction. Evaluating the margin
across the three fonts the firmware ships, from the real glyph tables, gives
`idxDesc` = **+20** (numeric), **+6** (standard, matching the mutation's printed
6) and **+6** (tiny). Zeroing `barThick` and `fracGap` still leaves +2;
`vincThick` cancels algebraically; `radGap` would have to go 1 → 7. The only
remaining route is moving `standardFont`'s minus-sign ink up three rows relative
to the digits — a broken font, not a font change, and one that would visibly
wreck every fraction the calculator draws. The "quietly" fails too: the suite
hardcodes derived geometry (M2's width/ascent 26, M5's ascent 30, P4's vinculum
rows 130–133), so any metric shift that large reddens loudly first. `bbf43534b`
is a good commit; the mutation that deleted its clamp reddened M9 alone.

### The wave's fixes, re-derived rather than trusted

**`PP18RR4-1` / `ppvLiftNeutral`'s membership — CLEAR, and this was the brief's
top question.** Four dimensions enumerated every arm of `ppvStepArm` against
upstream's real dispatch, not against the SLS column alone, and agreed. Only
`ITM_ENTER` arms the latch and only `ITM_XEQ` walks a callee on the shared stack,
so those two plus the no-stack declarations are the whole membership. **The two
apparent counterexamples are not:** `ITM_LBL` and `ITM_REM` are `SLS_ENABLED` in
`items.c`, but `executeOneStep` returns on `PTP_DECLARE_LABEL` and `PTP_REM`
without dispatching (`lblGtoXeq.c:826`, `:842`), so their SLS never runs and a
LBL step really does leave a pending lift alone. `ITM_LITERAL`/`ITM_RCL` return
early and are correctly absent, since `ppvPushLifting` clears the latch itself on
both branches. `ITM_PGMINT`/`ITM_PGMDRV` clear it in-arm and are `SLS_ENABLED`,
so the epilogue's second clear is idempotent (their comment *"clear BEFORE the
nested walk, see ITM_XEQ"* is copy-pasted — neither arm walks — which is
cosmetic). The construct arms recurse through `ppvBody`, which builds a fresh
frame with `liftDisabled = false` (`:432`), so nothing leaks in either
direction. And the new direction matches upstream's ordering: XEQ's SLS is
applied inside `reallyRunFunction` at the XEQ step, **before** the callee's first
step, which is exactly the arm's clear-before-the-walk; `RTN` and `END` are
`SLS_UNCHANGED`, which is what makes the callee's latch survive the return. The
list is right. Its written rule and its duplicate copy are `PP18RR5-4`.

**`PP18RR4-5` / the menu guard — CLEAR against every clearer in the tree.** I
looked for a route leaving both `MANUAL_MENU` and `MANUAL_SHIFT_STATUS` clear
while a full-screen surface is held. `popSoftmenu` clears only `MANUAL_MENU`
(`softmenus.c:3697`). `refreshScreen`'s second clearer (`screen.c:6245`,
`if((doRefreshSoftMenu && !SHOWMODE) || calcMode == CM_ASSIGN)`) is real and
independent, but `SHOWMODE` is true for every package surface (they all hold
`TI_SHOWNOTHING`), so it cannot fire against one, and it does not touch
`SHIFT_STATUS` in any case. `commonShiftProcessing` (`keyboard.c:1520/1531`)
clears `SHIFT_STATUS` but leaves `MENU`, and `RETURN_NORMAL` (`screen.c:6078`)
re-sets `MENU` on every pass, so the two bits are never both stripped while the
surface is up. The reverse regression is excluded too: the only upstream sites
setting `STACK|SHIFT_STATUS` without `MENU` are the CM_MIM and CM_TIMER arms,
both outside the guard's `calcMode == CM_NORMAL`. A dismissal-path hypothesis
(`keyboard.c:2097` clearing `SHIFT_STATUS` under a held surface) was built and
killed: `btnPressed` calls `closeShowMenu()` on any key while `SHOWMODE`, and
`closeShowMenu` resets `screenUpdatingMode = SCRUPD_AUTO`, so the early return is
not taken on that path. The Z/T arm's behaviour is byte-identical before and
after the change.

**`PP18RR4-2` / the `PPN_VAL2` reassembly — CLEAR on bounds and on the
writer/reader contract.** `head = sizeof(payload) = 16`,
`full[PPC_VAL_CAPACITY] = 32`, and `bytes > sizeof(full)` is refused before
either `xcopy`, so `bytes - head ≤ 16 = sizeof(c->payload)` — the arithmetic
closes with no slack. The producer cannot mint a `bytes` it cannot honour:
`ppcValLeafFromRegister` caps at `PPC_VAL_CAPACITY` for `dtComplex34` and turns
everything else over 16 bytes into `PPN_OPAQUE` (`prettyCapture.c:227-232`).
`ppcDeepCopy` recurses into `child[0]` before copying the payload, so a copied
value leaf keeps its continuation and the new `return PP_NONE` ("a split value
with no continuation") is unreachable by construction — and it refuses before
staging, so it leaves no half-built state. The reader tests only `PPC_NIL` where
the serializer tests both sentinels, which is safe because `ppcNodeAt`
range-tests against `PPC_NODES` and both 0xFE and 0xFF are out of range. The
history path needs no equivalent change: `PPT_TKV` is written flat.

**`PP18RR4-11`/`-12` / the wrapped-x clamps — CLEAR, at every boundary.** `x` is
`uint32_t`, so the comment's "a negative x is a huge uint32" is exact and the new
pre-clear test does screen it. `x2` is `x1` or `x1-1`, so `x2 < SCREEN_WIDTH`
implies `x1 ≤ SCREEN_WIDTH` and hoisting the `x2` writes cannot admit a wrapped
column; the only behaviour change is `x1 == SCREEN_WIDTH` exactly, whose twin at
399 is a genuine on-screen half-column — which is the pixel the fix set out to
recover. The bold twin (`x1+1`) stayed nested under `x1 < SCREEN_WIDTH`, and
that is load-bearing: hoisting it the way `x2` was hoisted would let a wrapped
`x1 == 0xFFFFFFFF` compute `x1+1 == 0` and write column 0. It was not hoisted.
The unscreened right edge (`x + dx`) is not a new exposure: both simulator HALs
drop a rect whose `endX > SCREEN_WIDTH` outright, and on the device it is
unchanged from upstream.

**`PP18RR4-10` / `ppSuppressBold`'s corrected comment — CLEAR, and verified
rather than assumed.** `FLAG_BOLD` is in `refreshStateFlags[]` (`flags.c:64`),
`systemFlagAction` runs that loop before the `doInteractionFlags` switch
(`:78-82`), and `fnRefreshState` is exactly `doRefreshSoftMenu = true`
(`radioButtonCatalog.c:544`). The new comment is now accurate where the old one
was false. "Harmless today" also survives contact: the one consumer that would
bite is `screen.c:6245`, gated on `!SHOWMODE`, and every package surface that
flips bold holds `TI_SHOWNOTHING`. Both wrappers place the suppress after all
early returns, so the comment's "no early return between save and restore" is
true.

**`PP18RR4-6` / M8's `FLAG_PROPFR` and `PP18RR4-13` / T23c — CLEAR.**
`FLAG_PROPFR` is what makes `ppParseFraction` take the `headLen > 0` branch, the
head run is built in `PP_FONT_NUMERIC`, and `showGlyphCode`'s bold substitution
is gated on `font == &numericFont` — so the fix is exactly the state the
assertion needed, and M8 restores it. T23c's narrowing from "UNKNOWN or NIL" to
"UNKNOWN" cannot false-fail: every write to `ppcSlot[]` (nine sites in
`prettyCapture.c`) stores `PPC_UNKNOWN` for degradation, and the two paths that
could produce `PPC_NIL` convert it, so `PPC_NIL` is unambiguously
`ppcTestSlotRaw`'s out-of-range sentinel. Both restore their flags on every
branch, and `ppTestFail` does not return, so no failure path skips a restore.

**T24c is discriminating, not vacuous.** `ppfTestSigNode` emits `S(...|...)` for
`PP_SUP` and `P(...)` for `PP_PAREN`, so `S(P(S(3|3))|3)` is precisely
cube-of-cube-with-bracketed-base and the pre-fix tree fails the `strcmp`. Its
only defect is class coverage, filed inside `PP18RR5-1`.

**The walker's surviving local stacked-power guard** (`prettyVisual.c:1122-1126`)
composes without double-bracketing: `ppfBuildOp1`'s new paren branch never
consults `aPrec`, so the walker's raised precedence is dead rather than
additive, and the walker's own pinned output is unchanged. It is now genuinely
redundant, as `24cbf7590` says.

**The equation renderer is the third `PP_SUP` producer and is safe by a
different route.** `ppqFactor`'s `^` arm recurses right, and a base always comes
from `ppqPrimary`, which cannot be a `PP_SUP` without explicit parentheses. It
does not need the kind test.

### Guards and conjuncts, falsified or proved load-bearing

- **`bytes > (uint8_t)sizeof(full)`** in the new reader is not falsifiable today
  (the producer caps first), but it is defensive against a second `PPN_VAL`
  producer rather than dead. Not flagged.
- **`a != PP_NONE`** added to the SQUARE/CUBE arm is belt-and-braces over an
  already-safe `ppNodeAt`. Not flagged.
- **The new pins' own setup guards are falsifiable and fail loudly** — M9 asserts
  `i->descent != 0`, T23c establishes `slot4WasKnown` before the store, M8 sets
  `FLAG_PROPFR`. No vacuous conjunct among them.

### Unreached shapes, named so they are findings the day something changes

- **The pre-clear now REFUSES where the package's own painters CLIP.**
  `ppFillVal` trims four edges and `ppDrawLine` both axes; the new
  `x < SCREEN_WIDTH` test skips the clear entirely, so a glyph whose origin
  wrapped negative would paint the columns that wrap back on-screen over an
  **uncleared** box. No caller passing a negative or `SCREEN_WIDTH` origin could
  be derived — every `ppPaintAt` origin in the package is non-negative and
  width-bounded — and the commit's own stated reaching input ("panning a history
  row off the left edge") is not derivable either, since PHIST pages vertically.
  The one entry not ruled out is `prettyEquation.c:825`'s `xLeft` from upstream's
  equation paint site.
- **`ppvBody` leaks a binding slot on every early return** (`:418-453`
  increments at entry, decrements only on success). Every such return has
  `ctx->failed` set and `ppvWalk`'s first statement aborts the walk, so the count
  is never read. Pre-existing, outside the range.
- **`ppvPush` entered with `depth 8` against 4 live slots** discards the top four
  and keeps the bottom four. The only producer is `ITM_FILL`, whose eight entries
  are identical, so the discard is invisible — `PP18RR4-4`'s territory, recorded
  in §3, not re-filed.
- **`uint8_t full[PPC_VAL_CAPACITY]` adds 32 bytes to every frame of a recursive
  function.** Depth is bounded by the 24-node arena so the worst case is well
  under 1 KiB, but RAM discipline is binding in this project and the wave
  recorded no arena figure.

### Doc drift, found and not filed as defects

- **`DESIGN.md:571-574`** still reads *"a stacked power DOES need its base
  bracketed, which the walker does locally because `ppfBuildOp` deliberately has
  no POW level"*. Ownership moved into `ppfBuildOp1` this wave; the sentence is
  now stale about who does it, though its ruling against a POW level stands and
  is the reason `PP18RR5-3`'s repair is a decision rather than a line.
  `DESIGN.md` is outside the PR file set.
- **`DESIGN-HISTORY.md:217-218`** carries the same sentence one more time.

### Known, ruled, or below the bar

- **`PP18RR4-4`** and **`PP18RR4-8`** are open by the wave's own statement;
  `-4`'s reaching input is closed in §3, `-8`'s file was not touched.
- **`PP18RR4-P1`** (the matrix-SHOW reach) was not re-derived and the probe that
  would settle it was not run.
- **The text back end still duplicates precedence.** `ppvAstPrec`/`ppvOperand`
  and `ppfBuildOp1/2` remain two bracketing implementations and nearly every V
  pin asserts the **text** one, so a divergence in the drawing path does not go
  red. Ruled deliberate in `DESIGN.md`; `PP18RR5-1` is what that risk looks like
  when it lands.
- **`files/` sync** — all six in-scope sources are byte-identical between the
  flat working area and `files/`, and the gate's own refresh left the tree clean,
  so the build read what this report read.

---

## 7. Verdict

**Would I ship this? Yes, with one repair first — and this is a better wave than
round 4's.** Twelve findings closed, eleven of them exactly right, and the two
that mattered most were re-derived from upstream by four independent dimensions
rather than accepted: the lift-latch exception list is complete against
`items.c`'s SLS rows **and** `executeOneStep`'s PTP early returns, and the menu
guard holds against every clearer of `MANUAL_MENU` in the tree. The `PPN_VAL2`
reassembly's arithmetic closes with no slack. The wrapped-x clamps are correct
at every boundary, including the one nesting whose removal would have been a
real defect. M9 is the best pin the stage has produced — a property, not a
number, and the refutation pass could not reach vacuity across any font the
firmware ships.

What stops it is one closure that stopped one arm short and then wrote down
that it hadn't. `24cbf7590` moved the stacked-power bracket into
`ppfBuildOp1` — the right structural instinct — and `ppfBuildOp2`'s `ITM_YX`
arm, twelve lines above in the same file and named as adjacent by round 4's own
report, still brackets by precedence alone. `2 ENTER 3 yˣ 2 yˣ` draws `2³²` for
64, on both capture surfaces, while `prettyInternal.h` tells the next reader
that shape is handled.

**Where would it break first?** In an owner's hands, in this order:

1. **`2 ENTER 3 yˣ 2 yˣ`, or `3 x² 2 yˣ`** (`PP18RR5-1`). Four keystrokes, a
   silently wrong picture on the T line and in PHIST, and the layout draws both
   exponents at identical height so there is nothing on screen to hint at it.
   Measured with two probes through the real dispatch.
2. **A program with `FILL` and a chain longer than the stack, under the default
   eight-level stack** (`PP18RR4-4`, still open). "Cannot be drawn (D10)" for a
   program `XEQ` runs to 63. Measured three ways this round; the input the round-4
   report said needed re-deriving is simply the machine's own reset default.
3. **`x²` on a value in scientific form** (`PP18RR5-3`). `1×₁₀⁵⁰²` for
   `(1×10⁵⁰)²`. Read from the code rather than rendered, which is exactly the
   check it still needs.
4. **The next maintainer adding an inert item to the walker** (`PP18RR5-4`). Add
   it to the dispatch group, miss the exception list, and the walker draws an
   expression for a program the machine cannot compute — with the gate green, as
   measured.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.**

- **`PP18RR5-7`, the churn.** Four whitespace-only upstream lines in the
  firmware's hottest glyph loop. Taking them to zero earns nothing until the next
  rebase, and re-indenting that block is itself a diff nobody should make on its
  own. Do it the next time `showGlyphCode` is opened for behaviour, verify with
  `git diff -w` plus the gate, and state the count in the commit — which is the
  part that is actually missing.
- **`PP18RR5-6`, V-XEQ under eRPN.** The picture the walker draws in that mode is
  **correct**; the pin is mode-locked, not wrong. It costs nothing until someone
  reorders the fixtures. Fold it into whatever change fixes `PP18RR5-2`, because
  it is the same one-line question — which configuration does this pin assert
  under — asked about the other flag.
- **`PP18RR5-5`, V-XEQ's comment.** Three words in a comment. Real, worth doing,
  worth nobody's wave. The label-uniqueness check is the part with a return, and
  only because this exact collision has now cost two debugging sessions.
- **`PP18RR5-4`, the lift-neutral rule.** Do **not** restructure the switch to
  chase it: the list is right, and the last structural refactor of this switch
  produced `PP18RR4-1`. The wording is a comment fix. The part with value is the
  structural assertion that ties the two switches, and it exists because deleting
  six entries left the whole gate green.
- **`PP18RR5-3`** needs a **ruling first, not a patch.** The obvious repairs are
  both wrong: a POW precedence level is ruled against in `DESIGN.md` because it
  changes the contract under the capture engine, and special-casing SCI text
  inside the builder puts a formatter concern in a layout file. The honest
  options are a flag on the value leaf that says "my text ends in an exponent" or
  an accepted, written limitation — and either way, measure the picture first.

**What should not wait.** `PP18RR5-1`: four keystrokes, wrong mathematics, and a
header sentence that will stop the next reader from looking. And `PP18RR5-2`
together with `PP18RR4-4`, because they are one decision, not two — the arm must
use the live depth and the pin must run under both stack sizes, and fixing
either alone leaves the other's evidence unchanged. The measurement that says so
is in §2: applying the `ITM_FILL` fix leaves the gate green and V-FILL emitting
the same string.

**What is genuinely solid, verified rather than assumed.** The exception list is
complete and its two apparent counterexamples (LBL and REM carrying
`SLS_ENABLED`) are refuted by upstream's own dispatch. The menu guard has no
reachable regression. The value-leaf reassembly cannot overrun and its refusal
path cannot be reached. The clamps are exact. M8, T23c and M9 all now establish
the state their assertions need. `ppSuppressBold`'s comment is true where it
was false. The gate is green on a clean tree in 170 s, with no new warning.

**The pattern to carry.** Round 4's was *"a class fixed by a refactor, at every
site that looked like the others"*. Round 5's is one turn further in:
**a class fixed at the one place that can answer it — and then declared answered
for callers nobody enumerated.** `ppfBuildOp1` was the right place. `ppfBuildOp2`
and a value leaf's own glyphs were the callers, and the header now says they do
not exist. The remedy is the one every class-level test above is written in, and
it is cheap here because every enumeration in this package is two or three
members long: grep the producers of the node kind, list them in the commit, and
either fix them all or write the exception into the same paragraph as the
guarantee.

Its companion, from the pins: **a pin written for a defect you could not
reproduce will agree with the configuration you could not reach.** V-FILL is the
clean example — it was written from the one stack size where the bug cannot
bite, and it is green with the fix and without it.

---

## 8. Round and exit state

**Round: PP18 round 5 of the restarted series**, and the second consecutive
fix-wave audit. Subject `6e6c2c0ab..9de22ac7d`, seven commits, restricted to the
upstream-PR file set. The rate the round was set to measure: r2 was 4 of 7 from
r1's fixes, r3 4 of 4, r4 8 of 14, and **r5 is 6 of 7 outright — 7 of 7 if the
false closure claim counts — against twelve closures.**

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

**Counts.** Twenty-four findings raised across eight dimensions; convergence was
heavy — six of the eight independently reached the `ITM_YX` arm and four reached
V-FILL — so they collapse to **fourteen distinct claims**, all of which went
through refutation. **Five refuted**, nine survived, deduplicating to **seven
CONFIRMED** (`PP18RR5-1`–`-7`), **one PLAUSIBLE** (`PP18RR5-P1`), and six design
observations (`PP18RR5-D1`–`D6`). Separately, the still-open `PP18RR4-4` was
closed on its reaching input and is recorded in §3 without a new number.

**Evidence discipline.** **Five of the seven** are backed by a mutation applied
in an isolated worktree, built through the package gate with presence verified in
`build.sim/custom_pkg_shadow/*`, observed in `build.sim/meson-logs/testlog.txt`,
and reverted. The two static ones say so: `PP18RR5-3` is a code trace plus an
exhaustive documentation search with no rendered picture, and `PP18RR5-7` is a
scanner result — run by hand in the verifier's worktree because the sandbox
refused the scanner four times there, and re-run for real by this synthesis pass
on the main tree, where it agrees exactly (5 findings, 4 in `screen.c`). No
simulator ran; no finding rests on an LCD photograph. Main tree clean at start
and finish, gate green on it in 170.53 s, no `AUDIT-PROBE` marker anywhere in
`packages/`.

**Exit criterion: NOT MET, and this round cannot advance it.** Seven new
CONFIRMED findings would reset the count on their own; separately the round had
no out-of-family reader, and the criterion requires two consecutive clean rounds
with at least one of them out of family. **Five consecutive rounds have now run
in-family only.**

**Process items.**

1. **Stale worktrees: ninth consecutive round.** Every verifier worktree again
   spawned at `e21af8d28`, which is *not an ancestor of the audited tip*, so the
   audited code did not exist there. **Every single verifier detected it** with
   `git merge-base --is-ancestor` and ran `git checkout 9de22ac7d` before its
   first read, and every evidence block opens with the check. That is
   `c719b1a9c`'s instruction working at 100% for the second round running. It is
   still a reader-side workaround for a spawner-side bug, and the guard in
   `audit-workflow.js` is still absent.
2. **`/tmp` is shared between sibling worktrees — new this round.** Two
   verifiers wrote gate logs to the same `/tmp/gate_baseline.log` path and
   clobbered each other mid-read; one caught it because the tail carried another
   worktree's shadow paths. No verdict was affected (each build resolves
   `CUSTOM_PKG` to its own worktree and the per-worktree
   `build.sim/meson-logs/testlog.txt` is authoritative), but the mutation
   protocol should require worktree-unique scratch filenames.
3. **The sandbox classifier refused `python3 patch_churn_scan.py` four times** in
   one verifier worktree, forcing a by-hand execution of the scanner's algorithm.
   The result was correct — this pass re-ran the tool and got the same answer —
   but a reader who could not read the scanner's source would have had to drop
   the finding.
4. **`pkg_patch_refresh.py` after every revert is now standard practice** and
   every verifier did it unprompted: the gate's own refresh regenerates `files/`
   and `.refresh-manifest.json` from the probe, so reverting the source is not
   enough. Worth writing into the mutation protocol rather than rediscovering it.
5. **The round-4 report is untracked** (§2, `PP18RR5-D6`). Twelve commits cite
   `PP18RR4-n` numbers that exist in no committed document.
6. **Zero flash/RAM figures in seven commits** (§2), against CLAUDE.md's standing
   rule.
7. **Report filename truncated.** The requested filename is 344 bytes against a
   255-byte filesystem limit; this file's name is the requested one truncated
   after "…Round-4-found-8-of-14-in-the-wave-s", with the date and the `-r5`
   suffix preserved.

**Round 6's axis, in priority order.**

1. **An out-of-family reader, over `24cbf7590` plus the two builder functions.**
   Five rounds in one family, and this round's worst finding is a fix whose
   defect is visible the moment someone asks "what else builds this node kind?" —
   the question a reader without the author's model asks first. The packet is
   small and self-contained: `ppfBuildOp1`, `ppfBuildOp2`, `ppfFromCaptureNode`,
   `prettyInternal.h`'s banner, and the `PP_SUP` measure and paint arms. The
   question — "is the precondition in the header true?" — is answerable without
   the rest of the package.
2. **The whole V battery under `SS=8`, and under all four flag settings.** One
   line in `pretty_print.txt` reddens V-FILL today; five EQ pins go with it for
   unrelated fixture reasons, which is why nobody has run it. Sorting that out
   once opens a configuration branch that four audit rounds have never entered,
   and it is where `PP18RR4-4` has been hiding.
3. **`prettyCapture.c`, still unread.** Second consecutive round outside the PR
   file set, and it is the producer half of `PP18RR5-3` and of round 4's `-2`.
4. **The three-surface differential oracle.** `PP18RR5-1`, `PP18RR5-3`, round
   4's `-2` and `-3` are all rows in the same table: drive one operator sequence
   through `ppvTestBuildNodes`, `ppfBuildCurrent` and `ppfBuildEntry` and assert
   the three signatures are equal. Four findings across two rounds collapse into
   one fixture.
