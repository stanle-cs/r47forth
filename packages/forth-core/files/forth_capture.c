#include "forth_capture.h"

static forthCap_t forthCap;   /* zero-initialized: FCAP_CLOSED */

void forthCapOpen(void) {
  /* A still-SUSPENDED object is an orphan here (exotic mode changes that
   * skip the resume choke point); the assignment below drops it. */
  aimBuffer[0] = 0;                         /* reopen = fresh line */
  forthCap.state = FCAP_OPEN;
  forthCap.keysMode = 0;                    /* K1: a fresh capture starts in
                                               alpha input (owner default) */
}

void forthCapClose(void) {
  forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* K1/E14: since FIX-8 every close
                                               path runs through here, so this
                                               one site covers the whole sweep */
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
uint16_t forthCapSavedStepCount(void)  { return forthCap.savedStepCount; }
void     forthCapAbandonSuspended(void){ if (forthCap.state == FCAP_SUSPENDED) { forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* K3/E14: since K3 the bit rides
                                               the suspension, so an abandoned
                                               suspension must clear it here or
                                               it leaks into the next capture */
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
}

bool_t forthCapIsOpen(void)  { return forthCap.state == FCAP_OPEN; }
bool_t forthCapTextNonEmpty(void) {
  return forthCap.state == FCAP_OPEN && aimBuffer[0] != 0;
}

/* K1 (E10-E12): the keys-mode bit.  Transient, never persisted. */
bool_t forthCapKeysMode(void)            { return forthCap.keysMode != 0; }
void   forthCapSetKeysMode(bool_t on)    { forthCap.keysMode = on ? 1 : 0; }

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t forthTestCapState(void) { return forthCap.state; }
const char *forthTestCapText(void) {
  return forthCap.state == FCAP_OPEN ? (const char *)aimBuffer : "";
}
#endif
