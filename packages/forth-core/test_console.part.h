/* packages/forth-core/test_console.part.h — Stage N packet N1-1 (2026-08-06).
 *
 * This is NOT a standalone header: it is a source PART, #included exactly
 * once at the end of test_dict_reloc.c so the suite stays one compilation
 * unit (shared statics, unchanged build/audit/citations). Edit rules are
 * the same as for test_dict_reloc.c; anchor edits on subcase printf text.
 * Functions here are forward-declared in the main file before the runner.
 *
 * Subject: the console VIEW ring (forth_console.c).  Storage, records,
 * eviction, the line cap, glyph safety, the roll offset and the reset
 * seam.  Nothing here paints or formats — N1-2/N1-3 own that.
 */

/* Shared helper: byte length of the line the ring reports at index n. */
static int _consoleLineIs(uint16_t n, const char *expect)
{
  char buf[512];
  if (!forthConsoleLineAt(n, buf, sizeof(buf))) { return 0; }
  return stringByteLength(buf) == stringByteLength((char *)expect)
         && memcmp(buf, expect, (size_t)stringByteLength((char *)expect)) == 0;
}

static int _consoleLineLen(uint16_t n)
{
  char buf[512];
  if (!forthConsoleLineAt(n, buf, sizeof(buf))) { return -1; }
  return stringByteLength(buf);
}

/* ---- 1: line order.  NEWEST IS INDEX 0 — reverse this and N1-2 paints the
 * transcript upside down, which is why it is asserted before anything else. */
static int test_console_ring_basic(void)
{
  forthConsoleClear();
  forthConsoleAppendLine("one");
  forthConsoleAppendLine("two");
  forthConsoleAppendLine("three");

  if (forthConsoleLineCount() != 3) {
    printf("    FAIL: expected 3 lines, got %u\n", forthConsoleLineCount());
    return 1;
  }
  if (!_consoleLineIs(0, "three") || !_consoleLineIs(1, "two") || !_consoleLineIs(2, "one")) {
    printf("    FAIL: line order wrong (index 0 must be the NEWEST line)\n");
    return 1;
  }
  { char buf[16];
    buf[0] = 'x';
    if (forthConsoleLineAt(3, buf, sizeof(buf))) {
      printf("    FAIL: LineAt(3) must be false on a 3-line ring\n");
      return 1;
    }
    if (buf[0] != 0) {
      printf("    FAIL: LineAt past the end must still NUL-terminate out\n");
      return 1;
    }
  }
  printf("    PASS: 3 lines, newest at index 0, past-the-end refused\n");
  return 0;
}

/* ---- 2: the open (unterminated) record is READABLE.  A word that emits
 * without a trailing CR must still show its output. ---- */
static int test_console_ring_partial(void)
{
  forthConsoleClear();
  forthConsoleAppend("ab");
  if (forthConsoleLineCount() != 1 || !_consoleLineIs(0, "ab")) {
    printf("    FAIL: unterminated line not readable\n");
    return 1;
  }
  if (!forthConsoleTestHasOpen()) {
    printf("    FAIL: expected an open record after a bare Append\n");
    return 1;
  }
  forthConsoleAppend("cd");
  if (forthConsoleLineCount() != 1 || !_consoleLineIs(0, "abcd")) {
    printf("    FAIL: second Append must EXTEND the open line, not start a new one\n");
    return 1;
  }
  forthConsoleNewline();
  if (forthConsoleLineCount() != 1 || forthConsoleTestHasOpen()) {
    printf("    FAIL: Newline must close the line without adding one\n");
    return 1;
  }
  forthConsoleAppend("e");
  if (forthConsoleLineCount() != 2 || !_consoleLineIs(0, "e") || !_consoleLineIs(1, "abcd")) {
    printf("    FAIL: Append after Newline must start a fresh line\n");
    return 1;
  }
  printf("    PASS: partial line readable, extends, closes, then a fresh line\n");
  return 0;
}

