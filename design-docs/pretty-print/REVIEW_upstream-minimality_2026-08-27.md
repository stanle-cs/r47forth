# Review — upstream-diff minimality, 2026-08-27

Subject: every generated patch in `packages/pretty-print/patches/` at HEAD
`6cc133c76`, against DESIGN.md:6-7/:1854 ("byte-identical to upstream
except the marked insertion"). Commissioned as a combined architectural,
design and implementation review against upstream.

Method: refresh-sync verified (`python3 tools/pkg_patch_refresh.py
packages/pretty-print` then `git status --porcelain packages/` — empty);
hunk-by-hunk read; mechanical tiers from
`.claude/skills/upstream-diff-review/references/patch_churn_scan.py`.
`references/deliberate-exceptions.md` read first — all 13 entries are
forth-core's; none covers this package, so nothing here is pre-sanctioned.

## The numbers

13 patches, 36 hunks, 745 added lines, 22 deleted/modified upstream lines.
Command:

    python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
        packages/pretty-print/patches/*.patch

Concentration is extreme and is the story of this review:

| patch | adds | dels | hunks |
|---|---|---|---|
| `solver/equation.c` | **589** | 1 | 4 |
| `keyboard.c` | 76 | 3 | 11 |
| `softmenus.c` | 18 | 2 | 4 |
| `items.c` | 17 | 9 | 4 |
| all nine others | 45 | 7 | 13 |

`solver/equation.c` alone is 79% of the package's added upstream lines.

Prior run: none — this is the first upstream-diff review of this package.

## Verdict

The discipline mostly holds, and where the package touches upstream it
generally follows upstream's own conventions: item rows fill `CAT_FREE`
spare slots, the menu registers at the table TAIL exactly as that table's
own repeated comment instructs, item entry points use
`fnPrettyXxx(uint16_t unusedButMandatoryParameter)`, and the display hook
is a `bool_t` try-function whose false return leaves upstream's arm
untouched. Deleted upstream lines are 22 and every one is justifiable.

One finding dominates everything else: **529 lines of package logic live
inside `solver/equation.c` while coupling to exactly ONE upstream static,
called once.** That is the textbook extraction candidate, and moving it
would cut the package's largest patch by ~95%.

The second finding is documentary rather than mechanical, and matters more
than its size suggests: **DESIGN.md's §6 hook table — the authoritative
inventory of what this package touches upstream, complete with
per-file sibling-adjacency analysis — describes 7 files and the tree has
13.** The two files it never mentions are `items.h`, which all three
packages patch, and `solver/equation.c`, the largest patch in the package.

## 1. Churn — modified upstream lines carrying zero behavior

**1 finding**, scanner exit 1.

- **WS-ONLY, wrap-reindent class — `solver/equation.c`.** The 2D strip
  hook wraps upstream's `showString(tmpString, &standardFont, 1 + X_OFF,
  …)` in a guard and re-indents it by two spaces, converting a pure
  addition into a modified upstream line.

  Fix idiom, from SKILL.md "Diff-minimal idioms": the **no-reindent
  wrap** — add the `if (...) {` and the closing `}` on their own lines
  and leave the enclosed upstream line at its ORIGINAL indentation,
  byte-identical. Behavior-neutral by construction; verify with the gate
  plus `git diff -w` showing the shadow tree unchanged.

  Note this is NOT the catalogued softmenus.c exception (entry 22), which
  stands because it applies upstream's own guard convention at a site that
  ignores a `bool_t`. No such ruling exists here.

## 2. Extraction candidates — inline blocks that could be package files

### `solver/equation.c` — 529 lines, coupling of ONE static

The PP14/PP16 construct block (`ppEqStackExceeded` … `ppEqBigopIntercept`,
package file lines 1715-2243) implements SUM/PROD/INTEG/DERIV for the
equation language. Evidence of its actual coupling:

    # upstream statics in equation.c: 12
    grep -c "^static" src/c47/solver/equation.c
    # of those, called by the block: 1  (_pushNumericStack, 1 call, line 2242)
    # upstream file-local macros (PARSER_*): 20 — used by the block: 0
    # upstream file-static variables used by the block: 0

`parseEquation`, the block's main upstream entry point, is **public** —
declared in `solver/equation.h:60`, defined non-static at
`equation.c:1273`. `EQUATION_PARSER_XEQ` is a public constant. The one
in-file comment claiming "the PARSER_* macros bind to this name" is
stale: the block uses none of them.

**Seam shape** — the package's own precedent (`paramCorePutLiteral`,
lblGtoXeq.c): a 3-line non-static wrapper beside `_pushNumericStack`
exporting it, and the block moves to `files/solver/prettyEqConstructs.c`.

**What stays inline afterwards:** the interceptor call in `parseEquation`
(:1341), its forward declaration (:710), the wrapper, and the strip hook
(:662). Roughly 15-20 added lines instead of 589 — the largest patch in
the package becomes one of the smallest.

**When:** rebase-adjacent stage work, never mid-audit. Relocating state is
this project's most dangerous fix shape, and this block is live audit
ground — round 4 asked its failure-path question only days ago.

### `keyboard.c` — 76 adds, 11 hunks, no extraction available

Hunk-dense but irreducible: five are the `fnKeyEnter/Exit/Backspace/Up/
Down/DotD` browser arms (one per upstream key function, each a genuine
hook), three are the softkey-containment range clause (see §4), and the
rest are the `determineItem` resolve arm and the containment guard. This
is the shape a modal browser costs in this codebase; undo-history's
equivalent patch is the same size. No finding.

## 3. Documentation drift

1. **The §6 hook table is stale and incomplete (the C-4 class, and the
   most consequential item in this review).** It lists `c47.h`,
   `screen.c`, `items.c`, `bufferize.c`, `calcMode.c`, `config.c`,
   `testSuite/*` — 7 files — and closes with "No patches to `stack.c`,
   `defines.h`, `keyboard.c`, `softmenus.c`, `statusBar.c` **until PP4**".
   The tree at PP16 patches 13 files. `defines.h`, `keyboard.c` and
   `softmenus.c` are legitimate post-PP4 additions the sentence
   anticipates, but **`items.h` and `solver/equation.c` appear nowhere in
   the table at all.**

   This is not pedantry: the table's stated purpose is per-file
   sibling-adjacency analysis, and it is what a future rebaser reads to
   know the surface. Measured now, the two undocumented files are in fact
   safe — no sibling patches `solver/equation.c`; `items.h` is patched by
   all three packages at :228/:481 (ours), :446/:2351 (undo-history),
   :2948/:3036 (forth-core), nearest approach 26 lines — but that analysis
   existed only in this review until now.

2. **`equation.c:1799` comment stale** — "the PARSER_* macros bind to this
   name" describes a coupling the block does not have (§2 evidence).

3. **DESIGN.md's PP5 roadmap row** says "one hunk at solver/equation.c's
   paint site". It is four hunks and 589 lines since PP14.

(DESIGN.md's calcMode row — "20 reserved (not wired)" — was the same
class; corrected in audit R5-3 the same day as this review.)

## 4. Deliberately non-minimal, correctly — do NOT "fix" these

Encountered this run. None were previously catalogued, because the catalog
is forth-core's; the first three below want entries of their own.

- **`items.c` ×9 / `items.h` ×6 — spare `CAT_FREE` rows replaced in
  place** (215-219, 459-462). Same sanctioned mechanism as catalog entry
  18: filling spare slots is how this codebase adds items; may fill, must
  not grow past `LAST_ITEM`. Not a finding.
- **`defines.h` — `NUMBER_OF_SYSTEM_FLAGS 64+48` → `64+51`.** The
  identical-edit claim: a single hardcoded count cannot be edited
  independently by two packages, so both carry the byte-identical line and
  3-way merge unifies them. Documented in DESIGN.md §composition.
- **`keyboard.c` ×3 — the softkey gates gain `&& calcMode < 19 /* package
  browsers 19-23, claims registry */`.** Landed by audit R5-3 hours before
  this review. Byte-identical to undo-history's, same unification
  mechanism. Modifying three upstream lines is the *point*: upstream
  enumerates its own browsers by name and a package browser is invisible
  to that list, so a package that ships a modal browser must extend the
  condition or its softkeys run underneath it.
- **`softmenus.c` ×2 — menu rows edited in place** to place `-MNU_PP` in
  a spare `ITM_NULL` slot of `menu_DISP` and `EQSHW` in `menu_EQN`'s tail
  row. Spare-slot filling again, same class as the item rows.

**Recommendation:** add the three novel shapes above to
`deliberate-exceptions.md` with their citations, so the next minimality
review does not re-litigate them. The catalog's own rule requires a
citation per entry; all three have one.

## 5. Standing guard

Scanner count at this HEAD: **1** mechanical churn finding (command in
"The numbers"). The churn gate is not wired beside this package's pins;
the next run must state its own count against this one.
