# Implementation Report — Plain-Diff Package Overlay System (Revision 2 +
Automatic Classification / Flat Working Directory)

**Branch:** `package-manager/patch-based-overlay` (local commits only; not
pushed, not merged).
**Status:** revision 2 (plain-diff, single mechanism) fully implemented over
6 units, superseding revision 1 (function-boundary/libclang), which was
implemented and self-audited on this same branch and is preserved at
`checkpoint/pre-plain-diff-revert-20260712-1541` for reference/rollback.
A follow-on revision (§9 below) then replaced revision 2's manually
populated `patches/`/`files/` split with fully automatic classification
from a flat working directory, preserved pre-change at
`checkpoint/pre-flat-workdir-revision-20260712-1824`. Self-audit run after
each revision; no defects found in either round beyond what each round's
own commits already fixed.

> **Audit status: this implementation has NOT been independently audited by
> a separate model or session.** Everything below — including every
> self-audit round — was produced and verified by the same session that
> wrote the code. Treat it as self-reviewed only until an independent
> review happens.

---

## 1. Why this document is a revision, not an addendum

This branch's package-manager work happened in two passes:

- **Revision 1** (commits `8599271cf`..`cc3eac47e`, preserved on
  `checkpoint/pre-plain-diff-revert-20260712-1541`): libclang-based
  function-boundary patch granularity, with a separate whole-file-override
  mechanism for anything libclang couldn't attribute to a function body, and
  a mutual-exclusivity check keeping the two mechanisms apart.
- **Revision 2** (commits `b81055d28`..`a7f7c47eb`, this report): plain
  whole-file `git diff`, one mechanism, no restriction on what a developer
  edits. Revision 1's function-boundary machinery was judged to buy conflict
  precision the actual constraint (package size) didn't require, at a real
  cost — a build-adjacent libclang dependency, a second storage convention,
  a cross-mechanism exclusivity check, and authoring friction. See
  `custom_package/PROPOSED_SPEC_CHANGES.md`'s "Why revision 2" section for
  the full rationale and the explicitly accepted trade-off (two packages
  editing *different lines* of the same function still compose; *same-line*
  edits now conflict, where function-boundary splitting would have composed
  them — judged acceptable since package size, not conflict avoidance, is
  the actual goal).

This report describes revision 2's current, final state. It does not
re-narrate revision 1's implementation history in detail — that's on the
checkpoint branch and in that branch's own commit messages — but does
record what was removed and why, since "removed, not just superseded" is
itself a decision worth a reviewer's attention.

## 2. Commits

