# PACKET L1-1 — capture origin bit, interactive open, minimum close

**Stage L packet 1** (design: STAGE_L_INTERACTIVE.md, rulings L-R1..L-R8;
evidence: STAGE_L_TRACES.md §T1, §T2, §T6). **Prerequisite: L1-0 must be
landed and green.**

**Revision 2 (2026-08-04)** after an adversarial review against the tree.
Rev 1 had five blockers; the two structural ones were architect errors and
are called out where they land (C1's resume bracket, and the scope claim
that an interactive capture "closes cleanly by every existing seam" — it
had no close path at all). Rev 1's §C3 is **deleted**, not fixed — see
"Why there is no restore sanitizer" below.

**Scope.** The capture object learns *where it was opened from*;
`fnForthOuter` becomes an interactive capture opener; and every existing
`CM_AIM` teardown closes it. This packet does NOT add ENTER semantics or
the REPL loop (L1-2), the dispatch divert (L1-3), or the fold (L1-F*).
After it lands, pressing FORTH outside PEM opens an alpha line, EXIT
leaves it, and nothing else has changed.

**Why the close is in scope even though EXIT belongs to L1-2.** Nothing
in the landed tree closes an interactive capture: `closeAim()`
(src/c47/bufferize.c:2693-2712) never mentions `forthCap`, and every
production `forthCapClose()` site (manage.c:1005, :1138, :1143) sits
inside `pemAlpha`/`pemCloseAlphaInput`, reached only under `CM_PEM`. A
leaked `FCAP_OPEN` is **not inert**: `insertStepInProgram`'s
`func == ITM_AIM && forthCapIsOpen()` arm (manage.c:1719-1734) has no
origin discriminator, so the next PEM ALPHA press would toggle keys mode
instead of opening literal input. L1-1 must therefore ship its own close
or leave the machine broken.

## Implementer contract (Claude-subagent edition)

- Isolated git worktree. Work ONLY through `packages/forth-core/` — never
  edit `patches/` or `files/` (generated), never `src/c47/` (upstream).
- Gate: `./packages/forth-core/build-test.sh`. Redirect to a log, inspect
  with bounded `grep -a`. Green iff the log shows
  `FORTH SELF-TEST: ALL PASSED` **and** `BUILD + SELF-TEST GREEN`.
- **STOP conditions (report, do not adapt):** a red test this packet did
  not write; any EXECUTION GATE anchor not matching; any spec statement
  here that contradicts the tree.
- Fixture rules (binding): clear `lastErrorCode` per subcase;
  `dynamicMenuItem = -1` and `programRunStop = PGM_STOPPED` before
  `fnGotoDot`; never prime the state under test; `compareString` returns 0
  on equal; a PEM capture is opened by DRIVING it (`runFunction(ITM_AIM)`
  with the cursor ON the opening marker).
- Every mutation must be applied, shown RED at the named assertion, and
  reverted. Record each RED line verbatim.
- Report the `FORTH ARENA` line (§5.4) and the measured
  `make dmcp5r47 CUSTOM_PKG=packages/forth-core` flash delta (RULE-1).
- **Quote the `[DEBUG] running …` line and every PASS line for the new
  tests verbatim from the green log.** The `ALL PASSED` banner alone is
  not evidence a new test ran — precedent, this stage: commit `14fecc428`,
  where K4's registrations sat inside the suite's `if(fail)` branch and
  the landing gate never ran them.

## EXECUTION GATE (verify before any edit; STOP on mismatch)

```
grep -c "forthTestRunFromX" packages/forth-core/forth_compile.c        # >=1 (L1-0 landed)
grep -c "fnForthOuter(NOPARAM)" packages/forth-core/test_params.part.h # 0   (L1-0 landed)
grep -n "uint8_t     keysMode;" packages/forth-core/forth_capture.h
grep -c "origin" packages/forth-core/forth_capture.h                   # 0 (not implemented)
grep -n "bool_t keysWas = forthCapKeysMode" packages/forth-core/programming/manage.c
grep -c "closeAim()" packages/forth-core/keyboard.c                    # record the count
grep -n "test_capture_close_paths_reset_tuple" packages/forth-core/test_dict_reloc.c
grep -n "void fnForthOuter" packages/forth-core/forth_compile.c
```

