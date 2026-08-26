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

## r2 rejected — framework iterated, r3 drafted (2026-08-25)

Stan on r2: "Better, but too many AI-generated tell tales slipped
through. Still a shit framework, need to iterate further." Diagnosis
against the published v0.3 artifact and the trap catalog, tell by tell:
two segue hypophora ("So what are you looking at?", "What about states
too big to keep?"), a twist-tailed analogy ("except the calculator makes
them for you"), "like the solver never ran", the fabricated vignette
("you usually notice a mistake three operations later"), "isn't just X.
It's Y" split across the sentence boundary the aiaudit regex couldn't
cross, a no-judgment hedge ("I think that's the fastest way"),
"surprisingly handy", "Here's ENTER...:", "exactly one step", "drops you
straight into", and the fake-humble closer ("I'm sure there are corners
I haven't hit"). Root cause: voicematch2 v1 set corpus floors with no
caps, so drafting to the tool stuffed argument-register constructions
into a release post; it even REQUIRED a question in release prose.

Framework changes: voicematch2 register-conditioned (release/update/
chat) with caps as blocking as floors; the question rule corrected
against the artifact (his published post carries two content-bearing
reader questions, one paragraph-opening; segue formulas are what died,
forbidden in any position); floors scale with prose length; eight new
aiaudit classes for the tells above. Calibration now reproduces all
three of his verdicts: v0.1 BLOCKING (absence), r2 BLOCKING (stuffing:
analogy + both segues), published v0.3 clean under --register update.

r3 (undo-history-v0.1-r3.txt) is r2's surgery: every tell above removed,
the him-ness kept (I-lead, one plain analogy, stakes, comma splices,
fronted But/So), one grounded hedge added ("I think it's quicker than
digging U.HIST out of the STK menu" — a real comparison), and one
content-bearing question in the published post's exact shape ("Why skip
instead of dropping older levels to make room?" — the documented design
reason). Battery: voicematch2 release all ok; aiaudit 5 flags (4
rule-of-three enumerations + instead-of-pivot on the real design
alternative, judged keep); framescan shapes are enumeration frames, no
tails, no shingles. Cross-model round on r3 launched.

Cross-model round on r3 (readers verified: Gemini 3.1 Pro (High),
GPT-5): both human verdicts. Gemini's comma-splice observation is the
right kind of evidence ("a massive tell that a human wrote it" — they
are corpus-native). Dispositions: parenthetical-reason bullets and the
UNDO-subject openers KEPT (his ruled shapes; auditor polish-push);
"Why skip instead of dropping older levels" KEPT (content-bearing
reader question, the published v0.3 shape; Codex itself says leave it);
ONE fix applied — the comma before "without restoring anything" dropped
(bolted-on rhythm, same words). Battery re-run clean after the edit.
Exit criterion met: scanners clean-as-judged plus clean-as-judged
audits from both non-drafting models in the same round. Stan reads r3;
the FACTS sheet stands if he prefers to write from it. Open items
unchanged: unpushed base commit, thread placement, attachment indices.

## The dash finding (2026-08-25, Stan reading r3)

"reading this I realize, a dash can easily be confused with a
substraction." Confirmed worse than prose: ITM_SUB's catalog name IS
"-" (items.c row 96), so in the view itself a value-entry row and a
subtraction row were indistinguishable — a negative value entry
rendered "- -3.". Fixed in the package red-first: B8 scans the whole
item namespace against both synthetic labels (red on the shipped "-",
green after), placeholder now "(val)" — parenthesized like "(now)" so
the meta-label namespace is disjoint by construction, 5 glyphs so the
label column keeps its gap ("(entry)" at 7 touched the preview column;
seen on the LCD, rejected). Gate green solo+combined; all three shots
re-captured with the recovered marker-block drivers (removed again,
post-removal gate green); zip rebuilt; flash re-measured +3904 B total
(was +3896). Post change is one factual marker in the same sentence
("a dash" -> "(val)"), battery re-run clean; no new lane round for a
verified-fact substitution, logged as such.

## Stan's edit adopted; flag-picker shot; PR line (2026-08-25)

Stan reviewed r3 and returned his own rewrite — adopted byte-for-byte
as undo-history-v0.1-final.txt (his grammar untouched, per the standing
rule). Every claim his edit changed was re-verified: "HCLR ... does not
affect stock UNDO" (fnHistoryClear touches only package counters, never
thereIsSomethingToUndo or the SAVED_* bank), the shortened REDO bullet,
the reworded ~ paragraph, all numbers unchanged. His deltas are logged
as register calibration in the voice profile: both I-hedge lines and
the analogy deleted, impersonal open, the design-justification question
KEPT — voicematch2's release i_hedge floor is now 0; the tool flagged
his own text before the fix, which is the definition of a wrong floor.

Two additions he asked for, drafted clean and gated: the SYSFL picker
shot (undo-attach-4-sysfl-uhist.png — UHIST is index 100 of the
GENERATED 113-entry menu_SYSFL catalog, so the picker carries it with
no menu patch; page 90 rendered via refreshScreen, flag set for the
filled radiobutton; two wrong-page attempts recorded: 108 showed the
printer flags, 114 was off the 18-alignment and drew blank) with the
one-line caption, and the closing PR-offer sentence built from his own
words ("If people think it's useful I can create a pull request for
it, since it's made with that in mind"). Battery on the assembled
final: aiaudit 5 (the kept question's instead-of + 4 factual
enumerations), framescan no tails/shingles, voicematch2 clean after
the floor fix. Cross-model round on the final launched; only the two
added sentences are actionable — his text is the author's.

Cross-model round on the final (readers verified: Gemini 3.1 Pro,
GPT-5): no actionable findings. Gemini's "slogan-like fragments" are
Stan's own Limits bullets (author's text, not in scope); Codex flags
the PR line's "since it's made with that in mind" as an impersonal
tail — it is Stan's own phrasing from his request, used nearly
verbatim per the ruled procedure, KEPT. Both lanes independently chose
to leave the Why-skip question; Stan's own edit had already kept it.
Exit criterion met. Remaining are his-side items: upload order decides
the attachment indices (post encodes 3/2/0/1), the base commit line
needs the branch pushed or restated, thread placement.

## Final version locked (2026-08-25, Stan's second pass)

Adopted verbatim. His changes: the PR offer moved up front, reworded
impersonal ("This is built with the hope to be submitted as a PR to
upstream if people think it's useful"); "the UHIST flag" named in the
convenience line; my picker caption deleted (the attachment rides
under the convenience line); the restored-stack and gap shots DROPPED
with their prose — the forum allows three attachments and the zip
takes a slot; the package-manager thread URL added to Install
(t=4876, his thread; external verification blocked by 403, author-
verified). Checks: no dangling references to the dropped shots, three
attachments exactly, BBCode url tag well-formed, numbers unchanged.
Battery re-run for the record. Standing rule recorded in DESIGN.md:
plan future posts around the three-slot limit, collate related
pictures into one image.
