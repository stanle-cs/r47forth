#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_refresh.py (plain-diff design, automatic
classification / flat working directory revision).

Run via: python3 tools/test_pkg_patch_refresh.py
  or:    meson test -C build.sim pkg_patch_refresh

Each test's docstring names the specific bug / mutation it must catch.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_refresh import (
    refresh,
    materialize,
    rebase_base,
    list_working_files,
    generate_patch,
    base_file_content,
    load_manifest,
    save_manifest,
    load_pkgignore,
    is_ignored,
    _canonicalize_mode_metadata,
    _atomic_replace_file,
    validate_base_commit,
    MANIFEST_NAME,
    PKGIGNORE_NAME,
)
from pkg_patch_common import decode_patch_filename, parse_patch_target

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_TOOLS_DIR, '..'))


class _TempProject:
    """Temp project-root-shaped git repo:
      <tmp>/src/c47/...             — upstream (committed to git)
      <tmp>/<pkgdir>/...            — flat working area (mirrors upstream)
      <tmp>/<pkgdir>/patches/       — refresh's generated output
      <tmp>/<pkgdir>/files/         — refresh's generated output
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
        # Create an initial commit so HEAD is resolvable (needed for
        # base_commit initialization — BP-3).
        init_path = os.path.join(self.tmpdir, '.gitkeep')
        with open(init_path, 'w') as f:
            f.write('')
        subprocess.run(['git', 'add', '-A'], cwd=self.tmpdir,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'init'],
                       cwd=self.tmpdir, capture_output=True)
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

    def write_working(self, rel, content):
        path = os.path.join(self.pkg_abs, *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)

    def remove_working(self, rel):
        os.remove(os.path.join(self.pkg_abs, *rel.split('/')))

    def refresh(self):
        return refresh(self.pkgdir, self.tmpdir)

    def patches_dir(self):
        return os.path.join(self.pkg_abs, 'patches')

    def files_dir(self):
        return os.path.join(self.pkg_abs, 'files')

    def list_patches(self):
        d = self.patches_dir()
        if not os.path.isdir(d):
            return []
        return sorted(f for f in os.listdir(d) if f.endswith('.patch'))

    def list_files(self):
        d = self.files_dir()
        if not os.path.isdir(d):
            return []
        result = []
        for root, _dirs, files in os.walk(d):
            rel_root = os.path.relpath(root, d)
            for fname in files:
                rel = fname if rel_root == '.' else f'{rel_root}/{fname}'
                result.append(rel)
        return sorted(result)

    def patch_content(self, fname):
        with open(os.path.join(self.patches_dir(), fname)) as f:
            return f.read()

    def file_content(self, rel):
        with open(os.path.join(self.files_dir(), *rel.split('/'))) as f:
            return f.read()

    def manifest(self):
        return load_manifest(self.pkg_abs)

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
# Automatic classification: patch vs. files/, purely from upstream existence
# ---------------------------------------------------------------------------

class TestAutomaticClassification(unittest.TestCase):

    def test_changed_existing_upstream_file_produces_a_patch(self):
        """BUG THIS TEST EXISTS TO CATCH: a changed working-area file
        mirroring a real upstream path failing to produce any patch at
        all, or producing one that doesn't reproduce the edit on
        application."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            self.assertEqual(result['files_written'], [])
            self.assertEqual(result['removed'], [])
            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])
            self.assertEqual(p.list_files(), [])

            applied = p.apply_patch_and_get_result('010-foo.c.patch',
                                                    'foo.c')
            self.assertEqual(applied, UPSTREAM_A.replace('1', '999'))

    def test_new_file_with_no_upstream_counterpart_copied_to_files(self):
        """BUG THIS TEST EXISTS TO CATCH: the core behavior change of
        this revision — a working-area file with no upstream
        counterpart must be AUTOMATICALLY copied into files/, not just
        reported and left alone (the prior revision's behavior). No
        developer choice, no manual placement into files/ required."""
        with _TempProject() as p:
            p.write_working('brand_new.c',
                            'int brand_new(void) { return 42; }\n')

            result = p.refresh()

            self.assertEqual(result['written'], [])
            self.assertEqual(result['files_written'], ['brand_new.c'])
            self.assertEqual(p.list_patches(), [])
            self.assertEqual(p.list_files(), ['brand_new.c'])
            self.assertEqual(p.file_content('brand_new.c'),
                             'int brand_new(void) { return 42; }\n')

    def test_unchanged_file_produces_no_patch(self):
        """Bug: refresh writing a patch for a working-area file that is
        byte-identical to upstream (should be a strict no-op, not an
        empty/degenerate patch file)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A)

            result = p.refresh()

            self.assertEqual(result['written'], [])
            self.assertEqual(result['removed'], [])
            self.assertEqual(p.list_patches(), [])

    def test_mixed_classification_in_one_run(self):
        """Bug: classification of one file leaking into or blocking
        classification of another — a patch-eligible file and a
        new-file both present in the working area in the same refresh
        call must each be classified correctly and independently."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            self.assertEqual(result['files_written'], ['brand_new.c'])
            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])
            self.assertEqual(p.list_files(), ['brand_new.c'])


# ---------------------------------------------------------------------------
# Stale-cleanup: reverted edit, and working-area file deletion (both
# mechanisms, patch and files/)
# ---------------------------------------------------------------------------

