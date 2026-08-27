# Pretty-print package — testing

The package is tested in upstream harness 2 (`src/testSuite/`), the same way
undo-history is: coverage drivers registered in `funcTestNoParam[]` with
`coverageDriver = 1`, driven by `testSuite/tests/pretty_print.txt`
(`In:/Func:/Out:` blocks; each driver writes its failure count into X as a
long integer, `Out: EC=0 RX=LonI:"0"`). Drivers live in the package file
`prettyTest.c` under `#if defined(PC_BUILD)` — test-only code, never built
for the device (undo-history's driver shape, packages/undo-history/
undoHistory.c PC_BUILD section).

The gate is `./packages/pretty-print/build-test.sh` (refresh → configure →
build → testSuite, solo AND combined with forth-core + undo-history; exit
status and the success banner both required). An edit to the flat working
area is invisible until `tools/pkg_patch_refresh.py` runs — the gate script
does this first, always.

## Drivers (stage PP1)

- **`prettyTestMeasure`** — engine-level battery against `ppNewRun`/
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
- **`prettyTestPixels`** — the real inline path: X = 0.75, `FLAG_FRACT` set,
  `CM_NORMAL`/`TI_NO_INFO`, band cleared, `refreshRegisterLine(REGISTER_X)`.
  X-line geometry (baseY 132, baseline B 160, fraction x-span 388..399):
  - P1 (bar, exact rows): rows **149 and 150** lit across 388..399; rows 148
    and 151 clear in that span.
  - P2 (numerator strictly above): gap row 147 fully clear in-span; digit
    ink present in rows 135..146.
  - P3 (denominator strictly below): gap row 152 fully clear in-span; digit
    ink present in rows 153..164.
  - P5 (toggle-off control): same state with the package disabled → row 149
    is NOT a full lit run (upstream's diagonal rendering differs).
- **`prettyTestFallback`** — identity pins for "pretty declines":
  - F1: X = string (unsupported type): band bitmap with package enabled ==
    band bitmap disabled, bit for bit.
  - F2: plain real (`FLAG_FRACT` and `FLAG_IRFRAC` both clear, no exponent
    form): same identity.
  - F3: `displayValueX` parity: fraction rendered pretty vs upstream —
    `displayValueX` strings byte-identical (pins the builder-first rule).

### PP2 additions

- `prettyTestMeasure`: M5 radical ascent 29/descent 0 (numeric radicand);
  M6 exponent split (base text `1.5·10`, exponent `40`, SUP node); M7
  IRFRAC `√3/2` → RAD inside a FRAC numerator, denominator `2`; M8
  `3×π/4` accepted, paren-power `(π²)` and bare `π` decline.
- `prettyTestPixels` P4: X = √2 (34-digit) with IRFRAC on — vinculum rows
  131-132 (baseline 160, numeric radicand top 134, radGap 1, vincThick 2),
  gap row 133 clear and radicand ink present, both probed in the
  radicand's columns only (the sign's own diagonal legitimately crosses
  the gap row on the left).
- **`prettyTestShow`** — PSHOW: frame lines rows 20/168 full width;
  centered stacked 3/4 at the numeric/numeric rung (bar rows 93-94,
  midpoint within 40 px of center); `SCRUPD_MANUAL_STACK` +
  `screenHoldsDrawnPixels` armed; string X falls back to the ordinary
  SHOW (temporaryInformation leaves TI_NO_INFO).

### PP5 additions

- **`prettyTestEquation`** — the EQN strip grammar: EQ1 `1/X+2` keeps
  '/' bound to factors (`[F(1|X) + 2]`); EQ2/EQ3 parens unwrap under a
  bar or vinculum; EQ4 strict declines (no-fraction expressions, dangling
  operators, ellipsis-truncated strings, glyphs outside the grammar); EQ5
  `prettyTryEquation("1/X", 1)` paints the bar at row 182 inside the
  equation's own 23 px strip row and nothing below it.

### PP4 additions

