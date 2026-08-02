# Package CLI and integration-worktree implementation plan

This series adds one short public entry point for the package system and a
safe way to test a package against a newer upstream tree without modifying the
caller's branch or dirty worktree.

Use the four implementation packets in order. Give each packet to a fresh Qwen
session together with the repository's `AGENTS.md`. The packets are deliberately
small and do not require reading `DESIGN.md`, `DESIGN-HISTORY.md`, or any large
C source.

## Locked architecture

The public interface is the executable at repository root:

```text
./package refresh forth-core
./package materialize forth-core programming/manage.c
./package rebase forth-core --onto upstream/master
./package integrate forth-core --onto upstream/master
./package resume /tmp/c43-package-integrate-...
./package build forth-core
./package audit forth-core
./package status forth-core
```

`package` is a small Python-shebang dispatcher with no `.py` suffix. Users
invoke it directly; the existing `tools/pkg_patch_*.py` files remain importable
implementation modules and their legacy command lines remain compatible.

An integration session contains:

```text
SESSION/
  session.json
  package-snapshot/
  worktree/
```

The snapshot is immutable and excludes generated `patches/`, `files/`, and
`__pycache__`. The detached worktree starts at the caller's `HEAD`, merges the
requested target without committing, receives the snapshot, rebases and
refreshes the package, and optionally runs its sanctioned `build-test.sh`.

## Safety invariants

- The caller's index, branch, tracked files, and untracked files are never
  changed.
- Never stash, reset, restore, checkout caller files, commit, move a branch, or
  edit `src/`.
- Repository conflicts and package conflicts retain the session and return
  nonzero with one exact resume command.
- Success removes the session unless `--keep` was supplied.
- Cleanup operates only on a validated registered worktree and its validated
  session directory.
- There is no automatic promotion in this series. Adoption of an upstream
  merge/rebase remains an explicit human Git operation.
- The resolver continues to consume the source checkout it is given. It never
  silently substitutes the package manifest's base commit.

## Ordered packets

| Packet | Outcome | Principal files |
|---|---|---|
| PCLI-1 | Short root CLI for existing operations | `package`, CLI tests |
| PCLI-2 | Safe session creation and conflict reporting | integration module/tests |
| PCLI-3 | Resume, package rebase, build, and cleanup lifecycle | integration module/tests |
| PCLI-4 | Rebase preflight, status, documentation, full tooling acceptance | CLI, refresh module, docs/tests |

## Current repository fact used by acceptance

At authoring time `forth-core` records base
`3c84890a1190511abb80ebf62166bb7879c3a2a2`, while this branch's `src/c47`
checkout is older. The normal resolver therefore cannot build the rebased
package in the caller worktree. The integration command must diagnose or
isolate that condition; it must not conceal it.

## Completion criteria

- All public operations are reachable through `./package`.
- A dirty caller worktree can create an isolated integration session without
  changing its before/after status.
- Both repository and package conflicts are retained and resumable.
- A conflict-free synthetic session reaches refresh and its fake package gate.
- Rebase warns before mutation when the target `src/c47` tree differs from the
  caller checkout.
- `status` explains whether the package is locally buildable.
- Legacy Python entry points still work.

