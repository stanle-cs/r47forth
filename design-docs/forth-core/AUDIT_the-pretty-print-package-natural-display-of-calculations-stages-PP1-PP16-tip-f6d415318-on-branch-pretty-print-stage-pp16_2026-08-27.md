# Audit — the pretty-print package (natural display of calculations), stages PP1–PP16, at `f6d415318`

Subject: the whole package, `undo-history/stage-u2..pretty-print/stage-pp16`,
29 commits, PP1 through PP16 plus the sprintf/warning sweep that is the
audited tip. Eight finder dimensions ran blind to each other; every
finding then went to an independent refutation pass with a single assigned
lens (reachability, correctness, or intent), adversarial by construction.

**Fourteen distinct CONFIRMED findings, two REFUTED, eleven beyond the
verification cap and reported as unverified.** Four of the fourteen were
found independently by two to five dimensions and therefore carry two to
five independent lenses; the other ten carry one each.

The worst finding is not the browser and not the equation language. It is
one line in the capture engine: `ppcInvalidate(true)` is the only
emit-with-register call site that runs **after** the dispatch, so when an
unmodelled function follows a finished formula, the formula is filed into
history with that function's output as its result. `2 ENTER 3 + IP` stores
`2 + 3.7 = 5`, permanently, and ENTER on that browser row recalls the
wrong number into X. The shipped fixture T14 drives this exact path and
asserts only the history *count*, which is why the gate is green.

The second is that the package's flagship PP10 feature does not survive
its first keypress in the configuration the package's own gate builds
first. `calcMode 20` is registered in the six `fnKey*` switches
pretty-print patched and in neither of the two upstream gates that decide
whether a key becomes an item at all; both of those edits live in sibling
packages. `DESIGN-HISTORY.md:540-542` says solo resolves those keys
"through the final else". The final else is `displayBugScreen`.

Nothing was fixed. The tree this report finishes on is the tree it started
on.

*(Tree note: the working tree is at `6ee277762`, one commit past the audited
tip — the r1 out-of-family fix wave, which closed two findings the
errorpaths dimension also reached independently and which is therefore
out of scope here. See §6a.)*

---

## 1. Subject and coverage

