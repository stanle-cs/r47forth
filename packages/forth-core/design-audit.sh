#!/usr/bin/env bash
#
# forth-core design audit — mechanical half of DESIGN_AUDIT.md.
#
# Cheap enough to run on every stage close. Does NOT build or run the gate.
#
# Usage:
#   ./packages/forth-core/design-audit.sh                # audit
#   ./packages/forth-core/design-audit.sh --accept       # re-baseline to now
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
UPSTREAM="src/c47"
BASELINE="${PKG}/.design-audit-baseline"
ACCEPT=0
[[ "${1:-}" == "--accept" ]] && ACCEPT=1

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
KEY = re.compile(r'forth|fdict|gdict|FCAP|FWRD|ITM_FCALL|param_core|paramCore|aimBuffer', re.I)
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
for root, dirs, files in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('patches', 'files')]
    for fn in files:
        if not fn.endswith(('.c', '.h')): continue
        rel = os.path.relpath(os.path.join(root, fn), pkg)
        u = os.path.join(up, rel)
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
pat = re.compile(r'\b(alloc|free|realloc)C47Blocks\b')
for root, dirs, files in os.walk(pkg):
    dirs[:] = [d for d in dirs if d not in ('patches', 'files')]
    for fn in sorted(files):
        if not fn.endswith('.c') or fn == 'test_dict_reloc.c': continue
        rel = os.path.relpath(os.path.join(root, fn), pkg)
        if os.path.exists(os.path.join(up, rel)):   # upstream file: not ours
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
head2 "F. Generated output vs working area"
if git diff --quiet -- "${PKG}/patches" "${PKG}/files" 2>/dev/null; then
  note "patches/ and files/ clean in git"
else
  flag "patches/ or files/ dirty — run refresh and commit; never hand-edit generated output"
fi

# --- G. Stray shippable content (HARD) ---------------------------------------
head2 "G. Working-area files that would ship as firmware"
g_out=$(python3 - "${PKG}" "${UPSTREAM}" <<'PYEOF'
import sys, os, fnmatch
pkg, up = sys.argv[1], sys.argv[2]
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
        if not os.path.exists(os.path.join(up, rel)) and not fn.endswith(('.c', '.h')):
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
h_out=$(python3 - "${PKG}" <<'PYEOF'
import re, sys, os
pkg = sys.argv[1]
root = os.path.abspath(os.path.join(pkg, '..', '..'))
txt = open(os.path.join(pkg, 'DESIGN.md'), errors='replace').read()
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
PYEOF
)
if [[ -n "${h_out}" ]]; then
  echo "${h_out}" | sed 's/^/  /'
  flag "DESIGN.md cites source that is gone — the authoritative doc describes code that no longer exists"
else
  note "all cited paths resolve"
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
note "The script cannot see the judgement half — now do DESIGN_AUDIT.md Parts 2-3,"
note "especially Part 3 (expired premises), which is what it exists for."
[[ "${findings}" -eq 0 ]] && exit 0 || exit 1
