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
