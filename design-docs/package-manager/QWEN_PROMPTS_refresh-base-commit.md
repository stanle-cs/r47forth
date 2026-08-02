# Qwen Implementation Prompts — Refresh Base Pinning

Task: make `refresh` diff against a package's **recorded base commit**
instead of live upstream (design record: `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md`, section "Refresh Base Pinning — diff against
the recorded base commit", decisions BP-1 … BP-8, APPROVED 2026-07-14).

Execute the prompts below **in order, one at a time, each as a fresh
session** — the implementing model has no memory between prompts; each
prompt restates everything it needs. Do not skip ahead. Do not combine
prompts. Human reviews between prompts as desired.

Conventions used in every prompt (restated inside each anyway):
- Branch: `pkgmgr/refresh-base-commit`. Confirm before touching anything.
- Never commit, never push — prepare changes for human review only.
- Fast gate first (`python3 tools/test_pkg_patch_refresh.py`), then the
  full gate (`make test`) green before reporting done.

---

## PROMPT 1 — Manifest `base_commit` plumbing + initialization

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if it prints anything else, STOP and report, touch nothing.

Read, in this order:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — only the section titled
   "Refresh Base Pinning — diff against the recorded base commit",
   specifically decisions **BP-1** and **BP-3**.
2. `tools/pkg_patch_refresh.py` — whole file (~430 lines).
3. `tools/test_pkg_patch_refresh.py` — the `_TempProject` helper class at
   the top (lines ~33–90) and skim the test names.

### What to implement (this prompt implements BP-1 and BP-3 only)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

In `tools/pkg_patch_refresh.py`:

1. Add a helper (place it after `save_manifest`):

```python
def resolve_head_commit(project_root):
    """Full 40-char SHA of HEAD in project_root's repository.
    RuntimeError if unresolvable (not a git repo / unborn HEAD)."""
    r = run(['git', 'rev-parse', 'HEAD'], cwd=project_root)
    if r.returncode != 0 or not r.stdout.strip():
        raise RuntimeError(
            'cannot resolve HEAD in {root!r} — refresh needs a git '
            'repository to record the package base commit '
            '(PROPOSED_SPEC_CHANGES.md, BP-3)'.format(root=project_root))
    return r.stdout.strip()
```

2. Add a helper (place it after `resolve_head_commit`):

```python
def ensure_base_commit(manifest, project_root, warnings):
    """Return (base_commit, initialized_bool). If the manifest already
    records a base_commit, return it unchanged. Otherwise record
    HEAD's full SHA into manifest['base_commit'] (BP-3). If generated
    entries already exist (legacy pre-base-pinning package), append a
    loud warning to *warnings* saying the base was ASSUMED to be
    current HEAD and earlier patches were generated against live
    upstream."""
```
   Pseudocode for the body:
   - `existing = manifest.get('base_commit')`; if truthy → return
     `(existing, False)`.
   - `head = resolve_head_commit(project_root)`
   - if `manifest['patches']` or `manifest['files']` is non-empty →
     `warnings.append(...)` with a message containing the words
     `base_commit initialized to current HEAD` and the SHA.
   - `manifest['base_commit'] = head`; return `(head, True)`.

3. In `refresh()`: immediately after `manifest = load_manifest(...)` and
   `warnings = []`, call
   `base_commit, base_initialized = ensure_base_commit(manifest, project_root, warnings)`.
   `base_commit` is unused by the diffing logic **in this prompt** (that
   is a later, separate change) — only recording/persisting happens now.

4. Change the manifest-save condition at the end of `refresh()` from
   `if written or files_written or removed or files_removed:` to also
   save when `base_initialized` is true (otherwise a no-op first refresh
   would forget the base it just chose).

Do NOT change `load_manifest` beyond what exists (absence of
`base_commit` is meaningful — no `setdefault` for it). Do NOT change
`generate_patch`, classification logic, the CLI, or any other file.

### Tests to add (in `tools/test_pkg_patch_refresh.py`, using `_TempProject`)

Each test docstring must name the mutation it catches, as the existing
tests do:

1. `test_first_refresh_records_base_commit` — after one
   `write_upstream` + `write_working` + `refresh()`, the manifest JSON on
   disk contains a `base_commit` equal to
   `git rev-parse HEAD` in the temp project. **Catches:** base never
   recorded (regression to no-base behavior).
2. `test_noop_first_refresh_still_records_base` — working copy identical
   to upstream (refresh writes nothing), yet the manifest is still saved
   with `base_commit`. **Catches:** the save-condition mutation where
   `base_initialized` is dropped from the save guard.
3. `test_existing_base_commit_not_overwritten` — pre-write a manifest
   with `base_commit` set to a fixed fake-but-well-formed SHA (40 hex
   chars), run `refresh()`, assert the manifest still records that exact
   SHA. **Catches:** refresh silently re-pinning the base to HEAD on
   every run (which would reintroduce the live-upstream bug wholesale).
4. `test_legacy_manifest_init_warns` — create a manifest that has a
   `patches` entry recorded but NO `base_commit` (simulating a
   pre-base-pinning package), run `refresh()`, assert some returned
   warning contains `base_commit initialized to current HEAD`.
   **Catches:** silent initialization on legacy packages (BP-3 requires
   it to be loud).

### Gate, then report

Run `python3 tools/test_pkg_patch_refresh.py` — all tests (existing 27 +
yours) must pass. Then run `make test` and confirm it is green. If any
pre-existing test fails, report the failure verbatim and STOP — do not
"fix" pre-existing tests in this prompt.

Do not commit. Do not push. Report what you changed and the test output
summary.

---

## PROMPT 2 — Diff against base content (the core fix)

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo):
`tools/pkg_patch_refresh.py` already records a per-package
`base_commit` in `.refresh-manifest.json` via `ensure_base_commit()`
called at the top of `refresh()`, initialized to HEAD on first run.

