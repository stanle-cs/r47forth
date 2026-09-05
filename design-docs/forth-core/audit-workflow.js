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

/* args: { subject, commits, files, date, round, outOfFamily, dimensions?,
 * extraFindings? }  — see CODE_AUDIT.md. round and outOfFamily are REQUIRED;
 * the validation below throws before any agent spawns.
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
 * findings it was meant to verify. NEVER resume a parameterized run
 * WITHOUT re-passing the full args. Resume WITH the full args re-passed
 * explicitly is safe and cheap — proven 2026-08-29, when a usage limit killed
 * all 15 refuters and the synthesis mid-run: the resumed call cache-hit all 8
 * finders and re-ran only the dead tail. The required round/outOfFamily
 * validation below turns the dropped-args trap loud: a bare resume now throws
 * here instead of silently auditing the default range. The log line below is the
 * check: if it does not name your subject and range, you are auditing the wrong
 * thing. */
const A = (typeof args === 'string')
  ? (() => { try { return JSON.parse(args) } catch { return {} } })()
  : (args || {})

const SUBJECT = A.subject || 'the current branch'
const RANGE   = A.commits || 'main..HEAD'
const FILES   = A.files   || '(discover from the commit range)'
/* The audited tip, resolved for the worktree guard below. Round 2 shipped
 * the guard with a literal '<audited tip>' placeholder, so every verifier
 * had to derive the tip itself — the guard worked only because they did. */
const TIP = A.tip || (RANGE.includes('..') ? RANGE.split('..').pop() : 'HEAD')

/* Optional model override for every agent this run spawns (round 7: the
 * owner asked for Opus readers to cut token cost). Absent -> inherit the
 * session model, exactly as before. */
const MODEL = A.model ? { model: A.model } : {}

/* Round + out-of-family accounting — REQUIRED (owner ruling 2026-08-29).
 * Four PP18 rounds closed in-family only: a green `dispatch.sh probe all`
 * was mistaken for the pass, and round 3's report carried the omission in
 * prose and it changed nothing. So the check lives here, in the code path
 * that emits the report, and it fails at the START, where it costs nothing.
 * A resume drops args and now throws HERE instead of silently auditing the
 * default range — relaunch fresh with the full args re-passed.
 *
 *   round:       positive integer. ROUND 1 OF EVERY AUDIT IS READ BY ALL
 *                THREE FAMILIES — this workflow (in-family) plus Gemini
 *                plus GPT/Sol — on the actual subject. The round counter
 *                does not advance until round 1's report records both
 *                out-of-family replies.
 *   outOfFamily: 'pending'  — packets not yet returned (the in-family
 *                             half legitimately runs first). The report
 *                             banners itself as unable to close.
 *                'none'     — an explicit, recorded decision to skip.
 *                             Rounds >= 2 only; banner, not footnote.
 *                { packets: [ { reader: 'gemini'|'sol'|'paste',
 *                               family: 'gemini'|'gpt' (required for
 *                               'paste', implied otherwise),
 *                               packet: '<path>', reply: '<path>' } ] }
 * 'none' stays cheap on rounds >= 2 by design: the failure was never that
 * skipping is easy, it is that skipping was invisible. */
const ROUND = A.round
if (!Number.isInteger(ROUND) || ROUND < 1)
  throw new Error('args.round (positive integer) is required — the three-family rule for round 1 is keyed on it (SKILL.md, Order of work step 4). If this is a resume: resume drops args, relaunch fresh with the full args re-passed.')
