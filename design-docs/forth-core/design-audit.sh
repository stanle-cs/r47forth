#!/usr/bin/env bash
#
# forth-core design audit — mechanical half of DESIGN_AUDIT.md.
#
# Cheap enough to run on every stage close. Does NOT build or run the gate.
#
# Usage:
#   ./design-docs/forth-core/design-audit.sh                # audit
#   ./design-docs/forth-core/design-audit.sh --accept       # re-baseline to now
#
# Design: the audit must be QUIET when nothing drifted, or it will be ignored.
# So checks split in two:
#
#   HARD   — never acceptable; always a finding (churn, generated drift,
#            stray shippable content, footprint over budget).
#   REVIEW — legitimate in bounded amounts; baselined in
#            .design-audit-baseline and flagged only when the count GROWS.
#
# A finding is a prompt to think, not proof of a defect. DESIGN_AUDIT.md says
# what each one means and what the accepted exceptions are.
#
# Exit: 0 = clean, 1 = findings to triage.

set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "${REPO_ROOT}"

PKG="packages/forth-core"
BASELINE="${SCRIPT_DIR}/.design-audit-baseline"
ACCEPT=0
[[ "${1:-}" == "--accept" ]] && ACCEPT=1

# Audit against the package's recorded base, not the repository checkout.
# The package manager can rebase independently of the current Git branch.
BASE_COMMIT=$(python3 -c 'import json; print(json.load(open("packages/forth-core/.refresh-manifest.json"))["base_commit"])')
AUDIT_UPSTREAM_TMP=$(mktemp -d)
cleanup_audit_upstream() {
  find "${AUDIT_UPSTREAM_TMP}" -depth -delete
}
trap cleanup_audit_upstream EXIT
# src/c47 plus the sibling roots the package system may touch (T2-A;
# keep in sync with SIBLING_ROOTS in tools/pkg_patch_common.py). A sibling
# root is archived only when it exists at the base (scratch/test repos and
# very old bases may not have one).
AUDIT_ROOTS="src/c47"
if git rev-parse --verify -q "${BASE_COMMIT}:src/testSuite" >/dev/null; then
  AUDIT_ROOTS="${AUDIT_ROOTS} src/testSuite"
fi
# shellcheck disable=SC2086  # AUDIT_ROOTS is a deliberate word list
if ! git archive "${BASE_COMMIT}" ${AUDIT_ROOTS} | tar -x -C "${AUDIT_UPSTREAM_TMP}"; then
  printf 'error: cannot materialize package base %s\n' "${BASE_COMMIT}" >&2
  exit 1
fi
UPSTREAM="${AUDIT_UPSTREAM_TMP}/src/c47"

findings=0
note()  { printf '  %s\n' "$*"; }
head2() { printf '\n== %s ==\n' "$*"; }
flag()  { findings=$((findings+1)); printf '  [!] %s\n' "$*"; }

# Baseline values (overridden by the file if present)
MAX_OVERRIDE_FILES=14
MAX_ADDED_LINES=634
BASE_NO_FORTH=3
BASE_BIG_BLOCKS=17
# shellcheck disable=SC1090
[[ -f "${BASELINE}" ]] && source "${BASELINE}"

