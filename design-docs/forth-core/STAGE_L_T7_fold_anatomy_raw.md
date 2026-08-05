# FOLD BUILD SHAPE — Stage L interactive fold

## 1. VERDICT

**Buildable. Nothing is blocked.** But not in the shape the L-R4 background assumed.

The background (`design-docs/forth-core/STAGE_L_INTERACTIVE.md:234-260`) frames the fold as three independent constructions: a non-executing TAM variant, a new suspend store, and a synthesis path. **All three collapse into one decision** once you accept the following:

> The interactive fold does not *emulate* PEM's fold. It **runs** PEM's fold, on a real program step, with `calcMode` temporarily equal to `CM_PEM` for the duration of `_tamProcessInput` only.

Consequences, each of which removes a whole piece of previously-planned work:

- **(a) non-executing TAM: zero new gates.** Every commit site in `tam.c` already has a `calcMode == CM_PEM` arm that records a step instead of dispatching. Making that predicate true is the entire mechanism. This matters because *every* enumeration of those sites produced by the trace round was wrong: 6-vs-8 (`packages/forth-core/ui/tam.c:606`, `:619` missing), 20-vs-39, 3-vs-4 cancel sites (`packages/forth-core/ui/tam.c:238-239` missing). An enumeration-based gate is empirically not achievable here.
- **(b) suspend store: zero new bytes of storage.** If a real `ITM_FORTH` capture step exists in program memory, `forthCaptureSuspend`/`forthCaptureResume` (`packages/forth-core/programming/manage.c:1180-1290`) are reused **verbatim, unmodified**. The store is the step, exactly as in PEM.
- **(c) text synthesis: nothing to extract, nothing to refactor.** The step is a genuine in-program step, so `decodeOneStep` is handed exactly what it is handed in PEM. Every decoder hazard the refuters found — the `firstFreeProgramByte` name clamp (`packages/forth-core/programming/decode.c:129-133`), `findKey2ndParam`/`programBytesAvailable` (`src/c47/programming/nextStep.c:17`), the `forthMarkerTurnsOn` owning-program walk (`packages/forth-core/programming/decode.c:837`) — is a hazard **only** for a scratch buffer outside program memory. All of them evaporate.

The encoder-extraction path (`insertStepInProgram`, `packages/forth-core/programming/manage.c:1708-2226`) is **rejected outright**, not deferred: `_insertInProgram` is `static` (`packages/forth-core/programming/manage.c:716`) and itself performs opcode substitution (`packages/forth-core/programming/manage.c:743-758`), several switch arms execute side effects instead of emitting bytes (`:2015`, `:2020`, `:2025`, `:2106`), and `tmpString` is an implicit *input* (`:1919-1920` consumed at `:2192`). There is no clean split.

**New RAM cost: +8 bytes BSS.** (Derivation in §2b.)

**The one genuine cost of this shape:** program memory is transiently mutated — one `ITM_FORTH` capture step plus the TAM's own step — for the span of one TAM episode. §4 names every consequence and the sweep that neutralises it.

---

## 2. THE THREE PIECES

### (a) Non-executing TAM — `calcMode` bracket

**Mechanism: bracket `calcMode` to `CM_PEM` across `_tamProcessInput` only.**

Exact site — `packages/forth-core/ui/tam.c:1414-1417`, today:

```c
void tamProcessInput(uint16_t item) {
  _tamProcessInput(item);
  _tamUpdateBuffer();
}
```

becomes:

```c
void tamProcessInput(uint16_t item) {
  const uint8_t savedMode = calcMode;
  const bool_t  brk       = forthFoldArmed();   /* interactive FOLD outstanding */
  if(brk) { calcMode = CM_PEM; }
  _tamProcessInput(item);
  if(brk && calcMode == CM_PEM) { calcMode = savedMode; }
  _tamUpdateBuffer();
}
```

The `calcMode == CM_PEM` re-test on restore is load-bearing: an error raised inside the commit may have changed `calcMode`, and we must not clobber that.

