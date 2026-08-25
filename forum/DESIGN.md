# Forum post design spec

Standing specification for every forum post published to
`forum.swissmicros.com` under Stan's name. Read this before drafting or editing
any post, README or INSTALL text. It exists because five separate drafts were
rejected for reading like a machine wrote them.

Companion tooling lives beside this file: `aiaudit.py`, `framescan.py` (same folder).

---

## 1. Locked decisions

| Decision | Value |
|---|---|
| Threading | Replies in the existing threads, not new topics. No back-links needed |
| Order | Package-manager post first, Forth post second (Forth depends on it) |
| Distribution | Public repo, release branch linked in the post |
| Format | `[list]` bullets plus short declaratives. No paragraph over 3 sentences |
| Reference tables | Full, and re-verified against source every time |
| Hardware claim | Only if Stan has actually flashed and run the build |

`t=4876` is the **custom package** thread. An older AI draft
(`packages/forth-core/FORUM_POST_STAGE2.txt`) mislabels it as the Forth thread.
Do not trust that draft for anything.

---

## 2. Voice and tone (governs everything else)

This is a constraint on the writing, not a polish pass at the end.

**Hard rules**

- Write as Stan. First person where he has a stake. Contractions throughout.
- Register ruling (2026-08-03, after the first pipeline draft failed his
  read): technical writing is either cut and dry, or explained in his
  style. The polished-technical-writer middle register is the failure
  mode — crafted contrasts, tidy mid-length sentences, no person.
  The ONLY authentic voice anchor is the Reddit corpus (first person
  3.8/100w, contractions 3.5, questions 11%). "Cut and dry" means plain
  and terse — short factual sentences, ordinary verbs, no flourish — it
  is a mode, not a document to imitate. Stan's correction of the same
  day: INSTALL.txt, RELEASE_README.md and every other project-authored
  doc are ALSO AI-generated and carry no voice authority; do not use
  them as register anchors, style donors, or reuse-comparison baselines.
  They are facts-only references until rewritten through this pipeline.
  `voicematch.py` measures a draft against the corpus; `--attest` checks
  phrase-level support.
- No em dashes. Use a comma or a full stop.
- No rhetorical antithesis: `X rather than Y`, `not X but Y`, `no X, no Y`,
  trailing `, not Z`. Keep only where the contrast **is** the fact
  (`4320 instead of 5040`).
- No counting or signposting headings (`Two things to know`, `The one trap`).
  Name the subject.
- No trailing `, which is why ...` clauses bolted onto a finished sentence.
- Never reuse a sentence across documents. Say it differently in each.
- **No anecdotes.** Anything that reads as an incident must trace to a logged
  event, and appears only if the reader needs it. Bugs go in as
  behaviour-change notes, never as discovery stories. If a sentence explains
  how something was found rather than what it does, cut it.
- Never invent an event. A fabricated "after getting burned by exactly this"
  reached a near-final draft; the record showed the trap had been verified
  deliberately with a marker injection, and nobody had ever been bitten.

**Measured targets**

- Sentence-length CV >= 0.5, with several sentences under 8 words.
- `, so` at most about 1 sentence in 8. It is the house crutch: an early draft
  ran 15 uses in 120 sentences.

**BBCode conventions**

- Reference tables are the highest-risk section. `[b]Term[/b] — description`
  inline-header lists are on Wikipedia's AI-formatting list. Use `[b]WORD[/b]`
  followed by a plain stack comment, no dash pivots.
- Keep bold for scanning only. Heavy bold reads as machine formatting.

**Screenshots (ruled 2026-08-03)**

- Every shot in a post comes from `forum/screenshots/`; its README carries
  the provenance (which test pins the behaviour shown, how it was captured).
- Each shot gets a one-line factual caption. Caption prose goes through the
  same gate as body text: the §2 rules, the scanners, the rotation.
- A shot may only illustrate a claim the simulator verifies. Hardware-only
  claims (R/S interrupting `SPIN`) stay text-only; a sim capture must never
  imply a hardware observation.

