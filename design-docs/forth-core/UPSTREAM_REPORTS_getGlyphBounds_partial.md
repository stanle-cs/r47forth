# Upstream report — one finding in `src/c47/screen.c`

**Version:** base `00.109.04.00b0` (the `.refresh-manifest.json` base commit,
`3de5b4be0`); all call sites unchanged from that base. Found by code
inspection, 2026-08-11, while triaging a package-side initializer against
the upstream-minimality rule. Same defect class as
`UPSTREAM_REPORTS_softmenus_timer_coords.md` — a partial function whose
refusal path leaves out-parameters unwritten.

Paste as its own issue.

---

## `getGlyphBounds` returns without writing its out-parameters, and three of its four callers pass them straight to `lcd_fill_rect`

**File:** `src/c47/screen.c` — `getGlyphBounds` (:1313) and its callers at
:1996, :2201 and :5603-5604.

### The mechanism

`getGlyphBounds` fills `*col` and `*row` from the glyph it finds, but it has
an earlier exit that writes neither:

```c
static void getGlyphBounds(const char *ch, uint16_t *offset, const font_t *font, uint32_t *col, uint32_t *row) {
  int32_t        glyphId;
  const glyph_t *glyph;

  glyphId = findGlyph(font, charCodeFromString(ch, offset));
  if(glyphId < 0) {
    sprintf(errorMessage, commonBugScreenMessages[bugMsgValueReturnedByFindGlyph], "getGlyphBounds", glyphId);
    displayBugScreen(errorMessage);
    return;                      // *col and *row are never written
  }
  ...
}
```

It returns `void`, so a caller cannot ask whether the fill happened. Three
callers declare the destinations as plain automatics and hand them to
`lcd_fill_rect` on the next line:

- `:1994-1997` — `uint32_t ccol, crow;` → `lcd_fill_rect(xCursor, yCursor - checkHPoffset, ccol, crow, LCD_SET_VALUE)`
- `:2200-2202` — `uint32_t fcol, frow;` → `lcd_fill_rect(g_line_x, g_line_y, SCREEN_WIDTH-g_line_x-1, frow, LCD_SET_VALUE)`
- `:5602-5605` — `uint32_t fcol, frow, gcol, grow;` → `lcd_fill_rect(X_SHIFT, Y_SHIFT, max(fcol,gcol), max(frow,grow), LCD_SET_VALUE)`

On the refusal path each of those is an indeterminate `uint32_t` used as a
fill width or height.

**The fourth caller already defends against exactly this.**
`getStringBounds` (:1336) pre-zeroes its locals before the loop:

```c
uint32_t lcol, lrow;
lcol = 0;
lrow = 0;
```

Those two assignments have no other purpose — the callee writes both on
every successful call — so the file already contains the knowledge that the
callee may not write them. The other three sites were simply not given it.

### Reachability

`findGlyph` failing for `" "`, `STD_CURSOR`, `STD_f`/`STD_g` in
`standardFont` means a broken or truncated font table, so this is a
corrupted-state path rather than an everyday one. It is also the path where
a wild fill is least welcome: `displayBugScreen` has just been asked to put
a diagnostic on the LCD.

### Suggested fix

Either give the helper the same shape as `initSoftkeyCoordinates`, which is
this codebase's own convention for a coordinate helper that can refuse —

```c
static bool_t getGlyphBounds(...)   // false = nothing written
```

— and guard the three call sites, or write safe values before the early
return:

```c
*col = 0;
*row = 0;
displayBugScreen(errorMessage);
return;
```

The second is the smaller change and matches what `getStringBounds` already
assumes.

---

## Package-side note (not part of the issue text)

This package briefly carried `uint32_t fcol = 0, frow = 0;` at the
`clearRect` site as a local mitigation. It was reverted on 2026-08-11 by
owner ruling: the package's diff surface carries what Forth needs, and an
upstream defect belongs in an upstream report. The mitigation only covered
one of the three exposed sites anyway.
