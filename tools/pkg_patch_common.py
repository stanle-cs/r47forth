"""Shared parsing / validation helpers for patch-based package overlays.

Used by both the build-time resolver (resolve_c47_src.py) and the
future authoring tool (pkg_patch_extract.py).  Contains NO libclang
imports — those belong only in authoring tooling (§4, ratified).
"""
import os
import re


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
    return (ordinal, rel)


# ---------------------------------------------------------------------------
# Patch-header parsing  (§2 – explicit override-target declaration)
# ---------------------------------------------------------------------------

_PLUS_PLUS_RE = re.compile(r'^\+\+\+ b/(.+)$')


def parse_patch_target(patch_file_path):
    """Read the patch file, find the first '+++ b/...' header line,
    strip the 'b/' prefix and any leading 'src/c47/', return the bare
    rel path (e.g. 'keyboard.c', 'programming/manage.c').

    Raises ValueError if no '+++ b/...' line is found.
    """
    with open(patch_file_path, 'r') as f:
        for line in f:
            line = line.rstrip('\n').rstrip('\r')
            m = _PLUS_PLUS_RE.match(line)
            if m:
                target = m.group(1)
                if target.startswith('src/c47/'):
                    target = target[len('src/c47/'):]
                return target
    raise ValueError(
        f'no +++ b/... header found in patch file: {patch_file_path!r}')


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

    upstream_file = os.path.join(project_root, 'src', 'c47', rel_from_name)
    if not os.path.isfile(upstream_file):
        raise ValueError(
            f'patch targets {rel_from_name!r} which does not exist in '
            f'src/c47/ ({upstream_file})')

    return rel_from_name
