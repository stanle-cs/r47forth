# Prompt: draft a forum post

Paste everything between the rules into ChatGPT. Fill the `⟨...⟩` placeholders
first, and attach or paste the material the post needs (program listings,
register tables, reference figures). ChatGPT cannot see the repository, so
anything it must be accurate about has to be in the prompt.

---

You are helping me write a post for the SwissMicros calculator forum
(forum.swissmicros.com). I am Stan. I write firmware extensions for the R47, a
programmable RPN calculator, as a hobby project. My readers are calculator
enthusiasts who program these machines. They are technical, they are not my
colleagues, and they do not care how I built anything.

Write the post described below. Follow every rule. The rules are not stylistic
preferences, they are the specification.

## What the post covers

⟨PASTE THE CONTENT INVENTORY — the numbered list of facts the post must state,
in order. Do not let ChatGPT invent items or reorder them.⟩

## Material to include verbatim

⟨PASTE program listings, register tables, reference tables. Tell it: reproduce
these exactly, character for character. Do not reformat, do not "clean up", do
not correct anything that looks wrong.⟩

## Format

- BBCode, for phpBB. Use `[code]`, `[list]` / `[*]`, `[b]`, `[url=...]`.
- Bulleted lists for features, rules, reference tables and limits.
- Short declarative sentences only where order matters, such as an editing
  sequence or install steps.
- No paragraph longer than three sentences.
- One line per paragraph, no hard wrapping: phpBB renders every newline
  as a line break, so wrapped prose posts as ragged short lines. Only
  code blocks keep internal newlines.
- Do not use `[b]Term[/b] — description` inline-header list items. Write
  `[b]WORD[/b]` followed by a plain description with no dash pivot.
- Use bold for scanning only. Heavy bold reads as machine formatting.

## Voice

Write as me. Specifically:

- First person where I have a stake in a decision. "I made program-local the
  default because..." is right. Impersonal passive everywhere is wrong.
- Contractions throughout. "doesn't", "isn't", "won't", "it's".
- Plain statement over performance. State what a thing does, not why it is
  interesting.

## Banned constructions

These are the ones I keep having to remove. Do not produce any of them.

- Em dashes. Use a comma or a full stop.
- Rhetorical antithesis: "X rather than Y", "not X but Y", "no X, no Y",
  trailing ", not Z". Only keep a contrast when the contrast IS the fact, as in
  "returned 4320 instead of 5040".
- Counting or signposting headings: "Two things to know", "The one trap",
  "What is here". Name the actual subject instead.
- Trailing clauses bolted onto a finished sentence: ", which is why...",
  ", which is what...".
- Reusing a sentence that appears in another document. Say it differently.
- Anecdotes. Do not write how something was discovered, debugged or fixed. If a
  sentence explains how I found something rather than what it does, cut it. Bugs
  appear as behaviour-change notes, never as stories.
- Inventing an event. If you do not have evidence something happened, do not
  write that it happened. No "after getting burned by this", no "took an
  embarrassing amount of debugging".
- Closing questions that fish for replies.
- Summary or wrap-up paragraphs that restate what was already said.

## Rhythm

Vary sentence length deliberately. Several sentences under eight words. Do not
let every sentence land between thirteen and twenty-five words, which is the
single most reliable sign a machine wrote something.

Do not lean on one connective. `, so` should appear in at most one sentence in
eight. Vary with a full stop, a semicolon-free split, or a restructure.

## Accuracy

- Never state a figure, register value or limit I have not given you. If you
  need one and do not have it, write `⟨NEED: description⟩` and continue.
- Never claim testing that I have not told you happened.
- Reproduce listings and tables character for character.

## Output

Give me the post as raw BBCode in one block, ready to paste. No commentary
before or after it. If you had to insert any `⟨NEED: ...⟩` markers, list them
underneath the post.
