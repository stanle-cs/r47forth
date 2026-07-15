# R1 — audit of DESIGN.md [VERIFIED:] citations

Date: 2026-07-15  
Branch inspected: forth-core/pem-entry-fixes

## Executive result

I found 74 “[VERIFIED: …]” tokens. DESIGN.md:23 is an illustrative placeholder,
not an assertion, so the audit population is 73 substantive citations:

| Verdict | Count |
|---|---:|
| TRUE | 45 |
| STALE | 14 |
| FALSE | 13 |
| UNVERIFIABLE | 1 |

“TRUE” means the cited slice supports the whole attached claim, not merely one
clause. “STALE” means the claim is still supported but the line moved. “FALSE”
means the cited text does not support the whole claim or the claim is opposite
to the current source. “UNVERIFIABLE” means there is no external file-and-line
evidence to inspect.

The warning in the task is confirmed. The placeholder branch reads
tam.function, but the re-insert branch derives aimFunc exclusively from
currentStep[0..1]. A hand-set tam.function is therefore not sufficient to make
the re-insert path capture a high opcode.

The audit also found four bounded code defects for which the design is
implementation-complete enough to write Qwen prompts:

1. The outer interpreter has no C47 item lookup, so names such as SIN fall
   through to label/undefined handling instead of producing FTOK_C47.
2. Compile-state C47 labels are rejected; FTOK_XEQN is absent from the emitter
   and inner interpreter.
3. The first allocation in forthDictEnsure(bytes) always allocates 64 blocks
   and returns success even when bytes needs more space.
4. forthInner reads token/inline bytes without checking them against
   fdict.here, so a truncated restored body can read beyond the logical
   dictionary instead of reporting corrupted data.

The separate prompt file is QWEN_PROMPTS_R1_code_defects.md.

## Citation-by-citation audit

The “DESIGN” column is the line carrying the citation. Line ranges below refer
to the current working tree.

