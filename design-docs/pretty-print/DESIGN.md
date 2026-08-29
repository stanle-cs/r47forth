# Pretty-print package — design

This document is authoritative for `packages/pretty-print/`. Amendments and
rejected shapes go to DESIGN-HISTORY.md; the test contract lives in TESTING.md.

Pretty print means **natural display of calculations** (ruled 2026-08-26):
textbook-style rendering of values (stacked fractions with a real bar, √ with
a vinculum, raised exponents) and of the user's chained RPN operations
reconstructed as infix formulas (`2+3 = 5`), eventually including 2D display
of EQN formulas. It is an external package with the same structure and
discipline as `packages/undo-history/`: flat working area mirroring upstream
paths, generated `patches/`+`files/`, gate at
`./packages/pretty-print/build-test.sh` (solo + combined passes).

Upstream baseline facts the whole design leans on: value rendering is strictly
one-baseline glyph strings (`display.c` builders → `showString`); fractions
render *diagonally* (sup-digits `/` sub-digits, `fractionToDisplayString`,
display.c:1684); `STD_SQUARE_ROOT` has no overbar; there is no 2D layout
anywhere except the matrix editor. Everything 2D in this package is therefore
new code painting through the existing pipeline (`showString` at explicit
(x,y)) plus `lcd_fill_rect` bars.

## §1 The layout engine (`prettyLayout.c`)

A minimal box model. Six node kinds:

| kind | children | meaning |
|---|---|---|
| `PP_RUN` | none | glyph string, one baseline, drawn by `showString` |
| `PP_HBOX` | n | children side by side on a shared baseline |
| `PP_FRAC` | num, den | stacked fraction, painted bar |
| `PP_RAD` | radicand | radical sign + painted vinculum |
| `PP_SUP` | base, exponent | raised exponent (real superscript placement) |
| `PP_PAREN` | inner | parens; degenerate to glyphs, synthesized when tall (PP4) |

Node = 16 bytes, index-linked (`firstChild`/`nextSibling`), living in fixed
BSS pools: `ppNode_t ppPool[48]` (768 B), `char ppText[512]` (run text),
`char ppLeafScratch[200]` (upstream-builder output). Max nesting depth 6,
enforced at build time (bounds measure/paint recursion). No heap, no
resident-pool use, ever.

Each `FRAC`/`RAD`/`SUP` node carries a **context font id**; a const flash
table `ppMetrics[3]` (numeric/standard/tiny) holds: box ascent (36px font: 28,
22px: 16, tiny: 8), the **math axis** — calibrated to each font's minus-sign
ink so a fraction bar sits exactly where a minus sign does (numeric: rows
B−11..B−10, bar thickness 2; standard: B−6, thickness 1; tiny: B−4) — bar
overhang, gaps, vinculum thickness, superscript drop.

Two passes. **Measure** (post-order) computes width/ascent/descent per node,
with run ink extents taken from real glyph metrics (`findGlyph`,
`rowsAboveGlyph`/`rowsGlyph`). **Paint** draws runs via
`ppShowRun()` (see the clearing-extent rule below) and
bars/vinculums via `lcd_fill_rect(..., LCD_EMPTY_VALUE)` — the same call
shape as `drawSinglePixelFullWidthLine` (screen.c:1554). The radical glyph is
painted raised so its top row meets the vinculum; radicands taller than the
glyph get a synthesized sign (integer DDA over `setBlackPixel`).

**Paint-order rule (BINDING, found by pin P1 on 2026-08-26): every painted
rule (fraction bar, vinculum) goes AFTER the glyph runs it neighbours.**
A glyph painted with a font-box pre-clear erases padding rows
(`rowsBelowGlyph`/`rowsAboveGlyph`) that reach past its ink into the bar
band even when the ink honours the gap — a bar painted first is wiped under
every digit column, leaving only its overhang pixels. Measure-pass gaps are
ink-relative and correct; only the paint order compensates.

