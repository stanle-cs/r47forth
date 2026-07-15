# Forth engine robustness fixes — Qwen implementation prompts

4 tasks, strictly ordered. Each is sized for a ~100k-token context window: it
names the only files and line ranges to read, and never requires reading
DESIGN.md.

**How to use:** paste the PREAMBLE, then one task block, into a fresh Qwen
session. Do not run tasks out of order.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes`. If not, STOP.
2. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success = `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.`
   and exit 0. Never invoke meson or ninja directly — a hand-rolled build omits
   the self-test suite entirely and reports green having asserted nothing.
3. All edits go in `packages/forth-core/`. Never edit `src/`. The build reads
   only the GENERATED `patches/`+`files/`; build-test.sh refreshes first, so
   using the gate is sufficient. Never hand-edit `patches/`/`files/`.
4. Never touch `src/c47/core/freeList.c` or any copy. Never read DESIGN.md or
   DESIGN-HISTORY.md — your prompt carries every slice you need. Never read
   items.c (it is enormous) or test_dict_reloc.c in full; read only the ranges
   listed. Use `grep -a`.
5. Match surrounding code style. Keep upstream-derived files byte-identical
   except the marked change, so the generated patch stays small.
6. Do not commit unless told. Never `git add -A`. **Never run `git stash`,
   `git stash pop`, `git reset`, `git checkout -- <file>`, or `git restore`** —
   the tree carries uncommitted work and `stash@{0}` is a foreign stash from
   another branch. If you think you need to undo something, STOP and report. A
   red gate is safe; a mangled tree is not.
7. If the gate goes red on a test asserting the OLD behaviour your task was
   written to change, that test is part of your task — but STOP and report
   before touching it. Never make a test pass by weakening the change it caught.
   If a task changes a contract without listing the tests that encode it, the
   spec is wrong: say so and stop.
8. Report what you changed, the gate output, and anything that surprised you.

---

## R4-1 — Enforce the exact exponent-sign grammar

**File(s):** `packages/forth-core/forth_compile.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** In `forth_compile.c`, use `grep -an "static forthNumType_t
classifyNumber"` and read only that function through its closing brace (about
46 lines). In `test_dict_reloc.c`, use `grep -an` for
`test_number_1e_minus_5`, `test_number_bad_3e`, and the self-test call
`running test_number_bad_3e`; read no more than 55 lines around any one anchor.
Trust the grep anchor, not the line number — earlier tasks shift lines.

**The defect.** The grammar is `[eE][+-]?digit+`: a sign is legal only as the
first byte after `e`/`E`, and only one sign is legal. The current clause accepts
`+` or `-` anywhere after an exponent marker. Executed probe:
`forthOuterInterpret("1e2-3 7")` left `lastErrorCode == ERROR_NONE`; it must
reject `1e2-3` as an undefined word and stop before `7`. A malformed token is
therefore silently accepted or silently truncates the line instead of telling
the calculator owner what was wrong.

**The change.** Replace only the exponent-sign condition in `classifyNumber`.
The sign arm is taken iff all of these are true:

```c
(s[i] == '+' || s[i] == '-')
hasExp
expDigits == 0
i > 0
s[i - 1] == 'e' || s[i - 1] == 'E'
```

Keep the existing `i++`. Do not change the valid `1e-5` path, number parsing,
or any error mapping.

Add `test_number_bad_exponent_sign_position` beside the existing bad-number
tests. It must:

1. `forthDictClear()`, set `lastErrorCode = ERROR_NONE`, clear
   `errorMessage[0]`, and push a long-integer sentinel 444 with
   `forthPushInt32(444)`.
2. Call `forthOuterInterpret("1e2-3 7")`.
3. Require `lastErrorCode == ERROR_FUNCTION_NOT_FOUND`.
4. Require `strcmp(errorMessage, "1e2-3") == 0`.
5. Require `x_is_longint(444)` so neither the malformed token nor tail `7`
   changed X.
