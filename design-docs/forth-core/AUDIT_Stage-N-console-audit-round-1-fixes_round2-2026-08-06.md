# Code audit — Stage N console + audit round-1 fixes, `adaa12b8a..d2fcb401c`

Round 2, 2026-08-06. Subject: Stage N (the console) and the three commits that
fix round 1's findings. Run under `CODE_AUDIT.md`: eight blind dimension
readers, every finding piped to refuters who did not produce it, three lenses
per finding (reachability, correctness, intent), ties to refuted.

Round 1 read Stages K–N and reported fifteen. This round exists because a fix is
new code and new code has not been audited by anybody. That premise paid: four of
the seven findings below are attributable to the fixes.

---

## 1. Subject and coverage

**Range.** `adaa12b8a` (M1-3, Stage M closes) .. `d2fcb401c`, 15 commits, 49
files, +10420/−321. The production surface is much smaller: 1100 inserted lines
of package source across 13 files, plus 2178 lines of test part-headers and 3742
lines of `design-docs/`.

**Commits in range.**

| group | commits |
|---|---|
| Stage N authored, amended, traces | `194a60017`, `99c6f3914`, `4daedd485` |
| Stage N implementation | `84da2ff9c` N1-1 ring, `25dc37bb8` N1-2 view, `54d265cfd` N1-3 dialogue, `1d7c21688` N1-4 words, `b96ae1b33` N1-5 keys-first, `486574c78` N1-6 acceptance + fold-in |
| audit process | `d49ff2916`, `9a1f000ad` (docs only) |
| **round-1 fixes** | `3712ea3a9` C1, `db5e5ca30` C2/C3/C4/C8/C9, `d2fcb401c` the out-of-family leak |

The three fix commits are **194 inserted / 48 deleted lines of production code
across 6 files**, plus 280 lines of new test. `forth_menu.c` (+118) and
`forth_menu.h` (+9) are entirely new; `items.c` **shrank** by 30 lines, which is
the right direction and is noted in §5.

**Read in full, by at least one reader and usually three:** `forth_console.c`/`.h`,
`forth_menu.c`/`.h`, `forth_capture.c`/`.h`, `forth_prims.c`,
`forth_bridge.c`'s `forthConsoleFormatRegister`, `forth_compile.c`'s
`forthEnterAimSurfaceNoLift`/`fnForthOuter`/`forthOuterInterpret`/
`forthCheckSourceLine`, `programming/manage.c`'s Forth blocks
(`forthCaptureSuspend`/`Resume`, `_closeAlphaMenus`, `forthInteractiveEnter`,
`forthFoldEnter`/`Leave`/`UnwindIfDone`, `forthCaptureSanitizeRestoredUi`),
`keyboard.c`'s `determineItem` plane selection and roll/recall arm, `executeFunction`,
`processKeyAction`'s CM_AIM arm and the whole `fnKeyExit` ladder, `items.c`'s
interactive divert and the rewritten ITM_AIM toggle, `screen.c`'s
`_forthConsoleActive`/`_forthConsoleRender` and their wiring in
`_refreshNormalScreen`, `forth_inner.c`'s depth/spill bracket,
`test_console.part.h` in full (all 1897 lines, tests 1–30), and the diffs of
`test_capture.part.h`, `test_params.part.h`, `test_dict_reloc.c`.

