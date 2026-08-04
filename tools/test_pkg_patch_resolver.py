#!/usr/bin/env python3
"""Integration tests for the plain-diff patch-overlay wiring in
tools/resolve_c47_src.py (shadow mode) plus unit tests for the
auto-discovery collection functions in pkg_patch_apply.py.

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

from pkg_patch_apply import PatchApplyError, collect_new_files, collect_patch_stacks
from pkg_patch_refresh import refresh

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_RESOLVER = os.path.join(_TOOLS_DIR, 'resolve_c47_src.py')


def _author_git_patch(base_text, new_text, rel):
    """Author a standalone git-format patch (full index line) taking
    src/c47/<rel> from base_text to new_text, via a throwaway git repo.
    Used where a test needs to hand-construct a patch independent of
    refresh() (e.g. an explicit revert-to-upstream patch)."""
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

UPSTREAM_TEST_C = "int a(void) {\n    return 1;\n}\n\nint b(void) {\n    return 2;\n}\n"
UPSTREAM_OTHER_C = "int other_func(void) {\n    return 5;\n}\n"


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
            f.write(UPSTREAM_TEST_C)
        with open(os.path.join(src, 'other.c'), 'w') as f:
            f.write(UPSTREAM_OTHER_C)
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

    def refresh_pkg(self, pkg, rel, edited_content):
        """Materialize rel with edited_content under pkg, run the real
        refresh() to produce a patch, then delete the materialized
        working copy (only patches/ should remain, matching the
        checked-in convention)."""
        pkg_abs = os.path.join(self.root, pkg)
        mat_path = os.path.join(pkg_abs, *rel.split('/'))
        os.makedirs(os.path.dirname(mat_path), exist_ok=True)
        with open(mat_path, 'w') as f:
            f.write(edited_content)
        result = refresh(pkg, self.root)
        os.remove(mat_path)
        return result

    def add_new_file(self, pkg, rel, content):
        path = os.path.join(self.root, pkg, 'files', *rel.split('/'))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(content)

    def run_resolver(self, pkg_list=()):
        cmd = [sys.executable, _RESOLVER, '--shadow',
               os.path.join(self.root, 'src', 'c47', 'meson.build'),
               self.root, self.shadow] + list(pkg_list)
        return subprocess.run(cmd, capture_output=True, text=True)

    def __exit__(self, *exc):
        shutil.rmtree(self.root, ignore_errors=True)


# ---------------------------------------------------------------------------
# collect_patch_stacks / collect_new_files unit tests (auto-discovery)
# ---------------------------------------------------------------------------

class TestCollectPatchStacks(unittest.TestCase):

    def test_no_declaration_needed_every_patch_on_disk_is_used(self):
        """New Decision 3: unlike the removed declaration scheme, every
        *.patch file found under patches/ is used automatically — there
        is nothing to declare, so nothing can be silently dropped by a
        typo'd declaration."""
        with _MiniProject() as p:
            edited = UPSTREAM_TEST_C.replace('return 1;', 'return 999;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edited)

            stacks = collect_patch_stacks(['packages/pkg-a'], p.root)
            self.assertIn('test.c', stacks)
            self.assertEqual(len(stacks['test.c']), 1)

    def test_nonexistent_upstream_target_is_fatal(self):
        """Unit 1 New Decision 6, first bullet: a patch whose mirrored
        path doesn't correspond to any real upstream file (typo,
        renamed, deleted) is a fatal error naming the patch and path.
        """
        with _MiniProject() as p:
            pkg = 'packages/pkg-bad'
            patches_dir = os.path.join(p.root, pkg, 'patches')
            os.makedirs(patches_dir)
            with open(os.path.join(patches_dir,
                                   '010-nonexistent.c.patch'), 'w') as f:
                f.write('--- a/src/c47/nonexistent.c\n'
                        '+++ b/src/c47/nonexistent.c\n'
                        '@@ -1 +1 @@\n-x\n+y\n')

            with self.assertRaises(PatchApplyError) as cm:
                collect_patch_stacks([pkg], p.root)
            self.assertIn('nonexistent.c', str(cm.exception))


class TestCollectNewFiles(unittest.TestCase):

    def test_new_file_with_no_upstream_counterpart_accepted(self):
        """The straightforward legal case: files/<rel> where rel has no
        upstream counterpart."""
        with _MiniProject() as p:
            p.add_new_file('packages/pkg-a', 'brand_new.c',
                           'int brand_new(void) { return 0; }\n')
            result = collect_new_files(['packages/pkg-a'], p.root)
            self.assertIn('brand_new.c', result)

    def test_files_entry_mirroring_existing_upstream_is_fatal(self):
        """New Decision 6, second bullet: a files/<rel> entry whose
        mirrored path DOES exist upstream is fatal — that change
        belongs under patches/, not files/."""
        with _MiniProject() as p:
            p.add_new_file('packages/pkg-a', 'test.c', 'int x;\n')
            with self.assertRaises(PatchApplyError) as cm:
                collect_new_files(['packages/pkg-a'], p.root)
            msg = str(cm.exception)
            self.assertIn('test.c', msg)
            self.assertIn('patches/', msg)

    def test_two_packages_same_new_file_is_fatal(self):
        """New Decision 6, third bullet: two packages both claiming to
        introduce the SAME new file — no merge concept applies to two
        competing whole files with no common base, so this must be
        caught before either is copied into the shadow tree."""
        with _MiniProject() as p:
            p.add_new_file('packages/pkg-a', 'collide.c', 'int v = 1;\n')
            p.add_new_file('packages/pkg-b', 'collide.c', 'int v = 2;\n')
            with self.assertRaises(PatchApplyError) as cm:
                collect_new_files(['packages/pkg-a', 'packages/pkg-b'],
                                  p.root)
            msg = str(cm.exception)
            self.assertIn('collide.c', msg)
            self.assertIn('pkg-a', msg)
            self.assertIn('pkg-b', msg)


# ---------------------------------------------------------------------------
# Through the real resolver CLI
# ---------------------------------------------------------------------------

class TestSameLineConflictTwoPackages(unittest.TestCase):
    """Unit 6: two packages patching OVERLAPPING lines of the same
    function must fail loudly through the real build path (the actual
    resolver CLI subprocess meson's run_command invokes — not `git
    apply` called in isolation). Self-contained: constructs its own
    fixture packages inside a temp project and tears the whole temp
    project down in __exit__; nothing synthetic is ever committed."""

    def test_same_line_conflict_fails_configure_loudly(self):
        """BUG THIS TEST EXISTS TO CATCH: today's pre-redesign behavior
        (or a regression back to it) would silently pick one package's
        version — mere same-file duplication was only ever a warning.
        Two packages both editing the same line of the same function
        with genuinely divergent content must instead fail configure,
        naming the losing patch, and must leave no marker-bearing file
        for the compiler to see."""
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 100;')
            edit_b = UPSTREAM_TEST_C.replace('return 1;', 'return 999;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            p.refresh_pkg('packages/pkg-b', 'test.c', edit_b)

            r = p.run_resolver(['packages/pkg-a', 'packages/pkg-b'])

            self.assertEqual(r.returncode, 1)
            self.assertIn('pkg-b', r.stderr)

            shadow_file = os.path.join(p.shadow, 'test.c')
            if os.path.isfile(shadow_file) and not os.path.islink(shadow_file):
                with open(shadow_file) as f:
                    self.assertNotIn('<<<<<<<', f.read(),
                                     'no conflict-marker file may reach '
                                     'the compiler')

    def test_different_lines_same_function_still_compose(self):
        """Contrast case guarding against over-broad failure: two
        packages editing DIFFERENT lines of the same function (with at
        least one unchanged line separating them) must still compose
        cleanly — this is the explicitly accepted trade-off from
        'Why revision 2' in PROPOSED_SPEC_CHANGES.md, not a bug."""
        upstream = ("int f(void) {\n"
                    "    int a = 1;\n"
                    "    int b = 2;\n"
                    "    int c = 3;\n"
                    "    return a + b + c;\n"
                    "}\n")
        with _MiniProject() as p:
            with open(os.path.join(p.root, 'src', 'c47', 'test.c'),
                     'w') as f:
                f.write(upstream)
            subprocess.run(['git', 'add', '-A'], cwd=p.root,
                           capture_output=True)
            subprocess.run(['git', 'commit', '-q', '-m', 'retarget'],
                           cwd=p.root, capture_output=True)

            edit_a = upstream.replace('int a = 1;', 'int a = 100;')
            edit_b = upstream.replace('int c = 3;', 'int c = 300;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            p.refresh_pkg('packages/pkg-b', 'test.c', edit_b)

            r = p.run_resolver(['packages/pkg-a', 'packages/pkg-b'])
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(os.path.join(p.shadow, 'test.c')) as f:
                content = f.read()
            self.assertIn('a = 100', content)
            self.assertIn('c = 300', content)


class TestResolverCumulativeApplication(unittest.TestCase):

    def test_two_packages_nonoverlapping_patches_both_apply_in_order(self):
        """Unit 4's specified multi-package test: two packages,
        non-overlapping patches on the SAME file, confirm both apply.
        BUG THIS TEST EXISTS TO CATCH: last-listed-wins regression —
        only one package's edit surviving in the shadow tree."""
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 111;')
            edit_b = UPSTREAM_TEST_C.replace('return 2;', 'return 222;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            p.refresh_pkg('packages/pkg-b', 'test.c', edit_b)

            r = p.run_resolver(['packages/pkg-a', 'packages/pkg-b'])
            self.assertEqual(r.returncode, 0, r.stderr)

            shadow_file = os.path.join(p.shadow, 'test.c')
            self.assertFalse(os.path.islink(shadow_file))
            with open(shadow_file) as f:
                content = f.read()
            self.assertIn('return 111;', content)
            self.assertIn('return 222;', content)


class TestResolverFatalTargetCheck(unittest.TestCase):

    def test_patch_targeting_nonexistent_path_fails_configure(self):
        """Unit 4's specified fatal-configure test, through the real
        resolver CLI (not just the collect_patch_stacks unit test)."""
        with _MiniProject() as p:
            pkg = 'packages/pkg-bad'
            patches_dir = os.path.join(p.root, pkg, 'patches')
            os.makedirs(patches_dir)
            with open(os.path.join(patches_dir,
                                   '010-typo_name.c.patch'), 'w') as f:
                f.write('--- a/src/c47/typo_name.c\n'
                        '+++ b/src/c47/typo_name.c\n'
                        '@@ -1 +1 @@\n-x\n+y\n')

            r = p.run_resolver([pkg])
            self.assertEqual(r.returncode, 1)
            self.assertIn('typo_name.c', r.stderr)


class TestResolverShadowIntegration(unittest.TestCase):

    def test_new_file_copied_and_compiled(self):
        """files/*.c is copied into the shadow tree as a regular file
        and appears in the emitted source list (so it actually gets
        compiled) — confirmed by checking the resolver's own stdout,
        not just the shadow tree's filesystem content."""
        with _MiniProject() as p:
            p.add_new_file('packages/pkg-a', 'brand_new.c',
                           'int brand_new(void) { return 7; }\n')
            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)

            shadow_file = os.path.join(p.shadow, 'brand_new.c')
            self.assertTrue(os.path.isfile(shadow_file))
            with open(shadow_file) as f:
                self.assertIn('return 7;', f.read())
            self.assertIn(shadow_file, r.stdout)

    def test_new_header_copied_but_not_added_to_source_list(self):
        """A files/*.h entry (no .c counterpart) must land in the
        shadow tree (so #include resolves) but must NOT be added to
        the compiled-sources output (headers aren't compiled)."""
        with _MiniProject() as p:
            p.add_new_file('packages/pkg-a', 'brand_new.h',
                           '#define BRAND_NEW 1\n')
            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)

            shadow_file = os.path.join(p.shadow, 'brand_new.h')
            self.assertTrue(os.path.isfile(shadow_file))
            self.assertNotIn(shadow_file, r.stdout)

    def test_real_upstream_file_never_modified(self):
        """BUG THIS TEST EXISTS TO CATCH: writing the patched result
        through the shadow symlink INTO src/c47/ — editing the real
        upstream file, violating the project's core invariant."""
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 111;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(os.path.join(p.root, 'src', 'c47', 'test.c')) as f:
                self.assertEqual(f.read(), UPSTREAM_TEST_C)

    def test_patches_and_files_coexist_on_different_targets(self):
        """A patch on test.c and a new files/ entry from the same
        package must both take effect without interfering."""
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 111;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            p.add_new_file('packages/pkg-a', 'brand_new.c',
                           'int brand_new(void) { return 7; }\n')

            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)

            with open(os.path.join(p.shadow, 'test.c')) as f:
                self.assertIn('return 111;', f.read())
            self.assertTrue(os.path.isfile(
                os.path.join(p.shadow, 'brand_new.c')))

    def test_sentinel_and_untouched_sibling_present(self):
        """Regression guard for the F9/F12 sentinel gate and the
        symlink-mode default: a patched file becomes a regular file,
        an untouched sibling stays a symlink to upstream, and the
        sentinel is present."""
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 111;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)

            self.assertTrue(os.path.isfile(os.path.join(
                p.shadow, 'DO_NOT_EDIT_shadow_tree.txt')))
            self.assertTrue(os.path.islink(
                os.path.join(p.shadow, 'other.c')))

    def test_dead_shadow_warning_on_self_cancelling_stack(self):
        """F15 extension: a two-patch stack that cancels itself out
        (edit then revert) must configure successfully but warn 'dead
        shadow' — silently masking future upstream changes is the
        failure mode this guards against."""
        with _MiniProject() as p:
            pkg = 'packages/pkg-a'
            edited = UPSTREAM_TEST_C.replace('return 1;', 'return 999;')
            p.refresh_pkg(pkg, 'test.c', edited)
            # Second patch reverts back to upstream exactly.
            patches_dir = os.path.join(p.root, pkg, 'patches')
            revert_text = _author_git_patch(edited, UPSTREAM_TEST_C,
                                            rel='test.c')
            with open(os.path.join(patches_dir, '020-test.c.patch'),
                     'w') as f:
                f.write(revert_text)

            r = p.run_resolver([pkg])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('dead shadow', r.stderr)
            with open(os.path.join(p.shadow, 'test.c')) as f:
                self.assertEqual(f.read(), UPSTREAM_TEST_C)