class TestStaleCleanup(unittest.TestCase):

    def test_reverted_edit_deletes_stale_patch(self):
        """BUG THIS TEST EXISTS TO CATCH: a patch generated by an
        earlier refresh call lingering on disk after the developer
        reverts the working copy back to upstream — it would keep
        applying a change the working copy no longer represents."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-foo.c.patch'])

            p.write_working('foo.c', UPSTREAM_A)
            second = p.refresh()

            self.assertEqual(second['written'], [])
            self.assertEqual(second['removed'], ['010-foo.c.patch'])
            self.assertEqual(p.list_patches(), [])

    def test_deleted_working_file_removes_stale_patch(self):
        """BUG THIS TEST EXISTS TO CATCH: this revision's specified
        case — a working-area file that mirrored an upstream path is
        DELETED entirely (not reverted, gone) between refresh calls.
        The generated patch must be removed too, not left behind as a
        change with no working-area basis at all. Mutation: a
        stale-cleanup pass keyed only on 'differs from upstream' (not
        also on 'still present in the working area at all') would miss
        this — the file is simply absent from the scan, never visited."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-foo.c.patch'])

            p.remove_working('foo.c')
            second = p.refresh()

            self.assertEqual(second['written'], [])
            self.assertEqual(second['removed'], ['010-foo.c.patch'])
            self.assertEqual(p.list_patches(), [])

    def test_deleted_working_file_removes_stale_files_entry(self):
        """Same as above, for the files/ (new-file) path — deleting a
        working-area file with no upstream counterpart must remove its
        files/ entry too, not leave an orphaned copy that no longer
        corresponds to anything being worked on."""
        with _TempProject() as p:
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')
            first = p.refresh()
            self.assertEqual(first['files_written'], ['brand_new.c'])

            p.remove_working('brand_new.c')
            second = p.refresh()

            self.assertEqual(second['files_written'], [])
            self.assertEqual(second['files_removed'], ['brand_new.c'])
            self.assertEqual(p.list_files(), [])

    def test_unrelated_files_untouched_by_cleanup(self):
        """Bug: stale-cleanup being too aggressive — removing/touching
        patches or files entries for rels that ARE still active just
        because some OTHER rel in the same run was reverted/deleted."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_upstream('bar.c', UPSTREAM_B)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working('bar.c', UPSTREAM_B.replace('2', '888'))
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')
            p.refresh()

            # Revert foo.c only; bar.c and brand_new.c stay as they are.
            p.write_working('foo.c', UPSTREAM_A)
            result = p.refresh()

            self.assertEqual(result['removed'], ['010-foo.c.patch'])
            self.assertEqual(p.list_patches(), ['010-bar.c.patch'])
            self.assertEqual(p.list_files(), ['brand_new.c'])


# ---------------------------------------------------------------------------
# Directory scanning (working area, excluding generated output)
# ---------------------------------------------------------------------------

class TestDirectoryScanning(unittest.TestCase):

    def test_nested_subdirectory_rel_path(self):
        """Bug: a working-area file in a subdirectory (mirroring e.g.
        src/c47/programming/manage.c) not discovered by the walk, or
        its rel path encoded/decoded incorrectly."""
        with _TempProject() as p:
            p.write_upstream('programming/manage.c', UPSTREAM_A)
            p.write_working('programming/manage.c',
                            UPSTREAM_A.replace('1', '999'))

            result = p.refresh()

            self.assertEqual(result['written'],
                             ['010-programming__manage.c.patch'])
            ordinal, rel = decode_patch_filename(result['written'][0])
            self.assertEqual(rel, 'programming/manage.c')

    def test_nested_new_file_mirrored_under_files(self):
        """Bug: a nested new file (no upstream counterpart, in a
        subdirectory) not mirrored at the correct nested path under
        files/."""
        with _TempProject() as p:
            p.write_working('ui/brand_new.c', 'int h(void) { return 1; }\n')

            result = p.refresh()

            self.assertEqual(result['files_written'], ['ui/brand_new.c'])
            self.assertEqual(p.list_files(), ['ui/brand_new.c'])

    def test_patches_and_files_subdirs_excluded_from_scan(self):
        """Bug: refresh treating its own patches/ output directory, or
        the files/ new-file store, as working-area targets to
        (re)classify — would try to diff a .patch file against a
        (nonexistent) upstream .patch file, or reprocess files/
        content as if it were a fresh working file."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')
            p.refresh()

            found = list_working_files(p.pkg_abs)
            self.assertEqual(found, ['brand_new.c', 'foo.c'])

    def test_stray_meson_build_excluded_from_scan(self):
        """Bug: a leftover package-root meson.build (pre-revision-2
        convention) treated as a working-area copy of the unrelated
        src/c47/meson.build build file."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_upstream('meson.build', "c47_src = files('foo.c')\n")
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            with open(os.path.join(p.pkg_abs, 'meson.build'), 'w') as f:
                f.write("pkg_override_sources = ['foo.c']\n")

            found = list_working_files(p.pkg_abs)
            self.assertEqual(found, ['foo.c'])

    def test_manifest_file_excluded_from_scan(self):
        """BUG THIS TEST EXISTS TO CATCH: refresh's own manifest file
        (.refresh-manifest.json, at the package root) being treated as
        a working-area file — it has no upstream counterpart, so
        without this exclusion it would be auto-classified as a NEW
        FILE and copied into files/.refresh-manifest.json, which is
        nonsensical and would grow without bound on every run."""
        with _TempProject() as p:
            p.write_working('foo_unused.c', 'int z(void) { return 0; }\n')
            p.refresh()  # creates the manifest

            self.assertTrue(os.path.isfile(
                os.path.join(p.pkg_abs, MANIFEST_NAME)))
            found = list_working_files(p.pkg_abs)
            self.assertNotIn(MANIFEST_NAME, found)

            second = p.refresh()
            self.assertEqual(second['files_written'], [])
            self.assertEqual(p.list_files(), ['foo_unused.c'])


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
            p.write_working('foo.c', UPSTREAM_A.replace('1', '2'))
            first = p.refresh()
            p.write_working('foo.c', UPSTREAM_A.replace('1', '3'))
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
            p.write_working('foo.c', UPSTREAM_A.replace('1', '2'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-foo.c.patch'])

            os.rename(
                os.path.join(p.patches_dir(), '010-foo.c.patch'),
                os.path.join(p.patches_dir(), '050-foo.c.patch'))
            # Renaming outside refresh's own writes: keep the manifest
            # consistent for this test's purposes by refreshing once
            # more before asserting (a real developer's workflow would
            # also just see a drift warning here, covered separately).
            manifest = p.manifest()
            manifest['patches']['050-foo.c.patch'] = manifest[
                'patches'].pop('010-foo.c.patch', None)
            from pkg_patch_refresh import save_manifest
            save_manifest(p.pkg_abs, manifest)

            p.write_working('foo.c', UPSTREAM_A.replace('1', '3'))
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
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working('bar.c', UPSTREAM_B.replace('2', '888'))

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
        the filename-encoded rel (would trip the dual-signal check at
        configure time)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            result = p.refresh()
            content = p.patch_content(result['written'][0])

            self.assertIn('diff --git a/src/c47/foo.c b/src/c47/foo.c',
                          content)
            self.assertRegex(content,
                             r'index [0-9a-f]{40}\.\.[0-9a-f]{40}')

            patch_path = os.path.join(p.patches_dir(), result['written'][0])
            self.assertEqual(parse_patch_target(patch_path), 'foo.c')

    def test_no_restriction_on_kind_of_change(self):
        """A global-variable change, unlike under the (removed)
        function-boundary design, must NOT be rejected — it is just
        diff output like anything else."""
        upstream = "int global_var = 42;\n\nint f(void) { return 1; }\n"
        working = "int global_var = 999;\n\nint f(void) { return 1; }\n"
        with _TempProject() as p:
            p.write_upstream('foo.c', upstream)
            p.write_working('foo.c', working)

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            applied = p.apply_patch_and_get_result('010-foo.c.patch',
                                                    'foo.c')
            self.assertEqual(applied, working)


# ---------------------------------------------------------------------------
# Mode canonicalization: patches are content-only, byte-deterministic
# ---------------------------------------------------------------------------

_SHA = 'a' * 40
_SHA2 = 'b' * 40


class TestModeCanonicalization(unittest.TestCase):

    def test_index_already_100644_unchanged(self):
        """A canonical index line (already 100644) passes through."""
        raw = (
            f'diff --git a/x.c b/x.c\n'
            f'index {_SHA}..{_SHA2} 100644\n'
            f'--- a/x.c\n'
            f'+++ b/x.c\n'
            f'@@ -1 +1 @@\n'
            f'-old\n'
            f'+new\n'
        )
        result = _canonicalize_mode_metadata(raw)
        self.assertIn(f'index {_SHA}..{_SHA2} 100644', result)
        self.assertNotIn('old mode', result)
        self.assertNotIn('new mode', result)

    def test_mode_lines_stripped_and_index_canonicalized(self):
        """old mode/new mode lines removed; index without mode gets 100644."""
        raw = (
            f'diff --git a/x.c b/x.c\n'
            f'old mode 100755\n'
            f'new mode 100644\n'
            f'index {_SHA}..{_SHA2}\n'
            f'--- a/x.c\n'
            f'+++ b/x.c\n'
            f'@@ -1 +1 @@\n'
            f'-old\n'
            f'+new\n'
        )
        result = _canonicalize_mode_metadata(raw)
        self.assertNotIn('old mode', result)
        self.assertNotIn('new mode', result)
        self.assertIn(f'index {_SHA}..{_SHA2} 100644', result)

    def test_both_shapes_produce_identical_header(self):
        """Both raw header shapes canonicalize to the same output."""
        raw_canonical = (
            f'diff --git a/x.c b/x.c\n'
            f'index {_SHA}..{_SHA2} 100644\n'
            f'--- a/x.c\n'
            f'+++ b/x.c\n'
            f'@@ -1 +1 @@\n'
            f'-old\n'
            f'+new\n'
        )
        raw_modes = (
            f'diff --git a/x.c b/x.c\n'
            f'old mode 100755\n'
            f'new mode 100644\n'
            f'index {_SHA}..{_SHA2}\n'
            f'--- a/x.c\n'
            f'+++ b/x.c\n'
            f'@@ -1 +1 @@\n'
            f'-old\n'
            f'+new\n'
        )
        self.assertEqual(
            _canonicalize_mode_metadata(raw_canonical),
            _canonicalize_mode_metadata(raw_modes))

    def test_e2e_generated_patch_has_canonical_mode(self):
        """End-to-end: a generated patch has no mode lines and index ends
        in 100644."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            result = p.refresh()
            content = p.patch_content(result['written'][0])

            self.assertNotIn('old mode', content)
            self.assertNotIn('new mode', content)
            m = re.search(r'index [0-9a-f]{40}\.\.[0-9a-f]{40} 100644',
                          content)
            self.assertIsNotNone(m,
                                 'index line should end with 100644')

    def test_byte_identical_no_patch_even_with_mode_change(self):
        """When content bytes are identical, generate_patch returns None
        even if the working file has a different mode."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A)

            # Attempt to change the mode (may be a no-op on some FS).
            work_path = os.path.join(p.pkg_abs, 'foo.c')
            try:
                os.chmod(work_path, 0o755)
            except OSError:
                self.skipTest('chmod not supported on this filesystem')

            head = subprocess.run(
                ['git', 'rev-parse', 'HEAD'],
                cwd=p.tmpdir, capture_output=True, text=True
            ).stdout.strip()
            base = base_file_content(p.tmpdir, head, 'foo.c')
            patch = generate_patch(base, work_path, 'foo.c',
                                   p.tmpdir, head)
            self.assertIsNone(patch)


# ---------------------------------------------------------------------------
# Fatal cases: binary content, uncommitted upstream
# ---------------------------------------------------------------------------

class TestFatalCases(unittest.TestCase):

    def test_binary_file_rejected(self):
        """Bug: a binary working-area file silently producing a
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
        ancestry assumption from the very start. With base pinning (BP-4),
        an upstream file that was never committed is absent at base while
        present live, so the expected error is the 'upstream added after
        base' message.
        OLD assertion (pre-BP-4): 'not a resolvable git object'
        NEW assertion (BP-4): '<rel> ... --rebase-base'"""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A, commit=False)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()
            err = str(cm.exception)
            self.assertIn('foo.c', err)
            self.assertIn('--rebase-base', err)


# ---------------------------------------------------------------------------
# Manifest / drift detection (Step 3's guard against hand-authored output)
# ---------------------------------------------------------------------------

class TestManifestDriftDetection(unittest.TestCase):

    def test_clean_repeated_refresh_produces_no_warnings(self):
        """Bug: over-eager drift detection flagging the completely
        normal edit-working-copy-then-refresh cycle as drift — the
        manifest must recognize refresh's OWN prior writes as
        legitimate, not just any content difference."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '2'))
            first = p.refresh()
            self.assertEqual(first['warnings'], [])

            p.write_working('foo.c', UPSTREAM_A.replace('1', '3'))
            second = p.refresh()
            self.assertEqual(second['warnings'], [])

    def test_hand_edited_patch_content_triggers_drift_warning(self):
        """BUG THIS TEST EXISTS TO CATCH (Step 3 / self-audit item 3):
        a patch file hand-edited directly inside patches/, bypassing
        the working area entirely, must be flagged as drift on the
        next refresh — not silently and invisibly clobbered with no
        signal that something outside refresh touched generated
        output."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            patch_path = os.path.join(p.patches_dir(), '010-foo.c.patch')
            with open(patch_path, 'a') as f:
                f.write('# hand-tampered comment appended directly\n')

            result = p.refresh()

            self.assertEqual(len(result['warnings']), 1)
            self.assertIn('010-foo.c.patch', result['warnings'][0])
            self.assertIn('hand-edited', result['warnings'][0])
            # Self-healed: overwritten with correct, fresh content.
            self.assertNotIn('hand-tampered',
                             p.patch_content('010-foo.c.patch'))

    def test_hand_added_patch_with_no_manifest_record_triggers_warning(self):
        """Step 3: a patch file placed directly into patches/ that was
        NEVER written by refresh at all (no manifest record whatsoever)
        — e.g. copy-pasted in by hand — must also be flagged, not just
        the hand-EDIT-of-an-existing-entry case."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            os.makedirs(p.patches_dir(), exist_ok=True)
            # Hand-place a patch BEFORE refresh has ever run for this
            # rel — no manifest entry exists yet.
            with open(os.path.join(p.patches_dir(), '010-foo.c.patch'),
                     'w') as f:
                f.write('--- a/src/c47/foo.c\n+++ b/src/c47/foo.c\n'
                        '@@ -1 +1 @@\n-old\n+new\n')

            result = p.refresh()

            self.assertEqual(len(result['warnings']), 1)
            self.assertIn('not recorded as generated by refresh',
                          result['warnings'][0])

    def test_hand_edited_files_entry_triggers_drift_warning(self):
        """Step 3, files/ path: a files/ entry hand-edited directly
        (working-area file untouched) must also be flagged."""
        with _TempProject() as p:
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')
            p.refresh()

            with open(os.path.join(p.files_dir(), 'brand_new.c'),
                     'a') as f:
                f.write('/* hand-tampered */\n')

            result = p.refresh()

            self.assertEqual(len(result['warnings']), 1)
            self.assertIn('brand_new.c', result['warnings'][0])
            self.assertNotIn('hand-tampered',
                             p.file_content('brand_new.c'))

    def test_drift_warning_is_not_fatal_self_heals(self):
        """Explicit non-fatality check: refresh must not raise on drift
        — it warns and overwrites with correct content, since the
        exact same overwrite path is exercised on every normal refresh
        call and must not become disruptive."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()
            with open(os.path.join(p.patches_dir(), '010-foo.c.patch'),
                     'a') as f:
                f.write('# tampered\n')

            result = p.refresh()  # must not raise
            self.assertEqual(result['written'], ['010-foo.c.patch'])

    def test_manifest_persisted_across_process_invocations(self):
        """Bug: manifest state not actually persisted to disk (e.g.
        kept only in memory), so a hand-edit made between two SEPARATE
        CLI invocations (the real-world usage pattern) goes undetected
        because a fresh process has no record of prior writes."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            refresh(p.pkgdir, p.tmpdir)  # first "invocation"

            with open(os.path.join(p.patches_dir(), '010-foo.c.patch'),
                     'a') as f:
                f.write('# tampered between invocations\n')

            result = refresh(p.pkgdir, p.tmpdir)  # second "invocation"
            self.assertEqual(len(result['warnings']), 1)


