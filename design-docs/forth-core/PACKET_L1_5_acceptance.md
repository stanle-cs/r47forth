# PACKET L1-5 — stage acceptance battery and close

**Stage L final packet.** **Prerequisite: L1-F3 landed and green.**

**Scope.** The end-to-end pin for the whole stage, the poison sweeps, the
DESIGN.md fold-in, and the stage's measured numbers. No new behaviour.

## Implementer contract

As PACKET_L1_1. Not repeated.

## C1 — the stage story (`test_interactive_acceptance`, new)

One test, driven entirely through real entry points, in this order. Each
step asserts before moving on.

1. From `CM_NORMAL` with a number in X, press FORTH. Capture opens, X
   untouched (non-string arm), `calcMode == CM_AIM`.
2. Type `1 2 +` in alpha through the key path. ENTER. X == 3, line empty,
   capture still open.
3. Type `: SQ DUP * ;`. ENTER. No error; `SQ` resolves via
   `forthFindColon`.
4. Type `4 SQ`. ENTER. X == 16 — an interactive definition used from a
   later interactive line, which is the stage's reason to exist.
5. ALPHA-gesture to keys mode. Press the `SIN` key. Line reads `SIN `.
   Backspace it away.
6. Still in keys mode, `STO` `0` `5`. Line reads `STO 05 `; register 05
   unchanged. ENTER; register 05 now holds 16.
7. EXIT. Capture closed, `calcMode == CM_NORMAL`, `FLAG_ALPHA` clear, and
   **X still 16** (no `closeAim` string commit).
8. FORTH again. f-up. The recalled line is `4 SQ` (or whatever step 6
   left newest — assert against FHIST, not a hardcoded guess).
9. EXIT. `XEQ 'FHIST'` runs; assert it re-runs the session's lines (L-R7,
   deliberately).
10. `GLOBAL` a word, then force a lifetime reset (run any program with an
    `ITM_FORTH` step), and assert the global survives while the
    interactive-scope word does not — the §8.3 durability contract L3
    exists to surface.

## C2 — poison sweeps

1. **Close-path sweep, interactive rows.** L1-1 deferred these because the
   landed test is a hardwired four-case PEM switch. Now that L1-2 owns the
   EXIT ladder, add the interactive open as a second open-path axis and
   assert the full close tuple (`state`, `keysMode`, `origin`, `foldMode`)
   after every (open × close) pair. Report the pair count.
2. **Arena residue.** Open/close an interactive capture 20 times; assert
   `getFreeRamMemory()` returns to its starting value, or that any residue
   is block-aligned and growth-only within the landed escape-valve bound
   (test_capture.part.h:3866-3888 documents the precedent and its
   reasoning — reuse it, do not invent a new tolerance).
3. **Program-memory residue.** Same, for `firstFreeProgramByte`, across a
   full history cap cycle including eviction.

## C3 — DESIGN.md fold-in

Stage L is normative from here. Fold in:

- **§8.4 gains an interactive origin.** E0–E14 are unchanged for PEM; add
  the interactive column: where the capture lives (`aimBuffer` on the AIM
  surface), what opens it (`fnForthOuter`, L-R2), what ENTER does (L-R3),
  the EXIT ladder, and the `forthCap.origin` bit with its E14 reset.
- **A new §8.4.2, the fold interactively** — the `calcMode` bracket, the
  FHIST materialisation, FOLD vs PARK, and the parity contract from F3.
- **§8.1 gains FHIST**: a named program of `ITM_FORTH` source steps,
  capped and evicted oldest-first, runnable by design.
- **§8.10 item 2 is discharged** — say so, and note that item 1 (Stage M:
  browse/assign of Forth words) remains deferred.
- **§3.3.2's "directly callable from PC tests"** re-points at
  `forthOuterRun`/`forthOuterInterpret`; `fnForthOuter` is now a capture
  opener with a running-program one-shot arm.
- **L3's durability paragraph**: interactive definitions die at the next
  lifetime consumption (§8.3); `GLOBAL` is the durability mechanism;
  cross-scope isolation is deliberate and unchanged.
- **L-R8's composing view** recorded as the render contract, with its
  known consequence (X hidden; T/Z/Y lost past ~350 px in the large font).
- **The known v1 limitations**, each with its anchor: catalog-driven TAM
  commits do not fold (T7.8); the `PTP_DISABLED` fold invariant is
  undocumented in upstream and pinned only by our class test (T7.5).

Amendment trail to DESIGN-HISTORY.md as usual, including the corrections
this stage made to its own record — the T7.5 retraction and the T8 pivot
rejection are part of the design history, not embarrassments to omit.

## C4 — the numbers

Report, in the stage-close commit message:

- Measured `make dmcp5r47 CUSTOM_PKG=packages/forth-core` flash delta for
  the whole stage (RULE-1). The doc's estimate is +3–5 KB; report the
  actual and, if it is outside that band, say why.
- Idle BSS delta. Expectation: **+8 bytes** (`forthFoldCtx_t`) plus the
  history cursor tuple and the byte fields on `forthCap` — the L-R5 ring's
  512 B is **not** spent (L-R7). If `sizeof(forthCap_t)` grew, say so.
- `FORTH ARENA` high-water (§5.4).
- Program-memory high-water with a full history at the 1024-byte cap.

## C5 — the bench

DM42n hardware pass at stage exit, per the standing discipline: the C1
story driven on the device, not only in the sim. Report what differed
from the sim, if anything.

## Acceptance

- Gate green. Full battery ALL PASSED, upstream testSuite GREEN.
- The C1 story green end to end.
- Pair counts and residue numbers from C2 reported.
- DESIGN.md and DESIGN-HISTORY.md updated; STAGE_L_INTERACTIVE.md and
  STAGE_L_TRACES.md marked superseded-by-DESIGN.md where they were
  normative-pending.
- All four C4 numbers reported.
- Sim captures for the C1 story attached (`run-sim`, copy-adapting
  `references/capture-driver.c`).
