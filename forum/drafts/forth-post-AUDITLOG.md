# forth-post.txt — pipeline log

Drafter: Fable, 2026-08-03, with the write-as-stan skill and the
approved package post as the register model. Every §7 figure
re-verified against source before drafting: the 19-word prim set read
from forth_prims.c, limits from the defines (names 31, source 256,
rstack 64, runaway 4096), FDEMO register values from
test_showcase_program, SAVE schedule from test_savings_program, spill
behavior from DESIGN §3.4/§5.7.

Two stale §7 inventory items corrected in DESIGN.md before drafting:
the pre-D3 recursion items (full-stack refusal, "needs the 8-level
stack") — the spill landed after the inventory was authored, 7 FACT =
5040 is the pinned truth. RELEASE_README carried the same dead claim
and was fixed. The flashable r47-dmcp5.zip was rebuilt at the release
base (the repo-root copy was pre-migration).

Six screenshots placed inline as phpBB attachments. Indices assume
upload order shot1 through shot6 (phpBB numbers them in reverse, so
shot1=[attachment=5] ... shot6=[attachment=0]). If Stan uses Place
Inline instead, his tags replace these.

Hardware claims: none. The post says sim-verified, R/S wired for
hardware, not flashed yet, flash at your own risk — inside §1's
hardware-claim rule.

## Audit round (both lanes, corpus-grounded, fresh)

framescan across both posts caught five closing-section shingles
reused from the approved package post; all five rewritten (Post A is
frozen). Convergent real findings from the lanes, all fixed: a
counting heading ("Two sample programs" — same class as Post A's "Two
constraints", now "The sample programs"); the 4320 clause reading as
debugging history (dropped, current behavior only); "kitchen-sink
program" and "just works" as unattested idioms; "R47 wins applies"
clunk; "on first touch" jargon; assembled-in-stages sentence in Rough
edges; ", so" over cap (6 -> 2). Ruled not-real: short UI-fact
fragments (EXIT/EDIT lines), the RCL "now" change-marker, Gemini's
structural complaints about the locked list format.

Final numbers: contractions 2.66, CV 0.61, questions 1.9%, first
person 0.76 (low against his 3.84 — the post is more instructional
than Post A; his read decides if it needs more I), cross-shingles
zero, banned shapes zero, 6 judged-factual flags.

## Remaining

- Stan's read (voice and content, the gate).
- At posting: upload shot1..shot6 in order, then r47-dmcp5.zip
  (rebuilt 2026-08-03 at 00.109.04.00b0) to the release branch; §5
  gates; the Forth thread per the locked threading decision.

## Stan's review, round one (2026-08-03)

The GLOBAL keyboard claim was imprecise and he caught it. Verified:
typed XEQ + alpha name runs a global (tam.c fallback, :997); the XEQ
label picker and PROG catalog are labelList-only and forth-core never
touches labelList — the spec's open item 1 records the invisibility as
a known deferred asymmetry. The paragraph now says both halves: runs
by typed name, won't appear on the picker or in PROG.