## C1 — `forth_capture.{h,c}`: the origin bit

Add after `keysMode`:

```c
typedef enum { FCAP_ORIGIN_PEM = 0, FCAP_ORIGIN_INTERACTIVE = 1 } forthCapOrigin_t;
```
```c
  uint8_t     origin;         /* L1-1: forthCapOrigin_t.  PEM captures live on
                                 a program step; INTERACTIVE captures live on
                                 the AIM surface (L-R2).  Transient, NEVER
                                 persisted.  Zero is PEM so every zero-init and
                                 memset-style reset means "PEM" — matching every
                                 capture that existed before Stage L. */
```

No explicit `pad`: `state`/`keysMode`/`origin` are three `uint8_t`s
followed by `uint16_t savedCursor`, and the compiler already inserts one
byte. **Report `sizeof(forthCap_t)` before and after; it must be
unchanged.** If it changes, STOP.

**Declarations** (`forth_capture.h`, beside `forthCapOpen` at :52 and
`forthCapKeysMode` at :58):

```c
void   forthCapOpenInteractive(void);   /* L1-1: open with INTERACTIVE origin */
bool_t forthCapIsInteractive(void);     /* origin == INTERACTIVE && state != CLOSED */
uint8_t forthCapOriginRaw(void);        /* the raw field — for the resume bracket */
void   forthCapSetOrigin(uint8_t o);    /* the raw field — for the resume bracket */
#if defined(FORTH_DEBUG_SELFTEST)
uint8_t forthTestCapOrigin(void);       /* raw field, so the E14 sweep is falsifiable */
#endif
```

The `state != FCAP_CLOSED` conjunction in `forthCapIsInteractive` is
deliberate defence-in-depth. It is **not** falsifiable on its own (the
E14 reset makes `(CLOSED, INTERACTIVE)` unreachable), which is why
`forthTestCapOrigin()` exists: the E14 sweep asserts the raw field, so
mutation 2 is killable.

**Open.** Split `forthCapOpen`:

```c
static void _forthCapOpenAs(uint8_t origin) {
  aimBuffer[0] = 0;
  forthCap.state    = FCAP_OPEN;
  forthCap.keysMode = 0;
  forthCap.origin   = origin;
}
void forthCapOpen(void)            { _forthCapOpenAs(FCAP_ORIGIN_PEM); }
void forthCapOpenInteractive(void) { _forthCapOpenAs(FCAP_ORIGIN_INTERACTIVE); }
```

**Reset — E14.** `origin = FCAP_ORIGIN_PEM` joins `keysMode` at exactly
three sites: `forthCapClose()`, `forthCapAbandonSuspended()`,
`forthCapPowerReset()`. No fourth *reset* site.

**The resume re-open is a fourth WRITE site and needs a bracket.**
`forthCaptureResume` calls `forthCapOpen()` at manage.c:1224 — that is
the SUSPENDED→OPEN re-open, **not** a PEM open, and `forthCapOpen`
unconditionally zeroes the fields. `keysMode` survives only because
resume brackets it explicitly (manage.c:1220-1226,
`bool_t keysWas = forthCapKeysMode(); … forthCapSetKeysMode(keysWas);`).
`origin` inherits the identical clobber. Extend that same bracket:

```c
  { bool_t keysWas   = forthCapKeysMode();
    uint8_t originWas = forthCapOriginRaw();
    ...
    forthCapOpen();
    forthCapSetKeysMode(keysWas);
    forthCapSetOrigin(originWas);
```

Suspend/resume is PEM-only today (`ui/tam.c:1181`, `:1408` both gate on
`calcMode == CM_PEM`), so this is inert by construction until L1-F* — but
it must be correct now, because L1-F* arms it.

## C2 — `forth_compile.c`: `fnForthOuter` becomes the interactive opener

**Include.** `forth_compile.c`'s include block (`:7-12`) does not reach
`forth_capture.h`, and there is no transitive route. Add
`#include "forth_capture.h"` there. Without it the new `bool_t` functions
are implicitly declared `int` and the build stays green
(`meson.build` is `warning_level=2`, no `werror`) with an ABI-unspecified
return — a silent defect.