**Update-post structure (Stan's edit, 2026-08-10, forth v0.3).** He
restructured the pipeline draft before posting; his shape governs
update posts from now on:

- The changelog LEADS, under a versioned heading (`[b]Changelog
  v0.3[/b]`). No intro paragraph.
- Interleave: `[list]` changelog block, then its attachment, then one
  or two short prose paragraphs about the picture. Not prose-first
  with the changelog at the bottom.
- No rhetorical questions, none. He deleted two corpus-attested
  hypophora that had survived two clean-ish audit rounds. Plain
  declaratives.
- Bullets stay impersonal. He removed the first-person ownership
  framing the auditors had pushed for ("I made the interpreter
  refuse" became "The interpreter refuse the compare tests").
  Auditor pressure toward "I" in changelog bullets is an anti-pattern
  for his posts.
- His grammar is his. Do not polish "The interpreter refuse" to
  "refuses" and do not imitate the pattern either; drafts are written
  clean and he roughens where he pleases.
- Behaviour changes and scope qualifiers go in parentheses with the
  reason inline: "( ENTER ran the line before the console landed,
  doesn't work well in new design)".
- Changelog verbs are plain: "added", "support:".
- The closing is repo link, build command, flash disclaimer. He cut
  the target/base/licence recap and the sim-vs-hardware disclosure
  from the update post (the stage-2 post already carries them).

---

## 3. Mechanical gate

Run both over every document before publishing:

```
python3 forum/aiaudit.py    <files...>
python3 forum/framescan.py  <files...>
```

- `aiaudit.py` — phrase patterns from Wikipedia:Signs_of_AI_writing, formatting
  tells, and sentence-length stylometry. Synced against the live catalogue
  2026-08-03: weasel attribution, the challenges/future-outlook formula,
  notability-canned phrases, newer vocabulary, title-case headings, emoji,
  markdown leaking into BBCode, and the inline-header list in its BBCode
  form. Its `[HARD]` class (chatbot artifacts, unfilled placeholders,
  knowledge-cutoff disclaimers, utm_source tracking) is disqualifying, not
  a judgment call — one hit means nobody read the text after generation.
- `framescan.py` — pattern-agnostic. Blanks content words and reports recurring
  function-word skeletons, contrast tails, cross-document shingles, repeated
  openers and closers. This is what catches tells nobody has named yet.

**Judge the flags, do not drive them to zero.** Roughly a third are factual
enumerations (three real parameter forms, three real words) that must not be
rewritten. Optimising against the detector produces worse text with better
scores. Fixing one frame also inflates another: replacing `so` with `and` moved
the count from one skeleton to the next.

**Rotate the reader.** A model auditing its own output shares its blind spots.
A fresh pass as a different model found five classes the self-audits never saw,
including overcorrection scars left by the previous fix pass. Stan's read is
the final gate; no scanner can tell whether it sounds like him.

**The loop (ruled 2026-08-03).** A draft reaches Stan's read only through
these steps, in order:

1. Draft against the content inventory (§6/§7), by hand or via
   `PROMPT_WRITE.md`. Record which model drafted.
2. Self-check against §2; fix what you can see.
3. Run both scanners; judge the flags.
3b'. Run `voicematch2.py --register release|update|chat` (added
   2026-08-25 after the undo-history v0.1 rejection, register-
   conditioned the same day after the r2 rejection). It measures his
   corpus constructions BOTH directions: floors (a construction his
   register owes that the draft lacks — MISSING) and caps (a
   construction stuffed past his release density — STUFFED/FORBIDDEN).
   Announcement posts use `release` (floors and caps), changelog-led
   update posts use `update` (his published v0.3 artifact owes no
   floors; caps and the segue-question rule still bind), replies use
   `chat`. Any blocking row: back to step 2. Constructions are
   licenses, not quotas — r2 satisfied every floor by stuffing
   (two segue hypophora, a twist-tailed analogy, a no-judgment hedge,
   a fake-humble closer) and read as AI to him anyway. Questions in
   posts must be the reader's actual mechanism-question (the published
   v0.3 post keeps two, one opening a paragraph); segue formulas
   ("So what are you looking at?", "What about X?") are forbidden in
   any position. The companion references are
   forum/reference/stan-voice-profile.md (register mapping + deployment
   rule) and stan-exemplars.md (local-only); the drafter writes with
   the exemplars open, then STRIPS every construction that is not doing
   real work before the scanners run.
