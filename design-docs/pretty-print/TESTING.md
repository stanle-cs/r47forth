# Pretty-print + pretty-print-extra: testing

This file is the test contract for BOTH halves of the PP19 split. The
pins grew as one battery across eighteen stages and nine audit rounds.
They share one scaffolding (the `ppTest*` helpers, exported from the
core's prettyTest.c), and they run in one suite. A split of the
registry only breaks its cross-references.

Both packages are tested in upstream harness 2 (`src/testSuite/`), the
same way undo-history is: coverage drivers registered in
`funcTestNoParam[]` with `coverageDriver = 1` (`In:/Func:/Out:` blocks).
Each driver writes its failure count into X as a long integer
(`Out: EC=0 RX=LonI:"0"`). All drivers build under
`#if defined(PC_BUILD)` only: test code never reaches the device.

Driver-to-package map since PP19:

| package | driver file | drivers | test scripts |
|---|---|---|---|
| pretty-print | `prettyTest.c` | Measure, Pixels, Fallback, Show, Equation, Visual, Real | `tests/pretty_print.txt`, `tests/pretty_visual_real.txt` |
| pretty-print-extra | `prettyExtraTest.c` | Capture, Formula, EqLang | `tests/pretty_extra.txt` |

The gates run refresh → configure → build → testSuite per pass, and
each pass requires the exit status AND the success banner:

- `./packages/pretty-print/build-test.sh`: solo, trio (forth-core +
  undo-history + core), full (all four packages).
- `./packages/pretty-print-extra/build-test.sh`: pair (core + extra),
  full. There is no solo pass: the package cannot link alone, by
  design.

An edit to the flat working area is invisible until
`tools/pkg_patch_refresh.py` runs. The gate scripts do this first,
always.

## Drivers (stage PP1)

- **`prettyTestMeasure`**, engine-level battery against `ppNewRun`/
  `ppNewBox`/`ppMeasure`/`ppParseFraction` (internal API, prettyInternal.h):
  - M1: `FRAC(3,4)`, numeric context, standard children → exact
    `width 12, ascent 25, descent 5` (pins the metric derivation against
    font drift: 13 = −barTopRel(11) + fracGap(2), + digit ink 12).
  - M2: `HBOX[RUN("1",numeric), FRAC]` → `ascent 26, descent 5, width 28`.
  - M3: parser round-trip on a hand-built improper string
    (sup-minus, sup-3, `/`, sub-4) → structure `HBOX[RUN("-"),
    FRAC(RUN("3"), RUN("4"))]`, digit runs mapped to plain digits.
  - M4: parser rejects: plain `3/4` (no sup digits), empty string, two
    slashes, sup digit after the slash.
- **`prettyTestPixels`**, the real inline path: X = 0.75, `FLAG_FRACT` set,
  `CM_NORMAL`/`TI_NO_INFO`, band cleared, `refreshRegisterLine(REGISTER_X)`.
  X-line geometry (baseY 132, baseline B 160, fraction x-span 388..399):
  - P1 (bar, exact rows): rows 149 and 150 lit across 388..399. Rows 148
    and 151 clear in that span.
  - P2 (numerator strictly above): gap row 147 fully clear in-span. Digit
    ink present in rows 135..146.
  - P3 (denominator strictly below): gap row 152 fully clear in-span. Digit
    ink present in rows 153..164.
  - P5 (toggle-off control): same state with the package disabled → row 149
    is NOT a full lit run (upstream's diagonal rendering differs).
- **`prettyTestFallback`**, identity pins for "pretty declines":
  - F1: X = string (unsupported type): band bitmap with package enabled ==
    band bitmap disabled, bit for bit.
  - F2: plain real (`FLAG_FRACT` and `FLAG_IRFRAC` both clear, no exponent
    form): same identity.
  - F3: `displayValueX` parity: fraction rendered pretty vs upstream:
    `displayValueX` strings byte-identical (pins the builder-first rule).

### PP2 additions

- `prettyTestMeasure`: M5 radical ascent 29/descent 0 (numeric radicand).
  M6 exponent split (base text `1.5·10`, exponent `40`, SUP node). M7
  IRFRAC `√3/2` → RAD inside a FRAC numerator, denominator `2`. M8
  `3×π/4` accepted, paren-power `(π²)` and bare `π` decline.
- `prettyTestPixels` P4: X = √2 (34-digit) with IRFRAC on. Vinculum rows
  131-132 (baseline 160, numeric radicand top 134, radGap 1, vincThick 2),
  gap row 133 clear and radicand ink present, both probed in the
  radicand's columns only (the sign's own diagonal legitimately crosses
  the gap row on the left).
- **`prettyTestShow`**, PSHOW: frame lines rows 20/168 full width.
  Centered stacked 3/4 at the numeric/numeric rung (bar rows 93-94,
  midpoint within 40 px of center). `SCRUPD_MANUAL_STACK` +
  `screenHoldsDrawnPixels` armed. String X falls back to the ordinary
  SHOW (temporaryInformation leaves TI_NO_INFO).

### PP5 additions

- **`prettyTestEquation`**, the EQN strip grammar: EQ1 `1/X+2` keeps
  '/' bound to factors (`[F(1|X) + 2]`). EQ2/EQ3 parens unwrap under a
  bar or vinculum. EQ4 strict declines (no-fraction expressions, dangling
  operators, ellipsis-truncated strings, glyphs outside the grammar). EQ5
  `prettyTryEquation("1/X", 1)` paints the bar at row 182 inside the
  equation's own 23 px strip row and nothing below it.

### PP4 additions

- **`prettyTestFormula`** FV6 (stress, post-PP5): the three-level
  continued fraction 1/(2+3/(4+5/6)) keyed through the real paths must
  appear in the pager. This pins the variable-height packing and the
  whole-tree tiny re-font (both were missing, and FV6 was red first).
  The packing rewrite turned FV5 red once: it wiped the frame line with
  glyph-box pre-clears, hence the 4 px band inset.
- **`prettyTestFormula`**, layout-signature pins over the infix
  renderer (RUN text / `[hbox]` / `F(a|b)` / `S(a|b)` / `R(a)` / `P(a)`,
  expectations built from catalog names): FV1 `(2+3)×4` keeps its
  parens. FV2 `6÷(2+3)` becomes `F(6|[2+3])` with unparenthesized
  children. FV3 a history token stream decodes to `[[2+3] = 5]`. FV4
  `√2` then `x²` gives `S(R(2)|2)`. FV5 the PHIST pager paints its
  frames, holds content ink, arms the manual-paint protocol, and PCLR
  empties the ring. Note for mutation runs: FAIL detail lines can carry
  raw glyph bytes (×), and these flip grep into binary mode. Trust the
  pass/fail counts.

### PP3 additions

- **`prettyTestCapture`**: sixteen traces through the REAL interactive
  paths. Digits go via `addItemToNimBuffer`, which opens NIM itself from
  CM_NORMAL. Operators close NIM at the addItemToNimBuffer tail before
  `runFunction`, exactly the press/release split. The traces: T1
  chaining, T2
  supersession (`2+3=5` emitted, `5+6` current), T3 monadic through the
  funnel, T4 ENTER dup (deep-copy independence: caught the aux/EMITTED
  length-truncation bug on first run), T5 chain through monadic, T6 CLX
  displacement, T7 as-typed `2.50`, T8 NIM abort (deferred lift → no
  ghost), T9 swap order, T10 UNDO discard, T11 DONE-on-error, T12
  arena-exhaustion truthful recovery (`# 1 + 1 +`), T13 LASTx as value
  leaf, T14 unknown-item default (MIN emits then invalidates), T15 eRPN
  ENTER (driver mimics `nimWhenButtonPressed`: keyboard-owned, false in
  a driver, so the eRPN condition needs the mimic), T16 abort with
  ASLIFT set (separates deferred-lift from lift-at-open designs).
  Expected signatures build from `indexOfItems[].itemCatalogName` at
  runtime so name changes never turn them red.

  **Signature alphabet** (postfix): a literal prints its own text, `#` a
  PPN_VAL value leaf, `~` the PPC_UNKNOWN sentinel, `!` a PPN_OPAQUE
  node, `?` PPC_NIL or an out-of-range index, `R<nn>` a register
  reference, `{a,b}NAME` a big operator, and `-` an EMPTY signature. An
  EMPTY signature means the display path WITHHELD the formula, not that
  nothing happened. Audit R4-2 split `~` out of `#`: the two shared one
  character, so no pin was able to tell a truthful value leaf from the
  unknown sentinel. That is exactly the distinction the binding
  invariant turns on. When the split landed, all three existing `#`
  expectations stayed green. This proves that none of them silently
  accepted an UNKNOWN.

  A signature pin that expects `-` asserts a real outcome. A pin that
  merely checks that some substring is ABSENT does not. An absent
  substring is satisfied by `-`, so such a pin passes just as well when
  capture no longer works at all. Audit R4-1 converted T23 and T26 from
  absence checks to exact-signature assertions for this reason. The same
  audit also rewrote T21: its every assertion sat behind a guard that
  `ppcCurrentFormulaRoot()` made false by construction. Before the
  rewrite, if ITM_RCLADD was removed from the classifier entirely, T21
  still passed. Where a pin needs to see through that screen,
  `ppcTestCurrentRaw()` returns the root before it.

### PP12 additions

- **`prettyTestCapture` B-traces**, the big-operator family, driven
  through a real loaded program (`ppcTestWriteAndLoadPgm`, copy-adapted
  from testSuite.c's `covWriteAndLoadPgm`, label `P`, body `x²`):
  B1 Σₙ over 1..10 (sig `{#,#}Σₙ`, X pinned = 385 as real34),
  B3 CLX displacement emits the bare BIGOP root,
  B4 history decode pins limit ORDER, label-name decode and node shape
  (`B(P(n)|[n= 1]|10)`), B5 the result chains (`{#,#}Σₙ 2 ×`, one
  emission), B6 the ∫ currency that actually integrates (PGMINT
  preselects the program, and the INTEGRAL_YX param is the integration
  VARIABLE) with X pinned ≈ 1/3, B6b the label-param SETUP form must
  not mint a node (sig `-`: it consumed X,Y with no result to vouch
  for), B7 a non-unit step is visible (`n=1,Δ2.`: the step travels as
  real34, upstream's real marker included), B8 pixel pin on the
  stroke-drawn Σ in the operator column only (a wider probe let the
  body ink mask a stroke deletion, so the first run stayed green).
  The traces restore `currentSolverStatus/Program/Variable` and
  `currentMvarLabel`: the ∫/Σ dispatches retarget the solver, and
  later suite files (deriv_cov) assume the status they inherited.

### PP13 additions

- **`prettyTestEquation` EQ10-EQ13**, the solver-surface frames, tested
  structurally through the exported builders (`ppqFrameIntegral` /
  `ppqFrameDerivative`) plus the existing EQ8 pixel pin for the render
  path: EQ10 interactive integrate shows the REAL limits and d<var>
  (`B([F(1|X) dX]|0.|1.)`), EQ11 without INTERACTIVE falls back to the
  bare stroke ∫ (`I(F(1|X))`: the walker gained a PP_INT arm), EQ12
  d/dX with the variable name decoded live, EQ13 d²/dX² carries the
  superscript-2 glyphs. Solve framing (f(x)=0) was SKIPPED and the
  reason recorded: `SOLVER_STATUS_EQUATION_SOLVER` is the zero value,
  so a stale INTERACTIVE bit is indistinguishable from a live session.

### PP14 additions

- **`prettyTestEquation` EQ14-EQ21**, the equation-language constructs,
  evaluated through the REAL fnEqCalc path: EQ14 `SUM(X^2;X;1;10)` = 385
  AND the bound variable comes back holding its prior 99. EQ15
  `PROD(X;X;1;5)` = 120 (seed one). EQ16 `DERIV(X^3;X;2)` = 12 and
  `DERIV(X^3;X;3;2)` = 18, EXACT, because the delegate runs the same
  engine deriv_cov pins. EQ17 `INTEG(X^2;X;0;1)` ≈ 1/3 (the
  double-exponential integrator itself). EQ18 the nested
  `SUM(SUM(Y;Y;1;X);X;1;3)` = 10 (argument slicing honours paren
  depth). EQ19 a wrong argument count raises the equation's own error.
  EQ20/21 the 2D shapes (`B([X ²]|[X = 1]|10)`,
  `[F(d|[d X]) U(P(X)|[X = 2])]`).

## Mutation pins (run each separately, battery green between restores)

All six demonstrated red on 2026-08-26 (targeted single-file battery), green
after restore.

| id | mutation | went red |
|---|---|---|
| MUT-1 | delete the `lcd_fill_rect` bar paint in `ppPaint`'s FRAC arm | P1 |
| MUT-2 | shift the bar two rows (barTopRel + 2) | M1, M2, P1, P2, P3 |
| MUT-3 | swap the numerator/denominator relBase formulas | M1, M2. NOT the P-pins: P2/P3 assert ink presence per band and are shape-blind to which digit sits where. The measure pins carry this mutation |
| MUT-4 | ignore the package toggle in `prettyTryRegisterLine` | P5 |
| MUT-5 | swap the sup/sub digit classification in the parser | M3 |
| MUT-6 | stray paint after the toggle gate, before the type gate declines | F1, F2 (the paint-then-decline failure mode the identity pins exist for) |
| MUT-7 | delete the vinculum fill in `ppPaint`'s RAD arm (PP2) | P4 |
| MUT-8 | exponent base loses the plain `10` append (PP2) | M6 |
| MUT-9 | IRFRAC accepts bare constant names (PP2) | M8 |
| MUT-10 | PSHOW does not arm the manual-paint protocol (PP2) | S3 |
| MUT-11 | swap not mirrored (PP3) | T9 (operand order flips) |
| MUT-12 | supersession removed (PP3) | T2 (history stays empty) |
| MUT-13 | as-typed literals dropped for value leaves (PP3) | T2/T3/T5/T8/T9/T12 |
| MUT-14 | shadow lift applied at NIM open instead of commit (PP3) | T16 (abort with ASLIFT set strands the tree, and it surfaces as a spurious emission) |
| MUT-15 | unknown-item default flipped to ignore (PP3) | T14 |
| MUT-16 | DONE applies the staged transform despite an error (PP3) | T11 |
| MUT-17 | precedence parens dropped (PP4) | FV1 |
| MUT-18 | DIV falls to function form instead of a stacked fraction (PP4) | FV2 |
| MUT-19 | token decoder pops operands swapped (PP4) | FV3 |
| MUT-20 | equation grammar accepts trailing content (PP5) | EQ4 |
| MUT-21 | paren unwrap under bar/vinculum dropped (PP5) | EQ2, EQ3 |
| MUT-22 | '/' swallows the whole expression instead of a factor (PP5) | EQ1 |
| MUT-23 | synthesized radical strokes deleted (PP6) | FV7 (probe excludes the vinculum column: the first version did not, and stayed green) |
| MUT-24 | ⁿ√ index dropped (PP6) | FV8 |
| MUT-25 | SUB script raised instead of lowered (PP6) | FV9 (pin reads the script node's relBase: root descent was satisfied by "log"'s descender) |
| MUT-26 | absolute-value bars deleted (PP6) | FV10 |
| MUT-27 | EQSHW parses with strip fonts (PP7) | EQ7 (30-row full-size span) + EQ8 |
| MUT-28 | big-∫ strokes deleted (PP7) | EQ8 (probe stays left of the operand's columns) |
| MUT-29 | unparseable-equation fallback removed (PP7) | EQ9 |
| MUT-30 | T-line formula default flipped to ON (PP8) | FV11 (default-off band identity) |
| MUT-31 | T-line branch loses its regist check (PP8) | FV11 (X-line identity under the toggle) |
| MUT-32 | RCL of a stack register loses its deep copy (PP9) | T18 |
| MUT-33 | RCL-arithmetic keeps the RCL item instead of mapping to the plain operator (PP9) | T19 |
| MUT-34 | x<>reg loses its displacement emit (PP9) | T20 |
| MUT-35 | browser recall leaves the shadow stale (PP10) | FV12 |
| MUT-36 | browser selection loses its clamp (PP10) | FV12 (over-navigated ENTER recalls nothing) |
| MUT-37 | reset drops the natural-display default-ON (PP11) | FV13 |
| MUT-38 | the toggle item flips nothing (PP11) | FV13 |
| MUT-39 | BIGOP limits swapped at STAGE (PP12) | B4 (under/over flip in the decoded layout) |
| MUT-40 | PPN_BIGOP dropped from emit eligibility (PP12) | B3 (hist stays 0), B4/B7 (no entry to decode) |
| MUT-41 | Σ strokes deleted from the PP_BIGOP paint arm (PP12) | B8 (operator column dark) |
| MUT-42 | derivative frame ignores the order (PP13) | EQ13 (superscript-2 glyphs missing). The FAIL line holds glyph bytes: trust the counts |
| MUT-43 | integral frame limits swapped (PP13) | EQ10 (under/over flip) |
| MUT-44 | EQSHW integrate arm dropped (PP13) | EQ8 (∫ sign missing) |
| MUT-45 | bound-variable restore dropped (PP14) | EQ14 (X left holding the counter) |
| MUT-46 | PROD seeded with zero (PP14) | EQ15 (product collapses to 0) |
| MUT-47 | argument slicer ignores paren depth (PP14) | EQ18 (nested construct mis-sliced) |
| MUT-48 | denominator bar clearance regressed (polish) | M1/M2 (descent 6), S2/EQ5 (bars move) |
| MUT-49 | the X-to-x typeset map dropped (polish) | EQ1/EQ3/EQ10-12/EQ21 (X reappears) |
| MUT-50 | ∫ hooks flattened to a bare bar (polish) | P5 (hook reach probes) |
| MUT-51 | PP_MAX_DEPTH regressed to 6 (stress) | EQ22 (the ultimate nesting fails to measure) |
| MUT-52 | additive big-operator bodies lose their parens (stress) | EQ25 (PROD 1+x misreads) |
| MUT-53 | multiplication dot reverts to the x glyph | FV1 (red only in the COUNTS: glyph bytes suppress the FAIL line) |
| MUT-54 | vinculum weight regressed to 1 px (standard) | P6 (row-2 probe at the vinculum's RIGHT end: near the sign, the glyph's own hook masked it) |
| MUT-55 | stack allowance zeroed (parity) | EQ18/EQ26/EQ27 (all nested evaluation refuses) |
| MUT-56 | MVAR scan hides only the construct NAME again (Bug 1) | EQ29 (the commit rejects the typed equation, error 45) |
| MUT-75 | negative paint x left unclipped (r3) | T29 (the fill-drawn rule is dropped instead of clipped) |
| MUT-76 | the `.d` exemption removed from the containment guard (r3) | **UNFALSIFIABLE from the harness**: documented gap, not a coverage hole. The guard lives in `executeFunction`. That function is `static`, and its only entry point hard-codes its item argument, so no test can drive a chosen key through it. Verified by trace instead: UP/DOWN are matched earlier in the same chain (`item == ITM_DOWN_ARROW \|\| item == ITM_UP_ARROW`), ENTER/EXIT/BACKSPACE have their own `case ITM_...` upstream, and `.d` has neither, so it needs the exemption. |
| MUT-77 | SUB/DIV operand order swapped (X emitted as the left operand) | V3, V16 |
| MUT-78 | ENTER stops duplicating | V3 |
| MUT-79 | right-operand parenthesization dropped (`prec < level` for both sides) | V21, V22 |
| MUT-80 | opaque taint dropped: a string literal's text reaches an operator | V10 |
| MUT-81 | the PGMINT latch cleared when a construct returns | V17 |
| MUT-82 | unit-step omission inverted (the step is always emitted) | V4 |
| MUT-83 | the invented sum counter's collision guard dropped | V6 |
| MUT-84 | the dirty-name guard dropped (a recall after STO reads the old meaning) | V11 |
| MUT-85 | solver-status neutralization removed around `ppqShowRender` | V19 |
| MUT-86 | a declined program painted anyway | V20 |
| MUT-87 | VISUAL removed from `menu_PP` | FV15 |
| MUT-88 | the construct body frame left unseeded | V4, V5 |
| MUT-89 | stack underflow yields an empty fragment instead of declining | V13 |
| MUT-90 | the shadowed-d-variable guard dropped | V23 |
| MUT-91 | an integral with no PGMINT latched allowed through | V14 |
| MUT-92 | unknown opcodes silently ignored instead of declining | V7, V15, V20 |
| MUT-93 | register reads spelled `Rnn` instead of declining | V12 |
| MUT-94 | local and indirect label parameters guessed at instead of declining | V8, V9 |
| MUT-95 | the radical takes precedence brackets on top of its own parentheses | V24 |
| MUT-96 | `1/x` loosens its argument level (ADD instead of MUL) | V26 |
| MUT-97 | the drawing painted full-screen instead of into the Z/T window | V27 |
| MUT-98 | the window narrowed to a single stack line | V27 (needed strengthening first, see below) |
| MUT-99 | the whole screen cleared instead of just the two rows | V27 |
| MUT-100 | item 984's parameter changed from `TM_LBLONLY` to `NOPARAM` | V36, V37 |
| MUT-101 | the ENTER lift latch ignored (a lifting read pushes instead of replacing) | V38 (V33 cannot see it, see below) |
| MUT-102 | `DROPY` drops the top instead of the second level | V35 |
| MUT-103 | `x<>y` does not swap | V34 |
| MUT-104 | `PROD` emits `SUM(` | V31 |
| MUT-105 | the renderer's f(x) arm removed | V41 |
| MUT-106 | the emitter's round-trip and drawability checks dropped | V42 |
| MUT-107 | `x³` emitted as a name instead of a superscript | V45 |
| MUT-115 | DERIV seeds the f' parameter again (PP18-1) | V60, V62, V63 |
| MUT-116 | the first declared MVAR always wins over a match | V61 |
| MUT-117 | a body declaring no MVAR is drawn anyway | V63 |
| MUT-118 | the layout latch ignored | **survives alone** (see below) |
| MUT-119 | the operand check moved after the second recursion | **survives alone** (see below) |
| MUT-120 | the full screen cleared before the fit is known (PP18-2) | V67 |
| MUT-121 | both PP18-3 guards removed (the shipped shape) | V66 |
| MUT-122 | a construct reports ATOM precedence again (PP18-4) | V68, V69 |
| MUT-123 | a nested construct body bracketed | V46 |
| MUT-124 | `varOff` back to `uint8_t` (PP18-7) | V70 |
| MUT-125 | the counter checked only against enclosing constructs (PP18-9) | V71 |
| MUT-126 | the lift latch survives XEQ again (PP18-5) | V72 |
| MUT-127 | PP18R2-1's regression restored: an MVAR-less body declines again | V63, V65 |
| MUT-128 | `ppfBigop` reports ATOM again (PP18R2-2, the neighbour) | B9 |
| MUT-129 | body seeding allocates one node per level again (PP18-8) | V77 |
| MUT-130 | a seeded (bound) read counts as a collision (PP18R2-4) | V77 |
| MUT-131 | an invented derivative name registered as real (PP18R3-1) | V78 |
| MUT-132 | the mirror picks the first DRAWABLE declaration (PP18R3-2) | V79 |
| MUT-133 | the EQN parser leaves a construct operand unbracketed (PP18R3-3) | EQ35 |
| MUT-134 | the limit subtrees not scanned when inventing (PP18R3-5) | V80 |
| MUT-135 | the full-screen arm claims only `MANUAL_STACK`, as the Z/T arm does | V-FULL |
| MUT-108 | a stacked power's base left unbracketed | V51 |
| MUT-109 | a construct body no longer scoped by precedence | V57 |
| MUT-110 | the derivative reads PGMINT's latch when PGMDRV has none | V55 |
| MUT-111 | the second-order flag dropped | V53 |
| MUT-112 | the construct builder's tiny/context variable runs swapped | V27, V36, V46, V50, V57 |
| MUT-113 | operand order swapped in the tree builder | 17 pins |
| MUT-114 | the ENTER lift latch ignored (tree form) | V38 |
| MUT-57 | prettyReset stops restoring the T-line default (PP15) | FV14 |
| MUT-58 | cold start restores factory defaults again (the PP11 persistence bug) | FV16 (both flags clobbered) |
| MUT-59 | the `-MNU_PP` slot in menu_DISP reverted to ITM_NULL (PP15) | FV15 |
| MUT-60 | the softkey state indicator frozen to unchecked (PP16) | FV17 (ink margin collapses) |
| MUT-61 | complex terms silently dropped in SUM/PROD (PP16) | EQ30 (both halves: result goes real, and the CPXRES refusal stops) |
| MUT-62 | infinite sums fall back to invalidate (PP16) | B9 (FAIL line is glyph-suppressed: trust the counts) |

Two lessons from the first mutation run, kept for honesty: (a) the original
MUT-4 ("drop the dtReal34 type gate") stays GREEN. A non-real register's
payload read as real34 is NaN-class garbage, so the adjacent range check
declines anyway and the mutation is masked. The type gate stays in the code
as belt-and-braces, but its pin-of-record is the F-identity contract via
MUT-6. (b) A stray paint placed BEFORE the toggle gate is invisible to the
F-pins (both compared renders paint it). The identity pins only see work
that the disabled path skips.

A pin that stays green under its mutation is decoration and gets deleted or
fixed. The forth-core 2026-07-21 rule applies unchanged.

### Visual-polish additions (post-PP14, Stan's review)

- **P5**: the integral sign's hooks reach ≥3 px sideways from the
  spine at the tip rows, both directions, spine present at mid-height.
- The Σ/∏ letterforms follow the FONT'S OWN designs scaled (the glyph
  bitmaps were rendered and read back as the reference). Width tracks
  height at the numeric Σ's 9/16 ratio via ppBigopBox, shared by
  measure and paint (the fixed 16 px width left tall operators
  pinched). Bar thickness is height/10, the Σ apex at 40% width, the ∏
  bar overhanging inset legs. B8's probe narrowed to the scaled box.
- M1/M2 descent moved 5→6 and M5 ascent 29→30: the symmetric-clearance
  fix. Descent counts rows at/below a baseline, ascent strictly above,
  so the bar's lower side was one row tighter than its upper side.
  The bar read as cutting the denominator's / radicand's top. P4 and
  S2 row pins shifted accordingly (vinculum up 1 at a fixed baseline,
  and the taller PSHOW fraction re-centers one row up).
- EQ1/EQ3/EQ10-13/EQ20/EQ21 pin the X-to-x typeset convention: the
  canonical variable X renders as the classic lowercase x in the
  pretty views only, and other names keep their letters. An early
  draft lowercased EVERY single letter and turned A+B into a+b. EQ2
  stayed pinned to caps and caught it.

### Ultimate-nesting additions (stress round, Stan's request)

- **EQ22**: the full tower `INTEG(DERIV(SUM(√x/(x+1))/(x×(x+1))
  ×PROD(1+1/(2+x²))))` parses, measures, uses ≥45 pool nodes and fits
  the EQSHW band at full size. Capacity raised for exactly this class:
  PP_MAX_DEPTH 6→12 (the pool is the real bound), PP_POOL_NODES
  48→72 (+384 B BSS: the DERIV layer alone measured 58 nodes).
- **EQ23-EQ25**: the stored-alphabet arms: '^' builds a real 2D
  superscript, a NAME: label prefix skips, an additive construct body
  scopes in parens. EQSHW now reads the STORED equation text.
  showEquation's display string is built for the 400 px strip and
  TRUNCATES long equations with an ellipsis. The strict parser then
  rightly declined those truncated strings: the ultimate demo found
  EQSHW silently capped at strip-width formulas. Construct
  limits/scripts always typeset TINY (the operator convention). tinyF
  only governs the strip's fraction shrink, and the '/' deep-refont is
  skipped when the caller's fonts are equal, so it cannot flatten
  them.

### PP16 additions (the deferred three, closed)

- **EQ30**: a construct body that evaluates COMPLEX now accumulates
  complex, on upstream's own terms: `SUM(X×i;X;1;3)` = 6i with CPXRES
  set, and the SAME formula is a domain error with CPXRES clear. That
  is exactly what upstream's `_programmableSumProd` does. DERIV and
  INTEG still refuse complex. This is not a gap: upstream's own
  differentiator and integrator contain no complex handling at all
  (verified: zero `dtComplex34` references in either file).
- **B9**: the early-stop sum (`ITM_SIGMAnINF`) captures like any other
  sum. `inf` changes only when the loop gives up, not what it reads. So
  the stack effect is identical, and its node carries the real limits
  the user supplied. GUARDED by `#if defined(OPTION_INFSUMS)`. Without
  the option, that item is an unimplemented stub that moves no stack.
  To classify it mints a node for an operation that never ran.
- **FV17**: the two toggles in our own menu show their state. The test
  renders twice with the flag opposite and compares ink in the PPON
  softkey cell: measured 291 lit (filled) vs 258 (outline). The assert
  requires a margin of 8 rather than a literal count, per the harness
  rule that a font change must never turn a test red.

### PP15 additions (both toggles are flags, and the menus are wired)

- **FV14**: the T line is a real flag: `prettySetTline` and the toggle
  item both move `FLAG_PTLINE`, and a reset leaves it OFF. Note the
  ASYMMETRY with FV13: the two defaults are reached by opposite routes.
  A reset's flag wipe already IS the T line's default, while the
  master toggle's default-ON must be re-established.
- **FV15**: the softmenu claims are wired. The check LOOKS at the
  live `softmenu[]` table, not the patch: `-MNU_PP`
  resolves, holds exactly its six items in order, and both parent slots
  (`menu_DISP`, `menu_EQN`) contain what §7 says they contain.
- **FV16**: the cold-start path initialises package data WITHOUT
  touching the user's flags. This pins a latent PP11 bug that FV14
  surfaced: `prettyReset()` was called from five lazy-init sites as well
  as from `doFnReset`, so the first dispatch after a cold start
  force-set the master flag and silently overwrote a saved preference
  (the persistence PP11 claimed to deliver). Init and factory-reset are
  now separate functions (`ppcInit` vs `prettyReset`). And
  `ppcTestDeinit` (package-internal, test only) re-arms the cold-start
  path so the contract is testable at all.

### Keyboard-journey addition (Stan's challenge: can it even be typed?)

- **EQ29**: the whole user journey through the REAL key path. It types
  `SUM(X;X;1;3)` one softkey at a time into the equation editor (with
  the alpha punctuation catalog open and `fnKeyInCatalog` set, exactly
  as a softkey press leaves it), runs the commit ENTER runs
  (`setEquation` + the MVAR parse), then evaluates to 6. Closes the gap
  that every other EQ test builds its equations from C and so never
  proved that a user can enter one. Harness note: `reallyRunFunction`
  passes the CALLER's param, so a character item driven through it
  calls `addItemToBuffer(NOPARAM)`, a bug-screen path that silently
  inserts nothing. keyboard.c calls `addItemToBuffer(item)` directly.
  Drive it the same way. Three probe rounds were lost to that.

### Render/eval parity additions (Stan's ruling: eval must match render)

- **EQ26**: `INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;1;2)` evaluates
  through the real fnEqCalc path to 7/24 within 1e-10 (~2.5 s sim).
  The limits [1,2] avoid an UPSTREAM second-derivative defect near
  zero (DESIGN.md table). They are not chosen to flatter the feature.
  The full reference tower (Σ√-fraction / multiplication × ∏, inside
  d²/dx², inside ∫ over [1,2]) was measured once: 0.18234918164357208…
  against the analytic value, 16+ digits in 27.8 s sim. It is recorded
  here, not pinned, so the gate stays fast.
- **EQ27**: `INTEG(INTEG(X;X;0;1);X;0;1)` = 0.5: nested integrals
  work (the new DEI path never increments the engine counter).
- **EQ28**: the upstream near-zero second-derivative defect and its
  upstream-native remedy. At default settings, the same integral over
  [0,1] returns −2.947e23. Once the derivative's own step variable
  `δ_d` is set, it returns 5/6 to 31 digits. The test restores the
  variable to zero (which the engine reads as "unset") so it cannot
  leak into later suite files.
- Debugging trail worth keeping: the differentiator's entry parse runs
  MVAR mode, which errored on every construct (';' is a base-grammar
  error). MVAR now consumes whole construct spans. Temp-slot appends
  refuse under a PENDING error (the rollback previously fired on
  errors it did not cause). And the zero-limit DERIV-integrand caveat
  was isolated by a per-sample probe that showed the stencil collapse
  at x ≈ 1E-24 abscissas.

## Blast radius and measurements

- Keep the pre-change testSuite log. Diff sorted `PASS:` lines after.
- BSS delta measured at every stage gate (`size` on the sim binary,
  before/after). The §6 budgets in the two DESIGN.md files are the ceiling.
- Flash: `make dmcp5r47 CUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print,packages/pretty-print-extra
  CUSTOM_PKG_RECONFIGURE=1` delta recorded in the stage commit (without the
  reconfigure flag the number is the previous tree's).
- Visual confirmation via the run-sim skill's capture driver (not a gate:
  the pixel pins are the gate).

### Documented gap: the browser's unbuildable-row branch (AUDIT R4-4)

`pbPaint`'s `!ppfBuildRow(...)` arm is HARDENED but NOT PINNED, because no
reaching input for it was found. The code defect is real and
was fixed. Both passes used to `continue` a failed row out, which also
skipped `selPage = page` when that row was the selected one. So `selPage`
kept its initialiser, pass 2 painted page 0, and no selection marker
appeared anywhere. Both passes now reserve a fixed-height placeholder,
so the row pages, selects and marks like any other.

One thing was not shown: that a row ever fails to build. Two ceilings
sit below the layout engine's:

- The capture arena is 24 nodes (`PPC_NODES`), and a chained operator
  costs an OP2 plus a literal, so a filed formula tops out near eleven
  operators, well inside the 72-node layout pool.
- Deep division chains do not stack. A 25-level tower measured h=31,
  w=278: one fraction level, because the renderer's depth guard drops
  nested division to an inline slash. Height failure needs a tower the
  arena cannot hold in the first place.

So this is recorded the way round 4's `.d` exemption was: a documented
gap with the analysis that reaches it, not a coverage hole. **Do not
"fix" this with a fixture that forces the state artificially.** A
pin that cannot reach its own state through a real gesture is the exact
vacuity class R4-1 was about. A T30 written for this branch was
removed the same hour for failing that test. If a reaching input is ever
found (a big operator with a tall body is the untried candidate), the
pin is a pixel check that the 3px selection marker appears somewhere in
rows 25..163.

### P13 and a documented gap in R3-13's own fix (audit R5-2)

**P13** repaints the X line: a 35-digit value, then a 3-glyph value over it
WITHOUT clearing the band. It asserts that the result is pixel-identical
to the short value painted onto a freshly cleared band. Measured, both are
467 lit pixels. It exists because R3-13 stopped each glyph clearing its
whole font box. That is safe ONLY while something else clears the band.
And upstream's `clearRegisterLine()` calls are commented out at their call
sites, so the thing this depends on is not obviously present. P13 holds
that dependency instead of assuming it.

**The gap, stated plainly.** MUT-D deleted the
measured-box `ppFillVal(..., LCD_SET_VALUE)` from `ppShowRun` entirely and
the suite stayed GREEN, P12 and P13 both. That is not a coverage hole for
a new fixture to close. It is the truth about the fix. Of
R3-13's two halves only one is load-bearing:

- `noPreClear = true` is what fixes the defect. P12 reds without it.
- The explicit measured-box clear is REDUNDANT on every surface the engine
  has, because each clears its whole band before it paints a tree.
  Measure lays siblings out non-overlapping, so nothing within a tree
  ever paints over anything else.

It is kept anyway: it costs nothing measurable, and it keeps
erase-before-draw true locally rather than as a property of every caller.
The commented-out `clearRegisterLine` above is exactly the kind of
upstream change that can make it load-bearing overnight. But no mutation
of THIS package's code can turn it red, so it is unverified code and is
recorded as such: the same disposition MUT-76 got, not a green light.

So P13 is an UPSTREAM-DRIFT pin, in the same family as P1's exact
bar rows: our own code cannot break it, and that is the point.


## The V family: VISUAL, the RPN-program walker

`prettyTestVisual` (tests/pretty_print.txt) pins the transpiled
string for most cases. But since PP18 that string is NOT the product.
The product is a node tree. The text back end is a PC_BUILD-only test
seam. The string is asserted because it is readable and derived from the
same AST the drawing is. The node-shape pins (V46-V51, V56, V57,
V68, V69, V73, V74) exist because a string cannot catch a fault in the
layout pass. `ppvTranspile` is the seam the string pins read through.

AUDIT PP18-16 corrected this section. The section used to call the string
"the walker's whole product", and that wording sends the next reader to
the wrong oracle. AUDIT PP18R2-8 then caught the correction pasted in
FRONT of the sentence it replaced, so the file asserted both readings at
once. A correction that does not delete what it corrects is not a
correction. It leaves the reader to guess which paragraph is current,
and the stale one reads more confidently.

Fixtures are appnote 22's own chain (`docs/appnotes/sources/AN0022`) with
package-local label names, loaded through `ppcTestWriteAndLoadPgm`, the
official program loader. So the pins walk real program memory, not a
hand-built array. The walker can read a hand-built array differently
from the calculator.

| pin | what it holds |
|---|---|
| V1 | `DBLINT` → `INTEG(INTEG(t;t;0;x);x;0;2)`. The ask itself: latch, recursion, seeding, rollback, and the plot-title idiom passing through, in one string |
| V2 | `TRPINT` → three coupled levels, plus an exponent literal and a STO'd name riding through without reaching the mathematics |
| V3 | `FX` → `x×x-x×p-2`: SUB operand order (Y−X), ENTER dup, MVAR skipped |
| V4/V5 | programmed sums. A unit step omitted, a step of 2 kept |
| V6 | the invented counter name refuses to shadow a real variable |
| V7-V15 | the decline catalog: SOLVE, local label, indirect, opaque-in-maths, dirty name, register read, underflow, unlatched integral, flow control. Each asserts its own D-number, so a decline for the WRONG reason is a failure |
| V16, V21, V22 | precedence. V16 is a lower-precedence right operand. V21/V22 are the equal-precedence case, and they are the ones that matter: the left/right asymmetry exists only for them, and MUT-79 survived until they were written |
| V17 | the PGMINT latch persists across a construct |
| V18 | the loop closed: a transpiled string EVALUATES to 4/3 through `fnEqCalc`. A picture that cannot compute is a transpilation that only looks right, and no string pin can catch that |
| V19/V20 | the surface: a stale solver session must not reframe the drawing and must come back bit-exact. A decline paints nothing |
| V23 | an inner d-variable spelled like an outer one declines |
| V27 | the drawing lands in the Z/T rows, spans BOTH of them, does not reach the Y line, leaves the X line byte-identical, and declares its pixels so EXIT dismisses it |
| V-FULL | the OTHER paint arm: a quadruple integral (98 px standard, 91 tiny) is past the 72-row Z/T band at both rungs, so the full band takes it. Asserts the two frame lines, ink below the Z line, and all three chrome bits: the stack-window arm claims one and clears the other two, so the chrome mask is what tells the arms apart. The pin exists because a probe found every painting call in the suite taking the stack window |
| V28 | the six measured heights the placement rests on, plus the two inequalities they support: one line does NOT hold a full-size integral, the pair DOES hold the double |
| V29-V32 | shapes appnote 22's own set never reaches: a constructed function UNDER an integral (its PLTINTG integrand), a SERIAL XEQ chain (the other half of "nested or serial"), the `PROD` arm, and an integration limit that is an expression rather than a literal or a bare name |
| V33, V38 | the ENTER lift latch. V33 is the readable case (`5 ENTER 3 +`). V38 is the one with teeth (see below) |
| V34, V35 | `x<>y` and `DROPY`, whose whole content is which stack level they touch |
| V36, V37 | driven through the real keys: the command, ALPHA, the label typed a letter at a time, ENTER. This is the transient-alpha path `TM_LBLONLY` exists for, and every other pin skips it. V37 pins that an unknown name is refused by TAM and never reaches the walker |
| V39 | a program KEYED IN through PEM, not hand-encoded (see below) |
| V40, V43, V45 | named functions: emitted from the item's own catalog spelling, composing under an integral, and `x³` staying a superscript rather than becoming a name |
| V41 | why the renderer gained an f(x) arm: `SIN(x)/2` must still build a FRACTION. There is no 2D gain in the function itself: the gain is that one unrecognised name no longer costs the whole formula its 2D form |
| V42 | a monadic whose catalog spelling is glyphs (`e^x`) declines rather than emit text that neither draws nor computes |
| V46-V51, V56, V57 | the node tree the product paints (`ppfTestExpect`), not the serialized text. V49 and V51 are the two places the node form differs from the text form: a fraction bar SCOPES so `a/(b+c)` needs no parentheses, and a stacked power DOES need its base bracketed. V57 pins that an additive construct body is still scoped: the parser sniffs runs for a `+`/`-`, the tree asks the precedence, and they must agree |
| V52-V55 | derivatives: the point off the stack, the program from PGMDRV, the order from which item was used, and (V55) that PGMINT's latch does NOT serve `f'`, because upstream keeps those slots apart on purpose |
| V65 | the differential oracle, and the pin this battery most needed. Runs each program, evaluates the walker's OWN drawing, and requires them to agree: no expected string appears in it. It is the only pin here that fails because the picture MEANS the wrong thing, not because it reads differently from what I typed. That failure mode is the class both PP18-1 and PP18R2-1 belonged to. AUDIT PP18R2-5 found it recorded as delivered in three places and never written: it was lost when a mutation runner reverted the tree, and nothing but prose referred to it, so nothing noticed |
| V75, V76, V77 | the two lift-latch arms PP18-5 fixed but did not pin, and five DISJOINT sums that must all be free to use `n` |
| B9 | the CAPTURE engine's big operator as an operand: the neighbour PP18-4 left behind, reached by every PSHOW and PHIST of a programmed sum |
| V58 | the REAL appnote-22 file (`docs/appnotes/sources/AN0022/func.p47`) loaded through the official loader and transpiled. Every other fixture is one I wrote and hand-encoded, so they are a statement about my encoding as much as about the walker. This is Jaymos's own file. Its own driver, `prettyTestReal`, registered before `graphs_cov` (see below) |
| V44 | an emitted name COMPUTES (`LN(1)+2` = 2): the round-trip through the evaluator's own resolution, checked end to end |
| V24-V26 | the four monadics with a grammar spelling. V24 and V26 were both written after a mutation survived: `√` bracketed its argument twice (its `pre` already emits a parenthesis), and `1/x`'s argument level is only distinguishable from a looser one by a SAME-level operand: `1/a×b` is not `1/(a×b)` |

**Each round's worst finding came from the previous round's fixes.**
Round 1 found defects in the shipped code. Round 2's two worst findings
came from round 1 repairs. Round 3's two worst findings came from round 2
repairs. One repair registered an invented derivative name as real. The
shadow guard for invented names did not arm. Another repair made the
mirror select the first DRAWABLE declaration. The mirror previously
selected the first declaration, as its name requires. **The rate has not
fallen, exactly as CODE_AUDIT.md says it does not.** The existing pins
reached neither defect. Both defects fit V65's shape. V65's five programs
missed each defect by one letter.

**The big-operator-as-operand class had three producers.** They were the
walker (PP18-4), the capture engine (PP18R2-2) and the EQN parser
(PP18R3-3). A wrong picture exposed the walker defect. A review of a
fix's neighbours exposed the capture-engine defect. The same review one
round later exposed the parser defect. Fixing a class only at its first
known site fixes only that site.

**A fix caused a regression. An existing pin catches it for free.**
PP18-1's fix declined every derivative over a body that declares no
`MVAR`. The fix assumed that upstream varies nothing in that body.
`fnFillStack` is unconditional, and only the `STO` is guarded. Thus, the
ordinary stack-consuming RPN body differentiates correctly. The refusal
was a regression against PP17 (PP18R2-1). **V65 catches this regression
in one line. The record already marked V65 as done.** A mutation runner's
`git checkout` removed the pin. Only prose referred to it, so no test
failed. A pin that exists only in prose is worse than a missing pin. It
is a missing pin that stops another review.

**MUT-118 and MUT-119 survive alone, and that is the design.** The
exponential fix has two guards. One guard is an entry latch. The other
guard checks the operand BEFORE the second recursion. Each guard heals
the other guard's mutation. If the latch is absent, the early return
still stops the doubling. If the early return moves, the latch still
stops the doubling. MUT-121 removes both guards. It reproduces the
shipped shape and reds V66. The record identifies two self-healing paths,
not two coverage holes. This follows the skill rule to examine
self-healing paths first. The redundancy is deliberate. This finding can
cause a calculator to never return.

**V66 asserts a visit COUNT, not a time.** A wall-clock pin passes on
this desktop for a program that hangs an 80 MHz DM42n. The counter lives
in the walker's own context and the test seam returns it.

**V38 and the invisible half of a stack rule.** MUT-101 (ignore the ENTER
lift latch) survived V33 (`5 ENTER 3 +` → `5+3`). With the latch,
ENTER-dup-then-replace leaves `[5, 3]`. Without the latch,
ENTER-dup-then-push leaves `[5, 5, 3]`. **The top two values are
identical in both traces.** The latch affects only stack DEPTH. Nothing
visible differs until an operation reaches PAST the top two values. V38
consumes one more value than the correct trace provides. The correct
trace declines with D10 underflow. A walker with the phantom copy finds
a value and prints an expression. The same-level lesson below describes
the same test class.

**V18 stopped grading its own fixture (PP18).** It previously used
`setEquation` on a string typed in this file. It then checked that the
string evaluated to 4/3. The same person typed the adjacent V1 string,
so agreement did not prove that either string was correct. V18 now
transpiles VDBL and evaluates **the walker's own output**. The pin does
not obtain 4/3 from an expected string in this file.

**V58 has to run late, but it cannot run at the end.** It CLEARS PROGRAM
MEMORY. The driver's fixtures have nearly filled program memory.
func.p47's labels (`DBLINT`, `HT`, `IT`, ...) intentionally collide with
labels in upstream's `nested_cov` programs. When `prettyTestVisual` ran
func.p47, it erased the programs that `programs.txt` expects. Six
upstream cases then failed 300 lines after the cause. Their messages did
not identify V58. V58 has its own driver and case file for this reason.

V58 also cannot use the end of `testSuiteList.txt`. forth-core appends
`forth_interp` there. Both patches then generate the same `@@ -507,3`
hunk, and the combined pass refuses to apply them. V58 is anchored before
`graphs_cov`. This location satisfies the ordering and stays ~10 lines
from the contended tail. **The solo gate was green for both mistakes.**
Only the combined pass caught the patch conflict. Only the full suite
caught the erased programs. The reduced case list showed neither defect.

**V39 verifies the encoding of a keyed program.** Every other fixture
hand-encodes its bytes (`ITM_LITERAL, STRING_LONG_INTEGER, 1,
'0'`).
Those fixtures assume that the calculator writes the same bytes for
keyed input. V39 keys `LBL 'VKEY' / RCL 'a' / ENTER / x / 2 / - / RTN`
through PEM. It uses `runFunction()` in `CM_PEM` and parameterised steps.
Those steps use the same transient-alpha machinery that a user drives.
V39 then transpiles the result. PEM's literal was `72 08 01 32`. This is
the byte sequence in the hand-encoded fixtures. **The PEM result verifies
the hand-encoded bytes.** If upstream changes PEM step encoding, V39 reds
while every hand-encoded pin stays green. **No package-code mutation can
red V39.** Its disposition is an upstream-drift pin, like P13.

The fixture has two setup constraints. `getNumberOfSteps()` returns the
step count of the CURRENT PROGRAM. Keying from that count inserts new
steps into the current program. The first attempt built V39 inside
another program. The walk then continued into that program's `XEQ 09`.
Also, `fnGotoDot` does **not** clamp its input. A step number past the end
moves `currentStep` to NULL and cores the suite. Derive the append point
by walking to `.END.`. This follows the run-sim rule: never hand-count a
step number.

**MUT-98 exposed a fallback that made a wrong band look correct.** The
first narrow-window mutation survived V27. The 2D form correctly failed
to fit. The LINEAR fallback then painted on the Z line without using the
selected band. Thus, ink still appeared inside the region that V27
checked. The fallback now centers itself in its selected band. A
placement that ignores its band cannot verify the band. V27 also requires
ink in the T half AND the Z half. This requirement distinguishes a form
across the Z/T window from a one-line form inside that window. A fallback
in the test region can mask failure of the tested path.

**Running the suite under AddressSanitizer.** The simulator build sets
`b_sanitize=none`. Thus, the normal gate cannot prove a memory-safety
claim. Configure a separate tree. Keep `build.sim` unchanged:

```
meson setup build.asan --buildtype=custom -DRASPBERRY=false \
  -DDECNUMBER_FASTMUL=true -Db_sanitize=address \
  -DCUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print
meson configure build.asan -Dc_args='-DFORTH_DEBUG_SELFTEST'
ninja -C build.asan
build.asan/src/testSuite/testSuite build.asan/custom_pkg_shadow/testSuite/tests/testSuiteList.txt
```

Run the binary directly. `meson test` sets its own `ASAN_OPTIONS`. It
discards an `ASAN_OPTIONS=` value from the command line. The
`halt_on_error=0` setting does not recover from a stack-buffer-overflow
without `-fsanitize-recover=address`. To continue PAST a known error,
edit the copy in `build.asan/custom_pkg_shadow/`. Materialized files are
regular files. Use `ls -l` to examine them first. Rebuild only that tree.

**Expected ASAN results.** ASAN reports one upstream error. The report is
in `UPSTREAM_REPORTS_addons_resultingIntStr.md`. All
`evaluate the drawing` pins fail under ASAN: EQ16/17/18/26/27/28/32/33/34,
V18 and V65.
No drawing or measurement pin fails. `PPEQ_STACK_ALLOWANCE` is 8000 bytes,
measured against a real stack probe (`solver/equation.c:1756`). ASAN's
redzones increase each frame. The evaluator then exceeds its budget and
correctly refuses. After a local guard for the upstream read, the sweep
completes with **zero** memory errors.

**And the caveat that was recorded against V27 was wrong.** Round 6 noted
that the fixture returned `lastErrorCode` 24 with
`screenHoldsDrawnPixels` false. That reading indicates that no paint arm
ran. A probe at the assertion point measures `err=0`, `holds=1`,
`inT=3827912`, `inZ=4394459`, `below=0` and a byte-identical X line. The
earlier reading belonged to V20. V20 runs immediately before V27 and
declines with that code. MUT-97, MUT-98 and MUT-99 had already reddened V27
through the paint arm. That evidence also showed that V27 reached the
paint arm, but the review did not use it. **Put the probe where the pin
asserts. A probe at another point measures a neighbour.** The probe also
established V-FULL. Six calls in this suite reach a paint arm. All six
used the stack window. Thus, the success path of `ppvPaintFullScreen` had
no reaching input.

**The same-level lesson.** MUT-79, MUT-90 and MUT-96 all survived their
first battery for the same reason. Each pin used a case where the correct
rule and the broken rule agree. A lower-precedence operand brackets under
both rules. Only an equal-precedence operand distinguishes the rules. If
a rule uses `<` on one side and `<=` on the other, test the boundary.
Otherwise, the pin tests the `<` case twice.

**Harness lessons from the 2026-08-28 session.** A fast mutation loop over
the reduced `pretty_print` case list produced three types of false result:

1. **The banner is not one string.** The suite prints `1 TEST  FAILED`
   (singular, two spaces) for one failure and `2 TESTS FAILED` for more.
   A runner grepping `TESTS FAILED` scores every single-failure mutation
   as survived. The fix is to compare the PASSED count against
   `NUMBER OF TESTS` and to refuse to report at all when neither number
   is present. A run that did not happen must never read as green.
2. **A mutation runner must restore the SHADOW, not just the source.**
   The runner reconfigured with the mutation applied, then reverted the
   source. This left `build.sim/custom_pkg_shadow/items.c` with MUT-100.
   Every later build ran a VISUAL whose parameter was
   `NOPARAM`, and two later pins failed for a reason that had nothing to
   do with them. Revert must be followed by refresh AND reconfigure.
3. **Only package-OWN files are symlinked into the shadow tree.**
   `prettyVisual.c` and `prettyTest.c` are symlinks. Thus, refresh and
   `ninja` rebuild them. The resolver materializes `softmenus.c`, `items.c`
   and every other PATCHED UPSTREAM file. `ninja` alone rebuilds the
   previous copy. MUT-87 "survived" because it tested stale code.
   Mutations in patched files need `meson setup --reconfigure`.
   `build-test.sh` performs that step on every pass. Only that script is
   the gate.

This is the same shape as the PP12-era stale-binary trap (`pp-iter.sh`'s
`|| true`), found again from the other end. The reduced-list loop is a
development convenience. The gate is `build-test.sh`.