**Tip.** `f6d415318` on `pretty-print/stage-pp16` ("pkg: bound every
sprintf in the package's own sources; clear the remaining warnings").
Range `undo-history/stage-u2..f6d415318` = `70f8b7db7..f6d415318`, 29
commits, `05500bae8` (docs) through the tip.

**Diff.** 73 files, +59,174 / −72. The number is dominated by generated
mirrors and by the two upstream files the package copies whole:

| area | files | notes |
|---|---|---|
| package's own new sources | 10 | `prettyCapture.c` 1152, `prettyValue.c` 868, `prettyEquation.c` 827, `prettyLayout.c` 759, `prettyFormula.c` 738, `browsers/prettyBrowser.c` 214, headers 245, `prettyTest.c` 2461 |
| upstream overrides | 11 | `screen.c`, `items.c`, `items.h`, `keyboard.c`, `bufferize.c`, `calcMode.c`, `config.c`, `defines.h`, `softmenus.c`, `c47.h`, `solver/equation.c` |
| generated `patches/` + `files/` + manifest | ~26 | regenerated output; read as the authoritative statement of what changes upstream |
| sibling packages | 19 files across forth-core + undo-history | the browser-range amendment `430b818f7` and its regenerated twins |
| design docs | 3 | `DESIGN.md` (513), `DESIGN-HISTORY.md` (800), `TESTING.md` |

**Read at line level** (union across the eight dimensions):
`DESIGN.md` in full by five dimensions; `DESIGN-HISTORY.md` in full by
two and at the PP9–PP16 entries by four; `TESTING.md` in full by two.
Code read in full: `prettyCapture.c`, `prettyFormula.c`,
`prettyEquation.c`, `browsers/prettyBrowser.c`, `prettyInternal.h`,
`prettyPrint.h`, `prettyLayout.c`, `prettyValue.c`, `prettyTest.c` (all
2461 lines, tests dimension), the PP14 block appended to
`solver/equation.c` (`:1685-2238`) and its two hook sites, every hunk of
all 13 generated patches. Upstream read for reachability:
`items.c reallyRunFunction/runFunction`, `keyboard.c determineItem /
btnPressed / btnReleased / processKeyAction / fnKey*`, `bufferize.c
closeNim + addItemToNimBuffer`, `calcMode.c calcModeNim`, `error.c
displayBugScreen`, `saveRestoreCalcState.c doLoad`,
`saveRestoreBackup.c restoreCalc`, `solver/sumprod.c`,
`solver/differentiate.c`, `solver/integrate.c`, `programming/lblGtoXeq.c`,
`ui/tam.c`, `flags.c`, `browsers/flagBrowser.c`, `registers.c`,
`store.c`, `charString.c`, `screen.c showGlyphCode/showString`, plus both
sibling packages' `keyboard.c`.

**Deliberately not audited.** Pixel geometry of the layout paint arms
(the Σ/∏ stroke construction, the radical DDA, `PP_BARS`/`PP_SUB`
placement) — that is what the P-, S- and EQ- pixel pins exist for, and
re-deriving them by reading is not a good use of an audit. The 6221-line
`testSuite/testSuite.c` mirror beyond its one added hunk. `softmenus.c`
outside its two hunks. The non-PP14 body of `solver/equation.c`. Flash
and BSS deltas — the stage commits record measured numbers and the
prompt excludes what the gate already reports.

**What the budget did not reach.** Nobody built the package for the
DM42n target or ran anything on hardware; every finding is PC-build or
static. The contracts dimension did not read `prettyTest.c` or
`TESTING.md`, so its four findings were written without knowing what the
suite pins (three of them turned out to be exercised-but-unasserted,
which the tests dimension then confirmed independently). The upstream
dimension did not run the combined build gate. The design dimension
could not close one path either way and says so: whether `closeNim` can
reach `closeNim_exit` with `calcMode == CM_MIM` — `prettyNoteNumberCommit`
is the one hook that does not enforce the design's `CM_NORMAL`/`CM_NIM`
scope rule, and the matrix-editor path was not traced to a conclusion.
That is an hour of someone who knows MIM, and it is the only known
unclosed reachability question in the report.

**Verification cap.** 22 finder reports went to the refutation pass; 20
survived (collapsing to 14 distinct findings after merge), 2 were
refuted. Eleven further findings — the whole design/upstream tail — were
beyond the cap and are listed in §4 and §5 as unverified, with what
would settle each.

**Numbering.** The out-of-family pass that ran the day before took
`R1-1`/`R1-2` (and a fix wave in progress has since taken `R1-3`). This
report's findings are `A1`–`A14` so a grep is unambiguous, following the
round-11 precedent where the second leg took its own prefix. Nothing here
is renumbered from that series and nothing from it is re-reported.

**Lens count per confirmed finding**, because a finding found once and
refuted once is weaker evidence than a finding found five times:

| finding | dimensions that found it | lenses applied |
|---|---|---|
| A (browser bug-screen, solo) | lifecycle, errorpaths, guards, tests, design | correctness ×3, reachability ×2 |
| B (post-op result on invalidate) | contracts, errorpaths, guards | reachability, intent ×2 |
| C (unguarded nested DONE) | contracts, errorpaths | correctness ×2 |
| D (TKBIG recall) | contracts, arithmetic | intent ×2 |
| E–N | one each | one each |

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh` at `f6d415318`, run in
isolated worktrees by four separate verifiers as the baseline for their
mutations: **solo GREEN** every time (173–253 s; 13,022 tests passed, 0
failed), **combined GREEN** once
(`forth-core,undo-history,pretty-print`). I did not re-run it as report
author; the owner's working tree is one commit ahead and I did not want
to regenerate `patches/`+`files/` under it.

**Warnings.** `f6d415318` is itself the warning-clearing commit; no
verifier reported a new diagnostic from any of the ~30 builds run during
verification.

**`design-audit.sh`.** Not applicable and not run: the script is
forth-core-scoped (`PKG="packages/forth-core"` hard-coded at line 30) and
there is no pretty-print equivalent. That absence is itself worth a
line — pretty-print has 11 upstream overrides and a claims registry
declared BINDING for other packages, and nothing mechanical checks either
against the tree. See §5, D7-1.

**Patch churn scan**
(`.claude/skills/upstream-diff-review/references/patch_churn_scan.py`):
13 patches, 31 hunks, **661 added / 17 deleted upstream lines**, one
mechanical churn finding — `[WS-ONLY]` in
`010-solver__equation.c.patch` (§5, D7-5). 583 of the 661 added lines are
the single appended block in `solver/equation.c`.

**Refresh sync.** Verified independently for this report: every file
under `packages/pretty-print/files/` is byte-identical (same git blob) to
its counterpart in the flat working area at `f6d415318`. The numbers
above describe the tree that actually builds.

**Composition probe** (upstream dimension, scripted): all three packages'
patches applied in sequence with `git apply --3way` onto pristine
upstream, for each of the 14 shared upstream files. All 14 compose
cleanly today, zero conflict markers, order-independent for `items.c`
across three orderings. Every override's pre-image hashes against
pristine upstream — no package diffs against a sibling's output.

**The mechanical picture and the findings disagree, and that is the
point.** The gate is green in both configurations while findings A and G
are live in the solo configuration the gate builds *first*, and findings
B, C, J and K are live in both.

---

## 3. CONFIRMED findings

Ranked by what they cost the owner. Every entry carries the reaching
input, the violated contract quoted, the bug class, and the class-level
test. No patches.

---

### A1 — `ppcInvalidate(true)` reads a POST-dispatch register as the finished formula's result

**Where.** `packages/pretty-print/prettyCapture.c:454` (the emit), reached
from `:999-1000` (`case PPC_INVALIDATE: ppcInvalidate(true);`).

**What breaks.** Every emit-with-register call site in the package runs at
STAGE, before `indexOfItems[func].func(param)`. This one runs at DONE,
after. The classifier's default rule sends every unmodelled `US_ENABLED`
item to `PPC_INVALIDATE`; the STAGE arm for that class is deliberately
empty ("applied at DONE — the dispatch may still error out"); so by the
time the loop at `:452-457` finds `ppcSlot[k] == ppcCurrent` and calls
`ppcEmit(ppcCurrent, REGISTER_X + k)`, the register it names holds the
new item's output. The history entry is written with that number as the
finished formula's `= result`.

**Reaching input.** `2 ENTER 3 . 7 +` (X = 5.7, current formula
`2 + 3.7`), then **IP** (items.c row 93, `fnIp`, `CAT_FNCT | US_ENABLED`,
no explicit case in `ppcClassify`, not `fnConstant`). Executed in the
package's own harness: the emitted entry decodes as **`2 + 3.7 = 5.`**.
Any unmodelled `US_ENABLED` item that writes X reproduces it — IP, FP,
RAN#, `->REAL`, RND, Σ+, the statistics keys. `2 ENTER 3 + RAN#` files
`2+3 = 0.4712…`.

**Violated.** `DESIGN.md` §3, BINDING: *"shadow slot k always holds an
expression whose value equals the live contents of register `REGISTER_X + k`
… The display never lies; over-invalidation only costs history
granularity."* And §4 rule 2, which is where the emit's result is
specified: *"its `= result` read from the register that still holds it —
the §3 invariant guarantees truth"*. `ppcInvalidate`'s own comment at
`:451` states the false premise as fact: *"the current formula's root sits
on some slot; that register holds its value."* The designed truthful
alternative exists and is used elsewhere — `ppcDisplaced(slot, false)`
passes `resultReg = -1` and `ppcEmit`'s header says *"pass -1 when the
value has already left the stack"*, which `DESIGN-HISTORY.md:718-720`
rules for the top-of-stack falloff case.

**Bug class.** *Hardened arm, exposed sibling* (r4→r5 P-B/G2) crossed with
a new one this package earns: **result snapshot taken on the wrong side of
the dispatch**. Every sibling call site of the idiom establishes the
precondition; the one added later does not, and nothing re-checks.

**Class test.** T14 already drives this and asserts only
`ppcHistoryCount()`. The class test is a sweep: for every classifier
outcome that can emit (`PPC_CLX`, `DROP`, `CLSTK`, `XSWAPREG`, `BIGOP*`,
`INVALIDATE`, supersession), drive a formula, capture the pre-op register
value, run the item, then **decode the emitted entry's TKRES** and assert
it equals the pre-op value or that no TKRES was written. `ppcTestExpectHist`
compares counts only and cannot see this class.

**Verification.** SURVIVES on three lenses. Reachability constructed the
whole chain and then executed it: probe output
`prettyPrint test FAIL: PROBE result truth (expected '[[2 + 3.7] = 5.7]',
actual '[[2 + 3.7] = 5.]')`, sole failure in a 13,022-test run. Both
intent verifiers searched `DESIGN.md`, `DESIGN-HISTORY.md`, `TESTING.md`,
the PP3 commit `db495d984` and the code comments for a ruling that
sanctions the stale read and found none — and found the project ruling
the *identical shape* a defect at PP12 (the ∫ SETUP form whose "result"
was whatever X held, `DESIGN-HISTORY.md:428-437`, "truthful-looking,
wrong"). One verifier also mutated the emit to `-1` (the conservative
no-result form) and the gate stayed green, proving the current behaviour
is not pinned in either direction.

---

### A2 — In any build without forth-core, every key inside the CM-20 formula browser hits the firmware bug screen, and EXIT returns to the browser

**Where.** `packages/pretty-print/keyboard.c:1681` (the resolution list
that has no arm for 20) and `:1691-1693` (the terminal `else`).

**What breaks.** `prettyBrowser()` sets `calcMode = CM_PRETTY_BROWSER`
(20). `btnPressed` calls `determineItem` unconditionally. 20 matches
neither the hex branch, the AIM/catalog branch, `tam.mode`, nor the
`CM_NORMAL | CM_NIM | … | CM_LISTXY` list, so control reaches
`displayBugScreen(bugScreenItemNotDetermined)`, which saves
`previousCalcMode = 20` and sets `CM_BUG_ON_SCREEN`. `fnKeyExit`'s
`CM_BUG_ON_SCREEN` arm restores `previousCalcMode` — back to 20 — and the
next key bug-screens again. The resolution that makes 20 work lives
entirely in a sibling: `packages/forth-core/keyboard.c:1847`,
`|| (calcMode >= 20 && calcMode <= 23)`. undo-history's amendment covers
the shift block and the key-containment lists, not the resolution chain
(its only new branch tests `CM_HIST_BROWSER == 19`).

**Reaching input.** Solo pretty-print — `./packages/pretty-print/build-test.sh --solo`,
which is the gate's own **first** pass and runs by default. `DISP → PP →
PHIST`, then any key. Measured, three times independently, through the
real `btnPressed`/`btnReleased` path with a B9-style harness:
`after PHIST calcMode=20` → `after UP keypress calcMode=10` (=
`CM_BUG_ON_SCREEN`) → `after EXIT calcMode=20` → `after second UP
calcMode=10`. pretty-print + undo-history without forth-core fails the
same way. pretty-print + forth-core without undo-history resolves keys but
loses the containment guard, so unhandled keys act on the machine under
the browser.

**Violated.** `DESIGN-HISTORY.md:540-542` asserts the opposite:
*"Solo-pretty-print (no forth-core) resolves nav keys through the final
else (primaryAim == primary for them); only .d-pan may differ solo —
documented quirk, direct-call pins unaffected."* The final else is not a
key plane; it is `displayBugScreen(bugScreenItemNotDetermined); result = 0;`.
(The author appears to have conflated it with the `: key->primaryAim`
default *inside* the AIM branch.) The same section concedes the coverage
half two paragraphs earlier: *"Keyboard-case reachability is proven
structurally (the handlers are 3-line breaks) … A B9-style real-keypress
harness remains open work."* `DESIGN.md` §7's registry still reads
"calcMode | **20 reserved** (not wired)", so nothing anywhere records
that the browser depends on a sibling package.

**Bug class.** *One-door seam* (r11 R11-1) — a mode registered at six of
eight doors, where the two unregistered doors are the ones that decide
whether a key becomes an item at all. Compounded by *the harness enters
below the layer where the bugs are* (r11): FV5/FV12 call
`prettyBrowserUp/Down/Enter/Leave` directly, so no pin can see it. The
sibling that faced the identical problem for calcMode 19 **did** add the
branch (`packages/undo-history/keyboard.c:1637`), so the requirement was
known and paid for once already.

**Class test.** A real-keypress harness for CM 20, copy-adapted from
`packages/undo-history/undoHistory.c:1470-1541` (restore `kbd_usr`, clear
`FLAG_USER`/`tam`/SHOW, drain `fnTimerExec`), driving `btnPressed` for
**every key** and asserting `calcMode` is 20 or `pbPreviousCalcMode`,
never `CM_BUG_ON_SCREEN` — run in **both** gate configurations. The
class is "every calcMode a package claims must be resolvable and
containable in every configuration the gate builds": enumerate the doors
(`determineItem`'s list, `processKeyAction`'s master switch default) and
assert each package's claimed modes appear in both, with a count check.

**Verification.** SURVIVES on five lenses, four of them with a live
probe. Attempted refutations that failed: a modal key loop inside
`prettyBrowser` (there is none — it paints and returns); an early return
in `btnPressed` (the only ones are SHOW-menu dismissal, double-click and
program-running, none of which apply); `catalog` non-zero under MNU_PP
(every assignment is `CATALOG_*` in `calcMode.c`; MNU_PP is an ordinary
softmenu); a resolution hunk hiding in the generated patch (the patch has
exactly six `case CM_PRETTY_BROWSER:` hunks, all in `fnKey*`, and the
built shadow tree at `build.sim/custom_pkg_shadow/keyboard.c` confirms
it); solo not being a supported configuration (`build-test.sh:97` runs it
first). One scope correction: "cannot get back to CM_NORMAL without a
reset" overstates it — `CM_BUG_ON_SCREEN` *is* in the resolution list, so
a digit key reaches NIM entry and escapes. **EXIT**, the documented way
out, provably loops.

---

### A3 — `prettyNoteFunctionDone()` has no scope guard, so a NESTED dispatch consumes the outer item's STAGE and answers the error question at the wrong instant

**Where.** `packages/pretty-print/prettyCapture.c:754-757` — the whole
guard is `if(!ppcStage.valid) return;`.

**What breaks.** PP12 made `prettyNoteFunction` nesting-safe by checking
scope **before** touching the stage (`:616-625`), which leaves the outer
stage armed during a nested dispatch — deliberately. Nothing gave the
same treatment to DONE. `items.c:411-416` calls both around **every**
dispatch, including the steps `execProgram` runs under `FLAG_SOLVING`. So
the first successful step of a Σ/∏/∫/SOLVE label program reaches DONE
with the outer item's stage still valid and `lastErrorCode` still
`ERROR_NONE`: the BIGOP transform is applied mid-loop, and when the loop
later aborts, the outer DONE finds `valid == false` and does nothing. The
error arm at `:759-766` — free the staged limit leaves, `ppcInvalidate` —
never runs for this entire class of operations.

**Reaching input.** Global alpha label `Q` = `LBL "Q" / +/- / √x / END`,
`FLAG_CPXRES` clear so `√(−1)` raises
`ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`. Keys `1 ENTER 3 ENTER 1 Σn Q`.
Measured: `err=1 sig='{#,#}Σn'` — the sum aborted and the shadow still
carries a Σ node in slot 0 asserting its value is whatever X holds (the
leftover loop counter, 1). The next displacing key writes that into
history: `hist before CLX = 0`, `hist after CLX = 1`. Two probe counters
show the same consumption happens in the **passing** B1 fixture
(`nestedDone=1` on the success path too) — it is invisible there only
because early application and correct application are indistinguishable
when nothing fails. Same shape via `2 ENTER 3.7 + EQN→Calc f`, where
`parseEquation`'s `_runDyadicFunction` calls `runFunction(ITM_MULT)` and
the nested DONE fires the outer `PPC_INVALIDATE` while X holds an
intermediate.

**Violated.** `DESIGN.md` §3: *"DONE applies the staged transform only
when `lastErrorCode == ERROR_NONE`, else discards it and invalidates the
shadow (a failed function may have partially moved the stack)."* And §3's
own nesting claim, *"The capture hooks are nesting-safe:
`prettyNoteFunction` checks scope BEFORE touching the stage"* —
`DESIGN-HISTORY.md` PP12 records fixing exactly that half of the pair.

**Bug class.** *Hardened arm, exposed sibling* (r4→r5 P-B/G2), textbook:
two hooks, one decision, one of them hardened and the other left with the
same shape untested. Also *Two-half gesture with the second half
conditional* (r10 R10-2) in its consumption pairing — the DONE hook has no
scope check, no depth counter, and no identity tie to the STAGE that armed
it.

**Class test.** For every classifier outcome that stages a transform,
drive a nested dispatch that (a) succeeds and (b) errors on step ≥2, and
assert in case (b) that the shadow is invalid and history is empty.
Structurally: pin that a DONE consumes only a stage armed by the *same*
dispatch (token equality or a depth counter), and assert the pairing over
the whole classifier table with a count check. The suite today is green
both with and without the defect: a verifier applied the one-line scope
guard and the **entire solo gate stayed green**, B1/B5/B6/B7 included —
so the guard is not over-broad and the failure side of the contract has
zero coverage (*Success-only coverage of a two-sided contract*,
undo-history r5 R16).

**Verification.** SURVIVES on two correctness lenses, both with runtime
probes, and one of them ran the counter-test above. Correction to the
finding as written: the literal keystrokes cited a **local** label A,
whose param falls below `FIRST_LABEL`, so that case degrades to
`PPC_INVALIDATE` at STAGE — still armed, still consumed nested, but the
consequence is an early invalidate rather than a lying node. A global
alpha label produces the stated wrong result.

**Fix ordering note.** A3 and A1 interact: fixing A3 alone makes A1 fire
at the *outer* DONE instead of the inner one. They need fixing together,
and the shared root is worth naming before either is touched — the
capture engine trusts a register to still mean what it meant one moment
ago.

---

### A4 — `STO` to a STACK register leaves the shadow slot claiming the register's old value

**Where.** `packages/pretty-print/prettyCapture.c:497` (`PPC_STO_NOP`
classification) and `:893-894` (the DONE arm, a bare `break`).

**What breaks.** The classifier files `ITM_STO`/`STO±×÷` as "register-side
only: the stack does not move", which is true about motion and irrelevant
to the invariant, which is about **values**. `STO Y` writes X into Y while
both STAGE and DONE do nothing; `ppcEnsureKnown(1)` cannot repair it
because slot 1 is already "known". The next dyadic reuses the stale tree
verbatim.

**Reaching input.** `7 ENTER 2 ENTER 3 + STO Y ×`. The STO target is one
softkey: `menu_TamSto[]` lists `ITM_REG_X..ITM_REG_T` as softkeys 3-6 of
the STO prompt, and `tam.c` sets `tam.value = indexOfItems[item].param`
with `tryOoR = true` for register letters. Measured through the real key
paths: `after STO Y: reg 101 = 5`, `after ×: reg 100 = 25`, current
formula signature `7 2 3 + ×`. The display shows `7·(2+3) = 25`.
7·(2+3) is 35.

**Violated.** `DESIGN.md` §3, BINDING (quoted in full under A1). Note
what the classification costs: `ITM_STO` is undo-enabled, so **without**
the explicit case the binding default rule would have invalidated the
shadow and been safe. The hand exception is what creates the hole. The
same file already knows stack registers reach these params —
`PPC_RCLCLS`/`PPC_RCLARITH` both test
`param >= REGISTER_X && param <= getStackTop()` and deep-copy the slot
(`:899`, `:923`); `PPC_XSWAPREG` handles the swap case. STO is the
unmirrored site.

**Bug class.** *Origin-vs-openness confusion* (r5), in its general form: a
predicate answering "what kind of operation is this" (register-side, no
stack motion) read as "does this falsify a shadow slot". Also *Predicate
widened for one consumer, others unchecked* (r1 N-T4 / r5 R3) — the
RCL family got the stack-register test; the STO family did not.

**Class test.** Enumerate every classifier row whose param can name a
stack register (`RCL`, `RCL±×÷`, `STO`, `STO±×÷`, `x<>reg`, and any
future addition) and drive each with `REGISTER_X..T`, asserting after the
dispatch that slot k's tree still evaluates to `REGISTER_X + k` or that
the shadow invalidated. Back the enumeration with a count check against
the classifier table (*Enumeration without a count check*, r2).

**Verification.** SURVIVES on correctness, with a live probe. Four
refutation attempts failed: reachability of a stack register as a STO TAM
parameter (`menu_TamSto` lists them literally); a late refresh of the slot
(`ppcCurrentRevalidate` compares nothing against a register); a design
ruling exempting STO (`DESIGN-HISTORY.md:569` only restates
"STO and STO±×÷ stay stack-silent (PPC_STO_NOP), as before"); a validity
check before render (there is none). One strengthening: the damage is not
confined to the T line (PTLIN is default-off) — `ppcEmit` later serializes
that same lying root into PHIST with a result snapshot from the register,
so the false formula persists in history.

---

### A5 — `US_CANCEL` is classified "ignore", so `LOAD`/`LOADST` replace the whole stack while the shadow keeps describing the old registers

**Where.** `packages/pretty-print/prettyCapture.c:546` (the default arm of
`ppcClassify`).

**What breaks.** The default rule invalidates only on
`US_ENABLED`/`US_ENABL_XEQ`; everything else, including `US_CANCEL`, is
`PPC_IGNORE`. But upstream's own header says the opposite of what the
rule assumes: `defines.h:1111`, `US_CANCEL` = *"The command cancels the
last UNDO data"* — i.e. the machine moved beyond what undo can describe —
against `US_UNCHANGED` = *"leaves the existing UNDO data as is"*, the
harmless one. `items.c:319` implements `US_CANCEL` by setting
`thereIsSomethingToUndo = false`.

**Reaching input.** `2 ENTER 3 +` (shadow slot 0 = `2+3`, X = 5), then the
I/O softmenu → **LOAD** (item 1509, `US_CANCEL`). `fnLoad(LM_ALL)` →
`doLoad` → `restoreOneSection`, whose `loadMode == LM_ALL` disjunct admits
`REGISTER_X..T`; say the file has X = 42. Then `7 ×`: the literal lifts,
the stale `2+3` tree moves to slot 1, the dyadic builds `(2+3)×7` and
`ppcEmit` reads the live X = 294. History and the browser show
`(2+3)×7 = 294`; ENTER recalls 294 as the value of a formula worth 35.
`LOADST` (item 2388) is the same shape. Other unscreened `US_CANCEL`
stack mutators in the same bucket: `fnIsPrime` (1859), `fnInput` (1869),
`fnClearRegisters` (3287).

**Violated.** `DESIGN.md` §3, Default rule (BINDING): *"an unknown
`US_UNCHANGED`/`US_CANCEL` item is ignored — upstream maintains that
annotation for its own undo correctness, so it maintains our invalidation
predicate too."* The premise is inverted for `US_CANCEL`, and the
justification the doc gives ("display/mode chatter") is exactly what
`US_CANCEL` is not.

**Bug class.** *Origin-vs-openness confusion* (r5) again, at the level of
a borrowed annotation: an upstream flag answering "what does this do to
the undo record" read as "does this move the stack". Sibling of A4 and A6.

**Class test.** Enumerate every `US_CANCEL` and `US_UNCHANGED` item whose
implementation writes any of `REGISTER_X..T` (a grep over the
implementations, machine-derived, not a hand list), assert each either
invalidates the shadow or appears on the hand-exception list, and pin the
count. The doc's exception hunt was scoped to *"stack mutators that are
`US_UNCHANGED`"*, so the `US_CANCEL` bucket was never screened at all.

**Verification.** SURVIVES on intent. A repo-wide markdown grep for
`US_CANCEL` returns exactly one hit — `DESIGN.md:155`, the rule under
attack. Greps for `LOADST`/`fnLoad`/`LM_ALL`/`saveRestore`/`ITM_LOAD` over
`design-docs/pretty-print/` return nothing; `git log -S"US_CANCEL"` over
the whole range shows the token entered the docs once, in the initial
docs commit, and was never revisited across 29 commits. There is no
ruling to cite, and the §3 invariant says the opposite.

---

### A6 — `R/S` is a `US_UNCHANGED` stack mutator missing from the classifier's hand-exception list

**Where.** `packages/pretty-print/prettyCapture.c:529` (the classifier
switch, which has no `ITM_RS` case).

**What breaks.** R/S is item 1725, `fnRunProgram`, `US_UNCHANGED` →
`PPC_IGNORE`. STAGE clears the stage and returns; the program then runs
with `programRunStop = PGM_RUNNING`, so every step's STAGE bails on scope
and every step's DONE bails on `!valid`. The program rewrites the stack
and the shadow is never told. `ITM_XEQ` (item 3) is `US_ENABLED` and is
therefore handled — R/S is the same operation reached by the other key.

**Reaching input.** *(Corrected by verification — the finding's original
sequence does not reproduce.)* `GTO A` is `US_ENABLED` and invalidates,
so `… GTO A, R/S` displays truthfully. The live path is the classic
prompt-and-continue idiom: a program that `STOP`s, the user computes, R/S
resumes. Measured: `beforeRS='2 3 +' afterRS='2 3 +' X_afterRS=25`, then
`2 ×` gives `end='2 3 + 2 x' X_end=50` — a displayed formula worth 10
beside a real X of 50, and `ppcEmit` files `(2+3)·2 = 50` into history.
Any R/S with no `US_ENABLED` item immediately in front of it reproduces.

**Violated.** `DESIGN.md` §3: *"Hand exceptions (stack mutators that are
`US_UNCHANGED`): `ITM_UNDO`, undo-history's REDO (item 428)."* R/S is
exactly a `US_UNCHANGED` stack mutator and is not on the list; upstream
marks it UNCHANGED because the *run*, not the program, is what undo
ignores. `DESIGN-HISTORY.md:438-443` already rules the neighbouring case
the right way — after a label program runs, *"only X can be vouched for …
every register but the result is somebody else's writing"* — and a
keyboard R/S is that situation with no result to vouch for at all.

**Bug class.** Same as A5. The hand-exception list is *Enumeration
without a count check* (r2): a human list of the items that violate the
borrowed annotation, and it came back short.

**Class test.** Shared with A5 — the machine-derived enumeration finds
R/S automatically. Additionally: pin that the shadow is invalid after any
transition into or out of `PGM_RUNNING`, which closes the class rather
than the instance.

**Verification.** SURVIVES on correctness with two live probes
(`PROBE-RS` disproving the stated sequence, `PROBE2-RESUME` establishing
the real one). No package hook exists at program start or end — diffs of
`keyboard.c` and `calcMode.c` against upstream add only the CM-20 arms and
the NIM-open latch.

---

### A7 — The browser's horizontal pan cannot engage, and every row wide enough to need it is dropped instead

**Where.** `packages/pretty-print/browsers/prettyBrowser.c:81` (the pan
arm) against `packages/pretty-print/prettyFormula.c:653` (the row
builder's accept gate).

**What breaks.** `ppfBuildRow` accepts a row only when
`n->width <= SCREEN_WIDTH - 8` (392 px), at the standard rung or the tiny
rung; otherwise it returns false and **both** callers `continue`
(`prettyBrowser.c:52/69`, `prettyFormula.c:700/722`). The pan arm fires
only when `n->width > SCREEN_WIDTH - 12` (388 px). So the entire pannable
band is widths 389..392 → `maxPan ≤ 4`, while `prettyBrowserPan` advances
`pbPan` by 60, which the very next paint wraps to 0. `.d` does nothing,
ever. And anything wider than 392 px at both rungs is not panned, not
truncated, not marked — it is absent.

**Reaching input.** An ordinary long formula. Measured: three 16-digit
numbers added together renders 724 px at the standard rung and 550 px at
the tiny rung, both rejected. With that entry as the only row, the
browser's content band (`y` 21..167) came back with **zero lit pixels** —
not even the "no formulas" string, because `totalRows == 1` so the
empty-state arm never fires. The owner's calculation is simply gone from
the history it was told is there.

**Violated.** `prettyPrint.h:36-38`: *"`PHIST` … opens the formula BROWSER
(calcMode 20 — UP/DOWN select, **.d pans a wide row**, ENTER recalls the
result to X, EXIT leaves)"*; `prettyBrowser.c:9-10`: *"the selected row …
pans horizontally when wider than the screen"*;
`DESIGN-HISTORY.md:524-525`: *".d pans a too-wide selected row (wraps)"*.

**Bug class.** *Scope-mismatched predicate pair* (D-C3): two tests on the
same quantity, in two modules, whose ranges do not overlap, each with
dependents. The row builder's contract is inherited from the pager, where
dropping a non-fitting row is right; the browser's pan arm assumes the
opposite precondition and neither side names the other. *Clamp without
its viewport* (C12) is the layering half.

**Class test.** A property pin: every root admitted by `ppfBuildRow`
satisfies the pan arm's precondition, asserted over a generated sweep of
widths, so the two ranges cannot drift apart again. Plus a fixture with a
formula wider than both rungs asserting the browser shows *something* —
marker, ellipsis, truncation — and never an empty band. No test today
calls `prettyBrowserPan` at all (its only caller is `keyboard.c:4960`).

**Verification.** SURVIVES on reachability, with the construction built
and run. Note for whoever fixes it: `showString` takes a `uint32_t` x, and
today `x = 8 - pbPan` stays positive only *because* `pbPan` can never
exceed 4 before wrapping. Letting wide rows through without fixing the
pan arithmetic makes a negative coordinate reachable.

---

### A8 — Solo build: `NUMBER_OF_SYSTEM_FLAGS` is 115 but the generated `menu_SYSFL[]` has 112 entries

**Where.** `packages/pretty-print/defines.h:1021`.

**What breaks.** The count and the catalog rows are split across two
packages on purpose — `DESIGN.md` §7: *"BOTH packages carry the
byte-identical `64+51` line … Both SYSFL catalog rows (`PPRTY`, `PTLINE`)
live in UNDO-HISTORY's items.c"*. In the solo configuration the count edit
lands in a package that does not carry the rows.
`browsers/flagBrowser.c` is not overridden by any package and still uses
the count as the array bound: `if(f+fOffset > NUMBER_OF_SYSTEM_FLAGS - 1) break;
systemFlag = menu_SYSFL[f+fOffset];`.

**Reaching input.** Solo pretty-print, then `STATUS, UP, UP` (or `FLGS`
then `DOWN`) to reach `SYSTEM_FLAGS_SCREEN_2`, where `fOffset = 60` and
the loop evaluates `menu_SYSFL[112]`, `[113]`, `[114]`. Verified by
configuring the solo build and reading the generated header: 112 entries,
and indices 112/113/114 land on `menu_alpha_INTL[0..2]` = 667, 665, 670
(byte delta exactly `112 * sizeof(int16_t)`). Item 667 is `ITM_A_GRAVE`,
so the cell label is `sprintf`'d as an accented A and
`getSystemFlag(667)` evaluates `(uint64_t)1 << (667 - 64)` — a shift by
603, undefined behaviour. Three softkey cells, drawn on screen.
`OPTION_FLAGBROWSER` is defined for both the PC sim and the DM42n target
profile. The combined build is consistent (115 rows against 115) — the
defect is solo-only, and the solo pass is what `build-test.sh` runs first.

**Violated.** Upstream keeps the two in lockstep by construction:
`#define NUMBER_OF_SYSTEM_FLAGS 64+48` with exactly 112 `CAT_SYFL` rows.
Verified counts at the tip: `src/c47/items.c` 112,
`packages/pretty-print/items.c` 112, `packages/undo-history/items.c` 115.

**Bug class.** *Hand-maintained inventory of a machine-derivable set*
(C15) crossed with *Constant copied by value across a module boundary*
(C14) — except here the two halves are not merely copied, they are
deliberately owned by different packages, which removes the only thing
that kept them equal.

**Class test.** `_Static_assert(sizeof(menu_SYSFL)/sizeof(menu_SYSFL[0]) == NUMBER_OF_SYSTEM_FLAGS)`
in a TU that includes the generated header — the header is a build-time
`custom_target` over the shadow `items.c`, so the assert fires per
configuration and cannot be satisfied by a comment. The general class:
any claims-registry split that puts a count in one package and its rows in
another needs a build-time assert in every configuration the gate builds,
not a doc sentence.

**Verification.** SURVIVES on reachability, proven empirically:
`meson setup build.sim -DCUSTOM_PKG=packages/pretty-print`, then
`ninja src/generateCatalogs/softmenuCatalogs.h`, then a throwaway TU
outside the repo printing the array size and the three past-the-end
values. The attempted refutation — that the combined build makes this
moot — instead confirmed the scope: combined is 115 against 115.

---

### A9 — `pbFindResult` has no `PPT_TKBIG` arm, so ENTER never recalls any history entry containing a Σ/∏/∫

**Where.** `packages/pretty-print/browsers/prettyBrowser.c:174`
(`default: return NULL`).

**What breaks.** `ppcSerializeNode` writes `PPT_TKBIG` (token 8) and
`ppcEmit` appends `PPT_TKRES` after it. Two decoders read that one
stream: `ppfBuildEntry` (`prettyFormula.c:562`) knows all eight tokens and
renders the row correctly; `pbFindResult` knows seven and bails on the
eighth, before reaching the TKRES that follows. Its contract comment says
NULL means "absent"; here the result is present.

**Reaching input.** `1 ENTER 5 ENTER 1 Σn(LBL)` to mint a BIGOP as the
current formula, then any new formula (`2 ENTER 3 +`) to supersede it,
which serializes the Σ entry with a TKRES (`ppcSupersedeCurrent` passes
`REGISTER_X + slot`). PHIST, DOWN to the Σ row — it renders with its
`= result` — ENTER. The browser closes and nothing lands in X. No error,
no message. Every neighbouring row recalls fine. Affects any formula
whose tree contains a big operator, including as a sub-term.

**Violated.** `prettyBrowser.c:12-18`: *"ENTER stages the selected history
entry's TKRES result into X … The live (now) row has no stored result —
its value IS X — so ENTER there just leaves"* — one exception, and this
is not it. `prettyPrint.h:37`, `prettyBrowser.h:8` and
`DESIGN-HISTORY.md:525-529` all restate the promise unconditionally. The
only documented no-result class is the top-of-stack falloff
(`resultReg = -1`), which is orthogonal.

**Bug class.** *Emit/accept parity violation* (D-C1, FIX-7) — the decoder
refuses a spelling its own emitter writes. Chronology confirms omission
rather than intent: `pbFindResult` landed in PP10 (`c6f85a267`),
`PPT_TKBIG` in PP12 (`31ca8821e`), and the PP12 commit touches no file
under `browsers/` while its stage sheet closes with "PP12 is entirely
package-internal (capture, layout, formula, tests)".

**Class test.** The catalogued one: a round-trip sweep over every
decodable form. For each token in `prettyInternal.h:87-94`, build an entry
containing it and assert `pbFindResult` locates the TKRES. Today the
suite's only `prettyBrowserEnter()` call is FV12, which recalls a plain
`2 ENTER 3 +` row.

**Verification.** SURVIVES on two intent lenses. Both searched `DESIGN.md`
§5/§6, the PP10 and PP12 history entries, `TESTING.md`'s mutation table
(MUT-35/36 cover recall staleness and selection clamp; MUT-39/40/41 cover
TKBIG capture and layout — nothing covers their intersection) and the
PP16 "three deferred items, closed" list (complex SUM/PROD results, Σ∞,
softkey indicators — not this). `grep -rn TKBIG design-docs/` returns
exactly one hit, the wire format.

---

### A10 — A number typed as exactly 31 characters is stored in the shadow literal one character short

**Where.** `packages/pretty-print/prettyCapture.c:1084` (the continuation
slice) against `:1035` (the accept gate).

**What breaks.** The gate is `n >= sizeof(ppcNimText)` = 32; the storage
is two 15-byte payloads = 30. Length 31 is the one value that is admitted
and truncated: `rest = (len - 15 > 15 ? 15 : len - 15)` = `(16 > 15 ? 15 : 16)`
= 15, and `aux` records 15, so nothing downstream can tell. The same
15+15 clamp is repeated in the serializer (`:284-297`) and both renderers
(`prettyFormula.c:381-389`, `prettyTest.c:676-686`), so the history copy is
wrong too.

**Reaching input.** Type 31 digits, `ENTER 2 +`. Measured sweep through
the real NIM path:

```
PROBE L=30 aim='+123456789012345678901234567890'  sig='123456789012345678901234567890 2 +'
PROBE L=31 aim='+1234567890123456789012345678901' sig='123456789012345678901234567890 2 +'   <- trailing 1 lost
PROBE L=32 aim='+12345678901234567890123456789012' sig='-'   (formula withheld, no lie)
```

The T line, the PHIST pager and the browser all render the operand ten
times smaller than the one that was typed, beside a correct result. Also
reachable as `-` plus 30 digits, since only a leading `+` is stripped.

**Violated.** `DESIGN.md` §3: *"Literal leaves store **as-typed text**
(`2.50` stays `2.50`)"*, under the binding *"The display never lies;
over-invalidation only costs history granularity."* The gate's own comment
states the intent it fails to implement — *"too long: the leaf will fall
back to a value"*.

**Bug class.** **Accept gate sized to the scratch buffer, not the
storage** — the boundary sibling of C20's *silent narrowing before a range
gate*. The gate is two off, not one: the real ceiling is 30.

**Class test.** Sweep literal lengths 1..40 through the real
`addItemToNimBuffer`/`closeNim` path and assert the shadow's literal text
equals what was typed **or** the leaf declined — never a silent
truncation. Today no test types a literal longer than four characters
(`"2.50"` is the longest in the whole file), so the continuation path is
unpinned for structure and unpinned for boundary.

**Verification.** SURVIVES on correctness, with the sweep above. One
refinement that does not rescue it: a 32-character entry does **not** fall
back to a value leaf as the finding claimed — the value snapshot also
fails (the long-integer payload exceeds the 16-byte node payload), the
leaf becomes `PPC_UNKNOWN` and `ppcTreeHasOpaque` withholds the whole
formula. That is still truthful (over-invalidation is explicitly
permitted). Only length 31 lies.

---

### A11 — `PSHOW` is missing the `!checkHP` conjunct the inline surface has, while its font ladder uses `numericFont`

**Where.** `packages/pretty-print/prettyValue.c:834` (`fnPrettyShow`,
whose only guard is `lastErrorCode != ERROR_NONE`), against `:780`
(`prettyTryRegisterLine`, which carries
`|| checkHP // HP layout doubles glyph rows; our metrics assume it off`).

**What breaks.** `ppFullRungs` supplies `PP_FONT_NUMERIC` for rungs 0-2
(`:768-772`), and `prettyLayout.c` maps that to `&numericFont`, measures
with `stringWidth` and paints with `showString`. In `showGlyphCode`,
`numDouble = font == &numericFont && checkHP && temporaryInformation == TI_NO_INFO`
turns on both horizontal and vertical doubling, and HPFONT substitutes
glyph codes. `stringWidth` compensates horizontally, so `ppMeasure`
succeeds and the `fnC47Show` fallback never fires — the owner gets a
garbled screen with roughly twice the budgeted height: numerator and
denominator overlapping each other and the fraction bar, ink past the
21..167 band.

**Reaching input.** The whole of `checkHP`: SDIGS ≤ 16, DSTACK 1,
exponent limit 99, decimal-point input default. Then from `CM_NORMAL`
with `TI_NO_INFO`, a proper fraction in X and `DISP → PP → PSHOW`.
(EQSHW, PHIST, the browser and the EQN strip are unaffected: they only
ever use standard/tiny, and `numDouble` requires `numericFont`.)

**Violated.** `DESIGN.md` §6 makes `checkHP` a gate for exactly this
reason: *"**`!checkHP`** (HP layout doubles glyph rows inside
`showGlyphCode`, invalidating all metrics — HP users get upstream
rendering)"*. The rationale is a property of `ppMet` + `&numericFont`, not
of the inline surface.

