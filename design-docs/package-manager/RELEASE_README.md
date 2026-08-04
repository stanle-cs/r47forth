# R47 Forth release tree

This branch is the build tree and nothing else: the package system, the
Forth package, and what you need to build both. Design docs and the
development history live in the working repo.

Licence: GPL-3.0-only, same as c43. Full text in `COPYING` at the root of
this branch.

I generated the patches against upstream c43 release `00.109.04.00b0`
(commit `057b62fc0`). A newer upstream will probably build too, since
the resolver checks every target file before it applies anything, but
if upstream moved a file, configure stops and says so.

## What is here

```
Makefile  meson.build  meson_options.txt     modified upstream build files
tools/resolve_c47_src.py                     configure-time overlay resolver
tools/pkg_patch_refresh.py                   authoring: working area -> patches/+files/
tools/pkg_patch_apply.py  pkg_patch_common.py
tools/pkg_patch_cli.py  pkg_patch_integrate.py  package     package commands
tools/test_pkg_patch_{common,refresh,resolver,integrate}.py
design-docs/package-manager/README.md                     the package system, in full
COPYING                                      GPL-3.0 licence text
packages/forth-core/patches/                 16 diffs against upstream files
packages/forth-core/files/                   19 new sources
```

The package system and the Forth package don't depend on each other.
The build files plus tools/ are the package system, v0.4, and any
package can ride on them. packages/forth-core/ is the one I built with
it.

Leave the test files in place. meson.build declares the first three as
tests no matter what, and `make pkg_build` runs `make test`, so removing
them breaks the build on a missing file.

The tests run without an upstream tree, so you can check the machinery
straight from this branch:

```
python3 tools/test_pkg_patch_common.py
python3 tools/test_pkg_patch_refresh.py
python3 tools/test_pkg_patch_resolver.py
python3 tools/test_pkg_patch_integrate.py
```

When all four pass, the resolver, the refresh tool, the patch stack and
the integrate tool are all working.

## Install

```
git clone https://gitlab.com/rpncalculators/c43.git
cd c43
git checkout 00.109.04.00b0
```

Copy this branch's files over that tree, keeping the layout, then:

```
make dist_dmcp5r47 CUSTOM_PKG=packages/forth-core
```

src/ stays untouched. Leave CUSTOM_PKG off and the firmware builds
exactly as stock. The simulator build is
`make sim CUSTOM_PKG=packages/forth-core`.

## Refresh before you build

The build only reads `packages/forth-core/patches/` and `files/`, and
this branch already ships them generated, so the first build just works.
Once you start editing the working copies next to them, run

```
python3 tools/pkg_patch_refresh.py packages/forth-core
```

before every build, or your edit won't be in the firmware you flash.

## Typing a Forth line

A Forth line opens in alpha for typing word names. If you want the calculator keys instead, press ALPHA inside the line and the keyboard goes back to normal. Now the function keys type their own names. Press SIN and you get `SIN` in the line. Digits work like you expect. STO and RCL still ask for their parameter the normal way, then the whole thing ends up in the line as text, like `STO 05`. You don't have to put spaces around any of it, the spaces get put in for you. ALPHA again brings the letters back.

R/S ends the line and puts a STOP step after it. EXIT goes back one step at a time, keys to letters to menus, then it closes the line.

Mostly it saves keys. SIN is one press instead of three letters. By hand x² needs numlock and the superscript arrow before the 2. The key is one press.

## Notes

The Forth data stack is the calculator's own RPN stack, 4 or 8 levels
depending on your stack setting. Anything deeper spills to memory and
drains back as the word unwinds, so deep recursion works at either
setting. A native C47 function can't run while values are spilled, the
line stops with a full-stack message instead.

This is a hobby project that replaces the firmware on your calculator.
Back up your state first, and flash at your own risk.
