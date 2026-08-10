#ifndef FORTH_CAPTURE_H
#define FORTH_CAPTURE_H

#include "c47.h"

/* File-statics exported to the self-test build only; production linkage
 * unchanged. */
#if defined(FORTH_DEBUG_SELFTEST)
  #define FORTH_SELFTEST_EXPORT
#else
  #define FORTH_SELFTEST_EXPORT static
#endif

/* Forth capture state.
 *
 * The capture line lives in `aimBuffer`, the native PEM/AIM input sink,
 * exactly like the REM and LITERAL captures it sits beside.  There is no
 * separate buffer and no allocation: the on-disk program step is the
 * single source of truth across a suspension — suspend recommits, resume
 * refills from the step payload.  aimBuffer is 1024 bytes; the capture
 * cap is 256 bytes / 196 glyphs enforced at the insertion sites.
 *
 * NOT derivable from calcMode/FLAG_ALPHA/tam.function: tamEnterMode
 * assigns the INCOMING TAM function before the CM_PEM suspend seam fires,
 * so tam.function is already the TAM op (not ITM_FORTH) at the one place
 * suspend must recognise an open capture.  Hence the explicit state. */

typedef enum { FCAP_CLOSED = 0, FCAP_OPEN = 1, FCAP_SUSPENDED = 2 } forthCapState_t;

/* Where the capture was opened from. */
typedef enum { FCAP_ORIGIN_PEM = 0, FCAP_ORIGIN_INTERACTIVE = 1 } forthCapOrigin_t;

typedef struct {
  uint8_t     state;          /* forthCapState_t */
  uint8_t     keysMode;       /* 0 = alpha input, 1 = keys.  Transient UI
                                 state, NEVER persisted; meaningful only
                                 while state == FCAP_OPEN. */
  uint8_t     origin;         /* forthCapOrigin_t.  PEM captures live on a
                                 program step; INTERACTIVE captures live on
                                 the AIM surface.  Transient, NEVER
                                 persisted.  Zero is PEM so every zero-init
                                 and memset-style reset means "PEM". */
  /* Frame ownership does NOT live here: a bit on the capture object can
   * say whether the console owns *a* softmenu frame but never *which*.
   * Ownership rides the frame itself — forth_menu.c stamps the registered
   * frame's userMenuId, and forthCapClose() clears the stamps. */
  uint8_t     foldMode;       /* 0 = none, 1 = FOLD (bracket armed),
                                 2 = PARK (materialised, bracket NOT armed).
                                 OWNED BY forthFoldEnter/forthFoldLeave, and
                                 by forthCapPowerReset() ONLY.  forthCapOpen,
                                 forthCapClose and forthCapAbandonSuspended
                                 MUST NOT touch it: forthCaptureResume calls
                                 both immediately before forthFoldLeave(),
                                 whose first line early-returns on
                                 foldMode == 0 — clearing it at those sites
                                 would make the fold unable to unwind after
                                 a resume that abandoned the suspension. */
  /* Suspend snapshot — meaningful only in FCAP_SUSPENDED: */
  uint16_t    savedCursor;    /* T_cursorPos at suspend */
  uint16_t    savedLocalStep; /* currentLocalStepNumber at suspend */
  uint32_t    savedStepOffset;/* capture step vs beginOfProgramMemory
                                 (offset: program memory may relocate) */
  uint16_t    savedStepCount; /* getNumberOfSteps() at suspend, so resume
                                 can tell how many steps a TAM commit
                                 inserted */
  uint16_t    historyIndex;   /* recall browse index into FHIST —
                                 transient, NEVER persisted.
                                 FORTH_HIST_BROWSE_NONE ("past the newest")
                                 outside a browse; reset at every capture
                                 open and every forthHistoryPush(). */
} forthCap_t;

/* The recall browse-index sentinel ("past the newest" / not currently
 * browsing).  Distinguishable from any real line index: FHIST's line
 * count is bounded well under this by FORTH_HISTORY_MAX_BYTES (each line
 * consumes at least 5 bytes). */
#define FORTH_HIST_BROWSE_NONE ((uint16_t)0xFFFFu)

void        forthCapOpen(void);       /* state FCAP_OPEN; clears aimBuffer.
                                         Cannot fail (nothing is allocated) */
