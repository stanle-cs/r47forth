#ifndef FORTH_CONSOLE_H
#define FORTH_CONSOLE_H

#include "c47.h"
#include "forth_capture.h"   /* FORTH_SELFTEST_EXPORT */

/* forth_console -- the console VIEW ring.
 *
 * The dialogue as DISPLAYED: committed lines echoed by the ENTER
 * orchestrator interleaved with the output FHIST must never hold (X
 * echoes, error lines, word output).  FHIST stays the history STORE —
 * a named, runnable program — and this ring stays the view; the two
 * are kept deliberately separate.
 *
 * Lifetime: BSS.  Survives capture close/reopen (reopening restores the
 * dialogue) and deep sleep, exactly like the FLAG_ALPHA state it sits
 * beside; cleared at the dictionary-lifecycle seam via
 * forthCapPowerReset(), and by PAGE.  Never persisted — the input lines
 * persist because FHIST already does (§5.5).
 *
 * This module does not paint.  It has no screen.h, no display.h and no
 * lcd_ call; the render arm is the only reader that draws.
 *
 * RECORD FORMAT: [len][payload...], len = payload byte count 0..255.
 * One record is one display line; len == 0 is a blank line.  Records
 * tile the used region from the tail forward, wrapping modulo the ring
 * size.
 *
 * Length-prefixed rather than '\n'-terminated, and the reason is
 * load-bearing: a C47 glyph is one byte below 0x80 or TWO bytes when the
 * first is 0x80..0xFF, and the second byte may be ANYTHING — 0x0A
 * included (STD_UP_ARROW is "\xa1\x91"; the decode loop takes the next
 * byte unconditionally).  A newline scanner would split such a glyph
 * and desynchronise the whole ring.  Length prefixes make the walk
 * glyph-blind and "skip to the next record" O(1). */

#define FORTH_CONSOLE_RING_BYTES   1024   /* the ring's RAM budget */
#define FORTH_CONSOLE_LINE_MAX      255   /* one record's payload cap.  Far under
                                             the ring, which is what makes
                                             eviction total: the open record is
                                             at most 256 bytes, so evicting
                                             everything else always leaves >= 768
                                             free and _reserve cannot fail. */
#define FORTH_CONSOLE_CONTROL_HINT  "ENTER=SPACE  R/S=RUN"
#define FORTH_CONSOLE_NO_OPEN    0xFFFFu  /* "no unterminated record" */
#define FORTH_CONSOLE_FMT_MAX      256    /* one formatted value; a bounded LOCAL,
                                             never tmpString — display.c writes
                                             tmpString in ~190 places and the
                                             caller cannot know which producer
                                             aliases it */

/* X (or any register) rendered per the CURRENT display mode, into the
 * caller's buffer.  Lives in forth_bridge.c — it needs display.h, and this
 * module stays display-free by construction. */
void forthConsoleFormatRegister(calcRegister_t regist, char *out, int16_t outSize);

/* Copy text into a fixed buffer, cutting only on a GLYPH boundary (the
 * C47 encoding is one byte below 0x80, two bytes above).  A byte cut
 * leaves a lone lead byte that the painter and forthConsoleLineAt both
 * re-pair with whatever follows.  Every site that puts a length-limited
 * string into the ring goes through this.  Returns the bytes written;
 * always NUL-terminates when cap >= 1. */
int32_t forthCopyWholeGlyphs(char *dst, const char *src, int32_t cap);

void     forthConsoleClear(void);
void     forthConsoleAppend(const char *s);      /* text; embedded '\n' breaks lines */
void     forthConsoleNewline(void);              /* close the line (CR) */
void     forthConsoleAppendLine(const char *s);  /* Append then Newline */
uint16_t forthConsoleLineCount(void);
bool_t   forthConsoleLineAt(uint16_t n, char *out, uint16_t outSize);  /* n = 0 is NEWEST */
uint16_t forthConsoleViewOffset(void);
void     forthConsoleSetViewOffset(uint16_t n);
void     forthConsoleRoll(int16_t delta);        /* +1 = one line OLDER (scroll back) */

/* View-side (implemented in forth_console_view.c): the band's row count
   and the roll bounded by it.  rows is a VIEW concept this ring module
   deliberately does not have — the view owns the clamp; forthConsoleRoll
   above stops only at ring bounds. */
uint16_t forthConsoleViewRows(void);
void     forthConsoleRollView(int16_t delta);

/* Gate and paint, called from the screen.c override's refresh arm.
   Non-static since they are called across the file boundary. */
bool_t   _forthConsoleActive(void);
void     _forthConsoleRender(void);

/* The offset a frame paints from, clamped to the frame's own bound.
   Exported to the self-test build only; static in production. */
FORTH_SELFTEST_EXPORT uint16_t _forthConsoleViewBase(uint16_t rows, uint16_t count);

/* Defined in the screen.c override — reads screen.c's checkHPoffset
   macro, a screen.c-local coupling this module cannot avoid. */
uint16_t _forthConsoleEditorTop(void);

/* showStringEdC47's own line pitch for the wrapped-line state
 * (`yincr = 35`).  Upstream keeps it as a function local, so the package
 * cannot reference it by name; naming it here is as close to upstream's
 * own constant as this side can get.
 *
 * The _Static_asserts in forth_console_view.c tie the BAND's row counts
 * to upstream's Y_POSITION_* names but cannot check this literal against
 * a function local.  design-audit.sh group I's yincr pin greps upstream's
 * showStringEdC47 for the literal and fails when it moves — that pin, not
 * the asserts, is what guards this number against silent drift. */
#define FORTH_CONSOLE_ED_YINCR    35
/* One pixel of clearance: the editor's cursor block starts one row above
 * the text baseline showStringEdC47 draws at, and the band stops below
 * that row. */
#define FORTH_CONSOLE_ED_CLEAR     1
bool_t   forthConsoleHasOpenLine(void);          /* a word left output unterminated */

/* The interactive EXIT ladder, out of upstream's fnKeyExit.  True means
   the press was consumed and the caller breaks; false means no
   interactive capture is engaged and the native arm runs. */
bool_t   forthConsoleExitLadder(void);
uint32_t forthConsoleWriteSeq(void);             /* bumped by every writer */

#if defined(FORTH_DEBUG_SELFTEST)
  /* Raw state for the invariant assertions.  Production code has no
   * business reading these. */
  uint16_t forthConsoleTestUsed(void);
  uint16_t forthConsoleTestTail(void);
  bool_t   forthConsoleTestHasOpen(void);
  uint8_t  forthConsoleTestByteAt(uint16_t i);
#endif

#endif // FORTH_CONSOLE_H
