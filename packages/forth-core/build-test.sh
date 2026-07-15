#!/usr/bin/env bash
#
# forth-core build + self-test runner.
# Canonical build/test invocation — do NOT re-derive meson/ninja flags by hand.
#
# Usage:
#   ./packages/forth-core/build-test.sh            # reconfigure, build, run self-tests
#   ./packages/forth-core/build-test.sh --no-setup # skip meson reconfigure (build+run only)
#   ./packages/forth-core/build-test.sh --build    # build only, do not run the suite
#
# Why refresh + --reconfigure by default:
#   The resolver builds the shadow tree from the package's GENERATED patches/ and
#   files/ — it never reads the flat working area. So an edit to
#   packages/forth-core/<file> is INVISIBLE to the compiler until
#   tools/pkg_patch_refresh.py regenerates patches/+files/ from it. Without the
#   refresh step this script happily reports GREEN for code you did not build:
#   verified directly by injecting a marker into a working-area source, running
#   setup, and grepping the shadow — the marker was absent.
#   The shadow itself is then (re)built at `meson setup` time, so a bare `ninja`
#   after a refresh would still compile the stale shadow. Both steps are needed.
# Pass --no-setup only when you KNOW no package source changed since last setup.
#
# Exit status: nonzero if configure fails, build fails, OR any self-test fails
# (the suite is gated: config.c aggregates failures and exit(1)s). A green run
# here genuinely means the suite passed.

set -euo pipefail

# --- Resolve repo root regardless of where the script is invoked from ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

BUILD_DIR="build.sim"
PKG="packages/forth-core"
BINARY="${BUILD_DIR}/src/c47-gtk/c47"   # NOTE: bare `ninja` leaves the binary here, not ./c47

DO_SETUP=1
DO_RUN=1
for arg in "$@"; do
  case "$arg" in
    --no-setup) DO_SETUP=0 ;;
    --build)    DO_RUN=0 ;;
    -h|--help)
      sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

echo "==> repo: ${REPO_ROOT}"

# --- Regenerate patches/+files/ from the flat working area ---
# MUST precede meson setup: the resolver globs patches/+files/ and never reads
# the working area, so skipping this compiles the PREVIOUS patch content and
# silently ignores every edit made since the last refresh.
if [[ "${DO_SETUP}" -eq 1 ]]; then
  echo "==> pkg_patch_refresh (regenerating ${PKG}/patches + files from working area)"
  python3 tools/pkg_patch_refresh.py "${PKG}"
fi

# --- Configure (re-shadows the CUSTOM_PKG tree from patches/+files/) ---
if [[ "${DO_SETUP}" -eq 1 ]]; then
  echo "==> meson setup (reconfigure, CUSTOM_PKG=${PKG})"
  meson setup "${BUILD_DIR}" \
    --buildtype=custom \
    --reconfigure \
    -DRASPBERRY=false \
    -DDECNUMBER_FASTMUL=true \
    -DCUSTOM_PKG="${PKG}"
  # FORTH_DEBUG_SELFTEST: under the patch-based package system the resolver
  # never reads packages/*/meson.build, so the meson_options.txt option is
  # consumed by nothing and the self-test suite silently compiles OUT unless
  # the define is injected here. (`meson setup --reconfigure -Dc_args=...`
  # does NOT reliably apply compiler args to an existing build dir;
  # `meson configure` does.) Without this the suite is vacuous green.
  echo "==> enabling FORTH_DEBUG_SELFTEST via c_args"
  meson configure "${BUILD_DIR}" -Dc_args=-DFORTH_DEBUG_SELFTEST
else
  echo "==> skipping meson setup (--no-setup); building against existing shadow"
fi

# --- Build ---
echo "==> ninja -C ${BUILD_DIR}"
ninja -C "${BUILD_DIR}"

if [[ ! -x "${BINARY}" ]]; then
  echo "ERROR: expected binary not found at ${BINARY}" >&2
  exit 1
fi

# --- Run the gated self-test suite (headless: no GTK window, clean exit) ---
if [[ "${DO_RUN}" -eq 1 ]]; then
  echo "==> running self-test suite headless: ${BINARY} --headless"
  set +e
  "${BINARY}" --headless
  status=$?
  set -e
  echo "==> self-test EXIT STATUS: ${status}"
  if [[ "${status}" -ne 0 ]]; then
    echo "==> SELF-TEST FAILED (exit ${status}) — suite gated the build red." >&2
    exit "${status}"
  fi
  echo "==> BUILD + SELF-TEST GREEN."
else
  echo "==> build only (--build); skipped self-test run."
fi