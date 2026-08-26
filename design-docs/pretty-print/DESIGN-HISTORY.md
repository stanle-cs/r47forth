# Pretty-print package — design history

Non-normative amendment trail. DESIGN.md is authoritative.

## 2026-08-26 — PP2 (radicals, exponents, IRFRAC, complex, PSHOW)

- **Radical sign strategy narrowed for PP2:** the raised-glyph approach
  only (sign painted so its ink top meets the vinculum; sign font = the
  node's font), with radicands taller than `radInk + 3` declining. The
  synthesized-DDA sign for tall radicands is deferred to PP4, where
  expression trees can put fractions under radicals.
- **IRFRAC parser scoped to the common template**
  `[sign] [multiple: n×|n|supN] name [/den]` with name ∈ {√d, √π, π, e, φ}.
  The exotic checkForAndChange outputs — mixed-number constant forms
  (`e+…`), the paren-power family `(π²)`, `(e⁻¹)` — decline to upstream's
  linear rendering by the fallback rule. Bare names with no structural
  win (lone `π`) also decline: no visual change, so no reason to own the
  paint.
- **The real path gained a third alternative:** IRFRAC's pure-fraction
  output (constant = 1) uses the same sup-num/`/`/sub-den alphabet as the
  FRACT builder, so `ppParseRealAny` = exponent → irfrac-template →
  fraction. A value like 0.75 with IRFRAC on but FRACT off now stacks.
- **Angular-tagged reals decline** (`getRegisterAngularMode != amNone`):
  the upstream angle path draws its mode suffix glyph in a separate pass
  the package arm would skip.
- **Exponent textbook form:** `mantissa·₁₀ⁿ` becomes base run
  `mantissa·10` (plain-size 10, original product glyph kept verbatim)
  with the exponent as a true raised run — PP_SUP with supDrop 10/6/4 px.
- **P4's probe lesson:** the √ glyph's own diagonal legitimately crosses
  the "gap row" in the sign's columns — gap/ink pixel pins must probe the
  radicand's columns only.

## 2026-08-26 — PP1 initial design and first findings

- Package created per the approved plan: natural display of calculations,
  undo-history-shaped package, five-stage roadmap (PP1 fractions inline →
  PP2 radicals/exponents/PSHOW → PP3 capture engine → PP4 formula view →
  PP5 EQN 2D). Segmentation ruled: liveness + new-root supersession; ENTER
  never terminates (DESIGN.md §4).
- **Slot claims verified against the live tree, two adjustments over the
  planning draft:** (1) the c47.h include anchors after the
  `#endif //TESTSUITE_BUILD` line, not the ui/ include list — undo-history's
  insertion after `ui/tone.h` sits inside the 3-line context window of any
  ui/-list anchor; (2) the package include enters via c47.h only, because
  forth-core patches screen.c's include block at line 3 and a second
  insertion there cannot compose. screen.c carries exactly one hunk (the
  render arm).
- **Non-X register lines have a 31-row descent budget, not 35.** Adjacent
  clear bands overlap: line N+1's `clearRegisterLine` starts at its own
  baseY−4 = line N's baseY+32, erasing anything painted there after line N
  rendered (refreshScreen order T,Z,Y,X). The planning draft assumed each
  line's own clear extent was usable. Consequence: an improper fraction
  (descent 5) floats its baseline up 1 px on Y/Z/T; the X line (band to
  baseY+38) does not float.
- **Paint-order finding (pin P1, red on first run): glyph-box pre-clears
  eat rules painted first.** The fraction bar was painted before the digit
  runs; `showGlyphCode`'s per-glyph box pre-clear (padding rows included)
  wiped it under both digits, leaving exactly the 2+2 overhang pixels lit —
  the pixel pin's "bar full-width at exact rows" assert caught it precisely
  as designed. Ruled into DESIGN.md §1 as the binding paint-order rule;
  PP2's vinculum inherits it.
- Measured constant worth remembering: `stringWidth(…, false, true)` drops
  the first glyph's leading empty columns, so a numericFont `"1"` run
  measures 14 px, not the 16 px advance (M2 pins 26 for the mixed-number
  HBOX, not 28).
- **First combined pass conflicted twice — both anchors moved, and a
  sharper rule emerged.** (1) items.c catalog stub: all three packages
  append stubs to the same list; undo-history inserts after :1677,
  forth-core after :1685 — an anchor at the list TAIL is contended by
  construction, so pretty-print's stub sits mid-list after :1670.
  (2) testSuite.c driver declarations: the declaration block (:83-:94) is
  contended the same way; eliminated entirely — testSuite.c includes c47.h,
  which already carries prettyPrint.h, so only the table-row hunk remains
  (anchored after `fnGetNDEC` :707, above both siblings' row hunks).
  The refined rule for future hooks: "≥4 lines from a sibling hunk" must be
  checked against EVERY package's hunks in that file, and natural append
  points (list tails, section ends) are exactly where everyone lands —
  prefer mid-list anchors next to entries no package will move.