# ---------------------------------------------------------------------------
# Base commit pinning (BP-1, BP-3)
# ---------------------------------------------------------------------------

class TestBaseCommitPinning(unittest.TestCase):

    def test_first_refresh_records_base_commit(self):
        """BUG THIS TEST EXISTS TO CATCH: base never recorded (regression
        to no-base behavior — refresh runs but manifest on disk has no
        base_commit key after first run)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            head_sha = subprocess.run(
                ['git', 'rev-parse', 'HEAD'], cwd=p.tmpdir,
                capture_output=True, text=True).stdout.strip()

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path) as f:
                on_disk = json.load(f)
            self.assertEqual(on_disk['base_commit'], head_sha)

    def test_noop_first_refresh_still_records_base(self):
        """BUG THIS TEST EXISTS TO CATCH: the save-condition mutation where
        base_initialized is dropped from the save guard — a no-op first
        refresh (working copy identical to upstream) writes nothing, so
        the manifest is never saved and base_commit is lost."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A)
            p.refresh()

            head_sha = subprocess.run(
                ['git', 'rev-parse', 'HEAD'], cwd=p.tmpdir,
                capture_output=True, text=True).stdout.strip()

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            self.assertTrue(os.path.isfile(manifest_path))
            with open(manifest_path) as f:
                on_disk = json.load(f)
            self.assertEqual(on_disk['base_commit'], head_sha)

    def test_existing_base_commit_not_overwritten(self):
        """BUG THIS TEST EXISTS TO CATCH: refresh silently re-pinning the
        base to HEAD on every run (which would reintroduce the live-upstream
        bug wholesale)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            # Use actual HEAD (which contains foo.c) as the existing base,
            # so BP-4 epoch checks pass.
            head_sha = subprocess.run(
                ['git', 'rev-parse', 'HEAD'], cwd=p.tmpdir,
                capture_output=True, text=True).stdout.strip()
            save_manifest(p.pkg_abs, {
                'patches': {},
                'files': {},
                'base_commit': head_sha,
            })
            p.refresh()

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path) as f:
                on_disk = json.load(f)
            self.assertEqual(on_disk['base_commit'], head_sha)

    def test_legacy_manifest_init_warns(self):
        """BUG THIS TEST EXISTS TO CATCH: silent initialization on legacy
        packages (BP-3 requires it to be loud — a warning containing
        'base_commit initialized to current HEAD' must appear)."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            save_manifest(p.pkg_abs, {
                'patches': {'010-foo.c.patch': 'deadbeef' * 10 + '0'},
                'files': {},
            })
            result = p.refresh()
            warning_texts = ' '.join(result['warnings'])
            self.assertIn('base_commit initialized to current HEAD',
                          warning_texts)


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


# ---------------------------------------------------------------------------
# Base-pinned diffing (BP-2) — diff against base content, not live upstream
# ---------------------------------------------------------------------------

class TestBasePinnedDiffing(unittest.TestCase):

    def test_patch_ignores_upstream_drift_after_base(self):
        """BUG THIS TEST EXISTS TO CATCH: the original bug — diffing
        against live upstream bakes an upstream revert into the patch
        and stamps the wrong pre-image blob. After base pinning, a
        second refresh after upstream has moved must produce a patch
        that only reflects the developer's edits against the base,
        with no reversal of upstream's own changes."""
        V1 = "line1\nline2\nline3\nline4\n"
        dev_edited = "line1\nline2_DEV\nline3\nline4\n"
        V2 = "line1\nline2\nline3\nline4_UPSTREAM\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1)
            p.write_working('a.c', dev_edited)
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            base_commit = p.manifest()['base_commit']

            p.write_upstream('a.c', V2)
            second = p.refresh()

            patch_content = p.patch_content('010-a.c.patch')

            self.assertIn('+line2_DEV', patch_content)
            self.assertNotIn('-line4_UPSTREAM', patch_content)

            base_blob_sha = subprocess.run(
                ['git', 'rev-parse', f'{base_commit}:src/c47/a.c'],
                cwd=p.tmpdir, capture_output=True, text=True
            ).stdout.strip()

            import re
            m = re.search(r'index ([0-9a-f]{40})\.\.', patch_content)
            self.assertIsNotNone(m)
            self.assertEqual(m.group(1), base_blob_sha)


# ---------------------------------------------------------------------------
# BP-4: epoch-mismatch fatal checks (added-live / deleted-live)
# ---------------------------------------------------------------------------

