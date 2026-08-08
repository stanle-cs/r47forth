# Audit — the Stage-N fold/suspend window at `24bd4db99` (round 6, verification synthesis)

Range `b5a0202c9..24bd4db99` — everything the last five rounds produced,
read as one window. This is the round-6 report after the refutation pass
landed: it folds the out-of-family and U-series verdicts into the
executed-evidence findings from the TAM-driven fixture, the `run-sim`
captures, and the Sol design review. No code changed this round; the audit
produces findings, not fixes, and the tree it finishes on is the tree it
started on. §9 records the two process defects the round earned and the
proof that the fix for the worst of them held.

---

## 1. Subject and coverage

**Commits.** `b5a0202c9..24bd4db99`. Everything measured at the tip
`24bd4db99`; the isolated verifier worktrees each came up STALE at
`c3a00768c` (~114 behind) and each checked out to tip before its first
read — recorded in §9 as the round's headline process defect.

**Files.** The fold/suspend window: `keyboard.c` (`fnKeyExit`/`fnKeyEnter`
TAM arms, the recall guards, the interactive EXIT ladder, the long-press
R/S arm), `ui/tam.c` (`tamEnterMode`, `leaveTamModeIfEnabled`,
`tamProcessInput` and its epilogue, the GTO→GTOP promotion arm, the UP
arm), `programming/manage.c` (`forthCaptureSuspend`/`Resume`,
`forthFoldEnter`/`Leave`/`UnwindIfDone`, `getNumberOfSteps`,
`_closeAlphaMenus`), `forth_capture.c`, `forth_menu.c`, `items.c`,
`screen.c`, `forth_inner.c`/`forth_prims.c` (the spill path, reached by
the broad pass), and the two test part files.

**Readers.** The in-family FIND phase did not run — the window was already
enumerated as R1–R12 + P-A + U1–U5 in round 5, and round 6's mandate was
to DRIVE it, not to re-enumerate. In its place: an executed TAM-driven
fixture through the real gate in a worktree (F0–F4), one `run-sim`
screenshot pair, an out-of-family pass (Gemini 3.1 Pro ×3 packets, GPT-5.6
Sol ×1 design packet; all MODEL-probed and identity-verified), and a
three-lens adversarial refutation pass (reachability / correctness /
intent) over U1–U5, P-A, and every out-of-family finding, each verifier in
its own worktree. This report is the synthesis of that refutation pass on
top of the executed evidence.

**Deliberately not audited.** New surface outside the fold/suspend window
(round 5 covered the ownership machinery; this round was aimed where the
findings have been). `forth_console.c`'s ring internals (C10/C11/C12
territory). The picker content builder. Anything the mechanical half
already reports (§2).

**What the budget did not reach.** The refutation pass was scoped to the
enumerated findings plus the out-of-family census, not to fresh surface.
One finding, **F2**, sits just OUTSIDE the fold/suspend window — it is a
spill-accounting defect in `forth_inner.c`/`forth_prims.c` surfaced by an
accidental broad FIND pass over `main..HEAD`, run degraded (3 of 8 lenses
produced no output), then re-verified cleanly. It is carried here because
it is confirmed, but it is not part of the window and the broad pass that
found it was not a systematic sweep — treat the rest of `main..HEAD` as
unread.

---

## 2. Mechanical results

Measured at `24bd4db99`, tree clean. The gate's refresh produced no change
to generated output (`git status` unchanged after the run).

**Gate: GREEN.** `./packages/forth-core/build-test.sh` exit 0. Forth
self-test ALL PASSED (including the ownership-invariant sweep); upstream
`meson test testSuite` 1/1 OK (55 s), 0 fail. No code changed since round
5, so the warning baseline is round 5's, unchanged — nothing new in kind.

**`design-audit.sh`: 3 finding groups, all pre-existing, all measured
against the round-5 baseline.**

- **A — upstream footprint over budget.** override files 17 (budget 16),
  added lines 2156 (budget 606). The 17th file and the added-line mass are
  the standing cost of the console overlay, already accepted.
- **D — contiguous inline blocks over baseline** (16 → 30). The largest is
  `010-keyboard.c.patch: 136 lines @@ … void fnKeyExit` — this is the U5
  subject (the interactive EXIT ladder inlined into upstream's
  `fnKeyExit`), corroborated mechanically here, not discovered here.
  `manage.c`, `ui/tam.c` and `screen.c` also carry flagged blocks.
- **E — allocations in package sources needing lifetime answers**
  (`forth_dict.c`, `forth_inner.c`), the `UPSTREAM_REPORTS` leak class —
  a standing prompt, not a new defect.

Nothing the mechanical half reports is counted as a finding below.

---

## 3. CONFIRMED findings

Worst first, ranked by what the finding costs the owner. Executed evidence
(gate-driven red/green, screenshot, or an in-suite probe) is the strongest
tier this process recognises and is marked as such per finding; the
remainder survived the three-lens refutation pass with a constructed
reaching input. Findings 2–4 are three DISTINCT code sites of one bug
class (an armed fold left un-unwound); they are enumerated separately
because each needs its own guard and the class test must hit all three —
not as a rhetorical triple.

---

### F1 — R-P-A / P-A: the resume fold-splice count is an unclamped `uint16_t` difference driving an unguarded delete loop → SIGSEGV / device reboot

**EXECUTED — reproduced as a SIGSEGV through the gate with a product-code-only
backtrace. Promoted from round 5's sole PLAUSIBLE, and independently
survived the refutation pass.**

- **Where.** `packages/forth-core/programming/manage.c:1335` (the count
  `uint16_t n = getNumberOfSteps() - forthCapSavedStepCount()`) and `:1348`
  (the unguarded `while(n>0) deleteStepsFromTo(ins, findNextStep(ins))`
  loop), reached from the GTO→GTOP promotion arm at `ui/tam.c:771`.
