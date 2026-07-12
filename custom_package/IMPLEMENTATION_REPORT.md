# Implementation Report — Patch-Based Package Overlay System

**Branch:** `package-manager/patch-based-overlay` (local commits only; not
pushed, not merged).
**Status:** all 8 units implemented, committed, and gated; self-audit run
with 3 findings fixed.

> **Audit status: this implementation has NOT been independently audited by
> a separate model or session. Everything below — including the self-audit —
> was produced and verified by the same session that wrote the code. Treat
> it as self-reviewed only until an independent review happens.**

---

## 1. Commits

| Unit | Commit | Implements |
|------|--------------|------------|
| 1 | `8599271cf` | Storage convention + documentation: `packages/<pkg>/patches/<NNN>-<rel_encoded>.patch`, dual-signal target declaration, ordering/composition rules, materialize/refresh workflow, patch-vs-whole-file-vs-new-file table in `custom_package/README.md`; commits the approved spec file. Docs only. |
| 2 | `c780d086f` | `tools/pkg_patch_extract.py` — libclang function-boundary extractor (authoring-only) + `tools/pkg_patch_common.py` — libclang-free shared convention module (filename codec, header parse, §2 dual-signal validation). Fixtures: braces-in-string, `#ifdef`-guarded definition, macro-expanded braces, header-identity, real-file check (`fnPExport` 162–271). |
| 3 | `c1ee04eb2` | `tools/pkg_patch_refresh.py` — detection (body string compare), per-function patch generation via synthetic single-function-replaced files, byte-exact totality check (loud §8 rejection of any non-function-scoped change), real CLI with ordinal reuse + stale-patch removal; meson test wiring. Replaced the prior draft whose hunk-attribution misclassified context-overlapping hunks (its own tests failed). |
| 4 | `002e4312d` | `tools/pkg_patch_apply.py` — `git apply -3` against a freshly materialized upstream copy in a scratch git repo (real tree never touched), pre-image blob seeding, **unconditional conflict-marker scan after every patch** (§5 ratified); empirical §5/§1 findings written into the spec. |
| 5 | `09580ef46` | `collect_patch_stacks()` — §2 validation of every declared patch, per-rel stacks sorted by `(integer ordinal, CUSTOM_PKG list index)`; loud rejection of malformed names and declared/on-disk mismatches in both directions. |
| 6 | `97135dab7` | End-to-end two-package same-function conflict through the real pipeline (refresh-generated patches → collect → apply): loud failure naming the losing patch; contrast test proving different-function edits still compose. Fixtures self-constructed/torn-down; nothing synthetic committed. |
| 7 | `64c606245` | §8 mutual exclusivity as a **fatal configure error**, checked **before** the F9 wipe; `pkg_patch_sources` meson variable + `--patches` resolver CLI; rel-key normalization so path formatting can't dodge the check; import-audited resolver run (decision 4 hard enforcement). Verified at the real meson level. |
| 8 | `8cb660a90` | Shadow-tree integration: patch stacks materialized as regular files in `custom_pkg_shadow/` with F10/F11 containment mirrored, symlink removed before write (+ defense-in-depth symlink-write refusal in apply), F15 dead-shadow warning extended to self-cancelling stacks. Real meson e2e: configure + compile of a patched `keyboard.c`; meson-level conflict failure; vanilla build unaffected. |
| audit | `cc3eac47e` | Self-audit fixes (see §5). |

All existing sentinel-gate delete-safety and symlink-escape containment
guards in `resolve_c47_src.py` are **unchanged** (`assert_shadow_dir`,
`assert_contained`, F9 wipe guard, F12 sentinel write). No unit required
touching them, so no stop-and-skip was triggered.

## 2. Empirical answers (formerly `[VERIFIED: pending]`, now in `PROPOSED_SPEC_CHANGES.md`)

**§5 blob ancestry — HOLDS, and every failure mode observed is loud.**
Real-drift experiment (patch authored against this repo's committed
`keyboard.c`, applied against the upstream clone's genuinely drifted HEAD
version, 7 churned hunks): with the pre-image blob resolvable and seeded
into the scratch apply repo, far-from-churn edits apply cleanly and
overlapping-churn edits three-way-merge into conflict markers that the
unconditional scan catches. Without ancestry, git prints `repository lacks
the necessary blob to perform 3-way merge. Falling back to direct
application...` and either direct-applies (clean context) or fails outright
— never a silent mis-merge. Safeguards that make this hold: `refresh`
writes full 40-char pre-image SHAs and hard-fails at generation if the blob
isn't resolvable (upstream must be committed); `apply_patch_stack` seeds
each patch's pre-image blob into its scratch odb (resolving abbreviated
index lines first). Residual caveat: a shallow clone lacking historical
blobs degrades to the loud no-ancestry behavior. Adjacent-line drift (no
unchanged separating line) conflicts loudly even via `-3`.

