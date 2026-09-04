# Custom packages: README

External custom packages let you extend C47 (patch upstream files, add new
files) **without modifying any `src/` file**.  Every new line of meson
configuration lives inside `if custom_pkg_list != []` blocks in the top-level
`meson.build`. A vanilla build (`-DCUSTOM_PKG` unset or empty) is
byte-for-byte identical.

This is the plain-diff design (PROPOSED_SPEC_CHANGES.md, revision 2). It
supersedes an earlier function-boundary/libclang design. That design briefly
existed on this branch and was reverted. For that history, see
`design-docs/package-manager/IMPLEMENTATION_REPORT.md`.

---

## Current status: what actually works right now

**The mechanism itself works.** Automated tests (`tools/test_pkg_patch_*.py`)
cover it, plus real end-to-end verification documented in
`design-docs/package-manager/IMPLEMENTATION_REPORT.md` §8. Patching, new
files, cumulative composition, loud conflict failure, and `pkg_build` were
all exercised against real `meson`/`ninja`/`make` runs, beyond unit tests.

**`packages/forth-core/` uses this convention.** It was migrated: its flat
working area mirrors upstream paths, its `meson.build` is gone, and
`refresh` generates its `patches/` (14) + `files/` (24). A run of
`./packages/forth-core/build-test.sh` verified this: `FORTH SELF-TEST: ALL PASSED`,
exit 0, with forth-core code demonstrably live in the binary.

**The silent-green trap this system creates: read before authoring.** The
resolver globs `patches/`+`files/` and **never reads the flat working area**.
So a package whose `patches/` are missing or stale:

```
make sim CUSTOM_PKG=packages/some-pkg
```

**configures and builds successfully, with zero errors or warnings, and
silently contains none of that package's functionality**. The resolver finds
nothing to apply and proceeds as if the package list were empty. The same
applies per-file: edit `packages/forth-core/foo.c`, skip `refresh`, and the
compiler builds the *previous* content while your gate reports green. This was
verified directly by injecting a unique marker into a working-area source,
running `meson setup --reconfigure`, and grepping the shadow tree: the marker
was absent.

**Always run `refresh` before building.**
`./packages/forth-core/build-test.sh` does this for you as its first step, and
`make pkg_build PKG=...` runs it too. A hand-rolled `meson setup && ninja` does
not. Do not hand-roll.

---

## Quick start

A package's **working area is flat**. It mirrors upstream paths directly,
with no subdirectories for you to manage, no `meson.build`, and nothing to
declare:

```
packages/my-pkg/
├── keyboard.c        # your edited copy of an existing upstream file
└── my_module.c       # a brand-new file, no upstream counterpart
```

You never create `patches/` or `files/` yourself, and never decide which
one anything belongs in. Both are **generated build output**, and `refresh`
writes all of it (see **Authoring Workflow** below). After you run
`refresh`, the package directory looks like:

```
packages/my-pkg/
├── keyboard.c                        # your working copy (tracked in git)
├── my_module.c                       # your working copy (tracked in git)
├── .pkgignore                        # optional, committed — see below
├── .refresh-manifest.json            # generated, committed
├── patches/
│   └── 010-keyboard.c.patch          # generated, committed
└── files/
    └── my_module.c                   # generated, committed
```

### `.pkgignore`: what is not package content

Everything in the working area is package content *by definition*. A file
with no upstream counterpart is a new source. So it is copied into `files/`,
shadowed into the build tree, and shipped inside the distributable. That is
wrong for the design docs, notes and dev scripts developers keep beside the
sources. An optional top-level `.pkgignore` excludes them from classification
entirely (neither patched nor copied):

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
working area. So the next `refresh` deletes any `patches/`/`files/` entry it
previously generated for the file. This is the same cleanup as for a reverted
or deleted working file. Removing the pattern brings it back. Ignoring never
touches the working copy.

