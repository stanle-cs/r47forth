# G1 — the FWRD picker's softkey→word mapping — Part A

Origin: the 2026-08-03 post-migration coverage review. The picker's
CONTENT is pinned to the project standard (order, sections, dedup,
14-byte filter, 170 cap, tokenizer). Everything DOWNSTREAM of the
content array is not. Every landed insert test assigns
`dynamicMenuItem = 0` by hand, so index 0 on page 1 unshifted is the
only softkey the suite has ever pressed. The real derivation is

```c
case MNU_FORTH: {
  dynamicMenuItem = firstItem + itemShift + fn;
  item = ITM_NOP;              // press handled in executeFunction (§9.6 P-H7)
  break;
}
```

in `determineFunctionKeyItem_C47` (forth-core's own patch to
`src/c47/keyboard.c`), with `itemShift = shiftF ? 6 : shiftG ? 12 : 0`
and `fn = data[0] - '0' - 1`. Unlike the `MNU_VAR`/`MNU_PROG` arms
beside it, the `MNU_FORTH` arm does NOT bound `dynamicMenuItem` against
`numItems` — the only bound is the `dynamicMenuItem <
dynamicSoftmenu[...].numItems` conjunct inside `forthPickerGuard`, and
that conjunct has no pin. Behind it, `dynmenuGetLabel()` returns `""`
out of range, which `forthCapInsertName("")` would turn into a bare
space inserted into the user's Forth line.

