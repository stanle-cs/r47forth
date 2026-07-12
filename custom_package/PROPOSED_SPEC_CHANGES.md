# Proposed Spec Changes — Patch-Based Package Overlay System

**Status:** proposal, not ratified. No `custom_package/DESIGN.md` exists today;
this document is written so it could later seed one, but formalizing it is a
human decision. `custom_package/README.md` is treated as the sole existing
authority on current behavior for this pass — `packages/forth-core/DESIGN.md`
governs forth-core's own Forth logic only and is not cited here.

**Scope:** package overlay/manager machinery only — `tools/resolve_c47_src.py`
(or its replacement), `custom_package/README.md`, and new patch
generation/application tooling. Does not touch forth-core's Stage-2 Forth
work, `freeList.c`, `labelList`/`programList`, or any other audit-backlog
item.

**Goal of the redesign:** move from whole-file symlink overrides
(last-listed-wins) to patch-based, function-boundary-granular overrides with
cumulative, explicitly ordered per-file composition.

No `[GAP]`s outside scope were surfaced during this pass.

---

## 1. Storage Format

**Decision:** package overrides are stored as unified diff patches (`.patch`
files, `git diff -U3` or greater context) against the upstream file they
target, rather than whole-file copies. Files with no upstream counterpart at
the same relative path continue to be stored whole, unchanged from today's
convention.

**Rationale:** a patch makes the *delta* — the actual thing a package is
responsible for — reviewable and diffable against upstream drift, instead of
requiring a human to diff two full files to find out what a package actually
changed.

**Today, for contrast:** overrides are committed as complete replacement
files. `packages/forth-core/meson.build:2` lists eleven whole-file overrides
(`config.c`, `error.c`, `items.c`, `screen.c`, …) via `pkg_override_sources`,
each a full copy of the upstream file with edits inline
[VERIFIED: packages/forth-core/meson.build:2]. "The override replaces the
upstream file entirely in the shadow tree" is the explicit current contract
[VERIFIED: custom_package/README.md:60].

**Open question for implementation:** minimum/maximum context-line policy for
generated patches — `-U3` is GNU-diff default and is assumed here, but a
larger context window trades larger patch files for more resilient matching
against minor upstream drift. Qwen should confirm this empirically against
real upstream churn (see item 5) rather than it being asserted here.

---

## 2. New-vs-Overlay Distinction

**Decision:** determined by path-mirroring — a patch file's relative path
must exactly match the upstream file it targets. No separate marker or
directive is needed to distinguish "this overrides an upstream file" from
"this is a new file."

**Rationale:** removes a class of declaration (today's separate
`pkg_override_sources` vs. `pkg_custom_sources` lists) in favor of one
inference rule, reducing the surface for the exact kind of silent
misconfiguration the current README already warns about (a typo'd variable
name silently producing no override, no error, green build
[VERIFIED: custom_package/README.md:50-53]).

**[RATIFIED]** — today, an override whose relative path doesn't match any
upstream source is a **fatal configure error** — `resolve_c47_src.py` exits 1
with "does not exist in src/c47/" [VERIFIED: tools/resolve_c47_src.py:196-199],
and the shadow-mode path emits the equivalent fatal error at
[VERIFIED: tools/resolve_c47_src.py:190-199] as well as a distinct "does not
match any upstream source — ignored" warning path for the non-shadow mode
[VERIFIED: tools/resolve_c47_src.py:326-327]. Under pure path-mirroring
inference, a misspelled path that was meant to override an existing file
would instead be silently classified as "new file, store whole" — turning
today's loud fatal error into silent misclassification.

**Decision: path-mirroring is not the only signal.** Every patch file must
carry an explicit override-target declaration in addition to its path — e.g.
a required one-line header (comment) in the patch naming the upstream file it
targets, or an equivalent required manifest entry — and the resolver must
verify the declared target matches the path-inferred target, failing loudly
on any mismatch. Rationale (human reviewer): the entire point of the
redesign is loud failure; silently reclassifying a typo'd override path as
"must be a new file" undermines that on day one, and the authoring overhead
of one required line is cheap relative to that risk. Left to Step 2 to pick
the exact declaration mechanism (header comment vs. manifest entry).

---

## 3. Composition

**Decision:** multiple packages targeting the same upstream file apply as a
cumulative, explicitly ordered patch stack, not last-listed-wins. Ordering is
declared explicitly per file via a numeric prefix convention (e.g. `001-`,
`002-`), following the OpenWrt/`quilt`-series model.

**Rationale:** today, when two packages override the same file, only the
last-listed package's version survives — the other package's contribution is
silently dropped from the build (a warning is printed, but the build still
succeeds and ships without it). Cumulative composition preserves every
package's independent contribution instead of requiring one package to
subsume the other's diff by hand.

**Today, for contrast:** `chosen = spec_list[-1]  # last wins`
[VERIFIED: tools/resolve_c47_src.py:184], with the equivalent non-shadow-mode
logic at [VERIFIED: tools/resolve_c47_src.py:308-310] (`override_map[s].pop()`
consuming overrides in list order, last one taking effect). The current
"duplicated (N packages override this file, last wins)" message is a
**warning only** — it does not fail the build
[VERIFIED: tools/resolve_c47_src.py:225-228,242-245]. This proposal changes
that: see item 7, which upgrades unresolved same-function conflicts (not mere
same-file duplication) to a hard failure.

