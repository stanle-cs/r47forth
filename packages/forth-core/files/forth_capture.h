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

/* L1-1: where the capture was opened from. */
typedef enum { FCAP_ORIGIN_PEM = 0, FCAP_ORIGIN_INTERACTIVE = 1 } forthCapOrigin_t;

typedef struct {
  uint8_t     state;          /* forthCapState_t */
  uint8_t     keysMode;       /* K1 (E10-E12): 0 = alpha input, 1 = keys.
                                 Transient UI state, NEVER persisted;
                                 meaningful only while state == FCAP_OPEN. */
  uint8_t     origin;         /* L1-1: forthCapOrigin_t.  PEM captures live on
                                 a program step; INTERACTIVE captures live on
                                 the AIM surface (L-R2).  Transient, NEVER
                                 persisted.  Zero is PEM so every zero-init and
                                 memset-style reset means "PEM" — matching every
                                 capture that existed before Stage L. */
  /* Suspend snapshot — meaningful only in FCAP_SUSPENDED: */
  uint16_t    savedCursor;    /* T_cursorPos at suspend */
  uint16_t    savedLocalStep; /* currentLocalStepNumber at suspend */
  uint32_t    savedStepOffset;/* capture step vs beginOfProgramMemory
                                 (offset: program memory may relocate) */
  uint16_t    savedStepCount; /* F6-4: getNumberOfSteps() at suspend, so
                                 resume can tell how many steps a TAM
                                 commit inserted */
  uint16_t    historyIndex;   /* L1-H: recall browse index into FHIST —
                                 transient, NEVER persisted.
                                 FORTH_HIST_BROWSE_NONE ("past the newest")
                                 outside a browse; reset to that value at
                                 every capture open and at every
                                 forthHistoryPush(). */
} forthCap_t;

/* L1-H: the recall browse-index sentinel ("past the newest" / not
 * currently browsing).  Distinguishable from any real line index: FHIST's
 * line count is bounded well under this by FORTH_HISTORY_MAX_BYTES (each
 * line consumes at least 5 bytes). */
#define FORTH_HIST_BROWSE_NONE ((uint16_t)0xFFFFu)

void        forthCapOpen(void);       /* state FCAP_OPEN; clears aimBuffer.
                                         Cannot fail (nothing is allocated) */
void        forthCapOpenInteractive(void);  /* L1-1: open with INTERACTIVE origin */
void        forthCapClose(void);      /* state FCAP_CLOSED; safe if already
                                         closed */
bool_t      forthCapIsOpen(void);     /* state == FCAP_OPEN */
bool_t      forthCapIsInteractive(void); /* L1-1: origin == INTERACTIVE &&
                                             state != CLOSED */
uint8_t     forthCapOriginRaw(void);     /* L1-1: the raw field — for the
                                             resume bracket */
void        forthCapSetOrigin(uint8_t o);/* L1-1: the raw field — for the
                                             resume bracket */
bool_t      forthCapTextNonEmpty(void); /* open && aimBuffer[0] != 0 */
bool_t      forthCapKeysMode(void);        /* K1: keys-mode bit */
void        forthCapSetKeysMode(bool_t on);

/* L1-2 (C1): ENTER's orchestrator for an interactive capture — runs the
 * line, then either reopens empty (REPL) or reopens with the line intact
 * for correction (error).  Defined in programming/manage.c beside the
 * other capture orchestrators (forthCaptureSuspend/Resume). Called from
 * fnKeyEnter's CM_AIM divert and from the ITM_RS guard (C3). */
void        forthInteractiveEnter(void);

/* L1-H: the FHIST interactive-history program — push, cap, evict, recall.
 * Defined in programming/manage.c beside the other capture orchestrators
 * (forthCaptureSuspend/Resume, forthInteractiveEnter). */

/* Push `line` onto FHIST (creating it on first use).  C1 (ENTER) and C2
 * (EXIT rung 3) both call it BEFORE the line is lost (run or discard).
 * A no-op when `line` is empty; silent (never blocks the caller) when the
 * program cannot be created or grown — history is a convenience, never an
 * error that blocks a run. */
void     forthHistoryPush(const char *line);

/* Program number of the FHIST program, or 0 if it does not exist yet. */
uint16_t forthHistoryProgram(void);

/* Creates FHIST (LBL 'FHIST' + END, appended after every existing program)
 * if it does not already exist.  Returns false only if creation failed.
 * Idempotent: returns true immediately if FHIST already exists. */
bool_t   forthHistoryEnsure(void);

/* Parks currentStep on FHIST's own END step (i.e. immediately before it),
 * ready for an _insertInProgram-style append as FHIST's newest line.
 * Returns false if FHIST does not exist.  L1-F1 (the fold) calls this too,
 * to park its transient step there. */
bool_t   forthHistoryGotoLastStep(void);

/* Deletes FHIST's oldest source steps (oldest-first) until its byte size
 * is at or under FORTH_HISTORY_MAX_BYTES.  A no-op if FHIST does not exist
 * or is already under the cap. */
void     forthHistoryEvict(void);

/* f-shifted up/down recall: moves the transient browse index by `delta`
 * (-1 for up/older, +1 for down/newer) and copies the resulting FHIST line
 * (or clears to empty, "past the newest") into aimBuffer.  Read-only: does
 * not create or modify FHIST.  Called from keyboard.c's CHR_caseUP/
 * CHR_caseDN arms, guarded on forthCapIsInteractive(). */
void     forthHistoryRecall(int16_t delta);

/* L1-H: the raw browse-index field — forthHistoryRecall's own state, plus
 * the two reset points (capture open, in _forthCapOpenAs; and every
 * forthHistoryPush()). */
uint16_t forthCapHistoryIndex(void);
void     forthCapSetHistoryIndex(uint16_t idx);

/* L1-1 (C2b): public wrappers for the file-static E1 catalog-drain helpers
 * (programming/manage.c:1165-1166, defined at :1689/:1701) — forthCapOpenInteractive's
 * caller (fnForthOuter, forth_compile.c) needs them too. */
bool_t   forthCatalogMenuOnTop(void);
bool_t   forthCatalogBuriedOnStack(void);

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
uint8_t     forthTestCapOrigin(void); /* L1-1: raw field, so the E14 sweep is
                                          falsifiable */
#endif

#endif