**C2a — the running-program guard (do NOT omit).** `ITM_FORTH` is
`PTP_REM` (items.c:4771) and `forthResolveXEQ` deliberately keeps
resolving it (its own comment: "ITM_FORTH (PTP_REM) keeps resolving") —
the item arm rejects only `PTP_DECLARE_LABEL..PTP_MENU`, and `PTP_REM` is
14 (defines.h:1136-1149). So a program step `XEQ 'FORTH'` reaches
`reallyRunFunction(ITM_FORTH, NOPARAM)` → `fnForthOuter`
(items.c:718-724) **while a program runs**. Unguarded, L-R2 would open an
interactive capture mid-run. First statement in the function:

```c
  if (programRunStop == PGM_RUNNING) {
    forthOuterCtx_t ctx;
    ctx.savedScope = forthCurrentScope;
    if (!forthTakeSourceFromX(ctx.source)) { return; }
    forthOuterRun(&ctx, FORTH_OUTER_FULL);
    return;
  }
```

**The shared X read** (used by C2a, the seed, and L1-0's
`forthTestRunFromX`, which is rewritten to call it so they cannot drift):

```c
/* Returns false with the documented error displayed and X untouched when X
 * is not a string or the line is oversize.  On true, dst holds the
 * NUL-terminated line and X HAS BEEN DROPPED.  Copy MUST precede drop:
 * drop invalidates the string (§3.3.2). */
static bool_t forthTakeSourceFromX(char *dst);
```
(body exactly as `fnForthOuter`'s current head, ending
`xcopy(dst, …, len + 1); fnDrop(NOPARAM); return true;`)

**`fnForthOuter` (new body):**

```
fnForthOuter(unused):
  if programRunStop == PGM_RUNNING:  one-shot interpret from X; return   /* C2a */

  seeded = false
  char seed[FORTH_SOURCE_MAX]
  if getRegisterDataType(REGISTER_X) == dtString:
      if !forthTakeSourceFromX(seed): return   /* oversize: error, NO capture */
      seeded = true

  if catalog:                                   /* T6: FIX-9 analog */
      leaveAsmMode()
      for i in 0 .. SOFTMENU_STACK_SIZE-1:
          if !(forthCatalogMenuOnTop() or forthCatalogBuriedOnStack()): break
          popSoftmenu()

  forthEnterAimSurfaceNoLift()                  /* see below — NOT fnAim */
  forthCapOpenInteractive()                     /* clears aimBuffer; cannot fail */
  T_cursorPos = 0
  displayAIMbufferoffset = 0

  if seeded:
      xcopy(aimBuffer, seed, stringByteLength(seed) + 1)
      /* Empty-line guard, copied from the landed PEM idiom (manage.c:900-904):
       * an EMPTY string in X is a valid dtString and passes the size check, and
       * stringLastGlyph("") + 1 == 1 would put the cursor one past the NUL and
       * silently eat every keystroke. */
      T_cursorPos = (aimBuffer[0] == 0) ? 0 : stringLastGlyph(aimBuffer) + 1
```

### C2b — the AIM surface WITHOUT the stack lift (T9 — do not shortcut)

**`fnAim` must not be used.** `fnAim` → `calcModeAim`
(src/c47/calcMode.c:62-76) calls `liftStack()`, which ends
**unconditionally** with

```c
  setRegisterDataPointer(REGISTER_X, allocC47Blocks(REAL34_SIZE_IN_BLOCKS));
  setRegisterDataType(REGISTER_X, dtReal34, amNone);
```

replacing X with an **uninitialised** `dtReal34` — pushed to Y when
`FLAG_ASLIFT` is set, freed outright when it is not. The interactive
capture's premise is that the line operates on the **live stack** (L-R2
drops a seeded string precisely "so interpreted words see a clean
stack"). With a lifting open, `16` in X followed by typing `1 +` and
ENTER computes `garbage + 1`. See STAGE_L_TRACES.md §T9.

Repairing after `fnAim` is **not** an option: the repair is conditional on
`FLAG_ASLIFT`, and when it is clear the old X has already been freed and
is unrecoverable. An override of `calcMode.c`/`bufferize.c` is **not** an
option: new upstream patch surface for one call, against S1 discipline.

Write the non-lifting equivalent in `forth_compile.c` beside
`fnForthOuter`. It is `calcModeAim`'s body **minus `liftStack()`**, and
nothing else may differ — read src/c47/calcMode.c:62-92 and mirror it:

