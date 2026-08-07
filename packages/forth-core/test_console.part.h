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

/* ==================================================================
 * N1-2 — the console VIEW: geometry, suppression, yields, the roll.
 *
 * Reads the framebuffer back, the G3/G4 way (test_capture.part.h:6304):
 * lcd_buffer is filled by the software blitter whether or not a GTK window
 * exists, and lcd_buffer_pixel_on() links in the sim binary and the
 * upstream testSuite binary alike.  No pixel COUNT is hard-coded — upstream
 * owns the font — only presence, absence and direction of change.
 * ================================================================== */

extern bool_t _forthConsoleActive(void);
extern void   _forthConsoleRender(void);

/* Lit pixels in the transcript band: the stack area ABOVE the AIM editor.
 * The short-line editor's first pixel row is 128, so 24..127 is the band the
 * console owns and the editor never touches.  The status bar ends below
 * Y_POSITION_OF_REGISTER_T_LINE (topLeftGraphInfoY is 20), so nothing else
 * paints here. */
static int32_t _consoleBandPixels(void)
{
  int32_t lit = 0;
  uint32_t x, y;
  for (y = Y_POSITION_OF_REGISTER_T_LINE; y < 128; y++) {
    for (x = 0; x < SCREEN_WIDTH; x++) {
      if (lcd_buffer_pixel_on(x, y)) { lit++; }
    }
  }
  return lit;
}

/* Put the calculator in the state the console arm gates on, with the
 * short-line editor geometry (yMultiLineEdOffset == 3). */
static void _consoleEnterViewState(void)
{
  forthCapOpenInteractive();
  calcMode              = CM_AIM;
  tam.mode              = 0;
  lastErrorCode         = ERROR_NONE;
  temporaryInformation  = TI_NO_INFO;
  yMultiLineEdOffset    = 3;
}

static void _consoleLeaveViewState(uint8_t savedMode)
{
  forthCapClose();
  calcMode = savedMode;
  lastErrorCode = ERROR_NONE;
  temporaryInformation = TI_NO_INFO;
}

/* ---- 10: the gate.  Every conjunct of _forthConsoleActive() falsified one
 * at a time — the arm must yield on each, or it swallows an error display,
 * a temporaryInformation screen, or a TAM prompt. ---- */
static int test_console_view_gate(void)
{
  uint8_t saved = calcMode;
  int fail = 0;

  _consoleEnterViewState();
  if (!_forthConsoleActive()) {
    printf("    FAIL: the console must be active in its own state\n");
    fail = 1;
  }

  lastErrorCode = ERROR_OUT_OF_RANGE;
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield while lastErrorCode != 0 (the error paints on Z)\n");
    fail = 1;
  }
  lastErrorCode = ERROR_NONE;

  temporaryInformation = TI_BATTV;
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield on temporaryInformation — displayTemporaryInformationOnX"
           " repaints all four rows from inside the X paint the console keeps\n");
    fail = 1;
  }
  temporaryInformation = TI_NO_INFO;

  tam.mode = TM_VALUE;
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield during TAM (calcMode stays CM_AIM; the prompt paints on T)\n");
    fail = 1;
  }
  tam.mode = 0;

  calcMode = CM_NORMAL;
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield outside CM_AIM\n");
    fail = 1;
  }
  calcMode = CM_AIM;

  forthCapClose();
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield with no interactive capture open\n");
    fail = 1;
  }

  /* A PEM capture is not the console's origin. */
  forthCapOpen();
  if (_forthConsoleActive()) {
    printf("    FAIL: must yield for a PEM capture — the listing stays\n");
    fail = 1;
  }

  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: the arm yields on every falsified conjunct (error, TI, TAM, mode, origin)\n");
  }
  return fail;
}

/* ---- 11: the transcript reaches the LCD, and an empty console shows an
 * EMPTY area.
 *
 * AUDIT C21: this case does NOT prove register suppression, and its first
 * form claimed it did.  It drives _forthConsoleRender() directly, and that
 * function contains no register-paint call by construction — no input can
 * reach the register path through it.  The suppression proof lives in the
 * ARM case (test 14), which goes through refreshScreen(), where the
 * suppressed refreshRegisterLine(T/Z/Y) calls actually exist. ---- */
static int test_console_view_paints(void)
{
  uint8_t saved = calcMode;
  int32_t withText, empty;
  int fail = 0;

  forthPushInt32(11111); forthPushInt32(22222); forthPushInt32(33333);

  _consoleEnterViewState();
  forthConsoleClear();

  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  empty = _consoleBandPixels();
  /* DOCUMENTED GAP — this assertion is defended three deep and NO single
   * mutation found so far can fire it.  Recorded rather than papered over,
   * and the history is worth keeping because two confident attributions were
   * both false:
   *
   *   - it was first advertised as proving "no register paints while the
   *     console is up" (AUDIT C21 killed that: this case calls
   *     _forthConsoleRender directly, which has no register-paint call on
   *     its path at all — the real proof lives in test 14);
   *   - the C21 commit then re-attributed it to the renderer's `count == 0`
   *     early return.  Also false: delete that return and every row hits the
   *     `view >= count` skip, so nothing paints (AUDIT round 3);
   *   - and the skip is not it either.  MUTATION RUN: turning that
   *     `continue` into a fallthrough left the gate GREEN, because
   *     forthConsoleLineAt() rejects an out-of-range view and the
   *     `!forthConsoleLineAt(...) || line[0] == 0` guard below catches it.
   *
   * So the assertion is real but effectively unfalsifiable by any one-line
   * change to the current renderer: three independent mechanisms each
   * produce the empty band.  It stays as a belt-and-braces regression guard
   * against a rewrite that removes all three.  It is NOT evidence of
   * anything, and no comment here may claim it is. */
  if (empty != 0) {
    printf("    FAIL: an empty console must paint nothing in the band"
           " (got %d lit px)\n", empty);
    fail = 1;
  }

  forthConsoleAppendLine("HELLO CONSOLE");
  forthConsoleAppendLine("SECOND LINE");
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  withText = _consoleBandPixels();
  if (withText <= 0) {
    printf("    FAIL: transcript text never reached lcd_buffer (%d lit px)\n", withText);
    fail = 1;
  }

  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: transcript reaches the LCD (%d px); an empty console paints 0 px\n",
           withText);
  }
  return fail;
}

/* ---- 12: row count per editor state.  Four rows fit above the short-line
 * editor and two above the long-line one, so a six-line dialogue paints
 * strictly MORE ink in the short state than in the long one. ---- */
static int test_console_view_rows(void)
{
  uint8_t saved = calcMode;
  int32_t shortState, longState;
  int i, fail = 0;

  _consoleEnterViewState();
  forthConsoleClear();
  for (i = 0; i < 6; i++) { forthConsoleAppendLine("MMMMMMMMMMMMMMMMMMMM"); }

  yMultiLineEdOffset = 3;                       /* short line: editorTop 128 -> 4 rows */
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  shortState = _consoleBandPixels();

  yMultiLineEdOffset = 1;                       /* long line: editorTop 67 -> 2 rows */
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  longState = _consoleBandPixels();

  if (!(shortState > longState && longState > 0)) {
    printf("    FAIL: expected short-line ink > long-line ink > 0, got %d and %d\n",
           shortState, longState);
    fail = 1;
  }
  /* Identical lines, so the ratio is the row ratio: 4 rows vs 2. */
  if (!fail && shortState != 2 * longState) {
    printf("    FAIL: with identical lines the ink must be exactly 4 rows vs 2"
           " (%d vs %d)\n", shortState, longState);
    fail = 1;
  }

  yMultiLineEdOffset = 3;
  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: 4 rows short-line, 2 rows long-line (%d px vs %d px)\n",
           shortState, longState);
  }
  return fail;
}

/* ---- 13: the roll.  Rolling back must reveal older lines, rolling forward
 * must restore the newest, and any output must snap the view home. ---- */
static int test_console_view_roll(void)
{
  uint8_t saved = calcMode;
  int32_t atNewest, rolledBack, rolledHome, afterOutput;
  int i, fail = 0;

  _consoleEnterViewState();
  forthConsoleClear();
  /* One wide line, then six narrow ones: the wide line is off-screen at the
   * newest view and must appear once the view rolls back past six lines. */
  forthConsoleAppendLine("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
  for (i = 0; i < 6; i++) { forthConsoleAppendLine("."); }

  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  atNewest = _consoleBandPixels();

  for (i = 0; i < 6; i++) { forthConsoleRoll(+1); }
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  rolledBack = _consoleBandPixels();

  if (rolledBack <= atNewest) {
    printf("    FAIL: rolling back must reveal the wide older line (%d px vs %d px)\n",
           rolledBack, atNewest);
    fail = 1;
  }

  for (i = 0; i < 20; i++) { forthConsoleRoll(-1); }   /* clamps at the newest */
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: rolling forward must clamp at the newest line, offset is %u\n",
           forthConsoleViewOffset());
    fail = 1;
  }
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  rolledHome = _consoleBandPixels();
  if (rolledHome != atNewest) {
    printf("    FAIL: rolling home must restore the newest view exactly (%d px vs %d px)\n",
           rolledHome, atNewest);
    fail = 1;
  }

  /* And output snaps the view home from wherever it is. */
  for (i = 0; i < 6; i++) { forthConsoleRoll(+1); }
  forthConsoleAppendLine(".");
  if (forthConsoleViewOffset() != 0) {
    printf("    FAIL: output must snap the view to newest, offset is %u\n",
           forthConsoleViewOffset());
    fail = 1;
  }
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  afterOutput = _consoleBandPixels();
  if (afterOutput != atNewest) {
    printf("    FAIL: after the snap the view must show the newest rows (%d px vs %d px)\n",
           afterOutput, atNewest);
    fail = 1;
  }

  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: roll reveals older lines (%d px), clamps, and output snaps home\n",
           rolledBack);
  }
  return fail;
}

/* ---- 14: the ARM is wired, and the suppression is REAL.  Everything above
 * drives _forthConsoleRender directly; this one goes through refreshScreen()
 * and proves _refreshNormalScreen's CM_AIM arm actually reaches it — and
 * that the yield falls back to the landed register paint rather than to
 * nothing.
 *
 * AUDIT C21: the two suppression oracles live here, because this is the only
 * case where the suppressed refreshRegisterLine(T/Z/Y) calls are even on the
 * code path.  Both are exact, not lower bounds — extra register ink
 * satisfies a lower bound MORE easily, which is how the original battery
 * could not fail when the suppression `else` was removed:
 *
 *   (a) EQUALITY: the band painted through the arm equals the band painted
 *       by a direct render of the same transcript.  Register ink on top of
 *       the transcript breaks the equality.
 *   (b) THE MIRROR: an ACTIVE console with an EMPTY ring, T/Z/Y holding
 *       values, refreshed through the arm, paints NOTHING in the band —
 *       _forthConsoleRender returns at its count==0 guard, so any ink here
 *       is provably a register paint the console failed to suppress. ---- */
