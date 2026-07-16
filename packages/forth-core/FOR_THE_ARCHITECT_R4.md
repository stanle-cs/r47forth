# R4 — Forth engine and unwritten-series design review

## Bottom line

The R4 decision interview is complete. B, C, and D now have an architectural
direction, recorded below. The existing four Qwen tasks remain intentionally
limited to defects in the engine as it stands; the accepted future work still
needs new, independently mutation-verified implementation prompts before it is
given to Qwen.

The existing engine has four bounded implementation defects; they are specified
as executable Qwen tasks in `QWEN_PROMPTS_R4_engine.md`. The original review
found these contradictions in the unwritten series; the accepted architecture
below resolves their product-level choices:

- B can invalidate the dictionary while `forthInner` is still executing from
  it.
- B promises an `XEQ 'NAME'` collision escape which its own lookup/PTP rules
  make unreachable.
- One factored `forthFindItem` cannot have both the proposed forward-lookup
  filter and `forthResolveXEQ`'s current reverse-lookup behavior.
- C names source forms but does not specify a parser or its errors, and its
  `PTP_NONE` lookup filter excludes the parameterised items C is meant to add.
- D's per-commit resolve check rejects the forward-reference program shape the
  pre-scan was built to support, unless validation becomes a transactional
  whole-program operation.

I executed eight temporary regression probes through the sanctioned gate. All
eight went red for the predicted reason. I then removed every probe with an
inverse patch. The evidence is recorded below; no production fix was made.

## Accepted R4 architecture

This section is normative for the findings later in this memo. It records the
owner's answers; it does not claim that the current code implements them. No R4
product decision remains open. Details which can be derived by tracing native
RPN behavior are implementation research, not new product questions.

### Execution lifetime and invalidation

1. A C47 program started from an active Forth interpreter frame is nested in
   that active Forth generation. It must not clear or replace the dictionary
   from which the suspended frame is executing. A pending top-level reset waits
   until no active Forth frame can be invalidated.
2. Replace correctness based on equality of two wrapping 16-bit generation
   counters with a Forth-private pending-reset event/flag. A true top-level RPN
   run marks it; the first safe Forth entry consumes it. A nested RPN launch
   from active Forth does not start a new Forth generation. A counter may remain
   for diagnostics, but not as the truth predicate.
3. Every PEM single-step is also a fresh Forth generation. The current hook
   excludes `singleStep`; that exclusion must change for the Forth-private
   invalidation signal.
4. This does not alter RPN mode. RPN keeps its existing run/step state and has
   no Forth dictionary to clear. The new flag is observed only on entry to
   Forth, and the active-Forth guard only distinguishes a nested RPN call made
   by Forth from a genuine top-level RPN start. That is an extension of RPN's
   existing execution contexts, not a new RPN unwind model.
5. Ordinary outer-interpretation re-entry while a colon definition is open is
   not a supported user path today. Add no user-visible prohibition or flash
   cost now. Document the internal precondition and revisit it only if a future
   `EVALUATE`-like or immediate source primitive makes the path reachable. This
   is unrelated to runtime recursion.

### Vocabulary, XEQ, and scopes

1. Bare Forth source retains §4.1's order: primitive → same-scope colon →
   number → calculator item → RPN label. Stable primitive, colon, and item ids
   are baked into compact tokens. A named RPN label call uses `FTOK_XEQN` because
   label ids can renumber after an edit.
2. Forth has an explicit `XEQ 'name'` source form. It first requests the native
   RPN label meaning of the quoted name; if no label exists, it resolves an
   ordinary callable Forth target: primitive → same-scope colon → calculator
   item. A number is not callable. Interpretation and compilation must share
   that rule; compiled label calls use `FTOK_XEQN`.
3. Runtime name storage is not a label-only principle. Named RPN parameters
   whose native ids can change also retain their canonical quoted names and use
   native RPN resolution/create/error behavior at execution. Stable target ids
   remain early-bound.
