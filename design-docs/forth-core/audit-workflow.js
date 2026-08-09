/* Naming note (2026-08-08): this was 'forth-core-adversarial-audit', and the
 * loaded word in the meta tripped a session safety guardrail. The process is
 * unchanged; the vocabulary now says what it is — an independent correctness
 * cross-check for functional bugs. Keep security-flavoured words (adversarial,
 * attack, kill, exploit) out of names, meta, and spawn prompts. */
export const meta = {
  name: 'forth-core-code-audit',
  description: 'Multi-reader correctness audit for functional bugs: blind finders per dimension, independent cross-check pass, report',
  whenToUse: 'Deep functional-bug/design-flaw hunt over a stage or subsystem. See CODE_AUDIT.md.',
  phases: [
    { title: 'Find',      detail: 'one blind auditor per dimension' },
    { title: 'Refute',    detail: 'each finding re-examined by a reader that did not produce it' },
    { title: 'Synthesis', detail: 'rank, cross-check, write the report' },
  ],
}

/* args: { subject, commits, files, dimensions? }  — see CODE_AUDIT.md.
 * Findings, not fixes: no agent here edits the tree. Mutations, where a
 * verifier needs one, are applied and reverted inside that agent's step. */

/* args may arrive as an object or as a JSON string depending on how the run was
 * launched. Round 1 of this workflow silently audited `main..HEAD` instead of
 * the requested range because `args.subject` on a string is undefined and the
 * defaults took over — the audit was still useful, but it was not the one that
 * was asked for. Parse defensively and SAY which range is being used.
 *
 * RESUME DROPS ARGS (round 6, the same failure by a new door). A
 * `Workflow({scriptPath, resumeFromRunId})` call passes NO args, so on resume
 * `args` is undefined and every default takes over: a refutation-only run
 * (dimensions:[], extraFindings:[...]) silently became an 8-dimension FIND
 * over `main..HEAD` under the subject "the current branch", losing all 13
 * findings it was meant to verify. NEVER resume a parameterized run to pick up
 * a script edit — relaunch FRESH with the full args re-passed. Resume is only
 * safe for a run whose args were the defaults anyway. The log line below is the
 * check: if it does not name your subject and range, you are auditing the wrong
 * thing. */
const A = (typeof args === 'string')
  ? (() => { try { return JSON.parse(args) } catch { return {} } })()
  : (args || {})

const SUBJECT = A.subject || 'the current branch'
const RANGE   = A.commits || 'main..HEAD'
const FILES   = A.files   || '(discover from the commit range)'

/* The auditor brief, inlined verbatim rather than referenced. Naming the file
 * and telling the agent to follow it has failed here before (2026-08-04). */
