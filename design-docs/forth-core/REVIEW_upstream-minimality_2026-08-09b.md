# Review — upstream-diff minimality, 2026-08-09 (post-wave)

Subject: every generated patch in `packages/forth-core/patches/` at HEAD
`29257b7f5`, against DESIGN.md:6-7/:1854 ("byte-identical to upstream
except the marked insertion"). This is the close-out run of the
consolidation wave `SPEC_consolidation-wave_2026-08-09.md` (P1-P10), read
against the same day's pre-wave baseline.

Method: refresh-sync verified (`python3 tools/pkg_patch_refresh.py
packages/forth-core` then `git status --porcelain packages/` — clean);
hunk-by-hunk read of every patch that changed in the wave; mechanical
tiers from
`.claude/skills/upstream-diff-review/references/patch_churn_scan.py`.

## The numbers

19 patches, 139 hunks, 1,243 added lines, 340 deleted upstream lines
(`cat packages/forth-core/patches/*.patch | grep -c '^+[^+]'` and
`'^-[^-]'` and `'^@@'`). Override files 19 (tracked budget was 16 — see
§4). Concentration, by added lines:

| patch | + | modified/deleted | hunks | pre-wave + |
|---|---|---|---|---|
| `010-programming__manage.c.patch` | 345 | 10 | 21 | 1,651 |
| `010-keyboard.c.patch` | 328 | 28 | 29 | 558 |
| `010-ui__tam.c.patch` | 181 | 29 | 28 | 190 |
| `010-items.c.patch` | 98 | 17 | 9 | 109 |
| `010-screen.c.patch` | 73 | 13 | 7 | 286 |
| `010-c47Extensions__keyboardTweak.c.patch` | 40 | 0 | 4 | 66 |

Prior run: 2026-08-09 at `88703343f` — 51 mechanical churn findings,
3,066 added lines, 6 NEAR hits, 18 override files. Delta: churn
**51 → 0**, added lines **−1,783 (−58%)**, modified/deleted upstream lines
**−22** (keyboard.c 54 → 28, manage.c 26 → 10, softmenus.c 6 → 4,
keyboardTweak.c 1 → 0; items.c and lblGtoXeq.c unchanged, both
catalogued), NEAR **6 → 4**, override files **18 → 19**.

## Verdict

The discipline holds, and the avoidable surface is now small enough to
read in one sitting. Every mechanical churn tier is empty and the four
surviving NEAR hits are the two shapes that cannot be spelled any other
way (an appended disjunct must add `||` to the previous last term) plus
one real rename. The wave removed the two things that dominated the
surface — mechanical churn and package subsystems living inside upstream
files — so what remains is 1,243 lines of genuine hook: table rows,
guards, early-return diverts, and the D7-1/H2 shapes that are
non-minimal on purpose.

The single largest remaining item is not a defect but a bet:
`010-ui__tam.c.patch` now leads on *modified* upstream lines (29 in 28
hunks) and that is inherent to D7-1's semantics, ruled and catalogued. The
next-best action is nothing structural — it is to keep the churn scanner
at 0, which is now cheap because there is nothing to clean.

## 1. Churn — modified upstream lines carrying zero behavior

Scanner exit 0. WS-ONLY 0 (was 43), COMMENT-ONLY 0 (was 8).

P1 closed all 51 in three classes, each with the idiom SKILL.md names:

- **wrap-reindent** → the no-reindent wrap (`else {`, upstream body at
  upstream's own indentation, added `}`). Three sites: `determineItem`'s
  plane selection (19 lines), `pemAlpha`'s `addItemToBuffer` arm (13),
  `keyboardTweak.c`'s `fnExitAllMenus` guard (1).
- **comment-attach** → comment on its own added line above the untouched
  call. Eight sites: keyboard.c's three D7-1 `leaveTamModeIfEnabled()`
  calls (one keeping upstream's `//JM` tail), manage.c's five.
- **alignment/paren** → reverted exactly: `if((aimBuffer[0]) == 0)`,
  `cursorByte`'s indent, `menu_ALPHA`'s continuation row, and the
  `/* 022 */ → /* 023 */` table renumber (positional comments are fiction
  after a mid-table insert — the inserted row keeps `/* 022 */` and
  `-MNU_TAMFLAG` keeps upstream's number).

Two hits the spec had not named were the same class and are fixed the same
way: the `VERBOSE_DETERMINEITEM` guards at keyboard.c's two dispatch
sites, whose added `#if`/`#endif` sat two spaces left of the upstream pair
they replaced. Put back at upstream's bytes they became context lines, so
the patch shrank by four lines as well.

NEAR verdicts, all four judged NOT findings:

| hit | verdict |
|---|---|
| keyboard.c `... CM_LISTXY) {` → appended `\|\| (calcMode == CM_AIM && ...)` | sanctioned appended-disjunct idiom; the trailing `) {` must move |
| softmenus.c `menu(n) == -MNU_ALPHAintl;` → `... \|\|` | same |
| manage.c `tmpChar` → `tmpChar4` ×2 | real rename (SKILL.md's own example of a non-finding) |

Behavior-neutrality was verified as the skill prescribes: gate green plus
`git diff -w` over the five touched sources showing comment moves and one
reformat only — no statement added, removed or reordered.

## 2. Extraction candidates — closed

All three candidates the pre-wave review ranked are gone from `patches/`:

- **the console view** (P7) — 200 lines out of screen.c into
  `files/forth_console_view.c`. One coupling could not move:
  `_forthConsoleEditorTop()` reads `checkHPoffset`, a screen.c-local macro
  with no upstream header, so it stayed as the exported seam — the exact
  shape §2 of the prior review predicted. screen.c 286 → 73.
- **the fold/history/capture subsystem** (P8) — 1,269 lines out of
  manage.c into `files/programming/forth_fold.c`, on two 3-line seam
  wrappers (`forthPkgInsertInProgram`, `forthPkgCloseAlphaMenus`), the
  `paramCorePutLiteral` precedent. Verified byte-identical mechanically.
  manage.c 1,651 → 345.
- **the interactive EXIT ladder** (P9) — 155 lines out of `fnKeyExit`
  into `forthConsoleExitLadder()`. keyboard.c 486 → 328.

No new extraction candidate is open.

**Corrected 2026-08-09 (round 9, D7-6).** This paragraph said "the largest
inline block left is keyboard.c's `determineItem` console-roll arm (~40
lines including its comment)". The roll arm's own claim stands — it cannot
move, because running at that exact point in the plane selection is the
whole content of its rationale — but *largest* was wrong: the ITM_FORTH arm
in `insertStepInProgram` is 61 lines (`programming/manage.c:1673-1733`).
That block is also ruled to stay (the wave's own spec names the hook arms
under "Stays in manage.c (do NOT move)", and STAGE_L_T7 rejected splitting
`insertStepInProgram` outright rather than deferring it), so nothing moves
either way — but a census that is wrong in a report becomes a premise in
the next spec, which is why it is corrected here rather than left.

## 3. Documentation drift

- **Fixed this wave.** The prior review's §3 finding — `param_core.c`'s
  header claiming a "byte-identical extraction" of code that is a
  deliberate fork — is NOT fixed here; it remains open and is still
  catalogued as "fix the comment, not the fork". Carried.
- **Found and fixed en route (P7).** screen.c called
  `forthConsoleBaseOnTop()` with no declaration in scope — an implicit-int
  resolution of a `bool_t` function, warned on every compile of every
  target. `forth_menu.h` declares it and screen.c did not include it.
  Inert on both targets today; the class stops being inert the moment a
  return type changes.
- **Counts corrected in the spec's own move list (P8).** The spec had
  `_forthCapBuildStep`'s call sites the wrong way round (three move, two
  stay, and `insertStepInProgram` does not call it at all), and named
  `forthCapInsertName` as living in `forth_capture.h` when it is declared
  in `forth_menu.h`. Both recorded in the P8 and P2 commits.
- **P10's relocation** moved ten narrative blocks to DESIGN-HISTORY,
  leaving one-line pointers. Every pointer names the new entry
  (2026-08-09, P10) and resolves.

## 4. Deliberately non-minimal, correctly — do NOT "fix" these

Encountered this run, all catalogued in
`.claude/skills/upstream-diff-review/references/deliberate-exceptions.md`:

- the H2 wholesale deletion in `lblGtoXeq.c` (−237 upstream lines,
  clash-seeking on purpose);
- D7-1's 26 in-file `_tamLeave()` renames in `ui/tam.c` — now the largest
  *modified*-line count in the tree, and inherent;
- `forthUserItemDispatch` replacing the six PEM/execute pairs;
- `runFunction`'s XEQ arm rewritten around `forthResolveXEQ`;
- the mid-table `softmenu[]` insert at slot 022 and
  `NUMBER_OF_DYNAMIC_SOFTMENUS` 22 → 23;
- `FORTH_SELFTEST_EXPORT` on `executeFunction` / `_closeCatalog` /
  `determineItem` and the console statics;
- item table rows 213 / 2842 / 2843 filled in place;
- `param_core.c` as a diverged fork.

**New this wave (already catalogued, in the P6 commit):** the `closeAim()`
funnel — the L1-1 interactive-close guard moved inside the upstream
teardown instead of enumerated at four call sites. It costs override 19
against a budget of 16, deliberately: a package-side census of an upstream
call graph is the shape D7-1 rejected and round 8's OOF-1 caught in the
act. Citation: this spec's P6, owner approval 2026-08-09, DESIGN-HISTORY
2026-08-09 (the closeAim funnel).

Also new and NOT an exception, stated so the next reader does not file it
as one: P3 deletes four upstream lines at the `ITM_BACKSPACE` empty-buffer
arm (the sequence became `_forthCapAbortPemInput()`). That is the ordinary
H2 convention applied at a small scale, named in the spec, and needs no
catalog entry of its own.

## 5. Standing guard

Scanner count at this HEAD: **0** — command:

```
python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
    packages/forth-core/patches/*.patch
```

Exit status 0 (was 1 at `88703343f`). The churn gate is still NOT wired
beside `design-audit.sh`'s group I pins; with the count at 0 that is now a
one-line addition and the cheapest moment to make it, because any future
regression is a diff of one against a known-empty baseline. Recommended,
not done here — it is a change to the audit script, not to the wave.

`design-audit.sh` section A budgets were re-baselined at close-out:
override files 16 → 19 (P6's catalogued cost) and added lines 606 → 1,243
(a tightening of 1,823 against the pre-wave reality). Both stated in the
close-out commit.

Next run must state its own count against this one: **0 churn findings,
1,243 added lines, 340 modified/deleted upstream lines, 139 hunks, 19
override files, at `29257b7f5`.**