**Bug class.** *Safety proof scoped to one caller, reused by others*
(r5) / *Rule corrected in a subset of its copies* (r5 R12). The one
PSHOW/inline asymmetry that **was** deliberate is recorded twice
(`FLAG_PRETTYP`, "asking to see something should show it",
`DESIGN.md:439-442` and `DESIGN-HISTORY.md:86-89`) — this project writes
down the gates it drops on purpose, and this one is not written down.

**Class test.** A gate-conjunct census across surfaces: for every surface
that can request `PP_FONT_NUMERIC`, assert `!checkHP` is in its guard
chain, and pin it with a fixture that sets the four `checkHP` settings and
asserts the surface declines (falls back to `fnC47Show`).

**Verification.** SURVIVES on intent — the omission is undocumented in
`DESIGN.md`, `DESIGN-HISTORY.md`, `TESTING.md`, every source comment and
all 29 commit messages; `checkHP` appears in the design corpus exactly
once, at `DESIGN.md:256`. **Confidence medium**, and the reason matters:
the intent verdict establishes that nobody decided to drop the conjunct,
not that the doubled metrics actually garble the screen. Nobody rendered
it. Settling it costs one PSHOW screenshot with HP layout on.

---

### A12 — The EQN renderer accepts a 5-argument `INTEG` that the evaluator rejects

