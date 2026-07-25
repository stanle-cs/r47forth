#ifndef FORTH_CAPTURE_H
#define FORTH_CAPTURE_H

#include "c47.h"

/* Forth capture state.
 *
 * S3 (simplification pass): the capture line lives in `aimBuffer`, the
 * native PEM/AIM input sink, exactly like the REM and LITERAL captures
 * it sits beside.  There is no separate buffer and no allocation.
 *
 * It used to have one.  F6-1 moved the capture text off aimBuffer
 * because TAM-cancel zeroes aimBuffer in PEM, which destroyed a
 * suspended capture line.  F6-2 then made the on-disk program step the
 * single source of truth — suspend FREES the buffer and resume refills
 * from the step payload — and audit #1 (88a2b5f85) made that recommit
 * unconditional.  Once the text is always recoverable from the step,
 * TAM clobbering aimBuffer stops mattering, and the separate buffer is
 * a vestige: it bought nothing and cost a `forthCapIsOpen() ?
 * forthCapBuf() : aimBuffer` ternary at every sink, cursor and render
 * site, plus its own allocator lifetime to get wrong.
 *
 * What remains is state, not storage: which of the three capture states
 * we are in, and the snapshot taken across a TAM suspension.  aimBuffer
 * is 1024 bytes (AIM_BUFFER_LENGTH) and the capture cap is 256 bytes /
 * 196 glyphs enforced in code at the insertion sites, so the cap
 * contract is unchanged by the move.
 *
 * NOT derivable from calcMode/FLAG_ALPHA/tam.function, which was the
 * first thing tried: tamEnterMode assigns the INCOMING TAM function
 * before the CM_PEM suspend seam fires, so tam.function is already the
 * TAM op (not ITM_FORTH) at the one place suspend must recognise an
 * open capture.  Hence the explicit flag below. */

typedef enum { FCAP_CLOSED = 0, FCAP_OPEN = 1, FCAP_SUSPENDED = 2 } forthCapState_t;

typedef struct {
  uint8_t     state;          /* forthCapState_t */
  /* Suspend snapshot — meaningful only in FCAP_SUSPENDED: */
  uint16_t    savedCursor;    /* T_cursorPos at suspend */
  uint16_t    savedLocalStep; /* currentLocalStepNumber at suspend */
  uint32_t    savedStepOffset;/* capture step vs beginOfProgramMemory
                                 (offset: program memory may relocate) */
  uint16_t    savedStepCount; /* F6-4: getNumberOfSteps() at suspend, so
                                 resume can tell how many steps a TAM
                                 commit inserted */
} forthCap_t;

void        forthCapOpen(void);       /* state FCAP_OPEN; clears aimBuffer.
                                         Cannot fail (nothing is allocated) */
void        forthCapClose(void);      /* state FCAP_CLOSED; safe if already
                                         closed */
bool_t      forthCapIsOpen(void);     /* state == FCAP_OPEN */
bool_t      forthCapTextNonEmpty(void); /* open && aimBuffer[0] != 0 */

/* F6-2: suspend/resume state ops */
void     forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset, uint16_t stepCount);
bool_t   forthCapIsSuspended(void);
uint16_t forthCapSavedCursor(void);
uint16_t forthCapSavedLocalStep(void);
uint32_t forthCapSavedStepOffset(void);
uint16_t forthCapSavedStepCount(void);  /* F6-4 */
void     forthCapAbandonSuspended(void);

/* F6-2: orchestrators (programming/manage.c — need the file-static
 * _closeAlphaMenus) */
void     forthCaptureSuspend(void);
void     forthCaptureResume(void);
void     forthCaptureSanitizeRestoredUi(void);

/* F6-3's shared inserter forthCapInsertName() now lives in forth_menu.h
 * alongside the picker that is its main caller (S2). */

/* F6-6: capture state cannot outlive the dictionary lifecycle.  Called at
 * the same seams as forthScanTrackReset (init / clear / restore
 * validation): a restored or re-initialized machine starts CLOSED.
 * Deep-sleep wake on hardware does NOT run these seams — a sleeping
 * capture legitimately survives, matching the landed FLAG_ALPHA
 * behavior. */
void     forthCapPowerReset(void);

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t     forthTestCapState(void);
const char *forthTestCapText(void);   /* "" when not open */
#endif

#endif
