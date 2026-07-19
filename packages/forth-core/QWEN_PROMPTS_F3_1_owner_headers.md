# Stage F3-1 — owner-tagged dictionary headers (bounded implementation prompt)

Origin: DESIGN §10.3 via the F3 design pass (`QWEN_PROMPTS_F3_core.md` D1).
This packet grows the dictionary header prefix from 4 to 6 bytes by adding a
16-bit `owner` field, moves header-padding zeroing into `forthDictAllocate`,
and migrates the machine-enumerated test arithmetic. Behavior is otherwise
IDENTICAL: every owner is written `FORTH_OWNER_INTERACTIVE`, no lookup
filtering exists yet (F3-3), no second region exists yet (F3-2).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes`;
   `git status --short` empty; `git log --oneline -1` shows `b5d794df4`
   (`F2-5 — close parameter acceptance review findings`).
2. `grep -n "fixed prefix = 4 bytes" packages/forth-core/forth_dict.h`
   matches exactly one line, and `grep -n "uint16_t owner"
   packages/forth-core/forth_dict.h` matches nothing.
3. `grep -n "hdrSize = 4" packages/forth-core/forth_dict.c
   packages/forth-core/forth_inner.c` matches exactly THREE lines: one in
   `forthDictAllocate` (`4u + nameLen`), one in `forthDictValidateRestored`,
   one in `bodyOffsetOfIndex`. (`startDefinition`'s own `4 + (uint16_t)nameLen`
   is a fourth, differently-spelled site — verify it exists with
   `grep -n "4 + (uint16_t)nameLen" packages/forth-core/forth_dict.c`,
   exactly two matches, both inside `startDefinition`.)
4. `grep -n "latest + 8\|latest + 7\|latest + 4;\|TO_BLOCKS(4 + 5)"
   packages/forth-core/test_dict_reloc.c` matches exactly the five code
   lines 4202, 4445, 4473, 4568, 10055 (line numbers may drift a few lines;
   the EXPRESSIONS and their count are the gate).
5. `./packages/forth-core/build-test.sh` is green before you start (run it,
   capture to `/tmp/forth-f3-1-pre.log`, verify both banners + exit 0), and
   record the `FORTH ARENA` line as the OLD baseline.

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
   `/tmp/forth-f3-1-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-1-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f3-1-todo.md`,
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

## F3-1 — the header grows an owner field; padding becomes Allocate's job

### Authority carried by this packet (no open choices)

1. `forthHeader_t` fixed prefix becomes 6 bytes:
   `uint16_t link; uint8_t flags; uint8_t nameLen; uint16_t owner;`.
   `bodyStart(entry) = ceil4(6 + nameLen)`.  New constants in forth_dict.h,
   placed with the FF_* group:

   ```c
   #define FORTH_OWNER_INTERACTIVE 0xFFFFu
   #define FORTH_OWNER_GLOBAL      0xFFFEu
   ```

   (`FORTH_OWNER_GLOBAL` is declared now, used from F3-2 on.)
2. `forthDictAllocate` writes `hdr->owner = FORTH_OWNER_INTERACTIVE;` and
   ZEROES the 0..3 header pad bytes (`off + 6 + nameLen` up to
   `off + alignedHdr`).  `startDefinition`'s own pad-zero loop is DELETED —
   padding is zeroed in exactly one place from now on.  This retires the
   "begin_word doesn't zero header padding" fixture trap for good.
3. The restore validator additionally requires `hdr->owner ==
   FORTH_OWNER_INTERACTIVE` on every entry.  A pre-F3-1 backup (4-byte
   headers) misparses under the new walk and resets to empty — that is the
   intended, automatic old-format rejection; no version field exists.
4. Everything else is byte-behavior-identical.  No lookup filtering, no new
   region, no API signature changes.

### Files

Modify only:

- `packages/forth-core/forth_dict.h`
- `packages/forth-core/forth_dict.c`
- `packages/forth-core/forth_inner.c`
- `packages/forth-core/test_dict_reloc.c`

### Targeted reads

1. In `forth_dict.h`: the `forthHeader_t` struct block and the FF_* defines.
2. In `forth_dict.c`: grep `hdrSize = 4\|4 + (uint16_t)nameLen\|off + 4\|hdrOff + 4\|entryOff + 4`
   and read 5 lines around each hit; separately read `forthDictAllocate`,
   `startDefinition`, and `forthDictValidateRestored` in full (they are
   short).
3. In `forth_inner.c`: grep `bodyOffsetOfIndex` and read only that function.
4. In `test_dict_reloc.c`: read ONLY the five slices this packet migrates
   (grep the exact expressions quoted in Change 4) plus `begin_word`.

### Change 1 — struct + constants (forth_dict.h)

In the `forthHeader_t` struct, add `uint16_t owner;` after `nameLen`, update
the layout comment ("fixed prefix = 6 bytes", body/pad offsets from 6), and
add the two FORTH_OWNER_* defines after `FF_RESERVED`.

### Change 2 — prefix arithmetic (forth_dict.c, forth_inner.c)

This is a mechanical 4→6 sweep over a CLOSED inventory.  Re-run the gate
greps from EXECUTION GATE items 3 first; a count mismatch is a STOP.

1. `forthDictAllocate`: `hdrSize = 4u + nameLen` → `6u + nameLen`.  After the
   existing header-field writes add, inside the same `if (fdict.base)` block:

   ```c
   hdr->owner = FORTH_OWNER_INTERACTIVE;
   {
     uint32_t padFrom = (uint32_t)off + 6u + nameLen;
     for (uint32_t i = padFrom; i < (uint32_t)off + alignedHdr; i++) {
       fdict.base[i] = 0;
     }
   }
   ```

2. `startDefinition`: `hdrSize = 4 + (uint16_t)nameLen` → `6 + ...`, and
   DELETE its pad-zero loop (`for (i = off + 4 + (uint16_t)nameLen; ...)`)
   together with the braces that exist only for it; keep the `bodyOff`
   computation if still used, else drop the whole block.
3. `forthDictWriteName`: `hdrOff + 4` → `hdrOff + 6`.
4. `forthFindColon`: `off + 4` → `off + 6` in the name memcmp.
5. `forthDictNameByIndex`: `off + 4` → `off + 6` in the name memcpy.
6. `openDefinitionName`: `entryOff + 4` → `entryOff + 6`.
7. `forthDictValidateRestored`: the header-fits check `off + 4` → `off + 6`;
   the name-fits check `off + 4u + hdr->nameLen` → `off + 6u + ...`; the pad
   walk start `off + 4 + hdr->nameLen` → `off + 6 + ...`; `hdrSize = 4 +
   hdr->nameLen` → `6 + ...`; and IMMEDIATELY after the existing
   `hdr->flags != 0` check add:

   ```c
   if (hdr->owner != FORTH_OWNER_INTERACTIVE) { ok = false; break; }
   ```

8. `forth_inner.c` `bodyOffsetOfIndex`: `hdrSize = 4 + hdr->nameLen` →
   `6 + ...`.

After the sweep, `grep -n "hdrSize = 4\|+ 4u + hdr\|off + 4\b\|hdrOff + 4\|entryOff + 4"
packages/forth-core/forth_dict.c packages/forth-core/forth_inner.c` must
return ZERO matches.  A leftover match is a STOP.

### Change 3 — two new direct pins (test_dict_reloc.c)

Register no new test function; extend the EXISTING validator direct-pin test
(the one containing the `V-B6` padding subcase — grep `V-B6` and work in
that function) with two subcases after V-B6, following its local style:

**V-B7 (owner pin).** Compile `: VB7 1 ;` via `forthOuterInterpret`, then set
`((forthHeader_t *)(fdict.base + fdict.latest))->owner = 0x1234;`, run
`forthDictValidateRestored()`, and require the reset outcome exactly as V-B6
does (base NULL path + the same free bookkeeping).  PASS line:

`PASS V-B7: foreign owner detected`

**V-B8 (Allocate zeroes padding).** Starting from a clean dict
(`forthDictClear(); lastErrorCode = ERROR_NONE;`), compile `: AAAA 1 ;` so
the region exists.  Predict the NEXT entry's pad location: with a 3-byte
name, pads sit at `fdict.here + 6 + 3 .. fdict.here + 11`.  Verify the poke
target lies inside the allocated region
(`fdict.here + 12 <= fdict.sizeBlocks * BYTES_PER_BLOCK` — if not, FAIL the
subcase as CONFIG).  Poke `fdict.base[fdict.here + 9] = 0xAA;` (the first pad
byte of the upcoming entry), then compile `: BBB 2 ;` (3-byte name,
`printf '%s' "BBB" | wc -c` = 3) and require
`fdict.base[fdict.latest + 9] == 0 && fdict.base[fdict.latest + 10] == 0 &&
fdict.base[fdict.latest + 11] == 0`.  PASS line:

`PASS V-B8: allocate zeroed header padding over poisoned byte`

### Change 4 — migrate the enumerated test arithmetic (test_dict_reloc.c)

Exactly these sites, nothing else (re-grep each expression first; if a
quoted expression is absent or duplicated, STOP):

1. `uint16_t bodyOff = fdict.latest + (uint16_t)TO_BLOCKS(4 + 5) * BYTES_PER_BLOCK;`
   → `TO_BLOCKS(6 + 5)` (5-byte name: ceil4(11) = 12, same value, honest
   expression).
2. V-B1: `memcpy(fdict.base + fdict.latest + 8 + 6, &badTok, 2);` →
   `fdict.latest + 12 + 6`, and update its comment ("3-glyph name: 6+3=9
   rounds to 12.  EXIT at offset latest+12+6.").
3. V-B2: `memcpy(fdict.base + fdict.latest + 8, &badTok, 2);` →
   `fdict.latest + 12`, comment likewise.
4. V-B6: `fdict.base[fdict.latest + 7] = 0xAA;` →
   `fdict.base[fdict.latest + 9] = 0xAA;`, comment: pads now at 9..11.
5. R4-3 probe: `fdict.here = fdict.latest + 4;` → `fdict.latest + 6;` and its
   comment at the function head (`latest+4` → `latest+6`).

### Existing tests and comments

Every other test must stay green UNCHANGED.  If any test outside the
enumerated set reddens, STOP and report — do not repair it.  (`begin_word`
fixtures flow through `forthDictAllocate` and inherit the new layout
automatically; that is by design.)

### Non-goals / STOP boundaries

- No second region, no ref encoding, no scope filtering, no save-format
  change (F3-2/F3-3).
- No behavior change to lookup order, lifecycle, param core, or the entry
  layer.
- Do not touch `forth_compile.c`, `forth_prims.c`, or any `programming/` or
  `ui/` file.
- If arithmetic other than the enumerated inventory appears to need a 4→6
  change to go green, STOP and report it — that is a packet defect.

### Gate and required mutations

Full sanctioned gate green first; record the NEW `FORTH ARENA` line — it is
the new stage baseline (the old one from the pre-log is superseded; quote
both in the report and in the commit message).  Then each mutation
separately, full gate, restore the hunk manually:

1. In `forthDictValidateRestored`, delete the new
   `if (hdr->owner != FORTH_OWNER_INTERACTIVE)` line.  V-B7 MUST go RED
   (foreign owner survives).  Green = STOP.
2. In `bodyOffsetOfIndex` (forth_inner.c), change `6 + hdr->nameLen` back to
   `4 + hdr->nameLen`.  The basic colon-word self-tests MUST go RED (bodies
   decoded 2 bytes early).  Name the first RED line in the report.
3. In `forthDictAllocate`, delete only the new pad-zero loop (keep the owner
   write).  V-B8 MUST go RED (0xAA poison survives at latest+9).
4. In `forthFindColon`, change `off + 6` back to `off + 4`.  Any
   lookup-dependent test MUST go RED (report the first: expected
   `: SQ ... ; SQ`-class resolution failure).

Logs `/tmp/forth-f3-1-mut1.log` .. `-mut4.log`; then
`git diff` must show no mutation residue, final full gate to
`/tmp/forth-f3-1-final.log`, record: the two new PASS lines, all V-B PASS
lines, both banners, exit 0, the arena line (new baseline), `git diff
--check` clean, byte equality of each flat file vs its generated `files/`
counterpart.

RULE-1: no flash-relevant change expected (layout constant shifts only);
still note `make dmcp5r47` as PENDING for the owner to run at their
convenience.

### Commit

`git status --short` may list only the four named flat files, their
generated `files/` counterparts, and
`packages/forth-core/.refresh-manifest.json`.  Stage exactly those and
commit:

```text
forth-core: F3-1 — dictionary headers carry an owner field
```

Report commit id, all required output, mutation symptoms, old + new arena
baselines, and anything surprising.