**Why this site is legitimate as a single choke:**
- `_tamProcessInput` is `static` at `packages/forth-core/ui/tam.c:249`; `tamProcessInput` at `:1414` is its only caller (the refuter confirmed this by whole-tree grep and I did not find a second).
- The one *internal* re-entrant call, `packages/forth-core/ui/tam.c:1347` inside `if(forceTamAlpha)`, is **dead code**: `forceTamAlpha` is only ever *cleared* — `packages/forth-core/ui/tam.c:1346`, `packages/forth-core/config.c:1789`, `src/c47/config.c:1778` — and is never assigned `true` anywhere in either tree. Verified this pass by grep over `packages/forth-core/` and `src/c47/`.
- `_tamHandleShuffle` returns early at `packages/forth-core/ui/tam.c:258-260` but is *inside* `_tamProcessInput`, so it is covered by the bracket; its `calcMode == CM_PEM` arm at `:217-218` fires correctly.

**Why the bracket does not leak to the display.** `screen.c` dispatches on `calcMode` at `packages/forth-core/screen.c:6151`, with `case CM_PEM: _refreshPemScreen();` at `:6176-6179` and the TAM overlay branching on `calcMode == CM_PEM` at `:5637-5640`. A whole-episode bracket would therefore paint the program listing under the TAM prompt. The narrow bracket cannot: no refresh runs inside the commit path — `_insertInProgram`'s tail calls `scanLabelsAndPrograms()` and `goToGlobalStep()` (`packages/forth-core/programming/manage.c:770-772`), and `goToGlobalStep` (`packages/forth-core/programming/lblGtoXeq.c:101-140`) contains no refresh call (the refreshes in that file are at `:206`, `:229`, `:755`, `:801`, in other functions). The only `refreshScreen` in `manage.c` reachable near this area is `:506`, inside `fnPem` (`:481`), not the insert path. So the interactive TAM renders exactly like a **native AIM TAM** — no new display code, per L1's expectation.

**Alternatives rejected:**
| Rejected | Why |
|---|---|
| Gate each commit site individually | Five independent tracers each produced an incomplete site list. `packages/forth-core/ui/tam.c:606`/`:619` (IP/FP shortcuts) and `:238-239` (shuffle-backspace) were each missed by the enumeration that claimed to be exhaustive. |
| Whole-episode `calcMode` bracket (tamEnterMode → leaveTamModeIfEnabled) | Paints `_refreshPemScreen` (`packages/forth-core/screen.c:6176`) and the PEM TAM overlay (`:5637`) for the duration. Visible, wrong. |
| Intercept per-keystroke at `tamProcessInput` *entry* and predict the commit | Re-implements TAM completion logic — the exact thing F6-4 was built to avoid. |
| Gate `reallyRunFunction` | Global primitive; used by all of dispatch. |

**Admission rule — FOLD vs PARK.** Evaluated once, in `tamEnterMode`, at the seam in §2b. Some TAM classes are out of v1 scope; they do **not** refuse the key and do **not** lose the line — they take **PARK**: the capture is still materialised and suspended (so the line survives), but the bracket is *not* armed and the TAM executes live. PARK is exactly the owner-raised option (c) behaviour, applied to the minority.

```c
/* manage.c, file-static */
static bool_t _forthFoldAdmits(int16_t func, uint16_t mode) {
  if(func == ITM_GTOP)   { return false; }   /* GTO. navigates the program pointer:
                                                packages/forth-core/ui/tam.c:888-899
                                                calls fnGoto/goToPgmStep with NO
                                                calcMode gate — not an operand */
  if(func == ITM_ASSIGN || func == ITM_USERMODE) { return false; }  /* zeroes aimBuffer
                                                at packages/forth-core/ui/tam.c:1198-1200 */
  if(func == ITM_DELP)   { return false; }   /* already excluded by the PEM commit's own
                                                guard, packages/forth-core/ui/tam.c:1102 */
  switch(mode) {
    case TM_NEWMENU:                          /* sets FLAG_ALPHA + zeroes aimBuffer,
                                                 packages/forth-core/ui/tam.c:1351-1355 */
    case TM_STRING:                           /* same, packages/forth-core/ui/tam.c:1319-1327 */
    case TM_KEY:                              /* half-buffer swap,
                                                 packages/forth-core/ui/tam.c:849-850 */
    case TM_DELITM:
      return false;
    default:
      return true;
  }
  /* `mode` is the value tamEnterMode computed at packages/forth-core/ui/tam.c:1151 */
}
```

