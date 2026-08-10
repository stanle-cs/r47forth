#include "forth_capture.h"
#include "forth_console.h"
#include "forth_menu.h"     /* forthConsoleUnstampAll at the close funnel */

static forthCap_t forthCap;   /* zero-initialized: FCAP_CLOSED */

/* forthCapOpen split into the shared body plus a per-origin entry.
 * A still-SUSPENDED object is an orphan here (exotic mode changes that skip
 * the resume choke point); the assignment below drops it. */
static void _forthCapOpenAs(uint8_t origin) {
  aimBuffer[0] = 0;                         /* reopen = fresh line */
  forthCap.state = FCAP_OPEN;
  forthCap.keysMode = 0;                    /* a fresh capture starts in
                                               alpha input (owner default) */
  forthCap.origin = origin;
  forthCap.historyIndex = FORTH_HIST_BROWSE_NONE;  /* fresh capture is not
                                                       browsing */
  /* Frame ownership is NOT reset here — it lives in the softmenu frame
   * itself (forth_menu.c's stamp), which is exactly why it survives the
   * REPL reopen and the fold resume without needing hand-preservation at
   * both sites. */
}

void forthCapOpen(void)            { _forthCapOpenAs(FCAP_ORIGIN_PEM); }
void forthCapOpenInteractive(void) { _forthCapOpenAs(FCAP_ORIGIN_INTERACTIVE); }

void forthCapClose(void) {
  forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* every close path runs through
                                               here, so this one site covers
                                               the whole sweep */
  forthCap.origin = FCAP_ORIGIN_PEM;        /* same rationale — every close
                                               path runs through here */
  forthConsoleUnstampAll();                 /* a frame stamp must not outlive
                                               the capture that minted it.
                                               Callers must read
                                               forthConsoleOwnsSlot0() BEFORE
                                               calling this. */
}

void forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset, uint16_t stepCount) {
  forthCap.savedCursor     = cursor;
  forthCap.savedLocalStep  = localStep;
  forthCap.savedStepOffset = stepOffset;
  forthCap.savedStepCount  = stepCount;
  /* The line is NOT carried across the suspension: the on-disk step is
   * the single source of truth, and forthCaptureSuspend() has just
   * recommitted it. TAM is free to use aimBuffer for its own name entry
   * while we are suspended; resume refills from the step. */
  forthCap.state = FCAP_SUSPENDED;
}
bool_t   forthCapIsSuspended(void)     { return forthCap.state == FCAP_SUSPENDED; }
uint16_t forthCapSavedCursor(void)     { return forthCap.savedCursor; }
uint16_t forthCapSavedLocalStep(void)  { return forthCap.savedLocalStep; }
uint32_t forthCapSavedStepOffset(void) { return forthCap.savedStepOffset; }
/* Re-point the suspension at a recovered capture step. Used only by
 * forthCaptureResume's fold-recovery path, when a second commit inside
 * one fold window shifted the step off the saved offset. */
void     forthCapSuspendStepOffset(uint32_t off) { forthCap.savedStepOffset = off; }
uint16_t forthCapSavedStepCount(void)  { return forthCap.savedStepCount; }
void     forthCapAbandonSuspended(void){ if (forthCap.state == FCAP_SUSPENDED) { forthCap.state = FCAP_CLOSED;
  forthCap.keysMode = 0;                    /* the bit rides the suspension,
                                               so an abandoned suspension must
                                               clear it here or it leaks into
                                               the next capture */
  forthCap.origin = FCAP_ORIGIN_PEM;        /* origin rides the suspension
                                               too */
  forthConsoleUnstampAll();                 /* this is a CLOSE path that does
                                               NOT go through forthCapClose(),
                                               so the frame stamp has to be
                                               cleared here too. Reached
                                               standalone by
                                               forthCaptureResume's canary
                                               arm — the power-reset caller is
                                               already covered by its own
                                               forthCapClose(). */
} }

/* Capture state cannot outlive the dictionary lifecycle. forthCapClose
 * already sets FCAP_CLOSED unconditionally (covering OPEN and SUSPENDED
 * alike) — forthCapAbandonSuspended is kept as the explicit,
 * belt-and-suspenders call for the suspended state.
 *
 * FLAG_ALPHA is deliberately NOT touched here. saveRestoreBackup.c's
 * restore sequence calls forthGDictValidateRestored()/forthDictInit()
 * (this seam) well before it restores systemFlags0/1 verbatim from the
 * backup file — any clear performed here would be silently overwritten
 * moments later by that restore. Clearing FLAG_ALPHA for a closed capture,
 * if ever wanted, belongs after the systemFlags restore, not in this seam;
 * that is exactly what forthCaptureSanitizeRestoredUi() does. */
