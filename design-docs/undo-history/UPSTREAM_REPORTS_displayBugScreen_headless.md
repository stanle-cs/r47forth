# Upstream report — displayBugScreen is invisible headless, and the machine keeps running in CM_BUG_ON_SCREEN

**Version:** base `faf9d698c304`. Found 2026-08-25 while root-causing a
wrong SPIRAL program result in the undo-history package bring-up; the
mechanism is upstream's, the trigger was a package-side bug (a formatter
called with a lied-about buffer size).

Paste as its own issue.

---

## A bug screen fired during testSuite changes calcMode silently and the suite reports wrong VALUES three files later

**Files:** `src/c47/error.c` (`displayBugScreen`, :352),
`src/testSuite/testSuite.c` (no hook), consumers throughout.

### The mechanism

`displayBugScreen` renders to the LCD and switches state:

```c
if(calcMode != CM_BUG_ON_SCREEN) {
  previousCalcMode = calcMode;
  calcMode = CM_BUG_ON_SCREEN;
  ...
```

In a headless run (testSuite, `--headless` sim) nothing reaches stdout and
nothing aborts. The suite keeps executing every subsequent case with
`calcMode == CM_BUG_ON_SCREEN`; behavior that branches on calcMode changes
quietly. Observed concretely: a bug screen raised during ulp.txt (a
formatter validation failure inside `longIntegerToAllocatedString`) made
`programs.txt` PGM="SPIRAL" produce deterministically wrong numbers
(Y = 908.9497... instead of 234.1120...) — two files and thousands of test
cases after the actual defect, with zero mention of the bug screen in the
log. The second bug screen is even quieter: the `calcMode !=
CM_BUG_ON_SCREEN` guard makes it a no-op, so only the FIRST defect leaves
state and every later one vanishes entirely.

### Why it matters

A testSuite failure should point at its defect. Here the report pointed at
a healthy program two files away; the real trigger was found only by
bisecting file lists and preview calls under gdb. Any contributor whose
code path can reach `displayBugScreen` (there are ~100 call sites) gets
this debugging experience.

### Suggested direction

In TESTSUITE_BUILD / headless builds, make `displayBugScreen` loud: print
`errorMessage` to stderr and fail the run (abort, or set a flag the harness
checks after every case). The LCD rendering can stay as-is for the
interactive sim.
