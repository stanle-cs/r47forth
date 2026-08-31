# Pretty-print-extra package — design

This document is authoritative for `packages/pretty-print-extra/`.
Amendments and rejected shapes go to DESIGN-HISTORY.md here (pre-split
history is in design-docs/pretty-print/DESIGN-HISTORY.md). The test
contract lives in TESTING.md.

This package is the remembering half of the PP19 split (2026-08-31,
boundary amended same day): the core package draws, this one captures
what the user computed and shows where results came from. It holds the
calculation-capture engine, the history ring, the views of that
history — the PHIST pager, the calcMode-20 browser, the T-line live
formula — the package-invented equation language (§5), and the whole
menu story for the pair. Everything that
draws native functions (the layout engine, the infix builders, the EQN
surfaces, VISUAL) is the `packages/pretty-print` core package, and its
DESIGN.md governs those parts.

**This package REQUIRES packages/pretty-print. There is no solo
build.** The viewer here decodes capture trees into layout nodes
through the core's §1 API and prettyInfix.c builders, and the core
paints them. The two extension points the core exposes
(`ppTlineExtension`, `ppResetExtension`, prettyPrint.h) are filled in
at this package's lazy init — core DESIGN.md §3 states the contract,
including why the registration timing is sound. The gate is
`./packages/pretty-print-extra/build-test.sh` (pair + full passes).

**Softkey containment (BINDING, audit R5-3) lives in this package's
keyboard.c hunks.** The rule, verbatim from the pre-split design:

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

## §1 The capture engine (`prettyCapture.c`)

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

## §2 Segmentation — where a formula ends (RULED 2026-08-26)

**The rule: liveness + new-root supersession.** A formula (an op-rooted tree)
is emitted to history when either
1. **displacement** — its root leaves the shadow stack unconsumed:
   overwritten in X, dropped, wiped (CLX/CLSTK/invalidation), swapped out to
   a register, or pushed off the stack top by lifts; or
