---
name: cross-model-audit
description: Run the multi-reader cross-model bug hunt for FUNCTIONAL defects and design flaws — a correctness review of this project's firmware, not a security assessment. Use when asked to audit code, run an audit round, hunt for bugs in a stage or subsystem, verify a fix wave, or close out an audit. Consolidates every rule five audit rounds and the forum review loop paid for. Packets are built from references/packet-template.md, checked with references/packet_lint.py, and dispatched with references/dispatch.sh — never assembled or invoked freehand; the reply's MODEL line is verified every pass.
---

# The cross-model audit

**If this task involves sending code to an out-of-family reader, you COPY
`references/packet-template.md` and fill it in, lint it with
`references/packet_lint.py`, and dispatch with `references/dispatch.sh`.
A packet assembled freehand or an `agy`/`codex` call typed from memory is
off-path even if it works — five rounds produced five packet defects and
one silent wrong-model audit, and every one looked exactly like success.**

A multi-reader bug hunt: blind in-family finders per dimension, at least
one out-of-family reader, every finding handed to a reader who did not
produce it with instructions to disprove it, findings-not-fixes, an exit
criterion that resets on every real finding. It descends from `forum/`'s
post-audit loop (rotation, judged flags, reset-on-finding, growth rule)
with the subject changed from prose to code. Voice and forum process stay
owned by `write-as-stan` and `forum/DESIGN.md`; this skill owns code.

Scope: **functional correctness** — wrong answers, lost work, stuck
states, crashes. Single-user handheld, no network, no privilege boundary.
A finding whose impact statement needs an attacker is not a finding, and
nothing in this process probes or exploits anything: the sharpest tool it
uses is a marked, reverted mutation in a private worktree, run to prove a
test can fail.

## Where things live

| artifact | role |
|---|---|
| `design-docs/forth-core/CODE_AUDIT.md` | the process record: dimensions, reader pool, what NOT to flag, report template, exit criterion. This skill is the operator's checklist; that file is the reasoning |
| `design-docs/forth-core/PROMPT_CODE_AUDIT.md` | auditor brief + refutation brief. Copy-adapt into packets and subagent prompts; never re-derive from memory |
| `design-docs/forth-core/audit-workflow.js` | the in-family runner: blind finders → dedup → worktree-isolated refutation → report. Also refutes out-of-family findings via `args.extraFindings`. Refuses to run without `round` and `outOfFamily` (2026-08-29) |
| `references/packet-template.md` | the packet skeleton, one comment per paid-for rule |
| `references/packet_lint.py` | mechanical packet checks; HARD hits block dispatch, the rest are judged |
| `references/dispatch.sh` | `agy`/`codex` invocations with the flag order, timeouts, empty-dir setup, and MODEL-line identity check built in |
| `references/bug-classes.md` | every named bug class the audits paid for; finders hunt them at all sites |

## Order of work

1. **Read the spec first.** `DESIGN.md`, the stage sheet, the traces, and
   the prior `AUDIT_*.md` reports plus `HANDOFF_*.md` for the open-finding
   list. An auditor who skips this reports rulings as defects — the
   failure mode that teaches the owner to skim, after which the one real
   finding gets skimmed too. Do not re-report open findings.
2. **Mechanical half.** `./packages/forth-core/build-test.sh` and
   `./design-docs/forth-core/design-audit.sh`. Compiler warnings count
   (`-Wtype-limits` caught a same-day vacuous assertion). Nothing the
   mechanical half reports is a finding.
3. **In-family pass.** `Workflow({scriptPath:
   'design-docs/forth-core/audit-workflow.js', args: {subject, commits,
   files, date, round, outOfFamily}})`. Args may arrive as a JSON string —
   the script parses defensively, and round 1 silently audited the wrong
   range, so SAY which range is being audited. The script THROWS without
   `round` and `outOfFamily`: four rounds closed in-family only and looked
   complete (2026-08-29). `outOfFamily: 'pending'` is the honest value for
   a FIND run whose packets are not back yet. All eight dimensions periodically; D7 (design)
   is the lens that pays for the exercise and silently lapsed for two
   rounds once. Sonnet carries the mechanical dimensions, the strongest
   model carries D7 and synthesis.