6. Print one PASS line and return 0; on each mismatch print the observed value
   and return 1.

Call it in `forthDictSelfTest` immediately after `test_number_bad_3e` and before
`test_number_bad_lone_dot`.

**Tests that encode the old contract.** none — the exact grammar already
rejects this token; the existing tests simply omit the sign-position edge.

**Facts the harness forces on you.** `x_is_longint` is already a file-static
test helper and is callable from this new test. Do not expose a production
function and do not simulate `classifyNumber`; drive the public
`forthOuterInterpret` path.

**Gate:** build-test green.
*Verified mutation:* restore the current broad clause
`(s[i] == '+' || s[i] == '-') && hasExp`. The new probe was executed against
that code and went RED with `lastErrorCode = 0` instead of
`ERROR_FUNCTION_NOT_FOUND = 7`.

**Report:** paste the changed condition, the new test's PASS line, both required
green gate lines, and any surprise. Do not commit in this task.

---

## R4-2 — Make dictionary capacity arithmetic truthful

**File(s):** `packages/forth-core/forth_dict.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** In `forth_dict.c`, use `grep -an "bool forthDictEnsure"` and read only
that function (about 40 lines), then `grep -an "uint16_t forthDictAllocate"`
and read only that function (about 25 lines). In `test_dict_reloc.c`, use
`grep -an` for `test_dict_space_full` and its self-test call; read at most 55
lines around either anchor.

**The defect.** There are two violations of the same capacity contract.

1. On a null dictionary, `forthDictEnsure(bytes)` always allocates the configured
   initial block count and returns true without checking whether that allocation
   contains `bytes`. Executed probe with a one-block test allocation:
   `forthDictEnsure(6)` returned true with only 4 bytes of capacity.
2. `forthDictAllocate` adds aligned header size and `bodyBytes` in `uint16_t`.
   Executed probe `forthDictAllocate(31, 0xFFF0)` wrapped the total and returned
   offset 0 with no error, although the request cannot fit the 16-bit offset
   space.

Today the compiler happens to call `forthDictAllocate(nameLen, 0)` and emits in
two-byte increments; the helpers themselves are still false contracts. A future
caller using the documented `bodyBytes` parameter can write past its region on
the handheld.

**The change.** Make both calculations explicit and checked:

- In the lazy-initial-allocation arm of `forthDictEnsure`, after applying the
  PC-only `testInitialBlocks` override, compute the block count required for
  `bytes` with `TO_BLOCKS(bytes)`. Set `initBlocks` to the larger of its current
  value and that required count before `allocC47Blocks(initBlocks)`. The existing
  `here + bytes <= 0xFFFE` check runs first, so the required count fits
  `uint16_t`.
- In `forthDictAllocate`, make `hdrSize`, `alignedHdr`, and `total` `uint32_t`
  expressions. Before casting `total` for `forthDictEnsure`, reject when
  `(uint32_t)fdict.here + total > 0xFFFEu`: display `ERROR_RAM_FULL` with the
  same register-line arguments used by `forthDictEnsure`, then return
  `FORTH_NULL`. Only after that check call
  `forthDictEnsure((uint16_t)total)`. Cast `alignedHdr` back to `uint16_t` only
  for the already-proved-safe `fdict.here` update. Do not change the fact that
  the header advances `here` while `bodyBytes` is capacity reservation, not an
  immediate bump.

Add one test named `test_dict_capacity_arithmetic` near
`test_dict_space_full`. It has two subcases and a single cleanup tail:

1. Clear the dictionary, set test initial blocks to 1, request
   `BYTES_PER_BLOCK + 2` through `forthDictEnsure`, and require success plus
   `fdict.sizeBlocks * BYTES_PER_BLOCK >= requested`.
2. Clear again, set `lastErrorCode = ERROR_NONE`, call
   `forthDictAllocate(31, 0xFFF0u)`, and require `FORTH_NULL` plus
   `ERROR_RAM_FULL`.

Always finish by `forthDictClear()` and
`forthDictSetTestInitialBlocks(4)`, including after a failed assertion; use a
local `fail` accumulator rather than early returns. Add its self-test call
immediately after `test_dict_space_full`.

**Tests that encode the old contract.** none — the public comments already say
Ensure supplies at least the requested free bytes and Allocate rejects failure.

**Facts the harness forces on you.** The suite deliberately uses a four-block
initial region; restore that override before returning or later relocation tests
change meaning. `BYTES_PER_BLOCK` is 4 in this build, so the verified first
failure was request 6 versus capacity 4.

**Gate:** build-test green. Report the line beginning `FORTH ARENA:` as the
dictionary arena high-water mark.
*Verified mutation:* restore the current fixed-size lazy allocation and the
current `uint16_t total`. The executed test went RED twice: `Ensure(6)` returned
1 with 4 bytes capacity, and the overflowing Allocate returned offset 0 with
error 0.

**Report:** paste both PASS subcase lines, the arena high-water line, both green
gate lines, and any surprise. Do not commit in this task.

---

## R4-3 — Reject restored headers whose names cross `here`

**File(s):** `packages/forth-core/forth_dict.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** In `forth_dict.c`, use
`grep -an "void forthDictValidateRestored"` and read only that function, in two
chunks of at most 55 lines each. In `test_dict_reloc.c`, use
`grep -an "test_validate_direct_corruption"`; read only that function and its
preceding mutation comment, in chunks no larger than 55 lines, plus the single
self-test call found by a second `grep -an`.

