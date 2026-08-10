/*
 * forth_console.c -- the console VIEW ring.
 *
 * Storage, records, eviction and the roll offset.  No display code:
 * this file must never grow a screen.h/display.h include or an lcd_
 * call — the "no display calls outside the view" property is checked
 * by grep, not hoped for.
 *
 * The interactive EXIT ladder also lives at the tail of this file, out
 * of upstream's fnKeyExit.  It is gesture logic — softmenu frames, calc
 * mode, the capture — and makes no display call, so the property above
 * is unchanged.
 */

#include "forth_console.h"
#include "forth_capture.h"
#include "forth_menu.h"

static char     consoleRing[FORTH_CONSOLE_RING_BYTES];
static uint16_t consoleTail;   /* index of the OLDEST record's length byte */
static uint16_t consoleUsed;   /* bytes occupied, 0..FORTH_CONSOLE_RING_BYTES */
static uint16_t consoleOpen = FORTH_CONSOLE_NO_OPEN;
                               /* index of the OPEN record's length byte */
static uint16_t consoleView;   /* roll offset: 0 = newest line at the bottom */
static uint32_t consoleSeq;    /* bumped by every writer.  The ENTER
                                  dialogue samples it across a run to tell
                                  "the line spoke for itself" from "the line
                                  said nothing", which is a stronger question
                                  than "is a line still open" — `.S` and PAGE
                                  both close their own output and would
                                  otherwise still collect an X echo underneath. */

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
  consoleSeq++;
  consoleTail = 0;
  consoleUsed = 0;
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;
}

void forthConsoleNewline(void) {
  consoleSeq++;
  if (consoleOpen == FORTH_CONSOLE_NO_OPEN) {
    (void)_openRecord();            /* CR on a fresh line IS a blank line */
  }
  consoleOpen = FORTH_CONSOLE_NO_OPEN;
  consoleView = 0;                  /* any output snaps the view to newest */
}

void forthConsoleAppend(const char *s) {
  const char *p;
  if (s == NULL) { return; }
  if (*s != 0) { consoleSeq++; }
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
  consoleView = 0;                  /* any output snaps the view to newest */
}

void forthConsoleAppendLine(const char *s) {
  forthConsoleAppend(s);
  forthConsoleNewline();
}

/* The record walk, bounded twice over.
 *
 * Neither guard can fire while the ring invariants hold — a record is at
 * least one byte, so a consistent ring holds at most
 * FORTH_CONSOLE_RING_BYTES of them, and the sizes sum to exactly `used`.
 * They exist because the unguarded loop's failure mode on a
 * desynchronised ring is not a wrong answer but a HANG: `remaining - sz`
 * underflows through 0 into 65535 and the walk never terminates.  This
 * runs on a calculator with no way to kill a spinning task, so a
 * corrupted ring must degrade to a miscount instead. */
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

/* Is a line still open?  The ENTER dialogue reads this to tell "the word
 * printed something and did not end the line" from "the word printed
 * nothing", without either side carrying extra state. */
bool_t forthConsoleHasOpenLine(void) { return consoleOpen != FORTH_CONSOLE_NO_OPEN; }

uint32_t forthConsoleWriteSeq(void) { return consoleSeq; }

uint16_t forthConsoleViewOffset(void) { return consoleView; }

void forthConsoleSetViewOffset(uint16_t n) {
  uint16_t count = forthConsoleLineCount();
  consoleView = (count == 0) ? 0 : ((n >= count) ? (uint16_t)(count - 1) : n);
}

/* The roll.  delta > 0 walks BACK through the dialogue (older lines),
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

/* The interactive EXIT ladder, extracted from upstream's fnKeyExit.
 * Returns true when it handled the press (the caller breaks); false
 * only when no interactive capture is engaged.  Bodies and rung
 * comments are the fnKeyExit block VERBATIM, including the
 * error-dismiss pre-rung and the suspended-residue recovery pre-rung,
 * which run before the three rungs proper.
 *
 * The CM_PEM arm's own keys-mode rung does NOT unify into this
 * function: it belongs to the PEM ladder's ordering, and merging the
 * two would couple two gesture systems for five lines. */
