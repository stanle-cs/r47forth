#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
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

A package may carry a top-level `<pkgdir>/.pkgignore` naming working-area
files that `refresh` must not classify at all — neither diffed into
`patches/` nor copied into `files/`. Without it, everything in the working
area is package content by definition, so design docs, notes and dev
scripts kept beside the sources get copied into `files/` and ship inside
the distributable package (and get regenerated on every refresh). One
pattern per line; `#` comments and blank lines are ignored. A deliberately
small `.gitignore` subset:

  - trailing `/` (`docs/`) — a directory: ignores everything beneath it
  - no `/` in the pattern (`*.md`) — matched against the BASENAME at any
    depth, so it ignores matching files anywhere in the working area
  - a `/` in the pattern (`notes/*.txt`) — matched against the whole path
    relative to the package root

Globbing is `fnmatch` (`*`, `?`, `[seq]`). Two deliberate divergences from
`.gitignore`, both to keep the implementation obvious: `*` matches across
`/` in path-form patterns, and there is no negation (`!`). `.pkgignore`
itself is never classified.

Adding a pattern is retroactive: an ignored file stops being producible
from the working area, so the normal stale-cleanup pass below deletes any
`patches/`/`files/` entry previously generated for it. Removing a pattern
brings it back on the next refresh. Ignoring never edits the working area.

The flat working area (everything outside `patches/`/`files/` and the
manifest) is a local editing convenience, `.gitignore`d — only `patches/`,
`files/`, and the manifest are meant to be committed.

