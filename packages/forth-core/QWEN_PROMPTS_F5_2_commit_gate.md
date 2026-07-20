# Stage F5-2 — the commit gate: atomic reject at ENTER, advisory stays silent

Origin: DESIGN §10.5/§8.4 E9 via `QWEN_PROMPTS_F5_core.md` §1-§2.  One
call site plus capture-driven acceptance.  This closes stage F5 and
implements E9 end to end.

Traced facts this packet relies on (`QWEN_PROMPTS_F5_core.md` §1): the
ENTER commit arm is `pemAlpha`'s `ITM_ENTER` branch
[packages/forth-core/programming/manage.c:924-936] computing
`wasForth`/`hadText` before `pemCloseAlphaInput()`; the step bytes track
the buffer incrementally, so refusing the commit means refusing the
CLOSE (capture stays open, `aimBuffer` intact, error displayed); the E3
empty-line rule lives inside `pemCloseAlphaInput` [manage.c:1017-1042]
and is untouched (an empty ENTER is not a tier-1 case).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log -1 --format=%s` is exactly:
   `forth-core: F5-1 — check mode: the tokenizer validates its own grammar`.
2. `grep -n "forthCheckSourceLine" packages/forth-core/forth_dict.h
   packages/forth-core/forth_compile.c` shows the declaration and
   definition; `grep -n "forthCheckSourceLine" packages/forth-core/programming/manage.c`
   → ZERO matches (no call site yet).
3. `grep -n "bool_t wasForth = (tam.function == ITM_FORTH);"
   packages/forth-core/programming/manage.c` matches exactly one line
   (the ENTER arm anchor).
4. `grep -n "test_accept_entry_state_roundtrip" packages/forth-core/test_dict_reloc.c`
   matches (the F15-2 fixture block this packet's drives reuse).
5. Pre-gate green (`/tmp/forth-f5-2-pre.log`); arena baseline from the
   F5-1 commit.

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
   `/tmp/forth-f5-2-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f5-2-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f5-2-todo.md`,
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

## F5-2 — ENTER refuses what can never run

### Authority carried by this packet (no open choices)

1. **The call site** — in `pemAlpha`'s `ITM_ENTER` arm, immediately
   after the `wasForth`/`hadText` locals and BEFORE
   `pemCloseAlphaInput()`:

   ```c
   if(wasForth && hadText && !forthCheckSourceLine(aimBuffer)) {
     return;   /* E9 tier 1: commit refused atomically — capture stays
                  open, aimBuffer intact for correction, the error is
                  already displayed.  Tier 2 (names) never reaches here:
                  forthCheckSourceLine accepts them. */
   }
   ```

   Nothing else changes: the empty-ENTER path (`!hadText`) keeps its E3
   semantics; non-Forth alpha commits are untouched; the multi-line
   lock after `pemCloseAlphaInput` is untouched.
2. Capture-drive contract for every fixture below (F15-4, binding):
   the ALPHA gesture (`runFunction(ITM_AIM)`) is the FIRST key with the
   cursor ON the opening marker and `pemCursorIsZerothStep` owned by
   the fixture; keys are typed via the F15-2/F15-4 landed drive idiom
   (read those tests, reuse their helpers verbatim).

### Files

Modify only: `packages/forth-core/programming/manage.c`,
`packages/forth-core/test_dict_reloc.c`.

### Targeted reads

1. manage.c: the ITM_ENTER arm plus 10 lines around it;
   `pemCloseAlphaInput` in full (small).
2. test_dict_reloc.c: the F15-2 entry-state test's PEM fixture block and
   its key-drive helpers; the F15-4 test's capture-typing idiom; tp
   helpers; registration lines.

### New test `test_commit_gate` (register after the newest test)

Build ONE tp fixture: `tpMarker` (opening `»FORTH`), `tpWrite`, then
open capture per the drive contract (cursor on the marker, ITM_AIM
first).  Record `stepCountBefore = getNumberOfSteps()` after capture
open (the placeholder is already inserted — record AFTER opening so the
deltas below are pure commit effects).  Subcases in ONE capture session
where feasible; otherwise reopen per the same contract and say so:

1. **Malformed line refuses atomically.**  Type `: A IF ;` (the F5-1
   tier-1 row) via the key/alpha drive; press ENTER
   (`pemAlpha(ITM_ENTER)` through the landed idiom).  Require:
   `lastErrorCode == ERROR_INVALID_NAME`; `getSystemFlag(FLAG_ALPHA)`
   still set (capture OPEN); `aimBuffer` still byte-equal to the typed
   line; `getNumberOfSteps() == stepCountBefore` (no committed advance —
   the placeholder is still the editing step).  Clear the error.
   `[1] PASS: tier-1 line refused; capture and buffer intact`
2. **Correction commits.**  Without reopening, edit the buffer to
   `: A 1 IF 2 THEN ;` (backspace/type via the drive idiom — or CLA and
   retype, whichever the landed helpers support; state which in the
   report).  ENTER.  Require: no error; FLAG_ALPHA still set (the
   multi-line lock reopened the next line — entry state derived);
   step count advanced by one committed source step.
   `[2] PASS: corrected line commits and the lock advances`
3. **Advisory line commits.**  Type `FUTUREWORD 9 +` and ENTER.
   Require: no error; the step committed (count advanced) — an
   unresolved name is tier 2 and never blocks (E9).
   `[3] PASS: unresolved names commit untouched`
4. **Empty ENTER keeps E3.**  With an empty buffer, ENTER.  Require the
   landed E3/§8.9-item-8 behavior byte-for-byte: no source step added,
   the capture close/reopen exactly as the F15-2 pins expect (assert
   via step count and FLAG_ALPHA per the neighboring landed test's
   expectations — reuse its assertion shape).
   `[4] PASS: empty ENTER unchanged (E3)`
5. **Run confirms the committed program.**  Close capture, run the
   program via the F15-1 drive discipline over its steps; `A` was
   defined with `IF` — drive the definition step, then interpret
   `"1 A"` in program scope? — NO: keep it minimal: after closing,
   execute the two committed source steps once each
   (`executeOneStep` per role handle) and require no error; then
   `forthCurrentScopeGet() == FORTH_OWNER_INTERACTIVE` and
   `forthTestGetRsp() == 0` (hygiene).
   `[5] PASS: the committed lines execute; state hygiene holds`

Cleanup: full capture teardown per the landed tests, program memory
restored, both regions cleared, error state cleared.

### Existing tests

Untouched and green — especially the F15-2 entry-state and F15-4
capture tests (they are the regression canaries for this key path; a
red there is an immediate STOP).

### Non-goals / STOP boundaries

- No advisory UI, no validation on scroll-away/EXIT, no interactive
  (`fnForthOuter`) pre-check, no changes to `pemCloseAlphaInput` or the
  multi-line lock.
- No new tier-1 rules — the check function is F5-1's, used as-is.

### Gate and required mutations

Full gate green (five PASS lines).  Mutations, each separately, verbatim
anchors, manual restore, `/tmp/forth-f5-2-mut1..3.log`:

1. Delete the new call site (the whole `if`).  Subcase 1 MUST go RED
   (the malformed line commits: FLAG_ALPHA advanced past it / step
   count moved / no INVALID_NAME at ENTER — name the observed symptom).
2. Change the call to clear the buffer on reject (add
   `aimBuffer[0] = 0;` before `return`).  Subcase 1's buffer-intact
   assertion MUST go RED.
3. Invert the verdict (`!forthCheckSourceLine` → `forthCheckSourceLine`).
   Subcases 1 and 3 MUST BOTH go RED (tier-1 commits, advisory
   refuses) — the double RED is required; a single RED is a STOP.

Residue-free diff; final gate; report the five PASS lines, banners,
exit 0, the two-region arena line, `git diff --check`, mirror
equality.  RULE-1: one call site — report the (near-zero) delta with
the stage-closing commit.

### Commit

```text
forth-core: F5-2 — E9 commit gate: structural rejects, advisory commits
```

---

## FIXTURE RULES carried forward from the F4-2/F4-3 debug (binding, 2026-07-19)

Each of these cost a red gate in stage F4. They apply to every test this
packet adds, whether or not the body above repeats them.

1. **Seeded stacks and `x_set_string` are incompatible.** `x_set_string`
   overwrites REGISTER_X with the source string and `fnForthOuter` drops
   it, shifting anything pushed beforehand one level down. A fixture that
   needs values on the stack runs its source through
   `forthOuterInterpret(...)`; `x_set_string` + `fnForthOuter` is for
   compile-only fixtures that need no seeded stack.
2. **`forthFindColon` returns a REF INDEX, not a byte offset.** Byte-image
   assertions walk from
   `fdict.latest + TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK`. Using the ref
   as an offset happens to work for the first word defined after a clear
   (both are 0) and silently corrupts every later image.
3. **Every subcase opens with `lastErrorCode = ERROR_NONE;`.** Neighbouring
   tests deliberately end on an error code; an unset read reports a phantom
   failure that looks like a code defect.
4. **Anything a fixture allocates, the fixture frees** — named variables in
   particular (data block per variable plus the header table, back to the
   pre-test count; see `test_param_named_indirect`'s cleanup). The suite's
   end-of-run `numberOfAllocatedMemoryRegions` gate reddens on a leak, and
   assigning that counter to hide the growth is a packet violation.
5. **`compareString` is a C-string comparison returning 0 on equal.** A
   truthy test (`if (compareString(a, b, CMP_BINARY))`) reads as "not
   equal"; and any buffer you hand it must be NUL-terminated.

---

## AMENDMENT F5-2A (2026-07-19) — what actually went wrong, and the rules it changes

The implementation of this packet was correct: the six-line call site above is
exactly what landed. The gate went red anyway, in **four tests this packet
never touched** (F3-3 scope isolation, the F4-4 parity sweep, and this
packet's own subcase 5 — all reporting a bogus `forthCurrentScope`, e.g.
`0x800E`). The implementer spent the session repairing its own correct code.

**Root cause — a latent defect in F5-1, not in F5-2.**
`forthOuterRun`'s epilogue restores `forthCurrentScope = ctx->savedScope`.
The prologue fills `savedDef` and `savedLatestClosed` itself, but
`savedScope` is the CALLER's to snapshot, and every entry point does it
(`forthOuterInterpret`, `fnForthOuter`, `forthProgramStep`, the pre-scan) —
except `forthCheckSourceLine`, which set only `ctx.source`. Check mode
therefore wrote an uninitialized stack word into the live scope. It stayed
invisible while only F5-1's own test called check mode (that test never
observed scope); wiring check mode into `pemAlpha`'s commit seam made every
PEM-driving test downstream inherit the garbage. The fix is one line in
`forth_compile.c`:

```c
ctx.savedScope = forthCurrentScope;   /* the epilogue restores FROM this */
```

### Rules this changes (binding for every later packet)

1. **REGRESSION TRIAGE — a red outside your diff is an immediate STOP.**
   Rule 6 forbids *editing* another test; it did not say what to do. It does
   now: if a test this packet did not write reddens, STOP at once and report
   `[SOL DEBUGGER HANDOFF]` — **zero** repair attempts. The two-attempt
   allowance applies only to a red in code or tests this packet authored.
   Do not try to attribute the failure first: "my change cannot have caused
   this" is the single most common wrong conclusion, and attribution is
   exactly what the debugger handoff exists to do.
2. **Diff the PASS sets, don't read the failures.** The pre-gate log
   (`/tmp/forth-f5-N-pre.log`) is kept for exactly this:
   `grep -c "PASS" pre.log` vs the failing gate, and
   `diff <(grep -o "PASS: .*" pre.log | sort) <(grep -o "PASS: .*" gate.log | sort)`.
   Newly-missing PASS lines in untouched tests name the blast radius in one
   command. Report that diff with the handoff.
3. **ENTRY-POINT CONTRACT PRE-FLIGHT.** Before wiring an existing function
   into a new call site, enumerate the process-global state that function's
   family saves and restores, and prove the callee does the same. Concretely:
   grep every sibling entry point for the fields the shared epilogue restores
   (here: `grep -n "savedScope\|savedDef\|savedLatestClosed"`), and STOP with
   a packet-defect report if the function you are about to call is missing
   one. A function that is correct in isolation can still be wrong the moment
   a second caller exists.
4. **Pin the contract, not just the verdict.** DESIGN §10.5 says check mode
   "executes nothing, allocates nothing, mutates no live state" — a normative
   claim that F5-1 shipped with no test at all, which is why the defect was
   free to land. Any packet adding an entry point with a state-neutrality
   claim MUST pin it directly: set the state to a NON-default value, call the
   entry point on both an accepted and a rejected input, and assert the state
   came back. `test_check_source_line` subcase 6 is the landed shape,
   including `poisonAutoFrame()` — which fills the stack region the callee's
   frame will occupy with `0xAA` so an uninitialized restore reports the
   deterministic `43690` instead of luck-of-the-stack.

### Landed additions beyond the packet body

- `forth_compile.c`: the missing `ctx.savedScope` snapshot.
- `forth_dict.h` / `forth_compile.c`: `forthTestScopeSet` (FORTH_DEBUG_SELFTEST
  only) so a test can prove restoration from a non-default scope.
- `test_dict_reloc.c`: `poisonAutoFrame()` + `test_check_source_line`
  subcase 6 (scope, open-definition state, rsp, and dictionary neutrality).
- Mutation 0 (beyond the packet's three): delete the `ctx.savedScope` line →
  subcase 6 reds with `scope 43690 (expected 4660)`, and it names the defect
  instead of scattering four unrelated reds.

Measured cost (RULE-1): `make dmcp5r47` flash 1092216 → 1093016, **+800
bytes**, RAM unchanged at 7188. The jump is larger than a six-line call site
because F5-1's check-mode code was unreachable until now and LTO had been
dropping it; this delta is the true cost of E9 tier 1 going live.

**Build-measurement trap worth knowing:** `make dmcp5r47 ... f=1` does NOT
re-materialize the package shadow, so a flash measurement taken after
swapping package sources with `f=1` silently re-reports the previous tree's
size. Use `CUSTOM_PKG_RECONFIGURE=1` for any before/after size comparison
(the tell: ~509 targets rebuilt, not ~51).
