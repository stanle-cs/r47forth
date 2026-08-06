# forth-core — adversarial code audit

A multi-reader bug hunt. Several independent auditors read the same code
without seeing each other's work, every finding is then handed to someone
who did **not** find it and who is asked to knock it down, and what
survives goes in a report.

This is `forum/`'s post-audit workflow with the subject changed. That
workflow exists because a model auditing its own output shares its blind
spots, and rotating the reader is the only thing that has reliably
surfaced new classes of tell — a fresh pass once found five that repeated
self-audits never saw, including damage left behind by the previous round
of fixes. The same is true of code, and this project has the scars to
prove it: every one of the last five "implementer failures" was a spec or
harness defect the implementer correctly surfaced, and the two worst
Stage N defects were found by a mutation that refused to go red and by a
screenshot, not by anybody re-reading their own work.

## What this is not

| | question it answers |
|---|---|
| `design-audit.sh` / `DESIGN_AUDIT.md` | *has the design drifted?* Not a bug hunt. Run it too; it is cheap. |
| `CODE_REVIEW_SERIES.md` (R1–R5) | the same bug hunt, one model, one area per session, output = a Qwen prompt list. This supersedes it for anything worth more than one pass. |
| `/code-review`, `/security-review` | working-diff review and security. This is deeper and slower, and it is aimed at correctness and shape, not at attackers. |

Scope note, carried from `CODE_REVIEW_SERIES.md` and still true: this is a
single-user handheld with no network stack, no untrusted input and no
privilege boundary. The worst outcome of a bug here is that the
calculator reboots and the owner loses the program they were typing.
**Do not audit this as though it were a server.** A finding whose impact
statement needs an attacker is not a finding.

---

## Order of work

1. **Read the spec.** `DESIGN.md` is authoritative; the stage's own sheet
   (`STAGE_*.md`) carries the rulings; `STAGE_*_TRACES.md` carries the
   evidence with file:line. An auditor who has not read these will report
   deliberate decisions as defects, which is the failure mode that makes
   audits get ignored.
2. **Run the mechanical half first**, so nobody spends a reading budget on
   what a script can find:

   ```bash
   ./packages/forth-core/build-test.sh            # gate: build + self-test + upstream suite
   ./design-docs/forth-core/design-audit.sh       # drift
   ```

   Compiler warnings count. `-Wtype-limits` found a vacuous assertion in
   Stage N that had been written that morning.
3. **Fan out.** One auditor per dimension (below), each blind to the
   others. Blindness is the point: two auditors who agree independently
   are evidence, two who agree because one read the other's notes are one
   auditor.
4. **Run at least one OUT-OF-FAMILY reader over the same subject.** Fresh
   sessions of one model are not a rotation — they share a training
   distribution, and therefore the blind spots. `forum/DESIGN.md` ruled
   this for prose on 2026-08-03 (the pool is ChatGPT and Gemini, one pass
   each) and the same reasoning is what makes it binding here. The
   out-of-family reader is not given a dimension: it is told the others
   exist and sent where it would go if nobody had reviewed the code at
   all. Depth on what looks wrong beats breadth over what looks fine.
5. **Verify adversarially.** Every finding goes to a reader who did not
   produce it, whose instructions are to **refute** it. Default to
   refuted when uncertain. A finding that survives is CONFIRMED; one that
   nobody could reproduce but nobody could kill is PLAUSIBLE and says so
   in the report.
6. **Synthesise the report** (template below). Findings ranked by what
   they cost the owner, not by how clever they are.
7. **Exit criterion.** Two consecutive rounds with no new CONFIRMED
   finding, **and at least one of those rounds out-of-family.** An
   all-in-family clean round does not close the audit. **A real finding
   resets the count** — because the fix is new code, and new code has not
   been audited by anybody.
8. **Stan reads the report.** Nothing is fixed as part of the audit; see
   "Findings, not fixes".

---

## The dimensions

Eight lenses. Each is a separate reader with its own budget. They overlap
deliberately at the edges — the overlaps are where independent agreement
becomes evidence.

