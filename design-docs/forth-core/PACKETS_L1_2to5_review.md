# REVISION LIST — Stage L packets

Line numbers in "NOW" are packet line numbers; all code anchors were read this pass.

---

## PACKET_L1_2_enter_exit_repl.md

### BLOCKERS

**B1 — C2 rung 3 (packet lines 131-143): the teardown never resolves the X placeholder that opening the capture created.**
- NOW: `forthCapClose(); aimBuffer[0]=0; T_cursorPos=0; displayAIMbufferoffset=0; calcModeNormal(); popSoftmenu(); clearSystemFlag(FLAG_ALPHA); break;`
- MUST SAY: rung 3 resolves the placeholder before closing — insert `undo();` (the `closeAim` empty-buffer branch, `src/c47/bufferize.c:2697-2701`) as the first teardown statement, plus `updateMatrixHeightCache(); saveForUndo();` (the pair the native arm runs at `packages/forth-core/keyboard.c:3878-3879`) or one sentence per omission saying why it is skipped. Then rewrite C5.4 (packet line 222-223) from "**X unchanged**" to "X holds the *pre-FORTH* value, read back through the landed `read_reg_int32` idiom", and re-aim mutation 2 (packet line 243) at that oracle.
- ANCHOR: `src/c47/bufferize.c:18` `fnAim` → `calcModeAim(NOPARAM)`; `src/c47/calcMode.c:75-76` `calcMode = CM_AIM; liftStack();`; `src/c47/stack.c:35-36` — `liftStack` unconditionally ends `setRegisterDataPointer(REGISTER_X, allocC47Blocks(REAL34_SIZE_IN_BLOCKS)); setRegisterDataType(REGISTER_X, dtReal34, amNone);` so X is destroyed and replaced with uninitialised blocks. Undo snapshot exists: `packages/forth-core/items.c:4771` (ITM_FORTH row is `US_ENABLED`) → `items.c:304 saveForUndo();`.
- REACHABILITY: this is the primary path — FORTH from `CM_NORMAL`, then EXIT. Nothing between rung 3's entry and the close touches X. Verified that EXIT in `CM_AIM` does reach `fnKeyExit`: `processKeyAction`'s own `case ITM_EXIT1` (`keyboard.c:2571-2604`) has no `CM_AIM` branch and never sets `keyActionProcessed`, so the key falls through to `fnKeyExit`.

**B2 — C2 rung 3 (packet line 140-141): `calcModeNormal(); popSoftmenu();` pops one level too many.**
- NOW: `calcModeNormal(); popSoftmenu(); /* drop -MNU_ALPHA */`
- MUST SAY: either (a) `if(currentMenu() == -MNU_ALPHA) { softmenuStack[0].softmenuId = 1; } calcModeNormal(); popSoftmenu();` — the native normalisation the ladder bypasses — or (b) `calcModeNormal();` alone with the trailing `popSoftmenu()` deleted. Pick one in the packet; do not leave it to the implementer. Also delete `clearSystemFlag(FLAG_ALPHA);` (packet line 142) as redundant or say why it is kept.
- ANCHOR: `src/c47/calcMode.c:44-46` — `calcModeNormal` already pops when `-MNU_ALPHA` is on top; `src/c47/calcMode.c:53` already clears `FLAG_ALPHA`. The native arm avoids the double pop only via `packages/forth-core/keyboard.c:3869-3871` (`if(currentMenu() == -MNU_ALPHA) { softmenuStack[0].softmenuId = 1; }`), which the divert — the first statement of `case CM_AIM:` at `keyboard.c:3868` — skips.
- REACHABILITY: `-MNU_ALPHA` is genuinely on top at rung 3: pushed by `src/c47/calcMode.c:86`, re-pushed by rung 1, and rung 2 cannot consume it (`packages/forth-core/softmenus.c:3880-3891` — `isAlphaSubmenu` lists `-MNU_MyAlpha`, the ALPHA_OMEGA/MATH/MISC/INTL family and `-MNU_FORTH`, **not** `-MNU_ALPHA`). User-visible loss: a STK/FIN/MATX menu open before FORTH is gone after EXIT.

