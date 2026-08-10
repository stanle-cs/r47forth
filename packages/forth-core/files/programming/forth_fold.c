// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file forth_fold.c
 * The capture orchestrators, the FHIST history program and the fold
 * context.  None of this is upstream code; it reaches the manage.c
 * override's statics only through the forthPkgInsertInProgram /
 * forthPkgCloseAlphaMenus seams.
 ***********************************************/

#include "c47.h"
#include "forth_dict.h"
#include "forth_capture.h"
#include "forth_console.h"
#include "forth_menu.h"

/* §8.1: an ITM_FORTH capture step with empty text is the OPEN-CAPTURE
 * PLACEHOLDER — len=1, single 0x00 payload byte — categorically distinct
 * from a len==0 region marker.  Every ITM_FORTH capture emit goes through
 * here: emitting len==0 instead would alias a marker and flip the parity
 * of every marker after the cursor. */
uint16_t forthCapBuildStep(char *dst, const char *text) {
  uint16_t n = stringByteLength(text);
  dst[0] = (ITM_FORTH >> 8) | 0x80;
  dst[1] =  ITM_FORTH       & 0xff;
  dst[2] = (char)STRING_LABEL_VARIABLE;
  if(n == 0) {
    dst[3] = 1;
    dst[4] = 0;
    return 5;
  }
  dst[3] = n;
  xcopy(dst + 4, text, n);
  return n + 4;
}

/* Steps the resume splice deliberately KEPT (oversize decode, no room in
 * the line).  forthFoldLeave's debris sweep counts them into its threshold
 * instead of deleting them.  Set by forthCaptureResume, consumed and
 * cleared by forthFoldLeave, reset by forthFoldEnter. */
static uint16_t _forthFoldKeptSteps = 0;

/* Re-establish the per-key recommit invariant — the on-disk capture step
 * mirrors aimBuffer.  Callers guarantee currentStep is ON the capture
 * step, and a capture step's opcode is always the 2-byte ITM_FORTH form. */
static void forthCapRecommitStep(void) {
  deleteStepsFromTo(currentStep, findNextStep(currentStep));
  forthPkgInsertInProgram((uint8_t *)tmpString, forthCapBuildStep(tmpString, aimBuffer));
  --currentLocalStepNumber;
  currentStep = findPreviousStep(currentStep);
}

void forthCaptureSuspend(void) {
  if (!forthCapIsOpen()) { return; }
  /* Recommit before snapshotting the step offset below; a redundant
   * recommit of an in-sync step is byte-neutral. */
  forthCapRecommitStep();
  uint16_t cursor    = T_cursorPos;
  uint16_t localStep = currentLocalStepNumber;
  uint32_t stepOff   = (uint32_t)(currentStep - beginOfProgramMemory);
  /* currentStep stays ON the capture step: TAM commits insert via
   * addStepInProgram, whose pre-move already places the new step AFTER
   * the current one — moving here would shift the TAM insert one step
   * too late.  tam.function is NOT touched: tamEnterMode assigned the
   * incoming TAM function before this seam; zeroing it would break the
   * TAM session. */
  clearSystemFlag(FLAG_ALPHA);
  calcModeNormalGui();
  forthPkgCloseAlphaMenus();
  forthCapSuspendState(cursor, localStep, stepOff, getNumberOfSteps());
}

/* Recover the capture step when the saved offset no longer describes it.
 *
 * An interactive fold can see a SECOND commit inside ONE fold window: a
 * menu_TamSto softkey supersedes the armed item and its own TAM commit
 * inserts a step while the cursor is still parked on the capture step.
 * Every fold-window commit inserts AFTER the capture step — forthFoldEnter
 * forces pemCursorIsZerothStep = false precisely so addStepInProgram's
 * pre-move steps the cursor forward off it — which is why the resume
 * splice scans forward.  PEM cannot produce this (its TAM commits exactly
 * once per suspension), so the recovery is gated on forthFoldPending()
 * and PEM keeps abandon-on-canary.
 *
 * The capture step is the LAST ITM_FORTH step in FHIST: forthFoldEnter
 * appends it immediately before FHIST's END, so every history line
 * precedes it, and the interloper (a native TAM step) is not ITM_FORTH at
 * all.  Returns NULL when FHIST is absent or holds no ITM_FORTH step. */
static uint8_t *_forthFoldFindCaptureStep(void) {
  uint16_t prog = forthHistoryProgram();
  uint8_t *step, *last = NULL;
  if (prog == 0) { return NULL; }
  step = programList[prog - 1].instructionPointer;
  for (int i = 0; i < 512 && step != NULL; i++) {
    uint8_t *next;
    if (isAtEndOfPrograms(step) || isAtEndOfProgram(step)) { break; }
    if (checkOpCodeOfStep(step, ITM_FORTH) && step[2] == (uint8_t)STRING_LABEL_VARIABLE) {
      last = step;
    }
    next = findNextStep(step);
    if (next == NULL || next <= step) { break; }
    step = next;
  }
  return last;
}

/* The capture step's SHAPE test: an ITM_FORTH step carrying a variable
 * string payload, inside program memory.  Necessary, never sufficient — a
 * Forth step inside a user's own program satisfies it exactly.  Use
 * _forthStepIsCaptureStepInHistory below unless the caller has no FHIST
 * rule to apply (a PEM capture does not). */
static bool_t _forthStepHasCaptureShape(const uint8_t *p) {
  return (bool_t)(p != NULL
                  && p < firstFreeProgramByte
                  && checkOpCodeOfStep((uint8_t *)p, ITM_FORTH)
                  && p[2] == (uint8_t)STRING_LABEL_VARIABLE);
}

/* The ONE spelling of the structural rule: the capture step lies INSIDE
 * FHIST, because the capture step is only ever created there.  Every
 * consumer takes the rule from this predicate; a new consumer must call
 * it, never restate the bound.  The `p >= from && p < to` bound is
 * defence in depth: the one door that grows program memory during a
 * suspension inserts at firstFreeProgramByte, above every program, so
 * FHIST never shifts underneath a suspension. */
static bool_t _forthStepIsCaptureStepInHistory(const uint8_t *p) {
  uint16_t hist = forthHistoryProgram();
  const uint8_t *from, *to;

  if(hist == 0)                        { return false; }
  if(!_forthStepHasCaptureShape(p))    { return false; }

  from = programList[hist - 1].instructionPointer;
  to   = (hist < numberOfPrograms) ? programList[hist].instructionPointer
                                   : firstFreeProgramByte;
  return (bool_t)(p >= from && p < to);
}