**Upstream producers were read where the new code calls them:** `display.c`
(`shortIntegerToDisplayString` including both `determineFont` arms and all six
compaction loops, `real34ToDisplayString`, `complex34ToDisplayString`, the matrix
and vector producers, `timeToDisplayString`, `dateToDisplayString`),
`charString.c` (`_calculateStringWidth`, `stringAfterPixels`, `stringWidth`),
`softmenus.c` (`pushSoftmenu`, `popSoftmenu`, `showSoftmenu`,
`showSoftmenuCurrentPart`, `menu`/`currentMenu`, `isAlphaSubmenu`, the
`softmenu[]` table and its terminator, `menu_ALPHA`), `calcMode.c`
(`calcModeNormal`, `enterAsmModeIfMenuIsACatalog`), `ui/tam.c`
(`tamEnterMode`, `leaveTamModeIfEnabled`, `tamProcessInput`'s fold bracket),
`bufferize.c` (`closeNIM`, `closeAim`), `stack.c`, `registerBrowser.c`,
`c47Extensions/keyboardTweak.c`, `assign.c`'s AIM key columns,
`src/c47-gtk/hal/lcd.c` (to establish that the band-pixel oracles measure what
they claim), and the generated font table
(`build.sim/src/ttf2RasterFonts/rasterFontsData.c`, all 710 `standardFont` glyph
widths extracted to settle the ellipsis question numerically).

**Specs read first:** `DESIGN.md` §§1.3, 8.4.1–8.4.4 and the Stage N fold-in
diff; `STAGE_N_CONSOLE.md` in full (N-R1..N-R9, non-goals, the risk register);
`STAGE_N_TRACES.md` (N-T1..N-T5); `TESTING.md` §§1–4; `DESIGN_AUDIT.md` parts
1–3; `DESIGN-HISTORY.md`'s Stage N entry; and
`AUDIT_stages-K-to-N_2026-08-06.md` in full — findings, refutations and both
cleared sections — so that C1–C15 and R1–R6 would not be re-reported.

**Deliberately not covered.**

- Anything already reported in round 1. Nine of the fifteen are still open and
  unchanged by these commits; they are named in §6b and not re-argued.
- The engine (`forth_inner.c` beyond the depth/spill bracket, `forth_dict.c`),
  the D3 spill region, the FHIST/fold internals beyond what N-R9 required, and
  the PEM origin except where the interactive path shares its code.
- The generated mirrors under `files/` and `patches/` as content — D8 read them
  as hunk and ±line profiles only.
- The 12,983 upstream `testSuite` cases: run, not reviewed.
- Arena/flash accounting. This audit changes nothing, so there are no
  high-water numbers to report; the stage commits carry them.

**What the budget did not reach.**

- **The DM42n.** Everything here is sim plus reading. C22 is the finding whose
  consequence differs on hardware and it was not run there.
- **`test_capture.part.h` (~12k lines)** was not read in full by anyone this
  round — greps and the named rows only. A "passes on the wrong answer" defect
  inside the Stage K/L oracles would not have been seen.
- `test_dict_reloc.c` (137 new lines, 8 changed by the fixes): scanned by two
  readers, read by none.
- `assign.c` beyond the AIM and `CHR_caseUP`/`CHR_caseDN` columns.
- **No out-of-family reader ran this round.** `d2fcb401c` is round 1's
  out-of-family pass landing, which is not the same thing: the fixes themselves
  have been read only in-family, and three of the seven findings are in them.
  This is a hard gap against the exit criterion — see §8.

---

## 2. Mechanical results

| check | result |
|---|---|
| `./packages/forth-core/build-test.sh` at `d2fcb401c` | **GREEN**, exit 0. Forth battery `ALL PASSED`; upstream `testSuite` 1/1 OK in 147 s. Run in a detached worktree (see the note below). |
| compiler warnings | **zero** in the whole gate log, at the package's configured flags. |
| `./design-docs/forth-core/design-audit.sh` | exit 1, **3 finding groups**, all footprint/budget. Sections B (3, at baseline), C, F, G, H clean. |
| generated output vs manifest | in sync. |

`design-audit.sh` reports nothing this round that round 1 did not already record
as C15 and §5:

- **Section A.** 17 override files against a budget of 16; **2088 added lines
  against 606**. Round 1 measured 2074, so the three fix commits cost +14 net
  upstream lines — while deleting 30 from `items.c`. See §5.
- **Section D.** Contiguous added blocks ≥12 lines in upstream files: 29 against
  a baseline of 16. Largest unchanged: the 878-line `manage.c` block and the
  103-line `screen.c` console arm.
- **Section E.** Eight allocation sites in package sources, unchanged in range.

**A note on where the gate was run, because it matters for trust.** The first
gate run was made in the owner's working tree and was contaminated: a concurrent
session began landing a fix for C16 partway through it (working-tree mtime
13:10:28, shadow 13:10:50, inside my build window). That result is discarded. The
number in the table above is from a clean `git worktree add --detach d2fcb401c`
under the scratchpad, which is exactly the audited commit and nothing else. The
audit itself made no code change; the C16 fix that appeared mid-run was that other
session's and has since been committed as `723361f58`, outside the audited range.

**Not surfaced by the mechanical half, and worth stating:** the sim build carries
`-fstack-protector-strong` (`src/c47-gtk/meson.build:19`, non-release buildtype
only) and the DMCP target build does not (`src/c47-dmcp/meson.build` — no
`-fstack-protector` anywhere in `dmcp_cargs`). That asymmetry is load-bearing for
C22.

---

## 3. CONFIRMED findings

Seven, worst first, ranked by what they cost the owner. Eleven finding reports
survived refutation; four of them were two readers describing the same defect and
are merged here, with the multiplicity recorded because independent agreement is
the evidence this workflow exists to produce.

Numbering continues round 1's: C16 onward.

---

### C16 — `forthCaptureResume` drops `homePushed`, so round 1's C3 survives on the fold path the C3 report named

**Where.** `packages/forth-core/programming/manage.c:1270-1286` (the restore
block inside `forthCaptureResume`), against the zeroing at
`packages/forth-core/forth_capture.c:17`.
*Found independently by five of the eight readers (D1 contracts, D2 lifecycle,
D4 error paths, D5 guards, D7 design).*

**Reaching input.** From CM_NORMAL with any non-FWRD row displayed (STK, MyMenu,
MODE), press FORTH. `forthEnterAimSurfaceNoLift` samples
`forthHomeWasFresh = (currentMenu() != -MNU_FORTH)` = true
(`forth_compile.c:1715`) and `fnForthOuter` sets `homePushed = 1` (`:1756`).
Type `1`, then press **STO**. STO is parameterized, so `runFunction`'s
interactive divert declines it — the divert covers `CAT_FNCT|PTP_NONE` only
(`items.c:766-771`) and its own comment says parameterized items fall through —
and `tamEnterMode` runs the interactive arm at `ui/tam.c:1180-1182`:
`if(forthCapIsInteractive()) { forthFoldEnter(func, tam.mode); } forthCaptureSuspend();`.
Type `0` `5`. `tamProcessInput`'s epilogue (`ui/tam.c:1472`) calls
`forthFoldUnwindIfDone` → `forthCaptureResume`, which at `manage.c:1282` calls
`forthCapOpen()`. `_forthCapOpenAs` zeroes `homePushed`
(`forth_capture.c:17`) and the surrounding block restores `keysWas` and
`originWas` — and only those. Press **EXIT**.

**Verified by execution.** A temporary probe driving exactly that gesture
(`showSoftmenu(-MNU_STK)` → `fnForthOuter` → `runFunction(ITM_STO)` →
`tamProcessInput(ITM_0/ITM_5)` → `fnKeyExit`), built and run headless, printed
`homePushed=1` after the open, `homePushed=0` after the fold's resume, and then
closed the capture with menu `-213` (`-MNU_FORTH`) on top where the owner had
`-1363` (`-MNU_STK`). A second EXIT restored `-1363`. Probe reverted; tree
verified clean of it.

**What breaks.** Rung 3 reads `popHome = forthCapHomePushed()` = false
(`keyboard.c:4173`) and skips the `popSoftmenu()` at `:4186`. The console closes
and the stack screen comes back with the **FWRD softkey row still displayed**,
the owner's own menu buried one frame under it. Stage M made a FWRD softkey
execute its word in CM_NORMAL, so the next softkey press runs a Forth word
against the live stack instead of the owner's menu assignment. The false bit is
then copied forward by the ENTER reopen (`manage.c:1489-1491`) for the rest of
the session. One extra EXIT press recovers the menu — this is a leaked frame,
not a lost one.

**Contract violated.** `forth_capture.h:66-67` on the field: *"Transient UI
state, never persisted; rides the same resets as `keysMode`."* It does not —
`manage.c:1284` restores `keysMode` across this exact re-open and drops
`homePushed`. `keyboard.c:4181`: *"Pop ONLY what the open pushed."* `DESIGN.md`
§8.4.4: *"Rung 3 closes, and pops ONLY what the open displaced:
`forthCap.homePushed` records whether FWRD was already the current menu."*
`forth_capture.c:17`'s own precondition — *"the open records this itself, right
after this call"* — is unmet at this third call site.

**Why it was missed.** The comment inside the restore block still reads *"Inert
until L1-F\* arms interactive suspend/resume (suspend/resume is PEM-only today,
`ui/tam.c:1181`,`:1408`)"*. L1-F armed it; `ui/tam.c:1181` is now the interactive
fold arm. The C3 fix landed at `manage.c:1485`, inside `forthInteractiveEnter` —
a different function — so it cannot cover this path. Round 1's own C3 entry
called this *"Second path, same defect"* (`AUDIT_stages-K-to-N_2026-08-06.md:262-264`).

**Bug class.** *Shared-body open with caller-side re-establishment: N callers,
M < N of them re-establish.* Named by round 1 under C3; the class-level test it
specified was never written. Round 1's §5 called the same thing "a contract every
caller gets wrong".

**Class-level test.** Enumerate the fields `_forthCapOpenAs` zeroes —
`aimBuffer[0]`, `keysMode`, `historyIndex`, `homePushed` — and drive **every**
call site of `forthCapOpen` (the fresh interactive open, the REPL reopen at
`manage.c:1487`, the fold resume at `manage.c:1282`, the PEM open), asserting the
documented post-state of each field at each site. Driven from a table so a new
caller that is not in it fails a count check. Note that **no test anywhere reads
`forthCapHomePushed`** — a grep across `test_capture.part.h` and
`test_engine.part.h` returns nothing — so this defect is invisible to the whole
battery, not merely to the acceptance row that walks the gesture.

---

### C17 — `forthConsoleShowSurface` retargets slot 0 on menu identity alone, so it rewrites the owner's own frame, which a later `calcModeNormal()` then destroys

**Where.** `packages/forth-core/forth_menu.c:316-325` (the retarget branch).
*Found independently by four readers (D1 contracts, D5 guards, D7 design,
D8 upstream). Reproduced RED by mutation.*

**Reaching input.** Browse to FWRD through the CATALOG tree so the **owner's own**
FWRD frame is slot 0 — the state `forth_capture.h:54-56` names verbatim. Press
FORTH: `forthHomeWasFresh = (currentMenu() != -MNU_FORTH)` is **false**,
`showSoftmenu(-MNU_FORTH)` early-returns on the slot-0 match
(`src/c47/softmenus.c:3652-3654`), and `homePushed = 0` — correctly meaning "slot
0 is not ours". Press f+XEQ: `items.c:761` calls `forthConsoleShowSurface`, which
takes the `cur == -MNU_FORTH` branch and overwrites `softmenuStack[0].softmenuId`
with the ALPHA index. It never asks whether that frame is the console's. Type
`XEQ 'CLSTK'` and press ENTER: `fnClearStack` → `calcModeNormal`, whose
`softmenu[softmenuStack[0].softmenuId].menuItem == -MNU_ALPHA` guard
(`src/c47/calcMode.c:44-46`) now matches and pops the frame.
`forthConsoleRestoreSurface` (`manage.c:1439`) scans the whole stack, finds no
console row anywhere (`forth_menu.c:372-379`), sets `homePushed = true` and
pushes a replacement. EXIT, EXIT.

**Verified by mutation.** `test_console_frame_conservation` was extended with two
rows whose owner fixture is `showSoftmenu(-MNU_CATALOG); showSoftmenu(-MNU_FORTH);`
instead of `-MNU_STK`. The row that runs the CLSTK line went red:

```
FAIL: [XPROBE own-FWRD, alpha, CLSTK line, EXIT] did not restore the owner's row
      — top is now -1318, the owner had -213
```

`-213` is `-MNU_FORTH`, `-1318` is `-MNU_CATALOG`: the owner's FWRD frame was
eaten and the catalog level beneath it revealed. The sibling row with a plain
`1 2 +` line passed, which pins the differential — the retarget alone is
symmetric and benign, and it only bites through a `calcModeNormal()` line. Probe
reverted.

**What breaks.** The owner pressed FORTH from their own FWRD row and gets back
whatever was under it. Frame **count** is conserved, which is why the landed
seven-session battery passes; frame **identity** is not, and unlike C16 this one
is not recoverable with another press — the frame is gone. The same shape reaches
through a pre-FORTH `-MNU_ALPHA` row, where the retarget creates a duplicate
instead of consuming the original.

**Contract violated.** `forth_capture.h:51-66` makes ownership explicit —
`homePushed` records whether the open *"DISPLACE[d] the user's top softmenu
frame … It does not when FWRD was already the current menu — precisely the state
you reach by browsing the CATALOG tree before pressing FORTH"* — and
`forthConsoleShowSurface` never consults it. `keyboard.c:4180-4184` already knows
ownership is conditional: *"popping anyway ate the user's own FWRD frame and
revealed whatever was beneath it."* The function's own fall-through comment
states the correct rule for the other direction (*"Something the USER stacked is
on top … Leave it: it is theirs"*, `forth_menu.c:327-329`) and the retarget branch
above it does not apply that rule when what the user stacked happens to be FWRD
or ALPHA.

**Bug class.** *Ownership inferred from a value two different owners can hold.*
This is a second instance of the class `d2fcb401c` already records as latent from
the out-of-family reader (*"an in-place retarget can leave a duplicate row deeper
in the stack when the owner's pre-console menu was ALPHA"*); that one duplicates
a frame, this one consumes one, and both come from the same missing conjunct.

**Class-level test.** Extend the frame-conservation battery with fixtures whose
**owner row is one of the console's own two rows** (`-MNU_FORTH`, and separately
`-MNU_ALPHA`), crossed with the alpha toggle and with a `calcModeNormal()`-calling
line, and assert **slot-0 identity** rather than only the frame count. That is
exactly the pair of rows the landed battery's comment at `test_console.part.h:1866-1869`
concedes it delegated to the M1-1 `[8]` fixture — which never toggles and never
runs a line.

---

### C18 — three callers commit the sub-mode flip and then call a function entitled to do nothing, so the row lies about the input mode

**Where.** `packages/forth-core/forth_menu.c:325-330` (the fall-through), reached
from `keyboard.c:4083` (EXIT rung 1), `items.c:760` (the E10/E11 toggle) and
`programming/manage.c:1501` (the REPL reopen). All three write `keysMode`
unconditionally and then call `forthConsoleShowSurface`, which may legally
return having changed nothing.
*Found by three readers (D2 lifecycle, D5 guards, D7 design). One narrower
framing of the same state was **refuted** — see R11 in §6a; the severity below is
what survives that.*

**Reaching input (a), the one that types wrong characters.** Press FORTH. Press
f+XEQ into the alpha excursion — slot 0 is now `-MNU_ALPHA`. Press softkey 1 on
that row: `menu_ALPHA[0]` is `-MNU_ALPHA_OMEGA` (`softmenus.c:1015`), a negative
item, so `executeFunction`'s `else if(item < 0)` arm (`keyboard.c:1153`, →
`showSoftmenu` at `:1184`) pushes the Greek keypad over the console's row. Press
**EXIT** without inserting a glyph (inserting one auto-pops the submenu at
`keyboard.c:1541`). Rung 1's predicate is `!forthCapKeysMode()`, true, so it sets
`keysMode = 1` and calls `forthConsoleShowSurface`, which sees
`cur == -MNU_ALPHA_OMEGA` — neither `want` nor one of its own two rows — and
falls off the end. Now press the key labelled **A**: `determineItem`'s
`calcMode == CM_AIM && forthCapIsInteractive() && forthCapKeysMode()` disjunct
(`keyboard.c:1843`) selects the NORMAL plane, so it resolves as `ITM_Σ+` and the
divert types `Σ+` into the line while the Greek keypad is displayed. Pressing
f+XEQ again produces the same state in one press instead of two.

**Reaching input (b), the press that appears to do nothing.** Same open, same
excursion, then f+`+`: in the AIM plane that key's `fShiftedAim` is
`-MNU_AIMCATALOG` (`assign.c:48`), which `btnReleased` pushes
(`keyboard.c:2387-2390`). `enterAsmModeIfMenuIsACatalog` has no case for it, so
`catalog` stays `CATALOG_NONE` (`src/c47/calcMode.c:198`) and `fnKeyExit`'s
catalog intercept at `:3929` does **not** fire. EXIT reaches rung 1, flips the
sub-mode, and changes nothing on screen. While `catalog != 0` the typed
characters do not change, so this shape costs a press and a lie rather than a
wrong token.

**What breaks.** K-R3's rule that the underlying row **is** the mode indicator.
`FLAG_ALPHA` stays set in both sub-modes, so the status bar is not an indicator
either; the row is the only one there is, and in these states it is wrong. The
condition is self-healing — one further EXIT press reaches rung 2, pops the
stacked row and retargets — so the cost is a wrong-typed token and a press, not
the session.

**Contract violated.** `DESIGN.md` §8.4.4: *"Rung 1 unwinds the ALPHA excursion
back to keys and restores the FWRD row (inverted)."* With anything stacked it
restores nothing. `forth_menu.c:268-269`: *"While an interactive capture is open,
the console owns EXACTLY ONE softmenu frame: FWRD in keys input, ALPHA in the
alpha excursion."* `DESIGN.md` §8.4.1, unchanged by the Stage N amendment:
*"alpha→keys closes the alpha menus (the underlying menu row IS the mode
indicator, K-R3)."* Nothing now closes them — the drain that did was removed
wholesale in `db5e5ca30` because it also popped FWRD, which was C2. And it is the
defect the C4 half of that same commit states in its own words at
`manage.c:1498-1501`: *"the row says A where the key now types Σ+."*