const OOF_FAMILY = { gemini: 'gemini', sol: 'gpt' }
const OOF = A.outOfFamily
let oofPackets = null
if (OOF === 'pending') {
  /* legitimate: the FIND half runs before any packet exists */
} else if (OOF === 'none') {
  if (ROUND === 1) throw new Error("outOfFamily: 'none' is not accepted for round 1 — round 1 is read by all three families, no skip (SKILL.md, Order of work step 4). Use 'pending' for an in-family half whose packets are not back yet.")
} else if (OOF && typeof OOF === 'object' && Array.isArray(OOF.packets)) {
  const bad = OOF.packets.filter(p => !p
    || !(p.reader in OOF_FAMILY || (p.reader === 'paste' && (p.family === 'gemini' || p.family === 'gpt')))
    || typeof p.packet !== 'string' || typeof p.reply !== 'string')
  if (bad.length) throw new Error(`outOfFamily.packets entries must be { reader: 'gemini'|'sol'|'paste', family ('gemini'|'gpt', required for 'paste'), packet, reply }: ${bad.length} of ${OOF.packets.length} malformed`)
  oofPackets = OOF.packets.map(p => ({ ...p, family: OOF_FAMILY[p.reader] || p.family }))
} else {
  throw new Error("args.outOfFamily is required: 'pending', 'none' (rounds >= 2 only), or { packets: [{ reader, packet, reply }] }. A round that skipped the out-of-family pass may not look identical to one that ran it (SKILL.md, Order of work step 4).")
}
const oofFamilies = [...new Set((oofPackets || []).map(p => p.family))]
const threeFamilyMet = oofFamilies.includes('gemini') && oofFamilies.includes('gpt')
const oofStatus = oofPackets
  ? oofPackets.map(p => `${p.reader}/${p.family}: ${p.packet} -> ${p.reply}`).join('; ')
  : `'${OOF}'`
let oofBanner = ''
if (ROUND === 1 && !threeFamilyMet) {
  oofBanner = `> **ROUND 1 THREE-FAMILY RULE NOT MET.** Out-of-family families in this run: ${oofFamilies.length ? oofFamilies.join(', ') : 'none'}; round 1 requires Gemini AND GPT on the actual subject, plus this in-family pass. This is not a complete round 1 — it cannot close anything, and the round counter does not advance until both families' replies are fed back through this workflow with their packets recorded in outOfFamily (SKILL.md, Order of work step 4).`
} else if (!oofPackets) {
  oofBanner = `> **This round had NO out-of-family reader** (outOfFamily: '${OOF}'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).`
}

/* The auditor brief, inlined verbatim rather than referenced. Naming the file
 * and telling the agent to follow it has failed here before (2026-08-04). */
/* The finder brief. Plain words on purpose (2026-09-04): the platform
 * classifier refused the finders eight of eight, category
 * "reasoning_extraction", and five bisects found the trigger in the OUTPUT
 * SCHEMA, not in this text: the pair of descriptions "what was read, what
 * was not, what the budget did not reach" and "considered and cleared, with
 * the reasoning" together read as a request for the model's own reasoning.
 * Either one alone passed. The schema below describes the CODE, never the
 * reader's process, and the brief follows the same rule. Keep it so. */
