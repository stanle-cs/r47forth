#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_refresh.py.

Run via: python3 tools/test_pkg_patch_refresh.py
  or:    meson test -C build.sim pkg_patch_refresh

Each test's docstring names the specific bug / mutation it must catch.
Ordered to mirror Unit 3's incremental steps:
  (a) detection  (b) single-function patch  (c) multi-function
  (d) real-repo CLI-path run against an actual src/c47/ file.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

# Ensure the tools directory is on sys.path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_refresh import (
    detect_changed_functions,
    refresh,
)

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.abspath(os.path.join(_TOOLS_DIR, '..'))

UPSTREAM_3FN = """\
// Three-function fixture
int alpha(int x) {
    return x + 1;
}

int beta(int x) {
    return x + 2;
}

int gamma(int x) {
    return x + 3;
}
"""


def _edit_function(src, old, new):
    assert old in src
    return src.replace(old, new)


class _TempGitRepo:
    """Temporary project-root-shaped git repo for refresh() tests.

    Structure:
      <tmp>/src/c47/<rel>            — upstream file (committed to git)
      <tmp>/packages/test-pkg/<rel>  — materialized working file
      <tmp>/packages/test-pkg/patches/ — patch output directory
      <tmp>/build.sim/compile_commands.json — for libclang
    """

    def __init__(self, rel='test.c', pkgdir='packages/test-pkg'):
        self.rel = rel
        self.pkgdir = pkgdir
        self.tmpdir = None

    def __enter__(self):
        self.tmpdir = tempfile.mkdtemp()
        self.src_path = os.path.join(self.tmpdir, 'src', 'c47', self.rel)
        self.materialized_path = os.path.join(
            self.tmpdir, self.pkgdir, self.rel)
        self.patches_dir = os.path.join(self.tmpdir, self.pkgdir, 'patches')
        self.cc_path = os.path.join(self.tmpdir, 'build.sim',
                                    'compile_commands.json')

        os.makedirs(os.path.dirname(self.src_path), exist_ok=True)
        os.makedirs(os.path.dirname(self.materialized_path), exist_ok=True)
        os.makedirs(self.patches_dir, exist_ok=True)
        os.makedirs(os.path.dirname(self.cc_path), exist_ok=True)

        for args in (['init', '-q'],
                     ['config', 'user.email', 'test@localhost'],
                     ['config', 'user.name', 'Test']):
            subprocess.run(['git'] + args, cwd=self.tmpdir,
                           capture_output=True)

        with open(self.cc_path, 'w') as f:
            json.dump([{
                'directory': os.path.dirname(self.src_path),
                'command': f'cc -c {os.path.basename(self.rel)} -o x.o',
                'file': os.path.basename(self.rel),
            }], f)
        return self

    def write_src(self, content, commit=True):
        with open(self.src_path, 'w') as f:
            f.write(content)
        if commit:
            subprocess.run(['git', 'add', '-A'], cwd=self.tmpdir,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'upstream'],
                           cwd=self.tmpdir, capture_output=True)

    def write_materialized(self, content):
        with open(self.materialized_path, 'w') as f:
            f.write(content)

    def refresh(self):
        return refresh(self.pkgdir, self.rel, self.tmpdir, self.cc_path)

    def list_patches(self):
        return sorted(f for f in os.listdir(self.patches_dir)
                      if f.endswith('.patch'))

    def apply_patches_to_src(self, filenames):
        """git-apply the given patches (in order) inside the repo and
        return the resulting src file content (src restored after)."""
        with open(self.src_path) as f:
            original = f.read()
        try:
            for fname in filenames:
                result = subprocess.run(
                    ['git', 'apply',
                     os.path.join(self.patches_dir, fname)],
                    cwd=self.tmpdir, capture_output=True, text=True)
                if result.returncode != 0:
                    raise AssertionError(
                        f'git apply {fname} failed: {result.stderr}')
            with open(self.src_path) as f:
                return f.read()
        finally:
            with open(self.src_path, 'w') as f:
                f.write(original)

    def __exit__(self, *exc):
        if self.tmpdir:
            shutil.rmtree(self.tmpdir, ignore_errors=True)


# ---------------------------------------------------------------------------
# (a) detection
# ---------------------------------------------------------------------------

