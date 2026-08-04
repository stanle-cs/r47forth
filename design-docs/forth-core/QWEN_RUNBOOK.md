# QWEN_RUNBOOK — what to run, in what order (2026-07-18)

One page for the operator. Answers exactly three questions: what is already
done, what to run next, and when work must come back to the architect
instead of Qwen. Authority for content stays with the packet files and
DESIGN.md; this file only sequences them.

## 0. Already executed — never run these again

Every pre-F prompt set is complete (verified in
`FOR_THE_ARCHITECT_R6_preexecution_audit.md` §table and
`R6_RESOLUTION_PLAN.md` §0):

| Prompt set | Disposition |
|---|---|
| `STAGE2_QWEN_PROMPTS.md` (Q1-Q15) | executed |
| `QWEN_PROMPTS_pem-entry.md` (A1-A9) | executed (`dcc8d6594`) |
| `QWEN_PROMPTS_R1_code_defects.md` | R1-1/R1-2 executed; R1-3 executed (`ae1a5a1af`); R1-4 **WITHDRAWN** (superseded by F3) |
| `QWEN_PROMPTS_R2_tests.md` (T1-T7) | executed |
| `QWEN_PROMPTS_R3_pem.md` | executed (`453205de0`) |
| `QWEN_PROMPTS_R4_engine.md` | executed (`bf29ad55f`, `b6a2a74dd`, `1806d48d8`, `390d7ba15`) |
| `design-docs/package-manager/QWEN_PROMPTS_R5_tooling.md` | executed (`e579d1821` + R5-7) |
| `QWEN_PROMPTS_R6_followups.md` (R6-1..R6-5) | R6-1..3 committed (`29f94d8e4`, `416c1e26b`, `4ab421a32`); R6-4/R6-5 complete (report-only, owner-confirmed 2026-07-16) |
| `design-docs/package-manager/QWEN_PROMPTS_refresh-base-commit.md` (BP-1..7) | executed |
| `design-docs/package-manager/QWEN_IMPLEMENTATION_PROMPTS.md` | ARCHIVED — execution hazard, never run |

The F series (F1.5 through F6) is fully landed as of F6-6 (`d9b1e894b`,
2026-07-20) — no Qwen packets remain queued. Both owner-directed audits
landed (rows 11j-11k). Remaining open work is row 11i, converted by owner
ruling 2026-08-02 from a hardware bench to an automated sim-run bench —
plan and packet decomposition in `TESTING.md` §5 (stages T1-T4); after
that the accepted backlog is empty.

## 1. Per-session procedure (identical for every packet)

1. Clean tree on `forth-core/pem-entry-fixes`; `git status --short` empty.
2. Run the packet's **EXECUTION GATE** greps yourself (top of each packet).
   Any mismatch → stop, bring it to the architect; do not hand to Qwen.
3. Paste the ENTIRE packet file to Qwen as the prompt (preamble included).
   One packet per session; do not batch.
4. If Qwen prints `[SOL DEBUGGER HANDOFF]`, stop the session and bring the
   handoff report to the architect. Do not let Qwen continue past it.
5. When Qwen reports done: verify yourself —
   `git status --short` clean, `git log --oneline -2` shows the packet's
   exact commit message, and the gate is green
   (`./packages/forth-core/build-test.sh > /tmp/gate.log 2>&1; echo $?;
   tail -n 12 /tmp/gate.log`). Skim Qwen's report for the required PASS
   lines, mutation REDs, and the arena line.
6. Optional per stage commit (RULE-1): record the `make dmcp5r47` flash
   delta.
7. Before the next session: run the NEXT packet's EXECUTION GATE greps.
   Mismatch → architect re-authors the packet; match → go to step 1.

## 2. The queue

Legend: **[QWEN]** = paste the named file and run it now.
**[GATE LOCKED]** = the packet is authored, but its predecessor must commit
green before its execution gate can open. **[ARCHITECT]** = come back to the
architect session first; the future-series packet has not been authored yet.

