# R2 test-suite credibility repairs — Qwen implementation prompts

8 tasks, strictly ordered. Each is sized for a ~100k-token context window: it
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

## R2-T1 — Give the inner-interpreter tests observable postconditions

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** use `grep -a -n '^static int test_\(stack_aslift\|branch_fwd\|literal_after_lit\|div_zero_halt\|rstack_overflow\|runaway_guard\|malformed_token\|br_delta_sign_extend\)' packages/forth-core/test_dict_reloc.c`, then read at most 60 lines from each returned anchor. Also read the `x_is_longint`/`y_is_longint` helpers, at most lines 90-115.

**The defect.** Eight tests accept executions that do not establish their own
claim. Empirical R2 probes produced these results:

- deleting `DUP PLUS` from `test_stack_aslift` left the full gate GREEN;
- changing the forward `BR` delta from 1 to 0 left the old test GREEN because
  the unskipped `DROP` had no relevant canary; adding a Y canary made that same
  mutation fail with `FAIL: forward BR consumed its canary`;
- deleting the second literal and `DROP` from `test_literal_after_lit` left the
  gate GREEN;
- changing the documented surviving 42 to 43 in `test_div_zero_halt` left the
  gate GREEN; moreover, a temporary `X == 42` assertion failed against the
  correct implementation, so the comment's “original value preserved” claim
  is false and must not be converted into an assertion;
- omitting the calls of `A0`, `RR2`, `MP`, `MC`, and `MR` left the gate GREEN;
- decoding `BR +260` as `BR +4` left `test_br_delta_sign_extend` GREEN because
  256 additional `DROP` fillers still led to `ILIT 777`.

For a calculator owner these are the guards against corrupt bytecode running
past bounds or silently executing the wrong stack program. A green test that
does not invoke its word is worse than no test because it blesses the guard.

**The change.** Make these exact test-only changes:

1. In `test_stack_aslift`, retain the ASLIFT assertion and also require
   `x_is_longint(10)` after `ILIT 5 DUP +`.
2. In `test_branch_fwd`, call `forthPushInt32(123456)` immediately before
   `run_word("BF")`; after X is checked as 42, require
   `y_is_longint(123456)`. Thus an unskipped `DROP` consumes the canary.
3. Change `test_literal_after_lit` to encode
   `ILIT 10 | ILIT 20 | PLUS | EXIT`, update its body comment, and require
   `x_is_longint(30)`. Do not keep the result-cancelling `DROP` fixture.
4. In `test_div_zero_halt`, remove the two comment claims that X remains 42 and
   that the “original value” is preserved. Keep the exact
   `ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN` check and the `X != 999` sentinel check.
   Do not add an X==42 assertion.
5. In `test_rstack_overflow`, success requires exactly
   `lastErrorCode == ERROR_RAM_FULL` as well as `X != 777`; ERROR_NONE or any
   other error is failure.
6. In `test_runaway_guard`, success requires both `x_is_longint(444)` and
   `lastErrorCode == ERROR_RAM_FULL`; delete both permissive success arms.
7. In each `test_malformed_token` subcase, add an exact error assertion before
   the existing sentinel checks: bad PRIM → `ERROR_OPERATION_UNDEFINED`; bad
   CALL → `ERROR_INVALID_CORRUPTED_DATA`; reserved token →
   `ERROR_INVALID_CORRUPTED_DATA`.
8. In `test_br_delta_sign_extend`, emit 260 `T_EXIT` fillers instead of 260
   `DROP` primitives. Correct +260 reaches `ILIT 777`; a low-byte-only +4
   reaches a filler EXIT with X not 777. Update the layout comment.
9. Do not alter production code. Give each new failure a unique `FAIL:` label.

**Tests that encode the old contract.** `test_div_zero_halt`'s comments encode
the false X-preservation claim; re-aim the comments as above. No test encodes a
different behavioral contract.

**Facts the harness forces on you.** `forthPushInt32` lifts the existing stack;
`y_is_longint` is already present. Production reports `ERROR_RAM_FULL` for both
the return-stack depth guard and the runaway dispatch cap. Production reports
the malformed-token errors listed above.

**Gate:** build-test green. Report the arena line; the R2 baseline high-water
was `dict here=36 sizeBlocks=16 freeRamDelta=64`.
*Verified mutation:* R2 ran omissions of `DUP PLUS`, `A0`, `RR2`, `MP`, `MC`,
and `MR`; with the proposed exact assertions temporarily installed they made
the gate RED with eight distinct failures (stack result, rstack error 0,
runaway error 0, and three malformed-token error-0 failures included). R2 also
ran `BR delta 1 → 0`: the proposed Y canary made it RED with
`FAIL: AUDIT R2 forward branch consumed the Y canary`. The new PLUS-form
literal fixture and T_EXIT large-delta fixture were not temporarily installed:
*Mutation: UNVERIFIED — run both named mutations before reporting this task
complete.*

**Report:** paste the changed test names, each mutation run and exact RED
symptom, the final green banners, and the arena high-water line.

---

## R2-T2 — Replace the always-green LBL? placeholder with a reachable integration test

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep `static int test_lblq_undefined_no_ub` and read 60 lines from its
anchor; grep `static bool writeTestProgram` and read only its definition plus
`cleanupTestProgram`; grep `test_gto_word_errors` and read that test only.