No libclang dependency anywhere in this module (or the whole pkg_patch_*
tree) — this is plain `git diff`, not AST-based extraction.
"""
import argparse
import fnmatch
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import (
    SIBLING_ROOTS,
    decode_patch_filename,
    upstream_abs_path,
    upstream_repo_rel,
)

DEFAULT_ORDINAL = 10
_EXCLUDED_TOP_DIRS = ('patches', 'files')
MANIFEST_NAME = '.refresh-manifest.json'
PKGIGNORE_NAME = '.pkgignore'
_EXCLUDED_TOP_FILES = ('meson.build', MANIFEST_NAME, PKGIGNORE_NAME)

_CONFLICT_MARKER_RE = re.compile(r'^(<{7}|={7}|>{7})', re.MULTILINE)


def check_rebase_preflight(project_root, target_ref):
    """Check whether the caller's upstream roots are buildable at
    *target_ref*.

    Resolves ``target_ref^{commit}``, compares the Git tree objects of
    every package-reachable upstream root (``src/c47`` plus the
    SIBLING_ROOTS) between HEAD and the target, and detects tracked or
    untracked changes under those roots.

    Returns a dict::

        {
            'target_sha': '<40-char SHA>',
            'head_sha': '<40-char SHA>',
            'src_c47_tree_matches': True|False,
            'src_c47_dirty': True|False,
            'buildable': True|False,
            'issues': [list of human-readable issue strings],
        }

    *buildable* is ``True`` only when ``src_c47_tree_matches`` is ``True``
    AND ``src_c47_dirty`` is ``False``.  The check is read-only — it never
    mutates the working tree or index.
    """
    # Resolve target ref.
    r = run(['git', 'rev-parse', f'{target_ref}^{{commit}}'],
            cwd=project_root)
    if r.returncode != 0 or not r.stdout.strip():
        raise RuntimeError(
            f'cannot resolve {target_ref!r} as a commit in '
            f'{project_root!r}')
    target_sha = r.stdout.strip()

    # Resolve HEAD.
    r = run(['git', 'rev-parse', 'HEAD'], cwd=project_root)
    if r.returncode != 0 or not r.stdout.strip():
        raise RuntimeError(
            f'cannot resolve HEAD in {project_root!r}')
    head_sha = r.stdout.strip()

    issues = []
    src_c47_tree_matches = True
    src_c47_dirty = False

    # Compare tree objects HEAD:<root> and target:<root> for every root
    # the package system can touch: src/c47 plus the sibling roots
    # (T2-A). The dict keys keep their historical src_c47 names; they
    # now mean "every package-reachable upstream root".
    upstream_roots = ['src/c47'] + [f'src/{r}' for r in SIBLING_ROOTS]

    for root_path in upstream_roots:
        r_head = run(['git', 'rev-parse', f'{head_sha}^{{tree}}:{root_path}'],
                      cwd=project_root)
        r_target = run(['git', 'rev-parse',
                        f'{target_sha}^{{tree}}:{root_path}'],
                        cwd=project_root)

        if r_head.returncode == 0 and r_target.returncode == 0:
            head_tree = r_head.stdout.strip()
            target_tree = r_target.stdout.strip()
            if head_tree != target_tree:
                src_c47_tree_matches = False
                issues.append(
                    f'{root_path} tree at HEAD ({head_sha[:12]}) differs '
                    f'from target {target_ref} ({target_sha[:12]})')
        elif r_head.returncode != 0 and r_target.returncode != 0:
            # Neither has this root — that's fine, they match.
            pass
        else:
            src_c47_tree_matches = False
            if r_head.returncode != 0:
                issues.append(
                    f'{root_path} does not exist at HEAD ({head_sha[:12]})')
            if r_target.returncode != 0:
                issues.append(
                    f'{root_path} does not exist at target {target_ref} '
                    f'({target_sha[:12]})')

    # Check for tracked/untracked changes under every reachable root.
    r_status = run(['git', 'status', '--porcelain', '--'] + upstream_roots,
                   cwd=project_root)
    if r_status.returncode == 0 and r_status.stdout.strip():
        src_c47_dirty = True
        status_lines = [
            line for line in r_status.stdout.splitlines() if line.strip()
        ]
        issues.append(
            f'{"/".join(upstream_roots)} has local changes '
            f'({len(status_lines)} file(s)): '
            + '; '.join(status_lines[:5]))

    buildable = src_c47_tree_matches and not src_c47_dirty

    return {
        'target_sha': target_sha,
        'head_sha': head_sha,
        'src_c47_tree_matches': src_c47_tree_matches,
        'src_c47_dirty': src_c47_dirty,
        'buildable': buildable,
        'issues': issues,
    }


def _working_file_marker_lines(path):
    """1-indexed line numbers of column-0 conflict-marker lines
    (<<<<<<< / ======= / >>>>>>>) in the text file at *path*.
    Returns an empty list if the file is binary (un-decodable)."""
    try:
        with open(path, 'r') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        return []
    hits = []
    for i, line in enumerate(lines, start=1):
        if _CONFLICT_MARKER_RE.match(line):
            hits.append(i)
    return hits


def run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def _sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def _sha256_file(path):
    with open(path, 'rb') as f:
        return _sha256_bytes(f.read())


def _check_drift_entry(kind, key, dest_path, manifest_section, warnings,
                       msg_missing, msg_mismatch):
    """Core drift comparison for a single generated entry.
    If dest_path exists, compare its hash against manifest_section[key].
    Appends a warning on mismatch or missing record.
    Returns False if file does not exist (nothing to check)."""
    if not os.path.isfile(dest_path):
        return False
    on_disk_hash = _sha256_file(dest_path)
    recorded_hash = manifest_section.get(key)
    if recorded_hash is None:
        warnings.append(msg_missing.format(kind=kind, key=key))
    elif on_disk_hash != recorded_hash:
        warnings.append(msg_mismatch.format(kind=kind, key=key))
    return True


def load_manifest(pkgdir_abs):
    """{'patches': {filename: sha256}, 'files': {rel: sha256}} recording
    the content refresh itself last wrote for each generated entry.
    A genuinely missing manifest initializes a fresh package (OK).
    An existing but unreadable manifest is fatal — treating it as fresh
    would re-pin base_commit to current HEAD and emit patches that revert
    upstream changes (R5-A1)."""
    path = os.path.join(pkgdir_abs, MANIFEST_NAME)
    if not os.path.isfile(path):
        return {'patches': {}, 'files': {}}
    try:
        with open(path) as f:
            data = json.load(f)
    except (ValueError, OSError) as exc:
        raise RuntimeError(
            f'{path} exists but is unreadable or is not valid JSON ({exc}). '
            f'Refusing to treat a corrupt manifest as a fresh package: that '
            f'would re-pin base_commit to current HEAD and emit patches that '
            f'revert upstream changes (R5-A1). Restore it from git '
            f'(git checkout -- {path}) or delete it deliberately to '
            f're-initialize this package.')
    if not isinstance(data, dict):
        raise RuntimeError(
            f'{path} exists but is unreadable or is not valid JSON '
            f'— top-level JSON is not an object. '
            f'Refusing to treat a corrupt manifest as a fresh package: that '
            f'would re-pin base_commit to current HEAD and emit patches that '
            f'revert upstream changes (R5-A1). Restore it from git '
            f'(git checkout -- {path}) or delete it deliberately to '
            f're-initialize this package.')
    data.setdefault('patches', {})
    data.setdefault('files', {})
    return data


def save_manifest(pkgdir_abs, manifest):
    """Write manifest atomically via same-directory temp file + fsync +
    rename, so the previous valid manifest survives interruption."""
    path = os.path.join(pkgdir_abs, MANIFEST_NAME)
    fd, tmp = tempfile.mkstemp(dir=pkgdir_abs, prefix='.refresh-manifest.',
                               suffix='.tmp')
    try:
        with os.fdopen(fd, 'w') as f:
            json.dump(manifest, f, indent=2, sort_keys=True)
            f.write('\n')
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


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


def validate_base_commit(project_root, base_commit):
    """Raise RuntimeError if *base_commit* is not resolvable as a commit
    in the repository at *project_root* (e.g. shallow clone, missing
    history). Uses `git cat-file -e <sha>^{commit}`."""
    r = run(
        ['git', 'cat-file', '-e', base_commit + '^{commit}'],
        cwd=project_root)
    if r.returncode != 0:
        raise RuntimeError(
            f'recorded base commit {base_commit[:12]} is not available '
            f'in this repository — this usually means the repository is '
            f'a shallow clone or is missing history. Fetch or unshallow '
            f'the repository to make the base commit available.')


def ensure_base_commit(manifest, project_root, warnings):
    """Return (base_commit, initialized_bool). If the manifest already
    records a base_commit, return it unchanged. Otherwise record
    HEAD's full SHA into manifest['base_commit'] (BP-3). If generated
    entries already exist (legacy pre-base-pinning package), append a
    loud warning to *warnings* saying the base was ASSUMED to be
    current HEAD and earlier patches were generated against live
    upstream."""
    existing = manifest.get('base_commit')
    if existing:
        validate_base_commit(project_root, existing)
        return (existing, False)
    head = resolve_head_commit(project_root)
    if manifest['patches'] or manifest['files']:
        warnings.append(
            f'base_commit initialized to current HEAD ({head}) — '
            f'existing patches/files/ entries were generated against '
            f'live upstream and may not match this base')
    manifest['base_commit'] = head
    return (head, True)


def base_file_content(project_root, base_commit, rel):
    """Raw BYTES of src/c47/<rel> as it existed at base_commit, or
    None if the path does not exist at that commit. Bytes, not text —
    the base copy must be byte-exact for the pre-image blob SHA to
    match (BP-2). Caller must validate base_commit first (the commit
    must exist in the repo). Any git error other than path absence
    raises RuntimeError."""
    repo_rel = upstream_repo_rel(rel)
    r = subprocess.run(
        ['git', 'show', f'{base_commit}:{repo_rel}'],
        capture_output=True, cwd=project_root)
    if r.returncode == 0:
        return r.stdout
    stderr_text = r.stderr.decode() if r.stderr else ''
    if ('does not exist' in stderr_text or 'not a blob' in stderr_text
            or 'but not in' in stderr_text):
        return None
    raise RuntimeError(
        f'failed to extract {repo_rel} at base {base_commit[:12]}: '
        f'{stderr_text.strip()}')


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
    _check_drift_entry(
        kind, key, dest_path, manifest_section, warnings,
        msg_missing=(
            '{kind} {key!r} exists but was not recorded as generated by '
            'refresh (hand-added, bypassing the working area?) — '
            'overwriting with freshly generated content.'),
        msg_mismatch=(
            '{kind} {key!r} content does not match what refresh last '
            'wrote for it (hand-edited directly, bypassing the working '
            'area?) — overwriting with freshly generated content.'),
    )


def _scan_drift_for_rebase(pkgdir_abs):
    """Read-only scan of generated patches/ and files/ entries against
    manifest hashes. Returns a list of warning strings for entries that
    are missing from the manifest or have a content hash mismatch.
    Does NOT overwrite or self-heal — used by rebase_base to warn the
    user before changing the base epoch."""
    warnings = []
    manifest = load_manifest(pkgdir_abs)

    msg_patch_missing = (
        'patch {key!r} exists but was not recorded as generated by '
        'refresh (hand-added?) — rebase leaves generated output unchanged; '
        'the next refresh will regenerate/self-heal it.')
    msg_patch_mismatch = (
        'patch {key!r} content does not match what refresh last wrote for it '
        '(hand-edited directly?) — rebase leaves generated output unchanged; '
        'the next refresh will regenerate/self-heal it.')
    msg_file_missing = (
        'files entry {key!r} exists but was not recorded as generated by '
        'refresh (hand-added?) — rebase leaves generated output unchanged; '
        'the next refresh will regenerate/self-heal it.')
    msg_file_mismatch = (
        'files entry {key!r} content does not match what refresh last wrote '
        'for it (hand-edited directly?) — rebase leaves generated output '
        'unchanged; the next refresh will regenerate/self-heal it.')

    patches_dir = os.path.join(pkgdir_abs, 'patches')
    if os.path.isdir(patches_dir):
        for fname in sorted(os.listdir(patches_dir)):
            if fname.endswith('.patch'):
                dest_path = os.path.join(patches_dir, fname)
                _check_drift_entry('patch', fname, dest_path,
                                   manifest['patches'], warnings,
                                   msg_patch_missing, msg_patch_mismatch)

    files_dir = os.path.join(pkgdir_abs, 'files')
    for rel in _list_files_dir_entries(files_dir):
        dest_path = os.path.join(files_dir, *rel.split('/'))
        _check_drift_entry('files entry', rel, dest_path,
                           manifest['files'], warnings,
                           msg_file_missing, msg_file_mismatch)

    return warnings


def load_pkgignore(pkgdir_abs):
    """Patterns from <pkgdir>/.pkgignore, in file order, with comments
    and blank lines dropped. Missing file → no patterns (the common
    case; not an error)."""
    path = os.path.join(pkgdir_abs, PKGIGNORE_NAME)
    if not os.path.isfile(path):
        return []
    patterns = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                patterns.append(line)
    return patterns


def is_ignored(rel, patterns):
    """True if *rel* (a '/'-separated path relative to the package root)
    matches any pattern. See the module docstring for the supported
    subset."""
    basename = rel.rsplit('/', 1)[-1]
    for pat in patterns:
        if pat.endswith('/'):
            prefix = pat.strip('/')
            if prefix and (rel == prefix or rel.startswith(prefix + '/')):
                return True
        elif '/' in pat:
            if fnmatch.fnmatch(rel, pat.lstrip('/')):
                return True
        elif fnmatch.fnmatch(basename, pat):
            return True
    return False


def list_working_files(pkgdir_abs):
    """Every regular file directly under pkgdir_abs, recursively,
    excluding the 'patches' and 'files' subdirectories (generated
    output), the manifest/'meson.build' (packages have no meson.build
    under revision 2 — one present is pre-revision-2 cruft, not a
    working-area file), '.pkgignore' itself, and anything '.pkgignore'
    matches.

    Ignoring is applied here, in the single place every caller reads the
    working area from, so `refresh` and `rebase_base` agree on what the
    package contains without either needing to know about `.pkgignore`.

    Returns a sorted list of paths relative to pkgdir_abs, using '/' as
    the separator regardless of platform.
    """
    if not os.path.isdir(pkgdir_abs):
        return []

    patterns = load_pkgignore(pkgdir_abs)
    result = []
    for root, dirs, files in os.walk(pkgdir_abs):
        rel_root = os.path.relpath(root, pkgdir_abs)
        if rel_root == '.':
            dirs[:] = [d for d in dirs if d not in _EXCLUDED_TOP_DIRS]
        # Prune ignored directories so an ignored tree is never walked.
        dirs[:] = [
            d for d in dirs
            if not is_ignored(
                d if rel_root == '.' else f'{rel_root}/{d}'.replace(os.sep, '/'),
                patterns)
        ]
        for fname in files:
            if rel_root == '.' and fname in _EXCLUDED_TOP_FILES:
                continue
            rel = fname if rel_root == '.' else f'{rel_root}/{fname}'
            rel = rel.replace(os.sep, '/')
            if is_ignored(rel, patterns):
                continue
            result.append(rel)
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


def _canonicalize_mode_metadata(raw):
    """Strip mode lines and normalize the index line to 100644.

    Patch application materializes/writes byte content, so canonical 100644
    prevents host-temp filesystem modes from becoming package state.
    """
    canonical_lines = []
    for line in raw.split('\n'):
        if re.match(r'^old mode ', line):
            continue
        if re.match(r'^new mode ', line):
            continue
        m_idx = re.match(r'^(index [0-9a-f]{40}\.\.[0-9a-f]{40})', line)
        if m_idx:
            canonical_lines.append(m_idx.group(1) + ' 100644')
            continue
        canonical_lines.append(line)
    return '\n'.join(canonical_lines)


def generate_patch(base_bytes, materialized_path, rel, project_root,
                   base_commit, context=3):
    """Whole-file unified diff of materialized_path against base_bytes,
    rewritten to a git-apply-able patch targeting src/c47/<rel> with a
    real, resolvable pre-image blob SHA.

    base_bytes is the raw content of src/c47/<rel> as it existed at
    base_commit (BP-2). A temporary file is used for the git diff
    invocation so the pre-image blob SHA matches the base commit's blob.

    Returns the patch text, or None if the two files are identical.
    Raises RuntimeError if either file is binary (not diffable as text),
    or if the base blob is not a resolvable git object (shallow clone
    missing history, or a bogus recorded base).
    """
    tmp_fd = tempfile.NamedTemporaryFile(delete=False)
    tmp_path = tmp_fd.name
    try:
        tmp_fd.write(base_bytes)
        tmp_fd.close()

        # Compare content bytes before diffing — a mode-only change produces
        # no meaningful patch (the resolver writes file bytes, not modes).
        with open(materialized_path, 'rb') as mf:
            if mf.read() == base_bytes:
                return None

        raw = run(['git', 'diff', '--no-index', '--full-index',
                   f'-U{context}', tmp_path, materialized_path]).stdout

        if raw == '':
            return None

        if '\nBinary files' in raw or raw.startswith('Binary files'):
            raise RuntimeError(
                f'{rel}: base/materialized file appears to be binary — '
                f'cannot generate a text patch. Binary overrides are not '
                f'supported by this mechanism.')

        raw = _canonicalize_mode_metadata(raw)

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
                f'{rel}: base blob {pre_sha} (from base_commit {base_commit}) '
                f'is not a resolvable git object in this repository — '
                f'this can happen with a shallow clone missing history '
                f'or a bogus recorded base '
                f'(PROPOSED_SPEC_CHANGES.md, BP-2).')

        repo_rel = upstream_repo_rel(rel)
        out_lines = []
        for line in raw.split('\n'):
            if line.startswith('diff --git '):
                out_lines.append(
                    f'diff --git a/{repo_rel} b/{repo_rel}')
            elif line.startswith('--- a/'):
                out_lines.append(f'--- a/{repo_rel}')
            elif line.startswith('+++ b/'):
                out_lines.append(f'+++ b/{repo_rel}')
            else:
                out_lines.append(line)

        text = '\n'.join(out_lines)
        if not text.endswith('\n'):
            text += '\n'
        return text
    finally:
        os.unlink(tmp_path)


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

    Raises RuntimeError on a binary file (via generate_patch), or on a
    BP-4 epoch mismatch: file exists in live upstream but not at base
    (upstream added after pin), or file at base but deleted live
    (upstream removed) — both fatal, named with --rebase-base remedy.
    """
    pkgdir_abs = os.path.join(project_root, pkgdir)
    patches_dir = os.path.join(pkgdir_abs, 'patches')
    files_dir = os.path.join(pkgdir_abs, 'files')

    manifest = load_manifest(pkgdir_abs)
    warnings = []
    base_commit, base_initialized = ensure_base_commit(
        manifest, project_root, warnings)

    written = []
    files_written = []
    removed = []
    files_removed = []

    expected_patch_names = set()
    expected_file_rels = set()

    for rel in list_working_files(pkgdir_abs):
        working_path = os.path.join(pkgdir_abs, *rel.split('/'))
        upstream_path = upstream_abs_path(project_root, rel)

        live_exists = os.path.isfile(upstream_path)
        base_bytes = base_file_content(project_root, base_commit, rel)

        # BP-4: epoch-mismatch fatal checks
        if live_exists and base_bytes is None:
            raise RuntimeError(
                f'{rel}: exists in current upstream but not at the recorded '
                f'base commit {base_commit} — upstream added this file after '
                f'the package base was pinned. Advance the base first: '
                f'python3 tools/pkg_patch_refresh.py <pkgdir> --rebase-base '
                f'(PROPOSED_SPEC_CHANGES.md, BP-4).')
        if (not live_exists) and (base_bytes is not None):
            raise RuntimeError(
                f'{rel}: existed at the recorded base commit {base_commit} '
                f'but has been deleted from current upstream — stopping so '
                f'you can check whether this package\'s change to it still '
                f'applies at all (PROPOSED_SPEC_CHANGES.md, BP-4, amended '
                f'2026-07-14).')

        if live_exists:
            # --- Classified as a patch: diff against base content (BP-2).
            existing = _existing_patches_for_rel(patches_dir, rel)

            # BP-7: refuse to bake unresolved conflict markers into a patch
            markers = _working_file_marker_lines(working_path)
            if markers:
                raise RuntimeError(
                    f'{rel}: working copy contains conflict markers at line(s) '
                    f'{markers} — resolve them first (unfinished --rebase-base?). '
                    f'Refusing to bake conflict markers into a patch '
                    f'(PROPOSED_SPEC_CHANGES.md, BP-7).')

            patch_text = generate_patch(base_bytes, working_path, rel,
                                        project_root, base_commit,
                                        context=context)

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

    if written or files_written or removed or files_removed or base_initialized:
        save_manifest(pkgdir_abs, manifest)

    return {
        'written': written,
        'files_written': files_written,
        'removed': removed,
        'files_removed': files_removed,
        'warnings': warnings,
    }


