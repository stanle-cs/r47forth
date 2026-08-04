# G2 — the picker's scan cut-off and its allocation — Part A

Origin: the same 2026-08-03 coverage review as G1. Two items in
`packages/forth-core/forth_menu.c`'s `forthBuildWordPicker` have no pin
at all:

1. **The 1000-step scan cut-off.** `if (stepCount > 1000) break;` is
   marked in the source as a "behavioral limit … §9.6 documented
   deviation". Documented, never pinned — a session that changed the
   constant, or dropped the break, would pass the whole gate.
2. **The content allocation.** `ptr = calloc(1, numberOfBytes);` is
   stored into `dynamicSoftmenu[menu].menuContent` and then written
   through on the next line with no NULL test. This matches upstream's
   own habit (six unchecked `malloc`s for `menuContent` in
   `src/c47/softmenus.c`), so it is a house pattern rather than a
   forth-core lapse — but ours is the one we own, the guard is one
   branch, and the empty-picker path is worth a pin regardless.

**GATE LOCKED**: G1 must be committed green before this packet's
execution gate can open. Authored per runbook §4a.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` → `upstream/migrate-2026-08-03`.
   `git status --short` → empty.
2. `git log --oneline -1` names the G1 commit
   (`forth-core: G1 — pin the FWRD picker's softkey-to-word mapping`).
   If it does not, this packet is gate-locked — STOP.
3. `grep -c "stepCount > 1000" packages/forth-core/forth_menu.c` → exactly 1.
4. `grep -c "calloc(1, numberOfBytes)" packages/forth-core/forth_menu.c` → exactly 1.
5. `grep -c "FORTH_PICKER_MAX_SCAN_STEPS" packages/forth-core/forth_menu.c` → exactly 0
   (step 1 introduces it).

## Step 1 — name the constant (forth_menu.c)

The pin below must reference the same number the code uses, or the two
drift. In `packages/forth-core/forth_menu.c`, immediately above
`void forthBuildWordPicker(int16_t menu)`, add:

```c
/* Scan cut-off for the picker's program-text pass (§9.6 documented
 * deviation): programs with more steps than this before the cursor are
 * not fully scanned. Named so the pin and the code cannot drift apart. */
#define FORTH_PICKER_MAX_SCAN_STEPS 1000
```

and change the loop's guard from

```c
      if (stepCount > 1000) break; /* behavioral limit: programs with >1000 steps are not fully scanned (§9.6 documented deviation) */
```

to

```c
      if (stepCount > FORTH_PICKER_MAX_SCAN_STEPS) break; /* §9.6 documented deviation */
```

Behavior is unchanged. Do not change the number.

## Step 2 — guard the allocation (forth_menu.c)

Replace

```c
  ptr = calloc(1, numberOfBytes);
  dynamicSoftmenu[menu].menuContent = ptr;
```

with

```c
  ptr = calloc(1, numberOfBytes);
  if (ptr == NULL) {
    /* Out of heap: leave the menu empty rather than writing through NULL.
     * An empty picker is a menu with no keys, which showSoftmenuCurrentPart
     * already renders (its numberOfItems == 0 arm blanks all six). */
    dynamicSoftmenu[menu].menuContent = NULL;
    dynamicSoftmenu[menu].numItems = 0;
    return;
  }
  dynamicSoftmenu[menu].menuContent = ptr;
```

The NULL branch itself is not reachable from the suite without an
allocator hook, and this packet does NOT add one — say so in the report
rather than inventing a way to force it.

## Subcases — two

**[1] The scan cut-off.** Build a program whose definition steps run
PAST the cut-off, and prove the far one is not listed while a near one
is.

Fixture: copy the fixture slice of the landed
`test_picker_capacity_boundary` in
`packages/forth-core/test_capture.part.h` **verbatim** (same
`0x8B 0x1A 0xFD <len>` header, same `len = 10` `": NNNN 1 ;"` body, same
`malloc`/`writeTestProgram`/`cleanupTestProgram` frame), changing only
the count and the name pattern:

- `totalDefs = FORTH_PICKER_MAX_SCAN_STEPS + 10` definition steps, named
  `sprintf(name, "N%03d", i % 1000)` is WRONG — names must stay unique.
  Use `sprintf(name, "%c%03d", 'A' + (i / 1000), i % 1000)` so index 0 is
  `A000` and index 1004 is `B004`, still 4 bytes, still distinct.
- `currentStep` is set to the CLOSING marker step (scan runs while
  `step <= currentStep`), the same way the donor test positions it.