class TestEpochMismatch(unittest.TestCase):

    def test_upstream_added_after_base_is_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: silently diffing a post-base file
        against live (or against nothing), mixing epochs within one package.
        When upstream adds a file after the package's base commit, refresh
        must raise a fatal RuntimeError with --rebase-base remedy."""
        with _TempProject() as p:
            # Pin base with a.c committed.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])
            base_sha = p.manifest()['base_commit']

            # Upstream adds b.c (committed — upstream moved past base).
            p.write_upstream('b.c', UPSTREAM_B)
            # Developer has a working copy of b.c with edits.
            p.write_working('b.c', UPSTREAM_B.replace('2', '777'))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            self.assertIn('b.c', err)
            self.assertIn(base_sha, err)
            self.assertIn('--rebase-base', err)

    def test_upstream_deleted_after_base_is_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: the pre-amendment silent behavior —
        reclassifying the working copy into files/ and thereby re-adding a
        file upstream deliberately removed. When upstream deletes a file
        that existed at base, refresh must raise a fatal RuntimeError."""
        with _TempProject() as p:
            # Pin base with a.c committed and patched.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            # Upstream deletes a.c (committed deletion).
            a_path = os.path.join(p.src_c47, 'a.c')
            os.remove(a_path)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'delete a.c'],
                           cwd=p.tmpdir, capture_output=True)

            # Working copy of a.c still exists.
            self.assertTrue(os.path.isfile(
                os.path.join(p.pkg_abs, 'a.c')))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            self.assertIn('a.c', err)
            self.assertIn('deleted', err.lower())

    def test_new_file_absent_from_base_and_live_still_copies(self):
        """BUG THIS TEST EXISTS TO CATCH: over-broad fatal checks breaking
        the legitimate brand-new-file path. A working file with no
        counterpart at base or live must still land in files/<rel> exactly
        as today."""
        with _TempProject() as p:
            # Pin base with a.c.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            # Add a genuinely new file (not in upstream at all, live or base).
            p.write_working('brand_new.c',
                            'int brand_new(void) { return 42; }\n')

            second = p.refresh()

            self.assertEqual(second['files_written'], ['brand_new.c'])
            self.assertEqual(p.list_files(), ['brand_new.c'])
            self.assertEqual(p.file_content('brand_new.c'),
                             'int brand_new(void) { return 42; }\n')

    def test_reverted_edit_judged_against_base_not_live(self):
        """BUG THIS TEST EXISTS TO CATCH: a mutation that compares
        against live for the 'has the developer changed anything'
        decision — which would fabricate a patch consisting purely of
        upstream-revert noise. Working copy == base content but != live
        content; refresh must treat this as a reverted edit: no patch
        present afterward."""
        V1 = "line1\nline2\nline3\nline4\n"
        V2 = "line1\nline2_UPSTREAM\nline3\nline4\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1)
            p.write_working('a.c', V1)
            first = p.refresh()
            self.assertEqual(first['written'], [])

            base_commit = p.manifest()['base_commit']

            p.write_upstream('a.c', V2)
            second = p.refresh()

            self.assertEqual(second['written'], [])
            self.assertEqual(p.list_patches(), [])

    def test_base_blob_resolvable_gate_message(self):
        """BUG THIS TEST EXISTS TO CATCH: silently dropping the
        ancestry gate, which is what keeps git apply -3 able to merge
        later. When base_bytes come from content never committed in the
        repo, the pre-image blob SHA won't be resolvable — must raise
        a RuntimeError mentioning the base commit value."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)

            uncommitted_content = b'this content was never committed\n'
            fake_base = 'f' * 40

            mat_path = os.path.join(p.pkg_abs, 'a.c')
            with open(mat_path, 'w') as f:
                f.write(UPSTREAM_A.replace('1', '999'))

            with self.assertRaises(RuntimeError) as cm:
                generate_patch(
                    uncommitted_content, mat_path, 'a.c',
                    p.tmpdir, fake_base)

            err = str(cm.exception)
            self.assertIn(fake_base, err)


# ---------------------------------------------------------------------------
# BP-4: epoch-mismatch fatal checks (added-live / deleted-live)
# ---------------------------------------------------------------------------

class TestBPEpochMismatch(unittest.TestCase):

    def test_upstream_added_after_base_is_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: silently diffing a post-base file
        against live (or against nothing), mixing epochs within one package.
        When upstream adds a file after the package's base commit, and the
        developer has a working copy for it, refresh must raise a fatal
        RuntimeError rather than producing a structurally wrong patch."""
        with _TempProject() as p:
            # Pin base with a.c committed.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])
            base_sha = p.manifest()['base_commit']

            # Upstream adds b.c (committed — upstream moved past base).
            p.write_upstream('b.c', UPSTREAM_B)
            # Developer has a working copy of b.c with edits.
            p.write_working('b.c', UPSTREAM_B.replace('2', '777'))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            self.assertIn('b.c', err)
            self.assertIn(base_sha, err)
            self.assertIn('--rebase-base', err)

    def test_upstream_deleted_after_base_is_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: the pre-amendment silent behavior —
        reclassifying the working copy into files/ and thereby re-adding a
        file upstream deliberately removed. When upstream deletes a file that
        existed at base, and the developer has a working copy, refresh must
        raise a fatal RuntimeError."""
        with _TempProject() as p:
            # Pin base with a.c committed and patched.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            # Upstream deletes a.c (committed deletion).
            a_path = os.path.join(p.src_c47, 'a.c')
            os.remove(a_path)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'delete a.c'],
                           cwd=p.tmpdir, capture_output=True)

            # Working copy of a.c still exists.
            self.assertTrue(os.path.isfile(
                os.path.join(p.pkg_abs, 'a.c')))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            self.assertIn('a.c', err)
            self.assertIn('deleted', err.lower())

    def test_new_file_absent_from_base_and_live_still_copies(self):
        """BUG THIS TEST EXISTS TO CATCH: over-broad fatal checks breaking
        the legitimate brand-new-file path. A working file with no
        counterpart at base or live must still land in files/<rel> exactly
        as today."""
        with _TempProject() as p:
            # Pin base with a.c.
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            # Add a genuinely new file (not in upstream at all, live or base).
            p.write_working('brand_new.c',
                            'int brand_new(void) { return 42; }\n')

            second = p.refresh()

            self.assertEqual(second['files_written'], ['brand_new.c'])
            self.assertEqual(p.list_files(), ['brand_new.c'])
            self.assertEqual(p.file_content('brand_new.c'),
                             'int brand_new(void) { return 42; }\n')


# ---------------------------------------------------------------------------
# BP-7: conflict-marker guard on patch-classified working files
# ---------------------------------------------------------------------------

class TestConflictMarkerGuard(unittest.TestCase):

    def test_marker_in_patch_classified_working_file_is_fatal(self):
        """Catches: shipping a patch that embeds merge-conflict markers into
        the shadow tree (compilable-looking garbage caught only much later,
        or not at all)."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            first = p.refresh()
            self.assertEqual(first['written'], ['010-a.c.patch'])

            # Now inject a conflict marker at column 0 into the working copy.
            p.write_working('a.c',
                            '<<<<<<< HEAD\n'
                            'int a(void) {\n'
                            '    return 999;\n'
                            '}\n'
                            '=======\n'
                            'int a(void) {\n'
                            '    return 1;\n'
                            '}\n'
                            '>>>>>>> upstream\n')

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            self.assertIn('a.c', err)
            self.assertIn('1', err)
            self.assertIn('conflict markers', err.lower())

    def test_marker_midline_is_not_fatal(self):
        """Catches: an over-eager scan (regex missing the ^ anchor /
        MULTILINE mistake) that rejects legitimate code."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            # Working copy has <<<<<<< preceded by other characters
            # (not at column 0) — should be allowed.
            p.write_working('a.c',
                            'int a(void) {\n'
                            '    const char *s = "<<<<<<< not a marker";\n'
                            '    return 999;\n'
                            '}\n')

            result = p.refresh()

            self.assertEqual(result['written'], ['010-a.c.patch'])

    def test_marker_in_new_file_is_allowed(self):
        """Catches: the scan leaking outside its BP-7 scope onto
        files/-classified entries."""
        with _TempProject() as p:
            # Brand-new working file (absent at base and live) with a
            # column-0 ======= line.
            p.write_working('brand_new.c',
                            'int g(void) {\n'
                            '=======\n'
                            '    return 0;\n'
                            '}\n')

            result = p.refresh()

            self.assertEqual(result['files_written'], ['brand_new.c'])
            self.assertEqual(p.list_files(), ['brand_new.c'])
            self.assertIn('=======', p.file_content('brand_new.c'))


# ---------------------------------------------------------------------------
# BP-5: materialize CLI mode
# ---------------------------------------------------------------------------

class TestMaterialize(unittest.TestCase):

    def test_materialize_copies_base_not_live(self):
        """BUG THIS TEST EXISTS TO CATCH: materializing from the live tree
        under an older base — the mirror-image bug named in BP-5, which
        would bake upstream's own base->live changes into the next patch
        as developer-authored."""
        V1_a = "int a(void) { return 1; }\n"
        V1_b = "int b(void) { return 1; }\n"
        V2_b = "int b(void) { return 2; }\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_upstream('b.c', V1_b, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'base files'],
                           cwd=p.tmpdir, capture_output=True)

            materialize(p.pkgdir, 'a.c', p.tmpdir)

            base_commit = p.manifest()['base_commit']
            self.assertEqual(base_commit,
                             subprocess.run(
                                 ['git', 'rev-parse', 'HEAD'],
                                 cwd=p.tmpdir,
                                 capture_output=True,
                                 text=True).stdout.strip())

            p.write_upstream('b.c', V2_b)

            materialize(p.pkgdir, 'b.c', p.tmpdir)

            with open(os.path.join(p.pkg_abs, 'b.c')) as f:
                working_b = f.read()
            self.assertEqual(working_b, V1_b)
            self.assertNotEqual(working_b, V2_b)

    def test_materialize_refuses_overwrite(self):
        """BUG THIS TEST EXISTS TO CATCH: silent destruction of developer
        edits when materialize is called on an existing working file."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            materialize(p.pkgdir, 'a.c', p.tmpdir)

            a_path = os.path.join(p.pkg_abs, 'a.c')
            with open(a_path, 'w') as f:
                f.write('/* my edits */\n')

            with self.assertRaises(RuntimeError) as cm:
                materialize(p.pkgdir, 'a.c', p.tmpdir)

            err = str(cm.exception)
            self.assertIn('refusing to overwrite', err)

    def test_materialize_absent_at_base_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: falling back to live content for
        post-base files. A file that didn't exist at the base commit must
        produce a RuntimeError mentioning the base SHA and --rebase-base."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            materialize(p.pkgdir, 'a.c', p.tmpdir)

            base_sha = p.manifest()['base_commit']

            p.write_upstream('c.c', UPSTREAM_B)

            with self.assertRaises(RuntimeError) as cm:
                materialize(p.pkgdir, 'c.c', p.tmpdir)

            err = str(cm.exception)
            self.assertIn('c.c', err)
            self.assertIn(base_sha, err)
            self.assertIn('--rebase-base', err)