**The defect.** `test_lblq_undefined_no_ub` prints `DEFERRED` and returns 0. It
cannot fail. Its claim that `_executeOp` is unreachable is wrong:
`executeOneStep`, already called by this file, reaches the static `_executeOp`.
R2 changed the real LBL? arm from
`reallyRunFunction(op, INVALID_VARIABLE)` to
`reallyRunFunction(op, resolvedParam)`; the full suite stayed GREEN.

**The change.** Replace the placeholder body with this real-program scenario;
do not export `_executeOp` and do not simulate it:

1. `forthDictClear()`, then define the colon word `FW` so its dictionary index
   is 0. The body may be an immediate `EXIT`; use the existing helpers.
2. Write exactly this program using `writeTestProgram`:

```c
uint8_t prog[] = {
  0x01, 0x00,                         /* LBL 00 */
  0x85, 0xDF, 0xFD, 2, 'F', 'W'      /* LBL? "FW" */
};
```

   `0x85 0xDF` is `ITM_LBLQ` (1503); `0xFD` is
   `STRING_LABEL_VARIABLE`. The explicit local label 00 and colon index 0 are
   intentionally the same numeric value.
3. Set `temporaryInformation = TI_TRUE`, set `lastErrorCode = ERROR_NONE`, and
   call `executeOneStep(beginOfProgramMemory + 2)`.
4. Require `lastErrorCode == ERROR_NONE` and
   `temporaryInformation == TI_FALSE`. Correct code asks ordinary LBL? to look
   up `INVALID_VARIABLE`, so the Forth-only name is not treated as local label
   00. The escaping mutation passes resolved index 0 and produces TI_TRUE.
5. On every exit, call `forthDictClear()` and `cleanupTestProgram()`; restore
   any global that this test changes and that those helpers do not restore.
6. Rename the test to `test_lblq_forth_name_not_local_label` and update its
   runner call and debug label.

**Tests that encode the old contract.** The old placeholder encodes none;
replace it, do not retain a “deferred” pass.

**Facts the harness forces on you.** `executeOneStep` is callable. Do not call
or export `_executeOp`. `writeTestProgram` appends the final `.END.`; the two
bytes `0x01,0x00` are a real LBL 00 step, not `.END.`.

**Gate:** build-test green. Report the arena high-water line.
*Mutation: UNVERIFIED — R2 verified that `INVALID_VARIABLE → resolvedParam`
left the old suite GREEN, but did not temporarily install this replacement
fixture. Apply that mutation after adding the test; it must go RED with TI_TRUE,
then revert it without git restore and rerun green.*

**Report:** paste the real program bytes, correct-run TI value, mutation-run TI
value and RED label, final green banners, and arena line.

---

## R2-T3 — Test the production reset hook and the runtime UNDO flags

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep `static int test_lifecycle_reset` and read 60 lines; grep
`static int test_undo_rows_us_enabled` and read that test; grep the self-test
runner call sites for those two names and read 30 lines around each. Do not read
`config.c`; the exact callable contract is below.

**The defect.** `test_lifecycle_reset` calls `forthDictInit()` itself, so it
proves the callee but not the reset wiring. R2 deleted the production
`forthDictInit()` call from `doFnReset`; the entire suite remained GREEN.
`test_undo_rows_us_enabled` opens an absolute source pathname and converts
missing files or missing rows into SKIP/pass. The calculator does not execute
that source file; it uses `indexOfItems[]`.

**The change.** Make two independent changes in the same lifecycle/registration
test task:

1. Keep the existing direct `test_lifecycle_reset` as the allocator-semantic
   unit test. Add `test_lifecycle_real_reset_hook`, PC self-test only, using
   `fork()` so the destructive calculator reset cannot corrupt the parent test
   process. In the child: define `SQ`; prove it is found; call
   `doFnReset(CONFIRMED, doNotLoadAutoSav)`; require `forthFindColon("SQ", &idx)`
   is false and `fdict.base == NULL`, `fdict.here == 0`, `fdict.count == 0`,
   `fdict.latest == FORTH_NULL`; `_exit(0)` on success and a unique nonzero
   code for each failure. In the parent: `waitpid`, require normal exit 0, and
   print the child's exit code on failure. Include `<sys/types.h>`,
   `<sys/wait.h>`, and `<unistd.h>` only inside the same PC/self-test
   compilation guard as this file. Do not add a production export: `doFnReset`
   is declared in `config.h` and is already callable. The self-test run-once
   guard is set before these tests, and fork inherits it, so reset will not
   recursively run the suite.
2. Replace all `fopen`, absolute-path, line scanning, and SKIP behavior in
   `test_undo_rows_us_enabled` with runtime assertions:

```c
(indexOfItems[ITM_FORTH].status & US_STATUS) == US_ENABLED
(indexOfItems[ITM_FCALL].status & US_STATUS) == US_ENABLED
```

   Fail if either differs, printing the actual masked status. Do not read
   `items.c` at runtime or in this task.

**Tests that encode the old contract.** `test_lifecycle_reset` remains; it has a
different direct-init contract. Re-aim `test_undo_rows_us_enabled` rather than
adding a second filesystem test.

