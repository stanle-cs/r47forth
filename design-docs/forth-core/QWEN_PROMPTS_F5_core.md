# Stage F5 — Series D commit validation (E9): ledger + derivation + design pass

> Operator sequencing lives in `QWEN_RUNBOOK.md`; the plan in
> `FSERIES_ROADMAP.md`.  This file is the stage ledger and the E9
> derivation DESIGN §10.5 requires ("derive the validator FROM the
> tokenizer, not parallel it").  Authored 2026-07-18; packets gate-locked
> on the F4-4 stage commit.  The §0 binding rules of
> `QWEN_PROMPTS_F3_core.md` apply to both packets.

## 1. The commit seam (traced 2026-07-18)

- ENTER during Forth capture is `pemAlpha`'s `ITM_ENTER` arm
  [packages/forth-core/programming/manage.c:924-936]: it computes
  `wasForth`/`hadText`, calls `pemCloseAlphaInput()` (finalize + advance),
  then re-opens capture per the multi-line lock
  (`forthEntryStateAtInsertion()`).
- The step bytes are maintained INCREMENTALLY while typing (the
  placeholder inserts at capture open [manage.c:859-871] and each edit
  rewrites the payload [manage.c:1000-1008]), so at ENTER the step already
  holds the text; `pemCloseAlphaInput` performs the COMMIT (E3
  empty-line handling, cursor advance, alpha close)
  [manage.c:1017-1042].
- Therefore the E9 reject point is BEFORE `pemCloseAlphaInput`: refuse
  the commit, keep capture open with `aimBuffer` intact for correction,
  error displayed.  "Prior step preserved unchanged" holds trivially
  (nothing before the authored step is touched) and the authored line
  stays editable.  Non-ENTER exits from capture (scroll-away, EXIT) are
  NOT commits and stay out of scope.

## 2. Tier boundary — the load-bearing ruling (DECIDED)

E9's tiers: structural malformation = hard atomic reject; name resolution
= advisory, never blocks.  The derivation must be SOUND: a commit-time
reject may never refuse a line that could legally run.  Under §4.1,
colon definitions shadow ITEMS and NUMBER-SHAPED tokens (order: prim →
colon → number → item), and the commit-time dictionary is a DIFFERENT
lifetime from the run that will execute the line — so no item-table-based
rule is ever certain at commit.  Consequently:

**Tier 1 (reject) = violations invariant under every possible dictionary:**
- (a) tokenizer: token > FORTH_TOKEN_MAX → `ERROR_INPUT_TOO_LONG`;
- (b) `:`/`;` structure (structurals are unshadowable): nested `:`,
  `:` without a name, definition name empty/over FORTH_NAME_MAX, stray
  `;`, unterminated definition → `ERROR_INVALID_NAME`;
- (c) control-word placement and pairing (PRIMS are unshadowable, §4.1
  step 1 wins): IF/ELSE/THEN/BEGIN/UNTIL/AGAIN/WHILE/REPEAT outside a
  definition (`ERROR_OPERATION_UNDEFINED`), pairing/kind/underflow
  violations and `;` with an open frame (`ERROR_INVALID_NAME`), depth
  overflow (`ERROR_RAM_FULL`);
- (d) RECURSE outside a definition (`ERROR_OPERATION_UNDEFINED`);
  GLOBAL/IMMEDIATE without a same-line closed definition
  (`ERROR_INVALID_NAME`); FORGET in compile state or without a name
  (`ERROR_INVALID_NAME`);
- (e) XEQ form syntax (XEQ is structural): missing token, neither
  `'N'` nor `:N:` shape, name length out of 1..31 →
  `ERROR_INVALID_NAME`;
- (f) number-shaped malformation: a token that CLASSIFIES as a number
  (§3.3.5 grammar) but fails conversion → `ERROR_INVALID_NAME`.  Sole
  soundness edge: a colon word whose name is exactly such a token would
  be shadow-legal at runtime; the checker consults the LIVE dictionary
  only to SUPPRESS this reject (a current-lifetime shadow accepts), and
  the residual false-reject (shadow defined only in a later lifetime,
  name exactly a parse-failing numeric form) is ACCEPTED and documented
  — E9 names "malformed numbers" tier 1 explicitly, and the colliding
  name class is pathological.

**Tier 2 (advisory, always commits):** every name — unknown words,
colon references, labels, ITEM names including flow items and
parameterized items with or without well-formed parameters, FORGET's
target, XEQ's resolvable name, named variables/menus/system flags.
Their errors surface at execution exactly as F3/F4 landed them.  The
§8.6 picker remains the only advisory UI; the commit adds NO new
surface for tier 2 (ruled here: silence is the advisory — resolution is
the pre-scan/run's job, §8.4 E9).

Note what this DELIBERATELY leaves out of tier 1: F4 parameter grammar
and ranges, flow-item rejects, bare parameterized items.  Each sits
behind a shadowable ITEM name, so commit-time enforcement would be
unsound.  They remain execution-time errors (F4's), reachable one run
later — RPN's own model for a step that fails at run time.

## 3. The derivation mechanism (DECIDED)

`forthOuterRun` gains mode `FORTH_OUTER_CHECK`.  The check IS the
tokenizer + state machine running with every side effect gated off —
one grammar, one walker, no parallel validator.  Gated site classes
(the F5-1 packet carries the machine-enumerated inventory with greps):
dictionary mutation (`startDefinition`/`finishDefinition`/
`abortDefinition`/every `forthDictEmit*`), execution
(`forthPrims[].fn`, `forthInner`, `reallyRunFunction`, `fnExecute`,
`forthXeqnDispatch`, marker dispatch), stack pushes
(`forthPushInt32/Real34`), lifecycle (`forthRunGenBump` never runs in
check — entry is via a dedicated public), ASLIFT writes, and the
GLOBAL/IMMEDIATE/FORGET actions.  Check-local simulation state (a small
struct on forthOuterRun's frame): `simOpen` (definition open),
`simClosedThisLine` (same-line tracker analog), and a sim control stack
using the SAME `CTL_*` kinds and `FORTH_CSTACK_DEPTH` with a per-word
pairing table (IF:+O; THEN:−O; ELSE:−O+O; BEGIN:+D; UNTIL/AGAIN:−D;
WHILE:−D+O+D; REPEAT:−D−O).  Number checks run classify+parse into
locals with the §2(f) suppression consult (`forthFindColonRef`,
read-only).  Items and labels are NEVER consulted.

Public API: `bool forthCheckSourceLine(const char *source)` — true =
committable; false = tier-1 violation, error already displayed via the
standard `displayCalcErrorMessage` path (that display IS the commit
UX).  Divergence control: the F5-1 acceptance battery pins the
soundness implication — every check-REJECTED line, executed for real,
must raise the SAME error code; check-ACCEPTED lines carry no claim.

## 4. Packets

| Task | Packet | Status | Dependency |
|---|---|---|---|
| F5-1 the check mode + soundness battery | `QWEN_PROMPTS_F5_1_check_mode.md` | AUTHORED, gate-locked | F4-4 stage commit green |
| F5-2 commit integration + atomic reject | `QWEN_PROMPTS_F5_2_commit_gate.md` | AUTHORED, gate-locked | F5-1 committed green |

Per-packet `/tmp/forth-f5-N-*` paths; strict order.  RULE-1: F5-1 adds
the check weave (flash delta recorded); F5-2 is one call site + tests.
Stage close (architect, docs-only): §8.4 E9 flipped to implemented,
§10.5 marked landed, tier ruling folded into DESIGN-HISTORY.