class TestDetection(unittest.TestCase):
    """Step (a): body-string comparison detection."""

    def test_detects_exactly_the_changed_function(self):
        """BUG THIS TEST EXISTS TO CATCH: detection reporting unchanged
        functions as changed (over-reporting produces patches that churn
        every function), or missing the changed one entirely.
        Three functions, only beta edited -> exactly ['beta'].
        """
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            d = detect_changed_functions(repo.src_path,
                                         repo.materialized_path,
                                         repo.cc_path)
        self.assertEqual(d['changed'], ['beta'])
        self.assertEqual(d['added'], [])
        self.assertEqual(d['removed'], [])

    def test_detects_multiple_changed_in_upstream_order(self):
        """Bug: multiple changed functions dropped or misordered.
        'changed' must be ordered by upstream start line (deterministic
        ordinal assignment depends on this)."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 3;', 'return x * 33;')
        materialized = _edit_function(materialized,
                                      'return x + 1;', 'return x * 11;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            d = detect_changed_functions(repo.src_path,
                                         repo.materialized_path,
                                         repo.cc_path)
        self.assertEqual(d['changed'], ['alpha', 'gamma'])

    def test_added_function_reported_as_added(self):
        """Bug: a brand-new function in the materialized file silently
        treated as a body change (or ignored). It must surface in
        'added' so refresh can reject it as structural (§8)."""
        materialized = UPSTREAM_3FN + '\nint delta(void) {\n    return 4;\n}\n'
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            d = detect_changed_functions(repo.src_path,
                                         repo.materialized_path,
                                         repo.cc_path)
        self.assertEqual(d['added'], ['delta'])


# ---------------------------------------------------------------------------
# §8 loud rejection of non-function-scoped changes
# ---------------------------------------------------------------------------

class TestRefreshRejectsNonFunctionChanges(unittest.TestCase):

    def test_global_change_rejected_no_patch_written(self):
        """BUG THIS TEST EXISTS TO CATCH: a change to a global variable
        initializer silently producing (or being folded into) a
        function-level patch.

        Mutation: remove the reconstruction totality check in refresh()
        — the global edit is silently dropped from the generated patches
        and this test fails.
        """
        upstream = 'int global_var = 42;\n\n' + UPSTREAM_3FN
        materialized = upstream.replace('int global_var = 42;',
                                        'int global_var = 99;')
        with _TempGitRepo() as repo:
            repo.write_src(upstream)
            repo.write_materialized(materialized)
            with self.assertRaises(RuntimeError) as cm:
                repo.refresh()
            self.assertIn('whole-file override', str(cm.exception))
            self.assertIn('§8', str(cm.exception))
            self.assertEqual(repo.list_patches(), [])

    def test_global_change_alongside_function_change_rejected(self):
        """Bug: a global change hidden *next to* a legitimate function
        change gets silently dropped while the function patch is
        written — the package would build without part of its edit.
        Refresh must reject the whole file and write nothing."""
        upstream = 'int global_var = 42;\n\n' + UPSTREAM_3FN
        materialized = upstream.replace('int global_var = 42;',
                                        'int global_var = 99;')
        materialized = _edit_function(materialized,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(upstream)
            repo.write_materialized(materialized)
            with self.assertRaises(RuntimeError):
                repo.refresh()
            self.assertEqual(repo.list_patches(), [])

    def test_added_function_rejected(self):
        """Bug: added function treated as patchable instead of the
        structural change it is (§8 — whole-file override territory)."""
        materialized = UPSTREAM_3FN + '\nint delta(void) {\n    return 4;\n}\n'
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            with self.assertRaises(RuntimeError) as cm:
                repo.refresh()
            self.assertIn('added', str(cm.exception))
            self.assertEqual(repo.list_patches(), [])


# ---------------------------------------------------------------------------
# (b) single changed function -> one valid patch
# ---------------------------------------------------------------------------

class TestSingleFunctionPatch(unittest.TestCase):

    def test_patch_written_applies_and_reproduces_exactly(self):
        """Step (b) gate: refresh writes exactly one .patch per the
        storage convention; git apply --check accepts it against the
        fixture upstream; applying it reproduces the edited file
        byte-for-byte.

        BUG THIS TEST EXISTS TO CATCH: malformed generated diffs (wrong
        hunk offsets from header rewriting, missing git headers) that
        git apply rejects, or that apply to the wrong content.
        """
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            self.assertEqual(written, ['010-test.c.patch'])
            self.assertEqual(repo.list_patches(), ['010-test.c.patch'])

            patch_path = os.path.join(repo.patches_dir, written[0])
            check = subprocess.run(['git', 'apply', '--check', patch_path],
                                   cwd=repo.tmpdir, capture_output=True,
                                   text=True)
            self.assertEqual(check.returncode, 0,
                             f'git apply --check failed: {check.stderr}')

            result = repo.apply_patches_to_src(written)
            self.assertEqual(result, materialized)

    def test_patch_carries_dual_signal_and_git_headers(self):
        """Bug: patch missing the 'diff --git' header or the index line
        (git apply -3 needs both), or +++ header disagreeing with the
        filename (would trip the §2 dual-signal check at configure)."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()
            with open(os.path.join(repo.patches_dir, written[0])) as f:
                content = f.read()

        self.assertIn('diff --git a/src/c47/test.c b/src/c47/test.c',
                      content)
        self.assertIn('+++ b/src/c47/test.c', content)
        self.assertRegex(content, r'index [0-9a-f]{40}\.\.[0-9a-f]{40}')

    def test_only_changed_function_touched(self):
        """Bug: hunks bleeding into neighboring functions. The patch's
        removed lines must all come from beta's body, none from alpha
        or gamma."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()
            with open(os.path.join(repo.patches_dir, written[0])) as f:
                content = f.read()

        removed = [l[1:] for l in content.split('\n')
                   if l.startswith('-') and not l.startswith('---')]
        self.assertEqual(removed, ['    return x + 2;'])


# ---------------------------------------------------------------------------
# (c) multiple changed functions
# ---------------------------------------------------------------------------

class TestMultiFunctionPatches(unittest.TestCase):

    def test_two_changed_functions_two_patches_reproduce_file(self):
        """Step (c) gate: two changed functions produce two patch files
        with distinct ordinals (upstream order), and applying the stack
        in ordinal order reproduces the edited file exactly.

        BUG THIS TEST EXISTS TO CATCH: both functions written to the
        same ordinal/filename so the second silently overwrites the
        first (the prior draft's actual bug), or hunk offsets breaking
        stacked application.
        """
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 1;', 'return x * 11;')
        materialized = _edit_function(materialized,
                                      'return x + 3;', 'return x * 33;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            self.assertEqual(written,
                             ['010-test.c.patch', '020-test.c.patch'])

            # alpha (earlier in upstream) gets the lower ordinal
            with open(os.path.join(repo.patches_dir, written[0])) as f:
                self.assertIn('x * 11', f.read())
            with open(os.path.join(repo.patches_dir, written[1])) as f:
                self.assertIn('x * 33', f.read())

            result = repo.apply_patches_to_src(written)
            self.assertEqual(result, materialized)


# ---------------------------------------------------------------------------
# Refresh bookkeeping: ordinal reuse, stale removal, blob ancestry
# ---------------------------------------------------------------------------

class TestRefreshBookkeeping(unittest.TestCase):

    def test_refresh_reuses_ordinal_on_second_call(self):
        """BUG THIS TEST EXISTS TO CATCH: re-running refresh after a
        further edit to the same function accumulating a second patch
        file instead of rewriting the existing one in place."""
        v1 = _edit_function(UPSTREAM_3FN, 'return x + 2;', 'return x * 3;')
        v2 = _edit_function(UPSTREAM_3FN, 'return x + 2;', 'return x * 4;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(v1)
            first = repo.refresh()
            repo.write_materialized(v2)
            second = repo.refresh()

            self.assertEqual(first, second)
            self.assertEqual(repo.list_patches(), first)
            result = repo.apply_patches_to_src(second)
            self.assertEqual(result, v2)

    def test_refresh_removes_stale_patch_when_function_reverted(self):
        """Bug: a patch for a function whose edit was reverted lingers
        on disk and keeps applying a change the materialized file no
        longer contains."""
        edited = _edit_function(UPSTREAM_3FN,
                                'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(edited)
            repo.refresh()
            self.assertEqual(len(repo.list_patches()), 1)

            # Revert: materialized now identical to upstream
            repo.write_materialized(UPSTREAM_3FN)
            written = repo.refresh()
            self.assertEqual(written, [])
            self.assertEqual(repo.list_patches(), [])

    def test_refresh_blob_sha_resolvable(self):
        """BUG THIS TEST EXISTS TO CATCH: a fabricated pre-image SHA in
        the index line. git apply -3 needs `git cat-file -e <sha>` to
        succeed or three-way merge silently degrades (§5)."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()
            with open(os.path.join(repo.patches_dir, written[0])) as f:
                content = f.read()

            m = re.search(r'index ([0-9a-f]{40})\.\.', content)
            self.assertIsNotNone(m)
            pre_sha = m.group(1)

            result = subprocess.run(['git', 'cat-file', '-t', pre_sha],
                                    cwd=repo.tmpdir, capture_output=True,
                                    text=True)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(result.stdout.strip(), 'blob')

    def test_refresh_refuses_uncommitted_upstream(self):
        """Bug: refresh generating a patch whose pre-image blob is not a
        real git object (upstream file modified but not committed) —
        breaking the -3 ancestry assumption from the very start.
        Mutation: remove the cat-file -e check in refresh()."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN, commit=False)  # never committed
            repo.write_materialized(materialized)
            with self.assertRaises(RuntimeError) as cm:
                repo.refresh()
            self.assertIn('not a resolvable git object', str(cm.exception))


# ---------------------------------------------------------------------------
# (d) real-repo run against an actual src/c47/ file
# ---------------------------------------------------------------------------

class TestRealRepoRefresh(unittest.TestCase):
    """Step (d): the refresh command against a real upstream file in
    this repository, via a scratch package directory (created and
    removed by the test)."""

    def _pick_real_target(self, cc_path):
        """Pick a small, git-clean src/c47/*.c file with a compile
        entry (resolved through custom_pkg_shadow symlinks if the build
        was configured with CUSTOM_PKG)."""
        src_c47 = os.path.realpath(os.path.join(_REPO_ROOT, 'src', 'c47'))
        with open(cc_path) as f:
            entries = json.load(f)
        candidates = []
        for entry in entries:
            path = os.path.realpath(
                os.path.join(entry.get('directory', '.'), entry['file']))
            if not path.startswith(src_c47 + os.sep):
                continue
            if not path.endswith('.c'):
                continue
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
        on a real upstream file with real compiler flags (include
        paths, defines, shadow-tree symlink resolution)."""
        cc_path = os.path.join(_REPO_ROOT, 'build.sim',
                               'compile_commands.json')
        if not os.path.isfile(cc_path):
            self.skipTest('no build.sim/compile_commands.json')

        picked = self._pick_real_target(cc_path)
        if picked is None:
            self.skipTest('no clean src/c47/*.c with a compile entry')
        _, rel, src_path = picked

        from pkg_patch_extract import list_function_ranges
        ranges = list_function_ranges(src_path, cc_path)
        if not ranges:
            self.skipTest(f'{rel} has no function definitions')
        fn_name, fn_start, fn_end = ranges[-1]

        pkgdir = 'packages/_scratch_refresh_test'
        pkg_path = os.path.join(_REPO_ROOT, pkgdir)
        try:
            os.makedirs(os.path.join(pkg_path,
                                     os.path.dirname(rel) or '.'),
                        exist_ok=True)
            with open(src_path) as f:
                lines = f.readlines()
            # Insert a line inside the last function's body
            lines.insert(fn_end - 1,
                         '  /* pkg_patch_refresh 3d test */\n')
            mat_path = os.path.join(pkg_path, rel)
            with open(mat_path, 'w') as f:
                f.writelines(lines)

            written = refresh(pkgdir, rel, _REPO_ROOT, cc_path)
            self.assertEqual(len(written), 1)

            patch_path = os.path.join(pkg_path, 'patches', written[0])
            with open(patch_path) as f:
                patch_content = f.read()
            self.assertIn('pkg_patch_refresh 3d test', patch_content)

            # Non-destructive validation against the real tree
            check = subprocess.run(['git', 'apply', '--check', patch_path],
                                   cwd=_REPO_ROOT, capture_output=True,
                                   text=True)
            self.assertEqual(check.returncode, 0,
                             f'git apply --check failed: {check.stderr}')

            # Full application in a scratch repo reproduces the edit
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
                    self.assertEqual(f.read(), ''.join(lines))
            finally:
                shutil.rmtree(scratch, ignore_errors=True)
        finally:
            shutil.rmtree(pkg_path, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