# --- A. Upstream footprint (HARD, against budget) ----------------------------
head2 "A. Upstream footprint"
nfiles=$(find "${PKG}/patches" -name '*.patch' 2>/dev/null | wc -l | tr -d ' ')
nadd=$(cat "${PKG}"/patches/*.patch 2>/dev/null | grep -c '^+[^+]')
ndel=$(cat "${PKG}"/patches/*.patch 2>/dev/null | grep -c '^-[^-]')
note "override files : ${nfiles}  (budget ${MAX_OVERRIDE_FILES})"
note "added lines    : ${nadd}  (budget ${MAX_ADDED_LINES})"
note "removed lines  : ${ndel}"
printf '\n  %-44s %5s %6s %6s\n' "patch" "hunks" "+" "-"
for f in "${PKG}"/patches/*.patch; do
  printf '  %-44s %5s %6s %6s\n' "$(basename "$f")" \
    "$(grep -c '^@@' "$f")" "$(grep -c '^+[^+]' "$f")" "$(grep -c '^-[^-]' "$f")"
done
if [[ "${ACCEPT}" -eq 0 ]]; then
  [[ "${nfiles}" -gt "${MAX_OVERRIDE_FILES}" ]] && \
    flag "override files ${nfiles} > budget ${MAX_OVERRIDE_FILES} — a NEW upstream file is being touched"
  [[ "${nadd}" -gt "${MAX_ADDED_LINES}" ]] && \
    flag "added lines ${nadd} > budget ${MAX_ADDED_LINES} — footprint growing"
fi

# --- B. Hunks with no Forth content (REVIEW) ---------------------------------
head2 "B. Hunks whose added lines never mention Forth"
b_out=$(python3 - "${PKG}" <<'PYEOF'
import re, sys, glob, os
# _tamLeave is D7-1's package-introduced rename of upstream teardown calls
# (2026-08-09 audit): those hunks ARE Forth content, and 20 of them were
# drowning check B's signal.
KEY = re.compile(r'forth|fdict|gdict|FCAP|FWRD|ITM_FCALL|param_core|paramCore|aimBuffer|_tamLeave', re.I)
for f in sorted(glob.glob(f'{sys.argv[1]}/patches/*.patch')):
    hunks, cur = [], None
    for l in open(f).read().split('\n'):
        if l.startswith('@@'): cur = [l]; hunks.append(cur)
        elif cur is not None: cur.append(l)
    for h in hunks:
        added = [l[1:] for l in h if l.startswith('+') and not l.startswith('+++')]
        if added and not any(KEY.search(a) for a in added):
            body = next((a.strip() for a in added if a.strip()), '')
            print(f"{os.path.basename(f)}  {h[0][:52]}\n        + {body[:74]}")
PYEOF
)
b_n=$(printf '%s' "${b_out}" | grep -c '^0' || true)
note "count: ${b_n}  (baseline ${BASE_NO_FORTH})"
[[ -n "${b_out}" ]] && echo "${b_out}" | sed 's/^/  /'
if [[ "${ACCEPT}" -eq 0 && "${b_n}" -gt "${BASE_NO_FORTH}" ]]; then
  flag "no-Forth-content hunks grew ${BASE_NO_FORTH} -> ${b_n}: an upstream fix or stray edit is riding in this package"
fi

# --- C. No-op churn vs upstream (HARD) ---------------------------------------
head2 "C. Whitespace / blank-line churn against upstream"
c_out=$(python3 - "${PKG}" "${UPSTREAM}" <<'PYEOF'
import sys, os, difflib
pkg, up = sys.argv[1], sys.argv[2]
def upath(rel):
    # sibling-root rel (T2-A): src/<rel> beside src/c47
    if rel.split('/', 1)[0] in ('testSuite',):
        return os.path.join(os.path.dirname(up), rel)
    return os.path.join(up, rel)
for root, dirs, files in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('patches', 'files')]
    for fn in files:
        if not fn.endswith(('.c', '.h')): continue
        rel = os.path.relpath(os.path.join(root, fn), pkg)
        u = upath(rel)
        if not os.path.exists(u): continue
        A = open(u, errors='replace').read().split('\n')
        B = open(os.path.join(pkg, rel), errors='replace').read().split('\n')
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, A, B, autojunk=False).get_opcodes():
            if tag == 'replace':
                a, b = A[i1:i2], B[j1:j2]
                if len(a) == len(b) and all(x.rstrip() == y.rstrip() for x, y in zip(a, b)) \
                   and any(x != y for x, y in zip(a, b)):
                    print(f"{rel}:{j1+1}: trailing-whitespace-only change")
            elif tag == 'delete' and 0 < i2-i1 <= 3 and all(x.strip() == '' for x in A[i1:i2]):
                print(f"{rel}: {i2-i1} blank line(s) deleted at upstream {i1+1}")
PYEOF
)
if [[ -n "${c_out}" ]]; then
  echo "${c_out}" | sed 's/^/  /'
  flag "no-op churn — revert to upstream's exact bytes (see S1)"
else
  note "none"
fi

# --- D. Package logic inline in upstream files (REVIEW) ----------------------
head2 "D. Contiguous added blocks >= 12 lines in upstream files"
d_out=$(python3 - "${PKG}" <<'PYEOF'
import sys, glob, os
THRESH = 12
for f in sorted(glob.glob(f'{sys.argv[1]}/patches/*.patch')):
    run, start, hdr = 0, '', ''
    for l in open(f).read().split('\n') + ['']:
        if l.startswith('@@'): hdr = l; run = 0; continue
        if l.startswith('+') and not l.startswith('+++'):
            if run == 0: start = hdr
            run += 1
        else:
            if run >= THRESH:
                print(f"{os.path.basename(f)}: {run} lines  {start[:50]}")
            run = 0
PYEOF
)
d_n=$(printf '%s' "${d_out}" | grep -c . || true)
note "count: ${d_n}  (baseline ${BASE_BIG_BLOCKS})"
[[ -n "${d_out}" ]] && echo "${d_out}" | sed 's/^/  /'
if [[ "${ACCEPT}" -eq 0 && "${d_n}" -gt "${BASE_BIG_BLOCKS}" ]]; then
  flag "inline blocks grew ${BASE_BIG_BLOCKS} -> ${d_n}: new package logic is being written INTO an upstream file"
fi

# --- E. Package-owned allocations (REVIEW, always listed) --------------------
head2 "E. Allocations in PACKAGE-OWNED sources"
e_out=$(python3 - "${PKG}" "${UPSTREAM}" <<'PYEOF'
import sys, os, re
pkg, up = sys.argv[1], sys.argv[2]
def upath(rel):
    # sibling-root rel (T2-A): src/<rel> beside src/c47
    if rel.split('/', 1)[0] in ('testSuite',):
        return os.path.join(os.path.dirname(up), rel)
    return os.path.join(up, rel)
pat = re.compile(r'\b(alloc|free|realloc)C47Blocks\b')
for root, dirs, files in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('patches', 'files')]
    for fn in sorted(files):
        if not fn.endswith('.c') or fn == 'test_dict_reloc.c': continue
        rel = os.path.relpath(os.path.join(root, fn), pkg)
        if os.path.exists(upath(rel)):   # upstream file: not ours
            continue
        for i, line in enumerate(open(os.path.join(pkg, rel), errors='replace'), 1):
            if pat.search(line) and not line.lstrip().startswith(('*', '/*', '//')):
                print(f"{rel}:{i}: {line.strip()[:78]}")
PYEOF
)
if [[ -n "${e_out}" ]]; then
  echo "${e_out}" | sed 's/^/  /'
  note "-> each needs an answer: is its lifetime >= a save/restore cycle?"
  note "   (a shorter-lived allocation is the UPSTREAM_REPORTS_976b864b5.md leak class)"
else
  note "none — package owns no allocation (the S3 end state)"
fi

# --- F. Generated-output drift (HARD) ----------------------------------------
head2 "F. Generated output vs working area / manifest / Git"
f_out=$(python3 - "${PKG}" <<'PYEOF'
import hashlib, json, os, sys, glob

pkg = sys.argv[1]
manifest_path = os.path.join(pkg, '.refresh-manifest.json')
if not os.path.exists(manifest_path):
    print("NO_MANIFEST")
    sys.exit(0)

try:
    with open(manifest_path) as manifest_file:
        manifest = json.load(manifest_file)
except (OSError, json.JSONDecodeError) as exc:
    print(f"INVALID_MANIFEST {exc}")
    sys.exit(0)
stored_patches = manifest.get('patches', {})
stored_files = manifest.get('files', {})
if not isinstance(stored_patches, dict) or not isinstance(stored_files, dict):
    print("INVALID_MANIFEST patches/files must be objects")
    sys.exit(0)

def file_hash(path):
    if not os.path.isfile(path):
        return None
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        h.update(f.read())
    return h.hexdigest()

actual_patches = {}
patches_root = os.path.join(pkg, 'patches')
if os.path.isdir(patches_root):
    for root, dirs, files in os.walk(patches_root):
        for dirname in dirs:
            path = os.path.join(root, dirname)
            if os.path.islink(path):
                rel = os.path.relpath(path, patches_root).replace(os.sep, '/')
                actual_patches[rel] = None
        dirs.sort()
        for filename in sorted(files):
            path = os.path.join(root, filename)
            rel = os.path.relpath(path, patches_root).replace(os.sep, '/')
            actual_patches[rel] = (
                file_hash(path)
                if os.path.isfile(path) and not os.path.islink(path)
                else None
            )
actual_files = {}
files_root = os.path.join(pkg, 'files')
if os.path.isdir(files_root):
    for root, dirs, files in os.walk(files_root):
        for dirname in dirs:
            path = os.path.join(root, dirname)
            if os.path.islink(path):
                rel = os.path.relpath(path, files_root).replace(os.sep, '/')
                actual_files[rel] = None
        dirs.sort()
        for filename in sorted(files):
            path = os.path.join(root, filename)
            rel = os.path.relpath(path, files_root).replace(os.sep, '/')
            actual_files[rel] = (
                file_hash(path)
                if os.path.isfile(path) and not os.path.islink(path)
                else None
            )

ok = True
for category, stored, actual in (
        ('patches', stored_patches, actual_patches),
        ('files', stored_files, actual_files)):
    for rel in sorted(set(stored) | set(actual)):
        if rel not in actual:
            print(f"MISMATCH missing {category}/{rel}")
            ok = False
        elif rel not in stored:
            print(f"MISMATCH unexpected {category}/{rel}")
            ok = False
        elif actual[rel] is None:
            print(f"MISMATCH non-regular {category}/{rel}")
            ok = False
        elif actual[rel] != stored[rel]:
            print(f"MISMATCH hash {category}/{rel}")
            ok = False

if ok:
    print("SYNCED")
PYEOF
)

if [[ "${f_out}" == INVALID_MANIFEST* ]]; then
  echo "${f_out}" | sed 's/^/  /'
  flag "manifest is invalid — generated output integrity is unknown"
elif [[ "${f_out}" == *"MISMATCH"* ]]; then
  echo "${f_out}" | grep "MISMATCH" | sed 's/^/  /'
  flag "generated output differs from manifest hashes — run refresh"
elif [[ "${f_out}" == "SYNCED" || "${f_out}" == *"SYNCED"* ]]; then
  note "generated output synchronized with manifest"
  # Check if it differs from Git (informational only).
  if [[ -n "$(git status --porcelain -- "${PKG}/patches" "${PKG}/files" 2>/dev/null)" ]]; then
    note "generated output differs from Git (informational — uncommitted refresh is OK)"
  else
    note "generated output clean in Git"
  fi
elif [[ "${f_out}" == "NO_MANIFEST" ]]; then
  flag "no manifest — cannot verify generated output hashes"
  if [[ -n "$(git status --porcelain -- "${PKG}/patches" "${PKG}/files" 2>/dev/null)" ]]; then
    note "generated output differs from Git"
  fi
else
  # Fallback: use Git diff for backward compatibility.
  if [[ -z "$(git status --porcelain -- "${PKG}/patches" "${PKG}/files" 2>/dev/null)" ]]; then
    note "patches/ and files/ clean in git"
  else
    note "patches/ or files/ differ from Git"
  fi
fi

# --- G. Stray shippable content (HARD) ---------------------------------------
head2 "G. Working-area files that would ship as firmware"
g_out=$(python3 - "${PKG}" "${UPSTREAM}" <<'PYEOF'
import sys, os, fnmatch
pkg, up = sys.argv[1], sys.argv[2]
def upath(rel):
    # sibling-root rel (T2-A): src/<rel> beside src/c47
    if rel.split('/', 1)[0] in ('testSuite',):
        return os.path.join(os.path.dirname(up), rel)
    return os.path.join(up, rel)
pats = []
pi = os.path.join(pkg, '.pkgignore')
if os.path.exists(pi):
    for line in open(pi):
        line = line.split('#')[0].strip()
        if line: pats.append(line)
def ignored(rel):
    base = os.path.basename(rel)
    for p in pats:
        if p.endswith('/') and rel.startswith(p): return True
        if '/' in p and fnmatch.fnmatch(rel, p): return True
        if '/' not in p and fnmatch.fnmatch(base, p): return True
    return False
for root, dirs, files in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('patches', 'files')]
    for fn in files:
        if fn.startswith('.'): continue
        rel = os.path.relpath(os.path.join(root, fn), pkg)
        if ignored(rel): continue
        if rel.split('/', 1)[0] in ('testSuite',):
            continue   # sibling-root content is dev-only (the testSuite
                       # binary), never firmware — T6, 2026-08-03
        if not os.path.exists(upath(rel)) and not fn.endswith(('.c', '.h')):
            print(f"{rel}  (no upstream counterpart, not .c/.h, not .pkgignore'd)")
PYEOF
)
if [[ -n "${g_out}" ]]; then
  echo "${g_out}" | sed 's/^/  /'
  flag "would be copied into files/ and shipped inside the firmware — add to .pkgignore"
else
  note "none"
fi

# --- H. DESIGN.md citations still resolve (HARD) -----------------------------
# DESIGN.md is authoritative and cites source with [VERIFIED: path:line].
# A citation to a file this package no longer overrides means the doc is
# describing code that is gone. This is how the audit found DESIGN.md still
# claiming the dropped error-text extension was live, one stage after S1
# removed it.
head2 "H. DESIGN.md source citations"
h_out=$(python3 - "${PKG}" "${SCRIPT_DIR}/DESIGN.md" <<'PYEOF'
import re, sys, os
pkg = sys.argv[1]
root = os.path.abspath(os.path.join(pkg, '..', '..'))
txt = open(sys.argv[2], errors='replace').read()
# Require a real extension; excludes prose like "forth_dict.c/.h".
cites = re.findall(r'((?:src/c47|packages/forth-core)/[A-Za-z0-9_/]+\.[ch])\b', txt)
seen, bad = set(), []
for p in cites:
    if p in seen: continue
    seen.add(p)
    if not os.path.exists(os.path.join(root, p)):
        bad.append(p)
for p in sorted(bad):
    print(f"{p}  — cited in DESIGN.md, file does not exist")

# AUDIT round 9 (R9-8): paths were the only thing checked, so a doc naming a
# DELETED FUNCTION as the live mechanism passed. CONSOLIDATE P6 moved the
# interactive-close guard into closeAim() and deleted
# _forthCapCloseIfInteractive; DESIGN.md §8.4.2 went on naming it as the
# choke point every close path goes through, which is the "comment that
# outlived its mechanism" class (r5 R13) at the authoritative doc — and a
# maintainer adding a sixth close path would go looking for a guard that is
# not there, or re-add a site-local one and fork the funnel P6 built.
#
# So: every package-private identifier DESIGN.md names in backticks must
# still exist in the package sources. Scope is the package's own `forth*`
# and `_forth*` symbols — upstream names are upstream's to delete, and
# DESIGN.md legitimately discusses them in the past tense.
ident = set(re.findall(r'`(_?forth[A-Za-z0-9_]+)\(?\)?`', txt))
live = ''
for dirpath, _dirs, files in os.walk(pkg):
    if '/files/' in dirpath + '/' or dirpath.endswith('/files'):
        continue
    for f in files:
        if f.endswith(('.c', '.h')):
            live += open(os.path.join(dirpath, f), errors='replace').read()
dead = sorted(n for n in ident if n not in live)
for n in dead:
    print(f"{n}  — named in DESIGN.md, no such symbol in the package sources")
PYEOF
)
if [[ -n "${h_out}" ]]; then
  echo "${h_out}" | sed 's/^/  /'
  flag "DESIGN.md cites source or a symbol that is gone — the authoritative doc describes code that no longer exists"
else
  note "all cited paths resolve; every package symbol DESIGN.md names is live"
fi

# --- I. Enumerated-site counts (HARD) ----------------------------------------
# AUDIT round 8, from round 7's D7-a: "enumeration without a count check" was
# the dominant defect class of the round, operating at code, record and
# process level at once — F7 guarded one of two consumers of a widened
# predicate, the F1 fix re-derived one of two rewrite sites, and the approved
# D7-1 design counted eleven of 28 sites. Every one was a hand list standing
# in for a counted one.
#
# This group is the countermeasure: a fix or design that enumerates sites
# registers its grep and its expected count HERE. When someone adds a site,
# the count diverges and the audit says so — instead of a reviewer being
# expected to remember. A divergence is not automatically a defect: it means
# the new site must be checked against the rule and the count re-accepted in
# the same commit that adds it.
#
# Format: one PIN per line — expected count, then the description, then the
# grep. Keep the grep anchored to a specific file where possible; a
# repo-wide grep drifts for unrelated reasons and stops being read.
head2 "I. Enumerated-site counts (D7-a pins)"
i_out=""
pin() {  # pin <expected> <description> <count-command...>
  local want="$1" desc="$2"; shift 2
  local got
  got=$("$@" 2>/dev/null | tr -d '[:space:]')
  [[ -z "${got}" ]] && got=0
  if [[ "${got}" != "${want}" ]]; then
    i_out+="${desc}: expected ${want}, found ${got}"$'\n'
  else
    printf '  ok  %-58s %s\n' "${desc}" "${got}"
  fi
}

# C-1 (round 8): every mid-session tam.function rewrite must re-derive fold
# admission through forthFoldRederiveAdmission. Two rewrite sites, two calls.
# Three writes total: tamEnterMode's entry write (whose admission
# forthFoldEnter derives from the same func) plus the two rewrites. Both
# counts are pinned so a new write of ANY shape moves one of them.
#
# R8-5 (round 8, against this pin's first version): the entry write is NOT
# always covered by forthFoldEnter. On a nested TAM — ui/tam.c's
# `_tamLeave(); runFunction(tamOperation());` sites, which the code documents
# as reachable — the capture is SUSPENDED, so tamEnterMode takes the
# fold-pending no-op arm and NOTHING re-derives admission for the new
# function: the fold keeps the outer item's verdict. Inert today because
# every reachable nested target is admitted, and recorded here rather than
# silently relied on. The pin below also could not see tam.function writes
# outside ui/tam.c, so the repo-wide count is pinned too.
pin 3 "ui/tam.c 'tam.function =' writes (1 entry + 2 rewrites)" \
    grep -c 'tam\.function *=[^=]' "${PKG}/ui/tam.c"
pin 2 "ui/tam.c mid-session tam.function rewrites" \
    grep -c 'tam\.function = ITM_' "${PKG}/ui/tam.c"
pin 2 "ui/tam.c forthFoldRederiveAdmission call sites" \
    grep -c 'forthFoldRederiveAdmission(' "${PKG}/ui/tam.c"

# C-2/OOF-1 (round 8): every call in keyboardTweak.c that DESTROYS a softmenu
# frame — rather than stacking over one — is guarded on
# forthCapInteractiveLive(). Two destroyers, two guards. A third destroyer
# arriving from upstream moves the first count and must be guarded or ruled.
# R8-3 (found by round 8 against this pin's first version): it was anchored
# to '^ +', so a destroyer at any other indentation — or reached through a
# differently-spelled call — arrived unguarded with the pin AND the full gate
# green. A verifier mutation proved it. Count the calls wherever they appear,
# and exclude only comment lines.
# R9-4 (round 9): the count stays 2 here. The console arm now performs the
# dismiss itself (HOME.3's native first half), but that pop lives in
# forth_menu.c's forthConsoleHomeRow, not in this override — which is the
# point of putting it there. The rule still holds for both remaining
# destroyers, and the moved one is gated on !forthConsoleBaseOnTop() so the
# only frame it can destroy is one that is not the console's own.
pin 2 "c47Extensions/keyboardTweak.c frame-destroying calls" \
    bash -c "grep -nE '(popSoftmenu *\(|fnExitAllMenus *\()' '${PKG}/c47Extensions/keyboardTweak.c' | grep -vE '^[0-9]+: *[*/]' | wc -l"