void forthCaptureResume(void) {
  if (!forthCapIsSuspended()) { return; }
  uint8_t *p = beginOfProgramMemory + forthCapSavedStepOffset();
  /* For a pending FOLD the saved address must also lie inside FHIST; when
   * it does not, fall through to the FHIST-scan recovery below, whose
   * hist == 0 arm abandons.  A PEM capture (no fold) has no FHIST rule —
   * its step lives in the program being edited — and keeps
   * abandon-on-canary.  The PEM sibling of this door (DELP from a PEM
   * TAM, alignment onto another program's Forth step) is a recorded open
   * design question, not silently fixed here. */
  bool_t pValid = forthFoldPending() ? _forthStepIsCaptureStepInHistory(p)
                                     : _forthStepHasCaptureShape(p);
  if (!pValid) {
    /* An interactive fold can shift the capture step off the saved offset
     * (see _forthFoldFindCaptureStep).  Recover rather than abandon — but
     * ONLY for a fold; PEM keeps abandon-on-canary. */
    uint8_t *recovered = forthFoldPending() ? _forthFoldFindCaptureStep() : NULL;
    if (recovered != NULL) {
      p = recovered;
      forthCapSuspendStepOffset((uint32_t)(p - beginOfProgramMemory));
    }
    else {
    forthCapAbandonSuspended();             /* defensive canary */
    #if defined(FORTH_DEBUG_SELFTEST)
    printf("FORTH CANARY: suspended capture step falsified; suspension abandoned\n");
    #endif
    return;
    }
  }
  { bool_t keysWas   = forthCapKeysMode();  /* resume is not a fresh capture —
                                               the sub-mode the user keyed the
                                               TAM item from comes back with
                                               the line */
    uint8_t originWas = forthCapOriginRaw();/* forthCapOpen() zeroes origin to
                                               PEM; this is the SUSPENDED→OPEN
                                               re-open, not a PEM open, so
                                               origin must survive it exactly
                                               like keysMode does */
    forthCapOpen();                         /* SUSPENDED → OPEN; clears aimBuffer,
                                               which TAM may have used meanwhile */
    forthCapSetKeysMode(keysWas);
    forthCapSetOrigin(originWas);
  }
  { uint8_t len = p[3];                     /* §8.1: the empty placeholder is
                                               len=1 payload 0x00 — the xcopy
                                               yields "" by construction */
    if (len > 0) { xcopy(aimBuffer, p + 4, len); }
    aimBuffer[len] = 0;
    T_cursorPos = forthCapSavedCursor();
    if (T_cursorPos > stringByteLength(aimBuffer)) { T_cursorPos = stringByteLength(aimBuffer); }
  }
  currentLocalStepNumber = forthCapSavedLocalStep();
  currentStep = p;
  /* A TAM item that ran LIVE inside the fold (the GTO→GTOP promotion) can
   * leave currentProgramNumber on ANOTHER program, and getNumberOfSteps()
   * is keyed entirely on currentProgramNumber — without re-anchoring, the
   * splice below subtracts two different programs' step counts and the
   * uint16_t underflow drives deleteStepsFromTo through a wild delete.
   * Re-anchor to the program that actually contains the validated capture
   * step, then clamp and guard like the sweep. */
  defineCurrentProgramFromCurrentStep();
  /* Steps the suspended TAM committed become canonical text.  n is 0
   * (cancel) or 1 (one commit) today; the loop is defensive. */
  { uint16_t total = getNumberOfSteps();
    uint16_t saved = forthCapSavedStepCount();
    uint16_t n     = (total > saved) ? (uint16_t)(total - saved) : 0;
    uint16_t kept  = 0;
    bool_t folded = false;
    while (n > 0) {
      uint8_t *ins = findNextStep(currentStep);   /* first inserted step */
      if (ins == NULL || isAtEndOfProgram(ins) || isAtEndOfPrograms(ins)) {
        break;   /* the count over-ran reality */
      }
      decodeOneStep(ins);                          /* canonical text → tmpString */
      if (stringByteLength(tmpString) > 255) {
        kept = n;  /* defensive: keep the step rather than truncate text */
        break;
      }
      /* the leading separator lives in forthCapInsertName itself
       * (token-boundary guard) — pass the decoded text straight through */
      if (!forthCapInsertName(tmpString)) {
        kept = n;  /* no room: keep this and later steps after the line —
                      and TELL the sweep, or it deletes what this arm just
                      promised to keep */
        break;
      }
      deleteStepsFromTo(ins, findNextStep(ins));
      folded = true;
      --n;
    }
    _forthFoldKeptSteps = kept;
    if (folded) {
      /* forthCapInsertName wrote into aimBuffer only — recommit so the
       * on-disk step holds the folded text; commit paths entered with no
       * intervening keystroke trust the per-key invariant. */
      forthCapRecommitStep();
    }
  }
  tam.function = ITM_FORTH;                 /* capture-era tam is exactly
                                                {mode 0, function ITM_FORTH} */
  resetShiftState();                        /* fresh-open parity */
  setSystemFlag(FLAG_ALPHA);
  calcModeAimGui();
  /* A catalog-initiated TAM buries its catalog menus under the TAM menu,
   * and the NEXT softkey dispatch's _closeCatalog would eat the -MNU_ALPHA
   * we are about to push.  Stack-wide predicate + bounded loop:
   * popSoftmenu() can re-push HOME, so never spin on the predicate. */
  for(int i = 0; i < SOFTMENU_STACK_SIZE
                 && (forthCatalogMenuOnTop() || forthCatalogBuriedOnStack());
      i++) {
    popSoftmenu();
  }
  if(forthCapIsInteractive()) {
    /* Re-establish the row THROUGH THE OWNER — a raw showSoftmenu push
     * here leaves the resumed excursion row unregistered.
     * forthConsoleRestoreSurface: stamp alive somewhere → the ownership
     * rules decide; stamp gone → acquire and register.  In keys mode it
     * is a no-op on the intact FWRD base. */
    forthConsoleRestoreSurface();
  }
  else if(!forthCapKeysMode()) {
    showSoftmenu(-MNU_ALPHA);   /* PEM resume: the native alpha row */
  }
  pemCursorIsZerothStep = false;
}


