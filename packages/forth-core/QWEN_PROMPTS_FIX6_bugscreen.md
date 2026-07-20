# Housekeeping FIX-6B — freeListFree overlap guard: fail loud, not silent-continue

Origin: upstream's response to `UPSTREAM_REPORTS_b8f79e486.md` §3 (recorded
there, 2026-07-19).  Upstream accepts the DETECTION but rejects the
survive-and-continue RESPONSE: once an overlapping/double free is detected
an earlier invariant already broke and memory is untrustworthy, so the
allocator must HALT LOUDLY (raise the firmware-bug screen, ask for a
hardware reset) rather than quietly refuse and continue.  This packet
reworks the guard to `displayBugScreen(...)` and re-pins the four affected
tests to the new contract.  It is INDEPENDENT of the F-series (no F-stage
gate); run it any time on a clean tree.

**RULE LIFT (this task only).** Every F-series packet forbids touching
`core/freeList.c` because the guard was earmarked for upstream.  THIS
packet is the sanctioned exception: it edits `packages/forth-core/core/
freeList.c` deliberately, and ONLY the guard hunk.  Do not touch anything
else in that file.  After this lands the no-touch rule resumes.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/pem-entry-fixes`; tree clean.
2. `grep -n "Double-free / invalid-free guard (FIX-6)" packages/forth-core/core/freeList.c`
   → exactly ONE match (the current guard this packet reworks).
3. `grep -n "backtrace(callstack" packages/forth-core/core/freeList.c`
   → exactly ONE match (the PC diagnostic block this packet DELETES).
4. `grep -rn "displayBugScreen" src/c47/error.h` matches (the mechanism);
   `grep -n '#include "c47.h"' packages/forth-core/core/freeList.c`
   matches, and `grep -n '#include "error.h"' src/c47/c47.h` matches —
   so `displayBugScreen` is reachable from freeList.c with NO new include.
   If either grep misses, STOP (do not add an include speculatively —
   report the reachability gap).
5. `grep -n "extern uint8_t  *calcMode;" src/c47/c47.h` matches
   (`calcMode` is `uint8_t`; `CM_BUG_ON_SCREEN` is
   `#define CM_BUG_ON_SCREEN 10`, `grep -n "define CM_BUG_ON_SCREEN"
   packages/forth-core/defines.h`).
6. All four target tests exist:
   `grep -n "static int test_freelist_double_free_guarded(void)$\|static int test_freelist_interior_double_free(void)$\|static int test_freelist_no_mutation_on_oversize_free(void)$\|static int test_program_memory_no_overlap(void)$" packages/forth-core/test_dict_reloc.c`
   → four matches.
7. Pre-gate green; arena baseline from the current HEAD commit message.

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
   `/tmp/forth-fix6b-todo.md` (outside the repo): one item per file, test,
   mutation, final gate, and report.  Keep it updated; mark each item in
   progress and completed as you work, and append `MUTATION APPLIED: <n>` /
   `MUTATION RESTORED: <n>` immediately.  Do not report success with an open
   item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-fix6b-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the two files named by this packet under `packages/forth-core/`:
   `core/freeList.c` (the guard hunk ONLY — see RULE LIFT above) and
   `test_dict_reloc.c`.  Never edit `src/`, generated `patches/`, or generated
   `files/`; the gate refreshes the generated package view.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read
   `test_dict_reloc.c` in full.  Grep the named anchors and read only the
   specified local slices.  Read `core/freeList.c` only around the guard.
6. Do not change a test unless this task names it.  If another test reddens,
   STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-fix6b-todo.md`,
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

---

## FIX-6B — an overlapping free raises the firmware-bug screen

### Authority carried by this packet (no open choices)

1. **The contract change.**  The DETECTION scan stays exactly as-is
   (unconditional, device + PC).  The RESPONSE changes: instead of a
   `#if !defined(DMCP_BUILD)` print block followed by `return;` with the
   list intentionally unmutated, the guard raises upstream's own
   internal-fault screen and returns.  The list is STILL not mutated (the
   `return` precedes all merge/insert logic), so every existing
   "list unchanged" assertion stays valid; the NEW behavior is the raised
   bug screen.