Read, in this order:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", decisions **BP-2** and the "Root cause" subsection; also the
   older "Application Mechanism" section for background on why the
   pre-image blob SHA matters.
2. `tools/pkg_patch_refresh.py` — whole file, especially
   `generate_patch()` and the patch-classification branch inside
   `refresh()`.
3. `tools/test_pkg_patch_refresh.py` — `_TempProject` and any test whose
   docstring mentions drift, upstream changes, or the uncommitted gate.

### What to implement (BP-2 only)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

1. Add a helper after `ensure_base_commit`:

```python
def base_file_content(project_root, base_commit, rel):
    """Raw BYTES of src/c47/<rel> as it existed at base_commit, or
    None if the path does not exist at that commit. Bytes, not text —
    the base copy must be byte-exact for the pre-image blob SHA to
    match (BP-2)."""
    r = subprocess.run(
        ['git', 'show', f'{base_commit}:src/c47/{rel}'],
        capture_output=True, cwd=project_root)
    return r.stdout if r.returncode == 0 else None
```
   (Note: this deliberately does NOT use the module's `run()` helper —
   `run()` sets `text=True`, which would newline-translate the content.)

2. Rework `generate_patch`. New signature:

```python
def generate_patch(base_bytes, materialized_path, rel, project_root,
                   base_commit, context=3):
```
   Behavior changes only in where the pre-image comes from:
   - Write `base_bytes` to a temporary file (`tempfile.NamedTemporaryFile
     (delete=False)`, **binary** mode, in the system temp dir), and run
     the existing `git diff --no-index --full-index -U{context}` with the
     temp file as the FIRST path and `materialized_path` as the second.
     Wrap everything after creation in `try/finally` and
     `os.unlink(tmp_path)` in the `finally`.
   - Everything else stays structurally identical: empty diff → return
     `None`; binary-detection RuntimeError; extract the 40-char pre-image
     SHA from the `index` line; `git cat-file -e <sha>` gate; header
     rewriting loop (`diff --git`/`--- a/`/`+++ b/` lines rewritten to
     `src/c47/<rel>` — this loop already works on arbitrary on-disk
     paths and needs no change).
   - Update the cat-file-gate error message: the pre-image is now the
     **base blob**; unresolvability means the base commit's blob is not
     in the local odb (e.g. a shallow clone missing history, or a bogus
     recorded base). Mention `base_commit` (include its value) and
     `PROPOSED_SPEC_CHANGES.md, BP-2` in the message. Keep it a
     RuntimeError.
   - Add `import tempfile` if not already imported.

3. In `refresh()`'s patch-classification branch (the
   `if os.path.isfile(upstream_path):` arm): fetch
   `base_bytes = base_file_content(project_root, base_commit, rel)`.
   **For this prompt only**, if `base_bytes is None`, fall back to
   reading the live upstream file's bytes (`open(upstream_path,'rb')`)
   — a temporary bridge so existing tests that commit upstream once and
   never move it keep passing; the next prompt replaces this fallback
   with the specified fatal errors. Mark the fallback with a comment
   `# TEMPORARY bridge, replaced by BP-4 epoch checks in the next step.`
   Then call the new
   `generate_patch(base_bytes, working_path, rel, project_root,
   base_commit, context=context)`.

### Tests to add

