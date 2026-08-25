# undo-history — UPSTREAMING.md

What an upstream MR contains beyond this package's patches/+files/:

1. `src/c47/meson.build`: add `'undoHistory.c'` and
   `'browsers/historyBrowser.c'` to the source list. The package must NOT
   patch meson.build — the overlay resolver compiles `files/` entries
   itself, and a meson.build patch would double-compile.
2. Doxygen: `docs/code` scans `src/c47/` only; the new files document
   themselves once moved there. No extra work expected.
3. Wiki/manual: U.HIST, REDO and HCLR item descriptions
   (catalog-reachable, user-assignable, and on the STK menu's free
   slots). The menu-number
   freeze note in softmenus.c is respected — no new menus.
4. Decide backup persistence of the ring (v1 is session-local; DESIGN.md
   §5) with upstream maintainers.
5. Tests ship as-is: testSuite.c driver registrations +
   tests/undo_history.txt + tests/testSuiteList.txt line are themselves
   upstream-shaped changes.
