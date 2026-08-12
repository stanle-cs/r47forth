# Audit — round 11, out-of-family refutation pass at `868a991ab`

Six findings from one reader family (Gemini 3.1 Pro, two self-contained
packets, no repository), each sent to a verifier instructed to kill it.
**One survived, five refuted.** No code changed; the tree this report
finishes on is the tree it started on.

The survivor is a seam installed at one of two doors. The five refutations
are all the shape the brief predicted — a real path with a wrong
conclusion — and in three of the five the *premise* is real too, which is
why this report spends more of its length on §6 than on §3.

Two synthesis corrections to the verifier record are carried below and
labelled as such: the shared premise behind three findings does **not**
fail (one narrow route reaches it, and the design rules the consequence),
and the survivor's reaching input is **commoner** than either the finder
or its verifier stated.

---

## 1. Subject and coverage

**Tip.** `868a991ab`, on `forth-core/stage-n`. Range since round 10's
verified tip: `9230d36b2..868a991ab`, 21 commits — round 10's fix wave
(`ea210c9ba`, `7b8299db9`, `4fc8662f4`), the stage-N package refresh
(`b5c4020af`), the console terminal controls (`d5f4811aa`,
`8a3c6e146`) and the two commits that trimmed the refresh sweep back
(`5412a6992`, `868a991ab`).

**What the packets actually covered.** Both packets sit on
`8a3c6e146`'s two new helpers and the code they touch:

- **Packet 1 — the interactive run.** `forthInteractiveRun` in full
  (`packages/forth-core/programming/forth_fold.c:277-393`): the
  `preRunCopy[256]` snapshot and its cap invariant, the surface-repair
  block, the error arm, the unconditional REPL reopen. Four findings.
- **Packet 2 — the two refresh seams.**
  `_forthConsolePrepareOpeningRefresh` and `_forthConsoleRefreshAfterRun`
  (`packages/forth-core/keyboard.c:45-67`) and their call sites at
  `:1523` and `:3042-3050`. Two findings.

Reading that the refutation pass added on its own initiative: the whole
`aimBuffer` writer set reachable under a live capture (both key seams,
`processAimInput`, `addItemToBuffer`'s AIM arms, `forthCapInsertName`,
`items.c:669`, history recall, fold/TAM resume, the X seed); the capture
lifecycle (`forth_capture.c`, `forth_dict.c`); `forthXeqnDispatch` and
the run-generation seam; `screen.c`'s CM_AIM refresh block and the
`SCRUPD_*` bit definitions; `assign.c`'s USER-key store.

**Six findings, five verdict records.** The sixth — *"TAM prompt
destroyed by the reopen"* — reached no verifier of its own. It was
disposed of inside the shared-premise settlement written by the
`error path` verifier, which killed it on the *suspend* half of the
premise. That half is genuinely dead by construction (§6), so the
disposition stands; but it is a **second-hand** disposition and is
labelled as such rather than counted as an independent refutation.

**Refutation-only.** This leg ran no find phase. Its coverage IS those
six findings. Everything else in the 21-commit range has **no
out-of-family coverage this round** and rests on the in-family passes
alone: the refresh-sweep ruling's five non-Forth hunks (`868a991ab`), the
`.S` spill-region change (`b5636f6c9`), the FHIST reservation
(`7b8299db9`), round 10's cursor-tuple and HOME.3 fixes, and the entire
test corpus read as code.

**What the budget did not reach.** Nothing was driven on simulator or
hardware. **No screenshot was taken**, and the brief authorised one for
exactly the finding that survived. That is this leg's largest single gap:
the survivor's *consequence* half is settled by reading the paint guard,
not by looking at an LCD, and the escalation this report makes on top of
it (§3, "reaching input") is traced at the tip and **not executed**. The
one experiment that would settle it is named at the finding.

**Stale-ref trap, fifth consecutive round, 5/5 caught.** Every verifier
worktree spawned at `e21af8d28` and every one executed the first-action
rule (`git log --oneline -1`, checkout of `868a991ab`, detached) before
its first read. *Correction to the verifier record:* one verifier
described `e21af8d28` as "5 commits past the audited tip on a different
line". It is not — `git merge-base --is-ancestor e21af8d28 868a991ab`
succeeds; it is 5 commits **behind** on the same line. The checkout was
still correct and the verdict is unaffected, but the trap's shape is
worth keeping accurate: worktrees spawn behind, not sideways.