Everything else — `TM_VALUE`, `TM_VALUE_NORM`, `TM_REGISTER`, `TM_STORCL`, `TM_VARONLY`, `TM_CMP`, `TM_LABEL`, `TM_LBLONLY`, `TM_SOLVE`, `TM_FLAGR`, `TM_FLAGW`, `TM_M_DIM`, `TM_MENU`, `TM_SHUFFLE` — is IN. That covers STO / RCL / GTO / FIX / SCI / ENG / flag tests, i.e. the whole ruled gesture set.

Note `TM_SHUFFLE` is deliberately **in**: its PEM arm at `packages/forth-core/ui/tam.c:217-218` inserts a step, and the canonical shuffle spelling already exists in the decoder at `packages/forth-core/programming/decode.c:441-447`. Under this shape the low-byte-masking hazard the refuter raised never arises, because we never hand `tam.value` to a renderer — `insertStepInProgram` writes the byte and `decodeOneStep` reads it back.

### (b) Interactive suspend store — the materialised capture step

**Mechanism: no new text store. Materialise a real `ITM_FORTH` capture step and reuse `forthCaptureSuspend`/`forthCaptureResume` unmodified.**

Two new fields land in `forthCap_t` (`packages/forth-core/forth_capture.h:37-50`) **in existing tail/alignment padding**:

```c
typedef struct {
  uint8_t  state;            /* offset 0  — unchanged */
  uint8_t  keysMode;         /* offset 1  — unchanged */
  uint8_t  origin;           /* offset 2  — NEW: 0 = PEM, 1 = INTERACTIVE (owned by L1) */
  uint8_t  foldMode;         /* offset 3  — NEW: 0 = none, 1 = FOLD, 2 = PARK */
  uint16_t savedCursor;      /* offset 4  — unchanged */
  uint16_t savedLocalStep;   /* offset 6  — unchanged */
  uint32_t savedStepOffset;  /* offset 8  — unchanged */
  uint16_t savedStepCount;   /* offset 12 — unchanged */
} forthCap_t;                /* sizeof unchanged: 16 (was 12 payload + 4 padding) */
```

**Byte cost of the struct change: 0.** Field offsets 4/6/8/12 are unchanged, so no existing accessor shifts.

One new file-static context in `packages/forth-core/programming/manage.c`, beside `forthCaptureSuspend`:

```c
static struct {
  uint16_t savedGlobalStep;      /* currentLocalStepNumber + programList[currentProgramNumber-1].step - 1
                                    — the formula _insertInProgram itself uses at manage.c:769 */
  uint16_t savedFirstDisplayed;  /* firstDisplayedLocalStepNumber */
  uint16_t entryStepCount;       /* getNumberOfSteps() BEFORE the capture-step insert */
  uint8_t  savedZerothStep;      /* pemCursorIsZerothStep  (src/c47/c47.h:311, bool_t) */
  uint8_t  pad;
} forthFoldCtx;                  /* 8 bytes exactly */
```

**Total new RAM: +8 bytes BSS.** Report the arena high-water mark unchanged (the fold allocates no arena memory; it may transiently grow *program* memory by up to one block via `resizeProgramMemory` — see §4.7).

New API, all in `manage.c` (they must be there: `_insertInProgram` at `:716` and `_forthCapBuildStep` at `:846` are both `static`):

```c
void   forthFoldEnter(int16_t func, uint16_t mode);  /* materialise + arm */
void   forthFoldLeave(void);                          /* sweep + restore; no-op unless armed */
bool_t forthFoldArmed(void);                          /* foldMode == 1 */
bool_t forthFoldPending(void);                        /* foldMode != 0 */
```

`forthFoldEnter` pseudocode — **exact**, no unstated steps:

```
forthFoldEnter(func, mode):
  if (currentProgramNumber < 1) { goToGlobalStep(1); }          /* guard programList[-1] */
  forthFoldCtx.savedGlobalStep     = currentLocalStepNumber
                                     + programList[currentProgramNumber - 1].step - 1;
  forthFoldCtx.savedFirstDisplayed = firstDisplayedLocalStepNumber;
  forthFoldCtx.savedZerothStep     = pemCursorIsZerothStep;
  forthFoldCtx.entryStepCount      = getNumberOfSteps();

  /* Materialise the capture step, seeded with the LIVE line.  This is
     manage.c:941-952's shape verbatim, with aimBuffer instead of "". */
  _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, aimBuffer));
  --currentLocalStepNumber;
  currentStep = findPreviousStep(currentStep);      /* park ON the capture step */

  forthCap.foldMode = _forthFoldAdmits(func, mode) ? 1 /*FOLD*/ : 2 /*PARK*/;
```

