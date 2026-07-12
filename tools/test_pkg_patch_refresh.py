#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_refresh.py (plain-diff design).

Run via: python3 tools/test_pkg_patch_refresh.py
  or:    meson test -C build.sim pkg_patch_refresh

Each test's docstring names the specific bug / mutation it must catch.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_refresh import (
    refresh,
    list_materialized_files,
    generate_patch,
)
from pkg_patch_common import decode_patch_filename, parse_patch_target

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_TOOLS_DIR, '..'))


class _TempProject:
    """Temp project-root-shaped git repo:
      <tmp>/src/c47/...             — upstream (committed to git)
      <tmp>/<pkgdir>/...            — materialized working copies
      <tmp>/<pkgdir>/patches/       — refresh's output
    """

    def __init__(self, pkgdir='packages/test-pkg'):
        self.pkgdir = pkgdir

    def __enter__(self):
        self.tmpdir = tempfile.mkdtemp()
        self.src_c47 = os.path.join(self.tmpdir, 'src', 'c47')
        self.pkg_abs = os.path.join(self.tmpdir, self.pkgdir)
        os.makedirs(self.src_c47, exist_ok=True)
        os.makedirs(self.pkg_abs, exist_ok=True)
        for args in (['init', '-q'],
                     ['config', 'user.email', 'test@localhost'],
                     ['config', 'user.name', 'Test']):
            subprocess.run(['git'] + args, cwd=self.tmpdir,
                           capture_output=True)
        return self

    def write_upstream(self, rel, content, commit=True):
        path = os.path.join(self.src_c47, *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)
        if commit:
            subprocess.run(['git', 'add', '-A'], cwd=self.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'upstream'],
                           cwd=self.tmpdir, capture_output=True)

    def write_materialized(self, rel, content):
        path = os.path.join(self.pkg_abs, *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)

    def refresh(self):
        return refresh(self.pkgdir, self.tmpdir)

    def patches_dir(self):
        return os.path.join(self.pkg_abs, 'patches')

    def list_patches(self):
        d = self.patches_dir()
        if not os.path.isdir(d):
            return []
        return sorted(f for f in os.listdir(d) if f.endswith('.patch'))

    def patch_content(self, fname):
        with open(os.path.join(self.patches_dir(), fname)) as f:
            return f.read()

    def apply_patch_and_get_result(self, fname, rel):
        """git-apply patches_dir/fname against a fresh copy of the
        committed upstream at src/c47/<rel>, return the resulting
        content. The real tree is restored afterward."""
        src_path = os.path.join(self.src_c47, *rel.split('/'))
        with open(src_path) as f:
            original = f.read()
        try:
            result = subprocess.run(
                ['git', 'apply', os.path.join(self.patches_dir(), fname)],
                cwd=self.tmpdir, capture_output=True, text=True)
            if result.returncode != 0:
                raise AssertionError(f'git apply {fname} failed: '
                                     f'{result.stderr}')
            with open(src_path) as f:
                return f.read()
        finally:
            with open(src_path, 'w') as f:
                f.write(original)

    def __exit__(self, *exc):
        shutil.rmtree(self.tmpdir, ignore_errors=True)


UPSTREAM_A = "int a(void) {\n    return 1;\n}\n"
UPSTREAM_B = "int b(void) {\n    return 2;\n}\n"


# ---------------------------------------------------------------------------
# Unit 3's three specified cases: (a) changed, (b) unchanged, (c) reverted
# ---------------------------------------------------------------------------

