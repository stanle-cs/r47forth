#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""T5-1: split test_dict_reloc.c into include-parts (single TU preserved).

Moves the capture-area and params-area test functions into
  packages/forth-core/test_capture.part.h
  packages/forth-core/test_params.part.h
which the main file #includes immediately before its closing #endif, so
the compilation unit, its statics, the build, the audit scope and every
doc citation to test_dict_reloc.c stay exactly as they were. Forward
declarations for every moved test are generated into the main file right
before forthDictSelfTest(), so definition order cannot break call sites
(duplicate declarations of an identical prototype are legal C).

Deterministic and validated: refuses to write unless every moved block
was cleanly detected (attached comment .. column-0 closing brace), the
line accounting balances exactly, and no moved definition remains in the
main file. --dry-run DIR writes the three outputs to DIR instead of the
working area.
"""
import os
import re
import sys

MAIN = 'packages/forth-core/test_dict_reloc.c'
PARTS = {
    # v1 (T5-1, landed): capture + params. v2 (2026-08-03): persist +
    # engine catch-all — after v2 the main file holds only helpers,
    # fixtures, accessors, forward decls and the runner. Order matters:
    # first matching area wins, so the catch-all is LAST.
    'capture': ('packages/forth-core/test_capture.part.h',
                re.compile(r'capture|alpha|menu|picker|softmenu|pem|tam|aim|sim_bench')),
    'params':  ('packages/forth-core/test_params.part.h',
                re.compile(r'param|glyph|outer|literal|number')),
    'persist': ('packages/forth-core/test_persist.part.h',
                re.compile(r'freelist|restore|backup|save|reloc|gdict|dirty|spill')),
    'engine':  ('packages/forth-core/test_engine.part.h',
                re.compile(r'.')),
}
DEF_RE = re.compile(r'^static int (test_[a-z0-9_]+)\(void\)$')
RUNNER = 'int forthDictSelfTest(void)'
FINAL_ENDIF = '#endif  // PC_BUILD && FORTH_DEBUG_SELFTEST'

PART_BANNER = """/* {path} — T5 split part of test_dict_reloc.c (2026-08-03).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 */
"""


def area_of(name):
    for area, (_path, rx) in PARTS.items():
        if rx.search(name):
            return area
    return None


def main():
    dry = None
    args = sys.argv[1:]
    if args and args[0] == '--dry-run':
        dry = args[1]
        os.makedirs(dry, exist_ok=True)

    with open(MAIN) as f:
        lines = f.readlines()
    n = len(lines)

    # --- locate moved blocks ------------------------------------------------
    blocks = []  # (start, end_inclusive, name, area)  0-based
    i = 0
    while i < n:
        m = DEF_RE.match(lines[i].rstrip('\n'))
        if m and i + 1 < n and lines[i + 1].startswith('{'):
            name = m.group(1)
            area = area_of(name)
            if area:
                # attached comment: walk back over a /* ... */ block
                start = i
                j = i - 1
                if j >= 0 and lines[j].rstrip().endswith('*/'):
                    while j >= 0 and not lines[j].lstrip().startswith('/*'):
                        j -= 1
                    if j < 0:
                        sys.exit(f'ERROR: unterminated comment walk above {name}')
                    start = j
                # end: first column-0 closing brace at/after the def
                k = i + 1
                while k < n and lines[k].rstrip('\n') != '}':
                    k += 1
                if k >= n:
                    sys.exit(f'ERROR: no column-0 closing brace for {name}')
                blocks.append((start, k, name, area))
                i = k + 1
                continue
        i += 1

    if not blocks:
        sys.exit('ERROR: no blocks detected')
    for (s1, e1, n1, _), (s2, e2, n2, _) in zip(blocks, blocks[1:]):
        if s2 <= e1:
            sys.exit(f'ERROR: overlapping blocks {n1}/{n2}')

    counts = {}
    for _s, _e, _n, a in blocks:
        counts[a] = counts.get(a, 0) + 1

    # --- build outputs ------------------------------------------------------
    moved = set()
    for s, e, _n, _a in blocks:
        moved.update(range(s, e + 1))

    part_lines = {a: [] for a in PARTS}
    for s, e, _n, a in blocks:
        part_lines[a].extend(lines[s:e + 1])
        part_lines[a].append('\n')

    runner_idx = next((idx for idx, ln in enumerate(lines)
                       if ln.startswith(RUNNER)), None)
    if runner_idx is None:
        sys.exit('ERROR: runner not found')
    endif_idx = next((idx for idx in range(n - 1, -1, -1)
                      if lines[idx].startswith('#endif')), None)
    if endif_idx is None or FINAL_ENDIF.split('//')[1].strip() not in lines[endif_idx]:
        sys.exit('ERROR: final #endif not found where expected')

    decls = ['\n', '/* T5 split: forward declarations for the tests that '
             'now live in the\n * .part.h include-parts (see the parts\' '
             'banner comments). */\n']
    for _s, _e, name, _a in blocks:
        decls.append(f'static int {name}(void);\n')
    decls.append('\n')

    stanza = ['\n/* T5 split parts — included last so every '
              'helper and file-scope static above is\n * '
              'visible to them; forward decls before the '
              'runner cover all call sites. */\n']
    for a, (path, _rx) in PARTS.items():
        if a in counts:   # only areas that moved blocks THIS run — a
            # rerun must not duplicate includes landed by a prior split
            stanza.append(f'#include "{os.path.basename(path)}"\n')
    stanza.append('\n')
    generated = decls + stanza

    out_main = []
    for idx, ln in enumerate(lines):
        if idx == runner_idx:
            out_main.extend(decls)
        if idx == endif_idx:
            out_main.extend(stanza)
        if idx not in moved:
            out_main.append(ln)

    # --- validation ---------------------------------------------------------
    moved_total = sum(e - s + 1 for s, e, _n, _a in blocks)
    part_total = sum(len(v) for v in part_lines.values())
    if part_total != moved_total + len(blocks):
        sys.exit('ERROR: part line accounting mismatch')
    joined = ''.join(out_main)
    added = sum(x.count('\n') for x in generated)
    main_newlines = joined.count('\n')
    if main_newlines != n - moved_total + added:
        sys.exit(f'ERROR: main line accounting mismatch '
                 f'({main_newlines} vs {n - moved_total + added})')
    for _s, _e, name, _a in blocks:
        if re.search(rf'^static int {name}\(void\)\n{{', joined, re.M):
            sys.exit(f'ERROR: moved definition {name} still in main')

    # --- write --------------------------------------------------------------
    def dest(path):
        return os.path.join(dry, os.path.basename(path)) if dry else path

    with open(dest(MAIN), 'w') as f:
        f.write(joined)
    for a, (path, _rx) in PARTS.items():
        if a not in counts:
            # An area with no moved blocks this run is a PRIOR split's
            # landed part file — writing it here would clobber it with
            # a bare banner (the 2026-08-03 v2 incident).
            continue
        with open(dest(path), 'w') as f:
            f.write(PART_BANNER.format(path=path))
            f.write(''.join(part_lines[a]))

    print(f'split OK: {counts} moved, {moved_total} lines out, '
          f'main {n} -> {len(out_main)} lines'
          + (f' (dry-run to {dry})' if dry else ''))


if __name__ == '__main__':
    main()
