# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors
"""Automated tests for tools/pkg_patch_upstream.py.

Uses synthetic Git repositories in temporary directories to test package upstreaming
without mutating the real repository.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import pkg_patch_upstream
from pkg_patch_upstream import (
    UpstreamConflictError,
    UpstreamError,
    compute_mr_web_url,
    parse_package_list,
    register_c47_meson_sources,
    resolve_target_ref,
    upstream,
)

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def _run(cmd, cwd=None):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)


class TestParsePackageList(unittest.TestCase):
    """Test parse_package_list resolution and validation."""

    def test_single_bare_and_prefixed(self):
        pkgs = parse_package_list('undo-history', _REPO_ROOT)
        self.assertEqual(pkgs, ['packages/undo-history'])

        pkgs2 = parse_package_list('packages/undo-history', _REPO_ROOT)
        self.assertEqual(pkgs2, ['packages/undo-history'])

    def test_comma_separated_multi(self):
        pkgs = parse_package_list(
            'undo-history, packages/pretty-print', _REPO_ROOT)
        self.assertEqual(pkgs, ['packages/undo-history', 'packages/pretty-print'])

    def test_empty_or_whitespace_raises(self):
        with self.assertRaises(UpstreamError):
            parse_package_list('', _REPO_ROOT)
        with self.assertRaises(UpstreamError):
            parse_package_list('   ,   ', _REPO_ROOT)

    def test_duplicate_raises(self):
        with self.assertRaises(UpstreamError):
            parse_package_list('undo-history,packages/undo-history', _REPO_ROOT)

    def test_nonexistent_package_raises(self):
        with self.assertRaises(UpstreamError):
            parse_package_list('non-existent-pkg-xyz', _REPO_ROOT)


class TestRegisterMesonSources(unittest.TestCase):
    """Test register_c47_meson_sources updating src/c47/meson.build."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='test_meson_')
        self.meson_file = os.path.join(self.tmpdir, 'meson.build')

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_insert_in_c47_src(self):
        initial = (
            "c47_src = files(\n"
            "  'assign.c',\n"
            "  'browsers/asnBrowser.c',\n"
            "  'c47.c'\n"
            ")\n"
            "\n"
            "other_src = files('foo.c')\n"
        )
        with open(self.meson_file, 'w') as f:
            f.write(initial)

        added = register_c47_meson_sources(
            self.meson_file,
            ['undoHistory.c', 'browsers/historyBrowser.c']
        )
        self.assertEqual(added, ['browsers/historyBrowser.c', 'undoHistory.c'])

        with open(self.meson_file) as f:
            updated = f.read()

        self.assertIn("'browsers/historyBrowser.c',", updated)
        self.assertIn("'undoHistory.c',", updated)
        self.assertIn("c47_src = files(", updated)
        self.assertIn("other_src = files('foo.c')", updated)

    def test_idempotent_when_already_present(self):
        initial = (
            "c47_src = files(\n"
            "  'assign.c',\n"
            "  'undoHistory.c',\n"
            "  'c47.c'\n"
            ")\n"
        )
        with open(self.meson_file, 'w') as f:
            f.write(initial)

        added = register_c47_meson_sources(self.meson_file, ['undoHistory.c'])
        self.assertEqual(added, [])


class TestComputeMrWebUrl(unittest.TestCase):
    """Test compute_mr_web_url."""

    def test_url_generation(self):
        mr_url, branch_url = compute_mr_web_url(_REPO_ROOT, 'mr/my-feature', target_branch='master')
        self.assertTrue(mr_url.startswith('https://gitlab.com/'))
        self.assertIn('merge_request[source_branch]=mr%2Fmy-feature', mr_url)
        self.assertIn('merge_request[target_branch]=master', mr_url)
        self.assertTrue(branch_url.startswith('https://gitlab.com/'))
        self.assertIn('tree/mr%2Fmy-feature', branch_url)