| # | Step | Who | Input |
|---|---|---|---|
| 1 | F1-1 pending-reset truth + active-frame guard | DONE (`1834901d3`) | `QWEN_PROMPTS_F1_1_pending_reset.md` |
| 2 | F1-2 runProgram = sole top-level lifetime signal | DONE (`542972b32`) | `QWEN_PROMPTS_F1_2_run_signaling.md` |
| 3 | F1-3 dynamic arena-backed scan tracking | DONE (`ecbd6bcce`) | `QWEN_PROMPTS_F1_3_scan_tracking.md` |
| 4 | F1-4 compile-only RECURSE | DONE (`2940a0f4f`) | `QWEN_PROMPTS_F1_4_recurse.md` |
| 5 | F1-5 restore-time threaded-code validator | DONE (`04006089f`) | `QWEN_PROMPTS_F1_5_restore_validator.md` |
| 6a | F1.5 stage opened: §8.3/§8.9 reconciled to landed F1 (`6345f6c64`); stage ledger is `QWEN_PROMPTS_F15_harness.md` | DONE | — |
| 6b | F15-1 end-to-end run lifecycle (§8.9 items 1, 7, 9) | DONE (`b773597bd`) | `QWEN_PROMPTS_F15_1_run_lifecycle.md` |
| 6c | F15-2 derived entry state + power-off round-trip (§8.9 item 2) | DONE (`5a9e9ce2d`) | `QWEN_PROMPTS_F15_2_entry_state.md` |
| 6d | F15-3 display parity across PEM/BST/SST surfaces (§8.9 item 4) | DONE (`c8b87dfa8`) | `QWEN_PROMPTS_F15_3_display_parity.md` |
| 6e | F15-4 glyph operators + literal type parity (§8.9 items 5, 6) | DONE (`6775252bf`, debugged) | `QWEN_PROMPTS_F15_4_glyph_type_parity.md` |
| 6f | F15-5 PEM XEQ-name step (§8.9 item 10) | DONE (`546aa8b6c`) | `QWEN_PROMPTS_F15_5_xeq_name_step.md` |
| 6g | F1.5 stage close: §8.9 coverage flipped COMPLETE, item-10 mutation reconciled, `vBodyWalk` nit fixed, ledger closed | DONE | — |
| 7a | F2 trace + ledger (`QWEN_PROMPTS_F2_core.md`) — PTP paths traced 2026-07-17, architecture decided | DONE (authored) | — |
| 7b | F2-1 extract the native parameter core | DONE (`6f0ffca4b`, recovered) | `QWEN_PROMPTS_F2_1_extraction.md` |
| 7c | F2-2 bounded name reader | DONE (`69e594c71`) | `QWEN_PROMPTS_F2_2_bounded_names.md` |
| 7d | F2-3 shared direct dispatch + FTOK_C47 re-route | DONE (`06ce84b5a`) | `QWEN_PROMPTS_F2_3_shared_dispatch.md` |
| 7e | F2-4 parity acceptance sweep (+ RULE-1 flash delta) | LANDED (`176e0be0f`) — post-stage review found acceptance escapes | `QWEN_PROMPTS_F2_4_parity_acceptance.md` |
| 7f | F2-5 bounded-reader + parity-acceptance correction | DONE (`b5d794df4`) | `QWEN_PROMPTS_F2_5_acceptance_correction.md` |
| 8a | F3 trace, design pass, and packet ledger | DONE (authored 2026-07-18) | `QWEN_PROMPTS_F3_core.md` |
| 8b | F3-1 owner-tagged dictionary headers | **LANDED** `31e4acbde` | `QWEN_PROMPTS_F3_1_owner_headers.md` |
| 8c | F3-2 global region, refs, persistence swap, validator retarget | **LANDED** `e8f1f16cd` (amendment F3-3/A `56f554673`) | `QWEN_PROMPTS_F3_2_global_region.md` |
| 8d | F3-3 current scopes and filtered lookup | **LANDED** `8af819797` | `QWEN_PROMPTS_F3_3_scopes_live.md` |
| 8e | F3-4 GLOBAL / IMMEDIATE / FORGET | **LANDED** `25ce96c25` | `QWEN_PROMPTS_F3_4_marks.md` |
| 8f | F3-5 compile-time control flow | **LANDED** `faa8c32a3` | `QWEN_PROMPTS_F3_5_control_flow.md` |
| 8g | F3-6 XEQ forms, FTOK_XEQN, and B3 | **LANDED** `2db8af231` | `QWEN_PROMPTS_F3_6_xeqn.md` |
| 8h | F3-7 §2.3 acceptance pins and final stage sweep | **LANDED** `992f7e817` | `QWEN_PROMPTS_F3_7_acceptance.md` |
| 9a | F4 trace + ledger (grammar/error table traced 2026-07-18) | DONE (authored) | `QWEN_PROMPTS_F4_core.md` |
| 9b | F4-1 flow classification + direct numeric params | **LANDED** `f043c63e7` | `QWEN_PROMPTS_F4_1_direct_numeric.md` |
| 9c | F4-2 register/flag/shuffle direct forms | **LANDED** `ac48f50a8` (sol debug; amendment F4-2A) | `QWEN_PROMPTS_F4_2_register_flag.md` |
| 9d | F4-3 named, system-flag, indirect forms | **LANDED** `fc0fabdad` (sol rewrite+debug; amendment F4-3A) | `QWEN_PROMPTS_F4_3_named_indirect.md` |
| 9e | F4-4 error-table + parity acceptance (stage close) | **LANDED** `81d73cd6a` | `QWEN_PROMPTS_F4_4_acceptance.md` |
| 10a | F5 ledger: commit seam + tier ruling (2026-07-18) | DONE (authored) | `QWEN_PROMPTS_F5_core.md` |
| 10b | F5-1 check mode + soundness battery | **LANDED** `ba304a3cf` (state-neutrality pin added by F5-2A) | `QWEN_PROMPTS_F5_1_check_mode.md` |
| 10c | F5-2 commit gate (stage close) | **LANDED** (sol debug; amendment F5-2A) | `QWEN_PROMPTS_F5_2_commit_gate.md` |
| 11a | F6 audit — traces T1-T7 FOLDED (2026-07-19); bench Blocks A-F DEFERRED to stage-exit (owner ruling 2026-07-18) | DONE (traces) — **stage-exit bench is [S]+[A]**, after F6-6 | `F6_KEYBOARD_PEM_AUDIT.md` + `F6_AUDIT_RESULTS.md` |
| 11b | F6 ledger: decomposition + design decisions | DONE (authored 2026-07-19) | `QWEN_PROMPTS_F6_core.md` |
| 11c | F6-1 managed capture buffer | **LANDED** `7862b896b` — Qwen stalled (3 gate failures); architect finished it | `QWEN_PROMPTS_F6_1_capture_buffer.md` |
| 11d | F6-2 TAM suspend/resume | **LANDED** `a0b3fe7f8` (architect) | `QWEN_PROMPTS_F6_2_tam_suspend.md` |
| 11e | F6-3 catalogs + menus during capture | **LANDED** `d1b6ac674` (architect; mutation-2 packet defect fixed, DESIGN-HISTORY 2026-07-19) | `QWEN_PROMPTS_F6_3_capture_menus.md` |
| 11f | F6-4 parameter entry emits canonical text | **LANDED** `4ca4bfde4` (architect) | `QWEN_PROMPTS_F6_4_param_text.md` |
| 11g | F6-5 dictionary-backed word catalog | **LANDED** `f7375ef37` (architect; clean first-gate run) | `QWEN_PROMPTS_F6_5_word_catalog.md` |
| 11h | F6-6 acceptance battery (stage close) | **LANDED** `d9b1e894b` (architect; 2 pre-existing save/restore-vs-allocator gaps found, logged for the code audit, not fixed here) | `QWEN_PROMPTS_F6_6_acceptance.md` |
| 11i | F6 stage-exit bench, Blocks A-F — automated sim bench (owner ruling 2026-08-02) | **DONE** — SB-1 `9aa57e93e`, SB-2 `833513bb4`, both implemented by the local model under architect supervision (2026-08-03); 13 bench PASS lines, 6 mutations RED, arena at baseline, flash −8 B net. B2/C2 rulings + findings in DESIGN-HISTORY | `F6_KEYBOARD_PEM_AUDIT.md` §6 |
| 11j | Test-suite audit (toothless/bad-design test sweep) | DONE `cbd285e09` (14 rigor fixes; 1 pre-existing production bug logged) | — |
| 11k | forth-core code audit (production code) | DONE — code audits #1-#4 (`88a2b5f85`, `976b864b5`, `434b79612`, `0e959574a`), S1-S3 cleanup series, design audit #1 (`8f3c3db7c`) | — |