**Bug class.** *A state change committed by the caller and the display of that
state established by a callee that may decline.* The callee acquired the right to
decline in `db5e5ca30`; none of its three callers was updated to check.

**Class-level test.** The invariant round 1 proposed for C4 and nobody landed: at
every writer of `keysMode`, assert the displayed row matches the sub-mode. As a
battery — for each of {toggle, EXIT rung 1, REPL reopen} × {console's own row on
top, a stacked alpha submenu, a stacked non-alpha menu} — assert that
(`forthCapKeysMode()`, `currentMenu()`) is one of the two legal pairs, **or**
that the sub-mode did not move. The disjunction is deliberate: refusing the flip
is as valid a fix as forcing the row, and the test should not pick. The landed
`test_console_exit_ladder` sets `keysMode` false with a plain
`showSoftmenu(-MNU_ALPHA)` and stacks `-MNU_STK` only from a fresh open, where
`keysMode` is already true and rung 1 short-circuits — so the
`keysMode == 0` + foreign-row-on-top combination is exercised nowhere in either
harness.

---

### C19 — the ENTER error arm appends the message into a word's still-open output record, where the line's own output can truncate it off the right edge

**Where.** `packages/forth-core/programming/manage.c:1448`.

**Reaching input.** Open the console. Type a line that prints and then fails:
`1 . 2 . BOGUS`. `.` is `PRIM_PRINT` (`forth_prims.c:99-106`), which does
`forthConsoleAppend(shown); forthConsoleAppend(" ")` and never calls
`forthConsoleNewline`, so the ring record stays **open**. The interpreter runs
tokens sequentially (`while (lineOK && nextToken(buf))`, `forth_compile.c:660`),
so the output really does land before `BOGUS` raises, and the ENTER gate
(`forthCheckSourceLine`) is tier-1 structural only, so an undefined token is a
run-time raise rather than a refusal. Press ENTER: the error arm calls
`forthConsoleAppendLine(errorMessageOf(lastErrorCode))`, and
`forthConsoleAppendLine` is `Append` + `Newline` whose `_openRecord` is
explicitly idempotent (`forth_console.c:70-76`), so the message lands **inside**
the word's open record. The success path two blocks below closes first —
`else if (forthConsoleHasOpenLine()) { forthConsoleNewline(); }` (`:1477-1479`).
The two post-run arms disagree.

**Aggravated form, one typed line.** The 256-byte capture cap admits ~63 `1 . `
groups, i.e. ~126 output glyphs, far past `SCREEN_WIDTH - 1`, while the combined
record stays under `FORTH_CONSOLE_LINE_MAX` (255) so the message is appended
rather than dropped. `_forthConsoleRender` then cuts at `SCREEN_WIDTH - 15` and
`xcopy(cut, STD_ELLIPSIS, 3)` overwrites the tail (`screen.c:5750-5754`) — which
is exactly where the error text sits.

**What breaks.** The transcript shows `1 2 Undefined routine` as one row instead
of two; with wide output the error text is the part that gets ellipsized away.
And because `_forthConsoleActive()` requires `lastErrorCode == 0`
(`screen.c:5688-5695`), the console only paints *after* the native error paint is
dismissed by the next key — so the ellipsized row is the only surviving record
that the line failed at all.

**Contract violated.** The N1-3 comment at `manage.c:1442-1446` states the reason
the line exists: *"§8.7's error PROTOCOL is unchanged — the native paint still
covers the area until the next key — but that paint is transient, and the
transcript line is what keeps the dialogue readable afterwards."* A message
merged into a word's output row and then truncated off the edge does not. N-R4
specifies the error echo as one of the two post-run echoes, parallel to the
result echo — and the result echo's arm closes the open line first.

**Bug class.** *Two arms of one post-condition, one of which re-establishes an
invariant the other assumes.* Enumerable: every exit path from
`forthInteractiveEnter`'s run, against "is the ring's open record closed".

**Class-level test.** For each post-run disposition — clean line with no output,
line with output, error with no output, **error after output**, E9 refusal —
assert the record count and the byte content of the last two records. The landed
error-echo test at `test_console.part.h:832-882` runs only `NOSUCHWORD`, a line
with no output, so its two-line expectation never sees the open-record case.

---

### C20 — `EMIT` truncates an out-of-`int32` long integer into the accepted glyph range and prints a wrong character instead of refusing it

**Where.** `packages/forth-core/forth_prims.c:155`
(`longIntegerToInt32(li, code)` into an `int32_t`).