> Never ignore a source the build needs. Package `.c` files are compiled **from
> the `files/` copy**. So ignoring one silently removes it from the build, with
> no error. `forth-core` ignores `*.md`, `*.txt` and `build-test.sh`, and
> nothing else.
>
> **This silence is a decided, accepted risk (R5-A3, 2026-07-15), not an
> oversight.** A `.pkgignore` pattern matching a `.c` file produces no `files/`
> entry and **no warning**: `warnings=[]`, and the compiler never sees the
> source. Verified. A dynamic warning was considered and declined. Refresh
> can reuse the resolver's own `rel.endswith('.c')` compilation predicate to
> make the case loud, without reintroducing a per-file declaration list. So
> the option remains open at low cost if this ever bites. It was declined to
> keep the no-declaration design free of special cases. If you are debugging a
> source the compiler seems not to see, check `.pkgignore` first: nothing will
> tell you.

1. Create the package directory and edit files directly in it, normally
   via the **materialize-and-refresh workflow** below.
2. Configure and build with the package active:

   ```
   meson setup build.sim --buildtype=custom -DRASPBERRY=false \
       -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=packages/my-pkg --reconfigure
   ninja -C build.sim
   ```

   or, via the Makefile (recommended, see **Makefile Targets** below):

   ```
   make sim CUSTOM_PKG=packages/my-pkg
   ```

## The two mechanisms

| Your change is…                                                         | Mechanism | Where |
|---------------------------------------------------------------------------|-----------|-------|
| Any change to an **existing** upstream file: a function body, a global, a `#define`, a struct, an added/removed function, anything | a `.patch` (plain `git diff`, no restriction on content) | `patches/<NNN>-<rel_encoded>.patch` |
| A **genuinely new file** with no upstream counterpart at that relative path | the whole file, stored as-is | `files/<rel>` |

There is no classification decision for you to make. `refresh` infers
which mechanism a working-area file belongs to purely from whether its
mirrored path exists under `src/c47/`. It writes the result to the right
place automatically. The resolver independently enforces the same
existence rule at configure time (see **Fatal Checks** below), as belt and
suspenders. So even a hand-tampered `patches/`/`files/` entry that got
the classification backwards is a loud, named error, not a silent misfile.

### Patch storage format

```
packages/<pkgdir>/patches/<NNN>-<rel_encoded>.patch
```

`<NNN>` is a zero-padded 3-digit ordinal (`010`, `020`, …) and `<rel_encoded>`
is the upstream relative path with every `/` replaced by `__` (for example,
`programming/manage.c` becomes `programming__manage.c`). A patch is a
`git diff` (`-U3` or greater context) of the **whole file**, and it targets
`src/c47/<rel>`. Every `*.patch` file found under `patches/` is applied
automatically. There is no separate list to keep in sync.

**Dual-signal validation:** every patch carries two independent signals for
its target. These are the on-disk filename, and the `+++ b/...` header line
inside the diff itself. These must agree, and the agreed target must exist
under `src/c47/`. If either check fails, the result is a **fatal configure
error** that names the patch and the mismatch. It is never silently
reclassified as something else.

### Composition and ordering

Multiple patches can target the same upstream file, from one package or
several. They apply as a **cumulative, explicitly ordered stack**, not
last-listed-wins:

1. All patches targeting a given rel are collected across every active
   package in the `-DCUSTOM_PKG` list.
2. Sorted by the numeric `<NNN>` ordinal (parsed as an integer). Ties (same
   ordinal, different packages) are broken by the package's position in the
   `CUSTOM_PKG` list: the earlier-listed package's patch applies first.
3. The current upstream file is materialized fresh **once**. Then each
   patch in the sorted stack applies in turn via `git apply -3`. Each
   patch three-way-merges against the *previous* patch's output, not
   against pristine upstream again. So the stack is genuinely cumulative.
4. After **every** patch application, the result is scanned for conflict
   markers (`<<<<<<<`, `=======`, `>>>>>>>`) regardless of `git apply`'s exit
   status. Any marker, or any outright apply failure, is a **fatal configure
   error** that names the patch file. A genuine overlapping edit between two
   packages fails loudly. It is never resolved by silently picking one.

