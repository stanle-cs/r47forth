# Stage L — architect pre-work traces T1–T6

**Status: traces complete 2026-08-04. Evidence for L-R4 is in T1 + T3 +
T5; the ruling is the owner's and is stated as a recommendation at the
end, not decided here. Not normative — this is the evidence sheet
STAGE_L_INTERACTIVE.md's "Mandatory architect pre-work" section
requires before any packet is authored.**

Method: read-only trace of the landed tree (branch `forth-core/stage-k`,
gate green — forth battery ALL PASSED + upstream testSuite GREEN — before
tracing). All anchors are the package working area
(`packages/forth-core/…`), which is what packets cite; upstream anchors
are given as `src/c47/…` where the file is not overridden. Every
`file:line` below was read this pass, not carried from an earlier stage.

---

## T1 — the interactive dispatch seam (the E0-equivalent)

### Deliverable

**Site: `runFunction()`, packages/forth-core/items.c:651 — a new arm
placed immediately before the TAM-entry block at items.c:736.**

**Gate expression:**

```c
if(forthCapIsOpen() && forthCapOriginInteractive())   /* new origin bit, L1-1 */
```

Nothing else in the interactive keyboard path is a funnel. Justification
below.

### The two interactive key paths, traced end to end

Both start at `btnPressed` (keyboard.c:1851) → `determineItem`
(keyboard.c:1569) → `processKeyAction` (keyboard.c:2416).

**Path A — character items (alpha input).** In `CM_AIM`,
`determineItem`'s first content arm (keyboard.c:1686) resolves the AIM
column, so a key yields e.g. `ITM_A`. `processKeyAction`'s `case CM_AIM`
(keyboard.c:2886-2898) calls `processAimInput(item)` (keyboard.c:498),
whose `func == addItemToBuffer` arm (keyboard.c:565) calls
`addItemToBuffer` (src/c47/bufferize.c:445), which writes `aimBuffer` at
`T_cursorPos` and sets `keyActionProcessed = true`.

*Consequence:* **Path A needs no divert at all.** The interactive capture
line IS `aimBuffer`, exactly as in PEM since S3 (forth_capture.h:8-27).
Alpha typing into a Forth capture is native AIM typing, unmodified. The
196-glyph/256-byte capture cap is NOT enforced on this path today
(`addItemToBuffer` bounds on `AIM_BUFFER_LENGTH` = 1024,
bufferize.c:466) — PEM gets the cap from `pemAlpha`'s own insert arm
(manage.c:983). **This is a real gap and belongs in L1-2**, not a
free ride: see "Cap enforcement" below.

**Path B — direct function items (keys mode).** With keys mode on,
`determineItem` must resolve the NORMAL column. It does not today: the
arm at keyboard.c:1686 tests `calcMode == CM_AIM` as its *first*
disjunct, unconditionally, and only the `CM_PEM` disjunct carries the
K1 keys-mode escape `&& !(tam.function == ITM_FORTH &&
forthCapKeysMode())`. So gate 1686 must widen (see the CM-gate audit).

Once widened, the item (e.g. `ITM_SIN`) flows:
`processKeyAction` `case CM_AIM` → `processAimInput(ITM_SIN)` → falls
through every arm (`indexOfItems[ITM_SIN].func != addItemToBuffer`) →
`keyActionProcessed` stays **false** → back in `btnPressed`,
`showFunctionName(item, 1000, funcParam)` (keyboard.c:1984) sets
`showFunctionNameItem = item` (packages/forth-core/screen.c:2141) →
`btnReleased`'s `else if(showFunctionNameItem != 0)` arm
(keyboard.c:2189) → **`runFunction(item)` (keyboard.c:2328)** → live
execution.

That is the exact interactive mirror of the PEM path, which reaches the
same `runFunction` and is diverted there by
`if(calcMode == CM_PEM) { … addStepInProgram(func); return; }`
(items.c:766-802) → `insertStepInProgram` (manage.c:1708) →
`pemAlpha` (manage.c:861) → the F6-3 CAT_FNCT insert arm
(manage.c:1089-1095).

**Softkeys converge on the same point.** `executeFunction`
(keyboard.c:954) reaches `runFunction(item)` at keyboard.c:1415 for
every menu/catalog pick. The FWRD picker short-circuits earlier, at
keyboard.c:987 via `forthPickerGuard` (forth_menu.c:65) — that guard is
CM_PEM-keyed and must widen, but it is not a second seam; it is a
pre-emption of one.

**Conclusion.** `runFunction` is the single site both physical keys and
softkeys pass through before execution, and it is where PEM already
diverts. One new arm there is the whole T1 seam. No change to
`btnPressed`/`btnReleased`/`processKeyAction`/`processAimInput` is
needed for dispatch.