1. `test_patch_ignores_upstream_drift_after_base` — **the core bug
   test.** Sequence: `write_upstream('a.c', V1)` (committed);
   `write_working('a.c', V1 + developer edit)`; `refresh()` (pins base,
   writes patch); now `write_upstream('a.c', V2)` where V2 changes a
   DIFFERENT line than the developer's edit (committed — upstream has
   moved); delete nothing; `refresh()` again. Assert the regenerated
   patch: (a) contains the developer's added line as a `+` line, (b) does
   **not** contain any `-` line carrying V2's changed content (i.e. no
   reversal of upstream's own change), and (c) its `index` line's
   pre-image SHA equals `git rev-parse <base_commit>:src/c47/a.c` in the
   temp repo. **Catches:** the original bug — diffing against live
   upstream bakes an upstream revert into the patch and stamps the wrong
   pre-image blob.
2. `test_reverted_edit_judged_against_base_not_live` — pin base with a
   working copy equal to upstream (or revert it after), then move
   upstream to V2. Working copy == base content but != live content.
   `refresh()` must treat this as a reverted edit: **no** patch present
   afterward. **Catches:** a mutation that compares against live for the
   "has the developer changed anything" decision — which would fabricate
   a patch consisting purely of upstream-revert noise.
3. `test_base_blob_resolvable_gate_message` — hand-write a manifest with
   a syntactically valid but nonexistent `base_commit`
   (40 hex chars, e.g. `'f' * 40`)… `base_file_content` will return None
   for it, hitting this prompt's temporary live fallback, so instead
   test the gate directly: call `generate_patch` with `base_bytes` of
   some content that was **never committed** in the temp repo and assert
   RuntimeError mentioning the base commit value. **Catches:** silently
   dropping the ancestry gate, which is what keeps `git apply -3`
   able to merge later.

### Existing-test triage rule

Run `python3 tools/test_pkg_patch_refresh.py`. If a PRE-EXISTING test now
fails, read its docstring:
- If it explicitly asserts generation against **live** upstream after
  upstream moved, or asserts the uncommitted-upstream RuntimeError path
  through the old signature — update it to the new semantics/signature
  and say so in your report, quoting the old and new assertion.
- Any other pre-existing failure: STOP and report it verbatim. Do not
  patch around it.

### Gate, then report

`python3 tools/test_pkg_patch_refresh.py` fully green, then `make test`
green. Do not commit. Do not push. Report changes, triaged tests, and
test output summary.

---

## PROMPT 3 — Epoch-mismatch fatal checks (added-live / deleted-live)

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo):
`refresh()` in `tools/pkg_patch_refresh.py` records `base_commit` in the
manifest and `generate_patch(base_bytes, working_path, rel, project_root,
base_commit, context)` diffs against base-commit content. The
patch-classification arm currently contains a temporary fallback marked
`# TEMPORARY bridge, replaced by BP-4 epoch checks in the next step.`

Read, in this order:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", decision **BP-4** (including the 2026-07-14 amendment: the
   upstream-deleted case is a fatal stop by explicit human direction).
2. `tools/pkg_patch_refresh.py` — `refresh()` in full.
3. `tools/test_pkg_patch_refresh.py` — tests covering files/
   classification and the uncommitted-upstream gate.

### What to implement (BP-4 only)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

Restructure the per-rel classification inside `refresh()`'s main loop to
this exact decision table (pseudocode):

```
live_exists = os.path.isfile(upstream_path)
base_bytes  = base_file_content(project_root, base_commit, rel)

if live_exists and base_bytes is None:
    # upstream ADDED this file after the package's base
    raise RuntimeError(
        f'{rel}: exists in current upstream but not at the recorded '
        f'base commit {base_commit} — upstream added this file after '
        f'the package base was pinned. Advance the base first: '
        f'python3 tools/pkg_patch_refresh.py <pkgdir> --rebase-base '
        f'(PROPOSED_SPEC_CHANGES.md, BP-4).')
if (not live_exists) and (base_bytes is not None):
    # upstream DELETED this file after the package's base
    raise RuntimeError(
        f'{rel}: existed at the recorded base commit {base_commit} '
        f'but has been deleted from current upstream — stopping so '
        f'you can check whether this package\'s change to it still '
        f'applies at all (PROPOSED_SPEC_CHANGES.md, BP-4, amended '
        f'2026-07-14).')
if live_exists:            # and base_bytes is not None
    ... existing patch-generation path, passing base_bytes ...
else:                      # absent from BOTH live and base
    ... existing files/ copy path, byte-identical to today ...
```

Remove the `# TEMPORARY bridge` fallback entirely — after this prompt,
`base_bytes` is never substituted with live content anywhere.

Note the `--rebase-base` flag named in the first error message does not
exist yet (a later step adds it); the message text is still correct to
write now — it is the sanctioned remedy per the spec.

### Tests to add / update

1. `test_upstream_added_after_base_is_fatal` — pin base via a first
   refresh of file `a.c`; then `write_upstream('b.c', ...)` (committed —
   upstream moved) and `write_working('b.c', <edited copy>)`;
   `refresh()` must raise RuntimeError whose message contains `b.c`,
   the base SHA, and `--rebase-base`. **Catches:** silently diffing a
   post-base file against live (or against nothing), mixing epochs
   within one package.