**§1 context window — keep `-U3`; the spec's assumption was reversed by
measurement.** `git apply` has no fuzz, so a *larger* window is strictly
worse for direct application under drift: real churn 6 lines from the edit
broke `-U10` but not `-U3`. Window size is irrelevant once `-3` engages
(the merge uses the recorded blob, not the context). Resilience comes from
ancestry, not width.

## 3. `[GAP]` items (out of scope, not touched)

- **testSuite pre-existing failure** — 6/9674 program tests fail (SPIRAL,
  `programs.txt` line 35: register Z complex34 instead of real34), with
  `freeList.c` double-free diagnostics from the forth-core override
  (`freeListFree` at `core/freeList.c:217/244/268`). Present at baseline
  before any change in this session; signature identical (6/9674, same
  test, same stderr) after every unit. Forth-core / `freeList.c` is
  explicitly out of scope for this branch.

## 4. `[DECISION NEEDED]` / judgment calls for human review

None of the existing safety guards needed modification, so no unit was
stopped/skipped. Items a reviewer should ratify:

1. **Tooling location:** new modules live in `tools/` beside
   `resolve_c47_src.py` (`pkg_patch_common/extract/refresh/apply` + tests),
   continuing the prior session's layout, rather than under
   `custom_package/` as the run instructions literally said. They are
   successor modules to the resolver; keeping them beside it kept the
   import graph and meson wiring simple.
2. **Conflict boundary:** §7 is enforced as *overlapping edits* (git merge
   semantics). Two packages editing the SAME function on lines separated by
   at least one unchanged line compose cleanly. The spec's wording ("same
   function, overlapping edits") supports this, but it's worth a conscious
   ratification since function-granularity governs authoring, while
   conflict detection is line-granular.
3. **Prior-session working-tree edits left uncommitted** (out of my scope
   lock): `packages/forth-core/PROPOSED_SPEC_CHANGES.md` (content moved to
   `custom_package/`), `packages/forth-core/PEM_FIX_COMMITS.md` (+2-line
   SUPERSEDED note). Also untracked and left alone:
   `custom_package/QWEN_IMPLEMENTATION_PROMPTS.md`, the read-only
   `upstream/` clone (used by the Unit 4 experiment), `tools/__pycache__/`.
4. **Superseded draft removed:** the prior session's parallel
   implementation `custom_package/tools/detect_changed_functions.py` (+
   test + fixtures, all untracked) was deleted; its detection tests were
   ported into `tools/test_pkg_patch_refresh.py`. Its patch generator was
   incorrect (rewrote hunk headers to whole-function ranges while emitting
   only `-U3` windows — invalid patches for any function taller than the
   window).
5. **Case-insensitive filesystems:** §8 rel keys are `normcase`d, but this
   Linux host cannot test real case-insensitive collision behavior
   (macOS/Windows checkouts).

## 5. Self-audit findings (fixed in `cc3eac47e`)

Adversarial pass attacking the loud-failure guarantees:

1. **Path traversal / absolute-path decode (fixed):**
   `010-..__evil.c.patch` decoded to `../evil.c`, and
   `010-__etc__passwd.patch` to `/etc/passwd` — `os.path.join` *discards*
   the `src/c47` prefix for absolute paths, so validation probed the raw
   filesystem path. The resolver's containment guards did catch these
   downstream, but the codec now rejects `..`/`.`/empty segments and
   absolute rels outright.
2. **Tab+timestamp diff headers (fixed):** a standard
   `+++ b/test.c\t2026-...` header parsed the timestamp into the target,
   wrongly rejecting a valid patch (loud-but-wrong). Tab suffix now
   stripped.
3. **Multi-file patches (fixed):** a patch with two diff sections was
   validated against its first `+++` header only; a second section
   creating a new file would apply in the scratch repo and be silently
   discarded — a real silent-drop hole. Exactly one `+++` header is now
   required.

Audits that found no defect: libclang import-graph grep + fresh-interpreter
import assertions + import-audited full resolver run (decision 4);
`rc==0`-with-markers caught by the unconditional scan (mutation-verified);
alternate same-function conflict construction (one package, two ordinals,
divergent same-line edits) fails loudly naming the second patch; §8 dodge
attempts via `./test.c` and `src/../test.c` both caught through the real
resolver CLI.

## 6. Mutation-test coverage

Each test's docstring names the bug it exists to catch. Summary by suite
(all wired as meson tests; all pass):

