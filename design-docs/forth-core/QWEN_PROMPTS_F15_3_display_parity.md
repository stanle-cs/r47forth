# Stage F15-3 — display parity across PEM/BST/SST surfaces (bounded implementation prompt)

Origin: DESIGN §8.9 item 4, against the landed F15-2 tree
(`5a9e9ce2d`). Stage ledger: `QWEN_PROMPTS_F15_harness.md`. Authored
2026-07-17 after tracing all three real display drives. The exact labeled
fixture is 35 bytes; its 12-byte source payload and every step offset below
were machine-verified.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -5` contains a commit whose subject is exactly
   `forth-core: F15-2 — end-to-end entry-state + power-off acceptance (§8.9 item 2)`.
3. `grep -n "static void decodeRem\|forthMarkerTurnsOn(opcodeStart)"
   packages/forth-core/programming/decode.c` shows one `decodeRem` and one
   parity call in its zero-length `ITM_FORTH` marker arm. The true arm writes
   `STD_RIGHT_DOUBLE_ANGLE "FORTH"`; the false arm writes
   `"FORTH" STD_LEFT_DOUBLE_ANGLE`.
4. `grep -n "void fnPem\|decodeOneStep(step)\|21 \* line\|: 62)"
   packages/forth-core/programming/manage.c` shows the real PEM listing
   function, its shared decode call, the 21-pixel row pitch, and x=62 for an
   ordinary non-label/non-GTO step.
5. `grep -n "static void _showStep\|decodeOneStep(tmpStep)\|void fnBst\|void fnSst"
   src/c47/programming/nextStep.c` shows the shared BST/SST display funnel;
   `fnBst` reaches `_showStep` after moving/defining the previous step, and
   `fnSst` reaches `showStep()` in non-PEM mode.
6. `grep -n "lcd_buffer_pixel_on" src/c47/hal/lcd.h` shows the real
   LCD-buffer probe, and `grep -n "#define clearScreen" src/c47/screen.h`
   shows the production `lcd_fill_rect` clearing macro. `showString` draws
   into this buffer; `screenData` is only the GTK presentation copy and is
   forbidden as the acceptance oracle.
7. `grep -n "test_decode_marker_directions\|test_accept_entry_state_roundtrip"
   packages/forth-core/test_dict_reloc.c` shows both tests registered, and
   `grep -n "test_accept_display_parity" packages/forth-core/test_dict_reloc.c`
   returns nothing (this task adds it).
8. `grep -n "return (markerCount % 2) == 0"
   packages/forth-core/forth_bridge.c` matches exactly once (the mutation
   anchor).

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, identifier, offset,
screen coordinate, or fixture byte does not match the tree, STOP and report
the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f15-3-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, and report. Keep the file updated — mark
   each item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f15-3-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the banners
   and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f15-3-gate.log | head -n 30` or a grep
   for the focused test's name, widening with `-B2 -A8` around at most one
   failure at a time. Never `cat`, `less`, or read the whole log.
4. Edit only flat working files under `packages/forth-core/`. Never edit
   `src/`, generated `patches/`, or generated `files/`; the gate refreshes the
   generated package view. Never touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`; this prompt carries the
   authoritative slice. Never read `items.c`, `config.c`, `lblGtoXeq.c`,
   `forth_inner.c`, or `test_dict_reloc.c` in full. Grep the named anchors and
   read only the specified local slices.
6. Do not change an old-contract test unless this task names it. If a gate
   failure asserts old behavior not listed here, STOP before editing the test.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`. Restore mutations by manually reversing only the mutation
   hunk. Never use `git add -A`.
8. Match local style and keep upstream-derived override files byte-identical
   outside the named mutation. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`design-docs/forth-core/QWEN_PROMPTS_F15_3_display_parity.md`), the todo file
   `/tmp/forth-f15-3-todo.md`, and `git status --short` / `git diff` are the
   ONLY durable truth about task state. If your context is compacted or
   summarized, or you are unsure what you have already done: STOP the current
   step, re-read this packet file and the todo file, run `git status --short`
   and `git diff --stat`, and check for an unrestored `MUTATION APPLIED` marker
   before doing anything else. Never reconstruct packet text, anchors, or code
   blocks from memory — re-read them from this file every time you need them.

**Two-attempt debugger handoff.** After the task implementation first fails a
required command because of your changes, you may make at most two distinct
repair attempts, each followed by the relevant rerun. This does not override
an immediate STOP rule and does not apply to an expected mutation RED. If the
second repair is not green, STOP and report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and relevant verbatim output;
- repair 1 and result;
- repair 2 and result;
- current `git status --short`, `git diff --stat`, and relevant diff;
- remaining hypotheses and surprises.

