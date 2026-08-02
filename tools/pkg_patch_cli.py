#!/usr/bin/env python3
"""Package management CLI — parser, commands, and dispatch.

Imported by the thin ./package entry-point.
"""
import argparse
import os
import subprocess
import sys

# Resolve repository root: this module lives in tools/, repo root is one level up.
_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# Ensure sibling modules in tools/ are importable.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pkg_patch_refresh  # noqa: E402
import pkg_patch_integrate  # noqa: E402


def _resolve_package(name):
    """Resolve *name* to a repository-relative package path.

    Accepts bare names like ``forth-core`` and ``packages/forth-core``.
    Rejects absolute paths, ``..`` components, empty names, symlinks that
    escape ``packages/``, and anything without a ``.refresh-manifest.json``.
    Returns ``packages/<name>``.
    """
    if not name:
        raise RuntimeError('package name must not be empty')

    if os.path.isabs(name):
        raise RuntimeError(
            f'package path must be relative, not absolute: {name!r}')

    norm = os.path.normpath(name)
    if norm.startswith('..'):
        raise RuntimeError(
            f'package path must not contain traversal: {name!r}')

    if norm.startswith('packages/'):
        bare = norm[len('packages/'):]
    else:
        bare = norm

    if not bare:
        raise RuntimeError('package name must not be empty')

    resolved = os.path.join('packages', bare)
    pkgdir_abs = os.path.join(_REPO_ROOT, resolved)

    if not os.path.isdir(pkgdir_abs):
        raise RuntimeError(
            f'package directory does not exist: {resolved}')

    # Reject package-directory symlinks that escape packages/.
    packages_real = os.path.realpath(os.path.join(_REPO_ROOT, 'packages'))
    pkgdir_real = os.path.realpath(pkgdir_abs)
    if not (pkgdir_real.startswith(packages_real + os.sep) or
            pkgdir_real == packages_real):
        raise RuntimeError(
            f'package directory escapes packages/: {resolved}')

    manifest = os.path.join(pkgdir_abs, '.refresh-manifest.json')
    if not os.path.isfile(manifest):
        raise RuntimeError(
            f'not a recognised package — missing .refresh-manifest.json: '
            f'{resolved}')

    return resolved


def _cmd_refresh(args):
    pkgdir = _resolve_package(args.package)
    result = pkg_patch_refresh.refresh(pkgdir, _REPO_ROOT)
    for w in result['warnings']:
        print(f'warning: {w}', file=sys.stderr)
    for fname in result['written']:
        print(f'wrote patches/{fname}')
    for rel in result['files_written']:
        print(f'wrote files/{rel}')
    for fname in result['removed']:
        print(f'removed patches/{fname} (no longer producible from the '
              f'working area)')
    for rel in result['files_removed']:
        print(f'removed files/{rel} (no longer producible from the '
              f'working area)')
    if not any(result[k] for k in
               ('written', 'files_written', 'removed', 'files_removed')):
        print(f'no changes under {pkgdir} — patches/ and files/ already '
              f'up to date')


def _cmd_materialize(args):
    pkgdir = _resolve_package(args.package)
    pkg_patch_refresh.materialize(pkgdir, args.rel, _REPO_ROOT)


