# Stage F15-4 — glyph operators + integer literal type parity (bounded implementation prompt)

Origin: DESIGN §8.9 items 5 and 6, against the landed F15-3 tree
(`c8b87dfa8`). Stage ledger: `QWEN_PROMPTS_F15_harness.md`. Authored
2026-07-17 after tracing the real alpha-capture and RPN-NIM paths. Every
payload and complete program byte count below was machine-verified.

> **AMENDMENT (2026-07-18, executed as landed in `6775252bf` — the landed
> test is normative over this packet's drive text).** The debug session on
> the original run found two packet defects. (1) DRIVE: capture must be
> opened with `runFunction(ITM_AIM)` BEFORE the first text key, with the
> cursor on the OPENING marker (`fnGotoDot(2)`), asserting
> `FLAG_ALPHA && tam.function == ITM_FORTH` before typing; the fixture must
> also set `pemCursorIsZerothStep = false`. Reason: only the ALPHA route
> consults `forthEntryStateAtInsertion()` AFTER `addStepInProgram`'s
> pre-move (governing predecessor = the opening marker); a leading `':'`
> or digit consults it WITHOUT the pre-move, sees the LBL, and lands in
> RPN/number entry. (2) MUTATION 1 below was REPLACED: disabling the
> `PRIM_DIVGL` alias row escapes — since R1-3, §4.1 step 4's item fallback
> resolves `÷` to the native divide item and the program still runs. The
> valid mutation is the capture store in `programming/manage.c`
> (`itemSoftmenuName` → `itemCatalogName` at the aimBuffer append): glyphs
> vanish from the captured line and both glyph subcases' byte comparisons
> go RED while subcase 3 stays green.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes` and
   `git status --short` is empty.
2. `git log --oneline -5` contains a commit whose subject is exactly
   `forth-core: F15-3 — end-to-end marker display parity (§8.9 item 4)`.
3. `grep -n "#define STD_CROSS\|#define STD_DIVIDE" src/c47/fonts.h`
   shows `"\x80\xd7"` and `"\x80\xf7"`; `grep -n "#define ITM_CROSS\|#define ITM_OBELUS"
   packages/forth-core/items.h` shows `855` and `857`.
4. `grep -n "/\\*  *855 \\*/\|/\\*  *857 \\*/" packages/forth-core/items.c`
   shows both items routed to `addItemToBuffer` with softmenu names
   `STD_CROSS` / `STD_DIVIDE`.
5. In `packages/forth-core/programming/manage.c`, the `pemAlpha` printable
   item arm computes `stringByteLength(indexOfItems[item].itemSoftmenuName)`
   and copies those exact bytes into `aimBuffer`; in `forth_compile.c`,
   `nextToken` advances with `stringNextGlyph` until the exact space delimiter.
6. `grep -n "PRIM_CROSS\|PRIM_DIVGL" packages/forth-core/forth_prims.c`
   shows designated rows mapping `STD_CROSS` to `pMul` and `STD_DIVIDE` to
   `pDiv`; `forthFindPrim` in `forth_dict.c` walks every row and compares
   `.name`, so deleting a designated row would create an unsafe NULL hole.
7. `grep -n "void addItemToNimBuffer\|void closeNim" src/c47/bufferize.c`
   shows both real RPN entry functions. The digit switch includes `ITM_7`,
   starts NIM from `CM_NORMAL`, and `closeNim` converts `NP_INT_10` through
   `convertLongIntegerToLongIntegerRegister` when `Input_Default == ID_LI`.
8. `grep -n "void forthPushInt32\|int32ToLongInteger\|convertLongIntegerToLongIntegerRegister"
   packages/forth-core/forth_inner.c` shows the Forth literal path producing a
   long integer.
9. `grep -n "test_outer_glyph_cross\|test_outer_glyph_divide\|test_ilit_compile_interpret_parity\|test_accept_display_parity"
   packages/forth-core/test_dict_reloc.c` shows all four tests registered, and
   `grep -n "test_accept_glyph_type_parity" packages/forth-core/test_dict_reloc.c`
   returns nothing (this task adds it).

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`. You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions. If a quoted anchor, function, test, branch, identifier, item,
offset, payload, or fixture byte does not match the tree, STOP and report the
mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`. The tree must be clean before any edit. Otherwise STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f15-4-todo.md` (outside the repo): one item per file, test
   subcase, mutation, final gate, and report. Keep the file updated — mark each
   item in progress/completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` the moment either
   happens. Do not report success with an open item.
3. The only build/test command is
   `./packages/forth-core/build-test.sh`. Success requires exit 0 plus both
   `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.` Never
   invoke meson or ninja directly. The gate prints thousands of lines —
   NEVER let its raw output into your context. Always run it log-captured:

   `./packages/forth-core/build-test.sh > /tmp/forth-f15-4-gate.log 2>&1; echo "gate exit: $?"`

   Then inspect ONLY bounded slices of the log: `tail -n 12` for the banners
   and arena line, and targeted greps such as
   `grep -n "FAIL\|error:" /tmp/forth-f15-4-gate.log | head -n 30` or a grep
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
8. Match local style and keep production files byte-identical outside the
   named temporary mutations. Report the arena line and anything surprising.
9. Small-context recovery. This packet on disk
   (`packages/forth-core/QWEN_PROMPTS_F15_4_glyph_type_parity.md`), the todo
   file `/tmp/forth-f15-4-todo.md`, and `git status --short` / `git diff` are
   the ONLY durable truth about task state. If your context is compacted or
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

## F15-4 — alpha-authored glyphs compile, and keypad/Forth literals share a type

### Authority carried by this packet

This task adds the end-to-end drives for DESIGN §8.9 items 5 and 6. Production
is already implemented and unit-pinned; there are no production changes:

- Alpha capture stores the exact `itemSoftmenuName` bytes. `ITM_CROSS` stores
  `STD_CROSS` = `0x80 0xD7`; `ITM_OBELUS` stores `STD_DIVIDE` =
  `0x80 0xF7`. Each is one glyph but two bytes.
- The Forth tokenizer advances glyph-wise and the primitive table resolves
  those tokens through `PRIM_CROSS` / `PRIM_DIVGL` to multiply/divide.
- The design's exact division definition is `: D2 2 ÷ ;`; this packet authors
  it through the real alpha capture, authors `8 D2` as the following source
  step, and runs the real labeled program to `X == 4`, `dtLongInteger`.
- The symmetric cross drive authors `: M2 2 × ;`, then `3 M2`, and requires
  `X == 6`, `dtLongInteger`. Keeping it separate makes the DIVGL mutation
  prove that divide fails while cross remains live.
- RPN keypad acceptance is the real NIM path:
  `addItemToNimBuffer(ITM_7)` followed by `closeNim()`, with
  `Input_Default = ID_LI`. Forth acceptance is a real source step containing
  the one-byte payload `"7"`, run by label. Both must leave X as value 7 with
  data type `dtLongInteger`.

### Files

Modify only:

- `packages/forth-core/test_dict_reloc.c`

Read-only (and mutation-only, always restored):

- `packages/forth-core/forth_prims.c`
- `packages/forth-core/forth_inner.c`

### Targeted reads

1. In `test_dict_reloc.c`, read only the complete functions
   `test_outer_glyph_cross`, `test_outer_glyph_divide`,
   `test_ilit_compile_interpret_parity`, `test_forth_capture_survives_keystroke`,
   `test_accept_run_lifecycle`, `writeTestProgram`, `cleanupTestProgram`, and
   the registration lines around `test_accept_display_parity`. Reuse their
   state cleanup, label drive, and `x_is_longint` patterns.
2. In `programming/manage.c`, read only the printable-item arm of `pemAlpha`,
   the `ITM_FORTH` placeholder commit in `pemCloseAlphaInput`, E2, and
   `addStepInProgram`. In `items.c`, read only rows 542/543/547/548/553/562,
   806/822/823/855/857 and the `runFunction` PEM dispatch. No edits.
3. In `forth_compile.c`, read only `nextToken`; in `forth_prims.c`, read only
   the enum/table through `forthPrimCount`; in `forth_dict.c`, read only
   `forthFindPrim`. No normal edits.
4. In upstream `src/c47/bufferize.c`, read only `addItemToNimBuffer` from its
   `CM_NORMAL` initialization through the digit case, and `closeNim` through
   its `NP_INT_10` conversion. Never edit `src/`.
5. In `forth_inner.c`, read only `forthPushInt32` (mutation anchor).

### Change — the focused acceptance test

Add `static int test_accept_glyph_type_parity(void)` immediately after
`test_accept_display_parity`, and register it immediately after that test with
the same `printf("  [DEBUG] running ...")`, `fail |= ...();`, and
`forthDictClear();` pattern.

At function entry save every program/input global touched by the existing
alpha/E2/NIM tests, including `currentStep`, `currentProgramNumber`,
`currentLocalStepNumber`, `firstDisplayedStep`,
`firstDisplayedLocalStepNumber`, `pemCursorIsZerothStep`, `calcMode`,
`catalog`, `tam.mode`, `tam.function`, `FLAG_ALPHA`, `FLAG_NUMIN`,
`FLAG_NUMLOCK`, `alphaCase`, `nextChar`, `shiftF`, `shiftG`,
`programRunStop`, `dynamicMenuItem`, `Input_Default`, `nimNumberPart`,
`lastIntegerBase`, `T_cursorPos`, the current softmenu, and the whole
`aimBuffer`. Restore them on every exit; call `cleanupTestProgram()` and
`forthDictClear()` between subcases and on every failure path.

Report three independently accumulated subcases, one PASS line each. Apart
from an unrecoverable fixture write, do not return early: both mutations must
show every affected subcase before cleanup.

#### Common alpha-authoring drive for subcases 1 and 2

Start each subcase from its exact 15-byte seed fixture:

```c
  uint8_t seedM[] = {
    0x01, 0xFD, 0x03, 'G', '4', 'M',
    0x8B, 0x1A, 0xFD, 0x00,
    0x8B, 0x1A, 0xFD, 0x00,
    0x04
  };
  uint8_t seedD[] = {
    0x01, 0xFD, 0x03, 'G', '4', 'D',
    0x8B, 0x1A, 0xFD, 0x00,
    0x8B, 0x1A, 0xFD, 0x00,
    0x04
  };
```

After `writeTestProgram`, configure `calcMode = CM_PEM`,
`catalog = CATALOG_NONE`, `tam.mode = 0`, `tam.function = 0`,
`aimBuffer[0] = 0`, `programRunStop = PGM_STOPPED`,
`dynamicMenuItem = -1`, `alphaCase = AC_UPPER`, `nextChar = NC_NORMAL`,
`shiftF = false`, `shiftG = false`, clear `FLAG_ALPHA` and `FLAG_NUMLOCK`,
and call `fnGotoDot(2)`. Require
step 2 / `currentStep == beginOfProgramMemory + 6`.

Every authored key goes through `runFunction(item)`. Do not assign text into
`aimBuffer`, call `pemAlpha` directly, or write the finished source bytes.
The first printable key opens E2 capture; ENTER after a non-empty line commits
it and opens the next capture; the final second ENTER is the empty-line rule
that deletes the last placeholder.

**Subcase 1 — §8.9 item 5, alpha-authored cross.**

Run these exact item arrays in order:

```c
  const int16_t defItems[] = {
    ITM_COLON, ITM_SPACE, ITM_M, ITM_2, ITM_SPACE, ITM_2,
    ITM_SPACE, ITM_CROSS, ITM_SPACE, ITM_SEMICOLON, ITM_ENTER
  };
  const int16_t useItems[] = {
    ITM_3, ITM_SPACE, ITM_M, ITM_2, ITM_ENTER, ITM_ENTER
  };
```

Require the committed program region is exactly 38 bytes and byte-equal to:

```c
  uint8_t expected[] = {
    0x01, 0xFD, 0x03, 'G', '4', 'M',
    0x8B, 0x1A, 0xFD, 0x00,
    0x8B, 0x1A, 0xFD, 0x0B,
    ':', ' ', 'M', '2', ' ', '2', ' ', 0x80, 0xD7, ' ', ';',
    0x8B, 0x1A, 0xFD, 0x04, '3', ' ', 'M', '2',
    0x8B, 0x1A, 0xFD, 0x00,
    0x04
  };
```

Resolve `G4M` with `findNamedLabel(..., GLOBAL_LABELS)`, run through
`fnExecute`, and require no error plus `x_is_longint(6)`. PASS text:

`[1] PASS: alpha-authored cross bytes run 3 M2 -> 6`

**Subcase 2 — §8.9 item 5, alpha-authored divide (design fixture).**

Run these exact item arrays in order:

```c
  const int16_t defItems[] = {
    ITM_COLON, ITM_SPACE, ITM_D, ITM_2, ITM_SPACE, ITM_2,
    ITM_SPACE, ITM_OBELUS, ITM_SPACE, ITM_SEMICOLON, ITM_ENTER
  };
  const int16_t useItems[] = {
    ITM_8, ITM_SPACE, ITM_D, ITM_2, ITM_ENTER, ITM_ENTER
  };
```

Require the committed program region is exactly 38 bytes and byte-equal to:

```c
  uint8_t expected[] = {
    0x01, 0xFD, 0x03, 'G', '4', 'D',
    0x8B, 0x1A, 0xFD, 0x00,
    0x8B, 0x1A, 0xFD, 0x0B,
    ':', ' ', 'D', '2', ' ', '2', ' ', 0x80, 0xF7, ' ', ';',
    0x8B, 0x1A, 0xFD, 0x04, '8', ' ', 'D', '2',
    0x8B, 0x1A, 0xFD, 0x00,
    0x04
  };
```

This preserves the design literal `: D2 2 ÷ ;` exactly: its internal payload
is 11 bytes because `÷` is one two-byte glyph. Resolve `G4D`, run through
`fnExecute`, and require no error plus `x_is_longint(4)`. PASS text:

`[2] PASS: alpha-authored divide bytes run 8 D2 -> 4`

**Subcase 3 — §8.9 item 6, RPN-keypad/Forth-source type parity.**

RPN half: restore neutral input state, set `calcMode = CM_NORMAL`,
`Input_Default = ID_LI`, `nimNumberPart = NP_EMPTY`, `aimBuffer[0] = 0`,
call `setLastintegerBasetoZero()`, then call exactly
`addItemToNimBuffer(ITM_7)` and `closeNim()`. Require no error and
`x_is_longint(7)`. This exact digit+close pair is the RPN keypad acceptance;
do not replace it with `forthPushInt32` or a direct register write.

Before the Forth half, deliberately seed X as **dtReal34 value 42** so a
no-op source step cannot inherit the passing RPN result:

```c
  real34_t seed;
  int32ToReal34(42, &seed);
  reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
  real34Copy(&seed, REGISTER_REAL34_DATA(REGISTER_X));
```

Then write this exact 20-byte real program:

```c
  uint8_t prog[] = {
    0x01, 0xFD, 0x03, 'T', '4', 'A',
    0x8B, 0x1A, 0xFD, 0x00,
    0x8B, 0x1A, 0xFD, 0x01, '7',
    0x8B, 0x1A, 0xFD, 0x00,
    0x04
  };
```

Resolve `T4A`, drive `fnExecute` with the same last-error/dynamic-menu/
program-stop discipline as F15-1, and require no error plus
`x_is_longint(7)`. Only after both halves pass, print:

`[3] PASS: RPN-keypad 7 and Forth-source 7 both leave dtLongInteger`

### Existing tests and comments

All existing tests stay green unchanged — this task adds one test and touches
no production code. If any existing test reddens in the normal green runs,
STOP (rule 6).

### Non-goals / STOP boundaries

- No production edits to alpha/PEM, tokenizer, dictionary, primitive, NIM, or
  inner-interpreter code outside the two temporary mutations, both restored.
- Do not delete the designated `PRIM_DIVGL` row: that creates a NULL `.name`
  hole and can crash `forthFindPrim`; the specified rename mutation is the
  only safe alias mutation.
- No display-parity or XEQ-name work (F15-5 owns the remaining §8.9 item 10).
- No DESIGN or history edits.

### Gate and required mutations

Run the full gate green first. Then run each mutation separately using the
full sanctioned gate, manually restoring its exact hunk before continuing.

1. In `forth_prims.c`, rename only the divide-glyph alias row from:

   ```c
     [PRIM_DIVGL]   = { STD_DIVIDE, 0, pDiv },
   ```

   to:

   ```c
     [PRIM_DIVGL]   = { "__F15_DIVGL_DISABLED__", 0, pDiv }, /* MUTATION F15-4-1 */
   ```

   Subcase 1 must stay PASS. Subcase 2 must go RED with
   `ERROR_FUNCTION_NOT_FOUND` while compiling the alpha-authored `÷` token;
   record the exact observed error. Existing direct glyph-divide units may
   redden collaterally.

2. In `forthPushInt32` (`forth_inner.c`), replace only the long-integer
   conversion block:

   ```c
       longInteger_t lgInt;
       longIntegerInit(lgInt);
       int32ToLongInteger(v, lgInt);
       convertLongIntegerToLongIntegerRegister(lgInt, REGISTER_X);
       longIntegerFree(lgInt);
   ```

   with:

   ```c
       reallocateRegister(REGISTER_X, dtReal34, 0, amNone); /* MUTATION F15-4-2 */
       int32ToReal34(v, REGISTER_REAL34_DATA(REGISTER_X));
   ```

   In subcase 3 the RPN half must remain a value-7 `dtLongInteger`, while the
   Forth-source half becomes value 7 with the wrong `dtReal34` type and goes
   RED. Record that split. Glyph subcases and existing literal tests may
   redden collaterally.

After both mutation runs, restore every production hunk manually. Grep for
`MUTATION F15-4` (there must be no match) and require `git diff` restricted to
`forth_prims.c` / `forth_inner.c` and their generated counterparts to be
empty. Run the full gate green again and record:

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
forth-core: F15-4 — glyph operators + literal type parity (§8.9 items 5, 6)
```

If any unrelated path is dirty, STOP and report instead of committing.

### Report

Report the new test's three PASS lines, both mutations' focused RED symptoms,
the final gate and arena lines, commit hash, and anything surprising —
explicitly including any alpha item byte, fixture offset, NIM-close behavior,
or register type in this packet that did not match observation (that is
architect feedback, not something to adapt around).
