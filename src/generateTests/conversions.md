# Conversion regression set

`src/testSuite/tests/conversions.txt` tests every conversion pair in `convertPairs[]`: convert
1 unit, compare register X against a value computed from first principles (the values are
exact; the testSuite compares to its tolerance of at least 30 significant digits). 318
items, repeated in identical passes to catch state contamination (the PASSES constant in
`generateTests.py`, currently 2, so 636 tests). It runs with the standard suite (`make test` /
`make repeattest`); it is listed in `testSuiteList.txt`.

The file is GENERATED at build time by `generateTests.py`, hooked into meson exactly like
generateConstants: any change to the constant definitions or item tables regenerates it on
the next testSuite build. Do not edit it by hand.

Each test dispatches through the real user chain: the `Item:` directive in testSuite.c calls
`reallyRunFunction(itemNr, indexOfItems[itemNr].param)`. `ITM_` names are resolved at runtime
from `src/c47/items.h`, so item renumbering cannot silently break the file.

## How the values are computed

`generateTests.py` reads only the C sources (`generateConstants.c`, `defines.h`,
`conversionUnits.c/.h`, `items.c/.h`) and computes every conversion itself with Python's
decimal module at the calculator's own precisions: each operation in a 39-digit context
(`ctxtReal39`), result rounded half-even to the 34-digit register (real34). Calculator
output is never read, so the testSuite checks the calculator against an independent
computation of the same definitions. Every formula family carries a comment citing the C
it mirrors.

Not covered: the constant values themselves. Test and calculator share `generateConstants.c`,
so a wrong literal there passes both. Verifying the constants needs an independent source.

`generateTests.py --check` compares computed values against the existing files without writing.

## SI round trips (conversionsSI.txt)

Custom conversion pairs execute through `runConversionToSI` / `runConversionFromSI`
(conversionUnits.c:757): the unity conversion plus a 10^exp scale, and their inverses.
`conversionsSI.txt` (also generated) runs both halves per configurable item through the
testSuite wrappers `covConvToSI` / `covConvFromSI` with input 1, compared against
values computed here. `UT_NOT_CONFIGURABLE` items are skipped: custom pairs cannot be formed
on them.

Before writing anything, the generator checks the `convertPairs[]` table itself and aborts
the build with a fix instruction per problem: partner rows must point at each other, a unity
must be a same-type item whose own unity is `ITM_NULL` (the C executes exactly ONE hop), and
`UT_NOT_CONFIGURABLE` rows must have `ITM_NULL` unity. A unit consistency check then verifies
that items converting FROM the same displayed unit reach the same SI value through their own
factor + unity + exponent route (to 1e-4 relative: novelty units use short constants;
reciprocal fuel/energy types are excluded, their shared-source values legitimately differ).
The display name is ground truth the factor table does not know, so this catches wrong
factors, swapped multiply/divide, wrong unity and wrong exponent independently.

## Adding a conversion pair - code changes

| File | What changes |
|---|---|
| `src/generateConstants/generateConstants.c` | `generateConstant(...)` for each new factor (`EXACT` if it terminates, else `APPROX` with 39 digits) |
| `src/c47/conversionUnits.h` | `constFactorXxx` enum entry; `UT_Xxx` unit type if a new physical type |
| `src/c47/conversionUnits.c` | `conversionFactors[]` entry; two `convertPairs[]` rows; bump `NUM_CONVERT_PAIRS`; list in `MimFunctionsType3Conv[]` |
| `src/c47/items.h` | `ITM_XtoY` / `ITM_YtoX` defines (use the CONV spare numbers; bump `LAST_ITEM` only when spares run out) |
| `src/c47/items.c` | Two `UNIT_CONV(factor, multiply/divide, ...)` lines at those item numbers |
| `src/c47/softmenus.c` | Add both items to the relevant `menu_ConvXxx[]` array (18-slot screens, `ITM_NULL` fills gaps) |

The tests come for free: the next build regenerates `conversions.txt` with the new pair's
predicted values, and the testSuite fails if the calculator disagrees with the prediction.
Check the git diff of `conversions.txt`: only the new tests may appear.

Rules that catch people out:

- Polarity: `multiply` means `result = X x factor` converts left unit to right unit; the
  partner item uses `divide` with the SAME factor. Check by computing `1 [left] [op] [factor]`
  by hand.
- `convertPairs[]` unity field: `ITM_NULL` when the item's OUTPUT is the SI base; otherwise
  the item that converts the output one hop towards SI. `UT_NOT_CONFIGURABLE` items always
  use `ITM_NULL`.
- If the new pair needs a new FORMULA (not a plain factor), `generateTests.py` must learn it
  too: add a predictor mirroring the new C function, or generation aborts with "no predictor".
- First build after adding a constant fails against the stale generated header:
  `install -C build.sim/src/generateConstants/constantPointers{.h,.c,2.c} src/generated/`
  then rebuild.
- If a generated `.txt` is deleted without touching any generator dependency, ninja considers
  the stamp fresh and rebuilds nothing; recreate it with `python3 src/generateTests/generateTests.py`.
