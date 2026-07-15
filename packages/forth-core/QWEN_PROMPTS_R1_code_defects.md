# R1 code defects — Qwen implementation prompts

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

Before any edit, run:

    tail -n 20 packages/forth-core/test_dict_reloc.c
    grep -a -n "audit_probe_" packages/forth-core/test_dict_reloc.c

At R1 audit time that file was foreign unfinished work: it ended mid
audit_probe_buried_catalog with the top-level #if unterminated and called five
audit_probe_* functions before declarations. Never repair, delete, or work
around that foreign code. If it remains unfinished or overlaps this task, STOP
and report the blocker.

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

## R1-3 — Add PTP_NONE C47 item lookup to the outer interpreter

**File(s):** packages/forth-core/forth_dict.h;
packages/forth-core/forth_dict.c; packages/forth-core/forth_compile.c;
packages/forth-core/test_dict_reloc.c

**Read:** grep -a -n "forthResolveXEQ" packages/forth-core/forth_dict.c and read
only that function. In forth_compile.c, grep -a -n
"step 3: number\|step 4: C47 label" and read from the first anchor through the
label arm, no more than 60 lines at a time. In test_dict_reloc.c, grep -a -n
"test_c47_ptp_none\|test_xeq_item_lookup\|test_xeq_precedence\|test_outer_compile_invoke"
and read only those functions and registration lines. Locate SIN and STO in
packages/forth-core/items.c with grep -a only; never open that file.

**The defect.** The required outer-source order is primitive, colon, number,
C47 item, C47 label, undefined. Current forth_compile.c jumps from number
directly to label, and no forthFindItem helper exists. A calculator owner who
types SIN in Forth gets label/undefined behavior instead of the native C47
function; compiling SIN cannot emit FTOK_C47.

**The change.** Declare and implement:

    bool forthFindItem(const char *name, uint16_t *itemId);

It scans IDs 1 through LAST_ITEM - 1 and returns a match only when:

    (indexOfItems[i].status & CAT_STATUS) == CAT_FNCT
    (indexOfItems[i].status & PTP_STATUS) == PTP_NONE
    compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME) == 0

On success set *itemId = i. Refactor only the item arm of forthResolveXEQ to use
this helper, preserving that function's different order label > item > colon.

In forth_compile.c define FTOK_C47 as 0x7F04 beside the other mirrored tokens
and add the item arm after number and before label. Compile state emits
FTOK_C47, then the uint16 item ID as one token cell. On either emit failure,
abort an open definition and stop the line exactly like adjacent branches.
Interpret state saves programRunStop, sets PGM_RUNNING, calls
reallyRunFunction((int16_t)itemId, NOPARAM), and restores the saved state only
if the call left programRunStop equal to PGM_RUNNING. Apply the adjacent
lastErrorCode/abort behavior.

Add one focused test group proving: forthFindItem("SIN") returns ITM_SIN;
forthFindItem("STO") is false because it is parameterized;
forthFindItem("FORTH") is false because it is not CAT_FNCT/PTP_NONE;
": ISIN SIN ;" stores FTOK_C47, ITM_SIN, FTOK_EXIT in order; and interpreting
SIN on a zero real completes without error with zero result. Do not admit any
parameterized item.

**Tests that encode the old contract.** none — missing item lookup is not an
intended contract. Leave test_xeq_item_lookup and test_xeq_precedence unchanged:
forthResolveXEQ remains label > item > colon even though outer source is item >
label. Leave number, C47 PTP, and outer compile/invoke tests unchanged.

**Facts the harness forces on you.** reallyRunFunction is already callable from
test-linked production code; do not export a static keyboard function or
simulate keyboard/catalog state. Use existing stack and dictionary test
helpers. Report the gate's FORTH ARENA high-water line.

**Gate:** build-test green.
*Mutation: UNVERIFIED — the sanctioned gate cannot currently compile the
foreign truncated audit_probe_* tail. Once available: (1) temporarily bypass
the new outer item arm; the compile/interpret assertions must fail; (2) restore
it and remove only the PTP_NONE filter; the STO rejection must fail. Restore
all code and rerun green.*

**Report:** paste back the resolver orders, exact item filter, emitted cells,
focused/old/full-gate results, FORTH ARENA high-water line, both mutation
results, and any blocker.

---

## R1-4 — Implement compiled C47 labels with FTOK_XEQN

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
