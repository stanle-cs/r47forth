# Pretty-print package — design history

Non-normative amendment trail. DESIGN.md is authoritative.

## 2026-08-26 — PP12 (captured Σ/Π/∫ big-operator nodes)

- **The capture hooks were not nesting-safe — latent since PP3, load-bearing
  from PP12 on.** `prettyNoteFunction` cleared `ppcStage.valid` BEFORE its
  scope check, and a BIGOP dispatch runs its label program's every step
  through `runFunction` (executeOneStep → runFunction, under
  FLAG_SOLVING/PGM_RUNNING): the first inner step destroyed the outer
  stage and the BIGOP DONE never applied. The same clobber was reachable
  pre-PP12 by any function whose body nests `reallyRunFunction` in scope
  (`.d` executing →REAL is upstream's own example). Fix: scope-check
  first — `valid` is only ever true strictly inside a dispatch, so a
  top-level STAGE out of scope has nothing to clear.
- **The screenshot caught a display lie the sig pins could not.** The first
  ∫ trace called `fnIntegrateYX(label)` — which is the interactive SETUP
  form: it harvests X,Y into ULIM/LLIM, drops them, retargets the solver
  and integrates NOTHING. The capture minted a BIGOP whose "result" was
  whatever X held (0), and the composed sheet read `∫₀¹P(x)dx = 0.` —
  truthful-looking, wrong. Ruling: capture only the dispatch that actually
  integrates (named-variable param over a preselected label program, the
  `covIntegratePgm` currency); every other param form invalidates (B6b
  pins it). The variable id rides in the payload so the body shows the
  real d-variable (`P(X)dX`).
- **After a BIGOP dispatch, only X can be vouched for.** The label program
  ran with the machine's full keyboard between STAGE and DONE — every
  register but the result is somebody else's writing. Slot 0 = the BIGOP,
  all other slots and slot L go UNKNOWN (lazy VAL re-materialization).
  The first draft slid-and-top-dupped the surviving trees; that claimed
  register values a user program can falsify. Reverted before it ever ran.
- **The ∫/Σ dispatches leak solver state into later suite files.** The
  full gate failed 4 upstream tests in deriv_cov: `fnPgmInt` (inside the
  ∫ path) clears `SOLVER_STATUS_USES_FORMULA` and retargets
  `currentSolverProgram` at label P, and `covDerivEq` — unlike
  `covSolveRoot` — does not reset the status it inherits, so it
  differentiated my x² program instead of its X³ formula (the observed
  2x/constant-2 values matched exactly). The driver now saves and
  restores `currentSolverStatus/Program/Variable` + `currentMvarLabel` —
  the aimBuffer restore rule, extended to solver globals.
- **MUT-41 stayed green on its first run** — the B8 probe rect reached
  column 35, and the body run (`P(n)`, starting at colW+3 = 29) shares
  the probe rows, so body ink masked the deleted strokes. Probe tightened
  to the operator column (x..x+26). Same lesson as FV7/FV9: pin the
  pixels only the mutated code can light.
- The sum step travels as real34 (`fnToReal`'s own currency, longint
  converted at STAGE via `convertLongIntegerRegisterToReal34`), so a
  non-unit step renders with the real marker: `n=1,Δ2.` — accepted as
  truthful rather than re-formatted.
- Fixture notes: program installed through a copy-adapted
  `covWriteAndLoadPgm` (label `P`, `x²`, the upstream pgmT shape); the
  history-order in `ppcHistoryEntry` is newest-first; live key replays
  repaint the register lines, so the capture sheet builds all entries
  first and paints after (the first composed shot lost its under-limits
  to exactly that).
- No new upstream hunks: PP12 is entirely package-internal (capture,
  layout, formula, tests). §7 claims unchanged.

## 2026-08-26 — post-PP11 capture polish

- The PP6-PP11 showcase captures found one cosmetic defect: the ⁿ√
  index kerned into the RAISED-GLYPH sign's own hook. Fixed: an indexed
  root always synthesizes its stroke sign (measure and paint recompute
  the same test). Also a capture-driver lesson worth keeping: CHS during
  NIM is a buffer EDIT, not a dispatch — a fixture that dispatched it
  left the NIM open and the following digits appended, producing
  ³√(-527) on screen. The engine rendered that truthfully, which is the
  invariant doing its job; the fixture now closes NIM through the real
  keypress pattern.

## 2026-08-26 — PP11 (the toggle becomes a persisted system flag)

- FLAG_PRETTYP (0x8071, bit 50) replaces the package bool: PPON now
  persists across power cycles with the ordinary flag machinery. The
  count-line conflict that made a flag impossible in v1 is resolved by
  the identical-edit claim: BOTH packages' defines.h carry
  NUMBER_OF_SYSTEM_FLAGS 64+50 verbatim, and the 3-way merge unifies.
- **Adjacency lesson sharpened: diff3 conflicts on TOUCHING-line edits,
  not just same-line ones.** My row-2300 items.c edit sat one line below
  undo-history's row-2299 edit and conflicted (my hunk's context carries
  upstream's 2299, their patch had changed it); the identical-edit trick
  cannot save a hunk whose CONTEXT mismatches. Resolution follows the
  case-20 pattern: the SYSFL row 2300 ("PPRTY", literal 0x8071) lives in
  UNDO-HISTORY's patch alongside its own row-2299 edit; solo
  pretty-print keeps the fully-working flag but shows no catalog row for
  it — a documented solo-config gap, not a behavior change.
- Ordering trap paid for: the config.c prettyReset hook originally sat
  BEFORE doFnReset's `systemFlags0 = 0` wipe — the default-ON would have
  been erased moments after being set. The hook moved below
  `Sett(_Reset)`; FV13 pins reset-restores-default-ON.
- DESIGN.md §7's "no system flag in v1" entry is superseded by this
  stage; the claims registry rows there stay authoritative.

## 2026-08-26 — PP10 (the browser lands on calcMode 20)

- **The composition wall was condition LINES, not insertions.**
  Undo-history's browser edits ~7 shared condition lines (fn-key/TAM
  guards, shift inclusion, keyActionProcessed, underline, keyboardTweak)
  — a third browser editing the same lines is a guaranteed 3-way
  conflict. Resolution: ONE undo-history amendment (Stan pre-authorized
  the mechanism class) converts those lines to the range form
  `calcMode 19..23 = package browsers` and adds literal `case 20:` to
  its case-list insertions — solo-safe, and browsers 21-23 now compose
  for free. Recorded in undo-history DESIGN.md §6.