---

## F15-3 — one real program renders the same marker directions everywhere

### Authority carried by this packet

This task adds the end-to-end drive for DESIGN §8.9 item 4. Production is
already implemented and unit-pinned; there are no production changes:

- A zero-length `ITM_FORTH` step is one marker item whose direction is derived
  at render time. Marker occurrences 1/2/3 render opening/closing/opening:
  `STD_RIGHT_DOUBLE_ANGLE "FORTH"`,
  `"FORTH" STD_LEFT_DOUBLE_ANGLE`,
  `STD_RIGHT_DOUBLE_ANGLE "FORTH"`.
- Internal marker tokens are exactly seven bytes:
  opening = `0x80 0xBB 'F' 'O' 'R' 'T' 'H'` and closing =
  `'F' 'O' 'R' 'T' 'H' 0x80 0xAB`. The standard-font definitions are
  `STD_RIGHT_DOUBLE_ANGLE "\x80\xbb"` and
  `STD_LEFT_DOUBLE_ANGLE "\x80\xab"`.
- `decodeOneStep` is the single decode funnel. `fnPem` uses it for every
  visible program-listing row. `_showStep`, reached by non-PEM `fnSst` and
  `fnBst`, uses the same funnel and leaves the decoded token in `tmpString`
  after rendering.
- The six-step fixture below fits one PEM window exactly: the header occupies
  row 0; label/source/markers/RTN occupy rows 1-6. Marker 1 is at listing row
  2, marker 2 at row 4, marker 3 at row 5. Ordinary marker text is rendered at
  x=62; PEM rows use y=`Y_POSITION_OF_REGISTER_T_LINE + 21 * row`. Those
  screen-coordinate literals come directly from `fnPem` and are normative.
- The PEM surface must be tested as a surface, not by another direct
  `decodeOneStep` unit. Render fixed expected tokens through the standard font
  into a clean calculator `lcd_buffer`, capture their exact pixel rectangles
  with `lcd_buffer_pixel_on`, then call the real `fnPem` and compare its three
  marker rectangles byte-for-byte. `screenData` is not the render buffer: it
  is only the GTK presentation copy updated by `LCD_write_line`; sampling it
  made the first packet revision blind under mutation. This corrected oracle
  adds no production test hook.

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

Read-only (and mutation-only, always restored):

- `packages/forth-core/forth_bridge.c`

### Targeted reads

1. In `test_dict_reloc.c`, grep
   `test_decode_marker_directions\|test_accept_entry_state_roundtrip\|writeTestProgram\|cleanupTestProgram`.
   Read only the complete named tests/helpers and the registration lines around
   `test_accept_entry_state_roundtrip`. Reuse the existing exact byte-token
   assertions and state-cleanup style; do not edit the unit test.
2. In `programming/decode.c`, grep
   `static void decodeRem\|forthMarkerTurnsOn(opcodeStart)\|void decodeOneStep`
   and read only those functions. No edits.
3. In `programming/manage.c`, read `fnPem` only. Confirm the decode call, the
   x-position ternary, row pitch, first-line/header behavior, loop bound, and
   end-of-program breaks. No edits.
4. In upstream `src/c47/programming/nextStep.c` (there is no package override),
   read only `_showStep`, `_bstInPem`, `fnBst`, `showStep`, and `fnSst`. Never
   edit `src/`.
5. In `forth_bridge.c`, read only `forthMarkerTurnsOn` (the mutation anchor).

### Exact fixture

Use this exact array once; all three surfaces drive this same real program:

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'D', '3', 'A',                       /* 1: LBL 'D3A'       (+0)  */
    0x8B, 0x1A, 0xFD, 0x00,                               /* 2: opening »FORTH  (+6)  */
    0x8B, 0x1A, 0xFD, 0x0C, ':', ' ', 'S', 'Q', ' ',       /* 3: : SQ DUP * ;    (+10) */
    'D', 'U', 'P', ' ', '*', ' ', ';',
    0x8B, 0x1A, 0xFD, 0x00,                               /* 4: closing FORTH«  (+26) */
    0x8B, 0x1A, 0xFD, 0x00,                               /* 5: opening »FORTH  (+30) */
    0x04                                                   /* 6: RTN             (+34) */
  };
