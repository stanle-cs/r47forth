# Audit — the round-6 fix wave and the D7-1 tamFinish implementation at `65f2dc709` (round 7)

Range `24bd4db99..65f2dc709` — five same-day commits: `26675d559` (the
round-6 report, docs only), `c701aa72b` (F1–F12 + D7-3, red-first),
`48c893d3c` (R12 + C12 red-first, D7-1 design drafted), `077485e0f`
(D7-1 design rev 2, docs only), `65f2dc709` (D7-1 `_tamLeave`/wrapper
implemented). The regression record said this round's findings would live
in these fixes (r2 4/7, r3 4/4, r5 9/12); the record held in kind and
attenuated in count — two of the seven confirmed findings are behavioral,
and both sit in the blast radius of a round-6 fix (F1's and F7's). The
rest is the record of the D7-1 commit disagreeing with itself. No code
changed this round; the tree the audit finishes on is the tree it started
on.

*File-name note.* The orchestration slug for this round exceeded the
filesystem's 255-byte name limit (1066 bytes); this file carries the
series convention (`AUDIT_round7_<date>.md`) instead, per this document's
own template.

---

## 1. Subject and coverage

**Commits.** `24bd4db99..65f2dc709`, everything measured at the tip
`65f2dc709`, branch `forth-core/stage-n`, tree clean. Every verifier
worktree again spawned STALE at `c3a00768c` (~114 behind); every one
executed the round-6 rule — `git log --oneline -1`, then checkout to the
audited tip — before its first read. The round-6 fix held (§8).

**Files.** The fix wave's whole footprint, read as current code plus the
range diff: `ui/tam.c` (all 28 `_tamLeave` sites mapped to enclosing
functions, `_tamHandleShuffle`, `tamEnterMode`, `_tamLeave`, the wrapper,
`tamProcessInput`'s bracket and epilogue, the GTO→GTOP promotion and
BACKSPACE demotion arms), `programming/manage.c`
(`forthCaptureSuspend`/`Resume` with the F1 re-anchor and F10 kept-steps
splice, `forthFoldEnter`/`Leave`/`UnwindIfDone`, `_forthFoldAdmits`,
`_forthFoldFindCaptureStep`, the FHIST helpers, `fnClP`/`_clearProgram`),
`forth_capture.c/.h` whole, `keyboard.c` (every predicate conversion, the
plane selects, recall guards, R/S and ENTER diverts, `fnKeyExit` end to
end including the R12 and F8 arms, `fnKeyBackspace` both arms),
`screen.c` (the console block, `_forthConsoleEditorTop`/`ViewRows`/
`RollView`, the F7 guard and its sibling long-press branches), `items.c`
dispatch arms, `forth_inner.c` (F11 and the depth/spill machinery),
`forth_prims.c` (every net<0 prim body), `forth_console.c` (view-offset
writers), `forth_menu.c` surface guards, both test part files' diffs and
the round-6 window fixture, and — the census D7-1 owed this round — the
upstream (`src/c47`) bodies of every un-overridden `leaveTamModeIfEnabled`
caller (`flags.c` ×3, `programming/input.c`, `printing/print.c`,
`ui/matrixEditor.c`, `c47Extensions/keyboardTweak.c`) traced for
fold-pending reachability.

**Readers.** Eight in-family dimension finders (D1–D8), blind to each
other, via `audit-workflow.js`; sixteen findings reached the three-lens
refutation pass (reachability / correctness / intent), each verifier in
its own worktree; eleven survived, five were refuted. After merging
cross-dimension duplicates (two findings each independently double-found
— which is evidence, not repetition) the report carries **seven CONFIRMED
and two PLAUSIBLE**. One mutation was run (tests dimension), observed,
and reverted; tree clean after. **The out-of-family pass is dispatched
separately and its results are NOT folded into this synthesis**; whatever
it returns lands as an addendum. It changes nothing about this round's
exit arithmetic — the in-family pass alone produced new CONFIRMED
findings, which resets the count regardless (§8).

**Deliberately not audited.** The standing open findings C5, C6, C7, C10,
C11, C13, C14, C15, C20, C22; the ruled items P1, P2, F13/U5; the round-6
refuted items U2, U3, U4, G-W1a-1, G-W2-1 — all excluded by the brief and
none re-reported here. Fresh surface outside the fix wave's footprint.
Anything the mechanical half reports (§2).

**What the budget did not reach.** `forth_compile.c` beyond its
error-loop structure and the `forthPrimInvoke` call sites the F11
refutation traced; `test_console.part.h` beyond the roll tests;
`fnClP`'s post-delete cursor placement and DELP's TM_LBLONLY keyability
(why P-1 stays PLAUSIBLE); `forth_console.c` ring internals beyond the
view-offset writers; the C12 renderer's behavior when writers append
while scrolled back (pre-existing, untouched by this range); DESIGN.md
end-to-end (each reader consulted it at its anchors; a contradiction
living solely in an unquoted section would have been missed); and — apart
from one gate-driven mutation — no dynamic execution: every behavioral
finding here is a constructed static trace, and says so in its
confidence.

---

## 2. Mechanical results

Measured at `65f2dc709`. **Gate: GREEN**, and its refresh produced no
change to generated output — the tree is clean after the run (the
silent-green trap stays closed; group F below confirms it independently).
Forth self-test ALL PASSED, including the new round-6 window fixture
[1]–[9] and the R12/C12 rulings tests; upstream `meson test testSuite`
1/1 OK (55.95 s), 0 fail. Warning baseline unchanged in kind.

**`design-audit.sh`: 4 finding groups.**

- **A — footprint.** Override files 17 (budget 16), added lines 2384
  (budget 606) — up 228 from round 6's 2156. The delta is the fix wave's
  inline arms plus D7-1; D7-1 itself is net-restorative in
  `lblGtoXeq.c` (+26/−237, the teardown deletions returning that file
  toward upstream shape). Standing overlay cost, already accepted; the
  growth is recorded here as the stage rule requires.
- **B — hunks whose added lines never mention Forth: 28 (baseline 3).**
  Dominated by the twenty-odd one-line `_tamLeave();` swap hunks in
  `010-ui__tam.c.patch` — the D7-1 rename's mechanical signature, a known
  cause — plus the FIX-6B freeList guard and the D7-1 wrapper comments in
  `keyboard.c`. Not new drift in kind, but the detector's signal is now
  saturated by a deliberate rename; worth remembering when B next moves.