- Pretty-print's own keyboard surface is insertions only: the
  head-of-chain resolution branch (both packages independently prepend
  `if(mode){...} else` blocks at different lines — the 3-way merge
  CHAINS them, which is the elegant part), and one case block per
  fnKey handler, each anchored after a complete case body well away
  from undo-history's insertion points (fallthrough groups must never
  be split — a braced case inside one captures the group).
- PHIST now opens the browser; UP/DOWN select (clamped), .d pans a
  too-wide selected row (wraps), ENTER recalls the selected entry's
  TKRES into X — saveForUndo first, lift, then `ppcShadowInvalidate()`:
  the recall bypasses item dispatch, so the shadow wipes to UNKNOWN
  rather than pretending it followed. The live (now) row recalls
  nothing — its value IS X. The manual-paint pager body remains as the
  non-browser fallback surface.
- Keyboard-case reachability is proven structurally (the handlers are
  3-line breaks); the handlers themselves are pinned via direct calls
  (FV12). A B9-style real-keypress harness remains open work.
- The head-of-chain branch was ABANDONED after conflicting at
  undo-history's exact insertion point (and the earlier anchor would
  have left a bare `else` binding into a PC_BUILD preprocessor block):
  instead forth-core's REWRITTEN key-resolution list line gained
  `|| (calcMode >= 20 && calcMode <= 23)` — package browsers without
  their own resolution branch resolve plainly through the standard
  branch. Solo-pretty-print (no forth-core) resolves nav keys through
  the final else (primaryAim == primary for them); only .d-pan may
  differ solo — documented quirk, direct-call pins unaffected.
- **FOUND, PRE-EXISTING, NOT OURS: the forth-core + undo-history
  HEADLESS BATTERY segfaults** right after fold test [9] (R8-P1) — with
  or without the amendment, with or without pretty-print. No gate ever
  ran the battery under a combined shadow (forth's battery gate is
  solo; combined passes run only the meson testSuite). Reproducer:
  configure CUSTOM_PKG=packages/forth-core,packages/undo-history with
  -DFORTH_DEBUG_SELFTEST, run `c47 --headless`. Filed here for the
  forth-core/undo-history owners; the forth+pretty-print battery is
  GREEN at this tip.

## 2026-08-26 — PP9 (RCL/STO capture classes)

- Register operations now chain instead of invalidating: RCL pushes a
  NAMED leaf for numbered registers (R05 stays R05 — a name is truthful
  even if the register is later overwritten), deep-copies the shadow
  tree for stack-register recalls (copy-then-lift, matching upstream's
  read-before-shift), and degrades lettered/named registers to value
  leaves (their display names are not item ids). RCL±×÷ builds a dyadic
  node with the PLAIN operator item so the renderer needs no new cases.
  x<>reg emits the departing tree (its value still sits in X at STAGE)
  and leaves a value leaf for the register's old content — naming the
  register would lie, since it now holds the swapped-in value. DROPY
  mirrors upstream's shift-with-top-dup from slot 1.