4. **Out-of-family pass.** Build packets from the template, lint, then
   `references/dispatch.sh gemini <packet>` (and `sol <packet>` for
   self-contained design review). Rules below. This pass is what closes an
   audit; if the automation fails, PASTE the packet into a fresh session
   by hand rather than skipping the pass. **Round 1 of every audit is
   read by all three families** — the in-family finders plus Gemini plus
   Sol, each over the actual subject (Sol's constraint means round 1
   always carves at least one self-contained packet). No skip: the round
   counter does not advance until both replies are fed back through the
   workflow (`outOfFamily: {packets: [...]}`) and the round-1 report
   loses its banner. Ruled 2026-08-29, after four rounds closed on a
   green probe.
5. **Cheapest evidence.** Any finding that ends in an on-screen claim gets
   a `run-sim` capture — one screenshot settled three findings in round 5
   and found C21 in round 3, and both times it was nearly left on the
   table. Follow the run-sim skill; copy its driver, never write one.
6. **Refute everything, both directions.** ORDER MATTERS: run the
   out-of-family half AFTER the in-family half and grep its numbers
   before minting one. Round 2's two families converged on the same
   defect from different evidence; the out-of-family half filed it as
   corroboration of the in-family number rather than a second ID,
   because **a number is a fix obligation and two numbers for one
   defect is how a class gets half-fixed.** In the other order the
   round would have shipped the duplicate. Cross-half dedup has no
   tooling — it is the operator's job, done by grep. Out-of-family findings go
   through the same refutation as in-family ones — feed them to the
   workflow as `args.extraFindings` (with `dimensions: []` for a
   refutation-only round, which is what round 4 was). Gemini has produced
   one real leak eight in-family readers missed AND two findings refuted
   against the code. Two rules round 9 paid for: TRANSLATE reply line
   numbers to real file:line before feeding them — packet replies cite
   excerpt-relative lines, and a wrong anchor is the round-6 stale-ref
   trap by another door; and SUPPLY pre-verified facts in the subject
   (prior dispositions, deleted-helper provenance, checked upstream
   contracts) — in round 9 those facts prevented two wrong credits and
   spared eight verifiers re-proving them.
7. **Report** per `CODE_AUDIT.md`'s template, ranked by owner cost.
   Findings, not fixes: the tree ends the audit exactly as it began.
8. **Bookkeeping.** Update the handoff (round, readers, exit state, where
   the next round starts), and apply the growth rule below to anything new
   this round taught.

## Packet rules — the packet IS the audit

A defective packet audits a codebase that does not exist, and every packet
defect so far looked exactly like a good finding. All five classes are in
the template and the linter; the linter's HARD hits are non-negotiable.

- **Model probe first, checked every pass.** `agy` with `-p` before
  `--model` silently serves Claude — the drafter exclusion failing open,
  discovered only by asking. The reply must open `MODEL: <name>` and
  `dispatch.sh` refuses a reply from the wrong family.
- **Whole functions, comments verbatim.** A `sed`-cut packet produced
  "this never writes its output" — true of the packet, false of the code.
  Condensing a load-bearing comment IS truncation: a paraphrase dropped
  the clause naming `forthConsoleRestoreSurface` and the reader's whole
  finding rested on the gap. If a comment names a function, that
  function's body is part of the packet.
