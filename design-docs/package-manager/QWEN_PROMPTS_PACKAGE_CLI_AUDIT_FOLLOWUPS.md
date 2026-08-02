# PCLI-5 — Audit follow-ups for repeatable upstream integration

Implement only this packet. Read `AGENTS.md`, write the required todo list
before editing, and do not read `packages/forth-core/DESIGN.md`,
`DESIGN-HISTORY.md`, or any large C file.

The package has already been migrated and gated at upstream
`1b4ff8e4308f2b79c031cb6625eabd5e2fe213d4`. This is a tooling-hardening
packet, not another product migration. Do not edit product C/H files,
`src/`, generated package `patches/` or `files/`, Git state, or the package
base commit.

## Current fixes that must be preserved

Two defects found by real integrations are already fixed. Retain them and
their regression coverage:

1. Integration sessions are created beside the source repository so the
   worktree uses the same executable-bit/filesystem semantics. Do not return
   to a process-wide temporary directory.
2. The package snapshot and restore path includes
   `.refresh-manifest.json`, `.pkgignore`, and `build-test.sh`. A repeated
   migration must use the caller's current uncommitted manifest/base and gate,
   not the stale committed copies in the detached worktree.

## Scope

Edit:

- `package`
- `tools/pkg_patch_integrate.py`
- `tools/test_package_cli.py`
- `tools/test_pkg_patch_integrate.py`
- `PACKAGE-MANAGER.md`
- `design-docs/forth-core/design-audit.sh`

Create:

- `tools/pkg_patch_cli.py`

Do not add promotion, commits, branch updates, automatic conflict resolution,
or product-warning cleanup.

## 1. Restore the short entry point

The root `package` file is currently about 421 physical lines despite PCLI-1's
hard limit of fewer than 220.

- Move parser construction, status reporting, and command dispatch into
  `tools/pkg_patch_cli.py`.
- Keep `package` as an executable Python-shebang entry point that adds
  `tools/` to `sys.path`, imports one `main` function, and exits with its
  result.
- `package` must remain below 220 physical lines; prefer below 80.
- Preserve every public command, option, exit code, and invocation through
  `./package`. Do not require users to call `python3 tools/...`.
- Add a test that fails when the root entry point reaches 220 lines.

## 2. Preserve visible build output and allow retry

The integration engine currently captures the real gate, discards output on
success, and strands a session permanently in `build-failed`.

- Execute exactly `packages/<name>/build-test.sh` with the integration
  worktree as cwd.
- Stream its stdout and stderr to the caller in normal order. Do not hide the
  two required success banners.
- Also save combined output as `build-test.log` in the validated session
  directory so retained failures are diagnosable.
- Record the exit code in `session.json`.
- A resumed `build-failed` session reruns the exact gate. It may transition to
  `complete` after a successful retry.
- Retain a failed session regardless of `--keep`.
- Synthetic tests must prove success output is visible, failure output and
  exit 42 propagate, the log is preserved, and a repaired gate succeeds on
  resume.

Do not weaken the firmware gate by recognizing banners in place of its exit
status. The package's `build-test.sh` owns that policy.

## 3. Make cleanup fail closed

Cleanup currently ignores `git worktree remove` failure and then recursively
removes the session anyway.

- Validate that the session directory's real parent is exactly the source
  repository's real parent and that its basename has the exact
  `<repo-name>-package-integrate-` prefix.
- Validate its expected top-level entries before deletion.
- Require the worktree to be registered under the source repository and
  contained by the session.
- Run `git worktree remove --force`.
- If removal fails, retain the entire session, record `cleanup-failed`, return
  nonzero, and print Git's diagnostic plus one safe next action.
- Only after successful Git removal may the code remove the validated session
  directory. Do not use `ignore_errors=True`.
- Prune stale worktree registration only after successful removal.
- Tests must inject removal and directory-deletion failures and prove no
  broader or partial deletion occurs.

## 4. Harden metadata, paths, snapshots, and merge classification

- Both the root resolver and reusable integration API must realpath-contain a
  package under `<source_repo>/packages/`. Reject package-directory symlinks
  that escape it.
- Validate `target_sha` as exactly 40 lowercase hexadecimal characters and
  require `git cat-file -e <sha>^{commit}` to succeed.
- Store a deterministic snapshot digest in `session.json`, covering relative
  paths, regular-file modes, and bytes.
- Verify the digest before every snapshot restore. Mutation produces
  `snapshot-invalid`, retains the session, and performs no worktree/package
  deletion.
- A nonzero `git merge` is `repo-conflict` only when Git reports at least one
  unmerged path. Otherwise record a distinct fatal phase, preserve stdout and
  stderr, retain the session, and return nonzero.
- Add tests for an escaping package symlink, non-hex/missing commit SHA,
  snapshot mutation, fatal merge with zero conflicts, and an ordinary merge
  conflict.

## 5. Correct status and audit semantics

`locally buildable` currently ignores package conflict markers and treats some
failed Git probes as a clean result.

- `locally buildable: yes` requires all probes to succeed, matching
  manifest/HEAD `src/c47` trees, clean caller `src/c47`, and no package
  conflict markers.
- A failed Git/status/tree probe is malformed state: print the failed probe
  and return nonzero rather than converting it to `no` or `clean`.
- Keep “generated output differs from Git” informational. Uncommitted,
  correctly refreshed output is not stale merely because it differs from the
  last commit.
- In `design-audit.sh`, verify generated patch/file hashes against the
  manifest. Report two separate facts:
  - synchronized with working area/manifest;
  - differs from Git.
- Count only a manifest/hash mismatch as an audit finding. A synchronized but
  uncommitted migration must not make `./package audit` red.
- Add CLI tests for marker-bearing status, failed Git probes, synchronized
  uncommitted generated output, and genuinely stale generated output.

## Documentation

Update `PACKAGE-MANAGER.md` to describe:

- streamed and retained build logs;
- retrying `build-failed`;
- `cleanup-failed`, `snapshot-invalid`, and fatal-merge behavior;
- the difference between synchronized generated output and uncommitted output;
- the fact that integration still does not promote or change branch history.

## Acceptance

Run in this order:

```text
python3 tools/test_package_cli.py
python3 tools/test_pkg_patch_integrate.py
python3 tools/test_pkg_patch_refresh.py
test "$(wc -l < package)" -lt 220
./package --help
./package status forth-core --onto 1b4ff8e4308f2b79c031cb6625eabd5e2fe213d4
./package audit forth-core
```

Expected:

- every Python test passes;
- root `package` is below 220 lines;
- status is marker-free and reports the exact target/base;
- the audit retains the 14-override / 612-added-line footprint and reports no
  stale generated output when manifest hashes match;
- no integration session or registered worktree is leaked by tests.

Do not run a new upstream migration, fetch, or firmware build in this packet.
Do not commit.

## Stop and report

Stop instead of guessing if:

- preserving existing CLI behavior conflicts with the line cap;
- cleanup safety would require deleting a path that fails any containment or
  registration check;
- a test requires weakening snapshot integrity or caller immutability;
- the audit cannot distinguish manifest mismatch from an ordinary uncommitted
  migration without changing the manifest format.

Report files changed, exact test counts, CLI line count, focused diffs, and any
remaining decision.
