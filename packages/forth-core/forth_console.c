/*
 * forth_console.c -- the console VIEW ring (Stage N, packet N1-1)
 * Per STAGE_N_CONSOLE.md N-R2/N-R3 and STAGE_N_TRACES.md N-T5.
 *
 * Storage, records, eviction and the roll offset.  No display code:
 * this file must never grow a screen.h/display.h include or an lcd_
 * call — the "no display calls outside the view" property N-T5 asserts
 * is checked by grep at the packet's acceptance, not hoped for.
 */

#include "forth_console.h"

static char     consoleRing[FORTH_CONSOLE_RING_BYTES];
static uint16_t consoleTail;   /* index of the OLDEST record's length byte */
static uint16_t consoleUsed;   /* bytes occupied, 0..FORTH_CONSOLE_RING_BYTES */
static uint16_t consoleOpen = FORTH_CONSOLE_NO_OPEN;
                               /* index of the OPEN record's length byte */
static uint16_t consoleView;   /* roll offset: 0 = newest line at the bottom */

/* head is DERIVED, never stored: one fewer field, one fewer invariant to
 * break. */
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

/* Drop the oldest record.  REFUSES to drop the OPEN record: the line
 * being written must survive its own eviction pressure. */
static bool_t _evictOldest(void) {
  uint16_t n;
  if (consoleUsed == 0) { return false; }
  if (consoleOpen != FORTH_CONSOLE_NO_OPEN && consoleTail == consoleOpen) { return false; }
  n = (uint16_t)(1 + _lenAt(consoleTail));
  consoleTail = _wrap((uint32_t)consoleTail + n);
  consoleUsed = (uint16_t)(consoleUsed - n);
  return true;
}

/* Make room for n more bytes.  Cannot fail in production — see the
 * FORTH_CONSOLE_LINE_MAX note in the header.  The false return is the
 * honest tail of the loop, not a case the callers are expected to hit. */
static bool_t _reserve(uint16_t n) {
  while ((uint16_t)(FORTH_CONSOLE_RING_BYTES - consoleUsed) < n) {
    if (!_evictOldest()) { return false; }
  }
  return true;
}

/* Start a record if none is open.  Idempotent. */
static bool_t _openRecord(void) {
  uint16_t h;
  if (consoleOpen != FORTH_CONSOLE_NO_OPEN) { return true; }
  if (!_reserve(1)) { return false; }
  h = _head();
  consoleRing[h] = 0;
  consoleOpen = h;
  consoleUsed = (uint16_t)(consoleUsed + 1);
  return true;
}

/* Append ONE glyph (g == 1 or 2) to the open record.  Over-cap glyphs are
 * dropped WHOLE — never half, which would leave a split glyph in the ring
 * for the painter to decode across a record boundary. */
static bool_t _appendGlyph(const char *p, uint16_t g) {
  uint16_t k;
  if (!_openRecord()) { return false; }
  if ((uint16_t)(_lenAt(consoleOpen) + g) > FORTH_CONSOLE_LINE_MAX) { return false; }
  /* _reserve may evict, which advances the tail but never invalidates
   * consoleOpen — eviction only ever passes records the open one is not. */
  if (!_reserve(g)) { return false; }
  for (k = 0; k < g; k++) {
    consoleRing[_head()] = p[k];     /* _head() re-derives after each bump */
    consoleUsed = (uint16_t)(consoleUsed + 1);
  }
  consoleRing[consoleOpen] = (char)(uint8_t)(_lenAt(consoleOpen) + g);
  return true;
}

void forthConsoleClear(void) {
  consoleTail = 0;
  consoleUsed = 0;
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;
}

void forthConsoleNewline(void) {
  if (consoleOpen == FORTH_CONSOLE_NO_OPEN) {
    (void)_openRecord();            /* CR on a fresh line IS a blank line */
  }
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;                  /* N-R3: any output snaps the view to newest */
}

void forthConsoleAppend(const char *s) {
  const char *p;
  if (s == NULL) { return; }
  p = s;
  while (*p) {
    uint16_t g = _glyphBytes(p);
    if (g == 1 && *p == '\n') {
      forthConsoleNewline();
    }
    else {
      (void)_appendGlyph(p, g);     /* over-cap glyphs drop, by rule */
    }
    p += g;
  }
  consoleView = 0;                  /* N-R3 */
}