4. Forth-to-RPN label XEQ must follow existing RPN XEQ behavior exactly for
   program-running state, return, pause/stop, and errors. XEQ is the sole RPN
   control-flow bridge admitted in Forth. GTO, RTN, STOP, BACK, SKIP, CASE, and
   other RPN control/declarative program steps are rejected during Forth
   parsing/compilation with `ERROR_OPERATION_UNDEFINED`; no token is emitted.
   Restored dictionaries must reject such encodings too.
5. **Derived consequence, not a new decision:** RPN-side XEQ keeps its
   established native resolution first. Only after the native label/item path
   fails may it prepare the current program's Forth scope and resolve a colon
   there. Interactive RPN uses the interactive Forth scope. It never searches
   another program's Forth words.
6. Colon definitions are local to their owning RPN program. Nested entry selects
   the callee's local scope and return restores the caller's scope. Definitions
   may coexist in one managed arena/generation, and already-compiled colon calls
   remain valid, but a name lookup cannot see a different owner's definitions.
   Primitives, calculator items, and native RPN labels retain their established
   global visibility.
7. Interactive definitions occupy one reserved interactive-local scope. A
   top-level RPN generation reset clears that scope too. There is no global
   Forth-word declaration in this series; global Forth words are explicitly
   deferred. The current single global colon chain does **not** yet honor this
   accepted local-scope rule.
8. Add the standard compile-only immediate word `RECURSE`. Keep the definition
   under construction smudged until `;`; `RECURSE` emits a call to that current
   definition without making its name generally visible. Thus recursive words
   are supported without changing normal lookup semantics.

### Parameterised calculator items

1. Series C covers all native RPN parameter types used by eligible non-flow
   items, not only `PTP_REGISTER`. Item eligibility still excludes the
   control/declarative operations listed above, with XEQ handled specially.
   Forth uses operation-first canonical RPN display spelling (`STO 05`, not
   stack-first `5 STO`) and invents no aliases. Quoted named parameters, for
   example `STO 'RATE'`, are parsed as source text; they do not open a nested
   alpha UI.
2. Parameter syntax and execution must reuse a factored, bounded native RPN
   semantic decoder. Add a Forth textual front end; do not fabricate a program
   step, prime `tam`/catalog state, or call `executeOneStep` as a simulation.
   Both native step execution and `FTOK_C47` should feed the same parameter
   decoder and end at `reallyRunFunction`, so Forth cannot drift from RPN
   register conversion, label/name handling, or errors.
3. The upstream scan found no existing pure API at that boundary:
   `_executeOp` is file-static in `programming/lblGtoXeq.c`;
   `executeOneStep` owns program traversal/state; `decodeOp` is display-only;
   and the current string-name reader is bounded by the live program buffer.
   Factoring the clean boundary may add upstream override files; that cost is
   accepted. A generalized reader must take explicit start/end bounds.
4. A typed item with a missing or malformed parameter errors atomically and
   opens no UI. Existing RPN editor catalogs remain unchanged. Exact accepted
   spellings, ranges, create semantics, and errors come from tracing the native
   RPN type paths; Qwen must not infer them from examples.

### Entry validation, restore safety, and ownership

1. Series D is lexical and structural validation on commit, not full name
   resolution. Syntactically valid unresolved names remain legal so forward
   program references continue to work. Final resolution remains the job of
   pre-scan/run.
2. Check-only validation executes nothing and mutates no stack, catalog,
   program, or dictionary allocation. Malformed numbers, quotes, parameter
   syntax, or colon structure reject the commit atomically and leave the prior
   step unchanged.
3. Replace the fixed eight-program pre-scan registry with compact, dynamic,
   capacity-bounded tracking in the managed dictionary arena. It fails only at
   ordinary dictionary capacity, not at an arbitrary number of programs. Any
   implementation must report the arena high-water mark.
