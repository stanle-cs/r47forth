# Implementation Plan: `./package upstream` Command

## Goal Description
Implement the `./package upstream` command in the package manager. This command automates folding one or more packages (such as [`undo-history`](file:///home/stan/c43/packages/undo-history) or [`pretty-print`](file:///home/stan/c43/packages/pretty-print)) into a clean, upstream-compatible branch based on upstream C43/C47 (`upstream/master`), with:
* All changes consolidated into a single clean squashed commit.
* Automated file mapping to `src/c47/` and `src/testSuite/` (supporting `SIBLING_ROOTS`).
* Automated registration of newly introduced `.c` files in [`src/c47/meson.build`](file:///home/stan/c43/src/c47/meson.build).
* Native upstream build and headless test suite verification (`build.sim`).
* Optional push to the GitLab fork (`origin`) and MR creation via `glab mr create` (with web URL fallback).
* Complete caller isolation: the caller's working tree and index are never modified.

---

## User Review Required

> [!IMPORTANT]
> **Caller Working Tree Safety**:
> The command operates entirely within a detached Git worktree (`.package-sessions/upstream-<session-id>/`). Even with uncommitted changes on your current branch (e.g. `pretty-print/stage-pp19`), your working tree remains completely untouched.

> [!NOTE]
> **Upstream Remote Fallback**:
> If `--onto` is not specified, the command will look for `upstream/master`, falling back to `upstream/HEAD`. If neither exists in the repository, it will prompt for an explicit `--onto <REF>`.

---

## Open Questions
None. All prior architectural questions (squashed commit, comma-separated package lists with `packages/` prefix support, and `glab` CLI integration with URL fallback) were confirmed.

---

## Proposed Changes

### Core Package Manager Tooling

Grouped under [`tools/`](file:///home/stan/c43/tools) and top-level entry point [`package`](file:///home/stan/c43/package).

```
tools/
├── pkg_patch_upstream.py      [NEW]
├── test_pkg_patch_upstream.py [NEW]
├── pkg_patch_cli.py           [MODIFY]
└── pkg_patch_common.py        (existing helper reused)
package                        [MODIFY]
```

---

#### [NEW] `tools/pkg_patch_upstream.py`
Core implementation module for the upstream folding session.

Key components:
1. **`parse_package_list(pkg_arg, project_root)`**:
   * Splits comma-separated strings (e.g. `packages/pretty-print,packages/pretty-print-extra` or `pretty-print,pretty-print-extra`).
   * Resolves and validates each package using `_resolve_package()`.
   * Rejects duplicates or missing packages.
2. **`create_upstream_worktree(project_root, session_dir, target_sha)`**:
   * Runs `git worktree add --detach <session_worktree> <target_sha>`.
3. **`fold_packages(project_root, session_worktree, package_dirs)`**:
   * For each package in the specified order:
     * Copies `files/` to `<session_worktree>/src/testSuite/` (if prefixed with `testSuite/`) or `<session_worktree>/src/c47/`.
     * Applies `patches/*.patch` in numerical ordinal order using `git apply -3`.
     * Scans modified files for Git conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`). Raises `UpstreamConflictError` if found.
4. **`register_c47_meson_sources(meson_build_path, new_c_files)`**:
   * Parses `c47_src = files(...)` in `<session_worktree>/src/c47/meson.build`.
   * Inserts newly introduced C sources (under `src/c47/`) in alphabetical order with 2-space indentation.
   * Preserves existing Meson formatting.
5. **`run_upstream_build_test(session_worktree)`**:
   * Executes upstream native build:
     ```bash
     meson setup build.upstream-sim
     ninja -C build.upstream-sim
     ./build.upstream-sim/c47-headless -T
     ```
   * Streams/captures build logs.
6. **`commit_and_branch(session_worktree, project_root, branch_name, title, message)`**:
   * Stages changes: `git add src/`.
   * Commits: `git commit -m "<title>\n\n<message>"`.
   * Updates caller repository branch ref: `git branch -f <branch_name> <commit_sha>`.
7. **`publish_and_mr(project_root, branch_name, remote, push, mr, title, message)`**:
   * If `push` or `mr`: runs `git push -u <remote> <branch_name>`.
   * If `mr`: runs `glab mr create --target-branch master --source-branch <branch_name> --title <title> --description <message> --yes`.
   * Generates and outputs fallback GitLab web creation URL:
     `https://gitlab.com/rpncalculators/c43/-/merge_requests/new?merge_request[source_branch]=<branch_name>`

---

#### [MODIFY] [`tools/pkg_patch_cli.py`](file:///home/stan/c43/tools/pkg_patch_cli.py)
1. Add `upstream` command parser:
   ```python
   sp = subs.add_parser(
       'upstream',
       help='Fold one or more packages into a clean upstream branch for Merge Request',
   )
   sp.add_argument(
       'packages',
       help='Comma-separated package names or paths (e.g. pretty-print,packages/pretty-print-extra)',
   )
   sp.add_argument(
       '--onto', default=None, metavar='REF',
       help='Target upstream commit/branch (default: upstream/master)',
   )
   sp.add_argument(
       '--branch', default=None, metavar='BRANCH',
       help='Local branch name to create (default: mr/<first-package>)',
   )
   sp.add_argument(
       '--remote', default='origin', metavar='REMOTE',
       help='Remote to push to (default: origin)',
   )
   sp.add_argument(
       '--push', action='store_true', default=False,
       help='Push the branch to the remote',
   )
   sp.add_argument(
       '--mr', action='store_true', default=False,
       help='Create a GitLab Merge Request via glab (implies --push)',
   )
   sp.add_argument('--title', default=None, help='Commit & MR title')
   sp.add_argument('--message', default=None, help='Commit & MR description')
   sp.add_argument('--no-build', action='store_true', default=False, help='Skip build & tests')
   sp.add_argument('--keep', action='store_true', default=False, help='Retain worktree session on success')
   sp.set_defaults(func=_cmd_upstream)
   ```
2. Implement `_cmd_upstream(args)` to dispatch to `pkg_patch_upstream.upstream(...)`.

---

#### [MODIFY] [`package`](file:///home/stan/c43/package)
Update the top-level usage docstring:
```python
"""
Usage:
    ./package refresh PACKAGE
    ./package materialize PACKAGE REL
    ./package rebase PACKAGE --onto REF
    ./package build PACKAGE
    ./package audit PACKAGE
    ./package status PACKAGE [--onto REF]
    ./package integrate PACKAGE --onto REF [--keep] [--no-build]
    ./package resume SESSION [--keep] [--no-build]
    ./package upstream PACKAGES [--onto REF] [--branch BRANCH] [--remote REMOTE] [--push] [--mr] [--no-build] [--keep]
"""
```

---

#### [NEW] `tools/test_pkg_patch_upstream.py`
Comprehensive automated unit tests using synthetic repositories:
1. `test_parse_package_list`: Single and multi-package input with/without `packages/` prefix, handling whitespace and rejects invalid/duplicate names.
2. `test_meson_source_registration`: Tests parsing `c47_src = files(...)` and inserting new `.c` files in sorted alphabetical order with exact indentation.
3. `test_fold_single_package`: Creates a synthetic upstream repo and a package with patches and new files; asserts files land in `src/c47/` and `src/testSuite/`, and `packages/` is not present in the resulting tree.
4. `test_fold_multi_package`: Folds two sequential companion packages into a single commit.
5. `test_upstream_conflict_handling`: Tests graceful suspension and reporting when a patch fails 3-way merge against target ref.
6. `test_web_url_generation`: Verifies parsing of SSH/HTTPS git remotes into proper GitLab web MR creation links.

---

## Verification Plan

### Automated Tests
Run unit tests for both existing package tooling and the new upstream module:
```bash
python3 -m unittest tools/test_pkg_patch_upstream.py
python3 -m unittest tools/test_package_cli.py
python3 -m unittest tools/test_pkg_patch_integrate.py
```

### Manual Verification
1. Verify CLI help output:
   ```bash
   ./package upstream --help
   ```
2. Run a dry/test run on an existing package (e.g. `undo-history` with `--no-build --keep`) targeting `upstream/master` into a test branch:
   ```bash
   ./package upstream undo-history --onto upstream/master --branch test-mr-undo --no-build --keep
   ```
3. Inspect `test-mr-undo` git log and diff to ensure:
   * Only `src/` files and `src/c47/meson.build` were touched.
   * No `packages/` or `.refresh-manifest.json` files exist.
   * `git diff upstream/master..test-mr-undo` contains the clean expected changes.
4. Clean up test branch after verification:
   ```bash
   git branch -D test-mr-undo
   ```
