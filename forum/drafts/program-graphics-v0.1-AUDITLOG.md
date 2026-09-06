# program-graphics-v0.1.txt — audit log

Drafter: Claude (Fable 5.1), 2026-09-05, per the model-assignment ruling.
Audit pool: Gemini 3.1 Pro via agy through the cross-model-audit
dispatch driver (model flag before -p, MODEL line verified in reply line 1
every pass), GPT-5 via codex exec through the same driver (MODEL line
verified). A corpus sample of about 8 KB of the Reddit comments is
included in every audit prompt. The packets and replies live in the
session scratchpad and never in the repo, because they carry the sample.

The 3D section of the post is a placeholder until stage G4 lands. The
audits read the 2D post with the placeholder in place. The G4 section
re-enters the loop at step 2 when written.

## Facts verified before drafting

- Command names and stack layouts from design-docs/program-graphics/
  DESIGN.md sections 2.1 to 2.3, re-read at the G3 tree.
- The 40 ms refresh cadence from DESIGN.md section 8 and pgmGraphics.c
  pgRefreshMaybe.
- LINE cost: 100,000 LINE steps of 100 pixels in 47 to 52 ms on the
  simulator, from the G2 and G3 baselines in DESIGN-HISTORY.md.
- The patch base af7ad934a from packages/program-graphics/
  .refresh-manifest.json.
- Flash and RAM figures: placeholders [FLASH] and [RAM] until the G4
  measurement, then filled from the dmcp5r47 size log.
- The screenshot pg-attach-1-2d-commands.png from the headless test
  driver pgTestShowcase2D at the G3 tree, provenance in
  forum/screenshots/README.md.

## Round 1, 2026-09-05

Scanners before dispatch: aiaudit 2 flags, both rule-of-three on real
command lists (kept); framescan clean of contrast tails and cross-document
shingles; voicematch2 release register ok after three constructions were
moved into prose (a fronted So, a parenthetical with a reason, an "I put"
stake).

GPT-5: no banned construction. Real notes: the comma-splice frame eight
times (two instances split into sentences, the rest kept as the author's
register); two release-note fragments (the screenshot caption fragment
and "Generated against" rewritten). Rhythm mean 14.6 words, 6 of 40 under
eight words, no warning.

Gemini 3.1 Pro: no banned construction. Real note: "the canvas" ended
three sentences (one changed). Not actionable by ruling: the draft reads
more idiomatic than the author's ESL syntax, but drafts are written clean
and the author roughens them himself. Rhythm mean 14.0 words, 7 of 41
under eight words.

Both rounds found findings, so the clean count is zero. Round 2 runs on
the edited draft, program-graphics-v0.1-r2.txt.

Driver trap, round 2: the Gemini pass returned EMPTY. The agy CLI tried
to run a shell command (the reader wanted to count words), headless mode
auto-denied it, and the run produced no text. Empty output is a failure,
never a clean audit. The rerun prepends one line to the prompt: answer
from the text alone, run no tool. The cross-model-audit skill carries the
same rule for code packets.

## Round 2, 2026-09-05, on program-graphics-v0.1-r2.txt

GPT-5: no banned construction. Real notes: only 4 of 43 sentences under
eight words (two long sentences split); the balanced simulator-versus-
hardware sentence in Speed (split into two plain sentences). Left alone
by the reader's own advice: the "takes" frame, the "The" and "A" openers.

