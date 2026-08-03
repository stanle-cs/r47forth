# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
"""Shared parsing / validation helpers for patch-based package overlays
(plain-diff design, PROPOSED_SPEC_CHANGES.md revision 2).

Used by both the build-time resolver (resolve_c47_src.py) and the
authoring tool (pkg_patch_refresh.py).  Generic filename/header parsing
only — no dependency on any particular diff granularity.
"""
import os
import re


# ---------------------------------------------------------------------------
# Upstream-root mapping  (T2-A, 2026-08-02)
# ---------------------------------------------------------------------------
# The working area is a flat mirror rooted at src/c47/ — with one deliberate
# extension: a rel path whose FIRST segment names a sibling root below maps
# to src/<rel> instead. This lets a package patch dev-only trees that sit
# beside src/c47 (today: the upstream test driver in src/testSuite/) without
# changing the meaning of any existing rel — src/c47 has no directory named
# like a sibling root, and validate_patch_declaration would reject one.
# Keep this tuple SHORT and deliberate: every entry widens the surface the
# package system can touch.

SIBLING_ROOTS = ('testSuite',)


def upstream_repo_rel(rel):
    """Map a working-area/patch rel path to its repo-relative upstream
    path: 'testSuite/x.c' -> 'src/testSuite/x.c'; anything else ->
    'src/c47/<rel>'."""
    if rel.split('/', 1)[0] in SIBLING_ROOTS:
        return 'src/' + rel
    return 'src/c47/' + rel


def upstream_abs_path(project_root, rel):
    """Absolute filesystem path of the upstream file *rel* maps to."""
    return os.path.join(project_root, *upstream_repo_rel(rel).split('/'))


# ---------------------------------------------------------------------------
# Filename encoding / decoding  (§1 – Storage Format)
# ---------------------------------------------------------------------------

_PATCH_FN_RE = re.compile(r'^(\d{3})-(.+)\.patch$')


def decode_patch_filename(filename):
    """'<NNN>-<rel_encoded>.patch' -> (ordinal: int, rel: str).

    Raises ValueError with a clear message on any malformed filename
    (missing NNN-, missing .patch, empty rel).
    """
    if not filename.endswith('.patch'):
        raise ValueError(
            f'patch filename must end with .patch: {filename!r}')

    stem = filename[:-6]  # strip '.patch'

    m = _PATCH_FN_RE.match(filename)
    if not m:
        raise ValueError(
            f'patch filename must match <NNN>-<rel_encoded>.patch: {filename!r}')

    ordinal = int(m.group(1))
    rel_encoded = m.group(2)

    if not rel_encoded:
        raise ValueError(
            f'patch filename has empty rel after ordinal: {filename!r}')

    rel = rel_encoded.replace('__', '/')

    # Containment at the source: a decoded rel must be a plain relative
    # path strictly below its upstream root (src/c47/, or src/<root>/ for
    # a SIBLING_ROOTS rel). '..' segments, absolute paths (a leading '__'
    # would decode to '/...' and os.path.join would then DISCARD the
    # upstream prefix entirely), '.' segments and empty segments are all
    # fatal here, before any path is ever built.
    parts = rel.split('/')
    if any(p in ('', '.', '..') for p in parts):
        raise ValueError(
            f'patch filename decodes to invalid rel {rel!r} '
            f'(absolute, empty, "." or ".." segment): {filename!r}')

    return (ordinal, rel)


# ---------------------------------------------------------------------------
# Patch-header parsing  (§2 – explicit override-target declaration)
# ---------------------------------------------------------------------------

_PLUS_PLUS_RE = re.compile(r'^\+\+\+ b/(.+)$')


def parse_patch_target(patch_file_path):
    """Read the patch file, find its '+++ b/...' header line, strip the
    'b/' prefix and any leading 'src/c47/', return the bare rel path
    (e.g. 'keyboard.c', 'programming/manage.c').

    Raises ValueError if no '+++ b/...' line is found, or if MORE than
    one is found: a patch targets exactly one upstream file — a second
    diff section would either fail to apply (file absent from the
    scratch repo) or, worse, create a file that is then silently
    discarded, dropping part of the package's edit from the build.
    """
    targets = []
    with open(patch_file_path, 'r') as f:
        for line in f:
            line = line.rstrip('\n').rstrip('\r')
            m = _PLUS_PLUS_RE.match(line)
            if m:
                # Standard unified-diff headers may carry a
                # tab-separated timestamp: '+++ b/file.c\t2026-...'.
                target = m.group(1).split('\t')[0]
                if target.startswith('src/c47/'):
                    target = target[len('src/c47/'):]
                elif (target.startswith('src/')
                      and target[len('src/'):].split('/', 1)[0]
                      in SIBLING_ROOTS):
                    # Sibling-root patch (T2-A): rel keeps its root prefix,
                    # e.g. '+++ b/src/testSuite/testSuite.c' ->
                    # 'testSuite/testSuite.c'.
                    target = target[len('src/'):]
                targets.append(target)
    if not targets:
        raise ValueError(
            f'no +++ b/... header found in patch file: {patch_file_path!r}')
    if len(targets) > 1:
        raise ValueError(
            f'patch file contains {len(targets)} +++ headers '
            f'({targets}) — a package patch must target exactly one '
            f'upstream file: {patch_file_path!r}')
    return targets[0]


# ---------------------------------------------------------------------------
# Cross-check validation  (§2, ratified)
# ---------------------------------------------------------------------------

def validate_patch_declaration(pkgdir, patch_filename, project_root):
    """Cross-check the two independent signals:
      - rel_from_name = decode_patch_filename(patch_filename)[1]
      - rel_from_header = parse_patch_target(full path to the patch file)
    Fatal (raise ValueError with both values in the message) if they
    disagree. Fatal if neither resolves to a real file under
    project_root/src/c47/<rel>. Returns rel on success.
    """
    ordinal, rel_from_name = decode_patch_filename(patch_filename)

    patch_path = os.path.join(project_root, pkgdir, 'patches', patch_filename)
    rel_from_header = parse_patch_target(patch_path)

    if rel_from_name != rel_from_header:
        raise ValueError(
            f'patch target mismatch in {patch_filename!r}: '
            f'filename declares {rel_from_name!r}, '
            f'+++ header declares {rel_from_header!r}')

    upstream_file = upstream_abs_path(project_root, rel_from_name)
    if not os.path.isfile(upstream_file):
        raise ValueError(
            f'patch targets {rel_from_name!r} which does not exist '
            f'upstream ({upstream_file})')

    return rel_from_name
