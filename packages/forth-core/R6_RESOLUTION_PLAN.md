# R6 resolution plan — from NO-GO to GO, with the 2026-07-15 rulings folded in

Companion to `FOR_THE_ARCHITECT_R6_preexecution_audit.md`. That file is the
audit record; this file records the architect rulings received after it and
the ordered path to execution. Authority: these rulings are class-2 (architect
decisions after the current DESIGN.md revision) until Step 1 folds them into
DESIGN.md.

---

## 0. Execution status (reconciled 2026-07-16)

**GO — Steps 1 through 6 are complete.**

- Step 1, authority fold: `1927d60af`.
- Step 2, prompt hygiene and R1-4 withdrawal: `8a5fde237`.
- Step 3, clean baseline: re-run green on 2026-07-16.
- Step 4, corrected R1-3: completion commit `ae1a5a1af`. The earlier outer
  arm (`4ab421a32`) and focused test body (`29f94d8e4`) were reconciled in
  that commit without rewriting shared history; all three required mutations
  went RED before the restored clean-tree gate passed.
- Step 5: R6-1 `29f94d8e4`, R6-2 `416c1e26b`, R6-3 `4ab421a32`.
  R6-4/R6-5 remain optional, report-only characterization probes and do not
  block GO.
- Step 6, GO checkpoint: clean gate passed with `FORTH SELF-TEST: ALL PASSED`,
  `BUILD + SELF-TEST GREEN`, exit 0, and arena
  `here=36 sizeBlocks=16 freeRamDelta=64`.

The next required action is Step 7 / **F1 lifetime foundations**. Author its
bounded execution prompt before changing production code.

---

## 1. Rulings recorded (2026-07-15, from Stan)

**RULE-1 — Platform.** The package targets the **R47 on DM42n (DMCP5)**.
DM42 compatibility is *not* a strict requirement and must not constrain design
choices. Where a flash increase simplifies the implementation, take it.
Consequences:
- CLAUDE.md's "Target: DM42-class hardware. Flash/RAM budget is the binding
  constraint" is retargeted: RAM/arena discipline stays (it is a pocket
  calculator and the arena is shared), but flash ceases to be a design-vetoing
  constraint; per-commit reporting shifts from "justify any byte" to "record
  the delta" (measured on `make dmcp5r47`).
- **Q9 is closed**: the shrunken DM42 PACKAGE1/PACKAGE3 headroom (AUD-U6) is
  noted, not binding. No re-pricing pass needed; the R4 rulings that accepted
  flash costs stand as written.
- **Q2 is closed by implication**: keep `boundedRead` (R1-2's per-token
  guards) as defense-in-depth *alongside* the future restore-time validator —
  the flash argument for removing it is gone, and belt-and-suspenders wins on
  a handheld.
- **Q3 default**: bless the suite's `FORTH ARENA:` line as the §5.4 reporting
  mechanism; `bench/hwm.fs` becomes optional (add later only if a scripted
  benchmark is wanted).

**RULE-2 — Q8: named local labels enter Forth's architecture, mimicking
upstream.** Design in §2 below. Bare names stay global; locals are reached by
upstream's own explicit colon spelling; resolution is *delegated* to
upstream's resolver with upstream's context, never re-implemented.

**RULE-3 — Q9.** Flash increase is fine when justified (subsumed by RULE-1).

Still open for Step 1 rulings: Q1 (§8.9 acceptance: implement vs downgrade),
Q4 (interim NOPARAM item dispatch statement), Q5 (bare-render
non-injectivity sentence), Q6 (decode.c renderer-side bound, now that upstream
guarded the walker), Q7 (F8/F9.1 deferral bookkeeping).

---

## 2. Q8 design — how Forth incorporates named local labels (mimic upstream)

### 2.1 What upstream built (the model to mimic)

- Two label kinds share `labelList[]`: global (payload kind byte
  `STRING_LABEL_VARIABLE` = 253) and named local (`LOCAL_LABEL_VARIABLE` =
  249, entries with `step < 0`, name at `labelPointer`).