### Placement is load-bearing (interacts with L-R4)

`runFunction`'s body order is: dynamic-menu RCL/XEQ resolution
(items.c:664-734) → **TAM entry** (items.c:736-760) → **PEM step
recording** (items.c:766-802) → `reallyRunFunction` (items.c:812).

- The interactive arm must sit **before items.c:736** so it can decide
  what happens to parameterized items. Placed after, `tamEnterMode`
  has already fired and the decision is gone.
- Under L-R4 option (a) the arm swallows parameterized items
  (`TM_VALUE <= indexOfItems[func].param <= TM_CMP`) as diverted no-ops
  and inserts text for `CAT_FNCT|PTP_NONE`.
- Under option (c) the arm inserts text for `CAT_FNCT|PTP_NONE` and
  *falls through* to items.c:736 for parameterized items.

Either way the placement is the same; only the arm's parameterized
branch differs. **L1-3 can be authored now with a single clearly-marked
hole**, which is why the TAM arm was deferred rather than the packet.

### Cap enforcement (new work L1-2 must carry)

PEM's capture insert enforces `len < 256 - inputCharLength &&
stringGlyphLength(aimBuffer) < 196` at manage.c:983. `addItemToBuffer`
does not. `forthCapInsertName` does (forth_menu.c:43). So an interactive
capture typed in alpha would accept up to 1024 bytes and then be
rejected by `forthOuterInterpret`'s `n >= FORTH_SOURCE_MAX` check
(forth_compile.c:1595) at ENTER — a silent-until-ENTER failure, and a
divergence from PEM. L1-2 adds the cap to the interactive alpha path
(a `forthCapIsOpen() && origin==interactive` guard around
`addItemToBuffer`, or a post-insert truncation refusal — the packet
picks one; recommendation: guard before the call in
`processKeyAction`'s CM_AIM arm, so the buffer is never over-filled).

---

## T2 — AIM lifecycle

### Open

`fnAim` (src/c47/bufferize.c:10) → `calcModeAim` (src/c47/calcMode.c:62).
Facts that bind L1-1:

- `calcModeAim` sets `calcMode = CM_AIM`, **calls `liftStack()`**, clears
  `FLAG_NUMLOCK`, sets `alphaCase = CAPS_AIM_DEFAULT`, `nextChar =
  NC_NORMAL`, pushes `-MNU_ALPHA`, sets `FLAG_ALPHA`.
- The `liftStack()` matters for L-R2's seed arm: `fnForthOuter` reads the
  string from X, and a lift before the read would move it. **Order for
  L1-1: copy the source out of X → `fnDrop` → open the capture (which
  lifts) → seed `aimBuffer`.** Copy-before-drop is already normative
  (§3.3.2; forth_compile.c:1616-1618); this trace adds copy-before-*open*
  for the same reason.
- `fnAim` clears `aimBuffer`/`T_cursorPos` only when not already in
  AIM/EIM/PEM-alpha (bufferize.c:11-16). The Forth open must clear
  unconditionally then seed, since it may be entered from CM_AIM.

### Close — the seam the Forth ENTER must NOT use

`closeAim()` (src/c47/bufferize.c:2693) does `calcModeNormal();
popSoftmenu();` then **commits `aimBuffer` into X as a `dtString`** (or
`undo()` when empty). `fnKeyEnter`'s `case CM_AIM` (keyboard.c:3513-3560)
does the same commit inline, plus stack lift and `printTraceX`.

**Both are wrong for a Forth capture line.** The interactive ENTER must:
copy `aimBuffer` into a private buffer, run `forthOuterRun(FULL)` on the
copy, and reopen the capture empty (L-R3 REPL) — never touching X except
through the interpreted words themselves. So the divert goes **at the
top of `fnKeyEnter`'s `case CM_AIM`** (keyboard.c:3513) and **at the top
of `fnKeyExit`'s `case CM_AIM`** (keyboard.c:3868), before either
reaches `closeAim`.

The copy is mandatory and already normative: executed words can rewrite
`aimBuffer` mid-line because it is also the NIM buffer (§3.3.2;
src/c47/c47.c:132). `forthOuterInterpret(const char *source)`
(forth_compile.c:1591) already does exactly the right thing — it
`memcpy`s into `ctx.source` before running — so **ENTER calls
`forthOuterInterpret(aimBuffer)` and needs no new copy logic.**

Callers of `closeAim()` that would destroy an open interactive capture,
all needing the origin guard: keyboard.c:1301 (catalog pick — see T6),
keyboard.c:2889 (BST/SST in CM_AIM), keyboard.c:4653 and 4871
(`fnKeyUp`/`fnKeyDown` — see T4), keyboard.c:1063 (INTEGRAL from a
softmenu), plus `fnKeyExit`/`fnKeyEnter` above.

### `aimBuffer` consumers interactively (collision list)

`aimBuffer` is simultaneously the AIM text buffer, the NIM digit buffer,
and TAM's name-entry buffer. Interactively, with a capture open:
`addItemToBuffer` (writes), `addItemToNimBuffer` (writes — only reachable
from `CM_NORMAL`/`CM_NIM`, so not while CM_AIM holds the capture),
`_tamProcessInput` (**zeroes it at commit** — see T3), `fnKeyBackspace`'s
CM_AIM arm (keyboard.c:4361, edits in place), `closeAim`/`fnKeyEnter`
(consume it). Only the TAM one is a genuine hazard, and only under
L-R4 option (c).

### PC-test / scripting entry — the sizeable finding

`fnForthOuter` (forth_compile.c:1604-1620) is driven directly by **51
self-test call sites** in the pattern `x_set_string("…");
fnForthOuter(NOPARAM);` — test_params.part.h (44), test_engine.part.h
(6), test_persist.part.h (1), plus test_dict_reloc.c's sub-phase C
harness (test_dict_reloc.c:245, 1446).

Under L-R2 (always capture) `fnForthOuter` stops interpreting: it opens a
capture instead. All 51 sites would go **silently vacuous** — no error,
no result, assertions failing on stale state. That is a mechanical but
non-trivial re-target and it is **not optional**; it is the gate.

Recommended shape (cheapest, preserves every existing assertion's stack
expectation exactly): keep today's `fnForthOuter` body as a
`FORTH_DEBUG_SELFTEST`-only helper `forthTestRunFromX(void)` in the test
harness (copy X → `fnDrop` → `forthOuterRun(FULL)`), then substitute
`fnForthOuter(NOPARAM)` → `forthTestRunFromX()` at the 51 sites. Do NOT
substitute `forthOuterInterpret(s)` — it never touches X, so the tests
that assert post-run stack depth would silently change meaning.

**This is a packet of its own (call it L1-0) and it must land before
L1-1**, or the battery is red for reasons unrelated to the code under
test. DESIGN.md §3.3.2's "directly callable from PC tests" sentence gets
re-pointed at `forthOuterRun`/`forthOuterInterpret` at stage close.

---

## T3 — TAM outside PEM (closes L-R4 with T1)

### Finding 1 — live execution confirmed, at the line

`_tamProcessInput` (ui/tam.c) records a step **only** under PEM:

```c
if(calcMode == CM_PEM && tam.function != ITM_DELP && lastErrorCode == 0) {
  addStepInProgram(tamOperation());              /* ui/tam.c:1102-1103 */
}
```

and dispatches live otherwise, via the `else { reallyRunFunction(
tamOperation(), value); }` tail (ui/tam.c:1134-1136), with the
`calcMode == CM_PEM` arm explicitly a no-op ("already done",
ui/tam.c:1124-1126). Indirect resolution is likewise gated
`calcMode != CM_PEM` (ui/tam.c:1108). **Confirmed for every item class
that reaches `_tamProcessInput`: outside PEM, TAM commit executes.**

### Finding 2 — the suspend seam is PEM-only, and there is no interactive analog

`tamEnterMode` (ui/tam.c:1148) has exactly one Forth arm:

```c
else if(calcMode == CM_PEM && forthCapIsOpen()) {
  forthCaptureSuspend();                         /* ui/tam.c:1180-1182 */
}
```

Under `CM_AIM` none of `tamEnterMode`'s arms fire (`CM_NIM` no, all three
`CM_PEM` arms no). `aimBuffer` is left **live and unprotected** while TAM
runs.

### Finding 3 — the killer: TAM commit zeroes `aimBuffer` unconditionally

```c
if((tam.mode != TM_NEWMENU) && (tam.mode != TM_STRING)) {
  aimBuffer[0] = 0;                              /* ui/tam.c:1105-1107 */
}
```

For every TAM mode a Forth capture would meet (TM_VALUE, TM_STORCL,
TM_REGISTER, TM_FLAGR/W, TM_LABEL, …) the commit **destroys the capture
line**. This is the fold-piece-2 requirement STAGE_L_INTERACTIVE.md
predicted, now pinned to a line: option (c) cannot be built without an
interactive snapshot store. Not a soft cost — a hard prerequisite.

### Finding 4 — new, not in the L-R4 background: `determineItem` breaks in TAM-from-AIM

`calcMode` does **not** change on TAM entry — `calcModeTamGui()` is a GUI
call and `tam.mode != 0` is the "in TAM" gate (typeDefinitions.h:672-680,
cited in manage.c:1002-1004). So during a TAM entered from a Forth
capture, `calcMode` is still `CM_AIM`, and `determineItem`'s arm at
keyboard.c:1686 fires on its **first disjunct** (`calcMode == CM_AIM`),
which sits *before* the `else if(tam.mode)` arm at keyboard.c:1729 that
selects `key->primaryTam`. Result: **TAM digit keys resolve to letters.**
`STO` `0` `5` would type `O`, `M` — not enter register 05.

Why this has never bitten: **TAM is not reachable from `CM_AIM` today.**
The only softmenu route closes AIM first
(`if(calcMode == CM_AIM && !(isAlphabeticSoftmenu() || …)) closeAim();`,
keyboard.c:1300-1302), so `tamEnterMode` always runs with
`calcMode == CM_NORMAL`. Interactive keys mode would be the **first
caller in the firmware to enter TAM with `calcMode == CM_AIM`.**

### Finding 5 — the AIM line does not render during TAM

The capture line's render arm is gated `!tam.mode`
(packages/forth-core/screen.c:3881, see T5). During a pass-through TAM
the line is invisible — correct behavior for a suspension, but it means
option (c) also needs the suspend/resume UI dance, not just a byte
snapshot.

### Divert list (K's E12 list, re-derived interactively)

Under an open interactive capture in keys mode, these must not execute
live and must be handled explicitly by the T1 arm or its callers:

| Class | Interactive behavior needed | Anchor |
|---|---|---|
| `CAT_FNCT` + `PTP_NONE` | insert `itemCatalogName` as text | T1 arm; forth_menu.c:32 |
| Parameterized (`TM_VALUE`…`TM_CMP`) | **L-R4** | items.c:736 |
| Digits / period | insert as characters | keys-mode column; manage.c:974-988 analog |
| `ITM_EXPONENT` (EEX) | insert `e`, no trailing space | manage.c:956-966 analog |
| numlock translation | guarded off in keys mode | manage.c:969-971 analog |
| `ITM_ENTER` | run the line (L-R3) | keyboard.c:3513 |
| `ITM_EXIT1` | E8 ladder + push to history | keyboard.c:3868 |
| `ITM_BACKSPACE` | edit; on empty → see T4 | keyboard.c:4361 |
| `ITM_AIM` | keys↔alpha toggle (E10/E11) | keyboard.c:1687-1690, manage.c:1718-1734 analog |
| `ITM_SST`/`ITM_BST` | **no PEM analog interactively** — they mean CAPS/NUM lock in AIM (keyboard.c:2887) and close AIM at keyboard.c:2889 | keyboard.c:2886-2892 |
| `ITM_RS` | **no interactive analog** (K's rule records a STOP step; there is no step). Propose: run the line, same as ENTER | keyboard.c:3222-3229 is PEM-only |
| `ITM_UP1`/`ITM_DOWN1` | history recall vs paging — see T4 | keyboard.c:4619, 4837 |

---

## T4 — key semantics inside an open interactive capture

### ENTER

`fnKeyEnter` `case CM_AIM` (keyboard.c:3513). Today: pops the alpha
softmenu, `calcModeNormal()`, commits `aimBuffer` to X as a string with a
stack lift. Forth divert goes first in the arm; it must not reach any of
that. L-R3 REPL: run, then reopen empty (re-enter the capture without
leaving CM_AIM), so the menu pops/`calcModeNormal` are skipped entirely
on the Forth path.

### EXIT ladder

`fnKeyExit` `case CM_AIM` (keyboard.c:3868-3886): if the current menu is
an alpha submenu it pops one level and `stayInAIM()`; otherwise
`closeAim()` + `saveForUndo()`. That is a two-rung ladder already.

The interactive E8-analog ladder, extending K's E12.4 rung:

1. keys mode on → back to alpha input (mirror of keyboard.c:3926-3933)
2. alpha submenu on the stack → `popSoftmenu(); stayInAIM();` (existing,
   keyboard.c:3882-3885)
3. otherwise → push the line onto the history ring (L2, one rule), close
   the capture, leave AIM **without** the `closeAim()` string commit

Rung 3 is the L-R2 "abandon preserves the line" consequence, and it is
the one that must bypass `closeAim`.

### Up / down — the recall-vs-paging collision, resolved

`fnKeyUp` (keyboard.c:4585) and `fnKeyDown` (keyboard.c:4804) share one
shape for `case CM_AIM` (keyboard.c:4627-4660, 4845-4878). In order:

1. `currentMenu() == -ITM_MENU` → programmable menu (keyboard.c:4619)
2. `!arrowCasechange && calcMode == CM_AIM && isJMAlphaSoftmenu(menuId)`
   → `fnT_ARROW(ITM_UP1)` — **the case-change gesture**
   (keyboard.c:4636-4640)
3. `currentSoftmenuScrolls()` → `menuUp()` — **multi-page paging**
   (keyboard.c:4644-4646)
4. else → `closeAim(); fnBst(NOPARAM);` (keyboard.c:4647-4661)

So in the *default* interactive capture state — alpha input with
`-MNU_ALPHA` up — arrow 2 fires: up/down are **case change**, a gesture
users rely on. In keys mode with a scrolling menu, arrow 3 fires. Arrow 4
is destructive (closes the capture and commits to X).

**T4 conclusion: unshifted up/down are NOT available for history
recall.** Both live candidate arms are load-bearing, and the fallback arm
is destructive. The doc's own fallback is the answer: **shifted arrows**
(`f`/`g` + up/down), which resolve through `determineItem`'s
`shiftF ? key->fShiftedAim : …` (keyboard.c:1702-1704) and reach
`fnKeyUp`/`fnKeyDown` only for the unshifted ids. Recommendation for
L1-4: **f-shifted up/down = older/newer recall**, gated on
`forthCapIsOpen() && origin==interactive`, diverted in `fnKeyUp`/
`fnKeyDown` *before* the `case CM_AIM` body (i.e. before keyboard.c:4619,
since the `-ITM_MENU` arm precedes everything). Empty-line gating is NOT
sufficient on its own — case change on an empty line is exactly when a
user reaches for it.

### Backspace on empty

`fnKeyBackspace` `case CM_AIM` (keyboard.c:4361-4396): with
`stringByteLength(aimBuffer) == 0` it does nothing — no close, no exit.
PEM's analog (`pemAlpha(ITM_BACKSPACE)` empty arm, manage.c:992-1007)
deletes the placeholder and closes the capture.

**Recommendation: keep the AIM behavior (inert).** Interactively there is
no placeholder to delete, EXIT is the documented close gesture and the
E8 ladder already gives it three rungs. Making backspace-on-empty close
would add a fourth, undocumented exit that skips the history push.

### R/S

K's E12 rule (commit the line, then record a native `STOP` step,
keyboard.c:3222-3229) is PEM-only by construction. Interactively there is
no step. `ITM_RS` in `CM_AIM` reaches `processAimInput`, falls through,
and would land in the T1 arm as a `CAT_FNCT` item — inserting the text
`R/S`, an unresolvable token. **Recommendation: divert `ITM_RS` to the
ENTER path** (run the line), which is the closest honest analog and
matches the doc's proposal.

---

## T5 — render path

**Zero new display code.** The capture line renders through the existing
AIM arm:

```c
else if(regist == AIM_REGISTER_LINE && calcMode == CM_AIM && !tam.mode) {
  …
  showStringEdC47(multiEdLines, displayAIMbufferoffset, T_cursorPos,
                  aimBuffer, 1, Y_POSITION_OF_NIM_LINE - 3 - checkHPoffset,
                  vmNormal, true, true, false);
  …                                    /* packages/forth-core/screen.c:3881-3900 */
}
```

Same buffer, same cursor variable, same scroll offset the PEM capture
uses. The refresh is already wired: `processKeyAction`'s CM_AIM arm calls
`refreshRegisterLine(AIM_REGISTER_LINE)` (keyboard.c:2896) and screen.c
does the same on long-press (screen.c:1086-1088).

Two consequences:

- The gate is `calcMode == CM_AIM && !tam.mode`. Any design that keeps a
  capture visible during TAM is impossible without new display code —
  reinforcing T3 finding 5.
- Keys mode shows the **underlying** softmenu row (K-R3), which is what
  `_closeAlphaMenus()` already produces in PEM (manage.c:1730). The
  interactive analog pops `-MNU_ALPHA` and reveals whatever is beneath —
  in CM_AIM that is `MyAlpha`/`MyMenu` (softmenuStack[0].softmenuId 0/1,
  forced by `calcModeAim`, src/c47/calcMode.c:88-90). **The row a user
  sees in interactive keys mode differs from PEM's.** Cosmetic, but it
  belongs in the L1-3 packet's expected-render notes so it is not
  mistaken for a bug at sim verification.

---

## T6 — catalogs outside PEM

### Today's behavior

A catalog softkey pressed during interactive alpha runs through
`executeFunction` (keyboard.c:954) and hits:

```c
if(calcMode == CM_AIM && !(isAlphabeticSoftmenu() || isJMAlphaOnlySoftmenu()
                           || item == ITM_KEYMAP)) {
  closeAim();                          /* keyboard.c:1300-1302 */
}
```

`isAlphabeticSoftmenu()` is false for `MNU_FCNS` and for `MNU_FORTH`, so
**picking any function from a catalog while in AIM closes AIM, commits
the text to X, and executes the item live.** That is native behavior and
must stay native when no Forth capture is open.

### Where the divert hooks

Two hooks, both pre-existing shapes:

1. **The FWRD picker**: `forthPickerGuard` (forth_menu.c:65-74) already
   short-circuits at keyboard.c:987 before any of the above. Its gate is
   `calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && tam.function ==
   ITM_FORTH && …` — widen to accept the interactive origin. The
   `menuItem != -MNU_FORTH` identity check at forth_menu.c:67 stays first
   (it is the OOB guard) and needs no change. `pickerInsertName()` writes
   `aimBuffer` and is origin-agnostic; only its `pemAlpha(ITM_NOP)`
   re-commit tail (keyboard.c:989) is PEM-specific and must be skipped
   interactively (there is no step to re-commit).

2. **Ordinary catalog items** (`CAT_FNCT|PTP_NONE` picked from FCNS
   etc.): the `closeAim()` at keyboard.c:1300 must be suppressed when an
   interactive capture is open, letting the item reach `runFunction`
   (keyboard.c:1415) where the T1 arm inserts its name. Guard:
   `!(forthCapIsOpen() && forthCapOriginInteractive())` added to the
   condition at keyboard.c:1300.

### The picker's text-scan section

`forthBuildWordPicker` (forth_menu.c:84) scans from
`forthOwningProgramStart(currentStep)` (forth_menu.c:107). Interactively
`currentStep` points at whatever the PEM cursor last was — **not NULL**.
The doc assumed "correctly empty interactively"; that is true only
because the scan is bounded by `step <= currentStep` and the section is
additive, but a stale `currentStep` from a previous PEM session would
list that program's definitions inside an interactive picker. Harmless
(the names resolve or don't, same as any pick), but **wrong provenance**.

Recommendation for L1-3: gate section (a) on the interactive origin —
skip the text scan entirely when the capture origin is interactive, so
the picker shows exactly the two dictionary sections (b) and (c). One
`if`, and it makes the doc's claim true by construction rather than by
accident.

### `_closeCatalog` and the buried-menu drain

`_closeCatalog` (keyboard.c:459) runs unconditionally at
`noMoreToDo:` (keyboard.c:1462). FIX-9's drain
(`_forthCatalogBuriedOnStack`/`_forthCatalogMenuOnTop`, manage.c:1165-1166,
used at manage.c:1785-1793) is inside `insertStepInProgram`'s
`ITM_FORTH` arm — PEM-only. The interactive open path (L1-1) needs the
same drain when FORTH is picked from a catalog, or the capture opens with
the catalog menu still on the stack. **Same bug class as FIX-9, and it
will recur interactively unless L1-1 carries it.**

---

## L-R4 — evidence summary, recommendation, and the ruling

**RULED 2026-08-04 by the owner on this evidence: (b), build the fold.**
The interactive capture gets the same parameterized-key behaviour as PEM
— pressing STO 0 5 types `STO 05` into the line. The recommendation
below was (a); it is left standing as the record, not relitigated. The
consequence for this stage is a further trace series (§T7) covering the
fold's three pieces, and the packet list in the final section is revised
accordingly.

The evidence the ruling was made on:

| Claim in STAGE_L_INTERACTIVE.md §L-R4 background | Trace verdict |
|---|---|
| "TAM outside PEM executes at commit" | **Confirmed** — ui/tam.c:1102 vs 1134 |
| "No suspend substrate interactively" | **Confirmed** — `tamEnterMode` has no CM_AIM arm (ui/tam.c:1148-1200) |
| "(c) needs the snapshot store, ~256 B + cursor" | **Confirmed and hardened** — `aimBuffer[0] = 0` at ui/tam.c:1105 destroys the line at commit for every relevant `tam.mode`; the store is a hard prerequisite, not a nicety |
| "(c) is 1–2 packets, zero tam.c changes" | **Refuted as stated.** Two additional required changes surfaced: `determineItem`'s AIM-first disjunct (keyboard.c:1686) misroutes every TAM keystroke to the alpha column when `calcMode == CM_AIM` (T3 finding 4), and the render gate `!tam.mode` (screen.c:3881) means the suspension needs UI handling too (T3 finding 5). Interactive keys mode would be the **first** code path in the firmware to enter TAM with `calcMode == CM_AIM` — a genuinely new machine state, on the keyboard resolution path, which is the highest-risk surface in this series. |
| "(a) forecloses nothing" | **Confirmed** — the T1 arm sits before items.c:736 either way; adding the fold later changes one branch of one arm |

**Recommendation was: (a), reject in v1** — on stronger ground than when
the doc was written. The two new findings do not make (c) impossible, but
they move it out of "cheaper than the fold, 1–2 packets, no native
surgery" and into "a new `determineItem` state plus a suspend UI",
i.e. exactly the coupling the doc recommended against taking on in the
stage that also creates the dispatch seam.

**Ruled (b) instead.** The owner's ground is consistency: the fold is
what a Forth capture *means*, and one gesture producing text in PEM and
nothing interactively is a second thing to learn for no gain. Worth
recording that the traces narrowed the gap the ruling had to cross —
findings 4 and 5 are costs **(b) and (c) share**, so the
`determineItem` fix and the suspend store are common infrastructure
rather than (b)-specific overhead. What is uniquely (b)'s is the
non-executing TAM gate and the text-synthesis path, and synthesis is the
piece with genuine unknowns:

- **Piece 1 (non-executing TAM)** is a gating problem. T3 located the
  live-dispatch tail (ui/tam.c:1114-1136) and the PEM step-recording
  branch (ui/tam.c:1102) it must sit beside. Whether one gate covers
  every commit path, or the F6-2 trace's six sites are genuinely
  independent, is T7(a).
- **Piece 2 (suspend store)** is sized by what the round trip clobbers,
  which T3 finding 3 pins at minimum: `aimBuffer` is zeroed at every
  relevant commit (ui/tam.c:1105). T7(e) completes the list.
- **Piece 3 (text synthesis)** is the real unknown, and it is the one
  that decides whether the fold is cheap or expensive. The PEM fold gets
  canonical text for free by decoding a step TAM actually committed. With
  no program to commit into, the question is whether step bytes can be
  built into a **scratch buffer** and handed to `decodeOneStep` — which
  requires that the decoder never reads past the step it was given and
  never assumes the step lives inside program memory. That is T7(c), and
  it is decisive: if the decoder is context-free the fold is cheap, and
  if it is not, the alternatives are extracting a context-free core from
  it or borrowing real program memory for a scratch step — the latter
  being invasive outside PEM. **Re-implementing canonical spelling stays
  rejected on principle** (duplicating the F4 grammar is the parity-bug
  factory F6-4 was built to avoid), so a negative T7(c) result narrows
  the design, it does not open that door.

Packet list under (b): non-executing TAM gate, interactive suspend store,
text synthesis, plus the `determineItem` TAM-precedence fix and a
render/suspension UI pass — and an acceptance battery that drives every
operand class of the F4 grammar through the fold **in PEM and
interactively, asserting identical text**, since parity between the two
is now the stage's contract rather than an accident.

---

## CM_PEM gate audit — the stage's principal regression surface

Every landed Forth gate keyed on `calcMode == CM_PEM`, with a
widen/keep verdict. This table is the L1-5 sweep's checklist; a class
test asserting each verdict is the deliverable.

| # | Anchor | Gate | Verdict |
|---|---|---|---|
| 1 | keyboard.c:1686 | AIM/PEM column selection, with the K1 keys-mode escape only on the CM_PEM disjunct | **WIDEN** — the `CM_AIM` disjunct needs the same escape; also see T3 finding 4 (TAM precedence) if (c) is ruled |
| 2 | keyboard.c:1687-1690 | E10 ALPHA-gesture → `ITM_AIM` resolution | **WIDEN** to the interactive origin |
| 3 | keyboard.c:1300 | `closeAim()` on catalog pick | **WIDEN** (suppress when an interactive capture is open) |
| 4 | keyboard.c:1443 | `popSoftmenu()` after a bufferized softkey | **KEEP** — already covers `calcMode == CM_AIM` unconditionally |
| 5 | keyboard.c:2781 | catalog letter-entry arm | **KEEP** — catalog alpha-selection, not capture text |
| 6 | keyboard.c:3168-3229 | `processKeyAction` CM_PEM arm (SST/BST/RS/dotD capture guards) | **KEEP PEM-only**; the interactive analogs are separate (T4) |
| 7 | keyboard.c:3926-3933 | `fnKeyExit` keys→alpha rung | **MIRROR** into the CM_AIM arm (not widen — different ladder) |
| 8 | keyboard.c:3947-3985 | `fnKeyExit` PEM commit + `currentStep` resync | **KEEP PEM-only** — no steps interactively |
| 9 | keyboard.c:4462-4485 | `fnKeyBackspace` PEM capture arm | **KEEP PEM-only** (T4: interactive backspace stays inert on empty) |
| 10 | items.c:766-802 | PEM step recording in `runFunction` | **KEEP**; the interactive arm is a new sibling before items.c:736 |
| 11 | ui/tam.c:1102 | TAM step recording | **KEEP PEM-only** — this IS the L-R4 asymmetry |
| 12 | ui/tam.c:1180 | `tamEnterMode` capture suspend | **KEEP PEM-only** under (a); new CM_AIM arm under (c) |
| 13 | manage.c:1294 | `pemAlphaEdit` guard | **KEEP PEM-only** — EDIT is a program-step gesture |
| 14 | manage.c:813-830 | `forthCaptureSanitizeRestoredUi` (CM_PEM + ALPHA + `tam.function == ITM_FORTH`) | **WIDEN** — a restored machine may hold an interactive origin; L1-1 extends the sanitizer and the close-paths class test |
| 15 | forth_menu.c:68-73 | `forthPickerGuard` | **WIDEN** to the interactive origin; skip the `pemAlpha` re-commit tail |
| 16 | forth_menu.c:107-177 | picker text-scan section keyed on `currentStep` | **GATE OFF** interactively (T6) |
| 17 | screen.c:902, 1031 | PEM/AIM display gates | **KEEP** — no capture-specific behavior |

---

## Consequences for the packet decomposition

The doc's predicted decomposition survives, with two additions and one
re-order:

- **L1-0 (new, must land first): re-target the 51 `fnForthOuter` test
  call sites** to a `FORTH_DEBUG_SELFTEST` helper preserving today's
  one-shot semantics. Without it the gate cannot distinguish L1's
  regressions from L-R2's intended behavior change. (T2)
- **L1-1** capture origin bit + open/close seams — **plus** the FIX-9
  catalog-drain analog on the interactive open path, and the
  `forthCaptureSanitizeRestoredUi` widen. (T6, gate 14)
- **L1-2** ENTER/EXIT/interpret loop — **plus** the 256-byte/196-glyph
  cap on the interactive alpha insert path, which PEM gets from
  `pemAlpha` and AIM does not. (T1)
- **L1-3** divert seam (`runFunction`, before items.c:736) + catalog/
  picker/keys-mode gates + the picker text-scan gate-off. Under the (b)
  ruling the parameterized branch **falls through** to the TAM-entry
  block at items.c:736 (it does not swallow the item), so L1-3 is
  authorable now and the fold hangs off the existing fall-through.
- **L1-F1..L1-F3 (new, from the L-R4 (b) ruling): the fold.** F1
  non-executing TAM gate; F2 interactive suspend store +
  `determineItem` TAM-precedence fix + the render/suspension UI pass;
  F3 text synthesis. Authored only after §T7 lands, since T7(c) decides
  F3's mechanism. Acceptance drives every F4 operand class through the
  fold in PEM and interactively and asserts identical text.
- **L1-4** history ring — recall gesture is **f-shifted up/down**,
  diverted in `fnKeyUp`/`fnKeyDown` before keyboard.c:4619. Unshifted
  arrows are unavailable (case change / paging / destructive close). (T4)
- **L1-5** acceptance battery + class tests, including the 17-row CM-gate
  sweep above and the close-paths poison sweep extended to the
  interactive origin.

Nothing found in T1–T6 allocates. Under the (b) ruling the RAM cost is
the +512 B BSS history ring (L-R5) plus the interactive suspend store
(order +260 B: a 256-byte line snapshot, cursor, and mode bits), sized
exactly by T7(e). Flash rises with the fold — the stage doc's estimate is
revised to +3–5 KB, measured at close per RULE-1.

---

## T7 — the fold's three pieces (added by the L-R4 (b) ruling)

Pending. Runs as its own trace series before any fold packet is authored,
under the same house rule that produced T1–T6: (a) TAM commit sites
re-derived at current line numbers, and whether one choke point covers
them; (b) whether the (item, operand) → step-bytes encoding can be
produced into a caller-supplied scratch buffer without touching program
memory; (c) whether `decodeOneStep` is context-free enough to render that
buffer — decisive for the synthesis mechanism; (d) the landed PEM
suspend/resume walked statement by statement, separating the
program-memory-bound half from the substrate-independent
decode→text→`forthCapInsertName`→recommit chain; (e) the full clobber
list across an interactive TAM, which sizes the suspend store. Every
finding adversarially refuted before it is built on.