| Unit | Commit | Implements |
|------|--------------|------------|
| 1 | `b81055d28` | Spec revision: `custom_package/PROPOSED_SPEC_CHANGES.md` rewritten — SUPERSEDED section (dual-signal classification, libclang granularity, mechanism mutual-exclusivity) with why each is overturned; KEPT section (materialize/refresh, `.patch` format, cumulative ordering, `git apply -3` + marker scan, loud conflicts, whole-new-file storage) with both empirical findings (blob ancestry, `-U3` vs `-U10`) carried forward verbatim; New Decisions 1–7. Docs only. |
| 2 | `47ff9e11c` | Removal: deleted `tools/pkg_patch_extract.py` (libclang extractor) + its test/fixtures; deleted `tools/pkg_patch_refresh.py` (function-boundary version) + its test; removed `assert_mutually_exclusive` from `pkg_patch_apply.py`; deleted `test_pkg_patch_apply.py`/`test_pkg_patch_resolver.py` (built on removed machinery); stripped the `--patches` CLI and patch-application block from `resolve_c47_src.py`, returning it to whole-file-override-only pending Unit 4. Confirmed no dead imports via fresh-interpreter smoke tests. |
| 3 | `b32499a4e` | `tools/pkg_patch_refresh.py` rewritten: `refresh(pkgdir, project_root)` scans a whole package directory (not one file), whole-file `git diff --no-index --full-index` per changed file (any kind of change, no restriction), ordinal reuse (including manual renames), stale-patch deletion on revert, new-file detection/reporting. 16 tests. |
| 4 | `5c5af5f5d` | Resolver auto-discovery: `collect_patch_stacks`/`collect_new_files` (glob-based, no declarations) in `pkg_patch_apply.py`; `resolve_c47_src.py do_shadow()` rewritten to take a package-directory list and apply patches/copy new files with the same F9/F10/F11/F12/F15 guards; `meson.build` Phase 1 no longer calls `subdir(pkg)` at all (no per-package `-I`, no declaration reading); `resolver_safety_test.sh` updated for the new CLI, 9/9 still pass. 14 tests. |
| 5 | `fb602b307` | Makefile: verified (not re-implemented) `CUSTOM_PKG=` already threaded through all 10 named targets; new `check-custom-pkg-{sim,dmcp,dmcp5}` phony targets fixing a real, empirically-reproduced bug (switching `CUSTOM_PKG` against an existing build dir silently reused the stale shadow tree — Make's directory-existence-gated target semantics meant `meson setup` never re-ran); `pkg_build PKG=<dir>` (clean → test-gated → refresh → size-checked zip at `pkg_dist/`, not `dist/` — collided with an existing upstream `dist` script, caught by the first real run). `custom_package/README.md` rewritten for revision 2. |
| 6 | `a7f7c47eb` | Same-line two-package conflict verified through the real build path: automated test (`test_pkg_patch_resolver.py`, 2 new tests — conflict case + different-lines-compose contrast case) plus a real, since-deleted `meson setup` run against two scratch packages, confirming the exact same failure message the automated test asserts. |

All existing sentinel-gate delete-safety and symlink-escape containment
guards in `resolve_c47_src.py` (`assert_shadow_dir`, `assert_contained`, F9
wipe guard, F12 sentinel write, F15 dead-shadow warning) are **unchanged**
across every unit — confirmed both by code inspection and by
`tools/resolver_safety_test.sh` (9/9 PASS on the current CLI).

## 3. What was removed, and why (Unit 2 detail)

- **`tools/pkg_patch_extract.py`** (libclang function-boundary extractor)
  and its fixtures/tests — no longer needed; revision 2 does no C-source
  parsing at all, only whole-file `git diff`.
- **The classification logic in the old `refresh`** (per-function hunk
  splitting via synthetic single-function-replaced files, a totality check
  rejecting non-function-scoped changes) — revision 2's `refresh` has no
  such restriction; any kind of change is just diff output.
- **`pkg_override_sources`/`pkg_override_headers`/`pkg_custom_sources`/
  `pkg_patch_sources` meson.build declarations**, and the `subdir(pkg)`
  evaluation that read them — replaced by pure auto-discovery
  (`patches/`+`files/` globbing); a package directory is never `subdir()`'d
  into at all now.
- **The mutual-exclusivity check** between function-patch and whole-file-
  override mechanisms — nothing left for it to guard against with only one
  override mechanism. (A different, much narrower pair of existence checks
  remains for `patches/` vs. `files/` — see New Decision 6 — but it can't be
  triggered by two competing *mechanisms*, only by an individual patch/file
  entry targeting a path that should/shouldn't exist upstream.)

## 4. `[GAP]` items (out of scope, not touched)

- **testSuite** — clean at 6/9674→9674/9674 (0 failures) on every vanilla
  `make test` run in this session. The 6-failure signature seen briefly
  during Unit 4's real end-to-end testing traced to a **leftover
  `CUSTOM_PKG=packages/forth-core`-configured `build.sim`** from an earlier
  session, not a real regression — confirmed by re-running `make test`
  (which always starts from `clean`, i.e. genuinely vanilla) and observing
  0 failures. Not a `[GAP]` against this branch's work; recorded here only
  because it caused momentary confusion during this session and is worth
  a future reader knowing was a red herring, not a real finding.
- **forth-core is not migrated to the new convention.** `packages/forth-core/`
  is out of scope (Stage-2 Forth work, explicitly excluded by the task's
  scope lock) and still uses the pre-revision-1 whole-file-override
  convention (`pkg_override_sources` in its own `meson.build`, override
  files sitting directly at `packages/forth-core/<rel>`). Since the
  resolver no longer reads any package `meson.build` at all (auto-discovery
  only globs `patches/`+`files/`), **`CUSTOM_PKG=packages/forth-core` will
  configure successfully but silently contribute nothing** — forth-core's
  override files aren't under `patches/` or `files/`, so they're simply not
  discovered; the build proceeds as if forth-core were empty, with no
  error. This is a real, foreseeable consequence of the revert that this
  task's scope lock explicitly forbids me from fixing (touching
  `packages/forth-core/` content beyond `[GAP]` logging). **Flagged
  prominently for human follow-up**: forth-core needs either (a) migration
  to the `patches/`+`files/` convention (materialize each of its 11
  whole-file overrides + 6 custom sources, run `refresh`, move new files
  under `files/`), or (b) an explicit decision to keep it on a different,
  older resolver/branch until migrated. Until then, `./packages/forth-core/
  build-test.sh` will very likely "succeed" at building a binary but with
  none of forth-core's actual functionality compiled in — worth verifying
  directly before assuming otherwise, since I did not run it (out of
  scope, and running it would itself risk misleading a reader about
  forth-core's status if it happened to pass for unrelated reasons).
- **Package-level build configuration has no home.** Recorded as a "Known
  open item" in `PROPOSED_SPEC_CHANGES.md`: forth-core's
  `FORTH_DEBUG_SELFTEST` conditional `-D` flag is exactly this class of
  thing, and there is no mechanism for a `patches/`+`files/`-only package
  to express it.

## 5. `[DECISION NEEDED]` / judgment calls for human review

1. **`pkg_build`'s test-then-refresh ordering.** Per the task's explicit
   step list, `pkg_build` runs the test gate (`make test
   CUSTOM_PKG=$(PKG)`) **before** `refresh`, then zips whatever `refresh`
   produces. This means the artifact that ships is `refresh`'s *output*,
   which was never itself run through the test gate — only the
   *pre-refresh* on-disk `patches/`+`files/` state was tested. In the
   common case these are identical (a developer runs `refresh` before
   committing, so `patches/` is already up to date and `refresh` in
   `pkg_build` is a no-op — confirmed empirically: every `pkg_build` run in
   this session's testing printed `no changes ... already up to date`).
   But if a developer has stale materialized working copies lying around
   at `pkg_build` time, the shipped zip could differ from what was tested.
   Implemented exactly as specified rather than reordered, since reordering
   against explicit instructions is exactly the kind of unstated decision
   this task's process is designed to avoid — flagged here for a human to
   ratify or override.
2. **`PKG` variable name collision** (Makefile): `pkg_build`'s `PKG=<dir>`
   shares a name with the pre-existing numbered DMCP build-variant pattern
   targets (`dmcp_pkg1`/`2`/`3`, `build.dmcp.p$(PKG)`). Safe for
   `pkg_build`'s normal standalone invocation; unsafe only if combined with
   a numbered `dmcp_pkg*` goal in the same `make` command line (Make
   expands `$(PKG)` in target names at parse time). Documented in both the
   Makefile and the README rather than silently risked or renamed against
   the task's explicit `PKG=<dir>` naming.
3. **Case-insensitive-filesystem patch-target collisions** (carried over
   from the revision-1 self-audit, still unresolved because still
   untestable): a patch filename/header declaring a case-different variant
   of a real upstream path (e.g. `KeyBoard.c` vs. `keyboard.c`) is rejected
   on this Linux host because `os.path.isfile` is case-sensitive here — but
   on a case-insensitive filesystem (macOS default, Windows), the same
   check could resolve the case-different path to the real file and accept
   it, writing the shadow-tree entry under the wrong-cased name. Flagged,
   not fixed — no case-insensitive test environment available in this
   session.
4. **forth-core migration** — see `[GAP]` above; a decision, not just a gap,
   since it determines whether `packages/forth-core/` needs active
   migration work before this branch is usable for its original purpose.

## 6. Self-audit (revision 2)

Adversarial pass against the current implementation, five checks per the
task's list, each empirically re-verified (not just re-asserted) with a
construction different from Unit 5's own verification runs where a prior
run existed:

1. **libclang/clang.cindex remnants** — exhaustive grep across
   `custom_package/` and `tools/` found only documentation comments and
   regression-guard tests asserting the *absence* of clang (e.g.
   `TestNoLibclangDependency`), never an actual import. A fresh-interpreter
   import of every `tools/pkg_patch_*.py` module + `resolve_c47_src.py`
   confirmed zero `clang`/`clang.*` modules loaded. **No defect.**
2. **`pkg_build` producing a zip despite a failing test** — tried a
   *different* failure mode than Unit 5's own verification (which used a
   compile-breaking syntax error): a patch targeting a **nonexistent
   upstream path**, which fails at the `meson setup` **configure** step
   inside `make test`, not at `ninja`. `pkg_build` stopped there; no
   `pkg_dist/` directory was even created. **No defect.**
3. **Oversized package bypassing the size check** — tried a *different*
   construction than Unit 5's own verification (which used one file
   shrunk via `PKG_MAX_SIZE`): 20 separate small `.c` files (84KB raw)
   whose zip compresses to 6534 bytes — confirming the check is genuinely
   against the real assembled zip's `stat`-reported size (an aggregate of
   many files, not any single file or an estimate), correctly rejected at
   `PKG_MAX_SIZE=5000` and correctly passed at `PKG_MAX_SIZE=50000`.
   **No defect.**
