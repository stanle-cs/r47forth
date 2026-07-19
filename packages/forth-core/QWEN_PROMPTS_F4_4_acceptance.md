# Stage F4-4 — Series C error-table and native/Forth parity acceptance

Origin: DESIGN §10.4 via `QWEN_PROMPTS_F4_core.md` §2.5.  Tests only (plus
one central fixture-builder extension).  This packet pins the complete
error table, the per-class native-step vs Forth-source parity, and the
deliberate divergences (flow steps execute natively but reject as Forth
names).  It closes stage F4.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log -1 --format=%s` is exactly:
   `forth-core: F4-3 — named, system-flag, and indirect parameters through the bounded core`.
2. `grep -n "test_param_named_indirect\|test_param_register_flag\|test_param_textual_numeric"
   packages/forth-core/test_dict_reloc.c` shows all three F4 tests
   registered.
3. `grep -n "tpStepParam" packages/forth-core/test_dict_reloc.c` → ZERO
   (this packet adds the builder).
4. `grep -n "forthItemIsFlowReject" packages/forth-core/forth_dict.h`
   matches (F4-1 landed).
5. Pre-gate green (`/tmp/forth-f4-4-pre.log`); arena baseline from the
   F4-3 commit.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`.  You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions.  If a quoted anchor, function, test, branch, literal, or identifier
does not match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`.  The tree must be clean before any edit.  Otherwise
   STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f4-4-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f4-4-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `items.c`,
   `config.c`, `lblGtoXeq.c`, `forth_inner.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f4-4-todo.md`,
   `git status --short`, and `git diff` are the durable task state.  After any
   compaction or uncertainty, STOP the current step and re-read those sources;
   never reconstruct packet text from memory.

**Two-attempt debugger handoff.** After the implementation first fails a
required command because of your changes, make at most two distinct repair
attempts, each followed by the relevant rerun.  This does not override any
immediate STOP rule and does not apply to an expected mutation RED.  If the
second repair is not green, STOP and report `[SOL DEBUGGER HANDOFF]` with the
command, bounded failure output, both repairs/results, current status/diff,
and remaining hypotheses.

**PROGRAM-FIXTURE AUTHORING RULE (mandatory)**

`test_dict_reloc.c` program fixtures are structural, not hand-addressed.
Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
the returned step handle when a test must execute or inspect that step, and
resolve it with `tpStepAddr`; abort the subcase if fixture construction,
`tpWrite`, or address lookup fails.

Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
`tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
authors must identify steps by role (for example `sSource` or `sXeq`) and
must not publish a calculated byte offset as a normative literal. If a
packet contains such an offset, stop with `[SOL DEBUGGER HANDOFF]` and report
the packet defect; do not repair its arithmetic locally.

Use a typed builder accessor such as `tpSrcPayload` for an internal field.
If the needed step or field helper does not exist, extend the central fixture
builder first; do not introduce local pointer arithmetic in the test.

Prefer named opcode/parameter constants in builder helpers. An exact byte
array may remain as the expected value of an encoding assertion. Raw bytes
inserted into the program fixture are allowed only for the encoding under
test or a deliberate malformation; they must enter through `tpRaw`, carry an
adjacent comment naming that purpose, and still use the returned handle and
builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
behavior fixture.

This rule is prospective. Do not widen the task by converting untouched
legacy fixtures.

---

## F4-4 — the sweep that certifies stage F4

### Change T — builder extension (test_dict_reloc.c, beside tpOp1)

```c
/* Native parameterized step: opcode bytes (1 or 2 per the func<128 rule)
 * followed by the given parameter bytes verbatim. */
static int tpStepParam(testProg_t *p, uint16_t func,
                       const uint8_t *param, uint16_t nParam)
{
  uint8_t s[2 + 40];
  uint16_t n = 0;
  if (param == NULL || nParam == 0 || nParam > 40) {
    return tpReject(p, "tpStepParam param");
  }
  if (func < 128) {
    s[n++] = (uint8_t)func;
  } else {
    s[n++] = (uint8_t)((func >> 8) | 0x80);
    s[n++] = (uint8_t)(func & 0xff);
  }
  memcpy(s + n, param, nParam);
  return tpAppend(p, s, (uint16_t)(n + nParam), TP_STEP_RAW);
}
```

