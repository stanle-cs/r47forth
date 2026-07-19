# Stage F3 — vocabulary, scopes, XEQ/XEQN, marks, control flow: ledger + trace + design pass

> Operator sequencing lives in `QWEN_RUNBOOK.md`; the series-wide plan in
> `FSERIES_ROADMAP.md`. This file is the stage ledger, the §10.3 trace
> record, AND the F3 design pass (the pre-work DESIGN.md §10.3 requires).
> Every task lives in its own self-contained packet file. Authored
> 2026-07-18 on the post-F2-5 tree (`b5d794df4`) per the owner's
> author-the-rest instruction; every packet opens with a hard EXECUTION
> GATE that fails closed on any drift.

## 0. The F2 error record — binding authoring rules for every F3..F6 packet

Stage F2 landed green but cost three pre-execution packet corrections
(`e503144fd`, `5b218b345`, `981c603a3`), one corrected re-handoff
(`891096184`), one new fixture law (`dc4c9efef`), and one full correction
packet (F2-5, `64e95fb71`/`b5d794df4`). Each failure is now a rule. These
are BINDING for every packet in this stage and every later stage; a packet
that violates one is defective by definition and goes back to the architect.

1. **Drive-loop contract (F2-1 first correction).** Never drive an
   acceptance fixture through a full-engine loop (`fnExecute(lbl)`) when any
   stepped instruction can leave `currentStep` unmoved — native
   `executeOneStep(ITM_XEQ)` returns `-1` unconditionally and the
   synchronous Forth fallback leaves `currentStep` unchanged, so the outer
   loop repeats forever. Acceptance drives execute `executeOneStep` once
   per exercised instruction with explicit step positioning through
   structural handles (`tpStepAddr` on a captured handle — see rule 7).
2. **Machine-enumerated inventories (F2-1 second correction).** Any count a
   packet asserts ("four call sites", "four unbounded reads") must carry
   the exact grep the implementer re-runs at execution; a count mismatch is
   an immediate STOP, never an adaptation. The architect runs the same grep
   at authoring; a packet may not state an inventory from memory.
3. **Verbatim mutation anchors (F2-1 third correction).** Every mutation
   instruction quotes the exact line as it exists in the tree (e.g.
   `reallyRunFunction(op, regKStoC(opParam));`, not a paraphrase); the
   implementer greps the quoted line before editing and STOPs if absent.
4. **Deterministic oracles only (F2-2 correction).** No oracle may depend
   on ASan, a guard page, or any sanitizer/crash behavior. Bounds oracles
   use real sentinel bytes (`writeTestProgram`'s `0xFF 0xFF`, explicit
   padding bytes) or `FORTH_DEBUG_SELFTEST` debug counters.
5. **No self-comparison acceptance (F2-4 escape, closed by F2-5).** A
   parity or differential test whose two halves route through the SAME
   shared function cannot be the only oracle for that function: a mutation
   of the shared code moves both halves together and stays green. Every
   shared-core contract needs at least one INDEPENDENT semantic assertion
   (a direct call with a required outcome), and every required mutation
   must name a RED line that does not route through the mutated code on
   both sides of a comparison.