**Open question for implementation:** exact sort key when two patches for the
same file share a numeric prefix (collision), and whether prefixes must be
globally unique per file or merely define a total order (ties broken by,
e.g., package directory name). `quilt` itself uses an explicit `series` file
rather than filename-embedded prefixes alone — Qwen should evaluate whether a
`series`-file-per-target or filename-prefix-only convention better fits this
project's existing "everything declared in `meson.build`" style
[VERIFIED: custom_package/README.md:4-6].

---

## 4. Patch Granularity

**Decision:** patches are generated and applied at function-boundary
granularity (one logical diff per overridden function), using libclang
(`clang.cindex`) against `compile_commands.json` to locate real function
boundaries — not brace-matching or raw line diffing.

**Rationale:** avoids spurious conflicts between packages editing unrelated
functions in the same file, and correctly handles macro-expanded braces,
`#ifdef`-guarded bodies, and string/char-literal braces that break
token/brace-based scanning.

**[VERIFIED: pending, checked in this pass]** `clang.cindex` is **not
currently installed** in this environment: `python3 -c "import clang.cindex"`
raises `ModuleNotFoundError: No module named 'clang'` (Python 3.12.3, checked
during this design pass). This is a new dependency the implementation must
add — to the dev environment and to any CI/build gate that invokes the
extractor — not something already available and merely unused.

**[RATIFIED]** — "A vanilla build (`-DCUSTOM_PKG` unset or empty) is
byte-for-byte identical [to upstream]" is a hard, explicit invariant today
[VERIFIED: custom_package/README.md:5-7]. The libclang dependency must be
scoped to package-authoring tooling (the `refresh` command, item 6) — invoked
by a developer when generating/updating a patch — and must **not** be a
dependency of the shadow-tree build step itself (`resolve_c47_src.py --shadow`,
invoked by every `meson`/`ninja` build with `CUSTOM_PKG` set
[VERIFIED: meson.build:133-138]). The build step must only ever consume
already-generated, checked-in `.patch` files via `git apply -3` (item 5) — it
must not re-parse C source with libclang on every build.

**Decision: hard-enforce, not document-only.** This must be a CI/configure-time
assertion, not a comment or README note that can silently rot. Concretely:
the configure-time build step (whatever runs on every `meson`/`ninja`
invocation) must fail if it can reach or import the libclang extractor
module — e.g. an explicit check that the shadow-tree resolver process has no
`clang`/`clang.cindex` import in its call graph, or a CI job that greps the
build-time code path for forbidden imports. Left to Step 2 (item 2 of the
Step 2 breakdown, function-boundary extractor) to specify the exact
assertion mechanism.

---

## 5. Application Mechanism

**Decision:** patches are applied via `git apply -3` against a freshly
materialized copy of the current upstream file at build/prepare time.

**[VERIFIED: pending]** Whether patch generation preserves real blob
ancestry — i.e., whether the generated patch's `index` line carries a blob
SHA that is actually resolvable as a git object in the repository where
`git apply -3` runs — is **not verified in this pass** and cannot be, without
the generator/applier code existing to test against. This matters because
upstream is consumed from a separate clone (`/home/stan/c43`, tracking
`https://gitlab.com/rpncalculators/c43.git`) rather than a single unified git
history shared with the patch-application site — `git apply -3`'s three-way
merge depends on the blob referenced by the patch's `index` line being a real
object `git cat-file -e` can resolve; without that, `-3` degrades to a plain
(non-merging) apply on any drift. **This is assigned as an explicit empirical
check to the Step 2 implementation prompt covering patch application** — Qwen
must test this against real patches and real upstream drift, not assume it
works.

**[RATIFIED]** — conflict-marker detection is not optional and is not
implied by `git apply -3`'s own exit code. `git apply -3` can exit
successfully while having left `<<<<<<<`/`=======`/`>>>>>>>` conflict markers
embedded in the merged output — a three-way merge "succeeding" in git's sense
is not the same as the result being valid, marker-free source. Relying on
the C compiler to incidentally reject such a file is not an acceptable
substitute for an explicit check (a marker could in principle land inside a
comment or string and "compile" while silently corrupting behavior).

**Decision: scan for conflict markers as a distinct checked step**, run
unconditionally after every `git apply -3` regardless of its reported exit
status, and fail the configure/build step if any marker is found. This ties
directly into item 7's hard-failure requirement — item 7's invariant is only
actually enforced if this scan exists.

---

## 6. Materialize/Refresh Developer Workflow

**Decision:** developers edit a fully materialized, real whole file (full
compiler/LSP context) in a working directory — not a bare patch or fragment.
A `refresh` command re-derives the patch from the developer's edits by
diffing the materialized file against upstream, at function-boundary
granularity per item 4.

