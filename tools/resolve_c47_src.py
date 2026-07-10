#!/usr/bin/env python3
"""Resolve c47_src with source overrides, or build a shadow tree for custom packages.

Source mode (default):
  resolve_c47_src.py <meson.build> [override1,override2,...]
  Each override is "pkgdir:relative/path" (e.g. "custom_package:statusBar.c").
  The ':' separator avoids ambiguity with nested package directories.
  Outputs one file path per line to stdout (resolved from project root):
    For overrides: the override path (e.g. "custom_package/statusBar.c")
    For originals: the upstream path with src/c47/ prefix (e.g. "src/c47/assign.c")

Shadow mode:
  resolve_c47_src.py --shadow <src_c47_meson_build> <project_root> <shadow_dir> [spec ...]
  Builds a shadow directory containing symlinks to every file under src/c47/,
  overlays package override files (both .c and .h) on top, and outputs include
  dirs and source list for meson.  Every c47 source compiles from the shadow
  directory so all translation units see the same headers by construction.
  Each spec is "pkgdir:relative/path".  The script distinguishes header vs
  source overrides by file extension (.h vs anything else).
  stdout line 1: INCDIRS:<dir1>;<dir2>;...   (shadow dir first)
  stdout lines 2..N: source paths, one per line (all into shadow_dir)

Warnings go to stderr and are captured by meson.
"""
import filecmp
import os
import re
import shutil
import sys


SENTINEL_NAME = 'DO_NOT_EDIT_shadow_tree.txt'


# ---------------------------------------------------------------------------
# Safety helpers  (F9, F10, F11)
# ---------------------------------------------------------------------------

def assert_shadow_dir(shadow_dir):
    """Validate that *shadow_dir* is a plausible custom_pkg_shadow path.

    The directory name must be 'custom_pkg_shadow' and the path must be
    absolute.  Fails with sys.exit(1) on any violation.
    """
    if not shadow_dir or not os.path.isabs(shadow_dir):
        print(f'ERROR: shadow_dir must be a non-empty absolute path: {shadow_dir!r}', file=sys.stderr)
        sys.exit(1)
    shadow_dir = os.path.realpath(shadow_dir)
    if os.path.basename(shadow_dir) != 'custom_pkg_shadow':
        print(f'ERROR: shadow_dir basename is not "custom_pkg_shadow": '
              f'{shadow_dir}', file=sys.stderr)
        sys.exit(1)


def assert_contained(path, parent, label='path'):
    """Assert *path* resolves inside *parent* using os.path.commonpath.

    Both arguments are resolved through os.path.realpath before comparison
    so symlinks and .. components cannot bypass the check.  On violation
    the offending path is printed to stderr and sys.exit(1).
    """
    parent = os.path.realpath(parent)
    resolved = os.path.realpath(path)
    try:
        common = os.path.commonpath([parent, resolved])
    except ValueError:
        # e.g. cross-drive on Windows
        print(f'ERROR: {label} escapes containment: {resolved} '
              f'(expected under {parent})', file=sys.stderr)
        sys.exit(1)
    if common != parent:
        print(f'ERROR: {label} escapes containment: {resolved} '
              f'(expected under {parent})', file=sys.stderr)
        sys.exit(1)
    return resolved


# ---------------------------------------------------------------------------
# Core helpers
# ---------------------------------------------------------------------------

def extract_rel(op):
    return op.split(':', 1)[1] if ':' in op else op


def strip_comments(content):
    """Strip inline comments so regex parsing is not confused."""
    return '\n'.join(line.split('#')[0] for line in content.split('\n'))


