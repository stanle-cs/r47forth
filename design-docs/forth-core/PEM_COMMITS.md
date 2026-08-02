# §9 PEM-native Forth entry — per-commit implementation prompts (Qwen)

Generated 2026-07-10 against the working tree (DESIGN.md §9 P-1..P-10 present,
uncommitted, together with the ui/tam.c + programming/lblGtoXeq.c P-3 changes).
Every line number below was verified against that tree on 2026-07-10.

**How Stan runs this:** one Qwen session per commit row. Paste the COMMON
PREAMBLE, then the commit's prompt. Qwen implements, tests, runs the mutation
check, STOPS, and reports. Stan reviews, runs the gate, commits, pushes, opens
the next session. Line numbers in later prompts may drift as earlier commits
land — each prompt names its anchors *textually* as well, and instructs Qwen to
re-locate by anchor text before editing.

---

## COMMON PREAMBLE (prepend to every Qwen session)

```
You are implementing ONE commit of a planned series in the C47-firmware Forth
package repo (/home/stan/c43). Rules, non-negotiable:

1. SCOPE. Implement exactly what this prompt specifies. Nothing else. If the
   spec and the code disagree, STOP and report the conflict — do not improvise.
2. UPSTREAM IS READ-ONLY. Never edit anything under src/c47/. All interception
   is via the package: packages/forth-core/ custom sources, or whole-file
   overrides registered in packages/forth-core/meson.build
   (pkg_override_sources / pkg_override_headers, paths relative to src/c47/).
   A new override starts as a byte-identical copy of the upstream file; keep
   every line identical except the marked insertions.
3. AUTHORITY. design-docs/forth-core/DESIGN.md is the single authority. Read the
   § slice named in the prompt BEFORE coding. Do not relitigate DECIDED items.
   Spec authority: DESIGN.md is written only by the design/review side.
   Implementation models propose changes via [PROPOSED] blocks in
   PROPOSED_SPEC_CHANGES.md; promotion into DESIGN.md is a human/design-side
   action.
4. LOCKED DECISIONS (apply everywhere):
   - Names-only invariant: no Forth dictionary index (widx) ever persists into
     a program step. Program↔Forth crossings are name strings resolved at run.
   - Entry-only toggle: keypad Forth/RPN state is DERIVED from program bytes
     at the cursor (DESIGN.md §9.4). If your implementation seems to need a
     persisted mode flag (system flag, tam field, static), STOP and report
     [DECISION NEEDED]. Do not add the flag.
   - FCALL reject-and-redirect (§9.10 item 2, resolved): a PEM gesture that
     would record ITM_FCALL+widx is rewritten to the §9.2 stored form
     (ITM_FORTH + STRING_LABEL_VARIABLE + [len][name]). Record the NAME by
     whatever route — reverse-lookup widx→name if a widx is in hand, else the
     typed token — NEVER write the widx.
   - Architecture-2 pre-scan and inline single-line entry are NOT built now.
     If a change would be easier "with a pre-scan", note it as future work.
5. TESTS. Every added test is a `static int test_xxx(void)` in
   packages/forth-core/test_dict_reloc.c, registered at the bottom of
   forthDictSelfTest() as `fail |= test_xxx();` with `forthDictClear()` after
   (see the existing pattern at test_dict_reloc.c:1940-1975). Each test's
   comment names the escaping mutation: the specific bug that must make it
   fail. MUTATION CHECK: after the test passes, temporarily re-introduce that
   bug, rebuild, confirm the test FAILS, revert the bug, confirm it passes
   again. Report the mutation run's output in your final report.
6. GATE (build/verify before reporting):
   - Build both sims: `make sim && make simr47` (build.sim/, shared tree).
   - Run the Forth self-test headless on BOTH binaries:
     `./build.sim/src/c47-gtk/c47 --headless` and
     `./build.sim/src/c47-gtk/r47 --headless`
     (paths per COMMIT 1's discovery report; FORTH_DEBUG_SELFTEST is on by
     default per packages/forth-core/meson.build). Expect
     "FORTH SELF-TEST: ALL PASSED".
   - Regression: run the suite COMMIT 1 established (`make test` or
     incremental `ninja test` in build.sim — per COMMIT 1's report):
     Since ninja test covers Forth cases but crashes (SIGSEGV in test_outer_glyph_divide, pre-existing), and make test does not include Forth tests (cleans CUSTOM_PKG), the gate is:
     ninja -C build.sim (build) + ./build.sim/src/c47-gtk/c47 --headless + ./build.sim/src/c47-gtk/r47 --headless (two headless self-tests, expect "FORTH SELF-TEST: ALL PASSED" on both)
7. STOP. When done: do NOT `git commit`, do NOT push. Leave the working tree
   modified. Write a report: what changed (file:line), test results, mutation
   check transcript, gate output tail, and any [DECISION NEEDED]/[GAP] found.
```

---

## Commit table (dependency-verified order)

