# R2 test-suite credibility — Qwen implementation prompts

15 tasks, strictly ordered. Each is sized for a ~100k-token context window: it
names the only files and anchors to read, and never requires reading DESIGN.md.

**How to use:** paste the PREAMBLE, then one task block, into a fresh Qwen
session. Do not run tasks out of order.

**Owner prerequisite:** the reviewed working tree contains unfinished,
pre-existing audit-probe work at the runner and EOF of
`packages/forth-core/test_dict_reloc.c`. The exact working-tree gate does not
compile: `audit_probe_marker_edit_leak`, `audit_probe_c47_fixed_param`,
`audit_probe_parameterized_resolver`, and `audit_probe_first_ensure_capacity`
are called without definitions, and `audit_probe_buried_catalog` ends after
`showSoftmenu(-MNU_CLK);` with no closing brace or `#endif`. Do not let Qwen
delete, complete, or reinterpret that foreign WIP. The owner must first finish
it or supply its intended final text. Tasks below assume that prerequisite has
been resolved and the baseline compiles.

The reviewer temporarily replaced only those incomplete probe hunks with their
previous runner/EOF shape to execute mutations, then restored the original WIP.
That normalized baseline still failed `test_truncated_inline_operand` with
`error 0, expected corrupted-data 18`. No production change from this review
remains in the tree.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes`. If not,
   STOP.
2. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success = `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.` and exit 0. Never invoke meson or ninja.
3. All edits go in `packages/forth-core/`. Never edit `src/`. Never hand-edit
   generated `patches/` or `files/`; `build-test.sh` refreshes them.
4. Never touch `src/c47/core/freeList.c` or any copy. Never read DESIGN.md,
   DESIGN-HISTORY.md, or a large source in full. Never read `items.c` at all;
   use `grep -a`. Read `test_dict_reloc.c`, `manage.c`, `keyboard.c`, and
   `softmenus.c` only at the anchors named by the task, at most 60 lines per
   slice.
5. Match surrounding style. Keep production changes minimal: flash/RAM are the
   binding constraints.
6. Do not commit unless the task explicitly tells you to. Never use `git stash`,
   `git reset`, `git checkout --`, or `git restore`; never use `git add -A`.
   Reverse a temporary mutation only with an explicit inverse `apply_patch`.
7. If an anchor differs, a named old-contract test fails, or the owner
   prerequisite above is unresolved, STOP and report. Never weaken a test to
   make a production regression pass.
8. For any dictionary test, paste the `FORTH ARENA:` line in the report.
9. The three free-list guard tests intentionally print `errorf` diagnostics.
   Do not attribute diagnostics by adjacent buffered output; use a debugger
   backtrace if attribution matters.
10. Report edits, the exact gate result, every mutation result, and surprises.

For a mutation-only task, apply one listed mutation, run the sanctioned gate,
record the named RED symptom, and immediately apply the explicit inverse patch.
Do not leave a mutant in the tree. `UNVERIFIED` below means the reviewer did not
execute it; it is a required part of that Qwen task, not evidence already held.

---

## R2-01 — Bound every inline dictionary operand

**File(s):** `packages/forth-core/forth_inner.c`,
`packages/forth-core/test_dict_reloc.c`

**Read:** grep `static inline ftoken_t readToken`, `case FTOK_LIT`,
`case FTOK_ILIT`, `case FTOK_BR`, `case FTOK_0BR`, `case FTOK_C47`, and
`test_truncated_inline_operand`; read at most 60 lines around each anchor.

**The defect.** A structurally valid restored word may end immediately after an
inline opcode. `forthInner` checks neither the token fetch nor inline payloads
against the logical end `fdict.here`; `test_truncated_inline_operand` constructs
an ILIT with no four-byte operand. On the normalized review baseline the test
was RED: `truncated ILIT returned error 0, expected corrupted-data 18`.

**The change.** Add one `static inline bool_t` range predicate beside
`readToken`. It accepts an offset and byte count and returns true only when
`fdict.base != NULL`, `offset <= fdict.here`, and
`byteCount <= fdict.here - offset`; use subtraction so addition cannot wrap.
Before every dictionary read, require the exact number of bytes:

- token fetch: 2;
- `FTOK_LIT`: `sizeof(real34_t)`;
- `FTOK_ILIT`: 4;
- `FTOK_BR` and `FTOK_0BR`: 2;
- `FTOK_C47` item id: 2;
- `PTP_NUMBER_8` and `PTP_NUMBER_16`: 2, including the NUMBER_8 pad byte.

On failure, set `lastErrorCode = ERROR_INVALID_CORRUPTED_DATA`, call the same
`displayCalcErrorMessage` form already used for a corrupt call target, then
`INNER_LEAVE()` before reading or changing the calculator stack. Keep the
existing ILIT test unchanged.

**Tests that encode the old contract.** None — accepting a truncated restored
definition is not a supported contract.

**Gate:** build-test green.

*Verified mutation:* the review baseline is the escaping mutant: no ILIT bounds
guard. Exact symptom: `FAIL: truncated ILIT returned error 0, expected
corrupted-data 18`; the sentinel-X assertion did not fire.

**Report:** paste the guard sites, exact gate result, mutation result, and arena
line.

---

## R2-02 — Make weak inner-interpreter assertions toothed

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_inner.c`

