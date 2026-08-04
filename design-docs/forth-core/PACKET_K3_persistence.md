# PACKET K3 — keys-mode persistence across the TAM round-trip (E13 proper)

**Stage K packet 3 of 4.** Replaces K1's E13 interim (suspend clears the
bit) with the ruled behavior: a parameterized item keyed in keys mode
returns to keys mode after its TAM completes. Predecessor: K2 on branch
forth-core/stage-k.

## Implementer contract

Identical to PACKET_K1_keys_toggle.md's contract. Base step: run
`git merge --ff-only forth-core/stage-k` inside the worktree; then
`git log --oneline -3` must show the K3-packet commit on top of the K2
commit (4358f832e). Else STOP.

## EXECUTION GATE (STOP on mismatch)

```
git merge-base --is-ancestor 4358f832e HEAD && echo K2-present
grep -n "K1/E13 interim\|E13 interim" packages/forth-core/programming/manage.c   # the clear to be removed
grep -n "forthCap.keysMode = 0;.*K1: a fresh capture" packages/forth-core/forth_capture.c
grep -n "if (forthCap.state == FCAP_SUSPENDED) forthCap.state = FCAP_CLOSED;" packages/forth-core/forth_capture.c
grep -n "if(!forthCapKeysMode())" packages/forth-core/programming/manage.c | head -1   # must be ABSENT (grep returns nothing)
```

## C1 — programming/manage.c, forthCaptureSuspend: remove the interim

Delete the `forthCapSetKeysMode(false);` line and its K1/E13-interim
comment (the lines immediately before `forthCapSuspendState(...)`). The
bit now rides the struct across suspension untouched.

## C2 — programming/manage.c, forthCaptureResume: survive the reopen

`forthCapOpen()` zeroes the bit by design (fresh captures start alpha).
Resume is not a fresh capture. Replace the bare call
`forthCapOpen();` (with its "SUSPENDED → OPEN" comment) by:

```c
  { bool_t keysWas = forthCapKeysMode();    /* K3/E13: resume is not a fresh
                                               capture — the sub-mode the user
                                               keyed the TAM item from comes
                                               back with the line */
    forthCapOpen();                         /* SUSPENDED → OPEN; clears aimBuffer,
                                               which TAM may have used meanwhile */
    forthCapSetKeysMode(keysWas);
  }
```

## C3 — programming/manage.c, forthCaptureResume tail: menu row per sub-mode

The tail currently runs the FIX-9 drain then `showSoftmenu(-MNU_ALPHA);`
unconditionally. Keep the drain unconditional; gate only the push:

```c
  if(!forthCapKeysMode()) {
    showSoftmenu(-MNU_ALPHA);
  }
  /* K3/E13 + K-R3: in keys mode the underlying menu row IS the mode
   * indicator — resume must not cover it with the alpha menu. */
```

## C4 — forth_capture.c: abandon clears the bit (E14)

`forthCapAbandonSuspended` flips SUSPENDED→CLOSED without touching the
bit — with C1/C2 the bit now survives suspension, so an abandoned
suspension would leak it into the next capture. Change the body to also
zero `forthCap.keysMode` when it flips the state (one line, with an
E14 comment).

## Tests (append; banner "FORTH K3 TESTS (keys-mode TAM persistence)")

Fixture idiom as K1/K2 (testProg_t, full reset, fnGotoDot(2),
runFunction(ITM_AIM)).

**T1 `test_keys_tam_roundtrip`** —
 sc1: toggle keys (`runFunction(ITM_AIM)`), `runFunction(ITM_STO)` →
   FCAP_SUSPENDED and `forthCapKeysMode()` still TRUE (persists through
   suspension);
 sc2: `tamProcessInput(ITM_0); tamProcessInput(ITM_5);` → FCAP_OPEN,
   bit still TRUE, text `"STO 05 "`, and `currentMenu() != -MNU_ALPHA`;
 sc3: `runFunction(ITM_AIM)` → bit FALSE, `currentMenu() == -MNU_ALPHA`
   (toggle still symmetric after a round-trip).

**T2 `test_alpha_tam_roundtrip_unchanged`** — same drive WITHOUT the
toggle (alpha sub-mode): after the TAM commit, FCAP_OPEN, bit FALSE,
`currentMenu() == -MNU_ALPHA`, text `"STO 05 "` — the C3 gate must not
regress the alpha path.

**T3 `test_abandon_clears_keys_bit`** — toggle keys, `runFunction(ITM_STO)`
(suspended, bit true), then falsify the saved step exactly the way
`test_capture_suspend`'s canary subcase does, drive the resume choke
point (`fnKeyExit(NOPARAM)` TAM-cancel path as that subcase does) →
suspension abandoned; assert bit FALSE and FCAP_CLOSED.

## Mutations

- M1: restore the C1 interim clear → T1 sc1 red (bit false while suspended).
- M2: drop C2's save/restore (bare `forthCapOpen()`) → T1 sc2 red.
- M3: make C3's push unconditional again → T1 sc2 red (menu is -MNU_ALPHA).
- M4: revert C4 → T3 red (bit survives the abandon).

## Acceptance

As K1/K2: final gate green incl. upstream suite; four mutations red;
PASS-set diff shows only the new K3 lines; K1/K2 groups and the landed
suspend battery unchanged-green; arena line reported; no commits.

---

## AMENDMENT K3-A (2026-08-04, post-implementation)

The packet's acceptance clause "K1 group unchanged-green" contradicted its
own C1: K1's test_keys_mode_nav_guards sc4 explicitly pinned the E13
interim that C1 deletes. Implementer resolved correctly per the re-pin
precedent: sc4 now pins the ruled behavior on the TAM CANCEL path
("a cancelled TAM round-trip resumes in keys mode"), complementing K3
T1's commit path; it stays in the K1 test where the drive lives. C4
needed braces (two statements), behavior as specified.
Implementation: opus subagent, 17.9 min, 113k tok, 7 gate runs, 4/4
mutations red, zero rescues.