**B3 — C4 (packet lines 196-203): the cap guard indexes `indexOfItems[]` with a negative item.**
- NOW: `if(forthCapIsInteractive() && indexOfItems[item].func == addItemToBuffer && !(…)) {`
- MUST SAY: `if(forthCapIsInteractive() && item > 0 && indexOfItems[item].func == addItemToBuffer && …)`. State that the `item > 0` conjunct is load-bearing, not defensive.
- ANCHOR: `src/c47/assign.c:46` — key 85 row has `fShiftedAim == -MNU_AIMCATALOG`; `packages/forth-core/keyboard.c:1686` returns it in `CM_AIM`; `keyboard.c:1976` `processKeyAction(item)` is called for any `item != ITM_NOP/ITM_NULL`; the `switch(item)` at `keyboard.c:2496` has no case for it (`case`s enumerated: 2497, 2519, 2545, 2571, 2607-2609, 2629, 2682, 2695, 2708, 2715, 2722, 2733), so it lands in `default:` at 2755 → `switch(calcMode)` at 2830 → `case CM_AIM:` at 2886, where the new guard evaluates `indexOfItems[-MNU_AIMCATALOG]`. `&&` short-circuits after `forthCapIsInteractive()`, so this fires exactly when a capture is open — f-shift + `+` is the AIM catalog gesture. The landed `keyboard.c:2794` has the same shape (`indexOfItems[item].func == addItemToBuffer || item < 0`); say in the packet that it is a latent upstream wart, not the house pattern to copy.

### MAJORS

**M1 — C4 (packet lines 191-193): "Guard at the call site" covers only the physical-key seam; softkey insertion is uncapped.**
- MUST SAY: add the identical guard immediately before `runFunction(item)` at `packages/forth-core/keyboard.c:1415`, gated `calcMode == CM_AIM && forthCapIsInteractive() && item > 0 && indexOfItems[item].func == addItemToBuffer`, or factor both into one helper. C5.7 (packet line 229-230) must drive the cap through **both** seams.
- ANCHOR: `keyboard.c:1443-1445` — `else if(((calcMode == CM_PEM && …) || calcMode == CM_AIM) && indexOfItems[item].func == addItemToBuffer) { popSoftmenu(); }` is the live `CM_AIM` softkey-character tail; `keyboard.c:1300-1302` confirms alphabetic softmenu presses in `CM_AIM` do not `closeAim()` and fall through to `runFunction`. `src/c47/bufferize.c:466`-bounded `addItemToBuffer` is the only limit on that path.

**M2 — C2 rung 2 (packet lines 124-130): wrong anchor, narrower predicate than the native rung, and a missing repair call.**
- NOW: "isAlphaSubmenu(0) is the landed test (keyboard.c:3882 uses the same in the native arm)" + `if(isAlphaSubmenu(0)) { popSoftmenu(); break; }`
- MUST SAY: (i) correct the anchor to `packages/forth-core/keyboard.c:3936` and label it as the **CM_PEM** arm — `keyboard.c:3882` is `goto undo_disabled;` and the native `CM_AIM` arm (`keyboard.c:3868-3889`) contains no `isAlphaSubmenu` call at all; (ii) state which predicate rung 2 uses — keep `isAlphaSubmenu(0)` and justify the narrowing against the native `softmenuStack[0].softmenuId <= 1 && menu(1) != -MNU_ALPHA` (`keyboard.c:3873`), or adopt the native test; (iii) add `stayInAIM();` after `popSoftmenu();` (the native pair is `keyboard.c:3885-3886`) or say why the interactive capture does not need `changeToALPHA(); setSystemFlag(FLAG_ALPHA); refreshModeGui()`.
- CONSEQUENCE OF (ii) AS WRITTEN: a non-alpha menu stacked over an interactive capture falls to rung 3 and **closes the capture**, discarding the line, where native AIM pops and stays. Extend C5.6 (packet line 227-228) with that case.

**Anchor correction (non-blocking):** C1 (packet line 76, line 188) cites `forth_compile.c:1591-1603` / `:1595`; the `n >= FORTH_SOURCE_MAX` check is at `forth_compile.c:1597`, the memcpy at `:1601`.

### VERDICT — implementable after edits (B1-B3, M1-M2 are local text changes; the mechanism stands).

---

## PACKET_L1_3_divert_seam.md

### BLOCKERS

**B1 — C1 "The dynamic-menu hole" (packet lines 64-77), Mutation 6 (line 234), C6.5 (lines 213-215): the early-out is placed in a sink that is unreachable interactively.**
- NOW: "Close it at the shared sink instead of at six call sites: add to `insertUserItemInProgram` (manage.c:2229) a first-statement early-out".
- MUST SAY: close it at the **decision**. Each site becomes `if(calcMode == CM_PEM) { insertUserItemInProgram(item, funcParam); } else if(forthCapIsInteractive()) { (void)forthCapInsertName(funcParam); } else { reallyRunFunction(…); }`, or one shared helper `forthUserItemDispatch(...)` called at all nine. If a sink-side guard is kept at all, spell it `forthCapIsInteractive() && calcMode != CM_PEM` and label it unreachable-today belt-and-braces; Mutation 6 and C6.5 must not depend on it (removing dead code cannot go RED).
- ANCHOR (all nine sites are `if(calcMode == CM_PEM) {insert} else {execute}`, and interactively `calcMode == CM_AIM`): `packages/forth-core/items.c:670/674`, `:699/703`, `:711/715`, `:719/723`; `packages/forth-core/keyboard.c:2259/2269`, `:2283/2293`; `packages/forth-core/screen.c:818/822`, `:836/840`; `packages/forth-core/forth_bridge.c:30/34`. Sink's only entry point: `packages/forth-core/programming/manage.c:2229`.

