# forth-post-2 audit log

Draft: `forth-post-2.txt` — stage K-N update post, picture-led, changelog
since the stage 2 post. Drafted 2026-08-10 by Fable (Claude), one session.

## Round 0 — mechanical battery (2026-08-10)

- `aiaudit.py`: 1 flag. rule-of-three on "echoes the line, prints its
  output, and prints an error line". KEPT: the dialogue does exactly
  three things, the enumeration is the fact.
- `framescan.py`: no coordinate negation, no contrast tails, no
  cross-document shingles. Repeated "The" openers reduced in rev 2.
- `voicematch.py`: contractions 0.71 vs corpus 3.45, first person 0.57
  vs 3.84. Judged acceptable for register: the post is deliberately
  list-heavy (changelog), the cut-and-dry mode. Prose paragraphs carry
  the voice markers; bullets don't, and shouldn't.
- `voicematch.py --attest`: low-support clusters are domain terms
  (FWRD, home row, stage post). No real fixes indicated.

## Facts verified before drafting

- ENTER = space, R/S = run: keyboard.c CM_AIM branch (this session).
- FHIST cap 1024 bytes, oldest-first eviction: forth_dict.h,
  forth_fold.c.
- f-shifted up/down recall: forthHistoryRecall, forth_fold.c:807.
- 29 primitives: forth_prims.c table count.
- |x| vs ABS, compare-family rejection: forth_dict.c resolver +
  forthItemIsFlowReject (README correction session, same day).
- Base still 00.109.04.00b0: .refresh-manifest.json base_commit is the
  00.109.04.00b0 merge.
- COUNT-DOWN example: sim-verified with stack canary (temp driver,
  removed).
- No new release branch exists; the post points at main + build
  command. Stage 2's zip claim not repeated.

## Screenshots

Three composites (3-attachment limit), provenance in
`forum/screenshots/README.md`:

1. `forth2-attach-1-console.png` — n-1 dialogue, n-2 rolled+GLOBAL,
   n-3 spill separator.
2. `forth2-attach-2-repl.png` — n-4 COUNT-DOWN, n-5 error line,
   n-6 history recall. All three captured fresh 2026-08-10 (the stage-L
   composing shots show the pre-console UI and were NOT used).
3. `forth2-attach-3-catalog.png` — m-1 catalog row, m-2 normal-mode
   softkeys, m-4 ASSIGN pending.

stage-l-4-recall.png found to be a byte-identical copy of
stage-l-3-keysmode-fold.png; no genuine stage-L recall shot exists.

## Round 1 — cross-model audits (2026-08-10)

Readers verified: "GPT-5 (Codex)" (model gpt-5.6-sol) and "Gemini 3.1
Pro". Both non-Claude; both valid passes.

Converged findings and dispositions:

- "Top to bottom:" caption opener x3 (both auditors) — FIXED: openers
  varied, one instance kept.
- "no program needed" trailing-negative slogan (both) — FIXED: bullet
  rewritten to name the screen.
- Impersonal "are rejected" where Stan owns the decision (both) —
  FIXED: first person, "I made the interpreter refuse".
- Staccato bullet run ".. reopens the editor empty. EXIT closes
  everything." (Gemini overcorrection) — FIXED: merged to one sentence.
- Participial tail "rolling upward like a terminal" (Gemini) — FIXED:
  finite verb.
- Comma splices 5x (Gemini frame finding) — THINNED by two (keys-mode
  para split, fold bullet takes a colon). The shape itself is
  corpus-attested and stays.
- Changelog restates the body (Codex #1, structural) — PARTIAL: the
  keys-mode prose section collapsed to two sentences pointing at the
  changelog. The remaining overlap is the changelog format itself;
  ruled format is [list] bullets, and a complete changelog repeats the
  body's highlights by nature. KEPT.
- Q&A "Why keep a failed line in history?" (Codex, mild) — KEPT:
  question-then-answer is Stan's corpus signature.
- "That changed, ENTER ran the line before the console landed" (Codex,
  mild anecdote) — KEPT: behaviour-change note, explicitly allowed by
  DESIGN §2; existing users need it.
- 24x declarative changelog frames (Codex) — KEPT: release-note
  format; both auditors said rewriting to dodge would hurt.

Real findings occurred, so the clean-pass count is 0. Round 2
launched on the revised draft.

## Round 2 — cross-model audits (2026-08-10)

Readers verified: "GPT-5 Codex" and "Gemini 3.1 Pro (High)". Both
verdicts accept the draft as plausibly human; findings shifted to
judgment calls, the expected seesaw.

Fixed from round 2:

- Near-duplicate fact pair, body vs changelog: "FORTH outside the
  program editor ... opens" twice (both auditors) — changelog bullet
  rewritten ("Typing FORTH with no program editor open starts a live
  session right there").
- "The name is reserved." passive on an author-owned decision (both) —
  now "I reserved that name for the package."
- Parallel frame "Calculator keys resolve / Calculator functions
  resolve" (Gemini) — keys bullet now "Physical keys land in their
  normal columns while you're on keys."
- Licence line shared "full text in COPYING" with the stage-2 post
  (sentence-reuse rule) — reworded. Target-hardware phrase varied for
  the same reason; the build command stays byte-identical on purpose.

Kept, with reasons:

- Comma splices (Gemini r2 calls them errors) — the shape appears in
  the corpus and in the approved stage-2 post; ruled corpus-attested
  in round 1. Not re-litigated.
- Two consecutive Q&A transitions (Codex r2, mild) — hypophora is
  Stan's corpus signature; both auditors declined to ban it.
- Changelog's declarative-run format — release notes; both auditors
  said leaving it is correct.
- Gemini r2's top finding claims the whole register is overcorrected
  and Stan's true voice is ESL with dropped articles. NOT ACTED ON:
  the register matches the stage-2 post that passed his read, and
  imitating ESL mistakes would be a fabricated voice. Surfaced to
  Stan as an open question rather than fixed.

## Stan's read — final (2026-08-10)

Stan rewrote the structure and posted his own version; it is recorded
verbatim in `forum/output/forth-v0.3.txt`. His edits, as rulings:

- Changelog LEADS the post, under a versioned heading ("Changelog
  v0.3"). No intro paragraph at all. Sections interleave list →
  attachment → short prose, the prose after its picture.
- BOTH hypophora Q&As deleted ("Why keep a failed line...", "Where do
  the lines go?") even though they were corpus-attested and survived
  two audit rounds. Plain declaratives replace them.
- First-person ownership claims REMOVED: "I made the interpreter
  refuse" became "The interpreter refuse the compare tests"; "I
  reserved that name" became "FHIST is also a reserved label/name".
  The round-1/round-2 auditor pushes toward first-person ownership
  were wrong for his bullets.
- His grammar stays his ("The interpreter refuse..."). Gemini r2's
  voice observation was partially right; the answer is his own edit,
  never imitation.
- Behaviour-change notes become parentheticals with the reason:
  "( ENTER ran the line before the console landed, doesn't work well
  in new design)". Technical scope also parenthetical: "(if forth
  exceeds max R47 stack size)".
- Changelog verbs: "added", "support:". Simpler than my fold/ride
  phrasings.
- Closing stripped to repo link + build command + flash disclaimer.
  Target/base/licence lines and the sim-vs-hardware disclosure all
  cut. The keys-mode prose section and the catalog caption also cut.
- The 29-primitives count bullet cut.

Encoded in write-as-stan (2026-08-10 addendum) and forum/DESIGN.md.
Cycle closed.