void        forthCapOpenInteractive(void);  /* open with INTERACTIVE origin */
void        forthCapClose(void);      /* state FCAP_CLOSED; safe if already
                                         closed */
bool_t      forthCapIsOpen(void);     /* state == FCAP_OPEN */
bool_t      forthCapIsInteractive(void); /* origin == INTERACTIVE &&
                                             state != CLOSED */
bool_t      forthCapInteractiveLive(void); /* origin INTERACTIVE and state
                                             OPEN — the "is there a live
                                             line" question.  IsInteractive
                                             above answers ORIGIN and is
                                             deliberately TRUE while
                                             SUSPENDED (TAM owns aimBuffer
                                             then); every render, route or
                                             gate site that means "live"
                                             must ask THIS one. */
uint8_t     forthCapOriginRaw(void);     /* raw field — for the resume
                                             bracket */
void        forthCapSetOrigin(uint8_t o);/* raw field — for the resume
                                             bracket */
uint8_t     forthCapFoldModeRaw(void);     /* raw field — for the fold
                                               bracket */
void        forthCapSetFoldModeRaw(uint8_t m);
bool_t      forthCapTextNonEmpty(void); /* open && aimBuffer[0] != 0 */
bool_t      forthCapKeysMode(void);        /* keys-mode bit */
void        forthCapSetKeysMode(bool_t on);

/* R/S's orchestrator for an interactive capture — runs the line, then
 * either reopens empty (REPL) or reopens with the line intact for
 * correction (error).  ENTER inserts a literal space; the ITM_RS guard
 * calls this routine to run the completed line. */
void        forthInteractiveRun(void);

/* The FHIST interactive-history program — push, cap, evict, recall.
 * Defined in programming/forth_fold.c. */

/* Push `line` onto FHIST (creating it on first use).  R/S and EXIT
 * rung 3 both call it BEFORE the line is lost (run or discard).  A no-op
 * when `line` is empty; silent when the program cannot be created or
 * grown — history is a convenience, never an error that blocks a run. */
void     forthHistoryPush(const char *line);

/* Program number of the FHIST program, or 0 if it does not exist yet. */
uint16_t forthHistoryProgram(void);

/* Creates FHIST (LBL 'FHIST' + END, appended after every existing program)
 * if it does not already exist.  Returns false only if creation failed.
 * Idempotent: returns true immediately if FHIST already exists. */
bool_t   forthHistoryEnsure(void);

#if defined(FORTH_DEBUG_SELFTEST)
/* Forces the "creation failed" return so a fixture can execute the
 * foldMode-0 family.  Selftest builds only; see the definition. */
extern bool_t forthHistoryEnsureFailInjected;
#endif

/* Parks currentStep on FHIST's own END step (i.e. immediately before it),
 * ready for an _insertInProgram-style append as FHIST's newest line.
 * Returns false if FHIST does not exist.  The fold calls this too, to
 * park its transient step there. */
bool_t   forthHistoryGotoLastStep(void);

/* Deletes FHIST's oldest source steps (oldest-first) until its byte size
 * is at or under FORTH_HISTORY_MAX_BYTES.  A no-op if FHIST does not exist
 * or is already under the cap.  Renumbers the push bracket's saved cursor
 * as it deletes (the deleter-adjusts convention).  Returns false when the
 * loop abandoned on an error — labelList/programList are then unsafe and
 * the caller must not restore a cursor through them. */
bool_t   forthHistoryEvict(void);

/* f-shifted up/down recall: moves the transient browse index by `delta`
 * (-1 for up/older, +1 for down/newer) and copies the resulting FHIST line
 * (or clears to empty, "past the newest") into aimBuffer.  Read-only: does
 * not create or modify FHIST.  Called from keyboard.c's CHR_caseUP/
 * CHR_caseDN arms, guarded on forthCapInteractiveLive() so recall is
 * refused while the capture is SUSPENDED and TAM owns aimBuffer. */
void     forthHistoryRecall(int16_t delta);

/* The raw browse-index field — forthHistoryRecall's own state, plus the
 * two reset points (capture open and every forthHistoryPush()). */
uint16_t forthCapHistoryIndex(void);
void     forthCapSetHistoryIndex(uint16_t idx);

/* Public wrappers for the file-static catalog-drain helpers in the
 * manage.c override — fnForthOuter needs them too. */