- **D — contiguous inline blocks 16 → 34** (round 6: 30). The four new
  blocks are the fix wave's arms in `keyboard.c`/`screen.c`/`manage.c`.
  The largest blocks are the standing F13/U5 subject, unchanged.
- **E — allocations in package sources** (`forth_dict.c`,
  `forth_inner.c`) — the standing lifetime prompt, unchanged.
- **F/G/H** — generated output synchronized with manifest and clean in
  Git; no working-area files would ship; all DESIGN.md citations resolve.

Nothing the mechanical half reports is counted as a finding below.

---

## 3. CONFIRMED findings

Worst first, ranked by what each costs the owner. All survived the
three-lens refutation pass with the verifier granting or constructing the
reaching input; two were independently found by two dimensions each and
are merged, with both routes noted. No patches — findings, not fixes.

---

### C-1 — the GTO→GTOP promotion re-derives fold admission; the sibling BACKSPACE demotion does not: the committed GTO is silently lost

**Double-found:** D1 contracts (reachability lens) at `ui/tam.c:344` and
D7 design (intent lens) at `ui/tam.c:345`, independently. A regression
opened by the F1 fix: pre-fix the promote–demote path stayed ARMED and
folded correctly (while promote–commit crashed).

- **Where.** `packages/forth-core/ui/tam.c:344-348` (the BACKSPACE
  demotion arm: `tam.function` ITM_GTOP→ITM_GTO, min/max reset, nothing
  else) against `:804-816` (the F1 promotion arm, whose `:816`
  `forthCapSetFoldModeRaw(2)` is the file's ONLY mid-session foldMode
  write).
- **Reaching input (five ordinary keypresses).** Console open (FORTH,
  keys mode). **GTO** — `tamEnterMode` → `forthFoldEnter`;
  `_forthFoldAdmits` admits GTO/TM_LABEL (`manage.c:1953-1967`), fold
  ARMED (foldMode 1), capture suspended, cursor parked on FHIST.
  **`.`** — the promotion arm sets `tam.function = ITM_GTOP` and
  re-derives: foldMode 1→2 (PARK). **BACKSPACE** (digitsSoFar==0) — the
  `:344` arm demotes `tam.function` back to ITM_GTO; foldMode stays 2;
  nothing re-derives. **0 5 ENTER** — at commit, `forthFoldArmed()` is
  false, so `tamProcessInput`'s bracket (`:1501-1502`) never forges
  CM_PEM; the commit dispatches `reallyRunFunction(ITM_GTO, 5)` LIVE
  instead of recording the step the resume splice folds; `_tamLeave`'s
  PARK seam resumes with a splice of n==0, and `forthFoldLeave`'s
  `goToPgmStep(savedProgram, savedLocalStep)` (`manage.c:2132`) rewinds
  whatever the live dispatch navigated.
- **What the owner sees.** The typed operation vanishes twice over: "GTO
  05" is neither spliced into the line as text (the identical five keys
  in PEM produce exactly that) nor left in effect as navigation. Because
  the fold parked the cursor on FHIST, `fnGoto` searches FHIST for the
  local label, so the concrete keying most likely raises a spurious
  label-not-found error rather than pure silence — either way the
  admitted item is gone and the console line comes back without it.
- **Why it is wrong.** The class rule this very fix minted
  (DESIGN-HISTORY 2026-08-08): *"any decision cached across a state
  rewrite must be re-derived at the rewrite."* `tam.function` has exactly
  two mid-session rewrite sites; only the promotion re-derives. Also
  DESIGN.md §8.4.3: *"Parity is the spec: a fold spelling defect is by
  definition a divergence from PEM"* and *"One behaviour for one gesture"*
  (L-R4). The intent search was exhaustive: no doc, comment, or commit
  anywhere rules the downgrade one-way — the promotion comment's own
  entry-equivalence logic, applied at the demotion, demands re-arming.
- **Bug class.** Decision cached across a state rewrite, re-derived at
  one of two rewrite sites — the F1 class applied in one direction only.
- **Class test.** For every `tam.function` rewrite site (currently two),
  assert after the rewrite that foldMode agrees with
  `_forthFoldAdmits(tam.function, tam.mode)`. Concretely: drive
  `GTO . BACKSPACE 0 5 ENTER` through the fixture and assert the line
  gains "GTO 05" (PEM-parity oracle), FHIST count reconciled — the
  mirror of class test [2], which pins the promotion direction only.

---

### C-2 — F7's row-juggling class recurs unguarded in upstream `openHOMEorMyM`: the un-overridden twin of the site the round-6 fix guarded

D5 guards (reachability lens). High confidence; the verifier constructed
the full path and failed to break any link.

- **Where.** `src/c47/c47Extensions/keyboardTweak.c:183-190` — inside
  `openHOMEorMyM`'s FLAG_ALPHA branch: `isAlphabeticSoftmenu()` →
  `popSoftmenu()` → raw `showSoftmenu(-MNU_ALPHA)`. No package override
  of `keyboardTweak.c` exists; this is the code that runs.
- **Reaching input.** FORTH (console open, keys plane, FWRD row
  registered, FLAG_ALPHA set — exactly what `fnForthOuter` constructs).
  With HOME.3 (`FLAG_HOME_TRIPLE`) enabled, press **f three times**
  inside the TO_3S_CTFF window (`fg_processing_jm`'s triple detector,
  `keyboardTweak.c:278-287` → `:313`), or hold **f** through the f→g
  long-press ladder (package `screen.c:1023`). The KEY_fg shift arm
  (package `keyboard.c:1739→1749→1636`) runs BEFORE any forthCap
  routing and returns ITM_NOP, so the capture never sees the key. Inside
  `openHOMEorMyM` every gate passes for the console state, and
  `isAlphabeticSoftmenu()` is TRUE for the FWRD row (`isAlphaSubmenu`
  widened to `-MNU_FORTH`, package `softmenus.c:3888`): the console's
  REGISTERED frame is popped — ownership stamp destroyed — and covered by
  a raw unregistered ALPHA row while `forthCapKeysMode()` stays true.
- **What the owner sees.** F7's exact symptom back again: the row reads
  ALPHA while the keypad types the keys plane. Worse than cosmetic: the
  destroyed stamp feeds the C18 class — a later EXIT commits keysMode
  where `forthConsoleShowSurface` is entitled to change nothing, and the
  close accounting pops the wrong frames.