`forthFoldLeave` pseudocode — **exact**:

```
forthFoldLeave():
  if (forthCap.foldMode == 0) { return; }

  /* Debris sweep.  Normally zero iterations: forthCaptureResume already
     deleted the folded step at manage.c:1253.  This covers every break
     path in that loop (manage.c:1246 oversize text, manage.c:1251 no room)
     and the PARK case, where interactively there is nowhere to leave a step. */
  while (getNumberOfSteps() > forthFoldCtx.entryStepCount + 1) {
    deleteStepsFromTo(findNextStep(currentStep), findNextStep(findNextStep(currentStep)));
  }

  /* Drop the capture step itself. */
  deleteStepsFromTo(currentStep, findNextStep(currentStep));

  /* Restore the PEM cursor context wholesale.  goToGlobalStep recomputes
     currentProgramNumber / beginOfCurrentProgram / endOfCurrentProgram /
     currentLocalStepNumber / currentStep consistently
     (packages/forth-core/programming/lblGtoXeq.c:101-140). */
  goToGlobalStep(forthFoldCtx.savedGlobalStep);
  firstDisplayedLocalStepNumber = forthFoldCtx.savedFirstDisplayed;
  defineFirstDisplayedStep();
  pemCursorIsZerothStep = forthFoldCtx.savedZerothStep;

  forthCap.foldMode = 0;
```

`forthFoldLeave` deliberately does **not** touch `calcMode`; the bracket epilogue in `tamProcessInput` owns that, using the local it captured before the call.

The two seams in `tam.c`:

**Seam 1 — `packages/forth-core/ui/tam.c:1180-1182`**, today
`else if(calcMode == CM_PEM && forthCapIsOpen()) { forthCaptureSuspend(); }`
becomes

```c
else if(forthCapIsOpen() && (calcMode == CM_PEM || forthCapIsInteractive())) {
  if(calcMode != CM_PEM) { forthFoldEnter(func, tam.mode); }   /* L: interactive only */
  forthCaptureSuspend();                                       /* F6-2: unchanged */
}
```

Ordering is safe: the arm sits after the `calcMode == CM_NIM` arm at `:1163` and before the two remaining `CM_PEM`-only arms at `:1183` and `:1194`, so a `CM_AIM` capture reaches it and nothing downstream changes. `func` and `tam.mode` are already final at this point — `tam.mode` is assigned at `:1151`, `tam.function` at `:1153`. This is also why the store cannot be derived from `tam.function` (`packages/forth-core/forth_capture.h:29-33`, confirmed).

**Seam 2 — `packages/forth-core/ui/tam.c:1406-1409`**, today
`if(calcMode == CM_PEM) { hourGlassIconEnabled = false; forthCaptureResume(); }`
becomes

```c
if(calcMode == CM_PEM || forthFoldPending()) {
  hourGlassIconEnabled = false;
  forthCaptureResume();      /* no-op unless FCAP_SUSPENDED — unchanged */
  forthFoldLeave();          /* no-op unless armed */
}
```

The `forthFoldPending()` disjunct is required for two reasons: PARK never brackets `calcMode`, and `leaveTamModeIfEnabled` is also reached from outside `tamProcessInput` (EXIT: `packages/forth-core/keyboard.c:3785`), where the bracket is off.

`leaveTamModeIfEnabled` is the right exit choke: it early-returns on `!tam.mode` (`packages/forth-core/ui/tam.c:1364-1366`), and every commit and cancel inside `_tamProcessInput` routes through it (`:924`, `:1143`, `:1130`, `:303`, `:317`, `:321`, `:431`, `:223`, `:238`). The one bypass in the tree is `doFnReset` (`packages/forth-core/config.c:1794`), which is a full reset — see §4.8.

**Alternatives rejected:** an explicit ~258-byte text buffer + cursor. It forks `forthCaptureSuspend`/`forthCaptureResume` into interactive variants (parity risk on the exact code F6-4 exists to keep single-sourced), forces named operands out of scope (they type into `aimBuffer`, which *is* the line), adds a lifetime and a poison sweep, **and still needs a program-memory anchor** for `addStepInProgram` to insert against and for `decodeOneStep` to read. It is strictly more work for strictly less coverage.