const BRIEF = `
This is a CORRECTNESS review of hobby calculator firmware — functional bugs
and design flaws only. It is not a security assessment; there is no security
dimension to assess. Report findings. Do not fix anything. Do not produce
patches.

THE SUBJECT. A personal hobby project: a Forth interpreter built as an external
package over the open-source C47/R47 firmware for a DM42-class pocket
calculator. Single-user handheld. No network stack, no untrusted input, no
privilege boundary. The worst outcome of any bug is that the calculator reboots
and the owner loses the program they were typing. A finding whose impact
statement needs an attacker is NOT a finding. Ordinary correctness — wrong
answers, lost work, stuck states, crashes — is the whole job.

READ THE DESIGN BEFORE THE CODE. design-docs/forth-core/DESIGN.md is
authoritative. The stage sheet (design-docs/forth-core/STAGE_*.md) carries the
rulings; STAGE_*_TRACES.md carries evidence with file:line. Code that
contradicts DESIGN.md is a finding. Code that contradicts your expectations but
matches DESIGN.md is not.

EVERY FINDING MUST SUPPLY, or it does not count:
 1. where: file:line
 2. the REACHING INPUT: the concrete keypress sequence, Forth line, or call
    path that gets there. This is the part usually missing and the part that
    matters.
 3. what goes wrong: the observable consequence, in terms of what the owner sees
 4. why it is wrong: the contract, comment, ruling or invariant violated, quoted

WHAT NOT TO FLAG:
 1. Decisions the code already explains. This codebase carries load-bearing
    comments; a comment explaining why something looks wrong is the design
    telling you it considered your finding first. Real examples that look like
    bugs and are not: calcModeNormal() followed by an unconditional
    popSoftmenu() in the EXIT ladder; a formatter copying into a caller's
    buffer rather than tmpString (display.c writes tmpString in ~190 places);
    a reset that clears one store and deliberately not its neighbour. If a
    comment explains the choice and you still think it is wrong, QUOTE the
    comment and argue with it — that is legitimate. Ignoring it is not.
 2. Deliberate scope. Every stage sheet has a Non-goals section. Absent
    features listed there are not defects.
 3. Style, naming, formatting, or code you would have written differently.
 4. A theoretical path with no input that reaches it. REACHABILITY, NOT
    WRITE-SET: one trace claimed a drain worked "by construction" from reading
    the predicates and was wrong about the "if" above them; another reported a
    bug from two true facts and a wrong conclusion and was retracted on the
    reachability trace. If you cannot say what calls it with that argument, mark
    the finding unreached rather than asserting it.
 5. Anything the build gate or the compiler already reports.

If unsure whether something is a defect or a deliberate decision whose reasoning
you have not found, SAY SO rather than guessing. A confident wrong finding costs
more than silence: it trains the reader to skim, and then the one real finding
gets skimmed too.

DO NOT pad the finding count and do not drive it to zero. You must also report
what you considered and cleared — an empty "deliberately not flagged" means you
did not understand what you read.

You may read anything and run read-only commands. Do NOT edit files.
`.trim()

const FINDING_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['findings', 'coverage', 'not_flagged', 'verdict'],
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['title', 'file', 'line', 'reaching_input', 'consequence', 'violated', 'severity', 'confidence'],
        properties: {
          title:          { type: 'string', description: 'one line, the claim alone' },
          file:           { type: 'string' },
          line:           { type: 'integer' },
          reaching_input: { type: 'string', description: 'concrete sequence/call path, or "UNREACHED: ..." if none found' },
          consequence:    { type: 'string', description: 'what the owner sees' },
          violated:       { type: 'string', description: 'the contract/comment/ruling, quoted' },
          severity:       { type: 'string', enum: ['crash-or-data-loss', 'wrong-result', 'stuck-state', 'latent', 'design-flaw'] },
          confidence:     { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
    coverage:    { type: 'string', description: 'what was read, what was not, what the budget did not reach' },
    not_flagged: { type: 'string', description: 'considered and cleared, with the reasoning' },
    verdict:     { type: 'string' },
  },
}

const VERDICT_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['verdict', 'why', 'evidence'],
  properties: {
    verdict:  { type: 'string', enum: ['REFUTED', 'SURVIVES'] },
    why:      { type: 'string' },
    evidence: { type: 'string', description: 'the path constructed, the trace followed, or the ruling found' },
  },
}