2. **supersession** — a new operator node is created whose operands do *not*
   include the current root. The old formula emits immediately (its
   `= result` read from the register that still holds it — the §1 invariant
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

## §3 History ring and display handoff

Finished formulas are serialized **eagerly as postfix token streams by pure
byte copies** — literal (as-typed text), value (raw payload), register,
constant, op tokens, plus a result snapshot. No display formatter runs at
capture time; formatting stays lazy (§1), which keeps both the letter and the
spirit of the no-strings-at-capture rule while letting the arena free the
tree on emission. Ring: `ppHist[640]` + 12 offsets, oldest-first eviction
(undo-history's eviction shape). Oversized entries (> half the ring) are
dropped, not stored.

Renderer-facing API (all lazy): `ppCurrentFormulaRoot()` (+ read-only arena
access), `ppHistoryCount()` / `ppHistoryEntry(idx, &len, &seq)`,
`ppHistoryClear()`. The renderer owns the item-id → infix-form table
(precedent: print.c:21-38) and precedence-driven parenthesization.

## §4 The history views

Every surface here follows the fallback rule and the manual-paint
conventions of the core package (core DESIGN.md §1 and §4):

### Formula view (PP4, RULED 2026-08-26)

A PAGER first, and since PP10 also the full browser on calcMode 20.
  `PHIST` (row 216, moved from 462 at PP19) renders the current formula plus the finished-formula
  history as 2D infix — division stacked, powers raised, roots under
  vinculums, precedence parens (glyph or synthesized-tall) — four rows per
  page on the same manual-paint protocol as PSHOW. Repeated `PHIST`
  presses page forward; any other key releases the screen. `PCLR` (row
  215, moved from 461 at PP19) clears the ring. Value leaves and results format at display time
  via TEMP_REGISTER_1 staging (undo-history's lazy-preview recipe),
  never at capture. PP4 shipped the pager with zero keyboard.c and
  defines.h churn, because keyboard.c is the project's riskiest
  composition surface. PP10 then wired the full CM-mode browser on
  calcMode 20 (selection, per-row scrolling, `.d` pan, ENTER recall);
  its keyboard.c hunks are in §5. The pager stays as the non-browser
  fallback surface.

### The T-line live formula

`FLAG_PTLINE` (default OFF; the flag and the `PTLIN` command are the
core package's) puts the open formula on the T register line. The
renderer is `ppfTlineTry` (prettyFormula.c), registered into the
core's `ppTlineExtension` slot: standard/standard rung first, then the
whole tree re-fonted tiny, the same ladder shape the pager uses. No
formula or no fit returns false and T shows its value.

## §5 The equation language (PP14)

The package-invented constructs — `SUM`, `PROD`, `DERIV`, `INTEG` with
the `;` separator — are this package's: the parseEquation interception
and the slice-evaluation machinery are its solver/equation.c hunks.
The RENDER arm stays in the core package's `ppqParse` (it draws the
language's picture, and drawing is the core's), gated by the one
shared spelling test `ppEqConstructIs` (prettyPrint.h) so the two
sides cannot disagree. The ruled design, verbatim from the pre-split
document:


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

## §6 Composition claims (BINDING for other packages)

This table and the core package's (core DESIGN.md §5) together are the
claims registry for the pair. Verified at PP19 against the four-package
tree (forth-core, undo-history, pretty-print, pretty-print-extra).

| resource | claim | verified placement |
|---|---|---|
| item rows | **215 `PCLR`, 216 `PHIST`, 217 `MNU_PP` (CAT_MENU)** | one contiguous hunk in the 213-219 free run, with one untouched row (214) between it and forth-core's edited row 213 (the gate-proven shape the pre-split 215-219 hunk used since PP17). `PCLR`/`PHIST` RENUMBERED at PP19 (from 461/462): their old rows touch the core's 459-462 claim, and a sibling row that touches another package's row is a 3-way conflict (the touching-line rule). |
| calcMode | **20 `CM_PRETTY_BROWSER`** | define in THIS package's defines.h hunk (:1716 region), 5 lines from undo-history's `CM_HIST_BROWSER 19` insertion (after :1721) — the ≥4-line rule holds. The `case` in screen.c's calcMode switch sits ONE untouched line from undo-history's case: contended by construction, both packages add a case to the same switch, and the full gate is the proof of composition (unchanged shape since PP10). |
| system flags | **none declared** | both flags, both toggle commands and the `NUMBER_OF_SYSTEM_FLAGS` count are the core's (identical-edit claim with undo-history). This package only reads `FLAG_PTLINE`. |
| softmenu | `menu_PP` (all 7 items), table entry before the `/* 186 */` sentinel, `menu_DISP` row 5 slot 3 → `-MNU_PP`, `menu_EQN` row 2 slot 2 → `ITM_EQSHW`, the PPON/PTLIN checkbox arm in `showSoftmenuCurrentPart` | the menu names five core items (`ITM_PSHOW`, `ITM_PPON`, `ITM_EQSHW`, `ITM_PTLIN`, `ITM_VISUAL`) — legal, because this package never builds without the core's items.h hunk. DISP and EQN are untouched by forth-core and undo-history (verified by diff). The stack menu stays deliberately avoided. |
| test-list slot | **`pretty_extra` after `program_flow_cov`** | one line, ~11 lines above the core package's `pretty_visual_real` hunk, ~225 below its `pretty_print` line, and clear of undo-history's (`nested_cov`) and forth-core's (EOF) claims. |
| resident pool | **zero** | capture arena, slots, ring and pager state are BSS (§7); the pool slack after undo-history's ring stays untouched |

Upstream files hooked, with verified adjacency to sibling packages' hunks:

| file | hook | adjacency notes |
|---|---|---|
| `c47.h` | `#include "prettyExtra.h"` after `statusBar.h` (:124) | six lines above undo-history's include (after :130), ten above the core's (after :134). Three includes, three anchors. |
| `items.c` | STAGE/DONE around dispatch (:412-414); two catalog stubs after `fn42Prompt` (:1660); rows 215-217 | the dispatch hunk is >95 lines from undo-history's :292-315 hunk. The stub anchor sits nine lines above the core's five (after `fnTripleFlipPolar` :1669), and clear of undo-history's (:1677) and forth-core's (:1685) tail inserts. |
| `keyboard.c` | the three browser-containment range clauses (`calcMode < 19`, byte-identical with undo-history's — identical-edit claim), the `CM_PRETTY_BROWSER` determineItem arm, the processKeyAction guard, the six key-handler cases, and the two `ppcShadowInvalidate` calls at the direct `fnRecall` sites | moved here whole at PP19, byte-identical to the pre-split hunks (gate-proven against forth-core's determineItem rewrite and undo-history's cases). |
| `bufferize.c` | `closeNim` head (:2341-2344) + `closeNim_exit` (:2688) | **forth-core has a hunk at :2691**, immediately after `closeNim` ends (:2690). Non-overlapping edits; contexts abut. Composes via normal patch offsetting — the full gate pass is the proof, and a conflict there is loud, which is the intended failure mode. |
| `calcMode.c` | `calcModeNim` success-path latch | virgin file (no other package patches it) |
| `screen.c` | ONE hunk: `case CM_PRETTY_BROWSER` in the calcMode dispatch (:6163) | see the calcMode claim. The core keeps the other four screen.c hunks, including the repaint guard the pager relies on. |
| `defines.h` | ONE line: `CM_PRETTY_BROWSER 20` | see the calcMode claim. No flag edits here. |
| `solver/equation.c` | THREE hunks: the `ppEqBigopIntercept` forward declaration (:736), the parseEquation interception (:1370), and the construct machinery block at the file tail | the core package patches the SAME file (its `ppEqFunctionItem` at :213 and paint hook at :684) — ~50 lines from ours at the closest point, and the pair gate proves the five hunks compose. |
| `testSuite/testSuite.c` + `testSuiteList.txt` | ONE hunk (three `funcTestNoParam` rows after `fnSetC47` :692) + one list line after `program_flow_cov` | twelve lines above the core's rows (after `fnGetNDEC` :704), ≥2 above undo-history's (:712+) and forth-core's (:713+). No declaration hunk: c47.h carries prettyExtra.h. |

No config.c hunk (reset rides the core's `ppResetExtension`) and no
flag or count edits. `stack.c` and `statusBar.c` stay untouched.

## §7 Budgets

- **RAM:** all BSS, no resident pool: capture ~0.65 KiB + history ring
  ~0.7 KiB + pager/browser state — the remembering half of the pair's
  pre-split ≈ 2.8 KiB. Measure the BSS delta at every stage gate.
- **Flash:** increases are fine when justified (project rule); record
  the measured `make dmcp5r47` delta for the PAIR in each stage commit
  — this package does not build alone, so the pair's combined number is
  the honest one.
- **Per-frame cost:** capture hooks are O(1) pointer work per dispatch.
  Rendering runs only on PHIST, in the browser, and on the opt-in
  T line.
