# Pretty-print package: design

This document is authoritative for `packages/pretty-print/`. Amendments and
rejected shapes go to DESIGN-HISTORY.md. The test contract lives in
TESTING.md.

Pretty print means **natural display of calculations** (ruled 2026-08-26).
That covers textbook-style rendering of values (stacked fractions with a
real bar, √ with a vinculum, raised exponents), 2D display of EQN formulas,
and the user's chained RPN operations reconstructed as infix formulas
(`2+3 = 5`). Since the PP19 split (2026-08-31, boundary amended same day)
this package holds **everything that draws native functions**: the layout
engine, the value converters, the inline lines, PSHOW, the 2D equation
surfaces (strip, EQSHW), and the VISUAL program walker. The
calculation-capture engine, the formula history with its views, and the
package-invented equation
language (the SUM/PROD/DERIV/INTEG evaluator) live in
`packages/pretty-print-extra`
(design-docs/pretty-print-extra/DESIGN.md). That package requires this
one. This one builds and works alone.

It is an external package with the same structure and discipline as
`packages/undo-history/`: flat working area mirroring upstream paths,
generated `patches/`+`files/`, gate at
`./packages/pretty-print/build-test.sh` (solo + trio + full passes).

Upstream baseline facts the whole design leans on: value rendering is
strictly one-baseline glyph strings (`display.c` builders → `showString`).
Fractions render diagonally (sup-digits `/` sub-digits,
`fractionToDisplayString`, display.c:1684). `STD_SQUARE_ROOT` has no
overbar. There is no 2D layout anywhere except the matrix editor. So
everything 2D in this package is new code that paints through the existing
pipeline (`showString` at explicit (x,y)) plus `lcd_fill_rect` bars.

## §1 The layout engine (`prettyLayout.c`)

A minimal box model. Six node kinds:

| kind | children | meaning |
|---|---|---|
| `PP_RUN` | none | glyph string, one baseline, drawn by `showString` |
| `PP_HBOX` | n | children side by side on a shared baseline |
| `PP_FRAC` | num, den | stacked fraction, painted bar |
| `PP_RAD` | radicand | radical sign + painted vinculum |
| `PP_SUP` | base, exponent | raised exponent (real superscript placement) |
| `PP_PAREN` | inner | parens. Degenerate to glyphs, synthesized when tall (PP4) |

Node = 16 bytes, index-linked (`firstChild`/`nextSibling`). Nodes live in
fixed BSS pools: `ppNode_t ppPool[48]` (768 B), `char ppText[512]` (run
text), `char ppLeafScratch[200]` (upstream-builder output). Max nesting
depth 6, enforced at build time (bounds measure/paint recursion). There is
no heap use and no resident-pool use, ever.

Each `FRAC`/`RAD`/`SUP` node carries a context font id. A const flash
table `ppMetrics[3]` (numeric/standard/tiny) holds: box ascent (36px font:
28, 22px: 16, tiny: 8), the math axis, bar overhang, gaps, vinculum
thickness, superscript drop. The math axis is calibrated to each font's
minus-sign ink, so a fraction bar sits exactly where a minus sign does
(numeric: rows B−11..B−10 with bar thickness 2, standard: B−6 with
thickness 1, tiny: B−4).

Two passes. **Measure** (post-order) computes width/ascent/descent per node,
with run ink extents taken from real glyph metrics (`findGlyph`,
`rowsAboveGlyph`/`rowsGlyph`). **Paint** draws runs via
`ppShowRun()` (see the clearing-extent rule below) and
bars/vinculums via `lcd_fill_rect(..., LCD_EMPTY_VALUE)`, the same call
shape as `drawSinglePixelFullWidthLine` (screen.c:1554). The radical glyph
is painted raised so its top row meets the vinculum. Radicands taller than
the glyph get a synthesized sign (integer DDA over `setBlackPixel`).

**Paint-order rule (BINDING, found by pin P1 on 2026-08-26): every painted
rule (fraction bar, vinculum) goes AFTER the glyph runs it neighbors.**
A glyph painted with a font-box pre-clear erases padding rows
(`rowsBelowGlyph`/`rowsAboveGlyph`) that reach past its ink into the bar
band, even when the ink honors the gap. A bar painted first is wiped
under every digit column. Only its overhang pixels remain. Measure-pass
gaps are ink-relative and correct. Only the paint order compensates.

