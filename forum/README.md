# forum/

Everything for writing and publishing forum posts to forum.swissmicros.com.

```
DESIGN.md            the specification. Read this first, always
PROMPT_WRITE.md      paste into ChatGPT to draft a post
PROMPT_AUDIT.md      paste into ChatGPT to audit a draft
aiaudit.py           phrase patterns, formatting tells, sentence-length stylometry
framescan.py         recurring frames, contrast tails, cross-document repetition
drafts/              current post drafts
screenshots/         sim LCD captures for the posts, with provenance README
reference/           voice reference corpus (LOCAL-ONLY, gitignored — see DESIGN.md §3)
```

## Order of work

1. Read `DESIGN.md`. It carries the locked decisions, the voice rules, the
   content inventories and the publication gates.
2. Draft. Ruled 2026-08-03: Fable (Claude) drafts in a repo-connected,
   deep-effort session against the spec's content inventory.
   `PROMPT_WRITE.md` is the fallback for drafting outside that setup.
3. Run both scanners:

   ```
   python3 forum/aiaudit.py   forum/drafts/<post>.txt
   python3 forum/framescan.py forum/drafts/<post>.txt
   ```

   Judge the flags. Roughly a third are factual enumerations that must not be
   rewritten. Driving the count to zero makes the text worse.
4. Audit with `PROMPT_AUDIT.md` in a fresh session of a **different** model
   from the one that drafted. Ruled 2026-08-03: the pool is ChatGPT and
   Gemini, one pass each, either order. This is the step that finds what
   the scanners and the author both miss.
5. Repeat step 4 until two consecutive audits by two different non-drafting
   models come back with no real findings. A real finding resets the count.
   The full exit criterion, the reply rule, the screenshot rules and what
   re-triggers auditing after approval are in `DESIGN.md` sections 2 and 3
   (ruled 2026-08-03).
6. Stan reads the result. Then check the publication gates in `DESIGN.md`
   section 5 before posting.

## Why the prompts exist

A model auditing its own output shares its blind spots. Rotating the reader is
the only thing that has reliably surfaced new classes of tell: a fresh pass
found five that repeated self-audits never saw, including damage left behind by
the previous round of fixes.

The scanners cover what can be checked mechanically. The prompts cover what
needs a reader. Neither can tell whether it sounds like you, which is why the
last gate is reading it yourself.