**Reaching input.** From the normal screen type `1099511627841` (= 2^40 + 65) and
press ENTER: `closeNIM`'s `NP_INT_10` arm is a bare `stringToLongInteger` with no
magnitude test (`src/c47/bufferize.c:2418-2435`), so X is `dtLongInteger`. Press
FORTH — the open does not lift (T9) and touches no register, so X survives. Type
`EMIT`, press ENTER. `pEmit` takes the `dtLongInteger` arm at `:151`;
`longIntegerToInt32` expands to `code = mpz_get_si(op)` with `int32_t code`, and
the low 32 bits of 2^40+65 are 65 on both the sim and the 32-bit target. `code`
passes the `code >= 0x20 && code <= 0x7E` gate at `:166`, `A` goes into the ring
and `fnDrop` runs. Verified against real GMP: `2^40+65 → code=65, gate=1,
glyph=A`. Reachable inside one line too: `2147483647 2147483647 + 67 + EMIT` —
`pPlus` is `fnAdd` and `addLonILonI` is an uncapped `longIntegerAdd`
(`src/c47/mathematics/addition.c:97-109`), giving 2^32+65, measured the same way.

**What breaks.** The transcript shows a letter the owner never asked for and X is
silently consumed, where the word is specified to raise *Out of range* and leave
the stack alone. No error, no diagnostic; the wrong glyph is indistinguishable
from a correct one.

**Contract violated.** `DESIGN.md:2810-2812`: *"`EMIT` takes the encoding
`showStringEdC47` decodes: 0x20..0x7E as one byte, 0x8000..0xFFFF as two. A bare
0x80..0xFF is a truncated glyph and is refused."* 1099511627841 is not in that
set. The word's own banner at `forth_prims.c:140-145` says the same, and the
`else` arm at `:172-174` (`ERROR_OUT_OF_RANGE`) is the enforcement the cast
bypasses. The `dtReal34` sibling arm does **not** have the hole:
`decQuadToInt32` flags out-of-range and returns 0, which the same gate then
refuses. Two arms of one word disagree about the same input magnitude.

**Bug class.** *Silent narrowing at a type boundary in front of a range gate — the
gate then validates the truncated value.* Enumerable: every conversion in the
package that lands a `longInteger` or `real34` in a fixed-width int before a
range test.

**Class-level test.** A table of (magnitude, expected disposition) crossing both
arms of the word: {in range, 0x80..0xFF, > 0xFFFF, > `INT32_MAX`, **a value
whose low 32 bits are in range**} × {`dtLongInteger`, `dtReal34`}, asserting the
same disposition from both arms and that X is untouched on every refusal. The
last magnitude class is the one that matters and the one no existing test uses.

**Note.** The finder's second reproducer (`2147483647 2147483714 +`) is bad
arithmetic — the second literal exceeds `INT32_MAX` so `parseNumberAsInt32`
rejects it and the line falls back to a real34 literal, which takes the safe arm.
The corrected in-line reproducer is the one given above. That is a correction to
the writeup, not to the finding.

---

### C21 — nothing in the battery can fail if the console stops suppressing the register paints

**Where.** `packages/forth-core/test_console.part.h:520` (the assertion that
advertises itself as this check), against `packages/forth-core/screen.c:6036-6042`
(the `else` that carries the ruling).

**Mutation, applied and observed and reverted.** Change `screen.c:6039` from
`else if(yMultiLineEdOffset == 3) {` to `if(yMultiLineEdOffset == 3) {`, so that
`_forthConsoleRender()` **and** `refreshRegisterLine(REGISTER_T/Z/Y)` both paint.
The gate stays green: exit 0, `FORTH SELF-TEST: ALL PASSED`, upstream `testSuite`
GREEN. Three runs — mutated, reverted, mutated — with `test_console_view_arm`'s
own number moving **1045 → 1875 → 1045 → 1875** lit px, so the 830 px of
register numerals landing inside the 24..127 transcript band is the mutation and
not run-to-run noise. The mutation was verified present in
`build.sim/custom_pkg_shadow/screen.c` rather than assumed, because the package
system compiles the shadow.

**Why it cannot fail.** Four of the five N1-2 view cases —
`test_console_view_paints`, `_rows`, `_roll`, `_placement` — call
`_forthConsoleRender()` directly, and that function contains no register-paint
call by construction, so no input can reach the register path through them. The
fifth, `test_console_view_arm`, does reach it through `refreshScreen()` but its
only oracles are `viaArm > 0` and `yielded > 0` — lower bounds, which extra
register ink satisfies *more* easily. And the assertion at `:520` that names the
defect in its message (*"got %d lit px — a register leaked through"*) is dead:
`forthConsoleClear()` at `:515` makes `_forthConsoleRender()` return at its
`count == 0` guard before painting anything, so `empty` is 0 whatever the
register code would have done. The three `forthPushInt32` calls at `:512`, whose
stated purpose is *"the stack holds values throughout"*, cannot influence any
direct-render case.

**What breaks on the calculator.** X/Y/Z/T numerals paint over the transcript
rows — registers draw right-aligned in `numericFont` on the same band the console
fills — leaving the dialogue unreadable, and the suite reports ALL PASSED. This
is the single most visible way the stage's central feature can break.

**Contract violated.** `STAGE_N_CONSOLE.md:202` (N-R3): *"No register paints
while the console is up."* Folded into `DESIGN.md:2766-2767` as normative:
*"While an interactive capture is open the transcript replaces the T/Z/Y
paints."* And the test's own header at `test_console.part.h:503-505`: *"an empty
console shows an EMPTY area — which is also how 'no register paints while the
console is up' is proven, since the stack holds values throughout."*

**Bug class.** *An oracle placed where the mechanism under test cannot reach it.*
Same shape as C22 below and as round 1's C13.

**Class-level test.** Give `test_console_view_arm` a second `refreshScreen()`
pass with a **non-empty** transcript and values loaded into T/Z/Y, and assert the
band pixel count **equals** the count from a direct `_forthConsoleRender()` of
the same transcript. An equality, not a lower bound — that is the one oracle the
mutation cannot satisfy. Round 1's §5 asked for the mirror of this (an arm case
with an empty ring, where any band ink is provably a register); both are needed
and neither exists.

---

### C22 — the C1 class test's canaries guard a buffer no producer ever receives, and its enumeration claim is false

**Where.** `packages/forth-core/test_console.part.h:1714`/`:1720` (the canary
checks), `:1725` (the NUL check) and `:1665-1668` (the four-entry type table).
*Found independently by two readers (D4 error paths, D6 tests).*

**Why the oracle cannot fire.** `forthConsoleFormatRegister` writes the caller's
`out` in exactly one place — `forth_bridge.c:328-332`,
`xcopy(out, buf, n)` with `n` clamped to `outSize - 1` and `out[n] = 0`
unconditional. Every arm of the switch is handed the **function-local**
`char buf[FORTH_CONSOLE_FMT_MAX]` declared at `:248`, or `tmpString`. No producer
is ever handed `out`, so a producer overrun lands in
`forthConsoleFormatRegister`'s own frame and can never reach `g.front`/`g.back`;
and `out[n] = 0` with `n <= outSize - 1` makes the NUL check at `:1725`
unconditionally true.

**Mutation, applied and observed and reverted.** The C1 defect transplanted onto
an uncovered arm — `memset(buf + FORTH_CONSOLE_FMT_MAX, 0xAA, 16);` in the
`dtComplex34` arm — left the gate green (exit 0, ALL PASSED, upstream GREEN) with
the test itself printing *"PASS: every formatter arm stays inside the caller's
buffer; the C1 gesture echoes cleanly"*. The control does not merely skip that
arm; it green-lights a live overrun on it while asserting the contrary.

**Enumeration.** The switch has 10 arms — `dtReal34`, `dtComplex34`,
`dtLongInteger`, `dtShortInteger`, `dtTime`, `dtDate`, `dtString`,
`dtReal34Matrix`, `dtComplex34Matrix`, `default` (`forth_bridge.c:255-326`). The
table is `uint8_t types[4]` = `dtLongInteger`, `dtReal34`, `dtShortInteger`,
`dtString`. There is no count check against the switch.

**What actually reddened the original C1 mutation** was the compiler's stack
protector (`*** stack smashing detected ***`, exit 134, per `3712ea3a9`'s own
message) firing inside `forth_bridge.c` — a build-configuration side effect, not
this test's oracle. **And that backstop does not exist on target.** The sim build
adds `-fstack-protector-strong` in the non-release buildtype
(`src/c47-gtk/meson.build:19-20`); `src/c47-dmcp/meson.build`'s `dmcp_cargs`
contains no `-fstack-protector` at all. So on the DM42n — where C1's consequence
is a reboot that eats the line being typed — the C1 bug class is guarded by
neither the test nor the compiler.

