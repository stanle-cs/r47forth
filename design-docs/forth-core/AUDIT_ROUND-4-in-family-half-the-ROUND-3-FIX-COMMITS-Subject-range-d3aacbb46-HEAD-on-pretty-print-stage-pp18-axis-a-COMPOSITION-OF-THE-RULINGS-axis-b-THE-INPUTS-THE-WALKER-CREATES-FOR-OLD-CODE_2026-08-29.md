# Audit round 4, in-family half — the round-3 fix commits, at `d5b61ab8c`

*(Filename truncated from the full subject line, as rounds 2 and 3 were: the
ext4 limit is 255 bytes and the tasking's subject is 454. The rotated axis, the
fix inventory and the pre-verified exclusions are reproduced in §1 rather than
in the name.)*

Subject: `d3aacbb46..HEAD` on `pretty-print/stage-pp18` — two commits, the wave
that closed round 3's seven findings, plus the mechanical warning fix that
preceded it. Eight finder dimensions ran blind to each other; every raised
finding then went to an independent refutation pass with one assigned lens
(reachability, correctness, intent), instructed to default to REFUTED.

**Eleven CONFIRMED findings, five PLAUSIBLE (three carried forward), two
REFUTED.** Nineteen findings were raised and survived refutation; deduplicated
across dimensions they are eleven. Fifteen of the nineteen are backed by a probe
or a mutation *applied, observed and reverted* inside an isolated worktree; two
carry a **number** measured this round — the value the program returns against
the value the drawing evaluates to, through `fnExecute` and `fnEqCalc` in the
real build — and a third inherits round 3's executed measurement of the same
import.

**The round asked two questions.**