4. Validate a restored dictionary fully once instead of adding bounds checks to
   every token dispatch. Validate header/name extents, body and cell alignment,
   token and operand extents, colon indices, XEQN length/padding, reserved token
   ranges, legal control targets, and termination. If invalid, clear only the
   Forth dictionary, preserve the owner's RPN save, and rebuild program
   definitions from authoritative source. A modest flash increase is accepted
   when it materially simplifies this validator; RAM remains the binding cost.
5. `forthOwningProgramStart` must explicitly compute the greatest program start
   not greater than the queried pointer. It must not depend on undocumented
   `programList` ordering. This mirrors the firmware's existing explicit
   min/max practice.

### Forth capture is a future PEM-shaped submode

1. Forth capture becomes a distinct PEM-style submode, not a wrapper around the
   alpha state machine. For keys, catalogs, parameter entry, cancel, cursor,
   softmenus, and alpha transitions it follows the real PEM paths. The sole
   semantic difference is the sink: PEM inserts an RPN instruction; Forth
   capture appends canonical source text or commits a Forth source step.
2. Its source buffer is a managed allocation held only while Forth capture is
   active. Nested ordinary alpha capture suspends and later restores the Forth
   source/cursor/insertion/softmenu state. Use a relocation-safe handle rather
   than retaining a raw pointer across allocator calls. Success appends the
   canonical result; cancel/error restores the exact prior buffer; the outer
   commit/cancel releases it.
3. These UI choices are accepted but are outside the R4 engine prompt set. A
   later keyboard/PEM audit must trace the reachable native paths and supply
   hardware-derived tests. Current physical entry of every named-register glyph
   remains unverified; this memo does not turn that hypothesis into a fact.

### Encoding decisions which are already closed

`FTOK_XEQN` is `0x7F05`; `0x7F06..0x7FFF` stays reserved. For a name length
`len`, `inline = 1 + len`, `padded = (inline + 1) & ~1`, and the complete token
uses `2 + padded` bytes. The pad exists only for even `len`. This allocation is
consistent with the current token map.

## Findings which the accepted B decisions resolve

### B1. `FTOK_XEQN` can free the body that is currently executing

**Claim.** DESIGN §3.3.6 says a compiled label call emits `FTOK_XEQN`, resolves
the name at execution, then calls the label with direct `fnExecute`. It also says
an interactive label start fires run-generation bump site A. Colon indices are
only stable within a dictionary generation.

**What the current code does.** `forthRunGenCheckReset` in
`forth_compile.c` clears the dictionary on the first nested Forth program step
after a generation bump. `forthInner` keeps only a region-relative `ip`; it does
not pin or retain the allocation.

**Concrete future path.** With B implemented exactly as written:

1. An interactive line defines and invokes `: CALLP P ; CALLP`, where `P` is a
   C47 label whose program contains an `ITM_FORTH` source step.
2. `CALLP` is running inside `forthInner`; its `ip` points into `fdict.base`.
3. `FTOK_XEQN P` resolves the label and direct `fnExecute(P)` starts a C47 run,
   bumping `forthRunGeneration`.
4. The called program reaches its Forth step. `forthProgramStep` calls
   `forthRunGenCheckReset`, which calls `forthDictClear`.
5. The nested program eventually returns to the suspended `forthInner`, which
   resumes with an offset into the freed dictionary.

This path is not executable today only because `FTOK_XEQN` emission/decoding is
not implemented. It follows directly from the proposed path plus the existing
reset code; it is not a claim that the current build already executes XEQN.

**Cost to the owner.** A compiled word calling an ordinary keystroke program can
reboot or run wrong tokens if that program itself contains a Forth step.

**Accepted ruling.** Treat a program started from an active Forth frame as part
of the same active Forth generation and defer any pending top-level reset until
it is safe. Never clear the allocation under a live inner frame. Merely widening
the generation counter would not satisfy this lifetime rule.

### B2. The promised collision escape is unreachable

**Claim.** DESIGN §4.1 says items resolve before labels, so a label named `SIN`
is not reachable by bare name, but says the escape hatch `XEQ 'NAME'` reaches
the program “in either state.”