- **Why it is wrong.** The F7 fix's own rule (package `screen.c:915-925`):
  *"the console OWNS its row while a live interactive capture is open …
  isAlphabeticSoftmenu is isAlphaSubmenu(0), widened to -MNU_FORTH in
  Stage L, and this consumer was never re-enumerated."* `openHOMEorMyM`
  is a second un-re-enumerated consumer of the widened predicate,
  reachable by the same gesture family, with no
  `forthCapInteractiveLive()` guard. It is also the census rule the stage
  just adopted, violated by the fix that prompted it: a package-tree grep
  is not an upstream census — F7 guarded only the package-tree copy of
  this shape.
- **Bug class.** Predicate widened for one consumer, other consumers
  unchecked; census short by one — the same class in code that C-4 is in
  the design record.
- **Class test.** Grep-backed enumeration of every
  `isAlphabeticSoftmenu`/`isAlphaSubmenu` consumer in `src/` and the
  package (with the count asserted), each driven with a live console row
  registered, asserting the FWRD frame's stamp survives. Concretely:
  triple-f with HOME.3 from an open console; assert `currentMenu()` is
  still `-MNU_FORTH` and the slot-0 stamp intact — the sibling of
  fixture subcase [6], which pins the `screen.c` door only.

---

### C-3 — C12's band-stays-full invariant is enforced only at roll time: an editor long→short transition under a clamped view blanks the top transcript rows

D3 arithmetic (correctness lens). Medium confidence; latent, cosmetic,
self-correcting — and exactly the symptom C12 was fixed to remove,
re-entering through a door the fix does not guard.

- **Where.** `packages/forth-core/screen.c:5736-5749`
  (`forthConsoleRollView`, the sole count−rows clamp) versus the renderer
  `:5798-5800` (`if(view >= count) continue;` — its only guard).
- **Reaching input.** Console with ≥5 transcript lines. Type a line long
  enough for the long-line state (`yMultiLineEdOffset` 1, rows = 2).
  g-roll-up until the view clamps at count−2. BACKSPACE the line back
  under the boundary (rows becomes 4) without touching a roll key. The
  next `_forthConsoleRender` computes rows = 4 against the stale offset:
  for the top two rows `view ≥ count` and nothing paints. Exhaustively
  verified: no editing path writes the view offset, the ring's own
  setters clamp only at count−1, and no render-time clamp exists.
- **What the owner sees.** After deleting text, the top two transcript
  rows sit blank though lines exist to fill a re-clamped band; persists
  across repaints until a roll key re-clamps or output snaps the view to
  0 (N-R3).
- **Why it is wrong.** The fix's own invariant comment (`:5739-5741`):
  *"The VIEW stops at count-rows, so the band stays full."* With rows
  shrunk out from under a clamped offset, the view sits past count−rows
  and the band is not full. Class: *clamp without its viewport* (C12's
  own class) — the viewport is now consulted, but only at roll time,
  while rows is frame-variable.
- **Bug class.** Write-time clamp against a frame-variable bound.
- **Class test.** For each rows transition (2→4 and 4→2): set the view to
  the old state's clamp, cross the long/short boundary by editing, render,
  and assert every band row above the newest paints a line whenever
  `count >= rows` — i.e. the render-time invariant `view <= count - rows`.

---

### C-4 — the approved D7-1 design says ELEVEN swap sites; the implementation swapped 28; the ruling document was never corrected

