# R4 — Forth engine and unwritten-series design review

## Bottom line

Do not hand B, C, or D to an implementer yet.

The existing engine has four bounded implementation defects; they are specified
as executable Qwen tasks in `QWEN_PROMPTS_R4_engine.md`. The larger result is
that the unwritten series do not yet form one implementable contract:

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

## Priority 0 — decisions required before B

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

**Decision required.** Pick a generation/lifetime rule before implementing
XEQN. Plausible policies have different semantics and RAM/flash costs:

- retain old dictionary generations until every active inner frame releases
  them;
- defer the reset until the outermost Forth frame unwinds (but then specify how
  the nested Forth program step resolves its own definitions);
- treat a program started from an active Forth frame as the same generation;
- forbid this nesting with a defined error.

Merely widening the generation counter does not address active-allocation
invalidation.

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

**Suggested correction.** Specify one explicit parsing word for XEQ-by-name:
its exact token spelling, quote grammar, error codes, interpret action, compile
encoding (`FTOK_XEQN`), and precedence. Or delete “in either state” and state
that the escape exists only from the C47 side. Do not ask Qwen to infer it from
`PTP_LABEL`.

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

**Decision required.** Define two policies, either as two helpers or as one
helper with an explicit supported-PTP mask. State separately:

- which item statuses forward Forth source may resolve in B and C;
- which item statuses reverse C47 `XEQ 'NAME'` may resolve;
- what a parameterised item name means when reverse XEQ supplies no parameter.

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

**Suggested correction.** Publish the exact three-arm pseudocode, including the
colon arm's `fromProgram` value and the error when runtime resolution finds
nothing. Then reconcile every earlier “same protocol” sentence to that matrix.

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

## Priority 0 — decisions required before C

### C1. The lookup filter excludes the feature

§4.1 defines item lookup as `PTP_NONE` only. §4.4 Phase 2 says the same path
adds `PTP_REGISTER`, `PTP_NUMBER_8`, and `PTP_NUMBER_16`. Unless the lookup
policy is explicitly widened by phase, `STO` never reaches the parser described
for `STO 05`.

Specify the supported PTP set per phase and keep it separate from the reverse
resolver policy in B3.

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
specify how source text produces that byte. Add exact parsing pseudocode and an
error table before C is tasked.

### C3. The decoder delta is smaller than the phasing text says

Current `forth_inner.c` already decodes `PTP_NUMBER_8` with a two-byte advance
and `PTP_NUMBER_16` with a two-byte advance. The actual inner-interpreter
widening for C is `PTP_REGISTER`; the larger missing work is forward lookup,
source parsing, and emission.

The proposed register layout itself is cell-consistent: one KS byte plus one
zero pad, then convert and range-check before dispatch. Specify the exact error
on a failed `regInRange` and require `regKStoC` to be evaluated once rather than
twice in prose/pseudocode.

## Priority 0 — D conflicts with the pre-scan contract

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

**Decision required.** First decide whether D is:

- lexical validation only (token lengths, `:`/`;`, number grammar, parameter
  grammar), allowing unresolved names until run time; or
- semantic validation of the tentative whole owning program.

For the second option, specify a transactional algorithm after tentative
insertion: which definitions are provisionally visible, how all tails are
checked, how dictionary scalars are rolled back, whether labels/items are
resolved, and which errors block commit. It must not execute a primitive, item,
colon body, or label and must not depend on the prior run generation.

## Current-engine defects which need an architectural ruling

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

**Decision required.** Choose the overflow behavior under the RAM constraint:
grow tracking, reject the ninth with a defined error, redesign tracking, or
rebuild generations deterministically. Increasing `FORTH_SCAN_MAX` merely moves
the cliff. No Qwen task was written because the tradeoff is architectural.

### E2. The 16-bit generation comparison aliases after 65,536 bumps

**Executed evidence.** I scanned a program defining `GA`, changed its source
name in place to `GB`, called `forthRunGenBump()` 65,536 times, then touched the
program again. The next touch retained `GA` and did not build `GB`, with error
0. `forthRunGeneration` had wrapped back to `forthResetGeneration`, so equality
falsely meant “current.”

**Cost to the owner.** On a long-lived calculator, enough edits/run starts
without sampling at a Forth step can reuse stale definitions and stale scanned
program pointers.

A 32-bit counter makes the case rarer and costs BSS but does not solve lifetime
or B1. Define invalidation semantics, not only a larger integer.

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

Natural current primitives do not create this path while compiling, so I have
not classified it as a present keypad failure. B and future immediate/re-entrant
words make the invariant relevant. Decide whether nested outer entry is rejected
while a definition is open or receives a truly isolated dictionary transaction.

### E4. Restored token bodies are trusted more than the safety claim admits

`forthDictValidateRestored` validates scalar bounds and header links, but not
header padding, body boundaries, token legality, inline-data extents, branch
targets, or a terminating EXIT. `forthInner` then reads tokens and LIT/ILIT data
without comparing `ip` with `fdict.here` or an entry end.

This is static evidence, not an executed OOB probe; I deliberately did not turn
a routine review into an intentional invalid read. The current H5 comment says
a corrupt backup must “never” leave the dictionary able to read out of bounds,
which these checks cannot establish. Decide whether to weaken that claim,
validate bodies once on restore, or add runtime bounds. The latter costs bytes
on every production dispatch and needs per-body end information.

### E5. Owning-program lookup relies on an unstated ordering invariant

`forthOwningProgramStart` claims to return the largest program start `<= ptr`,
but assigns every qualifying entry and therefore returns the *last* qualifying
entry. It is correct only if `programList` is sorted by address. The R4 reading
budget excluded the upstream program-list builder, so I did not promote this to
an implementation task or claim an observed failure. Either cite and validate
the sorted invariant or compute the maximum explicitly.

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
executed mutation symptom. No task implements B, C, or D.

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
