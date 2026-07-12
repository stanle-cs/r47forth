#!/usr/bin/env python3
"""Unit tests for tools/pkg_patch_extract.py.

Run via: python3 tools/test_pkg_patch_extract.py
  or:    meson test -C build.sim pkg_patch_extract

Each test's docstring names the specific bug / mutation it must catch.

Fixture .c files are checked in under tools/test_fixtures/pkg_patch_extract/;
each test stages them into a temp dir and generates compile_commands.json
there at run time, so no absolute paths are baked into the repository.
"""
import json
import os
import shutil
import sys
import tempfile
import unittest

# Ensure the tools directory is on sys.path so pkg_patch_extract is importable
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pkg_patch_extract import (
    list_function_ranges,
    function_at_line,
)

# Resolve paths
_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
_FIXTURES_DIR = os.path.join(_TOOLS_DIR, 'test_fixtures', 'pkg_patch_extract')
_REPO_ROOT = os.path.abspath(os.path.join(_TOOLS_DIR, '..'))


class _StagedFixture:
    """Copy fixture files into a temp dir and write a real
    compile_commands.json for them, so tests never depend on absolute
    paths checked into the repo."""

    def __init__(self, subdir, main_file, extra_files=(), flags=()):
        self.subdir = subdir
        self.main_file = main_file
        self.extra_files = extra_files
        self.flags = list(flags)
        self.tmpdir = None
        self.main_path = None
        self.cc_path = None

    def __enter__(self):
        self.tmpdir = tempfile.mkdtemp()
        src_dir = os.path.join(_FIXTURES_DIR, self.subdir)
        for fname in (self.main_file,) + tuple(self.extra_files):
            shutil.copy(os.path.join(src_dir, fname),
                        os.path.join(self.tmpdir, fname))
        self.main_path = os.path.join(self.tmpdir, self.main_file)
        self.cc_path = os.path.join(self.tmpdir, 'compile_commands.json')
        cmd = 'cc ' + ' '.join(self.flags + ['-I.']) + \
              f' -c {self.main_file} -o {self.main_file}.o'
        with open(self.cc_path, 'w') as f:
            json.dump([{
                'directory': self.tmpdir,
                'command': cmd,
                'file': self.main_file,
            }], f)
        return self

    def __exit__(self, *exc):
        if self.tmpdir:
            shutil.rmtree(self.tmpdir, ignore_errors=True)


class TestBracesInStringLiteral(unittest.TestCase):
    """Tests for brace-in-string handling.

    BUG THIS TEST EXISTS TO CATCH: a naive brace-counter would stop at
    the first '}' inside a string or char literal, producing a shorter
    end_line than the function's actual closing brace.  libclang's AST
    extent correctly identifies the real closing brace.
    """

    def test_brace_in_string_literal_end_line(self):
        """Bug: brace inside a string literal causes premature end_line.

        In brace_test.c, func_with_braces_in_string contains:
          const char *s1 = "hello { world }";
          char c = '}';
        A naive brace counter would stop at the '}' in char c = '}';
        (line 8), but libclang correctly reports end_line = 10
        (the actual closing brace of the function).
        """
        with _StagedFixture('brace_in_string', 'brace_test.c') as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        names = {name for name, _, _ in ranges}
        self.assertIn('func_with_braces_in_string', names)
        self.assertIn('simple_func', names)

        range_map = {name: (start, end) for name, start, end in ranges}

        # func_with_braces_in_string: real end is line 10 (closing brace)
        # A naive brace counter would say line 8 (the '}' in char c = '}';)
        self.assertEqual(range_map['func_with_braces_in_string'], (5, 10))

        # simple_func starts at line 12, confirming func_with_braces_in_string
        # ends at line 10 (not bleeding into simple_func)
        self.assertEqual(range_map['simple_func'][0], 12)

    def test_function_at_line_with_braces_in_string(self):
        """Bug: function_at_line returns wrong function when braces in string
        cause the range to be truncated.  Line 7 (inside the string-literal
        braces) should still be attributed to func_with_braces_in_string.
        """
        with _StagedFixture('brace_in_string', 'brace_test.c') as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        # Line 7 is inside func_with_braces_in_string (string literal braces)
        result = function_at_line(ranges, 7)
        self.assertIsNotNone(result)
        self.assertEqual(result[0], 'func_with_braces_in_string')

        # Line 8 (char c = '}';) is still inside the function
        result = function_at_line(ranges, 8)
        self.assertIsNotNone(result)
        self.assertEqual(result[0], 'func_with_braces_in_string')

        # Line 11 is between functions — should return None
        result = function_at_line(ranges, 11)
        self.assertIsNone(result)


