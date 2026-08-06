# PACKET N1-1 — the console ring: storage, records, eviction, the roll offset

**Stage N packet 1** (design: STAGE_N_CONSOLE.md N-R2/N-R3;
evidence: STAGE_N_TRACES.md N-T5). **Prerequisite: the Stage N traces
commit (`4daedd485`), green tree.** No display code, no prims, no
screen.c edit — those are N1-2 and N1-4. This packet is the substrate
and nothing else.

## What this packet is

One new module (`forth_console.c` / `forth_console.h`) holding the view
buffer N-R2 rules: a 1024-byte BSS ring of display lines, oldest-line
eviction, an open (unterminated) last line, and the roll offset. Plus
one line of reset wiring and one new test part.

**What it is NOT:** it does not paint, format, or know what a register
is. It has no `screen.h`, no `display.h`, no `lcd_` call, and no
knowledge of FHIST. Every writer arrives in N1-3 and N1-4.

## C1 — the module

### Files (both new, both package files)

- `packages/forth-core/forth_console.h`
- `packages/forth-core/forth_console.c`

Nothing to declare anywhere: the package resolver globs package sources
(`forth_dict.c`, `forth_prims.c`, `forth_capture.c` are all
package-only files with no upstream counterpart and no build entry).
`forth_console.c` includes `"c47.h"` and `"forth_console.h"` and
nothing else.

### Constants (forth_console.h)

```c
#define FORTH_CONSOLE_RING_BYTES   1024   /* N-R2: the declared RAM price */
#define FORTH_CONSOLE_LINE_MAX      255   /* one record's payload cap; fits the
                                             length byte and is far under the
                                             ring, which is what makes eviction
                                             total (see C2, _reserve) */
#define FORTH_CONSOLE_NO_OPEN    0xFFFFu  /* "no unterminated record" */
```

### State (file-scope statics in forth_console.c — 1032 B BSS total)

```c
static char     consoleRing[FORTH_CONSOLE_RING_BYTES];
static uint16_t consoleTail;   /* index of the OLDEST record's length byte */
static uint16_t consoleUsed;   /* bytes occupied, 0..FORTH_CONSOLE_RING_BYTES */
static uint16_t consoleOpen;   /* index of the OPEN record's length byte, or
                                  FORTH_CONSOLE_NO_OPEN */
static uint16_t consoleView;   /* roll offset: 0 = newest line at the bottom */
```

`head` is **derived, never stored**: `head = (consoleTail + consoleUsed)
% FORTH_CONSOLE_RING_BYTES`. One fewer field, one fewer invariant to
break.

### The record format

A record is `[len][payload…]`: one length byte holding the payload byte
count (0..255), then that many payload bytes. Records tile the used
region from `consoleTail` forward, wrapping modulo the ring size. A
record is one **display line**; a record with `len == 0` is a blank line.

**Why length-prefixed and not `\n`-terminated.** A C47 glyph is one byte
below 0x80, or two bytes when the first is 0x80..0xFF — and **the second
byte may be anything, including 0x0A** (`STD_UP_ARROW` is `"\xa1\x91"`;
the decode loop at screen.c:1725-1728 takes the next byte unconditionally).
A newline scanner would split such a glyph and desynchronise the whole
ring. Length prefixes make the walk glyph-blind and correct, and make
"skip to the next record" O(1).

### Invariants (assert these in the hammer test, not in production code)

1. `consoleUsed <= FORTH_CONSOLE_RING_BYTES`.
2. Walking records from `consoleTail` consumes **exactly** `consoleUsed`
   bytes and lands on `head`.
3. When `consoleOpen != FORTH_CONSOLE_NO_OPEN`, the open record is the
   newest record and ends at `head`.
4. Every record's payload is `<= FORTH_CONSOLE_LINE_MAX`.

## C2 — the functions, exactly

### Internal helpers (all `static`)