**Scope of that rule since R3-13 (audit R5-1).** The justification above
was written when every run went through `showString`. The rule now applies
to exactly one run: the radical sign glyph, the sole remaining
`showString` call. The reason: `ppShowRun` gives all other runs
`noPreClear` and bounds their clear to the measured box. For a fraction
the clears provably cannot reach the bar (the numerator's stops
`fracGap+1` rows above it, the denominator's starts `fracGap+2` below).
The ordering there is retained for uniformity, not because it is
load-bearing. Keep the rule. Do not re-derive its reason from the
fraction case: that case no longer demonstrates it.

**Clearing-extent rule (BINDING, audit R3-13): a node clears exactly the box
it measured, and never more.** Glyph runs go through `ppShowRun()`. It
clears the measured ink box with `ppFillVal(…, LCD_SET_VALUE)` and then
paints via `showGlyphCode` with `noPreClear` TRUE. Calling `showString`
directly from paint is a defect: its per-glyph font-box clear is larger than
anything measure reasoned about, so nodes measure-placed to sit clear of each
other still erase each other. The case that found it: a fraction denominator's
baseline rises as its ink shortens, so its font box crosses the bar into
the numerator. An '8' numerator kept 52 lit rows over an '8' denominator,
38 over an 'x', 20 over a '.'. Pin P12 holds the three equal. The rule
also keeps a run packed against a band edge from clearing frame rows
outside that band. The one deliberate exception is the radical sign glyph:
it is alone in its columns. The paint-order rule above still governs its
vinculum.

**Font ladder per surface**, rebuild-per-rung until the layout fits:
- `PP_SURF_INLINE` (register line, 36 px band): numeric ctx / standard
  children → standard/standard → standard/tiny → fail.
- `PP_SURF_FULL` (PSHOW, 147-row band): numeric/numeric → numeric/standard →
  standard/standard → standard/tiny → fail.
- `PP_SURF_BAND` (browser row): standard/standard → standard/tiny → fail.

The top-level renderer right-aligns and lets the baseline float inside the
band (clamped by ascent/descent). Verified pixel budgets: every register
line has 32 rows above the numeric baseline and 7 below (X line: 10). A
proper fraction at the first inline rung measures ascent 25 / descent 5
and fits with no float. √2 measures ascent 29 and fits. A two-level
`(a+b)/c` in standard/standard is 23/12 and fits with a ~5 px float. Full
screen takes three nesting levels in numericFont.

**Fallback rule (BINDING).** Every pretty path is a `bool_t` try-function.
Any failure (unsupported type, unexpected glyph, pool/text/depth overflow,
no rung fits) paints nothing and returns false. Upstream's own arm then
renders unchanged. The renderer never reads `tmpString` and never holds
a pointer into it. One write exists, as scratch: `ppfFormatStaged`'s
`dtShortInteger` arm gives `tmpString` to `shortIntegerToDisplayString`,
because that builder needs an `ERROR_MESSAGE_LENGTH`-byte buffer
(PP18RR7-5). On every surface that reaches the formatter, upstream holds
no live `tmpString` data across the call: the try-function declines first
(the `temporaryInformation` / `lastErrorCode` / `calcMode` gates in
`prettyTryRegisterLine`). On fallback the upstream path is untouched: each
upstream arm writes `tmpString` before it reads it. Overflow is never an
error screen. It is a legitimate "too complex to pretty-print".

## §2 Value converters (`prettyValue.c`)

**Builder-first invariant (BINDING).** The converter always calls the
upstream `…ToDisplayString` builder first, into `ppLeafScratch`, with the
same arguments the upstream display arm uses. It then parses the builder's
output into a layout tree. This preserves `displayValueX` and every
formatting decision (digits, separators, PROPFR/DENFIX, shrink-to-fit)
byte-for-byte. So the pretty form can never disagree with what upstream
shows. Parse failure → fallback rule.

- Fractions (PP1): parse `fractionToDisplayString` output. Closed alphabet:
  optional `< = >` prefix, sign, plain-digit integer part, `STD_SUP_0..9`
  numerator (0xa160+d), `/`, `STD_SUB_0..9` denominator (0xa080+d), separator
  glyphs. Tree: `HBOX[prefix?, int?, thin-space?, FRAC(num, den)]`.
- Exponent reals (PP2): split `real34ToDisplayString` output at the
  `PRODUCT_SIGN STD_SUB_10` marker (emitted by `exponentToDisplayString`,
  display.c:127). The sup-digit tail becomes a `PP_SUP` exponent run. No
  marker → plain real → false (upstream renders).
- IRFRAC symbolic forms (PP2): parse the `checkForAndChange` output
  alphabet into `PP_RAD`/`PP_FRAC`/`PP_SUP`. The alphabet is √, π, e, φ,
  sup/sub digits, `/` and parens (table at display.c:516-532).
- Complex (PP2): split at the imaginary-unit boundary. Both parts recurse
  through the parsers above. Polar stays linear.
- Long integers, strings, matrices, short integers, dates, times, configs:
  never pretty (immediate false).

## §3 The package boundary and the extension points (PP19)

The split rule, amended on the owner's correction the same day it first
shipped: this package draws. It draws register values AND the mathematics
of native functions (operators, named functions, Σ/∏/∫, equations,
programs). The extra package remembers. It captures live calculations
into formula trees, keeps the history ring, and owns the views of that
history (pager, browser, T line) plus the whole menu story. The extra
package never draws a pixel itself: it decodes capture trees into
layout nodes through this package's §1 and prettyInfix.c builders.

What the solo build contains, stated because it is a real install: the
inline register lines, PSHOW, EQSHW and the 2D EQN strip, VISUAL,
PPON, PTLIN and both system flags. The invented equation language is
the extra package's, so a solo build cannot save construct equations
(the base grammar rejects them). There is no softmenu: `MNU_PP` and
every menu placement belong to the extra package, so solo users reach
the commands through the catalog. The SYSFL catalog rows for the two
flags are this package's (items.c rows 985/986). `PTLIN` toggles its
flag in the solo build, and nothing reads it (the T-line renderer is
the extra package's).

**Extension points (BINDING for the pair).** This package never
references an extra symbol. Where a core code path must reach extra
behavior, the core exposes a function pointer in prettyPrint.h and the
extra package fills it in at its lazy init (`ppcInit`):

- `ppTlineExtension`: `prettyTryRegisterLine` calls it for
  `REGISTER_T` before the value rendering. The extra package draws the
  live formula there (`ppfTlineTry`, gated on `FLAG_PTLINE`). The
  registration timing is sound because no formula can exist before the
  first capture hook runs, and every capture hook runs `ppcInit` first.
- `ppResetExtension`: `prettyReset()` calls it before it restores the
  flag defaults. A NULL pointer means the capture engine never ran, so
  there is nothing to re-arm.

A NULL pointer is skipped, and the core behaves as if the extra
package were absent. The measured sim BSS delta for the two pointers
is zero: they sit in existing alignment padding (r47, PP19 gate).

**prettyInfix.c** holds the shared 2D infix builders
(`ppfBuildOp1`/`ppfBuildOp2`, the bracket rules, `ppfPowBase`,
`ppfTextIsAtom`, the display-time name decodes). Every producer of a
2D form calls through it (the equation renderer and the walker here,
the capture viewer in the extra package), so precedence is decided in
one place.

### Flag scope (RULED at PP15, both commands core-owned since the PP19 amendment)

`FLAG_PRETTYP` governs only what the calculator draws on its own
initiative: the inline register lines. The explicit view commands
`PSHOW` and `EQSHW` ignore it: a request to see something must show it.
`FLAG_PTLINE` is a second, independent opt-in for the T-line live
formula, default OFF.

Init and factory-reset are SEPARATE (PP15, after a latent PP11 bug).
The extra package's `ppcInit()` prepares its own data, and its lazy
first-use path calls it. `prettyReset()` here restores both flag
defaults AND re-arms the extra package through `ppResetExtension`.
Only `doFnReset` is permitted to call it. A lazy path that restores
defaults overwrites the user's saved preferences. Persisting them was
meant to prevent exactly that.


## §4 Surfaces

The formula-history views (pager, browser, T line) are the extra
package's (design-docs/pretty-print-extra/DESIGN.md §4). This
package's surfaces:

- **Inline register lines** (PP1): one hook arm in `_refreshRegisterLine`,
  inserted immediately before the `/*Main type dtReal34 FLAG_FRACT*/` arm
  (screen.c:3936): `else if(prettyTryRegisterLine(regist, baseY)) { }`. All
  gates live package-side: toggle on, `CM_NORMAL`, `TI_NO_INFO`, no error, no
  view-register prefix in play, `!checkHP` (HP layout doubles glyph rows
  inside `showGlyphCode`, so all metrics become invalid, and HP users get
  upstream rendering), supported type. Output is clipped to the line's own
  cleared band (`baseY−4 .. baseY+35`, X line `+38`). No fit → false.
- **`PSHOW` full screen** (PP2): a manual-paint item (row 459) using the
  `fnPixel` protocol verbatim (`screenUpdatingMode |= SCRUPD_MANUAL_STACK |
  SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS; screenHoldsDrawnPixels =
  true;`, screen.c:6552-6560): pixels survive refresh, and the next keypress
  releases them. Zero keyboard.c churn, zero screen.c hunks. Status-bar rows
  0..15 stay untouched, so the clock keeps ticking. Builder failure falls
  back to `fnC47Show(NOPARAM)`: the user always gets a SHOW.

  BINDING (2026-08-27, owner-reported): a surface that paints its own
  screen also declares it one, by setting `temporaryInformation =
  TI_SHOWNOTHING` beside `screenHoldsDrawnPixels = true`. Holding pixels
  is not what upstream dismisses on. Its EXIT arm tests
  `temporaryInformation != TI_NO_INFO || showScreenDismissed`
  (keyboard.c), and `showScreenDismissed` latches from SHOWMODE, itself a
  temporaryInformation test. A surface that omits it draws correctly and
  then cannot be closed with EXIT. The convention is upstream's own
  matrix SHOW: it paints its screen and then sets TI_SHOWNOTHING
  ("then tell the system it is in show nothing mode", display.c).

  Scope, stated because the obvious generalisation is wrong: this binds
  the surfaces upstream dismisses. That is every self-painted screen
  raised in CM_NORMAL, today PSHOW and EQSHW. The PHIST pager also
  holds pixels but is raised INSIDE `CM_PRETTY_BROWSER`, where the
  package's own containment routes every key and `prettyBrowserLeave`
  does the dismissing. Upstream's EXIT arm never sees it. It deliberately
  does NOT declare itself. A declaration breaks its paging: the pager
  reads `screenHoldsDrawnPixels` to tell a repeat press from a fresh
  one, and a SHOWMODE screen has that cleared for it on the next press.
  So a self-painted surface follows the dismissal contract of the mode
  it is raised in, and a new one must say which that is.

### Solver-surface frames (PP13)

EQSHW frames the equation with the interactive solver's own numbers:
integrate mode shows a PP_BIGOP ∫. Its under/over limits are the REAL
`RESERVED_VARIABLE_LLIM/ULIM` values, and its body appends ` d<var>`
(the variable name decoded live via `ppfVariableName`). Without
`SOLVER_STATUS_INTERACTIVE` (or non-real limit registers) it falls back
to PP7's bare stroke ∫. The derivative modes prefix `d/d<var>` (first)
or `d²/d<var>²` (second) with the equation in tall parens. Solve
framing (`f(x)=0`) is deliberately absent:
`SOLVER_STATUS_EQUATION_SOLVER` is the zero value of the mode field, so
with that framing a stale INTERACTIVE bit frames a plain view with an
`= 0` the user never asked for. Un-determinable state stays unframed.

### The equation-language constructs draw here, and evaluate in extra

`SUM(...)`, `PROD(...)`, `DERIV(...)` and `INTEG(...)` are the
package-invented equation language, and the language belongs to
pretty-print-extra (ruled 2026-08-31). Its parser interception and its
evaluation machinery are that package's solver/equation.c hunks, and
its design is design-docs/pretty-print-extra/DESIGN.md §5. The RENDER
arm for the constructs stays here, inside `ppqParse`: it draws the
language's picture. Both sides call the one spelling test
(`ppEqConstructIs`, prettyPrint.h), so a spelling one side accepts and
the other declines cannot exist in the pair. In a solo build the arm
is unreachable. Without the evaluator, construct text is a hard parse
error in the base grammar, so no construct equation can be saved and
no picture without a computation can appear.

### VISUAL: a program as its mathematics (PP17)

`VISUAL 'DBLINT'` (item 984, `TM_LBLONLY`) draws a stored RPN program as
the mathematics it computes, without running it, into the Z/T window.
So the answer the program just left in X stays visible underneath.
Jaymos requested it on the forum against appnote 22's `func.txt`. That
file's chains are the design's reference input, and his placement
request ("draw the integrals in the Z/T window") is part of the
specification: the point is to see the formula and its result together.
Captured 2026-08-28: `XEQ 'DBLINT'` gives 1.333333..., and `VISUAL
'DBLINT'` then draws the nested integral above it.

**Shape: a third front-end, not a third renderer.** The package already
had two producers of one node tree: the capture engine (live dispatches)
and the equation parser (EQN text). `prettyVisual.c` adds a static walker
over stored program steps.

PP17 shipped it as a transpiler to equation-language TEXT that
`ppqShowRender` then re-parsed. PP18 removed that round trip, for two
reasons. It settled precedence twice: the walker inserted brackets into
a string, and the parser read them back out to rediscover the same
structure. All along, `ppfBuildOp1`/`ppfBuildOp2` had been doing exactly
that job for the capture engine. And it made the text grammar a
dependency of drawing. That is the opposite of what an upstream reader
asked for.

The walker now builds a small expression tree and lays it out through the
shared builders: `ppfBuildOp1`/`ppfBuildOp2` for operators,
`ppqBuildBigop` for constructs, `ppfWrapIf` for scoping. **Nothing in the
walker decides where a bracket goes.** The drawing came out
byte-identical to PP17's. That is how the change was verified.

Three places where the node form is not merely equivalent to the text
form. A fraction bar SCOPES, so `a/(b+c)` draws with no parentheses at all
(V49). A stacked power DOES need its base bracketed, because `ppfBuildOp`
deliberately has no POW level: a `PP_SUP` normally scopes itself, and a
new level changes the contract underneath the capture engine (V51). That
rule now lives in `ppfPowBase`. Every producer of a `PP_SUP` calls it,
and the walker's older local copy is redundant. And a leaf whose TEXT is
not a visual atom (a signed numeral, a value in scientific form, a tagged
angle, a complex) reports `PPF_PREC_ADD` rather than `ATOM`, so the
ordinary bracket rules scope it. Both leaf builders ask the same
`ppfTextIsAtom`: when only the walker had that rule, the two surfaces
drew the same numeral differently (PP18RR6-2, r6). **A based
integer IS a visual atom (ruled, PP18RR8-6, option A): the base subscript
is part of the numeral's spelling, like its digit-group spaces, so
`10₁₆ · 2` draws bare.** The predicate's accepted two-byte window is the
digit-group spaces plus the fifteen base subscripts.

**Why a symbolic stack seeded with the variable NAME is faithful.**
Upstream feeds a body program through two channels. The integrator writes
each node into the named d-variable AND fills every stack level with it
(`integrate.c`, `DEI_xeq_user` + `fnFillStack`). A programmed sum
delivers its counter through the filled stack ALONE (`sumprod.c`, no
named variable). An equation body, by contrast, reads its variable by
NAME (`solver/equation.c`, the XEQ-mode RCL arm). Seeding a body frame
with the variable name on all eight levels reproduces both channels at
once. That is why the transpiled text computes what the RPN computed.

**RETIRED at PP18: the emitted alphabet.** While the walker drew by
emitting text, that text had to satisfy the renderer AND the evaluator,
two different parsers. Multiplication had to be `STD_CROSS`
(`\x80\xd7`, since `'*'` is accepted by NEITHER). Constructs were
all-upper. Numerals were digits and `.` and a leading `-` only. A wrong
byte dropped the whole formula to a linear line with no error. That made
it a BINDING rule, with MUT-106 as its guard.

It no longer binds the drawing path, because there is no longer any text
in it: the walker builds nodes, and a node cannot be mis-spelled. The
rule still governs the test back end (one pin feeds its output to
`fnEqCalc`) and the equation parser itself. The record stays because the
reasoning is what justifies not going back.

**BINDING: fail closed.** An opcode the dispatch table does not name
declines. There is no inferred "harmless item" rule, because the item
table carries no stack-effect metadata to infer from. `TICKS` is the
counterexample that breaks one: it looks inert and pushes a value no
drawing can predict. The skip list is explicit (`LBL`, `MVAR`, `REM`,
`PAUSE`, `SNAP`).

**The opaque-taint rule.** A value the text cannot spell (a string
literal, an exponent numeral) becomes an OPAQUE placeholder, not a
decline. Movers (`ENTER`, `x<>y`, the drops, `FILL`, `STO`) carry it
freely. Embedders (any operator, a construct's limits or body, the final
result) decline on it. This lets appnote 22's own idioms pass through
untouched: `'INT(INT) = 4/3' STO A DROPX` for a plot title, and
`1e-8 STO 'ACC' DROP` to set integrator accuracy. It also guarantees
that neither can reach the printed mathematics.

**Decline catalog** (D-numbers reach the user through
`moreInfoOnError`): D1 unsupported opcode · D2 indirect parameter · D3
local label · D4 unresolved label · D5 recall of a name a `STO` changed ·
D6 integral with no `PGMINT` latched during the walk · D7 register read
(the language reads names only) · D8 depth · D9 step budget · D10 stack
underflow · D11 opaque reaching the mathematics · D12 variable collision ·
D13 malformed program memory · D14 unreadable numeral · D15 fragment cap ·
D16 pool exhausted · D17 nothing to show · D18 name the grammar cannot
spell.

**Derivatives (PP18, corrected twice: read the whole paragraph).**
`PGMDRV` latches the program. `f'`/`f"` pop the point. **The variable is
NOT the `f'` parameter.** `calcDeriv` asks `deriv_pgm_variable(label)`.
That function walks the BODY program's own leading `MVAR` declarations
and returns the one matching the parameter, else the first declared,
else none. `ppvDerivVariable` mirrors that walk. Seeding by the
parameter drew a picture meaning a different number from the one `XEQ`
returns (AUDIT PP18-1).

**And "none declared" is not a refusal** (AUDIT PP18R2-1, the first
fix's own regression). `_differentiatorIteration`'s `fnFillStack` is
UNCONDITIONAL. Only the `STO` into the named variable is guarded. A body
that takes its argument off the stack, the ordinary RPN function shape,
is differentiated correctly, so the picture invents a name exactly as
`SUM` does for its counter. Refusing it was a regression against PP17.

The integral is genuinely simpler, and the two must not be reasoned
about together: `DEI_xeq_user` writes into `regist`, and `_fnIntegrate`
sets `regist = labelOrVariable`, the integral's own parameter. **INTEG's
seeding by parameter name is exact. DERIV's cannot be.**

`PGMDRV` is a separate latch from `PGMINT`, because it is separate
upstream: *"a slot of its own so that taking a derivative does not
repoint what SOLVE, INT and PLOT will run next."* A derivative that
reads `PGMINT`'s target draws the wrong function, with nothing on screen
saying so. V55 is that pin.

**Rulings.**

- **The `PGMINT` latch is NOT restored when a construct returns.**
  `currentSolverProgram` is a persistent global upstream, so a callee's
  relatch is exactly what a second integral then runs (V17).
- **Only a latch set DURING the walk counts.** A drawing must not
  quietly assume the runtime global's leftover value (D6).
- **Local labels are rejected**, as `fnPgmInt` rejects them: a raw local
  number means nothing without a running program's context, the one
  thing a static walk does not have. `XEQ`'s acceptance of locals is
  an execution feature, not a resolution one.
- **A sum's counter name is invented** (first free of `n`, `m`, `k`, `j`)
  because RPN has none. A body that recalls a real variable spelled
  the same way DECLINES, so the invented name cannot shadow it
  (V6). An inner d-variable spelled like an outer one declines for the
  mirror reason (V23).
- **A unit step is omitted** from `SUM`/`PROD`: it is the evaluator's
  default, and the renderer draws the `,Δstep` tail only when a fifth
  argument was parsed. So the omission is both identical arithmetic and
  the cleaner picture (V4/V5).
- **`ENTER ×` transpiles to `x×x`, not `x²`.** The walker transpiles
  structure, never intent.
- **The solver session is cleared around the paint and restored after.**
  `ppqShowRender` frames its result from `currentSolverStatus`, and a
  stale integrate or derivative bit wraps a program's drawing in an
  integral sign it never asked for (V19). VISUAL is raised in
  `CM_NORMAL` and inherits that mode's dismissal contract via
  `TI_SHOWNOTHING`, as §4 requires a new self-painted surface to state.
- **Nothing is painted on a decline.** The whole text is composed before
  a pixel is touched (V20).
- **The drawing goes in the Z/T rows, and the measurement decides that.**
  One stack line is 36 px. The transpiled forms measure (standard/tiny
  through `ppMeasure`): single integral 38/31, the double 58/51, the
  coupled triple 78/71. So ONE line holds only a single integral, and
  only once shrunk. His own double-integral example does not fit it. The
  T and Z bands together are rows 20..91, 72 px. That holds every
  chain in appnote 22. Only the stack refresh is suspended
  (`SCRUPD_MANUAL_STACK`), so the menu and status bar keep working and X
  keeps its value. V28 pins the six heights and the two inequalities the
  placement rests on. So a font or metric change that invalidates the
  choice fails loudly instead of silently overflowing into the Y line.
- **Taller than the pair falls through to the full-screen view** (147 px).
  A formula the 2D grammar declines (plain arithmetic gains nothing
  from stacking) still shows in the window, linear and centred in it.
  Dropping to a full screen for `x·x-p·x-2` is a worse answer than
  the stack rows already give.

**Named functions, and why the renderer grew an f(x) arm (widened
2026-08-28).** The first cut emitted only the four monadics with a 2D
spelling (`x²`, `√`, `1/x`, unary `−`): `ppqPrimary` had no
function-application arm, so the renderer had no way to draw `sin(x)`.
That reasoning was right about `sin(x)` and wrong about everything
around it. The strict parser failed on the trailing `(`, so ONE
unrecognised name cost the WHOLE formula its 2D form: `sin(x)/2` lost
its stacked fraction, and an integral over a sine lost its integral
sign. The arm exists for the context, not the function. A drawn
`SIN(x)` is the same shape as a linear one. That is why it deliberately
does NOT set `fracSeen`.

The name test is shared, for the same reason the construct spelling is.
`ppEqFunctionItem` (solver/equation.c, beside the alias table) mirrors
`_parseWord`'s own resolution: alias table, then catalog and softmenu
names gated on `EIM_ENABLED` and a parameterless item. The renderer's
arm, the walker's emitter and the evaluator all call it.

The walker emits a function only when BOTH hold. The item is in the
capture engine's `PPC_MO` monadic set (upstream has no usable arity
metadata: `EIM_DY` shares its bit with `RESULT_IN_X` and is vestigial).
And the item's own catalog spelling round-trips back through
`ppEqFunctionItem` to that same item. The round-trip removes the
need for a hand table that can drift. A name the evaluator does not
parse back is never emitted, so this arm cannot produce text that draws
but does not compute. Admitted in practice: `LN`, `LOG`, `SIN`, `COS`,
`TAN`, `ARCSIN`, `ARCCOS`, `ARCTAN`. Correctly refused: `e^x`, `10ˣ`,
`LN(1+x)`, `>ABS<`, `|x|` (catalog spellings that carry glyphs or
punctuation the grammar has no room for). `x³` joins `x²` as a real
superscript rather than a name.

**Documented gap:** `ABS` resolves but its catalog spelling is `>ABS<`,
so it declines even though `abs` is in the alias table. An emitter that
uses the ALIAS instead of the catalog name admits it, at the cost of a
second table to keep honest. Not done.

**Not in v1, deliberately.** `SOLVE`/`PGMSLV` chains: the equation
language has no root-of construct, and inventing a notation is a design
question, not an implementation one. They decline honestly. `CLX` (lift
behavior under eRPN unverified), `BINARY_REAL34` literals (they need a
plain-ASCII conversion free of display glyphs), and dyadic functions
(the emitter's arity source is a monadic list, and a two-argument form
needs both an arity answer and a `f(a;b)` grammar arm).

**Budget (measured 2026-08-28).** Flash 1,146,432 -> 1,151,640: +5,208 B
for VISUAL entire. Inside that, PP18's refactor gave back 136 B by
taking the text back end out of the device build, DERIV cost ~320, and
the audit-round fixes ~440. Device RAM 12,908/16,384, unchanged. That is
the design claim as an executable fact, not an intention. No BSS: the
walker's whole state is one ~1.5 KiB stack frame
in `fnPrettyVisual` (a 512 B leaf-text pool + a 48-node
expression arena), plus ~50 B per recursion level, capped at depth 5.
Both are bump-allocated and dropped whole per walk.

**PP18 note:** the fragment pool, the 256 B compose buffer and the
construct-boundary rollback this paragraph used to describe are gone
with the text back end. A tree holds its body as a child, so there is
nothing to roll back and no scratch buffer whose reuse has to be timed.


## §5 Composition claims (BINDING for other packages)

First verified against the tree at branch point (undo-history/stage-u2
tip, 70f8b7db7). PP19 re-verified the split placements. This table
and the extra package's (design-docs/pretty-print-extra/DESIGN.md §5)
together are the claims registry for the pair.

| resource | claim | verified placement |
|---|---|---|
| item rows | **459 `PSHOW`, 460 `PPON`, 461 `EQSHW`, 462 `PTLIN`** | the spare `itemToBeCoded` run at items.c:2290-2293, ~30 lines below undo-history's 427-429 hunk (ends :2260). items.h defines at :484-487. EQSHW and PTLIN moved here from 216/215 at PP19: their old rows sit inside the extra package's 215-217 hunk, and a sibling row that touches another package's row is a 3-way conflict (the touching-line rule). |
| item rows | **984 `VISUAL`, 985 `PPRTY` + 986 `PTLINE` (CAT_SYFL)** | the `CAT_FREE` 984-987 run at items.c:2826+, ~400 lines clear of every sibling hunk. The SYSFL rows moved here from 218/219 at PP19 (same touching-line reasoning). The generated SYSFL catalog carries the same row count from either home, and `prettySysflRows()` bounds every walk. Row 987 stays free: the pair's growth room. `VISUAL` keeps `PTP_LABEL` (a `PTP_DISABLED` copy makes the command un-programmable). |
| system flag | **50 `FLAG_PRETTYP` (0x8071)**, **51 `FLAG_PTLINE` (0x8072)** | superseded the v1 "none" ruling. The single `NUMBER_OF_SYSTEM_FLAGS` line cannot be edited by two packages independently, so undo-history and THIS package carry the byte-identical `64+51` line and 3-way unifies them (identical-edit claim). Undo-history owns 49. 50 and 51 are ours. The extra package carries NO defines.h flag edit. The comment on the count line is part of the byte-identical claim: an edit in one package alone breaks the trio configure (re-learned at PP19, loudly). |
| SYSFL bound | `browsers/flagBrowser.c` override | ONE hunk: the SYSFL walk bounds by the catalog's OWN row count (`softmenu[].numItems` for `-MNU_SYSFL`, through `prettySysflRows()`), correct in every package combination (audit r8, PP18RR8-2). Virgin file for every sibling. **KNOWN, NOT OURS TO FIX ALONE (PP18RR9-3):** a solo undo-history build still reads two entries past its own shorter array (see `SIBLING_REPORT_undo-history_menu_SYSFL.md`). |
| test-list slots | **`pretty_print` after `matrix`. `pretty_visual_real` before `graphs_cov`** | two hunks in one patch. `pretty_visual_real` CLEARS PROGRAM MEMORY, so it must come after every case that expects preloaded programs, and NOT at EOF. The tail is forth-core's claim, and two packages appending there is a real conflict (caught at PP18). The extra package's one list line sits after `program_flow_cov`, ~11 lines above our tail hunk. |
| resident pool | **zero** | all state is BSS (§6). The ~1.6 KiB pool slack remaining after undo-history's 4 KiB ring stays untouched |

Upstream files hooked, with verified adjacency to sibling packages' hunks:

| file | hook | adjacency notes |
|---|---|---|
| `c47.h` | `#include "prettyPrint.h"` after the TESTSUITE_BUILD block (:134) | undo-history's include sits after :130 (`ui/tone.h`). The extra package's sits after :124 (`statusBar.h`). Three includes, three anchors. forth-core does not patch c47.h. |
| `screen.c` | FOUR hunks: the glyph pre-clear x-guard (:1238), the doubled-write column clamps (:1275), the §4 inline arm (:3936), and the menu/status repaint guard on the total early return (:5864) | forth-core's nearest hunks clear ours by 61 lines (its :5930 against our :5864) and 73 (its :1162 against our :1238). The `case CM_PRETTY_BROWSER` hunk is the extra package's. The include goes via c47.h precisely so we do NOT touch screen.c's include block (forth-core patches it at :6). The repaint guard serves every manual-paint surface: PSHOW, EQSHW and VISUAL here, the pager there. |
| `items.c` | rows 459-462 and 984-986, plus five catalog stubs after `fnTripleFlipPolar` (:1669) | >95 lines from undo-history's :292-315 hunk. BOTH siblings insert stubs at the tail of that list (undo-history after :1677, forth-core after :1685), and the first combined pass conflicted there. The anchor must precede :1675. The extra package's two stubs anchor after `fn42Prompt` (:1660), nine lines above ours. |
| `defines.h` | the two flag defines (:1016) + the count line (:1021) | undo-history edits the SAME count line byte-identically (see the flags claim). `CM_PRETTY_BROWSER` is the extra package's. |
| `solver/equation.c` | TWO hunks: `ppEqFunctionItem` after fnEqCalc (:213) and the EQN strip paint hook (:684) | virgin file for both siblings. The extra package patches the SAME file for its equation language (its hunks at :736, :1370 and the file tail), ~50 lines from ours at the closest point. The pair gate proves the five hunks compose. |
| `config.c` | `prettyReset()` in `doFnReset` | anchor ≥4 lines from forth-core's hunks (:1541, :1964) and undo-history's (:1697). The extra package needs NO config.c hunk: its reset rides `ppResetExtension`. |
| `testSuite/testSuite.c` + `testSuiteList.txt` | ONE hunk (seven `funcTestNoParam` rows after `fnGetNDEC` :704) + the two list lines above | **No declaration hunk**: testSuite.c includes c47.h, and c47.h carries prettyPrint.h. Both siblings insert in the declaration region (:83-:94), and insertions there conflict. Table rows anchor ≥2 lines above undo-history's (:712+) and forth-core's (:713+) row hunks. The extra package's two rows anchor after `fnSetC47` (:692), twelve lines above ours. |

No patches to `stack.c`, `keyboard.c`, `softmenus.c`, `statusBar.c`,
`bufferize.c` or `calcMode.c`. The browser, the capture hooks and the
menus live in the extra package's patch stack.

## §6 Budgets

- **RAM:** all BSS, no resident pool. Engine + converters + toggles ≈
  1.6 KiB device-relevant: prettyLayout 1,408 B BSS (pool 768 + text
  512 + metrics/counters) + prettyValue ~200 B BSS + 1 B data (PP1
  measured 2026-08-26). VISUAL has NO BSS: its whole state is one
  ~1.5 KiB stack frame in `fnPrettyVisual` plus ~50 B per recursion
  level, capped at depth 5 (measured 2026-08-28). The two extension
  pointers cost no measured BSS (they sit in alignment padding).
  prettyTest's BSS is PC_BUILD-only and never reaches the device.
  Zero resident-pool use.
- **Flash:** increases are fine when justified (project rule). Record
  the measured `make dmcp5r47` delta for the PAIR in each stage commit
  (the split moved code between packages, so the pair's combined
  number is the honest one). PP1 measured: +1,920 B. The PP19
  split's pair delta against the pre-split trio is in the stage
  commit.
- **Per-frame cost:** measure+paint is O(glyphs) integer work, no FP,
  no allocation. It runs only when the toggle is on and the type is
  supported, or on the explicit view commands.

## §7 Staging

| stage | content | ships when |
|---|---|---|
| **PP1** | package skeleton + gate. Engine core (`RUN`/`HBOX`/`FRAC`, measure/paint, ladder, baseline float). Fraction parser. The screen.c arm + c47.h include. `PPON` toggle item | stacked fractions render inline. Pins green solo+combined |
| **PP2** | `PP_RAD`/`PP_SUP`. IRFRAC + exponent + complex parsers. `PSHOW` manual-paint surface | radicals/exponents/complex pretty. PSHOW works |
| **PP3** | capture engine complete (no UI): hooks, classifier, segmentation, ring | all capture pins green through real key paths |
| **PP4** | `prettyExpr.h` contract. Tree→2D infix with precedence parens + `PP_PAREN` synthesis. Current-formula line. History browser (calcMode 20, the keyboard.c stage) | formula view usable end to end |
| **PP5** | EQN strip 2D: strict display-string grammar, '/' terms stack (standard/tiny, 17 px in the 23 px strip row), √ gets vinculums, parens unwrap under both. One hunk at solver/equation.c's paint site, no-cursor path only | SHIPPED 2026-08-26 (authorized by the proceed-with-all-stages instruction) |

| **PP19** | the split, boundary amended same day: everything that draws stays here (engine, values, EQN surfaces, constructs, VISUAL). Capture and the history views move to `packages/pretty-print-extra` with their hooks. Extension points added | all five gate passes green. Combined behavior byte-identical except four renumbered item ids |

Branch per stage (`pretty-print/stage-pp1`, …), single clean commit series,
gate green per stage. PP1 branches from the undo-history/stage-u2 tip because
`main` does not yet contain `packages/undo-history` and the combined gate
needs all three packages present. pretty-print adds only new paths plus small
hunks, so a later rebase onto main-after-merge is mechanical.
