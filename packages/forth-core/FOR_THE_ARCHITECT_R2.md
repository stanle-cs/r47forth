# R2 — test-suite findings for the architect

Scope: all 117 unique `static int test_*` functions in
`packages/forth-core/test_dict_reloc.c`, their immediate subjects, and only the
relevant DESIGN.md §4, §7, and §8 slices. I did not treat the ordinary green
gate as evidence. I ran marked mutations, removed every working-area probe, and
recorded the implementation-ready test repairs separately in
`QWEN_PROMPTS_R2_tests.md`.

## 1. §8.9 is not an acceptance suite today

This is the highest-cost finding. A calculator owner can still lose program
execution, resume, save/load keypad state, or current picker contents while the
suite says “all passed.” The ten normative acceptance items at DESIGN.md's
`### 8.9 Acceptance` anchor are only partially represented:

| §8.9 item | What exists | What is not tested |
|---|---|---|
| 1 define/use | Direct `forthProgramStep` and `executeOneStep` unit tests | The specified marker-delimited program executed through XEQ/runProgram |
| 2 derived keypad | Cursor/insertion helper and continuation unit tests | Power-off/save-load, scroll away/back, and keypad behavior for all RPN/source/open-marker/close-marker cases |
| 3 fresh picker | Direct builder and insertion tests | “Without run, next capture line” through the cached public menu path; deleting rebuild-always stayed GREEN |
| 4 marker display | `decodeOneStep` parity tests | BST, SST, and PEM listing agreement on the same real program |
| 5 glyph program | Outer-interpreter aliases and picker glyph tokenization | The specified `: D2 2 ÷ ;` program step and `8 D2 → 4` execution |
| 6 type parity | ILIT/type unit tests | RPN-keypad 7 versus a real Forth program-step 7 |
| 7 run halt | A direct executeOneStep test with a malformed length fixture | runProgram stopping at the bad PC and the user-visible `SQX` error; deleting the run-loop guard stayed GREEN |
| 8 marker/empty | A one-marker stack-buffer call and an empty-ENTER test | A real marker pair run to completion with the entire stack untouched; the acceptance count claim also contradicts E3 (finding 4) |
| 9 lifecycle | Generation unit tests | Run twice without accumulation and STOP then R/S resume |
| 10 XEQ recording | Direct FCALL redirect byte probes | The specified PEM XEQ+alpha name-entry chain |

This is not a request to bulk-add ten improvised simulations. Several required
paths need a verified reachable harness first. In particular, tests must use
real program bytes and callable entry points. Qwen prompts cover only fixtures I
could specify without making a design decision; the remaining acceptance work
should be scheduled after the contradictions below are resolved.

Suggested correction: either implement each §8.9 item as written and name its
test beside the item, or explicitly downgrade unimplemented items from
“Acceptance — each with the mutation that must fail it” to planned acceptance.
The current wording falsely certifies coverage.

## 2. Source-line rendering has two normative answers

Claim A, DESIGN.md §8.5 at the `Render site` anchor: non-empty ITM_FORTH payload
renders **bare**, with no prefix or quotes. The code and
`test_decode_source_bare` implement this.

Claim B, DESIGN.md §8.8 at the `Listing tokens` anchor: source lines render
`FORTH '…'`.

Both cannot govern the PEM listing. The extension principle and the newer
specific §8.5 explanation favor bare rendering: a Forth `SIN` should display
like the RPN `SIN` it extends. Suggested correction: change §8.8 to “source
lines render bare” and retain the marker tokens only. If the architect instead
wants quoted listings, that is a contract change and must migrate
`test_decode_source_bare`, edit extraction offsets, and cursor behavior in one
task.

## 3. E2 names the wrong derived-state function

DESIGN.md §8.4 E2's pseudocode calls `forthEntryStateAtCursor()`. Current
production `manage.c`, anchored at the printable-item in-region route, calls
`forthEntryStateAtInsertion()`. The latter derives from the step immediately
before the insertion point; `forth_bridge.c` documents why this is required
after addStepInProgram's pre-move and after ENTER.

The mismatch also appears in §8.9 item 2's mutation, which says to replace
`forthEntryStateAtCursor()` with a static bool even though the relevant routes
now use AtInsertion. Existing continuation tests support AtInsertion.