**Where.** `packages/pretty-print/prettyEquation.c:249-260` — the
optional-fifth-slice block is not gated on `kind`.

**What breaks.** For `kind == 3` (INTEG) the block parses a fifth slice
into `stepN`, the `kind == 3` arm never reads it, `ppqEat(')')` succeeds,
and the construct renders as a well-formed ∫. The evaluator's arity gate
is stricter (`needMax = 4` for kind 3), so CALC raises
`ERROR_SYNTAX_ERROR_IN_EQUATION` (45). The stray argument is invisible in
the rendered form.

**Reaching input.** Type and store `INTEG(X;X;0;1;9)` — the `;` is
typeable from `menu_alphaMisc`, which the passing test EQ29 proves end to
end. ENTER commits it: the MVAR-mode parse consumes the whole `NAME(...)`
span by paren depth without slicing or counting, so the user is never
bounced back to the editor (measured: `MVAR commit gate err=0`). EQSHW
then reads the stored text with no validation and paints
`B([x d x]|0|1)` — a clean integral from 0 to 1 of `x dx`, measured
`w=53 h=38`. CALC on the same equation: `err=45`.

**Violated.** `DESIGN.md:435`: *"Malformed constructs decline the whole
strip/EQSHW render (strict; the linear line remains)"*, and `DESIGN.md:384`
fixes the syntax as `INTEG(body;var;from;to)` with no optional slice.
`DESIGN-HISTORY.md:222` records Stan's round-5 ruling requiring render/eval
parity. SUM/PROD (5 max) and DERIV (4 max, order restricted to literal 1
or 2) are gated correctly; INTEG is the one arm whose renderer is looser
than its evaluator.

