# Audit — PP18 in the pretty-print package: the walker builds an AST and lays it out through the shared node builders, plus a new DERIV construct, at `f0edd5ee7`

*(Filename truncated from the full subject line: the ext4 limit is 255 bytes and the
subject as given is 1,890.)*

Subject: `1fd492a48..f0edd5ee7` on `pretty-print/stage-pp18`, five commits.
Eight finder dimensions ran blind to each other; every finding then went to
an independent refutation pass with one assigned lens — reachability,
correctness, or intent.

**Sixteen distinct CONFIRMED findings, one REFUTED, one beyond the
verification cap.** Five findings were reached independently by two to four
dimensions and carry that many lenses. Fourteen of the surviving verdicts
are backed by a probe or mutation applied, observed and reverted inside an
isolated worktree — this round is unusually well evidenced: the blank-screen
routes, the exponential expansion, the DERIV divergence, the ATOM
precedence, the lift latch, the doubled parentheses, the arena exhaustion
and the missing `step` check were all *executed*, not argued.

The refactor's central claim is true and was not worth attacking:
precedence is settled once now, the drawing is byte-identical, and 45 pins
survived a representation change unedited. Every finding of consequence is
somewhere else — in the failure envelope the round trip used to carry.
PP17's text back end was holding three implicit bounds (a 1 KiB pool with
construct-boundary rollback, `PPV_FRAG_MAX` = 255 on every fragment, and a
guaranteed linear line of last resort). PP18 removed the text and all three
went with it, and none was replaced on the node path. Three of the four
worst findings are that one sentence.

The worst finding is not in the failure envelope. It is the new construct's
faithfulness argument. `DESIGN-HISTORY.md` says the DERIV deferral "turned
out to cost one read"; the read was one call short. Upstream stores the
sample point into the variable `deriv_pgm_variable()` extracts from the
*body program's own* `MVAR` declarations, not into the `f'` parameter the
walker names. A body that declares no `MVAR` — the ordinary case — is
differentiated as a constant and returns 0 while VISUAL draws a picture
that means 6. Both halves were run.

Nothing was fixed. The tree this report finishes on is the tree it started
on, apart from one uncommitted change that was already there (§2).

---

## 1. Subject and coverage

**Tip.** `f0edd5ee7` on `pretty-print/stage-pp18` ("pkg: VISUAL against the
real appnote-22 file"). Range `1fd492a48..f0edd5ee7`:

| commit | what it did |
|---|---|
| `47f6e609b` | the walker builds an expression tree instead of a string (48-node arena, 512 B pool, rollback machinery deleted) |
| `55c363ad5` | draw from the tree; the text back end becomes a `#if PC_BUILD` test seam (flash 1,151,016 → 1,150,880, −136 B) |
| `28cc9c49e` | VISUAL draws derivatives (PGMDRV + F1DRV/F2DRV, `ppqBuildBigop` extracted) |
| `f044f875e` | docs: PP18 |
| `f0edd5ee7` | VISUAL against the real appnote-22 file (`prettyTestReal`, V58, the test-list anchor) |

**Diff.** 18 files, +1,970 / −565, dominated by `prettyVisual.c` (a near
rewrite) and `prettyTest.c` (+217). The rest: three design docs, the
generated `files/` twins, two patch files, one `testSuite.c` line, one
test-list anchor and the new `pretty_visual_real.txt` case. Base
`1fd492a48` (the builder split) is excluded by the range and was read for
comparison only.

**Read at line level** (union across the eight dimensions):
`prettyVisual.c` in full (all 1,352 lines) by six dimensions, and the PP17
version at `1fd492a48` in full by three — the before/after read is what
produced most of this report. `prettyFormula.c` `ppfWrapIf`/`ppfCombine1`/
`ppfCombine2`/`ppfParen`; `prettyEquation.c` `ppqBuildBigop`,
`ppqBigopConstruct`, `ppqScopeBody`, `ppqBuildCall`, `ppqFactor`/`ppqPrimary`,
`ppqShowRender`; `prettyLayout.c` allocators, `ppMeasure`'s guards, the
`PP_BIGOP`/`PP_SUP`/`PP_FRAC` measure and paint arms; `prettyInternal.h` and
`prettyPrint.h` in full; the whole `prettyTest.c` delta plus the pins the
coverage claims depend on. Docs: `DESIGN.md` §1/§3/§6/§VISUAL/§7/§8 in full
by five dimensions, the PP18 `DESIGN-HISTORY.md` entry by six, the
`TESTING.md` delta by four.

