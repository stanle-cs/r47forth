# Audit round 5 — the C17/C18/C19 fixes, round 3's four regressions, round 4's hardening

Subject: `b5a0202c9..ff9274f9f` — the second and third sessions of 2026-08-06,
i.e. everything the last three audit rounds produced. Nine commits.

Round 4 said round 5 starts with the `design` (D7) dimension, which had not
run for two rounds. It ran, and all eight dimensions ran with it. Twelve
CONFIRMED findings, two PLAUSIBLE, five past the verification cap.

---

## 1. Subject and coverage

**Range.** `b5a0202c9..ff9274f9f`, 9 commits, 28 files, +3770/−548. The code
half: `forth_menu.c` (+387/−…), `forth_menu.h`, `forth_capture.c/.h`,
`forth_compile.c`, `items.c`, `keyboard.c`, `programming/manage.c`, and three
test files (`test_console.part.h` +839, `test_capture.part.h` +82,
`test_dict_reloc.c` +23). The doc half: `DESIGN.md` (+65), `DESIGN-HISTORY.md`,
`HANDOFF_2026-08-06.md`, `CODE_AUDIT.md`, and round 3's own report.

**Readers.** Eight in-family dimension finders, blind to each other — D1
contracts, D2 lifecycle, D3 arithmetic, D4 error paths, D5 guards, D6 tests,
D7 design, D8 upstream. First round in which all eight ran; first D7 pass
since round 2. Each finding then went to a reader who did not produce it,
with one of three refutation lenses (reachability / correctness / intent).

**An out-of-family pass also ran** — Gemini 3.1 Pro, three self-contained
packets over the ownership machinery, launched outside the workflow and
therefore invisible to the synthesiser. §9 is that record, written after the
rest of this report; where §8 says no out-of-family reader ran, §9 corrects
it. Its two results both converge with in-family findings, which is the
reason it is folded in here rather than filed separately.

**Deliberately not audited.** `forth_console.c`'s ring internals (C10/C11/C12
territory, and out of the named file set); `test_engine.part.h`,
`test_params.part.h`, `test_persist.part.h` (untouched by the range — grepped
only, for the stamp accessors and for `EXITALL`); the upstream files this
range did not touch; `design-audit.sh`'s judgement half (`DESIGN_AUDIT.md`
Parts 2–3).

**What the reading budget did not reach.**

- The bulk of `test_capture.part.h` (~614 KB). Only the `[3a]`/`[3(b)]`
  repairs, the fold batteries' entry points and `test_fold_close_paths` were
  read as code; the rest was grepped.
- `test_dict_reloc.c`'s 23 changed lines (grepped for the census accessors,
  not reviewed).
- The `screen.c` and `ui/tam.c` overrides were read in the regions the
  findings touch, not line by line.
- `forth_compile.c`'s per-token error gates (850–1450) were skimmed on the
  ground that rounds 1–3 worked that surface.
- `forth_menu.c`'s picker content builder (75–264) was read for arithmetic
  and for `forthPickerGuard`, not for behaviour.
- **Nothing was run on device or in the simulator.** Three findings (R3, R7,
  R8) end in the same on-screen claim — the row says `A` while the keypad
  types keys — and a `run-sim` capture would settle all three with one
  gesture. Round 3's C21 was found by a screenshot; this round produced no
  screenshot evidence, and that is the cheapest confirmation it left on the
  table.
- Mutation coverage was partial: four findings were tested by mutation (R9,
  R10, P-A, and one refuted item). Everything else here is a static trace.

---

## 2. Mechanical results

Measured at `ff9274f9f`, tree clean, one worktree.

**Gate: GREEN.** `./packages/forth-core/build-test.sh` exit 0. The refresh
step regenerated all 17 patches, meson removed and re-patched the whole
`custom_pkg_shadow`, so this was a full rebuild and the warning set below is
a fresh signal. `FORTH SELF-TEST: ALL PASSED`; upstream `testSuite` Ok 1 /
Fail 0.

**Warnings: 34 lines, 15 distinct, all baseline.** Unused variables and
labels in the test parts, `_XOPEN_SOURCE` redefinition, four
`-Wmaybe-uninitialized` in upstream paint code, one `-Wformat-overflow=` at
`test_capture.part.h:15597`, one `-Warray-bounds=`. Three touch files this
range edited (`test_capture.part.h:5604` unused `savedMenu`, `:15597`, and
`test_dict_reloc.c:823` unused `tpSrcPayload`); all three sit outside this
range's hunks, so nothing new. Nothing in `test_console.part.h`'s 839 added
lines.

**`design-audit.sh`: 3 finding groups, all pre-existing** — A (footprint over
budget), D (inline blocks over baseline), E (allocations needing lifetime
answers). Nothing new in kind. Measured against a `b5a0202c9` worktree to
separate this range's contribution:

| metric | `b5a0202c9` | HEAD | delta |
|---|---|---|---|
| upstream override files | 17 (budget 16) | 17 | 0 |
| upstream added lines | 2088 (budget 606) | 2156 | +68 |
| contiguous added blocks ≥12 lines | 29 (baseline 16) | 30 | +1 |
| largest `keyboard.c` block (`fnKeyExit`) | 121 lines | **136 lines** | +15 |

That last row is the D8 observation's subject: the interactive EXIT ladder is
now 136 lines of package ownership logic inside an upstream function, ten
lines above the native AIM ladder it was copied from. See §4 U5.

**Process note.** Three readers reported the shared working tree being
mutated by concurrent sibling readers during their runs — one saw a baseline
gate come back RED at a nominally clean HEAD, with two foreign
`/* MUTATION */` edits live in `forth_compile.c` and `forth_menu.c`, one of
which was reverted mid-session. Only one reader used a worktree. See §8.

---

## 3. CONFIRMED findings

Twelve, ranked by what they cost the owner. Sixteen confirmed items collapse
into these twelve because three pairs are the same expression or the same
line seen from different dimensions; two confirmed items had no reaching
input and are in §4 instead.

**The round's centre of gravity.** Three findings (R3, R7, R8) reach the same
state by three unrelated routes: *an interactive capture is open and no frame
is registered*. In that state `forthConsoleBaseOnTop()` answers "base on top"
from its identity fallback, both C18 gates commit a `keysMode` flip, and
`forthConsoleShowSurface()` — the function whose job is to make the row
follow — falls out of all three of its arms and changes nothing. The result
is C18's exact symptom, produced by the fix for C18. Three finders found
three doors into it independently; that agreement is the finding, more than
any one door is.

---

### R1 — `fnKeyExit`'s SYSFL recovery arm leaves an armed interactive fold unbracketed

**Where.** `packages/forth-core/keyboard.c:3936` — the `return;` in
`fnKeyExit`'s catalog `default:` arm, inside the
`tam.mode && currentMenu() == -MNU_SYSFL` branch. Pristine upstream `//JM`
code, identical at `src/c47/keyboard.c:3670`; it predates the fold and
therefore embodies no decision about it.

**Reaching input.** FORTH (console opens keys-first, its OWNED FWRD frame at
slot 0). Type `1` `2` `+`. Press f+6 → `-MNU_FLAGS` stacks over the console's
row (keys mode takes the NORMAL plane, `keyboard.c:1843`; a negative menu id
skips `items.c`'s interactive divert). Press softkey 1 = **SF**: `ITM_SF` is
`CAT_FNCT|PTP_FLAG` with param `TM_FLAGW` (`items.c:2016`), so the
name-insert arm (PTP_NONE only) does not fire and it falls to `items.c:790`'s
TAM gate — `TM_FLAGW` = 10005, inside [`TM_VALUE` 10001, `TM_CMP` 10022] →
`tamEnterMode(ITM_SF)` → `ui/tam.c:1181` `forthFoldEnter` (`_forthFoldAdmits`
default arm: FOLD, `foldMode` = 1 ARMED) then `forthCaptureSuspend()`. Press
softkey 2 = **SYS.FL** (`menu_TamFlag[1]` = `-MNU_SYSFL`, `softmenus.c:803`;
`catalog` = `CATALOG_SYFL`). Press **EXIT**.

**What breaks.** The arm sets `numberOfTamMenusToPop = 2`, calls
`leaveTamModeIfEnabled()` and returns — 50 lines above the `if(tam.mode)` arm
whose `forthFoldUnwindIfDone()` at `:3988` is the only one in the file.
`leaveTamModeIfEnabled` declines to resume an ARMED fold
(`ui/tam.c:1425 && !forthFoldArmed()`). The owner is left with: the line gone
from the editor (it exists only as the fold's materialised `ITM_FORTH` step in
FHIST), the capture stuck `FCAP_SUSPENDED`, and `forthFoldArmed()`
permanently true. Because `tamProcessInput` brackets every TAM input on
`forthFoldArmed()` (`ui/tam.c:1465-1467`, `if(brk) { calcMode = CM_PEM; }`),
the owner's **next `STO 05` in normal mode records a program step instead of
storing to R05**, and that TAM's epilogue then runs `forthFoldUnwindIfDone` →
`forthCaptureResume`, reopening the stale capture (FLAG_ALPHA,
`calcModeAimGui`) in the middle of unrelated work.

**Contract violated.** `forth_capture.h:201-203`: *"forthFoldEnter/
forthFoldLeave are a bracket: every forthFoldEnter that returns with
forthFoldPending() true must be matched by exactly one forthFoldLeave."* And
the sibling arm's own comment, `keyboard.c:3983-3987`: *"EXIT during TAM never
routes through tamProcessInput … without this call an armed fold is never
unwound and its materialised capture step stays in FHIST permanently. 'type
something, press STO, EXIT before finishing' is one of the most ordinary
cancel gestures there is."* `DESIGN.md` §8.4.3 states the unwind *"runs from
both the `tamProcessInput` epilogue and `fnKeyExit` (cancel mid-TAM must
unwind too)"*. The only documented deferral in this neighbourhood (§8.4.3
"Known v1 limitations (a)" / `STAGE_L_TRACES.md` §T7.8) is scoped by line
number to the catalog-driven TAM **commit** sites (`keyboard.c:1148`, `:1160`)
whose accepted consequence is the opposite of this one ("the capture is never
lost (the step is the store)").

**Bug class.** *A second exit from a function whose single exit was the fix.*
L1-F2's C2 moved the unwind to a place "where it cannot be bypassed"; a
bypassing `return` in the same function survived the move.

**Class test.** Enumerable and cheap: grep every `return` in `fnKeyExit`
reachable with `tam.mode != 0`, assert the count matches a pinned number, and
for each one drive it with an ARMED interactive fold and assert
`!forthFoldArmed()` and `!forthCapIsSuspended()` afterwards.
`test_fold_close_paths` has seven subcases and its `FCP_RESET` macro forces
`catalog = CATALOG_NONE`, so the catalog-up EXIT is structurally excluded
today — that is the subcase to add.

---

### R2 — the interactive EXIT ladder's entry gate tests origin, not openness, so its close rung closes a SUSPENDED capture

**Where.** `packages/forth-core/keyboard.c:4072` (`case CM_AIM:
if(forthCapIsInteractive())`), and `fnKeyEnter`'s twin at `:3707`.

**Reaching input.** Continue R1's sequence. After the SYSFL EXIT: `tam.mode`
0, `catalog` 0, `calcMode` CM_AIM, capture SUSPENDED, `keysMode` true,
`-MNU_FLAGS` at slot 0 with the console's OWNED FWRD at slot 1 (the hardcoded
`numberOfTamMenusToPop = 2` popped only the two TAM frames). Press EXIT: the
`tam.mode` and `catalog` arms are both skipped, and `forthCapIsInteractive()`
is TRUE for SUSPENDED (`forth_capture.c:126`, `origin == INTERACTIVE &&
state != CLOSED`), so the ladder is entered; the overlay rung pops `-MNU_FLAGS`
and `forthConsoleShowSurface()` returns immediately on `!forthCapIsOpen()`.
Press EXIT again → the close rung. (Without a stacked menu the two TAM pops
leave the stamped frame on top and the close rung fires on the **first**
press.)

