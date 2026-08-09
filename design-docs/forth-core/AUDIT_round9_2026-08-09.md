# Audit — round 9: the fix waves and the consolidation wave at `657387a22`

Subject in full: the round-8 fixes-to-fixes (`58e07a2bd`, `bdbfffeb1`,
`c106008de`, `59147c93e`, `cff1a1863`, `8cca77caa`), the rounds-1-2
standing-open-list fix wave (`7f2714a3b`: C5 C6 C7 C10 C11 C13 C15 C20
C22), the round-8 residue wave (`88703343f`: the R8-P1 test, R8-1's class
test (b), the `forthCaptureResume` canary fix — the fourth consumer of
R8-1's class, the `tamEnterMode` pin, the FHIST walker caps), and the
ten-packet consolidation wave (`178f1df70..9325b67eb`: code relocation
claimed behaviour-neutral — `programming/forth_fold.c` out of `manage.c`,
`forth_console_view.c` out of `screen.c`, the interactive EXIT ladder out
of `fnKeyExit`, the `closeAim` teardown funnel, the small
predicate/abort consolidations). Range `58e07a2bd~1..657387a22`.

The regression record (r2 4/7, r3 4/4, r5 9/12, r7 2/7, r8 both) said
this round's findings would live in the same-day fixes, and that the
consolidation wave — relocation being the most dangerous fix shape
measured here — was not exempt. **The record held, and the exemption
question got its answer: both behavioural findings live in the fix
waves, zero live in the ten relocation packets.** The wave's cost
surfaced one level up instead: relocated mechanisms whose documented
truth did not move with them (§5).

*Filename note.* The operator-specified subject slug is 1,162 bytes,
over the filesystem's 255-byte name limit; this file carries the
directory's `AUDIT_round<k>_<date>` convention and the full subject
above.

---

## 1. Subject and coverage

**Commits.** Twenty-one, `58e07a2bd~1..657387a22`, all landed
2026-08-08/09, in four waves plus records: the six round-8 fix-to-fix
commits and their report (`ae5e63c00`); `7f2714a3b` (nine standing
findings from rounds 1-2, red-first); `88703343f` (round-8 residues plus
the fourth consumer its own class test found); `7c346ed81` (the
upstream-diff-review skill and its first run); `178f1df70..9325b67eb`
(CONSOLIDATE P1-P10 and the close-out); `657387a22` (the 2026-08-09
design audit, docs only). 62 files, +12,920/−3,590; the package's share
45 files, +9,180/−3,546.

**Files read at line level** (union across the eight dimensions):
`programming/forth_fold.c` complete (several readers), `forth_console.c`,
`forth_console_view.c`, `forth_capture.c/h`, `forth_compile.c`
(`fnForthOuter`, `forthTakeSourceFromX`, the enter-surface path),
`forth_bridge.c`, `forth_prims.c`, `forth_inner.c` (C13 watermark),
`items.c`, `keyboard.c`, `c47Extensions/keyboardTweak.c`, `screen.c`,
`softmenus.c`, `ui/tam.c`, `programming/manage.c` residue, the new
`bufferize.c` override around `closeAim`, and the full cumulative test
diff (`test_console.part.h` +364, `test_capture.part.h` +564,
`test_dict_reloc.c` +15 — the tests dimension read every new assertion).
Upstream contracts consulted at source: `lblGtoXeq.c`
(`goToGlobalStep`/`goToPgmStep`), `nextStep.c`, `scanLabelsAndPrograms`'s
RAM_FULL path, `charString.c`, `realType.h` (`real34ToInt32` =
`decQuadToInt32`), `showStringEdC47`'s `yincr` block, `freeList.c`.

**Relocation verification.** The P7/P8/P9 extractions were mechanically
diffed against their pre-images (comment-stripped, the sanctioned
`forthPkg*`/`forthCapBuildStep`/`forthCatalog*` renames normalised) by
three dimensions independently: byte-identical except the declared seams;
every drift found traced to a named in-range fix, none silent. The
upstream dimension reproduced the close-out's churn numbers from the real
tree rather than trusting the report.

**Readers.** Eight in-family dimension finders (contracts, lifecycle,
arithmetic, errorpaths, guards, tests, design, upstream) via
`audit-workflow.js`, blind to each other. Every finding below survived
the independent refutation pass in an isolated worktree, lens named per
finding. Process note: the stale-worktree trap re-fired on every
verifier spawn (worktrees materialised at `c3a00768c`, the exact ref
round 6 warned about); the round-8 rule — first action is `git log
--oneline -1` and a checkout of the audited tip — caught it every time,
and every evidence record opens with that checkout. All probes and
mutations run during verification were reverted; the tree finished clean
at `657387a22`. The out-of-family leg of this round was still in
refutation when this synthesis closed; anything surviving it lands as
`AUDIT_round9_out-of-family_2026-08-09.md` (round 7's precedent).
Nothing below depends on it.

**Deliberately not audited.** The PEM sibling of the resume-canary door
(no structural bound stated for a PEM capture step's program — recorded
as the round-9 design question in `88703343f`'s comment in
`forth_capture.c`; not re-reported, though R9-5 names it as the on-track
third spelling, a different claim). P1 and P2, documented rulings. F13's
extraction, which landed as CONSOLIDATE P9 and was audited as such.
Rounds 6-8's refuted items.

**What the budget did not reach** (union of the dimensions' own
declarations): the ~900-1,100 lines of new test code beyond spot checks
and the pins named in §2 — the R8-P1 test body, the `[1]`
history-line-length parameterisation and the K4/N1-5 batteries were
skimmed, not audited, so fixture-guard defects of the R8-6..R8-9 kind
may remain there; the 2,801-line `bufferize.c` override outside the
`closeAim` hunk (the churn scanner's job, which ran clean); the 11
patches unchanged in the range; `forth_menu.c` and `forth_dict.c`
internals; the pre-range rev-3 double-TAM fold-window machinery;
DESIGN.md end-to-end (read via its load-bearing citations); the P1 mass
re-indentation byte-by-byte (taken on the churn scanner plus the green
gate). A report that does not say what it missed cannot be trusted about
what it found; this is what this one missed.

---

## 2. Mechanical results

**Gate GREEN**, re-run for this report at `657387a22`: refresh clean,
build clean, Forth self-test ALL PASSED (including the range's new
fixtures), upstream `meson test testSuite` 1/1 OK, exit 0. Generated
output synchronized with the manifest and clean in Git; tree clean
before and after.

**`design-audit.sh`: exit 0 — the mechanical half CLEAN**, the first
clean run in the series. Group A inside the close-out's re-set budgets
(override files 19/19, added lines 1,243/1,243, removed 340); B at
baseline (8, rebaselined by `657387a22`'s triage from the 29 the wave
inherited); C none; D at baseline (36 — the wave moved it 38 → 36, the
right direction); E the standing allocation prompts; F/G/H clean. Group
I: **all eleven pins ok** — including `capture navigations bracketing
dynamicMenuItem: 4`, which is green *and blind* (R9-7); that pin's
blindness is counted as a finding, not a mechanical result.

**Churn**: `patch_churn_scan.py` exit 0, zero findings; the upstream
dimension independently reproduced the standing count (1,243 added / 340
modified-deleted upstream lines / 139 hunks / 19 patches) from the tree.

**RULE-1 across the range**: flash 1,115,184 → 1,115,832 = **+648 B**
net (the ten-packet wave's own share +240 B, per-packet deltas in the
stage commits); RAM 8,884 → 9,144 = **+260 B**, 256 of it C5's browse
stash (`FORTH_CONSOLE_LINE_MAX + 1`, the only RAM growth the stage has
taken) and 4 the wave; arena untouched — no commit in the range touches
the dictionary.

Nothing the mechanical half reports is counted as a finding below.

---

## 3. CONFIRMED findings, worst first

Ten, after merging one pair: the contracts and guards dimensions found
the same C6 defect independently (one empirically, one through the
intent record); it is reported once as R9-3. Ranking is by what the
finding costs the owner, per the standing rule.

### R9-1 — R8-1's cursor restore clamps the program index but not the local step; DELP of the cursor's own program from the console leaves `currentStep` NULL and the next PEM insert writes wild

`packages/forth-core/programming/forth_fold.c:1276` —
crash-or-data-loss, medium confidence, arithmetic dimension,
correctness lens.

**Reaching input.** One user program PUSR (~15 steps) plus FHIST behind
it. In PEM, scroll the cursor down deep into PUSR (local step 12 — the
scroll-down arrival pins `firstDisplayedLocalStepNumber = cursor − 6`,
which matters below). Leave PEM, open the console, DELP (the
`tamEnterMode` seam parks the fold), spell `PUSR`, ENTER. `fnClP`'s
label arm → `_clearProgram` → `_forthFoldNoteProgramDeleted(1)`:
deleted == savedProgram, so per the R8-1 ruling the index is left alone
and now names the successor — but `savedLocalStep` is untouched by both
the deleter fix and the restore clamp (`forth_fold.c:1258-1283` clamps
only the program). The restore runs `goToPgmStep(1, 12)` against the
shrunken list: `defineCurrentProgramFromGlobalStepNumber` lands on the
trailing `.END.` pseudo-program, then `lblGtoXeq.c`'s `while(true)` walk
(`:122-133`, no NULL break) passes the `.END.` (`findKey2ndParam`
returns NULL at op `0x7fff`) and loops `findNextStep(NULL)` → NULL until
step 12, assigning `currentStep = NULL`. The verifier's one correction
to the finder: `fnPem`'s render self-heals the mid-window case
(`manage.c:561` reassigns `currentStep` when the cursor's row is drawn,
and the `:523` clamp forces that when the saved window is below the new
bogus local step) — but in the scroll-down-arrival case the render draws
one line, breaks at the `.END.`, and never reaches the cursor row;
`currentStep` stays NULL through console EXIT and PEM re-entry whenever
FHIST holds ≤ 6 steps. The first digit press then reaches
`_insertInProgram`'s shift loop (`manage.c:748`,
`for(pos = firstFreeProgramByte+1+size; pos > currentStep; --pos)`) with
`currentStep == NULL`: every byte from `firstFreeProgramByte` down
toward address 0 is overwritten. SIGSEGV on the simulator, wild
write/hard fault on the device; all unsaved state — the program being
edited and the console dialogue — destroyed. Milder sub-case: an
overshoot that stays inside surviving programs silently parks the cursor
mid-way into a program the owner never touched.

**Violated.** The R8-1 comment at the restore itself: *"The clamp below
stays as the crash guard … this used to be the only thing between a
stale index and `programList[numberOfPrograms]` on a freshly reallocated
arena, walked by `goToGlobalStep` with no NULL guard and no iteration
cap — reproduced as a SIGSEGV."* The clamp guards only
`forthFoldCtx.savedProgram`; `savedLocalStep` reaches the same unguarded
walk. It also breaks the ruling it cites — *"a deletion AT it leaves the
index alone … so the cursor lands on what is now the next program, and
the fold follows rather than inventing a different answer"* — upstream's
own do-nothing arm leaves the cursor at the successor's STEP 1;
restoring a stale `savedLocalStep` into the successor is an answer
upstream never produces.

**Class.** Saved cursor tuple with an unmaintained half — a quantity
remembered across a dispatch that mutates its base (R8-1's class, one
field over). The round-8 record already prescribes the fix shape
(`AUDIT_round8_in-family_2026-08-08.md:797-804`): a restore that
validates before it consumes, `savedLocalStep` bounded by the restored
program's length.

**Class test.** Enumerate every consumer of a saved
(program, localStep) restore; for each, drive every list mutation
between save and restore (delete-at, delete-before, evict) and assert
the restored cursor lands inside the restored program's bounds and
`currentStep != NULL` — the R8-1 class test (b) extended by the
localStep axis. The red-first reproducer is the sequence above.

### R9-2 — same class, second site: `_forthHistRestoreCursor` restores an unclamped local step; a mass-evicting push with the PEM cursor parked inside FHIST reaches the same NULL walk

`packages/forth-core/programming/forth_fold.c:557` —
crash-or-data-loss, medium confidence, arithmetic dimension, intent
lens. Traced, not executed — deserves a red-first reproducer before any
fix.

**Reaching input.** Fill FHIST to its 1,024-byte cap with ~200 tiny
lines. In PEM, navigate down into FHIST near its END (FHIST is an
ordinary visible program; no rule bars the cursor from parking in it).
Leave PEM, open the console, type a near-cap line (~250 chars, a
259-byte step), ENTER. `forthHistoryPush`: `_forthHistSaveCursor`
captures (FHIST, ~200); the push inserts; `forthHistoryEvict` deletes
~52 oldest steps; `_forthHistRestoreCursor` runs
`goToPgmStep(FHIST, ~200)` against a program that now has ~150 local
steps. FHIST is the last program, so the overshoot walks past its END
and the `.END.` exactly as R9-1 — `currentStep = NULL`. The console
keeps working (interactive typing never touches `currentStep`); the
corruption fires on the next PEM step insert.

**Violated.** The C2 cursor-tuple comment above the struct
(`forth_fold.c:512-516`): *"(program, localStep) — NOT a saved global
step number, which program-boundary shifts (FHIST growing/evicting)
would make stale by restore time"* — the tuple's localStep half is
itself stale in exactly the FHIST-evicting case the comment claims the
tuple form was chosen to survive, whenever the saved cursor is inside
FHIST. The intent record confirms this is an open exposure, not a
ruling: the round-8 record names this exact sibling (*"the sibling
`_forthHistCur` … caches the same triple and has the same exposure"*)
and puts it in R8-1's class-test enumeration; the R8-1 deleter ruling
was implemented only for the fold context and program deletion —
nothing adjusts `_forthHistCur`, and `forthHistoryEvict`'s
`deleteStepsFromTo` path is a deleter the ruling's fix never touched.

**Class and class test.** Same as R9-1 — this is the enumeration's
second member, and the class test above covers it by construction
(evict is one of the mutations driven).

### R9-3 — C6's "do nothing" second FORTH press still consumes a dtString in X before the no-op return

`packages/forth-core/forth_compile.c:1746-1770` — wrong-result, high
confidence; found independently by the contracts dimension (empirical,
reachability lens) and the guards dimension (intent lens). Empirically
proven: with the console live and a string in X, a second
`fnForthOuter` left the line and surface intact (the C6 contract held)
while X's string was silently replaced by the old Y content — and every
existing C6 assertion passed while it happened.

**Reaching input.** Console live (FORTH pressed once), any dtString in
X — e.g. store a string, open the console, type `RCL 05` ENTER (F4-1
parameterised dispatch executes it on the live stack, leaving dtString
in X with the console interactive-live). Press FORTH again via the FCNS
catalog or an assigned key: `runFunction`'s divert refuses ITM_FORTH
for name-insert (`forthCapNameInsertEligible`), so `reallyRunFunction`
reaches `fnForthOuter`. The dtString block at `:1743-1747` runs FIRST:
`forthTakeSourceFromX` copies and then `fnDrop`s X (`:1615-1628`) —
and only then does the C-6 interactive-live guard at `:1770` return,
discarding the seed. Sibling on the same ordering: an oversize
(≥ FORTH_SOURCE_MAX) string in X makes the same press raise
ERROR_INVALID_DATA_TYPE instead of being a no-op.

**Violated.** The fix's own contract in the C6 comment
(`forth_compile.c:1761-1769`): *"The gesture is already 'you are in the
console'; the honest answer is to do nothing"* — and the same comment
names consumption as part of the harm being fixed: *"If X held a string
the re-open additionally seeded from it and consumed it."*
DESIGN-HISTORY's 2026-08-09 close records the class rule *"a gesture
that is neither commit nor abandon must not be able to empty a line"* —
X being emptied instead of the line is the same class, half-unfixed.
Round 2 predicted this exact failure mode in writing: *"a reader fixing
only the line-discard half will leave this behind."*

**Class.** Guard placed below the state-consuming read it guards — a
same-day-fix regression of the round record's canonical shape. The C6
test (`test_console.part.h:1616-1633`) never puts a string in X before
the second press, so the path is unpinned; the verifier's probe passed
the whole existing battery with the consumption live, proving the
coverage gap.

**Class test.** Extend the C6 leg of
`test_console_line_survives_gestures`: before the second FORTH press,
place (a) a dtString and (b) an oversize dtString in X; after the
press, assert the line AND X both intact and no error raised. The class
axis: every "this gesture is a no-op" contract in the console gets a
does-not-consume-X assertion, not only a does-not-empty-the-line one.

### R9-4 — the OOF-B narrowed guard lets the else-arm push a raw `-MNU_ALPHA` row over the live keys-mode console after dismissing an overlay

`packages/forth-core/c47Extensions/keyboardTweak.c:220` (twin shape at
`screen.c:946`) — wrong-result, medium confidence, guards dimension,
reachability lens. Empirically observed at the tip: the shipped OOF-B
test `[7]` itself drives it and stays green.

**Reaching input.** Exactly test `[7]`'s drive: console open keys-first,
push `-MNU_MyAlpha` over it (this function's own MyM.3 arm, ruled
benign), then the HOME.3 long-press. The guard at `:196` —
`forthCapInteractiveLive() && forthConsoleBaseOnTop()` — is
Live=true/BaseOnTop=false (stamp buried), so the else arm runs:
`popSoftmenu()` removes MyAlpha, then the unconditional tail
`showSoftmenu(-MNU_ALPHA)` at `:220` pushes a fresh unstamped ALPHA
frame over the stamped console base. Verified end state: `currentMenu()
== -MNU_ALPHA` while `forthCapKeysMode() == 1` — the row reads ALPHA
while `determineItem` routes the keypad through the keys plane
(`keyboard.c:1824` keys the plane on the mode bit, not the displayed
row). The mismatch persists across ENTERs (`forthConsoleShowSurface` is
C18-refused while a foreign row covers the base) until an extra EXIT.
The `[7]` test passes because it asserts only
`currentMenu() != -MNU_MyAlpha` plus stamp survival — `-MNU_ALPHA`
satisfies both.

**Violated.** The guard's own justifying comment two lines above
(`:202-203`): *"a raw ALPHA push here would leave the row reading ALPHA
while the keypad types the keys plane"* — the narrowed conjunct
re-admits precisely that push for the buried-base case it was added to
handle. K-R3 ("the row IS the mode indicator") is the named invariant.
Caveat, stated both by finder and verifier: if HOME.3's native
semantics are read as "show the ALPHA row" rather than "dismiss the
overlay", the end state is arguably another benign overlay — but then
the OOF-B framing and this comment contradict each other. An owner
ruling is owed either way (§8); the finding is actionable either way,
because either the guard or the comment is wrong.

**Class.** Guard narrowed past its own justification — the conjunct
added to fix the stuck-overlay case re-opened the case the original
guard existed for.

**Class test.** For every overlay-dismiss gesture with the console live
in keys mode: assert afterwards that either the console base is on top
or the top frame is stamped — i.e. assert the K-R3 positive property,
not the absence of one named menu. Applies to both twin sites.

### R9-5 — the "capture step lies inside FHIST" structural rule is spelled twice, over two separate stored copies of the same offset, with no shared predicate

`packages/forth-core/programming/forth_fold.c:161` (inlined copy,
`:161-172`) vs `:1085-1088` (`_forthFoldResolveCaptureStep`) —
design-flaw, high confidence, design dimension, reachability lens. Both
spellings are live code on test-executed paths; grep confirms exactly
these two spellings of the `programList[hist-1]` bounds computation
exist with no shared helper, and the underlying quantity is stored twice
(`forthCap.savedStepOffset`, rewritten by the resume's recovery at
`:181`; `forthFoldCtx.capStepOffset`, assigned once at `:1021` and
never rewritten — the resolver tolerates its staleness instead).

**Why it is wrong.** No wrong behaviour today; the claim is about the
next edit, and the rule's consumer history inside this very range is
the evidence: P-1 guarded `forthFoldLeave`'s count, OOF-A added the
resolver because the fold context's copy was never rewritten,
`88703343f` found the FOURTH consumer (the resume canary) still on the
raw shape — and the fix for that fourth consumer INLINED a second copy
of the bounds test instead of sharing the resolver's. The recorded PEM
sibling question, when closed, is on track to be a third spelling. The
regression record of this codebase says that divergence becomes the
next round's crash or step-eating sweep; this class alone produced four
confirmed defects across rounds 8-9.

**Violated.** `forthFoldLeave`'s own comment states the principle the
resume site does not get: *"Through the same resolver, so the second
look cannot answer a different question than the first"*
(`forth_fold.c:1211-1215`). The commit that added the second spelling
names the class it belongs to: *"identity resolved by remembered
address plus a shape test, where the design states it structurally"*
(`88703343f`) — the structural statement now exists in two
hand-synchronized copies.

**Class.** Structural rule spelled per-site (the r5 R12 family: a rule
corrected in a subset of its copies, one edit away).

**Class test.** A group-I pin that greps the FHIST-bounds computation
and fails when it appears in more than one function — count the
spellings, expect one — plus, when the PEM sibling is closed, the
closure goes through the same predicate or the pin moves.

### R9-6 — `FORTH_CONSOLE_ED_YINCR` copies an upstream function-local the build cannot see, under a comment claiming `_Static_assert` enforcement that structurally cannot cover it

`packages/forth-core/forth_console.h:102` — latent, high confidence,
design dimension, correctness lens. **Mutation-proven silent**: the
verifier changed the compiled `yincr` from 35 to 30 while leaving the
macro at 35, and the full gate — build, self-test, upstream testSuite —
stayed GREEN. That is exactly the band/editor divergence the comment
promises cannot happen, and `design-audit.sh` carries no
`yincr`/`showStringEd` pin to catch it either.

**Reaching input.** Latent against an upstream rebase: if upstream's
`showStringEdC47` changes its local `yincr = 35`
(`src/c47/screen.c:1660`), nothing in this tree moves — the asserts in
`forth_console_view.c:38-46` reference only the macro itself plus
`Y_POSITION_OF_*` names. The transcript band and the input line then
silently overlap or gap, with the build green and every assert passing.
(The finder's secondary "today, in the HP35-style layout" consequence
was refuted: `checkHP` requires CM_NORMAL/CM_NIM while the console
render gate requires CM_AIM — mutually exclusive, so no band ever
paints in that layout; in every reachable console state the computed 67
is correct.)

**Violated.** `forth_console.h:100-101`: *"The `_Static_asserts` below
turn any drift into a build failure rather than a mispainted band"* —
false for the one constant the sentence annotates; an assert cannot
reference a function local. The C14 close's own claim (`8cca77caa`):
*"the numbers are now COMPUTED from upstream's names by upstream's own
arithmetic, never hand-fitted"* — `yincr` is not an upstream name; it
is a hand-copied value.

**Class.** Constant copied by value across a module boundary (C14's own
class — this is C14's close leaving one member of its class open under
an overstated cover claim).

**Class test.** A source-anchored pin: grep upstream's
`showStringEdC47` for `yincr = 35` so a rebase drift moves a counted
site — the same mechanism group I already uses for enumerated sites,
pointed at the one value the asserts cannot reach.

### R9-7 — the R8-2 bracket pin counts the fix, not the sites: an unbracketed package navigation is invisible to it, and one already exists

`design-docs/forth-core/design-audit.sh:506` — design-flaw, medium
confidence, design dimension, intent lens. **Mutation-proven blind**:
an unbracketed `goToPgmStep(1, 1)` appended to `forth_fold.c` left the
pin's count at 4, green.

**Reaching input.** The design gap needs no runtime input: pin 4 greps
`dynamicMenuItem = -1;` — the bracket idiom — and expects 4, so a fifth
`goToPgmStep`/`goToGlobalStep` call added to package code without a
bracket leaves the pin green, exactly the blindness R8-3/R8-4 were
about. The existing unbracketed site is `forthFoldEnter`'s
`goToGlobalStep(1)` (`forth_fold.c:979`), a package keypress navigation
on the same fold-entry path as the bracketed
`forthHistoryGotoLastStep` eighteen lines below it. Misbehaving at
runtime needs `currentProgramNumber < 1` simultaneously with a latched
`dynamicMenuItem ≥ 0`; neither finder nor verifier could construct that
conjunction (every latching gesture traced also navigates first) — the
runtime half is UNREACHED and the finding is filed as the design flaw
it is. But `:979` carries no exemption comment, so a reader cannot tell
whether it was judged safe or missed, and `c106008de`'s *"the package's
three keypress navigations now all bracket it"* is a hand census this
fourth site falsifies as a census. `goToGlobalStep` ignores its step
argument entirely under a latch, so `step=1` confers no intrinsic
safety.

**Violated.** The pin block's neighbouring rules, verbatim: *"R8-4
(round 8, against this pin's first version): counting FILES meant a NEW
consumer … never moved it … Count the call sites"*
(`design-audit.sh:485-489`); the C-1 pin's two-sided pattern (count the
writes AND the re-derivations) six entries up. R8-2's own class-test
rule: *"For every navigation the package performs during a key press …
assert `dynamicMenuItem < 0` at the call or bracket it"* — `:979` got
neither assert, bracket, nor exemption.

**Class.** Enumeration blind to its subject (D7-a recurring inside its
own countermeasure — the second consecutive round, after R8-3/4/5).

**Class test.** The pin counts `goToPgmStep`/`goToGlobalStep` call
sites in package sources and requires each bracketed or
comment-exempted; mutation-run the pin at authoring time (append an
unbracketed navigation, pin must go red) — the same discipline the
fixtures already carry.

### R9-8 — P6 relocated the interactive-close guard but not its citations: DESIGN.md still names the deleted `_forthCapCloseIfInteractive` as the live choke point

`design-docs/forth-core/DESIGN.md:2652` — latent, high confidence,
design dimension, reachability lens. Verified by grep at the tip: the
symbol survives ONLY in docs and test comments; the live guard is
inside `closeAim` (`packages/forth-core/bufferize.c:2705-2707`), and
`keyboard.c`'s five arms call bare `closeAim()`.

**Reaching input.** Read the authoritative doc: §8.4.2's close-path
dispositions say the five native `closeAim` arms *"close the capture
via one choke point (`_forthCapCloseIfInteractive`) and then run native
`closeAim()`"* — that function was deleted by CONSOLIDATE P6. Five test
comments still narrate the per-call-site model
(`test_capture.part.h:7557, :15897, :15900, :16104`;
`test_console.part.h:2738`) — including a REPORT printf that prints the
dead pair's description on every green run. A maintainer adding a sixth
close path per DESIGN.md's text goes looking for a call-site guard to
replicate and finds nothing — or re-adds a site-local guard, forking
the funnel P6 built. P6's verify step grepped only `keyboard.c` for the
symbol; the same-range design audit checked file paths and line
numbers, not symbol liveness.

**Violated.** DESIGN.md's own status — *"DESIGN.md there is
authoritative"* (CLAUDE.md) — and the bug-class catalog: *"Comment that
outlived its mechanism (r5 R13): the cited mechanism was deleted…; the
comment is now false evidence for a true claim"*, plus *"Enumeration
without a count check"* (the symbol's occurrences enumerated by hand,
one file).

**Class.** Comment/doc that outlived its mechanism.

**Class test.** Extend the citation checker: DESIGN.md-cited SYMBOLS
get a liveness grep against the package sources, not only paths and
line numbers — a deleted symbol cited as live is a failure.

### R9-9 — `forthConsoleExitLadder`'s rung-3 justification narrates machinery its own rung-1 comment declares deleted, and calls the now-conditional pop "unconditional"

`packages/forth-core/forth_console.c:351` — design-flaw, high
confidence, design dimension, correctness lens.

**Reaching input.** Read the one function. The rung-3 comment
(`:346-362`) argues from *"rung 2's pre-normalisation renames slot 0 to
id 1 IN PLACE; it does not pop … two renames, zero pops"* and concludes
*"The unconditional `popSoftmenu()` is what actually removes that
frame"*; the pop annotation at `:395-396` re-asserts *"the frame rung
2's pre-normalisation renamed but did not pop."* Sixty lines above, the
overlay rung's own comment records the pre-normalisation as REMOVED
(*"Neither half survives FWRD-as-home"*, `:284-300`), and the pop below
is conditional (`if(popHome)`, `:393`) with a comment explaining why
unconditional was wrong (`:388-392`). No rename code exists anywhere in
the console path; `forth_compile.c:1693` says the rename *"is GONE
deliberately"*. P9 moved the block verbatim per spec; P10's narrative
sweep exempted `files/` sources by rule, so nothing reconciled it. The
code executes correctly — the conditional pop is the audited fix — but
a maintainer editing rung 3, the rung whose own comment records that
"rev 2 of this packet got it wrong" once already, is handed premises
the same function refutes. The close rung is the highest-regression
code in the audit record (C3/C5.6b/C17/M1-1 `[8]` all lived here);
false load-bearing narration is the documented precursor of that class.

**Violated.** Bug-class catalog: *"Rule corrected in a subset of its
copies (r5 R12): the fix added the corrected clause and left the stale
one standing six lines away"* — here sixty. The audit method's own
premise — comments are load-bearing evidence a reviewer must trust — is
what a false present-tense narration corrodes.

**Class.** Stale load-bearing narration after relocation (r5 R12/R13
family).

**Class test.** None mechanically enumerable beyond R9-8's symbol
sweep; the enforceable half is process — a relocation packet's verify
step includes a narrative pass over the MOVED text, with `files/`
sources in scope (P10's exemption rule is what let this through).

### R9-10 — the fnPem cursor hunk carries 3 avoidable modified upstream lines (a purely additive shape exists) and an inert `tmpString[6]` save/zero/restore

`packages/forth-core/programming/manage.c:607` (patch
`010-programming__manage.c.patch` @@ -588) — latent (merge tax, behaviour
correct), medium confidence, upstream dimension, correctness lens.
**Gate-proven**: the verifier applied the additive shape (upstream's
four lines byte-identical, the Forth override appended as an `if` after
them, the inert triplet dropped), regenerated patches — the hunk became
purely additive, the churn scanner dropped to zero, and the full gate
ran GREEN; then reverted.

**Reaching input.** Unreached at runtime. The hunk modifies three
upstream lines (`tmpChar` → `tmpChar4`, the `cursorInString`
declaration hoisted off upstream's ternary, the restore rename) in
fnPem's hot decode loop; upstream touching that block conflicts on all
three. A purely additive shape exists because the ternary's result is a
harmless dead computation when `tam.function == ITM_FORTH` (strcmp is
pure, `T_cursorPos` a plain extern), and the added
`tmpChar6`/`tmpString[6]=0`/restore triplet is inert — both compared
literals are 4 bytes (`"REM "`; `"42"` + 2-byte glyphs,
`src/c47/fonts.h:347,396`), and `tmpString[4]=0` terminates every
strcmp before byte 6 is read. The reshape also retires both surviving
`tmpChar` NEAR hits — half the scanner's standing NEAR count.

**Violated.** `upstream-diff-review` SKILL.md rule 3 (*"Modified
upstream lines outrank added lines"*) and method step 3 class M (*"ask
whether a purely additive shape exists"*). Not a re-litigation: the
2026-08-09b review judged the rename at the NEAR tier only (*"real
rename — not a finding"* — correct for that tier); the class-M question
was never asked, and the hunk appears nowhere in
`deliberate-exceptions.md`. It predates the review range (`git log -S`:
the PEM-mode initial commit).

**Class.** Modified upstream line where a purely additive shape exists
(class M), plus dead code documenting nothing.

**Class test.** The churn-gate wiring the close-out already recommends:
the scanner beside the group-I pins, holding dels at the post-reshape
count and NEAR at 0 so any regression is a diff of one.

**Which of these I would leave alone if the goal were correct code
rather than a passing audit.** R9-10 entirely — the behaviour is right;
the reshape buys rebase economics, and it can wait for the next time
the file is open. R9-7's runtime half — the `:979` conjunction looks
genuinely unconstructible, and the pin reshape plus the missing
exemption comment are audit-infrastructure hygiene, cheap but not
correctness. R9-8 and R9-9 are document repairs, not code — they stay
flagged only because false load-bearing narration is this project's
measured precursor of next-round confirmed defects, and the close rung
has the record to prove it. R9-1 through R9-6 stand under either goal.

---

## 4. PLAUSIBLE findings

One — survived refutation on trace and provenance, but nobody could
construct the reaching input.

### R9-P1 — `forthFoldLeave` touches `labelList`/`programList` after the sweep's own UAF break

`packages/forth-core/programming/forth_fold.c:1216` — latent,
errorpaths dimension, correctness lens. If the sweep's
`deleteStepsFromTo` → `scanLabelsAndPrograms` exits with
`lastErrorCode = ERROR_RAM_FULL` (lists freed up front, early return
without reallocating), the sweep breaks at `:1203` — *"L1-H's UAF
guard"* — and the very next statement,
`cap = _forthFoldResolveCaptureStep()`, calls `forthHistoryProgram()`,
which walks `labelList[i]` for a nonzero `numberOfLabels` (counted
before the failed alloc) and reads `programList[hist-1]`: NULL/garbage
dereference inside the very error path whose guard exists to prevent
exactly this. Provenance is confirmed: `58e07a2bd` replaced a
list-safe raw-offset second look with the resolver, introducing the
list reads on the errored path. The violated contract is the L1-H
guard, quoted verbatim in `forthHistoryEvict`
(`forth_fold.c:797-807`): *"Abandon the loop rather than touch either
list again."*

**Why PLAUSIBLE, not CONFIRMED.** The verifier granted the error state
and confirmed every link — but could not construct the state: in the
delete context `scanLabelsAndPrograms` frees equal-or-larger lists
immediately before reallocating, and `freeListAlloc`
(exact-match-else-best-fit, merge-on-free) provably re-serves the
allocation in every case analysis constructed. The arm may be
structurally unreachable from this call path.

**What would settle it.** (a) The P-2-pattern fault-injection hook on
`allocC47Blocks`, driven under the sweep — executes the arm red-first
and settles it as CONFIRMED; or (b) an owner ruling that the L1-H
convention governs regardless of allocator reachability — the
convention is violated as written either way, and the sibling sites
(`forthHistoryEvict`, the sweep itself) all obey it by abandoning; or
(c) a proof pinned at the allocator that free-then-smaller-alloc cannot
fail, which justifies treating the break as dead and says so at the
resolver call.

---

## 5. Design observations (D7)

**D7-1 — the wave held, and the reason is checkable, not lucky.** Ten
relocation packets, eight dimensions, zero behavioural findings in the
moved code. The most dangerous fix shape in the record came back clean
because it was verified mechanically — moves diffed against pre-images
modulo declared seams, by three dimensions independently, with every
drift traced to a named fix — and because each packet proved its claim
by mutation before landing. Relocation is dangerous when it is trusted;
this wave was not trusted.

**D7-2 — the wave's real cost: relocated mechanisms, unrelocated
truth.** Four of the ten confirmed findings are one family — the
mechanism moved and its documented truth did not: R9-5 (a structural
rule now in two hand-synchronized spellings over two stored copies of
one offset), R9-6 (a copied constant under an enforcement claim its
asserts cannot honour), R9-8 (the authoritative doc citing a deleted
symbol as the live choke point), R9-9 (load-bearing comments narrating
deleted machinery in the highest-regression function of the record).
The regression record says exactly this family is next round's
confirmed-defect nursery. A relocation packet's definition of done
should include its narration and its citations, `files/` sources not
exempted.

**D7-3 — the remembered-identity ledger, closed form.** Six sites now
remember identity instead of deriving it: four consumers of the capture
step's offset (P-1's count, OOF-A's resolver, the resume canary, the
fold context's own copy) and two saved cursor tuples (fold context,
`_forthHistCur`). Rounds 8-9 harvested five confirmed defects from the
family, and round 8's record already prescribes the shape (one
validate-before-consume restore, one structural predicate). The class
wants one owner and one spelling, not a fifth per-site guard.

**D7-4 — pins are code, second consecutive round.** R8-3/4/5 were
group-I pins mutation-proven blind; R9-7 is another, written AFTER the
round that taught the lesson, violating the rule recorded ten lines
above it in the same file. The pin layer needs the fixtures'
discipline: a new pin is mutation-run at authoring time or it is
assumed blind.

**D7-5 — standing recommendation, still unwired.** The close-out
recommended wiring the churn scanner beside the group-I pins — a
one-line addition while the count is 0. It is still not wired; R9-10's
class test is the same wire.

**D7-6 — record accuracy.** The close-out's *"the largest inline block
left is keyboard.c's console-roll arm"* undercounts: the ITM_FORTH arm
in `insertStepInProgram` is 61 lines (`manage.c:1673-1733`). Its
placement is RULED (see §6), so no code moves — but the sentence should
be corrected in the standing record, because a census that is wrong in
a report becomes a premise in the next spec.

---

## 6. Deliberately not flagged

Merged from the finders' cleared lists and the refutation pass's kills.
Grouped; every entry carries why it cleared.

**Killed by the refutation pass** (finder asserted, verifier
disproved):

- **`forth_bridge.c:378` byte-boundary tail clamp (C11-shape).** Dead
  on all current paths: the internal buffer is bounded to 255 payload
  bytes and every one of the six callers passes a
  `FORTH_CONSOLE_FMT_MAX` (256) buffer, so the clamp can never cut,
  mid-glyph or otherwise. A hardening suggestion about a hypothetical
  future caller — the exact "reachable someday" shape the reachability
  lens rejects.
- **The interactive strand-residue population at the resume canary
  (`forth_fold.c:161`).** SUSPENDED + interactive + foldMode 0 is the
  state round 8's P-2 ruling designed out of existence, recorded at the
  refusal site (`ui/tam.c:1172-1200`) and in DESIGN-HISTORY with the
  class rule "stop building the state, not guard its consequences."
  Verified structurally: every production foldMode-clearing site
  settles the suspension in the same act, and the F2/F4 strand class
  leaves foldMode NONZERO, so a recurrence resumes through the
  FHIST-bounded arm. The population is empty by recorded design.
- **C11's empty-string subcase "silent no-op"
  (`test_console.part.h:1435`).** The class oracle is one documented
  sentence — no lead byte in the ring without its trailing byte — and
  an empty string satisfies it; the `> 0` guard is behaviourally inert
  (the skipped walk would pass vacuously). Empty output is a different
  class, ruled at the formatter itself ("name the type rather than
  printing nothing"). The test does its stated job.
- **The `insertStepInProgram` ITM_FORTH arm as a missed extraction
  candidate (`manage.c:1673`).** Ruled, same day, in the wave's own
  spec: *"Stays in manage.c (do NOT move)"* names the hook arms and the
  catalog helpers — written by the author who simultaneously minted the
  P8 seams, so the coupling was known at decision time; and
  STAGE_L_T7 rejects splitting `insertStepInProgram` *"outright, not
  deferred."* A candidate examined and ruled to stay is closed, not
  open. Only the census wording survives, as D7-6.
- **`forthCaptureSanitizeRestoredUi` inline in the manage.c override
  (`manage.c:835`).** Same spec, verbatim: *"forthCaptureSanitizeRestoredUi
  (sits above pemAlpha, calls `_closeAlphaMenus` directly — leave
  it)"* — issued in the very packet that minted
  `forthPkgCloseAlphaMenus`, and carried in-tree in the P8 header
  comment. At most a deliberate-exceptions catalog-entry gap.

**The wave's neutrality checks that cleared** (verified, not trusted):
P2's `forthCapNameInsertEligible` reproduces both forked pre-image
exclusion lists exactly, and `item > 0` is strictly additive (it closes
an `indexOfItems[negative]` hazard). P3's abort helper and P4's SST/BST
placeholder abort are byte-identical to both replaced inline copies,
including the `tam.function = 0` ordering. P6's funnel is
order-identical to the five deleted per-site guards (guard, then
teardown); widening it to every `closeAim` caller is the stated
invariant and strictly shrinks the leaked-FCAP_OPEN class; the FCNS
catalog-pick site keeps its `!forthCapIsInteractive()` conjunct; an
interactive-SUSPENDED capture cannot reach a `closeAim` site (the fold
forges CM_PEM, suspension leaves CM_NORMAL, all callers CM_AIM-gated).
P7/P8 are line-identical modulo the declared seams; P9's ladder is
verbatim against the pre-extraction `fnKeyExit` block, break→return-true
is caller-equivalent at the single call site, and the ladder makes no
display call, so N-T5's grep property holds. C-3's reshape clamps a
paint-local copy while the write clamp stays at `forthConsoleRollView`
— no other consumer assumes the stored offset pre-clamped; the one dead
roll keypress after a long-to-short transition is the documented
upstream-convention cost, owner-ruled. C14's derivation reproduces the
old 128/67 exactly in the default layout.

**Sibling and adjacent hazards attacked, and they held:** EMIT's
dtReal34 arm has no C20 sibling — `real34ToInt32` is `decQuadToInt32`,
which returns 0 out of range, and 0 fails both printable gates: refusal,
not a wrong glyph. bdbfffeb1's deliberately attackable assumption
("while a fold is pending its capture step is still in FHIST") was
attacked and holds: the only deleter of a first-content step while
FHIST survives is `forthHistoryEvict`, whose sole caller cannot run
inside a fold window. The resolver's `cap < firstFreeProgramByte`
conjunct is unfalsifiable given `cap < to` — redundant, harmless, and
this codebase's documented defense-in-depth habit. The C5 browse stash
survives close/reopen/power-reset uncleaned, but every restore path
first overwrites it (reopen resets `historyIndex` to BROWSE_NONE) — no
stale-restore door. The 512 walker caps cannot truncate a legal FHIST
(1,024-byte cap / ≥5 bytes per step ≈ 205 steps) and fail toward
stopping eviction, the safe direction; the post-increment guard
discriminates a legitimate boundary walk from a tripped cap.
`forthCopyWholeGlyphs` cap 0/1 edges and exact-fit verified in-bounds;
malformed input is copied bounded, garbage-in-garbage-out. The render
ellipsis `xcopy` overflow needs a single-byte standardFont glyph wider
than 14 px after a 385-px prefix in 254 bytes — none exists; thin
margin, holds, upstream's own idiom. R8-1's deleter-convention equal
case (DELP of the fold's own program parks the cursor on the successor)
is deliberate, matches upstream's own arm, and is stated at the site —
R9-1 is about the localStep half only. `fnClPAll` cannot have a fold
pending (not a TAM item), and resolve-NULL plus the clamp cover it. The
EXIT ladder's rung 2 cannot fire mid-TAM (TAM consumes EXIT before
`fnKeyExit`'s dispatch). A FORTH press during a SUSPENDED capture would
orphan-drop it, but no reaching input exists (TAM planes never resolve
ITM_FORTH) and the banner names the state exotic. The C7 test's
space-tokenizer can overcount only toward failing, the safe direction.
One corner recorded as uncertain rather than cleared: the
`pushSoftmenu` dedup-lift of a buried user ALPHA row through R9-4's raw
push — the guards finder could not conclusively separate it from the
round-5 benign-overlay ruling; it rides on R9-4's ruling (§8).

**Documented rulings honoured, and round-8 subjects closed:** the PEM
sibling of the resume-canary door — the recorded round-9 design
question, excluded by the tasking (R9-5's duplication claim is
distinct). `fnExitAllMenus`'s Live-only guard — ruled in the OOF-B
commit: that call wipes the whole stack, so base-on-top is the wrong
test there. The duplicate `/* 022 */` softmenu comment — P1 minimality,
§8.6's numbering rule. `FORTH_SELFTEST_EXPORT`'s
static-declared-never-defined shape — the compiler already reports it
(rule 5). `manage.c:2148`'s `0x7f → 0xff` — an in-place upstream bug
fix with no additive shape, where a loud conflict on upstream's own fix
is wanted. The determineItem console-roll arm cannot move — running at
that exact point in plane selection is its whole content; rationale
checked and agreed. Sol's two round-8 dependencies are discharged: the
P-2 refusal sits above every TAM state write, all 8 `tamEnterMode` call
sites are audited against it (now a group-I pin), and error-dismissal's
interaction with EXIT is pinned by test `[5]`. R8-P1 got its test in
`88703343f`. The tests dimension's own verdict stands as a cleared
class: R8-6..R8-9's repairs verified live (the C13 watermark survives
to its read, subcase `[9]` reds without the brackets, the C22 dead
canaries are demoted honestly to a documented net behind real
`_Static_asserts`).

---

## 7. Verdict

**Would I ship this? No — two doors first.** R9-1 and R9-2 are one
class with a constructible crash-plus-data-loss consequence, and the
class's fix shape is already prescribed in the round-8 record
(validate-before-consume, localStep bounded by the restored program's
length; both land under the standing rule — red-first reproducer, named
class, class-level test, which for this class is one enumeration
covering both sites). R9-3 is five minutes of understanding and closes
a silent stack-eater on a documented no-op gesture. R9-4 needs its
ruling before either the guard or the comment moves. Everything else is
next-edit insurance.

**Where it breaks first.** `FORTH` → `DELP` → spell the program the PEM
cursor is parked deep in → `ENTER` → `EXIT` → PEM → any digit: wild
write from `firstFreeProgramByte` downward — reboot on the device, the
edited program and the console dialogue gone. Second: any second FORTH
press with a string in X silently eats the top of the stack.

**The pattern, ninth round running, sharpened.** The findings live in
the same-day fixes again — and this round ran the controlled
experiment: the feared shape (ten packets of relocation) produced zero
behavioural findings because every move was mechanically verified,
while the fix waves around it produced both crashes-in-waiting and the
silent X-drop. The danger was never relocation; it is unverified
change, and fixes are the least-verified change this project makes.
What relocation does leave behind is documented-truth debt (D7-2), and
the record says that debt is where round 10's confirmed findings will
come from if it is not paid.

---

## 8. Round and exit state

**Round 9.** Readers: the eight in-family dimensions via
`audit-workflow.js`, blind to each other; findings through the
independent refutation pass in isolated worktrees (every verifier
checked out the audited tip first — the stale-worktree spawn trap fired
every time and was caught every time). The out-of-family leg was still
in refutation at synthesis time; its survivors, if any, land in
`AUDIT_round9_out-of-family_2026-08-09.md` and count against the exit
criterion like any others.

**Exit criterion (two consecutive rounds with no new CONFIRMED
findings): NOT met, and reset.** Round 9 produced ten confirmed
findings including a crash class in a round-8 fix. Earliest close is
now **round 11**, and round 8's condition stands: the closing round
must include an out-of-family pass on the actual fix commits.

**Round 10's subject, in order:**

1. **The fixes for R9-1..R9-4** — fixes to fixes, the highest prior
   this project has; the cursor-tuple fix relocates state decisions,
   the most dangerous shape measured, so its packet gets the wave's
   discipline: mechanical pre/post diff and mutation-run tests.
2. **R9-P1's settle** — the fault-injection drive or the ruling (§4).
3. **The doc/duplication debt** — R9-5 through R9-9 — verified by the
   pins and sweeps named in their class tests, then re-audited as
   changed code, not trusted as "just docs."
4. **The new test code this round did not read** (§1's budget gap): the
   R8-P1 body, the `[1]` parameterisation, the K4/N1-5 batteries — the
   fixture-defect record (R8-6..R8-9) says they are not exempt.

**Owner rulings owed:** (1) HOME.3 with the console live and buried —
"dismiss the overlay" or "show the ALPHA row"? R9-4's guard or its
comment moves accordingly, and the pushSoftmenu dedup corner rides on
it. (2) R9-P1 — does the L1-H list-touch convention govern regardless
of allocator reachability? Carried unchanged: P1 (round 3), P2's push
ruling, C22-vs-C1. **F13/U5 closed** — the ladder extraction landed as
CONSOLIDATE P9 and was audited this round.

**Standing items:** the DM42n hardware pass for stages L/M/N, the merge
to main (130+ commits now), the FIX-6B MR push, the leak-report filing,
the churn-scanner wiring (D7-5), and the record correction (D7-6). The
pre-round-6 standing open list (C5 C6 C7 C10 C11 C13 C15 C20 C22) is
**cleared by `7f2714a3b`** — with the note that C6's closure is
reopened in half by R9-3.