- T18 documents a subtle correct behavior: using a stack-recalled COPY
  of the open formula supersedes (emits) the original still sitting
  higher up — the copy continues visually, the original is finished.
- STO and STO±×÷ stay stack-silent (PPC_STO_NOP), as before.

## 2026-08-26 — PP8 (T-line live formula, opt-in)

- PTLIN (row 215) shows the OPEN formula on the T register line while
  chaining — DEFAULT OFF, exactly as Stan specified when selecting the
  gap: T's value is hidden while the toggle is on, so the user opts in.
  Implementation is a T-only branch at the top of the inline surface's
  ladder: standard rung, then the whole-tree tiny re-font, and any
  no-formula/no-fit case falls through to the ordinary value rendering.
- FV11 pins all three properties: OFF-by-default (band identity between
  the fresh state and forced-off), the formula appearing when toggled,
  and the X line staying a value under the toggle. The first FV11
  landed with a stubbed default-off comparison — caught in review
  before commit; the capture helper grew a band parameter to close it.

## 2026-08-26 — PP7 (EQN full view; the Σ finding)

- **The equation language has NO Σ/∏/∫ constructs** — its function
  vocabulary is trig/hyperbolic aliases plus the glyph→item map. A Σ
  template would be dead code with zero possible inputs (capture
  excludes solver items by standing ruling; EQN cannot express a sum),
  so Σ was SKIPPED, narrowing the purchased scope honestly. The one
  legitimate big-operator surface: the interactive integrate solver,
  where the stored equation IS the integrand — EQSHW frames it with a
  stroke-drawn big ∫ (PP_INT) when
  `(currentSolverStatus & SOLVER_STATUS_EQUATION_MODE) == INTEGRATE`.
- EQSHW (row 216 — the 214-216 spare run; forth-core edits row 213's
  line, different lines merge cleanly): full-screen equation view on the
  manual-paint protocol. `ppqParse` gained font parameters; the full
  view parses standard/standard, so the nested fractions the 23 px strip
  declines render at full size; unparseable equations still show their
  linear line (the always-show-something fallback, pinned by EQ9).
- Ellipsis caveat stands: the view renders showEquation's display
  string, which upstream truncates at screen width — very long equations
  fall back to the (truncated) linear line rather than mis-typesetting.

## 2026-08-26 — PP6 (typographic trio; gap-closure wave, Stan's selection)

- PP_RAD grew a synthesized stroke sign (Bresenham over setBlackPixel,
  2 px in numeric contexts) for radicands taller than the font glyph —
  √(fraction) now renders instead of declining — and an optional second
  child: the ⁿ√ index tucked above-left with ~half overlapping the sign.
  XTHROOT and CUBEROOT render as indexed radicals (index = X operand for
  ˣ√y, verified against fnXthRoot's register use).
- New node kinds: PP_SUB (log_b subscripts; LOGXY verified as log base X
  of Y from WP34S_Logxy's argument order — also had to JOIN the capture
  dyadic table, which is how FV9 found it missing) and PP_BARS (|x| for
  ABS/MAGNITUDE, two 2 px strokes one row proud of the child).
- Two pins were born decorative and sharpened same-day: FV7's sign probe
  included the vinculum's first column (any-ink satisfied without
  strokes), and FV9's root-descent assert was satisfied by "log"'s own
  descender — the pin now reads the script node's relBase directly. The
  mutation sweep exists precisely to catch pins like these.

## 2026-08-26 — post-PP5 stress test (continued fractions)

Stan asked for a complex-expression trial. `1/(2+3/(4+5/6))` keyed through
the real paths (6 ENTER 5 x<>y ÷ 4 x<>y + 3 x<>y ÷ 2 x<>y + 1 x<>y ÷):

- The capture engine held ONE open formula through all eleven operations
  (history stayed empty — every op consumed the previous root), and the
  layout engine rendered the full three-level nest with correctly nested
  bars. Depth headroom: a run at nesting level 6 hits PP_MAX_DEPTH — one
  more fraction level than this test would decline.
- **Found: the pager's fixed 36 px rows silently dropped tall formulas**
  (the rung ladder only re-fonted leaf runs, and even all-tiny the nest
  measures 37 px). Fixed red-first (FV6): `ppSetFontDeep` on the tiny
  rung AND the pager rewritten to variable-height packing — rows pack
  until the band fills, pages fall out of the packing walk.
- **Found: rows packed flush against the frame lines wipe them** — glyph
  boxes overhang their ink by the font padding and the pre-clear erases
  the frame (FV5 went red the moment packing started at the band edge).
  The packing band is inset 4 px from both frames.
- Σ+ after a chain: the shadow invalidated truthfully and emitted both
  finished formulas first — integration/summation internals stay outside
  capture scope by the standing ruling (solver/integrator storms), and
  the unknown-item default handles the items themselves.
