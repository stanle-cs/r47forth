#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_refresh.py (plain-diff design, automatic
classification / flat working directory revision).

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
    list_working_files,
    generate_patch,
    load_manifest,
    MANIFEST_NAME,
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
        ancestry assumption from the very start. Mutation: remove the
        cat-file -e check in generate_patch()."""
        with _TempProject() as p:
            p.write_upstream('foo.c', UPSTREAM_A, commit=False)
            p.write_working('foo.c', UPSTREAM_A.replace('1', '999'))

            with self.assertRaises(RuntimeError) as cm:
                p.refresh()
            self.assertIn('not a resolvable git object', str(cm.exception))


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
