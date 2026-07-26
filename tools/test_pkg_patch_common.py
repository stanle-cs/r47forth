#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_common.py.

Run via: python3 -m pytest tools/test_pkg_patch_common.py
  or:    meson test -C build.sim (when wired up)

Each test's docstring names the specific bug / mutation it must catch.
"""
import os
import sys
import tempfile
import unittest

# Ensure the tools directory is on sys.path so pkg_patch_common is importable
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_common import (
    decode_patch_filename,
    parse_patch_target,
    validate_patch_declaration,
)

# Resolve the repo root (two levels up from tools/)
REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


class TestDecodePatchFilename(unittest.TestCase):
    """Tests for decode_patch_filename — filename encoding/decoding."""

    def test_decode_patch_filename_roundtrip(self):
        """Bug: nested path encoding/decoding mismatch.
        A path like ui/tam.c encodes to ui__tam.c in the filename.
        The decode must recover the original path with /.
        """
        ordinal, rel = decode_patch_filename('010-ui__tam.c.patch')
        self.assertEqual(ordinal, 10)
        self.assertEqual(rel, 'ui/tam.c')

    def test_decode_simple_filename(self):
        """Bug: top-level path (no slash) should work correctly."""
        ordinal, rel = decode_patch_filename('020-keyboard.c.patch')
        self.assertEqual(ordinal, 20)
        self.assertEqual(rel, 'keyboard.c')

    def test_decode_patch_filename_rejects_missing_ordinal(self):
        """Bug: filename without NNN- prefix should be rejected."""
        with self.assertRaises(ValueError):
            decode_patch_filename('keyboard.c.patch')

    def test_decode_patch_filename_rejects_missing_patch_suffix(self):
        """Bug: filename without .patch suffix should be rejected."""
        with self.assertRaises(ValueError):
            decode_patch_filename('010-keyboard.c')

    def test_decode_patch_filename_rejects_empty_rel(self):
        """Bug: filename with empty rel after ordinal should be rejected."""
        with self.assertRaises(ValueError):
            decode_patch_filename('010-.patch')

    def test_decode_rejects_path_traversal_and_absolute(self):
        """SELF-AUDIT FINDING: '010-..__evil.c.patch' decoded to
        '../evil.c' and '010-__etc__passwd.patch' to '/etc/passwd' —
        and os.path.join DISCARDS the src/c47 prefix for an absolute
        rel, so validation would probe the raw filesystem path.
        Decode must reject '..'/'.'/empty segments and absolute rels
        before any path is built.

        Mutation: drop the segment check in decode_patch_filename —
        these decode 'successfully' and this test fails."""
        for fname in ('010-..__evil.c.patch',
                      '010-__etc__passwd.patch',
                      '010-ui__..__..__evil.c.patch',
                      '010-.__test.c.patch',
                      '010-ui____tam.c.patch'):
            with self.assertRaises(ValueError, msg=fname):
                decode_patch_filename(fname)


class TestParsePatchTarget(unittest.TestCase):
    """Tests for parse_patch_target — +++ header extraction."""

    def test_parse_simple_header(self):
        """Bug: basic +++ b/ header extraction."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/keyboard.c\n')
            f.write('+++ b/keyboard.c\n')
            f.write('@@ -1 +1 @@\n')
            f.write('- old\n')
            f.write('+ new\n')
            fname = f.name
        try:
            target = parse_patch_target(fname)
            self.assertEqual(target, 'keyboard.c')
        finally:
            os.unlink(fname)

    def test_parse_nested_header(self):
        """Bug: nested path in +++ header."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/programming/manage.c\n')
            f.write('+++ b/programming/manage.c\n')
            f.write('@@ -1 +1 @@\n')
            fname = f.name
        try:
            target = parse_patch_target(fname)
            self.assertEqual(target, 'programming/manage.c')
        finally:
            os.unlink(fname)

    def test_parse_strips_src_c47_prefix(self):
        """Bug: +++ b/src/c47/... prefix must be stripped."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/src/c47/keyboard.c\n')
            f.write('+++ b/src/c47/keyboard.c\n')
            f.write('@@ -1 +1 @@\n')
            fname = f.name
        try:
            target = parse_patch_target(fname)
            self.assertEqual(target, 'keyboard.c')
        finally:
            os.unlink(fname)

    def test_parse_handles_tab_timestamp_header(self):
        """SELF-AUDIT FINDING: a standard unified-diff header with a
        tab-separated timestamp ('+++ b/test.c\\t2026-07-12 ...')
        parsed to 'test.c\\t2026-07-12 ...' — a valid patch would be
        rejected as a target mismatch. The tab-suffix must be
        stripped per the unified-diff format."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/test.c\t2026-07-12 10:00:00\n')
            f.write('+++ b/test.c\t2026-07-12 10:00:01\n')
            f.write('@@ -1 +1 @@\n-x\n+y\n')
            fname = f.name
        try:
            self.assertEqual(parse_patch_target(fname), 'test.c')
        finally:
            os.unlink(fname)

    def test_parse_rejects_multi_file_patch(self):
        """SELF-AUDIT FINDING: a patch with TWO diff sections validated
        against its first +++ header only; a second section creating a
        new file would apply in the scratch repo and then be silently
        discarded (part of the package's edit dropped from the build).
        Exactly one +++ header per patch, or ValueError."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/test.c\n+++ b/test.c\n@@ -1 +1 @@\n-x\n+y\n')
            f.write('--- /dev/null\n+++ b/sneaky.c\n@@ -0,0 +1 @@\n+int s;\n')
            fname = f.name
        try:
            with self.assertRaises(ValueError) as cm:
                parse_patch_target(fname)
            self.assertIn('exactly one', str(cm.exception))
        finally:
            os.unlink(fname)

    def test_parse_no_header_raises(self):
        """Bug: patch file with no +++ b/ line must raise ValueError."""
        with tempfile.NamedTemporaryFile(mode='w', suffix='.patch',
                                         delete=False) as f:
            f.write('--- a/keyboard.c\n')
            f.write('@@ -1 +1 @@\n')
            fname = f.name
        try:
            with self.assertRaises(ValueError):
                parse_patch_target(fname)
        finally:
            os.unlink(fname)


class TestNoLibclangDependency(unittest.TestCase):
    """Revision 2 (plain-diff design) has no libclang dependency
    anywhere, build-time or authoring-time — there is no C-source
    parsing at all, only whole-file `git diff`. This guards
    pkg_patch_common specifically, since it's the module shared with
    the build-time resolver: every meson/ninja configure would depend
    on libclang if this ever regressed.

    BUG THIS TEST EXISTS TO CATCH: someone adds an import (direct or
    transitive) of clang/clang.cindex to pkg_patch_common.
    """

    def test_pkg_patch_common_never_imports_clang(self):
        """Mutation: add 'import clang.cindex' to pkg_patch_common.py
        — this test fails. Checked in a fresh interpreter so this test
        file's own imports cannot mask or pollute the result.
        """
        import subprocess
        code = (
            'import sys; sys.path.insert(0, sys.argv[1]); '
            'import pkg_patch_common; '
            'bad = [m for m in sys.modules if m == "clang" '
            'or m.startswith("clang.")]; '
            'sys.exit(1 if bad else 0)'
        )
        result = subprocess.run(
            [sys.executable, '-c', code,
             os.path.dirname(os.path.abspath(__file__))],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         'importing pkg_patch_common must not pull in '
                         'clang/clang.cindex '
                         '(build-time path must stay libclang-free)')


class TestValidatePatchDeclaration(unittest.TestCase):
    """Tests for validate_patch_declaration — cross-check of two signals.

    This is the core validation that prevents a typo'd patch filename
    from being silently accepted.
    """

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_validate_patch_declaration_agrees(self):
        """Bug: when filename and header agree on a real upstream file,
        validate must succeed and return the rel path.
        """
        pkgdir = os.path.join(REPO_ROOT, 'test-pkg-agrees')
        os.makedirs(os.path.join(pkgdir, 'patches'), exist_ok=True)
        self._create_patch_at(pkgdir, '010-keyboard.c.patch',
                              'keyboard.c')
        try:
            result = validate_patch_declaration(
                'test-pkg-agrees', '010-keyboard.c.patch', REPO_ROOT)
            self.assertEqual(result, 'keyboard.c')
        finally:
            import shutil
            shutil.rmtree(pkgdir, ignore_errors=True)

    def _create_patch_at(self, pkgdir, filename, header_target):
        """Create a minimal patch file at the given pkgdir."""
        pkg_patches = os.path.join(pkgdir, 'patches')
        os.makedirs(pkg_patches, exist_ok=True)
        patch_path = os.path.join(pkg_patches, filename)
        with open(patch_path, 'w') as f:
            f.write(f'--- a/{header_target}\n')
            f.write(f'+++ b/{header_target}\n')
            f.write('@@ -1 +1 @@\n')
            f.write('- old\n')
            f.write('+ new\n')
        return patch_path

    def test_validate_patch_declaration_catches_mismatch(self):
        """BUG THIS TEST EXISTS TO CATCH: a patch file whose filename says
        010-keyboard.c.patch but whose +++ b/ header says screen.c.
        Without the cross-check, this mismatch would be silently accepted.
        The validate_patch_declaration function MUST raise ValueError.

        Mutation: if the header-vs-filename comparison is removed/commented
        out, this test will silently pass (accept the mismatch).
        """
        pkgdir = os.path.join(REPO_ROOT, 'test-pkg-mismatch')
        os.makedirs(os.path.join(pkgdir, 'patches'), exist_ok=True)
        # Filename says keyboard.c, but header says screen.c
        patch_path = os.path.join(pkgdir, 'patches', '010-keyboard.c.patch')
        with open(patch_path, 'w') as f:
            f.write('--- a/screen.c\n')
            f.write('+++ b/screen.c\n')
            f.write('@@ -1 +1 @@\n')
            f.write('- old\n')
            f.write('+ new\n')
        try:
            with self.assertRaises(ValueError) as cm:
                validate_patch_declaration(
                    'test-pkg-mismatch', '010-keyboard.c.patch', REPO_ROOT)
            err = str(cm.exception)
            self.assertIn('keyboard.c', err)
            self.assertIn('screen.c', err)
        finally:
            import shutil
            shutil.rmtree(pkgdir, ignore_errors=True)

    def test_validate_patch_declaration_catches_no_upstream_match(self):
        """Bug: both signals agree on a rel that doesn't exist under
        src/c47/ -> raises ValueError, does NOT silently fall through
        to 'treat as new file'.
        """
        pkgdir = os.path.join(REPO_ROOT, 'test-pkg-noexist')
        os.makedirs(os.path.join(pkgdir, 'patches'), exist_ok=True)
        self._create_patch_at(pkgdir, '010-nonexistent_file.c.patch',
                              'nonexistent_file.c')
        try:
            with self.assertRaises(ValueError) as cm:
                validate_patch_declaration(
                    'test-pkg-noexist', '010-nonexistent_file.c.patch',
                    REPO_ROOT)
            err = str(cm.exception)
            self.assertIn('nonexistent_file.c', err)
        finally:
            import shutil
            shutil.rmtree(pkgdir, ignore_errors=True)


if __name__ == '__main__':
    unittest.main()
