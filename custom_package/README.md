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

**No package in this repository currently uses it.** `packages/forth-core/`
predates this convention and still declares itself the old way
(`pkg_override_sources` in its own `meson.build`, override files sitting
directly at `packages/forth-core/<rel>`). The resolver described in this
document **never reads a package's `meson.build`** — it only globs
`patches/`+`files/`. Concretely, right now:

```
make sim CUSTOM_PKG=packages/forth-core
```

**configures and builds successfully, with zero errors or warnings, and
silently contains none of forth-core's functionality.** forth-core has no
`patches/` or `files/` directory, so the resolver finds nothing to apply and
proceeds as if the package list were empty. This was verified directly
(`meson setup ... -DCUSTOM_PKG=packages/forth-core`, then grepped the
resulting shadow tree for forth-core content: none found) — it is not a
theoretical concern.

**If you want to use this system today**, author a new package from
scratch via the **Authoring Workflow** below. **If you need forth-core
working**, it needs to be migrated to the `patches/`+`files/` convention
first (materialize each of its whole-file overrides, run `refresh`, move
its genuinely-new source files under `files/`) — that migration has not
been done and is out of scope for this document to walk through.

---

## Quick Start

A package directory contains exactly two subdirectories — **nothing else is
read**. There is no `meson.build` inside a package, and nothing to declare:

```
packages/my-pkg/
├── patches/
│   └── 010-keyboard.c.patch     # a git diff against src/c47/keyboard.c
└── files/
    └── my_module.c              # a brand-new file, no upstream counterpart
```

1. Create the package directory and populate `patches/`/`files/` — normally
   via the **materialize-and-refresh workflow** below, not by hand.
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

There is no classification step and nothing to declare: whether a given
change belongs under `patches/` or `files/` is inferred purely from whether
its mirrored path exists under `src/c47/` — and the resolver enforces this
at configure time (see **Fatal Checks** below), so getting it backwards is
a loud, named error, not a silent misfile.

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
3. Each patch applies on top of the previous patch's output (`git apply -3`
   against a freshly materialized copy of the current upstream file).
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

You never write `.patch` files by hand. Edit a fully materialized real file
(full compiler/LSP context), then re-derive the patch set for the whole
package in one step:

```
cp src/c47/keyboard.c packages/my-pkg/keyboard.c   # materialize
# ... edit packages/my-pkg/keyboard.c freely — any kind of change ...
python3 tools/pkg_patch_refresh.py packages/my-pkg  # regenerate patches/
```

`refresh` scans every materialized file directly under the package
directory (excluding `patches/` and `files/` themselves) and, for each that
mirrors a real upstream path:

- differs from upstream → writes/overwrites `patches/<NNN>-<rel>.patch`
  (ordinal reused from any existing patch for that rel, including a manual
  rename — so refresh never fights an explicit cross-package ordering
  choice you made by hand);
- identical to upstream (a reverted edit) → deletes any existing patch for
  that rel, so `patches/` never accumulates stale output;
- has no upstream counterpart at all → left alone and reported — a
  genuinely new file belongs directly under `files/<rel>`, placed there by
  you, not auto-detected by `refresh`.

The materialized working copy is a local authoring convenience — only
`patches/` and `files/` are meant to be committed. `pkg_patch_refresh.py`
has no libclang or other AST-parsing dependency; it's plain `git diff`.

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
way.** Edit only your package's materialized working copies under
`packages/`, then re-run `refresh`.

A patched file's shadow entry is a **real file** (never a symlink) holding
the applied result — editing it directly is harmless-but-pointless (it's
regenerated on every reconfigure) but still isn't the right place to work;
edit the materialized copy and refresh instead.

---

## Rebuilding — When Reconfigure Is Needed

### Normal Edits (Symlink Mode — Default on Linux)

Unpatched upstream files are symlinked into the shadow tree; editing
upstream propagates automatically, no reconfigure needed. A **patched**
file's shadow entry is a real, applied-result file — editing your package's
materialized copy requires re-running `refresh` and reconfiguring to
re-apply the patch into the shadow tree.

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

Usage: `make sim CUSTOM_PKG=packages/my-pkg`

### `pkg_build PKG=<dir>` — Building a Distributable Package

The sole sanctioned way to produce a distributable package artifact:

```
make pkg_build PKG=packages/my-pkg
```

This runs, in order: `make clean`; `make test CUSTOM_PKG=<dir>` (a failing
test suite stops here — no artifact is produced); `refresh` against `<dir>`
(so any un-refreshed materialized edits are captured); then assembles a zip
containing **only** `<dir>/patches/*` and `<dir>/files/*` (materialized
working copies and anything else in the package directory are excluded) at
`pkg_dist/<pkg-name>.zip`.

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