**Contradiction.** The same design defines `forthFindItem` as accepting only
`CAT_FNCT + PTP_NONE`, while §2.2 and §4.4 explicitly exclude `PTP_LABEL` from
`FTOK_C47`. No separate outer-interpreter rule recognizes the source phrase
`XEQ 'SIN'`. Therefore token `XEQ` cannot reach a parser which consumes the
quoted label; the alleged escape hatch does not exist inside Forth.

**Cost to the owner.** Once item-before-label precedence lands, a colliding C47
program becomes uncallable from Forth even though the design promises otherwise.

**Accepted ruling.** Add the explicit Forth source phrase `XEQ 'name'`. It asks
for the native RPN label meaning first, then falls back to an ordinary callable
Forth target under the accepted scope/order rule. Interpretation and compilation
share the rule; a compiled label target uses `FTOK_XEQN`. This is parser syntax,
not generic `PTP_LABEL` item handling.

### B3. Factoring `forthFindItem` changes a current reverse-lookup contract

**Claim.** B calls for `forthFindItem` to be factored out of
`forthResolveXEQ`. DESIGN §4.1 specifies its filter as:

```c
(status & CAT_STATUS) == CAT_FNCT &&
(status & PTP_STATUS) == PTP_NONE
```

**What the current code does.** The item loop in `forthResolveXEQ`
(`forth_dict.c`, anchor `/* C47 item name second`) filters only `CAT_FNCT`; it
does not require `PTP_NONE`.

Replacing that loop with the proposed helper literally changes which item names
the C47-side XEQ resolver accepts. Widening the helper instead makes the §4.1
“safety boundary” false and forces the outer interpreter to parse params before
C has defined how.

**Accepted ruling.** Do not make one unqualified helper silently serve both
contracts. Forward Forth lookup admits the native parameter types implemented by
Series C and requires the canonical parameter source to follow. RPN-side XEQ
keeps its established native resolver first and adds only the current-scope
colon fallback. A bare parameterised item with no parameter is an atomic syntax
error; it does not open a catalog or manufacture a default.

### B4. The XEQN dispatch protocol contradicts itself

**Claims.** DESIGN §2.2 says XEQN dispatches label/item/colon “under the same
PGM_RUNNING protocol as the FTOK_C47 arm.” Later in the same section it says the
wrap is normative for XEQN's item/colon arms and *wrong* for its label arm.
§3.3.6 correctly uses direct `fnExecute` for a label.

The label exception is the right direction, but the runtime matrix is still not
fully specified:

- **item:** `reallyRunFunction` needs the save/set/conditional-restore wrap;
- **label:** `dynamicMenuItem = -1; fnExecute(label)` must not use that wrap;
- **colon:** forcing the whole nested colon call to PGM_RUNNING is not justified
  by the GTK item-dispatch argument. A normal interpreted colon call passes the
  existing program context into `forthInner`. Forcing RUNNING can make a label
  inside that colon take continuation semantics even when no enclosing
  `runProgram` loop exists.

**Accepted ruling.** The label arm must use the native RPN XEQ path and inherit
its program-running, return, pause/stop, and error behavior. Calculator-item
dispatch continues through the shared native semantic decoder and
`reallyRunFunction`. A colon call uses the current Forth execution context and
same active generation; it must not synthesize `PGM_RUNNING`. The later
implementation prompt must spell this as exact pseudocode after the native path
has been traced.

### B5. XEQN's alignment prose contains incompatible arithmetic

**Claim.** Inline bytes are `1 + len`, padded only to a whole two-byte cell.
That rule is consistent.

**Contradictory example.** DESIGN §2.2 says the 31-byte worst case is
`2 + 1 + 31 + 1 = 34`. That expression sums to 35, and a 31-byte name needs no
pad: `1 + 31` is already even.

**Exact correction.** State one formula and examples:

```text
inline = 1 + len
padded = (inline + 1) & ~1
total  = 2 + padded
```