6. **Proven selection and seeded state (F2-5).** A runtime-discovery loop
   must independently PROVE its selection property (e.g. first-match:
   rescan `1..discoveredId-1` and fail on an earlier match — an assertion
   that survives deletion of the loop's `break`). A comparative subcase
   must reseed the complete observable input state before EACH half
   (`seedParamParityState` model: full 4-register seed + `lastErrorCode` +
   `programRunStop` + `dynamicMenuItem`, then assert the seed took).
7. **Structural program fixtures (dc4c9efef, mandatory preamble).** Every
   packet that authors or materially rewrites program fixtures carries the
   PROGRAM-FIXTURE AUTHORING RULE block from `QWEN_RUNBOOK.md` §4 verbatim,
   plus the fixture-lint item (no `beginOfProgramMemory + <literal>`, no
   numeric `tpStepAddr` argument) before its build gate.
8. **Capture-drive contract (F15-4).** Any fixture that types into a Forth
   region drives `runFunction(ITM_AIM)` FIRST with the cursor ON the
   opening marker and `pemCursorIsZerothStep` fixture-owned; only
   `insertStepInProgram`'s ALPHA arm consults `forthEntryStateAtInsertion()`
   after `addStepInProgram`'s pre-move.
9. **Mutations verified against the CURRENT tree (F1.5 sweep; three §8.9
   mutations were falsified in execution).** A stated mutation consequence
   is re-derived against the tree the packet will run on. A mutation whose
   RED depends on unverified handler behavior gets either a differential
   oracle or an escape valve ("if this stays green, STOP and report" —
   never a silent pass).
10. **Machine-verified literals; landed-order fixtures (F1-5 P0).** Every
    payload length is `printf '%s' "…" | wc -c`-verified at authoring;
    fixture setup order is re-derived against the LANDED semantics of every
    predecessor stage (esp. F1's pending-reset consumption: build
    interactive fixtures AFTER the lifetime signal they must survive).

Ahead-of-execution discipline (unchanged from F1/F2): each packet's
EXECUTION GATE greps the expected post-predecessor tree and STOPs on any
mismatch. If a predecessor lands with deviations, the successor is
re-authored by the architect, never adapted by Qwen.

## 1. The trace (§10.3 "traced, not inferred") — 2026-07-18, file:line evidence

Performed against the post-F2-5 tree (`b5d794df4`). All anchors verified live.

- **Native label step encoding:** a TAM alpha entry records
  `[opcode][kind][len][name]` where the kind byte is
  `tam.indirect ? INDIRECT_VARIABLE : tam.colon ? LOCAL_LABEL_VARIABLE :
  STRING_LABEL_VARIABLE` [VERIFIED: src/c47/programming/manage.c:1759 and
  the PEM path :1537]. `insertUserItemInProgram` (the Forth tam-hook's
  recorder) always writes `STRING_LABEL_VARIABLE` — global-kind names only
  [VERIFIED: packages/forth-core/programming/manage.c:1941-1970].
- **Kind bytes ARE the resolver selectors:** `findNamedLabel(name, type)`
  wraps `findNamedLabelWithDuplicate(name, 0, type)`; `LOCAL_LABELS`(249)
  scans only `labelList[]` entries with `.program == currentProgramNumber`
  and `step < 0`, matching the FIRST occurrence with
  `labelLocalStepNumber > currentLocalStepNumber`, else the first in the
  program — position-sensitive [VERIFIED:
  src/c47/programming/manage.c:1863-1903]; `GLOBAL_LABELS`(253) scans
  `step > 0` entries [:1905-1912]. Matching is `compareString(...,
  CMP_BINARY)`.
- **TAM colon entry:** `ITM_COLON` during TM_LABEL/TM_LBLONLY/TM_KEY (and
  TM_SOLVE outside PEM) sets `tam.colon` and shows `-MNU_TAMLOCALLABEL`
  [VERIFIED: src/c47/ui/tam.c:756-763]; resolution selects
  `tam.colon ? LOCAL_LABELS : GLOBAL_LABELS` [tam.c:937]; the display
  renders `:name:` vs `'name'` [tam.c:161-175, decode.c:183-213].
- **The package TAM hook is kind-faithful already:** the Forth fallback is
  gated `!tam.colon` [VERIFIED: packages/forth-core/ui/tam.c:978-992] — a
  local request never reaches Forth vocabulary (AUD-U1, landed).
- **ITM_XEQ item row:** `{ fnExecute, TM_LABEL, "XEQ", "XEQ", ..., CAT_FNCT
  | ... | PTP_LABEL | ... }` [VERIFIED: src/c47/items.c:1786]. Because
  `forthFindItem` filters `CAT_FNCT && PTP_NONE`
  [packages/forth-core/forth_dict.c:377-389], a bare `XEQ` token in Forth
  source resolves as NOTHING today (falls to the last-resort error) — the
  F3 structural `XEQ` parsing word collides with no live meaning.
- **The compile-state label seam F3 replaces:** label hit in compile state
  raises `ERROR_INVALID_NAME` + `abortDefinition()` [VERIFIED:
  packages/forth-core/forth_compile.c:472-477 — the documented §3.3.6
  interim]. Interpret state is the landed C-1 dispatch:
  `dynamicMenuItem = -1; fnExecute((uint16_t)label);` [forth_compile.c:499-500].
- **Reserved-token arm waiting for XEQN:** `vBodyWalk`'s final else returns
  false with the comment `0x7F05..0xFFFF reserved — F3 adds XEQN here`
  [VERIFIED: packages/forth-core/forth_dict.c:140-141].
- **Compile-only immediate prim pattern (RECURSE, the model for all F3
  words):** `FF_IMMEDIATE` entry, handler fails closed via
  `forthOpenDefinitionIndex` → `ERROR_OPERATION_UNDEFINED`
  [VERIFIED: packages/forth-core/forth_prims.c:23-35, 67]. The prim table
  is append-only; `PRIM_COUNT` currently 12.
- **Scan records (the scope key F3 reuses):** 8-byte records
  `[uint32 progOffset][uint16 prevOff][uint16 zero]` linked from
  `forthScanHead`, appended record-first with full rollback
  (here/latest/count/head all restored on error) [VERIFIED:
  packages/forth-core/forth_compile.c:53-108, 588-633].
- **Lifetime seams (landed F1, untouched by F3):** signal at `runProgram`
  entry gated `!nestedEngine`; consumption in `forthRunGenCheckReset`
  (clears fdict + scan state) deferred while `forthInnerIsActive()`
  [VERIFIED: forth_compile.c:46-51, 110-117; forth_inner.c:32].
- **Save/restore mechanism tolerates key removal:** parameters are
  name-keyed lines searched per name; an absent key prints "using default"
  and keeps the pre-seeded value; unknown keys in the file are never
  queried; upstream's own comment blesses removing parameters
  [VERIFIED: packages/forth-core/saveRestoreBackup.c:276-277, 579-595;
  forthDict keys at :530-534 (save) and :863-870 (restore)].
- **FCALL bridge:** `fnForthCall(uint16_t param)` calls
  `forthInner(param, programRunStop == PGM_RUNNING)`
  [VERIFIED: packages/forth-core/forth_bridge.c:11-14]; the PEM
  `insertStepInProgram` ITM_FCALL arm records names via
  `forthDictNameByIndex` (the double name-guard, F15-5).
- **Branch runtime already landed:** `FTOK_BR`/`FTOK_0BR` decode with
  bounded reads and a type-dispatched `popIsFalse()`
  [VERIFIED: packages/forth-core/forth_inner.c:319-348]; deltas are signed
  int16 CELLS relative to the cell after the delta field (§2.2), pinned by
  the Stage-1-B backward-loop test (`DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`).
- **Arena report line:** `FORTH ARENA: dict here=%u sizeBlocks=%u
  freeRamDelta=%ld` [VERIFIED: packages/forth-core/test_dict_reloc.c:10729].

## 2. The design pass (architect, 2026-07-18 — DECIDED, no open choices)

Owner rulings already in DESIGN §10.3 (2026-07-16/18): scopes fold, control
flow + IMMEDIATE fold, global scope with postfix `GLOBAL`, classic
`FORGET <name>`, same arena + split §5.4 report. Everything below settles
the named design-pass questions against the traced tree.

### D1. Two regions, one header layout

- `fdict` (existing) becomes the TRANSIENT dictionary: per-program scopes +
  the interactive scope. It is **no longer persisted** — it is a
  per-lifetime cache re-derived from source (§8.1/§8.2), so the landed F1
  reset mechanism (whole-region clear at the consumption seam) is
  UNCHANGED. Interactive words stop surviving power cycles; program words
  never needed to (fresh lifetime re-derives). Class-2 amendment at stage
  close.
- `gdict` (new) is the GLOBAL dictionary: a second `forthDict_t` control
  block + region, persisted under NEW save keys
  (`forthGDictBase`/`forthGDictSizeBlocks`/`forthGDictHere`/
  `forthGDictLatest`/`forthGDictCount`), validated on restore by the F1-5
  validator retargeted to gdict. The five old `forthDict*` keys are
  REMOVED from both sides (the mechanism tolerates this; old backups
  restore as empty gdict + empty fdict — pre-F3 transient words were
  per-lifetime anyway). gdict is cleared only by: restore-validation
  failure, the cold-init/reset seams that today call `forthDictInit`/
  `forthDictClear` from config.c, and `FORGET` truncation.
- ONE header layout for both regions (shared helpers stay single-path):

  ```c
  typedef struct {              // fixed prefix = 6 bytes
    uint16_t link;              // region-relative offset of previous header, or FORTH_NULL
    uint8_t  flags;             // FF_IMMEDIATE | FF_SMUDGE
    uint8_t  nameLen;           // 1..31
    uint16_t owner;             // fdict: scan-record offset of the owning
                                //   program's F1-3 record, or
                                //   FORTH_OWNER_INTERACTIVE; gdict:
                                //   FORTH_OWNER_GLOBAL
  } forthHeader_t;
  #define FORTH_OWNER_INTERACTIVE 0xFFFFu
  #define FORTH_OWNER_GLOBAL      0xFFFEu
  ```

  `bodyStart = ceil4(6 + nameLen)`. §5.4 cost formula becomes
  `cost(word) = ceil4(6 + nameLen) + 2*cells`; the arena baseline shifts
  and is re-recorded at the F3-1 gate.
- Owner key = the owning program's SCAN-RECORD OFFSET (uint16). The F1-3
  record is created before any of that program's definitions
  (record-first), dies with the region at every seam, and rolls back with
  the pre-scan — so owners can never dangle. Sentinels come from the top
  of the offset space (`here ≤ 0xFFFE` by the ensure guard, and record
  offsets are strictly below `here`).

### D2. Word references and the FTOK_CALL split

- A **word ref** (uint16) names a word in either region:
  bit 15 clear = transient index `0..0x5FFF`; bit 15 set = global index
  `ref & 0x7FFF`. `forthInner`'s entry parameter and `ITM_FCALL`'s param
  become refs — every existing caller passing a plain transient index
  remains valid (bit 15 clear).
- Token space split: `FTOK_CALL` `0x1000..0x6FFF` = transient index
  `tok - 0x1000`; `0x7000..0x7EFF` = global index `tok - 0x7000`
  (`FORTH_GCALL_BASE 0x7000`, capacity 0xF00 = 3840 global words, absurdly
  above the 2 KB arena ceiling). `startDefinition`'s count cap tightens
  from `0x6F00` to `0x6000`.
- Helpers: `forthTokenFromRef` / `forthRefFromToken` /
  `bodyOffsetOfRef` (region-dispatching), and the scoped lookup
  `bool forthFindColonRef(const char *name, uint16_t *ref, uint8_t *flags)`
  replacing `forthFindColon` everywhere (search: fdict newest-first
  filtered to `owner == forthCurrentScope`, then gdict newest-first;
  smudge skipped; flags out for immediacy).
- `forthDictNameByIndex` becomes `forthDictNameByRef` (same walk,
  region-dispatched) — the FCALL name-redirect guard keeps working for
  both regions.
- **Cross-region execution:** a transient body must be able to call a
  global body and return. The inner interpreter therefore carries a
  current-region flag alongside `ip`, and the return stack carries one
  region bit per saved ip (`static uint64_t rstackRegionBits` — one bit
  per `rstack[64]` slot, zero extra structure). FTOK_CALL pushes the
  current bit and switches to the callee's region from the ref; FTOK_EXIT
  restores the popped entry's bit; every token/operand read goes through
  `innerBase(curG)`/`boundedRead(curG, ...)` so region moves mid-execution
  stay safe (fresh reads, offsets-only). The F1-5 `INNER_LEAVE` watermark
  unwind needs no bit cleanup: bits above `rsp` are dead and every push
  overwrites its slot's bit.

### D3. Current scope

`static uint16_t forthCurrentScope = FORTH_OWNER_INTERACTIVE;` in
forth_compile.c, with a public getter `forthCurrentScopeGet()`.
Assignment sites (each packet quotes them exactly):
- `forthProgramStep`: save, set to the owning program's record offset (new
  helper `forthScanFindRecord(progStart, &recOff)` — the
  `forthScanIsRecorded` walk returning the offset;
  `forthScanIsRecorded` becomes a wrapper over it), restore on exit.