void forthConsoleAppendLine(const char *s) {
  forthConsoleAppend(s);
  forthConsoleNewline();
}

/* The record walk, bounded twice over.
 *
 * Neither guard can fire while the C1 invariants hold — a record is at least
 * one byte, so a consistent ring holds at most FORTH_CONSOLE_RING_BYTES of
 * them, and the sizes sum to exactly `used`.  They exist because the
 * unguarded loop's failure mode on a desynchronised ring is not a wrong
 * answer but a HANG: `remaining - sz` underflows through 0 into 65535 and
 * the walk never terminates.  This runs on a calculator with no way to kill
 * a spinning task, so a corrupted ring must degrade to a miscount instead.
 * Found while mutation-testing this module (N1-1): every mutation that
 * desynchronises the records hung the suite rather than reddening it. */
uint16_t forthConsoleLineCount(void) {
  uint16_t n = 0, i = consoleTail, remaining = consoleUsed;
  while (remaining > 0 && n < FORTH_CONSOLE_RING_BYTES) {
    uint16_t sz = (uint16_t)(1 + _lenAt(i));
    if (sz > remaining) { break; }        /* inconsistent: stop, never underflow */
    i = _wrap((uint32_t)i + sz);
    remaining = (uint16_t)(remaining - sz);
    n++;
  }
  return n;
}

/* n = 0 is the NEWEST line; n = count-1 the oldest.  Copies GLYPH-WISE and
 * stops at the last glyph that fits, so a short out buffer truncates on a
 * glyph boundary and never leaves a half glyph for the painter.
 *
 * The pointer form of _glyphBytes cannot be reused here: the ring is
 * circular, so the two bytes of a glyph are not adjacent in memory. */
bool_t forthConsoleLineAt(uint16_t n, char *out, uint16_t outSize) {
  uint16_t count, target, i, k, src = 0, dst = 0;
  uint8_t  len;

  if (out == NULL || outSize == 0) { return false; }
  out[0] = 0;
  count = forthConsoleLineCount();
  if (n >= count) { return false; }

  target = (uint16_t)(count - 1 - n);
  i = consoleTail;
  for (k = 0; k < target; k++) { i = _wrap((uint32_t)i + 1 + _lenAt(i)); }
  len = _lenAt(i);

  while (src < len) {
    char first = consoleRing[_wrap((uint32_t)i + 1 + src)];
    /* src + 1 < len is the same defence as the pointer form's p[1] != 0:
     * a trailing high byte with no partner is copied as one byte rather
     * than read out of the record. */
    uint16_t g = (((uint8_t)first >= 0x80) && (src + 1 < len)) ? 2 : 1;
    if ((uint16_t)(dst + g) > (uint16_t)(outSize - 1)) { break; }
    for (k = 0; k < g; k++) {
      out[dst++] = consoleRing[_wrap((uint32_t)i + 1 + src + k)];
    }
    src = (uint16_t)(src + g);
  }
  out[dst] = 0;
  return true;
}

uint16_t forthConsoleViewOffset(void) { return consoleView; }

void forthConsoleSetViewOffset(uint16_t n) {
  uint16_t count = forthConsoleLineCount();
  consoleView = (count == 0) ? 0 : ((n >= count) ? (uint16_t)(count - 1) : n);
}

/* The roll (N1-2).  delta > 0 walks BACK through the dialogue (older lines),
 * delta < 0 forward.  Clamps at both ends rather than wrapping: a terminal
 * scrollback stops at the top, it does not cycle. */
void forthConsoleRoll(int16_t delta) {
  uint16_t count = forthConsoleLineCount();
  int32_t  v;
  if (count == 0) { consoleView = 0; return; }
  v = (int32_t)consoleView + delta;
  if (v < 0) { v = 0; }
  if (v > (int32_t)(count - 1)) { v = (int32_t)(count - 1); }
  consoleView = (uint16_t)v;
}

#if defined(FORTH_DEBUG_SELFTEST)
  uint16_t forthConsoleTestUsed(void)          { return consoleUsed; }
  uint16_t forthConsoleTestTail(void)          { return consoleTail; }
  bool_t   forthConsoleTestHasOpen(void)       { return consoleOpen != FORTH_CONSOLE_NO_OPEN; }
  uint8_t  forthConsoleTestByteAt(uint16_t i)  { return (uint8_t)consoleRing[_wrap(i)]; }
#endif
