# Stage F3-7 — §2.3 XEQ acceptance pins and the final F3 sweep

Origin: R6_RESOLUTION_PLAN §2.3 via the F3 design pass
(`QWEN_PROMPTS_F3_core.md` D6). This is the last implementation packet in
F3. It adds the four independent acceptance pins that F3-6 deliberately
left for this packet: position-sensitive native/Forth local-XEQ mimicry,
terminal local misses, kind-byte round-trip/corruption, and the bare-name
GLOBAL_LABELS pin. It changes no product behavior. The only non-test code is
`FORTH_DEBUG_SELFTEST`-guarded observability for proving that a rejected local
request did not execute another program step.

Traced facts this packet relies on (verified 2026-07-18, `b5d794df4`, and
normatively landed by F3-1..F3-6 before this packet runs):
- local named-label selection in `findNamedLabelWithDuplicate` is “first
  occurrence after `currentLocalStepNumber`, else first occurrence in the
  current program”; its load-bearing comparison is
  `labelLocalStepNumber > currentLocalStepNumber`
  [packages/forth-core/programming/manage.c, `findNamedLabelWithDuplicate`];
- native `ITM_XEQ` and source `XEQ :NAME:` both delegate the kind byte to the
  same resolver/dispatch seam after F3-6; kind 249 misses are terminal;
- bare tokens still call `findNamedLabel(buf, GLOBAL_LABELS)` in step 5;
- the F3-3 structural builder has `tpEnd`, and F3-6 has `FTOK_XEQN 0x7F05`
  with inline `[kind][len][name][pad]`.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty. `git log -1 --format=%s` is exactly:
   `forth-core: F3-6 — XEQ source forms and FTOK_XEQN, kind-faithful end to end`.
2. `grep -n "static int test_xeqn" packages/forth-core/test_dict_reloc.c`
   matches exactly one definition, and its seven F3-6 PASS strings are present.
3. `grep -n "forthXeqnDispatch" packages/forth-core/forth_dict.h
   packages/forth-core/forth_inner.c` finds the public declaration and the
   single implementation. In that implementation the first lookup is exactly
   `findNamedLabel(name, kind)` and the kind-249 miss branch precedes every
   fallback.
4. `grep -n "findNamedLabel(buf, GLOBAL_LABELS)"
   packages/forth-core/forth_compile.c` matches exactly one line in the bare
   token step-5 arm.
5. `grep -n "labelLocalStepNumber > currentLocalStepNumber"
   packages/forth-core/programming/manage.c` matches exactly one line.
6. `grep -n "tpEnd" packages/forth-core/test_dict_reloc.c` finds the one
   builder helper added by F3-3. Greps for `tpLblLocal`, `tpXeqLocal`, and
   `tpSelectStep` in that same file find ZERO matches; a grep for
   `forthTestProgramStepCount` in `forth_dict.h` and `forth_compile.c` also
   finds ZERO matches.
7. Run the sanctioned pre-gate, capture `/tmp/forth-f3-7-pre.log`, require
   both success banners and exit 0, and record the extended two-region
   `FORTH ARENA` line as the baseline.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, literal, or identifier
does not match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise
   STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f3-7-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, fixture lint, final gate, parity check, and report.
   Keep it updated; mark each item in progress and completed as you work, and
   append `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately. Do
   not report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.` Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-7-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure. Never read the full log.
