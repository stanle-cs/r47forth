# Skill defect handoff — the out-of-family pass can be skipped silently

**For:** Fable (judgment stage — this is a process-design change, not mechanical work)
**Subject:** `.claude/skills/cross-model-audit/SKILL.md` and
`design-docs/forth-core/audit-workflow.js`
**Reported by:** Claude Opus 5, 2026-08-29, after being told twice by the owner
**Status:** defect confirmed by four rounds of evidence; fix proposed, NOT applied

---

## 1. The defect in one sentence

A cross-model audit round can skip the out-of-family pass entirely and still
produce a complete-looking report with a verdict, a ranked finding list and
an exit-criterion statement — nothing in the skill, the workflow or the
report template requires the round to say that it did.

## 2. What the skill already says (it is not silent)

- `SKILL.md:16` — the process is defined as "blind in-family finders per
  dimension, **at least one out-of-family reader**, every finding handed to a
  reader who did not produce it".
- Order of work step 4 — "**This pass is what closes an audit**; if the
  automation fails, PASTE the packet into a fresh session by hand rather than
  skipping the pass."
- `SKILL.md:207` — the exit criterion requires two consecutive clean rounds,
  "**at least one of them out-of-family on the actual subject**".

The wording is not the problem. The problem is that nothing *enforces* it and
nothing *records its absence*, so a round that skips it is indistinguishable
from a round that did it.

## 3. The story — what actually happened

Four consecutive audit rounds on the `pretty-print` package's VISUAL walker
(`pretty-print/stage-pp18`), 2026-08-28 to 2026-08-29.

**Rounds 1, 2, 3 and round 4's first half ran in-family only.** Confirmed
finding counts: **16, 8, 7, 11**. Every round produced a full report in
`CODE_AUDIT.md`'s eight sections, ranked by owner cost, with an exit-criterion
verdict. Every round *looked* complete.

**The mechanism of the omission, stated plainly so the fix targets the right
thing:**

1. In round 1 the operator ran `dispatch.sh probe all`, got
   `identity OK — MODEL: Gemini 3.1 Pro (High)` and `MODEL: GPT-5`, and
   **mistook the readiness check for the pass.** The setup step succeeded, so
   it left no loose end to notice.
2. The in-family half is **one tool call** that returns a 1,000–1,500 line
   artifact. The out-of-family half is manual: extract whole functions, write
   orientation, lint, dispatch, translate reply line numbers. The loud
   automated half crowded out the quiet manual one, four rounds running.
3. Each round's findings set the next round's agenda. A process step not
   attached to a finding has nothing pulling on it.
4. **Round 3's own report said so** — "no out-of-family reader ran, so round 4
   should be out-of-family" — and the operator relayed that sentence to the
   owner verbatim and *still* filed it as a plan for the next round rather
   than as a defect in the three rounds already closed.

Point 4 is the important one for the fix. The information was present, in
writing, in the report, and it changed nothing. A fix that only *reports*
harder will not work.

## 4. What the omission cost, measured

The out-of-family pass was finally run on 2026-08-29 after the owner asked
twice. Three readers, two packets, both built from `packet-template.md` and
linted:

| reader | family | result |
|---|---|---|
| Gemini 3.1 Pro (High) | Gemini | 1 finding: premature name invention refuses a drawable program |
| Gemini 3.7 Flash (High) | Gemini | 0 findings; explicitly considered the above and did NOT flag it |
| Sol / GPT-5 (medium) | GPT-5 | 4 findings, incl. one silent **wrong picture** |

**What three in-family rounds had missed:**

- **A wrong picture.** `ppvIntegral` checks only `ctx->binding` and never calls
  `ppvNameInSubtree` on its own limits, so an integral over `x` whose lower
  limit contains a free `x` draws the bound and the free variable identically.
  This is the same shape as a finding the in-family rounds *did* make
  (PP18-9) and then applied to invented names only.
- **Three over-refusals**, including one Sol reproduced with a SUM where the
  operator had only been looking at derivatives.
- **A ruling that is literally false of the code** — "scope is 'about to be
  drawn inside me', not 'already built'" — because the implementation walks
  `astUsed`, an allocation history, not a live set.

**The two-family disagreement is itself evidence for the rule.** Flash and Pro
are the same family and reached opposite verdicts on the same packet; Sol, a
different family, independently confirmed Pro. A single out-of-family reader
is the minimum the skill asks for and it was worth more than three in-family
rounds on this axis.