- **`prettyTestFormula`** FV6 (stress, post-PP5): the three-level
  continued fraction 1/(2+3/(4+5/6)) keyed through the real paths must
  appear in the pager — pins the variable-height packing and the
  whole-tree tiny re-font (both were missing; FV6 was red first, and the
  packing rewrite turned FV5 red once by wiping the frame line with
  glyph-box pre-clears — hence the 4 px band inset).
- **`prettyTestFormula`** — layout-signature pins over the infix
  renderer (RUN text / `[hbox]` / `F(a|b)` / `S(a|b)` / `R(a)` / `P(a)`,
  expectations built from catalog names): FV1 `(2+3)×4` keeps its
  parens; FV2 `6÷(2+3)` becomes `F(6|[2+3])` with unparenthesized
  children; FV3 a history token stream decodes to `[[2+3] = 5]`; FV4
  `√2` then `x²` gives `S(R(2)|2)`; FV5 the PHIST pager paints its
  frames, holds content ink, arms the manual-paint protocol, and PCLR
  empties the ring. Note for mutation runs: FAIL detail lines can carry
  raw glyph bytes (×), which flips grep into binary mode — trust the
  pass/fail counts.

### PP3 additions

- **`prettyTestCapture`** — sixteen traces through the REAL interactive
  paths (digits via `addItemToNimBuffer`, which opens NIM itself from
  CM_NORMAL; operators close NIM at the addItemToNimBuffer tail before
  `runFunction`, exactly the press/release split): T1 chaining, T2
  supersession (`2+3=5` emitted, `5+6` current), T3 monadic through the
  funnel, T4 ENTER dup (deep-copy independence — caught the aux/EMITTED
  length-truncation bug on first run), T5 chain through monadic, T6 CLX
  displacement, T7 as-typed `2.50`, T8 NIM abort (deferred lift → no
  ghost), T9 swap order, T10 UNDO discard, T11 DONE-on-error, T12
  arena-exhaustion truthful recovery (`# 1 + 1 +`), T13 LASTx as value
  leaf, T14 unknown-item default (MIN emits then invalidates), T15 eRPN
  ENTER (driver mimics `nimWhenButtonPressed` — keyboard-owned, false in
  a driver, so the eRPN condition needs the mimic), T16 abort with
  ASLIFT set (separates deferred-lift from lift-at-open designs).
  Expected signatures build from `indexOfItems[].itemCatalogName` at
  runtime so name changes never turn them red.

  **Signature alphabet** (postfix): a literal prints its own text, `#` a
  PPN_VAL value leaf, `~` the PPC_UNKNOWN sentinel, `!` a PPN_OPAQUE
  node, `?` PPC_NIL or an out-of-range index, `R<nn>` a register
  reference, `{a,b}NAME` a big operator, and `-` an EMPTY signature —
  which means the display path WITHHELD the formula, not that nothing
  happened. Audit R4-2 split `~` out of `#`: the two shared one
  character, so no pin could tell a truthful value leaf from the unknown
  sentinel — exactly the distinction the binding invariant turns on. When
  the split landed, all three existing `#` expectations stayed green,
  which proves none of them had been silently accepting an UNKNOWN.

  **A signature pin that expects `-` is asserting a real outcome; a pin
  that merely checks some substring is ABSENT is not.** An absent
  substring is satisfied by `-`, so such a pin passes just as well when
  capture has stopped working altogether. Audit R4-1 converted T23 and
  T26 from absence checks to exact-signature assertions for this reason,
  and rewrote T21, whose every assertion sat behind a guard that
  `ppcCurrentFormulaRoot()` made false by construction — it would have
  passed with ITM_RCLADD removed from the classifier entirely. Where a
  pin needs to see through that screen, `ppcTestCurrentRaw()` returns the
  root before it.

### PP12 additions

