#!/usr/bin/env python3
"""Patch-generation (refresh) tool for the patch-based package overlay system.

Authoring-time CLI:

    python3 tools/pkg_patch_refresh.py <pkgdir> <rel>

e.g.    python3 tools/pkg_patch_refresh.py packages/my-pkg keyboard.c

<pkgdir> is the package directory relative to the project root (the same
form used in -DCUSTOM_PKG), <rel> the target path relative to src/c47/.

Diffs the materialized working file (<pkgdir>/<rel>) against the real
upstream file (src/c47/<rel>) at function-boundary granularity (libclang
via pkg_patch_extract) and writes one .patch file per changed function
under <pkgdir>/patches/, named <NNN>-<rel_encoded>.patch per the storage
convention (custom_package/README.md).

Refresh manages the full patch set for (pkgdir, rel): unchanged-again
functions get their stale patch removed, re-changed functions keep their
existing ordinal (the patch file is rewritten in place).

Loud-failure rules (PROPOSED_SPEC_CHANGES.md):
- Any change outside a function body (globals, #defines, structs,
  added/removed functions, comments between functions) is a fatal error
  directing the author to the whole-file override mechanism (§8) — never
  a silently mis-scoped patch.
- The pre-image blob SHA written into each patch's index line must be a
  git object resolvable in this repository (required for git apply -3
  three-way fallback, §5); if not, refresh aborts.

libclang (clang.cindex) is imported transitively through
pkg_patch_extract — this is an authoring-time dependency only (§4,
ratified). Build-time tooling must never import this module.
"""
import os
import re
import subprocess
import sys
import tempfile

# Ensure the tools directory is on sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import decode_patch_filename, parse_patch_target
from pkg_patch_extract import list_function_ranges, function_at_line


def run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def _read_lines(path):
    with open(path, 'r') as f:
        return f.readlines()


# ---------------------------------------------------------------------------
# Step (a) — detection: which functions differ (string-compare of bodies)
# ---------------------------------------------------------------------------

def detect_changed_functions(upstream_path, materialized_path,
                             compile_commands_json_path, flags_file=None):
    """Compare function bodies between upstream and materialized files.

    Both files are parsed with the *upstream* file's compile flags
    (flags_file defaults to upstream_path), since the materialized copy
    has no compile_commands.json entry of its own.

    Returns a dict:
      'changed': function names present in both files whose extracted
                 bodies differ (string comparison), sorted by upstream
                 start line;
      'added':   names only in the materialized file;
      'removed': names only in the upstream file;
      'up_ranges' / 'mat_ranges': the raw range lists.
    """
    if flags_file is None:
        flags_file = upstream_path

    up_ranges = list_function_ranges(upstream_path,
                                     compile_commands_json_path,
                                     flags_file=flags_file)
    mat_ranges = list_function_ranges(materialized_path,
                                      compile_commands_json_path,
                                      flags_file=flags_file)

    up_lines = _read_lines(upstream_path)
    mat_lines = _read_lines(materialized_path)

    up_map = {name: (start, end) for name, start, end in up_ranges}
    mat_map = {name: (start, end) for name, start, end in mat_ranges}

    changed = []
    for name, (start, end) in up_map.items():
        if name not in mat_map:
            continue
        m_start, m_end = mat_map[name]
        up_body = ''.join(up_lines[start - 1:end])
        mat_body = ''.join(mat_lines[m_start - 1:m_end])
        if up_body != mat_body:
            changed.append(name)

    changed.sort(key=lambda n: up_map[n][0])

    return {
        'changed': changed,
        'added': sorted(set(mat_map) - set(up_map)),
        'removed': sorted(set(up_map) - set(mat_map)),
        'up_ranges': up_ranges,
        'mat_ranges': mat_ranges,
    }


# ---------------------------------------------------------------------------
# Totality check — every difference must live inside a changed function
# ---------------------------------------------------------------------------

def _reconstruct(up_lines, mat_lines, up_map, mat_map, changed):
    """Upstream lines with each changed function's body replaced by the
    materialized version.  Replacements run bottom-up so earlier line
    numbers stay valid."""
    result = list(up_lines)
    for name in sorted(changed, key=lambda n: up_map[n][0], reverse=True):
        u_start, u_end = up_map[name]
        m_start, m_end = mat_map[name]
        result[u_start - 1:u_end] = mat_lines[m_start - 1:m_end]
    return result


# ---------------------------------------------------------------------------
# Steps (b)/(c) — per-function unified diff generation
# ---------------------------------------------------------------------------

def _strip_to_hunks(raw_diff):
    """Drop everything before the first @@ line of a git diff."""
    lines = raw_diff.split('\n')
    for i, line in enumerate(lines):
        if line.startswith('@@'):
            body = '\n'.join(lines[i:])
            if not body.endswith('\n'):
                body += '\n'
            return body
    return ''


