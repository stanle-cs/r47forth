# Stage L — architect pre-work traces T1–T6

**Status: traces complete 2026-08-04. Evidence for L-R4 is in T1 + T3 +
T5; the ruling is the owner's and is stated as a recommendation at the
end, not decided here. Not normative — this is the evidence sheet
STAGE_L_INTERACTIVE.md's "Mandatory architect pre-work" section
requires before any packet is authored.**

> **SUPERSEDED 2026-08-05 where normative-pending.** Stage L landed;
> DESIGN.md §8.4.2/§8.4.3 (and §8.1, §8.3, §3.3.2, §8.10) carry the
> normative record. This file remains the evidence sheet; where a trace
> disagrees with landed code, the code and DESIGN.md win (one such case:
> T7's forthCaptureSanitizeRestoredUi widen-recommendation, dropped by
> L1-1 rev 2 and corrected in the L1-F3 CM-gate audit, row 14).

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

**Narrowed 2026-08-04 (T8.4 item 4), and RULED — L-R8.** "Zero new
display code" is true, but it does not mean "the stack stays visible".
Verified by hand this pass:

- `showStringEdC47` measures the line in the **large numeric font**
  against `SCREEN_WIDTH - 50` = 350 px (screen.c:1667, defines.h:1510).
  Under it: `multiEdLines = 2`, `yMultiLineEdOffset = 3`. Over it:
  `multiEdLines = 3`, `yMultiLineEdOffset = 1` (small font, up to three
  rows).
- `_refreshNormalScreen`'s alpha arm branches on exactly that value
  (screen.c:5933-5941): `yMultiLineEdOffset == 3` refreshes T, Z, Y and
  then the edit line; otherwise it refreshes **the edit line alone**.
- `AIM_REGISTER_LINE == REGISTER_X` (defines.h:1495), so the editor sits
  on X's row. **X is never visible while composing**, at any line length.

So: short line → T/Z/Y plus the line; past ~350 px in the large font
(order 18–23 glyphs) → the line only. That threshold is reached by
ordinary Forth lines — `1 2 + 3 * DUP SWAP` is already in range.

**L-R8, RULED 2026-08-04: accept native AIM behaviour. No display code.**
Consistency with alpha entry everywhere else on the calculator wins over
a Forth-specific layout. Recorded consequence, so it is not a surprise at
sim verification: interactive Forth's stack visibility degrades to
nothing at ordinary composing lengths, and X — the last result, the
register a REPL user most wants — is hidden throughout. The architect
recommendation was a one-row line with horizontal scrolling
(`displayAIMbufferoffset` already exists for it), which would have kept
T/Z/Y visible at any length; not taken, not relitigated. Nothing in L
forecloses revisiting it as a later additive change, since it is purely a
render decision with no state behind it.

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
| 11 | ui/tam.c:1102 | TAM step recording | ~~KEEP PEM-only~~ **WIDEN-by-bracket** (restated after the L-R4 (b) ruling; the original verdict predates it). F2's `calcMode` bracket forges CM_PEM precisely so this records. |
| 12 | ui/tam.c:1180 | `tamEnterMode` capture suspend | ~~KEEP PEM-only under (a)~~ **WIDEN** — L-R4 ruled (b); F2 C1 rewrites this arm to fire interactively and to call `forthFoldEnter`. |
| 13 | manage.c:1294 | `pemAlphaEdit` guard | **KEEP PEM-only** — EDIT is a program-step gesture |
| 14 | manage.c:813-830 | `forthCaptureSanitizeRestoredUi` (CM_PEM + ALPHA + `tam.function == ITM_FORTH`) | ~~WIDEN~~ **KEEP PEM-only — corrected 2026-08-05.** L1-1 rev 2 dropped the widen deliberately (see its "Why there is no restore sanitizer" section): `forthCap` is process-local and reset at the dictionary seams, so a restore leaves the capture CLOSED, and what remains — `CM_AIM` + `FLAG_ALPHA` + the line in `aimBuffer` — **is** a native alpha session, which upstream restores on purpose (saveRestoreBackup.c:1545-1549). The PEM arm exists only because PEM's residue carries a Forth-specific persisted marker (`tam.function`); the interactive origin has none, so any arm keying on `CM_AIM` + `FLAG_ALPHA` alone would tear down a legitimate restored alpha session. F3's audit flagged the doc/code disagreement — the doc row was the stale half. |
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
  ~~`forthCaptureSanitizeRestoredUi` widen~~ — dropped in L1-1 rev 2, see
  the corrected gate-14 row above. (T6)
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

Method: five independent tracers over the fold's pieces, every
load-bearing claim put through an adversarial refuter, then an architect
pass. **Every claim carried below was then re-verified by hand against
the tree** — the sub-agent findings are evidence, not authority, and one
of them was wrong (noted in T7.6).

### T7.0 — the result: all three pieces collapse into one decision

The stage doc frames the fold as three constructions: a non-executing TAM
variant, an interactive suspend store, and a text-synthesis path. That
framing assumed the interactive fold would have to **emulate** PEM's
fold. It does not have to. It can **run** PEM's fold:

