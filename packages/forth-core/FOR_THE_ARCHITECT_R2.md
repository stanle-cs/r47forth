# R2 — notes for the architect

No normative design file was read; the R2 prompt prohibited reading DESIGN.md
and supplied the required behavioral facts. I therefore make no claim that a
DESIGN.md sentence or citation is wrong in this session.

One fixture needs an explicit architecture ruling before an implementer edits
it: `test_exec_step_halts_on_error` executes a standalone stack buffer, while
the surrounding Architecture-2 tests use source steps owned by a real program.
The Qwen task conservatively migrates it to `writeTestProgram`; if standalone
step execution is intentionally part of the supported API, correct that task
before handing it to Qwen.

The reviewed branch also contains incomplete foreign audit-probe WIP in
`test_dict_reloc.c`. It is a build blocker, not a design finding. Its intended
completion cannot be inferred safely, so the Qwen prompts treat owner resolution
as a prerequisite rather than telling an implementer to improvise.


---

## ARCHITECT RULING — 2026-07-15

### Standalone step execution is a fixture artifact, NOT a supported API.

**Hand R2's migration task to Qwen as written.** Its conservative choice —
migrate `test_exec_step_halts_on_error` to `writeTestProgram` — is correct. The
ruling is stronger than the question assumed, in two ways.

**1. The P2 ruling already decided this (2026-07-13).** `forthProgramStep`'s
contract requires the payload to reside inside a real program: the first touch
pre-scans the owning program (`FORTH_OUTER_DEFS_ONLY`), then executes only tails
(`FORTH_OUTER_SKIP_DEFS`). The `build_payload` helper was retired for exactly
this reason and the sibling tests were migrated, with the note recorded in
`test_dict_reloc.c`: *"Stack-buffer payloads encode the retired execute-in-place
semantics."* `test_exec_step_halts_on_error` is an unmigrated survivor of a
retired architecture — not a deliberate exception carved out for a supported
standalone API. There is no such API.

**2. The fixture does not merely lack an owning program — it silently acquires
the WRONG one.** `forthOwningProgramStart` (forth_bridge.c:33-42) returns the
last program whose `instructionPointer <= ptr`, with **no upper-bound check**. A
stack buffer sits at a high address, so that comparison holds for every program
and the function returns the last one rather than NULL.

Measured in the simulator (probe, since reverted):

```
stack step = 0x7fff26e95450   beginOfProgramMemory = 0x7ecb3270e004
forthOwningProgramStart(stack ptr) = 0x7ecb3270e004   -> a REAL program
```

So the test pre-scans an arbitrary, unrelated program and compiles whatever
definitions it holds into the dictionary before executing `3 SQX`. Its green
depends on what that program happens to contain: define `SQX` there and it
fails; put a malformed Forth line there and the pre-scan errors first, so
`lastErrorCode` is that error and never `ERROR_FUNCTION_NOT_FOUND`. The test is
order-dependent on unrelated fixtures and passes for a reason unrelated to the
property it names — the "test that cannot fail for the right reason" class R2
was chartered to find.

**Required of the migrated test.** Keep the property — an undefined word in a
source step sets `ERROR_FUNCTION_NOT_FOUND` and halts the run — but reach it
through a real program via `writeTestProgram`, so the pre-scan is live rather
than accidentally aimed elsewhere. The migrated test must fail if the arm stops
propagating the error; state that mutation in its header comment.

### New finding, not R2's, for the R1/R4 code-defect track

`forthOwningProgramStart` has no upper bound: any pointer at or past the last
program's start resolves to that program. It is currently harmless in production
— all three callers (forth_compile.c:476, forth_bridge.c:64, forth_bridge.c:114)
pass pointers genuinely inside program memory — and it is also only correct if
`programList` is sorted ascending by `instructionPointer`, which is assumed and
undocumented. It is a latent trap of the same shape as the catalog-drain
predicate mismatch: correct today by the callers' good behaviour, silently wrong
for the first caller that misbehaves. It is what made this fixture wrong.
Bounding it (`ptr < forthNextProgramStart(candidate)`, or an explicit
program-memory range check) is a bounded Qwen task; the sortedness assumption
should be asserted or documented.
