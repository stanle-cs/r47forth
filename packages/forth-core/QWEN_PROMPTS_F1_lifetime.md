# Stage F1 lifetime foundations — bounded implementation prompts

Origin: accepted R4 architecture, the R6 pre-execution audit, and
`R6_RESOLUTION_PLAN.md` Step 7. This file deliberately authors only the first
executable unit. Later units depend on APIs and layouts that do not exist until
their predecessor lands; their packets must be authored against the resulting
green tree, not guessed in advance.

## Status and dependency order

| Task | Status | Dependency |
|---|---|---|
| F1-1 pending-reset truth + active-frame guard | **READY TO EXECUTE** | R6 GO checkpoint (`b7fd711ff`) |
| F1-2 top-level run/SST lifetime signaling | **NOT YET AUTHORED** | F1-1 committed green |
| F1-3 dynamic arena-backed pre-scan tracking | **NOT YET AUTHORED** | F1-2 committed green |
| F1-4 compile-only `RECURSE` | **NOT YET AUTHORED** | F1-3 committed green |
| F1-5 full restore-time threaded-code validator | **NOT YET AUTHORED** | F1-4 committed green |

Only F1-1 below is executable. After it lands, the architect must re-grep the
new tree and author F1-2 with current anchors. Do not infer later tasks from the
outline at the end of this file.

---

## PREAMBLE (paste before F1-1)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, or identifier does not
match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list: one item per
   file, function, test subcase, mutation, final gate, and report. Mark each
   item in progress/completed as you work; do not report success with an open
   item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never invoke
   meson or ninja directly.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, or `test_dict_reloc.c` in full. Grep the named anchors and
   read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named hook. Report the arena line and anything surprising.

**Two-attempt debugger handoff.** After the task implementation first fails a
required command because of your changes, you may make at most two distinct
repair attempts, each followed by the relevant rerun. This does not override
an immediate STOP rule and does not apply to an expected mutation RED. If the
second repair is not green, STOP and report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and relevant verbatim output;
- repair 1 and result;
- repair 2 and result;
- current `git status --short`, `git diff --stat`, and relevant diff;
- remaining hypotheses and surprises.

---

## F1-1 — Pending reset is truth; active Forth frames defer invalidation

### Authority carried by this packet

The accepted lifetime contract is:

- Equality of wrapping 16-bit generation counters is never a correctness
  predicate. A Forth-private pending-reset event is the truth.
- A genuine top-level RPN start requests that reset. A launch made while a
  `forthInner` frame is active belongs to that active Forth lifetime and must
  not request a new one.
- The first safe Forth program-step entry consumes a pending reset by clearing
  the dictionary and first-touch scan state. If a Forth inner frame is active,
  consumption is deferred and the event remains pending.
- The existing counters may remain and wrap for diagnostics. They are updated
  but never consulted to decide whether to clear.
- F1-1 changes the reset mechanism only. F1-2 will centralize top-level run
  signaling in `runProgram` and add PEM single-step. Do not alter production
  bump call sites in this task.

### Files

Modify only:

- `packages/forth-core/forth_dict.h`
- `packages/forth-core/forth_inner.c`
- `packages/forth-core/forth_compile.c`
- `packages/forth-core/test_dict_reloc.c`

Read-only verification:

- `packages/forth-core/programming/lblGtoXeq.c` — confirm the two existing
  `forthRunGenBump()` sites still exist; do not edit them.

### Targeted reads

1. In `forth_compile.c`, grep
   `forthRunGeneration\|forthResetGeneration\|forthRunGenBump\|forthRunGenCheckReset\|forthScannedCount`
   and read lines 35-65 plus the `forthProgramStep` entry, no more than 35 lines
   per slice.
2. In `forth_inner.c`, grep `forthDepth\|void forthInner` and read lines 18-32
   and 170-190 only.
3. In `forth_dict.h`, grep `forthRunGenBump\|PC_BUILD` and read only the nearby
   prototype blocks.
4. In `test_dict_reloc.c`, grep
   `test_program_step_gen_reset\|test_prescan_generation_rearm\|writeTestProgram\|forthDictSelfTest`
   and read only those functions/helpers and their registration lines.
5. In `programming/lblGtoXeq.c`, grep `forthRunGenBump` only. There must be two
   production call sites. If not, STOP; F1-2 assumptions have already changed.

### Change 1 — expose the real active-frame predicate

In `forth_dict.h`, declare:

```c
bool forthInnerIsActive(void);

#if defined(PC_BUILD)
void forthSetTestInnerDepth(uint8_t depth);
#endif
```

In `forth_inner.c`, immediately after the private `forthDepth` declaration,
implement `forthInnerIsActive()` as `forthDepth != 0`. Under `PC_BUILD` only,
implement the setter as a direct assignment to `forthDepth`. The setter is a
focused guard canary; production code must never call it. Tests must restore
depth to zero on every exit path.

Do not export `forthDepth`, `rsp`, or the return stack. Do not change the
existing increment/decrement or `INNER_LEAVE()` protocol.

### Change 2 — pending reset replaces equality as truth

In `forth_compile.c`, keep both existing `uint16_t` counters as diagnostics and
add one private boolean initialized false:

```c
static bool forthResetPending = false;
```

Implement the exact behavior:

```c
void forthRunGenBump(void) {
  forthRunGeneration++;                 /* diagnostic only; wrapping is fine */
  if (!forthInnerIsActive()) {
    forthResetPending = true;
  }
}

static void forthRunGenCheckReset(void) {
  if (!forthResetPending || forthInnerIsActive()) {
    return;
  }
  forthDictClear();
  forthScannedCount = 0;
  forthResetGeneration = forthRunGeneration;  /* diagnostic sample only */
  forthResetPending = false;                  /* consume only after clear */
}
```

Ordering is normative. An active-frame bump must leave an already-pending event
unchanged, but must not create a new event. An active-frame check must leave the
event pending. Neither function may compare the counters for correctness.

Keep the current function names and `forthProgramStep` call site. Do not add a
second reset consumer.

### Change 3 — focused lifetime test

Add `test_pending_reset_lifetime`, registered immediately after
`test_program_step_gen_reset`. Use a real one-step program containing one Forth
source step whose payload is `"0"`; drive it through `writeTestProgram` and
`forthProgramStep(beginOfProgramMemory + 3)`. Save/restore `programRunStop`,
keep it `PGM_RUNNING` only around program-step calls, and clean the dictionary,
program fixture, errors, and test depth on every path.

The test has three independently reported subcases:

1. **16-bit wrap cannot cancel a reset.** First consume any prior event with a
   baseline program-step call. Define interactive `: WRAP 1 ;` and verify it
   exists. Call `forthRunGenBump()` exactly 65,536 times with test depth zero,
   then enter the real Forth program step. Require no error and require `WRAP`
   to be absent. This is the executable proof that counter equality is not
   truth.
2. **A nested launch does not request a generation.** Define `: KEEP 2 ;`.
   Set test depth to 1, call `forthRunGenBump()`, immediately restore depth to
   0, and enter the real program step. Require no error and require `KEEP` to
   remain visible.
3. **A pending reset waits for a safe entry.** Define `: HOLD 3 ;`, request a
   reset at depth 0, then set test depth to 1 and enter the program step.
   Require `HOLD` still visible. Restore depth to 0 and enter the same step
   again; require no error and require `HOLD` absent.

Print one PASS line per subcase. A failure path must reset test depth to zero
before returning. Do not hand-prime private pending/counter variables; the test
must use `forthRunGenBump()` and the real `forthProgramStep()` consumer.

### Existing tests and comments

`test_program_step_gen_reset` and `test_prescan_generation_rearm` encode the
surviving behavior and must remain green. Update only their nearby prose if it
claims counter inequality is authoritative; their observable assertions remain
valid. All other `forthRunGenBump()` tests remain unchanged.

### Non-goals / STOP boundaries

- No changes to `runProgram`, `fnExecute`, `fnRunProgram`, SST, or the two
  production bump sites. F1-2 owns top-level/single-step signaling.
- No dynamic scan registry; the fixed array remains until F1-3.
- No scopes, `RECURSE`, XEQN, parameter decoder, restore-body validator, or
  capture work.
- No removal of `boundedRead`.
- No DESIGN or history edits.
- No production test-depth setter.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately, using the
full sanctioned gate, manually restore the hunk, and continue only when the
focused test goes RED for the named reason:

1. In the reset consumer, temporarily replace the pending-event condition with
   the old `forthResetGeneration == forthRunGeneration` early return. Subcase 1
   must go RED because `WRAP` survives the 65,536-bump alias.
2. Temporarily remove `!forthInnerIsActive()` from `forthRunGenBump` so an
   active-frame bump requests reset. Subcase 2 must go RED because `KEEP` is
   cleared on the following safe entry.
3. Temporarily remove the active-frame guard from `forthRunGenCheckReset`.
   Subcase 3 must go RED because `HOLD` is cleared during the guarded entry.

After all mutations, grep for `MUTATION F1-1` (there must be no match), run the
full gate green again, and record:

- all three PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=... freeRamDelta=...`;
- `git diff --check`;
- byte equality between each flat file and its generated `files/` counterpart.

### Commit

After the final green gate, `git status --short` may contain only the four flat
files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only and
commit:

```text
forth-core: F1-1 — pending reset survives wrap and active frames
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the two new production functions, the pending-reset functions, the
three focused PASS lines, each mutation's RED symptom, the final gate and arena
lines, commit hash, and anything surprising.

---

## Ordered follow-ons (planning boundaries only — NOT executable)

After each predecessor commits green, the architect re-greps the tree and
authors exactly one next packet:

1. **F1-2:** move top-level reset signaling to the real `runProgram` entry,
   include every PEM single-step as a fresh lifetime, preserve nested RPN
   launches, remove superseded scattered bump semantics, and migrate the named
   generation/SST tests.
2. **F1-3:** replace `FORTH_SCAN_MAX` and `forthScannedProgs[]` with compact,
   dynamic, capacity-bounded tracking inside the managed dictionary arena;
   ordinary dictionary exhaustion is the only capacity failure and arena
   high-water reporting is mandatory.
3. **F1-4:** add compile-only immediate `RECURSE`; it emits a call to the
   current smudged definition by index and does not make that name visible.
4. **F1-5:** fully validate restored header padding, body/cell extents, token
   encodings, operands, call indices, branch targets, reserved ranges, and
   terminating EXIT. Invalid Forth state is orphan-cleared while RPN source is
   preserved. Existing `boundedRead` checks remain defense in depth. XEQN is
   still reserved until F3 and must be added to validation when F3 lands.