3b. Run `voicematch.py` against the reference corpus (added 2026-08-03
   after the first pipeline draft passed both model audits and still
   failed Stan's read — the audits measure "human", not "Stan"). Compare
   the draft's numbers against the two register anchors in §2; a draft
   on the polished side of both goes back to step 2, whatever the
   auditors said.
4. Cross-model audit via `PROMPT_AUDIT.md`, by a model that did not write
   the draft. The auditor reports; the fixing is done on our side.
5. Exit criterion: scanners clean as judged, AND two consecutive audits by
   two different non-drafting models with no real findings. Any real
   finding resets the count to zero.
6. Stan reads. Nothing else substitutes for this step.

**After approval.** A sentence-level rewrite re-runs the scanners on the
changed section plus Stan's re-read of that section. Pure figure, typo and
BBCode fixes are exempt. A new paragraph re-enters the loop at step 2.

**Replies.** Every thread reply runs the full loop, however short (ruled
2026-08-03; a lighter reply gate was offered and rejected).

**Scanner growth.** A tell class confirmed by an audit or a rejection gets
encoded before the next draft cycle: a pattern in `aiaudit.py` or
`framescan.py` where it is mechanical, a §2 rule where it is not.

**Model assignment (ruled 2026-08-03).** Fable (Claude) drafts, in one
deep-effort session with repository access — drafting is one writer's job,
and a multi-agent fan-out produces as many voices as agents. The audit
pool is ChatGPT and Gemini: one pass each, either order, both through
`PROMPT_AUDIT.md` in fresh sessions. The drafter exclusion applies at the
model-family level, so Claude never audits a Claude draft; if the drafter
ever changes, the exclusion travels with it.

**Audit drivers (2026-08-03; invocation CORRECTED 2026-08-06).** The
Gemini pass runs locally through the Antigravity CLI. The model flag must
come **BEFORE** `-p`, and the prompt must be an argument rather than
stdin:

```
agy --model gemini-3.1-pro-high --print-timeout 25m -p "$(cat prompt.txt)"
```

**`agy -p --model gemini-3.1-pro-high` — the order this file carried until
now — silently serves Claude.** Verified 2026-08-06 by asking each form
which model it was: the `-p`-first order answered "Claude Opus 4.6
(Thinking)", the `--model`-first order answered Gemini. That is not a
cosmetic difference, it is the drafter exclusion failing open: any audit
run with the old order was a Claude model auditing a Claude draft, which
is the one thing the two-family pool exists to prevent, and it would have
reported clean for exactly the tells a same-family reader cannot see.

**So verify the reader, every pass.** Put "state your model name" at the
top of the audit prompt and check the answer before believing the
findings. A silent fallback is indistinguishable from a good audit.

Two further traps, both hit the same day: print mode has a **5-minute
default timeout** and returns EMPTY output when a long prompt exceeds it
(hence `--print-timeout`), and `agy models` lists what is actually
available — the name in the flag has to be on that list. The ChatGPT pass
runs through the Codex CLI (`codex exec`) once it is installed and logged
in with the ChatGPT account (`npm install -g @openai/codex`, then
`codex login`); until then that pass is pasted into ChatGPT by hand.
Piping the prompt does not change who the auditor is: the model named in
the invocation is the reader of record.

**Voice reference (added 2026-08-03).** `forum/reference/reddit-trungdle.md`
holds eleven years of Stan's public Reddit writing (1696 comments, 80
submissions, collected from the RSS listings, deduplicated). It is
LOCAL-ONLY and gitignored: the repo is public and the corpus would
permanently link his Reddit history to this identity. Uses:

- The drafter reads a sample before drafting; the auditors may be given
  excerpts as "this is how the author actually writes".
- Measured baseline over 2488 sentences: mean 22.5 words, CV 0.83, 352
  sentences under 8 words. The §2 floor (CV >= 0.5) is conservative;
  aim near his real variance, not at the floor.
- His own prose trips 596 scanner flags. That is the proof of "judge the
  flags": a human writer uses rule-of-three and dashes freely; the
  scanners locate candidates, they do not define his voice.
- Never lift a sentence from the corpus into a post. It is public text
  under his name, and reuse is detectable. `framescan.py` run across a
  draft PLUS the corpus will surface accidental shingle reuse.
- After a fresh clone, re-collect via the RSS listings or restore from a
  local copy; the file does not travel with the repo.

---

## 4. Required in every post

**Tier 1 — blocking**

1. A download link that resolves.
2. No claim that has not been verified. Hardware claims need real hardware.
3. Correct thread attribution.
4. Every distributed artifact carries `COPYING`. GPL-3 sections 4 and 5 require
   a conveyed copy to ship the licence, and a download leaves the repo behind.
   The release branch and both zips each lacked it until 2026-07-25; the repo
   having `COPYING` at its root is not sufficient.

