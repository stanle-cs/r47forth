#!/usr/bin/env python3
"""Patch-generation (`refresh`) tool for the plain-diff package overlay
system (PROPOSED_SPEC_CHANGES.md, revision 2; automatic classification and
the flat working directory added in a later revision — see the "Automatic
Classification, Flat Working Directory" entry).

Authoring-time CLI:

    python3 tools/pkg_patch_refresh.py <pkgdir>

e.g.    python3 tools/pkg_patch_refresh.py packages/my-pkg

<pkgdir> is the package directory relative to the project root (the same
form used in -DCUSTOM_PKG).

A package's working area is FLAT, mirroring upstream paths directly:

    packages/my-pkg/
    ├── keyboard.c        # materialized copy of an existing upstream file
    └── my_module.c       # a brand-new file, no upstream counterpart

The developer never creates `patches/` or `files/` themselves and never
decides which one anything belongs in — `refresh` classifies each working
file automatically, purely from whether a same-path upstream file exists:

  - Upstream file exists at the mirrored path → diff it → write/overwrite
    <pkgdir>/patches/<NNN>-<rel_encoded>.patch (a whole-file `git diff`, no
    restriction on what kind of change it contains). The ordinal is reused
    from any existing patch already targeting that rel (so re-running
    refresh after further edits rewrites the same file in place, and a
    developer's manual ordinal rename for explicit cross-package ordering
    is preserved); a fresh rel defaults to ordinal 010. If the working copy
    no longer differs from upstream (reverted edit), any existing patch for
    it is deleted.
  - No upstream file at the mirrored path → copy it whole into
    <pkgdir>/files/<rel> (path-mirrored).
  - A working-area file that existed at a prior `refresh` but is now GONE
    (the developer deleted their scratch copy): the corresponding generated
    `patches/`/`files/` entry is deleted too, so generated output never
    drifts ahead of what the working area currently reflects. This is the
    same cleanup as the reverted-edit case, generalized: "not currently
    producible from the working area" removes any previously-generated
    entry, whether the reason is reversion or deletion.

`patches/` and `files/` are build OUTPUT ONLY, generated entirely by this
tool — never something a developer creates or edits directly. A hidden
per-package manifest (`<pkgdir>/.refresh-manifest.json`, tracking the
sha256 of each entry as `refresh` wrote it) lets `refresh` detect drift: an
existing `patches/`/`files/` entry whose on-disk content no longer matches
what `refresh` itself last wrote there means something outside `refresh`
touched it (hand-added or hand-edited, bypassing the working area). This is
warned about (loudly, on stderr) and then self-healed by overwriting with
freshly generated content — not treated as fatal, since the normal
"working copy changed, refresh regenerates" cycle covers the exact same
code path and must not be blocked by it.

The flat working area (everything outside `patches/`/`files/` and the
manifest) is a local editing convenience, `.gitignore`d — only `patches/`,
`files/`, and the manifest are meant to be committed.

No libclang dependency anywhere in this module (or the whole pkg_patch_*
tree) — this is plain `git diff`, not AST-based extraction.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import decode_patch_filename

DEFAULT_ORDINAL = 10
_EXCLUDED_TOP_DIRS = ('patches', 'files')
MANIFEST_NAME = '.refresh-manifest.json'
_EXCLUDED_TOP_FILES = ('meson.build', MANIFEST_NAME)


def run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def _sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def _sha256_file(path):
    with open(path, 'rb') as f:
        return _sha256_bytes(f.read())


def load_manifest(pkgdir_abs):
    """{'patches': {filename: sha256}, 'files': {rel: sha256}} recording
    the content refresh itself last wrote for each generated entry.
    Missing/corrupt manifest is treated as empty (fresh package, or a
    manifest that predates this mechanism) — not fatal."""
    path = os.path.join(pkgdir_abs, MANIFEST_NAME)
    if not os.path.isfile(path):
        return {'patches': {}, 'files': {}}
    try:
        with open(path) as f:
            data = json.load(f)
    except (ValueError, OSError):
        return {'patches': {}, 'files': {}}
    data.setdefault('patches', {})
    data.setdefault('files', {})
    return data


def save_manifest(pkgdir_abs, manifest):
    path = os.path.join(pkgdir_abs, MANIFEST_NAME)
    with open(path, 'w') as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write('\n')


def _check_drift(kind, key, dest_path, manifest_section, warnings):
    """Before overwriting dest_path with freshly generated content,
    compare its CURRENT on-disk content (if any) against the hash
    refresh itself recorded the last time it wrote this entry. A
    mismatch (or a file present with no manifest record at all) means
    something outside refresh touched patches/ or files/ directly —
    Step 3's guard. Appends a message to *warnings* but never raises;
    refresh overwrites and re-records the hash regardless (self-healing
    — the normal edit-working-copy-then-refresh cycle must not be
    blocked by the same mechanism that catches a hand-edit)."""
    if not os.path.isfile(dest_path):
        return
    on_disk_hash = _sha256_file(dest_path)
    recorded_hash = manifest_section.get(key)
    if recorded_hash is None:
        warnings.append(
            f'{kind} {key!r} exists but was not recorded as generated by '
            f'refresh (hand-added, bypassing the working area?) — '
            f'overwriting with freshly generated content.')
    elif on_disk_hash != recorded_hash:
        warnings.append(
            f'{kind} {key!r} content does not match what refresh last '
            f'wrote for it (hand-edited directly, bypassing the working '
            f'area?) — overwriting with freshly generated content.')


def list_working_files(pkgdir_abs):
    """Every regular file directly under pkgdir_abs, recursively,
    excluding the 'patches' and 'files' subdirectories (generated
    output) and the manifest/'meson.build' (packages have no
    meson.build under revision 2 — one present is pre-revision-2
    cruft, not a working-area file).

    Returns a sorted list of paths relative to pkgdir_abs, using '/' as
    the separator regardless of platform.
    """
    if not os.path.isdir(pkgdir_abs):
        return []

    result = []
    for root, dirs, files in os.walk(pkgdir_abs):
        rel_root = os.path.relpath(root, pkgdir_abs)
        if rel_root == '.':
            dirs[:] = [d for d in dirs if d not in _EXCLUDED_TOP_DIRS]
        for fname in files:
            if rel_root == '.' and fname in _EXCLUDED_TOP_FILES:
                continue
            rel = fname if rel_root == '.' else f'{rel_root}/{fname}'
            result.append(rel.replace(os.sep, '/'))
    return sorted(result)


# Backwards-compatible alias (pre-automatic-classification name).
list_materialized_files = list_working_files


def _existing_patches_for_rel(patches_dir, rel):
    """Return [(ordinal, filename), ...] sorted ascending for every
    patch already targeting *rel* in patches_dir (normally 0 or 1;
    tolerated as >1 for stray/hand-edited files, see refresh())."""
    matches = []
    if os.path.isdir(patches_dir):
        for fname in os.listdir(patches_dir):
            if not fname.endswith('.patch'):
                continue
            try:
                ordinal, r = decode_patch_filename(fname)
            except ValueError:
                continue
            if r == rel:
                matches.append((ordinal, fname))
    matches.sort()
    return matches


def generate_patch(upstream_path, materialized_path, rel, project_root,
                   context=3):
    """Whole-file unified diff of materialized_path against
    upstream_path, rewritten to a git-apply-able patch targeting
    src/c47/<rel> with a real, resolvable pre-image blob SHA.

    Returns the patch text, or None if the two files are identical.
    Raises RuntimeError if either file is binary (not diffable as text),
    or if the upstream file has uncommitted modifications (its blob
    would not be a resolvable git object, breaking the git-apply-3
    ancestry assumption at the source — see PROPOSED_SPEC_CHANGES.md §5).
    """
    raw = run(['git', 'diff', '--no-index', '--full-index',
               f'-U{context}', upstream_path, materialized_path]).stdout

    if raw == '':
        return None

    if '\nBinary files' in raw or raw.startswith('Binary files'):
        raise RuntimeError(
            f'{rel}: upstream/materialized file appears to be binary — '
            f'cannot generate a text patch. Binary overrides are not '
            f'supported by this mechanism.')

    m = re.search(r'^index ([0-9a-f]{40})\.\.', raw, re.MULTILINE)
    if m is None:
        raise RuntimeError(
            f'{rel}: git diff --no-index --full-index did not produce '
            f'a full 40-char pre-image blob SHA (unexpected git diff '
            f'output format)')
    pre_sha = m.group(1)

    check = run(['git', 'cat-file', '-e', pre_sha], cwd=project_root)
    if check.returncode != 0:
        raise RuntimeError(
            f'{rel}: pre-image blob {pre_sha} is not a resolvable git '
            f'object in this repository — src/c47/{rel} has uncommitted '
            f'modifications? refresh must run against committed '
            f'upstream content so git apply -3 can resolve the base '
            f'blob later (PROPOSED_SPEC_CHANGES.md, Application '
            f'Mechanism).')

    out_lines = []
    for line in raw.split('\n'):
        if line.startswith('diff --git '):
            out_lines.append(f'diff --git a/src/c47/{rel} b/src/c47/{rel}')
        elif line.startswith('--- a/'):
            out_lines.append(f'--- a/src/c47/{rel}')
        elif line.startswith('+++ b/'):
            out_lines.append(f'+++ b/src/c47/{rel}')
        else:
            out_lines.append(line)

    text = '\n'.join(out_lines)
    if not text.endswith('\n'):
        text += '\n'
    return text


def _list_files_dir_entries(files_dir):
    """Every regular file under files_dir, recursively, as rel paths
    (relative to files_dir, '/'-separated)."""
    result = []
    if not os.path.isdir(files_dir):
        return result
    for root, _dirs, files in os.walk(files_dir):
        rel_root = os.path.relpath(root, files_dir)
        for fname in files:
            rel = fname if rel_root == '.' else f'{rel_root}/{fname}'
            result.append(rel.replace(os.sep, '/'))
    return sorted(result)


def refresh(pkgdir, project_root, context=3):
    """Regenerate patches/ and files/ under <project_root>/<pkgdir> from
    every working-area file directly under <pkgdir>, classifying each
    automatically (patch if a same-path upstream file exists, whole-file
    copy into files/ otherwise) and cleaning up any generated entry no
    longer producible from the current working area.

    Returns a dict:
      'written': [patch filenames written/overwritten]
      'files_written': [rels copied/overwritten into files/]
      'removed': [patch filenames deleted (reverted edit or working-area
                  file deleted; also a duplicate-ordinal cleanup)]
      'files_removed': [rels deleted from files/ (working-area file
                        deleted)]
      'warnings': [drift warnings — see _check_drift]

    Raises RuntimeError (via generate_patch) on a binary file or an
    upstream file with uncommitted modifications — both fatal, named.
    """
    pkgdir_abs = os.path.join(project_root, pkgdir)
    patches_dir = os.path.join(pkgdir_abs, 'patches')
    files_dir = os.path.join(pkgdir_abs, 'files')
    src_c47_dir = os.path.join(project_root, 'src', 'c47')

    manifest = load_manifest(pkgdir_abs)
    warnings = []

    written = []
    files_written = []
    removed = []
    files_removed = []

    expected_patch_names = set()
    expected_file_rels = set()

    for rel in list_working_files(pkgdir_abs):
        working_path = os.path.join(pkgdir_abs, *rel.split('/'))
        upstream_path = os.path.join(src_c47_dir, *rel.split('/'))

        if os.path.isfile(upstream_path):
            # --- Classified as a patch: diff against upstream. --------
            existing = _existing_patches_for_rel(patches_dir, rel)
            patch_text = generate_patch(upstream_path, working_path, rel,
                                        project_root, context=context)

            if patch_text is None:
                # Reverted edit: nothing expected for this rel; the
                # stale-cleanup pass below removes any leftover patch.
                continue

            ordinal = existing[0][0] if existing else DEFAULT_ORDINAL
            encoded_rel = rel.replace('/', '__')
            out_name = f'{ordinal:03d}-{encoded_rel}.patch'
            out_path = os.path.join(patches_dir, out_name)

            _check_drift('patch', out_name, out_path,
                        manifest['patches'], warnings)

            os.makedirs(patches_dir, exist_ok=True)
            with open(out_path, 'w') as f:
                f.write(patch_text)
            manifest['patches'][out_name] = _sha256_bytes(
                patch_text.encode('utf-8'))
            written.append(out_name)
            expected_patch_names.add(out_name)

        else:
            # --- Classified as a new file: copy whole into files/. ----
            dest_path = os.path.join(files_dir, *rel.split('/'))
            expected_file_rels.add(rel)

            working_hash = _sha256_file(working_path)
            if (os.path.isfile(dest_path)
                    and _sha256_file(dest_path) == working_hash
                    and manifest['files'].get(rel) == working_hash):
                # Already up to date, and the on-disk copy is exactly
                # what refresh itself last recorded writing — nothing
                # to do, and nothing to report as "written" this run.
                continue

            _check_drift('files/ entry', rel, dest_path,
                        manifest['files'], warnings)

            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            shutil.copy2(working_path, dest_path)
            manifest['files'][rel] = working_hash
            files_written.append(rel)

    # --- Stale cleanup: any generated entry not (re)produced this run,
    # whether because its working-area file was reverted, deleted, or
    # (for patches) superseded by a different ordinal for the same rel.
    if os.path.isdir(patches_dir):
        for fname in sorted(os.listdir(patches_dir)):
            if not fname.endswith('.patch'):
                continue
            if fname in expected_patch_names:
                continue
            try:
                decode_patch_filename(fname)
            except ValueError:
                continue
            os.unlink(os.path.join(patches_dir, fname))
            manifest['patches'].pop(fname, None)
            removed.append(fname)

    for rel in _list_files_dir_entries(files_dir):
        if rel in expected_file_rels:
            continue
        os.unlink(os.path.join(files_dir, *rel.split('/')))
        manifest['files'].pop(rel, None)
        files_removed.append(rel)

    if written or files_written or removed or files_removed:
        save_manifest(pkgdir_abs, manifest)

    return {
        'written': written,
        'files_written': files_written,
        'removed': removed,
        'files_removed': files_removed,
        'warnings': warnings,
    }


def main():
    """CLI entry point."""
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <pkgdir>\n'
              f'  e.g. {sys.argv[0]} packages/my-pkg',
              file=sys.stderr)
        sys.exit(1)

    pkgdir = sys.argv[1].rstrip('/')
    project_root = os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

    try:
        result = refresh(pkgdir, project_root)
    except RuntimeError as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)

    for w in result['warnings']:
        print(f'warning: {w}', file=sys.stderr)
    for fname in result['written']:
        print(f'wrote patches/{fname}')
    for rel in result['files_written']:
        print(f'wrote files/{rel}')
    for fname in result['removed']:
        print(f'removed patches/{fname} (no longer producible from the '
              f'working area)')
    for rel in result['files_removed']:
        print(f'removed files/{rel} (no longer producible from the '
              f'working area)')

    if not any(result[k] for k in
               ('written', 'files_written', 'removed', 'files_removed')):
        print(f'no changes under {pkgdir} — patches/ and files/ already '
              f'up to date')


if __name__ == '__main__':
    main()