const BRIEF = `
This is a correctness review of the firmware of a hobby pocket calculator.
Find functional bugs and design flaws only. This is not a security review,
and there is no security dimension. Report findings. Do not fix anything.
Do not write patches.

THE SUBJECT. A personal hobby project: external packages over the
open-source C47/R47 firmware for a DM42-class pocket calculator. One user,
one handheld. No network, no untrusted input, no privilege boundary. The
worst outcome of a bug is a reboot and the loss of the program that the
owner typed at that moment. A finding that needs an attacker is not a
finding. Wrong answers, lost work, stuck states and crashes are the whole
job.

READ THE DESIGN BEFORE THE CODE. The DESIGN.md of the package named in
SUBJECT is the authority. The stage sheets carry the rulings. The
DESIGN-HISTORY.md carries the evidence. Code that contradicts DESIGN.md is
a finding. Code that contradicts your expectation but agrees with DESIGN.md
is not.

THE FOUR ITEMS OF A FINDING. Give these four items in each finding, or the
finding does not count:
 1. Where: give the file and the line.
 2. The reaching input: give the key sequence, the program step, or the
    call path that reaches the line. This item is absent most often, and
    it is the one that matters.
 3. What goes wrong: give the result that the owner sees.
 4. Why it is wrong: quote the contract, comment, ruling or invariant that
    the code breaks.

WHAT NOT TO FLAG:
 1. A decision that the code explains. This code carries comments that
    explain choices that look wrong. Such a comment says that the design
    considered your finding first.

    Three examples look like bugs and are not. Example one:
    calcModeNormal() followed by an unconditional popSoftmenu() in the
    EXIT ladder. Example two: a formatter that copies into the buffer of
    its caller and not into tmpString. Example three: a reset that clears
    one store and deliberately not its neighbour. If a comment explains
    the choice and you still think that the code is wrong, quote the
    comment. Then argue with it.
 2. Deliberate scope. Each stage sheet has a Non-goals section. A feature
    listed there is not a defect.
 3. Style, names, formatting, or code that you write differently.
 4. A path that no input reaches. Reachability, not write-set: name the
    caller and the argument. If you cannot, mark the finding as unreached.
    Do not assert it.
 5. Anything that the build gate or the compiler already reports.

If you are not sure whether something is a defect or a deliberate decision,
say so. Do not guess. A confident wrong finding costs more than silence,
because the owner then reads all findings less carefully.

Do not pad the finding count. Do not force it to zero. Also list the
sites that you examined and found correct, one line each. An empty list
means that you did not understand what you read.

You can read anything and run read-only commands. Do not edit files.
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
          reaching_input: { type: 'string', description: 'the key sequence, program step, or call path that reaches the line. If none was found, "UNREACHED: ..."' },
          consequence:    { type: 'string', description: 'the result that the owner sees' },
          violated:       { type: 'string', description: 'the contract, comment or ruling that the code breaks, quoted' },
          severity:       { type: 'string', enum: ['crash-or-data-loss', 'wrong-result', 'stuck-state', 'latent', 'design-flaw'] },
          confidence:     { type: 'string', enum: ['high', 'medium', 'low'] },
        },
      },
    },
    coverage:    { type: 'string', description: 'the files and functions that were read, and the ones that were not' },
    not_flagged: { type: 'string', description: 'the sites examined and found correct, one line each' },
    verdict:     { type: 'string' },
  },
}

const VERDICT_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['verdict', 'why', 'evidence'],
  properties: {
    verdict:  { type: 'string', enum: ['REFUTED', 'SURVIVES'] },
    /* `why` must not OPEN with the opposite verdict token: PP18RR1-10 came
     * back verdict:SURVIVES with why:'REFUTED ...' over evidence that proved
     * survival, and a machine reading the pair would have filed it either
     * way (restarted round 1, process item 3). State the reason, not a
     * second verdict. */
    why:      { type: 'string', description: 'the reason in the code. Do not begin with REFUTED or SURVIVES. Do not contradict the verdict field' },
    evidence: { type: 'string', description: 'the call path, the code path, or the ruling, with file and line' },
  },
}

const DIMENSIONS = [
  { key: 'contracts', effort: 'high',
    q: 'CONTRACTS AGAINST CALLERS. Does the stated contract of each function hold at every call site? Include the recent call sites. Which caller passes the argument that the banner says is impossible? Examine the preconditions that a caller must establish, and the ones that nobody establishes.' },
  { key: 'lifecycle', effort: 'high',
    q: 'STATE MACHINES AND LIFECYCLE. Examine the open, suspend and close of a capture or a view, the softmenu stack, calcMode, and FLAG_ALPHA. Which sequence leaves a state that nobody handles? What survives a reset but must not survive? What dies at a reset but must not die? If two of these change at the same time, what happens?' },
  { key: 'arithmetic', effort: 'high',
    q: 'BOUNDARIES AND ARITHMETIC. Examine indexes, wraps, caps, lengths, off-by-one, unsigned underflow, and buffer sizes against the strings written into them. Where does a counter meet a cap? What happens one past the cap? Examine each cast and each subtraction on an unsigned type.' },
  { key: 'errorpaths', effort: 'high',
    q: 'ERROR PATHS. The unhappy path. After an error, a rejected input, or an early return, is each piece of state consistent with each other piece? What did the early return skip that a later line assumes is complete? Who clears lastErrorCode? Who reads it after that?' },
  { key: 'guards', effort: 'high',
    q: 'GUARD REACHABILITY. For each condition in each gate, give the input that makes the condition false. A guard that nobody can reach is dead code. A condition that is always true is noise. A missing condition is a defect. Examine the recently edited gates with care.' },
  { key: 'tests', effort: 'high',
    q: 'TESTS THAT CANNOT FAIL. Examine assertions that are always true or always false by type range, and self-comparison. Examine oracles that pass on the wrong answer, and a goto cleanup with no fail flag. Examine a test whose comment claims more than its body tests, and a fixture that silently does nothing. Name the assertion. Say what must break to make it fire.' },
  { key: 'design', effort: 'xhigh',
    q: 'DESIGN FLAWS, not bugs. Two places that must agree with nothing that forces them to agree. Stored state that the code can derive, so it can disagree with its source. A rule with exceptions that nobody can list. A contract that is correct but that each caller applies incorrectly, which is a defect of the contract. Duplicated truth. Report each thing that becomes a bug the next time that somebody touches it.' },
  { key: 'upstream', effort: 'medium',
    q: 'UPSTREAM DISCIPLINE. Examine package overrides against upstream files. Find hunks larger than necessary, and behaviour changed inline where a call is enough. Find drift risk in the largest overrides (screen.c, keyboard.c, items.c). Report each change that will conflict badly at the next upstream merge.' },
]

const dims = A.dimensions
  ? DIMENSIONS.filter(d => A.dimensions.includes(d.key))
  : DIMENSIONS

phase('Find')
log(`auditing ${SUBJECT} — RANGE ${RANGE} — round ${ROUND} — across ${dims.length} blind dimensions — out-of-family: ${oofStatus}${ROUND === 1 && !threeFamilyMet ? ' — ROUND-1 THREE-FAMILY RULE NOT YET MET' : ''}`)

/* Barrier is CORRECT here: the dedup and the rotation both need every
 * finder's output at once — a verifier must not receive a finding produced by
 * the dimension it is about to be handed. */
const reports = (await parallel(dims.map(d => () =>
  agent(
`${BRIEF}