**What breaks.** The close rung pushes whatever TAM left in `aimBuffer` into
FHIST as if it were the owner's line, then `forthCapClose()` flips
SUSPENDED→CLOSED unconditionally (`forth_capture.c:28`, documented at `:84-86`
as *"covering OPEN and SUSPENDED alike"*). `forthFoldLeave()` never runs, so
the parked `ITM_FORTH` step holding the owner's real line stays in FHIST
forever and `foldMode` stays ARMED — the same durable CM_PEM-forging
corruption as R1. Two EXIT presses to get out of a menu; the line is silently
lost and a permanently mis-bracketed TAM is gained.

**Contract violated.** `forth_menu.c:543` and `:627` establish the pair
`forthCapIsInteractive() && forthCapIsOpen()` as the precondition for touching
the console surface; the ladder commits history, close and a frame pop under
half of it. The ladder's own rung-3 text — *"A non-empty line is pushed to
history BEFORE the close, so EXIT never loses it"* — assumes an open capture
throughout. With the capture suspended, `aimBuffer` is not the line, and the
line is exactly what is lost.

**Bug class.** *A two-valued predicate used for a three-state machine:*
`forthCapIsInteractive()` answers "whose capture is this", and four sites read
it as "is there a live line". Enumerable by grep.

**Class test.** A census: every `forthCapIsInteractive()` call site,
classified as an origin question or an openness question, with the count
pinned. Then a behavioural pin: with the capture SUSPENDED under a live
armed fold, assert EXIT and ENTER neither close nor commit.

---

### R3 — the f long-press de-registers the console's row, and an unregistered row is one `forthConsoleShowSurface` refuses to move

**Where.** `packages/forth-core/forth_menu.c:579` (the contract) against
`packages/forth-core/screen.c:915-916` (`Shft_handler`, unmodified upstream —
verified by diffing the package copy against `src/c47/screen.c`).

**Reaching input.** From CM_NORMAL with any non-FWRD menu up (the default
MyMenu will do), press **FORTH**, then **hold f** until the long press fires.
`FLAG_SH_LONGPRESS` is set by the reset profile (`src/c47/config.c:228`) and
`OPTION_LONGPRESS_CFG` is defined for the R47 target
(`src/c47/defines.h:276`), so this is a default-enabled gesture. Two presses
after opening the console.

**Mechanism.** `forthEnterAimSurfaceNoLift` leaves FLAG_ALPHA set, `calcMode`
CM_AIM, `keysMode` 1, OWNED `-MNU_FORTH` at slot 0. `commonShiftProcessing` →
`refreshFn(TO_FG_LONG)` → `Shft_handler`; `calcMode == CM_AIM` skips the
USER-mode branch; `shiftF` is true; at `screen.c:915`
`getSystemFlag(FLAG_ALPHA)` is true (the console set it) **and**
`isAlphabeticSoftmenu()` is true, because the package widened
`isAlphaSubmenu` to count `-MNU_FORTH`
(`packages/forth-core/softmenus.c:3880-3890`) and slot 0 *is* `-MNU_FORTH`. So
`popSoftmenu()` destroys the console's OWNED frame and, with the vacated slot,
its stamp; `:923` then pushes an **unregistered** `-MNU_ALPHA`
(`pushSoftmenu` writes `userMenuId = 0`). The capture is still OPEN,
INTERACTIVE, `keysMode == 1`, with no stamp anywhere. Long-pressing g instead
is benign (it pushes MyAlpha as an overlay and pops nothing).

**What breaks.** Immediately: the ALPHA keypad row with the keyboard still in
KEYS mode — the `A` key types Σ+ and the row says `A`, C18's symptom verbatim.
Nothing recovers it. `items.c:768`'s toggle asks
`forthConsoleBaseOnTop()`, which with no stamp falls back to the identity test
and answers TRUE for `-MNU_ALPHA`, commits the flip, and calls
`forthConsoleShowSurface()`, which with slot 0 neither owned nor stamped and
no owned frame buried falls off the end of `forth_menu.c:596-608` having
changed nothing. The EXIT excursion rung (`keyboard.c:4138-4144`) does the
same. So `(keysMode, row)` can be driven to `(keys, ALPHA)` again and again.
The close rung reads `forthConsoleOwnsSlot0() == false` (`keyboard.c:4183`)
and declines its pop; and after the next ENTER, `forthConsoleRestoreSurface`
pushes a fresh OWNED FWRD frame **above** the orphaned ALPHA frame, so the
EXIT that closes the console reveals the alpha keypad instead of the owner's
menu and costs one extra press. **This is a C17 regression by omission:**
pre-C17 `forthConsoleShowSurface` retargeted slot 0 on menu identity alone, so
the very next call put the row back in step with the mode.

**Contract violated.** `forth_menu.h:24-28`: *"establish the console's row for
the current input sub-mode. The ONLY function that may change it while an
interactive capture is open."* `screen.c:916` changes it. And `DESIGN.md`
§8.4.4: *"**The sub-mode and the base row never disagree (AUDIT C18).** K-R3's
rule — the underlying row IS the mode indicator — is enforced at every
keysMode writer"* — enforcing at `keysMode` writers is not sufficient once a
pure ROW writer exists that leaves `keysMode` alone.

The design record touches this site exactly once, and mis-triages it:
`STAGE_N_TRACES.md:418-421` enumerates the `isAlphaSubmenu` consumer set and
calls `screen.c:913` *"(a status-bar/shift-display test)"*, then says *"The one
that matters is keyboard.c:4162, the CM_PEM EXIT arm."* It is not a display
test — the arm calls `popSoftmenu()` and then `showSoftmenu(-MNU_ALPHA)`. Seen
and mis-triaged is the opposite of ruled deliberate.

**Bug class.** The one round 1 already named for C2: *a predicate widened for
one consumer, breaking a different consumer that was never enumerated.*
`isAlphaSubmenu` was taught `-MNU_FORTH` in Stage L; the consumer set was
re-derived in Stage N and came back one short.

**Class test.** The census round 1 asked for and nobody wrote: grep every
caller of `isAlphaSubmenu` / `isAlphabeticSoftmenu`, assert the count matches
a pinned number, and for each caller that can pop, assert the console's
registered frame survives when it runs with an interactive capture open.
Grep finds no test referencing either name as a census today (three incidental
externs and two comments, `test_persist.part.h:538`,
`test_capture.part.h:3329`/`:8061`/`:8182`/`:8287`).

---

### R4 — the FHIST recall gesture fires while the capture is SUSPENDED and overwrites TAM's own name buffer

**Where.** `packages/forth-core/keyboard.c:2839` (f+UP) and `:2860` (f+DOWN).

**Reaching input.** Stock keypad, no configuration. FORTH (keys-first) →
**g+XEQ** = `ITM_GTO` (param `TM_LABEL`, `PTP_LABEL`, so not the name-insert
arm) → `items.c:789` `tamEnterMode(ITM_GTO)` → `ui/tam.c:1180-1182`
`forthFoldEnter` (admitted) + `forthCaptureSuspend()` → **XEQ** again
(`primaryTam` = `ITM_alpha`; `TM_LABEL` passes `allowAlphaMode`, so
`tam.alpha = true`, `aimBuffer[0] = 0`, `calcModeAim`) → type **A**, **B** →
**f+UP**.

**Mechanism.** `determineItem` takes the AIM plane via the `|| tam.alpha`
disjunct (`keyboard.c:1798`) — a top-level disjunct, so it bypasses both the
`!(forthCapIsInteractive() && forthCapKeysMode())` and the
`!(tam.mode && forthFoldPending())` conjuncts — and returns
`key->fShiftedAim` = `CHR_caseUP`. The roll/re-home arm at `:1882` is skipped
by its own `!tam.mode`. `processKeyAction` reaches `case CHR_caseUP` with no
CM_/tam gate above it (the chain at `:2639-2645` screens only GRAPHMODE and
CM_ASN_BROWSER), and the guard at `:2839` is `forthCapIsInteractive()`, TRUE
for SUSPENDED — so `forthHistoryRecall(-1)` runs and xcopies a previous
console line over `aimBuffer`, which **is** the TAM's name buffer
(`ui/tam.c:940`, `char *buffer = (forcedVar ? forcedVar : aimBuffer)`).

**What breaks.** Mid-way through typing a GTO/STO name inside the console,
f+UP silently replaces the name with a whole previous Forth line (`12 34 +`).
Because `ui/tam.c:269` force-commits as soon as
`tam.alpha && stringGlyphLength(aimBuffer) > maxLen` (maxLen 6 for TM_LABEL),
**the very next keypress commits that line as the label** — "Label not found"
for a GTO the owner never typed, or a store into a garbage-named variable.
Secondary loss: f+UP/f+DOWN during TAM name entry is natively the case-toggle
gesture, and the console steals it unconditionally, so upper/lower case is
unreachable while entering a name inside the console.

**Contract violated.** `forth_capture.c:48-51`, the suspension contract
itself: *"The line is NOT carried across the suspension … TAM is free to use
aimBuffer for its own name entry while we are suspended; resume refills from
the step."* `forthHistoryRecall` writes `aimBuffer` while suspended, so TAM is
not in fact free to use it. And `keyboard.c:2833-2838`'s own comment states
the narrower contract the predicate does not implement: *"f-up recalls an
OLDER line into an OPEN interactive capture … falling through to the landed
case-change body when no interactive capture is OPEN"* — the guard is
`forthCapIsInteractive()`, not `forthCapIsOpen()`.

Not C5 (that is the `next == lineCount` wipe arm *inside*
`forthHistoryRecall`); this is the gesture reaching the function at all in a
state where it must refuse. Round 2 examined the same
`forthCapIsInteractive()`-where-"open"-is-meant question and cleared it on the
ground that *"plain TAM holds `tam.mode != 0` for the whole suspension … The
roll arm's identical guard clears on the same ground."* That clearing is
sound for the two sites that carry `!tam.mode` (the render gate and the roll)
and says nothing about this one, which never reads `tam.mode`. The packet that
fixed the guard's text verbatim (`PACKET_L1_H` C4) was written when an
interactive capture could not be SUSPENDED at all — interactive suspend/fold
is L1-F1/F2, which lists L1-H as a prerequisite.

**Bug class.** Same as R2: *origin tested where openness is meant.* This is
the instance the `tam.mode` clearing does not cover.

**Class test.** For every console gesture handler — recall ±1, the roll, R/S,
ENTER, EXIT, the A toggle — drive it with the capture SUSPENDED under a live
TAM and assert `aimBuffer`, `T_cursorPos` and `displayAIMbufferoffset` are
byte-identical before and after.

---

### R5 — `stringLastGlyph(aimBuffer) + 1` puts `T_cursorPos` inside the last glyph when that glyph is two bytes, and the AIM editor then draws no cursor at all