Suggested correction: update E2 and acceptance item 2 to AtInsertion, then state
separately where AtCursor remains normative for display/landing behavior. Do
not silently rename one helper to the other; they answer different adjacency
questions.

## 4. Empty ENTER cannot leave total step count unchanged

DESIGN.md §8.4 E3 says an empty commit deletes the placeholder so no empty
source line is created. That is what production does. The opening marker was
already inserted before capture began and remains.

DESIGN.md §8.9 item 8 instead says empty ENTER leaves “no step behind (step
count unchanged).” Starting from RPN and opening capture adds the marker;
deleting only the placeholder therefore leaves total count at `before + 1`.
`test_forth_empty_enter_leaves_no_step` correctly expects the marker to remain.

Suggested correction: replace “step count unchanged” with “no source step is
committed; only the already-inserted opening marker remains.” If the intended
policy is to remove the opening marker too, that is a different UI contract and
requires production and parity-test migration.

## 5. Capture-close cleanup is internally inconsistent

DESIGN.md §8.4 introductory prose says the transient alpha state
`FLAG_ALPHA`, `aimBuffer`, and `tam.function` is “cleared when capture closes.”
E1's exact closing pseudocode clears only FLAG_ALPHA. Production's E1 closing
arm likewise leaves `tam.function == ITM_FORTH`. The opening and closing
subcases in `test_toggle_inserts_marker` run consecutively and do not isolate
tam.function, so the suite neither exposes nor defines this.

Consequence: a later path can observe a stale Forth sentinel after the user
explicitly toggled the region closed. This may be harmless under every current
caller, but that is exactly the kind of unstated caller fact this audit was
asked to reject.

Architect decision required: either (a) E1 closing sets `tam.function = 0` and
tests pin it, consistent with the introductory contract, or (b) narrow the
introductory claim and state why the sentinel is allowed to outlive capture.
The Qwen isolation task deliberately does not assert closing behavior until
this is decided.

## 6. The picker has no specified or enforced unique-name capacity

At the `case MNU_FORTH` anchor in `softmenus.c`, accepted names are copied into
15-byte slots in global `tmpString` and `nNames` increments without a capacity
check. `TMP_STR_LENGTH` is 2560, so at most 170 complete 15-byte slots fit.
A personal program with more unique definitions before the cursor can write
past that workspace. This is ordinary single-user robustness, not a security
boundary, but on the calculator it can mean a reboot or lost edits.

DESIGN.md §8.6 specifies the 14-byte per-name cap but no maximum item count or
overflow behavior. The suite covers long individual tokens and deduplication,
not many unique valid names.

Suggested correction: specify a deterministic cap based on
`TMP_STR_LENGTH / 15` and whether excess names are omitted after sorting,
truncated by scan order, or reported. Then add a boundary test at cap and
cap+1. This needs an architect choice because those policies produce different
menus.

## 7. The required unchecked-copy mutation is not testable by the sanctioned gate

`test_picker_long_token_skipped` says its unchecked-copy mutation passes in a
non-ASan build. The only sanctioned gate is exactly such a build. Therefore the
test can pin semantic omission of a long name, but cannot support its stated
memory-safety claim.

Suggested correction: either provide a debug-only callable token-copy helper
with deterministic canaries and no production linkage/byte cost, or remove the
mutation claim. Do not cite an ASan command that the package's sole gate never
runs.

## 8. Numeric and section citations in the test suite have drifted

Concrete wrong citations:

- `test_useritem_xeqp1_decodes` cites `programming/decode.c:866-869` for the
  complete two-byte opcode reconstruction. During R2, 866 was blank and 869
  contained only the initial read; the low-byte OR was at 873. The stable
  evidence is the code anchor from first-byte read through low-byte OR.
- `test_alpha_menu_contains_fwrd` cites `softmenus.c:1000`; that line was blank.
  The MNU_FORTH row was at the `menu_ALPHA` anchor immediately above.
- Fourteen comments/runner labels still cite nonexistent current §9 sections.
  Exact current mappings are §9.4→§8.4, §9.5→§8.5, §9.9→§8.9, and
  §9.10→§8.10.

These are test-file citations rather than DESIGN.md `[VERIFIED:]` tags, but they
have the same failure mode: a future implementer trusts decoration instead of
the code. The Qwen task replaces numeric citations with grep anchors.