# R9-4 (round 9): "two destroyers, two guards" still holds, but one guard is
# now DELEGATED — the HOME.3 destroyer's guard is the first line of
# forth_menu.c's forthConsoleHomeRow(), which is where the pop moved too.
# Counting only the literal predicate would read 1 against correct code and
# force the next reader to re-derive why. Count the guard however it is
# spelled at this file's destroyers: inline, or through the delegate.
pin 2 "c47Extensions/keyboardTweak.c destroyer guards (inline or delegated)" \
    bash -c "grep -cE 'forthCapInteractiveLive\(\)|forthConsoleHomeRow\(\)' '${PKG}/c47Extensions/keyboardTweak.c'"

# Round 8, out-of-family: a guard that SKIPS a pop must ask whether the frame
# it would destroy is the console's own, not merely whether a line is live —
# otherwise it also refuses to dismiss a foreign row stacked over the console.
# Two sites have that shape: screen.c's F7 guard and its keyboardTweak twin.
#
# R9-4 (round 9) REPLACES that pin rather than re-accepting it, because its
# subject stopped existing: neither site skips a pop any more. Aligning with
# what HOME.3 natively does — dismiss the overlay, THEN land on the row
# matching the current input context — the console arm now performs both
# halves itself. The old pin counted the `Live && BaseOnTop` conjunction and
# would read 0 against the correct code, which is a pin outliving its rule
# (the same class as R9-8/R9-9, at the audit script).
#
# The invariant worth holding is the LAND half, because its absence WAS the
# defect: a console arm that dismisses an overlay and then leaves the row to
# a raw -MNU_ALPHA push breaks K-R3. Both twins must land through
# forthConsoleShowSurface, which picks the row from the sub-mode exactly as
# upstream picks TAMALPHA vs ALPHA from tam.alpha.
pin 2 "twin HOME.3 sites going through forthConsoleHomeRow()" \
    bash -c "grep -rn 'forthConsoleHomeRow()' '${PKG}' --include=*.c | grep -v '/files/' | grep -v 'bool_t forthConsoleHomeRow' | wc -l"

