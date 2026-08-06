# Code audit — `forth-core/stage-n`, `main..HEAD`

Round 1, 2026-08-06. Subject: the 81 commits on `forth-core/stage-n` that are
not on `main` — Stages K, L, M and N. Run under `CODE_AUDIT.md`: eight blind
dimension readers, every finding piped to refuters who did not produce it, three
lenses per finding (reachability, correctness, intent), ties to refuted.

---

## 1. Subject and coverage

**Range.** `main` (`0972a7ece`) .. `HEAD` (`486574c78`), 81 commits, 114 files,
+42316/−1567. The production surface is much smaller than that number suggests:
~30k of the insertions are `design-docs/` and the test part-headers.

**Stages in range.** K (keys mode, E10–E15), L (interactive capture, REPL, FHIST,
the fold), M (FWRD in the catalog tree, press-executes, the ASSIGN band), N (the
console — ring, render arm, dialogue, seven output words, keys-first entry).

**Read in full, by at least one reader, usually three:**
`forth_console.c`/`.h`, `forth_capture.c`/`.h`, `forth_prims.c`, `forth_bridge.c`,
`forth_compile.c`, `forth_menu.c`, `forth_inner.c` (the depth/spill bracket),
the full `keyboard.c` diff (28 hunks, 473 added lines — determineItem's plane
selection, the roll/recall arm, `executeFunction`, `processKeyAction`'s CM_AIM
arm, the whole `fnKeyExit` ladder, the CM_ASSIGN band), the `screen.c` console
arm and its wiring, `programming/manage.c`'s Forth blocks (the 859-line
insertion: `forthCaptureSuspend`/`Resume`, `forthInteractiveEnter`, the FHIST
group, `forthFoldEnter`/`Leave`/`UnwindIfDone`), `items.c`'s interactive divert
and E10/E11 toggle, `ui/tam.c`, `programming/decode.c`, `softmenus.c`,
`assign.c`'s ASSIGN band arms, all 17 generated patches (3412 lines) against
their upstream originals.

**Upstream producers were read where the new code calls them with a new buffer:**
`display.c` (`shortIntegerToDisplayString`, `real34MatrixToDisplayString`,
`longIntegerRegisterToDisplayString`), `charString.c` (`_calculateStringWidth`,
`charCodeFromString`, `stringAfterPixels`), `screen.c`'s `_doShowString` decode
loop, `softmenus.c` push/pop/show/`isAlphaSubmenu`, `calcMode.c`, `stack.c`,
`bufferize.c`'s `closeAim`, `memory.c`'s `resizeProgramMemory`.

**Specs read first** (this is the step that keeps an audit from reporting
rulings as defects): `DESIGN.md` §§1.3, 3.4, 4.1, 4.2, 5.4–5.7, 6, 8.4.1–8.4.4,
8.5, 8.7, 8.8, 11; `STAGE_K/L/M/N` sheets and all four trace files; the
`PACKET_*` set for K2, L1-1, L1-2, L1-3, L1-5C, L1-F1, L1-H, M1-2, N1-1;
`TESTING.md`; `DESIGN-HISTORY.md`'s Stage L/M/N entries; the package-manager
README's silent-green trap.

**Deliberately not covered.**
- Anything on `main`. Two pre-existing items surfaced anyway and are recorded in
  §6 rather than as findings.
- The ~12k lines of `test_capture.part.h` were pattern-scanned end to end for
  four vacuity classes (FAIL printf with no flag set, sign-vacuous comparison on
  unsigned, `goto cleanup` with no `fail = 1`, a `fail` local whose returns are
  all literals) and hand-read at the rows bearing on each finding — but the
  Stage K/L batteries' *oracles* (`test_fold_context`, `test_fold_seams`,
  `test_fold_operand_parity`, `test_fold_close_paths`, `test_history_program`,
  `test_capture_interactive_divert`, the five keys-mode tests) were not read line
  by line. A "passes on the wrong answer" defect inside those would not have been
  seen.
- `test_dict_reloc.c` and the `test_engine`/`test_params`/`test_persist` deltas:
  scanned, not read.
- The 12,983 upstream `testSuite` .txt cases: run, not reviewed.
- `assign.c` beyond the two ASSIGN band arms (~1200 lines of upstream keyboard
  layout tables).
- Arena/flash accounting: this audit changes nothing, so no high-water numbers
  are reported. The stage commits carry them.
- The DM42n itself. Everything here is sim + reading; the buffer-overflow finding
  (C1) is the one whose consequence differs on hardware and it was not run there.

**What the budget did not reach.** No out-of-family reader ran this round. That
is a hard gap against the exit criterion, not a footnote — see §8.

---

## 2. Mechanical results

| check | result |
|---|---|
| `./packages/forth-core/build-test.sh` | **GREEN**, exit 0. Full 782-target rebuild (nothing cached), forth battery `ALL PASSED`, upstream `testSuite` 1/1 OK in 56 s. |
| compiler warnings | **zero**, at `-Wall -Wextra -O3` (verified in `build.sim/compile_commands.json` for the package-owned objects). |
| `./design-docs/forth-core/design-audit.sh` | exit 1, **3 finding groups**, all budget/footprint. |
| generated output vs manifest and Git | in sync, clean. All 17 patches apply to the recorded base and reproduce the working area byte for byte (verified independently by exporting the base tree and re-applying). |
| working tree | clean of code changes throughout; the only dirt is this audit's own docs. |

`design-audit.sh` surfaced, and none of it is new information for the owner:

- **Section A.** 17 override files against a budget of 16 — the new one is
  `assign.c`, added by Stage M. 2074 added lines against a budget of 606.
- **Section D.** Contiguous added blocks ≥12 lines in upstream files grew 16 → 29,
  flagged as "new package logic is being written INTO an upstream file". The two
  largest are the 846-line `manage.c` block and the 103-line `screen.c` console
  arm.
- **Section E.** Eight allocation sites in package-owned sources, each needing a
  lifetime answer. Unchanged in this range.

Two of these are audit findings in their own right and are recorded below (C14,
C15). The `.S` defect (C7) is the one the compiler *should* have caught and did
not: `displayStack` is `uint8_t`, so `displayStack > 4` is not a `-Wtype-limits`
case — the predicate is dead by data-flow, not by type.

---

## 3. CONFIRMED findings

Fifteen, worst first, ranked by what they cost the owner. Every one survived a
three-lens refutation pass. Four of them were found independently by two readers
who could not see each other's notes; that is marked where it happened, because
independent agreement is the evidence this workflow exists to produce.

---

### C1 — `forthConsoleFormatRegister` hands a 256-byte stack buffer to a producer that uses `buf+256` as scratch

**Where.** `packages/forth-core/forth_bridge.c:248` (`char buf[FORTH_CONSOLE_FMT_MAX]`)
and `:275` (`shortIntegerToDisplayString(regist, buf, false, noBaseOverride)`).

**Reaching input.** Press FORTH. Type `1 HEX` and press ENTER.
`HEX` is `CAT_FNCT | PTP_NONE | RESULT_IN_X` (`packages/forth-core/items.c:3797`),
so it resolves from
a Forth line by §4.1 step 4; `fnChangeBase(16)` converts X to `dtShortInteger`
(`src/c47/integers.c:6-22`, unconditional). The line wrote nothing to the ring, so
`forthInteractiveEnter`'s X echo fires (`programming/manage.c:1464-1469`) and calls
`forthConsoleFormatRegister(REGISTER_X, shown, 256)`, which reaches `:275`.
The same reach exists through `.` (`forth_prims.c:101`) and `.S`
(`:116-133`, which scans X..T — a short integer parked in Y, Z or T is enough),
and with no Forth word at all: leave a short integer in X before pressing FORTH
(the open does not lift, T9) and ENTER any line that leaves it there.

**What breaks.** `shortIntegerToDisplayString` builds its digits starting at
`displayString[ERROR_MESSAGE_LENGTH / 2]` = `displayString[256]`
(`src/c47/display.c:2056`, then the stores at `:2075`/`:2110`/`:2160`/`:2202` and
the reverse-copy loops at `:2221`/`:2338` reading back from `k = i-1` down to
`256`). `ERROR_MESSAGE_LENGTH` is 512 in the package's own `defines.h:2516`, and
`FORTH_CONSOLE_FMT_MAX` is 256 (`forth_console.h:45`). So **every** short integer
writes at least one byte past the end of a 256-byte automatic array — the zero
case writes `buf[256]` before any digit exists. Base 16 at a 64-bit word size
writes ~21 bytes past; base 2 with `FLAG_LEAD0` writes ~80 (64 digits plus 16
group separators). The frame being smashed is `forthConsoleFormatRegister`'s own:
saved registers and the return address.

**Contract violated.** Not a written rule — a producer convention that every
other caller in either tree honours. `tmpString`/`TMP_STR_LENGTH` 2560
(`screen.c:3132`, `:5027`, `stringFuncs.c:320`, `registerBrowser.c:61`),
`errorMessage`/4096 (`registers.c:1648`), `ttt[500]` (`screen.c:6092`). The new
call site is the only one anywhere that passes 256. It also breaks the function's
own banner — "Formats into the CALLER's buffer and never into `tmpString`" — whose
intent was to avoid aliasing; the substituted buffer is half the size the producer
requires.

**Bug class.** *Producer/consumer buffer-size contract carried by convention and
not by the signature.* Enumerable: every `*ToDisplayString` in `display.c` that
seeds its index from `ERROR_MESSAGE_LENGTH / 2`.

**Class-level test.** Enumerate those producers, call each with a canary-padded
buffer of exactly `FORTH_CONSOLE_FMT_MAX` bytes, and assert the canary is intact —
run once per producer, driven from a table so a new `case` arm in
`forthConsoleFormatRegister` that is not in the table fails the count check.
`shortIntegerToDisplayString` is the arm that reddens today; `dtTime`, `dtDate`,
`dtReal34Matrix` and the `dtComplex34*` arms are the ones the table exists to keep
honest.