## 9. Tests whose names or prose overclaim their path

- `test_xeq_end_to_end` directly calls `fnForthCall`; it does not execute XEQ,
  ITM_FORTH, or both new items. Rename it to the path it actually tests. §7 H1's
  real XEQ coverage remains missing.
- `test_exec_step_halts_on_error` declares length 4 for five payload bytes
  `3 SQX`, so it executes `3 SQ` and ignores X. It also passes a stack buffer
  outside real program memory despite the pre-scan contract.
- `test_prescan_two_programs_first_touch` promises a third P1 touch but performs
  only P1 then P2. Its P1 defines no word, so a re-scan would be unobservable
  even if the touch were added.
- `test_picker_scan_basic` prints “sorted” but checks only membership.
- `test_picker_omits_long_names` claims the retained boundary is 14 bytes but
  retains `SHORT` (5).
- `test_div_zero_halt` claims X remains 42. Changing 42 to 43 left it GREEN, and
  a temporary X==42 assertion failed against otherwise-correct production. The
  real tooth is exact error plus “sentinel 999 did not run”; the preservation
  prose is false.
- `test_exec_step_marker_noop` claims the stack and alpha/drop state stay
  untouched but checks only X and dictionary count.

These are test repairs, not design choices, except where they expose a missing
acceptance path. Exact changes are in the Qwen file.

## 10. State isolation failures make ordering part of the harness contract

`test_alpha_menu_on_top_during_capture` primes `tam.function = ITM_FORTH`
before the subject derives it and does not restore the incoming value.
`test_toggle_inserts_marker` does not isolate tam.function between its two
subcases. The catalog tests legitimately set `fnKeyInCatalog` to model the
static keyboard caller, but restore a hardcoded 0 rather than the saved value.
`test_fcall_redirect_records_name` prints through a program-step pointer after
`cleanupTestProgram` has released/restored that fixture.

By contrast, the two newest derived-state tests are clean:
`test_forth_capture_survives_keystroke` and
`test_forth_alpha_gesture_resumes_forth` both start tam.function at 0 and drive
the real state derivation. The catalog chain `runFunction(); _closeCatalog()` is
also a valid reachable approximation because `executeFunction` is static; its
problem is cleanup, not reachability.

## 11. Empirical mutation record

All mutations were marked `AUDIT-PROBE R2`, built only with the sanctioned
script, and removed from working-area sources.

Mutations that left the complete gate GREEN:

- BR delta 1→0 in the forward test;
- BR delta 260→4 in the large-delta test;
- deleting DUP/PLUS from the stack test;
- deleting all live tokens after the first literal;
- changing the div-zero fixture's documented 42 to 43;
- not invoking A0 or RR2;
- not invoking any of MP, MC, or MR;
- passing the resolved Forth index to LBL? instead of INVALID_VARIABLE;
- deleting runProgram's `lastErrorCode == ERROR_NONE` guard;
- deleting FWRD qsort;
- rejecting 14-byte names (`<=14`→`<=13`);
- deleting FWRD's rebuild-always cache term;
- deleting the production reset hook.

Mutation caught by the current suite:

- `calloc`→`malloc` for the FWRD result buffer went RED at
  `test_softmenu_trailing_null`: `count=1, byte=100`.

Strengthened assertions temporarily verified RED:

- the forward-BR Y canary caught delta 1→0;
- X==10 caught deletion of stack operations;
- exact ERROR_RAM_FULL caught omitted rstack/runaway calls (observed error 0);
- exact malformed-token errors caught all three omitted calls (observed error
  0).

The temporary attempt to assert div-zero X==42 failed under correct production,
which is evidence against the comment rather than a proposed test.

## 12. Categories checked without a new defect

- `.END.` fixture handling was consistent: `writeTestProgram` appends the
  final sentinel, while explicit ITM_END bytes separate/end real programs.
- `test_prescan_no_early_tail`'s two drops and sentinel arithmetic are correct;
  premature tail execution changes the observed stack and fails it.
- The three free-list diagnostics are intentional. Their tests compare
  free-list state before/after the rejected operation; I did not attribute
  stderr by log adjacency.