- `forthPreScanOwningProgram`: save, set to the just-created record's
  offset for the DEFS_ONLY loop, restore after.
- `fnForthOuter` / `forthOuterInterpret` DELIBERATELY do not touch it: a
  genuinely interactive invocation already sees `FORTH_OWNER_INTERACTIVE`
  (the default, and every program path restores on exit), while a nested
  FORTH-item evaluation from a program's tail INHERITS the program's
  scope — its definitions belong to the program whose execution created
  them (the `currentProgramNumber` mental model).
- `forthOuterRun` snapshots and restores it around every invocation (with
  the defState snapshot), so nested interprets restore the caller's scope
  — "nested entry selects the callee's scope, return restores the
  caller's" falls out of the program-step entry setting it.
Invariant: `forthCurrentScope != FORTH_OWNER_INTERACTIVE` only while
inside `forthProgramStep`/the pre-scan; tests pin the restored value.
`startDefinition` stamps `owner = forthCurrentScope` into every new fdict
header. Runtime XEQN fallback resolution (D6) reads the getter.

### D4. GLOBAL / IMMEDIATE / FORGET

- **Same-line tracker.** `static ftoken_t forthLatestClosedRef` (0 = none;
  reset at forthOuterRun entry per line, snapshot/restored with the ctx).
  Set to the just-closed word's ref-token at `finishDefinition` success
  (FULL and DEFS_ONLY modes), and in SKIP_DEFS by the `:`-consumer
  resolving the consumed name through `forthFindColonRef` after the `;`.
  Rationale: pre-scan compiles ALL of a program's definitions before any
  tail runs, so "fdict.latest" at tail time is the LAST definition of the
  whole program — a naked latest-entry mark would mark the wrong word.
  The tracker makes `GLOBAL`/`IMMEDIATE` mean "the definition closed
  earlier on THIS line", which is also the only placement that works in
  the two-pass program model. Both words ERROR when the tracker is 0
  (`ERROR_INVALID_NAME`): they must follow their definition on the same
  line — same-line discipline, symmetric with §8.1's single-line
  definitions.