**Tier 2 — the reader cannot act without**

4. Upstream base commit the package was generated against.
5. Build command.
6. Target hardware, stated explicitly (R47/C47 on DM42n, DMCP5).
7. Any dependency on another package.

**Tier 3 — obligation**

8. Backup and flash-at-your-own-risk disclaimer.
9. GPL-3.0-only stated in the post and in the shipped README or INSTALL. The
   licence is inherited from c43, not chosen: forth-core patches c43 sources and
   links into the same binary.

**Tier 4 — honest limits.** Every known behaviour that will surprise someone.

---

## 5. Pre-publication gates

1. Repo public. Verify anonymously:
   `curl -o /dev/null -w "%{http_code}" https://api.github.com/repos/stanle-cs/r47forth`
   must return 200. A private repo returns 404 and every release link is dead.
2. Stan has flashed and run the build if the post says so. For the Forth post
   that means: FDEMO's documented registers, SAVE's schedule, R/S interrupting
   `SPIN` (hardware-only, unverifiable in the simulator), the PEM round-trip,
   FWRD insert, and `THEN` removal being rejected.
3. Every reference figure re-verified against source. **Stage 1's published
   figures are not trustworthy**: it printed "Name length: 63 chars max" when
   `FORTH_NAME_MAX` is 31 (63 is `FORTH_TOKEN_MAX`, a different limit).
4. Program listings byte-identical to the passing test fixtures.
5. BBCode tags balanced; one line per paragraph, never hard-wrapped
   (phpBB turns newlines into line breaks — ruled 2026-08-03 after the
   79-column habit reached a draft).
6. `COPYING` present in the release branch and inside every zip. `pkg_build`
   appends it automatically; hand-assembled artifacts need it added.

---

## 6. Post A content — custom package (v0.4; Stan re-versioned
2026-08-03 — v0.3 was the July snapshot already shipped under that
name, and this rewrite is a new version)

