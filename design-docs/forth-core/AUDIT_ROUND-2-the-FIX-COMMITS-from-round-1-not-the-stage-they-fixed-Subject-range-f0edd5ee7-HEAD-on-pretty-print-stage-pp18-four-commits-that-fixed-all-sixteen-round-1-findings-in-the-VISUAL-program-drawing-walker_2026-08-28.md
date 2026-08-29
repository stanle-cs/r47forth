# Audit round 2 — the PP18 fix commits, not the stage they fixed, at `2c13232b4`

*(Filename truncated from the full subject line: the ext4 limit is 255 bytes and
the subject as given is 3,062. The rotated axis, the fix inventory and the
pre-verified exclusions are reproduced in §1 rather than in the name.)*

Subject: `f0edd5ee7..HEAD` on `pretty-print/stage-pp18`, four commits, the wave
that closed all sixteen round-1 findings. Eight finder dimensions ran blind to
each other; every raised finding then went to an independent refutation pass
with one assigned lens — reachability, correctness or intent.

**Eight distinct CONFIRMED findings, three PLAUSIBLE, four REFUTED.**
Seventeen findings were raised and survived refutation; deduplicated across
dimensions they are eight. Five of the eight were reached independently by two
to four readers. Ten of the surviving verdicts are backed by a probe or
mutation applied, observed and reverted inside an isolated worktree — the D19
decline, the D18 abort, the capture/VISUAL bracketing divergence, the
name-pool exhaustion, the two unpinned lift-latch arms and the 6.0 that XEQ
returns for the program VISUAL refuses were all *executed*, not argued.

The round asked two questions the last one did not.

**(a) Did fixing VISUAL disturb the neighbours?** No, and this is mechanical
rather than argued: `prettyFormula.c`, `prettyEquation.c`, `prettyLayout.c` and
`prettyCapture.c` are byte-unchanged across the range, the range adds no
`patches/` hunk, and the two new values VISUAL hands the shared builders
(`PPF_PREC_ADD` as an operand precedence, `PP_NONE` as an absent step) are
values those builders already handle. The capture engine's PSHOW/PHIST rows,
the EQN strip and EQSHW are undisturbed by construction. What the fixes did to
the neighbours was leave one behind: PP18-4 established that a big operator is
not an atom and put that answer in one of the three producers of `PP_BIGOP`
nodes. `ppfBigop` still reports `PPF_PREC_ATOM`, so the same sum draws
bracketed in VISUAL and bare in the history — same build, same node kind,
same `ppfCombine1`.

**(b) Are the new refusals honest?** Two of them are not. The worst finding in
this report is a functional regression inside round 1's own worst fix: the new
D19 declines *every* derivative whose body declares no `MVAR`, on a stated
premise — "upstream varies nothing … the derivative is 0 whatever the body
says" — that upstream contradicts twice in its own comments and once in its
control flow. `_differentiatorIteration` calls `fnFillStack(NOPARAM)`
unconditionally, *before* the `if(variable != INVALID_VARIABLE)` that guards
only the `STO`. The ordinary stack-consuming RPN function body is differentiated
correctly by the calculator, drew correctly at `f0edd5ee7`, and is refused at
`HEAD`. The single no-`MVAR` fixture in the suite is the one body shape for
which the premise happens to hold — which is, verbatim, the bug class round 1
named for itself: *the only fixture satisfies the assumption the code never
checks*. It recurred inside the fix for it, one round later.

Five of the eight findings are in code the wave wrote. That is consistent with
the recorded fix-regression rate and is the argument for this round existing.

Nothing was fixed. The tree this report finishes on is the tree it started on;
every probe was reverted in the worktree that made it and the gate is green at
`2c13232b4`.

---

## 1. Subject and coverage