**Contract violated.** The test's own banner at `:1648-1651`: *"The class is
ENUMERABLE (every arm of `forthConsoleFormatRegister`'s switch), so this drives
all of them through a canary-guarded buffer. A producer that scribbles past what
it was given trips the canary regardless of what the returned text looks like."*
Both halves are false as written. `3712ea3a9`'s message repeats the claim, and
the test's PASS banner prints it.

**What this finding does NOT claim.** It does not assert a live overflow in an
uncovered arm. The reading that the skipped matrix arms are class members was
**refuted** (R7, §6a): the class round 1 named — *"every `*ToDisplayString` in
`display.c` that seeds its index from `ERROR_MESSAGE_LENGTH / 2`"* — has exactly
one member, verified by grep, and the matrix bound was examined and ruled fine in
round 1's §6c. The subject arm of C1 is genuinely closed. What is wrong is the
control, not the fix.

**Bug class.** *A canary guarding an object the code under test never writes.*
Enumerable at the harness level: every canary-style test in the battery, against
"does the function under test write the guarded object at all".

**Class-level test.** Move the guard to where the writes land: call each producer
**directly** with a canary-padded buffer of exactly `FORTH_CONSOLE_FMT_MAX`
bytes, which is what round 1's C1 entry specified
(`AUDIT_stages-K-to-N_2026-08-06.md:153-158`), and add the count check it also
specified so a new `case` arm not in the table fails. The fixture's type
assertion at `:1700` — *"THE FIXTURE MUST PROVE IT REACHED THE STATE IT CLAIMS TO
TEST"* — is the half that works and should be kept.

---

## 4. PLAUSIBLE findings

**None.** No finding survived refutation with the reaching input unconstructed.
Two of the seven were reproduced by execution (C16 by probe, C17 by mutation) and
two more by mutation of the harness (C21, C22); the remaining three are code
traces whose every link was read rather than inferred.

Two carry a narrower caveat than PLAUSIBLE and say so in place. **C18** is
traced, not run, and one framing of the same state was refuted on severity —
the disposition below R11 in §6a is part of the finding. **C22** explicitly does
not assert an overflow in any uncovered arm and says which reading of it died.

Several states that finders suspected and could not reach are in §6b, marked
UNREACHED. They are not parked here because nothing about them survived
refutation — they never entered it.

---

## 5. Design observations

Shape, not defects. These age better than the bug list.

**Auditing the fixes was worth a round on its own.** Four of the seven findings
are attributable to the round-1 fixes: three are defects in the 194 lines of new
production code they added (C17, C18, C22) and one is the defect a fix was
written to close and did not reach (C16). The other three are Stage N proper. The
exit rule that says "a real finding resets the count, because the fix is new code
and new code has not been audited by anybody" is not procedural caution; on this
evidence the fix commits had roughly the defect density of the stage they fixed,
in a twentieth of the lines.

**Both enumerations came back short, and both were believed.** Round 1's §5
diagnosed N-T4's `isAlphaSubmenu` consumer list as *"re-derived, and it came back
one entry short"*. The C2-family fix enumerated the sites that manage the
console's row and came back one short — `forthCaptureResume`, which round 1's own
C3 entry had named in the same words. The C1 class test enumerated the arms of a
switch and came back six short. Three enumerations, three shortfalls, all written
down and all trusted. What is missing in each case is the mechanical half — a
count check against the thing being enumerated, which round 1's C1 entry asked
for by name and which nobody landed. **An enumeration a human writes and no build
checks is a comment.** Whatever else is fixed, the count check is the cheapest
control in the report and it would have caught two of these three.

**Ownership of the console's row is now represented twice and reconciled
nowhere.** `homePushed` answers "did the open displace the owner's frame";
`forthConsoleShowSurface` answers "is this row mine" from the menu id. C16, C17
and C18 are the three ways they can disagree — the bit is lost, the id is
believed on a frame that isn't ours, and the row cannot follow the bit. The shape
the code wants is **one** fact: which slot the console owns, written at the open,
maintained by the retarget, read by rung 3 — or nothing stored at all, with rung
3 asking the same question the same way `forthConsoleShowSurface` does. Either is
representable. Carrying both and hoping they agree is what produced this section
of the report twice running.

**The single-owner refactor moved the failure from destroying a frame to
declining to act, which is the better trade and a new post-condition nobody
propagated.** Pre-fix, the toggle and rung 1 destroyed frames (C2, C9); post-fix
they conserve frames and sometimes do nothing visible (C18). A leaked press costs
less than a lost menu, so the direction is right. But "may legally do nothing" is
a post-condition the callee acquired in `db5e5ca30` and none of its three callers
checks. A callee that can decline needs a return value, or callers that do not
commit state before calling it.

**The battery's oracles are placed where the code is easy to call, not where the
mechanism is.** C21 and C22 are the same defect in two batteries written days
apart: a guard around the object the test can construct — a caller's buffer, a
direct render call — rather than around the object the code actually writes: the
function's own frame, the paint arm. Round 1 said the direct-drive export *"buys
geometry coverage at the price of arm coverage"* and asked for one
`refreshScreen`-driven case with an empty ring; that observation is now two
confirmed findings. This is worth a standing line in `TESTING.md` rather than a
per-battery fix: **before writing the assertion, name the object the code under
test writes, and put the oracle on that object.**

**The footprint budget scored a good refactor as a regression.** The C2 fix moved
30 lines of logic out of `items.c` — an upstream override — into a new
package-owned file, which is exactly what `DESIGN_AUDIT.md` 2.1 asks for. Section
A's added-line count went **up**, 2074 → 2088, because `keyboard.c` and
`manage.c` absorbed the difference. Round 1 said re-baseline section A or call it
informational; this round adds the argument, which is that the metric cannot
distinguish "logic moved to the right place" from "logic grew". A budget exceeded
3.4× that also mis-signs the one change that improved the thing it measures has
stopped being a control.

---

## 6. Deliberately not flagged

Mandatory section, and the one that says whether the audit understood what it
read. Three sources merged: findings the **refutation pass killed**, items the
finders **cleared** before reporting, and states that were **suspected and could
not be reached**.

### 6a. Killed by refutation

**R7 — "the C1 class test skips the two matrix arms, and both write `tmpString`,
the rule the C1 fix argued it was preserving."** Refuted on intent. The class
round 1 named is *"every `*ToDisplayString` in `display.c` that seeds its index
from `ERROR_MESSAGE_LENGTH / 2`"*, and every such seed (`display.c:2056`, `:2222`,
`:2245`, `:2275`, `:2298`, `:2339`, `:2362`) lies inside
`shortIntegerToDisplayString`, which runs from `:1994` to the next function at
`:2401` — so the documented enumerable class has exactly **one** member, which
the fixture drives directly at its widest rendering behind a type assertion. The
matrix arms were investigated and ruled fine in round 1's own §6c: *"The bound
itself is fine (`MATRIX_MAX_COLUMNS` 11 and a 380 px budget keep it inside
`buf[256]`)."* The banner's `tmpString` rule is pinned in code
(`forth_bridge.c:287-289`) to **holding** a pointer across producers, which the
matrix arm does not do. And the count check the finding said the commit violated
is round 1's *suggested* class test, which `CODE_AUDIT.md:334-335` and `:276-277`
leave to the owner to adopt. What survived from this family is C22, which is
about where the canary sits, not how long the table is.

**R8 — "the N1-6 surface repair reverts every `calcMode` change a line makes, not
only the `calcModeNormal()` ones its comment names."** Refuted on intent, twice
over. `DESIGN.md` §8.4.2 defines the interactive capture's home as the pair
(`CM_AIM`, `FLAG_ALPHA`) for the capture's lifetime and enumerates every lawful
way to leave it (the EXIT ladder's three rungs, the seven close-path
dispositions); an item-driven mode change is in neither list and does not close
the capture, so it produces a state no ruling admits. `DESIGN-HISTORY.md`'s
Stage N entry names the repaired defect **by symptom**, not by its one instance:
*"A native item could tear the capture's input surface away … drops CM_AIM,
clears FLAG_ALPHA and hides the cursor while the capture object survives."*
`registerBrowser.c:166-176` satisfies every clause, so REGS is inside the
repaired class, not outside it. The repair's own scope clause
(`manage.c:1425-1428`) is keyed on capture-openness and declares itself
unconditional, and round 1 already cleared a separate finding by relying on that
reading verbatim. Reachability was conceded — REGS resolves from a Forth line and
`OPTION_REGBROWSER` is defined for the DM42n — and does not save it.

