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
import os
import re
import shutil
import sys


def extract_rel(op):
    return op.split(':', 1)[1] if ':' in op else op


def strip_comments(content):
    """Strip inline comments so regex parsing is not confused."""
    return '\n'.join(line.split('#')[0] for line in content.split('\n'))


def do_shadow(meson_build, project_root, shadow_dir, specs):
    """Shadow-tree mode: build symlink tree, overlay overrides, emit paths."""
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

    # Wipe and recreate shadow directory
    shutil.rmtree(shadow_dir, ignore_errors=True)
    src_c47_dir = os.path.join(project_root, 'src', 'c47')

    # Symlink-or-copy helper (mutable state for fallback)
    copy_state = [os.environ.get('CUSTOM_PKG_SHADOW_COPY') == '1']

    def link_or_copy(src_path, dst_path):
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        try:
            if copy_state[0]:
                shutil.copy2(src_path, dst_path)
            else:
                os.symlink(os.path.abspath(src_path), dst_path)
        except OSError:
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

        if not os.path.isfile(pkg_file):
            print(f'CUSTOM_PKG override "{rel}": not found in package '
                  f'"{pkgdir}" — ignored', file=sys.stderr)
            continue

        if not os.path.isfile(upstream_file):
            print(f'CUSTOM_PKG override "{rel}": does not exist in '
                  f'src/c47/ — ignored', file=sys.stderr)
            continue

        # Replace existing symlink/file in shadow with the override
        dst_path = os.path.join(shadow_dir, rel)
        if os.path.exists(dst_path):
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


# ================================================================
# Entry point
# ================================================================
if __name__ == '__main__':
    if sys.argv[1:2] == ['--shadow']:
        do_shadow(sys.argv[2], sys.argv[3], sys.argv[4],
                  sys.argv[5:] if len(sys.argv) > 5 else [])
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