**Tip.** `2c13232b4` on `pretty-print/stage-pp18` ("docs: record the post-audit
flash figure"). Range `f0edd5ee7..HEAD`:

| commit | what it did |
|---|---|
| `472d542bd` | unused locals in the decline path (mechanical half) |
| `006210aad` | the three worst findings — a wrong picture (PP18-1), a hang (PP18-3), a blank screen (PP18-2) |
| `190bbb2fd` | the rest of the round — PP18-4/5/7/9/10/11/14/15/16 and the pins that missed them |
| `2c13232b4` | docs: the post-audit flash figure |

**Diff.** 13 files, +2,966 / −172. The code write set is three files:
`prettyVisual.c` (+273), `prettyTest.c` (+446), `prettyInternal.h` (1 line),
plus one test-list header, their generated `files/` twins and the refresh
manifest. The rest is documentation: `DESIGN.md` (+21, one hunk), `TESTING.md`
(+34), `DESIGN-HISTORY.md` (+61) and round 1's own report checked in.

**The fixes under review** (as characterised in the tasking, verified against
the diff): PP18-1 `ppvDerivVariable` mirrors upstream's `deriv_pgm_variable` —
walk the body label's leading `MVAR` declarations, REM-transparent, capped at
`MAX_MVAR_DECLARATIONS` = 18, return the one matching the `f'` parameter, else
the FIRST declared, else decline D19; the body is seeded with that name, not
the parameter. PP18-3 `ppvAstToNodes` gained a `ctx->layoutFull` latch checked
at entry plus an operand check before the second recursion. PP18-2
`ppvPaintFullScreen` measures before clearing and returns `bool`; when neither
surface fits, `fnPrettyVisual` raises `ERROR_INVALID_DATA_TYPE_FOR_OP` and
paints nothing. PP18-4 the `PPA_CONSTRUCT` arm sets `*outPrec =
PPF_PREC_ADD`, with a nested `CONSTRUCT` used as a construct BODY exempted
from `ppfWrapIf`. PP18-5 `ITM_XEQ`, `ITM_PGMINT` and `ITM_PGMDRV` clear
`stk->liftDisabled`. PP18-7 `varOff` widened to `uint16_t`. PP18-9
`ppvNameUsedInAst` scans every live AST node before choosing an invented
counter. PP18-10 the construct's step operand is checked for `PP_NONE`.
PP18-11 the test driver's output `fopen` is checked.

**Rotated axis.** Round 1 asked whether the refactor was faithful, and every
finding came from the failure envelope the text back end used to carry. Round 2
asked (a) whether fixing VISUAL disturbed the code it shares with the capture
engine (`ppfCombine1`, `ppfCombine2`, `ppfFromCaptureNode`, `ppfBuildRow`) and
with the EQN strip and EQSHW (`ppqBuildBigop`, `ppqBigopConstruct`,
`ppqShowRender`); and (b) whether the refusals the fixes added are honest,
reachable, and do not refuse things they should draw.

**Read at line level** (union across the eight dimensions): the full four-commit
diff by all eight; `prettyVisual.c` in full (1,530 lines) by six, and the
pre-fix `f0edd5ee7` version of it by three — the before/after read is what
established that R2-1 is a regression rather than a pre-existing limit.
`prettyInternal.h` in full. `prettyFormula.c` `ppfWrapIf` / `ppfCombine1` /
`ppfCombine2` / `ppfBigop` / `ppfFromCaptureNode` / `ppfBuildRow`'s token
machine. `prettyEquation.c` `ppqBuildBigop` / `ppqBigopConstruct` /
`ppqPrimary` / `ppqFactor` / `ppqTerm`. `prettyCapture.c` `ppcClassify`, the
`PPC_MO` / `PPC_DY` arms, the `PPC_BIGOPSUM` / `PPC_BIGOPINT` staging and slot
writeback. `prettyTest.c`: the whole changed half (fixtures `pgmB1`-`B4`,
`pgmD1`-`D5`, `pgmXP`, `pgmBIG`, `pgmNB`, `pgmSQ`, `pgmPX`, `pgmOFF`,
`pgmCOL`, `pgmXA`/`XB`, pins V59-V74, the V58 `fopen` fix) plus the V-family
driver header and V18/V44.

**Upstream read by execution path, not from memory:**
`src/c47/solver/differentiate.c` (`deriv_pgm_variable`,
`_differentiatorIteration`, `calcFuncValues`, `derivativeVariable`,
`calcDeriv`); `src/c47/items.c` (the `SLS_STATUS` epilogue at :604-610 and the
item rows for XEQ, PGMINT, PGMDRV, LBL, REM, MVAR, PAUSE, SNAP, NULL, RTN,
STO, RCL, ENTER, FILL, DROP, DROPY, XexY, CLX);
`src/c47/programming/lblGtoXeq.c` (`executeOneStep`'s PTP dispatch);
`src/c47/programming/manage.c` (`boundProgramNameLength`,
`scanLabelsAndPrograms`); `src/c47/error.c` (`displayCalcErrorMessage`);
`src/c47/screen.c` (the `SCRUPD_MANUAL_STACK` gate); `src/c47/defines.h`
(`MAX_LABEL_NAME_LENGTH`, `MAX_MVAR_DECLARATIONS`, `EXTRA_INFO_ON_CALC_ERROR`,
the band constants).

**Docs read:** `DESIGN.md` in full by two readers and §1/§3/§6 and the whole
VISUAL/PP17-PP18 section by six; `DESIGN-HISTORY.md`'s 2026-08-28 entries;
`TESTING.md`'s V-family section and the MUT-115..126 table; round 1's report
at the PP18-1/-4/-5/-16 findings in full and skimmed elsewhere (1,564 lines).

**Not reached, and it matters where.** `prettyLayout.c`'s measure and paint
internals were taken from their declared contracts in `prettyInternal.h` by
most readers — one reader traced `PP_SUP` and `PP_BIGOP` measure directly and
that trace is what killed a refutation of R2-2, but the rest of the layout pass
is unread. `prettyValue.c` beyond its `ppReset` call sites. `prettyCapture.c`
outside the classifier and the two apply arms. The equation-language evaluator
half (`parseEquation` interception), which is why one lead in §6 stayed a lead.
The browsers and `solver/` overrides. The `patches/` and `files/` generated
trees (spot-checked byte-identical to the working area; the refresh manifest
agrees). No reader ran the simulator, so no finding here is backed by a
photograph of an LCD — the drawing evidence is transpiled strings, node-shape
signatures and pixel sums. Three of the eight dimensions were read-only and did
not build; their claims are traces, and where a trace produced a numeric claim
(R2-1's "XEQ returns 6") a later verifier measured it rather than inheriting it.

**One process fact worth recording.** Every verifier worktree spawned at
`e21af8d28`, a forth-core README commit on an unrelated branch, with
`git log f0edd5ee7..HEAD` empty. Each had to detect that and `git checkout
2c13232b4` by hand. Every verdict in this report states the ref it worked at,
because a reader who had not noticed would have audited the wrong tree — round
1's wrong-range failure arriving by a third door.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh` is green at `2c13232b4`,
solo and combined (pre-verified in the tasking, and re-run to completion by
four verifiers after reverting their probes; the recorded tails are
`==> [combined] GREEN / ==> PRETTY-PRINT GATE GREEN`, exit status 0, testSuite
OK in 167-185 s). Compiler warnings clean.

**Mutation set.** MUT-115..MUT-126 red-verified. MUT-118 and MUT-119 survive
alone by design — the PP18-3 fix carries two deliberately redundant guards,
each healing the other's mutation, and only MUT-121 (both removed) reproduces
the shipped shape and reds V66. Recorded as redundancy, not a coverage hole,
and not re-litigated here.

**Drawing regression.** The DBLINT drawing is byte-identical to the pre-refactor
PP17 screenshot, re-verified by `cmp` *after* the PP18-4 precedence change,
which is the right order: that change alters bracketing for constructs used as
operands and could have moved a shipped picture.

**Budget.** `2c13232b4` records flash 1,146,432 → 1,151,640, **+5,208 B** for
VISUAL entire, of which the audit-round fixes are ~440 B. RAM unchanged; the
48-node arena and 512 B pool are the same objects PP18 shipped.

**`design-audit.sh`** is `design-docs/forth-core/design-audit.sh` and is
forth-core's; there is no pretty-print equivalent, so no override-budget check
ran. The substitute check is that the range touches no file under
`packages/pretty-print/patches/`: every changed C file is package-owned, the
wave adds no new override hunk and nothing new to conflict on an upstream
rebase.

**One harness trap surfaced during verification, and it is a live hazard for
whoever fixes these findings.** `./packages/forth-core/build-test.sh` does not
refresh pretty-print. A verifier mutating `prettyVisual.c` under it got GREEN
with the edit never reaching `files/`, i.e. never compiled. The pretty-print
gate is the only one that exercises this code, and a mutation is only real
once the marker appears in `packages/pretty-print/files/prettyVisual.c` *and*
in `build.sim/custom_pkg_shadow/prettyVisual.c`.

**A second hygiene note.** Two verifiers wrote build transcripts to a shared
`/tmp/probe.log` and read each other's tails, one of them a RED run from a
different worktree. No tree was cross-contaminated (each verdict was taken from
its own `build.sim/meson-logs/testlog.txt` and from markers only that worktree's
source could emit), but the next round should use a per-worktree log path.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner.

---

### R2-1 — the new D19 refuses every `MVAR`-less derivative body, including the stack-consuming one upstream differentiates correctly

`packages/pretty-print/prettyVisual.c:564`

**What breaks.** `ppvDerivVariable` walks the body label's leading `MVAR`
declarations and, finding none, declines `PPV_D_DERIVVAR`. It never looks at
the body. Every derivative over a program that takes its argument off the
stack — the ordinary RPN function body — is refused.

**Reaching input** (executed, not reasoned):

```
LBL 'VB5'  ENTER  ×  END          ( f(x) = x·x, consumes the stack, no MVAR )
LBL 'VD6'  PGMDRV 'VB5'  3  f' 'x'  END
```

`XEQ 'VD6'` returns **6.0**, measured through a probe that drove
`fnPgmDrv` / `fn1stDerivVar` directly: `PROBE upstream f'(v) over VB5: err=0
type=1 X=6.0`. `VISUAL 'VD6'` declines: `PROBE VD6 transpile ok=0 text=''
decline=D19 step=3`. Path: `fnPrettyVisual` → `ppvRun` → `ppvWalk` →
`ppvStep(ITM_F1DRV)` → `ppvDerivative` (:601) → `ppvDerivVariable(bodyIdx=VB5,
param="x")`; `labelList[VB5].instructionPointer` is the step *after* the LBL,
i.e. `ITM_ENTER`, so the `d=0` iteration matches neither `ITM_REM` nor
`ITM_MVAR`, breaks with `first[0]==0`, and :564 fires. On the device it
surfaces at :1495 as `ERROR_INVALID_DATA_TYPE_FOR_OP`.

**This is a regression, not a pre-existing limit.** `git show
f0edd5ee7:packages/pretty-print/prettyVisual.c` has no `ppvDerivVariable`
call: `ppvDerivative` passed the `f'` parameter straight to `ppvBody`, so this
program drew `DERIV(x×x;x;3)` — which evaluates to 6, the number XEQ returns.
That `ppvBody` handles an `ENTER`/`×` body is independently proven by the
shipped V62 pin, whose fixture `pgmB4` is the *same* body with `MVAR`s added
and which expects `DERIV(y×y;y;3)`.

**Contract violated.** The fix's own banner asserts what upstream denies —
`prettyVisual.c:514`:

> When upstream returns `INVALID_VARIABLE` it varies nothing: every sample is
> the same point and the derivative is 0 whatever the body says.

Upstream says the opposite twice and implements the opposite once.
`differentiate.c:286`, the header of the function being mirrored:

> Return the variable to perturb, or `INVALID_VARIABLE` for a program that
> declares none and therefore reads the stack.

`differentiate.c:337`:

> feed both channels: the stack for a program that consumes X, the variable for
> one that recalls its `MVAR`

and the stack channel is unconditional — `_differentiatorIteration` writes the
sample into `REGISTER_X` and calls `fnFillStack(NOPARAM)` at
`differentiate.c:329`, **before** the `if(variable != INVALID_VARIABLE)` that
guards only the `STO`. `calcDeriv` does not bail on `INVALID_VARIABLE`. The
stencil points genuinely vary (`differentiate.c:411-413`).

The zero-derivative claim is true for exactly one sub-class: a body that
`RCL`s a name, which `fnDeriv` froze at the point `STO`'d once
(`differentiate.c:187`). That sub-class is `pgmB3`, V63's fixture.

**Bug class.** *The only fixture satisfies the assumption the code never
checks* — round 1's own name for the class, recurring inside round 1's fix for
it. Secondarily: a refusal justified by a claim about upstream that was
reasoned rather than measured.

**Class-level test.** Split the no-`MVAR` row of the 3×2 matrix by the body's
INPUT CHANNEL, which is the axis the matrix never had: `{body RCLs a name}` ×
`{body consumes the stack}`, each driven by `PGMDRV` + `f'`. The first must
decline (V63's existing shape, honest), the second must draw. Then the real
guard, which is the oracle R2-5 says was delivered and was not: run the program
through `execProgram`, evaluate the walker's own drawing through `fnEqCalc`,
require agreement to tolerance, with no expected string in the pin. That pin
reds on this finding without anyone thinking about `MVAR`s.

**Also retract, with the code:** the pin comment at `prettyTest.c:4284`
("upstream varies nothing and returns 0 for any body"), `DESIGN-HISTORY.md:19`
("A body declaring no MVAR returns 0"), and round 1's own class-test text,
which prescribed "decline where it returns `INVALID_VARIABLE`". The fix
implemented the spec faithfully; the spec's premise was false.

---

### R2-2 — PP18-4's class is unfixed at the neighbour: `ppfBigop` still reports ATOM, so a captured or replayed sum takes no brackets

`packages/pretty-print/prettyFormula.c:316`

**What breaks.** `ppfBigop` opens `*outPrec = PPF_PREC_ATOM;` and never
reassigns it on any return path. Everything downstream of it — the capture
engine's live formula display and the persisted history rows — therefore treats
a big operator as an atom, exactly as VISUAL did before PP18-4.

**Reaching input.** With `FLAG_PRETTYP` on: `1 ENTER 5 ENTER 1`, ADV menu →
Σn → pick a program, then press `x²`. `prettyCapture.c:1090` mints `PPN_BIGOP`
and leaves it in `ppcSlot[0]`; `x²` classifies `PPC_MO` and
`prettyCapture.c:878` builds `PPN_OP1(ITM_SQUARE, child[0] = that BIGOP)`.
Render: `ppfFromCaptureNode` `PPN_OP1` (:417) → `ppfBigop` (ATOM) →
`ppfCombine1(ITM_SQUARE, a, aPrec=ATOM)` → `ppfWrapIf(a, 3, PPF_PREC_ATOM=3)`
→ `3 < 3` is false → no parentheses. `ppfBuildRow`'s `PPT_TKBIG` arm pushes the
same ATOM into `stackPrec` (:588), so the PHIST pager inherits it identically.
The keystroke sequence is not hypothetical — the shipped B5 pin already keys
`1 ENTER 5 ENTER 1 SIGMAn <lbl>` and then an operator.

**Measured.** An instrumented probe decoded the capture surface's own tree in
the same build as the VISUAL pins:

| surface | tree |
|---|---|
| VISUAL (`prettyTest.c:4846`, V68, shipped) | `S(P(B(n\|[n = 1]\|3))\|2)` |
| capture / history (probe) | `S(B(P(n)\|[n= 1]\|5)\|2)` |

Same node kind, same shared `ppfCombine1`, opposite bracketing, one binary.

**Contract violated.** The fix's own reasoning, `prettyVisual.c:1049-1060`:

> a big operator is NOT an atom, whatever the function-entry default says. Its
> body is drawn to the RIGHT of the stroke and extends as far as the body goes,
> so a factor or an exponent placed beside it binds INTO the body … The guard
> for stacked powers 40 lines up found this same class and enumerated the two
> OP1 members in front of it instead of the class; that is why the construct
> member shipped.

The named beneficiary is `ppfCombine` — the function `ppfBigop` feeds with
ATOM. And `DESIGN.md:549`, "a third front-end, not a third renderer": one tree,
one bracket decision, all surfaces.

**Honest narrowing.** Only the exponent case is a genuine wrong number. The
`×` half is mathematically vacuous — `Σ P(n)·2` misread as `Σ(P(n)·2)` is the
same value by distributivity, and the right operand comes off the stack so it
can never depend on `n` — and `+`/`−` is left bare in VISUAL too, deliberately.
Nor do two pictures literally collide, because the capture Σ body is always the
fixed text `LBL(n)` and the alternative reading is unproducible on that
surface. What collides is the picture and the value the owner is holding, on
the one operator (`x²`, `x³`, `yˣ`) that the fix was written for. The
refutation that the tall bigop box disambiguates it failed on the layout:
`PP_SUP` sets `exp.relBase = -m->supDrop`, a fixed drop independent of base
height, and `exp.relX = base.width + 1`, so the exponent lands at ordinary
superscript height immediately right of the body run.

**Scope.** `git diff f0edd5ee7..HEAD` shows `prettyFormula.c` untouched by all
four fix commits: this is PP12-era code, not a regression the wave introduced.
It is the fix's class left unclosed at precisely the neighbour axis (a) asks
about. The EQN parser holds the third instance (`ppqFactor` builds a `PP_SUP`
over a bare `PP_BIGOP` from `ppqPrimary`, `prettyEquation.c:578`; `ppqTerm`
the same at :636) — round 1 already named that one as inherited and it is not
re-reported here.

**Bug class.** *A scope rule implemented where it was noticed rather than over
the scope it names* — `DESIGN-HISTORY.md`'s own phrase for the PP18-9 sibling,
written in the same entry, and not extended to either neighbour.

**Class-level test.** One pin per producer of `PP_BIGOP`, asserting the same
bracketing for the same mathematics: VISUAL (V68/V69 exist), a captured Σ
squared read back through `ppfBuildEntry`, and an EQN string `SUM(...)^2`.
Three pins, one expectation. The mutation is `*outPrec = PPF_PREC_ATOM` in
whichever producer is fixed.

---

### R2-3 — the `MVAR` mirror aborts the whole walk on the drawability of a declaration it is not going to use

`packages/pretty-print/prettyVisual.c:547-550`

**What breaks.** Inside the declaration scan the order is: bound the name
(`len == 0 || len >= PPV_NAME_MAX` → `break`), copy it, then
`if(!ppvNameIsDrawable(nm)) { ppvDecline(ctx, PPV_D_NAME); return false; }`,
then the `strcmp(nm, param)` match, then the `first` capture. The drawability
test sits *ahead* of both the match and the `first` capture, and it exits the
whole function rather than continuing the scan.

**Reaching input** (executed):

```
LBL 'VB5'  MVAR 'a1'  MVAR 'x'  RCL 'x'  ENTER  ×  END
LBL 'VD6'  PGMDRV 'VB5'  3  f' 'x'  END
```

`PROBE VD6: DECLINED reason=18 atStep=3`. `ppvNameIsDrawable`
(`prettyVisual.c:293-306`) admits only A-Z/a-z, so any digit qualifies — `a1`,
`R0`, `V0`, `x2` — as does upstream's own step variable `δ_d`, which
`differentiate.c:308` tests for explicitly and which `DESIGN.md`'s own "SET THE
STEP" guidance points users at declaring. Upstream reaches `MVAR 'x'`, matches
`currentSolverVariable` (set to the `f'` parameter at `differentiate.c:187`)
and returns `x`; `DERIV(x×x;x;3)` is fully drawable.

The mirror case reaches it too, and is broader than the finding as first
raised: declare `MVAR 'x'` then `MVAR 'a1'` and call `f'` with a third name.
Upstream returns the first declaration `x`; the walker scans past `x` and then
declines on `a1`, a name that never enters the picture.

**Proof the refusal is the only blocker.** Replacing the abort with
`step = findNextStep(step); continue;` made the probe draw `DERIV(x×x;x;3)`
and the entire gate went green solo *and* combined — every existing pin,
V59-V63 included. Upstream's semantics cost nothing elsewhere in the suite.

**Contract violated.** `prettyVisual.c:509`: "This mirrors that walk, including
REM transparency." It does not. Upstream's loop
(`differentiate.c:308-319`) has exactly two exits, both on a malformed or
over-long declaration; a name it cannot turn into a register leaves `first`
alone and the loop continues. The mirror stops where upstream continues, so
the two answer different questions for a body upstream handles.

The divergence is also self-inconsistent inside one loop: the "unusable
declaration" test three lines above `break`s and falls back to `first`, the
drawability test hard-declines, and only the `break` arm carries a rationale.

**Where the correct fix is not the obvious one.** A bare skip is wrong: when no
later declaration matches, upstream returns the undrawable *first* declaration,
and the walker must still decline D18 for that. The defect is granularity —
skip while searching, decline only on the name actually chosen. That is where
the pre-fix code had the test, via `ppvVarName`.

**Bug class.** A rendering constraint applied to a value the renderer never
sees; equivalently, two hand-mirrored copies of one upstream decision with
nothing forcing their exception paths to agree.

**Class-level test.** 2×2: `{undrawable declaration first, drawable one later}`
× `{a later declaration matches the parameter / none does}`, plus the
undrawable-name-is-the-chosen-one case. Expect draw, draw, decline D18. The
mutation is restoring the abort.

**Intent search came back empty**, which is why this is a finding and not a
ruling: nothing in `DESIGN.md`'s catalog or Rulings block, `DESIGN-HISTORY.md`,
`TESTING.md`, round 1's PP18-1 spec or `006210aad`'s message mentions a
spelling test inside the declaration scan. Every artifact describes only the
selection ("the declaration matching the parameter, else the first declared,
else nothing") and only one decline. Round 1 did rule that `ppvNameIsDrawable`
being digit-strict "declines rather than mis-draws" — that ruling is about
names the walker *spells*.

---

### R2-4 — PP18-9's scan spends the four-name candidate pool on closed sibling scopes, so a fifth disjoint sum declines D12 with nothing to collide with

`packages/pretty-print/prettyVisual.c:669` (the scan), `:674` (the decline)

**What breaks.** `ppvRun` zeroes `astUsed` once per program and never frees;
`ppvAlloc` never dedupes. Every construct a program ever built stays live in
`ctx->ast[0..astUsed]`, including sums long since dropped off the stack. Each
completed Σ permanently burns 1 `PPA_CONSTRUCT` + 8 seeded `PPA_VAR` leaves
(`ppvBody` seeds `PPV_STACK_SLOTS` = 8 frame leaves spelled with the counter
name). `ppvNameUsedInAst` loops over that whole flat arena, matching `PPA_VAR`
text and `PPA_CONSTRUCT` `varOff` alike, so four closed sums consume `n`, `m`,
`k` and `j` and the fifth declines `PPV_D_COLLISION`.

**Reaching input, corrected from the shape first raised.** The naive
5 × (`1 ENTER ENTER Σn`) costs 50 of the 48 arena nodes and declined D16 before
the fix too, so at that shape PP18-9 only swaps the reason code. The drawing is
lost at exactly 48 nodes — refill the stack from one literal and drop each sum
as it closes:

```
LBL 'Z7'  1  ENTER×7
          Σn 'ZNB' DROP    Σn 'ZNB' DROP
          ENTER Σn 'ZNB' DROP
          1 ENTER ENTER Σn 'ZNB' DROP
          1 ENTER ENTER Σn 'ZNB'   END
```

Measured: shipped code `declined D12 at step 24`; with `ppvNameUsedInAst`
stubbed to `return false` the same program draws `SUM(n;n;1;1)`. The rename
half needs no arena pressure at all — two disjoint sums measured
`SUM(n;n;1;1)` and `SUM(m;m;1;1)` where both were `Σₙ` before the fix. (The
same mutation reds V71, confirming it landed on PP18-9 and not something else.)

**Contract violated.** `DESIGN.md:653-657`: "A sum's counter name is invented
(first free of `n`, `m`, `k`, `j`) … and a BODY that recalls a real variable
spelled the same way DECLINES rather than let the invented name shadow it
(V6)." The rule is about SHADOWING. The fix's own comment scopes itself
correctly — `prettyVisual.c:632`, "the limits are inside the operator's visual
scope, so they are part of it" — but the implementation scans the arena, and a
closed sibling scope is in neither the body nor the limits.

**Bug class.** A name reservation implemented as a point-in-time query over an
arena the producer keeps appending to. The arena is not the scope.

**Class-level test.** Two disjoint sums must both draw `Σₙ`; five disjoint sums
must draw; a sum whose LIMIT recalls `n` must decline (V71 has this); a sum
whose BODY recalls `n` must decline (V6 has this). The nested case — five sums
as each other's limits — declines D12 before and after the fix and is correct
by the fix's own rationale; it is not evidence for this finding and must not
be used as the pin.

**Would leave alone.** The rename half. `Σₙ` then `Σₘ` for two disjoint sums is
gratuitous but not wrong, and nobody is misled by it. The false D12 is the part
worth fixing.

---

### R2-5 — V65, the differential oracle three artifacts record as delivered, does not exist

`packages/pretty-print/prettyTest.c:3356-3358`,
`design-docs/pretty-print/DESIGN-HISTORY.md:61`

**What breaks.** Repo-wide, `V65` occurs on five lines from three sources —
`prettyTest.c:3356` and `:3358`, their generated mirrors, and
`DESIGN-HISTORY.md:61`. All five are prose. The pin numbering runs V1…V63 then
jumps to V66; V64 and V65 were never written. `TESTING.md`'s V-family
inventory and its MUT-115..126 table cite V60-V63 and V66-V74 and never V64 or
V65. `git log --all -S'V65'` returns one commit, `190bbb2fd`, which added the
string as prose and enumerated its actual new pins as V68-V74.

**Contract violated.** `prettyTest.c:3356-3358`:

> V18 and V65 close the loop from the other side: they evaluate the walker's
> own output and require it to agree with what the program actually computes.
> V65 is the one that would have caught PP18-1.

`DESIGN-HISTORY.md:61`: "The oracle the report recommended is now V65 … No
expected string appears in it." Commit `006210aad`: "V65 is the oracle the
audit asked for and the one I should have built first." Plus the standing
bug-fix rule — reproducer, named class, class-level test where enumerable.

**No ruling covers it.** This package records deliberate gaps loudly and in a
fixed register: MUT-76 as "UNFALSIFIABLE from the harness — documented gap, not
a coverage hole"; `TESTING.md:451` "Documented gap: the browser's unbuildable-row
branch"; `DESIGN.md:719` "**Documented gap:**"; and this very round carries
three such rulings (the unpinned REM arm, V66's visit count, the MUT-118/119
redundancy). Nothing anywhere defers V64/V65. Two artifacts assert it exists in
the present tense while two others that catalogue pins have never heard of it.

**Aggravating.** The sentence naming V65 was written by the commit fixing
PP18-16, whose stated purpose was that the old header "would have sent the next
reader to the wrong oracle". The correction substituted a new wrong oracle.

**Honest narrowing.** The PP18-1 *class* is not uncovered — V59-V63 pin the 3×2
matrix and MUT-115/116/117 are red-verified against them. What is missing is
the run-the-program-and-compare oracle, so the defect is traceability rather
than a bare coverage hole. But it is the oracle that would have caught R2-1
without anyone reasoning about `MVAR`s, and the tree contains no substitute:
the only `fnEqCalc` round-trips in the VISUAL driver are V18 (fixture `VDBL`, a
double *integral*, against a hand-written `1.33333…`) and V44 (a hand-typed
`"LN(1)+2"`, not walker output at all). No derivative fixture is executed
anywhere in the suite; every DERIV pin is a `strcmp` or a decline code.

**Bug class.** An artifact recorded as delivered that was never written, with
nothing forcing the record and the tree to agree.

**Class-level test.** The oracle itself, and it is cheap: for each of a set of
DERIV / INTEG / SUM fixtures, `execProgram` the label, `fnEqCalc` the walker's
own transpiled drawing, require agreement to tolerance. No expected string. Its
own mutation is any of MUT-115..117.

---

### R2-6 — PP18-5 was fixed at three arms and pinned at one

`packages/pretty-print/prettyVisual.c:847` (`ITM_PGMINT`), `:876`
(`ITM_PGMDRV`)

**What breaks.** Nothing at `HEAD`; the code is correct. The mechanism is
unpinned at two of its three sites. Measured: commenting out `:847` leaves the
gate green solo and combined; commenting out `:876` leaves it green; commenting
out the pinned `:867` (`ITM_XEQ`) reds it immediately (`1/1 testSuite FAIL`,
exit 1). The control run proves the harness is live and the mechanism is
detectable when pinned.

**Reaching input for the unpinned behaviour.** With `:847` deleted:

```
LBL 'VZZ'  RCL 'a'  ENTER  PGMINT 'VHT'  RCL 'b'  +  ×
```

`ENTER` leaves `[a,a]` with `liftDisabled`; `PGMINT` keeps the latch; `RCL b`
overwrites the top → `[a,b]`; `+` → `[a+b]`; `×` underflows into a false D10.
With the line present: `[a,a,b]` → `[a,a+b]` → `a×(a+b)`. Inside a construct
body the failure is worse and silent — the seeded frame supplies a phantom
operand and the picture is simply wrong. `:876` has the same shape with
`PGMDRV 'VDB' / 3 / f' 'x' / + / ×`. Every `PGMINT`/`PGMDRV` fixture in
`prettyTest.c` has the item first after `LBL`/`MVAR` or preceded by `DROP`,
never after an `ENTER` with a live latch; V72 drives `XEQ` only.

**Contract violated.** The fix's own comment, `prettyVisual.c:859-866`: "AUDIT
PP18-5: these three arms return before the epilogue that clears the latch."
`TESTING.md:291` records `MUT-126 | the lift latch survives XEQ again (PP18-5)
| V72` — one mutant for three sites, with no annotation, in a table whose
neighbours (MUT-76, MUT-118, MUT-119) carry explicit bolded rulings when a
non-pin is deliberate. And the standing rule: a class-level test where the
class is enumerable. Here it is, and the fix's own comment enumerates it.

**Bug class.** A fix applied to a class and pinned at one member —
`DESIGN-HISTORY`'s "a guard that enumerated its examples instead of its class",
one level out, in the test rather than the code.

**Class-level test.** V72's fixture shape (`RCL / ENTER / <item> / RCL / + /
×`) instantiated three times, once per arm, each asserting the drawn product
rather than a decline. Mutation: delete each clear in turn; all three must red.

---

### R2-7 — `DESIGN.md`, which is authoritative, still teaches the retracted seeding rule, and its decline catalog stops two numbers short of what the code emits

`design-docs/pretty-print/DESIGN.md:626-640` and `:616-624`

**What breaks, part 1.** `DESIGN.md:626` still reads "`PGMDRV` latches the
program, `f'`/`f"` pop the point and name the variable … The seeding rule is
the integrator's, and that is a measurement rather than an analogy … So a body
frame seeded with the variable name on all levels reproduces what the engine
actually offers a program." That is verbatim the sentence the same range's
`DESIGN-HISTORY` entry confesses as wrong ("I wrote that DERIV's seeding rule
was 'a measured claim, not an analogy' and then measured one call short"), and
`prettyVisual.c:568-579` now states in terms that "`variable` is NOT the `f'`
parameter". Neither `deriv_pgm_variable` nor `ppvDerivVariable` appears
anywhere in `DESIGN.md`. `git log -S` dates the paragraph to `f044f875e`, PP18
proper; the four fix commits left it untouched.

**What breaks, part 2.** The catalog at `:616-624` is a contiguous,
ellipsis-free enumeration ending "D18 name the grammar cannot spell." The code
now emits two more: `PPV_D_DERIVVAR` = 19 (`prettyVisual.c:564`, printed at
:1495 as `step %u: cannot be drawn (D%u)`) and `PPV_D_TOOBIG` = 20
(`prettyVisual.c:1525`, `too large to draw (D20)`). Both are reachable and both
are already pinned — V63 asserts the literal `19`, V67 drives the `D20` arm
with the `VBIG` fixture. `grep -rn 'D19|D20|DERIVVAR|TOOBIG' design-docs/`
returns zero. `prettyVisual.c:39`'s own file banner still asserts "The decline
catalog is D1..D18 (ppvDecline callers)" — and D19 *is* a `ppvDecline` caller,
so the parenthetical does not rescue it.

**Contract violated.** `CLAUDE.md` and `DESIGN.md:4`: DESIGN.md is
authoritative, DESIGN-HISTORY is its non-normative amendment trail scoped to
rejected shapes. `DESIGN.md:616`: "**Decline catalog** (D-numbers reach the
user through `moreInfoOnError`)".

**The refutation that failed.** "DESIGN-HISTORY is the sanctioned channel, so
DESIGN.md need not change" is refuted by the package's own practice:
`DESIGN.md:758` already carries an inline "**AMENDED (audit r1, A8):**" block
from a previous round, and this range's *own* DESIGN.md diff inline-corrects a
different refuted paragraph ("**PP18 note:** the fragment pool, the 256 B
compose buffer and the construct-boundary rollback this paragraph used to
describe are gone"). The wave corrects refuted DESIGN.md prose in DESIGN.md.
These two were missed — and `190bbb2fd` ran a deliberate doc-staleness sweep
(PP18-14/15/16) over this exact file while missing the catalog the same commit
had just outgrown.

**One clause of the finding as raised is dropped.** `DESIGN.md:684-687` still
promises that a formula too tall for the pair "still shows in the window,
linear and centred in it". That is stale, but the linear last resort died at
`55c363ad5` with the text back end, one stage *before* this range; PP18-2
replaced a blank framed screen, not a fallback. Out of subject, recorded so it
is not lost twice.

**Severity bound, honestly.** `defines.h:2497-2500` forces
`EXTRA_INFO_ON_CALC_ERROR` to 0 under `DMCP_BUILD`, so no D-number reaches a
device owner at all; the catalog is a simulator and PC-build artifact. That
binds D1..D18 identically, so it lowers the whole catalog's stakes without
distinguishing the two new entries. The seeding paragraph is the half that
costs something: it is what the next implementer re-derives from, and it
happens to state the rule that is *correct* for R2-1's case, so the owner has
to rule which of the two documents the design.

**Bug class.** A normative document left describing behaviour the code
retracted, by the commit that swept it for exactly that.

**Class-level test.** Not mechanically testable as prose. The enforceable
fragment is a doc-lint asserting that every `PPV_D_*` enumerator appears in
DESIGN.md's catalog; there is no doc-lint in `packages/pretty-print/` today.

---

### R2-8 — the PP18-16 correction was pasted in front of the sentence it was deleting, so `TESTING.md` now states both readings

`design-docs/pretty-print/TESTING.md:524-528`

**What breaks.** The V-family section reads, in one paragraph, four sentences
apart:

> …but since PP18 that string is NOT the product. The product is a node tree …
> AUDIT PP18-16: this section used to say the string was the product, which
> would have sent the next reader to the wrong oracle. **That is deliberate:
> the string is the walker's whole product**, and every rendering question
> about it was already settled by the equation battery, so a pin that checked
> pixels would be testing `ppqParse` a second time and the walker not at all.

**Evidence it is an edit artifact, not a considered retention.** `git log -p`
over the range shows the deletion half of the edit was dropped: the removed
line's tail clause "That is deliberate: the string is the walker's" was
re-appended verbatim to the last inserted line, and continuation line 526 is
unchanged context. The file is hard-wrapped at ~70 columns; line 525 is 114.
`grep "whole product"` returns exactly one live site. `190bbb2fd`'s own message
names the phrase as the deletion target.

**Contract violated.** The commit's claim, in the paragraph itself: "this
section used to say the string was the product". It still does, in the next
clause, in unhedged present tense, and it is the more emphatic of the two — it
carries the argument for not writing node or pixel pins, which is the practice
PP18-4, PP18-12 and PP18-13 were all found by. The sibling header in
`prettyTest.c:3346-3354` was corrected properly, quoting the bad phrase only in
disavowed past tense, so code comment and authoritative doc now disagree and
the authoritative one is wrong. `DESIGN.md:4`: "the test contract lives in
TESTING.md."

**Bug class.** An edit that inserted its replacement without deleting the
original — and the fix commit asserting a correction it did not fully make.

**Class-level test.** None worth writing. This is the finding fixed by reading
the paragraph aloud.

---

## 4. PLAUSIBLE

Survived refutation; nobody constructed the reaching input.

**P1 — `calcDeriv` demotes a non-numeric `MVAR` to `INVALID_VARIABLE` and the
mirror does not.** `differentiate.c:441-443`: `if(variable != INVALID_VARIABLE
&& !getRegisterAsRealQuiet(variable, &probeValue)) variable =
INVALID_VARIABLE;`. `ppvDerivVariable` has no counterpart, so a body declaring
`MVAR 'y'` while `y` holds a string draws `d/dy(…)` for a derivative upstream
takes over the stack instead. This is R2-1's class at a second site and with
the opposite sign. *What would settle it:* store a string into a variable a
body declares as `MVAR`, then run `f'` and VISUAL side by side. Note the
structural obstacle — the walk is static and cannot read runtime register
contents, so this may be an accepted limit rather than an oversight; nothing in
`DESIGN.md` or the new comments records the decision either way, and no pin
covers it.

**P2 — the mirror matches by `strcmp` where upstream matches by resolved
variable id.** Upstream compares `findOrAllocateNamedVariable(name)` against
`currentSolverVariable`; the package compares the raw text. `CMP_NAME` folds
superscript, subscript and struck forms, so two spellings can resolve to one
variable and the two selections diverge. *What would settle it:* a body
declaring an `MVAR` whose name is a sub/sup/struck form of the `f'` parameter's
name, and whether TAM entry can produce that pair. No reader could build it.

**P3 — the name-length bound diverges by one class.** `ppvDerivVariable` breaks
at `len >= PPV_NAME_MAX` (rejects 16+, accepts 15); upstream breaks at
`nameLength > MAX_LABEL_NAME_LENGTH` (rejects 15+, accepts 14). A 15-byte
`MVAR` name is therefore accepted here and skipped there. No overflow —
`sampled[16]`, `first[16]` and `binding[][16]` all take 15+NUL. *What would
settle it:* whether any loadable state or program file can carry a 15-byte
variable name past `boundProgramNameLength`. `manage.c:102` says such a name
"cannot have been produced by the calculator", and alpha entry force-closes at
6 glyphs, so the honest answer is probably no.

---

## 5. Design observations (D7)

Shape, not defects.

**D7-1 — three producers of `PP_BIGOP`, three answers about its precedence.**
`ppvAstToNodes` (fixed by PP18-4, reports ADD), `ppfBigop` (reports ATOM,
R2-2), `ppqPrimary`/`ppqFactor` (structural, no precedence channel at all).
`DESIGN.md`'s law is "a third front-end, not a third renderer", and it holds
for the *renderer*; nothing makes the precedence answer belong to the node
kind. The natural home is `ppfBigop`, which all three could read.

**D7-2 — `layoutFull` is derived state about `prettyLayout.c`'s pool, stored in
`prettyVisual.c`'s per-walk context.** `ppReset()` owns the source of truth and
empties the pool, but cannot clear the latch, so the obligation falls on every
caller by convention. Two of three consumers clear it beside their `ppReset()`
(`:1376`, `:1411`); the third relies on `ppvRun:1089` having cleared it once.
The failure needs a fourth caller or a third rung that resets the pool without
the neighbouring assignment, and then `ppvAstToNodes` returns `PP_NONE` at
`:953` for a pool that is empty — a "too large to draw (D20)" for a tree that
fits. UNREACHED today; nothing in either signature says the pairing exists.
`ctx->layoutVisits` one field below has the same shape: reset per walk,
cumulative across the up-to-four passes the product makes, and read by V66
after exactly one.

**D7-3 — `ppvDerivVariable` is a hand copy of a `static` upstream function.**
The copy is the right shape and I would not change it: upstream's version calls
`findOrAllocateNamedVariable`, i.e. it *allocates* named variables as a side
effect, so an exported wrapper would make merely drawing a formula mutate
variable storage. But nothing forces the two to agree, and this round found
four divergences in forty lines — R2-1 (the decline where upstream reads the
stack), R2-3 (abort vs skip), P2 (text vs id) and P3 (the length bound). A
comment claiming "This mirrors that walk" is the only thing holding them
together.

**D7-4 — the decline vocabulary is bimodal and undocumented.** Some rejections
`break` and fall back to a previous candidate; others abort the entire walk.
Two of them sit three lines apart in one loop (`prettyVisual.c:541` and `:547`)
and only the first carries a rationale. That ambiguity is exactly R2-3.

**D7-5 — two decline numbers are not in the decline mechanism.**
`PPV_D_TOOBIG` sits in the `ppvDecline` reason enum but is never passed to
`ppvDecline`; it only interpolates into a `sprintf` at `:1525`. So
`ctx.declineReason` can never hold 20 and no `ppvTestDecline` pin can ever
assert it. Arguably right — the too-big condition is a paint-time failure after
the walk succeeded, where the walk's decline/step machinery has nothing to
record — but it will confuse whoever extends the catalog. Separately
`PPV_D_FRAGMENT` (D15) went dead when the fragment pool left with the text back
end.

**D7-6 — no D-number reaches a device owner.** `defines.h:2497-2500` forces
`EXTRA_INFO_ON_CALC_ERROR` to 0 under `DMCP_BUILD` (and `:657-658` under
`TESTSUITE_BUILD`), so the whole catalog is a GTK-simulator artifact. Worth
ruling explicitly in `DESIGN.md`, because two findings here and one design
paragraph rest on "D-numbers reach the user through `moreInfoOnError`", and on
the shipped R47 they do not. On a DM42n the owner sees a bare
`ERROR_INVALID_DATA_TYPE_FOR_OP` and has no way at all to tell D19 from D10.

**D7-7 — `ppvNameIsDrawable`'s comment is the spec the mirror was written
against, and it overstates the body.** The comment at `:291` claims "and
subscript digits after the first"; the implementation admits A-Z/a-z only. Any
reasoning about which names decline, including round 1's ruling that the
predicate "declines rather than mis-draws", was done against the comment.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved.

### Killed by the refutation pass

**PP18-9's scan looks only backwards, so an `RCL 'n'` *after* the Σ is not
checked** (raised by two dimensions, one of them at high confidence). The design
ruled on exactly this residual and ruled the other way. The shadowing rule is
scoped to the construct's own visual scope: `DESIGN.md:653-657` says "a BODY
that recalls a real variable spelled the same way DECLINES", and the fix's
comment extends it only to the limits. Both halves are covered exactly and a
backwards scan is sufficient for both by construction — the limits are popped
off the stack before the counter is chosen, so they are always in the prefix,
and the body is guarded not by the AST scan at all but by the live
`bindingSynth` collision check in the `ITM_RCL` arm (`:809-815`) while
`ppvBody`'s frame is open. The surviving surface is coincident with a stated
ruling: `prettyVisual.c:1051-1060` says reporting `PPF_PREC_ADD` "leaves it bare
as the left operand of a `+`, which is the one place convention already scopes
it". At the `+` the sum has ended by convention and there is nothing to shadow.
"The drawn formula is the whole final tree" is the finder's reading, not the
design's.

**PP18-3 latched the layout walker but not the text back end**, which would
re-expand the same DAG 2^k. There is no path to it. `ppvSerialize` /
`ppvTranspile` live inside `#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)`
and a repo-wide grep finds exactly three callers, all in `prettyTest.c`, all
per-named-label — no sweep-all-labels loop and no device call site. The DAG
fixture `VXP` is consumed only by V66 through `ppvTestBuildNodes`; no pin
transpiles it. Measured rather than argued: instrumenting `ppvSerialize` with a
call counter and running the solo gate shows every transpile in the whole
battery entering it at most 12 times, the heaviest DAG actually serialized
being a single `ENTER`-dup at 9 calls. "The two back ends disagree about a
hazard" is a maintainability note, not a defect with an input.

**Full-screen vertical centring is one row high, and at the exact height cap it
puts ink on the frame rule.** The arithmetic is right and was verified
numerically (`h=147` → code top 20, bottom 166; correct is 21/167; the stack
window stays inside 20..91 for every admitted `h`). Three things kill it. It is
not in the range: the base expression and the fit cap are unchanged *context*
lines, and `git log -L` dates them to `55c363ad5`. It answers axis (a)
backwards: `(21 + 167 - h)/2 + ascent` with cap `> 167 - 21 + 1` is the
package's three-site full-screen idiom, byte-identical in `prettyValue.c:870`
(SHOW's big X, PP2) and `prettyEquation.c:904` (EQSHW, PP7), both shipped and
screenshot-verified — VISUAL copied its neighbours verbatim rather than
disturbing them. And the only visible case is unwitnessed: the suite's measured
heights (V28's 38/58/78 and 31/51/71; EQ22 pinned to a band) point nowhere near
147, and every other odd `h` is the 1-pixel offset the finder itself calls
invisible.

**PP18-10's `step` guard shipped with no pin and no MUT row.** The factual half
is true and was proved by mutation: delete the `(a->child[3] != PPV_NIL &&
step == PP_NONE)` conjunct and the gate stays green, and `TESTING.md:280-291`
has no PP18-10 row. The conclusion that gives it severity is wrong on its own
path. With a deep step subtree exhausting the 72-slot pool, the *unfixed* code
does not draw a step-less sum: `varTiny = ppNewRun(...)` returns `PP_NONE` from
the same exhausted pool, and `ppqBuildBigop`'s SUM/PROD arm checks `big ||
under || eqRun || varTiny || toN == PP_NONE` eleven lines before it reaches the
`if(stepN != PP_NONE)` line the finding cited. The result is the same clean
decline. PP18-10 is defensive symmetry with no observable behaviour on any path
the walker can construct through the node pool, which is why the standing rule
("a class-level test *where the class is enumerable*") does not demand a pin.
One residual, recorded and not claimed: `ppNewRun` has a second failure mode,
`ppTextLen + len + 1 > PP_TEXT_BYTES` (512), which could fail a long step run
while node slots remain — but that needs the text arena to exhaust before the
node arena, i.e. a mean run length above ~7 bytes across the whole tree, and it
is not the path anyone constructed.

### Cleared by the finders

**The strongest near-miss, found independently by four dimensions and retracted
by all four.** `items.c` marks `ITM_LBL` (row 1819) and `ITM_REM` (row 3406)
`SLS_ENABLED`, while the walker groups both with the `SLS_UNCHANGED` items at
`prettyVisual.c:729` under "no stack, no picture, and no effect on the pending
lift either". By PP18-5's own stated rule that is the same latch bug at two more
sites, and `RCL a / ENTER / REM / RCL b / + / ×` would draw the wrong picture.
It is not a bug: `executeOneStep` never calls `runFunction` for either —
`PTP_DECLARE_LABEL` returns at `lblGtoXeq.c:827` and `PTP_REM` at `:866` — so
the `SLS` epilogue at `items.c:605` never runs and the bit is dead for both.
The walker matches the machine, not the table. Two true facts, wrong conclusion,
caught by tracing the dispatch rather than trusting the row.

**PP18-5 completeness elsewhere, by enumeration rather than by spot check.**
Every `return` in `ppvStep` that skips the epilogue: the declaration arm
(above), `ENTER` (sets the latch deliberately), `ITM_LITERAL` and `ITM_RCL`
(both delegate to `ppvPushLifting`, which owns and consumes the latch exactly
once), and the three fixed arms. Everything else `break`s into the epilogue.
`DROP`, `DROPY`, `FILL`, `XexY`, `STO`, `RCL` are `SLS_ENABLED` and reach it.
`MVAR`/`PAUSE`/`SNAP`/`NULL` are `SLS_UNCHANGED`. `RTN` is `SLS_UNCHANGED`, and
`ppvWalk`'s `break` on it preserves the latch, which matches. `CLX` is
`SLS_DISABLED` but the walker does not implement it at all, so it declines D1
and the divergence is unreachable.

**The `layoutFull` latch is incomplete and still sound.** Five of roughly
twelve `PP_NONE` exits set it; `ppfWrapIf` (`:1035`), `ppfCombine1`/`2` and
`ppqBuildBigop` do not. Every unlatched `PP_NONE` is observed one frame up by a
parent OP1/OP2/CONSTRUCT arm that *does* latch, so the doubling survives at
most one level. Independently: every successful visit consumes at least one node
of the 72-node pool, so a DAG cannot reach 2^k visits without hitting `PP_NONE`
first. V66's 500 is a real bound, not a tuned number.

**The `PPA_CONSTRUCT` arm runs all four child recursions before testing any of
them** — the shape PP18-3 had to fix in the OP2 arm. Cleared on the same
argument: once anything below latches, every later recursion returns at the
entry test, and the only unlatched producers are leaves, which cannot double.

**`ctx->ast[a->child[0]].kind` at `:1032` indexes a 48-entry array with a child
index that is `0xFF` when absent.** The `body == PP_NONE` return four lines
above fires first and `ppvAstToNodes` returns `PP_NONE` for `PPV_NIL`, so
`child[0]` is provably in range.

**`varTiny` / `varCtx` are passed to `ppqBuildBigop` unchecked** — PP18-10's
class at a second site, apparently. `ppqBuildBigop` validates exactly the ones
each kind uses (SUM/PROD: `varTiny` + `toN`; INTEG: `varCtx` + `toN`; DERIV:
`varCtx` + `varTiny` + `fromN`) and returns `PP_NONE`, for the reason its own
header gives ("`ppAppendChild` silently no-ops on `PP_NONE` … audit R4-3").

**PP18-4's `PPF_PREC_ADD` checked at every `ppfCombine` arm that reads
`aPrec`,** not just the two the comment names. Wraps under `MULT`, `SUB`-right,
`YX` base, `SQUARE`/`CUBE` base, `1ONX`, `CHS`. Inert where the geometry already
scopes: `DIV` (PP_FRAC), the radicals (vinculum), `ABS` (bars), `XTHROOT`,
`LOGXY`, both function forms. Two comment imprecisions, no code defect: `/`
never brackets, so "brackets it under ×, / and ^" overstates by one operator;
and ADD leaves the construct bare on *both* sides, not only the left, which is
right because `1 + Σ…` is unambiguous.

**PP18-7's widening is complete.** `textOff` was already `uint16_t`; `varOff`
was the only 8-bit index into the 512-byte pool. `varLen`/`textLen` stay
`uint8_t` correctly — both arrive through a one-byte length prefix, and
`PPV_NAME_MAX` is 16. `child[]` entries are AST indices under 48 with
`PPV_NIL` = 0xFF outside that range; every counter is against a cap of 48/8/5.
No remaining truncating cast, and `ppvNameUsedInAst` plus both `ppNewRun` call
sites read the widened field consistently.

**`ppvBody` leaks `bindingCount` on all five of its failure paths** — the
decrement at `:440` is success-only. Every one of those paths sets
`ctx->failed` first, `ppvWalk` and `ppvStep` both return immediately on it, and
`ppvRun` returns `PPV_NIL`. The leaked binding is never read.

**PP18-2's screen lifecycle, traced for a partial paint before a false
return.** `ppvPaintStackWindow` returns false only at `:1379` or by exhausting
both rungs at the fit test, both before `ppvClearBand`; `ppvPaintFullScreen`
only at `:1414` or by exhausting both rungs, both before the `lcd_fill_rect` at
`:1431`. So "Nothing has been painted, so X still shows what the program
returned" holds on every exit, on both surfaces. `ppvPaintStackWindow` is
therefore not a second PP18-2 site. `screenHoldsDrawnPixels`,
`SCRUPD_MANUAL_*` and `TI_SHOWNOTHING` are set only where a drawing landed.
`currentSolverStatus` is restored on every path that saved it; the older
decline path returns before the save.

**The D20 error cannot be swallowed by a previous VISUAL's held screen.** This
is the composition risk I most expected to find. An error raised while
`SCRUPD_MANUAL_STACK` is still set would suppress the register-line refresh
(`screen.c:5917`) — except that `displayCalcErrorMessage` sets
`screenUpdatingMode = SCRUPD_AUTO` (`error.c:298`) before painting.

**A VISUAL that errors cannot destroy the owner's in-progress capture
formula.** VISUAL is `US_UNCHANGED` (`packages/pretty-print/items.c:2831`), so
`ppcClassify` never stages it and `prettyNoteFunctionDone` returns at its
`!ppcStage.valid` guard before the arm that calls `ppcInvalidate` on a nonzero
`lastErrorCode`. True for D19, D20 and every older decline.

**The new D20 error screen versus `DESIGN.md` §1's "Overflow is never an error
screen".** That rule governs surfaces with an upstream arm to fall through to
(the register line, PSHOW's value rendering). VISUAL has none, and its catalog
already routes D15 and D16 to `moreInfoOnError`. Raising an error for "neither
surface fits" is consistent with VISUAL's contract even though it contradicts
the value renderer's.

**`ppvDerivVariable`'s REM while-loop has no non-advancing-step guard**, unlike
`ppvWalk`'s D13 check. It mirrors upstream byte for byte and adds a NULL test
upstream does not have; spinning needs program memory `findNextStep` already
refuses to walk. Its missing `isAtEndOfProgram` guard likewise mirrors upstream
and terminates on `END`, which is neither REM nor MVAR. Its dead
`bodyIdx >= numberOfLabels` check is unfalsifiable (`ppvLabelIndex` already
rejects that) but mirrors upstream's own bounds check at `differentiate.c:290`.

**`ppvDerivVariable` copying names with `xcopy` rather than
`getStringLabelOrVariableName` is correct and deliberate** — it runs after
`ppvVarName` has read `tmpStringLabelOrVariableName`, and using the shared
buffer would clobber the parameter it is comparing against.

**Decline priority changed in `ppvDerivative`:** `ppvDerivVariable` now runs
before `ppvPop`, so a derivative with an empty stack reports D19 or D18 instead
of D10. Both refuse, the walk aborts identically, only the number differs.

**The nested-construct bracket exemption's rationale is INTEG-specific** ("the
outer construct's own ` d<var>` terminates it") but is applied to SUM and DERIV
bodies too. No wrong reading exists: Σ under Σ and ∫ Σ b dx are conventional
unbracketed notation, and `ppqBuildBigop`'s DERIV arm wraps its body in an
unconditional `PP_PAREN` anyway — which is PP18-6, ruled leave-alone by round 1.

**V67's pixel oracle**, which reads vacuous and is not. The before-sum is over
rows [132,152] and the after-sum over [21,167] — different bands. But
`LCD_SET_VALUE` is 0 and `lcd_fill_rect` maps 0 to `BLT_ANDN`, so the
full-screen prefill clears every bit and the only ink is a 40×12 rect at
(100,132), inside both bands. The pre-fix clear erases it and drives the
after-sum to zero. It fires. V67's missing `INVALID_VARIABLE` guard is also
harmless: 2199 vs `FIRST_LABEL` 2200 means a vanished `VBIG` raises
`ERROR_OUT_OF_RANGE` and fails the pin's first assertion loudly.

**V73 asserts the field the paint pass actually reads.** `ppqBuildBigop` takes
both `kind` and `tag`, so the pin looked like it might assert a field nothing
consumes; `prettyLayout.c:709-712` switches on `nd->textOff` to choose the
integral stroke, the Π or the Σ, and `ppSetBoxTag` writes exactly that field.
The `firstChild`-only descent reaches the outermost `PP_BIGOP` in all three
fixtures and fails loudly on `PP_NONE`. V74's `ppTreeHasRun` guards its
`ppTextAt` behind the kind test, so no `PP_BIGOP` tag is ever decoded as an
offset.

**V70's fixture really reaches an offset above 255** — 22 distinct 12-byte
names intern 264 bytes with no dedup, so `'w'` lands at 266 and a `uint8_t`
truncation prints a character from the first name. Discriminating. V59 and V61
are declared controls and are correctly labelled as such.

**PP18-11's `fopen` guard closes `in`, fails the case by name and skips the
load;** the trailing `outF = NULL;` is dead but harmless and the moved brace
keeps the load/assert block inside the success arm. `ppfTestExpect`'s silent
192-byte signature truncation produces a mismatch, not a pass.
`prettyTestReal`'s re-anchoring claim is accurate against
`testSuiteList.txt:505-515`. The two `errorMessage` `sprintf` sites are bounded
at roughly 33 bytes.

**Conservative-by-design, not defects.** `ppvNameUsedInAst` false-positives on
`n`/`m`/`k`/`j` used as ordinary variables anywhere in the walk, costing a D12
where a different letter would have done — that is decline-biased house style
and it fails safe. (R2-4 is not this: R2-4 is the pool being exhausted by names
nothing could have collided with.)

**Not re-reported because round 1 owns it:** the EQN parser's identical
bigop-precedence hole (`ppqFactor` at `prettyEquation.c:578`, `ppqTerm` at
`:636`). Round 1's PP18-4 names it explicitly — "the equation parser has the
identical hole … PP18 inherits the defect rather than introducing it" — and
this wave deliberately fixed only the walker. One reader raised it fresh and
withdrew it for the same reason, plus an honest second: the drawn exponent
lands beside the operator's upper limit rather than on the body, so the "two
equations, one picture" claim does not hold as cleanly there as on the capture
surface. The capture site (R2-2) is a different matter — it goes through the
same `outPrec` channel the fix corrected and appears nowhere in round 1.

**Not re-litigated, per the tasking:** MUT-118/119 surviving alone by design;
V66 asserting a visit count rather than a wall-clock time; the unpinned
REM-transparency arm as a documented gap; PP18-6's doubled parentheses ruled
leave-alone by round 1; the gate, warnings and `cmp` evidence; MUT-115..126
red-verified.

---

## 7. Verdict

**Would I ship this? Yes, with one blocker.**

R2-1 must be re-ruled before the stage goes out, because the wave traded a
wrong picture for a wrong refusal and the refusal hits the more common program.
Everything else in the four commits either holds or can ship with the finding
open. The central moves are sound and I could not break them: the exponential
is genuinely latched and the latch's incompleteness is genuinely harmless, the
full-screen clear genuinely waits for the fit and no path paints before it
knows, the lift latch is cleared exactly where upstream's epilogue clears it and
nowhere it should not be, `varOff` was the only member of its class, and the
DBLINT drawing is unmoved.

**Axis (a) comes back clean, mechanically.** No fix altered a shared builder's
behaviour for its existing callers, because no fix altered a shared builder at
all. What the fixes did to the neighbours was leave one behind, and that is
R2-2: the precedence answer PP18-4 established lives in one of three producers
of the same node, so the same sum draws bracketed in VISUAL and bare in PHIST,
in one build.

**Axis (b) does not come back clean.** Of the three refusals the wave added or
touched, one is honest (D20 — the tree really does not fit either surface and
nothing is painted), and two refuse things they should draw. D19 refuses the
ordinary stack-consuming derivative body on a premise upstream contradicts in
its own comments and its own control flow. D18 refuses on the spelling of a
declaration that never enters the picture.

**Where would it break first?** On an owner writing the plainest possible
derivative: a function program that takes its argument off the stack, which is
what every appnote-22 integrand looks like. `XEQ` gives the right number and
`VISUAL` gives an error with a number nobody can look up, on a device where the
number is compiled out anyway.

**What I would leave alone if the goal were correct code rather than a clean
audit.** R2-4's rename half — two disjoint sums drawing Σₙ and Σₘ is gratuitous,
not wrong. Most of R2-7's catalog half — D19 and D20 should be written down, but
no device owner ever sees a D-number, so the paragraph that matters is the
seeding rule, not the list. P1, P2 and P3, which are real divergences from
upstream that nobody can reach. And R2-8 is a one-line deletion, not a design
question. That leaves four things worth an engineer's afternoon: R2-1, R2-2,
R2-3, and writing V65.

**The pattern worth naming.** Five of eight findings are in code this wave
wrote, and the two worst are the two bug classes round 1 named in its own
report — *the only fixture satisfies the assumption the code never checks*, and
*a guard that enumerates its examples instead of its class* — recurring inside
the fixes for them. That is the recorded fix-regression rate holding, and it is
the argument for the standing rule that a fix ships with a class-level test:
V65, the one test that would have broken the pattern, is the one that was
recorded as delivered and never written.

---

## 8. Round and exit state

**Round 2** of the PP18 audit, over the fix commits rather than the stage.
Eight finder dimensions (contracts, lifecycle, arithmetic, error paths, guards,
tests, design, upstream) ran blind to each other; every raised finding went to
an independent refutation pass with one assigned lens (reachability,
correctness, intent), with instructions to default to REFUTED on a real path
with a wrong conclusion.

**Counts.** Seventeen findings raised and survived refutation, eight after
deduplication across dimensions. Four refuted. Three left PLAUSIBLE beyond the
verification cap. Five of the eight confirmed were reached independently by two
to four readers; R2-1 by four, R2-3 by four, R2-7 and R2-8 by four each.

**Exit criterion: not met.** A round that finds a functional regression in the
previous round's worst fix is not an exit round. Round 3 should run over the
R2 fixes with the axis rotated again — the natural next axis is *the rulings*:
does the code do what `DESIGN.md` says, statement by statement, in both
directions. Three findings this round are code-and-document divergence, two of
them introduced by a commit whose stated purpose was to sweep exactly that, and
the mechanical form of the question (every `PPV_D_*` in the catalog, every
`Rulings.` bullet against its implementation) has never been run.

**Before round 3, close two process items surfaced here.** The verifier
worktrees spawn at a stale ref and every reader has to notice; a spawn that
checked `git merge-base --is-ancestor` against the subject base would have
turned a silent wrong-tree audit into a loud failure. And
`packages/forth-core/build-test.sh` does not refresh pretty-print, so a
mutation under the wrong gate reports green without ever compiling — which
would have silently invalidated three of this round's mutation-backed verdicts
had the readers not checked for their markers in `files/` and in the build
shadow.