static int test_console_view_arm(void)
{
  uint8_t saved = calcMode;
  int32_t viaArm, direct, suppressed, yielded;
  int fail = 0;

  forthPushInt32(12345); forthPushInt32(23456); forthPushInt32(34567);
  _consoleEnterViewState();
  forthConsoleClear();
  forthConsoleAppendLine("THROUGH THE ARM");
  forthConsoleAppendLine("SECOND");

  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(900);
  viaArm = _consoleBandPixels();
  if (viaArm <= 0) {
    printf("    FAIL: refreshScreen did not reach the console arm (%d lit px)\n", viaArm);
    fail = 1;
  }

  /* C21 (a): the equality oracle. */
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  direct = _consoleBandPixels();
  if (viaArm != direct) {
    printf("    FAIL: the band through the arm must EQUAL a direct render of the"
           " same transcript (%d px vs %d px — a register painted with the"
           " transcript)\n", viaArm, direct);
    fail = 1;
  }

  /* C21 (b): the mirror — empty ring, loaded registers, through the arm. */
  forthConsoleClear();
  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(902);
  suppressed = _consoleBandPixels();
  if (suppressed != 0) {
    printf("    FAIL: an active console with an empty ring must paint NOTHING in"
           " the band through the arm (got %d lit px — a register leaked"
           " through)\n", suppressed);
    fail = 1;
  }

  /* Yield: with an error live, the native register/error paint comes back.
   * (The ring is already empty from (b), so any ink in the band is NOT ours.) */
  lastErrorCode = ERROR_OUT_OF_RANGE;
  errorMessageRegisterLine = REGISTER_Z;
  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(901);
  yielded = _consoleBandPixels();
  if (yielded <= 0) {
    printf("    FAIL: on yield the native paint must return (%d lit px) —"
           " an empty console must not swallow the error display\n", yielded);
    fail = 1;
  }
  lastErrorCode = ERROR_NONE;

  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: the arm is wired (%d px == %d px direct); empty-ring arm"
           " paints 0 px with registers loaded; yield restores the native paint"
           " (%d px)\n", viaArm, direct, yielded);
  }
  return fail;
}

/* ---- 15: row placement.  The newest line is at the BOTTOM, just above the
 * input band, and older lines roll upward.  Every other view test compares
 * ink TOTALS, which a reversed row order would leave untouched — this one
 * asks which half of the band the ink is in. ---- */
static int test_console_view_placement(void)
{
  uint8_t saved = calcMode;
  int32_t topHalf = 0, bottomHalf = 0;
  uint32_t x, y;
  int i, fail = 0;

  _consoleEnterViewState();
  forthConsoleClear();
  /* Four blank lines, then one wide line as the NEWEST.  Rows are at
   * Y 44/65/86/107, so with newest-at-bottom the only ink is at Y >= 107. */
  for (i = 0; i < 4; i++) { forthConsoleAppendLine(""); }
  forthConsoleAppendLine("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");

  lcd_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, LCD_SET_VALUE);
  _forthConsoleRender();
  for (y = Y_POSITION_OF_REGISTER_T_LINE; y < 128; y++) {
    for (x = 0; x < SCREEN_WIDTH; x++) {
      if (lcd_buffer_pixel_on(x, y)) {
        if (y < 86) { topHalf++; } else { bottomHalf++; }
      }
    }
  }

  if (bottomHalf <= 0) {
    printf("    FAIL: the newest line must paint in the BOTTOM rows, just above"
           " the input band (%d px there)\n", bottomHalf);
    fail = 1;
  }
  if (topHalf != 0) {
    printf("    FAIL: the older (blank) lines must leave the top rows empty"
           " (%d px there — row order reversed?)\n", topHalf);
    fail = 1;
  }

  _consoleLeaveViewState(saved);
  if (!fail) {
    printf("    PASS: newest line paints at the bottom (%d px), older rows above it empty\n",
           bottomHalf);
  }
  return fail;
}

/* ==================================================================
 * N1-3 — the dialogue: what ENTER writes into the transcript.
 * ================================================================== */

/* The same reset the landed L1-2 battery uses (test_capture.part.h:7743),
 * plus the ring, so each subcase starts from a known dialogue. */
#define N13_RESET() do { \
  calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
  programRunStop = PGM_STOPPED; dynamicMenuItem = -1; \
  lastErrorCode = ERROR_NONE; forthCapClose(); forthConsoleClear(); \
} while (0)

/* Type a line into the open capture and commit it, the way the key path
 * does — forthInteractiveEnter is what keyboard.c:3694's ENTER arm calls. */
static void _consoleEnterLine(const char *src)
{
  int32_t n = stringByteLength((char *)src);
  xcopy(aimBuffer, src, (uint32_t)n + 1);
  T_cursorPos = (int16_t)n;
  forthInteractiveEnter();
}

/* ---- 16: the echo, the result, and the one-act ordering. ---- */
static int test_console_dialogue_echo(void)
{
  int fail = 0;
  char l0[FORTH_CONSOLE_FMT_MAX], l1[FORTH_CONSOLE_FMT_MAX];

  N13_RESET();
  forthCapOpenInteractive();
  calcMode = CM_AIM;

  _consoleEnterLine("1 2 +");

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: \"1 2 +\" errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  if (forthConsoleLineCount() != 2) {
    printf("    FAIL: expected 2 transcript lines (echo + result), got %u\n",
           forthConsoleLineCount());
    fail = 1;
  }
  else {
    forthConsoleLineAt(1, l1, sizeof(l1));      /* older: the echo */
    forthConsoleLineAt(0, l0, sizeof(l0));      /* newer: the result */
    if (compareString(l1, STD_RIGHT_DOUBLE_ANGLE " 1 2 +", CMP_BINARY) != 0) {
      printf("    FAIL: echo is \"%s\", expected the prompt-prefixed line\n", l1);
      fail = 1;
    }
    if (stringByteLength(l0) == 0) {
      printf("    FAIL: no result echo — the console must answer with where X landed\n");
      fail = 1;
    }
    else if (compareString(l0, "3", CMP_BINARY) != 0) {
      printf("    FAIL: result echo is \"%s\", expected \"3\"\n", l0);
      fail = 1;
    }
  }

  /* The echo is ONE ACT with the FHIST push: the line the transcript shows
   * is the line history recalled. */
  { char recalled[FORTH_CONSOLE_FMT_MAX];
    aimBuffer[0] = 0;
    forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);
    forthHistoryRecall(-1);
    xcopy(recalled, aimBuffer, (uint32_t)stringByteLength(aimBuffer) + 1);
    if (compareString(recalled, "1 2 +", CMP_BINARY) != 0) {
      printf("    FAIL: FHIST recalled \"%s\", expected the echoed line \"1 2 +\"\n", recalled);
      fail = 1;
    }
  }

  forthCapClose();
  forthConsoleClear();
  if (!fail) {
    printf("    PASS: ENTER echoes the prompt-prefixed line then X, and history holds the same line\n");
  }
  return fail;
}

/* ---- 17: the error line.  The native error paint is transient; the
 * transcript is what keeps the dialogue readable afterwards. ---- */
static int test_console_dialogue_error(void)
{
  int fail = 0;
  char l0[FORTH_CONSOLE_FMT_MAX], l1[FORTH_CONSOLE_FMT_MAX];

  N13_RESET();
  forthCapOpenInteractive();
  calcMode = CM_AIM;

  _consoleEnterLine("NOSUCHWORD");

  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: an undefined word must raise\n");
    fail = 1;
  }
  if (forthConsoleLineCount() != 2) {
    printf("    FAIL: expected echo + error line, got %u lines\n", forthConsoleLineCount());
    fail = 1;
  }
  else {
    forthConsoleLineAt(1, l1, sizeof(l1));
    forthConsoleLineAt(0, l0, sizeof(l0));
    if (compareString(l1, STD_RIGHT_DOUBLE_ANGLE " NOSUCHWORD", CMP_BINARY) != 0) {
      printf("    FAIL: echo is \"%s\"\n", l1);
      fail = 1;
    }
    if (compareString(l0, errorMessageOf(lastErrorCode), CMP_BINARY) != 0) {
      printf("    FAIL: error line is \"%s\", expected \"%s\"\n",
             l0, errorMessageOf(lastErrorCode));
      fail = 1;
    }
  }
  /* L5 interplay: the line is still in the editor for correction AND the
   * transcript carries the message. */
  if (compareString(aimBuffer, "NOSUCHWORD", CMP_BINARY) != 0) {
    printf("    FAIL: the line must survive in the editor, got \"%s\"\n", aimBuffer);
    fail = 1;
  }
  if (forthTestCapState() != FCAP_OPEN) {
    printf("    FAIL: the capture must stay open after an error\n");
    fail = 1;
  }

  lastErrorCode = ERROR_NONE;
  forthCapClose();
  forthConsoleClear();
  if (!fail) {
    printf("    PASS: an error echoes the line then the message, and the editor keeps the line\n");
  }
  return fail;
}

/* ---- 18: a REFUSED line writes NOTHING.  The echo sits after the E9 gate,
 * so a line that never ran must not appear in the dialogue — nor in
 * history.  This is the ordering half of the one-act rule. ---- */
static int test_console_dialogue_refusal(void)
{
  int fail = 0;
  uint16_t before;

  N13_RESET();
  forthCapOpenInteractive();
  calcMode = CM_AIM;
  forthConsoleAppendLine("PREVIOUS");
  before = forthConsoleLineCount();

  _consoleEnterLine(": A IF ;");                 /* tier-1 structural reject */

  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: \": A IF ;\" must be refused by the E9 gate\n");
    fail = 1;
  }
  if (forthConsoleLineCount() != before) {
    printf("    FAIL: a refused line must not echo (%u lines, expected %u)\n",
           forthConsoleLineCount(), before);
    fail = 1;
  }
  if (compareString(aimBuffer, ": A IF ;", CMP_BINARY) != 0) {
    printf("    FAIL: the refused line must stay in the editor, got \"%s\"\n", aimBuffer);
    fail = 1;
  }

  /* An empty ENTER is a no-op too — nothing to run, nothing to echo. */
  aimBuffer[0] = 0;
  lastErrorCode = ERROR_NONE;
  forthInteractiveEnter();
  if (forthConsoleLineCount() != before) {
    printf("    FAIL: an empty ENTER must write nothing (%u lines)\n",
           forthConsoleLineCount());
    fail = 1;
  }

  lastErrorCode = ERROR_NONE;
  forthCapClose();
  forthConsoleClear();
  if (!fail) {
    printf("    PASS: a refused line and an empty ENTER both write nothing to the dialogue\n");
  }
  return fail;
}

/* ---- 19: the REPL runs, so the dialogue accumulates.  Three lines in,
 * six transcript lines out, in order, oldest first. ---- */
