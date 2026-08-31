# Sibling report: undo-history's solo flag browser reads past `menu_SYSFL`

For the undo-history owner. Found by pretty-print's audit round 8
(PP18RR8-2) and scoped in round 9 (PP18RR9-3). Paste-ready.

## The defect

A solo undo-history build declares 115 system flags and supplies 113
`menu_SYSFL` rows.

- `packages/undo-history/defines.h:1020` carries the shared
  `NUMBER_OF_SYSTEM_FLAGS 64+51` = 115. The count covers two ids
  reserved for pretty-print (`FLAG_PRETTYP`, `FLAG_PTLINE`), which have
  no catalog rows in your build.
- Your `items.c` supplies 113 `CAT_SYFL` rows (upstream's 112 plus
  `UHIST`). The generator emits exactly 113 entries.
- The un-overridden flag browser bounds its walk by the count:
  `flagBrowser.c:278` breaks when `f + fOffset > 114`. On
  `SYSTEM_FLAGS_SCREEN_2` it evaluates `menu_SYSFL[113]` and `[114]` —
  two entries past the array — and feeds both to `indexOfItems[]`.

## What the owner sees

Two softkey cells on the second system-flags screen paint from whatever
follows the array. `getSystemFlag` masks the garbage silently, so the
symptom depends on link order. The read is out of bounds in every
layout.

## The repair pretty-print ships, for reference

One hunk in `browsers/flagBrowser.c`: bound the walk by the catalog's
own row count from the softmenu table, not by the shared flag count.
See `packages/pretty-print/patches/010-browsers__flagBrowser.c.patch`
and `prettySysflRows()` in `prettyFormula.c`. The bound is correct in
every package combination. The same one-hunk override, or a shared
placement of it, closes your solo build too.

## Why the count cannot simply drop to 113

The `64+51` line is an identical-edit claim: both packages carry the
same byte-identical count so a 3-way merge unifies them. Your build
would balance at 113 alone, but the shared line must cover the highest
reserved id (114). The row count is the configuration-dependent number.
The walk bound is the correct place to read it.
