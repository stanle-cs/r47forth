#!/usr/bin/env python3
"""Integration tests for the patch-overlay wiring in
tools/resolve_c47_src.py (shadow mode) plus unit tests for the §8
mutual-exclusivity check.

Run via: python3 tools/test_pkg_patch_resolver.py
  or:    meson test -C build.sim pkg_patch_resolver

Each test's docstring names the specific bug / mutation it must catch.
The resolver is exercised through its real CLI (subprocess), against a
miniature project tree built in a temp dir.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_apply import PatchApplyError, assert_mutually_exclusive
from test_pkg_patch_apply import _author_git_patch
from test_pkg_patch_refresh import UPSTREAM_3FN, _edit_function

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_RESOLVER = os.path.join(_TOOLS_DIR, 'resolve_c47_src.py')


class TestAssertMutuallyExclusive(unittest.TestCase):
    """Unit tests for the §8 check itself."""

    def test_disjoint_sets_pass(self):
        """Bug: over-broad failure when the two mechanisms target
        different files — must not raise."""
        assert_mutually_exclusive(
            {'keyboard.c': ['packages/a']},
            {'screen.c': ['packages/b']})

    def test_same_rel_fatal_names_both_packages(self):
        """BUG THIS TEST EXISTS TO CATCH: silent pick-one resolution
        when a rel is both overridden and patched. Error must name the
        rel and the packages on both sides.

        Mutation: remove the intersection check — no raise, test fails.
        """
        with self.assertRaises(PatchApplyError) as cm:
            assert_mutually_exclusive(
                {'keyboard.c': ['packages/a']},
                {'keyboard.c': ['packages/b']})
        msg = str(cm.exception)
        self.assertIn('keyboard.c', msg)
        self.assertIn('packages/a', msg)
        self.assertIn('packages/b', msg)
        self.assertIn('§8', msg)

    def test_path_formatting_cannot_dodge_the_check(self):
        """Self-audit-class bug: './ui//tam.c' vs 'ui/tam.c' treated as
        different files, silently allowing both mechanisms."""
        with self.assertRaises(PatchApplyError):
            assert_mutually_exclusive(
                {'ui/tam.c': ['packages/a']},
                {'./ui//tam.c': ['packages/b']})


OTHER_C = """\
int other_func(void) {
    return 5;
}
"""


class _MiniProject:
    """Miniature project tree the real resolver CLI can run against:
      <root>/src/c47/meson.build   (c47_src / c47_inc parseable)
      <root>/src/c47/test.c, other.c   (committed)
      <root>/packages/...          (per test)
      <root>/build/custom_pkg_shadow   (shadow target)
    """

    def __enter__(self):
        self.root = tempfile.mkdtemp()
        src = os.path.join(self.root, 'src', 'c47')
        os.makedirs(src)
        with open(os.path.join(src, 'test.c'), 'w') as f:
            f.write(UPSTREAM_3FN)
        with open(os.path.join(src, 'other.c'), 'w') as f:
            f.write(OTHER_C)
        with open(os.path.join(src, 'meson.build'), 'w') as f:
            f.write("c47_src = files('test.c', 'other.c')\n"
                    "c47_inc = include_directories('.')\n")
        for args in (['init', '-q'], ['config', 'user.email', 'x@x'],
                     ['config', 'user.name', 'x']):
            subprocess.run(['git'] + args, cwd=self.root,
                           capture_output=True)
        subprocess.run(['git', 'add', '-A'], cwd=self.root,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'base'],
                       cwd=self.root, capture_output=True)
        self.shadow = os.path.join(self.root, 'build',
                                   'custom_pkg_shadow')
        return self

    def add_override_pkg(self, pkg='packages/pkg-over', rel='test.c',
                         content=None):
        path = os.path.join(self.root, pkg, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content if content is not None
                    else UPSTREAM_3FN.replace('x + 1', 'x + 111'))
        return f'{pkg}:{rel}'

    def add_patch_pkg(self, pkg='packages/pkg-patch',
                      fname='010-test.c.patch', patch_text=None):
        d = os.path.join(self.root, pkg, 'patches')
        os.makedirs(d, exist_ok=True)
        if patch_text is None:
            patch_text = _author_git_patch(
                UPSTREAM_3FN,
                _edit_function(UPSTREAM_3FN, 'return x + 2;',
                               'return x * 22;'))
        with open(os.path.join(d, fname), 'w') as f:
            f.write(patch_text)
        return f'{pkg}:{fname}'

    def run_resolver(self, specs=(), patch_specs=()):
        cmd = [sys.executable, _RESOLVER, '--shadow',
               os.path.join(self.root, 'src', 'c47', 'meson.build'),
               self.root, self.shadow] + list(specs)
        cmd += ['--patches'] + list(patch_specs)
        return subprocess.run(cmd, capture_output=True, text=True)

    def __exit__(self, *exc):
        shutil.rmtree(self.root, ignore_errors=True)


class TestResolverMutualExclusivity(unittest.TestCase):
    """Unit 7: the fatal configure error through the real resolver CLI."""

    def test_override_plus_patch_same_file_fails_configure(self):
        """BUG THIS TEST EXISTS TO CATCH: configure silently picking one
        mechanism when a file is both whole-file-overridden and
        function-patched. Resolver must exit 1 naming the rel and both
        packages (meson's run_command check:true then fails the
        configure)."""
        with _MiniProject() as p:
            spec = p.add_override_pkg()
            pspec = p.add_patch_pkg()
            r = p.run_resolver([spec], [pspec])
            self.assertEqual(r.returncode, 1)
            self.assertIn('mutual-exclusivity', r.stderr)
            self.assertIn('test.c', r.stderr)
            self.assertIn('pkg-over', r.stderr)
            self.assertIn('pkg-patch', r.stderr)

    def test_exclusivity_check_runs_before_shadow_wipe(self):
        """Bug: check placed after the F9 wipe — a failing configure
        would still have destroyed and half-rebuilt the shadow tree.
        A pre-existing sentinel'd shadow dir with a canary file must
        survive the failed run untouched."""
        with _MiniProject() as p:
            spec = p.add_override_pkg()
            pspec = p.add_patch_pkg()
            os.makedirs(p.shadow)
            with open(os.path.join(p.shadow,
                                   'DO_NOT_EDIT_shadow_tree.txt'),
                      'w') as f:
                f.write('sentinel\n')
            canary = os.path.join(p.shadow, 'canary.txt')
            with open(canary, 'w') as f:
                f.write('still here\n')

            r = p.run_resolver([spec], [pspec])
            self.assertEqual(r.returncode, 1)
            self.assertTrue(os.path.isfile(canary),
                            '§8 failure must precede any shadow-tree '
                            'mutation')

    def test_patch_only_and_override_only_both_pass(self):
        """Bug: over-broad exclusivity rejecting legitimate single-
        mechanism configurations."""
        with _MiniProject() as p:
            spec = p.add_override_pkg()
            r = p.run_resolver([spec], [])
            self.assertEqual(r.returncode, 0, r.stderr)
        with _MiniProject() as p:
            pspec = p.add_patch_pkg()
            r = p.run_resolver([], [pspec])
            self.assertEqual(r.returncode, 0, r.stderr)

    def test_dual_signal_mismatch_fatal_through_resolver(self):
        """§2 end-to-end: filename says test.c but the +++ header says
        screen.c — resolver must exit 1, not fall back to treating the
        patch as targeting either file."""
        with _MiniProject() as p:
            patch_text = _author_git_patch(
                UPSTREAM_3FN,
                _edit_function(UPSTREAM_3FN, 'return x + 2;',
                               'return x * 22;'),
                rel='screen.c')
            pspec = p.add_patch_pkg(fname='010-test.c.patch',
                                    patch_text=patch_text)
            r = p.run_resolver([], [pspec])
            self.assertEqual(r.returncode, 1)
            self.assertIn('mismatch', r.stderr)


class TestShadowTreeIntegration(unittest.TestCase):
    """Unit 8: patch stacks materialized into custom_pkg_shadow/."""

    def test_shadow_contains_patched_regular_file(self):
        """BUG THIS TEST EXISTS TO CATCH: the patched rel left as a
        symlink to pristine upstream (patch silently unapplied), or
        the result not matching hand-application of the patch."""
        expected = _edit_function(UPSTREAM_3FN, 'return x + 2;',
                                  'return x * 22;')
        with _MiniProject() as p:
            pspec = p.add_patch_pkg()
            r = p.run_resolver([], [pspec])
            self.assertEqual(r.returncode, 0, r.stderr)

            shadow_file = os.path.join(p.shadow, 'test.c')
            self.assertTrue(os.path.isfile(shadow_file))
            self.assertFalse(os.path.islink(shadow_file),
                             'patched result must be a real file, '
                             'not a symlink')
            with open(shadow_file) as f:
                self.assertEqual(f.read(), expected)

            # Untouched sibling stays a symlink to upstream
            other = os.path.join(p.shadow, 'other.c')
            self.assertTrue(os.path.islink(other))

            # Sentinel still present (F9/F12 guard untouched)
            self.assertTrue(os.path.isfile(os.path.join(
                p.shadow, 'DO_NOT_EDIT_shadow_tree.txt')))

    def test_real_upstream_file_never_modified(self):
        """BUG THIS TEST EXISTS TO CATCH: writing the patched result
        through the shadow symlink INTO src/c47/ — editing the real
        upstream file, violating the project's core invariant."""
        with _MiniProject() as p:
            pspec = p.add_patch_pkg()
            r = p.run_resolver([], [pspec])
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(os.path.join(p.root, 'src', 'c47', 'test.c')) as f:
                self.assertEqual(f.read(), UPSTREAM_3FN)
            self.assertFalse(
                os.path.islink(os.path.join(p.root, 'src', 'c47',
                                            'test.c')))

    def test_patch_and_override_coexist_on_different_files(self):
        """Bug: patch machinery breaking the existing whole-file
        override path (or vice versa) when both are active on
        DIFFERENT files — the legal §8 configuration."""
        override_content = OTHER_C.replace('return 5;', 'return 55;')
        expected_patched = _edit_function(UPSTREAM_3FN,
                                          'return x + 2;',
                                          'return x * 22;')
        with _MiniProject() as p:
            spec = p.add_override_pkg(rel='other.c',
                                      content=override_content)
            pspec = p.add_patch_pkg()
            r = p.run_resolver([spec], [pspec])
            self.assertEqual(r.returncode, 0, r.stderr)

            with open(os.path.join(p.shadow, 'test.c')) as f:
                self.assertEqual(f.read(), expected_patched)
            with open(os.path.join(p.shadow, 'other.c')) as f:
                self.assertEqual(f.read(), override_content)

    def test_net_identical_stack_warns_dead_shadow(self):
        """F15 extension: a stack that cancels itself out (010 edits,
        020 reverts) must configure successfully but warn 'dead
        shadow' — silent masking of future upstream changes is the
        failure mode."""
        edited = _edit_function(UPSTREAM_3FN, 'return x + 2;',
                                'return x * 22;')
        with _MiniProject() as p:
            pspec1 = p.add_patch_pkg(
                fname='010-test.c.patch',
                patch_text=_author_git_patch(UPSTREAM_3FN, edited))
            p.add_patch_pkg(
                fname='020-test.c.patch',
                patch_text=_author_git_patch(edited, UPSTREAM_3FN))
            pkg = pspec1.split(':', 1)[0]
            r = p.run_resolver(
                [], [f'{pkg}:010-test.c.patch',
                     f'{pkg}:020-test.c.patch'])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('dead shadow', r.stderr)
            with open(os.path.join(p.shadow, 'test.c')) as f:
                self.assertEqual(f.read(), UPSTREAM_3FN)

    def test_same_function_conflict_fails_configure_end_to_end(self):
        """§7 through the REAL configure entry point: two packages
        patch the same function divergently — resolver must exit 1
        naming the conflict; no marker-bearing file may remain in the
        shadow tree."""
        edit_a = _edit_function(UPSTREAM_3FN, 'return x + 2;',
                                'return x * 100;')
        edit_b = _edit_function(UPSTREAM_3FN, 'return x + 2;',
                                'return x - 5;')
        with _MiniProject() as p:
            pa = p.add_patch_pkg(
                pkg='packages/pkg-a',
                patch_text=_author_git_patch(UPSTREAM_3FN, edit_a))
            pb = p.add_patch_pkg(
                pkg='packages/pkg-b',
                patch_text=_author_git_patch(UPSTREAM_3FN, edit_b))
            r = p.run_resolver([], [pa, pb])
            self.assertEqual(r.returncode, 1)
            self.assertIn('pkg-b', r.stderr)

            shadow_file = os.path.join(p.shadow, 'test.c')
            if os.path.isfile(shadow_file) \
                    and not os.path.islink(shadow_file):
                with open(shadow_file) as f:
                    self.assertNotIn('<<<<<<<', f.read(),
                                     'no conflict-marker file may '
                                     'reach the compiler')


