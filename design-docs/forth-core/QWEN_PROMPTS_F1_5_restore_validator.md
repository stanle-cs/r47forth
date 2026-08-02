# Stage F1-5 — full restore-time threaded-code validator (bounded implementation prompt)

Origin: accepted R4 architecture (DESIGN.md §10.1: "one full validator on
restore — header/name extents, body and cell alignment, token and operand
extents, colon indices, XEQN kind/length/padding, reserved ranges, legal
branch targets, termination. Invalid → clear only the Forth dictionary,
preserve the RPN save, rebuild definitions from source. `boundedRead` stays
as defense-in-depth (RULE-1)."), `R6_RESOLUTION_PLAN.md` Step 7. XEQN does
not exist until stage F3; its encodings remain in the reserved-token reject
set here and F3 MUST extend this validator when it lands. Authored
2026-07-16 against the post-F1-4 target tree; the execution gate below
verifies the tree matches before any edit is allowed.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

F1-5 executes only after F1-4 is committed green. Verify all of the
following; if any check fails, STOP and report the mismatch — do not adapt.

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -12` contains all four F1-1..F1-4 stage commits.
3. `grep -n "forthDictValidateRestored" packages/forth-core/forth_dict.c`
   shows the current H5 validator (control-block checks + header-chain
   walk), and `grep -n "forthScanTrackReset" packages/forth-core/forth_dict.c`
   shows it called at the head of `forthDictValidateRestored` (F1-3).
4. `grep -n "PRIM_COUNT = 12" packages/forth-core/forth_prims.c` matches and
   `grep -n "RECURSE" packages/forth-core/forth_prims.c` shows the immediate
   row (F1-4).
5. `grep -n "test_validate_direct_corruption\|test_restore_validation_clamps"
   packages/forth-core/test_dict_reloc.c` shows both existing validator
   tests.
6. `grep -n "begin_word\|end_word" packages/forth-core/test_dict_reloc.c |
   head` shows the raw-entry test helpers (note: `begin_word` does NOT zero
   header padding — the production zeroing lives in `startDefinition`; this
   packet's hand-built fixtures avoid padding by using 4-glyph names).

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
   `/tmp/forth-f1-5-todo.md` (outside the repo): one item per file, function,
   test subcase, mutation, final gate, and report. Keep the file updated —
   mark each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f1-5-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the
   banners and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f1-5-gate.log | head -n 30` or a grep
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
   (`design-docs/forth-core/QWEN_PROMPTS_F1_5_restore_validator.md`), the todo
   file `/tmp/forth-f1-5-todo.md`, and `git status --short` / `git diff` are
   the ONLY durable truth about task state. If your context is compacted or
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

## F1-5 — Restored threaded code is proven valid or orphan-cleared

### Authority carried by this packet

Decided contract (no open choices):

- `forthDictValidateRestored()` (forth_dict.c) is EXTENDED in place — same
  name, same single call site in `saveRestoreBackup.c` (read-only anchor;
  do not edit), same failure action: message + `forthDictInit()` (the
  deliberate orphan — the restored allocation tables are exactly what was
  just distrusted). RPN program memory is never touched: an invalid Forth
  dictionary is dropped and definitions rebuild from source on the next
  first-touch pre-scan.
- Every existing check is retained verbatim, including the "declared
  redundant" comment block and the checks it protects. New checks run
  inside the existing chain walk, per entry, AFTER the existing name-extent
  and link checks.
- The chain walks newest→oldest with strictly decreasing offsets (already
  enforced), so the previously visited entry is the current entry's
  address-space successor. Maintain `uint16_t succOff` (init `fdict.here`,
  set to `off` at the end of each iteration): it is the exclusive upper
  bound of the current entry's bytes.
- Bytes between an entry's terminating `FTOK_EXIT` and `succOff` are NOT
  validated: block-rounding padding and F1-3 scan records legitimately live
  there and are never executed (execution starts at body starts and stops
  at EXIT; `boundedRead` remains runtime defense-in-depth).
- New per-entry rules (all normative):
  1. `hdr->flags != 0` → invalid. Finished colon entries always have flags
     0 (`forthDictAllocate` sets `FF_SMUDGE`; `forthDictFinishDef` clears
     it; no other bit is ever set). A restored smudged entry cannot be
     legitimate — `openDef` is BSS and definitions never span lines, so no
     save can contain an open definition. If a future stage adds header
     flag bits it must extend this check (staged discipline, like XEQN).
  2. Header padding bytes `[off+4+nameLen, off+alignedHdr)` must all be 0
     (`startDefinition` zeroes them at creation).
  3. `bodyStart = off + TO_BLOCKS(4 + nameLen) * BYTES_PER_BLOCK` must
     satisfy `bodyStart + 2 <= succOff` (room for at least EXIT).
  4. The token walk from `bodyStart` (grammar below) must reach a
     terminating `FTOK_EXIT` strictly within `succOff`, with every token,
     operand extent, and encoding valid.
- Token grammar (mirror of `forth_inner.c`'s consumer, tightened to what
  the compiler can produce; `entryIdx = fdict.count - 1 - n` is the current
  entry's own index):
  - `0x0000` `FTOK_EXIT` — terminates the body (the compiler emits EXIT
    exactly once, at the end; the first EXIT is the end).
  - `0x0001..0x0FFF` prim — valid iff `tok - 1 < forthPrimCount`.
  - `0x1000..0x7EFF` call — valid iff `tok - 0x1000 <= entryIdx` (calls
    reference already-defined words; equality is the F1-4 `RECURSE`
    self-call; larger indices cannot be produced and are corruption).
  - `0x7F00` `FTOK_LIT` — 16 inline bytes must fit before `succOff`.
  - `0x7F01` `FTOK_ILIT` — 4 inline bytes must fit.
  - `0x7F02` `FTOK_BR` / `0x7F03` `FTOK_0BR` — 2-byte little-endian signed
    cell delta must fit; the runtime target is
    `posAfterOperand + delta * 2` and must (a) lie in
    `[bodyStart, succOff)` and (b) be a token boundary of this body not
    beyond its EXIT — proven by the boundary sub-walk below.
  - `0x7F04` `FTOK_C47` — 2-byte itemId must fit; `itemId != 0` and
    `itemId < LAST_ITEM`; `ptp = indexOfItems[itemId].status & PTP_STATUS`
    must be `PTP_NONE`, `PTP_NUMBER_8`, or `PTP_NUMBER_16`; NUMBER_8 and
    NUMBER_16 carry one more 2-byte cell that must fit, and for NUMBER_8
    the high pad byte must be 0 (the compiler pads the 1-byte value to a
    cell with 0).
  - everything else (`0x7F05..0xFFFF`) — invalid. This is where FTOK_XEQN
    will land: **stage F3 must add its kind/len/padding validation here**.
- Restore-time only; simplicity beats speed. The boundary sub-walk makes
  validation O(body²) worst case — accepted; do not optimize.

### Files

Modify only:

- `packages/forth-core/forth_dict.c`
- `packages/forth-core/test_dict_reloc.c`

Read-only verification:

- `packages/forth-core/saveRestoreBackup.c` — grep
  `forthDictValidateRestored`; exactly one call site; do not edit.
- `packages/forth-core/forth_inner.c` — grep `FTOK_LIT\|FTOK_C47` to confirm
  the token constants mirrored below; do not edit.

### Targeted reads

1. In `forth_dict.c`, read `forthDictValidateRestored` in full, plus
   `forthDictAllocate`, `startDefinition`, `forthDictFinishDef` (creation
   invariants the rules above cite).
2. In `forth_inner.c`, grep `#define FTOK_` and read only that constant
   block (~lines 13-19).
3. In `test_dict_reloc.c`, grep
   `test_validate_direct_corruption\|test_restore_validation_clamps\|begin_word\|end_word\|T_EXIT`
   and read those two tests, the two helpers, and the `T_*` token constant
   definitions used by tests. Also grep `test_recurse_compile_only` for the
   registration anchor.

### Change 1 — the body walker (forth_dict.c)

Above `forthDictValidateRestored`, add the mirrored token constants (same
convention as the other engine files):

```c
/* ---- §2.2 token constants (mirror forth_inner.c) — F1-5 validator ---- */
#define FTOK_CALL_BASE    0x1000
#define FTOK_LIT          0x7F00
#define FTOK_ILIT         0x7F01
#define FTOK_BR           0x7F02
#define FTOK_0BR          0x7F03
#define FTOK_C47          0x7F04
```

Then the walker. Signature and behavior are normative; transcribe the
control flow exactly (little-endian reads via `memcpy`, never struct/word
casts):

```c
/* F1-5: validate one restored body, or (checkTarget != FORTH_NULL) prove
 * that checkTarget is a token boundary of this body at or before its EXIT.
 * limit is exclusive. Restore-time only; the per-branch boundary sub-walk
 * is O(body^2) and deliberately unoptimized. */
static bool vBodyWalk(uint16_t bodyStart, uint16_t limit, uint16_t entryIdx,
                      uint16_t checkTarget)
{
  uint16_t pos = bodyStart;
  for (;;) {
    if (checkTarget != FORTH_NULL && pos == checkTarget) {
      return true;
    }
    if ((uint32_t)pos + 2u > limit) {
      return false;                       /* ran out without EXIT / target */
    }
    ftoken_t tok;
    memcpy(&tok, fdict.base + pos, 2);
    pos += 2;

    if (tok == FTOK_EXIT) {
      return checkTarget == FORTH_NULL;   /* end of body; target missed */
    }
    else if (tok >= 0x0001 && tok <= 0x0FFF) {
      if ((uint16_t)(tok - 1) >= forthPrimCount) return false;
    }
    else if (tok <= 0x7EFF) {             /* 0x1000..0x7EFF: FTOK_CALL */
      if ((uint16_t)(tok - FTOK_CALL_BASE) > entryIdx) return false;
    }
    else if (tok == FTOK_LIT) {
      if ((uint32_t)pos + 16u > limit) return false;
      pos += 16;
    }
    else if (tok == FTOK_ILIT) {
      if ((uint32_t)pos + 4u > limit) return false;
      pos += 4;
    }
    else if (tok == FTOK_BR || tok == FTOK_0BR) {
      if ((uint32_t)pos + 2u > limit) return false;
      int16_t delta;
      memcpy(&delta, fdict.base + pos, 2);
      pos += 2;
      if (checkTarget == FORTH_NULL) {
        int32_t target = (int32_t)pos + (int32_t)delta * 2;
        if (target < (int32_t)bodyStart || target >= (int32_t)limit) return false;
        if (!vBodyWalk(bodyStart, limit, entryIdx, (uint16_t)target)) return false;
      }
    }
    else if (tok == FTOK_C47) {
      if ((uint32_t)pos + 2u > limit) return false;
      uint16_t itemId;
      memcpy(&itemId, fdict.base + pos, 2);
      pos += 2;
      if (itemId == 0 || itemId >= LAST_ITEM) return false;
      uint16_t ptp = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);
      if (ptp == PTP_NONE) {
        /* no inline param */
      }
      else if (ptp == PTP_NUMBER_8) {
        if ((uint32_t)pos + 2u > limit) return false;
        if (fdict.base[pos + 1] != 0) return false;   /* padded cell */
        pos += 2;
      }
      else if (ptp == PTP_NUMBER_16) {
        if ((uint32_t)pos + 2u > limit) return false;
        pos += 2;
      }
      else {
        return false;
      }
    }
    else {
      return false;   /* 0x7F05..0xFFFF reserved — F3 adds XEQN here */
    }
  }
}
```

(`FORTH_NULL` = 0xFFFF can never be a body offset — `here <= 0xFFFE` — so it
is a safe "no target" sentinel. Recursion depth is exactly 2: the sub-walk
never recurses because it skips target validation.)

### Change 2 — extend the chain walk (forth_dict.c)

In `forthDictValidateRestored`, inside the existing `if (ok)` walk:

- before the loop: `uint16_t succOff = fdict.here;`
- keep every existing check and the existing comment block untouched;
- after the existing `hdr->link` strictly-decreasing check and before
  `off = hdr->link;`, insert the per-entry F1-5 block implementing rules
  1-4 (flags, padding, bodyStart room, `vBodyWalk(bodyStart, succOff,
  (uint16_t)(fdict.count - 1 - n), FORTH_NULL)`), each failure setting
  `ok = false; break;`;
- set `succOff = off;` as the last statement of the loop body.

Add one comment line above the new block:
`/* F1-5: full threaded-code validation; bytes past EXIT (block padding, scan records) are inert and unchecked. */`

The failure tail (message + `forthDictInit()`) is unchanged.

### Change 3 — focused test

Add `test_validate_restored_bodies`, registered immediately after
`test_recurse_compile_only`. Use the T1.3b idiom throughout: build, corrupt,
call `forthDictValidateRestored()` directly, assert the outcome, release the
deliberate orphan (`preBase`/`preBlocks` captured before the call,
`freeC47Blocks` after a reset outcome), `forthDictClear()` between subcases.
Hand-built entries use `begin_word`/`end_word` with 4-glyph names ONLY
(header = 8 bytes, no padding — `begin_word` does not zero padding bytes).
Nine independently reported subcases, one PASS line each:

1. **P0 — a real mixed dictionary validates clean.** First write a one-step
   program `": PW 4 ; PW"` (payload length 11; bytes
   `0x8B 0x1A 0xFD 11 ...`) via `writeTestProgram`. After it succeeds, set
   `const uint8_t *payload = beginOfProgramMemory + 3;`, call
   `forthRunGenBump()`, and call `forthProgramStep(payload)` once under the
   `PGM_RUNNING` wrap. Require no error and `PW` found. This ordering is
   normative: the safe program-step entry consumes the F1-1 pending reset,
   compiles PW, and plants a live F1-3 scan record before any interactive
   words exist. Defining VA/VB/VC before this entry is wrong because the
   pending reset would clear them. After the program step, build interactively
   `: VA 1 ;`, `: VB VA ;`, `: VC RECURSE ;`. The record precedes PW's header
   (F1-3 record-first ordering), i.e. it sits below the oldest entry, where
   the chain walk never looks — the tolerance P0 pins is that validation
   passes with a live record among the region's bytes.
   The pointer contract is literal: offsets 0..2 are the `ITM_FORTH` opcode,
   offset 3 is the source-length byte, and offset 4 is the first `':'`;
   `forthProgramStep` must receive offset 3, never offset 4 or the first source
   character. There is no leading marker step in this fixture. Then call
   `forthDictValidateRestored()` directly: require `fdict.base != NULL`,
   `fdict.count == 4`, and all of VA/VB/VC/PW still found. (Pins: calls,
   RECURSE self-call, literals, and record bytes tolerated in gaps.)
   Capture `preBase`/`preBlocks` before validation and release the deliberate
   orphan if this supposedly valid dictionary is reset. Clean up the program
   fixture but keep going with a fresh dict.
2. **P0b — legal backward branch validates clean.** Hand-build `LOOP`
   (4 glyphs): `begin_word`, emit `FTOK_ILIT` + 4 bytes (int32 value 1),
   emit `FTOK_BR`, emit delta `(ftoken_t)(int16_t)-5` (target =
   bodyStart+10 + (-5)*2 = bodyStart, a boundary), `end_word`. Validate:
   must NOT reset. Do not execute the word. `forthDictClear()`.
3. **V-B1 — missing EXIT.** `forthOuterInterpret(": VB1 1 ;")` (body is
   exactly ILIT+4+EXIT = 8 bytes, block-tight, no trailing padding).
   Overwrite the EXIT token at `fdict.latest + 8 + 6` with token `0x0001`
   (DUP) via `memcpy`. Validate: must reset (`fdict.base == NULL`).
4. **V-B2 — call index above own index.** `: VA2 1 ;` then `: VB2 VA2 ;`;
   patch VB2's first body token (at `fdict.latest + 8`, name length 3 →
   header rounds to 8) from `0x1000` to `0x1005`. Validate: must reset.
5. **V-B3 — branch into a literal payload.** Rebuild the P0b construction
   with delta `-4` instead of `-5` (target = bodyStart+2, inside the ILIT
   operand — not a token boundary). Validate: must reset.
6. **V-B4 — reserved token.** Hand-build `RSV4`: `begin_word`, emit
   `0x7F05`, `end_word`. Validate: must reset. (The runtime twin of this
   reject already exists in the inner-interpreter tests; this one pins the
   restore-time layer.)
7. **V-B5 — restored smudge.** `: VB5 1 ;` then set `FF_SMUDGE` on
   `((forthHeader_t *)(fdict.base + fdict.latest))->flags`. Validate: must
   reset.
8. **V-B6 — nonzero header padding.** `: VB6 1 ;` (name length 3 → padding
   byte at `fdict.latest + 7`, zeroed by `startDefinition`); set it to
   `0xAA`. Validate: must reset.
9. **V-B7 — C47 item out of range.** Hand-build `ITM7`: `begin_word`, emit
   `FTOK_C47`, emit `(ftoken_t)0xFFFF`, `end_word`. Validate: must reset.

After the last subcase, rebuild-and-use once (define and find a fresh word)
to prove the dictionary is usable after the final reset, as
`test_restore_validation_clamps` does.

### Existing tests and comments

`test_restore_validation_clamps`, `test_validate_direct_corruption`, and
`test_save_restore_roundtrip` pin surviving behavior and must stay green
unchanged (their dictionaries are production-built and body-valid). Update
prose only if a nearby comment claims body bytes are unvalidated. If any
OTHER existing test reddens under the new checks, STOP and report — that is
an architect decision, not a test edit (rule 6).

### Non-goals / STOP boundaries

- No XEQN work (F3 extends the reserved-token arm).
- No change to `boundedRead` or any `forth_inner.c` runtime guard —
  defense-in-depth stays (RULE-1).
- No change to the orphan policy (`forthDictInit`, never `freeC47Blocks`,
  on the failure path) or to `saveRestoreBackup.c`.
- No save-format changes; scan records remain inert saved bytes.
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restore the hunk afterward, and continue only
when the named subcase goes RED for the named reason:

1. In `vBodyWalk`, change the top-of-loop extent check to `return true`
   (accept a body that runs out without EXIT). V-B1 must go RED.
2. Delete the call-range `> entryIdx` check. V-B2 must go RED.
3. In the BR/0BR arm, delete the boundary sub-walk call (keep the range
   check). V-B3 must go RED while P0b stays green.
4. In the final `else`, skip unknown tokens instead of rejecting
   (treat as 0-operand). V-B4 must go RED.

After all mutations, grep for `MUTATION F1-5` (there must be no match), run
the full gate green again, and record:

- all nine PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=... freeRamDelta=...`;
- `git diff --check`;
- byte equality between each flat file and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only the two
flat files above, their generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F1-5 — restored threaded code is validated or orphan-cleared
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the walker's landed grammar arms, the per-entry rule block, the nine
PASS lines, each mutation's RED symptom, the final gate and arena lines,
commit hash, and anything surprising — explicitly including any existing
test that needed attention (there should be none).
