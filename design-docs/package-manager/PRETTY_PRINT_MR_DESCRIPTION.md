# Add natural display for stack values and native equations

## Summary

This change adds two-dimensional natural display to C47.

The code draws mathematical expressions with horizontal fraction bars and exponents. It operates on the stack, in full-screen views, and on stored RPN programs.

When an expression is too large, the display falls back to standard text.

## Core capabilities

### Stack display

- Fractions on the stack draw with horizontal fraction bars when fraction mode is active.
- `PPON` toggles automatic natural display.
- System flag 88 (`PPRTY`) stores this setting. The default is on.
- Plain decimal values, strings, or matrices continue to use the standard display.

### Full-screen value view (PSHOW)

- `PSHOW` draws the value from the X register across the entire screen.
- The renderer selects the largest font that fits the screen dimensions.
- Press `EXIT` to close the view.

### Full-screen equation view (EQSHW)

- `EQSHW` draws the current native equation from the equation catalog in two dimensions.
- Nested fractions and roots appear in hierarchical layout.
- When an equation does not fit on the screen, `EQSHW` falls back to linear text.

### Program inspection (VISUAL)

- `VISUAL` reconstructs mathematical formulas from a stored RPN program in the Z and T registers.
- It analyzes the stored bytecode statically. It does not execute the program.
- The value in the X register does not change.

## Architecture and hooks

The package consists of eight files in `src/c47/`.

Upstream hooks are minimal:
- In `src/c47/screen.c`, a check intercepts register drawing when `PPRTY` is set. If `PPRTY` is clear, execution skips the renderer.
- In `src/c47/solver/equation.c`, a hook connects the `EQSHW` command.
- The layout engine uses bounded memory buffers. It does not allocate from the general pool during screen updates.

## File changes

### New files added
- `src/c47/prettyPrint.h`: public API definitions.
- `src/c47/prettyInternal.h`: layout types and metrics.
- `src/c47/prettyLayout.c`: two-dimensional layout engine and LCD glyph blitting.
- `src/c47/prettyValue.c`: register line layout and `PSHOW` full-screen rendering.
- `src/c47/prettyEquation.c`: native equation parser and `EQSHW` rendering.
- `src/c47/prettyVisual.c`: static bytecode decompiler for RPN programs.
- `src/c47/prettyInfix.c`: operator precedence and delimiter logic.
- `src/c47/prettyTest.c`: test harness and geometry assertions.

### Upstream files modified
- `src/c47/meson.build`: adds the new files to `c47_src`.
- `src/c47/items.h` and `src/c47/items.c`: declares items for `PSHOW`, `PPON`, `EQSHW`, `PTLIN`, and `VISUAL`.
- `src/c47/defines.h`: adds system flags `PPRTY` (88) and `PTLINE` (89).
- `src/c47/screen.c`: register rendering hook.
- `src/c47/solver/equation.c`: equation show hook.
- `src/c47/browsers/flagBrowser.c`: documents `PPRTY` and `PTLINE` in the flag catalog.
- `src/testSuite/testSuite.c`: registers the test battery.
- `src/testSuite/tests/testSuiteList.txt`: adds test scripts.
- `src/testSuite/tests/pretty_print.txt`: test script for stack and equation display.
- `src/testSuite/tests/pretty_visual_real.txt`: test script for program visualization.

## Testing

- Built and verified against upstream branch `00.109.04.00b0`.
- The headless test suite `./build.upstream-sim/c47-headless -T` passes all tests with zero failures.