**Read:** grep each test name in the table and read that function only; in
`forth_inner.c`, grep the token named by its mutation and read ±20 lines.

**The defect.** Five tests accept a silent stop or an execution path that skips
the behavior named by the docstring. Strengthen only these tests:

| # | Test | Escaping mutation now | Required RED symptom after repair |
|---:|---|---|---|
| 2 | `test_branch_fwd` | Decode `BR` as operand-consuming no-op. `DROP` then the trailing ILIT still leaves X=42. | Add a stack/sentinel assertion so the skipped body is observable; mutant must fail that assertion. |
| 7 | `test_literal_after_lit` | Remove/no-op the trailing ILIT and DROP; the test checks only `lastErrorCode`. | Assert the final stack/value effect of the live token; mutant must report the wrong X/depth. |
| 17 | `test_rstack_overflow` | Stop immediately on CALL without setting an error. Any X other than 777 is currently accepted when `lastErrorCode==ERROR_NONE`. | Require `ERROR_RAM_FULL` and unchanged sentinel X=777. |
| 18 | `test_runaway_guard` | Decode the back branch as EXIT/no-op. X remains 444 and no error is accepted. | Require the guard's current `ERROR_RAM_FULL` and unchanged X=444. |
| 19 | `test_malformed_token` | Silently halt each malformed arm without an error or executing sentinel 555. | Require each arm's current error: bad primitive `ERROR_OPERATION_UNDEFINED`; bad call and reserved token `ERROR_INVALID_CORRUPTED_DATA`; retain sentinel checks. |

Do not alter production behavior in this task. For tests 2 and 7, construct the
expected stack explicitly before execution and assert both the intended X and
the stack effect already required by the word; do not rely on a value that a
later instruction overwrites. For tests 17–19 add the exact error assertions
above.

**Tests that encode the old contract.** The five named tests are re-aimed, not
deleted. No production contract changes.

**Gate:** build-test green.

*Mutation: UNVERIFIED — the reviewer proved the assertions are logically
permissive but did not run these five mutants because the exact working tree
does not compile. Run each table mutation and record its named RED symptom.*

**Report:** paste each mutant and the exact assertion that caught it, plus arena.

---

## R2-03 — Verify the remaining inner-interpreter teeth

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_inner.c`

**Read:** grep each test and token/function named below; one ±20-line subject
slice and one test body at a time.

**The defect.** No structural defect was found in these tests, but their
docstring mutations were not executed during review. Verify every row; if a
mutant is green, stop and report instead of improvising a new assertion.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 1 | `test_stack_aslift` | Remove normal-return `setSystemFlag(FLAG_ASLIFT)`. | ASLIFT assertion false. |
| 3 | `test_branch_back` | Reverse/ignore the negative BR delta. | X is not 0 or runaway error. |
| 4 | `test_0br_longint` | Test long integers through `real34IsZero`. | Zero long integer takes wrong arm; X=999 instead of 42. |
| 5 | `test_0br_consumes` | Remove `fnDrop` from `popIsFalse`. | Y remains 7 / stack-effect assertion fails. |
| 6 | `test_0br_longint_taken_branch` | Treat long-integer zero as true. | Wrong arm, X=999 instead of 42. |
| 8 | `test_lit_roundtrip` | Advance LIT by 8 rather than `sizeof(real34_t)`. | Trailing ILIT misdecodes; error or X!=777. |
| 9 | `test_c47_ptp_none` | Always consume a parameter cell for `PTP_NONE`. | Trailing ILIT misdecodes; X!=55 or error. |
| 10 | `test_c47_ptp_number8_padded` | Advance NUMBER_8 by one byte, not its padded cell. | Trailing token misdecodes; X!=33 or error. |
| 11 | `test_c47_bad_ptp` | Dispatch an unsupported PTP instead of rejecting it. | Expected error is absent. |
| 12 | `test_c47_nested_call_succeeds` | Restore a single-level reentrancy refusal. | Nested call errors or X!=999. |
| 13 | `test_nested_preserves_outer_rstack` | Reset `rsp` on nested entry. | Outer tail does not reach X=9 or rstack/depth assertion fails. |
| 14 | `test_nested_error_unwinds_rsp` | Omit nested-entry rsp watermark restore. | `rsp!=0` or follow-up execution fails. |
| 15 | `test_outer_real_literal` | Reject/skip a real literal. | Error, wrong type, or wrong value. |
| 16 | `test_div_zero_halt` | Continue dispatch after divide-by-zero. | Sentinel X=999 executes. |
| 21 | `test_ilit_sign_extend` | Reconstruct byte zero through `int8_t`. | 128/300/-200 assertion fails. |
| 22 | `test_ilit_arithmetic_divergence` | Apply the same sign-extension regression. | Arithmetic result is not 170. |
| 23 | `test_br_delta_sign_extend` | Reconstruct branch delta through unsigned/byte sign extension. | Error or X!=777. |
| 24 | `test_ilit_compile_interpret_parity` | Regress only compiled ILIT reconstruction. | Interpreted and compiled results diverge. |

**The change.** Mutation verification only. Leave no source change if every row
is RED for the stated reason.

**Tests that encode the old contract.** None.

**Gate:** build-test green after all inverse patches.

*Mutation: UNVERIFIED — run every row and record the exact symptom.*

**Report:** mutation/result matrix and arena line.

---

## R2-04 — Verify outer-interpreter behavior without shared-state excuses

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_compile.c`

