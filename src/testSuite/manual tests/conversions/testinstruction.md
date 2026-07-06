# Conversion regression test

Tests every conversion pair in `convertPairs[]` (318 items, 3 passes each, 954 tests). Each test converts 1 unit and compares the printed result bit-exact against the authoritative reference `test_all_conversions_expected.tsv`.

## Run

From the repo root, with a current `t47` build (`make simc47 t47`):

```
rm -f test_all_conversions.T47.TSV
./t47 --reset --headless --script './src/testSuite/manual tests/conversions/test_all_conversions.t47'
python3 './src/testSuite/manual tests/conversions/verify_test_all_conversions.py'
```

Expected output: `954 tests compared, 0 failure(s)`. Any other output is a regression; the verifier prints the failing item labels with expected and actual values.

## Notes

- The old TSV must be deleted first: `tsvfn` output APPENDS to an existing file.
- `--reset` gives deterministic state (radix `.`, default flags); do not run against a loaded `backup.cfg`.
- The run output lands in the CWD, not in this folder.

## After adding new conversion items

Follow `CONV_ADDING_ITEMS.md` (repo root). Then:

1. Add one 7-line block per new item per pass to `test_all_conversions.t47` (pattern: `reg X T<num>_<lhs>_to_<rhs>` / `xeq ⎙x` / `clx` / `nim 1` / `xeq ⎙x` / `item <num>` / `xeq ⎙x`). Note: the DSL command is `item`, not `itemfn`.
2. Run the test; it will fail only on the count mismatch.
3. Verify the new items' results independently (compute `1 [lhs] × or ÷ [factor]` at full precision).
4. Bless: copy the verified run output over `test_all_conversions_expected.tsv`.

Reference file last blessed 2026-07-06: 306-item baseline preserved bit-exact from the original harness run, plus items 2834–2841 and 2860–2863 verified from first principles.