/* R/S's orchestrator for an interactive Forth capture.  calcMode stays
 * CM_AIM throughout — no calcModeNormal(), no closeAim(), no popSoftmenu()
 * on this path: run the line, decide whether to reopen empty or reopen
 * with the line intact.
 *
 * The pre-run copy is mandatory (§3.3.2): a word the line executes can
 * rewrite aimBuffer, because aimBuffer is also the NIM buffer.
 * forthOuterInterpret's own copy protects ITS parse, not this function's
 * error-path read-back — that must come from a copy taken before the
 * run. */
void forthInteractiveRun(void) {
  if (aimBuffer[0] == 0) {
    /* Empty R/S is a no-op, NOT a close: EXIT is the close gesture. */
    return;
  }

  /* Refuse the commit atomically: capture stays open with the line intact
   * for correction, error already displayed — the same gate the PEM
   * arm uses. */
  if (!forthCheckSourceLine(aimBuffer)) {
    return;
  }

  /* Push BEFORE the run: an executed word can rewrite aimBuffer (§3.3.2),
   * so the text must be captured while it is still the user's line. */
  forthHistoryPush(aimBuffer);
  /* The line echo and the FHIST push are ONE ACT — same bytes, same site,
   * ordered together before the run: after the refusal above (a refused
   * line neither echoes nor enters history) and before the run (a word
   * that rewrites aimBuffer cannot change what was echoed).  A second
   * echo writer, a reorder against this push, or an echo on a path the
   * push skips would make the transcript lie about history. */
  { char echo[FORTH_CONSOLE_FMT_MAX];
    /* snprintf truncates on a BYTE boundary, so a near-maximal line would
     * end the echo record with a lone lead byte.  Build the prefix, then
     * copy the line on a GLYPH boundary into what is left. */
    int32_t at = (int32_t)stringByteLength(STD_RIGHT_DOUBLE_ANGLE " ");
    xcopy(echo, STD_RIGHT_DOUBLE_ANGLE " ", (uint32_t)at);
    forthCopyWholeGlyphs(echo + at, aimBuffer, (int32_t)sizeof(echo) - at);
    forthConsoleAppendLine(echo);
  }

  /* Mandatory pre-run snapshot — see the banner.  256 bytes matches the
   * capture cap enforced at every insertion site, so this copy can never
   * truncate a line the cap already accepted. */
  char preRunCopy[256];
  {
    int32_t n = stringByteLength(aimBuffer);
    xcopy(preRunCopy, aimBuffer, n + 1);
  }

  { uint32_t seqBefore = forthConsoleWriteSeq();
  forthOuterInterpret(aimBuffer);

  /* Restore the capture's own input surface.  A native item the line ran
   * can call calcModeNormal() (CLSTK does), leaving the capture open but
   * off the AIM surface: keys stop routing through it and the editor
   * stops drawing.  Repaired here because this is the one choke point
   * that knows a capture is still open and runs on every path out of a
   * committed line.  Interactive origin only — a PEM capture lives on a
   * program step, not on this surface. */
  if (forthCapInteractiveLive()) {
    if (calcMode != CM_AIM) {
      calcMode = CM_AIM;
      setSystemFlag(FLAG_ALPHA);
      cursorEnabled = true;
      calcModeAimGui();
    }
    /* calcModeNormal() can also POP the console's own frame, and the
     * SURFACE repair is deliberately not gated on the MODE repair: a line
     * can damage the row without leaving CM_AIM (EXITALL pops every frame
     * and never touches calcMode).  forthConsoleRestoreSurface() is a
     * no-op when the frame is intact. */
    forthConsoleRestoreSurface();
  }

  if (lastErrorCode != ERROR_NONE) {
    /* The error echo.  §8.7's error protocol is unchanged — the native
     * paint still covers the area until the next key; the transcript line
     * keeps the dialogue readable afterwards.  Close the word's own open
     * output record FIRST: appending into it merges the message onto the
     * output row, where wide output pushes it off the right edge.  The
     * success arm below already closes before its echo; the two post-run
     * arms must agree on the invariant they re-establish. */
    if (forthConsoleHasOpenLine()) {
      forthConsoleNewline();
    }
    forthConsoleAppendLine(errorMessageOf(lastErrorCode));

    /* Reopen with the line intact so the user edits rather than retypes —
     * from the pre-run copy, not from aimBuffer. */
    int32_t n = stringByteLength(preRunCopy);
    xcopy(aimBuffer, preRunCopy, n + 1);
    T_cursorPos = stringLastGlyph(aimBuffer) + 1;   /* non-empty by construction */
    return;                                          /* capture stays OPEN */
  }

  /* The result echo — the calculator's "ok".  The stack is hidden while
   * the console is up, so the console answers with where X landed.
   * Suppressed when the line SPOKE FOR ITSELF — any console write during
   * the run, terminated or not: a write counter sampled across the run,
   * because "is a line still open" misses words that write and then
   * close. */
  if (forthConsoleWriteSeq() == seqBefore) {
    char shown[FORTH_CONSOLE_FMT_MAX];
    forthConsoleFormatRegister(REGISTER_X, shown, (int16_t)sizeof(shown));
    if (shown[0] != 0) {
      forthConsoleAppendLine(shown);
    }
  }
  else if (forthConsoleHasOpenLine()) {
    forthConsoleNewline();      /* close the word's own output line */
  }
  }

  /* REPL: reopen empty, stay in CM_AIM.  forthCapOpenInteractive clears
   * aimBuffer and resets keysMode. */
  forthCapOpenInteractive();
  forthCapSetKeysMode(true);   /* keys-first must survive every R/S, not
                                  just the first open */
  /* The row has to follow the sub-mode, or R/S from an alpha excursion
   * leaves the ALPHA keypad displayed while the keyboard already types
   * the keys plane. */
  forthConsoleShowSurface();
  T_cursorPos = 0;
  displayAIMbufferoffset = 0;
}