**Scope of that rule since R3-13 (audit R5-1).** The justification above
was written when every run went through `showString`. It now applies to
exactly one run — the radical sign glyph, the sole remaining `showString`
call — because `ppShowRun` gives all other runs `noPreClear` and bounds
their clear to the measured box. For a fraction the clears provably cannot
reach the bar (the numerator's stops `fracGap+1` rows above it, the
denominator's starts `fracGap+2` below), so the ordering there is retained
for uniformity, not because it is load-bearing. Keep the rule; do not
re-derive its reason from the fraction case, which no longer demonstrates
it.

**Softkey containment is a RANGE, and it is not ours (BINDING, audit
R5-3).** A modal browser must stop softkeys executing underneath it.
Upstream does this in THREE places — `btnFnPressed`, `btnFnReleased` and
`executeFunction` — each enumerating its own browsers by name
(`CM_REGISTER_BROWSER`, `CM_FLAG_BROWSER`, `CM_ASN_BROWSER`,
`CM_FONT_BROWSER`). A package browser is invisible to that list. Both
sibling packages therefore carry the byte-identical clause
`&& calcMode < 19 /* package browsers 19-23, claims registry */` on all
three lines, and 3-way merge unifies them (the same identical-edit claim
the `NUMBER_OF_SYSTEM_FLAGS` line uses).

This package carried the guard only in `processKeyAction`, which is the
DIRECT-key half of the driver. In the combined build undo-history's range
edit covered us anyway, so the hole was invisible; in the SOLO build —
one of the two gated configurations — pressing F3 while browsing ran
`PCLR` and wiped the history being browsed, with the browser repainting
over the evidence. Found by the round-5 fix review, which correctly
flagged that it could not tell whether a sibling closed it. Never rely on
a sibling for containment: the range clause is now in this package's
`keyboard.c` too, and FV19 pins `CM_PRETTY_BROWSER` inside 19..23, since
renumbering it out of that range would silently reopen the hole with
nothing else going red.

**Clearing-extent rule (BINDING, audit R3-13): a node clears exactly the box
it measured, and never more.** Glyph runs go through `ppShowRun()`, which
clears the measured ink box with `ppFillVal(…, LCD_SET_VALUE)` and then
paints via `showGlyphCode` with `noPreClear` TRUE. Calling `showString`
directly from paint is a defect: its per-glyph font-box clear is larger than
anything measure reasoned about, so nodes measure-placed to sit clear of each
other still erase each other. The case that found it: a fraction denominator's
baseline rises as its ink shortens, so its font box crosses the bar into the
numerator — an '8' numerator kept 52 lit rows over an '8' denominator, 38 over
an 'x', 20 over a '.'. Pin P12 holds the three equal. The rule also keeps a
run packed against a band edge from clearing frame rows outside that band.
The one deliberate exception is the radical sign glyph, which is alone in its
columns; the paint-order rule above still governs its vinculum.

**Font ladder per surface**, rebuild-per-rung until the layout fits:
- `PP_SURF_INLINE` (register line, 36 px band): numeric ctx / standard
  children → standard/standard → standard/tiny → fail.
- `PP_SURF_FULL` (PSHOW, 147-row band): numeric/numeric → numeric/standard →
  standard/standard → standard/tiny → fail.
- `PP_SURF_BAND` (browser row): standard/standard → standard/tiny → fail.

The top-level renderer right-aligns and lets the baseline float inside the
band (clamped by ascent/descent). Verified pixel budgets: every register line
has 32 rows above the numeric baseline and 7 below (X line: 10). A proper
fraction at the first inline rung measures ascent 25 / descent 5 — fits with
no float; √2 ascent 29 — fits; a two-level `(a+b)/c` in standard/standard is
23/12 — fits with a ~5 px float. Full screen takes three nesting levels in
numericFont.

**Fallback rule (BINDING).** Every pretty path is a `bool_t` try-function.
Any failure — unsupported type, unexpected glyph, pool/text/depth overflow,
doesn't fit any rung — paints nothing and returns false, and upstream's own
arm renders unchanged. The renderer never reads or writes `tmpString`; on
fallback the upstream path is provably untouched. Overflow is never an error
screen; it is a legitimate "too complex to pretty-print".

## §2 Value converters (`prettyValue.c`)

**Builder-first invariant (BINDING).** The converter always calls the
upstream `…ToDisplayString` builder first — into `ppLeafScratch`, with the
same arguments the upstream display arm would use — and then *parses the
builder's output* into a layout tree. This preserves `displayValueX` and
every formatting decision (digits, separators, PROPFR/DENFIX, shrink-to-fit)
byte-for-byte, and it means the pretty form can never disagree with what
upstream would have shown. Parse failure → fallback rule.

- Fractions (PP1): parse `fractionToDisplayString` output. Closed alphabet:
  optional `< = >` prefix, sign, plain-digit integer part, `STD_SUP_0..9`
  numerator (0xa160+d), `/`, `STD_SUB_0..9` denominator (0xa080+d), separator
  glyphs. Tree: `HBOX[prefix?, int?, thin-space?, FRAC(num, den)]`.
- Exponent reals (PP2): split `real34ToDisplayString` output at the
  `PRODUCT_SIGN STD_SUB_10` marker (emitted by `exponentToDisplayString`,
  display.c:127); sup-digit tail becomes a `PP_SUP` exponent run. No marker →
  plain real → false (upstream renders).
- IRFRAC symbolic forms (PP2): parse the `checkForAndChange` output alphabet
  (√, π, e, φ, sup/sub digits, `/`, parens — table at display.c:516-532) into
  `PP_RAD`/`PP_FRAC`/`PP_SUP`.
- Complex (PP2): split at the imaginary-unit boundary; both parts recurse
  through the parsers above; polar stays linear.
- Long integers, strings, matrices, short integers, dates, times, configs:
  never pretty — immediate false.

## §3 The capture engine (`prettyCapture.c`)

**Invariant (BINDING): shadow slot k always holds an expression whose value
equals the live contents of register `REGISTER_X + k`. When a transform
cannot maintain that, the slot degrades to a value leaf snapshotted from its
register (truthful by construction) or the whole shadow invalidates. The
display never lies; over-invalidation only costs history granularity.**

State (all static BSS, ~640 B + 692 B ring): `ppArena[24]` of 24-byte nodes
(op/literal/value/register/constant/opaque), shadow slots `ppSlot[8]` + L,
current-formula root, pending-lift/enter latches, NIM text snapshot, staged
transform. Literal leaves store **as-typed text** (`2.50` stays `2.50`);
value leaves store raw register payloads ≤16 B (complex via a two-child
header), formatted only at display time by staging into `TEMP_REGISTER_1` and
calling the standard display builders — the same lazy-preview rule
undo-history pinned. Matrix/string/oversized payloads become `PPN_OPAQUE`,
which poisons the containing tree into never-being-shown.

**Two-phase operator mirroring.** Hooks sit around the verified dispatch site
in `reallyRunFunction` (items.c: `stackWatermarkBeforeDispatch();` /
`indexOfItems[func].func(param);` / `stackWatermarkAfterDispatch();`, ~:412):
STAGE classifies the item and upgrades any UNKNOWN operand slots from the
pre-op registers; DONE applies the staged transform only when
`lastErrorCode == ERROR_NONE`, else discards it and invalidates the shadow (a
failed function may have partially moved the stack).

**Number entry** is mirrored at the `closeNim` funnel, not at NIM open: the
NIM-open hook only latches `FLAG_ASLIFT` (the flag is consumed by
`closeNim`'s own head before commit); the leaf push plus lift-or-overwrite
happen at `closeNim_exit` when the commit succeeded
(`calcMode != CM_NIM && lastErrorCode == 0` — upstream's own commit
predicate). A NIM aborted by backspace-to-empty runs upstream `undo()` and
needs no shadow rollback, because nothing was applied. A head hook snapshots
`aimBuffer` before `closeNim` mutates it (base suffixes, `#` truncation).

**Coverage classifier.** A hand table (verified: no usable arity metadata
exists upstream — `EIM_DY` shares its bit with `RESULT_IN_X` and is
vestigial): dyadic and monadic op lists mirroring `isDyadicFunction`
(equation.c:839), stack motions (ENTER incl. the eRPN no-dup branch, x<>y,
R↑/R↓, CLX, DROP, CLSTK, FILL, LASTx, RCL/STO, x<>reg). **Default rule
(BINDING): an unknown item that is undo-enabled (`US_ENABLED`/`US_ENABL_XEQ`)
invalidates the whole shadow; an unknown `US_UNCHANGED`/`US_CANCEL` item is
ignored** — upstream maintains that annotation for its own undo correctness,
so it maintains our invalidation predicate too. Hand exceptions (stack
mutators that are `US_UNCHANGED`): `ITM_UNDO`, undo-history's REDO (item
428). Capture scope: manual interactive only — not `PGM_RUNNING`, not
`FLAG_SOLVING`/`FLAG_INTING`, calcMode `CM_NORMAL`/`CM_NIM`.

### Big operators (PP12)

`PPN_BIGOP` captures a Σₙ/∏ₙ (and integer variants) or ∫yx dispatch:
`item` = the ITM id, `pad[0..1]` = the LABEL id shown in the body
(display-time best-effort decode through `labelList`; a stale id falls
back to `LBL nn`), `payload` = the step real34 for sums (the ∫ stores
the integration-variable id in payload[0..1] instead), `child[0]/[1]` =
from/to VAL leaves snapshotted PRE-op. Binding semantics:

- Sums capture only the direct label-param form (`FIRST_LABEL..LAST_LABEL`);
  the register-letter form resolves a label indirectly and invalidates.
- The ∫ captures only the dispatch that actually integrates: a
  named-variable param over a preselected label program
  (`!USES_FORMULA && currentSolverProgram < numberOfLabels`). The
  label/register param is the interactive SETUP form — it still harvests
  X,Y into ULIM/LLIM and drops them but leaves NO result, so it
  invalidates rather than mint a node that would display a lie. Formula
  targets belong to the EQN surface (PP13), not capture.
- Limits are consumed by VALUE, never by structure: an op tree in a
  consumed slot displaces (emits) at STAGE, and the current root is
  superseded unconditionally — the new root never contains it.
- After DONE the label program has run with the machine's full keyboard:
  slot 0 holds the BIGOP (its value is the result in X), every other
  slot and slot L go UNKNOWN and re-materialize lazily as VAL leaves.
- A non-unit step must be visible in the under-limit or the display
  lies (`n=from,Δstep`); step 1 renders as plain `n=from`.

`PP_BIGOP` (layout kind 9) carries children body/under/over; `textOff`
holds the operator ITM id (`ppSetBoxTag`) and the paint arm picks the
stroke glyph (Σ chevron+bars, ∏ bar+verticals, ∫ the PP_INT shape) —
strokes AFTER children per the binding paint-order rule. `PPT_TKBIG`
serializes postfix as from-VAL, to-VAL, then {item u16, label u16,
payload 16B}, popping two.

The capture hooks are nesting-safe: `prettyNoteFunction` checks scope
BEFORE touching the stage, because a BIGOP's label program runs every
step through `runFunction` (under FLAG_SOLVING/PGM_RUNNING) and an
unconditional stage clear would destroy the outer op's staging.

## §4 Segmentation — where a formula ends (RULED 2026-08-26)

**The rule: liveness + new-root supersession.** A formula (an op-rooted tree)
is emitted to history when either
1. **displacement** — its root leaves the shadow stack unconsumed:
   overwritten in X, dropped, wiped (CLX/CLSTK/invalidation), swapped out to
   a register, or pushed off the stack top by lifts; or
2. **supersession** — a new operator node is created whose operands do *not*
   include the current root. The old formula emits immediately (its
   `= result` read from the register that still holds it — the §3 invariant
   guarantees truth), stays on the shadow stack flagged EMITTED, and can
   still be consumed later to continue a larger formula (flag cleared on
   consumption, so nothing emits twice).

ENTER never terminates a formula — pressing ENTER on a result to duplicate
and continue (`2 ENTER 3 + ENTER ×` = (2+3)²) is a continuation idiom that
an ENTER-terminates rule would break, and the operand-separator ENTER is
already invisible to segmentation. Bare literals/values/RCL leaves never emit
(a number alone is not a formula). UNDO discards the current formula (the
user revoked it) but never un-emits history; a formula, once finished,
happened. Consequence accepted: undo-then-redo can eventually duplicate a
history entry.

Reference traces (normative; TESTING.md pins them):
- `2 ENTER 3 + 4 ×` → one formula `(2+3)×4`, nothing emitted early.
- `2 ENTER 3 + 5 ENTER 6 +` → at the second `+`, the new root doesn't
  contain `(2+3)` → history `2+3 = 5`, current `5+6`.
- `2 ENTER 3 + CLX` → displacement → history `2+3 = 5` (CLX is the natural
  explicit terminator).
- `5 1/x 3 +` → single formula `1/5+3`.
- `12 SIN` → current `sin(12)`.
- `2 ENTER ENTER ×` → `2×2` (ENTER dup mirrored as deep copy).

## §5 History ring and display handoff

Finished formulas are serialized **eagerly as postfix token streams by pure
byte copies** — literal (as-typed text), value (raw payload), register,
constant, op tokens, plus a result snapshot. No display formatter runs at
capture time; formatting stays lazy (§3), which keeps both the letter and the
spirit of the no-strings-at-capture rule while letting the arena free the
tree on emission. Ring: `ppHist[640]` + 12 offsets, oldest-first eviction
(undo-history's eviction shape). Oversized entries (> half the ring) are
dropped, not stored.

Renderer-facing API (all lazy): `ppCurrentFormulaRoot()` (+ read-only arena
access), `ppHistoryCount()` / `ppHistoryEntry(idx, &len, &seq)`,
`ppHistoryClear()`. The renderer owns the item-id → infix-form table
(precedent: print.c:21-38) and precedence-driven parenthesization.

## §6 Surfaces

- **Inline register lines** (PP1): one hook arm in `_refreshRegisterLine`,
  inserted immediately before the `/*Main type dtReal34 FLAG_FRACT*/` arm
  (screen.c:3936): `else if(prettyTryRegisterLine(regist, baseY)) { }`. All
  gates live package-side: toggle on, `CM_NORMAL`, `TI_NO_INFO`, no error, no
  view-register prefix in play, **`!checkHP`** (HP layout doubles glyph rows
  inside `showGlyphCode`, invalidating all metrics — HP users get upstream
  rendering), supported type. Output is clipped to the line's own cleared
  band (`baseY−4 .. baseY+35`, X line `+38`); doesn't fit → false.
- **`PSHOW` full screen** (PP2): a manual-paint item (row 459) using the
  `fnPixel` protocol verbatim (`screenUpdatingMode |= SCRUPD_MANUAL_STACK |
  SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS; screenHoldsDrawnPixels =
  true;` — screen.c:6552-6560): pixels survive refresh, the next keypress
  releases them. Zero keyboard.c churn, zero screen.c hunks, status-bar rows
  0..15 untouched so the clock keeps ticking. Builder failure falls back to
  `fnC47Show(NOPARAM)` — the user always gets a SHOW.

  BINDING (2026-08-27, owner-reported): a surface that paints its own
  screen also **declares** it one, by setting `temporaryInformation =
  TI_SHOWNOTHING` beside `screenHoldsDrawnPixels = true`. Holding pixels
  is not what upstream dismisses on — its EXIT arm tests
  `temporaryInformation != TI_NO_INFO || showScreenDismissed`
  (keyboard.c), and `showScreenDismissed` latches from SHOWMODE, itself a
  temporaryInformation test. A surface that omits it draws correctly and
  then cannot be closed with EXIT. The convention is upstream's own
  matrix SHOW, which paints its screen and then sets TI_SHOWNOTHING
  ("then tell the system it is in show nothing mode", display.c).

  Scope, stated because the obvious generalisation is wrong: this binds
  the surfaces upstream dismisses, which is every self-painted screen
  raised in CM_NORMAL — today PSHOW and EQSHW. The PHIST pager also
  holds pixels but is raised INSIDE `CM_PRETTY_BROWSER`, where the
  package's own containment routes every key and `prettyBrowserLeave`
  does the dismissing; upstream's EXIT arm never sees it. It deliberately
  does NOT declare itself, and declaring it would break its paging, which
  reads `screenHoldsDrawnPixels` to tell a repeat press from a fresh one
  — a SHOWMODE screen has that cleared for it on the next press. A
  self-painted surface therefore follows the dismissal contract of the
  mode it is raised in, and a new one must say which that is.
- **Formula view** (PP4, RULED 2026-08-26): a PAGER, not a browser mode.
  `PHIST` (row 462) renders the current formula plus the finished-formula
  history as 2D infix — division stacked, powers raised, roots under
  vinculums, precedence parens (glyph or synthesized-tall) — four rows per
  page on the same manual-paint protocol as PSHOW. Repeated `PHIST`
  presses page forward; any other key releases the screen. `PCLR` (row
  461) clears the ring. Value leaves and results format at display time
  via TEMP_REGISTER_1 staging (undo-history's lazy-preview recipe),
  never at capture. Zero keyboard.c/defines.h churn; the full
  CM-mode browser (selection, per-row scrolling) remains an explicitly
  possible upgrade on reserved calcMode 20 — it was traded away because
  keyboard.c is the project's riskiest three-package composition surface,
  not because the pager is the end state.

### Solver-surface frames (PP13)

EQSHW frames the equation with the interactive solver's own numbers:
integrate mode shows a PP_BIGOP ∫ whose under/over limits are the REAL
`RESERVED_VARIABLE_LLIM/ULIM` values and whose body appends ` d<var>`
(the variable name decoded live via `ppfVariableName`); without
`SOLVER_STATUS_INTERACTIVE` (or non-real limit registers) it falls back
to PP7's bare stroke ∫. The derivative modes prefix `d/d<var>` (first)
or `d²/d<var>²` (second) with the equation in tall parens. Solve
framing (`f(x)=0`) is deliberately absent:
`SOLVER_STATUS_EQUATION_SOLVER` is the zero value of the mode field, so
a stale INTERACTIVE bit would frame a plain view with an `= 0` the user
never asked for — un-determinable state stays unframed.

### Equation-language big operators (PP14) — RULED design

**Render and eval nesting limits MATCH (ruled on Stan's request).**
The old fixed eval-depth cap of 2 is replaced by a measured
stack-consumption guard: the outermost construct records the stack
pointer, and every deeper construct or delegate refuses cleanly once
consumption exceeds PPEQ_STACK_ALLOWANCE (8 KB; the reference tower
high-waters 5.3 KB on the 64-bit sim, ARM frames are smaller). The
depth cap (8) remains only as a runaway backstop. **Known UPSTREAM defect (measured 2026-08-26, corrects an earlier
mis-statement here).** `fn2ndDerivEq` sizes its sampling step as a
FRACTION of the evaluation point — upstream's own comment at
differentiate.c:478 says "the step is relative to x, and at x = 0 it
collapses", and an absolute fallback covers exactly zero. At small
NONZERO points the step underflows against the function's own scale,
the second difference cancels to noise, and dividing by h² amplifies
it. Measured on the plain formula `6/(X+2)` through the built-in
second derivative, NO package code involved (true value ≈ 1.4978):

| x | d/dx (true ≈ −1.5) | d²/dx² (true ≈ 1.5) |
|---|---|---|
| 1 | −0.666… ✓ (true −0.667) | 0.444… ✓ (true 0.444) |
| 0.001 | −1.4985 ✓ | **−1511.79 ✗** |
| 1e−8 | −1.49999998 ✓ | **−9.28e7 ✗** |
| 1e−12 | −1.4999999999985 ✓ | 1.5000000 ✓ (lucky) |
| 1e−16 | −1.49999999999999985 ✓ | **1.51e29 ✗** |
| 1e−24 | −1.50000000 ✓ | **0E+17 ✗** |
| 1e−40 | **0E+41 ✗** | **0E+82 ✗** |
| 0 | −1.4999999999999999999 ✓ | 1.5000000000000000002 ✓ |

So exactly zero WORKS and small nonzero values FAIL — the reverse of
what was written here before. The first derivative is robust to
~1e−24; only the second derivative is fragile, which is what the h²
division predicts. The failing band for this function begins below
about x = 0.01 (0.01 is exact, 0.005 already returns 4.28 for a true
1.4888) — which is why upstream's own AN0022 derivative plots, which
sample −5..5 at roughly 0.025 intervals, never meet it.

**There is an upstream-native remedy, and it is complete.** The
derivative honours a user-set step in the named variable
`δ_d` (`deriv_user_step`, differentiate.c:240 — exposed as the Δ
softkey in the derivative menu). With it set, the relative-step
collapse cannot happen. Measured on the case this package cares about,
`INTEG(DERIV(SUM(X;X;1;3)/(X+2);X;X;2);X;0;1)`, true value 5/6:

- default relative step → **−2.947e23** (garbage)
- `δ_d` = 0.001 → **0.8333333333333333333333333332992391** in 266 ms

EQ28 pins that remedy. So the guidance is: for a derivative sampled
near zero — which any `INTEG(DERIV(...))` over a range touching zero
will do — SET THE STEP, or keep the range away from zero.

Status: a genuine, silent, user-reachable UPSTREAM defect (the
built-in d²/dx² key at X = 0.005 answers confidently with nonsense at
default settings), fully mitigable with an existing built-in control
that the application notes never connect to this failure. Not the
package's to patch — and not worth patching around, since setting the
step is the upstream-convention answer. UPSTREAM-REPORTABLE.
Nested INTEG-in-INTEG works: the new double-exponential path never
increments the engine counter, so upstream refuses nothing; the stack
guard is the bound, and a refused engine (old path, defensive) turns
into a clean error rather than a stale-X read.

**Separator choice — evidence, not assertion (verified 2026-08-26 on
Stan's challenge).** `,` is UNAVAILABLE: the parser rewrites every comma
inside a number to `.` (equation.c:1191), so `1,5` IS the number 1.5 and
a comma-separated argument list would be silently ambiguous. `;` is
free: the grammar rejects it with a dedicated error ("cannot be appeared
in equations"). There is no upstream convention to conform to, because
upstream has NO working multi-argument call syntax — `MAX`, `MIN` and
`atan2` are in the alias table but every shape tried
(`MAX(3,5)`, `MAX(3;5)`, `3 MAX 5`, `MAX(3)(5)`, `MAX(3 5)`,
`3 MAX(5)`, `MAX 3 5`, `MAX(3)+0`) fails, most with
ERROR_ITEM_TO_BE_CODED. **Forward-compatibility risk, OPEN:** that error
is a statement of intent, so upstream will one day pick a separator; if
they pick anything but `;` this package's syntax becomes foreign on
their own machine. Ask them before this reaches users — the question
belongs with the derivative report.

Typeability is CONFIRMED, not assumed: `;` lives in the ALPHA
punctuation softmenu (`menu_alphaMisc`, beside `.` `,` `:`) and the
catalog branch of the equation-mode gate admits it. EQ29 types
`SUM(X;X;1;3)` one softkey at a time, runs the commit ENTER runs
(setEquation plus the MVAR parse) and evaluates it to 6 — MUT-56 (the
Bug 1 shape) turns it red with syntax error 45, which is also proof
that Bug 1 would have made these constructs impossible to SAVE from the
keyboard, not merely wrong under a derivative.

Syntax (parse-level, package-side): `SUM(body;var;from;to[;step])`,
`PROD(body;var;from;to[;step])`, `DERIV(body;var;at[;order])`,
`INTEG(body;var;from;to)`. The separator is `;` — today a hard parse
error, so the syntax space is free, and unlike `,` it can never collide
with a radix mark. The constructs nest (a SUM body may hold another);
depth caps at 2 and errors beyond.

BINDING, spelling (2026-08-27, owner-reported): a construct answers to
its **all-upper and all-lower spellings and nothing else** — `SUM(` and
`sum(`, never `Sum(`. This is upstream's convention, not a choice:
`functionAlias[]` carries both spellings of every name a user types into
an equation ("sinh" beside "SINH", "asinh" beside "ASINH",
solver/equation.c) because `compareString`'s CMP_NAME folds superscript,
subscript and struck forms but never case (sort.c:137). The test lives
in exactly one place, `ppEqConstructIs` (prettyPrint.h), and BOTH the
renderer and the evaluator call it. They must: a spelling one accepts
and the other declines gives the user a number with no picture, or a
picture that will not compute, with no error either way. Whoever adds a
construct adds it to both and pins the same spelling through both.

Machinery (one hook line at the top of parseEquation's scan loop + an
appended block in the same file):

- **Interception.** In XEQ mode the hook consumes the whole
  `NAME(...)` span (paren-depth scan finds the close), slices the
  arguments at top-level `;`, and pushes one value onto the OUTER
  numeric stack; the outer parser state is never touched. In MVAR mode
  it consumes ONLY the name and `(` so the construct name is not
  collected as a variable; the arguments scan normally (the bound
  variable appears in the menu — harmless, documented).
- **Slice evaluation.** parseEquation's entire state lives in its
  caller's mvarBuffer, so nested evaluation is re-entrant by
  construction with private buffers. A slice becomes a HIDDEN formula
  slot appended at the LIST END (own appender — fnEqNew opens the
  editor and moves currentFormula; deleteEquation's tail-delete is
  side-effect free for user slots, but resets currentSolverVariable,
  which the construct restores). Slice buffers are TRANSIENT pool
  allocations (allocC47Blocks) freed on every exit — zero resident
  BSS. Live blocks never relocate (free-list allocator, no
  compaction), so the outer parse's string pointer stays valid across
  the appends.
- **Loop binding.** The bound variable is written DIRECTLY
  (reallocateRegister + real34Copy), never through reallyRunFunction —
  no dispatch inside the eval. Its prior content is saved and restored
  with the saveRegisterSnapshot idiom (differentiate.c's own).
- **SUM/PROD** accumulate in real_t under ctxtReal75 with
  _programmableSumProd's counter walk (sign-aware termination, the
  same direction guard) — package loop, upstream accumulator
  discipline. Result: real path only in v1; a complex or error result
  from the body aborts the equation with the body's own error.
- **DERIV/INTEG delegate to the upstream engines** (fn1stDerivEq /
  fn2ndDerivEq / _fnIntegrate with a variable param) against the temp
  slot, so the numbers are IDENTICAL to the interactive surfaces. The
  delegate snapshots the outer parse's tmpString regions
  (buffer 1024 B + parser state) into a transient block plus the
  solver globals (currentFormula/SolverVariable/SolverStatus/
  currentSolverProgram, ULIM/LLIM for INTEG) and the X register flow
  is upstream's own (result read from X; the equation's final X
  overwrites it exactly as fnEqCalc would). _fnIntegrate's saveForUndo
  fires per INTEG — one extra undo point, same as the interactive ∫,
  accepted and documented.
- **Rendering** (ppqParse arms): SUM/PROD → PP_BIGOP Σ/∏ with
  `var=from` under (`,Δstep` when a step slice exists — textual, not
  evaluated), `to` over, the body as the operand; INTEG → PP_BIGOP ∫
  with from/to under/over and ` dvar` appended; DERIV → the PP13
  d/dx (d²/dx²) fraction with the body in tall parens and `var=at`
  appended as a subscript-style suffix. Malformed constructs decline
  the whole strip/EQSHW render (strict; the linear line remains).

### Flag scope (RULED, PP15)

`FLAG_PRETTYP` governs only what the calculator draws **on its own
initiative** — the inline register lines. The explicit view commands
`PSHOW` and `EQSHW` ignore it: asking to see something should show it.
`FLAG_PTLINE` is a second, independent opt-in for the T-line live
formula, default OFF.

Init and factory-reset are SEPARATE (PP15, after a latent PP11 bug):
`ppcInit()` prepares the package's own data and is what the lazy
first-use path calls; `prettyReset()` does that AND restores both flag
defaults, and only `doFnReset` may call it. A lazy path that restores
defaults overwrites the user's saved preferences, which is exactly what
persisting them was meant to prevent.

### VISUAL — a program as its mathematics (PP17)

`VISUAL 'DBLINT'` (item 984, `TM_LBLONLY`) draws a stored RPN program as
the mathematics it computes, without running it, **into the Z/T window**
— so the answer the program just left in X stays visible underneath.
Requested on the forum by Jaymos against appnote 22's `func.txt`, whose
chains are the design's reference input, and whose placement request
("draw the integrals in the Z/T window") is part of the specification,
not a detail: the point is seeing the formula and its result together.
Captured 2026-08-28: `XEQ 'DBLINT'` gives 1.333333..., and `VISUAL
'DBLINT'` then draws the nested integral above it.

**Shape: a third front-end, not a third renderer.** The package already
had two producers of one node tree — the capture engine (live dispatches)
and the equation parser (EQN text). `prettyVisual.c` adds a static walker
over stored program steps.

PP17 shipped it as a transpiler to equation-language TEXT which
`ppqShowRender` then re-parsed. **PP18 removed that round trip**, for two
reasons. It settled precedence twice — the walker inserted brackets into
a string and the parser read them back out to rediscover the same
structure — while `ppfCombine1`/`ppfCombine2` had been doing exactly that
job for the capture engine all along. And it made the text grammar a
dependency of drawing, which is the opposite of what an upstream reader
asked for.

The walker now builds a small expression tree and lays it out through the
shared builders: `ppfCombine1`/`ppfCombine2` for operators,
`ppqBuildBigop` for constructs, `ppfWrapIf` for scoping. **Nothing in the
walker decides where a bracket goes.** The drawing came out
byte-identical to PP17's, which is how the change was verified.

Two places where the node form is not merely equivalent to the text form:
a fraction bar SCOPES, so `a/(b+c)` draws with no parentheses at all
(V49); and a stacked power DOES need its base bracketed, which the walker
does locally because `ppfCombine` deliberately has no POW level — a
`PP_SUP` normally scopes itself, and adding a level would change the
contract underneath the capture engine (V51).

**Why a symbolic stack seeded with the variable NAME is faithful.**
Upstream feeds a body program through two channels: the integrator writes
each node into the named d-variable AND fills every stack level with it
(`integrate.c`, `DEI_xeq_user` + `fnFillStack`), while a programmed sum
delivers its counter through the filled stack ALONE (`sumprod.c` — no
named variable). An equation body, by contrast, reads its variable by
NAME (`solver/equation.c`, the XEQ-mode RCL arm). Seeding a body frame
with the variable name on all eight levels reproduces both channels at
once, which is why the transpiled text computes what the RPN computed.

**RETIRED at PP18 — the emitted alphabet.** While the walker drew by
emitting text, that text had to satisfy the renderer AND the evaluator,
which are different parsers: multiplication had to be `STD_CROSS`
(`\x80\xd7`, since `'*'` is accepted by NEITHER), constructs all-upper,
numerals digits and `.` and a leading `-` only. Getting a byte wrong
dropped the whole formula to a linear line with no error — which is what
made it a BINDING rule and MUT-106 its guard.

It no longer binds the drawing path, because there is no longer any text
in it: the walker builds nodes, and a node cannot be mis-spelled. The
rule still governs the **test** back end (whose output one pin feeds to
`fnEqCalc`) and the equation parser itself. Recorded rather than deleted
because the reasoning is what justifies not going back.

**BINDING — fail closed.** An opcode the dispatch table does not name
declines. There is no inferred "harmless item" rule, because the item
table carries no stack-effect metadata to infer from: `TICKS` is the
counterexample that would break one, looking inert and pushing a value no
drawing can predict. The skip list is explicit (`LBL`, `MVAR`, `REM`,
`PAUSE`, `SNAP`).

**The opaque-taint rule.** A value the text cannot spell — a string
literal, an exponent numeral — becomes an OPAQUE placeholder rather than
a decline. Movers (`ENTER`, `x<>y`, the drops, `FILL`, `STO`) carry it
freely; embedders (any operator, a construct's limits or body, the final
result) decline on it. This is what lets appnote 22's own idioms pass
through untouched — `'INT(INT) = 4/3' STO A DROPX` for a plot title,
`1e-8 STO 'ACC' DROP` to set integrator accuracy — while guaranteeing
neither can reach the printed mathematics.

**Decline catalog** (D-numbers reach the user through
`moreInfoOnError`): D1 unsupported opcode · D2 indirect parameter · D3
local label · D4 unresolved label · D5 recall of a name a `STO` changed ·
D6 integral with no `PGMINT` latched during the walk · D7 register read
(the language reads names only) · D8 depth · D9 step budget · D10 stack
underflow · D11 opaque reaching the mathematics · D12 variable collision ·
D13 malformed program memory · D14 unreadable numeral · D15 fragment cap ·
D16 pool exhausted · D17 nothing to show · D18 name the grammar cannot
spell.

**Derivatives (PP18, corrected twice — read the whole paragraph).**
`PGMDRV` latches the program, `f'`/`f"` pop the point. **The variable is
NOT the `f'` parameter.** `calcDeriv` asks `deriv_pgm_variable(label)`,
which walks the BODY program's own leading `MVAR` declarations and
returns the one matching the parameter, else the first declared, else
none; `ppvDerivVariable` mirrors that walk. Seeding by the parameter drew
a picture meaning a different number from the one `XEQ` returns
(AUDIT PP18-1).

**And "none declared" is not a refusal** — AUDIT PP18R2-1, the first fix's
own regression. `_differentiatorIteration`'s `fnFillStack` is
UNCONDITIONAL; only the `STO` into the named variable is guarded. A body
that takes its argument off the stack, the ordinary RPN function shape,
is differentiated correctly, so the picture invents a name exactly as
`SUM` does for its counter. Refusing it was a regression against PP17.

The integral is genuinely simpler, and the two must not be reasoned
about together: `DEI_xeq_user` writes into `regist`, and `_fnIntegrate`
sets `regist = labelOrVariable` — the integral's own parameter. **INTEG's
seeding by parameter name is exact; DERIV's cannot be.**

`PGMDRV` is a **separate latch** from `PGMINT`, because it is separate
upstream: *"a slot of its own so that taking a derivative does not
repoint what SOLVE, INT and PLOT will run next."* A derivative that read
`PGMINT`'s target would draw the wrong function with nothing on screen
saying so — V55 is that pin.

**Rulings.**

- **The `PGMINT` latch is NOT restored when a construct returns.**
  `currentSolverProgram` is a persistent global upstream, so a callee's
  relatch is exactly what a second integral would run (V17).
- **Only a latch set DURING the walk counts.** The runtime global's
  leftover value is not something a drawing may quietly assume (D6).
- **Local labels are rejected**, as `fnPgmInt` rejects them: a raw local
  number means nothing without a running program's context, which is the
  one thing a static walk does not have. `XEQ`'s acceptance of locals is
  an execution feature, not a resolution one.
- **A sum's counter name is invented** (first free of `n`, `m`, `k`, `j`)
  because RPN has none, and a body that recalls a real variable spelled
  the same way DECLINES rather than let the invented name shadow it
  (V6). An inner d-variable spelled like an outer one declines for the
  mirror reason (V23).
- **A unit step is omitted** from `SUM`/`PROD`: it is the evaluator's
  default and the renderer draws the `,Δstep` tail only when a fifth
  argument was parsed, so omitting it is both identical arithmetic and
  the cleaner picture (V4/V5).
- **`ENTER ×` transpiles to `x×x`, not `x²`.** The walker transpiles
  structure, never intent.
- **The solver session is cleared around the paint and restored after.**
  `ppqShowRender` frames its result from `currentSolverStatus`, and a
  stale integrate or derivative bit would wrap a program's drawing in an
  integral sign it never asked for (V19). VISUAL is raised in
  `CM_NORMAL` and inherits that mode's dismissal contract via
  `TI_SHOWNOTHING`, as §6 requires a new self-painted surface to state.
- **Nothing is painted on a decline**: the whole text is composed before
  a pixel is touched (V20).
- **The drawing goes in the Z/T rows, and the measurement decides that.**
  One stack line is 36 px; the transpiled forms measure (standard/tiny
  through `ppMeasure`): single integral 38/31, the double 58/51, the
  coupled triple 78/71. So ONE line holds only a single integral and only
  once shrunk — his own double-integral example does not fit it. The T
  and Z bands together are rows 20..91, **72 px**, which holds every
  chain in appnote 22. Only the stack refresh is suspended
  (`SCRUPD_MANUAL_STACK`), so the menu and status bar keep working and X
  keeps its value. V28 pins the six heights and the two inequalities the
  placement rests on, so a font or metric change that invalidates the
  choice fails loudly instead of silently overflowing into the Y line.
- **Taller than the pair falls through to the full-screen view** (147 px),
  and a formula the 2D grammar declines — plain arithmetic gains nothing
  from stacking — still shows in the window, linear and centred in it.
  Dropping to a full screen for `x·x-p·x-2` would be a worse answer than
  the stack rows already give.

**Named functions, and why the renderer grew an f(x) arm (widened
2026-08-28).** The first cut emitted only the four monadics with a 2D
spelling — `x²`, `√`, `1/x`, unary `−` — because `ppqPrimary` had no
function-application arm and `sin(x)` therefore could not be drawn. That
reasoning was right about `sin(x)` and wrong about everything around it:
the strict parser failed on the trailing `(`, so ONE unrecognised name
cost the WHOLE formula its 2D form — `sin(x)/2` lost its stacked
fraction, an integral over a sine lost its integral sign. The arm exists
for the context, not the function; a drawn `SIN(x)` is the same shape as
a linear one, which is why it deliberately does NOT set `fracSeen`.

The name test is shared, for the same reason the construct spelling is:
`ppEqFunctionItem` (solver/equation.c, beside the alias table) mirrors
`_parseWord`'s own resolution — alias table, then catalog and softmenu
names gated on `EIM_ENABLED` and a parameterless item — and the
renderer's arm, the walker's emitter and the evaluator all call it.

The walker emits a function only when BOTH hold: the item is in the
capture engine's `PPC_MO` monadic set (upstream has no usable arity
metadata — `EIM_DY` shares its bit with `RESULT_IN_X` and is vestigial),
and the item's own catalog spelling **round-trips** back through
`ppEqFunctionItem` to that same item. The round-trip is what removes the
need for a hand table to drift: a name the evaluator would not parse
back is never emitted, so this arm cannot produce text that draws and
will not compute. Admitted in practice: `LN`, `LOG`, `SIN`, `COS`,
`TAN`, `ARCSIN`, `ARCCOS`, `ARCTAN`. Correctly refused: `e^x`, `10ˣ`,
`LN(1+x)`, `>ABS<`, `|x|` — catalog spellings carrying glyphs or
punctuation the grammar has no room for. `x³` joins `x²` as a real
superscript rather than a name.

**Documented gap:** `ABS` resolves but its catalog spelling is `>ABS<`,
so it declines even though `abs` is in the alias table. Emitting an
ALIAS rather than the catalog name would admit it, at the cost of a
second table to keep honest; not done.

**Not in v1, deliberately.** `SOLVE`/`PGMSLV` chains — the equation
language has no root-of construct, and inventing a notation is a design
question, not an implementation one; they decline honestly. `CLX` (lift interplay under eRPN
unverified), `BINARY_REAL34` literals (they would need a plain-ASCII
conversion free of display glyphs), and dyadic functions — the emitter's
arity source is a monadic list, and a two-argument form would need both
an arity answer and a `f(a;b)` grammar arm.

**Budget (measured 2026-08-28).** Flash 1,146,432 -> 1,151,640
(**+5,208 B** for VISUAL entire; PP18's refactor gave back 136 B by
taking the text back end out of the device build, DERIV cost ~320, and
the audit-round fixes ~440);
device RAM **12,908/16,384, unchanged** — which is the
design claim as an executable fact, not an intention. No BSS: the
walker's whole state is one ~1.5 KiB stack frame
in `fnPrettyVisual` (a 512 B leaf-text pool + a 48-node
expression arena), plus ~50 B per recursion level, capped at depth 5.
Both are bump-allocated and dropped whole per walk.

**PP18 note:** the fragment pool, the 256 B compose buffer and the
construct-boundary rollback this paragraph used to describe are gone
with the text back end. A tree holds its body as a child, so there is
nothing to roll back and no scratch buffer whose reuse has to be timed.


## §7 Composition claims (BINDING for other packages)

Verified against the tree at branch point (undo-history/stage-u2 tip,
70f8b7db7):

| resource | claim | verified placement |
|---|---|---|
| item rows | **459 `PSHOW`, 460 `PPON`, 461 `PCLR`, 462 `PHIST`** | spare `itemToBeCoded` rows at items.c:2290-2293 — ~30 lines below undo-history's 427-429 hunk (ends :2260); items.h defines at :484-487, ~30 lines below its hunk (:446-454) |
| calcMode | **20 `CM_PRETTY_BROWSER`, WIRED since PP10** | PP4 shipped the history view as a manual-paint PAGER instead of a browser mode (see §6), avoiding ~20 keyboard.c sites in the one file where forth-core rewrites the determineItem chain undo-history already squeezed into. If a full browser lands later, its `#define` must NOT be adjacent to undo-history's `CM_HIST_BROWSER 19` insertion (after defines.h:1721) — anchor ≥4 context lines away |
| system flag | **50 `FLAG_PRETTYP` (0x8071)**, **51 `FLAG_PTLINE` (0x8072)** | superseded the v1 "none" ruling. The single `NUMBER_OF_SYSTEM_FLAGS` line cannot be edited by two packages independently, so BOTH packages carry the byte-identical `64+51` line and 3-way unifies them (identical-edit claim). Undo-history owns 49; 50 and 51 are ours. **AMENDED (audit r1, A8):** both SYSFL catalog rows (`PPRTY`, `PTLINE`) now live in THIS package's items.c at rows **218/219**, NOT in undo-history's. They were exiled there by the touching-line rule when they sat at 2300/2301 next to a sibling edit; at 218/219 they are inside our own existing 215-217 hunk and touch nothing of anyone else's. The move was forced: with the count here and the rows there, a SOLO pretty-print build declared 115 flags and supplied 112 rows, and the (un-overridden) flag browser indexed three entries past the end of `menu_SYSFL` into the alpha catalog. **KNOWN, NOT OURS TO FIX ALONE:** a single hardcoded count cannot be right for every package combination — upstream is balanced at 112/112, and every package that adds a flag over-declares in its own solo build. Ours is now exact (115/115) and combined over-SUPPLIES (116 ≥ 115, safe); undo-history's solo build over-declares, which is a property of the shared-count agreement and needs its owner. |
| item row | **984 `VISUAL`** (PP17) | a `CAT_FREE` "0984" row at items.c:2830, ~400 lines clear of every sibling hunk. Row 214 was rejected despite being contiguous with our own 215-219 block: it abuts forth-core's edited row 213, and the touching-line rule makes adjacency a conflict. **`PTP_LABEL`, not `PTP_DISABLED`** — copying a sibling PP row's status verbatim would have left the command un-programmable |
| menu id | **item 217 = `MNU_PP`** (CAT_MENU) | a `CAT_FREE` "0217" row adjacent to our own 215/216 claims, so the items.c hunk stays contiguous and nowhere near either sibling. forth-core's precedent: it turned free row 213 into `CAT_MENU` "FWRD". |
| test-list slot | **`pretty_visual_real` anchored before `graphs_cov`** (PP18) | NOT at EOF: forth-core appends `forth_interp` there, and both patches produced the same `@@ -507,3` hunk — a real conflict, caught by the combined gate. It must still come after `programs.txt`, because it clears program memory. The tail of a shared list is contended exactly like the tail of a table |
| softmenu slot | `menu_PP` slot 6 → `ITM_VISUAL` (PP17) | a softmenu's slots 6-11 ARE its f-shifted row, so VISUAL joins without displacing an announced key; the array pads to 12 with `ITM_NULL`, as every upstream menu is a multiple of six. `menu_PP` is our own array — no sibling can collide |
| softmenu slots | `menu_DISP` row 5 slot 3 → `-MNU_PP`; `menu_EQN` row 2 slot 2 → `ITM_EQSHW` | DISP and EQN are untouched by BOTH siblings (verified by diff), so neither edit can collide. The stack menu is deliberately AVOIDED: undo-history put `UHIST`/`REDO`/`HCLR` in its free slots and edited that exact line, leaving one slot and a guaranteed touching-line conflict. |
| softmenu table | new entry inserted immediately BEFORE the `/* 186 */` sentinel | the table's own instruction ("do not add menus here, add them at the end"). forth-core inserted mid-table at `/* 022 */`; anchoring at the tail keeps us ~160 lines clear of it. |
| resident pool | **zero** | all pretty-print state is BSS (~2.8 KiB end-state); the ~1.6 KiB pool slack remaining after undo-history's 4 KiB ring stays untouched |

Upstream files hooked, with verified adjacency to sibling packages' hunks:

| file | hook | adjacency notes |
|---|---|---|
| `c47.h` | `#include "prettyPrint.h"` | undo-history's include sits near :131; anchor ours on a different neighbor ≥4 lines away. forth-core does not patch c47.h. |
| `screen.c` | ONE hunk: the §6 inline arm at :3936 | forth-core's hunks (:3, :814-:934, :1159, :5662, :5927) and undo-history's are all far away. The include goes via c47.h precisely so we do NOT touch screen.c's include block (forth-core patches it at :3). |
| `items.c` | STAGE/DONE around dispatch (:412-414); rows 459-461; catalog stub after :1670 | >95 lines from undo-history's :292-315 hunk. The catalog stub anchors after `fnTripleFlipPolar` (:1670): BOTH siblings insert stubs at the tail of that list (undo-history after :1677, forth-core after :1685) and the first combined pass conflicted there — the anchor must precede :1675. |
| `bufferize.c` | `closeNim` head (:2341-2344) + `closeNim_exit` (:2688) | **forth-core has a hunk at :2691**, immediately after `closeNim` ends (:2690). Non-overlapping edits; contexts abut. Composes via normal patch offsetting — the combined gate pass is the proof, and a conflict there is loud, which is the intended failure mode. |
| `calcMode.c` | `calcModeNim` success-path latch | virgin file (no package patches it) |
| `config.c` | `prettyReset()` in `doFnReset` | anchor ≥4 lines from forth-core's hunks (:1541, :1964) and undo-history's (:1697) |
| `testSuite/testSuite.c` + `testSuiteList.txt` | ONE hunk (three `funcTestNoParam` rows after `fnGetNDEC` :707) + one list line after `matrix` | **No declaration hunk**: testSuite.c includes c47.h, which carries prettyPrint.h — the declaration region (:83-:94) is where both siblings insert and conflicts. Table rows anchor ≥2 lines above undo-history's (:712+) and forth-core's (:713+) row hunks. List line far from `nested_cov` (undo-history) and EOF (forth-core). |

No patches to `stack.c`, `defines.h`, `keyboard.c`, `softmenus.c`,
`statusBar.c` until PP4 (the browser stage).

## §8 Budgets

- **RAM:** all BSS, no resident pool. End-state ≈ 2.8 KiB (renderer pools
  ~1.5 KiB, capture ~0.65 KiB, history ring ~0.7 KiB). Measure the BSS delta
  at every stage gate; contingency is shrinking `ppPool`/`ppText`.
  **PP1 measured (2026-08-26): 1,609 B device-relevant** — prettyLayout
  1,408 B BSS (pool 768 + text 512 + metrics/counters) + prettyValue 200 B
  BSS + 1 B data (the toggle); prettyTest's 4,304 B is PC_BUILD-only and
  never reaches the device. Zero resident-pool use confirmed.
- **Flash:** increases are fine when justified (project rule); record the
  measured `make dmcp5r47 CUSTOM_PKG=… CUSTOM_PKG_RECONFIGURE=1` delta in
  each stage commit. **PP1 measured: +1,920 B** (R47_flash.bin 1,120,752 →
  1,122,672, baseline forth-core+undo-history at 70f8b7db7).
- **Per-frame cost:** measure+paint is O(glyphs) integer work, no FP, no
  allocation; runs only when the toggle is on and the type is supported.

## §9 Staging

| stage | content | ships when |
|---|---|---|
| **PP1** | package skeleton + gate; engine core (`RUN`/`HBOX`/`FRAC`, measure/paint, ladder, baseline float); fraction parser; the screen.c arm + c47.h include; `PPON` toggle item | stacked fractions render inline; pins green solo+combined |
| **PP2** | `PP_RAD`/`PP_SUP`; IRFRAC + exponent + complex parsers; `PSHOW` manual-paint surface | radicals/exponents/complex pretty; PSHOW works |
| **PP3** | capture engine complete (no UI): hooks, classifier, segmentation, ring | all capture pins green through real key paths |
| **PP4** | `prettyExpr.h` contract; tree→2D infix with precedence parens + `PP_PAREN` synthesis; current-formula line; history browser (calcMode 20, the keyboard.c stage) | formula view usable end to end |
| **PP5** | EQN strip 2D: strict display-string grammar, '/' terms stack (standard/tiny, 17 px in the 23 px strip row), √ gets vinculums, parens unwrap under both; one hunk at solver/equation.c's paint site, no-cursor path only | SHIPPED 2026-08-26 (authorized by the proceed-with-all-stages instruction) |

Branch per stage (`pretty-print/stage-pp1`, …), single clean commit series,
gate green per stage. PP1 branches from the undo-history/stage-u2 tip because
`main` does not yet contain `packages/undo-history` and the combined gate
needs all three packages present; pretty-print adds only new paths plus small
hunks, so a later rebase onto main-after-merge is mechanical.