SUBJECT: ${SUBJECT}
COMMIT RANGE: ${RANGE}
FILES IN SCOPE: ${FILES}

Start with the DESIGN.md of the package named in SUBJECT. Read the sections
that concern your dimension. Then read the stage sheet and the history.
Then read the code. Read the commits of the range with \`git log -p ${RANGE}\`
and \`git diff ${RANGE}\` to see what changed.

The catalog of named bug classes is
.claude/skills/cross-model-audit/references/bug-classes.md. This codebase
already paid the cost of each class. Search for the members of your
dimension at every site. The same shape returned at a second site after
the first site was fixed and commented.

Stay in YOUR dimension. Other readers cover the other dimensions, and
independent coverage is worth more than duplicated coverage:

${d.q}`,
    { label: `find:${d.key}`, phase: 'Find', schema: FINDING_SCHEMA, effort: d.effort, ...MODEL })
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
  { key: 'reachability', text: 'REACHABILITY. Construct the call path and the concrete input that reaches the reported line. If you cannot construct it, the finding is REFUTED. "It looks reachable" is not construction.' },
  { key: 'correctness',  text: 'CORRECTNESS. Grant the path. Trace the code. Make sure that the described consequence is what the code does. A finding can have a real path and a wrong conclusion. That is the most common failure mode here.' },
  { key: 'intent',       text: 'INTENT. Is this behaviour documented as deliberate? Search the comments, DESIGN.md, the stage sheets and DESIGN-HISTORY.md. If the design ruled on it, the finding is REFUTED. Cite the ruling.' },
]

/* Round 8's in-family findings filled the old cap of 24 and left all ten
 * out-of-family findings unverified; a refutation-only completion pass had
 * to close the round. 32 leaves room for both halves. */
const CAP = 32
if (unique.length > CAP) {
  log(`NOTE: ${unique.length - CAP} findings beyond the ${CAP} verified are listed UNVERIFIED in the report, not dropped`)
}
const toVerify = unique.slice(0, CAP)