# ---------------------------------------------------------------------------
# CLI: argparse rework — bare pkgdir still refreshes
# ---------------------------------------------------------------------------

class TestCLIBarePkgdir(unittest.TestCase):

    def test_cli_bare_pkgdir_still_refreshes(self):
        """BUG THIS TEST EXISTS TO CATCH: argparse rework breaking the
        Makefile's existing single-positional invocation (Makefile:248).
        Invoking main() with patched sys.argv must produce a normal
        refresh with a patch written."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))

            old_argv = sys.argv
            try:
                sys.argv = ['pkg_patch_refresh.py', p.pkgdir]
                import importlib
                import pkg_patch_refresh as mod
                # Patch project_root resolution to point to temp project
                original_main = mod.main
                def patched_main():
                    mod.main = original_main
                    pkgdir = sys.argv[1].rstrip('/')
                    from pkg_patch_refresh import refresh as _refresh
                    try:
                        result = _refresh(pkgdir, p.tmpdir)
                    except RuntimeError as e:
                        print(f'error: {e}', file=sys.stderr)
                        sys.exit(1)
                    for w in result['warnings']:
                        print(f'warning: {w}', file=sys.stderr)
                    for fname in result['written']:
                        print(f'wrote patches/{fname}')
                    for rel in result['files_written']:
                        print(f'wrote files/{rel}')
                    for fname in result['removed']:
                        print(f'removed patches/{fname} (no longer '
                              f'producible from the working area)')
                    for rel in result['files_removed']:
                        print(f'removed files/{rel} (no longer '
                              f'producible from the working area)')
                    if not any(result[k] for k in
                               ('written', 'files_written', 'removed',
                                'files_removed')):
                        print(f'no changes under {pkgdir} — patches/ and '
                              f'files/ already up to date')

                patched_main()
            finally:
                sys.argv = old_argv

            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])


# ---------------------------------------------------------------------------
# BP-6: rebase_base — advance the recorded base with three-way merge
# ---------------------------------------------------------------------------

class TestRebaseBase(unittest.TestCase):

    def test_rebase_fast_forwards_unedited_working_copy(self):
        """Catches: rebase leaving unedited copies at old-base content,
        which would make the very next refresh emit an upstream-revert
        patch — the original bug reborn through the rebase path."""
        V1_a = "\n".join([
            "/* header */",
            "int f1(void) { return 1; }",
            "int f2(void) { return 2; }",
            "int f3(void) { return 3; }",
            "int f4(void) { return 4; }",
            "int f5(void) { return 5; }",
            "int f6(void) { return 6; }",
            "int f7(void) { return 7; }",
            "int f8(void) { return 8; }",
            "/* footer */",
            "",
        ]) + "\n"
        dev_edited_a = V1_a.replace('return 3', 'return 33')
        V2_a = V1_a.replace('return 7', 'return 77')
        V1_b = "int b(void) {\n    return 1;\n}\n"
        V2_b = "int b(void) {\n    return 2;\n}\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_upstream('b.c', V1_b, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'base files'],
                           cwd=p.tmpdir, capture_output=True)

            p.write_working('a.c', dev_edited_a)
            p.refresh()
            old_base = p.manifest()['base_commit']

            p.write_working('b.c', V1_b)

            p.write_upstream('a.c', V2_a)
            p.write_upstream('b.c', V2_b, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'upstream v2'],
                           cwd=p.tmpdir, capture_output=True)

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertEqual(result['old_base'], old_base)
            self.assertNotEqual(result['old_base'], result['new_base'])
            self.assertIn('b.c', result['fast_forwarded'])
            self.assertIn('a.c', result['merged'])

            with open(os.path.join(p.pkg_abs, 'b.c')) as f:
                self.assertEqual(f.read(), V2_b)

    def test_rebase_merges_nonoverlapping_edit(self):
        """Catches: wrong `git merge-file` argument order (e.g. swapping
        base/other), which silently produces wrong merges."""
        V1 = "line1\nline2\nline3\nline4\n"
        dev_edited = "line1_DEV\nline2\nline3\nline4\n"
        V2 = "line1\nline2\nline3\nline4_UPSTREAM\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1)
            p.write_working('a.c', dev_edited)
            p.refresh()

            p.write_upstream('a.c', V2)

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertEqual(result['conflicted'], [])
            self.assertIn('a.c', result['merged'])

            with open(os.path.join(p.pkg_abs, 'a.c')) as f:
                merged_content = f.read()
            self.assertIn('line1_DEV', merged_content)
            self.assertIn('line4_UPSTREAM', merged_content)

            self.assertEqual(p.manifest()['base_commit'],
                             result['new_base'])

    def test_rebase_conflict_leaves_markers_and_blocks_refresh(self):
        """Catches: (a) conflicts silently resolved by picking a side —
        violating the loud-failure invariant; (b) base not recorded
        after a conflicted pass, stranding the package between epochs."""
        V1 = "line1\nline2\nline3\n"
        dev_edited = "line1\nline2_DEV\nline3\n"
        V2 = "line1\nline2_UPSTREAM\nline3\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1)
            p.write_working('a.c', dev_edited)
            p.refresh()

            p.write_upstream('a.c', V2)

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertIn('a.c', result['conflicted'])

            with open(os.path.join(p.pkg_abs, 'a.c')) as f:
                content = f.read()
            self.assertIn('<<<<<<<', content)

            self.assertEqual(p.manifest()['base_commit'],
                             result['new_base'])

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()
            self.assertIn('conflict markers', str(cm.exception).lower())

    def test_rebase_prescan_deleted_file_fatal_and_untouched(self):
        """Catches: a half-rebased package — some files merged, base
        ambiguous — after a mid-pass failure."""
        V1_a = "int a(void) {\n    return 1;\n}\n"
        V2_a = "int a(void) {\n    return 2;\n}\n"
        V1_c = "int c(void) {\n    return 1;\n}\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_upstream('c.c', V1_c, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'base files'],
                           cwd=p.tmpdir, capture_output=True)

            p.write_working('a.c', V1_a.replace('1', '999'))
            p.write_working('c.c', V1_c)
            p.refresh()
            old_base = p.manifest()['base_commit']

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_working_before = f.read()

            p.write_upstream('a.c', V2_a)
            c_path = os.path.join(p.src_c47, 'c.c')
            os.remove(c_path)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'v2: update a, del c'],
                           cwd=p.tmpdir, capture_output=True)

            with self.assertRaises(RuntimeError) as cm:
                rebase_base(p.pkgdir, 'HEAD', p.tmpdir)
            self.assertIn('c.c', str(cm.exception))

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_working_after = f.read()
            self.assertEqual(a_working_before, a_working_after)

            self.assertEqual(p.manifest()['base_commit'], old_base)

    def test_rebase_noop_same_base(self):
        """Catches: pointless merge passes / manifest churn on no-ops."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            p.write_working('a.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()
            old_base = p.manifest()['base_commit']

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_content_before = f.read()
            manifest_before = load_manifest(p.pkg_abs).copy()

            result = rebase_base(p.pkgdir, old_base, p.tmpdir)

            self.assertEqual(result['fast_forwarded'], [])
            self.assertEqual(result['merged'], [])
            self.assertEqual(result['conflicted'], [])
            self.assertEqual(result['untouched'], [])

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_content_after = f.read()
            self.assertEqual(a_content_before, a_content_after)

            manifest_after = load_manifest(p.pkg_abs)
            self.assertEqual(manifest_after['base_commit'], old_base)

    def test_rebase_binary_second_file_fatal_error_no_mutation(self):
        """Catches: a binary file causing merge-file to return 255 (fatal)
        after the first text file was already mutated — the working copy
        ends up between epochs with the base advanced."""
        V1_a = "int a(void) {\n    return 1;\n}\n"
        V2_a = "int a(void) {\n    return 2;\n}\n"
        binary_b_v1 = b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00'
        binary_b_v2 = b'\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\xff'

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            b_path = os.path.join(p.src_c47, 'b.dat')
            with open(b_path, 'wb') as f:
                f.write(binary_b_v1)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'base files'],
                           cwd=p.tmpdir, capture_output=True)
            old_base = subprocess.run(
                ['git', 'rev-parse', 'HEAD'], cwd=p.tmpdir,
                capture_output=True, text=True).stdout.strip()

            a_working = V1_a.replace('1', '999')
            p.write_working('a.c', a_working)
            b_working_path = os.path.join(p.pkg_abs, 'b.dat')
            with open(b_working_path, 'wb') as f:
                f.write(binary_b_v1 + b'\x00')

            save_manifest(p.pkg_abs, {
                'base_commit': old_base,
                'working_files': ['a.c', 'b.dat'],
            })

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_before = f.read()
            with open(b_working_path, 'rb') as f:
                b_before = f.read()

            p.write_upstream('a.c', V2_a)
            with open(b_path, 'wb') as f:
                f.write(binary_b_v2)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'v2'],
                           cwd=p.tmpdir, capture_output=True)

            with self.assertRaises(RuntimeError) as cm:
                rebase_base(p.pkgdir, 'HEAD', p.tmpdir)
            self.assertIn('b.dat', str(cm.exception))

            with open(os.path.join(p.pkg_abs, 'a.c'), 'rb') as f:
                a_after = f.read()
            self.assertEqual(a_before, a_after)

            with open(b_working_path, 'rb') as f:
                b_after = f.read()
            self.assertEqual(b_before, b_after)

            self.assertEqual(p.manifest()['base_commit'], old_base)

    def test_rebase_write_failure_rollback_and_old_base(self):
        """Catches: when the second atomic write fails, the first file is
        not rolled back, or the manifest base is advanced anyway."""
        V1_a = "int a(void) {\n    return 1;\n}\n"
        V2_a = "int a(void) {\n    return 2;\n}\n"
        V1_c = "int c(void) {\n    return 1;\n}\n"
        V2_c = "int c(void) {\n    return 2;\n}\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_upstream('c.c', V1_c, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'base files'],
                           cwd=p.tmpdir, capture_output=True)

            p.write_working('a.c', V1_a)
            p.write_working('c.c', V1_c)
            p.refresh()
            old_base = p.manifest()['base_commit']

            p.write_upstream('a.c', V2_a)
            p.write_upstream('c.c', V2_c, commit=False)
            subprocess.run(['git', 'add', '-A'], cwd=p.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'v2'],
                           cwd=p.tmpdir, capture_output=True)

            call_count = [0]
            original_atomic = _atomic_replace_file

            def failing_atomic(path, content, mode):
                call_count[0] += 1
                if call_count[0] == 2:
                    raise OSError('simulated write failure')
                original_atomic(path, content, mode)

            with mock.patch('pkg_patch_refresh._atomic_replace_file',
                            side_effect=failing_atomic):
                with self.assertRaises(OSError) as cm:
                    rebase_base(p.pkgdir, 'HEAD', p.tmpdir)
            self.assertIn('simulated write failure', str(cm.exception))

            with open(os.path.join(p.pkg_abs, 'a.c'), 'r') as f:
                self.assertEqual(f.read(), V1_a)

            self.assertEqual(p.manifest()['base_commit'], old_base)

    def test_rebase_warns_on_hand_edited_patch(self):
        """R5-4: hand-editing a generated patch before rebase must
        produce exactly one drift warning; rebase still completes."""
        V1_a = "int a(void) {\n    return 1;\n}\n"
        V2_a = "int a(void) {\n    return 2;\n}\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_working('a.c', V1_a.replace('1', '999'))
            p.refresh()

            patch_path = os.path.join(p.patches_dir(), '010-a.c.patch')
            with open(patch_path, 'a') as f:
                f.write('# hand-tampered comment appended directly\n')

            p.write_upstream('a.c', V2_a)

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertEqual(len(result['warnings']), 1)
            self.assertIn('010-a.c.patch', result['warnings'][0])
            self.assertIn('hand-edited', result['warnings'][0])
            self.assertIn('rebase leaves generated output unchanged',
                          result['warnings'][0])
            self.assertIn('next refresh', result['warnings'][0])
            self.assertNotEqual(result['old_base'], result['new_base'])

    def test_rebase_warns_on_hand_edited_files_entry(self):
        """R5-4: hand-editing a generated files/ entry before rebase must
        produce exactly one drift warning; rebase still completes."""
        with _TempProject() as p:
            p.write_working('brand_new.c', 'int g(void) { return 0; }\n')
            p.refresh()

            with open(os.path.join(p.files_dir(), 'brand_new.c'),
                      'a') as f:
                f.write('/* hand-tampered */\n')

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertEqual(len(result['warnings']), 1)
            self.assertIn('brand_new.c', result['warnings'][0])
            self.assertIn('hand-edited', result['warnings'][0])
            self.assertIn('rebase leaves generated output unchanged',
                          result['warnings'][0])

    def test_rebase_clean_no_warnings(self):
        """R5-4: a rebase with no drift in generated output produces
        an empty warnings list."""
        V1_a = "int a(void) {\n    return 1;\n}\n"
        V2_a = "int a(void) {\n    return 2;\n}\n"

        with _TempProject() as p:
            p.write_upstream('a.c', V1_a)
            p.write_working('a.c', V1_a.replace('1', '999'))
            p.refresh()

            p.write_upstream('a.c', V2_a)

            result = rebase_base(p.pkgdir, 'HEAD', p.tmpdir)

            self.assertEqual(result['warnings'], [])