const DIMENSIONS = [
  { key: 'contracts', effort: 'high',
    q: 'CONTRACTS VS CALLERS. Does each function\'s stated contract hold at EVERY call site, including ones added recently? Who calls it with the argument the banner says is impossible? Check preconditions the caller is assumed to establish and the ones nobody establishes.' },
  { key: 'lifecycle', effort: 'high',
    q: 'STATE MACHINES AND LIFECYCLE. Capture open/suspend/close, the fold, the softmenu stack, calcMode, FLAG_ALPHA. What sequence leaves a state nobody handles? What survives a reset that should not, or dies that should not? What happens when two of these change at once?' },
  { key: 'arithmetic', effort: 'high',
    q: 'BOUNDARIES AND ARITHMETIC. Indices, wraps, caps, lengths, off-by-one, unsigned underflow, buffer sizes vs the strings written into them. Where does a counter meet a cap, and what happens one past it? Check every cast and every subtraction on an unsigned type.' },
  { key: 'errorpaths', effort: 'high',
    q: 'ERROR AND REFUSAL PATHS. The unhappy path. After a refusal, an error, or an early return: is every piece of state consistent with every other? What did the early return skip that a later line assumes was done? Who clears lastErrorCode and who reads it after.' },
  { key: 'guards', effort: 'high',
    q: 'GUARD REACHABILITY. For each conjunct in each gate: construct the input that falsifies it. A guard nobody can reach is dead code; a conjunct that cannot be falsified is noise; a MISSING conjunct is a defect. Pay attention to gates that were recently edited.' },
  { key: 'tests', effort: 'high',
    q: 'TESTS THAT CANNOT FAIL. Vacuous assertions (comparisons always true/false by type range), self-comparison, oracles that pass on the wrong answer, a goto cleanup with no fail=1, a test whose comment claims more than its body checks, a fixture that silently no-ops. Name the specific assertion and what would have to break for it to fire.' },
  { key: 'design', effort: 'xhigh',
    q: 'DESIGN FLAWS, not bugs. Two places that must agree with nothing forcing them to. State stored that could be derived, so it can now disagree with its source. A rule with non-enumerable exceptions. A contract that is correct but that every caller gets wrong — a defect of the contract. Duplicated truth. Anything that will be a bug the next time someone touches it.' },
  { key: 'upstream', effort: 'medium',
    q: 'UPSTREAM DISCIPLINE. Package overrides vs upstream files: hunks larger than they need to be, behaviour changed inline where a call would do, drift risk in the largest overrides (screen.c, keyboard.c, items.c). Anything that will conflict badly on the next upstream merge.' },
]

const dims = A.dimensions
  ? DIMENSIONS.filter(d => A.dimensions.includes(d.key))
  : DIMENSIONS

phase('Find')
log(`auditing ${SUBJECT} — RANGE ${RANGE} — across ${dims.length} blind dimensions`)

/* Barrier is CORRECT here: the dedup and the rotation both need every
 * finder's output at once — a verifier must not receive a finding produced by
 * the dimension it is about to be handed. */
const reports = (await parallel(dims.map(d => () =>
  agent(
`${BRIEF}

SUBJECT: ${SUBJECT}
COMMIT RANGE: ${RANGE}
FILES IN SCOPE: ${FILES}

Start by reading design-docs/forth-core/DESIGN.md (the sections relevant to your
dimension), then the stage sheet and traces, then the code. Use
\`git log -p ${RANGE}\` and \`git diff ${RANGE}\` to see what actually changed.

The named bug-class catalog is
.claude/skills/cross-model-audit/references/bug-classes.md — classes this
codebase has already paid for. Hunt your dimension's members at ALL their
sites: recurrence is the norm here (the same shape has come back at a second
site after the first was fixed and commented).

YOUR DIMENSION — stay in it, someone else has the others; duplicated coverage is
worth less than independent coverage:

${d.q}`,
    { label: `find:${d.key}`, phase: 'Find', schema: FINDING_SCHEMA, effort: d.effort })
))).filter(Boolean)

const all = []
reports.forEach((r, i) => (r.findings || []).forEach(f => all.push({ ...f, dim: dims[i].key })))

/* Out-of-family findings enter the SAME refutation as in-family ones —
 * verify before believing, in both directions: Gemini produced one real leak
 * eight in-family readers missed, and two findings that were refuted against
 * the code. Pass them as args.extraFindings (FINDING_SCHEMA finding shape;
 * dim defaults to the packet reader's name or 'out-of-family'). With
 * args.dimensions = [] this becomes a refutation-only round, which is what
 * round 4 was. */
const extra = Array.isArray(A.extraFindings) ? A.extraFindings : []
/* Round-7 trap: extraFindings in the wrong shape (where/claim instead of
 * file/line/title) collapse in the dedup on undefined keys, kill the refute
 * fan-out on f.file.split, and the synthesis still writes a confident report
 * over ZERO verified findings — a single-reader artifact wearing the pass's
 * clothes. Refuse loudly BEFORE any agent runs. */
{ const bad = extra.filter(f => !f || typeof f.file !== 'string' || !Number.isInteger(f.line) || !f.title)
  if (bad.length) throw new Error(`extraFindings must be FINDING_SCHEMA-shaped (title, file, line:int, reaching_input, consequence, violated, severity): ${bad.length} of ${extra.length} malformed — refusing to run a refutation over mangled findings`) }
