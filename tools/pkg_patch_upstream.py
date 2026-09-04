# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
"""Automate folding package manager packages into clean upstream branches for GitLab MRs.

Creates an isolated detached worktree at the target upstream commit, copies
package-introduced files (files/) to their upstream locations (src/c47/ or
src/testSuite/), applies package patches (patches/*.patch) via git apply -3,
updates src/c47/meson.build with any new C source files, verifies the native
upstream build, and creates a clean squashed commit and local branch ready for
an upstream Merge Request.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

# Ensure tools/ is in sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import (  # noqa: E402
    SIBLING_ROOTS,
    decode_patch_filename,
    upstream_repo_rel,
)
from pkg_patch_refresh import list_working_files, load_manifest  # noqa: E402


class UpstreamError(Exception):
    """Base exception for upstream folding failures."""


class UpstreamConflictError(UpstreamError):
    """Raised when patch application encounters merge conflicts."""


class UpstreamBuildError(UpstreamError):
    """Raised when native upstream build or test suite fails."""


def _run(cmd, cwd=None, capture=True):
    """Run a command and return subprocess.CompletedProcess."""
    if capture:
        return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    return subprocess.run(cmd, cwd=cwd)


def _resolve_package(name, project_root):
    """Resolve *name* to a repository-relative package path ('packages/<name>')."""
    if not name:
        raise UpstreamError('package name must not be empty')
    if os.path.isabs(name):
        raise UpstreamError(
            f'package path must be relative, not absolute: {name!r}')
    norm = os.path.normpath(name)
    if norm.startswith('..'):
        raise UpstreamError(
            f'package path must not contain traversal: {name!r}')
    if norm.startswith('packages/'):
        bare = norm[len('packages/'):]
    elif norm.startswith('packages' + os.sep):
        bare = norm[len('packages' + os.sep):]
    else:
        bare = norm

    if not bare:
        raise UpstreamError('package name must not be empty')

    resolved = os.path.join('packages', bare)
    pkgdir_abs = os.path.join(project_root, resolved)
    if not os.path.isdir(pkgdir_abs):
        raise UpstreamError(f'package directory does not exist: {resolved}')

    manifest = os.path.join(pkgdir_abs, '.refresh-manifest.json')
    if not os.path.isfile(manifest):
        raise UpstreamError(
            f'not a recognised package — missing .refresh-manifest.json: {resolved}')
    return resolved


def parse_package_list(packages_arg, project_root):
    """Parse a comma-separated list of package names or paths.

    Accepts e.g. 'undo-history', 'packages/pretty-print,packages/pretty-print-extra'.
    Returns a list of resolved package relative paths without duplicates.
    """
    if not packages_arg or not packages_arg.strip():
        raise UpstreamError('at least one package must be specified')

    parts = [p.strip() for p in packages_arg.split(',') if p.strip()]
    if not parts:
        raise UpstreamError('at least one package must be specified')

    resolved_list = []
    seen = set()
    for item in parts:
        resolved = _resolve_package(item, project_root)
        if resolved in seen:
            raise UpstreamError(f'duplicate package in argument list: {item!r}')
        seen.add(resolved)
        resolved_list.append(resolved)
    return resolved_list


def resolve_target_ref(project_root, onto_ref=None):
    """Resolve target commit SHA from *onto_ref* or upstream defaults."""
    candidates = [onto_ref] if onto_ref else ['upstream/master', 'upstream/HEAD']
    for ref in candidates:
        if not ref:
            continue
        r = _run(['git', 'rev-parse', '--verify', '--quiet', f'{ref}^{{commit}}'],
                 cwd=project_root)
        if r.returncode == 0 and r.stdout.strip():
            return ref, r.stdout.strip()

    if onto_ref:
        raise UpstreamError(f'cannot resolve target ref: {onto_ref!r}')
    raise UpstreamError(
        'neither upstream/master nor upstream/HEAD found; specify target ref with --onto <REF>')


def _has_conflict_markers(content):
    """Check if string content contains git conflict markers."""
    return bool(re.search(r'^(<{7}|={7}|>{7})\b', content, re.MULTILINE))


def fold_package_files(session_worktree, pkg_abs_dir):
    """Copy files/ from package to session worktree.

    Paths under files/testSuite/ go to src/testSuite/.
    All other paths under files/ go to src/c47/.
    Returns a list of newly introduced C source filenames relative to src/c47/.
    """
    files_dir = os.path.join(pkg_abs_dir, 'files')
    if not os.path.isdir(files_dir):
        return []

    new_c47_c_sources = []
    for root, _dirs, files in os.walk(files_dir):
        rel_root = os.path.relpath(root, files_dir)
        for fname in sorted(files):
            rel = fname if rel_root == '.' else os.path.join(rel_root, fname)
            rel_fwd = rel.replace(os.sep, '/')
            src_path = os.path.join(root, fname)

            # Determine destination in worktree
            if rel_fwd.split('/', 1)[0] in SIBLING_ROOTS:
                dst_path = os.path.join(session_worktree, 'src', *rel_fwd.split('/'))
            else:
                dst_path = os.path.join(session_worktree, 'src', 'c47', *rel_fwd.split('/'))
                if fname.endswith('.c'):
                    new_c47_c_sources.append(rel_fwd)

            os.makedirs(os.path.dirname(dst_path), exist_ok=True)
            shutil.copy2(src_path, dst_path)

    return sorted(new_c47_c_sources)


def apply_package_patches(session_worktree, pkg_abs_dir):
    """Apply patches/*.patch in numerical ordinal order using git apply -3."""
    patches_dir = os.path.join(pkg_abs_dir, 'patches')
    if not os.path.isdir(patches_dir):
        return

    patch_entries = []
    for fname in os.listdir(patches_dir):
        if fname.endswith('.patch'):
            try:
                ordinal, rel = decode_patch_filename(fname)
                patch_entries.append((ordinal, fname, os.path.join(patches_dir, fname)))
            except ValueError:
                continue

    patch_entries.sort(key=lambda x: x[0])

    for _ordinal, fname, patch_abs in patch_entries:
        r = _run(['git', 'apply', '-3', patch_abs], cwd=session_worktree)
        if r.returncode != 0:
            raise UpstreamConflictError(
                f'failed to apply patch {fname} in {os.path.basename(pkg_abs_dir)}:\n'
                f'{r.stderr.strip()}')

    # Check for conflict markers in all modified/untracked files
    status_r = _run(['git', 'status', '--porcelain'], cwd=session_worktree)
    if status_r.returncode == 0:
        for line in status_r.stdout.splitlines():
            if len(line) > 3:
                target_rel = line[3:].strip()
                target_abs = os.path.join(session_worktree, target_rel)
                if os.path.isfile(target_abs):
                    try:
                        with open(target_abs, 'r', encoding='utf-8', errors='ignore') as f:
                            if _has_conflict_markers(f.read()):
                                raise UpstreamConflictError(
                                    f'conflict markers detected in {target_rel} after applying {pkg_abs_dir}')
                    except OSError:
                        pass


def register_c47_meson_sources(meson_build_path, new_c_files):
    """Register *new_c_files* into c47_src = files(...) in src/c47/meson.build.

    Returns the list of newly added files.
    """
    if not new_c_files or not os.path.isfile(meson_build_path):
        return []

    with open(meson_build_path, 'r', encoding='utf-8') as f:
        content = f.read()

    pattern = re.compile(r'(c47_src\s*=\s*files\s*\()(.*?)(\n\))', re.DOTALL)
    m = pattern.search(content)
    if not m:
        raise UpstreamError(
            f'could not find "c47_src = files(...)" block in {meson_build_path}')

    header = m.group(1)
    body = m.group(2)
    footer = m.group(3)

    existing = set(re.findall(r"'([^']+)'", body))
    to_add = [f for f in sorted(new_c_files) if f not in existing]
    if not to_add:
        return []

    lines = body.splitlines()
    for new_file in to_add:
        new_line = f"  '{new_file}',"
        inserted = False
        for idx, line in enumerate(lines):
            entry_m = re.search(r"'([^']+)'", line)
            if entry_m:
                entry = entry_m.group(1)
                # Keep alphabetical order
                if new_file < entry:
                    lines.insert(idx, new_line)
                    inserted = True
                    break
        if not inserted:
            lines.append(new_line)

    # Clean lines and ensure valid trailing commas
    cleaned_lines = []
    for l in lines:
        if not l.strip():
            continue
        entry_m = re.search(r"(\s*'[^']+')(,?)", l)
        if entry_m:
            cleaned_lines.append(entry_m.group(1) + ',')
        else:
            cleaned_lines.append(l)

    new_body = '\n' + '\n'.join(cleaned_lines)
    new_content = content[:m.start()] + header + new_body + footer + content[m.end():]

    with open(meson_build_path, 'w', encoding='utf-8') as f:
        f.write(new_content)

    return to_add


def run_upstream_build_test(session_worktree):
    """Execute native upstream build and headless test suite."""
    build_dir = os.path.join(session_worktree, 'build.upstream-sim')

    # Setup
    r = _run([
        'meson', 'setup', 'build.upstream-sim',
        '--buildtype=debug', '-DDECNUMBER_FASTMUL=true',
    ], cwd=session_worktree)
    if r.returncode != 0:
        raise UpstreamBuildError(
            f'upstream meson setup failed:\n{r.stderr.strip() or r.stdout.strip()}')

    # Ninja build
    r = _run(['ninja', '-C', 'build.upstream-sim'], cwd=session_worktree)
    if r.returncode != 0:
        raise UpstreamBuildError(
            f'upstream ninja build failed:\n{r.stderr.strip() or r.stdout.strip()}')

    # Run headless test suite
    headless_bin = os.path.join(build_dir, 'c47-headless')
    if os.path.isfile(headless_bin):
        r = _run([headless_bin, '-T'], cwd=session_worktree)
        if r.returncode != 0:
            raise UpstreamBuildError(
                f'upstream test suite failed:\n{r.stderr.strip() or r.stdout.strip()}')


def compute_mr_web_url(project_root, branch_name, remote='origin', target_branch='master'):
    """Compute GitLab web URLs for creating a new Merge Request."""
    import urllib.parse

    # Resolve upstream path (default: rpncalculators/c43)
    r_up = _run(['git', 'remote', 'get-url', 'upstream'], cwd=project_root)
    up_url = r_up.stdout.strip() if r_up.returncode == 0 else ''
    upstream_path = 'rpncalculators/c43'
    if 'gitlab.com' in up_url:
        m = re.search(r'gitlab\.com[:/](.+?)(?:\.git)?$', up_url)
        if m:
            upstream_path = m.group(1)

    # Resolve fork path (default: stanle-cs/c43)
    r_fork = _run(['git', 'remote', 'get-url', remote], cwd=project_root)
    fork_url = r_fork.stdout.strip() if r_fork.returncode == 0 else ''
    fork_path = upstream_path
    if 'gitlab.com' in fork_url:
        m = re.search(r'gitlab\.com[:/](.+?)(?:\.git)?$', fork_url)
        if m:
            fork_path = m.group(1)

    encoded_branch = urllib.parse.quote(branch_name, safe='')
    encoded_target = urllib.parse.quote(target_branch, safe='')

    mr_url = (
        f'https://gitlab.com/{upstream_path}/-/merge_requests/new'
        f'?merge_request[source_branch]={encoded_branch}&merge_request[target_branch]={encoded_target}'
    )
    branch_url = f'https://gitlab.com/{fork_path}/-/tree/{encoded_branch}'
    return mr_url, branch_url


def try_glab_mr_create(project_root, branch_name, title, message, target_branch='master'):
    """Attempt non-interactive GitLab MR creation via glab CLI.

    Returns (success: bool, output_or_url: str).
    """
    if not shutil.which('glab'):
        return False, 'glab CLI is not installed'

    cmd = [
        'glab', 'mr', 'create',
        '--source-branch', branch_name,
        '--target-branch', target_branch,
        '--title', title,
        '--description', message,
        '--yes',
    ]
    r = _run(cmd, cwd=project_root)
    if r.returncode == 0:
        return True, r.stdout.strip()
    return False, r.stderr.strip() or r.stdout.strip()


def upstream(
    packages_arg,
    project_root,
    onto_ref=None,
    branch_name=None,
    remote='origin',
    push=False,
    create_mr=False,
    title=None,
    message=None,
    no_build=False,
    keep=False,
):
    """Fold packages into an upstream-compatible branch and optionally push / open MR."""
    resolved_pkgs = parse_package_list(packages_arg, project_root)
    target_ref, target_sha = resolve_target_ref(project_root, onto_ref)

    pkg_basenames = [os.path.basename(p) for p in resolved_pkgs]
    if not branch_name:
        branch_name = f'mr/{pkg_basenames[0]}'

    if not title:
        if len(pkg_basenames) == 1:
            title = f'feat({pkg_basenames[0]}): fold {pkg_basenames[0]} into upstream tree'
        else:
            title = f'feat({", ".join(pkg_basenames)}): fold packages into upstream tree'

    if not message:
        message = (
            f'Folded package(s) into upstream tree:\n'
            + '\n'.join(f'- {p}' for p in resolved_pkgs)
        )

    # Session allocation
    session_id = uuid.uuid4().hex[:8]
    session_parent = os.path.dirname(os.path.abspath(project_root))
    session_dir = tempfile.mkdtemp(
        prefix=f'{os.path.basename(project_root)}-package-upstream-',
        dir=session_parent,
    )
    worktree_dir = os.path.join(session_dir, 'worktree')

    print(f'==> Creating upstream session on {target_ref} ({target_sha[:12]})')
    print(f'==> Session directory: {session_dir}')

    # Create detached worktree
    r = _run([
        'git', 'worktree', 'add', '--detach',
        worktree_dir, target_sha,
    ], cwd=project_root)
    if r.returncode != 0:
        shutil.rmtree(session_dir, ignore_errors=True)
        raise UpstreamError(f'cannot create detached worktree: {r.stderr.strip()}')

    all_new_c_sources = []
    session_failed = False
    try:
        # Fold each package
        for pkg_rel in resolved_pkgs:
            pkg_abs = os.path.join(project_root, pkg_rel)
            print(f'==> Folding {pkg_rel}...')
            new_sources = fold_package_files(worktree_dir, pkg_abs)
            all_new_c_sources.extend(new_sources)
            apply_package_patches(worktree_dir, pkg_abs)

        # Register new C sources in src/c47/meson.build
        meson_file = os.path.join(worktree_dir, 'src', 'c47', 'meson.build')
        added_sources = register_c47_meson_sources(meson_file, all_new_c_sources)
        if added_sources:
            print(f'==> Registered in src/c47/meson.build: {", ".join(added_sources)}')

        # Verification build
        if not no_build:
            print('==> Running native upstream build and test suite...')
            run_upstream_build_test(worktree_dir)
            print('==> Upstream build and test suite passed.')
        else:
            print('==> Build and test suite skipped (--no-build).')

        # Stage and commit
        print(f'==> Committing squashed upstream changes to branch {branch_name}...')
        _run(['git', 'add', 'src/'], cwd=worktree_dir)
        full_msg = f'{title}\n\n{message}'
        r = _run(['git', 'commit', '-m', full_msg], cwd=worktree_dir)
        if r.returncode != 0:
            raise UpstreamError(f'commit failed: {r.stderr.strip() or r.stdout.strip()}')

        commit_sha = _run(['git', 'rev-parse', 'HEAD'], cwd=worktree_dir).stdout.strip()
        print(f'==> Commit created: {commit_sha[:12]} {title}')

        # Update branch in caller repository
        r = _run(['git', 'branch', '-f', branch_name, commit_sha], cwd=project_root)
        if r.returncode != 0:
            raise UpstreamError(f'failed to update branch {branch_name}: {r.stderr.strip()}')
        print(f'==> Local branch created/updated: {branch_name}')

        # Determine target branch name
        target_branch = 'master'
        if onto_ref:
            if onto_ref.startswith('upstream/'):
                target_branch = onto_ref[len('upstream/'):]
            elif onto_ref.startswith('origin/'):
                target_branch = onto_ref[len('origin/'):]
            else:
                target_branch = onto_ref

        # Optional push
        if push or create_mr:
            print(f'==> Pushing {branch_name} to remote {remote}...')
            r = _run(['git', 'push', '-u', remote, branch_name], cwd=project_root)
            if r.returncode != 0:
                print(f'warning: git push failed: {r.stderr.strip()}', file=sys.stderr)
        else:
            print(f'==> NOTE: Branch is local only. Push to GitLab before opening MR:')
            print(f'    git push -u {remote} {branch_name}')

        # Optional glab MR create
        mr_created = False
        if create_mr:
            print('==> Attempting MR creation via glab CLI...')
            success, out = try_glab_mr_create(
                project_root, branch_name, title, message, target_branch=target_branch
            )
            if success:
                print(f'==> GitLab Merge Request created: {out}')
                mr_created = True
            else:
                print(f'warning: glab MR creation failed: {out}', file=sys.stderr)

        if not mr_created:
            mr_url, branch_url = compute_mr_web_url(
                project_root, branch_name, remote=remote, target_branch=target_branch
            )
            print(f'==> GitLab Merge Request URL:\n    {mr_url}')
            print(f'==> GitLab Fork Branch URL:\n    {branch_url}')

    except Exception:
        session_failed = True
        raise
    finally:
        # Cleanup worktree
        if keep or (session_failed and keep):
            print(f'==> Session retained for inspection: {session_dir}')
        else:
            if not session_failed or not keep:
                _run(['git', 'worktree', 'remove', '--force', worktree_dir], cwd=project_root)
                _run(['git', 'worktree', 'prune'], cwd=project_root)
                shutil.rmtree(session_dir, ignore_errors=True)
