#ifndef FORTH_CONSOLE_H
#define FORTH_CONSOLE_H

#include "c47.h"

/* forth_console -- the console VIEW ring (Stage N, N-R2).
 *
 * The dialogue as DISPLAYED: committed lines echoed by the ENTER
 * orchestrator interleaved with the output FHIST must never hold (X
 * echoes, error lines, word output).  FHIST stays the history STORE —
 * a named, runnable program — and this ring stays the view.  N-R9
 * ruled the pair kept; see STAGE_N_CONSOLE.md.
 *
 * Lifetime: BSS.  Survives capture close/reopen (reopening restores the
 * dialogue) and deep sleep, exactly like the FLAG_ALPHA state it sits
 * beside; cleared at the dictionary-lifecycle seam via
 * forthCapPowerReset(), and by PAGE.  Never persisted — the input lines
 * persist because FHIST already does (§5.5).
 *
 * This module does not paint.  It has no screen.h, no display.h and no
 * lcd_ call; N1-2's render arm is the only reader that draws.
 *
 * RECORD FORMAT: [len][payload...], len = payload byte count 0..255.
 * One record is one display line; len == 0 is a blank line.  Records
 * tile the used region from the tail forward, wrapping modulo the ring
 * size.
 *
 * Length-prefixed rather than '\n'-terminated, and the reason is
 * load-bearing: a C47 glyph is one byte below 0x80 or TWO bytes when the
 * first is 0x80..0xFF, and the second byte may be ANYTHING — 0x0A
 * included (STD_UP_ARROW is "\xa1\x91"; the decode loop at
 * screen.c:1725-1728 takes the next byte unconditionally).  A newline
 * scanner would split such a glyph and desynchronise the whole ring.
 * Length prefixes make the walk glyph-blind and "skip to the next
 * record" O(1). */

#define FORTH_CONSOLE_RING_BYTES   1024   /* N-R2: the declared RAM price */
#define FORTH_CONSOLE_LINE_MAX      255   /* one record's payload cap.  Far under
                                             the ring, which is what makes
                                             eviction total: the open record is
                                             at most 256 bytes, so evicting
                                             everything else always leaves >= 768
                                             free and _reserve cannot fail. */
#define FORTH_CONSOLE_NO_OPEN    0xFFFFu  /* "no unterminated record" */

void     forthConsoleClear(void);
void     forthConsoleAppend(const char *s);      /* text; embedded '\n' breaks lines */
void     forthConsoleNewline(void);              /* close the line (CR) */
void     forthConsoleAppendLine(const char *s);  /* Append then Newline */
uint16_t forthConsoleLineCount(void);
bool_t   forthConsoleLineAt(uint16_t n, char *out, uint16_t outSize);  /* n = 0 is NEWEST */
uint16_t forthConsoleViewOffset(void);
void     forthConsoleSetViewOffset(uint16_t n);

#if defined(FORTH_DEBUG_SELFTEST)
  /* Raw state for the N1-1 invariant assertions.  Production code has no
   * business reading these. */
  uint16_t forthConsoleTestUsed(void);
  uint16_t forthConsoleTestTail(void);
  bool_t   forthConsoleTestHasOpen(void);
  uint8_t  forthConsoleTestByteAt(uint16_t i);
#endif

#endif // FORTH_CONSOLE_H
