# Audit round 3 — the C17/C18/C19 fixes and the C21 battery

Subject: `b5a0202c9..48c8776fe` — everything landed in the second session of
2026-08-06. Written by the author of the code under audit, which is a
deviation stated up front; see §8.

---

## 1. Subject and coverage

Audited: `forth_menu.c/.h`, `forth_capture.c/.h`, `forth_compile.c`,
`keyboard.c`, `items.c`, `programming/manage.c`, and the three test files
those commits touched.

**Readers.** Seven in-family finders (contracts, lifecycle, arithmetic,
error paths, guards, tests, upstream — the `design` finder died on credits),
plus **two out-of-family readers**, Gemini and GPT-5 Sol, each sent the same
self-contained packet of the landed ownership code.

**What this round did NOT get.** The `design` (D7) dimension never ran, and
**the entire adversarial refutation pass and the report synthesis died on
usage credits** — eleven verifier agents and the synthesiser, all of them.
So no finding below carries the three-lens rotation the process requires;
every verdict here is the author's own trace. That is the weakest form of
verification this process has, and it is why §8 says the exit criterion is
not met.

The fold/suspend window is reasoned about but not driven: there is still no
TAM-driven interactive fixture (the gap round 2 recorded, unchanged).

## 2. Mechanical results

Gate green at every step. `design-audit.sh`: 3 finding groups, all baseline
(2 above baseline in group D, both the expected growth from this session's
added blocks). No new compiler warnings.

## 3. CONFIRMED findings — four, all regressions from this session

Four of the seven finders converged independently on the first one, which is
the strongest agreement signal this process has produced.

### R1 — the C17 frame stamp is persisted, and outlives its capture

**Where.** `packages/forth-core/forth_menu.c` (the stamp), against
`saveRestoreBackup.c:293` / `:986`.

**Reaching input.** Open the console, save (or let any backup be taken),
restore. `softmenuStack` is saved and restored **wholesale as a hex dump**,
`userMenuId` included. The restore writes the stamps back at `:986`, which
is **after** the dict-lifecycle seam at `:975-976` whose
`forthCapPowerReset()` → `forthCapClose()` → `forthConsoleUnstampAll()` was
supposed to clear them. The unstamp is silently overwritten.

**What breaks.** After the restore the stack carries stamps with no capture
open. The next FORTH press finds a stamp already present, so
`forthConsoleRegisterSlot0()` declines, the session registers nothing, and
its EXIT reads ownership off a frame belonging to a capture that ended
before the restore — popping the owner's menu or leaking the console's.

**Contract violated.** `forth_capture.h`, on the bit this replaced:
*"Transient UI state, NEVER persisted."* C17 moved ownership out of the
capture object (right, and necessary) and into a **persisted** structure,
which is the one thing the old `homePushed` bit got for free. Exactly the
ordering trap F6-6 already documented for `FLAG_ALPHA`.

**Bug class.** *State moved into a structure with a different persistence
contract than the one it left.*

**Fixed** — unstamp at the top of `forthCaptureSanitizeRestoredUi()`, which
runs at `:1496`, after the stack is back. Class test row (d); mutation R1
reddens it twice.

### R2 — `forthCapAbandonSuspended` closes a capture without unstamping

**Where.** `forth_capture.c:63`. *Two finders.*

**Reaching input.** `forthCaptureResume`'s canary arm calls it standalone
when a suspended capture's step is falsified. It sets `FCAP_CLOSED`
directly and hand-clears `keysMode` and `origin` — it never calls
`forthCapClose()`, so the stamp survives a closed capture.

**Bug class.** Same as R1: a close path that does not pass the funnel.
The function's own existing comments prove the class was already known —
`keysMode` and `origin` are cleared here for precisely this reason.

**Fixed**; class test row (c); mutation R2 reddens it.

### R3 — a line that destroys the console's row without leaving `CM_AIM` is never repaired

**Where.** `programming/manage.c` (the post-run repair block).

