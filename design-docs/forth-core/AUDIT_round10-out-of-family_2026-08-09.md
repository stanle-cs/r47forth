# Audit — round 10 out-of-family refutation pass at `9230d36b2` (addendum to AUDIT_round10_2026-08-09.md)

Range `013256b76..9230d36b2` — round 9's two fix commits (`7d5d805ec`,
`b9d885046`) plus their two records commits — same subject as the
in-family report, verified at the same tip, which is also the main repo
HEAD. This is the out-of-family leg that report's §1 and §8 reserved,
under the name §8 reserved.

One reader family this round: **Gemini 3.1 Pro, working from
self-contained packets with no repository.** That is the whole shape of
this leg — every claim arrived as a structural reading of quoted code,
with the call paths guessed, so every claim was judged against the tree
rather than against the packet. Ten findings, three packets, one verifier
each in its own worktree with a named lens. **Two survived, eight
refuted.** Five of the ten refutation runs proved their verdict by
executed mutation rather than by reading, including the one that went
deliberately RED. No code changed; the tree every verifier finished on is
the tree it started on.

Both survivors are the same question asked at two sites: **a proxy is
being tested for a match where the code needs an identity.** Neither is
new machinery from this wave — one is the wave's own shared restore, one
is Stage L's — and neither changes the in-family verdict.

---

## 1. Subject and coverage

**Commits.** `013256b76..9230d36b2`, everything verified at
`9230d36b2`. Every verifier worktree spawned STALE at `c3a00768c` — the
exact round-6 ref, fourth consecutive pass — and every verifier executed
the first-action rule (`git log --oneline -1`, checkout of the audited
tip, detached) before its first read. 10/10 fired, 10/10 caught. All ten
worktrees were re-checked at synthesis: clean, at `9230d36b2`.

**Reader and packets.** Three packets, ten findings that reached
refutation (packet labels `A2`–`A3`, `B1`, `B2`, `B4`, `B5`, `C1`–`C4`;
`A1` and `B3` did not reach this pass).

- **Packet A — the shared cursor restore and FHIST.**
  `_forthRestoreCursorTuple` and its two callers, `forthHistoryPush`'s
  save/evict/restore bracket. Two findings.
- **Packet B — R9-4's HOME.3 seams.** `forthConsoleHomeRow`,
  `forthConsoleBaseOnTop`, `forthConsoleShowSurface`, and both upstream
  twins (`keyboardTweak.c` `openHOMEorMyM`, `screen.c` `Shft_handler`).
  Four findings.
- **Packet C — the resume path.** `_forthStepHasCaptureShape`,
  `_forthStepIsCaptureStepInHistory`, `forthCaptureResume`'s rule
  selection and splice, `forthHistoryProgram`. Four findings.

**Refutation-only.** This leg ran no find phase; its coverage IS those
ten findings. Everything outside the three packets has no out-of-family
coverage this round and rests on the in-family pass alone — in
particular R9-10's `fnPem` reshape, R9-3/C6's `fnForthOuter` arm, the
`design-audit.sh` instruments (where three of the in-family ten live),
and the test sources read as code.

**What the budget did not reach.** Nothing was driven on simulator or
hardware. Five findings were settled by trace and rulings without a
mutation, correctly — four of them are reachability or intent claims,
where a green suite proves nothing. The one escalation this report makes
beyond a finder's own stated consequence (R10-OOF-2's history-push
capture) is traced at the tip and **not** executed; it is named as such
at the finding, with the one-fixture test that would settle it.

**Collisions with the in-family leg were checked, not assumed.** Two of
the ten touch findings the in-family report already ruled on — one it
CONFIRMED (R10-2) and one it explicitly cleared (§6's last bullet, the
C2 tuple comment). Both collisions are resolved in this file rather than
left for the reader to notice: §3's first finding promotes the cleared
residual on executed evidence, and §6 carries the R10-2 collision with
its disposition.

---

## 2. Mechanical results

**No mechanical half of its own.** Same tip, no code changed; the
in-family §2 stands unmodified — gate GREEN with zero compiler warnings,
`design-audit.sh` exit 0 with the mechanical half clean, churn scanner
zero, RULE-1 net flash 0 B across the range, arena untouched.

Five verifiers independently ran the full gate at `9230d36b2` as part of
their proofs (`build-test.sh`: refresh clean, `FORTH SELF-TEST: ALL
PASSED`, upstream `meson test testSuite` OK, exit 0). One run was RED on
purpose: packet B4's second mutation, which installed the finder's own
requested spec and reddened test `[7]` — see §6. Every mutation was
applied, observed and reverted inside its step, including the
refresh-generated `files/` twins and `.refresh-manifest.json` that the
gate rewrites from a mutated working area.