- EQN strip: `(A+B)/C+1/D` renders as two stacked fractions in the 23 px
  row; nested `1/(2+3/4)` declines to the linear line as designed.

## 2026-08-26 — PP5 (EQN strip 2D)

- The EQN prettifier parses showEquation's DISPLAY string, not the
  stored source — superscript exponents are already glyphs there, so the
  grammar only owns what it improves: '/' terms stack as
  standard-context/tiny-child fractions (17 px, inside the 23 px strip
  row) and √ gets its vinculum. `ppSetFontDeep` re-fonts a built subtree
  instead of re-parsing per rung.
- The grammar is deliberately strict: numbers (with verbatim ·₁₀ⁿ
  tails), ASCII names (+ subscript digits), + − × · / ( ) √ = and
  attached sup-runs. Ellipsis truncation, unknown glyphs, or any
  unconsumed tail decline to the linear line. A parse with no fraction
  and no radical also declines — nothing 2D is gained.
- Hook: one hunk at the solver/equation.c paint site, gated to the
  no-cursor path — editing always shows the linear form the cursor
  logic was built for. solver/equation.c was a virgin file (no sibling
  package touches it).

## 2026-08-26 — PP4 (formula view)

- **The browser became a pager (RULED).** The plan's CM-mode browser
  costs ~20 keyboard.c sites in the one upstream file where forth-core
  rewrites the determineItem chain and undo-history already had to
  squeeze its branch into a hunk gap — the project's riskiest
  three-package composition surface. PHIST on the PSHOW manual-paint
  protocol delivers the user goal (seeing the chained operations
  naturally) with zero keyboard/defines churn: repeated presses page,
  any key releases. calcMode 20 stays reserved; the full browser is an
  explicitly possible later upgrade, not a closed door.
- **One constructor pair (`ppfCombine1/2`) serves both the live tree and
  the token stream**, so the two paths cannot drift typographically.
  DIV → FRAC (children never parenthesized — the bar scopes), YX → SUP
  with the base keeping its parens, √ → RAD (the vinculum scopes),
  1/x → FRAC(1,x), x²/x³ → SUP, CHS → leading minus, everything else →
  function form name(…) — which also future-proofs unknown dyadics that
  later become classified.
- **PP_PAREN has two modes chosen at measure time**: glyph parens when
  the child fits the font's '(' ink, synthesized 5 px stroke parens for
  tall children (fraction inside parens). The paint recomputes the same
  test instead of storing a mode bit.
- All six drivers green on the first PP4 build — the PP3 trace helpers
  (real key paths) carried the formula tests for free.

## 2026-08-26 — PP3 (capture engine)

- **Thirteen of sixteen traces passed on the first real-path run** — the
  two-phase STAGE/DONE design and the deferred NIM lift survived contact
  with the actual choreography. The three findings:
  (1) `ppcDeepCopy` masked `PPA_EMITTED` out of `aux` for every node
  kind, but LIT/VAL store a LENGTH there — a copied `"2"` truncated to
  the empty string (T4). Flags now strip for op nodes only.
  (2) A trace script bug, not an engine bug: backspacing an
  already-aborted NIM lands in CM_NORMAL and leaves an error that the
  next DONE treats as invalidation (T8's stray second backspace).
  (3) Arena exhaustion recovers BETTER than designed: after the
  invalidate, ensureKnown rebuilds truthfully from value leaves and the
  chain continues as `# 1 + 1 +` — the pin now asserts the recovery.
- **`nimWhenButtonPressed` is keyboard-owned** and false under a test
  driver, which flips fnKeyEnter's eRPN condition; both the shadow's
  mirror and the driver read/mimic the real global (T15).
- **L degrades to UNKNOWN on every op** rather than deep-copying the
  consumed operand (the design's "move" would alias a child node):
  LASTx returns as a truthful value leaf — `(2+3)×3` renders with the 3
  as a value, which is what the registers say (T13).
- **Deferred lift earned its keep twice**: T8 (abort after ENTER) and
  T16 (abort with ASLIFT set), the latter added when MUT-14 survived T8
  — lift-at-open only mis-fires when the abort follows a lift-armed
  open, and T8's abort follows ENTER (ASLIFT clear).
- **Top-of-stack falloff emits without a result** — by the time the
  shadow applies a lift, the real register is gone; the token stream
  simply omits TKRES for those formulas.
- **Driver hygiene:** the capture driver's real NIM typing left
  `aimBuffer` residue that broke `fn42Alpha` in string_cov.txt ("aimBuffer
  is empty headless") — a one-test blast radius the full-suite gate
  caught. Rule reaffirmed: a driver leaves the machine as it found it,
  including input buffers, not just flags and registers.

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
