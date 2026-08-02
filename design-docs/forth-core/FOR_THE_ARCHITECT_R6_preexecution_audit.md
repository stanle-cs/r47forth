# R6 — Fable pre-execution audit of the complete forth-core corpus

Date: 2026-07-15. Auditor: Claude Fable 5 (chief-architect / independent audit role).
Tree audited: branch `forth-core/pem-entry-fixes`, HEAD `b0c54128c` (clean), package
base_commit `b8f79e486` (post-migration, `.refresh-manifest.json`). Audit was
read-only: no build was run (out of scope by charter); the last documented gate
state at HEAD is green — commit `b0c54128c` records "Gate green (exit 0, ALL
PASSED, 111 test invocations). Arena unchanged: here=36 sizeBlocks=16
freeRamDelta=64."

---

## 1. Executive verdict

**NO-GO.**

Exactly **two Qwen prompts remain unexecuted**: **R1-3** and **R1-4** in
`QWEN_PROMPTS_R1_code_defects.md`. Every other prompt file in the repository is
fully executed and verified present in the tree (§6, §7 below). Both pending
prompts are unsafe as written:

- **R1-4** implements `FTOK_XEQN` now, which the architect's own later R4
  interview explicitly re-sequenced to *after* the engine-lifetime foundations
  (`FOR_THE_ARCHITECT_R4.md`, "Boundary and order", items 1→3), because XEQN
  label dispatch is exactly what makes the **B1 use-after-free path**
  (dictionary cleared under a live `forthInner` frame) reachable. R1-4 also
  prescribes a `PGM_RUNNING` wrap around the XEQN **colon** arm that the
  accepted **B4** ruling forbids ("must not synthesize PGM_RUNNING").
- **R1-3** is internally contradictory: it orders a refactor of
  `forthResolveXEQ`'s item arm onto a `PTP_NONE`-filtered helper while also
  ordering that `test_xeq_item_lookup` be left unchanged — but that test pins
  `FORTH` (PTP_REM) and `FCALL` (PTP_NUMBER_16) resolving as items
  (`test_dict_reloc.c:1866-1880`), so the refactor turns the gate red on a
  preserved test. It also violates the accepted **B3** ruling ("do not make one
  unqualified helper silently serve both contracts") and names a nonexistent
  identifier (`ITM_SIN`; the real one is `ITM_sin`, `items.h:88`).

Independently of the two prompts, the **authority base is currently split**:
the R4 architecture interview's accepted decisions (recorded in
`FOR_THE_ARCHITECT_R4.md` and committed as `2cc6b1d03`) supersede several
normative DESIGN.md mechanisms (run-generation counters as truth, the 8-slot
scan list, the global colon chain, SST non-reset, advisory-only entry
validation) — and none of that has been folded into DESIGN.md. Any prompt
authored against DESIGN.md alone in those areas would codify contracts the
architect has already replaced. DESIGN.md additionally contains one
instruction that would **delete a live production fix** if followed (E7 /
P-H2 cursor-hack text vs. the landed R3-1 fix — AUD-H1).

Resolve the blockers in §13; the corrected path to GO is short (§12).

---

## 2. Artifact coverage manifest

Authority classes: 1 = DESIGN.md; 2 = architect decisions after its current
revision; 3 = CLAUDE.md / verified build mechanics; 4 = approved proposals;
5 = plans/audits/prompts/reports; 6 = DESIGN-HISTORY.md; 7 = older plans.

"Read": F = read in full; S = read structurally (head/section map + targeted
slices + git-status verification of every claimed-executed item). Every S-read
document is a superseded/historical artifact whose execution status was
verified against the tree and git history rather than trusted from its prose;
none is an input to pending execution.

