# Upstream report — fnEigenvalues workspace vs. pool residency

**Version:** base `faf9d698c304` (the `.refresh-manifest.json` base commit of
`packages/undo-history`); the eigen code and matrix.txt are unchanged from
that base. Found 2026-08-24 while bringing up the undo-history package's
ring storage; every measurement below was reproduced deterministically on
the PC testSuite.

Paste as its own issue.

---

## A single resident 8 KiB pool block makes matrix.txt RCL58 fail with "OUT OF MEMORY"

**Files:** `src/c47/mathematics/matrix.c` (`fnEigenvalues`, :1660),
`src/testSuite/tests/matrix.txt` ("039 RCL58: 14x14 integer in, irrational
out", line 997).

### The mechanism

RCL58 runs a 14×14 real eigenvalue decomposition. During the run the
solver's workspace grows until roughly **half the pool is claimed at once**
— the failing run reports:

```
OUT OF MEMORY
Memory claimed: 119280 bytes
Fragmented free memory: 119300 bytes
```

so the allocation pattern needs, at peak, close to the pool's entire
contiguous free space (PC pool = 65534 blocks ≈ 256 KiB). Later measured
precisely: `QR_decomposition_householder` (matrix.c:5413) makes ONE
contiguous `allocC47Blocks(29820)` request (116.5 KiB for a 14×14 real
matrix at 75-digit precision), and the vanilla pool's top free run at that
moment is ~31,236 blocks — about 1,400 blocks (5.6 KiB) of slack. That
slack is the ENTIRE firmware's budget for resident pool allocations, of
any size, anywhere: every resident block below the top run shrinks it
one-for-one regardless of placement. With a vanilla
build that just barely works. Allocate one long-lived 8 KiB block from
`allocC47Blocks` early in the run (anything resident: a package buffer, a
future upstream feature) and the test fails — not because 8 KiB is missing,
but because the resident block splits the free space and the largest
contiguous region no longer fits the peak request.

Freeing the resident block at the moment an allocation fails does **not**
recover: the freed hole sits low in the pool, surrounded by live workspace
blocks, and does not coalesce with the main free region. (Measured — a
free-and-retry-once hook in `allocC47Blocks` was implemented and the test
still failed.)

### Why it matters

- ~119 KiB of transient workspace for a 14×14 real matrix (3.1 KiB of
  data) looks like unbounded intermediate growth — the test's own comment
  says "2001 iterations, needs some kind of acceleration".
- Any future resident allocation, upstream or package-side, silently
  reduces the largest solvable eigenproblem. The failure mode is a distant
  "OUT OF MEMORY", not a hint about residency.

### Suggested direction

Either bound the eigen workspace (free per-iteration temporaries, or
preallocate one reusable block), or treat RCL58's headroom as a documented
invariant so resident allocations are a known trade-off. No package-side
change is requested: the undo-history package sized its resident ring
block (4 KiB, armed at RESET time) inside the measured slack. Until the
workspace is bounded, ~5.6 KiB is the hard ceiling for everything anyone
adds to this firmware's pool.