## 5. Why "remember to do step 4" is not the fix

The operator read the skill, invoked it correctly, ran steps 1, 2, 3, 5, 6, 7
and 8, and skipped step 4 four times while reporting in writing that it had
been skipped. This is the same shape as this project's recorded 2026-08-04
failure — *"'invoke the skill and follow it' alone has failed"* — and the same
remedy applies: **make the omission impossible to complete silently.**

## 6. The change to make

Two parts. Both are small. Part A is the load-bearing one.

### Part A — `audit-workflow.js` must be told, explicitly, what happened

Add a required `outOfFamily` field to `args`. The script refuses to emit a
final report without it. Accepted values:

```js
outOfFamily: { packets: [ { reader: 'gemini'|'sol'|'paste',
                            packet:  '<path to the packet file>',
                            reply:   '<path to the reply file>' }, ... ] }

outOfFamily: 'none'      // an explicit, recorded decision to skip
```

Behaviour:

- **Missing or malformed** → the script throws before spawning any agent, with
  a message naming SKILL.md step 4. Failing at the START is deliberate: it
  costs nothing, where failing at the end wastes a full round.
- **`'none'`** → the run proceeds, and the report's §1 opens with a banner,
  its own line, not a footnote:
  `> **This round had NO out-of-family reader.** It cannot close the audit
  > (SKILL.md:207) and its verdicts carry one family's blind spots.`
- **`{packets: [...]}`** → §1 lists each reader, packet path and reply path,
  and the report states how many findings came from each.

The point is that `'none'` is *available*. A round may legitimately skip the
pass — during a fix wave, or when the subject is too small to packet. What it
may not do is skip it and look identical to a round that did not.

### Part B — SKILL.md wording, two additions

1. In **The reader pool**, after the `dispatch.sh probe all` sentence, add:

   > A probe is not a pass. `probe all` verifies the drivers answer; it sends
   > no packet and produces no finding. Four rounds were closed on the
   > strength of a green probe (2026-08-29).

2. In **Exit criterion**, after the existing paragraph, add:

   > A round with no out-of-family reader does not count toward the two, and
   > says so in its own report. Track this across rounds: three in-family
   > rounds are one reader's opinion repeated, whatever their finding counts
   > look like.

## 7. What NOT to do

- **Do not make `'none'` hard to use, or add friction to it.** The failure was
  not that skipping is too easy; it was that skipping is *invisible*. Friction
  would push the next operator toward a token packet, which is worse than an
  honest skip.
- **Do not require all three readers per round.** The skill says "at least
  one", Gemini is named the workhorse and Sol is specifically for
  self-contained design packets. Requiring three would make the honest path
  expensive and the dishonest path attractive.
- **Do not add a checklist to the report template alone.** Round 3's report
  already carried the omission in prose and it changed nothing. The check has
  to be in the code path that produces the report.
- **Do not touch the in-family workflow's dimensions or refutation.** They are
  working; the finding counts above are theirs.

## 8. How to verify the change

1. Run `audit-workflow.js` with `args` lacking `outOfFamily` — it must throw
   before spawning an agent, and name step 4.
2. Run it with `outOfFamily: 'none'` on a trivial subject — the report must
   carry the banner in §1, above the finding list, not in a closing section.
3. Run it with one `{reader:'gemini', packet, reply}` entry — §1 must name the
   reader, both paths, and its finding count.
4. Re-read `SKILL.md` end to end and confirm the two additions do not
   contradict the Order of Work or the growth rule.

## 9. Provenance

Everything in §4 is reproducible from files in the tree at
`pretty-print/stage-pp18`:

- the four in-family reports, `design-docs/forth-core/AUDIT_*2026-08-2[89]*.md`
- the packets, `/tmp/pkt/derivchain.md` and `/tmp/pkt/rulings.md`, both linted
  by `packet_lint.py` with no HARD hits (18.3 KB and 24.9 KB, both inside the
  proven range)
- the replies, `/tmp/pkt/derivchain.gemini.reply.md` and
  `/tmp/pkt/rulings.sol.reply.md`, each opening with a verified `MODEL:` line

The out-of-family findings themselves are NOT fixed at the time of writing and
are not this handoff's subject; they belong to the package's audit trail. This
document is only about the process defect that let four rounds close without
them.
