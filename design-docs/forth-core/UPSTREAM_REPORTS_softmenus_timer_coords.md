# Upstream report — one finding in `src/c47/softmenus.c`

**Version:** base `00.109.04.00b0` (the `.refresh-manifest.json` base commit,
`3de5b4be0`); the call site is unchanged from that base. Found by code
inspection while auditing this package's own override of the same file,
2026-08-11. Not observed on screen — the reachability argument is below and
rests on `GRAPHMODE` being live while the TIMER menu is open.

Paste as its own issue.

---

## `showSoftmenuCurrentPart` draws the TIMER key hints from uninitialized coordinates when `initSoftkeyCoordinates` refuses

**File:** `src/c47/softmenus.c` — the `-MNU_TIMERF && y == 0` branch of
`showSoftmenuCurrentPart` (near :3534 at the base commit).

### The mechanism

`initSoftkeyCoordinates` is a **partial** function and says so by returning
`bool_t`: it refuses in three cases, and one of them returns *before* writing
any of the four out-parameters:

```c
if(GRAPHMODE && xSoftkey >= 2) {           //prevent softkeys columns 3-6 from displaying over the graph
  return false;
}
```

Every other caller in the file honours that contract:

```c
static void showSoftkey(...) {
  int16_t x1, y1, x2, y2;
  if(!initSoftkeyCoordinates(label, xSoftkey, ySoftKey, &x1, &x2, &y1, &y2)) {
    return;
  }
  showKey(label, x1, x2, y1, y2, ...);
}
```

`showSoftkey2` guards it the same way. The TIMER hint branch does not:

```c
int16_t x1, y1, x2, y2;
initSoftkeyCoordinates(tmpq, x, 2, &x1, &x2, &y1, &y2);
showKey(tmpq, x1, x2, y1, y2, vmNormal, false, true, NOVAL, NOVAL, tmpp);
diagonalsOnTop(x1, x2, y1, y2, vmNormal);
```

When the refusal fires, `x1`/`x2`/`y1`/`y2` are indeterminate automatics and
both `showKey` and `diagonalsOnTop` draw from them — a write to whatever
those stack bytes happened to hold, in a function whose whole job is to put
pixels on the LCD. The two guarded callers make the omission look like an
oversight rather than an intended fast path.

### Reachability

`GRAPHMODE` live with the TIMER function menu open, on softkey columns 3-6
(`x >= 2`). The refusal exists precisely so those columns do not paint over
a graph; the TIMER hint row is the one place that then paints anyway.

### Suggested fix

Guard it the way the file already guards the other two call sites:

```c
int16_t x1, y1, x2, y2;
if(initSoftkeyCoordinates(tmpq, x, 2, &x1, &x2, &y1, &y2)) {
  showKey(tmpq, x1, x2, y1, y2, vmNormal, false, true, NOVAL, NOVAL, tmpp);
  diagonalsOnTop(x1, x2, y1, y2, vmNormal);
}
```

This is what this package carries in its own `softmenus.c` override
(`010-softmenus.c.patch`), which is how the finding surfaced: the guard
wraps two existing lines, so the generated patch re-indents them and the
package's minimality scanner flagged the re-indent.
