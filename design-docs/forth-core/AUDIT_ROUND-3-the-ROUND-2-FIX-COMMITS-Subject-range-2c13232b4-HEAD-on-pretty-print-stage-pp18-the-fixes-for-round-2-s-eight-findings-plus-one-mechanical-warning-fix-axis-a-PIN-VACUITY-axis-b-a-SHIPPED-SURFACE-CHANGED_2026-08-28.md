# Audit round 3 — the round-2 fix commits, at `10e49e084`

*(Filename truncated from the full subject line, as round 2's was: the ext4
limit is 255 bytes. The rotated axis, the fix inventory and the pre-verified
exclusions are reproduced in §1 rather than in the name.)*

Subject: `2c13232b4..HEAD` on `pretty-print/stage-pp18` — two commits, the wave
that closed round 2's eight findings, plus one mechanical warning fix. Eight
finder dimensions ran blind to each other; every raised finding then went to an
independent refutation pass with one assigned lens (reachability, correctness,
intent), instructed to default to REFUTED.

**Seven CONFIRMED findings, three PLAUSIBLE (two carried forward), four
REFUTED, one demoted to the mechanical section.** Eleven findings were raised
and survived refutation; deduplicated across dimensions they are seven. Every
CONFIRMED finding in this report is backed by a probe or a mutation *applied,
observed and reverted* inside an isolated worktree: seven distinct programs
were built, loaded and walked, and four of the seven findings were measured as
a number the picture asserts against a different number `XEQ` returns.

**The round asked two questions.**

**(a) Pin vacuity — which pins would still pass if the behaviour they claim to
test were deleted?** None of the five new pins is vacuous. V65, V75, V76, V77
and B9 each have a concrete deletion that reds them, and readers verified
V75/V76 and V77 by hand rather than inheriting the mutation table. What the
axis found instead is one level up: **a pin can be non-vacuous and still be
misfiled.** The new capture pin was numbered `B9`, which is a live pin already;
three audit tags were reused where they already name other findings, one of
them twice in the same file; and V65 — the oracle this project spent a whole
finding on — has a fixture set with a hole exactly at the two regressions this
wave introduced. Adding four programs to `oracle[]` reds both, with no expected
string. That is the reusable result: the battery's *names* are unchecked, and
its coverage is thin at the precise shape the previous round's fixes touched.

**(b) `ppfBigop` reports `PPF_PREC_ADD` — is the new drawing right?** Yes, and
right for every shape the function can build, not just the pinned sum. Five
readers independently walked all eleven consuming arms of `ppfCombine1`/`2`;
`PPF_PREC_ADD` can only *add* a bracket, never remove one, the four
structurally-scoping arms (`FRAC`, `RAD`, `BARS`, `1/x`) ignore precedence, and
the integral shape gains a redundant-but-harmless pair that makes the capture
engine agree with the walker. The change reaches PHIST identically because
`ppfBuildEntry` stores the returned precedence rather than forcing ATOM. It
also reaches a third shipped surface the record does not name — the PP8 T-line
(`prettyValue.c:803`, `FLAG_PTLINE`) — where the extra node costs width in a
band that is already tight. And the class has a **third producer** the wave did
not fix: the EQN parser, where a typed `SUM(X;X;1;3)^2` is drawn as the picture
of 14 and evaluated as 36.

**Five of the seven findings are in code or documents this wave wrote.** The
two worst are both on the derivative path, both silent wrong pictures, and both
are the class the wave existed to fix. That is the fourth consecutive round in
which the round's worst finding came from the previous round's fix.

Nothing was fixed. The tree this report finishes on is the tree it started on;
every probe was reverted in the worktree that made it and the gate is green at
`10e49e084`.

---

## 1. Subject and coverage

**Tip.** `10e49e084` on `pretty-print/stage-pp18` ("pkg: format-overflow warning
in V77's expectation (mechanical half)"). Range `2c13232b4..HEAD`:

| commit | what it did |
|---|---|
| `d3aacbb46` | the round-2 wave — R2-1 through R2-8, PP18-8, V65/V75/V76/V77/B9, `PPV_D_DERIVVAR` deleted |
| `10e49e084` | `snprintf` into a right-sized buffer in V77's expectation (mechanical half) |

**Diff.** 11 files, +1,805 / −133. The code write set is three files:
`prettyVisual.c` (+88/−53), `prettyTest.c` (+181/−3), `prettyFormula.c`
(+8/−1), plus their generated `files/` twins and the refresh manifest. The rest
is documentation: `DESIGN.md` (+29/−…, one section), `TESTING.md` (+36),
`DESIGN-HISTORY.md` (+55) and round 2's own report checked in (1,144 lines).
**No `patches/` file changed**, so the wave adds no upstream override and
nothing new to conflict on a rebase.

**The fixes under review** (as characterised in the tasking, verified against
the diff): R2-1 — `ppvDerivVariable` returns `""` instead of declining when the
body declares no `MVAR`, and `ppvDerivative` calls the new shared
`ppvInventName`, because `_differentiatorIteration`'s `fnFillStack` is
unconditional and only the `STO` is guarded. R2-2 — `ppfBigop` reports
`PPF_PREC_ADD`. R2-3 — the `MVAR` scan checks drawability only for the name it
ends up using. R2-4 — `ppvNameUsedInAst` ignores `PPV_F_BOUND` VAR nodes and no
longer scans `CONSTRUCT` nodes. PP18-8 — `ppvBody` allocates one shared seed
node instead of eight. `PPV_D_DERIVVAR` deleted as dead. New pins V65 (the
differential oracle over five programs), V75, V76, V77, B9.

**Rotated axis.** Round 1 asked whether the refactor was faithful. Round 2 asked
whether fixing disturbed the neighbours and whether the new refusals are honest.
Round 3 asked (a) **pin vacuity** — for any pin in doubt, what would you delete
to make it fail, and would it? — and (b) whether the **shipped surface change**
in `ppfBigop` is right for every shape it can build, not just the pinned
sum-times-two.

**Numbering.** The PP1–PP16 report took `A1`–`A14`; PP18 round 1 took
`PP18-1`–`PP18-16` and `D18-1`–`D18-7`; an out-of-family pass took `R1-1`–`R1-3`;
round 2 took `R2-1`–`R2-8`. `AUDIT R1`–`AUDIT R5` and `AUDIT PP18` are *all*
already in the package's source as tag prefixes (21, 20, 12, 7, 5 and 32
occurrences respectively), which is finding PP18R3-7. This round's findings are
therefore **`PP18R3-1`–`PP18R3-7`** and its design observations
**`PP18R3-D1`–`PP18R3-D6`**, so a grep is unambiguous. `grep -rn PP18R3` returns
nothing outside this report and nothing from this round is renumbered from any
earlier series.

**Read at line level** (union across the eight dimensions): the two-commit diff
by all eight; `prettyVisual.c` in full by six, and the pre-wave `2c13232b4`
version of the changed functions by four — the before/after read is what
established that PP18R3-2 is a regression rather than a pre-existing limit.
`prettyFormula.c`'s `ppfWrapIf` / `ppfCombine1` / `ppfCombine2` / `ppfBigop` /
`ppfFromCaptureNode` / `ppfBuildEntry`'s token machine, by five, arm by arm.
`prettyEquation.c`'s `ppqScopeBody` / `ppqBuildBigop` / `ppqBigopConstruct` /
`ppqPrimary` / `ppqFactor` / `ppqTerm` / `ppqExpr` / `prettyTryEquation` /
`ppqShowRender`, by four. `prettyLayout.c`'s `PP_BIGOP` and `PP_SUP` measure
arms (:341-425) by three — that trace is what turns "no bracket" into "the
exponent lands at ordinary superscript height beside the body run", which is
the difference between an untidy picture and a wrong one. `prettyValue.c`'s
`ppfBuildCurrent` call sites. `prettyTest.c`: the new and neighbouring pins
(B1–B11 incl. both `B9`s, V4–V6, V17, V18, V44, V54, V55, V63/V63b, V65,
V68–V77, EQ22/EQ25/EQ29/EQ33) and the harnesses `ppvTestExpect`,
`ppvTestDecline`, `ppfTestExpect`, `ppfTestSigNode`, `ppcTestExpectSig`,
`ppcTestWriteAndLoadPgm`.