4. Edit only the flat implementation files named by this packet under
   `packages/forth-core/`; during the mutation phase, temporarily edit only
   the exact additional mutation hunks named by the packet. Never edit `src/`,
   generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`. Never read `items.c`,
   `config.c`, `lblGtoXeq.c`, `forth_inner.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f3-7-todo.md`,
   `git status --short`, and `git diff` are the durable task state. After any
   compaction or uncertainty, STOP the current step and re-read those sources;
   never reconstruct packet text from memory.

**Two-attempt debugger handoff.** After the implementation first fails a
required command because of your changes, make at most two distinct repair
attempts, each followed by the relevant rerun. This does not override any
immediate STOP rule and does not apply to an expected mutation RED. If the
second repair is not green, STOP and report `[SOL DEBUGGER HANDOFF]` with the
command, bounded failure output, both repairs/results, current status/diff,
and remaining hypotheses.

> **PROGRAM-FIXTURE AUTHORING RULE (mandatory)**
>
> `test_dict_reloc.c` program fixtures are structural, not hand-addressed.
> Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
> the returned step handle when a test must execute or inspect that step, and
> resolve it with `tpStepAddr`; abort the subcase if fixture construction,
> `tpWrite`, or address lookup fails.
>
> Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
> `tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
> authors must identify steps by role (for example `sSource` or `sXeq`) and
> must not publish a calculated byte offset as a normative literal. If a
> packet contains such an offset, stop with `[SOL DEBUGGER HANDOFF]` and report
> the packet defect; do not repair its arithmetic locally.
>
> Use a typed builder accessor such as `tpSrcPayload` for an internal field.
> If the needed step or field helper does not exist, extend the central fixture
> builder first; do not introduce local pointer arithmetic in the test.
>
> Prefer named opcode/parameter constants in builder helpers. An exact byte
> array may remain as the expected value of an encoding assertion. Raw bytes
> inserted into the program fixture are allowed only for the encoding under
> test or a deliberate malformation; they must enter through `tpRaw`, carry an
> adjacent comment naming that purpose, and still use the returned handle and
> builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
> behavior fixture.
>
> This rule is prospective. Do not widen the task by converting untouched
> legacy fixtures.

---

## F3-7 — independent pins at the native/Forth resolution boundary

### Authority carried by this packet (no open choices)

1. No product-resolution code changes. The F3-6 dispatch, token, validator,
   compiler, and B3 behavior remain byte-for-byte unchanged.
2. Add one `FORTH_DEBUG_SELFTEST`-only program-step counter. It counts entry
   to `forthProgramStep`, including an entry whose pre-scan later rejects.
   It does not exist in a production build and must not affect program state.
3. Extend the central `testProg_t` builder with typed helpers for a local
   label, a local native XEQ, RTN, and structural selection of a captured
   step. Tests never calculate a program byte offset or a local step number.
4. Add one test, `test_xeqn_acceptance`, with EXACTLY the four §2.3 subcases
   below. Register it immediately after `test_xeqn`.
5. The final stage sweep is the sanctioned full gate with every prior F3 test
   still registered. Do not invent a fifth integration fixture: F3-3 through
   F3-6 already independently cover scope, GLOBAL/IMMEDIATE/FORGET,
   control-flow, both dictionary regions, persistence, and XEQN execution.

### Files

Modify only these flat working-area files:

- `forth_dict.h`
- `forth_compile.c`
- `test_dict_reloc.c`

`programming/manage.c` and `forth_inner.c` are touched temporarily only by
the named mutation runs, one hunk at a time, then manually restored.

### Targeted reads

1. `forth_dict.h`: the existing `FORTH_DEBUG_SELFTEST` test-hook block only.
2. `forth_compile.c`: `forthProgramStep` and its adjacent test hooks only.
3. `test_dict_reloc.c`: the central `tp*` builder block; `x_is_longint`,
   `begin_word`/`end_word`, and `run_word`; the body of `test_xeqn`; and the
   registration lines around `test_xeqn`.
4. For mutations only: grep the exact anchors named under “Gate and required
   mutations” and read at most ±20 lines around each. Never read a prohibited
   large file in full.

### Change A — test-only program-step observability

In `forth_dict.h`, inside the existing `#ifdef FORTH_DEBUG_SELFTEST` hook
block, add:

```c
void forthTestProgramStepCountReset(void);
uint32_t forthTestProgramStepCountGet(void);
```

In `forth_compile.c`, beside the existing test hooks, add a static
`uint32_t forthTestProgramStepCount` plus the two functions. At the FIRST
line of `forthProgramStep`, before `forthRunGenCheckReset`, increment it under
the same guard. Reset sets zero; get returns the exact count. There is no
saturation, logging, or product-side branch.

### Change B — structural fixture helpers

Add the following central helpers beside their same-kind `tp*` neighbors.
Keep the existing 16-byte native label-name bound; these acceptance names are
`T` and `FOO`.

