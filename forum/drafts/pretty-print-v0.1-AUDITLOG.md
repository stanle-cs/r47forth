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

## Not done, and it matters

The machines endorse this draft. That is not the same as done, and the
skill says so in as many words. Two of the last three drafts to reach
this state were rejected on Stan's read — v0.1 for having none of his
constructions, r2 for wearing them as ornaments. **Only his read gates.**

Open items he should decide:

1. **The PR sentence.** His undo-history post says the package is built
   hoping to go upstream as a PR. Nothing in the record establishes that
   intent for this package, so it was not invented. If he wants it, it is
   his sentence to write.
2. **"roughly a hundred cases of mine"** is the one soft number in the
   post. The suite's pretty-print drivers are countable exactly if he
   wants a precise figure instead.
3. **Hardware.** The post says it has not been flashed. That stays true
   unless he flashes it, and then the sentence changes rather than
   disappears.
