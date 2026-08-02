# PCLI-1 — Add the short `./package` entry point

Implement only this packet. Read `AGENTS.md` first and create a todo item for
each file, each subcommand, tests, and the final report. Do not read
`DESIGN.md`, `DESIGN-HISTORY.md`, or large C files.

## Scope

Create:

- `package`
- `tools/test_package_cli.py`

Do not edit any other file. Do not edit generated `patches/` or `files/`.
Do not commit or manipulate Git state.

## Required interface

`package` is an executable Python-shebang file at repository root. Users invoke
it as `./package`; do not add a shell alias and do not require
`python3 tools/...`.

Keep the entry point under 220 physical lines. It may add the repository's
`tools/` directory to `sys.path` and import existing functions from
`pkg_patch_refresh`.

Implement:

```text
./package refresh PACKAGE
./package materialize PACKAGE REL
./package rebase PACKAGE --onto REF
./package build PACKAGE
./package audit PACKAGE
```

Package resolution rules:

- Accept `forth-core` or `packages/forth-core`.
- Reject absolute paths, `..`, empty names, and paths outside `packages/`.
- Require `.refresh-manifest.json`.
- Return the normalized repository-relative path `packages/<name>`.
- Resolve the repository root from the executable's own path, not `$PWD`.

Dispatch rules:

- `refresh` calls `pkg_patch_refresh.refresh`.
- `materialize` calls `pkg_patch_refresh.materialize`.
- `rebase` calls `pkg_patch_refresh.rebase_base`.
- `build` executes exactly the package's `build-test.sh`.
- `audit` executes `design-docs/<name>/design-audit.sh` and gives a clear
  nonzero error if it does not exist.
- Preserve the existing Python command lines; do not change their `main()`
  functions in this packet.
- Propagate nonzero exits and print exceptions as one-line `error:` messages,
  without tracebacks for expected user errors.

Match the existing refresh CLI's useful success reporting for written patches,
files, merged paths, and conflicts. Do not duplicate refresh/rebase algorithms.

## Tests

`tools/test_package_cli.py` must use `unittest` and subprocesses. It may inspect
the real repository but must not run a mutating command against
`packages/forth-core`.

Cover:

- `./package --help`;
- every subcommand's `--help`;
- invocation from a directory other than repository root;
- both accepted package spellings;
- rejection of absolute, traversal, missing, and non-package paths;
- missing audit/build script diagnostics;
- executable bit on `package`;
- importing `package` does not run a command.

Use temporary synthetic package directories for commands that would mutate.

Run exactly:

```text
python3 tools/test_package_cli.py
```

Do not run the firmware gate in this packet: the recorded package base and
caller source tree are intentionally mismatched until integration support
lands.

## Stop conditions

Stop and report if an existing refresh function cannot be called without
changing its public behavior, or if testing would require modifying the real
package. Report files changed, test exit/result, and surprises.