def do_shadow(meson_build, project_root, shadow_dir, specs, gen_lists=False):
    """Shadow-tree mode: build symlink tree, overlay overrides, emit paths."""

    # --- F9/F10: validate shadow_dir before ANY mutation -------------------
    assert_shadow_dir(shadow_dir)

    with open(meson_build) as f:
        content = f.read()

    content_clean = strip_comments(content)

    # Parse c47_src = files(...)
    m = re.search(r'c47_src\s*=\s*files\((.*?)\)', content_clean, re.DOTALL)
    if not m:
        print('ERROR: could not parse c47_src from meson.build', file=sys.stderr)
        sys.exit(1)

    upstream = re.findall(r"'([^']+)'", m.group(1))

    # Parse c47_inc = include_directories(...)
    m_inc = re.search(r'c47_inc\s*=\s*include_directories\((.*?)\)', content_clean, re.DOTALL)
    if not m_inc:
        print('ERROR: could not parse c47_inc from meson.build', file=sys.stderr)
        sys.exit(1)

    inc_dirs_raw = re.findall(r"'([^']+)'", m_inc.group(1))
    resolved_incs = [os.path.normpath(os.path.join('src/c47', d)) for d in inc_dirs_raw]

    # Build INCDIRS line: shadow dir first, then upstream include dirs
    inc_dirs_line = 'INCDIRS:' + ';'.join([shadow_dir] + resolved_incs)

    # --- F9: wipe and recreate shadow directory with safety guards ----------
    src_c47_dir = os.path.join(project_root, 'src', 'c47')

    # F9: only delete what we created — sentinel gate
    abs_shadow = os.path.realpath(shadow_dir)
    sentinel = os.path.join(abs_shadow, SENTINEL_NAME)
    if os.path.isdir(abs_shadow) and os.listdir(abs_shadow) and not os.path.isfile(sentinel):
        print(f'ERROR: refusing to delete {abs_shadow}: existing non-empty directory has no shadow-tree sentinel — not created by this script', file=sys.stderr)
        sys.exit(1)
    print(f'REMOVING: {abs_shadow}', file=sys.stderr)
    shutil.rmtree(abs_shadow, ignore_errors=True)
    # F9: verify the directory is actually gone
    if os.path.isdir(abs_shadow):
        print(f'ERROR: rmtree failed — shadow dir still exists: '
              f'{abs_shadow}', file=sys.stderr)
        sys.exit(1)
    # F12: write sentinel immediately after (before walk) so a crash mid-walk
    # doesn't leave a sentinel-less tree that blocks the next run.
    os.makedirs(shadow_dir, exist_ok=True)
    sentinel_path = os.path.join(shadow_dir, SENTINEL_NAME)
    with open(sentinel_path, 'w') as sf:
        sf.write('DO NOT EDIT files in this directory.\n'
                 'This is a generated shadow tree managed by resolve_c47_src.py.\n'
                 'Edits to symlinks here modify upstream source files.\n')

    # Symlink-or-copy helper (mutable state for fallback)
    copy_state = [os.environ.get('CUSTOM_PKG_SHADOW_COPY') == '1']
    copy_warned = [False]

    def link_or_copy(src_path, dst_path):
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        try:
            if copy_state[0]:
                shutil.copy2(src_path, dst_path)
            else:
                os.symlink(os.path.abspath(src_path), dst_path)
        except OSError:
            if not copy_state[0]:
                print('WARNING: symlink not available, using file copies — bare ninja will NOT see', file=sys.stderr)
                print('         source edits; reconfigure required after each change', file=sys.stderr)
            copy_state[0] = True
            shutil.copy2(src_path, dst_path)

    # Walk src/c47/ recursively; symlink every file except meson.build
    for root, dirs, files in os.walk(src_c47_dir):
        for fname in files:
            if fname == 'meson.build':
                continue
            rel = os.path.relpath(os.path.join(root, fname), src_c47_dir)
            link_or_copy(os.path.join(src_c47_dir, rel),
                         os.path.join(shadow_dir, rel))

    # --- Overlay package overrides ---
    override_map = {}
    for spec in specs:
        rel = extract_rel(spec)
        override_map.setdefault(rel, []).append(spec)

    used = set()
    rels_with_match = set()

    for rel, spec_list in override_map.items():
        chosen = spec_list[-1]  # last wins
        pkgdir = chosen.split(':', 1)[0]
        pkg_file = os.path.join(project_root, pkgdir, rel)
        upstream_file = os.path.join(src_c47_dir, rel)

        # --- F4: fatal if override file missing from the package -----------
        if not os.path.isfile(pkg_file):
            print(f'ERROR: CUSTOM_PKG override "{rel}": not found in package '
                  f'"{pkgdir}" ({pkg_file})', file=sys.stderr)
            sys.exit(1)

        # --- F4: fatal if no corresponding upstream file --------------------
        if not os.path.isfile(upstream_file):
            print(f'ERROR: CUSTOM_PKG override "{rel}": does not exist in '
                  f'src/c47/ ({upstream_file})', file=sys.stderr)
            sys.exit(1)

        # --- F11: containment on upstream_file (defence-in-depth) ----------
        assert_contained(upstream_file, src_c47_dir,
                         label=f'upstream_file for override "{rel}"')

        # --- F10: containment on dst_path parent dir -----------------------
        dst_path = os.path.join(shadow_dir, rel)
        assert_contained(os.path.dirname(dst_path), shadow_dir,
                         label=f'dst_path for override "{rel}"')

        # --- F15: byte-identical override warning --------------------------
        if filecmp.cmp(pkg_file, upstream_file, shallow=False):
            print(f'CUSTOM_PKG override "{rel}": byte-identical to upstream '
                  f'(dead shadow — upstream changes to this file are masked)',
                  file=sys.stderr)

        # Replace existing symlink/file in shadow with the override
        if os.path.lexists(dst_path):
            print(f'REMOVING: {dst_path}', file=sys.stderr)
            os.remove(dst_path)
        link_or_copy(pkg_file, dst_path)

        used.add(chosen)
        rels_with_match.add(rel)

        if len(spec_list) > 1:
            print(f'CUSTOM_PKG override "{rel}": duplicated '
                  f'({len(spec_list)} packages override this file, last wins)',
                  file=sys.stderr)

    # Warn about overrides that were never consumed
    warned = set()
    for spec in specs:
        if spec not in used:
            rel = extract_rel(spec)
            if rel in warned:
                continue
            warned.add(rel)
            same_rel = [s for s in specs if extract_rel(s) == rel]
            if len(same_rel) == 1:
                print(f'CUSTOM_PKG override "{rel}": does not match any '
                      f'upstream source — ignored', file=sys.stderr)
            elif rel in rels_with_match:
                print(f'CUSTOM_PKG override "{rel}": duplicated '
                      f'({len(same_rel)} packages override this file, last wins)',
                      file=sys.stderr)
            else:
                print(f'CUSTOM_PKG override "{rel}": {len(same_rel)} packages '
                      f'override this file but none match upstream',
                      file=sys.stderr)

    # Emit output
    print(inc_dirs_line)
    for s in upstream:
        print(os.path.join(shadow_dir, s))

    # --- F1: Emit generator source lists if --gen-lists flag is set ---
    if gen_lists:
        m_gc = re.search(r'generateCatalogs_src\s*=\s*files\((.*?)\)', content_clean, re.DOTALL)
        if m_gc:
            gen_cat_srcs = re.findall(r"'([^']+)'", m_gc.group(1))
            for s in gen_cat_srcs:
                print('GENCAT:' + os.path.join(shadow_dir, s))
        m_gt = re.search(r'generateTestPgms_src\s*=\s*files\((.*?)\)', content_clean, re.DOTALL)
        if m_gt:
            gen_tst_srcs = re.findall(r"'([^']+)'", m_gt.group(1))
            for s in gen_tst_srcs:
                print('GENTST:' + os.path.join(shadow_dir, s))