class TestResolverLibclangFreedom(unittest.TestCase):
    """§4 (ratified) at the resolver level: the configure-time process
    must have no libclang anywhere in its import graph."""

    def test_resolver_process_never_loads_clang(self):
        """Mutation: import pkg_patch_extract (or clang) anywhere in
        resolve_c47_src.py / pkg_patch_apply / pkg_patch_common — this
        fails. Uses a real resolver run with import auditing."""
        audit = (
            'import builtins, runpy, sys\n'
            'real_import = builtins.__import__\n'
            'def guard(name, *a, **k):\n'
            '    if name == "clang" or name.startswith("clang.") '
            'or name == "pkg_patch_extract":\n'
            '        raise AssertionError("forbidden import: " + name)\n'
            '    return real_import(name, *a, **k)\n'
            'builtins.__import__ = guard\n'
            'sys.argv = [sys.argv[1]] + sys.argv[2:]\n'
            'runpy.run_path(sys.argv[0], run_name="__main__")\n'
        )
        with _MiniProject() as p:
            pspec = p.add_patch_pkg()
            r = subprocess.run(
                [sys.executable, '-c', audit, _RESOLVER, '--shadow',
                 os.path.join(p.root, 'src', 'c47', 'meson.build'),
                 p.root, p.shadow, '--patches', pspec],
                capture_output=True, text=True)
            self.assertNotIn('forbidden import', r.stderr)
            self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == '__main__':
    unittest.main()