### (c) Text synthesis — none; reuse `forthCaptureResume` as-is

**Mechanism: `packages/forth-core/programming/manage.c:1240-1256`, unmodified.**

`n = getNumberOfSteps() - forthCapSavedStepCount()` → `ins = findNextStep(currentStep)` → `decodeOneStep(ins)` → `forthCapInsertName(tmpString)` → `deleteStepsFromTo(ins, findNextStep(ins))`.

Why this works unchanged interactively:
- `addStepInProgram`'s pre-move (`packages/forth-core/programming/manage.c:2264-2266`) places the TAM step **after** `currentStep`, which suspend deliberately left parked on the capture step (`packages/forth-core/programming/manage.c:1192-1197`, confirmed). `findNextStep(currentStep)` therefore finds it, identically to PEM.
- `forthCapInsertName` (`packages/forth-core/forth_menu.c:32-53`) is gated **only** on `forthCapIsOpen()` at `:35` — no `calcMode`, no `tam.function`. Verified this pass. It touches `aimBuffer` and `T_cursorPos` only. Fully reusable.
- `decodeOneStep` receives a real in-program step, so the name clamp at `packages/forth-core/programming/decode.c:129-133` is a no-op and `findKey2ndParam`'s `programBytesAvailable` window (`src/c47/programming/nextStep.c:17`) is satisfied.

**No function is extracted. No function is refactored. `manage.c`'s fold body is not edited.**

One latent defect inherited from PEM, which this stage should fix while it is in the file: `packages/forth-core/programming/manage.c:1245` rejects only on `stringByteLength(tmpString) > 255`. `_decodeOneStep`'s `case PTP_DISABLED:` (`packages/forth-core/programming/decode.c:917-922`) writes **nothing** to `tmpString`, leaving stale text from a previous caller. Add `tmpString[0] = 0;` immediately before `decodeOneStep(ins);` at `packages/forth-core/programming/manage.c:1244`, and treat an empty result as the "keep the step" break. This is a one-line PEM bug fix, and per the bug-fix class-test rule it lands with a reproducer and a class-level test.

---

## 3. THE `determineItem` PROBLEM

**The defect.** `packages/forth-core/keyboard.c:1686` opens the AIM-column arm with `calcMode == CM_AIM` as its **first disjunct**, before the `else if(tam.mode)` arm at `:1723` that selects `key->primaryTam`. In PEM the equivalent disjunct is `calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && …`, and `forthCaptureSuspend` clears `FLAG_ALPHA` at `packages/forth-core/programming/manage.c:1202` — which is precisely what lets a PEM TAM fall through to `primaryTam`. `calcMode == CM_AIM` has no such escape: it is unconditional. An interactive TAM would resolve digit keys to letters and be unusable.

**Exact minimal fix** — one disjunct at `packages/forth-core/keyboard.c:1686`:

```c
    else if((calcMode == CM_AIM && !(tam.mode && forthFoldPending())) || (catalog && …
```

Everything else on that line is untouched.

