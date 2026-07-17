# R1 code defects — Qwen implementation prompts

**STATUS (2026-07-15, post-R6 audit — read before pasting anything):**

| Task | Status |
|---|---|
| R1-1 | **EXECUTED** (commit 1806d48d8, reconciled with R4-2) — historical record only |
| R1-2 | **EXECUTED** (commit 390d7ba15) — historical record only |
| R1-3 | **REWRITTEN 2026-07-15** per R6 §10.1 — the only executable task in this file |
| R1-4 | **WITHDRAWN** — see its banner; FTOK_XEQN lands in stage F3 (DESIGN.md §10.3) |

**How to use:** paste the PREAMBLE, then the R1-3 block, into a fresh Qwen
session. R1-3 commits by itself; the old "one commit for R1-1..R1-4"
instruction is void (R1-1/R1-2 were committed separately).

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm git branch --show-current is forth-core/pem-entry-fixes. If not,
   STOP.
2. The only build/test command is ./packages/forth-core/build-test.sh.
   Success = FORTH SELF-TEST: ALL PASSED and ==> BUILD + SELF-TEST GREEN.
   and exit 0. Never invoke meson or ninja directly — a hand-rolled build omits
   the self-test suite entirely and reports green having asserted nothing.
3. All edits go in packages/forth-core/. Never edit src/. The build reads
   only the GENERATED patches/ + files/; build-test.sh refreshes first, so
   using the gate is sufficient. Never hand-edit patches/ or files/.
4. Never touch src/c47/core/freeList.c or any copy. Never read DESIGN.md or
   DESIGN-HISTORY.md — your prompt carries every slice you need. Never read
   items.c (it is enormous) or test_dict_reloc.c in full; read only the ranges
   listed. Use grep -a.
5. Match surrounding code style. Keep upstream-derived files byte-identical
   except the marked change, so the generated patch stays small.
6. Do not commit unless told. Never git add -A. Never run git stash,
   git stash pop, git reset, git checkout -- <file>, or git restore — the tree
   carries uncommitted work and stash@{0} is a foreign stash from another
   branch. If you think you need to undo something, STOP and report. A red gate
   is safe; a mangled tree is not.
7. If the gate goes red on a test asserting the OLD behaviour your task was
   written to change, that test is part of your task — but STOP and report
   before touching it. Never make a test pass by weakening the change it
   caught. If a task changes a contract without listing the tests that encode
   it, the spec is wrong: say so and stop.
8. Report what you changed, the gate output, and anything that surprised you.

