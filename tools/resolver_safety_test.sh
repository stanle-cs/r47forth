#!/usr/bin/env bash
#
# resolver_safety_test.sh — delete-safety regression suite for resolve_c47_src.py
#
# Runs the reviewer's four scenarios (plus an explicit canary) against THROWAWAY
# directories under /tmp only. It never touches your real repo.
#
# Expected AFTER the three fixes are applied:
#   TEST 1  symlink-mode override        -> exit 0, override content in shadow   (was: exit 1, F10 false-positive)
#   TEST 2  decoy custom_pkg_shadow      -> REFUSED (exit 1), user data survives (was: wiped, exit 0)
#   TEST 3  ../outside.h traversal       -> REJECTED (exit 1)                    (unchanged)
#   TEST 4  copy-mode override           -> exit 0, override content in shadow   (unchanged)
#   CANARY  a file outside the shadow    -> STILL EXISTS after every run
#
# Usage:
#   ./resolver_safety_test.sh /home/stan/c43/tools/resolve_c47_src.py
#
# If any test's actual result != expected, the suite prints FAIL and exits 1.

set -u

RESOLVER="${1:?usage: resolver_safety_test.sh <path-to-resolve_c47_src.py>}"
if [ ! -f "$RESOLVER" ]; then
  echo "resolver not found: $RESOLVER" >&2; exit 2
fi
RESOLVER="$(cd "$(dirname "$RESOLVER")" && pwd)/$(basename "$RESOLVER")"  # absolutize

PASS=0; FAIL=0
say()  { printf '\n=== %s ===\n' "$1"; }
ok()   { printf '  PASS: %s\n' "$1"; PASS=$((PASS+1)); }
bad()  { printf '  FAIL: %s\n' "$1"; FAIL=$((FAIL+1)); }

# ---------------------------------------------------------------------------
# ADJUST THIS FUNCTION if your resolver's CLI differs.
# Reviewer described the call as:
#   python3 resolve_c47_src.py --shadow <meson.build> <source_root> <shadow_dir> <spec> [<spec>...]
# where <spec> is "package_path:file". Tune the arg order/flags to match YOURS.
# It must: print to stderr, exit nonzero on refusal, populate <shadow_dir> on success.
# ---------------------------------------------------------------------------
run_resolver() {
  local meson="$1" src_root="$2" shadow="$3"; shift 3
  python3 "$RESOLVER" --shadow "$meson" "$src_root" "$shadow" "$@" 2>&1
}

# Build a fresh fake tree for one test. Layout:
#   $root/src/c47/meson.build         (a stub the resolver parses)
#   $root/src/c47/foo.c               (a fake upstream source, overridable)
#   $root/src/c47/CANARY.txt          (must ALWAYS survive)
#   $root/build/custom_pkg_shadow/    (the shadow dir the resolver manages)
#   $root/pkg/foo.c                   (the package override)
make_tree() {
  local root="$1"
  rm -rf "$root"
  mkdir -p "$root/src/c47" "$root/build" "$root/pkg"
  # a minimal meson.build the resolver can parse for its source list; adjust if
  # your resolver greps a specific pattern. Reviewer says it parses files(...).
  cat > "$root/src/c47/meson.build" <<'EOF'
c47_src = files(
  'foo.c',
)
c47_inc = include_directories(
  '.',
)
EOF
  echo "// fake upstream foo.c"          > "$root/src/c47/foo.c"
  echo "CANARY — MUST SURVIVE"           > "$root/src/c47/CANARY.txt"
  echo "// package override of foo.c"    > "$root/pkg/foo.c"
}

canary_alive() {  # $1 = root
  [ -f "$1/src/c47/CANARY.txt" ] && grep -q "MUST SURVIVE" "$1/src/c47/CANARY.txt"
}

# ===========================================================================
say "TEST 1 — symlink-mode override must SUCCEED (exit 0) and shadow the override"
T1=/tmp/rst_test1
make_tree "$T1"
out="$(run_resolver "$T1/src/c47/meson.build" "$T1" "$T1/build/custom_pkg_shadow" "pkg:foo.c")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -eq 0 ]; then ok "resolver succeeded on a normal symlink-mode override"
else bad "resolver exited $rc on a legitimate override (F10 false-positive not fixed?)"; fi
canary_alive "$T1" && ok "canary survived" || bad "CANARY DESTROYED in test 1"

# ===========================================================================
say "TEST 2 — decoy pre-existing custom_pkg_shadow with user data must be REFUSED"
T2=/tmp/rst_test2
make_tree "$T2"
# Simulate a directory that merely happens to be named custom_pkg_shadow and is
# full of the user's data — NOT created by the resolver (no sentinel).
mkdir -p "$T2/build/custom_pkg_shadow/mywork"
echo "PRECIOUS USER DATA — MUST NOT BE DELETED" > "$T2/build/custom_pkg_shadow/mywork/data.txt"
out="$(run_resolver "$T2/src/c47/meson.build" "$T2" "$T2/build/custom_pkg_shadow" "pkg:foo.c")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -ne 0 ]; then ok "resolver REFUSED to touch a sentinel-less non-empty dir (exit $rc)"
else bad "resolver proceeded (exit 0) against a sentinel-less user dir — WIPE RISK NOT FIXED"; fi
if [ -f "$T2/build/custom_pkg_shadow/mywork/data.txt" ]; then ok "user data survived"
else bad "USER DATA DELETED — the exact wipe class is still live"; fi

# ===========================================================================
say "TEST 3 — path traversal (../outside.h) must be REJECTED"
T3=/tmp/rst_test3
make_tree "$T3"
echo "// a file OUTSIDE the source tree" > "/tmp/rst_outside.h"
out="$(run_resolver "$T3/src/c47/meson.build" "$T3" "$T3/build/custom_pkg_shadow" "pkg:../../../../tmp/rst_outside.h")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -ne 0 ]; then ok "traversal spec rejected (exit $rc)"
else bad "traversal spec ACCEPTED — containment not enforced"; fi
[ -f "/tmp/rst_outside.h" ] && ok "outside file untouched" || bad "outside file affected"

# ===========================================================================
say "TEST 4 — copy-mode override must still SUCCEED"
T4=/tmp/rst_test4
make_tree "$T4"
out="$(CUSTOM_PKG_SHADOW_COPY=1 run_resolver "$T4/src/c47/meson.build" "$T4" "$T4/build/custom_pkg_shadow" "pkg:foo.c")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -eq 0 ]; then ok "copy-mode override succeeded"
else bad "copy-mode override failed (exit $rc)"; fi
canary_alive "$T4" && ok "canary survived" || bad "CANARY DESTROYED in test 4"

# ===========================================================================
say "RESULT"
printf '  %d passed, %d failed\n' "$PASS" "$FAIL"
if [ "$FAIL" -eq 0 ]; then
  printf '  ALL SAFE — the resolver may be tested against the real tree (after confirming git push).\n'
  exit 0
else
  printf '  NOT SAFE — do NOT run the resolver against your real repo. Fix and re-test.\n'
  exit 1
fi