Thus len 1 totals 4 bytes, len 2 totals 6, and len 31 totals 34. A pad byte is
written only when `len` is even.

### B6. Token-space allocation itself is clean

I found no collision in the proposed token map. Current decode ranges stop
colon calls at `0x7EFF`; `0x7F00..0x7F04` are existing extended tokens;
`0x7F05` is free; and `0x7F06..0x7FFF` remains reserved. The count cap prevents
a colon index from encoding as `0x7F00`. With the corrected length formula,
XEQN inline data can remain cell-aligned.

## Findings which the accepted C decisions resolve

### C1. The lookup filter excludes the feature

§4.1 defines item lookup as `PTP_NONE` only. §4.4 Phase 2 says the same path
adds `PTP_REGISTER`, `PTP_NUMBER_8`, and `PTP_NUMBER_16`. Unless the lookup
policy is explicitly widened by phase, `STO` never reaches the parser described
for `STO 05`.

**Accepted ruling.** Series C supports every native RPN parameter type used by
eligible non-flow items, using the operation-first canonical display spelling
and a shared bounded semantic decoder. This forward-parser policy remains
separate from RPN-side XEQ's native resolver contract.

### C2. Source forms are examples, not an implementable grammar

The table names `item nn`, `item nnnn`, `STO 05`, `STO .05`, and `STO X`, but
does not answer the decisions an implementer immediately faces:

- decimal-only or other C47 bases;
- leading sign and leading-zero rules for NUMBER_8/16;
- exact range failures and their error codes;
- how `05`, `.05`, and `X` become a one-byte KS code;
- case sensitivity and the accepted register-name set;
- missing-param and extra-token behavior;
- whether invalid compile-state params abort the open definition;
- whether generic `PTP_NUMBER_16` exposes `FCALL n` in Forth source;
- how the promised `XEQ 'NAME'` parsing word relates to the explicitly excluded
  `PTP_LABEL` class.

“Mirror the VM” specifies decoding after a byte already exists; it does not
specify how source text produces that byte. The architectural choice is now
closed: trace and accept the native RPN spelling, range, create, and error rules
for every parameter type. A future C prompt must carry the resulting exact
grammar and error table; Qwen must not fill them in from the examples.

### C3. The decoder delta is smaller than the phasing text says

Current `forth_inner.c` already decodes `PTP_NUMBER_8` with a two-byte advance
and `PTP_NUMBER_16` with a two-byte advance. The actual inner-interpreter
widening for C is `PTP_REGISTER`; the larger missing work is forward lookup,
source parsing, and emission.

The proposed register layout itself is cell-consistent: one KS byte plus one
zero pad, then convert and range-check before dispatch. The exact error on a
failed `regInRange` is inherited from the traced native RPN path, and
`regKStoC` must be evaluated once rather than twice in prose/pseudocode.

## Findings which the accepted D decisions resolve

### D1. Per-line resolve-on-commit rejects supported forward references

The only supplied D requirement is “check-only tokenize+resolve on commit.” A
single-line implementation contradicts the current pre-scan architecture.

Concrete supported program shape:

```text
step 1: LATER 1 +
step 2: : LATER 41 ;
```

The pre-scan exists so step 1's tail resolves after definitions from later
steps are compiled. When step 1 is committed, step 2 may not exist yet; a full
resolve check must reject it. Even if both steps already exist, resolving only
the edited line cannot reproduce the owning-program pre-scan.

The same issue occurs within one source line: `: A 1 ; A` requires a provisional
definition to resolve its tail.

### D2. Using the live dictionary makes validation history-dependent

At PEM entry time, `fdict` may contain interactive definitions, definitions
from another program touched earlier in the generation, or nothing. Calling the
current resolver against that dictionary makes the same source line pass or
fail depending on what the owner ran beforehand.

### D3. “Check-only” needs a real side-effect contract

