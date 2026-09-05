# Audit — program-graphics stage G1, the canvas view: round 1, at `cb2ae56e7`

Subject: `a0077d62b..cb2ae56e7` on `program-graphics/stage-g0`, one commit.

The stage adds `calcMode` 21 (`CM_GRAPHICS_CANVAS`), the commands `PVIEW n`
and `ERASE`, a `refreshScreen` case that paints the status bar only (plus the
softmenu for region 2), a guard arm in `processKeyAction`, an R/S release arm,
an EXIT case in `fnKeyExit` that calls `pgCloseView`, five no-op key-function
cases, the three softkey gate lines in the registry range form, and the
identical-edit arm `calcMode >= 20 && calcMode <= 23` in the key resolution
chain, shared with pretty-print-extra (amended the same day).

The view holds. Every V pin and every K pin stands on the code as written,
EXIT closes the view from the keyboard, R/S continues the program, and the
drawing outlives STOP. The round's yield is one layer out from the view:
**mode 21 is the first package mode that stays live while a program runs, and
the program's own steps do not know it.** ENTER is a no-op inside the view.
CC raises the firmware bug screen. CLSTK erases the drawing. A run-time error
has no surface and eats the next EXIT. And the design's model of the key path
("a guard arm marks every key except SNAP") is not the mechanism that keeps
EXIT alive; the pin that covers EXIT drives the release directly and cannot
see the difference.

> **Filename note.** The dispatched subject string is about 2,300 bytes and
> cannot be a filename (255-byte limit). This file keeps the round-9/round-10
> convention: truncated after the tip, with the full subject stated in §1.

---

## 1. Subject and coverage

> **ROUND 1 THREE-FAMILY RULE NOT MET.** Out-of-family families in this run: none; round 1 requires Gemini AND GPT on the actual subject, plus this in-family pass. This is not a complete round 1 — it cannot close anything, and the round counter does not advance until both families' replies are fed back through this workflow with their packets recorded in outOfFamily (SKILL.md, Order of work step 4).

### The full dispatched subject

Program-graphics stage G1: the canvas view. A new package for the C47
firmware lets a user program draw on the screen. Stage G1 adds calcMode 21
(`CM_GRAPHICS_CANVAS`) in the package registry 19-23, the commands `PVIEW n`
(n = 2 or 6, a step parameter) and `ERASE`, a `refreshScreen` case that
paints only the status bar (and the softmenu for region 2), a guard arm in
`processKeyAction` that marks every key except SNAP as processed (R/S clears
`showFunctionNameItem`), a release-path arm that sets
`showFunctionNameItem = ITM_RS` for the view, an EXIT case in `fnKeyExit`
that calls `pgCloseView`, no-op cases in `fnKeyEnter`, `fnKeyBackspace`,
`fnKeyUp`, `fnKeyDown` and `fnKeyDotD`, the three softkey gate lines in the
byte-identical range form, and one identical-edit arm in the key resolution
chain shared with pretty-print-extra (`calcMode >= 20 && calcMode <= 23`),
which required amending pretty-print-extra's `keyboard.c`. Authority:
`design-docs/program-graphics/DESIGN.md` §3, §6, §7, §8; `TESTING.md` §4
(pins V1-V8, K1-K3, verified red-first per `DESIGN-HISTORY.md`, stage G1).
Pre-verified facts and documented limits as dispatched; none of the four
limits is reported here.

### The commit

| commit | content |
|---|---|
| `cb2ae56e7` | 31 files, 27,818 insertions, 48 deletions. 26,963 of the insertions are the flat-mirror copies (`defines.h`, `items.c`, `items.h`, `keyboard.c`, `screen.c`, `screen.h`, `softmenus.c`). The audited delta is the eight generated patches (278 patch lines: `keyboard.c` 145, `items.c` 33, `softmenus.c` 31, `screen.c` 17, `screen.h` 16, `items.h` 14, `defines.h` 12, `testSuite.c` 10), `files/pgmGraphics.c` (+190), `files/pgmGraphics.h` (+11), `tests/program_graphics.txt` (+7), the 10-line amendment of `packages/pretty-print-extra/keyboard.c`, and the four design documents |

Every patch, both package sources, the test file and the sibling amendment
were read in full. The upstream context of every inserted arm was read:
`btnPressed` (keyboard.c 1815-1995), `btnReleased` (2060-2330),
`processKeyAction` (2367-2830), the five key functions with bug-screen
defaults plus `fnKeyExit` and `fnKeyCC`, `refreshScreen` (screen.c
6060-6290), `runProgram` and `executeOneStep` (lblGtoXeq.c 754-1030),
`runFunction`/`reallyRunFunction` (items.c 243-700), `displayCalcErrorMessage`
and `displayBugScreen` (error.c), `_view`, `fnPause`, `fnClLcd`,
`lcd_fill_rect` (c47-gtk/hal/lcd.c 174-200), `showSoftmenuCurrentPart`,
`calcModeNormal` and its 18 callers, and every `switch(calcMode)` in
`src/c47` (25 sites, enumerated by grep).