| DESIGN | Cited subject | Verdict | What the cited source actually establishes / corrected location |
|---:|---|---|---|
| 75 | Free item slots 213–219 | FALSE | packages/forth-core/items.c:2003 is now MNU_FORTH. Only 214–219 are free in the current package table; the claimed example range is no longer wholly free. See packages/forth-core/items.c:2003-2009. |
| 82 | MNU_PROG row | TRUE | src/c47/items.c:3196 is the MNU_PROG menu row. |
| 121 | PTP_REM step length | TRUE | src/c47/programming/nextStep.c:291-300 selects the two-byte opcode path; :236-238 accounts for the variable string length. |
| 122 | PTP_REM decode path | TRUE | src/c47/programming/decode.c:905-908 routes PTP_REM through decodeRem; :828-843 decodes the string payload generically. |
| 124 | PTP_REM clear-variable behavior | TRUE | src/c47/programming/clcvar.c:273-277 has the stated arm. |
| 126 | PTP_REM program-management handling | TRUE | src/c47/programming/manage.c:1705-1708 has the stated no-op arm. |
| 128 | PTP_REM label/GTO/XEQ handling | TRUE | packages/forth-core/programming/lblGtoXeq.c:838-863 contains the package-specific handling. |
| 163 | ITM_FORTH item entry | TRUE | packages/forth-core/items.c:4707 is the ITM_FORTH row. |
| 165 | ITM_REM item entry | TRUE | packages/forth-core/items.c:3374 is the REM row. |
| 286 | Primitive wrapper shape | TRUE | packages/forth-core/forth_prims.c:9-17 shows wrappers forwarding through the C47 item machinery. |
| 356 | PEM high-opcode re-insert shape | TRUE | src/c47/programming/manage.c:953-959 shows the two-byte opcode reconstruction/insertion shape. |
| 359 | Two-byte opcode stepping | TRUE | src/c47/programming/nextStep.c:236-238 supports the stated size rule. |
| 901 | Outer context/depth storage | STALE | The variables are now packages/forth-core/forth_compile.c:36-37; cited :23-24 is an enum. |
| 1083 | Label list rebuilt “on every edit” | FALSE | The cited rebuild body and the call at :730 establish insertion only, not every mutation. Current evidence needs src/c47/programming/manage.c:102-180 plus mutation calls at :194-200, :221-249, :262-277, and :710-730. |
| 1088 | Upstream stores label name, never ID | FALSE | src/c47/programming/lblGtoXeq.c:365-368 shows run-time name resolution, but not the storage assertion. The cited slice begins inside the STRING_LABEL_VARIABLE dispatch arm; an insertion-path citation is also required. |
| 1285 | EXIT/ALPHA excluded by item filter | TRUE | packages/forth-core/items.c:3557 and :3560 have the stated status categories. |
| 1381 | forthResolveXEQ order | STALE | The function is now packages/forth-core/forth_dict.c:390-420, not :292-323. |
| 1383 | tam.function/item-selection state | TRUE | packages/forth-core/ui/tam.c:943-971 supports the stated capture state. |
| 1388 | Name recording and later redirect | TRUE | src/c47/programming/manage.c:1775-1804 and packages/forth-core/ui/tam.c:964 support the two halves. |
| 1393 | FCALL/name encoding | TRUE | packages/forth-core/programming/lblGtoXeq.c:364-390 supports the stated representation/dispatch. |
| 1397 | Run-time name lookup | TRUE | packages/forth-core/programming/lblGtoXeq.c:376-378 resolves the inline name. |
| 1406 | FCALL redirect | STALE | The redirect is now packages/forth-core/programming/manage.c:1586-1600; cited :1567-1582 is lead-in code. |
| 1449 | FCALL invalid-index error | TRUE | packages/forth-core/programming/lblGtoXeq.c:485-491 has the stated guard. |
| 1451 | Error-code constants | TRUE | packages/forth-core/defines.h:1386-1398 contains the cited codes. |
| 1635 | PROG dynamic-menu model | TRUE | src/c47/softmenus.c:1673-1704 is the label enumeration model. |
| 1664 | One patch per upstream override | TRUE | The generated patches directory currently has the stated one-per-override layout/count. This is a tree-state check, not a code behavior check. |
| 1699 | forthDictInit startup call | STALE | The call is packages/forth-core/config.c:1941; cited :1945 is the self-test comment. |
| 1704 | Dictionary init/reset mechanics | TRUE | packages/forth-core/forth_dict.c:39-58 supports the claim. |
| 1815 | Upstream tam.function assignment | TRUE | src/c47/programming/manage.c:854 assigns it. |
| 1816 | FORTH_SOURCE_MAX | STALE | It is packages/forth-core/forth_compile.c:27, not :22. |
| 1868 | PTP_REM program hook | TRUE | packages/forth-core/programming/lblGtoXeq.c:838-863 supports the hook. |
| 1886 | runProgram error stop | FALSE | The cited :925-947 omits the actual error break at packages/forth-core/programming/lblGtoXeq.c:955-956. Cite :925-956. |
| 1891 | Primitive/colon/number error gates | STALE | Current branches are packages/forth-core/forth_compile.c:313-318 (primitive), :333-345 (colon), and :179-214 (number helper). |
| 1896 | execute-one-step and SST stop points | FALSE | executeOneStep is called at packages/forth-core/programming/lblGtoXeq.c:935, not :925; the SST break is :984-986, not :974-976. Cite :924-935 and :984-986. |
| 1903 | fnForthOuter error propagation | STALE | fnForthOuter is now packages/forth-core/forth_compile.c:451-466; cited :344-363 is resolver logic. |
| 1935 | Program-internal XEQ continuation | FALSE | Cited :886-897 is the end of one executor/start of runProgram. The relevant programRunStop gate is packages/forth-core/programming/lblGtoXeq.c:161-180, especially :162-164. |
| 1940 | Program-step advancement/menu dispatch | FALSE | :904 contains the increment, but the menu dispatch is outside the cited :904-912 range at packages/forth-core/programming/lblGtoXeq.c:914-920. Cite :904-920. |
| 1972 | Generic REM placeholder/re-insert model | TRUE | src/c47/programming/manage.c:1386-1399 and pemAlphaInput :773-966 support the broad model. |
| 2004 | Label scan “on every edit” | FALSE | Same defect as DESIGN:1083: the citation proves the scan body and insertion call, not every edit. Cite the full mutation call map listed above. |
| 2009 | Insertion triggers label scan | TRUE | src/c47/programming/manage.c:730 calls the scan. |
| 2018 | Generic high-opcode routing | TRUE | src/c47/programming/manage.c:1376-1383 supports the claim. |
| 2063 | Catalog selection dispatch/close | STALE | Current package locations are packages/forth-core/keyboard.c:1225 and :1228, not :1213/:1216. |
| 2068 | Program-entry keyboard branches | TRUE | packages/forth-core/keyboard.c:443 and :466-475 support the claim. |
| 2110 | Placeholder and re-insert both usable through tam.function | FALSE | Placeholder insertion uses tam.function at src/c47/programming/manage.c:825-838. Re-insert instead sets aimFunc from currentStep[0..1] at :938-943 and never consults tam.function. The citation exposed, but did not justify, the stronger assumption that caused the reported crash. |
| 2126 | Alpha keyboard selection | STALE | The relevant package branch is packages/forth-core/keyboard.c:1710-1713, not :1698-1702. |
| 2149 | Upstream program-mode gate | TRUE | src/c47/programming/manage.c:994 has the stated gate. |
| 2158 | Default cursor initialization | STALE | T_cursorPos is initialized at src/c47/programming/manage.c:570; :577 is inside the numeric-input branch. |
| 2174 | MNU_FORTH absent from dynamic layout | FALSE | packages/forth-core/softmenus.c:3879 already includes MNU_FORTH; the design says it is absent “today.” |
| 2177 | Package keyboard menu routing | STALE | The relevant branch is packages/forth-core/keyboard.c:3911-3925, not :3905-3913. |
| 2181 | f-shifted AIM key | TRUE | src/c47/assign.c:32 maps it as stated. |
| 2210 | Marker glyph widths | TRUE | src/c47/fonts.c:150 and :156 support the width claim. |
| 2214 | Marker insertion source site | TRUE | src/c47/programming/manage.c:604 is the cited insert site. |
| 2219 | Marker decode dispatch | TRUE | src/c47/programming/decode.c:828-843 supports the generic string decode path. |
| 2228 | Empty-string display handling | TRUE | src/c47/programming/decode.c:707-713 supports the stated behavior. |
| 2233 | nextStep/checkOpCodeOfStep callers | TRUE | The cited callers exist at src/c47/programming/manage.c:565/:785, nextStep.c:364, and decode.c:42/:65/:112. |
| 2251 | Label scanner and PROG menu analogy | TRUE | src/c47/programming/manage.c:102-160 and softmenus.c:1673-1704 support the analogy. |
| 2260 | Dynamic softmenu capacity | TRUE | src/c47/softmenus.c:1017-1029 and :1211-1234 define 22 matching dynamic slots. |
| 2271 | PROG label menu loop | TRUE | src/c47/softmenus.c:1675-1698 supports the claim. |
| 2281 | Menu-open rebuild path | TRUE | src/c47/softmenus.c:1261-1270 supports the opening/rebuild path. |
| 2283 | Rebuild-always condition | TRUE | src/c47/softmenus.c:3039-3042 contains the condition. |
| 2291 | Catalog key and tam.function assignment | TRUE | src/c47/keyboard.c:1153-1156 and manage.c:854-857 support the two halves. |
| 2302 | Outer error-path range | STALE | The general outer loop/error handling is now packages/forth-core/forth_compile.c:232-435, not :210-341. |
| 2304 | Program error halts before next step | FALSE | Same truncated citation as DESIGN:1886; the break is packages/forth-core/programming/lblGtoXeq.c:955-956. |
| 2309 | Error display plumbing | TRUE | src/c47/error.c:288 and screen.c:3734 support the mapping. |
| 2313 | Malformed-definition handling | STALE | Current branches are packages/forth-core/forth_compile.c:249-301 and :420-427, not :212-244/:333-336. |
| 2322 | Dynamic menu scan slot | TRUE | packages/forth-core/softmenus.c:1680-1682 supports the stated scan point. |
| 2356 | tam.function assignment | TRUE | src/c47/programming/manage.c:853-856 supports the claim. |
| 2358 | Tokenizer implementation | STALE | The tokenizer loop is packages/forth-core/forth_compile.c:71-85, not :42-57. |
| 2359 | Primitive table | TRUE | packages/forth-core/forth_prims.c:44-46 supports the mutation target. |
| 2364 | Inner token dispatch | TRUE | packages/forth-core/forth_inner.c:43-56 supports the mutation target. |
| 2388 | “mechanism specified in §4.2” | UNVERIFIABLE | This is an internal section reference, not a file:line citation and not evidence that the mechanism exists. Current code has forthResolveXEQ, but that does not make the citation itself verifiable. |
| 2440 | Backup save/restore is one-way text | FALSE | saveRestorePrograms.c:162/:362 are function entries and do not establish the representation claim. |
| 2443 | Export/import is binary/raw bytes | FALSE | The claim is factually wrong. packages/forth-core/saveRestorePrograms.c:432-451 writes headers and each byte as decimal text lines; :561-585 parses those lines back. It is lossless textual byte serialization, not binary/raw serialization. |