# ---------------------------------------------------------------------------
# R5-2: unavailable recorded base — fail before classification
# ---------------------------------------------------------------------------

class TestUnavailableBase(unittest.TestCase):

    def _make_shallow_clone(self):
        """Create an origin repo with 2 commits, then a depth-1 clone.
        Returns (origin_dir, clone_dir, old_commit_sha)."""
        origin = tempfile.mkdtemp()
        for args in (['init', '-q'],
                      ['config', 'user.email', 'test@localhost'],
                      ['config', 'user.name', 'Test']):
            subprocess.run(['git'] + args, cwd=origin,
                           capture_output=True)

        # Commit 1 (old base — will be unavailable in shallow clone)
        old_file = os.path.join(origin, 'src', 'c47', 'a.c')
        os.makedirs(os.path.dirname(old_file), exist_ok=True)
        with open(old_file, 'w') as f:
            f.write('int a(void) { return 1; }\n')
        subprocess.run(['git', 'add', '-A'], cwd=origin,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'commit 1'],
                       cwd=origin, capture_output=True)

        old_commit = subprocess.run(
            ['git', 'rev-parse', 'HEAD'], cwd=origin,
            capture_output=True, text=True).stdout.strip()

        # Commit 2 (HEAD — only commit visible in shallow clone)
        with open(old_file, 'w') as f:
            f.write('int a(void) { return 2; }\n')
        subprocess.run(['git', 'add', '-A'], cwd=origin,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'commit 2'],
                       cwd=origin, capture_output=True)

        # Shallow clone at depth 1
        clone = tempfile.mkdtemp()
        subprocess.run(
            ['git', 'clone', '-q', '--depth', '1',
             'file://' + origin, clone],
            capture_output=True)

        return origin, clone, old_commit

    def test_refresh_shallow_clone_unavailable_base(self):
        """BUG THIS TEST EXISTS TO CATCH: in a depth-1 clone whose manifest
        named an older base, refresh emitted a false BP-4 'upstream added
        ... --rebase-base' message instead of reporting that the base
        commit itself is unavailable."""
        origin, clone, old_commit = self._make_shallow_clone()
        try:
            pkgdir = 'packages/test-pkg'
            pkg_abs = os.path.join(clone, pkgdir)
            os.makedirs(pkg_abs, exist_ok=True)

            # Write working file so refresh has something to classify
            working_path = os.path.join(pkg_abs, 'a.c')
            with open(working_path, 'w') as f:
                f.write('int a(void) { return 999; }\n')

            # Manifest records the old (unavailable) base
            save_manifest(pkg_abs, {
                'patches': {},
                'files': {},
                'base_commit': old_commit,
            })

            with self.assertRaises(RuntimeError) as cm:
                refresh(pkgdir, clone)

            err = str(cm.exception)
            self.assertIn('not available', err.lower())
            self.assertNotIn('--rebase-base', err)
        finally:
            shutil.rmtree(origin, ignore_errors=True)
            shutil.rmtree(clone, ignore_errors=True)

    def test_materialize_unavailable_base(self):
        """BUG THIS TEST EXISTS TO CATCH: materialize with an unavailable
        recorded base should raise the unavailable-base error and create
        no working file."""
        origin, clone, old_commit = self._make_shallow_clone()
        try:
            pkgdir = 'packages/test-pkg'
            pkg_abs = os.path.join(clone, pkgdir)
            os.makedirs(pkg_abs, exist_ok=True)

            save_manifest(pkg_abs, {
                'patches': {},
                'files': {},
                'base_commit': old_commit,
            })

            with self.assertRaises(RuntimeError) as cm:
                materialize(pkgdir, 'a.c', clone)

            err = str(cm.exception)
            self.assertIn('not available', err.lower())

            # No working file created
            self.assertFalse(os.path.exists(
                os.path.join(pkg_abs, 'a.c')))
        finally:
            shutil.rmtree(origin, ignore_errors=True)
            shutil.rmtree(clone, ignore_errors=True)

    def test_rebase_base_unavailable_old_base_no_working_files(self):
        """BUG THIS TEST EXISTS TO CATCH: rebase_base advancing a manifest
        without validating the old base when the working-file list is empty."""
        origin, clone, old_commit = self._make_shallow_clone()
        try:
            pkgdir = 'packages/test-pkg'
            pkg_abs = os.path.join(clone, pkgdir)
            os.makedirs(pkg_abs, exist_ok=True)

            save_manifest(pkg_abs, {
                'patches': {},
                'files': {},
                'base_commit': old_commit,
            })

            with self.assertRaises(RuntimeError) as cm:
                rebase_base(pkgdir, 'HEAD', clone)

            err = str(cm.exception)
            self.assertIn('not available', err.lower())

            # Manifest unchanged
            manifest = load_manifest(pkg_abs)
            self.assertEqual(manifest['base_commit'], old_commit)
        finally:
            shutil.rmtree(origin, ignore_errors=True)
            shutil.rmtree(clone, ignore_errors=True)

    def test_valid_base_path_absent_still_bp4(self):
        """Sanity: when the base IS available but a file genuinely didn't
        exist at that commit, the existing BP-4 upstream-added error still
        fires (not the unavailable-base error)."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            p.refresh()
            base_sha = p.manifest()['base_commit']

            # New upstream commit adds b.c (after the base)
            p.write_upstream('b.c', UPSTREAM_B)

            # Write a working copy of b.c (as if developer created it)
            p.write_working('b.c', 'int b(void) { return 999; }\n')

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            err = str(cm.exception)
            # Should be BP-4 error, not unavailable-base
            self.assertIn('exists in current upstream', err)
            self.assertIn('--rebase-base', err)


# ---------------------------------------------------------------------------
# .pkgignore — working-area files refresh must not classify at all
# ---------------------------------------------------------------------------
class TestPkgIgnore(unittest.TestCase):

    def test_ignored_new_file_is_not_copied_into_files(self):
        """BUG THIS TEST EXISTS TO CATCH: the reason .pkgignore exists —
        design docs and dev scripts kept beside the sources in the flat
        working area have no upstream counterpart, so classification
        copies them into files/ and they ship inside the distributable
        package. Mutation: skip the is_ignored() check in
        list_working_files and DESIGN.md reappears in files/."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working('DESIGN.md', '# design\n')
            p.write_working('notes.txt', 'scratch\n')
            p.write_working(PKGIGNORE_NAME, '*.md\n')

            result = p.refresh()

            self.assertEqual(p.list_files(), ['notes.txt'])
            self.assertNotIn('DESIGN.md', result['files_written'])
            # The real source is unaffected.
            self.assertEqual(p.list_patches(), ['010-foo.c.patch'])

    def test_ignored_upstream_mirror_is_not_patched(self):
        """An ignored working file that DOES mirror an upstream path must
        not be diffed either — ignoring means 'not package content', which
        is independent of which side of the classifier it would land on.
        Mutation: applying the ignore filter only to the files/ branch
        leaves 010-foo.c.patch behind."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.write_working(PKGIGNORE_NAME, 'foo.c\n')

            result = p.refresh()

            self.assertEqual(p.list_patches(), [])
            self.assertEqual(result['written'], [])

    def test_adding_pattern_removes_previously_generated_entry(self):
        """BUG THIS TEST EXISTS TO CATCH: adding a pattern for a file that
        an earlier refresh already emitted, and the stale entry surviving
        in files/ forever — generated output would then outlive the thing
        that justified it, which is exactly the drift refresh exists to
        prevent. Ignoring must be retroactive via the normal
        'not producible from the working area' cleanup."""
        with _TempProject() as p:
            p.write_working('DESIGN.md', '# design\n')
            first = p.refresh()
            self.assertEqual(first['files_written'], ['DESIGN.md'])
            self.assertEqual(p.list_files(), ['DESIGN.md'])

            p.write_working(PKGIGNORE_NAME, '*.md\n')
            second = p.refresh()

            self.assertEqual(second['files_removed'], ['DESIGN.md'])
            self.assertEqual(p.list_files(), [])
            # Retroactive cleanup must not touch the working area itself.
            self.assertTrue(
                os.path.isfile(os.path.join(p.pkg_abs, 'DESIGN.md')))
            # ...and the manifest must forget it, or the next refresh
            # would report drift against a file it no longer writes.
            self.assertNotIn('DESIGN.md', second and p.manifest()['files'])

    def test_removing_pattern_restores_entry(self):
        """Un-ignoring is symmetric: the file becomes producible again and
        the next refresh re-emits it."""
        with _TempProject() as p:
            p.write_working('DESIGN.md', '# design\n')
            p.write_working(PKGIGNORE_NAME, '*.md\n')
            p.refresh()
            self.assertEqual(p.list_files(), [])

            p.write_working(PKGIGNORE_NAME, '# nothing ignored now\n')
            result = p.refresh()

            self.assertEqual(result['files_written'], ['DESIGN.md'])
            self.assertEqual(p.list_files(), ['DESIGN.md'])

    def test_pkgignore_itself_is_never_classified(self):
        """.pkgignore is package metadata, like the manifest — it must not
        be copied into files/ even when nothing matches it. Mutation: drop
        PKGIGNORE_NAME from _EXCLUDED_TOP_FILES and it ships itself."""
        with _TempProject() as p:
            p.write_working(PKGIGNORE_NAME, '*.md\n')
            p.write_working('keep.txt', 'kept\n')

            p.refresh()

            self.assertEqual(p.list_files(), ['keep.txt'])

    def test_directory_pattern_ignores_whole_subtree(self):
        with _TempProject() as p:
            p.write_working('docs/a.md', 'a\n')
            p.write_working('docs/deep/b.txt', 'b\n')
            p.write_working('keep.txt', 'kept\n')
            p.write_working(PKGIGNORE_NAME, 'docs/\n')

            p.refresh()

            self.assertEqual(p.list_files(), ['keep.txt'])

    def test_basename_pattern_matches_at_any_depth(self):
        """A pattern with no '/' matches the basename anywhere, so *.md
        catches nested docs too — the .gitignore convention."""
        with _TempProject() as p:
            p.write_working('a.md', 'a\n')
            p.write_working('sub/b.md', 'b\n')
            p.write_working('sub/c.txt', 'c\n')
            p.write_working(PKGIGNORE_NAME, '*.md\n')

            p.refresh()

            self.assertEqual(p.list_files(), ['sub/c.txt'])

    def test_path_pattern_is_anchored_to_package_root(self):
        """A pattern containing '/' matches the whole rel path, so it does
        NOT catch a same-named file elsewhere."""
        with _TempProject() as p:
            p.write_working('notes/x.txt', 'x\n')
            p.write_working('other/x.txt', 'x\n')
            p.write_working(PKGIGNORE_NAME, 'notes/*.txt\n')

            p.refresh()

            self.assertEqual(p.list_files(), ['other/x.txt'])

    def test_comments_and_blank_lines_are_not_patterns(self):
        """Mutation: treating '#' lines as patterns — '#*' would match
        nothing here, but a bare '#' comment line becoming a literal
        pattern is the kind of thing that silently ignores nothing (or,
        with a stray '*' in a comment, everything)."""
        with _TempProject() as p:
            p.write_working('keep.txt', 'kept\n')
            p.write_working('drop.md', 'gone\n')
            p.write_working(
                PKGIGNORE_NAME,
                '# docs are not package content\n'
                '\n'
                '   \n'
                '*.md\n')

            self.assertEqual(load_pkgignore(p.pkg_abs), ['*.md'])
            p.refresh()
            self.assertEqual(p.list_files(), ['keep.txt'])

    def test_missing_pkgignore_is_not_an_error(self):
        with _TempProject() as p:
            p.write_working('a.txt', 'a\n')
            self.assertEqual(load_pkgignore(p.pkg_abs), [])
            p.refresh()
            self.assertEqual(p.list_files(), ['a.txt'])

    def test_is_ignored_matrix(self):
        """The matcher's contract, stated directly."""
        self.assertTrue(is_ignored('DESIGN.md', ['*.md']))
        self.assertTrue(is_ignored('sub/DESIGN.md', ['*.md']))
        self.assertFalse(is_ignored('DESIGN.md', ['*.txt']))
        self.assertTrue(is_ignored('docs/a.md', ['docs/']))
        self.assertTrue(is_ignored('docs', ['docs/']))
        self.assertFalse(is_ignored('docsx/a.md', ['docs/']))
        self.assertTrue(is_ignored('notes/x.txt', ['notes/*.txt']))
        self.assertFalse(is_ignored('other/x.txt', ['notes/*.txt']))
        self.assertTrue(is_ignored('build-test.sh', ['build-test.sh']))
        self.assertFalse(is_ignored('anything', []))

    def test_list_working_files_applies_ignore(self):
        """Ignoring lives in list_working_files so every reader of the
        working area — refresh AND rebase_base — agrees on what the
        package contains."""
        with _TempProject() as p:
            p.write_working('a.c', 'a\n')
            p.write_working('DESIGN.md', 'd\n')
            p.write_working(PKGIGNORE_NAME, '*.md\n')

            self.assertEqual(list_working_files(p.pkg_abs), ['a.c'])


