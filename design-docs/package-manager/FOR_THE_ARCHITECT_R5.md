# R5 tooling — design defects for the architect

Three findings require policy decisions before Qwen can implement them. They
are intentionally absent from `QWEN_PROMPTS_R5_tooling.md`.

## A1 — Corrupt manifest silently destroys the pinned epoch

**Claim.** `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` BP-1 explicitly keeps
`load_manifest`'s “missing/corrupt-tolerant behavior”; the tool docstring says a
missing or corrupt manifest is treated as empty.

**Code.** `load_manifest` catches JSON/OSError and returns empty `patches` and
`files`. `ensure_base_commit` then records current HEAD. Because the returned
hash maps are empty, even legacy-output detection cannot say the old base was
lost.

**Verified failure.** Starting from a valid base-A manifest and patch, moving
upstream to B, then truncating the manifest JSON and running refresh produced:

- `repinned_to_new_head=True`;
- only a generic “patch exists but was not recorded” warning;
- a patch containing `-line4_UPSTREAM` / `+line4`, i.e. silently reverting B's
  upstream change.

This recreates the exact base-pinning bug BP-1..BP-7 were meant to remove.
`save_manifest` also writes the sole manifest in place, so process interruption
can create the corrupt state without a human edit.

**Decision needed.** Approve both of these as one manifest-integrity contract:

1. missing manifest may initialize a new package, but malformed/unreadable JSON
   is fatal and must never be treated as a fresh package;
2. save through a same-directory temporary file, flush/fsync, then `os.replace`
   so the prior valid manifest survives interruption.

If approved, add a Qwen task covering `load_manifest`, `save_manifest`, and
tests that prove corrupt JSON leaves patch, working copy, and base untouched.

## A2 — Gitignored working areas and mandatory refresh are incompatible

**Claim.** The spec's “The flat working area is `.gitignore`d” section and
`packages/.gitignore` say only generated patches/files/manifest are committed.
`build-test.sh` must refresh before every normal build, and refresh defines a
missing working file as deletion of its generated output.

**Verified failure.** In a temporary repository using the real
`packages/.gitignore`:

1. `packages/test-pkg/new_source.c` was ignored by the `*/*` rule;
2. refresh generated and Git staged only
   `files/new_source.c` plus `.refresh-manifest.json`;
3. a clean clone contained the generated source but not the working source;
4. the clone's first refresh returned `files_removed=['new_source.c']` and
   deleted the compiler-visible source.

This already affects every future forth-core working file that was not tracked
before the blanket ignore landed. Existing forth-core sources happen to survive
only because Git ignore rules do not retroactively untrack them.

**Decision needed.** Choose one coherent source of truth:

- **Recommended:** track the flat working area as well as generated output.
  Remove the blanket working-area ignore, retain `.pkgignore` solely for
  classification, and update the spec/README. This makes refresh reproducible
  after clone at the cost of committed redundancy.
- Or keep working files local, but then normal build cannot unconditionally
  refresh. That alternative needs an explicit deletion protocol plus a
  generated-output-to-working-area rehydration design; simply skipping refresh
  reopens the already-proven silent-stale build bug.

Do not ask Qwen to choose between these.

## A3 — `.pkgignore` can still remove compiled C with no dynamic warning

**Claim.** The spec explicitly accepts silent removal of ignored `.c` files to
avoid a declaration list. The current forth-core patterns (`*.md`, `*.txt`,
`build-test.sh`) are safe and do not match source.

**Verified behavior.** A brand-new `new_source.c` plus `.pkgignore` pattern
`*.c` produced no files/ entry and `warnings=[]`. The compiler therefore never
sees it.

**Cheap option that is not a per-file declaration.** The resolver already has
the compilation predicate `rel.endswith('.c')`. Refresh can reuse that same
predicate to emit a loud warning whenever `.pkgignore` suppresses a C file.
This preserves intentional ignores and the no-declaration design while making
the high-risk case non-silent. A similar warning for a working path that mirrors
any real upstream file would catch ignored overrides without an extension list.

**Decision needed.** Approve dynamic warnings, or explicitly retain fully
silent behavior. This is not in the Qwen list because the current spec calls
the silence accepted.

## Confirmed clean / no architect action

- A transient patch/hash mismatch heals once: one warning on the first refresh,
  none on the second. The drift warning does not persist forever.
- Mode headers do not reach compiler output, but their host-dependent hashes
  are a tooling determinism defect; R5-1 specifies the content-only fix.
