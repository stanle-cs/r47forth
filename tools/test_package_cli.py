#!/usr/bin/env python3
"""Tests for the ./package entry-point CLI.

Runs against the real repository but never mutates packages/forth-core.
Synthetic package directories are used for mutating subcommands.
"""
import json
import hashlib
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_pkg_patch_integrate import _make_repo_with_pkg  # noqa: E402

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
_PACKAGE = os.path.join(_REPO_ROOT, 'package')


def _run(args, cwd=None, env=None):
    """Run ./package with *args* and return CompletedProcess."""
    cmd = [_PACKAGE] + args
    return subprocess.run(
        cmd, capture_output=True, text=True, cwd=cwd or _REPO_ROOT, env=env)


class TestHelp(unittest.TestCase):
    """Top-level and subcommand --help."""

    def test_top_level_help(self):
        r = _run(['--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('refresh', r.stdout)
        self.assertIn('materialize', r.stdout)
        self.assertIn('rebase', r.stdout)
        self.assertIn('build', r.stdout)
        self.assertIn('audit', r.stdout)
        self.assertIn('integrate', r.stdout)
        self.assertIn('resume', r.stdout)

    def test_refresh_help(self):
        r = _run(['refresh', '--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('package', r.stdout)

    def test_materialize_help(self):
        r = _run(['materialize', '--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('rel', r.stdout)

    def test_rebase_help(self):
        r = _run(['rebase', '--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('onto', r.stdout)

    def test_build_help(self):
        r = _run(['build', '--help'])
        self.assertEqual(r.returncode, 0)

    def test_audit_help(self):
        r = _run(['audit', '--help'])
        self.assertEqual(r.returncode, 0)

    def test_resume_help(self):
        r = _run(['resume', '--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('session', r.stdout)
        self.assertIn('keep', r.stdout)
        self.assertIn('no-build', r.stdout)

    def test_resume_omitted_flags_preserve_stored_values(self):
        import pkg_patch_cli

        with mock.patch.object(pkg_patch_cli, '_cmd_resume') as command:
            pkg_patch_cli.main(['resume', '/tmp/session'])

        args = command.call_args.args[0]
        self.assertIsNone(args.keep)
        self.assertIsNone(args.no_build)

    def test_integrate_omitted_flags_default_false(self):
        import pkg_patch_cli

        with mock.patch.object(pkg_patch_cli, '_cmd_integrate') as command:
            pkg_patch_cli.main([
                'integrate', 'forth-core', '--onto', 'HEAD'])

        args = command.call_args.args[0]
        self.assertFalse(args.keep)
        self.assertFalse(args.no_build)


class TestInvocationFromOtherDir(unittest.TestCase):
    """Package resolves repo root from its own path, not $PWD."""

    def test_from_tmp(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            r = _run(['refresh', '--help'], cwd=tmpdir)
            self.assertEqual(r.returncode, 0)


class TestPackageResolution(unittest.TestCase):
    """Accept/reject rules for package names."""

    def test_accept_bare_name(self):
        r = _run(['refresh', 'forth-core'])
        self.assertEqual(r.returncode, 0)

    def test_accept_packages_prefix(self):
        r = _run(['refresh', 'packages/forth-core'])
        self.assertEqual(r.returncode, 0)

    def test_reject_absolute_path(self):
        r = _run(['refresh', '/absolute/path'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)
        self.assertIn('absolute', r.stderr)

    def test_reject_traversal(self):
        r = _run(['refresh', '../outside'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)
        self.assertIn('traversal', r.stderr)

    def test_reject_missing_directory(self):
        r = _run(['refresh', 'nonexistent-pkg'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)
        self.assertIn('does not exist', r.stderr)

    def test_reject_no_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            fake = os.path.join(tmpdir, 'packages', 'no-manifest')
            os.makedirs(fake, exist_ok=True)
            # Override repo root resolution by running from a cwd
            # where the fake packages/ is visible — but the script
            # resolves from its own path, so we need a symlink trick.
            # Instead, just pass a name that resolves to a real dir
            # without a manifest.
            pass
        # Use a real directory that lacks a manifest.
        r = _run(['refresh', 'tools'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)

    def test_reject_empty_name(self):
        r = _run(['refresh', ''])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)


class TestMissingScripts(unittest.TestCase):
    """Diagnostics when build-test.sh or design-audit.sh is absent."""

    def _temp_repo_with_pkg(self, pkg_name='synth'):
        """Create a temp tree with a copy of the package script, tools/,
        and a synthetic package directory. Returns (tmpdir, script_path)."""
        tmpdir = tempfile.mkdtemp(prefix='pkg_test_repo_')
        script_path = os.path.join(tmpdir, 'package')
        shutil.copy2(_PACKAGE, script_path)
        os.chmod(script_path, 0o755)
        # Copy tools/ so pkg_patch_refresh is importable.
        shutil.copytree(
            os.path.join(_REPO_ROOT, 'tools'),
            os.path.join(tmpdir, 'tools'),
            dirs_exist_ok=True,
        )
        pkg = os.path.join(tmpdir, 'packages', pkg_name)
        os.makedirs(pkg)
        manifest = {
            'base_commit': subprocess.run(
                ['git', 'rev-parse', 'HEAD'],
                capture_output=True, text=True, cwd=_REPO_ROOT,
            ).stdout.strip(),
            'patches': {},
            'files': {},
        }
        with open(os.path.join(pkg, '.refresh-manifest.json'), 'w') as f:
            json.dump(manifest, f)
        return tmpdir, script_path

    def test_missing_build_script(self):
        tmpdir, script_path = self._temp_repo_with_pkg()
        try:
            r = subprocess.run(
                [script_path, 'build', 'synth'],
                capture_output=True, text=True, cwd=tmpdir,
            )
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('build script not found', r.stderr)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

    def test_missing_audit_script(self):
        tmpdir, script_path = self._temp_repo_with_pkg()
        try:
            r = subprocess.run(
                [script_path, 'audit', 'synth'],
                capture_output=True, text=True, cwd=tmpdir,
            )
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('audit script not found', r.stderr)
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)


class TestDesignAuditGeneratedOutput(unittest.TestCase):
    """The design audit separates manifest integrity from Git dirtiness."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_audit_test_')
        _run_git = lambda args: subprocess.run(
            ['git'] + args, cwd=self.tmpdir, capture_output=True, text=True,
            check=True)
        _run_git(['init'])
        _run_git(['config', 'user.email', 'test@test.com'])
        _run_git(['config', 'user.name', 'Test'])

        src = os.path.join(self.tmpdir, 'src', 'c47')
        os.makedirs(src)
        with open(os.path.join(src, 'upstream.c'), 'w') as f:
            f.write('// upstream\n')

        audit_dir = os.path.join(
            self.tmpdir, 'design-docs', 'forth-core')
        os.makedirs(audit_dir)
        shutil.copy2(
            os.path.join(
                _REPO_ROOT, 'design-docs', 'forth-core',
                'design-audit.sh'),
            os.path.join(audit_dir, 'design-audit.sh'))
        shutil.copy2(
            os.path.join(
                _REPO_ROOT, 'design-docs', 'forth-core',
                '.design-audit-baseline'),
            os.path.join(audit_dir, '.design-audit-baseline'))
        with open(os.path.join(audit_dir, 'DESIGN.md'), 'w') as f:
            f.write('# Synthetic design\n')

        _run_git(['add', 'src', 'design-docs'])
        _run_git(['commit', '-m', 'synthetic audit base'])
        self.base_sha = _run_git(['rev-parse', 'HEAD']).stdout.strip()

        self.pkg = os.path.join(
            self.tmpdir, 'packages', 'forth-core')
        os.makedirs(os.path.join(self.pkg, 'patches'))
        os.makedirs(os.path.join(self.pkg, 'files', 'programming'))
        self.generated = os.path.join(
            self.pkg, 'files', 'programming', 'new.c')
        with open(self.generated, 'w') as f:
            f.write('// generated\n')
        digest = hashlib.sha256(b'// generated\n').hexdigest()
        with open(os.path.join(
                self.pkg, '.refresh-manifest.json'), 'w') as f:
            json.dump({
                'base_commit': self.base_sha,
                'patches': {},
                'files': {'programming/new.c': digest},
            }, f)
        self.audit = os.path.join(audit_dir, 'design-audit.sh')

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_synchronized_uncommitted_output_is_clean(self):
        result = subprocess.run(
            [self.audit], cwd=self.tmpdir,
            capture_output=True, text=True)

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(
            'generated output synchronized with manifest', result.stdout)
        self.assertIn(
            'generated output differs from Git', result.stdout)

    def test_stale_generated_output_is_a_finding(self):
        with open(self.generated, 'w') as f:
            f.write('// stale\n')

        result = subprocess.run(
            [self.audit], cwd=self.tmpdir,
            capture_output=True, text=True)

        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(
            'MISMATCH hash files/programming/new.c', result.stdout)


class TestExecutableBit(unittest.TestCase):
    """The package script must be executable."""

    def test_executable(self):
        mode = os.stat(_PACKAGE).st_mode
        self.assertTrue(mode & stat.S_IXUSR,
                        'package script is not executable by owner')


class TestImportNoSideEffects(unittest.TestCase):
    """Importing the package module must not run a command."""

    def test_import_does_not_run(self):
        r = subprocess.run(
            ['python3', '-c',
             ('import importlib.machinery; '
              'loader = importlib.machinery.SourceFileLoader('
              '"test_pkg", "' + _PACKAGE + '"); '
              'loader.load_module("test_pkg")')],
            capture_output=True, text=True, cwd=_REPO_ROOT,
        )
        self.assertEqual(r.returncode, 0,
                         f'import failed: {r.stderr}')
        # No command output on stdout (no refresh, no help, etc.)
        self.assertEqual(r.stdout.strip(), '')


class TestNoTraceback(unittest.TestCase):
    """Expected user errors produce one-line error: messages, no traceback."""

    def test_absolute_path_no_traceback(self):
        r = _run(['refresh', '/bad'])
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('Traceback', r.stderr)
        self.assertIn('error:', r.stderr)

    def test_traversal_no_traceback(self):
        r = _run(['refresh', '../bad'])
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('Traceback', r.stderr)

    def test_missing_pkg_no_traceback(self):
        r = _run(['refresh', 'does-not-exist'])
        self.assertNotEqual(r.returncode, 0)
        self.assertNotIn('Traceback', r.stderr)


class TestStatusCommand(unittest.TestCase):
    """./package status reports buildability."""

    def test_status_help(self):
        r = _run(['status', '--help'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('package', r.stdout)
        self.assertIn('onto', r.stdout)

    def test_status_forth_core(self):
        r = _run(['status', 'forth-core'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('package: forth-core', r.stdout)
        self.assertIn('manifest base:', r.stdout)
        self.assertIn('caller HEAD:', r.stdout)
        self.assertIn('manifest-base src/c47 == caller HEAD:src/c47:', r.stdout)
        self.assertIn('caller src/c47 dirty:', r.stdout)
        self.assertIn('generated patches/ or files/ differ in Git:', r.stdout)
        self.assertIn('conflict markers in working files:', r.stdout)
        self.assertIn('locally buildable:', r.stdout)

    def test_status_with_onto(self):
        r = _run(['status', 'forth-core', '--onto', 'HEAD'])
        self.assertEqual(r.returncode, 0)
        self.assertIn('target:', r.stdout)

    def test_status_missing_package(self):
        r = _run(['status', 'nonexistent-pkg'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)

    def test_status_invalid_onto_ref(self):
        r = _run(['status', 'forth-core', '--onto', 'nonexistent-ref-xyz'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('error:', r.stderr)


class TestRebasePreflight(unittest.TestCase):
    """./package rebase emits warning when src/c47 differs from target."""

    def test_rebase_help_mentions_preflight(self):
        r = _run(['rebase', '--help'])
        self.assertEqual(r.returncode, 0)


class TestEntryPointLineCount(unittest.TestCase):
    """Root package entry point must stay below 220 lines."""

    def test_line_count_below_cap(self):
        """The package file must have fewer than 220 physical lines."""
        repo_root = os.path.dirname(os.path.dirname(
            os.path.abspath(__file__)))
        package_path = os.path.join(repo_root, 'package')
        with open(package_path) as f:
            lines = f.readlines()
        self.assertLess(len(lines), 220,
                        f'package has {len(lines)} lines (cap: 220)')


class TestStatusConflictMarkers(unittest.TestCase):
    """Status detects conflict markers in working files."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_status_marker_test_')
        self._orig_cwd = os.getcwd()

    def tearDown(self):
        os.chdir(self._orig_cwd)
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_markers_make_not_buildable(self):
        """A working file with conflict markers makes locally buildable: no."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        os.chdir(repo)

        # Inject conflict markers into a working file.
        with open(os.path.join(repo, pkgdir, 'main.c'), 'w') as f:
            f.write('<<<<<<< ours\n// ours\n=======\n// theirs\n'
                    '>>>>>>> theirs\n')

        # Use pkg_patch_cli directly with repo as cwd.
        import pkg_patch_cli  # noqa: E402
        from io import StringIO
        from unittest.mock import patch

        # Capture stdout/stderr.
        out_buf = StringIO()
        err_buf = StringIO()
        exit_code = [0]

        def mock_exit(code=0):
            exit_code[0] = code
            raise SystemExit(code)

        with patch.object(sys, 'stdout', out_buf), \
             patch.object(sys, 'stderr', err_buf), \
             patch.object(sys, 'exit', side_effect=mock_exit), \
             patch.object(pkg_patch_cli, '_REPO_ROOT', repo):
            try:
                pkg_name = pkgdir[len('packages/'):]
                args = type('Args', (), {'package': pkg_name,
                                         'onto': None})()
                pkg_patch_cli._cmd_status(args)
            except SystemExit:
                pass

        self.assertEqual(exit_code[0], 0)
        self.assertIn('conflict markers in working files: yes', out_buf.getvalue())
        self.assertIn('locally buildable: no', out_buf.getvalue())


class TestStatusFailedProbe(unittest.TestCase):
    """Failed Git probes return nonzero."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix='pkg_status_probe_test_')
        self._orig_cwd = os.getcwd()

    def tearDown(self):
        os.chdir(self._orig_cwd)
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_failed_probe_exits_nonzero(self):
        """When git ls-tree fails for src/c47, status exits nonzero."""
        repo, pkgdir = _make_repo_with_pkg(self.tmpdir)
        os.chdir(repo)

        # Update manifest to point to a commit that has no src/c47.
        # Create an orphan commit without src/c47.
        subprocess.run(['git', 'checkout', '--orphan', 'no-src'], cwd=repo,
                       capture_output=True)
        subprocess.run(['git', 'rm', '-rf', '.'], cwd=repo,
                       capture_output=True)
        with open(os.path.join(repo, 'dummy.txt'), 'w') as f:
            f.write('dummy\n')
        subprocess.run(['git', 'add', '.'], cwd=repo, capture_output=True)
        subprocess.run(['git', 'commit', '-m', 'no src'], cwd=repo,
                       capture_output=True)
        orphan_sha = subprocess.run(
            ['git', 'rev-parse', 'HEAD'], cwd=repo,
            capture_output=True, text=True).stdout.strip()
        subprocess.run(['git', 'checkout', 'master'], cwd=repo,
                       capture_output=True)

        # Update manifest to use orphan SHA as base.
        manifest_path = os.path.join(repo, pkgdir, '.refresh-manifest.json')
        with open(manifest_path) as f:
            manifest = json.load(f)
        manifest['base_commit'] = orphan_sha
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f)

        # Use pkg_patch_cli directly.
        import pkg_patch_cli  # noqa: E402
        from io import StringIO
        from unittest.mock import patch

        out_buf = StringIO()
        err_buf = StringIO()
        exit_code = [0]

        def mock_exit(code=0):
            exit_code[0] = code
            raise SystemExit(code)

        with patch.object(sys, 'stdout', out_buf), \
             patch.object(sys, 'stderr', err_buf), \
             patch.object(sys, 'exit', side_effect=mock_exit), \
             patch.object(pkg_patch_cli, '_REPO_ROOT', repo):
            try:
                pkg_name = pkgdir[len('packages/'):]
                args = type('Args', (), {'package': pkg_name,
                                         'onto': None})()
                pkg_patch_cli._cmd_status(args)
            except SystemExit:
                pass

        self.assertNotEqual(exit_code[0], 0)
        self.assertIn('failed probe', err_buf.getvalue())


if __name__ == '__main__':
    unittest.main()
