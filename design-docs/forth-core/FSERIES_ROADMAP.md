# F-series roadmap — the complete remaining plan (2026-07-18)

Authored per the owner's 2026-07-17 instruction: plan the ENTIRE remaining
series now instead of stage-by-stage. Authority stays with DESIGN.md §10 and
the packet files; this roadmap is the architect's working plan — scope,
pre-work, predicted packet decomposition, and risk register per stage. The
operator still sequences sessions from `QWEN_RUNBOOK.md`.

F1.5 and F2 are complete. F3's mandatory trace/design pass and all seven
packets are authored; F3-1 is the next executable packet and F3-2..F3-7 are
gate-locked in dependency order. F4..F6 retain mandatory architect pre-work —
native-path tracing or an owner ruling — before their packets may be authored.
A packet authored before its trace would repeat the F1-5 P0 defect at stage
scale.

## Standing discipline (all stages)

- One packet per file; EXECUTION GATE; shared preamble with per-packet
  `/tmp/forth-<stage>-<n>-{todo.md,gate.log}` paths; two-attempt handoff.
- Literals machine-verified at authoring (`wc -c`, byte arrays proven
  against in-tree fixtures); fixture ordering re-derived against LANDED
  predecessor semantics; mutations verified compile-safe and meaningful.
- Arena high-water reported every packet (§5.4); flash deltas recorded per
  stage commit (RULE-1, Stan runs `make dmcp5r47`).
- A failed gate or a spec mismatch goes to the architect; Qwen never adapts.

## Stage F1.5 — §8.9 acceptance harness (COMPLETE)

| Packet | Status |
|---|---|
| F15-1 run lifecycle (items 1, 7, 9) | DONE `b773597bd` |
| F15-2 entry state + power-off (item 2) | DONE `5a9e9ce2d` |
| F15-3 display parity (item 4) | DONE `c8b87dfa8` |
| F15-4 glyph + type parity (items 5, 6) | DONE (`6775252bf`, debugged) |
| F15-5 XEQ-name step (item 10) | DONE (`546aa8b6c`) |

Stage close is complete: §8.9 coverage is "fully covered end-to-end", the
`vBodyWalk` nit was fixed, and the ledger was closed. The harness now pins
every later stage: F2..F6 packets inherit an acceptance backstop that fails
loudly if they break lifecycle, entry state, display, parity, or
name-faithfulness.

## Stage F2 — shared RPN parameter semantic core (DESIGN §10.2) — COMPLETE

**Status 2026-07-17:** the pre-work is DONE and the packets exist. The PTP
trace was performed against the tree (evidence recorded in
`QWEN_PROMPTS_F2_core.md`, the stage ledger) and the architecture decided:
the byte grammars stay format-specific (changing Forth's operand encoding
would break the F1-5 validator and save format), and the SHARED layer is
the semantic tail — extraction of `_executeOp` into package-new
`programming/param_core.c/.h`, a bounded name reader on execution paths,
`paramCoreValidateDirect`/`paramCoreDispatchDirect` used by both worlds,
and a parity sweep. A traced fact worth remembering: native out-of-range
direct parameters are SILENT no-ops (sprintf only, no error code) — parity
pins that silence.

**Post-stage review 2026-07-18:** F2-1..F2-4 landed, but the first F2-4
mutation stayed green because both engines used the same mutated validator.
The same review found two bounded-reader defects plus last-match and state-
isolation defects in the NUMBER_16 sweep. `QWEN_PROMPTS_F2_5_acceptance_correction.md`
was the bounded correction and committed green as `b5d794df4`; the F3-1 gate
is now open.

| Packet | Status |
|---|---|
| F2-1 extract the native core | DONE (`6f0ffca4b`) |
| F2-2 bounded name reader | DONE (`69e594c71`) |
| F2-3 shared dispatch + FTOK_C47 re-route | DONE (`06ce84b5a`) |
| F2-4 parity acceptance sweep | LANDED (`176e0be0f`) — review found escapes |
| F2-5 acceptance correction | DONE (`b5d794df4`) |

**Risks (unchanged):** hidden `_executeOp` call sites or PTP consumers
surfacing at execution → gates STOP, architect re-authors.

## Stage F3 — vocabulary, scopes, XEQ + control flow + globals (DESIGN §10.3) — FULLY AUTHORED

**Goal:** per-program scopes + interactive scope + global scope (2026-07-16
fold), `XEQ 'NAME'`/`XEQ :NAME:` source forms, `FTOK_XEQN` with kind byte,
control-flow words `IF/ELSE/THEN/BEGIN/UNTIL/AGAIN/WHILE/REPEAT` +
`IMMEDIATE` (2026-07-16 fold), scope-aware reset.

**Architect pre-work — DONE 2026-07-18:** owner rulings, native label grammar
trace, control-flow compilation shapes, two-region arena/persistence model,
and the `FTOK_XEQN` validator extension are recorded in
`QWEN_PROMPTS_F3_core.md`. Zero open design choices remain in F3.

