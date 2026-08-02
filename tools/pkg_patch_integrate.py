#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
"""Integration session management for the package CLI.

Creates isolated sessions for integrating package changes against a target
upstream ref.  The caller's repository is never modified — a detached
worktree hosts the merge.

PCLI-2: session creation, repository-merge classification, caller
immutability, conflict reporting.
PCLI-3: resume, package rebase, build, cleanup.
PCLI-5: streamed build output, retry, hardened cleanup, snapshot digest,
        SHA validation, fatal-merge distinction.
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_refresh import (  # noqa: E402
    list_working_files, load_manifest, rebase_base, refresh,
)


def _run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def _resolve_commit(project_root, ref):
    """Resolve *ref* to a full 40-char SHA via ``git rev-parse <ref>^{commit}``.

    RuntimeError if the ref cannot be resolved.
    """
    r = _run(['git', 'rev-parse', ref + '^{commit}'], cwd=project_root)
    if r.returncode != 0:
        raise RuntimeError(
            f'cannot resolve ref {ref!r}: {r.stderr.strip()}')
    return r.stdout.strip()


def _validate_target_sha(project_root, target_sha):
    """Validate *target_sha* is 40 lowercase hex and exists as a commit."""
    if not re.fullmatch(r'[0-9a-f]{40}', target_sha):
        raise RuntimeError(
            f'target SHA is not 40 lowercase hex characters: {target_sha!r}')
    r = _run(['git', 'cat-file', '-e', f'{target_sha}^{{commit}}'],
             cwd=project_root)
    if r.returncode != 0:
        raise RuntimeError(
            f'target SHA not found in repository: {target_sha}')


def _validate_package_dir(project_root, pkgdir):
    """Validate and return a package path contained by ``packages/``."""
    if not isinstance(pkgdir, str) or not pkgdir:
        raise RuntimeError(f'invalid package path: {pkgdir!r}')
    if os.path.isabs(pkgdir):
        raise RuntimeError(f'package path must be relative: {pkgdir!r}')

    normalized = os.path.normpath(pkgdir)
    if normalized != pkgdir:
        raise RuntimeError(f'package path is not normalized: {pkgdir!r}')
    if not normalized.startswith('packages' + os.sep):
        raise RuntimeError(
            f'package path escapes packages/ or is outside it: {pkgdir!r}')

    packages_root = os.path.realpath(
        os.path.join(project_root, 'packages'))
    pkgdir_abs = os.path.abspath(os.path.join(project_root, normalized))
    pkgdir_real = os.path.realpath(pkgdir_abs)
    if not pkgdir_real.startswith(packages_root + os.sep):
        raise RuntimeError(
            f'package path escapes packages/: {pkgdir!r}')
    if not os.path.isdir(pkgdir_abs):
        raise RuntimeError(
            f'package directory does not exist: {pkgdir_abs}')

    manifest_path = os.path.join(pkgdir_abs, '.refresh-manifest.json')
    if not os.path.isfile(manifest_path):
        raise RuntimeError(
            f'not a recognised package — missing .refresh-manifest.json: '
            f'{pkgdir_abs}')
    return normalized, pkgdir_abs


def _get_repo_name(project_root):
    """Return the basename of the repository root (used as session prefix)."""
    return os.path.basename(os.path.abspath(project_root))


def _record_caller_state(project_root):
    """Capture caller repository state for immutability verification.

    Returns a dict with branch name, HEAD SHA, porcelain-v1 status bytes,
    and index tree SHA.
    """
    branch_r = _run(['git', 'symbolic-ref', '--short', 'HEAD'],
                    cwd=project_root)
    branch = (branch_r.stdout.strip()
              if branch_r.returncode == 0 else 'detached')

    head_r = _run(['git', 'rev-parse', 'HEAD'], cwd=project_root)
    head = head_r.stdout.strip() if head_r.returncode == 0 else ''

    status_r = _run(['git', 'status', '--porcelain', '-v'], cwd=project_root)
    status_bytes = (status_r.stdout.encode()
                    if status_r.returncode == 0 else b'')

    index_r = _run(['git', 'write-tree'], cwd=project_root)
    index_tree = index_r.stdout.strip() if index_r.returncode == 0 else ''

    return {
        'branch': branch,
        'head': head,
        'status_bytes': status_bytes,
        'index_tree': index_tree,
    }


def _verify_caller_state(project_root, original):
    """Verify caller repository has not been modified.

    Raises RuntimeError if any tracked state diverged.
    """
    current = _record_caller_state(project_root)
    if current['branch'] != original['branch']:
        raise RuntimeError(
            f'caller branch changed: {original["branch"]!r} -> '
            f'{current["branch"]!r} — session retained')
    if current['head'] != original['head']:
        raise RuntimeError(
            f'caller HEAD changed: {original["head"][:12]} -> '
            f'{current["head"][:12]} — session retained')
    if current['status_bytes'] != original['status_bytes']:
        raise RuntimeError(
            'caller working tree or index changed — session retained')
    if current['index_tree'] != original['index_tree']:
        raise RuntimeError(
            f'caller index tree changed: {original["index_tree"][:12]} -> '
            f'{current["index_tree"][:12]} — session retained')


# ---- Snapshot digest ----

def _compute_snapshot_digest(snapshot_dir):
    """Compute a deterministic digest over snapshot contents.

    Covers relative paths, regular-file modes, and file bytes, sorted
    deterministically.
    """
    h = hashlib.sha256()
    for root, dirs, files in os.walk(snapshot_dir):
        for dirname in dirs:
            path = os.path.join(root, dirname)
            if os.path.islink(path):
                raise RuntimeError(
                    f'snapshot contains symlink: '
                    f'{os.path.relpath(path, snapshot_dir)}')
        dirs.sort()
        for fn in sorted(files):
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, snapshot_dir)
            if os.path.islink(path):
                raise RuntimeError(f'snapshot contains symlink: {rel}')
            if not os.path.isfile(path):
                raise RuntimeError(
                    f'snapshot contains non-regular file: {rel}')
            mode = os.stat(path).st_mode & 0o7777
            h.update(rel.encode('utf-8') + b'\0')
            h.update(f'{mode:04o}'.encode('utf-8') + b'\0')
            with open(path, 'rb') as f:
                h.update(f.read())
    return h.hexdigest()


# ---- Package snapshot ----

def _copy_package_snapshot(pkgdir_abs, snapshot_dir):
    """Copy the normalized package working area into *snapshot_dir*.

    Excludes ``patches/``, ``files/``, ``__pycache__``, and any symlinks
    that escape the package directory.  Includes the package control files
    needed to reproduce the caller's current base and build behavior.
    """
    files = list_working_files(pkgdir_abs)
    for control_file in (
            '.refresh-manifest.json', '.pkgignore', 'build-test.sh'):
        if (os.path.isfile(os.path.join(pkgdir_abs, control_file)) and
                control_file not in files):
            files.append(control_file)
    pkgdir_abs_real = os.path.realpath(pkgdir_abs)

    for rel in files:
        if '__pycache__' in rel.split('/'):
            continue
        src = os.path.join(pkgdir_abs, rel)

        if os.path.islink(src):
            target = os.path.realpath(src)
            if not (target.startswith(pkgdir_abs_real + os.sep) and
                    target != pkgdir_abs_real):
                raise RuntimeError(
                    f'symlink escapes package: {rel!r} -> {target!r}')

        dst = os.path.join(snapshot_dir, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


# ---- Session JSON ----

def _write_session_json(session_dir, metadata):
    """Write session.json atomically via temp file + rename."""
    path = os.path.join(session_dir, 'session.json')
    fd, tmp = tempfile.mkstemp(dir=session_dir, prefix='.session.',
                               suffix='.tmp')
    try:
        with os.fdopen(fd, 'w') as f:
            json.dump(metadata, f, indent=2, sort_keys=True)
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


def _update_session_json(session_dir, updates):
    """Load session.json, update with *updates*, and write atomically."""
    path = os.path.join(session_dir, 'session.json')
    with open(path) as f:
        data = json.load(f)
    data.update(updates)
    _write_session_json(session_dir, data)


# ---- Conflict marker detection ----

_CONFLICT_MARKERS = ('<<<<<<<', '=======', '>>>>>>>')


def _has_conflict_markers(file_path):
    """Return True if *file_path* contains Git conflict markers."""
    try:
        with open(file_path, 'r') as f:
            return any(marker in f.read() for marker in _CONFLICT_MARKERS)
    except (OSError, UnicodeDecodeError):
        return False


def _scan_conflict_files(pkgdir_abs):
    """Scan working-area files under *pkgdir_abs* for conflict markers.

    Returns a sorted list of relative paths that contain markers.
    """
    conflicted = []
    for rel in list_working_files(pkgdir_abs):
        if _has_conflict_markers(os.path.join(pkgdir_abs, rel)):
            conflicted.append(rel)
    return sorted(conflicted)


def _git_unmerged_paths(cwd):
    """Return list of unmerged paths reported by Git in *cwd*."""
    r = _run(['git', 'diff', '--name-only', '--diff-filter=U'], cwd=cwd)
    if r.returncode != 0:
        raise RuntimeError(
            f'cannot inspect unmerged paths: {r.stderr.strip()}')
    return [p for p in r.stdout.splitlines() if p]


# ---- Snapshot replacement ----

def _replace_package_with_snapshot(snapshot_dir, worktree, package):
    """Replace the worktree's package working-area with the snapshot.

    Only touches working-area files (per ``list_working_files``).
    Preserves generated patches/ and files/, while restoring snapshotted
    package control files such as the manifest and build gate.

    Validates that every snapshot entry is contained under the snapshot
    root before copying.
    """
    pkg_in_worktree = os.path.join(worktree, package)
    snapshot_real = os.path.realpath(snapshot_dir)
    pkg_real = os.path.realpath(pkg_in_worktree)

    worktree_real = os.path.realpath(worktree)
    if not (pkg_real.startswith(worktree_real + os.sep) or
            pkg_real == worktree_real):
        raise RuntimeError(
            f'package path escapes worktree: {package}')

    for rel in list_working_files(pkg_in_worktree):
        target = os.path.join(pkg_in_worktree, rel)
        if os.path.isfile(target) or os.path.islink(target):
            os.unlink(target)
        elif os.path.isdir(target):
            shutil.rmtree(target)

    snapshot_files = list_working_files(snapshot_dir)
    for control_file in (
            '.refresh-manifest.json', '.pkgignore', 'build-test.sh'):
        if (os.path.isfile(os.path.join(snapshot_dir, control_file)) and
                control_file not in snapshot_files):
            snapshot_files.append(control_file)

    for rel in snapshot_files:
        src = os.path.join(snapshot_dir, rel)

        src_real = os.path.realpath(src)
        if not (src_real.startswith(snapshot_real + os.sep) or
                src_real == snapshot_real):
            raise RuntimeError(
                f'snapshot entry escapes snapshot root: {rel!r}')

        dst = os.path.join(pkg_in_worktree, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)


# ---- Build streaming ----

def _stream_build(build_script, cwd, session_dir):
    """Run *build_script* streaming output to caller and saving to log.

    Returns (exit_code, combined_output).
    """
    log_path = os.path.join(session_dir, 'build-test.log')
    log_buffer = []

    proc = subprocess.Popen(
        [build_script], cwd=cwd,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )

    for line in iter(proc.stdout.readline, ''):
        sys.stdout.write(line)
        sys.stdout.flush()
        log_buffer.append(line)
    proc.stdout.close()
    exit_code = proc.wait()

    combined = ''.join(log_buffer)
    with open(log_path, 'w') as f:
        f.write(combined)

    return exit_code, combined


# ---- Hardened cleanup ----

def _cleanup_session(session_dir, source_repo, worktree):
    """Clean up a completed session directory and its worktree.

    Validates containment and registration before any deletion.
    Returns True on success, False on failure (session retained).
    """
    source_repo_real = os.path.realpath(source_repo)
    session_real = os.path.realpath(session_dir)
    repo_name = os.path.basename(source_repo_real)
    expected_parent = os.path.dirname(source_repo_real)

    # Validate session parent matches source repo parent.
    if os.path.dirname(session_real) != expected_parent:
        print(f'error: session parent mismatch — cleanup skipped',
              file=sys.stderr)
        return False

    # Validate session basename prefix.
    base = os.path.basename(session_real)
    if not base.startswith(f'{repo_name}-package-integrate-'):
        print(f'error: session basename prefix mismatch — cleanup skipped',
              file=sys.stderr)
        return False

    # Validate expected top-level entries.
    required_entries = {'session.json', 'package-snapshot', 'worktree'}
    allowed_entries = required_entries | {'build-test.log'}
    actual_entries = set(os.listdir(session_real))
    for expected in sorted(required_entries):
        if not os.path.exists(os.path.join(session_real, expected)):
            print(f'error: missing expected entry {expected!r} — '
                  f'cleanup skipped', file=sys.stderr)
            return False
    unexpected = sorted(actual_entries - allowed_entries)
    if unexpected:
        print(f'error: unexpected session entries '
              f'{", ".join(unexpected)} — cleanup skipped',
              file=sys.stderr)
        return False

    # Validate worktree is under session.
    worktree_real = os.path.realpath(worktree)
    if not (worktree_real.startswith(session_real + os.sep) or
            worktree_real == session_real):
        print(f'error: worktree escapes session — cleanup skipped',
              file=sys.stderr)
        return False

    # Validate worktree is registered under source repo.
    git_file = os.path.join(worktree, '.git')
    if not os.path.isfile(git_file):
        print(f'error: worktree .git file missing — cleanup skipped',
              file=sys.stderr)
        return False
    with open(git_file) as f:
        git_content = f.read().strip()
    if not git_content.startswith('gitdir: '):
        print(f'error: worktree .git file malformed — cleanup skipped',
              file=sys.stderr)
        return False
    reg_path = git_content[len('gitdir: '):]
    if not os.path.isabs(reg_path):
        reg_path = os.path.join(worktree, reg_path)
    reg_dir = os.path.realpath(reg_path)
    expected_prefix = os.path.realpath(
        os.path.join(source_repo, '.git', 'worktrees')) + os.sep
    if not reg_dir.startswith(expected_prefix):
        print(f'error: worktree not registered with source repo — '
              f'cleanup skipped', file=sys.stderr)
        return False
    if not os.path.isdir(reg_dir):
        print(f'error: worktree registration missing — cleanup skipped',
              file=sys.stderr)
        return False

    # Remove worktree via Git.
    r = _run(['git', 'worktree', 'remove', '--force', worktree],
             cwd=source_repo)
    if r.returncode != 0:
        # Retain entire session, record cleanup-failed.
        try:
            _update_session_json(session_dir, {
                'phase': 'cleanup-failed',
                'cleanup_error': r.stderr.strip(),
            })
        except Exception:
            pass
        print(f'error: git worktree remove failed: {r.stderr.strip()}',
              file=sys.stderr)
        print(f'Hint: manually run: git worktree remove --force {worktree}',
              file=sys.stderr)
        return False

    # Prune stale worktree registration.
    r = _run(['git', 'worktree', 'prune'], cwd=source_repo)
    if r.returncode != 0:
        try:
            _update_session_json(session_dir, {
                'phase': 'cleanup-failed',
                'cleanup_error': r.stderr.strip(),
            })
        except Exception:
            pass
        print(f'error: git worktree prune failed: {r.stderr.strip()}',
              file=sys.stderr)
        return False

    # Remove session directory.
    try:
        shutil.rmtree(session_dir)
    except OSError as e:
        try:
            _update_session_json(session_dir, {
                'phase': 'cleanup-failed',
                'cleanup_error': str(e),
            })
        except Exception:
            pass
        print(f'error: failed to remove session directory: {e}',
              file=sys.stderr)
        return False

    return True


# ---- Integration session creation ----

def create_integrate_session(pkgdir, project_root, target_ref,
                             keep=False, no_build=False):
    """Create an integration session.

    Args:
        pkgdir: Repository-relative package path (e.g. ``packages/forth-core``).
        project_root: Absolute path to the repository root.
        target_ref: Git ref to merge (resolved to a commit).
        keep: If True, retain the session on success.
        no_build: If True, skip the build step (handled in PCLI-3).

    Returns:
        Dict with ``session_dir``, ``phase``, and other session metadata.

    Raises:
        RuntimeError: On validation errors, ref resolution failures, or
            caller immutability violations.  The session directory is
            retained on error for debugging.
    """
    project_root = os.path.realpath(project_root)

    # --- Pre-flight: validate inputs BEFORE creating anything ---
    pkgdir, pkgdir_abs = _validate_package_dir(project_root, pkgdir)

    # Resolve and validate target SHA.
    target_sha = _resolve_commit(project_root, target_ref)
    _validate_target_sha(project_root, target_sha)

    # Resolve source HEAD.
    r = _run(['git', 'rev-parse', 'HEAD'], cwd=project_root)
    if r.returncode != 0:
        raise RuntimeError(
            f'cannot resolve HEAD in {project_root!r}')
    source_head = r.stdout.strip()

    # Record caller state.
    caller_state = _record_caller_state(project_root)

    # --- Create session ---
    repo_name = _get_repo_name(project_root)
    session_parent = os.path.dirname(os.path.abspath(project_root))
    session_dir = tempfile.mkdtemp(
        prefix=f'{repo_name}-package-integrate-',
        dir=session_parent,
    )

    session_meta = {
        'schema_version': 1,
        'source_repo': os.path.abspath(project_root),
        'source_head': source_head,
        'package': pkgdir,
        'target_ref': target_ref,
        'target_sha': target_sha,
        'worktree': None,
        'keep': keep,
        'no_build': no_build,
        'phase': 'initializing',
        'conflict_paths': [],
    }

    try:
        # --- Package snapshot ---
        snapshot_dir = os.path.join(session_dir, 'package-snapshot')
        os.makedirs(snapshot_dir, exist_ok=True)
        _copy_package_snapshot(pkgdir_abs, snapshot_dir)

        # Compute and store snapshot digest.
        snapshot_digest = _compute_snapshot_digest(snapshot_dir)
        session_meta['snapshot_digest'] = snapshot_digest

        # --- Write session.json ---
        _write_session_json(session_dir, session_meta)

        # --- Create worktree ---
        worktree_dir = os.path.join(session_dir, 'worktree')
        r = _run([
            'git', 'worktree', 'add', '--detach',
            worktree_dir, source_head,
        ], cwd=project_root)
        if r.returncode != 0:
            raise RuntimeError(
                f'cannot create detached worktree: {r.stderr.strip()}')

        session_meta['worktree'] = worktree_dir
        _update_session_json(session_dir, {'worktree': worktree_dir,
                                           'phase': 'merging'})

        # --- Merge target into worktree (no commit) ---
        r = _run([
            'git', 'merge', '--no-commit', '--no-ff', target_sha,
        ], cwd=worktree_dir)

        if r.returncode != 0:
            # Check for unmerged paths to distinguish conflict from fatal.
            r_conflicts = _run([
                'git', 'diff', '--name-only', '--diff-filter=U',
            ], cwd=worktree_dir)
            conflict_paths = (
                [p for p in r_conflicts.stdout.splitlines() if p]
                if r_conflicts.returncode == 0 else []
            )

            if conflict_paths:
                _update_session_json(session_dir, {
                    'phase': 'repo-conflict',
                    'conflict_paths': conflict_paths,
                })
                _verify_caller_state(project_root, caller_state)
                return {
                    'session_dir': session_dir,
                    'phase': 'repo-conflict',
                    'conflict_paths': conflict_paths,
                    'keep': True,
                }
            else:
                # Fatal merge with no conflicts.
                _update_session_json(session_dir, {
                    'phase': 'merge-fatal',
                    'merge_stdout': r.stdout,
                    'merge_stderr': r.stderr,
                })
                _verify_caller_state(project_root, caller_state)
                return {
                    'session_dir': session_dir,
                    'phase': 'merge-fatal',
                    'merge_stdout': r.stdout,
                    'merge_stderr': r.stderr,
                    'keep': True,
                }
        else:
            _update_session_json(session_dir, {'phase': 'repo-ready'})
            _verify_caller_state(project_root, caller_state)
            return {
                'session_dir': session_dir,
                'phase': 'repo-ready',
                'conflict_paths': [],
                'keep': keep,
            }

    except BaseException:
        try:
            _update_session_json(session_dir, {'phase': 'error'})
        except Exception:
            pass
        raise


# ---- Resume engine ----

def resume_session(session_dir, keep=None, no_build=None):
    """Resume an integration session from its current phase.

    Phase transitions::

        repo-conflict -> repo-ready -> package-copy -> package-rebase
        package-conflict -> package-refresh -> build -> complete
        build-failed -> build (retry) -> complete | build-failed
        merge-fatal -> terminal (start a new session after fixing Git)

    Args:
        session_dir: Absolute path to the session directory.
        keep: If True, retain session on success (overrides stored value).
              If None, use stored value.
        no_build: If True, skip build step (overrides stored value).
                  If None, use stored value.

    Returns:
        Dict with ``session_dir``, ``phase``, and phase-specific keys.

    Raises:
        RuntimeError: On validation errors, unresolved conflicts, or
            forged metadata.  The session directory is retained on error.
    """
    session_dir = os.path.abspath(session_dir)

    if not os.path.isdir(session_dir):
        raise RuntimeError(f'session directory does not exist: {session_dir}')

    session_json_path = os.path.join(session_dir, 'session.json')
    if not os.path.isfile(session_json_path):
        raise RuntimeError(f'missing session.json: {session_dir}')

    try:
        with open(session_json_path) as f:
            meta = json.load(f)
    except (OSError, ValueError) as exc:
        raise RuntimeError(
            f'invalid session metadata in {session_json_path}: {exc}')
    if not isinstance(meta, dict):
        raise RuntimeError(
            f'invalid session metadata in {session_json_path}: '
            f'top level must be an object')

    if meta.get('schema_version') != 1:
        raise RuntimeError(
            f'unsupported schema version: {meta.get("schema_version")!r}')

    source_repo = meta.get('source_repo')
    package = meta.get('package')
    target_sha = meta.get('target_sha')
    worktree = meta.get('worktree')

    if not source_repo or not os.path.isdir(source_repo):
        raise RuntimeError(f'invalid source repository: {source_repo!r}')

    r = _run(['git', 'rev-parse', '--git-dir'], cwd=source_repo)
    if r.returncode != 0:
        raise RuntimeError(f'not a git repository: {source_repo}')

    if not package:
        raise RuntimeError('missing package in session metadata')

    _validate_target_sha(source_repo, target_sha)
    package, _ = _validate_package_dir(source_repo, package)

    if not worktree:
        raise RuntimeError('session has no registered worktree')

    # --- Containment checks ---
    source_repo_real = os.path.realpath(source_repo)
    session_real = os.path.realpath(session_dir)
    expected_parent = os.path.dirname(source_repo_real)
    if os.path.dirname(session_real) != expected_parent:
        raise RuntimeError(
            f'session path is not beside source repository: {session_dir}')
    expected_prefix = (
        os.path.basename(source_repo_real) + '-package-integrate-')
    if not os.path.basename(session_real).startswith(expected_prefix):
        raise RuntimeError(
            f'session basename has unexpected prefix: {session_dir}')

    worktree_real = os.path.realpath(worktree)
    if not (worktree_real.startswith(session_real + os.sep) or
            worktree_real == session_real):
        raise RuntimeError(
            f'worktree escapes session directory: {worktree}')

    if not os.path.isdir(worktree):
        raise RuntimeError(f'worktree does not exist: {worktree}')

    git_file = os.path.join(worktree, '.git')
    if not os.path.isfile(git_file):
        raise RuntimeError(
            f'worktree .git file missing: {worktree}')
    with open(git_file) as f:
        git_content = f.read().strip()
    if not git_content.startswith('gitdir: '):
        raise RuntimeError(
            f'worktree .git file has unexpected content: {worktree}')
    reg_path = git_content[len('gitdir: '):]
    if not os.path.isabs(reg_path):
        reg_path = os.path.join(worktree, reg_path)
    reg_dir = os.path.realpath(reg_path)
    expected_prefix = os.path.realpath(
        os.path.join(source_repo, '.git', 'worktrees')) + os.sep
    if not reg_dir.startswith(expected_prefix):
        raise RuntimeError(
            f'worktree not registered with git: {worktree}')
    if not os.path.isdir(reg_dir):
        raise RuntimeError(
            f'worktree not registered with git: {worktree}')

    snapshot_dir = os.path.join(session_dir, 'package-snapshot')
    if not os.path.isdir(snapshot_dir):
        raise RuntimeError(f'missing package snapshot: {snapshot_dir}')

    # Verify snapshot digest.
    stored_digest = meta.get('snapshot_digest')
    if not isinstance(stored_digest, str) or not re.fullmatch(
            r'[0-9a-f]{64}', stored_digest):
        _update_session_json(session_dir, {'phase': 'snapshot-invalid'})
        raise RuntimeError(
            f'invalid or missing snapshot digest: {stored_digest!r}')
    try:
        actual_digest = _compute_snapshot_digest(snapshot_dir)
    except RuntimeError:
        _update_session_json(session_dir, {'phase': 'snapshot-invalid'})
        raise
    if actual_digest != stored_digest:
        _update_session_json(session_dir, {'phase': 'snapshot-invalid'})
        raise RuntimeError(
            f'snapshot digest mismatch — possible mutation: '
            f'expected {stored_digest[:12]}, got {actual_digest[:12]}')

    caller_state = _record_caller_state(source_repo)

    effective_keep = keep if keep is not None else meta.get('keep', False)
    effective_no_build = (no_build if no_build is not None
                          else meta.get('no_build', False))

    phase = meta.get('phase')

    # --- Already complete ---
    if phase == 'complete':
        _verify_caller_state(source_repo, caller_state)
        return {
            'session_dir': session_dir,
            'phase': 'complete',
            'keep': effective_keep,
        }

    # --- repo-conflict: resolve and continue ---
    if phase == 'repo-conflict':
        conflict_paths = meta.get('conflict_paths', [])

        unmerged = _git_unmerged_paths(worktree)
        if unmerged:
            raise RuntimeError(
                f'unresolved repository conflicts: '
                f'{", ".join(unmerged)}')

        for rel_path in conflict_paths:
            full = os.path.join(worktree, rel_path)
            if os.path.isfile(full) and _has_conflict_markers(full):
                raise RuntimeError(
                    f'conflict markers remain in {rel_path}')

        if conflict_paths:
            _run(['git', 'add'] + conflict_paths, cwd=worktree)

        _update_session_json(session_dir, {'phase': 'repo-ready'})
        phase = 'repo-ready'

    # --- merge-fatal: terminal; the target was not successfully merged ---
    if phase == 'merge-fatal':
        diagnostic = (meta.get('merge_stderr') or
                      meta.get('merge_stdout') or
                      'Git merge failed without conflict paths')
        raise RuntimeError(
            f'merge-fatal session cannot be resumed: '
            f'{diagnostic.strip()}. Fix the Git failure and start a new '
            f'integration session.')

    # --- repo-ready: copy snapshot ---
    if phase == 'repo-ready':
        _replace_package_with_snapshot(snapshot_dir, worktree, package)
        _update_session_json(session_dir, {'phase': 'package-copy'})
        phase = 'package-copy'

    # --- package-copy: rebase ---
    if phase == 'package-copy':
        rebase_result = rebase_base(package, target_sha, worktree)

        if rebase_result['conflicted']:
            _update_session_json(session_dir, {
                'phase': 'package-conflict',
                'conflict_paths': rebase_result['conflicted'],
            })
            _verify_caller_state(source_repo, caller_state)
            return {
                'session_dir': session_dir,
                'phase': 'package-conflict',
                'conflict_paths': rebase_result['conflicted'],
                'keep': True,
            }

        _update_session_json(session_dir, {'phase': 'package-rebase'})
        phase = 'package-rebase'

    # --- package-rebase: refresh + check markers ---
    if phase == 'package-rebase':
        refresh(package, worktree)

        pkg_in_worktree = os.path.join(worktree, package)
        conflict_files = _scan_conflict_files(pkg_in_worktree)
        if conflict_files:
            _update_session_json(session_dir, {
                'phase': 'package-conflict',
                'conflict_paths': conflict_files,
            })
            _verify_caller_state(source_repo, caller_state)
            return {
                'session_dir': session_dir,
                'phase': 'package-conflict',
                'conflict_paths': conflict_files,
                'keep': True,
            }

        _update_session_json(session_dir, {'phase': 'package-refresh'})
        phase = 'package-refresh'

    # --- package-conflict (resumed): verify clean, refresh ---
    if phase == 'package-conflict':
        pkg_in_worktree = os.path.join(worktree, package)
        conflict_files = _scan_conflict_files(pkg_in_worktree)
        if conflict_files:
            raise RuntimeError(
                f'conflict markers remain: {", ".join(conflict_files)}')

        refresh(package, worktree)
        _update_session_json(session_dir, {'phase': 'package-refresh'})
        phase = 'package-refresh'

    # --- package-refresh: build ---
    if phase == 'package-refresh':
        if not effective_no_build:
            build_script = os.path.join(worktree, package, 'build-test.sh')
            if not os.path.isfile(build_script):
                raise RuntimeError(
                    f'build script not found: {build_script}')

            exit_code, _ = _stream_build(build_script, worktree, session_dir)

            if exit_code != 0:
                _update_session_json(session_dir, {
                    'phase': 'build-failed',
                    'build_exit_code': exit_code,
                })
                _verify_caller_state(source_repo, caller_state)
                return {
                    'session_dir': session_dir,
                    'phase': 'build-failed',
                    'build_exit_code': exit_code,
                    'keep': True,
                }

            _update_session_json(session_dir, {'phase': 'build'})

        _update_session_json(session_dir, {'phase': 'complete'})
        phase = 'complete'

    # --- build-failed (resumed): retry the build ---
    if phase == 'build-failed':
        if not effective_no_build:
            build_script = os.path.join(worktree, package, 'build-test.sh')
            if not os.path.isfile(build_script):
                raise RuntimeError(
                    f'build script not found: {build_script}')

            exit_code, _ = _stream_build(build_script, worktree, session_dir)

            if exit_code != 0:
                _update_session_json(session_dir, {
                    'phase': 'build-failed',
                    'build_exit_code': exit_code,
                })
                _verify_caller_state(source_repo, caller_state)
                return {
                    'session_dir': session_dir,
                    'phase': 'build-failed',
                    'build_exit_code': exit_code,
                    'keep': True,
                }

            _update_session_json(session_dir, {'phase': 'build'})
            phase = 'build'
        else:
            phase = 'complete'
            _update_session_json(session_dir, {'phase': 'complete'})

    # --- Unexpected phase ---
    if phase not in ('complete', 'build'):
        raise RuntimeError(f'unexpected session phase: {phase!r}')

    # --- Complete: cleanup or retain ---
    _verify_caller_state(source_repo, caller_state)

    if not effective_keep:
        if not _cleanup_session(session_dir, source_repo, worktree):
            _verify_caller_state(source_repo, caller_state)
            return {
                'session_dir': session_dir,
                'phase': 'cleanup-failed',
                'keep': True,
            }

    return {
        'session_dir': session_dir,
        'phase': 'complete',
        'keep': effective_keep,
    }