### New test `test_param_series_c_acceptance` (register after the newest test)

State discipline for EVERY parity pair (F2-5 rule 6): a local
`seedSweepState()` helper pushing T=11, Z=22, Y=33, X=44 via
`forthPushInt32` in that order, then `lastErrorCode = ERROR_NONE;
programRunStop = PGM_STOPPED; dynamicMenuItem = -1;` — called immediately
before EACH half, with a `read_reg_int32` seed-assert after each call.
Native halves drive `executeOneStep` EXACTLY ONCE on a `tpStepParam`
step handle with `programRunStop = PGM_RUNNING` set after seeding
(F15-1/F2-1 drive discipline); Forth halves interpret the equivalent
source.  Record X (via `read_reg_int32`) and `lastErrorCode` after each
half; a pair passes iff both match.

1. **REGISTER parity, numbered + letter.**  Pair A: native
   `tpStepParam(ITM_STO, {0x05}, 1)` vs `"STO 05"`; then a shared
   `"RCL 05"` proves the store landed identically (run between the
   halves with its own seeding; compare recalled X).  Pair B: the same
   shape with `{104}` / `"STO A"`.
   `[1] PASS: register parameter parity (05, A)`
2. **NUMBER_8 boundary parity with an independent oracle.**  Compute
   `sdlMax = indexOfItems[ITM_SDL].tamMinMax & TAM_MAX_MASK`; require
   `paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, sdlMax)` true FIRST
   (the F2-5 anti-self-comparison oracle).  Pair: native
   `tpStepParam(ITM_SDL, {sdlMax}, 1)` vs sprintf'd `"SDL <max>"`;
   both halves must agree; then the over-max Forth spelling
   `"SDL <max+1>"` must raise `ERROR_OUT_OF_RANGE` while the NATIVE
   step with `{sdlMax+1}`… cannot be built by TAM — build it via
   `tpStepParam` anyway as the corrupt-step case: drive it and record
   the traced SILENCE (no error, X unchanged); the Forth PARSE reject
   vs the native EXECUTE silence is a DOCUMENTED divergence (parse-time
   vs run-time), print both outcomes in the PASS line.
   `[2] PASS: NUMBER_8 boundary agrees; over-range diverges as designed (parse reject vs native silence)`
3. **Named-variable parity.**  Native `tpStepParam(ITM_STO,
   {253, 2, 'V','S'}, 4)` vs `"STO 'VS'"` (both create on first use —
   run native first with X=44, assert via `"RCL 'VS'"` → 44; reseed,
   run the Forth half with X=44 after storing 20… keep it simple:
   delete-and-recreate is unavailable, so pin EQUIVALENCE by storing
   DIFFERENT sentinels and recalling between: native stores 44,
   `RCL 'VS'` → 44; Forth `"STO 'VS'"` with X=99 seeded via
   `forthPushInt32(99)` then `RCL 'VS'` → 99.  Both paths touched the
   SAME variable through the same core.)
   `[3] PASS: named variable parameter reaches one variable from both engines`
4. **Indirect parity.**  Register 05 := 7 (via `"STO 05"` prelude each
   half).  Native `tpStepParam(ITM_STO, {254, 5}, 2)` with X=99 vs
   Forth arrow spelling with X=99; after each, `"RCL 07"` must yield 99.
   `[4] PASS: indirect register parameter parity`
5. **Flow divergence table.**  For each of `RTN`, `STOP`, `END` (as
   ITM_END), `CASE` (param {0x05}), `GTO` (param {253,1,'Q'}): the
   NATIVE step, driven once, must NOT raise
   `ERROR_OPERATION_UNDEFINED` (record what it does — RTN/END return
   codes and GTO's label miss are native semantics, not this packet's
   concern; only the ABSENCE of the Forth reject code is asserted); the
   Forth NAME must raise exactly `ERROR_OPERATION_UNDEFINED` (clear
   between).  This pins §10.4's "XEQ is the sole control-flow bridge"
   from both sides.
   `[5] PASS: flow steps stay native-only; names reject with OPERATION UNDEFINED`
