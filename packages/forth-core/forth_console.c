/*
 * forth_console.c -- the console VIEW ring (Stage N, packet N1-1)
 * Per STAGE_N_CONSOLE.md N-R2/N-R3 and STAGE_N_TRACES.md N-T5.
 *
 * Storage, records, eviction and the roll offset.  No display code:
 * this file must never grow a screen.h/display.h include or an lcd_
 * call — the "no display calls outside the view" property N-T5 asserts
 * is checked by grep at the packet's acceptance, not hoped for.
 *
 * CONSOLIDATE P9 also put the interactive EXIT ladder at the tail of this
 * file, out of upstream's fnKeyExit.  It is gesture logic — softmenu frames,
 * calc mode, the capture — and makes no display call, so N-T5's property is
 * unchanged.
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
static uint32_t consoleSeq;    /* N1-6: bumped by every writer.  The ENTER
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
  consoleView = 0;                  /* N-R3: any output snaps the view to newest */
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

/* N1-3: is a line still open?  The ENTER dialogue reads this to tell "the
 * word printed something and did not end the line" from "the word printed
 * nothing", without either side carrying extra state. */
bool_t forthConsoleHasOpenLine(void) { return consoleOpen != FORTH_CONSOLE_NO_OPEN; }

uint32_t forthConsoleWriteSeq(void) { return consoleSeq; }

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

/* F13/U5 (CONSOLIDATE P9): the interactive EXIT ladder, extracted from
 * upstream's fnKeyExit.  Returns true when it handled the press (the
 * caller breaks); false only when no interactive capture is engaged.
 * Bodies and rung comments are the fnKeyExit block VERBATIM — including
 * the R12 error-dismiss pre-rung and the F8 suspended-residue recovery
 * pre-rung, which run before the three rungs proper.
 *
 * The CM_PEM arm's keys-mode rung (K2/E12.4) does NOT unify into this
 * function: it belongs to the PEM ladder's ordering, and merging the two
 * would couple two gesture systems for five lines. */
