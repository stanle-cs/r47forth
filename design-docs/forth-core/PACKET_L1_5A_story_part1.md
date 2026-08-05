# PACKET L1-5A — the stage story, part 1 (C1 steps 1–7)

**Stage L final packet, session A.** Parent packet:
`PACKET_L1_5_acceptance.md` (C1). **Prerequisite: L1-F3 landed and green
(`115ca3c59`).** This session builds `test_interactive_acceptance` steps
1–7 and registers it. Session B appends steps 8–10 INTO the same
function; leave the seam it needs (named below). NO mutations in this
session — Part B runs them later, in fresh sessions.

## Implementer contract

- Work ONLY in `packages/forth-core/test_capture.part.h` and
  `packages/forth-core/test_dict_reloc.c`. NEVER edit any production
  file, `patches/`, `files/`, or anything under `src/`.
- Todo file: `/tmp/qwen-l1-5a-todo.md`. Gate log: `/tmp/qwen-l1-5a-gate.log`
  (these names OVERRIDE any remembered form).
- Gate: `./packages/forth-core/build-test.sh > /tmp/qwen-l1-5a-gate.log 2>&1`;
  inspect with bounded `grep -a`. Green iff the log shows
  `FORTH SELF-TEST: ALL PASSED` **and** `BUILD + SELF-TEST GREEN`.
- **STOP conditions (report `[SOL DEBUGGER HANDOFF]`, do not adapt):** a
  red test this packet did not write; any EXECUTION GATE anchor not
  matching; any spec statement here that contradicts the tree; any
  armed-state assert below failing (a failed precondition means the
  fixture is wrong — forcing the state to satisfy it is forbidden).
- Literals are law (AGENTS.md): every item id, expected string, and
  numeric literal below is normative. Never "correct" one to make a test
  pass.
- **Quote the `[DEBUG] running test_interactive_acceptance...` line and
  every `PASS` line verbatim from the green log in your final report.**
  The `ALL PASSED` banner alone is not evidence the new test ran.
- Finish with the commit step at the bottom. One commit, exact message.

## EXECUTION GATE (verify before any edit; STOP on mismatch)

```
grep -c "static int test_interactive_acceptance" packages/forth-core/test_capture.part.h   # expect 0
grep -c "fail |= test_cm_gate_audit();" packages/forth-core/test_dict_reloc.c              # expect 1
grep -c "static int test_cm_gate_audit(void);" packages/forth-core/test_dict_reloc.c       # expect 1
grep -c "void forthInteractiveEnter" packages/forth-core/forth_capture.h                   # expect 1
grep -c "runFunction(ITM_COLON);" packages/forth-core/test_capture.part.h                  # expect >=1
grep -c "extern void tamProcessInput(uint16_t);" packages/forth-core/test_capture.part.h   # expect >=1
git status --short                                                                          # expect empty
git log --oneline -3 | grep -c "L1-F3: operand-class parity"                                # expect 1 (the landed base)
```

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
> authors must identify steps by role and
>
> **Inspection clause (T5-2, 2026-08-03):** the same applies to READING
> steps. Tests never hand-index step bytes (signatures, length bytes,
> payload offsets) — they use the reader-side accessors in
> `test_dict_reloc.c` (`stepIsForthStep`, `stepIsMarker`,
> `stepSrcTextEq`, `tpSrcPayload`). A new layout fact means a new
> accessor, never an inline byte index. Legacy hand-index sites are
> burned down opportunistically as tests are touched.
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

Fixture-lint (run on your diff before the gate; any match outside the
central builder is a packet failure):

```
git diff -- packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c | grep -n "^+" | grep -E "beginOfProgramMemory \+ [0-9]|tpStepAddr\([^,]*, *[0-9]"
```

## C1 — `test_interactive_acceptance`, steps 1–7 (test_capture.part.h, append at END of file)

One static test function, one story, driven entirely through real entry
points; each step asserts before the next. Steps run in ONE function body
in this order — state flows from step to step deliberately (that is the
point of the test); do not "isolate" the steps from each other.

**Skeleton rules:**

- Externs: copy the extern set `test_capture_interactive_close` declares
  (grep `static int test_capture_interactive_close` for the block), plus
  `extern void tamProcessInput(uint16_t);` and
  `extern void fnKeyEnter(uint16_t);` and `extern void runFunction(int16_t);`.