**Why it cannot break any existing caller.** `forthFoldPending()` reads `forthCap.foldMode`, which is written at exactly two places: `forthFoldEnter` (set) and `forthFoldLeave` (cleared). `forthFoldEnter` is called from exactly one site, the new arm at `packages/forth-core/ui/tam.c:1181`, under `calcMode != CM_PEM && forthCapIsInteractive()`. Before this stage, `forthCapIsInteractive()` is false everywhere (`origin` is only ever set by L1's interactive open). Therefore **the predicate's value is identical to today's for every execution that exists today** — this is provable from the write-set, not from testing.

**What changes after the stage, precisely.** Only this state: interactive capture + `tam.mode != 0` + fold/park pending. In it:
- `tam.alpha` false (numeric/register/flag/label-by-number operands) → falls to `packages/forth-core/keyboard.c:1723` → `key->primaryTam` — the same column a PEM TAM gets.
- `tam.alpha` true (named operands) → the `tam.alpha` disjunct on the same line still fires → AIM column → letters — again the same as PEM.

Parity in both branches, by the same mechanism PEM uses.

**Rejected alternative:** the broader `calcMode == CM_AIM && !tam.mode`. It also fixes what looks like a genuine pre-existing bug (a numeric TAM entered natively from CM_AIM is currently unkeyable), but it changes upstream behaviour on a path this stage has not traced and cannot test. File it as a separate observation; do not bundle it.

---

## 4. REGRESSION SURFACE

| # | What could break | Protecting gate / anchor |
|---|---|---|
| 4.1 | PEM fold behaviour changes | `packages/forth-core/ui/tam.c:1180` keeps `calcMode == CM_PEM` as a disjunct; `forthFoldEnter` is called only under `calcMode != CM_PEM`. `forthCaptureSuspend`/`forthCaptureResume` bodies (`packages/forth-core/programming/manage.c:1180-1290`) are **not edited** except for the `tmpString[0]=0` fix at `:1244`. |
| 4.2 | PEM resume double-fires or fires late | `packages/forth-core/ui/tam.c:1406` keeps `calcMode == CM_PEM` as a disjunct; `forthFoldLeave` early-returns on `foldMode == 0` (always true in PEM). |
| 4.3 | `tamProcessInput` bracket perturbs PEM or native AIM | `brk = forthFoldArmed()` is false whenever `foldMode != 1`, which in PEM is always. The function is otherwise byte-identical to `packages/forth-core/ui/tam.c:1414-1417`. |
| 4.4 | `determineItem` changes for any existing flow | §3: write-set argument. Guarded by `forthFoldPending()`. |
| 4.5 | Native AIM TAM (no Forth capture) | Every new predicate is false; bracket off; no step materialised. `packages/forth-core/ui/tam.c:1180`'s new arm requires `forthCapIsOpen()`. |
| 4.6 | Program listing visibly flashes during the interactive TAM | The bracket does not span a refresh — `packages/forth-core/screen.c:6151` dispatch and `:5637` overlay both see `CM_AIM`. Verified: no `refreshScreen` in `_insertInProgram`'s tail (`packages/forth-core/programming/manage.c:770-772`) or in `goToGlobalStep` (`packages/forth-core/programming/lblGtoXeq.c:101-140`). |
| 4.7 | Program memory grows and is not given back | `_insertInProgram` may call `resizeProgramMemory` (`packages/forth-core/programming/manage.c:723-728`); `deleteStepsFromTo` (`:221-228`) does not shrink. A fold can permanently grow program memory by up to one block. Same exposure PEM already has; measure and report with the stage. On DMCP an OOM grow calls `backToSystem(NOPARAM)` (`src/c47/memory.c:181`). |
| 4.8 | Transient step leaks into the user's program | `forthFoldLeave`'s unconditional sweep covers every break path in `packages/forth-core/programming/manage.c:1246`/`:1251` and the PARK case. The single bypass of `leaveTamModeIfEnabled` is `doFnReset` (`packages/forth-core/config.c:1794`), which resets program memory anyway. A power-cut mid-TAM leaks one `ITM_FORTH` step, which is exactly the §8.1 leaked-placeholder case with the existing EDIT recovery gesture (`packages/forth-core/programming/manage.c:892-909`). |
| 4.9 | The user's PEM cursor position moves | `forthFoldLeave` restores via `goToGlobalStep(savedGlobalStep)` + `firstDisplayedLocalStepNumber` + `defineFirstDisplayedStep()` + `pemCursorIsZerothStep`. `savedGlobalStep` uses `_insertInProgram`'s own formula at `packages/forth-core/programming/manage.c:769`. |
| 4.10 | `labelList` / step numbering disturbed | `scanLabelsAndPrograms()` runs on insert (`packages/forth-core/programming/manage.c:770`) and delete (`:228`); insert-then-delete is byte-exact, so the final scan restores the entry state. |
| 4.11 | Catalog-driven TAM commit diverges from PEM | **Known v1 limitation, not fixed.** `packages/forth-core/keyboard.c:1148` and `:1160` gate three commit sites (`:1154`, `:1167`, `:1191`) on `calcMode == CM_PEM`; they bypass `tamProcessInput` entirely and the bracket does not reach them. Interactively they do not fire. The capture is never lost (the step is the store), but a flag-by-name or dynmenu-label pick during an interactive TAM will not fold. Deferred to P6. |
| 4.12 | `popSoftmenu` takes the wrong arm during the bracket | It takes the PEM arm (`packages/forth-core/softmenus.c:3725-3730`) rather than the CM_AIM arm (`:3731-3733`) — which is exactly what PEM does, and `forthCaptureResume` then re-pushes `-MNU_ALPHA` explicitly at `packages/forth-core/programming/manage.c:1284-1286`. Parity, not regression. |
| 4.13 | GTO. moves the program pointer mid-fold | Excluded by `_forthFoldAdmits` (`ITM_GTOP` → PARK). The unguarded `fnGoto`/`goToPgmStep` at `packages/forth-core/ui/tam.c:891`/`:898` therefore never run inside a fold. |

---

## 5. PACKET DECOMPOSITION

| # | Scope (one line) | Acceptance criterion | Prereq |
|---|---|---|---|
| **P0** | *(L1, not this fold)* interactive capture exists on the AIM surface with `forthCap.origin = INTERACTIVE` and `forthCapIsInteractive()`. | An interactive capture opens, accepts alpha input into `aimBuffer`, closes via the E14 choke. | — |
| **P1** | `forthFoldCtx` + `forthFoldEnter`/`forthFoldLeave`/`forthFoldArmed`/`forthFoldPending`/`_forthFoldAdmits` in `manage.c`. **No wiring into `tam.c`.** | Self-test in `packages/forth-core/test_engine.part.h`: with an open interactive capture holding known text, `forthFoldEnter` then `forthFoldLeave` with no TAM in between leaves `getNumberOfSteps()`, `firstFreeProgramByte`, `currentStep`, `currentLocalStepNumber`, `firstDisplayedLocalStepNumber`, `pemCursorIsZerothStep`, `aimBuffer` and `T_cursorPos` **bit-identical** to entry. Also passes with an empty program (only `.END.`). | P0 |
| **P2** | The `tmpString[0] = 0;` guard before `decodeOneStep` at `packages/forth-core/programming/manage.c:1244`, plus empty-result → break. | Reproducer: a `PTP_DISABLED` step folded after an unrelated `tmpString` write inserts nothing instead of stale text. Class test over every `PTP_DISABLED` item reachable from a TAM commit. **PEM-only change — lands and ships independently of the rest.** | — |
| **P3** | The three `tam.c` seams: `:1180` widened, `:1406` widened, `:1414` bracketed. | STO 0 5 during an interactive capture leaves `aimBuffer == "STO 05 "` and program memory bit-identical to before the keypress. The full landed F6/K PEM test suite passes unchanged. | P1 |
| **P4** | `determineItem` fix at `packages/forth-core/keyboard.c:1686`. | With the fold pending, keys 0-9 resolve to `key->primaryTam`; with `tam.alpha` set they still resolve to `key->primaryAim`; every existing `determineItem` expectation is byte-identical when no fold is pending. | P3 |
| **P5** | Operand-class parity battery. | For each of {direct register, dotted local register, indirect register, indirect variable, flag by number, dotted local flag, named global label, named local label (`tam.colon`), `TM_VALUE` > 250, `TM_VALUE` min/max edge, `TM_MENU`, `TM_SHUFFLE`}: the interactive fold's resulting line text is **string-equal** to the PEM fold's for the same key sequence, asserted pairwise in one test. | P4 |
| **P6** | Cancel / abandon / sweep / close-paths class test. | EXIT mid-TAM, backspace-to-empty, error at commit (`lastErrorCode != 0` → `packages/forth-core/ui/tam.c:1102` inserts nothing), oversize-text break, PARK-mode commit: in every case the capture line is intact, `T_cursorPos` valid, and `getNumberOfSteps()` equals the pre-fold count. Extends the existing E14 close-paths class test with the "no outstanding transient step" invariant. | P5 |
| **P7** | *(follow-on, optional)* widen `packages/forth-core/keyboard.c:1148` and `:1160` to `(calcMode == CM_PEM \|\| forthFoldArmed())` with a `calcMode` bracket around each block body. | Catalog-driven flag-by-name and dynmenu-label picks fold interactively with the same text PEM produces. | P6 |

P2 is independent of everything and can land first as a standalone PEM fix. P1 → P3 → P4 → P5 → P6 is a strict chain.

---

## 6. WHAT IS STILL UNVERIFIED

1. **That L1's interactive capture really is `calcMode == CM_AIM` with `FLAG_ALPHA` set and the line in `aimBuffer`.** The whole shape assumes it. `design-docs/forth-core/STAGE_L_INTERACTIVE.md` L1 says "the natural host is the AIM surface (`ITM_AIM` seam)" — a proposal, not landed code. Settle by reading L1's landed open path once it exists; until then P1's self-test must set that state explicitly.

2. **Whether a softkey press in `CM_AIM` with `tam.mode != 0` can execute live before reaching `tamProcessInput`.** `executeFunction` (`packages/forth-core/keyboard.c:954`) has `calcMode != CM_PEM` arms *above* the TAM chain — `:1047` (`-MNU_Sfdx` → `tamEnterMode`) and `:1053-1060` (`ITM_INTEGRAL`, whose `case CM_AIM:` at `:1059` calls `closeAim()`). I read those four lines but did **not** trace the full `CM_AIM` + `tam.mode` softkey fallthrough.
   `awk 'NR>=1040 && NR<=1240' packages/forth-core/keyboard.c` — read the whole chain and record which arms are reachable with `tam.mode != 0`.

3. **What renders on the AIM register line during `CM_AIM` + `tam.mode`.** `packages/forth-core/screen.c:3881` gates the AIM line on `calcMode == CM_AIM && !tam.mode`, so something else paints it. This is pre-existing native behaviour, not a regression, but the fold's look depends on it.
   `awk 'NR>=3860 && NR<=3900' packages/forth-core/screen.c` plus a `run-sim` capture of a native AIM TAM.

4. **Whether `_decodeOneStep`'s `PTP_DISABLED` arm is reachable from any TAM-committable item** (drives whether P2's class test is non-empty).
   `grep -n "PTP_DISABLED" packages/forth-core/items.c | wc -l` then cross-reference those rows' `tamMinMax` for a non-zero max.