## Contradictions and design defects

### 1. E4 overstates tam.function

The quoted E4 citation is unsafe as specification. There are two different
sources of opcode truth:

- Placeholder insertion: tam.function at src/c47/programming/manage.c:825-838.
- Per-key re-insert: currentStep[0..1] at :938-943.

The re-insert path does not consult tam.function. E4 should name the two
sources separately and explicitly say that setting tam.function alone cannot
open a valid high-opcode capture.

### 2. “forthInner never nests” is the opposite of the implementation

DESIGN.md:627-628 says forthInner never nests and is always the only engine.
DESIGN §3.2, packages/forth-core/forth_inner.c:155-168, and existing tests
test_c47_nested_call_succeeds, test_nested_preserves_outer_rstack, and
test_nested_error_unwinds_rsp all require bounded nesting. This is a normative
contradiction, not line drift.

### 3. Numeric-overflow prose describes an already-fixed omission

DESIGN.md:1222 says forthDictEnsure “never checks” the 64 KiB offset limit.
packages/forth-core/forth_dict.c:113-117 performs that check. The required-change
prose is stale.

### 4. Source-step rendering has three incompatible contracts

§8.5 and test_decode_source_bare require bare source text. The hook table says
only that non-empty FORTH payloads use generic display, while §8.8 says the
display is FORTH ‘…’. The current package/test contract is bare source. Choose
one normative wording and remove the other two implications.