**R9 — "hammer invariant (4) is a tautology by type range, and its comment argues
wrongly that it is not."** The premise is right and the conclusion is wrong.
`FORTH_CONSOLE_LINE_MAX` is 255 against a `uint8_t` length byte, so the check at
`test_console.part.h:357` cannot fire — confirmed empirically: across two
deliberate cap-breaking mutations the string never printed once. But the stated
consequence, that the per-line cap is only covered by tests that never run under
wrap-around pressure, is false about this suite. **Mutation B** — a cap enforced
only when the open record does not straddle the ring end, i.e. a cap that breaks
*only* under wrap-around, exactly the class the finding said was uncovered — left
`test_console_ring_linecap` and `test_console_ring_glyph` **green** and was killed
by `test_console_ring_hammer` alone, at iteration 26, through invariant (2). The
hammer is the sole detector of that class, by the mechanism its own comment states
(*"a cap failure would instead wrap the length byte, and that desyncs the walk
against `used`, which invariant (2) below catches"*). What survives is that line
357 is a redundant restatement whose deletion costs zero mutation coverage, and a
PASS banner that counts it — a dead-line tidy, not a testing gap. `TESTING.md` §1
binds the test, which dies under both named mutations, not each `if` inside it.

**R10 — "`test_console_words_program`'s header claims a program-step write
context; the body runs a colon word from an interpreted line."** Refuted on
intent. `DESIGN.md` §8.4.4 rules the write context-free: *"They write the ring
wherever they run (interactive, key press, program step) and none of them paints;
a ring append is a bounded BSS write, legal from every context."* N-T5 gives the
reason in one line — *"Program-step runs under `runProgram` — same call, same
absence of paint"* — and `STAGE_N_CONSOLE.md`'s risk register item 6 names the
evidence standard the owner bought: *"bounded-write proof plus a hammer test"*,
explicitly not an end-to-end program run. The source bears it out:
`forthProgramStep` ends in the same `forthOuterRun` the other two entries call,
differing only in scope entry and `SKIP_DEFS`, and `forth_console.c` contains no
`programRunStop`/`PGM_RUNNING`/context conditional at all — so the proposed
mutation would first have to invent the branch it then nulls. The depth-accounting
consequence it named is the shared `forthDataDepthApply` path, already
mutation-proven red by test 21. Residue: the header's *"from a PROGRAM STEP
too"* is the §3.3.2 claim being illustrated, and the inline comment at `:1253-1254`
already says what the body does.

**R11 — "the E10/E11 toggle no longer changes the row when an alpha submenu is on
top, so keys input runs under an alpha keypad" (as a wrong result).** Path
granted; the wrong-result framing refuted, and the refutation is right about two
things that shape C18's severity. First, in keys mode the six displayed softkeys
still insert exactly the characters they show, `FLAG_ALPHA` and the status-bar A
are unchanged by the toggle in every state including correct ones, and the only
residue is that FWRD is not visible while a menu the **user** opened sits on top —
the uniform rule the finding itself quotes, identical to STK or a catalog stacked
over the console. Second, the claimed equivalence with C4 fails materially: in
C4's state the offending frame was the console's **own** slot-0 frame and rung 2's
predicate fell through to rung 3, which **closed** the capture; here slot 0 is
genuinely foreign, so rung 2 pops it and `forthConsoleShowSurface` retargets — one
press, no leak, no close. The old drain did not restore FWRD here either: from
`[alpha_omega, ALPHA(console), owner]` it popped both and landed on the owner's
menu with the console frameless, which is the C2 defect. So *"the code this fix
deleted handled exactly this case"* is false. What survives of the family is C18,
at the severity this refutation supports: the wrong plane is on the **physical**
keys, not the softkeys, and the real cost is that the owner has no indicator.

### 6b. Cleared by the finders

**The ring module's own arithmetic** (read in full by three readers, cleared with
the numbers worked rather than by inspection). `_reserve`'s documented "cannot
fail" holds because `FORTH_CONSOLE_LINE_MAX` 255 caps the open record at 256
bytes of a 1024-byte ring. The whole-glyph cap refuses at `len + g == 256`, so
the maximum payload is exactly 255 and a two-byte glyph is dropped whole — the
pinned numbers check out (93 × 11 = 1023 ≤ 1024; 127 two-byte glyphs = 254, with
the 128th refused). `_evictOldest`'s `consoleUsed - n` cannot underflow while the
records tile `used`, and the unguarded case is the one the module's own comment
at `:140-150` says it degrades to a miscount for. `forthConsoleLineAt`'s guards
are correct at `outSize == 1`, and its ring-aware glyph test is the right
analogue of the pointer form. Round 1 cleared this module and this round agrees
after independent reading.

**The ellipsis overwrite** (`screen.c:5757-5760`). `xcopy(cut, STD_ELLIPSIS, 3)`
into `char line[FORTH_CONSOLE_LINE_MAX + 1]` looked like an off-by-two. It cannot
fire: the branch needs total width > `SCREEN_WIDTH - 1` = 399 and cuts at 385, so
at least one glyph remains after `cut` carrying ≥ 15 px. An overflow needs
`cut == line+254`, i.e. 254 bytes inside 385 px followed by a single **one-byte**
glyph wider than 14 px — and the widest one-byte `standardFont` glyph is 13 px,
measured by extracting all 710 widths from the generated font table and checking
that `DOUBLINGBASEX` is 8 so the widths are unscaled. `cut == line+255` is
impossible because `withEndingEmptyRows` is true, which skips the tail block that
would advance past the last glyph. Same idiom as the landed `screen.c:4982-4983`.

**The C1 fix's `tmpString` borrow.** The banner it appears to contradict is about
**holding** a pointer into `tmpString` across producers; nothing runs between the
producer and the copy-out. All four production call sites were checked
(`forth_prims.c:102`, `:127`, `:189`, `manage.c:1472`) and none holds `tmpString`
across the call; `forthConsoleAppend` does not touch it. The byte-boundary clamp
the fix added is the C11 class but unreachable for this arm: the widest
short-integer rendering is base 2 at 64 bits with `FLAG_LEAD0`, ~96–160 bytes
against 255, and short-integer renderings are ASCII, so the cut cannot split a
glyph.

**`yMultiLineEdOffset == 0` on the first console frame** takes the two-row branch
where four rows belong, because the global is 0 at cold boot (`c47.c:220`) and
after A.RESET (`config.c:1826`) and only `showStringEdC47` writes it — during the
REGISTER_X paint, i.e. *after* the console arm in the same frame. Invisible: both
zeroing paths coincide with an empty ring and `_forthConsoleRender` returns early
at `count == 0`. The one-frame lag on a long/short crossing is argued at
`screen.c:5720-5724` and the argument holds.

**The 1-pixel band/editor relationship.** Bottom transcript row at Y 107, 21 px
pitch, `STANDARD_FONT_HEIGHT` 22, against `editorTop` 128. The pitch is the
landed `fnPem` listing pitch, pinned by N-T1 and stated normatively in §8.4.4,
and `fnPem` has the same relationship at seven rows; the glyph ink sits inside
the row padding. Design, not defect.

**The render gate's `forthCapIsInteractive()` where §8.4.4 says "open"** — true
for `FCAP_SUSPENDED` too. Cleared for both suspension shapes: plain TAM holds
`tam.mode != 0` for the whole suspension (assigned at `ui/tam.c:1151`, before the
suspend arm at `:1180`), and an armed fold runs under the forged
`calcMode = CM_PEM`. Two readers independently tried to place a repaint in the
window between `leaveTamModeIfEnabled` clearing `tam.mode` and
`forthCaptureResume` and could not; both marked it UNREACHED rather than
reporting it. The roll arm's identical guard clears on the same ground.

