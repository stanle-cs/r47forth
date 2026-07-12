"""Build-time patch application for the patch-based package overlay system.

Applies an ordered stack of function-level .patch files to a freshly
materialized copy of the current upstream file via `git apply -3`
(PROPOSED_SPEC_CHANGES.md §5), inside a scratch git repository — the real
working tree (src/c47/) is never touched.

RATIFIED §5: after EVERY `git apply -3` call the result is scanned for
conflict markers as a distinct step, regardless of git's exit status —
`-3` can leave `<<<<<<<`/`=======`/`>>>>>>>` in the output, and a marker
could in principle sit in content git considers successfully merged.
Any marker, or any outright apply failure, raises PatchApplyError — the
configure step must die loudly (§7), never ship a silently-picked
version.

Blob ancestry (§5, verified empirically — see PROPOSED_SPEC_CHANGES.md):
`git apply -3` can only three-way merge if the pre-image blob named in
the patch's `index` line exists in the object database it runs against.
The scratch repository starts empty, so every patch's pre-image blob is
seeded from this repository's odb (`git cat-file blob | git hash-object
-w --stdin`) when resolvable. A missing pre-image blob is not fatal by
itself — `git apply` still direct-applies when context matches — but
under drift it downgrades a mergeable situation to an outright failure,
which is loud, not silent.

This module is imported by the build-time resolver: it must NEVER import
libclang/clang.cindex or pkg_patch_extract (§4, ratified — enforced by
test_pkg_patch_apply.py and the resolver's own import assertion).
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import (
    decode_patch_filename,
    parse_patch_target,
    validate_patch_declaration,
)


class PatchApplyError(Exception):
    """Loud failure applying a package patch stack."""


_CONFLICT_MARKER_RE = re.compile(r'^(<{7}|={7}|>{7})', re.MULTILINE)

# refresh writes full 40-char SHAs; accept abbreviated ones too (a
# hand-maintained patch from plain `git diff`) — _seed_blob resolves
# the abbreviation against the source repo before seeding.
_INDEX_LINE_RE = re.compile(r'^index ([0-9a-f]{7,64})\.\.([0-9a-f]{7,64})',
                            re.MULTILINE)


def run(cmd, cwd=None, input_text=None):
    """Run a command, capturing output."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd,
                          input=input_text)


def scan_conflict_markers(text):
    """Return the 1-indexed line numbers of conflict-marker lines
    (<<<<<<< / ======= / >>>>>>> at column 0) in *text*."""
    hits = []
    for i, line in enumerate(text.split('\n'), start=1):
        if _CONFLICT_MARKER_RE.match(line):
            hits.append(i)
    return hits


def _patch_pre_image_sha(patch_path):
    """The pre-image blob SHA recorded in the patch's index line, or
    None if the patch has no full index line."""
    with open(patch_path, 'r') as f:
        m = _INDEX_LINE_RE.search(f.read())
    return m.group(1) if m else None


def _seed_blob(sha, source_repo, scratch_repo):
    """Copy blob *sha* (possibly abbreviated) from source_repo's odb
    into scratch_repo's odb.  Returns True on success, False if the
    blob is unresolvable in source_repo."""
    full = run(['git', 'rev-parse', '--verify', '--quiet',
                sha + '^{blob}'], cwd=source_repo).stdout.strip()
    if not full:
        return False
    content = subprocess.run(['git', 'cat-file', 'blob', full],
                             capture_output=True, cwd=source_repo)
    if content.returncode != 0:
        return False
    written = subprocess.run(['git', 'hash-object', '-w', '--stdin'],
                             input=content.stdout, capture_output=True,
                             cwd=scratch_repo)
    return (written.returncode == 0
            and written.stdout.decode().strip() == full)


def _rel_key(rel):
    """Canonical comparison key for an upstream rel path: separators
    and dot-segments normalized, case folded per platform rules — so
    './ui//tam.c' and 'ui/tam.c' (and, on case-insensitive
    filesystems, 'UI/Tam.c') cannot dodge the §8 exclusivity check by
    formatting alone."""
    return os.path.normcase(os.path.normpath(rel.replace('\\', '/')))


def assert_mutually_exclusive(override_rel_to_pkgs, patch_rel_to_pkgs):
    """§8 (ratified): a given upstream file may be targeted by EITHER
    whole-file override OR function-level patches, never both, across
    all active packages.  Both arguments map rel -> list of pkgdirs.
    Raises PatchApplyError naming every offending rel and the packages
    on both sides.  Must run BEFORE any shadow-tree mutation."""
    overrides = {_rel_key(rel): (rel, pkgs)
                 for rel, pkgs in override_rel_to_pkgs.items()}
    patches = {_rel_key(rel): (rel, pkgs)
               for rel, pkgs in patch_rel_to_pkgs.items()}
    both = sorted(set(overrides) & set(patches))
    if both:
        details = '; '.join(
            f'"{overrides[k][0]}" is whole-file-overridden by '
            f'{sorted(set(overrides[k][1]))} AND function-patched by '
            f'{sorted(set(patches[k][1]))}'
            for k in both)
        raise PatchApplyError(
            f'mutual-exclusivity violation (§8): one mechanism per '
            f'upstream file — {details}. Pick whole-file override OR '
            f'function-level patches for each file.')


