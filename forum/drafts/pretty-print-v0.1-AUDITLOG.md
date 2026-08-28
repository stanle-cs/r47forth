# pretty-print v0.1 post — audit log

Register: **release** (new package announcement, not an update post).
Calibration artifact throughout: `undo-history-v0.1-final.txt`, which Stan
edited himself and posted. Every scanner number below is quoted against
that file, because "clean" only means anything relative to something he
actually published.

His post scores, for reference:
`aiaudit` 4 flags (instead-of-pivot 1, rule-of-three 3);
`framescan` 10 recurring frames, top 5x; mean 21.2w, CV 0.69,
longest 59w, shortest 4w.

## Rounds

**v0.1** — first draft. `aiaudit` 15. `voicematch2` **BLOCKING (2)**:
`hedge_adv` and `paren` MISSING. Cause: the draft's only parentheticals
were inside `[list]` bullets, so the prose carried none.

**r2** — added a hedging adverb beside a real claim ("You'll probably
never see it refuse on ordinary arithmetic") and two prose parentheticals
carrying reasons. Killed one `X-rather-than-Y` and cut `instead-of-pivot`
3 → 1. `aiaudit` 11. `voicematch2` clean.

**r3** — cut stylistic triples toward his rate, split long compounds.
`aiaudit` 8. Stylometry landed on his: mean 16.8, CV 0.67, longest 58.

**Cross-model pass (Gemini, corpus + his published post inlined).** Six
findings. Dispositions:

| finding | disposition |
|---|---|
| Fragments "They nest." / "The browser pans." read as metric-chasing | **FIXED.** Both folded into neighbours. His shortest is 4w but he never writes a 2w fragment; this was the overcorrection tell the skill names |
| Lifted his sentence frames — "This is a package that", "This is the ordinary stack screen", "This is the browser after" | **FIXED, and it was the serious one.** DESIGN.md forbids reusing a sentence across documents. All three reframed |
| Sign-off paragraph near-verbatim from his post | **FIXED.** Rewritten keeping every Tier 1-3 fact |
| `[b]When it refuses[/b]` is a signposting clause; his headings are nouns | **FIXED** → `[b]Refusals[/b]` |
| "the test suite keeps it as the capacity case" is testing trivia | **FIXED.** Cut. §2 bans sentences about how a thing was found |
| Impersonal passive in Limits bullets | **REJECTED.** His 2026-08-10 ruling is explicit that bullets stay impersonal and that auditor pressure toward "I" in them is an anti-pattern. He reverted exactly this himself once |

**r4** — the six above, then `voicematch2` went **BLOCKING (1)** because
rewriting the install paragraph had removed the only fronted connector.
Restored with a fronted "So". Two boilerplate shingles still matched his
post; both reworded.

## Final state (r4)

- `aiaudit` **10** (his 4). Remaining: `instead-of-pivot` 1 — the
  mechanism question "Why refuse instead of drawing something close?",
  which is exactly the shape and count of his own "Why skip instead of
  dropping older levels to make room?", and is content-bearing rather
  than a segue, so it is inside the two-question ceiling. `rule-of-three`
  9 — judged artifacts and factual inventories (SUM/PROD/INTEG/DERIV;
  strings, matrices, dates and long integers; display mode, rounding and
  digit grouping). The flagged example is a pair, not a triple.
- `framescan` 12 recurring frames against his 10, in a slightly longer
  text. Proportionate.
- **Cross-document prose reuse: none.** Only the forum URL shingles
  remain, and a link is not prose.
- `voicematch2 --register release`: no MISSING, no STUFFED, no FORBIDDEN.
- Stylometry: mean 16.8w, CV 0.65, longest 59w, shortest 5w — his are
  21.2 / 0.69 / 59 / 4.

**r5 — Stan's decisions applied (2026-08-27).** All three open items
closed by him, plus the attachment restructure:

| his call | what changed |
|---|---|
| "i want to pr" | PR intent added after the opener, in his stake position. Worded fresh — his undo-history line ("built with the hope to be submitted as a PR") is NOT reused, and framescan confirms no shared shingle |
| "remove that all together" | The soft "roughly a hundred cases of mine" is gone. The testing sentence now claims only what the gate proves |
| "keep hardware claim" | Read literally: the existing statement stays, which says it has NOT been flashed. No hardware claim was added — DESIGN.md §1 allows one only if he has actually flashed and run the build, and he has not said so |
| "collate them into 2 attachments" | Three shots became two images. The stack screen and the browser stack into one with a rule between them; the capacity case stands alone. Neither frame is cropped or retouched, so both are still whole 400x240 screens |

r5 re-ran the full battery (every new sentence runs the loop): `aiaudit`
10, `voicematch2` clean, no cross-document prose reuse, mean 16.4w,
CV 0.65.

**r6 — Stan's rewrite adopted, tail redrafted (2026-08-27).** Stan rejected
r5's prose past the browser paragraph and rewrote the opening himself; his
text is adopted byte-for-byte with the two fixes he ruled ("both errors
should be fixed"): the PSHOW bullet now describes the value draw
(`fnPrettyShow` builds REGISTER_X's value, prettyValue.c:856, verified
against a live Σ capture), and 3 1/3 became 3 1/6 (the shot's formula is
exactly 19/6). Every sentence from `[b]Refusals[/b]` on is fresh — the r5
tail is a facts-only source per the handoff. New in this round: the
empty-history bullet (Tier 4 owed it; "no formulas" string verified at
prettyBrowser.c:44) and the pager gesture (PHIST inside the browser,
verified at prettyFormula.c:708).

Battery at r6: `aiaudit` 8 (his 4) — copula-avoidance 1 sits in HIS
authoritative sentence, instead-of-pivot 1 is the licensed mechanism
question, rule-of-three 6 are factual inventories. `voicematch2 --register
release` clean, no MISSING, no STUFFED. `framescan` cross-document: URL
shingles only after the licence-line reword (it shared "the licence is
GPL-3.0-only, inherited" with his published post). Stylometry mean 17.1w,
CV 0.85.

**Cross-model r6, lane 1 (GPT-5 via codex, identity line verified).**
Verdict "a careful reader would probably accept this as human-written."
Four findings:

| finding | disposition |
|---|---|
| d_d bullet reads as fix narrative | **REJECTED.** Auditor's own note says retain; it is a Tier 4 workaround instruction, not a discovery story |
| "Why draw nothing instead of coming close?" | **REJECTED, ruled keep.** Exact shape and count of his published "Why skip instead of dropping older levels to make room?" |
| "lowercase names are accepted" impersonal passive | **FIXED** → "lowercase names work fine" |
| The/A/It opener runs in Limits and Install (mild) | **PARTIAL.** Two Install openers varied ("Generated against r47forth commit..."); the rest is spec density inherent to impersonal bullets, which are ruled impersonal |

**Cross-model r6, lane 2 (Gemini 3.1 Pro high via agy, identity line
verified).** Six findings and a harsher verdict ("a careful reader would
spot this as AI-generated"). Dispositions:

| finding | disposition |
|---|---|
| "Math hallucination": the INTEG demo differentiates at a fixed point, so the outer integral integrates a constant, "proving the author didn't write the math" | **REJECTED as a tell, premise half-true.** The expression is EQ22/EQ33's pinned capacity case, typed into the sim, evaluating to exactly the value the post quotes, and it matches the attachment. The post frames it as renderer capacity, not mathematics. The kernel — a math-literate reader may notice the integrand is constant in x — goes to Stan as an optics call, not a text change |
| Staccato run of clipped sentences in Install | **FIXED.** Target, flash cost and memory merged into one flowing sentence in the shape of his published install paragraph |
| Comma-splice skeleton four times in quick succession | **FIXED by redistribution.** Install keeps two; two moved into Limits bullets, where his published bullets splice the same way |
| Parenthetical skeleton six times in the drafted half | **PARTIAL.** The two weakest became plain clauses (30-char bullet, d_d softkey); five remain across ~550 words, matching his published post's density. Gemini itself marked them all leave-alone for accuracy |
| "the risk is yours" slogan fragment | **FIXED.** The disclaimer folded into the simulator sentence; "So back the calculator up first." keeps the fronted connector the release floor requires |
| The mechanism question, again | **REJECTED, ruled keep.** Same disposition as lane 1, and Gemini also marked it leave-alone for accuracy |

Post-fix battery: `aiaudit` 8 (unchanged, all judged), `voicematch2` clean,
cross-document reuse URL-only, mean 18.0w, CV 0.84.

**Exit-criterion state, honestly:** the ruled bar (two consecutive clean
audits by two non-drafting models) is NOT met — both lanes produced real
findings this round and the fixes have not been re-audited out-of-family.
Every prior round went to Stan's read in exactly this state, and his read
is the only gate that can say yes.

## Not done, and it matters

The machines endorse this draft. That is not the same as done, and the
skill says so in as many words. Two of the last three drafts to reach
this state were rejected on Stan's read — v0.1 for having none of his
constructions, r2 for wearing them as ornaments. **Only his read gates.**

What is left is his read, and one thing the record cannot settle for
him: whether the two collated images read better than three separate
ones on the actual forum. The originals are all in
`forum/screenshots/pretty-print-archive/` if he wants them re-cut.
