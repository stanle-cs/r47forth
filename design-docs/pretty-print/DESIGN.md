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
`showString(text, font, x, baselineY − boxAscent, vmNormal, false, true)` and
bars/vinculums via `lcd_fill_rect(..., LCD_EMPTY_VALUE)` — the same call
shape as `drawSinglePixelFullWidthLine` (screen.c:1554). The radical glyph is
painted raised so its top row meets the vinculum; radicands taller than the
glyph get a synthesized sign (integer DDA over `setBlackPixel`).

**Paint-order rule (BINDING, found by pin P1 on 2026-08-26): every painted
rule (fraction bar, vinculum) goes AFTER the glyph runs it neighbours.**
`showGlyphCode` pre-clears each glyph's full box, and a glyph box's padding
rows (`rowsBelowGlyph`/`rowsAboveGlyph`) reach past its ink into the bar
band even when the ink honours the gap — a bar painted first is wiped under
every digit column, leaving only its overhang pixels. Measure-pass gaps are
ink-relative and correct; only the paint order compensates.

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

## §7 Composition claims (BINDING for other packages)

Verified against the tree at branch point (undo-history/stage-u2 tip,
70f8b7db7):

| resource | claim | verified placement |
|---|---|---|
| item rows | **459 `PSHOW`, 460 `PPON`, 461 `PCLR`, 462 `PHIST`** | spare `itemToBeCoded` rows at items.c:2290-2293 — ~30 lines below undo-history's 427-429 hunk (ends :2260); items.h defines at :484-487, ~30 lines below its hunk (:446-454) |
| calcMode | **20 reserved** (not wired) | PP4 shipped the history view as a manual-paint PAGER instead of a browser mode (see §6), avoiding ~20 keyboard.c sites in the one file where forth-core rewrites the determineItem chain undo-history already squeezed into. If a full browser lands later, its `#define` must NOT be adjacent to undo-history's `CM_HIST_BROWSER 19` insertion (after defines.h:1721) — anchor ≥4 context lines away |
| system flag | **none in v1** | undo-history's patch edits the single `NUMBER_OF_SYSTEM_FLAGS` line; two packages editing the same line cannot compose. Toggle = package BSS bool + `PPON` item. Flag persistence is deferred to an explicitly coordinated change. |
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
