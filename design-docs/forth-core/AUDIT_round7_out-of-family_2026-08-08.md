# Audit — round 7 out-of-family refutation pass at `65f2dc709` (addendum to AUDIT_round7_2026-08-08.md)

Range `24bd4db99..65f2dc709`, same subject as the in-family report; this
file is the out-of-family addendum that report's §1 and §8 promised.
Eleven findings from four out-of-family sources went through the same
three-lens refutation pass as the in-family sixteen: **two survived (one
new CONFIRMED, one DUPLICATE-CONFIRM of C-2), nine refuted.** The fold/
splice/sweep machinery absorbed eight independent adversarial
constructions and lost none of them; both survivors are in
`openHOMEorMyM`, the function C-2 already convicted, and both ride the
same newly-minted shape: the D7-1 wrapper settling a fold MID-GESTURE and
the gesture's tail destroying the state the resume just rebuilt. No code
changed; the tree every verifier finished on is the tree it started on.

*Run note.* This is a RERUN: the first launch of this pass mangled the
finding shape in dispatch and its output is VOID — discarded wholesale,
nothing from it folded in, relaunched fresh rather than repaired (the
round-6 lesson: a parameterized run is never resumed or patched in
place). *File-name note.* The orchestration slug for this pass measured
1260 bytes against the filesystem's 255-byte name limit; per the report
template and the in-family precedent, the series convention names this
file and the slug is not the subject.

---

## 1. Subject and coverage

**Commits.** `24bd4db99..65f2dc709`, everything verified at the tip
`65f2dc709`. Every verifier worktree again spawned STALE at `c3a00768c`
(~114 behind, the exact round-6 ref); every verifier executed the
first-action rule — `git log --oneline -1`, checkout of the audited tip —
before its first read, and every evidence block opens with it.

**Readers.** Three Gemini 3.1 Pro packets (identity verified per the
dispatch driver's MODEL-line check) plus one trace-agent residual.
Twelve raw findings; G-A4 was independently double-found by two packets
and merged, leaving eleven. Distribution of the attack: eight against
`programming/manage.c`'s fold/splice/sweep machinery (G-A1–G-A4,
G-B1–G-B4), three against `keyboardTweak.c`'s `openHOMEorMyM`
(G-P3-1, G-P3-3, T1-RS). Each finding got its own verifier in its own
worktree, lens per the brief (reachability / correctness / intent).

**Refutation-only.** This pass ran NO find phase: its coverage IS the
eleven findings. Surfaces the packets did not attack — everything
outside `manage.c`'s fold path and `openHOMEorMyM` — have no
out-of-family coverage this round and rest on the in-family pass alone.

**Executed context.** Verifiers were supplied the round's executed
facts and used them: `test_fold_round6_window` drives fold commits
through the real gate and its splice DOES fold committed steps
(subcases [2],[3] green at tip — any "the splice can never X" claim had
to name a path the fixture does not drive, and none did); FHIST is
capped at `FORTH_HISTORY_MAX_BYTES=1024` (`forth_dict.h:18`) with
oldest-first eviction (`manage.c:1807-1814`); `deleteStepsFromTo` is
byte-identical to upstream; and the in-family verdicts C-2 CONFIRMED /
P-1 PLAUSIBLE were standing, so re-derivations were verdicted as
DUPLICATE-CONFIRM with the overlap stated, not re-litigated.

**What the budget did not reach.** No dynamic execution in this pass:
zero mutations were run (none of the eleven was a coverage claim — all
were reachability, ordering, or bounds claims, settled by constructed
static trace plus the executed context above). The two confirmed
gestures were traced, not driven on simulator or hardware. P-1's two
unverified links (DELP's TM_LBLONLY keyability; the landing-program
length) are untouched. G-P3-1's verifier did not exhaust every
`fnExitAllMenus` consumer — only the one arm the finding named.

---

## 2. Mechanical results

This pass has no mechanical half of its own: same tip, no code changed,
and the in-family report's §2 stands unmodified (gate GREEN with no
generated-output drift, Forth self-test ALL PASSED including window
fixture [1]–[9], upstream `meson test testSuite` 1/1 OK, design-audit
4 finding groups, none counted as findings). Every verifier finished on
a clean tree (`git status` empty) at detached `65f2dc709`; zero
mutations applied, so nothing to revert.

---

## 3. CONFIRMED findings