/* ---- 3: eviction arithmetic, pinned exactly.  200 appends of a 10-byte
 * payload = 11-byte records; 93 * 11 = 1023 <= 1024.  An off-by-one in
 * _evictOldest shows up here and nowhere else. ---- */
static int test_console_ring_evict(void)
{
  int i;
  forthConsoleClear();
  for (i = 0; i < 200; i++) {
    forthConsoleAppendLine("0123456789");
  }
  if (forthConsoleLineCount() != 93) {
    printf("    FAIL: expected exactly 93 records after eviction, got %u\n",
           forthConsoleLineCount());
    return 1;
  }
  if (forthConsoleTestUsed() != 1023) {
    printf("    FAIL: expected used == 1023 bytes, got %u\n", forthConsoleTestUsed());
    return 1;
  }
  if (!_consoleLineIs(0, "0123456789")) {
    printf("    FAIL: newest line corrupted by eviction\n");
    return 1;
  }
  { char buf[64];
    if (forthConsoleLineAt(93, buf, sizeof(buf))) {
      printf("    FAIL: LineAt(93) must be false — only 93 records survive\n");
      return 1;
    }
  }
  printf("    PASS: eviction leaves exactly 93 records / 1023 bytes, newest intact\n");
  return 0;
}

/* ---- 4: the per-line cap.  Over-cap appends DROP; they do not wrap into a
 * new line (a wrapping cap would silently invent transcript lines). ---- */
static int test_console_ring_linecap(void)
{
  char big[401];
  int i;
  for (i = 0; i < 400; i++) { big[i] = (char)('A' + (i % 26)); }
  big[400] = 0;

  forthConsoleClear();
  forthConsoleAppend(big);
  if (forthConsoleLineCount() != 1) {
    printf("    FAIL: an over-cap append must stay ONE line, got %u\n",
           forthConsoleLineCount());
    return 1;
  }
  if (_consoleLineLen(0) != FORTH_CONSOLE_LINE_MAX) {
    printf("    FAIL: expected a %u-byte line, got %d\n",
           (unsigned)FORTH_CONSOLE_LINE_MAX, _consoleLineLen(0));
    return 1;
  }
  { char buf[512];
    forthConsoleLineAt(0, buf, sizeof(buf));
    if (memcmp(buf, big, FORTH_CONSOLE_LINE_MAX) != 0) {
      printf("    FAIL: the kept prefix must be the FIRST 255 bytes\n");
      return 1;
    }
  }
  forthConsoleAppend("Z");
  if (_consoleLineLen(0) != FORTH_CONSOLE_LINE_MAX || forthConsoleLineCount() != 1) {
    printf("    FAIL: appending to a capped line must drop, not wrap\n");
    return 1;
  }
  forthConsoleNewline();
  forthConsoleAppend("Z");
  if (forthConsoleLineCount() != 2 || !_consoleLineIs(0, "Z")) {
    printf("    FAIL: after Newline the cap must reset for the next line\n");
    return 1;
  }
  printf("    PASS: line cap holds at 255, drops rather than wraps, resets on CR\n");
  return 0;
}

/* ---- 5: glyph safety — the test a '\n'-scanning ring fails.
 * "\x80\x0a" is a legal two-byte C47 glyph whose LOW byte is ASCII LF. ---- */