**Tree state at synthesis.** `packages/` and `design-docs/` clean at
`9230d36b2`; the only uncommitted paths in the subject tree are the three
`.claude/skills/cross-model-audit/` files carrying this round's packet
tooling and the in-family report itself, both outside the audited
subject. All ten verifier worktrees clean.

---

## 3. CONFIRMED findings, worst first

Two.

**Ranking, stated because the two are close and the ordering is a
judgement.** R10-OOF-2 has the worse ceiling — the owner's own program
steps deleted — and R10-OOF-1 has the reachable door: an executed probe,
no unusual choice by the owner, and a gesture the console performs on
every ENTER. Ranked by what it costs the owner, reach beats ceiling here,
so the drift goes first. If the owner's judgement is that a documented,
unreserved name in the user's own namespace is the more expensive
exposure, the order reverses and nothing else in this report changes.

### R10-OOF-1 (packet A3) — the keep-the-saved-step arm tests that the saved step number still FITS, never that it still means the same step; an evicting history push restores the cursor onto a different FHIST line, silently

`packages/forth-core/programming/forth_fold.c:624-625`, consumed at
`:695-707` — wrong result, medium-high confidence, correctness lens.
**Executed, not argued.**

**Reaching input.** PRGM → PROGS → open FHIST in PEM → park anywhere deep
enough to survive the next eviction and shallow enough to still fit after
it (step 50 of 147 in the executed probe) → EXIT → FORTH → type a line
long enough to carry FHIST past `FORTH_HISTORY_MAX_BYTES` → ENTER.
`forthInteractiveEnter` (`:415`) → `forthHistoryPush` →
`_forthHistSaveCursor` (`:984`) → `forthHistoryEvict` (`:988`) →
`_forthHistRestoreCursor` (`:990`). Eviction removes steps oldest-first
from the FRONT of FHIST (`:922-940`), renumbering every later local step
down; the restore then re-parks on the saved NUMBER.

Executed at the audited tip, in a worktree, with a probe mirroring test
`[11]`'s fixture but parking shallow instead of deep, comparing the step
payload across the push:

```
[PROBE] hist=2 numberOfPrograms=2 err=0
[PROBE] parked prog=2 step=50 payload='143' | steps 147 -> 130 | restored prog=2 step=50 payload='161'
```

Eighteen lines of drift, cursor still `prog=2 localStep=50`,
`lastErrorCode` 0. Nothing tells the owner.

**What breaks.** The owner returns to PEM looking at a history line they
were not on, with the display window re-derived around it. The next PEM
edit — insert, delete, BST/SST from that position — acts on that line.
No error, no visible cue: the step number survived and the meaning did
not.

**Violated.** The tuple's own contract, `forth_fold.c:655-657`:
*"C2: the cursor tuple. (program, localStep) — NOT a saved global step
number, which program-boundary shifts (FHIST growing/evicting) would make
stale by restore time."* R9-2's own comment forty lines below concedes
the gap — *"it does for the program half only"* (`:695-702`) — and files
it as narration, not as a decision anyone ruled. The wave's class-closure
claim is the sharper contract, DESIGN-HISTORY.md's R9-1/R9-2 entry:
*"the class test drives every mutation that can happen between save and
restore — deletion for the fold context (`[10]`), eviction for L1-H's
(`[11]`)"*. Eviction is driven; its consequence is not asserted.

**No ruling covers it.** Upstream's `fnClP`
(`src/c47/programming/manage.c:316-360`) restores
`savedCurrentLocalStepNumber` verbatim and renumbers only the PROGRAM
half across a deletion — but upstream has no background step-deleting
mutation of a program the cursor is parked in, so its convention does not
reach this arm. The standing "upstream convention first" rule decides the
fallback (step 1 on overshoot, already implemented); it says nothing
about a step number that still fits.

**Two of the finder's claims are wrong and must not ride along.**
*(1) "at both callers" is not established.* The fold caller
(`forthFoldLeave`, `:1459`) holds its tuple only between `forthFoldEnter`
and the unwind, and a console ENTER cannot occur inside that bracket: two
sibling verifiers traced it independently (packet A2's — `forthHistoryPush`'s
only non-test caller is `forthInteractiveEnter`, unreachable while
`tam.mode` routes keys to TAM; packet C2's — `tam.mode` reaches 0 only in
`_tamLeave`, which is followed by `forthFoldUnwindIfDone` in the same
call chain on both routes). The finding stands at
`_forthHistRestoreCursor` alone. *(2) "a fold whose sweep deletes
preceding debris"* is wrong about the sweep: it cuts forward from the
capture step at FHIST's tail (`:1352-1360`), so it can only remove steps
AFTER a parked cursor.