```c
static int tpLblLocal(testProg_t *p, const char *name)     /* LBL :name: */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpLblLocal name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpLblLocal name"); }
  s[0] = ITM_LBL; s[1] = LOCAL_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_LBL);
}

static int tpXeqLocal(testProg_t *p, const char *name)     /* XEQ :name: */
{
  uint8_t s[3 + 16];
  size_t n;
  if (name == NULL) { return tpReject(p, "tpXeqLocal name"); }
  n = strlen(name);
  if (n == 0 || n > 16) { return tpReject(p, "tpXeqLocal name"); }
  s[0] = ITM_XEQ; s[1] = LOCAL_LABEL_VARIABLE; s[2] = (uint8_t)n;
  memcpy(s + 3, name, n);
  return tpAppend(p, s, (uint16_t)(3 + n), TP_STEP_XEQ_NAME);
}

static int tpRtn(testProg_t *p)                           /* ITM_RTN = 0x04 */
{
  const uint8_t op = 0x04;
  return tpAppend(p, &op, 1, TP_STEP_OP1);
}
```

Add `tpSelectStep` immediately after `tpStepAddr`. It takes a CAPTURED handle,
sets `currentStep` to `tpStepAddr(p, idx)`, calls
`defineCurrentProgramFromCurrentStep()`, and walks from
`beginOfCurrentProgram` with `findNextStep` until it reaches that exact
pointer, counting structurally from local step 1. It fails with
`FIXTURE BUG: tpSelectStep could not locate captured step` if the address is
NULL, outside the selected program, or not reached exactly. On success it
sets `currentLocalStepNumber` to the counted value and returns true. The
helper contains no subtraction from `beginOfProgramMemory`, no caller-supplied
step number, and no program-layout literal.

Every drive below uses:

```c
if (!tpSelectStep(&tp, sRole)) { /* FAIL, cleanup, return */ }
executeOneStep(currentStep);     /* exactly once for this role */
```

The tests never call `fnExecute` directly. The XEQ operation under test may
call it internally; that is the behavior being accepted.

### Change C — new test `test_xeqn_acceptance`

Fresh transient/global dictionaries, fresh program memory, saved
`programRunStop`, `lastErrorCode = ERROR_NONE`, and `dynamicMenuItem = -1`
at entry. Each subcase owns and cleans its fixture. A fixture-build failure,
`tpWrite` failure, `tpSelectStep` failure, or missing dictionary helper result
is a test failure, never an assertion skip.

#### 1. Native/Forth mimicry selects the same NEXT local instance

Run two independently rebuilt variants of this exact structural program:

```text
LBL 'MIM'
LBL :T:
»FORTH 11 FORTH«
RTN
P = variant step
LBL :T:
»FORTH 22 FORTH«
RTN
END
```

Build every ordinary step through the typed helpers. The native variant uses
`sP = tpXeqLocal(&tp, "T")`; the Forth variant uses
`sP = tpSrc(&tp, "XEQ :T:")` (`printf '%s' "XEQ :T:" | wc -c` = 7).
Capture all handles needed for construction checks; no offsets. For EACH
variant: `tpWrite`; `forthRunGenBump`; set `programRunStop = PGM_RUNNING`;
select `sP` with `tpSelectStep`; execute that step exactly once; require no
error and `x_is_longint(22)`. The first `:T:` body yields 11, so 22 is an
independent proof that the occurrence after P was selected. Clean the first
fixture completely before building the second.

PASS only after both variants independently yield 22:

`[1] PASS: native and Forth local XEQ select the same next label instance`

#### 2. A local miss is terminal; no word or global label dispatches

Build this exact fixture:

```text
LBL 'MISS'
P0 = »FORTH 0 FORTH«
P  = »FORTH XEQ :FOO: FORTH«
END
LBL 'FOO'
»FORTH 99 FORTH«
END
```

There is deliberately NO local `:FOO:` in program MISS. Handles `sPrime` and
`sMiss` come from `tpSrc`; source length for `"XEQ :FOO:"` is 9. After
`tpWrite`, bump the run generation, structurally select/execute `sPrime` once
to consume the new-lifetime pre-scan, then define the same-lifetime global
Forth word with exactly:

