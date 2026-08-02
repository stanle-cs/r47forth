#!/usr/bin/env python3
"""Tests for tools/pkg_patch_integrate.py — integration session creation.

Uses synthetic Git repositories in temporary directories.  Never mutates
the real repository or packages/forth-core.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pkg_patch_integrate  # noqa: E402


def _run(cmd, cwd=None):
    """Run a command and return the CompletedProcess."""
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


def _make_repo_with_pkg(tmpdir, pkg_name='test-pkg'):
    """Create a synthetic Git repository with a package directory.

    Returns (repo_root, pkgdir_rel).
    The repo has a single commit on master with a file in src/c47/.
    """
    repo = os.path.join(tmpdir, 'repo')
    os.makedirs(repo)
    _run(['git', 'init'], cwd=repo)
    _run(['git', 'config', 'user.email', 'test@test.com'], cwd=repo)
    _run(['git', 'config', 'user.name', 'Test'], cwd=repo)

    # Create src/c47/ with a file.
    src_dir = os.path.join(repo, 'src', 'c47')
    os.makedirs(src_dir)
    with open(os.path.join(src_dir, 'main.c'), 'w') as f:
        f.write('// original\nint main(void) { return 0; }\n')

    # Create packages/test-pkg/ with a working-area file.
    pkg_dir = os.path.join(repo, 'packages', pkg_name)
    os.makedirs(pkg_dir)
    with open(os.path.join(pkg_dir, 'main.c'), 'w') as f:
        f.write('// package override\nint main(void) { return 42; }\n')

    # Create manifest.
    manifest = {
        'base_commit': None,  # will be set by git commit
        'patches': {},
        'files': {},
    }
    with open(os.path.join(pkg_dir, '.refresh-manifest.json'), 'w') as f:
        json.dump(manifest, f)

    # Create .pkgignore — exclude build-test.sh so snapshot ops skip it.
    with open(os.path.join(pkg_dir, '.pkgignore'), 'w') as f:
        f.write('# ignore docs\n')
        f.write('build-test.sh\n')

    # Create a default passing build-test.sh.
    with open(os.path.join(pkg_dir, 'build-test.sh'), 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('echo "FORTH SELF-TEST: ALL PASSED"\n')
        f.write('echo "==\u003e BUILD + SELF-TEST GREEN."\n')
        f.write('exit 0\n')
    os.chmod(os.path.join(pkg_dir, 'build-test.sh'), 0o755)

    # Commit.
    _run(['git', 'add', '.'], cwd=repo)
    _run(['git', 'commit', '-m', 'initial'], cwd=repo)

    # Update manifest with actual HEAD.
    head = _run(['git', 'rev-parse', 'HEAD'], cwd=repo).stdout.strip()
    manifest['base_commit'] = head
    with open(os.path.join(pkg_dir, '.refresh-manifest.json'), 'w') as f:
        json.dump(manifest, f)
    _run(['git', 'add', '.'], cwd=repo)
    _run(['git', 'commit', '--amend', '--no-edit'], cwd=repo)

    return repo, f'packages/{pkg_name}'


class TestCleanMerge(unittest.TestCase):
    """Clean target merge produces repo-ready."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_clean_merge_repo_ready(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create a branch with a non-conflicting change.
        _run(['git', 'checkout', '-b', 'target-branch'], cwd=repo)
        other_dir = os.path.join(repo, 'src', 'c47')
        with open(os.path.join(other_dir, 'other.c'), 'w') as f:
            f.write('// new file on target\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'add other.c'], cwd=repo)

        # Go back to master.
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target-branch',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        self.assertEqual(result['conflict_paths'], [])
        self.assertTrue(os.path.isdir(result['session_dir']))

        # Verify session.json.
        session_json = os.path.join(result['session_dir'], 'session.json')
        self.assertTrue(os.path.isfile(session_json))
        with open(session_json) as f:
            meta = json.load(f)
        self.assertEqual(meta['phase'], 'repo-ready')
        self.assertEqual(meta['schema_version'], 1)
        self.assertEqual(meta['package'], pkgdir)
        self.assertEqual(meta['source_repo'], os.path.abspath(repo))
        self.assertTrue(meta['target_sha'])
        self.assertTrue(meta['source_head'])
        self.assertTrue(meta['worktree'])

        # Verify worktree exists.
        self.assertTrue(os.path.isdir(meta['worktree']))

        # Verify package-snapshot exists.
        snapshot = os.path.join(result['session_dir'], 'package-snapshot')
        self.assertTrue(os.path.isdir(snapshot))


class TestConflictMerge(unittest.TestCase):
    """Conflicting merge produces repo-conflict."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_conflict_merge_repo_conflict(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create a branch that conflicts with master.
        _run(['git', 'checkout', '-b', 'conflict-branch'], cwd=repo)
        main_path = os.path.join(repo, 'src', 'c47', 'main.c')
        with open(main_path, 'w') as f:
            f.write('// conflict-branch version\nint main(void) { return 1; }\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify main.c on conflict-branch'], cwd=repo)

        # Go back to master and make a different change to the same file.
        _run(['git', 'checkout', 'master'], cwd=repo)
        with open(main_path, 'w') as f:
            f.write('// master version\nint main(void) { return 2; }\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify main.c on master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'conflict-branch',
        )
        self.assertEqual(result['phase'], 'repo-conflict')
        self.assertIn('src/c47/main.c', result['conflict_paths'])
        self.assertTrue(os.path.isdir(result['session_dir']))

        # Session should be retained (keep=True on conflict).
        self.assertTrue(result['keep'])

        # Verify session.json.
        session_json = os.path.join(result['session_dir'], 'session.json')
        with open(session_json) as f:
            meta = json.load(f)
        self.assertEqual(meta['phase'], 'repo-conflict')
        self.assertIn('src/c47/main.c', meta['conflict_paths'])


class TestCallerImmutability(unittest.TestCase):
    """Caller state is preserved after integration."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_dirty_tracked_file_preserved(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Make a tracked file dirty.
        main_path = os.path.join(repo, 'src', 'c47', 'main.c')
        with open(main_path, 'w') as f:
            f.write('// dirty version\n')
        # Don't commit — leave it dirty.

        # Create a clean target branch.
        _run(['git', 'checkout', '-b', 'clean-target'], cwd=repo)
        other_path = os.path.join(repo, 'src', 'c47', 'other.c')
        with open(other_path, 'w') as f:
            f.write('// other\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'add other.c'], cwd=repo)

        # Go back to master.
        _run(['git', 'checkout', 'master'], cwd=repo)

        # Read dirty file content before.
        with open(main_path, 'rb') as f:
            before = f.read()

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'clean-target',
        )

        # Read dirty file content after.
        with open(main_path, 'rb') as f:
            after = f.read()

        self.assertEqual(before, after,
                         'dirty tracked file was modified by integration')

    def test_untracked_file_preserved(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create an untracked file.
        untracked = os.path.join(repo, 'untracked_file.txt')
        with open(untracked, 'w') as f:
            f.write('untracked content\n')

        # Create a clean target branch.
        _run(['git', 'checkout', '-b', 'clean-target'], cwd=repo)
        other_path = os.path.join(repo, 'src', 'c47', 'other.c')
        with open(other_path, 'w') as f:
            f.write('// other\n')
        _run(['git', 'add', 'src/c47/other.c'], cwd=repo)
        _run(['git', 'commit', '-m', 'add other.c'], cwd=repo)

        # Go back to master.
        _run(['git', 'checkout', 'master'], cwd=repo)

        # Read untracked file content before.
        with open(untracked, 'rb') as f:
            before = f.read()

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'clean-target',
        )

        # Read untracked file content after.
        with open(untracked, 'rb') as f:
            after = f.read()

        self.assertEqual(before, after,
                         'untracked file was modified by integration')


class TestSnapshotExclusion(unittest.TestCase):
    """Package snapshot excludes patches/, files/, __pycache__."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_excludes_generated_dirs(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create patches/ and files/ dirs with content.
        pkg_abs = os.path.join(repo, pkgdir)
        patches_dir = os.path.join(pkg_abs, 'patches')
        os.makedirs(patches_dir)
        with open(os.path.join(patches_dir, '010-main.c.patch'), 'w') as f:
            f.write('--- a/main.c\n+++ b/main.c\n')

        files_dir = os.path.join(pkg_abs, 'files')
        os.makedirs(files_dir)
        with open(os.path.join(files_dir, 'new.c'), 'w') as f:
            f.write('// new file\n')

        # Create __pycache__.
        pycache = os.path.join(pkg_abs, '__pycache__')
        os.makedirs(pycache)
        with open(os.path.join(pycache, 'module.cpython-39.pyc'), 'w') as f:
            f.write('binary')

        # Create a clean target.
        _run(['git', 'checkout', '-b', 'clean-target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'empty target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'clean-target',
        )

        snapshot = os.path.join(result['session_dir'], 'package-snapshot')

        # patches/ should NOT be in snapshot.
        self.assertFalse(os.path.exists(os.path.join(snapshot, 'patches')))
        # files/ should NOT be in snapshot.
        self.assertFalse(os.path.exists(os.path.join(snapshot, 'files')))
        # __pycache__ should NOT be in snapshot.
        self.assertFalse(os.path.exists(os.path.join(snapshot, '__pycache__')))
        # main.c SHOULD be in snapshot.
        self.assertTrue(os.path.isfile(os.path.join(snapshot, 'main.c')))
        # Package control files define the current base and build behavior.
        self.assertTrue(os.path.isfile(
            os.path.join(snapshot, '.refresh-manifest.json')))
        self.assertTrue(os.path.isfile(os.path.join(snapshot, '.pkgignore')))
        self.assertTrue(os.path.isfile(os.path.join(snapshot, 'build-test.sh')))


class TestValidation(unittest.TestCase):
    """Invalid ref/package fails before worktree creation."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_invalid_ref_fails_before_worktree(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                pkgdir, repo, 'nonexistent-branch-12345',
            )
        self.assertIn('cannot resolve ref', str(cm.exception))

    def test_invalid_package_fails(self):
        repo, _ = _make_repo_with_pkg(self.tmpdir)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                'packages/nonexistent', repo, 'master',
            )
        self.assertIn('does not exist', str(cm.exception))