- One resolver: `findNamedLabel(name, labelType)` /
  `findNamedLabelWithDuplicate(...)` with `namedLabels_t` selector — and the
  selector values **are** the payload kind bytes (`GLOBAL_LABELS = 253`,
  `LOCAL_LABELS = 249`, `ALL_LABELS = 0`).
- Local resolution is **position-sensitive**: searched only within
  `currentProgramNumber`; the match is the first occurrence *after*
  `currentLocalStepNumber`, else the first in the program.
- Entry: TAM `:` key sets `tam.colon`; steps encode
  `[opcode][249][len][name]`; listings render `XEQ :name:` (vs `XEQ 'name'`
  for globals). Locals are *defined* by RPN `LBL :name:` steps — that side is
  pure RPN and Forth does not touch it.

### 2.2 The Forth design (extension principle applied)

**Syntax = the canonical listing spelling, both kinds.** The future `XEQ`
parsing word (accepted B2 ruling) accepts exactly the forms the PEM listing
shows: `XEQ 'NAME'` → global request; `XEQ :NAME:` → local request. No
invented aliases (V4). A bare name in Forth source (§4.1 step 5) remains
**global-only** — exactly upstream's rule that locals require the explicit
colon spelling. This keeps the migration's GLOBAL_LABELS pin for bare names
and `forthResolveXEQ` intact and makes AUD-U1's gate (`XEQ :FOO` must never
fall through to Forth vocabulary) permanent policy: **a local request
resolves local labels or fails; it never falls back to items, colon words, or
global labels** — mirroring `_executeOp`'s kind-faithful dispatch.

**Encoding = upstream's payload shape, verbatim.** `FTOK_XEQN` inline data
becomes `[kind][len][name bytes][pad to cell]` with kind ∈ {253, 249} — byte-
compatible with the RPN step payload after the opcode. Arithmetic: `inline =
2 + len`, `padded = (inline + 1) & ~1`, `total = 2 + padded`; len 1..31;
kind validated against exactly {253, 249} on decode (anything else →
`ERROR_INVALID_CORRUPTED_DATA`).

