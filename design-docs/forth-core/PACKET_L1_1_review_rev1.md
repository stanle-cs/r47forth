# REVISION LIST — PACKET_L1_1_origin_seams.md

All packet line numbers below are from the file as it stands on disk (400 lines, untracked).

---

## 1. BLOCKERS

### B1 — Scope sentence is false: no seam closes an interactive capture (packet 12-14)

**Now:** "pressing FORTH outside PEM opens a capture line you can type into and that closes cleanly by every existing seam — and nothing else."

**Must say:** L1-1 leaves an interactive capture with **no close path**. Either (a) fold in the minimum close — `forthCapClose()` guarded on `forthCapIsInteractive()` at the CM_AIM teardown seams — or (b) state in Scope that the machine is left with a leaked `FCAP_OPEN` until L1-2 and that L1-1 must not ship to a branch alone. Option (a) is not one seam: it must cover `fnKeyExit` (`packages/forth-core/keyboard.c:3868`), `fnKeyEnter` (`keyboard.c:3513`), `fnKeyUp` (`keyboard.c:4654`), `fnKeyDown` (`keyboard.c:4872`) — or the shared choke point `closeAim()`.

**Anchors:**
- `packages/forth-core/keyboard.c:3868-3888` — `case CM_AIM:` in `fnKeyExit` calls `closeAim()` / `popSoftmenu()+stayInAIM()`; no `forthCap` contact.
- `src/c47/bufferize.c:2693-2712` — `closeAim()` never mentions `forthCap` (no package override; `packages/forth-core/` has no `bufferize.c`).
- Only production `forthCapClose()` sites: `packages/forth-core/programming/manage.c:1005`, `:1138`, `:1143` — all inside `pemAlpha`/`pemCloseAlphaInput`, every caller `CM_PEM`-gated; plus `forth_dict.c:57`,`:71` (power reset).
- Leak is not inert: `packages/forth-core/programming/manage.c:1720-1734` — `if(func == ITM_AIM && forthCapIsOpen())` has no mode or origin discriminator, so the next PEM ALPHA press ping-pongs keys mode instead of opening literal input.

---

### B2 — T2's interactive rows cannot go green (packet 344-351)

**Now:** "add the interactive open as a new **row** in whatever table/loop it drives, so every existing close path is exercised against an interactive capture too. Report the number of (open-path × close-path) pairs before and after."

**Must say:** delete the interactive rows and the pairs language. Keep only the tuple extension to `origin` for the existing four PEM rows, and state that an interactive capture has no landed close path to sweep — that sweep arrives with L1-2's EXIT. Honest count is 4 before and 4 after.

**Anchors:**
- `packages/forth-core/test_capture.part.h:6951` — `for (int sc = 1; sc <= 4 && !fail; sc++)`; there is no table and no open-path axis.
- `test_capture.part.h:6964`, `:6982-6983` — the open is hardwired once above the switch: `calcMode = CM_PEM; … fnGotoDot(2); runFunction(ITM_AIM);` with a hard FIXTURE FAIL if `!forthCapIsOpen()`.
- All four close drives require CM_PEM: `fnKeyBackspace` reaches its capture arm only at `keyboard.c:4456 case CM_PEM:` (CM_AIM arm at `:4361` never touches `forthCap`); `ITM_ENTER`/`ITM_FORTH` reach `pemCloseAlphaInput` only through `packages/forth-core/items.c:766 if(calcMode == CM_PEM)`; `fnKeyUp`'s capture arm is at `keyboard.c:4722 case CM_PEM:`.
- Worse for case 4: `runFunction(ITM_FORTH)` outside PEM falls to `items.c:813` → `fnForthOuter`, i.e. C2's new body, which **re-opens** the capture — the row would assert `FCAP_CLOSED` against a freshly reopened capture.

---

### B3 — `manage.c:1224` is not a PEM open; resume clobbers `origin` (packet 113-115, 120, 122-125)

**Now:** "every landed call site (manage.c:897, :918, :1224) is a PEM open and must not be touched"; "Do NOT add a fourth site"; "**Suspend does NOT touch `origin`** — K3's precedent for `keysMode` is that the bit rides the suspension (forth_capture.c:26-31 comment)".

**Must say:** `:897` and `:918` are PEM opens (both in `pemAlpha`). `:1224` is the SUSPENDED→OPEN **re-open** inside `forthCaptureResume()`. `keysMode` does not ride the suspension by being untouched — `forthCapOpen()` zeroes it and resume rescues it with an explicit save/restore bracket. Under C1, `origin` inherits the identical clobber with no bracket. Fix one of:
- (a) add the mirror bracket at `manage.c:1220-1226`: `uint8_t originWas = forthCapOriginRaw(); forthCapOpen(); forthCapSetOrigin(originWas);` — and **declare both accessors** in C1 (packet 88-97 currently declares only `forthCapIsInteractive`); or
- (b) split out `forthCapReopenPreserving(void)` used only at `:1224`.