static int test_console_ring_glyph(void)
{
  int i;
  forthConsoleClear();
  forthConsoleAppendLine("a\x80\x0a" "b");
  if (forthConsoleLineCount() != 1) {
    printf("    FAIL: a 0x0A INSIDE a two-byte glyph must not terminate the line"
           " (got %u lines)\n", forthConsoleLineCount());
    return 1;
  }
  if (_consoleLineLen(0) != 4 || !_consoleLineIs(0, "a\x80\x0a" "b")) {
    printf("    FAIL: the glyph must survive intact (expected 4 bytes, got %d)\n",
           _consoleLineLen(0));
    return 1;
  }

  /* 130 two-byte glyphs = 260 bytes against a 255 cap: the 128th glyph would
   * make 256, so it is dropped WHOLE and the line stops at 254 — never 255,
   * which would be half a glyph. */
  forthConsoleClear();
  for (i = 0; i < 130; i++) { forthConsoleAppend(STD_UP_ARROW); }
  if (_consoleLineLen(0) != 254) {
    printf("    FAIL: expected 254 bytes (whole-glyph cap), got %d\n",
           _consoleLineLen(0));
    return 1;
  }

  /* And a short out buffer truncates on a glyph boundary too. */
  { char small[6];
    forthConsoleLineAt(0, small, sizeof(small));
    if (stringByteLength(small) != 4) {
      printf("    FAIL: a 6-byte out buffer must hold 2 whole glyphs (4 bytes), got %d\n",
             stringByteLength(small));
      return 1;
    }
  }
  printf("    PASS: 0x0A inside a glyph is data; the cap and read truncation are glyph-whole\n");
  return 0;
}

/* ---- 6: clear ---- */
static int test_console_ring_clear(void)
{
  forthConsoleClear();
  forthConsoleAppendLine("one");
  forthConsoleAppendLine("two");
  forthConsoleAppend("open");
  forthConsoleSetViewOffset(2);
  forthConsoleClear();

  if (forthConsoleLineCount() != 0 || forthConsoleTestUsed() != 0
      || forthConsoleViewOffset() != 0 || forthConsoleTestHasOpen()) {
    printf("    FAIL: Clear must empty the ring, the open record and the view offset\n");
    return 1;
  }
  { char buf[16];
    if (forthConsoleLineAt(0, buf, sizeof(buf))) {
      printf("    FAIL: LineAt(0) must be false on a cleared ring\n");
      return 1;
    }
  }
  printf("    PASS: Clear empties records, open record and view offset\n");
  return 0;
}

/* ---- 7: the roll offset clamps, and ANY output snaps it to newest (N-R3) ---- */
static int test_console_ring_view(void)
{
  forthConsoleClear();
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: fresh ring must have view offset 0\n");
    return 1;
  }
  forthConsoleSetViewOffset(3);
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: an EMPTY ring must clamp any offset to 0\n");
    return 1;
  }

  forthConsoleAppendLine("1"); forthConsoleAppendLine("2"); forthConsoleAppendLine("3");
  forthConsoleAppendLine("4"); forthConsoleAppendLine("5");

  forthConsoleSetViewOffset(2);
  if (forthConsoleViewOffset() != 2) {
    printf("    FAIL: in-range offset not kept\n");
    return 1;
  }
  forthConsoleSetViewOffset(99);
  if (forthConsoleViewOffset() != 4) {
    printf("    FAIL: offset must clamp to count-1 (4), got %u\n", forthConsoleViewOffset());
    return 1;
  }

  forthConsoleSetViewOffset(3);
  forthConsoleAppendLine("new");
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: AppendLine must snap the view to newest\n");
    return 1;
  }
  forthConsoleSetViewOffset(3);
  forthConsoleAppend("x");
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: Append must snap the view to newest\n");
    return 1;
  }
  forthConsoleSetViewOffset(3);
  forthConsoleNewline();
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: Newline must snap the view to newest\n");
    return 1;
  }
  printf("    PASS: view offset clamps, and every writer snaps it to newest\n");
  return 0;
}

/* ---- 8: the reset seam.  Cleared at the dictionary lifecycle; UNTOUCHED by
 * capture close — N-R2 rules the dialogue survives close and reopen, and a
 * "clear it everywhere" reflex breaks exactly this direction. ---- */