2. **`displayBugScreen` is non-blocking** (verified: it draws to the LCD
   buffer, sets `previousCalcMode = calcMode; calcMode = CM_BUG_ON_SCREEN`,
   and RETURNS — the EXIT-to-leave handling lives in the keyboard handler,
   not in this call).  Safe to call from the headless self-test.
3. **Message.**  `displayBugScreen` prepends "This is most likely a bug in
   the firmware!" and appends " Try to reproduce this and report a bug.
   Press EXIT to leave."  Pass ONLY the middle sentence.

### Files

Modify only: `packages/forth-core/core/freeList.c` (guard hunk only),
`packages/forth-core/test_dict_reloc.c`.

### Targeted reads

1. freeList.c: the guard block only (grep `Double-free / invalid-free
   guard (FIX-6)`, read from that comment through the closing `}` of the
   `for` loop — about 32 lines).
2. test_dict_reloc.c: the four named tests (grep each signature, read each
   body); the `test_cleanup_no_overlap` comment header (grep, read the
   4-line comment above it).

### Change A — rework the guard (freeList.c)

Replace the ENTIRE guard block — from the `// Double-free / invalid-free
guard (FIX-6):` comment through the `for` loop's closing brace — with:

```c
  // Double-free / invalid-free guard (FIX-6B): reject any free whose range
  // overlaps an existing free region. Runs unconditionally (device + PC).
  // An overlap means an earlier invariant already broke and memory is no
  // longer trustworthy; per upstream doctrine we HALT rather than continue.
  // The list is left unmutated (return precedes all insert/merge logic) and
  // the firmware-bug screen is raised so the user can hardware-reset and
  // report. displayBugScreen is upstream's own internal-fault mechanism and
  // is non-blocking (draws + sets calcMode, returns).
  for(i=0; i<numberOfFreeMemoryRegions; i++) {
    uint32_t rStart = (uint32_t)freeMemoryRegions[i].blockAddress;
    uint32_t rEnd   = rStart + (uint32_t)freeMemoryRegions[i].sizeInBlocks;
    if((uint32_t)C47RamPtr < rEnd && (uint32_t)C47RamPtr + (uint32_t)sizeInBlocks > rStart) {
      displayBugScreen("Memory management fault: an overlapping or double free was detected.");
      return;
    }
  }
```

This deletes the `#if !defined(DMCP_BUILD)` wrapper, `errorf`, the
`fprintf`, and the `backtrace`/`backtrace_symbols` block.  Do NOT remove
any `#include` from the file even if the backtrace removal leaves one
unused — leave includes untouched (they are shared with the rest of the
allocator or pulled via c47.h; an unused include is harmless and removing
one risks an unrelated break).  Report any `-Wunused` include warning
rather than acting on it.

### Change B — the three double-free tests now assert the bug screen

Each of `test_freelist_double_free_guarded`,
`test_freelist_interior_double_free`, and
`test_freelist_no_mutation_on_oversize_free` performs the TRIGGERING
double free (the second/interior/oversize `freeC47Blocks` call whose
adjacent comment says "must be a no-op").  For EACH test, apply the same
three edits around that specific call:

1. Immediately BEFORE the triggering `freeC47Blocks(...)`, insert:

   ```c
   uint8_t savedCalcMode = calcMode;
   calcMode = CM_NORMAL;   /* ensure displayBugScreen's guard can fire */
   ```

   (If a `savedCalcMode` name collides in that scope, suffix it — but the
   scopes are per-function, so no collision is expected.)

2. Immediately AFTER the triggering call, ADD a new assertion alongside the
   existing list-unchanged checks (do not remove the existing checks — the
   list must still be unchanged):

   ```c
   if (calcMode != CM_BUG_ON_SCREEN) {
     printf("    FAIL: overlapping free did not raise the firmware-bug screen\n");
     fail = 1;
   }
   ```

3. Immediately AFTER that assertion (before the `test_freelist_consistent`
   call, so consistency runs in a restored mode), restore and clear:

   ```c
   calcMode = savedCalcMode;
   clearScreen(0);   /* wipe the bug-screen pixels so later display tests
                        start clean; harmless if unused elsewhere */
   ```

   If `clearScreen` takes different/no arguments in this tree, match its
   real signature (grep `void clearScreen`); if it is not trivially
   callable here, drop the `clearScreen` line and report — the calcMode
   restore alone is the correctness-critical part.

