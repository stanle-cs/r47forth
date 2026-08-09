---
name: upstream-diff-review
description: Review the package's generated patches for upstream-diff minimality — keeping every override byte-identical to upstream except sanctioned hook lines, so future upstream merges conflict as little and as loudly-on-purpose as possible. Use when asked to review upstream modifications, check patch/override minimality, audit migration-clash risk, prepare for an upstream rebase, or after any fix wave that touched override files. Metrics and churn come from references/patch_churn_scan.py — never eyeballed; deliberate non-minimal shapes are catalogued in references/deliberate-exceptions.md and are NOT findings.
---

# The upstream-diff review

**Before flagging anything, read `references/deliberate-exceptions.md`.
This project contains patches that are non-minimal ON PURPOSE, each with a
ruling behind it — "fixing" one undoes a paid-for decision. A reviewer who
skips the catalog re-litigates D7-1 and H2 every time, and both have
already been re-litigated to death.**

## What this review is

The package system (design-docs/package-manager/README.md) delivers all
work as generated `patches/` against upstream files plus `files/` with no
upstream counterpart. The binding rule is DESIGN.md:6-7 and :1854:

> overrides "add the documented hook lines and nothing else" — "keep every
> override byte-identical to upstream except the marked insertion; that is
> what keeps the generated diff small and future upstream merges
> reviewable."

Every upstream line a patch *modifies*, and every context line beside an
insertion, is a potential conflict when the package moves forward with
`pkg_patch_refresh.py --rebase-base`. This review measures that surface
and shrinks the avoidable part. It hunts merge tax, not bugs — functional
defects belong to `cross-model-audit`.

## Ground rules (all paid for; do not re-derive)

1. **Patches and `files/` are generated output.** Review them, but fix in
   the flat working area and re-run
   `python3 tools/pkg_patch_refresh.py packages/forth-core`. Editing
   `patches/` directly is off-path even if it works.
2. **Verify refresh-sync first.** Run refresh, then `git status
   --porcelain packages/` — any output means working area and generated
   output disagree, and every number you are about to compute is about the
   wrong tree. (The build reads only `patches/`+`files/`; a stale patch
   builds green with the package's edits silently absent.)
3. **Modified upstream lines outrank added lines.** A deletion or
   in-place edit conflicts whenever upstream touches that line; a purely
   additive hunk conflicts only on context collision. Rank findings
   accordingly. Fewer hunks beats fewer lines at equal behavior.
4. **Upstream-convention-first (owner standing rule, 2026-08-08).** When
   a change to an upstream file is unavoidable, its *shape* follows how
   upstream handles the same class at its own sites, and the commit names
   that site. This binds the review's recommendations too.
5. **Deletion of superseded upstream code is sanctioned clash-SEEKING**
   (the H2 convention, DESIGN.md:1861): when package code supersedes an
   upstream block, delete the block so upstream edits to it conflict
   loudly at integrate time instead of landing silently in dead code.
   Never flag such a deletion as non-minimal without checking the catalog.
6. **The enumeration rule (D7-a):** this review enumerates sites, so its
   report carries the grep/scanner invocation and the count, and each run
   states its own count against the last.

## Method

1. **Sync check** (rule 2 above).
2. **Metrics + mechanical churn:**
   ```
   python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
       packages/forth-core/patches/*.patch
   ```
   Exit 1 means WS-ONLY (whitespace-only) or COMMENT-ONLY (trailing
   comment attached to an unchanged call) modified upstream lines exist —
   always findings. NEAR hits are judged one by one: `(x)` paren churn is
   a finding; a real rename (`tmpChar` → `tmpChar4`) is not.
3. **Read every hunk** of every patch (deletion-heavy and hunk-dense
   files first) and classify:
   - **H — hook**: a sanctioned insertion (include, guard, call-out,
     early-return divert, table row). Check it against DESIGN.md §6's
     hook tables where covered. Good; no action.
   - **I — inline logic**: a multi-function or multi-screen block living
     inside the upstream file. Ask: what upstream *statics* does it
     actually call? (Grep the hunk's identifiers against `^static` in the
     upstream file.) If the coupling is a handful of statics, it is an
     extraction candidate — the package's own precedent is a 3-line
     non-static wrapper per needed static (`paramCorePutLiteral`,
     lblGtoXeq.c) with the block moved to a `files/` source. If the
     coupling is a file-local macro, consider leaving one small exported
     seam function behind (the `_forthConsoleEditorTop` shape).
   - **M — modified/deleted upstream lines**: check the catalog first;
     if not catalogued, ask whether a purely additive shape exists
     (early-return divert above the block; appended disjunct; the
     no-reindent wrap — see idioms below).
   - **C — churn**: scanner tiers plus anything judged behavior-neutral.
     Always a finding, always cheaply fixable.
4. **Write the report** from `references/report-template.md` (copy-adapt,
   never freehand): numbers, verdict, churn list, extraction candidates
   ranked by clash risk, the do-not-fix list restated, and the standing
   count.
5. **Fixes are the owner's call.** Churn fixes are behavior-neutral by
   construction — verify with the gate
   (`./packages/forth-core/build-test.sh`) plus `git diff -w` showing the
   shadow-tree result unchanged — but they still land as a normal commit
   through the working area, and extraction moves are rebase-adjacent
   stage work, not review-day work (the fold code especially: it is audit
   ground, and relocating state is the most dangerous fix shape).

## Diff-minimal idioms (the package's own precedents — copy these)

- **No-reindent wrap**: to make an upstream block conditional, add
  `if (guard) { /* new arm */ }` and `else {` on their own lines, leave
  the enclosed upstream lines at their ORIGINAL indentation byte-identical,
  close with an added `}`. Landed examples: screen.c F7 guard,
  keyboardTweak.c triple-f guard. Counter-examples that cost ~35 churned
  lines: keyboard.c determineItem, manage.c addItemToBuffer.
- **Comment on its own added line**: annotate an unchanged upstream call
  from an added line above it, never by appending to the line.
- **Seam wrapper**: export an upstream static to package code with a
  3-line non-static wrapper instead of moving or duplicating the static.
- **Early-return divert**: insert the package arm above the upstream
  block with `return`/`break`, so the block itself stays untouched.
- **Appended disjunct/row**: extend a condition or table by appending,
  never by rewriting the existing spelling (watch continuation-line
  alignment — do not re-align neighbors).

## Report and record

Reports are `design-docs/forth-core/REVIEW_upstream-minimality_<date>.md`
(series convention — filename from the subject, not an orchestration
slug). First run: 2026-08-09, 51 mechanical churn findings at
`88703343f`; see that report for the worked example of every
classification above.

## Subagent note

If any part of this runs in a subagent, the spawn prompt must INLINE
rules 1-6 and name `patch_churn_scan.py`, `deliberate-exceptions.md`, and
`report-template.md` as copy-adapt artifacts. "Invoke the skill and
follow it" alone has failed before (2026-08-04) and is off-path.