| id | spec § | files touched | override? / precedent | tests (escaping mutation) | judgment-flags |
|----|--------|---------------|------------------------|---------------------------|----------------|
| C1 | §4.2 P-3 (pending tree) | none (report + optional test) | none | verification only; discovery | R47 binary path + `make test` Forth-coverage discovery sets the gate for C2..C13 |
| C2 | §0.2 P-1, §2.1, §9.1 | packages/forth-core/items.c | YES items.c / REM row :3374 | PTP probe + findNextStep sizing (mutation: revert PTP_REM→PTP_NONE) | breaking encode change; no migration (§0.2 P-1) |
| C3 | §3.3.2 P-2, §9.3 | forth_compile.c, forth_dict.c, forth_dict.h (custom) | no | forthProgramStep define-and-use; gen-reset; forthDictNameByIndex (3 named mutations) | forthDictClear NOT forthDictInit (§6.2 P-4) |
| C4 | §9.2, §9.3 | packages/forth-core/programming/lblGtoXeq.c | YES lblGtoXeq.c / ITM_42STRING arm :839-844 | executeOneStep marker-noop / source-exec / halt (3 mutations) | TWO bump sites only: fnExecute(:161) + runProgram menu-start(:886-897); R/S(:294) and SST deliberately EXCLUDED |
| C5 | §9.4 helpers, §9.5 parity | forth_bridge.c, forth_dict.h, test infra (custom) | no | parity + derived-state over hand-built program (2 mutations) | test-program-in-RAM infra built here; fiddly |
| C6 | §9.4 E1/E2 + FCALL redirect | packages/forth-core/programming/manage.c (NEW), meson.build | YES manage.c / ITM_REM arm :1386-1399 | toggle byte-probe; FCALL→name redirect probe; in-region route (3 mutations) | RISKIEST commit; FCALL contract baked; STOP-on-mode-flag rule |
| C7 | §9.4 E3/E5 | packages/forth-core/programming/manage.c (slice 2) | YES manage.c (same file, 2nd slice) | ENTER-empty deletes placeholder (parity-flip mutation); EDIT extraction | manage.c split point: C6=insertStepInProgram, C7=pemAlpha/pemAlphaEdit/fnPem |
| C8 | §9.5 P-7 | packages/forth-core/programming/decode.c (NEW), meson.build | YES decode.c / decodeRem :828-843 | render byte-probe »FORTH/FORTH«/source (parity-invert mutation) | capture-transient exception included |
| C9 | §0.1, §9.6 (menu id) | packages/forth-core/items.c (2nd touch) + items.h (1 line) | YES items.c / MNU_PROG row upstream items.c:3196 | CAT_MENU row probe (revert mutation) | 1-line items.h companion (documented exception); dead MENUS-catalog entry until C10 |
| C10 | §9.6 registration | src-copies: softmenus.c (NEW) + defines.h (NEW, 1 line), meson.build | YES softmenus.c + 1-line defines.h / dynamic-area rows :1017-1041, :1211-1234 | slot-22 + static-menu-integrity probes (mutation: define bump w/o rows) | TWO-override exception, build-atomicity forced (softmenu[22]=MNU_TAMFLAG misclassification); defines.h re-diff-on-merge; softmenuId shift hazard to verify |
| C11 | §9.6 builder (picker) | packages/forth-core/softmenus.c (slice 2) | YES softmenus.c / MNU_PROG builder :1673-1704 | builder content probe incl. inclusive-cursor + omit->14 (2 mutations) | MUST follow C10 (defines.h + disjunction) or acceptance 3 silently fails |
| C12 | §9.6 pick action | packages/forth-core/keyboard.c | YES keyboard.c / MNU_MyMenu side-effect :43-48, dynmenuGetLabel :1154 | manual sim script (mutation named: wrong cursor offset) | press/release double-fire check; STOP-on-mode-flag rule |
| C13 | §9.6 presentation + §9.9 sweep | packages/forth-core/programming/manage.c (slice 3) | YES manage.c / showSoftmenu(-MNU_ALPHA) :822 | full §9.9 acceptance sweep script for Stan | must follow C10 (else fnOpenMenu ERROR_UNDEF_MENU on every capture entry, softmenus.c:1278) |

Dependency skeleton (why this order — citations in each prompt):
C2 → C4 (dispatch by PTP class, lblGtoXeq.c:815); C3 → C4 (link: forthProgramStep,
forthRunGenBump); C5 → C6, C8 (link: forthEntryStateAtCursor / forthMarkerTurnsOn);
C3 → C6 (link: forthDictNameByIndex); C6 → C7 (same file; E3/E5 presuppose E1/E2
capture paths); C9 → C10 (MNU_FORTH define referenced by softmenu rows);
C10 → C11 → C12 → C13 (array slot, then builder, then press path, then menu push —
and the task-mandated "picker after defines.h + disjunction" ordering).

---

## COMMIT 1 — Baseline: pending P-3 work, discovery, gate calibration

**Goal.** Establish the series baseline: verify the uncommitted working-tree
changes match DESIGN.md §4.2 P-3, and empirically determine the build/test gate
used by every later commit. You implement no feature code in this session.