Assert, after `testInitVariableSoftmenu(22)`:
- `A000` IS present in `dynamicSoftmenu[22].menuContent` (the near
  definition, step 1, well inside the cut-off);
- `B004` is NOT present (step 1005, past it);
- `dynamicSoftmenu[22].numItems` is strictly less than `totalDefs`.

Do not assert an exact `numItems` — the cut-off counts STEPS including
the opening marker, and pinning the exact off-by-one here would pin the
donor fixture's marker layout rather than the deviation. The three
asserts above are the deviation.

PASS line, exact text:
`    [1] PASS: scan stops at the documented cut-off — near definition listed, far one absent`

**[2] The empty picker.** With no program and an empty dictionary, the
builder still allocates a 1-byte terminator blob (`numberOfBytes` starts
at 1), and the menu must be a well-formed empty menu rather than a NULL
with a stale count.

Drive: `cleanupTestProgram()` state (no program), then
`testInitVariableSoftmenu(22)`. Assert:
- `dynamicSoftmenu[22].numItems == 0`;
- `dynamicSoftmenu[22].menuContent != NULL`;
- `((uint8_t *)dynamicSoftmenu[22].menuContent)[0] == 0` (the terminator);
- `dynmenuGetLabel(0)` returns `""` (out of range against
  `numItems == 0`) — use `extern char *dynmenuGetLabel(int16_t menuitem);`
  and compare with `compareString(..., "", CMP_BINARY) == 0`.

Then call `showSoftmenuCurrentPart()` once with
`softmenuStack[0].softmenuId = 22` and `softmenuStack[0].firstItem = 0`,
and assert `numItems` and `menuContent` are unchanged by the draw.

PASS line:
`    [2] PASS: empty picker is a well-formed empty menu, not a NULL with a count`

## Registration

`packages/forth-core/test_dict_reloc.c`, anchored on the G1 lines this
packet's predecessor added (unique strings):

1. After `static int test_picker_key_mapping(void);`
   add `static int test_picker_scan_and_alloc(void);`
2. After the pair
   ```c
   printf("  [DEBUG] running test_picker_key_mapping...\n");
   fail |= test_picker_key_mapping();
   ```
   add
   ```c
   printf("  [DEBUG] running test_picker_scan_and_alloc...\n");
   fail |= test_picker_scan_and_alloc();
   ```

Both subcases live in ONE function `test_picker_scan_and_alloc(void)` in
`packages/forth-core/test_capture.part.h`, immediately after
`test_picker_key_mapping`'s closing brace, in the same `sc1`/`sc2`
accumulation style.

## Gate

```
./packages/forth-core/build-test.sh > /tmp/gate-g2.log 2>&1; echo $?; tail -n 12 /tmp/gate-g2.log
```

Log path is `/tmp/gate-g2.log` for this packet and overrides any
remembered log name. Green means exit 0 AND both `[N] PASS:` lines in
the log.

The subcase-1 fixture writes ~1010 program steps (~14 KB of program
memory). Report the arena high-water line from the gate log and compare
it against the pre-packet run — this packet is the first to build a
program that large, so a rise is expected and must be REPORTED, not
explained away. If the run hits an out-of-memory path instead, STOP and
report; do not shrink the fixture below the cut-off, which would delete
the pin.

The two `forth_menu.c` edits are code, so record the `make dmcp5r47
CUSTOM_PKG=packages/forth-core CUSTOM_PKG_RECONFIGURE=1` flash delta
against 1105360 B (RULE-1). The owner runs this if the implementer
cannot.

## Commit

One commit, message exactly:

```
forth-core: G2 — pin the picker's scan cut-off; guard its allocation

The 1000-step scan cut-off in forthBuildWordPicker was documented in
§9.6 and in the source, and pinned nowhere: dropping the break, or
changing the constant, passed the whole gate. It is now named
FORTH_PICKER_MAX_SCAN_STEPS and pinned by a program whose definitions
run past it — the near one lists, the far one does not.

The content calloc was stored into menuContent and written through on
the next line with no NULL test. It now leaves an empty menu instead,
which showSoftmenuCurrentPart already renders. The NULL branch is not
reachable from the suite without an allocator hook and this packet does
not add one; the empty-picker path beside it is pinned.
```

## Part B

NOT in this file. Architect-derived after this packet lands green — one
mutation per subcase (for subcase 1, raising
`FORTH_PICKER_MAX_SCAN_STEPS` must make `B004` appear and turn the
"far one absent" assert RED). Do not invent mutations here.