**Scope, stated so the fix is not over-built.** Only a cursor parked
inside FHIST can drift. Local step numbers and `getNumberOfSteps()` are
per-program, so eviction renumbers nothing outside FHIST.

**Class.** *Range test standing in for an identity test* — a saved index
restored on a validity check when the tuple promises a position. Third
turn of the "saved cursor tuple with an unmaintained half" class
(R8-1 → R9-1/R9-2 → here): round 8 fixed the program half, round 9 fixed
the crash the step half caused, and what is still unmaintained is the
step half's *meaning*.

**Class test.** For every mutation that can occur between save and
restore — delete-at, delete-before, evict — assert step IDENTITY, not
step validity: stamp each fixture line with distinct payload text and
assert the restored `currentStep`'s payload equals the parked one, or
that the ruling's stated fallback (step 1) was taken. Test `[11]` must be
parameterised over park depth, because today it parks at
`stepsBefore - 1` (146 of 147), overshoots the post-eviction count (130)
and lands on the else arm — **the fixture exercises only the arm where
the bug is absent**, and its three assertions (`currentStep` non-NULL,
program in range, `localStep <= stepsAfter`) are all satisfied by a
cursor on the wrong line. Red-first reproducer: the probe above.

### R10-OOF-2 (packet C3) — `forthHistoryProgram` identifies the package's own store by NAME, first match wins; an owner program labelled `FHIST` takes the identity, and the console then appends to and evicts from the owner's program

`packages/forth-core/programming/forth_fold.c:714-724` — wrong result
with a data-loss ceiling, medium confidence on reach, intent lens.
Pre-range: the identity is Stage L's (L-R7), not this wave's.

**Mechanism.** The function walks `labelList`, returns
`labelList[i].program` on the first global label whose bounded name is
the five bytes `FHIST`, and applies no uniqueness test and no test that
the program is the one the package created.

**Reaching input, primary.** On a machine that has not yet pushed
history, the owner writes a program with a leading `LBL 'FHIST'` (PEM →
LBL → ALPHA → `FHIST` → ENTER; nothing upstream rejects a duplicate or
reserved global name — `findNamedLabelWithDuplicate`,
`src/c47/programming/manage.c:1887`, is built on the assumption that
duplicates exist). Then: FORTH → type a line → ENTER. `forthHistoryEnsure`
(`:861-863`) sees `forthHistoryProgram() != 0` and returns true without
creating anything, so the package never gets a store of its own;
`forthHistoryPush` parks at that program's END (`:986`), inserts the line
as an `ITM_FORTH` step (`:987`), and `forthHistoryEvict` (`:988`) then
deletes that program's OLDEST steps, oldest-first, until it is under
1,024 bytes (`:922-940`). The owner's program is now a history buffer:
their lines run when it is executed, and everything above 1 KB is gone.
(One accidental limit: `_forthHistProgramBytes` returns 0 past a 512-step
walk cap, which stops eviction on very long programs. That is the round-8
walk guard doing something it was not written for, not a defence.)

**Reaching input, secondary** — the finder's own, when the package's
FHIST already exists and the owner's label sits in a lower-numbered
program: `_forthStepIsCaptureStepInHistory` (`:197-207`) bounds the
capture step against the owner's program, the resume canary falsifies,
the suspension is abandoned and the typed line is dropped.

**Escalation status.** The primary path is traced at the tip
(`forthHistoryEnsure` → `Push` → `Evict`, all three read line by line),
**not executed**. The finder claimed only the secondary path. If the
owner wants one number before ruling, it is the class test below.

**Violated.** The function's own header — *"Program number of the FHIST
program"* — and the structural rule R9-5 built the predicate around
(`:157`): *"The capture step lies INSIDE FHIST, because the capture step
is only ever created there."* Both read the name as an identity.
DESIGN.md §8.1 states the artefact as *"One kept, named, runnable
program"* and gives it a name with no reservation.