class TestIfdefGuardedFunction(unittest.TestCase):
    """Tests for #ifdef-guarded function definitions.

    BUG THIS TEST EXISTS TO CATCH: a scanner that ignores the
    preprocessor would see TWO definitions of guarded_function (the
    #ifdef and #else branches) and could pick the wrong one, or
    miscount braces across the inactive branch.  libclang parses with
    the real compile flags (-DFEATURE_ON from compile_commands.json),
    so only the active branch exists in the AST.
    """

    def test_only_active_branch_definition_reported(self):
        """Bug: inactive #else-branch definition (lines 10-12) leaks into
        the range list, or shadows the active one (lines 6-8).

        Mutation: parsing without the -DFEATURE_ON define (e.g. guessing
        flags instead of using compile_commands.json) flips which branch
        is active — the range would become (10, 12) and this fails.
        """
        with _StagedFixture('ifdef_guard', 'ifdef_test.c',
                            flags=['-DFEATURE_ON']) as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        guarded = [(s, e) for n, s, e in ranges if n == 'guarded_function']
        self.assertEqual(guarded, [(6, 8)],
                         'exactly one guarded_function definition, the '
                         'active #ifdef FEATURE_ON branch at lines 6-8')

    def test_ifdef_inside_body_spans_both_branches(self):
        """Bug: an #ifdef *inside* a function body truncates the extent.
        after_func (lines 15-21) contains an #ifdef/#else/#endif; its
        extent must span the whole body regardless of which branch is
        active.
        """
        with _StagedFixture('ifdef_guard', 'ifdef_test.c',
                            flags=['-DFEATURE_ON']) as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        range_map = {name: (start, end) for name, start, end in ranges}
        self.assertEqual(range_map['after_func'], (15, 21))


class TestMacroExpandedBrace(unittest.TestCase):
    """Tests for macro-expanded braces.

    BUG THIS TEST EXISTS TO CATCH: macro_brace_func's opening and
    closing braces exist only after macro expansion (BEGIN_BODY /
    END_BODY); a raw-token brace scanner sees no literal '{' or '}' on
    those lines at all and cannot find the function's extent.  libclang
    reports the extent of the parsed expansion.
    """

    def test_macro_expanded_body_extent(self):
        """Bug: function whose braces come from macros gets no range or a
        truncated range.  macro_brace_func must span lines 10-14
        (definition line through the END_BODY line), and following_func
        must be separate at lines 16-18.
        """
        with _StagedFixture('macro_brace', 'macro_test.c') as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        range_map = {name: (start, end) for name, start, end in ranges}
        self.assertEqual(range_map['macro_brace_func'], (10, 14))
        self.assertEqual(range_map['following_func'], (16, 18))

        # Line 12 (EMPTY_STMT_BLOCK inside the body) attributes to
        # macro_brace_func, line 15 (between functions) to nothing.
        self.assertEqual(function_at_line(ranges, 12)[0], 'macro_brace_func')
        self.assertIsNone(function_at_line(ranges, 15))


class TestFileIdentityCheck(unittest.TestCase):
    """Tests for the file-identity check in list_function_ranges.

    BUG THIS TEST EXISTS TO CATCH: without the file-identity check
    (os.path.realpath(cursor.location.file.name) == os.path.realpath(file_path)),
    libclang's AST walk will include functions from included headers.
    When parsing main.c (which includes header_func.h), header_function
    from header_func.h must NOT appear in the results.
    """

    def test_header_functions_excluded(self):
        """Bug: without file-identity check, functions from included headers
        leak into the caller's function list.  main.c includes header_func.h
        which defines header_function().  Only main_function should appear.

        Mutation: removing the os.path.realpath(cursor.location.file.name) ==
        os.path.realpath(file_path) filter causes header_function to appear
        in the results, making this test fail.
        """
        with _StagedFixture('header_identity', 'main.c',
                            extra_files=('header_func.h',)) as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        names = {name for name, _, _ in ranges}
        self.assertIn('main_function', names,
                      'main_function should be present (defined in main.c)')
        self.assertNotIn('header_function', names,
                         'header_function should NOT be present '
                         '(defined in header_func.h, not main.c)')

    def test_function_at_line_global_scope(self):
        """Bug: function_at_line should return None for lines outside any
        function (file-scope code, includes, etc.).
        """
        with _StagedFixture('header_identity', 'main.c',
                            extra_files=('header_func.h',)) as fx:
            ranges = list_function_ranges(fx.main_path, fx.cc_path)

        # Line 1 is the #include directive — not inside any function
        result = function_at_line(ranges, 1)
        self.assertIsNone(result)

        # Line 5 is inside main_function
        result = function_at_line(ranges, 5)
        self.assertIsNotNone(result)
        self.assertEqual(result[0], 'main_function')


