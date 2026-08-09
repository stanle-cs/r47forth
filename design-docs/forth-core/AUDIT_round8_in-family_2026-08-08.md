# Audit — the round-8 fix wave, in-family pass, at `db2186cf1`

Range `28eb24b93..db2186cf1` — P-1's sweep re-anchor, C-1's shared fold
re-derivation, the C-2 + OOF-1 `keyboardTweak.c` override, C-3's
render-time view clamp, the P-2 family's `tamEnterMode` refusal and its
selftest fault-injection hook, and the C-4..C-7 record/pin changes.

*File-name note (second round running).* The orchestration slug for this
subject is 267 bytes and the filesystem name limit is 255; this file
carries the series convention, matching
`AUDIT_round7_out-of-family_2026-08-08.md`. The slug is not the subject.

*Companion file.* `AUDIT_round8_2026-08-08.md` carries the round's
opening note, its coverage header and the process/growth-rule section
(the seventh packet-defect class, the MODEL check, the deleted
unreachable subcase). This file is the full eight-section report for the
in-family pass and does not duplicate that material.

*The one fact that reorders everything below.* The out-of-family reader
(Gemini 3.1 Pro) found the P-1 re-anchor's stale-offset root first, and
it was fixed in `58e07a2bd` — **after** the tip this pass audits. Three
of the eight in-family dimensions found the same root independently, two
of them by execution, and a fourth dimension's version of it was
REFUTED. Those findings were real at `db2186cf1` and are closed at
`HEAD`; they are reported as R8-10 at the end of §3, ranked last because
a closed defect costs the owner nothing, and the convergence is the most
useful thing this round produced (§5, §8).

---

## 1. Subject and coverage

**Commits.** `28eb24b93..db2186cf1`, five commits, all 2026-08-08:
`a96350e20` (P-1 + C-1), `0aaf53393` (C-2 + OOF-1, the new package
override of `c47Extensions/keyboardTweak.c`), `d4a204d66` (C-3 and the
P-2 family, both owner rulings), `1212e4efb` (C-4..C-7, the record and
the pins), `db2186cf1` (DESIGN-HISTORY). Every verifier worktree again
spawned STALE at `c3a00768c` (~114 behind) and every one executed the
round-6 rule — `git log --oneline -1`, then checkout to the audited tip —
before its first read. Eleven for eleven; the rule has now paid for
itself three rounds running.

**Files read.** `programming/manage.c` (the re-anchored sweep and its
canary, `forthFoldEnter`/`Leave`/`UnwindIfDone`/`RederiveAdmission`,
`_forthFoldAdmits`, `_forthFoldFindCaptureStep`, suspend/resume including
the rev-3 recovery arm, the FHIST helpers, `forthHistoryEnsure` and its
injection hook, `_insertInProgram`); `ui/tam.c` (`tamEnterMode` end to
end including the mode-normalisation switch, both `tam.function` rewrite
arms, `_tamLeave`, the D7-1 wrapper, `tamProcessInput`'s bracket, the
`dddVEL` and `M_GOTO_ROW` leave-then-dispatch sites, the TM_LBLONLY
commit block); `c47Extensions/keyboardTweak.c` (the new override,
`openHOMEorMyM` in full, and a grep of every menu-stack call in the
file); `screen.c` (`_forthConsoleMaxView`/`ClampView`/`Render`/`Active`,
the renderer call site, the round-6 F7 twin and the long-press ladder);
`forth_menu.c`, `forth_capture.c/.h`; `test_capture.part.h`
(`test_fold_round8_window` [1]–[5] and the round-6 [7] guard rewrite),
`test_console.part.h` (the clamp fixture); `design-audit.sh` group I in
full. Upstream read where the package's cached values are consumed:
`programming/manage.c` (`fnClP`, `_clearProgram`, `deleteStepsFromTo`,
`defineCurrentProgramFrom*`, `getNumberOfSteps`, `scanLabelsAndPrograms`),
`programming/lblGtoXeq.c` (`goToPgmStep`, `goToGlobalStep`),
`programming/nextStep.c`, `softmenus.c` (`dynmenuGetLabelWithDup`, the
TAM menu tables), `keyboard.c` (`determineFunctionKeyItem_C47`,
`determineItem`), `items.c`, `assign.c`, `error.c`, `calcMode.c`.

**Readers.** Eight in-family dimension finders (contracts, lifecycle,
arithmetic, errorpaths, guards, tests, design, upstream), blind to each
other, via `audit-workflow.js`; twenty-one findings into the three-lens
refutation pass (reachability / correctness / intent), each verifier in
its own worktree; sixteen survived, five were refuted. After merging
cross-dimension duplicates — four findings were one root, and two further
pairs were the same pin — this report carries **nine CONFIRMED open, one
CONFIRMED closed at HEAD, and one PLAUSIBLE**. Sixteen of the twenty-one
verification runs applied a mutation or an instrumented probe; all were
reverted and every verifier reported a clean tree at finish.

**Deliberately not audited.** The standing open findings C5, C6, C7, C10,
C11, C13, C14, C15, C20, C22; the ruled items P1 (round 3), P2 (the push
ruling), F13/U5; rounds 6 and 7's refuted items. None re-reported. **The
two out-of-family fixes `58e07a2bd` and the comment commit `bdbfffeb1`
are outside the audited range** and were read only to determine whether
they close what this pass found; they are covered by their own class
tests and are round 9's explicit subject.

**What the budget did not reach.** DESIGN.md end to end (every reader
consulted it at its anchors; a contradiction living only in an unquoted
section would have been missed); the ~1500 lines of the new
`keyboardTweak.c` override outside `openHOMEorMyM` and `fg_processing_jm`
(the refresh tool and the gate own the faithfulness of the copy, and the
generated patch was read); `forth_console.c` ring append/eviction
internals; `packet_lint.py`'s D7-a rule beyond reading it;
`test_dict_reloc.c`'s registration diff; `keyboard.c` outside the F8
conjunct, the EXIT ladder and the two `tamEnterMode` call sites. **The
finders performed no dynamic execution** — every finding entered
refutation as a static trace; six of the thirteen were then executed by
their verifiers, and each says which.

---

## 2. Mechanical results

**At the audited tip `db2186cf1`.** Six verifier worktrees independently
ran `./packages/forth-core/build-test.sh` at a clean checkout of the tip
before mutating anything: all six GREEN — `FORTH SELF-TEST: ALL PASSED`,
self-test exit 0, upstream `meson test testSuite` 1/1 OK. Two verifiers
re-ran the gate after reverting their mutations and got GREEN again, so
the tip's greenness is measured, not inherited from the commit messages.

**At `HEAD` (`bdbfffeb1`), measured for this report.** Gate GREEN —
`FORTH SELF-TEST: ALL PASSED`, upstream testSuite 1/1 Ok, `==> BUILD +
SELF-TEST GREEN`, exit 0 — and the refresh the gate performs first left
the tree clean, so the silent-green trap stays closed and group F is
confirmed independently. `design-audit.sh` reports **4 finding groups**,
the same four as round 7
and all chronic: A (override files 18, budget 16; added lines 2574,
budget 606), B (hunks whose added lines never mention Forth 29, baseline
3 — still saturated by the D7-1 rename's one-line swaps), D (contiguous
inline blocks 16 → 37), E (the standing allocation-lifetime prompt in
`forth_dict.c`/`forth_inner.c`). F/G/H clean: generated output
synchronized with the manifest and clean in Git, no working-area file
would ship, all DESIGN.md citations resolve. Nothing the mechanical half
reports is counted as a finding below.

**Group I — the wave's own deliverable — reports all eight pins ok**
(3 / 2 / 2 / 2 / 2 / 2 / 5 / 0). That is the point of R8-3, R8-4 and
R8-5: three of those eight greens were mutation-proven this round to be
green through exactly the drift they were bought to catch, and one of the
three mutations left the **full gate** green as well.

**Warnings.** Baseline unchanged in kind.

---

## 3. CONFIRMED findings

Worst first, ranked by what each costs the owner. All survived the
three-lens refutation pass with the verifier granting or constructing the
reaching input. Cross-dimension duplicates are merged with both routes
named — a double-find is evidence, not repetition. Line numbers are given
at the audited tip and, where they moved, at `HEAD`. No patches.

---

### R8-1 — `forthFoldLeave`'s cursor restore consumes a program NUMBER cached across a PARK dispatch that deletes programs: wrong-program cursor in the ordinary case, out-of-bounds read and SIGSEGV at the boundary

**Found by** D3 arithmetic (reachability lens). **Verified by
execution**: both consequences reproduced in the real dispatch chain,
the boundary case as a segmentation fault under the gate.

- **Where.** `packages/forth-core/programming/manage.c:2182` at the
  audited tip, **`:2251` at HEAD — still open**:
  `goToPgmStep(forthFoldCtx.savedProgram, forthFoldCtx.savedLocalStep)`,
  consuming the number sampled at `:2028` (`:2093` at HEAD) before the
  dispatch. `goToPgmStep` does `programList[program - 1].step` with no
  bound at either end (`src/c47/programming/lblGtoXeq.c:155-158`), and
  `programList` is reallocated to exactly `numberOfPrograms` entries by
  every `scanLabelsAndPrograms`.