def materialize(pkgdir, rel, project_root):
    """BP-5: write the upstream file <rel> maps to (src/c47/<rel>, or
    src/<rel> for a SIBLING_ROOTS rel) AS OF THE RECORDED BASE COMMIT
    into the flat working area at <pkgdir>/<rel>. Initializes the base
    to HEAD (BP-3) if the package has none yet — saving the manifest in
    that case. Returns the base commit used."""
    pkgdir_abs = os.path.join(project_root, pkgdir)
    os.makedirs(pkgdir_abs, exist_ok=True)

    manifest = load_manifest(pkgdir_abs)
    warnings = []
    base_commit, initialized = ensure_base_commit(
        manifest, project_root, warnings)
    if initialized:
        save_manifest(pkgdir_abs, manifest)
    for w in warnings:
        print(f'warning: {w}', file=sys.stderr)

    working_path = os.path.join(pkgdir_abs, *rel.split('/'))
    if os.path.exists(working_path):
        raise RuntimeError(
            f'{rel}: working copy already exists at {working_path} — '
            f'refusing to overwrite your edits (delete it first if you '
            f'really want a fresh copy).')

    content = base_file_content(project_root, base_commit, rel)
    if content is None:
        raise RuntimeError(
            f'{rel}: does not exist at the recorded base commit '
            f'{base_commit}. If upstream added it recently, advance the '
            f'base first (--rebase-base).')

    dirname = os.path.dirname(working_path)
    if dirname:
        os.makedirs(dirname, exist_ok=True)
    with open(working_path, 'wb') as f:
        f.write(content)

    print(f'materialized {rel} at base {base_commit[:12]} -> '
          f'{pkgdir}/{rel}')
    return base_commit