**Facts the harness forces on you.** `CONFIRMED` is 9877;
`doNotLoadAutoSav` is false; `doFnReset(uint16_t, bool_t)` is public. The child
must use `_exit`, not `exit`, to avoid flushing duplicated parent buffers.

**Gate:** build-test green. Report the arena high-water line.
*Mutation: UNVERIFIED — R2 verified that deleting the production reset hook
left the old gate GREEN. After adding the fork test, delete only that one call;
the new test must go RED with a nonzero child status, then restore it manually.
For the UNDO test, temporarily clear `US_ENABLED` from one runtime table entry
inside the test setup and require its exact FAIL label, then restore.*

**Report:** paste child exit behavior under the hook deletion, the runtime
statuses, final green banners, and arena line.

---

## R2-T4 — Make program-step fixtures exercise the program and scan state they claim

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep and read at most 60 lines from each of
`test_program_step_define_and_use`, `test_prescan_two_programs_first_touch`,
`test_exec_step_marker_noop`, `test_exec_step_source_runs`, and
`test_exec_step_halts_on_error`; also read the definitions of
`writeTestProgram` and `cleanupTestProgram` only.

**The defect.** The two-program test never performs the promised third touch,
and its first program defines no word, so even adding the touch would not expose
a single-slot scan cache. The source-execution tests do not seed X, so they are
order-sensitive. The error test's length says 4 while its comment and byte array
say `3 SQX` (5 bytes); it actually executes `3 SQ` and ignores the final `X`.
It also passes a stack buffer although the Architecture-2 pre-scan contract
requires a real program. The marker test checks only X and dictionary count,
not its claim that the stack is untouched.

**The change.** Implement these exact repairs:

1. Replace the two-program fixture with two programs separated by `ITM_END`:
   program 1 has one Forth step `: P1W 9 ; P1W` (length 13); program 2 has one
   Forth step `: P2W 4 ; P2W` (length 13). Touch P1, then P2, then P1 again in
   one generation. After P1 require X=9/count=1; after P2 require X=4/count=2;
   capture `fdict.here`; after the third P1 require X=9, count still 2, and here
   unchanged. Compute step offsets from named `sizeof` constants or explicit
   header+payload sizes, not magic values copied from the old fixture.
2. Immediately before the execution under test in
   `test_program_step_define_and_use` and `test_exec_step_source_runs`, call
   `forthPushInt32(-123456)`. Retain the X==9/type assertions. A dropped
   handler must leave the canary, not an earlier test's 9.
3. Rebuild `test_exec_step_halts_on_error` with
   `writeTestProgram` and exactly one source step whose length is 5 and payload
   is `3 SQX`. Call `executeOneStep(beginOfProgramMemory)`. Require
   `ERROR_FUNCTION_NOT_FOUND`, then cleanup on every path. Do not define SQ and
   do not use a stack-local fake step.
4. Strengthen `test_exec_step_marker_noop`: seed all four stack registers with
   distinct long integers using four `forthPushInt32` calls, snapshot their
   values/types, execute the marker, and compare all four registers plus
   `fdict.count` and `fdict.here`. Add a small test-local helper for reading a
   specified stack register if necessary; do not add production code.

**Tests that encode the old contract.** Re-aim the five named tests. None of
these changes production behavior.

**Facts the harness forces on you.** `writeTestProgram` appends `.END.`. An
explicit `0x85,0xB2` separates the two programs; it is not a duplicate final
terminator. Forth step header bytes are `0x8B,0x1A,0xFD,len`. A scan-cache
mutation can only be observed if the re-scanned program contains a definition.

**Gate:** build-test green. Report the arena high-water line.
*Mutation: UNVERIFIED — after the change, temporarily reduce the scanned-program
cache to one remembered pointer; the third P1 touch must make count 3/here grow.
Separately omit each execution call; its seeded canary must produce the named
RED label. Change the error-step length back to 4; the test must fail because
`3 SQ` is not `3 SQX`. Run and report each before reverting manually.*

**Report:** paste the exact two-program bytes/offset calculation, all mutation
symptoms, final green banners, and arena line.

---

## R2-T5 — Make FWRD menu tests pin ordering, boundaries, refresh, and deterministic storage

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep and read at most 60 lines from each of
`test_picker_scan_basic`, `test_picker_omits_long_names`,
`test_picker_long_token_skipped`, and `test_softmenu_trailing_null`; read the
existing `test_dynamic_menu_registration` and its helper declarations only.

**The defect.** `test_picker_scan_basic` says “sorted” but checks only set
membership. `test_picker_omits_long_names` says it keeps a 14-byte name but its
kept name is `SHORT` (5 bytes). R2 removed qsort and changed the accepted bound
from 14 to 13; the full suite stayed GREEN. R2 also removed the special
rebuild-always condition for MNU_FORTH; the suite stayed GREEN, so no test
reaches the cache behavior required for newly authored words. The long-token
test admits that its unchecked-copy mutation passes without ASan, while the
only sanctioned gate is non-ASan. By contrast, `calloc → malloc` did make
`test_softmenu_trailing_null` RED (`count=1, byte=100`), so that test is a real
but allocator-state-dependent tooth.