```c
static uint16_t _wrap(uint32_t i) {
  return (uint16_t)(i % FORTH_CONSOLE_RING_BYTES);
}

static uint16_t _head(void) {
  return _wrap((uint32_t)consoleTail + consoleUsed);
}

static uint8_t _lenAt(uint16_t i) {
  return (uint8_t)consoleRing[i];
}

/* Byte length of the glyph at p.  A leading byte >= 0x80 takes the NEXT
 * byte with it — unless that byte is the terminator, which cannot happen
 * in well-formed text and must not be read past. */
static uint16_t _glyphBytes(const char *p) {
  return (((uint8_t)p[0] >= 0x80) && (p[1] != 0)) ? 2 : 1;
}

/* Drop the oldest record.  Refuses to drop the OPEN record — the line
 * being written must survive its own eviction pressure. */
static bool_t _evictOldest(void) {
  if (consoleUsed == 0) { return false; }
  if (consoleOpen != FORTH_CONSOLE_NO_OPEN && consoleTail == consoleOpen) { return false; }
  uint16_t n = (uint16_t)(1 + _lenAt(consoleTail));
  consoleTail = _wrap((uint32_t)consoleTail + n);
  consoleUsed = (uint16_t)(consoleUsed - n);
  return true;
}

/* Make room for n more bytes.  Cannot fail in production: the open record
 * is at most 1 + FORTH_CONSOLE_LINE_MAX = 256 bytes, so evicting everything
 * else always leaves >= 768 free, and no single call ever asks for more
 * than 2.  The false return is the honest tail of the loop, not a case the
 * callers are expected to hit. */
static bool_t _reserve(uint16_t n) {
  while ((uint16_t)(FORTH_CONSOLE_RING_BYTES - consoleUsed) < n) {
    if (!_evictOldest()) { return false; }
  }
  return true;
}

/* Start a record if none is open.  Idempotent. */
static bool_t _openRecord(void) {
  if (consoleOpen != FORTH_CONSOLE_NO_OPEN) { return true; }
  if (!_reserve(1)) { return false; }
  { uint16_t h = _head();
    consoleRing[h] = 0;
    consoleOpen = h;
    consoleUsed = (uint16_t)(consoleUsed + 1);
  }
  return true;
}

/* Append ONE glyph (g == 1 or 2) to the open record.  Over-cap glyphs are
 * DROPPED whole — never half, which would leave a split glyph in the ring. */
static bool_t _appendGlyph(const char *p, uint16_t g) {
  if (!_openRecord()) { return false; }
  if ((uint16_t)(_lenAt(consoleOpen) + g) > FORTH_CONSOLE_LINE_MAX) { return false; }
  if (!_reserve(g)) { return false; }
  for (uint16_t k = 0; k < g; k++) {
    consoleRing[_head()] = p[k];      /* _head() re-derives after each bump */
    consoleUsed = (uint16_t)(consoleUsed + 1);
  }
  consoleRing[consoleOpen] = (char)(uint8_t)(_lenAt(consoleOpen) + g);
  return true;
}
```

`_reserve` inside `_appendGlyph` may evict, which moves `consoleTail` but
never invalidates `consoleOpen` — eviction only ever advances the tail
past records the open one is not.

### Public API (forth_console.h)

```c
void     forthConsoleClear(void);
void     forthConsoleAppend(const char *s);       /* text; embedded '\n' breaks lines */
void     forthConsoleNewline(void);               /* close the line (CR) */
void     forthConsoleAppendLine(const char *s);   /* Append then Newline */
uint16_t forthConsoleLineCount(void);
bool_t   forthConsoleLineAt(uint16_t n, char *out, uint16_t outSize);  /* n = 0 is NEWEST */
uint16_t forthConsoleViewOffset(void);
void     forthConsoleSetViewOffset(uint16_t n);
#if defined(FORTH_DEBUG_SELFTEST)
  uint16_t forthConsoleTestUsed(void);
  bool_t   forthConsoleTestHasOpen(void);
#endif
```

