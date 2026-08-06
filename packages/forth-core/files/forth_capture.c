#include "forth_capture.h"
#include "forth_console.h"

static forthCap_t forthCap;   /* zero-initialized: FCAP_CLOSED */

/* L1-1: forthCapOpen split into the shared body plus a per-origin entry.
 * A still-SUSPENDED object is an orphan here (exotic mode changes that skip
 * the resume choke point); the assignment below drops it. */
static void _forthCapOpenAs(uint8_t origin) {
  aimBuffer[0] = 0;                         /* reopen = fresh line */
  forthCap.state = FCAP_OPEN;
  forthCap.keysMode = 0;                    /* K1: a fresh capture starts in
                                               alpha input (owner default) */
  forthCap.origin = origin;
  forthCap.historyIndex = FORTH_HIST_BROWSE_NONE;  /* L1-H: fresh capture
                                                       is not browsing */
}

void forthCapOpen(void)            { _forthCapOpenAs(FCAP_ORIGIN_PEM); }
void forthCapOpenInteractive(void) { _forthCapOpenAs(FCAP_ORIGIN_INTERACTIVE); }

void forthCapClose(void) {
  forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* K1/E14: since FIX-8 every close
                                               path runs through here, so this
                                               one site covers the whole sweep */
  forthCap.origin = FCAP_ORIGIN_PEM;        /* L1-1/E14: same rationale — every
                                               close path runs through here */
}

void forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset, uint16_t stepCount) {
  forthCap.savedCursor     = cursor;
  forthCap.savedLocalStep  = localStep;
  forthCap.savedStepOffset = stepOffset;
  forthCap.savedStepCount  = stepCount;
  /* The line is NOT carried across the suspension: the on-disk step is
   * the single source of truth (F6-2), and forthCaptureSuspend() has
   * just recommitted it.  TAM is free to use aimBuffer for its own name
   * entry while we are suspended; resume refills from the step. */
  forthCap.state = FCAP_SUSPENDED;
}
bool_t   forthCapIsSuspended(void)     { return forthCap.state == FCAP_SUSPENDED; }
uint16_t forthCapSavedCursor(void)     { return forthCap.savedCursor; }
uint16_t forthCapSavedLocalStep(void)  { return forthCap.savedLocalStep; }
uint32_t forthCapSavedStepOffset(void) { return forthCap.savedStepOffset; }
/* L1-F2 rev 3: re-point the suspension at a recovered capture step.  Used
 * only by forthCaptureResume's fold-recovery path, when a second commit
 * inside one fold window shifted the step off the saved offset. */
void     forthCapSuspendStepOffset(uint32_t off) { forthCap.savedStepOffset = off; }
uint16_t forthCapSavedStepCount(void)  { return forthCap.savedStepCount; }
void     forthCapAbandonSuspended(void){ if (forthCap.state == FCAP_SUSPENDED) { forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* K3/E14: since K3 the bit rides
                                               the suspension, so an abandoned
                                               suspension must clear it here or
                                               it leaks into the next capture */
  forthCap.origin = FCAP_ORIGIN_PEM;        /* L1-1/E14: origin rides the
                                               suspension too (C1) */
} }

/* F6-6: capture state cannot outlive the dictionary lifecycle.
 * forthCapClose already sets FCAP_CLOSED unconditionally (covering OPEN
 * and SUSPENDED alike) — forthCapAbandonSuspended is kept as the
 * explicit, belt-and-suspenders call for the suspended state per the
 * packet.
 *
 * FLAG_ALPHA is deliberately NOT touched here. saveRestoreBackup.c's
 * restore sequence calls forthGDictValidateRestored()/forthDictInit()
 * (this seam) well before it restores systemFlags0/1 verbatim from the
 * backup file — any clear performed here would be silently overwritten
 * moments later by that restore. Clearing FLAG_ALPHA for a closed
 * capture, if ever wanted, belongs after the systemFlags restore, not
 * in this seam (see the F6-6 commit for the traced call order); that is
 * exactly what forthCaptureSanitizeRestoredUi() does. */
void forthCapPowerReset(void) {
  forthCapClose();              /* flips OPEN and SUSPENDED alike */
  forthCapAbandonSuspended();   /* explicit for the suspended state */
  forthCap.keysMode = 0;        /* K1: transient UI state never survives a
                                   dictionary-lifecycle reset */
  forthCap.origin = FCAP_ORIGIN_PEM; /* L1-1: same rationale as keysMode above */
  forthCap.historyIndex = FORTH_HIST_BROWSE_NONE; /* L1-H: same rationale */
  forthCap.foldMode = 0;        /* L1-F1: the fold's own last-resort reset —
                                   forthCapOpen/Close/AbandonSuspended MUST
                                   NOT touch this field; see forth_capture.h */
  forthConsoleClear();          /* N1-1 (N-T5): the view ring is not capture
                                   state, but it shares the capture's lifecycle
                                   seam — this function's only two production
                                   callers are forthDictInit (forth_dict.c:57)
                                   and forthDictClear (:71), which is exactly
                                   the power-reset boundary N-R2 clears the view
                                   at.  Deliberately NOT in forthCapClose /
                                   forthCapAbandonSuspended: N-R2 rules the
                                   dialogue SURVIVES capture close and reopen,
                                   and test_console_ring_reset_seam pins that
                                   direction. */
}

bool_t forthCapIsOpen(void)  { return forthCap.state == FCAP_OPEN; }
bool_t forthCapIsInteractive(void) {
  /* Defence-in-depth: the state != CLOSED conjunction is not falsifiable on
   * its own (forthCapClose() also resets origin to PEM, so (CLOSED,
   * INTERACTIVE) is unreachable) — forthTestCapOrigin() exists precisely so
   * mutation coverage can still pin the raw field. */
  return forthCap.origin == FCAP_ORIGIN_INTERACTIVE && forthCap.state != FCAP_CLOSED;
}
uint8_t forthCapOriginRaw(void)          { return forthCap.origin; }
void    forthCapSetOrigin(uint8_t o)     { forthCap.origin = o; }
/* L1-F1: raw foldMode access for forthFoldEnter/forthFoldLeave (manage.c —
 * see forth_capture.h for why they cannot reach forthCap directly). */
uint8_t forthCapFoldModeRaw(void)        { return forthCap.foldMode; }
void    forthCapSetFoldModeRaw(uint8_t m){ forthCap.foldMode = m; }
bool_t forthCapTextNonEmpty(void) {
  return forthCap.state == FCAP_OPEN && aimBuffer[0] != 0;
}

/* K1 (E10-E12): the keys-mode bit.  Transient, never persisted. */
bool_t forthCapKeysMode(void)            { return forthCap.keysMode != 0; }
void   forthCapSetKeysMode(bool_t on)    { forthCap.keysMode = on ? 1 : 0; }

/* L1-H: the recall browse-index field. */
uint16_t forthCapHistoryIndex(void)          { return forthCap.historyIndex; }
void     forthCapSetHistoryIndex(uint16_t i) { forthCap.historyIndex = i; }

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t forthTestCapState(void) { return forthCap.state; }
const char *forthTestCapText(void) {
  return forthCap.state == FCAP_OPEN ? (const char *)aimBuffer : "";
}
uint8_t forthTestCapOrigin(void) { return forthCap.origin; }
#endif