**The change.** Make these exact test repairs without changing production code:

1. In `test_picker_scan_basic`, assert exact order: item 0 is `CUBE`, item 1 is
   `SQ`, and the byte after the second string terminator is zero. Keep the
   membership diagnostics only if they add information.
2. In `test_picker_omits_long_names`, replace `SHORT` with the distinct
   14-byte name `KEEPABCDEFGHIJ`; set its source-step length to 18 and adjust
   the closing-marker offset from header/payload sizes. Require exactly that
   full 14-byte name and exactly one item. It must not share the first 14 bytes
   of either rejected 15-byte name.
3. Add `test_picker_rebuilds_same_menu`: create a real program with marker and
   `: ONE 1 ;`; display/build MNU_FORTH through the same public softmenu path
   used by the UI; prove ONE is listed; append or replace the real program so a
   second definition `: TWO 2 ;` is before currentStep without closing/reopening
   another menu; invoke that same public path again while MNU_FORTH is still the
   cached menu; require TWO immediately. Save/restore the entire softmenu stack,
   cached-menu-visible state exposed to tests, currentStep/program number, and
   dynamic allocation. If the needed cached variable is file-static, add a
   `FORTH_SELFTEST_EXPORT` wrapper beside the existing softmenu test wrappers;
   it must compile to `static` outside `FORTH_DEBUG_SELFTEST` and must not alter
   production linkage or bytes.
4. Do not claim that `test_picker_long_token_skipped` catches an unchecked
   copy. Rename its comment to say it pins semantic omission only. Add a
   deterministic debug-only copy-bound test by using a self-test wrapper around
   the exact picker token-copy helper with pre/post canaries. If the production
   code has no separable helper, STOP and report instead of inventing an ASan
   command or relying on stack corruption; the sanctioned gate cannot verify
   that mutation.
5. Keep `test_softmenu_trailing_null`. Before calling the builder, allocate and
   free several same-size blocks filled with `0xA5` so the test is not relying
   on a freshly zeroed heap page. The assertion remains the exact final zero.

**Tests that encode the old contract.** Re-aim the four named tests and add the
one rebuild test. No behavior contract changes.

**Facts the harness forces on you.** Menu entries are 15-byte slots: at most 14
name bytes plus NUL. The expected sorted order is binary `CUBE`, then `SQ`.
The final menu buffer is a sequence of NUL-terminated names plus one additional
NUL. Do not read `softmenus.c` beyond a 60-line slice anchored at `case
MNU_FORTH` if a wrapper anchor must be confirmed.

**Gate:** build-test green. Report the arena high-water line.
*Verified mutation:* R2 ran qsort deletion, `<=14 → <=13`, and deletion of the
MNU_FORTH rebuild-always term; all were falsely GREEN. R2 ran `calloc → malloc`;
the existing trailing-NUL test was RED with `byte=100`. *Mutation: UNVERIFIED —
the new exact-order, true-14-byte, and same-menu refresh tests were not
temporarily installed; run each named mutation after implementation and report
its exact RED label. The unchecked-copy mutation is explicitly UNVERIFIED unless
the deterministic canary wrapper can be built under the sanctioned gate.*

**Report:** paste the exact order/boundary assertions, whether a safe wrapper
was possible, each mutation symptom, final green banners, and arena line.

---

## R2-T6 — Stop PEM tests from priming or leaking the global state under test

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep and read at most 60 lines from each of
`test_toggle_inserts_marker`, `test_alpha_menu_on_top_during_capture`,
`test_forth_toggle_from_catalog_leaves_alpha_menu`, and
`test_forth_drain_clears_buried_catalog`; also read
`test_fcall_redirect_records_name`; grep all assignments to
`tam.function` and `fnKeyInCatalog`, but do not read unrelated functions.

**The defect.** `test_alpha_menu_on_top_during_capture` assigns
`tam.function = ITM_FORTH` before calling the path that is supposed to derive
it and never restores the prior value. `test_toggle_inserts_marker` also does
not save/restore tam.function between or after its opening and closing cases.
The two catalog tests set `fnKeyInCatalog = 1` legitimately to model the static
keyboard caller, but restore it to 0 rather than its incoming value. Test order
therefore affects later state. `test_fcall_redirect_records_name` prints through
`s + 4` after `cleanupTestProgram()` has released/restored the program region;
that PASS diagnostic dereferences a stale fixture pointer. R2 confirmed these
by source audit; no probe was left.

**The change.** Make test isolation exact:

1. In both subcases of `test_toggle_inserts_marker`, save incoming
   `tam.function`, set it to 0 before the call, and restore it in cleanup.
   In the opening case require the real call leaves `tam.function == ITM_FORTH`.
   Do not assert closing-case tam.function until the architect resolves whether
   E1 must clear the sentinel.
2. In `test_alpha_menu_on_top_during_capture`, save tam.function, seed it to 0
   rather than ITM_FORTH, require the call derives ITM_FORTH, and restore it on
   every return path.
3. In each test that assigns `fnKeyInCatalog`, capture
   `bool_t savedFnKeyInCatalog` before setup and restore that exact value after
   `_closeCatalog`; do not write a hardcoded 0. Preserve the required ordering:
   set it only after `showSoftmenu`, immediately before `runFunction`.
