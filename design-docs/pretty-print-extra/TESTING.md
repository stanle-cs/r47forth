# Pretty-print-extra package: testing

The test contract for this package lives with the core package:
design-docs/pretty-print/TESTING.md covers BOTH halves of the PP19
split. The pins grew as one battery. Both packages use one scaffolding.
Both packages run in one suite. That file explains the
driver-to-package map and both gate scripts.

This package's gate is `./packages/pretty-print-extra/build-test.sh`
(pair + full passes). Its drivers are in `prettyExtraTest.c`:

- prettyTestCapture
- prettyTestFormula
- prettyTestEqLang

`testSuite/tests/pretty_extra.txt` drives the three drivers.
prettyTestEqLang must run after the core package's prettyTestVisual.
That driver provides its fixtures. prettyTestEqLang must run before
serialize_cov. serialize_cov performs the reset.
