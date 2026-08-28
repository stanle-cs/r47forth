# pretty-print v0.1 post — handoff

Stan rejected the drafted prose and rewrote the opening himself, then
stopped partway. This hands the rest over. The drafting half stands down;
what follows is status, his authoritative text, two fact-checks against
it, and what is left.

## Rule for whoever picks this up

**Stan's text below is authoritative and gets adopted byte-for-byte**,
including its grammar. Do not polish it and do not imitate its roughness
(write-as-stan: "drafts are written clean and he roughens where he
pleases"). Only the sections he did not reach get drafted, and each new
sentence runs the full loop: `forum/aiaudit.py`, `forum/framescan.py`,
`forum/voicematch2.py --register release`, then a cross-model pass with
the corpus, then his read.

His previous published announcement, `undo-history-v0.1-final.txt`, is the
calibration artifact. It scores `aiaudit` 4. Do not reuse a sentence from
it; framescan compares across files and will catch it.

## His text, as written (authoritative)

> Introducing pretty print for R47. This package draws out previous
> calculations made in the form of an expression like natural display on
> other mainstream calculators. I'd like to send this upstream as a PR if
> people find it useful, though it's a bit messy for now because it's made
> by Claude.
>
> Added functions:
> [list]
> [*]PSHOW: draws the steps that was done to calculated the current value
> of X on screen.
> [*]PHIST opens the formula browser. Move through it with UP and DOWN.
> ENTER to put the picked result into X, EXIT to leave. A row too wide for
> the screen pans with the .d key.
> [*]PCLR empties the formula history.
> [*]EQSHW draws the current equation in 2D on the pretty print browser
> (it sits on the EQN menu too).
> [*]PPON toggles natural display (PPRTY  flag), PTLIN toggles the formula
> line on T (PTLINE flag, replace register T with a small pretty print
> area).
> [*]PP menu under DISP, and in the catalog holds all 6 items.
> [/list]
> [attachment=2]pp-attach-1-stack-and-browser.png[/attachment]
>
> Top half is the stack with the formula line turned on. T carries the
> working and X carries what it came to, 3 1/3.
>
> Bottom half is the formula browser. Every row is one finished formula
> with its result, newest on top. The bar on the left marks the selection.
> The bottom row is a sum over a program called P with n running 1 to 10,
> sitting on a root.

Note he numbered the image `[attachment=2]`, so the zip is presumably
attachment 1. Keep the rest consistent with that.

## Two fact-checks on the above — both need his decision

These are checked claims, not style opinions. Fact-checking his text is
the pipeline's job when he writes it himself.

1. **"X carries what it came to, 3 1/3" — the shot shows 3 1/6.**
   The formula is `root(4/9) + (1/2 + 3/4) x 2`. Exactly:
   2/3 + 5/2 = 19/6 = **3 1/6**, and that is what the screen reads.
   Either the number changes to 3 1/6, or the shot is retaken with a
   formula that really does come to 3 1/3.

2. **"PSHOW: draws the steps that was done to calculated the current
   value of X" — PSHOW does not do this.**
   `fnPrettyShow` calls `ppBuildRegister(REGISTER_X, ...)`
   (prettyValue.c:856): it draws the VALUE of X full screen, in 2D, at
   the largest font that fits. It does not draw the steps. Verified
   directly: with a captured Sigma formula current, PSHOW rendered
   `192 1/2`, the value, not the formula.
   The surface that draws the STEPS is PHIST. If he wants a
   draws-the-steps item, that is a feature request, not a wording fix.

## What is left to write

He stopped after the browser paragraph. Still needed, and all of it is
already verified in `pretty-print-v0.1-FACTS.md`:

- the refusal behaviour (any failure draws nothing, the ordinary line
  renders; which types never get 2D)
- the equation constructs SUM/PROD/INTEG/DERIV, with the input-syntax
  note (root takes brackets, powers are `^`, lowercase is fine)
- the second attachment, `pp-attach-2-nesting.png`, and its value
  1.228593777031159439372254772764558
- Limits
- Install and the boilerplate: base commit, build commands, target,
  firmware size, licence, COPYING, backup-and-flash disclaimer

Everything past his stopping point in `pretty-print-v0.1-r5.txt` is the
rejected draft. Treat it as a source of FACTS only, not of sentences.

## Artifacts, all committed

| artifact | state |
|---|---|
| `pkg_dist/pretty-print.zip` | built, 118,450 bytes, COPYING verified inside |
| `forum/screenshots/pp-attach-1-stack-and-browser.png` | current, retaken 2026-08-27 |
| `forum/screenshots/pp-attach-2-nesting.png` | current, retaken 2026-08-27 |
| `forum/drafts/pretty-print-v0.1-FACTS.md` | verified fact sheet, measured not recalled |
| `forum/screenshots/README.md` | provenance for both attachments |
| `forum/drafts/pretty-print-v0.1-AUDITLOG.md` | round dispositions |

Source frames for both collages are in
`forum/screenshots/pretty-print-archive/`, so images can be re-cut
without recapturing.

## Blocking, and not mine to clear

**The base commit the post cites, `70f8b7db7`, is not on `origin/main`.**
90 commits are unpushed. Any reader who tries to verify the base or
rebase the package hits a commit that does not exist publicly. Pushing is
outward-facing and needs Stan.

The repo itself is public (`api.github.com` returns 200).

## Package state, unrelated to the post

- Gate green solo and combined at HEAD. Flash 1,146,336.
- Five audit rounds run. The exit bar (two consecutive rounds with no
  confirmed finding) is NOT met: round 5 found one.
- Known open: the pager has no "no formulas" empty state (cosmetic,
  deliberately not fixed); T25's oracle is partial.
- Not this package: undo-history leaks 9 memory regions in the headless
  battery (forth-only 147, forth+pretty 147, forth+undo 156, all three
  156). Measured, not fixed.