def _generate_function_patch(rel, fn_name, up_lines, mat_lines,
                             up_map, mat_map, pre_sha, context=3):
    """Unified diff for exactly one changed function, as full git-format
    patch text (git apply -3 compatible: diff --git header + index line
    with a real pre-image blob SHA).

    The diff is taken between the pristine upstream file and a synthetic
    copy in which ONLY fn_name's body is replaced — so hunks carry
    correct upstream line numbers and never touch other functions.
    """
    synthetic = _reconstruct(up_lines, mat_lines, up_map, mat_map, [fn_name])

    with tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                     delete=False) as f_old:
        f_old.writelines(up_lines)
        old_path = f_old.name
    with tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                     delete=False) as f_new:
        f_new.writelines(synthetic)
        new_path = f_new.name

    try:
        raw = run(['git', 'diff', '--no-index', f'-U{context}',
                   old_path, new_path]).stdout
        post_sha = run(['git', 'hash-object', new_path]).stdout.strip()
    finally:
        os.unlink(old_path)
        os.unlink(new_path)

    hunks = _strip_to_hunks(raw)
    if not hunks:
        return None

    return (
        f'diff --git a/src/c47/{rel} b/src/c47/{rel}\n'
        f'index {pre_sha}..{post_sha} 100644\n'
        f'--- a/src/c47/{rel}\n'
        f'+++ b/src/c47/{rel}\n'
        f'{hunks}'
    )


# ---------------------------------------------------------------------------
# Existing-patch bookkeeping (ordinal reuse, stale removal)
# ---------------------------------------------------------------------------

def _first_changed_orig_line(patch_text):
    """1-indexed original-file line number of the first +/- change in the
    patch, or None if the patch has no change lines."""
    orig_line = None
    for line in patch_text.split('\n'):
        m = re.match(r'^@@ -(\d+)(?:,\d+)? \+\d+(?:,\d+)? @@', line)
        if m:
            orig_line = int(m.group(1))
            continue
        if orig_line is None:
            continue
        if line.startswith('-') and not line.startswith('---'):
            return orig_line
        if line.startswith('+') and not line.startswith('+++'):
            # pure insertion: attribute to the original line it follows
            return orig_line
        if line.startswith(' ') or line == '':
            orig_line += 1
    return None


def _existing_patches_for_rel(patches_dir, rel, up_ranges):
    """Map existing patches in patches_dir targeting rel to functions.

    Returns (fn_to_entry, unattributed) where fn_to_entry maps function
    name -> {'ordinal', 'filename'} and unattributed is a list of
    filenames targeting rel whose first change can't be placed inside
    any current upstream function (stale after upstream drift; refresh
    regenerates the whole set for rel, so these get removed).
    """
    fn_to_entry = {}
    unattributed = []
    if not os.path.isdir(patches_dir):
        return fn_to_entry, unattributed

    for fname in sorted(os.listdir(patches_dir)):
        if not fname.endswith('.patch'):
            continue
        try:
            ordinal, rel_from_name = decode_patch_filename(fname)
        except ValueError:
            continue
        if rel_from_name != rel:
            continue
        path = os.path.join(patches_dir, fname)
        with open(path, 'r') as f:
            content = f.read()
        line = _first_changed_orig_line(content)
        fn = function_at_line(up_ranges, line) if line is not None else None
        if fn is None:
            unattributed.append(fname)
        else:
            fn_to_entry[fn[0]] = {'ordinal': ordinal, 'filename': fname}
    return fn_to_entry, unattributed


# ---------------------------------------------------------------------------
# Step (d) — the refresh command
# ---------------------------------------------------------------------------