### 5. The P-H2 cursor contract conflicts with E7

The §6/P-H2 table still describes a “FORTH ” prefix/cursor hack. E7 explicitly
forbids such a branch and relies on upstream’s default cursor behavior for bare
source. The current code follows E7.

### 6. Acceptance mutation §8.9(7) is not a mutation

It proposes forcing the source-step arm to return 1 while ignoring
lastErrorCode. The arm already returns 1 unconditionally; runProgram performs
the separate lastErrorCode stop. This mutation cannot demonstrate the claimed
property and needs replacement.

### 7. Save/load wording is materially wrong

§10 calls file export/import “binary” and “raw program bytes.” The file format
is textual decimal-byte serialization. The semantic point—Forth payload bytes
are opaque and round-trip losslessly—is supportable, but the representation
words are not.

### 8. “Cross-program visibility open” and “program-local” cannot both stand

§8.10 labels cross-program word visibility open, then later states categorically
that words are program-local. The current dictionary and first-touch logic are
global within a run generation, so source inspection predicts cross-program
visibility. The required empirical check could not run; see below.

### 9. Missing executable E9 implementation

E9 normatively requires an advisory, check-only tokenizer/resolver pass on
commit. No check-only entry point or commit-time call exists in
forth_compile.c, programming/manage.c, or the self-test registration. This is a
code-wrong/design-right gap. I did not create a Qwen task because the design
does not pin the API, how the picker’s “plausibly coming” set is supplied, or
the exact observable warning/test contract. Those choices need an architect
task breakdown first.

### 10. FTOK_XEQN worst-case arithmetic is mistyped

The stated worst-case encoded size, 34 bytes, is correct: token 2 + length 1 +
name 31 = 34, with no pad because the inline 32 bytes are already even. The
displayed expression “2 + 1 + 31 + 1 = 34” both adds incorrectly and claims a
pad that is not present for a 31-byte name.