def collect_patch_stacks(pkg_patch_specs, project_root):
    """Validate and order every declared patch across all active
    packages (§2 dual-signal, §3 cumulative ordered composition).

    pkg_patch_specs: ordered list of (pkgdir, declared_filenames) —
    pkgdir project-root-relative, in -DCUSTOM_PKG list order;
    declared_filenames from that package's pkg_patch_sources.

    Returns {rel: [absolute patch paths]} with each stack sorted by
    (integer ordinal from the filename, package list index) — the
    earlier-listed package wins ordinal ties (§3). Dict insertion order
    follows first appearance; callers iterate stacks per rel.

    Loud failures (PatchApplyError), never silent skips:
    - a declared patch file that does not exist on disk;
    - a .patch file on disk that is not declared in pkg_patch_sources
      (a typo'd declaration must not silently drop a patch from the
      build — same philosophy as the README's scope warning);
    - a malformed filename (missing <NNN>- prefix, missing .patch);
    - filename/+++-header target mismatch or a target with no upstream
      counterpart (§2, via validate_patch_declaration).

    Duplicate ordinals for the same rel within ONE package cannot
    exist: the filename IS <NNN>-<rel>.patch, so the filesystem forbids
    them. Duplicates across packages are legal ties, broken by package
    order. Ordinals are exactly three digits (decode rejects anything
    else), so integer and string ordering coincide; we sort by the
    parsed integer regardless.
    """
    entries = {}  # rel -> list of (ordinal, pkg_index, path)

    for pkg_index, (pkgdir, declared) in enumerate(pkg_patch_specs):
        patches_dir = os.path.join(project_root, pkgdir, 'patches')
        declared = list(declared)

        on_disk = set()
        if os.path.isdir(patches_dir):
            on_disk = {f for f in os.listdir(patches_dir)
                       if f.endswith('.patch')}
        undeclared = on_disk - set(declared)
        if undeclared:
            raise PatchApplyError(
                f'package {pkgdir}: patch file(s) present in patches/ '
                f'but not declared in pkg_patch_sources: '
                f'{sorted(undeclared)} — declare or remove them '
                f'(a patch must never be silently dropped from the '
                f'build)')

        for fname in declared:
            path = os.path.join(patches_dir, fname)
            if not os.path.isfile(path):
                raise PatchApplyError(
                    f'package {pkgdir}: declared patch {fname!r} not '
                    f'found at {path}')
            try:
                ordinal, _ = decode_patch_filename(fname)
                rel = validate_patch_declaration(pkgdir, fname,
                                                 project_root)
            except ValueError as e:
                raise PatchApplyError(
                    f'package {pkgdir}: invalid patch {fname!r}: {e}')
            entries.setdefault(rel, []).append((ordinal, pkg_index,
                                                path))

    stacks = {}
    for rel, lst in entries.items():
        lst.sort(key=lambda t: (t[0], t[1]))
        stacks[rel] = [path for _, _, path in lst]
    return stacks


def apply_patch_stack(rel, patch_paths, project_root, dest_path,
                      base_content=None):
    """Apply *patch_paths* (already ordered) targeting src/c47/<rel>
    onto a fresh copy of the current upstream file, writing the result
    to *dest_path*.  Returns the final file content.

    base_content overrides the materialized starting content (used by
    drift tests); default is the current src/c47/<rel>.

    Raises PatchApplyError on: outright apply failure, or any conflict
    marker in the result after ANY patch (checked unconditionally,
    independent of git's exit status).
    """
    src_file = os.path.join(project_root, 'src', 'c47', rel)
    if base_content is None:
        if not os.path.isfile(src_file):
            raise PatchApplyError(
                f'cannot materialize {rel}: no upstream file at '
                f'{src_file}')
        with open(src_file, 'r') as f:
            base_content = f.read()

    scratch = tempfile.mkdtemp(prefix='pkg_patch_apply_')
    try:
        for args in (['init', '-q'],
                     ['config', 'user.email', 'pkg@localhost'],
                     ['config', 'user.name', 'pkg-patch-apply']):
            r = run(['git'] + args, cwd=scratch)
            if r.returncode != 0:
                raise PatchApplyError(
                    f'scratch repo setup failed: {r.stderr}')

        target = os.path.join(scratch, 'src', 'c47', rel)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, 'w') as f:
            f.write(base_content)
        run(['git', 'add', '-A'], cwd=scratch)
        run(['git', 'commit', '-q', '-m', 'materialized upstream'],
            cwd=scratch)

        for patch_path in patch_paths:
            # Seed the recorded pre-image blob so -3 has a real base.
            pre_sha = _patch_pre_image_sha(patch_path)
            if pre_sha is not None:
                _seed_blob(pre_sha, project_root, scratch)

            result = run(['git', 'apply', '--3way', patch_path],
                         cwd=scratch)

            with open(target, 'r') as f:
                content = f.read()

            # RATIFIED §5: unconditional marker scan, independent of
            # git apply's exit status.
            markers = scan_conflict_markers(content)
            if markers:
                raise PatchApplyError(
                    f'{os.path.basename(patch_path)} applied against '
                    f'{rel} left conflict markers at line(s) '
                    f'{markers} — unresolved same-function conflict '
                    f'(§7). Full patch path: {patch_path}. '
                    f'git apply stderr: {result.stderr.strip()!r}')

            if result.returncode != 0:
                raise PatchApplyError(
                    f'git apply -3 failed outright for '
                    f'{os.path.basename(patch_path)} against {rel}: '
                    f'{result.stderr.strip()!r} '
                    f'(full patch path: {patch_path})')

            # Keep the scratch index clean so the next patch in the
            # stack three-way-merges against this patch's output.
            run(['git', 'add', '-A'], cwd=scratch)
            run(['git', 'commit', '-q', '-m',
                 f'applied {os.path.basename(patch_path)}'], cwd=scratch)

        os.makedirs(os.path.dirname(os.path.abspath(dest_path)),
                    exist_ok=True)
        with open(target, 'r') as f:
            final = f.read()
        with open(dest_path, 'w') as f:
            f.write(final)
        return final
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