> Materialise a real `ITM_FORTH` capture step in program memory, then set
> `calcMode = CM_PEM` for the duration of `_tamProcessInput` only. The
> landed F6-2/F6-4 machinery then runs unmodified, on a real step, and
> the interactive line gets the same text by the same code.

Each piece then costs approximately nothing:

- **(a) non-executing TAM — zero new gates.** Every commit site in
  `ui/tam.c` *already* has a `calcMode == CM_PEM` arm that records a step
  instead of dispatching (ui/tam.c:1102 is the main one). Making that
  predicate true is the whole mechanism. No `tam.c` commit site is edited.
- **(b) suspend store — +8 bytes, not +260.** With a real capture step in
  program memory, `forthCaptureSuspend`/`forthCaptureResume`
  (manage.c:1180-1290) are reused **verbatim**: the step IS the store,
  exactly as in PEM. The 8 bytes are a save/restore context for the PEM
  cursor, not a copy of the line.
- **(c) text synthesis — nothing to write.** The decoder is handed a real
  in-program step, so the fold splice at manage.c:1240-1256 works as-is.

**This retires the scratch-buffer question that T7 was called to answer.**
The answer would have mattered only if the step lived outside program
memory; it does not, so every decoder hazard the tracers found —
`firstFreeProgramByte` name clamping (decode.c:129-133),
`findKey2ndParam`'s `programBytesAvailable` window
(src/c47/programming/nextStep.c:17), the `forthMarkerTurnsOn` owning-
program walk (decode.c:837) — is moot.

**The encoder-extraction path is rejected outright, not deferred.**
`_insertInProgram` is static (manage.c:716) and performs opcode
substitution itself (manage.c:743-758); several `insertStepInProgram`
arms execute side effects instead of emitting bytes; and `tmpString` is
an implicit *input* to the encoder on some paths. There is no clean
split. [VERIFIED by hand: manage.c:716-775.]

### T7.1 — the bracket, and why it is one site

`_tamProcessInput` is static (ui/tam.c:249) and
**`tamProcessInput` (ui/tam.c:1414) is its only caller** — verified by
grep over both trees this pass. So one wrapper brackets every commit and
cancel path at once:

```c
void tamProcessInput(uint16_t item) {
  const uint8_t savedMode = calcMode;
  const bool_t  brk       = forthFoldArmed();
  if(brk) { calcMode = CM_PEM; }
  _tamProcessInput(item);
  if(brk && calcMode == CM_PEM) { calcMode = savedMode; }
  _tamUpdateBuffer();
}
```

The `calcMode == CM_PEM` re-test on restore is load-bearing: an error
raised inside the commit may have changed `calcMode`, and the epilogue
must not clobber that.

**Why per-site gating was rejected.** Five independent tracers each
produced an *incomplete* enumeration of the commit sites, and they
disagreed with each other — 6 vs 8, 20 vs 39, 3 vs 4 cancel sites. An
enumeration nobody can produce reliably is not a gate anyone can
maintain. The single bracket needs no enumeration at all.

**Why a whole-episode bracket was rejected.** `screen.c` dispatches on
`calcMode` (screen.c:6151) with a `CM_PEM` arm that paints the program
listing, and the TAM overlay branches on `calcMode == CM_PEM`
(screen.c:5637). A bracket spanning the episode would paint PEM under the
TAM prompt. The narrow bracket cannot: no refresh runs inside the commit
path — `_insertInProgram`'s tail calls `scanLabelsAndPrograms()` and
`goToGlobalStep()` (manage.c:770-772), and neither refreshes
[VERIFIED by hand: manage.c:770-775; lblGtoXeq.c:101-140]. The
interactive TAM therefore renders exactly like a native AIM TAM.

### T7.2 — what is materialised, and the one real hazard

`forthFoldEnter` inserts one `ITM_FORTH` capture step seeded with the
live line, using manage.c:941-952's shape verbatim (with `aimBuffer` in
place of `""`), and parks `currentStep` on it — the state
`forthCaptureSuspend` already expects (manage.c:1192-1197, whose comment
documents exactly this contract). `forthFoldLeave` sweeps any residual
step, deletes the capture step, and restores the PEM cursor context via
`goToGlobalStep`.

**This transiently mutates the user's program memory outside PEM.** That
is the design's one genuine cost and it is stated plainly rather than
buried:

- `currentStep` interactively points wherever the PEM cursor was left, so
  the transient steps land inside a real user program.