```c
/* T9: calcModeAim's setup WITHOUT its liftStack().  Every line below is
 * calcModeAim's (src/c47/calcMode.c:62-92); the ONLY omission is the lift,
 * because an interactive Forth line operates on the live stack.  If
 * calcModeAim gains a statement upstream, this must gain it too — the
 * rebase discipline for this function is "diff it against calcModeAim". */
static void forthEnterAimSurfaceNoLift(void) {
  alphaCase = CAPS_AIM_DEFAULT;
  nextChar  = NC_NORMAL;
  clearSystemFlag(FLAG_NUMLOCK);
  scrLock   = NC_NORMAL;

  calcMode = CM_AIM;
  /* NO liftStack() — T9 */
  clearRegisterLine(AIM_REGISTER_LINE, true, true);
  xCursor = 1;
  yCursor = Y_POSITION_OF_AIM_LINE + 6;
  cursorFont = &standardFont;
  cursorEnabled = true;

  showSoftmenu(-MNU_ALPHA);
  if(softmenuStack[0].softmenuId == 0) { softmenuStack[0].softmenuId = 1; }
  setSystemFlag(FLAG_ALPHA);
  calcModeAimGui();
}
```

**Verify against the tree before landing**: read src/c47/calcMode.c:62-92
and report any statement of `calcModeAim`'s that this omits other than
`liftStack()`. A silent omission is a STOP condition. Note `calcModeAim`
guards its `calcMode`/lift block on
`!tam.mode && calcMode != CM_ASSIGN && calcMode != CM_PEM &&
calcMode != CM_ASN_BROWSER`; state in your report whether
`fnForthOuter` can be reached in any of those states and, if so, add the
same guard.

`forthCapOpenInteractive()` clears `aimBuffer` itself, which is why the
seed copy follows it. The X read still precedes everything, because
`forthTakeSourceFromX` drops X and the drop must happen before the
surface is set up.

**Test obligation (replaces T1.2's and T1.5's "X untouched" wording,
which was written against a lifting open and would have masked this):**

- T1.2 asserts X is **bit-identical** to its pre-FORTH value — same type,
  same value — via `read_reg_int32`, not merely "not a string".
- New subcase: put `16` in X, open interactive, type `1 +`, ENTER, assert
  X == 17. This is the assertion that actually pins T9.
- New subcase: assert `getStackTop()`-relative depth is unchanged by the
  open (nothing was pushed).

**Mutation 9:** replace `forthEnterAimSurfaceNoLift()` with
`fnAim(NOPARAM)`. Expect RED at the `1 +` subcase.

**`tam.function` is NOT set.** It is the PEM capture's sentinel, keyed to
`calcMode == CM_PEM` at every consumer; setting it interactively leaks PEM
behaviour into plain alpha. The interactive origin is carried by
`forthCap.origin` alone. **A subagent that "helpfully" sets
`tam.function = ITM_FORTH` here has broken the stage.**

**Drain helper wrappers** (manage.c; the two helpers are file-static there,
declared manage.c:1165-1166), declared in `forth_capture.h`:

```c
bool_t forthCatalogMenuOnTop(void)     { return _forthCatalogMenuOnTop(); }
bool_t forthCatalogBuriedOnStack(void) { return _forthCatalogBuriedOnStack(); }
```

## C3 — the minimum close

Add to `packages/forth-core/keyboard.c`, above the first use:

```c
/* L1-1: nothing in the landed tree closes an INTERACTIVE capture —
 * closeAim() (upstream bufferize.c) does not know about forthCap, and every
 * forthCapClose() production site is CM_PEM-gated inside pemAlpha.  A leaked
 * FCAP_OPEN is not inert: insertStepInProgram's ITM_AIM arm
 * (manage.c:1719-1734) has no origin discriminator and would toggle keys mode
 * on the next PEM ALPHA press.  L1-2 replaces this with the full E8 ladder;
 * until then, every AIM teardown closes the capture. */
static void _forthCapCloseIfInteractive(void) {
  if(forthCapIsInteractive()) { forthCapClose(); }
}
```

