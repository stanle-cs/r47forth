# Stage K — keys mode inside Forth capture (design, pre-packet)

**Status: STAGE COMPLETE 2026-08-04. K1-K4 all landed on forth-core/stage-k
(eb6c36faf, 4358f832e, 1ac2f6653, b3f83e562 + packet commits); E10-E15
folded into DESIGN.md 8.4.1; flash 1108000 -> 1108360 (+360 B for the
stage; +584 B including the pre-K FIX series); arena unchanged. Every
packet implemented by opus subagents, zero architect rescues; amendments
K1-A..K4-A record the four architect spec defects the implementers
caught. Branch unmerged/unpushed — owner's call.** Authority: DESIGN.md §8.4 rules E0–E9 stay normative and
unchanged; this stage adds rules E10–E15 (drafted here, folded into §8.4 at
landing). Amendment trail goes to DESIGN-HISTORY.md as usual.

## Problem (owner statement, 2026-08-04)

Typing in Forth capture is alpha-style input, which is right for writing word
names — but keying in R47 buttons isn't available: while the capture holds
FLAG_ALPHA, `determineItem` resolves the whole keyboard through the AIM
columns [keyboard.c:1683-1694], so the physical SIN key produces a letter and
every native function is reachable only through catalogs (F6-3), the FWRD
picker, or letter-by-letter typing.

## Owner rulings (2026-08-04)

| # | Ruling |
|---|--------|
| K-R1 | Architecture: **resolution overlay** (option B). Capture stays OPEN; a transient bit switches key resolution to the normal columns. The persistent-suspension design (option A) is rejected — it opens the suspension-seam trap surface the F6 ledger documents. |
| K-R2 | Toggle gesture: **the ALPHA gesture** (f + the key whose normal-column `fShifted` is ITM_AIM; key 51 on all four R47 layouts). Cost accepted: the Greek α glyph is no longer typeable via that gesture during Forth capture (other routes: pre-work trace T6). |
| K-R3 | Softkey row in keys mode: **underlying menus** — pop the ALPHA menu; the row shows what normal PEM would show. The visible menu swap is the primary mode indicator; no statusBar.c override (footprint discipline). |
| K-R4 | SST/BST/R-S in keys mode: **upstream PEM behavior**, reconciled with the landed commit-before-navigate precedent (Up/Down during capture commit the line first — pinned by `test_forth_capture_navigation`). Exact reconciliation specced after pre-work trace T3. |

## Architecture

One new transient UI bit, package-private:

```c
// forth_capture.h — alongside forthCap state; NEVER persisted.
// Meaningful only while forthCap.state != FCAP_CLOSED and FLAG_ALPHA is set.
bool_t forthKeysMode;   // false = alpha input (today's behavior), true = keys
```

Guardrail compliance (§8.4 closing guardrail): this is not a persisted byte
and not an entry-mode record — keypad *entry* state is still derived from
program bytes at the cursor. `forthKeysMode` only selects which key columns
`determineItem` reads while a capture is already open, exactly as
`alphaCase`/`FLAG_NUMLOCK` select sub-layouts today. It dies with the capture
(reset discipline below) and is reset at the same lifecycle seams as
`forthCap` state.

**FLAG_ALPHA stays SET for the whole life of keys mode.** That is the load-
bearing choice: E0's first arm keeps diverting every dispatched item into
`pemAlpha`, so the landed text sinks keep working unmodified:

- Direct items (`CAT_FNCT|PTP_NONE`) → F6-3 arm → `forthCapInsertName(itemCatalogName)` → recommit tail. [manage.c:1044-1050]
- Parameterized items → `tamEnterMode` → F6-2 suspend → F6-4 fold via `decodeOneStep` → canonical text. [tam.c:1180-1182, manage.c:1164-1215]
- Forth words → FWRD picker path, unchanged. [keyboard.c:987-993]

The only resolution-layer change: in `determineItem`, when
`calcMode == CM_PEM && FLAG_ALPHA && tam.function == ITM_FORTH && forthCapIsOpen() && forthKeysMode`,
select `primary/fShifted/gShifted` instead of the AIM columns. Everything
downstream is the existing dispatch chain (btnReleased → runFunction →
items.c PEM gates → insertStepInProgram E0 → pemAlpha). The catalog path
proves that chain end-to-end today; keys mode only changes which item ids
enter it.

## Rules (draft E10–E15, to be folded into DESIGN.md §8.4 at landing)

- **E10 (toggle-in).** During an open Forth capture in alpha input, the key
  event whose *normal-column* resolution is ITM_AIM (f + key 51 on R47)
  resolves to ITM_AIM instead of its AIM-column item (ITM_alpha). The
  ITM_AIM arm in `insertStepInProgram`, when `forthCapIsOpen()`, flips
  `forthKeysMode = true`, pops `-MNU_ALPHA` and any alpha submenu
  (bounded drain, per the E1 teardown rule — never spin-on-predicate),
  and triggers a redraw. Layout-independent spec: the condition keys on the
  normal-column `fShifted == ITM_AIM` of the pressed key, not on a key
  number.