6. **Error-table sweep.**  One probe per row of the F4 core table, each
   asserting the EXACT code and clearing it: `"RTN"` →
   OPERATION_UNDEFINED; `"SDL"` → INVALID_NAME; `"SDL 1X"` →
   INVALID_NAME; `"SDL 100"` → OUT_OF_RANGE; `"SF .32"` → OUT_OF_RANGE;
   `"SDL 'X'"` → INVALID_NAME; N16-item + arrow (F4-3's discovery
   spelling) → INVALID_NAME; `"RCL 'NOVAR9'"` → UNDEF_SOURCE_VAR;
   `"OPENM 'NOMENU9'"` → UNDEF_MENU; compile-state atomicity re-pin:
   `": EA SDL 100 ;"` → OUT_OF_RANGE and EA unfindable.
   `[6] PASS: the Series C error table holds row by row`
7. **Extra-token and stage-wide state hygiene.**  `"STO 05 7"` → no
   error, `x_is_longint(7)`.  Then `forthTestGetRsp() == 0` and
   `forthCurrentScopeGet() == FORTH_OWNER_INTERACTIVE` (nothing leaked
   across the sweep).
   `[7] PASS: one-token consumption and engine state hygiene hold`

Cleanup per the suite conventions (regions, program memory, error
state); named variables VS handled as in F4-3's cleanup note.

### Existing tests

Untouched and green.

### Non-goals / STOP boundaries

- Tests only (plus tpStepParam).  Any production diff outside
  test_dict_reloc.c is a packet violation.
- No flag-observation helpers, no local-register allocation, no menu
  positive paths.

### Gate and required mutations

Full gate green (seven PASS lines).  Mutations, each separately,
verbatim anchors, manual restore, `/tmp/forth-f4-4-mut1..4.log` — these
mutate PRODUCTION code to prove the sweep's teeth, then restore:

1. In `forthItemIsFlowReject`, drop `ITM_CASE`.  Subcase 5's CASE row
   MUST go RED (the name parses as an eligible REGISTER item).
2. In `paramCoreValidateDirect`'s REGISTER arm, drop the
   `regInRange` conjunct (accept any KS ≤ 224).  Subcase 2 stays green
   (NUMBER_8) — the discriminator is subcase 1: it must STAY green too
   (05/A are in range)… therefore this mutation's RED lives in the
   corrupt-step silence pin: ADD to subcase 2 a REGISTER-class
   corrupt-value drive — `tpStepParam(ITM_STO, {230}, 1)` driven once
   must stay silent AND store NOTHING: precede with `"RCL 05"`
   expectation unchanged after the drive.  Under the mutation the
   dispatch fires with a garbage conversion; the follow-up recall or
   error state MUST differ → RED.  If it does not, STOP and report
   (escape valve).
3. In the F4-3 runtime 253 decode, skip the pad-byte check.  Subcase 3
   stays green (even-length name) — so use name `'VS'`… even.  ADD to
   subcase 3 an odd-name compiled probe: `": EO RCL 'ABC' ;"` run once
   (UNDEF_SOURCE_VAR expected, clear) after hand-poking its pad byte to
   7 via the dictionary image (encoding-assertion arithmetic on
   fdict.latest is permitted): under the CORRECT decoder the poked body
   raises INVALID_CORRUPTED_DATA; under the mutation it raises
   UNDEF_SOURCE_VAR instead → the probe's exact-code assert goes RED.
4. In `seedSweepState`, drop the `dynamicMenuItem = -1` line.  The
   sweep's own seed-assert CONFIG check MUST go RED (state-isolation
   teeth — the F2-5 lesson embodied).

Residue-free diff; final gate; report the seven PASS lines, mutation
symptoms, banners, exit 0, the two-region arena line, `git diff
--check`, mirror equality.  RULE-1: tests only — report that no
production flash changed, and quote the stage-total `make dmcp5r47`
delta if the owner runs it here (stage-closing commit).

### Commit

```text
forth-core: F4-4 — Series C error table and native parity are pinned
```

---

## AMENDMENT F4-4A (2026-07-19) — corrections carried from the F4-2/F4-3 debug