- **Pre-scan applies marks.** In DEFS_ONLY mode the two tokens `GLOBAL`
  and `IMMEDIATE` EXECUTE (definition-completing markers) instead of being
  skipped; everything else in the tail stays skipped. In SKIP_DEFS they
  re-execute idempotently: the tracker then holds the already-moved
  (global) ref, and GLOBAL-on-a-global-ref is a no-op while
  IMMEDIATE-on-a-set-flag is idempotent. FORGET is program BEHAVIOR, not a
  definition marker: DEFS_ONLY consumes its name token and skips;
  SKIP_DEFS/FULL execute.
- **GLOBAL mechanics** (plain prim + tracker): requires tracker ≠ 0; a
  global-ref tracker → success no-op. A transient-ref tracker must be the
  LATEST fdict entry (invariant by construction; defensive STOP-class
  check). Walk the body once (token-wise, all emittable tokens): every
  FTOK_CALL must be global-space, EXCEPT the self-call token
  (`0x1000 + selfIdx`, from RECURSE) whose positions are collected for
  rewrite; any other transient-space call → `ERROR_INVALID_NAME` (a global
  word may only call global words — the discipline that keeps the
  persisted region closed under restore). Then: compute the entry's used
  extent (header + body through its FTOK_EXIT), `gdict`-ensure, copy
  wholesale, patch `link = gdict.latest`, `owner = FORTH_OWNER_GLOBAL`,
  rewrite collected self-calls to `FORTH_GCALL_BASE + gdict.count`, bump
  gdict here (block-rounded)/latest/count, then roll the transient copy
  back off fdict (`here = old latest`, `latest = old link`, `count--` —
  the abortDefinition shape) and update the tracker to the new global
  ref-token. Flags are preserved by the copy (IMMEDIATE before or after
  GLOBAL both work). Refuses while `forthInnerIsActive()`
  (`ERROR_OPERATION_UNDEFINED`).
