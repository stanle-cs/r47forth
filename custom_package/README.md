# Custom Packages — README

External custom packages let you extend C47 (patch upstream files, add new
files) **without modifying any `src/` file**.  Every new line of meson
configuration lives inside `if custom_pkg_list != []` blocks in the top-level
`meson.build` — a vanilla build (`-DCUSTOM_PKG` unset or empty) is
byte-for-byte identical.

This is the plain-diff design (PROPOSED_SPEC_CHANGES.md, revision 2). It
supersedes an earlier function-boundary/libclang design that briefly existed
on this branch and was reverted — see
`custom_package/IMPLEMENTATION_REPORT.md` if you're looking for that history.

---

## Current Status — What Actually Works Right Now

**The mechanism itself works** and is covered by automated tests
(`tools/test_pkg_patch_*.py`) plus real end-to-end verification documented
in `custom_package/IMPLEMENTATION_REPORT.md` §8 — patching, new files,
cumulative composition, loud conflict failure, and `pkg_build` have all been
exercised against real `meson`/`ninja`/`make` runs, not just unit tests.

**`packages/forth-core/` uses this convention.** It was migrated: its flat
working area mirrors upstream paths, its `meson.build` is gone, and
`refresh` generates its `patches/` (14) + `files/` (24). Verified by running
`./packages/forth-core/build-test.sh` — `FORTH SELF-TEST: ALL PASSED`, exit 0,
with forth-core code demonstrably live in the binary.

**The silent-green trap this system creates — read before authoring.** The
resolver globs `patches/`+`files/` and **never reads the flat working area**.
So a package whose `patches/` are missing or stale:

```
make sim CUSTOM_PKG=packages/some-pkg
```

**configures and builds successfully, with zero errors or warnings, and
silently contains none of that package's functionality** — the resolver finds
nothing to apply and proceeds as if the package list were empty. The same
applies per-file: edit `packages/forth-core/foo.c`, skip `refresh`, and the
compiler builds the *previous* content while your gate reports green. This was
verified directly by injecting a unique marker into a working-area source,
running `meson setup --reconfigure`, and grepping the shadow tree: the marker
was absent.

**Therefore: always run `refresh` before building.**
`./packages/forth-core/build-test.sh` does this for you as its first step, and
`make pkg_build PKG=...` runs it too. A hand-rolled `meson setup && ninja` does
not — do not hand-roll.

---

## Quick Start

A package's **working area is flat**, mirroring upstream paths directly —
no subdirectories for you to manage, no `meson.build`, nothing to declare:

```
packages/my-pkg/
├── keyboard.c        # your edited copy of an existing upstream file
└── my_module.c       # a brand-new file, no upstream counterpart
```

You never create `patches/` or `files/` yourself, and never decide which
one anything belongs in — both are **generated build output**, written
entirely by `refresh` (see **Authoring Workflow** below). After running
`refresh`, the package directory looks like:

```
packages/my-pkg/
├── keyboard.c                        # your working copy (gitignored, not committed)
├── my_module.c                       # your working copy (gitignored, not committed)
├── .pkgignore                        # optional, committed — see below
├── .refresh-manifest.json            # generated, committed
├── patches/
│   └── 010-keyboard.c.patch          # generated, committed
└── files/
    └── my_module.c                   # generated, committed
```

### `.pkgignore` — what is not package content

Everything in the working area is package content *by definition*: a file with
no upstream counterpart is a new source, so it gets copied into `files/`,
shadowed into the build tree, and shipped inside the distributable. That is
wrong for the design docs, notes and dev scripts developers keep beside the
sources. An optional top-level `.pkgignore` excludes them from classification
entirely — neither patched nor copied:

```
# one glob per line; blank lines and # comments ignored
*.md            # no '/' → matches the BASENAME at any depth
docs/           # trailing '/' → the whole subtree
notes/*.txt     # contains '/' → anchored to the package root
build-test.sh
```

Globbing is `fnmatch`. Two deliberate divergences from `.gitignore`, both to
keep the implementation obvious: `*` matches across `/` in path-form patterns,
and there is no negation (`!`). `.pkgignore` never classifies itself.

Adding a pattern is **retroactive**: the file stops being producible from the
working area, so the next `refresh` deletes any `patches/`/`files/` entry it had
previously generated for it — the same cleanup as a reverted or deleted working
file. Removing the pattern brings it back. Ignoring never touches the working
copy.

