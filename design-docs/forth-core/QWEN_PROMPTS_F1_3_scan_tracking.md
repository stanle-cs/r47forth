# Stage F1-3 — dynamic arena-backed pre-scan tracking (bounded implementation prompt)

Origin: R4-E1 accepted ruling ("Replace the fixed array with compact dynamic
tracking in the managed dictionary arena. Capacity failure is ordinary
dictionary exhaustion, never an arbitrary program-count cliff."), DESIGN.md
§10.1, `R6_RESOLUTION_PLAN.md` Step 7. Authored 2026-07-16 against the
post-F1-2 target tree; the execution gate below verifies the tree matches
before any edit is allowed.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

F1-3 executes only after F1-2 is committed green. Verify all of the
following; if any check fails, STOP and report the mismatch — do not adapt.

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -8` contains commits whose subjects are
   `forth-core: F1-1 — pending reset survives wrap and active frames` and
   `forth-core: F1-2 — runProgram entry is the sole top-level lifetime signal`.
3. `grep -c forthRunGenBump packages/forth-core/programming/lblGtoXeq.c`
   prints `1` (the centralized `runProgram` site).
4. `grep -n "FORTH_SCAN_MAX\|forthScannedProgs\|forthScannedCount"
   packages/forth-core/forth_compile.c` shows the fixed 8-slot array, the
   count, the linear check inside `forthPreScanOwningProgram`, the append at
   its end, and the `forthScannedCount = 0;` line inside
   `forthRunGenCheckReset`. These identifiers must appear in NO other file
   (`grep -rn` over `packages/forth-core/*.c *.h` excluding comments in
   `test_dict_reloc.c` — if a test references them by code, STOP).
5. `grep -n "test_run_entry_lifetime_signaling\|test_pending_reset_lifetime"
   packages/forth-core/test_dict_reloc.c` shows both tests registered.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, or identifier does not
match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f1-3-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f1-3-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f1-3-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
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
9. Small-context recovery. This packet on disk
   (`design-docs/forth-core/QWEN_PROMPTS_F1_3_scan_tracking.md`), the todo file
   `/tmp/forth-f1-3-todo.md`, and `git status --short` / `git diff` are the
   ONLY durable truth about task state. If your context is compacted or
   summarized, or you are unsure what you have already done: STOP the
   current step, re-read this packet file and the todo file, run
   `git status --short` and `git diff --stat`, and check for an unrestored
   `MUTATION APPLIED` marker before doing anything else. Never reconstruct
   packet text, anchors, or code blocks from memory — re-read them from this
   file every time you need them.

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

## F1-3 — Scan tracking lives in the dictionary region, capacity-bounded by it

### Authority carried by this packet

Decided architecture (R4-E1 ruling; no open choices):

- The fixed `forthScannedProgs[8]` array and `forthScannedCount` are
  deleted. First-touch tracking becomes a linked list of **8-byte records
  stored inside the dictionary region itself**, appended through the normal
  emit path. Layout, little-endian, written/read with `memcpy` only (never a
  struct cast — alignment is not guaranteed to matter but unaligned casts
  are banned):

  `[uint32_t progOffset][uint16_t prevOff][uint16_t zero]`

  - `progOffset` = `(uint32_t)(progStart - beginOfProgramMemory)` — program
    identity is its offset in program memory, valid for comparison within
    one lifetime (programs never move during a run).
  - `prevOff` = region-relative offset of the previous record, or
    `FORTH_NULL`.
  - The trailing 2 bytes are written as 0 and never read (keeps records at
    2 blocks so block alignment of `fdict.here` is preserved).
- One BSS head: `static uint16_t forthScanHead = FORTH_NULL;` — offset of
  the newest record. Records die with the region: the head is reset wherever
  the region is dropped or replaced (exact call sites below).
- **Record-first ordering (normative).** The record is appended *before*
  the program's steps are scanned, inside the same rollback snapshot the
  pre-scan already keeps. Consequences, all intended:
  - If the record append fails, that is ordinary dictionary exhaustion: the
    step errors with `ERROR_RAM_FULL` (if nothing else was raised) and the
    program stays unrecorded. There is no silent recompile-forever mode and
    no program-count cliff.
  - If the scan itself errors, the existing three-scalar rollback also
    restores the head, so the record is trimmed with everything else and the
    program stays unrecorded (a later touch re-scans) — same contract the
    tests already pin.
- Defensive walk guards (self-heal on corruption) are belt-and-suspenders,
  documented as such in code, and are NOT mutation-tested (mirroring the
  "declared redundant" annotation convention in `forthDictValidateRestored`).
- Records occupy bytes between dictionary entries. Nothing walks the region
  linearly (lookups follow the header chain), so word lookup, indexing, and
  execution are unaffected. The suite's arena numbers (`here`) will grow by
  8 bytes per recorded program — expected; record the new arena line.

### Files

Modify only:

- `packages/forth-core/forth_compile.c`
- `packages/forth-core/forth_dict.h`
- `packages/forth-core/forth_dict.c`
- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. In `forth_compile.c`, grep
   `forthScannedProgs\|forthScannedCount\|FORTH_SCAN_MAX\|forthRunGenCheckReset\|forthPreScanOwningProgram`
   and read the generation/scan block (~lines 40-80 post-F1-1) and the whole
   of `forthPreScanOwningProgram` plus `forthProgramStep` (~515-600). No
   other slices.
2. In `forth_dict.h`, read the API block around `forthDictClear` /
   `forthDictValidateRestored` prototypes and the `forthRunGenBump`
   prototype block.
3. In `forth_dict.c`, grep
   `forthDictInit\|forthDictClear\|forthDictValidateRestored\|forthDictEmitBytes`
   and read those four functions only.
4. In `test_dict_reloc.c`, grep
   `test_prescan_two_programs_first_touch\|test_prescan_error_rolls_back_prior_defs\|test_prescan_generation_rearm\|cleanupTestProgram\|writeTestProgram`
   and read only those functions/helpers and their registration lines.

### Change 1 — the record store (forth_compile.c)

Delete these three declarations and every use of them:

```c
#define FORTH_SCAN_MAX 8
static const uint8_t *forthScannedProgs[FORTH_SCAN_MAX];
static uint8_t        forthScannedCount = 0;
```

In their place (same position in the file), add exactly:

```c
/* §9.2 first-touch pre-scan tracking — F1-3 (R4-E1): dynamic records inside
 * the dictionary region. One 8-byte record per scanned program:
 * [uint32 progOffset][uint16 prevOff][uint16 zero], newest at forthScanHead.
 * Records die with the region (clear/init/restore reset the head); capacity
 * failure is ordinary dictionary exhaustion. The two walk guards below are
 * defense-in-depth for a dangling head — declared redundant on every
 * production path (a generation seam precedes every query); do not remove
 * without re-running the mutation analysis. */
static uint16_t forthScanHead = FORTH_NULL;

void forthScanTrackReset(void) {
  forthScanHead = FORTH_NULL;
}

static bool forthScanIsRecorded(const uint8_t *progStart) {
  if (!fdict.base) {
    forthScanHead = FORTH_NULL;
    return false;
  }
  uint32_t key = (uint32_t)(progStart - beginOfProgramMemory);
  uint16_t off = forthScanHead;
  while (off != FORTH_NULL) {
    if ((uint32_t)off + 8u > fdict.here) {   /* dangling head: self-heal */
      forthScanHead = FORTH_NULL;
      return false;
    }
    uint32_t recKey;
    uint16_t prev;
    memcpy(&recKey, fdict.base + off, 4);
    memcpy(&prev, fdict.base + off + 4, 2);
    if (recKey == key) {
      return true;
    }
    if (prev != FORTH_NULL && prev >= off) { /* chain must strictly decrease */
      forthScanHead = FORTH_NULL;
      return false;
    }
    off = prev;
  }
  return false;
}

static bool forthScanRecord(const uint8_t *progStart) {
  uint8_t rec[8];
  uint32_t key = (uint32_t)(progStart - beginOfProgramMemory);
  uint16_t newOff = fdict.here;
  memcpy(rec, &key, 4);
  memcpy(rec + 4, &forthScanHead, 2);
  rec[6] = 0;
  rec[7] = 0;
  if (!forthDictEmitBytes(rec, 8)) {
    return false;
  }
  forthScanHead = newOff;
  return true;
}
```

In `forthRunGenCheckReset`, delete the `forthScannedCount = 0;` line and do
NOT add a replacement there — `forthDictClear()` now resets the head (Change
3). The rest of the F1-1 function body stays byte-identical.

### Change 2 — rewrite `forthPreScanOwningProgram` (forth_compile.c)

Keep the function comment's first paragraph (D-2 contract) and the R4-4
rollback commentary, updating only sentences that name the fixed array.
Delete the trailing "List full: program scanned but unrecorded" comment
block entirely. New body, exactly this control flow:

```c
static void forthPreScanOwningProgram(const uint8_t *anyPtrInProgram)
{
  uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
  if (!progStart) {
    return;
  }
  if (forthScanIsRecorded(progStart)) {
    return;   /* first touch already done this generation */
  }

  /* Snapshot for rollback (R4-4 policy unchanged: base/sizeBlocks are
   * deliberately NOT restored). The record participates in the snapshot:
   * appended first, trimmed with everything else if the scan errors. */
  uint16_t scanHere   = fdict.here;
  uint16_t scanLatest = fdict.latest;
  uint16_t scanCount  = fdict.count;
  uint16_t scanHead   = forthScanHead;

  if (!forthScanRecord(progStart)) {
    /* Ordinary dictionary exhaustion (R4-E1): surface it, halt the step. */
    if (lastErrorCode == ERROR_NONE) {
      displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    }
    fdict.here   = scanHere;
    fdict.latest = scanLatest;
    fdict.count  = scanCount;
    forthScanHead = scanHead;
    return;
  }

  uint8_t *nextStart = forthNextProgramStart(progStart);
  forthOuterCtx_t ctx;
  uint8_t *step = progStart;
  while (step && (nextStart == NULL || step < nextStart)) {
    uint8_t len;
    if (forthStepPayload(step, &len) && len > 0) {   /* markers (len==0) skipped */
      xcopy(ctx.source, step + 4, len);
      ctx.source[len] = 0;
      forthOuterRun(&ctx, FORTH_OUTER_DEFS_ONLY);
      if (lastErrorCode != ERROR_NONE) {
        /* Roll back this pre-scan's definitions AND its record. */
        fdict.here   = scanHere;
        fdict.latest = scanLatest;
        fdict.count  = scanCount;
        forthScanHead = scanHead;
        return;
      }
    }
    uint8_t *next = findNextStep(step);
    if (!next || next <= step) {
      break;      /* defensive, mirrors forthMarkerTurnsOn */
    }
    step = next;
  }
  /* Success: the record is already in place. */
}
```

`forthProgramStep` is unchanged.

### Change 3 — head resets at region seams (forth_dict.h / forth_dict.c)

In `forth_dict.h`, next to the `forthRunGenBump` prototype, declare:

```c
/* F1-3: drop all first-touch scan records (state lives in forth_compile.c). */
void forthScanTrackReset(void);
```

In `forth_dict.c`:

- `forthDictInit()`: add `forthScanTrackReset();` as the last statement.
- `forthDictClear()`: add `forthScanTrackReset();` as the last statement.
- `forthDictValidateRestored()`: add `forthScanTrackReset();` as the FIRST
  statement, with the comment
  `/* F1-3: a restore is a lifetime seam — records never survive it. */`
  (a restored region's bytes replaced whatever the head pointed at, on both
  the accept and reject paths).

### Change 4 — test-helper hygiene (test_dict_reloc.c)

In `cleanupTestProgram`, immediately after the block that zeroes the `fdict`
fields, add `forthScanTrackReset();` (the helper drops the region without
`forthDictClear`, so it must drop the records too).

### Change 5 — focused test

Add `test_scan_dynamic_no_cliff`, registered immediately after
`test_prescan_two_programs_first_touch`'s registration lines. It is the
executable successor of R4-E1's nine-program probe.

Build nine programs in one buffer (model:
`test_prescan_two_programs_first_touch`; `0x85 0xB2` is the END separator):

```c
  uint8_t prog[169];   /* 9 steps x 17 bytes + 8 separators x 2 bytes */
  uint16_t p = 0;
  for (int i = 1; i <= 9; i++) {
    char src[16];
    int len = snprintf(src, sizeof(src), ": P%dW %d ; P%dW", i, i, i);  /* 13 */
    prog[p++] = 0x8B; prog[p++] = 0x1A; prog[p++] = 0xFD;
    prog[p++] = (uint8_t)len;
    memcpy(prog + p, src, (size_t)len);
    p += (uint16_t)len;
    if (i != 9) { prog[p++] = 0x85; prog[p++] = 0xB2; }
  }
  /* p == 169; program i's step starts at (i-1)*19, payload at +3 */
```

Pointer contract (do not reinterpret the offsets): for program `i`,
`stepStart = beginOfProgramMemory + (i-1)*19`; `stepStart[0..2]` is the
`ITM_FORTH` opcode, `stepStart[3]` is the source-length byte, and
`stepStart[4]` is the first source character. `forthProgramStep` takes
`stepStart + 3`, the length byte. It must never receive `stepStart + 4` or the
first source character.

`writeTestProgram(prog, sizeof(prog))`; require `numberOfPrograms >= 9`
(FAIL, clean up, return 1 otherwise — same policy as the two-programs test).
Then, with the usual fixture discipline (save/restore `programRunStop`,
`PGM_RUNNING` only around `forthProgramStep` calls, `lastErrorCode`
pre-cleared, `forthDictClear()` + `cleanupTestProgram()` on every path):

1. `forthRunGenBump()` once, then touch programs 1 through 9 in order via
   `forthProgramStep(beginOfProgramMemory + (i-1)*19 + 3)`. After each touch
   require no error, `x_is_longint(i)`, and `fdict.count == i`. (The old
   array died at 8: program 9 was the cliff.)
2. Record `hereAfter = fdict.here`. Re-touch program 1 and program 9 (same
   generation). After each require no error, the correct X value, AND
   `fdict.count == 9` AND `fdict.here == hereAfter` — no re-scan, no
   recompile, regardless of how many programs this run touched.

Print one PASS line naming both properties (nine recorded + re-touch
stable).

### Existing tests and comments

`test_prescan_two_programs_first_touch`,
`test_prescan_error_rolls_back_prior_defs`, `test_prescan_error_halts`,
`test_prescan_generation_rearm`, `test_program_step_gen_reset`, and the
F1-1/F1-2 lifetime tests encode surviving behavior and must stay green
unchanged — their assertions are all relative (count/here deltas), which the
record scheme preserves. If a gate failure shows any test pinning an
ABSOLUTE `fdict.here` or arena value, STOP and report (architect decision
needed); do not adjust the number in place.

### Non-goals / STOP boundaries

- No changes to `forthProgramStep`, `forthRunGenBump`, the F1-1 pending
  logic, or `lblGtoXeq.c`.
- No persistence-format work: records are saved with the region as inert
  bytes and are invalidated on restore by Change 3; the F1-5 validator (not
  this task) is what must tolerate them later.
- No `RECURSE`, no validator work, no DESIGN or history edits.
- Do not "optimize" the walk or pack records differently; layout is
  normative.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue only
when the named tests go RED for the named reason:

1. Make `forthScanIsRecorded` return `false` unconditionally (keep the
   body compiled but short-circuit at the top). RED:
   `test_scan_dynamic_no_cliff` (re-touch grows `fdict.count`/`here`) and
   `test_prescan_two_programs_first_touch` (third touch recompiles —
   count 3, here grew).
2. In `forthScanRecord`, store `key = 0` for every program (replace the
   subtraction with a constant). RED: `test_scan_dynamic_no_cliff` — the
   program-9 re-touch no longer matches its record and recompiles
   (`fdict.count` reads 10); program 1 (offset 0) still matches, which is
   exactly why the constant-key defect is caught only by a multi-program
   test.
3. In the scan-error rollback inside `forthPreScanOwningProgram`, delete the
   four restore lines (`here/latest/count/head`) of the DEFS_ONLY error
   branch. RED: `test_prescan_error_rolls_back_prior_defs` (count/here not
   restored after a failed scan).

After all mutations, grep for `MUTATION F1-3` (there must be no match), and
`grep -rn "FORTH_SCAN_MAX\|forthScannedProgs\|forthScannedCount" packages/forth-core/`
must return only prose hits in `.md` files (none in `.c`/`.h`). Run the full
gate green again and record:

- the focused PASS line plus the existing prescan/lifetime PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=... freeRamDelta=...` — this task
  is REQUIRED to report the arena high-water movement vs. the pre-task run
  (R4-E1 ruling); run the gate once before editing to capture the baseline
  number in your todo list;
- `git diff --check`;
- byte equality between each flat file and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only the four
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F1-3 — pre-scan tracking is dynamic, arena-backed, cliff-free
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the record layout as landed, the three seam call sites of
`forthScanTrackReset`, the focused PASS line, each mutation's RED symptom,
the before/after arena lines (high-water delta), the final gate lines,
commit hash, and anything surprising.
