# Prompt: adversarially audit forth-core code for bugs and design flaws

Paste everything between the rules into a fresh session of a model that did
**not** write the code, then name the dimension and the files underneath.
One dimension per session — the reading budgets do not compose.

Use this on code written by a *different* model than the one auditing. A model
reviewing its own output shares its blind spots, and that is the whole reason
this prompt exists.

This asks for findings, not patches. Take the findings, decide which ones are
real, and fix those separately.

For subagents, inline this whole brief into the spawn prompt. Naming the file
and telling the agent to follow it has failed here before.

---

You are auditing firmware code for **bugs and design flaws**. Report what you
find. Do not fix anything. Do not produce patches unless I ask for them.

## The subject

A personal hobby project: a Forth interpreter built as an external package over
the open-source C47/R47 firmware for a DM42-class pocket calculator. Single-user
handheld. No network stack, no untrusted input, no privilege boundary, no
multi-user model. The owner types programs on a rubber keypad and runs them.
The worst outcome of any bug here is that the calculator reboots and the owner
loses the program they were typing.

**Audit it as what it is.** A finding whose impact statement needs an attacker
is not a finding. Ordinary correctness — wrong answers, lost work, stuck
states, crashes — is the whole job.

## Read the design before the code

`design-docs/forth-core/DESIGN.md` is authoritative. The stage sheet
(`STAGE_*.md`) carries the rulings; `STAGE_*_TRACES.md` carries the evidence
with file:line. Code that contradicts `DESIGN.md` is a finding. Code that
contradicts your expectations but matches `DESIGN.md` is not.

## What to look for

Your dimension is named below. Stay in it — someone else has the others, and
duplicated coverage is worth less than independent coverage.

**Bugs.** Wrong results, lost or corrupted state, unreachable code that should
be reachable, states no path handles, resources not released, arithmetic that
wraps or underflows, an early `return` that skips something a later line
assumes was done, a contract broken at one call site out of nine.

**Design flaws.** Not defects today; defects waiting. Two places that must
agree with nothing forcing them to. State stored that could be derived, so it
can now disagree with its source. A rule with exceptions that are not
enumerable. A contract that is correct but that every caller gets wrong —
which is a defect of the contract, not of the callers. A guard whose conjuncts
cannot all be falsified.

**Standing lenses.** These are findings by definition wherever they appear,
in any dimension — each is a class the audit has already paid for:

- **A teardown that bypasses the wrapper.** `leaveTamModeIfEnabled` is the
  ONE public TAM teardown (D7-1, approved 2026-08-08): it ends the session
  and settles the fold bracket in one act, which is what makes the F2/F4
  strand class impossible by construction. Any NEW direct caller of
  `_tamLeave`, or any teardown path that reaches TAM's exit without going
  through the wrapper, is a finding. The one direction the construction
  CANNOT defend, so look for it specifically: a future upstream merge that
  adds an in-file `leaveTamModeIfEnabled(); <dispatch>` pair inside
  `_tamProcessInput`'s call tree. It references the public name, so it
  merges with no conflict, and it fires the unwind BEFORE its dispatch
  inserts its step — the exact L1-F2 rev-2 typed-line loss D7-1 exists to
  close, with the gate green because no fixture drives the new site.

- **An enumeration with no count behind it.** A fix, comment or design that
  lists call sites, arms or consumers — "the eleven sites", "both
  consumers", "every caller" — and is not backed by a grep whose count is
  pinned in `design-audit.sh` group I. Round 7's dominant class (D7-a): a
  hand list stands in for a counted one, and it comes back short.

For each finding you must supply, or the finding does not count:

1. **Where** — `file:line`.
2. **The reaching input** — the concrete sequence, keypress, Forth line or call
   that gets there. This is the part that is usually missing and it is the part
   that matters.
3. **What goes wrong** — the observable consequence, in terms of what the owner
   sees.
4. **Why it is wrong** — the contract, comment, ruling or invariant it
   violates. Quote it.

## What NOT to flag

**1. Decisions the code already explains.** This codebase carries load-bearing
comments. A comment explaining why something looks wrong is the design telling
you it considered your finding first. Three real examples that all look like
bugs and are not: `calcModeNormal()` followed by an unconditional
`popSoftmenu()` (removing either buries the user's menu); a formatter copying
into a caller's buffer rather than `tmpString` (`display.c` writes `tmpString`
in ~190 places); a reset that clears one store and deliberately not its
neighbour.

If a comment explains the choice and you still think it is wrong, quote the
comment and argue with it. That is a legitimate finding. Ignoring the comment
and reporting the surface is not.

**2. Deliberate scope.** Every stage sheet has a Non-goals section. Absent
features listed there are not defects. Check before reporting a gap.

**3. Style, naming, formatting, and code you would have written differently.**
Not this audit's job.

**4. A theoretical path with no input that reaches it.** The project's most
expensive recurring lesson, and it has a name: **reachability, not write-set.**
One trace claimed a drain worked "by construction" from reading the predicates
and was wrong about the `if` above them. Another reported a bug from two true
facts and a wrong conclusion; it was retracted on the reachability trace. If
you cannot say what calls it with that argument, say so and mark the finding
unreached rather than asserting it.

**5. Anything the build gate or the compiler already reports.** Those ran
first.

If you are unsure whether something is a genuine defect or a deliberate
decision whose reasoning you have not found, **say so explicitly rather than
guessing.** An honest "I could not tell whether this is intentional" is useful.
A confident wrong finding costs more than silence, because it trains the reader
to skim.

## Output

1. **Findings**, worst first, ranked by what they cost the owner — a crash on a
   common gesture outranks a wrong result nobody reaches, which outranks an
   untidy contract. For each: where, reaching input, consequence, the violated
   contract quoted, and your confidence.
2. **Coverage**: what you read, what you did not, and what you ran out of
   budget for. A report that does not say what it missed cannot be trusted
   about what it found.
3. **Deliberately not flagged**: what you considered and judged correct, with
   the reasoning that cleared it. **Mandatory.** An empty section here means
   you did not understand what you read.
4. **Verdict**: would you ship this? Where does it break first?

Do not try to reduce the finding count to zero, and do not pad it. Tell me
which findings you would leave alone if the goal were code that is correct
rather than code that passes an audit.

---

# Refutation brief (second pass, different reader)

Paste this instead when handing a finding to a verifier. The verifier must not
be the reader that produced the finding.

---

You are trying to **refute** a code-audit finding. Not evaluate it — refute it.
Assume it is wrong and look for the reason. Findings that survive a genuine
attempt to kill them are worth acting on; findings that were merely admired are
not.

You have one of three lenses. Use only yours:

- **Reachability** — construct the call path and the concrete input that
  reaches the reported line. If you cannot construct it, the finding is
  REFUTED. "It looks reachable" is not construction.
- **Correctness** — grant the path. Is the described consequence what the code
  actually does? Trace it. A finding can have a real path and a wrong
  conclusion; that is the commonest failure mode here.
- **Intent** — is this documented as deliberate? Search the comments,
  `DESIGN.md`, the stage sheets and `DESIGN-HISTORY.md`. If the design ruled on
  it, the finding is REFUTED and you cite the ruling.

Where the claim is "this is not covered" or "this test cannot fail", the proof
is a **mutation**: break the thing, run the gate, and see whether it goes red.
Apply the mutation, observe, and revert it in the same step. The tree must be
clean when you finish.

**Default to REFUTED when uncertain.** State your verdict as REFUTED or
SURVIVES, then one paragraph of why, then the evidence — the path you
constructed, the trace you followed, or the ruling you found.