**B2 — C2 (packet lines 79-97): escaping the `CM_AIM` disjunct drops interactive keys mode off the end of the mode chain → bug screen on every key.**
- NOW: only the `keyboard.c:1686` edit is specified.
- MUST SAY: the escape is one half of an atomic two-part edit. Also add to the normal-column branch at `keyboard.c:1726`: `|| (calcMode == CM_AIM && forthCapIsInteractive() && forthCapKeysMode())`, using the identical predicate so the two cannot disagree. State that dropping either half produces the bug screen, and make it a mutation.
- ANCHOR: chain is `keyboard.c:1686` (escaped) → `:1723 else if(tam.mode)` (false) → `:1726` normal column, which lists `CM_PEM` (why K1 works) but **not** `CM_AIM` (`GRAPHMODE` is `CM_PLOT_STAT || CM_GRAPH`, `packages/forth-core/defines.h:2338`) → `:1737 displayBugScreen(bugScreenItemNotDetermined);`.
- Also delete or rewrite the "Safety is provable from the write-set" paragraph (packet lines 95-97): the claim is true and irrelevant — the defect is in the paths the escape newly opens, not in existing executions.

### MAJORS

**M1 — C1/C4: `items.c` has no declaration route to the four capture symbols.**
- MUST SAY: add to C1 an explicit instruction — insert `#include "forth_capture.h"` and `#include "forth_menu.h"` into `packages/forth-core/items.c` after `forth_dict.h`, with the L1-1 C2 rationale verbatim, plus a gate grep for it.
- ANCHOR: `packages/forth-core/items.c:4-5` are the only includes (`c47.h`, `forth_dict.h`); `forth_dict.h:9-10` is `<stdbool.h>/<stdint.h>` only; `forth_menu.h:15` `bool_t forthCapInsertName(const char *name);` and `forth_capture.h:58-59` `forthCapKeysMode`/`forthCapSetKeysMode` are the declarations that would be missing. The file's own `items.c:8-9` hand-declares `fnForthOuter`/`fnForthCall`, which is precisely the pattern not to repeat.

**M2 — C1 snippet (packet line 72): `name` does not exist in that function.**
- NOW: `(void)forthCapInsertName(name);`
- MUST SAY: `(void)forthCapInsertName(funcParam);` — and only as the optional defensive guard once B1's fix is adopted.
- ANCHOR: `packages/forth-core/programming/manage.c:2229` `void insertUserItemInProgram(int16_t func, char *funcParam) {`.

**M3 — C4 (packet lines 130-141): one `popSoftmenu()` is not the twin of `_closeAlphaMenus()`.**
- MUST SAY: specify the bounded drain up front rather than "verify by test": `for(int i = 0; i < SOFTMENU_STACK_SIZE; ++i) { if(!isAlphaSubmenu(0) && currentMenu() != -MNU_ALPHA) break; popSoftmenu(); }`. Note explicitly that this is **not** byte-for-byte PEM parity: `_closeAlphaMenus` has no `MNU_FORTH` case and returns without popping when FWRD is on top, so "the twin" is undefined for FWRD and the packet must state the interactive choice. Extend C6.3 (packet lines 206-208) with a second leg: open FWRD, toggle to keys mode, assert the current menu is neither `-MNU_ALPHA` nor an alpha submenu.
- ANCHOR: `packages/forth-core/programming/manage.c:776-800` — `_closeAlphaMenus` is a `SOFTMENU_STACK_SIZE`-bounded drain over ALPHAINTL/ALPHAintl/ALPHAMATH/ALPHA_OMEGA/alpha_omega/ALPHA/MyAlpha, with `default: return;`. `packages/forth-core/softmenus.c:3880-3891` — `isAlphaSubmenu` includes `-MNU_FORTH` ("FWRD is an ALPHA submenu"), so an alpha submenu above `-MNU_ALPHA` is reachable in the very state C6.5 tells the tester to create.

**Gate correction (packet line 21):** `grep -n "      if(calcMode == CM_PEM) {" packages/forth-core/items.c   # expect one at ~766` returns **five** lines (670, 699, 711, 719, 766). Under "STOP on mismatch" a literal implementer stops. Change the comment to "expect five; the load-bearing one is 766".

### VERDICT — implementable after edits. B1 changes the shape of the C1 work (nine sites or a helper), so re-check C6.5 and Mutation 6 in the same pass.

---

## PACKET_L1_H_history_program.md

### BLOCKERS