bool_t forthConsoleExitLadder(void) {
  if(forthCapIsInteractive() && lastErrorCode != 0) {
    /* EXIT is the other key the error recovery invites, exempted from
     * the sweep without its paired clear.  Dismiss the stale error
     * first — the CM_NORMAL arm's own order — and unwind on the next
     * press. */
    lastErrorCode = 0;
    return true;
  }
  if(forthCapIsInteractive() && !forthCapIsOpen()) {
    /* The suspended residue: TAM torn down without the unwind, or an
     * exotic path that skipped the resume choke point.  EXIT is the
     * recovery gesture: resume the line instead of running the ladder
     * (whose close rung would eat it) or falling to the native arm
     * (whose closeAim would commit TAM's scratch to X). */
    forthFoldUnwindIfDone();
    if(forthCapIsSuspended()) { forthCaptureResume(); }
    return true;
  }
  if(forthCapIsInteractive()) {
    /* The interactive EXIT ladder does its own teardown and never
     * calls closeAim(), so the native arm below is unreachable for an
     * interactive capture and closeAim()'s own funnel does not reach
     * here either. */

    /* The OVERLAY rung runs FIRST.  EXIT unwinds the topmost thing on
     * screen, and a row the user stacked (the Greek keypad, a catalog)
     * is above the sub-mode: popping it before the flip is what keeps
     * the row-is-the-mode-indicator invariant true at every step.  With
     * an overlay up, flipping keysMode first would commit it while
     * forthConsoleShowSurface is entitled to change nothing, so the
     * surface and the mode would disagree.
     *
     * The rung asks "is anything stacked ABOVE the console's base?".
     * An alpha submenu, a catalog, STK, FIN — none of them is the base,
     * so they pop one per press and the capture stays open; the base
     * itself falls through to the rungs below.
     *
     * "Is this the base" is an OWNERSHIP question: menu identity is a
     * value two owners can hold, so a duplicate FWRD/ALPHA row stacked
     * over the console's registered frame must pop like any other user
     * row, not masquerade as the base.  The predicate tests the frame
     * stamp (forth_menu.c), with the old identity test as the
     * conservative fallback for unregistered states. */
    if(!forthConsoleBaseOnTop()) {
      popSoftmenu();
      /* NO stayInAIM() here.  Its job is the native AIM rule "always
       * leave an alpha row showing", implemented via changeToALPHA() ->
       * showSoftmenu(-MNU_ALPHA), which PUSHES a frame.  Called on
       * every rung-2 pop it both covered the console's own row and
       * leaked a frame the close accounting knew nothing about — the
       * console has its own rule for which row belongs to which
       * sub-mode, and one owner for it. */
      forthConsoleShowSurface();
      return true;
    }
    /* The EXCURSION rung.  Keys input is the console's GROUND state, so
     * EXIT unwinds the ALPHA excursion back to it and restores the FWRD
     * home row.  Runs with the base on top by construction (the overlay
     * rung above already broke), so the flip can never be committed
     * where the row cannot follow. */
    if(!forthCapKeysMode()) {
      forthCapSetKeysMode(true);
      /* REPLACE the excursion's ALPHA row, do not push FWRD over it —
       * a push left two console rows on the stack and rung 3's single
       * pop then revealed ALPHA instead of the owner's menu. */
      forthConsoleShowSurface();
      return true;
    }
    /* Rung 3: close.  A non-empty line is pushed to history BEFORE
     * the close, so EXIT never loses it.
     *
     * Must NOT call closeAim() — that commits aimBuffer to X as a
     * dtString, exactly the native behaviour the Forth capture exists
     * to avoid.
     *
     * The teardown is calcModeNormal() FOLLOWED BY popSoftmenu() —
     * exactly closeAim()'s own shape, minus its string commit.  Both
     * calls are needed:
     *
     *   - the console's home row is -MNU_FWRD, not -MNU_ALPHA, so
     *     calcModeNormal()'s own pop (guarded on -MNU_ALPHA) does not
     *     fire for the console's own base;
     *   - so calcModeNormal() leaves the frame standing, and the
     *     explicit popSoftmenu() below is the only thing that removes
     *     it and reveals the pre-FORTH menu;
     *   - and it is CONDITIONAL on popHome: when the open borrowed the
     *     user's own FWRD row instead of pushing one, popping would eat
     *     the owner's frame.
     *
     * No undo(), no saveForUndo(), no updateMatrixHeightCache() — the
     * native arm below runs those because closeAim() either commits
     * the buffer to X or undo()s the placeholder that calcModeAim's
     * liftStack() created.  The interactive open does not lift, so X is
     * untouched from FORTH-press to EXIT and there is nothing to
     * resolve.  Calling undo() here would be actively wrong — it
     * would roll back whatever the user's ENTER'd lines did to the
     * stack. */
    if(aimBuffer[0] != 0) {
      forthHistoryPush(aimBuffer);
    }
    { bool_t popHome = forthConsoleOwnsSlot0(); /* pop only a frame the
                                                  console CREATED — a
                                                  BORROWED base is the
                                                  user's own row and is
                                                  released, not popped.
                                                  Read BEFORE the close
                                                  clears the stamp. */
      forthCapClose();
      aimBuffer[0] = 0;
      T_cursorPos = 0;
      displayAIMbufferoffset = 0;
      calcModeNormal();
      /* Pop ONLY what the open pushed.  The unconditional pop was right
       * while the open always added a frame; with FWRD as the home row
       * it does not when FWRD was already on the stack (pushSoftmenu
       * dedups), and popping anyway ate the user's own FWRD frame and
       * revealed whatever was beneath it — see forth_capture.h's
       * homePushed note. */
      if(popHome) {
        popSoftmenu();      /* the console's own base frame, which
                               calcModeNormal() above cannot pop (its pop
                               is guarded on -MNU_ALPHA and this row is
                               -MNU_FWRD). */
      }
    }
    return true;
  }
  return false;
}

#if defined(FORTH_DEBUG_SELFTEST)
  uint16_t forthConsoleTestUsed(void)          { return consoleUsed; }
  uint16_t forthConsoleTestTail(void)          { return consoleTail; }
  bool_t   forthConsoleTestHasOpen(void)       { return consoleOpen != FORTH_CONSOLE_NO_OPEN; }
  uint8_t  forthConsoleTestByteAt(uint16_t i)  { return (uint8_t)consoleRing[_wrap(i)]; }
#endif