**Bug class.** *Structural rule spelled per-site* (r9 R9-5): the accepted
argument count is written twice, in two files, with nothing forcing
agreement — the inverse direction of the emit/accept parity class in A9.

**Class test.** For each construct kind, sweep argument counts 2..6 and
assert `renderer-accepts == evaluator-accepts` for every pair. One
predicate, one table, both consumers reading it.

**Verification.** SURVIVES on reachability, with two mutation runs. **One
correction to the consequence:** the equation **strip** does *not* show
the bogus integral — measured ascent+descent = 38 against the strip's
23 px band, so `prettyTryEquation` declines on size and the upstream
linear line runs, where the user would in fact see the `;9`. The defect
is **EQSHW-only**. That narrows the blast radius and leaves the claim
standing: the one full-screen 2D view where an argument-list typo should
be visible erases it. *(Incidental, seen while tracing: for `kind == 3`
the `stepN` node is allocated and never linked into the tree, so each
5-arg INTEG render strands arena nodes until the next `ppReset`.)*

---

### A13 — `DERIV` order is narrowed to `uint16_t` before the 1-or-2 range gate

**Where.** `packages/pretty-print/solver/equation.c:2047` *(the finding
said 2060; the cast is at 2047, the gate at 2048, the error at 2049)*.

**What breaks.** `order = (uint16_t)real34ToInt32(&argS)` truncates before
`if(order != 1 && order != 2) ppEqSyntaxError("DERIV order must be 1 or 2")`.
`real34ToInt32` is `decQuadToInt32` with `DEC_ROUND_DOWN`, so an in-int32
value arrives intact and only the cast narrows it.

**Reaching input.** `DERIV(X^3;X;3;65538)`, evaluated. Measured live
through `setEquation` + `fnEqCalc`: `err=0 type=1`, X = exactly 18 — the
second derivative of x³ at 3 — with no syntax error. Any order congruent
to 1 or 2 mod 65536 does it. The 2D render declines separately
(`ppqBigopConstruct` requires the literal text "1" or "2"), so the strip
falls back to the linear form and gives no hint the order was
reinterpreted.

**Violated.** The guard's own intent, spelled at the next line.

**Bug class.** *Silent narrowing before a range gate* (C20), verbatim.

**Class test.** The catalogued one: sweep `{1, 2, 3, 65537, 65538, 0x10001}`
and assert the syntax error for everything but 1 and 2 — a magnitude whose
truncation is in range is the whole point of the class.

**Verification.** SURVIVES on reachability, executed end to end through
the real user surface. **Severity latent**: it needs a deliberately
absurd order argument, and the wrong answer it produces is a *correct*
second derivative for a construct the user wrote wrong.

---

### A14 — B9's only assertion sits under an error guard that then swallows the error

**Where.** `packages/pretty-print/prettyTest.c:1156`.

**What breaks.**
`ppcTestOpParam(ITM_SIGMAnINF, bigLbl); if(lastErrorCode == ERROR_NONE) { ppcTestExpectSig(…); } lastErrorCode = 0;`
— when the dispatch raises, **zero assertions execute**, the error is
cleared, `ppTestFailures` stays 0, and the case file's `Out: EC=0` check
still matches. The pin cannot fail on the regression it exists for.

**Reaching input.** Not a user input — a maintenance input. Today the
dispatch succeeds. Any change that makes it raise takes the else path:
`_programmableSumProd`'s `ERROR_BAD_INPUT` ("Counter will not count to
destination") if the limit/step conventions move, or its complex/CPXRES
refusal. Proven by mutation: with `fnProgrammableSumInf` reduced to a bare
`displayCalcErrorMessage(ERROR_BAD_INPUT, …)` — the Σ∞ feature completely
dead — the solo gate came back **GREEN** (`Fail: 0`,
`PRETTY-PRINT GATE GREEN`), with the build log confirming the mutated TU
was recompiled into all three targets. Re-running the identical mutation
with only the guard removed turned the gate **RED**:
`B9 infinite sum captured (expected '{#,#}Σ∞', actual '-')` — which is
precisely the "falls back to invalidate" regression MUT-62 names B9 as the
pin of record for.

**Violated.** `TESTING.md` PP16: *"B9 — the early-stop sum
(`ITM_SIGMAnINF`) captures like any other sum"* and
*"MUT-62 | infinite sums fall back to invalidate | B9"*. A mutation-table
pin of record must be unconditional. B1/B6 in the same function assert
their dispatch's result unconditionally.

**Bug class.** *Upper-bound-only oracle* (r5 R9) in its general shape — an
assertion satisfied by not running. Compare the PP16 commit's own citation
of MUT-60, *"a pin green under its mutation is decoration"*, which is the
rule this hunk breaks one function away.

**Class test.** Sweep the battery for every assertion inside an
`if(lastErrorCode == ERROR_NONE)` and require an `else` that fails, or a
gate that asserts the guard's own precondition. Then re-run MUT-62
red-first: a pin lands with a red-first injection of the class it exists to
catch, or it does not land (r10 R10-3/4/5).

**Verification.** SURVIVES on reachability, by the double mutation above.
No ruling exculpates it — the guard appears in no doc and in no commit
message.

---

### What I would leave alone

If the goal were code that is correct rather than code that passes an
audit, four of these are not worth the owner's evening:

- **A13** (DERIV narrowing) — needs an order argument congruent to 1 or 2
  mod 65536. Nobody types 65538. The *class test* is worth more than the
  fix, and it is one line of sweep.
- **A12** (INTEG 5-arg) — the evaluator does reject it, so no wrong number
  ever reaches the user; the cost is a confusing error. EQSHW-only after
  the correction.