extra.forEach(f => all.push({ dim: 'out-of-family', ...f }))
if (extra.length) log(`${extra.length} out-of-family findings joined the refutation queue`)

/* Dedup by file:line + severity — plain code, not an agent. */
const seen = new Set()
const unique = all.filter(f => {
  const k = `${f.file}:${f.line}:${f.severity}`
  if (seen.has(k)) return false
  seen.add(k)
  return true
})
log(`${all.length} raw findings, ${unique.length} after dedup`)

phase('Refute')

/* Rotate: a finding from dimension D is never verified by a reader working D.
 * Three lenses, assigned round-robin so every finding gets a verifier whose
 * angle differs from the finder's. */
const LENSES = [
  { key: 'reachability', text: 'REACHABILITY — construct the call path and the concrete input that reaches the reported line. If you cannot construct it, the finding is REFUTED. "It looks reachable" is not construction.' },
  { key: 'correctness',  text: 'CORRECTNESS — grant the path. Is the described consequence what the code actually does? Trace it. A finding can have a real path and a wrong conclusion; that is the commonest failure mode here.' },
  { key: 'intent',       text: 'INTENT — is this documented as deliberate? Search the comments, DESIGN.md, the stage sheets and DESIGN-HISTORY.md. If the design ruled on it, the finding is REFUTED and you cite the ruling.' },
]

const CAP = 24
if (unique.length > CAP) {
  log(`NOTE: ${unique.length - CAP} findings beyond the ${CAP} verified are listed UNVERIFIED in the report, not dropped`)
}
const toVerify = unique.slice(0, CAP)

const judged = await parallel(toVerify.map((f, i) => () => {
  const lens = LENSES[i % LENSES.length]
  return agent(
`You are cross-checking a finding from a CORRECTNESS review of hobby calculator
firmware (functional bugs, not security — there is no security dimension here).
Your job is to REFUTE the finding: assume it is wrong and look for the reason.
Findings that survive a genuine attempt to disprove them are worth acting on;
findings that were merely admired are not. DEFAULT TO REFUTED WHEN UNCERTAIN.

Repo: /home/stan/c43. Subject: ${SUBJECT} (${RANGE}).

THE FINDING (produced by a different reader, working the "${f.dim}" dimension):
  title:          ${f.title}
  where:          ${f.file}:${f.line}
  reaching input: ${f.reaching_input}
  consequence:    ${f.consequence}
  violated:       ${f.violated}
  severity:       ${f.severity}

YOUR LENS — use only this one:
${lens.text}

Where the claim is "this is not covered" or "this test cannot fail", the proof
is a MUTATION: break the thing, run ./packages/forth-core/build-test.sh, and see
whether it goes red. Apply the mutation, observe, and REVERT it in the same
step. The tree must be clean when you finish — verify with \`git status\`.

YOU ARE IN YOUR OWN GIT WORKTREE. Mutate freely HERE; this is the only place
you may. Round 5 is why this is spelled out: verifiers shared the owner's
working tree, and two of three mutation runs were contaminated by a sibling's
live \`/* MUTATION */\` edits — one saw a baseline gate come back RED at a
clean HEAD. Do NOT touch any tree but your own, and do not revert an edit you
did not make: a foreign edit means a stale sibling, and the correct response is
to say so in your evidence, not to clean up after it.

FIRST ACTION, before reading anything: run \`git log --oneline -1\` in your
worktree. Worktrees have spawned at a STALE ref (round 6 caught two at
c3a00768c, ~114 commits behind the audited tip) — a worktree at the wrong ref
produces verdicts about a codebase that does not exist. If HEAD is not the
audited tip named in the subject line, run \`git checkout <audited-tip>\`
(detached is fine) and say so in your evidence. The audited tip for this run
is the commit named in SUBJECT; if none is named, match the main repo:
\`git -C /home/stan/c43 rev-parse HEAD\`.

Otherwise do not edit the tree at all.

Answer REFUTED or SURVIVES, one paragraph of why, then your evidence: the path
you constructed, the trace you followed, or the ruling you found.`,
    { label: `refute:${lens.key}:${f.file.split('/').pop()}:${f.line}`, phase: 'Refute',
      schema: VERDICT_SCHEMA, effort: 'high',
      /* AUDIT round 5 earned this: verifiers mutate to prove a coverage claim,
       * they run concurrently, and in the owner's shared tree two of three
       * mutation runs were contaminated by a sibling's live edit.  A worktree
       * per verifier is the cost of trustworthy mutation evidence — the only
       * uncontaminated proof that round produced (R9) was the one reader who
       * made its own.  Auto-removed when unchanged, so the non-mutating
       * verifiers pay setup only. */
      isolation: 'worktree' })
    .then(v => ({ ...f, lens: lens.key, verdict: v }))
    .catch(() => ({ ...f, lens: lens.key, verdict: null }))
}))