def _atomic_replace_file(working_path, new_bytes, file_mode):
    """Atomically replace *working_path* with *new_bytes* using a
    same-directory temporary file plus os.replace, preserving *file_mode*."""
    dir_name = os.path.dirname(working_path)
    fd, tmp_path = tempfile.mkstemp(dir=dir_name)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(new_bytes)
        os.chmod(tmp_path, file_mode)
        os.replace(tmp_path, working_path)
    except BaseException:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass
        raise


def rebase_base(pkgdir, new_base_ref, project_root):
    """BP-6: advance the package's recorded base to *new_base_ref*
    (a committish; 'HEAD' by default from the CLI), three-way-merging
    every working file from old-base content onto new-base content
    via `git merge-file`. Pre-scans and fails fast BEFORE mutating
    anything. Plan-then-commit: all merges are planned before any file
    is written; if any install fails, all installed files are rolled
    back and the manifest base is left unchanged.

    Returns dict: {'old_base', 'new_base',
    'fast_forwarded': [rels], 'merged': [rels], 'conflicted': [rels],
    'untouched': [rels], 'warnings': [drift warnings before rebase]}."""
    r = subprocess.run(
        ['git', 'rev-parse', f'{new_base_ref}^{{commit}}'],
        capture_output=True, text=True, cwd=project_root)
    if r.returncode != 0 or not r.stdout.strip():
        raise RuntimeError(
            f'cannot resolve {new_base_ref!r} as a commit in '
            f'{project_root!r}')
    new_base = r.stdout.strip()

    pkgdir_abs = os.path.join(project_root, pkgdir)
    manifest = load_manifest(pkgdir_abs)
    old_base = manifest.get('base_commit')

    warnings = _scan_drift_for_rebase(pkgdir_abs)

    empty_result = {
        'old_base': old_base,
        'new_base': new_base,
        'fast_forwarded': [],
        'merged': [],
        'conflicted': [],
        'untouched': [],
        'warnings': warnings,
    }

    if old_base is None:
        manifest['base_commit'] = new_base
        save_manifest(pkgdir_abs, manifest)
        return empty_result

    validate_base_commit(project_root, old_base)

    if old_base == new_base:
        return empty_result

    # --- Pre-scan pass (no mutation) ---
    merge_candidates = []
    untouched = []
    for rel in list_working_files(pkgdir_abs):
        old_bytes = base_file_content(project_root, old_base, rel)
        new_bytes = base_file_content(project_root, new_base, rel)

        if old_bytes is None and new_bytes is None:
            untouched.append(rel)
        elif old_bytes is None and new_bytes is not None:
            raise RuntimeError(
                f'{rel}: exists at new base {new_base[:12]} but not at '
                f'old base {old_base[:12]} — upstream added this file at '
                f'a path this package also created. Nothing has been '
                f'modified. Advance the base differently or remove the '
                f'working copy manually.')
        elif old_bytes is not None and new_bytes is None:
            raise RuntimeError(
                f'{rel}: existed at old base {old_base[:12]} but has been '
                f'deleted at new base {new_base[:12]}. Nothing has been '
                f'modified.')
        else:
            working_path = os.path.join(pkgdir_abs, *rel.split('/'))
            with open(working_path, 'rb') as f:
                working_bytes = f.read()
            merge_candidates.append((rel, old_bytes, new_bytes, working_path,
                                     working_bytes))

    # --- Planning pass (no mutation) ---
    plan = []
    fast_forwarded = []
    merged = []
    conflicted = []

    for rel, old_bytes, new_bytes, working_path, working_bytes in merge_candidates:
        if working_bytes == old_bytes:
            plan.append((rel, new_bytes, working_bytes, working_path,
                         os.stat(working_path).st_mode, 'fast_forward'))
            fast_forwarded.append(rel)
        else:
            cur_tmp = None
            old_tmp = None
            new_tmp = None
            try:
                cur_fd, cur_tmp = tempfile.mkstemp()
                with os.fdopen(cur_fd, 'wb') as f:
                    f.write(working_bytes)

                old_fd, old_tmp = tempfile.mkstemp()
                with os.fdopen(old_fd, 'wb') as f:
                    f.write(old_bytes)

                new_fd, new_tmp = tempfile.mkstemp()
                with os.fdopen(new_fd, 'wb') as f:
                    f.write(new_bytes)

                result = subprocess.run(
                    ['git', 'merge-file', '-p', '-L', 'working', '-L', 'base',
                     '-L', 'upstream', cur_tmp, old_tmp, new_tmp],
                    capture_output=True, cwd=project_root)

                if result.returncode < 0:
                    raise RuntimeError(
                        f'git merge-file failed for {rel} (signal '
                        f'{result.returncode}): {result.stderr.decode()!r}')
                elif result.returncode >= 128:
                    raise RuntimeError(
                        f'git merge-file fatal error for {rel} (exit code '
                        f'{result.returncode}): {result.stderr.decode()!r}')
                elif result.returncode == 0:
                    plan.append((rel, result.stdout, working_bytes,
                                 working_path,
                                 os.stat(working_path).st_mode, 'merged'))
                    merged.append(rel)
                else:
                    plan.append((rel, result.stdout, working_bytes,
                                 working_path,
                                 os.stat(working_path).st_mode, 'conflicted'))
                    conflicted.append(rel)
            finally:
                for tmp in (cur_tmp, old_tmp, new_tmp):
                    if tmp:
                        os.unlink(tmp)

    # --- Commit phase (atomic per-file, with rollback) ---
    installed = []
    original_err = None
    try:
        for rel, proposed_bytes, original_bytes, working_path, file_mode, _cat in plan:
            _atomic_replace_file(working_path, proposed_bytes, file_mode)
            installed.append((rel, working_path, original_bytes, file_mode))

        manifest['base_commit'] = new_base
        save_manifest(pkgdir_abs, manifest)

    except Exception as e:
        original_err = e
        restore_failures = []
        for rel, working_path, original_bytes, file_mode in installed:
            try:
                _atomic_replace_file(working_path, original_bytes, file_mode)
            except Exception as restore_err:
                restore_failures.append((rel, str(restore_err)))

        if restore_failures:
            fail_str = '; '.join(
                f'{rel}: {err}' for rel, err in restore_failures)
            raise RuntimeError(
                f'Rebase commit failed (original: {original_err!r}) and '
                f'rollback incomplete — could not restore: {fail_str}')
        raise original_err

    return {
        'old_base': old_base,
        'new_base': new_base,
        'fast_forwarded': fast_forwarded,
        'merged': merged,
        'conflicted': conflicted,
        'untouched': untouched,
        'warnings': warnings,
    }