5. **`resizeProgramMemory` worst case for the transient pair.** The capture step is `4 + len` bytes (`packages/forth-core/programming/manage.c:846-856`); the TAM step is up to ~259 for a named operand. Whether that can cross a block boundary and permanently grow program memory needs a measurement, not a read.
   Instrument `freeProgramBytes` around `forthFoldEnter`/`forthFoldLeave` in P1's self-test with a worst-case name.

6. **Whether `tam.value`'s `TM_MENU` / `MNU_DYNAMIC` path (`packages/forth-core/programming/manage.c:2191-2195`) reads a `tmpString` prefix the fold does not supply.** `manage.c:1919-1920` snapshots inbound `tmpString` into a 16-byte `buffer`. In PEM the caller happens to leave a menu name there; interactively `tmpString` was last written by `_forthCapBuildStep` at fold entry. `TM_MENU` is currently admitted by `_forthFoldAdmits` — if this is unsafe, move `TM_MENU` to PARK.
   `awk 'NR>=2186 && NR<=2200' packages/forth-core/programming/manage.c` and trace who sets `tmpString` before `addStepInProgram(tamOperation())` on the `TM_MENU` path in PEM.

---

### Refuted claims deliberately NOT built on

- The "six commit sites" list (`design-docs/forth-core/DESIGN-HISTORY.md:825`, `design-docs/forth-core/STAGE_L_INTERACTIVE.md:234`) and every later enumeration of TAM effect/termination sites. **Superseded, not corrected** — the bracket makes enumeration unnecessary. If P7 or a future stage needs one, it must be re-traced mechanically (`grep -n 'addStepInProgram\|reallyRunFunction\|runFunction\|mimRunFunction\|forthDispatchColon\|insertStepInProgram' packages/forth-core/ui/tam.c`), not taken from any prior document.
- Any scratch-buffer decode plan, including the `firstFreeProgramByte` override borrowed from `packages/forth-core/test_params.part.h:1114-1119`. Not used; `programBytesAvailable`'s lower bound (`src/c47/programming/nextStep.c:17`) makes it unsound for `PARAM_KEYG_KEYX` in either direction.
- Any plan to extract or split `insertStepInProgram`.
- "`tamProcessInput` is the sole choke for TAM *commits*" — false (`packages/forth-core/keyboard.c:1154`, `:1167`, `:1191`); it is the sole choke for TAM *keystrokes*, which is all the bracket needs. §4.11 records the gap.