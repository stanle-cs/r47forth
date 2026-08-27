# pretty-print v0.1 — verified fact sheet (for Stan to write from)

Every number here was measured in this tree at HEAD `8a36388b5`, not
recalled. Where a figure came from a command, the command is given so it
can be re-run. Nothing in this file is prose for publication — it is the
facts the post gets built from, per forum/DESIGN.md §5.3 ("every reference
figure re-verified against source").

## What it is

Natural display of calculations. Two independent engines behind one
package:

- Values and formulas drawn in 2D — stacked fractions with a real bar,
  roots with a vinculum over the radicand, raised exponents, Sigma / Pi /
  integral signs.
- A shadow of the stack that remembers HOW each number was produced, so
  `2 ENTER 3 +` can be shown as `2+3 = 5` and kept in a history you can
  scroll and recall from.

Either would work without the other. They meet only where the second
hands the first a tree to draw.

## The user-facing pieces

Softmenu **PP** (item 217), six keys in this order:

    PSHOW  PHIST  PCLR  EQSHW  PPON  PTLIN

Reached from the DISP menu. EQSHW also appears in the EQN menu.

| name | item | what it does |
|---|---|---|
| PSHOW | 459 | one value or formula, full screen, largest font that fits |
| PHIST | 462 | the formula browser: scroll, pan a wide row with `.d`, ENTER recalls to X |
| PCLR | 461 | empties the formula history |
| EQSHW | 216 | the current equation drawn in 2D, full screen |
| PPON | 460 | toggles natural display (system flag PPRTY) |
| PTLIN | 215 | toggles the live formula on the T line (system flag PTLINE) |

Two system flags, both settable from the SYSFL catalog:

| flag | number | catalog row | default |
|---|---|---|---|
| PPRTY | 50 (0x8071) | item 218 | **ON** |
| PTLINE | 51 (0x8072) | item 219 | **OFF** |

The equation language gains four constructs, argument separator `;`:

    SUM(body;var;from;to[;step])
    PROD(body;var;from;to[;step])
    INTEG(body;var;from;to)
    DERIV(body;var;at[;order])

## Numbers (all measured)

Flash cost of the package, device build, measured as the difference
between two real builds:

    make dmcp5r47 CUSTOM_PKG=packages/forth-core,packages/undo-history
      -> flash 1120776
    make dmcp5r47 CUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print
      -> flash 1146336

**25,560 bytes.**

Fixed working memory, all of it static, no heap and nothing taken from the
calculator's own pool:

| pool | size | what it holds |
|---|---|---|
| `PP_POOL_NODES` | 72 nodes | the layout tree being drawn |
| `PP_TEXT_BYTES` | 512 | text of those nodes |
| `PP_MAX_DEPTH` | 12 | deepest nesting the renderer accepts |
| `PPC_NODES` | 24 | the shadow-stack expression arena |
| `PPC_HIST_BYTES` | 640 | the formula history ring |
| `PPC_HIST_MAX` | 12 | most formulas the ring holds |

Upstream surface: 13 patched files, 36 hunks, 745 added lines, 22 modified
or deleted upstream lines. Command:

    python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
        packages/pretty-print/patches/*.patch

## Behaviour rules worth stating

- Any failure draws nothing and the ordinary display runs unchanged. Type
  it cannot handle, glyph it does not know, formula too big for the space,
  pool exhausted, nesting too deep — all the same outcome.
- Numbers are always formatted by the calculator first and only then
  rearranged, so display modes, rounding and digit grouping are inherited
  rather than reimplemented.
- The history captures manual interactive work only. Program runs, solver
  and integrate sessions are not captured.
- The shadow never guesses. If it cannot describe a value truthfully it
  stores an opaque marker, and any formula containing one is withheld
  from display entirely.
- A formula too wide for the screen pans in the browser. The full-screen
  pager cannot pan, so it omits such a row rather than showing a truncated
  one.

## Install / build (verified commands)

- Unzip into `packages/pretty-print`.
- `make sim CUSTOM_PKG=packages/pretty-print`
- With both siblings:
  `make sim CUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print`
- Device: `make dmcp5r47 CUSTOM_PKG=...` (same variable).

No dependency on either sibling. It builds and runs alone.

## Testing statement (true as of this session)

- Gate is `./packages/pretty-print/build-test.sh`, which refreshes the
  generated patches, builds, and runs the upstream test suite in BOTH
  configurations — the package alone and the package with forth-core and
  undo-history. Green in both at HEAD.
- The package ships its own drivers in the upstream harness: layout
  measurement pins, pixel pins at exact rows, capture traces driven
  through the real key paths, the equation grammar, and the browser.
- Five cross-model audit rounds were run against it. Rounds 3 and 4 fixed
  display and builder defects; round 5 reviewed those fixes and found one
  more. The audit's own exit bar — two consecutive rounds finding nothing
  — has not been met.

## Boilerplate the post must carry (forum/DESIGN.md tiers)

- Upstream base commit the package was generated against:
  `70f8b7db7425422ec80e0342e627ed3e2cfd71a6`
- Target: R47/C47 on DM42n, DMCP5.
- Licence GPL-3.0-only, inherited from c43, stated in the post and in the
  shipped README.
- `COPYING` inside the zip and on the release branch. The repo having it
  at the root is not sufficient.
- Backup and flash-at-your-own-risk disclaimer.
- **Hardware claim: NOT AVAILABLE.** Everything above was verified in the
  simulator. The post may not say the build has been flashed unless Stan
  has flashed it.

## Honest limits (Tier 4 — every known thing that will surprise someone)

- Long integers, strings, matrices, short integers and dates never draw in
  2D. They fall back to the ordinary line.
- A typed literal longer than 30 characters makes the formula withhold
  itself rather than show a truncated value.
- Formula capture stops at 24 arena nodes; a long enough chain degrades to
  a plain value rather than a formula.
- Upstream's second derivative sizes its step as a fraction of x with an
  absolute fallback only at exactly 0. So `x = 0` and `x >= 0.01` are
  exact, and `x <= 0.005` returns a wrong answer silently. Setting the
  named variable `d_d` (the Δ softkey in the derivative menu) fixes it.
  This is upstream's behaviour, not the package's, and an upstream report
  is owed.
- The full-screen pager has no "no formulas" message where the browser has
  one; with an empty history it draws an empty frame.

## Attachments (Stan's call 2026-08-27: collate to TWO)

Captures are now tracked in `forum/screenshots/`, with the full 21-shot
working set under `pretty-print-archive/` (they previously lived only in
`/tmp`). The forum allows three; this post uses two.

1. `pp-attach-1-stack-and-browser.png` — the everyday view and the
   browser, stacked with a rule between them. Neither frame is cropped.
2. `pp-attach-2-nesting.png` — the capacity case, drawn by EQSHW.

Provenance and the tests that pin each behaviour are recorded in
`forum/screenshots/README.md`.