4. Audit every early return/goto in the four functions so cleanup restores
   currentStep, program number/local step, catalog, calcMode, FLAG_ALPHA,
   tam.function, fnKeyInCatalog, and the softmenu stack values that the function
   changed. Do not introduce a generic global reset that could hide leaks.
5. In `test_fcall_redirect_records_name`, print/copy the recorded name before
   `cleanupTestProgram`; never dereference `s` afterward. Preserve all byte
   assertions and cleanup.

**Tests that encode the old contract.** The newest
`test_forth_capture_survives_keystroke` and
`test_forth_alpha_gesture_resumes_forth` already seed tam.function to 0 and
derive it; leave them unchanged.

**Facts the harness forces on you.** The catalog tests cannot call static
`executeFunction`; their exact reachable approximation is
`runFunction(func); _closeCatalog();`. `_closeCatalog` is already exposed with
`FORTH_SELFTEST_EXPORT`; do not invent another simulation.

**Gate:** build-test green. Report the arena high-water line.
*Mutation: UNVERIFIED — seed each saved global to a distinctive non-default
value in the runner immediately before its test and assert the value afterward;
the pre-fix test must leak and go RED, the fixed test must preserve it. Run that
probe for both tam.function and fnKeyInCatalog, then remove it manually.*

**Report:** paste the pre-fix leak values, post-fix preservation, final green
banners, and arena line.

---

## R2-T7 — Correct false test claims and add the missing item-over-colon precedence tooth

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep and read at most 60 lines from each of
`test_xeq_end_to_end`, `test_xeq_item_lookup`,
`test_useritem_xeqp1_decodes`, and `test_alpha_menu_contains_fwrd`; grep all
literal occurrences of `§9` and `[VERIFIED:` in this test file only.

**The defect.** Several comments make claims their code does not establish:

- `test_xeq_end_to_end` directly calls `fnForthCall`; it does not execute an
  XEQ step, ITM_FORTH, or both new items as its name/header says.
- `test_xeq_item_lookup` proves item lookup, but no test proves the required
  item-before-colon ordering.
- the decode citation says `decode.c:866-869`, while complete two-byte opcode
  reconstruction is at the current anchor spanning the initial byte through
  the low-byte OR (physical lines 868-873 during R2).
- the alpha-menu citation says `softmenus.c:1000`, a blank line; the MNU_FORTH
  row is at the `menu_ALPHA` anchor immediately above (physical line 999 during
  R2).
- all test-file references to §9 are stale. Current normative replacements are
  exact: §9.4→§8.4, §9.5→§8.5, §9.9→§8.9, §9.10→§8.10.

**The change.** Do only these decisions-free repairs:

1. Rename `test_xeq_end_to_end` to
   `test_fnforthcall_executes_colon_by_index` and rewrite its header to describe
   only what it calls/asserts. Update runner/debug labels.
2. Extend `test_xeq_item_lookup`: define a Forth colon word named `SIN`, call
   `forthResolveXEQ("SIN", &param)`, and require `FORTH_XEQ_ITEM` with
   `param == ITM_SIN`. This pins item-before-colon. Clear the dictionary before
   and after this subcase.
3. Replace the two physical-line `[VERIFIED:]` citations with grep anchors:
   `programming/decode.c, opCode reconstruction from first byte through low-byte
   OR` and `softmenus.c, menu_ALPHA row containing -MNU_FORTH`. Do not install
   new numeric line decorations.
4. Perform only the four exact § replacements listed above throughout this
   test file.
5. Add a short TODO beside the renamed fnForthCall test:
   `Missing acceptance coverage: real XEQ execution of ITM_FORTH and ITM_FCALL;
   fixture deferred pending an independently verified reachable path.` Do not
   improvise a simulation in this task.

**Tests that encode the old contract.** Re-aim the renamed test and extend
`test_xeq_item_lookup`; no production contract changes.

**Facts the harness forces on you.** `ITM_SIN` is the real item identifier; do
not use an ASCII character or guess a number. Resolver precedence is
label→item→colon. This task pins only item→colon because the existing
`test_xeq_precedence` pins label→colon.

**Gate:** build-test green. Report the arena high-water line.
*Mutation: UNVERIFIED — after adding the SIN subcase, temporarily swap the
item and colon lookup order in `forthResolveXEQ`; the new subcase must go RED
reporting COLON. Revert manually and rerun green. Citation/comment changes have
no meaningful RED mutation; report them as documentation corrections, not
coverage.*

**Report:** paste the resolver mutation symptom, all renamed/replaced anchors,
final green banners, and arena line.

---

## R2-T8 — Run the integrated gate, report the high-water mark, and commit once

**File(s):** only files changed by R2-T1 through R2-T7 and their generated
package metadata/output.

**Read:** `git status --short`; `git diff --stat`; grep the final self-test log
for `FORTH ARENA`, `FORTH SELF-TEST`, and `BUILD + SELF-TEST` only. Do not read
large source files again.

**The defect.** The preceding tasks intentionally remain uncommitted so they
can be reviewed and mutation-tested as one test-suite repair. This task is the
single integration/commit point.

