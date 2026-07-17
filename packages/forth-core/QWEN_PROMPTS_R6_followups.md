# R6 follow-ups — Qwen implementation prompts

Origin: the R6 pre-execution audit + addendum
(`FOR_THE_ARCHITECT_R6_preexecution_audit.md`) and the 2026-07-15 rulings
(`R6_RESOLUTION_PLAN.md`). Five tasks. R6-1 is the priority (a live defect
introduced by the upstream named-local-labels feature meeting a hook that
predates it). R6-2/R6-3 are small hygiene tasks. R6-4/R6-5 are
**report-only characterization probes** — they change no production code and
commit nothing; the architect rules on their evidence afterwards.

Order: R6-1 first (it commits). R6-2 and R6-3 may run in either order after
it (one committer at a time). R6-4/R6-5 last, any order.

**How to use:** paste the PREAMBLE, then one task block, into a fresh Qwen
session.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes`. If not, STOP.
2. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success = `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.`
   and exit 0. Never invoke meson or ninja directly — a hand-rolled build omits
   the self-test suite entirely and reports green having asserted nothing.
3. All edits go in `packages/forth-core/`. Never edit `src/`. The build reads
   only the GENERATED `patches/`+`files/`; build-test.sh refreshes first, so
   using the gate is sufficient. Never hand-edit `patches/`/`files/`.
4. Never touch `src/c47/core/freeList.c` or any copy. Never read DESIGN.md or
   DESIGN-HISTORY.md — your prompt carries every slice you need. Never read
   items.c (it is enormous) or test_dict_reloc.c in full; read only the ranges
   listed. Use `grep -a`.
5. Match surrounding code style. Keep upstream-derived files byte-identical
   except the marked change, so the generated patch stays small.
6. Do not commit unless told. Never `git add -A`. **Never run `git stash`,
   `git stash pop`, `git reset`, `git checkout -- <file>`, or `git restore`** —
   if you think you need to undo something, STOP and report. A red gate is
   safe; a mangled tree is not.
7. If the gate goes red on a test asserting the OLD behaviour your task was
   written to change, that test is part of your task — but STOP and report
   before touching it. Never make a test pass by weakening the change it
   caught. If a task changes a contract without listing the tests that encode
   it, the spec is wrong: say so and stop.
8. Report what you changed, the gate output, and anything that surprised you.

---

## R6-1 — Gate the TAM Forth fallback on `!tam.colon` (AUD-U1)

**File(s):** `packages/forth-core/ui/tam.c`;
`packages/forth-core/test_dict_reloc.c`

**Read:** In `ui/tam.c`, `grep -a -n "forth-core H-hook"` and read 25 lines
around the match; also `grep -a -n "tam.colon ? LOCAL_LABELS"` and read 5
lines around it (context only). In `test_dict_reloc.c`, `grep -a -n
"test_tam_dispatcher\|begin_word\|x_is_longint\|writeTestProgram"` and read
only those helpers/tests plus registration lines. `grep -a -n
"define ITM_COLON \|define ITM_A  \|define ITM_ENTER \|define ITM_XEQ "
packages/forth-core/items.h` for the item ids you need — never open items.c.

**The defect.** Upstream b8f79e486 added named LOCAL labels with a TAM colon
syntax: pressing `:` in a label TAM sets `tam.colon`, and the buffer then
resolves via `findNamedLabelWithDuplicate(buffer, dupNum, tam.colon ?
LOCAL_LABELS : GLOBAL_LABELS)`. Our Forth fallback hook in `_tamProcessInput`
predates that feature and is gated only on `!tam.indirect` — so `XEQ :FOO`
(an explicitly LOCAL request) that misses local labels falls through to
`forthFindColon` and either dispatches the Forth word FOO interactively or,
in PEM, records a **global**-name XEQ step for a local request. The program
path already does this right (`_executeOp`'s fallback is gated
`opParam == GLOBAL_LABELS`); the interactive TAM path must match: **a local
request resolves local labels or fails — never Forth vocabulary.**

**The change.** In the hook block

```c
            /* forth-core H-hook: Forth fallback after label miss (DESIGN.md §4.2) */
            {
              uint16_t widx;
              if (forthFindColon(buffer, &widx)) {
```

change the condition to

```c
              if (!tam.colon && forthFindColon(buffer, &widx)) {
```

and extend the comment's first line to
`/* forth-core H-hook: Forth fallback after GLOBAL label miss — never for a
tam.colon (LOCAL) request (DESIGN.md §0.3/§4.2 label-kind pins) */`.
Change nothing else. In particular, do NOT touch upstream's item-name scan
above the hook (it has the same gap for `XEQ :SIN`; that is upstream's code
and is reported upstream separately — out of scope here).

**Test.** Add `test_tam_colon_never_falls_to_forth`, registered beside
`test_tam_dispatcher`. Drive the REAL public TAM chain — `tamEnterMode()` and
`tamProcessInput()` are public (package ui/tam.c:1141, :1401). Shape:

1. Save and later restore every global you touch (calcMode, lastErrorCode,
   aimBuffer[0], any tam state not cleared by the chain itself).
   `forthDictClear()`, then define a colon word FOO returning 42 (begin_word
   "FOO" / T_ILIT 42 / end_word). Push a sentinel: `forthPushInt32(31337)`.
   Do NOT create any program — no label FOO, no local label FOO.
2. Control leg (pins that the gate is not over-broad — this is exactly
   today's working behavior): `tamEnterMode(ITM_XEQ)`, feed the name via
   three letter items (`tamProcessInput(ITM_F)`, `ITM_O`, `ITM_O` — grep
   items.h for the exact letter item defines; ITM_A is 550 and letters are
   contiguous, but verify with grep, do not assume), then
   `tamProcessInput(ITM_ENTER)`. Require: no error, `x_is_longint(42)` (the
   Forth word dispatched via the hook).
3. Reset: `lastErrorCode = ERROR_NONE`, push the sentinel again
   (`forthPushInt32(31337)`).
4. Colon leg: `tamEnterMode(ITM_XEQ)`, then `tamProcessInput(ITM_COLON)`
   (sets tam.colon — the real local-request gesture), then the same three
   letters, then `tamProcessInput(ITM_ENTER)`. Require:
   `lastErrorCode == ERROR_FUNCTION_NOT_FOUND` (upstream's
   neither-label-nor-function surface), `x_is_longint(31337)` (nothing
   dispatched), and `fdict.count` unchanged.
5. Cleanup on every path: `forthDictClear()`, restore saved globals,
   `aimBuffer[0] = 0`.

**STOP conditions specific to this test:** if the letters do not appear in
`aimBuffer` after step 2's feeds (probe it), or `tamEnterMode` misbehaves
headless (hangs, opens no TAM), STOP and report exactly what you observed —
do NOT fall back to hand-priming `tam.mode`/`tam.colon`/`aimBuffer` and
calling internals directly; a test that primes the state its subject derives
proves nothing (this repo has been burned by that four times).

**Tests that encode the old contract.** none — the old behavior (colon
request dispatching a Forth word) was never pinned; the control leg pins the
surviving global-request behavior.

**Gate:** build-test green.
*Mutation:* remove the `!tam.colon &&` gate → the colon leg goes RED with
X=42 and no error (today's defective behavior). Restore, rerun green.

**Commit** (this task alone): stage exactly
`packages/forth-core/ui/tam.c`, `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/patches/010-ui__tam.c.patch`,
`packages/forth-core/files/test_dict_reloc.c`,
`packages/forth-core/.refresh-manifest.json`; never `git add -A`; message
`forth-core: AUD-U1 — a tam.colon (local) request never falls through to Forth`.

**Report:** the changed condition, both legs' PASS lines, the mutation RED
symptom, green banners + exit code, FORTH ARENA line, commit hash.

---

## R6-2 — Picker builder: owning program via `forthOwningProgramStart` (AUD-M3)

**File(s):** `packages/forth-core/softmenus.c`

**Read:** `grep -a -n "case MNU_FORTH"` in packages/forth-core/softmenus.c and
read from there through the `if (progStart)` line only (~25 lines). Do not
read the rest of the file.

**The defect.** The MNU_FORTH builder still finds the owning program with the
iteration-order idiom the R4-E5 ruling retired:

```c
        for (i = 0; i < numberOfPrograms; i++) {
          if (programList[i].instructionPointer <= currentStep) {
            progStart = programList[i].instructionPointer;
          }
        }
```

This returns the *last qualifying* entry, which equals the maximum only
because `programList` happens to be built address-ascending — an unstated
invariant the ruling forbids relying on. `forth_bridge.c`'s
`forthOwningProgramStart()` was already fixed to compute the maximum
explicitly (and bounds-checks the pointer); this is the second site.

**The change.** Replace exactly that loop with:

```c
        progStart = forthOwningProgramStart(currentStep);
```

(`forth_dict.h` is already included; the following `if (progStart)` guard
stays — the helper returns NULL for the same no-owner cases the loop left
`progStart` NULL for.) Delete nothing else; the loop variable `i` is used by
later loops in the same case and must remain declared.

**Tests that encode the old contract.** none. The full picker suite
(`test_picker_*`) must stay green unchanged — the helper is
behavior-identical on ordered lists, and its max-not-last property is already
pinned at the helper level by `test_owning_program_start_max_not_last`.

**Gate:** build-test green.
*Mutation:* none runnable — this is a delegation refactor; the order-
dependence it removes cannot be provoked without hand-shuffling
`programList`. Acceptance is structural instead: after the change,
`grep -a -n "programList" packages/forth-core/softmenus.c` must show NO match
inside the `case MNU_FORTH` block (paste the grep output as proof), plus the
green suite.

**Commit** (this task alone): stage exactly
`packages/forth-core/softmenus.c`,
`packages/forth-core/patches/010-softmenus.c.patch`,
`packages/forth-core/.refresh-manifest.json`; message
`forth-core: FWRD builder derives its owning program via forthOwningProgramStart (R4-E5)`.

**Report:** the replaced lines, the structural grep, green banners, FORTH
ARENA line, commit hash.

---

## R6-3 — Document + canary the nested-outer open-definition precondition (R4-E3)

**File(s):** `packages/forth-core/forth_compile.c`

**Read:** `grep -a -n "static void forthOuterRun"` and read that function's
first 20 lines only.

**The context (accepted ruling, no production fix).** A nested outer
interpretation that starts while a definition is open can, on a nested error
path, call `abortDefinition()` against the OUTER line's `openDef` — the
epilogue restores `openDef.open` but not the dictionary bytes the abort
already rolled back. R4-E3 ruled: natural paths cannot reach this (nothing
executes mid-compile in stage C), so spend no production bytes — document
the precondition and add a debug-only canary.

**The change.** Two insertions in `forthOuterRun`, immediately after the
depth-cap check:

1. A comment:
```c
  /* PRECONDITION (R4-E3, accepted): a nested outer interpretation must begin
   * with no open definition. Nested error paths call abortDefinition(),
   * which observes the OUTER invocation's openDef — the epilogue restores
   * openDef.open but not the dictionary bytes an inner abort rolled back.
   * Unreachable from natural stage-C paths (compile state executes nothing);
   * revisit only if an EVALUATE-like or immediate source word lands. */
```
2. A debug-only, non-fatal canary right below it:
```c
  #ifdef FORTH_DEBUG_SELFTEST
  if (forthOuterDepth > 0 && isDefinitionOpen()) {
    printf("FORTH CANARY: nested outer interpret entered with an open definition (R4-E3 precondition violated)\n");
  }
  #endif
```

No behavior change, no new test (the canary is a tripwire for FUTURE
regressions; today no test may trigger it — if the gate output shows the
canary line after your change, STOP and report, because that means the
precondition is already being violated somewhere).

**Gate:** build-test green AND the canary line absent from the gate output
(paste `grep -c "FORTH CANARY" <gate log>` = 0).

**Commit** (this task alone): stage exactly
`packages/forth-core/forth_compile.c`,
`packages/forth-core/files/forth_compile.c`,
`packages/forth-core/.refresh-manifest.json`; message
`forth-core: document + canary the nested-outer open-definition precondition (R4-E3)`.

**Report:** both insertions, the canary grep, green banners, FORTH ARENA
line, commit hash.

---

## R6-4 — CHARACTERIZATION PROBE (report-only): F8 reject-path cursor drift

**No production change. No commit. The probe is removed before you finish.**

**File(s):** `packages/forth-core/test_dict_reloc.c` (temporarily)

**Read:** `grep -a -n "test_fcall_redirect_rejects_stale"` and read that test
plus 20 lines; it already drives the FCALL reject path
(`ERROR_NON_PROGRAMMABLE_COMMAND`).

**The question (PEM_AUDIT F8, deferred 2026-07-11, disposition pending
evidence).** When `insertStepInProgram`'s FCALL arm rejects (returns after
`displayCalcErrorMessage` without inserting) and the call came through
`addStepInProgram` with the cursor at end-of-program, the pre-move is skipped
but the post-move still runs — the claim is the cursor drifts one step back
although nothing was inserted.

**The probe.** Copy `test_fcall_redirect_rejects_stale`'s fixture into a
temporary `/* AUDIT-PROBE R6-4 */` test that (a) parks the cursor at
end-of-program (the fixture's cleanup notes show how it positions
`currentStep`/`currentLocalStepNumber`), (b) snapshots
`currentStep`/`currentLocalStepNumber`/`pemCursorIsZerothStep`, (c) drives
the rejection through `addStepInProgram(ITM_FCALL)` (not
`insertStepInProgram` directly — the drift lives in the caller), (d) prints
before/after values with a distinctive `AUDIT-PROBE R6-4:` prefix, asserting
nothing. Run the gate once, capture the printed lines, then REMOVE the probe
completely (re-edit, no git commands) and rerun the gate green.

**Report:** the captured before/after values (drift or no drift), both gate
runs' banners, and confirmation `grep -a -n "AUDIT-PROBE R6-4"` matches
nothing anywhere in the working tree. The architect rules fix-or-close from
your evidence — do not propose or apply a fix.

---

## R6-5 — CHARACTERIZATION PROBE (report-only): F9.1 phantom marker on power-off

**No production change. No commit. The probe is removed before you finish.**

**File(s):** `packages/forth-core/test_dict_reloc.c` (temporarily)

**Read:** `grep -a -n "test_save_restore_roundtrip"` and read that test (the
save/restore idiom); `grep -a -n "test_forth_toggle_from_catalog_leaves_alpha_menu"`
and read it (the real capture-open idiom via `runFunction`); `grep -a -n
"test_decode_marker_directions"` and read its marker-walk assertions.

**The question (PEM_AUDIT F9.1, deferred, disposition pending evidence).** An
empty open placeholder (len==0, byte-identical to a marker) persists in
program memory if state is saved mid-capture — E3's empty-commit deletion
never runs — so on restore, every later marker's parity is flipped
(display-only; the runner treats it as a marker no-op).

**The probe.** A temporary `/* AUDIT-PROBE R6-5 */` test: build a program
with an opening marker + one source line + closing marker (the
decode-marker-directions fixture shape); open a NEW capture at the end via
the real chain (toggle arm through `runFunction(ITM_FORTH)` with the A8
catalog discipline) so the len==0 placeholder exists; save state; restore
state (the roundtrip test's idiom); then walk the program counting
`ITM_FORTH` len==0 steps and decode the ORIGINAL two markers' directions.
Print counts and directions with an `AUDIT-PROBE R6-5:` prefix, assert
nothing. Capture output, REMOVE the probe completely, rerun green.

**Report:** marker count and rendered directions before/after restore
(phantom present? parity flipped?), both gate runs' banners, and the empty
`AUDIT-PROBE R6-5` grep. The architect rules document-vs-clear-on-restore
from your evidence — do not propose or apply a fix.