The current outer modes are FULL, DEFS_ONLY, and SKIP_DEFS. Resolution is
interleaved with effects: primitive execution, stack pushes, dictionary emits,
label execution, and future item dispatch. Adding a boolean named `checkOnly`
without specifying each branch invites exactly the simulation error this review
was asked to prevent.

**Accepted ruling.** D is lexical and structural validation only. It validates
token lengths, quotes, number and parameter grammar, and colon structure while
allowing unresolved names. It executes nothing, allocates nothing, and mutates
no live state. Failure rejects the tentative edit atomically and preserves the
previous step. Whole-program pre-scan/run remains responsible for final
resolution.

## Current-engine findings with accepted rulings

### E1. The ninth scanned program is not a bounded exception

**Code claim.** `forth_compile.c`, anchor `List full: program scanned but
unrecorded`, says re-scan/recompile is a “Bounded, documented” exception and
that eight distinct Forth-bearing programs is beyond a realistic session.

**Executed evidence.** I built nine real programs through `writeTestProgram`,
touched the first eight to fill `forthScannedProgs`, then touched the ninth
defining program twice. `fdict.count` was 1 after the first touch and 2 after the
second, with error 0. Every later touch adds another definition; the behavior is
unbounded until RAM full.

**Cost to the owner.** Re-running the ninth Forth-bearing program consumes
dictionary RAM even when no program was edited.

**Accepted ruling.** Replace the fixed array with compact dynamic tracking in
the managed dictionary arena. Capacity failure is ordinary dictionary
exhaustion, never an arbitrary program-count cliff. Report the arena high-water
mark in the implementation task. Increasing `FORTH_SCAN_MAX` merely moves the
defect and is not the accepted fix.

### E2. The 16-bit generation comparison aliases after 65,536 bumps

**Executed evidence.** I scanned a program defining `GA`, changed its source
name in place to `GB`, called `forthRunGenBump()` 65,536 times, then touched the
program again. The next touch retained `GA` and did not build `GB`, with error
0. `forthRunGeneration` had wrapped back to `forthResetGeneration`, so equality
falsely meant “current.”

**Cost to the owner.** On a long-lived calculator, enough edits/run starts
without sampling at a Forth step can reuse stale definitions and stale scanned
program pointers.

**Accepted ruling.** A 32-bit counter makes the case rarer and costs BSS but
does not solve lifetime or B1. Use a Forth-private pending-reset event/flag plus
an active-frame lifetime guard; any numeric counter is diagnostic only.

### E3. The outer definition snapshot does not prevent nested abort

**Design claim.** §3.3.2 says save/restore means a nested line can never close or
abort the outer line's definition.

**What the current code does.** Nested error paths call
`if (isDefinitionOpen()) abortDefinition()`. That observes the outer global
`openDef`. The epilogue then restores `openDef.open`, but it does not restore the
dictionary mutation the nested abort already performed.

**Executed evidence.** I started definition `OUT` (expected
`here/latest/count = 8/0/1`), then invoked an undefined nested line. It returned
with `openDef.open == 1` but `here/latest/count = 0/65535/0`: an apparently open
definition whose dictionary entry had been erased.

**Accepted ruling.** Natural current primitives do not create this path while
compiling, so I have not classified it as a present keypad failure. Do not spend
production bytes on a user-visible restriction today. Document the internal
precondition that a nested outer interpretation begins with no open definition,
and revisit with a real isolated transaction only if a future
`EVALUATE`-like/immediate source word makes the path reachable. Runtime recursion
is separately supported by the standard compile-only immediate `RECURSE` word.

### E4. Restored token bodies are trusted more than the safety claim admits

`forthDictValidateRestored` validates scalar bounds and header links, but not
header padding, body boundaries, token legality, inline-data extents, branch
targets, or a terminating EXIT. `forthInner` then reads tokens and LIT/ILIT data
without comparing `ip` with `fdict.here` or an entry end.

This is static evidence, not an executed OOB probe; I deliberately did not turn
a routine review into an intentional invalid read.