```

The array is 35 bytes. The source payload `": SQ DUP * ;"` is exactly 12
bytes (`0x0C`). Global step/offset pairs are label `(1,+0)`, marker 1
`(2,+6)`, source `(3,+10)`, marker 2 `(4,+26)`, marker 3 `(5,+30)`, RTN
`(6,+34)`. Transcribe every literal exactly.

### Change 1 — two focused helpers

Add these two `static` helpers next to the new F15-3 test:

1. `accept_copy_screen_rect(uint8_t *dst, int x, int y, int width, int height)`
   loops over every row and column and stores
   `lcd_buffer_pixel_on(x + col, y + row) ? 1 : 0` at
   `dst[row * width + col]`. It performs no rendering and changes no globals.
   Do not read `screenData` here.
2. `accept_marker_token_is(bool_t opening)` returns true only for the exact
   seven-byte NUL-terminated internal token currently in `tmpString`:
   opening requires bytes `0x80 0xBB` followed by `"FORTH"`; closing requires
   `"FORTH"` followed by `0x80 0xAB`. Require `strlen(tmpString) == 7` in both
   branches. Do not use a substring check.

### Change 2 — the focused acceptance test

Add `static int test_accept_display_parity(void)` immediately after
`test_accept_entry_state_roundtrip`, and register it immediately after that
test with the same `printf("  [DEBUG] running ...")`, `fail |= ...();`, and
`forthDictClear();` pattern.

At function entry save all display/program globals the three drives touch:
`currentStep`, `currentProgramNumber`, `currentLocalStepNumber`,
`firstDisplayedStep`, `firstDisplayedLocalStepNumber`,
`pemCursorIsZerothStep`, `calcMode`, `screenUpdatingMode`, `programRunStop`,
`temporaryInformation`, `currentInputVariable`, `programListEnd`,
`lastProgramListEnd`, `tam.mode`, `tam.function`, `FLAG_ALPHA`, and the whole
`aimBuffer`. Restore every saved value on every exit after
`cleanupTestProgram()`.

Require every heap allocation below to succeed; otherwise print a focused
FAIL, release anything already allocated, restore state, clean the fixture if
written, and return 1. Call `clearScreen(0)` before every expected or actual
PEM rendering; this production macro clears the LCD buffer through
`lcd_fill_rect` and is linked in the self-test target. Never clear or sample
`screenData` (and do not call GTK-only `lcd_clear_buf`, which is not linked in
the self-test executable).

Report three independently accumulated subcases, one PASS line each. Do not
return early after a surface mismatch; all surfaces must report before final
cleanup.

**Subcase 1 — the real PEM listing renders all three directions.**

Fixed tokens and geometry:

```c
  const char opening[] = STD_RIGHT_DOUBLE_ANGLE "FORTH";
  const char closing[] = "FORTH" STD_LEFT_DOUBLE_ANGLE;
  const int x = 62;
  const int rowPitch = 21;
  const int marker1Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 2;
  const int marker2Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 4;
  const int marker3Y = Y_POSITION_OF_REGISTER_T_LINE + rowPitch * 5;
  const int rectHeight = 21;
```

Compute opening/closing widths with
`stringWidth(token, &standardFont, false, false)` and require them positive and
within `SCREEN_WIDTH`. Allocate three `uint8_t` expected pixel rectangles (opening at
`marker1Y`, closing at `marker2Y`, opening at `marker3Y`) and one actual
scratch rectangle large enough for the greater width. For each expected
rectangle separately: call `clearScreen(0)`, call `showString` with the exact
token/x/y and `vmNormal, false, false`, then copy its rectangle with
`accept_copy_screen_rect`.

Call `clearScreen(0)` again, write the exact fixture, and configure the listing:
`calcMode = CM_PEM`, `tam.mode = 0`, `tam.function = 0`, `aimBuffer[0] = 0`,
`clearSystemFlag(FLAG_ALPHA)`, `pemCursorIsZerothStep = false`,
`programRunStop = PGM_STOPPED`, `lastErrorCode = ERROR_NONE`;
call `fnGotoDot(2)` and require marker 1 / offset `+6`; set
`firstDisplayedLocalStepNumber = 0`, call `defineFirstDisplayedStep()`, then
call the real `fnPem(NOPARAM)`.

Copy and `memcmp` all three actual marker rectangles against their own
expected rectangle and exact byte count (`width * rectHeight`, one byte per
pixel). Require all three equal. Do not accept a hash, nonblank pixel count,
`tmpString`, `screenData`, or direct `decodeOneStep` as a substitute for the
PEM surface. PASS text:

`[1] PASS: PEM listing renders opening/closing/opening markers`

**Subcase 2 — real SST display matches the PEM directions.**

Keep the same fixture. For each target, reset `calcMode = CM_NORMAL`,
`programRunStop = PGM_STOPPED`, `lastErrorCode = ERROR_NONE`,
`dynamicMenuItem = -1`, `tam.mode = 0`, `aimBuffer[0] = 0`, and clear
`FLAG_ALPHA`. Use these exact arrays:

```c
  const uint16_t steps[] = { 2, 4, 5 };
  const uint16_t offsets[] = { 6, 26, 30 };
  const bool_t openingExpected[] = { true, false, true };