- **Reaching input.** Open the console with at least three programs in
  memory and the PEM cursor last parked in one that is not the first —
  the state after writing a program and pressing P/R out. Type any line,
  then **DELP → alpha → a program name that precedes the cursor's →
  ENTER**. DELP is fold-NON-admitted (`_forthFoldAdmits`), so the fold
  PARKs, and the commit dispatches `reallyRunFunction(ITM_DELP, value)`
  LIVE at `ui/tam.c:1147` **before** its `_tamLeave()` at `:1166`.
  `fnClP` deletes the program, `numberOfPrograms` drops, and fnClP
  renumbers *its own* saved cursor. Control then returns through
  `_tamLeave` → `forthFoldUnwindIfDone` → `forthFoldLeave`, which
  restores the *un*-renumbered copy. This is the wave's own subcase [1]
  drive with a different program named.
- **What the owner sees.** Ordinary case: the PEM cursor silently lands
  in a program they were not editing, at an arbitrary step — fnClP's
  correct restore is overwritten, no error, `lastErr=0`. Boundary case
  (the cursor's program was the last one, so `savedProgram ==
  numberOfPrograms + 1` after the delete): `programList[numberOfPrograms]`
  is an out-of-bounds read of the freshly reallocated arena. Executed, it
  returned `-2147483645`; `defineCurrentProgramFromGlobalStepNumber`
  narrows it, `currentProgramNumber` can reach 0, and `goToGlobalStep`
  walks the result with `while(true) { ... stepPointer =
  findNextStep(stepPointer); }` — no NULL guard, no end-of-program guard,
  no iteration cap. The verifier's run died at
  `Segmentation fault ... (exit 139)`. The typed console line is gone
  either way.
- **Why it is wrong.** The fold context's own contract defends the wrong
  half of the staleness (`manage.c:1993-2000`): *"forthFoldLeave restores
  via goToPgmStep, which re-reads `programList[program - 1]` AT RESTORE
  TIME, after scanLabelsAndPrograms has rebuilt it, so the base is
  current."* That reasoning covers the program's base ADDRESS moving; it
  does not cover the program's NUMBER changing or ceasing to exist, which
  is what a delete does. Upstream states the rule in code in the very
  function the PARK dispatch runs: `fnClP` does `if(programNumberToDelete
  < savedCurrentProgramNumber) { --savedCurrentProgramNumber; }`
  (`src/c47/programming/manage.c:350-355`) and `_clearProgram` clamps
  again at `:302-307`. `forthFoldCtx.savedProgram` is a third cache of
  the same quantity with neither guard. And the wave's own P-1 comment
  enumerates the hazard — *"BOTH numbers this block consumes were sampled
  in FHIST ... the cursor is NOT guaranteed to be in FHIST when we get
  here"* — then rules the third consumer out of scope in one clause:
  *"The cursor restore and the foldMode clear below still run."*
- **Bug class.** A number sampled across a dispatch that can invalidate
  it, consumed without a count check — the round's own named class, at
  the site the wave hardened, on the one consumer the fix exempted.
- **Class test.** Enumerate every quantity `forthFoldCtx` caches across
  the PARK dispatch (`savedProgram`, `savedLocalStep`,
  `savedFirstDisplayed`, `entryStepCount`, and the capture step) and
  assert at `forthFoldLeave` that each is still valid for the machine as
  it now stands — for `savedProgram`, `1 <= savedProgram <=
  numberOfPrograms`, and that the restore agrees with the renumbering
  `fnClP` performed. Concretely: three programs, cursor in the LAST one,
  DELP a program below it from the console; assert no fault and that the
  cursor is where fnClP put it. The sibling site `manage.c:1648`
  (`_forthHistCur.savedProgram`) caches the same quantity the same way
  and belongs in the same enumeration.

---

### R8-2 — `dynamicMenuItem` is latched by the softkey that commits the TAM, so `forthFoldLeave`'s cursor restore silently does not happen and the owner is parked inside FHIST

**Found by** D4 errorpaths (correctness lens). **Verified by
execution** in the real resolution layer; three of the finding's line
cites were wrong and every load-bearing claim was right.

- **Where.** `packages/forth-core/programming/manage.c:2182` (`:2251` at
  HEAD — **still open**), the same call as R8-1, failing for a second and
  independent reason. `goToGlobalStep` is not a "go to this step"
  primitive: with `dynamicMenuItem >= 0` it reinterprets the request as
  "go to the label the dynamic menu names" and **returns without
  navigating** when that name does not resolve
  (`packages/forth-core/programming/lblGtoXeq.c:102`, `:114-116`).
- **Reaching input.** Live console. Press **DELP** — ITM_DELP is
  TM_LBLONLY (`src/c47/items.c:3277`), so the TAM row is
  `MNU_TAMLBLONLY`, whose softkey 2 is `-MNU_PROG`
  (`src/c47/softmenus.c:859`). Press it, then press a **program-name
  softkey**. That press runs `case MNU_PROG:` in
  `determineFunctionKeyItem_C47` (`packages/forth-core/keyboard.c:123-133`),
  which sets `dynamicMenuItem = firstItem + itemShift + fn` and returns
  `MNU_DYNAMIC`; `_tamProcessInput`'s MNU_DYNAMIC arm consumes it via
  `forcedVar` and **nothing resets it** on the non-indirect commit path
  (the only reset, `ui/tam.c:982`, is inside the `tam.indirect &&
  calcMode != CM_PEM` sub-branch). The commit dispatches live, `_tamLeave`
  pops the TAM rows, and `forthFoldLeave` calls `goToPgmStep` with the
  latch still set. The same shape reaches the ARMED fold: `menu_TamSto`'s
  softkey 2 is `-MNU_VAR` and `keyboard.c:151-154` returns MNU_DYNAMIC
  unconditionally — console **STO** with the variable picked from the
  VARS row, which is the ordinary way to use STO.
- **What the owner sees.** The cursor restore does not happen. Because
  the round-8 hunk one screen above deliberately leaves
  `currentStep = cap; defineCurrentProgramFromCurrentStep();` pointing
  into FHIST, and then deletes `cap`, the owner is left with
  `currentProgramNumber` = FHIST and `currentLocalStepNumber` a stale
  count belonging to a different program. Executed: `prog=1` (FHIST) with
  `step=3` where `prog=2 step=3` was wanted, `lastErr=0`. The next entry
  into PEM lands inside the hidden history program at an inconsistent
  step number, and the two lines that follow
  (`firstDisplayedLocalStepNumber = savedFirstDisplayed;
  defineFirstDisplayedStep()`) then walk FHIST by a count that belongs
  elsewhere.
- **Why it is wrong.** The P-1 comment asserts the thing that does not
  happen: *"The cursor restore and the foldMode clear below still run."*
  The hazard is already known in this tree and neutralised at two other
  sites — upstream's own `_insertInProgram` brackets its reposition
  (`packages/forth-core/programming/manage.c:721/772/774`:
  `dynamicMenuItem = -1; goToGlobalStep(...); dynamicMenuItem =
  _dynamicMenuItem;`), and `forth_compile.c:1431-1437` does the same
  before `fnExecute` with the comment *"fnGoto's `dynamicMenuItem >= 0`
  branch reinterprets the label ID as a global step number"*. DESIGN.md
  §3.3.6 states the rule outright. `forthFoldLeave` is the third
  navigation and has no bracket. The battery cannot see it: `R8_RESET`
  (`test_capture.part.h:17663`) seeds `dynamicMenuItem = -1`, and subcase
  [1] types "FHIST" into `aimBuffer` instead of pressing the softkey.
- **Bug class.** A primitive whose meaning depends on a global that key
  handling latches and no caller clears — neutralised by enumeration at
  two of three sites.
- **Class test.** For every navigation the package performs during a key
  press (`forthFoldEnter`'s `forthHistoryGotoLastStep`, `forthFoldLeave`'s
  `goToPgmStep`, `_forthHistCur`'s restore at `manage.c:1648`), assert
  `dynamicMenuItem < 0` at the call or bracket it. Concretely: drive DELP
  with the program picked from the `MNU_PROG` softmenu — real
  `determineFunctionKeyItem_C47`, not a hand-set `aimBuffer` — and assert
  the cursor lands where it was parked. The fixture must stop seeding
  `dynamicMenuItem = -1` for that subcase, or the test cannot fail.

---

### R8-3 — group I's frame-destroyer pin counts two spellings at the start of a line, so a third destroyer can arrive, be unguarded, and leave the pin AND the full gate green

**Double-found:** D1 contracts (reachability lens) and D8 upstream
(intent lens), independently, at `design-audit.sh:447-448`. **Both
verifiers mutated it; one mutation left the entire gate green.**

- **Where.** `design-docs/forth-core/design-audit.sh:447-448` (same line
  at HEAD — **still open**). The count command is
  `grep -cE '^ +(popSoftmenu\(\)|fnExitAllMenus\()'` over
  `c47Extensions/keyboardTweak.c`.
- **Reaching input.** An upstream revision of `openHOMEorMyM` that
  destroys a frame by any other name or in any other shape. This is not
  speculative: `fnExitAllMenus` is one of at least seven frame-destroying
  entry points in the same tree — `fnLeaveTimerApp`, `fnPollTimerApp`,
  `_clearAlpha`, `fnDeleteMenu`, `fnProgrammableMenu`, `fnMenuDump` all
  reduce to `popSoftmenu()`, and `softmenus.h` additionally exports
  `removeMenuFromStack`, `removeUserMenuFromStack` and `changeToALPHA`.
  None is in the alternation. The `^ +` anchor is a second, independent
  hole: `if(cond) popSoftmenu();` is not matched either.
- **What the owner sees.** C-2's symptom returning silently instead of
  loudly: the live console's registered row destroyed by the long-press
  or triple-f gesture, the row reading ALPHA while the keypad types the
  keys plane, the ownership stamp gone and the C18 close accounting
  popping the wrong frames — with `design-audit.sh` reporting the pin ok
  and the reviewer, who has been told explicitly not to hold the list in
  their head, not looking.
- **Executed.** Verifier A inserted `fnLeaveTimerApp();` (upstream's own
  indentation, a real destroyer via `timer.c:806`) plus
  `if(getSystemFlag(FLAG_HOME_TRIPLE)) popSoftmenu();` into
  `openHOMEorMyM`: pin still `ok 2`, guards still `ok 2`, whole script
  still 4 finding groups — identical to baseline with two extra
  destroyers in the file. Verifier B did the same with
  `removeMenuFromStack(-MNU_FORTH)` in the MyM.3 arm: `ok 2`. Verifier A
  then placed a single evading destroyer on the arm fixture [3] drives —
  gate RED, so on that one arm the recurrence would be caught — and then
  placed the same destroyer in the Normal-mode HOME.3 arm instead:
  **`ok 2`, `FORTH SELF-TEST: ALL PASSED`, exit 0, upstream testSuite
  1/1 OK.** An unguarded third frame destroyer inside `openHOMEorMyM`
  with every mechanical check green. A fourth mutation (flipping fixture
  [4]'s two user-settable flags from MyM.3 to HOME.3 — a different owner,
  same gesture) reddened it, proving that arm is live-reachable with a
  suspended capture and not dead code.
- **Why it is wrong.** The pin's own contract, `design-audit.sh:445-446`:
  *"A third destroyer arriving from upstream moves the first count and
  must be guarded or ruled."* It moves the count only if it arrives in
  one of two spellings at the start of a line. The group banner states
  the class the group exists to retire (`:405-412`): *"Every one was a
  hand list standing in for a counted one."* The destroyer alternation is
  a hand list. The `keyboardTweak.c` banner argues for the broader unit
  itself: *"the census unit here is 'the calls in this function that
  destroy a frame', not 'the consumers of that predicate'."*
- **Bug class.** Enumeration without a count check, one level down — the
  count exists, but the thing counted is a hand-written name list.
- **Class test.** Pin the destroyer set against `softmenus.h`'s exports
  rather than against two remembered names, drop the `^ +` anchor, and
  state the two banner-ruled exclusions (`_executeItem`, pushes) in the
  pin comment rather than leaving them implicit. The battery half:
  fixtures currently drive two of `openHOMEorMyM`'s arms; the arm
  mutation C reached is driven by none, so pin and battery share a blind
  region instead of covering for each other.

---

### R8-4 — the upstream-consumer pin counts FILES, so a new consumer in any of the five files it already knows about never moves it

**Double-found:** D6 tests (reachability lens) and D8 upstream
(correctness lens), independently, at `design-audit.sh:456` (**`:464` at
HEAD — still open**). Both verifiers mutated it, one by synthesising a
real upstream base commit so the audit's own `git archive`
materialisation path was exercised end to end.

- **Where.** `pin 5 "upstream files consuming
  isAlphabeticSoftmenu/isAlphaSubmenu" ... grep -rl ... | wc -l`. Every
  one of its six sibling pins in the same group counts SITES with
  `grep -c`; this one counts files.
- **Reaching input.** `tools/pkg_patch_refresh.py --rebase-base` onto an
  upstream commit that adds one more `isAlphaSubmenu(0)` /
  `isAlphabeticSoftmenu()` call inside a file that already has one. The
  recorded base (`3de5b4be0`) holds **15 sites in 5 files** —
  `keyboard.c` 9, `softmenus.c` 3, `screen.c` 1,
  `c47Extensions/keyboardTweak.c` 1, `bufferize.c` 1. A sixteenth site in
  `keyboard.c` — which holds 9 of the 15 and is the likeliest place a new
  one lands — leaves the pin at 5.
- **Executed.** Verifier: base tree materialised, a tenth consumer
  appended to `keyboard.c` as a real commit, `.refresh-manifest.json`
  repointed, `design-audit.sh` re-run — `ok ... 5`, unchanged, and the
  script's flagged-group set byte-identical to baseline. The best
  refutation available (that the rebase itself would surface the new
  consumer) is killed by the package manager's own contract:
  *"Unedited files fast-forward"* — a call added in a region the package
  does not override merges silently, and the named error is raised only
  for an added or deleted FILE. `bufferize.c` is not overridden at all,
  so a new site there has no merge step to be seen in.
- **Why it is wrong.** The pin's own comment: *"this counts the upstream
  files, so a new upstream consumer is a finding the day the package
  rebases onto it."* A new consumer inside an existing file is not a
  finding the pin can produce. The specified unit is on the record twice:
  `AUDIT_round5_2026-08-06.md:298-300` asked to *"grep every CALLER of
  `isAlphaSubmenu` / `isAlphabeticSoftmenu`, assert the count matches a
  pinned number"*, and round 6 repeats it as *"each CONSUMER"*. This is
  the exact path by which `-MNU_FORTH` reached `openHOMEorMyM`'s
  `popSoftmenu` unguarded — the C-2 defect this pin was written to stop
  recurring, whose site (`screen.c`) holds 1 of the 15.
- **Bug class.** Enumeration without a count check — registered unit is
  not the unit the claim is about.
- **Class test.** Pin the SITE count (`grep -rho ... | wc -l`, or the
  per-file `grep -rc` summed) and keep the file count as a second pin, so
  both units move. Round 5's original form — for each consumer that can
  pop, assert the console's registered frame survives when it runs with
  an interactive capture open — is still the class test proper and still
  unlanded.

---

### R8-5 — "every mid-session `tam.function` rewrite re-derives fold admission" is pinned by a grep whose own justification is false in a reachable arm, and which cannot see two rewrite sites

**Found by** D7 design (correctness lens). Verified; the verifier
mutation-proved the pin blindness and **refuted one of the finding's two
stated consequences**, which is recorded below rather than quietly
dropped.

- **Where.** `packages/forth-core/ui/tam.c:1213` (`tam.function = func`,
  unchanged at HEAD) and `packages/forth-core/programming/manage.c:1399`
  (`tam.function = ITM_FORTH` inside `forthCaptureResume`), against the
  pins at `design-audit.sh:436-441`.
- **Reaching input for the false justification.** Every documented nested
  TAM: `ui/tam.c:571-572` (`_tamLeave(); runFunction(tamOperation());`
  for the `dddVEL` family after STO) dispatches into `tamEnterMode` with
  a fold PENDING — the code documents that arm as reachable at
  `:1244-1252` and subcase [B] of the round-6 battery drives it. On that
  path `tam.function = func` at `:1213` executes, `forthCapIsOpen()` is
  false (the capture is SUSPENDED), so the `forthFoldEnter` arm is
  skipped and the `forthFoldPending()` no-op arm fires: **nothing
  re-derives admission for the new function** and the fold keeps the
  outer item's verdict. Today every reachable nested target
  (ITM_STOVEL/ITM_RCLVEL, TM_VALUE) is admitted, so the stale verdict
  happens to be right. The verifier found a **second, unnamed** site of
  the same shape at `ui/tam.c:943-945`
  (`if(tamOperation() == ITM_M_GOTO_ROW) { _tamLeave();
  tamEnterMode(ITM_M_GOTO_COLUMN); }`).
- **Executed (pin blindness).** A new mid-session rewrite to a
  NON-admitted item with no re-derivation, planted in
  `programming/manage.c`, left all seven group-I pins green and
  byte-identical to baseline. There are 17 `tam.function =` writes in
  that file today that no pin counts.
- **What it costs.** Not a defect on today's tree: the verifier traced
  the finding's stronger consequence ("obeying the header contract at
  `manage.c:1399` ARMS a fold during teardown") and showed it is inert —
  `_tamLeave` has already zeroed `tam.mode`, `forthFoldUnwindIfDone`
  clears foldMode before any armed-sensitive consumer reads it, and the
  only such consumer sampled its value before `_tamProcessInput` ran.
  What remains is structural: a future leave-then-dispatch site whose
  target is non-admitted (ITM_GTOP, ITM_ASSIGN, TM_KEY/TM_STRING/
  TM_NEWMENU) keeps the fold ARMED over an item the PARK list exists to
  keep out of the bracket — the F1 `GTO . .` SIGSEGV shape — and neither
  the pin nor any fixture would say so.
- **Why it is wrong.** `design-audit.sh:432-435`: *"Three writes total:
  tamEnterMode's entry write (whose admission `forthFoldEnter` derives
  from the same func) plus the two rewrites. Both counts are pinned so a
  new write of ANY shape moves one of them."* The parenthetical is false
  in the nested arm — `forthFoldEnter` does not run there — and "a new
  write of ANY shape" is false outside `ui/tam.c`.
  `forth_capture.h:231-238` states the contract unconditionally: *"EVERY
  mid-session rewrite of `tam.function` must re-derive it."*