- **A14** (B9's guard) — no user impact today whatsoever. It is a
  maintenance trap, and its whole value is that the mutation proof is
  already written down here.
- **A10** (31-character literal) — one boundary value nobody reaches by
  accident. I list it because the invariant it breaks is declared BINDING
  and because the gate is two characters from correct, not because the
  owner will ever hit it.

**A11** (PSHOW/`checkHP`) is the one I would *not* leave alone despite the
medium confidence, because the fix is one conjunct copied from a sibling
line and the failure mode is a screen the user cannot read.

And the reverse: **A1 and A3 are not separable**. Fixing A3 alone moves
A1's stale read to the outer DONE. Whoever takes them should name the
shared class first.

---

## 4. PLAUSIBLE and UNVERIFIED findings

None of these went through the refutation pass — the verification cap was
reached. They are reported as the finders wrote them, with what would
settle each. Confidence is the finder's.

**P1 — The MVAR scan consumes the whole construct span, so variables
inside a construct never reach the MVAR menu.**
`packages/pretty-print/solver/equation.c:1943`. Type `SUM(A×X;X;1;3)` and
press ENTER; the MVAR arm scans to the matching `)` and returns the whole
span length, so the word scanner never sees `A` and the collector never
adds it. The solver/MVAR softmenu then offers no key for the equation's
one free variable. `DESIGN.md:396-399` (authoritative) says the opposite:
*"In MVAR mode it consumes ONLY the name and `(` … the arguments scan
normally"*. Round 5 changed the code and recorded it only in the
non-normative `DESIGN-HISTORY.md:236-238`, whose summary
("Construct-internal variables are not enumerated (they bind their own)")
is true of the loop variable and false of every other name in the body.
*Settles it:* store that equation, open the MVAR menu, count the softkeys.
Confidence medium; class *Rule corrected in a subset of its copies*
(r5 R12).

**P2 — Upstream's deliberately unexported `_fnIntegrate` is re-declared
inside the override, outside any header.**
`packages/pretty-print/solver/equation.c:1707` declares
`void _fnIntegrate(uint16_t, bool_t);` and `:1904` calls it. Upstream
defines it at `solver/integrate.c:67` and publishes only the
`fnIntegrate`/`fnPgmInt` wrappers — no prototype in any header (confirmed
by grep). A future `static` gives a clean link error; a changed parameter
type does not, because there is no header to cross-check the two
declarations, and the integrator would silently receive a garbage XY flag.
Upstream's own answer to "package code needs an unexported symbol" is the
seam-wrapper precedent (`paramCorePutLiteral`). *Settles it:* nothing, until
upstream moves — this is a merge-time hazard, and the decision is whether
to pay a 3-line wrapper now. Confidence medium.

**P3 — pretty-print's `items.c` hunk is one unchanged line from
forth-core's row-213 hunk, where `DESIGN.md` says "nowhere near either
sibling".** `packages/pretty-print/items.c:2045`. Reproduced rather than
theorised: all three packages' `items.c` patches apply cleanly today, but
turning upstream row 214 (the very next spare `CAT_FREE` slot) into a named
row inside forth-core's existing hunk yields
`pretty-print:1 -> "Applied patch to 'items.c' with conflicts"` with a
`<<<<<<<` marker in the shadow tree. forth-core's hunk spans upstream
2032-2038 and modifies 2035; pretty-print's spans 2034-2042 and modifies
2037-2039. One unchanged line between the changed regions is the minimum
separation at which 3-way does not conflict. `DESIGN.md:463` claims the
hunk is *"nowhere near either sibling"* and cites as precedent the very
hunk that is the neighbour. *Settles it:* it is settled — the finder
reproduced it; what is unverified is only whether the owner considers the
1-line margin acceptable. Confidence high.

**P4 — The identical-edit flag claim is specified with two different
values in its two owners' docs.** Both shipped `defines.h` overrides carry
`64+51`, byte-identical, which is the entire safety argument for the
claim. `design-docs/undo-history/DESIGN.md:180-182` still specifies
*"bumped to 64+50, reserving ONE flag for pretty-print's FLAG_PRETTYP"*
and `:167-169` still says *"NUMBER_OF_SYSTEM_FLAGS 112 -> 113"*. Whoever
next edits undo-history's `defines.h` from its own spec writes `64+50`
back, the two overrides stop being identical, `git apply --3way` sees two
conflicting edits to one line, and if `64+50` wins, `FLAG_PTLINE` (bit
114) falls outside the count and the T-line toggle stops persisting across
a state save. *Settles it:* read both docs side by side; it is a
documentary fact, not a runtime one. Confidence high. Sibling of A8 — the
same split claim, failing in the other direction.

**P5 — `prettyPrint.h`'s public contract comment still declares PPON a
non-persisted package bool, three stages after it became `FLAG_PRETTYP`.**
`packages/pretty-print/prettyPrint.h:21`: *"Not a system flag: two
packages cannot both edit the NUMBER_OF_SYSTEM_FLAGS line (see DESIGN.md
§7), so the toggle is package state, default ON, not persisted."* The code
two files over is `getSystemFlag(FLAG_PRETTYP)`, and §7 — the section the
comment cites as its authority — now reads *"system flag | **50
FLAG_PRETTYP (0x8071)**, **51 FLAG_PTLINE (0x8072)** | superseded the v1
'none' ruling"*. The header is where this package puts binding contracts
("The one binding contract callers rely on", `:12`) and it arrives in
every TU via `c47.h`. A maintainer reading it concludes PPON does not
survive a power cycle and either re-adds persistence or "fixes" a reset
path. Classes: *Comment that outlived its mechanism* (r5 R13) and *Stale
load-bearing narration* (r9 R9-9). Confidence high.

---

## 5. Design observations (D7)

Shape, not defects. These are the reason to run the audit.

**D7-1 — §7 is declared BINDING for other packages and describes the
PP4-era footprint.** `DESIGN.md:453` heads it *"§7 Composition claims
(BINDING for other packages)"* and `:3` says the document is authoritative
for the package. The registry row still reads *"calcMode | **20
reserved** (not wired)"* — PP10 wired it with six `keyboard.c` hunks, in
the file the same document calls *"the project's riskiest three-package
composition surface"*. `DESIGN.md:480` still reads *"No patches to
`stack.c`, `defines.h`, `keyboard.c`, `softmenus.c`, `statusBar.c` until
PP4"*, while `patches/` contains `010-defines.h.patch`,
`010-keyboard.c.patch`, `010-softmenus.c.patch` and the 583-line
`010-solver__equation.c.patch` — none of which appear in the
"Upstream files hooked, with verified adjacency" table. `DESIGN-HISTORY.md`
records both waves properly; the table a fourth package is told to plan
against did not move. Class C15: `patches/` is the computed truth and
nothing diffs it against the doc. **This is the systemic one**, and it is
also what hides A2: nothing in §7 says the browser depends on a sibling.
A checker that diffs the registry against `patches/` costs an afternoon
and would have caught this, P3, P4 and half of A8.

**D7-2 — 529 lines of package logic live inside upstream
`solver/equation.c`, appended at EOF, for one static's worth of
coupling.** The identifier census of the appended block returns exactly
`_pushNumericStack` (one call, static at `:743`),
`PARSER_OPERATOR_STACK_SIZE` and `PARSER_NUMERIC_STACK_SIZE` (used only
inside a size macro), plus two non-static/comment-only names. The EOF
anchor is the one location the project's own doctrine names a
guaranteed-conflict site (`design-docs/undo-history/DESIGN.md:189`:
`testSuiteList` is anchored mid-file because *"forth-core appends at EOF;
sharing that hunk context would be a guaranteed apply conflict"*). This is
arguing with a stated ruling — `DESIGN.md:389` rules *"one hook line at the
top of parseEquation's scan loop + an appended block in the same file"* —
and the argument is that the ruling was made without the coupling census;
the package's own F2 precedent (`files/programming/param_core.c`)
extracted a more entangled block behind 3-line non-static wrappers. If the
ruling stands after the census, it belongs in
`.claude/skills/upstream-diff-review/references/deliberate-exceptions.md`
with its citation: it is currently the largest uncatalogued non-minimal
shape in the tree, and it is where the next reviewer will look.

**D7-3 — forth-core's override carries a pretty-print-owned condition
with no record in `design-docs/forth-core/`.** The stage series edits
`packages/forth-core/keyboard.c:1847` to add
`|| (calcMode >= 20 && calcMode <= 23)` plus its regenerated twins;
`design-docs/forth-core/` receives zero lines in the same range. The
identical mechanism applied to undo-history **is** recorded, at
`design-docs/undo-history/DESIGN.md:173-183` ("**AMENDED 2026-08-26
(browser-range coordination, for pretty-print PP10/PP11)**"). A forth-core
rebase or minimality review meets a widened condition naming calcModes
that do not exist in a forth-core-only build; deleting it silently breaks
pretty-print's browser, and the failure surfaces in a package whose own
patches all still apply. Half of a cross-package wave was documented and
half was not.

**D7-4 — Two decoders read one token stream and only one knows the full
token set.** A9 is the instance; the shape is the observation.
`ppfBuildEntry` (display) and `pbFindResult` (recall) walk the same bytes
with independently maintained switches, and `prettyInternal.h`'s enum has
no mechanism forcing either to be complete. The same shape one level down:
`ppfBuildEntry`'s 8-deep postfix stack is smaller than what the 24-node
arena can emit — a 9-leaf right-leaning formula is constructible (an
8-leaf `+` chain on an SSIZE8 stack, one more literal, `x<>y`, `+`) and the
row is then silently skipped everywhere. That one is contrived and fails
gracefully, so it is folded here rather than counted; both belong in the
same round-trip fixture.

**D7-5 — WS-ONLY churn: the equation.c paint hook re-indents the upstream
`showString` line instead of using the no-reindent wrap.** The scanner's
single mechanical finding: identical text, two extra leading spaces, on
`showString(tmpString, &standardFont, 1 + X_OFF, …)`. One upstream line
modified where zero needed to be — the class the upstream review ranks
worst. The package's own `screen.c` hook gets this right (`else if(
prettyTryRegisterLine(…)) { }` with the enclosed lines byte-identical);
the `equation.c` hook does not. Not covered by
`deliberate-exceptions.md`. Cheap and behaviour-neutral; left alone it is
a conflict the next merge raises for no behavioural reason.

**D7-6 — Three upstream/sibling truths are hand-copied into the capture
and value paths with nothing forcing them to stay in step.** All three
copies currently match; the coupling is the finding. (1)
`prettyValue.c:731-733` re-derives upstream's fraction-arm predicate
verbatim from `screen.c:3941-3948`, and because the pretty arm is inserted
**above** that arm, a divergence makes the pretty form choose a different
builder than upstream would have — against `DESIGN.md:90-96`'s
builder-first invariant, *"the pretty form can never disagree with what
upstream would have shown"*. (2) `prettyCapture.c:809-811` copies
`fnKeyEnter`'s eRPN dup condition from `keyboard.c:3416`; a divergence
mirrors a duplication that did not happen and every later slot is off by
one. (3) `prettyCapture.c:529` hard-codes `case 427: case 428:` for
undo-history's rows, where the design text lists only 428, and in a solo
build those rows are `CAT_FREE` stubs. Classes C14 and R9-5;
`_Static_assert` pins the constants, and one shared predicate pins (3).

**D7-7 — The solo configuration is gated but never exercised by a
keypress.** A2 and A8 are both solo-only, both in code paths no headless
test enters, and both in a configuration `build-test.sh` builds **first**.
The package's test surface is unusually strong — the mutation table is
real, expectations are built from live catalog names, most pins probe
pixels through the actual render path, and the case file seeds X with 99 so
a driver that never ran fails loudly — and its one systematic blind spot is
exactly where the drivers stop using the real path. `DESIGN-HISTORY.md:531-533`
names it ("A B9-style real-keypress harness remains open work") and then
the very next paragraph makes a load-bearing claim that only such a harness
could have checked, and gets it wrong. The r11 class is already in the
catalog: *the harness enters below the layer where the bugs are*.

**D7-8 — The capture engine's root cause, stated once.** A1, A4, A5, A6
and A3 are five faces of one assumption: **a register still means what it
meant a moment ago.** A1 reads a register after the dispatch that moved
it; A4 and A5 leave a slot describing a register somebody else wrote; A6
lets a whole program run unobserved; A3 answers the "did it fail?"
question before the failure. The design has the vocabulary for all five —
"at quiescence", `resultReg = -1`, over-invalidation is free — and applies
it at the call sites the author had in mind. Whatever fix wave lands
should name this class in `DESIGN-HISTORY.md` and hunt it at *all* its
sites, because on the regression record (r2 4-of-7, r3 4-of-4, r10 10-of-11)
the next round's findings will come from this one's fixes.

---

## 6. Deliberately not flagged

Merging what the eight finders cleared with what the refutation pass
disproved. This section is longer than the finding list, which is the
correct ratio.

### 6a. Refuted by the refutation pass

**FV5 "asserts none of the three things it pins" — REFUTED by mutation.**
The claim was that FV5 is decoration. It is not: deleting
`drawSinglePixelFullWidthLine(20)` from `pbPaint` turned the gate red with
exactly one failure, `FV5 frame 20`, so the frame oracle is live (against
the browser's paint rather than the pager's — naming drift, not a missing
oracle), and "PCLR empties" is a direct `ppcHistoryCount()` check. The
finding's own suggested mutation (forcing the browser's row count to zero)
turned the gate **RED at FV6**, so "a mutation that stays green" is false
as stated. And the two facts the finding called defects were ruled at
PP10, in the code (`prettyFormula.c:669-671`) and in the amendment trail
(`DESIGN-HISTORY.md:530`); commit `c6f85a267` shows the same hand
deliberately replacing FV5's two protocol asserts with browser-mode
asserts. What genuinely survives is prose-only: the FV5 comment,
`TESTING.md:89` and `pretty_print.txt:46` still say "pager" and "arms the
manual-paint protocol", three lines PP10 forgot to re-word, and the "no
content ink" oracle is coarse (satisfied by the "no formulas" placeholder
at y=90). Doc drift and an oracle-tightening nit, not a finding. *(A
secondary mutation confirmed the retained pager body has zero coverage —
killing it entirely produced no failing pin — but its retention is exactly
what `prettyFormula.c:669-671` rules as deliberate fallback code.)*

**`ppcEnsureKnown` reads `REGISTER_X+slot` during the CM_NIM deferred-lift
window — REFUTED, the window does not exist.** The reaching input needed
STAGE to run with `calcMode == CM_NIM` and X holding the zeroed
placeholder. Upstream closes NIM **before** the item is dispatched
(`addItemToNimBuffer`'s `default:` calls `closeNim()`;
`executeFunction:1260` closes it again for the direct path), which is why
the header invariant is deliberately qualified *"at quiescence"* and why
the test driver encodes it as a modelled fact
(`prettyTest.c:751`, *"the keypress closes NIM before the run"*). The one
residual the design **did** anticipate — a `closeNim` that runs without
committing — has an explicit arm (`:805`,
`dup = (calcMode != CM_NIM); // only if closeNim committed`) and is
unreachable for ENTER anyway, because the one state where `closeNim`
refuses to close (a bare `#` base prefix) is the state where
`addItemToNimBuffer` swallows ENTER as the base digit. Probe over the whole
battery: STAGE-ENTER fired 46 times, `calcMode == CM_NIM` **zero** times;
the finding's own sequence produced the truthful formula.

**Already fixed at `6ee277762`, out of scope, found independently by the
errorpaths dimension.** (a) `ppEqDelegate`'s DERIV branch read
`engineNestingWasRefused` without arming it while the INTEG branch cleared
it first, so one earlier refused nesting made every later DERIV discard a
correct answer. (b) The bound-variable snapshot guard was narrower than
`saveRegisterSnapshot`'s own coverage, so a variable holding a complex with
nonzero imaginary part was overwritten by the counter and never restored.
Both are now named classes in the catalog ("save-test narrower than the
save", "stale global read as this call's verdict"). They are recorded here
only because a reader comparing this report against the tree will find them
already gone.

### 6b. Capture engine

- **`ppcEmit`'s eviction loop cannot spin.** `while(count >= MAX || used + off > BYTES)`
  with a serialize buffer of `PPC_HIST_BYTES/2` = 320 against a 640-byte
  ring: `off <= 320` always, so an emptied ring always admits the entry.
  This is also *how* `DESIGN.md` §5's "oversized entries (> half the ring)
  are dropped" is satisfied — by construction, not by a test. Checked by
  three dimensions independently; all three expected a hang (Stage N's
  class) and all three cleared it.
- **Value-leaf round trip is exact, and carries the TAG.** `bytes = TO_BYTES(getRegisterFullSizeInBlocks())`
  copied via `getRegisterDataPointer()` matches upstream `copyRegister`'s
  own pairing; for `dtLongInteger` the recorded `allocParam` is already
  limb-aligned so `reallocateRegister`'s round-up is a no-op. The register
  tag rides in `pad[0]` and the header's tag field is 5 bits, so the
  `uint8_t` narrowing is lossless. **This is explicitly not a repeat of
  r11 R11-IF-1** — checked for it deliberately.
- **All serializer/decoder length checks are correct or conservative.**
  TKBIG `off+21>cap` for 21 bytes written; TKV `off+7+bytes>cap` for
  6+bytes; TKL `off+2+total>cap` for 2+total; TKRES
  `off+7+bytes<=sizeof(buf)` for 6+bytes.
- **SSIZE4 ↔ SSIZE8 is `US_UNCHANGED` → IGNORE, and that is safe.** Four
  dimensions traced this expecting a lie. Registers A..D are not part of
  the stack under SSIZE4 and no stack op touches them (`getStackTop()`
  bounds them all), so slots 4..7 stay truthful across an 8→4→8 round
  trip; `ppcCurrentRevalidate` drops a root above the new top; and
  `ppcInvalidate` frees all eight slots regardless, so nothing leaks. The
  one shape that could turn it into a visible wrong answer also needs A4's
  hole, so it is folded there rather than double-counted.
- **`ppcClassify` indexing `indexOfItems[func]` with a negative `func`.**
  Upstream itself does `indexOfItems[func].func(param)` two lines below the
  hook, and `runFunction` routes negatives to `showSoftmenu` before
  reaching `reallyRunFunction`; `func >= 0` is an established precondition
  of the hook site.
- **`ppcRclLeaf`'s param handling.** A named `PPN_RCL` is minted only for
  `param <= 99`; otherwise the register is snapshotted. DONE runs only
  under `ERROR_NONE` and `fnRecall` gates on `regInRange`, so a bogus or
  indirect param cannot reach `getRegisterDataType`.
- **`prettyNoteNimText` / `prettyNoteNumberCommit` pairing.**
  `ppcNimTextValid` and `ppcPendingLift` are re-latched at
  `closeNim`'s head and at every `calcModeNim`, four lines above any
  `goto` target, so neither can be read stale.
- **Node aliasing.** `ppAppendChild` would self-loop on a double append;
  every append site in `prettyFormula.c`, `prettyEquation.c`,
  `prettyValue.c` and every slot shuffle
  (`ppcShiftDownAfterConsume`, `PPC_DROPY`, `PPC_FILL`, RUP/RDOWN/SWAP)
  replaces each transient alias with a deep copy.
- **`ppcHistoryClear` leaves `ppcHistSeq` and live trees' `PPA_EMITTED`
  alone.** `seq` is a display ordinal; keeping EMITTED after PCLR is what
  §4's "nothing emits twice" asks for. *(The parenthetical in §4, "flag
  cleared on consumption", describes a mechanism the code does not have —
  the prose is backwards about a correct implementation. Too small to
  spend a reader on beside D7-1.)*
- **`ppcTopSlot()` re-read at STAGE and again at DONE**, so a dispatch that
  changed the stack size between them would scan a different range — but
  every stack-size item is `US_ENABLED` → `PPC_INVALIDATE`, whose free loop
  covers all eight slots unconditionally.
- **Emission at STAGE followed by a dispatch that errors** leaves a history
  entry for an operation that did not complete — but the entry's `= result`
  is read pre-op and is therefore true, and §4 accepts over-emission ("a
  formula, once finished, happened").

### 6c. Layout, value and equation rendering

- **`ppPaintAt`'s precondition (a successful `ppMeasure`).** The
  `PP_BIGOP`/`PP_RAD`/`PP_PAREN`/`PP_INT` paint arms dereference
  `ppPool[nd->firstChild]` with no `PP_NONE` guard, which would index
  `ppPool[255]` for a childless box. Every call site measures first —
  `ppRenderRightAligned`, `fnPrettyShow`, `ppfBuildRow`,
  `prettyTryEquation`, `ppqShowRender`, `pbPaint`. Cleared on
  reachability, not on shape.
- **`ppAppendChild` silently ignoring a `PP_NONE` child** would render a
  formula with a term missing instead of declining. Two unchecked
  `ppfParen` returns exist (`prettyFormula.c:145`, `:179`), both failing
  only on pool exhaustion. The worst tree the 24-node capture arena can
  produce was costed (11 chained LOGXY ≈ 59 of 72 nodes) and cannot fill
  the pool. **UNREACHED** — reporting it would be asserting a path nobody
  can produce. *(Also: `prettyFormula.c:179`'s generic two-arg arm is dead
  — every dyadic the classifier admits has an explicit case.)*
- **`ppSetFontDeep` flattens a construct's deliberately-tiny limits** and is
  guarded at only one of nine call sites. The other eight either target
  leaf limit nodes on purpose or re-font capture/value trees that
  structurally cannot contain an EQN construct. Correct today; the
  one-guard-per-site shape is worth remembering, not reporting.
- **Recomputed rather than stored:** `PP_RAD`'s `synth` test and
  `PP_PAREN`'s mode test are derived again in paint. Both carry "the paint
  pass recomputes this same test", and their inputs are measure outputs
  nothing mutates between passes. Deriving beats storing here — the
  opposite of the usual complaint.
- **`ppSpanA` re-entrancy in `ppParseComplex`** and the **torn 2-byte glyph
  in `ppParseIrfrac`'s multiple span**: both UNREACHED. `ppReset()` empties
  the 512-byte text pool before each attempt and input is capped by
  `ppScratch[200]`, so the pool cannot be near-full; and the only 2-byte
  code the `S_AFTER_SIGN` state admits is a space glyph.
- **Every `snprintf`/`strcpy` in the package fits.** `ppfLabelName` 17
  into 24, `ppfVariableName` 16 into 20, `ppfBigop` worst case 65 into 96,
  `ppqFrameDerivative` 20 into 28, `ppqFrameIntegral` 19 into 24,
  `ppfFormatStaged` gated on `strlen(buf) >= destSize`.
- **Negative coordinates into `lcd_fill_rect`.** `ppRenderRightAligned`
  refuses `width > xRight`; the pager paints at x=4 with width ≤ 392; the
  synthesized-paren `hh - 4` is only reached when the child is taller than
  the glyph paren. `pbPan`'s int16 overflow after ~546 `.d` presses turns
  the pan arm off rather than producing a negative x. (See A7's note if
  that changes.)
- **The PP14 construct's bounds and balance.** `argStart[5]`/`argLen[5]`
  with both an in-loop and a post-loop `nArgs >= 5` check; `varName[16]`,
  `bodyText[256]`, `text[256]` all gated on their slice lengths;
  `ppEqDepth` incremented after every early return and decremented on
  every path; `ppEqStackBase` re-armed at every depth-0 entry;
  `ppEqTempAppend`/`Delete` strictly LIFO so the "temp slot is always last"
  precondition holds under nesting; `PPEQ_SNAP_BYTES` = 1400 against
  `TMP_STR_LENGTH` 2560.
- **`ppqParse`'s label-prefix scan** tests `src[p]` only after advancing —
  correct for the grammar, since a label is at least one character.
- **The synthesized-radical DDA and the ∫ hook quadratic** guard their
  divisors and clamp `hh` to [3,7].
- **Non-X register lines clipped at `baseY+31` rather than §6's `+35`**:
  the comment gives the reason (the next line's clear band starts at
  `baseY+32`) and the change is strictly more conservative.

### 6d. Lifecycle, surfaces and error paths

- **`prettyTryRegisterLine`'s guard chain.** Each conjunct is falsifiable
  and each excludes a distinct upstream arm; the global `lastErrorCode != 0`
  is deliberately more conservative than upstream's
  `regist == errorMessageRegisterLine`. It writes `*lineWidth` only on
  success, matching the arm contract every upstream branch honours.
- **`ppfStageValFields` clearing `lastErrorCode`** after a failed
  `reallocateRegister`: every caller gates on `lastErrorCode == 0` first,
  so 0 is the correct restore, and a display path must not raise.
- **`prettyBrowserEnter` swallowing `ERROR_RAM_FULL` from `saveForUndo`:**
  silent, but the alternative (leaving the browser open under an error) is
  worse and the mutation genuinely did not happen.
- **`closeNim`'s exits, enumerated.** Only the `CM_PEM` branch bypasses
  `closeNim_exit`, and it must — the number went into the program, not onto
  the stack. Every other early exit is a `goto closeNim_exit` with
  `lastErrorCode` set or after `undo()`, which the commit predicate
  refuses. The one hole is the `shortIntegerWordSize` `displayBugScreen`
  path, an explicit "cannot happen" with no reaching input.
- **`calcModeNim`'s `ERROR_RAM_FULL` early return sits above the
  `prettyNoteNimOpen` hook**, so an aborted NIM open latches nothing.
  Deliberate placement.
- **`prettyReset()` before `configCommon(CFG_DFLT)` in `doFnReset`:**
  `configCommon` only force-sets the date/US flags, never `FLAG_PRETTYP`/
  `FLAG_PTLINE`, so default-ON survives. Pinned by FV13/FV14/FV16.
- **`prettyBrowser` clears `FLAG_ALPHA` and `cursorEnabled` on entry and
  restores neither** — same shape as upstream `registerBrowser.c:166-175`,
  the browser is not reachable from `CM_AIM`, and `cursorEnabled` is
  re-set on the next NIM/AIM.
- **`restoreCalc` bug-screens on an unknown calcMode**, so quitting the
  *simulator* with the browser open and restoring gives "20 is an
  unexpected value for calcMode" — that file is PC_BUILD-only, the DMCP
  target retains RAM instead, and undo-history's CM 19 has the identical
  exposure.
- **`pbSelection` indexes rows `ppfBuildRow` declines**, so a too-complex
  row makes the marker vanish and UP/DOWN look dead — display granularity,
  and the same skip exists in the shipped pager. (A7 is the version of
  this that costs the owner a calculation.)
- **`fnPrettyHist`'s pager body is unreachable through the keyboard** and
  is retained deliberately: `prettyFormula.c:670-672`, "the non-browser
  fallback surface and for the packing reference".
- **PSHOW/EQSHW leave `screenHoldsDrawnPixels` set on purpose** (the
  verbatim `fnPixel` protocol); `refreshScreen` clears it.
- **The `softmenus.c` checkbox branch** reads `item%10000` inside the
  `item >= 0` arm, guarded on `softmenu[m].menuItem == -MNU_PP`, so it
  cannot fire on a menu item or perturb the `-MNU_SYSFL`/`-MNU_TAMFLAG`
  neighbours.
- **The `FLAG_PRETTYP` gate on the EQN strip** looked like it contradicted
  the PP15 flag-scope ruling ("only the inline register lines"), but the
  strip is drawn on the calculator's own initiative, which is the side of
  the line the ruling puts under the flag. The sentence is loose; the code
  is right.
- **In-scope nesting where the inner STAGE overwrites the outer's**, rather
  than A3's out-of-scope shape. `fnKeyDotD → runFunction(ITM_toREAL)` is
  upstream's own example and the one `DESIGN-HISTORY` names — but
  `ITM_dotD` is `US_UNCHANGED` → IGNORE, so the outer stage is never armed.
  Every nested `runFunction` in upstream was enumerated (`fnCFGsettings`,
  `fnChangeBaseMNU`, `_fnSetC47`, the addons PEM paths, plotstat demo,
  conversion round-trips) and none implements a modelled class, so the
  skipped transform is always an invalidate the inner dispatch already
  performed. **UNREACHED**, and recorded as A3's second face.

### 6e. Tests and oracles

- **EQ22's pool-usage pin** (`while(used < PP_POOL_NODES && ppNodeAt(used) != NULL) used++`)
  looked like a bounds-only walk that always reaches 72. It is not:
  `ppNodeAt` returns NULL past the **allocated** count.
- **EQ9's "fallback text missing" check** looked satisfied by EQ8's
  leftover pixels. It is not: `ppqShowRender`'s first statement is a
  full-screen `lcd_fill_rect`.
- **FV16 asserts a negative** ("cold start did not touch the flags"),
  vacuous if the lazy init never runs. It runs — `ppcTestOp(ITM_ENTER)`
  reaches `prettyNoteFunction`, which begins `if(!ppcInited) ppcInit();`.
- **FV11's "default OFF" capture** compares the untouched state against a
  forced `prettySetTline(false)`, trivially equal today — which is the
  point of the pin.
- **B9's comment "the stack effect is identical"** is not quite true of
  upstream (the INFSUMS path writes the iteration count into Y), but the
  capture cannot be misled: the BIGOP DONE arm sets every slot to
  `PPC_UNKNOWN`. *(The B9 defect is A14, and it is elsewhere in the
  fixture.)*
- **`ppTestRowAllLit`/`ppTestRowAnyLit` return vacuously for an empty
  span.** Every call site was checked; all spans are literal or derived
  from a found run with `vx0 <= vx1`.
- **Every P-, S-, FV- and EQ- pixel probe** was checked for a region
  something other than the feature could light — the failure mode B8 and
  MUT-23 were paid for. Apart from FV5's coarse ink oracle, none.
- **`prettyTestFallback`'s F1/F2/F3 identity pins have no non-emptiness
  control**, so two empty renders would compare equal. Not flagged: P1-P3
  prove `refreshRegisterLine` paints under the same state, and
  `TESTING.md` already records the blind spot ("A stray paint placed BEFORE
  the toggle gate is invisible to the F-pins"). Worth a control if those
  pins are ever touched again.
- **T11 clears `FLAG_SPCRES` and never restores it**, against the driver's
  own stated rule — the case file's `In:` line already sets `FL_SPCRES=0`,
  so today it changes nothing.
- **`build-test.sh` greps for "TESTS PASSED SUCCESSFULLY"** while the
  harness prints the singular form when exactly one test passes.
  Unreachable in practice, and the exit status is the real gate.
- **Stale comment, no behaviour:** `prettyTest.c:357` says "row 131" while
  the loop scans row 130 and the message says 130.

### 6f. Upstream discipline (checked against `deliberate-exceptions.md`)

Flagging any of these would re-litigate a paid-for ruling:

- **`bufferize.c` at :2686-2691 overlapping forth-core's :2691 by one
  line** — `DESIGN.md:474` states it in advance and calls a conflict there
  "loud, which is the intended failure mode". Composes.
- **The `items.c` catalog-stub anchor one line clear of undo-history's**
  — `DESIGN.md:473` predicts it to the line, and both sides are pure
  insertions (unlike P3, where both sides *modify*).
- **`testSuite.c` rows two lines above undo-history's** — built to the
  `DESIGN.md:479` spec.
- **Filling spare `CAT_FREE` item rows and replacing `ITM_NULL` in
  `menu_DISP`/`menu_EQN`** — the sanctioned mechanism; both menus verified
  untouched by both siblings.
- **The softmenu entry appended before the sentinel** — the table's own
  instruction. forth-core's mid-table insert has its own separate ruling
  for a different reason.
- **The empty `else if(prettyTryRegisterLine(…)) { }` body in `screen.c`**
  — the minimal additive shape mid-ladder; the comment explains it.
- **`defines.h` and `c47.h` anchor distances** — measured both ways and
  reported without a finding, because the rule's wording ("anchor ≥4
  context lines away") does not settle whether it means insertion points
  (8 and 4 lines — satisfied) or hunk contexts (2 lines apart and
  overlapping by 2 — not). Both sides are pure insertions and both
  compose. **Worth one sentence from the owner to disambiguate**, since the
  same rule is quoted at other sites.
- **`keyboard.c` hunks one line from undo-history's and overlapping in
  shared context** — twelve pure insertions, which 3-way merges regardless
  of proximity, and no plausible upstream edit was found that breaks them
  the way P3's injection breaks `items.c`. The real problem at these sites
  is that §7 does not admit they exist (D7-1).
- **`#define MNU_PP 217` placed in `items.h`'s ITM_ block** rather than the
  MNU_ block where forth-core put `MNU_FORTH` — almost certainly
  deliberate collision avoidance (forth-core's two `items.h` hunks are
  precisely in the MNU_ region), but unstated either way. Noted so the
  reason gets written down rather than rediscovered.
- **`prettyPrint.h` duplicating `prettyBrowser.h`'s six prototypes** —
  both package-owned, identical, compiler-checked where both are included.
- **pretty-print not patching `browsers/browsers.h`** (undo-history does)
  and routing declarations through `c47.h` — good discipline, the same
  reasoning §7 gives for keeping the include out of `screen.c`.
- **No dead overrides.** Every upstream mirror has a corresponding patch —
  the failure mode of `b5c4020af`. `.pkgignore` correctly excludes only
  `*.md` and `build-test.sh`.
- **The forth-core + undo-history headless battery segfault**
  (`DESIGN-HISTORY.md:546-552`) — explicitly pre-existing, not
  pretty-print's, and already on the record.

---

## 7. Verdict

**Would I ship it? Not as it stands, and the reason is two lines of code,
not the feature set.** The package is good work: a decline-everything
layout engine that held up under every attack four dimensions could reach,
a capture engine whose ring arithmetic, serialisation (tag included) and
PP14 stack fences are all clean, a test battery with a real mutation table
and pixel oracles that mostly probe through the actual render path, and an
override footprint of 661 added upstream lines across 11 files that
composes cleanly with both siblings today.

**Where it breaks first: the shadow tells the truth about a formula and
lies about its result.** A1 is one register read on the wrong side of a
dispatch, and it turns a very ordinary gesture — finish a calculation,
press any function the classifier does not model — into a permanent
history entry that says `2 + 3.7 = 5`. It is recallable into X. On a
calculator that is the worst class of defect there is, and the binding
invariant the design wrote for exactly this ("the display never lies") is
the one it breaks. A3 sits behind it, disables the error rule for every
Σ/∏/∫/SOLVE, and must be fixed in the same wave or it will move A1's bug
rather than remove it. A4, A5 and A6 are the same assumption at three more
sites.

**Second: the flagship feature does not work in the configuration the gate
builds first.** A2 is not subtle — open the browser, press a key, get the
firmware's internal-error screen, press EXIT, get it again. It works only
because two *other* packages carry pretty-print's enabling edits, which is
recorded nowhere in the registry that is declared binding for other
packages, and the one sentence in the amendment trail that addresses it is
factually wrong about the branch it names. Whether that is a ship blocker
depends on a question I cannot answer: does anyone install pretty-print
without forth-core? The gate says yes.

**What I would do in order:** name the register-staleness class in
`DESIGN-HISTORY.md`; fix A1 and A3 together, red-first, with the
decode-the-TKRES sweep as the class test; add the CM-20 resolution branch
and a real-keypress harness (A2), which also closes D7-7 and would have
caught A8; then A4/A5/A6 as one classifier wave with the machine-derived
enumeration. A7 and A9 are a browser afternoon. A10–A14 can wait, and four
of them can wait forever (§3, "What I would leave alone").

**What I would not do:** re-litigate D7-2. The 529-line block in
`solver/equation.c` is a stated ruling, the coupling census is now on the
record, and if the ruling stands it needs a line in
`deliberate-exceptions.md` more than it needs a refactor.

---

## 8. Round and exit state

**Round 1 for this package** — the first full audit of pretty-print. (An
out-of-family pass ran the day before and its two findings were fixed at
`6ee277762`; that wave is out of this report's range and is recorded in
§6a.)

**Readers.** Eight finder dimensions, blind to each other: contracts,
lifecycle, arithmetic, errorpaths, guards, tests, design, upstream. Each
finding then went to an independent refutation pass with one assigned
adversarial lens; four findings collected two to five lenses because
multiple dimensions reached them independently. Twelve of the twenty
surviving verdicts are backed by a mutation or probe **applied, observed
and reverted** in an isolated worktree; every worktree spawned at a stale
ref (`e21af8d28`) and every verifier checked out `f6d415318` before its
first read — the round-6 runner trap, caught by the standing first-action
rule, twelve times out of twelve.

**Exit criterion: not met.** The criterion is a round that produces no
confirmed finding of consequence. This one produced six wrong-result
defects and one stuck state, five of which share a single unnamed class.
The regression record (r2 4-of-7, r3 4-of-4, r5 9-of-12, r10 10-of-11)
says the next round's findings will come mostly from this round's fixes,
and it says relocating state is the most dangerous fix shape — which is
exactly what fixing A1/A3 together will be tempted to do. Round 2 should
run over the fix wave, not over the package again, and the fix wave should
land red-first.

**Tree state.** The audit itself wrote exactly one file — this one. Every
probe and mutation was applied, observed and reverted inside its own
worktree; none touched the owner's tree, which was clean when this report
began.

It is not clean now, and not by this audit's hand: while the report was
being written, a fix for **A3** landed in the working tree as `AUDIT R1-3`
— a `ppcDispatchDepth` counter that stamps `ppcStage.depth` at STAGE and
refuses the transform at DONE unless the depths match, plus a fixture in
`prettyTest.c`. That is the pairing this report asks for, and its comment
independently reaches the same reading ("the end state usually coincided,
which is why every test passed"). A reader diffing this report against the
tree will find A3 in progress rather than open. **A1 must land in the same
wave**: with A3 fixed and A1 not, the stale result read simply moves from
the inner DONE to the outer one.