**`test_pkg_patch_common.py` (16)** — filename codec roundtrip/rejections
(missing prefix, missing suffix, empty rel); traversal/absolute rejection
(5 hostile filenames; mutation: drop the segment check); `+++` target
parse (nested, `src/c47/` prefix strip, tab-timestamp, no-header,
multi-file rejection); §2 dual-signal: filename/header mismatch raises
naming both values (mutation: remove the cross-check), agreed-but-missing
upstream raises (mutation: remove the existence check — silent "new file"
reclassification); fresh-interpreter proof that importing the shared
module pulls in no `clang*`/extractor module.

**`test_pkg_patch_extract.py` (10)** — braces in string/char literals
(naive counter ends the function early); `#ifdef`-guarded definitions
(parsing without the real `-D` flags reports the inactive branch);
in-body `#ifdef` must not truncate the extent; macro-expanded braces (no
literal brace on the line at all); included-header function leakage
(mutation: drop the cursor-file identity check); real upstream file
(`fnPExport`); missing `compile_commands.json`/entry raise clearly (no
flag-guessing fallback).

**`test_pkg_patch_refresh.py` (15)** — detection over/under-reporting;
upstream-order determinism; added function surfaces as structural;
global-only and global-beside-function-change both rejected with nothing
written (mutation: drop the totality check — the global edit silently
vanishes from the build); single patch applies via `git apply --check`
and reproduces the edit byte-for-byte; dual-signal + git headers present;
removed lines come only from the edited function; two changed functions →
two ordinals in upstream order (the prior draft overwrote one patch with
the other); ordinal reuse on re-refresh; stale patch removal on revert;
pre-image blob resolvable (`cat-file -t == blob`); refresh refuses
uncommitted upstream (mutation: drop the `cat-file -e` gate); real-repo
run against `src/c47/mathematics/min.c` via a self-removed scratch
package.

**`test_pkg_patch_apply.py` (19)** — fresh-interpreter no-libclang
assertion; marker scanner catches all three marker kinds, no false
positives on `<<=`/`>=`/dash-rule comments; single/stacked application
reproduces the authored file, cumulative not last-wins, upstream file
untouched; outright failure names the patch and writes no output;
same-line conflict raises; **`rc==0`-with-markers caught only by the
unconditional scan** (mutation: make the scan conditional on exit
status); drift trio — separated-context drift merges only via the seeded
pre-image blob (mutation: drop `_seed_blob`, verified failing with git's
"lacks the necessary blob"), same-line drift and adjacent-line drift both
conflict loudly; ordering — order-dependent chain applies 010→020→030
under shuffled declarations, swapped ordinals fail with no output,
missing prefix rejected, declared-but-missing and on-disk-but-undeclared
both rejected, ordinal tie follows package list order and reverses with
it; Unit 6 pair — same-function divergence fails naming the loser,
different-function edits compose.

**`test_pkg_patch_resolver.py` (13)** — §8 unit checks (disjoint passes,
same rel raises naming both packages, formatting variants caught);
through the real resolver CLI: override+patch on one file exits 1 with
the §8 message, the check precedes the F9 wipe (sentinel'd canary
survives a failing run; mutation: move the check after the wipe),
single-mechanism configs pass, §2 mismatch fatal end-to-end; shadow
integration: patched rel is a regular file with hand-verified content,
siblings stay symlinks, sentinel intact, real upstream byte-identical
after the run (the write-through-symlink corruption bug),
override+patch coexist on different files, self-cancelling stack warns
"dead shadow", two-package conflict exits 1 leaving no marker-bearing
file; import-audited resolver run aborts on any `clang*` import.

## 7. End-to-end evidence (real meson, scratch artifacts deleted after)

- `meson setup … -DCUSTOM_PKG=packages/_scratch_e2e` → configure OK,
  `CUSTOM_PKG patched "keyboard.c": 1 patch(es) applied`, shadow holds the
  patched regular file, `ninja` compiled the patched `keyboard.c` object.
- Two scratch packages patching the same `keyboard.c` line divergently →
  `meson setup` **fails** configure: `010-keyboard.c.patch applied against
  keyboard.c left conflict markers at line(s) [15, 17, 19]`.
- Whole-file override + patch on `keyboard.c` → `meson setup` **fails**
  configure with the §8 mutual-exclusivity error naming both packages.
- Declared-but-missing patch file → `meson setup` fails configure loudly.
- Vanilla configure (no `CUSTOM_PKG`): no shadow tree created; all 5
  pkg_patch suites pass in the vanilla build dir; every meson change is
  inside `custom_pkg_list != []` blocks plus unconditional `test()`
  entries (no product-code effect).
- Full `meson test -C build.sim`: 5/5 pkg_patch suites OK; forth-core
  (whole-file overrides) reconfigured and rebuilt cleanly under the new
  resolver; testSuite unchanged at its pre-existing baseline (§3 `[GAP]`).