- **Bug class.** A pin whose justification asserts a mechanism that does
  not run on one of the paths it covers; enumeration scoped to one file
  for a rule stated for all files.
- **Class test.** Assert the invariant rather than counting its writers:
  at every `tamEnterMode` entry and at the top of every commit, assert
  `foldMode == (_forthFoldAdmits(tam.function, tam.mode) ? 1 : 2)`
  whenever a fold is pending. That is falsifiable at the nested arm,
  which the grep is not. Concretely: STO from the console → `dddVEL` →
  commit, asserting the admission verdict matches the *current*
  `tam.function`.

---

### R8-6 — the C-7 guard rewrite's replacement disjunct cannot be true on any reachable state, and it shadows the four real assertions beneath it

**Found by** D6 tests (correctness lens). **Verified by three
mutations**, one of which reproduced verbatim the vacuity proof the C-7
commit itself used to condemn the disjunct it removed.

- **Where.** `packages/forth-core/test_capture.part.h:17441` (unchanged
  at HEAD), round-6 subcase [7]'s reach guard:
  `!forthCapIsSuspended() || !forthFoldArmed() ||
  !forthCapIsInteractive() || forthCapInteractiveLive()`. The same shape
  recurs twice in the NEW round-8 fixture at `:17916-17917` and
  `:17981-17982`, where `forthConsoleTestOwnedCount() +
  forthConsoleTestBorrowCount() == 0` and `!forthConsoleStampOnStack()`
  are logically identical (`forth_menu.c:364-368`, `:424-435`).
