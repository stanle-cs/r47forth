# Audit — PP18 RESTARTED round 1, whole stage, at `34ac6e97f`

Subject: `pretty-print/stage-pp17..HEAD` (`8484417..34ac6e97f`) on
`pretty-print/stage-pp18` — the whole PP18 stage, fourteen commits, re-audited
as **round 1 under the 2026-08-29 ruling**: round 1 of every audit is read by
all three families — in-family, Gemini, and GPT/Sol — over the actual subject,
and the round counter does not advance until round 1's report records both
out-of-family replies. The four in-family rounds of 2026-08-28/29 do not
satisfy that ruling; this round restarts the counter with all three families
over the stage itself. Their open findings — **PP18R4-1..11 and the 2026-08-29
out-of-family set — are KNOWN and are not re-reported here.**

Eight in-family finder dimensions ran blind to each other; two out-of-family
readers took packets over the two subsystems the in-family rounds had leaned on
hardest without independent eyes (the layout engine end-to-end; parser/walker
operand scoping). Every raised finding then went to an independent refutation
pass with one assigned lens (reachability, correctness, intent), instructed to
default to REFUTED and to prove coverage claims by mutation.

**Twelve CONFIRMED findings, one new PLAUSIBLE, two REFUTED.** Seventeen
findings were raised across the three families; fifteen survived refutation;
deduplicated across dimensions they are thirteen, and one of the thirteen has
no constructed reaching input and files as PLAUSIBLE per the rule. **Both
out-of-family findings survived, and one of them is this report's #1** — a
measure/paint containment break on the always-on capture surface, five
keystrokes from the default screen, in code four in-family rounds had read and
cleared.

The in-family axis that paid is the one no previous round asked: **the mode
axis.** The walker simulates exactly one calculator configuration — classic
lift, eight stack levels — and reads no system flag at all, so under `FLAG_ERPN`
or `SSIZE4` an ordinary program draws a formula that computes a different
number than the one `XEQ` puts in X, silently. Both are the wrong-picture class
this stage's worst findings were fixed for, reproduced by a mode flag instead
of an opcode.

Nothing was fixed. Every probe and mutation was applied, observed and reverted
inside an isolated worktree; the tree this report finishes on is the tree it
started on, and the gate is green at `34ac6e97f`. (One verifier worktree was
found *already contaminated* by a stale sibling's live mutation — §8, process
item 2. The main tree is clean.)

---

## 1. Subject and coverage

### Out-of-family accounting — round 1, both replies on file

Both reply files were **read for this report**; both are present, non-empty,
and open with a `MODEL:` line quoted verbatim below. No banner is required.

| reader | packet | reply | `MODEL:` line (verbatim) | findings raised |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r1/layout.md` (31,041 B) | `/tmp/pkt-r1/layout.gemini.reply.md` (5,624 B) | `MODEL: Gemini 3.1 Pro (High)` | **1** (plus 5 items considered and explicitly cleared — merged into §6) |
| sol | `/tmp/pkt-r1/scoping.md` (29,725 B) | `/tmp/pkt-r1/scoping.sol.reply.md` (4,762 B) | `MODEL: GPT-5` | **1** (plus a cleared scope-path list — merged into §6) |

`layout.gemini.reply.md.err` is empty; `scoping.sol.reply.md.err` is a 40 KB
dispatch stderr log, not a reply defect — the reply itself opens with the
verified `MODEL:` line. Both findings survived refutation and are CONFIRMED
below (PP18RR1-1, PP18RR1-6); the same table with survival counts is in §8.

### Subject

**Tip.** `34ac6e97f` on `pretty-print/stage-pp18` ("docs: skill defect handoff
— the out-of-family pass can be skipped silently"). Range
`pretty-print/stage-pp17..HEAD` (`8484417` excluded): fourteen commits — the
PP18 refactor proper (`1fd492a48`..`f044f875e`: node builders split out, walker
builds a tree instead of a string, text back end demoted to a test seam, DERIV
construct, the appnote-22 real-file driver), then the four audit fix waves and
their reports, then the handoff. **26 files, +8,668 / −859.** Code write set:
`prettyVisual.c` (+1,211 hunk lines, now 1,623 lines), `prettyEquation.c`
(±303, 975), `prettyTest.c` (+891, 5,229), `prettyFormula.c` (±21),
`prettyInternal.h` (±37), `prettyPrint.h` (+1), their generated `files/` twins,
the refresh manifest, and two test-harness patches (+13 lines, the only
`patches/` delta). `prettyLayout.c` (847 lines) is **unchanged in range** but
in scope: the walker's new trees ride it, and it had never been read
out-of-family — which is where this round's #1 came from.

**KNOWN, excluded from re-reporting** (verified still present by the finders,
then fenced): **PP18R4-1..11**; the round-4 PLAUSIBLE carry (P1 8–14-glyph
`MVAR` import, P2 `PP_MAX_DEPTH`, R2-P1, R3-P1..P3); and the **2026-08-29
out-of-family set** (`HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md` §4:
`ppvIntegral` never checks its own limits — the wrong-picture finding; three
over-refusals on the invented-name/allocation-history paths; the "about to be
drawn inside me" ruling that is literally false of `astUsed`). Where a new
finding sits adjacent to a known one, the finding states the distinction
(PP18RR1-4 vs R4-5; PP18RR1-5 vs R4-5/R4-7/R4-11).

**Numbering.** This round's findings are **`PP18RR1-1`–`PP18RR1-12`**, its
plausible **`PP18RR1-P1`**, its design observations **`PP18RR1-D1`–`D4`**.
`grep -rn PP18RR1` over the repository returns nothing outside this report.
Prior series, all distinct and greppable: `A1`–`A14`, `PP18-*`/`D18-*`,
`R1-1`–`R1-3`, `PP18R2-*`, `PP18R3-*`, `PP18R4-*`, `OOF-*` (used only inside
this round's verifier records, mapped here to `PP18RR1-1`/`-6`).

### Coverage (union across the eight in-family dimensions)

**Package code read at line level:** `prettyVisual.c` in full (all 1,623
lines) by six dimensions; `prettyLayout.c` in full (847) by four — including
the first *design-lens* end-to-end read this file has had; `prettyInternal.h`
and `prettyPrint.h` in full; `prettyEquation.c` — full stage diff by five, plus
the parser entry, `ppqName`, `ppqBigopConstruct`/`ppqBuildBigop`,
`ppqScopeOperand`/`ppqScopeBody`, `ppqShowRender` in situ; `prettyFormula.c` —
full stage diff plus `ppfCombine1/2`, `ppfBigop`, `ppfFromCaptureNode`, the
token decoder, lines 1–420; `prettyCapture.c` at its classifier
(`ppcClassify`) and eRPN arm; `prettyTest.c` — the full +891 stage diff, the
helper layer end-to-end, the harness dispatch, V18–V80 selectively (~600–900
lines per reader; the ~4,300 pre-stage pin lines were *not* re-read pin by pin
— that was rounds 3–4's own axis); `testSuite/testSuite.c`, both list files,
`pretty_visual_real.txt`, and the testSuite LCD HAL (blitter polarity, which
cleared two pixel-pin suspicions).

**Upstream verified by execution path** (every mirror and lifecycle claim was
checked against upstream, not assumed): `stack.c` `liftStack`/`getStackTop`;
`keyboard.c` `fnKeyEnter`'s eRPN arms (`:3417`, `:3437-3441`), the dismissal
channels, `NC_SUBSCRIPT`; `bufferize.c` `convertItemToSubOrSup`;
`programming/lblGtoXeq.c` `runProgram`/`executeOneStep` per-PTP dispatch
(`PTP_DECLARE_LABEL`/`PTP_REM` never reach `runFunction`), `_executeOp`
`PARAM_REGISTER` → `findNamedVariable` folding, `_putLiteral`; `items.c` SLS
rows for every walker-dispatched item; `registers.c` `validateName`
(`:693-753`) and `findNamedVariable`/`nameEqualsPrefolded`;
`programming/decode.c` `getStringLabelOrVariableName`'s clamp;
`programming/manage.c` `boundProgramNameLength`; `solver/integrate.c`
`fnPgmInt`'s ladder; `solver/differentiate.c` `deriv_pgm_variable`/`calcDeriv`;
`sort.c` CMP_NAME; `defines.h` (`getStackTop`, register line Y positions,
lettered registers); `flags.c` `SS_4`; `config.c` reset personalities;
`fonts.h` `STD_SUB_*`; the raster font tables in `build.sim` (subscript-digit
ink in all three fonts; glyph metrics for PP18RR1-1's arithmetic);
`screen.c`/`screen.h` refresh and clear extents; `solver/equation.c`'s
package-side `ppEqBigopIntercept` (the INTEG arity contract).

**Docs read:** `DESIGN.md`'s VISUAL/PP18 sections, §6/§7, the Rulings block and
the decline catalog in full by six; `DESIGN-HISTORY.md`'s PP5/PP6/PP18 entries;
`TESTING.md`'s mutation catalog and V/B families; all four prior PP18 reports
at their findings, PLAUSIBLE, cleared and not-flagged sections (so nothing they
killed was unknowingly re-raised); the skill-defect handoff;
`REVIEW_upstream-minimality_2026-08-27.md`; the `upstream-diff-review` skill
and all 13 deliberate-exception entries.

**Not reached, and it matters where.** `prettyCapture.c`'s staging state
machine beyond the classifier and the eRPN arm; `prettyValue.c` internals
beyond `ppfBuildCurrent`/band-check call sites — **PP18RR1-1 paints through
surfaces owned by both files, and their internals have still never had a full
in-family pass this round**; the browsers, softmenu stack and the package
`solver/` dir beyond the INTEG intercept; `prettyTest.c`'s pre-stage pin bodies
except by spot-check; `TESTING.md` in full by only one reader. **No reader ran
the simulator** — no finding is backed by an LCD photograph; the evidence is
end-to-end code traces, glyph-metric arithmetic cross-validated against the
pinned M1/M2 fixtures, executed gate runs with mutations baked in, and one live
in-suite decline pin. The finders were read-only by mandate; every mutation
below is a *verifier's*, run and reverted in an isolated worktree.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh` is green at `34ac6e97f` —
re-run to completion by four verifiers after applying and reverting probes
(recorded tails `PRETTY-PRINT GATE GREEN`, testSuite 1/1 OK, 168–172 s solo).
The finders themselves did not execute it (read-only passes); no verifier
reported a new compiler warning, and the range's own `10e49e084` closed the one
known warning (V77 format-overflow). Not a discovery; recorded as the baseline
every mutation result below is measured against.

