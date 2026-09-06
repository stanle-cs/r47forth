# undo-history

This archive contains the `undo-history` package. It adds multi-level undo and a full-screen history browser to R47 and C47 firmware.

Stock firmware provides one level of undo. This package lets you press `UNDO` repeatedly to step back through previous calculator states. It also adds a redo command, a history browser, and a history clear command.

The package runs alone. It has no dependency on a sibling package.

## Scope and commands

The package provides three commands and one system flag:

| Command | Catalog Item | Key / Menu | Effect |
| --- | --- | --- | --- |
| `UNDO` | Stock command | `UNDO` key | Press 1 performs stock undo. Presses 2 to n step deeper into previous states. |
| `REDO` | Item 428 | `STK` menu, `FCNS` | Steps forward along the undone history path. A new operation drops the redo trail. |
| `U.HIST` | Item 427 | `STK` menu, `FCNS` | Opens the full-screen history browser. |
| `HCLR` | Item 429 | `STK` menu, `FCNS` | Clears the multi-level history buffer. The stock 1-level undo buffer is untouched. |
| `UHIST` | System flag | `SYSFL` catalog | When set, pressing `f` then `UP` opens the browser instead of `BST`. Default is off. |

The three commands occupy the three open slots in the `STK` softmenu. They are also available in the `FCNS` catalog and can be assigned to keys.

## What each history level stores

Each history level saves a complete snapshot of calculator state:

- The full operational stack (`X`, `Y`, `Z`, `T`, or `X` through `D` in 8-level mode)
- `LASTx` register
- Complete system flags
- Linear regression mode and selection registers
- Statistical accumulation registers (`Σ`) when present

Restoring a level restores all of these elements together.

## The history browser

Select `U.HIST` to open the full-screen browser:

- Press `UP` and `DOWN` to navigate history rows.
- Press `ENTER` to restore the selected state and exit the browser.
- Press `EXIT` or `BACKSPACE` to close the browser without restoring.

The browser identifies entries with these labels:

| Label | Meaning |
| --- | --- |
| Catalog name | Standard command name (such as `+`, `SIN`, `SLVQ`, or `[M]^T`). |
| `(val)` | Direct numeric or string entry from the keyboard. |
| `(now)` | The anchor state before undo navigation began. |
| `*` | The current active level in history. |
| `~` | A skipped oversized state (over 1 KB). Undo steps across this gap. |

## Install and build

Build the simulator with the package active:

```sh
make sim CUSTOM_PKG=packages/undo-history
```

Load alongside other packages with a comma-separated list:

```sh
make sim CUSTOM_PKG=packages/forth-core,packages/undo-history
```

Build for DMCP5 hardware (DM42n):

```sh
make dist_dmcp5r47 CUSTOM_PKG=packages/undo-history
```

## Example: step back through calculations

Enter this sequence on the keyboard:

```text
10 ENTER
20 +
30 +
40 +
```

The stack shows `100`.

1. Press `UNDO`. The stack returns to `60` (`10 + 20 + 30`).
2. Press `UNDO` again. The stack returns to `30` (`10 + 20`).
3. Press `UNDO` again. The stack returns to `10`.
4. Select `REDO` from the `STK` menu. The stack steps forward to `30`.

## Example: restore from the browser

Perform several calculations, then select `U.HIST`:

1. The browser displays numbered rows with their operation labels and results.
2. Press `UP` to highlight an earlier step.
3. Press `ENTER`. The calculator restores the exact stack and flags from that step.

## Memory and footprint

- Flash memory growth: +4.2 KB (4,264 bytes)
- Static RAM: 124 bytes
- History buffer: 4 KB allocated from the calculator memory pool at reset
- Capacity: Approximately 25 levels of real numbers at 4-level stack size (~16 levels at 8-level stack size). Hard limit is 48 levels.

## Limits and behavior

- **Session only**: History clears on calculator reset (`RESET`) or when restoring a backup image.
- **Programs do not capture**: Steps executed inside running R47 programs do not create history entries (same rule as stock undo).
- **Solvers and integration**: Internal solver iterations do not pollute history. One `SLV` or `SLVQ` execution records as a single operation.
- **1 KB per-level limit**: Very large objects (such as oversized matrices) that exceed 1 KB are skipped. The sequence number increments and displays `~`. Undo steps across the gap without error.

## Compatibility and license

The package targets R47 or C47 on DM42n hardware running DMCP5. It is licensed under GPL-3.0-only.