**Note.** This is the one finding whose consequence differs between sim and
target. On the sim the smashed frame is usually survivable; on the DM42n the owner
sees a reboot and loses the line being typed, or the console prints garbage. It is
reached by an ordinary session in BASE mode.

---

### C2 — the E10/E11 ALPHA gesture drains the console's own FWRD home row, after which EXIT eats the owner's menu frames

**Where.** `packages/forth-core/items.c:782` (the drain loop's break test).
*Found independently by two readers (D2 lifecycle, D4 error paths).*

**Reaching input.** Have STK displayed (any non-alpha user menu). Press FORTH →
softmenu stack `[FWRD(22), STK, …]`, `keysMode = 1`. Press f+XEQ → `items.c:747-751`
sets `keysMode = 0` and pushes ALPHA → `[ALPHA, FWRD, STK, …]`. Press f+XEQ again
→ `items.c:753` sets `keysMode = 1` and runs the drain at `:781-790`:

- iteration 0: `currentMenu() == -MNU_ALPHA`, no break; `softmenuStack[1]` is
  FWRD, dynamic softmenu id 22 (`softmenus.c:1062`), so `> 1`; `popSoftmenu()`
  removes ALPHA. `popSoftmenu`'s own CM_AIM compensation is inert — it fires only
  when the landing id is 0 or 1.
- iteration 1: `currentMenu()` is now `-MNU_FORTH`, and `isAlphaSubmenu(0)` returns
  **true** for it (`softmenus.c:3888`, the forth-core row); `softmenuStack[1]` is
  STK, `> 1`; `popSoftmenu()` **destroys the console's own home row**.
- iteration 2 breaks on STK.

With MyMenu underneath instead, iteration 1 takes the `<= 1` branch and overwrites
slot 0 raw at `:784` — the FWRD frame is gone either way.

Now press EXIT. Rung 1 is skipped (keys mode is on). Rung 2 (`keyboard.c:4119`)
tests `currentMenu() != -MNU_FORTH && != -MNU_ALPHA`, true for `-MNU_STK`, so it
pops **the owner's STK frame** and stays open.

**What breaks.** The console is in keys mode with no FWRD row, so word discovery
on the softkeys is gone and — per K-R3 — the row that *is* the mode indicator now
shows the owner's old menu. Then EXIT appears not to work: it changes the menu row
and leaves the console up, one owned frame per press, until slot 0 normalises.
Frames the owner put there are destroyed.

**Contract violated.** `DESIGN.md` §8.4.4: "The interactive capture opens with
`keysMode = 1` and `MNU_FORTH` as its home row … alpha is the excursion the
E10/E11 gesture toggles into", and "Rung 1 unwinds the ALPHA excursion back to
keys and restores the FWRD row (inverted)". The two documented ways back to keys
ground now disagree: EXIT rung 1 does `showSoftmenu(-MNU_FORTH)`
(`keyboard.c:4085`); the ALPHA gesture deletes it.

**Arguing with the comment, as required.** `items.c:757-762` justifies the drain:
"drain EVERYTHING alpha, FWRD (`-MNU_FORTH`) included … K-R3's rationale (the
underlying row IS the mode indicator) says leave nothing alpha standing when keys
mode goes on." That was written in Stage L, when FWRD was a *picker* pushed above
the alpha row and the keys-mode indicator was the non-alpha row underneath. Under
N-R6, FWRD **is** the keys-mode row, so "leave nothing alpha standing" now deletes
the indicator it was written to reveal. The comment even labels itself
"Interactive choice (provisional, flagged for owner review)" — it was known
provisional at Stage L and was not re-derived when Stage N inverted the ground.

