# Package Manager — Command Reference & Integration Lifecycle

## Commands

### `./package refresh PACKAGE`

Regenerate `patches/` and `files/` from the working area.

Scans every file under `packages/<name>/`, classifies it against the
upstream tree at the package's `base_commit`, and writes:
- `patches/<NNN>-<rel>.patch` for files that mirror an upstream path and differ.
- `files/<rel>` for files with no upstream counterpart (copied verbatim).

Resolves `.refresh-manifest.json` if present.

### `./package materialize PACKAGE REL`

Materialize an upstream file into the working area at the base commit.

Use this when you need to start overriding an upstream file that is not yet
in the working area. Writes `packages/<name>/<rel>` as it existed at the
package's `base_commit`, so you can then edit it and run `refresh`.

### `./package rebase PACKAGE --onto REF`

Rebase the package's base commit onto a new target ref.

**Preflight check (non-blocking):** before rebasing, compares the Git tree
objects `HEAD:src/c47` and `REF:src/c47`, and checks for local changes under
`src/c47`. If the tree differs or `src/c47` is dirty, the command prints a
warning to stderr recommending `./package integrate PACKAGE --onto REF`
instead, but **still proceeds** with the rebase.

Resolves `REF^{commit}`, runs `refresh` at the old base, then at the new
base, and applies any upstream deltas to working-area files using
`git-apply --reject`.

### `./package build PACKAGE`

Build the project with the package's patches injected.

Runs `refresh`, then `meson setup --reconfigure`, then `ninja`. Verifies the
build exits 0.

### `./package audit PACKAGE`

Audit the package for drift, footprint, and generated-output integrity.

Runs `design-docs/forth-core/design-audit.sh` which checks:

- **Upstream footprint** — override file count and added-line budget
- **Hunks with no Forth content** — baseline-tracked review items
- **No-op churn** — whitespace-only changes vs upstream
- **Inline blocks** — contiguous added blocks in upstream files
- **Package-owned allocations** — lifetime review for new allocations
- **Generated output** — verifies patch/file hashes against manifest:
  - "synchronized with manifest" when hashes match (clean)
  - "differs from Git" is informational — uncommitted refresh is OK
  - Only a manifest/hash mismatch is an audit finding
- **Stray shippable content** — files that would ship as firmware
- **DESIGN.md citations** — cited source paths still exist

A synchronized but uncommitted migration (manifest hashes match, Git has
older copies) does NOT make the audit red. Only a genuine manifest mismatch
is a finding.

### `./package status PACKAGE [--onto REF]`

Show the package's buildability status.

Reports:
- **package** — bare package name
- **manifest base** — SHA prefix from `.refresh-manifest.json` (or "none")
- **caller HEAD** — current HEAD SHA prefix
- **target** — optional target ref and SHA prefix
- **manifest-base src/c47 == caller HEAD:src/c47** — whether the
  manifest's base commit has the same `src/c47` tree as the caller's HEAD
- **caller src/c47 dirty** — whether `src/c47` has tracked or untracked
  local changes
- **generated patches/ or files/ differ in Git** — whether the generated
  output has uncommitted changes
- **conflict markers in working files** — whether any working-area file
  contains `<<<<<<<`, `=======`, or `>>>>>>>` markers
- **locally buildable** — `yes` only when `src/c47` matches the manifest
  base AND is clean

If not locally buildable, prints a recommendation to run
`./package integrate PACKAGE --onto REF`.

Exits 1 if the manifest has no base commit.

### `./package integrate PACKAGE --onto REF [--keep] [--no-build]`

Full integration session — the safe way to bring a package's changes to a
new target.

**This command never modifies the caller's working tree or index.** It
creates a dedicated session directory beside the source repository and
works entirely within it.

#### Session lifecycle

1. **`initializing`** — Create session dir, snapshot package working area
   (including `.refresh-manifest.json`, `.pkgignore`, `build-test.sh`),
   compute deterministic snapshot digest, create detached worktree.

2. **`merging`** — Merge target ref into worktree. If Git reports unmerged
   paths, transition to `repo-conflict`. If Git fails with no unmerged
   paths (fatal error), transition to `merge-fatal` with stdout/stderr
   preserved.

3. **`repo-conflict`** — Repository-level merge conflict. Resolve markers
   in the worktree, then resume.

