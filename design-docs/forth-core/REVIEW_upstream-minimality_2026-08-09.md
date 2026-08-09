# Review — upstream-diff minimality, 2026-08-09

Subject: every generated patch in `packages/forth-core/patches/` at HEAD
`88703343f` — the full upstream-modification surface, reviewed against the
binding rule (DESIGN.md:6-7, restated at :1854): overrides *"add the
documented hook lines and nothing else"* / *"keep every override
byte-identical to upstream except the marked insertion."* The point of the
rule is migration: every upstream line a patch touches, and every context
line near an insertion, is a potential `--rebase-base` conflict.

Method: hunk-by-hunk read of all 18 patches; every deleted/added line pair
classified (hook / inline logic / justified modification / churn); the
mechanical tiers re-checked by
`.claude/skills/upstream-diff-review/references/patch_churn_scan.py`
(written for this review, now the standing tool). Refresh-sync verified:
`pkg_patch_refresh.py` regenerates the committed `patches/` byte-identically
— no working-area drift.

## The numbers

18 patches, 139 hunks, ~3,066 added lines, ~398 deleted upstream lines.
Override files 18 (tracked budget 16 — exceeded since round 8, known).
Concentration: `manage.c` +1,651 (one 1,281-line hunk), `keyboard.c` +558/31
hunks, `screen.c` +286, `tam.c` +190/28 hunks, `lblGtoXeq.c` −250.

## Verdict

The discipline is real and mostly holds: the small patches are clean
documented hooks, the big deviations are *designs with rulings* (§4 below),
and the two riskiest-looking shapes — the `_executeOp` deletion and the
D7-1 rename wave — are deliberate and correctly reasoned. What the review
found is one layer below that: **51 mechanically-confirmed churned upstream
lines** (whitespace-only and comment-only modifications) that are pure
avoidable clash surface, plus two large inline blocks that are extraction
candidates on the F13 precedent. Nothing found is a defect; everything
found is merge tax.

## 1. Churn — modified upstream lines carrying zero behavior (fix cheaply, one commit)

Every line below conflicts at `--rebase-base` time if upstream touches it,
and buys nothing. Scanner output is authoritative; the classes:

- **Wrap-reindent** (~35 lines): `keyboard.c`'s `determineItem` body
  (~20 lines re-indented inside the new `else`), `manage.c`'s
  `addItemToBuffer` arm (~13 lines), `keyboardTweak.c`'s `fnExitAllMenus`
  line, `items.c` two brace lines. The package already has the right idiom
  in two places — `screen.c`'s F7 guard and `keyboardTweak.c`'s triple-f
  guard wrap upstream blocks in `if/else` *without re-indenting the
  enclosed upstream lines*, which keeps them byte-identical. Apply that
  idiom to the two sites that didn't.
- **Comment-attached-to-upstream-line** (8 lines): the three
  `leaveTamModeIfEnabled();  /* D7-1 ... */` sites in `keyboard.c`, and
  `pemCloseAlphaInput()` / `defineFirstDisplayedStep()` /
  `_closeAlphaMenus()` / `return;` / `tam.function = ITM_LITERAL;` in
  `manage.c`. The annotations are valuable — the audit trail wants them —
  but they belong on their *own added lines* above the call, where the
  upstream line stays byte-identical and the comment survives a merge
  trivially.
- **Pure alignment churn** (3 lines): `manage.c` `(aimBuffer[0]) == 0`
  (parens added, nothing else), `char cursorByte` re-indent; `softmenus.c`
  `menu_ALPHA`'s `-MNU_MyAlpha` continuation row re-indented by one space.
- **Half-measure renumber** (1 line): `softmenus.c` renumbered TAMFLAG's
  `/* 022 */` comment to `/* 023 */` but not the rows after it, so the
  table now has two `/* 023 */`. Minimal form: renumber nothing (the
  comments are already positional fiction after a mid-table insert, which
  the order-must-match constraint forces — see §4).

Fixing all of this is behavior-neutral by construction (the gate plus a
`git diff -w` over the shadow tree proves it), shrinks the churned-line
count to zero, and makes the scanner a standing red/green check.

## 2. Extraction candidates — inline blocks that could be package files (schedule, don't rush)

Additive-at-one-point blocks are *lower* clash risk than modified lines —
they conflict only if upstream edits their context — so these rank below
§1 in urgency, but they dominate the added-line count and each is one
`--rebase-base` context collision away from a bad afternoon.