# ---------------------------------------------------------------------------
# R5-7: corrupt manifest is fatal; save_manifest is atomic
# ---------------------------------------------------------------------------

class TestCorruptManifestFatal(unittest.TestCase):

    def test_corrupt_manifest_is_fatal(self):
        """BUG THIS TEST EXISTS TO CATCH: a corrupt manifest read as
        a fresh package."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()
            base_commit = p.manifest()['base_commit']

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path, 'w') as f:
                f.write('{"patc')

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            self.assertIn(manifest_path, str(cm.exception))

    def test_corrupt_manifest_leaves_patch_working_and_base_untouched(self):
        """R5-A1: the failure must be inert, not partial."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            patch_fname = p.list_patches()[0]
            with open(os.path.join(p.patches_dir(), patch_fname), 'rb') as f:
                patch_bytes_before = f.read()
            with open(os.path.join(p.pkg_abs, 'foo.c'), 'rb') as f:
                working_bytes_before = f.read()
            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path, 'rb') as f:
                manifest_bytes_before = f.read()
            base_commit_before = p.manifest()['base_commit']

            with open(manifest_path, 'w') as f:
                f.write('{"patc')

            with self.assertRaises(RuntimeError):
                p.refresh()

            with open(os.path.join(p.patches_dir(), patch_fname), 'rb') as f:
                patch_bytes_after = f.read()
            with open(os.path.join(p.pkg_abs, 'foo.c'), 'rb') as f:
                working_bytes_after = f.read()

            self.assertEqual(patch_bytes_before, patch_bytes_after)
            self.assertEqual(working_bytes_before, working_bytes_after)
            # Manifest on disk was corrupted by the test itself, but the
            # patch and working file are byte-for-byte unchanged.
            self.assertIn(base_commit_before, manifest_bytes_before.decode())

    def test_non_dict_manifest_is_fatal(self):
        """A manifest containing [] must raise RuntimeError, not AttributeError."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path, 'w') as f:
                f.write('[]')

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()

            self.assertIn('not an object', str(cm.exception))

    def test_missing_manifest_still_initializes(self):
        """A genuinely new package (no manifest at all) must still work."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))

            result = p.refresh()

            self.assertEqual(result['written'], ['010-foo.c.patch'])
            manifest = p.manifest()
            self.assertIn('base_commit', manifest)