| # | lens | the question |
|---|---|---|
| D1 | **Contracts vs callers** | Does each function's stated contract hold at *every* call site, including the ones added last? Who calls it with the argument the banner says is impossible? |
| D2 | **State machines and lifecycle** | capture open/suspend/close, the fold, menu stack, calcMode. What sequence leaves a state nobody handles? What survives a reset that should not, or dies that should not? |
| D3 | **Boundaries and arithmetic** | indices, wraps, caps, lengths, off-by-one, unsigned underflow. Where does a counter meet a cap, and what happens one past it? |
| D4 | **Error and refusal paths** | the unhappy path. After a refusal or an error, is every piece of state consistent with every other? What did the early `return` skip? |
| D5 | **Guard reachability** | for each conjunct in each gate: construct the input that falsifies it. A guard nobody can reach is dead; a guard that cannot be falsified is noise; a missing conjunct is a defect. |
| D6 | **Tests that cannot fail** | vacuous assertions, self-comparison, oracles that pass on the wrong answer, a `goto cleanup` with no `fail = 1`, a test whose comment claims more than its body checks. |
| D7 | **Design flaws** | not bugs. Two places that must agree and nothing forcing them to. A rule with exceptions. State derivable from other state and stored anyway. A contract that is right but that every caller gets wrong — which is a defect of the contract. |
| D8 | **Upstream discipline** | package overrides vs upstream: hunks that could be smaller, behaviour changed where a call would do, drift risk in the largest overrides (`screen.c`, `keyboard.c`). |

Sizing: D1–D6 are mechanical enough for a cheap model; D7 needs the
strongest one available and is the lens that pays for the exercise.
(Standing policy: Sonnet for mechanical subagent work, Fable for judgment
stages, no Haiku for multi-step — bench-backed 2026-08-04.)

### The reader pool

Dimensions are *coverage*. The pool is *independence*, and they are not
the same axis — eight dimensions run by one model is one reader wearing
eight hats.

| reader | how | role |
|---|---|---|
| in-family subagents | the workflow | the eight dimensions, blind to each other |
| **GPT-5.6 Sol / Gemini, by paste** | paste `PROMPT_CODE_AUDIT.md` plus the diff into a fresh session | **the out-of-family pass. This is the route that works.** |
| GPT-5.6 Sol, via `codex` | see the note below before trying | automation attempt; unproven here |

**Automating the out-of-family pass — what works, 2026-08-06.**

| reader | invocation | state |
|---|---|---|
| **Gemini** | `agy --model gemini-3.1-pro-high --print-timeout 12m -p "$(cat prompt.txt)"` | **WORKS.** Answers a ~3 KB packet in a couple of minutes. This is the reader to use. |
| Sol | `codex exec -s read-only --skip-git-repo-check -m gpt-5.6-sol -c model_reasoning_effort="medium" --cd <empty dir> -o out.txt - < prompt.txt` | **BLOCKED on a system package** — see below. |

**Settled 2026-08-06 after twelve runs: use Gemini. Sol via `codex` does
not complete an audit, and the reason is behavioural rather than
environmental.**

The sandbox blocker below is real and the owner's bypass authorisation
clears it — `--dangerously-bypass-approvals-and-sandbox` gives
`sandbox: danger-full-access`, needs no bubblewrap, and Sol then ran four
times longer than any sandboxed attempt and left the tree untouched
despite write access. It still produced no report. It reads
exhaustively and does not self-limit: told to skip the prior audit
reports it went to `DESIGN.md` instead and was still reading at 58
minutes. Told in three different phrasings to budget its exploration and
to write a partial report rather than run out, it did neither.

That is not a flag to tune. **Give the out-of-family pass to Gemini**,
which answers a whole-function packet in minutes and has already earned
its place — it found a frame leak eight in-family readers missed. Keep
Sol for the paste route, where a human closes the loop.

The rest of this section is kept because each item still bites, and
because the bubblewrap diagnosis is correct for anyone running codex
sandboxed:

**Sol is blocked on bubblewrap when sandboxed, and that part is the whole story.** `codex`
sandboxes every shell command the model runs; bubblewrap is not
installed here (`apt-cache policy bubblewrap` → `Installed: (none)`), so
it falls back to a bundled copy that does not work under WSL2. The
symptom is exact and reproducible: prompts needing NO shell answer fine
(a "reply SOL-OK" probe returned twice), and every prompt where the model
runs a command hangs until the wall clock kills it, emitting no final
message. Nine runs on 2026-08-06 chased this through three wrong
diagnoses before the pattern was clear.

Unblocking it is one line and needs the owner's sudo:

```bash
sudo apt-get install bubblewrap
```

`--dangerously-bypass-approvals-and-sandbox` also removes the dependency
and is deliberately NOT used here: this model has already demonstrated it
ignores an explicit "do not run any commands", and an unsandboxed reader
loose in the repo is a worse trade than one missing reader.

Three more things learned the same day, all of which cost a run each:

- **`reasoning effort` defaults to `none`** in `~/.codex/config.toml`.
  That alone makes a free-form run wander a large diff and never
  conclude. Diagnose it FIRST; it presents exactly as a wall-clock
  problem.
- **Both readers fail on large prompts and succeed on small ones.** The
  full 304-line packet returned nothing from either; a 60-line one
  worked first try. Give the out-of-family reader the FUNCTIONS under
  review, not the stage — which is what it should get anyway, since its
  job is depth rather than coverage.