2. `test_upstream_deleted_after_base_is_fatal` — pin base with `a.c`
   patched; delete `src/c47/a.c` from the temp repo's working tree AND
   commit the deletion; `refresh()` must raise RuntimeError whose message
   contains `a.c` and `deleted`. **Catches:** the pre-amendment silent
   behavior — reclassifying the working copy into `files/` and thereby
   re-adding a file upstream deliberately removed.
3. `test_new_file_absent_from_base_and_live_still_copies` — a working
   file with no counterpart at base or live must still land in
   `files/<rel>` exactly as today. **Catches:** over-broad fatal checks
   breaking the legitimate brand-new-file path.
4. Update (if not already updated in the previous step) any pre-existing
   test asserting the old "upstream file has uncommitted modifications"
   RuntimeError: with base pinning, an upstream file that was never
   committed is simply absent at base while present live, so the
   expected error is now the `test_upstream_added_after_base_is_fatal`
   message shape. Quote old/new assertions in your report.

Existing-test triage rule: same as before — update only tests whose
docstrings assert now-superseded live-diff semantics; STOP and report
verbatim on any other pre-existing failure.

### Gate, then report

`python3 tools/test_pkg_patch_refresh.py` fully green, then `make test`
green. Do not commit. Do not push.

---

## PROMPT 4 — Conflict-marker guard on patch-classified working files

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo):
`refresh()` in `tools/pkg_patch_refresh.py` classifies per the BP-4
decision table (fatal on added-live/deleted-live epoch mismatches) and
diffs patch-classified files against recorded-base content.

Read:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", decision **BP-7**.
2. `tools/pkg_patch_refresh.py` — `refresh()` and `generate_patch()`.
3. `tools/pkg_patch_apply.py` — ONLY the `_CONFLICT_MARKER_RE` regex and
   `scan_conflict_markers()` near the top (~lines 50–72), as the
   reference pattern. Do NOT modify this file.

### What to implement (BP-7 only)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

1. In `tools/pkg_patch_refresh.py`, add a module-level regex and helper
   mirroring the apply-side scan (duplicate them here rather than
   importing from `pkg_patch_apply` — refresh must not grow a dependency
   on the apply module):

```python
_CONFLICT_MARKER_RE = re.compile(r'^(<{7}|={7}|>{7})', re.MULTILINE)

def _working_file_marker_lines(path):
    """1-indexed line numbers of column-0 conflict-marker lines
    (<<<<<<< / ======= / >>>>>>>) in the text file at *path*."""
```
   Body: read the file as text, enumerate lines from 1, collect lines
   matching the regex at position 0 (same semantics as
   `scan_conflict_markers` in `pkg_patch_apply.py`).

2. In `refresh()`'s patch-classified arm (live exists AND base exists),
   BEFORE calling `generate_patch`:

```python
markers = _working_file_marker_lines(working_path)
if markers:
    raise RuntimeError(
        f'{rel}: working copy contains conflict markers at line(s) '
        f'{markers} — resolve them first (unfinished --rebase-base?). '
        f'Refusing to bake conflict markers into a patch '
        f'(PROPOSED_SPEC_CHANGES.md, BP-7).')
```

3. Scope: do NOT apply this scan to files/-classified working files
   (absent from base and live) — BP-7 explicitly excludes them.

### Tests to add

1. `test_marker_in_patch_classified_working_file_is_fatal` — pin base
   for `a.c`; write a working `a.c` containing a line starting with
   `<<<<<<<` at column 0; `refresh()` raises RuntimeError naming `a.c`
   and the line number. **Catches:** shipping a patch that embeds
   merge-conflict markers into the shadow tree (compilable-looking
   garbage caught only much later, or not at all).
2. `test_marker_midline_is_not_fatal` — a working `a.c` where `<<<<<<<`
   appears only preceded by other characters (e.g. inside a string
   literal, not at column 0) refreshes normally. **Catches:** an
   over-eager scan (regex missing the `^` anchor / MULTILINE mistake)
   that rejects legitimate code.
3. `test_marker_in_new_file_is_allowed` — a brand-new working file
   (absent at base and live) whose content includes a column-0
   `=======` line still copies into `files/` without error. **Catches:**
   the scan leaking outside its BP-7 scope onto files/-classified
   entries.

### Gate, then report

`python3 tools/test_pkg_patch_refresh.py` fully green, then `make test`
green. Do not commit. Do not push.

---

## PROMPT 5 — CLI rework + `--materialize`

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo):
`tools/pkg_patch_refresh.py` has `ensure_base_commit()`,
`base_file_content()`, base-pinned diffing, BP-4 epoch fatals, and the
BP-7 marker guard. Its CLI is still the original bare
`pkg_patch_refresh.py <pkgdir>`.