| Path | Purpose | Class | Chronology | Read | Status | Depended on by |
|---|---|---|---|---|---|---|
| `design-docs/forth-core/DESIGN.md` | authoritative design | 1 | rev of 75506c49d (2026-07-15 19:55) | F | **current, with stale/conflicting passages** (§5) | everything |
| `FOR_THE_ARCHITECT_R4.md` "Accepted R4 architecture" | architect rulings post-DESIGN-rev for lifetime/scope/C/D | 2 | 2026-07-15, committed 2cc6b1d03 | F | current, **not folded into DESIGN.md** | future series, R1-3/R1-4 safety |
| `CLAUDE.md`, `BUILD.md` | project/build controls | 3 | current | F | current | all |
| `AGENTS.md` | Qwen execution contract | 3 | updated by R5-5 | F | current, accurate | all Qwen runs |
| `packages/forth-core/.pkgignore`, `build-test.sh` | gate + classification controls | 3 | post-R5-5 | F | current (banner check present) | gate |
| `design-docs/package-manager/README.md` | package-system authority | 3 | post-R5 | F | current; one stale self-contradiction (AUD-L2) | authoring |
| `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` | pkg-mgr spec rev 2 + BP/R5 approvals | 4 | post-R5-2 | S | current for pkg-mgr; implemented | tooling |
| `design-docs/forth-core/PROPOSED_SPEC_CHANGES.md` | fc spec proposals | 4 | 2026-07-13 | F | item 1 ratified (upstream MR pending); item 2 **stale-"PROPOSED"** — already ratified into DESIGN §3.3.6 (AUD-L3) | — |
| `QWEN_PROMPTS_R1_code_defects.md` | R1 defect prompts | 5 | 2026-07-15 08:19 | F | R1-1/R1-2 executed; **R1-3/R1-4 pending & defective** | execution |
| `QWEN_PROMPTS_R2_tests.md` (+appendix) | test-credibility repairs | 5 | 2026-07-15 | F | T1–T7 executed (T1 rode in 2bfc9ba51; per-task commits superseded T8) | — |
| `QWEN_PROMPTS_R3_pem.md` | bare-render + citation repair | 5 | 2026-07-15 | F | executed (453205de0 = R3-2's exact message) | — |
| `QWEN_PROMPTS_R4_engine.md` | 4 bounded engine fixes | 5 | 2026-07-15 | F | executed (bf29ad55f, b6a2a74dd, 1806d48d8, 390d7ba15); scope note correctly forbids future-work leakage | — |
| `design-docs/package-manager/QWEN_PROMPTS_R5_tooling.md` | pkg tooling hardening (R5-1..7) | 5 | 2026-07-15 | F | executed (e579d1821 = R5-6 message; R5-7 code + tests verified in `tools/`) | — |
| `QWEN_PROMPTS_pem-entry.md` | A1–A9 PEM entry fixes | 5 | 2026-07-15 05:23 | F | executed (dcc8d6594 = A9's exact message) | — |
| `STAGE2_QWEN_PROMPTS.md` | Q1–Q15 three pillars | 5 | 2026-07-13 | S | executed (H5/T1.x, D-3 context model, P2 pre-scan all verified in tree) | — |
| `design-docs/package-manager/QWEN_IMPLEMENTATION_PROMPTS.md` | **revision-1 (libclang)** pkg prompts | 5 | pre-revert | S | **superseded, unmarked** — execution hazard (AUD-M5) | none (must stay unused) |
| `design-docs/package-manager/QWEN_PROMPTS_refresh-base-commit.md` | BP-1..BP-7 base pinning | 5 | 2026-07-14 | S | executed (verified in `tools/pkg_patch_refresh.py` + tests) | — |
| `FOR_THE_ARCHITECT_R1.md` | citation audit | 5 | 2026-07-15 | F | current record; DESIGN fixes applied by 2a2d26fc1 | R1 prompts |
| `FOR_THE_ARCHITECT_R2.md` (+rulings) | test-suite audit | 5 | 2026-07-15 | F | findings 2–6 ruled (75506c49d); **finding 1 (§8.9) unruled** (AUD-M1) | — |
| `FOR_THE_ARCHITECT_R3.md` | PEM design findings A1–A3 | 5 | 2026-07-15 | F | A1/A2 undecided (A2 partially mooted by upstream `programBytesAvailable` guard); A3 ruled via R2-6 cap | — |
| `design-docs/package-manager/FOR_THE_ARCHITECT_R5.md` | tooling policy A1–A3 | 5 | 2026-07-15 | F | A1 approved+implemented (R5-7); A2 implemented (e688535e2); A3 ruled accepted-silent (README) | — |
| `design-docs/package-manager/manager-audit.md` | audit of the *old* declaration/resolver system | 7 | pre-redesign | F | superseded (system replaced twice); surviving ideas landed (sentinel, stubs, test_asan docs) | — |
| `CPLAN.md`, `Stage1.md`, `Stage-H1.md` | phase plans | 7 | Jul 8–9 | S/F/S | superseded; Stage1 still says `forthArena` (AGENTS.md already warns) | — |
| `STAGE2_THREE_PILLARS_PLAN.md` / `_AUDIT.md` | stage-2 plan + phase-1 audit | 5/7 | Jul 12–13 | S | executed/superseded ("nothing implemented yet" banner is a stale snapshot) | — |
| `TEST_PLAN_POST_AUDIT.md` | post-audit test plan | 7 | Jul 9 | S | superseded; its "helper is `forthPopIsZero` NOT `popIsFalse`" note contradicts current code+DESIGN (both `popIsFalse`) — historical only | — |
| `CONSOLIDATION-CHECKLIST.md` | 07-08 consolidation verify | 7 | Jul 8 | S | historical snapshot (quotes error codes since changed) | — |
| `CODE_REVIEW_SERIES.md` | the R1–R5 review commissioning doc | 5 | Jul 15 | S | executed (produced R1–R5 outputs) | — |
| `FIXES_DRAFT_PATCHES.md` | 15 must-fix patch drafts | 7 | Jul 9 | S | executed/superseded; explicit deferrals are tracked in TEST_PLAN (UNDO integration, DMCP key poll) | — |
| `PEM_AUDIT_REPORT.md`, `PEM_COMMITS.md`, `PEM_FIX_COMMITS.md` | PEM audit + per-commit prompts | 5/7 | Jul 10–13 | S | executed; PEM_FIX deferrals: F7 landed (`forthPickerGuard` menu-identity), F9.6 resolved, **1000-step-cap documentation still open** (AUD-L6); F8/F9.1 still untracked anywhere current | — |
| `ARCHIVE-3.3C-amendment.txt` | archived §3.3-C amendment text | 7 | Jul 8 | S | archived, correctly labeled | — |
| `UPSTREAM_BUG_REM_alpha_menu.md` | upstream bug report (REM/catalog) | 5 | Jul 15 | S | **confirmed & fixed upstream** in b8f79e486 (`fix-REM-in-PEM-from-FCNS-menu`); no package action | — |
| `qwen_changes.diff` (root) | historical DESIGN diff snapshot | 7 | Jul 11 | S | inert history | — |
| `design-docs/package-manager/IMPLEMENTATION_REPORT.md` | pkg-mgr implementation record | 5 | Jul 15 | S | current record; its "not independently audited" banner is now satisfied by R5 | — |
| Package sources: `forth_dict.c/.h`, `forth_inner.c`, `forth_compile.c`, `forth_bridge.c`, `forth_prims.c/.h` | current engine | — | HEAD | F | current | — |
| All 14 `patches/*.patch` | generated override deltas | — | HEAD | F | current, match DESIGN §6 hook set | — |
| `test_dict_reloc.c` (8568 ln, ~130 unique tests) | self-test suite | — | HEAD | S (targeted: every test bearing on findings; full name inventory) | current, complete (R1's "foreign unfinished tail" is gone; ends `#endif // PC_BUILD`, banner at :7726) | gate |

No artifact relevant to **pending execution** was skipped; the S-reads are
confined to documents that are superseded or fully executed, each verified by
tree/git evidence cited above.

---

## 3. Canonical requirements ledger (DESIGN.md, stable IDs)

Status legend: ✅ implemented+covered · ✅◐ implemented, coverage partial ·
⏳ deferred with authoritative reason · ✗ not started · ⚠ see finding.

**Items / status / dispatch (§0)**
- **REQ-IT1** ITM_FORTH=2842 `fnForthOuter`, CAT_FNCT|SLS|US|EIM_DISABLED|**PTP_REM**|HG (§0.2) — ✅ `items.c:4722`, `test_forth_step_ptp_rem`, `test_undo_rows_us_enabled`.
- **REQ-IT2** ITM_FCALL=2843 `fnForthCall`, PTP_NUMBER_16, tamMinMax 16383, RESULT_IN_X — ✅ `items.c:4723`, `test_fnforthcall_*`.
- **REQ-IT3** MNU_FORTH=213 CAT_MENU "FWRD" — ✅ `items.c:2020`, `test_mnu_forth_row`.
- **REQ-IT4** never grow `indexOfItems[]` past LAST_ITEM; only slots 2842/2843(+213) claimed — ✅ (upstream grew LAST_ITEM to 2870 itself; package adds no rows). ⚠ DESIGN.md §0.1 still cites LAST_ITEM 2860 and "free 213-219 at items.c:2003-2009" — both stale post-migration (AUD-M8).
- **REQ-IT5** generator stubs for new handlers — ✅ `items.c:1686-1687`.

**Token encoding (§2)**
- **REQ-TK1** C47 step encodings (`0x8B 0x1A 0xFD len …`; `0x8B 0x1B` + u16) — ✅, `test_forth_step_sizing`, `test_useritem_xeqp1_*`.
- **REQ-TK2** ftoken map: EXIT/PRIM(≤0x0FFF)/CALL(0x1000-0x7EFF)/LIT(16B)/ILIT(4B→dtLongInteger)/BR/0BR(i16 cells)/C47/XEQN; 0x7F06+ & 0x8000+ reserved — ✅ except **XEQN not implemented** (⏳ deliberately: R4 sequencing; R1-4 withdrawn per this audit). Reserved-token rejection ✅ `test_malformed_token`.
- **REQ-TK3** cell alignment invariant; PTP_NUMBER_8 padded; alignment-safe LE fetch — ✅ (`readToken` two-byte form; R4 ruled the "memcpy" spelling non-normative), `test_c47_ptp_number8_padded`.
- **REQ-TK4** FTOK_C47 PGM_RUNNING save/set/conditional-restore — ✅ `forth_inner.c:393-398`, `test_c47_nested_call_succeeds`.
- **REQ-TK5** prim emit = idx+1; never raw idx; prim table ≤0x0FFF `_Static_assert` — ✅ `forth_prims.c:51`. ⚠ DESIGN §7 still claims "no assert of any kind exists" (AUD-H2).

**Inner interpreter (§3.2)**
- **REQ-IN1** depth cap 4 → ERROR_OPERATION_UNDEFINED; rstack overflow → ERROR_RAM_FULL (distinct) — ✅ `test_reentrancy`, `test_rstack_overflow`.
- **REQ-IN2** watermark unwind, INNER_LEAVE on every exit, rsp==0 at rest — ✅ `test_nested_*`, `test_outer_ctx_at_rest`. ⚠ DESIGN §3.2 pseudocode itself uses two bare `return`s that would leak depth (AUD-M10) — code is right, doc wrong.
- **REQ-IN3** runaway cap backstop ERROR_RAM_FULL — ✅ `test_runaway_guard`.
- **REQ-IN4** DMCP per-dispatch key poll (36/33 → PGM_WAITING; else setLastKeyCode); PC: programRunStop check only — ✅ code `forth_inner.c:102-114`; ⏳ test hardware-only (documented deferral, TEST_PLAN).
- **REQ-IN5** ASLIFT set on normal exit; per-dispatch scrub — ✅ `test_stack_aslift`. ⚠ DESIGN §3.2 still carries the "Required code change (NOT applied here)" paragraph (AUD-H2).
- **REQ-IN6** popIsFalse type-dispatched; 0BR consumes — ✅ `test_0br_*`.
- **REQ-IN7** bad/stale entry index → ERROR_INVALID_CORRUPTED_DATA (never silent) — ✅ `forth_inner.c:191-196, 266-271`.
- **REQ-IN8** truncated operand/token reads rejected (R1-2) — ✅ `boundedRead`, `test_truncated_{token_fetch,inline_operand,c47_item_id}`. ⚠ Tension with accepted R4-E4 (restore-time validator instead of per-token checks) — reconcile when the validator lands (AUD-M9).
- **REQ-IN9** FTOK_C47 itemId bound < LAST_ITEM — ✅ `forth_inner.c:348-353`.

**Compiler / line discipline (§3.3, §3.3.1, §3.3.7-9)**
- **REQ-CP1** state local per line; `:`/`;` error cases → ERROR_INVALID_NAME; unterminated def aborts, never masks prior error — ✅ `test_unterminated_def_errors`, `test_overlong_token_in_def_keeps_error`.
- **REQ-CP2** grow-in-place openDef API; abort restores here/latest/count; emit failures abort — ✅ `forth_dict.c:290-371`. ⚠ DESIGN §3.3.7 still says the API "does not exist yet" (AUD-H2).
- **REQ-CP3** count cap 0x6F00 in startDefinition; 64KB wrap guard in Ensure — ✅ `test_dict_space_full`, `test_dict_capacity_arithmetic`.
- **REQ-CP4** WriteName clamps to allocated nameLen — ✅ (3-arg form). ⚠ DESIGN still lists it as a "known defect to fix" (AUD-H2).
- **REQ-CP5** smudge semantics (open word invisible; indices stable) — ✅ `forthFindColon` skips FF_SMUDGE.
- **REQ-CP6** immediacy prims-only (stage C) — ✅ (no colon-immediate path; `RECURSE` is accepted future work).
- **REQ-CP7** emit scope excludes BR/0BR (compiler) — ✅.
- **REQ-CP8** error table §3.3 — ✅◐ implemented as written **except** the "C47 label in compile state → ERROR_INVALID_NAME" row, which contradicts §3.3.6/§4.1's normative XEQN emission (AUD-H3). Current code = the table row; DESIGN pseudocode = XEQN. Both cannot stand.
- **REQ-CP9** `ERROR_FUNCTION_NOT_FOUND` context display incl. `(in WORD)` — ✅ `forth_compile.c:416-424`, error.c/screen.c hooks, width guard (`screen.c` patch).

**Source acquisition / re-entrancy (§3.3.2)**
- **REQ-SR1** per-invocation ctx on caller stack; OUTER_NEST_MAX 2 → OP_UNDEFINED; restore on every exit — ✅ `test_outer_depth_cap`, `test_outer_nesting_tokenizer`, `test_outer_ctx_at_rest`.
- **REQ-SR2** private source buffer (never tmpString/aimBuffer); copy before drop — ✅ `fnForthOuter`.
- **REQ-SR3** X non-string → INVALID_DATA_TYPE; overlong → no silent truncation — ✅ `test_outer_nonstring_x` (overlong path untested — minor).
- **REQ-SR4** `forthProgramStep`: copy payload before interpreting; gen-check; pre-scan; SKIP_DEFS — ✅ (code orders copy after the pre-scan but before any interpretation of *this* payload; the pre-scan makes its own private copies per step, so the §3.3.2 rationale — never interpret out of program memory — holds; cosmetic divergence from the snippet's line order only).
- **REQ-SR5** nested line can never close/abort outer def — ✅◐ `openDef.open` protected; **dictionary bytes not transactional** under a nested abort (R4-E3) — ⏳ accepted ruling: document internal precondition, no code now. The precondition comment is not yet written anywhere (fold with AUD-H2 batch).

**Tokenizer (§3.3.3)**
- **REQ-TZ1** glyph-wise advance; delimiter 0x20 only; TOKEN_MAX 63 → INPUT_TOO_LONG; names ≤31 bytes — ✅ `test_outer_glyph_*`, `test_dict_name_too_long`, `test_picker_glyph_tokenize`.

**Numbers (§3.3.5)**
- **REQ-NU1** int→dtLongInteger via closeNim idiom; real→real34; identical both states — ✅ `test_ilit_compile_interpret_parity`, `test_outer_real_literal`.
- **REQ-NU2** exact grammar incl. R4-1 sign-position; NaN/Infinity never reach decQuad; '.' radix; e/E only; base 10 — ✅ `test_number_*` (+`bad_exponent_sign_position`). No focused "NaN"/"Infinity" literal test (LOW gap, §8).
- **REQ-NU3** +skip / range-check via longIntegerCompareInt / free on all paths; out-of-range → real34 both states — ✅.

**Lookup (§4)**
- **REQ-LK1** forward order prim→colon→number→**item**→label; case-sensitive CMP_BINARY — ✅◐ **item step missing** (R1-3's subject): `forth_compile.c` jumps number→label. ⚠ pending prompt defective (AUD-B2).
- **REQ-LK2** forthFindPrim miss = FORTH_PRIM_NONE (never ≥0) — ✅.
- **REQ-LK3** forthFindColon bool + out-param, newest-first, skips smudge — ✅.
- **REQ-LK4** item filter CAT_FNCT && PTP_NONE (stage; §4.4 phases widen later) — ✗ (with REQ-LK1; note R4-C1 accepted: series C widens to all native param types).
- **REQ-LK5** label step: compile → FTOK_XEQN; interpret → `dynamicMenuItem=-1; fnExecute` — ✅ interpret (`forth_compile.c:404-405`, `test_xeq_word_still_calls`); ✗ compile (R1-4's subject; currently INVALID_NAME) — ⏳ re-sequenced per R4 (AUD-B1).
- **REQ-LK6** reverse resolver label→item→colon; LBL? gating to XEQ/XEQP1; XEQP1 return-step bump — ✅ `forthResolveXEQ`, lblGtoXeq patch, `test_xeq_precedence`, `test_xeq_item_lookup`, `test_lblq_forth_name_not_local_label`, `test_gto_*`. ⚠ resolver item arm is CAT_FNCT-only (no PTP filter) and dispatch sites pass NOPARAM to any matched item — R4-B3's dual-contract hazard, currently pinned by tests as intended behavior (AUD-M11).
- **REQ-LK7** PEM records names, never widx (FCALL redirect, ERROR_NON_PROGRAMMABLE_COMMAND; tam.c hook) — ✅ `test_fcall_redirect_*`, tam.c patch.
- **REQ-LK8** upstream-migration label typing: bare-name and resolver lookups GLOBAL_LABELS only; LOCAL_LABEL steps never fall through to Forth — ✅ (migration hand-resolution, lblGtoXeq patch) — post-DESIGN decision recorded only in code comments + commit message; fold into DESIGN §4.2 (AUD-M8).

**Dictionary & arena (§1, §5)**
- **REQ-DA1** header layout/alignment/region-relative links — ✅ core reloc tests.
- **REQ-DA2** lazy alloc honors first request (R1-1/R4-2) — ✅ `test_dict_first_ensure_capacity`, `test_dict_capacity_arithmetic`.
- **REQ-DA3** grow = max(double, need); base refresh; offsets-only across emits — ✅.
- **REQ-DA4** validator: scalars, chain walk, nameLen, name extent (R4-3), link decrease, count match; orphan-don't-free — ✅ `test_validate_direct_corruption` V1-V3, `test_restore_validation_clamps`.
- **REQ-DA5** §5.4 cost formula + fixed overheads — ✅ (doc). ⚠ §5.4 also says the 256-byte source buffer is BSS; it is caller-stack since D-3 (R4 doc note, fold with AUD-H2).
- **REQ-DA6** hwm bench `bench/hwm.fs` + report rule — ✗ **file absent** (AUD-M4); de-facto substitute: suite's `FORTH ARENA:` line, quoted in every dictionary-touching commit (observed: R2/R4/75506c49d messages). ≤2KB ceiling: observed here=36/sizeBlocks=16 — far inside budget.
- **REQ-DA7** freeListFree overlap guard, unconditional, non-mutating — ✅ `core/freeList.c` patch, 3 freelist tests. Upstream MR still pending (tracked, PROPOSED_SPEC_CHANGES).

**Save/restore & reset (§5.5, §6.2)**
- **REQ-SV1** five name-keyed params after programList, defaults pre-seeded, validator on restore — ✅ saveRestoreBackup patch, `test_save_restore_roundtrip`, `test_restore_missing_params_defaults`.
- **REQ-SV2** doFnReset → forthDictInit; run-once self-test guard; Init-vs-Clear distinction — ✅ config patch, `test_lifecycle_real_reset_hook` (fork-isolated), `test_program_step_gen_reset` (Clear path).

**Hooks (§6)** — **REQ-HK1..HK17**: all H1-H10 and P-H1..P-H7 hooks verified
present in the 14 generated patches, byte-scoped as specified (items.c/.h,
defines.h 22→23, error.c, screen.c fallback+width guard, keyboard.c fallback+
picker+_closeCatalog export, softmenus.c menus+builder+rebuild-always+
isAlphaSubmenu, manage.c E0-E7+FCALL redirect+F4, decode.c render arm,
lblGtoXeq.c REM-arm dispatch+bump sites+FIX-3+XEQP1, saveRestoreBackup.c,
tam.c, core/freeList.c, config.c). ⚠ Hook-table rows H6/P-H2/P-H5 lack the
[LANDED] marker their siblings carry, and P-H2's cursor-hack sentence is the
AUD-H1 contradiction.

**PEM-native entry (§8)**
- **REQ-PE1** stored representation (len>0 source / len==0 marker; empty line unrepresentable; ≤255) — ✅ `test_forth_step_*`, E3 tests.
- **REQ-PE2** pre-scan DEFS_ONLY→SKIP_DEFS, owning-program scope, first-touch, rollback on error (R4-4) — ✅ `test_prescan_*` (8 tests incl. rollback, two-program third-touch, owning scope).
- **REQ-PE3** run-generation lazy reset; bump sites exactly 2; non-bump R/S+SST — ✅ code+`test_program_step_gen_reset`, `test_prescan_generation_rearm`. ⚠ **superseded in direction** by accepted R4 lifetime rulings (pending-reset flag as truth, PEM single-step = fresh generation, active-frame guard) — DESIGN.md not yet amended (AUD-B3). No STOP→R/S resume test (§8.9-9b) exists (§8).
- **REQ-PE4** E0–E8 entry routing — ✅ all landed (manage.c patch; E1 drain uses the stack-wide predicate + bounded loop — improvement over DESIGN's literal pseudocode, 59f58dbe3; DESIGN text not updated, part of AUD-M8). Toggle-close clears tam.function (R2-5 ruling) ✅ `test_forth_toggle_close_resets_sentinel`; multi-line lock ✅; ALPHA gesture ✅; EXIT ladder ✅; capture-survives-keystroke ✅ — all driven through `runFunction`+`_closeCatalog` per the A8 discipline.
- **REQ-PE5** E9 entry-time validation (advisory, check-only) — ✗ **not implemented** (no check-only mode exists; modes are FULL/DEFS_ONLY/SKIP_DEFS) and **redefined** by accepted R4-D (lexical/structural = atomic reject; names stay unresolved-legal). DESIGN.md must be amended before any D prompt (AUD-M2).
- **REQ-PE6** §8.5 render: bare source, »FORTH/FORTH« parity, transient-capture exception — ✅ decode patch, `test_decode_source_bare`, `test_decode_marker_directions`; live-capture cursor fix R3-1 ✅ — ⚠ contradicted by DESIGN E7/P-H2 text (AUD-H1).
- **REQ-PE7** §8.6 picker: static ALPHA-row submenu, ≤14-byte names omitted, dedupe, 170-name cap truncate-by-scan-order, rebuild-always, insert-at-cursor+space, menu-identity guard — ✅ softmenus/keyboard patches, 10 picker tests incl. `test_picker_capacity_boundary` (170/171 edges). ⚠ builder's owning-program loop still last-qualifying-wins (AUD-M3); 1000-step scan cap undocumented in §8.6 (AUD-L6).
- **REQ-PE8** §8.7 error surface (halt at step, no advance) — ✅ `test_exec_step_halts_on_error` (real program), runProgram guard.
- **REQ-PE9** §8.8 naming/width — ✅.
- **REQ-PE10** §8.9 acceptance items 1–10 — ✅◐ **unit-level analogs only**; the specified end-to-end paths (XEQ/runProgram-driven define-and-use, BST/SST/listing agreement, power-off keypad-state round-trip, alpha-keypad glyph program run, STOP→R/S resume, PEM XEQ+alpha recording chain) remain untested. R2 finding 1 (certification wording) is **unruled** (AUD-M1).

**Open questions (§8.10)**
- **OQ-1** cross-program visibility — still OPEN, no test; **direction now settled** by accepted R4 scope rulings (per-program local scopes), which also contradict the §8.10 "global within one generation" prose — needs one DESIGN.md reconciliation, not a test of the doomed behavior (AUD-B3/AUD-M6).
- **OQ-2** R/S-after-GTO boundary — documented, benign; subsumed by the R4 pending-reset design when it lands.
- **OQ-3** words invisible to catalogs/ASSIGN — deferred (authoritative).
- **OQ-4** interactive FORTH needs string in X — deferred (authoritative); note R4 capture-submode section is the accepted future shape.

---

## 4. End-to-end traceability matrix

The ledger above carries per-requirement location/implementation/tests/status.
This matrix lists only rows whose traceability is **incomplete or findings-bearing**;
every requirement not listed here is fully traced (implemented + named tests, or
explicitly deferred with the authoritative reason cited in §3).

| Req | Design loc | Planned work item | Prompt/step | Target files | Test | State | Finding | Correction |
|---|---|---|---|---|---|---|---|---|
| REQ-LK1/LK4 (forward item step) | §4.1 step 4, §3.3 pseudocode | "Next series B" (pem-entry file) → R1 defect 1 | **R1-3 (pending)** | forth_dict.h/.c, forth_compile.c, tests | planned in-prompt | not started | AUD-B2 (self-contradictory refactor; ITM_SIN; B3 conflict) | rewrite per §10.1, then execute |
| REQ-LK5-compile / REQ-TK2-XEQN | §3.3.6, §2.2 | "Next series B" → R1 defect 2 | **R1-4 (pending)** | forth_compile.c, forth_inner.c, tests | planned in-prompt | not started | AUD-B1 (B1/B4 conflicts; superseded sequencing) | withdraw; re-author inside the R4-ordered series (§12 step 4) |
| REQ-CP8 error-table label row | §3.3 table | — | (interacts with R1-4) | — | none pins it (verified) | implemented-as-written but contradicts §3.3.6 | AUD-H3 | delete/annotate the row when XEQN lands; until then mark "stage-interim" |
| REQ-PE3 lifecycle truth-predicate | §8.3 | R4 accepted arch. (lifetime 1-4, E1, E2) | **no prompts yet** (correctly withheld) | forth_compile.c, lblGtoXeq.c | future | superseded-in-direction | AUD-B3 | fold rulings into DESIGN.md before authoring |
| REQ-PE5 / E9 | §8.4 E9 | R4-D accepted ruling | none | forth_compile.c, manage.c | none | not started + spec conflict | AUD-M2 | amend E9 to the accepted split (structural=atomic reject; names advisory) before any D prompt |
| REQ-PE10 §8.9 end-to-end | §8.9 | R2 finding 1 (unruled) | none | test_dict_reloc.c (+harness path work) | missing (unit analogs only) | partial | AUD-M1 | rule finding 1: implement per item or downgrade wording; schedule the reachable-path harness work |
| REQ-DA6 bench | §5.4 | "to be added" since stage 1 | none | bench/hwm.fs | n/a | not started | AUD-M4 | either add the benchmark or amend §5.4 to bless the suite's `FORTH ARENA:` line as the reporting mechanism |
| OQ-1 | §8.10-1 | R1 attempted probe (blocked then); R4 scopes supersede | none | — | none | open/superseded | AUD-M6 | resolve by DESIGN amendment (scopes), not by testing the superseded behavior |
| REQ-PE7 builder scan | §8.6 + R4-E5 | E5 ruling (applied to bridge only) | none | softmenus.c | `test_owning_program_start_max_not_last` covers bridge only | partial | AUD-M3 | small follow-up prompt (§10.3) |
| REQ-IN8 vs R4-E4 | §3.2/R4 | restore-time validator (accepted) | future series step 1 | forth_dict.c, forth_inner.c | future | tension documented | AUD-M9 | decide: keep boundedRead as belt-and-suspenders or remove with the validator (flash cost rule) |

---

## 5. Conflict and contradiction register

**BLOCKER**

- **AUD-B1 — R1-4 vs accepted R4 architecture.**
  Evidence: `QWEN_PROMPTS_R1_code_defects.md:296-301` ("FORTH_XEQ_COLON: save
  programRunStop, set PGM_RUNNING, call forthInner(...)") vs
  `FOR_THE_ARCHITECT_R4.md:283-292` (B4 accepted: colon call "must not
  synthesize PGM_RUNNING"); `…R4.md:185-219` (B1: XEQN label dispatch + the
  current `forthRunGenCheckReset` = dictionary freed under a suspended
  `forthInner`; "not executable today only because FTOK_XEQN…is not
  implemented"); `…R4.md:517-538` (accepted order: lifetime foundations §1
  before XEQN §3); `QWEN_PROMPTS_R4_engine.md:10-18` ("do not add FTOK_XEQN …
  It still needs separate prompts"). Consequence: executing R1-4 as written
  opens a use-after-free path on the handheld (interactive `: CALLP P ; CALLP`
  where P contains a Forth step: `fnExecute` bumps the generation, the callee's
  first Forth step calls `forthDictClear()`, the suspended frame resumes into
  freed arena) and codifies a colon-dispatch protocol the architect rejected.
  Resolution: **withdraw R1-4**; mark it superseded in the file; re-author
  within the R4-ordered future series. Blocks execution: **yes**.

- **AUD-B2 — R1-3 self-contradiction + B3 conflict + wrong identifier.**
  Evidence: `QWEN_PROMPTS_R1_code_defects.md:216-218` ("Refactor only the item
  arm of forthResolveXEQ to use this helper" — the helper requires
  `PTP_STATUS == PTP_NONE`, :211-215) vs :237-239 ("Leave test_xeq_item_lookup
  and test_xeq_precedence unchanged") vs `test_dict_reloc.c:1866-1880`
  (`forthResolveXEQ("FORTH")`→ITEM with ITM_FORTH = **PTP_REM**;
  `"FCALL"`→ITEM = **PTP_NUMBER_16**) and `forth_dict.c:434-443` (resolver item
  arm filters CAT_FNCT only). Also `…R1….md:229-230` "returns ITM_SIN" — the
  identifier is `ITM_sin` (`items.h:88`). R4-B3 accepted ruling
  (`FOR_THE_ARCHITECT_R4.md:261-266`): do not make one unqualified helper serve
  both contracts. Consequence: verbatim execution turns the gate red on
  preserved tests (safe stop, but the prompt cannot succeed) or tempts an
  unauthorized contract change to the reverse resolver. Resolution: §10.1
  rewrite (drop the resolver refactor; fix the identifier; refresh stale
  preamble/commit text). Blocks execution: **yes** (for R1-3 as written).

- **AUD-B3 — Accepted R4 architecture not folded into DESIGN.md.**
  Evidence: DESIGN.md §8.2 (`forthScannedProgs[FORTH_SCAN_MAX=8]`, :1840-1843),
  §8.3 (generation counters normative; "Deliberate non-bump sites: fnRunProgram
  (R/S) and SST", :1934-1957; "Generation wrap … harmless", :1963-1964), §8.10
  ("dictionary … global within one generation", :2498-2511), §8.4 E9 advisory
  (:2228-2239) — each superseded by `FOR_THE_ARCHITECT_R4.md` accepted rulings
  (lifetime 1-4 incl. **PEM single-step = fresh generation** and
  **pending-reset flag as the truth predicate**; vocabulary 6-7 **per-program
  local scopes** + interactive scope; E1 dynamic scan tracking; E2 counter =
  diagnostics only; D structural-atomic validation), committed as decisions in
  `2cc6b1d03`. Consequence: two live authorities disagree about lifecycle,
  scan tracking, scoping, SST semantics and validation; any prompt authored
  from DESIGN.md in these areas is wrong on arrival (R1-4 is the first
  casualty). Resolution: one DESIGN.md amendment pass recording the accepted
  architecture (as target-state sections or explicit "superseded by R4,
  pending redesign" banners), plus DESIGN-HISTORY entry. Blocks execution:
  **yes** for any prompt touching lifecycle/lookup/validation — including both
  pending prompts.

**HIGH**

- **AUD-H1 — DESIGN.md instructs deleting a live fix (E7 / P-H2 cursor hack).**
  DESIGN.md :2196-2201 ("needs **no** `"FORTH "` branch: … the existing default
  … is already correct … If a branch exists … delete it — dead code") and the
  §6 P-H2 row :1650 — vs the landed R3-1 fix: `patches/010-programming__manage.c.patch`
  :25-36 (`if(tam.function == ITM_FORTH) cursorInString = T_cursorPos - 2;`,
  commit 453205de0) which exists precisely because the default was **wrong**
  for bare-rendered lines (cursor landed +2 into the payload; `FORTH ''` shown
  for the empty placeholder — `QWEN_PROMPTS_R3_pem.md:59-69`). Consequence: a
  future DESIGN-following edit removes the fix and regresses live capture.
  Resolution: rewrite E7's paragraph and the P-H2 row to describe the landed
  `-2`/empty-render behavior. Blocks execution: no (no pending prompt touches
  it) — but fold into the same amendment pass as AUD-B3.

- **AUD-H2 — DESIGN.md carries stale "required change / does not exist" claims (R4 list, unapplied).**
  Verified still present: :657 (ASLIFT "Required code change … NOT applied
  here" — landed, `forth_inner.c:229-233`); :986-990 ("remove `static` from
  forthPushInt32/forthPushReal34" — public, `forth_dict.h:125-126`); :1012-1015
  ("forthPushInt32 … currently does int32ToReal34" — false, `forth_inner.c:42-54`);
  :1137-1141 (emit/start/finish/abort "do **not exist yet**" — they exist,
  `forth_dict.c:290-371`); :1164-1167 (count cap "no committed code checks
  today" — checked, `forth_dict.c:327-330`); :1216-1220 (WriteName "known
  defect" — fixed, 3-arg clamp); :1740-1744 ("no assert of any kind exists in
  the tree" — `forth_prims.c:51`); plus §3.3.2/§5.4 BSS-vs-stack wording and
  the §2.2 "keep the token fetch as memcpy" spelling (R4: normative requirement
  is alignment-safe LE read). Consequence: the authority document instructs
  re-applying landed work; a prompt author quoting these slices would emit
  no-op or regressive tasks. Resolution: rewrite each as an implemented
  invariant with grep anchors (R4's suggested correction). Blocks execution:
  no single pending prompt quotes them — but this is the same amendment pass.

- **AUD-H3 — DESIGN.md internal conflict: §3.3 error table vs §3.3.6/§4.1 (compile-state labels).**
  :797 ("C47 label in compile state | ERROR_INVALID_NAME (48)") vs :759-771 +
  :1092-1095 (compile state **emits FTOK_XEQN**; "labels are never baked …
  they go through FTOK_XEQN", :395-396). The table row describes the current
  interim code (`forth_compile.c:377-382`), the pseudocode the target.
  Consequence: R1-4-class work has two normative answers in one document.
  Resolution: annotate the row as the stage-interim behavior superseded when
  XEQN lands (or move it to a staging note). Blocks execution: only with R1-4.

**MEDIUM**

- **AUD-M1 — §8.9 still reads as a delivered acceptance suite; R2 finding 1 unruled.**
  `FOR_THE_ARCHITECT_R2.md:10-39` (per-item gap table) — commit `75506c49d`
  ruled findings 2-6 only. Unit analogs exist; the specified end-to-end paths
  do not (verified: `test_dict_reloc.c:1628-1630` TODO; no XEQ/runProgram-driven
  define-and-use, no BST/SST agreement, no power-off keypad case (d), no
  STOP→R/S test, no alpha-keypad `8 D2 → 4` program run). Resolution: rule it —
  either schedule the harness work or downgrade §8.9's wording to "planned
  acceptance" per item.
- **AUD-M2 — E9 unimplemented and redefined** (see REQ-PE5). DESIGN.md's
  "advisory … must not block the commit" (:2236-2237) vs R4-D "reject the
  commit atomically" for structural malformation (`…R4.md:408-413`). Amend
  before any D prompt.
- **AUD-M3 — Picker builder ignores the E5 ruling.**
  `patches/010-softmenus.c.patch:67-71` still assigns the *last* qualifying
  `programList[i]` (order-dependent) while `forth_bridge.c:58-74` was fixed to
  compute the max explicitly (8da24c062) and now carries the "do not rely on
  ordering" comment. Same latent trap, second site. Small bounded prompt
  (§10.3).
- **AUD-M4 — `bench/hwm.fs` absent** though §5.4 makes it the reporting
  mechanism ("to be added", :1537-1539). De-facto substitute (suite arena line,
  quoted in commits) works; make DESIGN say what practice does, or add the file.
- **AUD-M5 — `design-docs/package-manager/QWEN_IMPLEMENTATION_PROMPTS.md` is the reverted
  revision-1 (libclang) prompt set with no supersession banner** (Prompt 2 =
  "Function-Boundary Extractor (libclang)", :215) while
  `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md:1-8` declares revision 2 supersedes
  it and `README.md:9-13` records the revert. An operator pasting from it would
  rebuild the reverted system. Add a one-paragraph ARCHIVED banner.
- **AUD-M6 — §8.10-1 vs accepted scopes** (see OQ-1): the "Not a gap … global
  within one generation" paragraph (:2498-2511) now contradicts the accepted
  per-program-scope ruling (`…R4.md:90-100`). Fold with AUD-B3.
- **AUD-M7 — R1 prompt-file staleness beyond the two tasks:** PREAMBLE
  precondition about the "foreign unfinished audit_probe_* tail" (:48-57) is
  obsolete (file complete, zero matches); R1-4's commit step (:336-342) says
  "one commit for R1-1 through R1-4" with message "forth-core: honor verified
  Forth runtime contracts" — R1-1/R1-2 are already committed separately
  (1806d48d8, 390d7ba15). Must be rewritten with the surviving scope.
- **AUD-M8 — Post-migration citation drift in DESIGN.md.** §0.1 LAST_ITEM
  2860 (:57) vs 2870 (`items.h:2992` both trees); "free slots … 213-219 at
  items.c:2003-2009 [VERIFIED: packages/forth-core/items.c:2003]" (:74-76) —
  line 2003 is now item 195 and 213 is MNU_FORTH (already FALSE at R1 time,
  worse now); ITM_FORTH row cited at items.c:4707 (:163) now :4722; §4.2
  call-site line numbers shifted; E1's drain pseudocode (:2037-2062) no longer
  matches the landed bounded-drain shape; the GLOBAL_LABELS/labelType selector
  decision exists only in code comments and the migration commit message.
  One citation-refresh pass (same class as 2a2d26fc1).
- **AUD-M9 — R1-2 per-token bounds checks vs accepted R4-E4** ("Use one full
  restore-time validation, not per-token production bounds checks"). Both
  landed/accepted; when the validator arrives, decide explicitly whether
  boundedRead stays (defense-in-depth at flash cost) or goes. Record the
  decision; do not let the future prompt silently strip R1-2.
- **AUD-M10 — DESIGN §3.2 pseudocode leaks on two paths** (:568 prim-error
  `return`; :571 rstack-guard `return`) while its own prose mandates
  INNER_LEAVE on *every* exit (:526-534). Code is correct
  (`forth_inner.c:250-252, 258-263`). Fix the pseudocode.
- **AUD-M11 — Reverse-resolver item dispatch passes NOPARAM to any CAT_FNCT
  item** (`forth_dict.c:434-443` no PTP filter;
  `patches/010-items.c.patch:43-45` and R1-4's planned XEQN item arm dispatch
  `reallyRunFunction(param, NOPARAM)`). A user gesture `XEQ` on a name matching
  a parameterized item (no label of that name) dispatches it with NOPARAM =
  65535. Currently pinned by `test_xeq_item_lookup` as intended. R4-B3's
  accepted ruling ("a bare parameterised item with no parameter is an atomic
  syntax error") will change this; until then it is a documented-behavior
  hazard needing an interim architect statement (§11-Q4).

**LOW**

- **AUD-L1** DESIGN-HISTORY.md:468-471 calls `_saveProgram`/`fnLoadProgram`
  "binary … raw program bytes"; DESIGN.md :2517-2521 (correctly) says textual
  decimal-per-line. History file has the bug (per its own rules).
- **AUD-L2** `design-docs/package-manager/README.md` Limitations (:506-509) still claims "No
  existing package uses this system yet — forth-core needs migration",
  contradicting Current Status (:16-28).
- **AUD-L3** `design-docs/forth-core/PROPOSED_SPEC_CHANGES.md:20-22` marks the
  fnExecute label-arm change "PROPOSED"; it was ratified into DESIGN §3.3.6
  (DESIGN-HISTORY 2026-07-13). Companion stale comment: `forth_compile.c:384`
  "C-1 amendment (proposed, see PROPOSED_SPEC_CHANGES.md)".
- **AUD-L4** Production comments cite pre-renumber sections (`§9.2/§9.3/§9.4/
  §9.6/§9.10` in forth_dict.h, forth_bridge.c, forth_compile.c, manage.c,
  softmenus.c, keyboard.c patches; e.g. softmenus "documented deviation
  (§9.6)"). R2-T7 fixed the test file only.
- **AUD-L5** softmenus.c patch: duplicated `/* 023 */` row comment
  (`patches/010-softmenus.c.patch:31-32`); MNU_FORTH inserted mid-table against
  the upstream "add at the end, numbers fixed for Wiki" comment — deliberate
  (dynamic-area contiguity requires it) but undocumented; one sentence in
  DESIGN §8.6 would prevent a future "fix".
- **AUD-L6** The picker's 1000-step scan cap exists only as a code comment
  claiming "§9.6 documented deviation" — §8.6 documents no such cap (tracked
  as deferred since PEM_FIX_COMMITS).
- **AUD-L7** PEM_FIX_COMMITS deferrals F8 (reject-path cursor drift) and F9.1
  (phantom marker on power-off) appear in no current tracking document —
  either close them with evidence or carry them forward.
- **AUD-L8** No focused tests: "NaN"/"Infinity" literal rejection; prim-index-0
  emit; fnForthOuter overlong-X rejection (all noted by R1's absolutes table).
- **AUD-L9** `Stage1.md` still describes `forthArena` with no superseded
  banner (AGENTS.md warns, but the file itself doesn't).

---

## 6. Per-Qwen-prompt verdicts

| File / step | Status | Verdict as an executable packet |
|---|---|---|
| R1-1, R1-2 | executed (1806d48d8, 390d7ba15) | done; historical |
| **R1-3** | **pending** | **UNSAFE as written** — AUD-B2 (self-contradictory refactor; false "tests that encode the old contract: none" claim — `test_xeq_item_lookup` encodes it; `ITM_SIN` identifier; stale preamble/commit instructions AUD-M7). Structure otherwise strong (anchors verified against current tree: "step 3: number"/"step 4: C47 label" greps match; LAST_ITEM symbolic). Needs the §10.1 rewrite, then executable. |
| **R1-4** | **pending** | **UNSAFE — withdraw** (AUD-B1: B1 lifetime hazard, B4 colon-wrap conflict, architect-mandated re-sequencing; also inherits AUD-M7 staleness and the AUD-H3 spec fork). Do not repair in place; re-author within the future series after lifetime foundations. |
| R2-T1…T7 | executed (T1 rode in 2bfc9ba51; T2-T7 per-task commits) | done. T8's single-commit design was superseded by per-task commits — acceptable deviation, record only. |
| R3-1, R3-2 | executed (453205de0) | done |
| R4-1…R4-4 | executed (bf29ad55f, 1806d48d8, b6a2a74dd) | done; the file's future-work fence (:10-18) is exemplary and remains binding |
| R5-1…R5-7 | executed (7ca5b1e29, e688535e2, e579d1821, 9010c2af8 series) | done |
| pem-entry A1–A9 | executed (dcc8d6594) | done |
| STAGE2 Q1–Q15 | executed | done |
| custom_package refresh-base-commit P1–P8 | executed | done |
| custom_package QWEN_IMPLEMENTATION_PROMPTS | never to execute | **superseded rev-1 — add ARCHIVED banner** (AUD-M5) |

Packet-quality note against the §5 checklist of the audit charter: the R1-R5 /
pem-entry generation of prompts carries authoritative excerpts, single bounded
objectives, exact files/anchors, non-goals, error codes/bounds, todo-list
mandate (AGENTS.md), the sole gate command + banners, refresh implications,
dirty-tree warnings, prohibited paths/git ops, STOP conditions, and required
report evidence — the template is sound. The defects found are **content**
defects (stale facts, cross-authority conflicts), not template gaps. The one
systemic weakness: prompts pre-date the upstream migration and the R4
interview, and nothing in the process forces a re-verification pass over
not-yet-executed prompts when the base moves — R1-3/R1-4 are the demonstration.

---

## 7. Current implementation status by design area

| Area | State |
|---|---|
| Dictionary core (§1, §5.2-5.3, §3.3.7-8) | implemented & covered; capacity arithmetic hardened (R1-1/R4-2/R4-3) |
| Token set (§2.2) | implemented & covered **except FTOK_XEQN** (deliberately deferred) |
| Inner interpreter (§3.2) | implemented & covered incl. truncation guards; DMCP poll code present (hardware test deferred) |
| Compiler/outer (§3.3) | implemented & covered; forward **item step missing** (R1-3's subject); compile-state labels interim-reject |
| Numbers (§3.3.5) | implemented & covered (incl. R4-1 edge) |
| Lookup fwd/rev (§4.1/4.2) | reverse complete incl. LBL? gating + XEQP1; forward lacks step 4 |
| Arena/save/reset (§5.4-5.6, §6.2) | implemented & covered; bench file absent (AUD-M4); freeList guard upstream-MR pending |
| Hooks (§6) | all 17 landed, verified in generated patches |
| PEM entry (§8.1-8.8) | implemented & covered at unit level; E9 not implemented (redefined by R4-D) |
| §8.9 acceptance | unit analogs only; end-to-end paths open (AUD-M1) |
| Lifecycle/scopes (§8.3 + R4 arch) | current design implemented; **accepted replacement architecture not yet specified in DESIGN.md nor prompted** |
| Package system & tooling | complete through R5-7; manifest atomicity + fatal-corrupt landed |
| Upstream migration | complete (b8f79e486); anchors in docs/prompts partially stale (AUD-M8) |

---

## 8. Missing and inadequate tests

Beyond AUD-M1's end-to-end acceptance list (the largest block), in charter order:

- **Dictionary relocation** — covered (forced-small initial region; reloc across realloc).
- **Malformed/truncated streams** — covered (3 truncation tests + malformed-token trio + validator V1-V3).
- **Bounds/overflow** — covered post R4-2/R4-3/R1-1.
- **Re-entrancy & compiler-state restore** — covered (depth caps, watermark, ctx-at-rest); nested-abort dictionary transactionality intentionally out (R4-E3 accepted) but the promised precondition comment is unwritten.
- **Lookup precedence** — forward: number-before-label & prefix covered; item-step tests arrive with R1-3. Reverse: covered (label>item>colon, LBL?, GTO negatives).
- **Reverse lookup / recording** — covered (FCALL redirect + stale-reject + tam.c name recording at unit level; §8.9-10's full PEM chain open).
- **Program-scoped lifetime** — generation reset/rearm/no-recompile covered; **STOP→R/S resume (8.9-9b) and SST-retention have no test**; whole area to be re-tested under the R4 redesign anyway.
- **PEM pre-scan/entry-only** — strong (8 prescan tests incl. rollback + owning-scope; entry-state derivation incl. RPN-gap and ENTER-continuation).
- **Save/restore & reset** — covered incl. fork-isolated real hook; double-free trio present.
- **Error surfaces** — covered (context display concatenation tested indirectly via message probes; width-guard path untested — LOW).
- **Flash/RAM/high-water reporting** — arena line asserted by convention in commits, not by a test; bench absent (AUD-M4).
- Specific small gaps: AUD-L8's three (NaN/Infinity literal, prim-0 emit, overlong-X), DMCP key poll (hardware, documented deferral), cross-program visibility (superseded — do not write; see AUD-M6).

---

## 9. Dependency graph and critical path

Single-writer rule holds (one shared working tree; AGENTS.md). Read-only
review/measurement may parallelize; nothing else may.

```
A. Architect DESIGN.md amendment pass                      [no code; BLOCKS everything below]
   ├─ fold R4 accepted architecture (AUD-B3, AUD-M6, AUD-M2/E9)
   ├─ fix E7/P-H2 (AUD-H1), stale claims (AUD-H2), table row (AUD-H3)
   ├─ citation refresh post-migration (AUD-M8) + LOW batch (L1-L6, L9)
   └─ rule R2 finding 1 (AUD-M1) and the §11 questions
B. Rewrite R1-3 (§10.1)            ── depends on A (B3 wording) ── then EXECUTE (green baseline required)
C. Withdraw R1-4; banner rev-1 pkg prompts (§10.2, AUD-M5)   ── with A
D. Small follow-ups (§10.3: picker max-scan, precondition comment, bench-or-amend)
   ── independent of B; each needs a green baseline
E. Author future series per R4 order (lifetime → shared param decoder → XEQN/vocab
   → C params → D validation → capture submode), each stage: DESIGN excerpt,
   traced native behavior, old-contract test migration, executed RED mutation
   ── depends on A and B (forward item lookup is the vocabulary substrate)
```

**Critical path:** A → B(execute) → E-stage-1 (lifetime foundations). D and C
ride along at any green point. Stage gates: green `build-test.sh` (both
banners + exit 0) after every prompt; arena line quoted in every
dictionary-touching commit (CLAUDE.md).

---

## 10. Exact prompt corrections required before execution

**10.1 R1-3 (rewrite in place):**
1. Delete "Refactor only the item arm of forthResolveXEQ to use this helper,
   preserving that function's different order label > item > colon." Replace
   with: "Do NOT touch `forthResolveXEQ` or `forth_dict.c`'s reverse lookup in
   any way: its item arm deliberately filters CAT_FNCT only and its behavior
   is pinned by `test_xeq_item_lookup`/`test_xeq_precedence` (R4-B3: one
   helper must not serve both contracts). `forthFindItem` is the FORWARD
   lookup only."
2. Fix the identifier: `ITM_sin` (items.h:88), not `ITM_SIN`; keep the name
   string "SIN" for the lookup argument (`itemCatalogName` scan).
3. Correct "Tests that encode the old contract: none" → name
   `test_xeq_item_lookup`/`test_xeq_precedence` as *must-stay-green unchanged*
   (they encode the reverse contract this task must not alter).
4. Replace the PREAMBLE's audit_probe/foreign-work block (obsolete) with the
   standard R4-file preamble; drop file(s) list entry for forth_dict.c's
   resolver; keep forth_dict.h/.c only for declaring/implementing
   `forthFindItem` (new function, no resolver edit).
5. Replace the stale R1-4 commit block: R1-3 commits alone ("forth-core: R1-3 —
   outer interpreter resolves C47 items (§4.1 step 4)"), staging only its
   flat files + generated counterparts + manifest.
6. Add the standing mutation duties (bypass item arm; drop PTP_NONE filter —
   the second mutation's expected RED is the new focused `forthFindItem("STO")
   == false` test, not the reverse-lookup tests).
7. Add a non-goal: "No FTOK_XEQN, no label-arm change, no resolveXEQ change,
   no parameterised items" (mirrors the R4 fence).

**10.2 R1-4:** do not execute; prepend a STATUS banner: "WITHDRAWN 2026-07-15
— superseded by the R4 accepted architecture (B1 lifetime, B4 dispatch matrix,
sequencing). FTOK_XEQN lands in the future series stage 3, after lifetime
foundations." Same-file note that R1-1/R1-2 were committed separately.

**10.3 New small prompts worth authoring with the same template (optional,
non-blocking):** (a) softmenus MNU_FORTH builder: replace the last-qualifying
owning-program loop with `forthOwningProgramStart(currentStep)`
(AUD-M3; mutation: revert to iteration-order loop, new test with a shuffled
programList fixture — or reuse `test_owning_program_start_max_not_last`'s
approach at the builder level); (b) the R4-E3 precondition comment +
`forthOuterRun` debug assert (`assert(!isDefinitionOpen())` under
FORTH_DEBUG_SELFTEST at nested entry), per the accepted ruling's "document the
internal precondition"; (c) bench/hwm.fs or the §5.4 amendment (AUD-M4).

**10.4 design-docs/package-manager/QWEN_IMPLEMENTATION_PROMPTS.md:** add the ARCHIVED
banner (AUD-M5). One paragraph, no other edits.

---

## 11. Questions requiring an architect/user decision

1. **Q1 (AUD-M1):** §8.9 — implement the end-to-end acceptance paths, or
   downgrade the section's wording per item? If implement: the reachable
   harness for XEQ/runProgram-driven flows is the prerequisite work R2
   identified; order it before or after the R4 lifetime redesign? (After is
   cheaper: the lifetime redesign changes §8.3 behavior these tests would pin.)
2. **Q2 (AUD-M9):** when restore-time full validation lands (R4-E4), does
   `boundedRead` stay as defense-in-depth (flash cost) or go?
3. **Q3 (AUD-M4):** bench/hwm.fs — build it, or amend §5.4 to bless the
   suite's `FORTH ARENA:` line as the reporting mechanism?
4. **Q4 (AUD-M11):** interim statement for reverse-lookup item dispatch of
   parameterized items with NOPARAM (today's pinned behavior) until B3's
   "atomic syntax error" arrives — accept-as-documented or hotfix the filter
   now (a contract change with named test migrations: `test_xeq_item_lookup`
   rows for FORTH/FCALL)?
5. **Q5 (R3-A1):** is the bare-render listing's non-injectivity (a Forth line
   `SIN` indistinguishable from the RPN step `SIN` when markers are
   off-screen) accepted-by-design? One sentence in §8.5 settles it.
6. **Q6 (R3-A2):** upstream b8f79e486 added the `programBytesAvailable()`
   bounds guard in `findKey2ndParam` — does that close the malformed-opcode
   lead, or do you still want the decode.c-side `indexOfItems[op]` bound and a
   malformed-save policy? (Restore/import reachability remains unverified.)
7. **Q7 (AUD-L7):** PEM_FIX deferrals F8 (reject-path cursor drift) and F9.1
   (phantom marker on power-off) — close with evidence or carry into current
   tracking?
8. **Q8 (AUD-U1c, addendum):** label-kind policy for the vocabulary series now
   that upstream has position-sensitive **named local labels**: does Forth's
   future `XEQ 'name'` source form (and FTOK_XEQN's runtime resolution, and
   bare-name step 5) request GLOBAL_LABELS only (the current pin, symmetric
   interactive/program semantics) or the native ALL_LABELS meaning
   (RPN-parity, but position-sensitive resolution is ill-defined from an
   interactive Forth line with no current-step context)? Recommend: pin
   GLOBAL_LABELS for all Forth-side resolution, state it normatively in §4.1/
   §4.2, and revisit only with the scoped-vocabulary work.
9. **Q9 (AUD-U6, addendum):** upstream consumed a large slice of the DM42
   dist-package flash headroom (PACKAGE1 free 4984→2560, PACKAGE3
   12192→8304). The R4 rulings that "accept a modest flash increase"
   (restore-time validator, shared param-decoder factoring, capture submode)
   were priced against the old headroom. Re-baseline forth-core's measured
   flash delta on the dmcp package builds before authoring the future series,
   and set a per-stage flash budget line?

---

## 12. Recommended canonical implementation sequence

1. **Architect pass on DESIGN.md** (one commit, no code): fold R4 accepted
   architecture; fix AUD-H1/H2/H3; AUD-M6/M2 wording; AUD-M8 citation refresh;
   LOW batch (L1-L6, L9); rule §11 questions; record R2-finding-1 ruling.
   DESIGN-HISTORY entry for the fold.
2. **Prompt hygiene** (one commit, docs only): §10.1 R1-3 rewrite; §10.2 R1-4
   withdrawal banner; §10.4 ARCHIVED banner.
3. **Execute corrected R1-3** (Qwen, green gate + mutations + arena line).
4. **Author + execute the future series in the R4-mandated order**, one stage
   at a time, each with traced native behavior and executed RED mutations:
   (i) lifetime foundations (pending-reset flag, active-frame guard, PEM
   single-step, dynamic scan tracking, RECURSE, restore-time validator —
   resolves Q2), (ii) shared RPN parameter semantic core, (iii) vocabulary/XEQ
   incl. FTOK_XEQN + `XEQ 'name'` source form (the reborn R1-4), (iv) series C
   textual params, (v) series D commit validation (E9 successor), (vi) capture
   submode.
5. **§8.9 acceptance harness** per Q1's ruling (recommended: after stage 4-i).
6. Optional §10.3 small prompts at any green point.

---

## 13. Final pre-execution checklist

- [ ] AUD-B3: R4 accepted architecture folded into DESIGN.md (or explicitly
      bannered per section) — no dual authority remains.
- [ ] AUD-H1: E7/P-H2 cursor-hack text rewritten to match landed R3-1.
- [ ] AUD-H2: stale "required change / does not exist" claims rewritten as
      implemented invariants.
- [ ] AUD-H3: §3.3 error-table label row reconciled with §3.3.6.
- [ ] AUD-B2: R1-3 rewritten per §10.1 (resolver untouched; ITM_sin; test
      claims corrected; commit block replaced).
- [ ] AUD-B1: R1-4 withdrawn/bannered; FTOK_XEQN deferred to the ordered series.
- [ ] AUD-M5: rev-1 package prompt file bannered ARCHIVED.
- [ ] AUD-M1/Q1: §8.9 ruling recorded.
- [ ] Baseline gate re-run green immediately before the first Qwen session
      (`./packages/forth-core/build-test.sh`, both banners, exit 0) and the
      arena line captured as the session baseline (last known: here=36
      sizeBlocks=16 freeRamDelta=64).
- [ ] Qwen sessions launched one at a time, clean tree between tasks, per-task
      RED mutation evidence required in each report.
- [ ] Addendum R6.1: AUD-U1 tam.colon gate fixed (or explicitly ruled
      acceptable); Q8 label-kind policy ruled; Q9 flash re-baseline decided;
      the U-series doc corrections folded into the same DESIGN.md amendment
      pass as AUD-B3/H1/H2/H3.

---

## Addendum R6.1 — upstream-delta audit (79ce0898f → b8f79e486, 296 commits)

Added 2026-07-15 after the main report. The main report audited the migration
only mechanically (anchor drift, the two hand-resolved conflicts, the two
upstream changes the migration commits named). This addendum is the feature
sweep of the full range, asking: does anything new upstream change our plans?

**Verdict: the NO-GO blockers are unchanged; the sweep adds one MEDIUM code
finding (AUD-U1), two architect questions (Q8, Q9), and a batch of doc/plan
corrections that fold into the already-required DESIGN.md amendment pass.**

### AUD-U1 (MEDIUM, code) — the tam.c Forth fallback is not gated on `tam.colon`

Upstream's named-local-label feature added a TAM colon syntax: pressing `:` in
the label TAM menus sets `tam.colon` (typeDefinitions.h `tam` struct gained
`bool_t colon`), the buffer resolves via
`findNamedLabelWithDuplicate(buffer, dupNum, tam.colon ? LOCAL_LABELS :
GLOBAL_LABELS)` (package ui/tam.c:937), and committed steps encode
`LOCAL_LABEL_VARIABLE`. Local resolution is **position-sensitive**: within the
current program only, preferring the first occurrence *after*
`currentLocalStepNumber`, else the first in the program
(upstream manage.c, `findNamedLabelWithDuplicate`).

Our interactive hook (package ui/tam.c:978-991) fires on any label miss for
`ITM_XEQ/ITM_XEQP1` with only a `!tam.indirect` gate. So `XEQ :FOO` — an
explicitly *local* request — that misses local labels falls through to
`forthFindColon` and either dispatches the Forth word FOO interactively or, in
PEM, records a **global**-name XEQ step via `insertUserItemInProgram` (which
writes `STRING_LABEL_VARIABLE`). Both outcomes violate the principle the
migration itself established for the program path ("a step asking for a LOCAL
label must fail as not found, never fall through to Forth vocabulary" —
package lblGtoXeq.c rebase comment), which the `_executeOp` hook honors
(`forthFallbackEligible = (opParam == GLOBAL_LABELS) && …`) but the TAM hook
predates. Note upstream's *own* item-name scan in the same chain
(ui/tam.c:964-976) has the identical gap (`XEQ :SIN` miss → runs the item) —
that half is upstream's to fix; optionally report it.

*Fix (bounded, Qwen-able):* add `&& !tam.colon` to the hook's guard (or gate
both fallbacks by hoisting `if(tam.colon)` to the miss-error path), plus a
regression test driving `_tamProcessInput` with `tam.colon = true`, a Forth
word defined, and no matching local label — expect `ERROR_FUNCTION_NOT_FOUND`
(or upstream's label-not-found surface), not FCALL dispatch, and in PEM no
step recorded. Mutation: remove the gate → the test observes the Forth word
run / the global-name step appear. Non-blocking for R1-3, but it is a live
shipped defect introduced by the feature/hook interaction — schedule with the
§10.3 batch.

### AUD-U2 (plan input) — named local labels change the B-series contract surface

The accepted B2 ruling ("`XEQ 'name'` … first requests the native RPN label
meaning") now points at a *two-kind, position-sensitive* label namespace. The
migration pinned every Forth-side lookup to GLOBAL_LABELS (bare-name step 5,
`forthResolveXEQ`, both keyed to the R4 "established global visibility"
language) — correct and conservative, but recorded only in code comments and
the merge commit. Decision Q8 must land in DESIGN.md §4.1/§4.2 **before** the
vocabulary-series prompts are authored; FTOK_XEQN's inline-name resolution
must name its labelType explicitly. Also fold: §0.3's labelList description
(local entries store `step = -stepNumber`, `labelPointer = step+2`), and the
`:name:` listing render for LOCAL_LABEL_VARIABLE steps (upstream decode.c) —
Forth's bare source render is unaffected.

### AUD-U3 (doc) — upstream fixed the REM/catalog bug themselves

`fix-REM-in-PEM-from-FCNS-menu` (the tip merge, b8f79e486) adds a two-pop
teardown to the shared REM/LITERAL capture-open arm (drop FCNS, then CAT if on
top). Weaker than our stack-wide `_forthCatalogBuriedOnStack` drain but covers
the field case. Consequences: `UPSTREAM_BUG_REM_alpha_menu.md` is fully closed
(upstream confirmed + fixed); DESIGN.md §8.4 E1's rationale "The REM arm has
the same latent flaw and has never shown it… Do not 'fix' REM by symmetry" is
**stale** — rewrite to: upstream fixed REM with a shallower teardown; ours
remains the stack-wide drain; do not unify them. No behavioral interaction
(both landed, gate green, catalog tests drive the real chain).

### AUD-U4 (doc) — upstream adopted our `dynamicMenuItem = -1` default

`fix/dynamic-menu-item-default` (in-range) is our fix upstreamed. The
`dynamicMenuItem = -1` clear in forth_compile.c's label arm is now
defense-in-depth rather than the sole guard; keep it, and its comment may note
upstream adoption.

### AUD-U5 (surface note) — Forth-visible item-name changes

Newly `CAT_FNCT|PTP_NONE` (auto-enter the §4.1 step-4/reverse surfaces; no
action required — the filter is the contract): `X.SWAP`, `X.EDIT`, `cpxSlv`
(all PTP_DISABLED→PTP_NONE, i.e. upstream made them programmable), and
`PLTFCNS` (CAT_MENU→CAT_FNCT — a pseudo-menu item that now passes the filter
and opens a menu; a good second example beside `OFF` for the §4.1 guardrail
paragraph). Leaving the surface: `>RCLVEL<`, `>STOVEL<`, `>CNCAT<`
(CAT_FNCT→CAT_NONE). Renamed: `DMXX`→`DMX`. New CONV items 2860-2863 are
conversion-class, not CAT_FNCT — invisible to Forth. No current test or prompt
references any affected name (verified: R1-3's SIN/STO examples unaffected).

### AUD-U6 (plan input) — flash headroom shrank on the DM42 dist packages

defines.h bookkeeping: PACKAGE1 free 4984→2560, PACKAGE3 12192→8304 (PACKAGE2
≈flat). CLAUDE.md makes flash/RAM the binding constraint, and three R4
rulings explicitly accepted flash costs (restore validator, decoder factoring,
capture submode) priced against the old headroom → Q9: re-baseline
forth-core's measured flash delta on the dmcp builds before the future series
and give each stage a flash budget line alongside the arena line.

### AUD-U7..U10 (notes, no action beyond docs)

- **U7** `saveRestoreCalcState.c` gained d47 data-file versioning
  (configFileVersion 10000026, version-gated Conf descriptor). Device save
  format only; the dictionary is simulator-backup-only and run-scoped — no
  Forth impact.
- **U8** `MNU_TAMLOCALLABEL` is a *static* menu appended at softmenu[] slot
  185 — no collision with our dynamic slot 22 / NUMBER_OF_DYNAMIC_SOFTMENUS
  23. `tam.colon` is only live in TAM modes (`tam.mode != 0`), Forth capture
  is non-TAM alpha capture — disjoint from the E-series guards; the future R4
  capture submode must include `tam.colon` in the tam state it
  suspends/restores.
- **U9** Upstream's "Fix-memory-issue---double-free" was a one-line *caller*
  fix (config.c: DELall freed the saved statistical sums block) — the
  allocator is untouched; our H10 guard and its pending upstream MR are
  unaffected and the MR case is strengthened (this is the second caller-side
  double-free upstream fixed this cycle; the guard converts that class into a
  loud non-corrupting reject).
- **U10** Upstream restated "add new MENUS at the end" while adding slot 185;
  our `-MNU_FORTH` sits mid-table at slot 022 (end of the *dynamic area*),
  shifting every static softmenu index ≥022 — deliberate (dynamic-area
  contiguity), benign at runtime, but it desyncs upstream's Wiki numbering;
  document the deviation in DESIGN §8.6 (merge with AUD-L5). Also upstream's
  walker hardening (`programBytesAvailable` in `findKey2ndParam`, including
  the *computed-end* check over `countLiteralBytes`/PTP_REM results) closes
  the walker half of R3-A2/Q6 — the renderer-side `indexOfItems[op]` indexing
  in decode.c remains upstream-unguarded but is now harder to reach; Q6's
  framing updated accordingly.

---

FINAL VERDICT: NO-GO — RESOLVE THE LISTED BLOCKERS BEFORE EXECUTION