Either way: `:1224` must stop being called a PEM open, and "Do NOT add a fourth site" must be narrowed to *reset* sites (the three-site claim is correct for **clearing** origin) — the resume re-open is a fourth **write** site the E14 sweep does not cover. Also drop the `forth_capture.c:26-31` citation: that comment says the *line* is not carried across the suspension; it says nothing about `keysMode`.

**Anchors:**
- `packages/forth-core/programming/manage.c:1208-1209` — `void forthCaptureResume(void) { if (!forthCapIsSuspended()) { return; }` ; `:1224` `forthCapOpen();` with its own comment "SUSPENDED → OPEN".
- `manage.c:1220-1226` — the landed bracket `{ bool_t keysWas = forthCapKeysMode(); … forthCapOpen(); forthCapSetKeysMode(keysWas); }`.
- `packages/forth-core/forth_capture.c:5-12` — `forthCapOpen()` unconditionally sets `forthCap.keysMode = 0`.
- `forth_capture.c:26-31` — the comment C1 cites, about the line, not the bit.

---

### B4 — T1 subcase 5 is unimplementable and destroys program memory (packet 338-340)

**Now:** "Open interactive, drive `forthCaptureSuspend`/`forthCaptureResume` directly, assert `forthCapIsInteractive()` is still true after resume."

**Must say:** delete it. Replace with a non-destructive state-level subcase: open interactive → `forthCapSuspendState(0,0,0,0)` → assert `forthCapIsInteractive()` still true in `FCAP_SUSPENDED` → `forthCapClose()`. Add to C1 the plain statement that suspend/resume is PEM-only today (`ui/tam.c:1181`, `:1408`), so origin's suspension behaviour is inert by construction. If the architect instead keeps a real round-trip assertion, B3(a) is a hard prerequisite.

**Anchors:**
- `packages/forth-core/programming/manage.c:1181` — the whole guard is `if (!forthCapIsOpen()) { return; }`; an interactive capture is `FCAP_OPEN`, so it does **not** no-op.
- `manage.c:1188` → `manage.c:1173-1174` — `forthCapRecommitStep()` does `deleteStepsFromTo(currentStep, findNextStep(currentStep))` then `_insertInProgram`, with only a comment as contract ("callers guarantee currentStep is ON the capture step"). An interactive capture owns no step.
- `packages/forth-core/test_dict_reloc.c:548` — after `restoreTestProgram()`, `currentStep = beginOfProgramMemory`, i.e. the 4-byte `.END.` the subcase would delete.
- Reachability: `packages/forth-core/ui/tam.c:1180` `else if(calcMode == CM_PEM && forthCapIsOpen())` and `ui/tam.c:1406` `if(calcMode == CM_PEM)` are the only production callers of suspend/resume; an interactive capture is `CM_AIM` (packet 243).
- Landed correct idiom for direct driving: `packages/forth-core/test_engine.part.h:5996`, `:6034`, `:6049` — program written, `currentStep` provably on the placeholder.

---

### B5 — Mutations 1 and 2 are unkillable; no raw `origin` reader exists (packet 362-365, 88-97)

**Now:** M1 "Drop the conjunction … Expect RED at T1 subcase 6"; M2 "Remove `origin = FCAP_ORIGIN_PEM` from `forthCapClose()`. Expect RED at T2." Accessors block declares only `forthCapIsInteractive()`.

**Must say:** the conjunction (`state != FCAP_CLOSED`) and the E14 reset mask each other — `(CLOSED, INTERACTIVE)` is unreachable, so M1 is extensionally identical to the spec'd version on every reachable state, and M2's leak is unobservable through the only declared reader. Fix:
- Add to C1 a selftest-gated raw reader beside the landed pair: `#if defined(FORTH_DEBUG_SELFTEST) uint8_t forthTestCapOrigin(void); #endif` in `forth_capture.h` and its body in `forth_capture.c`.
- State that T2 asserts `forthTestCapOrigin() == FCAP_ORIGIN_PEM` after each close (kills M2), and that T1.6 does the same after an interactive close.
- M1 still cannot be killed by a getter alone (you cannot manufacture `(CLOSED, INTERACTIVE)`). Rule one of: drop the conjunction and let the E14 reset be the single defense — matching the landed `forthCapKeysMode()` idiom exactly — or keep it and replace M1 with an explicit "unfalsifiable by construction" note.
- Also name T1.6's close drive: after L1-1 the only one available is a direct `forthCapClose()` call.