**B1 — C4 (packet line 161): the specified divert site is unreachable for the f-shifted arrows.**
- NOW: "Divert them in `processKeyAction`'s `case CM_AIM`, beside L1-2's R/S guard".
- MUST SAY: spec two new arms in the **item** switch — `case CHR_caseUP:` / `case CHR_caseDN:` guarded by `forthCapIsInteractive()`, placed immediately before `packages/forth-core/keyboard.c:2682`, falling through to the landed case-change body when the guard is false. Delete the "beside L1-2's R/S guard" sentence: ITM_RS and the arrows reach `processKeyAction` by different arms.
- ANCHOR: `keyboard.c:2496 switch(item)`; `:2682 case CHR_caseUP:` and `:2695 case CHR_caseDN:` each end `keyActionProcessed = true; break;`; only `:2755 default:` reaches `:2830 switch(calcMode)` → `:2886 case CM_AIM:`. Verified ITM_RS has no case in 2496-2755 (cases: 2497, 2519, 2545, 2571, 2607-2609, 2629, 2682, 2695, 2708, 2715, 2722, 2733), which is why L1-2's R/S guard works and this one would not. Key rows: `src/c47/assign.c:27` (`CHR_caseUP` in field 7) and `:32` (`CHR_caseDN`), field 7 = `fShiftedAim` per `src/c47/typeDefinitions.h:66`.

**B2 — C1 `forthHistoryEnsure` (packet lines 52-59): insert order plus an ambiguous cursor position appends FHIST's label to the user's last program.**
- NOW: "position at the end of program memory / insert: LBL 'FHIST' / insert: END".
- MUST SAY: name the byte position and the order. Either (a) `currentStep = firstFreeProgramByte; currentProgramNumber = numberOfPrograms;` then LBL then END; or (b) park on the last program's `END` step and insert **END first, then LBL**. Add to C5.1 an assertion that `forthHistoryProgram()` differs from the pre-existing user program number and that the user's last program's bytes are unchanged (C5.9 must compare before/after bytes explicitly, not just "byte-identical" as prose).
- ANCHOR: `packages/forth-core/programming/manage.c:735` — `for(uint8_t *pos = firstFreeProgramByte + 1 + size; pos > currentStep; --pos) *pos = *(pos - size);` then the write at `:759-767`: insertion is **before** the step `currentStep` points at. `manage.c:2264` — `addStepInProgram`'s pre-move stops on END/.END., i.e. the landed convention parks the cursor **on** the END step. `src/c47/programming/manage.c:143-146` — a program is counted at an END whose successor is not `.END.`; `src/c47/programming/manage.c:177` — `labelList[…].program = numberOfPrograms` is taken at the LBL's position, so an LBL inserted before the last END resolves to the *user's* program.

**B3 — EXECUTION GATE line 24: the grep can never match.**
- NOW: `grep -n "bool_t forthStepPayload" packages/forth-core/forth_bridge.c`
- MUST SAY: `grep -n "forthStepPayload" packages/forth-core/forth_bridge.c` (expect `:60` plus call sites `:176`, `:215`).
- ANCHOR: `packages/forth-core/forth_bridge.c:60` `bool forthStepPayload(const uint8_t *step, uint8_t *lenOut)`. Ran the gate: 0 matches for the packet's pattern, under a "STOP on mismatch" header.

### MAJORS

**M1 — C2 (packet lines 77-89): `savedGlobalStep` is not a stable restore key.**
- MUST SAY: save `(currentProgramNumber, currentLocalStepNumber)` and restore with `goToPgmStep(savedProgram, savedLocalStep)`, remapping `savedProgram` if the ensure created a program at a lower index. Add to C5.5 that the restore test must run with FHIST **before** the caller's program, not only after.
- ANCHOR: `packages/forth-core/programming/lblGtoXeq.c:120-133` — `goToGlobalStep` resolves by counting from `defineCurrentProgramFromGlobalStepNumber`; `src/c47/programming/manage.c:392` — program boundaries are themselves global step numbers and all shift when FHIST grows or evicts. The landed suspend keys off a byte offset for exactly this reason: `packages/forth-core/programming/manage.c:1191` `uint32_t stepOff = (uint32_t)(currentStep - beginOfProgramMemory);`. `goToPgmStep` is at `lblGtoXeq.c:155`.

### VERDICT — implementable after edits, but B2 is a spec decision the packet currently defers; write the chosen order into the text before handing it over.

---

## PACKET_L1_F1_fold_context.md

### BLOCKERS

