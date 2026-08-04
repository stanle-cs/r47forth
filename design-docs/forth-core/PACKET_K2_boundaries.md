# PACKET K2 — token-boundary guard, EXIT ladder rung, EEX/numlock residuals

**Stage K packet 2 of 4** (design: STAGE_K_KEYS_MODE.md; rules E12.3
residuals, E12.4, plus the token-boundary defect the owner's question
surfaced 2026-08-04: the direct insert paths glue digits to names).
Predecessor: K1 (commit eb6c36faf on branch forth-core/stage-k).

## Implementer contract

Identical to PACKET_K1_keys_toggle.md's contract (worktree, gate =
`./packages/forth-core/build-test.sh` with `grep -a` on logs, STOP
conditions, fixture rules, mutation protocol, arena report, no commits).
Base verification: run `git merge --ff-only forth-core/stage-k` INSIDE
the worktree (no-op if already at tip), then verify `git log --oneline -2`
shows the K2-packet commit on top of K1 (eb6c36faf). Else STOP.

## EXECUTION GATE (STOP on mismatch)

```
git merge-base --is-ancestor eb6c36faf HEAD && echo K1-present            # K1 in history
grep -n "forthCapKeysMode" packages/forth-core/forth_capture.h        # K1 landed
grep -n "K1/E12.2" packages/forth-core/keyboard.c | head -1           # RS/SST guards present
grep -n "one trailing space" packages/forth-core/forth_menu.c         # forthCapInsertName pre-K2 shape
grep -n "conv\[258\]" packages/forth-core/programming/manage.c        # F6-4 wrapper pre-K2 shape
grep -n "numlockReplacements(0, item" packages/forth-core/programming/manage.c
grep -c "K2/" packages/forth-core/keyboard.c                          # must be 0
```

## C1 — forth_menu.c: token-boundary guard in forthCapInsertName

**The defect (reproducer T2 pins it):** every name-insert path except the
F6-4 fold calls `forthCapInsertName` raw; with the cursor after typed
digits, `42` + SIN yields the glued token `42SIN ` — unresolvable (the
tokenizer splits on 0x20 only). Keys mode makes digits-then-function the
primary flow.

Replace the function body with (exact code):

```c
bool_t forthCapInsertName(const char *name)
{
  int32_t nameLen = stringByteLength(name);
  if(!forthCapIsOpen()) { return false; }
  int32_t bufLen = stringByteLength(aimBuffer);
  /* K2: token-boundary guard — a name must land as its own token.  When
   * the byte before the cursor is neither a space nor the line start,
   * insert one leading separator space.  Previously only the F6-4 fold
   * wrapper did this; the direct F6-3/picker/keys-mode paths glued
   * digits to names ("42" + SIN -> "42SIN ", an unresolvable token). */
  int32_t lead = (T_cursorPos > 0 && aimBuffer[T_cursorPos - 1] != ' ') ? 1 : 0;
  if(bufLen + nameLen + lead + 1 < 256 && stringGlyphLength(aimBuffer) + nameLen + lead + 1 <= 196) {
    xcopy(aimBuffer + T_cursorPos + nameLen + lead + 1, aimBuffer + T_cursorPos,
          stringByteLength(aimBuffer + T_cursorPos) + 1);
    if(lead) { aimBuffer[T_cursorPos] = ' '; }
    xcopy(aimBuffer + T_cursorPos + lead, name, nameLen);
    aimBuffer[T_cursorPos + lead + nameLen] = ' ';
    T_cursorPos += nameLen + lead + 1;
    return true;
  }
  return false;
}
```

Update the function's comment block ("Insert name + one trailing space")
to state the leading-separator rule too.

## C2 — programming/manage.c: simplify the now-redundant F6-4 wrapper