**Why it was missed.** N-T4's `isAlphaSubmenu` consumer list
(`STAGE_N_TRACES.md`: "screen.c:913, keyboard.c:1309, :1313, :1380, :1489, :1535,
:3910, :4162, and test_persist.part.h:538") omits `items.c:782` — the one consumer
that pops the console's own home row. And `test_capture.part.h:8388-8412` [3a]
executes `items.c:782` on **every suite run** with exactly this stack; it stays
green because its assertion is `currentMenu() != -MNU_ALPHA` plus a keysMode bit
check. Nothing asserts FWRD survived. Subcase [3(b)] at `:8445-8476` goes further
and *positively requires* FWRD to be drained — the Stage L arrangement, never
revisited.

**Bug class.** *A predicate widened for one consumer, breaking a different
consumer that was never enumerated.* `isAlphaSubmenu` was taught `-MNU_FORTH` in
Stage L; the consumer set was re-derived in Stage N and came back one short.

**Class-level test.** Enumerate every caller of `isAlphaSubmenu` by grep, assert
the count matches a pinned number, and for each caller that can *pop*, assert the
console's home row survives when it runs with an interactive capture open. The
grep-count half is what makes it a class test rather than one more case.

**Severity note.** The refuters trimmed the finders' "stuck state" label: the
post-drain EXIT ladder still terminates, and Stage M put FWRD in the CATALOG tree
so the picker is recoverable by another route. What stands is destroyed owner
frames and extra presses.

---

### C3 — the REPL reopen clears `homePushed`, so EXIT after any ENTER leaves the FWRD frame on top of the owner's menu

**Where.** `packages/forth-core/programming/manage.c:1479`
(`forthCapOpenInteractive()` in `forthInteractiveEnter`'s tail).
*Found independently by three readers (D1 contracts, D2 lifecycle, D5 guards).*

**Reaching input.** From CM_NORMAL with any non-FWRD softmenu up (the default
MyMenu, MODE, anything), press FORTH. `forthEnterAimSurfaceNoLift` sets
`forthHomeWasFresh = (currentMenu() != -MNU_FORTH)` = true and pushes `-MNU_FORTH`;
`fnForthOuter` then calls `forthCapSetHomePushed(true)` (`forth_compile.c:1756`).
Type `1 2 +` — three keypresses in keys mode — and press ENTER.
`forthInteractiveEnter` calls `forthCapOpenInteractive()` at `manage.c:1479`, whose
shared body `_forthCapOpenAs` sets `forthCap.homePushed = 0`
(`forth_capture.c:17`), and unlike the other caller **this one never re-records
it**. Press EXIT: rung 3 reads `popHome = forthCapHomePushed()` = false
(`keyboard.c:4161`) and skips the pop. `calcModeNormal()` cannot compensate: its
pop is gated on slot 0 being `-MNU_ALPHA` and its rename on `softmenuId == 1`
(`src/c47/calcMode.c:42-48`), while slot 0 is `-MNU_FORTH`, id 22.

**Second path, same defect.** Open the console, press STO, type `0 5`. The fold's
`forthCaptureResume` (`manage.c:1270-1286`) restores `keysMode` and `origin`
across `forthCapOpen()` — and not `homePushed`. EXIT leaks the same frame.

**What breaks.** The console closes and the stack screen returns with the **FWRD
softkey row still displayed** and the owner's own menu buried one frame deeper.
Because Stage M made a FWRD softkey *execute* its word in CM_NORMAL
(`keyboard.c:133-146`), the next softkey press runs a Forth word against the live
stack instead of the owner's menu assignment.

**Contract violated.** `forth_capture.h:51-66`, the `homePushed` banner: "did the
interactive open DISPLACE the user's top softmenu frame with its FWRD home row?"
After one ENTER it answers a question about the *reopen* instead, and reports "no"
for an open that did displace. `keyboard.c` rung 3: "Pop ONLY what the open
pushed." `DESIGN.md` §8.4.4: the console "opens with `keysMode = 1` and `MNU_FORTH`
as its home row, and **both survive every REPL reopen**" — so after a reopen the
design requires a pop, and the code cannot produce one. `forth_capture.c:17`'s own
comment states the precondition the caller must meet — "N1-5: the open records this
itself, right after this call" — and this second caller does not.

**Evidence.** A refuter instrumented `test_console_exit_ladder` and ran the
harness (probe since removed, tree clean):

```
pre-FORTH menu = -1363 (STK)
after open:  menu = -213 (FWRD)  homePushed = 1
after ENTER: menu = -213         homePushed = 0
after EXIT:  menu = -213  open = 0  calcMode = CM_NORMAL   <-- FWRD still on top
after 2nd EXIT (CM_NORMAL): menu = -1363                   <-- recoverable
control (no ENTER) after EXIT: menu = -1363                <-- correct
```

The suite reported `ALL PASSED` on that same run. The refuters trimmed one claim:
one extra EXIT press in CM_NORMAL does recover the menu, so it is a leaked frame,
not a lost one.

**Bug class.** *Shared-body open with caller-side re-establishment: N callers,
M < N of them re-establish.* `forthCapSetHomePushed` has exactly one production
caller (`forth_compile.c:1756`) against three sites that reopen an interactive
capture.

**Class-level test.** Enumerate the fields `_forthCapOpenAs` zeroes
(`aimBuffer[0]`, `keysMode`, `historyIndex`, `homePushed`) and, for every call site
of `forthCapOpenInteractive`/`forthCapOpen`, assert the post-call value of each
field equals what that site's contract requires — with the site list pinned by
grep count so a fourth reopen site fails the test by existing.

**Coverage gap.** `test_capture.part.h:8097-8128` [6b] and `:16510-16552` [8] both
assert the menu is restored after open → EXIT with **no intervening ENTER**; the
story test at `test_console.part.h:1581-1592` deliberately declines to assert a
press count. No row exercises open → ENTER → EXIT against the softmenu stack.

---

### C4 — ENTER from the alpha excursion forces keys mode back on without taking the ALPHA row down

**Where.** `packages/forth-core/programming/manage.c:1480`
(`forthCapSetKeysMode(true)`, two lines after C3).

**Reaching input.** Press FORTH (console opens keys-first, FWRD row). Press the
f-shifted ALPHA gesture: `items.c:747-760` takes the `forthCapKeysMode()` branch →
`forthCapSetKeysMode(false); showSoftmenu(-MNU_ALPHA);`. Type a definition on the
alpha keypad — `: SQ DUP * ;` — and press ENTER. The reopen executes
`forthCapSetKeysMode(true)` with **no drain of the alpha frame and no
`showSoftmenu(-MNU_FORTH)`**. `grep -n MNU_FORTH programming/manage.c` returns no
hits at all: the file that owns ENTER never touches the console's home row.

**What breaks.** Two things the owner sees.

1. The softkey row still shows the alpha keypad while `determineItem` has switched
   to the NORMAL key plane (`keyboard.c:1856`, the
   `calcMode == CM_AIM && forthCapIsInteractive() && forthCapKeysMode()` disjunct),
   so the key physically labelled `A` now types `Σ+`.
2. Pressing EXIT to leave what looks like the alpha excursion instead **closes the
   console in one press**: rung 1 is skipped because keys mode is on, and rung 2's
   test is false because `-MNU_ALPHA` is on top, so control falls straight to rung 3.

**Contract violated.** `items.c`'s own drain rationale, quoted from K-R3: "the
underlying row IS the mode indicator" — and rung 1's contract, "Keys input is now
the console's GROUND state, so the first EXIT press unwinds the ALPHA excursion
back to it and restores the FWRD home row", which presumes
`!forthCapKeysMode()` ⇔ the alpha row is displayed. After this reopen that
equivalence is broken in the direction rung 1 cannot see. `DESIGN.md` §8.4.4 says
the home row survives every REPL reopen; §8.4.1's E10/E11 rule carries K-R3
verbatim, so a keys-mode state with `-MNU_ALPHA` displayed is exactly the state the
design forbids.

**The counter-precedent points the same way.** PEM's own ENTER arm — the "E5
relock" that `manage.c:1478`'s comment names as this code's model — calls
`_closeAlphaMenus()` and then `pemAlpha(0)` to re-establish the row matching its
relocked mode (`manage.c:1032-1038`). The interactive analogue reproduces the mode
flip and drops both menu halves.

**Bug class.** Same as C3: shared-body open, caller-side re-establishment, one of
two obligations honoured. C3 is the `homePushed` half; C4 is the menu-row half.

**Class-level test.** For every site that writes `keysMode`, assert the displayed
row matches the mode immediately afterwards — `keysMode == 1` ⇒
`currentMenu() == -MNU_FORTH`, `keysMode == 0` ⇒ `currentMenu() == -MNU_ALPHA`.
That single invariant, checked at every writer, also catches C2 and C8.

---

### C5 — history recall with nothing to recall wipes the line the owner is typing

**Where.** `packages/forth-core/programming/manage.c:1787`
(`aimBuffer[0] = 0;  /* past newest = empty */`).

**Reaching input.** Open the console, type `12 34 +`, press f-down.
`determineItem` re-homes f-up/f-down to `CHR_caseUP`/`CHR_caseDN` in **both** input
modes since Stage N (`keyboard.c:1882-1902`), and `processKeyAction`'s arms call
`forthHistoryRecall(±1)` as their first act with no other precondition
(`keyboard.c:2840`, `:2861`). The browse index is `FORTH_HIST_BROWSE_NONE` at open
and after every push, so `cur = lineCount`; `next = lineCount + 1` clamps back to
`lineCount`; `(uint16_t)next == lineCount` is true and `:1787` runs. **The same
happens on f-UP when FHIST does not exist yet or is empty** (`lineCount == 0`:
`cur = 0`, `next` clamps to 0, `0 == lineCount`) — so on a fresh calculator the
very first f-up a curious owner presses destroys whatever they have typed.

**What breaks.** The typed, uncommitted line disappears. It was never pushed to
FHIST (only ENTER and EXIT rung 3 push), the ring never saw it, and there is no
undo. The only stale copy is the mirrored capture step, and it is not
user-recoverable: no key restores `aimBuffer` from it, and the next suspend calls
`forthCapRecommitStep` (`manage.c:1174-1178`), which overwrites the step with the
now-empty buffer. Pressing f-up after the wipe yields FHIST's newest line, not the
typed one.

**Contract violated.** `DESIGN.md` §8.4.2: "f-shifted up/down recalls older/newer
into the editor" — with no older/newer entry there is nothing to recall, so the
gesture should refuse, not clear. It also runs against the L5 principle the same
section states for the sibling path ("On an interpret error the capture reopens
with the line intact from the pre-run copy (L5) — edit, don't retype") and rung 3's
"EXIT never loses a line".

**Honest caveat, carried from the finder and not resolved by refutation.** The code
matches `PACKET_L1_H_history_program.md`'s C4 pseudocode verbatim
(`if index == lineCount: aimBuffer[0] = 0 /* past newest = empty */`). This may be
a deliberate readline-style choice whose empty-history and at-the-end corners were
never separated out. The behaviour is what is flagged, not the packet. The
refutation pass ran the correctness lens and confirmed the mechanism and the
irrecoverability; nobody ran the intent lens to a ruling, because there is no
ruling to find beyond the pseudocode itself.

**Bug class.** *A no-op case implemented as a destructive default.* The clamp
produces "index == count" for both "you are already at the newest" and "there is no
history", and both are answered by clearing the editor.

**Class-level test.** For every history gesture (f-up, f-down) × every boundary
state (empty FHIST, at-newest, at-oldest) × line present/absent: assert that a
gesture with no entry to move to leaves `aimBuffer` byte-identical. Twelve cells,
fully enumerable. The four existing recall call sites in `test_console.part.h`
(`:816`, `:1224`, `:1562`, `:1613`) all pass delta −1 and all zero `aimBuffer`
immediately before the call, so no test has a typed line live across a recall.

---

### C6 — `fnForthOuter` has no already-open guard: pressing FORTH inside the console discards the line, with no history push

**Where.** `packages/forth-core/forth_compile.c:1753-1755` (the interactive arm;
the only guard is `programRunStop == PGM_RUNNING`).

**Reaching input.** Open the console and type a line (`: SQ DUP * ;`). Press f+`+`
to open the catalog, navigate to FCNS, press the FORTH softkey. `executeFunction`'s
generic AIM arm (`keyboard.c:1380`) is explicitly gated `!forthCapIsInteractive()`,
so the capture is **not** closed there; the pick reaches `runFunction`, and
`items.c:794-800`'s name-insert divert excludes `ITM_FORTH` by name, so it falls
through to `fnForthOuter`. The interactive arm runs `forthEnterAimSurfaceNoLift()`
then `forthCapOpenInteractive()`, whose first statement is `aimBuffer[0] = 0`
(`forth_capture.c:10`). Same path from a user-assigned FORTH key.

**What breaks.** The line vanishes and is not in FHIST — f-up does not bring it
back and the transcript never echoed it. If X happens to hold a string, the
reopened capture is additionally seeded from X and consumes it. It also resets
`homePushed` to 0, compounding C3.

**Contract violated.** `DESIGN.md` §8.4.2 enumerates every path that can end an
interactive line, and this is none of them: "EXIT … a non-empty line is pushed onto
FHIST first, so EXIT never loses a line", and the five native `closeAim` arms where
"the line is preserved *in X*". `STAGE_L_INTERACTIVE.md:224-227` states the rule
without cases: "every interactive close path with non-empty text pushes an entry —
run, abandon, power reset … One rule, no cases." Every designed close path either
pushes to history or commits to X. This one does neither — and it is not even a
close path (`forthCapClose` never runs), so the E14 sweep and the close-path class
test never see it.

**The one place the record touches this.** `PACKET_L1_3_divert_seam.md:92` excludes
`ITM_FORTH` from the name-insert divert on the ground that "`ITM_FORTH` re-entry" is
owned by L1-1/L1-2 — but neither packet ever specifies what re-entry *does*. That
line documents the routing and leaves the behaviour unspecified. L-R2 ("always
capture") was ruled for `ITM_FORTH` pressed with **no** capture open.
`PACKET_L1_1_origin_seams.md:425-429` notes that `runFunction(ITM_FORTH)` "would
re-open the capture" and uses that only to keep interactive rows out of the
close-path sweep — the re-open is recorded as a *test hazard*, never as a designed
user gesture.

**Bug class.** *An idempotent-looking entry point that is destructive when the
state it establishes already exists.*

**Class-level test.** For every item that opens a capture (`ITM_FORTH`, the FWRD
softkey, an assigned FORTH key), press it twice with a non-empty line between and
assert either the line survives or FHIST gained it. The enumeration is the set of
entries into `fnForthOuter`, and grep bounds it.

**Confidence.** The intent lens confirmed no ruling exists. The refuter explicitly
declined to re-verify the `keyboard.c:1380` / `items.c:796` reachability chain, so
if a later reader kills the path, this finding dies on reachability rather than on
intent.

---

### C7 — `.S` prints a constant `<4>` and never shows the top four levels of an 8-level stack

**Where.** `packages/forth-core/forth_prims.c:118`
(`uint16_t levels = (displayStack > 4) ? 8 : 4;`), printed at `:121`.

**Reaching input.** Any `.S`. With SSIZE8 on (MODE → SSIZE8), type
`1 2 3 4 5 6 7 8 .S` and press ENTER.

**What breaks.** `displayStack` is the number of stack **display lines**, written
only by `fnDisplayStack` (`src/c47/stack.c:106-108`), whose item row caps it at 4
(`dSTACK`, `packages/forth-core/items.c:3409` = `src/c47/items.c:3302`,
`(1 << TAM_MAX_BITS) | 4`) and whose other writers pass
1/2/3/`4-displayStackSHOIDISP`. The `DSTACK` config preset row holds only 1 and 4
(`packages/forth-core/config.c:177`); save/restore round-trips an already-bounded value; the
package patches none of it. So `displayStack > 4` is **dead on every path** and
`levels` is the constant 4.

Two consequences. The prefix is `snprintf(head, …, "<%u> ", levels)` — it prints the
*window size*, not the depth, so **every** `.S` reads `<4>` regardless of what is on
the stack. And under SSIZE8, registers A..D — the top half of the live Forth stack —
are silently absent from the picture: the transcript reads `<4> 8 7 6 5`.

**Contract violated.** `STAGE_N_TRACES.md` N-T5: "The Forth data stack *is* the
calculator stack plus the D3 spill region … `.S` is specified as 'depth first, then
levels until the width runs out' — the depth is the part that must never be
truncated away." The one part of the line N-T5 says must never be lost is the part
that is wrong. The engine two files away uses the right predicate:
`forthStackCapacity()` is `getStackTop() - REGISTER_X + 1`
(`forth_inner.c:127-130`), and `getStackTop()` is
`getSystemFlag(FLAG_SSIZE8) ? REGISTER_D : REGISTER_T` (`defines.h:2287`).

**Arguing with the comment.** `forth_prims.c:107-108` asserts the false premise
outright: "The visible window is what displayStack says (4 or 8)". It does not; it
says 1..4.

**Bug class.** *A display setting read as a model quantity.* The word needed "how
many registers does the Forth stack have" and asked "how many rows does the stack
display draw".

**Class-level test.** For each of the four display-stack settings × SSIZE8 on/off,
push a known depth and assert `.S`'s prefix equals the true depth and that the level
count equals `forthStackCapacity()`. Eight cells. Today `test_console.part.h:1484`
does `_consoleEnterLine(".S")` and asserts only `line[0] == '<'`, which is why the
gate is blind.

---

### C8 — EXIT rung 2's `stayInAIM()` pushes the ALPHA row over the console's FWRD home row on every stacked-menu pop

**Where.** `packages/forth-core/keyboard.c:4121`.

**Reaching input.** Open the console (keys mode, `-MNU_FORTH` on top). Press f+CHS —
key 43's `fShifted` is `-MNU_MODE` (`assign.c:26`), a negative item, so `items.c`'s
name-insert divert (`func > 0`) does not take it and `btnReleased`'s
`if(item < 0) showSoftmenu(item)` (`keyboard.c:2388-2390`) pushes MODE:
`[MODE, FWRD, user]`. Press EXIT. Rung 1 skipped (keys mode), rung 2's predicate is
true for `-MNU_MODE`, so `popSoftmenu()` reveals FWRD (id 22, so `popSoftmenu`'s own
CM_AIM compensation does not fire), then `stayInAIM()` (`keyboard.c:3877-3880`) sees
`calcMode == CM_AIM` and `currentMenu()` neither `-MNU_ALPHA` nor `-MNU_MyAlpha`, and
calls `changeToALPHA()` = `showSoftmenu(-MNU_ALPHA)`.

**What breaks.** The console is still in keys input — the bit is untouched, key
resolution keys on `forthCapKeysMode()` alone (`keyboard.c:1786`/`:1843`) and never
consults `currentMenu()` — but the softkey row shows the alpha keypad. Word
discovery is covered and the mode indicator lies. The FWRD row cannot be recovered
by EXIT: the next press sees `-MNU_ALPHA`, skips rung 2, and closes the console.

If anything the finding understates it. `stayInAIM` fires on **any** rung-2 pop
whose revealed menu is not `-MNU_ALPHA`/`-MNU_MyAlpha`, so `[CATALOG, MODE, FWRD]`
gets ALPHA pushed over MODE on the first press.

**Contract violated.** `DESIGN.md` §8.4.4: "Rung 2 asks directly whether anything is
stacked above the console's base", and §8.4.1 K-R3: "in keys mode the underlying menu
row IS the mode indicator". The rung's own comment — "An alpha submenu, a catalog,
STK, FIN — none of them is the base, so they pop one per press and the capture stays
open; the base itself falls through to rung 3" — is true of the predicate and false
of the paired `stayInAIM()`, which was correct only while the base *was* `-MNU_ALPHA`.
N-T4's rung table re-derived the predicate and left the paired call unexamined.

**Bug class.** *A predicate re-derived, its paired side-effect not.* Rung 2 was
adopted "verbatim, INCLUDING its pre-normalisation" from native CM_AIM, and only one
half of the pair was re-thought for the inverted ground.

**Class-level test.** For each rung of the EXIT ladder, assert both halves of the
post-condition: the capture state **and** `currentMenu()`. Today
`test_console.part.h:1379-1401` drives exactly this scenario with `-MNU_STK` and
asserts only `forthCapIsOpen()` and `currentMenu() != -MNU_STK` — never
`== -MNU_FORTH` — so it stays green with ALPHA on top.

---

### C9 — EXIT rung 1 re-pushes FWRD *over* the excursion's ALPHA frame instead of popping it, so rung 3's single pop reveals ALPHA

**Where.** `packages/forth-core/keyboard.c:4085` (`showSoftmenu(-MNU_FORTH)` in
rung 1).

**Reaching input.** Press FORTH with STK displayed → `[FWRD, STK, …]`,
`homePushed = 1`. Press the ALPHA gesture once → `[ALPHA, FWRD, STK, …]`. Press EXIT:
rung 1 fires, sets keys mode, and calls `showSoftmenu(-MNU_FORTH)`. `pushSoftmenu`
finds FWRD at index 1 and lifts the stack over it
(`src/c47/softmenus.c:3656-3670` — the `xcopy` copies slot 0, ALPHA, into slot 1,
overwriting the found FWRD entry) → `[FWRD, ALPHA, STK, …]`. **The ALPHA frame is
stranded, not popped.** Press EXIT again: rung 2 false, rung 3 closes,
`calcModeNormal()` does not pop (slot 0 is FWRD), the single `popSoftmenu()` removes
FWRD → `[ALPHA, STK, …]`.

**What breaks.** After any alpha excursion, closing the console lands the owner on
the normal stack screen with the **ALPHA keypad row** displayed instead of the menu
they had before pressing FORTH. A third EXIT recovers it. Combined with C3 the row
left up is FWRD instead.

**Contract violated.** `DESIGN.md` §8.4.4: "Rung 3 closes, and pops ONLY what the
open displaced" — the accounting covers what the OPEN pushed and not what the LADDER
pushed. The landed Stage L rung 1 could not strand a frame: its base already was
`-MNU_ALPHA`, so `showSoftmenu(-MNU_ALPHA)` hit `pushSoftmenu`'s slot-0 early return
(`softmenus.c:3652`). The N-T4 inversion introduced the imbalance, and rung 3 was
dispositioned **KEEP** ("the pop count is unchanged — one frame in, one frame out")
without re-deriving it for the new rung 1.

**Bug class.** Same class as C8 — the ladder pushes frames that the close accounting
does not know about. C8 is rung 2's push, C9 is rung 1's.

**Class-level test.** Assert frame-count conservation over the ladder: record
`softmenuStackPointer` before FORTH, drive any sequence of console gestures, press
EXIT until the capture closes, and assert the depth and slot 0 match the pre-FORTH
values. That one invariant catches C3, C8 and C9 together.

**Coverage.** `test_console.part.h:1364-1381` builds exactly this state and asserts
only `currentMenu() == -MNU_FORTH` — it never inspects `menu(1)` and never presses
EXIT a second time from that fixture. The stranded frame sits one slot beneath its
assertion, and that is why the rung was marked KEEP.

---

### C10 — `EMIT` accepts glyph codes whose low byte is `0x00` and writes a lone high byte into the ring

**Where.** `packages/forth-core/forth_prims.c:169-170`.

**Reaching input.** In the console type `33024 EMIT` (0x8100) and press ENTER. The
code passes the `code >= 0x8000 && code <= 0xFFFF` gate; `:170` writes `g[0]=0x81`,
`g[1]=0x00`, `g[2]=0`, so the C string handed to `forthConsoleAppend` is **one byte
long**. `_glyphBytes` (`forth_console.c:45`) sees `p[1] == 0`, returns 1, and
`_appendGlyph` stores the orphan 0x81 alone. Any code `0x??00` does it — 33024,
33280, … 65280.

**What breaks.** The record holds half a glyph. `forthConsoleLineAt`
(`forth_console.c:188`) re-pairs that high byte with whatever follows it in the same
record, so `33024 EMIT 65 EMIT` renders as **one wrong glyph** and swallows the `A`.
When the orphan is the last byte of the record, the row string handed to
`stringWidth`/`showString` ends in a lone high byte, and both decoders consume the
NUL as the glyph's second byte and keep scanning
(`charString.c:250-254`, `screen.c:1730-1732` —
`charCode = (charCode<<8) | (uint8_t)str[ch++]` is unconditional): the render walks
off the end of `line[FORTH_CONSOLE_LINE_MAX + 1]` in `_forthConsoleRender`'s frame
and paints garbage until it happens on a zero byte. No error is shown and X is
dropped.

**Evidence.** A refuter built the real binary with a probe in test 22 (since
reverted, tree clean):

```
PROBE:  err=0 lines=1  strlen=2  bytes: 81 41    <- `33024 EMIT 65 EMIT`
PROBE2: err=0 lines=1  strlen=1  bytes: 81       <- `33024 EMIT` alone
PROBE2: first glyph decoded as 8100, offset now 2 (strlen 1)
PASS: EMIT writes ASCII and two-byte glyphs, refuses a lone high byte and a string
```

Line 3 is the production decoder `charCodeFromString` run against the exact `line`
buffer the render arm paints: it returns 0x8100 and advances the offset to 2 on a
string of length 1 — one past the NUL. The suite still printed PASS.

**Contract violated.** The word's own comment two lines above: "A bare 0x80..0xFF is
a TRUNCATED glyph, not a character, and is refused — writing it would put a lone high
byte in the ring for the painter to pair with whatever follows." `DESIGN.md` §8.4.4
states the same rule normatively. The range test enforces it only for 0x80..0xFF,
not for the `0x??00` codes that are equally unrepresentable in a NUL-terminated C47
string. `forth_console.h` says it from the other side: over-cap glyphs "are dropped
WHOLE — never half, which would leave a split glyph in the ring for the painter to
decode."

**Bug class.** *An invariant enforced against one encoding of the violation and not
the other.* Shared with C11.

**Class-level test.** Enumerate the unrepresentable glyph codes — `0x00..0x1F`,
`0x7F`, `0x80..0xFF`, and every `0x??00` — and assert `EMIT` refuses each and writes
nothing to the ring. The battery currently checks 200 (`test_console.part.h:1141`)
and no `0x??00` code.

---

### C11 — the formatter's `dtString` arm truncates on a byte boundary, leaving the same orphan at the end of a record

**Where.** `packages/forth-core/forth_bridge.c:288-294` (`n > sizeof(buf) - 1` → cut,
no glyph check). The identical byte cut exists on the ENTER echo at
`programming/manage.c:1396` (`snprintf(echo, 256, "» %s", aimBuffer)`), which needs a
near-maximal line and is the harder reach of the same class.
*Found independently by two readers (D3 arithmetic, D7 design).*

**Reaching input.** From the normal screen press ALPHA, type 128 two-byte glyphs
(`AIM_BUFFER_LENGTH` is 1024, `MAX_NUMBER_OF_GLYPHS_IN_STRING` is 508; the width
guard at `src/c47/bufferize.c:597` is 1984 px against ~1200 px for 128 standard-font
glyphs), ENTER — X is a 256-byte `dtString`. Press `1`, ENTER. Press FORTH: X is not a
string, so the seed arm is skipped and Y survives. Type `SWAP`, ENTER — `pSwap` is a
type-agnostic header exchange, so X is now the long string, and `SWAP` wrote nothing
to the ring, so the X echo fires and calls
`forthConsoleFormatRegister(REGISTER_X, shown, 256)`. `n = 256 > 255` → `n = 255` →
`buf` ends with the lone lead byte of the glyph that started at offset 254.

**What breaks.** `_glyphBytes` admits the orphan as a 1-byte glyph (254+1 == 255, so
it is not dropped as over-cap), `forthConsoleLineAt` copies it back out the same way,
and the render arm's `char line[FORTH_CONSOLE_LINE_MAX + 1]` holds 255 data bytes with
the NUL at index 255. `_calculateStringWidth` (`src/c47/charString.c:249-253`) then
consumes that NUL as the glyph's second byte and re-tests `str[256]` — one past the
256-byte stack array — and the `while(string[ch] != 0)` walk continues until it meets
a zero byte. The refuter's byte simulation of the exact index arithmetic prints
`width walk final ch = 256, buffer size 256`. The ellipsis path does not save it:
`stringWidth` runs on the full line *before* `stringAfterPixels` cuts it, so the
over-read is on the measuring pass, and the `xcopy(cut, STD_ELLIPSIS, 3)` that follows
is derived from that runaway walk.

**Contract violated.** `forth_console.c:163-168` and `:188` go out of their way to
make the ring's own truncation glyph-wise — "Copies GLYPH-WISE and stops at the last
glyph that fits, so a short out buffer truncates on a glyph boundary and never leaves
a half glyph for the painter" — and `_appendGlyph` drops over-cap glyphs "WHOLE — never
half". `forth_console.h:26-35` justifies the length-prefixed record format precisely
because a glyph's second byte "may be ANYTHING". The producer feeding that ring does
not honour the invariant. Against all of that, the `dtString` arm carries only "The
string's own glyphs, not a quoted rendering", and `grep -rn forthConsoleFormatRegister
design-docs/` returns nothing — the formatter's clamp was never specified.

**Bug class.** Same as C10, from the other ingress. The ring is glyph-careful
internally and byte-careless at both of its production entry points.

**Class-level test.** Enumerate the ring's ingress points (`forthConsoleAppend`,
`AppendLine`, and every producer that feeds them: the formatter's `dtString` arm, the
ENTER echo, `EMIT`) and assert, for a payload whose glyph boundary straddles the cut,
that the resulting record's last byte is not a lone lead byte. One property, one
assertion, run per ingress.

**Accuracy note from refutation.** The finders' `.S`-with-string-in-Y route mostly
degrades to one wrong glyph rather than an overrun, because `.` and non-final `.S`
levels append a separator that the orphan swallows. The clean overrun reaches are the
plain ENTER X echo and `.$`.

---

### C12 — the roll clamps at `count-1` rather than `count-rows`, so scrolling back empties the transcript band

**Where.** `packages/forth-core/forth_console.c:222` (the clamp) and
`packages/forth-core/screen.c:5748` (the `continue` the clamp lets rows reach).
*Found independently by two readers (D1 contracts, D5 guards).*

**Reaching input.** Run eight lines (`1 .` ENTER … `8 .` ENTER) so the ring holds 16
records; the short-line editor state gives `rows == 4`. Press g-shift then UP,
repeatedly: `determineItem`'s roll arm (`keyboard.c:1884-1888`) calls
`forthConsoleRoll(+1)`, which clamps `consoleView` at `count - 1`.
`_forthConsoleRender` computes `view = viewOffset + (rows - 1 - r)` top-to-bottom and
skips any row with `view >= count`.

**What breaks.** Presses 4, 5 and 6 past the correct stopping point progressively
empty the pane from the top until a single line — the oldest — sits alone on the
bottom row with three blank rows above it. The owner cannot bring the start of the
session to the top of the pane; scrolling back "past" it deletes the view instead. A
refuter compiled the unmodified `forth_console.c` against the renderer's exact index
arithmetic:

```
view=3 -> [L1][L2][L3][L4]      (oldest at top: the correct stopping point)
view=4 -> [   ][L1][L2][L3]
view=5 -> [   ][   ][L1][L2]
view=6 -> [   ][   ][   ][L1]   (and further presses stay here)
```

**Contract violated.** The clamp's own comment, `forth_console.c:213-215`: "Clamps at
both ends rather than wrapping: **a terminal scrollback stops at the top**, it does
not cycle." It stops at the bottom. `DESIGN.md` §8.4.4 pins only "Newest line at the
bottom, older rolling upward", so the bound itself is not ruled — this argues with
the comment, not with a ruling.

**Bug class.** *A clamp computed without the viewport it clamps for.* The correct
bound is `count - rows` (or `count - 1` only when `count < rows`), and the ring module
has no `rows` — by design: "This module does not paint."

**Class-level test.** Property test over `count` × `rows`: after any sequence of
rolls, assert the top row of the rendered band is non-empty whenever
`count >= rows`. Today `test_console_view_roll`
(`test_console.part.h:590-655`) drives precisely the degenerate state — 7 lines,
roll +6 — and asserts only that the band's pixel count rose, which the single
surviving wide row satisfies.

---

### C13 — the assertion pinning `.`'s declared stack delta reads a counter that is zeroed before it is read

**Where.** `packages/forth-core/test_console.part.h:1101`
(`if (forthSpillCount() != 0)`).

**Reaching input (mutation pin).** Set `PRIM_PRINT`'s fourth field in
`forth_prims.c:256` from `-1` to `0` — the exact defect the comment at `:1088-1094`
describes. A refuter ran this in an isolated worktree through the full gate: exit 1,
and the **only** FAIL line in a 3818-line log is
`FAIL: a print-heavy line errored (11)` — the generic neighbouring check, error 11
being `ERROR_RAM_FULL`. The `%u value(s) spilled …` diagnostic never printed.

**What breaks.** `_consoleRun` → `forthOuterInterpret` → `forthOuterRun`; at nesting 0
its epilogue calls `forthDataDepthLeaveOuter` (`forth_inner.c:168-178`), which raises
`ERROR_RAM_FULL` if `forthSpillCount() > 0` **and then calls `forthSpillReset()`**,
setting `forthSpillSlots = 0` before the test ever looks. The branch is structurally
dead for every reachable input. Consequently the diagnostic that names the defect —
"`.`'s declared stack delta is wrong" — never prints, and a stack-effect regression
that spills but settles before the line ends (any case where depth drops back under
capacity) is not caught at all.

**Contract violated.** The comment the assertion carries: "The DECLARED stack delta,
not just the observable DROP … the engine spills values that should never have
spilled — invisible in X, **loud here**." It is silent here. `TESTING.md` §6.3 ("New
behavior lands with a mutation that proves the test can fail") for this specific
assertion, and the `DESIGN-HISTORY` Stage N ruling that "`.`'s declared stack delta is
load-bearing for the spill accounting" — this is that ruling's only pin.

**Bug class.** *An assertion that inspects state after the code under test has reset
it.* Enumerable: every `forthXxxCount()` accessor read outside the bracket that owns
its lifetime.

**Class-level test.** Sample spill/depth counters **inside** the run, not after it —
either a peak-watermark accessor that survives `forthSpillReset`, or a probe hook the
prim table's mutation can redden. Then re-run each of the stage's named mutations and
assert the *specific* diagnostic fires, not merely that the suite goes red.

---

### C14 — the console's `editorTop` pixel constants (128 / 67) are hand-copied from `showStringEdC47`'s geometry, with nothing forcing them to agree

**Where.** `packages/forth-core/screen.c:5725`
(`uint16_t editorTop = (yMultiLineEdOffset == 3) ? 128 : 67;`).

**Reaching input.** None today, and this is flagged as a next-merge hazard rather
than a live defect: 128 == `Y_POSITION_OF_NIM_LINE`(132) − 3 − 1 and
67 == `(yincr-1) + 1*(yincr-1) - 1` with `yincr == 35` are both correct for the landed
geometry. `checkHP` is dead in this state (`defines.h:2378` requires CM_NORMAL or
CM_NIM; the gate requires CM_AIM), so `checkHPoffset` is 0 here.

**What breaks, and the proof it is uncovered.** A refuter changed
`screen.c:3883`'s `Y_POSITION_OF_NIM_LINE - 3 - checkHPoffset` to `- 33 -` in an
isolated worktree — moving the editor's top row from 128 to 98, so the console's
bottom transcript row now sits under the editor line — verified the mutation reached
the compiled shadow, and ran the gate:

```
FORTH SELF-TEST: ALL PASSED
1/1 testSuite OK
==> BUILD + SELF-TEST GREEN.   EXIT=0
```

Nothing went red. `_consoleBandPixels` hard-codes the same literal 128 as its own band
ceiling — a third hand-copy — and the only test that renders through `refreshScreen`
asserts `> 0` on both arms, so editor ink moving into the band can only add to a
one-sided count.

**Why a merge conflict will not catch it either.** `patches/010-screen.c.patch` has
hunks at lines 3, 814, 832, 5662 and 5927 only; both drift sites (`:1659`, `:3883`) sit
in untouched regions, so an upstream edit there applies clean and silent.

**Contract violated.** `screen.c:5711-5724` claims the geometry is "DERIVED from
`yMultiLineEdOffset`, never re-measured, so the console and the editor cannot fall a
frame out of step", and `DESIGN.md` §8.4.4 repeats it. Only the *state selection* is
derived; the pixel values are duplicated constants. The arm mixes idioms in the same
expression — `Y_POSITION_OF_REGISTER_T_LINE` as a symbol two lines later, the editor
edge as a literal. `STAGE_N_CONSOLE.md` risk 7 names `screen.c` upstream drift as the
largest override risk and mitigates it with "markers around the arm, standing re-grep
discipline", which protects the arm's *placement*, not these two numbers.

**Tempering, from the refuter.** The comment's "cannot fall a frame out of step" is a
claim about temporal alignment across the long/short boundary, and that claim is true.
The overstatement is the paraphrase, not the comment. The title, the consequence and
the coverage gap all stand.

**Bug class.** *A constant copied by value across a module boundary where the symbol
was available.* The project already owns the fix idiom — `_Static_assert` pins upstream
constants twice in this package — and at least the short-line half is expressible:
`Y_POSITION_OF_NIM_LINE - 4 == 128`.

**Class-level test.** A `_Static_assert` per derived constant, plus a render test that
asserts zero console ink at or below `editorTop` — which is what the mutation above
would have reddened.

---

### C15 — `DESIGN.md`'s authoritative override list omits `assign.c` and still names `error.c`, which has no patch

**Where.** `design-docs/forth-core/DESIGN.md:1891-1896`.

**Reaching input.** The documented next step: on an upstream merge the owner re-diffs
the files this list names ("keep the override byte-identical except this one line, and
re-diff it on every upstream merge"). `assign.c` is not among them.

**What breaks.** The list reads:

> Current overrides: `items.c`/`items.h`, `defines.h`, `config.c`, `error.c`,
> `screen.c`, `keyboard.c`, `softmenus.c`, `saveRestoreBackup.c`, `core/freeList.c`,
> `programming/lblGtoXeq.c`, `programming/manage.c`, `programming/decode.c`,
> `ui/tam.c` [VERIFIED: packages/forth-core/patches/ — one generated patch per entry].

The `[VERIFIED]` tag is false in both directions. `patches/` holds 17 entries
including `010-assign.c.patch`, `010-c47Extensions__addons.c.patch` and the two
testSuite patches; it holds **no** `error.c` patch — that override was evicted in
`79ead518b`, before this range. Stage M added `assign.c` (commit `c88d23f42`) and the
M1-3 fold-in (`adaa12b8a`) updated `DESIGN.md` without touching the list.
`design-audit.sh` section A independently flags the same thing from the other side:
"override files 17 > budget 16 — a NEW upstream file is being touched."

`assign.c` is the worst file to miss: its head carries upstream's own warning "C47
Layout from Layout_template_automation template: Do not change manually" — it is
machine-regenerated upstream, so a silent whole-file override there is precisely what
the list exists to catch.

**Bug class.** *A hand-maintained inventory of a machine-derivable set.*

**Class-level test.** `design-audit.sh` already computes the true set; have it diff
that set against the `DESIGN.md` list and fail on either direction. That is the check
the `[VERIFIED]` tag claims to have run.

**Provenance, stated honestly.** This item exhausted the verification cap and entered
synthesis unverified. It was verified during synthesis by direct inspection of
`patches/` and the `DESIGN.md` text, both reproduced above. The patch content itself is
clean and minimal (2 hunks); the defect is the bookkeeping.

---

## 4. PLAUSIBLE findings

**None.** No finding survived refutation with the reaching input unconstructed.

C15 is the only item that entered synthesis unverified, and it was resolved by
inspection rather than left open — recorded above with its provenance rather than
parked here, because "nobody could construct the input" is not what happened to it.

Two findings carry a narrower caveat than PLAUSIBLE but wider than nothing, and both
say so in place: **C5** (the behaviour matches the packet's C4 pseudocode verbatim, so
it may be a deliberate readline-style choice whose corners were never separated) and
**C6** (the refuter worked the intent lens only and explicitly declined to re-verify
the reachability chain, so a later reader could kill it there).

---

## 5. Design observations

Shape, not defects. These are the reason to run the audit.

**The N-R6 inversion was carried through one consumer.** Stage N flipped what the
softkey row *means*: FWRD went from a picker pushed above the alpha row to the
console's ground-state home row. That flip was re-derived carefully in exactly one
place — the EXIT ladder, in N-T4's rung table — and nowhere else. Five confirmed
findings are the same shape seen from five code paths: C2 (the ALPHA gesture still
drains it), C3 (the reopen no longer records it), C4 (the reopen no longer re-pushes
it), C8 (rung 2's inherited `stayInAIM` covers it), C9 (rung 1 strands a frame over
it). None of them is a hard problem; collectively they say the trace answered "what
does rung *n* do now?" and never asked "what else in the tree believes the old thing
about `-MNU_FORTH`?" The `isAlphaSubmenu` consumer list in N-T4 is the visible
artefact of that: it was re-derived, and it came back one entry short, and the missing
entry is the one that pops.

**`forthCapOpenInteractive` is a contract every caller gets wrong — which is a defect
of the contract.** `_forthCapOpenAs` zeroes four fields (`aimBuffer[0]`, `keysMode`,
`historyIndex`, `homePushed`) that the *caller* is expected to re-establish. One caller
re-establishes two of them; the other re-establishes one, loudly, with a comment. There
is nothing in the signature, the header or the build that notices. A struct-returning
open, or an open that takes the intended post-state as a parameter, would make C3 and
C4 unrepresentable rather than untested.

**The ring is glyph-careful inside and byte-careless at its border.** `forth_console.c`
is the best-defended code in the range — its eviction totality argument holds, its
`_reserve` false return really is unreachable, its double-bounded record walk is the
right call for a device with no task killer, and its own comments anticipated both the
walk-underflow and the split-glyph hazards. Every defect found in it is at a *border*:
C1 (a caller's buffer too small for a producer), C10 (an ingress that admits half a
glyph), C11 (an ingress that manufactures half a glyph). The module states its
precondition; nothing enforces it where the data is produced.

**Two sources of truth for "how big is the stack".** C7 is not an off-by-one; it is
`forth_prims.c` asking the display layer a question the engine layer already answers
two files away (`forthStackCapacity()` / `getStackTop()`). Any word that needs stack
extent should be routed through the engine accessor, and the display global should not
be reachable from `forth_prims.c` at all.

**The direct-drive test pattern buys geometry coverage at the price of arm coverage.**
Exporting `_forthConsoleRender` to the self-test is ruled and well argued
(`screen.c:5692-5696`), and it is why the geometry cases exist at all. But the ratio is
now five direct-drive cases to one arm case, and the one arm case asserts `ink > 0`.
Two of the confirmed test findings (C13's dead branch, C14's uncovered constants) and
one refuted one (§6, R4) all sit in that gap. The direct-drive export is correct; what
is missing is a single `refreshScreen`-driven case with an *empty* ring, where any band
ink is provably a register and the assertion cannot be satisfied by the console's own
output.

**The footprint budgets are stale or the discipline is.** `design-audit.sh` reports 17
override files against 16, 2074 added lines against 606, and inline blocks 16 → 29. Each
individual growth is defensible and several were priced in writing (the 846-line
`manage.c` block needs upstream file-statics; the `screen.c` arm was assigned to
`screen.c` by the stage sheet). But a budget that is exceeded by 3.4× and left in place
stops being a control. Either re-baseline it with the stages' justifications attached, or
accept that section A is now informational.

---

## 6. Deliberately not flagged

Mandatory section, and the one that says whether the audit understood what it read.
Two sources merged: findings that the refutation pass **killed**, and items the finders
**cleared** on their own before reporting.

### 6a. Killed by refutation

**R1 — "the interactive suspend can run `forthCapRecommitStep` on a step that is not
the capture step, deleting a user program step" (`ui/tam.c:1181`).** Dead premise. The
consequence needs `forthHistoryEnsure()` to return false while execution continues, and
it cannot for the stated reason: `_insertInProgram` (`manage.c:717-775`) has **no failure
return** — when `freeProgramBytes < size` it calls `resizeProgramMemory`, whose OOM arm
(`src/c47/memory.c:178-190`) either `exit(-3)`s on the sim or, on `DMCP_BUILD`, calls
`backToSystem(NOPARAM)`, and since `NOPARAM`(9876) != `NOT_CONFIRMED`(9878) that takes the
else branch, sets `backToDMCP = true` and **returns**, after which the LBL/END bytes are
written anyway. So on target the FHIST label exists even in extremis and `ensure` returns
true. The only false-return route the refuter could construct is a pre-existing
undefined-opcode step truncating `scanLabelsAndPrograms`' second pass — program memory
already broken by upstream's own accounting, not a state Stage L/N creates.

**R2 — "the console EXIT ladder has no error rung".** Ruled, twice.
`STAGE_N_CONSOLE.md:416` non-goals: "No change to §8.7's error protocol or the S1
ruling"; `DESIGN.md`:2792 and the landed N1-3 comment at `manage.c:1437` both say "the
error PROTOCOL is unchanged", with the paint's lifetime delegated to native. §8.4.2
states the governing posture: "native behaviour stays native outside the ladder (the L1-2
KEEP disposition)". The ladder was not written blind to error consumption — its PEM
sibling inherits PEM's error rung because `PACKET_K2` C3 deliberately specified the Forth
rung go "immediately AFTER the lastErrorCode check"; CM_AIM simply has no such native
check to sit after. Parity is total, including the second complaint: `fnKeyBackspace`'s
CM_AIM arm likewise has no consume (only CM_NORMAL and CM_MIM do), and every key except
EXIT/BACKSPACE clears the error, so the state is not sticky. Wanting an error rung is a
proposal to diverge from native AIM, not a defect against a design that ruled the
divergence out of scope.

**R3 — "the ring hammer's per-line-cap invariant (4) is the tautology its own comment
rejects" (`test_console.part.h:357`).** Real observation, wrong conclusion, and the
refutation is the strongest piece of work in the round. The finding's load-bearing claim
was that going through the public reader "does not escape the `uint8_t`". The `uint8_t`
bounds the *source*; invariant (4) tests the *destination* length
`forthConsoleLineAt` produces, and destination == source only because the reader's copy
loop maintains it — a property of the code under test. The refuter broke that loop
(`forth_console.c:188`, `(src + 1 < len)` → `<= len`) and invariant (4) fired:
`_consoleLineLen(0)` returned 256 against a cap of 255 where the pristine reader returns
255. Further, the comment never claims line 357 catches cap failures — it names invariant
(2) as the catcher and predicts the exact mechanism ("would instead wrap the length byte,
and that desyncs the walk against `used`"). That is precisely what the finding's own
mutation produced — 4817 of 5000 iterations red. And line 357 is the *only* call to
`forthConsoleLineAt` inside the hammer, so deleting it would leave the shipped reader with
zero exercise across 5000 wrap-around states.

**R4 — "'no register paints while the console is up' is pinned by nothing"
(`test_console.part.h:520`).** The coverage observation is conceded and is recorded as a
design observation in §5 — but the split is ruled at the arm itself.
`screen.c:5692-5696` states it with its reason ("so N1-2's geometry and roll cases can
drive the renderer directly, without the menus, status bar and `screenUpdatingMode`
arithmetic a full `refreshScreen()` drags in. The ARM being wired is proven separately,
through `refreshScreen()`, by `test_console_view_arm`"), and `test_console.part.h:658-661`
repeats it in the test file. Test 11's empty case is not vacuous under its own scope
either: it pins the renderer's ruled early return ("an empty console shows an empty area,
not registers"). The exact drift the finding names is assigned by the risk register to a
different control on purpose — risk 7, markers plus re-grep — and the markers exist. The
finding's proposed fix contradicts the documented reason the export exists.

**R5 — "FHIST is pushed at EXIT rung 3 with no echo — a third, unnamed divergence".**
Ruled. The one-act invariant is site-scoped by its own wording: N-R4 is "same bytes, same
site (`manage.c:1381`)", and N-R9 closes it — "the one-act echo (N-R4) is a single call
site beside the landed push." The other push is Stage L's normative rule: "every
interactive close path with non-empty text pushes an entry — run, abandon, reset seams —
so EXIT never loses a line. One rule, no cases." An EXIT-abandoned line never ran, so
echoing it as `» line` would claim it executed; the transcript correctly has nothing to
show. N-T4's rung table enumerates "history push :4084" explicitly and dispositions rung 3
KEEP with the check being the `popSoftmenu` frame count, not the push. The finding's
appeal to N-R2's "two designed divergences, and only these two" over-reads that list —
three sentences earlier the same paragraph names a content divergence in the other
direction that the list also omits, so the list scopes lifetime behaviour, not set
equality.

**R6 — "the console renderer (110 lines) lives inside `screen.c`, the largest
override".** Ruled before implementation. `STAGE_N_CONSOLE.md:321-327` assigns the whole
body — not a three-line call — to `screen.c`: "N1-2 — the view. The screen.c arm per
N-T1: suppression gate, per-editor-state row counts, row paints, truncation, and the
roll". The merge cost is priced in the risk register as risk 7 with markers as the
mitigation, and the markers were applied (`screen.c:5662-5670`, `:6030-6035`). Neither
rule the finding invokes says what it claims: `DESIGN.md:1854` requires overrides
byte-identical "except the marked insertion" with **no size bound**, and §6's own table
records larger ruled deviations (H2 deletes upstream's `_executeOp` block; P-H7 lists
package functions living at `keyboard.c:13-46`). `forth_console.h:52-54` rules only that
the *ring* module stays display-free. What remains is a taste preference the design
considered and settled the other way.

### 6b. Cleared by the finders

**Ring arithmetic (the whole module).** Eviction totality holds — an open record is at
most 256 bytes against a 1024-byte ring, so evicting everything else always leaves ≥768
free and `_reserve`'s false return is unreachable. `consoleUsed` cannot exceed
`RING_BYTES` (both growth sites reserve first); `RING_BYTES - consoleUsed` cannot
underflow; `_lenAt + g > 255` cannot wrap. The double-bounded walk in
`forthConsoleLineCount` is redundant against the C1 invariants and its comment says so
with the reason: the unguarded failure mode is a *hang* on a device with no way to kill a
task, not a miscount. Considered and agreed with.

**`xcopy(cut, STD_ELLIPSIS, 3)` at `screen.c:5758`.** Only reached when the line exceeds
`SCREEN_WIDTH-1`, and `cut` is at `SCREEN_WIDTH-15` px, so ≥14 px of glyphs remain after
it; single-byte codes are ASCII 0x20..0x7E (all under ~9 px) and 254 bytes cannot fit in
385 px at any width. Worst case touches index 255 of a 256-byte buffer. The zero-width
escape (charCode 1) is closed by EMIT refusing codes below 0x20. It is also the landed
idiom the comment cites.

**Stale `consoleView` after eviction.** Unreachable: every writer that can evict
(`Append`, `Newline`, `AppendLine`, `Clear`) resets `consoleView` to 0, and
`Roll`/`SetViewOffset` clamp against the live count.

**The render gate's five conjuncts.** Each is justified at `screen.c:5673-5690` and
traced in N-T1, including the two non-obvious ones: `!tam.mode` rather than the fold's
forged CM_PEM, and `temporaryInformation == TI_NO_INFO` because the TI arms repaint all
four rows from inside the `REGISTER_X` call the console keeps. Nobody found a hole. The
`calcMode == CM_AIM` conjunct is unfalsifiable at the production call site — that is what
makes the self-test's direct drive safe, and the comment says so.

**`yMultiLineEdOffset` read one frame stale.** The landed `else if` block the arm replaces
reads the same global at the same point in the frame; re-measuring would put the console
and the editor a frame apart on the crossing call.

**`checkHP` forcing `yMultiLineEdOffset = 1`.** Unreachable in CM_AIM
(`defines.h:2378` requires CM_NORMAL or CM_NIM), exactly as N-R3 states. Checked hoping
to escalate C14 to reachable; it is not.

**The empty console leaving the whole stack area blank.** N-R7's ruling: "The console
replaces the stack display while open — deliberately." And nothing goes stale: the arm
sits inside the `SCRUPD_MANUAL_STACK` guard, and `_selectiveClearScreen` clears the stack
rect under the same condition.

**Prims returning after an error without the `fnDrop` their declared −1 already
accounted for.** `forthPrimInvoke` applies the delta before `fn()`, so after a refusal the
counter sits one below the truth. Inside the contract: `forth_inner.c:52-55` states it —
"It is only ever <= the true depth, so the guard can fail to fire but can never fire
falsely" — the clamp is at 0, and the line-boundary reset means the skew cannot outlive the
failed line. Leaving the operand on the stack is §8.7's protocol working, and it is what
lets the owner fix and re-run.

**`.S` reading `REGISTER_X + i` up to `i == 7`.** In range — X..T and A..D are consecutive
(`defines.h:1272-1279`). Even if `levels` could be 8 the read is safe; it cannot be, which
is C7.

**`forthDictNameByRef` truncating into `argumentName[16]` while `FORTH_NAME_MAX` is 31.**
`PACKET_M1_2_assign_band.md` answers it: every listable picker name fits the picker's own
15-byte slot cap (`test_picker_omits_long_names`), so a name that would truncate cannot be
picked.

**The ASSIGN band (`ASSIGN_FORTH_WORDS` = 24000).** The "every consumer tests this band
FIRST" rule is the classic contract-nobody-honours shape, so it was checked by
enumeration: upstream has exactly three band consumers (`assign.c:744`, `:808`,
`items.c:200`) and the package overrides all three with the FORTH branch placed ahead of
LABELS. The int16 channel is range-guarded at `keyboard.c:348`; the band dies at
`_assignItem`, which stores `(ITM_XEQ, name)` late-bound per M-R3, so nothing stale is
persisted.

**`_forthCapBuildStep`'s one-byte length field.** `FORTH_SOURCE_MAX` is 256, the seeded
open caps `aimBuffer` at 255, and every keystroke/insert site caps at
`len < 256 - inputCharLength`, so `dst[3] = n` cannot truncate. Chased specifically
because ENTER feeds `aimBuffer` straight into it.

**`forthCapInsertName`'s new `lead` term** and **`_forthCapAtCap`'s `< 256` vs
`pemAlpha`'s `len < 256 - inputCharLength`**: both checked algebraically and both correct;
the glyph test conservatively counts name *bytes* as glyphs.

**`forthConsoleClear()` in `forthCapPowerReset()` but deliberately not in
`forthCapClose()`.** The "clears one store and not its neighbour" shape. N-R2 rules the
dialogue survives close and reopen; `test_console_ring_reset_seam` pins that direction;
the comment names it.

**`forthTakeSourceFromX` dropping X on the seeded open.** Looks like it contradicts T9's
"X untouched from FORTH-press to EXIT", but T9's claim is about the *lift*; L-R2 has always
dropped a seeded source string so interpreted words see a clean stack. Two different acts,
both ruled, code matches both.

**`forthConsoleRoll` as a side effect inside `determineItem`.** `determineItem` has exactly
one caller, once per press, and the comment argues the alternative (latching "this
`CHR_caseUP` is a roll" for a downstream handler) is a one-shot flag that desyncs.

**The roll arm not gating on `calcMode == CM_AIM`.** Two readers tried to build a state
where the capture is interactive, `tam.mode == 0`, and calcMode is something else at
`determineItem` time, and neither could: the fold's forged CM_PEM lives inside
`tamProcessInput` with `tam.mode` non-zero, and N1-6's repair forces calcMode back to
CM_AIM on every path out of a committed line. Reporting it would be write-set reasoning,
which is the failure mode `CODE_AUDIT.md` names.

**The catalog-drain predicates, the E13 resume guard, `_closeAlphaMenus` returning without
popping when FWRD is slot 0.** Verified against the actual upstream sources rather than the
traces' summaries. All correct as written.

**The X-echo suppression using `forthConsoleWriteSeq` rather than
`forthConsoleHasOpenLine`.** The commit message and the comment both record why the first
shape was wrong (`.S` and `PAGE` close their own line); the counter asks the question that
was meant.

**`lblGtoXeq.c` deleting 246 lines of upstream `_executeOp`/`_executeWithIndirect*`.**
Largest deletion in the package and the first thing that looks alarming in section A. It is
the cure, not the disease: the F2 extraction lives in `programming/param_core.c`, and
deleting upstream's superseded copy means an upstream edit there *collides* instead of
landing silently in dead code. `DESIGN.md` §6 H2 records it.

**The 859-line insertion in `manage.c`.** Prime suspect for "package code in an override",
and it survives: `forthCapRecommitStep`/`forthHistoryPush`/`forthCaptureSuspend` call
`_insertInProgram` and `_closeAlphaMenus`, both file-static upstream. Moving them out would
mean exporting upstream statics — worse drift. (This is why R6 was refuted on a ruling and
this one is cleared on mechanics: the console renderer needs no statics at all.)

**Patch file-mode normalisation, `items.h` declaring a package function, `static` dropped
under `FORTH_DEBUG_SELFTEST`.** All cosmetic or gated; production linkage unchanged; tested
against a real `git apply`.

**The `#define FORTH_CONSOLE_ROW_PITCH 21` duplicating the `fnPem` listing pitch.** The
console borrows the metric for its own band; nothing requires the two to track each other,
and the comment says so. (Distinct from C14, where the two values *must* agree.)

### 6c. Pre-existing, on `main`, reported for the record only

**`softmenus.c:1062` inserts `-MNU_FORTH` into `softmenu[]` at index 022**, shifting ids
022..185 up by one, on the very line whose upstream comment reads "NOTE !! do not add menus
here, add them at the end. The menu numbers are fixed for the Wiki references." It is not
avoidable — the dynamic block must stay parallel to `dynamicSoftmenu[]` — so two upstream
rules conflict and this one had to give. Worth knowing: `DESIGN.md` P-H5 describes it as
"rows appended to BOTH arrays", which is true of `dynamicSoftmenu[]` and not of
`softmenu[]`. The only substantiable consequence is simulator-only and cosmetic:
`softmenuStack` is persisted raw in the `backup.cfg` path (`saveRestoreBackup.c:291`,
`:960`, PC_BUILD only) with no id remap, so restoring a pre-Forth backup shows the
neighbouring menu. `saveRestoreCalcState.c` does not save `softmenuStack`, so the DM42n is
unaffected.

**Two no-op edits inflating the `manage.c` patch:** `if(aimBuffer[0] == 0)` →
`if((aimBuffer[0]) == 0)` (`:993`) and a two-space re-indent at `:1014`. Zero behaviour,
two extra upstream lines in the changed set. `git log -S` puts both before `main`.

**`showRealMatrix` writes the global `tmpString` (`matrixEditor.c:1355`)** despite its
banner promising it never does, and it can *paint* from inside a Forth run
(`toDisplay` is `!toDisplayVectorMatrix || rows > 1`). Outside this audit's dimensions and
outside the range; recorded because a reader went looking for a bound on the matrix arm of
`forthConsoleFormatRegister` and found this instead. The bound itself is fine
(`MATRIX_MAX_COLUMNS` 11 and a 380 px budget keep it inside `buf[256]`).

---

## 7. Verdict

**Would I ship this?** Not without fixing C1. Everything else is shippable with known
costs; C1 is unbounded stack-frame corruption on an ordinary gesture, and on the DM42n the
observable is a reboot that eats the line the owner is typing. It is also the cheapest fix
in the report and the one with the clearest class boundary.

**Where would it break first?** In this order, from a session that does nothing unusual:

1. An owner who works in BASE mode presses FORTH and prints an integer. C1.
2. An owner who has a menu up presses FORTH, types one line, presses ENTER, presses EXIT,
   and then presses a softkey expecting their own assignment. C3 plus Stage M's
   execute-in-CM_NORMAL.
3. An owner who toggles into alpha to type a definition and toggles back. C2 destroys the
   home row; the next EXIT starts eating their menu frames, and EXIT appears not to work.
4. An owner who defines a word in alpha and presses ENTER. C4 — the keyboard now types
   `Σ+` where the row says `A`.
5. An owner on a fresh calculator who presses f-up out of curiosity while a line is typed.
   C5.

That list is the audit's real content. Four of the five are the same root cause seen from
different code (§5, first observation), and none of them is a hard bug — they are the
consequences of a correct, well-argued inversion that was carried through one consumer and
not the rest.

**What is in good shape,** and worth saying because it is most of the range: the ring
module's own arithmetic, its eviction and cap reasoning, and its load-bearing comments,
which anticipated two of the hazards a reader would go looking for. The render gate's five
conjuncts. The fold bracket and its canary recovery. The error and refusal paths inside the
engine. The upstream discipline — 17 patches that apply to the recorded base and reproduce
the working area byte for byte, a manifest in sync, a `git apply -3` applier with
conflict-marker scanning, an ASSIGN band threaded through every consumer, and one large
deletion that exists specifically to force a conflict. Nothing in the range changes upstream
behaviour for a non-Forth user outside a Forth-gated predicate.

**What I would leave alone if the goal were correct code rather than code that passes an
audit.**

- **C9.** One extra EXIT press after an alpha excursion, no lost owner state, and the fix
  touches the ladder that C3/C8 will already be touching. If C3 and C8 are fixed with the
  frame-conservation invariant in C9's class test, C9 dies as a side effect; on its own it
  is not worth a change.
- **C13.** The battery still goes red under the named mutation — via the neighbouring
  generic check rather than the diagnostic that names the defect. That is a worse error
  message, not a missing gate. Fix it when the spill accounting is next touched.
- **C12** is the borderline one. Argued from the comment rather than from a ruling, and
  self-correcting with g-down — but an owner scrolling back through a full session hits it
  every time, and the correct clamp (`count - rows`) is not expressible in the ring module
  without giving it a `rows` it deliberately does not have. That layering question is worth
  a ruling before it is worth a patch.

**Not code defects at all:** C14 (a merge hazard, unreached today, fixable with two
`_Static_assert`s) and C15 (one documentation line, which `design-audit.sh` can be taught
to check).

**If only three things are fixed:** C1, C3 and C2 — in that order. C1 for the corruption;
C3 and C2 because between them they account for the leaked frame, the destroyed frames and
the EXIT presses that do not close — the things an owner would actually report.

---

## 8. Round and exit state

**Round 1.**

**Readers.** Eight in-family dimension finders (D1 contracts, D2 lifecycle, D3 arithmetic,
D4 error paths, D5 guard reachability, D6 tests, D7 design, D8 upstream), blind to each
other by construction. Every finding then piped to refuters who did not produce it, with
distinct lenses — reachability, correctness, intent — and a standing instruction to
default to refuted.

**Yield.** 15 CONFIRMED, 0 PLAUSIBLE, 6 refuted. Four defects were reported independently
by two or three readers who could not see each other's notes (C2, C3, C11, C12); that
agreement is evidence, and it is the reason the fan-out is run blind.

**Mutations run** (all in isolated worktrees or on reverted probes; the tree this report
finishes on is the tree it started on, `git status` clean of code changes, gate green):

| what was mutated | result |
|---|---|
| `PRIM_PRINT` stack effect −1 → 0 | suite red, but only via the generic neighbour → **C13** |
| editor top row moved 30 px (`screen.c:3883`) | **suite GREEN** → **C14** |
| `_appendGlyph` cap → 4096 | invariant (2) red, invariant (4) silent → **R3 half** |
| `forthConsoleLineAt` glyph guard `<` → `<=` | invariant (4) **fires** → **R3 refuted** |
| `homePushed` probe in `test_console_exit_ladder` | wrong menu after EXIT, suite `ALL PASSED` → **C3** |
| EMIT probe in test 22 | orphan byte in the ring, suite `PASS` → **C10** |

**Exit criterion: NOT met**, on two independent counts.

1. **No out-of-family reader ran.** `CODE_AUDIT.md` step 4 makes this binding, not
   optional: "Fresh sessions of one model are not a rotation — they share a training
   distribution, and therefore the blind spots." Sol (`codex exec -s read-only -m
   gpt-5.6-sol`) is installed, authenticated and is this project's established independent
   reviewer. Until it has read this code with no dimension assigned, the round is
   incomplete regardless of the finding count.
2. **A real finding resets the count**, and this round produced fifteen. Even a clean
   round 2 would leave the criterion one round short, and the fixes for C1–C15 will be new
   code that nobody has audited.

**Recommended next steps, in order.**

1. Owner triage of §3 — decide which findings are real, per "Findings, not fixes". Expect
   to reject some; the report names the ones I would reject myself in §7.
2. Sol reads the same range with no dimension, sent where it would go if nobody had
   reviewed the code at all. Depth on what looks wrong beats breadth over what looks fine.
3. Fixes land per the standing rule: reproducer, named bug class, class-level test where
   the class is enumerable. Every finding above carries all three.
4. Round 3 audits the fixes.
