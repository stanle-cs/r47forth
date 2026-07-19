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
   `/tmp/forth-f3-3-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-3-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f3-3-todo.md`,
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

---

## AMENDMENT F3-3A (2026-07-18) — XEQ-name steps enter the owning program's scope

The STOP report is CORRECT and accepted: the packet as authored is
contradictory.  Item 6's parenthetical ("the current scope is INTERACTIVE
unless a program step is executing") assumed XEQ-name steps participate in
scope tracking, but items 1–4 only wire the `ITM_FORTH` source-step arm.
An `XEQ 'W7'` step executed from a running program therefore resolves in
INTERACTIVE scope and cannot see its own program's words — the three
param_core failures (`ERROR_LABEL_NOT_FOUND` = 6 from the fallback arm).
The two remaining failures (`test_recurse_compile_only` [5],
`test_accept_run_lifecycle` [3]) are harness-level `forthFindColon` calls:
cross-scope introspection the new contract deliberately rejects; those two
assertions flip (named below, authorizing the edits under preamble rule 6).

Design authority (architect-ruled, recorded in DESIGN-HISTORY 2026-07-18):
scope is a property of the *executing step*, not of the source-step handler
alone.  Every step arm that resolves Forth names on a step's behalf enters
the owning program's scope through ONE shared primitive and restores on
exit.  Scope guards name→ref resolution only; by-ref execution (`FCALL`)
and ref→name display (`forthDictNameByRef`) stay scope-free.  Mutation 3
is NOT contradictory once the XEQ arm has its own enter/restore: the
per-source-step restore in `forthOuterRun` stays, and the XEQ arm's
enter/restore is separate.

### Resumption gate (replaces the original EXECUTION GATE for this resume)

1. `git branch --show-current` is `forth-core/pem-entry-fixes`.  The tree
   is DIRTY with exactly the F3-3 work already implemented — do NOT
   require a clean tree and do NOT revert anything.
2. `grep -n "forthCurrentScopeGet" packages/forth-core/forth_dict.h`
   matches (the F3-3 export exists).
3. `grep -rn "forthScopeEnterProgramStep" packages/forth-core` → ZERO
   matches (this amendment adds it).
4. The gate log shows exactly the five reported legacy failures plus all
   five `test_scope_isolation` PASS lines.
5. Append the amendment items to `/tmp/forth-f3-3-todo.md` (one per change
   letter, subcase, mutation, gate, report) before editing.

### Files (amends the packet's list)

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_compile.c`,
`programming/param_core.c`, `test_dict_reloc.c` — all under
`packages/forth-core/`.  `programming/param_core.c` is a flat package
file; preamble rule 4 extends to it.

### Change C — shared scope-entry primitive (forth_compile.c, forth_dict.h)

Insert immediately ABOVE `forthProgramStep`:

```c
/* F3-3A: single scope-entry primitive for every scope-sensitive step arm
 * (the ITM_FORTH source-step handler below; the XEQ/XEQP1 name fallback
 * in param_core.c).  Generation check + first-touch pre-scan, then select
 * the owning program's scope.  Returns the previous scope for
 * forthScopeRestore.  On pre-scan error the scope is left unchanged and
 * the caller halts its step. */
uint16_t forthScopeEnterProgramStep(const uint8_t *anyPtrInProgram)
{
  uint16_t prev = forthCurrentScope;
  forthRunGenCheckReset();
  forthPreScanOwningProgram(anyPtrInProgram);
  if (lastErrorCode != ERROR_NONE) {
    return prev;
  }
  {
    uint16_t recOff;
    uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
    if (progStart && forthScanFindRecord(progStart, &recOff)) {
      forthCurrentScope = recOff;
    } else {
      forthCurrentScope = FORTH_OWNER_INTERACTIVE;
    }
  }
  return prev;
}