**Read:** grep the nine test names and the named tokenizer/guard anchor; read
only the function bodies and ±20 subject lines.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 25 | `test_outer_simple_expr` | Remove DUP or `+` dispatch. | X!=6. |
| 26 | `test_outer_compile_invoke` | Skip compilation or invocation. | Error or X!=9. |
| 27 | `test_outer_nonstring_x` | Remove the X-type guard. | Expected type error is absent/wrong. |
| 28 | `test_outer_glyph_cross` | Remove the multiplication-glyph mapping. | Wrong value or lookup error. |
| 29 | `test_outer_glyph_dot` | Remove the dot-glyph mapping. | Wrong parsed value/error. |
| 30 | `test_outer_glyph_divide` | Remove the divide-glyph mapping. | Wrong quotient/error. |
| 31 | `test_outer_nesting_tokenizer` | Reuse one static tokenizer context for nested input. | X!=5 or Y!=3. |
| 32 | `test_outer_depth_cap` | Remove the depth cap, then separately omit context restore on the continuation arm. | Forbidden line executes/no expected error; second mutant leaves wrong X/depth. |
| 33 | `test_outer_ctx_at_rest` | Omit final tokenizer-context restoration. | Context pointer non-NULL or depth nonzero. |

**The defect.** No bad fixture was found in this group; mutations remain
unexecuted. `test_outer_depth_cap` deliberately primes depth through its test
hook because the guard itself is the subject; that is legitimate, not the
derived-state defect seen in the PEM tests.

**The change.** Mutation verification only. Stop on any green mutant.

**Tests that encode the old contract.** None.

**Gate:** build-test green after inverse patches.

*Mutation: UNVERIFIED — exact working baseline was uncompilable.*

**Report:** matrix and arena line.

---

## R2-05 — Replace component tests that claim end-to-end dispatch coverage

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/ui/tam.c`, `packages/forth-core/keyboard.c`,
`packages/forth-core/config.c`

**Read:** grep `test_xeq_end_to_end`, `test_tam_dispatcher`,
`test_xeq_item_lookup`, `test_lifecycle_reset`; in subjects grep
`tamProcessInput`, `_tamProcessInput`, `executeFunction`, `FORTH_SELFTEST_EXPORT`,
and the `forthDictInit` call in the reset path. Read ±30 lines each.

**The defect.** Four test names/docstrings overstate their path coverage:

- #34 `test_xeq_end_to_end` calls `fnForthCall` directly. Deleting XEQ routing
  leaves it green; it proves only the bridge function.
- #35 `test_tam_dispatcher` calls `reallyRunFunction(ITM_FCALL, idx)` directly.
  Deleting the TAM H-hook leaves it green. Public `tamProcessInput(uint16_t)` is
  callable and must be the entry point.
- #38 `test_xeq_item_lookup` says a label shadows an item but creates no label
  collision; reversing that precedence is not tested.
- #42 `test_lifecycle_reset` calls `forthDictInit()` itself after staging the
  word. Deleting the production reset hook in `config.c` leaves it green.

**The change.** Keep the direct component coverage but rename #34 to
`test_fnforthcall_end_to_end`. Add a separate keyboard integration test by
changing the existing `static void executeFunction` declaration and definition
to `FORTH_SELFTEST_EXPORT void executeFunction`, using the already-defined macro
whose production expansion is `static`; declare it `extern` in the self-test.
Drive its real XEQ input state and assert the Forth word result. Re-aim #35 to
enter through public `tamProcessInput`, not `reallyRunFunction`. In #38 create
both a calculator label and a Forth item with the same spelling, then assert the
documented winner. In #42 invoke the actual reset entry point containing the
production `forthDictInit` hook; do not call `forthDictInit` from the test after
staging the word.

If the reset entry point or the exact keyboard state cannot be driven without a
new test-only export, use the same `FORTH_SELFTEST_EXPORT` pattern so production
linkage stays static. Do not simulate the file-static chain with hand-written
state changes.

**Tests that encode the old contract.** Re-aim the four named tests; preserve
their direct component checks under truthful names where specified.

**Gate:** build-test green.

*Mutation: UNVERIFIED — run these four deletions/reversals. Required symptoms:
XEQ integration leaves the result unchanged; TAM dispatch leaves the result
unchanged; collision resolves to the item rather than the label; production
reset leaves the staged word visible.*

**Report:** exact entry points driven, test-only exports, four mutant symptoms,
and arena line.

---

## R2-06 — Verify the remaining H1, lifecycle, and dictionary-error tests

**File(s):** `packages/forth-core/test_dict_reloc.c` and the one small
`forth_*.c` subject named by each test

**Read:** grep each test name; then grep the called subject identifier. Do not
read `items.c`.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 36 | `test_reentrancy` | Remove the reentrancy/depth guard. | Inner call proceeds; expected error/depth assertion fails. |
| 37 | `test_xeq_precedence` | Resolve a colon word before a calculator label. | Label-shadow assertion fails. |
| 39 | `test_fnforthcall_interactive` | Hard-code `fromProgram=true`. | X remains 999 / interactive execution fails. |
| 40 | `test_lblq_undefined_no_ub` | Any production mutation. | **No RED exists:** function prints `DEFERRED` and returns 0 unconditionally. Decoration. Replace it with a real callable-path test or delete it; do not count it. |
| 41 | `test_lifecycle_pre_init` | Remove the pre-init/base-null guard. | Crash, lookup succeeds unexpectedly, or required error missing. |
| 43 | `test_dict_name_too_long` | Accept an overlong name or suppress its error. | Error/visibility assertion fails. |
| 44 | `test_dict_space_full` | Permit allocation past the arena or suppress error. | Unexpected success or missing error. |

**The defect.** #40 is the only defined-and-called test for which no mutation
can make the current body RED. Replace it only if the undefined-label path is
callable from this harness; otherwise delete the test and its runner line and
print no PASS/DEFERRED claim. The other rows need mutation verification.

**The change.** Remove decorative success from #40. Mutation-test the remaining
rows one at a time.

**Tests that encode the old contract.** #40 is delete-or-replace; the rest stay.

**Gate:** build-test green.

*Mutation: UNVERIFIED — #40 is proven structurally incapable of RED; the other
mutants were not executed.*

**Report:** #40 disposition, mutation matrix, arena line.

---

## R2-07 — Make parser and static-source checks fail closed

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_compile.c`