4. **`CUSTOM_PKG` switch without an intervening `make clean`** — tried a
   *different* scenario than Unit 5's own verification (which switched
   real-package↔empty): switched directly between **two different
   real, non-empty packages** (`packages/_audit4_a` → `packages/_audit4_b`)
   via successive `make sim` calls with no `clean` between them. Confirmed
   via `grep` on the shadow tree: package A's marker text was fully absent
   and package B's fully present after the switch — the reconfigure-on-
   change stamp mechanism genuinely replaced the shadow tree, not silently
   reused it. **No defect.**
5. **Subtly wrong patch target path** — three variants probed directly
   against `collect_patch_stacks`: a trailing slash in the `+++` header
   target (caught by the dual-signal filename/header mismatch check); a
   case-different target name relative to the real upstream file (caught
   by the upstream-existence check — see item 3 in §5 above for the
   case-insensitive-filesystem caveat on this one specifically); an extra
   path segment (`src/keyboard.c` instead of `keyboard.c`, caught by the
   upstream-existence check). **No defect found on this platform** (Linux,
   case-sensitive filesystem).

No fixes were required this round — the hardening from revision 1's own
self-audit (path-traversal/absolute-path rejection, tab-timestamp header
parsing, multi-file-patch rejection, all in `pkg_patch_common.py`) carried
forward unchanged into revision 2, since that module was reused as-is
rather than rewritten.

