# undo-history-v0.1.txt — audit log

Drafter: Claude (Opus 5), 2026-08-25, per the model-assignment ruling.
Audit pool: Gemini 3.1 Pro via agy (model flag before -p, reader name
verified in reply line 1 every pass), GPT-5 via codex exec (reader name
verified). Corpus sample (~8k of the Reddit comments) included in every
audit prompt.

## Facts verified before drafting

- Depth figures recomputed from the entry layout: 32-byte header + 24 per
  real34 register; ~25 levels at SS=4 plain reals, ~16 at SSIZE8, 48 cap.
- Flash delta re-read from the final dmcp5r47 log: +3832+64 = 3896 -> "3.9 KB".
- Pool figures from the RCL58 free-list dump (29,820 requested vs ~31,236
  available): "about 5 KB to spare".
- Zip contents listed: patches/, files/, COPYING present (pkg_build ran the
  full suite green before zipping; 44 KB, under the 1 MB cap).
- Item names, flag name, menu placement checked against items.c rows
  427-429/2299 and softmenus.c menu_STK.
- Screenshots: forum/screenshots/undo-attach-{1,2,3}, provenance appended
  to forum/screenshots/README.md; behaviors pinned by battery B1-B6 and
  ring case R5.

## Rounds

**Round 0, scanners.** aiaudit 5 flags, all rule-of-three on real
three-item facts (columns list, session inventory) — judged keep.
framescan: no contrast tails, no shingles, 1 colon-before-code (the
published package-v0.3 post uses the same shape) — keep. voicematch:
impersonal vs corpus; judged against the release-register ruling
(questions 0 is correct for release notes, bullets stay impersonal), added
contractions and prose-I only.

**Round 1.** Gemini (verified): verdict human; structural notes on
end-parentheticals (his ruled aside habit — keep), comma splices x4 (two
were my CV-chasing edits — fixed two, kept two), trailing "same/exactly
as" x3 (varied two). GPT-5.4 (verified): the gap paragraph's engineered
So/The/Numbering/But cadence (real — my overcorrection scar, rewrote
plain with the reason back in a parenthetical), "budget's tight for a
reason" slogan splice (removed), "covering the undo semantics" register
slip (fixed), "complete machine" inventory (simplified), "can be assigned"
in a bullet (bullets impersonal by ruling — keep).

**Round 2.** Gemini (verified): clean as judged — only residual is
parenthetical caveats on bullets, which is the ruled changelog style.
GPT-5 (verified): line-58 test summary still promotional (split into two
sentences, second one carries "My own tests"), "doesn't get stored"
passive-where-stake (now "doesn't fit and I skip it"), the licence
fragment (full sentence now, also removed accidental closeness to the
package-v0.3 closing per the no-reuse rule).

**Round 3.** Both lanes re-run on the fixed draft. Results recorded below
when in.

## Open items for Stan

- Package base line quotes r47forth commit faf9d698c. That commit is on an
  UNPUSHED branch. Push the branch stack (or restate the base) before
  publishing, and run the §5 gates (repo 200 check, licence, BBCode).
- Thread placement: drafted for the custom package thread (t=4876). Your
  call.
- Attachment indices in the draft assume 3 images; the zip attachment
  shifts phpBB numbering. Set [attachment=N] at upload.
- The zip is pkg_dist/undo-history.zip (suite ran green in pkg_build
  before zipping).
- Hardware: the post says sim-only and it is. If you flash and run it
  before posting you can drop that line.

**Round 3 results.** Gemini 3.1 Pro (verified): flags the end-parentheticals
x8 and the telegraphic fragments — both structures its OWN round-1 and
round-2 passes explicitly endorsed keeping, and six of the eight
parentheticals are the ruled bullet-caveat style; judged not real. One real
micro-hit: "carries" is a formal verb, changed to "has COPYING inside".
GPT-5 (verified): flags the balanced "and"-joined clauses that round 2's
splice fixes created — the documented seesaw (fixing one frame inflates the
next), judged not real; calls the 5 KB pool parenthetical an anecdote — it
is a present-tense behavioral fact stating a limit's reason, not a
discovery story, judged not real; flags the closing fact-stack — that is
the Tier-2/3 required content in the same shape the published
package-v0.3 closing uses, judged not real.

**Loop verdict.** Findings stopped being real in round 3 (one lexical fix
applied). Both lanes' overall verdicts in every round: human-written.
Stopping per the seesaw rule. Stan's read is the remaining gate and
outranks all of the above.

## Redraft r2 under the rebuilt framework (2026-08-25)

Stan rejected the v0.1 draft ("doesn't sound like me at all") and ruled
the framework rebuilt from the Reddit data. undo-history-v0.1-r2.txt is
the first draft written the new way: composed with the exemplar bank
open, constructions from the measured profile used where they do real
work (the emulator save-state analogy, two explanatory questions,
I-hedges on genuine judgments, direct-you, fronted connectors,
enthusiasm on the view, a warm closing line). Facts unchanged from the
FACTS sheet.

Battery: voicematch2 all constructions present (the rejected draft
scores 6 MISSING on the same tool); two tool calibrations landed while
running it ("like save states" and "I'm sure/I'd" added to regexes,
skeleton metric marked informational). aiaudit 4 flags, all factual
enumerations. framescan: no tails, no shingles, 2 pre-code colons
(published shape).

Cross-model round (readers verified: Gemini 3.1 Pro, GPT-5): both
human verdicts, no category-1 findings. Both note the hypophora "So
what are you looking at?" as the first place a suspicious reader might
pause; disposition KEEP, it is his corpus construction ("Now is it a
wing chun stance...?") and removing it is the old framework's exact
failure mode. GPT-5 notes comma splices as possible overcorrection;
they are corpus-native (Gemini: "appear naturally throughout his
provided Reddit samples"); keep.

Stan reads r2. Same open items as before: unpushed base commit, thread
placement, attachment indices.
