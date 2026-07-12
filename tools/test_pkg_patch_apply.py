#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_apply.py.

Run via: python3 tools/test_pkg_patch_apply.py
  or:    meson test -C build.sim pkg_patch_apply

Each test's docstring names the specific bug / mutation it must catch.

These tests use refresh() (authoring side, libclang) to GENERATE real
patches, then exercise apply_patch_stack (build side) on them — the same
producer/consumer pairing as production. libclang stays a test-only
dependency here; apply itself must never import it (asserted below).
"""
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_apply import (
    collect_patch_stacks,
    PatchApplyError,
    apply_patch_stack,
    scan_conflict_markers,
)
from test_pkg_patch_refresh import _TempGitRepo, _edit_function, UPSTREAM_3FN


def _patch_paths(repo, filenames):
    return [os.path.join(repo.patches_dir, f) for f in filenames]


class TestNoLibclangDependency(unittest.TestCase):
    """§4 (ratified): the build-time apply path must not import libclang."""

    def test_pkg_patch_apply_never_imports_clang(self):
        """Mutation: add an import of clang or pkg_patch_extract to
        pkg_patch_apply.py — this fails. Fresh interpreter so this test
        file's own (legitimate, test-only) libclang import can't mask
        the result."""
        code = (
            'import sys; sys.path.insert(0, sys.argv[1]); '
            'import pkg_patch_apply; '
            'bad = [m for m in sys.modules if m == "clang" '
            'or m.startswith("clang.") or m == "pkg_patch_extract"]; '
            'sys.exit(1 if bad else 0)'
        )
        result = subprocess.run(
            [sys.executable, '-c', code,
             os.path.dirname(os.path.abspath(__file__))],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         'importing pkg_patch_apply must not pull in '
                         'clang/clang.cindex or pkg_patch_extract')


class TestScanConflictMarkers(unittest.TestCase):

    def test_detects_all_three_marker_kinds(self):
        """Bug: scanner missing one marker kind (e.g. only checking
        <<<<<<<) — a merge that only left ======= /  >>>>>>> lines
        would slip through."""
        text = ('int f(void) {\n'
                '<<<<<<< ours\n'
                '    return 1;\n'
                '=======\n'
                '    return 2;\n'
                '>>>>>>> theirs\n'
                '}\n')
        self.assertEqual(scan_conflict_markers(text), [2, 4, 6])

    def test_clean_text_no_hits(self):
        """Bug: over-eager matching flagging normal C (comparison
        operators, <<= shifts, comment rules of dashes)."""
        text = ('int f(int a, int b) {\n'
                '    a <<= 2;\n'
                '    // ---- section ----\n'
                '    return a >= b ? a : b;\n'
                '}\n')
        self.assertEqual(scan_conflict_markers(text), [])


class TestCleanApplication(unittest.TestCase):

    def test_single_patch_applies_to_dest(self):
        """Bug: basic materialize+apply pipeline broken (wrong paths in
        scratch repo, result not copied to dest). The dest file must be
        byte-identical to the authored materialized file, and the real
        upstream file must be untouched."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            final = apply_patch_stack(
                repo.rel, _patch_paths(repo, written), repo.tmpdir, dest)

            self.assertEqual(final, materialized)
            with open(dest) as f:
                self.assertEqual(f.read(), materialized)
            with open(repo.src_path) as f:
                self.assertEqual(f.read(), UPSTREAM_3FN,
                                 'real upstream file must never be '
                                 'modified by patch application')

    def test_two_patch_stack_cumulative(self):
        """Bug: second patch applied against fresh upstream instead of
        the first patch's output (last-wins regression) — result would
        contain only one of the two edits."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 1;', 'return x * 11;')
        materialized = _edit_function(materialized,
                                      'return x + 3;', 'return x * 33;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()
            self.assertEqual(len(written), 2)

            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            final = apply_patch_stack(
                repo.rel, _patch_paths(repo, written), repo.tmpdir, dest)

            self.assertIn('x * 11', final)
            self.assertIn('x * 33', final)
            self.assertEqual(final, materialized)


class TestLoudFailure(unittest.TestCase):

    def test_outright_failure_names_patch(self):
        """Bug: a patch that cannot apply at all (its target text is
        gone and its pre-image blob is unknown) being silently skipped.
        Must raise PatchApplyError naming the patch file."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            # Drifted base: beta was rewritten upstream; the patch's
            # context is gone AND the pre-image blob is not in this
            # scratch project's odb (simulates cross-clone drift with
            # no shared history).
            drifted = UPSTREAM_3FN.replace(
                'int beta(int x) {\n    return x + 2;\n}',
                'long beta_impl(long x) {\n    return x + 20;\n}')
            other = _TempGitRepo(rel='test.c', pkgdir='packages/other')
            with other as fresh_project:
                fresh_project.write_src(drifted)
                dest = os.path.join(fresh_project.tmpdir, 'out', 'test.c')
                with self.assertRaises(PatchApplyError) as cm:
                    apply_patch_stack(
                        'test.c',
                        _patch_paths(repo, written),
                        fresh_project.tmpdir, dest)
                self.assertIn(written[0], str(cm.exception))
                self.assertFalse(os.path.exists(dest),
                                 'no output may be produced on failure')

    def test_genuine_conflict_fails_loudly(self):
        """§7 hard invariant: patch edits a function the base has ALSO
        changed (same lines, different content), pre-image blob
        available → git apply -3 three-way-merges into a conflicted
        state. Must raise, whether git signals it or the marker scan
        catches it."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            conflicted_base = _edit_function(
                UPSTREAM_3FN, 'return x + 2;', 'return x - 99;')
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            with self.assertRaises(PatchApplyError) as cm:
                apply_patch_stack(
                    repo.rel, _patch_paths(repo, written), repo.tmpdir,
                    dest, base_content=conflicted_base)
            self.assertIn(written[0], str(cm.exception))

    def test_preexisting_markers_caught_even_when_git_apply_succeeds(self):
        """RATIFIED §5 core case: git apply exits 0, but the result
        contains conflict markers — the unconditional scan must catch
        it. Constructed via a base that already carries marker lines
        away from the patch site (so the patch itself applies cleanly).

        Mutation: make the scan conditional on git apply's exit status
        — this test fails, because git reports success here."""
        materialized = _edit_function(UPSTREAM_3FN,
                                      'return x + 2;', 'return x * 22;')
        with _TempGitRepo() as repo:
            repo.write_src(UPSTREAM_3FN)
            repo.write_materialized(materialized)
            written = repo.refresh()

            poisoned_base = UPSTREAM_3FN.replace(
                'int gamma(int x) {\n    return x + 3;\n}',
                'int gamma(int x) {\n'
                '<<<<<<< pkg-a\n'
                '    return x + 3;\n'
                '=======\n'
                '    return x - 3;\n'
                '>>>>>>> pkg-b\n'
                '}')
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            with self.assertRaises(PatchApplyError) as cm:
                apply_patch_stack(
                    repo.rel, _patch_paths(repo, written), repo.tmpdir,
                    dest, base_content=poisoned_base)
            self.assertIn('conflict markers', str(cm.exception))


UPSTREAM_TALL = """\
// Tall fixture for drift tests
int beta(int x) {
    int acc = x;
    acc += 10;
    acc += 20;
    acc += 30;
    return acc;
}
"""


class TestBlobAncestryDrift(unittest.TestCase):
    """Regression encoding of the §5 empirical findings: -3 merges
    under drift ONLY when the pre-image blob is resolvable where apply
    runs (apply_patch_stack seeds it from the project repo's odb), and
    only when drift and edit are separated by at least one unchanged
    line — adjacent-line drift conflicts loudly."""

    def _refreshed_repo(self):
        """Repo with a committed tall upstream and one patch editing
        the middle of beta ('acc += 20;' -> 'acc += 2000;')."""
        repo = _TempGitRepo()
        repo.__enter__()
        repo.write_src(UPSTREAM_TALL)
        repo.write_materialized(
            _edit_function(UPSTREAM_TALL, 'acc += 20;', 'acc += 2000;'))
        written = repo.refresh()
        return repo, written

    def test_separated_context_drift_merges_via_seeded_blob(self):
        """Bug: dropping the blob-seeding step. Drift rewrites beta's
        signature line — a context line of the hunk — so plain apply
        fails on context mismatch; only a real three-way merge against
        the recorded (seeded) pre-image blob can succeed. The drift is
        separated from the edited line by two unchanged lines, so the
        merge is clean and must contain BOTH the drift and the edit.

        Mutation: remove _seed_blob() from apply_patch_stack — git
        apply -3 reports 'lacks the necessary blob' and this fails."""
        repo, written = self._refreshed_repo()
        try:
            drifted = UPSTREAM_TALL.replace(
                'int beta(int x) {',
                'int beta(int x) { /* upstream drift */')
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            final = apply_patch_stack(
                repo.rel, _patch_paths(repo, written), repo.tmpdir,
                dest, base_content=drifted)
            self.assertIn('/* upstream drift */', final)
            self.assertIn('acc += 2000;', final)
        finally:
            repo.__exit__()

    def test_same_line_drift_conflicts_loudly(self):
        """§7: drift rewrote the very line the patch edits (divergent
        content) — must raise, never silently pick either side."""
        repo, written = self._refreshed_repo()
        try:
            drifted = UPSTREAM_TALL.replace('acc += 20;', 'acc += 21;')
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            with self.assertRaises(PatchApplyError) as cm:
                apply_patch_stack(
                    repo.rel, _patch_paths(repo, written), repo.tmpdir,
                    dest, base_content=drifted)
            self.assertIn(written[0], str(cm.exception))
        finally:
            repo.__exit__()

    def test_adjacent_line_drift_conflicts_loudly(self):
        """Empirical §5/§1 finding encoded: drift on the line ADJACENT
        to the edited line (no unchanged separation) is treated as one
        overlapping region by git's merge — it must conflict loudly,
        not silently reorder or drop either change."""
        repo, written = self._refreshed_repo()
        try:
            drifted = UPSTREAM_TALL.replace('acc += 30;', 'acc += 31;')
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            with self.assertRaises(PatchApplyError) as cm:
                apply_patch_stack(
                    repo.rel, _patch_paths(repo, written), repo.tmpdir,
                    dest, base_content=drifted)
            self.assertIn('conflict markers', str(cm.exception))
        finally:
            repo.__exit__()


def _author_git_patch(base_text, new_text, rel='test.c'):
    """Author a git-format patch (full index line) taking src/c47/<rel>
    from base_text to new_text, via a throwaway git repo."""
    import shutil
    import tempfile
    t = tempfile.mkdtemp()
    try:
        for args in (['init', '-q'], ['config', 'user.email', 'x@x'],
                     ['config', 'user.name', 'x']):
            subprocess.run(['git'] + args, cwd=t, capture_output=True)
        path = os.path.join(t, 'src', 'c47', rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(base_text)
        subprocess.run(['git', 'add', '-A'], cwd=t, capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'base'], cwd=t,
                       capture_output=True)
        with open(path, 'w') as f:
            f.write(new_text)
        return subprocess.run(['git', 'diff', '--full-index'], cwd=t,
                              capture_output=True, text=True).stdout
    finally:
        shutil.rmtree(t, ignore_errors=True)


class TestOrderingEnforcement(unittest.TestCase):
    """Unit 5: numeric-prefix ordering for cumulative per-file stacks
    (§3): sort by integer ordinal, tie-break by CUSTOM_PKG list order;
    malformed or misdeclared patches rejected loudly."""

    # Order-dependent chain: each patch edits the SAME line, so any
    # out-of-order application fails to apply.
    V1 = UPSTREAM_3FN
    V2 = UPSTREAM_3FN.replace('return x + 2;', 'return x + 2 + 10;')
    V3 = UPSTREAM_3FN.replace('return x + 2;', 'return x + 2 + 10 + 20;')
    V4 = UPSTREAM_3FN.replace('return x + 2;',
                              'return x + 2 + 10 + 20 + 30;')

    def _repo_with_chain(self, names=('010-test.c.patch',
                                      '020-test.c.patch',
                                      '030-test.c.patch')):
        """Repo whose patches/ holds the three chained patches under
        *names* (written to disk in reverse order deliberately)."""
        repo = _TempGitRepo()
        repo.__enter__()
        repo.write_src(self.V1)
        chain = [_author_git_patch(self.V1, self.V2),
                 _author_git_patch(self.V2, self.V3),
                 _author_git_patch(self.V3, self.V4)]
        for fname, text in reversed(list(zip(names, chain))):
            with open(os.path.join(repo.patches_dir, fname), 'w') as f:
                f.write(text)
        return repo

    def test_stack_applies_in_ordinal_order_regardless_of_declaration_order(self):
        """BUG THIS TEST EXISTS TO CATCH: applying patches in
        declaration or directory order instead of parsed-ordinal order.
        The chain only applies 010→020→030; declarations are shuffled.
        """
        repo = self._repo_with_chain()
        try:
            declared = ['030-test.c.patch', '010-test.c.patch',
                        '020-test.c.patch']
            stacks = collect_patch_stacks(
                [(repo.pkgdir, declared)], repo.tmpdir)
            self.assertEqual(list(stacks.keys()), ['test.c'])
            self.assertEqual(
                [os.path.basename(p) for p in stacks['test.c']],
                ['010-test.c.patch', '020-test.c.patch',
                 '030-test.c.patch'])

            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            final = apply_patch_stack('test.c', stacks['test.c'],
                                      repo.tmpdir, dest)
            self.assertEqual(final, self.V4)
        finally:
            repo.__exit__()

    def test_wrong_ordinal_assignment_fails_loudly(self):
        """Bug: an out-of-order stack silently producing a wrong file.
        With the chain's first patch renamed to 030 and its last to 010
        the sorted stack is genuinely mis-ordered — application must
        raise, not emit a half-patched file."""
        repo = self._repo_with_chain(names=('030-test.c.patch',
                                            '020-test.c.patch',
                                            '010-test.c.patch'))
        try:
            declared = sorted(repo.list_patches())
            stacks = collect_patch_stacks(
                [(repo.pkgdir, declared)], repo.tmpdir)
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            with self.assertRaises(PatchApplyError):
                apply_patch_stack('test.c', stacks['test.c'],
                                  repo.tmpdir, dest)
            self.assertFalse(os.path.exists(dest))
        finally:
            repo.__exit__()

    def test_missing_prefix_rejected(self):
        """Bug: a patch without the <NNN>- prefix accepted with some
        default ordinal — ordering would become implementation-defined.
        Must be a loud failure naming the file."""
        repo = self._repo_with_chain()
        try:
            bad = os.path.join(repo.patches_dir, 'test.c.patch')
            with open(bad, 'w') as f:
                f.write(_author_git_patch(self.V1, self.V2))
            declared = ['010-test.c.patch', '020-test.c.patch',
                        '030-test.c.patch', 'test.c.patch']
            with self.assertRaises(PatchApplyError) as cm:
                collect_patch_stacks([(repo.pkgdir, declared)],
                                     repo.tmpdir)
            self.assertIn('test.c.patch', str(cm.exception))
        finally:
            repo.__exit__()

    def test_declared_but_missing_rejected(self):
        """Bug: a declared patch file missing on disk silently skipped
        (typo'd filename in pkg_patch_sources -> patch quietly absent
        from the build)."""
        repo = self._repo_with_chain()
        try:
            declared = ['010-test.c.patch', '020-test.c.patch',
                        '030-test.c.patch', '040-test.c.patch']
            with self.assertRaises(PatchApplyError) as cm:
                collect_patch_stacks([(repo.pkgdir, declared)],
                                     repo.tmpdir)
            self.assertIn('040-test.c.patch', str(cm.exception))
        finally:
            repo.__exit__()

    def test_on_disk_but_undeclared_rejected(self):
        """Bug: a .patch present in patches/ but absent from
        pkg_patch_sources silently dropped from the build — the exact
        silent-misconfiguration failure mode the README warns about
        for typo'd variable names."""
        repo = self._repo_with_chain()
        try:
            declared = ['010-test.c.patch', '020-test.c.patch']
            with self.assertRaises(PatchApplyError) as cm:
                collect_patch_stacks([(repo.pkgdir, declared)],
                                     repo.tmpdir)
            self.assertIn('030-test.c.patch', str(cm.exception))
            self.assertIn('not declared', str(cm.exception))
        finally:
            repo.__exit__()

    def test_ordinal_tie_broken_by_package_list_order(self):
        """§3: same ordinal in two packages targeting the same rel —
        the earlier package in the CUSTOM_PKG list applies first, and
        reversing the list reverses the order.

        Bug: tie-break by package name / path / hash order instead of
        list position."""
        repo = _TempGitRepo()
        repo.__enter__()
        try:
            repo.write_src(UPSTREAM_3FN)
            edit_alpha = _edit_function(UPSTREAM_3FN,
                                        'return x + 1;',
                                        'return x * 11;')
            edit_gamma = _edit_function(UPSTREAM_3FN,
                                        'return x + 3;',
                                        'return x * 33;')
            pkg_a = 'packages/pkg-a'
            pkg_b = 'packages/pkg-b'
            for pkg, text in ((pkg_a, edit_alpha), (pkg_b, edit_gamma)):
                d = os.path.join(repo.tmpdir, pkg, 'patches')
                os.makedirs(d, exist_ok=True)
                with open(os.path.join(d, '010-test.c.patch'), 'w') as f:
                    f.write(_author_git_patch(UPSTREAM_3FN, text))

            spec_ab = [(pkg_a, ['010-test.c.patch']),
                       (pkg_b, ['010-test.c.patch'])]
            spec_ba = list(reversed(spec_ab))

            stack_ab = collect_patch_stacks(spec_ab, repo.tmpdir)['test.c']
            stack_ba = collect_patch_stacks(spec_ba, repo.tmpdir)['test.c']
            self.assertEqual([p.split(os.sep)[-3] for p in stack_ab],
                             ['pkg-a', 'pkg-b'])
            self.assertEqual([p.split(os.sep)[-3] for p in stack_ba],
                             ['pkg-b', 'pkg-a'])

            # Both orders compose cleanly (different functions) to the
            # same cumulative result.
            dest = os.path.join(repo.tmpdir, 'out', 'test.c')
            final = apply_patch_stack('test.c', stack_ab, repo.tmpdir,
                                      dest)
            self.assertIn('x * 11', final)
            self.assertIn('x * 33', final)
        finally:
            repo.__exit__()


if __name__ == '__main__':
    unittest.main()