> Never ignore a source the build needs. Package `.c` files are compiled **from
> the `files/` copy**, so ignoring one silently removes it from the build rather
> than erroring — `forth-core` ignores `*.md`, `*.txt` and `build-test.sh`, and
> nothing else.

1. Create the package directory and edit files directly in it — normally
   via the **materialize-and-refresh workflow** below.
2. Configure and build with the package active:

   ```
   meson setup build.sim --buildtype=custom -DRASPBERRY=false \
       -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=packages/my-pkg --reconfigure
   ninja -C build.sim
   ```

   or, via the Makefile (recommended — see **Makefile Targets** below):

   ```
   make sim CUSTOM_PKG=packages/my-pkg
   ```

## The Two Mechanisms

| Your change is…                                                         | Mechanism | Where |
|---------------------------------------------------------------------------|-----------|-------|
| Any change to an **existing** upstream file — a function body, a global, a `#define`, a struct, an added/removed function, anything | a `.patch` (plain `git diff`, no restriction on content) | `patches/<NNN>-<rel_encoded>.patch` |
| A **genuinely new file** with no upstream counterpart at that relative path | the whole file, stored as-is | `files/<rel>` |

There is no classification decision for you to make: `refresh` infers
which mechanism a working-area file belongs to purely from whether its
mirrored path exists under `src/c47/`, and writes the result to the right
place automatically. The resolver independently enforces the same
existence rule at configure time (see **Fatal Checks** below) — belt and
suspenders — so even a hand-tampered `patches/`/`files/` entry that got
the classification backwards is a loud, named error, not a silent misfile.

### Patch Storage Format

```
packages/<pkgdir>/patches/<NNN>-<rel_encoded>.patch
```

`<NNN>` is a zero-padded 3-digit ordinal (`010`, `020`, …) and `<rel_encoded>`
is the upstream relative path with every `/` replaced by `__` (e.g.
`programming/manage.c` becomes `programming__manage.c`). A patch is a
`git diff` (`-U3` or greater context) of the **whole file**, targeting
`src/c47/<rel>` — every `*.patch` file found under `patches/` is applied
automatically; there is no separate list to keep in sync.

**Dual-signal validation:** every patch carries two independent signals for
its target — the on-disk filename, and the `+++ b/...` header line inside
the diff itself. These must agree, and the agreed target must exist under
`src/c47/`. Either check failing is a **fatal configure error** naming the
patch and the mismatch — never silently reclassified as something else.

### Composition and Ordering

Multiple patches may target the same upstream file — from one package or
several. They apply as a **cumulative, explicitly ordered stack**, not
last-listed-wins:

1. All patches targeting a given rel are collected across every active
   package in the `-DCUSTOM_PKG` list.
2. Sorted by the numeric `<NNN>` ordinal (parsed as an integer); ties (same
   ordinal, different packages) are broken by the package's position in the
   `CUSTOM_PKG` list — the earlier-listed package's patch applies first.
3. The current upstream file is materialized fresh **once**, then each
   patch in the sorted stack applies in turn via `git apply -3` — each
   patch three-way-merges against the *previous* patch's output, not
   against pristine upstream again, so the stack is genuinely cumulative.
4. After **every** patch application, the result is scanned for conflict
   markers (`<<<<<<<`, `=======`, `>>>>>>>`) regardless of `git apply`'s exit
   status. Any marker, or any outright apply failure, is a **fatal configure
   error** naming the patch file — a genuine overlapping edit between two
   packages fails loudly, never resolved by silently picking one.

(Trade-off, stated explicitly: without function-boundary splitting, two
packages editing *different lines of the same function* still compose
cleanly via `-3`; two packages editing the *same* lines conflict loudly.
The package-size goal this system exists for doesn't need finer granularity
than that — see `PROPOSED_SPEC_CHANGES.md`'s "Why revision 2" section.)

### Fatal Checks

All of the following are **configure-time fatal errors**, checked before
the shadow tree is touched:

- A patch whose target doesn't exist under `src/c47/` (typo, renamed,
  deleted upstream file).
- A `files/<rel>` entry whose mirrored path **does** exist under `src/c47/`
  — that's an existing-file change and belongs under `patches/` instead.