- **Sol ignores "do not run any commands"** even with the source fully
  inlined. Running it in an empty scratch directory
  (`--skip-git-repo-check --cd <dir>`) is what removes the temptation.
- **Send WHOLE FUNCTIONS, and state the orientation of anything the
  reader cannot see.** Round 2's packet was assembled with `sed` and cut
  a function off before its copy-out tail; the reader duly reported "this
  never writes its output", which was true of the packet and false of the
  code. The same round produced a finding that inverted the softmenu
  stack — slot 0 is the TOP (`currentMenu()` is `menu(0)`), and nothing
  in the excerpt said so. A reader with no repository cannot check either
  of those, so both belong in the packet: whole functions, and a line of
  orientation for every shared structure they touch. The failure mode is
  expensive because it looks exactly like a good finding.

The earlier failure list, kept because each one still bites:

- `reasoning effort` defaults to **none** in `~/.codex/config.toml`,
  which is the real reason a free-form run wanders a large diff and never
  concludes. Set `-c model_reasoning_effort="high"`. Diagnose this FIRST;
  it looks exactly like a wall-clock problem and is not.
- `codex exec review --base <commit>` is the bounded form, but `--base`
  is mutually exclusive with a custom prompt — so the audit brief cannot
  be supplied with it, and the review runs on its own generic
  instructions.
- `codex exec review` rejects `--cd`; plain `codex exec` accepts it.
- `--cd /tmp` fails the trusted-directory check; run from the repo.
- Even single-shot, with the source inlined and no exploration allowed,
  the run returned with no model output inside 25 minutes.

The value of the out-of-family reader does not depend on any of this.
`PROMPT_CODE_AUDIT.md` is written to be pasted whole, which is how
`forum/PROMPT_AUDIT.md` has always been used and how the prose workflow
gets its second family. **Paste it. Do not let the automation become the
reason the pass is skipped** — the pass is what closes the audit, and the
tooling is only there to save a copy-paste.

One partial run did land a finding before it died: `.S` reports the
display-window size as if it were the stack depth. The in-family pass
found the same defect independently (C7). Two readers, no contact, same
defect — that is the agreement worth having, and it arrived from a run
that never finished.

Sol is already this project's established independent reviewer:
`CODE_REVIEW_SERIES.md` is addressed to it by name, and it ran the F1-5,
F4-2, F4-3 and F5-2 rescues. It is a reviewer, never the author of the
code it reviews.

**Do not send anything to an out-of-family reader that you would not put
in the public repo.** This one is a fork of open-source firmware with no
secrets in it, which is why the rule is cheap to keep here; it is still
worth stating, because the rule is about the habit rather than about this
repository.

---

## What NOT to flag

The single most important section, and the direct descendant of
`PROMPT_AUDIT.md`'s "do not flag a list of three real things as a
rhetorical triple". An audit that reports deliberate decisions as defects
trains its reader to skim, and then the one real finding gets skimmed
too.

**1. Decisions the code already explains.** This codebase carries
load-bearing comments, and a comment that says *why* something looks
wrong is the design telling you it considered your finding first. Real
examples, all of which look like bugs and are not:

- `calcModeNormal()` followed by an unconditional `popSoftmenu()` in the
  EXIT ladder — two calls that look redundant. Removing either leaves the
  user's menu buried; the comment says so and names the rev that got it
  wrong.
- A formatter that copies into a caller-supplied buffer instead of
  `tmpString`. Looks like a pointless copy. `display.c` writes
  `tmpString` in ~190 places.
- `forthCapPowerReset()` clearing the console ring while `forthCapClose()`
  deliberately does not. Looks asymmetric. It is: the dialogue survives
  close by ruling.

If a comment explains the choice and you still think it is wrong, say
that, quote the comment, and argue with it. That is a legitimate finding.
Ignoring the comment and reporting the surface is not.

**2. Deliberate scope.** "No line wrapping", "no string literals", "no
input words" are Stage N non-goals, written down. Absent features are not
defects. Check the stage's Non-goals section before reporting a gap.

**3. Anything that needs an attacker.** See the scope note above.

**4. Style, naming and formatting.** Not this audit's job. `/simplify`
exists.

**5. A theoretical path with no input that reaches it.** This is the
project's most expensive recurring lesson and it has a name:

> **Reachability, not write-set.** M-T5 claimed a drain worked "by
> construction" from reading the predicates, and was wrong about the `if`
> above them. T7.5 reported a bug from two true facts and a wrong
> conclusion, and it was retracted on the reachability trace. A finding
> that says "if X were called with Y" must show what calls X with Y.

**6. Whatever the mechanical half already reported.** The gate, the
warnings and `design-audit.sh` ran first; do not re-report their output as
your own discovery.