- Two windows, with different durations — but **both leak live steps.**
  The **capture step** is present for the whole TAM episode (seconds).
  The **TAM step** (e.g. `STO 05`) exists only between its commit and the
  resume splice, both inside one `_tamProcessInput` call — microseconds,
  no user input in between.

  **CORRECTION (2026-08-04, caught by the owner's question).** An earlier
  draft of this section claimed a leaked capture step is benign by
  construction, citing the len=1/NUL placeholder. That is wrong for the
  fold. `forthFoldEnter` seeds the step with the **live line**, and
  `_forthCapBuildStep` emits `len = n` with the text whenever the text is
  non-empty (manage.c:846-859) — the len=1/NUL form is produced *only*
  for empty text. So the fold's capture step is a real `ITM_FORTH` source
  step, and `executeOneStep`'s `ITM_FORTH` arm runs **any** `len > 0`
  step through `forthProgramStep` [VERIFIED by hand:
  lblGtoXeq.c:632-638] — markers are display-only and gate nothing at
  run time. A leaked fold step would therefore silently execute the
  user's half-typed Forth line inside whatever program the PEM cursor
  happened to be parked in.

  Exposure requires a crash or reset inside the TAM window. **PEM carries
  the identical exposure today** — its capture step holds the line for
  the whole episode too — so this is not a new hazard *class*. The
  difference that matters: in PEM the user is deliberately editing that
  program and would see the stray line; interactively it lands somewhere
  they are not looking.

### T7.2a — the scratch program (owner-proposed 2026-08-04)

**Direction: park the interactive fold's steps in a dedicated scratch
program, not at `currentStep`.** Owner's proposal, adopted over the
in-place variant. It fixes the property the earlier mitigation list could
not:

- **A leak becomes identifiable.** Debris inside the scratch program is
  transient by definition, so a boot/restore sweep finally has something
  to key on. Option (iii) above was weak precisely because a leaked step
  is byte-indistinguishable from a legitimate PEM Forth source line —
  *location* supplies the discriminator that the bytes cannot.
- **The user's programs are never structurally touched.** No insert or
  delete inside their code, so their step numbering, labels and cursor
  are not perturbed-and-restored, they are simply left alone.
  `forthFoldLeave`'s restore becomes `goToPgmStep` onto an *unmodified*
  program instead of `goToGlobalStep` onto one that was edited twice.
  Strictly less can go wrong.
- **A leaked step is inert** unless the user deliberately runs the
  scratch program, and a stray program is obvious and deletable in a way
  a stray line buried in program #4 is not.

Feasibility [VERIFIED by hand this pass]: a program boundary is simply an
`END` step — `scanLabelsAndPrograms` increments `numberOfPrograms` on an
`END` whose successor is not `.END.` (src/c47/programming/manage.c:143-146)
— and `insertStepInProgram` already has an `ITM_END` arm
(manage.c:1714). `deleteProgram` (src/c47/programming/manage.c:295-310)
shows the delete shape, including the last-program adjustment
`endOfCurrentProgram - ((currentProgramNumber == numberOfPrograms) ? 2 : 0)`.
We would use raw `deleteStepsFromTo`, not `deleteProgram`, so
`_removeLabelsAssignments()` is not involved — the scratch program has no
labels.

**Cost, in proportion.** Every `_insertInProgram` (manage.c:770) and
every `deleteStepsFromTo` (manage.c:228) calls `scanLabelsAndPrograms`,
which does two full walks of program memory and a
`freeC47Blocks`/`allocC47Blocks` pair for **both** `labelList` and
`programList` (src/c47/programming/manage.c:129-163). So:

| | rescans per folded keypress |
|---|---|
| PEM today | 4 (capture-step recommit + TAM step in/out) |
| interactive, in-place | 4 |
| interactive, scratch program **per fold** | 6 |
| interactive, scratch program **per capture** | 4 |

Creating and destroying the scratch program **once per interactive
capture** rather than once per fold brings it back to parity with what
PEM already pays and nobody has complained about. It also makes the sweep
rule trivial: *no scratch program may exist while no interactive capture
is open.* Recommended shape.

### T7.2b — keep it and grow it: the scratch program IS the history
(owner-proposed 2026-08-04)

**Proposed amendment to L-R5.** Do not create and destroy the scratch
program. Keep it, and let committed lines accumulate in it as steps —
the fold's workspace and L2's line history are then the same object.

This is strictly better than the ruled ring on four counts, and it
deletes a packet:

1. **L2's ring disappears.** No 512 B BSS, no packed `[len][bytes]`
   encoding, no eviction arithmetic, no separate lifetime or poison
   sweep. History is a sequence of `ITM_FORTH` steps.
2. **History persists across save/restore for free.** L-R5 settled for
   NO persistence explicitly because "new save keys + restore validation
   for a convenience buffer is not worth the format surface". Program
   memory already *has* a persisted format and restore validation, so
   this buys persistence at **zero** format cost — strictly better than
   what the ruling settled for, by the ruling's own reasoning.
3. **The leak problem dissolves entirely.** T7.2's hazard was a capture
   step left behind by a crash. If committed lines belong in this
   program, a leftover capture step *is* a history entry — it is where it
   belongs. Only the transient TAM step still needs sweeping, and that
   window is microseconds inside one `_tamProcessInput` call.
4. **History becomes inspectable and editable with landed machinery.**
   It is a program: open it in PEM, scroll it, edit a line, delete
   entries. `pemAlpha(ITM_EDIT)` already turns an `ITM_FORTH` step back
   into an editable capture line (manage.c:892-909), which is most of
   the recall mechanism. T4's open question narrows from "how do we
   store and re-edit history" to "which key browses it".

**The one real cost, verified this pass.** `resizeProgramMemory` claims
blocks from `freeMemoryRegions` (src/c47/memory.c:177-188) — the same
pool `allocC47Blocks` serves, and therefore the same pool the Forth
dictionary allocates from (forth_dict.c:367, 383). So history in program
memory **does** compete with compile capacity, which is precisely what
L-R5's arena-based alternative was rejected for. Two differences make it
a different bargain rather than the same one:

- It is **visible and under user control** — a program they can see,
  count, and delete — not an invisible allocator quietly fighting the
  dictionary.
- It adds **no allocator machinery**: no new lifetime, no ownership, no
  poison sweep. The steps simply exist, in the region already designed
  to hold them.

Against that: growth is not graceful at the limit. `resizeProgramMemory`
on exhaustion calls `backToSystem(NOPARAM)` on DMCP
(src/c47/memory.c:180-182) — it does not fail an insert, it leaves the
app. Unbounded history in a shared pool therefore needs a policy, which
is the one open owner decision (see below).

**Decisions — RULED 2026-08-04 (L-R7):**

- **Growth: soft cap, evict oldest.** A byte budget over the history
  program's own steps; on overflow, `deleteStepsFromTo` the first step
  repeatedly until under budget. One call per evicted line — cheaper
  than the ring's packed-buffer eviction, and bounded by construction,
  so the `backToSystem` exhaustion path is never approached by history
  alone. The user can still clear the program by hand at any time.
  **Proposed budget: 1024 bytes** (≈2× the ruled ring's promise, ≈25–100
  realistic lines at 4 + `len` bytes per step). The number is a tunable
  the acceptance battery pins, not a design invariant; it is reported
  against §5.4 with the stage.
- **Identity: a named, runnable program.** A leading `LBL` gives it a
  name in PROGS, lets `XEQ` reach it, and gives the sweep a
  discriminator. **Running it re-runs the session, deliberately** — that
  is a feature, and it is opt-in by construction since the user must go
  find the program and run it. No execution guard is built: it would be
  new machinery and a new documented rule to prevent something the user
  has to do on purpose.
  **Proposed name: `FHIST`.** It must NOT be `FORTH`: `forthResolveXEQ`
  tries labels before items (items.c:698-718), so a label named `FORTH`
  would shadow the `FORTH` item for `XEQ 'FORTH'`. The chosen name must
  be checked against that resolution order before it lands.
- **Save-file size** grows with history — accepted; it is the user's
  program memory and it is visible to them.

**Binding obligation on the eviction packet — the `leavePem`
use-after-free.** `scanLabelsAndPrograms` frees `labelList` and
`programList` up front (manage.c:129-130) and can early-return on
`ERROR_RAM_FULL` after the free without reallocating
(src/c47/programming/manage.c:151-163), leaving both NULL. `leavePem`
then calls `defineCurrentStep()` unguarded (keyboard.c:2404-2409), which
dereferences `programList[...]` in src/c47/programming/nextStep.c:532 —
a file with **no package override**.

This is an **upstream** defect: upstream logic, upstream failure mode,
and the vulnerable dereference is in a file we do not override. Per the
S1 precedent recorded in `UPSTREAM_REPORTS_globalRegister_reset.md` — a
correct fix evicted from forth-core because it "has nothing to do with
Forth, so it should not ride in this package's patch set" — **we do not
fix it in our overrides.** Carrying an upstream behaviour change inside a
package patch is the same divergence class the dead `_executeOp` block
just cost us.

What Stage L owes instead: L-R7's eviction is what makes it *reachable*
(every eviction is a `deleteStepsFromTo`, hence a
`scanLabelsAndPrograms`, hence a chance to leave both lists NULL). So the
eviction path must not rely on upstream being safe — it **checks
`lastErrorCode` after each `deleteStepsFromTo` and abandons the eviction
loop on `ERROR_RAM_FULL` rather than proceeding**, and it never leaves a
`leavePem` reachable with the lists in that state. That guard lives in
code this stage writes, not in code it inherited. Owner ruled 2026-08-04
not to file the upstream report at this time; the finding is recorded
here so it is not lost.

**Not proposed here:** turning interactive Forth into PEM-on-a-hidden-
program. This amendment changes where committed lines *live*; L1's host
surface (`CM_AIM`, stack visible, AIM line render per T5) is unchanged.
The convergence is worth noticing but is a separate question and would
reopen ruled ground.

Open, for the fold packet to settle before code (measurement, not
reading):

1. Whether an `END` immediately before `.END.` counts as a program at
   all — src/c47/programming/manage.c:144 says an `END` followed by
   `.END.` does **not** increment `numberOfPrograms`, so an *empty*
   scratch program may be invisible while a *non-empty* one is visible.
   That asymmetry decides whether the user ever sees a program count
   change, and it must be pinned by test, not assumed.
2. Arena high-water across a fold, given `scanLabelsAndPrograms`
   allocates on every call and its `ERROR_RAM_FULL` arm bails leaving
   `labelList`/`programList` NULL (src/c47/programming/manage.c:151-163)
   — a failure mid-fold needs a defined outcome. §5.4 discipline applies.
3. The last-program special cases in `goToGlobalStep`/`_getProgramSize`
   (src/c47/programming/manage.c:376-429) all key on
   `currentProgramNumber == numberOfPrograms`; a transient trailing
   program changes those values. This is the regression surface to test.
4. Save/restore while a scratch program exists — it would be persisted,
   and the restore sweep must remove it. New restore-path surface, the
   area that produced `forthCaptureSanitizeRestoredUi`.
5. That `addStepInProgram`'s pre-move still lands the TAM step correctly
   when the capture step's successor is `END` rather than a closing
   `»FORTH` marker (manage.c:2264).
- `scanLabelsAndPrograms()` runs on both insert and delete
  (manage.c:770, :228), so insert-then-delete restores `labelList` and
  step numbering exactly.
- `_insertInProgram` may call `resizeProgramMemory` (manage.c:723-728)
  and `deleteStepsFromTo` does not shrink, so the first fold can
  permanently grow program memory by up to one block. Not cumulative —
  later folds reuse the freed space. Measure and report with the stage.

The alternative — an explicit ~258-byte line buffer — forks
`forthCaptureSuspend`/`forthCaptureResume` into interactive variants
(parity risk on the exact code F6-4 exists to keep single-sourced),
pushes named operands out of scope (they type into `aimBuffer`, which
*is* the line), adds a lifetime and a poison sweep, **and still needs a
program-memory anchor** for `addStepInProgram` to insert against and for
`decodeOneStep` to read. Strictly more work for strictly less coverage.

### T7.3 — admission: FOLD vs PARK

Some TAM classes are out of v1 scope. They do **not** refuse the key and
do **not** lose the line: they take **PARK** — the capture is still
materialised and suspended (so the line survives) but the bracket is not
armed, and the TAM executes live. PARK is precisely the owner-raised
option (c), applied to the minority that cannot fold.

PARK list, each with its reason: `ITM_GTOP` (navigates the program
pointer via unguarded `fnGoto`/`goToPgmStep`, ui/tam.c:888-899 — not an
operand); `ITM_ASSIGN`/`ITM_USERMODE` (zero `aimBuffer`, ui/tam.c:1198);
`ITM_DELP` (already excluded by the PEM commit's own guard,
ui/tam.c:1102); and modes `TM_NEWMENU`, `TM_STRING`, `TM_KEY` (each sets
`FLAG_ALPHA` and/or zeroes `aimBuffer` on its own path, ui/tam.c:1351-1355).

### T7.4 — the `determineItem` fix, and why it is provably safe

One disjunct at keyboard.c:1686:

```c
else if((calcMode == CM_AIM && !(tam.mode && forthFoldPending())) || (catalog && …
```

PEM gets its escape for free because `forthCaptureSuspend` clears
`FLAG_ALPHA` (manage.c:1202) and PEM's disjunct is
`calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && …`. `CM_AIM` has no
such conjunct, so it needs the explicit one.

Safety is provable from the write-set, not from testing:
`forthFoldPending()` reads `forthCap.foldMode`, written at exactly two
places (`forthFoldEnter` sets, `forthFoldLeave` clears);
`forthFoldEnter` has exactly one call site, gated on
`forthCapIsInteractive()`, which is false everywhere before this stage.
**The predicate's value is therefore identical to today's for every
execution that exists today.**

After the stage, in the one new state, parity holds both ways:
`tam.alpha` false → falls through to `key->primaryTam` (the same column a
PEM TAM gets); `tam.alpha` true → the `tam.alpha` disjunct on the same
line still fires → AIM column → letters (again as PEM).

Rejected: the broader `calcMode == CM_AIM && !tam.mode`. It also fixes
what looks like a genuine pre-existing bug — a numeric TAM entered
natively from `CM_AIM` is currently unkeyable — but on a path this stage
has not traced and cannot test. Filed as a separate observation; not
bundled.

### T7.5 — the `PTP_DISABLED` fold hazard: NOT a bug (corrected 2026-08-04)

**First reported here as "a latent PEM bug, shippable now". That was
wrong, and the correction is the useful part.** The two halves are real;
the conclusion was not.

Real half one: `decodeOneStep`'s `case PTP_DISABLED:` (decode.c:917-922)
writes **nothing** to `tmpString` — it printfs under PC_BUILD and breaks.
Real half two: the fold splice trusts it, rejecting only on
`stringByteLength(tmpString) > 255` (manage.c:1244-1245). Both [VERIFIED
by hand].

What was not traced before the claim was made: **what step actually
reaches the fold.** Only `addStepInProgram` → `insertStepInProgram` puts
a step there, and that function's own `case PTP_DISABLED:` switch
(manage.c:1958-2070) never emits a step carrying a `PTP_DISABLED`
opcode. Every arm either rewrites the opcode or emits nothing:

| Item | What is actually committed | Anchor |
|---|---|---|
| `ITM_KEYG`/`KEYX`/`42KEYG`/`42KEYX` | rewritten to `ITM_KEY`/`ITM_42KEY`, which are `PTP_KEYG_KEYX` (items.c:3383) — decodes normally | manage.c:1959-2010 |
| `ITM_GTOP` | nothing inserted (a PC_BUILD printf) | manage.c:2012-2018 |
| `ITM_DELPALL`, `ITM_BST`, `ITM_SST` | nothing inserted — they call `fnClPAll`/`fnBst`/`fnSst` | manage.c:2025-2038 |
| `VAR_ACC`, `VAR_UEST`, `VAR_LEST`, … | rewritten to `ITM_STO 'ACC'` etc. | manage.c:2040-2069 |

So `decodeOneStep`'s `PTP_DISABLED` arm is **unreachable from the fold**,
and the earlier reproducer sketch (KEYG during a capture) does not
reproduce anything: `ITM_KEYG` is indeed `TM_KEY` + `PTP_DISABLED`
(items.c:3384) and does enter TAM, but the step it commits is an
`ITM_KEY` step.

Status: **an undocumented invariant, not a defect.** The fold is correct
only because no `insertStepInProgram` arm emits a `PTP_DISABLED` opcode —
a property of a switch nobody has written down, in upstream-shaped code,
which a future arm could break silently. Proportionate response is
hardening plus a class test that pins the invariant ("no step reaching
the fold may carry a `PTP_DISABLED` opcode"), NOT a bug fix with a
reproducer — there is nothing to reproduce. Carried as a Stage L
hardening item, not as an independent shippable fix.

Method note, since this cost a wrong claim twice: the sub-agent finding
was accurate about both halves and silent about reachability, and I
repeated it before tracing the commit path. Reachability is not a detail
to defer — it is what turns two true facts into a bug or into an
invariant.

### T7.6 — one sub-agent claim corrected

The synthesis reported a dead re-entrant call to `_tamProcessInput` at
ui/tam.c:1347. The actual call there is to **`tamProcessInput`** — the
public wrapper, i.e. the bracketed one. It is inside a `PC_BUILD`-only
block guarded by `forceTamAlpha`, which is **never assigned `true`
anywhere in either tree** (only ever cleared: ui/tam.c:1346,
config.c:1789, src/c47/config.c:1778, src/c47/ui/tam.c:1331), so it is
dead today. But the distinction matters: if that debug aid were ever
enabled, it would re-enter the bracket from inside
`leaveTamModeIfEnabled`, and the nested epilogue would restore
`calcMode = CM_PEM`. The bracket must carry a comment pinning this, and
the fold packet asserts `!forthFoldArmed()` on entry rather than assuming
non-reentrancy.

### T7.7 — still open before code is written

1. Whether L1's landed interactive capture really is `CM_AIM` +
   `FLAG_ALPHA` + line in `aimBuffer`. The whole shape assumes it; T1/T2
   support it but L1 is not built yet. The fold's first packet sets that
   state explicitly in its own fixture rather than assuming it.
2. Whether a softkey press in `CM_AIM` with `tam.mode != 0` can execute
   live before reaching `tamProcessInput` — `executeFunction` has
   `calcMode != CM_PEM` arms above the TAM chain (keyboard.c:1047,
   1053-1060). Settle by reading keyboard.c:1040-1240 as one chain.
3. What paints the AIM register line during `CM_AIM` + `tam.mode`, given
   screen.c:3881 gates it off. Pre-existing native behaviour, not a
   regression, but the fold's look depends on it. Needs a `run-sim`
   capture of a native AIM TAM, not a read.
4. ~~Whether `PTP_DISABLED` is reachable from any TAM-committable item~~
   **CLOSED 2026-08-04: not reachable** — see the corrected T7.5. This
   was the open item that should have blocked the bug claim.
5. `resizeProgramMemory` worst case for the transient pair — needs a
   measurement of `freeProgramBytes` around enter/leave with a worst-case
   named operand, not a read.
6. Whether `TM_MENU`'s path reads a `tmpString` prefix the fold does not
   supply (manage.c:2191-2195 against the inbound snapshot at
   manage.c:1919-1920). If unsafe, `TM_MENU` moves from FOLD to PARK.

## T8 — the PEM-host pivot: investigated 2026-08-04, REJECTED

**Question (owner-directed).** Since history is now a real program of
`ITM_FORTH` steps (L-R7) and the fold already works by temporarily
pretending `calcMode == CM_PEM` (T7), should interactive Forth simply
**be** PEM capture on the `FHIST` program, with the display showing the
stack instead of the listing? If so it would delete the `runFunction`
divert seam, the ~17 gate widenings, the fold bracket and context, and
the `determineItem` fix.

**Verdict: do not pivot. Stay on the AIM host.** One showstopper, and it
is exactly the arbitrary-user-code case.

### T8.1 — the showstopper: item functions test `calcMode` themselves

I had checked that the Forth inner interpreter dispatches C47 items
through `reallyRunFunction` (forth_inner.c:378), **not** `runFunction`,
and concluded that executed words behave identically in PEM. That was
the right check on the wrong layer. The dispatcher is
`calcMode`-independent; **the item implementations are not.**

```c
void fnGoto(uint16_t label) {
  if(tam.mode || calcMode != CM_PEM) {
    …actually jump…                       /* lblGtoXeq.c:17-92  */
  }
  else {
    insertStepInProgram(ITM_GTO);         /* lblGtoXeq.c:94-96   */
  }
}
```

[VERIFIED by hand: packages/forth-core/programming/lblGtoXeq.c:16-97.]

Under the pivot `tam.mode == 0` and `calcMode == CM_PEM`, so a Forth line
that calls a C47 label **writes a GTO step into FHIST instead of
jumping**, and `runProgram` then executes from `currentStep` inside the
history program. Calling a label is among the most ordinary things a user
does from a Forth line.

The escape is a bracket — save `calcMode`, set `CM_NORMAL`, interpret,
restore — and it is disqualifying for three reasons:

1. **It spans arbitrary, re-entrant user code**, not one bounded call. A
   line can `XEQ` a program containing its own `ITM_FORTH` steps;
   `runProgram` polls keys for abort; every error path must restore.
   Contrast T7's bracket, which wraps a single TAM dispatch.
2. **It concedes the thesis.** At the one moment the host mode would
   matter for execution, the host mode must be switched off. The
   record-instead-of-execute semantics the pivot adopts are precisely
   what it must then suppress.
3. **`fnGoto` is one instance of a class that cannot be enumerated
   cheaply.** Because the Forth engine bypasses `runFunction`'s PEM
   divert, the protection the pivot *appears* to inherit does not apply,
   and every item leaf with its own `calcMode == CM_PEM` arm is reachable
   from user text.

Under the AIM host this does not exist: `CM_AIM != CM_PEM`, so
lblGtoXeq.c:17 takes the jump arm.

### T8.2 — the display saving was illusory

My own earlier read — "the pivot costs two edits, because
`_refreshNormalScreen()` already serves CM_NORMAL/CM_AIM/CM_NIM
(screen.c:6192-6227) and the CM_PEM arm is one line (screen.c:6176)" —
was **too optimistic**. To render the stack correctly under `CM_PEM` you
must lie about `calcMode` there too:

- `display.c:234-237` forces `displayFormat = DF_ALL` and clears
  `FLAG_ENGOVR` whenever `calcMode == CM_PEM` — the stack would ignore
  the user's display setting. [VERIFIED by hand.]
- `screen.c:3636` suppresses the X-line solver/probability prefix under
  `calcMode == CM_PEM`. [VERIFIED by hand.]

So the pivot must bracket `calcMode` for the display *and* for execution.
Once both brackets exist it has bought nothing in either place — only the
input seam, which is one dispatch arm.

### T8.3 — the cost the AIM host quietly avoids

PEM capture rewrites the on-disk step **on every keystroke**:
`pemAlpha`'s recommit tail does `deleteStepsFromTo` (manage.c:1104) then
`_insertInProgram` (manage.c:1113) per character, and each of those calls
`scanLabelsAndPrograms`, which walks program memory twice and does a
`freeC47Blocks`/`allocC47Blocks` pair for both `labelList` and
`programList`. Under the pivot, **typing** an interactive line would
churn program memory per character; under the AIM host typing is
`addItemToBuffer` into `aimBuffer` and touches program memory not at all.
Arena cost is binding under CLAUDE.md, and this one is unmeasured.

### T8.4 — adopted from the investigation anyway (the pivot's real yield)

The pivot was rejected; the investigation was not wasted. Each of these
applies to the AIM host and several are corrections to the plan of
record. Anchors from the investigation are marked *(to verify at packet
time)* where I did not re-read them myself this pass.

1. **The fold's step-materialisation anchor is ui/tam.c:907-909**, the
   `case CM_PEM: addStepInProgram(tamOperation())` inside the numeric
   commit switch — not only the `:1102` site T7 cited, which is the
   alpha-buffer branch. [VERIFIED by hand: ui/tam.c:902-917, and a third
   at :929-930.] Both sit inside `_tamProcessInput` and are therefore
   covered by T7's single bracket, so the **design is unchanged** — but
   the packet must cite all three, not one.
2. **`fnKeyEnter`'s CM_AIM arm needs its own explicit seam** in the plan:
   it pops softmenus, calls `calcModeNormal()`, and stores `aimBuffer` to
   X as a `dtString` with a stack lift (keyboard.c:3513-3570). T2 said
   this; the packet list did not carry it as a named seam. Now it does.
3. **The divert seam misses the dynamic-menu / USER RCL-XEQ arms.** T1
   placed the new arm at `runFunction` before items.c:736, but
   items.c:665-735 sits *above* it — the `ITM_RCL`/`ITM_XEQ`
   dynamic-menu arms (items.c:670, :699, :711, :719) and their siblings
   in keyboard.c/screen.c/forth_bridge.c dispatch before the divert is
   reached. Fix: either place the divert above them, or add a
   `forthCapIsOpen()` early-out in `insertUserItemInProgram`
   *(to verify at packet time)*.
4. **T5's "zero new display code" is too strong and must be narrowed.**
   `_refreshNormalScreen`'s AIM arm paints T/Z/Y only when
   `yMultiLineEdOffset == 3` (short lines), and the edit line occupies
   REGISTER_X because `AIM_REGISTER_LINE == REGISTER_X`
   (defines.h:1495). So the AIM host shows T/Z/Y plus the line for short
   lines, and line-only for long ones, with X never visible while
   composing. Still enough to justify L-R2, but **the long-line case
   needs an explicit ruling** rather than an assumption *(to verify at
   packet time)*.
5. **`ITM_PR` leaks an open capture**: keyboard.c:3169-3176 calls
   `leavePem(); calcModeNormal(); extractPFNMenus();` with no
   `forthCapClose()`. Needs the guard on either host *(to verify at
   packet time)*.
6. **FHIST recall must copy the step's text, not execute the step.**
   Payload is at `step + 4` for `step[3]` bytes and is **not**
   NUL-terminated (`_forthCapBuildStep`, manage.c:846-859). Do not use
   `+3` — that is the length byte.
7. **Two bugs found in passing, independent of Stage L** *(both to verify
   at packet time)*: a `leavePem` use-after-free — `scanLabelsAndPrograms`
   frees `labelList`/`programList` and can early-return on RAM_FULL,
   after which `leavePem` → `defineCurrentStep` dereferences
   `programList[...]`, reachable once L-R7's eviction runs at the cap;
   and a `.d` keystroke silently dropped at keyboard.c:3227 when the line
   is non-empty.

### T7.8 — known v1 limitation, recorded not fixed

Catalog-driven TAM commits bypass `tamProcessInput` entirely:
keyboard.c:1148 and :1160 gate three commit sites on
`calcMode == CM_PEM`, so the bracket never reaches them. Interactively
they do not fire. The capture is never lost (the step is the store), but
a flag-by-name or dynmenu-label pick during an interactive TAM will not
fold. Deferred to a follow-on packet.

---

## T9 — the stack lift: opening a capture destroys X (found 2026-08-04)

Found while verifying the L1-2 review. It is the most consequential
finding of the packet round because it is a **design** error, not a spec
slip, and it invalidates an assertion in the already-landed L1-1 packet.

`fnForthOuter`'s interactive open calls `fnAim(NOPARAM)` →
`calcModeAim` (src/c47/calcMode.c:62), whose body includes:

```c
    if(!tam.mode && calcMode != CM_ASSIGN && calcMode != CM_PEM && calcMode != CM_ASN_BROWSER) {
      calcMode = CM_AIM;
      liftStack();                                    /* calcMode.c:76 */
```

and `liftStack` (src/c47/stack.c) ends **unconditionally** with

```c
  setRegisterDataPointer(REGISTER_X, allocC47Blocks(REAL34_SIZE_IN_BLOCKS));
  setRegisterDataType(REGISTER_X, dtReal34, amNone);
```

[VERIFIED by hand.] So opening the capture replaces X with a fresh,
**uninitialised** `dtReal34` — pushing the old X to Y when `FLAG_ASLIFT`
is set, and freeing it outright when it is not.

**Why this is fatal rather than cosmetic.** The interactive capture's
whole premise is that the line operates on the live stack: L-R2 already
rules that a seeded string is *dropped* at seed "so interpreted words see
a clean stack". If opening the capture lifts, then with 16 in X a user
who types `1 +` and presses ENTER computes `garbage + 1`. The feature
would be wrong on its most ordinary use.

**Consequence: the interactive open must NOT route through
`calcModeAim`'s lifting arm.** `calcModeAim` is upstream and not
overridden, so L1-1 C2 cannot simply call `fnAim`. The options, in
preference order:

1. **Inline the non-lifting equivalent** in `fnForthOuter` — everything
   `calcModeAim` does except `liftStack()`: `alphaCase = CAPS_AIM_DEFAULT`,
   `nextChar = NC_NORMAL`, clear `FLAG_NUMLOCK`, `scrLock = NC_NORMAL`,
   `calcMode = CM_AIM`, `clearRegisterLine(AIM_REGISTER_LINE, true, true)`,
   the cursor variables, `showSoftmenu(-MNU_ALPHA)`, the
   `softmenuStack[0].softmenuId` 0→1 normalisation, `setSystemFlag(FLAG_ALPHA)`,
   `calcModeAimGui()`. About ten lines, and it is the only option that
   leaves the stack demonstrably untouched.
2. Call `fnAim` then repair — rejected: the repair is conditional on
   `FLAG_ASLIFT` (with it set, drop; with it clear, the old X is already
   freed and unrecoverable), so it cannot be made correct.
3. Add a `bufferize.c`/`calcMode.c` override — rejected: new upstream
   patch surface for one call, against the S1 discipline.

**Assertions this invalidates.** L1-1's T1.2 ("non-string X, X untouched")
and T1.5 ("oversize: X still holds the string") are correct only *after*
this fix; as landed they would have been written against a lifting open
and would have masked it. L1-2's C5.4 ("X unchanged after EXIT") likewise.
The review's own L1-2 B1 — restore the pre-FORTH X via `undo()` at EXIT —
is treating the symptom: with option 1 there is nothing to restore, and
the undo slot is not consumed (which is the review's own owner question 2,
answered by construction).

**Standing lesson, added to the packet-authoring checklist:** a spec that
says "call the landed entry point" must state what that entry point does
to the *machine state the feature depends on*. `fnAim` was cited for the
mode and the menu; nobody asked what it did to the stack.