**Rationale:** preserves a property the current design already relies on —
today's overrides *are* real whole files specifically so IDEs/LSPs following
`compile_commands.json` resolve real definitions
[VERIFIED: custom_package/README.md:88-90]. Editing a bare patch fragment
would regress that.

**Open question for implementation:** where the materialized working file
lives, and which artifact is the checked-in source of truth. Two options:
(a) the materialized file lives at today's conventional path
(`packages/<pkg>/<relative-path>`) and the `.patch` is a derived/generated
build artifact (not committed); or (b) the `.patch` is what's committed and
is the source of truth, with the materialized file living in a separate,
gitignored working directory regenerated on demand. This choice determines
code-review ergonomics (reviewing a real diff vs. reviewing a diff derived
from a working copy that may be stale relative to the committed patch) and
must be decided before Step 2's patch-generation prompt can specify exact
file paths.

---

## 7. Conflict Philosophy

**Decision (hard invariant):** a genuine same-function conflict between two
packages — same function, overlapping edits — must fail the build loudly,
either as a patch-apply failure or explicit conflict markers requiring manual
resolution. Never silently resolved by picking one package's version.

**Rationale:** silent resolution would mean one package's fix or behavior
change simply disappears from the shipped build with no signal to either
package author — the exact failure mode item 3 is designed to eliminate for
non-overlapping changes to the same file, and item 5's marker-detection
requirement exists specifically to make this invariant enforceable rather
than aspirational.

**Relationship to current behavior:** this is a strictly *stronger*
guarantee than exists today. Today, two packages overriding the same file is
only ever a stderr warning ("last wins") and the build still succeeds
[VERIFIED: tools/resolve_c47_src.py:225-228]. Under this proposal, mere
same-file overlap (different functions) composes cleanly per item 3, but a
genuine same-function content conflict must hard-fail — a case that today's
whole-file model cannot even distinguish from "different functions, same
file," since it never looks inside the file. This is called out explicitly
so this isn't mistaken for an accidental behavior change during review.

---

## 8. Fallback for Non-Function-Scoped Changes

**Decision:** (a) — changes to globals, `#define`s, macros, structs, or
typedefs fall back to whole-file override under today's existing convention
(`pkg_override_sources`, unchanged), rather than being deferred as an
undesigned gap.

**Rationale:** the existing whole-file mechanism is already proven and
already carries the full existing safety-guard set — sentinel-gated
delete-safety [VERIFIED: tools/resolve_c47_src.py:126-137,
custom_package/README.md:76-80], symlink-escape containment guards
[VERIFIED: tools/resolve_c47_src.py:55-75,201-208], fatal-on-missing-match
[VERIFIED: tools/resolve_c47_src.py:189-199], and byte-identical "dead
shadow" warnings [VERIFIED: tools/resolve_c47_src.py:210-214]. Building a
second new mechanism for non-function-scoped changes, on day one of the
patch-based redesign, would duplicate that guard set for no proven benefit —
function-boundary patching's whole rationale (item 4) is about reducing
spurious conflicts between packages editing unrelated *functions*, which
doesn't apply to a global/macro/struct change in the first place.

**[RATIFIED]** — if package A function-patches file X and package B
whole-file-overrides file X, the configure step must treat this as a
**fatal error**, not attempt to layer one mechanism's output on top of the
other's. The two mechanisms must be mutually exclusive per target file,
enforced by the tool at configure time — not left to convention or authoring
discipline. Rationale (human reviewer): same reasoning as other
same-file-ambiguity cases in this project — one mechanism per file, and the
tool must be the thing that enforces it, not documentation. This should be
added as an explicit checked invariant in Step 2's ordering/composition
prompt (item 5 of the Step 2 breakdown), not left implicit.

---

## Summary of `[DECISION NEEDED]` Items — RATIFIED (human review)

1. §2 — **RATIFIED:** path-mirroring alone is insufficient; every patch must
   carry an explicit override-target declaration (header comment or manifest
   entry, mechanism TBD in Step 2), checked against the path-inferred target,
   failing loudly on mismatch.
2. §4 — **RATIFIED:** libclang/`clang.cindex` is a dependency of authoring
   tooling only, never of the build-time shadow-tree step; enforced by a
   CI/configure-time assertion (not documentation alone) — exact mechanism
   TBD in Step 2.
3. §5 — **RATIFIED:** conflict-marker detection after `git apply -3` is a
   distinct checked step, run unconditionally regardless of `git apply -3`'s
   own exit status.
4. §8 — **RATIFIED:** function-patch and whole-file-override mechanisms are
   mutually exclusive per target file, enforced by the tool as a fatal
   configure error.

All four `[DECISION NEEDED]` items from the initial draft are now resolved.
Ready for Step 2 pending explicit go-ahead.

## Summary of `[VERIFIED: pending]` Items (assigned to Step 2 for empirical confirmation)

1. §5 — whether `git apply -3` blob-ancestry (`index` line SHA resolvability)
   actually holds given upstream is consumed from a separate clone, across
   real drift scenarios.
2. §1 — context-line window (`-U3` vs. larger) resilience against real
   upstream churn.