class TestUpstreamSynthetic(unittest.TestCase):
    """Test full upstream command execution against synthetic git repository."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='test_pkg_upstream_')
        self.repo = os.path.join(self.tmpdir, 'repo')
        os.makedirs(self.repo)

        _run(['git', 'init', '-b', 'master'], cwd=self.repo)
        _run(['git', 'config', 'user.email', 'test@example.com'], cwd=self.repo)
        _run(['git', 'config', 'user.name', 'Test'], cwd=self.repo)

        # Create upstream structure
        src_c47 = os.path.join(self.repo, 'src', 'c47')
        src_test = os.path.join(self.repo, 'src', 'testSuite', 'tests')
        os.makedirs(src_c47, exist_ok=True)
        os.makedirs(src_test, exist_ok=True)

        with open(os.path.join(src_c47, 'main.c'), 'w') as f:
            f.write("// Original upstream main\nint main(void) { return 0; }\n")

        with open(os.path.join(src_c47, 'meson.build'), 'w') as f:
            f.write("c47_src = files(\n  'main.c'\n)\n")

        with open(os.path.join(src_test, 'testSuiteList.txt'), 'w') as f:
            f.write("test1.txt\n")

        _run(['git', 'add', 'src/'], cwd=self.repo)
        _run(['git', 'commit', '-m', 'Initial upstream commit'], cwd=self.repo)

        # Tag as upstream/master
        _run(['git', 'branch', 'upstream/master'], cwd=self.repo)

        # Now create a package
        pkg_dir = os.path.join(self.repo, 'packages', 'demo-feature')
        os.makedirs(os.path.join(pkg_dir, 'files', 'testSuite', 'tests'), exist_ok=True)
        os.makedirs(os.path.join(pkg_dir, 'patches'), exist_ok=True)

        with open(os.path.join(pkg_dir, 'files', 'feature.c'), 'w') as f:
            f.write("// new feature file\nint feature(void) { return 42; }\n")

        with open(os.path.join(pkg_dir, 'files', 'testSuite', 'tests', 'demo.txt'), 'w') as f:
            f.write("// test case\n")

        # Create patch for main.c
        patch_content = (
            "diff --git a/src/c47/main.c b/src/c47/main.c\n"
            "--- a/src/c47/main.c\n"
            "+++ b/src/c47/main.c\n"
            "@@ -1,2 +1,3 @@\n"
            " // Original upstream main\n"
            "+// Patched by demo-feature\n"
            " int main(void) { return 0; }\n"
        )
        with open(os.path.join(pkg_dir, 'patches', '010-main.c.patch'), 'w') as f:
            f.write(patch_content)

        manifest = {
            'base_commit': _run(['git', 'rev-parse', 'HEAD'], cwd=self.repo).stdout.strip(),
            'patches': {'010-main.c.patch': 'hash1'},
            'files': {'feature.c': 'hash2', 'testSuite/tests/demo.txt': 'hash3'}
        }
        with open(os.path.join(pkg_dir, '.refresh-manifest.json'), 'w') as f:
            json.dump(manifest, f)

        # Also add a dirty uncommitted file in repo root to verify caller immutability
        with open(os.path.join(self.repo, 'dirty_scratch.txt'), 'w') as f:
            f.write('untracked working file')

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def test_upstream_folding_creates_clean_branch(self):
        upstream(
            packages_arg='demo-feature',
            project_root=self.repo,
            onto_ref='upstream/master',
            branch_name='mr/demo-feature',
            no_build=True,
            keep=False,
        )

        # Verify caller branch/status didn't lose dirty_scratch.txt
        self.assertTrue(os.path.isfile(os.path.join(self.repo, 'dirty_scratch.txt')))

        # Verify local branch mr/demo-feature was created
        r = _run(['git', 'rev-parse', 'refs/heads/mr/demo-feature'], cwd=self.repo)
        self.assertEqual(r.returncode, 0)
        commit_sha = r.stdout.strip()

        # Check commit tree: should contain src/c47/main.c, src/c47/feature.c,
        # src/testSuite/tests/demo.txt, but NO packages/ directory
        ls_tree = _run(['git', 'ls-tree', '-r', '--name-only', commit_sha], cwd=self.repo)
        files_in_commit = ls_tree.stdout.splitlines()

        self.assertIn('src/c47/main.c', files_in_commit)
        self.assertIn('src/c47/feature.c', files_in_commit)
        self.assertIn('src/testSuite/tests/demo.txt', files_in_commit)
        self.assertIn('src/c47/meson.build', files_in_commit)

        # Ensure no package files leaked into the upstream commit
        for path in files_in_commit:
            self.assertFalse(path.startswith('packages/'), f'leaked package file: {path}')
            self.assertFalse('manifest' in path)

        # Verify patched content
        main_content = _run(['git', 'show', f'{commit_sha}:src/c47/main.c'], cwd=self.repo).stdout
        self.assertIn('// Patched by demo-feature', main_content)

        # Verify meson.build content has feature.c
        meson_content = _run(['git', 'show', f'{commit_sha}:src/c47/meson.build'], cwd=self.repo).stdout
        self.assertIn("'feature.c',", meson_content)

    def test_multi_package_folding(self):
        # Create second package
        pkg2_dir = os.path.join(self.repo, 'packages', 'extra-feature')
        os.makedirs(os.path.join(pkg2_dir, 'files'), exist_ok=True)
        os.makedirs(os.path.join(pkg2_dir, 'patches'), exist_ok=True)

        with open(os.path.join(pkg2_dir, 'files', 'extra.c'), 'w') as f:
            f.write("// extra feature\nint extra(void) { return 99; }\n")

        manifest2 = {
            'base_commit': _run(['git', 'rev-parse', 'HEAD'], cwd=self.repo).stdout.strip(),
            'patches': {},
            'files': {'extra.c': 'hash4'}
        }
        with open(os.path.join(pkg2_dir, '.refresh-manifest.json'), 'w') as f:
            json.dump(manifest2, f)

        upstream(
            packages_arg='packages/demo-feature, extra-feature',
            project_root=self.repo,
            onto_ref='upstream/master',
            branch_name='mr/combo-feature',
            no_build=True,
            keep=False,
        )

        r = _run(['git', 'rev-parse', 'refs/heads/mr/combo-feature'], cwd=self.repo)
        self.assertEqual(r.returncode, 0)
        commit_sha = r.stdout.strip()

        ls_tree = _run(['git', 'ls-tree', '-r', '--name-only', commit_sha], cwd=self.repo)
        files_in_commit = ls_tree.stdout.splitlines()

        self.assertIn('src/c47/feature.c', files_in_commit)
        self.assertIn('src/c47/extra.c', files_in_commit)

        meson_content = _run(['git', 'show', f'{commit_sha}:src/c47/meson.build'], cwd=self.repo).stdout
        self.assertIn("'feature.c',", meson_content)
        self.assertIn("'extra.c',", meson_content)

    def test_upstream_conflict_handling(self):
        # Mutate upstream main.c in a new upstream commit to conflict with patch
        src_main = os.path.join(self.repo, 'src', 'c47', 'main.c')
        with open(src_main, 'w') as f:
            f.write("// Upstream completely diverged main\nvoid completely_different(void) {}\n")
        _run(['git', 'add', src_main], cwd=self.repo)
        _run(['git', 'commit', '-m', 'Diverge upstream main'], cwd=self.repo)
        _run(['git', 'branch', '-f', 'upstream/master', 'HEAD'], cwd=self.repo)

        with self.assertRaises(UpstreamConflictError):
            upstream(
                packages_arg='demo-feature',
                project_root=self.repo,
                onto_ref='upstream/master',
                branch_name='mr/will-fail',
                no_build=True,
                keep=False,
            )


if __name__ == '__main__':
    unittest.main()