```

For each element, call `fnGotoDot(steps[i])`; require the exact local step and
`currentStep == beginOfProgramMemory + offsets[i]`; call `fnSst(NOPARAM)`;
require `lastErrorCode == ERROR_NONE` and
`accept_marker_token_is(openingExpected[i])`. Reset `programRunStop` before
the next drive because `fnSst` sets `PGM_SINGLE_STEP`. Only after all three
directions match, print:

`[2] PASS: SST display matches PEM marker directions`

**Subcase 3 — real BST display matches the PEM directions.**

Reset the same transient state before each element. Start one step after each
target so `fnBst` performs the real backward movement:

```c
  const uint16_t startSteps[] = { 3, 5, 6 };
  const uint16_t startOffsets[] = { 10, 30, 34 };
  const uint16_t targetSteps[] = { 2, 4, 5 };
  const uint16_t targetOffsets[] = { 6, 26, 30 };
  const bool_t openingExpected[] = { true, false, true };
```

For each element, call `fnGotoDot(startSteps[i])`, require the exact start
step/offset, then call `fnBst(NOPARAM)`. Require `lastErrorCode == ERROR_NONE`,
the exact target local step/offset after the backward move, and
`accept_marker_token_is(openingExpected[i])`. Only after all three directions
match, print:

`[3] PASS: BST display matches PEM marker directions`

### Existing tests and comments

All existing tests stay green unchanged — this task adds two small test
helpers and one test, and touches no production code. If any existing test
reddens in the normal green runs, STOP (rule 6).

### Non-goals / STOP boundaries

- No production edits to `decode.c`, `manage.c`, `nextStep.c`, screen/font
  code, or `forth_bridge.c` outside the one temporary parity mutation, which
  must be restored.
- No console-stdout capture and no substitution of `listPrograms()` for the
  actual PEM LCD-buffer surface. Never use `screenData` as the oracle.
- No new production test hook, cached render state, second marker item, or
  saved direction byte.
- No entry-state, glyph, literal-type, or XEQ-name work (F15-4/F15-5 own those
  §8.9 items).
- No DESIGN or history edits.

### Gate and required mutation

Run the full gate green first. Then change only this line at the end of
`forthMarkerTurnsOn` in `forth_bridge.c`:

```c
    return (markerCount % 2) == 0;
```

to:

```c
    return (markerCount % 2) != 0;  /* MUTATION F15-3 */
```

Run the full sanctioned gate. All three F15-3 subcases must go RED: the PEM
pixel rectangles are reversed, and every SST/BST exact token direction is
wrong. The existing unit marker/decode tests may redden collaterally; record
the focused F15-3 symptoms and do not edit those tests.

Restore the original line manually. Grep for `MUTATION F15-3` (there must be
no match) and require `git diff` restricted to
`packages/forth-core/forth_bridge.c` to be empty. Run the full gate green again
and record:

- all three PASS lines;
- both success banners and exit 0;
- `FORTH ARENA: dict here=... sizeBlocks=...` (the §5.4 acceptance-run arena
  report is mandatory for every F1.5 packet);
- `git diff --check`;
- byte equality between `test_dict_reloc.c` and its generated `files/`
  counterpart.

### Commit

After the final green gate, `git status --short` may contain only
`packages/forth-core/test_dict_reloc.c`, its generated `files/` counterpart,
and `packages/forth-core/.refresh-manifest.json`. Stage those exact paths only
and commit:

```text
forth-core: F15-3 — end-to-end marker display parity (§8.9 item 4)
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the new test's three PASS lines, the mutation's RED symptoms for all
three surfaces, the final gate and arena lines, commit hash, and anything
surprising — explicitly including any fixture offset, LCD-buffer coordinate,
BST/SST movement, or shared-render-funnel assertion in this packet that did
not match observed behavior (that is architect feedback, not something to
adapt around).