Read:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", decisions **BP-5** and **BP-3**.
2. `tools/pkg_patch_refresh.py` — `main()`, `refresh()`, and the
   manifest helpers.
3. `Makefile` line 248 (`python3 tools/pkg_patch_refresh.py $(PKG)`) —
   this invocation form MUST keep working unchanged; do not edit the
   Makefile.

### What to implement (BP-5, plus the argparse groundwork)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

1. Add a function:

```python
def materialize(pkgdir, rel, project_root):
    """BP-5: write src/c47/<rel> AS OF THE RECORDED BASE COMMIT into
    the flat working area at <pkgdir>/<rel>. Initializes the base to
    HEAD (BP-3) if the package has none yet — saving the manifest in
    that case. Returns the base commit used."""
```
   Pseudocode:
   - `pkgdir_abs = os.path.join(project_root, pkgdir)`;
     `os.makedirs(pkgdir_abs, exist_ok=True)` (materialize may be the
     very first touch of a new package).
   - `manifest = load_manifest(pkgdir_abs)`; `warnings = []`;
     `base_commit, initialized = ensure_base_commit(manifest,
     project_root, warnings)`; if `initialized`:
     `save_manifest(pkgdir_abs, manifest)` (unconditional — there is no
     later save on this path). Print each warning to stderr prefixed
     `warning: `.
   - `working_path = os.path.join(pkgdir_abs, *rel.split('/'))`; if it
     already exists → RuntimeError
     `f'{rel}: working copy already exists at {working_path} — refusing
     to overwrite your edits (delete it first if you really want a
     fresh copy).'`
   - `content = base_file_content(project_root, base_commit, rel)`; if
     None → RuntimeError
     `f'{rel}: does not exist at the recorded base commit
     {base_commit}. If upstream added it recently, advance the base
     first (--rebase-base).'`
   - `os.makedirs(os.path.dirname(working_path), exist_ok=True)` (only
     if there is a dirname); write `content` in **binary** mode.
   - `print(f'materialized {rel} at base {base_commit[:12]} -> {pkgdir}/{rel}')`
   - return `base_commit`.

2. Rework `main()` to argparse:
   - Positional `pkgdir` (required). Strip trailing `/` as today.
   - `--materialize REL` — optional.
   - Mutually exclusive with `--rebase-base` (which is added by the NEXT
     prompt; define the mutually-exclusive group now with only
     `--materialize` in it, or simply add `--materialize` alone —
     your choice, but do not implement `--rebase-base` behavior here).
   - No flags → exactly today's refresh flow, byte-identical output
     format (the `wrote patches/...` / `removed ...` / `no changes...`
     lines and `error: ...` on RuntimeError, exit 1). The Makefile's
     `python3 tools/pkg_patch_refresh.py $(PKG)` form must behave
     identically to before.
   - `--materialize REL` → call `materialize(...)`; on RuntimeError
     print `error: {e}` to stderr and exit 1 (same convention as
     refresh).

### Tests to add

1. `test_materialize_copies_base_not_live` — commit `a.c` as V1
   (base will pin here), run `materialize(pkgdir, 'a.c', root)` once to
   pin base — actually pin the base FIRST via a refresh or a prior
   materialize, then commit upstream V2, then materialize a second file
   that existed at base with different content at V2… Simplest exact
   sequence: commit `a.c`=V1 and `b.c`=V1b in one commit; run
   `materialize` for `a.c` (pins base at this commit); commit
   `b.c`=V2b (upstream moves); run `materialize` for `b.c`; assert the
   working `b.c` content equals **V1b** (base), not V2b (live).
   **Catches:** materializing from the live tree under an older base —
   the mirror-image bug named in BP-5, which would bake upstream's own
   base→live changes into the next patch as developer-authored.
2. `test_materialize_refuses_overwrite` — materialize `a.c`, edit the
   working copy, materialize `a.c` again → RuntimeError mentioning
   `refusing to overwrite`. **Catches:** silent destruction of developer
   edits.
3. `test_materialize_absent_at_base_fatal` — commit base without `c.c`;
   pin base; commit `c.c` upstream afterward; `materialize('c.c')` →
   RuntimeError mentioning the base SHA and `--rebase-base`.
   **Catches:** falling back to live content for post-base files.
4. `test_cli_bare_pkgdir_still_refreshes` — invoke the tool as a
   subprocess (`sys.executable tools/pkg_patch_refresh.py <pkgdir>`
   with cwd/temp-project arranged appropriately, or by calling `main()`
   with patched `sys.argv`) and assert a normal refresh happens (patch
   written). **Catches:** argparse rework breaking the Makefile's
   existing single-positional invocation (Makefile:248).

