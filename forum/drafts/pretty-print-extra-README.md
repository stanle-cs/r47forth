# pretty-print extra

This archive contains only the `pretty-print-extra` package. It requires the separate `pretty-print` core package. You need both package archives.

The extra package records manual calculations and shows their formulas. It also adds the equation constructs and the `PP` softmenu.

## Extra-package scope

The extra package adds these features to the core package:

- Calculation capture
- Formula history
- The `PHIST` browser
- The `PCLR` command
- A live formula on the T register line
- The `SUM`, `PROD`, `DERIV`, and `INTEG` equation constructs
- The `PP` softmenu under `DISP`

The core package supplies the drawing engine. It also supplies `PPON`, `PSHOW`, `EQSHW`, `PTLIN`, and `VISUAL`.

## Install

Run these commands from the root of the c43 source tree:

```sh
mkdir -p packages/pretty-print packages/pretty-print-extra
unzip pretty-print.zip -d packages/pretty-print
unzip pretty-print-extra.zip -d packages/pretty-print-extra
```

Always list the core package before the extra package.

Build the R47 simulator:

```sh
make simr47 CUSTOM_PKG=packages/pretty-print,packages/pretty-print-extra
```

Build the C47 simulator:

```sh
make sim CUSTOM_PKG=packages/pretty-print,packages/pretty-print-extra
```

Build the R47 image for DMCP5:

```sh
make dmcp5r47 CUSTOM_PKG=packages/pretty-print,packages/pretty-print-extra
```

## Open the PP menu

Open `DISP`. Select `PP`.

The first row contains these commands:

```text
PSHOW  PHIST  PCLR  EQSHW  PPON  PTLIN
```

Press `f` to select `VISUAL` on the shifted row.

`PSHOW`, `EQSHW`, `PPON`, and `VISUAL` belong to the core package. This package supplies the menu placement and the history functions.

## Example: browse formula history

Enter this calculation:

```text
2 ENTER 3 + 5 ENTER 6 +
```

Select `PHIST`. The browser shows `5 + 6` as the current formula. It also shows `2 + 3 = 5` in history.

Use these controls:

- Press `UP` or `DOWN` to select a row.
- Press `.d` to pan a wide selected row.
- Press `ENTER` to copy the selected result to X.
- Press `EXIT` or `BACKSPACE` to close the browser.
- Press `PHIST` again to open the full-screen pager.

Each additional `PHIST` press selects the next pager page. Select `PCLR` to clear the stored history.

## Example: show the live formula

The `PTLINE` system flag is off after a reset. Select `PTLIN` to set `PTLINE`.

Continue a manual calculation. The open formula uses the screen location of the T register line. The T register value does not change.

The normal T value appears when no formula is open. The normal T value also appears when the formula does not fit.

## Equation constructs

Use a semicolon to separate the arguments:

```text
SUM(body;var;from;to[;step])
PROD(body;var;from;to[;step])
DERIV(body;var;at[;order])
INTEG(body;var;from;to)
```

The square brackets mark optional text. Do not enter the brackets.

The default step is 1. The default derivative order is 1.

Use all-uppercase or all-lowercase construct names. Mixed-case names are not valid.

These tested equations show the syntax:

```text
SUM(X^2;X;1;10)
PROD(X;X;1;5)
DERIV(X^3;X;2)
INTEG(X^2;X;0;1)
```

The equations return these results in the same order:

- 385
- 120
- 12
- Approximately 1/3

The constructs can contain other constructs. Very deep expressions stop with an error when they reach the evaluator guard.

## Limits and fallback behavior

The capture engine follows manual keyboard calculations. Program execution does not add history. Solver and integrator sessions also do not add history.

The history stores at most 12 formulas in a 640-byte ring. Large entries can reduce the number that fits. The package removes the oldest entry first.

The package does not show a formula when it cannot describe the value correctly. These values can cause this result:

- Strings
- Matrices
- Oversized values

The browser displays `(cannot draw)` when a stored row cannot use the available fonts or height. A wide selected row can pan horizontally.

`PCLR` clears the stored history. A calculator reset clears the capture state and history.

`DERIV` uses the upstream derivative engine. A second derivative can be inaccurate at a small nonzero evaluation point. The exact threshold probably depends on the equation.

So set the named variable `δ_d` when this occurs. The derivative menu provides the delta control for this value.

## Compatibility and license

The package targets R47 or C47 on DM42n with DMCP5. It uses c43 commit `80533eac4e12cf493398c4a09f53368e2034da3c` as its patch base.

I built and tested this package in the simulator. I did not test the package on a physical DM42n. Create a calculator backup before installation.

The package uses GPL-3.0-only. The archive includes `COPYING`.
