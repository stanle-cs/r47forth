# Pretty-print-extra package — testing

The test contract for this package lives with the core package:
design-docs/pretty-print/TESTING.md covers BOTH halves of the PP19
split. The pins grew as one battery, they share one scaffolding, and
they run in one suite — that file explains the driver-to-package map
and both gate scripts.

This package's gate is `./packages/pretty-print-extra/build-test.sh`
(pair + full passes). Its drivers are prettyTestCapture,
prettyTestFormula and prettyTestEqLang, in `prettyExtraTest.c`, driven
by `testSuite/tests/pretty_extra.txt`. prettyTestEqLang must run after
the core package's prettyTestVisual (its fixtures) and before
serialize_cov (the reset).
