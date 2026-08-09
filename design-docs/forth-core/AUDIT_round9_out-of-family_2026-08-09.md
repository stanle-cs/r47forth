# Audit — round 9 out-of-family refutation pass at `657387a22` (addendum to AUDIT_round9_2026-08-09.md)

Range `58e07a2bd~1..657387a22`, same subject as the in-family report;
this file is the out-of-family addendum that report's §1 and §8
promised. Eight findings from four out-of-family sources — three Gemini
packets against the resume-canary fix, the P9 EXIT-ladder extraction
and the P6 `closeAim` funnel, plus one Sol-derived observation — went
through the same three-lens refutation pass as the in-family set:
**zero survived, eight refuted, every refutation positive** (a
constructed geometric contradiction, an executed pin asserting the
opposite, or a recorded ruling — no tie-breaks). The attacked surfaces
are the range's three most feared: a fix to a fix, and the two largest
relocation packets. They absorbed all eight constructions. One of the
eight is a byte-for-byte re-report of a standing open finding
(R13/R9-9) and is credited as corroboration, not re-confirmed. No code
changed; the tree every verifier finished on is the tree it started on.

*Filename note.* The orchestration slug for this pass measured 854
bytes against the filesystem's 255-byte name limit; per the report
template, the round-7 precedent, and the in-family §8's explicit
reservation of this name, the series convention names this file and
the slug is not the subject.

---

## 1. Subject and coverage

**Commits.** `58e07a2bd~1..657387a22`, everything verified at the tip
`657387a22`. Every verifier worktree again spawned STALE at
`c3a00768c` (the exact round-6 ref — third consecutive pass); every
verifier executed the first-action rule — `git log --oneline -1`,
checkout of the audited tip, detached — before its first read, and
every evidence record opens with it. 8/8 fired, 8/8 caught.

**Readers.** Three Gemini packets plus one Sol-derived observation.
Packet 1 attacked the resume-canary fix (`88703343f`'s canary and the
`58e07a2bd` resolver): three findings, G1-1..G1-3. Packet 2 attacked
the P9 EXIT-ladder extraction (`51b84023a`): two findings,
G2-1/G2-2. Packet 3 attacked the P6 `closeAim` funnel (`a5d943880`):
two findings, G3-1/G3-2. Sol's observation (S-1) attacked the
recovery path's cursor state. Eight raw findings, no duplicates among
themselves (one duplicate against the standing record, G2-2). Each
finding got its own verifier in its own worktree, lens per the brief
(reachability / correctness / intent).

**Refutation-only.** This pass ran NO find phase: its coverage IS the
eight findings. Surfaces the sources did not attack — everything
outside `forth_fold.c`'s resume/splice/resolver path, the extracted
ladder, and the funnel — have no out-of-family coverage this round and
rest on the in-family pass alone.

**Supplied verifier context, and it was used.** Four pre-verified
facts rode in every verifier brief: *(a)* the four `keyboard.c`
`closeAim` sites (`:1118`, `:3058`, `:4847`, `:5065`) carried the
per-site `_forthCapCloseIfInteractive()` helper BEFORE the P6 funnel
commit `a5d943880` — the L1-1 disposition table's pre-funnel state,
checked before crediting any wrong-disposition claim; *(b)* round 5's
R13 concerned rung comments citing the deleted pre-normalisation
mechanism, recorded disposition checked before crediting a re-report;
*(c)* P9 claims verbatim-move behaviour neutrality; *(d)* upstream
`defineCurrentProgramFromCurrentStep` (`src/c47/programming/
manage.c:410`) sets only `currentProgramNumber`/begin/end, never
`currentLocalStepNumber`. Fact (a) settled G3-2's disposition
question, (b) identified G2-2 as a re-report, (c) was verified rather
than trusted where it mattered (G2-1's two `ShowSurface` calls traced
to the pre-extraction `fnKeyExit` block), and (d) closed S-1's last
consumer route.

**What the budget did not reach.** Zero mutations were run — none of
the eight was a coverage claim; all were reachability, ordering,
disposition, or intent claims, settled by constructed static trace
plus rulings plus one executed proof (G1-2's verifier drove the
subcase-B pin through the full gate). Nothing was driven on simulator
or hardware. G1-1's refutation rests on a door census — the one named
FHIST-relative grower (the GTO→GTOP promotion) plus the fold-window
insert census; a program-memory door outside that census would reopen
the question. S-1's dead-window claim rests on a consumer census of a
single dispatch (unwind → leave), traced not driven. The G3
refutations rest on the disposition table's ruling being the owner's
intent — the table itself carries R9-8's dead-symbol citation (§5).