- **E11 (toggle-out, symmetric).** In keys mode the same gesture resolves
  through the normal columns to ITM_AIM natively; the same arm flips
  `forthKeysMode = false` and re-establishes `showSoftmenu(-MNU_ALPHA)`.
  E6 (ITM_AIM outside an open capture re-enters Forth capture) is
  unaffected: the toggle arm is gated on `forthCapIsOpen()`.
- **E12 (keys-mode routing).** With the bit set, a resolved item routes as:
  1. `processKeyAction` CM_PEM specials PR/OFF: native (unchanged).
  2. SST/BST: **native with no new code** (per trace T3 result). `fnSst`/
     `fnBst`'s own alpha-close branch (`aimBuffer[0]!=0 && FLAG_ALPHA →
     pemCloseAlphaInput()`, nextStep.c:344/417) already commits-and-closes
     the capture before navigating — the identical mechanism Up/Down use
     today, pinned by `test_forth_capture_navigation`. Closing clears the
     bit (E14). Edge to pin in K2: SST/BST with an EMPTY capture line skips
     that branch (aimBuffer[0]==0) — the packet must abort the placeholder
     first so no navigation ever leaves FCAP_OPEN behind.
     R/S: upstream PEM inserts a STOP step — but with FLAG_ALPHA set the E0
     divert would instead route ITM_STOP into the F6-3 arm and type the
     text `STOP `, which the resolver REJECTS (flow-reject class) — broken
     by construction. Spec: R/S in keys mode first commits the line via
     `pemCloseAlphaInput()`, then performs the native
     `addStepInProgram(ITM_STOP)` — a real STOP step inside the region
     (legal: native steps inside regions are landed E2-class semantics).
  3. Digits/period/exponent: **no divert needed** (trace T2 result). They
     are `addItemToBuffer`-class items; `processKeyAction`'s CM_PEM case
     never matches them, and `pemAlpha`'s character arm already inserts
     them as text under the recommit tail. The column swap alone is the
     whole change. One packet check: ITM_EXPONENT inserts its
     `itemSoftmenuName` — verify that spelling is ASCII `e`/`E` (the only
     exponent forms the number grammar accepts) or map it; also confirm
     `numlockReplacements()` is inert for digit items.
  4. EXIT: press-time `fnKeyExit` as today; the CM_PEM arm gains one new
     first rung: keys mode set → clear it, restore `-MNU_ALPHA`, consume
     the press. The E8 ladder becomes: keys→alpha→submenu→ALPHA-menu→
     drop-keypad→leave-PEM, still one level per press.
  5. ENTER/BACKSPACE: resolve to the same items in both column sets; capture
     semantics unchanged (commit gate, glyph delete, empty-abort).
  6. Everything else: `runFunction` → the landed F6-3/F6-4/picker sinks.
- **E13 (TAM round-trip).** `forthKeysMode` is snapshotted in the F6-2
  suspend state and re-applied at resume: resume sets FLAG_ALPHA as today
  but pushes `-MNU_ALPHA` only when the saved mode was alpha; in keys mode
  it restores the underlying-menu row. A parameterized item keyed in keys
  mode therefore returns to keys mode after its TAM completes.
- **E14 (reset discipline — the trap-#2 checklist).** `forthKeysMode` is
  cleared at every site that closes/aborts/tears down a capture:
  `pemAlpha` backspace-abort; both `pemCloseAlphaInput` branches; the
  ITM_FORTH toggle-close arm; `forthCapPowerReset`;
  `forthCaptureSanitizeRestoredUi`; the E12.4 EXIT rung. A landed test must
  poison the bit and drive each site (mutation-checked), mirroring the
  tam.function pinning.
- **E15 (α cost) — RESOLVED (trace T6, 2026-08-04): zero observable cost.**
  α stays typeable during capture via CHARS/AIMCATALOG → ALPHA_OMEGA
  (softkey resolution is `determineFunctionKeyItem_C47`, untouched by the
  column swap; all four R47 layouts). Of the 21 item names containing the
  α glyph, the 13 CAT_FNCT rows are all parameterized (never PTP_NONE), so
  they insert atomically via the TAM fold and never depend on typing α.
  Minor: `alphaCase == AC_UPPER` at menu-push time yields capital ITM_ALPHA
  on that softkey — one extra case-toggle press, not a loss.

## Interactions verified by the research pass (2026-08-04)

- E0 already preserves `tam.function == ITM_FORTH` on every diverted key;
  keys mode adds no new writer of tam.function. [manage.c:1643-1657]
- The per-key recommit tail runs on every insert path keys mode uses, so the
  on-disk step keeps tracking the buffer (power-off contract intact).
  [manage.c:1052-1079]
- `forthCapInsertName` enforces the 256-byte/196-glyph cap on every keys-mode
  insert, same as typing. [forth_menu.c:29-43]
- The F6-3 arm excludes ITM_AIM and ITM_FORTH, so neither the toggle item nor
  the region toggle can ever be inserted as text. [manage.c:1044-1050]
- Number-literal grammar: keys-mode text inserts are item catalog names or
  digit characters; catalog names containing 2-byte glyphs are structurally
  non-numbers, so no collision with the ASCII-only number grammar.
  [forth_compile.c:456-506]

## Pre-work register — ALL TRACES EXECUTED 2026-08-04 (sonnet fan-out, results folded above)

| # | Question | Result |
|---|----------|--------|
| T1 | Physical vs catalog TAM entry | **Identical for the suspend/fold seam** — neither `forthCaptureSuspend/Resume` nor the tamEnterMode gate reads `fnKeyInCatalog`/`catalog`; `numberOfTamMenusToPop` is unconditionally reset to 1 inside tamEnterMode. The physical route is CLEANER: it never buries a catalog menu on the stack (see D-C3). Keys-mode STO takes the same seam as today's tested route. |
| T2 | Digit routing | Digits/period/exponent are `addItemToBuffer` items; column swap alone suffices (E12.3 rewritten). Residual packet checks: ITM_EXPONENT spelling, `numlockReplacements()` body. |
| T3 | SST/BST/R-S | `fnSst/fnBst` already commit-and-close via their own `pemCloseAlphaInput()` branch — E12.2 needs no new commit code; R/S needs the commit-then-native-STOP arm (E12.2). Empty-buffer navigation edge unpinned — K2 test. |
| T4 | FIX-8 | Code defect **confirmed**; unreachable today (ITM_FORTH lives only in FCNS; the CATALOG key is invisible in the AIM columns), **activated by Stage K** (keys mode makes -MNU_CATALOG→FCNS→FORTH reachable). See sequencing below. |
| T5 | FIX-7 | Asymmetry **confirmed and worse**: check mode skips item branches, so a folded `GTO 'X'` COMMITS SILENTLY and fails only when that step executes; folded `XEQ '…'` is refused at ENTER purely because the structural XEQ keyword shares its ASCII spelling. Glyph-delimiter acceptance has zero number-grammar collision (bytes ≥0x80 are never numbers); only a structural possibility of names containing the glyphs as content (no instance found). |
| T6 | α glyph | Zero cost — see E15. g+key51 = ITM_omega confirmed on all four layouts. |

## Related defects and sequencing — RESOLVED 2026-08-04

**FIX-7/7b, FIX-8, FIX-9 all landed on forth-core/capture-fixes
(81a326a0c, 3e0c6264c, 62f40bd8f; +224 B flash combined). Stage K's
sequencing preconditions are satisfied; packets K1-K4 are authorable.**
The paragraphs below record the pre-fix sequencing rationale.

See DEFECTS_capture_roundtrip.md (D-C1/D-C2 now CONFIRMED by trace, D-C3
added). Sequencing:

- **FIX-7 (quote round-trip) lands BEFORE Stage K** — keys mode makes
  parameterized folds a primary flow; today the folded text silently
  commits and detonates at run time for everything except XEQ.
- **FIX-8 (toggle-close stale FCAP_OPEN) lands WITH or BEFORE Stage K** —
  it is latent today but keys mode is exactly what makes it user-reachable
  (keys mode → CATALOG key live → FCNS → FORTH pick → stale state).
- **FIX-9 / D-C3 (buried catalog menu after catalog-initiated TAM during
  capture) is reachable TODAY** and independent of Stage K — needs a
  reproducer first (static trace only). The `_forthCatalogBuriedOnStack`
  drain guard exists for the ITM_FORTH-from-catalog path but has no
  equivalent on the TAM suspend/resume path.

## Predicted packet decomposition (after traces land)

- K1 — `forthKeysMode` bit + determineItem column branch + toggle arms +
  menu swap + E14 reset sites + poisoning tests.
- K2 — E12 routing: digit divert, SST/BST/R-S reconciliation, specials
  allow-list, tests driving the real key path (trap-#8 discipline: no
  hand-set state, drive `determineItem`/`processKeyAction`).
- K3 — E13 TAM round-trip persistence + resume menu branch + tests.
- K4 — acceptance battery: toggle in/out; SIN→`SIN ` text; STO in keys mode
  → TAM → `STO 05 ` → still keys mode; digits as text; EXIT rung order;
  navigation commit; close-site bit clears (mutation-pinned); picker and
  catalogs unaffected in both sub-modes; arena high-water report; flash
  delta per RULE-1.

Standing discipline applies unchanged (one packet per file, EXECUTION GATE,
two-attempt handoff, mutation checks, arena/flash reporting).