Authored before F4-2 and F4-3 were executed; five items above are now wrong
or unsound. Where they conflict, THIS section wins.

### A. Binding fixture rules (all four cost a red gate in F4-2/F4-3)

1. **`x_set_string` destroys a seeded stack.** It overwrites REGISTER_X with
   the source string and `fnForthOuter` drops it, shifting everything one
   level. Any half of a parity pair that needs `seedSweepState()`'s stack
   MUST run its source through `forthOuterInterpret(...)`, never
   `x_set_string` + `fnForthOuter`. (Compile-only fixtures, which need no
   seeded stack, may keep the x_set_string form.)
2. **`seedSweepState()` push order is literal.** To end at X=44, Y=33, Z=22,
   T=11, push `11, 22, 33, 44` in THAT sequence — each push lifts.
3. **`forthFindColon` returns a REF INDEX, not a byte offset.** Every byte
   image walks from `fdict.latest + TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK`.
   (Using the ref as an offset silently works for the first word defined
   after a clear, where both are 0, and corrupts every later image.)
4. **Every subcase opens with `lastErrorCode = ERROR_NONE;`** — a neighbour
   test may deliberately end on an error (F4-1's subcase 7 leaves
   ERROR_OPERATION_UNDEFINED), and an unset read reports a phantom failure.
5. **Named variables must be unwound.** Subcase 3 creates `VS`; the suite's
   end-of-run `numberOfAllocatedMemoryRegions` gate reddens on the leak.
   Snapshot `numberOfNamedVariables` at test start and, at cleanup, free
   each new variable's data (`freeRegisterData(FIRST_NAMED_VARIABLE + i)`)
   plus the header table back to the snapshot (`freeC47Blocks` when the
   snapshot was 0, else `reallocC47Blocks` down) — the F4-3 test has the
   exact code. Assigning `numberOfAllocatedMemoryRegions` to mask the growth
   is a packet violation.

### B. `regInRange` is not silent (amendment F4-2A)

`regInRange()` raises `ERROR_OUT_OF_RANGE` itself on a miss
(src/c47/store.c:17-72) and only then returns false, so "the REGISTER arm is
silent out of range" is false. Two consequences here:

- Subcase 2's traced-silence claim applies ONLY to the NUMBER_8 arm (whose
  validate has no `regInRange` conjunct) — keep it, it is correct there.
- A native REGISTER step with a byte ABOVE 224 (e.g. `{230}`) is still
  silent, but for a different reason: the native arm's range gate lives
  inside the `opParam <= 224` branch, so 230 reaches the sprintf-only
  default without ever calling `regInRange`. Say that, do not call it a
  range-gate silence.

### C. Mutation 2 is replaced

The old text could not name a deterministic RED. Landed replacement:

> **Mutation 2.** In `paramCoreValidateDirect`'s REGISTER arm, drop the
> `regInRange` conjunct (accept any KS ≤ LAST_SPARE_REGISTERS_IN_KS_CODE).
> **F4-2's `test_param_register_flag` subcase 4 MUST go RED**: an
> unallocated local (`STO .05`) stops raising ERROR_OUT_OF_RANGE and
> dispatches instead. This is a legacy-test RED caused by a deliberate
> mutation, which rule 6 permits — do not edit that test.

Drop the added REGISTER corrupt-value drive from subcase 2; it pinned a
silence that is not the one the mutation breaks.

### D. Mutation 3's anchor moved

The pad-byte check is no longer inside a per-class runtime decode. It lives
once, in `forthParamCellSpan` (forth_dict.c), which the runtime decoder and
all three walks call. The mutation is: **in `forthParamCellSpan`, drop the
`strict` pad-byte check** (`base[pos + 2 + len] != 0`). The odd-name probe
in subcase 3 is unchanged and still the RED oracle.

### E. Subcase 6's N16 row

The N16-plus-arrow row stays (the compiler rejects it — that is where the
exclusion is enforced). Do NOT add a validator pin for it: for a NUMBER_16
item the parameter cell is a full little-endian value, so `{254, 5}` IS the
legal value 1534 and no walk can reject it without rejecting legal programs
(this is why F4-3's subcase 8 dropped that pin).
