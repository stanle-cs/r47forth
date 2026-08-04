# package-post.txt — pipeline log

Drafter: Fable (Claude), repo-connected session, 2026-08-03. Content:
DESIGN.md §6 inventory in order; every figure re-verified against source
(16 patches / 19 new sources counted from the tree; §6's "14/13" was
stale). Voice calibrated on forum/reference/reddit-trungdle.md.

Scanners: aiaudit 4-5 flags across revisions, all judged factual
enumerations or contrast-is-the-fact; CV 0.49-0.58 over the revisions.

## Gemini passes (agy -p --model gemini-3.1-pro-high, fresh each time)

**Pass 1** — 7 findings, 4 judged REAL and fixed: the "Two constraints"
counting heading; "No meson.build, no file list" (banned shape); two
staccato fragments I had added chasing CV; the ", all riding on this
system" participial tail; plus a near-reuse of INSTALL.txt's
"cheerfully compile" joke, caught while judging. Auditor's own accuracy
section said to KEEP "reads only X, never Y" and "instead of".

**Pass 2** — 8 findings, 2 judged real in part (opener monotony: 13 of
35 sentences on one subject-verb skeleton; the clipped v0.2 bullet).
Fixed with moderate variety. The rest re-flagged pass 1's fixes as
overcorrection or flagged ordinary technical register.

**Pass 3** — 5 findings, ALL judged not-real; this is the clean pass:

1. "reads only ... never your flat working area" — flagged third time;
   pass 1 itself ruled keep-for-accuracy. The binary is the §21 fact.
   KEPT, permanently ruled.
2. Headings "unnaturally neat" — flags the cure for pass 1's heading
   finding. §2 says name the subject; they do. KEPT.
3. "A failing suite produces no artifact" etc. as slogan-fragments —
   these are §6's load-bearing guarantees, not rhythm filler. KEPT.
4. "is refused ... doesn't" register mix — Stan's own INSTALL.txt mixes
   passives and contractions the same way. NOT-REAL.
5. "instead of" — pass 1 evaluated and kept it. KEPT.

Note for the next reader: passes 2 and 3 demonstrate the seesaw the
spec warns about ("fixing one frame inflates another"). The remaining
flags have ruled dispositions; do not reopen them without a new reason.

## Remaining before posting

- ChatGPT pass (the second clean pass the exit criterion needs): paste
  PROMPT_AUDIT.md + the draft into ChatGPT, or install codex
  (npm install -g @openai/codex; codex login) so it runs locally.
- Stan's read.
- §5 gates at post time: repo public, release branch + zip uploaded
  (c47-pkg-manager-v0.3.zip, rebuilt 2026-08-03, COPYING inside),
  BBCode balanced, thread t=4876.
