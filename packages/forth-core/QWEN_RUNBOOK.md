# QWEN_RUNBOOK — what to run, in what order (2026-07-16)

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
| `QWEN_PROMPTS_R5_tooling.md` | executed (`e579d1821` + R5-7) |
| `QWEN_PROMPTS_R6_followups.md` (R6-1..R6-5) | R6-1..3 committed (`29f94d8e4`, `416c1e26b`, `4ab421a32`); R6-4/R6-5 complete (report-only, owner-confirmed 2026-07-16) |
| `custom_package/QWEN_PROMPTS_refresh-base-commit.md` (BP-1..7) | executed |
| `custom_package/QWEN_IMPLEMENTATION_PROMPTS.md` | ARCHIVED — execution hazard, never run |

The only remaining implementation work is the F series. After F6 the
accepted backlog is empty (DESIGN-HISTORY 2026-07-16 entry).

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
**[ARCHITECT]** = come back to the architect session (Claude) first —
those packets do not exist yet by design (each is authored against the
green tree its predecessor leaves behind).

| # | Step | Who | Input |
|---|---|---|---|
| 1 | F1-1 pending-reset truth + active-frame guard | **[QWEN]** | `QWEN_PROMPTS_F1_1_pending_reset.md` |
| 2 | F1-2 runProgram = sole top-level lifetime signal | **[QWEN]** | `QWEN_PROMPTS_F1_2_run_signaling.md` |
| 3 | F1-3 dynamic arena-backed scan tracking | **[QWEN]** | `QWEN_PROMPTS_F1_3_scan_tracking.md` |
| 4 | F1-4 compile-only RECURSE | **[QWEN]** | `QWEN_PROMPTS_F1_4_recurse.md` |
| 5 | F1-5 restore-time threaded-code validator | **[QWEN]** | `QWEN_PROMPTS_F1_5_restore_validator.md` |
| 6 | F1.5 — §8.9 end-to-end acceptance harness (Q1 ruling: pins the NEW F1 semantics). Architect authors packets, then Qwen runs them. Architect also does the post-F1 DESIGN reconciliation pass (§8.3 interim text etc.) here. | **[ARCHITECT]** then [QWEN] | DESIGN §8.9 |
| 7 | F2 — shared RPN parameter semantic core. Architect traces every native PTP path first (tracing, not inference), then authors packets. | **[ARCHITECT]** then [QWEN] | DESIGN §10.2 |
| 8 | F3 — vocabulary/scopes/XEQ/XEQN **+ control-flow words & IMMEDIATE + global scope** (2026-07-16 folds). Architect design pass first: rule globals' entry spelling / FORGET / arena accounting; trace label grammar (R4 C2); settle control-flow compilation shapes; extend the F1-5 validator with XEQN. Then packets. | **[ARCHITECT]** then [QWEN] | DESIGN §10.3 |
| 9 | F4 — Series C textual parameters. Architect traces native grammar + error table, then packets. | **[ARCHITECT]** then [QWEN] | DESIGN §10.4 |
| 10 | F5 — Series D commit validation (E9 two tiers). | **[ARCHITECT]** then [QWEN] | DESIGN §10.5 |
| 11 | F6 — capture submode **+ dedicated Forth word catalog** (2026-07-16 fold). HARD PRECONDITION: its own keyboard/PEM audit with hardware-derived tests BEFORE any packet is authored. | **[ARCHITECT]** (audit + packets) then [QWEN] | DESIGN §10.6 |

## 3. After the series — no Qwen work

Owners as in `R6_RESOLUTION_PLAN.md` §3: **A** = architect session (Claude),
**S** = Stan.

- Step 8 housekeeping: freeList guard upstream MR — **A** prepares the
  patch + MR text (the guard is already in-tree; packets forbid touching
  `freeList.c` precisely because it is earmarked for upstream), **S** forks,
  pushes, and opens the MR upstream. Optional upstream reports (tam.c
  item-scan colon gap; decode.c renderer indexing) — **A** drafts in the
  `UPSTREAM_BUG_REM_alpha_menu.md` style, **S** files them.
- Final docs reconciliation pass (DESIGN.md stale/interim prose,
  DESIGN-HISTORY, ledger closeout) — **A**, docs-only commits.
- Flash baseline/deltas (`make dmcp5r47`, RULE-1) — **S** runs, **A**
  records.
- Then the accepted implementation backlog is **empty**; anything further
  starts with a new owner ruling (**S**).

## 4. Standing rules that survive every step

- Packets are written for a ~100k-context Qwen: log-captured gate runs,
  on-disk todo file, rule-9 compaction recovery. The `/tmp` todo and
  gate-log files are named per packet (`…-f1-N-…`) so nothing leaks between
  packet sessions. Future packets must keep those three rules and the
  per-packet paths (see `QWEN_PROMPTS_F1_lifetime.md`).
- One writer at a time; never run a packet on a dirty or red tree.
- A packet whose EXECUTION GATE fails is never "adapted" — it goes back to
  the architect.