(If invoking as a subprocess is awkward because `main()` hardcodes
`project_root` relative to the tools dir, test via `main()` with
monkeypatched `sys.argv` and a monkeypatched project-root resolution —
state in your report which route you took.)

### Gate, then report

`python3 tools/test_pkg_patch_refresh.py` fully green, then `make test`
green. Do not commit. Do not push.

---

## PROMPT 6 — `--rebase-base`

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo):
`tools/pkg_patch_refresh.py` has base-pinned diffing (BP-2), epoch
fatals (BP-4), a conflict-marker guard that makes `refresh` refuse to
generate from working files containing column-0 markers (BP-7), an
argparse `main()` with `--materialize REL` (BP-5), and helpers
`ensure_base_commit`, `base_file_content`, `resolve_head_commit`,
`load_manifest`, `save_manifest`, `list_working_files`.

Read:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", decision **BP-6** (note: `--set-base` was explicitly
   rejected at review — do not add it) and **BP-4** (the two fatal
   epoch cases, which rebase must pre-scan for).
2. `tools/pkg_patch_refresh.py` — whole file.

### What to implement (BP-6 only)

Files to touch: `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`. Nothing else.

1. Add:

```python
def rebase_base(pkgdir, new_base_ref, project_root):
    """BP-6: advance the package's recorded base to *new_base_ref*
    (a committish; 'HEAD' by default from the CLI), three-way-merging
    every working file from old-base content onto new-base content
    via `git merge-file`. Pre-scans and fails fast BEFORE mutating
    anything. Returns dict: {'old_base', 'new_base',
    'fast_forwarded': [rels], 'merged': [rels], 'conflicted': [rels],
    'untouched': [rels]}."""
```
   Pseudocode (stateful — follow exactly):
   - Resolve `new_base`: `git rev-parse <new_base_ref>^{commit}` in
     project_root; fatal RuntimeError on failure.
   - `pkgdir_abs`; `manifest = load_manifest(pkgdir_abs)`;
     `old_base = manifest.get('base_commit')`.
   - If `old_base` is None: record `manifest['base_commit'] = new_base`,
     `save_manifest`, return with all lists empty (nothing to merge —
     equivalent to first-time initialization at an explicit commit).
   - If `old_base == new_base`: return unchanged, all lists empty
     (caller prints a no-op message).
   - **Pre-scan pass (no mutation):** for each
     `rel in list_working_files(pkgdir_abs)`:
     `old_bytes = base_file_content(project_root, old_base, rel)`,
     `new_bytes = base_file_content(project_root, new_base, rel)`.
       - both None → package-new file → classify `untouched`.
       - old None, new not None → upstream added a file at a path this
         package also created → fatal RuntimeError naming rel, both
         SHAs, and that nothing has been modified.
       - old not None, new None → upstream deleted it at the new base →
         fatal RuntimeError naming rel and that nothing has been
         modified — same human-directed stop as BP-4's deleted case.
       - both present → merge candidate; ALSO pre-read the working
         file's bytes now.
   - **Mutation pass** (only reached if pre-scan raised nothing): for
     each merge candidate:
       - If working bytes == old_bytes (developer never edited it):
         overwrite the working file with `new_bytes` (binary), classify
         `fast_forwarded`.
       - Else: write `old_bytes` and `new_bytes` to two temp files;
         run `git merge-file -L working -L base -L upstream
         <working_path> <old_tmp> <new_tmp>` (cwd=project_root; NOTE:
         `git merge-file` edits `<working_path>` in place). Exit code:
         0 → classify `merged`; >0 (= number of conflicts) → classify
         `conflicted`; <0 → RuntimeError with stderr. `finally`-unlink
         both temp files.
       - **Temp-file discipline (a prior implementation attempt broke
         here):** the temp files MUST be fully written and **closed**
         before `git merge-file` is invoked. Use
         `tempfile.NamedTemporaryFile(delete=False)` in binary mode,
         `write()`, then `close()` — do NOT hold the handle open and
         rely on buffering. An empty or partially flushed base temp
         file makes `git merge-file` see both sides as wholesale
         rewrites and report conflicts on lines neither side touched
         (the symptom looks like "merge-file is doing a 2-way merge" —
         it is not; `git merge-file <current> <base> <other>` is a
         true 3-way merge, verified empirically: a 10-line file with a
         developer edit on line 2 and an upstream edit on line 9
         merges cleanly, rc=0, with the exact argument order above).
         If you see spurious conflicts, verify in this order before
         touching the merge call: (1) dump the two temp files just
         before invoking git — are they non-empty and holding OLD-base
         and NEW-base content respectively (not swapped, not working
         content)? (2) is `old_bytes` really fetched from the OLD
         recorded base commit (not HEAD)? (3) only then question the
         fixture (see test-fixture note below).
   - Record `manifest['base_commit'] = new_base`; `save_manifest`
     unconditionally (yes, even with conflicts — the working area now
     holds markers and the BP-7 guard blocks `refresh` until the
     developer resolves them; the base itself HAS moved).
   - Return the dict.