**Structure ruling (2026-08-03, Stan's content review of the first
pipeline draft).** The numbered inventory below still binds as facts,
but the post is BUILT differently: `./package` is the interface and is
introduced first, with every instruction going through it (the raw
tools get one mention as what's underneath). The materialize command is
taught with its reason — copying a file out of your checked-out tree
drags in upstream drift because the tree has moved past the package's
base; materialize pulls the file at the recorded base so the patch is
exactly your edits. Package testing is explicit (`make test
CUSTOM_PKG=...`, and pkg_build as the release lap). Composition gets a
concrete loading-order example (two packages, NNN prefixes, tie by
CUSTOM_PKG position). Definitions are positive — what a package is and
what you do — never by absence; his corpus runs 1.1% double-negation
and none of it definitional.

**What changed from v0.2**

1. Overrides are git diffs against upstream, not whole-file copies
2. A package's delta is readable without diffing two files by hand
3. Two packages can patch the same file; patches stack in `NNN` order
4. Different parts of one function compose; same lines is a fatal configure
   error naming the patch
5. A package declares nothing: no `meson.build`, no file list
6. Working area is flat and mirrors upstream paths
7. `refresh` classifies: path exists under `src/c47/` becomes a patch, no
   counterpart is copied whole

**Tooling reference**

8. `--materialize <file>` pulls an upstream file into the working area
9. Base-commit pinning: the package records the commit it was authored against
10. `--rebase-base` moves the package forward; conflicts land as markers
11. Drift detection: manifest hashes, hand-edited entries warn then overwrite
12. `.pkgignore` keeps docs and dev scripts out of `patches/`+`files/`

**Composition**

13. Ordered by numeric filename prefix, ties by `CUSTOM_PKG` list position
14. Upstream materialized once, each patch applies `git apply -3` against the
    previous output
15. Conflict-marker scan after every application, regardless of exit status
16. Patch filename and `+++ b/` header must agree, and the target must exist
17. Two packages providing the same new file is caught before the shadow tree

**pkg_build**

18. Cleans, runs the suite with the package active, refreshes, zips
19. Failing suite produces no artifact
20. Zip size-checked against `PKG_MAX_SIZE`, 1MB default

**Constraints**

21. Build reads generated `patches/`+`files/` only, never the working area
22. Everything in a package directory is tracked in git, working area included

**Closing**

23. Target, licence, download link, base commit
24. forth-core as the worked example: 14 patches, 13 sources

---

## 7. Post B content — Forth Stage 2

**What changed since Stage 1**

1. Forth source lives inside normal R47 programs as program steps
2. String-in-X entry still works
3. PEM: insert FORTH, alpha editor opens, closing marker inserted with it
4. ENTER stores and opens the next line inside the pair; EXIT closes; EDIT
   reopens

**Dictionary scope**

5. Definitions are program-local by default
6. Rationale: two programs defining the same name must not affect each other
7. `GLOBAL` promotes: survives power-off and backup/restore, keyboard-callable
8. Definitions pre-scanned, so a word works above the line defining it

**Language**

9. `IF/ELSE/THEN`, `BEGIN/UNTIL`, `BEGIN/AGAIN`, `BEGIN/WHILE/REPEAT`
10. `RECURSE`, `IMMEDIATE`, `FORGET`
11. C47 functions with the machine's own parameter grammar and dispatch code
12. `XEQ` resolves C47 labels, C47 functions and Forth words; R47 programs XEQ
    Forth words
13. `FWRD` lists words visible to the current program, inserts at the cursor
14. Source line checked before storing; an invalid line leaves the step alone
15. Design rule: where Forth and R47 disagree, R47 wins

**Samples**

16. FDEMO listing and its 16 documented registers
17. [SUPERSEDED by D3, corrected 2026-08-03 — was "FDEMO needs the
    8-level stack"; the spill makes depth work at either window, and no
    pin covers FDEMO-at-4 specifically, so the post makes no such claim]
18. SAVE listing and its 7 documented registers
19. `GROW` remains keyboard-callable after the run

**Behaviour changes since Stage 1**

20. `RCL` lifts onto a Forth value instead of overwriting it
21. [CORRECTED 2026-08-03, D3 landed after this inventory was authored]
    Values past the visible stack spill to memory and drain back as the
    word unwinds; `7 FACT` = 5040 is the pinned acceptance. A native
    C47 function will not run while values are spilled (loud stop with
    a named message), and a finished line may not leave spilled values.

**Limitations**

22. A native C47 item's result does not return to the Forth stack
23. `SPIN` reaches the runaway guard; R/S interrupts on hardware
24. `IF` tests X for zero. Any non-numeric type counts as true, so a string or
    a matrix takes the true branch
25. Not implemented: `CREATE/DOES>`, `VARIABLE`, `CONSTANT`, vocabularies, a
    Forth terminal

**Reference tables**

26. Core dictionary: stack ops, arithmetic, glyph aliases, compiler words,
    system words, control flow, `RECURSE`/`GLOBAL`/`IMMEDIATE`/`FORGET`
27. XEQ forms: `XEQ 'NAME'` against `XEQ :NAME:`
28. Parameter grammar: direct numeric, direct register, named register, system
    flag, indirect `→nn` and `→'NAME'`
29. Tokens: Stage 1 set plus `FTOK_XEQN`, `FTOK_C47`
30. Execution precedence: the five-step chain, updated for scope filtering
31. Limits: return stack 64, runaway 4096, source buffer 256, name length 31
    (Stage 1 printed 63, correct it), nesting 4, data stack 4 or 8 levels

**Closing**

32. Target, licence, download, build command, disclaimer

---

## 8. Standards background

Useful when a reviewer asks whether a Forth behaviour is standard-conforming.

`IF` is specified as `( x -- )`, deliberately `x` and not `flag`. Run-time
semantics: "If all bits of x are zero, continue execution at the location
specified by the resolution of orig." So **non-zero is true is exactly
standard**.

The non-numeric case is **outside** the standard, not contrary to it. Standard
Forth's data stack is untyped cells and does no type checking; strings live
there as addr+count (two ordinary cells) and floats go on a separate FP stack,
which the standard requires to be separate from the data and return stacks.
A non-numeric value cannot be on the data stack at all, so the question never
arises.

forth-core inherited a *typed* stack by making C47's RPN stack the data stack.
Where the standard is silent, "R47 wins" governs unopposed, and R47's own
comparison words raise `ERROR_INVALID_DATA_TYPE_FOR_OP` on non-numeric operands.

The standard's own answer to values that do not fit the data stack is
segregation onto a separate stack. That is a precedent for the parked hybrid
spill design in `design-docs/forth-core/DEFECTS_stack_semantics.md` D3.