**B1 — C3 line 111 vs C4 line 139: `entryStepCount` is sampled in the caller's program and compared against FHIST's count.**
- NOW: sample at C3 line 111 (before "position the cursor on FHIST's last step" at line 113); compare at C4 line 139.
- MUST SAY: move the sample to **after** repositioning onto FHIST — immediately before the `_insertInProgram` at C3 line 117 — and state that both the sample and the C4 comparison are taken with `currentProgramNumber == FHIST`. Add a C6 assertion that FHIST's count is unchanged across enter+leave when FHIST holds ≥2 history steps and the caller's program is shorter.
- ANCHOR: `packages/forth-core/programming/manage.c:2374-2387` — `getNumberOfSteps()` is entirely keyed on `currentProgramNumber` (last-program arm walks from `programList[currentProgramNumber-1].instructionPointer`; else `abs(programList[n].step - programList[n-1].step)`). There is no global reading. C6.2 (packet line 178) explicitly drives an empty caller program (`entryStepCount == 1`) while FHIST is ≥3 — the sweep runs and eats real history. None of C6.1/6.2/6.3/6.4/6.6 can catch it (they assert on the *caller's* program, or between enter and leave, or accept "count returns to entry", which the runaway also satisfies).

**B2 — C3 lines 103-123: `pemCursorIsZerothStep` is saved but never normalised.**
- NOW: `forthFoldCtx.savedZerothStep = pemCursorIsZerothStep` (line 110), inbound value left live.
- MUST SAY: add one line after the save — `pemCursorIsZerothStep = false;` — with the reason (a parked capture step is a real step, never the zeroth-step pseudo-position). C4 already restores the saved value. Add a C6 case that sets `pemCursorIsZerothStep = true` before `forthFoldEnter` and asserts the TAM step lands **after** the capture step.
- ANCHOR: `packages/forth-core/programming/manage.c:2264` — `if((!pemCursorIsZerothStep) && (… || tam.mode) && !isAtEndOfProgram(currentStep) && !isAtEndOfPrograms(currentStep)) { currentStep = findNextStep(currentStep); … }`: the pre-move the packet's own park contract depends on (`manage.c:1194-1196`) is gated on it. Consequence: the TAM step commits *before* the capture step, the offset-derived pointer in `forthCaptureResume` (`manage.c:1210-1213`) reads the TAM step, the canary falsifies and the capture is abandoned. `pemCursorIsZerothStep` is a persistent global with no reset on leaving PEM (set at `src/c47/programming/nextStep.c:325`/`:399`, `packages/forth-core/keyboard.c:4486`/`:4504`, `manage.c:517`; cleared only at `manage.c:1289` and `ui/tam.c:890`).

**B3 — C1 line 51 and C6.7 (packet lines 188-189): clearing `foldMode` "at the same E14 reset sites as keysMode" disarms the fold mid-flight.**
- NOW: `"Transient; cleared at the same three E14 reset sites as keysMode and origin."` + C6.7 "Assert `foldMode` is cleared by `forthCapClose()`, `forthCapAbandonSuspended()` and `forthCapPowerReset()`."
- MUST SAY: `foldMode` is owned by `forthFoldEnter`/`forthFoldLeave` and by `forthCapPowerReset()` **only**. `forthCapOpen()`, `forthCapClose()` and `forthCapAbandonSuspended()` must leave it untouched. Rewrite C6.7 accordingly, and state the invariant: `forthFoldLeave()` must be able to run its sweep-and-restore after a resume that abandoned the suspension.
- ANCHOR: `keysMode` has **four** clear sites, not three — `packages/forth-core/forth_capture.c:10` (`forthCapOpen`), `:16` (`forthCapClose`), `:38` (`forthCapAbandonSuspended`), `:61` (`forthCapPowerReset`). `forthCaptureResume` calls `forthCapOpen()` at `packages/forth-core/programming/manage.c:1224` (saving/restoring keysMode around it at `:1220`/`:1226`) and calls `forthCapAbandonSuspended()` on the canary path at `manage.c:1214`. F2's C2 runs `forthCaptureResume(); forthFoldLeave();` in that order, and F1's C4 line 133 is `if forthCap.foldMode == 0: return` — so under the packet as written the fold never unwinds: the transient step stays in FHIST and the PEM cursor globals stay pointing into it.

### MAJORS

**M1 — C4 line 140: the debris sweep has no cap, no progress check, and no in-FHIST guard.**
- MUST SAY: bound it (`for(int i = 0; i < 4 && …; i++)`), snapshot and clear `lastErrorCode` before the loop, and guard each iteration: `uint8_t *victim = findNextStep(currentStep); if(victim == NULL || isAtEndOfProgram(victim) || isAtEndOfPrograms(victim)) break;` then delete only `[victim, findNextStep(victim))`.
- ANCHOR: `packages/forth-core/programming/manage.c:221-227` — `deleteStepsFromTo` is a silent no-op when `from == to` (`opSize = to - from`, `xcopy(from, to, …)`), so the `while` can spin. `src/c47/programming/nextStep.c:151-157` — `findNextStep` returns NULL on NULL and via `findKey2ndParam` (`:172-178`), and the NULL propagates into the pointer subtraction. The break is on `lastErrorCode`, which `deleteStepsFromTo` only forwards from `scanLabelsAndPrograms` and which is not pre-cleared. Landed precedent for the bound is in the same file: `manage.c:1279-1282` ("so never spin on the predicate").

**M2 — C4 line 143: the capture-step delete trusts a raw `currentStep` that survived an arbitrary TAM.**
- MUST SAY: add `uint32_t capStepOffset;` to `forthFoldCtx_t`, set from `(uint32_t)(currentStep - beginOfProgramMemory)` right after the C3 park; in C4 re-derive `uint8_t *cap = beginOfProgramMemory + capStepOffset;` and require `cap < firstFreeProgramByte && checkOpCodeOfStep(cap, ITM_FORTH) && cap[2] == (uint8_t)STRING_LABEL_VARIABLE` before deleting. On failure: skip the delete, still restore the cursor, still clear `foldMode`.
- ANCHOR: `packages/forth-core/programming/manage.c:723-733` — `_insertInProgram` rebases `currentStep`/`firstDisplayedStep`/`beginOfCurrentProgram`/`endOfCurrentProgram` whenever `freeProgramBytes < size`; `:767-772` — it ends on `goToGlobalStep` computed from the post-increment `currentLocalStepNumber`, i.e. parked on the step *after* the insert; `:1210-1219` — resume's canary path abandons **without** restoring `currentStep`. The landed pattern is offset + canary: `packages/forth-core/forth_capture.h:44-48` (`savedStepOffset` — "program memory may relocate").

### VERDICT — implementable after edits. B3 crosses into F2 (see CROSS-PACKET); fix both packets in the same pass.

---

## PACKET_L1_F2_fold_seams.md

### BLOCKERS

**B1 — C1 (packet line 39): `calcMode != CM_PEM` discriminates on a value C3's own bracket forges.**
- NOW: `if(calcMode != CM_PEM) { forthFoldEnter(func, tam.mode); }`
- MUST SAY: `if(forthCapIsInteractive()) { forthFoldEnter(func, tam.mode); }`. Add: `forthCapIsInteractive()` must stay true across a suspension (state it as a contract on L1-1's `origin` bit), and add a C5 subcase driving the re-entry chain that asserts the capture is OPEN, not SUSPENDED, after the bracket drops.
- ANCHOR (reachable re-entry, all inside `tamProcessInput` with the bracket on): `packages/forth-core/ui/tam.c:980 leaveTamModeIfEnabled();` (C2 fires: `calcMode == CM_PEM` → resume + `forthFoldLeave`) then `:987 runFunction(i);` → `packages/forth-core/items.c:736` (third disjunct `calcMode != CM_PEM` is true after the bracket… and `tam.mode` is 0) → `items.c:744 tamEnterMode(func);` → back at `ui/tam.c:1180` where `forthCapIsOpen()` is true and `calcMode == CM_PEM` is still true, so the arm suspends with **no** fold armed. `forthFoldPending()` is then false and C3's epilogue restores `CM_AIM`, so the C2 resume block at `ui/tam.c:1406-1409` is unreachable: the capture is stuck `FCAP_SUSPENDED`. Second instance of the same shape: `ui/tam.c:1130` and `:1139-1143`.

**B2 — C3 comment block (packet lines 100-103): the blanket claim is false, and the packet forbids editing commit sites on the strength of it.**
- NOW: "every TAM commit site in this file already has a `calcMode == CM_PEM` arm that RECORDS a step instead of dispatching".
- MUST SAY: replace with the enumerated route list and a per-site disposition (in F1's `_forthFoldAdmits` admit set, or excluded). For the leave-then-dispatch class, say plainly that `leaveTamModeIfEnabled` cannot be the choke — either move `forthFoldLeave()` into the C3 epilogue (after `_tamProcessInput` returns), or require F1's admit set to exclude `ITM_GTO`/`ITM_XEQ`/`ITM_DELP`/`TM_NEWMENU` and every `TM_STORCL` softkey that reaches `ui/tam.c:566`.
- ANCHOR — sites that record: `ui/tam.c:217`, `:552`, `:587`, `:605`, `:618`, `:907`, `:929`, `:1102`. Sites that dispatch/navigate with no CM_PEM arm: `ui/tam.c:566-573` (`tam.currentOperation = item; … leaveTamModeIfEnabled(); runFunction(tamOperation());` — reachable from the packet's own headline gesture: `menu_TamSto` carries `ITM_dddVEL`/`ITM_dddIX`/`ITM_dddVEL1..3`); `:303-304`; `:888-899` (`ITM_GTOP` → `fnGoto` / `goToPgmStep`, ahead of the `else if(run)` switch); `:771-796`; `:975-999` (`:980` leave then `:987 runFunction(i)`, `:996` leave then `:997 forthDispatchColon`); `:1119-1124` (`ITM_GTOP` → `goToGlobalStep`, `ITM_DELP` → `reallyRunFunction`, both tested **before** the `else if(calcMode == CM_PEM) { /* already done */ }` at `:1125`, and DELP is excluded from the record at `:1102`).
- Consequence to state for the GTOP navigators: `forthCaptureResume` restores `currentStep = p` (`manage.c:1237`) and computes `uint16_t n = getNumberOfSteps() - forthCapSavedStepCount();` (`manage.c:1240`) **without** re-deriving `currentProgramNumber`, which `goToGlobalStep` changed via `defineCurrentProgramFromGlobalStepNumber` (`lblGtoXeq.c:120`) — `n` underflows and the loop at `manage.c:1241-1254` eats real steps.
- `leaveTamModeIfEnabled` route list to paste into C2 (all `packages/forth-core/ui/tam.c`): commit-then-leave 223, 560, 595, 614, 626, 794, 920, 924, 931, 1041, 1139, 1143; leave-then-dispatch 303, 494, 508, 520, 536, 572, 913, 934, 980, 996, 1130; cancel/EXIT 238, 317, 321, 431; from outside `tamProcessInput` `keyboard.c:3785` and `ui/tam.c:1168`.

### MAJORS

None beyond B1/B2 — but the C4 "Safety is provable from the write-set" paragraph (packet lines 150-155) repeats L1-3 B2's fallacy verbatim and must be replaced with a reachability statement over the newly-opened paths (see CROSS-PACKET).

### VERDICT — needs re-spec of C3's comment block and C1's guard. The mechanism survives; the site inventory does not.

---

## PACKET_L1_F3_fold_parity.md

### BLOCKERS

**B1 — C1 table row 9 (packet line 47): not drivable, and the anchor points at the wrong arm.**
- NOW: "`TM_VALUE` > 250 | an item whose commit takes the `CNST_BEYOND_250` encoding (manage.c:2185-2188)"
- MUST SAY: delete row 9 (or replace it with a reachable `TM_VALUE` row) and record `CNST_BEYOND_250` as unreachable in this tree, so the implementer does not hunt a fold bug when the row will not drive. Correct the anchor to `manage.c:2174-2177`.
- ANCHOR: `packages/forth-core/programming/manage.c:2174` is the gate `if(tam.mode == TM_VALUE && ((indexOfItems[func].status & PTP_STATUS) == PTP_NUMBER_8_16) && tam.value > 250)`; the emit is `:2175-2177`; `:2182-2185` is the `SYSTEM_FLAG_NUMBER` arm and `:2187` heads `TM_MENU`. `PTP_NUMBER_8_16` occurs exactly once per tree (`grep -c` = 1 in both `src/c47/items.c` and `packages/forth-core/items.c`): `packages/forth-core/items.c:2063`, item 207 `ITM_CNST`, `tamMinMax = (0 << TAM_MAX_BITS) | (NOUC-1)` with `NOUC 84` (`src/c47/defines.h:1179`) — max 83, and `ui/tam.c:744` clamps the accumulator to `tam.max`.

**B2 — C3 (packet lines 96-99): encoding the traces verdicts verbatim produces two assertions that are RED by construction.**
- NOW: "For each row marked **KEEP PEM-only**, drive the interactive equivalent and assert the PEM-only behaviour did **not** happen."
- MUST SAY: restate rows 11 and 12 in this packet as post-ruling **WIDEN-by-bracket**, and say explicitly that the class test asserts the fold's behaviour, not the stale KEEP.
- ANCHOR: `design-docs/forth-core/STAGE_L_TRACES.md:617` (row 11, `ui/tam.c:1102`, "KEEP PEM-only — this IS the L-R4 asymmetry") and `:618` (row 12, `ui/tam.c:1180`, "KEEP PEM-only under (a); new CM_AIM arm under (c)") — both predate the L-R4 (b) ruling, as their own "under (a)/(c)" wording shows. F2 C3 forges `calcMode = CM_PEM` precisely so `ui/tam.c:1102` records, and F2 C1 rewrites `ui/tam.c:1180` to fire interactively.

### MAJORS
None.

### VERDICT — implementable after edits (tests-only packet; both defects are table/text corrections).

---

## PACKET_L1_5_acceptance.md

### BLOCKERS

**B1 — C2.1 (packet lines 42-46): the (open × close) cross product is not well-defined.**
- NOW: "add the interactive open as a second open-path axis and assert the full close tuple (`state`, `keysMode`, `origin`, `foldMode`) after every (open × close) pair. Report the pair count."
- MUST SAY: drop the cross product. Enumerate the interactive close paths on their own axis — L1-2 C2 rung 3, `forthCapPowerReset`, and the restore sanitizer — and report that count.
- ANCHOR: `packages/forth-core/test_capture.part.h:6995-7012` — the landed switch is four hardwired **PEM** close paths (`fnKeyBackspace` on empty, `runFunction(ITM_ENTER)` on empty, `fnKeyUp` with text after `showSoftmenu(-MNU_FORTH)`, `runFunction(ITM_FORTH)` with text). Three do not close an interactive capture: `PACKET_L1_2` line 57 makes empty ENTER an explicit no-op; `PACKET_L1_H` C4 diverts only the f-shifted arrow ids, leaving unshifted Up as the case-change/scroll/`closeAim(); fnBst();` gesture (`packages/forth-core/keyboard.c:4636-4655`); and L-R2 makes FORTH always open, never toggle-close.

### MAJORS
None.

### VERDICT — implementable after edits.

---

## CROSS-PACKET

1. **`keyboard.c:1686` is edited by two packets and must stay one predicate.** L1-3 C2 and F2 C4 both rewrite the same `CM_AIM` disjunct. F2's edit inherits L1-3's missing companion at `keyboard.c:1726` (L1-3 B2); if L1-3 ships without it, F2's fold state lands in the bug-screen `else` at `keyboard.c:1737` too. State in F2 that its clause is an extension of L1-3's, and that both halves (escape at 1686, entry at 1726) must be present.

2. **The same fallacious safety argument appears twice** — L1-3 C2 lines 95-97 and F2 C4 lines 150-155: "the predicate's value is identical to today's for every execution that exists today." True in both, irrelevant in both: the defect class is in the paths the change newly opens. Replace both paragraphs with an explicit reachability statement over the new state, and add it to the packet-authoring checklist.

3. **`foldMode` lifetime is specified in F1 and consumed in F2, and they contradict.** F1 C1 line 51 / C6.7 clear it at the capture-object reset sites; F2 C2 calls `forthCaptureResume()` — which calls `forthCapOpen()` (`manage.c:1224`) and, on canary failure, `forthCapAbandonSuspended()` (`manage.c:1214`) — immediately before `forthFoldLeave()`, whose first line early-returns on `foldMode == 0`. Fix once, in F1 (see F1 B3), and cite it from F2 C2.

4. **The cursor-restore key is wrong the same way in two packets.** L1-H C2 (`savedGlobalStep`) and F1 C1 (`savedGlobalStep`) share the identical instability (`lblGtoXeq.c:120-133`, `src/c47/programming/manage.c:392`). Apply the same `(program, localStep)` fix to both, or both will drift.

5. **Ordering gap: F1 C3 line 113 calls "L1-H's helper" that L1-H never exports.** L1-H C1 (packet lines 45-59) exports only `forthHistoryProgram()` and `forthHistoryEnsure()`; "position on FHIST's last step (before its END)" appears only as inline prose in L1-H C3 line 106. Name it in L1-H (e.g. `bool_t forthHistoryGotoLastStep(void)`), declare it in `forth_capture.h`, and have F1 C3 call it by that name.

6. **Ordering gap: L1-2 C2 rung 3 depends on a decision L1-3 makes.** Rung 2's predicate (`isAlphaSubmenu(0)`, `packages/forth-core/softmenus.c:3880-3891`) already returns true for `-MNU_FORTH`, and L1-3 C4's toggle changes what sits on the stack. Whatever L1-2 M2 settles for rung 2 must be re-checked against L1-3 M3's drain; say so in L1-3's out-of-scope section.

7. **`item > 0` gating is missing in every new `indexOfItems[item]` read.** L1-2 C4 (blocking, verified reachable) and, for the same reason, the L1-3 C1 divert arm which reads `indexOfItems[func].status`. Add the conjunct in both and note the landed wart at `keyboard.c:2794` so it is not copied.

## OWNER QUESTIONS

1. **Interactive keys-mode toggle with FWRD (or any alpha submenu) on top — what should the softmenu stack look like?** PEM's `_closeAlphaMenus` (`packages/forth-core/programming/manage.c:776-800`) has no `MNU_FORTH` case and returns without popping, so "match PEM" has no answer here. L1-3 C4's K-R3 rationale ("the underlying row IS the indicator") assumes a single alpha level. Ruling needed: drain everything alpha including FWRD, or mirror PEM and leave FWRD standing.

2. **EXIT rung 3 and undo.** B1's fix restores the pre-FORTH X by calling `undo()` on the snapshot `reallyRunFunction` took at the FORTH press (`items.c:304`). That makes EXIT-from-capture consume the user's undo slot. Acceptable, or should the placeholder be dropped without touching undo (and `saveForUndo()` at `keyboard.c:3879` deliberately skipped)?