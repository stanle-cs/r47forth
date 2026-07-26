# Proposed Spec Changes — Plain-Diff Package Overlay System

**Status:** revision 2, supersedes revision 1's libclang/function-boundary
design. No `custom_package/DESIGN.md` exists today; this document is written
so it could later seed one, but formalizing it is a human decision.
`custom_package/README.md` is the sole existing authority on current
behavior — `packages/forth-core/DESIGN.md` governs forth-core's own Forth
logic only and is not cited here.

**Scope:** package overlay/manager machinery only — `tools/resolve_c47_src.py`
and the `tools/pkg_patch_*.py` tooling, `custom_package/README.md`,
`Makefile`, top-level `meson.build`. Does not touch forth-core's Stage-2
Forth work, `freeList.c`, `labelList`/`programList`, or any other
audit-backlog item.

**Goal of the redesign (unchanged across both revisions):** move from
whole-file symlink overrides (last-listed-wins, no diff visibility) to
patch-based overrides with cumulative, explicitly ordered per-file
composition — so a package's actual delta against upstream is reviewable
without diffing two full files by hand, and two packages' independent
contributions to the same file compose instead of one silently clobbering
the other.

---

## Why revision 2: what changed and why

Revision 1 (implemented, then reverted on this branch) added
function-boundary granularity via libclang: patches were split per changed
function, and a separate whole-file-override mechanism handled anything
libclang couldn't attribute to a function body (globals, macros, structs,
added/removed functions), with a configure-time check enforcing the two
mechanisms stay mutually exclusive per file.

That design achieved its stated goal — avoiding spurious conflicts between
packages editing unrelated functions in the same file — but the actual
motivating constraint for this whole redesign is **package size** (small,
reviewable diffs), not merge-conflict avoidance specifically. (This
originally read "DM42-class flash/RAM budget"; the target is R47
specifically — ruled 2026-07-25 — so the reviewability half is the part
that still holds.) Function-boundary splitting bought conflict precision at a
real cost: a new build-time dependency (libclang) that had to be walled off
from the build path with its own enforcement machinery, a second storage
convention (whole-file override) that existed solely to catch what
function-splitting couldn't express, a mutual-exclusivity check to keep
those two conventions from colliding, and real authoring friction (a
developer editing a global couldn't use the same command as one editing a
function body). None of that machinery is required to hit the actual goal.

**Revision 2: plain whole-file `git diff`, one mechanism, no restriction on
what a developer edits.** This achieves the same size win (a diff is still
small relative to a full-file copy for a localized change) with none of the
above cost. The tradeoff being accepted: two packages editing different
parts of the *same function* now produce a textual diff conflict (git's
merge granularity is line-based, not AST-based) where function-boundary
splitting would have composed them cleanly. This is judged an acceptable
tradeoff — it fails loudly (per §5/§7, unchanged) rather than silently, and
same-function-different-package edits are the less common case in practice.

---

## SUPERSEDED (revision 1 decisions, not carried forward)

