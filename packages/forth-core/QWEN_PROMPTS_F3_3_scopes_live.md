# Stage F3-3 — scopes live: owner stamping + filtered lookup

Origin: DESIGN §10.3 via the F3 design pass (`QWEN_PROMPTS_F3_core.md` D3).
This packet turns the dormant owner field on: definitions are stamped with
the current scope, transient lookup filters to the current scope, and the
scope variable tracks program-step execution.  gdict lookup (already wired
in F3-2) is the second search stage and is scope-free.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F3-2 commit
   `forth-core: F3-2 — global dictionary region, word refs, gdict
   persistence`.
2. `grep -n "forthFindColonRef" packages/forth-core/forth_dict.c` shows the
   definition; `grep -n "FORTH_REF_GLOBAL" packages/forth-core/forth_dict.h`
   matches.
3. `grep -n "forthCurrentScope" packages/forth-core -r` → ZERO matches (no
   scope variable exists yet).
4. `grep -n "forthScanIsRecorded\|forthScanRecord\|forthScanHead"
   packages/forth-core/forth_compile.c` shows the F1-3 record walk
   (`forthScanIsRecorded` with the strictly-decreasing `prev` guard) and
   `forthScanRecord`.
5. `grep -n "tpEnd" packages/forth-core/test_dict_reloc.c` → ZERO matches
   (this packet adds the builder helper).
6. Pre-gate green; arena baseline from the F3-2 commit message.

---

## PREAMBLE (paste before the task)

Identical to F3-1's preamble with paths renamed to `f3-3`
(`/tmp/forth-f3-3-todo.md`, `/tmp/forth-f3-3-gate.log`, mutation logs
`/tmp/forth-f3-3-mutN.log`).  All nine rules and the two-attempt handoff
apply verbatim.

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

## F3-3 — every definition knows its owner; every lookup honors it

### Authority carried by this packet (no open choices)

1. **Scope variable** (forth_compile.c):

   ```c
   static uint16_t forthCurrentScope = FORTH_OWNER_INTERACTIVE;
   uint16_t forthCurrentScopeGet(void) { return forthCurrentScope; }
   ```

   Declare the getter in forth_dict.h.  `fnForthOuter` and
   `forthOuterInterpret` DO NOT touch it (a nested FORTH-item evaluation
   from a program tail inherits the program's scope by design; genuinely
   interactive entries already see the INTERACTIVE default).
2. **Record lookup helper** (forth_compile.c): rework
   `forthScanIsRecorded` into

   ```c
   static bool forthScanFindRecord(const uint8_t *progStart, uint16_t *recOff);
   static bool forthScanIsRecorded(const uint8_t *progStart) {
     uint16_t t; return forthScanFindRecord(progStart, &t);
   }
   ```

   `forthScanFindRecord` is today's `forthScanIsRecorded` walk verbatim
   (including both self-heal guards) with `*recOff = off;` before the
   `return true`.
3. **Scope assignment**, exactly two owners of the variable:
   - `forthProgramStep`: after the pre-scan returns clean, save the scope,
     set it from `forthScanFindRecord(forthOwningProgramStart(payload), …)`
     (defensive fallback `FORTH_OWNER_INTERACTIVE` if either lookup fails),
     run the SKIP_DEFS interpretation, restore the scope before returning
     — including on the pre-scan-error early return path (structure the
     function so the save happens before any early return that follows the
     assignment; the pre-scan-error return happens BEFORE the assignment
     and needs no restore).
   - `forthPreScanOwningProgram`: after `forthScanRecord(progStart)`
     succeeds, save the scope and set it to the new record's offset (the
     `newOff` value recorded by `forthScanRecord` — capture it via
     `forthScanHead` immediately after the call, which now points at the
     new record); restore it on EVERY exit path after that point (the
     error-rollback return and the success fall-through).
4. **Snapshot in the context**: add `uint16_t savedScope;` to
   `forthOuterCtx_t`; in `forthOuterRun`, save `forthCurrentScope` beside
   the defState save and restore it beside the defState restore.
