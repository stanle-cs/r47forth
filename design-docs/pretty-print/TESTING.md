# Pretty-print package — testing

The package is tested in upstream harness 2 (`src/testSuite/`), the same way
undo-history is: coverage drivers registered in `funcTestNoParam[]` with
`coverageDriver = 1`, driven by `testSuite/tests/pretty_print.txt`
(`In:/Func:/Out:` blocks; each driver writes its failure count into X as a
long integer, `Out: EC=0 RX=LonI:"0"`). Drivers live in the package file
`prettyTest.c` under `#if defined(PC_BUILD)` — test-only code, never built
for the device (undo-history's driver shape, packages/undo-history/
undoHistory.c PC_BUILD section).

The gate is `./packages/pretty-print/build-test.sh` (refresh → configure →
build → testSuite, solo AND combined with forth-core + undo-history; exit
status and the success banner both required). An edit to the flat working
area is invisible until `tools/pkg_patch_refresh.py` runs — the gate script
does this first, always.

## Drivers (stage PP1)

- **`prettyTestMeasure`** — engine-level battery against `ppNewRun`/
  `ppNewBox`/`ppMeasure`/`ppParseFraction` (internal API, prettyInternal.h):
  - M1: `FRAC(3,4)`, numeric context, standard children → exact
    `width 12, ascent 25, descent 5` (pins the metric derivation against
    font drift: 13 = −barTopRel(11) + fracGap(2), + digit ink 12).
  - M2: `HBOX[RUN("1",numeric), FRAC]` → `ascent 26, descent 5, width 28`.
  - M3: parser round-trip on a hand-built improper string
    (sup-minus, sup-3, `/`, sub-4) → structure `HBOX[RUN("-"),
    FRAC(RUN("3"), RUN("4"))]`, digit runs mapped to plain digits.
  - M4: parser rejects: plain `3/4` (no sup digits), empty string, two
    slashes, sup digit after the slash.
- **`prettyTestPixels`** — the real inline path: X = 0.75, `FLAG_FRACT` set,
  `CM_NORMAL`/`TI_NO_INFO`, band cleared, `refreshRegisterLine(REGISTER_X)`.
  X-line geometry (baseY 132, baseline B 160, fraction x-span 388..399):
  - P1 (bar, exact rows): rows **149 and 150** lit across 388..399; rows 148
    and 151 clear in that span.
  - P2 (numerator strictly above): gap row 147 fully clear in-span; digit
    ink present in rows 135..146.
  - P3 (denominator strictly below): gap row 152 fully clear in-span; digit
    ink present in rows 153..164.
  - P5 (toggle-off control): same state with the package disabled → row 149
    is NOT a full lit run (upstream's diagonal rendering differs).
- **`prettyTestFallback`** — identity pins for "pretty declines":
  - F1: X = string (unsupported type): band bitmap with package enabled ==
    band bitmap disabled, bit for bit.
  - F2: plain real (`FLAG_FRACT` and `FLAG_IRFRAC` both clear, no exponent
    form): same identity.
  - F3: `displayValueX` parity: fraction rendered pretty vs upstream —
    `displayValueX` strings byte-identical (pins the builder-first rule).

### PP2 additions

- `prettyTestMeasure`: M5 radical ascent 29/descent 0 (numeric radicand);
  M6 exponent split (base text `1.5·10`, exponent `40`, SUP node); M7
  IRFRAC `√3/2` → RAD inside a FRAC numerator, denominator `2`; M8
  `3×π/4` accepted, paren-power `(π²)` and bare `π` decline.
- `prettyTestPixels` P4: X = √2 (34-digit) with IRFRAC on — vinculum rows
  131-132 (baseline 160, numeric radicand top 134, radGap 1, vincThick 2),
  gap row 133 clear and radicand ink present, both probed in the
  radicand's columns only (the sign's own diagonal legitimately crosses
  the gap row on the left).
- **`prettyTestShow`** — PSHOW: frame lines rows 20/168 full width;
  centered stacked 3/4 at the numeric/numeric rung (bar rows 93-94,
  midpoint within 40 px of center); `SCRUPD_MANUAL_STACK` +
  `screenHoldsDrawnPixels` armed; string X falls back to the ordinary
  SHOW (temporaryInformation leaves TI_NO_INFO).

## Mutation pins (run each separately, battery green between restores)

All six demonstrated red on 2026-08-26 (targeted single-file battery), green
after restore.

| id | mutation | went red |
|---|---|---|
| MUT-1 | delete the `lcd_fill_rect` bar paint in `ppPaint`'s FRAC arm | P1 |
| MUT-2 | shift the bar two rows (barTopRel + 2) | M1, M2, P1, P2, P3 |
| MUT-3 | swap the numerator/denominator relBase formulas | M1, M2 — NOT the P-pins: P2/P3 assert ink presence per band and are shape-blind to which digit sits where; the measure pins carry this mutation |
| MUT-4 | ignore the package toggle in `prettyTryRegisterLine` | P5 |
| MUT-5 | swap the sup/sub digit classification in the parser | M3 |
| MUT-6 | stray paint after the toggle gate, before the type gate declines | F1, F2 — the paint-then-decline failure mode the identity pins exist for |
| MUT-7 | delete the vinculum fill in `ppPaint`'s RAD arm (PP2) | P4 |
| MUT-8 | exponent base loses the plain `10` append (PP2) | M6 |
| MUT-9 | IRFRAC accepts bare constant names (PP2) | M8 |
| MUT-10 | PSHOW does not arm the manual-paint protocol (PP2) | S3 |

Two lessons from the first mutation run, kept for honesty: (a) the original
MUT-4 ("drop the dtReal34 type gate") stays GREEN — a non-real register's
payload read as real34 is NaN-class garbage, so the adjacent range check
declines anyway and the mutation is masked; the type gate stays in the code
as belt-and-braces, but its pin-of-record is the F-identity contract via
MUT-6. (b) A stray paint placed BEFORE the toggle gate is invisible to the
F-pins (both compared renders paint it) — the identity pins only see work
that the disabled path skips.

A pin that stays green under its mutation is decoration and gets deleted or
fixed — the forth-core 2026-07-21 rule applies unchanged.

## Blast radius and measurements

- Keep the pre-change testSuite log; diff sorted `PASS:` lines after.
- BSS delta measured at every stage gate (`size` on the sim binary,
  before/after); the §8 budget in DESIGN.md is the ceiling.
- Flash: `make dmcp5r47 CUSTOM_PKG=packages/forth-core,packages/undo-history,packages/pretty-print
  CUSTOM_PKG_RECONFIGURE=1` delta recorded in the stage commit (without the
  reconfigure flag the number is the previous tree's).
- Visual confirmation via the run-sim skill's capture driver (not a gate;
  the pixel pins are the gate).