def main():
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description='Refresh package patches from working area.')
    parser.add_argument('pkgdir', help='Package directory relative to root')
    mut_group = parser.add_mutually_exclusive_group()
    mut_group.add_argument(
        '--materialize', metavar='REL',
        help='Materialize src/c47/<REL> at base commit into working area '
             '(BP-5)')
    mut_group.add_argument(
        '--rebase-base', nargs='?', const='HEAD', default=None,
        metavar='COMMIT',
        help='Rebase the package base to COMMIT (default HEAD) using '
             'three-way merge (BP-6)')
    args = parser.parse_args()

    pkgdir = args.pkgdir.rstrip('/')
    project_root = os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

    if args.materialize:
        try:
            materialize(pkgdir, args.materialize, project_root)
        except RuntimeError as e:
            print(f'error: {e}', file=sys.stderr)
            sys.exit(1)
        return

    if args.rebase_base is not None:
        try:
            result = rebase_base(pkgdir, args.rebase_base, project_root)
        except RuntimeError as e:
            print(f'error: {e}', file=sys.stderr)
            sys.exit(1)

        for w in result['warnings']:
            print(f'warning: {w}', file=sys.stderr)

        if result['old_base'] is None:
            print(f"base initialized to {result['new_base'][:12]}")
            return

        if result['old_base'] == result['new_base']:
            print(f"base already at {result['new_base'][:12]} — "
                  f"nothing to do")
            return

        for rel in result['fast_forwarded']:
            print(f'fast-forwarded {rel}')
        for rel in result['merged']:
            print(f'merged {rel}')
        for rel in result['conflicted']:
            print(f'CONFLICT in {rel} — resolve the markers, then '
                  f're-run refresh')
        print(f"base: {result['old_base'][:12]} -> "
              f"{result['new_base'][:12]}")
        return

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
