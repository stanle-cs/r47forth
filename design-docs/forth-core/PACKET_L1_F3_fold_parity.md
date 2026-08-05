# PACKET L1-F3 — operand-class parity and the fold's close paths

**Stage L fold packet 3 of 3.** **Prerequisite: L1-F2 landed and green.**

**Scope.** Tests only, plus whatever they force. The contract this packet
establishes is the one L-R4 (b) was ruled for:

> For the same key sequence, the interactive fold and the PEM fold produce
> **string-identical** line text, across every operand class of the §4
> (F4) grammar.

Parity is now the stage's contract rather than an accident, so it is
asserted **pairwise in one test** rather than by two separate expectation
lists that can drift.

## Implementer contract

As PACKET_L1_1. Not repeated. One addition: **if a parity subcase fails,
that is a finding, not a test bug.** Report the PEM text and the
interactive text side by side and STOP; do not adjust the expectation to
match the implementation.

## EXECUTION GATE (STOP on mismatch)

```
grep -n "forthFoldArmed" packages/forth-core/ui/tam.c        # F2 landed
grep -n "test_capture_param_text" packages/forth-core/test_capture.part.h
grep -n "test_capture_close_paths_reset_tuple" packages/forth-core/test_capture.part.h
```

## C1 — the pairwise parity battery (`test_fold_operand_parity`, new)

For each class below, drive the **same key sequence** twice — once in a
PEM Forth capture, once in an interactive capture — and assert the two
resulting line texts are `compareString(..., CMP_BINARY) == 0`.

| # | Class | Drive |
|---|---|---|
| 1 | direct register | `STO` `0` `5` |
| 2 | dotted local register | `STO` `.` `0` `3` |
| 3 | indirect register | `STO` `→` `0` `5` (the landed indirect gesture) |
| 4 | indirect variable | `RCL` indirect + a named variable |
| 5 | flag by number | `SF` `1` `2` |
| 6 | dotted local flag | `SF` `.` `0` `2` |
| 7 | named global label | `XEQ` + alpha name |
| 8 | named local label | `XEQ` `:` + name (`tam.colon`) |
| 9 | `TM_VALUE` > 250 | an item whose commit takes the `CNST_BEYOND_250` encoding (manage.c:2185-2188) |
| 10 | `TM_VALUE` min/max edge | the item's `tamMinMax` bounds, both ends |
| 11 | `TM_SHUFFLE` | the shuffle gesture |
| 12 | `TM_MENU` | **only if F1 admitted it**; if F1 moved it to PARK, assert PARK behaviour instead and say so |

**Fixture discipline.** The PEM half must use the landed idiom
(test_capture.part.h:3624-3668: `testProg_t`, `fnGotoDot(2)` onto the
marker, `runFunction(ITM_AIM)`); the interactive half opens via
`fnForthOuter(NOPARAM)` from `CM_NORMAL`. Reset `lastErrorCode` and the
capture between halves. Capture each half's text into its own buffer
**before** comparing, so a failure can print both.

**Report the full table** — twelve rows, PEM text and interactive text —
in your report, not just pass/fail. This table is the stage's evidence
that L-R4 (b) was delivered.

## C2 — the fold's close paths (`test_fold_close_paths`, new)

Extends the E14 close-paths class with the fold's own invariant:

> **No fold may leave an outstanding transient step.**

For each of these, assert afterwards: the capture line is intact,
`T_cursorPos` is valid (`<= stringByteLength(aimBuffer)`),
`getNumberOfSteps()` equals the pre-fold count, `firstFreeProgramByte`
equals its pre-fold value, and `forthCap.foldMode == 0`.

1. **EXIT mid-TAM** (before any digit).
2. **EXIT mid-TAM** (after one digit of two).
3. **Backspace-to-empty during the TAM's operand entry**, if reachable.
4. **Error at commit** — `lastErrorCode != ERROR_NONE` makes
   `ui/tam.c:1102` insert nothing; assert the fold still sweeps and the
   line survives.
5. **Oversize-text break** — a capture line long enough that
   `forthCapInsertName` refuses (forth_menu.c:43). The landed loop breaks
   and keeps the step (manage.c:1251); assert `forthFoldLeave`'s sweep
   removes it anyway.
6. **PARK commit** — the TAM executes live; assert the line survived and
   nothing was left behind.
7. **`forthCapPowerReset` mid-fold** — assert `foldMode` clears and no
   step is orphaned in FHIST beyond what a legitimate history push would
   leave.

## C3 — the CM-gate audit sweep (`test_cm_gate_audit`, new)

STAGE_L_TRACES.md carries a 17-row table of every landed `calcMode ==
CM_PEM` gate with a widen/keep verdict. Encode the **verdicts** as
assertions so a future edit that flips one goes red:

- For each row marked **KEEP PEM-only**, drive the interactive equivalent
  and assert the PEM-only behaviour did **not** happen.
- For each row marked **WIDEN**, drive it interactively and assert it
  **did**.
- For rows that cannot be driven from a test, say so per row with a
  reason. **A row you cannot drive is a reported gap, not a silent
  omission.**

Report the table with a driven/not-driven column.

## C4 — what this packet may force

If a parity row fails, the fix belongs in F1 or F2, not here. Likely
candidates, in the order the review expects them:

- `_forthFoldAdmits` mis-classifying a mode (F1 C2).
- The `TM_MENU` `tmpString` prefix hazard (T7.7 item 6).
- A commit site that does not route through `leaveTamModeIfEnabled`
  (F2 C2's enumerated list).

Report which, with the failing pair.

## Acceptance

- Gate green; the twelve-row parity table reported in full; the CM-gate
  table reported with its driven column; PASS lines quoted.
- Flash + arena reported.
- **Sim:** the mixed-input story end to end — open interactive, type
  `42`, keys-mode `STO` `0` `5`, ALPHA back, type `DUP *`, ENTER, see the
  result. Capture via `run-sim`, copy-adapting
  `references/capture-driver.c`.
