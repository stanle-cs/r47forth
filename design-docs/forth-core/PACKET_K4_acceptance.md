# PACKET K4 — Stage K acceptance battery

**Stage K packet 4 of 4.** End-to-end acceptance over the landed K1-K3
behavior through real key drives; no production-code changes are expected
(a needed production change is a STOP — it means K1-K3 left a defect).
Predecessor: K3 (1ac2f6653) on forth-core/stage-k.

## Implementer contract

Identical to PACKET_K1_keys_toggle.md's contract. Base step: `git merge
--ff-only forth-core/stage-k` in the worktree; `git log --oneline -3`
must show the K4-packet commit on top of 1ac2f6653. Else STOP.
Report protocol: full report to `K4_REPORT.md` at the worktree root;
terse final message (STATUS, gate runs, per-test one-liners, mutation
line, deviations one sentence each).

## Battery (one test function per item; banner "FORTH K4 TESTS (stage acceptance)")

**A1 `test_k4_mixed_input_definition`** — the stage's headline story,
driven end-to-end: open a capture inside a region; alpha-type
`: SQ DUP ` (ITM_colon? no — type the characters `:`, space, S, Q, space,
then the word DUP letter-by-letter or via keys mode); toggle to keys;
press the multiply key (the row whose normal `primary` is the native
multiply item — locate it from kbd_std like K1's T1 locates the ALPHA
row); toggle back to alpha; type ` ;`; ENTER (relock); type `4 SQ`;
commit via EXIT; then run the program by label (the F15/showcase idiom:
XEQ the fixture label) and assert X == 16 as a long integer. This proves:
mixed-sub-mode entry, token boundaries, glyph-name resolution of the
keys-inserted multiply, commit, and execution.

**A2 `test_k4_keys_only_line`** — keys mode throughout: digits `4`,`2`,
then STO (TAM fold → `42 STO 00` with the boundary guard separating),
ENTER, run, assert register 00 holds 42. Use TAM digits `0`,`0`.

**A3 `test_k4_relock_submode`** — PINS the current default: after a
keys-mode line is committed by ENTER, the E5 relock opens the NEXT line
in ALPHA input (close cleared the bit; fresh-open default). Assert bit
false + `-MNU_ALPHA` after the relock. Comment in the test: this is the
pinned K1 default, not a ruling — a future refinement may persist the
sub-mode across relock, and must flip this pin deliberately.

**A4 `test_k4_ladder_full_unwind`** — from keys mode with the FWRD picker
unreachable (keys mode has no alpha menus): EXIT #1 → alpha (+ALPHA
menu); push the FWRD picker (showSoftmenu(-MNU_FORTH) as the landed
picker tests do); EXIT #2 → pops picker to ALPHA menu; EXIT #3 (empty
line) → abort, FCAP_CLOSED; EXIT #4 → leaves... stop at #3; assert each
rung's state tuple after each press (one level per press, E8+E12.4).

**A5 `test_k4_arena_sweep`** — three full cycles of: open, toggle keys,
digits+STO fold, ENTER commit, reopen via E2, toggle, EXIT-unwind to
closed. Assert step counts return to fixture baseline each cycle and
freeRam residue obeys the block-aligned growth-only escape valve (cite
the K2/F6 valve shape; bound: 8 quanta for three cycles). Report the
FORTH ARENA lines.

## Mutations (cross-pins — each disables a landed K-arm and must red the battery)

- M1: disable K1's toggle arm gate (make `if(func == ITM_AIM &&
  forthCapIsOpen())` into `if(false)`) → A1 red (toggle dead, multiply
  key types a letter into the line or the definition fails).
- M2: force K2's `lead` to 0 → A2 red (`42STO`-class glue upstream of the
  fold) or A1 red — record which asserts fire.
- M3: restore K3's interim suspend-clear → A2 red? A2 does not assert the
  bit mid-fold — if A2 stays green, the battery's persistence coverage is
  K3's T1 (cite it); apply the mutation and record honestly whether the
  battery alone catches it (an ESCAPE here is acceptable WITH the note
  that K3 T1 is the pin — the battery need not duplicate every unit pin).

## Acceptance

Gate green incl. upstream suite; PASS-set diff = only new K4 lines;
mutation results per above with honest ESCAPE notes; arena lines; the
K1/K2/K3 groups unchanged-green; no production diffs (STOP otherwise);
no commits.

---

## AMENDMENT K4-A (2026-08-04, post-implementation)

Battery green, no production changes, M1-M3 all RED (M3 needed no escape —
A5 catches the interim regression at cycle 0). One PASS-set finding: run
ORDER matters — the battery's program runs ahead of the FIX-6 group shifted
the free-list shape and pushed test_freelist_interior_double_free into its
defensive SKIP. Resolution (architect): K4 registers AFTER the FIX-6 group;
the assertion runs again (13 regions, guard verified). A2's EXIT-before-run
deviation records a real trap: running a parameterized word from inside PEM
takes the E0 divert and records a step instead of executing. Implementation:
opus subagent, 43.6 min, 233k tok, 14 gate runs, zero rescues.