**Read first.** DESIGN.md §4.2, the P-3 paragraph ("PEM recording of
`XEQ 'NAME'`", around DESIGN.md:1240-1260) and §9 preamble (DESIGN.md:1566-1594).

**Step 1 — verify the pending diff.** Run `git diff`. Expected content, verify
each against the spec:
- `packages/forth-core/ui/tam.c` around :958-971: the PEM branch of the Forth
  XEQ fallback calls `insertUserItemInProgram(tam.function, buffer)` (:964) —
  records the typed NAME, not a widx. Confirm the non-PEM branch still calls
  `reallyRunFunction(ITM_FCALL, widx)` (:967).
- `packages/forth-core/programming/lblGtoXeq.c` :376-378: after
  `reallyRunFunction(ITM_FCALL, resolvedParam)`, the `ITM_XEQP1` return-step
  adjustment gated on `programRunStop == PGM_RUNNING && lastErrorCode ==
  ERROR_NONE`.
- `design-docs/forth-core/DESIGN.md`: the §9 spec itself (do not edit it).
Also verify the define-later degrade path the P-3 contract relies on: in
`packages/forth-core/ui/tam.c` ~:1073-1074, when NO resolution succeeded in
PEM, `addStepInProgram(tamOperation())` records the step with the typed name
(`tam.alpha` string arm, upstream manage.c:1749-1754 shape). Report the exact
line you find it at.

**Step 2 — DISCOVER the R47 binary and gate (mandatory, report all findings).**
1. `make sim && make simr47` (targets exist: Makefile:94, :106-107).
2. `ls build.sim/src/*/` — locate both sim binaries. Expected from
   Makefile:60: `build.sim/src/c47-gtk/r47` (and `c47` beside it). Report the
   actual paths.
3. Run each binary with `--headless` and capture output. With
   FORTH_DEBUG_SELFTEST on (default, packages/forth-core/meson.build:8-10) the
   reset path runs forthDictSelfTest and exits (packages/forth-core/
   config.c:1945-1956). Confirm "FORTH SELF-TEST: ALL PASSED" on BOTH.
4. EMPIRICALLY DETERMINE whether `make test` runs the package's Forth cases:
   run `make test` (WARNING: Makefile:142 does `clean` first — full rebuild;
   also try the incremental `cd build.sim && ninja test`), save the log, and
   `grep -i "forth\|test_dict_reloc\|SELF-TEST"` over it.
5. Set the gate rule for commits 2..13 from evidence and STATE IT explicitly
   in your report:
   - If `make test` (or `ninja test`) covers the Forth cases → it is the sole
     gate.
   - If not → gate = `ninja test` (upstream regression) PLUS the two headless
     self-test runs (Forth cases).

**Step 3 — optional test.** If (and only if) it needs no new infrastructure,
add a PC test asserting `forthResolveXEQ` still returns FORTH_XEQ_COLON for a
defined word and that the tam precedence order (label → item → colon,
forth_dict.c:292-323) is unchanged — this may already be covered by
`test_xeq_precedence` (test_dict_reloc.c:1284); if so, say so and add nothing.
No mutation check required for a session that adds no test — state that
explicitly rather than inventing one.

**Report.** Pending-diff verification (per item), binary paths, gate decision
with log evidence, define-later path line number. STOP. Do not commit.

---

## COMMIT 2 — Representation: ITM_FORTH becomes PTP_REM

**Goal.** Flip the stored representation (P-1): `ITM_FORTH` program steps are
string-payload `PTP_REM` steps.

**Read first.** DESIGN.md §0.2 (the P-1 block, ~:103-137), §2.1 (P-1 table
note), §9.1 (:1596-1624).

**File.** `packages/forth-core/items.c` ONLY (existing override).

**Edit.** Row `/* 2842 */` at items.c:4707: change `PTP_NONE` → `PTP_REM`.
Nothing else — same row shape as `REM` itself (items.c:3374:
`fnNop, NOPARAM, "REM", "REM", ... CAT_FNCT | SLS_ENABLED | US_ENABLED |
EIM_DISABLED | PTP_REM | HG_ENABLED`). Do NOT touch row 4708 (`ITM_FCALL`),
do NOT add the MNU_FORTH row yet (that is COMMIT 9).

**Why this is safe alone (verify, don't assume):**
- No entry path creates ITM_FORTH steps yet: `insertStepInProgram`'s PTP
  switch treats PTP_REM as "nothing to do" (upstream manage.c:1705-1708).
- A hypothetical in-program ITM_FORTH step now falls into `executeOneStep`'s
  PTP_REM arm and is ignored (package lblGtoXeq.c:859-861 `else { // REM }`).
- Interactive dispatch is unchanged: catalog XEQ still calls fnForthOuter
  (PTP not consulted; DESIGN.md §3.1).
- BREAKING: any program recorded earlier with a bare 2-byte ITM_FORTH step is
  now misdecoded (§0.2 P-1). Accepted, no migration. Say so in the report.

**Tests (test_dict_reloc.c, registered per the :1940-1975 pattern).**
1. `test_forth_step_ptp_rem` — assert
   `(indexOfItems[ITM_FORTH].status & PTP_STATUS) == PTP_REM`.
   Escaping mutation: reverting this commit's one-line change.
2. `test_forth_step_sizing` — build two stack buffers and size them with the
   UNMODIFIED upstream walker `findKey2ndParam` (nextStep.c:273; PTP_REM path
   :297-300 → countLiteralBytes :236-238):
   - marker: `{0x8B,0x1A,0xFD,0x00}` → next == buf+4;
   - source: `{0x8B,0x1A,0xFD,0x05,'3',' ','S','Q',' '}` → next == buf+9.
   Escaping mutation: same revert (PTP_NONE sizes the step as 2 bytes and
   both assertions fail).
Run the mutation check (revert row → rebuild → tests fail → restore → pass).

**Gate** per COMMIT 1's rule. **Report and STOP.**

---

## COMMIT 3 — Engine: forthProgramStep, run-generation, widx→name

**Goal.** The program-step entry point (P-2), the §9.3 lifecycle machinery,
and the reverse name lookup the FCALL redirect (C6) will need. Custom sources
only — no overrides.

**Read first.** DESIGN.md §3.3.2 P-2 block (the `forthProgramStep` code,
~:830-870 region — grep for "Program-step entry point (P-2"), §9.3 in full
(:1676-1731), §6.2 P-4 (forthDictClear vs forthDictInit).

**Files.** `packages/forth-core/forth_compile.c`, `forth_dict.c`,
`forth_dict.h`.

**Edits.**
1. forth_compile.c — add exactly the P-2 function from DESIGN.md §3.3.2:
   `void forthProgramStep(const uint8_t *payload)`. It MUST live in this file:
   `forthSource` and `forthOuterActive` are static here (:23-24). Payload is
   `[len][bytes...]`; copy BEFORE interpreting (the payload lives in program
   memory which interpreted words can move); `len ≤ 255 < FORTH_SOURCE_MAX
   (256, :22)` so no truncation branch exists.
2. forth_compile.c — §9.3 machinery, verbatim from the spec:
   `static uint16_t forthRunGeneration, forthResetGeneration;`
   `void forthRunGenBump(void)`; `static void forthRunGenCheckReset(void)`
   called by forthProgramStep AFTER the forthOuterActive guard, BEFORE
   interpreting. The reset action is `forthDictClear()` — NOT forthDictInit;
   forthDictInit on a live arena leaks the region (forth_dict.c:39-58,
   DESIGN.md §6.2 P-4).
3. forth_dict.c — `bool forthDictNameByIndex(uint16_t idx, char *buf,
   int bufSize)`: same latest-chain walk as `bodyOffsetOfIndex`
   (forth_inner.c:119-141 — `fdict.count - 1 - n == idx`), copying
   `hdr->nameLen` bytes from `fdict.base + off + 4`, NUL-terminated, clamped
   to bufSize-1 — mirror `openDefinitionName` (forth_dict.c:279-288). Returns
   false for idx ≥ fdict.count, NULL base, or a FF_SMUDGE entry.
4. forth_dict.h — declare `forthProgramStep`, `forthRunGenBump`,
   `forthDictNameByIndex` (put them near the existing "Bridge functions (§6)"
   block).

**Tests.**
1. `test_program_step_define_and_use` — build payload buffers for
   `: SQ DUP * ;` then `3 SQ` (`{len, bytes...}`), call forthProgramStep on
   each; assert X == 9 and `getRegisterDataType(REGISTER_X) == dtLongInteger`.
   Escaping mutation: forthProgramStep skipping the forthOuterInterpret call
   (a no-op handler — §9.9 acceptance 1's mutation).
2. `test_program_step_gen_reset` — define SQ via forthProgramStep; call
   `forthRunGenBump()`; run payload `3 SQ` via forthProgramStep; assert
   `lastErrorCode == ERROR_FUNCTION_NOT_FOUND` (dictionary was cleared) and
   clear the error. Then define SQ again, do NOT bump, run `3 SQ`, assert it
   works (resume keeps the dictionary — §9.9 acceptance 9b's logic).
   Escaping mutation: deleting the forthRunGenCheckReset call.
3. `test_dict_name_by_index` — define two words, assert both names round-trip
   by index and that idx == fdict.count returns false.
   Escaping mutation: off-by-one in the `count - 1 - n` walk (returns the
   wrong word's name — exactly the bug that would make the C6 FCALL redirect
   record the WRONG name).
Run the mutation check for all three.

**Gate** per COMMIT 1's rule. **Report and STOP.**

---

## COMMIT 4 — Runner arm + the two generation bump sites

**Goal.** Wire §9.2 execution and §9.3 bump sites into the program runner.

**Read first.** DESIGN.md §9.2 in full (:1625-1675), §9.3 bump-site block
(:1676-1731).

**File.** `packages/forth-core/programming/lblGtoXeq.c` ONLY (existing
override).

**Prerequisites (verify before editing; this is why C2/C3 precede):**
- `executeOneStep` dispatches by `indexOfItems[op].status & PTP_STATUS`
  (:815); the arm below is unreachable until items.c:4707 says PTP_REM (C2).
- `forthProgramStep` / `forthRunGenBump` must link (C3).

**Edit 1 — the ITM_FORTH case.** In the `case PTP_REM:` arm (:838-863),
after the `ITM_42APPEND` else-if (:845-858) and before the final
`else { // REM }` (:859), insert exactly the §9.2 arm:

```c
else if(op == ITM_FORTH) {
  if(*step++ == STRING_LABEL_VARIABLE) {
    if(*step != 0) {              // len > 0: source step
      forthProgramStep(step);     // step -> [len][bytes...] (§9.2)
    }                             // len == 0: marker — run-time no-op
  }
  return 1;
}
```
Model: the `ITM_42STRING` case (:839-844). Pointer math note (verify):
at arm entry `step` points at the type byte (executeOneStep consumed the
2-byte opcode at :758-763); after `*step++` it points at the len byte —
exactly what forthProgramStep expects. Do NOT check lastErrorCode here:
runProgram already halts without advancing when a step errors (:925-947).

**Edit 2 — bump site A (interactive XEQ).** First statement of `fnExecute`
(:161): `if(programRunStop != PGM_RUNNING) { forthRunGenBump(); }` with a
one-line comment citing §9.3. Program-internal XEQ steps arrive with
PGM_RUNNING set and must not bump. `fnExecutePlusSkip` (:205) calls fnExecute
— covered, add nothing there.

**Edit 3 — bump site B (menu-key start).** In `runProgram` (:886), after
`bool_t nestedEngine = ...` (:887) and BEFORE `programRunStop = PGM_RUNNING;`
(:897): `if(!nestedEngine && !singleStep && menuLabel != INVALID_VARIABLE)
{ forthRunGenBump(); }`.

**FORBIDDEN (judgment-flag, from the spec):** do NOT bump in `fnRunProgram`
(:294, the R/S path) and do NOT bump for singleStep/SST. R/S resume and SST
must keep the dictionary (§9.3 "Deliberate non-bump sites", §9.9 acceptance
9b). If review of the code makes you think a third bump site is needed, STOP
and report [DECISION NEEDED].

**Tests.**
1. `test_exec_step_marker_noop` — snapshot X and fdict.count; run
   `executeOneStep` on `{0x8B,0x1A,0xFD,0x00}`; assert return 1, X unchanged,
   count unchanged, lastErrorCode clean (§9.9 acceptance 8a).
   Escaping mutation: the arm calling forthProgramStep for len==0 too (the
   marker would interpret an empty line and set FLAG_ASLIFT/N drop state).
2. `test_exec_step_source_runs` — executeOneStep on a hand-built
   `: SQ DUP * ;` step buffer, then on a `3 SQ` step buffer; assert X == 9,
   dtLongInteger (§9.9 acceptance 1 at executeOneStep granularity).
   Escaping mutation: dropping the forthProgramStep call (arm returns 1
   silently).
3. `test_exec_step_halts_on_error` — executeOneStep on `3 SQX` (undefined);
   assert `lastErrorCode == ERROR_FUNCTION_NOT_FOUND` (§9.9 acceptance 7b's
   PC-testable half; the halt-at-step behavior is runProgram's :926 check,
   already upstream). Escaping mutation: the arm clearing lastErrorCode
   before returning.
Note in the report: bump-site wiring (fnExecute/runProgram) is not directly
unit-testable headless (needs labels/programs in RAM); its behavioral test is
C3's test 2 plus the §9.9 acceptance-9 sim script (C13). Run mutation checks.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 5 — Derived-state helpers + test-program infrastructure

**Goal.** The three §9.4 read-only helpers every UI commit depends on, plus
the test harness that lets later commits byte-probe real program memory.

**Read first.** DESIGN.md §9.4 from the top through the
`forthMarkerTurnsOn` paragraph (:1732-1800), §9.5 parity notes (:1870-1903).

**Files.** `packages/forth-core/forth_bridge.c` (currently 13 lines — just
fnForthCall), `forth_dict.h` (declarations), `test_dict_reloc.c` (infra +
tests). No overrides.

**Edits (forth_bridge.c).**
1. `uint8_t forthStepPayloadLen(const uint8_t *step)` — returns step[3] iff
   step[0]==0x8B && step[1]==0x1A && step[2]==STRING_LABEL_VARIABLE, else 0
   with a false out-signal (design the signature so marker (len==0) and
   "not an ITM_FORTH step" are distinguishable — e.g.
   `bool forthStepPayload(const uint8_t *step, uint8_t *lenOut)`).
2. `bool forthMarkerTurnsOn(const uint8_t *markerStep)` — owning program
   start = largest `programList[i].instructionPointer <= markerStep` over
   `numberOfPrograms` (programList is rebuilt on every edit by
   scanLabelsAndPrograms, upstream manage.c:102, called at :730); walk
   `findNextStep` from there to markerStep counting ITM_FORTH steps with
   len==0 strictly before it; return (count % 2) == 0.
3. `bool forthEntryStateAtCursor(void)` — exactly the §9.4 function:
   pemCursorIsZerothStep → false; non-ITM_FORTH step → false; source step
   → true; marker → forthMarkerTurnsOn.
Use `checkOpCodeOfStep(step, ITM_FORTH)` for opcode matching (upstream
pattern, manage.c:533-535). Declare all three in forth_dict.h.

**Test infrastructure (test_dict_reloc.c).** Add
`static bool writeTestProgram(const uint8_t *bytes, uint16_t n)` /
`static void restoreTestProgram(void)`: copy `bytes` to
`beginOfProgramMemory` (append the `0xff 0xff` .END. sentinel — inspect how
the empty program looks after reset before writing this), fix
`firstFreeProgramByte`/`freeProgramBytes` consistently, call
`scanLabelsAndPrograms()`; restore = re-zero to the pristine empty program
and re-scan. INVESTIGATE the reset-time program memory layout first (where
beginOfProgramMemory points, what the minimal valid program region is) and
report what you found; if the bookkeeping is more entangled than described,
STOP and report rather than corrupting the arena.

**Tests.**
1. `test_marker_parity` — program: marker, source(`: SQ DUP * ;`), marker,
   marker; assert turnsOn == true/­–/false/true for the 1st/3rd/4th
   (§9.9 acceptance 4 logic). Escaping mutation: inverting the parity test
   (odd instead of even) — every direction flips.
2. `test_entry_state_derivation` — same program + an RPN step (e.g. a bare
   `ITM_SIN` opcode byte) appended inside the region; point `currentStep` at
   each step in turn and assert: RPN step → false, source step → true,
   opening marker → true, closing marker → false, zeroth-step flag → false
   (§9.9 acceptance 2 logic). Escaping mutation: replacing the derivation
   with a static bool toggled by callers (the persisted-flag bug) — the
   land-on-step cases regress.
Run mutation checks. Restore program memory after each test.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 6 — manage.c override, slice 1: toggle, in-region route, FCALL redirect

**Goal.** The §9.4 entry crux: E1 (toggle arm), E2 (in-region capture route),
and the LOCKED FCALL reject-and-redirect — all inside `insertStepInProgram`.

**Read first.** DESIGN.md §9.4 E1/E2 blocks and the invariant text
(:1732-1868); §9.10 item 2 (resolved decision, :2056+); the COMMON PREAMBLE's
FCALL contract.

**Files.** CREATE `packages/forth-core/programming/manage.c` as a
byte-identical copy of `src/c47/programming/manage.c` (1896 lines), then add
the three arms below. Add `'programming/manage.c'` to `pkg_override_sources`
in packages/forth-core/meson.build. Include `"forth_dict.h"` near the top
(follow the include style of the package's lblGtoXeq.c override).
THIS SESSION EDITS ONLY `insertStepInProgram` (:1366-1773). pemAlpha /
pemAlphaEdit / fnPem edits are COMMIT 7 — do not touch them.

**Edit 1 — E1 toggle arm.** Insert after the `ITM_REM` arm (:1386-1399),
modeled on it, exactly the §9.4 E1 code (marker bytes
`0x8B 0x1A 0xFD 0x00` via `_insertInProgram(...,4)`; `wasOn =
forthEntryStateAtCursor()` computed BEFORE inserting; opening → 
`tam.function = ITM_FORTH; pemAlpha(ITM_FORTH);` — ITM_FORTH is not an
addItemToBuffer item so pemAlpha feeds no character, same trick as the REM
arm's `pemAlpha(func)` :1396; closing → stay RPN, no capture).

**Edit 2 — E2 in-region route.** Immediately BEFORE the
`indexOfItems[func].func == addItemToBuffer` check (:1411), the §9.4 E2
guard: `!tam.mode && !getSystemFlag(FLAG_ALPHA) && aimBuffer[0] == 0 &&
indexOfItems[func].func == addItemToBuffer && forthEntryStateAtCursor()` →
`tam.function = ITM_FORTH; pemAlpha(func); pemCursorIsZerothStep = false;
return;`. Only printable items open a capture; everything else keeps its
normal PEM meaning (that is the §9.4 "flip" definition — do not route
navigation/function keys).

**Edit 3 — FCALL reject-and-redirect (LOCKED contract).** New early arm next
to E1 (before the PTP switch at :1493, which would otherwise record the
PTP_NUMBER_16 index at :1676-1702):

```c
else if(func == ITM_FCALL) {
  // §9.10 item 2 (resolved): never persist a dictionary index in a program.
  char fname[FORTH_NAME_MAX + 1];
  if(!tam.indirect && forthDictNameByIndex(tam.value, fname, sizeof(fname))) {
    // widx in hand -> redirect: record the §9.2 stored form with the NAME
    uint16_t nameLen = stringByteLength(fname);
    tmpString[0] = (ITM_FORTH >> 8) | 0x80;
    tmpString[1] =  ITM_FORTH       & 0xff;
    tmpString[2] = (char)STRING_LABEL_VARIABLE;
    tmpString[3] = (char)nameLen;
    xcopy(tmpString + 4, fname, nameLen);
    _insertInProgram((uint8_t *)tmpString, nameLen + 4);
  }
  else {
    // no resolvable name (invalid/stale widx, or indirect): REJECT
    displayCalcErrorMessage(ERROR_NON_PROGRAMMABLE_COMMAND, ERR_REGISTER_LINE, REGISTER_X);
  }
  return;
}
```
Contract notes to honor: the typed-token degrade path already exists and is
NOT in this function — `XEQ 'NAME'` for an uncompiled word records the name
via tam.c ~:1073-1074 → addStepInProgram (verified in COMMIT 1); your arm
only handles the numeric-FCALL gesture, where a name exists iff the widx
resolves. NEVER write tam.value into the step.

**Feasibility caveat (investigate, then act).** The tests below call
insertStepInProgram directly from the self-test. That requires sane PEM
globals (currentStep, currentProgramNumber, programList, aimBuffer, tam).
Investigate what fnPem/reset initializes; if a minimal setup
(writeTestProgram from C5 + setting currentStep/currentLocalStepNumber) is
insufficient and the call corrupts state, STOP, report exactly which global
blocked you, and downgrade the affected test to a documented manual-sim step
— do not fake a pass.

**Tests.**
1. `test_toggle_inserts_marker` — in-test PEM setup; call
   `insertStepInProgram(ITM_FORTH)` with state OFF at cursor; byte-probe the
   program for `0x8B 0x1A 0xFD 0x00`; call again from ON state (cursor on the
   marker) and assert a second marker and NO capture opened (FLAG_ALPHA
   clear). Escaping mutation: E1 unconditionally entering capture (ignoring
   wasOn) — the closing-toggle assertion fails.
2. `test_fcall_redirect_records_name` — define `SQ` in the dictionary
   (define_word helper, test_dict_reloc.c:44); set `tam.value` to its widx;
   `insertStepInProgram(ITM_FCALL)`; byte-probe: step is
   `0x8B 0x1A 0xFD 0x02 'S' 'Q'` and NO `0x8B 0x1B` (ITM_FCALL opcode)
   anywhere in program memory (§9.9 acceptance 10's byte probe). Escaping
   mutation: falling through to the PTP_NUMBER_16 arm (recording
   `0x8B 0x1B` + index — the exact names-only violation).
3. `test_fcall_redirect_rejects_stale` — tam.value = fdict.count (invalid);
   call; assert ERROR_NON_PROGRAMMABLE_COMMAND and step count unchanged.
   Escaping mutation: recording a step with an empty/garbage name instead of
   rejecting.
Run mutation checks (or report the manual-sim downgrade per the caveat).

**Gate** per COMMIT 1. **Report and STOP.** Flag explicitly in the report:
no mode flag was added (state derived via forthEntryStateAtCursor on every
decision).

---

## COMMIT 7 — manage.c slice 2: empty-commit rule, EDIT support, cursor hack

**Goal.** §9.4 E3 and E5 — the pemAlpha-side rules that keep markers and
source lines unambiguous and make existing Forth lines editable.

**Read first.** DESIGN.md §9.4 E3/E4/E5 (:1800-1868), §9.5 transient note
(:1870-1903).

**File.** `packages/forth-core/programming/manage.c` (the override created in
COMMIT 6 — second slice; you touch `pemAlpha`, `pemAlphaEdit`, `fnPem` only).

**Edit 1 — E3 empty-commit rule.** In pemAlpha's `ITM_ENTER` arm (:882-888)
and on the pemCloseAlphaInput-triggering paths (fnSst closes alpha input via
pemCloseAlphaInput — nextStep.c:495-514 shows the SST side; the close itself
is manage.c:968-979): when `tam.function == ITM_FORTH && aimBuffer[0] == 0`,
delete the placeholder step exactly as the empty-BACKSPACE arm does
(:861-868: deleteStepsFromTo + clear FLAG_ALPHA + calcModeNormalGui +
_closeAlphaMenus) instead of committing. Rationale (§9.1): an empty committed
line is byte-identical to a marker and flips every later marker's parity.

**Edit 2 — E5 EDIT extraction.** pemAlpha's `ITM_EDIT` arm (:775-807):
add an `ITM_FORTH` case mirroring the REM case (:795-803) with offset **8**
(decoded render is `"FORTH"`(5) + `" "`(1) + 2-byte STD_LEFT_SINGLE_QUOTE —
decode.c:832-835): `xcopy(aimBuffer, tmpString + 8, ll);
aimBuffer[ll - 2 - 8] = 0;` and `tam.function`/`item` handling as the REM
case does. Markers (len==0) are NOT editable: make EDIT on a marker a no-op.

**Edit 3 — E5 gate + cursor hack.** `pemAlphaEdit` (:982-998): the gate at
:994 becomes `(func == ITM_LITERAL || func == ITM_REM || func == ITM_FORTH)`.
`fnPem`'s cursor-offset hack (:566-575, the `strcmp(tmpString, "REM ")`
branch at :570): add a `"FORTH "` branch — slice 6 bytes for the compare
(the existing code slices 4 via `tmpString[4] = 0`; extend the mechanism,
don't break the REM/42 cases) and use `cursorInString = T_cursorPos + 6`
(pattern: offset == prefix byte length, as REM uses +4).

**Tests.** These paths are keystroke-driven; reuse COMMIT 6's PEM-setup
approach. If direct calls proved feasible there:
1. `test_forth_empty_enter_leaves_no_step` — open capture via
   insertStepInProgram(ITM_FORTH) (opening toggle), then `pemAlpha(ITM_ENTER)`
   with empty aimBuffer; assert program step count returned to exactly one
   marker (no phantom second marker) and FLAG_ALPHA clear (§9.9 acceptance 8b).
   Escaping mutation: dropping E3 — the empty placeholder commits and
   COMMIT 5's `test_marker_parity` invariant would flip downstream (state
   this linkage in the test comment).
2. `test_forth_edit_extracts_source` — write a `FORTH ': SQ DUP * ;'` step,
   point currentStep at it, call `pemAlpha(ITM_EDIT)`; assert aimBuffer ==
   `": SQ DUP * ;"`. Escaping mutation: using the REM offset 6 instead of 8
   (aimBuffer starts with two garbage bytes).
If COMMIT 6 downgraded direct calls to manual: do the same here, write the
manual sim script for both cases, and say so. Run whatever mutation checks
are automatable.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 8 — decode.c override: »FORTH / FORTH« rendering

**Goal.** §9.5 symmetric display, computed at render time.

**Read first.** DESIGN.md §9.5 in full (:1870-1903).

**Files.** CREATE `packages/forth-core/programming/decode.c` as a
byte-identical copy of `src/c47/programming/decode.c` (945 lines); register in
meson.build. Depends on C5 (forthMarkerTurnsOn) and C2 (PTP_REM routes
ITM_FORTH into decodeRem via _decodeOneStep :905-908).

**Edit.** In `decodeRem` (:828-843) add an ITM_FORTH branch. For
`op == ITM_FORTH` with payload len == 0 (peek `*literalAddress` == 0 after
the STRING_LABEL_VARIABLE check; note decodeRem receives `step` already past
the opcode — the marker-parity helper needs the OPCODE start, so pass
`literalAddress - 2`... verify the exact pointer the caller passes at :906
before writing this):
- If `(uint8_t*)opcodeStart == currentStep && getSystemFlag(FLAG_ALPHA) &&
  tam.function == ITM_FORTH` → render as an (empty) source line via the
  generic path (the capture-transient exception, §9.5).
- Else → `tmpString` = `STD_RIGHT_DOUBLE_ANGLE "FORTH"` when
  `forthMarkerTurnsOn(opcodeStart)`, else `"FORTH" STD_LEFT_DOUBLE_ANGLE`
  (glyphs verified: fonts.h:156 / :150).
For len > 0 keep the generic `FORTH '…'` render UNCHANGED (fall through to
the existing code). Include forth_dict.h.

**Tests.**
1. `test_decode_marker_directions` — write (C5 infra) marker/source/marker/
   marker; decodeOneStep each marker; assert tmpString bytes are
   `\x80\xbbFORTH`, `FORTH\x80\xab`, `\x80\xbbFORTH` respectively; decode the
   source step and assert it starts `FORTH ` and contains the source text
   (§9.9 acceptance 4). Escaping mutation: inverting the parity (call
   `!forthMarkerTurnsOn`) — all three direction assertions fail.
2. `test_decode_source_unchanged` — a `len > 0` step renders through the
   pre-existing quoting path byte-for-byte (compare against a reference built
   from the unmodified logic). Escaping mutation: the new branch swallowing
   `len > 0` steps too (rendering them as markers).
Run mutation checks.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 9 — Menu id: items.c slot 213 + items.h define

**Goal.** Claim the `MNU_FORTH` id (§0.1 third row) so the C10 registration
can reference it.

**Read first.** DESIGN.md §0.1 (the three-row table), §9.6 first bullet
(:1904-1952), §9.8.

**Files.** `packages/forth-core/items.c` (2nd touch of this override) and
`packages/forth-core/items.h` — the items.h change is a ONE-LINE companion
(`#define MNU_FORTH 213` next to ITM_FORTH/ITM_FCALL at :2951-2952).
[Documented exception to the one-override-per-commit rule: the .h line is a
single line, costs no context, and the .c row is meaningless without it.]

**Edit.** Replace the CAT_FREE spare at items.c:2003 (`/* 213 */`,
verified free) with a CAT_MENU row shaped exactly like upstream `MNU_PROG`
(src/c47/items.c:3196): `{ itemToBeCoded, NOPARAM, "FWRD", "FWRD",
(0 << TAM_MAX_BITS) | 0, CAT_MENU | SLS_UNCHANGED | US_UNCHANGED |
EIM_DISABLED | PTP_DISABLED | HG_ENABLED }`.

**Known transient (state in report, do nothing about it):** until COMMIT 10
registers the menu, "FWRD" appears in the MENUS catalog and pressing it
raises ERROR_UNDEF_MENU (fnOpenMenu, softmenus.c:1278). One-commit window,
accepted.

**Tests.** `test_mnu_forth_row` — assert
`(indexOfItems[MNU_FORTH].status & CAT_STATUS) == CAT_MENU` and
`compareString(indexOfItems[MNU_FORTH].itemCatalogName, "FWRD", CMP_BINARY)
== 0`. Escaping mutation: reverting the row (slot 213 back to CAT_FREE) —
which is also exactly what a botched upstream-merge of items.c would do.
Run the mutation check.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 10 — softmenus.c registration + defines.h (the atomic pair)

**Goal.** Register MNU_FORTH as dynamic softmenu #22 and grow the dynamic
area to 23 — the infrastructure the picker (C11) and the task-mandated
ordering depend on.

**Read first.** DESIGN.md §9.6 bullets 1 and "Refresh" (:1904-1952); §6 hook
rows P-H5/P-H6.

**Files.** CREATE `packages/forth-core/softmenus.c` (copy of
src/c47/softmenus.c, 4362 lines) and CREATE `packages/forth-core/defines.h`
(copy of src/c47/defines.h, 2507 lines — change ONE line). Register both in
meson.build (`pkg_override_sources` / `pkg_override_headers`).

[JUDGMENT FLAG — two override files in one commit, deliberately.] Build
atomicity forces it: `softmenu[22]` is `-MNU_TAMFLAG`, a STATIC menu
(softmenus.c:1041). Bumping NUMBER_OF_DYNAMIC_SOFTMENUS (defines.h:1429)
without inserting matching rows makes `fnOpenMenu`'s
`if(i < NUMBER_OF_DYNAMIC_SOFTMENUS)` (softmenus.c:1266) and the display
path's `if(m < NUMBER_OF_DYNAMIC_SOFTMENUS)` (:3035) misclassify TAMFLAG as
dynamic → it renders empty. Conversely the rows can't land first: 
`dynamicSoftmenu[NUMBER_OF_DYNAMIC_SOFTMENUS]` (:1211) would have 23
initializers in a 22-slot array. The defines.h slice is one line; the
context-budget intent of the one-override rule is honored.

**Edits.**
1. defines.h:1429 — `NUMBER_OF_DYNAMIC_SOFTMENUS` 22 → 23. THE ONLY change
   in the file. Add a `// forth-core P-H6` end-of-line comment. RE-DIFF NOTE
   for the report: this override must be re-diffed against upstream on every
   merge; it shadows a machine-wide header.
2. softmenus.c — append to the END of the dynamic area, keeping both arrays
   in the same order (upstream's own rule, comment :1021-1028):
   - `softmenu[]`: insert `/* 022 */ {.menuItem = -MNU_FORTH, .numItems = 0,
     .softkeyItem = NULL}` after the `/* 021 */ -MNU_MENU` row (:1040), i.e.
     BEFORE the `-MNU_TAMFLAG` row (:1041), which becomes index 23.
   - `dynamicSoftmenu[]` (:1211-1234): append
     `/* 22 */ {.menuItem = -MNU_FORTH, .numItems = 0, .menuContent = NULL}`.
3. `initVariableSoftmenu` (:1648, switch :1657): add
   `case MNU_FORTH: { dynamicSoftmenu[menu].menuContent = malloc(1);
   dynamicSoftmenu[menu].menuContent[0] = 0;
   dynamicSoftmenu[menu].numItems = 0; break; }` — a STUB; the real builder
   is COMMIT 11. (Check how the other cases handle the prior
   `free(menuContent)` at :1655 and mirror allocation discipline exactly.)
4. The rebuild-always disjunction (:3039):
   `if(softmenu[m].menuItem != cachedDynamicMenu || softmenu[m].menuItem ==
   -MNU_DYNAMIC)` gains `|| softmenu[m].menuItem == -MNU_FORTH`. Without it a
   word defined on the previous line never appears while the menu stays up —
   this is the line §9.9 acceptance 3's mutation targets.

**Verify and report (do not fix):** whether `softmenuStack` softmenuId
indices are persisted by saveRestoreBackup.c — if yes, saved states from
before this commit will point one menu off after the TAMFLAG..end shift
(index 22→23 etc.). Report what you find; the shift is inherent to the
upstream-sanctioned procedure (comment :1025-1028) and accepted. Also note
the upstream "menu numbers are fixed for the Wiki references" comment (:1041)
— documentation-only cost.

**Tests.**
1. `test_dynamic_menu_registration` — assert
   `dynamicSoftmenu[22].menuItem == -MNU_FORTH`, and a compile-time
   `_Static_assert(NUMBER_OF_DYNAMIC_SOFTMENUS == 23, ...)` beside the test.
2. `test_static_menu_integrity` — assert `softmenu[23].menuItem ==
   -MNU_TAMFLAG` (guards the exact off-by-one this commit risks).
   Escaping mutation (covers both): apply the defines.h bump WITHOUT the row
   insertions — test 2 fails (TAMFLAG found at 22, treated dynamic). Run the
   mutation check that way (temporarily stash the softmenus.c row edits).

**Gate** per COMMIT 1 — plus, on the sim, manually open a menu beyond the
dynamic area (e.g. the TAM flag menu) and confirm it still renders. **Report
and STOP.**

---

## COMMIT 11 — softmenus.c slice 2: the `: NAME` picker builder

**Goal.** §9.6 content: the entry-time text scan. ORDERING GUARANTEE: this
lands only now — after C10's defines.h bump and rebuild-always disjunction —
because without them the picker either doesn't build (array too small) or
silently shows stale content while displayed (acceptance 3's failure mode).

**Read first.** DESIGN.md §9.6 "Content build" and "No validation" bullets
(:1904-1952); the MNU_PROG builder (upstream softmenus.c:1673-1704) is the
model to copy.

**File.** `packages/forth-core/softmenus.c` (slice 2 — only the
`case MNU_FORTH:` body you stubbed in C10).

**Edit.** Replace the stub with the builder:
- Walk `findNextStep` from the owning program's start (programList lookup as
  in forthMarkerTurnsOn, C5) up to and INCLUDING `currentStep`.
- For each ITM_FORTH step with len > 0: tokenize the payload with the same
  glyph-wise discipline as the compiler's tokenizer (forth_compile.c:42-57 —
  advance only via stringNextGlyph, delimiter exactly 0x20); every token `:`
  followed by a name token yields that name (mid-line `; : NAME2` occurrences
  included).
- Collect into fixed 15-byte slots in tmpString, cap check: names LONGER
  than 14 bytes are OMITTED, not truncated (deliberate deviation from
  MNU_PROG's truncation :1680-1682 — a truncated pick would insert a WRONG
  name). Deduplicate (redefinitions collapse to one entry). qsort with
  `sortMenu` and pack, exactly as :1691-1698.
- No dictionary lookups, no validation: text scan only (§9.6).

**Tests.**
1. `test_picker_scan_basic` — program: marker, `: SQ DUP * ;`,
   `: CUBE DUP DUP * * ;`, marker; currentStep on the last marker; call
   `initVariableSoftmenu(22)`; assert menuContent contains "SQ" and "CUBE",
   numItems == 2, sorted. Escaping mutation: the walk stopping BEFORE
   currentStep (exclusive bound) — a word defined on the immediately
   preceding line is missing; this is §9.9 acceptance 3's essence.
2. `test_picker_omits_long_names` — a 15-byte word name in a source step is
   absent from menuContent; a 14-byte one is present. Escaping mutation:
   truncating instead of omitting — the 15-byte name appears cut to 14 and
   the "absent" assertion fails.
3. `test_picker_dedupes` — the same `: SQ` on two lines yields one entry.
   Escaping mutation: skipping the dedupe — numItems == 2 for one name.
Run mutation checks; restore program memory after each.

**Gate** per COMMIT 1. **Report and STOP.**

---

## COMMIT 12 — keyboard.c: picker press inserts the name

**Goal.** §9.6 pick action: pressing a MNU_FORTH softkey during Forth capture
inserts `NAME ` into the line buffer at the cursor.

**Read first.** DESIGN.md §9.6 "Pick action" bullet (:1904-1952).

**File.** `packages/forth-core/keyboard.c` (existing override, 4996 lines —
you need only two regions).

**Where.** The softkey→item mapping switch in `determineFunctionKeyItem_C47`
(:13, switch at :42). Precedents: `MNU_MyMenu` performs a side effect inside
its case (:43-48, setCurrentUserMenu), and `MNU_PROG` maps to sentinel items
(:68-77). Name retrieval: `dynmenuGetLabel(dynamicMenuItem)` (:1154, :1331).

**Edit (strategy — verify each step against the code before writing it).**
Add `case MNU_FORTH:` to the :42 switch:
```c
case MNU_FORTH: {
  dynamicMenuItem = firstItem + itemShift + fn;
  if(calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA)
     && tam.function == ITM_FORTH
     && dynamicMenuItem < dynamicSoftmenu[menuId].numItems) {
    // insert "<name> " into aimBuffer at T_cursorPos — same xcopy idiom as
    // pemAlpha's addItemToBuffer arm (manage.c:854-857), then re-commit the
    // step by calling pemAlpha with a non-addItemToBuffer item so its
    // re-insert block (manage.c:938-960) runs.
  }
  item = ITM_NOP;
  break;
}
```
MANDATORY checks before finalizing:
1. Grep the call sites of determineFunctionKeyItem_C47. If it runs on BOTH
   key press and release (double-fire), move the side effect to the
   release-path handler instead (precedent: the dynamicMenuItem-consuming
   blocks in executeFunction, :1329-1347) and leave the case mapping pure.
   Report which you did and why.
2. Respect the 255-byte/196-glyph aimBuffer cap (manage.c:854) — skip the
   insert if it would overflow.
3. Verify the re-commit trick: pemAlpha(<item with func != addItemToBuffer>)
   must fall through its special-item chain to the re-insert block. Pick the
   item (ITM_NOP or ITM_FORTH), verify its indexOfItems func, and report.
4. NO mode flag: the guard is derived state (FLAG_ALPHA + tam.function) that
   ordinary alpha capture already maintains. If you find yourself needing
   more persistent state, STOP — [DECISION NEEDED].

**Tests.** This path is keystroke-driven end-to-end; there is no honest
headless unit test without a key-injection harness. Provide instead:
- A MANUAL SIM SCRIPT for Stan (exact keystrokes: enter PEM, toggle FORTH,
  type `: SQ DUP * ;`, ENTER, press the FWRD softkey showing SQ, assert the
  buffer shows `SQ ` at the cursor and the listing's current step updates).
- The named escaping mutation, in the code comment: inserting at aimBuffer
  end instead of T_cursorPos (breaks mid-line picks), and omitting the
  trailing space (gluing tokens). Stan's script must exercise a mid-line pick
  so both would be caught.
State explicitly in the report that no automated test was added and why.

**Gate** per COMMIT 1 (regression only — the new path needs the manual
script). **Report and STOP.**

---

## COMMIT 13 — manage.c slice 3: menu presentation + full acceptance sweep

**Goal.** Push the picker menu when Forth capture opens, then drive the whole
§9.9 acceptance list.

**Read first.** DESIGN.md §9.6 "Presentation" bullet; §9.9 in full
(:1987-2056).

**File.** `packages/forth-core/programming/manage.c` (slice 3 — one edit).
ORDERING: this could not land before C10 — pushing an unregistered menu makes
every capture entry raise ERROR_UNDEF_MENU (fnOpenMenu, softmenus.c:1278).

**Edit.** In pemAlpha's capture-opening branch, where `showSoftmenu(-MNU_ALPHA)`
runs (:822 in the upstream numbering; find it in the override), add, gated on
`tam.function == ITM_FORTH`: `showSoftmenu(-MNU_FORTH);` AFTER the alpha menu
push (picker lands on top; EXIT pops back to alpha — §9.6).

**Tests / acceptance sweep.** Add no new unit tests unless a gap emerges.
Instead produce the ACCEPTANCE RUN SHEET for Stan mapping every §9.9 item to
its concrete check:
- 1, 6, 7, 8, 9a/9b, 10 → point at the automated tests from C3/C4/C6/C7 (name
  each test function) plus the sim script for the runProgram-level halt (7)
  and STOP/resume (9b).
- 2 (derived keypad, incl. power-off-mid-region), 3 (picker before any run),
  4 (marker directions on-screen), 5 (glyph operators typed on the alpha
  keypad), 8b (empty ENTER) → exact keystroke scripts on
  `build.sim/src/c47-gtk/r47`.
Also run the arena report required by §5.4/§9.9: print `getFreeRamMemory()`
before/after the test-1 program run and include the dictionary high-water
delta in the report (budget: ≤ 2 KB region ceiling).
Escaping-mutation duty for the one code edit: pushing -MNU_FORTH for EVERY
capture (REM/literal too) — Stan's script includes opening a plain REM line
and asserting the FWRD menu does NOT appear.

**Gate** per COMMIT 1 + the full run sheet executed once by you (Qwen) as far
as headless allows; everything keystroke-bound is Stan's half. **Report and
STOP.**