---

## 2. Mechanical results

This pass has no mechanical half of its own: same tip, no code
changed, and the in-family report's §2 stands unmodified (gate GREEN,
`design-audit.sh` mechanical half CLEAN — first in the series, churn
scanner zero). One verifier independently re-ran the full gate at
`657387a22` as part of G1-2's executed proof: refresh clean, BUILD +
SELF-TEST GREEN including the leave-then-dispatch pin (`[B] REPORT:
aimBuffer="42 STOVEL 5 " fhist 2->2 capstate=1`), upstream `meson
test testSuite` GREEN; the build-regenerated
`constantsVerification.txt` was reverted. Every verifier finished on
a clean tree (`git status` empty) at detached `657387a22`; zero
mutations applied, nothing to revert.

---

## 3. CONFIRMED findings

**None. Zero of eight survived.** This is the measured result of
refuting these eight claims, not a statement about the range: the
pass hunted nothing on its own, so zero here means the out-of-family
set added no findings. The in-family ten (R9-1..R9-10) stand exactly
as written — in particular, S-1's refutation is about the transient
recovery-window pair and does NOT weaken R9-1/R9-2, which convict the
saved cursor tuple's `localStep` half at restore time (§6). One
re-derivation of a standing open finding (G2-2 = R13/R9-9) is
credited as corroboration under the duplicate rule, not re-confirmed:
it adds no door, no arm, no consequence — only weight (§5).

---

## 4. PLAUSIBLE findings

None. Every refutation was positive — a geometry the finder's input
cannot produce, an executed test pinning the opposite of the claimed
consequence, or a recorded ruling on the exact question — so nothing
sits in the survived-but-unconstructed band.

---

## 5. Design observations

**O-a — the resume/fold core is now twelve independent constructions
deep with zero survivors.** Round 7 threw eight out-of-family
constructions at the fold/splice/sweep machinery and lost all eight;
this pass threw four more at its round-8/9 successors — the canary,
the resolver, the splice, the recovery cursor — and lost all four.
The kills keyed on the same shape round 7 measured: comments and
rulings that name the attack in advance (`forthFoldEnter`'s
`pemCursorIsZerothStep` comment for G1-2, the R8-10 resolver ruling
for G1-3, the armed-fold resume gate for S-1, the
stated-so-it-can-be-attacked resolver assumption for G1-1 — attacked,
held). The most dangerous provenance in the record — a fix to a fix —
came back clean under external attack because its decisions are
written down where the attacker must read them.

**O-b — the wave's documentation did the refuting, and its
documented-truth debt did the misleading.** All four ladder/funnel
findings died against documents: §8.4.2's close-path disposition
table (G3-1, G3-2), the P9 placement note plus N1-1's grep definition
(G2-1), the R13 record (G2-2). But three of the eight cycles were
provoked, at least in part, by exactly the debt the in-family pass
convicted: G2-2 IS R9-9's stale narration, relocated; G3-1/G3-2's
readers worked against a §8.4.2 that is right on the ruling and wrong
on the mechanism — it still cites the deleted
`_forthCapCloseIfInteractive` as the live choke point (R9-8), the
precise mix that invites a wrong-disposition read; G1-2's finder
built its geometry from the recovery banner's loose phrase, not a
trace. Independent external readers stumbling where the in-family
design dimension already filed findings is measured corroboration for
D7-2, and it prices the debt: each unrepaired narration bills every
future external reader one full construct-and-refute cycle. The debt
is not yet producing wrong code; it is producing wrong findings.