Before any edit, run `git status --short`. The tree must be clean (only your
own task's edits may appear once you start). If anything else is modified,
STOP and report. (The 2026-07-15 preamble's "foreign unfinished
audit_probe_*" warning is obsolete — `test_dict_reloc.c` is complete; if a
`grep -a -n "audit_probe_" packages/forth-core/test_dict_reloc.c` ever
matches again, STOP: that is someone else's in-flight work.)

**Two-attempt debugger handoff (mandatory).** This rule applies only when this
task authorizes you to fix the observed error; it never overrides an earlier
immediate-STOP rule. After a command, test, or gate first fails because of your
task changes, you may make at most two distinct repair attempts. A repair
attempt is an edit intended to clear that failure followed by rerunning the
relevant command. The original task implementation is not a repair attempt. If
the required command is still not green after repair attempt 2 — even if the
visible error changes — STOP. Do not make a third repair, broaden scope, or use
git to undo anything. Leave the tree exactly as it stands; read-only inspection
is allowed only to prepare this report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and its relevant verbatim output;
- attempt 1: files/hunks changed, rationale, and resulting output;
- attempt 2: files/hunks changed, rationale, and resulting output;
- current `git status --short`, `git diff --stat`, and relevant diff excerpts;
- your best remaining hypotheses and anything that surprised you.

---

## R1-1 — Make the first dictionary allocation honor the requested capacity

**File(s):** packages/forth-core/forth_dict.c;
packages/forth-core/test_dict_reloc.c

**Read:** grep -a -n "forthDictEnsure\|FORTH_INITIAL_BLOCKS\|testInitialBlocks"
packages/forth-core/forth_dict.c, then read from the forthDictEnsure function
anchor through its lazy-allocation branch, no more than 50 lines. In
test_dict_reloc.c, grep -a -n
"test_dict_space_full\|forthDictSetTestInitialBlocks\|first_ensure" and read
only each matching test plus 20 lines and the registration lines.

**The defect.** In forthDictEnsure(uint16_t bytes), the null-base branch sets
initBlocks to FORTH_INITIAL_BLOCKS (or the PC test override), allocates exactly
that many blocks, and returns true. It never raises the initial allocation to
TO_BLOCKS((uint32_t)fdict.here + bytes). A first request larger than the initial
region is therefore reported safe and its caller can write beyond the
allocation. This can corrupt calculator RAM while compiling a large first
definition.

**The change.** In the existing null-base branch:

1. Compute uint32_t need = (uint32_t)fdict.here + bytes.
2. Compute uint32_t minBlocks = TO_BLOCKS(need).
3. Preserve the existing selection of initBlocks from FORTH_INITIAL_BLOCKS and
   the PC_BUILD testInitialBlocks override.
4. If minBlocks is greater than initBlocks, assign minBlocks to initBlocks.
   Keep the value widened until after this comparison.
5. Pass the resulting count to allocC47Blocks and store that same count in
   fdict.sizeBlocks.
6. Preserve the existing allocation-failure behavior and the existing 0xFFFE
   rejection. Do not change the established non-null growth branch.

Add test_dict_first_ensure_capacity. From an empty dictionary, set the PC test
initial block count deliberately small, request a byte count larger than that
capacity but below 0xFFFE, and assert: success; non-null base; TO_BYTES of
sizeBlocks is at least here + requested bytes; the last requested byte can be
written without passing the allocation; and the test override is restored on
every exit. If an existing unfinished audit_probe_first_ensure_capacity is
still present, STOP instead of creating a competing test.

**Tests that encode the old contract.** none — this changes no intended
contract. Leave test_dict_space_full, lifecycle init/reset, and save/restore
tests unchanged.

**Facts the harness forces on you.** FORTH_DEBUG_SELFTEST exposes
forthDictSetTestInitialBlocks. Use the existing self-test setup/cleanup helpers.
Do not allocate a second arena. The gate prints
"FORTH ARENA: dict here=... sizeBlocks=..."; report that final high-water line.

**Gate:** build-test green.
*Mutation: UNVERIFIED — the sanctioned gate cannot currently compile the
foreign truncated audit_probe_* tail. Once it is available, temporarily remove
only the minBlocks-to-initBlocks raise; test_dict_first_ensure_capacity must
fail its capacity assertion. Restore the line and rerun green.*

**Report:** paste back the exact changed functions, focused-test output, the
full gate's two success banners and exit code, the FORTH ARENA high-water line,
the mutation failure and restored-green result, or the exact foreign-work
blocker.

---

## R1-2 — Reject truncated threaded-code operands before reading them

**File(s):** packages/forth-core/forth_inner.c;
packages/forth-core/test_dict_reloc.c

**Read:** grep -a -n
"readToken\|FETCH\|case FTOK_LIT\|case FTOK_ILIT\|case FTOK_BR\|case FTOK_C47"
packages/forth-core/forth_inner.c. Read the readToken helper plus at most 60
lines around each switch group. In test_dict_reloc.c, grep -a -n
"test_malformed_token\|test_truncated_inline_operand" and read only those
functions and their registration lines.

**The defect.** forthInner reads the next token and the inline LIT, ILIT,
branch, and C47 operands directly from fdict.base without first proving the
bytes lie below fdict.here. A restored word whose logical end is immediately
after FTOK_ILIT can read beyond the dictionary instead of raising
ERROR_INVALID_CORRUPTED_DATA, potentially executing garbage or corrupting the
calculator stack.

**The change.** Add one file-static guard in forth_inner.c. It takes ip and
byteCount, checks with widened arithmetic that

    (uint32_t)ip + byteCount <= fdict.here

and returns true on success. On failure it sets and displays
ERROR_INVALID_CORRUPTED_DATA. At every call site, a false result must exit via
the existing INNER_LEAVE path so rsp and forthDepth unwind.

Call it before: every two-byte token fetch; FTOK_LIT's sizeof(real34_t) bytes;
FTOK_ILIT's four bytes; FTOK_BR and FTOK_0BR's two-byte delta; FTOK_C47's
two-byte item ID; and the two-byte parameter cell read by PTP_NUMBER_8 or
PTP_NUMBER_16. Do not change valid encodings, stack effects, error codes,
runaway behavior, or branch-target semantics.

Use the already-authored test_truncated_inline_operand only if it is complete
and no longer foreign unfinished work. It must create a valid word whose
logical fdict.here ends immediately after FTOK_ILIT, execute it, assert
ERROR_INVALID_CORRUPTED_DATA, and prove sentinel X is unchanged. Add analogous
focused cases for a truncated token fetch and truncated FTOK_C47 item ID if
they are absent.

**Tests that encode the old contract.** none — accepting truncated bodies was
never intended. Leave test_malformed_token, all LIT/ILIT/branch tests, all C47
PTP tests, and nested cleanup tests unchanged.

**Facts the harness forces on you.** test_truncated_inline_operand already
exists in the current flat file but that file cannot compile because later
foreign audit_probe_* work is truncated. Treat overlap as a STOP condition.
Report the gate's final FORTH ARENA high-water line.

**Gate:** build-test green.
*Mutation: UNVERIFIED — the sanctioned gate cannot currently compile the
foreign truncated audit_probe_* tail. Once available, temporarily bypass only
the pre-FTOK_ILIT guard; test_truncated_inline_operand must fail because it no
longer gets ERROR_INVALID_CORRUPTED_DATA and/or changes sentinel X. Restore the
guard and rerun green.*

**Report:** paste back each guarded read, focused malformed-data output, old
test/full-gate result, FORTH ARENA high-water line, mutation result, and any
foreign-work blocker.

---

## R1-3 — Add the §4.1 step-4 C47 item lookup to the outer interpreter
### (REWRITTEN 2026-07-15 per R6 §10.1 — supersedes the earlier R1-3 text)

**File(s):** packages/forth-core/forth_dict.h;
packages/forth-core/forth_dict.c (ADD one new function only — see the
prohibition below); packages/forth-core/forth_compile.c;
packages/forth-core/test_dict_reloc.c

**Read:** In forth_compile.c, grep -a -n
"step 3: number\|step 4: C47 label" and read from the first anchor through the
label arm, no more than 60 lines at a time. In forth_dict.c, grep -a -n
"forthFindColon\|forthResolveXEQ" and read forthFindColon only (you will place
the new function near it); you may read forthResolveXEQ to see the house
style but you MUST NOT change it. In test_dict_reloc.c, grep -a -n
"test_c47_ptp_none\|test_xeq_item_lookup\|test_xeq_precedence\|test_outer_compile_invoke\|writeTestProgram\|x_is_longint"
and read only those functions and registration lines. Locate item rows in
packages/forth-core/items.c with grep -a only; never open that file.

**The defect.** The required outer-source order is primitive, colon, number,
C47 item, C47 label, undefined (DESIGN.md §4.1). Current forth_compile.c jumps
from number directly to label, and no forthFindItem helper exists. A
calculator owner who types SIN in Forth gets label/undefined behavior instead
of the native C47 function; compiling SIN cannot emit FTOK_C47.

**The change — part 1, the helper.** Declare in forth_dict.h and implement in
forth_dict.c:

    bool forthFindItem(const char *name, uint16_t *itemId);

It scans IDs 1 through LAST_ITEM - 1 and returns a match only when ALL of:

    (indexOfItems[i].status & CAT_STATUS) == CAT_FNCT
    (indexOfItems[i].status & PTP_STATUS) == PTP_NONE
    compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME) == 0

On success set *itemId = i and return true; on miss return false and leave
*itemId untouched. This is the FORWARD (Forth-source) lookup only.

**PROHIBITION (this is the load-bearing correction to the old prompt): do NOT
touch `forthResolveXEQ` or any other existing function in forth_dict.c.** Its
item arm deliberately filters CAT_FNCT only — no PTP filter — and that
reverse-lookup contract is pinned by test_xeq_item_lookup (which asserts
"FORTH" and "FCALL", both CAT_FNCT but NOT PTP_NONE, resolve as items) and
documented as interim behavior in DESIGN.md §4.2. One helper must not serve
both contracts (R4-B3). If you find yourself editing forthResolveXEQ, STOP.

**The change — part 2, the outer arm.** In forth_compile.c define
FTOK_C47 as 0x7F04 beside the other mirrored token constants, and add the
item arm between the number step and the label step (comment anchors
"step 3: number" and "step 4: C47 label"; renumber the label arm's comment to
"step 5" and add "step 4: C47 item" on the new arm). Behavior:

- Compile state: emit FTOK_C47, then the uint16 item ID as one token cell
  (forthDictEmit twice). On either emit failure: abortDefinition() and stop
  the line exactly like the adjacent branches.
- Interpret state: save programRunStop; set PGM_RUNNING; call
  reallyRunFunction((int16_t)itemId, NOPARAM); restore the saved value only
  if programRunStop is still PGM_RUNNING (§2.2 protocol, identical to the
  FTOK_C47 inner arm). Then the adjacent lastErrorCode gate: on error,
  abortDefinition() if a definition is open, and stop the line.

**Non-goals (all STOP conditions if you find yourself doing them):** no
FTOK_XEQN, no change to the label arm, no change to forthResolveXEQ or any
reverse-lookup path, no parameterized items (PTP_REGISTER/NUMBER_8/16 stay
out of forthFindItem — stage F4), no new primitives, no DESIGN.md edits.

**Tests.** Add one focused group `test_outer_item_lookup` (plus helpers as
needed), registered beside test_outer_compile_invoke:

1. forthFindItem("SIN") returns true with *itemId == ITM_sin. **The
   identifier is lowercase `ITM_sin` (items.h:88, value 76)** — `ITM_SIN`
   does not exist; do not "fix" the case anywhere else.
2. forthFindItem("STO") returns false (parameterized item).
3. forthFindItem("FORTH") returns false and forthFindItem("FCALL") returns
   false (CAT_FNCT but PTP_REM / PTP_NUMBER_16 — the PTP_NONE filter, and
   the contrast with forthResolveXEQ which DOES resolve them).
4. Compile ": ISIN SIN ;" and byte-probe the body: FTOK_C47 (0x7F04), then
   ITM_sin (76) as one cell, then FTOK_EXIT — in that order.
5. Interpret order pin, colon-over-item: define ": SIN 42 ;" (colon word
   deliberately named like the builtin), interpret "SIN", require
   x_is_longint(42) — the colon word wins (§4.1: prim → colon → number →
   item → label). forthDictClear() before and after this subcase.
6. Interpret dispatch + item-over-label pin: with the dictionary clear,
   write a real program via writeTestProgram whose bytes are exactly
   `{ 0x01, 0xFD, 3, 'S', 'I', 'N' }` (ITM_LBL + STRING_LABEL_VARIABLE +
   len 3 + "SIN" — a global label named SIN), then forthPushInt32(0) and
   forthOuterInterpret("SIN"). Require lastErrorCode == ERROR_NONE and X of
   type dtReal34 with real34IsZero true (the ITEM ran: sin(0) = 0 as a
   real34 in every angular mode; if the LABEL had hijacked the name, the
   empty program leaves X the unchanged dtLongInteger). Cleanup:
   cleanupTestProgram() and forthDictClear() on every path.

**Tests that encode the old contract.** test_xeq_item_lookup and
test_xeq_precedence pin the REVERSE lookup this task must not alter — they
must pass **unchanged**; if either goes red you have touched forthResolveXEQ:
revert per rule 6 guidance (STOP and report, no git commands). Leave number,
C47 PTP, and outer compile/invoke tests unchanged.

**Facts the harness forces on you.** reallyRunFunction is callable from
test-linked production code; do not export a static keyboard function or
simulate keyboard/catalog state. writeTestProgram/cleanupTestProgram,
x_is_longint, begin_word/end_word/emit helpers all exist. In DEFS_ONLY
pre-scan mode, interpret-state tokens are skipped before your new arm is
reached — no special handling needed. Report the gate's FORTH ARENA line.

**Gate:** build-test green.
*Mutations (run all three after the green run, one at a time, restore by
re-editing — never git):*
1. Bypass the new outer item arm (comment out its body) → subcases 4 and 6
   go RED (SIN falls through to label/undefined).
2. Remove only the PTP_NONE conjunct from forthFindItem → subcases 2 and 3
   go RED.
3. Move the item arm ABOVE the colon lookup → subcase 5 goes RED with X=…
   real34 instead of 42.
Restore all code and rerun green.

**Commit (this task commits alone).** After the final green run:
`git status --short`; stage exactly
packages/forth-core/forth_dict.h, packages/forth-core/forth_dict.c,
packages/forth-core/forth_compile.c, packages/forth-core/test_dict_reloc.c,
their generated counterparts under packages/forth-core/files/, and
packages/forth-core/.refresh-manifest.json — never git add -A. If any other
path is dirty, STOP and report. Commit once with message
"forth-core: R1-3 — outer interpreter resolves C47 items (§4.1 step 4)".

**Report:** paste back forthFindItem in full, the new outer arm, the emitted
cells from subcase 4, all six subcase PASS lines, the three mutation RED
symptoms, both green gate banners + exit code, the FORTH ARENA line, the
commit hash, and anything surprising.

---

## R1-4 — Implement compiled C47 labels with FTOK_XEQN

> **STATUS: WITHDRAWN 2026-07-15 — DO NOT EXECUTE ANY PART OF THIS TASK.**
> Superseded by the R4 accepted architecture before it ever ran:
> (1) implementing FTOK_XEQN before the stage-F1 lifetime foundations opens
> the B1 use-after-free (an interactive word XEQ'ing a program that contains
> a Forth step bumps the generation; the callee's first Forth step calls
> `forthDictClear()` under the suspended `forthInner` frame);
> (2) this task's FORTH_XEQ_COLON arm prescribes a PGM_RUNNING wrap the
> accepted B4 ruling forbids;
> (3) the accepted ordering puts XEQN in stage F3, after F1/F2
> (DESIGN.md §10.3, which also adds the [kind][len][name] encoding this
> task predates — its [len][name] encoding is obsolete).
> Its commit instruction ("one commit for R1-1 through R1-4") is void.
> The text below is retained as the historical record only.

**File(s):** packages/forth-core/forth_compile.c;
packages/forth-core/forth_inner.c; packages/forth-core/forth_dict.h only if
sharing the token constant is required; packages/forth-core/test_dict_reloc.c

**Read:** grep -a -n "step 4: C47 label" packages/forth-core/forth_compile.c and
read that arm plus 20 lines on each side. In forth_inner.c, grep -a -n
"case FTOK_C47\|default:" and read the C47 arm in chunks of at most 60 lines.
In forth_dict.c, grep -a -n "forthResolveXEQ" and read only that function; do
not change its order. In test_dict_reloc.c, grep -a -n
"test_xeq_end_to_end\|test_xeq_precedence\|test_xeq_item_lookup\|test_c47_nested_call_succeeds"
and read only those functions, raw-token helpers, and registration lines.

**The defect.** Compile-state label lookup currently displays
ERROR_INVALID_NAME and aborts. FTOK_XEQN (0x7F05) is absent from both emitter
and inner interpreter. Label IDs renumber whenever program labels are rebuilt,
so storing an ID would silently call the wrong calculator program after an
edit. A compiled Forth word cannot currently call a C47 program by the
required persistent name.

**The change.** Add FTOK_XEQN = 0x7F05 to the mirrored token constants. Its
encoding is: two-byte token; one uint8 length; exactly length name bytes; then
one zero pad byte iff 1 + length is odd. Valid length is 1..FORTH_NAME_MAX
(31). No label ID is stored.

In the existing compile-state label arm: emit FTOK_XEQN, length, name bytes,
and conditional zero pad. On any failure, abort the definition and stop the
line through the adjacent error path. Leave interpret-state label behavior
exactly as it is: dynamicMenuItem = -1, then direct fnExecute(label). Never
force PGM_RUNNING around a label.

Add an FTOK_XEQN inner switch arm that bounds-checks the length byte and complete
padded name before reading; rejects length 0 or > FORTH_NAME_MAX with
ERROR_INVALID_CORRUPTED_DATA; copies into a local NUL-terminated
FORTH_NAME_MAX + 1 buffer; and calls forthResolveXEQ(name, &param) fresh on
every execution. Dispatch:

- FORTH_XEQ_LABEL: dynamicMenuItem = -1; fnExecute(param) directly.
- FORTH_XEQ_ITEM: save programRunStop, set PGM_RUNNING, call
  reallyRunFunction((int16_t)param, NOPARAM), conditionally restore.
- FORTH_XEQ_COLON: save programRunStop, set PGM_RUNNING, call
  forthInner(param, programRunStop == PGM_RUNNING), conditionally restore.
- FORTH_XEQ_NONE: set errorMessage to the unresolved name and raise
  ERROR_FUNCTION_NOT_FOUND.

Every error exits through INNER_LEAVE. Preserve forthResolveXEQ's label > item
> colon order. Never bake/cache a resolution.

Add focused tests proving: compiled ": CALLP MYPROG ;" contains the name bytes,
not the current label ID; odd/even name lengths leave FTOK_EXIT cell-aligned;
execution resolves and runs the current label; label-list reordering still
calls the same name; length 0, length 32, truncated name, and truncated pad
raise ERROR_INVALID_CORRUPTED_DATA without changing sentinel X; a removed name
raises ERROR_FUNCTION_NOT_FOUND; and item and colon resolver results use their
specified arms. Use existing program-memory and label helpers; never simulate a
file-static keyboard chain.

**Tests that encode the old contract.** none — rejecting labels in compile
state was an implementation omission, not the intended contract. Leave
test_xeq_end_to_end, test_xeq_precedence, test_xeq_item_lookup, nested-call,
rstack cleanup, and interactive direct-label tests unchanged.

**Facts the harness forces on you.** fnExecute, reallyRunFunction,
forthResolveXEQ, and forthInner are production-callable from this path; no
keyboard static export is needed. Entry resolution must be repeated after a
real label-list rebuild, not after hand-assigning a cached label ID. The inline
byte count 1 + length is padded only when odd; for a 31-byte name it is already
32 and needs no pad. Report the final FORTH ARENA high-water line.

**Gate:** build-test green.
*Mutation: UNVERIFIED — the sanctioned gate cannot currently compile the
foreign truncated audit_probe_* tail. Once available: (1) temporarily store the
current label ID; the reorder test must fail; (2) restore names and cache the
first resolution; reorder must fail; (3) restore fresh resolution and force the
label arm through PGM_RUNNING; the direct-dispatch regression must fail.
Restore all code and rerun green.*

After every test and mutation is restored and the final gate is green, make one
commit for R1-1 through R1-4 only. Before staging, run git status --short. If
there is any foreign or ambiguous change, STOP and report; do not commit.
Otherwise stage only the explicitly changed flat files, their matching
generated files, and the refresh manifest—never git add -A—and commit once with
message "forth-core: honor verified Forth runtime contracts".

**Report:** paste back the exact encoded bytes and dispatch arms, all focused
and preserved test results, full gate banners/exit, FORTH ARENA high-water
line, all mutation results, the exact files committed and commit hash, or the
foreign-work reason no commit was made.

---
