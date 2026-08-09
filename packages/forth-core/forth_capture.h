#ifndef FORTH_CAPTURE_H
#define FORTH_CAPTURE_H

#include "c47.h"

/* CONSOLIDATE P5: file-statics exported to the self-test build only;
 * production linkage unchanged.  One definition — keyboard.c and
 * screen.c each carried a private copy of this idiom. */
#if defined(FORTH_DEBUG_SELFTEST)
  #define FORTH_SELFTEST_EXPORT
#else
  #define FORTH_SELFTEST_EXPORT static
#endif

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
  /* AUDIT C17: the homePushed bit that lived here is gone.  "Did the open
   * displace the user's top frame" is ownership information about a FRAME,
   * and a bit on the capture object can say whether the console owns *a*
   * frame but never *which* — the difference is exactly C17 (the user's own
   * FWRD frame consumed after a CATALOG-tree open), and the bit had to be
   * hand-preserved across every capture reopen (the C3 family; two sites,
   * each missed once).  Ownership now rides the frame itself: forth_menu.c
   * stamps the registered frame's userMenuId (owned vs borrowed), the EXIT
   * ladder and the restore path test the stamp, and forthCapClose() clears
   * it.  Nothing here to reset, preserve, or persist. */
  uint8_t     foldMode;       /* L1-F1: 0 = none, 1 = FOLD (bracket armed),
                                 2 = PARK (materialised, bracket NOT armed).
                                 OWNED BY forthFoldEnter/forthFoldLeave, and
                                 by forthCapPowerReset() ONLY.  forthCapOpen,
                                 forthCapClose and forthCapAbandonSuspended
                                 MUST NOT touch it: forthCaptureResume calls
                                 both forthCapOpen() and, on the canary path,
                                 forthCapAbandonSuspended(), immediately
                                 before F2 will call forthFoldLeave(), whose
                                 first line early-returns on foldMode == 0 —
                                 clearing it at those sites would make the
                                 fold unable to unwind after a resume that
                                 abandoned the suspension. */
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
bool_t      forthCapInteractiveLive(void); /* AUDIT round 6 (F8/D7-2): origin
                                             INTERACTIVE and state OPEN — the
                                             "is there a live line" question.
                                             IsInteractive above answers
                                             ORIGIN and is deliberately TRUE
                                             while SUSPENDED (TAM owns
                                             aimBuffer then); every render,
                                             route or gate site that means
                                             "live" must ask THIS one. */
uint8_t     forthCapOriginRaw(void);     /* L1-1: the raw field — for the
                                             resume bracket */
void        forthCapSetOrigin(uint8_t o);/* L1-1: the raw field — for the
                                             resume bracket */
uint8_t     forthCapFoldModeRaw(void);     /* L1-F1: the raw field — for
                                               forthFoldEnter/forthFoldLeave,
                                               which live in manage.c (they
                                               need the file-static insert/
                                               step-build helpers there) and
                                               so cannot reach the static
                                               forthCap object directly */
void        forthCapSetFoldModeRaw(uint8_t m); /* L1-F1: ditto */
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

#if defined(FORTH_DEBUG_SELFTEST)
/* AUDIT round 8 (P-2, owner ruling): forces the "creation failed" return so
 * a fixture can execute the foldMode-0 family.  Selftest builds only; see
 * the banner at the definition in programming/manage.c. */
extern bool_t forthHistoryEnsureFailInjected;
#endif

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
 * CHR_caseDN arms, guarded on forthCapInteractiveLive() — round 6's F6 fix
 * moved them off forthCapIsInteractive() so recall is refused while the
 * capture is SUSPENDED and TAM owns aimBuffer.  (AUDIT round 8, C-6: this
 * sentence still named the old predicate, which is false data for the next
 * reader enumerating the Live sites.) */
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
void     forthCapSuspendStepOffset(uint32_t off);  /* L1-F2 rev 3: fold recovery */
uint16_t forthCapSavedStepCount(void);  /* F6-4 */
void     forthCapAbandonSuspended(void);

/* F6-2: orchestrators (programming/manage.c — need the file-static
 * _closeAlphaMenus) */
void     forthCaptureSuspend(void);
void     forthCaptureResume(void);
void     forthCaptureSanitizeRestoredUi(void);

/* F6-3's shared inserter forthCapInsertName() now lives in forth_menu.h
 * alongside the picker that is its main caller (S2). */

/* CONSOLIDATE P2: "can this item be inserted into a capture as text" — the
 * one predicate behind pemAlpha's PEM arm and runFunction's interactive
 * divert, which used to carry forked copies of the exclusion list.  Pass
 * interactive = true from the raw key stream (runFunction), false from the
 * PEM arm, whose earlier arms have already consumed ENTER / BACKSPACE /
 * EXIT / R-S.  See the definition in forth_capture.c for why item > 0 is
 * load-bearing rather than defensive. */
bool_t   forthCapNameInsertEligible(int16_t item, bool_t interactive);

/* L1-F1: the fold context — materialise a real ITM_FORTH capture step in
 * FHIST (L1-H's program), seeded with the live line, then (F2) let calcMode
 * = CM_PEM for the duration of a TAM run the landed PEM step-insert
 * machinery unmodified.  Defined in programming/manage.c beside the other
 * capture orchestrators (forthCaptureSuspend/Resume, forthInteractiveEnter,
 * the FHIST group) — they need the same file-static helpers.
 * forthFoldEnter/forthFoldLeave are a bracket: every forthFoldEnter that
 * returns with forthFoldPending() true must be matched by exactly one
 * forthFoldLeave.  Inert until F2 wires a caller — this packet proves the
 * pair only via its own self-test, which calls them directly. */
void   forthFoldEnter(int16_t func, uint16_t mode);
void   forthFoldLeave(void);
void   forthFoldUnwindIfDone(void);  /* L1-F2 rev 3: resume+leave, once tam.mode is 0 */
bool_t forthFoldArmed(void);     /* foldMode == 1 (FOLD: bracket armed) */
bool_t forthFoldPending(void);   /* foldMode != 0 (FOLD or PARK) */

/* AUDIT round 8 (C-1): admission was decided from (tam.function, tam.mode)
 * at forthFoldEnter, so EVERY mid-session rewrite of tam.function must
 * re-derive it — the round-6 F1 fix re-derived at the GTO->GTOP promotion
 * and missed the sibling BACKSPACE demotion, which silently lost the
 * committed operation.  Call this at every tam.function rewrite site
 * instead of writing foldMode by hand; it no-ops when no fold is pending.
 * The site count is pinned in design-audit.sh group I. */
void   forthFoldRederiveAdmission(int16_t func, uint16_t mode);

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