class TestRealUpstreamFile(unittest.TestCase):
    """Tests against real src/c47/ files using the vanilla build's
    compile_commands.json.  These verify the tool works on actual
    upstream code with full compiler flags.
    """

    def test_saveRestorePrograms_fnPExport(self):
        """Bug: fnPExport in saveRestorePrograms.c contains braces inside
        string literals (sprintf format strings like "0000: { Prgm #..."
        and RTF content "{\\rtf1\\ansi...").  libclang must correctly
        identify the function extent despite these embedded braces.

        This is a real-file test against the actual upstream codebase,
        not a synthetic fixture.  Requires a vanilla build (no CUSTOM_PKG)
        so that compile_commands.json contains src/c47/ paths.
        """
        cc_path = os.path.join(_REPO_ROOT, 'build.sim', 'compile_commands.json')
        file_path = os.path.join(
            _REPO_ROOT, 'src/c47', 'saveRestorePrograms.c')

        if not os.path.isfile(cc_path):
            self.skipTest(
                'build.sim/compile_commands.json not found — '
                'configure a vanilla build first (no CUSTOM_PKG)')

        # Check that the entry exists for src/c47/ path (vanilla build)
        # If build.sim was configured with CUSTOM_PKG, the entry will be
        # under custom_pkg_shadow/ instead, and we should skip.
        with open(cc_path, 'r') as f:
            entries = json.load(f)
        file_real = os.path.realpath(file_path)
        has_entry = False
        for entry in entries:
            entry_dir = entry.get('directory', '.')
            entry_file = os.path.join(entry_dir, entry['file'])
            if os.path.realpath(entry_file) == file_real:
                has_entry = True
                break
        if not has_entry:
            self.skipTest(
                'compile_commands.json has no entry for '
                'src/c47/saveRestorePrograms.c — '
                'build.sim may be configured with CUSTOM_PKG. '
                'Reconfigure with CUSTOM_PKG unset for this test.')

        ranges = list_function_ranges(file_path, cc_path)
        names = {name for name, _, _ in ranges}

        self.assertIn('fnPExport', names)

        # fnPExport contains braces in strings at line 181:
        #   sprintf(tmpString, "0000: { Prgm #%" ...
        # The function should span 162-271
        fn_range = None
        for name, start, end in ranges:
            if name == 'fnPExport':
                fn_range = (name, start, end)
                break

        self.assertIsNotNone(fn_range)
        self.assertEqual(fn_range[1], 162)
        self.assertEqual(fn_range[2], 271)

        # Line 181 (brace-in-string) should be inside fnPExport
        result = function_at_line(ranges, 181)
        self.assertIsNotNone(result)
        self.assertEqual(result[0], 'fnPExport')

    def test_missing_compile_commands_raises(self):
        """Bug: when compile_commands.json doesn't exist, the tool must
        raise FileNotFoundError with a clear message, not crash with
        an obscure traceback.
        """
        file_path = os.path.join(
            _FIXTURES_DIR, 'brace_in_string', 'brace_test.c')

        with self.assertRaises(FileNotFoundError) as cm:
            list_function_ranges(file_path, '/nonexistent/compile_commands.json')

        err = str(cm.exception)
        self.assertIn('compile_commands.json not found', err)
        self.assertIn('vanilla build', err)

    def test_no_matching_entry_raises(self):
        """Bug: when the target file has no entry in compile_commands.json,
        the tool must raise FileNotFoundError, not silently return empty
        results or crash.
        """
        with _StagedFixture('brace_in_string', 'brace_test.c') as fx:
            with self.assertRaises(FileNotFoundError) as cm:
                list_function_ranges('/nonexistent/file.c', fx.cc_path)

        err = str(cm.exception)
        self.assertIn('no compile_commands.json entry', err)


if __name__ == '__main__':
    unittest.main()
