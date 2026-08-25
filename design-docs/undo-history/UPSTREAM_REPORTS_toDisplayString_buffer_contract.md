# Upstream report — the *ToDisplayString family's buffer contract is implicit and unevenly enforced

**Version:** base `faf9d698c304`. Found 2026-08-24/25 during undo-history
bring-up: one member smashed a caller's stack frame, another silently
bug-screened, for the same class of caller mistake.

Paste as its own issue.

---

## Callers must guess the required buffer size; one function smashes, one bug-screens, none document it

**Files:** `src/c47/display.c` — `shortIntegerToDisplayString` (:1994),
`longIntegerToAllocatedString` (:2697) and its callers,
`real34ToDisplayString`, `complex34ToDisplayString`.

### The mechanism

The family is written against `TMP_STR_LENGTH`-sized buffers (2560), but
the parameter lists don't say so uniformly:

- `shortIntegerToDisplayString(regist, displayString, ...)` takes **no
  length at all**. With WS=64 and a low base it builds far past 200 bytes
  (measured: a `char buf[200]` caller had its stack frame overwritten with
  the rendered digit groups — SIGSEGV with display text in the return
  addresses). Nothing checks, nothing warns.
- `longIntegerToAllocatedString(lgInt, str, strLen)` **does** validate —
  but its failure path is `displayBugScreen`, which in a headless build is
  invisible and leaves the machine in CM_BUG_ON_SCREEN (see the companion
  report `UPSTREAM_REPORTS_displayBugScreen_headless.md`). The caller's
  string stays "0", execution continues.
- The same class exists inside upstream itself: `testSuite.c:5493/:5501`
  sprintf up to 1999 bytes into `char str[404]` (gcc -Wformat-overflow
  flags it on every build).

So the same caller mistake — a too-small buffer — produces a stack smash,
a silent wrong result, or latent UB depending on which member is called.

### Verified contract

For reference, the package's test battery pins what a
`TMP_STR_LENGTH`-sized buffer guarantees (sentinel-guarded, worst-case
values: all-ones 64-bit base-2 short integer, a >1000-bit long integer, a
34-digit subnormal real): no member writes past `TMP_STR_LENGTH`
(`packages/undo-history`, battery case R10).

### Suggested direction

Give the family one documented rule (buffers are TMP_STR_LENGTH, or pass
and honor an explicit length everywhere), and make
`shortIntegerToDisplayString` take/validate a length. Fixing the
`testSuite.c` sprintf sites closes the in-tree instances (the forth-core
package already carries that fix as a patch).
