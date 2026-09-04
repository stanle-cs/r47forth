# R47 Forth and package-manager release tree

This branch contains the package system and the Forth package. It also
contains the files that build both packages. Design docs and the
development history are in the working repo.

Licence: GPL-3.0-only, same as c43. Full text in `COPYING` at the root of
this branch.

I generated the patches against upstream c43 release `00.109.04.00b0`
(commit `057b62fc0`). A newer upstream will probably build too. The
resolver checks every target file before it applies a patch. If upstream
moved a file, configure stops and reports the moved file.

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
The build files plus tools/ are the package system, **v0.4**, and any
package can ride on them. `packages/forth-core/` is versioned separately:
the Forth-core baseline is **v0.3**, and the current Forth-core hotfix is
**v0.3.1**. A Forth release does not imply a new package-manager release.
Package-manager v0.4 remains the compatible tooling release.

## Forth core v0.3.1 hotfix

- The interactive console now advertises its terminal controls directly:
  `ENTER=SPACE  R/S=RUN`.
- ENTER inserts a literal token separator in a live Forth console. R/S runs
  the completed line. This lets calculator-key arithmetic such as
  `3 ENTER 4 ENTER + R/S` evaluates as a Forth line.
- The existing keys-first input, retained transcript, output words, FHIST
  history, program-mode folding, FWRD browsing, and global-word assignment
  remain part of the Forth core. Package-manager v0.4 is unchanged.

Leave the test files in place. The first three Python files are meson
tests. `make pkg_build` runs `make test`. Removing a test file stops the
build.

The tests run without an upstream tree, so you can check the machinery
straight from this branch:

```
python3 tools/test_pkg_patch_common.py
python3 tools/test_pkg_patch_refresh.py
python3 tools/test_pkg_patch_resolver.py
python3 tools/test_pkg_patch_integrate.py
```

When all four pass, the resolver and refresh tool work. The patch stack
and integrate tool also work.

## Install

```
git clone https://gitlab.com/rpncalculators/c43.git
cd c43
git checkout 00.109.04.00b0
```

Copy this branch's files over that tree. Keep the directory layout. Then
run:

```
make dist_dmcp5r47 CUSTOM_PKG=packages/forth-core
```

src/ stays untouched. If you omit CUSTOM_PKG, the firmware builds
exactly as stock. Use this command for the simulator build:
`make sim CUSTOM_PKG=packages/forth-core`.

## Refresh before you build

The build reads only `packages/forth-core/patches/` and `files/`. This
branch already contains the generated files, so the first build works.
After you edit the adjacent working copies, run:

```
python3 tools/pkg_patch_refresh.py packages/forth-core
```

Run this command before every build. Without refresh, the firmware
contains the previous source.

## Typing a Forth line

The interactive console opens in calculator-key mode. ENTER inserts a
literal space. R/S runs the current line.
`3 ENTER 4 ENTER + R/S` evaluates to `7`. Press ALPHA inside the line to
type word names. Press ALPHA again to return to calculator keys. In key
mode, function keys type their own names. For example, pressing SIN
inserts `SIN`. Digits work normally. STO and RCL request their parameter.
They then insert text such as `STO 05` into the line.

EXIT unwinds one level at a time: letters, keys and menus. It then closes
the line.

## Notes

The Forth data stack is the calculator's own RPN stack. It has 4 or 8
levels, depending on your stack setting. Deeper values spill to memory.
They return as the word unwinds, so deep recursion works with either
setting. A native C47 function cannot run while values are spilled. The
line stops with a full-stack message.

This is a hobby project that replaces the firmware on your calculator.
Back up your state first, and flash at your own risk.
