# R47 Forth — release tree

Everything needed to build R47/C47 firmware with the Forth package, and nothing
else. No development history, no design documents, no prompts.

Built against upstream **c43 `b8f79e486`**. Later upstream revisions will
probably work, but the patches were generated against that commit and the
resolver checks the target of every one of them, so a moved line fails loudly
at configure time rather than silently.

## What is here

```
Makefile  meson.build  meson_options.txt     modified upstream build files
tools/resolve_c47_src.py                     configure-time overlay resolver
tools/pkg_patch_refresh.py                   authoring tool: working area -> patches/+files/
tools/pkg_patch_apply.py  pkg_patch_common.py
tools/test_pkg_patch_{common,refresh,resolver}.py
custom_package/README.md                     the package system, in full
packages/forth-core/patches/                 14 diffs against upstream files
packages/forth-core/files/                   13 new sources
```

The three `test_pkg_patch_*.py` files are required, not extras: `meson.build`
declares them as tests unconditionally, so without them `make test` fails on a
missing file — and `make pkg_build` runs `make test`.

They are self-contained, so you can check the overlay machinery straight out of
this branch, before copying anything anywhere and without an upstream tree:

```
python3 tools/test_pkg_patch_common.py
python3 tools/test_pkg_patch_refresh.py
python3 tools/test_pkg_patch_resolver.py
```

Three passes means the resolver, the refresh tool and the patch stack all work.

The two halves are independent. `tools/` + the three build files are the
**custom package system** (v0.3) and will carry any package. `packages/forth-core/`
is one package that happens to use it.

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
firmware back, byte for byte.

For the simulator instead:

```
make sim CUSTOM_PKG=packages/forth-core
```

## One thing that will catch you out

The build reads only `packages/forth-core/patches/` and `files/`. It never reads
the flat working area beside them. That does not matter here — this tree ships
the generated output already — but it matters the moment you edit anything:
run `python3 tools/pkg_patch_refresh.py packages/forth-core` first, or the build
succeeds with no error and quietly compiles the previous version of your code.

## Notes

The Forth data stack is the calculator's own RPN stack, so it is 4 or 8 levels
deep depending on the stack size setting. Recursive words hold one operand per
level and want the 8-level setting; at 4 they will report a full stack rather
than return a wrong answer.

This is a hobby project that replaces the firmware on your calculator. Keep a
backup of your state and flash at your own risk.
