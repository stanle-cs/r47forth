#!/usr/bin/env python3
"""Patch-generation (`refresh`) tool for the plain-diff package overlay
system (PROPOSED_SPEC_CHANGES.md, revision 2).

Authoring-time CLI:

    python3 tools/pkg_patch_refresh.py <pkgdir>

e.g.    python3 tools/pkg_patch_refresh.py packages/my-pkg

<pkgdir> is the package directory relative to the project root (the same
form used in -DCUSTOM_PKG).

Scans every materialized working file directly under <pkgdir> (recursively,
excluding the 'patches/' and 'files/' subdirectories, and any stray
'meson.build' — packages have no meson.build under revision 2) whose
relative path mirrors an upstream file under src/c47/. For each:

  - If it differs from its upstream counterpart: writes/overwrites
    <pkgdir>/patches/<NNN>-<rel_encoded>.patch (a whole-file `git diff`,
    no restriction on what kind of change it contains — function bodies,
    globals, macros, structs, added/removed functions, all just diff
    output). The ordinal is reused from any existing patch already
    targeting that rel (so re-running refresh after further edits rewrites
    the same file in place); a fresh rel defaults to ordinal 010.
  - If it is now identical to upstream (a reverted edit): deletes any
    existing patch for that rel, so patches/ never accumulates stale
    output from abandoned edits.
  - If it has no upstream counterpart at its mirrored path at all: left
    alone and reported — refresh only diffs EXISTING upstream files; a
    genuinely new file belongs directly under <pkgdir>/files/<rel>
    instead (placed there by the developer, not auto-detected here).

Materialized working copies are an authoring-time convenience — only
patches/ and files/ are meant to be committed.

No libclang dependency anywhere in this module (or the whole pkg_patch_*
tree) — this is plain `git diff`, not AST-based extraction.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import decode_patch_filename

DEFAULT_ORDINAL = 10
_EXCLUDED_TOP_DIRS = ('patches', 'files')


def run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def list_materialized_files(pkgdir_abs):
    """Every regular file directly under pkgdir_abs, recursively,
    excluding the 'patches' and 'files' subdirectories and any
    'meson.build' (packages have no meson.build under revision 2 — one
    present is pre-revision-2 cruft, not a materialized working copy).

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
            if fname == 'meson.build':
                continue
            rel = fname if rel_root == '.' else f'{rel_root}/{fname}'
            result.append(rel.replace(os.sep, '/'))
    return sorted(result)


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


def refresh(pkgdir, project_root, context=3):
    """Regenerate the .patch set under <project_root>/<pkgdir>/patches/
    from every materialized working file directly under <pkgdir>.

    Returns a dict:
      'written': [patch filenames written/overwritten]
      'removed': [patch filenames deleted (reverted edit, or stray
                  duplicate targeting an already-current rel)]
      'new_files': [rels with no upstream counterpart — left alone;
                    reported so the caller/CLI can point the developer
                    at files/ instead]

    Raises RuntimeError (via generate_patch) on a binary file or an
    upstream file with uncommitted modifications — both fatal, named.
    """
    pkgdir_abs = os.path.join(project_root, pkgdir)
    patches_dir = os.path.join(pkgdir_abs, 'patches')
    src_c47_dir = os.path.join(project_root, 'src', 'c47')

    written = []
    removed = []
    new_files = []

    for rel in list_materialized_files(pkgdir_abs):
        materialized_path = os.path.join(pkgdir_abs, *rel.split('/'))
        upstream_path = os.path.join(src_c47_dir, *rel.split('/'))

        if not os.path.isfile(upstream_path):
            new_files.append(rel)
            continue

        existing = _existing_patches_for_rel(patches_dir, rel)
        patch_text = generate_patch(upstream_path, materialized_path, rel,
                                    project_root, context=context)

        if patch_text is None:
            # Reverted edit: remove any patch(es) that used to cover it.
            for _, fname in existing:
                os.unlink(os.path.join(patches_dir, fname))
                removed.append(fname)
            continue

        ordinal = existing[0][0] if existing else DEFAULT_ORDINAL
        encoded_rel = rel.replace('/', '__')
        out_name = f'{ordinal:03d}-{encoded_rel}.patch'
        os.makedirs(patches_dir, exist_ok=True)
        with open(os.path.join(patches_dir, out_name), 'w') as f:
            f.write(patch_text)
        written.append(out_name)

        # Remove stray duplicate patches for the same rel (keep only
        # the one we just (re)wrote), so patches/ always reflects
        # exactly the current materialized state.
        for _, fname in existing[1:]:
            if fname != out_name:
                os.unlink(os.path.join(patches_dir, fname))
                removed.append(fname)

    return {'written': written, 'removed': removed, 'new_files': new_files}


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

    for fname in result['written']:
        print(f'wrote {fname}')
    for fname in result['removed']:
        print(f'removed {fname} (no longer differs from upstream)')
    for rel in result['new_files']:
        print(f'note: {rel} has no upstream counterpart — refresh only '
              f'diffs existing upstream files; place new files directly '
              f'under {pkgdir}/files/{rel} instead', file=sys.stderr)

    if not result['written'] and not result['removed']:
        print(f'no changes under {pkgdir} — patches/ already up to date')


if __name__ == '__main__':
    main()