- **Orientation for everything the reader cannot see.** Shared-structure
  facts ("slot 0 is the TOP"), every package override the excerpt depends
  on (round 3's bad Sol finding), and what ESTABLISHES each state, not
  only what it means — "a BORROWED frame is always FWRD" blocks the wrong
  trace; "BORROWED = the user's own row" licenses it (cost two runs).
- **Size:** 3–11 KB proven to answer in minutes; rounds 9 and 10 extended
  the proven range to 23.9 KB (Gemini) and 19.2 KB (Sol, self-contained
  design packet); the undo-history round-1 refutation packets extended it
  again to 27.9 KB (Gemini) and 26.6 KB (Sol), and the PP18 restarted
  round 1 to 30.3 KB (Gemini) and 28.9 KB (Sol) — all fully structured
  answers. Keep packets small for depth, not from fear of size —
  the old 13 KB failure was over-read. Round 10 split two oversized
  packets and the splits cost nothing, so **split on the QUESTION, not on
  the byte count**: one packet, one question, and the size follows.
- **One packet, one question**, with a budget note and "name the gap
  instead of guessing".

## The reader pool

Dimensions are coverage; the pool is independence. Fresh sessions of one
model are NOT a rotation, and the exclusion is at model-family level: the
family that wrote the code never audits it. The author may run the
workflow; it may not be one of the readers.

- **Gemini via `agy`** — the workhorse out-of-family reader, and the only
  one that can take a packet needing any repository context.
  `dispatch.sh gemini` runs
  `agy --model gemini-3.1-pro-high --print-timeout 12m -p "$(cat pkt)"`.
  Flag order is load-bearing; print mode returns EMPTY on timeout, and
  empty output is a failure, never a clean bill. The model name must be on
  `agy models`.
- **Sol via `codex`** — self-contained packets ONLY: everything inline,
  EMPTY working dir, `-s read-only` (no shell → the missing-bubblewrap
  wall never bites), reasoning medium, hard `timeout` backstop — that is
  `dispatch.sh sol`. Given anything to explore it reads exhaustively,
  ignores every budget instruction, and never concludes (twelve runs).
  Proven for design review: the C17 packet got three concrete failure
  sequences in under ten minutes, one of which reshaped the fix.
- **Paste route** — when a driver misbehaves, paste
  `PROMPT_CODE_AUDIT.md` + the packet into a fresh session by hand. Do
  not let the automation become the reason the pass is skipped.
- Never send anything you would not put in the public repo.
- `dispatch.sh probe all` verifies both drivers end-to-end for pennies;
  run it at the start of an audit session rather than debugging identity
  mid-round. A probe is not a pass: it verifies the drivers answer — it
  sends no packet and produces no finding. Four rounds were closed on the
  strength of a green probe (2026-08-29).

## Verification rules

- **Refutation is deliberately one-sided**: the verifier's job is to
  disprove the finding, with a distinct lens per verifier
  (reachability / correctness / intent), default REFUTED, majority rules,
  ties go to refuted. Separate the finding's premise from its conclusion —
  a real path with a wrong conclusion is the commonest failure mode here.
- **Reachability, not write-set.** A finding that says "if X were called
  with Y" must show what calls X with Y. Two true facts and a wrong
  conclusion has been reported to the owner twice.
- **Mutations prove coverage claims** — and run ONLY in a per-verifier
  worktree (`audit-workflow.js` enforces `isolation: 'worktree'`; two of
  three shared-tree runs were contaminated, one seeing a baseline gate RED
  at clean HEAD). Report a foreign edit, never revert it. Marked
  (`AUDIT-PROBE R<n>`), reverted by inverse edit in the same step — never
  `git checkout`/`restore`, which once nearly destroyed uncommitted stage
  work. Verify the mutation actually reached the built artifact (the
  build compiles the shadow, not your edit).
- **A mutation that stays green is one of three different things**: a
  coverage hole (fix the test), a mutation-design error — it replicated
  the code's own arithmetic or probed a property the operation cannot
  violate (fix the mutation), or unfalsifiable-by-construction (record it
  as a documented gap, as round 4's hardening was). Check for redundant
  paths that self-heal the mutation before crediting any of the three.
- **A pin written beside a fix can enshrine the fix's bug.** undo-history
  round 3: R13 v1 asserted the merged-gap representation its own fix
  produced, and the next round ruled that representation defective — the
  pin was red-first and still pinned a bug. Write pins from the ruled
  SEMANTICS; where the semantics are unsettled, pose the doubt as an
  explicit packet question (posing the merged-top ~ direction as one
  turned a hunch into two independent confirmations in a single pass).
- **An operator-run probe transcript is the cheapest refutation
  evidence.** undo-history round 1: a five-line real-key probe transcript
  supplied as a pre-verified fact killed a "stuck state" finding in one
  paragraph, where a code trace would have had to reconstruct the shift
  state machine. When a finding claims an interactive stuck state, run
  the keys first and hand the refuter the transcript.
- **A refuted finding is not a worthless one.** Round 4: both readers
  refuted a fix with the same wrong trace, and a mutation reverting the
  fix reproduced their exact failure in the code it replaced. Before
  discarding, ask which version of the code the finding is true of, and
  whether the refutation itself exposed a different live defect at the
  same site.
- **Evidence strength is part of the finding**: label executed evidence
  (probe/mutation through the gate, red for the predicted reason) apart
  from static trace. Independent agreement is the currency — record the
  multiplicity ("found by 5 of 8 readers"), and track convergence on one
  site across rounds: three independent out-of-family readers on one
  function's one shape is the threshold the process acts on.
- **The fixture rule** (three vacuous tests in one day): a fixture must
  assert it REACHED the state it claims to test, must drive the real
  gesture rather than force flags, and a fix is not finished until its
  mutation reddens. A green suite proves nothing about a test written the
  same day, and the simulator's stack protector is not the device's — the
  DMCP build carries none.

## Exit criterion and the fix trap

**Two consecutive rounds with no new CONFIRMED finding, at least one of
them out-of-family on the actual subject** — the fix commits themselves,
not just the stage they fixed. A real finding resets the count. A round
whose refutation pass died is not a verified round (round 3's verdicts
were the author's own traces; round 4 existed to pay that debt).

A round with no out-of-family reader does not count toward the two and
says so in its own report — the workflow refuses to run without being
told (`outOfFamily`, required, 2026-08-29). Round 1 is stricter: **round
1 is not complete until all three families have read the subject** —
in-family, Gemini, and Sol. Three in-family rounds are one reader's
opinion repeated, whatever their finding counts look like: PP18 rounds
1–4 confirmed 16, 8, 7 and 11 in-family, and the pass that finally ran
still returned findings in code those rounds had cleared — over-refusals
and a ruling stated in a comment that is literally false of its own
function.

Never close on a round that contains fixes: **r2 = 4 of 7 findings from
r1's fixes, r3 = 4 of 4, r5 = 9 of 12, and the rate is not falling.** A
fix is new code written at peak confidence in exactly the spot the
blind spot is widest. Relocating state is the most dangerous fix shape
(see bug-classes.md, persistence-contract mismatch). When a fix lands,
the standing rule applies: reproducer red-then-green, named class, class
test where enumerable — and the class name goes into
`references/bug-classes.md` and DESIGN-HISTORY.md.

**Rotate the question axis, not just the readers.** undo-history
rounds 1–4 all asked "is the semantics right?" and every finding
tracked the current fix wave; round 5 asked "what happens when it
FAILS?" and surfaced a wave-0 defect (torn staging consumed later)
plus the coverage hole hiding it — failure returns no fixture could
reach. An axis the audit has never asked about is where wave-0 bugs
survive four clean-looking rounds; success semantics, failure
semantics, and pin vacuity are three different axes.

## What not to flag

The full section is in `CODE_AUDIT.md` and the brief; the short form: the
code's own load-bearing comments (argue with a comment, never ignore it),
stage Non-goals, style, anything needing an attacker, theoretical paths
with no reaching input, and whatever the mechanical half already said.
Roughly a third of any audit's flags are legitimate behavior — judge
them; a finding count driven to zero means the audit was optimized
against, exactly like the forum scanners. "I could not tell whether this
is intentional" is a valid and useful verdict.

## Subagents and models

Inline the briefs into every subagent prompt — naming the file and saying
"follow it" has failed here before (2026-08-04). Sonnet for mechanical
dimensions and extraction, strongest model for D7, refutation judgment,
and synthesis. One packet, one reader, one session: reading budgets do
not compose.

Keep security-flavoured vocabulary — adversarial, attack, kill, exploit —
out of skill and workflow names, meta descriptions, and spawn prompts,
and open every spawned prompt with the correctness-review scope.
"Adversarial audit" in the workflow's name tripped a session safety
guardrail (2026-08-08) on a process whose whole point is functional bugs.
The process is unchanged; the words now describe it as what it is: an
independent cross-check.

## The growth rule

What the forum loop does with tells, this audit does with misses. A new
packet-defect class becomes a `packet_lint.py` check or a template
comment before the next round. A new reader-pool trap goes into
`dispatch.sh` or this file. A new bug class goes into
`references/bug-classes.md` and DESIGN-HISTORY.md. A process failure
amends `CODE_AUDIT.md` and this skill. If a round taught nothing new
about the process, say so in the report; if it did and nothing was
encoded, the round is not finished.