```c
void forthConsoleClear(void) {
  consoleTail = 0;
  consoleUsed = 0;
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;
}

void forthConsoleAppend(const char *s) {
  if (s == NULL) { return; }
  { const char *p = s;
    while (*p) {
      uint16_t g = _glyphBytes(p);
      if (g == 1 && *p == '\n') {
        forthConsoleNewline();
      }
      else {
        (void)_appendGlyph(p, g);   /* over-cap glyphs drop, by rule */
      }
      p += g;
    }
  }
  consoleView = 0;                  /* N-R3: any output snaps the view to newest */
}

void forthConsoleNewline(void) {
  if (consoleOpen == FORTH_CONSOLE_NO_OPEN) {
    (void)_openRecord();            /* CR on a fresh line IS a blank line */
  }
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;
}

void forthConsoleAppendLine(const char *s) {
  forthConsoleAppend(s);
  forthConsoleNewline();
}

uint16_t forthConsoleLineCount(void) {
  uint16_t n = 0, i = consoleTail, remaining = consoleUsed;
  while (remaining > 0) {
    uint16_t sz = (uint16_t)(1 + _lenAt(i));
    i = _wrap((uint32_t)i + sz);
    remaining = (uint16_t)(remaining - sz);
    n++;
  }
  return n;
}

/* n = 0 is the NEWEST line; n = count-1 the oldest.  Copies GLYPH-WISE and
 * stops at the last glyph that fits, so a short out buffer truncates on a
 * glyph boundary and never leaves a half glyph for the painter. */
bool_t forthConsoleLineAt(uint16_t n, char *out, uint16_t outSize) {
  uint16_t count, target, i, k;
  uint8_t  len;
  if (out == NULL || outSize == 0) { return false; }
  out[0] = 0;
  count = forthConsoleLineCount();
  if (n >= count) { return false; }
  target = (uint16_t)(count - 1 - n);
  i = consoleTail;
  for (k = 0; k < target; k++) { i = _wrap((uint32_t)i + 1 + _lenAt(i)); }
  len = _lenAt(i);
  { uint16_t src = 0, dst = 0;
    while (src < len) {
      char first = consoleRing[_wrap((uint32_t)i + 1 + src)];
      uint16_t g = (((uint8_t)first >= 0x80) && (src + 1 < len)) ? 2 : 1;
      if (dst + g > (uint16_t)(outSize - 1)) { break; }
      for (k = 0; k < g; k++) {
        out[dst++] = consoleRing[_wrap((uint32_t)i + 1 + src + k)];
      }
      src = (uint16_t)(src + g);
    }
    out[dst] = 0;
  }
  return true;
}

uint16_t forthConsoleViewOffset(void) { return consoleView; }

void forthConsoleSetViewOffset(uint16_t n) {
  uint16_t count = forthConsoleLineCount();
  consoleView = (count == 0) ? 0 : ((n >= count) ? (uint16_t)(count - 1) : n);
}
```

Note the `_glyphBytes` reimplementation inside `forthConsoleLineAt`: the
ring is circular, so the two bytes of a glyph are not adjacent in memory
and the pointer form cannot be reused. The `src + 1 < len` conjunct is
the same defence — a trailing high byte with no partner is copied as one
byte rather than read out of the record.

## C3 — the reset wiring (one edit, one file)

`forth_capture.c`, inside `forthCapPowerReset()` (currently :73-85), add
as the last statement:

```c
  forthConsoleClear();          /* N-T5: the view ring is not capture state, but
                                   it shares the capture's lifecycle seam — this
                                   function's only two production callers are
                                   forthDictInit (forth_dict.c:57) and
                                   forthDictClear (:71), which is exactly the
                                   power-reset boundary N-R2 clears the view at.
                                   Deliberately NOT in forthCapClose/
                                   forthCapAbandonSuspended: N-R2 rules the
                                   dialogue SURVIVES capture close and reopen. */
```

plus `#include "forth_console.h"` at the top of `forth_capture.c`.

**One site, not two.** Calling from the two `forth_dict.c` seams would
work identically today, but `forthCapPowerReset` is the named seam and
already carries the "transient state never survives a dictionary
lifecycle reset" rule for four other fields.

## Tests — `test_console.part.h`

New file `packages/forth-core/test_console.part.h`, included in
`test_dict_reloc.c` beside the landed parts (after
`#include "test_engine.part.h"`), with the usual `static int
test_xxx(void);` forward declarations above the runner and a registered
block inside `forthDictSelfTest` following the landed idiom:

```c
  /* N1-1: the console view ring */
  printf("\nFORTH N1-1 TESTS (console ring)\n");
  forthDictInit();
  printf("  [DEBUG] running test_console_ring_basic...\n");
  fail |= test_console_ring_basic();
  ...
  forthDictClear();
  forthGDictClear();
```

Every test calls `forthConsoleClear()` first (except case 8, which is
about the seam) and prints PASS/FAIL per assertion in the landed style.

1. **`test_console_ring_basic`** — `AppendLine("one")`, `AppendLine("two")`,
   `AppendLine("three")`. Assert `LineCount() == 3`; `LineAt(0)` is
   `"three"` (**newest is index 0** — get this backwards and N1-2 paints
   the transcript upside down), `LineAt(1)` is `"two"`, `LineAt(2)` is
   `"one"`, and `LineAt(3)` returns false with `out[0] == 0`.