**What the reading did not reach.** (a) The generic TM_VALUE completion site
in `ui/tam.c` that runs `fnPview` after the user types the digit: the shuffle
and GTO completions were read, the value completion was not located within
budget; `leaveTamModeIfEnabled` writes no `calcMode` for mode 21 (its PC-only
switch has no arm for 21). (b) The tail of `fg_processing_jm` and the
`FLAG_FGLNFUL`/`FLAG_FGLNLIM` underline paint (§4). (c) The DMCP long-press
executor for `longpressDelayedkey1` (SNAP on the R47). (d) No GTK-simulator
run: every key claim below is a read of the two key paths, not a keypress.
(e) The DM42 hardware, as dispatched.

**Deliberately not audited.** The four documented limits. The flat-mirror
copies (byte-identical to upstream except the patched lines; the patches are
the subject). The G0 baseline driver. The uncommitted fix wave on the working
tree (§2, §8): it postdates the subject and nobody has audited it.

### Out-of-family accounting

The workflow's `outOfFamily` is `'pending'`; the banner above stands. Two
packets and two replies exist on disk in the untracked directory
`design-docs/program-graphics/audit/`. They were **not** fed back through the
workflow: the refutation pass received nothing, and no `outOfFamily` record
exists. They are inventoried here so the completion half can run on them.
Their findings are **not** CONFIRMED or REFUTED by this report; §6.1 records
what this reader's trace supports and contradicts, as advisory notes only.

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| gemini | `design-docs/program-graphics/audit/PACKET_G1_A_keys_gemini.md` (697 lines, axis: the key paths) | `REPLY_G1_A_gemini.md` (34 lines; `.err` empty) | `MODEL: Gemini 3.1 Pro (High)` | 3 (ENTER/BACKSPACE/UP/DOWN/.d bypass the guard; f and g are ignored; an active shift state strands the user) |
| sol / gpt | `design-docs/program-graphics/audit/PACKET_G1_B_lifecycle_sol.md` (481 lines, axis: the view's lifecycle) | `REPLY_G1_B_sol.md` (129 lines; `.err` 47,858 bytes, the codex session log: `model: gpt-5.6-sol`) | `MODEL: GPT-5` | 3 (EXIT never closes the view; upstream `calcMode = CM_NORMAL` sites abandon the view; the guard does not cover several direct keys) |

Note on the identity check: Sol's `MODEL:` line reads `GPT-5`, the codex
header in the `.err` file reads `gpt-5.6-sol`. The line is not the exact
model name the brief asks for. Record it; do not re-dispatch for it.

Note on the packets: both packets carry the same orientation bullet, and it
is wrong in two places that matter. It says EXIT, ENTER, BACKSPACE, UP, DOWN
and `.d` "have their own `case`" (`.d` has none; it reaches the guard arm)
and that those keys run their key function "on the key RELEASE" (BACKSPACE,
UP and DOWN run theirs at the PRESS, inside their own case). Packet B also
pastes the no-op case text under the label "`fnKeyExit`: the package's case";
the package's `fnKeyExit` case calls `pgCloseView()`. Sol's top finding is
built on that paste (§6.1).

### In-family coverage

The workflow's finder and refutation passes returned empty lists for this
run (no CONFIRMED, no REFUTED, no per-dimension coverage). What follows is
therefore one in-family reader's trace of the stage, written to the report
template. Nothing in §3 has been through an independent refutation. Each
entry carries the reaching input, the quoted contract, the bug class and the
class-level test so that the completion half can refute or confirm it
without re-deriving the path.

Dimensions covered by this trace: D1 (every inserted arm against its
callers), D2 (the view's open/close/run/stop sequence), D4 (the error path),
D5 (each conjunct of the guard arm and the R/S arms), D6 (K2 and K3 against
the real release), D7, D8. D3 was covered at the two `lcd_fill_rect` calls
and the clip rows only; there is no other arithmetic in the stage.

---

## 2. Mechanical results

| check | result |
|---|---|
| `tools/pkg_patch_refresh.py packages/program-graphics` on the audited tip | idempotent: the tree was clean after the refresh (checked before the fix wave landed on disk, see below) |
| `tools/pkg_patch_refresh.py packages/pretty-print-extra` | idempotent |
| solo gate, **working tree** (`build-test.sh --solo`) | **GREEN**, exit 0, success banner present. Caveat: by the time the gate ran, an uncommitted fix wave had landed on the working tree (K4 in `pgmGraphics.c`, a new `patches/010-calcMode.c.patch`, a comment edit in `keyboard.c`, doc edits). The script refreshes from the working area first, so this run tested the fix-wave tree, not the subject |
| solo gate, **audited tip** (detached worktree at `cb2ae56e7`, its own `build.sim`, full build) | **GREEN**, exit 0, `NUMBER OF TESTS 13020`, `13020 TESTS PASSED SUCCESSFULLY`, 0 failed. Matches the commit's recorded solo count. The same four upstream warnings |
| combined gate (`PKG_TEST_WITH=` the four siblings) | not re-run here; the commit records 13,046 tests green, and the pretty-print-extra pair/full gates at 13,042 after the arm amendment |
| compiler warnings, solo build | 4, none in package files: `testSuite.c:5497,5505` `-Wformat-overflow` (upstream test driver, `%s` into a 404-byte buffer) and `c47.h:10` `_XOPEN_SOURCE` redefined, twice. All four are upstream and predate the package |
| `design-audit.sh` | forth-core's drift script; no equivalent exists for this package. Not applicable |
| `git diff` at the end | the tree is as found: the fix-wave modifications (nine tracked files, two untracked) belong to a concurrent session and are not this audit's; the worktree was removed |

Concurrency note. `git status` was clean (one untracked `audit/` directory)
when this audit started. During the audit, nine tracked files and two
untracked files changed on disk under `packages/program-graphics/` and
`design-docs/program-graphics/`: a fix wave for Sol-2 (K4 pins CLSTK; a
`calcMode.c` patch guards `calcModeNormal`), a DESIGN.md §3.6 amendment, a
TESTING.md row, and a PLAN.md §10 ruling ("one audit round per stage"). This
report audits the committed tip only. The fix wave is new code that nobody
has audited (§8).

---

## 3. CONFIRMED findings

**None survived a refutation pass, because none was run.** The five entries
below are TRACED: one reader, every step read in the code, no independent
refutation. They are ranked by what they cost the owner. The refutation-only
completion half decides their status.

### G1R1-1 — ENTER is a no-op as a program step inside the canvas view; so is `.d`

**Where.** `packages/program-graphics/keyboard.c`, `fnKeyEnter`,
`case CM_GRAPHICS_CANVAS: { break; }` (patch hunk `@@ -3587`); `fnKeyDotD`,
the same shape (hunk `@@ -4944`).

**What breaks.** A program that runs `ENTER` after `PVIEW` gets no stack lift
and no copy of X into Y. `FLAG_ASLIFT` is not set. Every RPN idiom that
depends on ENTER computes the wrong number, silently. `.d` (`→REAL` or the
fraction-flag clear) does nothing.

**The path.** ENTER is a program step (`ITM_ENTER`, items.c row 35,
`PTP_NONE`). `executeOneStep` (lblGtoXeq.c 819-821) dispatches it through
`runFunction(op)` → `reallyRunFunction` → `fnKeyEnter(NOPARAM)`.
`fnKeyEnter` computes `effectiveCalcMode = calcMode` and remaps to
`CM_NORMAL` only for `GRAPHMODE` (keyboard.c 3410-3413). With `calcMode` 21
the switch takes the package's case and returns. The CM_NORMAL arm (the
`liftStack` and the Y←X copy, 3416-3440) never runs. `fnKeyDotD` has no
remap at all.

**Reaching input.** `LBL "SQ"  PVIEW 2  7  ENTER  ×  STOP`, run with 3 in X.
Expected X = 49. Under the view X = 21 (7 times the old X, which the literal
step lifted into Y). The same program without `PVIEW 2` gives 49. For `.d`:
`PVIEW 2  7  .d  STOP` leaves X a long integer; without the view X is the
real 7.

**Violated contract.** Upstream states the rule four lines above the
package's case, keyboard.c 3411: *"a program running under CM_GRAPH or
CM_PLOT_STAT (e.g. plot(int) integrand, programmed HPLOT) needs normal ENTER
dup, not the empty interactive-graph case"*. DESIGN.md §3.6 writes the no-op
for the *direct key path* ("Any other key, direct key path: Ignored ...
`fnKeyEnter` ... have a no-op case for the mode, because their default arm
shows a bug screen") and says nothing about the step path. The stage's
premise, DESIGN.md §1: "The package lets a user program draw on the screen."
A program that cannot ENTER cannot compute what to draw.

**Bug class.** A mode that stays live while a program runs reaches a key
function's `switch(calcMode)`, and the new arm was written for the
interactive key path. Upstream met the same class for `CM_GRAPH` and fixed
it in the function, not at the call site (`effectiveCalcMode`). Modes 19 and
20 never had this problem: they are browsers that stop the world.

**Class-level test.** Enumerate the programmable items whose item function
switches on `calcMode`: intersect `grep -rn "switch(calcMode)" src/c47
--include=*.c` (25 sites) with the `PTP_NONE` rows of `items.c`. Today that
set is `fnKeyEnter` (ENTER), `fnKeyCC` (CC), `fnKeyDotD` (`.d`), `fnTo_ms`
(`.ms`) and `fnHRtoTM` (default arm safe). For each: set
`programRunStop = PGM_RUNNING`, load the same stack, run the item once under
`CM_NORMAL` and once under `CM_GRAPHICS_CANVAS`; assert identical X, Y, Z, T,
`FLAG_ASLIFT`, `lastErrorCode`, and that `calcMode` is unchanged on return.
Red today for ENTER and `.d` (stack) and for CC and `.ms` (G1R1-3).

### G1R1-2 — `CLSTK` (every program step that calls `calcModeNormal()`) abandons the view, and the next repaint erases the drawing

Raised by Sol as finding 2 (on disk, unrefuted). This reader's trace
supports it in full; the owner's fix wave on the working tree already
targets it (K4, `010-calcMode.c.patch`).

**Where.** Upstream `stack.c:16`, `fnClearStack` → `calcModeNormal()`;
`calcMode.c:38-58` sets `calcMode = CM_NORMAL`. Nothing in the package
intercepts it. `canvas.region` stays 2 or 6.

**What breaks.** The program continues under `CM_NORMAL`. Every later
drawing step paints into a screen that the next `refreshScreen` repaints. At
STOP, `refreshScreen(4)` takes the CM_NORMAL group (`_refreshNormalScreen`),
clears the register lines and the drawing is gone. The view is half-open:
`region` 2, `calcMode` 0, `prevCalcMode` stale. `pgCloseView` returns at its
first line for `calcMode != 21`. EXIT now behaves as CM_NORMAL.

**Reaching input.** `PVIEW 6  200 100 PIXEL  CLSTK  STOP`. Expected: the
pixel at column 200, row 139 stays lit after STOP (V4's oracle). Under the
subject: the register lines are repainted over it. Programmable callers of
`calcModeNormal()` found by grep: `fnClearStack` (CLSTK, `PTP_NONE`) and
`_clearAlpha` (`flags.c:354,362`, reached by CLA). The other 16 callers are
interactive or `PTP_DISABLED` (RESET, the NIM close paths, `fnSetC47`). The
26 direct `calcMode = CM_NORMAL` assignments are the wider class; the plot
family among them is now ruled "by design" in the on-disk DESIGN.md edit.

**Violated contract.** DESIGN.md §3.3: `region // 0 = view closed, else 2 or
6`. §3.6: "A program stops: the stop path calls `refreshScreen(4)`. The rule
above keeps the canvas."

**Bug class.** Split truth. "The view is open" is held twice (`calcMode ==
21`, `canvas.region != 0`); upstream writes one half at 44 sites and reads
neither of the package's.

**Class-level test.** For every `PTP_NONE` item whose function reaches
`calcModeNormal()` or assigns `calcMode` (one level of call graph from the
grep above), run it under mode 21 with `PGM_RUNNING`, then `refreshScreen(4)`;
assert `calcMode == 21` and the V4 pixel lit, or, for the plot family, assert
the documented abandonment. The on-disk K4 covers CLSTK only.

### G1R1-3 — `CC` and `→h.ms` raise the firmware bug screen inside the view; CC does so from the keyboard too

**Where.** Upstream `fnKeyCC`, keyboard.c 4088-4215, `default:` at
4209-4212; upstream `fnTo_ms`, c47Extensions/addons.c 1369-1436, `default:`
at 1434-1436. The package's DESIGN.md §3.6 names five key functions "because
their default arm shows a bug screen". The enumeration
`grep -rn bugMsgCalcModeWhileProcKey src/c47` returns eight: `fnKeyEnter`,
`fnKeyExit`, `fnKeyCC`, `fnKeyBackspace`, `fnKeyUp`, `fnKeyDown`,
`fnKeyDotD` in keyboard.c and `fnTo_ms` in addons.c. The package gave mode 21
an arm in six. `fnKeyCC` and `fnTo_ms` have none.

**What breaks.** `displayBugScreen` (error.c 359-) sets
`previousCalcMode = 21`, `calcMode = CM_BUG_ON_SCREEN` and paints the bug
text over the canvas. It sets no `lastErrorCode`, so a running program keeps
executing under `CM_BUG_ON_SCREEN`. The stop path's `refreshScreen(4)` has
no case for that mode and paints nothing. EXIT from the bug screen restores
`calcMode` 21 with the bug text still in the canvas rows.

**Reaching input, program.** `PVIEW 2  3  4  CC  STOP`. Expected X = 3+4i.
Result: the bug screen "fnKeyCC ... 21 ... CC". `CC` is `ITM_CC`, items.c
row 1730, `PTP_NONE`. `.ms`: `PVIEW 2  1.5  .ms  STOP` (`ITM_ms`, row 1909,
`PTP_NONE`). The G2 vocabulary makes CC the natural step before `ARC`
("center as a complex number in T", DESIGN.md §2.2).

**Reaching input, keyboard.** After any stop in the view, press CC. The item
has its own `case ITM_CC` in `processKeyAction` (2558-2578); neither branch
fires for mode 21, so the key is not processed, `showFunctionName` runs, and
the release runs `fnKeyCC(NOPARAM)`: `calcMode == CM_NORMAL` is false, the
switch takes `default`.

**Violated contract.** DESIGN.md §3.6: "Any other key, direct key path:
Ignored." and, for the set, "because their default arm shows a bug screen":
the set was enumerated by hand and is short by two.

**Bug class.** Incomplete enumeration of a mechanical class. The class is one
grep.

**Class-level test.** The step half is G1R1-1's test. The key half: for every
item with its own `case` in `processKeyAction`'s `switch(item)` (BACKSPACE,
UP, DOWN, EXIT, ENTER, CC, `op_j`, `op_j_pol`), press it in mode 21 and run
the release the way `btnReleased` does (`if(showFunctionNameItem != 0)
runFunction(showFunctionNameItem)`); assert `calcMode` is 21 or
`prevCalcMode`, never `CM_BUG_ON_SCREEN`.

### G1R1-4 — A run-time error inside the view has no surface; the first EXIT clears it silently and paints the Z register line over canvas rows 60 to 95

**Where.** Upstream `processKeyAction`, `case ITM_EXIT1`, keyboard.c
2539-2545: `else if(lastErrorCode != 0) { lastErrorCode = 0;
refreshRegisterLine(ERR_REGISTER_LINE); screenUpdatingMode = SCRUPD_AUTO;
refreshScreen(139); keyActionProcessed = true; }`. `ERR_REGISTER_LINE` is
`REGISTER_Z` (defines.h 1498). `_refreshRegisterLine` (screen.c 3229-)
clears and paints the line for every mode except `CM_BUG_ON_SCREEN`,
`GRAPHMODE` and `CM_LISTXY` (3277); the band is
`132 - 36*(Z - X)` = rows 60 to 95 (`clearRegisterLine`).

**What breaks.** Inside a running program, `displayCalcErrorMessage` sets
`lastErrorCode` and `screenUpdatingMode = SCRUPD_AUTO`; `runProgram` breaks
on the error and reaches `refreshScreen(4)`, which takes the package case:
status bar, and the menu for region 2. The error message is painted only by
`_refreshNormalScreen`. The user sees the drawing, a stopped program, and no
message. The next EXIT is consumed at the press by the error clear
(`keyActionProcessed = true`, so the release never runs `fnKeyExit`), the Z
register line is painted into the canvas, and the view stays open. The second
EXIT closes it. Any other key clears the error at the press head (2375-2380)
with `refreshScreen(138)`, which paints nothing; R/S then re-executes the
failing step and raises the same silent error.

**Reaching input.** `PVIEW 2  200 100 PIXEL  PVIEW 4  STOP`, then EXIT once.
`PVIEW 4` is typeable (TAM min 2, max 6) and raises `ERROR_OUT_OF_RANGE` by
DESIGN.md §3.2 (V3 pins it). After the run: pixel (200,139) lit, no message,
`lastErrorCode` set. After one EXIT: `calcMode` still 21, rows 60 to 95
repainted with the Z register. Any upstream error (`XEQ` of a missing label,
`1/x` of 0 with SPCRES clear) reaches the same state.

**Violated contract.** DESIGN.md §3.6: "EXIT, direct key path: `fnKeyExit`
has a case for the mode that calls `pgCloseView()`" — unconditional as
written; one EXIT does not close the view here. §3.6, the refreshScreen row:
"It does not clear or paint any other row" — true of `refreshScreen`, and the
canvas still gets painted over, from the press path. §3.2: "`PVIEW` with a
parameter other than 2 or 6 raises `ERROR_OUT_OF_RANGE` and does nothing
else" — inside a program it stops the program with no message.

**Bug class.** D4: an error raised in a mode with no error surface, plus an
upstream error-clear that runs the register-line painter with no mode gate.

**Class-level test.** Raise one package error (`PVIEW 4`) and one upstream
error inside the view with `PGM_RUNNING`. After the stop, assert either a
visible message or a cleared `lastErrorCode`. Then pin the press count: one
`processKeyAction(ITM_EXIT1)` plus the real release closes the view. Then pin
"no pixel in rows 20 to `clipY1` changes on EXIT".

### G1R1-5 — K3 drives the release directly and cannot see a swallowed EXIT; the design's key-path model is not what keeps EXIT alive

**Where.** `files/pgmGraphics.c`, `pgTestKeys`, K3:
`processKeyAction(ITM_EXIT1); assert calcMode == 21; runFunction(ITM_EXIT1);
assert calcMode == CM_NORMAL`. K2 is the same shape for ENTER, UP, DOWN,
BACKSPACE and `.d`.

**What the code does.** The real release runs a key function only through
`showFunctionNameItem` (keyboard.c 2144-2270), and the press sets it only
when `processKeyAction` left `keyActionProcessed` false (1938-1940). EXIT
stays unprocessed in mode 21 only because `case ITM_EXIT1` (2520-2555)
precedes the `default` arm that holds the package's guard. DESIGN.md §3.6
says the guard "marks every key except SNAP as processed". Under the stated
mechanism EXIT would never run; it runs because of the switch layout, which
the design does not mention. The real split is four ways: BACKSPACE, UP and
DOWN run their function at the **press** inside their own case (2440-2515);
EXIT, ENTER and CC run at the **release** through the shown name; digits,
R/S, `.d` and everything else reach the guard and never run; f and g never
engage (§6.2). K2's comment "as the keyboard does" is false for four of its
six keys, and `.d`'s no-op case is reachable only from a program step, where
it is G1R1-1.

**The mutation that proves it.** Move the guard arm above `switch(item)`, or
add EXIT to the keys it marks processed. On the keyboard EXIT is dead. K3
stays green: `processKeyAction` leaves `calcMode` at 21 either way, and the
pin calls `runFunction(ITM_EXIT1)` itself. Not applied here; a worktree run
is the completion half's job.

**Cost today.** None; EXIT works. The cost is the precedent: TESTING.md §2
rule 3 requires "the key through both key paths", and this pin proves the
function, not the gesture. The DESIGN-HISTORY red-first entry "No no-op case
in `fnKeyEnter` → K2 and K3 red through the bug screen that the release
raises" was red through the pin's own `runFunction`, not through a release.

**Bug class.** D6: a pin that drives the release directly.

**Class-level test.** The testable seam is the pair `processKeyAction(item)`
then `if(showFunctionNameItem != 0) runFunction(showFunctionNameItem)`. With
that seam K3 goes red under the mutation above, and K2 would show which of
its five keys reach their function at all.

---

## 4. PLAUSIBLE

**P1 — the opt-in f/g underline paints into region 6.** `show_f_jm` →
`toggle6UnderLines` → `underline_softkey` (screen.c 615-680) draws at
`239 - SOFTMENU_HEIGHT*y`, inside the canvas for region 6, gated by
`FLAG_FGLNFUL`/`FLAG_FGLNLIM`. Whether a shift press reaches
`showShiftState` at all in mode 21 is doubtful: `determineItem`'s shift
delegation (keyboard.c 1604) lists fourteen modes and 21 is not one, so the
f key resolves to `ITM_SHIFTf` and the guard swallows it. What settles it:
set FGLNFUL, open `PVIEW 6`, press f on the GTK simulator, read rows 217 to
239.

**P2 — interactive `PVIEW` from the CANVAS softmenu.** The path runs through
TAM; the TM_VALUE completion site was not located (§1). `leaveTamModeIfEnabled`
pops the TAM menu and writes no `calcMode` for 21. For region 6 the pop must
not repaint rows 171 to 239; `popSoftmenu` only clears `SCRUPD_MANUAL_MENU`,
and the release-end `refreshScreen(117)` takes the package case. What settles
it: run `PVIEW 2` and `PVIEW 6` from P.FN page 2 on the GTK simulator and read
rows 171 to 239 after the TAM menu pops.

---

## 5. Design observations

**O1 — a mode that is live during execution.** Modes 19 and 20 are browsers:
they take the keyboard and nothing else runs. Mode 21 is the first package
mode under which the interpreter keeps dispatching, and every upstream site
that reads `calcMode` during a step is now a composition point with no
forcing function: the four key functions of G1R1-1 and G1R1-3, `fnHRtoTM`,
the two programmable `calcModeNormal()` callers of G1R1-2, the 26 direct
assignments, the status-bar painter's two switches (benign defaults), and
`processKeyAction`'s `GRAPHMODE` remaps (which do not apply). Upstream's own
convention for the one case it met is the `effectiveCalcMode` remap inside
the function. Whatever the fix shape, G2 will add ten commands that run under
this mode; the enumeration in G1R1-1's test is the thing to keep green.

**O2 — two truths for "open".** DESIGN.md §3.7 writes `if canvas.region ==
0: fnPview(2)`; the code writes `if(calcMode != CM_GRAPHICS_CANVAS)`. Every
reader that matters uses `calcMode`; `region` is written in two places and
read in one (`pgRefreshCanvasView`, under `calcMode == 21`). The on-disk
DESIGN.md edit now rules the stale `region` "harmless, because every reader
of it runs in mode 21 only". That is true today and is exactly the kind of
sentence that stops being true one stage later. Either `region` is the open
flag and `calcMode` follows it, or `region` is a clip descriptor and the
comment `0 = view closed` goes.

**O3 — the key-path model.** The design, the pin comments, both out-of-family
packets and the fix-wave comment on disk all describe the same mechanism:
"a guard arm marks every key except SNAP as processed; six keys have their
own case and run on release". The code has three routes and the design names
one (G1R1-5). The wrong model cost the round one out-of-family finding built
on a mis-pasted excerpt (§6.1, Sol-1) and one that reasons from "shifted
R/S" (§6.1, Gemini-3). The fix-wave comment still says `.d` has its own case.
It does not. A one-table statement of "which key, which route, which
function, at press or at release" in DESIGN.md §3.6 would have prevented all
of it and is what the class-level test in G1R1-5 enumerates.

**O4 — a painted menu with no live keys.** Region 2 repaints the current
softmenu at every refresh and the three gates block every softkey. The user
sees CANVAS, PVIEW, ERASE and cannot press them. The only live keys in the
view are R/S, EXIT, and SNAP by long-press EXIT on the R47
(keyboardTweak.c 606). Design intent is clear (the register band is the
canvas, the menu band is upstream's), but the picture says otherwise.

**O5 — `screenHoldsDrawnPixels = true` in `fnPview` is inert.**
`refreshScreen` clears it on entry (screen.c 6066); its only reader is SNAP's
pre-refresh (6299), which under mode 21 takes the package case and is
harmless. Setting it costs nothing and protects nothing. Both out-of-family
readers cleared it too.

**O6 — upstream discipline (D8).** The identical-edit arm is byte-identical
to pretty-print-extra's, comment bytes included (the third member of the
identical-edit class after V38/T31/V36b; the class lesson holds). Every other
`keyboard.c` hunk sits ten or more lines from the nearest pretty-print-extra
hunk (headers: `-2763` vs `-2777`; `-3587` vs `-3577`; `-3961` vs `-3938`;
`-4465` vs `-4348`; `-4685` vs `-4612`; `-4904` vs `-4830`; `-4944` vs
`-4931`), and the combined gate proves the merge. `defines.h`: three packages
anchor at three different lines (undo-history after `CM_LISTXY`,
pretty-print-extra after `CM_BUG_ON_SCREEN`, this package after `CM_ASSIGN`);
K1 pins the value. The `softmenus.c` registry row is inserted at 180b against
upstream's "do not add menus here, add them at the end" note; the softmenu
index is not persisted (`saveRestoreCalcState.c` has no `softmenuId`), so the
shift of rows 181 onward costs wiki numbering only. The alternative (a second
tail row next to pretty-print-extra's) does not merge; the choice is
recorded in DESIGN.md §6 and stands.

**O7 — the branch.** G1 sits on `program-graphics/stage-g0`. The convention
is one branch per stage.

---

## 6. Deliberately not flagged

### 6.1 The on-disk out-of-family claims, with this reader's trace (advisory; the completion half rules)

| claim | this reader's trace |
|---|---|
| **Sol-1** "EXIT never closes the canvas view; `fnKeyExit`'s case only does `break`" | Refuted by the code. The package's `fnKeyExit` case (patch hunk `@@ -3961`) is `pgCloseView(); break;`. Packet B pasted the no-op case under the `fnKeyExit` label; the reader reasoned correctly from a wrong excerpt. A packet defect, not a code defect. The path EXIT → own case, unprocessed → shown → release → `runFunction(ITM_EXIT1)` → `pgCloseView` was traced (G1R1-5) |
| **Sol-2** CLSTK and the `calcMode = CM_NORMAL` sites abandon the view | Supported in full; carried as G1R1-2. The owner's fix wave targets it |
| **Sol-3** and **Gemini-1** "ENTER, BACKSPACE, UP, DOWN, `.d` bypass the guard and run their native functions" | Bounded. Interactively they reach the package's no-op cases (ENTER at the release; BACKSPACE, UP, DOWN at the press; `.d` never, it reaches the guard). No crash, no state change from the keyboard. The real cost is on the step path, where the no-op is the defect (G1R1-1), and in the two functions the package did not cover (G1R1-3) |
| **Gemini-2** "f and g are ignored, so SNAP and every shifted key are unreachable" | True and costless. `determineItem`'s shift delegation (keyboard.c 1604) has no arm for 21, so f resolves to `ITM_SHIFTf` and the guard swallows it. Shifted keys are "any other key" and are meant to do nothing. SNAP on the R47 is a long-press EXIT (keyboardTweak.c 606), not a shifted key; on the C47 no standard key carries `ITM_SNAP` at all. The shift indicator is not painted because the package case never calls `displayShiftAndTamBuffer`. Documenting "f and g do nothing in the view" would be enough |
| **Gemini-3** "an active shift state makes R/S and EXIT resolve to their shifted items; the user is stranded" | Precondition not reachable. `shiftF`/`shiftG` are set only by `commonShiftProcessing`, which mode 21 never reaches (1604-1616); `determineItem` resets the shift state after every non-SNAP resolution (1719-1721); during a running program the DMCP poll (lblGtoXeq.c 972) and `fnPause` (input.c 205) consume the shift keys; on the PC, `btnPressed` returns before `processKeyAction` while running or paused (1876-1897). There is no path that leaves `shiftF` true inside the view |
| Both readers' "not flagged": stale `region`, `screenHoldsDrawnPixels`, the solo-build resolution arm | Agree on all three (O2, O5, and the arm is reached in the solo build and bypassed in the combined one via forth-core's rewrite of the condition above it, `010-keyboard.c.patch` hunk `-1674,11`) |

### 6.2 Upstream-consistent behaviour, cleared

- **EXIT after VIEW or PROMPT inside the view is a dead press.** `_view` sets
  `TI_VIEW_REGISTER` and leaves it set through STOP by upstream design
  (display.c 3993, the JM comment). The next EXIT is consumed at the press to
  dismiss it (keyboard.c 2379-2383, 2529-2537). In CM_NORMAL the same press
  clears the viewed register from the screen; in the view nothing was shown,
  so the press looks dead. Same count of presses as upstream; DESIGN.md §3.6
  ("On EXIT, temporaryInformation is reset") is true of that press.
- **EXIT during a running program stops the program and does not close the
  view.** DMCP poll, lblGtoXeq.c 972 (`key == 36 || key == 33`); PC,
  keyboard.c 1876-1884. Upstream: EXIT stops first. The second EXIT closes.
- **SST and BST are dead in the view.** They reach the guard. DESIGN.md
  §3.6: "Any other key ... Ignored." A drawing program cannot be
  single-stepped past its `PVIEW`; that is the ruling, not a defect.
- **INPUT and PROMPT inside the view show nothing.** `fnInput` sets
  `PGM_WAITING` and calls `refreshScreen(10)`; `fnPrompt` is `_view` plus
  `fnStopProgram`. Both are the documented VIEW class. R/S continues.
- **`CM_CONFIRMATION` and `CM_BUG_ON_SCREEN` stacked on 21 restore it.** Both
  save `previousCalcMode` and EXIT restores it (keyboard.c 3972-3980 region;
  error.c 359). CONFIRMATION repaints the normal screen over the canvas first
  (screen.c 6176-6183); a program that asks a confirmation question mid-draw
  is rare and ERASE recovers.
- **The run loop resets `screenUpdatingMode` every step** (lblGtoXeq.c
  997-998), so `fnPview`'s MANUAL bits last one step. Intended: the package's
  `refreshScreen` case is the protection, and V4 pins it.

### 6.3 Mechanics, cleared

- `lcd_fill_rect` bounds: region 2 → `(0, 20, 400, 151)`, end row 171;
  region 6 → `(0, 20, 400, 220)`, end row 240. Both within `SCREEN_HEIGHT`;
  the whole-call drop at lcd.c 184 is not reached.
- `last_CM = calcMode` in the package case mirrors the three browser cases
  (screen.c 6148-6168); `RELEASE_END`'s `switch(last_CM)` takes `default`
  for 21, which is the CM_NORMAL behaviour.
- `pgCloseView`'s early return has one caller, the `fnKeyExit` case for 21.
- `fnErase` while closed opens region 2 (DESIGN.md §3.7). A second `PVIEW`
  keeps `prevCalcMode` (§3.5). Both designed.
- `prevCalcMode` under a non-NORMAL mode: `closeNim` runs at the release
  before any function executes (keyboard.c 2150-2160), so Sol's stipulated
  "PVIEW with `calcMode == CM_NIM`" is not reachable from the keyboard; SST
  in PEM moves the pointer and does not execute. In practice `prevCalcMode`
  is CM_NORMAL.
- `ERROR_OUT_OF_RANGE` rather than the data-type error for `PVIEW 3`: ruled
  (DESIGN-HISTORY.md, stage G1, item 1). V3 pins it.
- `PTP_NUMBER_8` with `(2 << TAM_MAX_BITS) | 6` decodes as min 2, max 6
  (tam.c 1142-1143, the PAUSE row as the model).
- The `CANVAS` menu row (items.c 2462, `CAT_MENU`) and `menu_PFN_2` slot 17:
  no sibling touches either (`grep menu_PFN_2 packages/*/patches`).
- The refresh is idempotent on the tip; the manifest matched before the fix
  wave landed.
- The status-bar painter's two `switch(calcMode)` (statusBar.c 569, 784)
  take `default` for 21, which keeps the bar's normal behaviour.

### 6.4 Named, not argued

- `#define CM_GRAPHICS_CANVAS 21` sits between `CM_ASSIGN 4` and
  `CM_REGISTER_BROWSER 5`, out of numeric order. Style; the sibling did the
  same at another anchor.
- The K1 "pin" asserts a compile-time constant. It documents the registry
  claim; it cannot go red by any mutation of behaviour.
- The `.refresh-manifest.json` base is `af7ad934a`, two commits behind the
  tip's parent. Bookkeeping.

**If the goal were correct code rather than a passed audit,** leave G1R1-5
alone until the next test-harness pass, leave O4, O5, O7 and everything in
§6.4 alone, and fix G1R1-1, G1R1-2 and G1R1-3 as one class with one test.

---

## 7. Verdict

Not as it stands. The view is right: the pins hold, EXIT and R/S work from
the keyboard, and the drawing survives STOP, which is the stage's promise.
What is wrong is one layer out. The first program a user writes after this
lands, `x ENTER x × ... PIXEL`, gives the wrong number and no sign of it.
The first complex number (`CC`) shows the firmware's bug screen. The first
`CLSTK` after `PVIEW` erases the picture at STOP. None of those is a defect
of the view; each is the view being live while the interpreter runs, which
no earlier package mode was.

Where it breaks first: G1R1-1, on any calculator, in any program with an
ENTER after PVIEW. The fix shape upstream already uses for the one case it
met is a remap inside the key function; the package would need the same in
four functions, and one enumeration test to keep the set closed as G2 adds
commands. Findings, not fixes.

---

## 8. Round and exit state

- **Round:** 1 of the program-graphics audit, subject `a0077d62b..cb2ae56e7`.
- **Readers:** in-family, this synthesis pass only. The workflow's finder and
  refutation stages returned empty lists; nothing here has been refuted by a
  second reader. Out-of-family: Gemini 3.1 Pro (High) and GPT-5 (codex
  `gpt-5.6-sol`) replies exist on disk, **not fed back**, `outOfFamily:
  'pending'`. The banner in §1 stands.
- **Exit criterion:** not met, and cannot be measured: round 1 is incomplete
  until both out-of-family replies pass through the refutation-only
  completion half with their packets recorded. Their six findings and the
  five TRACED entries of §3 are that half's input.
- **A ruling on disk changes the criterion for this package.** PLAN.md §10
  (uncommitted, dated this evening): "one round of bug hunting and fix is
  enough, then move on" — one audit round per stage, one fix wave with
  red-first pins, and the two-clean-rounds criterion does not apply. Under
  that ruling this report plus the completion half is the round, and the fix
  wave already on the working tree is the wave. Recorded, not applied: the
  ruling postdates the dispatch and is not committed.
- **The fix wave is unaudited code.** K4, the `calcMode.c` guard and the
  DESIGN.md §3.6 amendment landed on the working tree during this audit. The
  standing lesson (rounds 2 and 3 of forth-core, rounds 4 to 6 of PP18) is
  that a round's findings come mostly from the previous round's fixes. If a
  second look happens at all under the new ruling, it looks at those commits.
- **Housekeeping:** the tree is as found (the fix-wave edits are the
  concurrent session's); the detached worktree used for the tip gate was
  removed; the report file is this one, in `design-docs/forth-core/`, with the
  filename truncated after the tip as in rounds 9 and 10.