static int test_console_dialogue_session(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];

  N13_RESET();
  forthCapOpenInteractive();
  calcMode = CM_AIM;

  _consoleEnterLine("XEQ 'CLSTK'");
  _consoleEnterLine("2 3 *");
  _consoleEnterLine("4 +");

  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: session errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  if (forthConsoleLineCount() != 6) {
    printf("    FAIL: three lines must leave six transcript lines, got %u\n",
           forthConsoleLineCount());
    fail = 1;
  }
  else {
    forthConsoleLineAt(0, line, sizeof(line));
    if (compareString(line, "10", CMP_BINARY) != 0) {
      printf("    FAIL: newest line is \"%s\", expected the result \"10\"\n", line);
      fail = 1;
    }
    forthConsoleLineAt(1, line, sizeof(line));
    if (compareString(line, STD_RIGHT_DOUBLE_ANGLE " 4 +", CMP_BINARY) != 0) {
      printf("    FAIL: line 1 is \"%s\", expected the last echo\n", line);
      fail = 1;
    }
    forthConsoleLineAt(5, line, sizeof(line));
    if (compareString(line, STD_RIGHT_DOUBLE_ANGLE " XEQ 'CLSTK'", CMP_BINARY) != 0) {
      printf("    FAIL: oldest line is \"%s\", expected the first echo\n", line);
      fail = 1;
    }
  }

  forthCapClose();
  forthConsoleClear();
  if (!fail) {
    printf("    PASS: a three-line REPL session leaves six transcript lines in order\n");
  }
  return fail;
}

/* ==================================================================
 * N1-4 — the seven output words.  Ring bytes, stack deltas, type errors,
 * and the rule that they write with no console open.
 * ================================================================== */

/* Run a Forth line with a clean ring and a clean error state. */
static void _consoleRun(const char *src)
{
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret((char *)src);
}

/* ---- 20: `.` `SPACE` `CR` — the printing core, with NO console open.
 * The words write the ring wherever they run: one rule, no cases. ---- */
static int test_console_words_print(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];

  forthCapClose();                        /* deliberately: no console open */
  calcMode = CM_NORMAL;
  forthDictInit();
  forthConsoleClear();

  _consoleRun("XEQ 'CLSTK' 1 . 2 . 3 .");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: printing line errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "1 2 3 ", CMP_BINARY) != 0) {
    printf("    FAIL: `1 . 2 . 3 .` wrote \"%s\", expected \"1 2 3 \"\n", line);
    fail = 1;
  }
  if (!forthConsoleHasOpenLine()) {
    printf("    FAIL: `.` must not close the line — CR does that\n");
    fail = 1;
  }

  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 7 . CR 8 .");
  if (forthConsoleLineCount() != 2) {
    printf("    FAIL: CR must break the line (%u lines)\n", forthConsoleLineCount());
    fail = 1;
  }
  else {
    forthConsoleLineAt(1, line, sizeof(line));
    if (compareString(line, "7 ", CMP_BINARY) != 0) {
      printf("    FAIL: first line \"%s\", expected \"7 \"\n", line);
      fail = 1;
    }
  }

  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 5 . SPACE 6 .");
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "5  6 ", CMP_BINARY) != 0) {
    printf("    FAIL: SPACE line is \"%s\", expected \"5  6 \"\n", line);
    fail = 1;
  }

  forthConsoleClear();
  if (!fail) {
    printf("    PASS: `.` `SPACE` `CR` write the ring with no console open\n");
  }
  return fail;
}

/* ---- 21: stack deltas.  `.` consumes, `.S` does not. ---- */
static int test_console_words_stack(void)
{
  int fail = 0;
  uint8_t t; int32_t v;
  char line[FORTH_CONSOLE_FMT_MAX];

  forthDictInit();
  forthConsoleClear();

  _consoleRun("XEQ 'CLSTK' 11 22 .");
  read_reg_int32(REGISTER_X, &t, &v);
  if (t != dtLongInteger || v != 11) {
    printf("    FAIL: after `11 22 .` X should be 11 (`.` DROPs), got type %u value %ld\n",
           t, (long)v);
    fail = 1;
  }

  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 33 .S");
  read_reg_int32(REGISTER_X, &t, &v);
  if (t != dtLongInteger || v != 33) {
    printf("    FAIL: `.S` must be non-destructive, X is type %u value %ld\n", t, (long)v);
    fail = 1;
  }
  forthConsoleLineAt(0, line, sizeof(line));
  if (line[0] != '<') {
    printf("    FAIL: `.S` must lead with the depth, wrote \"%s\"\n", line);
    fail = 1;
  }
  if (forthConsoleHasOpenLine()) {
    printf("    FAIL: `.S` writes a whole line and closes it\n");
    fail = 1;
  }

  /* The DECLARED stack delta, not just the observable DROP.  forthPrims'
   * fourth field feeds forthDataDepthApply (forth_inner.c:152), which spills
   * Forth-owned values into the arena once the counter reaches capacity.  A
   * `.` declared 0 instead of -1 never decrements, so the counter climbs on a
   * print-heavy line and the engine spills values that should never have
   * spilled — invisible in X, loud here.  Ten push/print pairs never exceed a
   * depth of one, so the correct answer is zero spills. */
  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 1 . 2 . 3 . 4 . 5 . 6 . 7 . 8 . 9 . 10 .");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: a print-heavy line errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  if (forthSpillCount() != 0) {
    printf("    FAIL: %u value(s) spilled on a line whose depth never exceeds one —"
           " `.`'s declared stack delta is wrong\n", forthSpillCount());
    fail = 1;
  }

  forthConsoleClear();
  if (!fail) {
    printf("    PASS: `.` consumes X and declares it (zero spills), `.S` leaves the stack alone\n");
  }
  return fail;
}

/* ---- 22: EMIT's code space and its refusals. ---- */
static int test_console_words_emit(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];

  forthDictInit();
  forthConsoleClear();

  _consoleRun("XEQ 'CLSTK' 65 EMIT 66 EMIT 67 EMIT");
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "ABC", CMP_BINARY) != 0) {
    printf("    FAIL: `65 EMIT 66 EMIT 67 EMIT` wrote \"%s\", expected \"ABC\"\n", line);
    fail = 1;
  }

  /* A two-byte glyph: STD_UP_ARROW is 0xa191 = 41361. */
  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 41361 EMIT");
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, STD_UP_ARROW, CMP_BINARY) != 0) {
    printf("    FAIL: a two-byte glyph code must emit both bytes\n");
    fail = 1;
  }

  /* A bare high byte is a TRUNCATED glyph, not a character: refused. */
  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 200 EMIT");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: EMIT must refuse 200 — a lone high byte is half a glyph\n");
    fail = 1;
  }
  if (forthConsoleLineCount() != 0) {
    printf("    FAIL: a refused EMIT must write nothing\n");
    fail = 1;
  }
  lastErrorCode = ERROR_NONE;

  /* And a string in X is the standard type error. */
  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK'");
  x_set_string("ABC");
  _consoleRun("EMIT");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: EMIT of a string must raise the type error\n");
    fail = 1;
  }
  lastErrorCode = ERROR_NONE;

  forthConsoleClear();
  if (!fail) {
    printf("    PASS: EMIT writes ASCII and two-byte glyphs, refuses a lone high byte and a string\n");
  }
  return fail;
}

/* ---- 23: `.$` and `PAGE`. ---- */
static int test_console_words_str_page(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];
  uint8_t t; int32_t v;

  forthDictInit();
  forthConsoleClear();

  /* No string LITERALS in this dialect (§2.2 has no string token — a Stage N
   * non-goal), so X is seeded directly, the way every landed string test
   * does it. */
  _consoleRun("XEQ 'CLSTK'");
  x_set_string("HELLO");
  _consoleRun(".$");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: `.$` of a string errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "HELLO", CMP_BINARY) != 0) {
    printf("    FAIL: `.$` wrote \"%s\", expected \"HELLO\"\n", line);
    fail = 1;
  }

  forthConsoleClear();
  _consoleRun("XEQ 'CLSTK' 42 .$");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FAIL: `.$` of a non-string must raise the type error\n");
    fail = 1;
  }
  lastErrorCode = ERROR_NONE;
  read_reg_int32(REGISTER_X, &t, &v);
  if (t != dtLongInteger || v != 42) {
    printf("    FAIL: a refused `.$` must not DROP\n");
    fail = 1;
  }

  /* PAGE clears the VIEW and leaves history alone.  Probed through recall
   * rather than a step count: recall is the user-visible face of FHIST, and
   * what must survive is the ability to get the line back. */
  forthConsoleClear();
  forthHistoryPush("REMEMBER ME");
  forthConsoleAppendLine("one");
  forthConsoleAppendLine("two");
  _consoleRun("PAGE");
  if (forthConsoleLineCount() != 0) {
    printf("    FAIL: PAGE must clear the view (%u lines left)\n", forthConsoleLineCount());
    fail = 1;
  }
  { char recalled[FORTH_CONSOLE_FMT_MAX];
    aimBuffer[0] = 0;
    forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);
    forthHistoryRecall(-1);
    xcopy(recalled, aimBuffer, (uint32_t)stringByteLength(aimBuffer) + 1);
    if (compareString(recalled, "REMEMBER ME", CMP_BINARY) != 0) {
      printf("    FAIL: PAGE must not touch FHIST — recall gave \"%s\","
             " expected \"REMEMBER ME\" (history surgery is not a display act)\n",
             recalled);
      fail = 1;
    }
  }

  forthConsoleClear();
  if (!fail) {
    printf("    PASS: `.$` prints a string and refuses anything else; PAGE clears the view only\n");
  }
  return fail;
}

/* ---- 24: the words run from a PROGRAM STEP too — a bounded BSS write is
 * legal from every context (§3.3.2), and no console need be open. ---- */
static int test_console_words_program(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];

  forthDictInit();
  forthConsoleClear();
  forthCapClose();
  calcMode = CM_NORMAL;

  /* A colon definition that prints, invoked from an interpreted line: the
   * word body runs under forthInner, one nesting level down. */
  _consoleRun("XEQ 'CLSTK' : SHOUT 72 EMIT 73 EMIT ; 0 DROP SHOUT");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: defining and running a printing word errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "HI", CMP_BINARY) != 0) {
    printf("    FAIL: a word body's output is \"%s\", expected \"HI\"\n", line);
    fail = 1;
  }

  forthConsoleClear();
  if (!fail) {
    printf("    PASS: output words write the ring from inside a compiled word, no console open\n");
  }
  return fail;
}

/* ==================================================================
 * N1-5 — keys-first entry, the FWRD home row, and the re-derived ladder.
 * ================================================================== */

/* ---- 25: the default flips for the INTERACTIVE origin and only there.
 * Both leak directions are regressions: a PEM capture opening in keys, or an
 * interactive one opening in alpha (the feature dead). ---- */
