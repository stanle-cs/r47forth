# Showcase screenshots (sim, 2026-08-03)

Four LCD captures for the Forth post, taken in the GTK simulator at the
stage-G tree (base 26ec91634). Real 400x240 framebuffer dumps via the
calculator's own `fnScreenDump`, scaled 2x. Not mockups: every screen
came from the real render path (PEM listing via the F15-3 idiom, picker
via the G3/G4 idiom, normal-mode refresh), driven by a temporary test
driver that was reverted after capture.

Both demo programs are permanently tested in the sim on every gate run:

- `test_showcase_program` builds FDEMO+NATADD with the fixture builder,
  runs FDEMO through the real XEQ engine, and asserts every documented
  result: registers 00-16, flag 10, named variables DEMO and PTR, the
  GLOBAL/IMMEDIATE/FORGET effects, and both XEQ bridge directions.
- `test_savings_program` runs SAVE and asserts the six-period schedule
  in registers 00-05 and 22, the drained counters, and GROW surviving
  as a global word.

The captures reproduce those runs step for step, so what the screens
show is what the tests pin.

## The shots

1. `shot1-pem-listing.png` — PEM listing at the top of FDEMO: the
   program header (813 bytes / 43 steps), `LBL 'FDEMO'`, the opening
   `»FORTH` marker, and the first colon definitions as ordinary program
   steps. Cursor on step 0003.
2. `shot2-picker.png` — the tail of FDEMO (FORGET GONE1, the late
   `: LATER 2 * ;` definition, cursor on the closing `FORTH«`) with the
   FWRD picker open below: 13 words on the softkeys. Long names are
   clamped by the renderer, same as on hardware.
3. `shot3-fdemo-results.png` — normal screen after XEQ 'FDEMO':
   Z=17 (CALLWORD through the global PLUS10), Y=42 (named variable
   DEMO), X=15 (5 SUMDOWN, the recursive sum).
4. `shot4-save-balance.png` — after XEQ 'SAVE': Y=1050 (first period,
   R00), X=1340.095640625 (final balance, R22) from six compounding
   periods at 5% on 1000.

## Not yet covered

Simulator captures cannot stand in for the hardware checks the post
needs (R/S interrupting SPIN is hardware-only; forum/DESIGN.md §5 has
the full list). These are illustrations, taken where the sim and the
device provably render alike.