5. **Owner stamping**: in `startDefinition` (forth_dict.c), immediately
   after `forthDictWriteName(...)`:

   ```c
   ((forthHeader_t *)(fdict.base + off))->owner = forthCurrentScopeGet();
   ```

   (`forthDictAllocate`'s own INTERACTIVE default remains for non-compiler
   callers, e.g. test fixtures.)
6. **Filtered lookup**: in the shared walk behind
   `forthFindColonRef`/`forthFindColon`, the fdict stage additionally
   skips entries whose `hdr->owner != forthCurrentScopeGet()` (the skip
   sits beside the FF_SMUDGE skip; the index bookkeeping `n` still counts
   every entry).  The gdict stage is unchanged (globals are visible from
   every scope).  `forthResolveXEQ`'s colon arm and the tam/keyboard/
   screen hooks go through this same walk and inherit the filtering —
   from RPN surfaces the current scope is INTERACTIVE unless a program
   step is executing, which is exactly the contract.
7. No other behavior changes.  `forthDictNameByRef` stays UNFILTERED (the
   FCALL redirect must be able to name any ref it is handed).

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_compile.c`,
`test_dict_reloc.c` — all under `packages/forth-core/`.

### Targeted reads

1. forth_compile.c: `forthScanIsRecorded`, `forthScanRecord`,
   `forthProgramStep`, `forthPreScanOwningProgram`, the `forthOuterCtx_t`
   struct and `forthOuterRun`'s save/restore prologue/epilogue.
2. forth_dict.c: `startDefinition`, the shared colon walk.
3. test_dict_reloc.c: the tp* builder block (grep `tpMarker` and read
   through `tpSrcPayload`), the F15-1 drive discipline (grep
   `test_accept_run_lifecycle` and read its subcase-1 drive block only),
   `run_word`, and the registration lines after the newest test.

### Change T — builder extension (test_dict_reloc.c)

Add after `tpOp1`, mirroring its style:

```c
static int tpEnd(testProg_t *p)                            /* ITM_END separator */
{
  uint8_t s[2];
  s[0] = (ITM_END >> 8) | 0x80;
  s[1] =  ITM_END       & 0xff;
  return tpAppend(p, s, 2, TP_STEP_OP1);
}
```

### Change A — new test `test_scope_isolation` (register after the newest test)

One fixture, built once (roles, not offsets; abort the test on any -1
handle or `tpWrite` failure):

```c
testProg_t tp; tpInit(&tp);
int sLblA  = tpLbl(&tp, "PA");
int sDefA  = tpSrc(&tp, ": WA 41 ;");
int sUseA  = tpSrc(&tp, "WA");
int sUseWI = tpSrc(&tp, "WI");
(void)tpEnd(&tp);
int sLblB  = tpLbl(&tp, "PB");
int sDefB  = tpSrc(&tp, ": WB 42 ;");
int sUseB  = tpSrc(&tp, "WA 1 +");
if (!tpWrite(&tp)) { /* FAIL + return */ }
```

(`printf '%s' ": WA 41 ;" | wc -c` = 9; `": WB 42 ;"` = 9; `"WA 1 +"` = 6;
all ≤ 64.)  Drive discipline for every subcase: save `programRunStop`;
`lastErrorCode = ERROR_NONE; dynamicMenuItem = -1;` set
`programRunStop = PGM_RUNNING`; `currentStep = tpStepAddr(&tp, <role>)`;
`executeOneStep(currentStep);` exactly once; then assert; restore
`programRunStop` and clean up on every path (fixture discipline of the
F15-1 test).  Call `forthRunGenBump()` once before subcase 1 to model the
top-level run start (the first source-step drive consumes it).

1. **Program A derives and uses its own word.**  Drive `sDefA` (first
   touch: pre-scans A — both definitions? No: A's pre-scan compiles only
   A's `ITM_FORTH` steps, i.e. `: WA 41 ;` — `sUseA`/`sUseWI` are tail
   lines, compiled nothing), then drive `sUseA`.  Require no error and
   `x_is_longint(41)`.  PASS line:
   `[1] PASS: program word resolves in its own scope`
2. **Program B cannot see A's word.**  Drive `sDefB` (B's first touch),
   then drive `sUseB`.  Require `lastErrorCode == ERROR_FUNCTION_NOT_FOUND`
   and that X is UNCHANGED from a sentinel you set immediately before the
   drive (seed X with `forthPushInt32(77)` and require `x_is_longint(77)`
   after).  Clear the error before the next subcase.  PASS line:
   `[2] PASS: cross-program lookup rejected`
3. **Interactive cannot see program words; programs cannot see
   interactive words.**  Interactively run `forthOuterInterpret(": WI 7 ;")`
   (same lifetime — no bump since subcase 1).  Then:
   (a) `forthOuterInterpret("WA")` must set `ERROR_FUNCTION_NOT_FOUND`
   (clear after); (b) `run_word("WI")` must leave `x_is_longint(7)`;
   (c) drive `sUseWI` (program A context) and require
   `ERROR_FUNCTION_NOT_FOUND` (clear after).  PASS line:
   `[3] PASS: interactive and program scopes are mutually invisible`
4. **Scope restores to INTERACTIVE after every drive.**  Require
   `forthCurrentScopeGet() == FORTH_OWNER_INTERACTIVE` here (after all
   drives), and additionally require it immediately after subcase 2's
   error drive (error path restore).  PASS line:
   `[4] PASS: current scope restored to interactive`

Cleanup: `forthDictClear(); forthGDictClear();` restore program memory the
way neighboring tp-based tests do, `lastErrorCode = ERROR_NONE`.

### Change B — global words stay visible from program scope

Extend `test_scope_isolation` with subcase 5: hand-build global `GVIS`
(g-helpers from F3-2; body `FTOK_ILIT`, int32 9, `FTOK_EXIT`), then drive a
new source step in program A... the fixture is already written, so instead
interpret through the PROGRAM path: drive `sUseA` again — no.  Use this
exact design: build `GVIS` BEFORE `tpWrite` happens is impossible (order);
therefore subcase 5 uses the INTERACTIVE path plus the scope variable
directly is FORBIDDEN (tests must not poke the static).  Do this instead:
after subcase 4, `forthOuterInterpret("GVIS")` must resolve (interactive →
gdict stage) leaving `x_is_longint(9)`, and `forthOuterInterpret(": WG GVIS 1 + ;")`
then `run_word("WG")` must leave `x_is_longint(10)` — a transient word
CALLING a global one through the compile-time gdict arm.  PASS line:
`[5] PASS: global word visible and callable from transient scope`

### Existing tests

All stay green untouched.  Legacy tests that define words interactively
and look them up interactively are scope-neutral (INTERACTIVE = default on
both sides).  If any legacy test reddens, STOP and report — the likely
cause is a packet defect in the filter placement, not the test.

### Non-goals / STOP boundaries

- No GLOBAL/IMMEDIATE/FORGET words (F3-4).  No XEQN (F3-6).
- No lifecycle change: signal/consumption sites untouched.
- `forthDictNameByRef` stays unfiltered; the picker (§8.6) is text-only
  and untouched.

### Gate and required mutations

Full gate green first (all five PASS lines + every legacy banner).
Mutations, each separately:

1. In the shared colon walk, delete the owner-filter skip.  Subcase 2 MUST
   go RED (WA resolves from B: no error, X == 42).  Green = STOP.
2. In `forthProgramStep`, delete the scope assignment (keep save/restore).
   Subcase 3(c) MUST go RED (WI resolves from program context).
3. In `forthOuterRun`, delete the scope restore (keep the save).  Subcase
   4 MUST go RED (scope left non-INTERACTIVE after a program drive).
4. In `startDefinition`, delete the owner stamp.  Subcase 3(a) MUST go RED
   (WA carries the INTERACTIVE default owner and resolves interactively).

Logs `/tmp/forth-f3-3-mut1..4.log`; residue-free diff; final gate;
record: five PASS lines, both banners, exit 0, arena line vs baseline,
`git diff --check`, generated-mirror equality.  RULE-1: negligible flash
delta expected; note PENDING.

### Commit

```text
forth-core: F3-3 — definitions are scope-owned and lookup honors the owner
```