## 2b. Stage D3 — hybrid spill stack (owner ruling 2026-08-03; design DESIGN.md §11)

| # | Step | Who | Input |
|---|---|---|---|
| D3-1 | spill region + accessors | **DONE** `30d29d7e8` | `QWEN_PROMPTS_D3_1_spill_region.md` |
| D3-2 | spill live via forthPrimInvoke; 7 FACT = 5040 | **DONE** `540977271` (+ amendment D3-2A) | `QWEN_PROMPTS_D3_2_spill_wiring.md` |
| D3-3 | boundary rule, named message, both-sides pins | **DONE** `3c44cc58d` | `QWEN_PROMPTS_D3_3_boundary_rule.md` |
| D3-4 | window-parity acceptance (WP-1/WP-2) | **DONE** (2026-08-03; docs fold queued with the §10 fold) | `QWEN_PROMPTS_D3_4_acceptance.md` |

## 2c. Stage G — GUI coverage gaps (post-migration review 2026-08-03)

The upstream migration to `26ec91634` prompted a coverage review of the
package's GUI surface. Content, capture lifecycle, TAM suspend/resume
and the EXIT ladder are pinned to standard; everything downstream of the
picker's content array is not. Two packets close it. Findings and
reasoning: DESIGN-HISTORY 2026-08-03 (the migration entry's successor).

| # | Step | Who | Input |
|---|---|---|---|
| G1 | softkey→word mapping: index ≥ 1, f/g shift rows, `firstItem` paging, the blank-key refusal, draw path per page | **DONE** `78e32af30` (architect-implemented; 5 mutations RED) | `QWEN_PROMPTS_G1_picker_key_mapping.md` |
| G2 | scan cut-off named + pinned; content allocation guarded | **DONE** `2f378c911` (architect-implemented; 3 mutations RED after two fixture rewrites) | `QWEN_PROMPTS_G2_picker_scan_alloc.md` |
| G3 | the picker's label reaches the LCD (read-back) | **DONE** `00c5cf2d3` (architect-implemented; 2 mutations RED) | — (no packet; see the commit) |
| G4 | paging changes the picture; nothing drawn past `numItems`; a maximal name stays in its cell | **DONE** — implemented by the local model (`66f4bfa32`), corrected `fac79a397`, Part B `98e343810` | `QWEN_PROMPTS_G4_picker_pixels.md` |

Both were implemented in the architect session rather than handed to the
local model (FIX-6B precedent). The packets stay on disk as the authored
record; anyone re-running them should read the commit messages first,
since G2's subcase 1 fixture is NOT the one the packet specifies — see
below.

**What the mutation runs changed (binding for anyone authoring a
cut-off pin).** G2's packet specified a fixture of one definition per
step, sized from `FORTH_PICKER_MAX_SCAN_STEPS`. Both properties were
wrong and both stayed green under mutation:

1. Sizing the fixture from the constant makes it immune to a change in
   the constant, and therefore blind to one. A constant that changes
   calculator behaviour is pinned as a literal, beside the mechanism.
2. One definition per step means the picker's 170-name cap
   (`TMP_STR_LENGTH/15`) bites at step 170 and the step cut-off never
   acts. The fixture was measuring the name cap. A cut-off pin needs
   steps the other limit cannot reach — here, two definitions separated
   by 1000 non-defining filler steps.

Neither was visible from the code or from a green gate. Both came out of
running the mutations, which is the argument for running them.

**G3 (`00c5cf2d3`) — the residual above was wrong and is now closed.**
Stage G was authored recording pixel-level rendering as out of reach
without "an LCD read-back harness" that was said not to exist. It does
exist and always did: `lcd_buffer_pixel_on()` is declared in
`src/c47/hal/lcd.h` for every non-DMCP build and implemented in both the
c47-gtk and testSuite HALs, and the software blitter fills `lcd_buffer`
whether or not a window exists — `headlessMode` only gates
`gtk_widget_queue_draw_area`. G3 reads the framebuffer back and pins the
picker's label at the pixel: 414 px for a maximal 14-byte name > 150 px
for a 2-byte name > 33 px for an empty picker.

The one real constraint, for anyone extending this: `lcd_clear_buf()` is
c47-gtk-only and absent from the testSuite HAL, and `test_dict_reloc.c`
compiles into BOTH binaries — so a pixel test may not clear the buffer.
G3 renders in decreasing label length and asserts a strict decrease
instead, which fails rather than flatters if a cell ever stops
repainting.

**G4 — the first packet in this stage actually run through the local
model, and the round trip is the lesson.** Headless `opencode run`
auto-rejects every permission set to `ask`, so the first attempt read the
skill, hit the execution gate and stalled having changed nothing while
exiting 0 (recorded in the qwen-implementer memory). Re-run with
`--auto`, the model stayed inside the two files the packet allowed,
touched no production file and no `src/`, and committed.

Three architect spec defects against two implementer process violations:

- **Mine.** The packet said to assert `numItems` "before any act", but
  `numItems` is 0 until a render calls the builder — the model followed
  it literally, read 0 three times and began misdiagnosing the donor
  fixture. The packet said a page is 6 items when the renderer draws
  three rows of 18. And its geometry note divided `SCREEN_WIDTH` by six
  where the real borders are `KEY_X`.
- **Its.** Both violations were correct observations reported the wrong
  way: an inverted assertion in subcase 1 (which the 18-item page made
  empirically right), and two silent workarounds for the divider column
  in subcases 2 and 3 — one exclusion, one tolerance — with a PASS line
  left claiming an assertion that was no longer made.

The standing consequence: **a packet specifies exact PASS text, and the
verifier reads the PASS text against the packet.** Every one of these
landed a GREEN gate. Nothing but the strings caught them.

**Nothing residual after G1-G4 — the last item on that list was traced
and is not ours.** Stage G carried "the combined-key `trimSoftKeyName`
path" as an open gap through three closeouts. It is not a gap: that path
is unreachable from anything forth-core draws. `showSoftkey2`/`showKey2`,
the only callers of the `trimSoftKeyName` at softmenus.c:2077-2078, sit
inside `if(convUserMenu)`, and `convUserMenu` is set true only in
`case MNU_MyMenu` and the user-menu case (softmenus.c:3251, 3270).
`MNU_FORTH` is neither, so the picker always takes the
`else // fall through for non-user menus` branch: plain `showSoftkey` ->
`showKey` -> `trimKey`. That is the clamp G4 subcase 3 pins by raising
`lim` from 66 to 120.

It was listed as residual because it was named from reading a grep of
call sites, never traced to a caller — the same mistake as declaring the
read-back harness absent. A gap is only a gap once you have shown
something of ours reaches it.

## 3. After the series — no Qwen work

Owners as in `R6_RESOLUTION_PLAN.md` §3: **A** = architect session (Claude),
**S** = Stan.

- Step 8 housekeeping: freeList guard upstream MR — **upstream ruled
  2026-07-19** (UPSTREAM_REPORTS §3): halt over continue. The guard is
  reworked to raise `displayBugScreen` (fail-loud) rather than
  silently-refuse-and-continue; detection scan unchanged. Reworked as
  `QWEN_PROMPTS_FIX6_bugscreen.md` (FIX-6B) — the SOLE packet allowed to
  touch `freeList.c` (it lifts the no-touch rule for that one hunk).
  **EXECUTED 2026-08-02 (`5c2e7109a`, architect; two gate amendments and
  the +80 B flash correction recorded in DESIGN-HISTORY).** Remaining:
  **S** forks/pushes/opens the MR with the fail-loud patch; the reply
  (in UPSTREAM_REPORTS §3) flags one open call-context question (immediate
  vs. latched raise) for upstream to finalize. Optional upstream reports
  (tam.c item-scan colon gap; decode.c renderer indexing) — **MOOT
  2026-08-03**: both were fixed upstream inside the migration window
  (`05508a7a7`, `6e26d2c09`), never filed by us; re-verification recorded
  in `UPSTREAM_REPORTS_b8f79e486.md`. The restoreCalc leak draft
  (`UPSTREAM_REPORTS_976b864b5.md`) was re-verified the same day:
  mechanism intact at the new base, but our reproducer is gone (S3 moved
  capture onto aimBuffer) — filing is **S**'s call, draft carries the
  framing notes.
- Final docs reconciliation pass (DESIGN.md stale/interim prose,
  DESIGN-HISTORY, ledger closeout) — **A**, docs-only commits.
- Flash baseline/deltas (`make dmcp5r47`, RULE-1) — **A** runs and records
  (owner ruling 2026-08-02; was S).
- Then the accepted implementation backlog is **empty**; anything further
  starts with a new owner ruling (**S**).

## 4a. Packet authoring for the local implementer (BINDING, from the 2026-08-03 sim-bench trial)

The SB-1/SB-2 trial (DESIGN-HISTORY 2026-08-03) measured exactly where the
27B succeeds and fails. Every future packet is authored against these
rules; a packet that violates one is a packet defect.

**Split every packet into two parts with different shapes.**

- **Part A — implementation** (the model is genuinely good at this): the
  subcases/spec, sized to **at most 5-7 subcases per session** (SB-2's
  session died mid-body on a larger scope; SB-1's 7 fit). Delegating
  reads ("grep X, copy its idiom") is fine — the tool-using implementer
  handles it — but see the exactness rules below for what may NOT be
  delegated.
- **Part B — mechanical choreography** (the model CANNOT self-sequence
  this): mutations, restore chains, commits. Authored as EXACT command
  blocks — apply sed, verify grep, gate to a named log, required RED
  line, revert sed, green gate — and run **one block per FRESH session**,
  nothing else in the prompt. The architect derives Part B's line numbers
  AFTER Part A lands (they shift), never at packet-authoring time.

**Exactness rules — what the model will otherwise guess wrong:**

1. **Byte-layout facts are stated or grep-directed, never derivable.**
   Length-byte conventions, marker signatures, payload offsets: either
   write the literal in the packet or direct a grep at a NAMED existing
   assert. Left open, the model invents constants (the 0x85B2/ITM_END
   incident) or conventions (len 6 vs 5).
2. **Drive paths name the exact call, not the gesture.** "Press the TAM
   colon" is under-specified — the model picks the wrong dispatcher
   (runFunction(ITM_COLON) reaches nothing; tamProcessInput(ITM_COLON)
   is the machine; ITM_DOT is not ITM_PERIOD). Every key drive names
   function AND item, or names the landed test whose drive slice to copy
   verbatim.
3. **Every fixture asserts its armed state before the act** (picker
   current, flag set, colon armed) — and forcing the state to satisfy the
   assert is forbidden and named as such (the setSystemFlag hack): if the
   precondition fails, the fixture is wrong; STOP.
4. **Expected mutation REDs quote the exact FAIL line text.** A mutation
   that stays green is a finding about the TEST (it caught C2's
   form-blind assert) — the session STOPs and reports it, never
   "fixes" the mutation.
5. **Edit anchors are unique strings** (subcase printf text), never
   structural boilerplate — 41% of historical edit failures came from
   `if (!sc) {`-class anchors.
6. **Log paths are named per packet and declared to override any
   remembered form** (the lessons plugin steers commands toward the
   previous packet's log names).

**Session management (operator/architect side):**

- Corrections carry **exactly ONE item per message** — the model applies
  one change per cycle and silently drops the rest of a list.
- After ~2 corrections on one session, or on ANY command loop (same
  command twice with no edit between), kill and start a FRESH session
  with only the remaining work — long correction chains poison the
  context and the model replays its last pattern. Kill by recorded PID
  only (a pkill pattern once matched the monitoring itself).
- A mechanical block NEVER contains tree-reverting git commands
  (`checkout --`, `restore`) as a "restore" step — with uncommitted
  stage work in the file, that destroys the stage (2026-08-03
  near-miss). Restores are always explicit inverse edits, and a
  mutation must cover ALL redundant code paths of the mechanism it
  breaks, or a parallel path self-heals the RED.
- The gate-grep EXECUTION GATE discipline stays: stale branch names,
  counts, and line citations rot between authoring and execution — no
  bare `file:line` in a packet without a grep+expected-count beside it.

## 4. Standing rules that survive every step

- Packets are written for a ~100k-context Qwen: log-captured gate runs,
  on-disk todo file, rule-9 compaction recovery. The `/tmp` todo and
  gate-log files are named per packet (`…-f1-N-…`) so nothing leaks between
  packet sessions. Future packets must keep those three rules and the
  per-packet paths (see `QWEN_PROMPTS_F1_lifetime.md`).
- One writer at a time; never run a packet on a dirty or red tree.
- A packet whose EXECUTION GATE fails is never "adapted" — it goes back to
  the architect.

### Mandatory preamble for every future packet that authors program fixtures

Copy the following block into the packet verbatim. It applies to fixtures
newly authored or materially rewritten after 2026-07-18. Existing tests are
legacy evidence: do **not** migrate or reformat them unless a packet explicitly
names that migration as its task.

> **PROGRAM-FIXTURE AUTHORING RULE (mandatory)**
>
> `test_dict_reloc.c` program fixtures are structural, not hand-addressed.
> Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
> the returned step handle when a test must execute or inspect that step, and
> resolve it with `tpStepAddr`; abort the subcase if fixture construction,
> `tpWrite`, or address lookup fails.
>
> Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
> `tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
> authors must identify steps by role (for example `sSource` or `sXeq`) and
>
> **Inspection clause (T5-2, 2026-08-03):** the same applies to READING
> steps. Tests never hand-index step bytes (signatures, length bytes,
> payload offsets) — they use the reader-side accessors in
> `test_dict_reloc.c` (`stepIsForthStep`, `stepIsMarker`,
> `stepSrcTextEq`, `tpSrcPayload`). A new layout fact means a new
> accessor, never an inline byte index. Legacy hand-index sites are
> burned down opportunistically as tests are touched.
> must not publish a calculated byte offset as a normative literal. If a
> packet contains such an offset, stop with `[SOL DEBUGGER HANDOFF]` and report
> the packet defect; do not repair its arithmetic locally.
>
> Use a typed builder accessor such as `tpSrcPayload` for an internal field.
> If the needed step or field helper does not exist, extend the central fixture
> builder first; do not introduce local pointer arithmetic in the test.
>
> Prefer named opcode/parameter constants in builder helpers. An exact byte
> array may remain as the expected value of an encoding assertion. Raw bytes
> inserted into the program fixture are allowed only for the encoding under
> test or a deliberate malformation; they must enter through `tpRaw`, carry an
> adjacent comment naming that purpose, and still use the returned handle and
> builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
> behavior fixture.
>
> This rule is prospective. Do not widen the task by converting untouched
> legacy fixtures.

Every such packet must also include a fixture-lint item before its build gate.
Inspect only added lines in the packet diff and require no matches for either
of these forms outside the central builder:

```text
beginOfProgramMemory + <numeric literal>
tpStepAddr(..., <numeric literal>)
```

Any match is a packet failure. Encoding tests may retain exact expected byte
arrays, but their execution addresses must still obey this rule.
