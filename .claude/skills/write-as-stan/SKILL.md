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

## Added 2026-08-04 (release-README cycle, Stan's rulings)

- **When a fact inside his prose dies, the REPLACEMENT is his call.**
  Deleting the dead claim is mechanical; choosing what argument (or no
  argument) stands in its place is voice and opinion under his name.
  Bring him the options — keep-with-truth, different example, delete the
  passage — with a recommendation. Do not silently substitute an
  argument he never made. (The x²-untypeable claim died to his read;
  the silently-substituted "saves keys" pitch then died too, and the
  ruled outcome was deletion.)
- **Checkable behavior claims get sim-verified before they go in his
  prose.** Facts-first now explicitly includes the run-sim path: if the
  simulator can show the claim on the LCD, capture it before asserting
  it. His two catches this cycle (register, then fact) both survived the
  full scanner + cross-model battery — the machines endorse; only
  verification and his read gate.

## Added 2026-08-10 (forth v0.3 update post, Stan's structural rewrite)

His final edit restructured the whole post; the full shape is ruled in
forum/DESIGN.md §2 ("Update-post structure"). What it teaches about
drafting:

- **Update posts are changelog-led.** Versioned heading, no intro,
  list → attachment → short prose per section. The pipeline draft had
  it inverted (prose tour first, changelog last) and he flipped it.
- **He deletes hypophora on sight.** Two question-then-answer
  transitions were corpus-attested, measured toward his 11% question
  rate, and kept through two audit rounds with reasons logged. He cut
  both. The corpus stat describes Reddit arguments, not his release
  notes. Register beats corpus statistics.
- **Changelog bullets stay impersonal.** Both audit lanes pushed
  "are rejected" toward "I made the interpreter refuse"; he reverted
  to "The interpreter refuse the compare tests". First-person is for
  prose where he acts ("after I define SQ"), never for bullets.
- **Parenthetical asides carry reasons and scope.** "( ENTER ran the
  line before the console landed, doesn't work well in new design)",
  "(if forth exceeds max R47 stack size)". This is his aside habit
  doing changelog work.
- **Boilerplate does not repeat across posts.** Target, base, licence
  and the sim-only disclosure live in the announcement post; the
  update post carries only repo + build + disclaimer.
- **Write clean, let him roughen.** His posted grammar ("The
  interpreter refuse") is his ESL fingerprint. Drafts neither polish
  his edits nor imitate the roughness.

## Added 2026-08-25 (undo-history v0.1 rejection, framework rebuilt)

Stan rejected a draft that had passed both scanners, voicematch rates and
three rounds of the two-lane audit pool: "The writing sucks, and it
doesn't sound like me at all. The framework developed didn't
appropriately capture my own writing style, from the data saved." His
ruling on the fix: rebuild from the Reddit data.

What the rebuild established, all measured from the corpus:

- **The old method was subtractive and that is the root failure.**
  Deleting AI tells converges on polished-plain prose that belongs to
  nobody. His voice is a set of PRESENT constructions: first-person
  walkthroughs, hedges beside the verb, direct-you, questions inside
  explanations, everyday analogies, enthusiasm, warm asides. A draft
  with zero of these passes every scanner and fails his read.
- **Several old "his numbers" claims were lore, not data.** "for
  example" appears 11 times in the corpus (the old skill said almost
  never). "Thus", "However", "Moreover" are in-voice. Questions run
  1.33/100w including explanatory hypophora — but see the r2 section
  below for what that licenses in a post: content-bearing reader
  questions only, never segue formulas.
- **New reference set** (all LOCAL-ONLY beside the corpus):
  `forum/reference/stan-voice-profile.md` (measured rates, the
  construction catalog with corpus quotes, the grammar fingerprint,
  the register mapping) and `forum/reference/stan-exemplars.md`
  (30 full explanatory comments, ~270 words average). The drafter
  writes WITH the exemplars open, imitating construction by
  construction. Never lift a sentence.
- **New tool**: `forum/voicematch2.py <draft> [--register release|chat]`
  measures construction PRESENCE against the live corpus and reports
  which of his moves are missing, plus sentence-skeleton support.
  Calibration: the rejected draft scores 6 of 10 constructions MISSING
  and 27/30 unsupported skeletons. It joins the mechanical battery as a
  blocking check: any MISSING row sends the draft back to step 2.
- **Fact-sheet path.** When he wants to write the post himself, deliver
  a verified fact inventory (numbers, commands, captions, required
  boilerplate) and fact-check his text afterward; the pipeline's
  drafting half stands down.

## Added 2026-08-25, same day (r2 rejection: construction stuffing)

The first draft under the rebuilt framework satisfied every presence
row and Stan rejected it too: "Better, but too many AI-generated tell
tales slipped through. Still a shit framework." The failure inverted:
v0.1 had none of his constructions, r2 had all of them AS ORNAMENTS.

- **Constructions are licenses, not quotas.** voicematch2 floors are
  minimums of opportunity; writing TO the tool produces stuffing, which
  reads as AI exactly like absence does. The tool now carries CAPS
  (STUFFED/FORBIDDEN are as blocking as MISSING) and a third register:
  `release` for announcements, `update` for changelog-led posts (owe NO
  floors — his published v0.3 artifact has none of them), `chat` for
  replies.
- **The question rule, finally correct.** His published v0.3 post
  carries TWO questions and one OPENS a paragraph — position and count
  were never the tell. Both of his are the reader's actual mechanism-
  question, answered at once ("Why keep a failed line in history? You
  get it back with one press", "Where do the lines go? There's a hidden
  program named FHIST"). What died to his read, twice, are presentational
  SEGUES that frame a tour instead of asking anything: "So what are you
  looking at?", "What about states too big to keep?". Formula segues are
  forbidden in any position; two content-bearing questions is the
  ceiling.
- **Tells that reached r2 unflagged, now aiaudit classes:** twist tails
  (", except ..." on an analogy; "like the solver never ran"),
  negative parallelism split across a sentence boundary ("isn't just X.
  It's Y" — the one-sentence regex missed it), fabricated vignette
  ("you usually notice a mistake three operations later"), fake-humble
  closer ("I'm sure there are corners I haven't hit"), colon-elaboration
  ("Here's ENTER ...:"), precision theater ("exactly one step"),
  "drops you straight into".
- **Manufactured constructions are stuffing too:** a hedge with no
  judgment under it ("I think that's the fastest way to use the whole
  thing") and ornament enthusiasm ("surprisingly handy") both died. A
  hedge must sit beside a claim he could actually be wrong about
  (r3's: "I think it's quicker than digging U.HIST out of the STK
  menu" — a real comparison).
- **Calibration is the acceptance test for the framework itself.** After
  this iteration the tools reproduce all three of his verdicts: v0.1
  red (absence), r2 red (stuffing), the published v0.3 post green. A
  framework change that cannot re-derive his past verdicts is not an
  iteration, it is drift.