# ================================================================
# Entry point
# ================================================================
if __name__ == '__main__':
    if '--shadow' in sys.argv[1:]:
        shadow_idx = sys.argv.index('--shadow')
        gen_lists = '--gen-lists' in sys.argv
        non_flag_args = [a for a in sys.argv[1:] if a not in ('--shadow', '--gen-lists')]
        do_shadow(non_flag_args[0], non_flag_args[1], non_flag_args[2],
                  non_flag_args[3:] if len(non_flag_args) > 3 else [],
                  gen_lists)
        sys.exit(0)

    # --- Source override mode (default) — unchanged for H1-H5 ---
    meson_build = sys.argv[1]
    overrides = sys.argv[2:] if len(sys.argv) > 2 else []

    with open(meson_build) as f:
        content = f.read()

    content = '\n'.join(line.split('#')[0] for line in content.split('\n'))

    m = re.search(r'c47_src\s*=\s*files\((.*?)\)', content, re.DOTALL)
    if not m:
        print('ERROR: could not parse c47_src from meson.build', file=sys.stderr)
        sys.exit(1)

    upstream = re.findall(r"'([^']+)'", m.group(1))

    override_map = {}
    for op in overrides:
        rel = extract_rel(op)
        override_map.setdefault(rel, []).append(op)

    used = set()
    rels_with_match = set()

    for s in upstream:
        if s in override_map and override_map[s]:
            chosen = override_map[s].pop()
            used.add(chosen)
            rels_with_match.add(s)
            parts = chosen.split(':', 1)
            print(parts[0] + '/' + parts[1])
        else:
            print(f'src/c47/{s}')

    warned = set()
    for op in overrides:
        if op not in used:
            rel = extract_rel(op)
            if rel in warned:
                continue
            warned.add(rel)
            same_rel = [o for o in overrides if extract_rel(o) == rel]
            if len(same_rel) == 1:
                print(f'CUSTOM_PKG override "{rel}": does not match any '
                      f'upstream source — ignored', file=sys.stderr)
            elif rel in rels_with_match:
                print(f'CUSTOM_PKG override "{rel}": duplicated '
                      f'({len(same_rel)} packages override this file, last wins)',
                      file=sys.stderr)
            else:
                print(f'CUSTOM_PKG override "{rel}": {len(same_rel)} packages '
                      f'override this file but none match upstream',
                      file=sys.stderr)