**Upstream read by execution path:** `src/c47/solver/differentiate.c`
(`deriv_pgm_variable` :286-322, `_differentiatorIteration` :328-357, `calcDeriv`
:430-445); `src/c47/registers.c` (`findOrAllocateNamedVariable` :986,
`allocateNamedVariableOnMiss` :963); `src/c47/saveRestorePrograms.c`
(`_screenFileStep` :141-152, the load-time length screen);
`src/c47/programming/manage.c` (`programMemoryHasOverlongLabelName` :102-115);
`src/c47/programming/decode.c` (`getStringLabelOrVariableName`'s own clamp);
`src/c47/items.c` (the `ITM_MVAR` row, :3376); `src/c47/solver/equation.c`
(`setEquation`, `fnEqCalc`, `ppEqBigopIntercept` :1371-1382);
`src/c47/programming/lblGtoXeq.c` (`fnExecute`, `runProgram`); `src/c47/defines.h`
(`MAX_LABEL_NAME_LENGTH` :1188, `MAX_MVAR_DECLARATIONS`, `OPTION_INFSUMS` :61
and its `#undef` at :284).

**Docs read:** `DESIGN.md`'s VISUAL/PP17-PP18 section and the Rulings block in
full by six; `DESIGN-HISTORY.md`'s 2026-08-28 entries by all eight;
`TESTING.md`'s MUT table and V/B families by six; round 2's report in full by
four, including its §6, so that nothing it already killed was re-raised
unknowingly; round 1's report at the PP18-1/-4/-8/-9 findings;
`REVIEW_upstream-minimality_2026-08-27.md`; the `upstream-diff-review` skill's
`SKILL.md` and all 13 entries of `references/deliberate-exceptions.md`.

**Not reached, and it matters where.** `prettyLayout.c`'s measure and paint
internals beyond `PP_BIGOP`/`PP_SUP`/`PP_PAREN` — **no round has read the
layout pass end to end**, and it is the one component every finding about
bracketing ultimately rests on. `prettyCapture.c`'s staging state machine
outside `ppcClassify` and the `PPN_BIGOP` mint. `prettyValue.c` beyond its
`ppfBuildCurrent` and `ppReset` call sites. The browsers, the softmenu stack
and `FLAG_ALPHA` (untouched by the range). `parseEquation`'s XEQ-mode internals
past the point where it sets `lastErrorCode`, which is why PLAUSIBLE P3 below
stays plausible. **No reader ran the simulator**, so no finding here is backed
by a photograph of an LCD: the drawing evidence is transpiled strings, node
signatures from `ppfTestSigNode`, and — for four findings — the *number the
drawing evaluates to*, taken through `setEquation` + `fnEqCalc` in the real
build.

**Process fact, recurring.** Every verifier worktree again spawned at
`e21af8d28`, a forth-core README commit on an unrelated branch, with
`git log 2c13232b4..HEAD` empty. Round 2's report asked for a
`git merge-base --is-ancestor` check before round 3; it was not added, and all
ten verifiers had to detect the stale ref and `git checkout 10e49e084` by hand.
Every verdict in this report states the ref it worked at. One of them also
re-confirmed round 2's other trap by running into it: `packages/forth-core/build-test.sh`
does not compile `prettyVisual.c`, and returned a meaningless green for a
mutation of it.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh` is green at `10e49e084`, solo
and combined (pre-verified in the tasking; re-run to completion by six
verifiers after reverting their probes — recorded tails `PRETTY-PRINT GATE
GREEN`, exit 0, testSuite OK in 207–221 s). Compiler warnings clean. Not
re-reported as a discovery.

**Mutation set.** MUT-127..MUT-130 red-verified in the tasking. MUT-128 was
re-applied independently during verification and produced **exactly one**
failure line in the whole battery — the new `B9` — while the old `B9` compiled,
ran and passed; that single-failure result is the evidence for PP18R3-6.
MUT-118/MUT-119 surviving alone and V66's visit-count assertion are
pre-verified design and were not re-litigated.

**Probes and mutations this round ran**, all applied, observed and reverted
inside isolated worktrees, none numbered (the catalog is the owner's to
extend):

| probe | result |
|---|---|
| `VD7` = `PGMDRV 'VBN'` + `f' 'x'`, added beside the R2-1 fixtures | draws `DERIV(n;n;3)`; through V65: program `0`, picture `1` → **PP18R3-1** |
| `VB6`/`VD8` = `MVAR 'X1'` then `MVAR 'Y'`, param `'Z'` | draws `DERIV(Y×Y;Y;3)`; program `0`, picture `6` → **PP18R3-2** |
| `SUM(X;X;1;3)^2` and `PROD(X;X;1;3)×2` through `ppqParse` + `fnEqCalc` | sigs `S(B(x\|[x = 1]\|3)\|2)` / `[B(x\|[x = 1]\|3) . 2]`, no `P(...)`; EQCALC 36 and 12 → **PP18R3-3** |
| `MVAR "abcdefghijklmno"` (15 bytes) then `MVAR "x"`, written as a real `.p47` and loaded through `fnLoadProgram` | loads; draws `DERIV(x×x;x;3)` where upstream computes 0 → **PP18R3-4** |
| same, with `len > MAX_LABEL_NAME_LENGTH` substituted at `prettyVisual.c:598` | draws `DERIV(x×x;n;3)`; **no other assertion in the battery changes** |
| `VNS` = a sum whose upper limit is a closed sum | draws `SUM(n;n;1;SUM(n;n;1;3))` → **PP18R3-5** |
| same, with R2-4's `ppvNameUsedInAst` reverted | draws `SUM(m;m;1;SUM(n;n;1;3))` **and reds V77** — the trade is real and unpinned in one direction |
| an enumerator inserted at position 5 of the decline enum | **10 pins red** with "expected 12, actual 13" — the 17 integer literals are a real tripwire |
| an enumerator inserted at position 13 of the decline enum | **gate green** — D13..D18 are documented and asserted by no pin (see PP18R3-D2) |

**Upstream churn.** `patch_churn_scan.py` over all 13 patches at HEAD: 790 added
/ 23 deleted upstream lines across 39 hunks, **1 mechanical churn finding** —
the `[WS-ONLY]` wrap-reindent of `showString` in
`010-solver__equation.c.patch:46` (shadow source `solver/equation.c:692`), where
the PP5 hook re-indents an upstream line by two spaces instead of using the
package family's no-reindent wrap. This was raised as a finding and **demoted
here** under `CODE_AUDIT.md`'s rule 6: it is the mechanical half's output, it
predates the subject range (`git log -S` → `888f46343`, PP5), the patches are
byte-unchanged across `2c13232b4..HEAD`, and
`REVIEW_upstream-minimality_2026-08-27.md` already enumerated it one day
earlier, distinguished it from the catalogued softmenus exception, prescribed
the fix idiom and recorded a standing count of 1. **It remains open**; it is not
a round-3 discovery.

**`design-audit.sh`** is forth-core's; there is still no pretty-print
equivalent, so no override-budget check ran. The substitute check —
`git diff --name-only 2c13232b4..HEAD -- packages/pretty-print/patches/` —
returns nothing.

**Generated output in sync.** All 13 `files/` entries hash-match the flat
working area and the `.refresh-manifest.json`, three ways.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner.

---

### PP18R3-1 — R2-1's invented derivative name is registered as REAL, so the shadow guard that makes invention safe never arms for a derivative

`packages/pretty-print/prettyVisual.c:684`

**What breaks.** `ppvDerivative` invents a counter when the body declares no
`MVAR`, then hands it to `ppvBody` with `synthetic = false`:

```c
if(!ppvBody(ctx, bodyIdx, sampled, false, &body)) {
```

`ppvBody:417` records `ctx->bindingSynth[n] = false`, and the collision guard in
the `ITM_RCL` arm (`:845-850`) reads that flag first:

```c
if(ctx->bindingSynth[i] && strcmp(ctx->binding[i], nm) == 0) {
  // an invented counter name must never shadow a real variable
```

So a body that recalls a real variable spelled like the invented name is drawn
as if it were the counter. `ppvSumProd:718` passes `true` at the identical call;
the derivative is the one caller that does not.

**Reaching input** (executed). Reuses a fixture already in the file —
`prettyTest.c:3663`, `pgmBN`, commented *"body recalls a REAL 'n'"*:

```
LBL 'VBN'  RCL 'n'  END                      ( no MVAR, recalls a real n )
LBL 'VD7'  PGMDRV 'VBN'  3  f' 'x'  END      ( = PPVDRV("VD7","VBN",'x') )
```

By keypress: type those two programs, then `VISUAL 'VD7'`. Measured:

```
PROBE VD7 body RCLs a REAL n  (actual 'DERIV(n;n;3)')
PROBE-ORACLE VD7:  program=0E-1051  picture=1  drawn='DERIV(n;n;3)'
```

Trace: `ppvDerivVariable` finds no `MVAR` and returns `""` (:620);
`ppvInventName` (:664) scans `ctx->binding` (empty at top level) and
`ppvNameUsedInAst` (the AST holds only the literal 3) and returns `"n"`; :684
seeds the frame non-synthetic; the body's `ITM_RCL 'n'` passes the guard at :846
without firing and pushes a **free** VAR at :852.

**What the owner sees.** `d/dn(n)` evaluated at 3 — a slope of 1 — for a program
whose `XEQ` returns 0. Upstream's `deriv_pgm_variable` returns
`INVALID_VARIABLE` for an `MVAR`-less body, so no `STO` happens, `RCL n` reads
the owner's ordinary global `n` on every sample, and the result is constant. The
drawn `n` under the `d` and the drawn `n` in the body are different things and
nothing on screen says so. **No decline, no D-number.** Any body recalling a
variable named `n`, `m`, `k` or `j` reaches it.

**Contract violated.** `DESIGN.md:664-666`, in the BINDING rulings:

> **A sum's counter name is invented** (first free of `n`, `m`, `k`, `j`)
> because RPN has none, and a body that recalls a real variable spelled the same
> way DECLINES rather than let the invented name shadow it (V6).

`DESIGN.md:639`, added by this very wave, extends the invention to the
derivative — *"the picture invents a name exactly as `SUM` does for its
counter"* — and `prettyVisual.c:664` repeats it: *"as a sum body does"*. It is
not exactly as `SUM` does: `SUM` sets the flag that arms the guard and `DERIV`
does not. Round 2's own refutation pass leaned on that guard being armed when it
killed the RCL-after-Σ report: *"the body is guarded not by the AST scan at all
but by the live `bindingSynth` collision check in the `ITM_RCL` arm … while
`ppvBody`'s frame is open"* — true for `SUM`, false for the derivative path
added in the same commit.

**Why the battery misses it.** The pinned fixture `VD5` (`PPVDRV("VD5","VB3",'v')`,
prettyTest.c:4100/4377) is exactly this shape but recalls `v`, a name **outside**
the invented pool. The `SUM` twin over the identical body — `VS4` → `VBN` —
declines D12 and is pinned as V6 (prettyTest.c:3695). One letter separates the
covered case from the uncovered one.

**Bug class.** *An invariant extracted into a shared helper without the
obligation that made it safe.* R2-1 shared `ppvInventName` between `SUM` and
`DERIV` and did not share `bindingSynth`.

**Class-level test.** Split the derivative matrix by **what the body reads**,
which is the axis it has: `{recalls a name inside the invented pool}` ×
`{recalls a name outside it}` × `{consumes the stack}`, each under `PGMDRV` +
`f'`. The first must decline D12, mirroring V6; the other two must draw. Then
the cheap guard: append `VD7` to V65's `oracle[]` roster — it reds on the number
alone, with no expected string and nobody thinking about `MVAR`s.

---

### PP18R3-2 — R2-3 made the mirror pick the first DRAWABLE declaration, not the first declaration, so it no longer mirrors the walk it names

`packages/pretty-print/prettyVisual.c:616`

**What breaks.**

```c
if(first[0] == 0 && ppvNameIsDrawable(nm)) {
  strcpy(first, nm);
}
```

R2-3 correctly removed an unconditional `ppvNameIsDrawable` abort from the top
of the loop, and then added drawability as a **conjunct to the `first`
capture**. An undrawable declaration is now *skipped* rather than recorded, so
when the `f'` parameter matches nothing the walker falls back to a later
declaration that upstream never reaches.

**Reaching input** (executed). `ppvNameIsDrawable` (:293-307) admits A-Z/a-z
only, so any digit or any Greek letter — both of which C47 allows in a variable
name — is undrawable:

```
LBL 'VB6'  MVAR 'X1'  MVAR 'Y'  RCL 'Y'  ENTER  ×  END
LBL 'VD8'  PGMDRV 'VB6'  3  f' 'Z'  END
```

Measured: `PROBEVALS VD8: program=0E+1 picture=6.0 drawing='DERIV(Y×Y;Y;3)'`,
and V65 reds on it. Exactly two failures appeared in the whole battery, both the
probe's — a coverage hole, not a disturbed neighbour.

**What the owner sees.** `DERIV(Y×Y;Y;3)` = 6 while `XEQ` returns 0: upstream
stores the sample into `X1`, its first declaration, and the body's `RCL Y` reads
an unrelated global that does not vary. **Before R2-3 this program declined D18
honestly.**

**Contract violated.** The function's own banner, `prettyVisual.c:558-561`:

> `calcDeriv` asks `deriv_pgm_variable(label)` …, which walks the BODY
> program's own leading `MVAR` declarations and returns the one matching the
> `f'` parameter, else the FIRST declared, else `INVALID_VARIABLE`. **This
> mirrors that walk**, including REM transparency.

Upstream (`differentiate.c:286-322`) has no drawability notion at all —
`if(first == INVALID_VARIABLE) first = variable;` on every declaration
`findOrAllocateNamedVariable` accepts, and `registers.c:986` does no
character validation, so upstream's `first` genuinely is `X1`. R2-3's own
comment states the right rule and the code implements half of it
(`prettyVisual.c:604`):

> drawability is a property of the name we END UP with, not of every declaration
> we walk past.

The honest shape is to record `first` unconditionally and test
`ppvNameIsDrawable` once on the name actually returned, declining D18 there —
which is what the comment asks for and what the param-match arm three lines
above already does.

**Sub-case that is benign:** all declarations undrawable. `first` stays `""`,
the caller invents a name, and upstream's unconditional `fnFillStack` makes the
stack-reading body correct. The harmful shape is specifically
first-undrawable-then-drawable with a parameter matching neither.

**Bug class.** *A fix that carried across half of its own prescription* — the
skip was implemented, the relocated decline was not.

**Class-level test.** A mirror matrix over `{declaration spelling} × {param
matches: first / later / none}`: `MVAR 'X1'` then `MVAR 'Y'` with a third
parameter must decline D18 (upstream would vary `X1`, which cannot be drawn);
`MVAR 'X1'` alone with a third parameter must draw with an invented name; the
existing V61 covers match-beats-first. Then add `VD8` to `oracle[]`.

---

### PP18R3-3 — the EQN parser is the third producer of the big-operator-as-operand defect, and R2-2 fixed two of three

`packages/pretty-print/prettyEquation.c:578` (the `^` base), `:585-598` (the
superscript-run arm), `:636-652` (the product's left operand)

**What breaks.** `ppqPrimary:496` returns `ppqBigopConstruct`'s `PP_BIGOP`
bare. `ppqFactor`'s `^` arm makes that bare node the base of a `PP_SUP`
(`ppAppendChild(sup, n)`) with no `ppfWrapIf` and no `PP_PAREN`; its
trailing-superscript-run arm does the same into an HBOX; `ppqTerm`'s
`PPQ_IS_PROD` arm appends it as the left operand of `·` unwrapped. There is no
precedence value anywhere in `ppqParse` to correct — which is why R2-2's
one-line repair could not reach it. The only bracket a construct ever gets in
this file is `ppqScopeBody` (:149), which wraps the **body** and only when the
body's own runs carry a `+`/`−`.

**Reaching input** (executed). Type the equation `SUM(X;X;1;3)^2` — the `;`
comes from the ALPHA punctuation softmenu, exactly as the shipped pin EQ29 types
`SUM(X;X;1;3)` one softkey at a time — then `EQSHW`. Measured through the real
harness:

```
PROBE A sup-base   sig 'S(B(x|[x = 1]|3)|2)'          <- no P(...)
PROBE B mul-left   sig '[B(x|[x = 1]|3) . 2]'         <- no P(...)
PROBE C mul-right  sig '[2 . B(x|[x = 1]|3)]'         <- correctly bare
PROBE D frac-num   sig 'F(B(x|[x = 1]|3)|2)'          <- correctly bare, the bar scopes
PROBE E INTEG      sig 'S(B([x d x]|0|1)|2)'          <- same miss, not sum-only
PROBE_EVAL  SUM(X;X;1;3)^2 = 36.000…
PROBE_EVAL2 PROD(X;X;1;3)×2 = 12
```

`prettyLayout.c:341` places a `PP_BIGOP`'s body to the RIGHT of the stroke
(`relX = colW + 3`) and `:400-425` places a `PP_SUP`'s exponent at
`base.width + 1`, `relBase = -supDrop` — a fixed drop, not a raise to the base
box's ascent. The `2` therefore lands one pixel right of the body `x` at exactly
the height it occupies in `x²`. **The picture is Σ x² = 14 for an equation the
calculator evaluates as 36** — PP18-4's own factor-2.6 example, still live.

**Two honest narrowings**, both measured, neither fatal:

- **The `×` half is a wrong picture but not always a wrong number.** For `SUM`
  a constant factor distributes, so `SUM(X;X;1;3)×2` = 12 and the unbracketed
  drawing also reads 12. It bites for `PROD` (program 12, picture
  Π(x·2) = 48) and for any non-constant factor. The `^` half is wrong for both.
- **The editor's equation strip never shows it.** The shape measures 38 px tall
  against `prettyTryEquation`'s `ascent+descent > 23` cap (`prettyEquation.c:790`),
  so the strip falls through to the linear line. This is an **EQSHW-only**
  defect. (Also: the literal ASCII `*` does not reach — `PPQ_IS_PROD` accepts
  only `0x80b7`/`0x80d7` and the evaluator only `PRODUCT_SIGN` — but the real
  MULT key emits `0x80d7` and reproduces it exactly.)

**Contract violated.** R2-2's own ruling, `prettyFormula.c:317-322`:

> a big operator is not an atom **here either**. Its body is drawn to the RIGHT
> of the stroke, so a factor or exponent beside it binds INTO the body — the
> same defect PP18-4 fixed in the walker, left standing at the neighbour that
> shares this file.

The equation parser draws the identical shape through the identical layout and
got no equivalent. `TESTING.md:566` states the cross-producer invariant for the
neighbouring case — *"the parser sniffs runs for a `+`/`-`, the tree asks the
precedence, and they must agree"* — and for the operand case the parser has no
rule at all, so agreement is impossible by construction. Meanwhile
`DESIGN-HISTORY.md:17`, written in this range, says PP18-4 *"had been fixed at
one of the two sites that share the defect."* There are three producers, and
the count in the closing document is what will stop the next reader looking.
(Round 2's own D7-1 counts three correctly; the narrative does not.)

**Status: pre-existing and knowingly deferred, not introduced here.** Round 1's
PP18-4 named it (*"PP18 inherits the defect rather than introducing it"*), and
round 2's report declined to re-report it in three separate places. It is
re-raised because it is now the **last** unfixed site of a class the project has
now fixed twice, because five readers reached it independently, and because the
one number nobody had measured before — that the drawn form evaluates to 14
against the equation's 36 — is now measured. The refutation pass is right that
its *novelty* claim was wrong; it is not right that the defect is closed.

**Bug class.** *A scope rule implemented where it was noticed rather than over
the scope it names* — `DESIGN-HISTORY.md`'s own phrase, third recurrence.

**Class-level test.** The one round 2 already prescribed and got two thirds of:
one pin per producer of `PP_BIGOP`, asserting the same bracketing for the same
mathematics — V68/V69 (walker), B9 (capture), and an EQN string `SUM(...)^2`
through `ppqParse`. Three pins, one expectation. The EQN pin cannot be a
precedence mutation, because the parser has no precedence; it must assert the
signature.

---

### PP18R3-4 — the mirror's name-length bound is the buffer size, not upstream's tested limit, and an imported 15-byte `MVAR` name is walked past instead of ending the scan

`packages/pretty-print/prettyVisual.c:598`

**What breaks.** The mirrored loop has upstream's two exits, but one uses a
different constant: `len == 0 || len >= PPV_NAME_MAX` (16, documented at :53 as
*"== the evaluator's `varName[16]`"* — a buffer size) where upstream breaks at
`nameLength > MAX_LABEL_NAME_LENGTH` (14, the tested limit,
`differentiate.c:305`). A 15-byte declaration ends upstream's scan with
`first = INVALID_VARIABLE`; the package walks past it and keeps looking.

**Reaching input** (executed — this was round 2's PLAUSIBLE **P3**, and it was
reached). `defines.h:1188` calls 14 *"the longest label name the calculator can
produce"* because TAM alpha entry force-closes beyond 6 glyphs, so the input has
to arrive by import. It survives import:

- `saveRestorePrograms.c:_screenFileStep` sets
  `declaredLabel = (paramMode == PARAM_DECLARE_LABEL)` (:141) and only then
  applies `length > MAX_LABEL_NAME_LENGTH` (:152). `ITM_MVAR`'s item row is
  `PTP_REGISTER` (`items.c:3376`), i.e. `PARAM_REGISTER`, so the `MVAR` name's
  length byte is never bounded.
- The second loader guard, `programMemoryHasOverlongLabelName`
  (`manage.c:102-115`), tests `ITM_LBL` only.

A real `.p47` was written and loaded through `fnLoadProgram` (the harness's own
`ppcTestWriteAndLoadPgm`, the actual user import channel):

```
LBL 'VB7'  MVAR 'abcdefghijklmno'  MVAR 'x'  RCL x  ENTER  ×  END
LBL 'VD7'  PGMDRV 'VB7'  3  f' 'x'  END
```

It loaded, and the walker drew `DERIV(x×x;x;3)` — a picture worth 6. Upstream
breaks at 15, returns `INVALID_VARIABLE`, stores nothing, and the derivative is
**0**. With the parameter `'v'` instead, the walker draws
`DERIV(x×x;abcdefghijklmno;3)` — a 15-letter subscript upstream could not select
even in principle, since `allocateNamedVariableOnMiss` (`registers.c:963`) caps
a variable name at 7 glyphs.

**Blast radius of the correct constant: zero.** Substituting
`len > MAX_LABEL_NAME_LENGTH` turns the same fixture into `DERIV(x×x;n;3)` — the
invented-name convention the design already uses for "upstream varies nothing"
— and **no other assertion in the battery changes**.

**Contract violated.** `prettyVisual.c:562`: *"This mirrors that walk, including
REM transparency."* Two exits, one constant wrong.

**Why it ranks below the two above.** It needs an imported or hand-built program
file. It ranks above the remaining findings because the consequence is the
PP18-1 class — a confident picture asserting a number the machine does not
compute — and because the fix is one constant with a measured-empty blast
radius.

**Bug class.** *A mirrored predicate re-derived from the buffer instead of
copied from the original.*

**Class-level test.** One pin per mirrored bound, driven through
`ppcTestWriteAndLoadPgm` at the boundary: a 14-byte `MVAR` name must be
selected, a 15-byte one must end the scan exactly as upstream ends it. The same
harness reaches the `REM`-transparency arm that is currently recorded as an
unpinnable gap — it writes real program files, so `literalTailBytes`' encoding
is produced by the writer rather than by hand.

---

### PP18R3-5 — R2-4 stopped scanning CONSTRUCT nodes entirely, so a construct nested in another construct's LIMIT can be given the same counter name

`packages/pretty-print/prettyVisual.c:531`

**What breaks.** `ppvNameUsedInAst` now skips `PPA_VAR` nodes carrying
`PPV_F_BOUND` and no longer inspects `PPA_CONSTRUCT` nodes at all. A closed
construct therefore leaves nothing behind for the next one to collide with —
including when the closed construct ends up **inside** the next operator's
limits.

**Reaching input** (executed), built from fixture pieces already in the file
(`VNB` is the existing empty-body program):

```
LBL 'VNS'   1   ( from )
            1  3  1  Σn 'VNB'      ( the inner sum closes; invents 'n' )
            1                       ( step )
            Σn 'VNB'                ( pops step=1, to=<inner sum>, from=1 )
            END
```

Measured at HEAD: `SUM(n;n;1;SUM(n;n;1;3))`. With R2-4's scan reverted:
`SUM(m;m;1;SUM(n;n;1;3))` — **and V77 goes red**, so the trade is real and only
one of its two directions is pinned.

**What the owner sees.** A Σ whose subscript is `n = 1` and whose upper limit is
another Σ also indexed by `n`, with nothing on screen distinguishing them. The
inner binder shadows, so **the number is defensible** — what is lost is the
guarantee, and the guarantee was the whole point of scanning the tree.

**Contract violated.** The PP18-9 comment R2-4 deliberately left standing
immediately above the changed predicate, `prettyVisual.c:513-519`:

> The rule `DESIGN.md` states is about shadowing in the drawn formula; **the
> limits are inside the operator's visual scope, so they are part of it.**

R2-4's own justification (`:525-530`) scopes its exemption to *"a closed
**sibling** construct's counter"*. The implemented test is only "closed", and
the two differ for every construct that lands in the next operator's
`from`/`to`/`step` slot. V77's own comment states the rule as *"only free
variables and ENCLOSING constructs collide"* — and the outer sum does visually
enclose the inner.

**Correcting the record.** Round 2's report asserted, in R2-4's class-test
paragraph, that *"the nested case — five sums as each other's limits — declines
D12 before and after the fix"*. Measured at the two-sum shape, it does not
decline: it draws. That sentence is the only artifact that considered the shape,
and its premise is false.

**Bug class.** Same as PP18R3-3's, with the sign reversed: *a scope rule
narrowed to where the fix was noticed rather than to the scope its own comment
names.*

**Class-level test.** One pin per relationship between two constructs —
disjoint siblings (V77, exists), one nested in the other's **body**, one nested
in the other's **limit** — asserting distinct counters wherever the scopes
overlap visually and reuse wherever they do not.

---

### PP18R3-6 — the new capture pin is named `B9`, which is already a live pin; MUT-128's coverage row resolves through the documentation to the wrong test

`packages/pretty-print/prettyTest.c:1774`

**What breaks.** Two unrelated pins in `prettyTestCapture` are both prefixed
`B9`:

- `:1671` / `:1676` — the `ITM_SIGMAnINF` early-stop sum capture pin, inside
  `#if defined(OPTION_INFSUMS)`.
- `:1769` / `:1774` — R2-2's new bracketing pin.

Both compile into the same binary: `defines.h:61` defines `OPTION_INFSUMS` and
the only `#undef` is at `:284`, inside the `DMCP_BUILD`/`TWO_FILE_PGM` branch,
while `prettyTest.c` is `PC_BUILD`-only. `B10` and `B11` already exist
(`:1606`, `:1634`), so `B9` was not the next free number.

**Reaching path** (a reader's, and the one the next round is instructed to
take). `TESTING.md` now assigns the identifier twice and to two different
mutations:

```
:293  | MUT-128 | `ppfBigop` reports ATOM again (R2-2, the neighbour) | B9 |
:308  | MUT-62  | infinite sums fall back to invalidate (PP16) | B9 (FAIL line is glyph-suppressed — trust the counts) |
:371  - **B9** — the early-stop sum (`ITM_SIGMAnINF`) captures like any other sum …   (B-family prose reference)
:570  | B9 | the CAPTURE engine's big operator as an operand … |   (inside the V-family table)
```

A reader checking MUT-128's coverage follows the table to the B-family
reference at `:371` — the only place `B9` is *defined* in prose — and lands on
the infinite-sum pin, which **passes untouched under MUT-128**. That was
verified: applying MUT-128 produced exactly one failure in the battery, the new
`B9`, while the old `B9` ran and passed.

**Honest narrowing** (from the refutation pass, which killed a broader version
of this finding). At the console the harm does not appear: applying MUT-128 and
MUT-62 *together* prints two fully distinct, self-identifying lines, because the
identifier a reader sees is the whole `what` string and every emitter
(`ppTestFail`, `ppTestFailInt`, `ppcTestExpectSig`, `ppfTestExpect`) prints it
verbatim before any glyph payload. So the defect is on the **documentation
path**, not the failure path. It is still a defect there: MUT-62's own row says
*"trust the counts"*, and a count-based verification of MUT-62 now cannot tell
which `B9` produced the single failure.

**Contract violated.** This wave's own lesson, `DESIGN-HISTORY.md:37-38`:

> **A pin that exists only in prose is worse than a missing one: it stops anyone
> looking for the gap.**

A pin whose recorded name resolves to a different pin fails the same way one
level up. And R2-8's rule, from the same wave: *"A correction that does not
delete what it corrects is not a correction"* — here two records claim one label
without either deleting or cross-referencing the other.

**Bug class.** *An identifier minted without checking the namespace it lives
in.* Nothing machine-checks pin names; `grep -rn "prettyPrint test FAIL"
--include=*.py --include=*.sh` returns no tool that cross-references printed ids
against `TESTING.md`.

**Class-level test.** Not a runtime pin — a lint, run in the gate: extract the
first whitespace-delimited token of every string passed to `ppTestFail`,
`ppTestFailInt`, `ppcTestExpectSig`, `ppfTestExpect` and `ppvTestExpect`, and
fail on a duplicate. It is the same lint PP18R3-7 needs.

---

### PP18R3-7 — the round-2 wave reused audit tags `R2-1`, `R2-2` and `R2-4`, which already name different findings in this package; `R2-2` twice in one file

`packages/pretty-print/prettyFormula.c:316`

**What breaks.** `grep -n "AUDIT R2-2" packages/pretty-print/prettyFormula.c`
returns two hits, ~370 lines apart, naming unrelated rulings:

| tag | old (PP1–PP16 series, `e84e9a1db`) | new (this wave, `d3aacbb46`) |
|---|---|---|
| `R2-1` | emit at STAGE so the entry is recallable — `prettyCapture.c:473`, `:820`, `prettyTest.c:1241` | the `MVAR`-less decline was a regression — `prettyVisual.c:542`, `:566`, `prettyTest.c:4129`, `:4368`, `DESIGN.md:635` |
| `R2-2` | an over-wide row is refused rather than clipped — `prettyFormula.c:685`, `prettyInternal.h:173`, `prettyTest.c:1280` | a big operator is not an atom — **`prettyFormula.c:316`**, `prettyTest.c:1748` |
| `R2-4` | the counter saturated at 255 — `prettyCapture.c:653` | the pool spent on CLOSED sibling scopes — `prettyVisual.c:525`, `prettyTest.c:4254`, `:4601` |

Neither the old nor the new tags carry a series qualifier, so nothing in-file
disambiguates. The whole space is crowded: `AUDIT PP18` (32 occurrences),
`AUDIT R1` (21), `AUDIT R2` (20), `AUDIT R3` (12), `AUDIT R4` (7), `AUDIT R5`
(5) — an unqualified `Rn-m` is guaranteed to collide.

**Contract violated.** The package's own twice-stated practice, dropped without
comment. The PP1–PP16 report's **§Numbering**: *"This report's findings are
`A1`–`A14` so a grep is unambiguous, following the round-11 precedent where the
second leg took its own prefix."* PP18 round 1's **§Numbering**: *"This round's
findings are `PP18-1`–`PP18-16` and its design observations `D18-1`–`D18-7`, so
a grep is unambiguous."* **Round 2's report has no §Numbering section at all**,
and nothing in `DESIGN.md`, `DESIGN-HISTORY.md`, `TESTING.md`, the round-2
commit message or the `cross-model-audit` skill rules on tag scoping or
authorises reuse. This is also the class `MEMORY` already records as open in
forth-core: *"round 10's FHIST fix mints a duplicate label (unruled)"*.

**Cost.** A reader checking whether a ruling still holds — the standing practice
here, and the reason these tags are in the code at all — lands on the wrong
finding, in the same file for `R2-2`. It is paid later, when someone edits
`ppfBuildRow`'s width rule believing the `R2-2` they read in `DESIGN-HISTORY`
(bracketing) is the ruling that governs it.

**One correction to the finding as raised.** `DESIGN-HISTORY.md` does *not*
"open with two entries both headed audit round 2": it opens with round 2 then
round 1, and contains no entry for the **old** `R2-n` series at all (that series
lives only in `e84e9a1db`'s commit message). The document-level collision is
weaker than claimed; the code-level one is exact, and the code is where the tags
are.

**Bug class.** Same as PP18R3-6 — *an identifier minted without checking the
namespace*.

**Class-level test.** Extend the pin-name lint to `AUDIT <prefix>-<n>` tags:
fail on any tag whose `(prefix, n)` pair appears in two commits' worth of
distinct rulings, and require a `§Numbering` paragraph in every audit report
(the two reports that have one were the two that did not collide).

---

## 4. PLAUSIBLE

Survived refutation; nobody constructed the reaching input.

**P1 — V65 is one behaviour change away from being vacuous for all five
programs at once.** The oracle reads `viaProgram` out of `REGISTER_X`, evaluates
the drawing, and reads `viaPicture` out of `REGISTER_X`, **and never clobbers X
in between** (`prettyTest.c:4553-4592`). If `fnEqCalc` ever returned without
touching X and without setting `lastErrorCode`, `diff` would be zero and every
case would pass on a picture that never evaluated. Today it does not:
`parseEquation` raises through `displayCalcErrorMessage` and the pin's
`lastErrorCode` check catches it, so the pin is not vacuous now. Two readers
reached this independently, and the file's own B11 pin uses the clobber-first
idiom for exactly this reason (*"a value the recall must replace"*,
`prettyTest.c:1636`). *What would settle it:* push a NaN into X between the two
measurements, which closes it for free and costs one line.

**P2 — `lastErrorCode = ERROR_NONE` is assigned AFTER `setEquation`.**
`prettyTest.c:4580-4581`. A `setEquation` failure is therefore swallowed, and
`fnEqCalc` silently re-evaluates the **previous** iteration's formula against
this iteration's program value. Reachable only under RAM exhaustion during the
test run; the five produced strings are ~30 bytes against a 256-byte cap. *What
would settle it:* a fault-injected `setEquation` return, or moving the
assignment one line up.

**P3 — `ppvAstPrec` dereferences `ctx->ast[n]` without the
`PPV_NIL`/`>= PPV_AST_NODES` guard its sibling `ppvSerialize` has one line
later.** A `PPV_NIL` child would read `ast[255]` out of bounds. WRITE-SET says
reachable; REACHABILITY says no — OP2 children come only from `ppvPop`,
`ppvPush` rejects `PPV_NIL` before anything enters the stack, and the
`CONSTRUCT`'s two optional children (`child[2]` for F1DRV, `child[3]` for a unit
step) are both gated by an explicit item/NIL test before `ppvSerialize` is
called. PC_BUILD-only in any case. *What would settle it:* a fourth optional
child, or any future arm that pushes a NIL.

**Carried forward from round 2, not re-examined this round and still open:**
its P1 (`calcDeriv` demotes a non-numeric `MVAR` to `INVALID_VARIABLE` and the
mirror does not — value-dependent, statically unknowable) and its P2 (the mirror
matches by `strcmp` where upstream matches by resolved variable id, so
`CMP_NAME`'s folding of superscript/subscript/struck forms can diverge). Round
2's **P3 is closed**: it is PP18R3-4 above, reached through the import path.

---

## 5. Design observations (D7)

Shape, not defects.

**PP18R3-D1 — V65 is an oracle over the TEXT seam, and its prose claims the
picture.** `V65` drives `ppvTranspile`, the `PC_BUILD`-only text back end, whose
precedence table is deliberately independent of the drawing's. It is a real,
non-vacuous oracle for the class where the walker derives the wrong
*mathematics* from the program — PP18-1, R2-1, and both of this round's worst
findings. It cannot see a fault in the layout pass: revert `prettyVisual.c:1097`
to ATOM and V65 stays green on all five programs. The split is stated honestly
in code (`prettyTest.c:3374-3388`: *"what it CANNOT do is catch a fault in the
layout pass, which is what the node-shape pins … are for"*) and correctly
allocated in the mutation table (MUT-122 → V68/V69, MUT-128 → B9). It is
`TESTING.md:568`'s sentence — *"the only pin here that fails because the picture
MEANS the wrong thing"* — that reads wider than the mechanism, and that sentence
is what a reader will quote. Worth one clause: *the only pin that fails because
the picture means a different NUMBER*.

**PP18R3-D2 — nine of eighteen decline reasons have no pin, and the enum's
ordinals are user-visible.** Deleting `PPV_D_DERIVVAR` was correct and it moved
only `PPV_D_TOOBIG` (20 → 19), which is a sentinel `ppvDecline` never receives
— that finding was refuted (§6). But the mutation that refuted it found
something else: inserting an enumerator at **position 5** turns ten pins red
with explicit "expected 12, actual 13" diagnostics, while inserting one at
**position 13 leaves the gate green**. The pinned reason set from the 17
`ppvTestDecline` literals is `{1,2,3,5,6,7,10,11,12}`; D13–D18 are documented in
`DESIGN.md` and asserted by nothing. An insertion below 13 drifts six documented
numbers silently. Pre-existing, untouched by this range, and cheap to close with
one pin per unpinned reason.

**PP18R3-D3 — "a big operator is `PPF_PREC_ADD`" is now a literal in two files
with nothing forcing them to match, and absent in a third.**
`prettyFormula.c:323` and `prettyVisual.c:1097` each carry the value; the EQN
parser has no channel for it at all. Round 2's D7-1 called this shape out and
the fix made it more concrete rather than less: the natural home remains
`ppfBigop`, which all three could read, and the natural pin is one expectation
asserted against three producers.

**PP18R3-D4 — `ppvBody`'s `synthetic` argument is a bare boolean literal at
three call sites, and one of them is wrong.** `ppvBody(ctx, bodyIdx, name,
false, &body)` at `:505` (integral, correct — the parameter is upstream's own),
`:684` (derivative, **PP18R3-1**) and `ppvBody(..., true, ...)` at `:718` (sum,
correct). The flag decides whether the shadow guard arms, and nothing at the
call site says so. This is the D7 shape of the round's worst finding: a contract
that is right and that a caller got wrong is a defect of the contract.

**PP18R3-D5 — `ppvNameIsDrawable` is a package invention participating in a
mirror of an upstream function that has no such notion.** Upstream resolves any
name `findOrAllocateNamedVariable` accepts; the package additionally asks
whether it can be *drawn*. Three of the last four derivative findings — PP18-1's
fix, R2-3, PP18R3-2 — come from that seam, and the predicate's own comment
(`:291`) still claims "and subscript digits after the first" while the body
admits A-Z/a-z only. The seam is probably correct to keep; what is missing is a
single stated rule for **where** in the mirror the package's extra question is
allowed to be asked. R2-3's comment states that rule; the code does not
implement it.

**PP18R3-D6 — the package has no uniqueness check for any of its identifier
spaces.** Pin names, audit tags, mutation ids and decline numbers are all
free-text or hand-maintained, and two of the four collided in this one wave.
The pin-name lint proposed in PP18R3-6 covers three of them for a few dozen
lines of Python in the gate; the fourth (decline numbers) is D2 above.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. Mandatory section; this one is long because the wave's central
moves are sound and saying so precisely is most of this round's output.

### Killed by the refutation pass

**Deleting `PPV_D_DERIVVAR` silently renumbered the user-visible "too large to
draw" message from D20 to D19.** The arithmetic is right and every consequence
drawn from it is wrong. `PPV_D_DERIVVAR` sat at ordinal 19, second-from-last, so
the deletion shifted only position 20: D1–D18 — the entire catalogue
`DESIGN.md:616-624` documents — are character-identical across the range. The
one number that moved, `PPV_D_TOOBIG`, is **never passed to `ppvDecline`**; it
appears twice in the file, in the enum and in one `sprintf`, and
`prettyVisual.c:39` already scopes the contract to *"the decline catalog is
D1..D18 (`ppvDecline` callers)"*. The claimed "prose catalogue that must agree
and already does not" is the state the fix **repaired**: at `2c13232b4` the
D1..D18 comment was stale because `:564` held a live
`ppvDecline(ctx, PPV_D_DERIVVAR)` at 19. And the load-bearing generalisation —
"the pins will still pass, because they assert the same integer the code now
computes differently" — is false by mutation (see PP18R3-D2). The commit message
also says *"the catalog now runs to D19"*, so the renumbering was seen, reasoned
about and recorded.

**`B9`'s duplication makes a FAIL line unidentifiable at the console.** Refuted
by construction of the worst case: MUT-128 and MUT-62 applied *together* print
two distinct, self-identifying lines, because every emitter prints the whole
`what` string verbatim and MUT-62's glyph mangling is confined to the
expected/actual payload. The surviving half — the documentation path — is
PP18R3-6.

**The EQN parser is an unswept third site whose existence round 2 missed.** The
code observation is accurate and is PP18R3-3; the *novelty* claim is not.
Round 2's report, added by `d3aacbb46` inside the subject range, names the site
three times with the same line numbers, attributes it to PP14 inheritance via
round 1's PP18-4, records that this wave deliberately fixed only the walker,
and records that another reader raised it fresh and withdrew it. Its D7-1
enumerates three producers explicitly. The finding is reported above as a live
defect and **not** as a discovery.

**V65 evaluates the `PC_BUILD` text seam, so it is over-claimed as an oracle
over the picture.** Refuted as a *finding* and kept as PP18R3-D1: the scope
split is stated in `prettyTest.c:3374-3388`, restated at `TESTING.md:517-527`
forty lines above the sentence complained of, and correctly allocated in the
mutation table (MUT-122 → V68/V69, not V65). V68/V69 are node-shape pins run
through `ppvTestBuildNodes` + `ppfTestExpect`, not expectation strings, so a
reader following the prose is sent *toward* the layout pins, not away from them.

**The upstream patch's `showString` re-indent.** Confirmed on the merits and
demoted to §2: it is the mechanical half's output, it predates the range, and
`REVIEW_upstream-minimality_2026-08-27.md` already owns it with the fix idiom
prescribed. Re-reporting it as a round-3 finding would be the exact
double-counting `CODE_AUDIT.md`'s rule 6 forbids.

### Cleared by the finders

**R2-2 is correct for every shape `ppfBigop` can build** — the axis-(b) answer,
checked arm by arm by five readers independently and agreeing. `PPF_PREC_ADD`
can only *add* a bracket, never remove one. `ITM_YX`, `ITM_SQUARE`, `ITM_CUBE`
wrap the base at ATOM → the bigop now brackets (this is the fix). `ITM_MULT`
wraps both sides at MUL → brackets. `SUB`'s right operand wraps at `myPrec + 1`
→ brackets, redundant for a bigop but not wrong. `ADD`'s operands and `SUB`'s
left stay bare, which is the convention the comment cites and which VISUAL also
uses. `ITM_DIV`, `XTHROOT`, `SQUAREROOTX`, `ITM_1ONX` and `ITM_ABS` consult no
precedence at all because the bar, vinculum and bars scope structurally
(`DESIGN.md:568`, V49). `LOGXY`'s subscript slot and the function-form arms
always parenthesise. Both call sites thread it identically: `ppfFromCaptureNode`
(:447, live capture/PSHOW) and `ppfBuildEntry`'s `PPT_TKBIG` arm (:591, PHIST
replay) **stores** the returned precedence rather than forcing ATOM, so no
persisted format changed and the replay pager inherits the fix without a code
edit. Every shape reaches it: `SIGMAn`, `PIn`, `iSIGMAn`, `iPIn`, `SIGMAnINF`
and `INTEGRAL_YX` (`prettyCapture.c:533-549`) all return the same precedence.

**The integral gains a bracket it does not need.** `∫₀¹P(x)dx · 2` is
self-delimited — the body run ends in `d<var>` — so it was already unambiguous.
Cleared as consistent-by-design rather than flagged: `ppvAstToNodes:1091`
reports ADD for `INTEG` too, so the change makes PSHOW/PHIST **agree** with
VISUAL, which is the point of R2-2; disagreeing on the integral would recreate
the split it closed.

**The extra `PP_PAREN` node per bracketed big operator.** Real cost against
`PP_POOL_NODES` and `PP_MAX_DEPTH`; on failure `ppfCombine2` returns `PP_NONE`,
`ppfBuildEntry` returns false and the row drops. No reader could construct a
reaching entry (a history entry is capped at an 8-deep token stack and a Σ row
measures well under the pool), and `DESIGN.md:129-134` rules overflow *"never an
error screen; it is a legitimate 'too complex to pretty-print'"*. Unreached, not
a finding.

**But R2-2 reaches a third shipped surface the record does not name.**
`prettyValue.c:803` — the PP8 T-line live formula, `FLAG_PTLINE` — also builds
through `ppfBuildCurrent`, and the extra bracket costs width in a band bounded
at `baseY+31`. The fallback is graceful (the T line shows T's value), so this is
not a defect; but the fix comment and `TESTING.md`'s B9 row both say "PSHOW and
PHIST", and there are three.

**The `ppfBigop` comment's "/" claim, at both sites.** `prettyFormula.c:321` and
`prettyVisual.c:1091` both say ADD *"brackets it under ×, / and ^"*.
`ppfCombine2`'s `ITM_DIV` arm consults no precedence and appends both children
raw — *"FRAC scopes its children: no parens"* (`prettyFormula.c:101`). The
**behaviour** is right: a stacked fraction's bar spans the numerator, so a big
operator in it is visually scoped. The sentence claims a mechanism that is not
there, now in two files. Not flagged because nothing acts on it — but a future
reader "restoring" symmetry under `/` would add a bracket the bar already
provides.

**PP18-8's shared seed node, cleared on four independent checks.** One AST leaf
now sits in all eight body-frame slots, making the body tree a DAG. (a) No code
mutates an AST node after `ppvAlloc` — the builders only write the node they
just allocated. (b) `ppvAstToNodes` has no memo table and rebuilds layout per
visit, so a shared AST leaf never becomes a shared layout box with two parents.
(c) Sharing a **leaf** multiplies visits by its use count (≤ 8), not
exponentially; PP18-3's doubling needs a shared **internal** node, which `ENTER`
already produced before this change, and the `layoutFull` latch plus V66's count
still bound it. (d) On allocation failure `ppvPush` rejects `PPV_NIL` and
declines D16, the same reason the per-level version gave. The change strictly
reduces arena pressure and is the accurate model of what a body can read.

**`ppvBody` leaves `bindingCount` incremented on all five failure returns.**
Every one of them is preceded by a `ppvDecline` (depth, `ppvPush`'s NIL arm, the
post-walk `ctx->failed` test, `ppvPop`'s underflow, the opaque test), `ctx->failed`
latches, `ppvWalk`/`ppvRun` abandon the walk, and `ppvRun:1123` re-zeroes the
count. `ctx` is a per-walk stack local. There is no path that returns false with
`ctx->failed` clear, so no input observes the leaked frame.

**A free variable RCL'd AFTER a construct closes.** `1 3 1 Σn 'B'` then
`RCL 'n'` then `+` draws `SUM(n;n;1;3)+n` with a bound and a free `n` side by
side. Raised by three dimensions and retracted by all three on the reachability
trace: RPN pushes all three limits **before** the dispatch, so a later name can
never reach a limit slot; it can only become a sibling of an enclosing operator,
where it is either bracketed (×, /, ^, since PP18-4/R2-2) or conventionally
scoped (the `+` case). `Σ f(n) + n` is well-defined notation. This is
materially different from PP18-9, whose free `n` sat *in* the upper limit — and
it is the distinction that makes PP18R3-5 a finding, because there the later
thing is itself a binder that lands in a limit.

**Two disjoint sums where the SECOND body recalls a real `n`.** After R2-4 this
declines D12 where the pre-fix accidental reservation drew `Σₘ`. Cleared as
ruled behaviour: `DESIGN.md`'s V6 ruling is that a body recalling a real
variable of the same name declines, and a single sum in that shape declined
before and after. Retrying the next candidate would be an enhancement.

**`ppvDerivative`'s `ppvNameInList(ctx->binding, …, sampled)` check at :679.**
Non-falsifiable on the invented path — `ppvInventName` already excluded every
binding — and load-bearing on the `MVAR`-derived path. Noise, not a defect.

**Every cap in the changed code guards before the write.** `ppvIntern`'s
`(uint32_t)poolUsed + len > PPV_POOL_BYTES` promotes before the add and is
correct at exactly 512. `ppvLeaf`'s `textLen = (uint8_t)len` cannot truncate:
`getStringLabelOrVariableName` (`decode.c:120`) reads a single `uint8_t` length
and clamps further against `firstFreeProgramByte`. `ppfBigop`'s buffers are
sized against their producers (worst `"%s(%s)d%s"` is 58 into `text[96]`; the
comment's "66 worst case" overstates and is still safe). `ppfBuildEntry`'s token
decoder checks `sp >= 8` before every push and `sp < 1`/`sp < 2` before every
`sp-1`/`sp-2` read. `bindingCount`, `callDepth`, `stepsWalked` and `astUsed` all
guard before the increment. The four-candidate name pool returns NULL one past
the end and both callers decline D12.

**V65's global side effects.** It runs five real programs (setting
`currentSolverProgram`/`currentSolverStatus` through their `PGMDRV`/`PGMINT`
steps), overwrites `currentFormula` via `setEquation`, calls `fnEqNew` when no
formula exists, and clobbers X — restoring only `calcMode` (`uint8_t`, correct;
`c47.h:422`). The capture battery explicitly saves and restores those solver
globals around its own big-operator block, so the asymmetry is visible. Cleared:
no pin after it in `prettyTestVisual` reads what it perturbs, `prettyTestReal`
is a static transpile over program bytes, `graphs_cov` re-stages its own
programs (`testSuiteList.txt:468-471`), and the `fnEqNew`/`setEquation` idiom is
pre-existing at V44 and throughout the EQ family. Recorded as an argument from
inspection, not a measurement, and as the kind of thing that becomes a finding
when a pin is added after it.

**V65's oracle strength, examined rather than assumed.** Agreement is on a
**number**, so a wrong picture that happens to evaluate the same passes. Three
of its five entries land on 6, but the mutations that matter turn the drawn
function into a constant evaluating to 0, so 6-vs-6 still discriminates; and
`VD5`, whose true answer is 0 on both sides and which would therefore pass on
any constant drawing, was correctly left **out** of the roster. Not a defect —
worth knowing before leaning on it.

**Pin vacuity, worked pin by pin — the axis's primary result.** Every one of the
five new pins has a concrete deletion that reds it, and readers verified them by
hand rather than inheriting the mutation table:

- **V75/V76** — delete `stk->liftDisabled = false` from the `ITM_PGMINT` or
  `ITM_PGMDRV` arm and the fixture walks `RCL a → [a]`, `ENTER → [a,a]` latched,
  the construct leaves the latch armed, `RCL b` **overwrites** the dup → `[a,b]`,
  `ADD → [a+b]` at depth 1, `MULT` underflows → D10 decline. Both red.
- **V77** — dies to per-level seeding by arena arithmetic (5 sums × 8 seeds = 40
  nodes, plus 15 limit literals + 5 constructs + 4 adds = 64 > `PPV_AST_NODES`
  48 → D16), and to counting bound reads as collisions (the four candidates are
  exhausted by the sibling seeds → D12). Two mutations, two different D-numbers,
  one pin.
- **B9** (the new one) — the `P(...)` wrapper in its expectation is exactly what
  reverting `ppfBigop` removes, and it builds through `ppfBuildCurrent` →
  `ppfFromCaptureNode` → the capture engine's own precedence threading rather
  than a precedence the pin supplies. Verified by running MUT-128: one failure,
  and it is this pin.
- **V65** — delete the `setEquation` call and from the second iteration on it
  evaluates the previous program's drawing against this program's value and
  reds. Non-vacuous today; see P1/P2 for the two ways it could stop being.

**The mechanical warning fix (`10e49e084`) is genuinely mechanical.**
5 × 12 chars + 4 separators = 64 bytes into `want[128]`, and a truncating
`snprintf` would red V77 rather than hide it.

**Upstream discipline: zero override churn in the range.**
`git diff --name-only 2c13232b4..HEAD -- packages/pretty-print/patches/` is
empty and every `files/` hash matches its working-area twin and its manifest
entry. The whole fix wave landed in package-owned sources. Merge tax added by
this wave: none. Also cleared: the 565-line block appended to
`solver/equation.c` (ruled at `DESIGN.md:474`, purely appended at EOF, coupled
only to file-local parser macros — the "leave a seam behind" case the skill
itself says to leave alone); `keyboard.c`'s three deleted containment lines,
`items.c`'s ten `CAT_FREE` rows, `defines.h`'s `NUMBER_OF_SYSTEM_FLAGS`,
`softmenus.c`'s two announced slot claims — all catalogued in `DESIGN.md` §0.1,
§7 and the deliberate-exceptions list; and `items.h` keeping `ITM_0217`/`0218`/
`0219` defined while `items.c` replaces those rows (a stale alias with no
behavioural consequence and no merge cost).

**`ppfTestSigNode`'s `strlen(out) + 24 >= cap` entry guard** admits unbounded
growth in the HBOX arm, where the per-child `strcat(out, " ")` happens after the
child has already returned early. It needs ~70 siblings under one HBOX with the
buffer near 168 of 192; nothing builds that, it is test-only, and it predates
the range.

**`PPV_FRAG_MAX` (255) is now referenced nowhere.** Dead constant left behind
when the fragment pool went with the text back end. Not a defect.

---

## 7. Verdict

**Would I ship this? Not without re-ruling the two derivative findings.**

The wave's central moves are sound and five readers could not break them. R2-2
is right, and right for every shape — the axis-(b) question comes back clean at
the function it asks about, with the one qualification that the class has a
third producer nobody has fixed. PP18-8's shared seed is the accurate model and
tightens the arena. R2-4's *stated* rule is right. The mechanical warning fix is
mechanical. Upstream discipline is unblemished: no patch byte moved.

But **both regressions this round found are silent wrong pictures on the
derivative path, and both are the class the wave existed to fix.** R2-1 extended
name invention from `SUM` to `DERIV` and carried the invention without the flag
that makes invention safe, so a body recalling a global named `n` draws a slope
of 1 for a program that returns 0. R2-3 removed a premature refusal and, in the
same edit, changed which declaration the mirror selects, so a program that
declined honestly now draws a picture over a variable upstream never varies. In
both cases the pre-fix code refused and the post-fix code asserts a number the
calculator does not compute. Neither has a D-number, and on a DM42n a D-number
would not reach the owner anyway.

**Where would it break first?** On an owner who names a loop variable `n`. That
is the first letter of the invented pool and the most natural name in the
language; a derivative over a body that recalls it draws confidently and wrongly
with no refusal. Second on an owner who types `SUM(...)^2` into EQN and presses
EQSHW — a shipped surface, a rendering of their own equation that means 14 for
a formula the machine evaluates as 36.

**The axis (a) answer in one line.** The pins are not vacuous; the *battery* is.
V65 is a real differential oracle and its five programs miss both of this wave's
regressions by one letter each. Four programs appended to `oracle[]` red them
both, with no expected string and no reasoning about `MVAR`s — which is the same
conclusion round 2 reached about the pin it found missing, arrived at from the
other side.

**What I would leave alone if the goal were correct code rather than a clean
audit.** PP18R3-5 — two nested sums both indexed `n` is unreadable, not wrong;
the inner binder shadows and the number is right. PP18R3-6 and PP18R3-7 —
identifier hygiene, real and cheap, but nothing computes a wrong answer because
of them; one lint closes both and the design observations that follow from them
(D6) matter more than the instances. Most of PP18R3-D2 — the unpinned decline
numbers are a coverage shape nobody has tripped in three rounds. And the
carried-forward P1/P2 from round 2, which are genuine divergences from upstream
that nobody can reach. That leaves three things worth an engineer's afternoon:
**PP18R3-1, PP18R3-2, and the third `PP_BIGOP` producer** — plus the one
constant in PP18R3-4, whose blast radius was measured empty.

**The pattern worth naming, fourth round running.** The round's worst findings
came from the previous round's fixes, and both have the same shape at a finer
grain than "fixes regress": **a fix that relocates a decision rather than
removing one.** R2-1 moved the invention into a shared helper and left the
obligation behind. R2-3 moved the drawability test and left the decline behind.
`MEMORY` already records that relocating state is the most dangerous fix shape;
this round says the same is true of relocating a *check*, and that the tell is a
fix whose comment states a rule in two clauses and whose diff implements one.

---

## 8. Round and exit state

**Round 3** of the PP18 audit, over the round-2 fix commits. Eight finder
dimensions (contracts, lifecycle, arithmetic, error paths, guards, tests,
design, upstream) ran blind to each other; every raised finding went to an
independent refutation pass with one assigned lens (reachability, correctness,
intent), instructed to default to REFUTED and to prove coverage claims by
mutation.

**Counts.** Eleven findings raised and survived refutation; **seven** after
deduplication across dimensions. Four refuted. Three PLAUSIBLE, two of them
carried forward from round 2. One confirmed finding demoted to §2 under rule 6.
Round 2's P3 was **promoted** from PLAUSIBLE to CONFIRMED by constructing the
import path it needed.

**Independent agreement.** PP18R3-2 was reached by all eight dimensions.
PP18R3-1 by seven of eight (every dimension but upstream). PP18R3-3 by five.
PP18R3-5 by three. That is the strongest convergence any round of this audit has
produced, and it is the argument for the fan-out: the two derivative findings are
one line apart in the same function and no single reader found both by the same
route.

**Every CONFIRMED finding is backed by an executed probe**, applied, observed
and reverted inside an isolated worktree; four of them by a *number* — the
program's value against the drawing's, taken through `fnExecute` and `fnEqCalc`
in the real build. Every worktree finished clean; no foreign edits were
encountered.

**Exit criterion: not met.** Seven CONFIRMED findings, five of them in code or
documents this wave wrote. The count resets again, and the rule against closing
on a round that contains fixes stands. **No out-of-family reader ran this
round** — the exit criterion requires at least one, and three consecutive
in-family rounds is exactly the blind-spot shape `CODE_AUDIT.md` warns about.

**Round 4's axis, in priority order.** (1) The axis round 2 proposed and round 3
did not take: **the rulings, statement by statement** — every `PPV_D_*` against
its catalogue entry, every `Rulings.` bullet in `DESIGN.md` against its
implementation, in both directions. Three of this round's seven findings are
code-and-document divergence and the mechanical form of that question has still
never been run. (2) **The layout pass**, `prettyLayout.c`'s measure and paint,
which no round has read end to end and which every bracketing finding in three
rounds ultimately rests on. (3) It should be **out-of-family**, per the exit
criterion.

**Process items, two of them repeats.** The verifier worktrees again spawned at
`e21af8d28`; round 2 asked for a `git merge-base --is-ancestor` guard before
round 3 and it was not added, so ten readers each spent a step detecting it.
`packages/forth-core/build-test.sh` still returns a meaningless green for a
`prettyVisual.c` mutation, and one reader hit it again this round. New this
round: **the mutation-and-revert cycle must also revert `files/` and
`.refresh-manifest.json`**, which the gate's own refresh step regenerates from
the mutated source — every reader here handled it, and a reader who did not
would leave a mutated package behind a clean-looking source diff.