**Anchors:**
- `packages/forth-core/forth_capture.c:3` — `static forthCap_t forthCap;` file-static; `test_capture.part.h` is `#include`d into a different TU (`test_dict_reloc.c:2452`).
- `forth_capture.c:71` — `bool_t forthCapKeysMode(void) { return forthCap.keysMode != 0; }` — a **bare** field read; that is why the landed poison works.
- `test_capture.part.h:6993` — `forthCapSetKeysMode(true);` (poison); `test_capture.part.h:7030` — the bare-read assertion that catches a missed clear. There is no origin equivalent.
- `forth_capture.c:74-79` / `forth_capture.h:87-90` — the landed selftest export block, the idiom to copy.
- Every writer of `forthCap.state` is in `forth_capture.c` (`:9`, `:15`, `:30`, `:37`); with C1's three reset sites, `(CLOSED, INTERACTIVE)` has no producer.

---

### B6 — T3 asserts `CM_NORMAL`; the arm never assigns `calcMode` (packet 356, 300)

**Now:** T3 "assert it clears to `CM_NORMAL` with `FLAG_ALPHA` clear"; the arm's only mode-adjacent statement is `calcModeNormalGui();`.

**Must say:** pick one and write it out in full. Either
- (i) restate T3 to what the code guarantees — `aimBuffer[0] == 0`, `T_cursorPos == 0`, `displayAIMbufferoffset == 0`, `FLAG_ALPHA` clear, `tam.function` unchanged, **`calcMode` still `CM_AIM`** — citing `test_persist.part.h:530` as the landed precedent that the sanitizer leaves `calcMode` untouched; or
- (ii) add `calcMode = CM_NORMAL;` to the new arm explicitly (do **not** substitute `calcModeNormal()` as a one-word swap: it pops only one `-MNU_ALPHA` frame and duplicates the arm's own `clearSystemFlag(FLAG_ALPHA)`, silently weakening the teardown relative to `_closeAlphaMenus()`).

If (i) is chosen, the packet must also state that the arm deliberately leaves `CM_AIM` with `FLAG_ALPHA` cleared, and that `saveRestoreBackup.c:1545-1548` will then re-run `calcModeAimGui(); cursorEnabled = true;` on the torn-down state.

**Anchors:**
- `src/c47/hal/gui.h:11` — `#define calcModeNormalGui()` (empty) under `DMCP_BUILD || SIMULATOR_ON_SCREEN_KEYBOARD == 0`; `src/testSuite/hal/gui.c:6` — `void calcModeNormalGui (void) {}` (the HAL the gate links); `src/c47-gtk/gtkGui.c:3689-3692` — early return on `headlessMode`, widgets only.
- `src/c47/calcMode.c:44` — `calcMode = CM_NORMAL;` lives in `calcModeNormal()`, with `calcModeNormalGui()` only as its tail at `:57`.
- `packages/forth-core/programming/manage.c:836` — the landed PEM arm calls `calcModeNormalGui()` and correctly leaves `CM_PEM`; the shape cannot be copied blind.
- `packages/forth-core/saveRestoreBackup.c:1496` → `:1545-1548` — straight-line fall-through inside `restoreCalc`, no return or branch between.

---

### B7 — T1 subcase 3's stack assertion is false (packet 329-333)

**Now:** "note `getStackTop`-equivalent depth, call `fnForthOuter`; assert … X now holds what was in Y before (the drop happened)."

**Must say:** `calcModeAim()` calls `liftStack()`, which both lifts **and replaces X with a freshly allocated `dtReal34`** on both branches. After the interactive open X is a fresh real34 in every case; where the old Y lands depends on `FLAG_ASLIFT`, which the fixture never pins. Rewrite the clause as: set `FLAG_ASLIFT` explicitly in the fixture, assert `getRegisterDataType(REGISTER_X) == dtReal34`, and pin the drop by asserting the pre-call Y value is now in Y. Delete "note `getStackTop`-equivalent depth" — `getStackTop()` is a stack-*size* macro, not an occupancy count, and cannot witness a drop. Add the destructive half of the fact to the C2 order note at packet 240-242, with the reason ("the interactive open deliberately discards X — L-R2 consequence, not a leak"), so the next reader does not file it as a bug.

Note the asymmetry that makes this look plausible: T1.4 (packet 334-337) **is** correct — the oversize path returns before `fnAim`, so no `liftStack` runs and "X still holds the string" holds.

**Anchors:**
- `src/c47/stack.c:35-36` — `setRegisterDataPointer(REGISTER_X, allocC47Blocks(REAL34_SIZE_IN_BLOCKS)); setRegisterDataType(REGISTER_X, dtReal34, amNone);` runs on **both** branches of the `FLAG_ASLIFT` split at `:22`/`:32`.
- Caller chain traced: packet:243 `fnAim(NOPARAM)` → `src/c47/bufferize.c:18` `calcModeAim(NOPARAM);` (unconditional) → `src/c47/calcMode.c:74` `if(!tam.mode && calcMode != CM_ASSIGN && calcMode != CM_PEM && calcMode != CM_ASN_BROWSER)` — true from CM_NORMAL — → `calcMode.c:76` `liftStack();`. No package override of `stack.c`/`calcMode.c`/`bufferize.c`.
- `src/c47/defines.h:2284` — `#define getStackTop() (getSystemFlag(FLAG_SSIZE8) ? REGISTER_D : REGISTER_T)`.
- `FLAG_ASLIFT` is live state, not zero: `packages/forth-core/forth_inner.c:156` (and `:249`, `:261`, `:571`) sets it during interpretation.

---

## 2. MAJORS

### M1 — C3's rationale sentence is false about the tree (packet 288-294, 309-315)

**Now:** "the discriminator is the restored UI state itself, exactly as for the PEM arm above" + a STOP asking the implementer to go check whether native AIM collides.

**Must say:** the tree answer is already known: **it collides.** `calcMode` and `FLAG_ALPHA` both round-trip verbatim, so a plain native alpha session restores as exactly `CM_AIM + FLAG_ALPHA + !forthCapIsOpen()` and the proposed arm tears it down, discarding the user's alpha line. The analogy is wrong: the PEM arm has *two* discriminators (`calcMode == CM_PEM` **and** `tam.function == ITM_FORTH`, a Forth-only persisted marker); the interactive arm has none, and C2 forbids setting `tam.function` while C1 makes `origin` never-persisted. Delete the rationale sentence and the "go check" STOP; state the collision. See §3 for the ruling this forces.

**Anchors:**
- `packages/forth-core/saveRestoreBackup.c:335` / `:1036` — `calcMode` saved and restored verbatim; `:392-393` / `:1099-1101` — `systemFlags0/1` likewise; `packages/forth-core/defines.h:920` `#define FLAG_ALPHA 0x800e` → bit 14 of `systemFlags0` per `src/c47/flags.c:286-287`.
- `packages/forth-core/saveRestoreBackup.c:1496` — sanitizer runs *after* both restores; `:1545-1549` — upstream deliberately restores native alpha (`else if(calcMode == CM_AIM) { … calcModeAimGui(); cursorEnabled = true; }`).
- `packages/forth-core/programming/manage.c:826-828` — the PEM arm's guard, showing the `tam.function != ITM_FORTH` discriminator the new arm lacks.
- Scope note for the packet: all of `saveCalc`/`restoreCalc` is inside `#if defined(PC_BUILD)` (`saveRestoreBackup.c:31` … `:1572`), so this is simulator-and-gate only, not DMCP.

### M2 — `forth_compile.c` cannot see `forth_capture.h` (packet 88, 110, 269-273)

**Now:** the new symbols (`forthCapOpenInteractive`, `forthCatalogMenuOnTop`, `forthCatalogBuriedOnStack`) are placed in `forth_capture.h` and called from `fnForthOuter`; no include change is stated anywhere in the packet, and `forthCapOpenInteractive`'s declaration site is never named at all.

**Must say:** add `#include "forth_capture.h"` to `forth_compile.c`'s include block after `forth_prims.h` (order-independent — `forth_capture.h:1-4` has its own guard and includes `c47.h`), and declare `forthCapOpenInteractive(void)` in `forth_capture.h` beside `forthCapOpen` at `:52`.

**Anchors:** `packages/forth-core/forth_compile.c:7-12` is the complete include list (`<string.h>`, `c47.h`, `forth_dict.h`, `forth_prims.h`, `programming/param_core.h`); `grep -in forth src/c47/c47.h` is empty and `forth_dict.h`/`forth_prims.h`/`param_core.h` include only `<stdbool.h>`/`<stdint.h>`, so no transitive route exists. This does **not** stop the build — `meson.build:4` is `warning_level=2` with no `werror`, so the two `bool_t` wrappers get implicitly declared as `int` and the drain loop breaks on an ABI-unspecified return, behind a green gate.

### M3 — `T_cursorPos = stringLastGlyph(aimBuffer) + 1` drops the landed empty-line guard (packet 250, 331-332)

**Now:** C2 sets `T_cursorPos = stringLastGlyph(aimBuffer) + 1`; T1.3 asserts the same expression back.

**Must say:**
- An empty string in X is a valid `dtString` (`len+1 == 1` passes the size check), so `seeded == true` and `stringLastGlyph("") + 1 == 1` puts the cursor one past the NUL. The landed PEM code carries `T_cursorPos = (ll == 0) ? 0 : stringLastGlyph(aimBuffer) + 1;` for exactly this, with a comment naming the consequence. **Restore that guard in C2 and say so.**
- T1.3's cursor assertion is self-confirming (it asserts the implementation's own expression). Give it an independent oracle and add an empty-string seed subcase.
- The multi-byte-glyph case (`+1` lands one byte *inside* a trailing 2-byte glyph) is inherited from the landed PEM idiom, not introduced here — flag it as a named bug class per the standing class-test rule rather than silently diverging one call site.