`forthOuterInterpret(": FOO 88 ; GLOBAL")`

(`printf '%s' ": FOO 88 ; GLOBAL" | wc -c` = 17), requiring success. Thus
both fallback classes are live: a global colon word FOO would yield 88 and a
global program label FOO would execute a program step yielding 99.

Seed X with 55, reset `forthTestProgramStepCount`, structurally select
`sMiss`, and execute it exactly once. Require all of:

- `lastErrorCode == ERROR_LABEL_NOT_FOUND`;
- `x_is_longint(55)` (the global word did not execute);
- `forthTestProgramStepCountGet() == 1` (only P itself entered
  `forthProgramStep`; the global-label body did not).

Clear the expected error only after all three assertions.

`[2] PASS: local XEQ miss is terminal with no fallback dispatch`

#### 3. Kind round-trip and malformed inline data reject before dispatch

With empty program memory and both dictionaries clear, compile exactly:

```c
forthOuterInterpret(": KQ XEQ 'A' ;");
forthOuterInterpret(": KL XEQ :A: ;");
```

Each source is 14 bytes. Immediately after each definition, copy its
eight-byte body before compiling another word. Name length is 2 and the F3
header prefix is 6, so the body address for the latest transient entry is
`fdict.base + fdict.latest + TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK`. These are
dictionary-encoding assertions, not program-fixture offsets. The exact images
are:

```c
static const uint8_t quotedBody[8] =
  { 0x05, 0x7F, 0xFD, 0x01, 0x41, 0x00, 0x00, 0x00 };
static const uint8_t localBody[8] =
  { 0x05, 0x7F, 0xF9, 0x01, 0x41, 0x00, 0x00, 0x00 };
```

Require exact equality to both arrays and independently compare every byte
except index 2 for equality; index 2 must be the sole difference. Seed X with
31 and run KQ, then independently reseed X with 31 and run KL. With no A
target, each must reach `ERROR_LABEL_NOT_FOUND`, not corrupted-data, with X
still 31. Clear the expected error between runs. This proves both 253 and 249
decode as valid kinds without using one path as the oracle for the other.

Now build two transient malformed words with the existing `begin_word`,
`forthDictEmit`, `forthDictEmitBytes`, and `end_word` helpers:

- `XI`: emit `FTOK_XEQN`, raw inline bytes
  `{ 0xAA, 0x01, 0x41, 0x00 }`, then end normally. Seed X with 31; running XI
  must set `ERROR_INVALID_CORRUPTED_DATA` and leave X 31.
- `XT`: after `begin_word("XT", 2)`, capture `truncBody = fdict.here`; emit
  `FTOK_XEQN` and the single byte `STRING_LABEL_VARIABLE`, then call
  `end_word` to close/un-smudge the entry. Set
  `fdict.here = (uint16_t)(truncBody + 3)` — exactly the two-byte token plus
  one available operand byte — so the runtime operand-cell read is bounded
  short. Seed X with 31; running XT must set
  `ERROR_INVALID_CORRUPTED_DATA` and leave X 31.

Clear the expected error after each malformed run. Do not use a crash,
allocator symptom, or shared validator result as the oracle.

`[3] PASS: XEQN kind round-trip is exact and malformed data cannot dispatch`

#### 4. Bare names remain GLOBAL_LABELS-only

Build this exact fixture:

```text
LBL 'PIN'
P = »FORTH FOO FORTH«
LBL :FOO:
»FORTH 99 FORTH«
RTN
END
LBL 'FOO'
»FORTH 44 FORTH«
END
```

Use `tpLblLocal`, `tpRtn`, `tpEnd`, and `tpSrc`; capture `sBare` from
`tpSrc(&tp, "FOO")`. After `tpWrite`, bump the run generation, set running
state, structurally select `sBare`, and execute it exactly once. Require no
error and `x_is_longint(44)`: the global label FOO ran. The next local FOO
would yield 99, so the two outcomes independently discriminate the selector.

`[4] PASS: bare Forth name lookup ignores a colliding local label`

### Cleanup and registration