(Trade-off, stated explicitly: without function-boundary splitting, two
packages that edit *different lines of the same function* still compose
cleanly via `-3`. Two packages that edit the *same* lines conflict loudly.
The package-size goal this system exists for does not need finer granularity
than that. See the "Why revision 2" section of `PROPOSED_SPEC_CHANGES.md`.)

### Fatal checks

All of the following are **configure-time fatal errors**. They are checked
before the shadow tree is touched:

- A patch whose target doesn't exist under `src/c47/` (typo, renamed,
  deleted upstream file).
- A `files/<rel>` entry whose mirrored path **does** exist under `src/c47/`:
  that is an existing-file change and belongs under `patches/` instead.
- Two packages that both provide a `files/<rel>` entry for the same path.
  These are two competing whole files with no common base to merge against.
  So this cannot degrade to a loud `git apply` conflict the way a `patches/`
  collision can. It is caught before either file reaches the shadow tree.
- A malformed patch filename (missing the `NNN-` prefix, missing `.patch`,
  path-traversal or absolute-path encoding).

## Authoring workflow: materialize and refresh

You never write `.patch` files, and never touch `files/`, by hand. Both
are generated output. Edit a fully materialized real file (full
compiler/LSP context) directly in the flat working area. Then let
`refresh` classify and regenerate everything in one step:

```
python3 tools/pkg_patch_refresh.py packages/my-pkg --materialize keyboard.c
# ... edit packages/my-pkg/keyboard.c freely ...
python3 tools/pkg_patch_refresh.py packages/my-pkg
```

Brand-new files need no materialize step. Create them in the working area:

```
echo 'int helper(void) { return 1; }' > packages/my-pkg/helper.c
python3 tools/pkg_patch_refresh.py packages/my-pkg
```

(Or `make pkg_build PKG=packages/my-pkg`, which calls `refresh` for you as
one of its steps. See **Makefile Targets** below.)

`refresh` scans every file directly under the package directory's flat
working area. It excludes `patches/`, `files/`, and its own manifest, all
of which are generated output, never scan input. It classifies each file
**automatically**, and you make no placement decision:

- **mirrors a real upstream path, and differs from it at the recorded base** → writes/overwrites
  `patches/<NNN>-<rel>.patch` (ordinal reused from any existing patch for
  that rel, including a manual rename, so refresh never fights an
  explicit cross-package ordering choice you made by hand).
- **mirrors a real upstream path, and no longer differs from it** (a
  reverted edit) → deletes any existing patch for that rel.
- **has no upstream counterpart at all** → copied automatically into
  `files/<rel>` (you never place a file into `files/` yourself).
- **existed at a prior `refresh` run but is now deleted from the working
  area** (you removed your scratch copy) → the corresponding generated
  entry (patch or `files/` copy, whichever it was) is deleted too. So
  generated output never drifts ahead of what you are actually still
  working on.

### `patches/`/`files/` are output only: drift detection

A hidden manifest, `<pkgdir>/.refresh-manifest.json`, records the hash of
every entry as `refresh` itself wrote it, plus the upstream commit
(`base_commit`) the package was authored against. The manifest is
**committed alongside `patches/`+`files/`**, not gitignored (see
**Version Control** below for why). Before it overwrites any generated
entry, `refresh` compares the entry against this record. If a
`patches/`/`files/` entry was hand-added or hand-edited directly
(bypassing the working area entirely), `refresh` prints a warning that
names it. Then it **self-heals**: it overwrites the entry with freshly
generated, correct content. This is a visibility check, not a hard
failure. The exact same overwrite happens on every completely normal
edit-then-refresh cycle, and the mechanism that also catches tampering
must not block it.

### Version control

**Everything in a package directory is tracked in git**: your flat
working-area files as well as the generated `patches/`, `files/`, and
`.refresh-manifest.json`. `git add -A` stages all of it.

This reverses an earlier design in which the working area was `.gitignore`d as
regenerable scratch. It is not regenerable: **refresh treats a missing working
file as an instruction to delete its generated output**. So a clean clone,
which has no working area, deleted the compiler-visible source on its first
refresh (R5-A2, reproduced in a scratch repo). The same rule also silently
swallowed every file created after it landed, because ignore rules never
untrack what is already tracked.