### 11. The malformed/truncated threaded-code promise is not implemented

DESIGN.md:539 says an invalid/stale entry is never a silent no-op and maps it
to ERROR_INVALID_CORRUPTED_DATA. Current forth_inner.c calls readToken and
memcpy/byte reads for LIT, ILIT, branches, and C47 operands without first
checking fdict.here. test_truncated_inline_operand has been authored in the
unfinished self-test work, but the production guard is absent. This is a
code-wrong/design-right defect and has a bounded Qwen task.

## Runtime absolute-claim audit

I screened every case-insensitive occurrence of “will be”, “cannot”, “always”,
and “never”. The table below contains every match that asserts execution,
encoding/persistence, memory-safety, or visible runtime behavior. “Existing
test” means a named test is present in test_dict_reloc.c; it does not mean this
audit successfully reran it. The current gate cannot compile that file.

| DESIGN | Runtime absolute claim | Executable? | Did anyone? / audit result |
|---:|---|---|---|
| 143 | FCALL argument comes from TAM entry, never the item param | Yes | Existing test_fnforthcall_interactive plus redirect tests cover the bridge/recording split. Not rerun. |
| 191 | Parameter tokens belong to GTO/XEQ payloads, never opcodes | Yes | User-item XEQP1 encode/decode tests exercise this representation. Not rerun. |
| 226 | Stored dictionary headers are never dereferenced “as-is” | Source-auditable | FALSE literally: forth_dict.c repeatedly casts fdict.base + offset to forthHeader_t * and dereferences it. If “as-is” meant “never persist a native pointer,” rewrite the sentence to say that. |
| 396 | Labels are never baked; FTOK_XEQN is used | Yes | FALSE in current code: compile state rejects a label and FTOK_XEQN is absent. No positive test. |
| 399 | PTP_NUMBER_8 FTOK_C47 is always six bytes/cell-aligned | Yes | test_c47_ptp_number8_padded exists. Not rerun. |
| 419 | Primitive index 0 never emits as FTOK_EXIT | Yes | Primitive/outer tests indirectly cover DUP; there is no focused emitted-token assertion for index 0. |
| 432 | A bare programRunStop check cannot interrupt hardware mid-word | Hardware-only | Reasoned from DMCP polling; no DMCP key-injection test. |
| 539 | Invalid/stale threaded entries are never silent no-ops | Yes | FALSE for truncated bodies: operand reads are not bounded by fdict.here. test_malformed_token and test_truncated_inline_operand exist, but the current test file cannot compile and the production guard is absent. |
| 563 | Primitive dispatch never indexes by raw token | Yes | Existing primitive execution tests are indirect; no focused mutation test was found. |
| 624 | Interactive entry was never PGM_RUNNING before a key poll | Yes | No direct DMCP interactive-poll test. |
| 627-628 | forthInner never nests and is always the only engine | Yes | FALSE: code permits bounded nesting and three tests require it. |
| 717-721 | Miss sentinel test is never unsigned >=0; index 0 never emits raw | Yes | Unknown-word and primitive tests are indirect; no focused predicate mutation found. |
| 786 | Inner anomaly operations genuinely cannot execute | Yes | Bad-PTP/reentrancy tests cover representative cases. Not rerun. |
| 814 | fnForthOuter never dispatches from a PEM source/marker step | Yes | test_exec_step_source_runs and marker tests exercise the direct program hook. Not rerun. |
| 838 | A nested line can never close/abort the outer definition | Yes | test_outer_nesting_tokenizer and context-at-rest tests exist. Not rerun. |
| 844 | Outer frames never exceed typed-line → program-step | Yes | Guard is tested by test_outer_depth_cap; the “natural paths cannot exceed” proof is source reasoning. |
| 909 | Program payload length is always below FORTH_SOURCE_MAX | Static/executable | True by uint8_t range versus 256; no useful dynamic test needed. |
| 922 | Program memory is never interpreted in place | Yes | Source shows a private copy; no mutation-after-copy test was found. |
| 938, 947 | Tokenizer position is always at glyph start and never splits byte 2 | Yes | Three glyph tokenizer tests exist. Not rerun. |
| 959 | Tokens over 31 bytes can only be number literals | Yes | Overlong-token definition test exists; broad positive number case is not focused. |
| 1017 | Compile/interpret numeric semantics never diverge | Yes | test_ilit_compile_interpret_parity exists. It does not prove every out-of-range real fallback. |
| 1034 | NaN/Infinity never reach stringToReal34 | Yes | No focused NaN/Infinity test found. |
| 1045 | Integer conversion never uses truncating longIntegerToInt32 without range checks | Source-auditable | Current helper performs range checks; no mutation test found. |
| 1063, 1070 | Baked item IDs never renumber at runtime | Build invariant | True for a fixed firmware image; append-only table maintenance is not a runtime test. |
| 1087 | Upstream never stores a label ID | Yes | Runtime resolution is visible, but the citation does not prove insertion storage. No focused round-trip test found. |
| 1099 | Forced-PGM interactive label dispatch never runs and leaks a level | Yes | test_outer_nesting_tokenizer is documented as the regression that exposed it. Not rerun. |
| 1206 | Resolver cannot see a half-built smudged definition | Yes | Unterminated-definition tests cover hiding/abort; no true concurrent scheduler test. |
| 1222 | forthDictEnsure never checks 64 KiB wrap | Yes | FALSE/stale: it now checks at forth_dict.c:113-117. |
| 1230, 1736 | Compiler never bypasses startDefinition | Source-auditable | Definition/error tests are indirect; no instrumentation proving every creation path. |
| 1235, 1242 | Colon definitions cannot be immediate / encoding never marks them immediate | Yes | Feature is absent; no focused rejection test found. |
| 1261-1262 | Unsigned miss test is always true; resolver must never use it | Source-auditable | Current resolver uses boolean forthFindColon; no focused mutation test. |
| 1303 | User names never hijack built-in machine meanings | Yes | FALSE for outer source because C47 item lookup is missing. Reverse XEQ precedence is tested, but it is a different order. |
| 1385, 1395, 1763 | Program memory persists names, never dictionary indices | Yes | FCALL redirect/stale-index tests exist. Not rerun. |
| 1557 | fdict.base is always the raw allocation pointer | Yes | Save/restore and relocation tests are indirect; no focused pointer-identity assertion found. |
| 1588-1590 | Rejected frees never mutate the free list | Yes | Three double/interior/oversize free guard tests exist. Not rerun. |
| 1635, 1650, 2284 | MNU_FORTH always rebuilds dynamic content | Yes | Menu registration/picker tests exist; current source contains the condition. Not rerun. |
| 1744 | fdict.here is always block-rounded at header boundaries | Yes | Dictionary allocation tests are indirect; no all-boundaries invariant walk found. |
| 1747 | Forth never grows indexOfItems or invents IDs | Architecture invariant | Source-auditable, not meaningfully dynamic. |
| 1770, 1977 | Keypad/entry state is derived, never stored | Yes | marker_parity, entry_state_derivation, and E2 tests exist. Not rerun. |
| 1865 | Remembered program pointers are compared, never dereferenced | Source-auditable | No poison-pointer test found. |
| 1904 | ITM_FORTH never reaches ordinary item dispatch in PEM | Yes | Source-step and marker direct-hook tests exist. Not rerun. |
| 1949 | A paused word always survives R/S resume | Yes | No focused pause/resume dictionary-retention test found. |
| 1954 | SST-only execution never resets | Yes | No focused SST generation-retention test found. |
| 1969 | Scanner/branch code never asks for a persisted Forth mode | Source-auditable | True of current helpers; no meaningful dynamic test. |
| 2127 | Without alpha, letter keys never produce ITM_A etc. | Yes | Alpha/capture tests are adjacent, but no direct full key-map assertion found. |
| 2188 | Malformed RPN steps cannot be entered | Only under keypad-authoring assumptions | Overbroad as written: imported/corrupt program bytes can exist. No end-to-end import-corruption test. |
| 2207 | Marker direction is never stored/cached as two items | Yes | marker parity/direction decode tests cover the representation. Not rerun. |
| 2275 | Picker selection never resolves the dictionary | Yes | Picker text-scan tests cover authored names; no mutation test that poisons fdict lookup. |
| 2315 | A smudged entry never leaks | Yes | test_unterminated_def_errors exists. Not rerun. |
| 2316 | Marker steps cannot error | Yes | test_exec_step_marker_noop exists. Not rerun. |