**Reaching input.** Type `EXITALL` and press ENTER. It is
`CAT_FNCT | PTP_NONE` (`items.c:3321`), so a typed line resolves and runs
it; `fnExitAllMenus` (`softmenus.c:4250`) pops every frame down to MyMenu —
the console's registered frame among them — and never touches `calcMode`.
The repair block was gated `calcMode != CM_AIM`, so the whole thing was
skipped: console left with no row and no stamp, after which EXIT's fallback
identity test pops the user's own remaining menus one press at a time.

**Bug class.** *Two repairs sharing one guard, where the guard belongs to
only one of them.* The mode repair legitimately needs
`calcMode != CM_AIM`; the surface repair never did.

**Fixed** — the surface repair is now unconditional for an open interactive
capture (`forthConsoleRestoreSurface()` is a no-op when the frame is
intact). Class test 35, driven as a differential against `CLSTK`; mutation
R3 reddens it.

### R4 — the stated invariant was false

**Where.** The `forth_menu.c` banner, `forth_menu.h`, and the DESIGN.md
amendment, all of which claimed *"exactly one frame is registered while an
interactive capture is open."*

*Found by two in-family finders **and** by Sol, independently.* The alpha
excursion opened over the user's own FWRD row registers **two** — a
BORROWED base and an OWNED excursion frame — and that is correct and
load-bearing. The documentation described a rule the code does not follow
and must not follow.

**Fixed** — the invariant now reads *at most one borrowed base and at most
one owned frame, owned above borrowed; never two owned, never a stamp
outliving its capture*, in all three places. `forthConsoleRegisterSlot0`
now enforces the real rule (refuse a second OWNED; refuse a borrow when any
stamp exists), and the ALPHA acquire path routes through it instead of
writing the stamp itself, so one site decides ownership.

## 4. PLAUSIBLE — carried, not fixed

**P1 — a non-ladder close leaves the console's created row standing.**
*Sol.* Any key that closes the capture through
`_forthCapCloseIfInteractive()` clears the stamp but pops nothing, so the
console's FWRD row stays on the owner's stack. **Exact pre-C17 parity** —
the old code left the same frame with a forgotten `homePushed` — so it is
not a regression, and it is now *visible* rather than latent because the
stamp made the ownership explicit. Fixing it means giving the funnel a pop,
which needs a ruling: the funnel runs in contexts where popping may not be
wanted. **Owner's call.**

**P2 — a full stack loses its bottom frame on the alpha acquire.** *Sol.*
With eight frames occupied, the hand-rolled ALPHA insert shifts the last one
off. Identical to what every native `pushSoftmenu` does on a full stack,
including the console's own open — but it *is* a new push site where the
pre-C17 code retargeted in place. Exotic (needs a full stack) and
consistent with native behaviour; recorded, not fixed.

## 5. REFUTED

- **Gemini G1** — double pop at close from a stamped ALPHA row. Refuted on
  reachability: the close rung only runs with `keysMode == 1`, and every
  path into it retargets the owned frame to FWRD first, which
  `calcModeNormal` never pops. The state is unconstructible.
- **Gemini G2** — restore leaves a line-pushed menu over the borrowed base.
  Refuted on intent: a foreign row above the base is the documented decline
  case; it pops on EXIT like any user-stacked row, and the borrowed FWRD
  base beneath is already correct for keys mode.
- **Sol S4** — the open steals a buried FWRD frame. Refuted on intent:
  documented deliberate at the open site (the dedup lift is landed
  behaviour, ruled latent in round 2).
- **Sol S5** — the FWRD picker is not rebuilt after an alpha excursion.
  Refuted on fact: the package's `softmenus.c:3160` adds
  `|| softmenu[m].menuItem == -MNU_FORTH` to the dynamic-menu cache test, so
  every FWRD paint rebuilds. **This one is a packet defect, not a reader
  defect** — the override is invisible to a reader with no repository, and
  the orientation block should have stated it. Same class as round 2's two
  packet artefacts.

## 6. Deliberately not flagged