**Nothing rules on it, and the project's posture is against reading it as
the owner's own fault.** L-R7 ruled the identity deliberately
(`STAGE_L_TRACES.md:918-932`: *"a named, runnable program … a leading LBL
gives it a name in PROGS, lets XEQ reach it, and gives the sweep a
discriminator"*) and ruled exactly one collision family with it —
label-vs-item shadowing, *"it must NOT be `FORTH`"* — with a landing
check to match (`PACKET_L1_H:28-41`: grep `items.c`, drive
`forthResolveXEQ("FHIST")`). No document in `design-docs/forth-core/`
rules on a SECOND label spelled `FHIST`; the searches are in the
verifier's record. And the DELP-of-FHIST-by-name door, which is the same
gesture family (the owner acting on the store by its published name), was
twice worked as a defect and fixed rather than ruled out of scope
(round 7/8's P-1, then the residue wave's executed 13 → 5 step loss).

**Mitigation for the fix discussion, not a refutation.** First-match by
name is upstream's own convention for resolving a named global label
(`manage.c:1887`, duplicate index threaded from the TAM at
`src/c47/ui/tam.c:947`), so the SHAPE is conventional. What is unruled is
that a package-private store is identified by an ambiguous name in the
owner's namespace.

**Class.** *Identity by unreserved name* — a private artefact named in a
namespace the owner also writes, with no ownership test at the point of
use. Same family as R10-OOF-1 and as R8-1's address-plus-shape identity:
a proxy that matches is treated as the thing itself.

**Class test.** Enumerate the package's named artefacts (today: FHIST)
and, per artefact, drive the collision from both sides. Fixture: build an
owner program `LBL 'FHIST'` with N content steps totalling more than
1 KB, push one console line, and assert (a) `forthHistoryProgram()` is
not the owner's program, and (b) the owner's step count is unchanged. Then
the reverse order — package FHIST first, owner's label inserted into a
lower-numbered program — and assert a fold suspend/resume round trip
still lands its line. Red-first: (a) and (b) fail today as written.

**What it costs.** If the owner never picks the name, nothing. If they do
— by hand, or by restoring a backup made before this package existed —
their program is silently rewritten by an unrelated gesture.

---

## 4. PLAUSIBLE findings

**None.** Both survivors carry a constructible reaching input: OOF-1's
was executed, OOF-2's is an owner-typed gesture with no state precondition
beyond a name. The nearest miss is packet C1's missing lower bound on
`_forthStepHasCaptureShape`, and it is not in this section on purpose:
the verifier ran a positive control (214 live calls, minimum offset +10,
zero below base) and enumerated the writer set to two live address
differences, so it is refuted, not unconstructed. It stays in §6.

---

## 5. Design observations (D7)

**O-a — every survivor this project has produced in three rounds is an
identity resolved by a proxy.** R8-1: remembered address plus opcode
shape. R9-5: the same rule spelled per site. R10-OOF-1: a step NUMBER
standing for a step. R10-OOF-2: a NAME standing for a program. C17's
softmenu stamp is the one member of the family that was built the other
way round — it exists precisely because a menu id could not tell the
console's frame from anyone else's — and it is the only one that has
never produced a finding. The shape worth carrying forward is a question
to ask at every stored reference: *what makes this the same one, rather
than a matching one?* Where the answer is "nothing", the code should say
so in one line, the way `_forthStepIsCaptureStepInHistory`'s banner
already does for its own bound.

**O-b — rulings bind mechanisms; findings are about outcomes, and this
pass nearly lost a confirmed finding to the difference.** Packet B1
reported the HOME.3 depth-2 failure — the same defect the in-family pass
CONFIRMED as R10-2 with an executed depth-2 trace — and the intent-lens
verifier refuted it, correctly citing DESIGN.md's C18 rulings: one pop
per press, and a buried owned base retargeted in place. Both rulings are
real and both are about the MECHANISM. Neither says what the owner should
see at depth 2, which is what the finding was about. The disposition is
in §6; the lesson is procedural and belongs in the brief: an intent
refutation clears a claim only when the cited ruling addresses the claimed
OUTCOME, not when it explains the mechanism that produces it.

**O-c — this leg boxes R10-2's fix space on three sides, which is worth
more to the owner's ruling than the finding it lost.** Executed here:
narrowing the dismiss predicate back to upstream's three-way alphabetic
test makes the gate RED (`menu=-1331 baseOnTop=0`, test `[7]`'s own
"the overlay is stuck" assertion — packet B4), so the ownership predicate
cannot be traded back. Ruled and cited: hoisting the console's base to
slot 0 is refused by C17/C18 (user rows above and below stay untouched),
and pushing a fresh row over the base is R9-4's original defect. What is
left for the owner's ruling #1 is exactly two shapes — unwind to the
console's base (pop until `forthConsoleBaseOnTop()`, then retarget) or
keep one-pop-per-press and rule that the gesture is a ladder rung, not a
landing — and the second requires amending R9-4's own text, which says
the gesture is named for landing.

**O-d — a reader with no repository gets the structure right and the
call paths wrong, and the numbers say so.** Ten findings: the two that
survived are both structural questions about what a stored value means
(range vs identity, name vs identity), answerable from the quoted code
alone. Every one of the eight refuted died on a mechanism the reader had
to guess — that the fold sweep cuts at the cursor (A2), that
`forthConsoleShowSurface` is skipped (B2), that `execTimerApp` reaches
`Shft_handler` and that the twin has no repaint (B5), that a saved-state
restore can plant a synthetic offset (C1), that a PEM capture can be
resumed under a fold rule (C2), that a live program insert grows the
capture step's own program (C4). Even the survivors carry a wrong
mechanism each (OOF-1's "the sweep deletes preceding debris", OOF-2's
understated consequence). The way to spend this reader family is on the
structural question — *what does this value promise, and what is
actually checked?* — and to treat every call path it states as a
hypothesis for the verifier, which is what the workflow already does.

---

## 6. Deliberately not flagged

Mandatory, and this leg's material is the eight refutations plus the
collisions and the residuals they left. The reader supplied findings
only, with no per-dimension coverage census of its own, so nothing here
comes from a finder's cleared list; every entry below is something a
verifier disproved at the tip, or something this synthesis saw and left.

### The HOME.3 seams — packet B, four findings, all refuted

**B1 — the land half cannot land: `forthConsoleShowSurface` retargets a
buried base in place rather than lifting it (`forth_menu.c:711`).** The
mechanics are right; the classification is refuted on intent. Both halves
are ruled: DESIGN.md:2852-2856 rules the overlay rung pops *"any
user-stacked row above the console's base, one per press"*, and
DESIGN.md:2863-2872 rules the console *"retargets the console's OWNED
base frame in place at depth so the indicator is right the moment the
overlay pops — possible only because C17's stamp identifies the frame
when it is buried"*, restated at the site (`forth_menu.c:586-594`,
*"User rows above and below stay untouched"*). "Landing" in this package
means the console's base row agrees with `keysMode` (K-R3), not that a
row is pushed to slot 0 — and pushing a fresh row over the base is R9-4's
removed defect. **Collision, and it is not a tie:** this is the in-family
R10-2's claim, and R10-2 stands. The refutation clears the two mechanisms
it cites and does not reach R10-2's decisive evidence — the executed
depth-2 drive showing a raw `-MNU_ALPHA` frame over a keys-mode keypad,
which is the K-R3 state R9-4 exists to eliminate and which no ruling
sanctions. Net: no new finding here (R10-2 already carries it), and the
refutation's rulings are promoted into R10-2's fix space (§5 O-c).