**Upstream read for the call-site contracts** — this is where two findings
came from: `solver/differentiate.c` (`derivativeVariable`, `calcDeriv`,
`deriv_pgm_variable`, `_differentiatorIteration`), `solver/integrate.c`
(`_fnIntegrate`, `DEI_xeq_user`), `programming/lblGtoXeq.c` (`_executeOp`'s
`PARAM_LABEL`/`PARAM_REGISTER` arms, `executeOneStep`'s PTP dispatch),
`items.c` rows 1819/1821/1853/1546/2882 and the `SLS_STATUS` epilogue at
`:604-611`, `keyboard.c fnKeyEnter`, `programming/decode.c`
`getStringLabelOrVariableName`, `registers.c allocateNamedVariable`,
`testSuite/testSuite.c` `covLoad*`/`covWriteAndLoadPgm`, `testSuiteList.txt`
`:480-540`.

**Deliberately not audited.** The 45 pre-existing pins' own correctness, and
everything the brief lists as already verified (byte-identical screenshot,
`func.p47` transpiling identically, gate green solo and combined, clean
warnings) — none of it was re-derived. Pixel geometry of the layout paint
arms beyond what two findings needed (the Σ/∏ strokes, the radical DDA):
that is what the P/S/EQ pixel pins are for. `prettyCapture.c` and
`prettyValue.c` (untouched by the range; the only question asked of them was
whether extracting `ppqBuildBigop` could change `ppfCombine`'s contract
underneath the capture engine — it cannot, the signature and the levels are
unchanged). `solver/equation.c` (unchanged in this range; its 619-line
override and the standing extraction debt belong to the 2026-08-27 upstream
review, restated in §5). Flash and BSS deltas — the stage commits record
measured numbers.

**What the budget did not reach.** Nobody built for the DM42n target or ran
anything on hardware; every timing figure is desktop and every claim about
the device is extrapolated. Three dimensions (contracts, arithmetic, design)
ran read-only and could not execute their own reaching inputs — the
refutation pass supplied the execution for all of their findings that
survived, which is why several verdicts read as more certain than the
finding that raised them. Nobody re-ran the combined gate as report author.
`ppMeasure`/`ppPaintAt` were not audited as arithmetic. The one reachability
question left open either way is the dirty-set/body-scope interaction (§4).

**Verification cap.** 21 finder reports went to the refutation pass; 20
survived (collapsing to 16 distinct findings after merge), 1 was refuted.
One further finding, the upstream dimension's `func.p47` coupling, was
beyond the cap and is reported as unverified in §4.

**Numbering.** Round 1 took `A1`–`A14` and its out-of-family pass took
`R1-1`–`R1-3`. This round's findings are `PP18-1`–`PP18-16` and its design
observations `D18-1`–`D18-7`, so a grep is unambiguous. Nothing from round 1
is re-reported.

**Lens count per confirmed finding**, because a finding reached once is
weaker evidence than a finding reached four times:

| finding | dimensions that reached it | lenses applied |
|---|---|---|
| PP18-2 (blank framed screen) | contracts, arithmetic, errorpaths, guards | correctness, reachability ×2, intent — **and one intent refutation**, §6a |
| PP18-3 (exponential DAG expansion) | lifecycle, arithmetic | correctness, intent |
| PP18-5 (ENTER latch across calls) | contracts, lifecycle | reachability, correctness |
| PP18-7 (`varOff` narrowing) | contracts, guards | intent ×2, plus an incidental observation from PP18-10's verifier |
| PP18-15 (stale budget paragraph) | errorpaths, design | intent, correctness |
| all others | one each | one each |

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh --solo` at `f0edd5ee7`,
run independently in eleven isolated verifier worktrees as the baseline for
their probes and mutations: **GREEN every time** (185–257 s, `1/1 testSuite
OK`, `Fail: 0`, `==> PRETTY-PRINT GATE GREEN`). Two of those runs carried a
deliberate mutation and stayed green — that is PP18-12 and PP18-13. Eight
carried added probe fixtures and went red only on the probe's own
deliberately-wrong expectation, with `grep -c "prettyPrint test FAIL"`
confirming the count each time; that is the evidence behind most of §3, and
it also confirms the coverage claims, because every one of those probes
drives a shape no existing pin observes.

I did not re-run the gate as report author. The brief states solo and
combined are green at the tip and the eleven independent runs corroborate
solo; re-running would regenerate `patches/`+`files/` under a working tree
that is not mine to dirty.

**Warnings.** No verifier reported a new diagnostic from any of the ~15
builds run during verification. Taken as given from the brief.

**`design-audit.sh`.** Not applicable to this package (it is forth-core's).
The equivalent here is `tools/pkg_patch_refresh.py`, and the refresh state at
the tip is clean: all 16 `files/` entries and all 13 patches hash-match
`.refresh-manifest.json`, drift 0.

**Patch-surface metrics** (`references/patch_churn_scan.py`, upstream
dimension). PP18 added 921 lines of walker, a new construct and a new test
driver for **two upstream lines of behaviour**: one `funcTest` row appended
to the package's existing `testSuite.c` hunk (hunk count unchanged at 1,
adds 11 → 12) and one `testSuiteList.txt` entry with a five-line comment in
upstream's own annotation style. No new override files, no new forward
declarations pushed into upstream. Hunk-range collision against forth-core:
none — pretty-print's list hunk spans old 501-506, forth-core's spans
507-509, no shared context line. The standing `solver/equation.c` debt
(619 adds / 5 hunks, up from 589/4 at the 2026-08-27 review) predates this
range and is unchanged by it.

**Tree state.** The working tree carries one uncommitted change that was
present before this audit began and is not its doing:
`packages/pretty-print/prettyVisual.c:1314-1322` — the decline branch's two
locals folded into the `sprintf` with explicit casts, plus a one-word
comment edit, with the regenerated `files/` twin and manifest hash.
Cosmetic; it does not move any line cited in §3 (all of which are below
1314). Every probe and mutation in this report was applied, observed and
reverted inside its own worktree.

**One contamination note, recorded because it is a process defect, not a
code one.** Two verifiers wrote console logs to the same `/tmp/baseline.log`
and one clobbered the other's. The affected verifier detected it (the file's
first line named a different worktree), re-verified from its own
worktree-local `build.sim/meson-logs/testlog.txt`, and its verdict stands.
Shared `/tmp` filenames across parallel verifier worktrees are a
round-5-class hazard; verifier scaffolding should write worktree-local.

---

## 3. CONFIRMED findings

Ranked by what they cost the owner. Each carries the reaching input, the
violated contract quoted, the bug class, and the class-level test. No
patches.

---

### PP18-1 — DERIV seeds the body with the `f'` **parameter** name, but upstream samples into the body program's own `MVAR` variable

**Where.** `packages/pretty-print/prettyVisual.c:527` (`ppvBody(ctx, bodyIdx,
v, false, &body)` with `v` = the `f'` parameter, unconditionally), the
faithfulness argument at `:490-497`, and `:613` (`case ITM_MVAR:` in the
"declarations, comments" skip arm — the declaration that decides the answer
is invisible to the walk).

**What breaks.** The drawn derivative names a variable the engine does not
differentiate with respect to. The walker draws `d/dv(body)|v=3` because the
program wrote `f' 'v'`; upstream differentiates with respect to whatever
`deriv_pgm_variable(label)` extracts from the **body program's** `MVAR`
declarations. The two agree only when the body declares an `MVAR` equal to
the `f'` parameter.

**Reaching input.** Both halves built as fixtures and run in the real binary
at the audited tip — upstream numerically through `fnPgmDrv` +
`fn1stDerivVar`, VISUAL through the product node path:

```
LBL 'MBA'                   LBL 'MDA'
  RCL 'v'                     PGMDRV 'MBA'
  ENTER                       3
  ×                           f' 'v'
  END                         END
```
`MBA` declares no `MVAR`. **XEQ 'MDA' returns 0** (`PROBE-XEQ MBA -> 0E+1`,
err 0): `derivativeVariable` STOs 3 into `v` once, `calcDeriv` then asks
`deriv_pgm_variable` for the sampling variable, gets `INVALID_VARIABLE`, and
`_differentiatorIteration` does **no per-sample STO at all** — `v` stays 3,
every sample is 9, the derivative is 0. **VISUAL draws `DERIV(v×v;v;3)`,
which means 6.**

Give the body an `MVAR` that is not the parameter and the error reverses:
`LBL 'MBB' / MVAR 'y' / RCL 'y' / ENTER / ×` driven by `f' 'v'`.
`deriv_pgm_variable` returns `y` (the first declaration, since none equals
`currentSolverVariable`), **upstream returns 6**, and **VISUAL draws
`DERIV(y×y;v;3)`, which means 0.**

**Violated.** `prettyVisual.c:490-497`, which states the rule as a
measurement rather than an analogy: *"The seeding rule is the integrator's,
and that is a measured claim, not an analogy: `_differentiatorIteration`
fills every stack level with the sample point AND stores it into the named
variable … **Identical to `DEI_xeq_user`**."* It is not identical.
`DEI_xeq_user` (`integrate.c:411-423`) writes into `regist`, and
`_fnIntegrate` sets `regist = labelOrVariable` — the ∫'s own parameter, so
INTEG's seeding by parameter name is exact. `_differentiatorIteration`
(`differentiate.c:326-338`) writes into `variable`, which `calcDeriv`
obtained from `deriv_pgm_variable(label, &usesDelta)` at
`differentiate.c:438`; that function (`:286-324`) walks the label's leading
`ITM_MVAR` steps and returns the one matching `currentSolverVariable`, else
the first declared, else `INVALID_VARIABLE`. And above all the premise the
whole subsystem is built on: the walker must never draw something that means
something different from what the program computes.

**Bug class.** *Measured one call short* — an invariant verified at the
callee and assumed at the caller. Compounded by *the only fixture satisfies
the assumption the code never checks*: `pgmDB` declares `MVAR 'x'` and is
driven by `f' 'x'`, the single configuration in which the two channels
agree, so all of V52–V56 pass a rule that holds only for them.

**Class test.** Enumerable, 3 × 2. For each of {body declares no `MVAR`,
body declares a different `MVAR`, body declares the matching `MVAR`} × {`f'`
names it, `f'` names another}, assert the drawn subscript equals the register
`deriv_pgm_variable` actually returns — not the parameter — and decline where
it returns `INVALID_VARIABLE`, because there is no honest picture for
"upstream never varies anything". The stronger oracle already exists in this
battery: V18's shape, extended to DERIV — run the program numerically, feed
the walker's own transpiled formula to `fnEqCalc`, assert the two agree.
That oracle catches this whole class and would have caught it here.

**Verification.** SURVIVES, reachability lens, by execution of both channels.
Every attempted refutation failed: neither shape declines; the wrong name
reaches the **drawn** subscript, not merely the PC_BUILD text seam (the same
`ppvConstruct` node feeds `ppqBuildBigop`'s `varTiny`/`varCtx` at
`prettyVisual.c:893-906`); and both programs executed upstream with `err 0`,
so neither is degenerate. With the two probes added the gate showed exactly
two failures — the probes' own deliberately-wrong expectations — confirming
no existing pin observes this shape.

---

### PP18-2 — a layout failure paints a blank framed screen: no formula, no D-number, and the X answer erased

**Where.** `prettyVisual.c:1240` (`lcd_fill_rect(0, 16, …)` — the clear runs
*before* the function knows it can draw), `:1249` (`break` on `PP_NONE`),
`:1244-1258` (`continue` on a measure refusal, both rungs), `:1266-1269` (the
self-painted-screen state committed unconditionally), and the justification
at `:1235-1238`. Contradicted by its own caller 27 lines up, `:1211-1212`.

**What breaks.** The owner presses VISUAL and gets the full-screen frame —
two full-width rules at y=20 and y=168 — with **nothing between them**, no
error, no D-number, and X's answer wiped by the fill. `lastErrorCode` stays
`ERROR_NONE`, `screenHoldsDrawnPixels` is set and `temporaryInformation =
TI_SHOWNOTHING`, so the blank is held until EXIT. PP17 always showed
something here.

**Reaching input.** Four routes, all measured on the product entry point
`fnPrettyVisual`, all with `err=0`:

| program | why layout refuses | measured |
|---|---|---|
| `LBL 'Z' / RCL x /` **13 × `1/x`** | each `ITM_1ONX` is one `PP_FRAC` (`prettyFormula.c:225`), so the leaf runs sit at depth 13 and `ppMeasure` refuses at `depth > PP_MAX_DEPTH` (12, `prettyLayout.c:212`) | `nodesBuilt=1 measured=0 interiorInk=0 ruleInk=800 err=0 held=1 ti=93`; the 9× control draws (`interiorInk=243`) |
| `LBL 'A' / 1 / ENTER /` **14 chained `÷`** | RPN left-nests, 14-deep `PP_FRAC`; transpiles cleanly to `1/2/3/…/6` first | `laidOut=1 nodesUsed=29 measure=0`; rows 22..166 empty |
| twelve additive factors `(a+b)·(c+d)·…` | 47 AST nodes — inside the 48-node arena, and the walk proves it by transpiling — but the paren boxes push layout past `PP_POOL_NODES` = 72 | `buildNodes laid=0`, `wholeInk=15238000`, which is *arithmetically exactly* the two rules and nothing else |
| `LBL 'P' / RCL x /` **5 × (`ENTER`, `×`)** — x³² by repeated squaring | the ENTER DAG re-expands: N(k)=2·N(k−1)+2 → 4, 10, 22, 46, **94** against 72 | `nodesBuilt=0 interiorInk=0 ruleInk=800`; the 4-pair control (N=46, x¹⁶) draws normally in the stack window with the X line untouched |

The threshold is crisp in both directions and the controls prove the
measurement is not an artefact.

**Violated.** The comment that argues the case away, `prettyVisual.c:1235-1238`:
*"no linear fallback beneath it, because a tree that failed to lay out here
failed on **SIZE**, and there is nothing smaller left to try."* Neither the
depth cap nor `ppNewBox` returning `PP_NONE` is a size failure, and its own
caller says so at `:1211-1212` — *"the layout pool ran out; the tree itself
is sound"* — and again at `:1346`, *"taller than the two rows, **or the
layout pool gave out**"*. Because `ppvPaintFullScreen` re-runs the
byte-identical rung 0 after `ppReset`, over the same 72-node pool and the
same depth cap, a non-size failure is **guaranteed** to recur: the retry is
provably futile rather than a last chance. `DESIGN.md:683-685` still states
the opposite as a ruling — *"a formula the 2D grammar declines … still shows
in the window, linear and centred in it"* — as does this file's own header
comment at `:1191-1193`, and the house convention next door,
`prettyEquation.c:909-914`: *"always show SOMETHING: the linear line,
centered-ish"* (pinned EQ9). The decline catalog's contract that every
failure mode has a D-number the user sees is bypassed rather than applied:
no `ppvDecline` call exists on this path.

**Bug class.** *An implicit bound removed with its carrier, retired on a
rationale that covers only one of its causes.* `DESIGN-HISTORY.md`'s PP18
entry rules the deletion: *"The linear fallback became unnecessary. It
existed because `ppqParse` could decline. Tree-to-nodes cannot."* True for
grammar decline. False for pool exhaustion, the depth cap, over-width and
over-height, which the same commit's own comments name.

**Class test.** Enumerable — there are exactly five ways the paint path can
fail (node pool, text-byte pool, depth cap, width, height). For each, assert
**either something is drawn or `lastErrorCode != ERROR_NONE`**, and
specifically that no path leaves the band between the rules empty with a
clean error code. A pixel-sum probe over rows 22..166 is the oracle; the
verifiers wrote it four times independently.

**Verification.** SURVIVES on four lenses across four dimensions. One
intent verifier **refuted** a narrower framing of the same claim; the
disagreement is recorded in full in §6a because it is the most interesting
argument in this round, and because the majority rule that carried the
finding is the process working, not a formality.

---

### PP18-3 — `ppvAstToNodes` re-expands the ENTER DAG with no early abort, so layout work is 2^k and VISUAL hangs

**Where.** `prettyVisual.c:863-869` — the `PPA_OP2` arm recurses into
`child[0]` **and** `child[1]` unconditionally, and the `if(x == PP_NONE ||
y == PP_NONE)` test is evaluated only *after* both calls return. Reached
from `:624`, where `ITM_ENTER` pushes `stk->ast[stk->depth - 1]` — the same
index — so every `×` after a dup has `child[0] == child[1]`.

**What breaks.** Visits double per level with nothing to stop them. Measured
by instrumenting the function and driving synthesised programs through the
official loader: **visits = 2^(k+1) − 1, exactly**, for `LBL 'VXk' / 2 /
k × (ENTER, ×)`:

```
EXP k= 4 visits=       31 ok=1        EXP k=15 visits=    65535 ok=0
EXP k= 5 visits=       63 ok=0        EXP k=18 visits=   524287 ok=0  ms=2.9
```

The decisive datum is `ok=0` from k=5: the 72-node layout pool is exhausted
and **the walk keeps doubling anyway**, because `ppNewRun`/`ppNewBox`
returning `PP_NONE` is a per-call return value, not a latch. Recursion depth
is only k+1, so there is no stack overflow to abort it. The 48-node arena
admits k ≤ 46 and the 256-step budget k ≤ 127, so the worst case inside the
walk's own caps is 2⁴⁷. At the measured 5.5 ns/visit on this desktop, k=40 is
≈3.4 h here and one to two orders worse on an 80 MHz DM42n. And the price is
paid **twice** per VISUAL: rung 0 of `ppvPaintStackWindow`, then again in
`ppvPaintFullScreen`.

**Reaching input.** `LBL 'SQ' / 2 /` then `ENTER`, `×` repeated 40 times,
then VISUAL. The walk itself is clean — 41 of 48 arena nodes, 81 of 256
steps, 1 pool byte — and returns a root. At k≈20 it is a visible
multi-second stall, at k≈30 minutes, at k=40 it never returns: screen frozen
mid-command, no error, no keyboard, hardware reset, and whatever program or
entry was in progress is lost.

**Violated.** The justification for the DAG, `prettyVisual.c:621-622`:
*"the dup shares the operand node; the tree is a DAG here and both back ends
only ever read it"*, restated in commit `47f6e609b` — *"which is sound
because both back ends only ever read it"*. Read-only-ness is mutation
safety, a different property from bounded work; reading a shared node is
exactly what costs 2^k. PP17 had no exposure: `ppvAppendOperand`/
`ppvEmitBinary` materialised the operand **text** during the budgeted walk,
and the doubling hit `PPV_FRAG_MAX` = 255 or the pool cap after ~7–8
repetitions and declined cleanly with a D-number
(`git show 1fd492a48:…prettyVisual.c:108-117`, *"a binary op leaks its
operands' bytes, which the pool cap bounds with a clean decline"*). Also the
decline-catalog contract: every failure mode has a D-number the user sees.
This one has no failure mode at all.

**Bug class.** *A bound removed with the representation that carried it*
(shared with PP18-2), crossed with *a guard evaluated after the work it
guards*. The design deliberately requires re-expansion — `DESIGN.md:662`,
*"`ENTER ×` transpiles to `x×x`, not `x²`. The walker transpiles structure,
never intent"* — so the sharing is intended and only the compounding is
unruled. This is not an argument for memoising the DAG; it is an argument
for a visit budget or an early abort.

**Class test.** Instrument the visit count and assert it is linear in the AST
node count for k = 1..8 of `ENTER ×` (the shape where the two diverge
fastest), plus a wall-clock ceiling on the worst program the arena admits.
The mutation that proves the pin works is to delete the abort and watch the
count double.

**Verification.** SURVIVES on two lenses. The correctness verifier's
measurement above; the intent verifier searched `DESIGN.md` §6, the PP18
`DESIGN-HISTORY` entry, `TESTING.md` (MUT-108..114, V46..V57), all five
commit messages and the code comments, and found the sharing ruled and the
cost never mentioned — the only two budgets that exist, `PPV_STEP_BUDGET`
and `PPV_AST_NODES`, are both spent *before* `ppvAstToNodes` is entered. One
finder dimension had cleared this by reading ("it terminates cheaply — a
leaf's `ppNewRun` fails first and every parent aborts on its first `PP_NONE`
child"); the measurement refutes that reading, and it is exactly the kind of
plausible-but-wrong clearance a mutation is for.

---

### PP18-4 — a Σ/∏/∫/derivative reports ATOM precedence, so squaring a construct draws the exponent on the construct's **body**

**Where.** `prettyVisual.c:825` sets `*outPrec = PPF_PREC_ATOM` as the
function-entry default; the `PPA_CONSTRUCT` arm at `:873-907` never assigns
`*outPrec` on any exit. Consumers: `prettyFormula.c:237`
(`ppfWrapIf(a, aPrec, PPF_PREC_ATOM)` for SQUARE/CUBE) and `:155`
(`ppfWrapIf(a, aPrec, myPrec)` for MULT) — ATOM ≥ both, so no `PP_PAREN`.

**What breaks.** Two programs whose answers differ by a factor of 2.6 draw
the same picture, and the picture is the wrong one for the first:

```
LBL 'A': 1 / 3 / 1 / Σ 'B' / x²      computes (1+2+3)² = 36
LBL 'C': 1 / 3 / 1 / Σ 'D'           computes 1²+2²+3² = 14   (D squares the counter)
```
Measured node signatures from the product tree: `S(B(n|[n = 1]|3)|2)` and
`B(S(n|2)|[n = 1]|3)` — the bigop sits directly under `PP_SUP` with no
`P(…)`. Rendered and compared pixel by pixel: the `n` glyph occupies columns
22-29 and the `2` glyph columns 33-38, at the same raise, in **both**; both
roots are 39 px wide. The only difference is the Σ stroke being 4 px taller
in the second (`ppBigopBox` sizes the glyph off the body's height), which no
reader can read as a bracket.

The same root cause hits multiplication: `1 / 3 / 1 / Π 'B' / 2 / ×`
computes (1·2·3)·2 = 12 and draws `Π(n=1..3) n·2`, measured sig
`[B(n|[n = 1]|3) · 2]`, which reads 2·4·6 = 48.

**Violated.** The guard four lines above, `prettyVisual.c:848-853`:
*"`ppfCombine` has no POW level — a `PP_SUP` scopes itself, so it reports
ATOM and a stacked power would come out unbracketed … Bracket the base
ourselves."* Its conjunct list at `:854-857` is
`(item == ITM_SQUARE || item == ITM_CUBE) && ctx->ast[child[0]].kind ==
PPA_OP1 && (…)` — `PPA_CONSTRUCT` is not in it, though a `PP_SUP` scopes a
bigop even less than it scopes a nested power. `DESIGN.md:570-575`:
*"a stacked power DOES need its base bracketed, which the walker does
locally"*. V51 pins only the OP1-over-OP1 member (`S(P(S(a|2))|2)` — note the
`P(…)` the construct case does not get); no pin puts a construct under a
power or left of a `×`, and there is no node pin for a product at all.

**Bug class.** *A guard whose conjunct list enumerates the examples in front
of it rather than the class.* The author found this class once, at the
immediately preceding line, and did not extend it to the member the same
commit introduced — the recurrence shape the bug-class catalog warns about.

**Class test.** Fully enumerable: {LIT, VAR, OP1, OP2, CONSTRUCT, CALL} as
the base of SQUARE/CUBE and as the left operand of MULT/DIV — 6 × 4 node-sig
pins asserting a `P(…)` appears exactly when `ppfCombine`'s contract requires
one. Half of them are already implied by existing pins.

**Note, and it lowers the severity slightly.** The equation parser has the
identical hole — `ppqFactor` (`prettyEquation.c:557-580`) wraps `ppqPrimary`'s
bigop in `PP_SUP` with no parens — so `SUM(X;X;1;3)^2` typed into EQN has
drawn this way since PP14. PP18 inherits the defect rather than introducing
it. What PP18 *did* introduce is the walker's own local copy of the
bracketing guard, one line above, which stops short of the case.

---

### PP18-5 — the ENTER lift latch survives XEQ, PGMINT and PGMDRV, which upstream's SLS epilogue clears

**Where.** `prettyVisual.c:740-746` (`ITM_XEQ`), `:748` (`ITM_PGMDRV`, new
this stage) and the `ITM_PGMINT` arm — all three `return` before the
epilogue at `:772`, `stk->liftDisabled = false; // every op above finishes
with lift enabled`.

**What breaks.** Two consequences, and the second is the serious one.

*At top level, a false decline.* `LBL 'M' / RCL 'a' / ENTER / XEQ 'B' / × /
RTN` with `LBL 'B' = RCL 'b' / + / RTN` computes a·(a+b) on the calculator.
Measured: **`declined D10 at step 6`** — a stack-underflow refusal for a
program that never underflows. The control with the XEQ removed declines at
step 5, so the shortfall is attributable to the interposed step alone. Same
with PGMDRV and PGMINT: `RCL a / ENTER / PGMDRV 'F' / 2 / × / +` declines
D10 at step 6 against a baseline of step 5.

*Inside a construct body, a silent wrong drawing.* Where `ppvBody` has
pre-seeded all eight levels there is no underflow to catch it. Measured:
the same subroutine call inside an integrand draws **`INTEG(x×(a+b);x;0;1)`**
for a program that computes a·(a+b). The seeded frame supplies the phantom
operand, no decline fires, and the picture means something else. Inline the
callee and both models agree — the divergence is the XEQ arm alone.

**Violated.** The epilogue's own claim at `:772`. Upstream: a `PARAM_LABEL`
step is dispatched by `_executeOp` (`lblGtoXeq.c:369-379`) into
`reallyRunFunction`, whose no-error epilogue (`items.c:603-610`) runs
`setSystemFlag(FLAG_ASLIFT)` for `SLS_ENABLED`; `ITM_XEQ` (`items.c:1821`),
`fnPgmInt` (row 1546) and `fnPgmDrv` (row 2882) are all `SLS_ENABLED`, while
ENTER is `SLS_UNCHANGED` (`:1853`) and `fnKeyEnter` itself clears
`FLAG_ASLIFT` (`keyboard.c:3439`). The latch is one-shot and any of these
three ends it. Also `DESIGN.md`'s fail-closed reasoning, *"the item table
carries no stack-effect metadata to infer from"*: for stack **lift**
specifically the table does carry it, in the `SLS_STATUS` bits the walker
never consults.

**Refuted sub-claim, recorded because one finder got it wrong.** `ITM_LBL`
is **not** an instance. Its row carries `PTP_DECLARE_LABEL`, and
`executeOneStep`'s default arm (`lblGtoXeq.c:826-828`) returns 1 for that
class without calling `runFunction`, so an interior LBL never reaches the SLS
epilogue and does not set `FLAG_ASLIFT`. The walker's LBL arm agrees with
upstream, and the dispatch comment at `:611-612` is correct for
LBL/MVAR/REM/PAUSE/SNAP. `SLS_ENABLED` being the 0-valued default on a row
that is never executed is exactly the trap here.

**Bug class.** *Hand-modelled metadata where the table carries it*, crossed
with *copied-shape recurrence*: PP18's new PGMDRV arm reproduces PP17's
PGMINT arm verbatim, including the early `return`.

**Class test.** Machine-derived, and therefore worth doing properly: for
every opcode the walker dispatches, assert its `liftDisabled` post-condition
matches `indexOfItems[op].status & SLS_STATUS`, with `PTP_DECLARE_LABEL`
stated as the one exception and eRPN declared out of scope. The enumeration
comes from the table, not from a list somebody maintains. V33/V38 pin the
latch for shapes with no call in them; no pin crosses ENTER with a call (V30,
the "serial XEQ chain", has no ENTER).

**Verification.** SURVIVES on two lenses, both by execution. The severity is
`latent` only in the sense that it needs a subroutine call after an ENTER;
the body case is a wrong drawing with no warning, which is the same
prohibition as PP18-1.

---

### PP18-6 — the walker scopes a DERIV body the parser deliberately exempts, so an additive derivative body draws in doubled parentheses

**Where.** `prettyVisual.c:888` — `body = ppfWrapIf(body, pBody,
PPF_PREC_MUL, ctxFont)` runs **before any kind discrimination** (`isInt` and
`isDrv` are computed on the following lines), so DERIV goes through it like
every other construct. `ppqBuildBigop`'s DERIV arm then wraps the body again
in its own `PP_PAREN` (`prettyEquation.c:249, :277`).

**Reaching input.** `LBL 'D' / PGMDRV 'G' / 3 / f' 'x'` with
`LBL 'G' = MVAR 'x' / RCL 'x' / x² / RCL 'x' / +`. Measured node signature:
`[F(d|[d x]) U(P(P([S(x|2) + x]))|[x = 3])]` — `P(P(…))`, two nested tall
parenthesis pairs. Typed into EQN as `DERIV(x^2+x;x;3)` the same formula
gets exactly one.

**What it costs.** Doubled parentheses do not change the meaning, so this is
a drawing defect rather than a meaning defect — which is why it sits here
rather than higher. But the extra pair inflates the measured height, so a
derivative that would have fitted the 72-px Z/T band can fail the test at
`:1221` and fall through to the full-screen view, which destroys the X answer
the feature exists to keep visible beside the formula. (The height half was
not measured; it was outside the assigned lens.)

**Violated.** `prettyEquation.c:399-404` is explicit that DERIV is exempt:
`if(kind != 2) { body = ppqScopeBody(c, body, font); }`, guarded because the
DERIV shape already parenthesises. `TESTING.md:541` states the two must
agree: *"V57 pins that an additive construct body is still scoped — the
parser sniffs runs for a `+`/`-`, the tree asks the precedence, and **they
must agree**."* For kind DERIV they do not. V56 is the only derivative node
pin and its body is `x·x` (MUL level), where `ppfWrapIf` is a no-op; V57's
fixture is an integral.

**Bug class.** *Duplicated truth with a pin on only one member* — two
implementations of one rule, an explicit doc claim that they agree, and a
pin whose fixture cannot distinguish them.

**Class test.** 4 × 3: for each construct kind {SUM, PROD, INTEG, DERIV} ×
each body precedence {ADD, MUL, ATOM}, assert the walker's node signature
equals the parser's for the same formula. That is the agreement
`TESTING.md` claims, expressed as a table, and it is small enough to write.

---

### PP18-7 — `varOff` is a `uint8_t` index into a 512-byte pool

**Where.** `prettyVisual.c:448`, `ctx->ast[n].varOff = (uint8_t)voff;` with
no range check; the field declared at `:75` beside its correctly-sized
sibling `uint16_t textOff` at `:72`; `ppvIntern` (`:143-155`) gates only
against `PPV_POOL_BYTES` (512) and returns a `uint16_t`. Readers:
`:897`/`:900` (`ppNewRun(ctx->pool + a->varOff, a->varLen, …)`, the drawn
path) and `:1101` (the text seam).

**What breaks.** Past pool offset 255 the construct's variable name is read
from 256 bytes lower in the pool — another leaf's text. The integral draws
`∫ … d<some other variable or numeral>`. Nothing is out of bounds
(offset + len stays inside 512), so there is no crash, no decline and no
D-number: a well-formed integral naming a variable the program never
integrates over.

**Reaching input.** Two, and one of them was **observed**, not constructed.

*Observed.* During PP18-10's verification — a probe that legitimately fills
the pools — the drawn counter came out as `4` and `2` instead of `n`, in the
node signature taken from the product tree (`|[2 = 1 , 777…]|9)` where `n`
was expected). That is this defect firing in the wild, on a program built for
an unrelated reason.

*Constructed.* Four nested integrals over 7-character variable names (7 is
upstream's maximum, `registers.c:812`, *"The length must be from 1 to 7
glyphs"*), in the appnote-22 idiom. `ppvBody` interns **eight** copies of the
name per frame (PP18-8) = 56 bytes per level; four levels plus limits and
recalls reaches `poolUsed` ≈ 250 as the innermost body is walked. The
innermost construct interns at 250 and fits; the next one out interns at 257
and stores `(uint8_t)257` = 1. The AST cost is 44 of 48 nodes and the call
depth 4 of 5, so neither documented cap fires first.

**Violated.** The field's own declaration against its sibling: two offsets
into the same 512-byte array, two widths, both written in commit
`47f6e609b`, which in the same message deliberately states *"Pool halves to
512 B since composed text no longer lands in it"* — so the pool size is a
considered decision and the 8-bit index into it is discussed nowhere. Both
intent verifiers searched `DESIGN.md` (including the Rulings and Budget
blocks), `DESIGN-HISTORY.md`, `TESTING.md`, all five commit messages and the
code comments: `grep -rn "varOff" --include=*.md .` returns **zero hits
repo-wide**. The decline catalog names the two boundaries the design meant to
defend, D8 (depth 5) and D16 (pool 512); 255 is not one of them. The project
records such narrowings when it makes them deliberately — round 1's A13 ruled
the register tag's `uint8_t` lossless in exactly this form.

**Bug class.** *Silent narrowing before a range gate.* The gate exists
(`ppvIntern`'s 512 check) and the cast happens after it, in a different
function, at a different width.

**Class test.** Fill the pool to `poolUsed` ∈ {254, 255, 256, 257} before a
construct interns its name and assert the drawn variable run equals the
program's name. Plus the compile-time form, which is the one that scales: a
static assertion that every pool-offset field is at least as wide as
`PPV_POOL_BYTES` requires. There are two such fields; one is right.

---

### PP18-8 — body seeding allocates eight arena nodes where its own comment says one shared node, and refuses a formula it can draw

**Where.** `prettyVisual.c:405-411` — `ppvLeaf(…)` is **inside** the
`for(i < PPV_STACK_SLOTS)` loop, under a comment at `:406-407` reading
*"one shared VAR node on every level: the seeding is about what the program
can READ, and it reads the same variable from all of them"*.

**What breaks.** Eight arena nodes and eight interned pool copies per
construct frame instead of one. Four nested integrals over the appnote's own
`x·x − x·p − 2` integrand spend 32 of the 48 arena nodes on four sets of
eight identical VAR leaves and **decline D16 (`PPV_D_ARENA`) at step 25** —
not D8, not the documented depth budget, which explicitly permits four levels
(`PPV_MAX_DEPTH` = 5; `callDepth` tops out at 5 inside the fourth body). With
the seed hoisted out of the loop and the same index pushed eight times, the
identical program **transpiles and lays out**, and the whole solo gate stays
green with zero pin failures — so nothing depends on the eight nodes being
distinct, and nothing in either back end mutates an AST node.

The three-deep control — the shape appnote 22 actually contains — sits at
**41 of 48 nodes, 24 of them duplicate seeds**. Seven nodes of headroom stand
between the documented appnote shape and a false refusal.

**Reaching input.** The four-level chain above (`INTEG(INTEG(INTEG(INTEG(
x×x−x×p−2;x;0;y);y;0;z);z;0;w);w;0;2)`), measured both ways.

**Violated.** The comment on the line, which describes the correct design;
the code does not implement it. Sharing is already established as safe on the
identical shape — `ITM_ENTER` pushes one index twice *"because both back ends
only ever read it"*.

**Bug class.** *Comment/code mismatch where the comment is right.* Its cost
is not only the refusal: it multiplies pool consumption by eight, which is
what carries PP18-7's 256-byte line into reach at ordinary nesting depths
instead of never.

**Class test.** Assert the arena cost of a construct frame is O(1) in the
stack-slot count: a pin on nodes-used for a 1-deep and a 4-deep construct,
plus the 4-deep integral chain as a positive draw case. The mutation that
proves the pin works is to put the allocation back inside the loop.

---

### PP18-9 — the invented Σ counter is checked only against enclosing constructs, so a sum can bind its own upper limit

**Where.** `prettyVisual.c:548-555` — the candidate loop tests only
`ctx->binding` (the enclosing constructs), which is empty at top level; the
VAR node for the real variable was already built at `:700` and is never
consulted. The RCL-side shadow guard at `:707-712` only fires while
`ctx->bindingCount` is non-zero, i.e. inside a body.

**Reaching input.** With a named variable `n` holding the loop count:
`LBL 'S' / 1 / RCL n / 1 / Σ 'SQ'` where `LBL 'SQ' = ENTER / × / END`.
Measured: **`SUM(n×n;n;1;n)`**. The counter is invented as `n` — the first
candidate and the most natural name for a user's loop count — while the free
`n` sits in the upper-limit slot of the operator that binds `n`.

**What breaks.** The picture shows `Σ` with `n = 1` underneath and `n` on
top: an upper limit that reads as bound by its own operator. Nothing on
screen distinguishes the free variable from the counter.

**Violated.** `DESIGN.md`'s ruling for this construct: *"A sum's counter name
is invented (first free of `n`, `m`, `k`, `j`) because RPN has none, and a
body that recalls a real variable spelled the same way **DECLINES** rather
than let the invented name shadow it (V6)."* The rule is written about the
body because that is where the implementation happens to look; the shadowing
it forbids is a property of the whole drawn formula, and the limits are drawn
inside the operator's visual scope.

**Bug class.** *A scope rule implemented at the site where it was noticed
rather than over the scope it names.*

**Class test.** Extend V6's rule to the AST: before choosing a counter,
reject any candidate that appears as a `PPA_VAR` anywhere in the tree built
so far. The limits are already built when `ppvSumProd` runs, so the check is
available; four candidates × {appears in limits, appears in body, appears in
neither} is the pin table.

**Not part of this finding.** `Σ(n·n, n=1..5) + n` — the same name free
*outside* the operator, measured as `SUM(n×n;n;1;5)+n` — is standard
bound/free notation and the evaluator scopes it the same way. The design
dimension cleared it independently; see §6b.

---

### PP18-10 — `step` is the one construct operand `ppvAstToNodes` does not check for `PP_NONE`

**Where.** `prettyVisual.c:877-879` builds it; `:881-884` validates `body`,
`from` and `to` (and deliberately excuses `to` for DERIV); `:903-906` passes
`step` **unvalidated**. `ppqBuildBigop` reads `stepN == PP_NONE` as "there is
no step" and omits the `,Δ` tail (`prettyEquation.c:214-223`) — it has no way
to tell a missing step from a failed one.

**What breaks.** A Σ/∏ that summed with a non-unit step draws with no step
tail — the picture asserts a unit step — and reports success. Every other
operand in the same expression turns the same failure into a clean decline.

**Reaching input.** Constructed and measured, and legal: no corruption, no
hardware fault, just a program a user can type or load. With the layout text
pool (512 B) nearly consumed by an accepted body, `from` and `to`, the step
subtree's `ppNewRun` fails on bytes while ~168 bytes remain — enough for the
tiny counter run and `ppqBuildBigop`'s `"="` run, which is what keeps the
construct alive. Fixture: a body of eight 40-digit literals joined by seven
`÷` (nested fractions, so the picture stacks rather than running off screen)
plus `+0` three times; caller pushes 1, 9 and a 172-digit step, then `Σ`.
Node signature from the product tree: **`…|[2 = 1]|9)`** against the
20-digit-step control's `…|[2 = 1 , 77777777777777777777]|9)`.
`ppvTranspile` on the same label still emits the 172-digit step, so the AST
is intact and only the drawn path lost it; `fnPrettyVisual` returns `err=0`
and the picture reaches the glass (band ink 132,209,722). An earlier probe
with a flat additive body and a 65-digit step reproduced the same loss, so
the window is a ~10-byte band any body/step size pair can be tuned into, not
a knife edge.

**Violated.** The asymmetry against its own siblings three lines above, and
`prettyFormula.c:138-141`'s already-paid-for lesson from round 4 (R4-3):
*"`ppfParen` allocates, so it can fail… An unchecked append here renders
'log2' with its argument silently absent, reported as success."* The text
back end gets it right — `ppvSerialize` tests `a->child[3] != PPV_NIL` on the
**AST** (`:1116`) rather than on a layout result — so only the product path
is exposed.

**Bug class.** *An already-named class recurring at a new site.* R4-3 named
"allocation failure read as absence"; this is the same sentence with a
different operand.

**Class test.** Fault injection, five cases: for each sub-allocation in the
`PPA_CONSTRUCT` arm (`body`, `from`, `to`, `step`, `varTiny`/`varCtx`), force
`PP_NONE` and assert the arm returns `PP_NONE` rather than drawing something
with an operand missing.

**Confidence note.** The reaching input is contrived — a 172-digit step over
a 320-digit body — which is why this sits at #10 and not higher. The *guard
asymmetry* is not contrived, and it is the thing to fix.

---

### PP18-11 — `prettyTestReal` checks the input `fopen` but not the output one; the suite segfaults instead of reporting

**Where.** `packages/pretty-print/prettyTest.c:4535`,
`FILE *outF = fopen("c47programTest.bin", "wb");` used at `:4538`
(`fputc(ch, outF)`) and `:4541` (`fclose`), with no NULL check — while the
sibling `fopen` two lines earlier at `:4530` **is** checked.

**What breaks.** Measured by mutation: with that `fopen` pointed at a
nonexistent directory, the run dies `killed by signal 11 SIGSEGV` inside
`prettyTestReal`, and `graphs_cov`, `nested_cov`, `config_cov` and
`stack_cov` never execute. The log's last progress line does name the driver,
so attribution is not as bad as first reported, but the run is aborted rather
than a case failing.

**Violated.** The convention both upstream and this file already follow.
Upstream checks the identical `fopen("c47programTest.bin","wb")` at
`testSuite.c:1171` and `:1624` and calls `abortTest`; this file checks it at
`:783` and calls `ppTestFail`. One call site out of three deviates, and it is
the one added by the in-range commit (`git log -S` on the exact line →
only `f0edd5ee7`).

**Bug class.** *A convention followed everywhere but the newest site.*

**Reachability, honestly.** The concrete conditions originally offered (repo
root not writable, the name taken by a directory) would trip upstream's
**checked** writer at `testSuite.c:1171` far earlier in the list and abort
cleanly, so this is a robustness/convention gap rather than a crash anyone
reaches today. It is one line and costs nothing to close.

**Refuted half, recorded.** The claim that the `in == NULL` branch skipping
both `fnClPAll(CONFIRMED)` calls contaminates later drivers is **wrong**, and
was disproved by mutation: running that branch for real leaves exactly one
failing case, `pretty_visual_real` itself, attributed to
`pretty_visual_real.txt line 14`, with `graphs_cov` and `nested_cov` both
passing. The clears are inert on that branch because `fnLoadProgram` was
never reached, and the named contamination source is impossible —
`prettyTestVisual` runs from `pretty_print.txt` at list line 270, three
clearing events (including `serialize_cov`'s full calculator reset) earlier.

**Class test.** Not a class; it is a grep. Every `fopen` in the package's own
sources is checked — there are two.

---

### PP18-12 — the second-order-derivative flag's node wiring has no pin; MUT-111 is guarded only in the text seam

**Where.** `prettyVisual.c:906`, `(a->flags & PPV_F_SECOND) != 0` as
`ppqBuildBigop`'s `secondOrder` argument. Hardwiring it to `false` and
running the full solo gate: **`1/1 testSuite OK`, `Fail: 0`,
`PRETTY-PRINT GATE GREEN`.** Nothing in the suite reads the node tree of a
second-order derivative.

**What breaks (if it regresses).** The argument is load-bearing: in
`ppqBuildBigop`'s DERIV arm `secondOrder` selects the numerator run
`"d\xa1\x62"` versus `"d"` and appends the squared glyph to the denominator
box. Dropped, a program computing d²/dx² draws as d/dx — a different
quantity, silently — and the suite stays green, so the regression ships.
Today the line is correct; this is a hole in the net.

**Reaching input for the resulting defect.** `LBL 'D2' / PGMDRV 'DB' / 3 /
f" 'x' / RTN`, then VISUAL. The product path is
`fnPrettyVisual → ppvRun → ppvPaintStackWindow/ppvPaintFullScreen →
ppvAstToNodes → :906`.

**Violated.** `TESTING.md:283` asserts the guard exists: *"| MUT-111 | the
second-order flag dropped | V53 |"*. V53 (`prettyTest.c:4070`) is a
`ppvTestExpect` → `ppvTranspile` → `ppvSerialize`, which re-reads
`PPV_F_SECOND` independently at `:1107`; `DESIGN.md` now describes that back
end as a test seam the product never uses, and `prettyTest.c:4427` states
that the node pins are the ones covering *"the tree the product actually
paints"*. The node block's eight cases contain exactly one derivative,
V56/VDRV, first order.

**Nuance that keeps the doc honest.** MUT-111 as *literally worded* — the
flag dropped at its assignment, `:450` — **would** be caught by V53. The
`TESTING.md` row is not vacuous. What is unpinned is the flag's *consumption*
on the product side.

**Bug class.** *A pin on the seam rather than the product* — the general form
of PP18-16.

**Class test.** One row: `VDR2` in the V46-V57 node block with V56's
signature amended for the squared glyphs. The general form is worth stating
because it is enumerable: every AST field the layout arm reads (`item`,
`flags`, `child[0..3]`, `varOff`/`varLen`) needs at least one node pin that
fails when it is dropped — eight mutations, eight pins.

---

### PP18-13 — the node signature erases a `PP_BIGOP`'s operator tag, so no node pin can tell an integral from a sum from a product

**Where.** `packages/pretty-print/prettyTest.c:1849-1856` — the `PP_BIGOP`
arm of `ppfTestSigNode` emits `B(body|under|over)` and never reads
`nd->textOff`, which is where `ppSetBoxTag` stores the operator item
(`prettyLayout.c:154`) and where `prettyLayout.c:711-741` reads it to choose
between `ppDrawIntegralSign`, the ∏ strokes and the Σ strokes. Contrast the
`PP_RUN` arm at `:1780`, which does read `textOff`.

**Mutation.** Change the `tag` argument at `prettyVisual.c:904` from
`a->item` to `ITM_PIn`, verify it reached the compiled shadow, run the solo
gate: **GREEN, `grep -c "prettyPrint test FAIL"` = 0.** Every VISUAL big
operator in the battery — including the V46 double integral and the V50 sum —
was painted with ∏ strokes and not one pin moved. `VISUAL 'VS1'` paints a ∏
over a program that sums.

**Why nothing catches it.** `ppvSerialize` switches on `a->item`, never on
the node tag, so V1/V4/V31 are blind by construction. B8
(`prettyTest.c:1525`) is `ppTestRectAnyLit(108, 118, 10, 22)` — an *any-lit*
check, satisfied by ∏ strokes exactly as by Σ strokes — and it drives the
formula/history builder, not the walker. V27/V36 compare pixel sums against
zero/non-zero. And `ppBigopBox` derives geometry from the body's metrics
alone, so a glyph swap changes no width, no ascent, no signature.

**Violated.** `prettyTest.c:4427`: *"V46-V51 assert the NODE shape, not the
serialized text. The text back end is a test seam; **these are the
product**."* For constructs, the node shape as asserted omits the operator
identity — the most meaning-bearing element of the construct.

**Bug class.** *An oracle that cannot observe the field it exists to
protect.* Related: there is **no node pin for VPRD at all**, and adding one
would not help while the signature is tag-blind; and MUT-112 (tiny/context
variable runs swapped) is unpinned specifically for DERIV, the one kind where
both runs are live and differ only in font, which the signature does not
encode either.

**Class test.** Emit the tag — `B[Σ](…)`, `B[∫](…)` — and add a VPRD row.
Then the tag mutation and the DERIV run-swap mutation both go red, and the
existing eight cases gain the discrimination they were written to have. One
correction to the finding as raised: the signature *can* separate an integral
from a sum structurally (an integral's body carries the `" d x"` run and its
under-limit is bare), but sum and product signatures are byte-identical and
the drawn glyph identity of all three is unpinned.

---

### PP18-14 — four comments say the real-file driver is registered at the TAIL and runs LAST; the patch anchors it mid-list, before `graphs_cov`

**Where.** `prettyTest.c:4501-4502` (*"It lives in its own driver, registered
at the **TAIL** of testSuiteList.txt"*), `:4506-4508` (*"Running last makes
both harmless"*), `:4520-4521` (*"It runs LAST"*), and
`testSuite/tests/pretty_visual_real.txt:3` (*"Registered at the TAIL of
testSuiteList.txt"*). `git blame` attributes every one of them to
`f0edd5ee7` — the same commit that anchored the entry at list line **510**,
with `graphs_cov` at 512 and `nested_cov` at 514.

**What it costs.** A maintainer who reads the header, finds the entry is not
at the tail, and "restores" it to EOF re-creates the exact `@@ -507,3` patch
conflict with forth-core's `forth_interp` that `DESIGN.md:758` records as
already having happened once. **The solo gate stays green** (forth-core
absent); only the combined pass fails, at patch application. Secondarily,
`:4506-4508`'s justification is now false as written: `nested_cov` runs
*after* `pretty_visual_real`.

**Violated.** `DESIGN.md:758`, the §7 hook-table row added by this very
stage: *"test-list slot | **`pretty_visual_real` anchored before
`graphs_cov`** (PP18) | NOT at EOF: forth-core appends `forth_interp` there,
and both patches produced the same `@@ -507,3` hunk — a real conflict, caught
by the combined gate."* Also `TESTING.md:576` and the commit message itself.
The C comments assert the opposite of the authoritative document, in the same
commit.

**Bug class.** *A comment that survives the change it describes*, in the file
a maintainer reads before editing the thing.

**Mitigation already on the record, which is why this ranks here and not
higher.** The file a maintainer must actually edit —
`testSuite/tests/testSuiteList.txt:506-510` — carries the correct reason on
the five lines directly above the anchor, and the failure mode is the loud
one the composition law designs for.

**Refuted sub-claim.** The trailing `fnClPAll(CONFIRMED)` at `:4564` is not
an accidental save: `:4519-4522` documents it and gives the right reason
(*"a later driver should find a clean slate rather than somebody else's
programs"*), a sentence that only makes sense if the driver does **not** run
last. The protective mechanism is deliberate; only the words are stale.

**Class test.** None — this is three or four sentences. A grep that diffs
comment claims against patch hunk ranges is over-engineering; say what the
anchor is and why, once, in the place the reader edits.

---

### PP18-15 — `DESIGN.md`'s VISUAL budget and `ppvBody`'s comment describe the fragment pool, compose buffer and rollback that PP18 deleted

**Where.** `design-docs/pretty-print/DESIGN.md:736-743` and
`prettyVisual.c:389-391`.

**What is false.** The paragraph reads: *"one ~1.5 KiB stack frame in
`fnPrettyVisual` (**1 KiB fragment pool + a 256 B compose buffer**), plus ~50
B per recursion level, **capped at depth 5**. **Fragments are descriptors
into one linear pool** … **Reclamation is by construct-boundary rollback**:
every descriptor alive when a body walk starts points below the mark taken at
that moment."* At the tip: `PPV_POOL_BYTES` is **512** and holds leaf text
only (`:46`); there is no compose buffer on the product path (the whole text
back end is behind `#if PC_BUILD || TESTSUITE_BUILD`, and `fnPrettyVisual`'s
only local aggregate is `ppvCtx_t ctx`); there is no descriptor type and no
`scratch` member; `grep -n poolUsed` finds no `= mark` anywhere — the only
reset is `ctx->poolUsed = 0` once per walk at `:919`, so the pool is
**append-only for a whole walk**; and the depth-5 cap guards `ppvWalk`'s
recursion only — `ppvAstToNodes` (`:823`, recursing at `:845`, `:865-866`,
`:875-880`) has no depth counter at all. `prettyVisual.c:389-391` still says
the body result *"is handed back in `ctx->scratch`, which survives the
caller's pool rollback"*, naming a field that no longer exists — it is
unchanged **context** in the very hunk of `47f6e609b` that deleted it.

Two clauses do survive and are recorded against the finding: the headline
"~1.5 KiB stack frame" is still right (measured `sizeof(ppvCtx_t)` = 1,414 B),
and "capped at depth 5" is still true of the walker. The composition given
inverts reality (512 pool + 672 B `ast[48]` + ~230 B of tables), and the
**largest single consumer is undocumented**.

**What it costs.** The next reader deciding whether a pool is big enough
reasons from a reclamation scheme the code does not have and concludes the
pool is bounded per construct when it is append-only for a whole walk. PP18-7
and PP18-8 are both memory-shaped and sit exactly where the claims went
stale; and the one number a reader would size against is wrong by 2× in the
*safe* direction, so "contingency is shrinking the pool" reasoning targets
the smaller half.

**Violated.** `CLAUDE.md`: *"DESIGN.md there is authoritative,
DESIGN-HISTORY.md is its non-normative amendment trail."* And the package's
own convention for superseded prose, eleven lines earlier in the same file:
*"**RETIRED at PP18 — the emitted alphabet** … Recorded rather than deleted
because the reasoning is what justifies not going back."* The marker exists
and was not applied here. This is in range rather than pre-existing:
`f044f875e` edited this very paragraph's first sentence for the new flash
numbers (*"PP18's refactor gave back 136 B"*) and re-dated it *"measured
2026-08-28"*, so it cannot be defended as a frozen historical note.

**Bug class.** *Duplicated truth with the losing copy marked authoritative.*

**Class test.** None at code level. The enumerable discipline is the one the
package already has for `patches/`: the doc claims that are computable
(`PPV_POOL_BYTES`, `PPV_AST_NODES`, `PP_POOL_NODES`, `PP_MAX_DEPTH`,
`sizeof(ppvCtx_t)`) could be emitted by the test build and diffed against the
document, the way the refresh manifest diffs the generated tree.

**Adjacent, same paragraph block.** `DESIGN.md:622-625` still lists *"D16
pool exhausted"* while the enum slot at `prettyVisual.c:64-66` is now
`PPV_D_ARENA`. Also `DESIGN.md` §1 still says `ppNode_t ppPool[48]` and "max
nesting depth 6" against the header's 72 and 12 — that one predates this
range.

---

### PP18-16 — the V-family driver header and `TESTING.md` still tell a reader the transpiled string is the product

**Where.** `prettyTest.c:3324-3326`: *"The pins assert the TRANSPILED STRING,
not the picture: **the string is the walker's whole product**, and every
rendering question about it was already settled by the equation battery."*
And `TESTING.md:506-511`, which asserts the same **with "That is deliberate"
attached** — a third stale site the finding as raised did not name.

**What it costs.** After this range the string is a `#if PC_BUILD` seam and
the node tree is the product. The false statement is the *section header a
maintainer reads before adding a V pin*; the true one — *"The text back end
is a test seam; these are the product"* — sits ~1,100 lines lower in the same
file, at `:4427`. That is precisely the shape that produces PP18-12 (a new
construct pinned in the seam only) and left PP18-6 uncovered.

**Violated.** `DESIGN.md` as amended in this range (*"PP18 removed that round
trip … The walker now builds a small expression tree and lays it out through
the shared builders"*) and `prettyInternal.h:141` as amended (*"the walker's
**TEST** seam … Not in the device build — the drawing path builds nodes and
never makes text"*). The same commit wave corrected the authoritative
document and the header, and marked the retired alphabet rule explicitly,
which is what rules out "this banner is a stage record": the convention for
keeping superseded prose exists and was applied three times in this range,
just not here. `TESTING.md`'s diff in the range is 34 added / 0 deleted — the
V46-V58 rows were appended and the intro never revised.

**Bug class.** Same as PP18-15; worse, because it is load-bearing for future
test authorship rather than for future sizing.

**Refuted sub-claim.** The `pretty_print.txt` case blurb "stopping at V20" is
**not** PP18 staleness: it is byte-identical at `1fd492a48`, where V21-V45
already existed, so it is pre-existing summary style, and its one factual
sentence (*"V18 evaluates a transpiled string (4/3)"*) is still true.

---

### What I would leave alone

If the goal is code that is correct rather than code that passes an audit:

**PP18-6** (doubled parentheses on an additive DERIV body). It is ugly and it
breaks a documented agreement, but the drawing means the right thing. Fix it
when the height consequence is measured and shown to push a real formula out
of the Z/T band; otherwise it is a paren.

**PP18-10** (the unchecked `step`). The guard asymmetry is real and the fix
is one conjunct, but the reaching input is a 172-digit step over a 320-digit
body. I report it because it is the same class R4-3 already cost this project
once, not because anyone will hit it.

**PP18-11** (the unchecked `fopen`). Test-driver robustness whose only
reachable conditions trip a *checked* upstream writer first. One line, no
class test, no ceremony.

**PP18-14, PP18-15, PP18-16** are documentation. PP18-14 has a real
combined-gate cost and is four sentences, so fix it. PP18-15 and PP18-16 I
would fix because two of this round's findings and two of its coverage holes
are downstream of them — but nobody's calculator misbehaves either way, and
if the choice were between them and PP18-1 there is no choice.

I would **not** leave alone: PP18-1 through PP18-5, PP18-7 and PP18-8. Two of
them make the calculator draw mathematics it does not compute, one hangs it,
one blanks it, and two are the memory arithmetic underneath.

---

## 4. PLAUSIBLE and UNVERIFIED findings

No finding in this round survived refutation without a constructible reaching
input, so there are no PLAUSIBLE entries in the strict sense. Three items sit
below the line:

**U1 — the package gate hard-binds to an upstream docs path and to the byte
content of an upstream-owned file.** `prettyTest.c:4530` opens
`docs/appnotes/sources/AN0022/func.p47`; `:4553-4560` hard-codes four
transpile expectations against its contents. Upstream has already moved this
tree once (`dcf6e4a9b`, "Appnote renames and re-arrangement"). After such a
rebase the package gate goes red with *"V58 cannot open the appnote-22
program file"* — a failure whose text names the package for a change made
entirely outside it. If upstream instead *edits* `func.p47`, the file opens
and the four string expectations fail with a diff, against no recorded
version. The driver's justification (*"A missing file FAILS rather than
skips… absence means something is wrong that a silent skip would hide"*)
reasons about a file the package owns; this one is upstream's, in a directory
upstream has already rearranged, and the comment never addresses the
content-churn case, which is the more likely one. **Beyond the verification
cap — not refuted, not confirmed.** What would settle it: is the owner
content for the gate to be hostage to an upstream docs tree? If yes, record
`func.p47`'s hash in the package and make the failure text say "upstream
moved or edited `docs/appnotes/…`, not a package regression". If no, vendor
the fixture.

**U2 — the dirty set is walk-ordered, not body-scoped.** A construct body
that does `RCL a … STO a` draws `a` as if it were constant, although the
engine re-runs that body once per sample and the value differs from the
second iteration on (`prettyVisual.c:707` tests the dirty list as it stood at
that point in the walk). The design dimension could construct no arrangement
where the tainted value survives into the printed integrand rather than being
dropped or declining for another reason first, and says so. **Unreached, not
cleared.** What would settle it: an afternoon with the taint rules and the
seeded-frame interaction, or a fuzz over short bodies containing one STO.

**U3 — `varOff` reachability by the blunt route.** PP18-7's second reaching
input — two long numerals pushed and dropped before any construct, carrying
`poolUsed` past 255 with no nesting at all — depends on the maximum numeral
length a program step can carry (`getStringLabelOrVariableName`'s length byte
is a `uint8`; `NIM_BUFFER_LENGTH` is 200). Nobody built it. The nesting route
was verified on intent and the truncation itself was *observed* in another
verifier's probe, so the finding does not rest on this; but if someone wants
a short reproducer, this is where to look.

---

## 5. Design observations (D7)

Shape, not defects. These age better than the bug list.

**D18-1 — the refactor removed three bounds that were carried by the text,
and replaced none of them.** This is the systemic observation of the round
and it explains PP18-2, PP18-3 and half of PP18-10. PP17's back end
materialised strings during the *budgeted* walk, and three properties fell
out of that for free: `PPV_FRAG_MAX` = 255 capped any single fragment (so
runaway duplication declined D15); the 1 KiB pool with construct-boundary
rollback capped total text (D16); and `ppqShowRender`'s *"always show
SOMETHING"* arm guaranteed the owner saw a line even when the 2D grammar
refused. PP18's node path has no fragment cap, no rollback and no fallback —
and its two remaining budgets, `PPV_STEP_BUDGET` and `PPV_AST_NODES`, are
both **spent before `ppvAstToNodes` is entered**. The new pass has no budget
of any kind. The rationale on record covers only grammar decline (*"It
existed because `ppqParse` could decline. Tree-to-nodes cannot"*), which is
true and not the question. **The durable fix is not four patches; it is
deciding what the node path's failure envelope IS and writing it into
§VISUAL beside the decline catalog.**

**D18-2 — the same picture is now built by two back ends with different
scoping rules, and the doc says they must agree.** `ppfWrapIf(body, pBody,
PPF_PREC_MUL)` asks the tree its precedence; `ppqScopeBody` sniffs the run
text for a `+`/`-`. Three dimensions went looking for a divergence that
changes meaning and found none (SUB, ADD, CHS, negative literals, additive
numerators over a fraction bar all agree); the one place they differ is the
kind the parser deliberately exempts, PP18-6. That is a good outcome for the
design and a bad one for the pin: V57 exists to assert the agreement and its
fixture cannot see the disagreement. Duplicated truth survives here only
because both copies are currently right.

**D18-3 — `ITM_F1DRV` carries both derivative orders, with the order in a
flag.** So `ppvAstToNodes:882` needs
`(to == PP_NONE && a->item != ITM_F1DRV)` as a special case meaning "this
construct has a point, not a range". It is correct today and every verifier
checked it. It is also fragile in an unusually specific way: the obvious
future edit — storing `ITM_F2DRV` for a second derivative, which is what a
reader would expect the AST to do — silently turns that guard off. If the
representation stays, the guard wants a comment saying why the conjunct is an
item test and not a kind test.

**D18-4 — the walker hand-models stack lift while the item table carries it.**
`DESIGN.md` justifies fail-closed dispatch with *"the item table carries no
stack-effect metadata to infer from"*. For **lift** specifically it does:
`SLS_STATUS`, read by `reallyRunFunction`'s epilogue, is exactly the bit the
walker's `liftDisabled` model reproduces by hand — and PP18-5 is where the
hand copy and the table disagree. The trap is that `SLS_ENABLED` is the
0-valued default, so a row that is never executed (`PTP_DECLARE_LABEL`) reads
as enabled; a machine-derived model has to know that. Worth doing anyway,
because the alternative is a hand-maintained list that has now been copied
five times.

**D18-5 — the pins that assert meaning run through a back end the product
does not use.** PP18-12, PP18-13 and PP18-16 are three faces of it. The V
family was designed when the string *was* the product; PP18 made it a seam
and added eight node pins beside it without retiring the header that says
otherwise. The node signature is the new oracle and it is weaker than the old
one in one specific way — it cannot see a box tag — which is how a ∏ over a
summation passes the whole gate.

**D18-6 — the DERIV seeding argument shows the project's own method working
and stopping one step early.** `DESIGN-HISTORY.md` records that the deferral
*"turned out to cost one read"*, and the code comment insists the claim is
*"measured … not an analogy"*. Both are the right instinct — the same
instinct that makes this package's rulings trustworthy elsewhere. The read
covered `_differentiatorIteration` and not `calcDeriv`, its caller, which is
where the variable comes from. **The general form: when a faithfulness claim
is grounded in one upstream function, the claim is only as good as that
function's *arguments*.** Naming that as a class is worth more than the fix.

**D18-7 — the standing `solver/equation.c` extraction debt is untouched and
grew.** 619 added lines across 5 hunks at this range's base, up from 589/4 at
the 2026-08-27 upstream-minimality review, which recommended the extraction.
Neither the growth nor the one WS-ONLY churn hit (the re-indented
`showString` in the 2D strip hook) is in this range, and nothing PP18 did
made it worse. Restated only so a reader does not take its absence from §3 as
closure. Against it: PP18's *own* upstream footprint is the cleanest this
package has produced — 921 lines of new walker, a new construct and a new
test driver for two upstream lines of behaviour, no new override files, no
new forward declarations. The discipline works when it is applied.

---

## 6. Deliberately not flagged

Mandatory section, and this round's is long because six of the eight
dimensions cleared more than they reported. Merged from the finders' cleared
lists and the refutation pass's one disproof.

### 6a. Refuted by the refutation pass

**`ppvPaintFullScreen` committing the self-painted state with nothing drawn
(`prettyVisual.c:1266`) — REFUTED as raised, and the argument is worth
recording in full because the majority went the other way.**

The intent verifier's case: the design ruled on exactly this, at exactly this
call site. The header comment states the removal and its reason in the same
breath as the failure mode (*"no linear fallback beneath it, because a tree
that failed to lay out here failed on SIZE"*), and `DESIGN-HISTORY.md`
records it as one of PP18's three deliberate consequences. Disputing the
persuasiveness of a stated ruling is disagreement, not an undocumented gap.
It further held that both "violated" citations were misattributed:
`DESIGN.md:683-685`'s *"still shows in the window, linear and centred"* sits
in the **stack-window** bullet, mirroring `prettyVisual.c:1191-1194`'s
*"still shows here, linear, on the Z line"*, and still holds in PP18 because
plain arithmetic now lays out as an ordinary node chain in the band; and
`prettyEquation.c:909-914`'s *"always show SOMETHING"* is EQSHW's invariant
over a **user-typed source string** that upstream itself truncates at screen
width — an invariant VISUAL structurally cannot hold, because after PP18 no
string exists in the product path. It closed with the sharpest point in the
round: PP17's fallback drew a **truncated** linear line, which is itself a
formula meaning something other than what the program computes.

**Why the finding stands anyway (four lenses to one).** The ruling's stated
premise is false in fact, not merely unpersuasive: the depth-cap refusal and
`ppNewBox` returning `PP_NONE` are not size failures, the same commit's own
comments say so at `:1212` and `:1346`, and the retry over the same pool and
the same cap is provably futile. And the measured end state is worse than
"no linear fallback": it is a **cleared screen with the two rules painted, no
formula, no D-number, no error code, X's answer erased, held until EXIT**.
Whatever the right answer is — a decline with a D-number, a "formula too
large" line, or a tiny-font third rung — painting the frame *before* knowing
there is anything to put in it is not it. The truncation objection is a real
argument against restoring PP17's arm verbatim, and it is the strongest
reason to prefer a decline over a fallback.

### 6b. The walker's model of the machine

- **ENTER building a DAG, and both back ends expanding it independently.**
  Correct. `ppvAstToNodes` builds a fresh layout subtree per reference, so
  `x·x` is two runs and no node is shared between two parents. Its only
  consequence is the node count, reported as PP18-3 and as a route into
  PP18-2, not as an aliasing defect. And the design *requires* the
  re-expansion: *"`ENTER ×` transpiles to `x×x`, not `x²`"*.
- **`ppvPush` dropping the bottom slot at depth 8.** It shifts out `ast[0]`,
  the deepest level — the register RPN actually loses. *"Drops its bottom,
  exactly as the hardware one does"* is right.
- **Net stack effects of the constructs.** Checked against upstream rather
  than assumed: `derivativeVariable` STOs X without dropping and finishes
  with `convertRealToResultRegister(&x, REGISTER_X)`, depth unchanged —
  `ppvDerivative` pops one and pushes one. `fnIntegrateYX` drops twice and
  leaves the result in X — the walker pops two and pushes one. Both match.
- **`f'`'s parameter possibly naming a LABEL.** `derivativeVariable` has a
  label branch, but as a *program step* `ITM_F1DRV` is `PTP_REGISTER`, so
  `_executeOp` resolves the name through `findNamedVariable` only
  (`lblGtoXeq.c:486-497`) and the label branch is keyboard-only.
  `ppvVarName`'s reading matches.
- **`ITM_STO` to a numbered or lettered register not entering the dirty
  list.** Safe by construction, not by luck: `ppvVarName` declines every read
  of a non-named register with D7, so no such name can reach a drawing.
- **The latch rules.** `latchedInt`/`latchedDrv` not restored when a
  construct returns is ruled deliberate (`currentSolverProgram` is a
  persistent global upstream, V17), and the PGMDRV/PGMINT split is exactly
  what V55 pins — verified against `fnPgmDrv`'s separate
  `currentDerivProgram` global rather than taken from the brief.
- **A program starting with ENTER declining D10 rather than duplicating a
  zero.** `DESIGN.md` calls the empty frame *"the honest boundary of what a
  static walk can claim"*.
- **`Σ(n·n, n=1..5) + n`** — the invented counter matching a variable read
  *outside* the operator. Standard bound/free notation; the evaluator scopes
  it the same way; it computes correctly. Only the **limits** case is a
  finding (PP18-9), because the limits are drawn inside the operator's visual
  scope.

### 6c. Guards, bounds and narrowings that are correct

- **The stacked-power guard dereferencing `ctx->ast[a->child[0]]` with no NIL
  test (`:855`).** Not reachable: the recursive call two lines above returns
  `PP_NONE` for `PPV_NIL` and the arm has already returned.
- **`ppvIntern`'s bound.** `poolUsed + len > PPV_POOL_BYTES` correctly allows
  exactly 512 (offset/length pairs, no NUL); `ppvOutRaw`'s `len + n + 1 > cap`
  correctly reserves the NUL.
- **Every name path is length-safe.** `ppvNameIsDrawable` rejects at
  `i >= PPV_NAME_MAX-1` so names are ≤ 15; `ppvVarName` `strcpy`s into
  `char[16]`; `ITM_STO` checks `strlen >= PPV_NAME_MAX` **before** the
  `strcpy` into `dirty[][16]`; `binding[]` takes names from those two sources
  plus one-character invented counters.
- **`textLen = (uint8_t)len` in `ppvLiteral`.** Cannot truncate: a program
  step's string parameter carries a one-byte length that
  `getStringLabelOrVariableName` additionally clamps. Checked deliberately,
  because PP18 deleted `ppvPush`'s old `len > PPV_FRAG_MAX` decline — the
  same shape as PP18-7 with the opposite answer, which is why both were
  looked at.
- **All band arithmetic is guarded before it subtracts.** Ascent/descent are
  `int16_t` (no unsigned underflow); `PPV_BAND_ROWS - (asc+desc)` and
  `(21+167-(asc+desc))/2` both sit after the `>` test; the x coordinate after
  `width > SCREEN_WIDTH-4`.
- **The label index is bounded three times** (`ppvLabelIndex`,
  `fnPrettyVisual`, `ppvWalk`) and `id - FIRST_LABEL` is cast before the
  unsigned compare, so a below-`FIRST_LABEL` id wraps into the rejected range
  rather than indexing.
- **`ppvOpAt`'s two-byte opcode decode** matches `decodeOp`'s grammar.
- **`ppvSumProd` exhausting all four counter names declines D12.** With
  `bindingCount ≤ 5` that is reachable and correct, not a wrap.
- **`ppvBody` leaving `bindingCount` incremented on its failure paths.** Real,
  unobservable: every one of those paths has already called `ppvDecline`,
  `ctx->failed` is terminal, and `ppvRun` re-zeroes. Four dimensions found
  this independently and all four cleared it.
- **`ITM_DROP` ignoring `ppvPop`'s return, and the fall-through to
  `liftDisabled = false` after a failed op.** Same reasoning; redundant, not
  wrong.
- **`int pStep` uninitialised when `child[3] == PPV_NIL` (`:874`).** Assigned
  in the ternary and never read on that path; warnings are clean.
- **`varTiny`/`varCtx`'s `PP_NONE` convention.** Checked at all three arms
  against both callers: SUM/PROD check `varTiny` and never touch `varCtx`,
  INTEG the reverse, DERIV both — and each caller supplies exactly the pair
  its kind checks. This is why the missing `step` check (PP18-10) stands out:
  it is the one operand nobody checks at either end.
- **`ppvTranspile` leaving the caller's buffer untouched on a decline.**
  `out[0]=0` lives inside the success branch; both callers check the return
  first. Test-seam only.

### 6d. Conservative behaviour that declines rather than mis-draws

The file's fail-closed rule sanctions erring toward refusal, and several
things that look wrong err that way:

- **The dirty set is never rolled back at a construct boundary, and
  `ppvIntegral` does not test the d-variable against it.** A program that
  does `STO 'X'` before `∫'X'` and then recalls X inside the integrand is
  refused (D5) although the integrator overwrites X per node. Worth knowing;
  not a defect. (The opposite direction of the same mechanism is U2.)
- **Unit-step detection matching only the single character `'1'`.** A step
  stored as `1.0` draws a `,Δ1.0` tail: noisier, arithmetically identical,
  truthful.
- **A comma-radix numeral declining D14 in `ppvLiteral`.** PP17-era,
  consistent with the separator evidence in `DESIGN.md`.
- **`ppvNameIsDrawable`'s comment mentions subscript digits while the code
  rejects all digits.** More restrictive than advertised, so it declines
  rather than mis-draws.
- **`ppvMonadicName`'s dead `ITM_ABS`/`ITM_MAGNITUDE` arms** (the catalog
  spelling `>ABS<` fails `ppvNameIsDrawable`, so they can never pass).
  `DESIGN.md` records this as the "Documented gap", and had they passed,
  `ppfCombine1`'s `|x|` BARS arm would have drawn them correctly.
- **CLX, SOLVE/PGMSLV, dyadics and exponent numerals absent from the
  dispatch.** All in `DESIGN.md`'s Non-goals with reasons; scope, not holes.
- **`PPV_FRAG_MAX` and D15 are now dead on the device** — the fragment cap
  survives only inside the `#if PC_BUILD` seam, and `DESIGN.md:622` still
  lists D15 in the catalog. Dead, not wrong; the D-numbers are stable and no
  user-visible number changed. Mentioned rather than flagged, but it is the
  second half of what D18-1 describes.

### 6e. Tests and oracles that are stronger than they look

- **V18's rewrite is real.** It feeds `ppvTranspile`'s output to `fnEqCalc`,
  and its `strcpy(produced,"0")` fallback after a `ppTestFail` double-fails
  rather than passing (0 is 1.333 from the oracle).
- **V54/V55 genuinely separate the two latches.** D6 (`PPV_D_NOLATCH`) is
  raised only from `ppvIntegral:465` and `ppvDerivative:511`, and VDIN's
  PGMINT step cannot itself decline, so the pin distinguishes MUT-110: with
  the mutation VDIN would transpile to `DERIV(x×x;x;3)` and the decline pin
  would fire.
- **V58's two decline pins land on the intended opcodes today.** `func.txt`
  confirms SLVINT's first step is `PGMSLV 'SI'` and PLTROOT's is
  `PGMPLT 'RT'`. `ppvTestDecline` ignoring `atStep`, and D1 being a
  catch-all, make it a weak oracle — any earlier unhandled step would satisfy
  it — but it is not currently wrong, and it predates PP18.
- **`pretty_visual_real.txt` cannot silently no-op.** It seeds
  `RX=LonI:"99"` and expects `LonI:"0"`, and the driver zeroes
  `ppTestFailures` on entry and writes it on every exit path including the
  file-missing one.
- **MUT-112 (tiny/context runs swapped) does hold for SUM/PROD/INTEG.** For
  INTEG `varTiny` is `PP_NONE` and for SUM/PROD `varCtx` is, so a swap makes
  `ppqBuildBigop` return `PP_NONE` and V46/V50 report "did not lay out". Only
  DERIV, where both runs are live, is unpinned — folded into PP18-13 rather
  than reported twice.
- **The V46-V57 `cases[8]` loop, `ppfTestSigNode`'s silent truncation, and
  the new sprintf buffers.** All eight slots are assigned before the loop; a
  layout failure hits `ppTestFailures++` with a `continue` rather than reading
  an uninitialised root; truncation shortens the actual signature and can only
  cause a mismatch, never a false pass (the longest new expectation is ~46
  bytes against `sig[192]`); every new buffer is comfortably sized.
- **`prettyPrint.h` declaring `prettyTestReal` unconditionally while
  `prettyTest.c` defines it inside `#if PC_BUILD`.** `testSuite.c` is a
  test-build artefact and the gate is green in both configurations; the
  compiler would report otherwise.

### 6f. Upstream discipline (checked against `deliberate-exceptions.md`)

- **The `testSuite.c` table addition** extends the package's existing single
  hunk rather than opening a new one (hunk count 1, adds 11 → 12), and the
  declaration arrives through the already-patched `c47.h → prettyPrint.h`
  chain, so upstream gained no forward declaration. Contrast forth-core,
  which added two `fnForthOuter` decls into `testSuite.c`'s prologue. This is
  the minimal available shape.
- **The 6-line comment block inserted into upstream's `testSuiteList.txt`.**
  Adding prose to an upstream data file looks like churn; upstream's own list
  is annotated in exactly this style at exactly this spot (`deriv_mvar_cov`,
  `readp_cov`, `shortint_restore_cov` at `:480-500`, each stating its
  ordering constraint). Upstream convention for the same problem, which is
  the standing rule.
- **Adjacency with forth-core's EOF hunk.** No shared context line (501-506
  against 507-509), so both apply in any order. The mid-list anchor is the
  correct call; only the comments describing it are wrong (PP18-14).
- **`extern void fnClPAll(uint16_t confirmation);` at `prettyTest.c:4529`.**
  Redundant — `manage.h:17` is already in scope via `c47.h:111` — but an
  upstream signature change produces a conflicting-declaration compile error,
  not silent drift. Loud, so not flagged.
- **`c47programTest.bin` left in the repo root holding `func.p47`'s bytes.**
  Upstream's `covWriteAndLoadPgm` writes and abandons the same file at the
  same path (`testSuite.c:1171`); every later staging call overwrites it.
- **Clearing program memory before `graphs_cov` and `nested_cov`.** Checked
  rather than assumed: both stage through their own loaders
  (`covLoadGraphPgms`, `covLoadNestedPgms`) and build their own programs;
  nothing after list line 508 depends on `programs.txt` or on XEQ key
  assignments. The order is safe as shipped; the residual hazard is
  documentary and is PP18-14.

---

## 7. Verdict

**Would I ship it? Not until PP18-1 lands, and PP18-2/PP18-3 are a close
second.** The refactor itself is the best-argued piece of work in this
package's history. Precedence is genuinely settled once, in the shared
builders. The byte-identical screenshot and 45 unedited pins make it
verifiable rather than plausible. The upstream footprint — two lines of
behaviour for 921 lines of walker — is the cleanest the package has
produced. Nothing in §3 is an argument against the direction.

**Where it breaks first: the new construct's faithfulness claim.** PP18-1 is
the one prohibition the whole subsystem is built around — draw what the
program computes — and it fires on the *plainest* DERIV program there is, one
whose body declares no `MVAR`. The engine returns 0; the screen says 6. The
argument in the code is right about `_differentiatorIteration` and wrong about
who supplies its argument, and the only DERIV fixture in the suite happens to
be the single configuration where the two channels agree. Everything about
this defect is quiet: no decline, no D-number, no pin, and a design document
that records the read as *"one read"* well spent.

**Second: the failure envelope did not come along with the refactor.** Three
findings share one sentence — the text back end was carrying bounds that
nobody wrote down as bounds. Thirteen presses of `1/x`, or fourteen chained
divisions, or five `ENTER ×` pairs now buy a **blank framed screen with the X
answer erased and no error at all**, where PP17 printed a line of text or a
D-number; and twenty-five to forty `ENTER ×` pairs — an idiom `DESIGN.md`
blesses by name — buy an unkillable freeze. Neither needs an unusual machine
or a hostile file, only a program a little larger than the fixtures.

**Third: two guards stop one member short of their class.** A construct under
a power draws its exponent on the body, so `(Σn)²` and `Σn²` — 36 and 14 —
are the same picture; and the ENTER lift latch crosses XEQ/PGMINT/PGMDRV,
which upstream clears, silently substituting a seeded variable for a real
operand inside an integrand. Both are the recurrence shape this project has
now paid for four times: a guard whose conjunct list enumerates the examples
in front of it.

**What I would do, in order.** Name the two classes in `DESIGN-HISTORY.md`
first — *measured one call short* and *a bound removed with the
representation that carried it* — because they are what the next stage will
re-earn. Then fix PP18-1 red-first with the 3 × 2 MVAR matrix and the
`fnEqCalc` oracle extended to DERIV; that oracle is the single
highest-value test in this report and it catches the whole class. Then decide
the node path's failure envelope as a design question and implement PP18-2
and PP18-3 together, since they are the same decision — what happens when
layout cannot proceed, and how much work may it spend finding out. Then
PP18-4 and PP18-5 as one guard wave with the machine-derived `SLS_STATUS`
enumeration. PP18-8 shrinks PP18-7 and should precede it. PP18-12/PP18-13 are
a test afternoon that would have caught two of the above. The documentation
trio can ride with any wave, except PP18-14, which costs four sentences and
prevents a combined-gate conflict.

**What I would not do.** Do not memoise the ENTER DAG. `DESIGN.md` rules that
`ENTER ×` draws `x·x` and not `x²`, deliberately, and collapsing shared nodes
in the layout pass would quietly overturn it; the fix for PP18-3 is a budget
or an early abort, not a cache. And do not restore PP17's linear fallback
verbatim: the refutation pass's strongest point is that it drew a *truncated*
line, which is itself a formula meaning something other than what the program
computes. A decline with a D-number is the better answer, and it is the one
the rest of this file already uses.

---

## 8. Round and exit state

**Round 2 for this package** — the first audit of PP18, following round 1's
`A1`–`A14` over PP1–PP16 and its out-of-family `R1-1`–`R1-3`. The prior
round's exit note asked that round 2 run over the fix wave rather than the
package again; this round instead ran over a new stage, which the regression
record predicts is the easier target. That prediction held only partly: five
of sixteen findings are in code PP18 wrote from scratch, but **eight are in
what PP18 removed** — a shape the fix-regression record has not seen before
and worth adding to it. *Deletion is a fix shape too, and it regresses like
one.*

**Readers.** Eight finder dimensions, blind to each other: contracts,
lifecycle, arithmetic, errorpaths, guards, tests, design, upstream. Each
finding then went to an independent refutation pass with one assigned lens;
five findings collected two to four lenses because multiple dimensions
reached them independently. **Fourteen of the twenty surviving verdicts are
backed by a probe or mutation applied, observed and reverted in an isolated
worktree** — the highest proportion of any round in this project. Every
worktree spawned at the stale ref `e21af8d28` and **every verifier checked
out `f0edd5ee7` before its first read**: the round-6 runner trap, caught by
the standing first-action rule, twelve times out of twelve. One verifier
additionally detected and worked around a `/tmp` filename collision with a
sibling (§2).

**Two process notes worth keeping.** First, the mutation was decisive in both
directions again: it killed a finder's plausible clearance (PP18-3's "it
terminates cheaply" reading) and it confirmed two coverage holes by staying
green (PP18-12, PP18-13). Second, three dimensions ran read-only and could
not execute their own reaching inputs; every one of their findings that
survived did so because a verifier ran it. A read-only finder is still worth
running — PP18-1 came from one — but its confidence labels should be read as
the verifier's, not the finder's.

**Exit criterion: not met.** The criterion is a round that produces no
confirmed finding of consequence. This one produced two wrong-mathematics
defects, one stuck state, one blank-screen regression and two silent
mis-drawings. Round 3 should run over the fix wave, red-first, and should
watch the shape the record warns about: the highest-risk fix here is PP18-2
plus PP18-3, because deciding the failure envelope means **relocating state**
— what the paint path knows before it clears the screen — which the
fix-regression record names as the most dangerous fix shape.

**Tree state.** This audit wrote exactly one file — this one. Every probe and
mutation was applied, observed and reverted inside its own worktree. The
owner's working tree carries one uncommitted cosmetic change to
`fnPrettyVisual`'s decline branch that was present before this audit began
and is not its doing; it moves no line cited above.