One expression, four package sites. Two finders raised it independently (as
"the recall site" and "three more sites"); both survived.

**Where.** `packages/forth-core/programming/manage.c:1883` (history recall),
`:1517` (the error re-open), `:936` (the `ITM_FORTH` EDIT extraction), and
`packages/forth-core/forth_compile.c:1777` (the seeded open). The two
inherited upstream copies at `manage.c:910`/`:918` mirror
`src/c47/programming/manage.c:812`/`:820` and are out of the package's charter.

**The arithmetic, executed rather than reasoned** (a verifier compiled
upstream's real `stringLastGlyph` body, `src/c47/charString.c:453-479`, and ran
it): the loop's `if(next > lg) next = lg;` clamp means it returns the byte
offset of the **start** of the last glyph. For `"2 3 \x80\xd7"` (length 6) it
returns 4, so `+1` = 5 — the second byte of `×`. For `"ABC"` it returns 2 and
`+1` = 3 == the byte length, which is why the idiom looks right: it is
correct for ASCII and silently lands mid-glyph otherwise.

**Reaching input A — history recall.** FORTH → the keys/alpha toggle → ALPHA
row → MATH softkey → the ×/÷ page → type `2`, space, `3`, space, then the `×`
softkey (`ITM_CROSS` = `STD_CROSS` `\x80\xd7`, `src/c47/softmenus.c:676`;
`DESIGN.md` §3.3.3 blesses exactly this spelling and `forth_prims.c:241`
aliases it to `pMul`) → ENTER (X = 6, the REPL reopens empty) → **f-↑**
(`CHR_caseUP` → `forthHistoryRecall(-1)`). `ITM_CROSS`'s item func is
`addItemToBuffer`, which inserts `itemSoftmenuName` with **no trailing
space** — unlike the word-insert path at `forth_menu.c:43-48`, which appends
one and would have masked this.

**Reaching input B — the error re-open, which is the worst of the four.** In
the alpha excursion type `1 ` then the `∡` softkey (`ITM_ANGLE` = `STD_ANGLE`
`\xa2\x20`) and ENTER. The tier-1 gate is structural and passes; the tokenizer
consumes `\xa2\x20` as ONE glyph-token, fails to resolve it, and the error arm
restores `preRunCopy` and sets the cursor mid-glyph — **precisely when the
owner is being asked to correct the line.**

**Reaching input C — EDIT.** In PEM, park on an `ITM_FORTH` source step whose
text ends in `×` and press EDIT.

**What breaks.** The line appears with **no cursor anywhere.**
`screen.c:3885` sets `cursorEnabled = (T_cursorPos == tmplen)` → false (5 ≠ 6),
suppressing the trailing block cursor (the only paints are the two blink
sites at `screen.c:520`/`:558`); and `showStringEdC47`'s render loop advances
`ch` glyph-wise (`screen.c:1687-1716`: 0,1,2,3,4,6), so `if(ch == edcursor)`
with `edcursor` 5 never fires and the in-line cursor is never drawn either.
The function tail has no post-loop draw and no snap to a boundary; the arm's
only normalization is a range clamp, which 5 passes. The editor looks dead.
Then BACKSPACE: the CM_AIM arm (`keyboard.c:4710-4727`) splits at byte 5,
deletes the prefix's last glyph and re-splices, leaving `2 3 ` + a lone
`\xd7` — the `×` becomes a not-found glyph instead of being deleted, the
guard `T_cursorPos <= 1 + stringLastGlyph` then moves the cursor to 4, and a
**second** BACKSPACE deletes the SPACE, not the stray byte.

**Why it survived four rounds.** Every path that WRITES repairs it
(`bufferize.c:601-605` snaps the cursor up to the next glyph boundary); only
the paths that DISPLAY and DELETE believe it. And the two landed assertions
on this value (`test_capture.part.h:2637`, `:7879`) assert
`T_cursorPos == stringLastGlyph(...) + 1` — they re-assert the suspect
expression rather than an independent end-of-line oracle, so they cannot fail
on it.