- **IMMEDIATE** (plain prim + tracker): sets `FF_IMMEDIATE` on the
  tracker's header (either region). Compiler honor: `forthFindColonRef`'s
  flags out-param — an immediate colon word in compile state is EXECUTED
  (`forthInner(ref, ...)` + ASLIFT scrub + error gate) instead of emitted.
- **FORGET** is a STRUCTURAL parsing word (it consumes the next source
  token, which a prim cannot): recognized in `forthOuterRun` after
  `:`/`;`, before prim lookup (unshadowable, like `:`). Compile state →
  `ERROR_INVALID_NAME`, abort, stop (not compilable). DEFS_ONLY → consume
  name, skip. Execution: refuse while `forthInnerIsActive()`
  (`ERROR_OPERATION_UNDEFINED` — a nested FORTH-evaluation could otherwise
  truncate a body under an executing ip); resolve the name in GDICT ONLY
  (newest-first); miss → `ERROR_FUNCTION_NOT_FOUND` with the name in
  `errorMessage`; hit at entry offset E → `gdict.here = E`,
  `gdict.latest = header(E).link`, `gdict.count -= walked entries`.
  Safety: define-before-use + truncation-from-the-top ⇒ surviving global
  bodies only reference surviving indices. Transient bodies holding
  now-stale global refs fail at call time through the existing
  `bodyOffsetOfRef → FORTH_NULL → ERROR_INVALID_CORRUPTED_DATA` path —
  deterministic, already-tested error class.