**The retarget bypassing `showSoftmenu`'s machinery.** What it skips was
enumerated rather than assumed: `userMenuId` (nonzero only for `-MNU_DYNAMIC`,
which neither console row is), `softmenuStack[].calcMode` (read only in
`pushSoftmenu`'s CM_NORMAL/CM_NIM re-find arm; both frames are pushed in CM_AIM),
`setScreenUpdateFromMenu` (a switch over solver/MVAR menus, inert here), and the
dynamic rebuild — which is **not** skipped, because `showSoftmenuCurrentPart`
rebuilds `-MNU_FORTH`'s word list from `cachedDynamicMenu` on every paint
(`softmenus.c:3145-3149`). `doRefreshSoftMenu = true` alone forces the repaint:
`screen.c:5902` and `:5933` test it before the SCRUPD mask.

**`_softmenuIndexOf`'s unbounded-looking loop** (`forth_menu.c:297`).
`softmenu[]` is explicitly 0-terminated at index 186 and this is `showSoftmenu`'s
own idiom (`softmenus.c:3019`, `:4125`, `:4422`, `:4463`). Gemini's claim that the
loop compares the wrong field was refuted in `d2fcb401c` and independently
re-confirmed here: `softmenu[].menuItem` is the id.

**`forthConsoleRestoreSurface`'s scan of all 8 slots** regardless of a live
depth. There is no depth pointer in this firmware — `softmenuStack` is a fixed
8-entry array, `popSoftmenu` shifts and memsets the vacated slot, and
`screen.c:5973`/`:5996` and `keyboard.c:439` scan it the same way.

**The declared stack delta applied before the prim runs** (`forth_inner.c:152`).
`EMIT` and `.$` return on their error paths without the `fnDrop` their declared
−1 already charged, leaving `forthDataDepth` one low. Harmless: the interpret loop
is `while (lineOK && nextToken(buf))` and the prim dispatch sets `lineOK = false`
on any `lastErrorCode`, so the desync cannot outlive the line, and
`forthDataDepthLeaveOuter` resets. Two readers reached this independently and both
said they only stopped believing it was a bug after finding the loop condition.

**`.S` reading `(calcRegister_t)(REGISTER_X + i)` for `i` up to 7.**
`defines.h:1272-1279` makes `REGISTER_X..REGISTER_D` consecutive at 100..107 —
exactly the 8-level window. No out-of-bounds behind C7's wrong-count defect. The
comment calling them "the spare registers" is inaccurate (the spares are
E/F/G/H/O/U/V/W at 118–125) but the code is right.

**A console line running CLALL / RESET / CLREGS entering confirmation mode**, with
the N1-6 repair eating the prompt. Refuted before write-up: the Forth item
dispatch calls `reallyRunFunction(itemId, NOPARAM)`, and `NOPARAM` (9876) ≠
`NOT_CONFIRMED` (9878), so `setConfirmationMode` is never reached from a Forth
line. Recorded because the same fact means `CLALL` typed into a console line
clears everything with **no confirmation at all** — pre-existing F-series dispatch
behaviour, outside this range, and §6c.

**`popSoftmenu`'s CM_AIM compensation firing inside rung 2.** Real mechanism — a
pop revealing MyMenu/MyAlpha in CM_AIM ends in `changeToALPHA()`, which pushes,
and the following `forthConsoleShowSurface` would adopt that frame as ours. It
needs the console's frame **not** to be directly under the stacked menu, and two
readers could not construct that stack: the open always leaves FWRD (or the
owner's own FWRD) immediately beneath. UNREACHED.

**Rung 3 double-popping when slot 0 is `-MNU_ALPHA` with `keysMode` true.** Rung
2's predicate does let rung 3 run in that state, where `calcModeNormal`'s own
`-MNU_ALPHA`-guarded pop plus the `popHome` pop would take two frames for one.
Three readers went after it — six routes between them: the toggle, rung 1, the
ENTER reopen, the fold resume's `showSoftmenu(-MNU_ALPHA)`, a stacked alpha
submenu, and picking ALPHA out of the MENUS catalog — and every one either
retargets through `forthConsoleShowSurface` first or leaves a foreign row that
rung 2 pops first. UNREACHED; asserting it would be write-set reasoning.

**`forthCapAbandonSuspended` clearing `keysMode` and origin but not
`homePushed`.** Harmless as a flag — it sets `FCAP_CLOSED` and the next
`_forthCapOpenAs` zeroes it. The frame it strands is real, but its only reach is
`forthCaptureResume`'s defensive canary, which needs a falsified capture step;
round 1's R1 established that requires program memory already broken by upstream's
own accounting.

**`consoleView` surviving close and reopen** (it is BSS), so a console left rolled
back reopens rolled back rather than at the newest line. N-R3 snaps only on commit
or output; a reopen is neither. Deliberate-adjacent and cosmetic.

**`forthConsoleClear()` in `forthCapPowerReset` and deliberately not in
`forthCapClose`** — the "clears one store and not its neighbour" shape. N-R2 rules
that the dialogue survives close and reopen, the comment says so, and
`test_console_ring_reset_seam` pins it.

**Pressing FORTH again while the console is open** re-runs
`forthEnterAimSurfaceNoLift`, which with FWRD already current sets
`forthHomeWasFresh = false` and so clears `homePushed`; from the alpha excursion
it pushes a second console frame. Same root as round 1's **C6** (`fnForthOuter`
has no already-open guard), still open, so not re-reported — but C6's consequence
list should gain *"and corrupts the close accounting"*, because a reader fixing
only the line-discard half will leave this behind.

**EXIT rung 3 pushing a non-empty line to FHIST with no ring echo.** Round 1's R5
ruled it: the one-act invariant is site-scoped, and an EXIT-abandoned line never
ran, so echoing it as `» line` would claim it executed.

**The empty-string X echo.** `forthConsoleAppend` bumps `consoleSeq` only when
`*s != 0`, so a word that appended an empty string would collect an unasked X
echo of the value beneath. No reader could name a gesture that puts an empty
`dtString` in X from the console, and the mechanism is documented at
`manage.c:1466-1471`. Observation.

**Test fixtures that enter the alpha excursion with `forthCapSetKeysMode(false)`
instead of `runFunction(ITM_AIM)`** (`test_capture.part.h:8006`, `:8455`,
`:15872`) leave the row on FWRD while the bit says alpha, so subcase [5]'s new
`currentMenu() != -MNU_FORTH` assertion passes even if rung 1's
`forthConsoleShowSurface` did nothing. That weakens the C9 pin, but it is not a
firmware defect and `test_console_frame_conservation` cases 1/2/4 drive the real
toggle, so the fix is not unpinned.

**Oracles checked and cleared** (D6, with the upstream behaviour read rather than
assumed): test 3's eviction arithmetic; test 5's whole-glyph cap and 6-byte
out-buffer truncation; the hammer's invariants (1), (2) and (3), including its
own non-termination guard, which reports rather than hangs; test 8's third leg
(capture close must **not** clear the ring — the direction a "clear everywhere"
reflex breaks); the view gate's five falsifiers, each distinct and reachable;
`test_console_view_rows`' exact-ratio oracle (`shortState == 2 * longState` over
identical lines, which a row-count off-by-one reddens);
`test_console_view_placement`, the only case that asks *which half* of the band
the ink is in; `test_console_view_arm`'s yield leg, sound specifically because
`forthConsoleClear()` at `:683` makes any band ink provably native and
`_selectiveClearScreen` wipes the previous frame — both checked upstream;
`test_console_frame_conservation`'s slot-0 identity compare and differential leak
count (`popSoftmenu` memsets the vacated slot, so counting over all 8 slots is not
reading garbage, and case 6's fixture really does reach
`forthConsoleRestoreSurface`); the retargeted `test_number_bad_lone_dot`, where
moving the probe from bare `.` to `+.`/`-.` still reaches `classifyNumber`'s
`mantissaDigits == 0` arm and the oracle was **tightened** from "any error" to
exactly `ERROR_FUNCTION_NOT_FOUND`; `test_history_program` subcase [0]'s
both-modes strengthening, which looks true by construction but reddens when the
new arm is deleted; and `test_console_words_stack`'s spill assertion, a real
oracle for `.`'s declared delta.

**Upstream discipline, cleared.** The 103-line renderer body living in
`screen.c` is the obvious D8 finding and is **not** reported: round 1's R6 killed
it correctly, `STAGE_N_CONSOLE.md:321-327` assigns the whole body — *"suppression
gate, per-editor-state row counts, row paints, truncation, and the roll"* — to
`screen.c` before implementation, risk 7 prices the merge cost, and the override
rule carries no size bound. The technical premise a re-report would need was
re-checked and holds (the renderer touches no `screen.c` file-static;
`yMultiLineEdOffset` is extern in `c47.h:485`), so the move is *possible* — it is
a taste preference the design settled. Worth one line only: the mitigation
markers are opening banners with no closing marker, so a re-grep after an upstream
merge has to eyeball where the block ends. Also cleared: the `keyboard.c`
roll/recall arm (49 added lines, ~10 of logic, placed there by N-T4);
`FORTH_CONSOLE_ROW_PITCH` 21 (upstream writes the literal itself at
`manage.c:546-551` — there is no symbol to use, and round 1's C14 already names
the class); the roll arm against `Check_MultiPresses`, whose CM_AIM branch arms a
longpress only when `!shiftF && !shiftG` while both roll gestures are shifted;
`CHR_caseUP`/`CHR_caseDN` appearing exactly once per layout row in all eleven
`assign.c` tables, so the layout-independent key identification is sound; and the
`fnKeyExit` hunk growing to 121 contiguous added lines with every removed line
package-authored, so the merge liability grew in size but not in kind.

