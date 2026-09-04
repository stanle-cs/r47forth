# pretty-print-extra README audit log

## Source checks

- Package boundary and user controls: `design-docs/pretty-print-extra/DESIGN.md`
- Browser controls: `browsers/prettyBrowser.c` and `prettyExtra.h`
- Menu order: `softmenus.c` and test case FV15
- Equation syntax and results: test cases EQ14 through EQ17
- History capacity: `prettyExtraInternal.h` and `prettyCapture.c`
- Build command: repository `Makefile` and package-manager documentation
- Package base: `packages/pretty-print-extra/.refresh-manifest.json`

## Existing comments and documents

AI Audit ran on all three files under `design-docs/pretty-print-extra`.

- `TESTING.md`: 0 flags
- `DESIGN-HISTORY.md`: 0 flags
- `DESIGN.md`: no hard artifacts

The current design uses bold binding labels and measured tables with checkmarks. It also contains technical inventories and long code paths. AI Audit reports these structures, but they are part of the specification format.

The audit found one vague adjective in the current design. The revision replaced `robust` with the measured statement `remains correct at ~1e−24`.

The fact review also found a stale depth-2 sentence. The evaluator now uses an 8 KB stack guard and a depth-8 backstop. The current design now states that rule.

AI Audit also ran on comments from package-owned new files and added patch lines. The revision removed contrast-tail constructions from operational comments. It also split several long causal comments.

The remaining source-comment flags are divider lines, exact test contrasts, and technical inventories. These are ruled keeps. No source-comment hard artifacts remain.

The fact review found an outdated test comment that said the menu had six entries. The current menu has seven commands. The comment now matches test case FV15.

## README audit rounds

README audit results will be recorded after the first scan.