- Two packages both providing a `files/<rel>` entry for the same path — two
  competing whole files with no common base to merge against, so this can't
  degrade to a loud `git apply` conflict the way a `patches/` collision can;
  it's caught before either file reaches the shadow tree.
- A malformed patch filename (missing the `NNN-` prefix, missing `.patch`,
  path-traversal or absolute-path encoding).

## Authoring Workflow — Materialize and Refresh

You never write `.patch` files, and never touch `files/`, by hand — both
are generated output. Edit a fully materialized real file (full
compiler/LSP context) directly in the flat working area, then let
`refresh` classify and regenerate everything in one step:

```
python3 tools/pkg_patch_refresh.py packages/my-pkg --materialize keyboard.c
# ... edit packages/my-pkg/keyboard.c freely ...
python3 tools/pkg_patch_refresh.py packages/my-pkg
```

Brand-new files need no materialize step — just create them in the working area:

```
echo 'int helper(void) { return 1; }' > packages/my-pkg/helper.c
python3 tools/pkg_patch_refresh.py packages/my-pkg
```

(or `make pkg_build PKG=packages/my-pkg`, which calls `refresh` for you as
one of its steps — see **Makefile Targets** below.)

`refresh` scans every file directly under the package directory's flat
working area (excluding `patches/`, `files/`, and its own manifest, all of
which are generated output, never scan input) and classifies each
**automatically** — you make no placement decision:

- **mirrors a real upstream path, and differs from it at the recorded base** → writes/overwrites
  `patches/<NNN>-<rel>.patch` (ordinal reused from any existing patch for
  that rel, including a manual rename — so refresh never fights an
  explicit cross-package ordering choice you made by hand);
- **mirrors a real upstream path, and no longer differs from it** (a
  reverted edit) → deletes any existing patch for that rel;
- **has no upstream counterpart at all** → copied automatically into
  `files/<rel>` — you never place a file into `files/` yourself;
- **existed at a prior `refresh` run but is now deleted from the working
  area** (you removed your scratch copy) → the corresponding generated
  entry (patch or `files/` copy, whichever it was) is deleted too, so
  generated output never drifts ahead of what you're actually still
  working on.

### `patches/`/`files/` Are Output Only — Drift Detection

A hidden manifest, `<pkgdir>/.refresh-manifest.json`, records the hash of
every entry as `refresh` itself wrote it, plus the upstream commit
(`base_commit`) the package was authored against — **committed alongside
`patches/`+`files/`**, not gitignored (see **Version Control** below for
why). Before overwriting any generated entry, `refresh` compares it against
this record: if a `patches/`/`files/` entry was hand-added or hand-edited
directly (bypassing the working area entirely), `refresh` prints a warning
naming it, then **self-heals** by overwriting with freshly generated,
correct content — this is a visibility check, not a hard failure, since the
exact same overwrite happens on every completely normal edit-then-refresh
cycle and must not be blocked by the mechanism that also catches tampering.

### Version Control

Your flat working-area files (everything directly under a package
directory except `patches/`, `files/`, and the manifest) are local editing
scratch — `.gitignore`d via `packages/.gitignore`, never committed. Only
`patches/`, `files/`, and `.refresh-manifest.json` are tracked in git.
Running `git add -A` inside a package directory stages only these generated
entries; your working copies are silently excluded, by design.

Note the distinction from `pkg_build`'s distributable zip (below): the
**git repo** tracks the manifest alongside `patches/`+`files/` (the
manifest records `base_commit` as well as content hashes, so drift
detection and base tracking have history across clones), but the **zip
artifact** contains only `patches/`+`files/` — the manifest is an
authoring-time bookkeeping file with no purpose for a consumer of the
built package, so it's not shipped in the zip.

`pkg_patch_refresh.py` has no libclang or other AST-parsing dependency;
it's plain `git diff` plus a straight file copy for new files.

### When Upstream Moves

A package records the upstream commit it was authored against (its
*base*) in `.refresh-manifest.json`. `refresh` always diffs against that
base — pulling new upstream commits does **not** change your patches.

To move a package forward, run:

```
python3 tools/pkg_patch_refresh.py packages/my-pkg --rebase-base
```

This merges upstream's changes into your working copies. Unedited files
fast-forward; genuine overlaps leave standard conflict markers
(`<<<<<<<`/`=======`/`>>>>>>>`) in the working copy. `refresh` will refuse
to run until you resolve them — edit the markers away, then re-run
`refresh`.