# C-2 (round 8): the UPSTREAM census of the predicate Stage L widened to
# count -MNU_FORTH. Round 6's F7 fix enumerated the package tree only and
# came back one consumer short — this counts the upstream files, so a new
# upstream consumer is a finding the day the package rebases onto it.
# R8-4 (round 8, against this pin's first version): counting FILES meant a
# NEW consumer inside any of the five files already counted never moved it —
# and screen.c and keyboard.c, the two files where consumers actually live,
# were both already in the count. Count the call sites.
pin 13 "upstream call sites of isAlphabeticSoftmenu/isAlphaSubmenu" \
    bash -c "grep -rn 'isAlphabeticSoftmenu\|isAlphaSubmenu' '${UPSTREAM}' --include=*.c | grep -vE ':[0-9]+: *(//|/\*|\*)' | wc -l"

# C-6 (round 8): forthCapInteractiveLive IS "origin INTERACTIVE and state
# OPEN". Three production sites hand-rolled that conjunction, which is
# bit-identical today and forks the day the predicate's definition moves or
# one copy is edited alone. No render, route or gate site may spell it out
# longhand — that is the header's contract. The test battery may and does:
# a fixture's REACHED check asserts the two facts SEPARATELY, on purpose, so
# its failure message says which one was wrong. Hence production only.
# R8-2 (round 8): goToPgmStep/goToGlobalStep silently do NOT navigate when
# dynamicMenuItem >= 0, and a softkey commit latches it. FOUR brackets: the
# manage.c override's own at _insertInProgram (:772), and the package's three
# keypress navigations — _forthHistRestoreCursor, forthHistoryGotoLastStep,
# forthFoldLeave's restore. The pin caught its own author's miscount the first
# time it ran, which is the entire argument for group I.
# CONSOLIDATE P8 (2026-08-09): those three navigations moved to
# programming/forth_fold.c with the rest of the fold subsystem, so the FILE
# TARGET is now both files. The count is unchanged and re-verified: 1 in
# manage.c, 3 in forth_fold.c.
#
# R9-7 (round 9): the pin above counted the FIX, not the SUBJECT — the
# bracket idiom, so a package navigation added WITHOUT a bracket left it
# green. Mutation-proven blind: an unbracketed goToPgmStep(1, 1) appended to
# forth_fold.c kept the count at 4 and the whole gate green. That is R8-3/4/5
# recurring inside the countermeasure they created, second consecutive round,
# and the rule it violates is written ten lines above it.
#
# Now it counts the NAVIGATIONS themselves — the thing that must be
# bracketed — and separately asserts that NONE of them is unbracketed. The
# second pin is the one with teeth: it goes red on a navigation the fix
# idiom never reached, which is precisely what the old pin could not see.
#
# Scope is forth_fold.c, the package's OWN file, where P8 collected every
# package navigation. programming/manage.c is an UPSTREAM override and its
# goToPgmStep calls are upstream's own (fnClP, _clearProgram); counting
# those would flag upstream's code as the package's debt. The one bracket
# the package added inside that override is pinned on its own below.
#
# R9-7 also closed the site that made the census ambiguous: forthFoldEnter's
# goToGlobalStep(1) guard now carries the bracket like every other, so the
# rule reads "every package navigation is bracketed" with no exemption
# mechanism to argue about.
pin 4 "package navigations subject to the dynamicMenuItem bracket" \
    bash -c "grep -cE 'goToPgmStep\(|goToGlobalStep\(' \"${PKG}/programming/forth_fold.c\""