void forthScopeRestore(uint16_t prev) { forthCurrentScope = prev; }
```

Declare both in `forth_dict.h` directly under `forthCurrentScopeGet`:

```c
uint16_t forthScopeEnterProgramStep(const uint8_t *anyPtrInProgram);
void forthScopeRestore(uint16_t prev);
```

### Change D — forthProgramStep uses the primitive (forth_compile.c)

Replace the body of `forthProgramStep` (from `forthRunGenCheckReset();`
through the `forthOuterCtx_t ctx;` declaration and its scope block) with:

```c
void forthProgramStep(const uint8_t *payload) {
  uint16_t prevScope = forthScopeEnterProgramStep(payload);
  if (lastErrorCode != ERROR_NONE) {
    return;                                 /* pre-scan error halts before executing this step */
  }
  forthOuterCtx_t ctx;
  ctx.savedScope = prevScope;
  uint8_t len = *payload;
  xcopy(ctx.source, payload + 1, len);
  ctx.source[len] = 0;
  forthOuterRun(&ctx, FORTH_OUTER_SKIP_DEFS);
}
```

The old inline save/derive block (`uint16_t savedScope = ...` through the
`}` closing the record-derivation scope) is gone; `forthOuterRun`'s
epilogue remains the single restore point for the source-step path
(mutation 3's target, unchanged).

### Change E — delete the forthOuterRun no-op (forth_compile.c)

In `forthOuterRun`, delete these five lines entirely (a tautological
self-assignment left by the packet's ambiguity between items 3 and 4):

```c
  /* F3-3: if caller pre-set savedScope (e.g., forthProgramStep setting it to
   * the caller's scope), do not overwrite — use it as the restore target. */
  if (ctx->savedScope == forthCurrentScope) {
    ctx->savedScope = forthCurrentScope;
  }
```

Contract (now explicit): EVERY caller of `forthOuterRun` pre-sets
`ctx.savedScope`; `forthOuterRun` only restores.  All four call sites
already comply (`forthOuterInterpret`, `fnForthOuter`, the pre-scan loop,
`forthProgramStep`).

### Change F — the XEQ/XEQP1 fallback arm participates (programming/param_core.c)

In `paramCoreExecuteOp`, PARAM_LABEL case, replace the entire
`else if (forthFallbackEligible) { ... }` block with:

```c
        else if (forthFallbackEligible) {
          /* F3-3A: this resolution acts for the step being executed — enter
           * the owning program's scope (first touch included) so same-
           * program words resolve and cross-scope words do not.  paramAddress
           * points into the step; a non-program address (defensive) falls
           * back to INTERACTIVE inside the helper.  Scope guards name
           * resolution only: the FCALL dispatch below runs by ref and needs
           * no scope of its own. */
          uint16_t prevScope = forthScopeEnterProgramStep(paramAddress);
          if(lastErrorCode != ERROR_NONE) {
            forthScopeRestore(prevScope);   /* first-touch pre-scan failed: halt this step */
          }
          else {
            uint16_t resolvedParam;
            forthXEQType_t res = forthResolveXEQ(tmpStringLabelOrVariableName, &resolvedParam);
            if(res == FORTH_XEQ_COLON) {
              reallyRunFunction(ITM_FCALL, resolvedParam);
              if(op == ITM_XEQP1 && programRunStop == PGM_RUNNING && lastErrorCode == ERROR_NONE) {
                currentReturnLocalStep++;
              }
            }
            else if(res == FORTH_XEQ_ITEM) {
              reallyRunFunction(resolvedParam, NOPARAM);
            }
            else {
              displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
              #if (EXTRA_INFO_ON_CALC_ERROR == 1)
                sprintf(errorMessage, "string '%s' is not a named label", tmpStringLabelOrVariableName);
                moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
              #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
            }
            forthScopeRestore(prevScope);
          }
        }
```

The GTO/LBLQ/not-found arms outside this block are untouched (GTO is not
fallback-eligible; the two direct `paramCoreExecuteOp(..., ITM_GTO, ...)`
test calls never reach the hook).

### Change G — two legacy assertions flip to the new contract (test_dict_reloc.c)

These two edits are NAMED by this amendment (preamble rule 6 satisfied).
Both tests' product assertions (the RAM_FULL recursion; X == 9 after
resume) already prove the words compiled and ran in program scope; the
flipped lines now pin the isolation instead of the old cross-scope
visibility.

1. `test_recurse_compile_only` subcase 5 — replace:

```c
    else if (!forthFindColon("PRW", &idx)) {
      printf("    FAIL [5]: PRW not found after scan\n");
      sub5Fail = 1;
    }
```

with:

```c
    else if (forthFindColon("PRW", &idx)) {   /* F3-3: program-owned, invisible interactively */
      printf("    FAIL [5]: PRW visible from interactive scope (F3-3 isolation)\n");
      sub5Fail = 1;
    }
```

2. `test_accept_run_lifecycle` subcase 3 — replace:

```c
      else if (!forthFindColon("SQ", &idx)) {
        printf("    [3] FAIL: SQ not found after resume\n");
        fail = 1;
      }