**Resolution = delegation, never re-implementation.** At run time the XEQN
arm passes the stored kind byte straight through:
`findNamedLabel(name, kind)` — the same resolver, the same
`currentProgramNumber`/`currentLocalStepNumber` context the RPN step path
uses. Position-sensitivity is therefore *inherited*, not re-specified:
- From a program-context Forth step, `currentLocalStepNumber` is live and
  points at the executing `ITM_FORTH` step, so `XEQ :T:` inside a Forth line
  behaves exactly like an RPN `XEQ :T:` step at that position ("next `:T:`
  after here, else first in the program").
- Interactively, it resolves the way upstream's interactive TAM colon entry
  resolves (current cursor program) — same globals, same answer.
- The B2 fallback chain (label miss → callable Forth target) applies to the
  **global/quoted** form only; the local form has no fallback.

**Dispatch** follows the accepted B4 matrix unchanged: a resolved label (either
kind) takes the native XEQ path (`dynamicMenuItem = -1; fnExecute(label)` /
native program-running semantics); locals add no new dispatch arm because
`fnExecute` already handles local-label registers.

**Scoping rhyme (for the vocabulary series).** The R4-accepted per-program
colon-word scopes are semantically parallel to named local labels
(program-local names). Implement the owner scope by mirroring upstream's
pattern — `labelList[]` entries carry a `.program` field; `fdict` entries
gain an owner tag with lookups filtered to the current scope — so the two
"local name" systems share one mental model and one context definition
(`currentProgramNumber`). This is design guidance for stage F3, not a new
mechanism to invent.

**Grammar caveat (trace, don't guess).** Whether upstream label names may
contain spaces/glyphs, and the exact quote/colon scanning rules, are settled
by tracing the native TAM/entry paths per the R4 C2 rule ("Qwen must not
infer them from examples"). The stage-F3 prompt carries the traced grammar.

### 2.3 Acceptance tests the F3 prompt must carry

1. **Mimicry pin (the load-bearing test):** a program with `LBL :T:` …
   `XEQ :T:` (RPN step) at position P, and a variant where position P is a
   Forth source step `XEQ :T:` — both runs reach the *same* label instance,
   including the position-sensitive "next occurrence after P" case with two
   `:T:` definitions. Mutation: resolve locals with `ALL_LABELS` or from
   program start — the two-definition case picks the wrong instance.
2. Local request never falls through: `XEQ :FOO:` with no local `FOO` but a
   Forth word `FOO` and an item/global label of that name → label-not-found
   surface, nothing dispatched, no step recorded. (Extends the AUD-U1
   regression test to the source form.)
3. Kind byte round-trip: compiled `XEQ 'A'` vs `XEQ :A:` bodies differ only
   in the kind byte; decode validates {253, 249}; truncated/invalid kind →
   `ERROR_INVALID_CORRUPTED_DATA`, sentinel X unchanged.
4. Bare-name pin: bare `FOO` in Forth source still resolves GLOBAL_LABELS
   only (mutation: switch step 5 to ALL_LABELS → a local-label collision test
   goes red).

---

## 3. Step-by-step: resolving the blockers to GO

Single-writer discipline throughout; every code step ends with the green gate
(`./packages/forth-core/build-test.sh`, both banners, exit 0) and quotes the
arena line. Owners: **A** = architect session (Claude), **Q** = Qwen from an
architect-written prompt, **S** = Stan.

**Step 0 (done, this session).** Rulings recorded here; R6 report + this plan
in the tree, uncommitted, for review.

**Step 1 — A: the DESIGN.md amendment pass** (one docs-only commit; clears
AUD-B3, H1, H2, H3, M2, M6, M8 and the LOW batch).
  1. Fold the R4 accepted architecture as explicit target-state text:
     lifecycle (pending-reset flag as truth predicate, active-frame guard,
     PEM single-step = fresh generation), dynamic scan tracking, per-program
     scopes + interactive scope + RECURSE, restore-time validator (with the
     RULE-1 note that `boundedRead` stays), E9 → structural-atomic /
     names-advisory split, §8.10-1 reconciled to the scope ruling.
  2. Fold §2 above (named local labels) into §2.2 (FTOK_XEQN kind byte),
     §3.3.6/§4.1 (label kinds; bare = global; `XEQ` source forms), §4.2
     (labelType per call site; the GLOBAL_LABELS pins), §0.3 (labelList local
     entries).
  3. Platform retarget (RULE-1): §5.4 wording (arena ceiling stays; flash =
     report-the-delta on `make dmcp5r47`; FORTH ARENA line blessed per Q3);
     edit CLAUDE.md's target/constraint line to R47/DM42n.
  4. Corrections: E7/P-H2 rewritten to the landed `-2` cursor branch (H1);
     the ~8 stale "required change/does not exist" claims rewritten as
     implemented invariants with anchors (H2); §3.3 error-table label row
     annotated stage-interim-until-XEQN (H3); §3.2 pseudocode `return`s →
     `INNER_LEAVE()` (M10); REM-arm rationale updated to upstream's own fix
     (U3); `dynamicMenuItem` comment notes upstream adoption (U4); PLTFCNS
     joins the §4.1 guardrail examples (U5); citation refresh post-migration
     incl. LAST_ITEM 2870 and row 4722 (M8); LOW batch L1-L6, L9 (history
     binary→textual, README limitation line, PROPOSED "RATIFIED" flip,
     §9.x→§8.x comment refs may be listed for a later sweep, picker 1000-step
     cap documented, softmenu numbering deviation documented, Stage1.md
     banner).
  5. Rule the remaining questions in place: Q1 (recommended: downgrade §8.9
     wording per item now, schedule the end-to-end harness after stage F1),
     Q4 (interim statement; F3 makes bare parameterised dispatch an atomic
     error), Q5 (one sentence: listing is intentionally contextual), Q6
     (decision on decode.c renderer bound — recommended: accept upstream's
     walker guard as sufficient, revisit only on evidence), Q7 (close or
     carry F8/F9.1).
  6. Append the DESIGN-HISTORY entry; run the gate once for the record;
     commit (`forth-core: fold R4 architecture + R6 audit corrections`).

**Step 2 — A: prompt hygiene** (one docs-only commit; clears AUD-B1, B2, M5,
M7).
  1. Rewrite R1-3 per report §10.1 (no resolver refactor; `ITM_sin`;
     corrected test claims; fresh preamble; own commit block).
  2. R1-4: `STATUS: WITHDRAWN` banner pointing at B1/B4 and stage F3.
  3. `custom_package/QWEN_IMPLEMENTATION_PROMPTS.md`: ARCHIVED banner.
  4. Author the **AUD-U1 prompt** (add `&& !tam.colon` to the tam.c fallback
     guard + the regression test driving `_tamProcessInput` with
     `tam.colon = true`; mutation: remove the gate).
  5. Optionally author the small §10.3 prompts (picker owning-scan via
     `forthOwningProgramStart`; E3 precondition comment + debug assert).

**Step 3 — S: re-verify the baseline.** Run the gate once on the clean tree
(the audit was read-only; last documented green is HEAD's commit message).
Record the arena line. Optional but recommended once: `make dmcp5r47 f=1` and
record forth-core's flash delta as the RULE-1 baseline number.

**Step 4 — Q: execute corrected R1-3.** Forward item lookup lands
(prim → colon → number → **item** → label). Green gate, both mutations RED
then restored, arena line, single commit.

**Step 5 — Q: execute AUD-U1** (and the optional §10.3 prompts). Independent
files from R1-3, but run sequentially (single writer).

**Step 6 — GO checkpoint.** All §13 checklist boxes in the R6 report are now
checkable. Pre-execution state is clean: no pending prompt contradicts the
(now unified) authority.

**Step 7 — A then Q, per stage: the future series in R4 order**, each stage
authored only after the previous is green, each with DESIGN excerpts, traced
native behavior, old-contract test migration lists, executed RED mutations,
arena + flash-delta reporting:
  - **F1 — lifetime foundations:** pending-reset flag/event + active-frame
    guard (replaces generation-equality as truth; counters demoted to
    diagnostics), PEM single-step reset, dynamic scan tracking in the arena,
    `RECURSE`, restore-time full validator (boundedRead retained per RULE-1).
  - **F2 — shared RPN parameter semantic core:** trace every supported native
    PTP path; factor the bounded decoder (upstream override files acceptable
    per RULE-1); both native steps and `FTOK_C47` feed it.
  - **F3 — vocabulary/XEQ:** per-program scopes + interactive scope (the
    labelList-`.program` pattern), `XEQ 'NAME'` / `XEQ :NAME:` parsing word,
    FTOK_XEQN with kind byte (§2 above, superseding withdrawn R1-4), B4
    dispatch matrix, §2.3 tests.
  - **F4 — series C textual parameters** (traced grammar + error table).
  - **F5 — series D commit validation** (E9 successor: structural = atomic
    reject, names advisory).
  - **F6 — capture submode** (managed source buffer, relocation-safe handle;
    `tam.colon` included in the suspended/restored TAM state).
  - **F1.5/parallel — §8.9 end-to-end acceptance harness** per the Q1 ruling,
    scheduled right after F1 so the lifecycle tests pin the *new* semantics.

**Step 8 — housekeeping at any green point:** freeList guard upstream MR
(unchanged plan, case strengthened by U9); optionally report upstream's own
tam.c item-scan colon gap and the decode.c renderer indexing.

---

## 4. GO condition restated

GO is declared at Step 6: authority unified (Step 1), prompts hygienic
(Step 2), baseline green (Step 3), the only two formerly-pending prompts
resolved (R1-3 executed, R1-4 withdrawn), and the new-feature gate fix landed
(AUD-U1). Everything after Step 6 is normal staged development under the
existing prompt template discipline.