---

## 2. Mechanical results

**No mechanical half of its own** — same tip, no code changed, so the
in-family §2 for this range stands.

One verifier ran the full gate at `868a991ab` as the baseline for its
mutations: `./packages/forth-core/build-test.sh` exit 0, refresh clean,
`FORTH SELF-TEST: ALL PASSED`, upstream `meson test testSuite` OK. Three
mutations, each applied, observed and reverted inside its own step:

| # | mutation | result |
|---|---|---|
| A | move `_forthConsoleRefreshAfterRun`'s `return` inside the `if(forthTestSuppressConsoleRefresh)` arm — the finder's implied remedy | **EXIT 139, segfault**, immediately after `[9] PASS (R8-P1)` |
| B | delete the suppression clear at `keyboard.c:3049` | GREEN — a real, small coverage gap (§6) |
| C | delete the `_forthConsoleRefreshAfterRun()` call at `keyboard.c:3050` | **RED**: `FAIL: R/S must immediately request one unsuppressed full repaint (requests=0 mode=0)` |

Mutation A is the load-bearing result of the whole leg: it proves by
experiment that other terminal tests do reach that seam with a live
capture and no arming, and that handing them a real `refreshScreen(142)`
kills the harness — which is precisely what the banner at
`keyboard.c:21-25` says. The finding that called the placement a defect
was disproved by installing its own fix.

**Tree state at synthesis.** `git status --porcelain` empty, HEAD
`868a991ab`. All verifier worktrees clean at the audited tip, including
the one that ran three mutations (patches regenerated via
`tools/pkg_patch_refresh.py`, the `src/generated/constantsVerification.txt`
build side-effect restored).

---

## 3. CONFIRMED findings, worst first

One. It is a wrong-result-class defect, not a crash — there are no
crashes this round — and by the ranking rule it would sit below any
crash on a common gesture. It is here because the gesture it breaks is
the one a daily Forth user builds for themselves.

### R11-1 — the opening refresh is prepared at the softkey door only, so opening the console from an assigned key leaves the transcript unpainted

`packages/forth-core/keyboard.c:1523` (the seam that exists),
`keyboard.c:2458` / `:2508` (the door that has none) — wrong result,
**high** confidence on reachability, **medium** on the escalated
frequency. Reachability lens; constructed, not executed.

**What breaks.** `fnForthOuter` opens the AIM surface, opens the
interactive capture, and appends `FORTH_CONSOLE_CONTROL_HINT` to the
console ring. The refresh that owns the resulting frame is
`refreshScreen(117)` at the end of `btnReleased`. With
`SCRUPD_MANUAL_STACK` (or `SCRUPD_SKIP_STACK_ONE_TIME`) set,
`_refreshNormalScreen`'s guarded block at `screen.c:5946` is skipped
entirely — and that block is the one holding both
`if(_forthConsoleActive()) { _forthConsoleRender(); }` (`screen.c:5963`)
and `refreshRegisterLine(REGISTER_X)` (`screen.c:5971`). The console is
open and already holds its hint; the transcript band still shows the
previous stack. `_selectiveClearScreen` skips the band too, so nothing
even blanks it. The repair that would have covered the
`CM_NORMAL`→`CM_AIM` transition is commented out at
`screen.c:6240-6246`.

**Reaching input.** ITM_FORTH (item 2842, `items.c:4800`) is an ordinary
`CAT_FNCT` row with `US_ENABLED` and **no softmenu row anywhere** —
`grep ITM_FORTH packages/forth-core/softmenus.c` is empty. Its only menu
surface is the FCNS catalog, which is a softmenu and therefore goes
through `executeFunction` → the seam at `:1523`. The covered door is the
catalog. The uncovered door is the key you assign it to:

1. ASSIGN → FCNS → FORTH. `executeFunction` stores the catalog pick
   verbatim (`keyboard.c:1495`, no CAT or flag filter);
   `_typeOfFunction(ITM_FORTH)` falls to `default: return 4`
   (`assign.c:924`), and `assignToKey`'s `default:` arm at keyStateCode 0
   writes `key->primary = tmpMenuItem.item` (`assign.c:1054`) into
   `kbd_usr + keyCode`.