- Save/restore the full global tuple around the test exactly as
  `test_capture_interactive_close` does (calcMode, catalog, tam.function,
  tam.mode, FLAG_ALPHA, programRunStop, dynamicMenuItem, T_cursorPos,
  softmenuStack via xcopy), plus `shiftF`/`shiftG`.
- Define a reset macro `L15_RESET()` copying `L12_RESET()`'s body verbatim
  (grep `#define L12_RESET` in test_capture.part.h) — same fields, same
  order — and additionally `shiftG = false;` if L12_RESET lacks it.
- Baseline program isolation (the landed L1-H lesson): before step 1,
  `cleanupTestProgram();` then build and write a baseline program
  `LBL 'BASEA'` + END via `tpInit`/`tpLbl`/`tpEnd`/`tpWrite` and abort the
  test (`return 1` after printing `    FIXTURE FAIL: baseline build/write`)
  if `tpWrite` fails. FHIST will be created after it by the story's first
  ENTER. End of the test (after restores): `forthCapClose();
  cleanupTestProgram();` then the global restores.
- `longInteger_t li;` declared once at top for the register seeds.
- Every step: on subcase failure set `scFail = 1`, print the FAIL line
  with observed values, `fail |= scFail;` — copy the landed pattern. Print
  the step's PASS line EXACTLY as given below (these strings are verified
  against the log).

**Step 1 — open from CM_NORMAL, X untouched.**

```c
scFail = 0;
L15_RESET();
longIntegerInit(li); int32ToLongInteger(7, li);
convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);
fnForthOuter(NOPARAM);
```

Assert: `forthCapIsOpen()`, `forthCapIsInteractive()`,
`calcMode == CM_AIM`, `getSystemFlag(FLAG_ALPHA)`, and X bit-identical:
`read_reg_int32(REGISTER_X, &tType, &tVal)` gives
`tType == dtLongInteger && tVal == 7` (the non-lifting open, T9).
PASS line:

```
    [1] PASS: FORTH from CM_NORMAL opens interactive; X bit-identical (no lift)
```

**Step 2 — type `1 2 +`, ENTER: live stack, REPL reopen.**

```c
runFunction(ITM_1); runFunction(ITM_SPACE);
runFunction(ITM_2); runFunction(ITM_SPACE);
runFunction(ITM_PLUS);
fnKeyEnter(NOPARAM);
```

Assert: `lastErrorCode == ERROR_NONE`, `x_is_longint(3)`,
`aimBuffer[0] == 0`, `forthCapIsOpen()`, `calcMode == CM_AIM`.
PASS line:

```
    [2] PASS: "1 2 +" ENTER computes 3 on the live stack; REPL reopens empty
```

**Step 3 — define `: SQ DUP * ;` through the key path.**

```c
runFunction(ITM_COLON); runFunction(ITM_SPACE);
runFunction(ITM_S); runFunction(ITM_Q); runFunction(ITM_SPACE);
runFunction(ITM_D); runFunction(ITM_U); runFunction(ITM_P); runFunction(ITM_SPACE);
runFunction(ITM_ASTERISK); runFunction(ITM_SPACE);
runFunction(ITM_SEMICOLON);
fnKeyEnter(NOPARAM);
```

Assert: `lastErrorCode == ERROR_NONE`, and `SQ` resolves:
`uint16_t idx; forthFindColon("SQ", &idx)` returns true. (Declare
`extern bool forthFindColon(const char *, uint16_t *);` if the file's
existing externs do not already cover it — grep first.)
PASS line:

```
    [3] PASS: ": SQ DUP * ;" defines SQ interactively
```

**Step 4 — `4 SQ` uses the definition from a later line.**

```c
runFunction(ITM_4); runFunction(ITM_SPACE);
runFunction(ITM_S); runFunction(ITM_Q);
fnKeyEnter(NOPARAM);
```

Assert: `lastErrorCode == ERROR_NONE`, `x_is_longint(16)`.
PASS line:

```
    [4] PASS: "4 SQ" uses the interactive definition; X == 16
```

**Step 5 — keys mode: SIN inserts its name; backspace edits.**