**Mutations and probes this round ran** — all applied, observed and
**reverted** in isolated worktrees, none numbered (the MUT catalog is the
owner's to extend):

| probe | result |
|---|---|
| `ppqBuildBigop`'s second-order denominator block (`prettyEquation.c:289-295`, the `if(secondOrder)` s2 emission) deleted | full solo gate **GREEN** → **PP18RR1-8** |
| `ppMeasure`'s PP_RAD synth threshold at `prettyLayout.c:262` changed `radInk+3`→`radInk+4`, paint copy at `:662` untouched | gate **GREEN** — no agreement tripwire exists → **PP18RR1-9** |
| fixture `LBL 'VCBT' / RCL a / CUBEROOT` + temporary pin `ppvTestDecline("…", "VCBT", 1)` | gate **GREEN** — the pin passes only if `ppvTranspile` declines with reason exactly `PPV_D_OPCODE`(=1): the walker refuses live what the capture path draws → **PP18RR1-7** |
| `PPV_BAND_BOTTOM` mutated `Z+31`→`Z+10` (a foreign edit found already live in the verifier's worktree — §8 item 2 — and used as the probe, since it is exactly the required mutation), baked into the gate build | gate **GREEN**, no `V28` failure output → **PP18RR1-10** |
| glyph-metric model of the ˣ√y-with-fraction-index box (standardFont rows from `rasterFontsData.c`), first validated by reproducing the pinned M1 (frac 25/6) and M2 (26) numbers exactly | index ink bottom = promised descent + 6 rows → **PP18RR1-1** |

The wrong-picture traces (PP18RR1-2, -3, -4) and the refusal traces (-5, -6)
are constructed end-to-end in code, hop by hop against upstream, but were not
executed in a build — stated per finding.

**One verifier trap, new this round.** A hand-run of the `testSuite` binary
from the shadow tests directory false-failed `pretty_visual_real` with "V58
cannot open the appnote-22 program file" — a cwd artifact of manual invocation.
The canonical gate runs it green. The governing gate for this package remains
`./packages/pretty-print/build-test.sh`; the round-3/4 warning that
`./packages/forth-core/build-test.sh` returns a meaningless green for
pretty-print mutations stands.

**Upstream churn.** `patch_churn_scan.py` over all 13 pretty-print patches at
HEAD: standing churn count **1** (`[WS-ONLY]`, the `showString` wrap-reindent
in `010-solver__equation.c.patch`) — pre-existing, catalogued, owned by
`REVIEW_upstream-minimality_2026-08-27.md`, not re-reported under rule 6. The
in-range `patches/` delta is +13 purely-additive lines in the two test-harness
patches, both idiomatic (row appended inside the existing pretty block; new
testSuiteList hunk anchored mid-list deliberately to avoid EOF collision with
forth-core's append — the two packages' hunks verified disjoint). **Zero growth
of the eleven firmware overrides across the whole stage.** Refresh sync
verified read-only: working area vs `files/` clean, every
`.refresh-manifest.json` hash re-computed and matching for all 13 files and 13
patches — the build reads the code this audit read.

**`design-audit.sh`** is forth-core's; there is still no pretty-print
equivalent, so no override-budget check ran. The substitute is the patch-surface
audit above, which is clean.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. Ink painted outside the box on the
always-on surface outranks a silent wrong number under an explicit command,
which outranks losing a drawable program to a refusal, which outranks a pin or
a contract that will fail the next editor. For every finding: the reaching
input (or the mutation, for coverage findings), the violated contract quoted,
the bug class, and the class-level test. No patches.

---

### PP18RR1-1 — `ppMeasure`'s PP_RAD arm never widens descent for the index, so a five-keystroke xth-root paints ink below the box every acceptance check trusted

`packages/pretty-print/prettyLayout.c:279` (descent copied from the radicand),
`:285-292` (index block expands ascent only), paint arm `:656-665` (index drawn
at `baseline + relBase`, unclamped). Mint site `prettyFormula.c:118-128`, the
`ITM_XTHROOT` arm of `ppfCombine2`. **Out-of-family
finding (Gemini 3.1 Pro), OOF-G1**, verified worse than filed.

**What breaks.** The measure pass copies `nd->descent` from the radicand and
grows only `ascent` when an index child is present. The index is placed at a
*positive* `relBase` (tucked into the sign's shoulder), so an index with its
own descent — a fraction — puts ink at `relBase + index.descent`, below the
measured box. Every band check downstream (`prettyValue.c:809`'s T-line rung
test, `ppfBuildRow`'s height check, `prettyEquation.c:826`/`:935`) trusts
`ascent + descent`; acceptance is granted for a box the index tail exits, and
the paint pass draws it anyway.

**Reaching input** (constructed, every hop read in code; glyph arithmetic
cross-validated against the pinned M1/M2 numbers). With `FLAG_PRETTYP` set, key
`8 ENTER 1 ENTER 2 ÷ ˣ√y` — five keystrokes of ordinary arithmetic.
`ppcClassify` marks `ITM_XTHROOT` `PPC_DY` (`prettyCapture.c:499`), the capture
AST is `OP2(XTHROOT, 8, DIV(1,2))`, and `ppfCombine2`'s XTHROOT arm appends the
`PP_FRAC` as the radical's index. In standardFont: FRAC(1,2) measures
ascent 20 / descent 8; radicand "8" ascent 12 / descent 0; the PP_RAD box comes
out ascent 22 / **descent 0**, with index ink bottom at **+6** — six rows of
the denominator painted below the accepted box, on the T-line (`PTLINE`), the
PHIST pager and the equation strip. Where the band below is occupied, that is
overpaint of the next line; at a band edge it is clipped ink. Contrary to the
packet's filing, no "deep" index is needed — any `b ÷ a` index does it — and
`VISUAL` is *not* a reaching surface (the walker's op set,
`prettyVisual.c:879-886`, omits XTHROOT), so this is a capture-surface defect.

**Why it is wrong.** The measure/paint contract the layout engine exists to
keep, as the packet and the file's own comments state it: *an accepted tree
must draw everything it measured*, and no pass may put *"ink where the measure
pass promised none."* The arm keeps the ascent half of that promise and omits
the descent half.

**Older than the stage, found by the restart.** Indexed radicals shipped at PP6
(`DESIGN-HISTORY.md:1008-1013`); `prettyLayout.c` is unchanged in range. Four
in-family rounds read this file and cleared these arms; the first out-of-family
reader over the same lines found this in one pass. That is the argument for the
2026-08-29 ruling, in one finding.

**Bug class.** One-sided bounding-box aggregation — a parent grows for a
child's ascent and not its descent; the dual conjunct is missing. (Same family
as the PP18-1 "measured one call short" lesson: a box that under-promises on
exactly one edge.)

**Class-level test.** A containment sweep, not an example: after every
successful `ppMeasure`, walk the pool and assert for every child of every node
kind that `relBase − ascent` and `relBase + descent` lie inside the parent's
measured box — one mechanical assertion covering all six construct kinds and
every optional child, plus fixtures `ˣ√y` with fraction and construct indices
at both fonts through each capture surface's band check.

---

### PP18RR1-2 — the walker models classic ENTER lift only; in eRPN mode the drawn formula computes a different number than XEQ returns, silently

`packages/pretty-print/prettyVisual.c:834` (the unconditional
`stk->liftDisabled = true`), `:207-209` (the justifying comment), `:210-222`
(`ppvPushLifting` overwriting the dup). Found by the lifecycle dimension;
survived a correctness refutation with every link verified against upstream.

**What breaks.** In eRPN mode a *running program's* ENTER duplicates X **and
leaves `FLAG_ASLIFT` set**: `keyboard.c:3417`'s third disjunct
(`FLAG_ERPN && PGM_RUNNING`) performs the lift+dup, then `:3437-3441` re-sets
the latch, and ENTER is `SLS_UNCHANGED` so `runFunction` leaves it. The next
literal therefore **lifts**. The walker's ITM_ENTER arm disables lift
unconditionally, so its model overwrites the dup with the literal — classic
semantics, always. `prettyVisual.c` contains zero `getSystemFlag` calls; there
is no eRPN decline anywhere on the path.

**Reaching input** (traced end-to-end; not executed in a build). Set
`FLAG_ERPN` — a first-class C47 toggle with its own status-bar indicator. Store
`LBL 'A' / 1 / 2 / ENTER / 3 / × / +`. `XEQ 'A'`: stack `[1,2,2,3] → [1,2,6] →`
**X = 8**. `VISUAL 'A'`: the walker draws `1+2×3` = **7**. No decline, no
D-number — the picture asserts a number the program does not compute.

**Why it is wrong.** `prettyVisual.c:33`: *"the transpiled text computes what
the RPN computed"*, and `:37-38`: effects are *"none of them … guessed at."*
The walker's own comment at `:207-209` — *"fnRecall consults FLAG_ASLIFT for
exactly this, and ENTER clears it"* — is **false under FLAG_ERPN**, where
`fnKeyEnter` sets it. The package itself treats eRPN as in-scope for
faithfulness surfaces: the capture engine models the eRPN no-dup branch
(`prettyCapture.c:893-901`, pinned by T15) and `DESIGN.md:200` says so; the
only contrary text is a test-scoping clause in round 1's non-normative report,
which neither qualifies the faithfulness contract nor makes the walker decline.
By the design's own fail-closed principle (D6's *"a drawing may not quietly
assume"* runtime state), this wants a flag consult or an honest decline.

**Bug class.** A mode flag consulted by the mirrored engine and not by the
mirror — a configuration branch of the simulated machine omitted silently.
(PP18RR1-3 is the second member; the class is the mode axis itself, §5 D1.)

**Class-level test.** Run the V65 differential-oracle battery under **each**
stack-semantics mode, not only the build default: every oracle program executed
and drawn with `FLAG_ERPN` set and clear, asserting program value == picture
value or an explicit decline. Any fixture with a mid-program ENTER reds this
finding; the mode loop is the class-level part.

---

### PP18RR1-3 — the walker simulates SSIZE8 unconditionally; under SSIZE4 a five-deep push chain draws operands the real stack dropped

`packages/pretty-print/prettyVisual.c:49` (the choice, stated in one comment),
`:191-205` (`ppvPush`, drop at 8 slots only), `:197` (the fidelity claim).
Found independently by two dimensions (arithmetic, lens reachability; lifecycle,
lens intent) — both verdicts survived.

**What breaks.** `PPV_STACK_SLOTS` is a compile-time 8 and the walker never
reads `FLAG_SSIZE8`. Under SSIZE4 — a user command (`flags.c:747`) and the
reset default of one `config.c` personality column — the hardware's
`liftStack` drops T at **four** levels (`getStackTop()` = `REGISTER_T`,
`defines.h:2284`), while the walker keeps up to eight operands.

**Reaching input** (constructed; both sides of the arithmetic traced, not
executed). SSIZE4; `LBL 'A' / 1 / 2 / 3 / 4 / 5 / + / + / + / +`. `XEQ`: the
fifth literal's lift drops the 1 off T, the adds consume the replicated T →
**X = 16**. `VISUAL 'A'`: depth peaks at 5 < 8, no drop, no D10 → draws
`1+2+3+4+5` = **15**. No decline. (Inside construct bodies the divergence
mostly cancels — the seeded stack holds the same name on every level — which
is why the reaching input is a top-level chain of distinct values.)

**Why it is wrong.** `:197`'s comment — *"a full stack drops its bottom,
exactly as the hardware one does"* — is true only when `FLAG_SSIZE8` is set;
the hardware drops at four when it is clear. The `:49` comment (*"SSIZE8
simulated regardless of the flag"*) states the choice but carries no
faithfulness argument, and the intent search was exhaustive: `DESIGN.md` never
mentions SSIZE anywhere, its faithfulness claim is unqualified, and its
machinery for deliberate gaps (Not-in-v1, Documented gap, Rulings) records
stack-mode exclusions when it makes them (CLX is excluded *for a stack-lift
reason*) and does not record this one. A choice whose adjacent comment asserts
a hardware-exactness the choice breaks, in a design that declines every other
limit it cannot model (D8/D9/D10), is an unexamined assumption, not a ruling.

**Bug class.** Same as PP18RR1-2 — unread mode flag in the machine mirror. The
test build never leaves SSIZE8, which is why four rounds of green oracles never
saw it.

**Class-level test.** The same mode-looped oracle battery as PP18RR1-2, with
SSIZE4/SSIZE8 as the second loop axis and at least one fixture pushing five
distinct values; plus a unit pin that the walker's drop threshold tracks
`getStackTop()` rather than a constant (assert divergence → decline if the
design rules that way instead).

---

### PP18RR1-4 — the dirty-list D5 guard compares bytes where upstream resolves folded names, so `STO '<sub-x>'` then `RCL 'x'` draws the pre-store meaning with no decline

`packages/pretty-print/prettyVisual.c:897` (`ppvNameInList` on the RCL path),
`:353` (the `strcmp`), `:926-932` (the STO arm recording raw bytes). Found by
the error-paths dimension; survived an intent refutation — every ruling on the
dirty list permits over-refusal, none permits this under-refusal.

**What breaks.** Upstream executes both parameters through
`findNamedVariable`, whose `nameEqualsPrefolded` folds subscript glyphs to
plain letters — PP18R4-5's live probe already measured
`findNamedVariable("\xa4\xb3") == findOrAllocateNamedVariable("x") == 262`. So
`STO '<sub-x>'` and `RCL 'x'` hit the **same register**. The walker's dirty
list stores the raw bytes and matches with `strcmp`, so the recall misses the
list, no D5 fires, and a free VAR `x` is pushed.

**Reaching input** (constructed; the fold identity is round 4's executed
measurement, re-cited). PEM or `.p47`: `LBL 'VFO' / 2 / STO '<sub-x>' /
RCL 'x' / × / END`, subscript-x entered by the same `ITM_DOWN_ARROW` path
R4-5 measured (`bufferize.c:459-463`). `XEQ` stores 2 into x, recalls 2 → 4
regardless of x's prior value. `VISUAL 'VFO'` draws `2×x` — a picture meaning
whatever x held *before* the store (x=99 → picture 198, program 4).

**Why it is wrong.** `DESIGN.md`'s catalog: *"D5 recall of a name a STO
changed."* The code's own comments, `:898-899`: *"the emitted text would read
the variable's ORIGINAL meaning and silently ignore the store that changed
it"*, and `:916-918`: *"The NAME, though, now means something the emitted text
cannot express, so later reads of it decline."* This input is byte-for-byte the
scenario those sentences refuse, reached through a spelling the comparison
cannot see.

**Bug class.** *A mirror that re-implements upstream's comparison instead of
calling it* — PP18R4-5's named class, **recurring at a second site** the R4-5
finding and its proposed fold-matrix test do not cover (that test asserts only
the derivative's drawn variable, not dirty-list membership). Per this project's
own regression record, recurrence at a second site after the first is fixed is
the norm, not the surprise.

**Class-level test.** The fold matrix, extended to the dirty list as a class:
for every pair of spellings upstream's `CMP_NAME` folding resolves to one
register (plain/subscript/superscript variants per `sort.c`), a fixture doing
`STO` under one spelling and `RCL` under the other must either decline D5 or
draw the post-store meaning — asserted for every `ppvNameInList` consumer, not
per drawn surface.

---

### PP18RR1-5 — `ppvNameIsDrawable` is strictly narrower than the grammar its own banner cites: subscript-digit names decline D18 though `ppqName` draws them

`packages/pretty-print/prettyVisual.c:294` (the predicate), `:302` (the
letters-only conjunct), `:343-344` (`ppvVarName`'s decline), `:646`/`:664` (the
derivative tail check — a second consequence). Found independently by **two
dimensions** (contracts, lens reachability; guards, lens correctness); both
verdicts survived.

**What breaks.** The banner above the function, `:291-293`, states the
contract: *"A name has to survive the renderer's `ppqName`, which takes ASCII
letters **(and subscript digits after the first)**."* The loop implements the
letters arm and omits the subscript-digit arm it promises — no `0xa080-0xa089`
decode at all. Every `ppvVarName` site funnels through it (RCL, STO, INTEG's
variable, `f'`'s parameter), plus the derivative's final drawability check. So
a name like `X₁` — register-style spelling, common on this class of calculator
— costs the owner the **whole drawing**: *"cannot be drawn (D18)"* for a
program whose every glyph the package's own 2D grammar spells
(`prettyEquation.c:116`, `PPQ_IS_SUB`) and whose ink exists in all three fonts;
the identical formula typed as an equation draws today. On the `PGMDRV`/`f'`
path, a body whose only MVAR is `a₁` declines a derivative upstream's
`calcDeriv` differentiates without complaint.

**Reaching input** (three independent constructions). (1) Creation through the
sanctioned path: upstream's `validateName` (`registers.c:693-753`) admits `X`
then blacklists only listed ASCII punctuation, so `X` + `0xa0 0x81`
(`STD_SUB_1`) is creatable by plain `STO` — no import needed. (2) Keyboard:
`ITM_DOWN_ARROW` sets `NC_SUBSCRIPT` (`keyboard.c:517-523`) and
`convertItemToSubOrSup` maps `ITM_0..9` → `ITM_SUB_0..9` (`bufferize.c:29`) in
PEM alpha entry. (3) The `.p47` import channel round 4's P1 established.
Program `LBL 'P' / RCL 'X₁' / x² / END`, then `VISUAL 'P'` → D18.

**Why it is wrong.** The banner quoted above versus the loop below it; and
`DESIGN.md:623`'s catalog rationale — *"D18 name the grammar cannot spell"* —
does not apply: the grammar **can** spell it, and post-PP18 the walker feeds
names straight to `ppNewRun`, the same mechanism `ppqName`'s runs use, so
nothing downstream constrains it either. Caveat carried from the finder: this
*could* be deliberate conservatism (a wider predicate widens exposure to the
known R4-5 fold gap on the deriv path) — but no comment or ruling says so, and
the comment on record promises the wider behaviour. Distinct from the KNOWN
set: R4-7 judges the wrong *name* by this predicate, R4-11 is the unfalsifiable
conjunct, R4-5 is subscript *letters* on the *match* path; this is the
predicate itself being narrower than its stated contract, with a
drawable-name-refused consequence. The round-4 refutation of the adjacent
claim tested ASCII `x1` only and never constructed the glyph range.

**Bug class.** Accept-set predicate narrower than its own written contract —
a comment/code mismatch on a gate, which is worse than a plain bug because the
comment will mislead the next fixer either way.

**Class-level test.** An alphabet-parity pin: for every character class
`ppqName` accepts (enumerated from `PPQ_IS_SUB` and its letter arms), a
program-side fixture asserting `ppvNameIsDrawable` agrees — or, if the owner
rules ASCII-only deliberate, the ruling lands in the banner and the D18 catalog
line and the pin fixes *that*. Either way the truth stops living in two
disagreeing places.

---

### PP18RR1-6 — `ppqBigopConstruct` accepts an optional fifth INTEG argument and `ppqBuildBigop` silently drops it: `INTEG(X;X;0;1;2)` draws identically to the four-argument form and then refuses to compute

`packages/pretty-print/prettyEquation.c:397` (fifth argument parsed for every
kind), `:250-268` (the INTEG branch appends body/from/to and never `stepN`);
package evaluator contract at `solver/equation.c:2057-2060`. **Out-of-family
finding (Sol / GPT-5), OOF-S1**; the verifier resolved the reply's open horn:
the evaluator **rejects** five arguments (`needMax = 4` for INTEG, "wrong
number of big-operator arguments"), so the defect is the renderer drawing a
construct the evaluator errors on.

**What breaks.** The optional-argument block runs unconditionally for all four
construct kinds; only DERIV consumes `stepN` (the order check). For INTEG the
parsed operand belongs to no node: `c->failed` is never set, the linear
fallback never runs, and the drawn tree is byte-identical to
`INTEG(X;X;0;1)`. The owner sees a clean integral for an equation that then
errors on evaluation.

**Reaching input.** Type `INTEG(X;X;0;1;2)` in the equation editor and view
it. Drawn: the four-argument integral. Computed: a syntax error.

**Why it is wrong.** The renderer's own contract comment,
`prettyEquation.c:320-322`: *"the renderer has to accept exactly what the
evaluator accepts or a typed equation draws and refuses to compute, or the
reverse"* — this input is the first horn verbatim. And the strict-decline
design: *"a malformed construct sets `c->failed` and the whole parse fails."*
Severity graded honestly: no wrong number is produced (the error still comes at
compute time); this is an acceptance mismatch producing a misleading picture,
below the wrong-picture findings above it.

**Bug class.** A two-sided acceptance contract enforced on one side — the
parser's arity check exists only in the evaluator.

**Class-level test.** A differential accept/refuse oracle over the construct
grammar: for each construct kind, drive argument counts from `needMin−1` to
`needMax+1` through both `ppqParse` and the evaluator's intercept and assert
the accept sets are identical — no expected pictures or values needed, which
makes it cheaper than V65 and orthogonal to it. (PP18RR1-5 and -7 are the same
class in the opposite direction; §5 D3.)

---

### PP18RR1-7 — the drawable-monadic vocabulary is hand-enumerated in three walker sites plus the capture classifier; DESIGN.md claims a shared source (`PPC_MO`) the code cannot read, and the copies already disagree — CUBEROOT draws in PSHOW and declines D1 in VISUAL

`packages/pretty-print/prettyVisual.c:793` (`ppvMonadicName`'s switch), `:883`
(the OP1 case list), `:1258` (`ppvAstPrec`), `:1310-1345` (`ppvSerialize`);
the authoritative set at `prettyCapture.c:503-508` (`ppcClassify`, file-static).
Found by the design dimension; the reachability verifier set out to refute and
instead **reached the divergence live in the suite**.

**What breaks, today.** `2 CUBEROOT` with `FLAG_PRETTYP` draws an indexed
radical on the capture surfaces (`ppfCombine1`'s arm, `prettyFormula.c:207-215`,
shipped at PP6). The same two steps stored as a program and given to
`VISUAL` decline **D1**: `prettyVisual.c` contains zero occurrences of
`CUBEROOT` or `XFACT`. Proven in-suite: a temporary fixture
`LBL 'VCBT' / RCL a / CUBEROOT` with `ppvTestDecline(…, "VCBT", 1)` ran the
full gate GREEN — that pin passes only on a decline with reason exactly
`PPV_D_OPCODE`. And structurally: any item added to `PPC_MO` never reaches
VISUAL, and nothing fails — the copies drift one item at a time.

**Why it is wrong.** `DESIGN.md:717-718`, quoted: *"The walker emits a function
only when BOTH hold: the item is in the capture engine's `PPC_MO` monadic set
(upstream has no usable arity metadata …)"* — the code implements "is in this
file's own hand-copied subset", because `ppcClassify` is file-static and the
set was copied instead of exported. The same operation drawing as mathematics
in one front-end and erroring in another is an owner-visible inconsistency, and
the next admitted monadic must be re-enumerated by hand at four sites or the
front-ends diverge further.

**Bug class.** Hand-maintained inventory of a machine-derivable set (C15) plus
constant copied by value across a module boundary (C14) — both already paid
for in this codebase.

**Class-level test.** A membership-parity pin: enumerate `PPC_MO` through a
test seam and assert every member either transpiles in VISUAL or appears on a
named refused-list (with its refusal reason); the refused list is then a
reviewable artifact where the current silent subset is not.

---

### PP18RR1-8 — V74 guards only the numerator superscript; `ppqBuildBigop`'s second-order denominator emission has no pin from any producer

`packages/pretty-print/prettyTest.c:4594` (V74), subject
`prettyEquation.c:289-295`. Found by the tests dimension; the intent verifier
**applied the mutation the finder could only argue statically** — the
`if(secondOrder)` s2 block deleted — and the full solo gate ran **GREEN**.
Reverted; tree clean.

**What breaks.** V74 asserts `ppTreeHasRun(root, "d\xa1\x62")` — the
*numerator* run built at `prettyEquation.c:274`. The denominator's squared
glyph (`s2`, appended to `denBox`) is observed by no pin through any producer:
V53 pins the text seam serialized from the AST flag; EQ21 parses first-order
only; EQ13 goes through `ppqFrameDerivative`, a different producer; EQ22's
height/width bands cannot see a missing ~6 px glyph. If the denominator half
regresses, a second derivative draws as **d²/dx instead of d²/dx²** — a wrong
picture of a different quantity — on both producers (VISUAL on any f″ program,
e.g. V53's own VDR2 fixture, and typed `DERIV(X^3;X;3;2)`), and the suite stays
green.

**Why it is wrong.** V74 exists to close PP18-12, whose text (round-1 report
§3) is two-sided: *"`secondOrder` selects the numerator run … **AND** appends
the squared glyph to the denominator box. Dropped, a program computing d²/dx²
draws as d/dx — a different quantity, silently — and the suite stays green, so
the regression ships."* The fix commit claims *"V74 covers the second-order
derivative's node wiring"*; V74's own comment says *"could have lost its
superscript**s**"*; the body checks one run. No leave-alone or
survives-alone entry exists for the gap anywhere in `DESIGN.md`, `TESTING.md`
or `DESIGN-HISTORY.md` — and this project documents its deliberate coverage
gaps, so the silence is meaningful.

**Bug class.** Coverage credited per fix rather than per site — the exact
class PP18R4-8 and PP18R4-9 named, recurring at a third site, inside a round-1
fix. The audit-fix-regression pattern's sixth consecutive appearance.

**Class-level test.** Node-tree pins on **both** runs of the second-order
shape (`d²` numerator, `dx²` denominator) through **both** producers — VDR2's
walker output and a typed `DERIV(…;2)` equation — i.e., one pin per emission
site, not per fix. `TESTING.md`'s MUT-111 row then maps to a pin that actually
reads the node tree.

---

### PP18RR1-9 — `prettyLayout.c`'s paint pass re-derives measure's decisions by textual copy (radical synth test, paren glyph-vs-tall test, BIGOP column width) instead of reading a stored result

`packages/pretty-print/prettyLayout.c:262`/`:662` (PP_RAD `synth` + `signW`),
`:438`/`:760` (PP_PAREN `h <= parInk + 2`), `:363-365`/`:705-707` (PP_BIGOP
`colW`). Found by the design dimension; the correctness verifier proved the
missing tripwire by mutation: measure's `radInk+3` → `radInk+4` with paint
untouched, and the gate ran **GREEN** — a divergence between the copies ships
silently.

**What breaks.** Nothing today — the copies are byte-equivalent, and two of
them even carry comments admitting the duplication ("The paint pass recomputes
this same test", `:259-261`, `:439`). The defect is the shape: paint computes
`signX = x + relX − signW` where `relX` is measure's stored result but `signW`
comes from paint's own recomputed `synth`; the first person who tunes one
threshold in one copy gets a radical sign overlapping its radicand, tall parens
in glyph-paren width, or a Σ stroke off-centre — with `ppMeasure` still
reporting success, and (per the mutation) no test noticing. Every bracketed
operand the PP18 walker emits rides these arms.

**Why it is wrong.** The file's own stated pattern, `ppBigopBox`'s comment at
`:604-605`: *"Measure and paint call this same function"* — factored for
exactly this reason, and the sharing stopped at ga/gd/gw, leaving `colW` and
the RAD/PAREN tests as copies. Round 4's axis-b read cleared these arms as
laying out correctly *today*; no artifact covers the copies' agreement going
forward, which the mutation proves.

**Bug class.** Rule corrected in a subset of its copies (r5's R12) — here,
rule *shared* in a subset of its instances.

**Class-level test.** The mutation run here, as a standing pin: a debug-build
assertion (or test-seam comparison) that paint's recomputed decision equals
measure's stored one for each duplicated pair, red under exactly the
one-copy-tuned mutation that ran green this round.

---

### PP18RR1-10 — V28 re-derives the Z/T band bounds from hand-copied magic offsets (−4, +31), so a band change leaves the placement pin green and stale

`packages/pretty-print/prettyTest.c:4944` (and `:4878-4884`, `:5004-5007`,
`:5042-5043` — the offsets hand-copied at five sites); the product's
`PPV_BAND_TOP`/`PPV_BAND_BOTTOM` are file-local defines at
`prettyVisual.c:1456-1457`. Found by the design dimension; proven by mutation
under unusual provenance: the verifier's worktree **arrived** with the exact
required mutation live (`PPV_BAND_BOTTOM` → `Z+10`, a stale sibling's edit —
§8, process item 2), and the canonical gate, with the mutated band baked into
the build, ran **GREEN with no V28 output**. `grep` confirms the V28 region
references no `PPV_BAND_*` symbol; with T=24/Z=60, line `:4944` evaluates
`58 <= 72` regardless of the product's band.

**What breaks.** Nothing today — both copies say T−4 and Z+31. Edit either
offset in the product (e.g. to stop abutting the Y separator) and V28 keeps
testing the *old* 72-px band: the "58 ≤ band" claim passes while the double
integral no longer fits the real surface — the drawing overflows or falls
through to full-screen with the placement pin green.

**Why it is wrong.** `DESIGN.md:694-697`, quoted: *"V28 pins the six heights
and the two inequalities the placement rests on, so a … change that
invalidates the choice **fails loudly instead of silently overflowing** into
the Y line."* Against band-geometry changes — the pin's own subject — it
cannot fail loudly, which is the pin-vacuity class round 3 made its axis and
round 4 found three more of.

**Bug class.** Constant copied by value across a module boundary (C14), in a
pin whose purpose is to notice that constant changing.

**Class-level test.** The placement pin asserts against the product's own band
constants (exported or via a test seam) so any band edit moves both sides of
the inequality — the general rule being: a drift tripwire may not re-derive
the quantity it trips on.

---

### PP18RR1-11 — `ppvStep`'s stack-lift invariant is enforced by break-vs-return discipline across ~20 arms; PP18-5's fix patched the three offending arms rather than the shape

`packages/pretty-print/prettyVisual.c:992` (the epilogue clear), `:940`/`:960`/
`:969` (PP18-5's three hand-inserted clears). Found by the design dimension;
the correctness verifier independently re-walked every current arm and could
not refute: the invariant holds **today** at every arm (break arms reach the
epilogue; ENTER owns the latch; LITERAL/RCL route through `ppvPushLifting`,
which clears it both ways; declines latch `ctx->failed`, terminal; the skip
list is genuinely lift-neutral — see §6), but nothing structural enforces it
per arm.

**What breaks.** The recurrence path is the stage's own roadmap: `DESIGN.md`'s
Not-in-v1 list names dyadic named functions as the next emitter growth, which
means a new arm in this switch. Written like the construct arms but ending in
`return` instead of `break`, it silently keeps ENTER's latch armed across it —
and the failure is the exact PP18-5 pair: a false D10 decline at top level; a
silent wrong drawing (`a×(a+b)` drawn as `x×(a+b)`) inside a seeded construct
body. Not hypothetical in kind: it shipped once and was fixed as PP18-5.

**Why it is wrong.** PP18-5's own analysis, `prettyVisual.c:944-953`: *"these
three arms return before the epilogue that clears the latch… Upstream clears
stack-lift in the dispatch epilogue for every SLS_ENABLED item"* — upstream's
shape is a single epilogue no arm can skip; the mirror's is per-arm memory.
The epilogue comment (*"every op above finishes with lift enabled"*) documents
the invariant without enforcing it.

**Bug class.** Guard-enumerates-examples-not-class — named twice already in
this stage's own trail (PP18-4's comment, PP18R4's verdict).

**Class-level test.** Enumerable per arm: a table test driving every opcode
the walker accepts through `ppvStep` and asserting `liftDisabled == false` on
exit unless the arm is ENTER or on the documented skip list — a new arm added
without a table row fails by default, which is the property the per-arm clears
lack.

---

### PP18RR1-12 — `ppqBigopConstruct` passes raw kind literals (0/1/2/3) into `ppqBuildBigop`'s `PPQ_BIG_*` enum contract; the same mapping spelled twice, aligned only by parallel numbering

`packages/pretty-print/prettyEquation.c:352-357` (`kind = 0..3` with its own
private comment), `:381`/`:407`/`:423`/`:428-436` (raw `kind == 2` / `== 3`
comparisons) versus `prettyInternal.h:132` (`PPQ_BIG_SUM..PPQ_BIG_INTEG`,
consumed symbolically by the builder at `:220`/`:250` and the walker at
`prettyVisual.c:1140-1142`). Found by the design dimension; survived an intent
refutation with the strongest possible evidence: the commit that minted the
enum (`1fd492a48`) explicitly enumerates what stayed with the caller *"on
purpose"* — body scoping and DERIV's order — and the kind numbering is not on
that list. No ruling anywhere covers it.

**What breaks.** Nothing today: the parser's private numbering happens to equal
the enum. The enum gains or reorders a member — a SOLVE construct is the
documented open design question — and the parser's literals silently build the
wrong construct shape for every typed equation: a SUM rendering as a PROD, with
no compile error and no decline.

**Why it is wrong.** The header's own contract comment,
`prettyInternal.h:129-130`: *"node assembly, shared with the walker (PP18).
Both keep the shapes the EQ pins fix"* — sharing that holds only while two
independently-maintained numberings coincide. This is the D7 dimension's core
shape: two places that must agree and nothing forcing them to.

**Bug class.** Parallel-numbering coincidence across a module boundary — C14's
enum-shaped variant.

**Class-level test.** A compile-time identity tripwire in the test build
asserting the parser's numbering *is* the enum (the V28-lesson shape: the
tripwire must read the product's symbol, not restate its value), so a reorder
fails the build rather than the owner.

---

## 4. PLAUSIBLE

Survived refutation; nobody constructed the reaching input.

**PP18RR1-P1 — the full-screen centering truncation puts a 147-px-tall
formula's top ink row on the y=20 frame line, one row above the stated band.**
`prettyVisual.c:1517` admits `h <= 147`; for the odd maximum the truncated
centering at `:1528` yields top row `(188−147)/2 = 20` — on the frame line
drawn at `:1526` and one row above the band the comment at `:1499-1500` states
("the full band, 21..167"); every `h <= 146` stays at ≥ 21. `ppShowRun`'s
R3-13 measured-box pre-clear then blanks the frame-line segment under the
tallest run's width (polarity verified: the frame is ink `LCD_EMPTY_VALUE`, the
pre-clear is blank `LCD_SET_VALUE`). The correctness verifier granted the path
and confirmed every step of the arithmetic, including tight ascent aggregation
— the defect is real *at the boundary*. What is missing is an input measuring
`ascent+descent == 147` exactly at one of the two font rungs: heights are
discrete metric sums, nested integrals grow ~20 px per level, and no fixture or
hand-derivation produced 147 on the nose. The same admit-147/truncate pair
exists at `ppqShowRender` (`prettyEquation.c:935-937`, reachable via EQSHW) and
PSHOW (`prettyValue.c:864-868`, implausible for values). Cosmetic one-row frame
corruption at the exact boundary height; latent. *What would settle it:* the
mechanical height maximiser round 4's P2 already asked for — enumerate
layoutable trees and report reachable heights per rung — or one hand-built
deep-nesting fixture that measures 147.

**Carried forward, still open, not re-examined** (KNOWN set, listed for the
ledger only): round 4's P1 (8–14-glyph `MVAR` through the import channel) and
P2 (`PP_MAX_DEPTH` composition), round 2's P1 (non-numeric first `MVAR`),
round 3's P1/P2 (V65 ordering) and P3 (`ppvAstPrec`'s missing NIL guard).

---

## 5. Design observations (D7)

Shape, not defects. Four observations; the first two organize most of this
round's findings.

**PP18RR1-D1 — the walker is a simulator of exactly one calculator
configuration, and nothing says so.** `grep` finds zero `getSystemFlag` calls
in `prettyVisual.c`; the machine it simulates has classic lift and eight stack
levels, always. The capture engine — the package's other faithfulness surface —
reads the eRPN dup condition and is pinned for it (T15). PP18RR1-2 and -3 are
two cells of a table nobody holds: for each mode flag that changes the semantics
the walker mirrors (`FLAG_ERPN`, `FLAG_SSIZE8`, and any future stack-shape
flag), the walker either reads it, models it as a documented constant, or
declines. Writing that table down is cheaper than the next round finding cell
three. It also composes with round 4's D4 (name provenance): the walker's
missing instruments are both *tables over axes*, mode and provenance.

**PP18RR1-D2 — the PP18 unification is real and unfinished, and the residue is
one shape.** Five confirmed findings — PP18RR1-7, -9, -10, -11, -12 — are the
same statement: a truth the refactor centralised in principle is still spelled
at N sites, aligned by discipline. The stage itself proved both remedies work:
`ppfCombine` now owns precedence (the refactor's central move, byte-identical
drawings as proof) and `ppBigopBox` is shared by measure and paint precisely so
the two passes cannot disagree — every finding in this family is a place one of
those two proofs stopped short. The pattern to look for in review is the one
round 4 named: a diff that *adds* a parallel spelling instead of exporting the
existing one.

**PP18RR1-D3 — acceptance parity has no oracle, and three findings this round
are acceptance mismatches in two directions.** V65 compares *values* where both
sides accept. Nothing compares *what the two sides accept*: the renderer draws
a construct the evaluator refuses (PP18RR1-6), and the walker refuses names and
items its sibling front-ends draw (PP18RR1-5, -7). A differential accept/refuse
oracle — same input, both engines, assert same verdict — needs no expected
pictures or numbers, which makes it the cheapest instrument proposed in this
report, and it pins all three findings' classes at once.

**PP18RR1-D4 — the name-alphabet truth lives in at least five places.**
Upstream's `validateName` (what can exist), CMP_NAME folding (what is equal),
`ppqName` (what the grammar draws), `ppvNameIsDrawable` (what the walker
admits), and the dirty list's `strcmp` (what the walker remembers). PP18RR1-4
and -5 are both cells of the spelling-by-site table nobody holds — the
SPELLING axis of the same missing-table family as round 4's provenance axis
(R4-D4). Two of the five places are upstream's and cannot move; the three
package-side ones could answer through one predicate pair (exists/equal), which
is where this family of findings stops recurring.

---

## 6. Deliberately not flagged

Merged from what the ten readers reported clearing and what the refutation
pass disproved. Mandatory section; the clearances below are most of what this
round bought, because they are the difference between "the stage is riddled"
and the true statement, which is that the stage's contracts overwhelmingly hold
and the defects cluster on two axes (mode, unfinished centralisation).

### Killed by the refutation pass

**A program running VISUAL twice, tall then short, leaves half the first
drawing on screen with X and the menu suppressed** (raised against
`prettyVisual.c:1485`). Refuted on mechanism: the claimed chimera requires the
first paint's manual-mode bits to survive to end-of-run, and the program runner
destroys them — `runProgram`'s loop bottom resets `screenUpdatingMode` to
`SCRUPD_AUTO|SKIP_STATUSBAR_ONE_TIME` after **every** running step, and the
`stopProgram` epilogue leaves exactly `SCRUPD_AUTO`, which satisfies the
end-of-run guard and calls `refreshScreen(4)` → the hold-gate fails
(`TI_SHOWNOTHING` holds only when mode ≠ AUTO) → `_selectiveClearScreen`'s AUTO
branch runs `clearScreen(6)` — an unconditional full-screen `lcd_fill_rect` —
then repaints the register lines. Both paints, including the claimed stale
lower half, are erased before control returns to the owner. Whether a
program-drawn VISUAL *should* survive the end-of-run refresh is a different
question from the one reported, and upstream's own CLLCD/PIXEL program protocol
answers it the same way this code does.

**The two paint surfaces duplicate the rung ladder, and the declaration ritual
lives at a different altitude in each** (raised against `prettyVisual.c:1464`).
Refuted on intent, with the rulings cited: the per-surface ladder is
`DESIGN.md:114`'s documented architecture ("Font ladder per surface,
rebuild-per-rung"), and the two copies *deliberately* diverge in load-bearing
ways (PP18-2's clear-after-fit placement, band vs framed clear, alignment, and
"no linear fallback beneath it" stated at `:1495`); the per-site declaration
triple is the 2026-08-27 BINDING ruling, which chose upstream's own convention
("one assignment at each surface", matrix SHOW precedent) over centralisation,
and the two declarations are deliberately different (`DESIGN.md:688-696`: the
Z/T window suspends only the stack refresh so the menu keeps working; the full
screen uses the §6 PP2 protocol verbatim). The double-landing exhibit
(`layoutFull` cleared in both surfaces) was examined and cleared by round 4
explicitly. What remains of the complaint is per-site pin coverage, which is
the KNOWN R4-8/9 class.

### The lift/skip-list family — the PP18-5 class does NOT recur where it looks like it should

Four dimensions independently suspected the skip list (`LBL`/`MVAR`/`REM`/
`PAUSE`/`SNAP` returning with the latch untouched) as PP18-5's class at five
more sites, and all four cleared it by tracing upstream's runner rather than
the item table: `executeOneStep` routes `PTP_DECLARE_LABEL` and `PTP_REM` to
bare returns without `runFunction`, so the SLS epilogue never runs for LBL or
REM in a running program — SLS_ENABLED in `items.c` notwithstanding — and
MVAR/PAUSE/SNAP are `SLS_UNCHANGED`. The walker's comment ("no effect on the
pending lift either") is exactly right. Likewise cleared: a trailing ENTER's
latch surviving a subroutine return (upstream RTN is not in the SLS-clearing
set — faithful); XEQ sharing the caller's latch and the PGMINT/PGMDRV latches
persisting across construct returns (ruled, V17/D6, and matching upstream's
persistent global); `ppvPushLifting`'s latch+empty-stack combination
(unreachable — every op that could empty the stack clears the latch on its way
out); and its `depth > 0` conjunct (unfalsifiable belt-and-braces, kept under
the project's thrice-applied subsumed-guard ruling).

### Fail-closed paths that are actually closed

- `ctx->bindingCount` not decremented on `ppvBody` failure paths — every such
  path latches `ctx->failed`, which is terminal for the walk, and `ppvRun`
  reinitialises every field; no continuing reader exists (cleared by three
  dimensions and the prior out-of-family reader independently).
- `varTiny`/`varCtx` passed to `ppqBuildBigop` without a caller-side `PP_NONE`
  check — the builder's banner promises and delivers the check for exactly the
  operands each kind uses; a failure abandons the pool and the caller latches
  `c->failed`, so no partial tree escapes (the letter-of-comment ordering
  quibble in the SUM arm has no escaping consequence).
- A missing HBOX child from a `PP_NONE` append cannot mis-draw on the walker
  path: the strict-arity measure arms (`PP_BIGOP`/`PP_FRAC`/`PP_SUB`/`PP_SUP`/
  `PP_PAREN`) arity-check and fail loudly; R4-3's comment covers the two HBOX
  sites in `ppfCombine2`. Gemini cleared the variadic-HBOX case from the other
  side: measuring and painting exactly the subset that made it into the box
  honors the containment contract.
- Layout-pool exhaustion reported as D19 rather than D16 — D16 is the
  walk-time AST arena; a tree that exhausts the 72-node layout pool after both
  rungs and both surfaces is honestly "too large to draw".
- `ppvPaintStackWindow` skipping rung 1 after rung-0 pool exhaustion — rung 1
  would exhaust identically; no behavior lost.
- A corrupt opcode ≥ `LAST_ITEM` — `ppvMonadicName`'s switch filters before
  `indexOfItems` is indexed, then D1; mirrors upstream's
  `ERROR_UNDEFINED_OPCODE` guard, no OOB.
- `currentSolverStatus` stripped and restored on every exit of the paint
  region, including the D19 arm (single restore after the if/else; the decline
  path returns before the save touches anything).
- The 9th distinct STO name and ≥16-byte STO names declining D5 — fail-closed
  by the file's stated rule; refusal, not mis-drawing. Same for `RCL` of a
  fold-alias of a *binding* name declining D18 today — an honest refusal
  adjacent to the KNOWN over-refusal set (the dirty-list under-refusal is
  PP18RR1-4 and is different).
- Upstream's "counter will not count to destination" validation missing from
  the walker — VISUAL draws the program text without running it; decline-bias
  does not require refusing a program XEQ would error on.

### Boundary arithmetic that held under re-derivation

`ppvLeaf`'s `(uint8_t)len` narrowing (every caller bounded); `ppvIntern` and
`ppNewRun` size sums (promoted, no wrap); all symbolic-stack index arithmetic
(pop/underflow/depth guards, caps, sp guards); `ppFillVal`'s four-edge clip and
`ppDrawLine`'s both-ends screen (R3-11/12 fixes correct as written);
`ppDrawIntegralSign`'s zero-guarded divisor; `ppqMatchName`'s
apparent off-by-one (`pos+l >= len`) is exactly right — `ppEqConstructIs` must
read `s[l]`; `ppqPeek` fails the parse on a half-glyph rather than reading past
it; ENTER at a full stack dropping `ast[0]` is the correct mirror of dropping
the level farthest from X; the full-screen clear from y=16 is the package-wide
PP2 convention mirroring upstream's SHOW; `PPV_BAND_BOTTOM` = Z+31 (91, not 95)
is pinned deliberate. Gemini's parallel clearances in the layout engine: the
index-vs-sign pre-clear interaction is protected by construction (an indexed
root forces `synth`, stroke drawing has no glyph-box pre-clear); the `!synth`
descent formula is *flawless* once `showString`'s cell-top y-semantics are
read; the fraction-bar "BOTH sides" comment is a comment defect only (measure
and paint agree on the actual asymmetric offsets); `PP_HBOX`'s `int16_t` width
sum cannot overflow under the 72-node/512-byte pools.

### Test-harness idioms that look wrong and are right

V67's cross-range pixel comparison (and V36/V37's idiom) — sound once the
testSuite blitter's polarity is read: `LCD_SET_VALUE` fill *clears* the buffer,
so the assert fires on any band clear or stray ink. `ppcTestWriteAndLoadPgm`
not checking `lastErrorCode` — a failed load false-FAILS loudly downstream,
never false-greens. `ppfTestSigNode`'s truncation can only make actual ≠
expected. V66's visits seam counts before the latch check, so un-latching
reproduces millions of visits and the bound fires; visits==0 catches a decline.
V65 is not self-comparison (`fnExecute` vs `fnEqCalc` are independent paths)
and NaN fails in the right direction. V58/prettyTestReal fails rather than
skips on a missing file, checks both fopens, and its RX-99 preset makes the
failure count observable. V78/V79 are red-first by construction. B9/B10 and the
vacuous-pin set are KNOWN (R4-8/-9/-10/-11).

### Ruled, known, or below the bar

The walker double-parenthesising an additive DERIV body — independently
re-derived by a finder, then found to be PP18-6, ruled leave-alone by round 1
and re-affirmed by round 2: a ruling argued with and lost. `fnPrettyVisual`'s
label ladder — byte-for-byte `fnPgmInt`'s including both error arms, as its
comment claims; upstream-convention-first. `ppvSumProd`'s pop order verified
against `_programmableSumProd` (X=step, Y=to, Z=from). D-numbers never reaching
the device owner (`EXTRA_INFO_ON_CALC_ERROR` forced 0 under DMCP_BUILD) —
upstream's universal convention, PP17 baseline. The `ppvSerialize`/`ppvOperand`
unlatched DAG expansion — recorded inside round 4's report as PC_BUILD-only and
deliberately unreported; unchanged. `PPV_FRAG_MAX` now dead and the
ITM_YX/XTHROOT/LOGXY arms unreachable from the walker — stale comment /
capture-surface style, not defects (the *vocabulary* consequence is
PP18RR1-7, which is about provenance, not deadness). `ppqScopeOperand` calling
`ppNodeAt` before the `PP_NONE` check — `ppNodeAt` bounds-checks. The
stacked-power guard's child enumeration — the other OP1 children were
constructed and none escapes unbracketed. `ppvConstruct`'s second interning of
the variable name — one buffer, one call, no divergence channel. `DESIGN.md`'s
skip-list omitting `ITM_NULL` and the V28 "(standard/tiny through ppMeasure)"
phrasing — doc nits. The mirror being a hand mirror *at all* — design-
sanctioned (`DESIGN.md:628-631`), a seam call genuinely unusable (upstream's
static side-effects and register-not-name return), and its two real drift
defects are the KNOWN R4-4/R4-5. The 15-byte-name reachability — loader-
rejected upstream (`manage.c:102-104`); the open remainder is KNOWN P1.
The 2026-08-27 review's two open items (the 529-line `solver/equation.c`
extraction candidate; the §6 hook table listing 7 of 13 patched files) —
pre-dating the range and recorded in-tree.

### Sol's cleared scope paths (the packet's subject, reported clean)

Numbers/names as single leaf runs; subscript digits staying in their name run;
`ppqUnwrapParen` removing only visually-redundant parens under bars and
vincula; left-associative fraction chains; radicals consuming one primary;
function arguments always inside explicit `PP_PAREN`; right-associative carets
with base parens kept and exponent parens safely dropped (raised position
supplies scope); unary sign scoping the full term; additive big-op bodies
parenthesized by `ppqScopeBody`; SUM/PROD limits and DERIV's point attached to
the intended nodes with the order encoded; `Σx+2` treated per the source
comments; `∫Σx dx` typography left unflagged for want of a grammar contract;
`X^-2` and `A/-B` refused by the parser. Sol deliberately did not re-report the
two pre-owned scoping findings; those reductions were checked and are correct.

---

## 7. Verdict

**Would I ship this? No.** Not on the design family — on the top of the list.
PP18RR1-1 paints ink outside the measured box on the **always-on capture
surface**, five keystrokes from the default screen, in a code path every band
check trusts; and PP18RR1-2/-3 mean the flagship VISUAL surface silently draws
a formula computing a different number than X on any machine configured off the
test build's defaults — eRPN being the firmware's own signature feature. The
stage's fifth audit family repeats the standing pattern in a new key: the worst
material was not in what the stage wrote this week, but in what the process had
never pointed a different family of eyes at (the layout engine) and the
question no round had asked (the mode axis).

**Where would it break first?** On the T-line, today: `8 ENTER 1 ENTER 2 ÷
ˣ√y` with pretty print on. Then in any eRPN owner's hands the first time they
VISUAL a program with a mid-program ENTER. Then on `STO` to a subscript-spelled
name — rarer, but the picture it draws is confidently wrong mathematics.

**What is genuinely solid, verified rather than assumed.** The PP18 refactor's
central move survives its fifth adversarial pass: bracketing decided once,
byte-identical drawings, no walker-owned parentheses. The lift-latch model is
correct opcode-by-opcode against upstream's real dispatch (four dimensions
tried to break the skip list and failed). The unhappy paths fail closed
everywhere they were pushed. Upstream discipline is exemplary across the whole
stage: +13 additive test-harness patch lines, zero firmware-override growth,
churn count unchanged. And the test additions are unusually honest — the two
pixel idioms that looked vacuous are correct, and the differential oracle is
real.

**What I would leave alone if the goal were correct code rather than a clean
audit.** PP18RR1-11 and PP18RR1-12 — the invariant holds at every current arm
and the numberings currently agree; both are next-editor insurance, and the
class tests are cheap whenever the next emitter or construct actually lands.
PP18RR1-10 in isolation (the band has no scheduled change), though it is one
export away from closing. PP18RR1-P1, unless the height maximiser gets built
for other reasons. And PP18RR1-6 is worth a *ruling* more than a fix — the
evaluator still errors, so the owner is misled for exactly one keypress.
None of these computes a wrong number on a path anyone reaches.

**What should not wait.** PP18RR1-1 (live misdraw, five keystrokes);
PP18RR1-2/-3 (a ruling first — model the flags or decline — then the mode-
looped oracle, which is one loop around an instrument that already exists);
PP18RR1-4 (the second site of a class whose first site is already open — fix
them under one comparison or the third site is a matter of time); PP18RR1-5
(either widen the predicate to its own banner or rewrite the banner and the
catalog line — the current state misleads in both directions); PP18RR1-7 and
PP18RR1-8 (a front-end divergence visible today, and a half-covered guard on
the stage's marquee construct); PP18RR1-9 (the mutation that ran green this
round is the standing pin the file already argues for).

---

## 8. Round and exit state

**Round: PP18 RESTARTED round 1** — the whole stage
(`pretty-print/stage-pp17..34ac6e97f`), re-audited as round 1 under the
2026-08-29 three-family ruling. This is the first PP18 round that satisfies it.

**Readers.** Eight in-family finder dimensions (contracts, lifecycle,
arithmetic, error paths, guards, tests, design, upstream), blind to each other;
two out-of-family readers on linted packets; every finding refuted
independently under an assigned lens (reachability, correctness, intent),
default REFUTED, coverage claims proven by mutation.

**Out-of-family accounting, with survival** (same table as §1):

| reader | packet → reply | `MODEL:` line (verbatim) | raised | survived refutation |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r1/layout.md` → `/tmp/pkt-r1/layout.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 1 | **1** → PP18RR1-1 (verified broader than filed: plain `b÷a` index suffices; mint-site citation corrected to `prettyFormula.c:118`) |
| sol | `/tmp/pkt-r1/scoping.md` → `/tmp/pkt-r1/scoping.sol.reply.md` | `MODEL: GPT-5` | 1 | **1** → PP18RR1-6 (verifier closed the reply's open horn against the package evaluator: rejects at 5 args) |

Both replies present, non-empty, read, and identity-verified. **Round 1's
report records both out-of-family replies; the round counter may advance.**

**Counts.** Seventeen findings raised across the three families (fifteen
in-family, one per out-of-family reader); two refuted; fifteen survived;
deduplicated across dimensions, thirteen — **twelve CONFIRMED** (PP18RR1-1..12)
and **one PLAUSIBLE** (PP18RR1-P1, exact-boundary input not constructed). The
KNOWN sets (PP18R4-1..11, the 2026-08-29 out-of-family four, the carried
plausibles) were verified present and fenced, not re-reported.

**Independent agreement.** PP18RR1-5 was reached by two dimensions (contracts,
guards) through different consequences (RCL refusal; derivative refusal).
PP18RR1-3 by two (arithmetic via reachability, lifecycle via intent), and the
mode-axis family (-2/-3) by two dimensions overall. The out-of-family readers
agreed with nobody because nobody in-family was looking where they looked —
which is the point of the rotation, and this round's clearest evidence for the
2026-08-29 ruling: PP18RR1-1 sits in a file four in-family rounds had read.
The upstream dimension reports **CLEAN** for the whole stage — its first
no-finding verdict of this audit family, and it is evidence, not absence: the
patch surface, manifests and mirror were re-verified, and its two real drift
candidates reduced to KNOWN findings.

**Evidence discipline.** Five findings are backed by an executed
mutation/probe applied, observed and reverted (PP18RR1-7, -8, -9, -10 by gate
runs with the mutation baked in; PP18RR1-1 by a glyph-metric model first
validated against the pinned M1/M2 numbers). The wrong-picture and refusal
traces (-2, -3, -4, -5, -6) are constructed hop-by-hop against upstream but
were **not executed in a build**, and PP18RR1-4's fold identity is round 4's
executed measurement re-cited. No simulator ran; no finding rests on an LCD
photograph. Main tree clean at start and finish.

**Exit criterion: not met.** Twelve CONFIRMED findings; the criterion is two
consecutive rounds with no new CONFIRMED finding, at least one of them
out-of-family. The clock starts properly only now: this round is the first
that counts as round 1 at all.

**Process items.**

1. **Stale worktrees, now a standing defect across two audit families.**
   Verifier worktrees again spawned at `e21af8d28` (110 commits behind, not an
   ancestor of the tip); every affected verifier detected it and checked out
   `34ac6e97f` before reading. Rounds 2, 3 and 4 each requested the
   `git merge-base --is-ancestor` guard in `audit-workflow.js`; it is still
   absent.
2. **Worktree contamination — new, and worse than staleness.** One verifier's
   worktree *arrived* containing a live foreign mutation
   (`prettyVisual.c:1457` → `Z + 10 /* MUTATION V28-verify */`) written by a
   stale sibling working the same finding, with that sibling's gate observed
   still running in a neighbouring worktree. Per instruction the verifier did
   not revert the foreign edit (it was exactly the mutation the verification
   required, and was used as such); the edit was left in place in that worktree
   and is **not** in the main tree. Two worktrees of one run mutating one line
   is a serialization defect in the verifier fan-out; until fixed, every
   verifier's existing first-action check must also diff its worktree against
   the audited tip, not just check the ref.
3. **A verifier verdict record is internally contradictory.** PP18RR1-10's
   structured verdict carries `why: REFUTED` against `verdict: SURVIVES` and
   evidence that proves survival (mutated band baked into a green gate, no V28
   output, zero `PPV_BAND_*` references in the pin region). The evidence
   decides it — CONFIRMED — but a machine reading the verdict field pair would
   have filed it either way. The verifier output format wants a lint: `why`
   must not begin with the opposite verdict token.
4. **The manual-run trap** (§2): invoking the testSuite binary by hand from the
   shadow tests directory false-fails V58 on the appnote-22 file path. The
   canonical gate is the only trustworthy runner for `pretty_visual_real`.
5. **The skill-defect fix is still only proposed.** This round complied with
   the 2026-08-29 ruling by hand; `audit-workflow.js` and the skill still do
   not enforce the §1 accounting or throw on a missing out-of-family arg
   (`HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md` §8 lists the
   verification steps for the fix, unexecuted).

**Round 2's axis, in priority order.** (1) **The fix wave for this report,
when it lands** — the audit-fix-regression pattern is now six-for-six, and the
two shapes to hunt are the ones this stage already named: state relocated, and
a parallel spelling added instead of an export shared. (2) **The mode axis,
mechanically**: the producer-by-flag table of D1, plus the mode-looped oracle —
one loop around V65's existing instrument settles -2/-3's whole class,
including cells nobody has named yet. (3) **The acceptance-parity oracle** of
D3 across `ppqParse`/evaluator/walker — it pins -5, -6 and -7's classes without
expected values. (4) `prettyCapture.c` and `prettyValue.c` internals, which
PP18RR1-1 paints through and which have still never had a full pass. At least
one out-of-family reader stays in rotation per the ruling; the layout packet's
yield this round argues the next packet should be the capture staging machine.