static int test_console_keys_first(void)
{
  int fail = 0;

  N13_RESET();
  fnForthOuter(NOPARAM);
  if (!forthCapIsInteractive()) {
    printf("    FAIL: fixture — interactive open did not take\n");
    return 1;
  }
  if (!forthCapKeysMode()) {
    printf("    FAIL: the console must open in KEYS input (N-R6)\n");
    fail = 1;
  }
  if (currentMenu() != -MNU_FORTH) {
    printf("    FAIL: the console's home row must be FWRD, got menu %d\n", currentMenu());
    fail = 1;
  }

  /* And it must survive the REPL reopen — every ENTER, not just the open. */
  xcopy(aimBuffer, "1 2 +", 6);
  T_cursorPos = 5;
  forthInteractiveEnter();
  if (!forthCapKeysMode()) {
    printf("    FAIL: keys input must survive the REPL reopen\n");
    fail = 1;
  }

  /* The leak direction: PEM inherits the universal open-reset untouched. */
  forthCapClose();
  forthCapOpen();                          /* the PEM origin */
  if (forthCapKeysMode()) {
    printf("    FAIL: a PEM capture must still open in ALPHA input (K4's pin stands)\n");
    fail = 1;
  }
  if (forthCapIsInteractive()) {
    printf("    FAIL: fixture — forthCapOpen must be the PEM origin\n");
    fail = 1;
  }

  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: interactive opens keys-first with FWRD home and stays; PEM still opens alpha\n");
  }
  return fail;
}

/* ---- 26: the re-derived EXIT ladder, rung by rung, plus the frame the open
 * must not eat. ---- */
static int test_console_exit_ladder(void)
{
  int fail = 0;

  /* Rung 1 INVERTED: from the alpha excursion, EXIT returns to keys and
   * restores the FWRD home row; the capture stays open.
   *
   * AUDIT C17 fixture repair: the excursion is entered through the REAL
   * E10/E11 toggle, not by forcing keysMode and hand-pushing -MNU_ALPHA.
   * The hand-push created a SEPARATE unregistered row above the console's
   * frame — under frame ownership that is a user-stacked row (rung 1
   * declines it, rung 2 pops it), not the excursion, so the old fixture no
   * longer reaches the state this case claims to test (the C22 rule). */
  N13_RESET();
  fnForthOuter(NOPARAM);
  runFunction(ITM_AIM);
  if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
    printf("    FIXTURE FAIL: rung 1 — the E10/E11 toggle did not enter the"
           " alpha excursion (keys=%d, menu %d)\n",
           (int)forthCapKeysMode(), currentMenu());
    fail = 1;
  }
  fnKeyExit(NOPARAM);
  if (!forthCapIsOpen() || !forthCapKeysMode()) {
    printf("    FAIL: rung 1 — EXIT from alpha must return to keys with the capture open\n");
    fail = 1;
  }
  if (currentMenu() != -MNU_FORTH) {
    printf("    FAIL: rung 1 must restore the FWRD home row, got menu %d\n", currentMenu());
    fail = 1;
  }

  /* Rung 2: a menu stacked over the console pops and the capture survives.
   * Its own fixture — a fresh open, so the stack under the console is
   * whatever the reset left and not the residue of the rung-1 case. */
  N13_RESET();
  fnForthOuter(NOPARAM);
  showSoftmenu(-MNU_STK);
  if (currentMenu() != -MNU_STK) {
    printf("    FAIL: fixture — could not stack STK over the console\n");
    fail = 1;
  }
  fnKeyExit(NOPARAM);
  if (!forthCapIsOpen()) {
    printf("    FAIL: rung 2 — a stacked menu must pop without closing the capture\n");
    fail = 1;
  }
  if (currentMenu() == -MNU_STK) {
    printf("    FAIL: rung 2 did not pop the stacked menu\n");
    fail = 1;
  }

  /* Rung 3: from the keys ground, EXIT closes. */
  N13_RESET();
  fnForthOuter(NOPARAM);
  fnKeyExit(NOPARAM);
  if (forthCapIsOpen()) {
    printf("    FAIL: rung 3 — EXIT from the keys ground must close the capture\n");
    fail = 1;
  }

  /* And the frame the open must NOT eat: with FWRD already displayed, the
   * open pushes nothing that displaces it, so EXIT must leave it standing. */
  N13_RESET();
  showSoftmenu(-MNU_STK);
  showSoftmenu(-MNU_FORTH);
  fnForthOuter(NOPARAM);
  fnKeyExit(NOPARAM);
  if (forthCapIsOpen()) {
    printf("    FAIL: EXIT did not close over an already-FWRD stack\n");
    fail = 1;
  }
  if (currentMenu() != -MNU_FORTH) {
    printf("    FAIL: opening over an already-displayed FWRD must not eat the user's own"
           " frame — got menu %d, expected FWRD still up\n", currentMenu());
    fail = 1;
  }

  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: rung 1 inverts to keys+FWRD, rung 2 pops, rung 3 closes, home frame preserved\n");
  }
  return fail;
}

/* ==================================================================
 * N1-6 — the stage acceptance battery: one session, end to end, plus the
 * one-history assertion and the close/power-reset sweep.
 * ================================================================== */

/* ---- 27: the story.  Open keys-first, do arithmetic, take an alpha
 * excursion to define a word, make it GLOBAL, print with the new words,
 * roll back through the dialogue and snap forward, PAGE, EXIT, reopen with
 * the dialogue intact, then power-reset and watch the view die while FHIST
 * still recalls. ---- */
static int test_console_story(void)
{
  int fail = 0;
  char line[FORTH_CONSOLE_FMT_MAX];
  uint16_t afterSession;

  N13_RESET();
  forthDictInit();

  /* [1] Open: keys input, FWRD home, empty dialogue. */
  fnForthOuter(NOPARAM);
  if (!forthCapIsInteractive() || !forthCapKeysMode() || currentMenu() != -MNU_FORTH) {
    printf("    [1] FAIL: the console did not open keys-first with FWRD home\n");
    fail = 1;
  }
  if (forthConsoleLineCount() != 0) {
    printf("    [1] FAIL: a fresh console after a dictionary init must show nothing\n");
    fail = 1;
  }

  /* [2] Arithmetic: the echo, then where X landed. */
  _consoleEnterLine("XEQ 'CLSTK' 2 3 +");
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "5", CMP_BINARY) != 0) {
    printf("    [2] FAIL: expected the result \"5\" as the newest line, got \"%s\"\n", line);
    fail = 1;
  }

  /* [2b] The line surface SURVIVES a native item that resets calcMode.
   * `XEQ 'CLSTK'` above calls calcModeNormal() inside fnClearStack, which
   * without the repair leaves the capture open but off the AIM surface —
   * keys stop routing through it and the console vanishes. */
  if (calcMode != CM_AIM || !getSystemFlag(FLAG_ALPHA)) {
    printf("    [2b] FAIL: the line surface did not survive a calcMode-resetting item"
           " (calcMode=%u alpha=%d)\n", calcMode, getSystemFlag(FLAG_ALPHA));
    fail = 1;
  }

  /* [3] The alpha excursion: define a word, make it global. */
  forthCapSetKeysMode(false);
  _consoleEnterLine(": SQ DUP * ; GLOBAL");
  if (lastErrorCode != ERROR_NONE) {
    printf("    [3] FAIL: defining SQ errored (%u)\n", lastErrorCode);
    fail = 1;
  }
  { uint16_t ref;
    if (!forthFindColonRef("SQ", &ref, NULL) || !(ref & FORTH_REF_GLOBAL)) {
      printf("    [3] FAIL: SQ is not a GLOBAL word after the definition\n");
      fail = 1;
    }
  }

  /* [4] Back to keys, and use the word with the new output words. */
  forthCapSetKeysMode(true);
  _consoleEnterLine("7 SQ .");
  forthConsoleLineAt(0, line, sizeof(line));
  if (compareString(line, "49 ", CMP_BINARY) != 0) {
    printf("    [4] FAIL: `7 SQ .` printed \"%s\", expected \"49 \"\n", line);
    fail = 1;
  }
  /* The word printed, so the dialogue must NOT also append X underneath. */
  forthConsoleLineAt(1, line, sizeof(line));
  if (compareString(line, STD_RIGHT_DOUBLE_ANGLE " 7 SQ .", CMP_BINARY) != 0) {
    printf("    [4] FAIL: a line that printed must not get a second X answer"
           " (line 1 is \"%s\")\n", line);
    fail = 1;
  }

  _consoleEnterLine(".S");
  forthConsoleLineAt(0, line, sizeof(line));
  if (line[0] != '<') {
    printf("    [5] FAIL: `.S` did not draw the stack picture (\"%s\")\n", line);
    fail = 1;
  }

  /* [6] Roll back through the dialogue, then snap forward. */
  afterSession = forthConsoleLineCount();
  forthConsoleRoll(+3);
  if (forthConsoleViewOffset() != 3) {
    printf("    [6] FAIL: the roll did not move the view (offset %u)\n",
           forthConsoleViewOffset());
    fail = 1;
  }
  _consoleEnterLine("1 1 +");
  if (forthConsoleViewOffset() != 0) {
    printf("    [6] FAIL: a commit must snap the view back to newest (offset %u)\n",
           forthConsoleViewOffset());
    fail = 1;
  }
  if (forthConsoleLineCount() <= afterSession) {
    printf("    [6] FAIL: the session did not accumulate\n");
    fail = 1;
  }

  /* [7] PAGE clears the view; the capture and history are untouched. */
  _consoleEnterLine("PAGE");
  if (forthConsoleLineCount() != 0) {
    printf("    [7] FAIL: PAGE left %u lines\n", forthConsoleLineCount());
    fail = 1;
  }
  if (!forthCapIsOpen()) {
    printf("    [7] FAIL: PAGE must not close the capture\n");
    fail = 1;
  }

  /* [8] EXIT closes; reopening RESTORES the dialogue (N-R2: BSS, not the
   * capture's lifetime).  Something must be in the ring first. */
  _consoleEnterLine("XEQ 'CLSTK' 9 9 +");
  { uint16_t before = forthConsoleLineCount();
    int press;
    /* The ladder unwinds one level per press and a long session can leave a
     * level or two up, so the story presses until it closes rather than
     * asserting a press count — rung-by-rung is test 26's contract, not the
     * story's. */
    for (press = 0; press < 4 && forthCapIsOpen(); press++) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) {
      printf("    [8] FAIL: EXIT did not close the capture within four presses\n");
      fail = 1;
    }
    if (forthConsoleLineCount() != before) {
      printf("    [8] FAIL: capture close must NOT touch the dialogue (%u -> %u lines)\n",
             before, forthConsoleLineCount());
      fail = 1;
    }
    fnForthOuter(NOPARAM);
    if (forthConsoleLineCount() != before) {
      printf("    [8] FAIL: reopening must RESTORE the dialogue (%u lines)\n",
             forthConsoleLineCount());
      fail = 1;
    }
  }

  /* [9] The power-reset seam: the VIEW dies, FHIST still recalls.  This is
   * the designed divergence N-R2 names — the view is BSS and never
   * persisted; the input lines persist because FHIST is a program. */
  forthCapClose();
  forthDictInit();                       /* the dictionary-lifecycle seam */
  if (forthConsoleLineCount() != 0) {
    printf("    [9] FAIL: the power-reset seam must clear the view (%u lines)\n",
           forthConsoleLineCount());
    fail = 1;
  }
  { char recalled[FORTH_CONSOLE_FMT_MAX];
    forthCapOpenInteractive();
    aimBuffer[0] = 0;
    forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);
    forthHistoryRecall(-1);
    xcopy(recalled, aimBuffer, (uint32_t)stringByteLength(aimBuffer) + 1);
    if (stringByteLength(recalled) == 0) {
      printf("    [9] FAIL: FHIST must still recall after the view is gone\n");
      fail = 1;
    }
  }

  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: story — open keys-first, compute, define GLOBAL in alpha, print,"
           " roll and snap, PAGE, EXIT/reopen keeps the dialogue, power reset clears it\n");
  }
  return fail;
}