2. USER mode on. `determineItem` reads the user keyboard unconditionally
   (`keyboard.c:1691`).
3. Press the key. `btnPressed` → `processKeyAction(ITM_FORTH)`
   (`:2122`) → outer `default:` → `case CM_NORMAL:` matches no arm →
   `keyActionProcessed == false` → `showFunctionName` → on release,
   `btnReleased`'s final `else` → `runFunction(ITM_FORTH)` (`:2458`) →
   `fnForthOuter`. `RELEASE_END`'s `default:` arm clears the stack bits
   only under `PROBMENU` (`:2493`), then `refreshScreen(117)` (`:2508`).
   `_forthConsolePrepareOpeningRefresh` is not on this path.

**Escalation — this is where the report parts company with both the
finder and the verifier, and it is traced, not executed.** The finder
named a preceding menu as the carrier of the suppression bit; the
verifier correctly refuted that (`menuUp`/`menuDown` set only
`SCRUPD_SKIP_STACK_ONE_TIME`, cleared at `keyboard.c:2511` at the end of
that same key's release) and substituted two real carriers: the sticky
CLLCD/PIXEL/POINT/AGRAPH family, and `RETURN_NORMAL` at
`screen.c:6073`. The second carrier is stronger than the verifier
allowed. `RETURN_NORMAL` ORs `SCRUPD_MANUAL_STACK` as the **last act of
every normal refresh**; `SCRUPD_ONE_TIME_FLAGS` is `0xf0` and
`SCRUPD_MANUAL_STACK` is `0x02` (`src/c47/defines.h:2029,2036`), so
`keyboard.c:2511` does not clear it; `refreshScreen`'s own entry
(`screen.c:6101-6118`) does not clear it; and on the ITM_FORTH press
nothing else does either — `processKeyAction`'s early resets at `:2556`
and `:2574` are gated on `lastErrorCode` and `temporaryInformation`,
neither of which holds, and no arm of the `CM_NORMAL` switch matches
ITM_FORTH. The ordinary state entering any keypress is therefore
**set**, and CLLCD is merely the case where it is guaranteed by design
rather than by default.

The package's own code is the best corroboration: the three other
console-touching seams each clear exactly this pair immediately before
repainting — the softkey open (`:1523`), R/S (`:3049`), and *every*
ordinary AIM keystroke (`:3067`). Those clears would be dead code if the
bits were normally clear. The physical open is the one seam of four with
no clear.

**Duration.** Not one video frame. The stale screen persists until the
owner's **next key event**, because the next CM_AIM keystroke hits
`:3067`, clears the bits and repaints. The observed behaviour is: press
the assigned FORTH key, watch the softkey row change to the Forth menu
and the input line blank (`clearRegisterLine(AIM_REGISTER_LINE, …)` runs
directly, outside the guard), and see the old stack still sitting where
the transcript should be — until you type something.

**Contract violated.** The helper's own banner, `keyboard.c:59-62`:

> The physical FORTH command opens an AIM editor AND adds the console's
> control hint above it. The caller's full refresh owns that screen, but
> can legitimately inherit a one-shot stack suppression from the
> preceding menu.

The banner says *physical*. The helper is called only from
`executeFunction`, which owns the softkey and the catalog — never the
physical key. `test_console.part.h:1713` records the same misreading in
the test's own words ("executeFunction owns the actual physical key and
its generic refresh"). `executeFunction` owns the softkey; `btnReleased`
owns the physical key.

**Bug class.** *One-door seam* — a guard or preparation installed at one
of the two dispatch doors into the same command. The class is enumerable
and the package already enumerates it correctly elsewhere: the capture
cap is installed at **both** doors and says so, in those words —
`keyboard.c:1515-1519` ("Seam 2: the softkey path. Seam 1
(processKeyAction) guards the physical-key path … so the cap must be
re-checked at this second entry point too") and `:3055-3056` ("Seam 1:
the physical-key path"). The opening refresh landed at Seam 2 only.

**Class-level test that would pin it.** A two-row table over the console
entry doors, driven with `SCRUPD_MANUAL_STACK` pre-set, asserting
`screenUpdatingMode & (SCRUPD_MANUAL_STACK | SCRUPD_SKIP_STACK_ONE_TIME)
== 0` at the moment the capture goes live — the same assertion shape
`test_console.part.h:1783-1789` already uses for the R/S seam, and the
same exported test hook (`forthTestConsolePrepareOpeningRefresh`) already
in place. Row 1 (softkey/catalog) passes today; row 2 (physical
`btnReleased` → `runFunction`) is missing and would fail. The assertion
must be on the flag word at the seam, **not** on pixels: mutation A
proved `refreshScreen` segfaults this harness.

**What would settle the escalation.** A run-sim screenshot of the frame
after the assigned-key press, with the capture driver copy-adapted from
`run-sim`'s `references/capture-driver.c`. That is the one experiment
this leg owed and did not run. If the frame turns out to be repaired by
something not on the traced path, the finding degrades from "every
assigned-key open" to "after CLLCD" and stays confirmed at lower cost;
it does not disappear, because the `:1523` seam demonstrably does not run
on that path.

**SETTLED THE SAME DAY — the escalation holds, executed.** The driver was
copy-adapted from `run-sim`'s `references/console-capture-driver.c` (the
console variant; its `_TLC_FULL_SHOT` machinery, save/restore and marker
scaffolding kept verbatim, fixture replaced) and run headless. Three
frames, one deliberate deviation from run-sim rule 5 recorded in the
driver's own banner: shots A and B must NOT force `SCRUPD_AUTO`, because
the suppression bit is the subject — forcing it away would capture the
absence of the finding. Nothing clears the screen before the refresh
either, since the claim is that the stale band survives the frame.

- **Shot 0** — ordinary normal screen under `SCRUPD_AUTO`, so a blank
  capture stays distinguishable from a suppressed one. Band ink 248 px.
- **Shot A, the assigned-key door** — `screenUpdatingMode |=
  SCRUPD_MANUAL_STACK` (what `RETURN_NORMAL` leaves), then
  `fnForthOuter`, then the frame. The log confirms the open really
  happened: `console live=1, ring holds 1 record(s)`. Band ink **248 px,
  byte-for-byte the pre-open value**: no control hint, no input line, and
  the previous frame's stack digits still on the right.
- **Shot B, the softkey door** — identical state and identical open, plus
  the single line `:1523` contributes
  (`screenUpdatingMode &= ~(SCRUPD_MANUAL_STACK | SCRUPD_SKIP_STACK_ONE_TIME)`).
  Band ink **322 px**, and `ENTER=SPACE  R/S=RUN` is legible on the glass.

Same open, same ring, same suppression inherited; the only difference is
the clear the key door does not perform. The consequence half is now
executed evidence rather than a trace, and the escalation stands as
written: the bit is set on the ordinary path, not only after CLLCD. The
driver was removed by its markers, `git diff --stat` on both touched
files is empty, `files/` was regenerated from the clean working area, the
BMPs are deleted, and the full gate is green.

---

## 4. PLAUSIBLE findings

**None.** Every refuted finding died on a constructed trace or an
executed mutation, not on "nobody could reproduce it".

Two near-misses are named here so they are not mistaken for silence, and
both live in §6 rather than in this section on purpose:

- **The uncovered `mode` clause at `test_console.part.h:1784-1785`**
  (mutation B: deleting the clear at `keyboard.c:3049` leaves the gate
  GREEN). Real, executed, and *not* the finding it arrived attached to —
  it is a test-coverage residual, not a defect in the code under test.
- **The `indexOfItems[negative]` read in `processAimInput`
  (`keyboard.c:656`).** Real out-of-bounds read, reachable, and
  **byte-identical to upstream** at `src/c47/keyboard.c:543`. Not this
  audit's finding; see §6.

---

## 5. Design observations (D7)

**O-a — the package knows this keyboard has two doors, and says so, in
the one place it got right.** `_forthCapAtCap` is installed at both
seams with a comment at each naming the other.
`_forthConsolePrepareOpeningRefresh` is installed at one, and its comment
names the door it is *not* on. The difference is not knowledge; it is
that the cap was written as a *rule about a surface* and the refresh
preparation was written as a *fix for a site*. Every future guard on this
keyboard has the same two-door question, and the cheapest place to answer
it once is a named list of the console's entry doors rather than a
comment per site.

**O-b — three of six findings clustered on one premise, the premise is
TRUE on exactly one narrow route, and nothing at the seam says so.** A
run *can* leave the capture non-OPEN: an interactive line that `XEQ`s a
global label reaches `forthXeqnDispatch` (`forth_inner.c:388-397`) →
`fnExecute` → `runProgram`, whose
`if(!nestedEngine) { … forthRunGenBump(); }` (`lblGtoXeq.c:676`) sets
`forthResetPending` because `forthDepth == 0` at outer-interpret time
(`forth_compile.c:153-158`); if that program contains an `ITM_FORTH`
source step, `forthScopeEnterProgramStep` (`forth_compile.c:1853`) →
`forthRunGenCheckReset` (`:228-235`) → `forthDictClear`
(`forth_dict.c:61-73`) → `forthCapPowerReset` → `forthCapClose`.
DESIGN.md §9.3 rules the consequence deliberate and §8.4.2 rules the
resulting state "a correct outcome, not a defect" — so all three findings
still fall. But `forth_fold.c:384`'s comment explains the *reopen* and
`:343-360`'s explains the *line restore*, and neither mentions the one
case in which the capture arriving there is already closed. One sentence
at either seam naming the run-generation route would have refuted three
of these six findings before they were written. That is the audit's own
rule 1 ("decisions the code already explains") applied to the one
decision this file does not explain.

**O-c — the three verifiers disagreed about that premise, and the
disagreement is diagnostic.** One called the route real and exotic; one
called the suspend half impossible and the close half ruled; one named
`forthRunGenCheckReset`'s `!forthInnerIsActive()` guard as blocking it,
which it does not — `forthDepth` is 0 for a line interpreted at the outer
level, which is exactly when `forthRunGenBump` arms the reset. Three
independent readers, three answers, one seam. Where three careful readers
of the same twenty lines cannot agree whether a path exists, the code is
under-documented at that point regardless of who was right.

**O-d — a reader with no repository gets the structure right and the
carriers wrong, and this round reproduces round 10's ratio.** Six
findings, one survivor, and the survivor is again a structural question
answerable from the quoted code alone ("this preparation is called from
one place; the command has two"). All five refuted died on a mechanism
the reader had to guess: that a negative item id can insert text, that a
menu leaves a sticky suppression bit, that TAM can be entered from inside
a run, that a test asserts a real refresh. Even the survivor arrived with
the wrong carrier for its own suppression bit. The way to spend this
reader family is on the structural question — *what does this contract
promise, and where is it actually enforced?* — and to treat every call
path it states as a hypothesis for the verifier. That is what the
workflow already does, and it is why one finding of six is worth the
round.

**O-e — the `preRunCopy` arithmetic is exactly tight, and nothing says
so.** The cap admits when `bytes + nameLen < 256`, so a line is at most
255 bytes; `xcopy(preRunCopy, aimBuffer, n + 1)` therefore writes at most
256 into `char preRunCopy[256]`. Correct, with zero margin, and the
correctness depends on `addItemToBuffer` appending exactly the string the
cap measured. `AIM_BUFFER_LENGTH` is 1024 (`src/c47/defines.h:737`), so
upstream's own `displayBugScreen` backstop is **not** what saves this
copy — the Forth cap is. The banner at `forth_fold.c:308-311` states the
invariant and not the margin. This is not a finding; it is the reason a
finding was plausible enough to write.

---

## 6. Deliberately not flagged

The finders supplied no cleared-item census of their own (the
per-dimension coverage list was empty), so every entry below is either
something a verifier disproved at the tip or something this synthesis saw
and left.

### The five refutations

**1. `preRunCopy[256]` overflow via unmetered non-positive item ids
(`forth_fold.c:312`).** *Premise true, conclusion false.*
`_forthCapAtCap` really does return `false` for `item <= 0`, exactly as
quoted (`keyboard.c:81`). That only matters if some non-positive id can
**append text** to `aimBuffer` while a capture is live, and none can. On
the softkey seam, negative ids are consumed far upstream at
`keyboard.c:1168`'s `else if(item < 0) { // softmenu }`, every arm of
which returns; seam 2 at `:1520` sits inside `else if(item > 0)` at
`:1373` and is never reached with a negative id at all. The one
`addItemToBuffer` inside the negative block requires `tam.mode == TM_MENU`
and flips the id positive first. `item == 0` is excluded by the enclosing
`if(item != 0)`, and `indexOfItems[0].func` is `itemToBeCoded`. On the
physical seam a negative id does fall through into `processAimInput`, but
every insertion arm there is a positive-constant compare:
`keyReplacements` matches only positive `kbd_std[].primaryAim` /
`gShiftedAim` entries and leaves `*item1 == 0`; `caseReplacements` tests
only ITM_A..ITM_Z / ITM_a..ITM_z; the COLON/COMMA/QUESTION/SPACE/
UNDERSCORE arm is explicit. Every other writer is separately bounded: the
metered seams admit only below 256 and `addItemToBuffer` appends exactly
the string the cap measured (`bufferize.c:613-624`, `:648`);
`forthCapInsertName` carries the same cap (`forth_menu.c:26-44`);
`items.c:669` is that same call; history recall and fold resume copy a
`uint8_t` payload length, max 255 (`forth_fold.c:836-846`, `:185-187`);
the X seed refuses oversize and opens no capture. **The invariant at
`forth_fold.c:308-311` survives because non-positive ids are not
insertion sites.**

**2. The unconditional `forthCapOpenInteractive` strands a capture the
line closed (`forth_fold.c:384`).** *Premise true on one route,
conclusion false, and the cited invariant misread.* The route is O-b's.
What the code then does is not what the finding describes:
`forthCapClose` touches capture fields plus `forthConsoleUnstampAll`
(which clears `userMenuId` and leaves the `-MNU_FORTH` frame in place),
and `forthCapPowerReset` explicitly does not touch `calcMode` or
`FLAG_ALPHA` — so `calcMode` is still `CM_AIM`. Line `:384` reopens,
`:385` relocks keys mode, `:391-392` zero the cursor, and
`_forthConsoleActive()` (`forth_console_view.c:123-131`) is satisfied:
the owner is looking at an open, empty Forth console with the Forth row
on screen. Nothing "appears closed", and the capture cannot escape into a
later native alpha session — `forthConsoleBaseOnTop()` falls back to
`currentMenu() == -MNU_FORTH` for exactly this unregistered state
(`forth_menu.c:416-420`), so rung 1 does not fire, keys mode is true so
rung 2 does not, and rung 3 closes normally. The only residue is
cosmetic: rung 3 leaves the Forth row on the stack. And the comment the
finding cites as violated is the ruling it breaks — `forth_fold.c:279`
("Empty R/S is a no-op, NOT a close: EXIT is the close gesture") and
DESIGN.md L-R3 ("reopens empty with keys mode relocked") both **mandate**
the unconditional reopen; making `:384` conditional would leave the owner
in CM_AIM with no capture, which is the actual stuck state.

**3. The error path restores the line but not the capture state
(`forth_fold.c:343`).** *Both halves ruled.* The suspend half is dead by
construction: `forthCapSuspendState` has one production caller
(`forthCaptureSuspend`, `forth_fold.c:54`), which has one
(`ui/tam.c:1216`, inside `tamEnterMode`), and Forth dispatches items
exclusively through `reallyRunFunction` — every site in
`programming/param_core.c` and `forth_compile.c:1103`, with no bare
`runFunction(` anywhere in the Forth dispatch files — so `items.c:765`'s
`tam.mode == 0 && TM_VALUE <= param <= TM_CMP` gate is never reached from
inside a run. **Synthesis correction:** the close half is *not*
structurally impossible, as O-b establishes; the error arm returns at
`:360` before `:384`, so on that one route the function does exit with
the capture CLOSED, `calcMode == CM_AIM`, and the line restored from
`preRunCopy`. That state is ruled, in the finding's own terms: DESIGN.md
§8.4.2's Restore paragraph — *"the line is in `aimBuffer`, no capture
behaviour attached. **That is a correct outcome, not a defect**"* — and
§8.4.2's close-path table names `forthCapPowerReset()` as one of the
seven interactive close paths that drops the line at the dictionary
seams. The LIVE gate the finding calls a violated contract is itself the
ruling (N-R10, echoed at `keyboard.c:3677-3681`), and D7-2 supplies the
recovery gesture the finding assumes missing (`forth_console.c:250-258`).
Refuted on intent rather than on reachability — a distinction that
matters if the owner ever revisits §9.3, because the same ruled path also
clears the console ring mid-line via `forthConsoleClear()`, so the
dialogue the owner was reading disappears before the error message is
appended to it.

**4. TAM prompt destroyed by the reopen.** *No independent verdict; the
suspend half is dead.* This finding requires a TAM prompt to be open when
`forth_fold.c:384` runs. Entry 3 establishes that nothing a run executes
can reach `tamEnterMode`, and a capture that is already suspended is not
`forthCapInteractiveLive()`, so the R/S arm at `keyboard.c:3042` never
calls `forthInteractiveRun` in the first place. Both preconditions fail
independently. Recorded here rather than in §3 with the note that it was
never sent to a verifier of its own — the disposition is sound, and it is
one reader deep instead of two.

**5. The self-test suppression makes the live-capture refresh untestable
and falls through when the capture is not live (`keyboard.c:45`).** *Both
halves: premise true, conclusion false; disproved by experiment.* Half
(i)'s premise is literally correct — the `return` at `:52` sits outside
the arm test, so a `FORTH_DEBUG_SELFTEST` build never calls
`refreshScreen` while the capture is live, armed or not — but it reads
the banner backwards. Sentences 1-2 of `keyboard.c:21-25` *are* the
rationale for suppressing on the live branch regardless of arming, and
"normal firmware always falls through" names the non-SELFTEST build where
the whole block compiles out. Installing the finding's implied remedy
segfaults the gate (mutation A). The coverage claim is false in the other
direction too: deleting the refresh request turns the gate RED with the
exact assertion the finding says does not exist (mutation C), and the
console's real painting is separately covered by `_forthConsoleRender`
plus a pixel count in the same test (`test_console.part.h:1731-1738`,
`:1758-1796`). Half (ii) fails on reachability: the sole caller sits
inside `if(forthCapInteractiveLive() && item == ITM_RS)` and every exit
of `forthInteractiveRun` leaves the capture live — including, per O-b's
route, because the closed case returns at `:360` before control ever
comes back to the R/S seam with a non-live capture in a state the seam
can observe. The empirical clincher is mutation A: if any test reached
the not-live fall-through, the unmutated baseline would already crash
there, and it is green. The crash mechanism is real and demonstrated; the
path to it is not, and no specific dereference inside `refreshScreen` was
identified, so the finding's own citation-only version of the crash claim
is not credited either.

### Residuals seen and deliberately left

**The untested `mode` clause (mutation B).** Removing the suppression
clear at `keyboard.c:3049` leaves the gate GREEN, so the `mode` half of
`test_console.part.h:1784-1785` is not load-bearing — the bits are
already clear when the seam samples them, because `processKeyAction` has
earlier resets. The `count` half **is** load-bearing (mutation C). A
genuine coverage gap in one clause of one assertion, test-only, and not
the defect the finding attributed it to. **Left alone deliberately:** the
clear at `:3049` is correct and cheap, and a test that pins a redundant
clear pins the wrong thing. If it is ever worth pinning, the right shape
is R11-1's two-row door table, which covers this clause as a side effect.

**`indexOfItems[negative]` in `processAimInput` (`keyboard.c:656`).**
Real out-of-bounds read on a negative item id in CM_AIM, reachable, and
**byte-identical to upstream** (`src/c47/keyboard.c:543`). The package's
own `_forthCapAtCap` comment (`keyboard.c:83-86`) already names the
hazard as the reason its own `item > 0` test is load-bearing — that test
protects the package's read, not upstream's downstream one. Not a package
finding: the package neither introduced it nor made it more reachable,
and CODE_AUDIT.md's rule 6 keeps upstream's pre-existing hazards out of
this report. It belongs in an `UPSTREAM_REPORTS_*.md`, alongside the
`getGlyphBounds` partial-write report the tip commit already filed.

**The error arm sets `T_cursorPos` to the line end and leaves
`displayAIMbufferoffset` alone, where the success arm zeroes both
(`forth_fold.c:358-359` vs `:391-392`).** Asymmetric, and correct: the
error arm restores the *same* line the owner typed, so the pre-run
horizontal offset is still the right one for it, and it parks the cursor
at the end on purpose ("so the user edits rather than retypes"). The
success arm reopens an *empty* line, where 0/0 is the only consistent
pair. Considered and cleared.

**`forthConsoleUnstampAll` leaving the `-MNU_FORTH` frame behind on the
run-generation close.** Cosmetic residue on an already-ruled path (entry
2): EXIT still closes correctly through the `currentMenu()` fallback; the
row is simply not popped. Left alone — the fallback exists for exactly
this unregistered state and is documented as such.

**The `NOPARAM` / `NOT_CONFIRMED` gap at `forth_inner.c:430`.** Noted by
one verifier while enumerating the callable set: Forth passes `NOPARAM`
(9876) where `NOT_CONFIRMED` is 9878, so a confirmation-gated item
reached this way would take its CONFIRMED branch. Cleared, because the
callable filter is `forthFindItem` (`forth_dict.c:602-614`), which admits
only `CAT_FNCT && (status & PTP_STATUS) == PTP_NONE`, and both
confirmation-gated items in question (`ITM_RESET` at `items.c:3420`,
LOAD at `:3361`) are `PTP_DISABLED`. Nothing follows today. Recorded
because the clearance depends on a filter in a different file from the
call, which is the shape that goes wrong when either side moves.

### If the goal were correct code rather than a passing audit

**Fix R11-1.** It is a real user-visible defect on the gesture a heavy
user builds for themselves, the class is enumerable, and the package
already contains the correct pattern one function away.

**Leave everything else in this section.** Specifically: the mutation-B
clause is redundant coverage of a redundant clear; the `processAimInput`
read is upstream's; the cursor asymmetry is deliberate and explained; the
unpopped frame is covered by a documented fallback; the `NOPARAM` gap has
no admitted item. The one change worth making that is *not* a fix is
documentation — O-b's missing sentence at `forth_fold.c:343` / `:384`,
which would have pre-refuted three of these six findings and cost two
lines.

---

## 7. Verdict

**Yes, ship it.** Nothing found this round threatens data, the
dictionary, or the owner's programs. The single confirmed defect is a
stale opening frame that self-repairs on the next keystroke, on a door
most users will never open because it requires ASSIGN plus USER mode.

**Where it breaks first:** at the keyboard's two-door seam. Every guard
this package has added to the key surface has had to be installed twice;
the cap got it right and said so, and the newest one did not. The next
console-touching helper faces the same choice, and the door that gets
forgotten is always the physical one — because the softkey path is the
one the harness can drive (`FORTH_SELFTEST_EXPORT void executeFunction`),
and what the harness can drive is what gets written. That is the
structural risk worth watching: the test surface is shaping which door
the production code defends.

**Second, and further out:** the run-generation close (O-b). It is ruled,
it is correct, and three careful readers could not agree it exists. A
route that only the design document knows about is one refactor away from
being a real finding.

---

## 8. Round and exit state

**Round 11**, out-of-family refutation-only leg, at `868a991ab`.
Reader: **Gemini 3.1 Pro**, two self-contained packets, no repository.
Verifiers: five independent worktrees, one per finding, named lenses
(reachability ×2, intent, correctness, coverage-by-mutation); the sixth
finding was disposed of second-hand inside a sibling's shared-premise
settlement. Three executed mutations, one deliberately RED, all reverted.

**Exit criterion NOT met, and the count resets.** R11-1 is a new
CONFIRMED finding, so by CODE_AUDIT.md's exit rule the
two-consecutive-clean-round counter returns to zero — and its fix will be
new code that nobody has audited. Round 12 is the earliest round that can
begin the count, and it must cover R11-1's fix; the earliest close is
round 13, with at least one of the two clean rounds out-of-family.

**Owed into round 12, from this leg's own gaps:**

1. **The screenshot this round did not take.** R11-1's consequence half
   is read, not seen, and the escalation to "the ordinary state entering
   any keypress" is traced, not executed. A run-sim capture of the
   assigned-key open settles both.
2. **Out-of-family coverage of the rest of the range.** Fourteen of the
   21 commits since `9230d36b2` were never in a packet.
3. **The sixth finding's independent verdict**, if the owner wants that
   record two readers deep rather than one.