- **`prettyTestCapture` B-traces** — the big-operator family, driven
  through a real loaded program (`ppcTestWriteAndLoadPgm`, copy-adapted
  from testSuite.c's `covWriteAndLoadPgm`; label `P`, body `x²`):
  B1 Σₙ over 1..10 (sig `{#,#}Σₙ`, X pinned = 385 as real34),
  B3 CLX displacement emits the bare BIGOP root,
  B4 history decode pins limit ORDER, label-name decode and node shape
  (`B(P(n)|[n= 1]|10)`), B5 the result chains (`{#,#}Σₙ 2 ×`, one
  emission), B6 the ∫ currency that actually integrates (PGMINT
  preselects the program; the INTEGRAL_YX param is the integration
  VARIABLE) with X pinned ≈ 1/3, B6b the label-param SETUP form must
  not mint a node (sig `-`: it consumed X,Y with no result to vouch
  for), B7 a non-unit step is visible (`n=1,Δ2.` — the step travels as
  real34, upstream's real marker included), B8 pixel pin on the
  stroke-drawn Σ in the operator column only (a wider probe let the
  body ink mask a stroke deletion — first run stayed green).
  The traces restore `currentSolverStatus/Program/Variable` and
  `currentMvarLabel`: the ∫/Σ dispatches retarget the solver, and
  later suite files (deriv_cov) assume the status they inherited.

### PP13 additions

- **`prettyTestEquation` EQ10-EQ13** — the solver-surface frames, tested
  structurally through the exported builders (`ppqFrameIntegral` /
  `ppqFrameDerivative`) plus the existing EQ8 pixel pin for the render
  path: EQ10 interactive integrate shows the REAL limits and d<var>
  (`B([F(1|X) dX]|0.|1.)`), EQ11 without INTERACTIVE falls back to the
  bare stroke ∫ (`I(F(1|X))` — the walker gained a PP_INT arm), EQ12
  d/dX with the variable name decoded live, EQ13 d²/dX² carries the
  superscript-2 glyphs. Solve framing (f(x)=0) was SKIPPED and the
  reason recorded: `SOLVER_STATUS_EQUATION_SOLVER` is the zero value,
  so a stale INTERACTIVE bit is indistinguishable from a live session.

### PP14 additions

- **`prettyTestEquation` EQ14-EQ21** — the equation-language constructs,
  evaluated through the REAL fnEqCalc path: EQ14 `SUM(X^2;X;1;10)` = 385
  AND the bound variable comes back holding its prior 99, EQ15
  `PROD(X;X;1;5)` = 120 (seed one), EQ16 `DERIV(X^3;X;2)` = 12 and
  `DERIV(X^3;X;3;2)` = 18 — EXACT, because the delegate runs the same
  engine deriv_cov pins, EQ17 `INTEG(X^2;X;0;1)` ≈ 1/3 (the
  double-exponential integrator itself), EQ18 the nested
  `SUM(SUM(Y;Y;1;X);X;1;3)` = 10 (argument slicing honours paren
  depth), EQ19 a wrong argument count raises the equation's own error,
  EQ20/21 the 2D shapes (`B([X ²]|[X = 1]|10)`,
  `[F(d|[d X]) U(P(X)|[X = 2])]`).

## Mutation pins (run each separately, battery green between restores)

All six demonstrated red on 2026-08-26 (targeted single-file battery), green
after restore.

| id | mutation | went red |
|---|---|---|
| MUT-1 | delete the `lcd_fill_rect` bar paint in `ppPaint`'s FRAC arm | P1 |
| MUT-2 | shift the bar two rows (barTopRel + 2) | M1, M2, P1, P2, P3 |
| MUT-3 | swap the numerator/denominator relBase formulas | M1, M2 — NOT the P-pins: P2/P3 assert ink presence per band and are shape-blind to which digit sits where; the measure pins carry this mutation |
| MUT-4 | ignore the package toggle in `prettyTryRegisterLine` | P5 |
| MUT-5 | swap the sup/sub digit classification in the parser | M3 |
| MUT-6 | stray paint after the toggle gate, before the type gate declines | F1, F2 — the paint-then-decline failure mode the identity pins exist for |
| MUT-7 | delete the vinculum fill in `ppPaint`'s RAD arm (PP2) | P4 |
| MUT-8 | exponent base loses the plain `10` append (PP2) | M6 |
| MUT-9 | IRFRAC accepts bare constant names (PP2) | M8 |
| MUT-10 | PSHOW does not arm the manual-paint protocol (PP2) | S3 |
| MUT-11 | swap not mirrored (PP3) | T9 (operand order flips) |
| MUT-12 | supersession removed (PP3) | T2 (history stays empty) |
| MUT-13 | as-typed literals dropped for value leaves (PP3) | T2/T3/T5/T8/T9/T12 |
| MUT-14 | shadow lift applied at NIM open instead of commit (PP3) | T16 (abort with ASLIFT set strands the tree; surfaces as a spurious emission) |
| MUT-15 | unknown-item default flipped to ignore (PP3) | T14 |
| MUT-16 | DONE applies the staged transform despite an error (PP3) | T11 |
| MUT-17 | precedence parens dropped (PP4) | FV1 |
| MUT-18 | DIV falls to function form instead of a stacked fraction (PP4) | FV2 |
| MUT-19 | token decoder pops operands swapped (PP4) | FV3 |
| MUT-20 | equation grammar accepts trailing content (PP5) | EQ4 |
| MUT-21 | paren unwrap under bar/vinculum dropped (PP5) | EQ2, EQ3 |
| MUT-22 | '/' swallows the whole expression instead of a factor (PP5) | EQ1 |
| MUT-23 | synthesized radical strokes deleted (PP6) | FV7 (probe excludes the vinculum column — the first version didn't and stayed green) |
| MUT-24 | ⁿ√ index dropped (PP6) | FV8 |
| MUT-25 | SUB script raised instead of lowered (PP6) | FV9 (pin reads the script node's relBase — root descent was satisfied by "log"'s descender) |
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
| MUT-42 | derivative frame ignores the order (PP13) | EQ13 (superscript-2 glyphs missing; the FAIL line holds glyph bytes — trust the counts) |
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
| MUT-53 | multiplication dot reverts to the x glyph | FV1 (red only in the COUNTS — glyph bytes suppress the FAIL line) |
| MUT-54 | vinculum weight regressed to 1 px (standard) | P6 (row-2 probe at the vinculum's RIGHT end — near the sign, the glyph's own hook masked it) |
| MUT-55 | stack allowance zeroed (parity) | EQ18/EQ26/EQ27 (all nested evaluation refuses) |
| MUT-56 | MVAR scan hides only the construct NAME again (Bug 1) | EQ29 (the commit rejects the typed equation, error 45) |
| MUT-75 | negative paint x left unclipped (r3) | T29 (the fill-drawn rule is dropped instead of clipped) |
| MUT-76 | the `.d` exemption removed from the containment guard (r3) | **UNFALSIFIABLE from the harness — documented gap, not a coverage hole.** The guard lives in `executeFunction`, which is `static` and whose only entry point hard-codes its item argument, so no test can drive a chosen key through it. Verified by trace instead: UP/DOWN are matched earlier in the same chain (`item == ITM_DOWN_ARROW \|\| item == ITM_UP_ARROW`), ENTER/EXIT/BACKSPACE have their own `case ITM_...` upstream, and `.d` has neither — which is why it needs the exemption. |
| MUT-57 | prettyReset stops restoring the T-line default (PP15) | FV14 |
| MUT-58 | cold start restores factory defaults again (the PP11 persistence bug) | FV16 (both flags clobbered) |
| MUT-59 | the `-MNU_PP` slot in menu_DISP reverted to ITM_NULL (PP15) | FV15 |
| MUT-60 | the softkey state indicator frozen to unchecked (PP16) | FV17 (ink margin collapses) |
| MUT-61 | complex terms silently dropped in SUM/PROD (PP16) | EQ30 (both halves: result goes real, and the CPXRES refusal stops) |
| MUT-62 | infinite sums fall back to invalidate (PP16) | B9 (FAIL line is glyph-suppressed — trust the counts) |

Two lessons from the first mutation run, kept for honesty: (a) the original
MUT-4 ("drop the dtReal34 type gate") stays GREEN — a non-real register's
payload read as real34 is NaN-class garbage, so the adjacent range check
declines anyway and the mutation is masked; the type gate stays in the code
as belt-and-braces, but its pin-of-record is the F-identity contract via
MUT-6. (b) A stray paint placed BEFORE the toggle gate is invisible to the
F-pins (both compared renders paint it) — the identity pins only see work
that the disabled path skips.

A pin that stays green under its mutation is decoration and gets deleted or
fixed — the forth-core 2026-07-21 rule applies unchanged.

### Visual-polish additions (post-PP14, Stan's review)

- **P5** — the integral sign's hooks reach ≥3 px sideways from the
  spine at the tip rows, both directions, spine present at mid-height.
- The Σ/∏ letterforms follow the FONT'S OWN designs scaled (the glyph
  bitmaps were rendered and read back as the reference): width tracks
  height at the numeric Σ's 9/16 ratio via ppBigopBox (shared by
  measure and paint — the fixed 16 px width left tall operators
  pinched), bar thickness height/10, the Σ apex at 40% width, the ∏
  bar overhanging inset legs. B8's probe narrowed to the scaled box.
- M1/M2 descent moved 5→6 and M5 ascent 29→30: the symmetric-clearance
  fix (descent counts rows at/below a baseline, ascent strictly above,
  so the bar's lower side was one row tighter than its upper side —
  the bar read as cutting the denominator's / radicand's top). P4 and
  S2 row pins shifted accordingly (vinculum up 1 at a fixed baseline;
  the taller PSHOW fraction re-centers one row up).
- EQ1/EQ3/EQ10-13/EQ20/EQ21 pin the X-to-x typeset convention (the
  canonical variable X renders as the classic lowercase x in the
  pretty views only; other names keep their letters — an early draft
  lowercased EVERY single letter and turned A+B into a+b, caught by
  EQ2 staying pinned to caps).

### Ultimate-nesting additions (stress round, Stan's request)

- **EQ22** — the full tower `INTEG(DERIV(SUM(√x/(x+1))/(x×(x+1))
  ×PROD(1+1/(2+x²))))` parses, measures, uses ≥45 pool nodes and fits
  the EQSHW band at full size. Capacity raised for exactly this class:
  PP_MAX_DEPTH 6→12 (the pool is the real bound), PP_POOL_NODES
  48→72 (+384 B BSS; the DERIV layer alone measured 58 nodes).
- **EQ23-EQ25** — the stored-alphabet arms: '^' builds a real 2D
  superscript, a NAME: label prefix skips, an additive construct body
  scopes in parens. EQSHW now reads the STORED equation text —
  showEquation's display string is built for the 400 px strip and
  TRUNCATES long equations with an ellipsis, which the strict parser
  then rightly declined: the ultimate demo found EQSHW silently capped
  at strip-width formulas. Construct limits/scripts always typeset
  TINY (the operator convention; tinyF only governs the strip's
  fraction shrink, and the '/' deep-refont is skipped when the
  caller's fonts are equal so it cannot flatten them).

### PP16 additions (the deferred three, closed)

- **EQ30** — a construct body that evaluates COMPLEX now accumulates
  complex, on upstream's own terms: `SUM(X×i;X;1;3)` = 6i with CPXRES
  set, and the SAME formula is a domain error with CPXRES clear, which
  is exactly what upstream's `_programmableSumProd` does. DERIV and
  INTEG still refuse complex — not a gap, because upstream's own
  differentiator and integrator contain no complex handling at all
  (verified: zero `dtComplex34` references in either file).
- **B9** — the early-stop sum (`ITM_SIGMAnINF`) captures like any other
  sum: `inf` changes only when the loop gives up, not what it reads, so
  the stack effect is identical and its node carries the real limits the
  user supplied. GUARDED by `#if defined(OPTION_INFSUMS)` — without the
  option that item is an unimplemented stub that moves no stack, and
  classifying it would mint a node for an operation that never ran.
- **FV17** — the two toggles in our own menu show their state. Rendered
  twice with the flag opposite, comparing ink in the PPON softkey cell:
  measured 291 lit (filled) vs 258 (outline), and the assert requires a
  margin of 8 rather than a literal count, per the harness rule that a
  font change must never turn a test red.

### PP15 additions (both toggles are flags; the menus are wired)

- **FV14** — the T line is a real flag: `prettySetTline` and the toggle
  item both move `FLAG_PTLINE`, and a reset leaves it OFF. Note the
  ASYMMETRY with FV13: the two defaults are reached by opposite routes,
  because a reset's flag wipe already IS the T line's default while the
  master toggle's default-ON must be re-established.
- **FV15** — the softmenu claims are wired, checked by LOOKING at the
  live `softmenu[]` table rather than trusting the patch: `-MNU_PP`
  resolves, holds exactly its six items in order, and both parent slots
  (`menu_DISP`, `menu_EQN`) contain what §7 says they contain.
- **FV16** — the cold-start path initialises package data WITHOUT
  touching the user's flags. This pins a latent PP11 bug that FV14
  surfaced: `prettyReset()` was called from five lazy-init sites as well
  as from `doFnReset`, so the first dispatch after a cold start
  force-set the master flag and silently overwrote a saved preference —
  the persistence PP11 claimed to deliver. Init and factory-reset are
  now separate functions (`ppcInit` vs `prettyReset`), and
  `ppcTestDeinit` (package-internal, test only) re-arms the cold-start
  path so the contract is testable at all.

### Keyboard-journey addition (Stan's challenge: can it even be typed?)

- **EQ29** — the whole user journey through the REAL key path: types
  `SUM(X;X;1;3)` one softkey at a time into the equation editor (with
  the alpha punctuation catalog open and `fnKeyInCatalog` set, exactly
  as a softkey press leaves it), runs the commit ENTER runs
  (`setEquation` + the MVAR parse), then evaluates to 6. Closes the gap
  that every other EQ test builds its equations from C and so never
  proved a user could enter one. Harness note: `reallyRunFunction`
  passes the CALLER's param, so driving a character item through it
  calls `addItemToBuffer(NOPARAM)` — a bug-screen path that silently
  inserts nothing. keyboard.c calls `addItemToBuffer(item)` directly;
  drive it the same way. Three probe rounds were lost to that.

### Render/eval parity additions (Stan's ruling: eval must match render)

- **EQ26** — `INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;1;2)` evaluates
  through the real fnEqCalc path to 7/24 within 1e-10 (~2.5 s sim).
  The limits [1,2] are chosen to avoid an UPSTREAM second-derivative
  defect near zero (DESIGN.md table), not to flatter the feature.
  The full reference tower (Σ√-fraction / multiplication × ∏, inside
  d²/dx², inside ∫ over [1,2]) was measured once: 0.18234918164357208…
  against the analytic value — 16+ digits in 27.8 s sim; recorded here
  rather than pinned (the gate stays fast).
- **EQ27** — `INTEG(INTEG(X;X;0;1);X;0;1)` = 0.5: nested integrals
  work (the new DEI path never increments the engine counter).
- **EQ28** — the upstream near-zero second-derivative defect and its
  upstream-native remedy: the same integral over [0,1] that returns
  −2.947e23 at default settings returns 5/6 to 31 digits once the
  derivative's own step variable `δ_d` is set. The test restores the
  variable to zero (which the engine reads as "unset") so it cannot
  leak into later suite files.
- Debugging trail worth keeping: the differentiator's entry parse runs
  MVAR mode, which errored on every construct (';' is a base-grammar
  error) — MVAR now consumes whole construct spans; temp-slot appends
  refuse under a PENDING error (the rollback previously fired on
  errors it did not cause); and the zero-limit DERIV-integrand caveat
  was isolated by a per-sample probe showing the stencil collapse at
  x ≈ 1E-24 abscissas.

## Blast radius and measurements

- Keep the pre-change testSuite log; diff sorted `PASS:` lines after.
- BSS delta measured at every stage gate (`size` on the sim binary,
  before/after); the §8 budget in DESIGN.md is the ceiling.
- Flash: `make dmcp5r47 CUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print
  CUSTOM_PKG_RECONFIGURE=1` delta recorded in the stage commit (without the
  reconfigure flag the number is the previous tree's).
- Visual confirmation via the run-sim skill's capture driver (not a gate;
  the pixel pins are the gate).