### D5. Control-flow shapes (§3.3.9 stage-2 machinery)

Eight FF_IMMEDIATE prims (append-only), compile-only via the RECURSE
guard pattern (`!isDefinitionOpen()` → `ERROR_OPERATION_UNDEFINED`).
**`IF` emits a bare `FTOK_0BR` — no hidden DUP**: §2.2's branch-token
stack effects are normative (`FTOK_0BR` CONSUMES its operand; the
Stage-1-B loop test DUPs explicitly when the value must survive), and
standard Forth `IF` consumes the flag. Compile-time control stack in
forth_compile.c (offsets only, §3.3.7 — the region may move on any emit):

```c
#define FORTH_CSTACK_DEPTH 8
typedef struct { uint16_t pos; uint8_t kind; } forthCtl_t;  // kind: CTL_ORIG / CTL_DEST
static forthCtl_t forthCstack[FORTH_CSTACK_DEPTH];
static uint8_t forthCsp;    // reset on startDefinition success; must be 0 at ';'
```

Shapes (delta = int16 CELLS relative to the cell after the delta field;
`P` = `fdict.here` captured AFTER emitting the branch token, i.e. the
offset of the delta cell; patching reads `fdict.base` fresh at write time):

- `IF`: emit `FTOK_0BR`; P; emit placeholder 0; push {P, ORIG}.
- `THEN`: pop ORIG (else pairing error); patch `*(int16*)(base+P) =
  (here - (P + 2)) / 2`.
- `ELSE`: pop ORIG1; emit `FTOK_BR`; P2; emit 0; patch ORIG1 to `here`;
  push {P2, ORIG}.
- `BEGIN`: push {here, DEST}.
- `UNTIL`: pop DEST; emit `FTOK_0BR`; P; emit `(int16)((dest - (P + 2)) / 2)`.
- `AGAIN`: pop DEST; emit `FTOK_BR`; delta as UNTIL.
- `WHILE`: top must be DEST; emit `FTOK_0BR`; P; emit 0; push {P, ORIG};
  swap the top two entries (ORIG under DEST — ANS pairing).
- `REPEAT`: pop DEST, pop ORIG; emit `FTOK_BR` back to DEST (delta as
  UNTIL); patch ORIG to `here`.