If upstream added or deleted a file your package touches, `refresh` and
`--rebase-base` stop with a named error so you can check — nothing is
ever silently reclassified or reverted.

## items.c — Generator Stub Requirement

If your package patches `src/c47/items.c` to add new handler table rows,
the patch must also add **stub definitions** for those handlers inside the
generator guard block:

```c
#if defined(GENERATE_CATALOGS) || defined(GENERATE_TESTPGMS)
  void fnMyNewHandler(uint16_t unusedButMandatoryParameter) {}
  /* … one stub per new handler … */
#endif
```

Upstream `items.c` provides stubs for all built-in handlers in this block.
The catalog generator (`generateCatalogs`) and test-program generator
(`generateTestPgms`) compile `items.c` with `GENERATE_CATALOGS` or
`GENERATE_TESTPGMS` defined — real handler bodies are replaced by empty
stubs. Adding table rows for new handler names without matching stubs makes
the generator builds fail to link with "undefined reference" errors.

---

## Shadow Tree — Safety & Editing

### Sentinel Gate

The resolver creates a *shadow tree* at `build.sim/custom_pkg_shadow/` — a
mirror of `src/c47/` with your package's patches/new files layered on top.
On each reconfigure, it wipes and rebuilds this tree, but only **after**
every active package's `patches/`+`files/` content validates — a failing
configure leaves any existing shadow tree untouched.

**Delete-safety:** before wiping, the resolver checks for the sentinel file
`DO_NOT_EDIT_shadow_tree.txt` at the shadow root. If the directory exists,
is non-empty, and **lacks** this sentinel, the resolver **refuses to delete
it** and aborts with a clear error — this prevents accidental destruction of
directories that were not created by the resolver.

### DO NOT Edit the Shadow Tree

Most of the shadow tree is symlinks to upstream source files (or, for a
patched file, a real file containing the applied result — see below).
Editing an unpatched entry through the shadow edits the **upstream** file —
violating the core promise that packages never modify `src/`.

IDEs and language servers that follow `compile_commands.json` will resolve
"Go to Definition" into the shadow tree. **Do not edit files you reach this
way.** Edit only your package's flat working-area files under `packages/`,
then re-run `refresh`.

A patched file's shadow entry is a **real file** (never a symlink) holding
the applied result — editing it directly is harmless-but-pointless (it's
regenerated on every reconfigure) but still isn't the right place to work;
edit your working copy and refresh instead.

---

## Rebuilding — When Reconfigure Is Needed

### Normal Edits (Symlink Mode — Default on Linux)