/* The ONE restore for a saved PEM cursor tuple — the fold context's and
 * the history push's.  Both are (program, localStep) pairs sampled before
 * a dispatch that can shorten or delete the program they name.
 *
 * The program half is MAINTAINED by the deleter (upstream's fnClP
 * convention); the clamp here is the crash guard for a shrink no deleter
 * announced.  The step half follows upstream's own answer for a cursor
 * its dispatch invalidated: honour the saved step when it still fits,
 * fall back to STEP 1 when it does not (_clearProgram restores step 1;
 * fnClP restores a saved localStep only where the cursor's program came
 * through intact).  goToGlobalStep's walk has no NULL break and no
 * iteration cap, so an unbounded restore can walk currentStep to NULL and
 * the next PEM insert then writes wild.
 *
 * Step 1 always exists, so the first navigation is unconditionally safe
 * and anchors getNumberOfSteps() to THIS program; the second runs only
 * inside the count it just measured.
 *
 * The dynamicMenuItem bracket lives here, once: goToGlobalStep with
 * dynamicMenuItem >= 0 is not a "go to this step" primitive at all — it
 * reinterprets the request as the label the dynamic menu names and
 * returns WITHOUT NAVIGATING when that does not resolve (§3.3.6), and
 * the softkey that commits a console TAM latches exactly that. */
static void _forthRestoreCursorTuple(uint16_t program, uint16_t localStep,
                                     uint16_t firstDisplayed, uint8_t zerothStep) {
  int16_t savedDynamicMenuItem;

  if(numberOfPrograms == 0) { return; }
  if(program < 1)                { program = 1; }
  if(program > numberOfPrograms) { program = numberOfPrograms; }

  savedDynamicMenuItem = dynamicMenuItem;
  dynamicMenuItem = -1;

  goToPgmStep(program, 1);                    /* always valid; anchors the count */
  if(localStep >= 1 && localStep <= getNumberOfSteps()) {
    /* The saved tuple is valid: restore EVERY field.  localStep == 1 is a
     * valid saved step (and the only pairing a zeroth-step cursor ever
     * has — upstream writes the flag and the 1 together), so "the second
     * navigation is redundant" must not be conflated with "the tuple is
     * invalid". */
    if(localStep > 1) {
      goToPgmStep(program, localStep);
    }
  }
  else {
    /* Upstream's answer for a cursor its own dispatch invalidated: stay
     * on step 1; the saved display window described a program state that
     * no longer exists, so it is not restored either. */
    firstDisplayed = 0;
    zerothStep     = 0;
  }
  firstDisplayedLocalStepNumber = firstDisplayed;
  defineFirstDisplayedStep();
  pemCursorIsZerothStep = zerothStep;

  dynamicMenuItem = savedDynamicMenuItem;
}


/* ==================================================================
 * The FHIST history program: push, cap, evict, recall.
 *
 * FHIST is a single, kept, named, runnable program that accumulates
 * interactive lines as ITM_FORTH source steps.  Created lazily (on the
 * first push) and appended AFTER every existing program, never spliced
 * into one — see the byte-layout note at forthHistoryEnsure().
 * ================================================================== */

#define FORTH_HISTORY_NAME     "FHIST"
#define FORTH_HISTORY_NAME_LEN 5

/* The cursor tuple.  (program, localStep) — NOT a saved global step
 * number, which program-boundary shifts (FHIST growing/evicting) would
 * make stale by restore time; the restore re-reads programList AFTER
 * scanLabelsAndPrograms has rebuilt it.  The tuple may name either a user
 * program or a step in package-owned FHIST; eviction repairs the latter's
 * local step after removing an older history entry. */
typedef struct {
  uint16_t savedProgram;          /* currentProgramNumber */
  uint16_t savedLocalStep;        /* currentLocalStepNumber */
  uint16_t savedFirstDisplayed;   /* firstDisplayedLocalStepNumber */
  uint8_t  savedZerothStep;       /* pemCursorIsZerothStep */
  uint8_t  pad;
} forthHistCursor_t;              /* 8 bytes, BSS, one instance */

static forthHistCursor_t _forthHistCur;

/* The line the owner was typing when browsing started.  Stashed on the
 * way out of the past-newest slot and restored on the way back in.
 * Strictly browse-local: lives in BSS, must not survive a suspension or
 * a restore, deliberately NOT in the persisted capture object. */
static char _forthHistScratch[FORTH_CONSOLE_LINE_MAX + 1];


static void _forthHistSaveCursor(void) {
  _forthHistCur.savedProgram        = currentProgramNumber;
  _forthHistCur.savedLocalStep      = currentLocalStepNumber;
  _forthHistCur.savedFirstDisplayed = firstDisplayedLocalStepNumber;
  _forthHistCur.savedZerothStep     = (uint8_t)pemCursorIsZerothStep;
}

static void _forthHistRestoreCursor(void) {
  /* Through the shared restore, which carries the dynamicMenuItem bracket
   * and the step bound. */
  _forthRestoreCursorTuple(_forthHistCur.savedProgram,
                           _forthHistCur.savedLocalStep,
                           _forthHistCur.savedFirstDisplayed,
                           _forthHistCur.savedZerothStep);
}

/* §8.1: FHIST is a RESERVED, package-owned label.  This predicate is
 * defence in depth for raw program memory restored from an older or damaged
 * image, not an ownership rule for a user's program.  It recognises an
 * EMPTY store (LBL + END) or one holding a
 * Forth source step, while refusing a native-only collision so the package
 * never appends to or evicts an untrusted image.  Kept native steps may
 * interleave with source steps in the real store, so order is not evidence. */
static bool_t _forthHistProgramConforms(uint16_t program) {
  uint8_t *step = findNextStep(programList[program - 1].instructionPointer);
  uint16_t guard = 0;
  bool_t sawForth = false, sawOther = false;
  while(step != NULL && !(isAtEndOfProgram(step) || isAtEndOfPrograms(step))
        && guard++ < 512) {
    if(checkOpCodeOfStep(step, ITM_FORTH)
       && step[2] == (uint8_t)STRING_LABEL_VARIABLE) {
      sawForth = true;
    }
    else {
      sawOther = true;
    }
    step = findNextStep(step);
  }
  if(step == NULL || guard > 512) { return false; }
  return (bool_t)(sawForth || !sawOther);
}

/* Program number of the FHIST program, or 0 if it does not exist yet.
 * Scans labelList for a GLOBAL label named "FHIST" (labelList[i].step > 0)
 * whose program passes the ownership test above — the first NAME match is
 * upstream's convention for resolving a label, but this store is a
 * package-private artefact, so an untrusted restored image is not accepted
 * by name alone.  boundProgramNameLength guards the read exactly as
 * _removeLabelsAssignments does: corrupt program memory cannot walk this
 * past firstFreeProgramByte. */