**Round-1 findings still open and deliberately not re-reported:** C5 (recall wipes
the typed line), C6 (no already-open guard), C7 (`.S`'s `displayStack`), C10
(EMIT's `0x??00` codes), C11 (the `dtString` byte cut), C12 (the roll clamp), C13
(the spill counter), C14 (the `editorTop` literals), C15 (the override list).
C10's range test and C12's clamp were re-read in the current tree and confirmed
unmodified. Two sites belong to open round-1 classes rather than to new findings:
the ENTER echo's `snprintf` at `manage.c:1394`, which cuts a two-byte glyph at 252
bytes and is C11's named second site, and the C1 fix's own copy-out clamp, same
class and unreachable for that arm.

### 6c. Pre-existing or out of range, for the record only

**`CLALL` from a console line takes no confirmation.** The Forth dispatch passes
`NOPARAM`, which is not `NOT_CONFIRMED`, so `setConfirmationMode` is bypassed —
F-series dispatch behaviour, present before this range, and not a Stage N defect.

**`forthCaptureSanitizeRestoredUi` recognises only the CM_PEM restore shape**
(`manage.c:814-840`). A backup taken during a console session restores CM_AIM +
`FLAG_ALPHA` + a FWRD row around a **closed** capture — native AIM, where EXIT
runs `closeAim()` and commits the buffer to X as a `dtString`. Pre-existing since
Stage L; the same hole existed with `-MNU_ALPHA`, and Stage N only changes which
row is left standing.

**`showRealMatrix` writes the global `tmpString`** despite its banner. Already in
round 1's §6c; re-encountered by two readers this round chasing the matrix arm's
bound, which is itself fine.

---

## 7. Verdict

**Would I ship this?** Yes, with C16 fixed. Nothing in this round corrupts memory
or loses the line being typed — C1, the one finding of that kind, was fixed in
this range and the fix is correct. What remains is menu-frame accounting, a
mis-rendered error line, a rare wrong glyph, and two controls that cannot fail.
C16 is the one an owner would actually report, and it is the one round 1 already
reported.

**Where would it break first,** from a session that does nothing unusual:

1. Owner has a menu up, presses FORTH, types a number, presses **STO**, names a
   register, presses EXIT — and then presses a softkey expecting their own
   assignment and runs a Forth word instead. **C16** plus Stage M's
   execute-in-CM_NORMAL.
2. Owner toggles into alpha to type a definition, taps a Greek or MISC softkey,
   presses EXIT to get back. The row still shows the alpha keypad and the keys
   now type item names. **C18.**
3. Owner prints something and makes a typo in the same line. One row instead of
   two, and if the line printed much, no error text at all once the native paint
   is dismissed. **C19.**
4. Owner who reaches FWRD by browsing the catalog opens the console there,
   toggles, and runs a line that clears the stack. Their FWRD row is gone — not
   leaked, gone. **C17.**
5. Nobody reaches **C20** by accident.

**What is in good shape,** and worth saying because it is most of the range: the
ring module, again — three readers worked its arithmetic independently and all
three cleared it, and its load-bearing comments anticipated the hazards they went
looking for. The C1 fix, which is correct, contained, and argues honestly with the
banner rule it appears to break; the bug class it names has exactly one member and
that member is closed. The single-owner architecture in `forth_menu.c`, which is
the right shape — retarget rather than push/pop is correct against
`pushSoftmenu`'s dedup and `popSoftmenu`'s CM_AIM compensation, and it kills all
five reported instances of the C2 family. The error and refusal paths inside
`forthInteractiveEnter` (the E9 refusal, the `calcModeNormal` repair, the L5 line
restore, `lastErrorCode` ownership across the render gate and the EXIT ladder),
which were traced end to end and are consistent. And most of the N1-2 battery:
the exact-ratio row test, the placement test that asks which half of the band the
ink is in, and the fixture-proves-its-own-state assertion are real oracles that
redden under the mutations they were written for.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.**

- **C22, conditionally.** The subject arm of C1 is genuinely closed and the C1
  mutation still reddens the gate — via the compiler's stack protector rather
  than the canary. That is a worse error message and a control that will be
  trusted for more than it checks. I would leave it **if** the DMCP build carried
  a stack protector; it does not (`src/c47-dmcp/meson.build`), so on the one
  build where C1's consequence is a reboot, the class is guarded by nothing. That
  moves it from bookkeeping to worth an hour. The cheap version is not the full
  table: add the count check, and call the producers directly.
- **C18's shape (b),** the AIM-catalog case. One press that appears to do nothing,
  and the owner presses EXIT again — which is what they would have done anyway.
  Shape (a), where the keys type the wrong characters under a Greek keypad, is
  the half worth fixing; a fix for (a) very likely covers (b) for free, but (b)
  alone would not be worth a change.
- **R9's residue.** Deleting the dead line at `test_console.part.h:357` is a
  tidy, not a fix, and the hammer is already the sole detector of the class the
  finding thought was uncovered. Do it when the ring tests are next touched, or
  never.

**Not code defects at all:** C21 and C22 are controls, not behaviour — they cost
the owner nothing today and everything on the day someone refactors the paint arm
or adds a `case` to the formatter.

**If only three things are fixed:** **C16**, **C18**, **C17** — in that order.
C16 because it is the ordinary gesture and it is the defect round 1 already
named; C18 because a lying mode indicator produces a wrong line and the owner has
nothing else to look at; C17 because it is the only one that takes something the
owner cannot get back. All three are the same root — see §5's third observation —
and a ruling on how the console records which frame it owns would let one change
close all three instead of three changes closing one each.

---

## 8. Round and exit state

**Round 2.**

**Readers.** Eight in-family dimension finders (D1 contracts, D2 lifecycle, D3
arithmetic, D4 error paths, D5 guard reachability, D6 tests, D7 design, D8
upstream), blind to each other by construction. Every finding piped to refuters
who did not produce it, with distinct lenses — reachability, correctness, intent
— and a standing instruction to default to refuted.

**Yield.** 7 CONFIRMED (from 11 surviving finding reports, four of them pairs
describing the same defect), 0 PLAUSIBLE, 5 refuted. Independent agreement:
**C16** by five readers, **C17** by four, **C18** by three, **C22** by two. That
agreement is the evidence the blind fan-out exists to produce, and on C16 it is
the reason a defect the fix commit's own class test asserts was closed is in this
report at position one.

**Mutations and probes run** (all reverted; every one applied in a detached
worktree or as a reverted probe):

| what was mutated | result |
|---|---|
| `screen.c:6039` `else` removed — registers paint **with** the transcript | whole gate GREEN, three runs, `viaArm` 1045 → 1875 px and back → **C21** |
| `memset(buf + FORTH_CONSOLE_FMT_MAX, …)` in the `dtComplex34` arm | gate GREEN, test printed *"every formatter arm stays inside the caller's buffer"* → **C22** |
| frame-conservation battery + owner-owned FWRD row + alpha + a CLSTK line | **RED**: *"top is now −1318, the owner had −213"* → **C17** reproduced |
| `homePushed` probe driving FORTH → STO → `0` `5` → EXIT | `homePushed` 1 → 0 across the fold resume; capture closed with −213 on top → **C16** reproduced |
| `_appendGlyph` cap deleted | linecap, glyph and hammer all red → R9 half |
| `_appendGlyph` cap enforced only for non-straddling records (wrap-only break) | linecap and glyph **GREEN**, hammer alone red at iteration 26 → **R9 refuted** |
| `2^40 + 65` through `longIntegerToInt32` against system GMP | `code = 65`, gate accepts, glyph `A` → **C20** |

**A note on the tree.** Three sessions were working this checkout concurrently
during the round, which cost one contaminated gate run and one confounded
mutation attempt (recorded in §2). The audit made no code change of its own; all
mutations were applied, observed and reverted, and the final mechanical numbers
come from a detached worktree at `d2fcb401c`. A fix for C16 and its class test
were written and committed by another session while this report was being
assembled (`723361f58`, outside the audited range). **That work is unaudited by
definition and belongs in round 3.**

**Exit criterion: NOT met**, on both counts, for the second round running.

1. **No out-of-family reader ran on this subject.** `d2fcb401c` is round 1's
   out-of-family pass landing — Gemini reading the *stage*, which is what
   produced the leak fix that commit celebrates. The fixes themselves have been
   read only in-family, and three of the seven findings are inside them. Step 4
   is binding: *"Fresh sessions of one model are not a rotation."*
2. **Seven CONFIRMED resets the count.** The fixes for C16–C22 will be new code
   that nobody has audited, which is precisely the argument this round exists to
   have proved.

**Recommended next steps, in order.**

1. Owner triage of §3. Expect to reject some; §7 names the ones I would reject
   myself and the condition under which C22 changes rank.
2. **Out-of-family pass on the fix commits specifically** — Gemini via `agy`,
   which `CODE_AUDIT.md` records as working. Small packet, whole functions, and
   a line of orientation for every shared structure: `softmenuStack` slot 0 is
   the **top**, `currentMenu()` is `menu(0)`, `softmenu[].menuItem` is the id.
   Round 2 of the process notes already record both of those as the failure modes
   that cost a run.
3. Fixes land per the standing rule: reproducer, named bug class, class-level
   test where the class is enumerable. Every finding above carries all three.
   **Land the count checks with them** — §5's second observation is the one that
   would have prevented two of these seven.
4. Round 3 audits the fixes, including the C16 fix already in the tree.