Unpatched upstream files, and `files/` (new-file) entries, are both
symlinked into the shadow tree — for `files/` entries, the shadow symlink
points at `<pkgdir>/files/<rel>`, so once that entry exists, re-running
`refresh` after further edits (which rewrites `files/<rel>` in place)
propagates through the symlink automatically, **no reconfigure needed**.
Reconfigure is only required the *first* time a given `files/<rel>` entry
is created (the shadow tree doesn't have the symlink yet).

A **patched** file's shadow entry is different: it's a real, applied-result
file, not a symlink (patched content is derived, and must never be reached
by following a symlink back to real upstream — see **DO NOT Edit the
Shadow Tree** above). Editing your working copy requires re-running
`refresh` **and reconfiguring** every time, to re-apply the patch stack
into the shadow tree.

### Copy Mode (Windows / `CUSTOM_PKG_SHADOW_COPY=1`)

If symlink creation fails (common on Windows without privilege) or you set
`CUSTOM_PKG_SHADOW_COPY=1`, the resolver falls back to file copies — a
warning is printed at configure time, and the shadow tree becomes a
snapshot requiring `--reconfigure` after every change.

### Changing `-DCUSTOM_PKG`

**Via the Makefile** (`make sim`/`make test`/etc.): switching `CUSTOM_PKG`
between invocations against an existing build directory **forces a
reconfigure automatically** — the Makefile compares the requested value
against a stamp left by the last successful setup and re-runs `meson setup
--reconfigure` if they differ (including switching to/from empty). This is
independent of `f=1`, which only controls whether the GMP subproject is
force-rebuilt for DMCP cross-builds. See the `check-custom-pkg-sim`/
`check-custom-pkg-dmcp`/`check-custom-pkg-dmcp5` targets in the top-level
`Makefile` if you want the mechanism in detail.

**Calling `meson setup` directly** (bypassing the Makefile): you are
responsible for passing `--reconfigure` yourself when changing `-DCUSTOM_PKG`
against an existing build directory — meson's own coredata dirtying handles
this on the next `ninja` run in most cases, but `--reconfigure` is the
reliable way.

**Do not `rm -rf build.sim`.** Deleting the build directory is unnecessary
cargo-cult; use `--reconfigure` (or the Makefile targets above, which handle
it for you).

---

## Multiple Packages

`CUSTOM_PKG` accepts a comma-separated list of package directories:

```
meson setup build.sim --buildtype=custom -DRASPBERRY=false \
    -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=packages/pkg-a,packages/pkg-b --reconfigure
ninja -C build.sim
```

Patches from different packages targeting the **same** upstream file compose
cumulatively in `CUSTOM_PKG` list order (see **Composition and Ordering**
above) — this is not last-wins.

## Makefile Targets

`CUSTOM_PKG=` threads through the standard build/test targets, same pattern
as the existing `DMCP_PACKAGE=`/`f=` variables:

| Target                              | Notes |
|--------------------------------------|-------|
| `sim`, `simc47`, `simr47`, `both`     | Standard/R47 simulator builds |
| `test`, `repeattest`                  | Full and incremental test runs |
| `dmcp`, `dmcpr47`, `dmcp5`, `dmcp5r47`| DM42/DM50 cross-builds |
| `test_asan`                           | ASan-instrumented test run — always passes `--reconfigure` itself, so it needs no reconfigure-on-change stamp check (see below) |

Usage: `make sim CUSTOM_PKG=packages/my-pkg`

### `pkg_build PKG=<dir>` — Building a Distributable Package

The sole sanctioned way to produce a distributable package artifact:

```
make pkg_build PKG=packages/my-pkg
```

This runs, in order: `make clean`; `make test CUSTOM_PKG=<dir>` (a failing
test suite stops here — no artifact is produced); `refresh` against `<dir>`
(classifying and regenerating `patches/`/`files/` from any working-area
edits not yet refreshed); then assembles a zip containing **only**
`<dir>/patches/*` and `<dir>/files/*` (your working-area files and anything
else in the package directory are excluded) at `pkg_dist/<pkg-name>.zip`.

Note the order: the test suite runs **before** `refresh`, against whatever
`patches/`/`files/` are already committed — `refresh` only re-syncs them
from any working-area edits you haven't refreshed yet. In the normal case
(you ran `refresh` yourself before committing) this step is a no-op and
what's tested and what's shipped are identical. If you leave un-refreshed
working-area edits lying around at `pkg_build` time, the artifact that
ships is `refresh`'s *output*, which the test step never saw — run
`refresh` yourself first if you want a guarantee that the tested and
shipped states match exactly.

The resulting zip's **actual size** is checked against `PKG_MAX_SIZE`
(default 200000 bytes / ~200KB) — exceeding it is a fatal error naming the
real size and the limit, and the oversized zip is deleted, not left behind
as a false-positive artifact. Override with
`make pkg_build PKG=packages/my-pkg PKG_MAX_SIZE=500000`.

**Note:** `PKG` is also used elsewhere in this Makefile for the numbered
DMCP build-variant targets (`dmcp_pkg1`/`2`/`3`, unrelated to `CUSTOM_PKG`
package overlays). `make pkg_build PKG=packages/my-pkg` alone is unaffected;
invoking `pkg_build` together with a numbered `dmcp_pkg*` target in the
*same* command line would collide (Make expands `$(PKG)` in target names at
parse time) — don't do that.

---

## Limitations

- **No existing package uses this system yet** — see **Current Status** at
  the top of this document. `packages/forth-core/` needs migration before
  `CUSTOM_PKG=packages/forth-core` does anything.
- **Documentation:** doxygen (`docs/code`) only scans `src/c47/`. Package
  sources and patches do not appear in generated documentation.
- **Package-level build configuration:** there is no mechanism for a
  package to add its own build options/flags (e.g. a custom meson
  `get_option()`-gated `-D` define) — a package directory contains only
  `patches/`+`files/`, nothing else is read. If you need this, see the
  "Known open item" in `PROPOSED_SPEC_CHANGES.md`.