**The defect.** The restore validator proves `off + 4 <= here` and proves
`nameLen` is 1..31, but never proves `off + 4 + nameLen <= here`. Executed
probe created a valid `: VX 1 ;` entry, set `here = latest + 4`, and ran
`forthDictValidateRestored`; the corrupt header survived. A damaged owner save
can therefore pass validation and make later lookup read bytes beyond the
logical dictionary.

**The change.** In the header-chain loop, immediately after the existing
`nameLen == 0 || nameLen > FORTH_NAME_MAX` rejection, add this independent
checked condition:

```c
if ((uint32_t)off + 4u + hdr->nameLen > fdict.here) {
  ok = false;
  break;
}
```

Do not add token-body validation in this task and do not change the deliberate
orphan policy on validator failure.

Extend `test_validate_direct_corruption` with V3 and update its mutation comment
to name the new bound. V3 must:

1. Clear the dictionary and successfully compile `: VD3 1 ;`.
2. Save `fdict.base` and `fdict.sizeBlocks` for deliberate-orphan cleanup.
3. Set `fdict.here = fdict.latest + 4` without changing the valid nonzero
   `nameLen`.
4. Call `forthDictValidateRestored()` and require `fdict.base == NULL`.
5. If validation incorrectly keeps the base, print FAIL and call
   `forthDictClear()` once. If validation resets it, manually call
   `freeC47Blocks(savedBase, savedBlocks)` once, matching V2's cleanup shape.

**Tests that encode the old contract.** none — accepting a name past logical
`here` is not a supported restore format.

**Facts the harness forces on you.** On rejected restore state the validator
intentionally calls `forthDictInit()` without freeing the suspect region; the
test, not production code, releases that known-good synthetic orphan exactly
once.

**Gate:** build-test green. Report the line beginning `FORTH ARENA:` as the
dictionary arena high-water mark.
*Verified mutation:* remove the new extent check. The executed V3 shape went
RED with `header name extending past here survived validation`.

**Report:** paste V3's PASS line, the arena high-water line, both green gate
lines, and any surprise. Do not commit in this task.

---

## R4-4 — Roll back a whole failed program pre-scan

