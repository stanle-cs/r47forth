# GitLab Merge Request Details

**Target Project**: `rpncalculators/c43`  
**Target Branch**: `00.109.04.00b0`  
**Source Branch**: `stanle-cs/c43:mr/pretty-print`  
**GitLab Branch URL**: https://gitlab.com/stanle-cs/c43/-/tree/mr/pretty-print  

---

## MR Title

```text
Add natural display for stack values and native equations
```

---

## MR Description (Copy everything below this line)

## Summary

This merge request adds two-dimensional natural display to C47.

It formats mathematical expressions with horizontal fraction bars, square roots, superscripts, and scalable parentheses. The renderer operates on the stack registers, in full-screen views, and on stored RPN programs.

When an expression is too large or unsupported, the display falls back to standard text.

## Core capabilities

### 1. Stack register display
- **Automatic fraction rendering**: When fraction mode is active, stack lines (X, Y, Z, T) draw fractions with horizontal bars.
- `PPON`: Toggles automatic natural display. Stored in system flag 88 (`PPRTY`). Default is ON.
- **Graceful fallback**: Unsupported objects (plain decimals, strings, matrices, complex numbers) continue to use the stock single-line display.

### 2. Full-screen value view (`PSHOW`)
- `PSHOW`: Draws the value in the X register across the entire screen.
- The renderer selects the largest font that fits the screen dimensions.
- Press `EXIT` to close the view and return to normal calculation.

### 3. Full-screen equation view (`EQSHW`)
- `EQSHW`: Draws the active native equation from the equation catalog in two dimensions.
- Nested fractions, roots, and exponents appear in hierarchical layout.
- If a two-dimensional equation does not fit on the screen, `EQSHW` falls back to linear text.

### 4. RPN program inspection (`VISUAL`)
- `VISUAL`: Decompiles stored RPN bytecode into mathematical notation in the Z and T registers.
- Uses static analysis of instructions without executing the program.
- The value in the X register does not change during inspection.

## Architecture and integration

The package consists of eight files in `src/c47/`.

Upstream hooks are minimal and strictly gated:
- `src/c47/screen.c`: Intercepts register drawing when `PPRTY` is set. If `PPRTY` is clear, execution skips the renderer with zero overhead.
- `src/c47/solver/equation.c`: Connects the `EQSHW` command.
- **Memory safety**: The layout engine uses bounded scratch buffers and does not allocate from the general pool during screen redraws.

## File changes

### New files added
- `src/c47/prettyPrint.h`: Public API declarations.
- `src/c47/prettyInternal.h`: Internal layout types and font metrics.
- `src/c47/prettyLayout.c`: 2D layout engine, bounding box calculation, and LCD glyph blitting.
- `src/c47/prettyValue.c`: Register line layout, fraction formatting, and `PSHOW` rendering.
- `src/c47/prettyEquation.c`: Native equation parser and `EQSHW` rendering.
- `src/c47/prettyVisual.c`: Static bytecode decompiler for RPN programs.
- `src/c47/prettyInfix.c`: Operator precedence, associativity, and delimiter placement.
- `src/c47/prettyTest.c`: Test harness and geometry assertions.

### Upstream files modified
- `src/c47/meson.build`: Adds new files to `c47_src`.
- `src/c47/items.h` & `src/c47/items.c`: Declares `PSHOW`, `PPON`, `EQSHW`, `PTLIN`, and `VISUAL`.
- `src/c47/defines.h`: Adds system flags `PPRTY` (88) and `PTLINE` (89).
- `src/c47/screen.c`: Register rendering hook.
- `src/c47/solver/equation.c`: Equation show hook.
- `src/c47/browsers/flagBrowser.c`: Documents `PPRTY` and `PTLINE` in the flag catalog.
- `src/testSuite/*`: Integrates regression tests (`pretty_print.txt`, `pretty_visual_real.txt`).

## Testing

- Built and verified against upstream branch `00.109.04.00b0`.
- Headless test suite (`./build.upstream-sim/c47-headless -T`) passes with zero failures.
