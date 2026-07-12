#!/usr/bin/env bash
#
# resolver_safety_test.sh — delete-safety regression suite for resolve_c47_src.py
#
# Runs the reviewer's four scenarios (plus an explicit canary) against THROWAWAY
# directories under /tmp only. It never touches your real repo.
#
# Updated for the plain-diff auto-discovery CLI (PROPOSED_SPEC_CHANGES.md,
# revision 2): the resolver's shadow-mode positional arguments are package
# directories now (each containing patches/ + optionally files/), not
# "pkgdir:relpath" override specs. The scenarios and their pass/fail meaning
# are unchanged from the original suite — only how each package's content is
# constructed changed.
#
# Expected:
#   TEST 1  symlink-mode patch            -> exit 0, patched content in shadow
#   TEST 2  decoy custom_pkg_shadow       -> REFUSED (exit 1), user data survives
#   TEST 3  path-traversal patch filename -> REJECTED (exit 1)
#   TEST 4  copy-mode patch               -> exit 0, patched content in shadow
#   CANARY  a file outside the shadow     -> STILL EXISTS after every run
#
# Usage:
#   ./resolver_safety_test.sh /home/stan/c43/tools/resolve_c47_src.py

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
run_resolver() {
  local meson="$1" src_root="$2" shadow="$3"; shift 3
  python3 "$RESOLVER" --shadow "$meson" "$src_root" "$shadow" "$@" 2>&1
}

# Build a fresh fake tree for one test. Layout:
#   $root/src/c47/meson.build         (a stub the resolver parses)
#   $root/src/c47/foo.c               (a fake upstream source, committed)
#   $root/src/c47/CANARY.txt          (must ALWAYS survive)
#   $root/build/custom_pkg_shadow/    (the shadow dir the resolver manages)
#   $root/pkg/patches/010-foo.c.patch (a real patch against foo.c)
make_tree() {
  local root="$1"
  rm -rf "$root"
  mkdir -p "$root/src/c47" "$root/build" "$root/pkg/patches"
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
  ( cd "$root" && git init -q && git add -A && \
    git -c user.email=t@t -c user.name=t commit -q -m base )

  # Materialize + edit a copy, diff it against the committed upstream to
  # produce a real, --3way-applicable patch, exactly like `refresh` would.
  # Headers are rewritten in Python (robust path-escaping) rather than
  # sed, mirroring pkg_patch_refresh.py's own header rewrite.
  local scratch; scratch="$(mktemp -d)"
  echo "// package patch of foo.c"       > "$scratch/foo.c.new"
  ( cd "$root" && git diff --no-index --full-index -U3 \
      src/c47/foo.c "$scratch/foo.c.new" 2>/dev/null ) \
    | python3 -c '
import sys
for line in sys.stdin:
    if line.startswith("diff --git "):
        line = "diff --git a/src/c47/foo.c b/src/c47/foo.c\n"
    elif line.startswith("--- a/"):
        line = "--- a/src/c47/foo.c\n"
    elif line.startswith("+++ b/"):
        line = "+++ b/src/c47/foo.c\n"
    sys.stdout.write(line)
' > "$root/pkg/patches/010-foo.c.patch"
  rm -rf "$scratch"
}

canary_alive() {  # $1 = root
  [ -f "$1/src/c47/CANARY.txt" ] && grep -q "MUST SURVIVE" "$1/src/c47/CANARY.txt"
}

# ===========================================================================
say "TEST 1 — symlink-mode patch must SUCCEED (exit 0) and shadow the patched content"
T1=/tmp/rst_test1
make_tree "$T1"
out="$(run_resolver "$T1/src/c47/meson.build" "$T1" "$T1/build/custom_pkg_shadow" "pkg")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -eq 0 ]; then ok "resolver succeeded on a normal symlink-mode patch"
else bad "resolver exited $rc on a legitimate patch (F10 false-positive not fixed?)"; fi
if [ $rc -eq 0 ] && grep -q "package patch of foo.c" "$T1/build/custom_pkg_shadow/foo.c" 2>/dev/null; then
  ok "shadow contains the patched content"
else
  bad "shadow does not contain the patched content"
fi
canary_alive "$T1" && ok "canary survived" || bad "CANARY DESTROYED in test 1"

# ===========================================================================
say "TEST 2 — decoy pre-existing custom_pkg_shadow with user data must be REFUSED"
T2=/tmp/rst_test2
make_tree "$T2"
# Simulate a directory that merely happens to be named custom_pkg_shadow and is
# full of the user's data — NOT created by the resolver (no sentinel).
mkdir -p "$T2/build/custom_pkg_shadow/mywork"
echo "PRECIOUS USER DATA — MUST NOT BE DELETED" > "$T2/build/custom_pkg_shadow/mywork/data.txt"
out="$(run_resolver "$T2/src/c47/meson.build" "$T2" "$T2/build/custom_pkg_shadow" "pkg")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -ne 0 ]; then ok "resolver REFUSED to touch a sentinel-less non-empty dir (exit $rc)"
else bad "resolver proceeded (exit 0) against a sentinel-less user dir — WIPE RISK NOT FIXED"; fi
if [ -f "$T2/build/custom_pkg_shadow/mywork/data.txt" ]; then ok "user data survived"
else bad "USER DATA DELETED — the exact wipe class is still live"; fi

# ===========================================================================
say "TEST 3 — path traversal via a hostile patch filename must be REJECTED"
T3=/tmp/rst_test3
make_tree "$T3"
mkdir -p "$T3/pkg2/patches"
echo "// a file OUTSIDE the source tree" > "/tmp/rst_outside.h"
# decode_patch_filename rejects '..' segments outright (self-audited,
# tools/test_pkg_patch_common.py) — this is the same class of attack the
# original suite exercised via an override spec, now via a patch filename.
cat > "$T3/pkg2/patches/010-..__..__..__..__tmp__rst_outside.h.patch" <<'EOF'
--- a/src/c47/nonexistent.h
+++ b/src/c47/nonexistent.h
@@ -1 +1 @@
-old
+new
EOF
out="$(run_resolver "$T3/src/c47/meson.build" "$T3" "$T3/build/custom_pkg_shadow" "pkg2")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -ne 0 ]; then ok "traversal patch filename rejected (exit $rc)"
else bad "traversal patch filename ACCEPTED — containment not enforced"; fi
[ -f "/tmp/rst_outside.h" ] && ok "outside file untouched" || bad "outside file affected"
rm -f "/tmp/rst_outside.h"

# ===========================================================================
say "TEST 4 — copy-mode patch must still SUCCEED"
T4=/tmp/rst_test4
make_tree "$T4"
out="$(CUSTOM_PKG_SHADOW_COPY=1 run_resolver "$T4/src/c47/meson.build" "$T4" "$T4/build/custom_pkg_shadow" "pkg")"
rc=$?
echo "$out" | sed 's/^/    | /'
echo "    (exit $rc)"
if [ $rc -eq 0 ]; then ok "copy-mode patch succeeded"
else bad "copy-mode patch failed (exit $rc)"; fi
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
