# F-series roadmap — the complete remaining plan (2026-07-17)

Authored per the owner's 2026-07-17 instruction: plan the ENTIRE remaining
series now instead of stage-by-stage. Authority stays with DESIGN.md §10 and
the packet files; this roadmap is the architect's working plan — scope,
pre-work, predicted packet decomposition, and risk register per stage. The
operator still sequences sessions from `QWEN_RUNBOOK.md`.

**Why F2..F6 packets do not exist yet (and must not):** each of those stages
has a mandatory architect pre-work phase — native-path tracing or an owner
ruling — that DESIGN.md names explicitly ("traced, not inferred"). A packet
authored before its trace would repeat the F1-5 P0 defect at stage scale.
The pacing instruction is honored where it is honorable: everything that CAN
be authored against verified ground now IS (F1.5 is fully authored,
F15-1..F15-5). For F2..F6 this roadmap fixes the pre-work checklists so each
authoring pass is mechanical once its inputs exist.

## Standing discipline (all stages)

- One packet per file; EXECUTION GATE; shared preamble with per-packet
  `/tmp/forth-<stage>-<n>-{todo.md,gate.log}` paths; two-attempt handoff.
- Literals machine-verified at authoring (`wc -c`, byte arrays proven
  against in-tree fixtures); fixture ordering re-derived against LANDED
  predecessor semantics; mutations verified compile-safe and meaningful.
- Arena high-water reported every packet (§5.4); flash deltas recorded per
  stage commit (RULE-1, Stan runs `make dmcp5r47`).
- A failed gate or a spec mismatch goes to the architect; Qwen never adapts.

## Stage F1.5 — §8.9 acceptance harness (IN FLIGHT, fully authored)

| Packet | Status |
|---|---|
| F15-1 run lifecycle (items 1, 7, 9) | DONE `b773597bd` |
| F15-2 entry state + power-off (item 2) | DONE `5a9e9ce2d` |
| F15-3 display parity (item 4) | DONE `c8b87dfa8` |
| F15-4 glyph + type parity (items 5, 6) | READY — `QWEN_PROMPTS_F15_4_glyph_type_parity.md` |
| F15-5 XEQ-name step (item 10) | AUTHORED, gate-locked on F15-4 — `QWEN_PROMPTS_F15_5_xeq_name_step.md` |

Stage close (architect, docs-only): §8.9 coverage note flipped to
"fully covered end-to-end", remaining §8.3-adjacent prose sweep, the
`vBodyWalk` indentation nit in `forth_dict.c`, ledger closeout. Then the
harness pins every later stage: F2..F6 packets inherit an acceptance
backstop that fails loudly if they break lifecycle, entry state, display,
parity, or name-faithfulness.

## Stage F2 — shared RPN parameter semantic core (DESIGN §10.2) — AUTHORED

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

| Packet | Status |
|---|---|
| F2-1 extract the native core | AUTHORED — gate-locked on F15-5 |
| F2-2 bounded name reader | AUTHORED — gate-locked on F2-1 |
| F2-3 shared dispatch + FTOK_C47 re-route | AUTHORED — gate-locked on F2-2 |
| F2-4 parity acceptance sweep | AUTHORED — gate-locked on F2-3 |

**Risks (unchanged):** hidden `_executeOp` call sites or PTP consumers
surfacing at execution → gates STOP, architect re-authors.

## Stage F3 — vocabulary, scopes, XEQ + control flow + globals (DESIGN §10.3)

**Goal:** per-program scopes + interactive scope + global scope (2026-07-16
fold), `XEQ 'NAME'`/`XEQ :NAME:` source forms, `FTOK_XEQN` with kind byte,
control-flow words `IF/ELSE/THEN/BEGIN/UNTIL/AGAIN/WHILE/REPEAT` +
`IMMEDIATE` (2026-07-16 fold), scope-aware reset.

**Architect pre-work:**
1. ~~Owner rulings~~ **RULED 2026-07-18** (§10.3, DESIGN-HISTORY): postfix
   `GLOBAL` (latest-entry marker, IMMEDIATE mechanism); classic `FORGET`
   truncating the global scope at the named word; same arena + same §5.4
   ceiling with a split global/transient high-water report. Zero open
   owner questions remain for F3.
2. Trace the native label grammar (R4 C2) for `XEQ :NAME:` parity —
   against the post-F2 tree.
3. Settle control-flow compilation shapes against the traced stack
   semantics (operand back-patching, compile-time control stack, §3.2
   consuming-pop question) — design pass documented before packets.
4. Extend the F1-5 validator spec with `FTOK_XEQN` kind/len/padding (the
   reserved-token arm at `0x7F05..` was left for exactly this).

**Predicted packets (6-7):** scope headers + lookup order; scope-aware
reset/lifetime integration; XEQN token + parser; XEQ dispatch matrix (B4);
IMMEDIATE + control-flow words (possibly split 2 packets); validator
extension + §2.3 acceptance tests.

**Risks:** largest stage; the scope/lifetime interaction with F1 semantics
must be re-verified against the landed tree at authoring time (the F1.5
harness is the safety net); globals persistence interacts with F1-5's
restore validation.

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