2. CLI: add `--rebase-base` with `nargs='?'`, `const='HEAD'`, mutually
   exclusive with `--materialize`. Output on success, in this order:
   one line per fast-forwarded rel (`fast-forwarded <rel>`), per merged
   rel (`merged <rel>`), per conflicted rel
   (`CONFLICT in <rel> — resolve the markers, then re-run refresh`);
   then a summary line
   `base: <old12> -> <new12>` (12-char abbreviations). If the result was
   a no-op (`old == new`), print `base already at <new12> — nothing to
   do`. RuntimeError → `error: {e}` on stderr, exit 1. Exit code 0 even
   with conflicts (the conflict is reported loudly and blocks refresh;
   rebase itself completed as specified).

### Tests to add

Use this shared arrangement where relevant: commit upstream `a.c`=V1;
refresh once with an edited working `a.c` (pins old base); commit
upstream `a.c`=V2 (moves upstream).

Test-fixture note: make every merge fixture at least ~10 lines, with
the developer's edit and upstream's V1→V2 change separated by at least
3 unchanged lines (e.g. edit line 2, upstream changes line 9). Diff
hunks that touch or are adjacent (no unchanged line between them)
conflict BY DESIGN — that is the loud-failure behavior, not a bug —
so a too-small fixture makes the "non-overlapping" tests assert the
wrong thing.

1. `test_rebase_fast_forwards_unedited_working_copy` — working `b.c`
   materialized at old base and never edited; after
   `rebase_base(..., 'HEAD', ...)` its content is V2's. **Catches:**
   rebase leaving unedited copies at old-base content, which would make
   the very next refresh emit an upstream-revert patch — the original
   bug reborn through the rebase path.
2. `test_rebase_merges_nonoverlapping_edit` — developer edit on a line
   far from V1→V2's change; after rebase the working file contains BOTH
   the developer's line and V2's change, `conflicted` empty, and the
   manifest records the new base. **Catches:** wrong `git merge-file`
   argument order (e.g. swapping base/other), which silently produces
   wrong merges.
3. `test_rebase_conflict_leaves_markers_and_blocks_refresh` — developer
   edit on the SAME line V1→V2 changed; after rebase: rel is in
   `conflicted`, the working file contains a column-0 `<<<<<<<`, the
   manifest records the NEW base, and a subsequent `refresh()` raises
   the BP-7 RuntimeError. **Catches:** (a) conflicts silently resolved
   by picking a side — violating the loud-failure invariant; (b) base
   not recorded after a conflicted pass, stranding the package between
   epochs.
4. `test_rebase_prescan_deleted_file_fatal_and_untouched` — package
   also has working `c.c` (existed at old base); upstream deletes
   `c.c` in the V2 commit. `rebase_base` must raise, AND `a.c`'s
   working copy must be byte-identical to before the call (pre-scan
   failed before any mutation), AND the manifest must still record the
   OLD base. **Catches:** a half-rebased package — some files merged,
   base ambiguous — after a mid-pass failure.
5. `test_rebase_noop_same_base` — rebase to the current base commit:
   returns all-empty, manifest unchanged, working files unchanged.
   **Catches:** pointless merge passes / manifest churn on no-ops.

### Gate, then report

`python3 tools/test_pkg_patch_refresh.py` fully green, then `make test`
green. Do not commit. Do not push.

---

## PROMPT 7 — README accuracy pass

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

Context from prior work (already on this branch, do not redo — and do
not modify any `tools/` file in this prompt): the package tooling now
pins a per-package base commit in `.refresh-manifest.json`; `refresh`
diffs working copies against `git show <base>:src/c47/<rel>`; epoch
mismatches (file added or deleted upstream after the base) are fatal,
named errors; working files containing conflict markers make refresh
refuse; the CLI grew `--materialize REL` and `--rebase-base [COMMIT]`.

Read:
1. `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md` — section "Refresh Base
   Pinning", ALL of it, especially **BP-8** (amended: the workflow
   documentation must be simple and accurate — this is a reviewed human
   requirement, not a style preference).
2. `design-docs/package-manager/README.md` — whole file.
3. `tools/pkg_patch_refresh.py` — `main()`'s argparse block and the
   docstrings of `materialize`, `rebase_base`, `refresh` (to quote exact
   CLI behavior, not remembered behavior).

### What to implement (BP-8 only)

File to touch: `design-docs/package-manager/README.md`. Nothing else. Do not touch
`PROPOSED_SPEC_CHANGES.md`, any `tools/` file, the `Makefile`, or
anything under `src/`.