def refresh(pkgdir, rel, project_root, compile_commands_json_path=None,
            context=3):
    """Regenerate the function-level patch set for (pkgdir, rel).

    Args:
        pkgdir: package directory relative to project_root
            (e.g. 'packages/my-pkg' — the same form used in CUSTOM_PKG).
        rel: target path relative to src/c47/ (e.g. 'keyboard.c').
        project_root: repository root.
        compile_commands_json_path: defaults to
            project_root/build.sim/compile_commands.json (vanilla build).
        context: unified-diff context lines (default -U3 per §1).

    Returns the list of patch filenames written.
    """
    src_path = os.path.join(project_root, 'src', 'c47', rel)
    materialized_path = os.path.join(project_root, pkgdir, rel)
    patches_dir = os.path.join(project_root, pkgdir, 'patches')

    if not os.path.isfile(src_path):
        raise FileNotFoundError(f'no upstream file at src/c47/{rel}')
    if not os.path.isfile(materialized_path):
        raise FileNotFoundError(
            f'no materialized working file at {pkgdir}/{rel} — '
            f'copy src/c47/{rel} there first and edit it')

    if compile_commands_json_path is None:
        compile_commands_json_path = os.path.join(
            project_root, 'build.sim', 'compile_commands.json')

    detection = detect_changed_functions(
        src_path, materialized_path, compile_commands_json_path,
        flags_file=src_path)

    if detection['added'] or detection['removed']:
        raise RuntimeError(
            f'{rel}: function set differs from upstream '
            f'(added: {detection["added"] or "none"}, '
            f'removed: {detection["removed"] or "none"}). '
            f'Adding or removing functions is not a function-body patch — '
            f'use a whole-file override (pkg_override_sources) instead; '
            f'the two mechanisms are mutually exclusive per file (§8).')

    up_lines = _read_lines(src_path)
    mat_lines = _read_lines(materialized_path)
    up_map = {n: (s, e) for n, s, e in detection['up_ranges']}
    mat_map = {n: (s, e) for n, s, e in detection['mat_ranges']}
    changed = detection['changed']

    # Totality check: replacing exactly the changed function bodies must
    # reproduce the materialized file byte-for-byte. Anything left over
    # is a change outside every function boundary (global, #define,
    # struct, inter-function comment, ...) — fatal per §8.
    reconstructed = _reconstruct(up_lines, mat_lines, up_map, mat_map,
                                 changed)
    if ''.join(reconstructed) != ''.join(mat_lines):
        raise RuntimeError(
            f'{rel}: changes found outside every function boundary '
            f'(globals, #defines, macros, structs/typedefs, or code '
            f'between functions). This file needs a whole-file override '
            f'(pkg_override_sources), not a function-level patch — the '
            f'two mechanisms are mutually exclusive per file (§8).')

    fn_to_entry, unattributed = _existing_patches_for_rel(
        patches_dir, rel, detection['up_ranges'])

    if not changed:
        stale = sorted(set(e['filename'] for e in fn_to_entry.values())
                       | set(unattributed))
        for fname in stale:
            os.unlink(os.path.join(patches_dir, fname))
            print(f'removed stale {fname} (no remaining changes)')
        print(f'no function changes between src/c47/{rel} and '
              f'{pkgdir}/{rel} — nothing to refresh')
        return []

    # Pre-image blob SHA — must be a real object in this repo or the
    # git apply -3 ancestry assumption (§5) is broken from the start.
    pre_sha = run(['git', 'hash-object', src_path],
                  cwd=project_root).stdout.strip()
    check = run(['git', 'cat-file', '-e', pre_sha], cwd=project_root)
    if check.returncode != 0:
        raise RuntimeError(
            f'pre-image blob {pre_sha} for src/c47/{rel} is not a '
            f'resolvable git object — src/c47/{rel} has uncommitted '
            f'modifications? Refresh must run against committed upstream '
            f'content so git apply -3 can resolve the base blob (§5).')

    # Assign ordinals: reuse for functions that already have a patch,
    # fresh (max+10 steps) for new ones, deterministic by upstream order.
    existing_ordinals = [e['ordinal'] for e in fn_to_entry.values()]
    next_ordinal = (max(existing_ordinals) if existing_ordinals else 0) + 10

    written = []
    kept_filenames = set()
    os.makedirs(patches_dir, exist_ok=True)

    for fn_name in changed:
        if fn_name in fn_to_entry:
            ordinal = fn_to_entry[fn_name]['ordinal']
        else:
            ordinal = next_ordinal
            next_ordinal += 10

        patch_text = _generate_function_patch(
            rel, fn_name, up_lines, mat_lines, up_map, mat_map, pre_sha,
            context=context)
        if patch_text is None:
            continue

        encoded_rel = rel.replace('/', '__')
        out_name = f'{ordinal:03d}-{encoded_rel}.patch'
        with open(os.path.join(patches_dir, out_name), 'w') as f:
            f.write(patch_text)
        kept_filenames.add(out_name)
        written.append(out_name)
        print(f'wrote {out_name} (function: {fn_name})')

    # Remove patches for rel that no longer correspond to a changed
    # function (function reverted to upstream, or unattributable).
    stale = (set(e['filename'] for e in fn_to_entry.values())
             | set(unattributed)) - kept_filenames
    for fname in sorted(stale):
        os.unlink(os.path.join(patches_dir, fname))
        print(f'removed stale {fname}')

    return written


def main():
    """CLI entry point."""
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <pkgdir> <rel>\n'
              f'  e.g. {sys.argv[0]} packages/my-pkg keyboard.c',
              file=sys.stderr)
        sys.exit(1)

    pkgdir = sys.argv[1].rstrip('/')
    rel = sys.argv[2]

    project_root = os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

    try:
        refresh(pkgdir, rel, project_root)
    except (FileNotFoundError, RuntimeError) as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