/* ---- 28: ONE HISTORY.  Every line the transcript shows prompt-prefixed is
 * byte-equal to the line FHIST recalls, in the same order.  The one-act echo
 * is what makes this true, and a second echo writer or a reorder against the
 * push would break exactly this. ---- */
static int test_console_one_history(void)
{
  int fail = 0;
  const char *lines[3] = { "XEQ 'CLSTK'", "11 22 +", "3 4 *" };
  int i;

  N13_RESET();
  forthDictInit();
  fnForthOuter(NOPARAM);

  for (i = 0; i < 3; i++) { _consoleEnterLine(lines[i]); }

  /* Walk the transcript's prompt-prefixed lines newest-first and compare each
   * against what f-shift recall returns, newest-first.  Same lines, same
   * order, byte for byte. */
  { uint16_t n = forthConsoleLineCount();
    uint16_t seen = 0;
    uint16_t idx;
    forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);
    for (idx = 0; idx < n && seen < 3; idx++) {
      char shown[FORTH_CONSOLE_FMT_MAX], recalled[FORTH_CONSOLE_FMT_MAX];
      char want[FORTH_CONSOLE_FMT_MAX];
      forthConsoleLineAt(idx, shown, sizeof(shown));
      if (compareString(shown, STD_RIGHT_DOUBLE_ANGLE " ", CMP_BINARY) == 0) { continue; }
      if (stringByteLength(shown) < 3
          || memcmp(shown, STD_RIGHT_DOUBLE_ANGLE " ", 3) != 0) {
        continue;                                  /* an output line, not an echo */
      }
      aimBuffer[0] = 0;
      forthHistoryRecall(-1);
      xcopy(recalled, aimBuffer, (uint32_t)stringByteLength(aimBuffer) + 1);
      snprintf(want, sizeof(want), "%s", shown + 3);   /* strip the prompt */
      if (compareString(recalled, want, CMP_BINARY) != 0) {
        printf("    FAIL: transcript line %u shows \"%s\" but history recalls \"%s\""
               " — the rolled lines and the old history must be the SAME history\n",
               idx, want, recalled);
        fail = 1;
      }
      seen++;
    }
    if (seen != 3) {
      printf("    FAIL: expected 3 prompt-prefixed lines in the transcript, found %u\n", seen);
      fail = 1;
    }
  }

  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: one history — every echoed line is byte-equal to what FHIST recalls\n");
  }
  return fail;
}

/* ---- 29: AUDIT C1 class test — the formatter must satisfy every producer's
 * BUFFER CONTRACT, not just its calling convention.
 *
 * Bug class: a display.c producer that uses the caller's buffer as scratch
 * beyond the text it returns.  shortIntegerToDisplayString is the known
 * member — it builds digits from displayString[ERROR_MESSAGE_LENGTH / 2]
 * upward and compacts them to the front — and handing it a 256-byte local
 * put its first write one past the end of the frame.
 *
 * The class is ENUMERABLE (every arm of forthConsoleFormatRegister's switch),
 * so this drives all of them through a canary-guarded buffer.  A producer
 * that scribbles past what it was given trips the canary regardless of what
 * the returned text looks like. ---- */
static int test_console_format_buffer_contract(void)
{
  /* The formatter's out buffer, wrapped in canaries.  Sized exactly as its
   * callers size theirs (FORTH_CONSOLE_FMT_MAX), because the defect was a
   * function of that size. */
  struct { uint32_t front; char out[FORTH_CONSOLE_FMT_MAX]; uint32_t back; } g;
  int fail = 0;
  uint8_t types[4];
  int n = 0, i;

  forthDictInit();
  forthConsoleClear();

  types[n++] = dtLongInteger;
  types[n++] = dtReal34;
  types[n++] = dtShortInteger;
  types[n++] = dtString;

  for (i = 0; i < n; i++) {
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret("XEQ 'CLSTK'");
    lastErrorCode = ERROR_NONE;

    switch (types[i]) {
      case dtLongInteger:  forthPushInt32(12345);                     break;
      case dtReal34:       forthOuterInterpret("1.5");                break;
      /* The C1 case, built directly and at its WIDEST: a full 64-bit value in
       * base 2 is the longest thing this producer can render, and it is the
       * rendering that runs furthest past the buffer.  Built rather than
       * typed because the first version of this test used
       * `255 XEQ 'HEX'` — HEX is a CAT_FNCT item reached by BARE NAME, so
       * that line raised label-not-found, X stayed a long integer, and the
       * short-integer arm was never reached.  The test passed the C1
       * mutation.  Hence the type assertion below. */
      case dtShortInteger:
        convertUInt64ToShortIntegerRegister(0, (uint64_t)0xFFFFFFFFFFFFFFFFull,
                                            2, REGISTER_X);
        break;
      case dtString:       x_set_string("A STRING");                  break;
      default: break;
    }
    lastErrorCode = ERROR_NONE;

    /* THE FIXTURE MUST PROVE IT REACHED THE STATE IT CLAIMS TO TEST.
     * Without this, a fixture that quietly fails to build the type turns the
     * whole subcase into a check of the long-integer arm wearing another
     * arm's name — which is exactly how the first version of this test
     * survived the defect it exists to catch. */
    if (getRegisterDataType(REGISTER_X) != types[i]) {
      printf("    FAIL: fixture — X is type %u, expected %u; this subcase did"
             " NOT exercise the arm it names\n",
             (unsigned)getRegisterDataType(REGISTER_X), (unsigned)types[i]);
      fail = 1;
      continue;
    }

    g.front = 0xA5A5A5A5u;
    g.back  = 0x5A5A5A5Au;
    memset(g.out, 0, sizeof(g.out));

    forthConsoleFormatRegister(REGISTER_X, g.out, (int16_t)sizeof(g.out));

    if (g.back != 0x5A5A5A5Au) {
      printf("    FAIL: type %u — the producer wrote PAST the %u-byte buffer"
             " (back canary %08X). Its buffer contract is not satisfied.\n",
             (unsigned)types[i], (unsigned)FORTH_CONSOLE_FMT_MAX, g.back);
      fail = 1;
    }
    if (g.front != 0xA5A5A5A5u) {
      printf("    FAIL: type %u — the producer wrote BEFORE the buffer"
             " (front canary %08X)\n", (unsigned)types[i], g.front);
      fail = 1;
    }
    if (stringByteLength(g.out) >= (int32_t)sizeof(g.out)) {
      printf("    FAIL: type %u — result not NUL-terminated inside the buffer\n",
             (unsigned)types[i]);
      fail = 1;
    }
  }

  /* And the reproducer end to end: the C1 gesture through the real dialogue. */
  N13_RESET();
  forthCapOpenInteractive();
  calcMode = CM_AIM;
  convertUInt64ToShortIntegerRegister(0, (uint64_t)0xFFFFFFFFFFFFFFFFull, 2, REGISTER_X);
  _consoleEnterLine("DUP DROP");
  { char line[FORTH_CONSOLE_FMT_MAX];
    forthConsoleLineAt(0, line, sizeof(line));
    if (stringByteLength(line) == 0) {
      printf("    FAIL: the C1 gesture produced no X echo\n");
      fail = 1;
    }
  }
  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;

  if (!fail) {
    printf("    PASS: every formatter arm stays inside the caller's buffer;"
           " the C1 gesture echoes cleanly\n");
  }
  return fail;
}

/* ---- 30: AUDIT C2/C3/C4/C8/C9 class test — FRAME CONSERVATION.
 *
 * Bug class: "the console changes the softmenu stack and does not put it
 * back". All five findings were instances, at five different sites, because
 * five sites each managed the console's row on their own.
 *
 * The invariant that kills the class: a console session that ends in EXIT
 * leaves the softmenu stack EXACTLY as it found it — same frames, same
 * order, same page. Byte-compared, not depth-compared: a leaked frame, an
 * eaten frame and a swapped frame are all failures and only a byte compare
 * catches the third.
 *
 * Driven over the paths that reach the five sites, since the invariant is
 * only as good as the paths it is checked on. ---- */