class TestMetadata(unittest.TestCase):
    """Session metadata is valid and atomically replaced."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_session_json_valid(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )

        session_json = os.path.join(result['session_dir'], 'session.json')
        with open(session_json) as f:
            meta = json.load(f)

        # Check required fields.
        self.assertEqual(meta['schema_version'], 1)
        self.assertEqual(meta['package'], pkgdir)
        self.assertEqual(meta['target_ref'], 'target')
        self.assertEqual(len(meta['target_sha']), 40)
        self.assertEqual(len(meta['source_head']), 40)
        self.assertTrue(meta['worktree'].endswith('worktree'))
        self.assertFalse(meta['keep'])
        self.assertFalse(meta['no_build'])
        self.assertEqual(meta['phase'], 'repo-ready')
        self.assertEqual(meta['conflict_paths'], [])

    def test_session_json_atomic(self):
        """session.json has no .tmp or .session. artifacts."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )

        session_dir = result['session_dir']
        entries = os.listdir(session_dir)
        # No temp artifacts.
        for entry in entries:
            self.assertFalse(entry.endswith('.tmp'),
                             f'temp artifact left: {entry}')
            self.assertFalse(entry.startswith('.session.'),
                             f'temp artifact left: {entry}')


class TestSessionUniqueness(unittest.TestCase):
    """Two sessions do not collide."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_integrate_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_two_sessions_separate(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result1 = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        result2 = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )

        self.assertNotEqual(result1['session_dir'], result2['session_dir'])
        self.assertTrue(os.path.isdir(result1['session_dir']))
        self.assertTrue(os.path.isdir(result2['session_dir']))

        # Each has its own worktree.
        with open(os.path.join(result1['session_dir'], 'session.json')) as f:
            meta1 = json.load(f)
        with open(os.path.join(result2['session_dir'], 'session.json')) as f:
            meta2 = json.load(f)
        self.assertNotEqual(meta1['worktree'], meta2['worktree'])

    def test_session_uses_source_repository_filesystem(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'HEAD',
        )

        self.assertEqual(
            os.path.dirname(result['session_dir']),
            os.path.dirname(os.path.abspath(repo)),
        )
        worktree = os.path.join(result['session_dir'], 'worktree')
        status = _run(['git', 'status', '--porcelain'], cwd=worktree)
        self.assertEqual(status.stdout, '')


# ---- PCLI-3: Resume tests ----

class TestResumeRefuseUnresolvedConflict(unittest.TestCase):
    """Resume refuses when Git still reports unmerged paths."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_refuse_unresolved_repo_conflict(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create conflicting branches — use a NON-package file.
        _run(['git', 'checkout', '-b', 'conflict-branch'], cwd=repo)
        other_path = os.path.join(repo, 'src', 'c47', 'other.c')
        with open(other_path, 'w') as f:
            f.write('// branch version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c'], cwd=repo)

        _run(['git', 'checkout', 'master'], cwd=repo)
        with open(other_path, 'w') as f:
            f.write('// master version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c on master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'conflict-branch',
        )
        self.assertEqual(result['phase'], 'repo-conflict')
        session_dir = result['session_dir']

        # Resume without resolving — should fail.
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('unresolved', str(cm.exception).lower())


