# Stage M — architect pre-work traces M-T1..M-T5

**Status: complete 2026-08-05. Evidence sheet for
STAGE_M_BROWSE_ASSIGN.md's packet decomposition; not normative. Every
file:line was read this pass on `forth-core/stage-m` at the Stage L
close. Line numbers cite the PACKAGE copies where an override exists
(keyboard.c, screen.c, softmenus.c, items.c are package files).**

## M-T1 — the pick dispatch, per mode

**Resolution.** A `MNU_FORTH` softkey resolves at
packages/forth-core/keyboard.c:132:

```c
case MNU_FORTH: {
  dynamicMenuItem = firstItem + itemShift + fn;
  item = ITM_NOP; // always ITM_NOP: press handled in executeFunction (§9.6 P-H7)
  break;
}
```

`dynamicMenuItem` is latched on every press; the ITEM is always
`ITM_NOP`. Downstream, exactly one consumer acts on that shape:
`forthPickerGuard` (forth_menu.c:65) in `executeFunction` — and it is
**capture-gated**: `(CM_PEM capture || forthCapIsInteractive()) && item
== ITM_NOP && 0 <= dynamicMenuItem < numItems`. Outside a capture the
guard is false and `ITM_NOP` dispatches to nothing: **a FWRD press in
`CM_NORMAL` today is a silent no-op** (it only latches
`dynamicMenuItem`). The M1 seat is therefore empty, and it is the
resolution case itself — not a new `executeFunction` arm.

**The parity target.** A `MNU_PROGS` softkey resolves at keyboard.c:239:

```c
case MNU_PROGS: {
  dynamicMenuItem = firstItem + itemShift + fn;
  item = (dynamicMenuItem >= numItems ? ITM_NOP
          : (tam.mode == TM_DELITM) ? MNU_DYNAMIC : ITM_XEQ);
  break;
}
```

`ITM_XEQ`-with-`dynamicMenuItem` then rides `runFunction`'s dynamic-XEQ
dispatch (items.c:693-726): `dynmenuGetLabel` → `forthResolveXEQ` →
LABEL / COLON / ITEM arms via `forthUserItemDispatch` — each already
carrying the `calcMode == CM_PEM` record-vs-execute split (2026-07-27
guards; regression-pinned by `test_pem_xeq_dynmenu_no_live_exec`) and
the not-found error surface (items.c:718,
`ERROR_LABEL_NOT_FOUND`). **"Press = run" for programs is literally
`item = ITM_XEQ`; every downstream behaviour M1 wants — resolution,
execution, PEM recording, errors — is this one landed dispatch.**

**The M1 edit (one case, four dispositions), stated for the packet:**

```c
case MNU_FORTH: {
  dynamicMenuItem = firstItem + itemShift + fn;
  if((calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && tam.function == ITM_FORTH)
     || forthCapIsInteractive()) {
    item = ITM_NOP;               /* captures: the picker-insert guard consumes it */
  }
  else if((calcMode == CM_NORMAL || calcMode == CM_ASSIGN) && tam.mode == 0) {
    item = (dynamicMenuItem >= dynamicSoftmenu[menuId].numItems)
             ? ITM_NOP : ITM_XEQ; /* M1 execute / M2 assign-pick (PROGS shape) */
  }
  else {
    item = ITM_NOP;               /* native AIM, PEM-no-capture, TAM, browsers:
                                     untouched surfaces stay inert */
  }
  break;
}
```

This is **one of two coupled edits** (checklist item 6): the capture
branch must return `ITM_NOP` exactly when `forthPickerGuard` will fire,
or the insert path dies. Both directions get a class test (capture
press still inserts; normal press executes). `TM_DELITM` stays inert via
the `tam.mode == 0` conjunct — dictionary words are not deletable
catalog items. XEQ-TAM picks keep today's latch-then-dispatch shape
(`tam.mode != 0` → NOP), unchanged.

**Native AIM stays untouched by construction:** `calcMode == CM_AIM`
falls to the final `else`. F6-5's insert surface (capture) and the
inert native-alpha surface are both preserved.

## M-T2 — the ASSIGN grammar

**Flow at current lines.** `fnAssign(0)` (src/c47/assign.c:555-560):
`previousCalcMode = calcMode; calcMode = CM_ASSIGN; itemToBeAssigned =
0`. Source pick → destination key → `assignToKey(data)` (:944):
`keyStateCode = (previousCalcMode == CM_AIM ? 3 : 0) + shift` — the AIM
assignment layers are native; `_assignItem(&tmp)` (:801) derefs the band
to `(item, argumentName)`; the key tables take `tmp.item` and
`setUserKeyArgument(keyCode * 6 + keyStateCode, tmp.argumentName)`
(:1043) stores the name.

**The pick switch** (packages/forth-core/keyboard.c:326-333) fires on
the ALREADY-RESOLVED item: `if(calcMode == CM_ASSIGN && item != ITM_NOP
&& item != ITM_NULL) switch(-softmenu[menuId].menuItem)` —
`case MNU_PROG/MNU_PROGS: return findNamedLabel(<picked name>,
GLOBAL_LABELS) - FIRST_LABEL + ASSIGN_LABELS;`. **M1's `ITM_XEQ` in
`CM_ASSIGN` is what makes M2's arm reachable** — with today's
always-NOP resolution the switch never sees a FWRD pick.