uint16_t forthHistoryProgram(void) {
  int16_t i;
  for(i = 0; i < numberOfLabels; i++) {
    if(labelList[i].step > 0) {
      uint8_t len = boundProgramNameLength(labelList[i].labelPointer + 1, labelList[i].labelPointer[0]);
      if(len == FORTH_HISTORY_NAME_LEN
         && memcmp(labelList[i].labelPointer + 1, FORTH_HISTORY_NAME, FORTH_HISTORY_NAME_LEN) == 0
         && _forthHistProgramConforms((uint16_t)labelList[i].program)) {
        return (uint16_t)labelList[i].program;
      }
    }
  }
  return 0;
}

/* Positions the cursor on the GLOBAL .END. step, the only safe insert
 * point for a brand-new program: _insertInProgram writes BEFORE
 * currentStep, and scanLabelsAndPrograms assigns a label to the program
 * number current AT THE LABEL'S POSITION, so any earlier position would
 * splice into an existing program. */
static void _forthHistPositionAtEnd(void) {
  currentStep = firstFreeProgramByte;
  defineCurrentProgramFromCurrentStep();      /* currentProgramNumber == numberOfPrograms */
  currentLocalStepNumber = getNumberOfSteps() + 1;   /* one past the last program's own END */
}

/* First content step of program `program` (right after its LBL), or its
 * own END step if it has none. */
static uint8_t *_forthHistFirstLineStep(uint16_t program) {
  return findNextStep(programList[program - 1].instructionPointer);
}

/* Last content (ITM_FORTH source) step, or NULL if FHIST is empty. */
static uint8_t *_forthHistLastLineStep(uint16_t program) {
  uint8_t *step = _forthHistFirstLineStep(program);
  uint8_t *last = NULL;
  while(step != NULL && !isAtEndOfProgram(step)) {
    last = step;
    step = findNextStep(step);
  }
  return last;
}

/* Number of content (ITM_FORTH source) steps.  Capped like the sibling
 * walkers; _forthHistLineAt needs no cap — its walk is bounded by
 * `index`, which strictly decreases. */
static uint16_t _forthHistLineCount(uint16_t program) {
  uint16_t n = 0;
  uint8_t *step = _forthHistFirstLineStep(program);
  while(step != NULL && !isAtEndOfProgram(step) && n < 512) {
    n++;
    step = findNextStep(step);
  }
  return n;
}

/* The content step at `index` counting from 0 = oldest, or NULL if `index`
 * is out of range. */
static uint8_t *_forthHistLineAt(uint16_t program, uint16_t index) {
  uint8_t *step = _forthHistFirstLineStep(program);
  while(index > 0 && step != NULL && !isAtEndOfProgram(step)) {
    step = findNextStep(step);
    index--;
  }
  if(step == NULL || isAtEndOfProgram(step)) {
    return NULL;
  }
  return step;
}

/* Total byte span of program `program`, from its first byte through its
 * own END inclusive — _getProgramSize()'s idiom, addressable for a
 * program that is not necessarily the last one.  findNextStep can return
 * NULL on an invalid parameter encoding; on NULL or a tripped cap, report
 * a size that STOPS the caller — the only use is
 * `> FORTH_HISTORY_MAX_BYTES`, so 0 ends eviction rather than letting it
 * delete steps measured against garbage. */
static uint32_t _forthHistProgramBytes(uint16_t program) {
  uint8_t *begin = programList[program - 1].instructionPointer;
  uint8_t *step = begin;
  uint16_t guard = 0;
  while(step != NULL && !(isAtEndOfProgram(step) || isAtEndOfPrograms(step))
        && guard++ < 512) {
    step = findNextStep(step);
  }
  if(step == NULL || guard > 512) {
    return 0;
  }
  return (uint32_t)(step - begin) + 2;
}

/* Locate-or-create.  Byte layout (currentStep on the .END. step for BOTH
 * inserts, per the position-and-order rule above):
 *
 *   [ …user progs… END ][ .END. ]          currentStep -> .END.
 *   insert LBL 'FHIST'
 *   [ …user progs… END ][ LBL ][ .END. ]   currentStep -> .END. (advanced past LBL)
 *   insert END
 *   [ …user progs… END ][ LBL ][ END ][ .END. ]
 *
 * The trailing END is what makes it a program: scanLabelsAndPrograms
 * counts a program at an END whose successor is not .END. — the user's
 * last END now counts (its successor is the new LBL), and FHIST's own END
 * (successor .END.) does not add another, so numberOfPrograms rises by
 * exactly 1 whether FHIST ends up empty or seeded. */
#if defined(FORTH_DEBUG_SELFTEST)
/* The ONE seam that lets a fixture reach the "no program, no fold"
 * family: _insertInProgram has no failure return, so forthHistoryEnsure()
 * cannot otherwise be made to fail.  Selftest builds only; set only by
 * the fixture that owns the family, which clears it on every arm out. */
bool_t forthHistoryEnsureFailInjected = false;
#endif

bool_t forthHistoryEnsure(void) {
#if defined(FORTH_DEBUG_SELFTEST)
  if(forthHistoryEnsureFailInjected) {
    return false;
  }
#endif
  if(forthHistoryProgram() != 0) {
    return true;
  }

  _forthHistSaveCursor();
  _forthHistPositionAtEnd();

  tmpString[0] = ITM_LBL;
  tmpString[1] = (char)STRING_LABEL_VARIABLE;
  tmpString[2] = FORTH_HISTORY_NAME_LEN;
  xcopy(tmpString + 3, FORTH_HISTORY_NAME, FORTH_HISTORY_NAME_LEN);
  forthPkgInsertInProgram((uint8_t *)tmpString, 3 + FORTH_HISTORY_NAME_LEN);

  tmpString[0] = (char)((ITM_END >> 8) | 0x80);
  tmpString[1] = (char)(ITM_END & 0xff);
  forthPkgInsertInProgram((uint8_t *)tmpString, 2);

  _forthHistRestoreCursor();

  return forthHistoryProgram() != 0;
}

/* Parks currentStep on FHIST's own END step; _insertInProgram's
 * insert-before-currentStep semantics then append new content as FHIST's
 * newest line, immediately preceding that END.  False if FHIST is absent.
 * The fold calls this too, to park its transient step. */
