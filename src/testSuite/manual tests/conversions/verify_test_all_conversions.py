#!/usr/bin/env python3
# Verify a test_all_conversions run against the authoritative reference.
# Usage: verify_test_all_conversions.py [run.T47.TSV] [expected.tsv]
# Defaults: run = ./test_all_conversions.T47.TSV (CWD), expected = the
# test_all_conversions_expected.tsv next to this script.
# Zero failures = no regression. See testinstruction.md and CONV_ADDING_ITEMS.md.
import os
import sys

def triplets(path):
    lines = [l.rstrip('\n') for l in open(path, encoding='utf-8')]
    if len(lines) % 3 != 0:
        sys.exit(f'{path}: line count {len(lines)} is not a multiple of 3')
    return [(lines[i], lines[i + 1], lines[i + 2]) for i in range(0, len(lines), 3)]

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    run_path = sys.argv[1] if len(sys.argv) > 1 else 'test_all_conversions.T47.TSV'
    exp_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, 'test_all_conversions_expected.tsv')
    run = triplets(run_path)
    exp = triplets(exp_path)
    fails = 0
    if len(run) != len(exp):
        print(f'test count mismatch: run {len(run)} vs expected {len(exp)}')
        fails += 1
    for i, (r, e) in enumerate(zip(run, exp)):
        if r != e:
            fails += 1
            label = e[0].split('\t')[-1]
            print(f'FAIL {i}: {label}')
            print(f'  expected: {e[2]}')
            print(f'  got:      {r[2]}')
    print(f'{min(len(run), len(exp))} tests compared, {fails} failure(s)')
    sys.exit(1 if fails else 0)

if __name__ == '__main__':
    main()