**Read:** grep tests #45–#51 and `test_undo_rows_us_enabled`; read each body
only. Grep parser anchors in `forth_compile.c`; do not read `items.c`.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 45 | `test_number_then_no_label_fallthrough` | Fall through from a parsed number to label lookup. | Function-not-found/wrong W or error assertion. |
| 46 | `test_prefix_no_match` | Compare only the input prefix. | `SQUARE` is incorrectly found. |
| 47 | `test_number_1e_minus_5` | Reject an exponent sign. | Parse error or wrong numeric value. |
| 48 | `test_number_bad_e5` | Accept `E5`. | Expected syntax error absent. |
| 49 | `test_number_bad_dot_e5` | Accept `.E5`. | Expected syntax error absent. |
| 50 | `test_number_bad_3e` | Accept `3E`. | Expected syntax error absent. |
| 51 | `test_number_bad_lone_dot` | Accept `.`. | Expected syntax error absent. |
| 52 | `test_undo_rows_us_enabled` | Rename/reformat the searched source rows so the scanner finds nothing. | **Currently green:** fopen failure or missing row prints SKIP and returns 0. |

**The defect.** #52 is a static text scan with two silent-green exits. Make an
open failure and “row not found” return failure. Keep the runtime PTP test in the
next task; it does not cover US.

**The change.** Change only #52's SKIP returns to FAIL returns, and preserve its
source path/row predicates. Mutation-test all eight rows.

**Tests that encode the old contract.** #52 is hardened; no production contract.

**Gate:** build-test green.

*Mutation: UNVERIFIED — structural inspection proves #52's missing-file/row
mutants green; parser mutants were not run.*

**Report:** eight mutation results and arena line where applicable.

---