bool_t forthHistoryGotoLastStep(void) {
  uint16_t program = forthHistoryProgram();
  uint8_t *step;
  uint16_t localStep;

  if(program == 0) {
    return false;
  }

  step = programList[program - 1].instructionPointer;   /* LBL */
  localStep = 1;
  while(!isAtEndOfProgram(step)) {
    step = findNextStep(step);
    localStep++;
  }

  /* Same bracket as the shared restore — this is the fold's entry-side
   * navigation, reached from the same keypress. */
  { int16_t savedDynamicMenuItem = dynamicMenuItem;
    dynamicMenuItem = -1;
    goToPgmStep(program, localStep);
    dynamicMenuItem = savedDynamicMenuItem;
  }
  return true;
}

/* Oldest-first eviction down to FORTH_HISTORY_MAX_BYTES.
 *
 * Returns false when the loop ABANDONED on an error: scanLabelsAndPrograms
 * frees labelList/programList up front and can early-return without
 * reallocating, so after a false return neither list may be touched again
 * — the caller must skip its cursor restore (the L1-H rule, at the second
 * site it was found short at).
 *
 * The saved tuple may name a step in package-owned FHIST.  When an eviction
 * deletes an older step from that same program, the adjustments below keep
 * the tuple on its original entry by upstream's deleter convention. */
bool_t forthHistoryEvict(void) {
  uint16_t program = forthHistoryProgram();
  if(program == 0) {
    return true;
  }

  while(_forthHistProgramBytes(program) > FORTH_HISTORY_MAX_BYTES) {
    uint8_t *lbl = programList[program - 1].instructionPointer;
    uint8_t *firstLine = findNextStep(lbl);
    uint8_t *afterFirstLine;

    if(firstLine == NULL || isAtEndOfProgram(firstLine)) {
      break;   /* nothing left to evict (cap smaller than LBL+END alone) */
    }
    afterFirstLine = findNextStep(firstLine);
    if(afterFirstLine == NULL) {
      break;
    }

    deleteStepsFromTo(firstLine, afterFirstLine);
    if(_forthHistCur.savedProgram == program) {
      if(_forthHistCur.savedLocalStep      > 2) { --_forthHistCur.savedLocalStep; }
      if(_forthHistCur.savedFirstDisplayed > 2) { --_forthHistCur.savedFirstDisplayed; }
    }
    /* Upstream use-after-free guard (binding): abandon the loop rather
     * than touch either list again. */
    if(lastErrorCode != ERROR_NONE) {
      return false;
    }

    program = forthHistoryProgram();   /* re-resolve against the rebuilt list */
    if(program == 0) {
      return true;
    }
  }
  return true;
}

/* Push, cap, evict.  Silent on failure throughout — history is a
 * convenience, never an error that blocks a run. */
void forthHistoryPush(const char *text) {
  uint16_t program;

  if(text[0] == 0) {
    return;
  }
  if(!forthHistoryEnsure()) {
    return;
  }

  /* Consecutive duplicates collapse. */
  program = forthHistoryProgram();
  if(program != 0) {
    uint8_t *newest = _forthHistLastLineStep(program);
    if(newest != NULL) {
      uint8_t len;
      if(forthStepPayload(newest, &len)) {
        uint16_t textLen = (uint16_t)stringByteLength(text);
        if(len == textLen && memcmp(newest + 4, text, textLen) == 0) {
          return;
        }
      }
    }
  }

  _forthHistSaveCursor();

  forthHistoryGotoLastStep();
  forthPkgInsertInProgram((uint8_t *)tmpString, forthCapBuildStep(tmpString, text));
  if(forthHistoryEvict()) {
    /* Only while the lists are safe: an abandoned eviction leaves
     * labelList/programList freed, and the restore walks both. */
    _forthHistRestoreCursor();
  }

  forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);   /* reset on every push */
  _forthHistScratch[0] = 0;                          /* the browse-local stash
                                                        dies with it */
}

/* f-shifted up/down recall.  Read-only: never creates or modifies FHIST.
 * The browse index lives in forthCap — FORTH_HIST_BROWSE_NONE resolves
 * against the CURRENT line count at first use, so the reset at open/push
 * needs no knowledge of FHIST's size. */
void forthHistoryRecall(int16_t delta) {
  uint16_t program = forthHistoryProgram();
  uint16_t lineCount = (program != 0) ? _forthHistLineCount(program) : 0;
  uint16_t cur = forthCapHistoryIndex();
  int32_t next;

  if(cur == FORTH_HIST_BROWSE_NONE || cur > lineCount) {
    cur = lineCount;
  }
  next = (int32_t)cur + delta;
  if(next < 0) {
    next = 0;
  }
  if(next > (int32_t)lineCount) {
    next = (int32_t)lineCount;
  }

  /* Leaving the past-newest slot stashes the line being typed. */
  if(cur == lineCount && (uint16_t)next != lineCount) {
    forthCopyWholeGlyphs(_forthHistScratch, aimBuffer, (int32_t)sizeof(_forthHistScratch));
  }

  if((uint16_t)next == lineCount) {
    /* Arriving back at it restores that line rather than emptying the
     * editor.  With nothing stashed — the ordinary "nothing to recall"
     * press — the stash is empty and the line stands untouched. */
    if((uint16_t)cur == lineCount) {
      return;                                          /* no movement at all */
    }
    xcopy(aimBuffer, _forthHistScratch, stringByteLength(_forthHistScratch) + 1);
  }
  else {
    uint8_t *step = _forthHistLineAt(program, (uint16_t)next);
    uint8_t len = 0;
    if(step != NULL && forthStepPayload(step, &len)) {
      /* Copy the text, do not execute the step: the payload is at step+4
       * for step[3] bytes and is NOT NUL-terminated. */
      if(len > 0) {
        xcopy(aimBuffer, step + 4, len);
      }
      aimBuffer[len] = 0;
    }
    else {
      aimBuffer[0] = 0;   /* defensive: should not happen */
    }
  }

  forthCapSetHistoryIndex((uint16_t)next);
  T_cursorPos = (aimBuffer[0] == 0) ? 0 : stringLastGlyph(aimBuffer) + 1;
  displayAIMbufferoffset = 0;
}