In `forthCaptureResume`'s fold loop, the `conv[258]` wrapper's ONLY jobs
were the leading separator (now C1's) and passing text through. Replace:

```c
      { char conv[258]; char *t = conv;            /* 1 + 255 + NUL */
        if (T_cursorPos > 0 && aimBuffer[T_cursorPos - 1] != ' ') {
          *t++ = ' ';        /* word separator when mid-text */
        }
        xcopy(t, tmpString, stringByteLength(tmpString) + 1);
        if (!forthCapInsertName(conv)) {
          break;   /* no room: keep this and later steps after the line */
        }
      }
```

with:

```c
      /* K2: the leading separator now lives in forthCapInsertName itself
       * (token-boundary guard) — pass the decoded text straight through. */
      if (!forthCapInsertName(tmpString)) {
        break;   /* no room: keep this and later steps after the line */
      }
```

The `> 255` length clamp above it stays. The landed parity pin
(`"5 DUP STO 05 "` in test_capture_suspend) must stay green unchanged —
it is the no-regression oracle for this simplification.

## C3 — keyboard.c: the EXIT ladder rung (E12.4)

In `fnKeyExit`'s `case CM_PEM:` arm, immediately AFTER the
`lastErrorCode` check and BEFORE the `isAlphaSubmenu(0)` pop:

```c
        if(forthCapIsOpen() && forthCapKeysMode() && !tam.mode) {
          /* K2/E12.4: first ladder rung — EXIT in keys mode returns to
           * alpha input; the rest of the E8 ladder is untouched below.
           * Ladder is now: keys -> alpha -> submenu -> ALPHA menu ->
           * drop keypad -> leave PEM, one level per press. */
          forthCapSetKeysMode(false);
          showSoftmenu(-MNU_ALPHA);
          break;
        }
```

## C4 — programming/manage.c: pemAlpha character-arm residuals (E12.3)

Anchor: the character arm containing
`item = numlockReplacements(0, item, getSystemFlag(FLAG_NUMLOCK), shiftF, shiftG);`.

(a) **EEX spelling.** `ITM_EXPONENT`'s `itemSoftmenuName` is literally
`"EEX"` (items.c row 990), which the number grammar cannot read (it
accepts `e`/`E` only). At the TOP of the character arm:

```c
      if(forthCapIsOpen() && item == ITM_EXPONENT) {
        /* K2/E12.3: EEX must produce the number grammar's exponent
         * spelling, not the three letters "EEX" (its softmenu name). */
        ... insert the single character "e" at T_cursorPos under the same
            cap and cursor advance as any other character insert ...
      }
```

Implement the insert the same way the arm inserts any other character's
bytes (one byte, `"e"`), then fall through to the recommit tail exactly
as the ordinary character path does. Do NOT route through
forthCapInsertName (no trailing space — `1e5` must stay one token).

(b) **numlock inertness.** Guard the existing call:

```c
      if(!(forthCapIsOpen() && forthCapKeysMode())) {
        item = numlockReplacements(0, item, getSystemFlag(FLAG_NUMLOCK), shiftF, shiftG);
      }
      /* K2/E12.3: keys-mode items are normal-column ids; the numlock
       * translation table is aim-column keyed and must not touch them. */
```

## Tests (append to test_capture.part.h; register after the K1 group, banner "FORTH K2 TESTS (token boundaries + ladder)")

Fixture idiom identical to K1's tests (testProg_t, full state reset,
fnGotoDot(2), runFunction(ITM_AIM) opens the capture).

**T1 `test_insert_token_boundary`** — the C1 class test. The class: every
name insert lands as its own token, whatever precedes the cursor. For
each of the three preconditions (i) empty line, (ii) buffer `"42"` cursor
at end, (iii) buffer `"42 "` cursor at end — drive a direct insert via
the real F6-3 path (`runFunction(ITM_sin)` with keys mode ON via the real
toggle `runFunction(ITM_AIM)`) and assert the buffer is exactly
(i) `"SIN "`, (ii) `"42 SIN "`, (iii) `"42 SIN "` (no double space).
Then one picker-path subcase: with the FWRD picker primed the way
test_picker_insert_at_cursor does, buffer `"42"` → pick → `"42 <word> "`.

**T2 `test_keys_digits_then_function`** — the reproducer, end-to-end:
keys mode ON, press `4`, `2` (digit items through the real dispatch),
then SIN; assert buffer `"42 SIN "`; press ENTER; assert commit succeeds
(lastErrorCode == ERROR_NONE, relock) — red on the unfixed tree at the
buffer assert (`"42SIN "`).

**T3 `test_exit_ladder_keys_rung`** — capture open, toggle to keys
(`runFunction(ITM_AIM)`), type nothing; `fnKeyExit(NOPARAM)` → bit clear,
`currentMenu() == -MNU_ALPHA`, capture still FCAP_OPEN, buffer intact;
second `fnKeyExit(NOPARAM)` on the empty line → aborts (FCAP_CLOSED),
i.e. the ladder advanced exactly one level per press. Repeat with text
`"2"`: EXIT from keys → alpha (capture open, text intact); EXIT again →
commit-and-close with text preserved as the committed step.

**T4 `test_keys_eex_and_numlock`** —
 - sc1: keys mode, press ITM_EXPONENT via dispatch → buffer gains `e`
   (exactly one byte, no space); compose `1` `EEX` `5` → `"1e5"`; ENTER
   commits and the line's number classifies (no error).
 - sc2: poison `setSystemFlag(FLAG_NUMLOCK)`, keys mode, press `2` →
   buffer gains `"2"` (numlock translation skipped); restore the flag.

**T5** — no new test: cite the landed `test_capture_suspend` subcase 1/2
PASS lines as the C2 no-regression oracle in your report.

## Mutations

- M1: revert C1 (drop the `lead` logic) → T1 (ii) red (`"42SIN "`).
- M2: remove the C3 rung → T3 red (first EXIT commits/aborts instead of
  returning to alpha).
- M3: remove the C4a EEX map → T4 sc1 red (`"1EEX5"`).
- M4 (escape-valve permitted): remove the C4b numlock guard → attempt
  T4 sc2 red. If the gate stays green because the translation table is
  inert for digit items in this state, report ESCAPE with the evidence
  (that is an acceptable outcome per the escaped-mutation rule; the guard
  is defensive).

## Acceptance

Final gate green incl. upstream suite; mutations per above; PASS-set diff
vs pre-edit baseline shows only the new K2 lines; arena line reported;
`test_capture_suspend` subcases 1-2 and the K1 group unchanged-green.
Deliver the same structured report as K1. Do NOT commit.