4. **`merge-fatal`** — Non-conflict merge failure. This state is terminal
   because the target was not merged. Inspect the preserved diagnostics,
   fix the Git failure, and start a new integration session.

5. **`repo-ready`** — Merge succeeded. Resume to continue.

6. **`package-copy`** — Snapshot is restored into worktree's package dir.
   Resume to continue.

7. **`package-rebase`** — Package rebase against target. If conflicts,
   transition to `package-conflict`.

8. **`package-conflict`** — Package-level conflict. Resolve markers, then
   resume.

9. **`package-refresh`** — Refresh completed. Resume to build.

10. **`build`** — Run `packages/<name>/build-test.sh` with output streamed
    to caller. Combined output saved as `build-test.log` in session dir.
    Exit code recorded in `session.json`.

11. **`build-failed`** — Build failed. Retained regardless of `--keep`.
    Repair the worktree and resume to retry.

12. **`complete`** — Clean up session (unless `--keep`).

#### Build output

The build gate (`build-test.sh`) output is streamed to the caller in real
time, so the two required success banners are visible. Combined stdout and
stderr are also saved as `build-test.log` in the session directory for
retained failures.

The package's `build-test.sh` owns the success/failure policy (exit code).
The integration engine does not parse banners — it trusts the exit code.

#### Retrying build-failed

A `build-failed` session can be resumed to retry the gate. The snapshot is
NOT re-copied — only the build is re-run. Repair the worktree's package
directory (e.g., fix a build script or resolve a compilation error), then:

```
./package resume SESSION
```

If the retry succeeds, the session transitions to `complete`. Failed retries
remain in `build-failed` and may be retried again.

#### Snapshot integrity

The package snapshot is hashed at creation time. Before every snapshot
restore, the stored digest is verified. If the snapshot was mutated, the
session transitions to `snapshot-invalid`, is retained, and no worktree or
package deletion occurs.

#### Cleanup safety

Cleanup validates the session before deletion:
- Session directory's parent must match the source repository's parent.
- Session basename must have the `<repo>-package-integrate-` prefix.
- Expected top-level entries (`session.json`, `package-snapshot`, `worktree`)
  must exist and no unexpected top-level entries may be present.
- Worktree must be registered under the source repository and contained by
  the session.

If `git worktree remove --force` fails, the entire session is retained,
`cleanup-failed` is recorded, and Git's diagnostic is printed with a safe
next action. The session directory is only removed after successful Git
removal.

#### Integration limitations

Integration does not promote the package, update branch history, or commit
to the caller's repository. It produces a validated worktree for inspection
and testing.

#### Resuming

```
./package resume SESSION [--keep] [--no-build]
```

Continues from the last completed phase. After resolving conflicts in a
paused session, run `resume` to continue through build.

Exit codes:
- 0 — success
- 1 — package conflict (resolve markers, then resume)
- Nonzero — build failed (exit code from `build-test.sh`; fix, then resume)

### `./package resume SESSION [--keep] [--no-build]`

Resume a paused integration session from its current phase.

## Caller Immutability

The `integrate` and `resume` commands are designed to **never touch the
caller's working tree or index**. All work happens in a dedicated temp
directory. The caller's state is only *read* (HEAD SHA, tree objects, file
contents) to seed the session and produce the final diff.

This means:
- Your uncommitted work is safe.
- You can run multiple integration sessions in parallel.
- The session directory is self-contained and inspectable.

## Session Directory Layout

Sessions are created beside the source repository (not in a system temp
directory) so the worktree uses the same filesystem semantics.

```
<repo-parent>/<repo>-package-integrate-<id>/
  session.json          — recorded state, phase, config, snapshot digest
  package-snapshot/     — normalized package working area snapshot
  worktree/             — detached Git worktree (merge target)
  build-test.log        — combined build output (after build phase)
```

The snapshot includes the package's control files (`.refresh-manifest.json`,
`.pkgignore`, `build-test.sh`) so a resumed migration uses the caller's
current uncommitted base and gate, not stale committed copies.

## .pkgignore

Each package directory may contain a `.pkgignore` file listing patterns of
files to exclude from the working area classification. Patterns:
- `*.md` — matches any `.md` file at any depth
- `build-test.sh` — matches exact basename
- `docs/` — matches directory (trailing slash)
- `tools/foo.py` — anchored to package root (contains `/`)

Lines starting with `#` are comments.