The cost is committed redundancy: a working file and its generated counterpart
both live in git, and a refresh-less edit makes them disagree (the refresh step
in `build-test.sh` is what keeps them honest). That is the accepted price for a
clone that can refresh reproducibly.

`.pkgignore` is **not** git policy and never was. It decides only what refresh
emits into `patches/`+`files/`. See below.

Note the distinction from `pkg_build`'s distributable zip (below). The
**git repo** tracks the manifest alongside `patches/`+`files/` (the
manifest records `base_commit` as well as content hashes, so drift
detection and base tracking have history across clones). But the **zip
artifact** contains only `patches/`+`files/`. The manifest is an
authoring-time bookkeeping file with no purpose for a consumer of the
built package. So it is not shipped in the zip.

`pkg_patch_refresh.py` has no libclang or other AST-parsing dependency.
It is plain `git diff` plus a straight file copy for new files.

### When upstream moves

A package records the upstream commit it was authored against (its
*base*) in `.refresh-manifest.json`. `refresh` always diffs against that
base. If you pull new upstream commits, your patches do **not** change.

To move a package forward, run:

```
python3 tools/pkg_patch_refresh.py packages/my-pkg --rebase-base
```

This merges upstream's changes into your working copies. Unedited files
fast-forward. Genuine overlaps leave standard conflict markers
(`<<<<<<<`/`=======`/`>>>>>>>`) in the working copy. `refresh` will refuse
to run until you resolve them. Edit the markers away, then re-run
`refresh`.

If upstream added or deleted a file your package touches, `refresh` and
`--rebase-base` stop with a named error so you can check. Nothing is
ever silently reclassified or reverted.

## items.c: generator stub requirement

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
`GENERATE_TESTPGMS` defined: real handler bodies are replaced by empty
stubs. If you add table rows for new handler names without matching stubs,
the generator builds fail to link with "undefined reference" errors.

---

## Shadow tree: safety and editing

### Sentinel gate

The resolver creates a *shadow tree* at `build.sim/custom_pkg_shadow/`: a
mirror of `src/c47/` with your package's patches/new files layered on top.
On each reconfigure, it wipes and rebuilds this tree, but only **after**
every active package's `patches/`+`files/` content validates. A failing
configure leaves any existing shadow tree untouched.

**Delete-safety:** before it wipes, the resolver checks for the sentinel file
`DO_NOT_EDIT_shadow_tree.txt` at the shadow root. If the directory exists,
is non-empty, and **lacks** this sentinel, the resolver **refuses to delete
it** and aborts with a clear error. This prevents accidental destruction of
directories that were not created by the resolver.

### DO NOT edit the shadow tree

Most of the shadow tree is symlinks to upstream source files. For a
patched file, the entry is a real file that contains the applied result
(see below). If you edit an unpatched entry through the shadow, you edit
the **upstream** file. That violates the core promise that packages never
modify `src/`.

IDEs and language servers that follow `compile_commands.json` will resolve
"Go to Definition" into the shadow tree. **Do not edit files you reach this
way.** Edit only your package's flat working-area files under `packages/`,
then re-run `refresh`.

A patched file's shadow entry is a **real file** (never a symlink) that
holds the applied result. To edit it directly is harmless but pointless
(it is regenerated on every reconfigure). It is still not the right place
to work. Edit your working copy and refresh instead.

---

## Rebuilding: when reconfigure is needed

### Normal edits (symlink mode, default on Linux)

