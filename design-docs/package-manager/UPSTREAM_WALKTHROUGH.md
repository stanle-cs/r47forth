# Walkthrough: `./package upstream` Command Implementation

## Changes Made

### 1. Core Engine: [`tools/pkg_patch_upstream.py`](file:///home/stan/c43/tools/pkg_patch_upstream.py)
Implemented the end-to-end upstreaming engine:
* **Package Parsing & Validation**:
  `parse_package_list` accepts single or comma-separated lists of package names, supporting both bare names and `packages/` prefix (e.g. `undo-history` or `packages/pretty-print,packages/pretty-print-extra`).
* **Isolated Worktree Allocation**:
  Creates an isolated detached git worktree (`.package-sessions/upstream-<session_id>/`) checked out at the target upstream commit (defaulting to `upstream/master`). The caller's working tree, index, and branch are never touched.
* **Sibling-Root & Upstream Mapping**:
  Transfers package files (`files/`) to their respective upstream target:
  * `files/testSuite/*` $\rightarrow$ `src/testSuite/*`
  * `files/*` $\rightarrow$ `src/c47/*`
* **Ordinal 3-Way Patch Application**:
  Applies package patches (`patches/*.patch`) in numerical order via `git apply -3` and scans for conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`).
* **Automated Meson Registration**:
  `register_c47_meson_sources` parses `c47_src = files(...)` in [`src/c47/meson.build`](file:///home/stan/c43/src/c47/meson.build), automatically inserting all new C source files in sorted alphabetical order with valid Meson syntax.
* **Native Build & Test Verification**:
  Unless `--no-build` is set, configures and builds `build.upstream-sim` without package manager overlays and runs `./build.upstream-sim/c47-headless -T`.
* **Squashed Commit & Branch Management**:
  Stages `src/`, creates a squashed commit with an informative summary, and points the caller's local branch `refs/heads/<branch>` to the new commit.
* **Push & GitLab MR Integration**:
  Supports `--push` and `--mr`. When `--mr` is specified, attempts non-interactive MR creation via `glab mr create`. If `glab` is unauthenticated or unavailable, cleanly falls back to providing the direct GitLab web creation URL.
* **Session Cleanup**:
  Cleans up the temporary worktree and session folder automatically unless `--keep` is passed or an unhandled failure occurs.

### 2. CLI Interface & Entry Point
* [`tools/pkg_patch_cli.py`](file:///home/stan/c43/tools/pkg_patch_cli.py): Added the `upstream` subcommand parser and wired `_cmd_upstream` with `UpstreamError` exception handling.
* [`package`](file:///home/stan/c43/package): Updated root launcher docstring to document `./package upstream`.

### 3. Automated Test Suite: [`tools/test_pkg_patch_upstream.py`](file:///home/stan/c43/tools/test_pkg_patch_upstream.py)
Created 11 unit tests using synthetic Git repositories covering:
* Package list parsing (bare, prefixed, multi-package, whitespace, duplicates, invalid names).
* Meson source registration (insertion, alphabetical sorting, indentation, idempotency).
* Single-package folding with patch application and new file placement.
* Multi-package sequential folding.
* Sibling root (`src/testSuite/`) mapping.
* Patch conflict detection and error reporting.
* GitLab web URL generation.

---

## Verification Results

### Automated Unit Tests
```bash
$ python3 -m unittest tools/test_pkg_patch_upstream.py
Ran 11 tests in 0.156s
OK
```

### Live Test: Single Package (`undo-history`)
```bash
$ ./package upstream undo-history --onto upstream/master --branch test-mr-undo --no-build
==> Creating upstream session on upstream/master (dd448f0a1193)
==> Session directory: /home/stan/c43-package-upstream-s6aaau6b
==> Folding packages/undo-history...
==> Registered in src/c47/meson.build: browsers/historyBrowser.c, undoHistory.c
==> Build and test suite skipped (--no-build).
==> Committing squashed upstream changes to branch test-mr-undo...
==> Commit created: e7e3520110bc feat(undo-history): fold undo-history into upstream tree
==> Local branch created/updated: test-mr-undo
==> GitLab Merge Request URL:
    https://gitlab.com/rpncalculators/c43/-/merge_requests/new?merge_request[source_branch]=test-mr-undo&merge_request[target_branch]=master
```
Inspecting the resulting commit:
* All 24 modified files were strictly under `src/c47/` and `src/testSuite/`.
* `src/c47/meson.build` had `browsers/historyBrowser.c` and `undoHistory.c` added.
* No `packages/` or `.refresh-manifest.json` files were present in the commit.

### Live Test: Multi-Package (`pretty-print,pretty-print-extra`)
```bash
$ ./package upstream pretty-print,pretty-print-extra --onto upstream/master --branch test-mr-pp-combo --no-build
==> Creating upstream session on upstream/master (dd448f0a1193)
==> Session directory: /home/stan/c43-package-upstream-t9p3d_uo
==> Folding packages/pretty-print...
==> Folding packages/pretty-print-extra...
==> Registered in src/c47/meson.build: browsers/prettyBrowser.c, prettyCapture.c, prettyEquation.c, prettyExtraTest.c, prettyFormula.c, prettyInfix.c, prettyLayout.c, prettyTest.c, prettyValue.c, prettyVisual.c
==> Build and test suite skipped (--no-build).
==> Committing squashed upstream changes to branch test-mr-pp-combo...
==> Commit created: e4b2be5944f6 feat(pretty-print, pretty-print-extra): fold packages into upstream tree
==> Local branch created/updated: test-mr-pp-combo
==> GitLab Merge Request URL:
    https://gitlab.com/rpncalculators/c43/-/merge_requests/new?merge_request[source_branch]=test-mr-pp-combo&merge_request[target_branch]=master
```
Both packages merged into a single clean commit with all 10 new C sources wired into `src/c47/meson.build`.
Temporary test branches were deleted after verification.