void forthCapPowerReset(void) {
  forthCapClose();              /* flips OPEN and SUSPENDED alike */
  forthCapAbandonSuspended();   /* explicit for the suspended state */
  forthCap.keysMode = 0;        /* transient UI state never survives a
                                   dictionary-lifecycle reset */
  forthCap.origin = FCAP_ORIGIN_PEM; /* same rationale as keysMode above */
  forthCap.historyIndex = FORTH_HIST_BROWSE_NONE; /* same rationale */
  forthCap.foldMode = 0;        /* the fold's own last-resort reset —
                                   forthCapOpen/Close/AbandonSuspended MUST
                                   NOT touch this field; see forth_capture.h */
  forthConsoleClear();          /* the view ring is not capture state, but it
                                   shares the capture's lifecycle seam — this
                                   function's only two production callers are
                                   forthDictInit and forthDictClear, exactly
                                   the power-reset boundary. Deliberately NOT
                                   in forthCapClose / forthCapAbandonSuspended:
                                   the dialogue SURVIVES capture close and
                                   reopen. */
}

bool_t forthCapIsOpen(void)  { return forthCap.state == FCAP_OPEN; }
bool_t forthCapIsInteractive(void) {
  /* The state != CLOSED conjunction is not falsifiable on its own:
   * forthCapClose() also resets origin to PEM, so (CLOSED, INTERACTIVE) is
   * unreachable. */
  return forthCap.origin == FCAP_ORIGIN_INTERACTIVE && forthCap.state != FCAP_CLOSED;
}
/* The OPENNESS question, distinct from forthCapIsInteractive above: that
 * predicate is true while SUSPENDED too, which is wrong wherever "is there
 * a live line" is meant — e.g. painting a stranded-fold residue as an
 * editable line, running it as Forth on R/S, or letting
 * the recall gesture clobber TAM's name entry. */
bool_t forthCapInteractiveLive(void) {
  return forthCap.origin == FCAP_ORIGIN_INTERACTIVE && forthCap.state == FCAP_OPEN;
}
uint8_t forthCapOriginRaw(void)          { return forthCap.origin; }
void    forthCapSetOrigin(uint8_t o)     { forthCap.origin = o; }
/* Raw foldMode access for forthFoldEnter/forthFoldLeave — see
 * forth_capture.h for why they cannot reach forthCap directly. */
uint8_t forthCapFoldModeRaw(void)        { return forthCap.foldMode; }
void    forthCapSetFoldModeRaw(uint8_t m){ forthCap.foldMode = m; }
bool_t forthCapTextNonEmpty(void) {
  return forthCap.state == FCAP_OPEN && aimBuffer[0] != 0;
}

/* The keys-mode bit. Transient, never persisted. */
bool_t forthCapKeysMode(void)            { return forthCap.keysMode != 0; }
void   forthCapSetKeysMode(bool_t on)    { forthCap.keysMode = on ? 1 : 0; }

/* The recall browse-index field. */
uint16_t forthCapHistoryIndex(void)          { return forthCap.historyIndex; }
void     forthCapSetHistoryIndex(uint16_t i) { forthCap.historyIndex = i; }

/* The one spelling of "can this item be inserted into a capture as text".
 * Two sites had forked copies: pemAlpha's PEM arm and runFunction's
 * interactive divert, whose exclusion lists differed because pemAlpha's
 * earlier arms consume ENTER/BACKSPACE/EXIT/R-S before its copy ran. That
 * coupling was implicit and cross-file; it is now this function. item > 0
 * is LOAD-BEARING at both sites: determineItem returns negative softmenu
 * ids and indexOfItems[negative] is out of bounds. interactive == the
 * runFunction divert (raw key stream); false == the PEM pemAlpha arm
 * (console keys already consumed). */
bool_t forthCapNameInsertEligible(int16_t item, bool_t interactive) {
  if(item <= 0)                                              { return false; }
  if((indexOfItems[item].status & CAT_STATUS) != CAT_FNCT)   { return false; }
  if((indexOfItems[item].status & PTP_STATUS) != PTP_NONE)   { return false; }
  if(item == ITM_AIM || item == ITM_FORTH)                   { return false; }
  if(interactive && (item == ITM_ENTER || item == ITM_EXIT1
                     || item == ITM_BACKSPACE || item == ITM_RS)) {
    return false;
  }
  return true;
}

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t forthTestCapState(void) { return forthCap.state; }
const char *forthTestCapText(void) {
  return forthCap.state == FCAP_OPEN ? (const char *)aimBuffer : "";
}
uint8_t forthTestCapOrigin(void) { return forthCap.origin; }
#endif