class TestResolverNoLibclangDependency(unittest.TestCase):
    """Revision 2 has no libclang dependency anywhere — this guards the
    resolver's whole import graph, not just one module."""

    def test_resolver_process_never_loads_clang(self):
        """Mutation: import clang (or pkg_patch_extract, which no
        longer exists but the check is future-proof) anywhere reachable
        from resolve_c47_src.py — this fails."""
        audit = (
            'import builtins, runpy, sys\n'
            'real_import = builtins.__import__\n'
            'def guard(name, *a, **k):\n'
            '    if name == "clang" or name.startswith("clang."):\n'
            '        raise AssertionError("forbidden import: " + name)\n'
            '    return real_import(name, *a, **k)\n'
            'builtins.__import__ = guard\n'
            'sys.argv = [sys.argv[1]] + sys.argv[2:]\n'
            'runpy.run_path(sys.argv[0], run_name="__main__")\n'
        )
        with _MiniProject() as p:
            edit_a = UPSTREAM_TEST_C.replace('return 1;', 'return 111;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edit_a)
            r = subprocess.run(
                [sys.executable, '-c', audit, _RESOLVER, '--shadow',
                 os.path.join(p.root, 'src', 'c47', 'meson.build'),
                 p.root, p.shadow, 'packages/pkg-a'],
                capture_output=True, text=True)
            self.assertNotIn('forbidden import', r.stderr)
            self.assertEqual(r.returncode, 0, r.stderr)


class TestSiblingRootShadow(unittest.TestCase):
    """T2-A: a package patch under testSuite/ shadows src/testSuite and
    emits SIBSRC: lines for the sibling target's source list; an
    untouched sibling root costs nothing."""

    @staticmethod
    def _add_sibling(p):
        sib = os.path.join(p.root, 'src', 'testSuite')
        os.makedirs(os.path.join(sib, 'hal'), exist_ok=True)
        with open(os.path.join(sib, 'testSuite.c'), 'w') as f:
            f.write('int suite(void) {\n    return 0;\n}\n')
        with open(os.path.join(sib, 'hal', 'gui.c'), 'w') as f:
            f.write('int gui;\n')
        with open(os.path.join(sib, 'meson.build'), 'w') as f:
            f.write("testSuite_src = files(\n"
                    "  'testSuite.c',\n"
                    "  'hal/gui.c')\n")
        subprocess.run(['git', 'add', '-A'], cwd=p.root,
                       capture_output=True)
        subprocess.run(['git', 'commit', '-q', '-m', 'sibling'],
                       cwd=p.root, capture_output=True)

    def test_sibling_patch_shadows_root_and_emits_sibsrc(self):
        with _MiniProject() as p:
            self._add_sibling(p)
            edited = 'int suite(void) {\n    return 42;\n}\n'
            p.refresh_pkg('packages/pkg-sib', 'testSuite/testSuite.c',
                          edited)
            r = p.run_resolver(['packages/pkg-sib'])
            self.assertEqual(r.returncode, 0, r.stderr)

            # Patched content lands in the shadow under its root prefix.
            with open(os.path.join(p.shadow, 'testSuite',
                                   'testSuite.c')) as f:
                self.assertEqual(f.read(), edited)
            # Untouched sibling files are shadowed too.
            self.assertTrue(os.path.exists(
                os.path.join(p.shadow, 'testSuite', 'hal', 'gui.c')))

            # SIBSRC lines: one per testSuite_src entry, shadow paths.
            sib_lines = [ln for ln in r.stdout.splitlines()
                         if ln.startswith('SIBSRC:testSuite:')]
            sib_paths = sorted(
                ln[len('SIBSRC:testSuite:'):] for ln in sib_lines)
            self.assertEqual(sib_paths, sorted([
                os.path.join(p.shadow, 'testSuite', 'testSuite.c'),
                os.path.join(p.shadow, 'testSuite', 'hal/gui.c'),
            ]))
            # ...and none of them leak into the c47_src lines.
            c47_lines = [ln for ln in r.stdout.splitlines()[1:]
                         if not ln.startswith(('SIBSRC:', 'SIBLIST:',
                                               'GENCAT:', 'GENTST:'))]
            for ln in c47_lines:
                self.assertNotIn('testSuite', ln)

    def test_inactive_sibling_root_costs_nothing(self):
        with _MiniProject() as p:
            self._add_sibling(p)
            edited = UPSTREAM_TEST_C.replace('return 1;', 'return 9;')
            p.refresh_pkg('packages/pkg-a', 'test.c', edited)
            r = p.run_resolver(['packages/pkg-a'])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertNotIn('SIBSRC:', r.stdout)
            self.assertFalse(os.path.exists(
                os.path.join(p.shadow, 'testSuite')))


if __name__ == '__main__':
    unittest.main()
