# Stage F1 lifetime foundations — packet ledger and execution order

> Operator sequencing for the whole project (what to run, in what order,
> including beyond F1) lives in `QWEN_RUNBOOK.md`.

Origin: accepted R4 architecture, the R6 pre-execution audit, and
`R6_RESOLUTION_PLAN.md` Step 7. This file is the stage ledger only; every
task lives in its own self-contained packet file (one packet per file). F1-1
was authored first by gpt 5.6 sol; per the owner's 2026-07-16 instruction,
F1-2 through F1-5 were authored ahead of execution, and F1-1 was then moved
into its own file to conform (content unchanged). Because F1-2..F1-5's
predecessors have not landed yet, each packet opens with a hard EXECUTION
GATE that re-verifies the expected post-predecessor tree by anchor greps and
STOPs on any mismatch. If a predecessor lands with any deviation from its
packet, the successor's gate fails closed and the architect must re-author
before execution.

## Status and dependency order

| Task | Packet | Status | Dependency |
|---|---|---|---|
| F1-1 pending-reset truth + active-frame guard | `QWEN_PROMPTS_F1_1_pending_reset.md` | **READY TO EXECUTE** | R6 GO checkpoint (`b7fd711ff`) |
| F1-2 top-level run/SST lifetime signaling | `QWEN_PROMPTS_F1_2_run_signaling.md` | AUTHORED — gate-locked | F1-1 committed green |
| F1-3 dynamic arena-backed pre-scan tracking | `QWEN_PROMPTS_F1_3_scan_tracking.md` | AUTHORED — gate-locked | F1-2 committed green |
| F1-4 compile-only `RECURSE` | `QWEN_PROMPTS_F1_4_recurse.md` | AUTHORED — gate-locked | F1-3 committed green |
| F1-5 full restore-time threaded-code validator | `QWEN_PROMPTS_F1_5_restore_validator.md` | AUTHORED — gate-locked | F1-4 committed green |

Execute the tasks strictly in order, one packet per session, each on a clean
green tree; every packet's EXECUTION GATE must pass before its first edit.
Do not infer task content from the outline at the end of this file — each
packet is self-contained and authoritative for its task. All five packets
share the same PREAMBLE (rules, gate command, two-attempt debugger handoff,
identical except for the packet's own paths: rule 9's self-path and the
per-packet `/tmp/forth-f1-N-todo.md` / `/tmp/forth-f1-N-gate.log`
filenames); paste it with the task exactly as it appears in the packet file.

The preamble is written for a small-context (~100k token) implementer and a
harness that may compact mid-task: gate runs are log-captured and inspected
only through bounded greps (rule 3), progress lives in an on-disk todo file
with explicit MUTATION APPLIED/RESTORED markers (rule 2), and rule 9 defines
recovery after any context loss (re-read the packet file + todo file +
`git diff`; never continue from memory). The todo and gate-log paths are
named per packet (`…-f1-N-…`, adopted 2026-07-17) so a stale file left by an
earlier packet's session can never be mistaken for current state during
rule-9 recovery. Future packets must keep these three rules and the
per-packet paths.

After each stage commit lands, the architect (not the implementer) re-runs
the successor packet's EXECUTION GATE greps before handing it to Qwen.

---

## Ordered follow-ons (superseded 2026-07-16 — packets now exist)

The outline below is retained as the planning record. The executable packets
live in the files named in the status table above; each opens with an
EXECUTION GATE that re-verifies the tree. The outline is NOT authoritative
for implementation:

1. **F1-2:** move top-level reset signaling to the real `runProgram` entry,
   include every PEM single-step as a fresh lifetime, preserve nested RPN
   launches, remove superseded scattered bump semantics, and migrate the named
   generation/SST tests.
2. **F1-3:** replace `FORTH_SCAN_MAX` and `forthScannedProgs[]` with compact,
   dynamic, capacity-bounded tracking inside the managed dictionary arena;
   ordinary dictionary exhaustion is the only capacity failure and arena
   high-water reporting is mandatory.
3. **F1-4:** add compile-only immediate `RECURSE`; it emits a call to the
   current smudged definition by index and does not make that name visible.
4. **F1-5:** fully validate restored header padding, body/cell extents, token
   encodings, operands, call indices, branch targets, reserved ranges, and
   terminating EXIT. Invalid Forth state is orphan-cleared while RPN source is
   preserved. Existing `boundedRead` checks remain defense in depth. XEQN is
   still reserved until F3 and must be added to validation when F3 lands.