bool_t forthConsoleExitLadder(void) {
  if(forthCapIsInteractive() && lastErrorCode != 0) {
    /* AUDIT round 5 R12 (ruled 2026-08-08): EXIT is the other key the
     * error recovery invites, exempted from the sweep without its
     * paired clear.  Dismiss the stale error first — the CM_NORMAL
     * arm's own order — and unwind on the next press. */
    lastErrorCode = 0;
    return true;
  }
  if(forthCapIsInteractive() && !forthCapIsOpen()) {
    /* AUDIT round 6 (F8): the suspended residue — TAM torn down
     * without the unwind (a strand door), or an exotic path skipped
     * the resume choke point.  EXIT is the recovery gesture: resume
     * the line instead of running the ladder (whose close rung would
     * eat it) or falling to the native arm (whose closeAim would
     * commit TAM's scratch to X). */
    forthFoldUnwindIfDone();
    if(forthCapIsSuspended()) { forthCaptureResume(); }
    return true;
  }
  if(forthCapIsInteractive()) {
    /* L1-2 (C2): the interactive EXIT ladder — supersedes the L1-1
     * close guard at this site (the native arm below is unreachable
     * for an interactive capture now that this branch never falls
     * through to it).  The ladder does its own teardown and never
     * calls closeAim(), so the P6 funnel inside closeAim() does not
     * reach here either. */

    /* AUDIT C18: the OVERLAY rung runs FIRST.  EXIT unwinds the topmost
     * thing on screen, and a row the user stacked (the Greek keypad, a
     * catalog) is above the sub-mode: popping it before the flip is
     * what keeps K-R3 — the row IS the mode indicator — true at every
     * step.  The old order flipped first: with an overlay up, rung 1
     * committed keysMode while forthConsoleShowSurface was entitled to
     * change nothing, and the keypad then typed Σ+ where the row said
     * A.  (The rung's own text below is the C17 ownership form.)
     *
     * Rung: anything stacked above the base pops and the capture
     * stays open — RE-DERIVED by N1-5 (N-T4), and stated directly
     * rather than by patching the landed boolean.
     *
     * The landed form pre-normalised an -MNU_ALPHA slot 0 to MyAlpha
     * (id 1) so that `softmenuStack[0].softmenuId <= 1 && menu(1) !=
     * -MNU_ALPHA` read "the base is displayed" and fell through to
     * rung 3.  Neither half survives FWRD-as-home:
     *
     *  - the rename is DESTRUCTIVE when the open pushed nothing.  With
     *    FWRD already on the stack, pushSoftmenu dedups, so slot 0 is
     *    the USER'S OWN FWRD frame — renaming it to MyAlpha corrupts
     *    the menu the user gets back on EXIT.
     *  - `menu(1)` cannot be consulted for FWRD either: the user's
     *    pre-FORTH menu is frequently FWRD, and testing it there pops
     *    the very frame rung 3 is supposed to leave standing.
     *
     * What the rung actually means is "is anything stacked ABOVE the
     * console's base?", so it asks exactly that.  An alpha submenu, a
     * catalog, STK, FIN — none of them is the base, so they pop one per
     * press and the capture stays open; the base itself falls through
     * to the rungs below.  (calcModeNormal's own -MNU_ALPHA-guarded pop
     * still cannot fire here, as before.)
     *
     * AUDIT C17: "is this the base" is an OWNERSHIP question, and menu
     * identity is a value two owners can hold — a duplicate FWRD/ALPHA
     * row stacked over the console's registered frame must pop like any
     * other user row, not masquerade as the base.  The predicate tests
     * the frame stamp (forth_menu.c), with the old identity test as the
     * conservative fallback for unregistered states. */
    if(!forthConsoleBaseOnTop()) {
      popSoftmenu();
      /* AUDIT C8: NO stayInAIM() here.  Its job is the native AIM rule
       * "always leave an alpha row showing", and it implements that with
       * changeToALPHA() -> showSoftmenu(-MNU_ALPHA), which PUSHES a
       * frame (softmenus.c:3844).  Called on every rung-2 pop it both
       * covered the console's own row and leaked a frame the close
       * accounting knew nothing about.  The console has its own rule for
       * which row belongs to which sub-mode, and one owner for it; the
       * surface is already CM_AIM with FLAG_ALPHA set, which is the only
       * other thing stayInAIM was contributing here. */
      forthConsoleShowSurface();
      return true;
    }
    /* The EXCURSION rung — INVERTED by N1-5 (N-T4).  Keys input is the
     * console's GROUND state, so EXIT unwinds the ALPHA excursion back
     * to it and restores the FWRD home row; it no longer unwinds keys
     * into alpha, which would have been a step away from the ground
     * rather than toward it.  Runs with the base on top by construction
     * (the overlay rung above already broke), so the flip can never be
     * committed where the row cannot follow (C18). */
    if(!forthCapKeysMode()) {
      forthCapSetKeysMode(true);
      /* AUDIT C9: REPLACE the excursion's ALPHA row, do not push FWRD
       * over it — a push left two console rows on the stack and rung 3's
       * single pop then revealed ALPHA instead of the owner's menu. */
      forthConsoleShowSurface();
      return true;
    }
    /* Rung 3: close.  A non-empty line is pushed to history BEFORE
     * the close, so EXIT never loses it.
     *
     * Must NOT call closeAim() — that commits aimBuffer to X as a
     * dtString (src/c47/bufferize.c:2693-2712), exactly the native
     * behaviour the Forth capture exists to avoid (C5.4).
     *
     * The teardown is calcModeNormal() FOLLOWED BY popSoftmenu() —
     * exactly closeAim()'s own shape (src/c47/bufferize.c:2693-2695),
     * minus its string commit.  Both calls are needed, and rev 2 of
     * this packet got it wrong by removing the pop:
     *
     *   - rung 2's pre-normalisation renames slot 0 to id 1 IN PLACE;
     *     it does not pop.  softmenu[1].menuItem is -MNU_MyAlpha
     *     (src/c47/softmenus.c:1039), NOT -MNU_ALPHA.
     *   - so calcModeNormal()'s own pop, guarded on
     *     softmenu[softmenuStack[0].softmenuId].menuItem == -MNU_ALPHA
     *     (src/c47/calcMode.c:45), can never fire here.  Only its
     *     second check (id 1 -> 0) runs, another in-place rename.
     *   - two renames, zero pops: the -MNU_ALPHA frame pushed at open
     *     stays, and the user's pre-FORTH menu stays buried under it.
     *
     * The unconditional popSoftmenu() is what actually removes that
     * frame and reveals the pre-FORTH menu.  C5.6b pins it.
     *
     * No undo(), no saveForUndo(), no updateMatrixHeightCache() — the
     * native arm below runs those because closeAim() either commits
     * the buffer to X or undo()s the placeholder that calcModeAim's
     * liftStack() created.  T9 removes that placeholder entirely: the
     * interactive open does not lift (PACKET_L1_1 C2b), so X is
     * untouched from FORTH-press to EXIT and there is nothing to
     * resolve.  Calling undo() here would be actively wrong — it
     * would roll back whatever the user's ENTER'd lines did to the
     * stack. */
    if(aimBuffer[0] != 0) {
      forthHistoryPush(aimBuffer);     /* L1-H fills this in */
    }
    { bool_t popHome = forthConsoleOwnsSlot0(); /* C17: pop only a frame
                                                  the console CREATED — a
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
       * homePushed note and the [8] row of the M1-1 battery. */
      if(popHome) {
        popSoftmenu();      /* the frame rung 2's pre-normalisation
                               renamed but did not pop */
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
