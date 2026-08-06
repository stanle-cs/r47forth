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
 * EMPTY area — which is also how "no register paints while the console is
 * up" is proven, since the stack holds values throughout. ---- */
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
  if (empty != 0) {
    printf("    FAIL: an empty console must paint nothing in the band"
           " (got %d lit px — a register leaked through)\n", empty);
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

/* ---- 14: the ARM is wired.  Everything above drives _forthConsoleRender
 * directly; this one goes through refreshScreen() and proves
 * _refreshNormalScreen's CM_AIM arm actually reaches it — and that the yield
 * falls back to the landed register paint rather than to nothing. ---- */
static int test_console_view_arm(void)
{
  uint8_t saved = calcMode;
  int32_t viaArm, yielded;
  int fail = 0;

  forthPushInt32(12345);
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

  /* Yield: with an error live, the native register/error paint comes back. */
  forthConsoleClear();                 /* so any ink in the band is NOT ours */
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
    printf("    PASS: the arm is wired (%d px through refreshScreen); yield restores"
           " the native paint (%d px)\n", viaArm, yielded);
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
   * restores the FWRD home row; the capture stays open. */
  N13_RESET();
  fnForthOuter(NOPARAM);
  forthCapSetKeysMode(false);
  showSoftmenu(-MNU_ALPHA);
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