**The change.** Confirm every prior task reported its required RED mutation or
explicitly reported `UNVERIFIED` and stopped. Search the working area (excluding
generated `patches/` and `files/`) for `AUDIT-PROBE`; there must be none. Run
`./packages/forth-core/build-test.sh` once. Require exit 0 and both exact green
banners. Report every `FORTH ARENA` line and specifically the maximum `here`,
`sizeBlocks`, and `freeRamDelta`; compare with the R2 baseline
`here=36 sizeBlocks=16 freeRamDelta=64`. Run `git diff --check`. Stage only the
specific working test file plus generated files that `build-test.sh` refreshed;
never use `git add -A`. Expected paths are:

```text
packages/forth-core/test_dict_reloc.c
packages/forth-core/files/test_dict_reloc.c
packages/forth-core/.refresh-manifest.json
```

If `git status` lists any other path, STOP and report it rather than staging.
Commit once with message `forth-core: make self-tests mutation-sensitive`.

**Tests that encode the old contract.** All migrations are contained in their
own tasks; none are deferred to this integration task.

**Gate:** build-test green.
*Verified mutation:* N/A — this is the integration task. It is valid only if
each test-changing task supplied its own mutation result; do not substitute the
ordinary green run for those RED proofs.

**Report:** paste `git status --short` before staging, the green banners, every
arena line and computed high-water maximum, `git diff --check`, the exact paths
staged, and the commit hash.

---

## Appendix A — all 117 tests and the mutation that makes each RED

Legend: **V** = mutation was run during R2; **S** = static audit only (a precise
hypothesis, not executed); **D** = the current test has no credible mutation or
does not prove the claim; **C** = R2 ran a production mutation and the current
test caught it. “Task” points to the repair above.