static int test_console_frame_conservation(void)
{
  softmenuStack_t before[SOFTMENU_STACK_SIZE];
  int fail = 0, i;

  /* Each row: a name, and what the session does between open and EXIT.
   *
   * Rows 7-11 are the AUDIT C17 class rows: the owner's row is one of the
   * console's OWN two rows — FWRD reached through the CATALOG tree (the
   * forth_capture.h state the old homePushed comment named verbatim), and
   * separately ALPHA — crossed with the alpha toggle and with a
   * calcModeNormal()-calling line.  The bug class is "ownership inferred
   * from a value two different owners can hold": every pre-C17 ownership
   * test asked `menu == -MNU_FORTH/-MNU_ALPHA`, the owner's own frame
   * answered "ours", and a CLSTK line then consumed it — slot 0 identity
   * broken while the frame COUNT stayed conserved, which is why rows 0-6
   * never saw it.  The pair of rows the landed battery's comment below
   * concedes it delegated to the M1-1 [8] fixture — which never toggles and
   * never runs a line. */
  for (i = 0; i < 12; i++) {
    const char *what = "";
    int presses = 0;

    N13_RESET();
    forthDictInit();
    /* A menu of the owner's, so there is something real to conserve.
     * Rows 7-10 make the owner's own FWRD frame slot 0 (CATALOG tree
     * underneath, exactly the browse-then-open gesture); row 11 makes it
     * the raw ALPHA row. */
    if (i >= 7 && i <= 10) {
      showSoftmenu(-MNU_CATALOG);
      showSoftmenu(-MNU_FORTH);
      if (currentMenu() != -MNU_FORTH) {
        printf("    FIXTURE FAIL: [row %d] owner's own FWRD not on top (menu %d)\n",
               i, currentMenu());
        fail = 1;
        continue;
      }
    }
    else if (i == 11) {
      showSoftmenu(-MNU_STK);
      showSoftmenu(-MNU_ALPHA);
      if (currentMenu() != -MNU_ALPHA) {
        printf("    FIXTURE FAIL: [row %d] owner's own ALPHA not on top (menu %d)\n",
               i, currentMenu());
        fail = 1;
        continue;
      }
    }
    else {
      showSoftmenu(-MNU_STK);
    }
    xcopy(before, softmenuStack, sizeof(before));

    fnForthOuter(NOPARAM);

    switch (i) {
      case 0: what = "open, EXIT";
        break;
      case 1: what = "open, toggle to alpha, EXIT";        /* C9 */
        runFunction(ITM_AIM);
        break;
      case 2: what = "open, toggle alpha then back, EXIT"; /* C2 */
        runFunction(ITM_AIM);
        runFunction(ITM_AIM);
        break;
      case 3: what = "open, ENTER a line, EXIT";           /* C3 */
        _consoleEnterLine("XEQ 'CLSTK' 1 2 +");
        break;
      case 4: what = "open, alpha, ENTER, EXIT";           /* C4 */
        runFunction(ITM_AIM);
        _consoleEnterLine("XEQ 'CLSTK' 3 4 +");
        break;
      case 5: what = "open, stack a menu, EXIT";           /* C8 */
        showSoftmenu(-MNU_FIN);
        break;
      /* Found by the out-of-family reader (Gemini, 2026-08-06), not by the
       * in-family audit and not by the five fixtures above: the restore path
       * only inspects the TOP row, so with a user menu stacked it concludes
       * the console's frame is gone and pushes a SECOND one.  Needs both a
       * stacked menu AND a line that calls calcModeNormal() — CLSTK does —
       * which is the combination no single-purpose fixture had. */
      case 6: what = "open, stack a menu, ENTER a CLSTK line, EXIT";
        showSoftmenu(-MNU_FIN);
        _consoleEnterLine("XEQ 'CLSTK'");
        break;
      /* ---- AUDIT C17 class rows ---- */
      case 7: what = "own FWRD: open, ENTER a plain line, EXIT";
        _consoleEnterLine("1 2 +");
        break;
      case 8: what = "own FWRD: open, ENTER a CLSTK line, EXIT";
        _consoleEnterLine("XEQ 'CLSTK'");
        break;
      case 9: what = "own FWRD: open, toggle alpha then back, EXIT";
        runFunction(ITM_AIM);
        runFunction(ITM_AIM);
        break;
      case 10: what = "own FWRD: open, toggle alpha, ENTER a CLSTK line, EXIT";
        runFunction(ITM_AIM);
        _consoleEnterLine("XEQ 'CLSTK'");
        break;
      case 11: what = "own ALPHA: open, toggle alpha, ENTER a plain line, EXIT";
        runFunction(ITM_AIM);
        _consoleEnterLine("3 4 +");
        break;
      default: break;
    }

    /* Unwind: the ladder takes one level per press, so press until closed
     * rather than assuming a count. Four is generous; not closing IS a
     * failure and is reported as one. */
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) {
      fnKeyExit(NOPARAM);
    }
    if (forthCapIsOpen()) {
      printf("    FAIL: [%s] did not close within six EXIT presses\n", what);
      fail = 1;
      forthCapClose();
      continue;
    }

    /* Compare frame IDENTITY (which menu, which user menu, which page) across
     * every slot — not the raw struct.  softmenuStack_t also carries a
     * `calcMode` bookkeeping field that pushSoftmenu stamps with the mode in
     * force at the time, so a byte compare reports a difference for a stack
     * that was perfectly restored. */
    { int slot, differs = 0;
      /* SLOT 0 — the row the owner is looking at.  That is the invariant all
       * five findings violated, and it is what the owner experiences: press
       * FORTH, work, press EXIT, be back where you were.
       *
       * Deeper slots are deliberately NOT compared.  pushSoftmenu reorders the
       * stack whenever a menu already on it is pushed again
       * (softmenus.c:3671-3683) — upstream behaviour that happens to any menu
       * visited twice, which the console did not invent and must not be held
       * to.  The leak check below covers what that would otherwise miss. */
      for (slot = 0; slot < 1; slot++) {
        if (before[slot].softmenuId != softmenuStack[slot].softmenuId
            || before[slot].userMenuId != softmenuStack[slot].userMenuId
            || before[slot].firstItem != softmenuStack[slot].firstItem) {
          differs = 1;
          break;
        }
      }
      if (differs) {
        printf("    FAIL: [%s] did not restore the owner's row — top is now %d,"
               " the owner had %d\n", what, currentMenu(),
               softmenu[before[0].softmenuId].menuItem);
        fail = 1;
      }
    }

    /* And nothing of the console's may be LEFT BEHIND anywhere on the stack.
     * Slot 0 alone would miss a frame leaked into slot 1+, which is how C8's
     * stayInAIM push and C9's double row hid: the owner's menu looked right
     * and an extra console row sat underneath it, surfacing on the next EXIT.
     * The DIFFERENTIAL form below is what makes this sound for the C17 rows
     * too, whose owner rows are themselves FWRD/ALPHA. */
    { int slot, wasCount = 0, nowCount = 0;
      for (slot = 0; slot < SOFTMENU_STACK_SIZE; slot++) {
        int16_t m0 = softmenu[before[slot].softmenuId].menuItem;
        int16_t m1 = softmenu[softmenuStack[slot].softmenuId].menuItem;
        if (m0 == -MNU_FORTH || m0 == -MNU_ALPHA) { wasCount++; }
        if (m1 == -MNU_FORTH || m1 == -MNU_ALPHA) { nowCount++; }
      }
      /* DIFFERENTIAL: the stack may already carry alpha rows the owner put
       * there, or that earlier batteries left. What must not happen is the
       * session ADDING one. */
      if (nowCount > wasCount) {
        printf("    FAIL: [%s] leaked %d console row(s) onto the stack"
               " (%d before, %d after)\n", what, nowCount - wasCount,
               wasCount, nowCount);
        fail = 1;
      }
    }
  }

  N13_RESET();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: twelve console sessions, each restores the owner's row and leaks no frame\n");
  }
  return fail;
}

/* ---- 31: AUDIT round 2 — the transient capture bits survive BOTH re-open
 * sites, not just the REPL one.
 *
 * forthCapOpenInteractive()/forthCapOpen() zero keysMode and origin by
 * design, and every path that re-opens an ALREADY-LIVE capture has to put
 * them back. There are two such paths — the REPL reopen after ENTER, and
 * forthCaptureResume() after a fold — and round 1's fix closed only the
 * first. Five of the eight round-2 readers found the second independently.
 *
 * Enumerated rather than sampled: the class is "a field that rides the
 * capture across a re-open".  homePushed left the class with C17: frame
 * ownership now lives in the softmenu frame itself (forth_menu.c's stamp),
 * which no capture re-open can touch — its reopen coverage moved into the
 * frame-conservation battery's line-running rows, which fail if ownership
 * is forgotten across the REPL reopen. ---- */
static int test_console_capture_bits_survive_reopen(void)
{
  int fail = 0;

  /* The REPL reopen (round 1's site). */
  N13_RESET();
  forthDictInit();
  fnForthOuter(NOPARAM);
  forthCapSetKeysMode(true);
  _consoleEnterLine("XEQ 'CLSTK' 1 2 +");
  if (!forthCapKeysMode()) {
    printf("    FAIL: REPL reopen dropped keysMode\n");
    fail = 1;
  }
  if (!forthCapIsInteractive()) {
    printf("    FAIL: REPL reopen dropped the interactive origin\n");
    fail = 1;
  }
  forthCapClose();

  /* THE SECOND SITE — forthCaptureResume() after a fold — IS NOT COVERED HERE,
   * and saying so is the point.
   *
   * The first draft of this test "covered" it by performing the save/restore
   * in the test body and then asserting the bits survived. That asserts the
   * TEST's copy of the block, not production's: reverting the production fix
   * left the suite green. Third vacuous test of this session, same class as
   * the two before it, caught the same way — by running the mutation.
   *
   * Reaching the real path needs a suspended capture on a real program step
   * (the L1-F fold fixture), which is a TAM-driven battery rather than a
   * console one. Until that exists, the resume site is held by the fix and by
   * five independent readers agreeing on it, NOT by a test. Recorded as a gap
   * rather than papered over with an assertion that cannot fail. */

  forthCapClose();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: keysMode and origin survive the REPL reopen"
           " (the resume site is a documented gap — see the comment)\n");
  }
  return fail;
}

/* ---- 32: AUDIT C18 class test — the row and the sub-mode never disagree.
 *
 * Bug class: "a state change committed by the caller and the display of that
 * state established by a callee that may decline."  K-R3's rule is that the
 * underlying row IS the mode indicator; the round-2 report's invariant is
 * asserted at every keysMode writer, over every overlay state:
 * (keysMode, base row) is one of the two legal pairs, OR the sub-mode did
 * not move.  The disjunction is the report's own — refusing the flip is as
 * valid as forcing the row — and each case below pins WHICH disposition the
 * landed fix chose, so a regression to the third option (flip committed,
 * row untouched) cannot hide in the disjunction.
 *
 * Overlay states: none, an alpha submenu (Greek keypad), a non-alpha menu
 * (STK).  Gestures: the E10/E11 toggle, EXIT, and ENTER's REPL reopen. ---- */