- **`manage.c`, the 1,281-line hunk** (~28 functions: the fold, FHIST,
  capture suspend/resume, `forthInteractiveEnter`). Its only genuine
  coupling to `manage.c` is four upstream statics: `_insertInProgram`
  (×5), `_closeAlphaMenus`, `_getProgramSize`, `_closeCatalog`. The
  package already holds the precedent for exactly this: `lblGtoXeq.c`'s
  `paramCorePutLiteral` is a 3-line non-static wrapper exporting a static
  to package code. Four such wrappers (~20 patch lines) let the whole
  subsystem move to `files/programming/forth_fold.c`, cutting the largest
  patch by ~80%. Belongs with F13's rebase-adjacent stage — same class,
  same timing, and the fold code is the audit's hottest ground, so moving
  it *between* audit rounds, not during one.
- **`screen.c`, the console view** (~250 lines). Self-contained except
  `_forthConsoleEditorTop()`, whose `checkHPoffset` is a screen.c-local
  macro. Keep that one function (plus the two `_Static_assert`s) in the
  patch as a small exported geometry seam; the renderer, clamp, and roll
  move to a package file. The invocation hook is already exemplary — one
  modified line, additive fall-through.
- **`keyboard.c`, the 155-line interactive EXIT ladder** — this is F13/U5,
  already ruled "rebase-adjacent stage." Nothing new; recorded here for
  the count.

NOT candidates: `items.c`'s `runFunction` divert (~55 lines — it *is* the
documented E0 seam and needs the surrounding dispatch context), the small
guards throughout `keyboard.c` (each is a genuine hook), and everything in
§4.

## 3. One documentation drift

`files/programming/param_core.c:6` still says *"byte-identical extraction
of the native parameter execution core."* It has not been byte-identical
since F2-2: bounded reads, `end` parameters on every signature, the Forth
XEQ fallback woven into `PARAM_LABEL`. This is the C-4 class (record
diverges from code) sitting at the top of the file every future
upstream-merge reader will open first. One-line fix: date the divergence
and point at DESIGN.md §10.2/F2.

## 4. Deliberately non-minimal, correctly — do NOT "fix" these

Recorded so a future minimality pass doesn't undo paid-for decisions:

- **`lblGtoXeq.c` deleting `_executeOp`/`_executeWithIndirect*` (−237).**
  DESIGN.md:1861: the extraction into `param_core.c` is the H2 hook shape,
  and deleting the superseded upstream copy is *clash-seeking on purpose* —
  an upstream edit there must conflict loudly at integrate time, not land
  silently in dead code. The fork's drift cost is real but chosen.
- **`tam.c`'s 26 `leaveTamModeIfEnabled()` → `_tamLeave()` renames.**
  D7-1 (approved 2026-08-08). The internal sites must NOT get the
  fold-settling public wrapper; the wrapper keeps upstream's name and
  signature precisely so un-overridden upstream callers inherit the fix
  through the link. The hunk count is inherent to the semantics. (The
  undefendable direction — a future upstream *in-file* caller of the
  public name — is C-5's documented residue.)
- **`forthUserItemDispatch` replacing the `CM_PEM ? insert : run` pairs**
  (items.c ×2, keyboard.c ×2, screen.c ×2). Same loud-conflict convention
  as H2, and the three-way dispatch cannot be expressed as an addition.
- **`items.c`'s XEQ dynamic-menu arm rewrite.** Forth resolution order
  (DESIGN.md §4) must be *inside* the resolution, and the comment carries
  the b8f79e486 rebase reasoning.
- **`softmenus.c` mid-table `softmenu[]` insert at 022** despite
  upstream's "add at the end" note: forced by the order-must-match
  constraint with `dynamicSoftmenu[]` (upstream's own comment,
  softmenus.c:1021-1028, cited by P-H5).
- **`defines.h` 22→23**: upstream's own documented procedure for adding a
  dynamic menu (P-H6).

## 5. Standing guard

`patch_churn_scan.py` exits 1 on any WS-ONLY/COMMENT-ONLY hit. Once §1
lands, wire it beside the group I pins (design-audit.sh) so churn cannot
re-accumulate silently — the D7-a lesson applies: this review enumerated
sites, so it carries the grep and the count (51, at this HEAD), and the
next run must state its own.
