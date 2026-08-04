# Prompt: audit a post for AI writing

Paste everything between the rules into ChatGPT or Gemini (the ruled audit
pool, one pass each), then paste the post underneath.

Use this on a post written by a *different* model than the one auditing. A model
reviewing its own output shares its blind spots, and that is the whole reason
this prompt exists. Running it in a fresh ChatGPT session on a draft written
elsewhere is the point.

This asks for findings, not a rewrite. Take the findings, decide which ones are
real, and fix those yourself.

---

You are auditing a technical forum post for signs it was written by a language
model. The author is a hobbyist who writes firmware extensions for RPN
calculators and posts to an enthusiast forum. The post must read as though he
wrote it himself.

Report what you find. Do not rewrite the post. Do not produce a corrected
version unless I ask for one.

## What to look for

**1. Banned constructions.** Quote every instance with its line.

- Em dashes used as a pivot.
- Rhetorical antithesis: "X rather than Y", "not X but Y", "no X, no Y",
  trailing ", not Z" appositives.
- Counting or signposting headings: "Two things to know", "The one trap".
- Trailing clauses bolted onto a finished sentence: ", which is why...".
- Anecdotes: any sentence describing how something was discovered, debugged or
  fixed, rather than what it does.
- Closing questions that fish for replies; summary paragraphs that restate.
- `[b]Term[/b] — description` inline-header list items.

**2. Repeated frames.** This matters more than the vocabulary. Ignore what the
sentences are about and look at their shape: strip the content words and see
which skeletons of function words and punctuation recur. Report any frame used
three or more times. In one earlier draft the frame `⟨clause⟩, so ⟨consequence⟩`
appeared fifteen times in a hundred and twenty sentences, and no vocabulary
check would ever have caught it, because "so" is not an AI word. Leaning on one
connective is the tell.

Also report repeated sentence openers and repeated two-word endings.

**3. Rhythm.** Count words per sentence across the whole post. Report the mean,
the shortest, the longest, and how many sentences fall under eight words. Flag
the post if most sentences land between thirteen and twenty-five words, or if
fewer than one sentence in eight is short. Uniform sentence length is the
strongest statistical marker of machine text and it is invisible when reading
one sentence at a time.

**4. Register slips.** Impersonal passive where the author has a stake in the
decision. Missing contractions: "cannot", "does not", "it is" sitting beside
"doesn't" elsewhere in the same post. Vocabulary no hobbyist uses in a forum
post: "leverage", "robust", "comprehensive", "seamless", "utilise", "delve",
"crucial", "underscore", "showcase", "furthermore", "moreover".

**5. Overcorrection.** If the post looks like it has already been edited to
remove AI patterns, say so, and look for what the editing introduced: runs of
three short sentences with identical shape, a connective swapped for a stiffer
one such as "therefore", or slogan-like fragments standing in for sentences.

## What not to flag

Do not flag a list of three real things as a rhetorical triple. "RECURSE,
IMMEDIATE and FORGET" is three actual words that exist. "direct numbers and
registers, named registers, flags, and the indirect forms" is four real
parameter forms. A technical document listing several real items is being
accurate, and rewriting it to dodge a pattern check would make it worse.

Likewise, keep a contrast where the contrast is the fact: "returned 4320 instead
of 5040" states two real values.

If you are unsure whether something is a genuine tell or an accurate technical
statement, say so explicitly rather than guessing.

## Output

1. **Findings**, worst first. For each: the quote, the line, which category, and
   one sentence on why it reads as machine-written.
2. **Rhythm numbers**: mean, shortest, longest, count under eight words, and the
   three most frequent sentence frames with their counts.
3. **Verdict**: would a careful reader take this as human-written? Where would
   they first doubt it?
4. **Deliberately not flagged**: anything you considered and judged to be
   accurate technical writing rather than a tell.

Do not try to reduce the finding count to zero. Tell me which findings you would
leave alone if the goal were a post that is accurate rather than one that passes
a pattern check.