Worst first, by what each costs the owner. Both survived the refutation
pass with the full path constructed at the tip. No patches — findings,
not fixes.

---

### OOF-1 (G-P3-1) — `openHOMEorMyM`'s `fnExitAllMenus(0)` arm wipes the whole softmenu stack two arms after its own wrapper resumed and re-registered the live console row

New CONFIRMED, and NOT covered by the planned C-2 fix shape: this arm
never consults `isAlphabeticSoftmenu`, so C-2's counted consumer census
of that predicate would close C-2 and leave this open. Third confirmed
destructive arm in the same function, second distinct mechanism.

- **Where.** `src/c47/c47Extensions/keyboardTweak.c:198` (the
  FLAG_ALPHA-clear branch's first statement, `leaveTamModeIfEnabled()`)
  against `:260` (the Normal-mode arm: FLAG_MYM_TRIPLE set,
  FLAG_HOME_TRIPLE clear, FLAG_BASE_MYM and FLAG_BASE_HOME both clear →
  `fnExitAllMenus(0)`). No package override of `keyboardTweak.c`
  exists; the `fnExitAllMenus` that runs is the package's
  (`softmenus.c:4266`), which pops every frame down to MyMenu.
- **Reaching input.** Keys-mode console, fold-pending TAM (any admitted
  function mid-entry — `forthFoldEnter` suspended the capture, and
  suspend cleared FLAG_ALPHA, `manage.c:1244`). MyM.3 set
  (FLAG_MYM_TRIPLE), HOME.3 clear, USER off, both base flags clear.
  Hold **f** through the f→g long-press ladder (package `screen.c:1023`
  → `openHOMEorMyM(keypress_long_f)` — this door runs NO wrapper before
  the call, unlike the fff door). The entry guard passes (console
  calcMode is CM_AIM, not EIM/MIM). FLAG_ALPHA is clear — suspend
  cleared it — so the `:197` else branch runs: `:198`
  `leaveTamModeIfEnabled()` is the D7-1 wrapper (`ui/tam.c:1466`:
  `_tamLeave()` then `forthFoldUnwindIfDone()`), the fold settles,
  `forthCaptureResume` sets FLAG_ALPHA (`manage.c:1402`) and
  re-registers the FWRD row through the owner (`manage.c:1426`, "stamp
  gone → acquire and register"). The capture is now LIVE. Then the
  Normal-mode ladder falls to the base-flags-clear arm and
  `fnExitAllMenus(0)` clears the whole stack — the just-re-registered
  FWRD frame included.
- **What the owner sees.** A live interactive capture with its
  registered row destroyed and the base menu in its place, while
  `forthCapKeysMode()` stays true and the keypad keeps typing the keys
  plane. The stamp census is false from that point: C18-class close
  accounting follows on the next EXIT, exactly as C-2's consequence
  chain.
- **Why it is wrong.** The F7 rule (package `screen.c:915-925`): *"the
  console OWNS its row while a live interactive capture is open."* The
  same comment's suspend carve-out (*"during TAM the capture is
  SUSPENDED, so the guard does not fire"*) does not apply here: the
  wrapper's own resume ended the suspension two statements earlier —
  the arm destroys a row the same gesture just rebuilt. And the
  destruction-by-`fnExitAllMenus` class is already ruled a defect once:
  DESIGN-HISTORY (R3, `:3110`) convicted the typed EXITALL door for
  popping every frame down to MyMenu over a console row, fixed via the
  post-run repair block's unconditional `forthConsoleRestoreSurface` —
  a repair block this keyboard door never passes through. The intent
  search was empty: no doc, comment, or ruling sanctions this arm over
  a live row.
- **Bug class.** F7's row-ownership class through a consumer census
  short by one — but of a SECOND teardown mechanism
  (`fnExitAllMenus`, not the widened alpha predicate), which is why the
  planned C-2 census would not have caught it; plus the R3 EXITALL
  class recurring through a door with no repair block downstream.
- **Class test.** Enumerate every keyboard-reachable softmenu-stack
  teardown (`fnExitAllMenus` consumers plus `openHOMEorMyM`'s pop
  arms), count asserted, each driven with a live registered console
  row, asserting the FWRD frame's slot-0 stamp survives. Concretely:
  fold-pending keys console, long-press f with MyM.3/Normal/base-clear;
  assert `currentMenu()` is still `-MNU_FORTH` and the stamp intact
  after the gesture.

---

### OOF-2 (T1-RS) — DUPLICATE-CONFIRM of C-2: the fold-pending arm, where the fff detector's own wrapper re-sets FLAG_ALPHA and re-registers the frame two statements before the pop that destroys it

Not a new finding — a new, verified ARM of C-2, reported per the
duplicate rule with the overlap stated precisely. One correction to the
residual as submitted: its "second door (fff vs long-press)" framing is
wrong — C-2's confirmed door already includes triple-f/HOME.3, so the
door is the SAME; what is new is the arm.

- **Overlap (identical to C-2).** Consequence: the registered FWRD
  frame popped inside `openHOMEorMyM`'s FLAG_ALPHA branch
  (`keyboardTweak.c:183-190`) because `isAlphabeticSoftmenu()` is TRUE
  for `-MNU_FORTH` (`isAlphaSubmenu` widened in Stage L, package
  `softmenus.c:3888`, predicate at `:4191`), then a raw ALPHA row over
  the live capture. Door: triple-f with HOME.3, `fg_processing_jm`'s
  detector. Symptom: row says ALPHA, keys type the keys plane,
  ownership stamp destroyed.
- **New arm (verified at tip, both links).** C-2's confirmed state has
  FLAG_ALPHA set throughout (a plain live keys console keeps it set,
  `test_capture.part.h:9218`). In the fold-pending state suspend has
  CLEARED it (`manage.c:1244`) — and the fff detector itself restores
  it mid-gesture: the third press calls `leaveTamModeIfEnabled()` at
  `keyboardTweak.c:285` BEFORE `openHOMEorMyM` at `:313`; the D7-1
  wrapper settles the fold (`ui/tam.c:1466`;
  `forthFoldUnwindIfDone`'s gate at `manage.c:2074` passes once
  `_tamLeave` zeroes `tam.mode`), `forthCaptureResume` sets FLAG_ALPHA
  (`manage.c:1402`) and re-registers the frame
  (`forthConsoleRestoreSurface`, `manage.c:1426`) — then
  `openHOMEorMyM` runs its alpha branch and pops the frame the same
  keypress just got back.
- **What it costs.** The C-2 fix's scope: a guard keyed on "the
  capture was live and the flag set when the gesture began" misses
  this arm, because at gesture start the capture was SUSPENDED and the
  flag CLEAR — everything the pop destroys was rebuilt inside the
  gesture. The fix must hold AFTER the wrapper's resume, not before
  it. This also discharges part of the round-7 owed `keyboardTweak.c`
  census: the `:285` wrapper site is not merely defensively covered —
  it is a live arm of a confirmed finding.
- **Violated contract.** Same as C-2 — the F7 rule (`screen.c:915-925`),
  quoted under OOF-1.
- **Bug class.** C-2's (predicate widened for one consumer, consumers
  never re-enumerated), plus the shape OOF-1 shares: resume-fresh
  state destroyed by the tail of the gesture that triggered the resume.
- **Class test.** The fold-pending sibling of C-2's class test: suspend
  a fold in the fixture, drive the triple, assert the FWRD frame and
  stamp survive — pins the resume-fresh FLAG_ALPHA case that C-2's own
  test (the plain-console sibling of subcase [6]) cannot reach.

---

## 4. PLAUSIBLE findings

None from this pass. G-B2's residue after refutation is the standing
P-1 (abandon-arm sweep, DELP door) with no new door or arm — it stays
where the in-family report put it, with the same two links to settle,
and is not re-opened here.

---

## 5. Design observations

**O-a — `openHOMEorMyM` is now three confirmed arms and two mechanisms
deep against a registered console row.** C-2 (alpha-branch pop, plain
live console), OOF-2 (same pop, fold-pending arm), OOF-1
(`fnExitAllMenus` wipe, MyM.3 arm) — plus one confirmed-dead branch
(G-P3-3's `tam.alpha` at `:186`). Two distinct teardown mechanisms in
one function, reached by one gesture family. The round's dominant class
(D7-a, enumeration without a count check) predicts the outcome of an
arm-by-arm fix wave here; whether the fix is per-arm with a counted
census or one ownership guard for the function is the owner's call, but
the evidence says any uncounted subset comes back as a round-8 finding.

**O-b — the pass minted a new shape: mid-gesture fold settlement
re-arms state the gesture's tail then destroys.** D7-1's wrapper is
doing exactly what it was built to do — both survivors depend on the
fold settling correctly. What fails is downstream: the caller's
remainder was written for the pre-resume world (flag clear, row
buried) and runs in the post-resume world (flag set, row live,
registered). The round-7 six-caller census asked "does the fold settle
on this path" and answered yes everywhere; the question it did not ask
is "what does the caller do AFTER the wrapper, now that the console is
live." That is the census to run when the OOF-1/C-2 wave lands.

**O-c — the fold machinery's comments and rulings held under
out-of-family attack.** Eight independent constructions against the
splice/sweep/fold path, zero survivors — and four of the eight
refutations turned on a comment or ruling that names the attacking
scenario in advance (`forthFoldEnter`'s `pemCursorIsZerothStep`
comment for G-A1; the `entryStepCount` struct comment that G-A2
almost certainly mis-attributed; the bounded-walk ruling for G-A3;
the L1-F2 rev-3 recovery comment for G-B2). This is the "decisions the
code already explains" class working as designed, and it is the
strongest external corroboration the round-6 F-wave core has had.

---

## 6. Deliberately not flagged / refuted

Mandatory. The finders returned no cleared-items census of their own
(the packets reported findings only), so this section is the
refutation pass's material: nine refutations, then the residuals the
pass saw and deliberately left.

### The fold/splice/sweep attack set — six findings, all refuted

**G-A1 — splice forward-scan misses TAM commits inserted BEFORE the
parked capture step (`manage.c:1366`).** Geometry unconstructible.
`addStepInProgram`'s pre-move (`manage.c:3122-3125`) advances the
cursor past the capture step before insertion, so TAM commits land
AFTER it; the one gate that could flip this (`pemCursorIsZerothStep`
true) is unconditionally foreclosed by `forthFoldEnter`
(`manage.c:2009`), whose comment names this exact scenario as the
reason for the assignment, and nothing in the fold window sets it true
again. The universal claim ("silently lost on EVERY fold commit") is
directly contradicted by fixture subcases [2]/[3], green at tip
through the real gate, and the finding named no path the fixture does
not drive. It even mis-predicts its own hypothetical: an insert before
the capture step would falsify the resume canary and be recovered by
the L1-F2 rev-3 machinery, not silently skipped.

**G-A2 — splice count `n = total − saved` overcounts by one
(`manage.c:1362`).** Premise false: at the seam (`ui/tam.c:1194-1196`)
`forthFoldEnter` materializes the capture step FIRST and
`forthCaptureSuspend` samples `getNumberOfSteps()` at its tail
(`manage.c:1247`), so `saved` includes the capture step and
`total − saved` counts exactly the TAM commits. The finder almost
certainly confused it with `forthFoldCtx.entryStepCount`, which IS
sampled pre-insert (`manage.c:2029-2036`, and says so) — but that
field feeds the sweep, not the splice. The consequence was
directionally impossible anyway: the splice loop starts at
`findNextStep(currentStep)` and walks strictly forward, so no
iteration can touch the capture step; and the canary it allegedly
breaks lives in resume, upstream of the splice.

**G-A3 — the 512-iteration cap in `_forthFoldFindCaptureStep` returns
a wrong step for FHIST >512 steps (`manage.c:1275`).** Premise
unconstructible: FHIST is capped at 1024 bytes (`forth_dict.h:18`),
each line costs at least 5 bytes (`forth_capture.h:90-93`) — ~204
lines maximum, plus one capture step and a fold window's interlopers,
several-fold under 512. The cap is the ruled bounded-walk class
(DESIGN.md:2755-2756; DESIGN-HISTORY:2836-2841: on a device with no
way to kill a spinning task, a corrupted walk degrades to a wrong
answer, never a hang); in the only regime where it could bite —
corruption — the canary/abandon machinery is the designed recovery.

**G-A4 — `deleteStepsFromTo` xcopy `+2` reads past allocation
(`manage.c:222`; double-found by two packets).** Layout invariant:
`firstFreeProgramByte` points AT the 2-byte `.END.` sentinel (diagram
`manage.c:59-64`; invariant set at `:214`; `freeProgramBytes` cannot
underflow because insertion grows memory first, `manage.c:724`), so
the source read ends exactly at the region boundary even at zero
slack — the `+2` exists to move the sentinel. Byte-identical upstream
and upstream-correct. The independent double-find is evidence of the
site's suspicious surface, but the layout beats it.

**G-B1 — foldLeave's fallback delete at `capStepOffset` destroys a
splice-KEPT step that shifted into the offset (`manage.c:2122`).**
Premise false on every branch: the splice never deletes the capture
step on any keep path (it deletes only steps strictly after it, and
`forthCapRecommitStep` is offset-neutral — `_insertInProgram` writes
at `currentStep` and its grow path rebases pointers,
`manage.c:1214-1219`, `724-763`), so the step at `capStepOffset` at
fallback time is the capture step itself, the intended target, and
every kept step sits at a higher offset — the delete stops at the
kept step's first byte, the F10 promise kept. Two backstops close
contrived variants: kept steps are native TAM steps that fail the
ITM_FORTH canary, and the rev-3 moved-step case fails the canary and
SKIPS the delete (worst case a bounded orphan in FHIST — a different,
lesser defect than the one alleged).

**G-B3 — forward-only sweep orphans every recorded step when an
abandon leaves the cursor at/after the debris (`manage.c:2103`).**
Abandon and recorded-steps-present are mutually exclusive: the only
resume early-return that skips the cursor restore while a fold pends
is the canary abandon at `manage.c:1303`, gated on
`_forthFoldFindCaptureStep()` returning NULL — and that scan matches
ANY recorded FHIST line, because every line is emitted by the same
builder (`_forthCapBuildStep`, `manage.c:878-891`) in the same
ITM_FORTH/STRING_LABEL_VARIABLE shape as the capture step. NULL
therefore implies zero recorded lines: nothing to orphan. Every
constructible commit path either holds the canary or recovers with
the debris ahead of the cursor (fixture [2]/[3] drive exactly this,
green at tip). Residually, a hypothetical orphan is cleaned by the
1024-byte oldest-first eviction — the round-6 G-W1a-1
design-accepted class, contradicting "permanently orphaned". The real
abandon door's hazard is P-1, already on the books.

### The abandon set — one finding, refuted as a duplicate

**G-B2 — abandon-without-re-anchor makes the debris sweep compare
FHIST's entry count against an arbitrary current program
(`manage.c:2100`).** Its named route — memory shift falsifies the
canary, abandon fires before the re-anchor — is exactly the case
L1-F2 rev 3 deliberately closed: with a fold/PARK pending, resume
recovers instead of abandoning (`manage.c:1247-1269` documents both
halves of that ruling), `_forthFoldFindCaptureStep` re-locates a
shifted-but-present capture step, and the F1 re-anchor runs before
any count arithmetic (`manage.c:1345-1357`). Abandon-before-re-anchor
requires the capture step genuinely gone from FHIST — which is the
DELP door, i.e. the standing P-1 PLAUSIBLE verbatim. No new door, no
new arm; positive refutation, not default-to-refuted.

### The keyboard set — one finding, refuted with a recorded residual

**G-P3-3 — `if(tam.alpha)` at `keyboardTweak.c:186` allegedly
always-false, pushing plain `-MNU_ALPHA` over the resumed console.**
Three-way disposition. (a) The premise "the branch test ran before the
wrapper" is inverted on the fff door: `fg_processing_jm` runs the
wrapper at `:285` before `openHOMEorMyM`, so in plain TAM alpha the
flag is already clear at `:180` and `:186` is unreachable. (b) On the
long-f door the true-branch IS dead — all seven `tam.alpha=true`
sites coincide with `tam.mode≠0` and both teardowns clear it — but
the dead branch is byte-identical upstream, predates the audited
range, and "fixing" it would be wrong anyway: by `:186` TAM is torn
down and its menu popped, so TAMALPHA is not the correct display
either. (c) The one range-live consequence — resume re-sets
FLAG_ALPHA, the pop eats the re-registered frame, a native menu lands
over the live console — is mechanism-for-mechanism C-2 as already
confirmed; which menu lands on top adds no door and no arm.
Upstream-latent dead branch recorded here; not a package finding.

### Residuals seen and deliberately left

- **`manage.c:2107`'s unguarded second `findNextStep`** (from G-B4's
  refutation): a hardening inconsistency against `forthHistoryEvict`,
  which guards both calls — but NULL there requires a malformed step
  the encoders never emit, and any such step crashes earlier, at fold
  entry's unguarded walk (`manage.c:1800-1805`) or at
  `getNumberOfSteps` in the same loop iteration (`:2100`). The code's
  own comment (`:2091-2094`) names the NULL risk and guards the first
  call only. G-B4's consequence half was accurately traced
  (`deleteStepsFromTo` is guardless and `to==NULL` would xcopy from
  address 0); its reaching input does not exist — the What-NOT-to-flag
  reachability rule, applied. One guard or comment line at the next
  edit; not a finding.
- **The `:186` dead branch** (G-P3-3(b)): upstream-latent, out of
  range, wrong to "fix" locally.
- **The rev-3 bounded-orphan disposition** (G-B1/G-B3 residue): an
  orphan capture step left in FHIST when the canary declines a
  fallback delete is the eviction-bounded class already
  design-accepted in round 6; still not worth a flag.

---

## 7. Verdict

**Scoped to what this pass measured: the out-of-family finding set.**
The net effect on round 7 is one new CONFIRMED (OOF-1) that the planned
C-2 fix shape would NOT have covered, one arm-widening of C-2 (OOF-2)
that constrains the fix's placement to hold after the wrapper's resume,
and zero new PLAUSIBLE. The ship answer is the in-family report's,
sharpened: not before the `openHOMEorMyM` wave — and that wave is now
three arms and two mechanisms wide, so a fix scoped to the
`isAlphabeticSoftmenu` census alone is known-insufficient before it is
written.

**Where this set breaks first.** Long-press f with MyM.3 in Normal
mode from a fold-pending keys console (OOF-1): the whole menu stack
wiped over a live capture, stamp destroyed, C18-class accounting to
follow. Behind two non-default settings and a mid-TAM gesture — rarer
state than C-2's plain-console door, which is why it ranks at C-2's
side rather than above the in-family C-1.

**What the pass is actually evidence FOR.** Eight independent
out-of-family constructions against the fold/splice/sweep core, zero
survivors, four of them killed by comments that anticipated the exact
attack. That machinery is the best-corroborated code in the range.

**What I would leave alone if the goal were correct code rather than
an audit-clean tree.** The `:2107` guard asymmetry (unreachable; a
line at the next edit). The `:186` dead upstream branch (not ours, and
the "fix" would be wrong). The bounded-orphan residue (ruled class).
OOF-2 as a separate fix — it is C-2's fix; only its fold-pending class
test must land with that wave, and a fix that passes C-2's plain
console test but not OOF-2's arm is the round-8 finding already
written.

---

## 8. Round and exit state

**Round 7, out-of-family leg** (the addendum the in-family §8
reserved). Subject `24bd4db99..65f2dc709` at `65f2dc709`, tree clean
before and after; RERUN of a voided first launch (finding shape
mangled in dispatch; discarded wholesale, relaunched fresh).

**Readers.** Three identity-verified Gemini 3.1 Pro packets plus one
trace-agent residual → twelve raw findings, eleven after merging
G-A4's cross-packet double-find; eleven verifiers, one per finding,
each in its own worktree, three-lens split per the brief. Two survived
(OOF-1 new CONFIRMED; OOF-2 DUPLICATE-CONFIRM of C-2), nine refuted.
Zero mutations — none of the eleven was a coverage claim.

**Process notes.** (1) The stale-worktree trap fired for all eleven
verifiers (`c3a00768c` again) and the first-action rule caught it all
eleven times; the rule needs no change. (2) The voided first launch is
the round-6 "never repair a parameterized run" lesson honored in the
expensive direction: a mangled finding shape poisons every verdict
downstream, so the relaunch re-passed the full finding set as args.
(3) The orchestration slug again exceeded the 255-byte name limit
(1260 bytes); series convention names the file, per the template.

**Exit criterion: unchanged — NOT met, earliest close round 9.** The
in-family pass already reset the count; this leg adds a new CONFIRMED,
so round 7 is doubly not a closing round. It does demonstrate the
out-of-family mechanics working end to end (identity checks, per-
finding verifiers, duplicate discipline), which one of the two closing
rounds must repeat. Round-8 gate items gain from this leg: OOF-1
folded into the C-2 wave with the teardown census counted across BOTH
mechanisms (O-a), the wave's guard placed to hold post-resume (OOF-2),
and the fold-pending class test landing red-first with it.