**Accepted ruling.** Use one full restore-time validation, not per-token
production bounds checks. An invalid Forth dictionary is cleared and rebuilt
from preserved RPN program source. The validator must cover the extents and
encodings listed in the accepted architecture above.

### E5. Owning-program lookup relies on an unstated ordering invariant

`forthOwningProgramStart` claims to return the largest program start `<= ptr`,
but assigns every qualifying entry and therefore returns the *last* qualifying
entry. It is correct only if `programList` is sorted by address.

**Accepted ruling.** Compute the maximum explicitly, matching the firmware's
existing min/max practice; do not rely on or prove list ordering.

## Bounded current-code fixes delegated to Qwen

These do not require a new design decision:

1. **Number grammar:** `1e2-3` produced error 0 instead of
   `ERROR_FUNCTION_NOT_FOUND`; the exponent sign is accepted anywhere after E.
2. **Capacity arithmetic:** Ensure(6) returned true with four bytes allocated;
   Allocate(31, 0xFFF0) wrapped and returned offset 0 with error 0.
3. **Restore name extent:** a header whose name crossed logical `here` survived
   validation.
4. **Failed pre-scan rollback:** the two-step `: G 1 ;` / `: B NOPE ;` program
   left counts 1 then 2 across two failed touches.

Each task in `QWEN_PROMPTS_R4_engine.md` contains the exact test, fix, and an
executed mutation symptom. No current task implements B, C, D, scoping,
generation redesign, `RECURSE`, or capture-mode work.

## Boundary and order for future implementation prompts

The accepted decisions are not permission to give Qwen an underspecified
mega-task. New prompt work should be split and mutation-proved in this order:

1. engine lifetime foundations: local owner scopes, pending-reset/active-frame
   rules including PEM single-step, dynamic pre-scan tracking, explicit owning
   maximum, `RECURSE`, and restore-time validation;
2. the bounded shared RPN parameter/dispatch semantic core, after tracing every
   supported native PTP path and its exact errors;
3. vocabulary and XEQ parsing/emission/runtime dispatch, including `FTOK_XEQN`
   and RPN-side current-scope colon fallback;
4. the Series C textual parameter front end and token encodings;
5. Series D lexical/structural commit validation and atomic old-step retention;
6. a separate keyboard/PEM prompt set for the dedicated Forth capture submode.

This order is dependency guidance, not a set of Qwen tasks. Each future task
still needs bounded file slices, old-contract test migration, a reachable test
path, and an actually executed RED mutation. Tasks touching the managed
dictionary must report arena high-water. The owner-scope representation is an
implementation choice only after RAM/flash measurements; it must not change the
accepted visibility semantics. Global Forth words remain deferred.

## Wrong citations and stale “required change” claims in §§2–4

### Two `[VERIFIED:]` citations in the permitted R4 files are wrong

1. DESIGN §3.3.2 cites `packages/forth-core/forth_compile.c:23-24` for the
   static context/depth fields. Those lines are the `FORTH_OUTER_*` enum in the
   current file; the fields are under anchor `FORTH_OUTER_NEST_MAX` at current
   lines 36-37.
2. DESIGN §4.2 cites `packages/forth-core/forth_dict.c:292-323` for
   `forthResolveXEQ`'s label → item → colon order. That range is
   `startDefinition` through the start of `finishDefinition`; the resolver is at
   anchor `forthResolveXEQ` near current lines 390-420.

I did not inspect the other §§2–4 `[VERIFIED:]` targets because they are outside
R4's allowed file list. They remain unchecked, not endorsed.

### §§2–4 still describe already-landed code as absent/future

These are not harmless history in an authority document; they are literal
instructions an implementer could repeat:

- §2.2 says no primitive-count static assert exists; `forth_prims.c` ends with
  `_Static_assert(PRIM_COUNT <= 0x0FFF, ...)`.
- §3.2 calls ASLIFT-on-exit and its flipped test a required future change;
  `forth_inner.c` already sets it on normal EXIT and the test expects it.