- The `-Wtype-limits`-clean `int` vs `int16_t` mix in the stamp helpers:
  `userMenuId` is `int16_t` and the sentinels are cast to it; the
  comparisons are exact.
- The two sentinel values (`-0x4643`, `-0x4642`): arbitrary negatives, and
  the negativity is what matters (`removeUserMenuFromStack` only decrements
  values above a non-negative threshold). Not worth a lookup table.
- `forthConsoleUnstampAll` scanning all eight slots on every close: eight
  `int16_t` compares, on a path that already does far more.
- The C18 buried retarget writing `softmenuStack[i].softmenuId` directly
  rather than through a helper: it is the same in-place idiom the EXIT
  ladder's pre-normalisation uses, and a helper would need the index anyway.

## 7. A finding about this session's own test work

Test 11's empty-band assertion has now been **mis-attributed twice** — once
as the register-suppression proof (killed by C21), once as the renderer's
`count == 0` guard (killed here). A third attribution was drafted and
**killed by its own mutation**: removing the `view >= count` skip leaves the
gate green, because `forthConsoleLineAt` rejects the out-of-range view.

The assertion is defended three deep and no single-line mutation can fire
it. It is recorded in the test as a **documented gap** — a belt-and-braces
regression guard that is not evidence of anything — rather than given a
fourth confident story. Three wrong attributions is the pattern worth
keeping: *an assertion's provenance is a claim, and claims get mutations
too.*

Fixture defects this session: **five**, all caught by the C22 rule. The
fifth is new in kind — round 3's own first-draft oracle used
`forthConsoleBaseOnTop()`, whose identity fallback answered "true" for a
stack with no stamp on it, and it **manufactured three false failures**
rather than hiding a real one. Wrong oracles fail in both directions.

## 8. Verdict, round and exit state

**Round 3. Four CONFIRMED, all regressions introduced by this session's own
fixes, all now fixed with class tests and mutation proof. Two PLAUSIBLE
carried. The exit criterion is NOT met and is not close.**

Would I ship it? The console itself, yes — every confirmed finding is fixed
and the batteries that pin them redden under mutation. The *process* state
is the problem, and it is worth stating plainly:

- The refutation pass **did not run at all**. Every verdict above is the
  author's trace of the author's code, which is the arrangement this whole
  system exists to avoid.
- The `design` dimension — the one `CODE_AUDIT.md` calls "the lens that pays
  for the exercise" — did not run either.
- Round 2's headline repeats exactly: **four of round 2's seven findings
  were caused by round 1's fixes; four of round 3's four were caused by
  round 2's fixes.** The rate is not falling. Each round's fixes are new
  code that nobody has audited, and this round proves it twice over.

**Round 4 is required**, and its first job is to re-verify round 3's four
fixes with a refutation pass that actually runs. The out-of-family half of
round 3 did work and did land real findings, so the paste/packet route is
sound; it is the in-family half that needs re-running.

Where it breaks first: the fold/suspend window, which no round has driven
with a real fixture and which R2 shows is reachable in ways the batteries
do not cover.

---

# Round 4 — the refutation pass round 3 never got

Round 3's verdicts were the author's own traces of the author's own code,
because eleven verifiers and the synthesiser died on usage credits. This is
that pass, run afterwards against the four landed fixes. **Readers: Gemini
and GPT-5 Sol, out-of-family, each given the refutation brief from
`PROMPT_CODE_AUDIT.md` verbatim and told to kill the fixes.**

The packet's orientation block was made exhaustive for every package
override the excerpts touch — the fix for the defect that produced round 3's
one bad finding. No packet artefact appeared this round.

## Verdicts

| fix | Gemini | Sol | resolution |
|---|---|---|---|
| R1 restore-seam unstamp | SURVIVES | SURVIVES | **CONFIRMED** |
| R2 abandon-path unstamp | SURVIVES | SURVIVES | **CONFIRMED** |
| R3 ungated surface repair | SURVIVES | SURVIVES | **CONFIRMED** |
| R4 ownership invariant + one register site | REFUTED | REFUTED | **survives — both traces disproved, but see below** |