static int test_console_submode_row_agreement(void)
{
  int fail = 0, o, presses;
  const int16_t overlays[3] = { 0, -MNU_ALPHA_OMEGA, -MNU_STK };
  const char *oname[3] = { "no overlay", "Greek submenu", "STK" };

  for (o = 0; o < 3; o++) {
    /* --- gesture 1: the toggle --- */
    N13_RESET();
    forthDictInit();
    showSoftmenu(-MNU_STK);                 /* something of the owner's */
    fnForthOuter(NOPARAM);
    if (overlays[o] != 0) {
      showSoftmenu(overlays[o]);
      if (currentMenu() != overlays[o]) {
        printf("    FIXTURE FAIL: [toggle, %s] overlay did not stack\n", oname[o]);
        fail = 1;
        continue;
      }
    }
    { bool_t keysBefore = forthCapKeysMode();
      runFunction(ITM_AIM);
      if (overlays[o] == 0) {
        if (forthCapKeysMode() == keysBefore || currentMenu() != -MNU_ALPHA) {
          printf("    FAIL: [toggle, %s] flip must land with the row following"
                 " (keys=%d, menu %d)\n", oname[o],
                 (int)forthCapKeysMode(), currentMenu());
          fail = 1;
        }
      }
      else {
        if (forthCapKeysMode() != keysBefore || currentMenu() != overlays[o]) {
          printf("    FAIL: [toggle, %s] with an overlay the flip must be"
                 " REFUSED with the row unmoved (keys %d->%d, menu %d)\n",
                 oname[o], (int)keysBefore, (int)forthCapKeysMode(),
                 currentMenu());
          fail = 1;
        }
      }
    }
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) {
      printf("    FAIL: [toggle, %s] did not close within six EXIT presses\n", oname[o]);
      fail = 1;
      forthCapClose();
    }

    /* --- gesture 2: EXIT from the alpha excursion --- */
    N13_RESET();
    forthDictInit();
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    runFunction(ITM_AIM);                    /* enter the excursion, base on top */
    if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
      printf("    FIXTURE FAIL: [EXIT, %s] excursion entry did not take\n", oname[o]);
      fail = 1;
      forthCapClose();
      continue;
    }
    if (overlays[o] != 0) { showSoftmenu(overlays[o]); }
    fnKeyExit(NOPARAM);
    if (overlays[o] == 0) {
      if (!forthCapKeysMode() || currentMenu() != -MNU_FORTH) {
        printf("    FAIL: [EXIT, %s] must unwind the excursion to keys+FWRD"
               " (keys=%d, menu %d)\n", oname[o],
               (int)forthCapKeysMode(), currentMenu());
        fail = 1;
      }
    }
    else {
      /* The overlay rung runs FIRST: the stacked row pops, the sub-mode
       * does NOT move, and the base beneath is the excursion's own row —
       * the legal (alpha, ALPHA) pair.  The old ladder flipped first and
       * left the Greek keypad up while the keys plane typed Σ+. */
      if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
        printf("    FAIL: [EXIT, %s] must pop the overlay first, sub-mode"
               " unmoved (keys=%d, menu %d)\n", oname[o],
               (int)forthCapKeysMode(), currentMenu());
        fail = 1;
      }
    }
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) { forthCapClose(); fail = 1; }

    /* --- gesture 3: ENTER's REPL reopen --- */
    N13_RESET();
    forthDictInit();
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    runFunction(ITM_AIM);
    if (forthCapKeysMode() || currentMenu() != -MNU_ALPHA) {
      printf("    FIXTURE FAIL: [ENTER, %s] excursion entry did not take\n", oname[o]);
      fail = 1;
      forthCapClose();
      continue;
    }
    { int slot, forthBefore = 0, alphaBefore = 0;
      if (overlays[o] != 0) { showSoftmenu(overlays[o]); }
      /* DIFFERENTIAL, like the frame-conservation battery: the stack may
       * carry FWRD/ALPHA residue from earlier batteries, so the buried
       * retarget is pinned as a delta, not as an absolute census. */
      for (slot = 0; slot < SOFTMENU_STACK_SIZE; slot++) {
        int16_t m = menu((uint8_t)slot);
        if (m == -MNU_FORTH) { forthBefore++; }
        if (m == -MNU_ALPHA) { alphaBefore++; }
      }
      _consoleEnterLine("1 2 +");
      if (!forthCapKeysMode()) {
        printf("    FAIL: [ENTER, %s] keys-first must survive every ENTER (N-R6)\n",
               oname[o]);
        fail = 1;
      }
      if (overlays[o] == 0) {
        if (currentMenu() != -MNU_FORTH) {
          printf("    FAIL: [ENTER, %s] reopen must show the FWRD home row"
                 " (menu %d)\n", oname[o], currentMenu());
          fail = 1;
        }
      }
      else {
        /* The reopen must not eat the user's overlay — but the BASE beneath
         * must be truthful for the moment the overlay pops: the excursion's
         * ALPHA became FWRD in place.  This is the buried retarget,
         * observable without stamp access. */
        int forthAfter = 0, alphaAfter = 0;
        if (currentMenu() != overlays[o]) {
          printf("    FAIL: [ENTER, %s] reopen must leave the user's overlay up"
                 " (menu %d)\n", oname[o], currentMenu());
          fail = 1;
        }
        for (slot = 0; slot < SOFTMENU_STACK_SIZE; slot++) {
          int16_t m = menu((uint8_t)slot);
          if (m == -MNU_FORTH) { forthAfter++; }
          if (m == -MNU_ALPHA) { alphaAfter++; }
        }
        if (forthAfter != forthBefore + 1 || alphaAfter != alphaBefore - 1) {
          printf("    FAIL: [ENTER, %s] the base beneath the overlay must be"
                 " retargeted to FWRD (FWRD %d->%d, ALPHA %d->%d)\n",
                 oname[o], forthBefore, forthAfter, alphaBefore, alphaAfter);
          fail = 1;
        }
      }
    }
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) { forthCapClose(); fail = 1; }
  }

  N13_RESET();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: nine gesture/overlay pairs — the row and the sub-mode"
           " never disagree, refusals leave both unmoved\n");
  }
  return fail;
}

/* ---- 33: AUDIT C19 class test — every post-run arm re-closes the ring.
 *
 * Bug class: "two arms of one post-condition, one of which re-establishes an
 * invariant the other assumes."  The invariant: no exit path from
 * forthInteractiveEnter's run leaves the ring's open record open.  Driven
 * over the post-run dispositions; the E9-refusal disposition is pinned by
 * test_console_dialogue_refusal (writes nothing at all) and not repeated
 * here.  The load-bearing row is "error AFTER output": `1 . BOGUS` really
 * does print before it raises, and the error message must land as its OWN
 * record, not inside the word's open output row where the renderer's
 * ellipsis can eat it. ---- */
static int test_console_error_echo_closes_output(void)
{
  int fail = 0;
  uint16_t before;
  char l0[FORTH_CONSOLE_FMT_MAX], l1[FORTH_CONSOLE_FMT_MAX];

  N13_RESET();
  forthDictInit();
  fnForthOuter(NOPARAM);

  /* Disposition: clean line, no output — echo + X echo, closed. */
  before = forthConsoleLineCount();
  _consoleEnterLine("1 2 +");
  if (forthConsoleLineCount() != before + 2 || forthConsoleHasOpenLine()) {
    printf("    FAIL: [clean, no output] expected +2 closed records, got +%d"
           " (open=%d)\n", forthConsoleLineCount() - before,
           (int)forthConsoleHasOpenLine());
    fail = 1;
  }

  /* Disposition: clean line WITH output — echo + the word's output, closed
   * by the success arm. */
  before = forthConsoleLineCount();
  _consoleEnterLine("1 .");
  if (forthConsoleLineCount() != before + 2 || forthConsoleHasOpenLine()) {
    printf("    FAIL: [clean, output] expected +2 closed records, got +%d"
           " (open=%d)\n", forthConsoleLineCount() - before,
           (int)forthConsoleHasOpenLine());
    fail = 1;
  }

  /* Disposition: error, no output — echo + message. */
  before = forthConsoleLineCount();
  _consoleEnterLine("NOSUCHWORD");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FIXTURE FAIL: [error, no output] NOSUCHWORD did not raise\n");
    fail = 1;
  }
  else {
    const char *msg = errorMessageOf(lastErrorCode);
    forthConsoleLineAt(0, l0, sizeof(l0));
    if (forthConsoleLineCount() != before + 2 || forthConsoleHasOpenLine()
        || compareString(l0, (char *)msg, CMP_BINARY) != 0) {
      printf("    FAIL: [error, no output] expected +2 records ending in the"
             " message, got +%d, last \"%s\"\n",
             forthConsoleLineCount() - before, l0);
      fail = 1;
    }
  }
  lastErrorCode = ERROR_NONE;
  _consoleEnterLine("");            /* not needed for state; keep the REPL shape */

  /* THE C19 ROW — error AFTER output.  `.` leaves its record open by
   * design; the error arm must close it and write the message as its own
   * record: echo + "1 " + message = +3, and the last two records are
   * distinct. */
  before = forthConsoleLineCount();
  _consoleEnterLine("1 . BOGUS");
  if (lastErrorCode == ERROR_NONE) {
    printf("    FIXTURE FAIL: [error after output] BOGUS did not raise\n");
    fail = 1;
  }
  else {
    const char *msg = errorMessageOf(lastErrorCode);
    forthConsoleLineAt(0, l0, sizeof(l0));
    forthConsoleLineAt(1, l1, sizeof(l1));
    if (forthConsoleLineCount() != before + 3) {
      printf("    FAIL: [error after output] expected +3 records (echo,"
             " output, message), got +%d — the message merged into the"
             " word's open output row\n", forthConsoleLineCount() - before);
      fail = 1;
    }
    else if (compareString(l0, (char *)msg, CMP_BINARY) != 0) {
      printf("    FAIL: [error after output] last record is \"%s\", expected"
             " the bare message\n", l0);
      fail = 1;
    }
    else if (stringByteLength(l1) == 0 || l1[0] != '1') {
      printf("    FAIL: [error after output] second-last record is \"%s\","
             " expected the word's own output\n", l1);
      fail = 1;
    }
    if (forthConsoleHasOpenLine()) {
      printf("    FAIL: [error after output] the ring is still open\n");
      fail = 1;
    }
  }

  lastErrorCode = ERROR_NONE;
  { int presses;
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
    if (forthCapIsOpen()) { forthCapClose(); fail = 1; }
  }
  N13_RESET();
  forthConsoleClear();
  if (!fail) {
    printf("    PASS: every post-run arm closes the ring; the error after"
           " output lands as its own record\n");
  }
  return fail;
}

/* ---- 34: AUDIT round 3 class test — a frame stamp never outlives its
 * capture.
 *
 * Bug class: "ownership state reachable by a close path that does not pass
 * the funnel that clears it."  C17 moved ownership OUT of the capture object
 * and INTO softmenuStack[].userMenuId, which bought the reopen/resume
 * survival the old homePushed bit could not have — and cost this: the frame
 * array is PERSISTED wholesale (saveRestoreBackup.c), and two paths reach a
 * closed capture without calling forthCapClose().
 *
 * Enumerated, because the class is enumerable: every way a capture can end.
 * The invariant asserted after each: NO stamp anywhere on the stack.
 *
 * Oracle: forthConsoleStampOnStack(), which asks the question directly.
 * The first draft of this test used forthConsoleBaseOnTop() and reported
 * three FALSE failures: that predicate falls back to a menu-IDENTITY test
 * when no stamp exists, and a non-ladder close legitimately leaves the
 * console's FWRD row standing — so it answered "true" for a stack with no
 * stamp on it at all.  Fifth fixture of this stage caught by the C22 rule,
 * and the first where the wrong oracle manufactured failures rather than
 * hiding them. ---- */