After every subcase and on every early exit: restore `programRunStop`, clear
both dictionary regions, restore program memory with the neighboring
tp-fixture cleanup idiom, and finish with `lastErrorCode = ERROR_NONE`.
Register `test_xeqn_acceptance` immediately after `test_xeqn` in
`forthDictionarySelfTest`, with its own DEBUG-running line.

### Fixture lint (mandatory before the first gate)

Inspect only added `test_dict_reloc.c` lines (`git diff -U0` filtered to `+`)
and require ZERO matches outside the central builder for:

```text
beginOfProgramMemory + <numeric literal>
tpStepAddr(..., <numeric literal>)
```

Also require every new behavior fixture to use only typed `tp*` helpers;
there is no `tpRaw` call in this test. The exact XEQN byte arrays are
dictionary-body encoding oracles and are permitted.

### Existing tests and the final F3 sweep

No existing assertion changes. Before the final gate, verify exactly one
definition and one registration each for:

- `test_scope_isolation`
- `test_global_marks`
- `test_control_flow`
- `test_xeqn`
- `test_xeqn_acceptance`

The full sanctioned gate is the stage sweep. Targeted log greps must show all
five tests' DEBUG-running lines, all four new PASS lines, every prior F3 PASS
line, both global-validator pins from F3-2, both final success banners, and no
`FAIL` line. Quote the extended arena line and confirm the existing combined
two-region ceiling remains green.

### Non-goals / STOP boundaries

- No production behavior change, new token, grammar, resolver, persistence
  key, dictionary layout, or lifecycle change.
- No edits to DESIGN.md, DESIGN-HISTORY.md, the R6 plan, upstream `src/`,
  generated package output, or `freeList.c`.
- No F4 parameter grammar.
- No full `fnExecute` test drive and no hand-addressed program fixture.
- If a prior F3 test must change to make this packet green, STOP: the packet
  or an earlier stage is defective.

### Gate and required mutations

Run the full gate green once before mutations. Then apply each mutation
SEPARATELY, run the sanctioned gate, require the named RED, and manually
reverse only that hunk before continuing:

1. In `findNamedLabelWithDuplicate`, change the sole comparison
   `labelLocalStepNumber > currentLocalStepNumber` to
   `labelLocalStepNumber > 0`. Subcase 1 MUST go RED with 11 instead of 22
   (search restarted at the program beginning). Green = STOP.
2. In `forthXeqnDispatch`, delete the kind-249 early-error branch so a local
   miss reaches the global fallback chain. Subcase 2 MUST go RED: FOO runs
   (X becomes 88 and the error/counter contract no longer matches). Green =
   STOP.
3. In `forthParseXeqForm`, change
   `*kind = (delim == ':') ? LOCAL_LABEL_VARIABLE : STRING_LABEL_VARIABLE;`
   to assign `STRING_LABEL_VARIABLE` unconditionally. Subcase 3's exact KL
   body comparison MUST go RED (0xFD at the sole discriminating byte instead
   of 0xF9). Green = STOP.
4. In the runtime `FTOK_XEQN` arm, delete only the invalid-kind half of the
   kind/length validation. Subcase 3's XI run MUST go RED with
   `ERROR_LABEL_NOT_FOUND` instead of `ERROR_INVALID_CORRUPTED_DATA`. Green =
   STOP.
5. In the bare-token step-5 arm, change its sole
   `findNamedLabel(buf, GLOBAL_LABELS)` call to `ALL_LABELS`. Subcase 4 MUST
   go RED with X == 99 instead of 44. Green = STOP.

Capture `/tmp/forth-f3-7-mut1.log` through
`/tmp/forth-f3-7-mut5.log`. After every manual restoration, require a
residue-free diff against the pre-mutation implementation diff. Run the
fixture lint again, then the final gate. Report the four PASS lines, each
expected mutation RED, both final banners, exit 0, the two-region arena line,
`git diff --check`, and generated mirror equality. This packet adds only
debug-guarded instrumentation plus tests, so no production flash delta is
required; report that no RULE-1 production code changed.

### Commit

```text
forth-core: F3-7 — pin XEQ resolution parity and close stage F3
```