bool_t   forthCatalogMenuOnTop(void);
bool_t   forthCatalogBuriedOnStack(void);

/* Suspend/resume state ops */
void     forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset, uint16_t stepCount);
bool_t   forthCapIsSuspended(void);
uint16_t forthCapSavedCursor(void);
uint16_t forthCapSavedLocalStep(void);
uint32_t forthCapSavedStepOffset(void);
void     forthCapSuspendStepOffset(uint32_t off);  /* fold recovery */
uint16_t forthCapSavedStepCount(void);
void     forthCapAbandonSuspended(void);

/* manage.c seams: the fold/history subsystem (programming/forth_fold.c)
 * needs exactly two of manage.c's file-statics; these 3-line wrappers
 * export them.  Package code only — nothing upstream calls them. */
void     forthPkgInsertInProgram(const uint8_t *dat, uint16_t size);
void     forthPkgCloseAlphaMenus(void);

/* §8.1: build an ITM_FORTH capture step for `text` into `dst`, returning
 * its byte length.  Empty text emits the OPEN-CAPTURE PLACEHOLDER (len 1,
 * one 0x00 payload byte), never a len==0 region marker — see the
 * definition for why that distinction is load-bearing. */
uint16_t forthCapBuildStep(char *dst, const char *text);

/* The fold context holds a saved program number, so the DELETER tells it
 * which program went — upstream's own convention for saved cursors.
 * Called from _clearProgram in the manage.c override; defined with the
 * fold context in programming/forth_fold.c. */
void     _forthFoldNoteProgramDeleted(uint16_t deletedProgramNumber);

/* Orchestrators.  forthCaptureSuspend/Resume live in
 * programming/forth_fold.c; forthCaptureSanitizeRestoredUi stays in the
 * manage.c override — it calls _closeAlphaMenus directly. */
void     forthCaptureSuspend(void);
void     forthCaptureResume(void);
void     forthCaptureSanitizeRestoredUi(void);

/* forthCapInsertName() lives in forth_menu.h alongside the picker that is
 * its main caller. */

/* "Can this item be inserted into a capture as text" — the one predicate
 * behind pemAlpha's PEM arm and runFunction's interactive divert.  Pass
 * interactive = true from the raw key stream (runFunction), false from
 * the PEM arm, whose earlier arms have already consumed ENTER /
 * BACKSPACE / EXIT / R-S.  See the definition in forth_capture.c for why
 * item > 0 is load-bearing rather than defensive. */
bool_t   forthCapNameInsertEligible(int16_t item, bool_t interactive);

/* The fold context — materialise a real ITM_FORTH capture step in FHIST,
 * seeded with the live line, then let calcMode = CM_PEM for the duration
 * of a TAM run the PEM step-insert machinery unmodified.  Defined in
 * programming/forth_fold.c.  forthFoldEnter/forthFoldLeave are a bracket:
 * every forthFoldEnter that returns with forthFoldPending() true must be
 * matched by exactly one forthFoldLeave. */
void   forthFoldEnter(int16_t func, uint16_t mode);
void   forthFoldLeave(void);
void   forthFoldUnwindIfDone(void);  /* resume+leave, once tam.mode is 0 */
bool_t forthFoldArmed(void);     /* foldMode == 1 (FOLD: bracket armed) */
bool_t forthFoldPending(void);   /* foldMode != 0 (FOLD or PARK) */

/* Admission is decided from (tam.function, tam.mode) at forthFoldEnter,
 * so EVERY mid-session rewrite of tam.function must re-derive it.  Call
 * this at every tam.function rewrite site instead of writing foldMode by
 * hand; it no-ops when no fold is pending.  The site count is pinned in
 * design-audit.sh group I. */
void   forthFoldRederiveAdmission(int16_t func, uint16_t mode);

/* Capture state cannot outlive the dictionary lifecycle.  Called at the
 * same seams as forthScanTrackReset (init / clear / restore validation):
 * a restored or re-initialized machine starts CLOSED.  Deep-sleep wake on
 * hardware does NOT run these seams — a sleeping capture legitimately
 * survives, matching FLAG_ALPHA. */
void     forthCapPowerReset(void);

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t     forthTestCapState(void);
const char *forthTestCapText(void);   /* "" when not open */
uint8_t     forthTestCapOrigin(void); /* raw field, so the origin sweep is
                                          falsifiable */
#endif

#endif
