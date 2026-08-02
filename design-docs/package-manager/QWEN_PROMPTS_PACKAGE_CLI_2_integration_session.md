# PCLI-2 — Create isolated integration sessions

Implement only this packet after PCLI-1 is green. Read `AGENTS.md` and
`QWEN_PROMPTS_PACKAGE_CLI_PLAN.md`, then write the packet todo list. Never read
the large design or C files.

## Scope

Create:

- `tools/pkg_patch_integrate.py`
- `tools/test_pkg_patch_integrate.py`

Edit:

- `package`

No other files. No commits, stashes, resets, caller checkouts, or edits under
`src/`.

## CLI

Add:

```text
./package integrate PACKAGE --onto REF [--keep] [--no-build]
```

This packet implements session creation and repository-merge classification.
PCLI-3 completes resume, package rebase, build, and cleanup.

## Session creation

Implement a reusable module API; do not put Git orchestration into the root
dispatcher.

1. Resolve `REF^{commit}` before creating anything.
2. Create a unique session directory with `tempfile.mkdtemp` and prefix
   `<repo-name>-package-integrate-`.
3. Create `package-snapshot/` by copying the normalized package working area.
   Exclude `patches/`, `files/`, and every `__pycache__`. Reject symlinks that
   escape the package.
4. Write `session.json` atomically. It records schema version, source repo,
   source `HEAD`, package relative path, target ref and resolved SHA, worktree
   path, `keep`, `no_build`, phase, and conflict paths.
5. Run:

```text
git worktree add --detach SESSION/worktree SOURCE_HEAD
git -C SESSION/worktree merge --no-commit --no-ff TARGET_SHA
```

Never commit the merge.

If the merge is clean, record phase `repo-ready`. If it conflicts, record
phase `repo-conflict`, capture only `git diff --name-only --diff-filter=U`,
retain the session regardless of `--keep`, return nonzero, and print exactly
one next action:

```text
./package resume SESSION
```

Print the session path prominently. Do not copy the package snapshot into the
worktree yet.

## Caller immutability

Record the caller's branch, `HEAD`, porcelain-v1 status bytes, and index-tree
identity before the operation. Verify them again after success or failure.
Treat any change as a fatal internal error and retain the session.

## Tests

Build small synthetic Git repositories in temporary directories. Do not use
the real repository as a mutation fixture.

Cover:

- clean target merge produces `repo-ready`;
- conflicting merge produces `repo-conflict`, records exact paths, and prints
  the resume command;
- dirty tracked and untracked caller files are byte-identical afterward;
- snapshot exclusion rules;
- invalid ref/package fails before worktree creation;
- metadata is valid and atomically replaced;
- two sessions do not collide.

Run:

```text
python3 tools/test_package_cli.py
python3 tools/test_pkg_patch_integrate.py
```

Do not run the firmware gate in this packet.

## Stop conditions

Stop if safe caller immutability would require stash/reset/checkout, or if Git
cannot create a detached worktree from a dirty caller without changing it.
Report exact commands, test results, and retained temporary paths.