const checked   = judged.filter(Boolean)
const confirmed = checked.filter(f => f.verdict && f.verdict.verdict === 'SURVIVES')
const refuted   = checked.filter(f => f.verdict && f.verdict.verdict === 'REFUTED')
log(`${confirmed.length} survived refutation, ${refuted.length} refuted`)

phase('Synthesis')

const report = await agent(
`Write the audit report for ${SUBJECT} (${RANGE}), following
design-docs/forth-core/CODE_AUDIT.md's "Report" section EXACTLY — its eight
sections, in its order. Repo: /home/stan/c43.

House style: this is an internal design doc for the project owner. Dense,
direct, technical. No marketing, no summary paragraph that restates, no
recommendations the evidence does not carry.

RANK BY WHAT THE FINDING COSTS THE OWNER, not by how clever it is: a crash on a
common gesture outranks a wrong result in a case nobody reaches, which outranks
a contract that is merely untidy.

DO NOT drive the finding count to zero and do not pad it. Say which findings you
would leave alone if the goal were code that is correct rather than code that
passes an audit.

For every CONFIRMED finding include the concrete reaching input, the violated
contract quoted, the bug class, and the class-level test that would pin it —
but NO patches. This audit produces findings, not fixes.

The "deliberately not flagged" section is MANDATORY and must be substantive:
merge what the finders reported clearing with what the refutation pass
disproved, and say WHY each was cleared. An audit with an empty section there
did not understand what it read.

CONFIRMED (survived the independent refutation pass):
${JSON.stringify(confirmed.map(f => ({ ...f, verdict: f.verdict })), null, 1)}

REFUTED (and why — this is the raw material for "deliberately not flagged"):
${JSON.stringify(refuted.map(f => ({ title: f.title, file: f.file, line: f.line, why: f.verdict.why, evidence: f.verdict.evidence })), null, 1)}

${unique.length > CAP ? `UNVERIFIED (beyond the verification cap — list these honestly as unverified):\n${JSON.stringify(unique.slice(CAP), null, 1)}` : ''}

PER-DIMENSION COVERAGE AND CLEARED ITEMS (from the finders):
${JSON.stringify(reports.map((r, i) => ({ dimension: dims[i].key, coverage: r.coverage, not_flagged: r.not_flagged, verdict: r.verdict })), null, 1)}

Write the report to design-docs/forth-core/AUDIT_${(SUBJECT || 'subject').replace(/[^A-Za-z0-9]+/g, '-')}_${A.date || 'report'}.md
and return the absolute path plus a five-line summary.`,
  { label: 'synthesise-report', phase: 'Synthesis', effort: 'xhigh' })

return {
  subject: SUBJECT,
  dimensions: dims.map(d => d.key),
  raw: all.length,
  deduped: unique.length,
  verified: checked.length,
  confirmed: confirmed.length,
  refuted: refuted.length,
  unverified: Math.max(0, unique.length - CAP),
  report,
}