1. **Authoring Workflow section:** replace the raw
   `cp src/c47/keyboard.c packages/my-pkg/keyboard.c` instruction with
   the sanctioned three-command happy path, presented exactly this
   simply:

```
python3 tools/pkg_patch_refresh.py packages/my-pkg --materialize keyboard.c
# ... edit packages/my-pkg/keyboard.c freely ...
python3 tools/pkg_patch_refresh.py packages/my-pkg
```
   Brand-new files need no materialize step — just create them in the
   working area (keep the existing `echo ... > packages/my-pkg/helper.c`
   example).

2. **New subsection "When upstream moves"** (place it right after the
   Authoring Workflow): explain in at most ~15 lines that (a) a package
   records the upstream commit it was authored against (its *base*) in
   `.refresh-manifest.json`, and `refresh` always diffs against that
   base — pulling new upstream commits does NOT change your patches;
   (b) to move a package forward run
   `python3 tools/pkg_patch_refresh.py packages/my-pkg --rebase-base`,
   which merges upstream's changes into your working copies, leaves
   standard conflict markers where your edits genuinely collide, and
   `refresh` will refuse to run until you resolve them; (c) if upstream
   added or deleted a file your package touches, `refresh`/
   `--rebase-base` stop with a named error so you can check — nothing is
   ever silently reclassified or reverted.

3. **Accuracy sweep of the whole README** — update every statement that
   still describes live-upstream diffing. At minimum check: the Quick
   Start numbered steps; "The Two Mechanisms" table intro ("diffs it
   against upstream" phrasing); the Patch Storage Format paragraph; the
   Version Control section (the manifest now also records
   `base_commit`); the refresh bullet list under Authoring Workflow
   ("mirrors a real upstream path, and differs from it" — differs from
   it *at the recorded base*). Do not restructure the document; keep
   edits minimal, but leave no sentence that is now false.

4. Keep it SIMPLE (BP-8 amendment, human requirement): a developer who
   never pulls upstream mid-package should be able to ignore the base
   concept entirely — the happy path must read as three commands, and
   base/rebase detail must live in the "When upstream moves" subsection,
   not spread through the happy path.

### Verification (documentation prompts still verify)

For every command you write into the README, actually run it once
against a scratch package (e.g. `packages/readme-check` — create it,
run the commands, then DELETE the scratch package directory and any
generated entries so the tree is clean of it). Confirm the commands
behave as the text claims. Then run
`python3 tools/test_pkg_patch_refresh.py` and `make test` and confirm
green (they should be untouched by a docs-only change — if they are not
green, something is wrong; STOP and report).

Do not commit. Do not push. In your report, list each README claim you
changed, as old → new.

---

## PROMPT 8 — Full-suite run + implementation report

You are working in `/home/stan/c43`. First run
`git branch --show-current`; it must print `pkgmgr/refresh-base-commit` —
if not, STOP and report.

This is the closing bookkeeping prompt for the "Refresh Base Pinning"
sequence (design record: `design-docs/package-manager/PROPOSED_SPEC_CHANGES.md`,
section "Refresh Base Pinning — diff against the recorded base commit",
BP-1 … BP-8). The prior prompts changed `tools/pkg_patch_refresh.py`,
`tools/test_pkg_patch_refresh.py`, and `design-docs/package-manager/README.md` — run
`git status --porcelain` and `git diff --stat` to see exactly what is on
the branch; that output, not this paragraph, is the authority on what
was implemented.

### Steps

1. Run the focused suites:
   `python3 tools/test_pkg_patch_refresh.py`,
   `python3 tools/test_pkg_patch_common.py`,
   `python3 tools/test_pkg_patch_resolver.py`.
   All must pass.
2. Run `make test`. It must be green. If anything fails at steps 1–2,
   STOP: report the failure verbatim; do not modify code in this prompt.
3. Update `design-docs/package-manager/IMPLEMENTATION_REPORT.md` (it exists — append
   a new dated section, do not rewrite prior content) titled
   "Refresh Base Pinning (branch pkgmgr/refresh-base-commit)":
   - one paragraph on the bug (refresh diffed against live upstream;
     upstream movement baked silent upstream-reverts into patches) and
     the fix (per-package `base_commit` in `.refresh-manifest.json`;
     diffs taken against `git show <base>:src/c47/<rel>`; epoch
     mismatches fatal; `--materialize` and `--rebase-base` CLI modes;
     conflict-marker guard);
   - the list of files changed (from `git diff --stat`);
   - the count of tests added (name them);
   - the `make test` result;
   - this exact closing sentence: "This implementation has not yet been
     independently audited."
4. Do not commit. Do not push. Report a summary of the report section
   you appended and the final test results.