Errors: pairing mismatch/underflow → `ERROR_INVALID_NAME`, abort, stop.
Overflow (`csp == FORTH_CSTACK_DEPTH`) → `ERROR_RAM_FULL`. `;` with
`csp != 0` → `ERROR_INVALID_NAME` + abort (checked before
`finishDefinition`). `forthCsp` lives entirely in forth_compile.c: reset
in the `:` handler after `startDefinition` succeeds; abort paths need no
reset beyond that (next `:` re-zeros). Delta magnitudes are line-bounded
(≤255-byte source) — no range check; the F1-5 validator's branch-target
walk remains the backstop.

### D6. XEQ source forms + FTOK_XEQN (B2/B4, R6_RESOLUTION_PLAN §2)

- **`XEQ` is a STRUCTURAL parsing word** (recognized with `:`/`;`/`FORGET`
  before all lookup, unshadowable — the B2 escape hatch must be
  deterministic). It consumes the next token. Accepted forms are exactly
  the canonical listing spellings: `'NAME'` (0x27 first AND last byte,
  total token length ≥ 3) → kind 253; `:NAME:` (0x3A first and last,
  length ≥ 3) → kind 249. The name is the raw bytes between the delimiters
  (no interior scanning; C47 glyphs pass through; a name containing 0x20
  is inexpressible in Forth source — token-bounded — and stays reachable
  from RPN steps; documented limitation). Name length 1..31
  (`FORTH_NAME_MAX`) else `ERROR_INVALID_NAME`. Anything else after `XEQ`
  (bare name, number, missing token) → `ERROR_INVALID_NAME`, atomic
  (abort open definition, stop line) — B3 syntax-error class.
- Compile state: emit `FTOK_XEQN` + inline `[kind][len][name][pad]`
  (`inline = 2 + len`, `padded = (inline + 1) & ~1` — pad byte present iff
  len is odd, written 0), via a local byte buffer through
  `forthDictEmitBytes`. Interpret state: resolve and dispatch IMMEDIATELY
  through the same shared helper the runtime arm uses (below).
  DEFS_ONLY: `XEQ` in a tail is interpret-state → skipped (but the form's
  syntax is NOT checked at pre-scan — execution checks it; F5's commit
  check adds the entry-time tier).
- **Shared dispatch helper** (forth_inner.c, public):

  ```c
  typedef enum { FORTH_XEQN_DONE, FORTH_XEQN_COLON, FORTH_XEQN_ERR } forthXeqnResult_t;
  forthXeqnResult_t forthXeqnDispatch(const char *name, uint8_t kind, uint16_t *colonRef);
  ```

  Resolution order (B4 + §2.2): `findNamedLabel(name, kind)` — the kind
  byte passes VERBATIM (position-sensitivity inherited from
  `currentProgramNumber`/`currentLocalStepNumber`, manage.c:1869-1903).
  Hit → `dynamicMenuItem = -1; fnExecute(label);` (the landed C-1 shape —
  NEVER the PGM_RUNNING wrap; program-context calls take fnExecute's
  nested continuation branch unchanged) → DONE. Miss with kind 249 →
  `ERROR_LABEL_NOT_FOUND` → ERR (kind-faithful, NO fallback). Miss with
  kind 253 → fallback chain: prim by name (`forthFindPrim`; dispatch fn +
  ASLIFT scrub) → DONE; colon via `forthFindColonRef` (current scope, then
  global) → return COLON + ref (the CALLER dispatches: the runtime arm
  pushes rstack and jumps like FTOK_CALL; the interpret-state caller runs
  `forthInner(ref, programRunStop == PGM_RUNNING)`); item
  (`CAT_FNCT && PTP_NONE` scan — B3 tightens the old CAT_FNCT-only arm) →
  `reallyRunFunction(item, NOPARAM)` under the PGM_RUNNING wrap (§2.2 item
  arm is wrapped, label arm is not) → DONE; else `ERROR_LABEL_NOT_FOUND`
  → ERR.
