# Upstream report — one finding in `res/combo/R47_combo.py`

**Version:** base `00.109.04.00b0` (the `.refresh-manifest.json` base
commit, `3de5b4be0`); the file is unchanged from that base at the time of
writing. Found while packaging an R47 release build from a two-part tag,
2026-08-10; verified by code inspection against the `dist_dmcp5r47` path
in `Makefile:483`.

Paste as its own issue.

---

## `R47_combo.py` raises `IndexError` for any firmware version with exactly two dot-separated parts

**File:** `res/combo/R47_combo.py` (the `Fw_Name` block), reached from
`Makefile:483` — `cd $(DMCP5R47_DIST_DIR) && python3 R47_combo.py $(VERSION)`.

### The mechanism

The script derives the 12-character firmware name stamped into the combo
image's `SMFW` tail from its one argument:

```python
splitFwName = FwName.split('.', 2)
if(len(splitFwName) < 2):
  name = FwName
else:
  name = splitFwName[2]
```

`str.split('.', 2)` returns **at most three** parts, so the length is 1,
2 or 3 — and the `else` arm indexes `[2]` for both 2 and 3. A version
string with exactly one dot lands in the `else` arm with a two-element
list and the script dies:

```
Traceback (most recent call last):
  File "R47_combo.py", line 43, in <module>
    name = splitFwName[2]
IndexError: list index out of range
```

The guard reads as "a name with no dots has no third field", which is
true; the case it misses is the name with one dot, which also has no
third field. The output `.bin` is left on disk without its 24-byte tail,
so the failure is not only noisy — it is a half-written artifact.

### Reproduction

```
make dist_dmcp5r47 CI_COMMIT_TAG=v0.3
```

Any `CI_COMMIT_TAG` of the form `A.B` reproduces (`v0.3`, `1.0`,
`2026.08`). `A`, `A.B.C` and `A.B.C.D` all succeed. Untagged builds are
unaffected: `Makefile:354` sets `VERSION` from
`git describe --match=NeVeRmAtCh --always --abbrev=8 --dirty=-mod`, whose
bare hash contains no dot and takes the `len < 2` arm. That is why the
default build documented for users has never hit this.

### Suggested fix

Take the third field only when there is one:

```python
splitFwName = FwName.split('.', 2)
name = splitFwName[2] if len(splitFwName) == 3 else FwName
```

This preserves today's behaviour for every version string that currently
works — three-or-more-part names still stamp the trailing field, no-dot
names still stamp the whole string — and gives the two-part name the same
whole-string fallback instead of a traceback. The 12-character truncation
below it is unchanged.

---

## Package-side note (not part of the issue text)

This fix was briefly carried in-tree as `f2bcf882c` and reverted in
`4a2d9e771`: `res/` is outside the external package system's reach (it
covers `src/c47` plus the `src/` sibling roots and nothing else), so
there is no patch that can carry it, and a direct edit to an upstream
file is exactly what the package discipline exists to prevent. Until
upstream takes the fix, the workaround for an R47 release build is a
version string with one part or three (`0.3.0`, not `0.3`).