def _cmd_rebase(args):
    pkgdir = _resolve_package(args.package)
    onto = args.onto if args.onto else 'HEAD'
    try:
        preflight = pkg_patch_refresh.check_rebase_preflight(
            _REPO_ROOT, onto)
    except RuntimeError as e:
        print(f'error: {e}', file=sys.stderr)
        sys.exit(1)
    if not preflight['buildable']:
        print(
            f'warning: the package can be rebased, but it is not buildable '
            f'in this checkout',
            file=sys.stderr,
        )
        bare = pkgdir[len('packages/'):]
        print(
            f'use: ./package integrate {bare} --onto {onto}',
            file=sys.stderr,
        )
    result = pkg_patch_refresh.rebase_base(pkgdir, onto, _REPO_ROOT)
    for w in result['warnings']:
        print(f'warning: {w}', file=sys.stderr)
    if result['old_base'] is None:
        print(f"base initialized to {result['new_base'][:12]}")
        return
    if result['old_base'] == result['new_base']:
        print(f"base already at {result['new_base'][:12]} — "
              f"nothing to do")
        return
    for rel in result['fast_forwarded']:
        print(f'fast-forwarded {rel}')
    for rel in result['merged']:
        print(f'merged {rel}')
    for rel in result['conflicted']:
        print(f'CONFLICT in {rel} — resolve the markers, then '
              f're-run refresh')
    print(f"base: {result['old_base'][:12]} -> "
          f"{result['new_base'][:12]}")


def _cmd_build(args):
    pkgdir = _resolve_package(args.package)
    pkgdir_abs = os.path.join(_REPO_ROOT, pkgdir)
    script = os.path.join(pkgdir_abs, 'build-test.sh')
    if not os.path.isfile(script):
        print(f'error: build script not found: {script}', file=sys.stderr)
        sys.exit(1)
    r = subprocess.run([script], cwd=pkgdir_abs)
    sys.exit(r.returncode)


def _cmd_audit(args):
    pkgdir = _resolve_package(args.package)
    bare = pkgdir[len('packages/'):]
    script = os.path.join(_REPO_ROOT, 'design-docs', bare, 'design-audit.sh')
    if not os.path.isfile(script):
        print(f'error: audit script not found: {script}', file=sys.stderr)
        sys.exit(1)
    r = subprocess.run([script], cwd=_REPO_ROOT)
    sys.exit(r.returncode)


def _cmd_integrate(args):
    pkgdir = _resolve_package(args.package)
    result = pkg_patch_integrate.create_integrate_session(
        pkgdir, _REPO_ROOT, args.onto,
        keep=args.keep, no_build=args.no_build,
    )
    session_dir = result['session_dir']
    print(f'session: {session_dir}')

    if result['phase'] == 'repo-conflict':
        for path in result['conflict_paths']:
            print(f'CONFLICT: {path}')
        print(f'./package resume {session_dir}')
        sys.exit(1)
    elif result['phase'] == 'merge-fatal':
        print(f'merge failed with no conflicts')
        if result.get('merge_stdout'):
            print(result['merge_stdout'])
        if result.get('merge_stderr'):
            print(result['merge_stderr'], file=sys.stderr)
        print('Fix the Git failure, then start a new integration session.')
        sys.exit(1)
    elif result['phase'] == 'repo-ready':
        _resume_from_integrate(session_dir, args)
    else:
        print(f'phase: {result["phase"]}')


def _resume_from_integrate(session_dir, args):
    """Resume a session created by ``integrate`` after repo-ready."""
    resume_result = pkg_patch_integrate.resume_session(
        session_dir,
        keep=args.keep,
        no_build=args.no_build,
    )
    print(f'phase: {resume_result["phase"]}')

    if resume_result['phase'] == 'package-conflict':
        for path in resume_result.get('conflict_paths', []):
            print(f'CONFLICT: {path}')
        print(f'./package resume {session_dir}')
        sys.exit(1)

    if resume_result['phase'] == 'build-failed':
        sys.exit(resume_result.get('build_exit_code', 1))

    if resume_result['phase'] == 'cleanup-failed':
        sys.exit(1)


def _cmd_resume(args):
    session_dir = os.path.abspath(args.session)
    resume_result = pkg_patch_integrate.resume_session(
        session_dir,
        keep=args.keep,
        no_build=args.no_build,
    )
    print(f'session: {resume_result["session_dir"]}')
    print(f'phase: {resume_result["phase"]}')

    if resume_result['phase'] == 'package-conflict':
        for path in resume_result.get('conflict_paths', []):
            print(f'CONFLICT: {path}')
        print(f'./package resume {resume_result["session_dir"]}')
        sys.exit(1)

    if resume_result['phase'] == 'build-failed':
        sys.exit(resume_result.get('build_exit_code', 1))

    if resume_result['phase'] == 'cleanup-failed':
        sys.exit(1)