/* ==================================================================
 * The fold context: materialise, arm, sweep, restore.
 *
 * Materialises a real ITM_FORTH capture step in FHIST, seeded with the
 * live interactive line, so the PEM step-insert machinery runs UNMODIFIED
 * against a real step during a TAM session — giving the interactive line
 * the same text by the same code.
 * ================================================================== */

/* Admission — FOLD (bracket armed) vs PARK (materialised and suspended so
 * the line survives, bracket NOT armed, TAM runs live).  PARK never
 * refuses the key and never loses the line. */
static bool_t _forthFoldAdmits(int16_t func, uint16_t mode) {
  if(func == ITM_GTOP)   { return false; }  /* navigates the program pointer via
                                               unguarded fnGoto/goToPgmStep —
                                               not an operand */
  if(func == ITM_ASSIGN || func == ITM_USERMODE) { return false; }  /* zeroes
                                               aimBuffer */
  if(func == ITM_DELP)   { return false; }  /* already excluded by the PEM
                                               commit's own guard */
  switch(mode) {
    case TM_NEWMENU:                         /* sets FLAG_ALPHA + zeroes aimBuffer */
    case TM_STRING:                          /* same */
    case TM_KEY:                             /* half-buffer swap */
      return false;
    default: return true;
  }
}

/* The fold context.  One static instance.  savedProgram is
 * currentProgramNumber — NOT a global step number: program boundaries are
 * themselves global step numbers and all shift when FHIST grows or
 * evicts.  forthFoldLeave restores via goToPgmStep, which re-reads
 * programList AT RESTORE TIME, after scanLabelsAndPrograms has rebuilt
 * it.  Do not "simplify" this back to a saved global number. */
typedef struct {
  uint16_t savedProgram;
  uint16_t savedLocalStep;
  uint16_t savedFirstDisplayed;
  uint16_t entryStepCount;      /* getNumberOfSteps() in FHIST, sampled AFTER
                                    the reposition onto FHIST and BEFORE the
                                    capture-step insert */
  uint32_t capStepOffset;       /* capture step vs beginOfProgramMemory —
                                    program memory may relocate */
  uint8_t  savedZerothStep;     /* pemCursorIsZerothStep */
  uint8_t  pad;
} forthFoldCtx_t;

static forthFoldCtx_t forthFoldCtx;

/* Arm the fold. */
void forthFoldEnter(int16_t func, uint16_t mode) {
  if(!forthHistoryEnsure()) {
    forthCapSetFoldModeRaw(0);   /* no program, no fold */
    return;
  }

  if(currentProgramNumber < 1) {
    /* Bracketed like every other package navigation. */
    int16_t savedDynamicMenuItem = dynamicMenuItem;
    dynamicMenuItem = -1;
    goToGlobalStep(1);           /* guard programList[-1] below */
    dynamicMenuItem = savedDynamicMenuItem;
  }
  forthFoldCtx.savedProgram        = currentProgramNumber;
  forthFoldCtx.savedLocalStep      = currentLocalStepNumber;
  forthFoldCtx.savedFirstDisplayed = firstDisplayedLocalStepNumber;
  forthFoldCtx.savedZerothStep     = (uint8_t)pemCursorIsZerothStep;
  pemCursorIsZerothStep = false;  /* MUST: a parked capture step is a real
                                      step, never the zeroth-step pseudo-
                                      position.  addStepInProgram's pre-move
                                      is gated on this being false; left
                                      true, the TAM step commits BEFORE the
                                      capture step, the resume's canary
                                      falsifies and the capture is
                                      abandoned.  It is a persistent global
                                      with no reset on leaving PEM. */

  forthHistoryGotoLastStep();     /* park on FHIST's last step, before its
                                      END */
  _forthFoldKeptSteps = 0;        /* defensive reset — a stale kept count
                                      would blind the next sweep */

  forthFoldCtx.entryStepCount = getNumberOfSteps();  /* AFTER the reposition:
                                      getNumberOfSteps() is keyed entirely on
                                      currentProgramNumber, so sampling it in
                                      the CALLER's program would make the
                                      sweep eat real history whenever FHIST
                                      is longer. */

  /* Materialise the capture step, seeded with the LIVE line — it stays
   * recoverable in FHIST across a crash inside the fold, which is exactly
   * where a history entry belongs.  Do not "simplify" this back to
   * inserting at the caller's currentStep. */
  forthPkgInsertInProgram((uint8_t *)tmpString, forthCapBuildStep(tmpString, aimBuffer));
  --currentLocalStepNumber;
  currentStep = findPreviousStep(currentStep);  /* park ON the capture step */
  forthFoldCtx.capStepOffset = (uint32_t)(currentStep - beginOfProgramMemory);

  forthCapSetFoldModeRaw(_forthFoldAdmits(func, mode) ? 1 : 2);
}

/* Unwind the fold once the TAM session has actually ENDED.
 *
 *  - The !tam.mode gate: the epilogue runs after EVERY tamProcessInput
 *    call, but only the committing one may unwind — "STO 0 5" is two
 *    calls, and tearing down before the commit runs the second digit with
 *    the bracket off.
 *  - The resume must not fire inside ui/tam.c's raw teardown for an ARMED
 *    fold: its leave-then-dispatch sites tear down and THEN dispatch, so
 *    resuming there happens before the dispatch inserts its step — the
 *    splice sees n == 0 and the line is lost.  The public
 *    leaveTamModeIfEnabled is a wrapper that calls this function itself,
 *    so every teardown outside ui/tam.c settles the bracket by
 *    construction.
 *
 * forthCaptureResume() is a no-op unless FCAP_SUSPENDED, so calling it
 * for a PARK that was already resumed is harmless. */
void forthFoldUnwindIfDone(void) {
  if(!forthFoldPending() || tam.mode) { return; }
  forthCaptureResume();
  forthFoldLeave();
}

/* Where the fold's parked capture step actually is, as opposed to where
 * forthFoldEnter left it.  capStepOffset is stable against everything
 * that happens ABOVE it in program memory and stale against anything
 * below — and a stale address can satisfy the opcode canary from inside
 * a USER's own program, so the answer is resolved through the FHIST rule,
 * never the raw offset.  Returns NULL when FHIST is gone or holds no
 * capture step (the DELP-of-FHIST door): the caller then does nothing at
 * all. */
