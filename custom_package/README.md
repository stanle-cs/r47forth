# Custom Packages — README

External custom packages let you extend C47 (add sources, override upstream
files) **without modifying any `src/` file**.  Every new line of meson
configuration lives inside `if custom_pkg_list != []` blocks in the top-level
`meson.build` — a vanilla build (`-DCUSTOM_PKG` unset or empty) is
byte-for-byte identical.

---

## Quick Start

1. Create a package directory, e.g. `packages/my-pkg/`.
2. Write a `meson.build` that declares what the package provides:

   ```meson
   # Override upstream src/c47/foo.c with your copy
   pkg_override_sources = ['foo.c']

   # Override upstream src/c47/bar.h with your copy
   pkg_override_headers = ['bar.h']

   # Add brand-new source files compiled into the build
   pkg_custom_sources   = files('my_module.c', 'my_helper.c')
   ```

3. Configure with the package:

   ```
   meson setup build.sim --buildtype=custom -DRASPBERRY=false \
       -DDECNUMBER_FASTMUL=true -DCUSTOM_PKG=packages/my-pkg --reconfigure
   ninja -C build.sim
   ```

## Variable Reference

| Variable              | Type   | Purpose                                                        |
|-----------------------|--------|----------------------------------------------------------------|
| `pkg_override_sources`| array  | Filenames (relative to `src/c47/`) to replace with your copies |
| `pkg_override_headers`| array  | Header filenames (relative to `src/c47/`) to replace            |
| `pkg_custom_sources`  | files() | New `.c` files your package adds to the build                  |

All three default to empty.  If none are declared, meson emits a warning:

```
WARNING: Package packages/my-pkg declared none of pkg_override_sources,
         pkg_override_headers, or pkg_custom_sources
```

**Scope warning:** your package's `meson.build` executes in the *top-level*
meson scope (via `subdir()`).  A typo such as `pkg_override_source` (missing
`_s`) creates an unused variable — no error, no override, green build.  Use
the exact variable names above.

## Override Semantics

- Your override file must live at `packages/my-pkg/<relative-path>`, matching
  the path relative to `src/c47/`.  E.g. `pkg_override_sources = ['foo.c']`
  expects `packages/my-pkg/foo.c`.