- **§2 (rev 1) — dual-signal override-target declaration** as a mechanism
  for distinguishing "this overrides an upstream file" from "this is a new
  file." Superseded not because dual-signal validation itself was wrong
  (it stays, see New Decision 6 below, and it caught two real self-audit
  bugs in the rev-1 implementation) — superseded because the thing it was
  disambiguating (patch vs. whole-file-override, two competing mechanisms)
  no longer exists. Under revision 2, "does this rel exist upstream"
  alone determines whether an entry belongs under `patches/` (exists) or
  `files/` (doesn't) — inferred, not developer-declared, and each of the
  two directories independently enforces its own existence check (New
  Decision 6), which is sufficient on its own; no separate classification
  layer is needed.
- **§4 (rev 1) — function-boundary patch granularity via libclang.**
  Superseded per the rationale above: the size goal doesn't require
  function-boundary granularity, and removing it eliminates an entire
  category of build-time risk (a wrongly-reachable libclang import) and
  authoring friction (restricting what a developer may edit in one pass).
  Plain `git diff` of the whole materialized file replaces it — see New
  Decision 1.
- **§8 (rev 1) — mutual exclusivity between function-patch and
  whole-file-override mechanisms.** Superseded because there is only one
  override mechanism now (`patches/`); the check existed solely to prevent
  two competing mechanisms from targeting the same file, and with only one
  mechanism there is nothing left for it to guard against. (A different,
  much narrower existence-based check remains for `patches/` vs. `files/`
  — see New Decision 6 — but it is not a revival of §8; it can't be
  triggered by two active mechanisms on the same file, only by a
  patch/file individually targeting a nonexistent/existent-when-it-
  shouldn't-be upstream path.)
- **`pkg_override_sources` / `pkg_override_headers` / `pkg_custom_sources`
  / `pkg_patch_sources`** manual meson.build declarations, and the
  per-package `subdir(pkg)` meson evaluation that read them. Superseded by
  auto-discovery (New Decision 3): a package directory contains exactly
  `patches/` and `files/`, nothing else is read or evaluated.

## KEPT (still valid under revision 2, unchanged in substance)

- **Materialize-then-edit developer workflow** (rev 1 §6): a full, real,
  editable copy of an upstream file, full compiler/LSP context — `refresh`
  re-derives patches from it. Revision 2 removes the function-boundary
  restriction on what may be edited in the materialized copy (see New
  Decision 1) but keeps the workflow shape itself.
- **`.patch` storage format** (rev 1 §1): `git diff` output, `-U3` or
  greater context, path-mirrored to the upstream file targeted. The
  empirical finding that `-U3` is the right choice (below) still holds —
  it was never about function-boundary granularity, only about `git
  apply`'s lack of fuzz under drift.
- **Cumulative, explicitly ordered composition** (rev 1 §3): numeric-prefix
  convention, applied in ordinal order across all active packages.
- **Application via `git apply -3`** against a freshly materialized
  upstream copy, with an **explicit, unconditional post-apply scan for
  conflict markers** as a distinct checked step (rev 1 §5) — a clean exit
  code alone was never sufficient and still isn't.
- **Loud-failure conflict philosophy** (rev 1 §7): a genuine overlapping
  edit between two packages fails the build, never silently resolved by
  picking one side.
- **New files with no upstream counterpart stored whole**, path-mirrored,
  no diffing (rev 1's "today, for contrast" baseline — unchanged).

### Empirical findings carried forward unchanged (still valid — neither depends on function-boundary granularity)

**Context window:** `-U3` is kept; a larger window is empirically *less*
resilient for direct application under drift (`git apply` has no fuzz — real
churn 6 lines from an edit broke `-U10` but not `-U3` in the original
experiment), and window size is irrelevant once `-3` three-way merge
engages (the merge uses the recorded pre-image blob, not the context). See
the full experiment record preserved below under "Application Mechanism."

**Blob ancestry:** holds, with the implemented safeguards (full-index
patches; generation-time `git cat-file -e` gate; scratch-repo application
with odb seeding of each patch's pre-image blob), and its failure mode is
loud in every observed case (conflict markers caught by the unconditional
scan, or an outright `git apply` failure) — never a silent mis-merge. Full
record preserved below.

---

## New Decisions (revision 2)

### 1. Single mechanism: plain whole-file diff

Package overrides of an existing upstream file are stored as one `git diff`
of the **whole materialized file** against its upstream counterpart —
`-U3` or greater context, no restriction on what kind of change it
contains. A function body, a global, a `#define`, a struct or typedef, an
added or removed function — all are just diff output; the tool draws no
distinction between them and imposes none on the developer.

**Rationale:** the package-size constraint this redesign exists for is
satisfied by diffing (a localized change produces a small patch regardless
of granularity); function-boundary restriction added real authoring
friction without being necessary for that goal (see "Why revision 2"
above).

### 2. `refresh` operates on a package directory, not a single file

`refresh` takes a package directory as its only argument (e.g.
`python3 tools/pkg_patch_refresh.py packages/my-pkg`), not a single target
file. It walks every materialized file directly under the package
directory that mirrors an upstream path (excluding `patches/` and `files/`
themselves — those are refresh's *output* and the new-file store,
respectively, not its input), diffs each against its upstream counterpart
at the same relative path, and for each: writes/overwrites a `.patch` file
if the materialized copy differs from upstream, or deletes any existing
`.patch` for that path if the materialized copy no longer differs (a
reverted edit) — so a package directory's `patches/` always reflects
exactly the current state of its materialized working copies, never
accumulating stale patches from abandoned edits.

Materialized working copies are an authoring-time convenience, not checked
in — only `patches/` and `files/` are (see New Decision 3).

**[SUPERSEDED — see "Automatic Classification, Flat Working Directory"
below]** the "no upstream counterpart → left alone and reported, developer
places it under `files/` themselves" half of this decision. `refresh` now
performs that placement automatically; a developer never manually creates
or populates `files/` (or `patches/`) at all. The "diff existing-upstream
files into `patches/`, clean up stale/reverted entries" half is unchanged
— only the new-file handling and the shape of the working area (flat,
mirroring upstream, with `patches/`+`files/` excluded from the scan) is
superseded, generalized to a genuinely automatic classification step with
no developer decision anywhere in it.

### 3. Auto-discovery — no manual declarations

A package directory contains exactly two subdirectories the resolver reads:
`patches/*.patch` (diffs, matched via the numeric-prefix + path-mirroring
filename convention already in place) and `files/*` (whole new files,
path-mirrored, recursively). Both are glob-matched by the resolver directly
at configure time — no `meson.build` inside a package directory is read or
evaluated, and none should exist; a package's on-disk `patches/`+`files/`
content **is** its declaration.

**Rationale:** removes an entire class of the silent-misconfiguration bug
this project has repeatedly had to guard against by hand (a typo'd
declaration variable name silently producing no override, no error, green
build). If a `.patch` file exists on disk, it is applied — there is no
separate "did you also remember to list it" step to typo.

### 4. `CUSTOM_PKG=` Makefile threading

`CUSTOM_PKG=` threads through the existing `sim`, `simc47`, `simr47`,
`both`, `test`, `repeattest`, `dmcp`, `dmcpr47`, `dmcp5`, `dmcp5r47`
Makefile targets as an optional variable, passed through to each target's
underlying `meson setup -D...` invocation — the same pattern already
established for `DMCP_PACKAGE=`/`f=`. **[VERIFIED during implementation]**
this threading was already in place for all ten targets prior to this unit
(each depends, directly or transitively, on `build.sim`/`build.dmcp`/
`build.dmcp5`, which already pass `-DCUSTOM_PKG=$(CUSTOM_PKG)`) — confirmed
via `make -n <target> CUSTOM_PKG=packages/x | grep -- -DCUSTOM_PKG` for
each target rather than re-implemented. New Decision 7 (reconfigure-on-
change) is what was actually missing and is implemented fresh.

### 5. `pkg_build PKG=<dir>` — the sole distributable-artifact path

A new Makefile target, `pkg_build PKG=<dir>`, is the only sanctioned way to
produce a distributable package artifact. It is test-gated (a package whose
`make test CUSTOM_PKG=$(PKG)` fails produces no artifact, full stop) and
enforces a size limit (`PKG_MAX_SIZE`, default 1MB — raised from 200KB on
2026-07-25, see `README.md`) against the actual assembled zip, not an
estimate. The limit is a tripwire against a package that has swallowed a
build directory, not a firmware budget; flash cost is measured separately. Full recipe in `Makefile`/§ below.

### 6. Fatal-at-configure: patch/file target must (not) exist upstream

Both directions are checked, each independently sufficient (see
"Superseded — §2" above for why no separate classification signal is
needed on top of these):

- A `patches/<NNN>-<rel>.patch` whose mirrored path does not correspond to
  a real file under `src/c47/<rel>` is a **fatal configure error** — typo,
  renamed, or deleted upstream file — naming the offending patch and path.
  (The dual-signal check from rev-1 — patch's own `+++` header cross-checked
  against its filename-decoded path — is kept as a second, independent
  check on top of the existence check: it catches a corrupted/mis-authored
  patch file even when the filename alone would resolve to a real path.)
- A `files/<rel>` whose mirrored path **does** correspond to a real file
  under `src/c47/<rel>` is likewise a **fatal configure error** — this
  is a change to an existing file placed under the wrong mechanism; it
  belongs under `patches/` instead. Naming the offending file and path.
- Two packages both placing a `files/<rel>` at the same mirrored path (both
  believing they're introducing the same new file) is a **fatal configure
  error** naming both packages and the path — there is no git-merge concept
  to fall back on for two competing whole files with no common base, so
  this cannot degrade to a loud-conflict-via-marker-scan the way two
  patches on the same existing file can; it must be caught before either
  file is ever copied into the shadow tree.

### 7. Reconfigure-on-`CUSTOM_PKG`-change, distinct from `f=1`

Switching the `CUSTOM_PKG` value between invocations against an existing
build directory forces a reconfigure by default. **Rationale:** Make's
directory-existence-gated target semantics mean `build.sim:`'s recipe (the
`meson setup` call) does not re-run once `build.sim/` already exists —
so today, invoking `make sim` with a different (or newly-empty, or
newly-set) `CUSTOM_PKG` value than the previous invocation used silently
reuses the previous shadow tree with no error and no rebuild of the
overlay. A stamp file recording the last-used `CUSTOM_PKG` value, checked
unconditionally before every build, closes this.

**This is independent of, and must not be conflated with, `f=1`**, which
controls only whether the GMP subproject is force-rebuilt
(`$(if $(f),test -d ...,rm -rf ...)` in the `build.dmcp`/`build.dmcp5`
rules) and has no relationship to `CUSTOM_PKG` or the shadow tree at all.
`f=1` says "trust the existing GMP build even if present"; the
`CUSTOM_PKG` stamp check says "always verify the package overlay matches
what was actually requested, regardless of what `f` says." A developer
passing `f=1 CUSTOM_PKG=packages/other-pkg` still gets a forced reconfigure
for the package-overlay reason, independent of GMP being skipped for the
`f=1` reason. Documented explicitly in the Makefile at the stamp-check
target, not left to be inferred.

---

## Automatic Classification, Flat Working Directory

**Status:** implemented. Supersedes the "developer manually places files
into `patches/` or `files/` themselves" half of New Decision 2 above. Does
not touch New Decision 3 (the build-time resolver's `patches/*.patch` /
`files/*` reading logic) at all — that remains exactly as implemented;
only what *populates* those two directories changes.

**Decision:** a package's working area is flat, mirroring upstream paths
directly — the same shape as the original (pre-revision-1) whole-file-
override convention:

```
packages/my-pkg/
├── keyboard.c        # materialized copy of an existing upstream file, edited
└── my_module.c       # a brand-new file, no upstream counterpart
```

The developer never creates `patches/` or `files/` themselves and never
decides which one a given file belongs in. `refresh` (invoked directly, or
via `make pkg_build PKG=<dir>`, which already called it) scans every file
directly under the package directory's flat working area — excluding
`patches/`/`files/` themselves, which are now purely generated — and
classifies each automatically:

- **A real upstream file exists at this mirrored path** → diff it against
  upstream → write/update `patches/<NNN>-<name>.patch`. If the working copy
  no longer differs from upstream (reverted edit), any stale patch for it
  is deleted.
- **No upstream file exists at this mirrored path** → copy it whole into
  `files/<name>` (path-mirrored), automatically — not left for the
  developer to place there by hand.
- **A working-area file present at a prior `refresh` but now deleted**
  (the developer removed their scratch copy): the corresponding generated
  `patches/`/`files/` entry is deleted too — the same "no longer producible
  from the working area" cleanup as the reverted-edit case, generalized to
  cover deletion as well as reversion, for both output mechanisms.
- **A working-area file matching `<pkgdir>/.pkgignore`** → not classified
  at all: neither diffed nor copied. See "Ignoring non-content" below.

### Ignoring non-content (`.pkgignore`)

**Status:** implemented (`load_pkgignore` / `is_ignored`, applied inside
`list_working_files`).

Automatic classification treats the working area as package content *by
definition* — a file with no upstream counterpart **is** a new source. That
inference is right for code and wrong for everything else a package
accumulates: design authority, amendment trails, plan/prompt documents, a
gate runner. `forth-core` had 18 such files; every one was being copied into
`files/`, shadowed into the build tree, and shipped inside the distributable,
with a fresh copy regenerated on every `refresh`.

An optional top-level `<pkgdir>/.pkgignore` excludes them. One glob per line,
`#` comments and blank lines dropped; a deliberately small `.gitignore`
subset: trailing `/` is a directory subtree, a pattern with no `/` matches the
**basename at any depth**, a pattern containing `/` is anchored to the package
root. `fnmatch` globbing, with two documented divergences kept for
implementation obviousness — `*` crosses `/` in path-form patterns, and there
is no negation. `.pkgignore` never classifies itself.

**Ignoring is retroactive by construction, not by special case.** An ignored
file is simply absent from `list_working_files`, so it is "no longer
producible from the working area" and the existing stale-cleanup pass deletes
any entry previously generated for it — the same code path as reversion and
deletion above. Removing a pattern restores it on the next run. The working
copy is never touched.

The filter lives in `list_working_files` — the one place every reader of the
working area goes through — so `refresh` and `rebase_base` cannot disagree
about what a package contains, and neither needs to know `.pkgignore` exists.

**Known sharp edge, accepted:** package `.c` files compile from the `files/`
copy, so ignoring one *silently removes it from the build* rather than
erroring. This is the same failure mode as deleting a working-area source, and
is warned about in the package README and `.pkgignore`'s own header rather
than mechanically prevented — a whitelist of "extensions you may not ignore"
would be a new declaration to keep in sync, which is precisely what revision 2
exists to abolish.

**Rationale:** removes the last remaining developer *decision* from the
authoring workflow. Revision 2 already removed manual declarations
(New Decision 3) and function-boundary classification (New Decision 1)
that were the point of a change; the one thing still left to a human was
*which generated directory a file belongs in* — trivially and always
correctly inferable from whether the path exists upstream, so leaving it
to the developer added a step that could only ever be done one right way,
with a wrong way (misplacing a new file into `patches/`, or an existing-
file edit into `files/`) available to get wrong. Automatic classification
removes that wrong way entirely; the developer's only decision is what to
edit, not where to put the result.

**`patches/` and `files/` are build OUTPUT ONLY.** A developer manually
placing or editing a file inside either directory is not a supported
workflow. This is enforced, not just documented: a hidden per-package
manifest, `<pkgdir>/.refresh-manifest.json` (tracking the sha256 of every
entry as `refresh` itself wrote it, committed alongside `patches/`+`files/`
— see below for why it must be committed), lets `refresh` detect drift on
every run. Before overwriting any `patches/`/`files/` entry, `refresh`
compares its current on-disk content against the manifest's recorded hash
for it:

- **Matches:** normal case — either untouched since last `refresh`, or this
  is the first time this entry has ever been written. No warning.
- **Mismatches, or the entry exists with no manifest record at all:**
  something outside `refresh` touched `patches/`/`files/` directly (hand-
  added or hand-edited, bypassing the working area). **Warned loudly (not
  failed)** — see "Warn, not fail" below — and then self-healed: `refresh`
  overwrites with freshly generated, correct content regardless, exactly as
  it would on the completely normal edit-working-copy-then-refresh path
  (which exercises the identical overwrite code, just without triggering
  the warning, since the manifest matches).

**Warn, not fail — reasoning:** the normal "edit working copy, run
`refresh`" cycle and the "someone hand-tampered with generated output"
case both end at the exact same overwrite operation; the only difference
`refresh` can observe is whether the *pre-overwrite* on-disk content
matches its own manifest record. Failing the build/authoring step here
would punish the artifact for existing in a state `refresh` can and does
correctly repair — the goal (per the task that introduced this check) is
"catch drift between what's committed and what the working area actually
reflects," i.e. visibility, not "be maximally strict for its own sake."
A human reading the warning can decide whether the drift was intentional
(e.g. a merge conflict resolution someone hand-patched into a `.patch`
file directly, which `refresh` will now silently discard) — the warning
exists so that decision is at least visible, not silent.

**The manifest is committed, not gitignored, alongside `patches/`+`files/`.**
Without it being committed, a fresh clone's first `refresh` run would have
no history of "what did refresh generate last" and would flag every single
legitimately-committed entry as unrecorded drift — a wall of false
positives on day one for every package. Committing it means the manifest's
state travels with the repo exactly like the patches/files it describes.

**The whole package directory is tracked, working area included**
(`packages/.gitignore` carries no rules; the top-level `.gitignore` is an
upstream file and is not touched). `patches/`, `files/`, the manifest **and**
the flat working-area sources are all committed.

**Superseded (R5-A2, decided 2026-07-15).** This section previously required
the opposite — the flat working area `.gitignore`d as "local editing scratch
[that] must never be committed" — and cited as verification that `git add -A`
stages only the generated entries. That verification tested the rule's
mechanism, not its consequence. Two consequences make the rule unsound:

1. **A clean clone deletes its own sources.** Refresh defines a missing working
   file as deletion of its generated output. A clone has no working area, so
   its first refresh reports `files_removed=[...]` and removes the
   compiler-visible source. Reproduced in a scratch repo using this exact
   `.gitignore`.
2. **It silently swallows everything new.** The old text noted that
   already-tracked files "are completely unaffected, since gitignore rules
   never apply to files git already tracks" — treating that as reassurance. It
   was the defect: the rule bit only files created *after* it landed, so the
   existing sources survived by accident while 15 design docs added later were
   tracked nowhere at all, with `git status` clean throughout.

The accepted cost is committed redundancy: working file and generated
counterpart both in git, able to disagree between refreshes. The refresh step
in `build-test.sh` is what reconciles them. The alternative — keeping the
working area local — requires an explicit deletion protocol plus a
generated-output-to-working-area rehydration design, and simply skipping
refresh reopens the proven silent-stale build bug.

`.pkgignore` is unchanged and is **classification only**: it decides what
refresh emits into `patches/`+`files/`, never what git tracks. Keeping docs out
of the distributable package is its job; git policy is not.

---

## Application Mechanism (record preserved from revision 1 — still valid)

**Decision:** patches are applied via `git apply -3` against a freshly
materialized copy of the current upstream file at build/prepare time.

**[VERIFIED: empirically]** Blob ancestry **holds, with the implemented
safeguards, and its failure mode is loud.** Real-drift experiment (patch
authored against this repo's committed `keyboard.c`, applied against
upstream HEAD's genuinely drifted version):

- *Ancestry available* (pre-image blob resolvable, seeded into the scratch
  apply repo): drift far from the edit → clean apply; drift overlapping the
  edit → `git apply -3` three-way-merges into a conflicted state and the
  unconditional marker scan catches it → **loud conflict**.
- *No ancestry* (pre-image blob unresolvable where apply runs): git prints
  `repository lacks the necessary blob to perform 3-way merge. Falling back
  to direct application...`; direct apply succeeds only while the `-U3`
  context is untouched by drift, and otherwise **fails outright** — loud,
  never a silent mis-merge.

Implementation notes that make ancestry hold in practice: (1) `refresh`
writes a full 40-char pre-image SHA and hard-fails if `git cat-file -e`
cannot resolve it at generation time (i.e. upstream file must be committed);
(2) the apply step runs inside a scratch git repo (never the real working
tree) and seeds each patch's pre-image blob into that scratch odb from this
repository (abbreviated index lines are resolved via `git rev-parse
<sha>^{blob}` first); (3) because patches are generated and applied within
this same repository, upstream pulls keep old pre-image blobs in history,
so ancestry survives drift. Known residual caveat: a **shallow clone** of
this repo may lack historical blobs — that degrades to the loud
no-ancestry behavior above, not to silent misapplication. **Authoring-side**
shallow behavior is stricter: `refresh`, `materialize`, and `--rebase-base`
all require the recorded base commit to be present in the repository
(validated via `git cat-file -e <sha>^{commit}`). If unavailable (shallow
clone, missing history), the tool fails with a named error before any
classification — never producing a misleading "upstream added" diagnosis.
Adjacent-line
drift (no unchanged line separating drift from an edit) conflicts loudly
even via `-3`.

**[RATIFIED]** — conflict-marker detection is not optional and is not
implied by `git apply -3`'s own exit code. `git apply -3` can exit
successfully while having left `<<<<<<<`/`=======`/`>>>>>>>` conflict
markers embedded in the merged output. Relying on the C compiler to
incidentally reject such a file is not an acceptable substitute for an
explicit check.

**Decision: scan for conflict markers as a distinct checked step**, run
unconditionally after every `git apply -3` regardless of its reported exit
status, and fail the configure/build step if any marker is found.

### Context-line window (`-U3`) — record preserved from revision 1

**[VERIFIED: empirically]** `-U3` is the default and larger windows are NOT
more resilient — they are strictly worse for direct application. Unlike
`patch(1)`, `git apply` has no fuzz — every context line must match (offset
search only). Real-drift experiment (this repo's committed `keyboard.c` vs
upstream HEAD's, 7 churned hunks): a one-line edit 6 lines away from real
churn applied cleanly at `-U3` but failed outright at `-U10` (the churn
landed inside the 10-line window). With blob ancestry available, `git apply
-3` rescued both window sizes identically (three-way merge uses the
recorded pre-image blob, not the context window). Conclusion: keep `-U3`;
resilience comes from `-3` + resolvable pre-image blobs, not from wider
context. (This finding predates and is independent of the function-boundary
vs. plain-diff granularity question — it is purely about `git apply`
mechanics and applies unchanged under revision 2.)

---

## Conflict Philosophy (unchanged from revision 1)

**Decision (hard invariant):** a genuine overlapping edit between two
packages targeting the same upstream file must fail the build loudly,
either as a patch-apply failure or explicit conflict markers requiring
manual resolution. Never silently resolved by picking one package's
version.

**Revision 2 note:** without function-boundary splitting, "overlapping
edit" is now git's own line-based merge-conflict definition, not an
AST-aware one — two packages editing *different lines of the same
function* still compose cleanly (git's three-way merge handles that as
today's `-3` mechanics already prove); two packages editing the *same
line*, or lines within each other's `-U3` context in a way that produces
a genuine hunk overlap, conflict loudly. This is the accepted tradeoff
described in "Why revision 2" above.

---

## Refresh Base Pinning — diff against the recorded base commit (PROPOSED, Phase 1 plan)

**Status:** APPROVED by human review 2026-07-14, with two amendments folded
in below: BP-4's upstream-deleted case is now a fatal stop (not a logged
`[GAP]`), and BP-6's `--set-base` open question is resolved **no**. Branch:
`pkgmgr/refresh-base-commit`.
Fixes: `refresh` diffs against live upstream instead of a package's recorded
base commit, producing wrong patches for packages whose upstream has since
moved.

**Scope:** `tools/pkg_patch_refresh.py`, `tools/test_pkg_patch_refresh.py`,
`custom_package/README.md`, this document. **Zero apply-side changes**
(`pkg_patch_apply.py`, `resolve_c47_src.py`, `Makefile` untouched — the
`refresh` CLI signature at the `pkg_build` call site
[VERIFIED: Makefile:248] is unchanged). Zero-touch upstream holds.

### Root cause / current behavior

1. `refresh` resolves every working file's diff counterpart to the **live
   on-disk working tree**: `src_c47_dir = <project_root>/src/c47`
   [VERIFIED: tools/pkg_patch_refresh.py:291], `upstream_path` built from it
   [VERIFIED: tools/pkg_patch_refresh.py:306], and `generate_patch` runs
   `git diff --no-index --full-index` between that live file and the working
   copy [VERIFIED: tools/pkg_patch_refresh.py:208-209].
2. No base-commit concept exists anywhere in the tooling: the manifest
   schema is exactly `{'patches': {name: sha256}, 'files': {rel: sha256}}`
   [VERIFIED: tools/pkg_patch_refresh.py:96-111].
3. **Failure scenario (the bug):** developer materializes `keyboard.c` at
   upstream state A and edits it; upstream then moves to state B (pull);
   developer runs `refresh`. The diff is (A + edits) vs B, so the patch
   encodes not only the developer's edits but the **reversal of every
   upstream A→B change** in that file. Nothing catches it:
   - the committed-content gate passes — B *is* committed
     [VERIFIED: tools/pkg_patch_refresh.py:228-236];
   - at configure time the patch applies **cleanly** — `apply_patch_stack`
     materializes current upstream (= B, the patch's exact pre-image)
     [VERIFIED: tools/pkg_patch_apply.py:236-243], so `-3` never needs to
     merge and no conflict is possible;
   - result: upstream's own changes are silently reverted in the shipped
     build — a **silent** wrong result, violating the loud-failure hard
     invariant (Conflict Philosophy above).
4. The irony: the design's verified drift machinery — pre-image blob
   seeding [VERIFIED: tools/pkg_patch_apply.py:263-267] plus `git apply
   --3way` [VERIFIED: tools/pkg_patch_apply.py:269] — was validated
   empirically for exactly this shape ("patch authored against this repo's
   committed keyboard.c, applied against upstream HEAD's genuinely drifted
   version", Application Mechanism section above). Generation-time
   live-diffing makes that path unreachable for the refresh-after-pull
   case: instead of a three-way merge (clean or loudly conflicted), you get
   a structurally wrong patch that applies cleanly.

### Decisions

**BP-1. Per-package base commit, recorded in the manifest.**
`.refresh-manifest.json` gains one top-level key, `"base_commit"` (full
40-char commit SHA). The manifest is already committed and travels with
`patches/`+`files/` (see "The manifest is committed" above), so the base
travels too. Per-package (not per-file) because a package is one authoring
epoch — mixed per-file bases would make every refresh and every rebase a
cross-epoch merge with no single "current upstream" to reason about, and
the task's own framing is a *package's* recorded base. `load_manifest`'s
missing/corrupt-tolerant behavior [VERIFIED: tools/pkg_patch_refresh.py:99-111]
is kept; a missing key triggers BP-3 initialization.

**BP-2. `generate_patch` diffs against base content, not the live file.**
The base copy of `<rel>` is extracted via `git show <base>:src/c47/<rel>`
to a temp file, and the existing `git diff --no-index --full-index`
mechanics run against that temp file instead of `src/c47/<rel>`.
[VERIFIED: empirically, this session] the resulting `index` line's
pre-image SHA is then exactly the **base blob's** SHA (git content-hashes
the on-disk file; identical content = identical blob id) — so:
- the existing `git cat-file -e` resolvability gate
  [VERIFIED: tools/pkg_patch_refresh.py:228-236] passes unchanged (the blob
  is in history);
- apply-side seeding + `-3` work unchanged, and application onto moved
  upstream goes through the exact drift path already verified in
  "Application Mechanism": clean merge when the developer's edits don't
  overlap upstream churn, loud conflict when they do. This is the entire
  fix at apply time — no apply-side code changes.

**BP-3. Base initialization: first refresh records `HEAD`.**
When no `base_commit` is recorded, `refresh` records `git rev-parse HEAD`
(fatal RuntimeError if unresolvable) at the start of the run, then
proceeds — so a fresh package's first refresh behaves byte-identically to
today. If generated `patches/`/`files/` entries already exist at
initialization time (a pre-fix package with a legacy manifest), a loud
stderr warning is printed (same channel as drift warnings) stating the
base was assumed to be current HEAD. If a `base_commit` IS already
recorded, it is validated with `git cat-file -e <sha>^{commit}` — fatal
if the commit is not available in the repository (shallow clone, missing
history). Residual, retroactive-only risk: if
upstream moved between a manual materialization and the first refresh
under the fixed tool, that first patch is wrong exactly as today —
unavoidable without a materialization-time record; closed going forward
by BP-5.

**BP-4. Classification stays live-keyed; epoch mismatches are handled
explicitly.** Before any file classification, the recorded base commit is
validated for availability (`git cat-file -e`); if unavailable, refresh
fails with a named error (the base is missing — not a file-level problem).
The patch-vs-files/ decision remains "does the rel exist in
the live tree" [VERIFIED: tools/pkg_patch_refresh.py:308] — it must stay
consistent with the configure-time existence checks (New Decision 6;
[VERIFIED: tools/pkg_patch_apply.py:205-212]). Two mismatch cases:
- **Exists live, absent at base** (upstream added the file after the
  package's base): **fatal**, named error telling the developer to advance
  the base (BP-6). Diffing that one file against live while its siblings
  diff against base would silently mix epochs inside one package.
- **Exists at base, deleted live** (upstream deleted the file): **fatal**,
  named error — stop so the developer can check whether the package's
  change is still applicable at all. [AMENDED at review, 2026-07-14: the
  previous behavior — silent reclassification of the working file into
  `files/`, i.e. a whole-file re-add of a file upstream deliberately
  deleted — was originally logged here as a pre-existing `[GAP]`; the
  human directed it be made a hard stop as part of this change.] The
  recorded base is what makes the two no-live-counterpart cases
  distinguishable at all: present at base → upstream deleted it → fatal;
  absent at base too → genuinely new package file → `files/`, exactly as
  today.

**BP-5. New `--materialize <rel>` CLI mode replaces raw `cp`.**
`python3 tools/pkg_patch_refresh.py <pkgdir> --materialize <rel>` writes
`git show <base>:src/c47/<rel>` into the flat working area (initializing
the base per BP-3 if unset). If the recorded base commit is unavailable,
materialize fails before attempting extraction. Fatal if `<rel>` doesn't
exist at base; fatal
if the working file already exists (no silent clobber of edits). The
README's raw-`cp`-from-live instruction [VERIFIED: custom_package/README.md:181]
is replaced. Rationale: once a base is pinned, copying from the *live*
tree bakes upstream's own base→live changes into the next patch as if the
developer authored them — the mirror image of the main bug — so the
sanctioned materialization must come from the base.

**BP-6. Advancing the base is explicit, never automatic:**
`--rebase-base [<commit>]` (default: current HEAD). The old recorded base
is validated for availability before any merge work — if unavailable,
rebase fails with a named error and the manifest is unchanged (even with
zero working files). For each working file
that exists at the old base: run `git merge-file` in place (working copy,
old-base content, new-base content); unedited copies fast-forward to
new-base content, genuine overlaps leave standard conflict markers in the
working copy. The new base is recorded after the pass; any file left with
markers blocks the next `refresh` via BP-7 until the developer resolves
them — in the working area, with full compiler context, per the
materialize-then-edit philosophy. `refresh` itself never moves the base.
Drift between `patches/` and the manifest at rebase time is warn-and-
proceed, consistent with the existing drift handling ("Warn, not fail"
above). [RESOLVED at review, 2026-07-14] no record-only
`--set-base <commit>` escape hatch — one sanctioned path
(`--rebase-base`) only.

**BP-7. Refresh hard-fails on conflict markers in a patch-classified
working file.** Same column-0 regex as the ratified apply-side scan
[VERIFIED: tools/pkg_patch_apply.py:50], applied to each working file that
classifies as a patch, fatal like the existing binary-file case
[VERIFIED: tools/pkg_patch_refresh.py:214-218]. Catches an unresolved
BP-6 rebase (or a hand-paste accident) before it's baked into a `.patch`.
Scope deliberately excludes `files/`-classified working files — they never
pass through `merge-file` (no base counterpart) and could conceivably
contain marker-shaped content legitimately.

**BP-8. Documentation.** `custom_package/README.md` Authoring Workflow and
Version Control sections updated: base concept, `--materialize`,
`--rebase-base`, the BP-3 legacy-initialization warning. This spec entry is
the design record. [AMENDED at review, 2026-07-14] hard requirement: the
developer-facing workflow must stay **simple** and the README **accurate**
— the happy path is exactly three commands (`--materialize`, edit,
`refresh`), plus `--rebase-base` only when upstream moves; every README
statement about diffing behavior must be updated to match the recorded-base
semantics, and no stale live-diff instruction (e.g. the raw `cp`) may
survive.

**Test wiring note:** the existing harness (`_TempProject`, a committed
temp git repo [VERIFIED: tools/test_pkg_patch_refresh.py:33-70]) already
supports multi-commit upstream histories via repeated `write_upstream`
commits — sufficient for base-pinning tests with no harness redesign. The
suite is reached by the gate: meson test entries
[VERIFIED: meson.build:172-186] run under `make test`
[VERIFIED: Makefile:216-217].

### Consolidated open items

- ~~`[GAP]` upstream-deleted-file silent reclassification~~ — RESOLVED at
  review (2026-07-14): now a fatal stop, folded into BP-4.
- ~~Open question (BP-6): record-only `--set-base`~~ — RESOLVED at review
  (2026-07-14): no.
- `[UNVERIFIED]`: none. The two load-bearing mechanical claims (historical
  blob SHA as `--no-index` pre-image; test-suite wiring into `make test`)
  were verified empirically this session, cited inline above.

---

## Known open item

**Package-level build configuration is not addressed by this design.**
Forth-core's current `packages/forth-core/meson.build` conditionally adds
`-DFORTH_DEBUG_SELFTEST` via a top-level meson option — a form of
package-level build configuration that has no home under the
patches-and-files-only convention (New Decision 3 explicitly states a
package directory contains nothing else, and no per-package `meson.build`
is read). This is out of scope for this revision (forth-core migration is
explicitly excluded — see Known Migration Gap in
`custom_package/IMPLEMENTATION_REPORT.md`) and is flagged here as a
`[DECISION NEEDED]` for whoever migrates forth-core: either such flags move
to the top-level `meson_options.txt` (one of the three files the package
manager itself owns) with the package selecting them by name somehow, or
this class of configuration is dropped entirely in favor of patches to the
relevant `#ifdef`/`#define` sites directly (which the single-diff mechanism
now supports without restriction, unlike revision 1).
