# Writing user-facing prose as Stan

Load this before drafting or revising ANY prose published under Stan's
name: forum posts, README, INSTALL, MR descriptions, upstream reports.
It records what the 2026-08-03 package-post cycle cost to learn: six
model audit passes, four human review rounds, and a voice rebuild.
forum/DESIGN.md stays the authority on process; this skill is the
distilled method and the traps.

## The one-sentence version

Match Stan's measured voice from his corpus, follow his content
pedagogy, verify every fact against source, and treat his read as the
only gate that can say yes.

## Voice truths (all measured, 2026-08-03)

The ONLY authentic voice anchor is `forum/reference/reddit-trungdle.md`
(1,696 comments + 80 submissions, 2015-2026). Every project doc —
INSTALL.txt, RELEASE_README.md, old forum posts, old drafts — is
AI-authored and carries no voice authority (Stan's explicit ruling).
Use them for facts only.

His numbers, per 100 words unless noted:
- first person 3.84, second person 2.31, contractions 3.45
- ", and" mid-sentence chains: 0.25 (a polished draft ran 1.45 — six
  times his rate — and he spotted it on read)
- questions: 11% of sentences overall; in explanations, roughly one
  per 200 words
- sentence-length CV 0.72; mean 12.5 words
- double negation: 1.1% of sentences, and NEVER definitional

How he explains a new thing (from his 175 explanatory comments):
- short independent sentences; But/And/So at the FRONT of a sentence,
  not mid-chain
- question-then-answer: "Why not just copy the file out of your
  checked-out tree? There's a chance..."
- direct you-instruction ("you can", "you need") and I-hedges ("I
  think", "I'd guess", "probably" placed NEXT TO THE VERB, never
  fronted)
- parenthetical asides (about one per 300 words)
- almost never: "for example", "basically", "imagine"

Register ruling: technical prose is either CUT AND DRY (short factual
sentences, ordinary verbs, zero flourish) or EXPLAINED IN HIS STYLE
(the constructs above). The polished-technical-writer middle register
is the named failure mode.

## The trap catalog (every one reached a draft this cycle)

Constructions that feel good and are wrong:
- definition by absence ("It declares nothing, no meson.build, no file
  list") — he defines by what a thing IS and what you DO
- trailing contrast tails ("...directories, never your flat working
  area") — survived seven model flags on an accuracy defense, died to
  his read; keep the FACT, drop the construction
- punchy fragment closers ("But that one gets its own post."),
  colon-flourishes ("which runs on this:"), announce-then-elaborate
  bullets, sentences wrapped AROUND a code block ("Then: [code] sorts
  your files")
- semicolon appositions, formal verbs ("ships", "produces no
  artifact", "materializes", "catch drift", "release lap") — plain
  verbs: "you get no zip", "copies the file in"
- passives where he has a stake — "I made the patches", never "the
  patches were made"
- metric-chasing overcorrection: staccato fragments added to lift CV
  read as edited-to-pass; both auditors flagged them
- ", so" is the house crutch: cap ~1 per 8 sentences
- 79-column hard wrap: phpBB renders newlines as breaks — one line per
  paragraph, only code blocks keep internal newlines

## Content pedagogy (his review rules, §6 structure ruling)

- The built interface leads. If a convenience entry point exists
  (./package), it is introduced first and every instruction rides it;
  raw tools get one mention as what's underneath.
- Every recommendation is taught WITH its reason (materialize exists
  because a copied file from a moved tree bakes upstream's changes
  into your patch).
- Testing is explicit, never implied.
- Ordering/composition rules get a concrete worked example.
- Anticipate the reader's next question and answer it inline (his own
  question about copy-vs-materialize became a post sentence).

## Process (order matters)

1. Facts first: verify every figure, command, and claim against source
   before writing (counts change; §6 itself carried stale 14/13).
   Command syntax comes from --help, mechanics from the tool's code.
2. Draft between his registers. No model pass can grant a yes.
3. Mechanical battery:
   - `python3 forum/aiaudit.py <draft>` — judge flags, never zero them
   - `python3 forum/framescan.py <draft>`
   - `python3 forum/voicematch.py <draft>` — compare against the
     numbers above
   - `python3 forum/voicematch.py --attest <draft>` — ordinary-word
     pairings he never wrote; fix real clusters, ignore artifacts
4. Cross-model audits WITH the corpus: build the prompt from
   PROMPT_AUDIT.md + an ~8k corpus sample + the draft, prepend "pure
   reading task, no tools". Gemini lane:
   `agy -p "$(cat prompt)" --model gemini-3.1-pro-high`. ChatGPT lane:
   `codex exec --sandbox read-only --skip-git-repo-check "$(cat prompt)"`.
   Fresh invocation per pass; absolute paths (a cd once emptied a
   prompt). Expect the seesaw: passes flag earlier passes' cures;
   judge, record dispositions, stop when findings stop being real.
5. Stan reads. His findings outrank every earlier disposition. When he
   offers a sentence, verify the claim inside it, then use his shape
   nearly verbatim. When unsure, ask him.
6. Log every round in a <draft>-AUDITLOG.md beside the draft:
   dispositions, ruled keeps, what died and why.

## What the models can and cannot do

Six passes converged on "a human wrote this" while the text sounded
nothing like Stan and taught the wrong interface. Model audits police
tells; voicematch polices Stan-ness mechanically; ONLY human review
catches content pedagogy. Never present model-clean as done.