static int test_console_ring_reset_seam(void)
{
  forthConsoleClear();
  forthConsoleAppendLine("a"); forthConsoleAppendLine("b"); forthConsoleAppendLine("c");
  forthDictInit();
  if (forthConsoleLineCount() != 0) {
    printf("    FAIL: forthDictInit must clear the ring (via forthCapPowerReset)\n");
    return 1;
  }

  forthConsoleAppendLine("a"); forthConsoleAppendLine("b");
  forthDictClear();
  if (forthConsoleLineCount() != 0) {
    printf("    FAIL: forthDictClear must clear the ring\n");
    return 1;
  }

  forthDictInit();
  forthConsoleAppendLine("kept");
  forthCapOpenInteractive();
  forthCapClose();
  if (forthConsoleLineCount() != 1 || !_consoleLineIs(0, "kept")) {
    printf("    FAIL: capture close must NOT clear the ring (N-R2: the dialogue survives)\n");
    return 1;
  }
  printf("    PASS: cleared at the dict lifecycle seam, untouched by capture close\n");
  return 0;
}

/* ---- 9: hammer.  5000 mixed operations with sustained wrap-around; the ring
 * invariants asserted after EVERY iteration. ---- */
static int test_console_ring_hammer(void)
{
  int i;
  char forty[41];
  for (i = 0; i < 40; i++) { forty[i] = (char)('a' + (i % 26)); }
  forty[40] = 0;

  forthConsoleClear();
  for (i = 0; i < 5000; i++) {
    switch (i % 7) {
      case 0: forthConsoleAppend("q");                    break;
      case 1: forthConsoleAppend(STD_UP_ARROW);           break;
      case 2: forthConsoleAppend(forty);                  break;
      case 3: forthConsoleNewline();                      break;
      case 4: forthConsoleAppendLine(forty);              break;
      case 5: forthConsoleAppend(forty);                  /* drive toward the cap */
              forthConsoleAppend(forty);
              forthConsoleAppend(forty);
              forthConsoleAppend(forty);
              forthConsoleAppend(forty);
              forthConsoleAppend(forty);
              forthConsoleAppend(forty);                  break;
      default: forthConsoleSetViewOffset((uint16_t)(i % 5)); break;
    }

    /* (1) used never exceeds the ring */
    if (forthConsoleTestUsed() > FORTH_CONSOLE_RING_BYTES) {
      printf("    FAIL: iteration %d: used %u > ring\n", i, forthConsoleTestUsed());
      return 1;
    }
    /* (4) the per-line cap, checked through the PUBLIC reader.  Not checked as
     * `len > FORTH_CONSOLE_LINE_MAX` on the length byte: that byte is a uint8_t
     * and the cap IS 255, so such a test can never fire — a tautology, not a
     * check.  A cap failure would instead wrap the length byte, and that
     * desyncs the walk against `used`, which invariant (2) below catches. */
    if (_consoleLineLen(0) > FORTH_CONSOLE_LINE_MAX) {
      printf("    FAIL: iteration %d: newest line %d bytes > cap\n", i, _consoleLineLen(0));
      return 1;
    }
    /* (2)+(3) walk the records: consume exactly `used` bytes and land on head,
     * with the walked record count equal to LineCount() */
    { uint16_t walked = 0, records = 0, at = forthConsoleTestTail();
      while (walked < forthConsoleTestUsed()) {
        uint8_t len = forthConsoleTestByteAt(at);
        walked = (uint16_t)(walked + 1 + len);
        at = (uint16_t)((at + 1 + len) % FORTH_CONSOLE_RING_BYTES);
        records++;
        if (records > FORTH_CONSOLE_RING_BYTES) {
          printf("    FAIL: iteration %d: record walk did not terminate\n", i);
          return 1;
        }
      }
      if (walked != forthConsoleTestUsed()) {
        printf("    FAIL: iteration %d: walk consumed %u bytes, used is %u\n",
               i, walked, forthConsoleTestUsed());
        return 1;
      }
      if (records != forthConsoleLineCount()) {
        printf("    FAIL: iteration %d: walked %u records, LineCount says %u\n",
               i, records, forthConsoleLineCount());
        return 1;
      }
    }
  }
  printf("    PASS: 5000-operation hammer, every ring invariant held every iteration\n");
  return 0;
}
