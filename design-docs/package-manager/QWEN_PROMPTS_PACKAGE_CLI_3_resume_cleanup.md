# PCLI-3 — Resume, rebase, build, and clean an integration session

Implement only this packet after PCLI-2 is green. Read `AGENTS.md` and the
package CLI plan, then create the required todo list. Do not read large design
or C files.

## Scope

Edit only:

- `package`
- `tools/pkg_patch_integrate.py`
- `tools/test_pkg_patch_integrate.py`
- `tools/test_package_cli.py`

Do not commit or manipulate the caller's Git state.

## CLI

Add:

```text
./package resume SESSION [--keep] [--no-build]
```

`integrate` must automatically call the same resume engine when its initial
repository merge reaches `repo-ready`.

## Resume state machine

Accept only a session whose metadata, source repository, package path, target
SHA, worktree registration, and containment checks all pass.

Phases:

```text
repo-conflict -> repo-ready -> package-copy -> package-rebase
package-conflict -> package-refresh -> build -> complete
```

Rules:

- For `repo-conflict`, refuse to continue while Git reports unmerged paths or
  any recorded conflict file still contains conflict markers. Once clean,
  stage only the recorded conflict paths; never commit.
- Replace only the isolated worktree's normalized package directory with the
  immutable snapshot. Validate containment before deletion. Never delete or
  rewrite the source package directory.
- Call the imported `rebase_base` and `refresh` functions with the integration
  worktree as `project_root`; do not spawn their legacy Python CLI.
- If package conflict markers remain, record `package-conflict`, retain the
  session, return nonzero, and print the single resume command.
- On a resumed `package-conflict`, require all package markers to be gone,
  refresh, and continue. Never guess a conflict resolution.
- Unless `--no-build`, execute exactly
  `packages/<name>/build-test.sh` with the integration worktree as cwd. Capture
  its exit and preserve its normal output.
- `--keep` on either command overrides the stored value for that invocation.
- On success without `--keep`, remove the registered worktree using Git, prune
  its registration, then remove only the validated session directory.
- Retain every failed session.

There is no `promote`, commit, branch update, merge completion, or copy-back
operation in this packet.

## Tests

Extend synthetic tests to cover:

- refusing unresolved or marker-bearing repository conflicts;
- successful resume after a test resolves the recorded conflict;
- snapshot copied exactly while generated directories are regenerated;
- package conflict retained and successfully resumed after marker removal;
- fake `build-test.sh` success and failure propagation;
- `--no-build`;
- success cleanup and `--keep`;
- refusing forged metadata, unregistered worktrees, escaped paths, and cleanup
  targets outside the session;
- caller branch, index, tracked bytes, and untracked bytes unchanged through
  every path.

The fake gate must print the two real success banners so result propagation is
tested without compiling firmware.

Run:

```text
python3 tools/test_package_cli.py
python3 tools/test_pkg_patch_integrate.py
```

Do not run the real firmware gate from the caller checkout.

## Stop conditions

Stop rather than weakening containment or conflict checks. After two failed
repair attempts, leave the session/test fixture intact and report a debugger
handoff with failure output and relevant focused diffs.