- §3.3.4 says to remove `static` from the push helpers; they are already public
  and declared in `forth_dict.h`.
- §3.3.5 says `forthPushInt32` still stores real34; it already constructs a long
  integer.
- §3.3.7 says the emit/start/finish/abort APIs do not exist; they all exist.
- The same pseudocode says no count-cap check exists; `startDefinition` checks
  `0x6F00`.
- §3.3.7 calls the WriteName overrun a known defect; the current helper takes an
  explicit length and clamps it to the header length.
- §3.3.8 says the 64-KB Ensure guard is missing; it is the first check in
  `forthDictEnsure`.

Suggested correction: rewrite these as implemented invariants with grep anchors
and tests, not “required changes” with decayed line numbers.

### Additional internal documentation contradictions

- §3.2 says `forthInner` “never nests” while the same section specifies depth 4
  and later calls the nested path reachable. The code does nest and polls at the
  active inner level.
- §3.3.2 says the 256-byte source context is on the caller's C stack, then calls
  it a 256-byte BSS cost. The code puts it on the stack; the RAM high-water
  budget should say stack, not BSS.
- §2.2 says “keep token fetch as memcpy”; current `readToken` uses two explicit
  bytes. The implementation is alignment-safe and enforces LE, so the normative
  requirement should be “alignment-safe LE read,” not one C spelling.
- `forth_dict.h` says `forthHeader_t` is “NEVER dereferenced as-is”; the current
  implementation casts and dereferences it repeatedly at block-aligned offsets.
  Either correct the comment or replace the accesses. Do not leave a false
  portability claim.

## Test audit notes

- `test_prescan_error_halts` places the error in the first definition. Its
  `fdict.count == 0` assertion cannot detect retention of an earlier successful
  definition; the new rollback test uses two steps and goes red on counts 1/2.
- The number suite covers signed exponents and missing digits but not a sign
  after exponent digits; the new test goes red with error 0.
- Direct restore tests cover zero/out-of-range `nameLen`, not a valid length
  extending beyond `here`.
- The multi-program pre-scan test uses two programs and cannot cross the
  eight-entry tracking cliff.
- The hook-primed inner and outer depth-cap tests are not decorative: removing
  the guard would execute their sentinel word/tail and fail. The real
  outer-in-outer tokenizer test also derives a label/program from real bytes.

Within the engine-test anchors inspected, I did not find a currently registered
test that literally cannot fail under its stated mutation. The missing axes
above are coverage gaps, not false positives in the tests that do exist.

## Clean findings

- Current inner return-stack watermarking and error unwind are internally
  consistent. Normal top-level EXIT does not spell the macro but exits at
  `rsp == rspBase`, so the state is equivalent; the nested-error and preserved
  outer-rstack tests exercise the important mutations.
- The current FTOK_C47 `PTP_NONE`, NUMBER_8 padded, and NUMBER_16 decode widths
  are cell-consistent. Unsupported PTPs fail before dispatch.
- The current outer lookup order through the implemented stages is primitive →
  colon → number → label. Inserting an item branch between number and label is
  structurally implementable once B3/C1 define the helper's PTP policy.
- `FTOK_XEQN = 0x7F05` and reservation of `0x7F06..0x7FFF` are consistent with
  the current decoder ranges.

## Executed evidence log

The temporary probe gate exited 1 as intended and printed these independent
failures:

```text
1e2-3 produced error 0, expected ERROR_FUNCTION_NOT_FOUND=7
Ensure(6) returned 1 with only 4 bytes capacity
overflowing Allocate returned off=0 error=0
header name extending past here survived validation
failed pre-scan retained definitions: counts 1 then 2
ninth program counts 1 then 2 (error 0)
generation wrap kept GA=1, rebuilt GB=0 (error 0)
nested error left open=1 here/latest/count=0/65535/0, expected 8/0/1
```

The probes were removed before writing this report. The final unmodified-code
gate result is reported in the session handoff, not inferred from this expected
red mutation run.