**B2 — `forthConsoleBaseOnTop`'s identity fallback is documented safe
only in the direction that DECLINES to pop, and R9-4 consumes it in the
direction where false MEANS pop (`forth_menu.c:706`).** The claimed
inversion does not exist: the EXIT ladder's rung 2
(`forth_console.c:310-314`) is `if(!forthConsoleBaseOnTop()) {
popSoftmenu(); … }` — byte-for-byte the same direction R9-4 added, and
the predicate's own header defines false as *"something is stacked above
it (so EXIT should pop)"*. The comment's "can only make rung 3 decline a
pop" is about rung 3, which the fallback reaches only on a TRUE answer.
The stated consequence is also wrong about the code: `forthConsoleShowSurface()`
runs unconditionally after the pop. And the required state (live capture,
no stamp anywhere, visible row neither `-MNU_FORTH` nor `-MNU_ALPHA`)
could not be constructed at a key-dispatch boundary — every enumerated
stamp-destroying path funnels through `forthConsoleRestoreSurface()`
before control returns to the key loop, and no key is dispatched inside a
line. Round 5's three documented no-stamp doors all leave `-MNU_ALPHA`
visible, which is where the fallback answers TRUE.

**B4 — the dismiss half pops ANY frame over the console, where the
gesture it translates pops only an alphabetic overlay
(`forth_menu.c:707`).** Path granted: a non-alphabetic overlay is
reachable and is popped. Verdict refuted, by mutation. The ownership
predicate is round 8's ruled fix (*"the guard's question is 'is slot 0
MINE', not 'am I live'"*, DESIGN-HISTORY.md:3728-3735) re-affirmed by
R9-4 (*"the faithful translation is not 'copy the ALPHA push' but
'evaluate the same conditional against the state we actually have'"*).
The counterfactual has no console analogue: with a foreign row on top and
no pop, `forthConsoleShowSurface` deliberately touches nothing visible,
so the gesture becomes a no-op. Executed both ways at the tip: with a
`-MNU_FIN` overlay the gate stays GREEN and the gesture lands on the
console's own owned base (`menu=-213 keys=1 baseOnTop=1`); with the
finder's requested spec installed the gate goes RED on test `[7]`'s own
assertions (`menu=-1331 baseOnTop=0`, *"the overlay is stuck and the
owner's dismiss gesture does nothing"*) — the round-8 defect the
predicate was introduced to close.

**B5 — the `screen.c` seam skips the native arm's
`showSoftmenuCurrentPart()` repaint, which the `keyboardTweak` twin does
not have (`screen.c:925`).** Both halves of the premise are false. The
`screen.c` guard block ends at `:977-980` with `screenUpdatingMode =
SCRUPD_AUTO; refreshScreen(23);` for every arm including the console's,
and `SCRUPD_AUTO` is `0x00` (`src/c47/defines.h:2027`), so
`_refreshNormalScreen`'s menu predicate is unconditionally true and
`showSoftmenuCurrentPart()` runs there — a stronger repaint than the one
the seam skips, in the same handler invocation. The twin does not "have
neither": `keyboardTweak.c:294-298` calls `showSoftmenuCurrentPart()` and
the same `SCRUPD_AUTO` + `refreshScreen(23)` after its if/else. The
two-site conversion is ruled and pinned (`design-audit.sh:508-527`,
`pin 2 "twin HOME.3 sites going through forthConsoleHomeRow()"`).
Two factual slips beside: `execTimerApp` is a different timer callback,
and the shift-F long-press the finding names lands at the
`keyboardTweak` site via `screen.c:1024`, not at `:925`.

### The resume path — packet C, three of four refuted

**A2 — the step-1 fallback is upstream's answer for a FOREIGN deletion,
applied to the fold's own LOCAL deletion (`forth_fold.c:632`).**
*(Packet A, filed with the resume set because it is the same helper.)*
The premise "the fold's deletion is LOCAL and initiated at the cursor" is
false in this tree: `forthFoldLeave` re-anchors onto the capture step
first (`currentStep = cap; defineCurrentProgramFromCurrentStep();`), and
`cap` is guaranteed inside FHIST — round 8's P-1 fix, whose purpose was
to stop the sweep measuring or cutting any other program. For a saved
program other than FHIST nothing the fold does changes its
`getNumberOfSteps()`; for saved program == FHIST the sweep is
floor-bounded at `entryStepCount + 1 + _forthFoldKeptSteps` and
`savedLocalStep` was sampled at or below `entryStepCount`, so the keep
arm is taken. The only door into the else arm is a third-party deletion
(a PARK dispatch running DELP through `_clearProgram`, or the clamp after
an unannounced shrink) — the foreign deletion upstream's convention
governs.

**C1 — the shape test dereferences `p` before any lower bound, and the
history predicate calls it before its `from`/`to` bound
(`forth_fold.c:149`).** The structure is exactly as described and no
below-base `p` is constructible. `savedStepOffset` has two production
writers (`:90`, `:250`), both live non-negative address differences
against `beginOfProgramMemory`; the three test writers pass 0. The
reader's named door is closed by construction: `forthCap` is file-static
BSS, deliberately not persisted, and the restore path drives it to
FCAP_CLOSED before any offset is consumed (`test_persist.part.h:487`
states and pins it), so the `forthCapIsSuspended()` gate refuses.
Relocation re-adds the offset to the new base, and a shrink pushes `p`
ABOVE `firstFreeProgramByte`, the conjunct that already exists. Measured:
instrumented gate, 214 live calls, minimum offset +10, zero below base,
with a positive control run because the first count was suppressed by
binary-file grep. The missing lower bound is defence in depth of the same
shape the R9-5 banner already documents — *"If someone finds the door, it
is a finding with a reaching input"* — and this was not the door.

**C2 — the resume picks its validation rule from `forthFoldPending()`,
the CURRENT bracket state, to validate an offset created under a PAST
state (`forth_fold.c:241`).** Requires `FCAP_ORIGIN_PEM` with a fold
pending at the resume, and that state is unreachable. `foldMode` has one
production writer (`forthFoldEnter`), one production caller
(`ui/tam.c:1232`, guarded by `forthCapIsInteractive()`), and it is set
one line BEFORE the `forthCaptureSuspend()` that creates the offset the
resume validates — so the flag is not "current state versus past state",
it is the state that governs the suspension it precedes. The reverse
ordering is closed too: PEM origin comes only from `pemAlpha`, whose
capture-opening site is gated on `!tam.mode`, and `tam.mode` reaches 0
only in `_tamLeave`, which is followed by `forthFoldUnwindIfDone` in the
same chain on both routes. Probe over the whole battery (folds, PARK, PEM
captures, DELP-of-FHIST, the GTOP promotion): zero hits, gate green.

**C4 — the splice's step-count baseline assumes the only mutations
during a suspension are TAM commits (`forth_fold.c:312`).** The two
quantities differenced measure the same program by construction: `saved`
is sampled while `currentProgramNumber` is the capture step's program,
`total` immediately after `defineCurrentProgramFromCurrentStep()`
re-anchors to it (round 6's F1 fix), so growth elsewhere is invisible to
the subtraction. The named growth door is count-neutral, not merely
unobserved: `firstFreeProgramByte` points AT the `.END.` step, so
`insertStepInProgram(ITM_END)` in the `GTO . .` arm lands before it and
the previously-last program swaps a counted `.END.` for a counted `END`.
The shrink door cannot reach the line: DELP of the capture step's program
falsifies the canary and the resume abandons first; DELP of any other
program leaves the count alone. Measured: 101 splice evaluations across
the gate, every `n` is 0 or 1 — including the reader's own scenario
(`[9] GTO . .`: `total=35 saved=35 n=0 prog=2`) and both DELP cases.

### Residuals seen and deliberately left

- **The fold half of R10-OOF-1.** Two verifiers independently closed the
  window in which `forthFoldCtx`'s tuple could see an eviction. Not
  flagged, and the fix for OOF-1 should not be built as though it were:
  if the shared helper grows an identity check, both callers get it for
  free, but the fold caller does not need one on today's evidence.
- **The C2 tuple comment.** The in-family §6 cleared it as *"worth a
  sentence, not a finding"*, reading the residual as *"the cursor landing
  on a different FHIST line after eviction — is what eviction means"*.
  The executed probe is why this leg promotes it: the drift is not the
  cursor following its line, it is the cursor keeping a number and
  changing lines. The comment is now the documentation half of
  R10-OOF-1 and should be repaired with it, not separately.
- **`_forthStepHasCaptureShape`'s missing lower bound (C1).** Left, per
  its own banner's standing rule. It becomes a finding the day a writer
  of `savedStepOffset` exists that does not compute a live difference.
- **`foldMode`'s deliberate stickiness across
  `forthCapClose`/`AbandonSuspended`/`Open` (C2's residual).** Documented
  at `forth_capture.h:70-82` and pinned by test `[7]`. It makes C2's
  state constructible the day a strand door leaves a fold un-unwound with
  `tam.mode` 0 — a conditional on a defect nobody has produced, and
  `forth_console.c`'s F8 residue rung unwinds the fold before resuming
  anyway. Not filed.
- **`popSoftmenu`'s absent depth precondition.** Already cleared
  in-family (shift-and-zero over a fixed array); B2's and B4's traces
  agree. No underflow boundary at `forthConsoleHomeRow`'s single pop.
- **`forthConsoleBaseOnTop`'s scope comment (B2's real content).** The
  comment defends the fallback for the EXIT consumer and is now read at
  two consumers. In-family R10-2 already names this in its class
  paragraph; one sentence when the file is next open, bundled with
  whatever R10-2's ruling moves.
- **The R9-5 banner's stated-so-it-can-be-attacked assumption.**
  Attacked by C1 and C4 from two directions this round; held both times.
  Still unbuilt, still correctly unbuilt.

---

## 7. Verdict

**Would I ship this? The in-family answer stands unchanged: not before
R10-1**, and nothing in this leg softens or sharpens it. Where the range
breaks first is unchanged — R10-1's lost zeroth-step position on every
console ENTER, then R10-2's HOME.3 depth question.

**What this leg adds.** Two findings and one fix-space narrowing. Neither
finding is a crash and neither was introduced by round 9's wave:
R10-OOF-1 is the wave's own consolidation carrying a defect the class it
closed did not name, and R10-OOF-2 is Stage L's identity decision meeting
a case nobody ruled on. Both are cheap to pin and neither is cheap to
notice from a bug report — they are silent by construction, which is
precisely why an audit is where they surface.

**What I would leave alone if the goal were correct code rather than an
audit-clean tree.** R10-OOF-2's *code*. The name collision needs a
ruling, not a patch: if the owner rules that a second `FHIST` is the
owner's business, the honest close is one sentence in DESIGN.md §8.1
reserving the name and the class test above kept red-first for the day
the ruling changes; if the owner rules the store must own its identity,
the fix is a check at one function and the test is already written in §3.
Writing the check before the ruling would be inventing a policy at the
same site where L-R7 already made one.

R10-OOF-1 I would fix. It is a silent wrong result on a stored value the
code says out loud is eviction-proof, its reproducer exists and executes,
and it sits in the same helper R10-1 must be reopened for — which makes
it nearly free to close and expensive to defer, because the next wave to
touch `_forthRestoreCursorTuple` is the fourth in a row.

**One process cost, priced.** Packet B spent four findings and four
verifier cycles on the HOME.3 seams and produced no finding the in-family
pass did not already have — and one of those cycles refuted a claim the
in-family pass confirmed. That is not waste (§5 O-c is the return), but
it is the measured price of running an out-of-family leg against a
surface where the in-family leg found a live defect first.

---

## 8. Round and exit state

**Round 10, out-of-family leg.** Subject `013256b76..9230d36b2` at
`9230d36b2`; tree clean before and after.

**Readers.** One out-of-family family — Gemini 3.1 Pro, no repository,
three self-contained packets — producing ten findings that reached
refutation (`A2`, `A3`, `B1`, `B2`, `B4`, `B5`, `C1`, `C2`, `C3`, `C4`).
Ten verifiers, one per finding, one worktree each, lens per the brief.
**Two survived, eight refuted.** Five refutations proved their verdict by
executed mutation (A3's identity probe, B4's two, C1's probe plus
positive control, C2's state probe, C4's splice probe); five were settled
by trace plus rulings, correctly, since four of those were reachability
or intent claims a green suite cannot answer. One duplicate against the
standing record (B1 = in-family R10-2), credited as corroboration of the
outcome and as material for its ruling, not re-confirmed.

**Process notes.**

1. The stale-worktree spawn trap fired on all ten verifiers
   (`c3a00768c`, fourth consecutive pass) and the first-action rule
   caught it all ten times. The rule needs no change; the spawner does.
2. **Shared scratchpad contamination, second sighting this round.** Two
   verifiers independently reported sibling agents writing the same
   scratchpad filenames — one had its gate log clobbered mid-run and
   another found three worktrees' output interleaved in one file. Both
   detected it and re-ran to uniquely named paths, and both flagged it
   unprompted. This is D7-7's per-verifier scratchpad item, now with two
   near-misses behind it: a verifier that did not notice would have
   quoted another worktree's numbers.
3. **A grep trap worth carrying into the brief.** The gate log contains
   control bytes, so plain `grep -c` reports nothing and reads as a clean
   negative. C1's verifier caught its own wrong count only because it ran
   a positive control. Probe counts over gate logs use `grep -a`, and a
   zero-hit probe result is not evidence without a control that hits.
4. Two verifiers reached the same conclusion about the fold bracket from
   opposite directions (A2 via `forthHistoryPush`'s caller set, C2 via
   `tam.mode`'s writers). That agreement is what let §3 scope OOF-1 to
   one caller rather than repeating the finder's "both".

**Exit criterion (two consecutive rounds with no new CONFIRMED
findings): NOT met, unchanged.** Round 10 now stands at twelve confirmed
(ten in-family, two here) plus one plausible. **Earliest close remains
round 12**, and round 8's condition stands: the closing round must
include an out-of-family pass on the actual fix commits.

**Round 11's subject — increments to the in-family list, not a
replacement.** The in-family §8's five items stand in order. Add:

1. **R10-OOF-1 folds into item 1.** R10-1's fix reopens
   `_forthRestoreCursorTuple`; the per-field round-trip class test that
   packet already owes must assert step IDENTITY, not step validity, and
   `[11]` must be parameterised over park depth so the keep arm is
   exercised at all. One packet, two findings, one test matrix.
2. **R10-OOF-2 is an owner ruling before it is a packet** — add it to the
   rulings owed, beside the HOME.3 depth question: does the package's
   store own its name, or does the owner? The fix shape follows, and
   either answer lands a red-first fixture.
3. **The B-packet material rides with R10-2's ruling** (§5 O-c): the
   executed RED that forbids narrowing the predicate, and the C17/C18
   rulings that forbid hoisting. Whoever writes that packet should not
   re-derive them.

**Owner rulings owed, updated.** Carried from in-family §8: HOME.3 at
overlay depth ≥ 2; the zeroth-step restore reading; R9-P1's L1-H
convention question; P1 (round 3); P2's push ruling; C22-vs-C1. **New:
FHIST's name — reserved, or the owner's to collide with.**