## 7. Mutation-test coverage (revision 2, current suites)

Each test's docstring names the bug it exists to catch; summarized by
suite (all wired as meson tests; all pass):

**`test_pkg_patch_common.py` (16, unchanged from revision 1)** — filename
codec roundtrip/rejections; traversal/absolute-path rejection (5 hostile
filenames); `+++` target parse (nested path, `src/c47/` prefix strip,
tab-timestamp header, no-header, multi-file-patch rejection); dual-signal
mismatch/missing-upstream rejection; fresh-interpreter no-libclang
assertion (updated wording for revision 2, same guarantee).

**`test_pkg_patch_refresh.py` (16, revision 2)** — the three cases the
task explicitly specified (changed file produces an applying patch;
unchanged file produces nothing; reverted edit deletes the stale patch);
directory-scanning correctness (mixed changed/unchanged, nested paths,
`patches/`/`files/`/stray-`meson.build` excluded from the scan); new-file
reporting (no upstream counterpart → reported, not patched, no error);
ordinal reuse across re-refresh and preservation of a manual rename;
patch validity (independent multi-file diffs don't interfere; git headers
+ dual-signal present; **no restriction on kind of change**, proving New
Decision 1 against the removed function-boundary restriction); binary-file
rejection; uncommitted-upstream rejection; a real-repo run against an
actual `src/c47/*.c` file via a self-cleaning scratch package.