**Call it immediately before EVERY `closeAim()` call site in
`packages/forth-core/keyboard.c`.** Enumerate them yourself from the
EXECUTION GATE count and **report the list with line numbers** — the
count is the completeness contract. Do not add call sites in any other
file, and do not modify `closeAim()` (upstream, not overridden — adding a
`bufferize.c` override for this is out of scope and would be new patch
surface for one line).

## Why there is no restore sanitizer in this packet

Rev 1 proposed widening `forthCaptureSanitizeRestoredUi` for the
interactive origin. **Dropped, and no owner ruling is needed** — there is
nothing to repair:

- `forthCap` is process-local and `forthCapPowerReset()` runs at the
  dictionary init/validate seams (forth_dict.c:57, :71), so after any
  restore the capture is already CLOSED.
- What remains is `calcMode == CM_AIM` + `FLAG_ALPHA` + the line in
  `aimBuffer` — which is **exactly a native alpha session**, and upstream
  restores that deliberately (saveRestoreBackup.c:1545-1549:
  `else if(calcMode == CM_AIM) { calcModeAimGui(); cursorEnabled = true; }`).
- The PEM arm exists because PEM's residue has a *Forth-specific*
  persisted marker to key on (`tam.function == ITM_FORTH`,
  manage.c:826-828). The interactive origin has none — C2 forbids setting
  `tam.function`, and `origin` is never persisted. Any arm keying on
  `CM_AIM + FLAG_ALPHA` alone would tear down a legitimate restored alpha
  session and discard the user's line.

So a save taken mid-interactive-capture restores as a usable alpha line
with no capture behaviour attached. That is a correct outcome, not a
defect. Record it in DESIGN.md at stage close as the interactive analogue
of the §8 A5 power-off contract.

## C4 — tests

Both new tests need **explicit registration** — `test_capture.part.h` is
bodies only, `#include`d at test_dict_reloc.c:2452, *after* the runner.
Forward-declare beside test_dict_reloc.c:1125 and invoke beside :2085-2086,
**outside** any `if(fail)` branch.

### `test_capture_origin_lifecycle` (new, test_capture.part.h)

Save/restore the full global tuple around the test, per the landed idiom
at test_capture.part.h:3604-3616.

1. **Default is PEM.** Drive a PEM capture open; assert
   `forthTestCapOrigin() == FCAP_ORIGIN_PEM` and `!forthCapIsInteractive()`.
2. **Interactive open.** From `CM_NORMAL`, non-string X, `fnForthOuter(NOPARAM)`;
   assert `FCAP_OPEN`, `forthCapIsInteractive()`, `calcMode == CM_AIM`,
   `FLAG_ALPHA` set, `aimBuffer[0] == 0`, `T_cursorPos == 0`, and
   **`tam.function == 0`**.
3. **Seed consumes X.** Put a known value in Y and `"1 2 +"` in X via
   `x_set_string`; call `fnForthOuter`; assert the line is `"1 2 +"`, that
   X now holds Y's former value (the drop happened — use the landed
   `read_reg_int32` idiom, not a self-confirming re-derivation), and that
   `T_cursorPos == 5` **as a literal** (independent oracle: `"1 2 +"` is
   five single-byte glyphs).
4. **Empty string in X.** `x_set_string("")`; call `fnForthOuter`; assert
   the capture opened, `aimBuffer[0] == 0`, and **`T_cursorPos == 0`**
   (the M3 guard). Then type one character through the real path and
   assert it lands at offset 0.
5. **Oversize refuses.** 300-byte string in X; assert
   `lastErrorCode == ERROR_INVALID_DATA_TYPE_FOR_OP`, `FCAP_CLOSED`,
   `calcMode` unchanged, X still holds the string.
6. **Running program keeps the one-shot.** `programRunStop = PGM_RUNNING`,
   `x_set_string("1 2 +")`, `fnForthOuter(NOPARAM)`; assert X == 3,
   `FCAP_CLOSED`, `calcMode` unchanged. Restore `programRunStop`.