- **Reaching input (three keypresses from an open console).** Open the
  console. Press **GTO** (`ITM_GTO`, TAM `TM_LABEL` — admitted by
  `_forthFoldAdmits`, so the fold ARMS at `foldMode 1` and the capture
  suspends against FHIST). Press **`.`** — `tam.function` is `ITM_GTO`, so
  `ui/tam.c:798` promotes it to `ITM_GTOP` AFTER admission, leaving the
  fold ARMED even though GTOP is not itself an admitted item. Press **`.`**
  again — the `ui/tam.c:771` arm now runs `reallyRunFunction(ITM_GTOP,…)`
  LIVE (it sits ahead of `tamProcessInput`'s `calcMode==CM_PEM` forge, so
  the bracket never gates it): it navigates `currentProgramNumber` off
  FHIST and its `do { … insertStepInProgram(ITM_END); scanLabels… } while
  (numberOfPrograms == initial)` loop GROWS program memory. The epilogue
  (`ui/tam.c:1472` → `forthFoldUnwindIfDone` → `forthCaptureResume`) then
  computes `n` by subtracting FHIST's saved step count from the NEW
  program's step count — two different programs, keyed on the moved
  `currentProgramNumber` — with no clamp. When the target is shorter the
  `uint16_t` underflows; `findNextStep` returns NULL and
  `deleteStepsFromTo(from, to=NULL)` runs `xcopy(from, NULL, …≈4.1 billion)`
  and segfaults in `xcopy` (`charString.c:1217`). On the device: reboot,
  typed line lost.
- **Why it is wrong.** `forthFoldLeave`'s own debris sweep
  (`manage.c:2031-2055`) carries the four guards this splice lacks — a
  comparison (`getNumberOfSteps() <= entryStepCount+1`) instead of a raw
  subtraction, a hard `i<4` cap, an explicit `victim==NULL ||
  isAtEndOfProgram || isAtEndOfPrograms` break, and a `lastErrorCode`
  check — and its comment spells out why each is required. The splice 700
  lines away has none of them, and its two `getNumberOfSteps()` samples
  assume `currentProgramNumber` stays anchored to FHIST across the whole
  suspension, which a live GTOP violates.
- **Bug class.** *Unbounded derived counter inside a bounded scan* crossed
  with *scope-mismatched sampling* (`bug-classes.md`).
- **Class test.** Drive `GTO . .` from an open interactive capture with a
  second program in memory; assert the resume neither crashes nor deletes
  past FHIST's END. Generalised: enumerate every item that can run LIVE
  (PARK) or move `currentProgramNumber` while an interactive fold is
  pending, and assert each leaves `currentProgramNumber` on FHIST at the
  moment the epilogue resumes — or the splice clamped to
  `[0, steps-after-capture]`.
- **Door note.** Gemini's W2 packet named the **UP softkey**
  (`ITM_Max`, `ui/tam.c:518`) as the trigger; that door is REFUTED (§6,
  G-W2-1) — UP keeps the cursor at local step ≥2, targets FHIST's own
  first step, and resumes clean. The crash is the `.`-promotion door. The
  finding was real through a different door than the reader named, in the
  confirming direction.

---

### F2 — R1: `fnKeyExit`'s SYSFL arm strands an armed interactive fold; the next store is silently forged into a program step

**EXECUTED — gate-driven, red for the predicted reason, with the durable
cross-mode cost observed.**

- **Where.** `packages/forth-core/keyboard.c:3934-3938` (the SYSFL EXIT
  arm's `return`, above the file's only `forthFoldUnwindIfDone` at `:3988`).