**Contract violated.** `DESIGN.md` §3.3.3: *"C47 two-byte glyphs are `lead
byte & 0x80` + **arbitrary second byte** … a byte-wise space scan can split a
glyph"*, and §8 repeats it as *"load-bearing"*. §3.3.3 also affirmatively
rules that trailing two-byte glyphs are first-class Forth source (`STD_CROSS`
and `STD_DIVIDE` resolved by prim-table aliases), so `1 2 ×` is a supported,
committable line. Upstream's own end-of-buffer idioms are glyph-safe:
`T_cursorPos = stringByteLength(aimBuffer)`
(`src/c47/c47Extensions/xeqm.c:94`, `addons.c:563`) and
`stringNextGlyph(aimBuffer, stringLastGlyph(aimBuffer))`
(`keyboardTweak.c:1516`). The comment at each of the four sites reasons only
about the EMPTY string (*"stringLastGlyph(\"\")+1 == 1, which would insert
every glyph behind the terminating NUL"*), and
`PACKET_L1_H_history_program.md:253` carries the same expression, so the
**spec inherits the defect** rather than the implementation deviating from it.
`git log -S` shows the lineage: cc41b7320 (PEM) → 7862b896b (F6-1) →
53002596c (S3) → bf90667d1 (L1-2) → cf938b49c (L1-H) — propagated by name,
never re-derived.

**Bug class.** *A correct-for-ASCII end-of-buffer idiom propagated by name;
the empty-string case was considered at every site and the two-byte case at
none.*

**Class test.** A glyph-boundary oracle applied to every site that sets
`T_cursorPos`: for each of the four entries, seed a line ending in each
two-byte glyph reachable from the alpha keypad (`×`, `÷`, `∡` at minimum) and
assert both `T_cursorPos == stringByteLength(aimBuffer)` **and**
`stringNextGlyph(buf, stringLastGlyph(buf)) == T_cursorPos`. The two
tautological assertions at `test_capture.part.h:2637`/`:7879` must be
replaced, not extended.

---

### R6 — K2's token-boundary guard tests one BYTE before the cursor, so a glyph whose second byte is `0x20` defeats it

**Where.** `packages/forth-core/forth_menu.c:42` —
`int32_t lead = (T_cursorPos > 0 && aimBuffer[T_cursorPos - 1] != ' ') ? 1 : 0;`

**Reaching input.** FORTH → toggle to the alpha excursion → ALPHA row → MATH
softkey → the `∡` page → press `∡` (`ITM_ANGLE`, func `addItemToBuffer`,
`itemSoftmenuName` = `STD_ANGLE` = `"\xa2\x20"`; grep confirms it is the ONLY
multi-byte macro in `src/c47/fonts.h` whose code contains `\x20`). `aimBuffer`
= `{0xa2,0x20}`, `T_cursorPos` = 2. Press **EXIT once** — `executeFunction`
auto-pops the MATH row after any `addItemToBuffer` item in CM_AIM
(`keyboard.c:1545`), so one press returns to the FWRD row; two would close the
capture. Then any word softkey on the picker (`pickerInsertName` →
`forthCapInsertName`), or equivalently any CAT_FNCT/PTP_NONE keypad key in
keys mode, which routes to the same inserter at `items.c:775`.

**What breaks.** The guard reads `aimBuffer[1]` = `0x20`, concludes "a
separator is already there", and inserts none. The line reads `∡DUP ` where
the owner asked for `∡ DUP `. The outer interpreter's tokenizer is glyph-wise
(`forth_compile.c:246-260` advances with `stringNextGlyph`), so the `0x20`
inside `∡` is never a delimiter and the first token is the single 5-byte
`\xa2\x20DUP` — undefined. ENTER reports an unknown word for a token the
owner never typed, which is the exact failure the guard exists to prevent.
Narrow: `∡` is the only glyph in the standard set that triggers it. The guard
is nonetheless wrong for the whole class.

**Contract violated.** The guard's own comment at `forth_menu.c:37-41`
(*"a name must land as its own token … the direct F6-3/picker/keys-mode paths
glued digits to names"*) and `DESIGN.md` §3.3.3's rule that byte-wise space
tests are illegitimate because a two-byte glyph's second byte may be any
value — including `0x20`. Round 1 looked at this expression and cleared it:
*"forthCapInsertName's new `lead` term … checked algebraically and both
correct"* (`AUDIT_stages-K-to-N_2026-08-06.md:1139`). The algebra **is**
correct; the defect is the byte-wise read, which that pass did not examine.

**Bug class.** R5's family, with its own sub-class: *a separator test on a
byte that can be a glyph's payload.*

**Class test.** Table-driven and self-maintaining: for every two-byte glyph
macro in `fonts.h` whose second byte is `0x20` (enumerable by grep, currently
one), insert it, then insert a name, and assert the result tokenizes into two
tokens. Pin the macro count so a newly added such glyph fails the test rather
than the calculator.

---

### R7 — an interactive fold taken from alpha input destroys the C17 frame stamp, and the resume re-pushes the row unregistered

**Three finders raised `manage.c:1379-1381` independently, from D4, D5 and
D7.** That is this round's strongest agreement. A fourth finder raised its
mirror image (that the resume DUPLICATES the stamped frame) and it was
refuted — with the correct residual inside it; see §6.1(a).

**Where.** `packages/forth-core/programming/manage.c:1236`
(`_closeAlphaMenus()` from `forthCaptureSuspend`) and `:1379-1381`
(`if(!forthCapKeysMode()) { showSoftmenu(-MNU_ALPHA); }`).

**Reaching input.** Needs a parameterized item keyed while the console is in
alpha input. A fresh interactive capture **opens in alpha input by default**
(`forth_capture.c:13`, *"a fresh capture starts in alpha input (owner
default)"*), so this is not an exotic sub-mode; the stock keymap simply
exposes no parameterized item on the AIM plane, so one prior ASSIGN is
required — which is what ASSIGN and USER mode are for. The verifier
constructed it two ways:

1. **USER-mode key.** From NORMAL, ASN, press the STO key (`ITM_STO`,
   `TM_STORCL`, `PTP_REGISTER`); with `previousCalcMode == CM_AIM` the assign
   writes `kbd_usr[key].primaryAim` (`assign.c:947`, `:1027-1029`); f+2 sets
   FLAG_USER. `determineItem` honours `kbd_usr` in CM_AIM unconditionally
   (`keyboard.c:1684`).
2. **MyAlpha slot.** `menu_ALPHA` carries `-MNU_MyAlpha` at f+softkey1
   (`src/c47/softmenus.c:1015`); `assignToMyAlpha` → `assign.c:848-854` writes
   the item into `userAlphaItems[]`.

Then: FORTH → (already in alpha, or f+XEQ to get there — `ShowSurface`
retargets slot 0 to `-MNU_ALPHA` **in place**, stamp intact) → the assigned
key. `processAimInput` matches nothing, so `btnReleased` runs
`runFunction(ITM_STO)` → `items.c:773-789`: PTP_REGISTER so not the
name-insert arm, `TM_STORCL` = 10006 inside the TAM range → `tamEnterMode` →
`ui/tam.c:1180-1182` `forthFoldEnter` + `forthCaptureSuspend()` →
`manage.c:1236` `_closeAlphaMenus()`, whose `MNU_ALPHA` case
`popSoftmenu()`s the console's OWNED frame — `popSoftmenu` shifts the array up
and memsets the tail, so the stamp is **destroyed, not relocated**. Type the
register digits; `tamProcessInput`'s epilogue →`forthFoldUnwindIfDone` →
`forthCaptureResume` → `manage.c:1380` raw `showSoftmenu(-MNU_ALPHA)`, which
neither calls `forthConsoleRegisterSlot0` nor routes through
`forth_menu.c`'s single owner.

**What breaks — four distinct outcomes, all verified by reading the
predicates.**

(a) *No stamp anywhere.* `forthConsoleBaseOnTop()` falls back to identity and
answers TRUE on `-MNU_ALPHA`, so EXIT's excursion rung commits
`keysMode = 1` and `forthConsoleShowSurface()` falls through all three
branches and does nothing: the ALPHA keypad displayed with the keyboard on the
keys plane. `items.c:768`'s toggle is broken the same way. R3's end state, by
a different door.

(b) *A duplicate, unstamped row.* When slot 0 is not the console's ALPHA row
at resume — an overlay is up, or the catalog is still non-zero because
`leaveTamModeIfEnabled`'s TM_STORCL + `-MNU_MVAR` branch
(`ui/tam.c:1383-1386`) sets `numberOfTamMenusToPop = 0` and skips
`catalog = CATALOG_NONE`, defeating `showSoftmenu`'s `!catalog` early return —
the same line pushes a second, unstamped ALPHA over the stamped one. Two
visually identical rows; `forthConsoleBaseOnTop()` now reads false, f+XEQ is
**silently refused** (`items.c:768` returns with no message), and EXIT presses
produce no visible change. The push also sets
`firstItem = lastCatalogPosition[catalog]`, so the row can return on an
arbitrary page.

(c) *C17's consequence, restated.* If the owner has their own ALPHA row deeper
in the stack, `pushSoftmenu`'s dedup LIFTS and consumes it
(`softmenus.c:3671-3683`) — their frame gone, EXIT eventually revealing
whatever was under it.

(d) *Not confined to the pre-ENTER window* — found by the correctness
verifier, and understated by all three finders. With the unstamped duplicate
at slot 0 and the stamp at slot 1, `forthInteractiveEnter`'s REPL reopen
(`manage.c:1551-1560`: `forthCapSetKeysMode(true)` then
`forthConsoleShowSurface()`) takes neither slot-0 branch and retargets the
**buried** frame, leaving the duplicate ALPHA row visible with keys mode ON —
K-R3 broken on screen *after* the ENTER that is supposed to be the repair.

In **keys** mode the branch does nothing at all: `_closeAlphaMenus` has no
`MNU_FORTH` case, so it hits `default: return` on the first iteration, the
stamped FWRD frame survives the suspension, and the resume's ALPHA push is
skipped. That is why the keys-mode fold traces clean (§6.2) and why this went
unseen for three rounds.

**Contract violated.** `forth_menu.h:24-27` / `forth_menu.c:273`
(`forthConsoleShowSurface` is *"the ONLY function that changes the console's
row"*); the C17 banner at `forth_menu.c:340-345`: *"HAND-ROLLED ALPHA
acquisition: re-establishing the ALPHA surface never goes through
showSoftmenu, whose dedup would LIFT a user's own ALPHA row to slot 0 — where
the next calcModeNormal() destroys it (C17's class)"* — `manage.c:1380` is
re-establishing the ALPHA surface, through `showSoftmenu`. `DESIGN.md`
§8.4.4's *"enforced at every keysMode writer"* enumerates three;
`manage.c:1315` (`forthCapSetKeysMode(keysWas)`) is a fourth and
`manage.c:1379` is its ad-hoc row establisher. The comment two lines below the
call (*"in keys mode the underlying menu row IS the mode indicator — resume
must not cover it with the alpha menu"*) shows the site was reasoned about for
the keys case only.

**Bug class.** *A surface with a declared single owner and a second
establisher that predates the declaration.* Enumerable: every `showSoftmenu`
/ `popSoftmenu` call reachable with an interactive capture open.

**Class test.** The TAM-driven interactive fixture the handoff has asked for
three rounds running: fold from **both** sub-modes, with and without an
overlay, with and without a catalog left up; after suspend **and** after
resume assert that exactly one frame is registered, that it is the row on
screen, and that `keysMode` and the row agree. Plus the census: every
`showSoftmenu`/`popSoftmenu` outside `forth_menu.c` is either unreachable with
`forthCapIsInteractive()` true or routes through the owner. Note §4 U1 before
writing it: the round-4 ownership oracle will fire falsely at the suspend
step.

---

### R8 — `forthConsoleShowSurface` has an unestablished precondition, and three of its four call sites cannot establish one

**Where.** `packages/forth-core/forth_menu.c:593` (the fall-through), against
`items.c:768` and `keyboard.c:4137`.

**The contract defect, independent of any gesture.**
`forthConsoleBaseOnTop()`'s identity fallback (`forth_menu.c:462`,
`return currentMenu() == -MNU_FORTH || currentMenu() == -MNU_ALPHA;`) reports
"base on top" with **no base at all**. Its own safety argument is scoped to a
single caller — `:456-458`, *"it can only make rung 3 decline a pop, never
over-pop"* — and C18 added two decision points that read the same boolean as
"ShowSurface will be able to follow the flip", which the fallback does not
guarantee.

**Reaching input** (the finder's own, and the weakest part of this round; the
verifier could not kill it but calls the gesture contrived). From the console
in keys mode press eight DIFFERENT f-/g-shifted menu keys: f+7, f+8, f+9,
f+/, f+4, f+5, f+6, f+* → `-MNU_EQN`, `ADV`, `MATX`, `STAT`, `BASE`,
`UNITCONV`, `FLAGS`, `PROB` (`assign.c:29-38`). In keys mode `determineItem`
resolves each through the NORMAL columns (`keyboard.c:1843`);
`processAimInput` matches no negative item so `keyActionProcessed` stays
false, `showFunctionName` records it (`screen.c:2133`) and `btnReleased`'s
`if(item < 0) { setCurrentUserMenu(item, funcParam); showSoftmenu(item); }`
(`keyboard.c:2389-2391`) pushes it — the route `items.c:736-741` documents in
prose. `SOFTMENU_STACK_SIZE` is 8 (`src/c47/defines.h:1514`) and
`pushSoftmenu` shifts the whole array on every non-dedup push, so push 7 drops
the owner's pre-FORTH menu and push 8 drops the console's STAMPED frame. None
of the eight is a catalog (`calcMode.c:100-190`), so the console stays open.
Now eight EXITs: each takes the overlay rung and calls
`forthConsoleShowSurface()`, which with zero stamps falls through its
slot-0/slot-1..7 arms and returns unchanged. On press 8 `popSoftmenu`'s CM_AIM
compensation renames the revealed MyMenu to MyAlpha and calls
`changeToALPHA()` (`softmenus.c:3719-3733`), pushing an UNSTAMPED
`-MNU_ALPHA`.

**What breaks.** Keys input with the ALPHA row displayed and the FWRD word
picker gone for the rest of the session: the A toggle passes the identity
fallback, commits the flip, and `ShowSurface` again changes nothing, so every
press flips the mode under a frozen row. The only escapes are ENTER (which
reaches `forthConsoleRestoreSurface`, the sole re-acquirer) and EXIT (which
closes the session). So "stuck-state" overstates it: this is a mode/row
desync plus a permanently lost picker, not a trap.

**Contract violated.** `keyboard.c:4135-4137`: *"Runs with the base on top by
construction (the overlay rung above already broke), so the flip can never be
committed where the row cannot follow (C18)."* False whenever nothing is
registered. Plus `DESIGN.md` §8.4.4's no-disagreement rule.

**Why it is in §3 and not §6.** R3 and R7 reach the same state on gestures the
owner actually makes. The state is the finding; this is the third route to it,
and it is the one that shows the contract is unsound rather than merely
un-defended. Stacking rows over the console is a first-class state the
battery already fixtures (`test_console.part.h:1436`, `:1924`).

**Bug class.** *A safety property proved for one caller, then reused by
callers whose failure mode it does not cover.*

**Class test.** Assert the C18 property directly instead of through the
predicate: for every `keysMode` writer, after the write, assert the row equals
the row the sub-mode requires — driven from a stack with NO stamp (destroy it
with `EXITALL`, or with eight pushes), where the writers must REFUSE rather
than commit.

---

### R9 — the round-4 ownership battery cannot detect a MISSING registration, and it is the only test that reaches the path where one can go missing

**MUTATION PROVEN, in a worktree, uncontaminated.**

**Where.** `packages/forth-core/test_console.part.h:2686` and `:2695` (test 36),
`:2552` (test 35), and `_consoleOwnershipOk` at `:2597-2624`.

**The mutation.** Delete `forthConsoleRegisterSlot0(!theirs);` at
`forth_menu.c:535` — the only registration in `_forthConsoleAcquireRow`'s FWRD
branch. Result: `./packages/forth-core/build-test.sh` exit 0, whole gate
GREEN (forth self-tests plus the 150 s upstream suite, zero FAIL lines in
3704 lines of output), with `test_console_stamp_never_outlives_capture`,
`test_console_surface_repair_ungated` and
`test_console_ownership_invariant` all printing their PASS banners. Not
vacuously green either: a probe at the deleted site fires **exactly three
times** in the entire suite — once in test 35, twice in test 36 — so both
tests genuinely traverse the mutated branch and still pass.

**Why the oracles cannot see it.** All four `_consoleOwnershipOk` clauses are
UPPER bounds (`owned > 1`, `borrow > 1`, the ordering check, and
`!forthCapIsOpen() && (owned||borrow)`), every one satisfied by
`owned = 0, borrow = 0`. And the two re-registration assertions read
`forthConsoleBaseOnTop()`, which with no stamp anywhere falls back to the
identity test and answers TRUE because `showSoftmenu(-MNU_FORTH)` did push the
row. The only lower-bound assertions in the file are `:2651`/`:2655`, both
immediately after `fnForthOuter`, guarding a different site (which the
mutation leaves intact).

**Why nothing else catches it.** `EXITALL` is the only line in the suite that
destroys every stamp while `keysMode` is true — it appears at `:2539` and
`:2684` and nowhere else. Test 35's `i == 0` fixture uses `XEQ 'CLSTK'`, which
does not pop a FWRD slot 0. So these two tests are also the only ones that
reach `_forthConsoleAcquireRow`'s FWRD branch at all.

**Owner cost.** A regression in the one surface-repair path only `EXITALL`
reaches ships with a green gate. On the calculator: FORTH over any menu,
`EXITALL`, ENTER, then the ALPHA toggle — key plane on alpha, row still
reading FWRD, and EXIT hands the owner's menu back with the console's own row
stacked on top of it.

**Contract violated.** The test's own messages claim what the oracles cannot
see (`:2687` *"the surface was not re-registered after EXITALL"*, `:2696`
*"excursion over a REBUILT base left an unregistered row on top"*).
`forth_menu.c:455-458` says outright that the branch which returns true is
precisely the state the assertion means to exclude. And the lower bound is a
stated contract: `DESIGN.md:2855-2857`. This is the same wrong-oracle defect
round 3 recorded as its fifth fixture defect
(`test_console.part.h:2470-2474`: *"The first draft of this test used
forthConsoleBaseOnTop() … so it answered 'true' for a stack with no stamp on
it at all"*), reused two tests later — this time in the direction that hides
rather than manufactures.

**Bug class.** Two, both enumerable: *an invariant asserted only as an upper
bound*, and *a predicate reused as an oracle in the direction where its
fallback hides failure*.

**Class test.** Replace both `forthConsoleBaseOnTop()` oracles with the stamp
census (`forthConsoleTestOwnedCount() + forthConsoleTestBorrowCount() == 1`)
at every step of both sweeps, and add the lower bound to
`_consoleOwnershipOk` — guarded by the capture state that genuinely permits
zero, which is the open question in §4 U1.

---

### R10 — subcase `[3a]`'s toggle-on oracle checks "not ALPHA" where its neighbour checks "is ALPHA"

**Where.** `packages/forth-core/test_capture.part.h:8421`.

**MUTATION PROVEN, and the backstops held.** Mutating
`forth_menu.c:561` to `_softmenuIndexOf(want == -MNU_FORTH ? -MNU_STK : want)`
puts slot 0 on `-MNU_STK` at the toggle-on, and `[3a]` prints
*"PASS: ALPHA gesture toggles keys mode both ways, softmenu tracks it"*. The
gate still went red — in `test_console_submode_row_agreement` and
`test_console_ownership_invariant`, exactly the backstops the finder named
(the finder's own unconditional `-MNU_STK` form would have tripped `[3a]`'s
FIXTURE FAIL at `:8404` instead of green-lighting it; the `want`-gated form is
what exposes the weak oracle).

**What is wrong.** Not coverage — the claim. `[3a]`'s PASS line says "softmenu
tracks it" and its FAIL message says "softmenu did not change", and
"changed to something" is all the body checks. The row after the toggle-on is
deterministic (`forthConsoleShowSurface` computes `want = -MNU_FORTH` and both
its paths end there), so an exact assertion was available. The toggle-off
assertion three lines down (`:8438`, and `[3(b)]` at `:8491`) is the exact
form, which is what makes the asymmetry visible: the C2 fix commit deliberately
strengthened `[3(b)]` and the C17 fixture repair rewrote only `[3a]`'s entry.

**Contract violated.** K-R3 as restated at `DESIGN.md:2838-2840` — *"the
underlying row IS the mode indicator"*. An indicator assertion has to name the
row it expects. Round 1's C2 already indicted this line
(`AUDIT_stages-K-to-N_2026-08-06.md:220-224`: *"it stays green because its
assertion is currentMenu() != -MNU_ALPHA plus a keysMode bit check. Nothing
asserts FWRD survived."*).

**Bug class.** *An assertion whose message claims tracking and whose body
checks non-equality.* Enumerable by grep: `!=` against a menu id in an
assertion whose message names a specific row.

**Class test.** No new fixture — tighten `[3a]` to
`currentMenu() == -MNU_FORTH`.

---

### R11 — `DESIGN.md` still states the invariant round 3's R4 retired, six lines before stating the corrected one

**Where.** `design-docs/forth-core/DESIGN.md:2856`.

**Both sentences were added by this range.** `git log -L2848,2860` shows the
stale clause came in with dad69ee16 (C17); the R4 fix (e3b52e83f) added 8
lines at `:2858` with **zero deletions**. So `:2855-2857` says *"The frame the
console relies on is REGISTERED by a sentinel in its own `userMenuId` …
**exactly one frame registered while a capture is open**"* and `:2861-2863`
says *"The invariant is **at most one borrowed base and at most one owned
frame, owned above borrowed** — the alpha excursion opened over the user's own
FWRD row registers both, and that is correct."*

**What breaks.** `DESIGN.md` is authoritative, so the next implementer or
auditor working from `:2856` writes `owned + borrow == 1` — which reddens on
correct code, because the round-4 battery's own `i == 1` sweep drives the
two-registration state (open over the user's FWRD row registers BORROWED at
slot 0; the alpha toggle shifts it down and stamps a second, OWNED frame at
slot 0). Or they file R4 for the third time. Round 3's R4 was CONFIRMED by
three independent readers and recorded as fixed *"in all three places"*; the
`forth_menu.c` banner (`:293-302`) and `forth_menu.h` (`:41-48`) are correct,
and the banner explicitly disowns the phrase: *"an earlier draft of this
banner claimed 'exactly one frame is registered', which is NOT what the code
does and was caught by three independent readers."* `DESIGN.md` is the one
copy the fix missed, and the authoritative one.

**Bug class.** *A rule stated in three places and corrected in two.* Round 3
named it; this is that finding surviving inside its own fix.

**Class test.** Not a test — a grep gate. `design-audit.sh` already verifies
`DESIGN.md`'s source citations resolve; add a deny-list of phrases an audit
round has retired. The project has now been bitten by this one twice.

---

### R12 — the error transcript's "transient until the next key" premise is false for the two keys the error recovery invites

**Where.** `packages/forth-core/programming/manage.c:1496` (the comment)
against `keyboard.c:2574` (the error sweep).

**Reaching input.** Open the console, type a line that raises at run time
(`1 BOGUS` → `ERROR_FUNCTION_NOT_FOUND`), ENTER. The error arm
(`manage.c:1493-1518`) appends the message to the ring, restores the line from
`preRunCopy` and returns with `lastErrorCode != 0`. Now press **BACKSPACE** to
fix the typo — precisely the gesture L5 exists for.

**What breaks.** `processKeyAction`'s error sweep is
`if(lastErrorCode != 0 && item != ITM_EXIT1 && item != ITM_BACKSPACE)`, so
BACKSPACE does not clear it; the CM_AIM arm of the item switch only refreshes;
`items.c:778`'s divert exempts `ITM_BACKSPACE` so it executes natively; and
`fnKeyBackspace`'s CM_AIM case (`keyboard.c:4700-4736`) deletes the glyph
without touching `lastErrorCode` — unlike its CM_NORMAL arm (`:4675`) and its
CM_MIM arm (`:4745`), both of which clear it first. Every `lastErrorCode`
clear that would help is on the softkey path (`executeFunction` `:1120`/`:1428`,
`btnFnPressed` `:801`) or inside `case CM_NORMAL` (`fnKeyExit` `:4031`). So
§8.4.4's five-conjunct gate keeps yielding on `lastErrorCode == 0`
(`screen.c:5703-5709`), the whole transcript stays gone and the stale error
text stays painted on REGISTER_Z, for as many BACKSPACEs as the owner presses.
**Correcting a rejected line — the documented recovery — is the one activity
during which the console is invisible.**

**Calibration.** The window is exactly the BACKSPACE run: the band returns on
the next non-BACKSPACE key, including the replacement character. Hence
"latent". It is here because round 4's R2 refutation leaned partly on the
sentence *"every key except EXIT/BACKSPACE clears the error, so the state is
not sticky"* — this is that sentence's counter-example on the console surface.

**Contract violated.** `manage.c:1494-1498`'s own premise: *"§8.7's error
PROTOCOL is unchanged — the native paint still covers the area until THE NEXT
KEY — but that paint is transient, and the transcript line is what keeps the
dialogue readable afterwards."* The two keys the sweep exempts are the edit
gesture §8.4.2 promises (*"the capture reopens with the line intact from the
pre-run copy (L5) — edit, don't retype"*) and the close gesture; for both,
"the next key" does not end the paint.

**This is an owner ruling, not an implementer's.** The sweep's exemption is
native and correct for CM_NORMAL, where those two keys ARE the dismiss
gestures with their own clears; the console inherited the exemption without
inheriting a clear, and it is the console that pays a whole-band price.
`STAGE_N_CONSOLE.md`'s non-goal ("no change to §8.7's error protocol") is why
this needs a decision rather than a patch.

**Class test.** Whatever is ruled, pin it: after a rejected line, for each key
in {BACKSPACE, EXIT, a letter, ENTER}, assert whether the transcript band
paints.

---

## 4. PLAUSIBLE and UNVERIFIED

### 4.1 PLAUSIBLE — survived refutation, no reaching input

**P-A — the resume fold-splice count is an unclamped unsigned difference
driving a loop with no NULL, END or iteration guard.**
`packages/forth-core/programming/manage.c:1335`,
`uint16_t n = getNumberOfSteps() - forthCapSavedStepCount();`. The two samples
are taken at suspend and at resume, and `getNumberOfSteps()` is keyed entirely
on `currentProgramNumber`, which `forthCaptureResume` never re-derives (it
sets `currentStep = p` and calls no
`defineCurrentProgramFromCurrentStep()`).

*Survived on correctness, by mutation.* Forcing the overshoot (`+ 2`) reddens
the gate with a SIGSEGV in the ordinary EXIT resume path, and the backtrace is
the proof of both halves of the claim: `deleteStepsFromTo (to=0x0,
from=…"\377\377…")` → `xcopy n=4095733758`. `to == NULL` is `findNextStep`
returning NULL with no guard between it and the arithmetic; `from` pointing at
`\377\377` means the previous iteration had already **deleted the program's
END** and the loop was standing on the terminator. The `aimBuffer` cap is the
only thing resembling a bound and it does not engage until ~196 folds, far
past the two iterations needed to destroy the program.

*Why PLAUSIBLE.* No live trigger. On every path the finder could enumerate the
two samples describe the same program and `n` is 0 or 1, matching the comment.
The one candidate that navigates the program pointer, `ITM_GTOP`, is PARK'd —
but PARK runs the item LIVE and `ui/tam.c:888-899`'s GTOP arm sits ahead of the
`calcMode` switch, so the forged CM_PEM bracket does not stop it; what could
not be established is whether `ITM_GTOP` is ever the item that ENTERS
`tamEnterMode` from the console (pressing GTO enters with `ITM_GTO`, which is
admitted, and the `.` that promotes `tam.function` arrives after admission).

*What would settle it.* Enumerate every item that can reach a TAM commit with
an armed or parked interactive fold and change `currentProgramNumber`.
Independent of that, the sibling sweep 700 lines away
(`forthFoldLeave`, `manage.c:2032-2055`) carries all four guards this loop
lacks — a comparison instead of a subtraction, a hard `i < 4`, and an explicit
`victim == NULL || isAtEndOfProgram(victim) || isAtEndOfPrograms(victim)` —
and its comment spells out why each is required. `forthFoldEnter` also
documents why the count's PROGRAM matters (`manage.c:1613-1620`).

**P-B — `_forthConsoleAcquireRow` decides OWNED vs BORROWED with the
whole-stack scan its sibling site documents as the wrong question.**
`packages/forth-core/forth_menu.c:528`.

*Survived on correctness with the path granted.* `theirs` is set by any
unstamped `-MNU_FORTH` frame anywhere on the stack. For a match at slot 0 that
is right (`showSoftmenu` early-returns and slot 0 stays the user's frame). For
a match at depth `i >= 1`, `pushSoftmenu` takes the dedup branch: it lifts
slots 0..i−1, **destroys** slot i, and writes a brand-new FWRD frame at slot 0
with `userMenuId = 0` — the console created a frame and buried the visible
row — and `forthConsoleRegisterSlot0(!theirs)` then registers it BORROWED. The
BORROW refusal guard cannot save it, because `RestoreSurface` only reaches
acquire when no stamp exists anywhere. At EXIT, rung 3 reads
`popHome = forthConsoleOwnsSlot0()` = false, so no pop runs and the console's
own row is left standing over the owner's menu. The open site making the
identical decision tests slot 0 only, and says why:
`forth_compile.c:1698-1706`, *"'did the stack grow?' is the WRONG question …
Scanning the whole stack therefore says 'nothing pushed' for a case that very
much did displace something, and EXIT would then leave the console's own row
up."*

*What would settle it.* A reaching input for "no stamp anywhere with an
interactive capture open **and** an unstamped `-MNU_FORTH` frame at depth ≥ 1".
R3, R7 and R8 all reach the first half; this is one push away from live. The
failure direction is under-pop, never over-pop.

### 4.2 UNVERIFIED — past the verification cap; no refutation lens ran

Listed as raised. None of these has been through a lens, and they should not
be treated as findings until one runs.

**U1 — the round-4 ownership oracle asserts a lifetime the design does not
have.** `test_console.part.h:2618`:
`if(!forthCapIsOpen() && (owned || borrow))` fires for `FCAP_SUSPENDED`, where
stamps must survive by design (`forthCapAbandonSuspended()` exists precisely
to unstamp when a suspension is ABANDONED). `forth_menu.c:302-305` says a
stamp must not outlive its **capture**, and a suspended capture has not ended;
`DESIGN.md:2858-2859` says the mark *"survives every REPL reopen and fold
resume by construction"*. The correct predicate is
`forthCapIsOpen() || forthCapIsSuspended()`. Whoever writes R7's fold fixture
gets a false failure at the suspend step — the same wrong-oracle shape that
manufactured three false failures in round 3. **Settle it by writing that
fixture**; this is the first thing it will hit.

**U2 — two writers of the BORROW stamp.** `forth_menu.c:555`: the fold-back
arm writes `softmenuStack[0].userMenuId` raw, bypassing the funnel the same
file calls *"the ONE site that decides ownership"* (`:523-524`) and its
refusal rule (`:388-390`, *"A BORROWED registration is refused while ANY stamp
exists"*). In the ordinary fold-back the revealed frame is the existing
borrow, so `!_stampedAt(0)` is false and nothing is written; minting a second
borrow needs an unstamped FWRD wedged between the owned frame and the borrowed
base, which the finder could not construct. Settle it by deciding whether that
state is possible; if it is, the round-4 battery's "at most one BORROWED" goes
red and an extra FWRD row is left standing over the owner's at close.

**U3 — C17's "the field is safe to borrow" premise is false for
`pushSoftmenu`.** `forth_menu.c:307-312` claims `userMenuId` is *"meaningful
only for -MNU_DYNAMIC frames (pushSoftmenu/popSoftmenu/fnGetMenu all gate on
that)"* and that a negative stamp is *"inert everywhere upstream"*.
`pushSoftmenu` reads `userMenuId` for **every** frame with no `menuItem` test
(`src/c47/softmenus.c:3652`, `:3657`), and the design *depends* on that ungated
read twelve lines later (*"an unmarked base no longer matches pushSoftmenu's
(softmenuId, userMenuId) dedup"*). No runtime failure is claimed. The cost is
the audit record: a reader re-checking C17 against a rebased upstream verifies
the wrong property, and the mechanism the wrong sentence hides — stamping a
frame permanently removes it from dedup — is how duplicate console rows appear
(the C9/RestoreSurface family, twice-found already, and R7(b) here). Settle it
by rewriting the sentence.

**U4 — the C18 refusal guard and ShowSurface's capability test different
predicates.** `items.c:768`. This is R8 from the upstream reader's angle, with
a different candidate route to the no-stamp state: `forthCapClose()` reached
while an interactive capture is SUSPENDED mid-fold (`forth_capture.c:34`
unstamps for ANY capture, including a PEM close). R2 makes that route live, so
U4 is probably a duplicate of R8 + R2 rather than a separate finding — check
that before counting it.

**U5 — the interactive EXIT ladder is 135 lines of package ownership logic
inlined into upstream `fnKeyExit`.** `keyboard.c:4072-4207`, inside upstream's
`case CM_AIM:` arm and ten lines above upstream's own surviving AIM ladder at
`:4208-4227` — from which it was copied and has since diverged (it replaced the
native CM_AIM test with `forthConsoleBaseOnTop()` while the original still
stands below). Realized at the next rebase, not at runtime, and this project
does rebase. Corroborated mechanically in §2: that block grew 121 → 136 lines
in this range. The equivalent seam is one call
(`if(forthCapIsInteractive() && forthConsoleKeyExit()) break;`), which is the
shape the package already uses for every other decision this range touched —
`forthConsoleBaseOnTop`, `forthConsoleOwnsSlot0`, `forthConsoleShowSurface`
are all calls, and `items.c:742-787` carries the same shape at smaller scale.

---

## 5. Design observations (D7 — first run in three rounds)

**D7-1. The console's row has a declared single owner and four writers.**
`forth_menu.h:24-28` says `forthConsoleShowSurface` is *"The ONLY function that
may change it while an interactive capture is open."* The other three are
`manage.c:1380` (the fold resume — R7), `screen.c:916` (the long press — R3),
and `manage.c:2518-2534`'s `ITM_AIM` arm in `insertStepInProgram` (gated on
`forthCapIsOpen()`, not on CM_PEM; the contracts finder could not reach it with
an interactive capture and correctly left it unreported). A contract that three
of its four writers get wrong is a defect of the contract, not of the writers.
Two shapes would fix it: make the field unwritable except through the owner
(move `softmenuStack` row selection behind an accessor), or accept that the
owner's real job is *repair on entry* rather than exclusive write and call the
repair from every keypress path — which is what round 3's R3 fix did for
committed lines, and it worked.

**D7-2. One predicate, two questions.** `forthConsoleBaseOnTop()` answers *is
anything stacked above the console's base* (a pop question) and three callers
ask *will ShowSurface be able to follow me* (a capability question). They
coincide only while a stamp exists. The fallback's conservatism was proved for
the pop question and is not conservative for the other. Three readers arrived
at this seam from three dimensions and produced R8, U4 and one refuted finding;
the disagreement about whether it is reachable matters less than the fact that
the predicate is doing two jobs.

**D7-3. C17 bought correctness and sold dedup, and only half of that is
written down.** Because the mark lives in `userMenuId`, the console's frames can
never match `pushSoftmenu`'s `(softmenuId, userMenuId)` dedup. That is exactly
why a borrowed base is safe from being lifted, and exactly why any native
`showSoftmenu` of a row the console also owns pushes a DUPLICATE instead of
lifting ours. Both are load-bearing; the banner asserts the opposite of the
second (U3). Worse, the asymmetry that keeps the ordinary case benign is
`showSoftmenu`'s own identity early return, which fires for static menus
(ALPHA) and not for dynamic ones (FWRD) and is gated on `!catalog` — nowhere
documented, and R7(b) is what happens when that conjunct is false.

**D7-4. Rules stated in three places drift.** The ownership rule now lives in
`DESIGN.md`, the `forth_menu.c` banner and `forth_menu.h`. It has drifted twice
in two rounds (round 3's R4, and R11 here — R4 surviving inside its own fix).
The banner is consistently the copy that is right and `DESIGN.md` is the
authoritative one; that is the wrong way round. Either the normative sentence
lives in one place and the others cite it, or `design-audit.sh` grows a check.

**D7-5. The fold/suspend window is the state the whole design reasons about
and no test enters.** Four of the twelve confirmed findings (R1, R2, R4, R7),
one PLAUSIBLE (P-A) and two unverified items (U1, U4) are in it. The handoff
has named the missing TAM-driven interactive fixture for three rounds. This
round ends the argument: the window is where the findings are, and its absence
is also why round 4's own new battery passes on a missing registration (R9) —
the battery's gesture sweeps have no TAM step in them.

**D7-6. Upstream discipline is drifting in exactly one place, and it is the
place with the most package logic.** Everything else this range touched is a
call. `fnKeyExit`'s block is 136 lines inside an upstream function (U5, and §2's
group-D delta). The mechanical half has been flagging this for rounds; it is
now the largest single inline block in the tree after `manage.c`'s orchestrator
group, and unlike that group it has a one-line seam available.

---

## 6. Deliberately not flagged

### 6.1 Refuted by the pass, and why

**(a) "The fold resume DUPLICATES the console's stamped ALPHA frame"**
(`manage.c:1379`, contracts). Killed at step one: `forthCaptureSuspend`'s
`_closeAlphaMenus()` has already popped that frame, so at resume there is no
registered ALPHA frame left to duplicate — frame count is conserved and nothing
is stranded one deeper. The accumulation half is independently impossible even
on the finding's own premises: after one hypothetical duplicate, slot 0 would be
an unstamped `-MNU_ALPHA` with `userMenuId` 0, so the next resume hits
`pushSoftmenu`'s early return and pushes nothing — growth caps at +1 and the
8-slot stack is never exhausted. **The residual is the opposite defect** (the
row comes back UNSTAMPED), which is R7. A refuted finding that contains the
correct finding, for the second round running.

**(b) "`forthCaptureSuspend` recommits at `currentStep` when `forthFoldEnter`
declined"** (`manage.c:1220`, lifecycle). Depends entirely on
`forthHistoryEnsure()` returning false, and every failure mode inside it faults
or corrupts before returning: `resizeProgramMemory`'s OOM arm calls
`backToSystem(NOPARAM)`, which with NOPARAM ≠ NOT_CONFIRMED sets
`backToDMCP = true` and **returns**, after which the caller xcopies through
NULL; `scanLabelsAndPrograms`' RAM_FULL early returns leave `programList` NULL
and `lblGtoXeq.c:120-121` dereferences it with no check. No fault-injection
hook exists (`grep` for `forthTest*Hist` / `FORCE_RAM_FULL`: no hits), so the
harness cannot reach it either.

**(c) "The C18 toggle gate is missing `forthCapIsOpen()`"** (`items.c:768`,
guards). The mechanical half is confirmed — with the capture SUSPENDED the flip
IS committed while `ShowSurface` returns on `!forthCapIsOpen()` — but the
consequence does not follow. `interactive && !open` is exactly and only
`FCAP_SUSPENDED`, and in that window neither value of `keysMode` can touch the
owner's line: the keys plane routes to `forthCapInsertName`, which returns false
when not open, and the alpha plane routes to `aimBuffer`, which the suspension
contract designates as TAM's scratch and which `forthCapOpen()` clears on the
way back. The flip described also moves resolution off the plane that can
execute live items onto the harmless one. And `manage.c:1379` makes the row
follow the flipped bit at resume, so the toggle is deferred by one resume, not
dead. *Note the tension:* the line that saves this finding is the line R7
indicts. Both hold — it re-establishes the ROW and fails to register the FRAME.

**(d) "`forthConsoleBaseOnTop`'s fallback answers true where `ShowSurface`
provably does nothing"** (`forth_menu.c:459`, contracts/guards). Refuted on
intent. The fallback's comment does not merely justify itself for one caller, it
**enumerates** the reach of the unstamped state (*"transiently possible
mid-line, between a destructive calcModeNormal() and the restore choke point, or
after an exotic external unwind"*), and round 3's R3 ruled directly on this
failure mode — its reaching input was verbatim *"console left with no row and no
stamp, after which EXIT's fallback identity test pops the user's own remaining
menus one press at a time"* — and fixed it by making the surface repair
UNCONDITIONAL at the one choke point that runs on every path out of a committed
line (`manage.c:1490`). `DESIGN.md` states the gates' condition as an OVERLAY
covering the base, and in this state there is none. What survives is a
documentation-scope nit. R8 is the same seam **with** a constructed route to the
state, which is why R8 is confirmed and this is not.

**(e) "The round-4 re-registration assertions cannot fail, and the invariant
checker omits the half the C18 gates depend on"** (`test_console.part.h:2687`,
guards). Refuted on intent: the missing `owned + borrow >= 1` conjunct is the
**outcome** of round 3's R4 adjudication, where "exactly one frame is
registered" was found false by three independent readers and deliberately
restated as "at most one borrowed, at most one owned, owned above borrowed";
adding the demanded conjunct would fire on states the design permits (the
fallback banner documents an open capture with no stamp as legitimate, and P1
carries a second such path awaiting a ruling). R3's pin is documented as test
35 with its own verified mutation; test 36's documented obligation is M-A.
**Distinguish from R9:** R9 does not ask for an unconditional lower bound — it
shows by mutation that deleting the FWRD-branch registration leaves the whole
gate green. "The checker omits a clause the design does not have" is refuted;
"no oracle in the suite sees a registration that never happened" is confirmed.

**(f) "Test 34's four rows have no positive control"**
(`test_console.part.h:2434`, tests). Refuted by execution. Row (d) at `:2487`
asserts `forthConsoleOwnsSlot0()` after a fresh `fnForthOuter`, and the
finding's own mutation (`forth_compile.c:1718` → `if(0)`) turns test 34 **red on
its own**, printing that FAIL and no PASS banner. What is true is narrower and
was not the claim: rows (a)–(c) carry no in-row positive control, so a mutation
that suppressed registration only on the non-restore path could leave those
three vacuous while (d) still passes.

**(g) "`forthConsoleBaseOnTop` is one predicate answering two questions of
opposite polarity, and its documented conservatism describes a ladder order C18
deleted"** (`forth_menu.c:457`, upstream). Refuted on intent: `DESIGN.md` §8.4
rules that all three `keysMode` writers consult the same question and prescribes
both dispositions (overlay rung pops; the toggle refuses); the unstamped arm's
decline is documented deliberate with a named owner for the repair; round 3's R3
closed the reachable window; round 4 set the precedent for unreachable-contract
findings (hardening, gate green, no test claimed). What survives is stale rung
LABELS in two comments (`forth_menu.c:452`, `forth_menu.h:46`) that
`DESIGN-HISTORY` explicitly renumbered — a comment refresh. The **shape**
observation is kept as D7-2.

### 6.2 Cleared by the finders, with the reasoning

**The C17 stamp machinery, re-derived rather than trusted.** Negative-stamp
inertness upstream was checked site by site, not assumed:
`removeUserMenuFromStack`'s renumber loop and removal arm are both inside
`menuItem == -MNU_DYNAMIC` (`softmenus.c:3903`), `fnGetMenu`'s
`userMenus[userMenuId]` index is inside its `MNU_DYNAMIC` arm (`:1477-1487`, so
`userMenus[-17987]` is unreachable), `popSoftmenu`'s
`currentUserMenu = softmenuStack[0].userMenuId` is likewise gated, and `src/`
has no other reader. No registration site can stamp a dynamic frame (three check
`currentMenu() == -MNU_FORTH` first; the fourth writes ALPHA into the slot before
registering). The sentinels `-0x4643`/`-0x4642` are inside `int16_t`. Index
discipline is clean: every scan is `0..SOFTMENU_STACK_SIZE-1` and `popSoftmenu`
memsets the vacated slot, so no stale stamp can sit above the notional top. The
hand-rolled ALPHA push omits nothing that matters — the refresh gates
short-circuit on `doRefreshSoftMenu` (which it sets),
`setScreenUpdateFromMenu` is a no-op for non-solver menus, FLAG_VMDISP is
re-derived by `popSoftmenu`, the `numberOfTamMenusToPop` accounting cannot apply
because `tam.mode` implies suspended, and the overlapping `xcopy` is the same
expression upstream uses in `pushSoftmenu`.

**The keys-mode fold, traced end to end and clean** — including the
FCNS-catalog variant. `_closeAlphaMenus` has no `MNU_FORTH` case so it hits
`default: return` and pops nothing; the TAM pops run with `calcMode` forged to
CM_PEM so `popSoftmenu`'s CM_AIM compensation cannot fire; the resume's ALPHA
push is gated on `!forthCapKeysMode()`; the FIX-9 drain finds no catalog. Owned
frame and stamp intact. This is why R7 is alpha-only, and it is worth knowing
that the common path is genuinely sound.

**Arithmetic caps.** `_forthCapBuildStep`'s one-byte length field (`n <= 255` at
every producer: the 256-byte source cap, the typing sink's
`len < 256 - inputCharLength`, and `forthCapInsertName`'s own algebra);
`preRunCopy[256]` with `xcopy(…, n + 1)` fitting exactly at 255; the picker's
15-byte slot stride against `TMP_STR_LENGTH` (highest byte written 2549 < 2560,
with `forthDictBrowseName` gating `nameLen <= 14` even though `FORTH_NAME_MAX`
is 31 — checked specifically because a 15-byte name would overflow);
`forthHistoryRecall`'s index clamp (`FORTH_HIST_BROWSE_NONE` = 0xFFFF caught
independently by `cur > lineCount`); and `cursorInString = T_cursorPos - 2`,
which looks like an underflow but whose only use is `2 + cursorInString`.

**The batteries this range added or repaired, verified rather than assumed.**
The C21 mirror: both oracles are exact, including the two facts that make them
non-vacuous — `lcd_fill_rect(…, LCD_SET_VALUE)` maps to BLT_ANDN and therefore
*clears*, so "lit px" counts drawn ink; and `forthPushInt32` really loads Y and
Z through `liftStack()`. The C19 deltas: each maps to a removable production
line, and the `l1[0] != '1'` check pins content rather than count. Test 34's
four negative oracles each fire on their intended mutation, and its
"every way a capture can end" enumeration checks out (`forthCap.state` is
written in exactly four places; both CLOSED writers are covered). Test 32's
nine gesture/overlay pairs pin BOTH `keysMode` and the row on refusal, so the
third disposition cannot hide in a disjunction, and gesture 3's buried-retarget
check is an exact delta. Test 30 rows 7–11: the slot-0 `userMenuId` comparison
works because unstamping writes 0, byte-identical to a native push.

**Documented-deliberate decisions, left alone.** The silent refusal of the
toggle under an overlay (`DESIGN.md:2839-2842` — the round-2 report's sanctioned
alternative, with EXIT as the documented recovery); the hand-rolled ALPHA
acquisition and the FWRD-only fold-back (both exist because of named
out-of-family attacks, and the fold-back's `popSoftmenu` safety argument was
verified against all four of `popSoftmenu`'s compensation branches); the
unconditional `forthConsoleRestoreSurface` after every committed line (round 3's
R3); round 4's hardening in `_forthConsoleAcquireRow`, explicitly not
mutation-provable (M-B removes it, gate green — the stage's third documented
gap); the picker's 170-name truncation, `FORTH_PICKER_MAX_SCAN_STEPS` cut-off
and NULL-calloc arm; `forthPickerGuard` testing menu identity rather than the
stamp (identity is the right test there — a user's own FWRD row is a legitimate
picker); the E7 PEM render offset. The FIX-9 catalog drain was probed and
cleared for want of a stack with a catalog UNDER the console's frame:
`fnForthOuter` drains catalogs at open, and `-MNU_CATALOG` itself maps to
CATALOG_NONE.

**Known-open findings, not re-reported.** C5, C6, C7, C10, C11, C13, C14, C15,
C20, C22, plus P1 and P2 awaiting rulings. Two specifics worth carrying:
`snprintf(echo, sizeof(echo), STD_RIGHT_DOUBLE_ANGLE " %s", aimBuffer)` at
`manage.c:1431-1432` is a genuine sizing defect (a 3-byte prefix plus a 255-byte
line does not fit 256, so lines of 253+ bytes truncate at a BYTE boundary and can
leave an orphan lead byte in the ring, against `forth_console.c:83-84`'s "dropped
WHOLE — never half") — it is the second half of C11 and is open. And the extra
unowned FWRD frame a second FORTH press pushes is C6, named in
`forth_menu.c:391-396`'s own comment. **Note that R5 is the same byte-vs-glyph
family at a site C10/C11 do not cover, so fixing them will not fix it.**

**Upstream defects not to patch.**
`xcopy(softmenuStack + i, softmenuStack + i + 1, (SOFTMENU_STACK_SIZE - i) * …)`
at `softmenus.c:3908` reads one element past the array (compare `:3930`, which
correctly uses `- i - 1`): genuine, upstream, unreachable from any Forth path
because console stamps never land on `-MNU_DYNAMIC` frames — recorded so it is
not re-discovered as new. The `isAlphaSubmenu` widening itself
(`packages/forth-core/softmenus.c:3888`) is the purest "predicate widened for one
caller" instance in the package; the file is outside this range, and R3 is its
consequence. And the three raw `showSoftmenu(-MNU_ALPHA)` calls on the
CM_ASSIGN / CM_ASN_BROWSER return paths (`keyboard.c:985`, `:2347`, `:4378`) are
the same class as R7 at inherited upstream sites — not reported because the
finder could not confirm the capture is still OPEN across an assign excursion.
**If R7 is accepted, those three are where to look next.**

**Documentation drift, noted not flagged.** `forth_menu.h:48` still annotates
`forthConsoleUnstampAll` as *"close funnel (forthCapClose) only"* while there are
three callers (the banner has it right, so the code is consistent). The stale
rung labels from 6.1(g). Rung 3's claim that `calcModeNormal`'s own
`-MNU_ALPHA` pop "can never fire here", whose stated reason (rung 2's
pre-normalisation) was deleted by C18 while the conclusion still holds for a
different reason.

**Coverage gaps recorded as gaps, because no assertion claims them.** Test 36
never stacks an overlay, so "at most one OWNED" is never asserted in the one
state where a second owned frame could be pushed (the buried-retarget state test
32 reaches but does not census). The FWRD fold-back branch
(`forth_menu.c:552-557`) is pinned by nothing: delete it and the toggle-back
retargets in place, leaving a duplicate FWRD above the user's; the pre/post
identity still matches and the duplicate is popped at close, so the only
owner-visible difference is the picker PAGE, which no oracle reads. The `theirs`
half of the FWRD acquire branch is never exercised — in both tests that reach it,
`EXITALL` has already popped the user's FWRD row, so `theirs` is always false,
and inverting `!theirs` is a second mutation the gate does not catch (same root
as R9).

**Diagnosability, not vacuity.** Four silent `fail = 1` sites with no message
(`test_console.part.h:2202`, `:2255`, `:2384`, `:2565`) turn the suite red with
no diagnostic beyond the harness's `[DEBUG] running <name>` line.

**Assertions already labelled as gaps by their own bodies.** Test 11's empty-band
assertion (*"It is NOT evidence of anything, and no comment here may claim it
is"*) and test 31's resume site. Both concede exactly what a finding would have
said; both left alone.

---

## 7. Verdict

**Would I ship it?** For what the stage claims, yes — the console works on the
gestures the design documents, the C17 ownership model is right, and the keys-mode
path (the common one) traces clean end to end. As *audited*, no: R1 and R2 lose
the owner's typed line on an ordinary cancel and leave a fold armed that silently
records a later `STO 05` as a program step. That is durable, crosses modes, and is
invisible until it bites. R3 is two gestures from the top of the console, is
enabled by default, and has no repair inside the session.

**Where would it break first?** In the fold/suspend window — press a parameterized
item from inside the console and cancel with a catalog up (R1 → R2). Second:
hold **f** with the console open (R3). Third: recall or re-open a line that ends
in `×` (R5). All three are one-gesture demonstrations, and none of them has a test.

**What I would leave alone if the goal were correct code rather than an audit
pass.**

- **R10.** The mutation it green-lights is caught two tests away. What is wrong
  is one PASS message's wording. Tighten the assertion when someone is next in
  that file; do not schedule it.
- **R12.** An owner ruling with a small window and nothing corrupted. If the
  ruling is "leave it", the only work is deleting the comment's false premise.
- **P-B and U2** as *behaviour*: both are wrong-on-their-face tests guarding
  states nobody has reached, and the failure direction is under-pop. Fix them if
  the code is being touched anyway; do not open them as work.
- **U3** as *code*: nothing to change. As *prose*, do rewrite it — the sentence
  is what a future reader will verify C17 against, and it is false.
- **R8's gesture.** I would not chase eight stacked menu rows. I would fix the
  *state* it reaches, because R3 and R7 reach the same state on gestures the
  owner actually makes.

**What I would not skip.** R9. It is "only" a test finding, but it is the only
thing standing between the FWRD surface-repair path and a silent regression, and
the mutation proof is unambiguous: the registration can be deleted and the whole
gate stays green.

**The pattern, fifth round running.** Nine of the twelve confirmed findings are
in code or tests this range wrote, or in pre-existing lines that this range's
fixes made harmful (R2, R3, R5's error-re-open site, R7, R8, R9, R10, R11, R12);
three predate it (R1, R4, R6). The standing note holds — *relocating state is the
most dangerous fix shape.* C17 relocated ownership from a capture field into the
frame, correctly, and six of this round's findings are about who is allowed to
write that frame.

---

## 8. Round and exit state

**Round 5.** Subject `b5a0202c9..ff9274f9f`. Tree clean at report time, gate
green, one worktree; everything in §2 measured at `ff9274f9f`.

**Readers.** Eight in-family dimension finders, blind to each other (D1 contracts,
D2 lifecycle, D3 arithmetic, D4 error paths, D5 guards, D6 tests, D7 design, D8
upstream) — the first round in which all eight ran, and the first D7 pass since
round 2, which is what round 4 asked for. Refutation: each finding to a reader
who did not produce it, one of three lenses (reachability / correctness /
intent).

**Numbers.** 29 findings raised. 24 verified — **17 survived, 7 refuted**. 5
exceeded the verification cap and are listed unverified in §4.2. §3 carries 12
numbered findings because three groups were the same expression or line from
different dimensions (R5 = two arithmetic findings on one expression; R7 = three
findings on `manage.c:1379-1381` from D4, D5 and D7) and two survivors had no
reaching input and belong in §4.1.

**Convergence, which is what the multi-reader form is for.** Three finders raised
`manage.c:1379-1381` independently from three dimensions (R7). Three finders found
three independent routes to the same state — open capture, nothing registered
(R3, R7, R8). A fourth finder's claim about the same line was refuted and
contains the correct residual (§6.1(a)) — round 4's lesson repeating: ask which
*version* a refuted finding is true of before discarding it.

**Out-of-family: one ran — see §9, which supersedes this paragraph.** The
synthesiser could not see it (it ran alongside the workflow, not inside it), and
wrote this section on the assumption it had not happened. What stands regardless:
the exit criterion needs two consecutive rounds with no new CONFIRMED finding and
at least one of them out-of-family, this round is not clean, so **the count
resets to zero and the earliest close is round 7.** The round-6 pass should be
aimed at the fold/suspend window with the functions inline — §9's packets covered
the surface-ownership machinery and deliberately not that window.

**Mutation discipline — a process defect this round earned.** Three readers ran
mutations. Two had their runs contaminated by *concurrent sibling readers mutating
the shared working tree*: one saw a baseline gate come back RED at a nominally
clean HEAD, with `forth_compile.c` and `forth_menu.c` both carrying foreign
`/* MUTATION */` edits, one of which was reverted between two of its own checks;
another's confirming green run was contaminated the same way and it said so.
Every one of them declined to revert another agent's work, which was the right
call. Only one reader used a `git worktree`, and its result (R9) is the only
mutation evidence in this report that is uncontaminated. `CODE_AUDIT.md` already
says *"where the tree must be touched to prove something, do it in a worktree,
never in the working tree the owner is using"* — this round is the proof of why,
and the workflow should enforce it: one worktree per verifier, or serialise the
mutating ones.

**Exit criterion: NOT MET,** and further from met than round 4 was.

**Open findings after this round.** New: R1–R12, plus P-A and P-B plausible and
U1–U5 unverified. Previously open and not re-reported: C5, C6, C7, C10, C11, C13,
C14, C15, C20, C22. Rulings owed: C12's layering, C18's catalog shape / R9's dead
line, whether C22 changes the C1 test, P1, P2 — and now R12, which is a ruling
rather than a fix.

**Where round 6 starts.** Write the TAM-driven interactive fixture (R7's class
test), because R1, R2, R4, R7, P-A, U1 and U4 all live inside it and because U1
says the round-4 oracle will fire falsely at its first step. Then the out-of-family
pass over the same window. Not with new coverage.

---

## 9. The out-of-family pass

Written after §1–§8, by the workflow's launcher rather than by the synthesiser,
which could not see this pass and wrote §8's "none ran" on that assumption.

**Reader.** Gemini 3.1 Pro, `agy --model gemini-3.1-pro-high --print-timeout
12m` — flag order per `CODE_AUDIT.md`. The model-name probe was answered
correctly on all three runs ("I am Gemini 3.1 Pro (High)"), so none of this is
Claude auditing Claude. Three self-contained packets over the surface-ownership
machinery, the highest-churn code in the range: **A** the acquisition and
retarget side, **A2** the same with a packet defect repaired, **B** the release
side — every close path, all three EXIT rungs, and one question: find a path
that ends a capture without leaving zero stamps.

Both results converge with the in-family half. That is the whole value of the
pass this round: it did not add a new surface, it **changed the weight of two
findings the in-family readers had already raised and ranked low.**

### R13 — rung 3's safety argument cites a mechanism C18/N1-5 deleted

Promoted out of §6.2's "documentation drift, noted not flagged", because two
families reached it independently and because §3 already carries R11, which is
the same defect in the same class one file away.

**Where.** `packages/forth-core/keyboard.c:4158-4164`, and the inline at
`:4202`.

**What it says.** *"rung 2's pre-normalisation renames slot 0 to id 1 IN PLACE;
it does not pop. `softmenu[1].menuItem` is `-MNU_MyAlpha`, NOT `-MNU_ALPHA`. —
so `calcModeNormal()`'s own pop … can never fire here."*

**Why it is wrong.** There is no pre-normalisation. `b96ae1b33` (N1-5) deleted
it, and rung 1's own comment seven lines above says so outright: *"The landed
form pre-normalised an `-MNU_ALPHA` slot 0 to MyAlpha (id 1) … Neither half
survives FWRD-as-home."* Every surviving mention in the ladder is comment text;
no code between `:4071` and `:4207` writes `softmenuId`. The rung number is
stale on a second axis — C18 reordered the rungs, so the "rung 2" being cited is
today's rung 1.

**What is actually holding it up.** The conclusion is still true, by a route the
comment does not state: rung 2 commits `keysMode` and breaks, so any press
reaching rung 3 has `keysMode` true, `forthConsoleShowSurface()` has already
made slot 0 FWRD, and `calcModeNormal()`'s `-MNU_ALPHA`-guarded pop declines on
a real FWRD row. The same unstated invariant is the only thing licensing
`if(popHome) popSoftmenu()` at `:4202`, whose comment names the same dead
mechanism.

**Bug class.** *A load-bearing comment that outlived its mechanism* — round 3's
R4 exactly, and R11's class. The consequence is not runtime: it is that the next
reader verifies rung 3 against a mechanism that is not there, and this audit's
own record is that these comments are what findings get argued against
(`§6.1`'s intent lens cleared four items by citing them).

**Fix.** Prose only. Replace both citations with the invariant that is really
doing the work, and state it as an invariant so R11's remedy applies here too.
No code changes.

### G2 — the same expression as P-B, from the other family, failing the other way

Gemini's A2 finding was **refuted** on its own terms: its premise was that the
open stamps whatever row is on top BORROWED. It does not — `forth_compile.c:1716`
computes `fresh = (currentMenu() != -MNU_FORTH)`, so a BORROWED stamp only ever
lands on a row that was **already FWRD**, and `ShowSurface`'s BORROWED branch
returns early on `cur == want` when `want` is FWRD. The FWRD acquire branch
cannot be reached from there at all. Its packet-B run repeated the same wrong
premise with an ALPHA base.

**But it attacked `forth_menu.c:528-535` — P-B's exact expression — and found
the other failure direction.** P-B has the `theirs` whole-stack scan registering
a console-*created* frame BORROWED, so rung 3 under-pops and the console's row
is left standing. Gemini has `forthConsoleRegisterSlot0(!theirs)` *refusing*
outright when a stamp exists elsewhere, leaving slot 0 unregistered and
orphaned. Same line, same root — a function that establishes a frame and then
delegates the stamping to a function entitled to decline — two consequences,
selected by whether a stamp exists anywhere else.

This is the **third independent out-of-family reader to attack this one
function on this one shape**: round 4 records both readers doing it, and round
4's response was to harden the OWNED arm on the argument that *"two independent
readers finding the same shape is the agreement `CODE_AUDIT.md` says to act
on."* Round 4 hardened the arm the readers described. **The BORROWED arm four
lines below was left as it was, and it is the arm P-B and G2 both land on.**
`§6.2` separately records that the `theirs` half is exercised by no test, and R9
records that its registration can be deleted with the gate staying green. Three
readers, two families, one untested expression.

**Recommendation.** P-B should be read as a design finding needing a ruling, not
as an unreached plausible. Its reachability is still open — the failure
direction is under-pop either way — but round 4's own standard for acting
without a reaching input is met twice over.

### Packet defects, which were mine

Two of Gemini's three runs produced findings whose premises came from my
packets, not from the code.

1. **Packet A condensed a load-bearing comment.** `forth_menu.c:579` reads *"or
   nothing is registered at all — a line just destroyed the surface and
   `forthConsoleRestoreSurface()` is the re-establisher, not this function."* I
   sent the first half, and omitted `RestoreSurface` from the excerpt. Gemini
   reported "a destroyed ALPHA frame is never re-acquired" and quoted my
   truncation back as its evidence. A2, with the comment restored verbatim and
   the function included, did not repeat it.
2. **Both packets defined what the stamps MEAN and never stated the open site's
   precondition.** Both then assumed the open borrows whatever is on top, which
   is the premise both wrong findings rest on. One omission, two runs, two
   plausible-looking wrong findings.

**Fourth and fifth packet artefacts in five rounds.** The standing rules were
"send whole functions" and "name every package override the excerpt depends on".
Neither caught these. Two additions:

- **Condensing a load-bearing comment is truncation.** If a comment names a
  function, that function is part of the packet.
- **The orientation block must state the PRECONDITION of every state the packet
  discusses, not just its meaning.** "BORROWED = the user's own row" is not the
  same claim as "a BORROWED frame is always FWRD", and only the second one
  blocks the wrong trace.

**One rule to relax.** `CODE_AUDIT.md` says ~3 KB works and a 13 KB packet
returned nothing. These were **9.6 KB and 10.9 KB and all three were answered
in minutes**, with the model probe passing. The ceiling is higher than the
record suggests; the 13 KB failure was probably not size alone.