```c
runFunction(ITM_AIM);              /* the ALPHA gesture: alpha -> keys mode */
```

Armed-state assert (STOP if it fails): `forthCapKeysMode()` is true.

```c
runFunction(ITM_sin);
```

Assert: `compareString((char *)forthTestCapText(), "SIN ", CMP_BINARY) == 0`
(name + trailing space, the landed divert insert).

```c
runFunction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
runFunction(ITM_BACKSPACE); runFunction(ITM_BACKSPACE);
```

Assert: `aimBuffer[0] == 0`, `forthCapKeysMode()` still true,
`forthCapIsOpen()` still true. (Backspace is excluded from the divert by
items.c's explicit list — it edits the line natively in keys mode.)
PASS line:

```
    [5] PASS: keys mode SIN inserts its name; backspaces clear it
```

**Step 6 — keys-mode fold: `STO 0 5` types text; ENTER executes it.**

```c
longIntegerInit(li); int32ToLongInteger(555, li);
convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);
runFunction(ITM_STO);
tamProcessInput(ITM_0);
tamProcessInput(ITM_5);
```

Assert, in this order:
1. `tam.mode == 0` (the fold completed and unwound);
2. `compareString((char *)forthTestCapText(), "STO 05 ", CMP_BINARY) == 0`;
3. `read_reg_int32(5, &rType, &rVal)` gives
   `rType == dtLongInteger && rVal == 555` — **the store did NOT
   execute** (L-R4 (b): text, not action).

```c
fnKeyEnter(NOPARAM);
```

Assert: `lastErrorCode == ERROR_NONE`; `read_reg_int32(5, &rType, &rVal)`
now gives `rType == dtLongInteger && rVal == 16` (the ENTER ran the
line; X was 16 from step 4); `x_is_longint(16)` (STO leaves X);
`aimBuffer[0] == 0`; `forthCapIsOpen()`; and `forthCapKeysMode()` is now
**false** (the REPL reopen relocks to alpha input — the E5-relock
analog, forthInteractiveEnter's documented reopen).
PASS line:

```
    [6] PASS: keys-mode fold "STO 05 " types text, executes only at ENTER (R05 555 -> 16)
```

**Step 7 — EXIT closes; no string commit.**

```c
fnKeyExit(NOPARAM);
```

Assert: `forthTestCapState() == FCAP_CLOSED`, `calcMode == CM_NORMAL`,
`!getSystemFlag(FLAG_ALPHA)`, `x_is_longint(16)` (X untouched — the
rung-3 teardown never commits aimBuffer to X).
PASS line:

```
    [7] PASS: EXIT closes; CM_NORMAL, FLAG_ALPHA clear, X still 16, no string commit
```

**Session-B seam (mandatory):** after step 7's `fail |= scFail;`, place
exactly this comment line before the cleanup/restore block:

```c
  /* L1-5B: steps 8-10 are appended here by session B. */
```

## Registration (test_dict_reloc.c — outside any `if(fail)` branch)

1. Forward declaration — insert immediately after the line
   `static int test_cm_gate_audit(void);                           /* L1-F3 */`:

```c
static int test_interactive_acceptance(void);                 /* L1-5 */
```

2. Invocation — find the block

```c
  forthDictInit();
  printf("  [DEBUG] running test_cm_gate_audit...\n");
  fail |= test_cm_gate_audit();
  forthDictClear();
  forthGDictClear();
```

   and insert immediately AFTER it (before the blank line that precedes
   `printf("\nFORTH FIX-7 TESTS`):

```c

  printf("\nFORTH L1-5 TESTS (stage acceptance battery)\n");
  forthDictInit();
  printf("  [DEBUG] running test_interactive_acceptance...\n");
  fail |= test_interactive_acceptance();
  forthDictClear();
  forthGDictClear();
```

## Gate, report, commit

1. Fixture-lint (above). 2. Gate; green required; if a test THIS session
did not write goes red, STOP and report. 3. Report: quote the `[DEBUG]`
line and all 7 PASS lines verbatim; quote the `FORTH ARENA` line. 4. Commit
exactly:

```
git add packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
git commit -m "L1-5: the stage story, part 1 — open to EXIT (C1 steps 1-7)"
```