# The bracket is a SCOPE, not a neighbouring line: one bracket legitimately
# covers several navigations (the shared cursor restore makes two calls
# inside one). So track the open/close pair rather than peeking at the
# preceding lines — a fixed-size window called the shared restore's second
# navigation unbracketed on this pin's first run, which is the same
# false-positive shape the pins exist to avoid producing.
pin 0 "package navigations left UNBRACKETED" \
    bash -c "awk '/dynamicMenuItem = -1;/ { inside = 1 }
                  /dynamicMenuItem = savedDynamicMenuItem;/ { inside = 0 }
                  /goToPgmStep\(|goToGlobalStep\(/ { if (!inside) n++ }
                  END { print n + 0 }' \"${PKG}/programming/forth_fold.c\""
pin 1 "the package's own bracket inside the manage.c override" \
    bash -c "grep -c 'dynamicMenuItem = -1;' \"${PKG}/programming/manage.c\""

# Sol's dependency (a), round 8 — checked 2026-08-09: tamEnterMode can REFUSE
# (the P-2 arm) and every present caller either dispatches terminally
# (items.c's TAM block, keyboard.c's MNU_Sfdx arm), tolerates a non-entry
# (the M.GOTO row->column chain, the CM_ASSIGN ITM_USERMODE arm), or cannot
# run while the live predicate holds (assignEnterAlpha and both addons.c
# PARAM_* arms — CM_ASSIGN entry and both CM_PEM forgeries suspend the
# capture first).  8 counts CALL SITES, not files (R8-4's lesson); a ninth
# caller has NOT been audited against the refusal — do that before
# re-accepting the count.
pin 8 "tamEnterMode call sites audited against the P-2 refusal" \
    bash -c "grep -rn 'tamEnterMode(' '${PKG}' --include=*.c | grep -v '/files/' | grep -v '/test_' | grep -v 'void tamEnterMode' | wc -l"

# C10/C11 (rounds 1-2, fixed 2026-08-09): every length-limited copy of text
# into the console ring cuts on a GLYPH boundary, through one helper. A byte
# cut leaves a lone lead byte the painter re-pairs with what follows.
# 5 = the definition plus its FOUR call sites (the formatter's short-integer
# and string arms, the ENTER echo, and C5's browse stash). The pin caught the
# author's count twice while this landed, same as the navigation pin did.
pin 5 "glyph-boundary copies (forthCopyWholeGlyphs, definition + sites)" \
    bash -c "grep -rn 'forthCopyWholeGlyphs(' '${PKG}' --include=*.c | grep -v '/files/' | grep -v '/test_' | wc -l"

pin 0 "longhand IsInteractive/IsOpen conjunctions in production sources" \
    bash -c "grep -rn 'forthCapIsInteractive() *&& *forthCapIsOpen()\|forthCapIsOpen() *&& *forthCapIsInteractive()\|!forthCapIsInteractive() *|| *!forthCapIsOpen()\|!forthCapIsOpen() *|| *!forthCapIsInteractive()' '${PKG}' --include=*.c --include=*.h | grep -v '/files/' | grep -v '/test_' | wc -l"

# R9-5 (round 9): the "capture step lies INSIDE FHIST" structural rule was
# spelled TWICE — inlined in the resume canary and again in the fold
# resolver — over two separately stored copies of the same offset, with
# nothing forcing them to agree. Four confirmed defects across rounds 8-9
# came from a consumer still carrying the raw shape test after the others
# had the rule. One definition: the FHIST span may be computed in exactly
# one function. When the recorded PEM-sibling question is closed it must
# resolve to a CALL here, not to a third spelling — this pin is what makes
# that visible the day it does not.
pin 1 "definitions of the FHIST-span bound (programList[hist-1] .. next)" \
    bash -c "grep -c 'programList\[hist - 1\].instructionPointer' \"${PKG}/programming/forth_fold.c\""

# R9-6 (round 9): FORTH_CONSOLE_ED_YINCR copies upstream showStringEdC47's
# FUNCTION-LOCAL yincr, which no _Static_assert can reference — the header
# claimed assert coverage it structurally cannot have, and the gap was
# mutation-proven silent (compiled yincr 35 -> 30, macro left at 35, whole
# gate GREEN, band and editor overlapping). C14's own class, left open by
# C14's close. This is the source-anchored pin that C14's fix should have
# carried: it greps UPSTREAM for the literal, so a rebase that moves 35
# moves this count and the build stops.
pin 1 "upstream showStringEdC47's yincr = 35, the value forth_console.h copies" \
    bash -c "grep -c 'yincr *= *35' \"${UPSTREAM}/screen.c\""

if [[ -n "${i_out}" ]]; then
  printf '%s' "${i_out}" | sed 's/^/  /'
  flag "an enumerated-site count moved — check every new site against the rule the pin encodes, then re-accept the count in the same commit"
fi

# --- J. Upstream-diff churn (HARD) -------------------------------------------
# D7-5, recommended by the consolidation close-out and again by round 9's
# R9-10, wired here on the third asking. The ten-packet wave drove mechanical
# churn from 51 findings to 0; with the count AT zero this is the cheapest
# regression guard the project has — any future churn is a diff of one
# against a known-empty baseline, instead of waiting for the next full
# upstream-diff-review to notice.
#
# CHURN is hard zero. NEAR is a judged tier: the standing hits were read
# individually by the 2026-08-09b review and cleared. R9-10 landed the
# fnPem hunk's purely additive reshape the same day, which retired the
# tmpChar rename in both its halves — 4 -> 2. The two that remain are the
# appended disjuncts in keyboard.c and softmenus.c, which cannot be spelled
# any other way.
head2 "J. Upstream-diff churn (patch minimality)"
j_scan="${SCRIPT_DIR}/../../.claude/skills/upstream-diff-review/references/patch_churn_scan.py"
if [[ -f "${j_scan}" ]]; then
  j_out=$(python3 "${j_scan}" "${PKG}"/patches/*.patch 2>&1 || true)
  j_churn=$(printf '%s\n' "${j_out}" | grep -c '^\[CHURN\]' || true)
  j_near=$(printf '%s\n' "${j_out}" | grep -c '^\[NEAR\]' || true)
  printf '  %-58s %s\n' "mechanical churn findings (must be 0)" "${j_churn}"
  printf '  %-58s %s\n' "NEAR hits (judged; baseline 2, see comment)" "${j_near}"
  if [[ "${j_churn}" -ne 0 ]]; then
    printf '%s\n' "${j_out}" | grep -A2 '^\[CHURN\]' | sed 's/^/  /'
    flag "patch churn regressed above zero — a hunk is carrying reformatting or a rewrite where an append would do"
  elif [[ "${j_near}" -ne 2 ]]; then
    flag "the NEAR count moved (baseline 2) — read each new hit and re-accept the count in the same commit"
  else
    note "churn 0, NEAR at its judged baseline"
  fi
else
  note "churn scanner not found — skipped (expected at .claude/skills/upstream-diff-review/references/)"
fi

# --- accept / summary --------------------------------------------------------
if [[ "${ACCEPT}" -eq 1 ]]; then
  cat > "${BASELINE}" <<EOF
# forth-core design-audit baseline — regenerated by design-audit.sh --accept
# Raising any of these is a deliberate act: say why in the commit message.
MAX_OVERRIDE_FILES=${nfiles}
MAX_ADDED_LINES=${nadd}
BASE_NO_FORTH=${b_n}
BASE_BIG_BLOCKS=${d_n}
EOF
  printf '\n== baseline written to %s ==\n' "${BASELINE}"
  exit 0
fi

printf '\n== summary ==\n'
if [[ "${findings}" -eq 0 ]]; then
  note "mechanical half CLEAN."
else
  note "${findings} finding group(s) to triage."
fi
note "The script cannot see the judgement half — now do design-docs/forth-core/DESIGN_AUDIT.md Parts 2-3,"
note "especially Part 3 (expired premises), which is what it exists for."
[[ "${findings}" -eq 0 ]] && exit 0 || exit 1