- `test_stack_aslift` — **V/D:** clear ASLIFT is RED, but deleting DUP/PLUS was GREEN; Task T1.
- `test_branch_fwd` — **V/D:** delta 1→0 was GREEN; the T1 Y canary made it RED.
- `test_branch_back` — **S:** change backward delta -9 by one cell; final X/error assertion should fail.
- `test_0br_longint` — **S:** treat dtLongInteger zero as nonzero; expected branch result fails.
- `test_0br_consumes` — **S:** leave the condition on stack; its Y-stack assertion fails.
- `test_0br_longint_taken_branch` — **S:** use raw real34 zero test; X will not become 42.
- `test_literal_after_lit` — **V/D:** delete live ILIT/DROP; gate stayed GREEN; Task T1.
- `test_lit_roundtrip` — **S:** advance LIT payload by 8 instead of `sizeof(real34_t)`; X/error fails.
- `test_c47_ptp_none` — **S:** reject PTP_NONE in the C47 primitive; expected result fails.
- `test_c47_ptp_number8_padded` — **S:** omit NUMBER_8 padding/advance; decoded value or continuation fails.
- `test_c47_bad_ptp` — **S:** accept an unknown PTP; exact error assertion fails.
- `test_c47_nested_call_succeeds` — **S:** suppress the nested C47 call; X==42 fails.
- `test_nested_preserves_outer_rstack` — **S:** reset the outer return stack during nested call; continuation fails.
- `test_nested_error_unwinds_rsp` — **S:** omit error unwind; saved rsp assertion fails.
- `test_outer_real_literal` — **S:** reject/retag the real literal; X type/value fails.
- `test_div_zero_halt` — **V:** initial 42→43 stayed GREEN, disproving its comment; sentinel/error mutation remains toothed; Task T1.
- `test_rstack_overflow` — **V/D:** omit A0; gate stayed GREEN; exact ERROR_RAM_FULL assertion makes RED; Task T1.
- `test_runaway_guard` — **V/D:** omit RR2; gate stayed GREEN; exact ERROR_RAM_FULL assertion makes RED; Task T1.
- `test_malformed_token` — **V/D:** omit MP/MC/MR; all stayed GREEN; exact error assertions make RED; Task T1.
- `test_ilit_sign_extend` — **S:** decode negative ILIT as unsigned; expected negative X fails.
- `test_ilit_arithmetic_divergence` — **S:** push ILIT as real34; integer arithmetic/type assertion fails.
- `test_br_delta_sign_extend` — **V/D:** decode +260 as +4; DROP filler still reached 777 and gate stayed GREEN; Task T1.
- `test_ilit_compile_interpret_parity` — **S:** compile integer literals through the real-number path; type/value parity fails.
- `test_outer_simple_expr` — **S:** omit PLUS dispatch; expected expression value fails.
- `test_outer_compile_invoke` — **S:** skip colon finalization/invocation; lookup or result fails.
- `test_outer_nonstring_x` — **S:** accept non-string X; exact error assertion fails.
- `test_outer_glyph_cross` — **S:** remove CROSS alias; function-not-found/result assertion fails.
- `test_outer_glyph_dot` — **S:** advance tokenizer byte-wise through the glyph; token/result fails.
- `test_outer_glyph_divide` — **S:** remove DIVIDE alias; function-not-found/result assertion fails.
- `test_outer_nesting_tokenizer` — **S:** reset tokenizer context on nested call; outer continuation fails.
- `test_outer_depth_cap` — **S:** remove depth cap; expected guard becomes hang/wrong error.
- `test_outer_ctx_at_rest` — **S:** omit context unwind; at-rest state assertion fails.
- `test_xeq_end_to_end` — **D:** it calls fnForthCall directly and never drives XEQ/ITM_FORTH; rename in T7.
- `test_tam_dispatcher` — **S:** route TAM colon selection to the wrong function; result fails.
- `test_reentrancy` — **S:** allow forbidden nested entry; exact error/context assertions fail.
- `test_xeq_precedence` — **S:** resolve colon before label; label-shadow result fails.
- `test_xeq_item_lookup` — **S:** skip item lookup; current item cases fail; item-over-colon gap fixed in T7.
- `test_fnforthcall_interactive` — **S:** remove interactive fnForthCall execution; X result fails.
- `test_lblq_undefined_no_ub` — **V/D:** returns 0 unconditionally; resolvedParam leak stayed GREEN; replace in T2.
- `test_lifecycle_pre_init` — **S:** remove `fdict.base` guard; NULL access/bogus-found assertion fails.
- `test_lifecycle_reset` — **V/D:** delete production reset hook; direct-init test stayed GREEN; Task T3.
- `test_dict_name_too_long` — **S:** accept overlength dictionary name; rejection/error assertion fails.
- `test_dict_space_full` — **S:** ignore arena-full return; exact full/error invariant fails.
- `test_number_then_no_label_fallthrough` — **S:** permit numeric parse to fall through label lookup; result/error fails.
- `test_prefix_no_match` — **S:** accept a name prefix as a word; not-found assertion fails.
- `test_number_1e_minus_5` — **S:** reject exponent-minus syntax; value/type assertion fails.
- `test_number_bad_e5` — **S:** accept missing exponent digits; unchanged-stack/error assertion fails.
- `test_number_bad_dot_e5` — **S:** accept malformed dot/exponent; unchanged-stack/error assertion fails.
- `test_number_bad_3e` — **S:** accept trailing exponent marker; unchanged-stack/error assertion fails.
- `test_number_bad_lone_dot` — **S:** accept lone dot; unchanged-stack/error assertion fails.
- `test_undo_rows_us_enabled` — **D:** missing absolute source path becomes SKIP/pass; replace with runtime table checks in T3.
- `test_forth_step_ptp_rem` — **S:** use a non-REM/string PTP for the step; byte assertion fails.
- `test_forth_step_sizing` — **S:** change encoded source-step length; step-boundary assertion fails.
- `test_program_step_define_and_use` — **S/order-sensitive:** drop handler; X==9 should fail but X is unseeded; seed in T4.
- `test_program_step_gen_reset` — **S:** omit generation reset; dictionary generation/count assertion fails.
- `test_prescan_forward_reference` — **S:** execute before full pre-scan; forward lookup/result fails.
- `test_prescan_no_early_tail` — **S:** execute the tail during pre-scan; stack sentinel math fails.
- `test_prescan_no_recompile` — **S:** recompile on second touch in one generation; count/here grows.
- `test_prescan_owning_scope` — **S:** scan outside owning program; foreign word/count assertion fails.
- `test_prescan_generation_rearm` — **S:** retain scanned state across generation bump; expected rearm result fails.
- `test_prescan_error_halts` — **S:** continue scan after compilation error; error/dictionary invariant fails.
- `test_prescan_last_step_visible` — **S:** use an exclusive final-step bound; final definition/result fails.
- `test_prescan_two_programs_first_touch` — **D:** promised third P1 touch is absent and P1 defines nothing; Task T4.
- `test_dict_name_by_index` — **S:** offset index/link walk by one; exact names/count-boundary fails.
- `test_exec_step_marker_noop` — **D/partial:** call source handler on marker may alter unobserved stack/flags; only X/count checked; Task T4.
- `test_exec_step_source_runs` — **S/order-sensitive:** drop ITM_FORTH call; X==9 should fail but seed is not controlled; Task T4.
- `test_exec_step_halts_on_error` — **D:** length 4 executes `3 SQ`, not claimed `3 SQX`, from a fake stack step; Task T4.
- `test_marker_parity` — **S:** invert even/odd parity; all direction assertions fail.
- `test_entry_state_derivation` — **S:** derive from current instead of predecessor step; region-state cases fail.
- `test_toggle_inserts_marker` — **S/leak:** force opening on close; FLAG_ALPHA assertion fails; tam.function is not isolated; Task T6.
- `test_fcall_redirect_records_name` — **S:** record ITM_FCALL/index instead of name bytes; byte probe fails; its PASS log also dereferences `s` after cleanup (T6).
- `test_fcall_redirect_rejects_stale` — **S:** accept stale dictionary index; rejection assertion fails.
- `test_forth_empty_enter_leaves_no_step` — **S:** commit empty placeholder; step-count/parity assertion fails.
- `test_forth_edit_extracts_source` — **S:** use generic quoted decode for edit; aimBuffer/cursor assertion fails.
- `test_decode_marker_directions` — **S:** invert marker parity; decoded marker strings fail.
- `test_decode_source_bare` — **S:** restore `FORTH '…'` rendering; exact bare output fails.
- `test_mnu_forth_row` — **S:** remove/rename FWRD row; menu metadata assertion fails.
- `test_dynamic_menu_registration` — **S:** remove MNU_FORTH registration; menu id/content assertion fails.
- `test_static_menu_integrity` — **S:** shift a fixed menu entry; table invariants fail.
- `test_picker_scan_basic` — **V/D:** qsort deletion stayed GREEN; exact-order repair in T5.
- `test_picker_omits_long_names` — **V/D:** `<=14 → <=13` stayed GREEN because kept name is 5 bytes; Task T5.
- `test_picker_dedupes` — **S:** remove duplicate suppression; numItems becomes 2.
- `test_picker_insert_at_cursor` — **S/paired:** omit insertion; buffer/cursor fails; mid-line semantics are pinned by the next test.
- `test_picker_insert_mid_line` — **S:** append at buffer end instead of cursor; exact buffer/cursor fails.
- `test_picker_trailing_space` — **S:** omit separator space; exact buffer fails.
- `test_picker_guard_menu_identity` — **S:** remove menu-identity conjunct; wrong-menu case becomes true.
- `test_picker_glyph_tokenize` — **S:** advance byte-wise; glyph-containing name splits and assertions fail.
- `test_picker_long_token_skipped` — **D:** semantic omission is checked, but its unchecked-copy mutation passes non-ASan; Task T5.
- `test_program_memory_no_overlap` — **S:** allocate dictionary into program region; overlap invariant fails.
- `test_cleanup_no_overlap` — **S:** omit cleanup/free-list reconciliation; overlap/allocation invariant fails.
- `test_softmenu_trailing_null` — **C:** `calloc → malloc` went RED with `count=1, byte=100`; harden determinism in T5.
- `test_e2_continuation_after_enter` — **S:** use cursor rather than insertion predecessor after ENTER; continuation/tam assertion fails.
- `test_e2_not_inside_rpn_gap` — **S:** treat RPN predecessor as Forth region; unexpected capture assertion fails.
- `test_gto_word_errors` — **S:** let GTO fall back to a colon word; exact label-not-found behavior fails.
- `test_gto_item_errors` — **S:** let GTO fall back to an item; exact label-not-found behavior fails.
- `test_xeq_word_still_calls` — **S:** remove XEQ colon fallback; X/result fails.
- `test_useritem_xeqp1_opcode` — **S:** emit XEQ instead of XEQ+1; opcode-byte assertion fails.
- `test_useritem_xeqp1_decodes` — **S:** reconstruct only one opcode byte; decoded item assertion fails; citation fixed in T7.
- `test_e1_direction_mid_program` — **S:** derive toggle direction from the wrong adjacent step; marker/capture assertions fail.
- `test_forth_multiline_lock_holds` — **S:** remove non-empty ENTER relock; next-line capture assertion fails.
- `test_tam_function_cleared_after_abort` — **S:** remove abort clear; stale ITM_FORTH assertion fails.
- `test_freelist_consistent` — **S:** corrupt coalescing/order; region-count/size invariant fails.
- `test_freelist_double_free_guarded` — **S:** remove exact double-free guard; free-list snapshot changes (expected diagnostic is not failure).
- `test_freelist_interior_double_free` — **S:** remove interior-overlap guard; free-list snapshot changes (expected diagnostic is not failure).
- `test_freelist_no_mutation_on_oversize_free` — **S:** mutate list before oversize rejection; snapshot changes (expected diagnostic is not failure).
- `test_alpha_menu_on_top_during_capture` — **S/leak:** force FWRD on top; menu assertion fails, but tam.function is primed/leaked; Task T6.
- `test_alpha_menu_contains_fwrd` — **S:** remove FWRD row; membership assertion fails; citation fixed in T7.
- `test_forth_toggle_from_catalog_leaves_alpha_menu` — **S:** restore catalog overlay after dispatch; top-menu/alpha assertions fail; fnKey state cleanup in T6.
- `test_forth_drain_clears_buried_catalog` — **S:** omit buried-catalog drain; SYSFL/catalog remains and assertions fail; fnKey cleanup in T6.
- `test_forth_capture_survives_keystroke` — **S/clean:** restore unconditional ITM_LITERAL; derived tam.function assertion fails; state is not primed.
- `test_forth_alpha_gesture_resumes_forth` — **S/clean:** delete insertion-state AIM arm; tam.function remains non-Forth and assertion fails; state is not primed.
- `test_unterminated_def_errors` — **S:** accept missing semicolon; exact error/no-definition assertion fails.
- `test_overlong_token_in_def_keeps_error` — **S:** clear the first compile error while recovering; exact retained error fails.
- `test_save_restore_roundtrip` — **S:** omit dictionary serialization field/data; restored names/results fail.
- `test_restore_missing_params_defaults` — **S:** treat missing Forth parameters as fatal/nondefault; default-state assertion fails.
- `test_restore_validation_clamps` — **S:** trust corrupt saved here/count; empty-reset bounds assertion fails.
- `test_validate_direct_corruption` — **S:** omit size/nameLen validation; direct corruption is accepted instead of reset.

R2 found no fixture duplication bug involving `.END.`: explicit `ITM_END`
bytes in these fixtures separate or terminate C47 programs, while
`writeTestProgram` appends the final `.END.` sentinel. The three free-list guard
diagnostics are expected and were not attributed by log adjacency.