static uint8_t *_forthFoldResolveCaptureStep(void) {
  uint8_t *cap;

  if(forthHistoryProgram() == 0) { return NULL; }

  /* Through the shared predicate — the one definition of the FHIST rule. */
  cap = beginOfProgramMemory + forthFoldCtx.capStepOffset;
  if(_forthStepIsCaptureStepInHistory(cap)) {
    return cap;
  }

  /* The offset is stale.  Same recovery the resume uses: the capture step
   * is the LAST ITM_FORTH step in FHIST.  The assumption this rests on,
   * stated so it can be attacked: while a fold is pending its capture
   * step is still in FHIST — this function is the only thing that deletes
   * the capture step and it clears foldMode in the same breath; the
   * resume's splice deletes only steps TAM inserted after it; and DELP of
   * FHIST removes the whole program, so the hist == 0 arm above returns
   * NULL first.  If someone finds a door that deletes the step while
   * FHIST survives, the guard to add is
   * `FHIST step count > forthFoldCtx.entryStepCount`. */
  return _forthFoldFindCaptureStep();
}

/* The fold's half of upstream's deleter convention — the rule fnClP
 * applies to its own saved cursor, character for character: a deletion
 * BELOW the saved program shifts it down by one; a deletion AT it leaves
 * the index alone, so the cursor lands on what is now the next program.
 * No-op when no fold is pending, so an ordinary DELP outside the console
 * costs one compare. */
void _forthFoldNoteProgramDeleted(uint16_t deletedProgramNumber) {
  if(!forthFoldPending()) {
    return;
  }
  if(deletedProgramNumber < forthFoldCtx.savedProgram) {
    --forthFoldCtx.savedProgram;
  }
}

void forthFoldLeave(void) {
  if(forthCapFoldModeRaw() == 0) {
    return;
  }

  /* BOTH numbers this block consumes were sampled in FHIST at
   * forthFoldEnter, and the cursor is NOT guaranteed to be in FHIST when
   * we get here: the PARK dispatch runs LIVE after the resume and can
   * navigate, and the resume's abandon arm returns before the re-anchor.
   * So re-anchor onto the capture step first, resolved through
   * _forthFoldResolveCaptureStep and NEVER the raw offset; when the
   * capture step is gone, do NOTHING here — no anchor, no sweep, no
   * delete.  The cursor restore and the foldMode clear below still run. */
  { uint8_t *cap = _forthFoldResolveCaptureStep();
    bool_t listsUnsafe = false;   /* set when the sweep abandons on an
                                     error: scanLabelsAndPrograms frees
                                     labelList/programList up front and
                                     returns early without reallocating */
    if(cap != NULL) {
      currentStep = cap;
      defineCurrentProgramFromCurrentStep();   /* the sweep's threshold is now
                                                  read in the fold's OWN program
                                                  by construction */

      /* Debris sweep.  Normally zero iterations: the resume already
       * deleted the folded step.  Covers the PARK case and break paths
       * that keep NOTHING; steps the splice deliberately KEPT are counted
       * in _forthFoldKeptSteps and stay, or the committed operation
       * vanishes between the splice's "keep" and this sweep.  BOUNDED and
       * guarded — deleteStepsFromTo is a silent no-op when from == to, so
       * an unbounded while can spin; findNextStep can return NULL; and
       * lastErrorCode may already be set on entry. */
      { uint16_t savedErr = lastErrorCode;
        int i;
        lastErrorCode = ERROR_NONE;
        for(i = 0; i < 4; i++) {
          uint8_t *victim;
          if(getNumberOfSteps() <= forthFoldCtx.entryStepCount + 1 + _forthFoldKeptSteps) {
            break;
          }
          victim = findNextStep(currentStep);
          if(victim == NULL || isAtEndOfProgram(victim) || isAtEndOfPrograms(victim)) {
            break;
          }
          deleteStepsFromTo(victim, findNextStep(victim));
          if(lastErrorCode != ERROR_NONE) {
            listsUnsafe = true;       /* the L1-H use-after-free rule */
            break;
          }
        }
        if(lastErrorCode == ERROR_NONE) {
          lastErrorCode = savedErr;
        }
      }

      /* The capture step, RE-RESOLVED a second time — the sweep may have
       * shortened the region, and _insertInProgram rebases every program
       * pointer whenever it grows it.  Through the same resolver, so the
       * second look cannot answer a different question than the first.
       * NOT when the sweep abandoned on an error: the resolver walks
       * labelList and reads programList, and the rule (stated at
       * forthHistoryEvict) is to abandon rather than touch either list
       * again.  The debris is left rather than swept; the error is
       * already on screen and the next unwind resolves it. */
      cap = listsUnsafe ? NULL : _forthFoldResolveCaptureStep();
      if(cap != NULL) {
        deleteStepsFromTo(cap, findNextStep(cap));
      }
    }

    /* The saved cursor, through the shared bounded restore.  savedProgram
     * is an INDEX into programList and the PARK dispatch can DELETE a
     * program: the index is MAINTAINED by the deleter
     * (_forthFoldNoteProgramDeleted, upstream's fnClP convention), and the
     * restore's clamp stays as the crash guard for a shrink no deleter
     * announced.  NOT when the sweep abandoned: the restore walks
     * programList through goToPgmStep, so it obeys the same abandon rule
     * as the resolver — the guard covers every list consumer after the
     * sweep, not only the resolve. */
    if(!listsUnsafe) {
      _forthRestoreCursorTuple(forthFoldCtx.savedProgram,
                               forthFoldCtx.savedLocalStep,
                               forthFoldCtx.savedFirstDisplayed,
                               forthFoldCtx.savedZerothStep);
    }
  }

  forthCapSetFoldModeRaw(0);
  _forthFoldKeptSteps = 0;      /* consumed by this sweep */
}

bool_t forthFoldArmed(void)   { return forthCapFoldModeRaw() == 1; }
bool_t forthFoldPending(void) { return forthCapFoldModeRaw() != 0; }

/* Re-derive fold admission after TAM rewrites tam.function mid-session:
 * a decision cached across a state rewrite must be re-derived at the
 * rewrite, and the two rewrites run in opposite directions, so a one-way
 * patch at one site is wrong at the other.  No-op unless a fold is
 * pending: a rewrite outside the console's bracket must never ARM one. */
void forthFoldRederiveAdmission(int16_t func, uint16_t mode) {
  if(!forthFoldPending()) {
    return;
  }
  forthCapSetFoldModeRaw(_forthFoldAdmits(func, mode) ? 1 : 2);
}