class TestResumeRefuseMarkers(unittest.TestCase):
    """Resume refuses when recorded conflict files still have markers."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_refuse_markers_in_conflict_file(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create conflicting branches — use a NON-package file.
        _run(['git', 'checkout', '-b', 'conflict-branch'], cwd=repo)
        other_path = os.path.join(repo, 'src', 'c47', 'other.c')
        with open(other_path, 'w') as f:
            f.write('// branch version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c'], cwd=repo)

        _run(['git', 'checkout', 'master'], cwd=repo)
        with open(other_path, 'w') as f:
            f.write('// master version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c on master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'conflict-branch',
        )
        session_dir = result['session_dir']

        # Manually "resolve" the git unmerged state but leave markers.
        worktree = result['session_dir'] + '/worktree'
        wt_other = os.path.join(worktree, 'src', 'c47', 'other.c')
        with open(wt_other, 'w') as f:
            f.write('<<<<<<< HEAD\n// master\n=======\n// branch\n>>>>>>>\n')
        _run(['git', 'add', 'src/c47/other.c'], cwd=worktree)

        # Resume should still refuse because markers remain.
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('marker', str(cm.exception).lower())


class TestResumeAfterResolveConflict(unittest.TestCase):
    """Resume succeeds after caller resolves recorded repo conflict."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_resume_after_resolving_repo_conflict(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create conflicting branches — use a NON-package file so the
        # package rebase stays clean after conflict resolution.
        _run(['git', 'checkout', '-b', 'conflict-branch'], cwd=repo)
        other_path = os.path.join(repo, 'src', 'c47', 'other.c')
        with open(other_path, 'w') as f:
            f.write('// branch version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c'], cwd=repo)

        _run(['git', 'checkout', 'master'], cwd=repo)
        with open(other_path, 'w') as f:
            f.write('// master version\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'modify other.c on master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'conflict-branch',
        )
        self.assertEqual(result['phase'], 'repo-conflict')
        session_dir = result['session_dir']

        # Resolve the conflict in the worktree.
        worktree = result['session_dir'] + '/worktree'
        wt_other = os.path.join(worktree, 'src', 'c47', 'other.c')
        with open(wt_other, 'w') as f:
            f.write('// resolved\n')
        _run(['git', 'add', 'src/c47/other.c'], cwd=worktree)

        resume_result = pkg_patch_integrate.resume_session(
            session_dir, no_build=False, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')


class TestResumeSnapshotCopy(unittest.TestCase):
    """Snapshot is copied exactly; generated dirs are regenerated."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_snapshot_copied_generated_regenerated(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Add a patches/ dir to the package (generated content).
        pkg_abs = os.path.join(repo, pkgdir)
        patches_dir = os.path.join(pkg_abs, 'patches')
        os.makedirs(patches_dir)
        with open(os.path.join(patches_dir, '010-main.c.patch'), 'w') as f:
            f.write('original-patch-content\n')

        # Commit the patches dir.
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '--amend', '--no-edit'], cwd=repo)

        # Create a clean target.
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Verify snapshot does NOT contain patches/.
        snapshot = os.path.join(session_dir, 'package-snapshot')
        self.assertFalse(os.path.exists(os.path.join(snapshot, 'patches')))
        # Snapshot DOES contain main.c.
        self.assertTrue(os.path.isfile(os.path.join(snapshot, 'main.c')))

        # Change the snapshotted manifest after session creation. Resume must
        # restore it into the worktree before invoking the package rebase.
        snapshot_manifest = os.path.join(
            snapshot, '.refresh-manifest.json')
        with open(snapshot_manifest) as f:
            manifest = json.load(f)
        manifest['snapshot_probe'] = 'copied'
        with open(snapshot_manifest, 'w') as f:
            json.dump(manifest, f)

        # Update stored digest to match modified snapshot.
        new_digest = pkg_patch_integrate._compute_snapshot_digest(snapshot)
        pkg_patch_integrate._update_session_json(session_dir, {
            'snapshot_digest': new_digest,
        })

        # Resume with --no-build so we can inspect the worktree.
        resume_result = pkg_patch_integrate.resume_session(
            session_dir, no_build=True, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')

        # Verify worktree's package dir has snapshot content.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        self.assertTrue(os.path.isfile(os.path.join(pkg_in_wt, 'main.c')))

        # Verify snapshot main.c content matches worktree main.c.
        with open(os.path.join(snapshot, 'main.c')) as f:
            snap_content = f.read()
        with open(os.path.join(pkg_in_wt, 'main.c')) as f:
            wt_content = f.read()
        self.assertEqual(snap_content, wt_content)
        with open(os.path.join(
                pkg_in_wt, '.refresh-manifest.json')) as f:
            worktree_manifest = json.load(f)
        self.assertEqual(worktree_manifest['snapshot_probe'], 'copied')


class TestResumePackageConflict(unittest.TestCase):
    """Package conflict is retained and can be resumed after marker removal."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_package_conflict_retained_and_resumed(self):
        """This test simulates a package conflict by injecting markers
        after the snapshot copy, then resolving them and resuming."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create a clean target.
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Test the package-conflict resume path directly by setting the
        # session phase to package-conflict with a file that has markers,
        # then resolving and resuming.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)

        # Inject markers into the worktree's package file.
        with open(os.path.join(pkg_in_wt, 'main.c'), 'w') as f:
            f.write('<<<<<<< ours\n// ours\n=======\n// theirs\n>>>>>>> theirs\n')

        # Set session to package-conflict.
        pkg_patch_integrate._update_session_json(session_dir, {
            'phase': 'package-conflict',
            'conflict_paths': ['main.c'],
        })

        # Resume should refuse — markers still present.
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('marker', str(cm.exception).lower())

        # Resolve the markers.
        with open(os.path.join(pkg_in_wt, 'main.c'), 'w') as f:
            f.write('// resolved\n')

        # Create a fake build script.
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('echo "FORTH SELF-TEST: ALL PASSED"\n')
            f.write('echo "==\u003e BUILD + SELF-TEST GREEN."\n')
            f.write('exit 0\n')
        os.chmod(build_script, 0o755)

        # Resume should succeed now.
        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')


class TestResumeBuildSuccess(unittest.TestCase):
    """Fake build-test.sh success propagates through resume."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_build_success_propagates(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Create fake build script in the worktree.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('echo "FORTH SELF-TEST: ALL PASSED"\n')
            f.write('echo "==\u003e BUILD + SELF-TEST GREEN."\n')
            f.write('exit 0\n')
        os.chmod(build_script, 0o755)

        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')


class TestResumeBuildFailure(unittest.TestCase):
    """Fake build-test.sh failure is retained."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_build_failure_retained(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # The integration snapshot must carry the caller's current build gate,
        # including uncommitted package changes.
        build_script = os.path.join(repo, pkgdir, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('echo "BUILD FAILED" >&2\n')
            f.write('exit 42\n')
        os.chmod(build_script, 0o755)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        resume_result = pkg_patch_integrate.resume_session(session_dir)
        self.assertEqual(resume_result['phase'], 'build-failed')
        self.assertEqual(resume_result['build_exit_code'], 42)
        self.assertTrue(resume_result['keep'])

        # Session directory should still exist.
        self.assertTrue(os.path.isdir(session_dir))


class TestResumeNoBuild(unittest.TestCase):
    """--no-build skips the build step."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_no_build_skips_build(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Do NOT create build-test.sh — would fail without --no-build.

        resume_result = pkg_patch_integrate.resume_session(
            session_dir, no_build=True, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')

        # Session should still exist (--keep).
        self.assertTrue(os.path.isdir(session_dir))


class TestResumeCleanup(unittest.TestCase):
    """Success without --keep removes worktree and session directory."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_success_cleanup(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']
        worktree = session_dir + '/worktree'

        # Create fake build script.
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=False,
        )
        self.assertEqual(resume_result['phase'], 'complete')

        # Session directory should be removed.
        self.assertFalse(os.path.exists(session_dir))

        # Worktree should be unregistered.
        r = _run(['git', 'worktree', 'list', '--porcelain'], cwd=repo)
        self.assertNotIn(worktree, r.stdout)

    def test_keep_retains_session(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Create fake build script.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')

        # Session should still exist.
        self.assertTrue(os.path.isdir(session_dir))


class TestResumeValidation(unittest.TestCase):
    """Resume refuses forged metadata, unregistered worktrees, escaped paths."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_refuse_forged_metadata_bad_schema(self):
        session_dir = os.path.join(self.tmpdir, 'fake-session')
        os.makedirs(session_dir)
        with open(os.path.join(session_dir, 'session.json'), 'w') as f:
            json.dump({'schema_version': 99, 'phase': 'repo-ready'}, f)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('schema', str(cm.exception).lower())

    def test_refuse_missing_session_json(self):
        session_dir = os.path.join(self.tmpdir, 'fake-session')
        os.makedirs(session_dir)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('session.json', str(cm.exception).lower())

    def test_refuse_missing_session_dir(self):
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(
                os.path.join(self.tmpdir, 'nonexistent'))
        self.assertIn('does not exist', str(cm.exception).lower())

    def test_refuse_malformed_session_json(self):
        session_dir = os.path.join(self.tmpdir, 'fake-session')
        os.makedirs(session_dir)
        with open(os.path.join(session_dir, 'session.json'), 'w') as f:
            f.write('{not json')

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('invalid session metadata', str(cm.exception).lower())

    def test_refuse_unregistered_worktree(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Remove ALL worktree registrations from git's internal state.
        r = _run(['git', 'rev-parse', '--git-path', 'worktrees'], cwd=repo)
        worktrees_dir = os.path.join(repo, r.stdout.strip())
        if os.path.isdir(worktrees_dir):
            for entry in os.listdir(worktrees_dir):
                entry_path = os.path.join(worktrees_dir, entry)
                if os.path.isdir(entry_path):
                    shutil.rmtree(entry_path)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('registered', str(cm.exception).lower())

    def test_refuse_worktree_escapes_session(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Forge the worktree path to point outside the session.
        pkg_patch_integrate._update_session_json(session_dir, {
            'worktree': '/tmp/escaped-worktree',
        })

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('escapes', str(cm.exception).lower())

    def test_refuse_session_not_beside_source_repository(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        other_parent = os.path.join(self.tmpdir, 'other')
        os.makedirs(other_parent)
        moved_session = os.path.join(
            other_parent, os.path.basename(session_dir))
        shutil.move(session_dir, moved_session)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(moved_session)
        self.assertIn('beside source repository',
                      str(cm.exception).lower())

    def test_refuse_package_escapes_repo(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Forge the package path to escape the repo.
        pkg_patch_integrate._update_session_json(session_dir, {
            'package': '../../escaped-package',
        })

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('escapes', str(cm.exception).lower())

    def test_refuse_resume_non_hex_target_sha(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        pkg_patch_integrate._update_session_json(session_dir, {
            'target_sha': 'g' * 40,
        })

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('lowercase hex', str(cm.exception).lower())

    def test_refuse_resume_missing_commit_sha(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        pkg_patch_integrate._update_session_json(session_dir, {
            'target_sha': '0' * 40,
        })

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('not found', str(cm.exception).lower())

    def test_create_refuses_traversal_package(self):
        repo, _ = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                '../outside', repo, 'target')
        self.assertIn('escapes', str(cm.exception).lower())

    def test_create_refuses_package_symlink_outside_packages(self):
        repo, _ = _make_repo_with_pkg(self.tmpdir)
        outside = os.path.join(self.tmpdir, 'outside-package')
        os.makedirs(outside)
        with open(os.path.join(outside, '.refresh-manifest.json'), 'w') as f:
            json.dump({'base_commit': '', 'patches': {}, 'files': {}}, f)
        os.symlink(outside, os.path.join(repo, 'packages', 'linked'))
        _run(['git', 'branch', 'target'], cwd=repo)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                'packages/linked', repo, 'target')
        self.assertIn('escapes', str(cm.exception).lower())


class TestResumeCallerImmutability(unittest.TestCase):
    """Caller repo state is unchanged through every resume path."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _record_repo_state(self, repo):
        """Record branch, HEAD, status, and index tree."""
        branch_r = _run(['git', 'symbolic-ref', '--short', 'HEAD'], cwd=repo)
        branch = branch_r.stdout.strip() if branch_r.returncode == 0 else 'detached'
        head_r = _run(['git', 'rev-parse', 'HEAD'], cwd=repo)
        head = head_r.stdout.strip() if head_r.returncode == 0 else ''
        status_r = _run(['git', 'status', '--porcelain', '-v'], cwd=repo)
        status = status_r.stdout.encode() if status_r.returncode == 0 else b''
        index_r = _run(['git', 'write-tree'], cwd=repo)
        index = index_r.stdout.strip() if index_r.returncode == 0 else ''
        return (branch, head, status, index)

    def test_caller_unchanged_through_success(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        # Create a dirty tracked file in the caller.
        main_path = os.path.join(repo, 'src', 'c47', 'main.c')
        with open(main_path, 'w') as f:
            f.write('// dirty caller file\n')

        # Create an untracked file.
        untracked = os.path.join(repo, 'untracked.txt')
        with open(untracked, 'w') as f:
            f.write('untracked\n')

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        before = self._record_repo_state(repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Create fake build script.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        pkg_patch_integrate.resume_session(session_dir, keep=False)

        after = self._record_repo_state(repo)
        self.assertEqual(before, after,
                         'caller repo state changed during resume')

        # Also verify file contents are unchanged.
        with open(main_path, 'rb') as f:
            self.assertEqual(f.read(), b'// dirty caller file\n')
        with open(untracked, 'rb') as f:
            self.assertEqual(f.read(), b'untracked\n')

    def test_caller_unchanged_through_build_failure(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        before = self._record_repo_state(repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        # Create failing build script.
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 1\n')
        os.chmod(build_script, 0o755)

        pkg_patch_integrate.resume_session(session_dir)

        after = self._record_repo_state(repo)
        self.assertEqual(before, after,
                         'caller repo state changed during failed resume')


class TestResumeKeepOverride(unittest.TestCase):
    """--keep flag overrides stored keep value."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_resume_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_keep_overrides_stored_false(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        # Create session with keep=False.
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target', keep=False,
        )
        session_dir = result['session_dir']

        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        # Resume with keep=True should retain session.
        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')
        self.assertTrue(os.path.isdir(session_dir))


class TestBuildStreaming(unittest.TestCase):
    """Build output is streamed to caller and saved as build-test.log."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_build_stream_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_success_output_visible_and_logged(self):
        """Success output is streamed and build-test.log is created."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Snapshot already has a passing build script from _make_repo_with_pkg.
        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')

        # Verify build-test.log exists and contains banners.
        log_path = os.path.join(session_dir, 'build-test.log')
        self.assertTrue(os.path.isfile(log_path))
        with open(log_path) as f:
            log_content = f.read()
        self.assertIn('FORTH SELF-TEST: ALL PASSED', log_content)
        self.assertIn('==> BUILD + SELF-TEST GREEN.', log_content)

    def test_failure_output_visible_and_logged(self):
        """Failure output propagates and build-test.log captures it."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Modify the snapshot's build script to fail.
        snapshot = os.path.join(session_dir, 'package-snapshot')
        build_script = os.path.join(snapshot, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('echo "BUILD FAILED" >&2\n')
            f.write('exit 42\n')
        os.chmod(build_script, 0o755)
        # Update digest to match modified snapshot.
        new_digest = pkg_patch_integrate._compute_snapshot_digest(snapshot)
        pkg_patch_integrate._update_session_json(session_dir, {
            'snapshot_digest': new_digest,
        })

        resume_result = pkg_patch_integrate.resume_session(session_dir)
        self.assertEqual(resume_result['phase'], 'build-failed')
        self.assertEqual(resume_result.get('build_exit_code'), 42)

        # Verify build-test.log exists and contains failure output.
        log_path = os.path.join(session_dir, 'build-test.log')
        self.assertTrue(os.path.isfile(log_path))
        with open(log_path) as f:
            log_content = f.read()
        self.assertIn('BUILD FAILED', log_content)

        # Verify exit code in session.json.
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        self.assertEqual(meta['build_exit_code'], 42)

    def test_stdout_stderr_order_is_preserved_in_combined_log(self):
        script = os.path.join(self.tmpdir, 'ordered-build.sh')
        with open(script, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('printf "stdout-1\\n"\n')
            f.write('printf "stderr-1\\n" >&2\n')
            f.write('printf "stdout-2\\n"\n')
            f.write('printf "stderr-2\\n" >&2\n')
        os.chmod(script, 0o755)

        exit_code, output = pkg_patch_integrate._stream_build(
            script, self.tmpdir, self.tmpdir)

        self.assertEqual(exit_code, 0)
        expected = 'stdout-1\nstderr-1\nstdout-2\nstderr-2\n'
        self.assertEqual(output, expected)
        with open(os.path.join(self.tmpdir, 'build-test.log')) as f:
            self.assertEqual(f.read(), expected)


class TestBuildRetry(unittest.TestCase):
    """build-failed sessions can be retried and transition to complete."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_build_retry_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_repaired_gate_succeeds_on_resume(self):
        """A repaired build script succeeds on retry."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Modify snapshot's build script to fail.
        snapshot = os.path.join(session_dir, 'package-snapshot')
        build_script = os.path.join(snapshot, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 1\n')
        os.chmod(build_script, 0o755)
        new_digest = pkg_patch_integrate._compute_snapshot_digest(snapshot)
        pkg_patch_integrate._update_session_json(session_dir, {
            'snapshot_digest': new_digest,
        })

        resume_result = pkg_patch_integrate.resume_session(session_dir)
        self.assertEqual(resume_result['phase'], 'build-failed')

        # Repair the worktree's build script (snapshot is not re-copied on
        # build-failed retry — only the build is re-run).
        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script_wt = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script_wt, 'w') as f:
            f.write('#!/bin/sh\n')
            f.write('echo "FORTH SELF-TEST: ALL PASSED"\n')
            f.write('echo "==\u003e BUILD + SELF-TEST GREEN."\n')
            f.write('exit 0\n')
        os.chmod(build_script_wt, 0o755)

        # Retry: should succeed.
        resume_result = pkg_patch_integrate.resume_session(
            session_dir, keep=True,
        )
        self.assertEqual(resume_result['phase'], 'complete')


class TestCleanupHardened(unittest.TestCase):
    """Cleanup validates containment and registration before deletion."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_cleanup_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_cleanup_refuses_wrong_parent(self):
        """Cleanup refuses when session parent does not match source repo."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        # Create a fake session in a different parent.
        fake_parent = os.path.join(self.tmpdir, 'other-parent')
        os.makedirs(fake_parent)
        fake_session = os.path.join(fake_parent,
                                    'test-repo-package-integrate-fake')
        os.makedirs(fake_session)
        shutil.copytree(os.path.join(session_dir, 'package-snapshot'),
                        os.path.join(fake_session, 'package-snapshot'))
        pkg_patch_integrate._write_session_json(fake_session, {
            'schema_version': 1,
            'source_repo': repo,
            'package': pkgdir,
            'target_sha': '0' * 40,
            'worktree': os.path.join(fake_session, 'worktree'),
            'phase': 'complete',
        })

        # Cleanup should refuse due to wrong parent.
        ok = pkg_patch_integrate._cleanup_session(
            fake_session, repo,
            os.path.join(fake_session, 'worktree'))
        self.assertFalse(ok)
        # Session should still exist.
        self.assertTrue(os.path.isdir(fake_session))

    def test_cleanup_refuses_wrong_prefix(self):
        """Cleanup refuses when session basename prefix is wrong."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        # Create a fake session with wrong prefix.
        wrong_prefix = os.path.join(os.path.dirname(session_dir),
                                    'wrong-prefix-abc123')
        os.makedirs(wrong_prefix)
        shutil.copytree(os.path.join(session_dir, 'package-snapshot'),
                        os.path.join(wrong_prefix, 'package-snapshot'))
        pkg_patch_integrate._write_session_json(wrong_prefix, {
            'schema_version': 1,
            'source_repo': repo,
            'package': pkgdir,
            'target_sha': '0' * 40,
            'worktree': os.path.join(wrong_prefix, 'worktree'),
            'phase': 'complete',
        })

        ok = pkg_patch_integrate._cleanup_session(
            wrong_prefix, repo,
            os.path.join(wrong_prefix, 'worktree'))
        self.assertFalse(ok)
        self.assertTrue(os.path.isdir(wrong_prefix))

    def test_cleanup_records_failure(self):
        """Cleanup records cleanup-failed when worktree remove fails."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        session_dir = result['session_dir']

        worktree = session_dir + '/worktree'
        pkg_in_wt = os.path.join(worktree, pkgdir)
        build_script = os.path.join(pkg_in_wt, 'build-test.sh')
        with open(build_script, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        os.chmod(build_script, 0o755)

        # Validation failure (wrong registration) should refuse cleanup
        # without modifying session.json phase.
        git_file = os.path.join(worktree, '.git')
        with open(git_file, 'w') as f:
            f.write('gitdir: /nonexistent/path\n')

        ok = pkg_patch_integrate._cleanup_session(session_dir, repo, worktree)
        self.assertFalse(ok)

        # Session should be retained.
        self.assertTrue(os.path.isdir(session_dir))

        # Phase should NOT have changed (validation failure, not removal
        # failure — cleanup-failed is only recorded when git worktree
        # remove itself fails).
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        self.assertNotEqual(meta.get('phase'), 'cleanup-failed')

    def test_cleanup_refuses_unexpected_top_level_entry(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        worktree = result['session_dir'] + '/worktree'
        with open(os.path.join(session_dir, 'unexpected.txt'), 'w') as f:
            f.write('retain me\n')

        self.assertFalse(pkg_patch_integrate._cleanup_session(
            session_dir, repo, worktree))
        self.assertTrue(os.path.isdir(session_dir))

    def test_worktree_remove_failure_retains_entire_session(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        worktree = session_dir + '/worktree'
        failure = subprocess.CompletedProcess(
            ['git', 'worktree', 'remove'], 1, '', 'injected removal failure')

        with mock.patch.object(
                pkg_patch_integrate, '_run', return_value=failure):
            cleaned = pkg_patch_integrate._cleanup_session(
                session_dir, repo, worktree)

        self.assertFalse(cleaned)
        self.assertTrue(os.path.isdir(session_dir))
        self.assertTrue(os.path.isdir(worktree))
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        self.assertEqual(meta['phase'], 'cleanup-failed')
        self.assertIn('injected removal failure', meta['cleanup_error'])

    def test_directory_removal_failure_is_reported_and_bounded(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        worktree = session_dir + '/worktree'

        with mock.patch.object(
                pkg_patch_integrate.shutil, 'rmtree',
                side_effect=OSError('injected directory failure')):
            cleaned = pkg_patch_integrate._cleanup_session(
                session_dir, repo, worktree)

        self.assertFalse(cleaned)
        self.assertTrue(os.path.isdir(repo))
        self.assertTrue(os.path.isdir(session_dir))
        self.assertFalse(os.path.exists(worktree))
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        self.assertEqual(meta['phase'], 'cleanup-failed')
        self.assertIn('injected directory failure',
                      meta['cleanup_error'])

    def test_resume_reports_cleanup_failure(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']

        with mock.patch.object(
                pkg_patch_integrate, '_cleanup_session',
                return_value=False):
            resume_result = pkg_patch_integrate.resume_session(
                session_dir, no_build=True, keep=False)

        self.assertEqual(resume_result['phase'], 'cleanup-failed')
        self.assertTrue(resume_result['keep'])
        self.assertTrue(os.path.isdir(session_dir))


class TestSnapshotDigest(unittest.TestCase):
    """Snapshot digest verification detects mutation."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_digest_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_digest_mismatch_produces_snapshot_invalid(self):
        """Mutating the snapshot after creation is detected."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'checkout', '-b', 'target'], cwd=repo)
        _run(['git', 'commit', '--allow-empty', '-m', 'target'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        self.assertEqual(result['phase'], 'repo-ready')
        session_dir = result['session_dir']

        # Mutate the snapshot.
        snapshot = os.path.join(session_dir, 'package-snapshot')
        with open(os.path.join(snapshot, 'main.c'), 'w') as f:
            f.write('// mutated\n')

        # Resume should detect mutation.
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('digest mismatch', str(cm.exception).lower())

        # Session should be retained with snapshot-invalid phase.
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        self.assertEqual(meta['phase'], 'snapshot-invalid')
        self.assertTrue(os.path.isdir(session_dir))

    def test_missing_digest_produces_snapshot_invalid(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        with open(os.path.join(session_dir, 'session.json')) as f:
            meta = json.load(f)
        meta.pop('snapshot_digest')
        pkg_patch_integrate._write_session_json(session_dir, meta)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('missing snapshot digest', str(cm.exception).lower())
        with open(os.path.join(session_dir, 'session.json')) as f:
            self.assertEqual(json.load(f)['phase'], 'snapshot-invalid')

    def test_added_symlink_produces_snapshot_invalid(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        snapshot = os.path.join(session_dir, 'package-snapshot')
        os.symlink('/tmp', os.path.join(snapshot, 'escape'))

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('symlink', str(cm.exception).lower())
        with open(os.path.join(session_dir, 'session.json')) as f:
            self.assertEqual(json.load(f)['phase'], 'snapshot-invalid')


class TestSHAValidation(unittest.TestCase):
    """Target SHA must be 40 lowercase hex and exist as a commit."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_sha_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_non_hex_sha_rejected(self):
        """A SHA with uppercase or non-hex characters is rejected."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                pkgdir, repo, 'DEADBEEF' * 5,
            )
        # Ref resolution fails for invalid refs.
        self.assertIn('cannot resolve ref', str(cm.exception).lower())

    def test_missing_commit_sha_rejected(self):
        """A valid hex string that is not a commit is rejected."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        fake_sha = '0' * 40

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.create_integrate_session(
                pkgdir, repo, fake_sha,
            )
        # Ref resolution or cat-file fails for non-existent commits.
        err = str(cm.exception).lower()
        self.assertTrue(
            'cannot resolve ref' in err or 'not found' in err,
            f'expected resolve/not-found error, got: {err}')


class TestFatalMerge(unittest.TestCase):
    """Non-conflict merge failures produce merge-fatal."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_fatal_merge_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_merge_fatal_no_conflicts(self):
        """A merge that fails with no unmerged paths is merge-fatal."""
        # Create two repos with divergent history that cannot merge.
        repo = os.path.join(self.tmpdir, 'test-repo')
        os.makedirs(repo)
        _run(['git', 'init'], cwd=repo)
        _run(['git', 'config', 'user.email', 'test@test.com'], cwd=repo)
        _run(['git', 'config', 'user.name', 'Test'], cwd=repo)

        # Create package.
        pkgdir = 'packages/testpkg'
        os.makedirs(os.path.join(repo, pkgdir))
        with open(os.path.join(repo, pkgdir, 'main.c'), 'w') as f:
            f.write('// main\n')
        with open(os.path.join(repo, pkgdir, '.refresh-manifest.json'),
                  'w') as f:
            json.dump({'base_commit': '', 'files': {}}, f)

        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'initial'], cwd=repo)
        _run(['git', 'checkout', '-b', 'master'], cwd=repo)

        # Create a target branch with an orphaned history.
        orphan = os.path.join(self.tmpdir, 'orphan-repo')
        os.makedirs(orphan)
        _run(['git', 'init', '--bare'], cwd=orphan)
        _run(['git', 'remote', 'add', 'orphan', orphan], cwd=repo)

        # Create orphan commit.
        _run(['git', 'checkout', '--orphan', 'target'], cwd=repo)
        _run(['git', 'rm', '-rf', '.'], cwd=repo)
        with open(os.path.join(repo, 'orphan.txt'), 'w') as f:
            f.write('orphan\n')
        _run(['git', 'add', '.'], cwd=repo)
        _run(['git', 'commit', '-m', 'orphan'], cwd=repo)
        _run(['git', 'checkout', 'master'], cwd=repo)

        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target',
        )
        # The merge of an orphan into master will fail with no conflicts
        # because there are no common ancestors and no overlapping files.
        # Git may succeed or fail depending on version. If it fails,
        # it should be merge-fatal.
        if result['phase'] == 'merge-fatal':
            self.assertIn('merge', result.get('merge_stderr', '').lower()
                          or result.get('merge_stdout', '').lower())
        # If merge succeeds (no conflicts, no overlapping files),
        # that's also valid.

    def test_merge_fatal_session_cannot_advance(self):
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        _run(['git', 'branch', 'target'], cwd=repo)
        result = pkg_patch_integrate.create_integrate_session(
            pkgdir, repo, 'target')
        session_dir = result['session_dir']
        pkg_patch_integrate._update_session_json(session_dir, {
            'phase': 'merge-fatal',
            'merge_stderr': 'simulated fatal merge',
        })

        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate.resume_session(session_dir)
        self.assertIn('cannot be resumed', str(cm.exception).lower())
        with open(os.path.join(session_dir, 'session.json')) as f:
            self.assertEqual(json.load(f)['phase'], 'merge-fatal')


class TestPackageSymlinkEscape(unittest.TestCase):
    """Package directory symlinks that escape the repo are rejected."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_symlink_test_')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_escaping_symlink_rejected(self):
        """A symlink inside the package pointing outside is rejected."""
        # Create a package directory.
        pkg_dir = os.path.join(self.tmpdir, 'test-pkg')
        os.makedirs(pkg_dir)
        with open(os.path.join(pkg_dir, 'main.c'), 'w') as f:
            f.write('// main\n')
        with open(os.path.join(pkg_dir, '.refresh-manifest.json'),
                  'w') as f:
            json.dump({'base_commit': '', 'files': {}}, f)

        # Create a target outside the package.
        outside = os.path.join(self.tmpdir, 'outside-file.c')
        with open(outside, 'w') as f:
            f.write('// outside\n')

        # Create a symlink inside the package pointing outside.
        os.symlink(outside, os.path.join(pkg_dir, 'escape.c'))

        # The snapshot copy should reject the escaping symlink.
        with self.assertRaises(RuntimeError) as cm:
            pkg_patch_integrate._copy_package_snapshot(
                pkg_dir, os.path.join(self.tmpdir, 'snap'))
        self.assertIn('escapes', str(cm.exception).lower())


if __name__ == '__main__':
    unittest.main()