| Packet | Status |
|---|---|
| F3-1 owner-tagged headers | READY — `QWEN_PROMPTS_F3_1_owner_headers.md` |
| F3-2 global region, refs, persistence, validator retarget | AUTHORED, gate-locked on F3-1 — `QWEN_PROMPTS_F3_2_global_region.md` |
| F3-3 current scopes + filtered lookup | AUTHORED, gate-locked on F3-2 — `QWEN_PROMPTS_F3_3_scopes_live.md` |
| F3-4 GLOBAL / IMMEDIATE / FORGET | AUTHORED, gate-locked on F3-3 — `QWEN_PROMPTS_F3_4_marks.md` |
| F3-5 compile-time control flow | AUTHORED, gate-locked on F3-4 — `QWEN_PROMPTS_F3_5_control_flow.md` |
| F3-6 XEQ forms + FTOK_XEQN + B3 | AUTHORED, gate-locked on F3-5 — `QWEN_PROMPTS_F3_6_xeqn.md` |
| F3-7 §2.3 acceptance pins + stage sweep | AUTHORED, gate-locked on F3-6 — `QWEN_PROMPTS_F3_7_acceptance.md` |

**Execution risks:** this remains the largest stage. Each successor gate must
be checked against the exact committed predecessor; the F1.5 harness guards
lifecycle/PEM regressions, and the F3 packets independently pin the split
dictionary, restore validation, scope filtering, branches, and XEQ dispatch.

## Stage F4 — Series C textual parameters (DESIGN §10.4)

**Goal:** `STO 05`, `STO .05`, `STO X`, quoted named forms —
operation-first canonical RPN spelling for all native parameter types of
eligible non-flow items; control/declarative steps stay rejected
(`ERROR_OPERATION_UNDEFINED`); XEQ remains the sole control-flow bridge.

**Architect pre-work — DONE 2026-07-18:** the native grammar and error
table are traced with file:line evidence in `QWEN_PROMPTS_F4_core.md`
(KS-code map, canonical decode spellings, the 0x27 typeable quote, the
upstream `funcIsProgramStopControl` flow set, tamMinMax min/max packing,
the CNST-83 extension-cell fact, the FLAG `< FLAG_W` quirk, the
indirect-NUMBER_16 encoding collision → documented exclusion).

| Packet | Status |
|---|---|
| F4-1 classification + direct numeric params | LANDED `f043c63e7` |
| F4-2 register/flag/shuffle direct forms | LANDED `ac48f50a8` — amendment F4-2A (regInRange raises, not silent) |
| F4-3 named/system-flag/indirect + bounded core | LANDED `fc0fabdad` — amendment F4-3A (one marker table + one cell-span function; +1920 B flash) |
| F4-4 error-table + parity acceptance | LANDED `81d73cd6a` |

**Execution risks:** F4-1 changes landed behavior for END/RTN/STOP/RTN+1
(currently name-dispatchable; no test pins it — verified); the
paramCoreDispatchDirect widening (F4-2) and the bounded-core threading
(F4-3) touch F2-pinned code — their gates re-verify the F2 parity suite.

## Stage F5 — Series D commit validation (DESIGN §10.5, implements E9)

**Goal:** lexical/structural validation at capture commit, two tiers:
structural malformation rejects atomically (prior step preserved);
unresolved names stay legal and advisory.  Executes nothing, allocates
nothing, mutates no live state.

**Architect pre-work — DONE 2026-07-18:** the commit seam is traced
(pemAlpha ITM_ENTER → pemCloseAlphaInput) and the tier boundary is RULED
in `QWEN_PROMPTS_F5_core.md`: tier 1 = violations invariant under every
possible dictionary (structurals, prim placement/pairing, XEQ/FORGET
form syntax, number-shaped parse failures with live-shadow suppression);
item-level rules stay tier 2 because colon definitions shadow items and
numbers (§4.1) — commit-time item rejects would be unsound.  Mechanism:
FORTH_OUTER_CHECK woven into forthOuterRun (one grammar, side effects
gated), soundness pinned by a check-implies-runtime battery.

| Packet | Status |
|---|---|
| F5-1 check mode + soundness battery | LANDED `ba304a3cf` — F5-2A added the missing state-neutrality pin |
| F5-2 commit gate (stage close) | LANDED — amendment F5-2A (regression-triage + entry-point-contract rules) |

**Execution risks:** the weave touches every forthOuterRun branch — the
site inventory greps in F5-1's gate are the control; F5-2 rides the
F15-2/F15-4 capture canaries.

## Stage F6 — capture as a PEM-shaped submode + word catalog (DESIGN §10.6)

