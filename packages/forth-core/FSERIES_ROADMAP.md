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

**Architect pre-work:** trace the native RPN grammars and error table
(exact spellings, ranges, create semantics, error codes) — the F4 packets
carry the resulting grammar + error table verbatim. Builds directly on the
F2 shared core; do not author until F2 lands.

**Predicted packets (3-4):** grammar/tokenizer extension; numbered/dotted
register forms; named/quoted forms + create semantics; error-table
acceptance sweep.

**Risks:** grammar edge cases (dot forms, indirection) — every spelling in
the packet must carry its traced file:line origin.

## Stage F5 — Series D commit validation (DESIGN §10.5, implements E9)

**Goal:** lexical/structural validation at capture commit, two tiers:
structural malformation rejects atomically (prior step preserved);
unresolved names stay legal and advisory. Executes nothing, allocates
nothing, mutates no live state.

**Architect pre-work:** enumerate the structural-reject grammar from the
landed F3/F4 tokenizer (it IS the grammar — no second grammar may exist);
define the advisory surface (how "unresolved name" is shown without error).

**Predicted packets (2-3):** the validator (pure function over source
text); commit-path integration + atomic reject; acceptance tests (both
tiers + the §8.4 E9 pins).

**Risks:** small stage; the main hazard is validator/tokenizer divergence —
the packet must derive the validator FROM the tokenizer, not parallel it.

## Stage F6 — capture as a PEM-shaped submode + word catalog (DESIGN §10.6)

**Goal:** capture becomes a real PEM-style submode (real key paths,
catalogs, parameter entry, cancel, cursor, softmenus, alpha transitions;
sink = source text); managed relocation-safe source buffer; nested alpha
capture suspends/restores full state including `tam.colon`; plus the
dedicated Forth word catalog (2026-07-16 fold) listing callable words per
F3 scopes.

**HARD PRECONDITION (DESIGN):** a dedicated keyboard/PEM audit with
hardware-derived tests BEFORE any packet is authored — Stan runs the
hardware side; the architect writes the audit doc and derives fixtures.

**Predicted packets (5-6):** managed source buffer + handle; submode key
dispatch; suspend/restore state machine; catalog data source (F3 scopes);
catalog UI on the §8.6 softmenu machinery; end-to-end capture acceptance.

**Risks:** highest UI risk of the series; the audit is the risk control —
no packet before it. The F15-2/F15-3 harness tests double as regression
canaries for every keyboard-path change.

## After F6 — closeout (no Qwen work)

Per `R6_RESOLUTION_PLAN.md` §3 and the runbook: freeList guard upstream MR
(A prepares, S files), optional upstream reports (already drafted in
`UPSTREAM_REPORTS_b8f79e486.md`), final docs reconciliation, flash
baseline/deltas. Then the accepted backlog is EMPTY; anything further
starts with a new owner ruling.

## Sequencing summary

```
F15-4 [QWEN now] → F15-5 [QWEN] → F1.5 close [A, docs]
  → F2 trace [A] → F2-1..5 [author A, run QWEN]
  → F3 rulings [S] + design pass [A] → F3-1..7 [A→QWEN]
  → F4 trace [A] → F4-1..4 [A→QWEN]
  → F5 grammar derivation [A] → F5-1..3 [A→QWEN]
  → F6 keyboard/PEM audit [A+S] → F6-1..6 [A→QWEN]
  → closeout [A+S]
```

Owner decision points (S): F3 rulings (globals spelling / FORGET / arena
accounting) — needed before F3 packets, can be ruled any time; F6 hardware
audit participation; flash-delta runs per stage.
