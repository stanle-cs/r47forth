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

## ChatGPT passes (codex exec --sandbox read-only, fresh each time)

**Pass 1** — verdict accept-as-human. One finding judged REAL: the
accidental "Nothing is silently replaced." / "A package declares
nothing." mirror across adjacent bullets. Fixed by deleting the
redundant first sentence (fails-loudly already carries the guarantee).
Its accuracy section independently endorsed every standing keep.

**Pass 2** (after that fix) — clean as judged; residuals were the ruled
keeps and opener counts at benign density. Gemini pass 4 meanwhile made
one new point with merit: all five changelog bullets shared a
lead-then-explain template. Taken as real; list restructured to varied
shapes (merge, inversion, single-sentence).

**Gemini pass 5** (restructured text) — verdict flips to human-written;
all three findings are standing ruled keeps. CLEAN.

**ChatGPT pass 3** (restructured text) — verdict accept-as-human; all
findings ruled keeps or benign counts except two micro-phrasings taken
as polish: the generic "and the problem goes away" close (truncated)
and a version-stamp line that near-duplicated INSTALL.txt's (reworded).

## Exit-criterion status

Ruled MET on this basis: Gemini pass 5 and ChatGPT pass 3 are
consecutive passes by the two pool models on the same text, each with
zero findings surviving judgment; the two polish edits after pass 3
were treated as figure-class touch-ups with a scanner re-run, not
defect fixes. A stricter reading would demand one more pair of passes
after those edits — Stan can order that with a word. Six passes total
sit in the scratchpad transcripts; the seesaw pattern (each pass
flagging the previous pass's cures) is documented above and is why
judgment, not zero-flags, is the standard.

Permanent ruled keeps, for any future reader: the "reads only X, never
Y" constraint sentence (flagged six times, kept six times — the binary
is the fact); "instead of the working copy" (literal file-target
choice); "declares nothing / no meson.build or file list" (§6's own
content item); technical passives describing mechanism.

## Remaining before posting

- Stan's read (the gate nothing substitutes for).
- §5 gates at post time: repo public, release branch + zip uploaded
  (c47-pkg-manager-v0.3.zip, rebuilt 2026-08-03, COPYING inside),
  BBCode balanced, thread t=4876.

## Register rejection and rewrite (2026-08-03, after the audit passes)

Stan's first read rejected the audited draft: "does not sound like me at
all." The model audits measured human-vs-machine, never Stan-vs-not-Stan
— a pipeline gap, now closed with voicematch.py (step 3b) and the §2
register ruling. The numbers that proved his read: first person 0.52 vs
his 3.84 per 100 words, "but" 0.00 vs 0.65, contractions at half his
rate, zero questions against his 11%.

Rewritten between his two measured registers (Reddit corpus and his
INSTALL.txt technical voice): first person 1.30, contractions 2.11,
but/though/probably/honestly present, one mid-post question, CV 0.58.
The facts and §6 coverage are unchanged.

The rewrite is a full prose change, so the two model lanes need a fresh
pair of passes AFTER Stan settles the register — auditing prose he may
still re-cut is wasted motion. Current state: awaiting his read of the
rewrite; then one Gemini + one ChatGPT pass on whatever he approves.

## Corpus-mechanical pass and corpus-grounded audits (2026-08-03, late)

Stan's second correction: INSTALL.txt and every project doc are ALSO
AI-generated. The §2 ruling was rewritten — the Reddit corpus is the
only voice anchor; "cut and dry" is a mode, not a file to imitate.

voicematch gained --attest (phrase-level: which ordinary-word pairings
never occur in his 47k corpus tokens). Five unattested constructions
fixed in the post (honestly-you-couldn't, fronted Probably, might-still-
work-though, worked-example, the fronted question fragment).

Both audit lanes then ran WITH an 8k corpus sample in the prompt,
instructed to flag voice deviations, terseness excluded. Their converged
real findings: documentation register in the mechanism paragraphs
(materializes, catch drift, produces no artifact, size gate) — all
replaced with plain verbs; agency added where he has a stake (I
generated everything against...). Their Reddit-costume suggestions
(smilies, Sup guys, self-deprecation) were ruled out: between his
registers, not parody.

INSTALL.txt and RELEASE_README.md were rewritten fresh through the same
process (facts identical, sentences differentiated from the post per
never-reuse), and the zip rebuilt with the new INSTALL.

Final tri-document pair: ChatGPT accepts all three; its residues (two
RELEASE_README wordings, one INSTALL pairing) were fixed. Gemini's
verdict text says no but its six findings are all settled dispositions
(the never-tail's seventh flag, the true first-person intro, the locked
list format). POST: clean pair as judged. Stan's read remains the gate.

## Human content review (2026-08-03, the deepest cut)

Stan's read found five things six model passes and every scanner
missed, all content and pedagogy:

1. The definition-by-absence bullet ("declares nothing... no
   meson.build or file list"). Corpus check: 1.1% of his sentences
   carry double negation, none definitional. Replaced with what a
   package is and what you do.
2. ./package is the built convenience and now leads the post; every
   instruction goes through it. Verified against the CLI: eight
   subcommands, bare package names accepted.
3. The materialize trade-off is now taught with its reason (hand-copy
   drags upstream drift; materialize pulls the file at the recorded
   base).
4. Package testing is explicit: make test CUSTOM_PKG=..., verified
   against the Makefile's test target.
5. Composition has a concrete loading-order example (alpha 005, two
   010s, tie by CUSTOM_PKG position).

DESIGN.md §6 carries the structure ruling. The restructure resets the
audit pairs; they rerun once Stan approves this shape. INSTALL.txt
still teaches raw tool paths and gets the ./package-first treatment
after the post's shape settles.

The lesson for the pipeline: the model audits and scanners police
voice and tells; they never noticed the post taught the wrong
interface. Content review is a distinct gate and stays human.

## Second human review round (2026-08-03)

Two more from Stan's read. First, a technical question the post now
answers inline: refresh diffs against the base-commit version always
(verified at pkg_patch_refresh.py generate_patch, base_bytes is the
file at base_commit), so a straight copy is harmless exactly when the
tree sits at the base, and polluted exactly when upstream touched the
file in between. Second, the materialize explanation was rewritten in
his own offered sentence shape ("if you just copy the file, there's a
chance your tree is on a different version..."), fact-checked true
before adoption. The drop-files-in bullet no longer implies bare
copying covers upstream files.
