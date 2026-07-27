#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
"""Resolve c47_src with source overrides, or build a shadow tree for custom packages.

Source mode (default):
  resolve_c47_src.py <meson.build> [override1,override2,...]
  Each override is "pkgdir:relative/path" (e.g. "custom_package:statusBar.c").
  The ':' separator avoids ambiguity with nested package directories.
  Outputs one file path per line to stdout (resolved from project root):
    For overrides: the override path (e.g. "custom_package/statusBar.c")
    For originals: the upstream path with src/c47/ prefix (e.g. "src/c47/assign.c")
  NOTE: this mode predates the patch-overlay package system and is not
  invoked by the current build (meson.build only ever calls --shadow
  mode below); kept as-is, out of scope for the patch-overlay redesign.

Shadow mode:
  resolve_c47_src.py --shadow <src_c47_meson_build> <project_root> <shadow_dir> [pkgdir ...]
  Builds a shadow directory containing symlinks to every file under src/c47/,
  overlays each active package's patches/ and files/ content on top (see
  "Patch-overlay package system" below), and outputs include dirs and
  source list for meson.  Every c47 source compiles from the shadow
  directory so all translation units see the same headers by construction.
  Each positional argument after shadow_dir is a package directory
  (project-root-relative, e.g. "packages/my-pkg"), in -DCUSTOM_PKG order.
  stdout line 1: INCDIRS:<dir1>;<dir2>;...   (shadow dir first)
  stdout lines 2..N: source paths, one per line, all into shadow_dir —
    upstream sources followed by any newly-compiled files/*.c sources
    (both are plain shadow-dir paths; the caller does not need to tell
    them apart, both compile the same way)

Patch-overlay package system (PROPOSED_SPEC_CHANGES.md, revision 2):
  A package directory (project-root-relative, e.g. "packages/my-pkg")
  contains exactly two subdirectories the resolver reads — no
  meson.build inside a package directory is read or evaluated, no
  declarations of any kind:
    <pkgdir>/patches/<NNN>-<rel_encoded>.patch
        A whole-file `git diff` against src/c47/<rel> (no restriction
        on what kind of change). Auto-discovered by globbing; every
        *.patch file found is applied — see collect_patch_stacks in
        pkg_patch_apply.py. Applied via `git apply -3` against a
        freshly materialized copy of the current upstream file, with
        an unconditional post-apply conflict-marker scan (see
        pkg_patch_apply.apply_patch_stack). Cumulative across packages,
        ordered by (numeric ordinal, -DCUSTOM_PKG list position).
    <pkgdir>/files/<rel>
        A genuinely new file with no upstream counterpart, stored
        whole, path-mirrored. Auto-discovered by walking files/
        recursively — see collect_new_files in pkg_patch_apply.py.
        Copied directly into the shadow tree; *.c entries are also
        added to the compiled source list (emitted as NEWSRC: lines).
  Both are validated, and the shadow tree is wiped/rebuilt, ONLY after
  validation succeeds — a failing configure leaves any existing shadow
  tree untouched (same F9 sentinel-gate discipline as always).

  This module (resolve_c47_src.py) and pkg_patch_apply.py/
  pkg_patch_common.py have NO libclang dependency — plain `git diff`,
  not AST-based extraction; there is nothing to wall off.

Warnings go to stderr and are captured by meson.
"""
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pkg_patch_apply import (
    PatchApplyError,
    apply_patch_stack,
    collect_new_files,
    collect_patch_stacks,
)


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


def do_shadow(meson_build, project_root, shadow_dir, pkg_list,
              gen_lists=False):
    """Shadow-tree mode: build symlink tree, overlay each active
    package's auto-discovered patches/ and files/ content, emit paths.

    pkg_list: package directories (project-root-relative), in
    -DCUSTOM_PKG order.
    """

    # --- F9/F10: validate shadow_dir before ANY mutation -------------------
    assert_shadow_dir(shadow_dir)

    with open(meson_build) as f:
        content = f.read()

    content_clean = strip_comments(content)

    # --- Auto-discover and validate, BEFORE any shadow-tree mutation -------
    try:
        patch_stacks = collect_patch_stacks(pkg_list, project_root)
        new_files = collect_new_files(pkg_list, project_root)
    except PatchApplyError as e:
        print(f'ERROR: {e}', file=sys.stderr)
        sys.exit(1)

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

    # --- Apply patch stacks (cumulative, ordered) ---------------------------
    for rel in sorted(patch_stacks):
        stack = patch_stacks[rel]
        upstream_file = os.path.join(src_c47_dir, *rel.split('/'))
        dst_path = os.path.join(shadow_dir, rel)

        # --- F11/F10 containment, same guards as the walk above ------------
        assert_contained(upstream_file, src_c47_dir,
                         label=f'upstream_file for patch target "{rel}"')
        assert_contained(os.path.dirname(dst_path), shadow_dir,
                         label=f'dst_path for patch target "{rel}"')

        # The shadow entry is currently a symlink INTO src/c47/ — remove
        # it before writing so the patched result never goes through
        # the link to the real upstream file.
        if os.path.lexists(dst_path):
            print(f'REMOVING: {dst_path}', file=sys.stderr)
            os.remove(dst_path)

        try:
            final = apply_patch_stack(rel, stack, project_root, dst_path)
        except PatchApplyError as e:
            print(f'ERROR: {e}', file=sys.stderr)
            sys.exit(1)

        # --- F15 extension: net-identical patch stack is a dead shadow -
        with open(upstream_file, 'r') as uf:
            if uf.read() == final:
                print(f'CUSTOM_PKG patches for "{rel}": net result is '
                      f'byte-identical to upstream (dead shadow — the '
                      f'patch stack cancels itself out)', file=sys.stderr)

        print(f'CUSTOM_PKG patched "{rel}": {len(stack)} patch(es) '
              f'applied', file=sys.stderr)

    # --- Copy new files (files/*), track newly-compiled sources ------------
    new_source_rels = []
    for rel in sorted(new_files):
        pkgdir, abs_path = new_files[rel]
        dst_path = os.path.join(shadow_dir, rel)

        assert_contained(os.path.dirname(dst_path), shadow_dir,
                         label=f'dst_path for new file "{rel}"')

        if os.path.lexists(dst_path):
            print(f'REMOVING: {dst_path}', file=sys.stderr)
            os.remove(dst_path)
        link_or_copy(abs_path, dst_path)

        print(f'CUSTOM_PKG new file "{rel}" from {pkgdir}', file=sys.stderr)
        if rel.endswith('.c'):
            new_source_rels.append(rel)

    # Emit output
    print(inc_dirs_line)
    for s in upstream:
        print(os.path.join(shadow_dir, s))
    for rel in sorted(new_source_rels):
        print(os.path.join(shadow_dir, rel))

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
        gen_lists = '--gen-lists' in sys.argv
        args = [a for a in sys.argv[1:]
                if a not in ('--shadow', '--gen-lists')]
        do_shadow(args[0], args[1], args[2],
                  args[3:] if len(args) > 3 else [],
                  gen_lists)
        sys.exit(0)

    # --- Source override mode (default) — predates the patch-overlay
    # package system, not invoked by the current build, out of scope
    # for the patch-overlay redesign. Unchanged. ---
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