2. **`test_console_ring_partial`** — `Append("ab")`: assert
   `LineCount() == 1` and `LineAt(0) == "ab"` (an unterminated line is
   readable — the console must show output before the word finishes).
   `Append("cd")`: still one line, now `"abcd"`. `Newline()`: still one
   line, `TestHasOpen()` false. `Append("e")`: two lines, `LineAt(0) ==
   "e"`, `LineAt(1) == "abcd"`.

3. **`test_console_ring_evict`** — append 200 lines of the 10-byte
   `"0123456789"`. Each record is 11 bytes and `93 * 11 = 1023 <= 1024`,
   so assert **`LineCount() == 93`** exactly, `TestUsed() == 1023`,
   `LineAt(0) == "0123456789"`, and `LineAt(93)` false. The exact count is
   the point: an off-by-one in `_evictOldest` shows up here and nowhere
   else.

4. **`test_console_ring_linecap`** — `Append` a 400-byte ASCII string with
   no newline. Assert the stored line is exactly `FORTH_CONSOLE_LINE_MAX`
   (255) bytes and equals the first 255 bytes of the input. `Append("Z")`
   again: still 255 (over-cap appends drop, they do not wrap into a new
   line). `Newline(); Append("Z")`: a second line appears and holds `"Z"`.

5. **`test_console_ring_glyph`** — the test a newline scanner fails.
   Append the two-byte glyph `"\x80\x0a"` (a legal C47 glyph whose low
   byte is ASCII LF) surrounded by text: `AppendLine("a\x80\x0a" "b")`.
   Assert `LineCount() == 1` and the line's byte length is 4 with the
   glyph intact — **the 0x0A must not have terminated the line.** Then
   append 130 copies of `STD_UP_ARROW` (`"\xa1\x91"`, 260 bytes) with no
   newline and assert the stored length is **254, not 255**: the cap
   drops the whole glyph that would overflow, never half of it.

6. **`test_console_ring_clear`** — populate, `SetViewOffset(2)`,
   `forthConsoleClear()`. Assert `LineCount() == 0`, `TestUsed() == 0`,
   `ViewOffset() == 0`, `TestHasOpen()` false, and `LineAt(0)` false.

7. **`test_console_ring_view`** — five lines. `SetViewOffset(2)` →
   `ViewOffset() == 2`. `SetViewOffset(99)` → clamps to 4 (`count-1`).
   Then `AppendLine("new")` → **`ViewOffset() == 0`**: any output snaps
   the view to newest (N-R3). Same assertion after a bare `Append` and
   after a bare `Newline`. On an empty ring `SetViewOffset(3)` → 0.

8. **`test_console_ring_reset_seam`** — populate three lines, call
   `forthDictInit()`, assert the ring is empty; populate again, call
   `forthDictClear()`, assert empty. Then populate, call
   `forthCapClose()` and assert the ring is **untouched** — the
   survives-close half of N-R2, and the one a "clear it everywhere"
   reflex would break.

9. **`test_console_ring_hammer`** — 5000 iterations of a deterministic
   mix (`i % 7` selects `Append` of a 1/2/40-byte string, `Newline`,
   `AppendLine`, an over-cap `Append`, a `SetViewOffset`). After **each**
   iteration assert the four C1 invariants: `used <= RING`; walking
   records from `consoleTail` consumes exactly `used` bytes; the walked
   record count equals `LineCount()`; every record payload `<= 255`. No
   `Clear` inside the loop — the point is sustained wrap-around.

## Build, measurement, acceptance

- Gate: `./packages/forth-core/build-test.sh` (refreshes patches/+files
  first — the build never reads the working area).
- **Two new source files, so the flash number needs a full reconfigure:**
  record `make dmcp5r47 CUSTOM_PKG=packages/forth-core
  CUSTOM_PKG_RECONFIGURE=1` in the packet commit (TESTING.md §5 — without
  the flag the number is the previous tree's).
- **BSS delta: +1032 B**, stated and expected (1024 ring + 4 × uint16_t).
  Arena high-water reported per §5.4 and expected **unchanged** — this
  packet allocates nothing and touches no dictionary.
- Acceptance checklist:
  1. `grep -nE 'lcd_|showString|refreshScreen|screen\.h|display\.h'
     packages/forth-core/forth_console.[ch]` returns **nothing** — the
     "no display calls outside the view" property N-T5 asserts, checked
     rather than hoped.
  2. All nine tests green, plus the whole landed suite green.
  3. `forth_console.c` includes only `c47.h` and `forth_console.h`.
  4. No edit to any file other than the two new ones, `forth_capture.c`
     (two lines), `test_dict_reloc.c` (the include + the registered
     block), and the new test part.
