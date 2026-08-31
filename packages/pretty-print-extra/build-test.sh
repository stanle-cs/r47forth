#!/usr/bin/env bash
#
# pretty-print-extra build + test runner.
# Canonical build/test invocation — do NOT re-derive meson/ninja flags by hand.
#
# Usage:
#   ./packages/pretty-print-extra/build-test.sh          # pair gate, then full gate
#   ./packages/pretty-print-extra/build-test.sh --pair   # core+extra only
#   ./packages/pretty-print-extra/build-test.sh --full   # all four packages only
#
# This package REQUIRES packages/pretty-print: it calls the core
# engine's layout API and registers its extension hooks. There is no
# solo pass — solo cannot link, by design. The minimal configuration
# is the pair.
#
# Why refresh + --reconfigure every time:
#   The resolver builds the shadow tree from the package's GENERATED patches/
#   and files/ — it never reads the flat working area. An edit to
#   packages/pretty-print-extra/<file> is INVISIBLE to the compiler until
#   tools/pkg_patch_refresh.py regenerates patches/+files/. Without the refresh
#   step this script reports GREEN for code it did not build.
#
# The gate for each pass: configure + build + upstream testSuite (which
# now includes tests/pretty_extra.txt and pretty_visual_real.txt),
# status AND banner both required. The full pass proves composition
# with forth-core AND undo-history AND the core package: all four patch
# stacks apply (loud conflict otherwise) and the same suite stays
# green. This package's keyboard.c, items.c and screen.c hunks sit in
# the most contended composition surfaces, so the full pass is the
# load-bearing one.
# NOTE: the full pass runs the plain testSuite battery only —
# forth-core's own FORTH_DEBUG_SELFTEST battery is forth-core's gate,
# not this one.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

BUILD_DIR="build.sim"
PAIR="packages/pretty-print,packages/pretty-print-extra"
FULL="packages/forth-core,packages/undo-history,packages/pretty-print,packages/pretty-print-extra"

DO_PAIR=1
DO_FULL=1
for arg in "$@"; do
  case "$arg" in
    --pair) DO_FULL=0 ;;
    --full) DO_PAIR=0 ;;
    -h|--help)  sed -n '2,34p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

run_pass() {
  local pkgs="$1" label="$2" cargs="$3"
  echo "==> [${label}] meson setup (reconfigure, CUSTOM_PKG=${pkgs})"
  meson setup "${BUILD_DIR}" \
    --buildtype=custom \
    --reconfigure \
    -DRASPBERRY=false \
    -DDECNUMBER_FASTMUL=true \
    -DCUSTOM_PKG="${pkgs}"
  # Per-pass c_args, set the reliable way (see forth-core's gate: `meson
  # setup --reconfigure -Dc_args=...` does NOT reliably apply to an existing
  # build dir; `meson configure` does). The full pass needs
  # FORTH_DEBUG_SELFTEST because forth-core's testSuite.c patch references
  # its self-test drivers, which compile only under that define.
  echo "==> [${label}] meson configure c_args='${cargs}'"
  meson configure "${BUILD_DIR}" -Dc_args="${cargs}"
  echo "==> [${label}] ninja -C ${BUILD_DIR}"
  ninja -C "${BUILD_DIR}"
  echo "==> [${label}] meson test testSuite"
  set +e
  meson test -C "${BUILD_DIR}" testSuite
  local status=$?
  set -e
  echo "==> [${label}] testSuite EXIT STATUS: ${status}"
  if [[ "${status}" -ne 0 ]]; then
    echo "==> [${label}] TESTSUITE FAILED. See ${BUILD_DIR}/meson-logs/testlog.txt" >&2
    exit "${status}"
  fi
  if ! grep -q "TESTS PASSED SUCCESSFULLY" "${BUILD_DIR}/meson-logs/testlog.txt"; then
    echo "==> [${label}] ERROR: testSuite did not report its success banner (wrong binary or truncated run?)" >&2
    exit 1
  fi
  echo "==> [${label}] GREEN"
}

echo "==> repo: ${REPO_ROOT}"
# Refresh all packages up front: the composed passes compose generated
# output, and a stale sibling patches/ would test yesterday's sibling.
echo "==> pkg_patch_refresh packages/pretty-print-extra"
python3 tools/pkg_patch_refresh.py packages/pretty-print-extra
echo "==> pkg_patch_refresh packages/pretty-print"
python3 tools/pkg_patch_refresh.py packages/pretty-print
if [[ "${DO_FULL}" -eq 1 ]]; then
  echo "==> pkg_patch_refresh packages/forth-core"
  python3 tools/pkg_patch_refresh.py packages/forth-core
  echo "==> pkg_patch_refresh packages/undo-history"
  python3 tools/pkg_patch_refresh.py packages/undo-history
fi

if [[ "${DO_PAIR}" -eq 1 ]]; then
  run_pass "${PAIR}" "pair" ""
fi
if [[ "${DO_FULL}" -eq 1 ]]; then
  run_pass "${FULL}" "full" "-DFORTH_DEBUG_SELFTEST"
fi
echo "==> PRETTY-PRINT-EXTRA GATE GREEN."