def _cmd_status(args):
    pkgdir = _resolve_package(args.package)
    pkgdir_abs = os.path.join(_REPO_ROOT, pkgdir)
    bare = pkgdir[len('packages/'):]

    manifest = pkg_patch_refresh.load_manifest(pkgdir_abs)
    base_sha = manifest.get('base_commit')

    # Resolve caller HEAD.
    r = subprocess.run(
        ['git', 'rev-parse', 'HEAD'],
        capture_output=True, text=True, cwd=_REPO_ROOT)
    if r.returncode != 0:
        print('error: cannot resolve HEAD', file=sys.stderr)
        sys.exit(1)
    head_sha = r.stdout.strip()

    # Resolve optional target.
    target_sha = None
    if args.onto:
        r = subprocess.run(
            ['git', 'rev-parse', f'{args.onto}^{{commit}}'],
            capture_output=True, text=True, cwd=_REPO_ROOT)
        if r.returncode != 0:
            print(f'error: cannot resolve {args.onto!r}', file=sys.stderr)
            sys.exit(1)
        target_sha = r.stdout.strip()

    # Check manifest-base src/c47 == caller HEAD:src/c47.
    # Use git ls-tree to get the tree object for src/c47 at each commit.
    # Note: git ls-tree returns exit 0 even for missing paths (empty stdout).
    base_tree_matches_head = False
    if base_sha:
        r_base = subprocess.run(
            ['git', 'ls-tree', base_sha, 'src/c47'],
            capture_output=True, text=True, cwd=_REPO_ROOT)
        r_head = subprocess.run(
            ['git', 'ls-tree', head_sha, 'src/c47'],
            capture_output=True, text=True, cwd=_REPO_ROOT)
        if r_base.returncode != 0 or not r_base.stdout.strip():
            print(f'error: failed probe: git ls-tree '
                  f'{base_sha} src/c47: {r_base.stderr.strip() or "path not found"}',
                  file=sys.stderr)
            sys.exit(1)
        if r_head.returncode != 0 or not r_head.stdout.strip():
            print(f'error: failed probe: git ls-tree '
                  f'{head_sha} src/c47: {r_head.stderr.strip() or "path not found"}',
                  file=sys.stderr)
            sys.exit(1)
        # Extract tree SHA from "040000 tree <sha>\t<name>"
        base_tree = r_base.stdout.split()[2]
        head_tree = r_head.stdout.split()[2]
        base_tree_matches_head = base_tree == head_tree

    # Check caller src/c47 is dirty.
    r_status = subprocess.run(
        ['git', 'status', '--porcelain', '--', 'src/c47'],
        capture_output=True, text=True, cwd=_REPO_ROOT)
    if r_status.returncode != 0:
        print(f'error: failed probe: git status --porcelain src/c47: '
              f'{r_status.stderr.strip()}', file=sys.stderr)
        sys.exit(1)
    src_c47_dirty = bool(r_status.stdout.strip())

    # Check generated patches/ or files/ differ in Git.
    r_gen = subprocess.run(
        ['git', 'status', '--porcelain', '--', pkgdir + '/patches',
         pkgdir + '/files'],
        capture_output=True, text=True, cwd=_REPO_ROOT)
    if r_gen.returncode != 0:
        print(f'error: failed probe: git status --porcelain '
              f'{pkgdir}/patches {pkgdir}/files: {r_gen.stderr.strip()}',
              file=sys.stderr)
        sys.exit(1)
    generated_dirty = bool(r_gen.stdout.strip())

    # Check conflict markers in package working files.
    working_files = pkg_patch_refresh.list_working_files(pkgdir_abs)
    conflict_files = []
    for rel in working_files:
        path = os.path.join(pkgdir_abs, rel)
        if pkg_patch_refresh._working_file_marker_lines(path):
            conflict_files.append(rel)

    # Determine buildability: requires matching trees, clean src/c47,
    # and no conflict markers.
    locally_buildable = (
        base_tree_matches_head and not src_c47_dirty and not conflict_files)

    # Print report.
    print(f'package: {bare}')
    print(f'manifest base: {base_sha[:12] if base_sha else "(none)"}')
    print(f'caller HEAD: {head_sha[:12]}')
    if target_sha:
        print(f'target: {args.onto} ({target_sha[:12]})')
    print(f'manifest-base src/c47 == caller HEAD:src/c47: '
          f'{"yes" if base_tree_matches_head else "no"}')
    print(f'caller src/c47 dirty: {"yes" if src_c47_dirty else "no"}')
    print(f'generated patches/ or files/ differ in Git: '
          f'{"yes" if generated_dirty else "no"}')
    if conflict_files:
        print(f'conflict markers in working files: yes')
        for rel in conflict_files:
            print(f'  {rel}')
    else:
        print(f'conflict markers in working files: no')
    print(f'locally buildable: {"yes" if locally_buildable else "no"}')

    if not locally_buildable:
        onto = args.onto if args.onto else 'HEAD'
        print(f'')
        print(f'==> use: ./package integrate {bare} --onto {onto}')

    if base_sha is None:
        sys.exit(1)


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog='package',
        description='Package management entry point.',
    )
    subs = parser.add_subparsers(dest='command')

    sp = subs.add_parser('refresh', help='Regenerate patches/ and files/')
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.set_defaults(func=_cmd_refresh)

    sp = subs.add_parser(
        'materialize',
        help='Materialize an upstream file at the package base commit',
    )
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.add_argument('rel', help='Relative path under src/c47/')
    sp.set_defaults(func=_cmd_materialize)

    sp = subs.add_parser('rebase', help='Rebase package base to a new commit')
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.add_argument(
        '--onto', default=None, metavar='REF',
        help='Target commit (default: HEAD)',
    )
    sp.set_defaults(func=_cmd_rebase)

    sp = subs.add_parser('build', help='Run the package build-test.sh')
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.set_defaults(func=_cmd_build)

    sp = subs.add_parser('audit', help='Run the package design-audit.sh')
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.set_defaults(func=_cmd_audit)

    sp = subs.add_parser(
        'integrate',
        help='Create an integration session against a target ref',
    )
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.add_argument(
        '--onto', required=True, metavar='REF',
        help='Target ref to merge',
    )
    sp.add_argument(
        '--keep', action='store_true', default=False,
        help='Retain session on success',
    )
    sp.add_argument(
        '--no-build', action='store_true', default=False,
        help='Skip the build step',
    )
    sp.set_defaults(func=_cmd_integrate)

    sp = subs.add_parser(
        'resume',
        help='Resume an integration session from its current phase',
    )
    sp.add_argument(
        'session',
        help='Path to the session directory',
    )
    sp.add_argument(
        '--keep', action='store_true', default=None,
        help='Retain session on success',
    )
    sp.add_argument(
        '--no-build', action='store_true', default=None,
        help='Skip the build step',
    )
    sp.set_defaults(func=_cmd_resume)

    sp = subs.add_parser(
        'status',
        help='Show package buildability status',
    )
    sp.add_argument('package', help='Package name or packages/<name>')
    sp.add_argument(
        '--onto', default=None, metavar='REF',
        help='Optional target ref to check against',
    )
    sp.set_defaults(func=_cmd_status)

    args = parser.parse_args(argv)
    if args.command is None:
        parser.print_help()
        return 1
    try:
        args.func(args)
        return 0
    except SystemExit as e:
        return e.code if isinstance(e.code, int) else 1
    except RuntimeError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1