If you are unsure whether something is a genuine defect or a deliberate
decision you have not read the reasoning for, **say so explicitly rather
than guessing.** An honest "I could not tell whether this is intentional"
is useful. A confident wrong finding costs more than silence.

---

## Findings, not fixes

The audit produces a report. It does not touch the code, and the tree it
finishes on is the tree it started on (`git diff` clean, gate green — the
`CODE_REVIEW_SERIES.md` rule, unchanged).

Two reasons, both learned here. Fixes made during a hunt are unaudited
code, and they invalidate every finding produced after them. And the
owner decides which findings are real: roughly a third of any audit's
output is legitimate behaviour that must not be "fixed", exactly as
roughly a third of the forum scanners' flags are factual enumerations
that must not be rewritten.

**Do not try to reduce the finding count to zero.** Say which findings
you would leave alone if the goal were code that is correct rather than
code that passes an audit.

When a finding *is* accepted for fixing, the standing rule applies: every
fix lands with a reproducer, a named bug class, and a class-level test
where the class is enumerable (Stan, 2026-08-04).

---

## Verification: how a finding earns CONFIRMED

The verifier's instructions are adversarial by construction — the job is
to kill the finding, not to appreciate it. Each verifier gets a distinct
lens, because redundancy catches less than diversity:

- **Reachability.** Construct the call path and the input. If you cannot,
  the finding is refuted.
- **Correctness.** Assume the path exists; is the described consequence
  actually what the code does?
- **Intent.** Is this documented as deliberate anywhere — comment,
  `DESIGN.md`, a stage sheet, `DESIGN-HISTORY.md`? A finding that the
  design already ruled on is refuted, and the ruling is cited.

A finding survives on majority. Ties go to refuted.

**The strongest available proof is a mutation.** For any finding of the
form "this is not covered" or "this test cannot fail", the proof is to
break the thing and watch the suite stay green — and Stage N is the
precedent that this works in both directions: the mutation that would not
go red *was* the finding (an unbounded ring walk that hung instead of
miscounting), and a stack-delta mutation that passed the whole suite
exposed a genuinely missing probe. Mutations are applied, observed, and
**reverted** within the same step; the tree must be clean at the end.

Where the tree must be touched to prove something, do it in a worktree,
never in the working tree the owner is using.

---

## Report

One file, `AUDIT_<subject>_<date>.md`, in `design-docs/forth-core/`.
Sections, in this order:

1. **Subject and coverage.** What was audited (commits, files, lines),
   what was deliberately not, and what the reading budget did not reach.
   A report that does not say what it missed cannot be trusted about what
   it found.
2. **Mechanical results.** Gate, warnings, `design-audit.sh`. Pass/fail
   and anything they surfaced.
3. **CONFIRMED findings**, worst first. Each carries: where
   (`file:line`), what breaks, **the concrete input or sequence that
   reaches it**, why it is wrong (with the contract or ruling it
   violates), the bug class, and the suggested class-level test. No
   patches.
4. **PLAUSIBLE findings** — survived refutation but nobody could
   construct the reaching input. Say what would settle each one.
5. **Design observations** (D7). Shape, not defects. These age better
   than the bug list and are usually the reason to run the audit.
6. **Deliberately not flagged.** What was considered and judged correct,
   with the reasoning that cleared it. **Mandatory.** An audit with an
   empty section here did not understand what it read.
7. **Verdict.** Would you ship this? Where would it break first?
8. **Round and exit state.** Which round this was, which readers, and
   whether the exit criterion is met.

Findings are ranked by what they cost the owner. A crash on a common
gesture outranks a wrong result in a case nobody reaches, which outranks
a contract that is merely untidy.

---

## Running it

Two ways, same prompt. `PROMPT_CODE_AUDIT.md` is the auditor brief; it is
written to be pasted whole into a fresh session **or** inlined into a
subagent prompt. Inlining it is mandatory for subagents, not optional:
"invoke the skill and follow it" has failed here before (2026-08-04, a
hand-rolled capture driver), so the spawn prompt carries the binding
rules and names the reference artifacts.

**By hand**, the forum way: paste `PROMPT_CODE_AUDIT.md` plus one
dimension into a fresh session of a model that did **not** write the
code, one dimension per session, then paste each finding into a second
fresh session with the refutation brief.

**As a workflow**, when the owner has opted into multi-agent
orchestration: `design-docs/forth-core/audit-workflow.js` fans the
dimensions out, pipelines each finding into three verifiers with distinct
lenses, and synthesises the report. It runs finders blind to each other
by construction, which is the part that is hard to hold to by hand.

The author of the code under audit may run the workflow. It may not be
one of the readers.