**File(s):** `packages/forth-core/forth_compile.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** In `forth_compile.c`, use
`grep -an "static void forthPreScanOwningProgram"` and read only that function
(about 42 lines). In `test_dict_reloc.c`, use
`grep -an "test_prescan_error_halts"` and read only that function plus its
preceding comment (under 55 lines), then use a separate `grep -an` for its
self-test call and read at most 20 lines there.

**The defect.** A program pre-scan is not transactional. If an earlier source
step compiles a valid definition and a later step errors, the valid definition
remains, the program is not recorded as scanned, and every retry compiles
another copy before failing again. Executed two-step program:

```text
: G 1 ;
: B NOPE ;
```

left `fdict.count` at 1 after the first failed touch and 2 after the second.
Repeatedly running a program with one typo therefore consumes calculator RAM
until the dictionary fills.

**The change.** After the already-scanned early-return loop and immediately
before walking program steps, snapshot only these three region-relative scalar
values:

```c
uint16_t scanHere = fdict.here;
uint16_t scanLatest = fdict.latest;
uint16_t scanCount = fdict.count;
```

When `forthOuterRun(..., FORTH_OUTER_DEFS_ONLY)` leaves
`lastErrorCode != ERROR_NONE`, restore those three fields before returning.
Do not restore `fdict.base` or `sizeBlocks`: an emit may have reallocated the
region, so the old base is invalid; retaining a grown region above the restored
`here` is the same deliberate policy used by `abortDefinition`. Do not record
the failed program in `forthScannedProgs`.

Add `test_prescan_error_rolls_back_prior_defs` next to
`test_prescan_error_halts`. Use these exact program bytes:

```c
uint8_t prog[] = {
  0x8B, 0x1A, 0xFD, 7,  ':', ' ', 'G', ' ', '1', ' ', ';',
  0x8B, 0x1A, 0xFD, 10, ':', ' ', 'B', ' ', 'N', 'O', 'P', 'E', ' ', ';'
};
```

The test must call `writeTestProgram`, then `forthRunGenBump`, save and set
`programRunStop = PGM_RUNNING`, and call
`forthProgramStep(beginOfProgramMemory + 3)`. Capture `fdict.count`; clear only
`lastErrorCode`; call the same public entry a second time; capture count again;
then restore `programRunStop`. Require both counts to be 0 and the second error
to be `ERROR_FUNCTION_NOT_FOUND`. Always clear the dictionary and call
`cleanupTestProgram`. Add the self-test call immediately after
`test_prescan_error_halts`.

**Tests that encode the old contract.** none — the existing error-halt test puts
the error in the first definition, so it never exercises rollback of an earlier
successful definition.

**Facts the harness forces on you.** `writeTestProgram` derives `programList`
from the bytes; do not assign `programList`, catalog state, or dictionary state
by hand. `forthProgramStep` is public and callable from the test. Its payload
argument for the first step is exactly `beginOfProgramMemory + 3`.

**Gate:** build-test green. This is the final task: paste the line beginning
`FORTH ARENA:` as the final dictionary arena high-water mark, then stage only
the R4 working files and their generated copies:

```text
packages/forth-core/forth_compile.c
packages/forth-core/forth_dict.c
packages/forth-core/test_dict_reloc.c
packages/forth-core/files/forth_compile.c
packages/forth-core/files/forth_dict.c
packages/forth-core/files/test_dict_reloc.c
packages/forth-core/.refresh-manifest.json
```

Never use `git add -A`. Run `git diff --cached --check`, inspect the staged file
list, and commit once with message `forth: harden engine edge cases`. If any
path outside that exact list is staged, STOP and report instead of committing.

*Verified mutation:* remove the three-field restore at the pre-scan error exit.
The executed regression shape went RED with counts 1 then 2.

**Report:** paste both count assertions' PASS line, the final arena high-water
line, both green gate lines and exit 0, the commit hash, and anything surprising.

---