**(a) Composition of the rulings — are any two in conflict, or does any pair
leave a shape neither covers?** No two are in conflict. Every pair a reader put
under load composed correctly, including the one that looked worst on paper
(R2-4's "a closed sibling's counter is reusable" against PP18R3-5's "a construct
in my limits is not a sibling" — the two govern disjoint bound scopes and agree).
What the axis found is the other failure mode, three times: **a ruling stated
over the drawn formula and implemented at one producer.** PP18-9's scope rule
lives only inside `ppvInventName`, so `INTEG` and the `MVAR` path never ask it
(PP18R4-6). R2-3's "judge the name we END UP with" lives in `ppvDerivVariable`
while `ppvVarName` still judges the `f'` parameter, which the drawing never
spells (PP18R4-7). And the worst finding of the round is a ruling that was not
composed with the fix that came after it at all: **PP18-3's DAG latch is a
property of `ctx->ast`, and PP18R3-5 added a second walk over `ctx->ast` without
it** (PP18R4-1). The instrument that would have caught all three is a table over
*name provenance* and *traversal kind*; neither exists.

**(b) The inputs the walker creates for old code.** `prettyLayout.c` and
`ppfCombine1`/`2` hold up. Deeper construct nesting stays flat because a nested
body is deliberately passed at `PPF_PREC_ATOM`; a `PP_BIGOP` inside a `PP_PAREN`
inside a `PP_SUP` measures and paints correctly through the synthesized
tall-delimiter branch, and is not even a new shape — `ppfWrapIf` has produced it
since R2-2; and the ENTER-DAG cannot produce a shared *layout* node, because
`ppvAstToNodes` allocates fresh nodes per visit, so the sibling chain and
`ppSetFontDeep` are safe on a proper tree. **The DAG is the one shape old code
got right and new code got wrong.**

**Four of the eleven findings are in code or documents this wave wrote**, and
the worst of them freezes the calculator. Two more are round-3 findings closed
in prose only: PP18R3-4's constant is recorded as fixed in the commit message
and in `DESIGN-HISTORY.md` and is not in the tree, and PP18R3-6's rename moved a
duplicate pin onto a number the finding itself named as taken. That is the fifth
consecutive round in which the round's worst finding came from the previous
round's fix.

Nothing was fixed. The tree this report finishes on is the tree it started on;
every probe was reverted in the worktree that made it and the gate is green at
`d5b61ab8c`.

---

## 1. Subject and coverage

**Tip.** `d5b61ab8c` on `pretty-print/stage-pp18` ("pkg: audit round 3 — the
class had three producers, and two fixes bit back"). Range `d3aacbb46..HEAD`:

| commit | what it did |
|---|---|
| `10e49e084` | `snprintf` into a right-sized buffer in V77's expectation (mechanical half) |
| `d5b61ab8c` | the round-3 wave — PP18R3-1, -2, -3, -5, the pop reorder, the `B9`→`B10` rename, the `R2-*`→`PP18R2-*` retag, pins V78/V79/V80/EQ35 |

**Diff.** 11 files, +1,695 / −91. The code write set is three files:
`prettyVisual.c` (+90/−…, four hunks), `prettyTest.c` (+127/−…),
`prettyEquation.c` (+33), plus their generated `files/` twins and the refresh
manifest. The rest is documentation: `DESIGN-HISTORY.md` (+68/−…),
`TESTING.md` (+35/−…), `DESIGN.md` (one line) and round 3's own report checked
in (1,175 lines). **No `patches/` file changed**, so the wave adds no upstream
override and nothing new to conflict on a rebase.

**The fixes under review** (as characterised in the tasking, verified against
the diff):

- **PP18R3-1** — `ppvDerivative` passes `invented` rather than a hardcoded
  `false` as `ppvBody`'s `synthetic` argument (`prettyVisual.c:742`), so the
  `ITM_RCL` shadow guard arms for an invented derivative name and not for one
  taken from the body's `MVAR`.
- **PP18R3-2** — `ppvDerivVariable` records the FIRST declaration
  unconditionally (`:659-661`) and judges drawability once, at the end, against
  the name actually chosen, declining if it cannot be drawn (`:664-666`).
- **PP18R3-3** — `prettyEquation.c` gained `ppqScopeOperand` (`:141`), which
  wraps a `PP_BIGOP` in a `PP_PAREN` at three sites: the `^` base (`:599`), the
  trailing superscript run (`:621`), and the product's left operand (`:681`).
  The parser has no precedence value, so the node KIND decides.
- **PP18R3-5** — `ppvInventName` also scans the limit subtrees through the new
  `ppvNameInSubtree` (`:550`, called at `:583-585`) for free variables and for
  other constructs' bound names, while still ignoring closed siblings.
- Plus: the derivative's pops reordered so the point is available before a name
  is invented (`:704-712`); a second pin named `B9` renamed `B10`; audit tags
  `R2-*` → `PP18R2-*`.

**Rotated axis.** Round 1 asked whether the refactor was faithful. Round 2 asked
whether fixing disturbed the neighbours and whether the new refusals are honest.
Round 3 asked about pin vacuity and the shipped-surface change. Round 4's
in-family half asks what is left: **(a) do this component's ~dozen rulings
compose** — the derivative's sampled variable, name invention and its synthetic
flag, the shadow guard, construct precedence in three producers, body-vs-operand
scoping, the two redundant layout guards, the decline catalog — and **(b) does
anything downstream assume a shape the walker's new trees violate** (deeper
nesting, a construct inside a `PP_PAREN` inside a `PP_SUP`, and expression trees
derived from a DAG).

**Numbering.** This round's findings are **`PP18R4-1`–`PP18R4-11`** and its
design observations **`PP18R4-D1`–`PP18R4-D6`**. `grep -rn PP18R4` over the
repository returns nothing outside this report, and nothing here is renumbered
from an earlier series. Prior series in use, all greppable and all distinct:
`A1`–`A14` (PP1–PP16), `PP18-1`–`PP18-16` and `D18-1`–`D18-7` (round 1),
`R1-1`–`R1-3` (out-of-family), `PP18R2-*` (round 2, retagged by this wave),
`PP18R3-1`–`PP18R3-7` and `PP18R3-D1`–`PP18R3-D6` (round 3).

**Read at line level** (union across the eight dimensions): the two-commit diff
by all eight. `prettyVisual.c` in full (1,623 lines) by five, and the
`d3aacbb46` version of the changed functions by four — the before/after read is
what establishes PP18R4-1 as a regression rather than a pre-existing limit, and
`git show d3aacbb46:…| grep -c ppvNameInSubtree` returning **0** is the single
most load-bearing fact in this report. `prettyEquation.c` (975) in full by two
and at `ppqScopeOperand` / `ppqUnwrapParen` / `ppqScopeBody` / `ppqBuildBigop` /
`ppqBigopConstruct` / `ppqPrimary` / `ppqFactor` / `ppqTerm` / `ppqExpr` /
`prettyTryEquation` / `ppqShowRender` / `fnPrettyEqShow` by five.
`prettyFormula.c`'s `ppfParen` / `ppfWrapIf` / `ppfCombine1` / `ppfCombine2` /
`ppfBigop` / `ppfVariableName` / `ppfLabelName` by five. **`prettyLayout.c` was
read end to end (847 lines) for the first time in four rounds**, by the
boundaries-and-arithmetic reader; four other readers took its `PP_BIGOP`,
`PP_PAREN`, `PP_SUP`, `PP_SUB`, `PP_RAD` and `PP_HBOX` measure and paint arms
plus `ppNodeAt` / `ppAppendChild` / `ppSetFontDeep` / `ppFillVal` / `ppDrawLine`
/ `ppDrawIntegralSign`. `prettyInternal.h` in full. `prettyTest.c`: the whole
diffed region plus V4–V6, V44, V62–V66, V71, V77–V80, EQ14–EQ35, B9/B10/B11,
the VISUAL fixture block (`:4055-4260`), `oracle[]`, and the harnesses
`ppvTestExpect`, `ppvTestDecline`, `ppvTestBuildNodes`, `ppfTestExpect`,
`ppfTestSigNode`, `ppcTestExpectSig`, `ppcTestWriteAndLoadPgm`.

**Upstream read by execution path:** `src/c47/solver/differentiate.c`
(`deriv_pgm_variable` :286-325, `derivativeVariable` :180-190,
`_differentiatorIteration`, `calcDeriv` :430-445); `src/c47/registers.c`
(`findNamedVariable` / `nameEqualsPrefolded` / `foldNameToCharCodes` :920-992,
`allocateNamedVariableOnMiss` :962-965); `src/c47/sort.c`
(`compareString` CMP_NAME, `_charCodeUnSupSubStruck` :21-29, :74-86);
`src/c47/store.c` (`_storeValue` :172-217, `fnStore` :219-232);
`src/c47/defines.h` (`MAX_LABEL_NAME_LENGTH` :1188, `FIRST_LETTERED_REGISTER`
and the nine stack registers :1271-1284, the `_IN_KS_CODE` twins :1406-1419);
`src/c47/programming/manage.c` (`boundProgramNameLength` :91-99,
`programMemoryHasOverlongLabelName` :102-115, the PEM alpha branch :872-874);
`src/c47/programming/lblGtoXeq.c` (`regKStoC` :481-482, the
`STRING_LABEL_VARIABLE` parameter resolve :486-495);
`src/c47/saveRestorePrograms.c` (`_screenFileStep` :141-152);
`src/c47/keyboard.c` (`NC_SUBSCRIPT` :517-523, the catalog/EIM gate :1226);
`src/c47/bufferize.c` (`convertItemToSubOrSup` :26-36, `addItemToBuffer`
:459-463, `NumMsg[]` :439); `src/c47/items.c` (the `ITM_MVAR` row :3376,
`ITM_SUP_2` :2848, `ITM_VISUAL` :2831); `src/c47/programming/decode.c`
(`getStringLabelOrVariableName`'s own clamp, `countLiteralBytes`).

**Docs read:** `DESIGN.md`'s §6 VISUAL section, the Rulings block and the
decline catalog in full by six, including the RETIRED-alphabet paragraph
(`:586-591`) *to its last sentence*, which is what refuted one finding;
`DESIGN-HISTORY.md`'s 2026-08-28 entries by all eight; `TESTING.md`'s mutation
catalog and V/B families by six; **round 3's report in full by five, including
its §4 and §6**, so nothing it already killed was re-raised unknowingly; round
2's report at its PLAUSIBLE list; round 1's report at PP18-1/-3/-4/-8/-9 and its
exclusions section (`:1280-1310`); the `upstream-diff-review` skill's
`SKILL.md` and all 13 entries of `references/deliberate-exceptions.md`.

**Not reached, and it matters where.** `prettyCapture.c`'s staging state machine
— only its `ppfCombine` contract was checked, through `ppfBigop`. `prettyValue.c`
beyond its `ppfBuildCurrent` and `ppReset` call sites. `prettyTest.c` outside
the diffed region and the named pins, so this report cannot claim there is no
*other* stale identifier beyond the `B9`/`B10` pair traced in PP18R4-10. The
browsers, the softmenu stack and `FLAG_ALPHA` (untouched by the range).
**No reader ran the simulator**, so no finding here is backed by a photograph of
an LCD: the drawing evidence is transpiled strings, node signatures from
`ppfTestSigNode`, measured node geometry, and — for five findings — the *number*
the drawing evaluates to. One finding, PP18R4-7, has no runtime probe at all and
says so.

**Process fact, third round running.** Every verifier worktree again spawned at
`e21af8d28`, a forth-core README commit on an unrelated branch (110 commits
behind the tip, and `d5b61ab8c` is not an ancestor of it). Round 2 asked for a
`git merge-base --is-ancestor` guard before round 3; round 3 asked again; it is
still not there, and every verifier this round spent a step detecting the stale
ref and running `git checkout d5b61ab8c`. Every verdict in this report states
the ref it worked at.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh` is green at `d5b61ab8c`, solo
and combined (pre-verified in the tasking; re-run to completion by seven
verifiers after reverting their probes — recorded tails `PRETTY-PRINT GATE
GREEN`, exit 0, testSuite OK in 169–303 s). Compiler warnings clean. Not
re-reported as a discovery.

**Generated output in sync.** `prettyVisual.c`, `prettyEquation.c` and
`prettyTest.c` hash-match their `files/` twins at HEAD (re-checked for this
report), so the build reads the code this audit read. This matters for
PP18R4-4: the unfixed predicate is in the generated tree too, not only in the
working area.

**Mutation set.** MUT-131..MUT-134 red-verified in the tasking and not
re-litigated. MUT-118/119 surviving alone, V66's visit-count assertion, and the
DBLINT rows-20-91 comparison are pre-verified design and were likewise left
alone.

**Probes and mutations this round ran**, all applied, observed and reverted
inside isolated worktrees, none numbered (the catalog is the owner's to extend):

| probe | result |
|---|---|
| `2 (ENTER ×)^k 1 1 SIGMAn` walked through `ppvRun`, with a counter in `ppvNameInSubtree` | k=16 → 524,295 visits; k=20 → 8,388,615; k=24 → 134,217,735; k=28 → 2,147,483,655. **Three readers measured this independently and agree to the unit at k=20 and k=24.** `layoutVisits` flat at 64–88 throughout → **PP18R4-1** |
| the same at k=45, with an early-return bypass so it terminates | `inventCalls=1 step=94 D16` — `ppvInventName` IS entered with the depth-45 DAG root; the arena decline fires only *after* it returns |
| `LBL 'A' 2.5 3.5 STO Y −` through V65's differential oracle | `program=0.0 picture=-1.0 text=2.5-3.5` → **PP18R4-2** |
| `2 × PROD(X;X;1;3) × 3` through `ppqParse` | `[[2 · B(x\|[x = 1]\|3)] · 3]` — no `P(…)`. The same construct as the FIRST factor gives `[P(B(x\|[x = 1]\|3)) · 3]` → **PP18R4-3** |
| the same text through `setEquation` + `fnEqCalc` | `144` measured for the `;1;4` variant, whose picture reads 3,888. The `;1;3` variant is 2·6·3 = 36 by arithmetic on the same stored text, against a picture that reads 324 |
| `len > MAX_LABEL_NAME_LENGTH` substituted at `prettyVisual.c:635` | gate GREEN, solo and combined — round 3's "blast radius empty" reproduces, and **no pin covers the mirrored bound** → **PP18R4-4** |
| `MVAR 'y'` then `MVAR '<sub-x>'`, driver `f' 'x'`, body `RCL x ENTER ×` | draws `DERIV(x×x;y;3)`; live in-binary probe `findOrAllocateNamedVariable("x")=262`, `findNamedVariable("\xa4\xb3")=262` → **PP18R4-5** |
| `PGMINT 'VNB' / 1 / RCL n / INTEG 'n'` | draws `INTEG(n;n;1;n)` — the PP18-9 picture at the producer PP18-9 never touched → **PP18R4-6** |
| `PGMINT 'VNB' / 1 / 1 3 1 SIGMAn 'VNB' / INTEG 'n'` | draws `INTEG(n;n;1;SUM(n;n;1;3))`; V80 asserts `SUM(m;m;1;SUM(n;n;1;3))` for the identical nesting at the sibling producer |
| `ppvVarName`'s drawability gate removed from the DERIV parameter path | gate GREEN, both gates — no pin depends on it → **PP18R4-7** |
| `ppvInventName(ctx, PPV_NIL, PPV_NIL, PPV_NIL)` substituted at `:723` | gate GREEN; with a probe present the picture changes `DERIV(m×m;m;SUM(n;n;1;3))` → `DERIV(n×n;n;SUM(n;n;1;3))` → **PP18R4-8** |
| the `ppqScopeOperand` call deleted at `:621` | gate GREEN (mutant survives) → **PP18R4-9** |
| the same deletion at `:599` (positive control) | RED, `EQ35 … expected 'S(P(B(x\|[x = 1]\|3))\|2)', actual 'S(B(x\|[x = 1]\|3)\|2)'` — the gate does propagate the edit |
| `ppqParse("SUM(X;X;1;3)" STD_SUP_2)` | `[P(B(x\|[x = 1]\|3)) ¡b]`, 48×40 px — site `:621` is live code, not dead, and 40 px exceeds `prettyTryEquation`'s 23 px refusal |
| `if(1)` substituted for the drawability test at `:646`, and a decline forced on `!ppvNameIsDrawable(param)` at the top of `ppvDerivVariable` | both GREEN — the conjunct is unreachable, **and no fixture ever satisfies `strcmp(nm, param) == 0` at all** → **PP18R4-11** |
| the free-variable arm of `ppvNameInSubtree` (`:556-559`) deleted | GREEN — true, and refuted as a finding under the project's masked-guard ruling (§6) |

**Upstream churn.** `patch_churn_scan.py` over all 13 patches at HEAD: **790
added / 23 deleted upstream lines across 39 hunks — byte-identical to round 3's
count**, and 1 mechanical churn finding, the `[WS-ONLY]` wrap-reindent of
`showString` in `010-solver__equation.c.patch` (shadow source
`solver/equation.c`). Catalogued, pre-dating the range, owned by
`REVIEW_upstream-minimality_2026-08-27.md`, and not re-reported here under
`CODE_AUDIT.md`'s rule 6. `git diff --name-only d3aacbb46..HEAD --
packages/pretty-print/patches/` returns nothing.

**`design-audit.sh`** is forth-core's; there is still no pretty-print
equivalent, so no override-budget check ran. The substitute check is the empty
`patches/` diff above.

**One gate trap, hit twice this round and once in round 3.**
`./packages/forth-core/build-test.sh` sets `CUSTOM_PKG=packages/forth-core`, so
`prettyVisual.c` and `prettyEquation.c` are not in the shadow tree and are never
compiled. Two verifiers ran a pretty-print mutation through it and got a green
that meant nothing; both caught it because the headless log contained no
`prettyPrint test` lines at all. The governing gate for this package is
`./packages/pretty-print/build-test.sh`. Recorded again in §8.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. A frozen calculator outranks a
silent wrong picture, which outranks a refusal of a program that could have been
drawn, which outranks a coverage claim the pins do not deliver.

---

### PP18R4-1 — `ppvNameInSubtree` walks the ENTER-DAG once per path with no latch, so VISUAL on an ordinary repeated-squaring program never returns

`packages/pretty-print/prettyVisual.c:550` (the function), `:565` (the
recursion), `:583-585` (its only call sites), reached from `ppvSumProd:765` and
`ppvDerivative:723`

**What breaks.** PP18R3-5 added a second structural traversal of `ctx->ast`:

```c
static bool_t ppvNameInSubtree(const ppvCtx_t *ctx, uint8_t n, const char *name) {
  …
  for(uint8_t c = 0; c < 4; c++) {
    if(ppvNameInSubtree(ctx, a->child[c], name)) {
      return true;
    }
  }
```

`ctx->ast` is not a tree. `ITM_ENTER` (`:833`) pushes `stk->ast[stk->depth-1]`
verbatim and `ppvOp2` (`:249`) stores the two stack indices unchanged, so
`2 ENTER × ENTER × …` builds a *k*-deep DAG in *k*+1 nodes and this walk
re-descends every shared child once per path. There is no visited set, no memo,
no counter and no latch — and there cannot be one as written, because the
function takes a `const ppvCtx_t *` and so structurally cannot touch
`ctx->layoutVisits` / `ctx->layoutFull`.

**Reaching input** (executed, three times, by three readers who did not see each
other's work). The shape is already in the file: `pgmXP` at `prettyTest.c:4141`
is this program at k=20, the V66 fixture.

```
LBL 'VZZ'
2
ENTER  ×          ( repeated k times — repeated squaring, the ordinary RPN idiom )
1                 ( to )
1                 ( step )
SIGMAn 'VNB'      ( VNB = the existing empty-body fixture )
END
```

Measured, with a counter in `ppvNameInSubtree` and the walk driven through the
shipped `ppvRun` entry:

| k | `ppvNameInSubtree` visits | `layoutVisits` | desktop time |
|---:|---:|---:|---:|
| 16 | 524,295 | 64 | 0.001 s |
| 20 | 8,388,615 | 68–70 | 0.020 s |
| 24 | 134,217,735 | 74 | 0.32–0.61 s |
| 28 | 2,147,483,655 | 78 | 5.2 s |

The closed form is 2^(k+3)−3 plus ten for the two limit literals, matching to
the unit. `layoutVisits` flat at 64–88 is the control: **PP18-3's latch works
perfectly and is irrelevant**, because the time is burned before layout is
entered. At the arena maximum — k=45, which is exactly `PPV_AST_NODES` = 48
nodes (1 base literal + 45 OP2 + 2 limits) and 94 of a 256-step budget — the
single call is ~2.8×10^14 visits: about fifteen days on an x86 desktop,
unbounded in any human sense on an 80 MHz DM42n.

Budgets do not pre-empt it, and this was checked rather than assumed. A bypass
probe at k=45 printed `inventCalls=1 step=94 D16`: `ppvInventName` **is** entered
with the depth-45 DAG root, and the `PPV_D_ARENA` decline fires only afterwards,
when `ppvConstruct` tries to allocate with `astUsed` already at 48. At k≤28 the
walk completes and `ppvRun` returns a valid root.

`ppvDerivative` reaches it identically and one node cheaper: `PGMDRV 'VB5'` (the
existing MVAR-less body), the same `ENTER ×` chain as the point, then `f' 'x'` —
`:723` passes the DAG root as `r1`. Round 3's own pop reorder is what put the
DAG into that argument.

**What the owner sees.** They press VISUAL on a stored program and the machine
stops responding. Nothing is painted — `fnPrettyVisual`'s own comment says the
tree is built entire before any pixel is touched — so the previous screen stays
up with no error, no D-number and no progress. Only a reset recovers it, and the
program being typed is lost. At a merely careless 25 dups it is tens of seconds
of a frozen machine; at 45 it does not come back.

**Contract violated.** `prettyVisual.c:112-118`, the comment on the field that
exists for exactly this:

> **AUDIT PP18-3.** ENTER pushes the SAME node twice, so the tree is a DAG and
> the layout pass would visit a shared child once per path: 2^k for k dups.
> Exhausting the 72-node layout pool does not stop it, because `ppNewBox`
> returning `PP_NONE` is a per-call value and not a latch — so the walk keeps
> doubling through failures that cost nothing. **This IS the latch.** Visits are
> counted so a pin can assert the bound rather than a wall-clock time.

`ppvAstToNodes` was given `layoutFull` + `layoutVisits` for this reason and V66
pins the bound. The traversal added one round later allocates nothing, so it has
neither a latch nor a bound — and it is not a variation on the old walk, it is a
walk over the *same structure*.

**This is a regression, not a pre-existing limit.**
`git show d3aacbb46:packages/pretty-print/prettyVisual.c | grep -c ppvNameInSubtree`
returns 0. Before PP18R3-5, `ppvInventName` consulted only `ppvNameInList` (a
bounded array scan) and `ppvNameUsedInAst` (a *flat* loop over `ctx->astUsed`,
`:521-538`, which is deliberately flat and cannot blow up). PP18R3-5 added the
only recursive, DAG-blind traversal on the shipped path.

**Bug class.** *A bound that belongs to a data structure, implemented on the one
function that first met it.* The sibling shape this project already names is
"a class fixed at the site where it was noticed"; this is its dual — a
*guarantee* installed at the site where it was needed.

**Class-level test.** A visit-bound pin in V66's shape, over
`ppvNameInSubtree`'s counter, asserted for the same `VXP` fixture — not a
wall-clock pin, which passes on a desktop for a program that hangs the
calculator (V66's own comment says so), and **not** an `oracle[]` entry: the
text back end `ppvSerialize`/`ppvOperand` (`:1268`, `:1281`) expands the same
DAG unlatched, so a doubling program appended to V65 would hang the test suite
rather than fail it. Enumerable form: one pin per traversal of `ctx->ast` —
there are three on the shipped path (`ppvNameUsedInAst`, `ppvAstToNodes`,
`ppvNameInSubtree`) and each must state whether it is flat, latched, or neither.

---

### PP18R4-2 — `ITM_STO` to a lettered STACK register is modelled as leaving the stack unchanged, so the walker draws an expression built from operands the program overwrote

`packages/pretty-print/prettyVisual.c:915`

**What breaks.** The arm reads `pa[0]`, declines on `INDIRECT_REGISTER` /
`INDIRECT_VARIABLE`, appends to the dirty list on `STRING_LABEL_VARIABLE`, and
otherwise falls through to a bare `break;` with the symbolic stack untouched.
For a numbered register or a named variable that is right. For a **lettered**
register it is not: `FIRST_LETTERED_REGISTER == REGISTER_X == 100`,
`REGISTER_Y == 101`, and in SSIZE8 the lettered band `A`–`D` is stack levels
5–8 (`defines.h:1271-1284`). `STO Y` *is* a stack write.

**Reaching input** (executed):

```
LBL 'A'
2.5
3.5
STO Y
−
```

`XEQ 'A'` returns 0: `fnStore(101)` → `_storeValue` falls to
`copySourceRegisterToDestRegister(REGISTER_X, 101)` (`store.c:215`), so Y := X
and Y−X = 0. The parameter byte really is 101 in program memory — the KS-code
twins start at the same 100 (`defines.h:1406-1419`) and `regKStoC` is the
identity over that band (`lblGtoXeq.c:481-482`).

Measured through **V65's own differential oracle**, which is the strongest
evidence form this project has:

```
ZPROBE65 VZSY program=0.0 picture=-1.0 text=2.5-3.5
prettyPrint test FAIL: V65 VZSY: the picture and the program
```

**What the owner sees.** `VISUAL 'A'` draws `2.5−3.5` — a picture worth −1 — for
a program whose answer, sitting in X directly underneath the drawing, is 0. No
decline, no D-number, both numbers on screen at once. `STO Z` and `STO T` are
wrong at any stack size; `STO A`–`STO D` join them under SSIZE8.

**Contract violated.** The arm's own comment, `prettyVisual.c:916-918`:

> STO copies X: the stack picture is unchanged. The NAME, though, now means
> something the emitted text cannot express, so later reads of it decline.

The second sentence is the one that was reasoned about, and it is correct. The
first is false for a lettered register. Round 1 recorded this arm as cleared,
but only for the dirty-list question —
`AUDIT_PP18…_2026-08-28.md:1291`:

> `ITM_STO` to a numbered or lettered register not entering the dirty list. Safe
> by construction, not by luck: `ppvVarName` declines every read of a non-named
> register with D7, so no such name can reach a drawing.

That reasoning covers later **reads** of the register by name. It says nothing
about the **write's** effect on the modelled stack, which the very next
arithmetic step consumes without naming anything. This is the shape the pair of
rulings leaves uncovered, and it is the axis-(a) question answered from the
wave-0 side.

**Bug class.** *A ruling cleared on one question, read as clearing the arm.*
The clearing note is about names; the defect is about depth.

**Class-level test.** A stack-write matrix through V65's differential oracle,
which needs no expected string: `STO X`, `STO Y`, `STO Z`, `STO T` (and `A`–`D`
under SSIZE8) each followed by one arithmetic step that consumes the overwritten
level. `STO` to a numbered register and to a named variable are the negative
cells and must still draw. The enumeration is closed — the register bands are
enumerable from `defines.h`.

---

### PP18R4-3 — `ppqScopeOperand` scopes only the accumulated LEFT operand, so a big operator that is not the first factor still takes the next factor into its body

`packages/pretty-print/prettyEquation.c:681` (the scoping call), `:686` (the
unscoped append)

**What breaks.** PP18R3-3's guard is applied to `n`, the accumulated left side,
at all three of its sites; `rhs` is never passed through it anywhere in the
file. In `ppqTerm`'s product arm that has two consequences at once. Iteration 1
appends the bare `PP_BIGOP` as `rhs` (harmless while it is last). Iteration 2
then asks `ppqScopeOperand` about `n`, which the previous iteration rebound to
its own `PP_HBOX` — kind `PP_HBOX`, not `PP_BIGOP`, so the guard is a no-op even
though the construct is still the rightmost thing drawn inside it.

**Reaching input** (executed). Store `2 × PROD(X;X;1;3) × 3` (the multiply glyph
is `STD_CROSS`, which `PPQ_IS_PROD` accepts) and press `EQSHW`, or let the
equation strip row render it. Measured through the real parser:

```
PROBE middle factor  actual '[[2 · B(x|[x = 1]|3)] · 3]'     <- no P(...)
PROBE first  factor  actual '[P(B(x|[x = 1]|3)) · 3]'        <- PP18R3-3 fires
PROBE EQCALC value = 144            (for the ;1;4 variant)
```

Both `ppqParse` entry points are shipped surfaces: `prettyTryEquation:818` (the
strip row) and `ppqShowRender:924` (EQSHW). A construct sets `c->fracSeen`, so
the pretty-worthiness gate does not decline it, and the tree is far inside the
size limits.

**What the owner sees.** `2 · ∏(x=1..3) x · 3` with the trailing factor flush
against the product's body at the body's own baseline. `prettyLayout.c:341-380`
places the body at `relX = colW + 3, relBase = 0` and ends the node's width at
the body's right edge, so the following `· 3` is baseline-continuous with the
body — pixel-identical to text inside it. That is the picture of
2·∏(3x) = 2·(3·6·9) = 324, for an equation `EQCALC` evaluates as 2·6·3 = 36.
(The measured pair is the `;1;4` variant: `EQCALC` returned 144 for a picture
that reads 2·(3·6·9·12) = 3,888.) With SUM the two readings coincide by
distributivity, which is exactly why EQ35's SUM-based pin passes and why `PROD`
is where the number moves.

**Contract violated.** `ppqScopeOperand`'s own banner, `prettyEquation.c:132-139`:

> a big operator used as an OPERAND needs brackets, and this parser is the THIRD
> producer of that shape … The body extends to the right of the stroke, so
> **anything beside it binds into the body**; the node KIND is enough to know.

The right-hand factor is beside it. The last clause is the defect: the KIND is
enough only while the operand is still the bare construct. The other two
producers of the class already cover both sides — `prettyFormula.c:152-167`
wraps `a` AND `b` for `ITM_MULT` via `ppfWrapIf`, and `prettyVisual.c:1155`
reports `PPF_PREC_ADD` for every construct — so the same mathematics draws
`2 · (∏ …) · 3` through the walker and the capture engine and draws it
unbracketed through the parser. That divergence is the exact condition PP18R3-3
exists to remove.

**Bug class.** *A class fixed by enumerating the sites where it was noticed
rather than the operand slot it lives in.* This is the commit's own moral —
"a class fixed at the site where it was noticed is a class fixed once" — one
grain finer: the wave enumerated three call sites and the slot is the fourth.
The walker's own PP18-4 comment confesses to the identical shape ("enumerated
the two OP1 members in front of it instead of the class; that is why the
construct member shipped").

**Class-level test.** Extend EQ35 into an operand-POSITION matrix rather than an
operator matrix: construct as the left factor, as the right factor, as a middle
factor with something on both sides, and the same three under `^`. The middle
cell is red at HEAD today with actual `[[2 · B(x|[x = 1]|3)] · 3]` against
expected `[[2 · P(B(x|[x = 1]|3))] · 3]`. Then the cheap oracle: the same text
through `setEquation` + `fnEqCalc`, which reds on the number alone.

---

### PP18R4-4 — PP18R3-4's one-constant fix is recorded as landed in the commit message and in `DESIGN-HISTORY.md`, and is not in the tree

`packages/pretty-print/prettyVisual.c:635`

**What breaks.** Nothing changed. `git diff d3aacbb46..HEAD --
packages/pretty-print/prettyVisual.c` is 90 lines over four hunks
(`ppvNameInSubtree`/`ppvInventName`, `ppvDerivVariable`'s `first` capture,
`ppvDerivative`, `ppvSumProd`) and does not touch `:635`;
`git show d3aacbb46:…prettyVisual.c` line 598 is character-identical to HEAD's
`:635`; and `packages/pretty-print/files/prettyVisual.c` — the copy the build
reads — is identical to the source. No pin and no MUT row was added for it
either.

```c
    uint8_t len = boundProgramNameLength(step + 4, *(step + 3));
    if(len == 0 || len >= PPV_NAME_MAX) {     // 16 — a buffer size
      break;
    }
```

against upstream's

```c
    if(nameLength == 0 || nameLength > MAX_LABEL_NAME_LENGTH) { break; }   // 14
```

(`src/c47/solver/differentiate.c:305-306`, `src/c47/defines.h:1188`). Both sides
call the same `boundProgramNameLength`, so the inputs are identical and only the
constant differs. Exactly one length — 15 — diverges, and 15 is drawable:
`ppvNameIsDrawable` rejects at `i >= PPV_NAME_MAX - 1`.

**Reaching input.** The one round 3 already executed: a `.p47` carrying
`LBL 'VB7' / MVAR 'abcdefghijklmno' (15 bytes) / MVAR 'x' / RCL x / ENTER / × /
END` plus `LBL 'VD7' / PGMDRV 'VB7' / 3 / f' 'x'`, loaded through
`fnLoadProgram`. It loads because `ITM_MVAR`'s row is `PTP_REGISTER`
(`items.c:3376`), so `_screenFileStep`'s `MAX_LABEL_NAME_LENGTH` test
(`saveRestorePrograms.c:141-152`) applies only to `PARAM_DECLARE_LABEL`, and
`programMemoryHasOverlongLabelName` (`manage.c:102-115`) tests `ITM_LBL` only.
Round 3 measured the result: the walker draws `DERIV(x×x;x;3)`, a picture worth
6, for a program `XEQ` returns 0 for.

**What the owner sees.** The wrong picture above. And a second cost, which is
why this ranks above the two findings below it rather than beside them: **the
record says it was repaired, so the next reader will not look.**
`d5b61ab8c`'s message — *"Also: the mirror's name-length bound, and two
namespace collisions this wave introduced …"* — and `DESIGN-HISTORY.md:45-51` —
*"Also fixed: … the mirror's name-length bound; and two namespace collisions the
wave introduced"* — both list it as landed. The other two items in that same
sentence (the `B9`→`B10` rename, the `PP18R2-*` retag) did land, which is what
makes the false one hard to spot.

**Contract violated.** `prettyVisual.c:599-600`, restated normatively at
`DESIGN.md:626-634`: *"This mirrors that walk, including REM transparency."* A
mirror with a different cut-off is not a mirror at the cut-off. And
`DESIGN-HISTORY.md`'s own standing rule, two sections earlier: *"A pin that
exists only in prose is worse than a missing one: it stops anyone looking for
the gap."* A **fix** that exists only in prose is the same shape.

**Verified absent of any ruling that would excuse it.** `grep` for
`MAX_LABEL_NAME_LENGTH` / `PPV_NAME_MAX` / "mirror" across `DESIGN.md`,
`DESIGN-HISTORY.md` and `TESTING.md`: the only hits are the mirror obligation
itself and audit narration about it. `PPV_NAME_MAX` is documented at
`prettyVisual.c:53` as a buffer size (`== the evaluator's varName[16]`), which
is precisely the complaint. Round 3's own §7 does not defer it — it lists it
among the three things worth an engineer's afternoon.

**One caveat on the prescribed repair, recorded because it makes the fix less
trivial than the record implies.** With `len > MAX_LABEL_NAME_LENGTH` the
package breaks the walk at the 15-byte declaration, leaving `first` empty, so
the caller invents a name and draws `DERIV(x×x;n;3)` — while upstream's
`deriv_pgm_variable` returns `INVALID_VARIABLE`, which yields 0. That reuses the
PP18R2-1 "none declared" arm for an "upstream errored" case, and may itself need
a decline. It does not make the current tree right.

**Bug class.** *A fix that exists only in the record.* The mechanical tell is a
commit-message "also fixed" clause with no diff hunk and no pin.

**Class-level test.** A mirror-bound matrix at 13, 14, 15 and 16 bytes, each
with a drawable second declaration and a parameter matching neither, asserting
which name the package returns against which register upstream resolves. The
process-level instrument is cheaper and covers PP18R4-10 too: every claimed fix
lands with a pin or a MUT row, which is the project's own standing bug-fix rule
(reproducer + named class + class-level test), currently unenforced for the
"also fixed" tail.

---

### PP18R4-5 — the mirror matches the derivative's variable with `strcmp` where upstream matches resolved REGISTERS under `CMP_NAME` folding, so a subscript `MVAR` spelling breaks it

`packages/pretty-print/prettyVisual.c:645`

**What breaks.** `ppvDerivVariable` decides "does this declaration match the
`f'` parameter?" with `strcmp(nm, param)`. Upstream decides it by *resolution*:
`deriv_pgm_variable` calls `findOrAllocateNamedVariable(name)` and compares the
resulting `calcRegister_t` against `currentSolverVariable`
(`differentiate.c:305-308`), and `findNamedVariable` compares fold-first —
`nameEqualsPrefolded` folds BOTH sides, and `_charCodeUnSupSubStruck` maps
`STD_SUB_a..STD_SUB_z` to `'a'..'z'` (`registers.c:926-932`, `sort.c:74-86`).
So a name spelled with subscript glyphs resolves to the same register as its
plain spelling, and upstream's match fires where the package's does not.

**Reaching input** (executed). Subscript letters are enterable: `ITM_DOWN_ARROW`
sets `NC_SUBSCRIPT` (`keyboard.c:517-523`), `convertItemToSubOrSup` maps
`ITM_a..ITM_z` → `ITM_SUB_a..ITM_SUB_z` (`bufferize.c:26-36`), and it is applied
on both the TAM alpha branch (`bufferize.c:459-463`, the `MVAR` name prompt) and
the PEM alpha branch (`manage.c:872-874`).

```
LBL 'VBY'  MVAR 'y'  MVAR '<sub-x>'  RCL 'x'  ENTER  ×  END
LBL 'VDY'  PGMDRV 'VBY'  3  f' 'x'  END
```

Measured, including a live in-binary register probe:

```
PROBE-REG: x=262 sub_x=262 y=268 equal=1
PROBE2 subscript MVAR, body recalls x   actual 'DERIV(x×x;y;3)'
```

**What the owner sees.** A picture that says *differentiate x² with respect to
y* — which is 0 — for a program the machine evaluates by perturbing `x`, which
is 6. No decline, no D-number. The window is specifically *a plain drawable
`MVAR` declared BEFORE the folded-equal one*: if the folded-equal declaration
were the only one, `first` would hold it and PP18R3-2's tail check would decline
honestly.

**Contract violated.** `prettyVisual.c:599-600` again — *"This mirrors that
walk"* — and upstream's rule it mirrors, `differentiate.c:281-283`: among
several `MVAR`s the caller's selection wins **whenever the program declares
it**, where "declares it" is decided by name resolution, not byte equality. It
is also the project's `upstream convention first` rule: the resolver upstream
uses is available and is not called. Corroboration that upstream expects such
spellings: its own step variable is `Δ`+`STD_SUB_d` (`differentiate.c:186`), and
`registers.c:926`'s comment says the second compare exists precisely because
users spell names this way.

**Bug class.** *A mirror that re-implements upstream's comparison instead of
calling it.* Beyond this input the hand-rolled compare drifts silently the next
time upstream edits `unSupSubRanges` / `unSupSubStruckTable`.

**Class-level test.** A fold matrix over `{plain, subscript, superscript,
struck}` spellings × `{parameter matches, parameter matches nothing}`, asserting
that the name the package draws belongs to the same register
`findNamedVariable` resolves the parameter to. The class is enumerable from
`sort.c`'s fold table.

**Provenance.** This is round 2's carried-forward P2, promoted from PLAUSIBLE to
CONFIRMED by constructing the entry path — exactly as round 3 promoted round 2's
P3.

---

### PP18R4-6 — the "limits are inside the operator's visual scope" ruling is enforced only on the INVENTED-name path; `INTEG`'s parameter name and `DERIV`'s `MVAR` name are never checked against their own limits

`packages/pretty-print/prettyVisual.c:500` (`ppvIntegral`'s only shadow test),
`:731` (the derivative twin)

**What breaks.** `ppvNameUsedInAst` (`:521`) and `ppvNameInSubtree` (`:550`) are
the two functions that carry the rule, and between them they have exactly one
caller: `ppvInventName` (`:582-585`). `ppvIntegral` takes its d-variable
straight from the `INTEGRAL_YX` parameter via `ppvVarName` and subjects it to
one test, `ppvNameInList(ctx->binding, …)` at `:500`, which consults only
ENCLOSING constructs and is empty at top level. `ppvDerivative`'s `MVAR`-supplied
name gets the same single test at `:731`; its point subtree reaches
`ppvNameInSubtree` only through the `sampled[0] == 0` branch at `:723`. The rule
is enforced where the walker *chooses* the name and never where the program
*gives* it.

**Reaching input** (executed; the simpler of the two is the one a reader found
while trying to refute the finding):

```
LBL 'VZF'  PGMINT 'VNB'  1  RCL n  INTEG 'n'  END      -> INTEG(n;n;1;n)
LBL 'VZI'  PGMINT 'VNB'  1  1 3 1 SIGMAn 'VNB'  INTEG 'n'  END
                                                       -> INTEG(n;n;1;SUM(n;n;1;3))
```

The first is character-for-character the PP18-9 picture — a FREE variable
sitting in the upper-limit slot of the operator that binds it — reproduced at
the producer PP18-9 never touched. The second is the shape V80
(`prettyTest.c:4685`) forbids one producer over: `ppvSumProd` renames its way
out to `SUM(m;m;1;SUM(n;n;1;3))` for the identical nesting, and `ppvIntegral`
cannot, because the name came from the program.

**What the owner sees.** An integration variable and a free variable, or an
integration variable and a sum counter, drawn with the same letter inside the
same operator's visual scope, with nothing on screen telling them apart. Ranked
below the wrong-number findings for the reason round 3 ranked PP18R3-5 last: the
inner binder shadows, so a defensible number can still be read out. What is gone
is the guarantee, which is what the scan exists to provide.

**Contract violated.** `prettyVisual.c:513-519`, the PP18-9 comment that both
R2-4 and PP18R3-5 deliberately left standing:

> a program whose loop count lives in a variable called `n` drew
> `SUM(n × n; n; 1; n)`, with the free variable sitting in the upper-limit slot
> of the operator that binds it. Nothing on screen told the two `n`s apart. The
> rule DESIGN.md states is about shadowing in **the drawn formula**; the limits
> are inside the operator's visual scope, so they are part of it.

and PP18R3-5's restatement at `:545-553`: *"The distinction is not 'already
built' but 'about to be drawn inside me', and the limit operands are exactly the
subtrees that will be."* Both are rules about the drawn formula; both are
implemented only where the name is chosen. `DESIGN.md:653-668`'s Rulings name
two shadowing cases — invented name vs a body recall (V6), and an inner
d-variable vs an OUTER one (V23) — and neither covers a program-supplied name
against its own limits. There is no ruling either way, and no comment claiming
the given-name path is out of scope.

**The structural cause, worth stating on its own.** In RPN the limit operands
are built BEFORE the operator that will enclose them. `ctx->binding` only ever
holds constructs whose body we are currently inside, so it is empty at the
moment the inner name is chosen, and the outer producer never looks back. That
is why the fix has to be a scan over the popped subtrees and not a binding-list
entry — and it is why `ppvSumProd` needed one.

**Bug class.** *A scope rule implemented where it was noticed rather than over
the scope it names* — `DESIGN-HISTORY.md:155-157` already names this class, for
PP18-9 itself.

**Class-level test.** A shadow matrix over **name provenance** — the axis the
guards are missing: `{invented, INTEG's parameter, DERIV's MVAR}` ×
`{name free in the limits, name bound by a construct in the limits, name in an
enclosing binder}`. Two of those nine cells are pinned today (V71 and V80, both
in the invented column).

---

### PP18R4-7 — the derivative's `f'` parameter is refused for undrawability, although PP18R3-2 ruled that only the name actually drawn may be judged

`packages/pretty-print/prettyVisual.c:696`

**What breaks.** `ppvDerivative`'s first act is `ppvVarName(ctx, pa, v)`, which
declines `PPV_D_NAME` (D18) unless `ppvNameIsDrawable(param)`. But `v` is used
at exactly one place afterwards — as the `strcmp` match key inside
`ppvDerivVariable` (`:716`) — and the name the picture spells is `sampled`. When
the body declares no `MVAR`, the parameter is never drawn at all.

**Reaching input.** `LBL 'FB': ENTER, ×` (no `MVAR` — the ordinary RPN shape
PP18R2-1 restored, pinned as V63b/VD6). `LBL 'FD': PGMDRV 'FB', 3, f' 'X1'`,
where `X1` is an ordinary named variable used only to hold the point. `XEQ 'FD'`
returns 6: `derivativeVariable` sets `currentSolverVariable = X1` and stores the
point there (`differentiate.c:187-188`), `deriv_pgm_variable` finds no `MVAR` and
returns `INVALID_VARIABLE`, and `calcDeriv` differentiates through the
unconditional `fnFillStack` channel — the name `X1` is never the variable of
differentiation and never appears in any picture. `VISUAL 'FD'` declines D18 at
`:696`, before anything else runs.

**What the owner sees.** *"step 3: cannot be drawn (D18)"* and no picture, for a
program whose picture is fully determined — the byte-identical program with the
parameter renamed to `x` draws `DERIV(n×n;n;3)`. The refusal is triggered by a
name the drawing never contains.

**Contract violated.** R2-3's own comment, still in the file at `:641-643`:

> drawability is a property of the name we END UP with, not of every declaration
> we walk past.

and `DESIGN.md` §6, which scopes the parameter as a matching key rather than a
drawn name: *"`DEI_xeq_user` writes into `regist`, and `_fnIntegrate` sets
`regist = labelOrVariable` — the integral's own parameter. INTEG's seeding by
parameter name is exact; DERIV's cannot be."* `ppvVarName` is shared by both
callers and applies INTEG's exact rule to DERIV. The catalog's own wording,
*"D18 name the grammar cannot spell"*, presumes a name the drawing must spell.

**Why this is a gap rather than a ruling I disagree with.** The intent search
comes back empty in both directions: nothing in `DESIGN.md`, `DESIGN-HISTORY.md`,
`TESTING.md`, the three audit reports or any comment requires the parameter to
be drawable. The one candidate ruling was explicitly scoped away by round 2 —
*"Round 1 did rule that `ppvNameIsDrawable` being digit-strict 'declines rather
than mis-draws' — that ruling is about names the walker* spells" — and round 3's
own PP18R3-D5 records the hole rather than closing it: *"what is missing is a
single stated rule for **where** in the mirror the package's extra question is
allowed to be asked. R2-3's comment states that rule; the code does not
implement it."* The gate at `:696` is a PP18-era artifact from when the drawn
variable WAS the parameter.

**Corroboration that the gate is misplaced.** The drawability test inside
`ppvDerivVariable`'s match arm (`:646`) is dead code precisely because of it —
see PP18R4-11.

**Confidence.** Medium, and the reason is recorded: no reader stood up a runtime
probe for `f' 'X1'`. The D18 outcome is derived from the call order and the enum
ordinal; what *was* measured is that removing the gate from the DERIV parameter
path leaves both gates green, so no pin encodes the refusal as intent.

**Bug class.** *A check left at the site where the drawn name used to be read,
after the drawn name moved.*

**Class-level test.** A parameter-spelling matrix: `{drawable, undrawable}` ×
`{parameter matches an MVAR, matches nothing, body declares no MVAR}`. Only the
cell where the parameter IS the drawn name may decline D18.

---

### PP18R4-8 — PP18R3-5 has two callers and only the sum one is pinned; MUT-134 is satisfied by V80 alone

`packages/pretty-print/prettyVisual.c:723`; `design-docs/pretty-print/TESTING.md:299`

**What breaks.** `TESTING.md:299` reads
`| MUT-134 | the limit subtrees not scanned when inventing (PP18R3-5) | V80 |`.
V80's fixture `VNS` is a sum whose upper limit is a sum, so it enters
`ppvInventName` only through `ppvSumProd:765`. The other caller, the
derivative's `ppvInventName(ctx, at, PPV_NIL, PPV_NIL)` at `:723`, has no
fixture that puts anything but a bare literal in the point: every derivative
program in the file (`prettyTest.c:4058-4195`) pushes `ITM_LITERAL 3`.

**Reaching input** (executed, both the program and the mutation):

```
LBL 'VZZ'  1  3  1  SIGMAn 'VNB'  PGMDRV 'VB5'  f' 'x'  END
```

— an eight-step program built entirely from fixtures already in the file. With
PP18R3-5 present it draws `DERIV(m×m;m;SUM(n;n;1;3))`. Substitute
`ppvInventName(ctx, PPV_NIL, PPV_NIL, PPV_NIL)` at `:723` and it draws
`DERIV(n×n;n;SUM(n;n;1;3))` — two different `n`s in one drawing, the exact
confusion PP18-9 and V71 exist to forbid — and **the gate is green either way**,
with exactly one failure line in the whole battery when the probe is present and
zero when it is not.

Nothing else catches it, and this was traced rather than assumed:
`ppvNameUsedInAst` cannot see the sum's counter (the construct's name lives in
`varOff`/`varLen`, which that function never reads, and the body's seeded read
carries `PPV_F_BOUND`), and `ppvNameInList(ctx->binding, …)` at `:730` is empty
at top level. Passing `at` into `ppvNameInSubtree` is the single thing that
rejects `n` here.

**Contract violated.** The fix's own comment at `:541-548` and the derivative's
at `:704-706` — *"an invented name must not collide with anything drawn inside
this operator, and the point is drawn inside it"* — against a coverage row that
records the fix as pinned when one of its two callers is.

**Bug class.** *A mutation row credited to a pin that exercises one of the
mutated site's callers.* The same shape as PP18R4-9 below; both are the audit
apparatus reporting more coverage than it has.

**Class-level test.** Promote the probe to a real pin —
`ppvTestExpect("V81 an invented derivative name avoids the point's construct",
"VZZ", "DERIV(m×m;m;SUM(n;n;1;3))")`. It reds the `:723` mutation and costs one
eight-step program. Generally: a MUT row names a *site*, so it must name a pin
per **caller** of that site, not per fix.

---

### PP18R4-9 — EQ35 pins two of `ppqScopeOperand`'s three call sites; the superscript-run site has no pin, and no production input reaches it

`packages/pretty-print/prettyEquation.c:621`

**What breaks.** Deleting the `ppqScopeOperand` call at `:621` leaves the
pretty-print gate GREEN. The positive control is the same deletion at `:599`,
which turns EQ35 red with its exact message — so the gate does propagate the
edit and the site is genuinely unpinned. EQ35 (`prettyTest.c:3305-3330`) parses
`SUM(X;X;1;3)^2` (site `:599`) and `SUM(X;X;1;3)×2` (site `:681`); the only pin
that reaches the trailing-superscript arm at all, EQ20, feeds it a NAME, never a
`PP_BIGOP`.

**The line is live code, not dead.** `ppqParse("SUM(X;X;1;3)" STD_SUP_2)` parses
and the wrap fires: `[P(B(x|[x = 1]|3)) ¡b]`, measured 48×40 px.

**Why it cannot misdraw today, checked and stronger than the finder's own
argument.** The two consumers are `prettyTryEquation`, which is fed the *display*
string where `^`+digits has become superscript glyphs but which refuses anything
taller than 23 px (`prettyEquation.c:826-828`; the measured tree is 40 px, and
`SUM(...)/2` is 41), and `ppqShowRender`, which is fed the *stored* text where
the power is `^` (site `:599`'s arm). A superscript glyph cannot enter stored
text: `ITM_SUP_2` is `CAT_NONE | EIM_DISABLED` (`items.c:2848`), which also
closes the catalog bypass at `keyboard.c:1226`, and decisively
`bufferize.c:439`'s `NumMsg[]` maps `ITM_SUP_0..ITM_SUP_9` to the literal ASCII
`"^0".."^9"`, so even a successful insertion writes `^` + digit. Latent.

**What it costs.** `TESTING.md:298`'s row
`| MUT-133 | the EQN parser leaves a construct operand unbracketed (PP18R3-3) | EQ35 |`
reads as though the parser fix is covered; a per-site mutant of the third call
survives, and no reader can learn from the pins whether that call is
load-bearing. If the strip band is ever widened, or superscript entry ever
enabled in EIM, the one site nothing checks becomes the one on the routine
display path.

**Contract violated.** The fix's own claim about the class
(`prettyEquation.c:133-139`) plus the standing rule this same commit wrote into
`TESTING.md`: *"a class fixed at the site where it was noticed is a class fixed
once."*

**Bug class.** As PP18R4-8: coverage credited per fix rather than per site.

**Class-level test.** One line added to EQ35 — the `STD_SUP_2` string, expected
`[P(B(x|[x = 1]|3)) \xa1\x62]` — which pins all three sites in one pin.

---

### PP18R4-10 — PP18R3-6's rename moved the duplicate pin name from `B9` to `B10`, which is itself already a live pin, and left the documentation pointing at `B9`

`packages/pretty-print/prettyTest.c:1748`

**What breaks.** At HEAD, `B10` names two unrelated pins in the *same* function
(`prettyTestCapture`, `:823`): `:1576`/`:1606`/`:1610`, R1-3's mid-loop
dispatch-failure pin, and `:1748`/`:1769`/`:1774`, the PP18R2-2 operand pin
renamed by this range (it read `B9` at `d3aacbb46:1748`). Both are
unconditional — unlike the collision the fix removed, where one of the two `B9`s
sat inside `#if defined(OPTION_INFSUMS)`. The fix relocated a partly-guarded
collision into a fully-unguarded one, and it picked the number round 3's own
evidence named as taken: *"`B10` and `B11` already exist (`:1606`, `:1634`), so
`B9` was not the next free number."*

**The documentation half is untouched, and demonstrably so.** `TESTING.md` at
HEAD contains four `B9` references and zero `B10`s. Two are correct (`:312`,
`:375` — the surviving `ITM_SIGMAnINF` pin) and two are stale: `:293`
(`| MUT-128 | ppfBigop reports ATOM again (PP18R2-2, the neighbour) | B9 |`) and
`:574`. The wave's own `TESTING.md` diff **edits that MUT-128 row** — `R2-2` →
`PP18R2-2` — and leaves the `B9` in the same row alone.

**Reaching input** (a reader's path, which is the failure mode). Apply MUT-128;
follow `TESTING.md:293` to the only prose definition of `B9` at `:375`; land on
the early-stop-sum pin, which passes untouched under MUT-128. Before the fix
that lookup was *ambiguous* between two code pins, one of them right. After it,
it resolves cleanly to the wrong one — strictly worse.

**What it costs.** A maintainer deleting "the duplicate `B10`" can delete the
wrong one and silently lose R1-3's only coverage, which `prettyTest.c:1600-1606`
explicitly calls *"the ONLY pin for R1-3's dispatch-depth pairing"*. And
`DESIGN-HISTORY.md`'s new round-3 entry asserts the wave fixed *"two namespace
collisions … a second pin named `B9`, and three audit tags reused from an
earlier round"*; the tag half landed and the `B9` half did not, so the amendment
trail is wrong about it — the same shape as PP18R4-4, in the same sentence.

**No machine check exists.** The only tooling under `packages/pretty-print/` is
`build-test.sh`, and nothing in `tools/` cross-references printed pin ids. The
lint round 3 proposed was not implemented.

**Bug class.** *A rename that resolves a collision into a different collision*,
in an identifier space with no uniqueness check (PP18R3-D6, unchanged).

**Class-level test.** The lint round 3 already specified: over the first token
of every `ppTestFail` / `ppTestFailInt` / `ppcTestExpectSig` / `ppfTestExpect` /
`ppvTestExpect` string, fail the gate on duplicates — and, one step further,
cross-check that token set against `TESTING.md`'s pin table, which is what would
have caught the stale rows.

---

### PP18R4-11 — the drawability conjunct in `ppvDerivVariable`'s parameter-match arm cannot be falsified

`packages/pretty-print/prettyVisual.c:646`

**What breaks.** Nothing, today. The guard fires only when
`strcmp(nm, param) == 0` (`:645`) and `!ppvNameIsDrawable(nm)`, and the two are
mutually exclusive by a closed static argument: `ppvDerivVariable` has exactly
one caller (`:716`), which passes `v`, written only by `ppvVarName` (`:696`),
which returns true only after `ppvNameIsDrawable` passes and then copies that
exact string out. `ppvNameIsDrawable` is a pure function of the bytes up to the
NUL, so identical bytes give an identical verdict. Nothing between `:696` and
`:716` rewrites `v`, and `v`/`sampled` are distinct buffers, so `out` cannot
alias `param`. The length edge is closed too: `strcmp` equality forces equal
length, so the two functions' differing caps cannot diverge.

**Mutations** (both green, both informative). Forcing a decline on
`!ppvNameIsDrawable(param)` at the top of `ppvDerivVariable`: green — no test
ever hands it an undrawable parameter, which is the same fact PP18R4-7 rests on.
Replacing the conjunct with `if(1)`, so every parameter match declines: **also
green** — so no fixture in the battery ever satisfies `strcmp(nm, param) == 0`
at all, and the match-wins-over-first rule at `:650` (`strcpy(out, nm)`) is
itself unpinned. That second result is an adjacent coverage gap and is the
reason this finding is worth a line rather than a footnote.

**What it costs.** Nothing to the owner. The cost is to the next reader and the
next fix: the arm looks like the live half of a symmetric pair with the
end-of-scan check at `:664`, so an edit that relaxes `ppvVarName`'s alphabet
would be reasoned about as already covered here, when only `:664` does any work.

**Contract violated.** The surrounding PP18R3-2 comment at `:653-659` —
*"Drawability is judged **once**, below, against the name we actually end up
with"* — which this earlier copy contradicts without being able to act on it.

**Not the MUT-118/119 shape.** Those two guards are each individually reachable
and each heals the other's mutation, which is why the project rules them
redundancy rather than coverage holes. This conjunct is refuted by its caller's
precondition and can never execute.

**Bug class.** *An unsatisfiable conjunct left behind by a moved check* — the
residue of PP18R4-7's misplaced gate.

**Class-level test.** None for the dead conjunct. The gap worth pinning is the
live rule it obscures: a body declaring `MVAR 'y'` then `MVAR 'x'` with
parameter `'x'` must draw `d/dx`, which is the match-beats-first rule and has no
fixture.

---

## 4. PLAUSIBLE

Survived refutation; nobody constructed the reaching input.

**P1 — the same import channel reaches an 8-to-14-glyph `MVAR`, where the
divergence survives even the fix PP18R4-4 prescribes.** Upstream records `first`
only when `findOrAllocateNamedVariable` succeeds, and
`allocateNamedVariableOnMiss` (`registers.c:962-965`) refuses any name outside
1–7 glyphs. So an 8-to-14-byte first declaration leaves upstream's `first` at
`INVALID_VARIABLE` while the package records the name and draws it — and
`len > MAX_LABEL_NAME_LENGTH` does not close it, because 8–14 is inside 14. One
reader cleared this as unreachable on the grounds that TAM alpha entry force-
closes at 7 glyphs; the same reader's own PP18R4-4 evidence shows the *import*
path bypasses the entry ceiling entirely, and nobody executed the 8-glyph case.
*What would settle it:* round 3's `.p47` fixture with `MVAR 'abcdefgh'` as the
first declaration and a short second one, loaded through `fnLoadProgram`,
compared against `XEQ`. It is the cheapest open experiment in this report.

**P2 — `PP_MAX_DEPTH` is 12 and every bracketed construct now adds a
`PP_PAREN` level.** PP18-4 and PP18R3-3 both add one. No reader could construct
a program that stays inside `PPV_MAX_DEPTH` 5 (which caps construct nesting at
four levels below the top walk) and still exceeds twelve layout levels, because
a nested construct BODY is deliberately passed at `PPF_PREC_ATOM`
(`prettyVisual.c:1123-1126`) and so takes no paren — which is what keeps
`INTEG(INTEG(…))` flat. The failure mode is benign in any case: `ppMeasure`
returns false, both font rungs and both surfaces are tried, and the owner gets
*"too large to draw (D19)"* with X's answer intact. Reported as unreached rather
than asserted, per the reachability rule. *What would settle it:* a mechanical
maximiser over `PPV_MAX_DEPTH`-legal AST shapes reporting the maximum layout
depth reached — a dozen lines in the test build.

**Carried forward, not re-examined this round and still open:**

- **Round 2's P1** — `calcDeriv` demotes a non-numeric first `MVAR` to
  `INVALID_VARIABLE` (`differentiate.c:434`, `getRegisterAsRealQuiet`) and the
  mirror does not, so a first declaration currently holding a string or a matrix
  makes the picture name a variable upstream did not vary. Value-dependent and
  statically unknowable; a freshly allocated named variable is a real34 zero, so
  the common case matches. *Settles only with a runtime probe that puts a string
  in the `MVAR` before `VISUAL`.*
- **Round 3's P1 and P2** — V65 never clobbers X between its two measurements,
  and assigns `lastErrorCode = ERROR_NONE` *after* `setEquation`. Both unchanged
  in this range; `oracle[]` at `prettyTest.c:4611` is still the same five names.
  Each closes for one line, as round 3 said.
- **Round 3's P3** — `ppvAstPrec` dereferences `ctx->ast[n]` without the
  `PPV_NIL` guard its sibling has. Unchanged in this range and the reachability
  argument still holds; `PC_BUILD`-only.

---

## 5. Design observations (D7)

Shape, not defects. These are the round's answer to axis (a) stated as structure
rather than as findings.

**PP18R4-D1 — the walker has three traversals of `ctx->ast` and nothing says
which of them is structural.** `ppvNameUsedInAst` is a flat scan over
`astUsed` and cannot blow up. `ppvAstToNodes` is a structural descent and
carries `layoutFull` + `layoutVisits` because PP18-3 made it. `ppvNameInSubtree`
is a structural descent with neither. (`ppvSerialize`/`ppvOperand` is a fourth,
`PC_BUILD`-only, also unlatched.) Nothing in the naming, the signatures or the
types distinguishes the flat one from the structural ones, and the DAG is a
property of the *arena*, not of any function. The natural home for the bound is
one guarded descent helper that every structural walk goes through; the natural
pin is one visit-count assertion per traversal.

**PP18R4-D2 — `const` made the latch impossible to write.**
`ppvNameInSubtree` takes `const ppvCtx_t *`, so it structurally cannot increment
`ctx->layoutVisits`. Constness was chosen as a purity signal for a predicate,
and it removed the only mechanism this file has for bounding a walk. Worth
naming because the obvious repair — a counter — requires dropping `const`, which
will read as a regression to anyone who does not know why. The alternative
(pass a `uint32_t *budget`) keeps the purity signal and is the shape the fix
probably wants.

**PP18R4-D3 — the EQN parser answers a question about the node it happens to be
holding, where the other two producers answer a question about the operand.**
`ppfCombine2` and `ppvAstToNodes` both carry a precedence number and wrap by
comparison, so they are correct for operands they have never seen.
`ppqScopeOperand` tests `nd->kind`, which is a fact about *this* node and goes
stale the moment `ppqTerm` rebinds `n` to its own `PP_HBOX`. PP18R4-3 is one
instance; the general statement is that a KIND test cannot express "this operand
will be drawn beside something". Round 2's D7-1 and round 3's PP18R3-D3 both
asked for a precedence channel in `ppqParse`; this round is the second
concrete instance of the cost of not having one.

**PP18R4-D4 — name PROVENANCE is the axis the shadow rules are missing.** A
construct's variable arrives one of three ways — invented by the walker,
supplied as `INTEG`'s parameter, or read from the body's `MVAR` — and each gets
a different, ad-hoc set of questions: the invented one is checked against the
binding list, the arena and (since PP18R3-5) the limit subtrees; the other two
are checked against the binding list only. Findings PP18R4-6, PP18R4-7 and
PP18R4-11 are all cells of that table, and there is no table. Two of the nine
cells are pinned.

**PP18R4-D5 — the record makes claims the diff does not contain, and nothing
checks.** Two of round 3's seven findings were closed in prose only: PP18R3-4's
constant (PP18R4-4) and half of PP18R3-6's rename (PP18R4-10). Both live in the
same "Also:" sentence of the same commit message, alongside two claims that are
true. The project already has the rule that would have caught them — every fix
lands with a reproducer, a named bug class and a class-level test — and it is
enforced for the numbered findings and unenforced for the tail. The cheapest
instrument is a MUT row or a pin per claimed fix, which makes the claim
falsifiable by the gate rather than by a reader.

**PP18R4-D6 — `oracle[]` is a hand-listed array of five names in a file with
about thirty loadable VISUAL fixtures.** Round 3's verdict that the *battery*
rather than any pin is vacuous is accepted and not re-argued; what this round
adds is that both of the findings V65 did catch (PP18R4-2, and the derivative
point in PP18R4-8) were caught by adding **one program**, and the differential
oracle needs no expected string to do it. Inverting the default — every fixture
that runs to completion goes through the oracle, with a named opt-out list for
the ones that legitimately decline — converts the battery's coverage from
"whatever five programs someone thought of" into "everything the file can
already load". The opt-out list is then itself a reviewable artifact, which the
current five-name array is not.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. Mandatory section, and this one is long because the wave's
central moves are sound and saying so precisely is most of this round's output.

### Killed by the refutation pass

**"The free-variable arm of `ppvNameInSubtree` (`:556-559`) is unfalsifiable,
because `ppvNameUsedInAst` has already returned true for every case it can
catch."** The mechanical half is TRUE and was proved by mutation: `astUsed` is
monotone within a walk, the subtree arm's predicate is the arena scan's
predicate restricted to one subtree, the `||` chain evaluates the arena scan
first, and deleting the arm leaves the gate green. Refuted anyway on intent,
because the project has a thrice-applied standing ruling that a guard subsumed
by an adjacent one is **kept** as belt-and-braces with its pin-of-record
elsewhere: `TESTING.md:313-319` (MUT-4's `dtReal34` type gate, *"the type gate
stays in the code as belt-and-braces, but its pin-of-record is the F-identity
contract via MUT-6"*), `DESIGN-HISTORY.md:160-163` (the two hang guards —
carried into this round's pre-verified list as MUT-118/119), and
`DESIGN-HISTORY.md:1408-1414` (R5-2, *"kept as defence, recorded as unverified
code"*). PP18R3-5's substance is the CONSTRUCT arm, which is live and pinned by
V80/MUT-134; the free-variable rule the arm duplicates has its own pin of record
in V6/V71 via MUT-125; and no artifact claims the disjunct is separately
reachable. **Residual, not a finding:** no `TESTING.md` row records this arm as
masked-and-unverified the way the MUT-4 lesson records the type gate. One
appended line discharges it.

**"`ppvNameIsDrawable` enforces an alphabet `DESIGN.md` says stopped binding at
PP18, and PP18R3-2 gave it a new whole-picture refusal."** Refuted, and the
reason is worth recording because it is a reading failure a future auditor will
repeat. The finding quotes `DESIGN.md:586-591`'s *"RETIRED at PP18 — the emitted
alphabet … It no longer binds the drawing path, because there is no longer any
text in it: the walker builds nodes, and a node cannot be mis-spelled"* — and
stops one sentence early. The paragraph continues: *"The rule still governs the
**test** back end (whose output one pin feeds to `fnEqCalc`) and the equation
parser itself."* `ppvNameIsDrawable` is not in the visual back end; it is in the
shared walk (`prettyVisual.c:57`, *"the walk produces one of these trees; the
back ends consume it"*), and both back ends emit the SAME pool bytes —
`ppNewRun` at `:1053` for pixels, `ppvOutRaw` at `:1290` for the text V44/V65
hand to `setEquation` + `fnEqCalc`. The alphabet is a live constraint on exactly
the node the finding says nothing constrains. Two corroborations: the PP18 doc
commit `f044f875e` added the RETIRED paragraph and left *"D18 name the grammar
cannot spell"* untouched in the live catalog 37 lines below it, so the
retirement was authored knowing D18 survives it; and the finding's subsidiary
claim that the predicate is stricter than `ppqName` is false on the very fixture
it names — `PPQ_IS_SUB` is `0xa080..0xa089`, the subscript GLYPHS, so `ppqName`
rejects ASCII `"x1"` too. **One true residue:** the comment at
`prettyVisual.c:288-292` still argues from a mechanism PP18 deleted. That is a
comment-accuracy nit against a non-authoritative source, not the design flaw
claimed. The refusal half of the finding survives separately and honestly, as
PP18R4-7, on a different argument.

### Cleared by the finders — the rulings that DO compose

This is the positive half of axis (a), and it is the larger half.

- **R2-4 ("a closed sibling's counter is reusable") against PP18R3-5 ("a
  construct in my limits is not a sibling").** The candidate gap was constructed
  and is not a defect: a construct in the outer's LIMITS binding `n` while a
  second construct inside the outer's BODY also invents `n`. The two are disjoint
  bound scopes, each under its own operator, which is exactly what R2-4 rules
  reusable; PP18R3-5's case is bad only because the shared name lands in the
  limit of the operator that binds it. The pair composes.
- **PP18R3-1's `synthetic` flag against the invention rule.** `invented` is
  precisely the case the `ITM_RCL` shadow guard exists for, and a name that came
  from the body's own `MVAR` is real and correctly does not arm it. No conflict
  with the `ppvNameInList` check that follows.
- **The reordered pops change decline PRIORITY, not outcome.** A program that
  both underflows and names an undrawable `MVAR` now reports D10 where it
  reported D4/D18. Cosmetic: both are honest refusals, nothing is painted either
  way, the D-number reaches the owner only through `moreInfoOnError`, no pin
  asserts an order, and the reorder is required by PP18R3-5 — the code says so
  at `:704-706`.
- **The invented-name shadow guard declines (D12) where a second candidate was
  free.** Ruled: `DESIGN.md:664-668`, *"a body that recalls a real variable
  spelled the same way DECLINES rather than let the invented name shadow it
  (V6)"*. Retrying with the next candidate is a design change, not a bug.
- **Five sums nested through each other's limits exhaust the four-name pool and
  decline D12.** Correct: five operators that genuinely nest visually need five
  distinct letters and there are four. Not the PP18R2-4 over-decline, which was
  about DISJOINT siblings.
- **`ppqScopeOperand` does not fire for `DERIV`.** `ppqBuildBigop`'s DERIV arm
  returns a `PP_HBOX`, not a `PP_BIGOP`, so the KIND test skips it — an obvious
  asymmetry with the walker, which reports `PPF_PREC_ADD` for `ITM_F1DRV` too.
  Correct rather than missed: that arm wraps the body in its own `PP_PAREN` and
  the `|var = at` subscript terminates it, so nothing beside it can bind in. The
  cost of the asymmetry is one bracket the walker draws and the parser does not.
- **`INTEG` as an unscoped right operand of `×`.** Same reason: the `" d<var>"`
  tail terminates the body, so `2·∫x dx·5` reads correctly. This is why
  PP18R4-3 is stated as SUM/PROD-specific rather than as "the right operand is
  never scoped".
- **A big operator bare as an operand of `+` or `−`.** Ruled, PP18-4: *"leaves
  it bare as the left operand of a `+`, which is the one place convention
  already scopes it"* — and all three producers agree there.
- **`SUM(…)/2` and `2/SUM(…)`.** The division arm calls no scoping and
  `ppqUnwrapParen` would strip a paren anyway; the vinculum scopes
  (`DESIGN.md:568`, V49). Checked at all four `ppqUnwrapParen` sites — exponent,
  numerator, denominator, radicand — none strips a bracket the new code adds,
  because `ppqScopeOperand` only ever wraps a base or a left operand.

### Cleared by the finders — axis (b), the new inputs to old code

- **A `PP_BIGOP` inside a `PP_PAREN` inside a `PP_SUP`.** `ppMeasure`'s
  `PP_PAREN` arm branches on child height and synthesizes tall strokes above
  `m->parInk + 2`; `ppPaint` recomputes the same test from the same inputs.
  Lays out correctly, and the shape is not new — `ppfWrapIf` has produced it
  since R2-2.
- **Deeper construct nesting.** A nested construct BODY is deliberately passed
  at `PPF_PREC_ATOM`, which is what keeps `INTEG(INTEG(…))` flat and bounds the
  added paren levels; the residual depth question is P2 above.
- **The DAG cannot produce a shared LAYOUT node.** `ppvAstToNodes` allocates
  fresh nodes on every visit, so the layout tree is a proper tree even when the
  AST is a DAG. `ppAppendChild`'s sibling chain cannot loop and `ppSetFontDeep`
  is bounded by the 72-node pool.
- **`ppvAstToNodes`' two returns that do not set `layoutFull`** (the `ppfWrapIf`
  failure at `:1128` and the `ppqBuildBigop` tail). Not the PP18-3 class: every
  OP1/OP2 parent latches on a `PP_NONE` child before recursing further, so the
  only unlatched escape is at the tree root, where the caller stops anyway. The
  CONSTRUCT arm also visits all four children before testing any of them, which
  is safe for the same reason. Asymmetric, not defective.
- **`ppMeasure`'s `PP_BIGOP` arity check against F1DRV's `PPV_NIL` `child[2]`.**
  F1DRV never reaches that arm (the parser's DERIV branch builds an HBOX), and
  the CONSTRUCT arm's `to == PP_NONE && a->item != ITM_F1DRV` test exempts it
  deliberately.
- **New `PP_PAREN` nodes consuming the 72-node layout pool.** Worst case a
  previously-fitting equation now declines and upstream's linear rendering runs
  — the documented graceful degradation. `ppqScopeOperand` setting `c->failed`
  when the pool is spent is the honest direction and matches every other
  allocation in the file.

### Cleared by the finders — state, arithmetic and the harness

- **`ppvBody` leaves `bindingCount` incremented on all five failure exits.**
  Every one of them has already set `ctx->failed`; `ppvWalk`, `ppvRun` and
  `ppvStep` all bail on it, and `ppvRun` re-zeroes the counter per walk. The ctx
  is a stack frame dropped whole.
- **`layoutFull` / `layoutVisits` across the two font rungs and two surfaces.**
  Both paint surfaces clear `layoutFull` per rung and `ppvRun` clears both per
  walk; `layoutVisits` is instrumentation and is never compared against
  anything.
- **`ppvLeaf`'s `(uint8_t)len` narrowing.** The one narrowing that looked like a
  truncation and is not: a program literal is length-prefixed by ONE byte
  (`decode.c`, `countLiteralBytes`), so the value is ≤255 and exactly
  representable. Also checked and correct: `ppvIntern`'s explicit `uint32_t`
  promotion, `ppNewRun`'s int promotion, `ppvLabelIndex`'s deliberate unsigned
  wrap used as a range guard (same idiom in `ppfVariableName`/`ppfLabelName`),
  all six `char[16]` write sites against `ppvNameIsDrawable`'s 15-char cap,
  `ppvPush`'s drop loop and every `depth` guard, `ppFillVal`'s edge trims,
  `ppDrawLine`'s four-sided test, and `ppDrawIntegralSign`'s divide guard.
- **`ppvSerialize`/`ppvOperand` expand the same DAG unlatched.** Real, and the
  same class as PP18R4-1 — but the whole text back end is
  `#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)` and no product surface
  calls `ppvTranspile`, so it cannot reach the device. Not reported as a
  separate finding; it is load-bearing inside PP18R4-1's class-level test,
  because it is why the pin must be a visit bound and not an `oracle[]` program.
- **`ppvInventName` scans `stepN` even when a unit step is not drawn.**
  Over-conservative only, and `unitStep` requires a literal `"1"`, which contains
  no names.
- **V77's `sprintf` → `snprintf(want, sizeof want)` with `want` shrunk 256→128.**
  Writes 64 bytes. `ppfTestExpect`'s `sig[192]` and `ppfTestSigNode`'s
  `len + 24 >= cap` truncate silently, but on the ACTUAL side of an exact
  `strcmp`, so a truncated dump fails loudly and cannot pass.
- **The bare integers `12` and `18` in the new decline pins** instead of
  `PPV_D_COLLISION`/`PPV_D_NAME`. The file's existing convention at all nineteen
  decline pins, not something this range introduced.
- **`PPV_D_TOOBIG` is D19 while `DESIGN.md`'s catalog stops at D18.** Two copies
  of one catalog with nothing forcing agreement — the D7 shape — but the
  sentinel is never passed to `ppvDecline`, the message is self-describing, and
  it predates the range. Round 3's PP18R3-D2 owns the unpinned-reason half.
- **The upstream `showString` re-indent.** Catalogued exception, predates the
  range, already owned with the fix idiom prescribed; §2, not a finding
  (`CODE_AUDIT.md` rule 6).

**Not re-reported, per the tasking's pre-verified list:** gate and warning state
solo and combined; the MUT-131..134 red-verifications; the DBLINT rows-20-91
comparison; MUT-118/119 surviving alone by design; V66 asserting a visit count
rather than a wall-clock time; `ppvDerivVariable`'s unreachable REM-transparency
arm; and round 3's verdict that no individual pin is vacuous but the battery is.

---

## 7. Verdict

**Would I ship this? No — and unlike round 3, the blocker is not a judgement
call.** One finding freezes the machine on a program built from an ordinary RPN
idiom, with no error, no D-number and nothing on screen to say what happened.
That is the worst outcome this scope note admits — *"the calculator reboots and
the owner loses the program they were typing"* — and it is reachable from the
keyboard in about ninety steps.

The wave's central moves are otherwise sound, and five readers could not break
them. PP18R3-1's flag is right and composes with the invention rule. PP18R3-2's
"record first, judge once" is right and its comment states the rule correctly.
PP18R3-3's KIND test is right at the three sites it guards. PP18R3-5's CONSTRUCT
arm is right and is the fix the round-3 finding asked for. The pop reorder is
required and harmless. Upstream discipline is unblemished for a second
consecutive round: no patch byte moved, and the churn count is identical to
round 3's.

But **the round's worst finding is again this wave's own new code**, and the
mechanism is one grain finer than round 3's diagnosis. Round 3 named "a fix that
relocates a decision rather than removing one". This round names its sibling:
**a fix that adds a new traversal over a structure whose existing traversal
carries a hard-won bound, and does not inherit it.** The tell is a fix that adds
a *function* rather than editing one — round 3's diff added exactly one new
function to `prettyVisual.c` and one to `prettyEquation.c`, and both are
findings here.

**Where would it break first?** On repeated squaring. `2 ENTER × ENTER × …` is
how this machine's owners square a running value, and a program that feeds one
to a `SUM` limit or a derivative's point does not come back. Second, on `STO Y`
in a program the owner then draws — the picture is built from operands the
program overwrote, and the right number is on the same screen. Third, on `EQSHW`
over a product with a construct in the middle: a rendering of the owner's own
equation that means 324 for a formula the machine evaluates as 36.

**The axis (a) answer in one line.** No two rulings conflict; three pairs leave
a shape neither covers, and all three are the same shape — a rule stated over
the drawn formula and implemented at the one producer where it was noticed.
PP18-9's scope rule (PP18R4-6), R2-3's judge-once rule (PP18R4-7), and PP18-3's
DAG latch (PP18R4-1), which is not composed with the round-3 fix at all. The
missing instrument is a table over name provenance and traversal kind.

**The axis (b) answer in one line.** Old code is fine with the walker's new
trees; the two places that could have gone wrong — `PP_PAREN`'s tall-stroke
synthesis and the `layoutFull` latch — already carry the reasoning. The DAG is
the one shape old code got right and new code got wrong.

**What I would leave alone if the goal were correct code rather than a clean
audit.** PP18R4-9, PP18R4-10 and PP18R4-11: a live-but-unreachable parser site
behind a 23-px guard, identifier hygiene, and a conjunct that cannot fire.
Nothing computes a wrong answer because of any of them, and one lint plus one
line in EQ35 closes two of the three. Most of P2, and the three carried-forward
plausibles, which are genuine divergences nobody can reach. And PP18R4-6 is a
legitimate ruling question rather than a bug: two `n`s in one drawing is
unreadable, not wrong, and the number is right — but the ruling should be
written down either way, because right now the code answers it differently
depending on where the name came from.

That leaves **PP18R4-1 before anything else**, then PP18R4-2, PP18R4-3,
PP18R4-4 and PP18R4-5 — five findings, four of them a wrong number the picture
asserts confidently — plus the two coverage rows (PP18R4-8, PP18R4-9) that
currently claim the wave is pinned when half of it is.

**Concrete V65 programs, since proposing them is in scope.** Round 3 asked for
four and `oracle[]` is still the same five names. The four that would have
red-ed this round: (a) `2 × PROD(X;X;1;3) × 3` driven through `ppqParse` +
`fnEqCalc`, which reds PP18R4-3 by a number (36 against 324) where the SUM form
cannot; (b) `2 / 3 / STO Y / −`, which reds PP18R4-2 by a number (0 against −1)
and costs four steps; (c) the round-3 `.p47` with the 15-byte `MVAR`, which reds
PP18R4-4 by a number (0 against 6) and incidentally reaches the REM arm
currently recorded as unpinnable; (d) `PGMDRV` over an `MVAR`-less body whose
point is a `SUM`, which reds PP18R4-8's mutation. PP18R4-1 wants the opposite of
an oracle entry — a visit-count assertion in V66's shape — because a doubling
program appended to `oracle[]` would hang the suite rather than fail it, and a
timing pin passes on a desktop for a program that hangs the calculator.

---

## 8. Round and exit state

**Round 4, in-family half**, over the round-3 fix commits. Eight finder
dimensions (contracts, lifecycle, arithmetic, error paths, guards, tests,
design, upstream) ran blind to each other; every raised finding went to an
independent refutation pass with one assigned lens (reachability, correctness,
intent), instructed to default to REFUTED and to prove coverage claims by
mutation.

**Counts.** Nineteen findings raised and survived refutation; **eleven** after
deduplication across dimensions. Two refuted. Five PLAUSIBLE, three of them
carried forward from rounds 2 and 3. None demoted to §2 this round; the one
mechanical item (`showString`) was recognised as pre-existing by the finder who
raised it.

**Independent agreement.** PP18R4-1 was reached by three dimensions (contracts,
lifecycle, error paths) whose three independent measurements agree to the unit
at k=20 (8,388,615 visits) and k=24 (134,217,735) — the strongest quantitative
convergence any round of this audit has produced. PP18R4-3 by four dimensions
(contracts, arithmetic, error paths, design). PP18R4-4 by three (contracts,
arithmetic, error paths). PP18R4-6 by two (contracts, guards). The remaining
seven were found by one dimension each, which is the argument for the fan-out:
PP18R4-2 came only from the arithmetic lens, PP18R4-5 only from upstream
discipline, and both are wrong numbers on a shipped surface.

**Evidence.** Fifteen of the nineteen raised findings are backed by an executed
probe or mutation, applied, observed and reverted inside an isolated worktree.
Two carry a number measured this round: PP18R4-2 through V65's own differential
oracle (`program=0.0 picture=-1.0`) and PP18R4-3 through `setEquation` +
`fnEqCalc` (144). PP18R4-4's number — 6 against 0 — is round 3's executed
measurement of the same `.p47`, re-cited rather than re-run. Two findings rest
on arguments a mutation cannot reach and say so: PP18R4-10 is decidable by
`grep` and `git diff` and needed no probe, and PP18R4-7 has no runtime probe of
its D18 outcome, its mutation showing only that no pin depends on the gate.
Every worktree finished clean; no foreign edits were encountered.

**Exit criterion: not met**, on two counts. Eleven CONFIRMED findings, four of
them in code or documents this wave wrote, so the count resets again and the
rule against closing on a round that contains fixes stands. And this is the
**fourth consecutive in-family round** — the criterion requires at least one
out-of-family pass, and `CODE_AUDIT.md`'s own reasoning is that fresh sessions
of one model share a training distribution and therefore the blind spots. The
out-of-family half of this round is still owed and is the highest-value
remaining pass.

**Round 5's axis, in priority order.** (1) **Out-of-family**, per the criterion,
and given depth rather than breadth: the subject that would pay is
`ppvNameInSubtree` + `ppvInventName` + `ppvAstToNodes` with the DAG explained
inline, sent to a reader who has never seen PP18-3's comment. (2) **The layout
pass**, `prettyLayout.c`'s measure and paint, which one reader has now read end
to end for the first time in four rounds but which no reader has audited from
the *design* lens, and on which every bracketing finding in four rounds
ultimately rests. (3) The mechanical form of the axis round 3 proposed and
neither round has run: **every `PPV_D_*` against its catalogue entry and every
`Rulings.` bullet against its implementation, in both directions** — three of
this round's eleven findings are code-and-document divergence and that check
still has never been executed.

**Process items, three repeats and one new.**

1. **Stale worktrees, third round running.** Every verifier again spawned at
   `e21af8d28`; round 2 asked for a `git merge-base --is-ancestor` guard before
   round 3, round 3 asked again, and it is still absent. Ten readers each spent
   a step detecting it.
2. **`packages/forth-core/build-test.sh` returns a meaningless green for a
   pretty-print mutation**, second round running, hit twice this round. It sets
   `CUSTOM_PKG=packages/forth-core`, so `prettyVisual.c` is never compiled. Both
   readers caught it only because the log contained no `prettyPrint test` lines.
   A one-line guard in the mutation checklist — *"the gate you run must be the
   package's own"* — would close it.
3. **The mutation-and-revert cycle must also revert `files/` and
   `.refresh-manifest.json`**, which the gate's refresh step regenerates from
   the mutated source. Every reader handled it this round; recorded again
   because a reader who did not would leave a mutated package behind a
   clean-looking source diff.
4. **New: concurrent gates.** Several verifier worktrees ran `build-test.sh`
   simultaneously (load average 44 at one point) and gate times spread from 169
   to 303 s. Nothing failed and no result was distorted, but a reader unaware of
   it can read a slow run as a hang and abandon a valid probe.