- **Runtime `FTOK_XEQN` arm** (forth_inner.c): bounded-read the kind/len
  cell; kind ∉ {253, 249} or len ∉ 1..31 → `ERROR_INVALID_CORRUPTED_DATA`;
  bounded-read `padded - 2` remaining bytes; copy name NUL-terminated to a
  local `char[32]`; `ip += padded`; call the shared helper; COLON → rsp
  guard + `rstack[rsp++] = ip; ip = bodyOffsetOfRef(ref)` (FORTH_NULL →
  corrupted-data error), i.e. exactly the FTOK_CALL dispatch shape.
- **Validator XEQN arm** (vBodyWalk, replacing the reserved-reject for
  0x7F05 only): kind ∈ {253, 249}; len 1..31; extent bounded; if len is
  even (inline `2+len` even → no pad) advance `padded = inline`, else
  check the trailing pad byte is 0; `pos += padded`. 0x7F06.. stays
  reserved-reject.
- **B3 lands both directions:** forward — a bare token matching a
  PARAMETERIZED `CAT_FNCT` item name (found by a new
  `forthFindItemAnyPtp` check after step 4 misses) → atomic syntax error
  `ERROR_INVALID_NAME` (parameter required; F4 adds the parameter
  grammar); reverse — `forthResolveXEQ`'s item arm gains the
  `PTP_NONE` filter, and the `test_xeq_item_lookup` FORTH/FCALL rows
  migrate to pin the reject.

### D7. What F3 does NOT change

The F1 lifecycle (signal/consumption sites, active-frame deferral), the F2
param core, byte grammars, the §8 entry layer, save-format mechanism
(name-keyed lines), `freeList.c`, upstream `src/`. DESIGN.md/HISTORY
reconciliation (emit-scope prose, §3.3.9, §8.10 item 1, §5.4 formula,
persistence amendment) is an ARCHITECT docs-only pass at stage close —
packets never edit design docs.

## 3. Packets — status and dependency order

| Task | Packet | Status | Dependency |
|---|---|---|---|
| F3-1 owner-tagged headers (layout 4→6) | `QWEN_PROMPTS_F3_1_owner_headers.md` | READY | F2-5 committed green (`b5d794df4`) |
| F3-2 global region, refs, persistence swap, validator retarget | `QWEN_PROMPTS_F3_2_global_region.md` | AUTHORED, gate-locked | F3-1 committed green |
| F3-3 scopes live (current-scope tracking + filtered lookup) | `QWEN_PROMPTS_F3_3_scopes_live.md` | AUTHORED, gate-locked | F3-2 committed green |
| F3-4 GLOBAL / IMMEDIATE / FORGET + tracker | `QWEN_PROMPTS_F3_4_marks.md` | AUTHORED, gate-locked | F3-3 committed green |
| F3-5 control-flow words | `QWEN_PROMPTS_F3_5_control_flow.md` | AUTHORED, gate-locked | F3-4 committed green |
| F3-6 XEQ forms + FTOK_XEQN + B3 | `QWEN_PROMPTS_F3_6_xeqn.md` | AUTHORED, gate-locked | F3-5 committed green |
| F3-7 §2.3 acceptance suite + stage sweep | `QWEN_PROMPTS_F3_7_acceptance.md` | AUTHORED, gate-locked | F3-6 committed green |

Execute strictly in order, one packet per session, clean green tree each,
per-packet `/tmp/forth-f3-N-*` paths. After each stage commit the operator
runs the successor's EXECUTION GATE; any mismatch returns to the
architect. RULE-1 flash deltas: F3-2 (new region + save keys), F3-5, and
F3-6 are the flash-visible packets; record `make dmcp5r47` deltas in those
stage commits. Arena: the report line is extended in F3-2
(`FORTH ARENA: dict here=.. sizeBlocks=.. gdict here=.. gsizeBlocks=..
freeRamDelta=..`) — quote it in every commit from F3-2 on; the ≤2 KB §5.4
ceiling now covers the SUM of both regions.

Stage close (architect, docs-only): DESIGN reconciliation per D7, ledger
closeout, §8.9-adjacent notes if any acceptance wording shifted.
