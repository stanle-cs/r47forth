# PACKET M1-2 — the ASSIGN band: a global word onto a key

**Stage M packet 2** (design: STAGE_M_BROWSE_ASSIGN.md M-R2/R3;
evidence: STAGE_M_TRACES.md M-T2/M-T3/M-T4). **Prerequisite: M1-1
landed and green** (its resolution case is what makes the CM_ASSIGN
pick switch reachable for FWRD — the trace's adversarial note).
Architect-implemented (G1-G3 precedent); this packet is the authored
record.

## Production edits (four)

**E1 — the band constant** (package `defines.h`, beside the other
ASSIGN bands ~:2242):

```c
#define ASSIGN_FORTH_WORDS                     24000  /* M2 (Stage M): global
        Forth words in the pick channel.  Labels keep 12000..23999; every
        consumer tests this band FIRST (>= ASSIGN_LABELS would claim it). */
```

**E2 — the pick arm** (package `keyboard.c`, the `CM_ASSIGN` dynamic
switch beside `case MNU_PROG/MNU_PROGS` ~:328): resolve the picked name,
REQUIRE the global bit (M-R2), carry the ORDINAL — never the raw ref
(bit 15 is the negative bands):

```c
        case MNU_FORTH: {
          /* M2 (Stage M): a FWRD pick during ASSIGN captures a GLOBAL
           * word as a named assignment.  Non-global picks (interactive
           * words; a stale name) refuse with no state change — the G1
           * blank-key precedent.  The ordinal rides the int16 channel;
           * _assignItem rebuilds the ref and derefs the name. */
          uint16_t fref;
          char *fpick = (char *)getNthString(dynamicSoftmenu[menuId].menuContent, dynamicMenuItem);
          if(forthFindColonRef(fpick, &fref, NULL) && (fref & FORTH_REF_GLOBAL)
             && (int32_t)(fref & (uint16_t)~FORTH_REF_GLOBAL) <= (int32_t)(32767 - ASSIGN_FORTH_WORDS)) {
            return (int16_t)((fref & (uint16_t)~FORTH_REF_GLOBAL) + ASSIGN_FORTH_WORDS);
          }
          return ITM_NOP;
        }
```

**E3 — `assign.c` joins the package** (new override, copied from
upstream) with the band arm at BOTH consumers, each ABOVE the
`>= ASSIGN_LABELS` arm (M-T2: every consumer treats the label band as a
floor), plus `#include "forth_dict.h"` (checklist item 5):

- `_assignItem` (~:808): `menuItem->item = ITM_XEQ;` and
  `forthDictNameByRef(FORTH_REF_GLOBAL | (itemToBeAssigned -
  ASSIGN_FORTH_WORDS), menuItem->argumentName,
  sizeof(menuItem->argumentName))` — the record is `(ITM_XEQ, name)`,
  the same kind a program label produces; the band dies here. A deref
  miss (unreachable between pick and key press — `CM_ASSIGN` executes
  nothing) degrades to `ITM_NULL` + empty name, a no-op assignment.
- the pending-display consumer (~:744): same deref into its name
  buffer, so the "ASSIGN xxx" TAM line shows the word.

`argumentName` is 16 bytes (typeDefinitions.h:625); every listable
picker name fits (the picker's own 15-byte slot cap,
`test_picker_omits_long_names`).

**E4 — the pick-time pseudo-item name** (package `items.c`,
`getItemCatalogName` ~:200): the `>= ASSIGN_FORTH_WORDS` arm above the
label arm, deref via `forthDictNameByRef` into the same scratch the
label arm uses.

## Tests — `test_fwrd_assign`

Fixture: baseline program; `": MA1 43 ; GLOBAL"` and `": MA2 44 ;"`;
picker built; `kbd_usr` row + `userKeyLabel` slot for the destination
key snapshotted and restored. The press half drives `determineItem` with
`FLAG_USER` set (fills the item + `funcParam` from the user tables) and
then the dispatch exactly as `btnReleased` does at its `item == ITM_XEQ
&& FLAG_USER` arm — label miss → `forthTryColonFallback` (M-T4's cited
site); where `btnReleased` itself is not driveable from the harness the
two halves are driven back to back and the join is the arm's own
verbatim shape (the 2026-07-27 fixture-size caveat, now with real
assignment state).

1. **End to end:** `fnAssign(0)`; FWRD pick of `MA1` through the real
   resolution + pick switch → `itemToBeAssigned == ASSIGN_FORTH_WORDS +
   ordinal`, and `getItemCatalogName(itemToBeAssigned)` reads `MA1`
   (E4); `assignToKey("21")` → `kbd_usr[21].primary == ITM_XEQ` and the
   key's label slot reads `MA1`; USER-mode press → `X == 43`.
2. **Non-global refusal:** pick `MA2` in `CM_ASSIGN` → the switch
   returns `ITM_NOP`, `itemToBeAssigned` unchanged (0), no record
   written.
3. **Save/restore round-trip:** assign, save, restore (the landed
   persist-idiom drive), press → `X == 43` — zero new format surface,
   executably.
4. **FORGET-then-press:** `FORGET MA1`, press → the native
   `ERROR_LABEL_NOT_FOUND` surface, X untouched.
5. **PEM press records:** `CM_PEM` + USER press of the assigned key →
   one step recorded (`XEQ 'MA1'` by name), nothing executes — the
   2026-07-27 guard finally driven through real assignment state.
6. **Pending display:** after the pick, the assign TAM buffer contains
   `MA1` (the E3 display consumer).

## Mutations (each applied, RED, reverted)

- **E** — `_assignItem`'s band arm moved BELOW the labels arm: RED at
  [1] (the labels arm claims 24000+ and derefs `labelList` garbage; the
  key label is not `MA1`).
- **F** — the global-bit requirement dropped in E2: RED at [2].
- **G** — E2 carries the raw ref instead of the ordinal: RED at [1]
  (bit 15 lands the channel in the negative bands; the record never
  forms).

## Acceptance

Gate green; PASS lines [1]-[6] quoted; three mutations RED and
reverted; flash delta at stage close (RULE-1); arena untouched.