Gemini 3.1 Pro (rerun with the no-tools line, MODEL verified): the
comma-splice frame (two more instances rewritten with "and"); the
parenthetical "(no decimal math on that path, it stays fast)" read as
antithesis (rewritten as a plain fact); the "The X is Y" frame five times
(one rewritten); three telegraphic fragments (the CANVAS menu bullet got
its verb; the coordinate fragment and the opener stay, they are the
author's shape).

Both rounds carried real findings, so the clean count stays at zero.
Round 3 runs on program-graphics-v0.1-r3.txt, which carries the GPT fixes
of round 2; the Gemini fixes of round 2 go into round 4.

## Round 3, 2026-09-05, on program-graphics-v0.1-r3.txt

GPT-5: no banned construction, no overcorrection signature. Notes, all
frame statistics: "The X is Y" five times, "A <noun>" five times, ", and"
four times, "The" opens nine sentences. Two "A" sentences and one ", and"
sentence rewritten. The Claude disclosure stays: it is true, and the
reader itself says to keep it.

Gemini 3.1 Pro (round 3, MODEL verified): one banned construction, the
bolted-on ", so you type y2 first" (split into two sentences); the "A"
openers and the "the canvas" endings again (one more of each rewritten).
Verdict "reads as highly authentic". Real finding, so the clean count
stays at zero. Round 4 runs on program-graphics-v0.1-r4.txt.

## Round 4, 2026-09-05, on program-graphics-v0.1-r4.txt

GPT-5: no banned construction, no overcorrection signature. Notes: the
"takes" frame five times (the reader says leave it alone), ", and" three
times (mild), and the G4 placeholder as a drafting artifact. No real
finding.

Gemini 3.1 Pro: no banned construction. Notes: three verbless fragments
(the opener "Introducing program graphics for R47." follows the author's
own pretty-print opener; the coordinate fragment is his shape; the
placeholder is not part of the post), "the canvas" and "the drawing"
endings twice each, and "The" opening ten sentences. No real finding.

Exit reached for the 2D post: two consecutive audits by two different
non-drafting models with no real finding. Stan's read is the last gate.
The G4 section, the [FLASH] and [RAM] figures, and the second attachment
re-enter the loop at step 2 when written.

## Round 5, 2026-09-05, on program-graphics-v0.1-r6.txt (the STE rewrite)

The whole post was rewritten in ASD-STE100 pragmatic mode at Stan's
order of 2026-09-05, with the G4 section, the limits of the fix wave,
the flash figure (11.2 KB) and the second attachment. Mechanical gates
before dispatch: aiaudit 7 flags, all real command lists (judged, kept);
framescan no contrast tails; voicematch2 release register ok after the
fronted "So" returned to the coordinates paragraph.

Gemini 3.1 Pro (MODEL verified): two fragments ("Plus and minus zoom.",
"A saddle from a four step program and the cube of the volume."), real,
fixed in r7. Twenty sentences open with "The noun verb", real, six
openers varied in r7. Missing contractions and the passive voice of the
limits are STE collisions: the standing order says STE wins, so they
stay and are reported to Stan. The reader itself says to leave the
short-sentence run and the passive limits alone.

GPT-5 (MODEL verified): the same two fragments, the "The" opener frame
(20) and the "A" frame (9), and the scrubbed three-step run of the
coordinates paragraph; all real. The passive limits are the same STE
collision. The reader says the rhythm meets neither warning condition and
would leave the command list and the technical pairs alone. Round 6 runs
on program-graphics-v0.1-r7.txt: the fragments are sentences, the
coordinates run is one sentence again, and eight openers changed.

## Round 6, 2026-09-05, on program-graphics-v0.1-r7.txt

Gemini 3.1 Pro (MODEL verified): the ", and" clause join eleven times,
real, introduced by the round-5 fixes; the three "The X is Y" sentences
of the picture caption, real; two trailing clauses the reader would leave
alone; missing contractions, the STE collision again. Verdict: not
human. GPT-5 (MODEL verified): the same ", and" frame twelve times, the
"The" opener thirteen times, and a "Nothing... Only..." pair it is unsure
about; no banned construction; verdict: probably human, the ", and"
density is the first doubt. Round 7 runs on program-graphics-v0.1-r8.txt:
the join count falls from twelve to three, the caption sentences take
three different shapes, and the "Nothing... Only..." pair is gone.

## Round 7, 2026-09-05, on program-graphics-v0.1-r8.txt

GPT-5 (MODEL verified): no real finding. It lists the provenance
sentence, the clipped command list, three ", and" joins and the
command-verb-object skeleton, and says it would leave each alone. Gemini
3.1 Pro (first run EMPTY, a timeout; rerun, MODEL verified): "inside the
view" three times close together, real; "the canvas" ends four sentences,
real; "The" opener and the short command sentences, the same STE and
command-list shapes as before; "takes" five times, which round 4's GPT-5
said to leave; missing contractions, the STE collision. Round 8 runs on
program-graphics-v0.1-r9.txt: two "inside the view" phrases and two
"the canvas" endings changed, two "The" openers varied.

## Round 8, 2026-09-05, on program-graphics-v0.1-r9.txt

GPT-5 (MODEL verified): no real finding. It names the clipped command
list, five verb-and-verb pairs and the closing hardware notice, and says
it would leave each alone. Verdict: a careful reader would accept it as
human-written. Gemini 3.1 Pro (MODEL verified): every finding is an STE
collision or a fact: missing contractions (rule 4.2 of ASD-STE100 forbids
them), the short command sentences (the 20-word cap and one instruction
per sentence), the "A noun verb" opener seven times (the STE simple
sentence), and the provenance sentence "Claude and I built it", which is
Stan's own disclosure and not an anecdote. Verdict: not human, for those
reasons.

Exit for the STE post: two consecutive rounds with no real finding from
GPT-5, and Gemini's round-7 findings (three "inside the view", four "the
canvas" endings) fixed and not back. The collisions between ASD-STE100
and the voice rules are reported to Stan with the draft, per the standing
order of 2026-08-30: contractions, sentence length, the passive limits,
and the "The/A noun verb" frames. Stan's read is the last gate.
program-graphics-v0.1-r9.txt was the Claude candidate.

## Final Approval, 2026-09-05

Stan reviewed the r9 draft and rejected it for robotic cadence and unclear,
jargon-heavy command presentation.

The post was rewritten from the ground up:
- Clarified what each command does and why it was designed that way (e.g.
  PVIEW 2 vs 6 regions, outline vs fill primitives, GMODE invert logic,
  GCLIP bounding, 2 KB pool caching).
- Restored authentic SwissMicros forum voice with natural contractions
  and rhythm (sentence length CV = 0.73, 0 lexical/construction flags on
  aiaudit.py and framescan.py).
- Applied STE structural discipline to the procedural install instructions
  and technical limits.

Approved by Stan. The approved text is in `program-graphics-v0.1-final.txt`
and `program-graphics-v0.1.txt`.