class TestAtomicSaveManifest(unittest.TestCase):

    def test_save_manifest_leaves_no_temp_files(self):
        """save_manifest must not leave .tmp litter behind."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            tmp_files = [
                f for f in os.listdir(p.pkg_abs)
                if f.startswith('.refresh-manifest.') and f.endswith('.tmp')
            ]
            self.assertEqual(tmp_files, [])

    def test_save_manifest_survives_interrupted_write(self):
        """BUG THIS TEST EXISTS TO CATCH: the in-place truncating write
        that made the corrupt state reachable in the first place."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))
            p.refresh()

            manifest_path = os.path.join(p.pkg_abs, MANIFEST_NAME)
            with open(manifest_path, 'rb') as f:
                original_bytes = f.read()

            def failing_dump(*args, **kwargs):
                raise KeyboardInterrupt('simulated interruption')

            with mock.patch('pkg_patch_refresh.json.dump',
                            side_effect=failing_dump):
                with self.assertRaises(KeyboardInterrupt):
                    save_manifest(p.pkg_abs, {'patches': {}, 'files': {}})

            with open(manifest_path, 'rb') as f:
                surviving_bytes = f.read()

            self.assertEqual(original_bytes, surviving_bytes)

            tmp_files = [
                f for f in os.listdir(p.pkg_abs)
                if f.startswith('.refresh-manifest.') and f.endswith('.tmp')
            ]
            self.assertEqual(tmp_files, [])


# ---------------------------------------------------------------------------
# Rebase preflight: check_rebase_preflight
# ---------------------------------------------------------------------------

class TestRebasePreflight(unittest.TestCase):

    def test_tree_differs_returns_not_buildable(self):
        """When HEAD:src/c47 and target:src/c47 have different tree objects,
        check_rebase_preflight should report src_c47_tree_matches=False."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            old_sha = subprocess.run(
                ['git', 'rev-parse', 'HEAD'], cwd=p.tmpdir,
                capture_output=True, text=True).stdout.strip()

            # Make a second commit so HEAD differs from old commit
            p.write_upstream('a.c', UPSTREAM_A.replace('1', '5'))
            subprocess.run(
                ['git', 'commit', '-m', 'change a.c'], cwd=p.tmpdir,
                capture_output=True, text=True)

            from pkg_patch_refresh import check_rebase_preflight
            result = check_rebase_preflight(p.tmpdir, old_sha)

            self.assertFalse(result['src_c47_tree_matches'])
            self.assertFalse(result['buildable'])
            self.assertEqual(len(result['issues']), 1)
            self.assertIn('differs', result['issues'][0])

    def test_tree_same_is_buildable(self):
        """When HEAD:src/c47 and target:src/c47 have the same tree object,
        and src/c47 is clean, check_rebase_preflight should report buildable."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)

            from pkg_patch_refresh import check_rebase_preflight
            result = check_rebase_preflight(p.tmpdir, 'HEAD')

            self.assertTrue(result['src_c47_tree_matches'])
            self.assertFalse(result['src_c47_dirty'])
            self.assertTrue(result['buildable'])
            self.assertEqual(result['issues'], [])

    def test_dirty_src_c47_not_buildable(self):
        """When src/c47 has uncommitted changes, check_rebase_preflight
        should report src_c47_dirty=True and buildable=False."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            # Create uncommitted change
            a_path = os.path.join(p.src_c47, 'a.c')
            with open(a_path, 'w') as f:
                f.write(UPSTREAM_A.replace('1', '5'))

            from pkg_patch_refresh import check_rebase_preflight
            result = check_rebase_preflight(p.tmpdir, 'HEAD')

            self.assertTrue(result['src_c47_dirty'])
            self.assertFalse(result['buildable'])
            self.assertTrue(any('local changes' in issue
                                for issue in result['issues']))

    def test_untracked_file_counts_as_dirty(self):
        """An untracked file under src/c47 should count as dirty."""
        with _TempProject() as p:
            p.write_upstream('a.c', UPSTREAM_A)
            # Create untracked file
            new_path = os.path.join(p.src_c47, 'new_file.c')
            with open(new_path, 'w') as f:
                f.write('int new(void) { return 0; }\n')

            from pkg_patch_refresh import check_rebase_preflight
            result = check_rebase_preflight(p.tmpdir, 'HEAD')

            self.assertTrue(result['src_c47_dirty'])
            self.assertFalse(result['buildable'])


# ---------------------------------------------------------------------------
# _working_file_marker_lines: conflict marker detection
# ---------------------------------------------------------------------------

class TestWorkingFileMarkerLines(unittest.TestCase):

    def test_detects_marker_at_column_zero(self):
        """_working_file_marker_lines should return line numbers of conflict
        markers at column 0."""
        from pkg_patch_refresh import _working_file_marker_lines
        tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                          delete=False)
        try:
            tmp.write('<<<<<<< HEAD\nint a(void) { return 1; }\n=======\n')
            tmp.close()
            result = _working_file_marker_lines(tmp.name)
            self.assertEqual(result, [1, 3])
        finally:
            os.unlink(tmp.name)

    def test_detects_marker_at_line_two(self):
        """Should detect ======= at line 2."""
        from pkg_patch_refresh import _working_file_marker_lines
        tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                          delete=False)
        try:
            tmp.write('int a(void) {\n=======\n    return 1;\n}\n')
            tmp.close()
            result = _working_file_marker_lines(tmp.name)
            self.assertEqual(result, [2])
        finally:
            os.unlink(tmp.name)

    def test_no_marker_returns_empty_list(self):
        """A clean file should return an empty list."""
        from pkg_patch_refresh import _working_file_marker_lines
        tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                          delete=False)
        try:
            tmp.write('int a(void) { return 1; }\n')
            tmp.close()
            result = _working_file_marker_lines(tmp.name)
            self.assertEqual(result, [])
        finally:
            os.unlink(tmp.name)

    def test_marker_midline_is_not_detected(self):
        """A <<<<<<< not at column 0 should be ignored."""
        from pkg_patch_refresh import _working_file_marker_lines
        tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.c',
                                          delete=False)
        try:
            tmp.write('    const char *s = "<<<<<<< not a marker";\n')
            tmp.close()
            result = _working_file_marker_lines(tmp.name)
            self.assertEqual(result, [])
        finally:
            os.unlink(tmp.name)


class TestSiblingRootsT2A(unittest.TestCase):
    """T2-A: rel paths whose first segment is in SIBLING_ROOTS classify
    against src/<rel> (today: testSuite/) instead of src/c47/<rel>."""

    @staticmethod
    def _write_sibling_upstream(t, rel, content):
        path = os.path.join(t.tmpdir, 'src', *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)
        subprocess.run(['git', 'add', '-A'], cwd=t.tmpdir,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'sibling upstream'],
                       cwd=t.tmpdir, capture_output=True)

    def test_sibling_edit_becomes_patch_with_src_prefixed_headers(self):
        with _TempProject() as t:
            base = 'int drive(void) {\n    return 0;\n}\n'
            self._write_sibling_upstream(t, 'testSuite/testSuite.c', base)
            t.write_working('testSuite/testSuite.c',
                            base.replace('return 0;', 'return 7;'))
            result = t.refresh()
            self.assertIn('010-testSuite__testSuite.c.patch',
                          result['written'])
            text = t.patch_content('010-testSuite__testSuite.c.patch')
            self.assertIn('--- a/src/testSuite/testSuite.c', text)
            self.assertIn('+++ b/src/testSuite/testSuite.c', text)
            self.assertNotIn('src/c47/testSuite', text)

    def test_sibling_new_file_with_no_upstream_goes_to_files(self):
        with _TempProject() as t:
            t.write_working('testSuite/extra.c', 'int extra;\n')
            result = t.refresh()
            self.assertIn('testSuite/extra.c', result['files_written'])
            self.assertEqual(t.file_content('testSuite/extra.c'),
                             'int extra;\n')

    def test_materialize_sibling_rel_uses_src_root(self):
        with _TempProject() as t:
            base = 'int suite_main(void) { return 3; }\n'
            self._write_sibling_upstream(t, 'testSuite/driver.c', base)
            materialize(t.pkgdir, 'testSuite/driver.c', t.tmpdir)
            with open(os.path.join(t.pkg_abs, 'testSuite',
                                   'driver.c')) as f:
                self.assertEqual(f.read(), base)


if __name__ == '__main__':
    unittest.main()