7. **Origin rides a suspension — state level only.** Open interactive,
   call `forthCapSuspendState(0,0,0,0)` directly, assert
   `forthCapIsInteractive()` still true in `FCAP_SUSPENDED`, then
   `forthCapClose()`. **Do NOT call `forthCaptureSuspend`/`Resume`** on an
   interactive capture: `forthCaptureSuspend` guards only on
   `forthCapIsOpen()` (manage.c:1181), so it would proceed and
   `forthCapRecommitStep()` (manage.c:1173-1175) would
   `deleteStepsFromTo` whatever `currentStep` points at — with the landed
   fixture that is the 4-byte `.END.` (test_dict_reloc.c:548). The real
   round-trip belongs to L1-F*, which owns the interactive suspend.
8. **CLOSED reads as not-interactive.** After an interactive capture
   closes, assert `!forthCapIsInteractive()` and
   `forthTestCapOrigin() == FCAP_ORIGIN_PEM`.
9. **Catalog drain.** Open an FCNS catalog, drive FORTH from it, assert no
   catalog menu remains on `softmenuStack` and the capture is open.

### Extend `test_capture_close_paths_reset_tuple` (test_capture.part.h:6925)

Add `origin` to the asserted close tuple, using `forthTestCapOrigin()`.
**Do NOT add interactive rows.** That test is a hardwired
`for (sc = 1; sc <= 4; sc++)` over four *close* paths with a single PEM
open above the switch (:6951, :6964, :6982-6983), and all four close
drives require `CM_PEM` — case 4 in particular calls
`runFunction(ITM_FORTH)`, which outside PEM now falls to `fnForthOuter`
and would **re-open** the capture it is asserting closed. The interactive
close sweep arrives with L1-2's EXIT ladder. Honest count: 4 before, 4
after.

### `test_capture_interactive_close` (new, test_capture.part.h)

Open interactive, then for each `closeAim()` call site reachable by a
driven key (at minimum `fnKeyExit` in `CM_AIM`), assert afterwards:
`FCAP_CLOSED`, `forthTestCapOrigin() == FCAP_ORIGIN_PEM`,
`!getSystemFlag(FLAG_ALPHA)`. Report which sites you could drive and
which you could only reach by direct call.

## Mutations (seven; each shown RED, then reverted)

1. `forthCapIsInteractive()` → bare `origin == FCAP_ORIGIN_INTERACTIVE`.
   RED at T1.8 **only if** T1.8 also asserts `forthTestCapOrigin()`; if it
   does not go red, report it rather than deleting the mutation — the
   conjunction is defence-in-depth over an unreachable state.
2. Remove `origin = FCAP_ORIGIN_PEM` from `forthCapClose()`. RED at the
   extended close-paths tuple (via `forthTestCapOrigin()`).
3. Set `tam.function = ITM_FORTH` in the interactive open. RED at T1.2.
4. Move the seed copy above `forthCapOpenInteractive()`. RED at T1.3.
5. In `forthTakeSourceFromX`, move `fnDrop` above the `xcopy`. RED at
   T1.3. If it passes, report it — the ordering is normative (§3.3.2).
6. Remove the catalog drain loop. RED at T1.9.
7. Delete the C2a running-program guard. RED at T1.6.
8. Drop `forthCapSetOrigin(originWas)` from the resume bracket. RED at a
   new close-paths subcase, or report that it is unobservable until
   L1-F* arms suspend/resume interactively — an honest "unobservable
   today" is acceptable here **provided you say so with evidence**.

## Out of scope (do NOT implement)

- ENTER, the REPL loop, the E8 ladder, or any interpret call — L1-2.
- Any change to `runFunction`, `determineItem`, or any dispatch — L1-3.
- Any TAM/fold behaviour — L1-F*.
- The history program — L-R7's packet.
- Any change to PEM capture behaviour. The landed F6/K suite must pass
  **unchanged**; report if any of it needed touching.

## Acceptance

- Gate green; landed F6/K suite unchanged; new tests' PASS lines quoted.
- `sizeof(forthCap_t)` unchanged (reported).
- The `closeAim()` call-site list reported with line numbers.
- All **eight** mutations shown RED (or explicitly reported unobservable
  with evidence) and reverted.
- Flash delta and arena high-water reported.
- **Sim check:** FORTH from `CM_NORMAL` opens a visible alpha line, and
  EXIT leaves it. Verify on the LCD via the `run-sim` skill,
  **copy-adapting `references/capture-driver.c` — do NOT hand-roll a
  driver** (standing rule, 2026-08-04). Attach the capture.
