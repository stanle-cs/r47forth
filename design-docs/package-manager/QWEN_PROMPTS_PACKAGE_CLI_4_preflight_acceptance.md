# PCLI-4 — Rebase preflight, status, documentation, and acceptance

Implement only this final packet after PCLI-1 through PCLI-3 are green. Read
`AGENTS.md` and the package CLI plan, create the todo list, and stay within the
named files.

## Scope

Edit:

- `package`
- `tools/pkg_patch_refresh.py`
- `tools/test_pkg_patch_refresh.py`
- `tools/test_package_cli.py`
- `tools/test_pkg_patch_integrate.py`
- `AGENTS.md`

Create:

- `PACKAGE-MANAGER.md`

Do not edit product C files, generated package output, or Git state.

## Rebase preflight

Before `./package rebase PACKAGE --onto REF` mutates the package:

1. Resolve `REF^{commit}`.
2. Compare the Git tree objects `HEAD:src/c47` and `REF:src/c47`.
3. Detect tracked or untracked changes under the caller's `src/c47`.
4. If either source condition differs, print a warning containing:

```text
the package can be rebased, but it is not buildable in this checkout
use: ./package integrate PACKAGE --onto REF
```

The warning does not block rebase. Return the mismatch facts from a small
testable helper in `pkg_patch_refresh.py`; keep the legacy CLI compatible.

## Status command

Add:

```text
./package status PACKAGE [--onto REF]
```

Report:

- normalized package;
- manifest base SHA;
- caller `HEAD`;
- optional target SHA;
- whether manifest-base `src/c47` equals caller `HEAD:src/c47`;
- whether caller `src/c47` is dirty;
- whether generated `patches/` or `files/` differ in Git;
- whether conflict markers exist in package working files;
- final `locally buildable: yes|no`;
- the exact `./package integrate ...` recommendation when no.

Status is read-only and returns nonzero only for malformed state, not merely
because integration is needed.

## Documentation

`PACKAGE-MANAGER.md` is the concise public command reference. Explain:

- name versus `packages/name`;
- refresh/materialize/rebase/build/audit/status;
- integration session layout and lifecycle;
- conflict resolution followed by `resume`;
- default cleanup and `--keep`;
- caller immutability;
- absence of promotion/commit/branch changes;
- continued availability of legacy Python entry points.

Update only command examples in `AGENTS.md` that tell users to invoke
`pkg_patch_refresh.py`; preserve all behavioral rules.

## Acceptance

Tests must cover preflight tree comparison, dirty `src/c47`, status output,
legacy CLI compatibility, and all prior integration tests.

Run:

```text
python3 tools/test_pkg_patch_refresh.py
python3 tools/test_package_cli.py
python3 tools/test_pkg_patch_integrate.py
./package --help
./package status forth-core --onto upstream/master
```

Then run:

```text
./package integrate forth-core --onto upstream/master --no-build
```

In the current repository this may correctly stop at repository conflicts.
If so, verify the caller status is byte-for-byte unchanged, report the retained
session and resume command, and do not resolve product conflicts in this
tooling packet. If it reaches `complete`, verify cleanup occurred.

Do not run `build-test.sh` in the mismatched caller checkout. A real firmware
gate is run by a resumed integration session only after its repository
conflicts have been explicitly resolved.

## Final report

Report the public commands, files changed, exact test results, real integration
session outcome, caller-immutability evidence, and any surprise. Do not commit.