**Anchors:** `src/c47/charString.c:453-479` — `stringLastGlyph` returns the byte offset of the **start** of the last glyph; `packages/forth-core/programming/manage.c:900-904` — the landed `(ll == 0) ? 0 : …` guard and its comment ("would insert every glyph behind the terminating NUL and silently eat the keystrokes"); `manage.c:878`, `:886` — the unguarded siblings that share the class.

### M4 — T1 and T3 have no runner registration (packet 319, 353)

**Now:** C4 names the file for T1 and nothing at all for T3; the packet never mentions `test_dict_reloc.c` or `forthDictSelfTest`.

**Must say:** (1) forward-declare `test_capture_origin_lifecycle` in `test_dict_reloc.c` beside the FIX-8 declaration and invoke it in `forthDictSelfTest` beside the FIX-8 invocation, **outside** the `if(fail)` verdict branch; (2) give T3 a function name and file and do the same; (3) require the report to quote the `[DEBUG] running …` line and its PASS lines verbatim from the green log, not just the `ALL PASSED` banner.

**Anchors:** `packages/forth-core/test_dict_reloc.c:1125` (forward-decl block) and `:2085-2086` (`printf("  [DEBUG] running test_capture_close_paths_reset_tuple...\n"); fail |= test_capture_close_paths_reset_tuple();`). There is no self-registration mechanism — `packages/forth-core/test_capture.part.h` holds bodies only, `#include`d at `test_dict_reloc.c:2452`, i.e. *after* the runner ends. An uncalled static test yields a warning and a green gate. Precedent, same stage: commit `14fecc428` — "the K4 registrations had been planted inside the suite's `if(fail)` verdict branch — its landing gate never ran them."