Update each test's trailing `PASS:` line to name the new contract, e.g.
`PASS: exact-match double free raises bug screen, free list unchanged`
(mirror the wording for interior/oversize).  Do NOT change the escaping-
mutation comment blocks — those still describe the DETECTION escape and
remain valid.

### Change C — overlap tests' mutation comments (comment-only)

`test_program_memory_no_overlap` and `test_cleanup_no_overlap` bodies are
UNCHANGED (their normal runs do not trigger the guard — they use the
proper API and assert ordering).  Only their comment headers describe a
mutation that "trigger[s] the overlap warning in freeListFree()".  Edit
just those comment phrases to read "trigger the firmware-bug screen in
freeListFree() (FIX-6B)".  No code change in these two tests.

### Existing tests

Every other test stays green untouched.  If any test other than the four
named reddens, STOP — the likely cause is a stray bug-screen calcMode not
restored (re-check the Change B restores) or a `clearScreen` signature
mismatch.

### Gate and required mutations

Full gate green first (the three reworked double-free PASS lines + every
other banner).  Mutations, each separately, logs
`/tmp/forth-fix6b-mut1..3.log`; post-restore gate green between mutations.

1. **NEW (the rework itself).**  In Change A, delete the
   `displayBugScreen(...)` line (keep the `return;`).
   `test_freelist_double_free_guarded` MUST go RED on the new
   `calcMode != CM_BUG_ON_SCREEN` assertion (detection still refuses the
   free, but no screen is raised).
2. **DETECTION still pinned (exact-match).**  Revert the guard's overlap
   condition to the OLD exact-match form
   `freeMemoryRegions[i].blockAddress == C47RamPtr` (drop the range
   arithmetic).  `test_freelist_interior_double_free` MUST go RED (the
   interior double free is no longer caught: the list mutates AND no
   screen is raised — either the list-unchanged or the calcMode assertion
   fires).
3. **DETECTION still pinned (oversize).**  Below the guard, re-add a
   size-grow branch on partial match (the escape described in
   `test_freelist_no_mutation_on_oversize_free`'s comment).  That test
   MUST go RED (a region grows).

Report: three reworked PASS lines, both banners, exit 0, arena line vs
baseline, `git diff --check`, generated-mirror equality, three mutation
REDs.  RULE-1: flash delta is a NET REDUCTION (the backtrace block is
deleted); record the measured `make dmcp5r47` delta — a negative number
is expected and fine.

### Commit

```text
forth-core: FIX-6B — overlapping free raises the firmware-bug screen (halt, not continue)
```

---

## REGRESSION + ENTRY-POINT RULES (binding, added 2026-07-19 after the F5-2 debug)

These cost a full session at F5-2, where a correct six-line change was
blamed for four red tests it never touched. They apply to this packet
whether or not the body above repeats them.

1. **A red outside your diff is an immediate STOP — zero repair attempts.**
   The two-attempt allowance applies only to code or tests THIS packet
   authored. If a test you did not write reddens, stop and report
   `[SOL DEBUGGER HANDOFF]` at once. Do not first try to decide whether your
   change could have caused it: "my change cannot have caused this" is the
   most common wrong conclusion, and deciding it is the debugger's job.
2. **Name the blast radius by diffing PASS sets, not by reading failures.**
   Keep the pre-gate log. Then:
   `diff <(grep -o "PASS: .*" /tmp/<pre>.log | sort) <(grep -o "PASS: .*" /tmp/<gate>.log | sort)`
   Newly-missing PASS lines in untouched tests are the report.
3. **Entry-point contract pre-flight.** Before wiring an existing function
   into a new call site, prove it saves/restores the process-global state its
   siblings do — grep the other entry points for the fields the shared
   epilogue restores and compare. A function correct in isolation can be
   wrong the moment a second caller exists. A mismatch is a packet defect:
   STOP and report, do not patch around it.
4. **Pin the contract, not just the verdict.** If this packet adds an entry
   point whose spec claims state neutrality ("mutates no live state",
   "restores the mode", "leaves the buffer untouched"), pin that claim
   directly: set the state to a NON-default value, drive both the accepting
   and the rejecting path, assert the state came back. See
   `test_check_source_line` subcase 6 and `poisonAutoFrame()` for the landed
   shape — the poison makes an uninitialized restore deterministic instead of
   luck-of-the-stack.