**The band channel.** `itemToBeAssigned` is `int16_t` (src/c47/c47.c:260).
Bands (src/c47/defines.h:2241-2245): NAMED 10000, RESERVED 10000+Δ,
LABELS 12000, USER_MENU −10000, CLEAR −32768. **Every consumer treats
`>= ASSIGN_LABELS` as a label**, so the Forth band must sit above and be
tested FIRST at every consumer. Ruling: `ASSIGN_FORTH_WORDS = 24000` —
labels keep 12000..23999 (12,000 slots), words get 24000..32767 (8,768
ordinals; the gdict count cap is far larger in theory, so the pick arm
range-checks and refuses an ordinal past the band — unreachable in any
real dictionary, stated so the guard reads as deliberate).

**The complete consumer list (grep `ASSIGN_LABELS`, all files):**

1. src/c47/assign.c:744 — the pending-assignment display name
   (`updateAssignTamBuffer` path).
2. src/c47/assign.c:808 — `_assignItem`, the record writer.
3. src/c47/items.c:200 — `getItemCatalogName`, the pseudo-item name.

Each gains an `>= ASSIGN_FORTH_WORDS` arm ABOVE the labels arm. (The
other grep hits — keyboard.c:333/1435/3363, assign.c:1233 — are
producers, not consumers.)

**Pick-arm resolution and deref (no index-order coupling).** The pick
arm resolves the picked NAME via `forthFindColonRef(name, &ref, NULL)`
(forth_dict.h:154) and REQUIRES `ref & FORTH_REF_GLOBAL` (M-R2); it
carries `ordinal = ref & ~FORTH_REF_GLOBAL` in the band. Consumers
rebuild `ref = FORTH_REF_GLOBAL | ordinal` and deref with
`forthDictNameByRef(ref, buf, size)` (forth_dict.h:293) — the same
accessor the FCALL insert arm uses (manage.c:2580). The raw ref is
NEVER carried in the channel: bit 15 would land in the negative bands.
Stability window: `CM_ASSIGN` executes nothing, so gdict cannot mutate
between pick and deref.

**Non-global refusal shape (M-R2):** the pick arm returns `ITM_NOP` for
a text-scan or interactive-section pick — no state change, the G1
blank-key-refusal precedent.

**The record** is `(ITM_XEQ, name)` — byte-identical in kind to a
program assignment (assign.c:808-812). The band dies inside
`_assignItem`; nothing stored distinguishes the two kinds, which is the
zero-new-surface claim M-T3 proves.

## M-T3 — persistence and display: zero new surface

- Save: `kbd_usr` fields serialize at src/c47/saveRestoreCalcState.c:773-780;
  the `userKeyLabel` block (argumentNames) restores at :1891-1899 with
  its own allocation rebuild. A Forth-word assignment writes only these
  existing structures — not one new byte of format. The M1-2 battery
  proves it executably with a full save/restore round-trip of an
  assigned key.
- Display: the pending assignment renders through assign.c:744's band
  arm (new arm added); an ASSIGNED key's name renders wherever native
  named assignments render (`getUserKeyLabelString` readers, e.g. the
  assignment browser) — the stored name is ordinary; nothing to add.
  `getItemCatalogName` (items.c:200) needs the band arm only for
  PICK-TIME pseudo-items (the interval between pick and key press).

## M-T4 — press-time resolution order (restated, unchanged)

The USER-key dispatch after a label miss falls to
`forthTryColonFallback(item, name)` (forth_bridge.c:40):
`forthFindColon` → `forthDispatchColon` → `forthUserItemDispatch(item,
name, ITM_FCALL, widx)` — the PEM record-vs-execute split inside. Call
sites: packages/forth-core/screen.c:831-835 (native label arm, then the
fallback) and packages/forth-core/keyboard.c:2373. So a pressed key and
a typed `XEQ 'NAME'` resolve in the same order — native label first,
then colon word — and a program/word name collision behaves identically
either way (§4.2's order). A name resolving to neither is the native
not-found surface. **No Stage M change**; the trace exists so the
fold-in can cite it, and the battery pins FORGET-then-press to the
standard error.

## M-T5 — catalog drains and gates

- `_forthCatalogMenuOnTop` (manage.c) matches CATALOG, FCNS, CONST,
  CHARS, PROGS, VARS, MENUS — not FWRD. `_forthCatalogBuriedOnStack`
  scans the stack for `-MNU_CATALOG` only.
- **FWRD-over-CATALOG at a capture open drains correctly BY
  CONSTRUCTION:** buried(CATALOG) is true → the loop pops FWRD, then
  CATALOG matches on top and pops. Disposition: **KEEP both predicates
  unchanged**; the M1-1 class test adds the row (open interactive with
  FWRD-over-CATALOG up → both drained, capture opens clean).
- FWRD reached directly (the alpha-menu path, no CATALOG beneath) at a
  capture open: neither predicate fires, FWRD stays up — which is the
  landed picker-over-capture state and correct. Disposition: KEEP.
- The `catalog` browse variable: `-MNU_FORTH` as a CATALOG row is a
  plain submenu push, the same shape as the `-MNU_PROGS` row beside it;
  neither sets `CATALOG_*` state. Symmetry claim, pinned by the M1-1
  tests rather than a code change.
- CM-gate audit deltas: the stage adds exactly two reachable new
  (mode, FWRD-press) rows — `CM_NORMAL` → execute, `CM_ASSIGN` →
  band — and changes no existing row. Both are driven by the batteries;
  the L1-F3 audit table gains the two rows at fold-in.

## Adversarial check note (the T7.5 lesson applied)

The one claim in this sheet that would have shipped wrong if taken from
the write-set alone: "the ASSIGN switch handles FWRD picks" — it
CANNOT, today, because the switch is gated `item != ITM_NOP` and the
FWRD resolution is always-NOP; the reachability trace is what surfaced
M1-before-M2 as a hard ordering (M2's arm is dead code until M1's
resolution ships). The packet order below follows from it.
