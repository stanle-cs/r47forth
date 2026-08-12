# Audit — round 11, in-family leg: round 10's fix wave plus the undocumented 2026-08-10 console wave, at `868a991ab`

Full subject: the round-10 fix wave (`8010d8c8a` comment sweep + gate
instruments, `ea210c9ba` the four red-first fixes, `7b8299db9` the
FHIST-RESERVED ownership test); the undocumented 2026-08-10 console wave
(`d5f4811aa` terminal controls, `8a3c6e146` the two refresh seams,
`b5636f6c9` `.S` over the D3 spill region); and the 2026-08-11 package
changes (`5412a6992`, `868a991ab`). Range `8010d8c8a~1..HEAD`, 21
commits.

**Ten CONFIRMED findings, one PLAUSIBLE. Nine of the ten are in the
console wave that landed with no design record and was never audited;
one is in round 10's fix wave; the comment sweep produced none.** That
breaks the pattern the regression record predicted (r2 4/7, r3 4/4, r5
9/12, r10 10/11 in the previous round's fixes), and it breaks it for a
reason worth naming: the fix wave was small, red-first and reviewed, and
a whole unaudited feature wave joined the range beside it.

The worst finding is not in the console at all. The D3 spill region has
never saved the register **tag**, and the tag is where a long integer's
sign lives — so a Forth line deep enough to spill and drain in the same
line returns the wrong number with no error. `.S`'s new spill walk
inherited the same omission, which is what finally made it visible.

Nothing was fixed. The tree this report finishes on is the tree it
started on.

*(Filename note: the operator passed the whole subject as the `subject` arg, and the runner derives the report filename from it — the name blew the 255-byte limit and was truncated. Renamed by hand to `AUDIT_round11_in-family_2026-08-11.md`, matching the round-7/9/10 convention. Keep the first clause of `subject` short: it becomes the filename.)*

---

## 1. Subject and coverage

**Tip.** `868a991ab` on `forth-core/stage-n`, which is also `main` and
`origin/main`. Range `8010d8c8a~1..HEAD` = `c3658a15d..868a991ab`, 21
commits, 2026-08-09 to 2026-08-11.

**Numbering.** The out-of-family leg of this round landed first and took
`R11-1` (`AUDIT_round11_out-of-family_2026-08-11.md`). This leg's
findings are `R11-IF-n` so a grep is unambiguous; R11-1 is **not**
renumbered and **not** re-reported here. §6a records where the two legs
disagree, because they do.

**Diff.** 129 files, +12,200/−10,670 across the whole range — dominated
by the comment sweep and the generated `files/`+`patches/` twins. The
surface that matters:

| area | files | + | − |
|---|---|---|---|
| package sources, sweep commit `8010d8c8a` only | 38 | 3,042 | 4,841 |
| package sources, everything after the sweep | 16 | 273 | 92 |
| package tests, after the sweep | 4 | 712 | 359 |
| design docs | 12 | 3,125 | 57 |
| generated (`files/`, `patches/`, manifest) | 40 | 4,157 | 5,376 |

So the *new, unaudited product code* in this range is 273 lines across 16
files, of which 96 are in `programming/forth_fold.c` and 73 in
`keyboard.c`.

**The comment sweep was verified code-neutral, mechanically, twice.** The
contracts dimension ran every in-scope file at `8010d8c8a~1` and
`8010d8c8a` through `gcc -fpreprocessed -dD -E -P` and compared the
whitespace-stripped token streams: all 15 identical (`keyboard.c`,
`forth_dict.c/.h`, `forth_compile.c`, `forth_console.c/.h`,
`forth_capture.c/.h`, `forth_bridge.c`, `forth_prims.c`, `forth_fold.c`,
`softmenus.c`, `items.c`, `manage.c`, `ui/tam.c`). The tests dimension
did the same for the six test files by stripping comment lines from both
sides — byte-identical, so no assertion was deleted under cover of the
ruling. That freed both budgets for the four functional commits, which is
the only reason a 12k-line range was auditable at all.

**Files read at line level** (union across eight dimensions):
`forth_inner.c` spill module and depth accounting in full;
`forth_prims.c` `pPrintStack` and the output words; `forth_bridge.c`
`forthConsoleFormatRegister`/`forthCopyWholeGlyphs`; `forth_console.c`
(ring, records, eviction, roll, EXIT ladder) in full;
`forth_console_view.c`'s five-conjunct paint gate; `forth_capture.c` in
full; `forth_menu.c`'s stamp helpers, `forthConsoleShowSurface`,
`forthConsoleRestoreSurface`, `forthConsoleHomeRow`, `forthCapInsertName`;
`programming/forth_fold.c`'s `forthInteractiveRun`,
`_forthRestoreCursorTuple`, the whole FHIST block
(`_forthHistProgramConforms`, `forthHistoryProgram`, `Ensure`,
`GotoLastStep`, `Evict`, `Push`, `Recall`), `forthFoldEnter`/`Leave`/
`UnwindIfDone` and the resume splice; `forth_compile.c`'s
`fnForthOuter`, `forthEnterAimSurfaceNoLift`, `forthTakeSourceFromX`,
`forthCheckSourceLine`, `classifyNumber`; and every changed region of
`keyboard.c` (`_forthCapAtCap`, all three cap seams, both refresh seams,
the `ITM_RS` arm, `fnKeyEnter`'s CM_AIM divert, `fnKeyExit`,
`determineItem`'s plane selection, `processAimInput`,
`btnPressed`/`btnReleased`, `executeFunction`).

**Upstream contracts consulted at source**, not from memory:
`keyboard.c` `processAimInput`/`determineItem`, `keyboardTweak.c`
`keyReplacements`/`Check_MultiPresses`, all ten `assign.c` layout tables,
`bufferize.c` `addItemToBuffer` and its pixel cap,
`registers.c`/`registers.h`/`registerValueConversions.c`/
`typeDefinitions.h` (the register header and tag semantics),
`memory.c` + `core/freeList.c`, `display.c`
`shortIntegerToDisplayString` and the matrix/vector formatters,
`error.c` `displayBugScreen`, `programming/manage.c`
`scanLabelsAndPrograms`/`deleteStepsFromTo`/`_insertInProgram`,
`lblGtoXeq.c` `goToPgmStep`, `softmenus.c` `menu_ALPHA` and the push/pop
pair, `flags.c` `SetSetting`, `items.c` rows for `ITM_SPACE`, `ITM_RS`,
`ITM_FORTH`, `CHR_num`.

**Tests read:** `test_console_terminal_controls`,
`test_console_print_stack_depth`, the new `test_console_print_stack_spill`,
`test_console_view_paints`/`_arm`, `test_console_story`;
`test_capture.part.h`'s subcases `[7]`, `[10]`, `[11]`, `[12]`, `[13]` of
`test_fold_round8_window`, `[7]` of `test_fold_round6_window`, the
input-cap battery, `test_keys_eex_and_numlock`; `test_dict_reloc.c`
registration; `test_engine.part.h`'s `test_data_stack_overflow_guard`.

**Deliberately not audited.** Round 10's closed findings and carried
rulings (P1, P2's push ruling, C22-vs-C1) — read for exclusion, not
re-reported. The catalogued `softmenus.c` TIMER-guard re-indent. The
documented `.S` best-effort skip as a *correctness* question (its
coverage gap is R11-IF-5). The `res/` reverts (`4a2d9e771`) and the
`R47_combo` short-tag report, both ruled.

**What the budget did not reach.** No hardware run and **no simulator
screenshot** — every reaching input below is traced or driven through the
self-test harness, never photographed; the one place that matters is
called out in §8. `forth_console_view.c`'s painter geometry and
`screen.c`'s render arm beyond the paint gate. The 2,253-line
`test_capture.part.h` diff was read at the subcases named above, not end
to end. The upstream `testSuite` script corpus beyond the two override
hunks. `display.c`'s formatter internals reachable from
`forthConsoleFormatRegister` were checked only for a `TEMP_REGISTER_1`
clash, not audited. DESIGN-HISTORY's and HANDOFF's new sections were read
through the diff by most dimensions and in full by two.

**Readers.** Eight in-family dimension finders (contracts, lifecycle,
arithmetic, errorpaths, guards, tests, design, upstream) via
`audit-workflow.js`, blind to each other; every surviving finding through
an independent refutation pass in an isolated worktree with a named lens
(reachability / correctness / intent). **Fourteen of the sixteen
refutation runs proved their verdict by executed mutation or probe rather
than by reading** — the highest ratio of any round so far, and the reason
several findings in §3 carry runtime output instead of an argument.

**Worktree hygiene, sixth consecutive round.** The stale-ref trap fired
again: most verifier worktrees spawned at `e21af8d28`, five commits
behind, and the first-action rule caught it — nine records open with
`git log --oneline -1` and a detached checkout of `868a991ab`. Four
verifiers found their worktree at `e21af8d28`, correctly established it
was an ancestor of the audited tip containing every commit their finding
named, and worked there. I checked whether that mattered: the two
commits they were missing (`5412a6992`, `868a991ab`) touch only
`screen.c`/`print.c`/`lcd.c` overrides and the docs, so **no code-level
verdict in this report depends on them**. Every worktree finished clean;
every mutation was reverted, including the `files/` twins,
`patches/*.patch` and `.refresh-manifest.json` that the gate's own
refresh regenerates from a mutated working area.

**A new runner defect this round earned**, reported by four verifiers
independently: the session scratchpad is **shared** between concurrently
running verifier agents, several of which chose the same log filename.
One agent's `baseline.log` came back with another worktree's header,
NUL-filled gaps from two writers on one `O_TRUNC` fd, and a tail naming a
sibling's `build.sim` — which reads exactly like the round-5
cross-worktree contamination and is not it. Round 5 isolated the *trees*;
the *logs* are still shared. Every affected agent re-ran to a
worktree-tagged path and re-verified its own build directory
(`meson-log.txt` / `build.ninja` referencing only its own worktree)
before standing by its evidence. It contaminated logs, never a tree.

---

## 2. Mechanical results

**Gate GREEN** at `868a991ab`, and green under eleven independent
verifier baselines across two refs — build clean, `FORTH SELF-TEST: ALL
PASSED`, upstream `meson test testSuite` OK (≈190–210 s). Compiler
warnings zero. Carried as a pre-verified fact and corroborated by every
mutation run's baseline.

**`design-audit.sh`, run for this report at `868a991ab`:**

| check | result |
|---|---|
| A. upstream footprint | override files **19 (budget 19)**, added lines **1078** (budget 1243), removed 348 |
| B. hunks whose added lines never mention Forth | **11 (baseline 11)** — every one dispositioned by addendum 18b's ruling |
| C. whitespace / blank-line churn | **none** — *and this is a false negative; see R11-IF-9* |
| D. contiguous added blocks ≥ 12 lines | **26 (baseline 26)**; largest is `010-keyboard.c.patch` at **82 lines** |
| E. allocations in package sources | 9, including the new `forth_inner.c:148` transient |
| F. generated output | synchronized with manifest, clean in Git |
| G. working-area files that would ship as firmware | none |
| H. DESIGN.md source citations | all resolve; every package symbol DESIGN.md names is live |
| I. enumerated-site pins | 15/15 ok |
| J. upstream-diff churn | **2** — the catalogued `softmenus.c` TIMER-guard re-indent, ruled, not a finding |

One triage group, and it is the ruled exception. **The audit script's own
blind spots are now two findings in this report rather than zero**: check
C prints `none` while `010-keyboard.c.patch` carries a deleted upstream
blank line (R11-IF-9), and check D's count fell 36→26 and was re-accepted
as a measured drop while its largest member grew 35→82 inside the same
wave (R11-IF-10).

---

## 3. CONFIRMED findings, worst first

Ranked by what they cost the owner. There are no crashes this round —
the one crash-class candidate is unconstructed and sits in §4 — so the
ranking opens with the only finding that makes the calculator produce a
wrong number and say nothing.

---

### R11-IF-1 — the spill record drops the register **tag**, so a spilled negative long integer comes back positive

`packages/forth-core/forth_inner.c:126` (refill) and `:152` (peek) —
**wrong result, silent.** High confidence, **executed**. Found by the
arithmetic and design dimensions independently.

**What breaks.** `forthSpillCatch` (`forth_inner.c:83-104`) writes
`[uint32 dataType][uint16 sizeInBlocks][payload]` and nothing else. The
register **tag** is not in the payload: it lives in the register header
(`src/c47/registers.c:334-338` writes `dataType` *and* `tag`), and for a
long integer the tag *is the sign* —
`convertLongIntegerToLongIntegerRegister` stores `longIntegerSignTag(lgInt)`
(`registerValueConversions.c:13`), and `getRegisterLongIntegerSign` is
`#define`d to `getRegisterTag` (`registers.h:295`). Both readers restore
with `setRegisterDataType(reg, (uint16_t)type, amNone)`. `amNone` is **5**
(`typeDefinitions.h:228`); `LI_NEGATIVE` is **1**. So
`convertLongIntegerRegisterToLongInteger`'s
`if(sizeInBytes > 0 && getRegisterLongIntegerSign(regist) == LI_NEGATIVE)`
(`registerValueConversions.c:33`) is never taken and the magnitude is
rebuilt positive — on the stack, in `.S`, and in every arithmetic that
follows.

**Reaching input — executed, not argued.** The suite's own
`test_data_stack_overflow_guard` subcase 2a
(`test_engine.part.h:10207`) with one literal changed:

```
XEQ 'CLSTK' -1 2 3 4 5 6 7 8 9 10 11 + + + + + + + + + +      (correct: 64)
    FAIL: same-line drain should give 64, got 66 type 0 (error 0)
```

`error 0` is `ERROR_NONE`; `type 0` is `dtLongInteger`, so the type
round-trips fine and only the tag dies. On the default 4-level stack it
takes five tokens. A probe driving `XEQ 'CLSTK' -5 1 2 3 4 .S DROP`:

```
PROBE0: no-spill .S="<4> -5  0.  0.  0."  X type=0 tag=1     (control)
PROBE:  .S line0="<5> 4 3 2 1 | 5"                            (the -5 prints as 5)
PROBE:  T type=0 tag=5 (LI_NEGATIVE=1 amNone=5)
PROBE:  refilled T reads as "5" (pushed -5)
```

**The silent window, established during verification.** A line that
*ends* with the spill still occupied stops loudly:
`forthDataDepthResync` (`forth_inner.c:240-251`) raises
`ERROR_RAM_FULL` and calls `forthSpillReset()`. The wrong answer is
therefore confined to lines that spill **and drain within the same
line** — which is every ordinary arithmetic line deep enough to spill,
and every `.S`. That narrows the trigger; it does not weaken it.

**Same drop, two more victims.** `getRegisterShortIntegerBase` is also
`getRegisterTag` (`registers.h:293`), so a spilled based integer comes
back claiming base 5 — legal, non-crashing, and wrong (it renders with a
`₅` subscript). `getRegisterAngularMode` masks the same tag
(`registers.h:291`), so a spilled DEG-tagged real34 or a polar complex34
comes back unitless / rectangular.

**Why five audit rounds and a green gate never saw it.** Every spill
fixture on record uses `dtReal34` (SP-1..5, WP-1/2,
`test_persist.part.h:623`) or positive long integers (`7 FACT = 5040`,
`:1069`). Plain reals legitimately carry `amNone`, and for a positive
long integer tag 5 and `LI_POSITIVE` (2) agree on "not negative". The
fixture set is exactly the set the defect cannot reach.

**Contract violated.** DESIGN.md §5.7 (`:1838-1840`):

> LIFO records `[uint32 dataType][uint16 sizeInBlocks][payload]` —
> byte-faithful register images, no second numeric representation.

The record is not a byte-faithful register image. `registerHeader_t`
(`typeDefinitions.h:415-424`) carries pointer, dataType **and** tag, and
documents the tag as *"Short integer base, real34 angular mode, or long
integer sign"* — value, not decoration. DESIGN.md §11's "the visible
window and every native's view remain exactly R47's" fails for the same
reason.

**Scope, stated honestly.** The wrong *answer* is carried by
`forthSpillRefill`, D3 code that predates this range. `b5636f6c9`
reproduced the identical `amNone` restore in the new
`forthSpillPeekInto`, which is what makes the defect **visible** — `.S`
now prints the sign-flipped value. The D3 packet specified the literal
`setRegisterDataType(reg, type, amNone)` form
(`QWEN_PROMPTS_D3_1_spill_region.md:32,102`) without deriving what the
tag carries, so the implementer followed the spec exactly. This is a spec
defect that has now propagated to three sites in one file.

**Bug class.** *Serialisation that claims byte-fidelity and omits a
header field.* Sibling already in the record: C11's orphan lead byte — a
copy that respects one boundary and not another.

**Class-level test that would pin it.** A round-trip table over exactly
the register kinds whose tag is load-bearing — negative long integer,
positive, zero, short integer in a non-decimal base, real34 tagged
DEG/RAD/DMS, complex34 polar. For each: push, force a spill, then (a)
read back through `forthSpillPeekInto` and (b) drain through
`forthSpillRefill`, asserting in both cases that the register compares
equal to a saved pre-spill copy **including `getRegisterTag`**, and that
`.S` prints the same glyphs above and below the `|`. The table is
enumerable from `registers.h`'s three tag accessors, which is what makes
it a class test rather than three cases.

---

### R11-IF-2 — ENTER types `+` instead of a space whenever NUMLOCK is on

`packages/forth-core/keyboard.c:3685` — **wrong result on a common
gesture.** High confidence, **executed**. Reported by three dimensions
(contracts, guards, arithmetic) with no contact.

**What breaks.** The N-R10 divert hands a synthesized `ITM_SPACE` to
`processAimInput`, the *physical-key* translator. Its **first** arm
(`keyboard.c:605`) is
`keyReplacements(item, &item1, getSystemFlag(FLAG_NUMLOCK), …)`, ahead of
the literal-`ITM_SPACE` arm at `:622`. With NUMLOCK set, `keyReplacements`
(`c47Extensions/keyboardTweak.c:1064-1095`) walks `kbd_std[15..36]`;
index 36 is key 85, whose `primaryAim` **is** `ITM_SPACE` and whose
`gShiftedAim` is `ITM_PLUS` (`assign.c:47` for C47, `:137`/`:181`/… for
the R47 layouts). ENTER inserts `+`.

**Reaching input, and NUMLOCK is reachable from inside the console.**
`fnForthOuter` opens clean (`forthEnterAimSurfaceNoLift` clears
`FLAG_NUMLOCK`, `forth_compile.c:1633`). Then: **f-ALPHA** into the
documented alpha excursion (row becomes `-MNU_ALPHA`, id 1922) →
**g-F2**, which is `menu_ALPHA` index 13 = `CHR_num`
(`softmenus.c:1017`, `items.h:2082` → item 2029). `CHR_num` is
`{SetSetting, JC_NL}` with `CAT_NONE` and param 0 (`items.c:3948`), so
**both** filters the interactive divert applies decline it —
`forthCapNameInsertEligible` requires `CAT_FNCT`
(`forth_capture.c:153-165`) and the fold gate requires
`TM_VALUE <= param <= TM_CMP` (`items.c:765`) — and the item falls
through to the real dispatch, `flags.c:824 fnFlipFlag(FLAG_NUMLOCK)`.
NUMLOCK is now set with the capture still OPEN and LIVE. Type `1 2`,
press ENTER. A probe driving only real dispatch functions printed:

```
PROBE: after console open calcMode=1 NL=0 live=1 menu=-213 keys=1
PROBE: after ITM_AIM menu=-1922 live=1 keys=0
PROBE: after CHR_num NL=1 live=1 open=1 calcMode=1
PROBE: after ENTER aim="1+" (want "1 ")
PROBE RESULT: ENTER DID NOT TYPE A SPACE
```

**Consequence.** `1 2 ENTER +` yields `12++`. There is no other way to
separate tokens while NUMLOCK is on, so the console becomes unusable in
the mode a user turns on precisely to type digits into an alpha editor —
and the hint line at the top of the transcript still reads
`ENTER=SPACE  R/S=RUN`, so the console advertises a control it no longer
performs. R/S then reports an unresolvable token on a line whose defect
is invisible on screen.

**Contract violated.** DESIGN.md §8.4.2, N-R10:

> **ENTER (N-R10: one token separator).** ENTER types a literal space
> into the line — `processAimInput(ITM_SPACE)`, metered by the same
> capture cap as any typed character

and DESIGN.md E12, which states numlock translation is *guarded off* for
the console — a guard that does not cover this new call site. The repo
already names the hazard in its own words at `test_capture.part.h:10456`
(`test_keys_eex_and_numlock`): *"The numlock translation table is keyed
on the AIM columns, so it must not rewrite the normal-column ids keys
mode feeds it."* The new call site feeds it an AIM-column id.

**Bug class.** *A control expressed as an item id, handed to a layer
whose job is rewriting item ids.* `processAimInput` is a key translator,
not an insertion API; every modifier table upstream owns sits between the
divert and the buffer.

**Class-level test.** A table over the console's two advertised controls
× {NUMLOCK off, on} × {keys plane, alpha plane}, asserting the **effect**
(one 0x20 byte appended / the line ran) rather than the item dispatched.
There is no test of the ENTER=space control at all today — `N-R10`,
"ENTER types" and the separator control return nothing in
`test_capture.part.h` — which is why the interaction was free to land.

---

### R11-IF-3 — R/S cannot run the line in the console's alpha excursion; it types `?`

`packages/forth-core/keyboard.c:3042` — **an advertised control that does
nothing, in half the documented input toggle.** High confidence,
mutation-proven blind spot.

**What breaks.** The guard is `forthCapInteractiveLive() && item ==
ITM_RS`. In the alpha excursion `keysMode` is false, so `determineItem`
takes the AIM plane (`keyboard.c:1790`, `:1815`) and key 84 resolves to
`primaryAim` = **`ITM_QUESTION_MARK`** in all ten layout tables
(`assign.c:46,136,181,226,271,316,406,451,498,545`). `item` is never
`ITM_RS`, the guard cannot fire, and the press falls to
`processAimInput`, whose DIRECT-LETTERS arm accepts `ITM_QUESTION_MARK`
and types `?` — which also sets `keyActionProcessed`, closing the
`btnReleased` escape hatch. f-shift gives `!`/`/`; g-shift, long press
(`keyboardTweak.c:404-413`) and NUMLOCK all give `/`. There is no softkey
route: the excursion's row is `-MNU_ALPHA`, and `keyboard.c:3043` is the
**only** production call site of `forthInteractiveRun` — the other twenty
are self-test fixtures calling it directly, which is exactly why the
suite cannot see this.

**Reaching input.** `FORTH` (console opens keys-first) → the ALPHA
gesture (f + the key whose normal-column `fShifted` is `ITM_AIM`; the
excursion is entered on key *release*, through `btnReleased`'s
`runFunction(ITM_AIM)` → `items.c:750`'s keysMode toggle) → type
`: SQ DUP * ;` → press R/S. The line is not run; a `?` is appended.

**Consequence.** In the alpha half of the console's own documented input
toggle there is **no run gesture at all**: R/S types `?`, ENTER types a
space, and the hint still reads `ENTER=SPACE  R/S=RUN` — as do
`README.md:13` and `:29`, this fork's front page. Recovery is one EXIT
(rung 2 unwinds the excursion), a backspace and R/S again; the verifier
was right that this is a missing gesture plus a stray character rather
than a true stuck state, and it is ranked here accordingly. **Before
N-R10 the state did not exist**: ENTER ran the line, and `ITM_ENTER`
resolves identically on both planes (`assign.c:22`), so the run gesture
was plane-independent. N-R10 traded a plane-independent control for a
plane-dependent one.

**Contract violated.** `STAGE_N_CONSOLE.md` N-R10 (`:241`) states it
unconditionally — *"R/S owns execution through `forthInteractiveRun()`
… reached from the `ITM_RS` guard at the head of the CM_AIM arm"* — with
no input-plane qualifier, and DESIGN.md §8.4.2 prints the hint
unconditionally. **The hazard was already on file**:
`STAGE_L_T8_pem_host_raw.md:77` — *"Keep the `determineItem` fix … and
extend it to `ITM_RS` if R/S is to mean 'run the line' rather than type
`?` (`src/c47/assign.c:45`, `primaryAim == ITM_QUESTION_MARK`)."* The
`determineItem` exception written for keys mode was never extended.

**The suite is blind, proven.** Mutating the guard to
`forthCapKeysMode() && forthCapInteractiveLive() && item == ITM_RS` —
i.e. encoding the broken behaviour as intent — leaves the **entire gate
green**. Every existing R/S test calls `processKeyAction(ITM_RS)`
directly, bypassing `determineItem`; `test_console_story` `[3]` even sets
`forthCapSetKeysMode(false)` and then runs the line by calling
`forthInteractiveRun()` itself, which is the shape of a test that asserts
the thing it performs.

**Bug class.** Same class as R11-IF-2: *a control bound to an item id
whose identity is plane- and modifier-dependent.* Two of the three worst
findings this round are this class; see §5.

**Class-level test.** The same two-plane × two-modifier table as
R11-IF-2, driven through `determineItem` from a **key code**, not from an
item, asserting the console ran the line. Any test that starts at
`processKeyAction(ITM_RS)` cannot see this class by construction.

---

### R11-IF-4 — the store fails its own ownership test when a kept native step is its only content, and the next push mints a second `LBL 'FHIST'`

`packages/forth-core/programming/forth_fold.c:530` (the predicate),
`:660` (the ensure that duplicates) — **stuck state, program-memory
litter.** Medium confidence on frequency, **probe-proven** on
consequence. Reached independently by a finder and by the verifier
assigned to kill a *different* FHIST finding.

**What breaks.** `_forthHistProgramConforms` returns
`sawForth || !sawOther`. `forthFoldEnter` creates the store
(`forthHistoryEnsure`) and materialises a capture step but pushes **no**
source line; if the TAM commit is unfoldable — `forthCapInsertName`
refuses when `bufLen + nameLen + lead + 1 < 256` fails
(`forth_menu.c:34`) — the splice sets `kept = n` and the native step
stays; `forthFoldLeave`'s sweep spares it and then unconditionally
deletes the capture step (`:1114-1118`). The body is now `LBL` +
`[STO 05]` + `END`: `sawForth` false, `sawOther` true, **the package's
own store fails its own ownership test**, and `forthHistoryEnsure`'s
single guard (`if(forthHistoryProgram() != 0) return true;`) appends
another `LBL 'FHIST'` + `END`.

**Reaching input — the suite already builds the state.**
`test_capture.part.h` subcase `[5]` ("oversize-text break", `:14444`)
does exactly this: `forthHistoryEnsure()` on a fresh fixture, an
interactive capture with a 193-glyph line, `STO 05` folded, asserting
only that `firstFreeProgramByte` grew. A probe inserted after its
`forthFoldPending()` check printed:

```
[5] PROBE: after kept-step fold, forthHistoryProgram()=0 numberOfPrograms=2
[5] PROBE: store DISOWNED — calling forthHistoryEnsure() again
[5] PROBE: after re-ensure, forthHistoryProgram()=3 numberOfPrograms=3 (was 2)
```

From the keyboard: put a ~250-byte string in X (`FORTH_SOURCE_MAX` is
256), press FORTH — `fnForthOuter` seeds `aimBuffer` in one act — then
STO and a register digit.

**Consequence.** The store the package created moments earlier is
orphaned with the owner's committed `STO` step inside it, unreachable
from the mechanism that wrote it; a third program is appended carrying a
duplicate `LBL 'FHIST'`; `XEQ 'FHIST'` and GTO resolve upstream's first
name match, which is the orphan. Every repetition leaks another one.

**Contract violated.** The predicate's own banner
(`forth_fold.c:507-513`) says it is *"defence in depth for raw program
memory restored from an older or damaged image, not an ownership rule for
a user's program"* — and here it refuses the store itself. DESIGN.md §8.1
(`:2103-2106`) carries, verbatim, the premise this state falsifies:

> No stronger test exists — kept native steps (an unfoldable TAM commit
> stays in the store, §8.4.3) **interleave arbitrarily with later lines
> and capture steps**, so step order carries no signal.

Interleaving is what makes the weak predicate safe. A kept step can be
the body's *only* content, because the fold deletes the capture step it
interleaved with and never pushes a line.

**Not the ruled residual.** The 2026-08-10 ruling (DESIGN.md §8.1,
DESIGN-HISTORY `:4279`, HANDOFF addendum 17) names one residual and it is
the opposite direction — the **false positive**, an owner program that
takes the reserved name *and* holds Forth source steps. The **false
negative**, the store failing its own test, is nowhere ruled and is
contradicted by the justification the docs give for the predicate's
weakness.

**Bug class.** *An ownership predicate over content the owner's own
lifecycle can empty.* The identity question the class asks (O-a: "what
makes this the same one, rather than a matching one?") was answered with
a content test whose subject can lose the content.

**Class-level test.** Enumerate the store's four body shapes — empty
(`LBL`+`END`), source-only, source + kept native, **kept-native-only** —
and assert for each that `forthHistoryProgram()` resolves it, that
`forthHistoryEnsure()` creates no second program, and that exactly one
global label spells `FHIST`. The fourth row fails today; the label-count
assertion is the one nothing in the suite makes.

---

### R11-IF-5 — `.S`'s spill picture has no oracle: order, values and separator position are all unasserted

`packages/forth-core/test_console.part.h:1487-1532` — **latent.** High
confidence, **three independent mutations green**. Found by the
lifecycle, arithmetic and tests dimensions with no contact.

**What breaks.** `test_console_print_stack_spill` drives
`XEQ 'CLSTK' 1 2 3 4 5 6 .S DROP DROP` and asserts exactly three things:
`memcmp(line, "<6>", 3) == 0`, `strstr(line, "|") != NULL`, and a
whitespace tokeniser counting 6 non-`|` tokens. None reads a value, a
side or a position. DESIGN.md §8.4.4 publishes the **complete expected
string for this exact input**:

> the register levels print top-down from X, then a `| ` separator, then
> the spilled values **deepest-last** — `<6> 6 5 4 3 | 2 1`

The design wrote the oracle; the test compares nothing to it.

**Mutations, executed at `868a991ab`, full gate GREEN in each:**

| mutation | printed line | gate |
|---|---|---|
| invert the peek index (`forthSpillPeekInto(spills - i, …)`), spacing preserved | `<6> 6 5 4 3 \| 1 2` — deepest-**first** | **GREEN**, test prints PASS |
| invert the register window (`REGISTER_X + (levels - 1 - i)`) | `<6> 3 4 5 6 \| 2 1` — bottom-up | **GREEN**, C7 passes too |
| emit `"\| "` before the register loop | `<6> \| 6 5 4 3 2 1` — boundary marking nothing | **GREEN**, test prints PASS |

**Correction the owner should carry**, because checking the finding as
first written would wrongly discredit it: the *illustrative* mutation
(reversing the loop to `for(i = 1; i <= spills; i++)`) **does** go red,
because the `if(i > 1)` separator is coupled to the loop direction and
the two spilled values fuse into one token (`<6> 6 5 4 3 | 12 `, caught
by the token count). The claim is right; that one recipe is not. Use the
index inversion.

**Second half — the documented arm has no test at all.** DESIGN.md
§8.4.4's *"A slot whose temporary allocation fails is skipped silently
while the depth prefix still counts it — the count is the truth, the
picture is best-effort"* is never entered: there is no allocation-failure
injection anywhere in the package (`grep` for
`allocFail`/`FORTH_TEST_ALLOC` returns nothing), so
`forth_inner.c:149`'s `return false` and `forth_prims.c:131`'s silent
skip are dead in test. Per the brief this is a legitimate coverage
finding and no correctness claim is made against the rule. Note that a
real skip omits that slot's separator too, so a genuine failure changes
the token count — and the loose end the rule does not name is
`spills == 1` with that one slot failing, which leaves the line ending in
a dangling `| `.

**Third half — fixture hygiene.** `:1496` clears `FLAG_SSIZE8` and never
restores it, while its immediate neighbour
`test_console_print_stack_depth` brackets the same flag (`:1428`/`:1475`);
and the fixture leaves four values on the stack. Nothing downstream
currently depends on either.

**Bug class.** *Green for the wrong reason* (architect R6): the pass is a
coincidence of unvalidated fixture data, not the guarded property. `.S`
is the **only** test surface for `forthSpillPeekInto` — the persist
battery's spill tests all walk the region through `forthSpillRefill` —
so this one weak oracle is the entire coverage of the function
`b5636f6c9` added.

**Class-level test.** Compare the whole line against the design's
published string for the driven input, in both stack sizes, plus a
no-spill row asserting the absence of `|`; and add a
`FORTH_DEBUG_SELFTEST` allocation-failure injection — the file already
has the precedent, `forthHistoryEnsureFailInjected` (`forth_fold.c:651`)
— to drive the documented skip and assert the `<n>` prefix still counts
the skipped slot.

---

### R11-IF-6 — the F9 residue pin can no longer fire: ENTER lost every path to FHIST, and its oracle was left behind

`packages/forth-core/test_capture.part.h:17357` — **latent.** High
confidence, **mutation green**.

**What breaks.** Subcase `[7]` of `test_fold_round6_window` drives a
mid-TAM suspended residue, sets `aimBuffer = "99"`, calls
`fnKeyEnter(NOPARAM)` and asserts `_tfcFhistStepCount()` did not change,
with the FAIL text *"ENTER in the residue committed TAM's scratch as a
Forth line"*. Since `d5f4811aa`, ENTER has no path to FHIST at all:
`forthHistoryPush` has exactly two call sites (`forth_fold.c:292` inside
`forthInteractiveRun`, `forth_console.c:340` inside the EXIT ladder) and
`fnKeyEnter` reaches neither. The oracle observes an event that can no
longer occur. `d5f4811aa` retargeted ~20 other `fnKeyEnter` drives to
`processKeyAction(ITM_RS)` for exactly this reason and missed this one.

**Mutation the pin exists to catch, executed.** Weaken the divert at
`keyboard.c:3682` from `forthCapInteractiveLive()` to
`forthCapIsInteractive()` — the LIVE/origin conflation F8/F9 were filed
for — and the **entire gate stays green**, printing a byte-identical
`[7] PASS (F8/F9): the residue is not live; EXIT recovers the line`.
Under the mutation ENTER inserts a space into TAM's scratch buffer:
`_forthCapAtCap` tests `InteractiveLive` itself so it returns false, the
insert happens, and the following EXIT-recovery assertions still pass
because `forthCaptureResume` restores `aimBuffer` from the fold's copy.

**Consequence.** The owner in a mid-TAM suspension (FORTH, a line typed,
STO pressed) presses ENTER expecting TAM to take the parameter and gets a
space silently inserted into TAM's scratch. The property DESIGN.md §8.4.2
states normatively — *"the divert … is **LIVE-gated**: a suspended
capture's `aimBuffer` belongs to TAM and keeps native ENTER"* — is
asserted nowhere in the suite.

**Bug class.** *The oracle outlived the mechanism it observed.* A wave
that changes what a key does must re-point every fixture that used that
key as a proxy for something else. The prior ruling on this very subcase,
R8-6 (`AUDIT_round8_in-family_2026-08-08.md:459`), minted the standing
test: *mutate each disjunct and require the fixture's output to change.*

**Class-level test.** For each of the two console diverts, mutate the
LIVE gate to the origin predicate and require a red. Concretely here:
assert the *effect* — `aimBuffer` unchanged after `fnKeyEnter` in the
residue — instead of an FHIST step count ENTER can no longer reach.

---

### R11-IF-7 — ENTER became a third capture-cap insertion seam and was not added to the sweep that enumerates them

`packages/forth-core/test_capture.part.h:7611` (the battery's "both
seams" enumeration), `:8037` (subcase `[7]`) — **latent.** Medium
confidence, **mutation green**.

**What breaks.** `keyboard.c:3683` added a third `_forthCapAtCap`
consumer. Subcase `[7]` drives seam 1 (`processKeyAction`, `:8068`) and
seam 2 (`executeFunction`, `:8093`) at 196 primed glyphs and asserts no
growth; it never presses ENTER. Deleting `!_forthCapAtCap(ITM_SPACE)`
leaves the full gate green, twice. The guard is load-bearing, not
decorative: an instrumented probe at 196 glyphs showed the first
`fnKeyEnter` growing `aimBuffer` 196→197 with the guard gone.

**Correction to the consequence as first written** — this is why it is
ranked here rather than higher. With a digit line (the battery's own `1`
priming) `bufferize.c`'s independent 1984 px width cap saturates growth
at **248 bytes, under `FORTH_SOURCE_MAX`**, so R/S runs the line
normally; only a narrow-glyph line (`.`, `:`, `i`, `l` at 5 px) reaches
322 bytes and trips the refusal. And when it trips, the owner *is* told:
`forthCheckSourceLine` (`forth_compile.c:1501`) displays
`ERROR_INPUT_TOO_LONG` ("Input is too long"), and `_forthConsoleActive()`
gates the console band off on `lastErrorCode == 0` precisely so error
text is not swallowed. The honest consequence is "a regression here
silently un-caps the line by up to ~126 glyphs, and in the narrow-glyph
case degrades R/S to a diagnosed refusal", not "an unrunnable line with
no explanation".

**Contract violated.** The battery header still enumerates the class as
two — *"the input cap holds on both insertion seams"* — as does subcase
`[7]`'s banner, and `keyboard.c`'s own seam-2 comment (*"the cap must be
re-checked at this second entry point too"*). The standing rule (Stan,
2026-08-04) is that a class test covers the class where the members are
enumerable; they are enumerable by `grep` and there are three
(`keyboard.c:1520`, `:3056`, `:3683`).

**Bug class.** *Enumerable-class test short by one member after a new
member landed* — the same shape the out-of-family leg's R11-1 reports on
the production side, and the shape the cap itself got right when it was
written.

**Class-level test.** Parameterise the existing subcase over all three
seams from one list, so a fourth seam fails the count rather than the
assertion.

---

### R11-IF-8 — the opening-refresh seam is pinned through its self-test wrapper, so deleting its production call site leaves the gate green

`packages/forth-core/test_console.part.h:1719` — **latent.** Medium
confidence, **mutation green**.

**What breaks.** `_forthConsolePrepareOpeningRefresh` has exactly one
production caller, `keyboard.c:1523`, immediately after
`runFunction(item)` in `executeFunction`.
`test_console_terminal_controls` calls
`forthTestConsolePrepareOpeningRefresh(ITM_FORTH)` — the
`#if defined(FORTH_DEBUG_SELFTEST)` wrapper around the same static
(`keyboard.c:69-70`) — directly, then asserts the mode word. Deleting the
production call site and re-running the gate: `FORTH SELF-TEST: ALL
PASSED`, upstream testSuite OK, **GREEN**. No test in the suite drives
`executeFunction` with `ITM_FORTH`; every `executeFunction` drive in the
corpus passes item 0 with a softkey data string.

**Contract violated.** This codebase has already named and paid for this
exact shape, in the caveat the consolidation wave condensed at
`test_console_capture_bits_survive_reopen`: *"The first draft of this
test 'covered' it by performing the save/restore in the test body and
then asserting the bits survived. That asserts the TEST's copy of the
block, not production's: reverting the production fix left the suite
green."* TESTING.md §1 is binding on the remedy: *"A test that stays
green under every named mutation is decoration."*

**The test's stated reason does not cover the gap.** Its comment
justifies not driving the whole physical key — *"assert the condition the
full screen observes without manufacturing a menu stack that leaks into
later Forth audit fixtures"* — which is a good reason not to drive a
menu, and not a reason to leave the call site unpinned. The sibling seam
already shows the codebase's own answer: a counter
(`forthTestConsoleRefreshCountGet`) that costs no menu stack.

**Why this one compounds.** The out-of-family leg's R11-1 is that this
seam is **missing at the other door**. A pin that cannot fail is why a
missing door went unnoticed: the wrapper made the seam look tested, and
the only thing it tested was the wrapper.

**Bug class.** *The test performs the production act itself.*

**Class-level test.** Increment a counter inside
`_forthConsolePrepareOpeningRefresh` and assert it from a drive through
the real dispatch — which, together with R11-1's two-row door table, is
one fixture covering both.

---

### R11-IF-9 — the opening-refresh hook deletes an upstream blank line instead of inserting additively, and both instruments are structurally blind to it

`packages/forth-core/keyboard.c:1523` /
`packages/forth-core/patches/010-keyboard.c.patch` — **merge tax.** High
confidence, verified mechanically for this report.

**What breaks.** `python3 tools/pkg_patch_refresh.py packages/forth-core`
regenerates the patch set byte-identically (tree stays clean), and
`grep -c '^-$' packages/forth-core/patches/010-keyboard.c.patch` returns
**1**, in hunk `@@ -1382,9 +1512,15 @@`:

```
                 runFunction(item);

-
+                _forthConsolePrepareOpeningRefresh(item);
                 // Double execution when a custom conversion: ...
```

Upstream `3de5b4be0:src/c47/keyboard.c:1386-1387` has **two** blank lines
after `runFunction(item);`; the override keeps one and puts the hook call
in the other's place. Landed by `8a3c6e146`.

**Both instruments miss it, for different structural reasons — verified
by running them.** `design-audit.sh` check C prints `none`: replaying its
own difflib call gives opcode `replace` (A=`['']`,
B=`['                _forthConsolePrepareOpeningRefresh(item);']`), while
`design-audit.sh:142`'s blank-line branch requires `tag == 'delete'` and
`:137`'s replace branch requires the pair to be rstrip-identical.
`patch_churn_scan.py` reports only the two catalogued `softmenus.c`
lines: at `patch_churn_scan.py:69-71` it does
`nd = norm_ws(d); if not nd: continue`, dropping a deleted blank line
before any tier is tested.

**Contract violated.** DESIGN.md §6 (`:1863`): *"Keep every override
byte-identical to upstream except the marked insertion; that is what
keeps the generated diff small and future upstream merges reviewable."*
Check C's own message: *"no-op churn — revert to upstream's exact
bytes."* The purely additive shape is available at zero cost — a verifier
rewrote it that way, refreshed, and the bare-`-` count went to 0 with the
patch wholly additive.

**Consequence.** On the next `--rebase-base`, any upstream edit to the
whitespace or first statement after `runFunction(item)` in
`executeFunction` collides on a line the package had no reason to own.

**Bug class.** *Churn the instruments cannot see.* The
deliberate-exceptions catalogue has no row for it, and no 2026-08-11
ruling covers it — `868a991ab` ruled the five non-Forth hunks the blanket
refresh swept in; this is a Forth hunk from `8a3c6e146`.

**Class-level test.** Teach check C the `replace`-with-empty-A case, or
teach the churn scanner not to discard a deleted blank; either makes the
class visible. A pin on `grep -c '^-$'` across the patch set, baselined
at the `lblGtoXeq.c` wholesale-deletion exception's count, does the same
job in one line.

---

### R11-IF-10 — an 82-line package block sits at the top of `keyboard.c`, 24 lines of it test-only, with no coupling to anything in that file

`packages/forth-core/keyboard.c:20-74` — **merge tax.** High confidence.

**What breaks.** `design-audit.sh` check D, run for this report:
`010-keyboard.c.patch: 82 lines @@ -2,12 +2,97 @@` — the largest
contiguous package block in any override, ahead of `manage.c`'s two
56-line blocks (the standing exception). Before this wave it was 35
lines; `8a3c6e146` added 54 — three `FORTH_DEBUG_SELFTEST` statics, three
exported test accessors, and the two seam functions.

**Nothing binds it to that translation unit.** Its only externals are
`refreshScreen()`, `screenUpdatingMode`, the `SCRUPD_*` defines
(`c47.h:447`, `defines.h:2029/2033`) and `forthCapInteractiveLive()` —
all global. The three accessors are reached cross-TU by plain `extern`
declarations at `test_console.part.h:1705-1707`, and that header is
included into `test_dict_reloc.c`, not into `keyboard.c`. Contrast the
design's own ruling when a block genuinely *is* coupled: P7 of
`SPEC_consolidation-wave_2026-08-09.md` moved the entire console view out
of the `screen.c` override and kept back exactly one function,
`_forthConsoleEditorTop`, with the reason written into the file — *"it
reads `checkHPoffset`, a screen.c-local macro … the one coupling that
cannot move."*

**The count-based guard cannot see it.** `BASE_BIG_BLOCKS` went 36→26 on
2026-08-11 and was accepted as *"a measured drop … the new ceiling"*,
whereas the 2026-08-09 note justified its 36 block by block (*"All 36
read: every block is either a seam arm that must run at that exact point
… or a comment-dominated call site"*). A block growing 35→82 inside a
falling count is invisible, and this block is neither a seam arm nor a
call site — it is definitions.

**One clause of the finding is refuted and is dropped here**: the
test-only half is not there "for no reason". Its existence is ruled in
DESIGN.md §8.4.4 (*"a full-screen refresh walks the live program cursor,
which the terminal tests that hand-build a line do not have"*). The
remedy is relocation to `forth_console.c` with a one-line call each,
never deletion.

**Contract violated.** DESIGN.md §6: overrides *"add the documented hook
lines and nothing else"*; check D's own flag text: *"new package logic is
being written INTO an upstream file."*

**Bug class.** *A count baseline re-accepted without re-reading its
members.* The 2026-08-09 pass read all 36 and wrote the movable one down
by name; the 2026-08-11 pass accepted a number.

**Class-level test.** Pin the *largest* block as well as the count, or
keep a per-block disposition file the way the deliberate-exceptions
catalogue does for churn.

---

## 4. PLAUSIBLE findings

### R11-IF-P1 — `forthFoldLeave`'s capture-step delete sits outside the `listsUnsafe` guard it feeds, so R10-P1's crash returns one statement later

`packages/forth-core/programming/forth_fold.c:1097`. Survived an intent
refutation; **the reaching input's last link — the allocator failure — is
unconstructed**, exactly as it was for R10-P1 and R9-P1, so it is filed
here rather than in §3.

**The shape.** The sweep at `:1067-1080` flags `listsUnsafe` on a
`deleteStepsFromTo` error. Control then reaches `:1095`:
`cap = listsUnsafe ? NULL : _forthFoldResolveCaptureStep()`, and `:1097`
`deleteStepsFromTo(cap, findNextStep(cap))`. That delete calls
`scanLabelsAndPrograms`, which frees `labelList`/`programList` up front
and returns early without reallocating if `allocC47Blocks` fails
(`manage.c:151-163`), leaving `numberOfPrograms` non-zero. **`listsUnsafe`
is never updated after `:1097` and `lastErrorCode` is never consulted**,
so the guard at `:1110` still reads false, `_forthRestoreCursorTuple`
runs, its `numberOfPrograms == 0` bail cannot fire, and
`goToPgmStep(program, 1)` dereferences `programList[program-1].step`.
Same trace as R10-P1, one delete later. Owner-visible: press STO from the
console under memory pressure and the calculator faults and reboots
instead of showing the RAM FULL that had just been raised.

**Why it is not refuted.** The rule is stated in three places and excused
nowhere: the fix's own comment at `:1106-1109` (*"the guard covers every
list consumer after the sweep, not only the resolve"*), `ea210c9ba`'s
commit message (*"the L1-H convention now holds at every list consumer
after the sweep"*), and DESIGN-HISTORY `:4269` (*"R10-P1 — the abandon
rule at every consumer"*). Round 9's R9-P1 offered three ways to settle
the family — fault injection, an owner ruling that L1-H governs
regardless of allocator reachability, or a proof pinned at the allocator
that free-then-smaller-alloc cannot fail. The project took the guard and
never took the proof, so no ruling exempts this site.

**What would settle it.** The P-2 fault-injection hook on
`allocC47Blocks` the record has owed since round 9 (HANDOFF `:1340`). The
precedent for the shape is now in the same file —
`forthHistoryEnsureFailInjected` (`forth_fold.c:651`) is a
`FORTH_DEBUG_SELFTEST` failure switch on a function nobody could make
fail otherwise. One hook settles this finding, the refuted
`forthSpillRefill` ordering in §6b, and R10-P1's own residual, and it
converts a recurring PLAUSIBLE family into red-or-green.

---

## 5. Design observations (D7)

**D7-a. The two new controls are items, and item identity is not
stable.** R11-IF-2 and R11-IF-3 are one shape. A control is an *action*,
but both diverts express it as an item id and then hand it to — or
receive it from — native layers whose entire job is rewriting item ids:
`determineItem` resolves by input plane, `keyReplacements` rewrites by
NUMLOCK, `caseReplacements` by CAPS, `Check_MultiPresses` by press
duration. `ITM_ENTER` survived all of them (identical on both planes, in
no replacement table), which is exactly why the pre-N-R10 design worked
and why nobody noticed the property was load-bearing. The rule this
suggests: **a console control must be resolved by the console, from a key
code, before the native tables see it** — or at minimum every divert must
state which tables it has cleared. The cap seams already got this right
by enumerating their doors in comments; the controls got the mechanism
right and the enumeration wrong.

**D7-b. FHIST ownership by content predicate leaves a residue the design
priced but the text still contradicts.** DESIGN.md §8.1 opens *"**One**
kept, named, runnable program"*, and after any collision there are two
programs named FHIST and the store is not runnable by its name. That
outcome is **ruled** — the 2026-08-10 ruling closed adoption, subcase
`[13]` asserts the two-program state as PASS, and creation-time refusal
is impossible because programs arrive by restore. But one verifier graded
the duplicate a finding on the reachability lens before finding the
ruling, which is fair evidence that the §8.1 sentence and the ruled
behaviour do not read as the same statement. The cheap repair is one
sentence in §8.1 stating what a collision leaves behind and that
`XEQ 'FHIST'` then reaches the owner's program, not the store. The
expensive repair — a stamp — was considered and rejected in writing; this
observation does not reopen it.

**D7-c. The spill record's layout is spelled three times in one file with
nothing forcing agreement.** `forth_inner.c:85` writes it, `:113` walks
it to refill, `:139` walks it to peek; the header arithmetic
(`6u + blocks*4u`) is duplicated at all three, and the tag omission is at
two of the three. R11-IF-1 is what "two places that must agree and
nothing forcing them" costs when the third place is added by a different
packet a stage later. A single `_spillSlotAt(index)` accessor returning a
struct would make the layout one statement instead of three.

**D7-d. The self-test seams are becoming a shadow API inside an upstream
override.** Three accessors, one wrapper, three statics, all
`FORTH_DEBUG_SELFTEST`, all inside `keyboard.c` (R11-IF-10) — and one of
them is the *only* thing a fixture drives (R11-IF-8). A seam that exists
so a test can observe production is sound; a seam the test drives
*instead of* production is the thing this project has already convicted
once. Worth writing into TESTING.md as a rule: **a `FORTH_SELFTEST_EXPORT`
wrapper may be an observer, never the subject.**

**D7-e. Instruments that count without reading lose the property the
count stood for.** Check D's baseline was justified member by member in
2026-08-09 and re-accepted as a number in 2026-08-11, and its largest
member doubled in between. Check C and the churn scanner both classify by
opcode/emptiness and cannot see a consumed blank line. Same failure in
both: a mechanical check's *baseline note* is the load-bearing part, and
a re-accept that drops the note keeps the number and loses the guarantee.

**D7-f. The regression pattern broke this round, and the reason is
instructive.** The record said each round's findings come mostly from the
previous round's fixes (r2 4/7, r3 4/4, r10 10/11). This round: **one**
finding in round 10's fix wave, **nine** in the wave that landed
undocumented on 2026-08-10, **zero** in the comment sweep. Round 10's
fixes came back clean under deliberate attack — the relocated
`listsUnsafe` scope is correctly a compound statement rather than an
`if`, the eviction renumbering thresholds are right, the zeroth-step
restore is right. The pattern's real content is not "fixes are
dangerous"; it is **"unaudited code is dangerous, and a fix wave is
usually the only unaudited code in range."** When a whole feature wave
joins the range, it takes the findings.

---

## 6. Deliberately not flagged

### 6a. The cross-leg disagreement, recorded because it went the other way

**The in-family refutation of "the opening-refresh seam is wired at one
of the two doors" was WRONG, and the out-of-family leg proved it with a
screenshot.** The finding is `R11-1` in
`AUDIT_round11_out-of-family_2026-08-11.md`; it is not re-reported here,
but this leg's record must say that its own verifier killed it on two bad
grounds:

1. The verifier refuted the finder's stated bridge to the physical-key
   path (`US_ENABLED` read as an assignability bit — it is the zero value
   of the *undo* class, `defines.h:1108`, and the verifier was right
   about that) and then concluded no such path was established. The real
   path is ASSIGN → FCNS → FORTH, `_typeOfFunction`'s `default: return 4`
   (`assign.c:924`) and `assignToKey`'s `default:` arm — which the
   out-of-family leg constructed and the in-family verifier never tried.
2. The verifier leaned on the seam's comment naming *"the preceding
   menu"* as the carrier of the suppression bit, and correctly showed
   `menuUp`/`menuDown`'s `SCRUPD_SKIP_STACK_ONE_TIME` is cleared at
   `keyboard.c:2511`. It missed the sticky carrier: `RETURN_NORMAL` ORs
   `SCRUPD_MANUAL_STACK` (0x02) as the last act of every normal refresh,
   and `SCRUPD_ONE_TIME_FLAGS` is 0xf0, so it is **not** cleared.

The out-of-family leg then captured three frames: same open, same ring,
band ink 248 px through the key door (no hint on the glass) versus 322 px
through the softkey door with `ENTER=SPACE  R/S=RUN` legible. Two of this
leg's dimension finders also reached the same site and both
self-suppressed it as "reachability from the write-set" — the correct
instinct applied to a path that turned out to be real. **The lesson is
the one the process already states and this round paid for again: an
unconstructed path is not a refuted path, and the way to settle it is to
construct it, not to reason about the flags.**

### 6b. The six findings the refutation pass killed, and why

1. **`.S` on a spilled short integer trips C47's bug screen (crash
   class).** Refuted on a constant. The claim was that `amNone` is base
   0; `amNone` is **5** (`typeDefinitions.h:228`, in the enum where
   `amRadian = 0`), and `shortIntegerToDisplayString`'s guard is
   `if(base <= 1 || base >= 17)` (`display.c:2016`). 5 passes. No bug
   screen, no `CM_BUG_ON_SCREEN`, no torn console — every step of the
   consequence was downstream of a branch that is not taken. The
   *mechanism* the finder saw is real and is R11-IF-1; the crash is not.
2. **`_forthConsolePrepareOpeningRefresh` wired at one of two doors.**
   Refuted in-family — see 6a. **Overturned out-of-family; treat it as
   confirmed (R11-1).**
3. **`forthSpillRefill`'s allocation-failure return leaves a NULL data
   pointer and a stale type.** Refuted on reachability. The finder's own
   input reaches the site in the free-then-**equal** shape (both spilled
   slots and the refill target are single-limb long integers), which the
   carried ruling already covers: `freeListAlloc` is exact-match-first
   over a merge-on-free list and re-serves unconditionally. A genuine
   free-then-**larger** shape exists (pure-prim growth,
   `2 2 * 2 * 2 * …`, then three pushes to walk the big value to T), but
   `forthSpillCatch` allocates ~B+33 blocks while the B-block original is
   still live and the following `liftStack` hands those B blocks back, so
   the request is short only inside a ~4-block window with a specific
   fragmentation shape. Constructed path, unconstructed branch →
   refuted; same §4 family as R11-IF-P1, settled by the same hook.
4. **Subcase `[7]`'s row/sub-mode agreement assertion is switched off by
   the state a regression would change.** Refuted by mutation. The
   finding's own post-gesture state is self-contradictory —
   `keysMode == false` with `currentMenu() == -MNU_ALPHA` is the state
   where row and sub-mode **agree** — and its mutation target does not
   exist (`forthConsoleHomeRow` only *reads* keysMode). Inserting the
   state it describes turned the gate **RED** in two other subcases
   (`[6] FAIL (F7)`, `[3] FAIL (C-2)`). The deleted disjunct was already
   vacuous before the rewrite. A banner-wording nit at most.
5. **The FHIST ownership test is the wrong shape; it should be a stamp.**
   Refuted on intent: the stamp was considered and rejected in writing on
   2026-08-10 (*"creation-time refusal cannot close the door …
   resolution-time ownership is the enforceable half"*), a stricter
   content predicate was drafted and went red on the suite's own
   kept-step pattern, and duplicate-rather-than-adopt is the asserted
   PASS of subcase `[13]`. The verifier that refuted it nevertheless
   surfaced the one unruled residual independently of the finder who
   filed it — that is R11-IF-4.
6. **The ENTER divert re-implements upstream's CM_AIM typing sequence
   inline instead of calling a seam.** Refuted by experiment. The finding
   traded on an asymmetry ("the context copy conflicts loudly, the
   package copy does not"); simulating upstream moving that sequence and
   running the resolver's own step (`git apply -3`) applied **cleanly,
   zero conflict markers** — neither copy conflicts, because the package
   never edited upstream's typing arm and a region only one side touched
   never conflicts. The proposed remedy is inert too: `files/` sources
   are copied verbatim and never diffed against upstream. The recorded
   technique for buying loudness is the *opposite* move (H2 deletes
   upstream's superseded `_executeOp` so edits there conflict on
   purpose), and §6's byte-identical rule is satisfied — the hunk is a
   pure 13-line insertion with zero upstream lines altered.

### 6c. Attacked and cleared, with the reasoning (union of the eight dimensions)

**The round-10 fixes themselves.** The `listsUnsafe` relocation moved the
restore into the outer `{ uint8_t *cap; bool_t listsUnsafe; }` *compound
statement*, not into `if(cap != NULL)`, so a missing capture step still
restores the cursor exactly as the banner promises — checked brace by
brace by three dimensions because "relocating state is the most dangerous
fix shape". The eviction renumbering's `> 2` thresholds are right (local
step 1 is the LBL, the evicted step is always local step 2, the successor
inherits its number), the `savedProgram == program` gate correctly leaves
other programs' cursors alone, and running the renumber *before* the
`lastErrorCode` check is right because `deleteStepsFromTo` has already
deleted by then. `_forthRestoreCursorTuple`'s widened `localStep >= 1`
arm is non-falsifiable but harmless. `forthConsoleHomeRow`'s new nested
drop loop is bounded twice by `SOFTMENU_STACK_SIZE`, uses `xcopy`
(memmove-safe, `charString.c:1206`), and cannot hang.

**`.S`'s `TEMP_REGISTER_1` borrow** — the item the eighteenth addendum
flagged for this round — cleared on five separate grounds: the saved raw
pointer cannot go stale because C47's allocator is a non-moving free list
(`memory.c:76` → `freeListAlloc`, no compaction); the per-slot free
re-derives its size through `getRegisterFullSizeInBlocks` from the type
the peek just wrote, the same expression `forthSpillCatch` recorded
`blocks` with; the failure arm returns before `setRegisterDataPointer`,
so the restore is always correct; the pointer/type/tag triple is the
complete descriptor (`readOnly` is always 0 for a temp register); and no
formatter reachable from `forthConsoleFormatRegister` touches
`TEMP_REGISTER_1` (`display.c` has zero references; `dateTime.c`'s only
use is off the display path). The borrow is the *correct* shape four
lines from the incorrect one — which is what made R11-IF-1 legible.

**The `FORTH_DEBUG_SELFTEST` refresh suppression** returns early for
*any* live interactive capture, armed or not, so the `return` is not
gated by `forthTestSuppressConsoleRefresh` and `refreshScreen(142)` never
runs in a test build. The comment overstates the guard's narrowness by a
clause, but DESIGN.md §8.4.4 states the seam and its limits (*"they
assert the REQUEST and the mode word, never the pixels. Normal firmware
always falls through to `refreshScreen()`"*), the macro is defined only
by `build-test.sh`, and the out-of-family leg proved by mutation that
handing those tests a real full refresh **segfaults the harness**.
Deliberate, and the alternative is worse.

**`_forthCapAtCap(ITM_SPACE)` meters the right string.** `ITM_SPACE`'s
`func` **is** `addItemToBuffer` (`items.c:2707`) and its
`itemSoftmenuName` is `STD_SPACE` (1 byte) — the empty string in that row
is `itemCatalogName`, a field-order trap two dimensions checked
independently. So the cap is a real measurement and
`forthInteractiveRun`'s `char preRunCopy[256]` cannot overflow. One
genuine undercount exists — `convertItemToSubOrSup` rewrites a letter to
a 2-byte glyph *after* the cap measured the 1-byte original — and it is
closed by `forthCheckSourceLine`'s `n >= FORTH_SOURCE_MAX` refusal before
the copy. The one-byte accounting slip under NUMLOCK (`ITM_SPACE`'s name
metered, `ITM_PLUS` inserted) is subsumed by R11-IF-2.

**The two keys exempted from `processKeyAction`'s error sweep**
(`ITM_EXIT1`, `ITM_BACKSPACE`) each have a paired `lastErrorCode` clear
for a live console — `forth_console.c:244-249` (the ladder's
error-dismiss pre-rung) and `keyboard.c:4538-4546`. Nothing reads a stale
code: the console paint gate requires `lastErrorCode == 0` and yields to
the landed register paint, which is where the message belongs.

**`forthHistoryEvict` renumbers `_forthHistCur` but not `forthFoldCtx`**,
two byte-identical cursor tuples with the deleter-adjusts convention
applied to one of them. Unreachable in both directions:
`forthHistoryPush`'s two callers both require
`forthCapInteractiveLive()`, which is mutually exclusive with a pending
fold (the fold suspends), and no program deletion can occur between save
and restore inside one push. A pin, not a finding.

**`forthHistoryEnsure`'s unguarded `_forthHistRestoreCursor` after two
inserts**, and the same shape at `forthCapRecommitStep` and the resume
splice, cleared under the upstream-convention-first ruling: upstream's own
`_insertInProgram` calls `scanLabelsAndPrograms()` and then
`goToGlobalStep()` with no guard (`manage.c:775-777`), so a package-side
guard at those sites would not close the window. That is an upstream
report, not a package finding.

**The 512-step guards** in `_forthHistProgramBytes` (`guard++ < 512`) and
`_forthHistProgramConforms` (`if(guard > 512)`) disagree by one and both
degrade to "stop". Reaching it needs ~512 one- or two-byte kept native
steps inside a 1 KB byte cap, and every ordinary source step is ≥ 5
bytes, capping the store near 200. Unreached, recorded as the exact
boundary a future cap change would trip. `_forthHistLastLineStep` is the
one FHIST walker with no step cap; only corrupt program memory reaches
it.

**`forthConsoleFormatRegister`'s final clamp** cuts on a byte boundary
unlike every other copy in the module. Not a finding today: every caller
passes exactly `FORTH_CONSOLE_FMT_MAX`, so the clamp is unreachable. It
becomes a C11-class orphan-lead-byte bug the moment anyone passes a
smaller buffer.

**`T_cursorPos = stringLastGlyph(aimBuffer) + 1`** lands mid-glyph when a
line ends in a two-byte glyph, at three sites. It self-corrects:
upstream's insertion loop (`bufferize.c:602-607`) advances glyph-wise
until the byte index reaches or passes `T_cursorPos` and rewrites it.
Pre-range and benign.

**The control hint is appended on every open**, so a close/reopen cycle
stacks hint lines in the transcript. §8.4.4 rules it (*"an ordinary ring
record — it rolls, evicts and clears like any other line"*), and
`fnForthOuter`'s already-open guard stops a duplicate on a second FORTH
press.

**A console line entering `CM_CONFIRMATION`** (CLREGS / CLVall / DELVall
/ CLFALL are `CAT_FNCT|PTP_NONE` and resolve as Forth words) — refuted
for the second time: the Forth dispatch passes `NOPARAM`, and those
functions arm the confirmation only when called with `NOT_CONFIRMED`.

**A Forth line reaching `enterAsmMode` and clearing `FLAG_ALPHA` with
`calcMode` still CM_AIM** — the shared-guard shape from round 3. Only
reachable by opening a catalog, and catalog menus are `CAT_MENU` with
negative ids, which §4.1's filter excludes. Unreached, not asserted.

**`screen.c`'s `fcol`/`frow` revert (`868a991ab`)** — ruled, filed as
`UPSTREAM_REPORTS_getGlyphBounds_partial.md`, and unreachable at this
call site anyway: `clearRect` passes `" "`, which `standardFont` always
contains, so the callee writes both out-parameters on every reachable
call. The `5412a6992` override removals are complete (no
`lcd_line_addr`/`printLcd` reference survives anywhere) and correctly
re-baselined at 19/19. The `res/` reverts and the `R47_combo` short-tag
`IndexError` are ruled with the boundary written down.

**The `testSuite.c` hardenings kept by `868a991ab`** —
`char str[sizeof(real) + sizeof(imag) + 6]` is sound (`real`/`imag` are
`char[2000]` arrays in scope, not pointers) and the
`while(fgets(...) != NULL)` rewrite is behaviour-identical to upstream's
read-ahead except on a read error, which is the defect it fixes.

**The gate instruments repaired in `8010d8c8a`** were attacked and hold:
`pin()`'s `2>/dev/null` swallow is unreachable because both `pin 0` rows
end in `wc -l` / `awk … print n+0`; check J's lost `|| true` is harmless
under `set -uo pipefail` without `errexit`; the new awk FHIST-span pin
fails loudly (0 against an expected 1) rather than silently; check H's
comment-stripping repair is correct for the class it targets.

**Test-side items cleared:** the `forthTestConsoleRefreshModeGet()` mask
assertion looks always-false but pins the clearing statement one line
above it; subcase `[11]`'s `after >= before + 1` eviction guard can
mis-attribute but still goes red; the dead locals the refresh deleted
from `test_params.part.h` had their assertions preserved two lines below;
subcase `[13]`'s banner claims eviction it does not drive, but the claim
is transitive through `forthHistoryProgram()`; `_consoleLineIs(0,
FORTH_CONSOLE_CONTROL_HINT)` compares against the same macro production
writes, but its real content — the hint is line 0 and there is exactly
one line — is proved, and the behaviours it advertises are asserted
separately.

**Excluded by ruling, not re-litigated:** the `softmenus.c` TIMER-guard
re-indent; the `.S` best-effort skip as a correctness question; round
10's findings and carried rulings (P1, P2's push ruling, C22-vs-C1);
rounds 1–9's refuted items.

### 6d. If the goal were correct code rather than a passing audit

**Fix R11-IF-1, R11-IF-2, R11-IF-3 and R11-IF-4.** The first is a silent
wrong answer, the next two are advertised controls that do not work, and
the fourth litters program memory with orphans the owner cannot clean up
by name.

**Fix R11-IF-5 and R11-IF-6**, cheaply, because they are the pins that
would have caught the others: one full-line assertion against the string
DESIGN.md already publishes, and one effect-based oracle where a dead
proxy sits now.

**Leave R11-IF-7, R11-IF-8, R11-IF-9 and R11-IF-10 alone** if the goal is
correct code. R11-IF-7 costs the owner nothing today and its worst case
is a diagnosed refusal. R11-IF-8 is a real dead pin, but its practical
content is already carried by R11-1's fix, which will pin the seam at
both doors. R11-IF-9 and R11-IF-10 are merge tax on a rebase that has not
been scheduled; they are cheap now and expensive later, which is an
argument for doing them in the next package sweep, not for doing them as
bug fixes. R11-IF-P1 should be closed by the fault-injection hook the
record already owes, not by a guard added on speculation.

---

## 7. Verdict

**Not as it stands** — and this leg's answer differs from the
out-of-family leg's "yes, ship it", because that leg's six findings did
not include an arithmetic one and this leg's ten do. v0.3 is already
published, so the practical question is not whether to ship but what a
user hits first.

**Where it breaks first, in the order a real user meets it:**

1. **The first time a line spills.** `.S` after five values on the
   default stack, or any nine-value line, and a negative number becomes
   positive with no error. This is the one defect in the stage that makes
   the calculator lie about arithmetic, and the owner's only signal is
   that the answer is wrong.
2. **The first time NUMLOCK is on** — which is the first time the owner
   wants digits in the alpha half of the console. ENTER stops separating
   tokens and starts typing `+`, while the hint line keeps promising
   otherwise.
3. **The first R/S in the alpha excursion.** No run gesture exists there
   at all; the key types `?`.
4. **The first unfoldable TAM commit on a long line.** One orphan FHIST
   per occurrence, silently.

**The structural risk worth watching** is D7-a: this package now binds
user-facing controls to item ids and lets upstream's key-translation
stack resolve them. That stack rewrites items by plane, by shift, by
NUMLOCK, by CAPS and by press duration; it was written for a calculator
keypad, not a terminal. Every future console control faces the same five
tables, and the tests as written cannot see any of them, because every
console test in the suite starts at `processKeyAction(<item>)` or calls
the orchestrator directly — **the harness enters below the layer where
the bugs are.** The out-of-family leg reached the same observation from
the other side ("the test surface is shaping which door the production
code defends"), independently, and it is the most useful thing this round
produced.

---

## 8. Round and exit state

**Round 11, in-family leg**, at `868a991ab` on `forth-core/stage-n`.
Range `8010d8c8a~1..HEAD`, 21 commits. Readers: eight dimension finders
via `audit-workflow.js`, blind to each other; sixteen refutation runs in
isolated worktrees with named lenses, fourteen proving their verdict by
executed mutation or probe. The out-of-family leg ran separately (Gemini
3.1 Pro, two self-contained packets) and is
`AUDIT_round11_out-of-family_2026-08-11.md`: one CONFIRMED (R11-1), five
refuted.

**Round total: eleven CONFIRMED (ten here plus R11-1), one PLAUSIBLE.**

**Exit criterion NOT met; the count stays at zero.** Round 12 audits
round 11's fix wave — new code touching the key surface, the spill record
and the FHIST predicate, three of the four areas that produced findings
here — and is the earliest round that can begin the count. Earliest close
is round 13, with at least one of the two clean rounds out-of-family.

**Independent agreement worth recording** — two readers, no contact, same
defect, which is the evidence the process exists to produce:

- R11-IF-1 found by the *arithmetic* and *design* dimensions
  independently, with different reaching inputs and the same root.
- R11-IF-5 found by *lifecycle*, *arithmetic* and *tests* independently;
  three verifiers mutated it three different ways and all three stayed
  green.
- R11-IF-2 reported by *contracts*, *guards* and *arithmetic*.
- R11-IF-4's unruled residual reached by a finder and, separately, by the
  verifier assigned to kill a *different* FHIST finding.

**Owed into round 12, from this leg's own gaps:**

1. **The allocator fault-injection hook (P-2).** Owed since round 9. It
   is now the sole blocker on three findings across three rounds (R9-P1,
   R10-P1, R11-IF-P1) and on the refuted `forthSpillRefill` ordering. The
   pattern for it already exists in the same file
   (`forthHistoryEnsureFailInjected`).
2. **A simulator pass on the two control diverts.** This leg drove
   everything through the self-test harness, which enters *below*
   `determineItem` — the exact layer R11-IF-2 and R11-IF-3 live in. A
   `run-sim` capture of ENTER-with-NUMLOCK and of R/S in the alpha
   excursion turns two traced findings into photographed ones, and it is
   the same gap the out-of-family leg closed for R11-1 the same day.
3. **Out-of-family coverage of the rest of the range.** Fourteen of the
   21 commits were never in a packet; the `.S` spill change, the FHIST
   reservation and round 10's cursor fixes rest on in-family reading
   alone.
4. **A runner fix for the shared scratchpad.** Round 5 isolated the
   trees; the logs are still shared, and four verifiers this round spent
   budget on a contamination that was not one. Worktree-unique log paths
   in `audit-workflow.js`, alongside the `isolation: 'worktree'` it
   already passes.