## R2-08 — Verify program-step representation and correct prescan fixtures

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_bridge.c`, `packages/forth-core/forth_compile.c`

**Read:** grep tests #53–#64 and `writeTestProgram`; read one test at a time.
In `forth_bridge.c` grep `forthEntryStateAtInsertion`; in `forth_compile.c` grep
the prescan entry point. Read ±30 lines only.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 53 | `test_forth_step_ptp_rem` | Return `PTP_NONE` instead of REM. | Status assertion fails. |
| 54 | `test_forth_step_sizing` | Size source payload as bare opcode/fixed item. | Length/next-step assertion fails. |
| 55 | `test_program_step_define_and_use` | Skip compile or execution of a source step. | Word absent or X wrong. |
| 56 | `test_program_step_gen_reset` | Keep the compiled-generation cache across reset. | Generation/recompile assertion fails. |
| 57 | `test_prescan_forward_reference` | Compile only definitions before the call. | **Currently green:** fixture puts the definition before the call despite claiming a forward reference. |
| 58 | `test_prescan_no_early_tail` | Continue scan past owning program END. | Tail definition becomes visible. |
| 59 | `test_prescan_no_recompile` | Recompile on every touch. | count/here changes. |
| 60 | `test_prescan_owning_scope` | Scan from global start rather than owning program start. | Wrong-scope definition visible/missing. |
| 61 | `test_prescan_generation_rearm` | Ignore generation change. | Reset word not recompiled/visible. |
| 62 | `test_prescan_error_halts` | Continue after prescan compile error. | Tail executes or wrong error. |
| 63 | `test_prescan_last_step_visible` | Stop before the last source step. | Last word absent. |
| 64 | `test_prescan_two_programs_first_touch` | Cache only one global “already scanned” pointer. | **Currently green:** fixture touches P1 then P2 but never touches P1 again. |

**The change.** In #57 put the call step before the `: FWD ... ;` source step,
set `currentStep` to the first call payload, and recalculate offsets from the
actual encoded lengths; do not park on a different step and hand-assign derived
state. In #64 execute P1, then P2, then P1 again and assert the third touch does
not change `fdict.count` or `fdict.here` and does not duplicate/recompile P1.
Mutation-test every row.

**Facts the harness forces on you.** `writeTestProgram` appends only `.END.`
bytes `FF FF`. Explicit `0x85,0xB2` is a real `ITM_END`. Prescan fixtures #60
and #64 use explicit END as a program separator; keep it. Do not add a second
END blindly.

**Tests that encode the old contract.** #57 and #64 are re-aimed; all others
stay.

**Gate:** build-test green.

*Mutation: UNVERIFIED — #57 and #64 escaping mutants are proven from fixture
shape but were not executed; all other rows require execution.*

**Report:** final byte arrays/cursor offsets, mutation matrix, arena line.

---

## R2-09 — Verify marker, entry-state, and FCALL tests on their real bytes

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/forth_bridge.c`, `packages/forth-core/programming/manage.c`

**Read:** grep tests #65–#75; read each function only. Grep
`addStepInProgram` and read the cursor-advance lines plus ±20; grep
`forthEntryStateAtInsertion` and read that function.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 65 | `test_dict_name_by_index` | Return latest name regardless of index. | Name/index assertion fails. |
| 66 | `test_exec_step_marker_noop` | Execute a zero-length marker as source. | Error, X, or dictionary changes. |
| 67 | `test_exec_step_source_runs` | Drop source-step dispatch. | X!=9. |
| 68 | `test_exec_step_halts_on_error` | Continue after source error. | Following sentinel executes / error changes. |
| 69 | `test_marker_parity` | Decode only one marker direction. | Parity assertion fails. |
| 70 | `test_entry_state_derivation` | Derive from current step instead of predecessor. | Direction/state table fails. |
| 71 | `test_toggle_inserts_marker` | Insert one marker direction for both toggles. | Bytes/state assertion fails. |
| 72 | `test_fcall_redirect_records_name` | Store dictionary index instead of name payload. | Byte/name assertion fails. |
| 73 | `test_fcall_redirect_rejects_stale` | Accept a stale dictionary index. | Unexpected step insertion. |
| 74 | `test_forth_empty_enter_leaves_no_step` | Remove `hadText` from the E5 multi-line-lock predicate. | `FAIL: FLAG_ALPHA still set after E3 deletion`. |
| 75 | `test_forth_edit_extracts_source` | Treat a bare source payload as a labelled string. | Extracted text/cursor assertion fails. |

**The defect.** #68 constructs a standalone stack buffer although production
source-step execution is described elsewhere as owning-program scoped. Move it
into a real `writeTestProgram` program before mutation testing, preserving the
same error and following-sentinel assertions. #72 prints `s + 4` after
`cleanupTestProgram`; copy the displayed name to a local buffer before cleanup
or print before cleanup so the PASS path does not dereference freed program
memory.

**Facts the harness forces on you.** `addStepInProgram` advances one step before
inserting. A cursor parked on an RPN step therefore makes that RPN step the
predecessor. The explicit-END toggle fixtures are otherwise correct: END makes
the pre-move a no-op. Marker-only capture fixtures intentionally omit ITM_END;
the appended `.END.` is their insertion point.

**The change.** Correct #68 and the #72 PASS print; mutation-test every row.

**Tests that encode the old contract.** #68 is fixture migration only; #72's
assertions remain.

**Gate:** build-test green.

*Verified mutation:* #74 was executed. Exact RED symptom was
`FLAG_ALPHA still set after E3 deletion`.

*Mutation: UNVERIFIED — rows other than #74 require execution.*

**Report:** #68 byte program/cursor, #72 lifetime fix, matrix, arena line.

---