class TestBasicRefreshCases(unittest.TestCase):

    def test_a_changed_file_produces_a_patch(self):
        """(a) BUG THIS TEST EXISTS TO CATCH: a changed materialized
        file failing to produce any patch at all, or producing one
        that doesn't reproduce the edit on application."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            self.assertEqual(result['removed'], [])
            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])

            applied = p.apply_patch_and_get_result('010-foo.c.patch',
                                                    'foo.c')
            self.assertEqual(applied, UPSTREAM_A.replace('1', '999'))

    def test_b_unchanged_file_produces_no_patch(self):
        """(b) BUG THIS TEST EXISTS TO CATCH: refresh writing a patch
        for a materialized file that is byte-identical to upstream
        (should be a strict no-op, not an empty/degenerate patch
        file)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A)

            result = p.refresh()

            self.assertEqual(result['written'], [])
            self.assertEqual(result['removed'], [])
            self.assertEqual(p.list_patches(), [])

    def test_c_reverted_edit_deletes_stale_patch(self):
        """(c) BUG THIS TEST EXISTS TO CATCH: a patch generated by an
        earlier refresh call lingering on disk after the developer
        reverts the materialized copy back to upstream — it would keep
        applying a change the materialized file no longer represents.
        """
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-foo.c.patch'])

            # Revert: materialized now identical to upstream again.
            p.write_materialized('foo.c', UPSTREAM_A)
            second = p.refresh()

            self.assertEqual(second['written'], [])
            self.assertEqual(second['removed'], ['010-foo.c.patch'])
            self.assertEqual(p.list_patches(), [])


# ---------------------------------------------------------------------------
# Directory scanning
# ---------------------------------------------------------------------------

class TestDirectoryScanning(unittest.TestCase):

    def test_multiple_files_mixed_changed_and_unchanged(self):
        """Bug: scanning stops at the first file, or a changed file
        elsewhere in the tree is missed while an unchanged one wrongly
        produces a patch."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_upstream('bar.c', UPSTREAM_B)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_materialized('bar.c', UPSTREAM_B)  # unchanged

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])

    def test_nested_subdirectory_rel_path(self):
        """Bug: a materialized file in a subdirectory (mirroring e.g.
        src/c47/programming/manage.c) not discovered by the walk, or
        its rel path encoded/decoded incorrectly."""
        with _TempProject() as p:
            p.write_upstream('programming/manage.c', UPSTREAM_A)
            p.write_materialized('programming/manage.c',
                                 UPSTREAM_A.replace('1', '999'))

            result = p.refresh()

            self.assertEqual(result['written'],
                             ['010-programming__manage.c.patch'])
            ordinal, rel = decode_patch_filename(result['written'][0])
            self.assertEqual(rel, 'programming/manage.c')

    def test_patches_and_files_subdirs_excluded_from_scan(self):
        """Bug: refresh treating its own patches/ output directory, or
        the files/ new-file store, as materialized targets to diff —
        would try to diff a .patch file against a (nonexistent)
        upstream .patch file, or reprocess files/ content."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            os.makedirs(os.path.join(p.pkg_abs, 'files'), exist_ok=True)
            with open(os.path.join(p.pkg_abs, 'files', 'new_thing.c'),
                     'w') as f:
                f.write('int new_thing(void) { return 0; }\n')

            found = list_materialized_files(p.pkg_abs)
            self.assertEqual(found, ['foo.c'])

            result = p.refresh()
            self.assertEqual(result['written'], ['010-foo.c.patch'])
            # files/ content must be untouched
            self.assertTrue(os.path.isfile(
                os.path.join(p.pkg_abs, 'files', 'new_thing.c')))

    def test_stray_meson_build_excluded_from_scan(self):
        """Bug: a leftover package-root meson.build (pre-revision-2
        convention) treated as a materialized copy of the unrelated
        src/c47/meson.build build file."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_upstream('meson.build', "c47_src = files('foo.c')\n")
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            with open(os.path.join(p.pkg_abs, 'meson.build'), 'w') as f:
                f.write("pkg_override_sources = ['foo.c']\n")

            found = list_materialized_files(p.pkg_abs)
            self.assertEqual(found, ['foo.c'])


# ---------------------------------------------------------------------------
# New files (no upstream counterpart) — reported, not patched
# ---------------------------------------------------------------------------

class TestNewFileReporting(unittest.TestCase):

    def test_no_upstream_counterpart_reported_not_patched(self):
        """Bug: refresh silently ignoring a materialized file with no
        upstream counterpart (confusing — the developer gets no
        signal at all), or worse, generating a nonsensical patch for
        it (diffing against a file that doesn't exist)."""
        with _TempProject() as p:
            with open(os.path.join(p.pkg_abs, 'brand_new.c'), 'w') as f:
                f.write('int brand_new(void) { return 42; }\n')

            result = p.refresh()

            self.assertEqual(result['written'], [])
            self.assertEqual(result['new_files'], ['brand_new.c'])
            self.assertEqual(p.list_patches(), [])


# ---------------------------------------------------------------------------
# Ordinal handling: reuse on re-refresh, manual override preserved
# ---------------------------------------------------------------------------

class TestOrdinalHandling(unittest.TestCase):

    def test_ordinal_reused_on_second_refresh(self):
        """BUG THIS TEST EXISTS TO CATCH: re-running refresh after a
        further edit to the same file accumulating a second patch file
        (different ordinal) instead of rewriting the existing one in
        place."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '2'))
            first = p.refresh()
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '3'))
            second = p.refresh()

            self.assertEqual(first['written'], second['written'])
            self.assertEqual(p.list_patches(), first['written'])
            applied = p.apply_patch_and_get_result(second['written'][0],
                                                    'foo.c')
            self.assertEqual(applied, UPSTREAM_A.replace('1', '3'))

    def test_manually_renamed_ordinal_is_preserved(self):
        """Bug: refresh ignoring a developer's manual ordinal choice
        (renaming 010-foo.c.patch to 050-foo.c.patch to force explicit
        cross-package ordering) and reverting to its own default on the
        next refresh instead of reusing the existing (renamed) file."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '2'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-foo.c.patch'])

            os.rename(
                os.path.join(p.patches_dir(), '010-foo.c.patch'),
                os.path.join(p.patches_dir(), '050-foo.c.patch'))

            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '3'))
            second = p.refresh()

            self.assertEqual(second['written'], ['050-foo.c.patch'])
            self.assertEqual(p.list_patches(), ['050-foo.c.patch'])


