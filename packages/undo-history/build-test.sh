#!/usr/bin/env bash
#
# undo-history build + test runner.
# Canonical build/test invocation — do NOT re-derive meson/ninja flags by hand.
#
# Usage:
#   ./packages/undo-history/build-test.sh            # solo gate, then combined-with-forth-core gate
#   ./packages/undo-history/build-test.sh --solo     # solo gate only
#   ./packages/undo-history/build-test.sh --combined # combined gate only
#
# Why refresh + --reconfigure every time:
#   The resolver builds the shadow tree from the package's GENERATED patches/
#   and files/ — it never reads the flat working area. An edit to
#   packages/undo-history/<file> is INVISIBLE to the compiler until
#   tools/pkg_patch_refresh.py regenerates patches/+files/. Without the refresh
#   step this script reports GREEN for code it did not build.
#
# The gate for each pass: configure + build + upstream testSuite (which now
# includes tests/undo_history.txt), status AND banner both required.
# The combined pass proves composition with forth-core: both patch stacks
# apply (loud conflict otherwise) and the same suite stays green.
# NOTE: the combined pass runs the plain testSuite battery only — forth-core's
# own FORTH_DEBUG_SELFTEST battery is forth-core's gate, not this one.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

BUILD_DIR="build.sim"
PKG="packages/undo-history"
COMBINED="packages/forth-core,packages/undo-history"

DO_SOLO=1
DO_COMBINED=1
for arg in "$@"; do
  case "$arg" in
    --solo)     DO_COMBINED=0 ;;
    --combined) DO_SOLO=0 ;;
    -h|--help)  sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
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
  # build dir; `meson configure` does). The combined pass needs
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
# Refresh both packages up front: the combined pass composes generated output,
# and a stale forth-core patches/ would test yesterday's forth-core.
echo "==> pkg_patch_refresh ${PKG}"
python3 tools/pkg_patch_refresh.py "${PKG}"
if [[ "${DO_COMBINED}" -eq 1 ]]; then
  echo "==> pkg_patch_refresh packages/forth-core"
  python3 tools/pkg_patch_refresh.py packages/forth-core
fi

if [[ "${DO_SOLO}" -eq 1 ]]; then
  run_pass "${PKG}" "solo" ""
fi
if [[ "${DO_COMBINED}" -eq 1 ]]; then
  run_pass "${COMBINED}" "combined" "-DFORTH_DEBUG_SELFTEST"
fi
echo "==> UNDO-HISTORY GATE GREEN."