## R2-10 — Verify rendering and menu registration tests

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/programming/decode.c`, `packages/forth-core/softmenus.c`

**Read:** grep tests #76–#80. In subjects grep the exact menu/opcode identifiers
used by those bodies; read ±20 lines only. Never read `softmenus.c` whole.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 76 | `test_decode_marker_directions` | Render both marker directions identically. | One expected rendering differs. |
| 77 | `test_decode_source_bare` | Restore label-style prefix/suffix around source. | Bare-render string assertion fails. |
| 78 | `test_mnu_forth_row` | Remove/renumber the Forth menu row. | Row identifier/content assertion fails. |
| 79 | `test_dynamic_menu_registration` | Remove dynamic Forth menu registration. | Dynamic lookup/count assertion fails. |
| 80 | `test_static_menu_integrity` | Insert Forth into the wrong static table/terminator position. | Integrity/sentinel assertion fails. |

**The defect.** No fixture defect found. Execute the named mutants.

**The change.** Mutation verification only.

**Tests that encode the old contract.** None.

**Gate:** build-test green after inverse patches.

*Mutation: UNVERIFIED — exact baseline did not compile.*

**Report:** mutation matrix.

---

## R2-11 — Make picker tests prove sorting, bounds, and NULL handling

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/softmenus.c`

**Read:** grep tests #81–#92 and their called picker helper names; read each test
body. In `softmenus.c`, grep those helpers and read ±30 lines; never read the
file whole.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 81 | `test_picker_scan_basic` | Remove sorting. Program order is SQ then CUBE. | **Currently green:** only presence/count are checked. Require exact order CUBE,SQ. |
| 82 | `test_picker_omits_long_names` | Admit names beyond the display limit. | Count/presence assertion fails. |
| 83 | `test_picker_dedupes` | Remove duplicate suppression. | Count/name duplication fails. |
| 84 | `test_picker_insert_at_cursor` | Insert at line start/end instead of cursor. | Exact text/cursor fails. |
| 85 | `test_picker_insert_mid_line` | Append rather than splice. | Exact text/cursor fails. |
| 86 | `test_picker_trailing_space` | Omit/add the separator incorrectly. | Exact text/cursor fails. |
| 87 | `test_picker_guard_menu_identity` | Ignore menu identity. | Predicate table fails. |
| 88 | `test_picker_glyph_tokenize` | Split a glyph token as bytes. | Token/name assertion fails. |
| 89 | `test_picker_long_token_skipped` | Restore unchecked copy. | **Named mutant is green in the sanctioned non-ASAN gate.** Add a canary-bounded fixture/assertion; do not claim ASAN coverage. |
| 90 | `test_program_memory_no_overlap` | Let program allocation overlap dictionary region. | Range assertion fails. |
| 91 | `test_cleanup_no_overlap` | Free/cleanup the wrong region. | Post-cleanup range/free-list assertion fails. |
| 92 | `test_softmenu_trailing_null` | Return `numItems=1` with `menuContent=NULL`. | **Currently green:** terminator check is conditional on non-NULL. Require non-NULL first; initialize the allocation before checking its terminator. |

**The change.** Add exact sorted-order assertions to #81. For #89, surround the
destination with fixed canary bytes and assert both remain unchanged after the
long token; the test must fail deterministically without ASAN. In #92 fail if
`numItems != 1` or `menuContent == NULL`, then assert the initialized trailing
NULL slot. Mutation-test all rows.

**Tests that encode the old contract.** #81, #89, #92 are hardened only.

**Gate:** build-test green.

*Mutation: UNVERIFIED — the three escaping paths were established by control
flow inspection, not an executed mutant; all rows require execution.*

**Report:** exact new assertions/canaries, mutation matrix, arena line.

---

