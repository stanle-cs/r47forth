# R47 Forth release tree

The build tree only: the package system, the Forth package, and how to install
both. Design documents and development history stay in the working repo.

Licence: GPL-3.0-only, same as c43. Full text in `COPYING` at the root of this
branch.

Built against upstream c43 `b8f79e486`. Newer upstream will often work, but
the patches were generated against that commit, and the resolver checks every
target before it applies anything. If a file has moved, configure stops with
an error.

## What is here

```
Makefile  meson.build  meson_options.txt     modified upstream build files
tools/resolve_c47_src.py                     configure-time overlay resolver
tools/pkg_patch_refresh.py                   authoring: working area -> patches/+files/
tools/pkg_patch_apply.py  pkg_patch_common.py
tools/test_pkg_patch_{common,refresh,resolver}.py
custom_package/README.md                     the package system, in full
COPYING                                      GPL-3.0 licence text
packages/forth-core/patches/                 14 diffs against upstream files
packages/forth-core/files/                   13 new sources
```

The two halves are independent. `tools/` and the three build files are the
custom package system (v0.3) and will carry any package. `packages/forth-core/`
is one package that happens to use it.

Keep the `test_pkg_patch_*.py` files: `meson.build` declares them as tests
unconditionally, so without them `make test` dies on a missing file, and
`make pkg_build` runs `make test`.

They don't need an upstream tree, so you can check the machinery straight
from this branch before touching anything else:

```
python3 tools/test_pkg_patch_common.py
python3 tools/test_pkg_patch_refresh.py
python3 tools/test_pkg_patch_resolver.py
```

Three passes means the resolver, the refresh tool and the patch stack all work.

## Install

```
git clone https://gitlab.com/rpncalculators/c43.git
cd c43
git checkout b8f79e486
```

Copy the files above over that tree, then:

```
make dist_dmcp5r47 CUSTOM_PKG=packages/forth-core
```

Nothing in `src/` is modified. Build without `CUSTOM_PKG` and you get vanilla
firmware back, byte for byte. For the simulator it's
`make sim CUSTOM_PKG=packages/forth-core`.

## Refresh before you build

The build compiles from `packages/forth-core/patches/` and `files/`. The flat
working area next to them is never read. This tree already ships the generated
output, so nothing bites until you edit something. After that, run

```
python3 tools/pkg_patch_refresh.py packages/forth-core
```

first, or the build will cheerfully compile the version from before your
edit.

## Notes

The Forth data stack is the calculator's own RPN stack, so it's 4 or 8 levels
deep depending on the stack size setting. Recursive words hold one operand per
level and want 8. At 4 they report a full stack.

Hobby project that replaces the firmware on your calculator. Keep a backup of
your state and flash at your own risk.