Unpatched upstream files, and `files/` (new-file) entries, are both
symlinked into the shadow tree. For `files/` entries, the shadow symlink
points at `<pkgdir>/files/<rel>`. So once that entry exists, a re-run of
`refresh` after further edits (which rewrites `files/<rel>` in place)
propagates through the symlink automatically, **no reconfigure needed**.
Reconfigure is only required the *first* time a given `files/<rel>` entry
is created (the shadow tree doesn't have the symlink yet).

A **patched** file's shadow entry is different: it is a real, applied-result
file, not a symlink. Patched content is derived, and must never be reached
by following a symlink back to real upstream (see **DO NOT Edit the
Shadow Tree** above). After you edit your working copy, you must re-run
`refresh` **and reconfigure** every time, to re-apply the patch stack
into the shadow tree.

### Optional incremental workflow: force package rematerialization

When you keep an existing build directory, set
`CUSTOM_PKG_RECONFIGURE=1` on the Make invocation immediately after
`refresh`. This forces Meson to rebuild the same package's shadow tree and
source list even though the `CUSTOM_PKG` value itself did not change:

```sh
python3 tools/pkg_patch_refresh.py packages/forth-core
make dmcp5r47 f=1 CUSTOM_PKG=packages/forth-core CUSTOM_PKG_RECONFIGURE=1
```

This is required when a refresh creates a new `files/<rel>` entry: the
copy of the generated file into `files/` is only the package-output step.
The existing shadow has no link for it yet, and Meson's configured source
list cannot know about a new `.c` file until reconfigure. It is also a
convenient explicit workflow after patched-file edits or in copy mode.
The switch is opt-in and preserves the incremental build directory. Once
the shadow is current, omit it on later unchanged builds.

### Copy mode (Windows / `CUSTOM_PKG_SHADOW_COPY=1`)

If symlink creation fails (common on Windows without privilege) or you set
`CUSTOM_PKG_SHADOW_COPY=1`, the resolver falls back to file copies. A
warning is printed at configure time, and the shadow tree becomes a
snapshot that requires `--reconfigure` after every change.

### Changing `-DCUSTOM_PKG`

**Via the Makefile** (`make sim`/`make test` and the other standard
targets): a switch of `CUSTOM_PKG` between invocations against an existing
build directory **forces a reconfigure automatically**. The Makefile
compares the requested value against a stamp left by the last successful
setup. If they differ, it re-runs `meson setup
--reconfigure` (this includes a switch to or from empty). This is
independent of `f=1`, which
only controls whether the GMP subproject is force-rebuilt for DMCP
cross-builds. If you want the mechanism in detail, see the
`check-custom-pkg-sim`/`check-custom-pkg-dmcp`/`check-custom-pkg-dmcp5`
targets in the top-level `Makefile`.

**Calling `meson setup` directly** (bypassing the Makefile): when you
change `-DCUSTOM_PKG` against an existing build directory, you are
responsible for passing `--reconfigure` yourself. Meson's own coredata
dirtying handles this on the next `ninja` run in most cases, but
`--reconfigure` is the reliable way.

**Do not `rm -rf build.sim`.** Deletion of the build directory is
unnecessary cargo-cult. Use `--reconfigure` (or the Makefile targets
above, which handle it for you).

---

## Multiple packages

`CUSTOM_PKG` accepts a comma-separated list of package directories:

```
meson setup build.sim --buildtype=custom -DRASPBERRY=false \
    -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=packages/pkg-a,packages/pkg-b --reconfigure
ninja -C build.sim
```

Patches from different packages that target the **same** upstream file
compose cumulatively in `CUSTOM_PKG` list order (see **Composition and
Ordering** above). This is not last-wins.

## Makefile targets

`CUSTOM_PKG=` threads through the standard build/test targets, same pattern
as the existing `DMCP_PACKAGE=`/`f=` variables:

| Target                              | Notes |
|--------------------------------------|-------|
| `sim`, `simc47`, `simr47`, `both`     | Standard/R47 simulator builds |
| `test`, `repeattest`                  | Full and incremental test runs |
| `dmcp`, `dmcpr47`, `dmcp5`, `dmcp5r47`| DM42/DM50 cross-builds |
| `test_asan`                           | ASan-instrumented test run: always passes `--reconfigure` itself, so it needs no reconfigure-on-change stamp check (see below) |

Usage: `make sim CUSTOM_PKG=packages/my-pkg`

### `pkg_build PKG=<dir>`: building a distributable package

The sole sanctioned way to produce a distributable package artifact:

```
make pkg_build PKG=packages/my-pkg
```

This runs the following, in order. First `make clean`. Then
`make test CUSTOM_PKG=<dir>` (a failing test suite stops here: no artifact
is produced). Then `refresh` against `<dir>`, to classify and regenerate
`patches/`/`files/` from any working-area edits not yet refreshed. Then it
assembles a zip that contains **only** `<dir>/patches/*` and
`<dir>/files/*` at `pkg_dist/<pkg-name>.zip` (your working-area files and
anything else in the package directory are excluded).

Note the order: the test suite runs **before** `refresh`, against whatever
`patches/`/`files/` are already committed. `refresh` only re-syncs them
from any working-area edits you have not refreshed yet. In the normal case
(you ran `refresh` yourself before committing), this step is a no-op, and
what is tested and what is shipped are identical. If you leave un-refreshed
working-area edits lying around at `pkg_build` time, the artifact that
ships is `refresh`'s *output*, which the test step never saw. If you want
a guarantee that the tested and shipped states match exactly, run
`refresh` yourself first.

The resulting zip's **actual size** is checked against `PKG_MAX_SIZE`
(default 1000000 bytes / ~1MB). If the zip exceeds it, that is a fatal
error that names the real size and the limit. The oversized zip is
deleted, not left behind as a false-positive artifact. Override with
`make pkg_build PKG=packages/my-pkg PKG_MAX_SIZE=2000000`.

**Dependent packages (`PKG_TEST_WITH`, added at PP19).** A package can
require another package (pretty-print-extra requires pretty-print: it
calls the core engine's API and never links alone). The resolver has no
dependency concept. The requirement is documented in the package's
DESIGN.md and enforced by its gate script. For `pkg_build`, pass the
package's minimal working composition:

```
make pkg_build PKG=packages/pretty-print-extra \
  PKG_TEST_WITH=packages/pretty-print,packages/pretty-print-extra
```

The gating test run uses the `PKG_TEST_WITH` list. The zip still
contains only `$(PKG)`'s own `patches/`+`files/`. If it is unset, the
test runs against `$(PKG)` alone (unchanged behavior).

`make clean` no longer removes `pkg_dist/` (changed at PP19): pkg_build
starts with `make clean`, so with two packages the second run destroyed
the first artifact. Each pkg_build refreshes its own zip. When you want
`pkg_dist/` gone, remove it by hand.

> **The cap is a tripwire, not a firmware budget** (raised from 200000,
> 2026-07-25). The original figure came from a "DM42-class flash/RAM"
> rationale. The target is R47 specifically, so that rationale is void.
> Zip bytes were never flash bytes in any case: forth-core's payload is
> ~75% `test_dict_reloc.c`, the self-test suite, and that suite compiles
> to nothing on device (`PC_BUILD && FORTH_DEBUG_SELFTEST`). Real flash
> cost is measured per stage with `make dmcp5r47` and recorded in the
> stage commit. At 200000, the check was already failing for forth-core
> before anyone first ran it (the package was already 223KB). That is
> what a threshold set 15% under the real value buys you. What remains
> worth catching is a package that swallowed a build directory or a
> binary: an order of magnitude out, not a sliver.

**Note:** `PKG` is also used elsewhere in this Makefile for the numbered
DMCP build-variant targets (`dmcp_pkg1`/`2`/`3`, unrelated to `CUSTOM_PKG`
package overlays). `make pkg_build PKG=packages/my-pkg` alone is unaffected.
If you invoke `pkg_build` together with a numbered `dmcp_pkg*` target in the
*same* command line, they collide (Make expands `$(PKG)` in target names at
parse time). Do not do that.

---

## Limitations

- *(Stale bullet removed 2026-07-15: `packages/forth-core/` migrated to this
  system long ago. See **Current Status** at the top of this document.)*
- **Documentation:** doxygen (`docs/code`) only scans `src/c47/`. Package
  sources and patches do not appear in generated documentation.
- **Package-level build configuration:** there is no mechanism for a
  package to add its own build options/flags (for example, a custom meson
  `get_option()`-gated `-D` define). A package directory contains only
  `patches/`+`files/`, and nothing else is read. If you need this, see the
  "Known open item" in `PROPOSED_SPEC_CHANGES.md`.