Three fixes survived both readers independently. That is the first
unanimous-survival result this audit has produced.

## R4 — both refutations were wrong, and both were worth having

Both readers, independently, refuted R4 with the same failure shape: the
ALPHA acquisition PUSHES a frame and then delegates stamping to a function
entitled to DECLINE, so a decline leaves a pushed frame nothing owns — EXIT's
overlay rung pops it, the surface owner re-pushes it, and the press cycles
without reaching the excursion rung.

**Both traces were disproved by probe.** Each assumed the sub-mode toggle
over an OWNED base enters the acquisition path. It does not:
`forthConsoleShowSurface` retargets that frame IN PLACE. Instrumented run,
applied and reverted:

```
R4-PROBE: after toggle — ownsSlot0=1 menu=-1922 menu(1)=-1363
```

Slot 0 is ALPHA and still stamped; slot 1 is the owner's STK. No second
frame, no acquisition call, no orphan. Sol's variant routed through a
surface rebuilt by R3 first, which reaches the same retarget branch.

**But the shape they described is real, and R4 is what prevents it.**
Mutation M-A reverts `forthConsoleRegisterSlot0` to its pre-R4 "decline if
ANY stamp exists" form. Five assertions redden, including the two the
readers predicted in their own words:

```
FAIL: [borrowed base] the excursion row must be the console's base —
      an unregistered row here traps EXIT on the overlay rung
FAIL: [own FWRD: open, toggle alpha then back, EXIT] did not close within
      six EXIT presses
```

So the readers described, precisely, the defect that R4 fixes — they simply
located it in the shipped code instead of the code it replaced. **Two
independent out-of-family readers converging on a failure mode that a
mutation then reproduces is the strongest signal this process has produced,
and it arrived from two findings that were both, as written, wrong.**

## What changed as a result

`_forthConsoleAcquireRow` now returns early by retargeting an existing OWNED
frame instead of stacking a second one, so the push-then-decline window does
not exist by construction. This is **hardening, not a bug fix**, and it is
recorded as such:

- The state is unreachable today. The function's two callers are
  `forthConsoleShowSurface` (only from the BORROWED-base branch, and an owned
  frame is always created ABOVE the borrow, so the borrow cannot be back on
  top while one exists) and `forthConsoleRestoreSurface` (only when no stamp
  exists anywhere).
- **Mutation M-B removes the guard and the gate stays GREEN.** No test can
  pin it, and none is claimed to. Stated here rather than papered over —
  the C22 rule, and the third documented gap of this stage.
- It earns its four lines anyway: the failure mode is the C18 class
  (*"a state change committed by the caller and the display of that state
  established by a callee that may decline"*), which this codebase has
  already paid for once; the reachability argument rests on an invariant a
  future caller could break silently; and two independent readers found it.

## And the invariant is now enforced rather than asserted in prose

Round 3's R4 was a *documentation* defect — the banner claimed "exactly one
frame is registered" for a whole session and three readers caught it before
any test did, because **no test asserted it**. `forth_menu.c` now exports a
selftest-only stamp census, and `test_console_ownership_invariant` checks,
after every step of two gesture sweeps: at most one owned, at most one
borrowed, owned above borrowed when both exist, neither with the capture
closed, and every EXIT press making progress. The sweeps deliberately
include the two gestures the round-4 readers attacked.

Mutation M-A reddens it. That is the fix for the class round 3 could only
describe: *an invariant that lives only in prose is not enforcement.*

## Round and exit state

**Round 4 was a verification round, not a coverage round.** Three fixes
confirmed by two independent out-of-family readers; one attacked by both and
sustained on probe evidence, with a hardening and a new invariant battery
landing as a result.

**The exit criterion is still not met, and one clean round does not meet
it** — it requires *two consecutive* rounds with no new confirmed finding.
Round 4 produced no new confirmed finding but did produce new code (the
hardening, the census, the battery), which by this project's own reset rule
is unaudited. Round 5 needs the in-family dimensions that credits killed in
round 3 — `design` (D7) above all, which has now not run for two rounds.