**O-c — the "no display call" property survived because it is a grep,
not a phrase.** G2-1 is what happens when a mechanically defined
property ("symbol table free of `lcd_`, `showString`,
`refreshScreen`; no `screen.h`/`display.h`") is read informally:
`forthConsoleShowSurface` retargets softmenu-stack state and paints
nothing, and the N1-1 acceptance grep still passes at the tip. The P9
placement note's parenthetical anticipated exactly this reading and
still could not prevent the cycle. The lesson runs toward mechanism,
not prose: properties that exist as a checked grep survive attack;
their English restatements attract it.

---

## 6. Deliberately not flagged / refuted

Mandatory. The sources returned findings only (no cleared-items
census of their own), so this section is the refutation pass's
material: eight refutations grouped by surface, then the residuals
the pass saw and deliberately left. Every entry carries why it
cleared.

### The resume-canary / fold set — packet 1 plus Sol, four findings, all refuted

**G1-1 — resume canary and fold resolver accept ANY `ITM_FORTH` step
inside FHIST, not the LAST one (`forth_fold.c:171`, resolver
`:1080-1095`).** The static premise is true — both bounds check
FHIST-span membership plus opcode shape, not last-ness — and the
reaching input cannot be constructed. The only named door that grows
program memory during a suspension, the GTO→GTOP promotion
(`ui/tam.c:783-806`), inserts exclusively at `firstFreeProgramByte`,
above every program including FHIST: `insertStepInProgram`'s shift
region is empty, zero bytes below the insertion move, so FHIST never
shifts and the saved offset can never come to rest on an older
history line. Every fold-window insert lands at the cursor
`forthFoldEnter` parked ON the capture step, where the stale address
holds a native, non-`ITM_FORTH` interloper — the canary falsifies and
the designed L1-F2 recovery runs. Round 6's F1 rederive at the
promotion was checked and is irrelevant (the growth still cannot move
FHIST). Reachability lens; no coverage claim, no mutation needed.

**G1-2 — resume splice scans only AFTER the capture step; an
interloper inserted BEFORE it is never folded (`forth_fold.c:247`).**
The scan direction is real; the insert-before geometry is foreclosed
by design, at a site whose comment names this exact hazard:
`forthFoldEnter` forces `pemCursorIsZerothStep = false`
(`:985-995`), so `addStepInProgram`'s pre-move
(`src/c47/programming/manage.c:1831-1838`) steps the cursor forward
off the capture step and every fold-window TAM commit lands AFTER it.
The saved offset stays valid (an insert above an address never moves
it; `_insertInProgram`'s grow path rebases, `manage.c:705-715`), the
canary passes, `n=1`, and the interloper folds. Decisive: the exact
gesture the finding names — STO arms the fold, dddVEL supersedes,
digits, ENTER — is pinned by an executed test asserting the opposite
of both claimed consequences (`test_capture.part.h:14006-14065`,
subcase B), and the verifier drove it through the full gate at the
tip: green, `fhist 2->2`, the committed step folded to canonical
text, no debris. The finder inferred its geometry from the
`_forthFoldFindCaptureStep` banner's loose phrase, not a trace — the
phrase is a residual below.

**G1-3 — recovery rewrites the capture's saved offset but not
`forthFoldCtx.capStepOffset`; two owners of one truth diverge
(`forth_fold.c:181`).** Re-report of a closed, documented decision.
R8-10 (`AUDIT_round8_in-family_2026-08-08.md:653-712`, "CONFIRMED at
the audited tip, CLOSED at HEAD") is verbatim this finding, and the
close deliberately chose the resolver over a resync:
`capStepOffset` was converted from a remembered truth to a validated
fast-path hint, every `forthFoldLeave` consumer resolves through
`_forthFoldResolveCaptureStep` (FHIST bounds + shape test, scan
fallback, hist==0 do-nothing), and the round-8 record rules the
conversion "the right shape … proven twice over." The claimed
latent-alignment residual is also ruled: the length-parameterised
class test landed 2026-08-09, and the one remaining assumption is
stated in-code "so it can be attacked," with the future guard named
(`FHIST step count > forthFoldCtx.entryStepCount`, `bdbfffeb1`).
What survives of the complaint — one structural rule in two
spellings over two stored copies — is already on the books as
in-family R9-5; nothing new to file. Intent lens.

**S-1 — recovered resume keeps the saved `currentLocalStepNumber`
while `currentStep` moved (`forth_fold.c:226`).** The inconsistent
pair momentarily exists and is dead state. Its only door is an armed
fold, and armed folds are excluded from the Seam-2 resume
(`ui/tam.c:1484` gates on `!forthFoldArmed()`); the recovery-path
resume therefore runs only inside `forthFoldUnwindIfDone`
(`forth_fold.c:1050-1054`), where `forthFoldLeave()` follows in the
same dispatch — no keypress, no repaint in between. Every step-acting
operation in that window is keyed on the `currentStep` pointer or on
`currentProgramNumber`, never on the local step number: the splice
uses `findNextStep(currentStep)`/`deleteStepsFromTo`,
`_insertInProgram` inserts AT the pointer and increments the local
number as bookkeeping only, `getNumberOfSteps` is keyed on the
program — and supplied fact (d) closes the last route
(`defineCurrentProgramFromCurrentStep` never writes
`currentLocalStepNumber`). `forthFoldLeave` then rewrites the whole
triple via `goToPgmStep(savedProgram, savedLocalStep)` — the USER's
pre-fold cursor, index-maintained by `_forthFoldNoteProgramDeleted`,
freshly derived through `programList` — a consistent pair by
construction. The surface is CM_AIM throughout, so no PEM paint
consumes the stale number either. **Scope note, so this refutation is
not over-read:** S-1 claimed the transient recovery-window pair;
in-family R9-1/R9-2 convict the SAVED tuple's `localStep` half at
restore time — a different pair, a different window, and both stand.

### The P6 `closeAim` funnel — packet 3, two findings, both refuted by the disposition table

**G3-1 — the funnel closes a live interactive capture but the native
tail still commits `aimBuffer` to X (`bufferize.c:2705`).** Fails on
mechanism and on classification. Mechanism: `forthCapClose()`
(`forth_capture.c:27-41`) writes state/keysMode/origin and unstamps —
it pushes NOTHING to history, so the claimed double commit (history
push + X commit) does not exist; only the EXIT ladder's rung 3 pushes
to FHIST, and the ladder never calls `closeAim` (the funnel's own
comment, `bufferize.c:2702-2704`). Classification: the X commit at
exactly these arms is the ruled L1-2 KEEP disposition, DESIGN.md
§8.4.2 verbatim — "the line is preserved *in X*, not in history;
native behaviour stays native outside the ladder" — swept by
`test_interactive_close_sweep`. C5.4 constrains the ladder, not the
native arms. Ruled behaviour re-reported as a bug.

**G3-2 — wrong close disposition at the four funnel-covered
`keyboard.c` sites (`:1118`; also `:3058`, `:4847`, `:5065`).** The
described behaviour — close, commit the line to X, run the native
function — is exactly what §8.4.2 rules these arms MUST do (the L1-2
KEEP disposition). The L1-3/C5 insert-name-and-leave-open convention
the finding cites governs the FCNS catalog-pick arm only: that site's
`!forthCapIsInteractive()` conjunct keeps it out of `closeAim`
entirely, and its comment plus DESIGN-HISTORY name L1-3 as
superseding L1-2 "at the FCNS arm" — nowhere else. Supplied fact (a)
closes the provenance question: all four sites carried the identical
disposition pre-funnel via the per-site
`_forthCapCloseIfInteractive()` helper, and the P6 record proves the
relocation behaviour-neutral by mutation (both close batteries redden
with the funnel body removed). Even the "X clobbered" framing inverts
the intent — the string commit IS the KEEP disposition's
line-preservation mechanism. The packet read a deliberate, tested,
owner-ruled disposition as a defect because it could not see the
pre-funnel state; the verifier context existed for exactly this.

### The P9 EXIT-ladder extraction — packet 2, two findings

**G2-1 — banner claim "the ladder makes no display call" vs
`forthConsoleShowSurface` on rungs 1 and 2 (`forth_console.c:321`,
`:336`).** "Display call" is N-T5's term of art, defined three times
over as a mechanical check: no `screen.h`/`display.h` include, symbol
table free of `lcd_`, `showString`, `refreshScreen` — the PACKET_N1_1
acceptance grep, re-run at the tip, still passes (`ShowSurface` is
not `showString`; the only hits are the banner's own wording).
`forthConsoleShowSurface` (`forth_menu.c:540`) swaps softmenuStack
slots and sets `doRefreshSoftMenu`; the paint happens in the
separately traced `fnKeyUp` → `refreshScreen(131)` path, and the
only module that draws is the N1-2 render arm (`forth_console.h:
21-22`). Decisive on intent: `51b84023a`'s placement note weighs the
banner against the ladder and rules it explicitly — "the ladder makes
no display call (softmenu frames, calc mode, the capture) so the
property holds" — with both `ShowSurface` calls verbatim-moved and in
front of the author when the classification was written. A documented
deliberate classification, not an oversight.

**G2-2 — rung-3 teardown comment justifies both calls by rung-2
pre-normalisation, a mechanism that no longer exists
(`forth_console.c:351`, `:395`).** True, and already on the books
twice. This is round 5's R13 byte for byte — CONFIRMED there
(`AUDIT_round5_2026-08-06.md:1425`, promoted by two-family
agreement, fix ruled "prose only"), open ever since (no fix commit,
no ledger closure; `git log -S"pre-normalisation"` shows only the
introducing commits and the P9 move), relocated verbatim by
`51b84023a` from `keyboard.c:4158-4164`/`:4202` to its new address —
and the same site is this round's in-family R9-9. Per supplied fact
(b) and the duplicate rule, a re-discovery of a recorded open finding
is corroboration, not a new finding: no new door, arm, or
consequence; the annotated code executes correctly (the conditional
pop is the audited fix). Now found independently by a third reader
family, the standing prose repair gains priority (§5 O-b) and
nothing else.

### Residuals seen and deliberately left

- **The `_forthFoldFindCaptureStep` recovery banner's loose phrase**
  ("that insert shifts the capture step off
  `forthCapSavedStepOffset()`") — imprecise about WHERE fold-window
  inserts can land, and it fathered G1-2. Not false (it states the
  recovery's general charter), so not a finding; one wording pass
  when the file is next open, naturally bundled with the R9-9 prose
  repair.
- **The N-T5 banner's informal "display call" wording** — already
  carries the classifying parenthetical, and the property is
  mechanically pinned by the N1-1 grep, so there is no drift risk.
  Leave alone.
- **R13's owed prose fix at its new address** — carried on the
  round-10 docket inside R9-9; corroborated here, not re-opened.
- **The resolver's deliberately-unbuilt FHIST-membership guard**
  (`FHIST step count > forthFoldCtx.entryStepCount`) — the stated
  assumption was attacked by G1-1 and held; the guard stays unbuilt
  per its own recorded ruling until a door is found.

---

## 7. Verdict

**Scoped to what this pass measured: the out-of-family finding set.**
Net effect on round 9 is zero new CONFIRMED, zero PLAUSIBLE, and one
third-family corroboration of a standing open prose finding
(R13/R9-9). The in-family verdict — not shippable before the
R9-1..R9-4 doors close — stands unmodified; nothing here softens it
(S-1's refutation does not touch R9-1/R9-2) and nothing sharpens it.
Where the range breaks first is unchanged: in-family R9-1's DELP wild
write.

**What the pass is actually evidence FOR.** The range's three most
feared surfaces — the round-8 canary fix (a fix to a fix, the
record's most dangerous provenance) and the two largest relocation
packets — absorbed eight independent external constructions and lost
none, most of them to rulings and comments written before the attack.
With round 7's eight-for-eight, the resume/fold machinery is twelve
constructions deep with zero survivors: the best-corroborated code in
the project.

**What the pass costs the owner.** Three of the eight cycles were
provoked by documented-truth debt the in-family pass already
convicted (R9-8's dead-symbol citation, R9-9's relocated stale
narration, one loose recovery banner phrase). The debt produced no
wrong code this round; it produced wrong findings, at one full
construct-and-refute cycle per misled reader, and it will bill every
future external reader the same way until the round-10 prose wave
pays it.

**What I would leave alone if the goal were correct code rather than
an audit-clean tree.** Everything in this file. All eight refutations
stand on geometry, executed pins, or recorded rulings — none on
charity. The two banner-wording residuals are cosmetic. The only real
work this leg surfaces — the R13/R9-9 and R9-8 repairs — was already
scheduled by the in-family report; this leg only re-prices it.

---

## 8. Round and exit state

**Round 9, out-of-family leg** (the addendum the in-family §1 and §8
reserved, under the filename §8 named). Subject
`58e07a2bd~1..657387a22` at `657387a22`, tree clean before and after.

**Readers.** Three Gemini packets (packet 1: resume-canary fix;
packet 2: P9 ladder; packet 3: P6 funnel) plus one Sol-derived
observation → eight raw findings, no duplicates among themselves, one
duplicate against the standing record (G2-2 = R13/R9-9). Eight
verifiers, one per finding, each in its own worktree, three-lens
split per the brief. Zero survived; eight refuted, all positively.
Zero mutations (none of the eight was a coverage claim); one full
gate execution and one pin-test drive (G1-2), green.

**Process notes.** (1) The stale-worktree trap fired for all eight
verifiers (`c3a00768c` again, third consecutive pass) and the
first-action rule caught it all eight times; the rule needs no
change. (2) The supplied verifier context earned its place: facts (a)
and (b) each prevented a wrong credit (a ruled disposition and a
re-report), fact (d) closed S-1's last consumer route without a
re-derivation — pre-verified context in the brief is cheaper than
eight verifiers re-proving it. (3) The orchestration slug again
exceeded the 255-byte name limit (854 bytes); series convention names
the file, per the template.

**Exit criterion (two consecutive rounds with no new CONFIRMED
findings): NOT met — unchanged.** The in-family ten already reset the
count; this leg adds zero CONFIRMED and moves nothing. Earliest close
remains **round 11**, and round 8's condition stands: the closing
round must include an out-of-family pass on the actual fix commits —
this leg demonstrates those mechanics end to end a second time
(packet-scoped sources, supplied context, per-finding verifiers,
duplicate discipline). Round 10's subject is the in-family §8's,
unchanged, with one increment: the R9-9 prose repair should sweep the
G1-2-misleading recovery banner phrase in the same pass.