- I found no new vacuity in the core dictionary relocation/link-chain tests,
  number-parser rejection tests, save/restore validation tests, or the direct
  free-list snapshot tests. Their mutations in the Qwen appendix are static
  hypotheses unless explicitly marked empirical.

The R2 mutation runs reported a dictionary arena maximum of
`here=36 sizeBlocks=16 freeRamDelta=64`. The final clean-tree gate must report
and compare that value again.

---

## ARCHITECT RULING — 2026-07-15

### Standalone step execution is a fixture artifact, NOT a supported API.

**Hand R2's migration task to Qwen as written.** Its conservative choice —
migrate `test_exec_step_halts_on_error` to `writeTestProgram` — is correct. The
ruling is stronger than the question assumed, in two ways.

**1. The P2 ruling already decided this (2026-07-13).** `forthProgramStep`'s
contract requires the payload to reside inside a real program: the first touch
pre-scans the owning program (`FORTH_OUTER_DEFS_ONLY`), then executes only tails
(`FORTH_OUTER_SKIP_DEFS`). The `build_payload` helper was retired for exactly
this reason and the sibling tests were migrated, with the note recorded in
`test_dict_reloc.c`: *"Stack-buffer payloads encode the retired execute-in-place
semantics."* `test_exec_step_halts_on_error` is an unmigrated survivor of a
retired architecture — not a deliberate exception carved out for a supported
standalone API. There is no such API.

**2. The fixture does not merely lack an owning program — it silently acquires
the WRONG one.** `forthOwningProgramStart` (forth_bridge.c:33-42) returns the
last program whose `instructionPointer <= ptr`, with **no upper-bound check**. A
stack buffer sits at a high address, so that comparison holds for every program
and the function returns the last one rather than NULL.

Measured in the simulator (probe, since reverted):

```
stack step = 0x7fff26e95450   beginOfProgramMemory = 0x7ecb3270e004
forthOwningProgramStart(stack ptr) = 0x7ecb3270e004   -> a REAL program
```

So the test pre-scans an arbitrary, unrelated program and compiles whatever
definitions it holds into the dictionary before executing `3 SQX`. Its green
depends on what that program happens to contain: define `SQX` there and it
fails; put a malformed Forth line there and the pre-scan errors first, so
`lastErrorCode` is that error and never `ERROR_FUNCTION_NOT_FOUND`. The test is
order-dependent on unrelated fixtures and passes for a reason unrelated to the
property it names — the "test that cannot fail for the right reason" class R2
was chartered to find.

**Required of the migrated test.** Keep the property — an undefined word in a
source step sets `ERROR_FUNCTION_NOT_FOUND` and halts the run — but reach it
through a real program via `writeTestProgram`, so the pre-scan is live rather
than accidentally aimed elsewhere. The migrated test must fail if the arm stops
propagating the error; state that mutation in its header comment.

### Corroborated by R2's rerun (2026-07-15)

The rerun reached this conclusion independently, without seeing this ruling —
its T4 item 3 rebuilds the test with `writeTestProgram` and states "do not use a
stack-local fake step". It also found a second defect this ruling missed: the
fixture declares payload length **4** for the five bytes `3 SQX`, so it actually
executes `3 SQ` and ignores the X. The test named for an undefined `SQX` never
mentions `SQX` to the interpreter at all.

The two defects compound. The declared word is `SQ`, and the stack pointer
resolves to a real, unrelated program whose definitions the pre-scan compiles —
so had that program ever defined `SQ`, the test would have failed for a reason
having nothing to do with its name. Green by two coincidences at once.

### New finding, not R2's, for the R1/R4 code-defect track

`forthOwningProgramStart` has no upper bound: any pointer at or past the last
program's start resolves to that program. It is currently harmless in production
— all three callers (forth_compile.c:476, forth_bridge.c:64, forth_bridge.c:114)
pass pointers genuinely inside program memory — and it is also only correct if
`programList` is sorted ascending by `instructionPointer`, which is assumed and
undocumented. It is a latent trap of the same shape as the catalog-drain
predicate mismatch: correct today by the callers' good behaviour, silently wrong
for the first caller that misbehaves. It is what made this fixture wrong.
Bounding it (`ptr < forthNextProgramStart(candidate)`, or an explicit
program-memory range check) is a bounded Qwen task; the sortedness assumption
should be asserted or documented.