```

with:

```c
      else if (forthFindColon("SQ", &idx)) {   /* F3-3: program-owned, invisible interactively */
        printf("    [3] FAIL: SQ visible from interactive scope after resume (F3-3 isolation)\n");
        fail = 1;
      }
```

The adjacent PZW must-not-be-found check and both PASS banner texts stay
unchanged.

### Change H — fixture step + subcase 6 (test_dict_reloc.c, test_scope_isolation)

Extend the Change A fixture: append directly after the `sUseB` line
(inside program B, before `tpWrite`):

```c
int sXeqA  = tpXeqName(&tp, "WA");
```

Include `sXeqA` in the existing -1 handle check.  ("WA" length 2 ≤ 16.)

Add subcase 6 after subcase 5, same drive discipline as subcases 1–2:

```c
  /* [6] cross-program XEQ-name step: B's XEQ 'WA' must reject in B's scope */
  forthPushInt32(88);
  savedRS = programRunStop;             /* reuse the subcase drive locals */
  lastErrorCode = ERROR_NONE;
  dynamicMenuItem = -1;
  programRunStop = PGM_RUNNING;
  currentStep = tpStepAddr(&tp, sXeqA);
  executeOneStep(currentStep);
  programRunStop = savedRS;
  if (lastErrorCode != ERROR_LABEL_NOT_FOUND) {
    printf("    [6] FAIL: expected ERROR_LABEL_NOT_FOUND, got %d\n", lastErrorCode);
    fail = 1;
  }
  else if (!x_is_longint(88)) {
    printf("    [6] FAIL: X changed across rejected XEQ\n");
    fail = 1;
  }
  else if (forthCurrentScopeGet() != FORTH_OWNER_INTERACTIVE) {
    printf("    [6] FAIL: scope not restored after rejected XEQ step\n");
    fail = 1;
  }
  else {
    printf("    [6] PASS: cross-program XEQ-name step rejected in the step's scope\n");
  }
  lastErrorCode = ERROR_NONE;
```

(Adapt the drive-local names to the subcase-1 block's actual spellings if
they differ; the discipline — save, seed, drive once, assert, restore,
clear — is normative, the local variable names are not.)  Note the error
is `ERROR_LABEL_NOT_FOUND` (the param_core arm's step-surface error), NOT
`ERROR_FUNCTION_NOT_FOUND` — the same name failing in a SOURCE step
(subcase 2) errors differently from an XEQ step by design.

### Gate and mutations (supersedes the packet's section)

Full gate green first: all SIX `test_scope_isolation` PASS lines, the two
flipped legacy branches silent, all five previously-failing legacy tests
back to their normal PASS banners, every other legacy banner, both
success banners, exit 0.

Mutations, each separately, logs `/tmp/forth-f3-3-mut1..5.log`.  During a
mutation run, co-reds beyond the named required RED are expected mutation
fallout and are NOT rule-6 STOP events; the named RED line must appear,
and the post-restore gate must be fully green before the next mutation.

1. UNCHANGED: delete the owner-filter skip in the shared colon walk →
   subcase 2 MUST go RED.  (Subcase 6 and others may co-red.)
2. RE-TARGETED: in `forthScopeEnterProgramStep`, delete the entire
   record-derivation compound (`{ uint16_t recOff; ... }` — scope keeps
   its previous value) → subcase 3(c) MUST go RED (WI resolves from
   program context).  (Subcase 1 and the param_core legacy tests co-red.)
3. UNCHANGED: in `forthOuterRun`, delete the scope restore (keep the
   epilogue's defState restore) → subcase 4 MUST go RED.
4. UNCHANGED: in `startDefinition`, delete the owner stamp → subcase 3(a)
   MUST go RED.
5. NEW: in `param_core.c`, revert Change F to the direct resolve (delete
   the enter/error-check/restore, keep the resolve+dispatch) →
   `test_param_core_bounded_names` [1] MUST go RED (`executeOneStep
   error 6` instead of X=7).  Subcase 6 stays green under this mutation
   by design (rejection either way) — the legacy positive is the
   detector; that is why it is the named RED.

Report: six PASS lines, the five recovered legacy banners, both success
banners, exit 0, arena line vs the F3-2 baseline, `git diff --check`,
generated-mirror equality, all five mutation REDs.  RULE-1: negligible
flash delta expected; note PENDING.  Commit line unchanged.