- **Reaching input.** `./packages/forth-core/build-test.sh` →
  `test_fold_round6_window` subcase [7]. `||` short-circuits, so disjunct
  4 is evaluated only when disjunct 1 is false, i.e. only when
  `state == FCAP_SUSPENDED`; `forthCapInteractiveLive()` requires
  `state == FCAP_OPEN` (`forth_capture.c:54`, `:133-135`). One enum field
  cannot be both. The `live=%d` in the message can only ever print 0.
- **Executed.** (A) Substituting constant `0` for disjunct 4 left the gate
  GREEN with byte-identical `[7] PASS (F8/F9)` output. (C) Deleting the
  disjunct AND regressing `forthCapInteractiveLive` to its pre-F8
  origin-only form still turned the gate RED with the four correct
  diagnoses — because `_forthConsoleActive()` (`screen.c:5776-5785`)
  contains the predicate and the fixture calls it three lines below. So
  the disjunct adds no detection. (B) Restoring the disjunct under the
  same regression printed **only** `[7] FIXTURE BUG: residue not reached
  (susp=1 armed=1 interactive=1 live=1)`; `scFail` was set, the `else`
  branch never executed, and none of the four F8/F9 failures printed.
- **What it costs.** Negative diagnostics on the one regression the guard
  could ever respond to: a genuine round-6 F8 regression is reported as a
  broken fixture rather than as the defect it is, and the four messages
  that name the real damage are suppressed.
- **Why it is wrong.** The comment the rewrite carries
  (`test_capture.part.h:17433-17439`): *"a guard may only read state the
  fixture did not just write. Every disjunct below is established by the
  real gesture above ... so each one falsifies if that machinery breaks."*
  Disjunct 4 is not established by the gesture and does not falsify when
  the gesture's machinery breaks. Commit `1212e4efb` and the DESIGN-HISTORY
  entry make the same claim.
- **Bug class.** A guard term that is dead because a sibling term implies
  it — the same observable property as the disjunct C-7 removed (dead
  because the fixture wrote it two lines up), by a different mechanism.
- **Class test.** For every fixture guard, mutate each disjunct to
  constant false and require the fixture's output to change; a disjunct
  whose removal changes nothing is either dead or belongs in a separate
  assertion that prints on its own. Applied to this file it flags all
  three sites in one pass.

---

### R8-7 — subcase [3] has no evidence that `openHOMEorMyM` ran: every post-gesture assertion is satisfied by a gesture that did nothing

**Found by** D6 tests (with D1 contracts reaching the same observation
and clearing it — see §6). **Verified by mutation** on the intent lens.

- **Where.** `packages/forth-core/test_capture.part.h:17915-17938`
  (unchanged at HEAD), the C-2 subcase. After `R8_LONGPRESS_F()` the only
  checks are "a console stamp still exists", "`currentMenu()` is still
  `-MNU_FORTH`" and "`aimBuffer` is still `1 2`" — all three true of a
  no-op.
- **Reaching input.** UNREACHED today as a live defect, and traced rather
  than assumed: `fnForthOuter` → `forthEnterAimSurfaceNoLift` sets
  FLAG_ALPHA (`forth_compile.c:1722`), so the long-press chain does reach
  `openHOMEorMyM`'s guarded FLAG_ALPHA branch. The defect is that the
  fixture would not notice if that stopped being true — a GRAPHMODE
  change, `calcMode` becoming CM_EIM/CM_MIM at `keyboardTweak.c:190`,
  FLAG_ALPHA no longer set at console open, a `calcModel` losing the
  long-press key code, or the TO_FG_LONG timer chain moving.
- **Executed.** `return;` as the first statement of `openHOMEorMyM` — the
  function never runs at all — gave `[3] PASS (C-2)` while `[4] FIXTURE
  BUG` fired on the identical mutation via its explicit REACHED proxy.
  That contrast is the whole finding. (A narrower mutation that bypasses
  only [3]'s arm reddens [3] incidentally, because control falls into the
  else arm and pushes MNU_HOME over the row; a gesture that merely stops
  happening produces no such side effect, which is why the first mutation
  is the dispositive one.)
- **What it costs.** The reader is told the C-2 door stays shut when the
  fixture may simply have stopped opening it — and C-2 is the one defect
  in this wave the owner can see on screen. The wave's weakest test
  guards its most visible finding. Mitigation the finding did not credit:
  today a fully-shut door still reddens [4]'s proxy, so the wave is not
  wholly unguarded; but that coupling is coincidental, since a gate change
  scoped to [3]'s HOME.3/FLAG_ALPHA door leaves [4]'s MyM.3 arm untouched.