static int test_console_stamp_never_outlives_capture(void)
{
  int fail = 0;

  /* (a) the ordinary EXIT close. */
  N13_RESET();
  forthDictInit();
  showSoftmenu(-MNU_STK);
  fnForthOuter(NOPARAM);
  { int presses;
    for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
  }
  if (forthConsoleStampOnStack()) {
    printf("    FAIL: [EXIT close] a stamp survived the close funnel\n");
    fail = 1;
  }

  /* (b) a close through the funnel that is NOT the EXIT ladder — the shape
   * every non-ladder key takes (_forthCapCloseIfInteractive). */
  N13_RESET();
  forthDictInit();
  showSoftmenu(-MNU_STK);
  fnForthOuter(NOPARAM);
  forthCapClose();
  if (forthConsoleStampOnStack()) {
    printf("    FAIL: [funnel close] a stamp survived forthCapClose()\n");
    fail = 1;
  }

  /* (c) an ABANDONED suspension — reached standalone by forthCaptureResume's
   * canary arm, and it does NOT call forthCapClose(). */
  N13_RESET();
  forthDictInit();
  showSoftmenu(-MNU_STK);
  fnForthOuter(NOPARAM);
  forthCapSuspendState(0, 0, 0, 0);            /* OPEN -> SUSPENDED */
  if (!forthCapIsSuspended()) {
    printf("    FIXTURE FAIL: [abandon] the capture did not suspend\n");
    fail = 1;
  }
  forthCapAbandonSuspended();
  if (forthCapIsOpen() || forthCapIsSuspended()) {
    printf("    FIXTURE FAIL: [abandon] the capture did not close\n");
    fail = 1;
  }
  if (forthConsoleStampOnStack()) {
    printf("    FAIL: [abandon] a stamp survived forthCapAbandonSuspended()"
           " — that close path bypasses the funnel\n");
    fail = 1;
  }

  /* (d) THE PERSISTENCE ROW.  softmenuStack is saved and restored as a raw
   * hex dump, so a backup taken with the console open carries the stamps
   * into the file and the restore writes them back — AFTER the seam whose
   * unstamp was supposed to clear them.  Simulated exactly as the restore
   * does it: capture reset first (the dict-lifecycle seam), then the stack
   * overwritten from the "file" image, then the sanitizer.
   *
   * The consequence this pins is the one that bites the NEXT session: with a
   * stale stamp present, forthConsoleRegisterSlot0 declines, so the next
   * open registers nothing and its EXIT reads ownership off a dead capture. */
  { softmenuStack_t image[SOFTMENU_STACK_SIZE];
    N13_RESET();
    forthDictInit();
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);                     /* console open, frame stamped */
    xcopy(image, softmenuStack, sizeof(image));/* <- what saveCalc writes */

    forthCapPowerReset();                      /* the seam at :975-976 */
    xcopy(softmenuStack, image, sizeof(image));/* <- restoreStateValue at :986 */
    forthCaptureSanitizeRestoredUi();          /* the seam at :1496 */

    if (forthCapIsOpen()) {
      printf("    FIXTURE FAIL: [restore] the capture must be closed after"
             " the reset\n");
      fail = 1;
    }
    if (forthConsoleStampOnStack()) {
      printf("    FAIL: [restore] a stamp came back from the state file and"
             " outlived its capture — it must be cleared AFTER the stack is"
             " restored, not before\n");
      fail = 1;
    }
    /* And the session after the restore must own its own frame. */
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    if (!forthConsoleOwnsSlot0()) {
      printf("    FAIL: [restore] the session after a restore did not register"
             " its frame — a stale stamp made the open decline\n");
      fail = 1;
    }
    { int presses;
      for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
    }
  }

  N13_RESET();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: four capture endings — EXIT, funnel, abandoned"
           " suspension, save/restore — none leaves a stamp behind\n");
  }
  return fail;
}

/* ---- 35: AUDIT round 3 class test — a line that destroys the console's row
 * WITHOUT leaving CM_AIM is still repaired.
 *
 * Bug class: "two repairs sharing one guard, where the guard belongs to only
 * one of them."  The post-run block repaired the MODE and the SURFACE under
 * a single `calcMode != CM_AIM` test, which conflated "the line left the AIM
 * surface" with "the line damaged the console's row".  `EXITALL` does the
 * second without the first: CAT_FNCT/PTP_NONE, so a typed line resolves and
 * runs it, and it pops every frame down to MyMenu without touching calcMode.
 *
 * Driven as a differential against `CLSTK`, which damages the row via
 * calcModeNormal() and DOES leave CM_AIM — the case the landed battery
 * already covered. Both must end with the console owning a row again. ---- */
static int test_console_surface_repair_ungated(void)
{
  int fail = 0;
  int i;

  for (i = 0; i < 2; i++) {
    const char *what = (i == 0) ? "XEQ 'CLSTK' (leaves CM_AIM)"
                                : "EXITALL (stays in CM_AIM)";
    N13_RESET();
    forthDictInit();
    showSoftmenu(-MNU_STK);
    fnForthOuter(NOPARAM);
    if (!forthConsoleOwnsSlot0()) {
      printf("    FIXTURE FAIL: [%s] the open did not register a frame\n", what);
      fail = 1;
      forthCapClose();
      continue;
    }

    _consoleEnterLine((i == 0) ? "XEQ 'CLSTK'" : "EXITALL");

    if (!forthCapIsOpen()) {
      printf("    FIXTURE FAIL: [%s] the capture did not survive the line\n", what);
      fail = 1;
      continue;
    }
    if (calcMode != CM_AIM) {
      printf("    FAIL: [%s] the line left the console off the AIM surface\n", what);
      fail = 1;
    }
    /* The repair: the console has a row again, and it is one it owns or
     * borrows — not whatever the pops happened to reveal. */
    if (!forthConsoleBaseOnTop()) {
      printf("    FAIL: [%s] the console's surface was not re-established"
             " (menu %d) — the repair was skipped\n", what, currentMenu());
      fail = 1;
    }
    if (currentMenu() != -MNU_FORTH) {
      printf("    FAIL: [%s] the restored row must be FWRD in keys mode"
             " (menu %d)\n", what, currentMenu());
      fail = 1;
    }

    { int presses;
      for (presses = 0; presses < 6 && forthCapIsOpen(); presses++) { fnKeyExit(NOPARAM); }
      if (forthCapIsOpen()) { forthCapClose(); fail = 1; }
    }
  }

  N13_RESET();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: both a mode-leaving and a mode-preserving destructive"
           " line leave the console owning its row\n");
  }
  return fail;
}

/* ---- 36: AUDIT round 4 class test — the OWNERSHIP INVARIANT itself.
 *
 * Bug class: "an invariant that lives only in prose."  The banner claimed
 * "exactly one frame is registered" for a whole session; it was false, and
 * three independent readers caught it before any test did — because no test
 * asserted it.  Prose is not enforcement.
 *
 * The invariant, as the code actually needs it:
 *   - at most ONE owned frame, ever;
 *   - at most ONE borrowed frame, ever;
 *   - when both exist, the owned one is ABOVE (lower slot index) the
 *     borrowed one;
 *   - with no capture open, NEITHER exists.
 *
 * Asserted after every step of a gesture sweep that crosses the paths which
 * create, move, fold and destroy registrations — including the two the
 * round-4 readers attacked (the sub-mode toggle over an owned base, and a
 * surface rebuilt after a line destroyed it). ---- */
static int _consoleOwnershipOk(const char *where, int *fail)
{
  uint8_t owned  = forthConsoleTestOwnedCount();
  uint8_t borrow = forthConsoleTestBorrowCount();
  int ok = 1;

  if (owned > 1) {
    printf("    FAIL: [%s] %u OWNED frames — at most one, ever\n", where, owned);
    *fail = 1; ok = 0;
  }
  if (borrow > 1) {
    printf("    FAIL: [%s] %u BORROWED frames — at most one, ever\n", where, borrow);
    *fail = 1; ok = 0;
  }
  if (owned == 1 && borrow == 1
      && forthConsoleTestOwnedSlot() > forthConsoleTestBorrowSlot()) {
    printf("    FAIL: [%s] the owned frame (slot %d) is BELOW the borrowed"
           " base (slot %d) — the close rung would release the wrong one\n",
           where, forthConsoleTestOwnedSlot(), forthConsoleTestBorrowSlot());
    *fail = 1; ok = 0;
  }
  if (!forthCapIsOpen() && (owned || borrow)) {
    printf("    FAIL: [%s] %u owned + %u borrowed with NO capture open\n",
           where, owned, borrow);
    *fail = 1; ok = 0;
  }
  return ok;
}

static int test_console_ownership_invariant(void)
{
  int fail = 0;
  int i;

  /* Two owner fixtures: a menu that is NOT one of the console's rows (the
   * open CREATES its frame), and the user's own FWRD row reached through
   * the catalog tree (the open BORROWS it).  The borrow fixture is the one
   * that legitimately reaches two registrations. */
  for (i = 0; i < 2; i++) {
    const char *own = (i == 0) ? "created base" : "borrowed base";

    N13_RESET();
    forthDictInit();
    if (i == 0) {
      showSoftmenu(-MNU_STK);
    }
    else {
      showSoftmenu(-MNU_CATALOG);
      showSoftmenu(-MNU_FORTH);
    }
    _consoleOwnershipOk(own, &fail);          /* nothing registered yet */

    fnForthOuter(NOPARAM);
    _consoleOwnershipOk(own, &fail);          /* after the open */
    if (i == 0 && forthConsoleTestOwnedCount() != 1) {
      printf("    FAIL: [%s] the open over a foreign row must CREATE a frame\n", own);
      fail = 1;
    }
    if (i == 1 && forthConsoleTestBorrowCount() != 1) {
      printf("    FAIL: [%s] the open over the user's own FWRD must BORROW it\n", own);
      fail = 1;
    }

    /* The gesture the round-4 readers attacked: toggle into the excursion. */
    runFunction(ITM_AIM);
    _consoleOwnershipOk(own, &fail);
    if (currentMenu() != -MNU_ALPHA) {
      printf("    FAIL: [%s] the toggle did not reach the alpha row (menu %d)\n",
             own, currentMenu());
      fail = 1;
    }
    if (!forthConsoleBaseOnTop()) {
      printf("    FAIL: [%s] the excursion row must be the console's base —"
             " an unregistered row here traps EXIT on the overlay rung\n", own);
      fail = 1;
    }

    /* Toggle back (the fold-back path for a borrowed base). */
    runFunction(ITM_AIM);
    _consoleOwnershipOk(own, &fail);
    if (currentMenu() != -MNU_FORTH) {
      printf("    FAIL: [%s] the toggle back did not reach FWRD (menu %d)\n",
             own, currentMenu());
      fail = 1;
    }

    /* A line that DESTROYS the registered frame, then the repair. */
    _consoleEnterLine("EXITALL");
    _consoleOwnershipOk(own, &fail);
    if (!forthConsoleBaseOnTop()) {
      printf("    FAIL: [%s] the surface was not re-registered after EXITALL\n", own);
      fail = 1;
    }

    /* And the excursion again, now over the REBUILT frame — the exact
     * sequence Sol's refutation predicted would leave an unstamped row. */
    runFunction(ITM_AIM);
    _consoleOwnershipOk(own, &fail);
    if (!forthConsoleBaseOnTop()) {
      printf("    FAIL: [%s] excursion over a REBUILT base left an"
             " unregistered row on top\n", own);
      fail = 1;
    }

    /* Unwind, and nothing may be left registered. */
    { int presses;
      for (presses = 0; presses < 8 && forthCapIsOpen(); presses++) {
        fnKeyExit(NOPARAM);
        _consoleOwnershipOk(own, &fail);
      }
      if (forthCapIsOpen()) {
        printf("    FAIL: [%s] did not close within eight EXIT presses —"
               " a press cycling without progress is the shape round 4"
               " predicted\n", own);
        fail = 1;
        forthCapClose();
      }
    }
    _consoleOwnershipOk(own, &fail);
  }

  N13_RESET();
  forthConsoleClear();
  lastErrorCode = ERROR_NONE;
  if (!fail) {
    printf("    PASS: the ownership invariant holds at every step of both"
           " gesture sweeps, and every EXIT press makes progress\n");
  }
  return fail;
}