**`test_pkg_patch_resolver.py` (16, revision 2)** — auto-discovery unit
tests (every on-disk patch used automatically, no declaration to drop;
nonexistent-upstream-target fatal — Unit 4's specified case; legal
new-file accepted; `files/` mirroring an existing upstream path fatal;
two packages claiming the same new file fatal); through the real resolver
CLI: two packages' non-overlapping patches on the same file both apply
(Unit 4's specified multi-package case); nonexistent-target patch fails
configure end-to-end; new `.c` copied AND compiled, new `.h` copied but
NOT added to the source list; real upstream file never modified;
patches+files coexist on different targets; sentinel present, untouched
sibling stays a symlink; self-cancelling stack warns "dead shadow";
import-audited resolver run aborts on any `clang*` import; **same-line
two-package conflict fails loudly naming the losing patch, no
marker-bearing file survives (Unit 6)**; **different-lines-same-function
still composes (Unit 6 contrast case, the explicit revision-2 trade-off)**.

Total: 48 automated tests across 3 suites, plus `tools/resolver_safety_test.sh`
(9 scenarios: symlink-mode success, decoy-shadow-dir refusal,
path-traversal-via-hostile-filename rejection, copy-mode success, each with
a canary-survival check) run manually (not meson-wired, matching its
pre-existing convention as a standalone regression script).

## 8. End-to-end evidence (real meson/make, scratch artifacts deleted after)

- Real `meson setup`+`ninja` with a scratch package patching `keyboard.c`
  and adding a `files/scratch_new.c`: both applied correctly, the patched
  object compiled, and the new file appeared in `compile_commands.json` and
  compiled too (Unit 4).
- Real `meson setup` with two scratch packages patching the same line of
  `keyboard.c` divergently: configure failed with `010-keyboard.c.patch
  applied against keyboard.c left conflict markers at line(s) [15, 17, 19]`
  naming the losing patch's full path (Unit 6).
- Real `make pkg_build` runs (Unit 5 + self-audit): clean patch → 4/4
  `make test` pass → 736-byte zip containing only `patches/`; oversized
  zip correctly rejected and deleted at both a tight literal limit and via
  20-small-files aggregation; compile-breaking patch and configure-fatal
  patch both stopped `pkg_build` before any `pkg_dist/` output existed;
  missing/nonexistent `PKG=` both rejected with a named error.
- Real `make sim` `CUSTOM_PKG` switching (Unit 5 + self-audit): populated
  build dir with no stamp forces one reconfigure (treating pre-existing
  state as unknown, not assumed-empty); identical repeated value is a
  silent no-op; switching to a real package, back to empty, and directly
  between two different real packages all force correct reconfigures,
  verified via shadow-tree content grep each time (not just exit codes).
- Vanilla `make test` (no `CUSTOM_PKG`): run after every unit in this
  session, always 0 testSuite failures out of the full suite, confirming
  the byte-for-byte-vanilla-build invariant held throughout.
- `tools/resolver_safety_test.sh`: 9/9 PASS against the current resolver
  CLI — F9 sentinel-gate delete-safety and F10/F11 containment guards
  fully intact under the new patches/+files/ discovery mechanism.

## 9. Automatic Classification, Flat Working Directory

**Commit:** `d3e593216`. **Checkpoint before this revision:**
`checkpoint/pre-flat-workdir-revision-20260712-1824`.

Revises revision 2's authoring workflow: previously a developer manually
placed pre-authored `.patch` files into `patches/` and whole new files into
`files/` themselves, deciding which mechanism applied. This revision
removes that decision entirely. A package's working area is now flat,
mirroring upstream paths directly (the same shape as the original,
pre-revision-1 whole-file-override convention); `patches/` and `files/`
become generated build output only, written entirely by `refresh`, never
created or edited by hand.

**What changed:**

- `refresh` (`tools/pkg_patch_refresh.py`) now scans a flat working area
  and classifies each file automatically: exists upstream → diff into
  `patches/`; doesn't exist upstream → copy whole into `files/`
  automatically (previously: left alone and reported, requiring the
  developer to place it under `files/` themselves — this "report only"
  half of revision 2's New Decision 2 is marked `[SUPERSEDED]` in
  `PROPOSED_SPEC_CHANGES.md`, the diff/stale-cleanup half is unchanged).
- Stale-cleanup generalized from "reverted edit" to "not producible from
  the current working area at all," which also covers a working-area file
  being **deleted** outright between `refresh` runs — this case did not
  exist before (revision 2's `refresh` only ever compared a *present*
  working file against upstream; a deleted file was simply never visited).
- New: a per-package manifest (`<pkgdir>/.refresh-manifest.json`, sha256
  of every entry as `refresh` wrote it, **committed**) enables drift
  detection — an existing `patches/`/`files/` entry that doesn't match what
  `refresh` itself last recorded writing there is **warned about** (not
  failed) and then overwritten with correct, freshly generated content.
  The warn-not-fail choice (the task left this as a judgment call) is
  because the normal edit-then-refresh cycle and a genuine hand-tamper
  both end at the identical overwrite operation — failing here would
  punish the ordinary case for sharing code with the suspicious one; the
  goal is visibility into drift, not blocking a self-healing artifact.
- New `packages/.gitignore` (scoped to `packages/`, does not touch the
  top-level upstream `.gitignore`) excludes flat working-area files from
  version control while keeping `patches/`, `files/`, and the manifest
  tracked. The manifest is deliberately **not** gitignored — without it
  committed, a fresh clone's first `refresh` would have no history of
  prior writes and would flag every legitimately committed entry as
  unrecorded drift.
- The build-time resolver (`resolve_c47_src.py`, `collect_patch_stacks`/
  `collect_new_files` in `pkg_patch_apply.py`) is **completely unchanged**
  — it never read anything but `patches/*.patch` and `files/*`, and still
  doesn't. Confirmed via `git diff` showing zero changes to those files in
  this commit, and via the full `test_pkg_patch_resolver.py` suite (16
  tests, all against the resolver's real CLI) passing unmodified against
  the new `refresh`.

**Test coverage added:** `tools/test_pkg_patch_refresh.py` grew from 16 to
34 tests. New coverage: automatic classification (new file → `files/`
automatically, the core behavior change); stale cleanup on working-file
deletion for both `patches/` and `files/`; the manifest file's own
exclusion from the working-area scan (would otherwise recursively
classify itself as a new file); five manifest/drift tests (clean-cycle
no-false-positives, hand-edited patch, hand-added-with-no-record patch,
hand-edited `files/` entry, non-fatality, cross-invocation persistence).
Full list and the bug each catches is in the commit message for
`d3e593216`.

**Self-audit (4 items specified by the task), each verified empirically
against a real scratch package in this repository, not just the unit test
suite:**

1. Grepped `tools/`, `custom_package/*.md`, and the `Makefile` for any
   remaining "developer manually places/creates `patches/`/`files/`"
   language — the only hit was `PROPOSED_SPEC_CHANGES.md`'s own
   `[SUPERSEDED]` note, correctly describing the retired behavior. **No
   defect.**
2. Real refresh cycle: materialized a `keyboard.c` edit and a new
   `scratch_helper.c`, refreshed (both classified correctly), then
   **deleted both working-area files** and refreshed again — both the
   patch and the `files/` entry were removed (`no longer producible from
   the working area`), directories left genuinely empty, not merely
   ignored. **No defect.**
3. Real hand-edit: appended a tamper line directly to a generated patch
   file (working copy left untouched), refreshed again — printed a
   warning naming the exact file (`content does not match what refresh
   last wrote for it (hand-edited directly, bypassing the working
   area?)`), then overwrote it with correct content (confirmed via
   `grep`, tamper text gone). **No defect — the guard fires and
   self-heals as designed.**
4. Real `git add -A` on a scratch package: staged only `patches/` and the
   manifest, never the flat working-area files; confirmed
   `packages/forth-core/`'s pre-existing tracked files continue to show
   modifications normally (ignore rules never affect already-tracked
   files, so this revision cannot accidentally hide changes to
   forth-core's still-unmigrated whole-file overrides). **No defect.**

No fixes were required this round.

---

Do not push. Do not merge. Everything above is committed locally on
`package-manager/patch-based-overlay` for human review; prior states
remain reachable at `checkpoint/pre-plain-diff-revert-20260712-1541`
(pre-revision-2) and `checkpoint/pre-flat-workdir-revision-20260712-1824`
(pre-§9, i.e. revision 2 as its own complete, self-audited state).
