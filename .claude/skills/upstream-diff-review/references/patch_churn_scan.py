#!/usr/bin/env python3
"""Churn scan for generated package patches.

Finds modified upstream lines that carry no behavior change — the class the
upstream-diff review exists to catch. Three tiers, two mechanical and one
judged:

  WS-ONLY       deleted/added pair identical after whitespace strip.
                Always a violation: re-indent, alignment, tab churn.
  COMMENT-ONLY  pair identical after stripping a trailing // or /* comment
                from the added line (then whitespace). Always a violation:
                the comment belongs on its own added line so the upstream
                line stays byte-identical.
  NEAR          pair within edit distance 3 after whitespace strip.
                A HINT, not a verdict — judge each: `(x)` vs `x` paren
                churn is a violation; `tmpChar` -> `tmpChar4` is a real
                rename and fine.

Also prints per-patch metrics (adds / dels / hunks / pairs) so the review
has its numbers from one run.

Usage: python3 patch_churn_scan.py <pkgdir>/patches/*.patch
Exit status: 1 if any WS-ONLY or COMMENT-ONLY hit, else 0.
"""

import re
import sys


def norm_ws(s):
    return re.sub(r'\s+', '', s)


def strip_trailing_comment(s):
    # strip a trailing // ... or /* ... (possibly unclosed: wrapped comment)
    s = re.sub(r'//.*$', '', s)
    s = re.sub(r'/\*.*?(\*/)?\s*$', '', s)
    return s


def edit_distance(a, b, cap=4):
    if abs(len(a) - len(b)) > cap:
        return cap + 1
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        best = cap + 1
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[-1] + 1,
                           prev[j - 1] + (ca != cb)))
            best = min(best, cur[-1])
        if best > cap:
            return cap + 1
        prev = cur
    return prev[-1]


def scan_patch(path):
    adds = dels = hunks = 0
    findings = []          # (tier, old, new)
    hunk_dels, hunk_adds = [], []

    def flush():
        added_norms = {norm_ws(a): a for a in hunk_adds}
        for d in hunk_dels:
            nd = norm_ws(d)
            if not nd:
                continue
            if nd in added_norms and added_norms[nd] != d:
                findings.append(('WS-ONLY', d, added_norms[nd]))
                continue
            for a in hunk_adds:
                na_stripped = norm_ws(strip_trailing_comment(a))
                if na_stripped and na_stripped == norm_ws(strip_trailing_comment(d)) \
                        and norm_ws(a) != nd:
                    findings.append(('COMMENT-ONLY', d, a))
                    break
            else:
                for a in hunk_adds:
                    na = norm_ws(a)
                    if na and na != nd and 0 < edit_distance(nd, na) <= 3:
                        findings.append(('NEAR', d, a))
                        break
        hunk_dels.clear()
        hunk_adds.clear()

    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n')
            if line.startswith('@@'):
                flush()
                hunks += 1
            elif line.startswith('+') and not line.startswith('+++'):
                adds += 1
                hunk_adds.append(line[1:])
            elif line.startswith('-') and not line.startswith('---'):
                dels += 1
                hunk_dels.append(line[1:])
    flush()
    return adds, dels, hunks, findings


def main(argv):
    hard = 0
    print(f"{'patch':<48} {'adds':>5} {'dels':>5} {'hunks':>5}")
    all_findings = []
    for path in argv:
        adds, dels, hunks, findings = scan_patch(path)
        name = path.split('/')[-1]
        print(f"{name:<48} {adds:>5} {dels:>5} {hunks:>5}")
        for tier, old, new in findings:
            all_findings.append((name, tier, old, new))
            if tier != 'NEAR':
                hard += 1
    print()
    for name, tier, old, new in all_findings:
        print(f"[{tier}] {name}")
        print(f"  - {old.strip()}")
        print(f"  + {new.strip()}")
    if hard:
        print(f"\n{hard} mechanical churn finding(s) (WS-ONLY / COMMENT-ONLY).")
    return 1 if hard else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