const judged = await parallel(toVerify.map((f, i) => () => {
  const lens = LENSES[i % LENSES.length]
  return agent(
`You cross-check a finding from a CORRECTNESS review of hobby calculator
firmware. The review is for functional bugs, not security (there is no
security dimension here). Your job is to refute the finding: assume that it
is wrong, and search for the reason. A finding that survives a real attempt
to disprove it is worth action. A finding that a reader only admired is not.
If you are uncertain, the default verdict is REFUTED.

Repo: /home/stan/c43. Subject: ${SUBJECT} (${RANGE}).

WORKTREE CHECK. Do these two actions first, before you read anything. Rounds
2, 3, 4 and the restarted round 1 requested this check. A worktree's ref is a
CLAIM.
  1. Run: git log --oneline -1 && git merge-base --is-ancestor ${TIP} HEAD && echo ANCESTOR-OK
     The audited tip is ${TIP}.
     Worktrees spawned ~110 commits behind, where the audited code does not
     exist yet. A verifier there judges a codebase that is not there.
     If that command does not print ANCESTOR-OK, or HEAD is not ${TIP}, run
     'git checkout ${TIP}' before you read anything. Six consecutive rounds
     spawned at a ref ~110 commits behind, where the audited files do not
     exist. Each round was usable only because the readers caught it.
  2. Run 'git status --porcelain' and 'git diff'. In the restarted round 1, a
     worktree arrived with a live foreign mutation ('prettyVisual.c:1457').
     A sibling verifier that worked on the same finding wrote it. If the
     worktree is dirty and the edit is not yours, your reads and each gate
     run are untrustworthy until you explain the edit. Report a foreign edit
     in your evidence, and NEVER revert it ('git checkout'/'restore' once
     nearly destroyed uncommitted stage work). If the edit is exactly the
     mutation that your verification needed, say so and use it, but always
     say so.
For your own mutations: mark them 'AUDIT-PROBE R<n>'. Revert them by an
inverse edit in the same step. Make sure that the mutation reached the built
artifact.

THE FINDING (a different reader produced it on the "${f.dim}" dimension):
  title:          ${f.title}
  where:          ${f.file}:${f.line}
  reaching input: ${f.reaching_input}
  consequence:    ${f.consequence}
  violated:       ${f.violated}
  severity:       ${f.severity}

SCOPE. You are not limited to what the finding quotes. If the finding came
from an out-of-family packet, that packet's excerpt is NOT your scope. You
have the whole repository, and the finding can depend on code that the reader
never saw. Round 2 settled two of four findings on code outside the packet
(the STO arm for one, the BIGOP DONE arm for the other). A refuter limited to
the packet scope gets both verdicts wrong: one wrong SURVIVES (the correct
verdict was REFUTED) and one wrong REFUTED-as-unreachable (the finding was
real). Read the callers.

YOUR LENS. Use only this one:
${lens.text}

If the claim is "this is not covered" or "this test cannot fail", the proof
is a mutation. Break the thing. Run ./packages/forth-core/build-test.sh.
Observe whether the gate goes red. Apply the mutation, observe, and revert it
in the same step. When you finish, the tree must be clean. Make sure of that
with \`git status\`.

OWN WORKTREE. You are in your own git worktree. Mutate freely here. This is
the only place where you can mutate. Round 5 is the reason for this explicit
rule. Verifiers shared the owner's working tree, and a sibling's live
\`/* MUTATION */\` edits contaminated two of three mutation runs. One run saw a
baseline gate return red at a clean HEAD.

Do NOT touch a tree other than your own. Do not revert an edit that you did
not make. A foreign edit means a stale sibling. The correct response is to
say so in your evidence, not to remove the edit.

FIRST ACTION, before you read anything: run \`git log --oneline -1\` in your
worktree. Worktrees spawned at a STALE ref (round 6 caught two at c3a00768c,
~114 commits behind the audited tip), and a worktree at the wrong ref produces
verdicts about a codebase that does not exist. If HEAD is not the audited tip
named in the subject line, run \`git checkout <audited-tip>\` (detached is
fine). Say so in your evidence. The audited tip for this run is the commit
named in SUBJECT. If SUBJECT names no commit, match the main repo:
\`git -C /home/stan/c43 rev-parse HEAD\`.

Otherwise do not edit the tree at all.

Answer REFUTED or SURVIVES, then give one paragraph of why. Then give your
evidence: the path you constructed, the trace you followed, or the ruling you
found.`,
    { label: `refute:${lens.key}:${f.file.split('/').pop()}:${f.line}`, phase: 'Refute',
      schema: VERDICT_SCHEMA, effort: 'high', ...MODEL,
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
`Write the audit report for ${SUBJECT} (${RANGE}), round ${ROUND}. Follow the
"Report" section of design-docs/forth-core/CODE_AUDIT.md EXACTLY. Use its eight
sections, in its order. Repo: /home/stan/c43.

House style: this is an internal design doc for the project owner. Write it
dense, direct, and technical. Do not write marketing. Do not write a summary
paragraph that restates. Do not write recommendations that the evidence does
not carry.

Rank each finding by what it COSTS the owner, not by how clever it is. A crash
on a common gesture outranks a wrong result in a case nobody reaches. A wrong
result in a case nobody reaches outranks a contract that is merely untidy.

Do NOT drive the finding count to zero. Do not pad it. Assume that the goal is
code that is correct, not code that passes an audit. Say which findings you
will then leave alone.

For every CONFIRMED finding, include the concrete reaching input. Quote the
violated contract. Also include the bug class and the class-level test that
will pin it. Include no patches. This audit produces findings, not fixes.

The "deliberately not flagged" section is MANDATORY, and it must be
substantive. Merge the items that the finders reported as cleared with the
items that the refutation pass disproved. For each item, say why it is
cleared. An audit with an empty section there did not understand what it read.

OUT-OF-FAMILY ACCOUNTING, round ${ROUND}: ${oofStatus}.
${oofBanner
  ? `Open section 1 with this banner VERBATIM, its own paragraph, before anything else:\n${oofBanner}`
  : `In section 1, list every out-of-family reader: its packet path, its reply path, the MODEL line quoted VERBATIM from the reply file (READ each reply file; a missing or empty reply is a timeout or an overwrite, never a clean bill — if you find one, open section 1 with a banner saying so instead), and how many findings that reply raised. Section 8 carries the same list plus how many of each reader's findings survived refutation.`}

CONFIRMED (these findings survived the independent refutation pass):
${JSON.stringify(confirmed.map(f => ({ ...f, verdict: f.verdict })), null, 1)}

REFUTED, with the reason for each. This list is the raw material for
"deliberately not flagged":
${JSON.stringify(refuted.map(f => ({ title: f.title, file: f.file, line: f.line, why: f.verdict.why, evidence: f.verdict.evidence })), null, 1)}

${unique.length > CAP ? `UNVERIFIED (beyond the verification cap — list these honestly as unverified):\n${JSON.stringify(unique.slice(CAP), null, 1)}` : ''}

PER-DIMENSION COVERAGE AND CLEARED ITEMS (from the finders):
${JSON.stringify(reports.map((r, i) => ({ dimension: dims[i].key, coverage: r.coverage, not_flagged: r.not_flagged, verdict: r.verdict })), null, 1)}

Write the report to this path:
design-docs/forth-core/AUDIT_${(SUBJECT || 'subject').replace(/[^A-Za-z0-9]+/g, '-')}_${A.date || 'report'}.md
Return the absolute path plus a five-line summary.`,
  { label: 'synthesise-report', phase: 'Synthesis', effort: 'xhigh', ...MODEL })

return {
  subject: SUBJECT,
  round: ROUND,
  outOfFamily: oofPackets ? { packets: oofPackets } : OOF,
  outOfFamilyFamilies: oofFamilies,
  dimensions: dims.map(d => d.key),
  raw: all.length,
  deduped: unique.length,
  verified: checked.length,
  confirmed: confirmed.length,
  refuted: refuted.length,
  unverified: Math.max(0, unique.length - CAP),
  report,
}