- **Why it is wrong.** The fixture's own header at `:17614-17616`:
  *"every subcase asserts it REACHED the state it claims to test before
  it asserts anything about the fix (the C22 rule)."* The DESIGN-HISTORY
  entry for this wave claims it too: *"five subcases, real dispatch only,
  each asserting it REACHED its state."* Subcase [4] honours it at
  `:17981-17984`; [3] does not, though a proxy is free —
  `openHOMEorMyM`'s `Shft_timeouts` caller clears `Shft_timeouts` and
  calls `resetShiftState()` before entering. The project has an
  established idiom for accepting such a gap on purpose (*"recorded in the
  test as a DOCUMENTED GAP"*, DESIGN-HISTORY:3133); [3] does not use it.
- **Bug class.** An oracle placed where the mechanism under test need not
  have reached it — reach proved at authoring time, not pinned for the
  future.
- **Class test.** The C22 rule mechanised: for each subcase, a mutation
  that makes the function under test a no-op must redden it. Round-6
  subcase [6], [3]'s named sibling, has the same shape and the same gap.

---

### R8-8 — the fault-injection hook's contract banner names an owner fixture that does not exist, and describes a fixture structure the tests do not have

**Found by** D1 contracts (correctness lens). Verified; the verifier
landed a partial refutation of one clause, recorded below.

- **Where.** `packages/forth-core/programming/manage.c:1771-1772`
  (unchanged at HEAD): *"Selftest builds only, and set only by the
  fixture that owns the family (`test_fold_foldmode0_family`) — which
  restores it in every exit path."*
- **Reaching input.** No runtime path — this is a contract statement a
  maintainer reads. `grep -rn 'test_fold_foldmode0_family'` returns the
  banner and its generated patch copy, nothing else;
  `git log --all -S` shows the string was added exactly once, by
  `d4a204d66` (the commit that introduced the hook) and never removed.
  It is not a stale name left by a rename — it never named anything. The
  actual and only setter is subcase [5] of `test_fold_round8_window`
  (`test_capture.part.h:18046`), and the outer clear at `:18108` sits at
  that whole fixture's tail, outside the family entirely, so *"the fixture
  that owns the family"* also describes a decomposition the test file does
  not have.
- **What it costs.** `forth_capture.h:155-158` deliberately routes the
  reader here (*"Selftest builds only; see the banner at the definition in
  programming/manage.c"*), so this banner is the designated sole authority
  on who may set a flag that makes `forthHistoryEnsure()` return false
  even when FHIST exists — i.e. that breaks the header's own *"Idempotent:
  returns true immediately if FHIST already exists"* for the whole battery
  if it ever leaks. Its one named authority is fictional. **Partial
  refutation, recorded:** the finding's clause "and cannot check the
  restore claim" is overstated — the flag name is the obvious grep key and
  returns all four sites at once, which is how the finding's own author
  verified the three clears. The restore claim itself holds: no early
  return between the set at `:18046` and the clears at `:18097`/`:18099`/
  `:18108`.
- **Why it is wrong.** This is the class the same wave fixed one file
  over: C-6 corrected `forth_capture.h:177-182` because *"this sentence
  still named the old predicate, which is false data for the next reader
  enumerating the Live sites."*
- **Bug class.** A contract sentence carrying a name that is not real.
- **Class test.** The cheapest mechanisation is a lint over the package
  sources: any identifier written inside parentheses in a comment that
  looks like a C identifier ending in a known fixture prefix
  (`test_*`) must resolve to a definition. Group I could hold it as a
  `pin 0`.

---

### R8-9 — C-4's amendment publishes a command that does not produce the number it publishes, and claims a pin that does not exist

**Found by** D8 upstream (intent lens). Verified; no ruling anywhere
sanctions the mismatch.

- **Where.** `design-docs/forth-core/DESIGN_D7-1_tamFinish_2026-08-08.md:16`
  (unchanged at HEAD): `grep -c '_tamLeave();'
  packages/forth-core/ui/tam.c   # 28`. Run it: **29**. The 29th match is
  the wrapper's own body at `ui/tam.c:1512`. The prose split *"26 in
  `_tamProcessInput`, 2 in `_tamHandleShuffle`"* is correct — only the
  published command is not. The same amendment states at `:28` that *"the
  count now lives in design-audit.sh group I, not only here"*: group I has
  eight pins and none of them is this one; `grep -rn '_tamLeave'` over
  every `.sh` and `.py` in the tree returns nothing.
- **Reaching input.** Run the command the amendment publishes as its
  authority. The mismatch existed at the amendment's own commit — both
  `65f2dc709` and `1212e4efb` return 29 — so it is not drift.
- **What it costs.** The next reader, or the next round's tasking, runs
  the pinned command, gets 29 against a documented 28, and re-derives the
  population by hand to discover the extra match is benign. That is
  exactly the cost C-4 was written to stop paying, and this round's own
  tasking paid it. **Correction to the finding as filed:** a genuinely new
  in-file `_tamLeave();` dispatch site would read 30, not 29, so it would
  not be indistinguishable from the benign extra — 29 is a stable
  baseline, and only the re-derivation cost stands.
- **Why it is wrong.** The amendment's own class, quoted from the
  document: *"a human list of call sites not backed by a build-time count
  is a comment, and it comes back short."* Commit `1212e4efb` states the
  deliverable as *"an amendment block with the count, the grep that
  produces it"*. The command as published does not produce the number as
  published, and the pin it defers to was never written.
- **Bug class.** A counted claim whose count and whose command disagree —
  D7-a at the record level, inside the fix for D7-a at the record level.
- **Class test.** `packet_lint.py`'s D7-a rule currently checks only that
  a counted claim CARRIES a `grep -c`; it never runs it, so this class
  passes lint by construction. Make the lint execute the command it finds
  and compare, or land the group-I pin the amendment already claims
  exists.

---

### R8-10 — CONFIRMED at the audited tip, **CLOSED at HEAD**: the P-1 re-anchor resolved the capture step through a stale absolute offset validated by a type-only canary

**Triple-found in family:** D1 contracts, D5 guards (twice, as two
consequences of one root) and D7 design, independently, at
`manage.c:2127-2133`. **Two of the four were executed.** The same root
was found out-of-family by Gemini 3.1 Pro and fixed in `58e07a2bd` +
`bdbfffeb1` — after the tip this pass audits. Reported for the record and
because the convergence is this round's most useful signal (§5); ranked
last because a closed defect costs the owner nothing today.

- **Where (at the audited tip).** `programming/manage.c:2127-2133` and
  `:2173-2177`: `cap = beginOfProgramMemory + forthFoldCtx.capStepOffset`
  guarded by `cap < firstFreeProgramByte && checkOpCodeOfStep(cap,
  ITM_FORTH) && cap[2] == STRING_LABEL_VARIABLE`, with the re-anchor's
  comment claiming *"the sweep's threshold is now read in the fold's OWN
  program by construction"*.
- **The two consequences, both executed.**
  1. **Debris.** `forthFoldCtx.capStepOffset` is written at exactly one
     site (`:2068`) and never resynced; `forthCaptureResume`'s rev-3
     recovery repairs only the CAPTURE's copy (`:1300`). DELP of a program
     that PRECEDES FHIST slides FHIST down, the canary falsifies on the
     stale address, and the round-8 fix's all-or-nothing block skips
     anchor, sweep AND delete. Driven: `FHIST steps 3 -> 4`, the parked
     capture step left in FHIST as a bogus history line the owner sees on
     the up-arrow scroll — with `aimBuffer` correctly restored and the
     fold correctly cleared, so the damage is solely the residue. This
     falsifies the block's own justification verbatim (*"in the only known
     door FHIST is itself the program that was deleted, so there is
     none"*): FHIST survives this door, is renumbered 2→1, and keeps the
     debris. DESIGN.md §8.4.3 forbids it outright — *"A fold may never
     leave an outstanding transient step"* — with a bare power reset as
     the only sanctioned +1.
  2. **A user program gutted.** The canary is a TYPE test, not an
     identity test: every Forth source step in every program is
     byte-identical to a capture step. Driven by changing **one thing** in
     the landed regression test — the typed history line from `"1 2 +"` to
     `"42"` — subcase [1] went from GREEN to RED with its own class
     assertion: `[1] FAIL (P-1): the fold's debris sweep ate the user's
     program — PUSR 13 steps -> 3`. No FIXTURE BUG fired, so the door
     genuinely opened. The landed test passes only because L1=5 happens to
     land the stale offset three bytes inside a step; `"by construction"`
     was byte-alignment luck.
- **Why it was wrong.** The identity contract is stated twice and was
  implemented once: DESIGN.md §8.4.3 (*"the last content step of FHIST"*)
  and `manage.c:1266` (*"The capture step is the LAST ITM_FORTH step in
  FHIST"*), which is what `_forthFoldFindCaptureStep` derives — and which
  `forthFoldLeave` never called.
- **Closed how.** `58e07a2bd` introduced `_forthFoldResolveCaptureStep`
  (`manage.c:2126-2162` at HEAD): the offset is still the fast path but
  the answer must lie **inside FHIST** (`cap >= from && cap < to`, bounds
  taken from `programList[hist-1]` and its successor), and otherwise it
  falls back to the same FHIST scan the resume uses; both the sweep anchor
  and the tail delete now go through it. `hist == 0` returns NULL, which
  preserves the DELP-of-FHIST arm P-1 closed. `bdbfffeb1` then states the
  fallback's own assumption and names the guard to add if a door is found.
  Consequence 1 is closed by the fallback, consequence 2 by the bounds
  check. **Verified by reading the resolver and the commit's red-first
  evidence (`[6] FAIL (OOF): the fold left FHIST changed (3 -> 4)`); the
  finders' two probes were not re-run against HEAD.**
- **Bug class.** An identity resolved by remembered address plus a shape
  test, where the design states the identity structurally.
- **Class test (still worth landing).** Both executed probes belong in the
  battery as permanent subcases, because the class test the fix carries is
  the debris case only: (a) DELP of a program preceding FHIST — assert
  FHIST's step count is unchanged; (b) subcase [1] parameterised over the
  history line's LENGTH, so no future offset arithmetic can pass by
  alignment. `58e07a2bd` landed (a); (b) is unlanded.

---

## 4. PLAUSIBLE findings

One item. This section is short not because the pass had no unreachable
candidates but because most were cleared by their own finders before
refutation and are recorded in §6 with their reasoning.

### R8-P1 — the same `dynamicMenuItem` divert defeats `forthFoldEnter`'s park onto FHIST, materialising the capture step in the caller's program

**Found by** D4 errorpaths (intent lens). Survived refutation: nothing in
the fold's governing documents mentions `dynamicMenuItem` at all — a grep
of `PACKET_L1_F1_fold_context.md`, `PACKET_L1_H_history_program.md`,
`STAGE_L_T7_fold_anatomy_raw.md` and `STAGE_L_T8_pem_host_raw.md` returns
zero hits — while DESIGN.md §3.3.6 and upstream's own `_insertInProgram`
bracket both rule the other way. Same root as R8-2, opposite end of the
fold, worse consequence.

- **The claim.** With `dynamicMenuItem >= 0` latched by the key that
  OPENS the TAM (a parameterized item — STO/RCL/GTO — sitting in a MyMenu
  or user DYNAMIC slot; `keyboard.c:98-102` latches it and `runFunction`
  never clears it), `forthFoldEnter`'s `forthHistoryGotoLastStep()` →
  `goToPgmStep` → `goToGlobalStep` takes the dynamic branch, fails the
  label lookup and returns **without moving the cursor**.
  `forthHistoryGotoLastStep` returns `true` regardless, so the caller
  cannot detect the no-op. `forthFoldEnter` then samples
  `entryStepCount = getNumberOfSteps()` in the CALLER's program and
  `_insertInProgram`s the ITM_FORTH capture step at the PEM cursor — the
  shape its own comment forbids: *"do not 'simplify' this back to
  inserting at the caller's currentStep"* (`manage.c:2058-2062`) — and
  the sweep's threshold is then read against the wrong program, the exact
  failure `:2049-2055` says the reposition exists to prevent. If anything
  aborts before the tail delete, a stray `FORTH "..."` step stays
  permanently in the owner's program.
- **Why it is only PLAUSIBLE.** The verifier's lens was intent and the
  reaching input was not constructed. The open question is whether a
  MyMenu slot holding a parameterized item actually opens TAM from a live
  console rather than being consumed earlier: `items.c:744`'s live-capture
  divert takes `PTP_NONE` items only, and `items.c:789-793` documents
  parameterized items falling through to the TAM block — which is what
  the finding needs — but no one drove it.
- **What would settle it.** Assign ITM_STO to a MyMenu slot, open the
  console, push MyMenu over the console row, press the slot; assert the
  capture step landed in FHIST and not in the program the PEM cursor was
  in. One fixture, and it is the same fixture R8-2's class test needs with
  the latch set at a different moment. If it opens, this outranks R8-2 and
  sits beside R8-1.

---

## 5. Design observations (D7)

**D7-a is now three rounds old and it recurred inside its own
countermeasure.** Group I was minted in this wave to retire hand lists;
three of its eight pins are hand lists — two spellings of "destroyer"
(R8-3), file cardinality standing in for consumer count (R8-4), one file
standing in for a rule stated for all files (R8-5) — and a fourth
enumeration the same commit published (`_tamLeave`, 28 sites) is the one
group I does not pin at all, while its published command prints a
different number (R8-9). The mechanism is consistent across all four: the
*rule* is stated correctly and the *unit registered* is narrower than the
rule. The countermeasure for that is not another pin; it is a rule about
pins — **a pin must count the thing its comment claims, and the way to
check that is to mutate the artifact and watch the pin move**. Every one
of R8-3, R8-4 and R8-5 was settled in minutes by exactly that mutation.
A `pin` helper that carried an optional "and here is a mutation that must
move it" is the only mechanised form of this I can see; short of that, the
review rule is: no new pin lands without its author having watched it go
red.

**The fold context is a snapshot of five quantities taken before a
dispatch that can move all five.** The wave converted one of them —
`capStepOffset` — from *remembered* to *re-derivable*, first partly (the
canary) and then properly (`_forthFoldResolveCaptureStep`, out of
family). That conversion is the right shape and it is now proven twice
over. The other four are still remembered rather than derived:
`entryStepCount` is at least now *consumed* in the right program, by the
re-anchor, but the cursor triple (`savedProgram`, `savedLocalStep`,
`savedFirstDisplayed`) has no repair at either end, and R8-1 is the first
of them to be executed as a crash. The shape to carry into round 9
is a single `forthFoldRestoreCursor()` that validates before it consumes,
in the same way the resolver does: `savedProgram` clamped and renumbered
the way `fnClP` renumbers its own copy, `savedLocalStep` bounded by the
restored program's length. Note the sibling `_forthHistCur` at
`manage.c:1631-1648` caches the same triple and has the same exposure.

**`goToGlobalStep` is not a navigation primitive; it is a mode-dependent
one, and the mode is a global that key handling latches.** Four sites in
this tree navigate during a key press; two carry the bracket — upstream's
own inside `_insertInProgram`, and the package's pre-`fnExecute` clear —
and the fold's two, `forthFoldEnter`'s park and `forthFoldLeave`'s
restore, carry neither. This is a *shape* problem rather
than three bugs: the package should own one wrapper — call it
`forthGoToPgmStep` — that brackets `dynamicMenuItem` and is the only
thing the Forth code calls, so that the enumeration becomes "grep for the
raw name in package sources, expect 0". That is a pin that counts the
right unit, which is what §5's first observation asks for.

**The battery's guard idiom is drifting toward disjunctions with dead
terms, and a dead term in a reach guard is worse than a missing one.**
Three sites now (R8-6). The mechanism that makes it costly is not the
redundancy — it is that a reach guard *precedes* the real assertions and
sets `scFail`, so when it fires it suppresses them. The idiom to prefer
is separate `if` statements that each print, rather than one disjunction
that prints a composite message; the fixture then tells the reader which
fact was wrong, which is the reason `design-audit.sh:459-467` exempts the
test battery from the C-6 longhand rule in the first place.

**On convergence and refutation.** Three in-family dimensions and one
out-of-family reader found R8-10's root by four different routes, and a
fourth in-family verifier REFUTED a fifth route to the same root — with a
full instrumented run showing the recovery arm firing zero times in ~2600
lines of output. That refutation was correct about everything it
measured and wrong about the conclusion: it drove the documented `dddVEL`
door and the existing battery, neither of which deletes a program below
FHIST. The lesson is narrow and worth encoding: **a REFUTED verdict whose
evidence is "I instrumented the existing battery and it never happens" is
weaker than one that constructs the negative case**, and when three
dimensions have converged on a root, a fourth reader's refutation of it
should be treated as a claim about the fixture's coverage rather than
about the code.

---

## 6. Deliberately not flagged

Merged from what the eight finders cleared and what the refutation pass
disproved. Where two dimensions disagreed about the same item, both
positions are recorded and the settling evidence named.

### Refuted by the pass

1. **C-1 pins only the `tam.function` half of admission; `tam.mode` is
   rewritten three times after `forthFoldEnter` consumed it**
   (`ui/tam.c:1295/:1301/:1307`). REFUTED on intent: the design named the
   admission input by line — `STAGE_L_T7_fold_anatomy_raw.md:94`, *"`mode`
   is the value tamEnterMode computed at ui/tam.c:1151"*, which at the
   authoring commit sat 67 lines ABOVE the normalising switch — and it
   enumerates `TM_VALUE_NORM` and `TM_VALUE` on the same admitted side.
   More decisively, DESIGN.md:2698-2703 rules the CRITERION, not just the
   list: a mode parks only when it *"navigates the program pointer, zeroes
   `aimBuffer`, or flips `FLAG_ALPHA` on its own path"*, and the three
   normalisation arms do none of those (their only other statement is a
   `showSoftmenu` of a flavoured TAM row). The two sub-claims are true and
   confirmed — the header sentence guards only `tam.function`, and the
   group-I pins are blind to `tam.mode` (mutation-confirmed) — but there
   is no defect behind either. *Residual worth one line:* STAGE_L_T7:211's
   aside *"`func` and `tam.mode` are already final at this point"* is
   false for both fields; that sentence is the thing to correct.
2. **The P-2 refusal is a failure return from a `void` function whose
   callers pre-write state; `assignEnterAlpha` strands `tam.alpha = 1`
   with `tam.mode = 0`.** REFUTED on correctness: the teardown owner of
   that pair is `assignLeaveAlpha`, not `_tamLeave`, and every
   `assignLeaveAlpha` site gates on `calcMode == CM_ASSIGN && tam.alpha`
   with no reference to `tam.mode`. One BACKSPACE (`keyboard.c:4909`),
   EXIT (`:4519`) or ENTER (`:2788`) clears it, and all three resolve
   through the alpha column of the layout tables, so the `|| tam.alpha ||`
   routing does not swallow them. `calcModeAim` explicitly declines to
   leave CM_ASSIGN. Separately the premise is unreachable: CM_ASSIGN is
   set at exactly one site (`assign.c:560`, `fnAssign`), and ITM_ASSIGN is
   `CAT_FNCT|PTP_NONE`, which `items.c:744`'s divert turns into
   `forthCapInsertName("ASSIGN")` whenever a console line is live — the
   ASSIGN key TYPES the word and never calls `fnAssign`.
3. **C-6's zero-longhand pin misses the `tamEnterMode` suspend seam three
   lines below the new guard** (`ui/tam.c:1240-1241`). REFUTED on intent:
   `PACKET_L1_F2_fold_seams.md` §C1 designs that exact two-line form
   verbatim and rules on the predicate — *"The guard is
   `forthCapIsInteractive()`, NOT `calcMode != CM_PEM`"* — with the
   contract it leans on named (*"`forthCapIsInteractive()` must stay true
   across a suspension"*). The outer arm is `IsOpen && (CM_PEM ||
   IsInteractive)`, strictly broader than Live and covering the PEM-origin
   capture, which Live cannot express; substituting Live would delete the
   PEM suspend path. Round 7's verification pass already ran the census
   (14 sites) and cleared this residue.
4. **`forthCaptureResume`'s recovery updates the capture's offset but not
   the fold's, and round 8 made the stale copy govern the anchor.**
   REFUTED on reachability by a fully instrumented gate run — 53
   `forthFoldLeave` entries, 4 canary falsifications, **0** recovery
   successes — and by the structural argument that `forthFoldEnter` forces
   `pemCursorIsZerothStep = false` so an in-fold commit lands AFTER the
   capture step and cannot move it. **This refutation did not hold.** Two
   sibling verifiers executed the divergence by a mover the refutation
   never considered: a DELETION BELOW FHIST (`fnClP` → `_clearProgram` →
   `deleteStepsFromTo`), not an insert. It is recorded here as refuted
   *and overturned*, and the same item is CONFIRMED as R8-10 — the
   instructive part is §5's closing note.
5. **D7-1's 28-site enumeration is unpinned in group I.** REFUTED on
   reachability, and the finding mis-identified its own population: the
   design says those 28 are *"already correct by the epilogue's
   construction"*, and the direction PROMPT_CODE_AUDIT.md:62-73 calls
   undefendable is a new upstream in-file caller of the PUBLIC name, which
   by definition does not move a `_tamLeave();` count. Mutation-proved:
   the verifier installed the demanded pin, then added the exact drift —
   an `else if(...) { leaveTamModeIfEnabled(); reallyRunFunction(...); }`
   arm inside `_tamProcessInput` — and the pin reported ok. The remedy is
   inert against the consequence. *Residue carried forward as R8-9*: the
   same verifier found the 29-vs-28 mismatch and the false "the count now
   lives in group I" claim.

### Cleared by the finders — the wave's own fixes, attacked and held

**The P-2 refusal (`ui/tam.c:1205`).** Cleared four ways. Its guard is
exactly co-extensive with the seam it protects (`calcMode != CM_NIM`
mirrors the `if(calcMode == CM_NIM)` arm above; Live is bit-identical to
the seam's `IsOpen && IsInteractive`), so no path suspends without it
having run and no path is refused that would not have entered the fold.
It is genuinely before any TAM write, so there is no teardown debt.
`screenUpdatingMode` looks skipped but `displayCalcErrorMessage` sets it
itself. `lastErrorCode` is left set, and R12's CM_AIM BACKSPACE arm is the
dismissal. `forthHistoryEnsure`'s earlier call is idempotent and brackets
itself with save/restore of the cursor, so `forthFoldCtx.savedProgram` is
sampled in the same state as before. The two callers a WIDER guard would
have wrecked — `assign.c`'s pre-set alpha state and `addons.c`'s
delete-then-enter step edit — are out of its reach by construction; the
hazard is that a later "defensive" widening arms both.

**C-1's re-derivation, in the new direction.** The BACKSPACE demotion can
now RE-ARM a fold inside `_tamProcessInput`, which the F1 one-way patch
could not. Traced and cleared: `forthFoldEnter` does all the machinery
(context save, reposition, capture-step insert) BEFORE it computes
admission, so foldMode 1 and 2 differ only in the bit; both rewrite sites
`return` before any commit in the same call; `tamProcessInput`'s
`const bool_t brk = forthFoldArmed()` is sampled at entry and the restore
is gated on `brk && calcMode == CM_PEM`; `forthFoldUnwindIfDone` no-ops
while `tam.mode` is non-zero. `GTO . BACKSPACE EXIT` then enters the next
`tamProcessInput` with `brk` true and unwinds correctly. Also confirmed
that foldMode 2 is only ever written as "admits == false", so re-deriving
cannot resurrect a park that meant something else.

**C-2/OOF-1's two guards.** Both are evaluated AT THE CALL and after the
branch's own `leaveTamModeIfEnabled()`, which is what OOF-2 requires. The
two arms are disjoint for a real reason: a live console always has
FLAG_ALPHA set, and `tamEnterMode` clears FLAG_ALPHA for every mode except
TM_NEWMENU/TM_STRING, so a mid-TAM suspension takes the non-alpha arm. The
skipped `showSoftmenu(-MNU_TAMALPHA)` inside guard 1 cannot matter because
`_tamLeave` has already cleared `tam.alpha` on that path (the comment
saying otherwise is loose; the code is right). `_executeItem` is
unreachable with a live console (`calcMode != CM_AIM`) and is
banner-ruled out of scope. The census of menu-stack calls in the file was
re-derived independently: exactly two destroy a frame, both guarded.

**C-3's render-time clamp.** Attacked as "a renderer that mutates state"
and cleared. One caller, behind `_forthConsoleActive()` (so never over a
TAM or an error frame), no measure pass, no double clamp;
`forthConsoleSetViewOffset` does not bump `consoleSeq`, so it cannot fool
the write-seq oracle; its own `count-1` clamp cannot fight the
`count-rows` bound because `count-rows <= count-1` for `rows >= 1`; the
`rows == 0 || count == 0` early return covers the degenerate frames. The
arithmetic checks out against N-R3's geometry (4 rows at 128, 2 at 67; no
unsigned underflow, no off-by-one at either end, the oldest line stays
reachable). The one-frame lag on `yMultiLineEdOffset` is pre-existing and
ruled. The documented cost — crossing the boundary settles the scroll
position at the new maximum — is stated at the site and is the owner's
accepted trade.

**The C-6 conversions.** Verified bit-identical rather than plausible:
`IsInteractive` is `origin == INTERACTIVE && state != CLOSED`, `IsOpen` is
`state == OPEN`, so the conjunction reduces exactly to Live's body. The
remaining bare `forthCapIsInteractive()` sites are deliberate origin
questions; `keyboard.c:4096` (`interactive && !open`) is the
suspended-residue test and is correctly not matched by the pin.

### Cleared by the finders — hazards traced to a stop

- **`screen.c:965-966`, the unguarded `fnExitAllMenus(0)` twin.** Looks
  like the OOF-1 shape in the package's own copy of the F7 function.
  Unreachable with a live capture: a live console always has FLAG_ALPHA
  set, so the `MyAlpha` push arm above always wins; mid-TAM the enclosing
  `tam.alpha || !tam.mode` gate excludes the block; and unlike
  `openHOMEorMyM` this function's `leaveTamModeIfEnabled` is commented
  out, so no wrapper resume can flip the state under it.
- **`bufferize.c:459`, the fifth upstream consumer of the widened
  predicate and the only one in a file the package does NOT override.**
  Gated on `calcMode == CM_NORMAL && !tam.mode`; a live console is CM_AIM
  and a suspended one has `tam.mode != 0`. No input constructed. Recorded
  because it is the reason R8-4's file-count pin matters: this is a
  consumer with no merge step to be seen in.
- **`ui/tam.c:1228`, a surviving in-file `leaveTamModeIfEnabled(); ...
  addItemToNimBuffer()` pair** — the exact shape C-5's new standing lens
  hunts. Unreachable: it sits in the CM_NIM arm, which the P-2 guard one
  screen above excludes precisely because no live interactive capture
  exists there. Worth a comment naming it as ruled so the next reader
  running the lens does not re-derive this.
- **The unbounded FHIST walkers** (`_forthHistLineCount`,
  `_forthHistLineAt`, `_forthHistProgramBytes`, `manage.c:1704-1740`) —
  no iteration cap, and the third passes `findNextStep`'s result to
  `isAtEndOfProgram` with no NULL check, while their siblings in the same
  file (`_forthFoldFindCaptureStep`'s `i < 512`,
  `forthConsoleLineCount`'s cap, whose comment names the class) are
  guarded. They only ever walk FHIST and `findNextStep` returns NULL only
  on an invalid parameter encoding, so no reaching input. Cheap to cap;
  the class has already come back at a second site once.
- **`getNumberOfSteps()` at `ui/tam.c:335/753/766/814` is FHIST-scoped
  during a fold** — the third and fourth consumers of P-1's own class.
  Cleared because all four feed only GTOP's digit-entry bound, GTOP PARKs,
  and `forthFoldLeave`'s cursor restore undoes the navigation. Owner-visible
  consequence nil today; the fifth site of the class if GTOP from the
  console is ever made to stick.
- **The debris sweep's UAF `break` then two further list touches**
  (`manage.c:2158-2160` then `:2176`/`:2182`). Its cited rule says
  *"Abandon the loop rather than touch either list again"* and the next
  statements do touch both. Pre-existing (round 8 only re-indented it) and
  no input constructed that puts ERROR_RAM_FULL inside
  `scanLabelsAndPrograms` during a sweep.
- **Post-abandon surface residue** — CM_AIM with FLAG_ALPHA cleared, the
  capture CLOSED, the FWRD row unstamped, none of the resume tail run.
  Recoverable with EXIT, pre-existing, and round 7's P-1 already ruled
  *"the line is lost either way"*.
- **`goToPgmStep` after DELP renumbering, considered and cleared by one
  finder** — the arithmetic reader traced it to the wrong-program landing
  and stopped at "documented round-3 residue"; a second reader executed it
  to a SIGSEGV. That is R8-1, and the disagreement is recorded because the
  clearing reasoning ("after FHIST is deleted the numbers shift down,
  which lands the cursor in the wrong program rather than off the end") is
  exactly right for the FHIST-first layout the fixture builds and wrong
  for the ordinary one.
- **The `XEQ → GTOP → GTO` identity loss.** `XEQ .` promotes to GTOP but
  BACKSPACE demotes to GTO, so C-1's newly-armed fold splices "GTO nn"
  into a line the owner began with XEQ. Byte-identical upstream
  (`src/c47/ui/tam.c:339/798`); C-1 makes it visible, does not cause it.
- **The 17 `tam.function =` writes in `programming/manage.c`.** Each was
  read: all execute with `tam.mode == 0` (capture-era tam is exactly
  `{mode 0, function ITM_FORTH}`), so no re-derivation is owed and
  `forthFoldRederiveAdmission` would no-op at every one. The pin's narrow
  scope is right in substance; only its comment overclaims (R8-5).
- **The re-anchor leaving `currentLocalStepNumber` and
  `firstDisplayedLocalStepNumber` describing the SAVED program while
  `currentProgramNumber` names FHIST.** Chased because relocating state is
  this project's most dangerous fix shape. `getNumberOfSteps` reads only
  `currentProgramNumber`; `defineFirstDisplayedStep` walks forward with a
  `findNextStep` NULL guard; and `forthFoldLeave` overwrites both
  quantities four lines later. Transient inconsistency, no crash — except
  through R8-2, where the overwrite does not happen.
- **Fault-injection hygiene.** `forthHistoryEnsureFailInjected` is cleared
  on all three exit paths plus once more after `R8_RESET`, is
  `#if FORTH_DEBUG_SELFTEST` on both definition and declaration, and no
  other fixture touches it. The banner naming its owner is wrong (R8-8);
  the discipline it describes is real.
- **Test-fixture arithmetic.** `R8_PROG_STEPS` mirrors `getNumberOfSteps`'
  walk, so subcase [1]'s 13-step expectation is right; the clamp test's
  `6-2` and `6-4` match the geometry; the hardcoded 6 cannot hide a wrong
  ring count because the FIXTURE-BUG checks fire first. Both new fixtures
  restore what they touch; no cross-test leakage.
- **The 1634-line `keyboardTweak.c` override for two guards.** The package
  system working as designed; the generated patch is 4 hunks / 54 added
  lines with unchanged context, and the un-reindented `else {` block is
  the merge-conservative choice that keeps it that small. The footprint
  cost is declared in the commit rather than smuggled.
- **Group A/B/D/E running red.** Chronic, baseline last written at
  `97e7cb5bf`, already reported by the script — out of scope per the
  brief's rule. This wave's own contribution to group D is +3.
- **`packet_lint.py`'s new JUDGE rule** suppressing itself on any
  `grep -c` in the packet and not recognising the `grep -rl | wc -l`
  idiom. Advisory lint, not a gate; below the bar as a finding, and the
  execution half is R8-9's class test.
- **The disagreement worth naming.** D2 lifecycle cleared the
  frame-destroyer regex as speculative on the grounds that upstream style
  puts such calls on their own line — a census of all 42 `popSoftmenu(`
  sites in `src/c47` confirms that, so the tab/inline half of R8-3 IS
  speculative. D1 and D8 flagged it on the differently-named-destroyer
  half instead, and the mutations settled it: the name half is real, the
  shape half is not, and the finding is filed on the name half.

---

## 7. Verdict

**Would I ship it? Not before R8-1.** Everything else in this range is
either sound or costs the record rather than the owner. R8-1 is a
segmentation fault reached by DELP of a program from an open console —
five keys, a gesture the fold exists to make safe, and the same gesture
family the wave's own regression test drives — and its milder form
silently parks the PEM cursor in a program the owner never named while
overwriting the correct restore upstream had just performed. It was
executed under the gate. It is open at `HEAD`.

**Where it breaks first.** Console open, three programs, cursor last in
the LAST one, DELP a program below it: out-of-bounds read of the
reallocated `programList` and a walk with no NULL guard (R8-1). Second:
DELP or STO from the console with the operand picked from the softmenu —
the ordinary way to use both — leaving the cursor inside the hidden
history program at a step number belonging to another program (R8-2).
Third, if R8-P1 opens, the capture step materialising in the owner's
program instead of FHIST.

**The fix-wave order the evidence carries.** R8-1 red-first, with the
class test over the whole fold context rather than a patch at the one
call. R8-2 in the same commit — it is the same call site, and the honest
fix for both is one validated restore helper plus the
`dynamicMenuItem` bracket, which also settles R8-P1 without waiting for
its reaching input. Then R8-3 and R8-4 as one pin commit, each with the
mutation that moves it recorded in the commit message. R8-5 and R8-6 fold
into the next edit of their files.

**What I would leave alone if the goal were correct code rather than an
audit-clean tree.** R8-8 (a name in a comment; correct it the next time
that banner is edited). R8-9 (one number and one false sentence in a
design doc — worth a line, not a commit of its own, though the round's
own tasking has now paid its cost twice). R8-6's two secondary sites at
`:17916` and `:17981` (the primary shadowing behaviour is worth fixing;
the duplicates are noise). R8-7 (add [4]'s proxy to [3] the next time
that fixture is opened — three lines, no commit needed on its own). None
of these should gate the wave that fixes R8-1 and R8-2.

**The pattern, eighth round running.** The mechanics keep holding and the
counting keeps failing — but this round the counting failed *inside the
countermeasure built for it*, and it failed the same way four times: the
rule stated broadly, the unit registered narrowly. Second pattern, newer
and more useful: three dimensions plus the out-of-family reader converged
on one root, and the pass's one wrong REFUTED was on that same root. The
in-family process reached the defect the out-of-family reader had already
got fixed, by three independent routes and two executions — and it also
found a crash (R8-1) that the out-of-family pass did not, sitting eleven
lines below the code both passes were staring at. Neither pool dominates
the other; what the round argues for is running the out-of-family pass
EARLIER, so the in-family readers are not spending their budget on a
defect that is already closed.

---

## 8. Round and exit state

**Round 8, in-family pass.** Subject `28eb24b93..db2186cf1`, the round-7
fix wave. Everything measured at that tip except §2's HEAD row and the
closure check for R8-10, both taken at `bdbfffeb1`. Tree clean at finish;
every verifier reported a clean worktree and every mutation was reverted.

**Readers.** Eight in-family dimension finders (contracts, lifecycle,
arithmetic, errorpaths, guards, tests, design, upstream), blind, via
`audit-workflow.js`; twenty-one findings into the three-lens refutation
pass, each verifier in its own worktree; sixteen survived, five refuted;
four cross-dimension duplicates merged into R8-10 and two pairs merged
into R8-3 and R8-4 → **nine CONFIRMED open, one CONFIRMED closed at HEAD,
one PLAUSIBLE**. Sixteen of the twenty-one verification runs mutated or
probed; all reverted. The out-of-family pass ran separately and its two
findings were fixed in `58e07a2bd`; this report does not re-audit those
fixes.

**What this round settled.**

- **The regression record held again, and harder than round 7:** every
  behavioural finding here (R8-1, R8-2, R8-10) sits in code committed the
  same day, and R8-10 was found by four independent routes. Rate: r2 4/7,
  r3 4/4, r5 9/12, r7 2-of-7, r8 3-of-11 behavioural — the shape is
  constant (a fix that enforces its rule on an enumerated subset, or that
  exempts one consumer of the class it names).
- **D7-a's countermeasure needs its own countermeasure** (§5): no pin
  lands without its author having watched it go red under a mutation.
  Three of group I's eight pins would not have landed under that rule.
- **One refutation overturned** (§6 item 4). The mechanism —
  instrumenting the existing battery and finding the arm never fires —
  now has a name and a caveat.
- **The stale-worktree rule paid for itself a third time**: eleven of
  eleven verifier worktrees spawned at `c3a00768c` and every one caught
  it before its first read.

**Process notes (the growth rule).** (1) **A new reader-pool trap:
the session scratchpad is shared across sibling verifiers.** Two verifiers
reported a sibling overwriting their `baseline.log` mid-check, and one
reported a `build-test.sh` invocation whose `==> repo:` line named a
sibling worktree — that run's numbers were discarded and everything
re-run into a private subdirectory. No evidence in this report comes from
a log whose first line does not name its own worktree. The fix belongs in
the workflow: give each verifier a private scratchpad path, or require
uniquely-named logs and a `==> repo:` check before any number is trusted.
(2) The report filename exceeded the 255-byte limit for the second round
running; the series convention is the filename, per this document's own
template. (3) `design-audit.sh` is not invoked by `build-test.sh`, so
group I is a manual stage-close check only — worth stating in the script's
own header, since three of this round's findings are about pins that
nothing runs automatically.

**Exit criterion: NOT met, and reset again.** Two consecutive rounds with
no new CONFIRMED finding, at least one out-of-family, close the audit.
Round 8 produced ten new CONFIRMED open findings (two behavioural, one a
crash), so the earliest close moves to **round 10**. Round 9's gate items:
the R8-1/R8-2 fix wave red-first with the fold-context class test; the
R8-3/R8-4 pin corrections with their mutations recorded; R8-P1 settled by
one fixture drive; and — explicitly named because they landed after this
pass's tip and are therefore unaudited — `58e07a2bd`
(`_forthFoldResolveCaptureStep`, the `forthConsoleBaseOnTop` guard
narrowing, the third group-I guard-shape pin) and `bdbfffeb1`. The
standing open findings C5–C22 carry forward per the handoff, untouched by
this round's scope.