# ---------------------------------------------------------------------------
# Multi-file patch validity (git apply --check + byte-exact reproduction)
# ---------------------------------------------------------------------------

class TestPatchValidity(unittest.TestCase):

    def test_two_changed_files_two_valid_independent_patches(self):
        """Bug: patches for different files interfering with each
        other (shared temp state, wrong rel embedded in one of them).
        """
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_upstream('bar.c', UPSTREAM_B)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_materialized('bar.c', UPSTREAM_B.replace('2', '888'))

            result = p.refresh()
            self.assertEqual(sorted(result['written']),
                             ['010-bar.c.patch', '010-foo.c.patch'])

            self.assertEqual(
                p.apply_patch_and_get_result('010-foo.c.patch', 'foo.c'),
                UPSTREAM_A.replace('1', '999'))
            self.assertEqual(
                p.apply_patch_and_get_result('010-bar.c.patch', 'bar.c'),
                UPSTREAM_B.replace('2', '888'))

    def test_patch_carries_git_headers_and_dual_signal(self):
        """Bug: patch missing the 'diff --git' header (git apply
        rejects headerless fragments), or the +++ target not matching
        the filename-encoded rel (would trip §2's dual-signal check at
        configure time)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))
            result = p.refresh()
            content = p.patch_content(result['written'][0])

            self.assertIn('diff --git a/src/c47/foo.c b/src/c47/foo.c',
                          content)
            self.assertRegex(content,
                             r'index [0-9a-f]{40}\.\.[0-9a-f]{40}')

            patch_path = os.path.join(p.patches_dir(), result['written'][0])
            self.assertEqual(parse_patch_target(patch_path), 'foo.c')

    def test_no_restriction_on_kind_of_change(self):
        """New Decision 1: a global-variable change, unlike under the
        (removed) function-boundary design, must NOT be rejected — it
        is just diff output like anything else."""
        upstream = "int global_var = 42;\n\nint f(void) { return 1; }\n"
        materialized = "int global_var = 999;\n\nint f(void) { return 1; }\n"
        with _TempProject() as p:
            p.write_upstream('foo.c', upstream)
            p.write_materialized('foo.c', materialized)

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            applied = p.apply_patch_and_get_result('010-foo.c.patch',
                                                    'foo.c')
            self.assertEqual(applied, materialized)


# ---------------------------------------------------------------------------
# Fatal cases: binary content, uncommitted upstream
# ---------------------------------------------------------------------------

class TestFatalCases(unittest.TestCase):

    def test_binary_file_rejected(self):
        """Bug: a binary materialized file silently producing a
        corrupt/unusable 'patch' instead of a clear, named error."""
        with _TempProject() as p:
            up_path = os.path.join(p.src_c47, 'blob.dat')
            with open(up_path, 'wb') as f:
                f.write(bytes(range(256)))
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'up'],
                           cwd=p.tmpdir, capture_output=True)
            mat_path = os.path.join(p.pkg_abs, 'blob.dat')
            with open(mat_path, 'wb') as f:
                f.write(bytes(range(255, -1, -1)))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()
            self.assertIn('binary', str(cm.exception).lower())

    def test_uncommitted_upstream_rejected(self):
        """BUG THIS TEST EXISTS TO CATCH: generating a patch whose
        pre-image blob is not a real git object (upstream file
        modified/created but not committed) — breaking the git-apply-3
        ancestry assumption from the very start. Mutation: remove the
        cat-file -e check in generate_patch()."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A, commit=False)
            p.write_materialized('foo.c', UPSTREAM_A.replace('1', '999'))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()
            self.assertIn('not a resolvable git object', str(cm.exception))