- The override replaces the upstream file entirely in the shadow tree.
- If the override is byte-identical to upstream, a warning is emitted ("dead
  shadow") — the file is still shadowed, masking future upstream changes.
- A missing override file or a spec that doesn't match any upstream source is
  a **fatal error** (configure fails).

---

## Shadow Tree — Safety & Editing

### Sentinel Gate

The resolver creates a *shadow tree* at `build.sim/custom_pkg_shadow/` — a
mirror of `src/c47/` with your overrides layered on top.  On each reconfigure,
it wipes and rebuilds this tree.

**Delete-safety:** before wiping, the resolver checks for the sentinel file
`DO_NOT_EDIT_shadow_tree.txt` at the shadow root.  If the directory exists, is
non-empty, and **lacks** this sentinel, the resolver **refuses to delete it**
and aborts with a clear error.  This prevents accidental destruction of
directories that were not created by the resolver.

### DO NOT Edit the Shadow Tree

The shadow tree contains symlinks (or copies) to upstream source files.  Editing
a file through the shadow edits the **upstream** file — violating the core
promise that packages never modify `src/`.

IDEs and language servers that follow `compile_commands.json` will resolve
"Go to Definition" into the shadow tree.  **Do not edit files you reach this
way.**  Edit only your package's own files under `packages/`.

---

## items.c Override — Generator Stub Requirement

If your package overrides `src/c47/items.c` (via
`pkg_override_sources = ['items.c', …]`), the override file must include
**stub definitions** for every new handler referenced by table rows you add,
inside the generator guard block:

```c
#if defined(GENERATE_CATALOGS) || defined(GENERATE_TESTPGMS)
  void fnMyNewHandler(uint16_t unusedButMandatoryParameter) {}
  /* … one stub per new handler … */
#endif
```

Upstream `items.c` provides stubs for all built-in handlers in this block.
The catalog generator (`generateCatalogs`) and test-program generator
(`generateTestPgms`) compile `items.c` with `GENERATE_CATALOGS` or
`GENERATE_TESTPGMS` defined — real handler bodies are replaced by empty stubs.
If your override adds new table rows with new handler names but doesn't provide
matching stubs, the generator builds will fail to link with "undefined
reference" errors.

**Rule:** an `items.c` override that adds table rows *must* add matching stubs
inside the `GENERATE_*` block.  The `runFunction`/dispatch path at the bottom
of `items.c` is outside the `GENERATE_*` guard and does not need stubs.

---

## Rebuilding — When Reconfigure Is Needed

### Normal Edits (Symlink Mode — Default on Linux)

When the shadow tree uses symlinks (default on Linux), editing your package
source files or upstream files propagates to the shadow tree automatically.
A bare `ninja -C build.sim` picks up the changes. **No reconfigure needed.**

### Copy Mode (Windows / `CUSTOM_PKG_SHADOW_COPY=1`)

If symlink creation fails (common on Windows without privilege) or you set
`CUSTOM_PKG_SHADOW_COPY=1`, the resolver falls back to file copies.  A loud
warning is printed at configure time:

```
WARNING: symlink not available, using file copies — bare ninja will NOT see
         source edits; reconfigure required after each change
```

In copy mode, the shadow tree is a snapshot.  Editing a source file does **not**
update the copy.  You must reconfigure after each change:

```
meson setup build.sim --reconfigure -DCUSTOM_PKG=packages/my-pkg …
ninja -C build.sim
```

### Structural Upstream Changes

If upstream `src/c47/meson.build` changes (new source files added, include dirs
changed), the shadow tree won't reflect the change until the next reconfigure.
This is benign — the `-Isrc/c47` fallback include path serves the new header —
until you override that file.  Reconfigure to pick up structural changes.

### Changing `-DCUSTOM_PKG`

Changing the `-DCUSTOM_PKG` option value dirties meson's coredata.  The next
`ninja` run will trigger regeneration, which re-executes the full `meson.build`
including the resolver's `run_command`.  The shadow tree is rebuilt
automatically.  Explicit `--reconfigure` is not strictly required but is good
practice:

```
meson setup build.sim --reconfigure -DCUSTOM_PKG=packages/my-pkg …
ninja -C build.sim
```

**Do not `rm -rf build.sim`.**  Deleting the build directory is unnecessary
cargo-cult.  Use `--reconfigure` instead.  The only time full deletion helps is
when meson's internal state is corrupted (rare).

---

## Makefile Targets

The top-level `Makefile` propagates `CUSTOM_PKG` through these targets:

| Target       | Propagates CUSTOM_PKG? | Notes                                    |
|-------------|------------------------|------------------------------------------|
| `sim`       | Yes (via `build.sim`)  | Standard simulator build                 |
| `simr47`    | Yes (via `build.sim`)  | R47 simulator build                      |
| `testPgms`  | Yes (via `build.sim`)  | Skips `res/testPgms/` copy when CUSTOM_PKG is set — prevents package builds from contaminating the shared test corpus |
| `test_asan` | Yes                    | ASan-instrumented test run               |
| `test`      | Yes (via `build.sim`)  | Full clean build + test                  |

Usage: `make sim CUSTOM_PKG=packages/my-pkg`

---

## Limitations

- **Documentation:** doxygen (`docs/code`) only scans `src/c47/`.  Package
  sources and overrides do not appear in generated documentation.
- **Generator source lists:** the catalog and test-program generators compile
  from the shadow tree.  If your package adds items via an `items.c` override,
  the generators will include them (provided the stub requirement above is met).