**Double-found:** D7 design (correctness lens) at
`DESIGN_D7-1_tamFinish_2026-08-08.md:67` and D8 upstream (reachability
lens) at `ui/tam.c:227`, independently. **No runtime defect** — both
verifiers independently reconstructed the dominance proof (all 28 sites
in `_tamHandleShuffle` and `_tamProcessInput`; sole callers `:263` and
`:1503`; the epilogue's `forthFoldUnwindIfDone` on every exit path), so
the blanket swap is the uniformly safe form and the widening itself was
REQUIRED (leaving 17 sites on the wrapper would have reintroduced the
L1-F2 rev-2 unwind-before-dispatch loss). The defect is the record.

- **Where.** `design-docs/forth-core/DESIGN_D7-1_tamFinish_2026-08-08.md`
  lines 16-19, 67, 122-123 ("the eleven … leave-then-dispatch sites …
  eleven same-file swaps") versus the 28 `_tamLeave()` call sites in
  `packages/forth-core/ui/tam.c` (26 in `_tamProcessInput`, 2 in
  `_tamHandleShuffle`); `tam.c:1390`/`:1453` cite "D7-1 (approved
  2026-08-08)" as their authority; `git log` shows the doc untouched
  after `077485e0f`.
- **What it costs.** The owner approved a design whose central
  enumeration was wrong by more than 2× — and wrong at drafting time,
  not overtaken by events: four of the eleven listed line numbers (913,
  980, 996, 1130) match no site in any relevant revision. The
  non-normative DESIGN-HISTORY and the commit message restate "28" as
  though it were the design; the approved doc still says eleven; the
  doc's own enforcement rule ("any NEW direct caller of `_tamLeave` …
  is a finding by definition") therefore mislabels 17 sanctioned sites
  against its own baseline. The predicted cost has already been paid
  once, inside this audit: the round's tasking carried the stale
  "approved design said eleven" baseline and forced the from-scratch
  reachability re-derivation both verifiers then performed.
- **Why it is wrong.** The project's own class — *"a human list of call
  sites/arms/consumers not backed by a build-time count is a comment, and
  it comes back short"* (bug-classes.md) — recurring inside the design
  process the same week the class was re-confirmed in code (the F4
  census, C12's missed site, the upstream-caller census gap, and C-2
  above).
- **Bug class.** Enumeration without a count check, at design level; an
  approval basis diverging from the landed change with no amendment.
- **Class test.** Process pin, not code: the doc amendment (28, with the
  grep command and count inline), and packet lint extended to check any
  design enumeration that names a count against a grep of the tree.

---

### C-5 — D7-1's enforcement deliverable never landed: no `_tamLeave`/wrapper-bypass lens line in PROMPT_CODE_AUDIT.md, and the one undefendable direction is recorded nowhere

D8 upstream (intent lens). High confidence; mechanically verified
(`grep` empty, `git log` for the file empty across the range; the file's
whole history is one round-1 commit).

- **Where.** `design-docs/forth-core/DESIGN_D7-1_tamFinish_2026-08-08.md`
  "Enforcement and tests": *"PROMPT_CODE_AUDIT.md's D1 lens gains one
  line: any NEW direct caller of `_tamLeave`, or any teardown path that
  bypasses the wrapper, is a finding by definition."* The commit landed
  the code and the mutation pin, not this line. The three owed-items
  lists (commit message, DESIGN-HISTORY, HANDOFF addendum) each name only
  the six-caller census — the lens line is neither done nor deferred
  anywhere. A drop, not a deferral.
- **What it costs.** The class guard D7-1 names as its own enforcement
  exists only in reviewer memory. And the one direction the promised
  line would NOT have covered is recorded in neither the Risks section
  nor DESIGN-HISTORY: a future upstream merge adding a new in-file
  `leaveTamModeIfEnabled(); <dispatch>` site inside `_tamProcessInput`'s
  call tree merges cleanly (it references the public name — no patch
  conflict), links the WRAPPER, and fires the unwind before the dispatch
  inserts its step — the exact L1-F2 rev-2 typed-line loss D7-1 exists
  to close, with the gate green because no fixture drives the new site.
- **Bug class.** Promised enforcement not landed; a by-construction
  guarantee whose construction rule is written nowhere it will be read.
- **Class test.** The lens line itself, plus the future-upstream
  direction named in the design's Risks; packet lint checking a design's
  "Enforcement and tests" items against the repo would have caught the
  drop mechanically.

---

### C-6 — the Live predicate's "every site that means live must ask THIS one" contract is enforced only by enumeration: three sites hand-roll the conjunction and the header's recall-arm comment is stale

D7 design (intent lens). High confidence. UNREACHED today —
`forthCapIsInteractive() && forthCapIsOpen()` is currently bit-identical
to `forthCapInteractiveLive()` — the fork arms the day Live's definition
moves or a copy is edited alone.

- **Where.** `packages/forth-core/forth_menu.c:543` and `:627`
  (ShowSurface/RestoreSurface), `programming/manage.c:1512` (the N1-6
  post-line surface-repair choke) — all three exactly the "render, route
  or gate site that means 'live'" the header contract
  (`forth_capture.h:104-112`) reserves for the named predicate, violated
  the day it shipped. Plus `forth_capture.h:~170`, which still documents
  the CHR_caseUP/CHR_caseDN recall arms as "guarded on
  forthCapIsInteractive()" when the F6 fix moved them to
  `forthCapInteractiveLive` (`keyboard.c:2850/2871`) — false data for the
  next enumerator.
- **What it costs.** Duplicated truth with nothing forcing agreement, on
  a predicate whose minting sweep already came back one site short once
  (the determineItem roll gate, caught and fixed in `48c893d3c` —
  the record's own proof that enumeration is the only enforcement). The
  r5 origin-vs-openness confusion re-enters through whichever copy
  drifts first.
- **Bug class.** Predicate duplication; comment that outlived its
  mechanism; enumeration without a count check.
- **Class test.** A grep pin: no site outside `forth_capture.c` conjoins
  `IsInteractive` with `IsOpen` (count asserted zero); the header comment
  corrected in the same edit.

---

### C-7 — vacuous half of the residue fixture's guard: `tam.mode != 0` tested two lines after `tam.mode = 0`

D6 tests (correctness lens). High confidence — **mutation-proved**:
replacing the disjunct with constant 0 left the full gate GREEN with
identical output (mutation applied and reverted in the same step; tree
clean).

- **Where.** `packages/forth-core/test_capture.part.h:17427`, in
  `test_fold_round6_window` subcase [7]: `:17425` assigns `tam.mode = 0`;
  `:17427` guards `if (tam.mode != 0 || !forthCapIsSuspended())`. The
  intervening `clearSystemFlag(FLAG_ALPHA)` is provably inert with
  respect to `tam` (flags.c never references it; FLAG_ALPHA falls to
  `default:` in `systemFlagAction`). The first disjunct can never be
  true on any tree, fixed or broken.
- **What it costs.** Nothing observable today — the live half
  (`!forthCapIsSuspended()`) still carries the guard against the
  realistic regression (someone restoring `leaveTamModeIfEnabled()`
  here, whose wrapper now resumes the capture). But the printed message
  claims the guard verifies TAM teardown, and that half verifies its own
  assignment: a reader auditing fixture coverage is told more was checked
  than was.
- **Bug class.** Vacuous assertion; a test whose message claims more than
  its body checks (the dimension's own class, D6).
- **Class test.** The mutation that proved it, kept as the pin in
  reverse: rewrite the guard so that mutating the disjunct to 0 reddens
  — i.e. make the fixture's residue priming observable through a real
  teardown call rather than re-read.

---

## 4. PLAUSIBLE findings

Survived the refutation pass; nobody constructed the reaching input. Each
says what would settle it.

**P-1 — `forthCaptureResume`'s abandon arm skips the F1 re-anchor;
`forthFoldLeave`'s debris sweep then compares FHIST's entry count against
an arbitrary current program** (`programming/manage.c:2100`, D1
contracts). The candidate door is DELP of the FHIST program keyed from a
live console: DELP is fold-non-admitted (PARK), its commit dispatches
`fnClP` LIVE before `_tamLeave`, deleting FHIST including the parked
capture step; resume's canary falsifies, `_forthFoldFindCaptureStep`
returns NULL (the FHIST label is gone), resume exits through
`forthCapAbandonSuspended` at `:1303` BEFORE the `:1357` re-anchor — and
the abandon deliberately must not touch foldMode, so the sweep still
runs, keyed on whatever program `fnClP` left the cursor in, deleting up
to 4 real steps of a stored user program and then restoring a
stale program number. The verifier confirmed every link of the mechanism
including the one refutation door (abandon clearing the fold — ruled out
by `forth_capture.c:104-106`'s own MUST-NOT). Two links stand unverified:
(a) that DELP's TM_LBLONLY prompt actually offers/accepts the FHIST
label from the console (plausible — FHIST is an ordinary GLOBAL label in
`labelList`), and (b) a landing program longer than FHIST's entry
count + 1 (ordinary user state). **Settles it:** drive FORTH → DELP →
FHIST in the fixture or simulator. If (a) holds this is silent damage to
a saved program — the F1 class, closed in the splice, open in the sweep
— and it moves to CONFIRMED at a rank above C-1. If (a) fails, the
residual is a defensible line-abandon and the finding dies.

**P-2 — F8's Live flip mis-routes mid-TAM keys to the AIM column in the
foldMode-0 suspension** (`keyboard.c:1789`, D2 lifecycle). Mechanism
verified end to end: in the state (origin INTERACTIVE, FCAP_SUSPENDED,
`tam.mode != 0`, fold NOT pending), the post-F8 conjunct
`!(forthCapInteractiveLive() && keysMode)` passes where the pre-F8
origin predicate blocked, and the CM_AIM column arm wins over the
`tam.mode` → `primaryTam` arm — a genuine F8 regression for keysMode-ON
sessions, matching the round's fix-regression pattern. But the ONLY door
into that state is `forthFoldEnter`'s "no program, no fold" arm firing —
`forthHistoryEnsure()` returning false — and that premise is the one the
audit record has killed twice by name (AUDIT_stages-K-to-N §6a R1;
AUDIT_round5 item (b): `_insertInProgram` has no failure return; on
target FHIST exists even in extremis; no fault-injection hook exists), a
ruling this round's refutation pass re-affirmed to kill the sibling
finding (§6, R-1) while P-2's own verifier granted the door via the
`scanLabelsAndPrograms`/NULL-`labelList` route the K-N ruling classed as
pre-existing corruption. The two verifiers disagree on constructibility;
the record's tie rule favors the standing ruling. **Settles it:** either
an owner ruling extending the dead-premise ruling to this consequence
(P-2 dies with it, and the defensive `foldMode 0` arm keeps its comment),
or a fault-injection hook for `forthHistoryEnsure` — which would also
let a fixture pin the whole foldMode-0 family at once.

---

## 5. Design observations (D7)

Shape, not defects. These outlast the bug list.

**D7-a — the round's dominant class is now "enumeration without a count
check," and it operates at every level of the project at once.** In code:
F7 guarded one of two consumers of the widened predicate (C-2); the F8
sweep came back one site short and was patched in `48c893d3c`; the F1
fix re-derived one of two rewrite sites (C-1). In the record: the
approved D7-1 doc counted eleven of 28 (C-4), and its listed line
numbers were wrong at drafting time. In the process: this round's own
tasking inherited the eleven. Every one of these is the same defect —
a hand enumeration standing in for a counted one. The cheap
countermeasure is uniform: any fix or design that enumerates sites
carries the grep and the count, and packet lint checks it. Until that
lands, expect the next round's findings in whatever the next fix
enumerates.

**D7-b — D7-1's construction is sound; its record is what will fail.**
The wrapper genuinely closes the round-6 strand class by construction —
the 28-site swap is epilogue-dominated (independently proven twice), the
thirteen external unwind deletions are call-for-call covered, and the
owed six-caller upstream census closes clean (§6). But the guarantee is
measured against a baseline the design doc states wrongly (C-4), enforced
by a lens line that was never written (C-5), and has exactly one
direction no construction can defend — a cleanly-merging future upstream
in-file caller of the public name — which no document names. A
by-construction invariant whose construction rule lives only in reviewer
memory is an invariant for one rebase.

**D7-c — duplicated-truth seams armed for the next edit.** The three
longhand Live composites and the stale header comment (C-6). The R12 arm
pair's predicate asymmetry (EXIT on origin, BACKSPACE on Live) — refuted
as a defect (§6, R-4: the divergence is unreachable), but undocumented at
either arm; one comment is cheap and the next editor will otherwise
"fix" the asymmetry in whichever direction they first read. F11's
all-or-nothing-on-error contract for prim fns, stated inside the invoker
rather than at `forth_prims.h` where the next prim author looks (§6,
R-5). None of these is worth a fix wave; all of them are worth a line at
the site the next edit will touch.

**D-audit corroboration.** Group B's jump to 28 IS the D7-1 swap
signature; group D's four new blocks are the fix wave's arms; group A's
growth is measured and recorded. The mechanical half agrees with the
design read; none of it is a separate finding.

---

## 6. Deliberately not flagged / refuted

Mandatory, and this round it is most of the audit: the special-attention
items came back predominantly CLEAR. Refutations first, then the cleared
census organized by the brief's seven attention items, then the standing
set.

### Refuted by the pass

**R-1 — interactive TAM over a failed FHIST creation suspends onto and
deletes a user program step; the same door falsifies D7-1's
"residue unreachable" claim (`ui/tam.c:1196`).** REFUTED on intent and
record: a third resurrection of a twice-killed premise. The entire chain
hangs on `forthHistoryEnsure()` returning false, and the record kills it
by name at these exact sites: AUDIT_stages-K-to-N §6a R1
(`_insertInProgram` has no failure return — the OOM arm exits on sim or
writes anyway on DMCP, so FHIST exists even in extremis) and
AUDIT_round5 item (b) (every failure mode inside ensure faults before
returning false; no fault-injection hook, so the harness cannot reach it
either). The range diff touches neither `forthHistoryEnsure` nor
`_insertInProgram`, so nothing reopens the ruling. The new consequence
dressing inherits the dead premise wholesale. (P-2 conditions on the
same door and is held PLAUSIBLE only because its mechanism, unlike this
finding's, is a verified regression if the door ever opens.)

**R-2 — `forthFoldEnter` has no pending-fold guard; a nested
`tamEnterMode` clobbers the single fold context (`manage.c:1996`).**
REFUTED on intent: ruled, not overlooked. The single-instance,
non-stack fold context is documented as deliberate in four places,
including the very comment the finding cited as violated — which IS the
ruling ("unreachable by construction; if forceTamAlpha is ever wired
live, this bracket must be made reentrant first"). The nested
tamEnterMode-over-pending-fold case has its own explicitly enumerated
arm (L1-F2 rev 3, `ui/tam.c:1197-1207`), and the one untraced candidate
door does not exist: the Stage M long-press band (`screen.c:905-965`)
never calls `tamEnterMode`, and its USER-mode dispatch hits the
`items.c` interactive divert.

**R-3 — DESIGN.md §8.4.3 still names the pre-D7-1 unwind owner set; the
authoritative doc was never amended (`DESIGN.md:2694`).** REFUTED on
correctness: the load-bearing hop is false at the tip. Both TAM-cancel
arms inside `fnKeyExit` (`keyboard.c:3951`, `:3999`) call
`leaveTamModeIfEnabled()` with explicit D7-1 comments, and the wrapper
IS `_tamLeave()` + `forthFoldUnwindIfDone()` — the doc's sentence ("the
unwind runs from … fnKeyExit") remains true, one well-commented call
deep. D7-1 widened the owner set in the safe direction, and
DESIGN-HISTORY — the designated amendment trail — records it in full.
Contrast C-4/C-5, which are NOT this finding: the divergences that
survive are in the D7-1 doc itself and the promised prompt line, not in
DESIGN.md.

**R-4 — R12's two dismiss-first arms encode different predicates while
both claim to mirror CM_NORMAL (`keyboard.c:4088`).** REFUTED on
reachability, by construction: the predicates diverge only in
(origin INTERACTIVE, SUSPENDED), and no input reaches either CM_AIM arm
in that state — the sole suspension entry always has a fold pending;
EXIT and BACKSPACE are intercepted while TAM is live; every TAM ending
settles the state; and the reported line is reachable only in FCAP_OPEN,
where the predicates agree. What survives is a one-line comment
suggestion (D7-c), not a defect.

**R-5 — F11's refusal un-apply rests on an all-or-nothing-on-error
contract stated only inside the invoker (`forth_inner.c:171`).** REFUTED
on correctness: the projected consequence ("every later depth check
passes against a phantom cell") never materializes even for the
hypothetical consume-then-error prim — all four `forthPrimInvoke` call
sites abort the line on `lastErrorCode` immediately after the invoke, no
later depth check runs on the errored line, and the next line's entry
bracket zeroes the depth before any check. The worst realizable effect
is one loud stop replacing another. What survives is the documentation
placement (D7-c).

### Cleared by the finders and verifiers, per the brief's attention items

**(1) The 28-site `_tamLeave` swap — the census question answered: NO
site is reachable outside `tamProcessInput`'s epilogue.** Proven
independently by five dimensions on the same call-graph facts: the 28
sites live in exactly two static functions — `_tamHandleShuffle` (2
sites; sole caller `_tamProcessInput:263`) and `_tamProcessInput` (26;
sole caller `tamProcessInput:1503`) — and the epilogue's
`forthFoldUnwindIfDone()` runs on every exit path. None lost its fold
unwind; converting all 28 rather than the design's eleven was REQUIRED
(the 17 left on the wrapper would each have been an L1-F2 rev-2
unwind-before-dispatch site). The thirteen deleted round-6 manual
unwinds are call-for-call covered by the wrapper at every external site.
`tamEnterMode:1182` deliberately keeps the public wrapper (CM_NIM arm;
fold cannot pend in CM_NIM; settling a prior bracket before a new TAM is
the commit's stated intent). Wrapper re-entry mid-dispatch is closed in
both fold states: under ARMED the forged CM_PEM records a step so
wrapper-calling functions never run live inside the bracket; under PARK
the resume already ran at `_tamLeave`'s tail and the epilogue's second
unwind no-ops on foldMode 0. What survives of item (1) is only the
record divergence, C-4.

**(1b) The owed six-upstream-caller census — executed this round, comes
back CLEAR.** `flags.c` (three FLAG_ALPHA arms), `programming/input.c`
(program-run only), `printing/print.c` (CM_NORMAL-gated),
`ui/matrixEditor.c` — unreachable with a fold pending (armed-fold
commits are step-recorded under the forged CM_PEM, so those bodies never
run inside the bracket). `keyboardTweak.c`'s sites ARE reachable mid-TAM
and now settle the fold correctly through the wrapper — pre-D7-1 they
were unguarded strand doors; D7-1 closed them exactly as designed. (The
`keyboardTweak.c` finding that DOES survive, C-2, is a different
function and a different class — menu-row juggling, not teardown.)

**(2) The `forthCapInteractiveLive` conversion — every site asks the
right question.** All 14 sites (the brief said ~12; the census counted
14) genuinely ask "is there a live line." The sites left on the origin
predicate each verified to need origin or to be Live-equivalent: the
EXIT ladder's deliberate residue handling, `_forthCapCloseIfInteractive`
and the close funnels that must cover SUSPENDED, resume internals where
the capture was just reopened (`manage.c:1416`), and the three composite
conjunctions (which are C-6's subject as duplication, not as wrong
semantics). `forthPickerGuard` still reads origin: during suspension the
FWRD row is buried and InsertName refuses on a non-open line — dead
gesture at worst, pre-existing.

**(3) F1's re-anchor + clamp — mechanically sound.**
`defineCurrentProgramFromCurrentStep` runs after the canary validates
`p` and before both step-count samples, so both are FHIST-scoped; the
splice count is clamped and NULL/END-guarded in the sweep's own shape;
the `GTO . .` reproducer (class test [9]) is green with the
FHIST-count-unchanged oracle — which also refuted the concern that
GTOP's END-creation arm would splice a spurious "END" into the line.
What survives of the F1 blast radius is C-1 (the admission half, not the
splice half) and P-1 (the abandon arm, not the resume arm).

**(4) F11's un-apply — correct for every prim in today's table, verified
per body.** EMIT and `.$` raise their errors before any effect;
RECURSE/GLOBAL/IMMEDIATE are net-0; the arithmetic wrappers delegate to
C47 fns whose refusal convention preserves operands. The errBefore
conjunct is unfalsifiable at both call sites (defensive noise, harmless);
the skipped settle deliberately leaves the spill for the designed
line-end loud stop, which the fix's comment names as intended. The
hypothetical future consume-then-error prim is R-5's territory and
bounded there.

**(5) F10's `_forthFoldKeptSteps` — lifecycle closed under every
ordering.** Reset at foldEnter, written only by a real resume (the
early return precedes the assignment), consumed and cleared at foldLeave;
the PARK double-resume cannot double-write (second resume no-ops on
!IsSuspended); stale-after-PEM-resume is covered by the foldEnter reset;
the sweep threshold entry+1+kept is consistent for every splice
disposition (a kept step can never be a sweep victim); uint16 overflow
needs ~65k steps. The kept step surviving in FHIST is the fix's designed
disposition.

**(6) R12's arms — faithful mirrors, as ruled.** The EXIT CM_AIM dismiss
(clear only) matches EXIT's CM_NORMAL arm exactly, including touching no
screenUpdatingMode (btnReleased's refresh clears it after return); the
BACKSPACE arm matches CM_NORMAL's clear + ~SCRUPD_MANUAL_STACK + return
shape in-file. The predicate asymmetry is R-4, refuted; the residue
recovery arm keying on CM_AIM only is explicitly defensive post-D7-1.

**(7) C12's layering — clean, and the ruling honored.** RollView = ring
roll then view clamp, single production caller (the shiftG gesture,
which is reachable in both editor states); geometry single-sourced
through `_forthConsoleEditorTop`/`forthConsoleViewRows` in both renderer
and clamp; rows arithmetic (128|67 − 24)/21 = 4|2 matches N-T1 and the
test's count-6/rows-4/max-2 expectation; the stored offset cannot go
stale against COUNT because every ring writer snaps the view to 0
(N-R3) — a nonzero offset implies no ring mutation since the roll. What
survives is C-3: the offset CAN go stale against ROWS, the one variable
N-R3 does not govern.

**Test work (D6), cleared subcase by subcase.** Every round-6 window
oracle [1]–[9] was traced to the pre-fix code path that reddens it —
none is unable to fail ([9] fails by SIGSEGV and is deliberately ordered
last). Subcase 5's inverted assertion is a documented contract migration
with F10, direction-checked. The `_consoleOwnershipOk` relaxation
(IsOpen → IsOpen||IsSuspended) is a deliberate, ruling-backed weakening;
both quoted authorities exist (the comment's `forth_menu.c:302` is ~100
lines stale — beneath the reporting bar). The C12 test's "both
directions" message mildly overclaims (the −1 assertion pins the ring's
bound, not the clamp) — noted, below the bar. Test [8]'s local capacity
re-derivation fails loud on divergence, so it cannot rot in the passing
direction. The D7-1 wrapper-revert mutation pin holds by trace (revert
leaves the fold armed; [1] fires).

**Standing looks-like-a-bug set, unchanged.** The g-long-press MyAlpha
push over a live console row (round-5 ruling: benign overlay, pops
nothing — unlike C-2's destructive pop). Catalog-driven TAM commits
executing without folding (Known v1 limitation (a), deferred to T7.8).
The F7 fix's `else { … }` around an un-reindented upstream block —
patch-minimal form, good discipline. `fnKeyExit`'s TAM-branch unwind
moving ahead of the PEM scroll tail — inert (the fold never arms for a
CM_PEM capture). GTO-from-console navigation undone by foldLeave's
cursor restore — PARK's documented borrow-and-restore semantics (the
defect in C-1 is the lost FOLD, not the restored cursor). The
"ten deletions" (design) vs "thirteen" (commit) count noise — subsumed
by C-4.

---

## 7. Verdict

**Would I ship it? Closer than round 6 by a wide margin — but not
before C-1 and C-2.** Round 6's answer was a flat no with a
three-keypress SIGSEGV; round 7's worst is a silent wrong-result on a
five-keypress gesture and an ownership corruption behind a non-default
setting. The wave itself held up far better than the regression record
predicted: the wrapper closes the strand class by construction, the
28-site swap is safe, the six-caller census is clean, and the
special-attention items came back predominantly clear.

**Where it breaks first.** `GTO . BACKSPACE 0 5 ENTER` in the console
(C-1): the committed operation vanishes — no text in the line, no
effect, at best a spurious label-not-found. Second: triple-f with HOME.3
enabled from a live console (C-2): the registered FWRD frame is popped
and the plane/row divergence plus C18-class close-accounting damage
follows. Both are regressions of round-6 fixes, and C-1 was opened by
the same commit that minted the rule it violates.

**The fix-wave order the evidence carries.** C-1 red-first (the fixture
gesture is five keys; the class test is the mirror of the existing [2]).
C-2 with a grep-backed consumer census in the same commit — the fix that
guards one more site without the census will be round 8's finding. P-1
settled next (one fixture drive; if the DELP door opens it outranks
everything above). C-4 and C-5 as one documentation commit: they are
cheap, and the audit process itself consumes those documents — this
round already paid for the stale baseline once.

**What I would leave alone if the goal were correct code rather than an
audit-clean tree.** C-7 (the live half still carries the guard; fold the
rewrite into the next fixture edit). C-6 (bit-identical today; fix it
the next time anyone touches the predicate, with the grep pin). C-3 is
the owner's call — a render-time clamp versus accepting a
self-correcting transient; the invariant comment should be amended
whichever way it goes. P-2 needs no code at all unless the owner
declines to extend the dead-premise ruling. None of these should gate
the wave that fixes C-1/C-2.

**The pattern, seventh round running.** Both behavioral findings are the
same shape: a fix that enforced its own rule on an enumerated subset of
the rule's sites. The fixes' MECHANICS are now consistently sound — what
fails is the counting. D7-a is the round's real deliverable.

---

## 8. Round and exit state

**Round 7.** Subject `24bd4db99..65f2dc709`, the round-6 fix wave and
the D7-1 implementation. Tree clean, gate green, everything measured at
`65f2dc709`.

**Readers.** Eight in-family dimension finders (D1–D8), blind, via
`audit-workflow.js`; sixteen findings into the three-lens refutation
pass (reachability / correctness / intent), each verifier in its own
worktree; eleven survived, five refuted; two cross-dimension duplicate
pairs merged → seven CONFIRMED, two PLAUSIBLE. One mutation run
(C-7's proof), applied and reverted per the rule. **The out-of-family
pass is dispatched separately and is NOT in this synthesis** — its
findings, when they land, go through the same refutation pass and attach
as an addendum to this report.

**What this round settled.**

- **The D7-1 owed census (the six un-overridden upstream callers) —
  EXECUTED, clear** (§6, item 1b). The one debt D7-1's record named is
  paid.
- **The special-attention items (1)–(7) — all substantially cleared**;
  what survives of each is enumerated in §3/§6.
- **The regression record extended:** round 7 adds (r7: 2 behavioral
  confirmeds, both in round-6 fixes' blast radius) — the rate is falling
  (r5 9/12 → r7 2-of-7-confirmed behavioral), the shape is constant
  (enumerated-subset fixes).
- **Five refutations recorded** (§6), one of which (R-1) is a third
  resurrection — the dead-premise ruling should be cited in the
  `ui/tam.c:1195` comment so a fourth finder finds the ruling before
  re-deriving the finding.

**Process notes (the growth rule).** (1) The stale-worktree trap fired
again for every verifier and the round-6 fix caught it every time —
every verifier's evidence block opens with the checkout; the rule has
paid for itself twice now and needs no further change. (2) The round's
own tasking carried the D7-1 doc's stale "eleven" baseline into the
special-attention brief — C-4's predicted cost, demonstrated inside the
audit that found it. The countermeasure is C-4's own fix plus D7-a's
count-check rule; no new tooling. (3) The orchestration slug exceeded
the filesystem's name limit; report filenames come from the template's
`AUDIT_<subject>_<date>` convention, with the subject SHORT — the slug
is not the subject.

**Exit criterion: NOT met, and reset.** Two consecutive rounds with no
new CONFIRMED finding, at least one out-of-family, close the audit.
Round 7 produced seven new CONFIRMED findings (two behavioral), so the
count resets: the earliest close is now round 9, and at least one of the
two closing rounds must be out-of-family. Round 8's gate items: the
C-1/C-2 fix wave (red-first, with the counted censuses), the P-1 fixture
drive, the C-4/C-5 documentation commit, the owner's C-3 ruling and the
P-2 dead-premise ruling, and the round-7 out-of-family results folded
in. Open findings C5–C22 carry forward per the handoff, untouched by
this round's scope.

---

# Addendum — the out-of-family pass and the executed evidence (same day)

The two items §8 left open are closed. Everything below was measured at
`65f2dc709`; the owner's tree was never touched — all drives ran in a
disposable worktree (`/tmp/claude-1000/r7-simdrive`, left in place with
its driver, logs and screenshots for inspection).

## The out-of-family pass ran: four packets, both readers, identity
verified every dispatch

Sol (GPT-5) took the D7-1 wrapper design with the mid-dispatch upstream
shapes: **no defect reached** — the CM_PEM forge and the `_forthFoldAdmits`
set close every concrete mid-dispatch ordering the packet could construct,
independently matching the in-family clearing (§6, item 1). What Sol adds
is a D7 note for the record: the wrapper's "correct by default" rests on a
nonlocal invariant it cannot enforce (a future external caller that
records its committed step AFTER its wrapper call reproduces the L1-F2
rev-2 loss); today's admission/recording correspondence is the enforcement,
and it lives in code, not in any document — C-5's gap, seen from outside.

Gemini 3.1 Pro took three code packets (the resume splice, the fold
bracket, the keyboardTweak gesture). Eleven out-of-family findings went
through the standard one-lens-per-finding refutation in isolated worktrees:
**two survived, nine refuted** — full verdicts in
`AUDIT_round7_out-of-family_2026-08-08.md`:

- **OOF-1 (CONFIRMED, new)** — `openHOMEorMyM`'s `fnExitAllMenus(0)` arm
  (`src/c47/c47Extensions/keyboardTweak.c:260`) wipes the whole softmenu
  stack two arms after its own wrapper resumed and re-registered the live
  console row. A SECOND mechanism at C-2's site: the
  isAlphabeticSoftmenu-census fix shape does NOT cover it. The C-2 fix's
  census unit is therefore openHOMEorMyM's row-destroying calls, not the
  predicate's consumers alone.
- **OOF-2 (DUPLICATE-CONFIRM of C-2)** — the fold-pending arm: the fff
  detector's wrapper settles the fold, the resume re-sets FLAG_ALPHA
  (manage.c:1402) and re-registers the frame, and the pop lands two
  statements later. Same door as C-2, not a second one — but it constrains
  the fix: the guard must hold POST-resume, in the state the wrapper
  itself creates.
- The nine refutations are all premise failures the packets could not
  carry: TAM commits insert AFTER the parked capture step; the splice's
  `saved` is the suspend-time snapshot; FHIST's 1024-byte cap makes >512
  steps unconstructible; `deleteStepsFromTo`'s `+2` relocates the
  in-bounds `.END.` sentinel (byte-identical upstream — flagged
  independently by two packets; do not "fix" it).

## P-1 is CONFIRMED by execution, and outranks C-1

The §4 drive ran (worktree, driver copy-adapted per the run-sim skill,
REACHED asserts quoted in the drive log). All three questioned links hold,
EXECUTED:

- The TM_LBLONLY prompt ACCEPTS the FHIST label (`lastErrorCode` 0 after
  commit; `ui/tam.c:961-963` resolves any GLOBAL label and FHIST carries a
  real `LBL 'FHIST'`).
- `fnClP` deletes FHIST from inside its own fold (both memory layouts).
- **In the FHIST-before-program order — the ordinary state when the
  console was used before the program was written — the debris sweep
  destroys four real user-program steps: 13 → 9, decoded before/after
  showing `111 222 333 444` gone.** In the program-first order the sweep
  lands on FHIST's leftover and the program survives. In BOTH orders the
  typed line is lost unconditionally, the capture is abandoned, and
  `aimBuffer` is left holding TAM's scratch `"FHIST"` while the FWRD menu
  id still shows.

Reaching input, five ordinary gestures: console open → DELP → spell
`FHIST` → ENTER. **P-1 moves to CONFIRMED at the rank §4 pre-committed:
above C-1** — silent destruction of a saved program, layout-dependent,
plus unconditional line loss. The round's confirmed count is nine
(C-1..C-7, P-1, OOF-1); the fix-wave order in §7 is amended to lead with
P-1's abandon-arm guard.

## C-2's evidence is now executed, on screen

Same worktree, the long-press door through subcase [6]'s timer chain:
before/after screenshots (viewed, non-blank; BMP+PNG pairs in the
worktree) show the typed line `1 2` under the FWRD row, then the same
live line under the full ALPHA keyboard — with the ownership census wiped
(owned=0, borrow=0, stamp=0, capture OPEN, keysMode=1) and a driven
keypress still typing into the line (`aimBuffer "1 2" → "1 21"`). The
fold variant shows D7-1 itself working (fold settled mid-gesture, line
recovered) with the softer symptom: HOME buries the just-restored row
rather than destroying it. C-2's evidence strength moves from
constructed-path to executed + screenshot.

## Process, for the growth rule (all encoded before this addendum)

- **The first refutation launch was VOID and its report deleted:**
  extraFindings passed in the wrong shape collapsed in the dedup on
  undefined keys, the verifier fan-out died, and the synthesis still wrote
  a confident single-reader report — the round-3 defect wearing the
  pass's clothes. `audit-workflow.js` now refuses malformed extraFindings
  loudly before any agent runs. Corollary, unfixed: a stage failure does
  not stop the synthesis — a workflow's counters (`verified: 0`) are the
  ground truth to check against its prose.
- **Sixth packet-defect class: context mislabeled as subject.** The P2
  packets' Subject said "every function below is same-day fix code" while
  two upstream-verbatim helpers were included only as context; both drew
  out-of-scope findings. The template now requires labeling context-only
  functions at their fence.
- The report-filename/slug-length note from §8 stands; this addendum's
  sibling file follows the convention.