# ---------------------------------------------------------------------------
# Real-repo run against an actual src/c47/ file
# ---------------------------------------------------------------------------

class TestRealRepoRefresh(unittest.TestCase):
    """Exercises refresh against a real upstream file in this
    repository via a scratch package directory the test creates and
    removes itself — no synthetic package is left behind."""

    def _pick_real_target(self):
        """A small, git-clean src/c47/*.c file to use as a diff target."""
        src_c47 = os.path.join(_REPO_ROOT, 'src', 'c47')
        candidates = []
        for root, _, files in os.walk(src_c47):
            for fname in files:
                if not fname.endswith('.c'):
                    continue
                path = os.path.join(root, fname)
                rel = os.path.relpath(path, src_c47)
                status = subprocess.run(
                    ['git', 'status', '--porcelain', '--',
                     os.path.join('src', 'c47', rel)],
                    cwd=_REPO_ROOT, capture_output=True, text=True).stdout
                if status.strip():
                    continue
                candidates.append((os.path.getsize(path), rel, path))
        candidates.sort()
        return candidates[0] if candidates else None

    def test_refresh_on_real_upstream_file(self):
        """Bug: the pipeline working on synthetic fixtures but failing
        on a real upstream file (real line endings, real content)."""
        picked = self._pick_real_target()
        if picked is None:
            self.skipTest('no clean src/c47/*.c file found')
        _, rel, src_path = picked

        pkgdir = 'packages/_scratch_refresh_test'
        pkg_path = os.path.join(_REPO_ROOT, pkgdir)
        try:
            os.makedirs(os.path.join(pkg_path, os.path.dirname(rel) or '.'),
                       exist_ok=True)
            with open(src_path) as f:
                content = f.read()
            mat_path = os.path.join(pkg_path, rel)
            with open(mat_path, 'w') as f:
                f.write(content + '\n/* pkg_patch_refresh real-repo test */\n')

            result = refresh(pkgdir, _REPO_ROOT)
            self.assertEqual(len(result['written']), 1)

            patch_path = os.path.join(pkg_path, 'patches',
                                      result['written'][0])
            with open(patch_path) as f:
                patch_content = f.read()
            self.assertIn('pkg_patch_refresh real-repo test', patch_content)

            check = subprocess.run(['git', 'apply', '--check', patch_path],
                                   cwd=_REPO_ROOT, capture_output=True,
                                   text=True)
            self.assertEqual(check.returncode, 0,
                             f'git apply --check failed: {check.stderr}')

            scratch = tempfile.mkdtemp()
            try:
                dst = os.path.join(scratch, 'src', 'c47', rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy(src_path, dst)
                r = subprocess.run(['git', 'apply', patch_path],
                                   cwd=scratch, capture_output=True,
                                   text=True)
                self.assertEqual(r.returncode, 0, r.stderr)
                with open(dst) as f:
                    self.assertEqual(
                        f.read(),
                        content + '\n/* pkg_patch_refresh real-repo test */\n')
            finally:
                shutil.rmtree(scratch, ignore_errors=True)
        finally:
            shutil.rmtree(pkg_path, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