**Goal:** capture becomes a real PEM-style submode (real key paths,
catalogs, parameter entry, cancel, cursor, softmenus, alpha transitions;
sink = source text); managed relocation-safe source buffer; nested alpha
capture suspends/restores full state including `tam.colon`; plus the
dedicated Forth word catalog (2026-07-16 fold) listing callable words per
F3 scopes.

**Precondition (AMENDED by owner ruling 2026-07-18):** the audit's
architect half — traces T1-T7, folded in `F6_AUDIT_RESULTS.md`
(2026-07-19) — satisfies the authoring precondition; the hardware bench
(`F6_KEYBOARD_PEM_AUDIT.md` Blocks A-F) is DEFERRED to STAGE-EXIT
confirmation on the DM42n, re-run against the landed F6 behavior before
the stage closes.  Divergence found there is triaged by the architect
(design amendment vs upstream report vs fix).

**PACKETS AUTHORED (2026-07-19), gate-locked in order** — ledger
`QWEN_PROMPTS_F6_core.md`: F6-1 managed capture buffer
(`QWEN_PROMPTS_F6_1_capture_buffer.md`, gate-locked on F5-2); F6-2 TAM
suspend/resume (`..._F6_2_tam_suspend.md`); F6-3 catalogs/menus during
capture (`..._F6_3_capture_menus.md`); F6-4 parameter entry emits
canonical text (`..._F6_4_param_text.md`); F6-5 dictionary-backed word
catalog (`..._F6_5_word_catalog.md`); F6-6 acceptance battery + lifecycle
reset (`..._F6_6_acceptance.md`).

**STAGE F6 PACKETS LANDED (2026-07-19/20)** — Qwen implementation stalled
on F6-1 (owner ruling: architect finishes the series directly); F6-1
through F6-6 all authored, implemented, gate-verified, and committed by
the architect:

| Packet | Status |
|---|---|
| F6-1 managed capture buffer | LANDED `7862b896b` |
| F6-2 TAM suspend/resume | LANDED `a0b3fe7f8` |
| F6-3 catalogs/menus during capture | LANDED `d1b6ac674` |
| F6-4 parameter entry emits canonical text | LANDED `4ca4bfde4` |
| F6-5 dictionary-backed word catalog | LANDED `f7375ef37` |
| F6-6 acceptance battery + lifecycle reset | LANDED `d9b1e894b` |

F6-6 surfaced two pre-existing save/restore-vs-allocator gaps in
`saveRestoreBackup.c` (allocator tracking arrays restored wholesale,
independent of Forth state) — logged in DESIGN-HISTORY.md 2026-07-20 for
the post-series forth-core code audit, not fixed in F6 (out of a
capture-lifecycle stage's scope). Only the stage-exit hardware bench
(`F6_KEYBOARD_PEM_AUDIT.md` Blocks A-F) remains open for this stage.

**Risks:** highest UI risk of the series; the risk controls are the
traces (every fixture is PC-build-derivable — see the deferred-bench
register in `F6_AUDIT_RESULTS.md`), the standing execution gates
(re-verified against the post-F5 tree before handoff), and the stage-exit
bench.  The F15-2/F15-3 harness tests double as regression canaries for
every keyboard-path change.

## After F6 — closeout (no Qwen work)

Per `R6_RESOLUTION_PLAN.md` §3 and the runbook: freeList guard upstream MR
(A prepares, S files) — **upstream ruled 2026-07-19 (UPSTREAM_REPORTS §3):
halt over continue; the guard is reworked to raise `displayBugScreen`
instead of silent-refuse, authored as `QWEN_PROMPTS_FIX6_bugscreen.md`
(FIX-6B, independent of the F-series, runnable any time). The MR now
offers the fail-loud version; one open call-context question (immediate vs.
latched raise) is deferred to upstream in the reply.** Optional upstream
reports (already drafted in `UPSTREAM_REPORTS_b8f79e486.md`), final docs
reconciliation, flash baseline/deltas. Then the accepted backlog is EMPTY;
anything further
starts with a new owner ruling.

## Sequencing summary

```
F1.5 ✔ → F2 ✔ (through F2-5)
  → F3-1..7 ✔ [QWEN, landed]
  → F4-1..4 ✔ [QWEN, landed]
  → F5-1..2 ✔ [QWEN, landed]
  → F6-1..6 ✔ [F6-1 stalled under QWEN; F6-1..6 landed by architect]
  → F6 stage-exit bench [S+A, Blocks A-F vs landed behavior] → closeout [A+S]
```

All 19 gate-locked packets across F3/F4/F5/F6 are landed. Remaining:
the F6 stage-exit hardware bench (S+A), then the test-suite and
forth-core code audits (2026-07-20 owner instruction, no Qwen work),
then closeout.  Owner decision points (S): the F6 stage-exit bench
session (`F6_KEYBOARD_PEM_AUDIT.md` Blocks A-F, after F6-6); flash-delta
runs per stage.