The remaining absolute-word matches are authoring rules, table-maintenance
rules, generated-file instructions, comments describing chosen ownership, or
mutation-test prose rather than claims about runtime behavior: DESIGN.md
294, 303, 306, 852-853, 1288, 1434, 1619, 2076, and 2345. Lines 1288 and
1434 are resolver/parser design constraints, but they do not independently
assert an observed runtime result beyond the rows above.

## §8.10 cross-program visibility: not executed

I added a temporary self-test that created program A and program LIB, made A
execute LIB, defined “: LIBW 7 ;” in LIB, and then attempted LIBW from A. I ran
the only sanctioned command:

    ./packages/forth-core/build-test.sh

The build stopped before compiling/running the probe because the pre-existing
flat test_dict_reloc.c is unfinished: it calls five audit_probe_* functions
before declarations, then ends mid-definition of audit_probe_buried_catalog
with the top-level #if still unterminated. The concrete missing/unfinished
names reported were audit_probe_buried_catalog, audit_probe_marker_edit_leak,
audit_probe_c47_fixed_param, audit_probe_parameterized_resolver, and
audit_probe_first_ensure_capacity.

Per the repository rule, I did not repair or weaken an unrelated test file. I
removed only my temporary probe from the flat working file. Therefore the
empirical answer is:

**UNEXECUTED — no runtime result is available from the current tree.**

The final sanctioned refresh regenerated files/test_dict_reloc.c from the flat
file. A grep of both copies confirms the temporary LIBW probe is absent.

Source reasoning predicts **visible across programs within one run
generation**: fdict is global; first-touch pre-scan adds definitions to that
global dictionary; a nested XEQ does not bump the generation; and first-touch
of A does not reset a generation that has not changed. That is a hypothesis,
not a substitute for the requested execution.

## Entry-state rule

DESIGN does state the immediate-predecessor rule, but only tersely in E1:
forthEntryStateAtInsertion is commented as deriving from “the step BEFORE
currentStep.” The implementation is unambiguous:

- forthEntryStateAtCursor examines the current step.
- forthEntryStateAtInsertion walks to exactly the immediate predecessor and
  classifies only that step.
- A predecessor source step yields Forth; a predecessor RPN step yields RPN;
  marker parity is consulted only when that predecessor itself is a marker.

Existing test_entry_state_derivation, test_e2_not_inside_rpn_gap, and
test_e2_continuation_after_enter pin this behavior. It is therefore intended,
not an accidental implementation leak.

Under the implemented model, markers do not establish an interval mode that
overrides the steps between them. Steps self-identify. An RPN step deliberately
punctures automatic Forth continuation, even if older markers farther back
would make a visual “region” look open; the ALPHA gesture resumes Forth entry.
The design’s repeated “region” language and arrows invite the opposite mental
model. Promote the predecessor rule to a named normative invariant beside both
state helpers and say explicitly: marker parity does not flow across an
immediate RPN predecessor.

## Cosmetic drift

As requested, I did not audit the regex damage to call-site parentheses. The
known under-/over-restoration remains cosmetic debt and is intentionally out of
scope for R1.

## Gate status

The final sanctioned command was run after both reports were written:

    ./packages/forth-core/build-test.sh

It exited 1 during ninja compilation of test_dict_reloc.c, before any Forth
self-test ran. The decisive diagnostics were:

- implicit declarations at :6452-6460 for audit_probe_buried_catalog,
  audit_probe_marker_edit_leak, audit_probe_c47_fixed_param,
  audit_probe_parameterized_resolver, and
  audit_probe_first_ensure_capacity;
- static audit_probe_buried_catalog at :7244 following its implicit non-static
  declaration;
- unterminated #if from :18;
- expected declaration/statement at end of input at :7272, where the file ends
  mid audit_probe_buried_catalog.

Neither required success banner was printed. No green claim can be made for
R1, and §8.10 remains empirically unanswered.