### M5 — Mutation count is wrong (packet 213, 395)

**Now:** Acceptance says "All **six** mutations shown RED and reverted", but Mutation 7 (C2a guard deletion) is specified at packet 213.

**Must say:** seven. Either renumber M7 into the §Mutations list or fix the acceptance count.

---

## 3. THE ONE QUESTION THE PACKET CANNOT ANSWER ITSELF

**Does C3 ship at all?** The restore sanitizer's new arm has no discriminator that distinguishes an interactive Forth capture from a plain native alpha session — the only candidate markers are `tam.function` (which C2 forbids setting) and `forthCap.origin` (which C1 declares never-persisted). The packet's own STOP names this and then says the fix "is a persisted discriminator, which is a design change this packet may not make." So the architect must rule: **drop C3 from L1-1** (an interactive capture is already reset to a harmless closed/PEM state by the restore, since `origin` is process-local), or **authorize the persisted discriminator** as a design change and re-spec C3 around it. This is not delegable to the implementer — ship/no-ship of a whole code block is a decision, and the packet currently asks the implementer to discover a fact the review has already established.

*(Secondary, decidable by the same owner: B5's M1 — keep the `state != FCAP_CLOSED` conjunction as unfalsifiable defense-in-depth, or drop it for symmetry with the landed bare-read `forthCapKeysMode()`.)*

---

## 4. VERDICT

**Needs re-spec, not just edits.** B1, B2, B4 and B6 each remove or rewrite a deliverable the packet currently orders, and B3/B5 add production and test API the packet does not declare. Sections C1 (close/reset + accessors), C4/T1.3, C4/T1.5, C4/T2, C4/T3 and the Mutations list must all be rewritten, and C3's existence is blocked on the §3 ruling. C2 itself survives — its X-read factoring, the C2a running-program guard, the catalog drain, and the `tam.function` prohibition are sound as written; the defects in that section are the missing include (M2), the missing empty-line cursor guard (M3), and the incomplete `liftStack` note (B7).