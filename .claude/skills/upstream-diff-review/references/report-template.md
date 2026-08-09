# Review — upstream-diff minimality, <DATE>
<!-- Copy-adapt this template. Filename:
     design-docs/forth-core/REVIEW_upstream-minimality_<DATE>.md
     Every <...> gets filled or the section is deleted with a stated reason.
     The enumeration rule (D7-a) binds: every count states its command. -->

Subject: every generated patch in `packages/forth-core/patches/` at HEAD
`<COMMIT>`, against DESIGN.md:6-7/:1854 ("byte-identical to upstream
except the marked insertion").

Method: refresh-sync verified (<refresh run, git status clean/dirty>);
hunk-by-hunk read; mechanical tiers from
`.claude/skills/upstream-diff-review/references/patch_churn_scan.py`.

## The numbers

<N> patches, <N> hunks, <N> added lines, <N> deleted upstream lines
(scanner totals). Override files <N> (tracked budget 16). Concentration:
<top 3-4 patches by adds/hunks/dels>.
Prior run: <date, churn count, HEAD> — delta: <±N>.

## Verdict

<Two-to-four sentences: does the discipline hold, what class dominates the
avoidable surface, what single action buys the most.>

## 1. Churn — modified upstream lines carrying zero behavior

<Scanner WS-ONLY/COMMENT-ONLY hits grouped by class (wrap-reindent,
comment-attach, alignment), plus judged NEAR verdicts. State the fix idiom
per class (SKILL.md "Diff-minimal idioms"). Total count, and that the fix
is behavior-neutral by construction: gate + `git diff -w` on the shadow
tree.>

## 2. Extraction candidates — inline blocks that could be package files

<Per candidate: size, the upstream statics/macros it actually couples to
(grep evidence), the wrapper-seam shape that frees it, what it cuts the
patch to, and WHEN it should move (rebase-adjacent stage, between audit
rounds — never during one).>

## 3. Documentation drift

<Comments/records inside patches or files/ that no longer match the code
they describe — the C-4 class.>

## 4. Deliberately non-minimal, correctly — do NOT "fix" these

<Restate the deliberate-exceptions.md entries ENCOUNTERED this run, plus
any NEW deliberate shape found — which must gain a catalog entry with its
citation in the same commit as this report.>

## 5. Standing guard

Scanner count at this HEAD: <N> (command: <exact invocation>). <Whether
the churn gate is wired beside the group I pins yet; next run must state
its own count against this one.>