This is the D3-5 lesson repeating one layer up: the battery drives the
helper with the index preset, not the entry the user reaches. Authored
per runbook §4a.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` → `upstream/migrate-2026-08-03`.
   `git status --short` → empty.
2. `grep -c "dynamicMenuItem = 0;" packages/forth-core/test_capture.part.h`
   → exactly 5, at lines 694, 819, 942, 1016 and 5680 (the three insert
   tests, the guard-identity test, and one sim-bench case). Every one of
   them hand-sets the index; that is the premise of this packet. If the
   count grew, a later session added another — STOP and bring it to the
   architect, because the new one may already cover a subcase here.
3. `grep -c "case MNU_FORTH:" packages/forth-core/keyboard.c` → exactly 1.
4. `grep -c "dynamicMenuItem < dynamicSoftmenu\[softmenuStack\[0\].softmenuId\].numItems" packages/forth-core/forth_menu.c`
   → exactly 1. (The guard conjunct subcase 4 pins.)
5. `grep -c "firstItem" packages/forth-core/test_capture.part.h` → exactly 0.
   (No paging pin exists yet; this packet creates the first.)
6. `grep -n "int16_t determineFunctionKeyItem_C47" packages/forth-core/keyboard.c`
   → one hit, and the line does NOT begin with `static`. (The test calls
   it directly; if it were static this packet cannot run — STOP.)

## The shared fixture — copy it, do not invent it

All five subcases use ONE picker of exactly **20 names**, so that
`numItems = 20` spans four pages of six with a partial last page
(indices 18, 19 present; 20..23 blank).

Build it by copying the fixture slice of the LANDED
`test_picker_capacity_boundary` in
`packages/forth-core/test_capture.part.h` **verbatim**, changing only
`totalDefs` from 171 to 20. That slice already establishes every byte
layout you need:

- step header is `0x8B 0x1A 0xFD <len>`, `len = 10` for `": NNNN 1 ;"`,
- names are `sprintf(name, "N%03d", i)` → `N000`..`N019`, 4 bytes, distinct,
- program is an opening marker step (`len = 0`), the definition steps,
  then a closing marker step,
- `writeTestProgram(prog, progLen)` writes it, `cleanupTestProgram()`
  tears it down.

Section (a) is sorted, and `N000`..`N019` are already in sort order, so
**picker index i holds name `N0ii`** — index 0 → `"N000"`, index 7 →
`"N007"`, index 19 → `"N019"`. Every assert below uses that identity.
Do not re-derive it; do not sort anything yourself.

For the capture-open half of the fixture, copy the drive slice of the
landed `test_picker_insert_at_cursor` **verbatim** — the save block, the
`calcMode = CM_PEM` / `catalog = CATALOG_NONE` / `tam.mode = 0` /
`tam.function = 0` / `aimBuffer[0] = 0` / `programRunStop = PGM_STOPPED`
/ `dynamicMenuItem = -1` / `pemCursorIsZerothStep = false` /
`clearSystemFlag(FLAG_ALPHA)` preamble, the `runFunction(ITM_AIM)` call,
the `FIXTURE BUG` assert on `getSystemFlag(FLAG_ALPHA) && tam.function
== ITM_FORTH`, and the cleanup tail. That is the CAPTURE-DRIVE CONTRACT;
reproduce it, do not shorten it.

**Armed-state assert (runbook §4a rule 3).** Before the act in every
subcase, assert `dynamicSoftmenu[22].numItems == 20`. If it is not 20,
print `    FIXTURE BUG: picker has %d names, expected 20` and STOP that
subcase — do NOT adjust anything to make it 20.

## The drive call — exact, not a gesture

The softkey press is:

```c
extern int16_t determineFunctionKeyItem_C47(const char *data, bool_t shiftF, bool_t shiftG);
int16_t it = determineFunctionKeyItem_C47("3", false, false);
```

- `data` is a **one-character string** `"1"`..`"6"`; `fn = data[0] - '0' - 1`,
  so `"1"` → fn 0 … `"6"` → fn 5.
- `shiftF = true` adds 6; `shiftG = true` adds 12; both false adds 0.
- The page comes from `softmenuStack[0].firstItem`, which you set
  directly before the call.
- `softmenuStack[0].softmenuId` must be `22` (the MNU_FORTH slot) before
  the call, exactly as the landed insert tests set it.

The press then goes through the guard and the insert:

```c
extern bool_t forthPickerGuard(int16_t item);
extern bool_t pickerInsertName(void);
if (forthPickerGuard(it)) { pickerInsertName(); }
```

Drive the guard and the insert **through `forthPickerGuard(it)` with the
`it` the mapping returned** — never call `pickerInsertName()`
unconditionally, and never set `dynamicMenuItem` by hand in this packet.
That is the whole point of it.

Read the resulting line with `forthTestCapText()` (as the landed tests
do). `T_cursorPos` is set to 0 before each act unless a subcase says
otherwise.

## Subcases — five, in this order

**[1] Index ≥ 1 on the first page.** `firstItem = 0`, key `"3"`,
unshifted. Assert `it == ITM_NOP`, `dynamicMenuItem == 2`, and after the
guarded insert `forthTestCapText()` is `"N002 "`.
PASS line, exact text:
`    [1] PASS: unshifted key 3 on page 1 selects index 2 and inserts N002`

**[2] The shift rows.** Same `firstItem = 0`. Two acts on a freshly
emptied line each time (`aimBuffer[0] = 0; T_cursorPos = 0;` between
them):
- key `"1"`, `shiftF = true`, `shiftG = false` → `dynamicMenuItem == 6`,
  text `"N006 "`;
- key `"2"`, `shiftF = false`, `shiftG = true` → `dynamicMenuItem == 13`,
  text `"N013 "`.
PASS line:
`    [2] PASS: f-shift adds 6 and g-shift adds 12 to the selected index`

**[3] Paging.** `firstItem = 6`, key `"1"`, unshifted → `dynamicMenuItem
== 6`, text `"N006 "`. Then `firstItem = 12`, key `"6"`, unshifted, on a
freshly emptied line → `dynamicMenuItem == 17`, text `"N017 "`.
PASS line:
`    [3] PASS: firstItem pages the selection — 6+0 and 12+5 resolve to N006 and N017`

**[4] The blank key on the partial last page — the load-bearing one.**
`firstItem = 18`, so indices 18 and 19 are the only live keys. Set the
capture line to a known non-empty state first: with `firstItem = 18`,
key `"1"` (index 18) → text `"N018 "`, `T_cursorPos == 5`. Then, WITHOUT
clearing, key `"5"` (index 22, past `numItems = 20`):
- assert `dynamicMenuItem == 22` (the mapping arm does not clamp — this
  is the documented shape, pin it),
- assert `forthPickerGuard(it) == false`,
- run the guarded insert as written above (it must therefore not fire),
- assert `forthTestCapText()` is still exactly `"N018 "` and
  `T_cursorPos` is still 5 — **no stray space appended**.
PASS line:
`    [4] PASS: blank key past numItems refuses — guard false, line unchanged`

**[5] The draw path at every page.** For `firstItem` in `{0, 6, 12, 18}`,
in that order:
- `softmenuStack[0].firstItem = firstItem;`
- `extern void showSoftmenuCurrentPart(void); showSoftmenuCurrentPart();`
- assert `dynamicSoftmenu[22].numItems == 20` and
  `dynamicSoftmenu[22].menuContent != NULL` still, after the draw;
- assert `dynmenuGetLabel(firstItem)` equals the expected `N0ii` — this
  is the same index walk the draw loop performs via
  `getNthString(dynamicSoftmenu[m].menuContent, currentFirstItem)`, so
  it pins the label the renderer receives for the first key of each page.

Use `extern char *dynmenuGetLabel(int16_t menuitem);`. Do not call
`showSoftmenu()` inside this subcase — it resets `firstItem`, which is
the variable under test; set `softmenuStack[0].firstItem` directly, as
subcase 3 does.
PASS line:
`    [5] PASS: draw path survives every page; first label of each page is correct`

**Scope note, state it in the report and do not act on it:** subcase 5
pins the label the renderer is handed, not the pixels it draws. Pixel-
level rendering (`showSoftkey`, the combined-key `trimSoftKeyName` path)
stays unpinned; closing that needs an LCD read-back harness, which is an
owner decision and NOT part of this packet. Do not add one.

## Registration

`packages/forth-core/test_dict_reloc.c`, two edits, both anchored on the
existing `test_picker_insert_at_cursor` lines (unique strings, runbook
§4a rule 5):

1. After the forward declaration `static int test_picker_insert_at_cursor(void);`
   add `static int test_picker_key_mapping(void);`
2. After the pair
   ```c
   printf("  [DEBUG] running test_picker_insert_at_cursor...\n");
   fail |= test_picker_insert_at_cursor();
   ```
   add
   ```c
   printf("  [DEBUG] running test_picker_key_mapping...\n");
   fail |= test_picker_key_mapping();
   ```

The five subcases live in ONE function `test_picker_key_mapping(void)`
in `packages/forth-core/test_capture.part.h`, placed immediately after
`test_picker_guard_menu_identity`'s closing brace, following the landed
`sc1`..`sc5` accumulation style of `test_capture_menus` (each subcase
sets its own `sc<N>`, prints its own PASS, and `fail |= scN`).

## Gate

```
./packages/forth-core/build-test.sh > /tmp/gate-g1.log 2>&1; echo $?; tail -n 12 /tmp/gate-g1.log
```

Log path is `/tmp/gate-g1.log` for this packet and overrides any
remembered log name. Green means exit 0 AND all five `[N] PASS:` lines
present in the log.

Report the arena high-water line from the gate log. No dictionary change
is expected in this packet (tests only), so the arena number must match
the pre-packet run; if it moved, STOP and report.

## Commit

One commit, message exactly:

```
forth-core: G1 — pin the FWRD picker's softkey-to-word mapping

Every landed insert test set dynamicMenuItem by hand, so index 0 on page
1 unshifted was the only softkey the suite had pressed. The mapping
itself — firstItem + itemShift + fn in determineFunctionKeyItem_C47's
MNU_FORTH arm — had no pin, and neither did forthPickerGuard's
numItems bound, the only thing standing between a blank key on a partial
page and a bare space inserted into the user's line.

Five subcases on one 20-name picker: index >= 1, the f/g shift rows,
firstItem paging, the blank-key refusal, and the draw path at every page
boundary. All drive determineFunctionKeyItem_C47 with a real softkey
string and route the press through forthPickerGuard.

Pixel-level rendering stays unpinned by design; subcase 5 pins the label
the renderer is handed.
```

## Part B

NOT in this file. The architect derives the mutation/restore blocks from
the landed line numbers after this packet commits green — one mutation
per subcase, each run in a fresh session, per runbook §4a. Do not invent
mutations here, and do not "verify" a subcase by editing the code it
tests.
