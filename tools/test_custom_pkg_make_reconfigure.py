#!/usr/bin/env python3
"""Focused regression tests for Makefile package-shadow reconfiguration.

These tests replace Meson with a recorder, so they exercise the real phony
Make targets without configuring or building firmware.
"""

import os
import shutil
import stat
import subprocess
import tempfile
import unittest


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


class TestCustomPkgMakeReconfigure(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.mkdtemp(prefix='custom-pkg-make-')
        self.addCleanup(shutil.rmtree, self.tempdir)
        shutil.copy2(os.path.join(REPO_ROOT, 'Makefile'), self.tempdir)

        self.fake_bin = os.path.join(self.tempdir, 'fake-bin')
        os.makedirs(self.fake_bin)
        self.meson_log = os.path.join(self.tempdir, 'meson.log')
        self._write_executable(
            os.path.join(self.fake_bin, 'meson'),
            '#!/bin/sh\n'
            'printf \'%s\\n\' "$*" >> "$FAKE_MESON_LOG"\n'
            'if [ "$FAKE_MESON_FAIL" = "1" ]; then exit 23; fi\n')

        tools_dir = os.path.join(self.tempdir, 'tools')
        os.makedirs(tools_dir)
        self._write_executable(
            os.path.join(tools_dir, 'onARaspberry'),
            '#!/bin/sh\nprintf false\n')

        self.env = os.environ.copy()
        self.env['PATH'] = self.fake_bin + os.pathsep + self.env['PATH']
        self.env['FAKE_MESON_LOG'] = self.meson_log

    @staticmethod
    def _write_executable(path, content):
        with open(path, 'w') as f:
            f.write(content)
        os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)

    def _prepare_build(self, build_dir, package='packages/forth-core'):
        path = os.path.join(self.tempdir, build_dir)
        os.makedirs(path)
        with open(os.path.join(path, '.custom_pkg_stamp'), 'w') as f:
            f.write(package + '\n')

    def _run_make(self, target, **variables):
        cmd = ['make', '-f', 'Makefile', target]
        cmd.extend(f'{key}={value}' for key, value in variables.items())
        return subprocess.run(
            cmd, cwd=self.tempdir, env=self.env,
            capture_output=True, text=True, check=False)

    def _meson_calls(self):
        if not os.path.isfile(self.meson_log):
            return []
        with open(self.meson_log) as f:
            return [line.rstrip('\n') for line in f]

    def test_same_package_stays_incremental_by_default(self):
        self._prepare_build('build.dmcp5')

        result = self._run_make(
            'check-custom-pkg-dmcp5',
            CUSTOM_PKG='packages/forth-core')

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self._meson_calls(), [])
        self.assertNotIn('rematerializing package overlay', result.stdout)

    def test_opt_in_forces_all_three_shadow_reconfigure_paths(self):
        cases = (
            ('check-custom-pkg-sim', 'build.sim'),
            ('check-custom-pkg-dmcp', 'build.dmcp.p4'),
            ('check-custom-pkg-dmcp5', 'build.dmcp5'),
        )
        for target, build_dir in cases:
            with self.subTest(target=target):
                if os.path.isfile(self.meson_log):
                    os.unlink(self.meson_log)
                self._prepare_build(build_dir)

                result = self._run_make(
                    target,
                    CUSTOM_PKG='packages/forth-core',
                    CUSTOM_PKG_RECONFIGURE='1')

                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(
                    f'rematerializing package overlay in {build_dir}',
                    result.stdout)
                calls = self._meson_calls()
                self.assertEqual(len(calls), 1)
                self.assertIn(f'setup {build_dir} ', calls[0])
                self.assertIn('-DCUSTOM_PKG=packages/forth-core', calls[0])
                self.assertTrue(calls[0].endswith('--reconfigure'))

    def test_package_change_still_reconfigures_without_opt_in(self):
        self._prepare_build('build.dmcp5', package='packages/old')

        result = self._run_make(
            'check-custom-pkg-dmcp5', CUSTOM_PKG='packages/new')

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CUSTOM_PKG changed ('packages/old' -> 'packages/new')",
                      result.stdout)
        self.assertEqual(len(self._meson_calls()), 1)
        with open(os.path.join(
                self.tempdir, 'build.dmcp5', '.custom_pkg_stamp')) as f:
            self.assertEqual(f.read(), 'packages/new\n')

    def test_failed_forced_reconfigure_is_fatal_and_keeps_stamp(self):
        self._prepare_build('build.dmcp5', package='packages/old')
        self.env['FAKE_MESON_FAIL'] = '1'

        result = self._run_make(
            'check-custom-pkg-dmcp5',
            CUSTOM_PKG='packages/new',
            CUSTOM_PKG_RECONFIGURE='1')

        self.assertNotEqual(result.returncode, 0)
        with open(os.path.join(
                self.tempdir, 'build.dmcp5', '.custom_pkg_stamp')) as f:
            self.assertEqual(f.read(), 'packages/old\n')


if __name__ == '__main__':
    unittest.main()