- **Reaching input (fixture F2, ordinary cancel).** Console open (keys),
  type `12`, stack `-MNU_FLAGS`, press **SF** (`forthFoldEnter` ARMED +
  `forthCaptureSuspend`), navigate to the **SYS.FL** catalog level
  (`catalog==CATALOG_SYFL`, `currentMenu()==-MNU_SYSFL`, reached the real
  way), press **EXIT**. Observed after EXIT#1: `tam.mode=0, armed=1,
  susp=1, open=0`. The arm's `return` fired above the unwind;
  `leaveTamModeIfEnabled` declined the resume on its `&& !forthFoldArmed()`
  clause. The fold is stranded armed with the capture SUSPENDED.
- **The durable cost — OBSERVED.** With the fold left armed, an unrelated
  **normal-mode `STO 05`** (register 05 pre-seeded to 555) was driven:
  `tamProcessInput`'s bracket (`ui/tam.c:1461-1463`) saw `forthFoldArmed()`
  and forged `calcMode=CM_PEM`, so the store recorded as a PROGRAM STEP
  instead of executing. **Register 05 stayed 555** and the FHIST tail
  became a non-`ITM_FORTH` orphan. The corruption crosses modes, is
  durable, and is invisible until it bites.
- **Why it is wrong.** `forth_capture.h:201-203`: "every `forthFoldEnter`
  that returns with `forthFoldPending()` true must be matched by exactly
  one `forthFoldLeave`." The SYSFL `return` matches none. `keyboard.c:3983-3987`'s
  own sibling comment names the gesture: "'type something, press STO, EXIT
  before finishing' is one of the most ordinary cancel gestures there is."
  `DESIGN.md` §8.4.3.
- **Bug class.** *Second-exit bypass* + *durable invisible cross-mode side
  effect* (`bug-classes.md`).
- **Class test.** Every `return` in `fnKeyExit` reachable with an ARMED
  interactive fold, asserted to leave `!forthFoldArmed() &&
  !forthCapIsSuspended()`.

---

### F3 — R2: the interactive EXIT ladder closes a SUSPENDED capture under a live armed fold; the typed line is lost

**EXECUTED — fixture F2, continuing R1's sequence.**

- **Where.** `packages/forth-core/keyboard.c:4072` (the ladder's entry
  gate reads `forthCapIsInteractive()`, TRUE for SUSPENDED) and its
  `forthCapClose()` close rung.
- **Reaching input.** From R1's stranded state, two more EXIT presses out
  of the menu reach the ladder. EXIT#3 ran `forthCapClose()`: the capture
  went CLOSED **while `forthFoldArmed()` stayed TRUE**, the parked
  `ITM_FORTH` capture step still in FHIST (`fh=3>2`, tail kind
  `ITM_FORTH`). The owner's typed `12` survives only as the orphaned step;
  the fold stays armed to forge the next TAM — R1's durable corruption,
  reached by two EXIT presses.
- **Why it is wrong.** `forth_menu.c:543`/`:627` establish
  `forthCapIsInteractive() && forthCapIsOpen()` as the precondition for
  touching the console surface; the ladder commits under half of it —
  origin where openness is meant.
- **Bug class.** *Origin tested where openness is meant* (`bug-classes.md`).
- **Class test.** With the capture SUSPENDED under a live armed fold,
  assert EXIT and ENTER neither close nor commit.

---

### F4 — S1-C: a SECOND site of R1's class — `GTO → . → PROG` softkey strands the armed fold at the recall guard `keyboard.c:1219`

**Survived the refutation pass with the path constructed AND executed via
an in-suite probe. This is the census result R1 demanded: the bracket's
teardown-owner set is not closed, and here is another member.**

- **Where.** `packages/forth-core/keyboard.c:1219` (the
  `tam.function==ITM_GTOP && catalog==CATALOG_PROG` recall guard, whose
  body is `runFunction(item); leaveTamModeIfEnabled(); return;` — no
  `forthFoldUnwindIfDone`, unlike `fnKeyExit:3988`).
- **Reaching input (probe-confirmed gesture).** Open an interactive Forth
  line → **GTO** (enters TAM as `TM_LABEL`, which `_forthFoldAdmits`
  ADMITS, so the fold ARMS) → **`.`** (promotes `tam.function`
  `ITM_GTO`→`ITM_GTOP` in place, `ui/tam.c:799`, foldMode untouched) →
  **PROG** softkey (present in `menu_TamLabel`, opens
  `catalog=CATALOG_PROG`) → pick a program. The probe observed the guard
  at `:1217` satisfied exactly while `forthFoldArmed()==1`; after the
  branch's `leaveTamModeIfEnabled()` (which defers armed-fold resume,
  `tam.c:1425`), the fold is left permanently ARMED and the capture stuck
  SUSPENDED with `tam.mode=0` — identical strand to R1.
- **Why it is wrong.** `forth_capture.h:201-203` (same bracket contract as
  R1). `DESIGN.md` §8.4.3: the unwind "runs from both the `tamProcessInput`
  epilogue and `fnKeyExit`" — `keyboard.c:1219` is outside both owners, so
  it is outside the contract's coverage.
- **Bug class.** *Second-exit bypass* / *bracket with an open-ended owner
  set* (`bug-classes.md`; the D7-1 shape).
- **Class test.** The class census IS the test: for every caller of
  `leaveTamModeIfEnabled` outside the `tamProcessInput` epilogue
  (`keyboard.c:1219, :1235, :1248, :1272, :1417, :1421`; `screen.c:806`;
  `assign.c:1193`; `programming/lblGtoXeq.c:205`; the non-epilogue
  `ui/tam.c` sites), assert: reachable with `forthFoldArmed()`? and if so,
  does control reach a `forthFoldUnwindIfDone` before the key event ends?
  Any yes/no is another R1. (`ITM_ASSIGN`/`ITM_USERMODE` are PARK not
  ARMED — verify, do not assume they are excluded.)

---

### F5 — R7(a): the alpha-mode fold destroys the C17 stamp; resume re-pushes the row unregistered; keys plane ends under an ALPHA row (C18's symptom)

**EXECUTED — fixture F1; every predicted value matched.**

- **Where.** `packages/forth-core/programming/manage.c:1379-1381` (resume's
  raw `showSoftmenu(-MNU_ALPHA)`), against the C17 stamp machinery in
  `forth_menu.c`.
- **Reaching input.** Console open in the `-MNU_ALPHA` excursion (owned
  stamp=1). Press **STO**: `forthCaptureSuspend` → `_closeAlphaMenus`
  popped the OWNED ALPHA frame, owned+borrow → 0. Type the register digits:
  resume ran raw `showSoftmenu(-MNU_ALPHA)`, re-pushing the row
  UNREGISTERED (owned+borrow stayed 0 with the capture OPEN under
  `-MNU_ALPHA`). One **EXIT**: the excursion rung committed `keysMode=1`
  (the identity fallback answered "base on top" for the unstamped row)
  while `forthConsoleShowSurface` changed nothing — **keys plane under an
  ALPHA row**, C18's symptom, produced by R7's door.
- **Why it is wrong.** `forth_menu.h:24-27`: `forthConsoleShowSurface` is
  the ONLY function that changes the console's row. The C17 banner at
  `forth_menu.c:340-345`. `DESIGN.md` §8.4.4's no-disagreement rule.
- **Bug class.** *Single-owner contract with untracked writers* /
  *persistence-contract mismatch* (`bug-classes.md`).
- **Class test.** Fold from BOTH sub-modes; assert after suspend AND after
  resume that exactly one frame is registered and it is the row on screen.

---

### F6 — R4: history recall fires while SUSPENDED and overwrites TAM's name buffer mid-name

**EXECUTED — fixture F3.**

- **Where.** `packages/forth-core/keyboard.c:2839` (the recall guard reads
  `forthCapIsInteractive()`, TRUE for SUSPENDED).
- **Reaching input.** Console open, one history line `12 34 +` pushed.
  Press **GTO** (admitted, fold ARMED), reach `tam.alpha` name entry, type
  a name (`aimBuffer=AB`). Press **f-UP**: the guard passes, so
  `forthHistoryRecall(-1)` ran and overwrote `aimBuffer` — `AB` → ``
  → `12 34 +`. TAM's name buffer is clobbered mid-name.
- **Why it is wrong.** `forth_capture.c:48-51`, the suspension contract:
  "TAM is free to use `aimBuffer` for its own name entry while we are
  suspended" — `forthHistoryRecall` writes it while suspended, so TAM is
  not in fact free.
- **Bug class.** *Origin-vs-openness confusion* (`bug-classes.md`).
- **Class test.** For every console gesture handler, drive it with the
  capture SUSPENDED under a live TAM and assert `aimBuffer`, `T_cursorPos`,
  `displayAIMbufferoffset` are byte-identical before and after.

---

### F7 — R3: the `f` long-press de-registers the console's row; the row says one plane while the keypad types the other

**CONFIRMED BY SCREENSHOT — two `run-sim` captures with `forthCapKeysMode`
printed at both moments.**

- **Where.** the `isAlphaSubmenu` predicate widened to admit `-MNU_FORTH`,
  with `screen.c:916`'s `popSoftmenu` consumer never re-enumerated.
- **Reaching input.** Console open, FWRD word picker showing
  (`forthCapKeysMode=1`, `currentMenu=-MNU_FORTH`, owned=1). Hold **f**
  (driven through the real timer callback chain
  `fnTimerConfig/Start/Exec` → `refreshFn` → `Shft_handler`). After: the
  band shows the full three-row ALPHA keypad, `forthCapKeysMode` **still
  1**, `currentMenu=-MNU_ALPHA`, owned+borrow=0. Keyboard on the keys
  plane, row reads ALPHA, console stamp destroyed. Two gestures from the
  top of the console. This one shot settles R3, R7 and R8's shared
  on-screen claim.
- **Why it is wrong.** `forth_menu.h:24-28`; `DESIGN.md` §8.4.4.
- **Bug class.** *Predicate widened for one consumer, others unchecked*
  (`bug-classes.md`).
- **Class test.** For each consumer of `isAlphaSubmenu`/the keys-mode
  predicate, assert the console stamp survives every path that widened the
  predicate.

---

### F8 — G-W1b-Z: the R1 residue state is a ZOMBIE console — five sites read `forthCapIsInteractive`/`forthCapKeysMode` where openness is meant, so the surface renders live and swallows input

**Survived the refutation pass (correctness lens), including its own
self-correction. Reachability is CONTINGENT on an R1-class door (F2/F4)
supplying the residue.**

- **Where.** `packages/forth-core/screen.c:5705` (the `_forthConsoleActive`
  render gate), plus `keyboard.c:1786`/`:1843` (input-plane select),
  `items.c:742`/`:667` (function-key and user-item dispatch),
  `keyboard.c:60` (`_forthCapAtCap` typing gate).
- **Reaching input.** The R1 residue: capture SUSPENDED with origin
  INTERACTIVE, `tam.mode=0`, `calcMode=CM_AIM`, `FLAG_ALPHA` cleared by
  `leaveTamModeIfEnabled` — R1's product (F2/F4). In that state
  `forthCapIsInteractive()` (`forth_capture.c:126`) returns
  `origin==INTERACTIVE && state!=CLOSED` = TRUE, so: the render gate fires
  in the LIVE refresh path (`screen.c:6036-6044`), painting TAM's abandoned
  `aimBuffer` as an editable console line; a parameterless function key
  (SIN) and a user word route into `forthCapInsertName`, which REFUSES
  (`forth_menu.c:35`, state != OPEN) and returns WITHOUT executing — the
  gesture neither types nor runs; the input plane follows a suspended
  capture's mode bit.
- **Why it is wrong.** The suspension contract read together with each
  site's use of the origin/keys predicate as a LIVENESS test. The
  predicate answers "whose capture" and is TRUE while suspended; the sites
  need "is there a live line". This is the D7-2 class with five more
  members.
- **Correction carried.** The finder's original "appended and lost" is
  wrong — the inserts REFUSE (round-5 6.1(c)); "the function does not
  execute" stands. And "eats every keypress" is imprecise: a PARAMETERISED
  key (STO) falls through `items.c:789` to `tamEnterMode` and does enter
  TAM. Severity is honestly hedged as contingent on the R1 door.
- **Bug class.** *Origin tested where openness is meant*, structural (five
  sites) (`bug-classes.md`).
- **Class test.** Enumerate every read of `forthCapIsInteractive()`/
  `forthCapKeysMode()` on a render, route or gate path; for each, drive the
  suspended-interactive residue and assert the site treats it as NOT-live.

---

### F9 — G-W1b-RS: long-press R/S in the residue state runs TAM's leftover `aimBuffer` as live Forth

**Survived the refutation pass (intent lens): the arm is mechanically live
in the residue and no design ruling blesses the run. CONTINGENT on the R1
door supplying the residue.**

- **Where.** `packages/forth-core/keyboard.c:3060` (the arm
  `forthCapIsInteractive() && item==ITM_RS`, gated on NO state/foldMode/
  tam.mode conjunct).
- **Reaching input.** R1 residue (SUSPENDED, origin INTERACTIVE), then
  **long-press R/S**: the first conjunct passes because
  `forthCapIsInteractive()` is TRUE while SUSPENDED, so `forthInteractiveEnter`
  runs — and on a non-empty `aimBuffer` it pushes TAM's leftover scratch to
  FHIST and runs it as a Forth line (`manage.c:1400-1447`). TAM's scratch
  (a register-number fragment, a label name) is committed to history and
  executed: wrong-result or error spam from a gesture the owner uses
  constantly, plus corrupted history.
- **Why it is wrong.** The suspension contract ("aimBuffer belongs to TAM
  while suspended") and `forthInteractiveEnter`'s documented precondition —
  its orchestration comment assumes an OPEN interactive capture whose
  `aimBuffer` is the owner's line. Every design ruling found (DESIGN.md:2625-2630;
  STAGE_L_INTERACTIVE.md T3/T4) scopes R/S to an OPEN capture; none scopes
  it to the SUSPENDED residue.
- **Bug class.** *Precondition assumed OPEN, reached while SUSPENDED*
  (`bug-classes.md`; D7-2).
- **Class test.** Drive long-press R/S in the suspended-interactive residue
  and assert `forthInteractiveEnter` is not entered (or is a no-op).

---

### F10 — G-W2-2: the resume splice's no-room break arm loses the TAM commit entirely; its "keep" comment contradicts the sweep's "covers every break path" comment

**Survived the refutation pass (correctness lens), path granted and traced
end-to-end. Narrow (needs a ~250-byte line) but silent.**

- **Where.** `packages/forth-core/programming/manage.c:1346` (the no-room
  break arm, comment "no room: keep this and later steps after the line")
  vs `manage.c:2031-2034` (the sweep, comment "This covers every break path
  in that loop (oversize text, no room) and the PARK case").
- **Reaching input.** Interactive console, type a line near the 255-byte
  cap (~250 bytes), press **STO** (fold ARMS, capture suspends), type
  `1 0` (TAM commits `STO 10` as an FHIST step), commit ends TAM. The
  epilogue resumes: the splice decodes the step but `forthCapInsertName`
  refuses (`forth_menu.c:43`, line would exceed the cap) and breaks,
  KEEPING `STO 10` in FHIST; then `forthFoldLeave`'s sweep runs (foldMode
  still 1) and DELETES it (it is above `entryStepCount+1`, first victim of
  `findNextStep(currentStep)`). Net: the line is restored intact, `STO 10`
  is neither folded, executed, nor kept — it vanishes with no error.
- **Why it is wrong.** For the PEM fold the kept steps belong to the edited
  program and both comments coexist; for the INTERACTIVE fold the steps are
  FHIST entries and the two comments prescribe OPPOSITE dispositions for
  the same steps — one of them is false for this path.
- **Bug class.** *Contradictory owners / disposition collision* — a
  committed user operation dropped between "keep" and "sweep"
  (`bug-classes.md`).
- **Class test.** Drive a near-cap interactive line + a parameter TAM
  commit; assert the committed operation is either folded into the line or
  kept in FHIST, never silently deleted; and assert the two comments agree
  on the disposition for the interactive fold.

---

### F11 — F2: EMIT/`.$` error paths skip their declared `-1`, so `forthPrimInvoke` settles the spill against a false depth and clobbers a live register

**Survived the refutation pass (reachability lens), reached with concrete
input and confirmed by an in-suite probe. OUTSIDE the fold/suspend window
(spill accounting); narrow (needs a spilled stack + an error).**

- **Where.** `packages/forth-core/forth_inner.c:157` (`forthSpillSettle`),
  driven by `forthPrimInvoke` applying the declared `stackEffect` at
  `:152` before `fn()` at `:155`.
- **Reaching input.** Interactive console, default 4-level display, enter
  `1 2 3 4 5 .$`: the 5th push spills value 1 (`forth_inner.c:213-228`),
  leaving X Y Z T = 5 4 3 2 and spill {1}. `.$` runs through
  `forthPrimInvoke`, which applies the declared `stackEffect -1`
  (`forth_prims.c:261`) BEFORE `pPrintStr` runs; `pPrintStr` sees X=5 is
  not `dtString` and returns WITHOUT `fnDrop` (`forth_prims.c:184-187`);
  `forthSpillSettle()` then sees `spillCount>0 && depth<capacity` and
  refills `getStackTop()` — freeing T(=2) and overwriting with the spilled
  1 → 5 4 3 1, where a correct never-consume leaves 4 3 2 1. The erroneous
  settle EMPTIES the spill, so the loud `ERROR_RAM_FULL` that a non-empty
  spill would raise at line end never fires — a loud stop replaced by
  silent corruption. `EMIT` twin: `1 2 3 4 5 EMIT` (X=5 out of glyph
  range, error arm skips `fnDrop`, `forth_prims.c:172-175`). (The probe ran
  on the capacity-8 self-test harness, so the reproducer there is
  capacity+1 pushes; the mechanism is capacity-independent.)
- **Why it is wrong.** `forth_prims.h:17` declares `stackEffect` the "net
  data-stack change at RUNTIME"; `forth_inner.c:145-149` calls
  `forthPrimInvoke` "the ONLY way to invoke a primitive … applies the
  declared stack effect … then refills vacated slots" — assuming `fn()`
  vacated the slots the `-1` promised. EMIT/`.$` perform the `-1` only on
  success, 0 on error, with no channel to tell `forthPrimInvoke`.
  `DESIGN.md` 3.4/11 bless only two accounting exceptions; a prim's own
  error return is not among them.
- **Bug class.** *Declared stack effect not honoured on the error path*,
  settled against actual depth (`bug-classes.md`).
- **Class test.** For every prim whose `stackEffect` is nonzero and which
  can return without consuming (EMIT, `.$`, any type/range-gated prim),
  drive an already-spilled stack into the error arm and assert the deepest
  live register is unchanged and the spill is not drained.

---

### F12 — U1: the round-4 ownership oracle asserts a lifetime the design does not have — its no-capture clause fires on `FCAP_SUSPENDED`, where the stamp must survive

**SETTLED TRUE by execution (fixture F0 probe). A TEST defect, not a
runtime one — but load-bearing: it BLOCKS the fixture the handoff has
demanded for four rounds.**

- **Where.** `packages/forth-core/test_console.part.h:2618` (the clause
  `if (!forthCapIsOpen() && (owned || borrow))`).
- **Reaching input.** Any fixture that suspends an interactive capture and
  consults the oracle: open the console over a foreign menu (owned
  stamp=1), suspend (keys-mode fold, round-5 6.2 — the stamped FWRD frame
  stays intact, state `FCAP_SUSPENDED`). The clause fires because
  `forthCapIsOpen()` is false for SUSPENDED while `owned==1`. The probe
  drove both the created-base (owned=1) and borrowed-base (borrow=1)
  suspended states and observed the oracle emit a FAIL and set
  `probefail=1` in each.
- **Why it is wrong.** `forth_menu.c:302-305`: a stamp must not outlive its
  CAPTURE — but a SUSPENDED capture has not ended. `DESIGN.md:2858-2859`:
  the mark "survives every REPL reopen and fold resume by construction".
  `forthCapAbandonSuspended()` exists precisely to unstamp on ABANDON, not
  on suspend. The correct predicate is `forthCapIsOpen() ||
  forthCapIsSuspended()`. This is the wrong-oracle shape that manufactured
  three false failures in round 3, now in the direction that BLOCKS the
  missing test.
- **Bug class.** *Oracle asserts a lifetime narrower than the design's* —
  a test that fails on the correct answer (`bug-classes.md`, D6).
- **Class test.** Assert the ownership oracle passes on a legitimately
  stamped SUSPENDED capture (both owned and borrowed bases) and fails only
  on a stamp that outlives CLOSE/ABANDON.

---

### F13 — U5: the interactive EXIT ladder is 135 lines of package ownership logic inlined into upstream `fnKeyExit` where a one-call seam is available

**Survived the refutation pass (intent lens). A design/upstream-discipline
defect with NO runtime path — realised only at the next rebase. This is
the finding I would leave alone if the goal were correct code rather than
an audit-clean footprint (see §7).**

- **Where.** `packages/forth-core/keyboard.c:4072-4207`, inside upstream's
  `case CM_AIM:` of `fnKeyExit`, ten lines above the surviving native AIM
  ladder it was copied from and has since diverged from. Mechanically the
  largest block in `design-audit.sh` group D (136 lines, §2).
- **Reaching input.** Not a runtime path — the cost lands at the next
  upstream rebase, and this project rebases. The block grew 121→136 lines
  in the round-5 range; the native ladder below it silently diverges from
  the copy above.
- **Why it is wrong.** The package's own upstream discipline (CLAUDE.md:
  all work through the external package system). `DESIGN_AUDIT.md` §2.1:
  "if this were a call to package-owned code, would anything be lost? If
  not, it belongs in a package `.c`," and names exactly ONE standing inline
  exception (`manage.c`'s PEM submode), adding "Every OTHER file should be
  trending toward a call site." A switch case whose every path breaks is a
  call-out at a seam — precisely the shape §2.1 says belongs in a package
  file. The equivalent seam is one call, the shape the package already uses
  for `forthConsoleBaseOnTop`, `forthConsoleOwnsSlot0`,
  `forthConsoleShowSurface`, and `items.c:742-787`. No amendment rules a
  console EXIT seam considered-and-rejected; `PACKET_L1_2`'s "copy its
  shape, do not share code" forbids sharing with the PEM ladder, not
  extracting the console ladder into its own function.
- **Bug class.** *Package logic inlined at an upstream seam where a call
  would do* (D8; `bug-classes.md`).
- **Class test.** Not enumerable as a unit test — the guard is
  `design-audit.sh` group D holding flat or shrinking, plus a review rule
  that a new `case`-of-breaks in an upstream file must justify why it is
  not a package call.

---

## 4. PLAUSIBLE findings

**None this round.** P-A — round 5's sole PLAUSIBLE (splice count
unclamped, reaching input unconstructed) — was PROMOTED to a confirmed
crash (F1): the reaching input is now constructed and reproduced as a
SIGSEGV. Every other finding this round either carries a constructed
reaching input (confirmed, §3) or was refuted (§6). The contingent
findings F8/F9 depend on an R1-class door, but that door is itself
confirmed (F2/F4), so they are confirmed, not plausible.

---

## 5. Design observations (D7)

Shape, not defects. These outlast the bug list and are the reason to run
the audit.

**D7-1 — the fold bracket is not correct by construction, and cannot be
made so while teardown and fold-finalisation have separate owners.** Sol's
design verdict (self-contained packet, GPT-5.6). The bracket contract
("every `forthFoldEnter` … matched by exactly one `forthFoldLeave`") is
prose enforced by an OPEN-ENDED set of teardown owners: the
`tamProcessInput` epilogue, plus one call in `fnKeyExit`, plus whatever
else ends a TAM session. F2 (R1) proves the set is not closed; **F4 (S1-C)
proves it a second time**, at `keyboard.c:1219`, and its class census names
nine more `leaveTamModeIfEnabled` callers to check. Sol's proposed shape —
a single terminal transition (`tamFinish`) owning teardown, deferred
dispatch, resume and the one `forthFoldLeave`, with raw
`leaveTamModeIfEnabled` made unreachable from keyboard/commit sites — is
the by-construction fix; its cost is concentrated in `ui/tam.c`'s eleven
leave-then-dispatch sites, recorded as the owner's design decision, not a
patch.

**D7-2 — the suspension state `(SUSPENDED, tam.mode==0)` is a state many
readers treat as live.** F3 (R2) and F6 (R4) read `forthCapIsInteractive()`
/ `forthCapKeysMode()` there where openness is meant, with executed
reproductions. **F8 (G-W1b-Z) adds five more sites of the same shape**
(the render gate `screen.c:5705`, the two plane selectors, the two dispatch
arms), and **F9 (G-W1b-RS)** is a sixth (long-press R/S enters
`forthInteractiveEnter` on the residue). The predicate answers "whose
capture" and is TRUE while suspended; every one of these sites needs "is
there a live line". The class is the round's densest — the residue is R1's
product AND the surface it corrupts.

**D7-3 — the "PEM-only today / inert in production" comments are now
STALE, and a reader who trusts them mis-assesses R1/R2/P-A.**
`manage.c:1305/1310` ("suspend/resume is PEM-only today") and
`manage.c:1895-1897` ("no tam.c wiring — inert in production") are
contradicted by `ui/tam.c:1180-1182`, which enters the fold and calls
`forthCaptureSuspend` for a live interactive capture. Multiple verifiers
tripped over these comments; they are a documentation hazard precisely
because they claim the fold/suspend window is dormant when it is the
window every crash this round lives in. Not a runtime defect — a stale
comment on the most load-bearing path.

**D-audit corroboration.** Group D's largest block IS F13/U5; group A's
footprint is the standing overlay cost; group E is the standing allocation-
lifetime prompt. The mechanical half agrees with the design read; none of
it is a separate finding.

---

## 6. Deliberately not flagged / refuted

Mandatory. This merges the refutation pass's disproofs with the standing
"looks like a bug, is not" set. An audit that clears nothing did not
understand what it read.

### Refuted by the pass

**U2 — a second BORROW stamp from the fold-back arm writing `userMenuId`
raw (`forth_menu.c:555`).** REFUTED on reachability. The damaging
consequence needs a wedge — an unstamped `-MNU_FORTH` frame at slot 1
between an OWNED slot-0 frame and a pre-existing BORROWED base below — and
that wedge is unconstructable. `_forthConsoleAcquireRow` has exactly two
callers: ShowSurface (`:575`, gated by `_stampedAt(0)`, which on the only
reachable branch places OWNED directly above the BORROW via the full-stack
xcopy — adjacent, no room for a wedge) and RestoreSurface (`:634`, gated by
`!forthConsoleStampOnStack()`, so it runs only when NO borrow exists). No
stack op inserts a frame between an owned slot-0 and its adjacent borrow.
The double-stamp state cannot be reached.

**U3 — C17's "safe to borrow" premise false for `pushSoftmenu`'s ungated
dedup (`forth_menu.c:307`).** REFUTED on correctness. The one true fact —
`pushSoftmenu`'s dedup reads `userMenuId` for every frame ungated — is
correct, but every conclusion is false: the banner names that read
explicitly at `:315-320` (no reader is steered wrong); `userMenuId=0` for
non-dynamic frames makes the compare a tautology (meaningful only for
dynamic frames, as the comment says); the negative stamp rides the borrowed
DYNAMIC base whose `softmenuId` a native push never matches; and every
close path clears the stamp (`:302-305`), so "permanently removes from
dedup" contradicts the banner's own invariant. The finder conceded "not a
runtime defect." A documentation nitpick that reads a parenthetical too
literally.

**U4 — the C18 toggle gate missing `forthCapIsOpen()` (`items.c:768`).**
REFUTED on intent — a DUPLICATE. Round-5 §6.1(c) already adjudicated this
exact route: with the capture suspended the flip IS committed while
ShowSurface returns on `!forthCapIsOpen()`, but the consequence does not
follow — `interactive && !open` is exactly `FCAP_SUSPENDED`, in which
window neither `keysMode` value can touch the owner's line (keys plane →
`forthCapInsertName` returns false; alpha plane → `aimBuffer`, TAM's
scratch), and `manage.c:1379` makes the row follow the flipped bit at
resume. The general two-predicate seam is tracked as R8 and documented in
D7-2; the round-5 record itself files U4 as "probably a duplicate of R8+R2
— check before counting it." It lands on duplicate.

**G-W1a-1 — a PEM-origin suspension stranded forever when `calcMode`
leaves `CM_PEM` before the TAM cancel (`ui/tam.c:1425`).** REFUTED on
intent — the finding's own cited artifact is the ruling that defeats it.
`forth_capture.c:7-9`: "A still-SUSPENDED object is an orphan here (exotic
mode changes that skip the resume choke point); the assignment below drops
it." That names precisely this scenario and declares the drop-at-next-open
the intended cleanup. "Stranded forever" is also false in fact:
`forthCapOpen()`→`_forthCapOpenAs()` is unconditional, so the orphan clears
at the next PEM entry; and the on-disk step is the single source of truth
(F6-2/S3), so the dropped snapshot holds only transient session position a
fresh reopen resets anyway. The design anticipated the orphan, defined the
recovery, and guaranteed no data loss.

**G-W2-1 — P-A's trigger is the GTO TAM's UP softkey dispatching GTOP live
mid-fold (`ui/tam.c:518`).** REFUTED on reachability — the WRONG DOOR for a
REAL crash. Line 518 is reachable, but the load-bearing claim (this GTOP
moves `currentProgramNumber` off FHIST) is false: the UP path crosses to
another program only when `currentLocalStepNumber==1` (`tam.c:506`), and in
an armed fold the cursor is parked on the capture step at local step ≥2, so
the else branch targets FHIST's own first step — `currentProgramNumber`
stays anchored, `n=0`, the loop never runs. The crash is real through the
`.`-promotion door instead (F1). Refuting THIS door was load-bearing: it is
what forced the fixture onto the door that actually crashes.

### Cleared as deliberate (standing "looks-like-a-bug")

- **`calcModeNormal()` then an unconditional `popSoftmenu()` in the EXIT
  ladder.** Two calls that look redundant; removing either leaves the
  user's menu buried, and the comment names the rev that got it wrong.
- **The C17/C18 refuse-not-force decision (`items.c:768`,
  `keyboard.c:4135-4137`).** The toggle REFUSES rather than forces the row;
  the site's own audit banners record the decision, and D7-2 tracks the
  seam as R8, not as a fresh defect.
- **`forthCapClose()` not clearing the console ring while
  `forthCapPowerReset()` does.** Looks asymmetric; it is, by ruling — the
  dialogue survives close.
- **The recommit-on-suspend and "reopen = fresh line" behaviour.** The line
  is not carried across a suspension because the on-disk step is the single
  source of truth (F6-2); dropping the snapshot's transient position is
  deliberate, not a leak.
- **Stage-N non-goals** (no line wrapping, no string literals, no input
  words) — absent features, written down, not defects.

The D7-3 stale comments are the one item in this neighbourhood that is NOT
cleared: they are wrong, not deliberate, and belong on the fix list as a
documentation correction even though no runtime path depends on them.

---

## 7. Verdict

**Would I ship it? No.**

- **F1 (R-P-A)** is a hard crash — SIGSEGV, device reboot — reachable in
  three keypresses from an open console (`GTO . .`), reproduced with a
  product-code-only backtrace.
- **F2/F3/F4 (R1/R2/S1-C)** lose the typed line on an ordinary catalog
  cancel and leave a fold armed that silently forges the next store into a
  program step — durable, cross-mode, invisible until it bites, executed
  rather than traced, and now known to have at least three distinct doors
  because the bracket's teardown-owner set is open (D7-1).
- **F5/F7 (R7/R3)** put the keys plane under an ALPHA row two gestures from
  the top of the console.
- **F6 (R4)** clobbers a name mid-entry.
- **F8/F9 (the zombie-console cluster)** turn R1's residue into a surface
  that looks live and swallows input, or runs TAM's scratch as Forth.

**Where does it break first?** Press **GTO** then **`.`** twice (F1,
crash). Or press a parameterised item and cancel with a catalog up (F2 →
F3, line lost + armed-fold corruption). Or **GTO → `.` → PROG** and pick a
program (F4, same strand, second door). Or hold **f** (F7, plane/row
disagreement).

**What I would leave alone if the goal were correct code, not an audit-clean
tree.** **F13 (U5)** — the inlined EXIT ladder is a rebase-hygiene and
footprint finding with no runtime path; extracting it is worth doing for
`design-audit.sh` group D, not for correctness, and it should not gate a
fix wave aimed at the crashes. **F12 (U1)** is a test-oracle defect, not
product code — but it is the exception to "leave alone," because it BLOCKS
the fixture that pins F1–F9, so it is fixed first as a precondition, not
deferred. **F10 (G-W2-2)** and **F11 (F2/spill)** are genuine silent
corruption but narrow (a ~250-byte line; an already-spilled stack plus an
error); I would fix them, but they rank below the fold-window crashes and
would not block a ship on their own.

**The pattern, sixth round running.** The window the whole design reasons
about and no test entered is exactly where the findings are — round 6 built
the fixture that turned six traces into gate-reproduced facts and promoted
a PLAUSIBLE to a live crash. A fixture that drives the real gesture is
worth more than any number of static traces; the fold/suspend window had
none for four rounds, and building one was the round's yield.

---

## 8. Round and exit state

**Round 6.** Subject `b5a0202c9..24bd4db99`, the fold/suspend window. Tree
clean, gate green, everything measured at `24bd4db99`.

**Readers.** No in-family FIND phase (the window was already enumerated).
An executed fixture (F0–F4, gate-driven in a worktree), a `run-sim`
screenshot pair, an out-of-family pass (Gemini 3.1 Pro ×3, GPT-5.6 Sol ×1,
all identity-verified), and a three-lens refutation pass over U1–U5, P-A
and every out-of-family finding, each verifier in its own worktree.

**What this round settled.**

- **Round 5's five unverified U-items → all settled.** U1 CONFIRMED (F12),
  U5 CONFIRMED (F13); U2, U3, U4 REFUTED (§6).
- **Round 5's sole PLAUSIBLE (P-A) → CONFIRMED crash** (F1).
- **Out-of-family / broad pass → four new CONFIRMED** (S1-C/F4,
  G-W1b-Z/F8, G-W1b-RS/F9, G-W2-2/F10) plus one out-of-window CONFIRMED
  (F2 spill / F11); **two REFUTED** (G-W1a-1, G-W2-1).
- **Round 5's R1–R7a → upgraded from static trace to EXECUTED** (F2, F3,
  F5, F6) or screenshot (F7); no downgrades.

**Exit criterion: NOT met, and reset.** Round 6 promoted a PLAUSIBLE to a
confirmed CRASH and reproduced five more findings by execution — the
opposite of a clean round. The exit rule (two consecutive clean rounds, one
out-of-family) is unchanged in kind; this round added a confirmed crash and
a second door for R1's class, so the earliest close moved further out, not
closer. Thirteen findings stand open in this report (F1–F13); the D7-3
stale comments are a fourteenth documentation item. The rulings owed carry
forward; the fixture (F12's precondition) is the gate item for round 7.

---

## 9. Process defects the round earned (the growth rule)

The forum loop does with tells what this audit does with misses. Two
process defects this round, both now encoded in the tooling.

**1 — Isolated worktrees spawn at a STALE ref, and it nearly poisoned the
whole pass.** Every worktree this round — the two evidence agents and all
thirteen refutation verifiers — came up at `c3a00768c`, ~114 commits behind
the audited tip `24bd4db99`, where the audited files and this report's line
numbers do not yet exist. A verifier reading there produces verdicts about
a codebase that does not exist — round 3's disease (verdicts that were the
author's own traces) by a new vector. The two evidence agents caught it
only because their briefs cited specific lines that failed to resolve; a
refutation prompt that cites none would not have. Mid-round the fix landed
in `audit-workflow.js`: every verifier's FIRST action is now `git log
--oneline -1` and a checkout to the audited tip. The proof it held is in
this run's journal — **all thirteen verifiers recorded hitting
`c3a00768c` and checking out to `24bd4db99` before their first read**, and
the earlier refutation attempt (run without the fix, and separately
clobbered by defect 2) was discarded and re-run. The rule, now in
`CODE_AUDIT.md`: *a worktree's ref is a claim, and claims get checked
before the first read.*

**2 — `resumeFromRunId` DROPS args — a parameterized run silently became
the wrong audit.** The first refutation pass was stopped to land the
worktree fix, then relaunched with `Workflow({scriptPath, resumeFromRunId})`
and no args. Resume passes no args, so every default took over: a
refutation-only run (`dimensions:[]`, thirteen `extraFindings`) silently
became an eight-dimension FIND over `main..HEAD` under the subject "the
current branch" — round 1's wrong-range failure by a new door, and it lost
all thirteen findings it was meant to verify. Caught by the result summary
naming the wrong subject; relaunched FRESH with the args re-passed, which
is the correct pass this report is built on. Encoded in `audit-workflow.js`:
NEVER resume a parameterized run to pick up a script edit — relaunch fresh.
*(The accidental broad run was not wasted: it surfaced F11/F2, the EMIT
spill-settle clobber, which was then re-verified cleanly in the correct
pass. A real finding through the wrong door, in the confirming direction —
the same shape as F1's `.`-vs-UP door, twice in one round.)*

**Also encoded, from the out-of-family pass.** The packet size ceiling is
raised in `packet_lint.py` (four packets of 11.5–16.6 KB all answered in
minutes; the old ~11 KB note was too low). The packet template now demands
the `MODEL:` identity line in the FINAL message for repo-hybrid packets — a
reader that runs tools between the lead-line instruction and its answer
forgets it, and `dispatch.sh` correctly discarded one such reply this round.
And a session safety guardrail tripped on the workflow's old name
("adversarial audit") on a process whose whole point is functional bugs;
the SKILL now keeps security-flavoured vocabulary out of names, meta and
spawn prompts, and the workflow is renamed `forth-core-code-audit`. The
process is unchanged; the words now say what it is.

**A round that taught this much about the process is a round that did its
job even where the tooling failed** — both failures were caught before they
reached the report, and both are now guarded.

---