## R2-12 — Verify continuation and GTO/XEQ opcode tests

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/programming/manage.c`,
`packages/forth-core/programming/lblGtoXeq.c`

**Read:** grep tests #93–#100 and read each body. In subjects grep only the
called function/opcode and read ±20 lines; never read `lblGtoXeq.c` whole.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 93 | `test_e2_continuation_after_enter` | Ignore a source predecessor when ENTER continues capture. | Marker/source/state assertion fails. |
| 94 | `test_e2_not_inside_rpn_gap` | Treat an RPN predecessor as inside Forth. | Unwanted continuation/marker assertion fails. |
| 95 | `test_gto_word_errors` | Suppress missing/invalid GTO word error. | Required error absent/wrong. |
| 96 | `test_gto_item_errors` | Resolve an invalid item as a label/word. | Required error absent/wrong. |
| 97 | `test_xeq_word_still_calls` | Route XEQ word through GTO/error path. | X/result or call assertion fails. |
| 98 | `test_useritem_xeqp1_opcode` | Truncate ITM_XEQP1 low byte to 0x2F. | Encoded byte probe fails. |
| 99 | `test_useritem_xeqp1_decodes` | Reconstruct opcode without the high-bit convention. | Rendered item/name differs. |
| 100 | `test_e1_direction_mid_program` | Insert the wrong marker direction mid-program. | Exact bytes/state fail. |

**The defect.** No fixture defect found. #93 parks on appended `.END.` so the
source step is the predecessor; #94 parks on an RPN step, pre-moves to the
closing marker, and therefore makes the RPN step the predecessor. Those setups
match `addStepInProgram` semantics and must not be “simplified.”

**The change.** Mutation verification only.

**Tests that encode the old contract.** None.

**Gate:** build-test green after inverse patches.

*Mutation: UNVERIFIED — exact baseline did not compile.*

**Report:** mutation matrix and arena line.

---

## R2-13 — Stop PEM tests from priming and leaking the state they claim to derive

**File(s):** `packages/forth-core/test_dict_reloc.c`,
`packages/forth-core/programming/manage.c`, `packages/forth-core/keyboard.c`

**Read:** grep tests #101–#111; read each body. In `manage.c` grep
`forthEntryStateAtInsertion`, `tam.function != ITM_FORTH`, and the catalog-menu
drain; in `keyboard.c` grep `_closeCatalog`. Read ±30 lines only.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 101 | `test_forth_multiline_lock_holds` | Clear capture after non-empty ENTER. | tam/alpha continuation assertion fails. |
| 102 | `test_tam_function_cleared_after_abort` | Leave `tam.function=ITM_FORTH` on abort. | Exact tam assertion fails. |
| 103 | `test_save_restore_roundtrip` | Omit one persisted Forth field. | Round-trip field mismatch. |
| 104 | `test_restore_missing_params_defaults` | Leave missing fields uninitialized/non-default. | Default assertions fail. |
| 105 | `test_restore_validation_clamps` | Accept out-of-range restored values. | Clamp assertion fails. |
| 106 | `test_validate_direct_corruption` | Skip direct validation repair. | Corrupt value remains. |
| 107 | `test_alpha_menu_on_top_during_capture` | Do not push ALPHA during real capture. | Menu assertion should fail, but fixture currently hand-sets `tam.function=ITM_FORTH`. |
| 108 | `test_alpha_menu_contains_fwrd` | Remove FWRD from ALPHA menu. | Item/presence assertion fails. |
| 109 | `test_forth_toggle_from_catalog_leaves_alpha_menu` | Replace the catalog-menu `while` drain with one `popSoftmenu()`. | `currentMenu() = -1318, expected -1922 (-MNU_ALPHA)`. |
| 110 | `test_forth_capture_survives_keystroke` | Replace the A1 preservation conditional with unconditional `tam.function=ITM_LITERAL`. | `tam.function = 0x0072, expected ITM_FORTH (0x0B1A)`. |
| 111 | `test_forth_alpha_gesture_resumes_forth` | Apply the same unconditional assignment on the AIM-resume path. | `tam.function = 0x0072, expected ITM_FORTH (0x0B1A)`. |

**The defect.** #107 primes the capture sentinel it claims to observe and saves
only `softmenuStack[0].softmenuId`. #109 primes `tam.function=ITM_FORTH`; that is
acceptable for its catalog-teardown subject, but it writes
`fnKeyInCatalog=1` then hard-clears it to 0 rather than restoring the saved
value. #110 and #111 correctly derive state through real actions and caught
their mutants.

Earlier #71 also leaks `tam.function`, `tam.mode`, `aimBuffer`, menu state, and
`calcMode`; #107 reinforces that leaked `ITM_FORTH`. Later tests save and restore
the already-leaked value, making order part of the fixture.

**The change.** Add a common PEM state snapshot/restore helper inside the
self-test file containing the full `softmenuStack`, `tam`, `aimBuffer`,
`calcMode`, `catalog`, `fnKeyInCatalog`, `currentStep`,
`pemCursorIsZerothStep`, `currentLocalStepNumber`, and relevant ALPHA flag.
Use it on every exit of #71, #74, #93, #94, and #100–#111. Do not replace real
setup actions with assignments. Rework #107 to open capture using the same real
marker + `addStepInProgram` path used by #110, starting from
`tam.function=0`/ALPHA clear; assert setup before checking the menu. In #109
restore the saved `fnKeyInCatalog`, not literal zero.

Also save/restore `FLAG_SPCRES` in #16 (`test_div_zero_halt`), which currently
clears it permanently.

**Tests that encode the old contract.** Fixtures only; production contract is
unchanged.

**Gate:** build-test green.

*Verified mutations:* #109, #110, and #111 were executed and produced exactly
the table symptoms. #107 and leak-order mutations are UNVERIFIED.

**Report:** snapshot fields, proof #107 derives capture, three verified mutation
outputs, all additional mutation results, and arena line.

---

## R2-14 — Eliminate silent setup skips and restore free-list coverage

**File(s):** `packages/forth-core/test_dict_reloc.c`

**Read:** grep `SKIP:`, `return 0`, tests #112–#117, and their runner lines.
Read only the enclosing test bodies and the runner block.

| # | Test | Mutation | Required RED symptom |
|---:|---|---|---|
| 112 | `test_freelist_consistent` | Create overlapping/out-of-range free regions. | Consistency assertion fails. |
| 113 | `test_freelist_double_free_guarded` | Remove exact double-free rejection. | Guard/error or consistency assertion fails — but current WIP runner does not call this test. |
| 114 | `test_freelist_interior_double_free` | Remove interior-free rejection. | Guard or consistency assertion fails; setup currently SKIPs if no adjacent chunks are found. |
| 115 | `test_freelist_no_mutation_on_oversize_free` | Mutate the list before rejecting oversize free. | Snapshot/consistency assertion fails. |
| 116 | `test_unterminated_def_errors` | Replace unterminated-definition error with success/another error. | Exact error and invisibility assertions fail. |
| 117 | `test_overlong_token_in_def_keeps_error` | Mask INPUT_TOO_LONG with INVALID_NAME. | Error precedence/count/visibility assertion fails. |

**The defect.** The exact-match double-free test is defined but was removed from
the current WIP runner, so its production guard can regress without that test
running. #114 can print SKIP and return success when it cannot find adjacent
chunks. More broadly, many dictionary tests treat failed subject setup as SKIP:
`begin_word`/`define_word` allocation failures, the H1 label setup, validation
V1/V2 setup, and adjacent-pair search can all turn production allocation
regressions green.

**The change.** Restore a direct runner call to #113 after #112 and before #114.
Make every allocation/setup failure in a `test_*` return failure, not SKIP,
unless the test is explicitly probing optional host infrastructure (#52 was
already fixed in R2-07). Make #114 deterministic by allocating/finding its
required pair in a freshly initialized arena; if that construction fails, the
test is RED. Do not edit any free-list production source.

**Facts the harness forces on you.** The expected stderr from #113–#115 is not a
failure. Judge their return values and assertions, not merged-log adjacency.
`test_freelist_consistent` is also called inside #114/#115, but that does not
replace direct runner registration for #112/#113.

**Tests that encode the old contract.** Setup semantics only.

**Gate:** build-test green.

*Mutation: UNVERIFIED — runner absence and SKIP paths are structurally proven;
guard mutants were not executed.*

**Report:** all SKIP-to-FAIL sites, runner order, six mutation results, expected
guard diagnostics, arena line and high-water mark.

---

## R2-15 — Run the full isolated suite and commit once

**File(s):** `packages/forth-core/test_dict_reloc.c` and only files changed by
R2-01 through R2-14

**Read:** grep `int forthDictSelfTest`, every `fail |= test_`, and every
`static int test_`; inspect only the runner and declaration anchors. Use a small
script or `rg` list comparison; do not read the file whole.

**The defect.** Before this review there were 117 static `test_*` definitions,
one decorative test (#40), and one defined guard test (#113) absent from the
current WIP runner. Global state made results order-dependent. A green run is
credible only if every retained test is registered exactly once, every repaired
test caught its named mutant, and the isolated normal suite is green.

**The change.** Compare definition and runner sets. Every retained `test_*` must
run exactly once, except a helper called intentionally by another test must
still have one explicit top-level coverage decision documented. Run the full
sanctioned gate once with all mutants removed. Confirm both success banners and
exit 0. Report the `FORTH ARENA:` high-water line. Then stage only the files
changed by these R2 tasks (never `git add -A`) and create one commit with message:

`forth-core: make self-tests mutation-sensitive`

Do not push.

**Tests that encode the old contract.** All 117 tests are accounted for in
R2-01 through R2-14; no additional contract change is authorized here.

**Gate:** build-test green.

*Verified mutation:* the required evidence is the complete mutation matrix from
the preceding tasks. If any row remains `UNVERIFIED`, do not commit; report the
specific row and stop.

**Report:** definition/runner counts, duplicate/missing list (must be empty or
explicitly justified), both green banners, exit code, arena high-water mark,
staged file list, and commit hash.

---

## Reviewer accounting: why the current green claim is not credible

- **Cannot fail:** `test_lblq_undefined_no_ub` always returns 0; no production
  mutation reaches an assertion.
- **Not run:** `test_freelist_double_free_guarded` is defined but its direct
  runner call was replaced by unfinished audit probes in the reviewed WIP.
- **Passes for a different reason/path:** `test_tam_dispatcher`,
  `test_xeq_end_to_end`, `test_lifecycle_reset`,
  `test_prescan_forward_reference`, and
  `test_prescan_two_programs_first_touch` do not execute the path/sequence their
  names or docstrings claim.
- **Known escaping assertions:** `test_branch_fwd`, `test_literal_after_lit`,
  `test_rstack_overflow`, `test_runaway_guard`, `test_malformed_token`,
  `test_picker_scan_basic`, `test_picker_long_token_skipped`, and
  `test_softmenu_trailing_null` admit the mutations specified above.
- **Order-dependent:** #71 and #107 leak Forth capture state; #109 overwrites
  `fnKeyInCatalog` rather than restoring it; #16 leaks `FLAG_SPCRES`; menu tests
  restore only part of `softmenuStack`.
- **Fixture audit:** aside from #57, #64, and the standalone #68 fixture, the
  explicit `ITM_END` versus appended `.END.` choices and the predecessor effects
  of `addStepInProgram` match their asserted scenarios.
- **Executed spot checks:** removing E5 `hadText`, reducing the catalog drain to
  one pop, and replacing both A1/A7 preservation arms with unconditional
  `ITM_LITERAL` each made its intended newest test RED with the exact symptoms
  recorded above.
