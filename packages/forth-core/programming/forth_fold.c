// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file forth_fold.c
 * CONSOLIDATE P8: the capture orchestrators, the FHIST history program and
 * the fold context — extracted byte-identically from the
 * programming/manage.c override.  None of this is upstream code; it lived
 * inside an upstream file only because it needed two of that file's
 * statics, which are now the forthPkgInsertInProgram /
 * forthPkgCloseAlphaMenus seams (the paramCorePutLiteral precedent,
 * lblGtoXeq.c).
 *
 * Every public symbol here was ALREADY declared in forth_capture.h: this
 * move changes no API surface, only where the definitions live.
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
 * of every marker after the cursor (the very hazard E3 names). */
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

/* AUDIT round 6 (F10): steps the resume splice deliberately KEPT (oversize
 * decode, no room in the line).  forthFoldLeave's debris sweep counts them
 * into its threshold instead of deleting them — before this, the splice's
 * "keep this and later steps" and the sweep's "covers every break path"
 * prescribed opposite dispositions for the same steps, and a committed
 * STO vanished between them with no error.  Set by forthCaptureResume,
 * consumed and cleared by forthFoldLeave, reset by forthFoldEnter. */
static uint16_t _forthFoldKeptSteps = 0;

/* FIX-7b: re-establish the per-key recommit invariant — the on-disk capture
 * step mirrors aimBuffer.  Mirrors pemAlpha's own glyph-editing recommit
 * tail; callers guarantee currentStep is ON the capture step, and a capture
 * step's opcode is always the 2-byte ITM_FORTH form, so this skips the
 * generic aimFunc branching the pemAlpha tail needs. */
static void forthCapRecommitStep(void) {
  deleteStepsFromTo(currentStep, findNextStep(currentStep));
  forthPkgInsertInProgram((uint8_t *)tmpString, forthCapBuildStep(tmpString, aimBuffer));
  --currentLocalStepNumber;
  currentStep = findPreviousStep(currentStep);
}

void forthCaptureSuspend(void) {
  if (!forthCapIsOpen()) { return; }
  /* Recommit before snapshotting the step offset below.  Since FIX-7b the
   * F6-4 fold recommits at its own tail, so the per-key invariant holds on
   * every entry here; this call stays as defense-in-depth — the audit-#1
   * data-loss bug (test-audit finding 2026-07-20, DESIGN-HISTORY.md) showed
   * what a stale snapshot costs, and a redundant recommit of an in-sync
   * step is byte-neutral. */
  forthCapRecommitStep();
  uint16_t cursor    = T_cursorPos;
  uint16_t localStep = currentLocalStepNumber;
  uint32_t stepOff   = (uint32_t)(currentStep - beginOfProgramMemory);
  /* currentStep stays ON the capture step: the landed commit-and-close
   * nets to exactly that (pemCloseAlphaInput steps forward, the tam.c
   * arm steps back), and TAM commits insert via
   * addStepInProgram(tamOperation()), whose pre-move already places
   * the new step AFTER the current one.  Moving here would shift the
   * TAM insert one step too late.
   * tam.function is NOT touched: tamEnterMode assigned the incoming
   * TAM function before this seam; zeroing it would break the TAM
   * session (the landed close's unconditional reset is the very
   * behavior suspend replaces). */
  clearSystemFlag(FLAG_ALPHA);
  calcModeNormalGui();
  forthPkgCloseAlphaMenus();
  forthCapSuspendState(cursor, localStep, stepOff, getNumberOfSteps());
}

/* L1-F2 rev 3: recover the capture step when the saved offset no longer
 * describes it.
 *
 * An interactive fold can see a SECOND commit inside ONE fold window: STO
 * arms the fold, then a menu_TamSto softkey such as dddVEL supersedes it
 * (ui/tam.c:566-573 calls leaveTamModeIfEnabled and THEN dispatches), and the
 * item it dispatches to — ITM_STOVEL, TM_VALUE max 4096 (items.c:4714) — runs
 * its own TAM whose commit inserts a step while the cursor is still parked on
 * the capture step.  That insert shifts the capture step off
 * forthCapSavedStepOffset(), the canary in forthCaptureResume falsifies, and
 * before this the capture was ABANDONED — closing the user's line and
 * orphaning both steps in FHIST.
 *
 * PEM cannot produce this: its TAM commits exactly once per suspension.  So
 * the recovery is gated on forthFoldPending() and PEM keeps its
 * abandon-on-canary behaviour, which test 5 pins.
 *
 * The capture step is the LAST ITM_FORTH step in FHIST: forthFoldEnter
 * appends it immediately before FHIST's END, so every history line precedes
 * it, and the interloper (a native TAM step) is not ITM_FORTH at all.
 * Returns NULL when FHIST is absent or holds no ITM_FORTH step. */
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

void forthCaptureResume(void) {
  if (!forthCapIsSuspended()) { return; }
  uint8_t *p = beginOfProgramMemory + forthCapSavedStepOffset();
  bool_t pValid = (p < firstFreeProgramByte
                   && checkOpCodeOfStep(p, ITM_FORTH)
                   && p[2] == (uint8_t)STRING_LABEL_VARIABLE);
  /* FOUND BY the [1] history-line-length parameterisation (2026-08-09),
   * the fourth consumer of R8-1's class — an identity resolved by
   * remembered address plus a shape test where the design states it
   * structurally.  The out-of-family fix gave the FOLD CONTEXT's copy of
   * this offset the structural rule (_forthFoldResolveCaptureStep: the
   * answer must lie INSIDE FHIST, because the capture step is only ever
   * created there) — but this canary, the recovery that fix's comment
   * POINTS AT, kept the raw shape.  Executed: DELP of FHIST with a 9-byte
   * history line puts a USER program's own Forth step at exactly the
   * stale offset; the opcode canary passed, the resume rebuilt the
   * owner's line from that step's text, and the splice plus sweep ate
   * eight of the user program's steps (13 -> 5).  The OOF reader named
   * this consequence and it was cleared as "forthCaptureResume recovers"
   * — the recovery's own gate was the door.
   *
   * So for a pending FOLD the address must also lie inside FHIST; when it
   * does not, fall through to the same FHIST-scan recovery below, whose
   * hist == 0 arm abandons — which is P-1's DELP-of-FHIST behaviour.  A
   * PEM capture (no fold) has no FHIST rule — its step lives in the
   * program being edited, no structural bound is stated for it, and
   * abandon-on-canary stays (test 5).  The PEM sibling of this door (DELP
   * from a PEM TAM, alignment onto another program's Forth step) is
   * recorded in the round-9 notes, not silently fixed here. */
  if (pValid && forthFoldPending()) {
    uint16_t hist = forthHistoryProgram();
    if (hist == 0) {
      pValid = false;
    }
    else {
      uint8_t *from = programList[hist - 1].instructionPointer;
      uint8_t *to   = (hist < numberOfPrograms)
                        ? programList[hist].instructionPointer
                        : firstFreeProgramByte;
      pValid = (p >= from && p < to);
    }
  }
  if (!pValid) {
    /* L1-F2 rev 3: an interactive fold can shift the capture step off the
     * saved offset (see _forthFoldFindCaptureStep).  Recover rather than
     * abandon — but ONLY for a fold; PEM keeps abandon-on-canary (test 5). */
    uint8_t *recovered = forthFoldPending() ? _forthFoldFindCaptureStep() : NULL;
    if (recovered != NULL) {
      p = recovered;
      forthCapSuspendStepOffset((uint32_t)(p - beginOfProgramMemory));
    }
    else {
    forthCapAbandonSuspended();             /* defensive canary — see test 5 */
    #if defined(FORTH_DEBUG_SELFTEST)
    printf("FORTH CANARY: suspended capture step falsified; suspension abandoned\n");
    #endif
    return;
    }
  }
  { bool_t keysWas   = forthCapKeysMode();  /* K3/E13: resume is not a fresh
                                               capture — the sub-mode the user
                                               keyed the TAM item from comes
                                               back with the line */
    uint8_t originWas = forthCapOriginRaw();/* L1-1: forthCapOpen() unconditionally
                                               zeroes origin to PEM too — this is
                                               the SUSPENDED->OPEN re-open, not a
                                               PEM open, so origin must survive it
                                               exactly like keysMode does.  LIVE
                                               since L1-F2 wired the interactive
                                               fold: ui/tam.c's tamEnterMode seam
                                               enters the fold and suspends for a
                                               live interactive capture (round 6
                                               D7-3 corrected this comment — it
                                               claimed PEM-only long after the
                                               wiring landed, and verifiers
                                               mis-assessed the window on it). */
    forthCapOpen();                         /* SUSPENDED → OPEN; clears aimBuffer,
                                               which TAM may have used meanwhile */
    forthCapSetKeysMode(keysWas);
    forthCapSetOrigin(originWas);
    /* AUDIT C17: frame ownership no longer needs hand-preservation here — it
     * rides the softmenu frame itself (forth_menu.c's stamp), which TAM's
     * pushes and pops shift but never rewrite.  This site was the round-2
     * homePushed leak (found by five of eight readers); the class is closed
     * by construction now, not by remembering to copy a bit. */
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
  /* AUDIT round 6 (F1): a TAM item that ran LIVE inside the fold (the
   * GTO->GTOP promotion door) can leave currentProgramNumber on ANOTHER
   * program — GTOP navigates and grows program memory.  getNumberOfSteps()
   * is keyed entirely on currentProgramNumber, so without re-anchoring, the
   * splice below subtracted two different programs' step counts: the
   * uint16_t underflow drove deleteStepsFromTo(from, NULL) through
   * xcopy(..., ~4.1e9) — SIGSEGV in three keypresses; on the device, a
   * reboot and the typed line lost.  Re-anchor to the program that actually
   * contains the validated capture step, then clamp and guard like
   * forthFoldLeave's own sweep. */
  defineCurrentProgramFromCurrentStep();
  /* F6-4: steps the suspended TAM committed become canonical text.
   * n is 0 (cancel) or 1 (one commit) today; the loop is defensive. */
  { uint16_t total = getNumberOfSteps();
    uint16_t saved = forthCapSavedStepCount();
    uint16_t n     = (total > saved) ? (uint16_t)(total - saved) : 0;
    uint16_t kept  = 0;
    bool_t folded = false;
    while (n > 0) {
      uint8_t *ins = findNextStep(currentStep);   /* first inserted step */
      if (ins == NULL || isAtEndOfProgram(ins) || isAtEndOfPrograms(ins)) {
        break;   /* round 6 (F1): the count over-ran reality — the sweep's
                    own NULL/END guard shape (see forthFoldLeave) */
      }
      decodeOneStep(ins);                          /* canonical text → tmpString */
      if (stringByteLength(tmpString) > 255) {
        kept = n;  /* defensive: keep the step rather than truncate text */
        break;
      }
      /* K2: the leading separator now lives in forthCapInsertName itself
       * (token-boundary guard) — pass the decoded text straight through. */
      if (!forthCapInsertName(tmpString)) {
        kept = n;  /* no room: keep this and later steps after the line —
                      and TELL the sweep (round 6 F10), or it deletes what
                      this arm just promised to keep */
        break;
      }
      deleteStepsFromTo(ins, findNextStep(ins));
      folded = true;
      --n;
    }
    _forthFoldKeptSteps = kept;
    if (folded) {
      /* FIX-7b: forthCapInsertName wrote into aimBuffer only — recommit so
       * the on-disk step holds the folded text.  Without this, a commit
       * path entered with NO intervening keystroke (ENTER, EXIT, Up/Down —
       * all of which trust the per-key invariant) silently committed the
       * PRE-fold text: audit #1 patched the suspend consumer, this closes
       * the breach at its source. */
      forthCapRecommitStep();
    }
  }
  tam.function = ITM_FORTH;                 /* capture-era tam is exactly
                                                {mode 0, function ITM_FORTH} */
  resetShiftState();                        /* fresh-open parity */
  setSystemFlag(FLAG_ALPHA);
  calcModeAimGui();
  /* FIX-9 (D-C3): a catalog-initiated TAM buried its catalog menus under
   * the TAM menu (_closeCatalog declines to pop there), and
   * leaveTamModeIfEnabled pops only the TAM menu — so without a drain the
   * NEXT softkey dispatch's _closeCatalog() finds the buried MNU_CATALOG,
   * sees the -MNU_ALPHA we are about to push (itself on CatalogMenus[]),
   * and eats it.  Same stack-wide predicate + bounded loop as the E1 arm:
   * popSoftmenu() can re-push HOME, so never spin on the predicate. */
  for(int i = 0; i < SOFTMENU_STACK_SIZE
                 && (forthCatalogMenuOnTop() || forthCatalogBuriedOnStack());
      i++) {
    popSoftmenu();
  }
  if(forthCapIsInteractive()) {
    /* AUDIT round 6 (F5): re-establish the row THROUGH THE OWNER.  The raw
     * showSoftmenu push here left the resumed excursion row UNREGISTERED —
     * owned+borrow 0 with the capture OPEN — after which one EXIT committed
     * keysMode where forthConsoleShowSurface is entitled to change nothing:
     * C18's exact symptom, produced by the fix's own resume path.
     * forthConsoleRestoreSurface is the named re-establisher: stamp alive
     * somewhere → the ownership rules decide; stamp gone → acquire and
     * register.  In keys mode it is a no-op on the intact FWRD base, which
     * is K3/E13 + K-R3 unchanged (the row IS the mode indicator). */
    forthConsoleRestoreSurface();
  }
  else if(!forthCapKeysMode()) {
    showSoftmenu(-MNU_ALPHA);   /* PEM resume: the native alpha row, unchanged */
  }
  pemCursorIsZerothStep = false;
}


/* L1-2 (C1): ENTER's orchestrator for an interactive Forth capture.
 * calcMode stays CM_AIM throughout — no calcModeNormal(), no closeAim(),
 * no popSoftmenu() on this path; the caller (fnKeyEnter's CM_AIM divert,
 * and the ITM_RS guard in keyboard.c) is exactly "run the line, decide
 * whether to reopen empty or reopen with the line intact".
 *
 * The pre-run copy is mandatory, not a nicety (§3.3.2): a word an
 * interactive line executes can rewrite aimBuffer, because aimBuffer is
 * also the NIM buffer (src/c47/c47.c:132). forthOuterInterpret's own
 * memcpy into ctx.source (forth_compile.c:1601) protects ITS parse, not
 * this function's error-path read-back — that must come from a copy
 * taken before the run, not from aimBuffer after. */
void forthInteractiveEnter(void) {
  if (aimBuffer[0] == 0) {
    /* Empty ENTER is a no-op, NOT a close. EXIT is the documented close
     * gesture (C2); an empty line has nothing to run and nothing to keep. */
    return;
  }

  /* E9 tier 1: refuse the commit atomically, capture stays open with the
   * line intact for correction, error already displayed. Same gate the
   * PEM ENTER arm uses (manage.c:1025). */
  if (!forthCheckSourceLine(aimBuffer)) {
    return;
  }

  /* L1-H fills this in; until then it is an empty inline function
   * (forth_capture.h). Push BEFORE the run: an executed word can rewrite
   * aimBuffer (it is also the NIM buffer, §3.3.2), so the text must be
   * captured while it is still the user's line. */
  forthHistoryPush(aimBuffer);
  /* N1-3 (N-R4): the line echo and the FHIST push are ONE ACT — same bytes,
   * same site, ordered together before the run.  That is mechanically what
   * makes the rolled transcript lines and the old history the same history
   * (the owner's 2026-08-05 amendment).  It sits after the E9 refusal above,
   * so a refused line stays in the editor and neither echoes nor enters
   * history; and before the run, so a word that rewrites aimBuffer cannot
   * change what was echoed.
   *
   * A SECOND echo writer, a reorder against this push, or an echo on a path
   * the push skips would make the transcript lie about history — the N1-6
   * one-history assertion pins byte-equality, and N-R2 names the only two
   * licensed divergences. */
  { char echo[FORTH_CONSOLE_FMT_MAX];
    /* AUDIT C11 (the third site of the class): snprintf truncates on a BYTE
     * boundary, so a near-maximal line ends the echo record with a lone lead
     * byte — the same orphan C10 refuses at EMIT.  Build the prefix, then
     * copy the line on a GLYPH boundary into what is left. */
    int32_t at = (int32_t)stringByteLength(STD_RIGHT_DOUBLE_ANGLE " ");
    xcopy(echo, STD_RIGHT_DOUBLE_ANGLE " ", (uint32_t)at);
    forthCopyWholeGlyphs(echo + at, aimBuffer, (int32_t)sizeof(echo) - at);
    forthConsoleAppendLine(echo);
  }

  /* Mandatory pre-run snapshot — see the function banner above. 256 bytes
   * matches the capture cap enforced at every insertion site (C4): the
   * cap keeps aimBuffer's byte length under 256, so this copy can never
   * truncate a line the cap already accepted. */
  char preRunCopy[256];
  {
    int32_t n = stringByteLength(aimBuffer);
    xcopy(preRunCopy, aimBuffer, n + 1);
  }

  { uint32_t seqBefore = forthConsoleWriteSeq();
  forthOuterInterpret(aimBuffer);

  /* N1-6: restore the capture's own input surface.
   *
   * A native item executed by the line can call calcModeNormal() — CLSTK does
   * it outright ("a cleared stack is only visible on the normal screen",
   * src/c47/stack.c:16) — which sets CM_NORMAL, clears FLAG_ALPHA, hides the
   * cursor and can pop the alpha frame.  The capture object survives all of
   * that, so the line surface came back OPEN but no longer on the AIM
   * surface: determineItem stopped routing keys through it and the editor
   * stopped drawing.  Pre-existing since Stage L and invisible while the
   * stack still painted; the console makes it obvious, because the whole
   * transcript vanishes after `XEQ 'CLSTK'`.
   *
   * Repaired here rather than at each offending item: this is the one choke
   * point that knows a capture is still open, and it runs on every path out
   * of a committed line.  Only for the interactive origin — a PEM capture
   * lives on a program step, not on this surface. */
  if (forthCapInteractiveLive()) {            /* C-6: the named predicate */
    if (calcMode != CM_AIM) {
      calcMode = CM_AIM;
      setSystemFlag(FLAG_ALPHA);
      cursorEnabled = true;
      calcModeAimGui();
    }
    /* AUDIT C2-family: calcModeNormal() does not only change the mode — it
     * POPS the console's own softmenu frame when that frame is the ALPHA row
     * and retargets MyAlpha to MyMenu (src/c47/calcMode.c:44-49).  Restoring
     * the mode without restoring the row left the console frameless and EXIT
     * then handed the owner MyMenu.
     *
     * AUDIT round 3: the SURFACE repair is no longer gated on the MODE
     * repair.  The two were one `if`, which conflated "the line left the AIM
     * surface" with "the line damaged the console's row" — and a line can do
     * the second without the first.  `EXITALL` is the reaching input: it is
     * CAT_FNCT/PTP_NONE, so a typed line resolves and runs it
     * (softmenus.c:4250), it pops every frame down to MyMenu — the console's
     * registered frame among them — and it never touches calcMode, so the
     * whole repair block used to be skipped.  The console was left with no
     * row and no stamp, after which EXIT's fallback identity test popped the
     * user's own remaining menus one press at a time.
     * forthConsoleRestoreSurface() is a no-op when the frame is intact, so
     * running it unconditionally costs a stack scan. */
    forthConsoleRestoreSurface();
  }

  if (lastErrorCode != ERROR_NONE) {
    /* N1-3 (N-R4): the error echo.  §8.7's error PROTOCOL is unchanged —
     * the native paint still covers the area until the next key — but that
     * paint is transient, and the transcript line is what keeps the dialogue
     * readable afterwards.  Generic message text only: S1 stands, no token.
     * View-only; FHIST never holds output.
     *
     * AUDIT C19: close the word's own open output record FIRST — `1 . BOGUS`
     * really does print before it raises (tokens run sequentially and the
     * ENTER gate is tier-1 structural only), and appending into the open
     * record merged the message onto the output row, where wide output
     * pushed it off the right edge under the renderer's ellipsis.  The
     * success arm below already closes before its echo; the two post-run
     * arms must agree on the invariant they re-establish. */
    if (forthConsoleHasOpenLine()) {
      forthConsoleNewline();
    }
    forthConsoleAppendLine(errorMessageOf(lastErrorCode));

    /* L5: reopen with the line intact so the user edits rather than
     * retypes. aimBuffer may have been rewritten by a partially-executed
     * line, so restore from the pre-run copy, not from aimBuffer itself. */
    int32_t n = stringByteLength(preRunCopy);
    xcopy(aimBuffer, preRunCopy, n + 1);
    T_cursorPos = stringLastGlyph(aimBuffer) + 1;   /* non-empty by construction */
    return;                                          /* capture stays OPEN */
  }

  /* N1-3 (N-R4): the result echo — the calculator's "ok".  The stack is
   * hidden while the console is up, so the console answers with where X
   * landed, rendered by the same display mode the stack would have used.
   * View-only, like the error line above.
   *
   * Suppressed when the line SPOKE FOR ITSELF — any console write during the
   * run, terminated or not.  Appending X underneath a line that already
   * answered is a second, unasked-for answer.  The first shape of this test
   * asked "is a line still open", which missed `.S` and PAGE: both write and
   * then close, so both collected a redundant X echo.  A write counter
   * sampled across the run asks the question that was actually meant. */
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

  /* L-R3: REPL. Reopen empty, stay in CM_AIM. forthCapOpenInteractive
   * clears aimBuffer and resets keysMode (E14/K1: a fresh capture opens
   * in alpha input, matching the PEM E5 relock). */
  /* AUDIT C3, closed for good by C17: the ownership that had to be
   * hand-preserved across this reopen now rides the softmenu frame itself
   * (forth_menu.c's stamp), which forthCapOpenInteractive() cannot touch. */
  forthCapOpenInteractive();
  forthCapSetKeysMode(true);   /* N1-5 (N-R6): the REPL reopen is the second
                                  interactive open site, and keys-first must
                                  survive every ENTER — not just the first
                                  one.  Same set-after-open shape as
                                  fnForthOuter's arm. */
  /* AUDIT C4: forcing keys mode back on is only half the job — the row has to
   * follow the sub-mode, or ENTER from an alpha excursion leaves the ALPHA
   * keypad displayed while the keyboard has already switched to keys input,
   * and the row says A where the key now types Σ+. */
  forthConsoleShowSurface();
  T_cursorPos = 0;
  displayAIMbufferoffset = 0;
}


/* ==================================================================
 * PACKET_L1_H — the FHIST program: push, cap, evict, recall.
 *
 * FHIST is a single, kept, named, runnable program that accumulates
 * interactive lines as ITM_FORTH source steps.  It is created lazily (on
 * the first push) and appended AFTER every existing program, never
 * spliced into one — see the byte-layout note at forthHistoryEnsure().
 * ================================================================== */

#define FORTH_HISTORY_NAME     "FHIST"
#define FORTH_HISTORY_NAME_LEN 5

/* C2: the cursor tuple.  (program, localStep) — NOT a saved global step
 * number, which program-boundary shifts (FHIST growing/evicting) would
 * make stale by restore time (see forthHistoryPush's use of goToPgmStep,
 * which re-reads programList AT RESTORE TIME, after scanLabelsAndPrograms
 * has rebuilt it). */
typedef struct {
  uint16_t savedProgram;          /* currentProgramNumber */
  uint16_t savedLocalStep;        /* currentLocalStepNumber */
  uint16_t savedFirstDisplayed;   /* firstDisplayedLocalStepNumber */
  uint8_t  savedZerothStep;       /* pemCursorIsZerothStep */
  uint8_t  pad;
} forthHistCursor_t;              /* 8 bytes, BSS, one instance */

static forthHistCursor_t _forthHistCur;

/* AUDIT C5: the line the owner was typing when browsing started.
 *
 * "Past the newest entry" is a real browse position — it is where you are
 * before you press anything — but it was spelled `aimBuffer[0] = 0`, so
 * arriving there EMPTIED the line instead of showing it.  Since the browse
 * index is NONE at open and after every push, the very first f-up or
 * f-down a curious owner pressed destroyed whatever they had typed, on a
 * fresh calculator with no history to show for it.
 *
 * The line is stashed on the way out of the past-newest slot and restored
 * on the way back in, which is what every line editor with a history does.
 * It lives in BSS beside the fold context, not in the capture object: it is
 * strictly browse-local, must not survive a suspension or a restore, and
 * has no persistence contract of its own (round 3's R1 is the record of
 * what happens when transient state is put somewhere persisted). */
static char _forthHistScratch[FORTH_CONSOLE_LINE_MAX + 1];


static void _forthHistSaveCursor(void) {
  _forthHistCur.savedProgram        = currentProgramNumber;
  _forthHistCur.savedLocalStep      = currentLocalStepNumber;
  _forthHistCur.savedFirstDisplayed = firstDisplayedLocalStepNumber;
  _forthHistCur.savedZerothStep     = (uint8_t)pemCursorIsZerothStep;
}

static void _forthHistRestoreCursor(void) {
  /* R8-2: the third of the package's three navigations during a keypress;
   * see the bracket's rationale at forthFoldLeave's restore. */
  int16_t savedDynamicMenuItem = dynamicMenuItem;
  dynamicMenuItem = -1;
  goToPgmStep(_forthHistCur.savedProgram, _forthHistCur.savedLocalStep);
  firstDisplayedLocalStepNumber = _forthHistCur.savedFirstDisplayed;
  defineFirstDisplayedStep();
  pemCursorIsZerothStep = _forthHistCur.savedZerothStep;
  dynamicMenuItem = savedDynamicMenuItem;
}

/* Program number of the FHIST program, or 0 if it does not exist yet.
 * Scans labelList for a GLOBAL label named "FHIST" (labelList[i].step > 0
 * — see scanLabelsAndPrograms, manage.c:190-193). boundProgramNameLength
 * guards the read exactly as _removeLabelsAssignments does: a corrupt or
 * crafted program cannot walk this past firstFreeProgramByte. */
uint16_t forthHistoryProgram(void) {
  int16_t i;
  for(i = 0; i < numberOfLabels; i++) {
    if(labelList[i].step > 0) {
      uint8_t len = boundProgramNameLength(labelList[i].labelPointer + 1, labelList[i].labelPointer[0]);
      if(len == FORTH_HISTORY_NAME_LEN
         && memcmp(labelList[i].labelPointer + 1, FORTH_HISTORY_NAME, FORTH_HISTORY_NAME_LEN) == 0) {
        return (uint16_t)labelList[i].program;
      }
    }
  }
  return 0;
}

/* Positions currentStep/currentProgramNumber/currentLocalStepNumber on the
 * GLOBAL .END. step (isAtEndOfPrograms), the only safe insert point for a
 * brand-new program: _insertInProgram writes BEFORE currentStep, and
 * scanLabelsAndPrograms assigns a label to the program number current AT
 * THE LABEL'S POSITION, so any earlier position would splice into an
 * existing program. Reuses the landed getNumberOfSteps()/
 * defineCurrentProgramFromCurrentStep() idiom rather than hand-counting. */
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

/* Number of content (ITM_FORTH source) steps.
 *
 * AUDIT round 8 §6 (the unbounded-walk class, third recurrence): this
 * walker and _forthHistProgramBytes below had no iteration cap while their
 * siblings in this file carry one (_forthFoldFindCaptureStep's i < 512).
 * They only ever walk FHIST and findNextStep returns NULL only on an
 * invalid parameter encoding, so there is no reaching input today — the
 * cap is the sibling idiom applied before the class comes back a third
 * time.  _forthHistLineAt needs none: its walk is bounded by `index`,
 * which strictly decreases. */
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
 * own END inclusive — same idiom as _getProgramSize() (manage.c:378-389)
 * but addressable for a program that is not necessarily the last one. */
static uint32_t _forthHistProgramBytes(uint16_t program) {
  uint8_t *begin = programList[program - 1].instructionPointer;
  uint8_t *step = begin;
  uint16_t guard = 0;
  /* The NULL check is the load-bearing half (round 8 §6): this walker
   * handed findNextStep's result straight to isAtEndOfProgram, and
   * findNextStep can return NULL on an invalid parameter encoding
   * (src/c47/programming/nextStep.c:151-157).  On NULL or a tripped cap,
   * report a size that STOPS the caller: forthHistoryEvict's only use is
   * `> FORTH_HISTORY_MAX_BYTES`, so 0 ends eviction rather than letting it
   * delete steps measured against garbage. */
  while(step != NULL && !(isAtEndOfProgram(step) || isAtEndOfPrograms(step))
        && guard++ < 512) {
    step = findNextStep(step);
  }
  if(step == NULL || guard > 512) {
    return 0;
  }
  return (uint32_t)(step - begin) + 2;
}

/* C1: locate-or-create.  Byte layout (currentStep on the .END. step for
 * BOTH inserts, per the position-and-order rule above):
 *
 *   [ …user progs… END ][ .END. ]          currentStep -> .END.
 *   insert LBL 'FHIST'
 *   [ …user progs… END ][ LBL ][ .END. ]   currentStep -> .END. (advanced past LBL)
 *   insert END
 *   [ …user progs… END ][ LBL ][ END ][ .END. ]
 *
 * The trailing END is what makes it a program: scanLabelsAndPrograms
 * counts a program at an END whose successor is not .END.
 * (src/c47/programming/manage.c:143-146) — the user's last END now counts
 * (its successor is the new LBL, not .END.), and FHIST's own END
 * (successor .END.) does not add another.  This increments numberOfPrograms
 * by exactly 1 regardless of whether FHIST ends up empty or seeded: the
 * increment is triggered by the PRECEDING program's END gaining a
 * non-.END. successor, not by FHIST's own trailing END — settled by the
 * C5.1 test, see its comment for the observed result. */
#if defined(FORTH_DEBUG_SELFTEST)
/* AUDIT round 8 (P-2, owner ruling 2026-08-08): the ONE seam that lets a
 * fixture reach the "no program, no fold" family.
 *
 * Three audit rounds raised findings whose entire chain hangs on this
 * function returning false (K-N §6a R1, round 5 (b), round 7 R-1 and
 * P-2), and every one was ruled on the premise rather than settled by
 * evidence, because there is no way in: _insertInProgram has no failure
 * return, and every failure mode inside faults before it could return
 * false.  The record calls that a dead premise; it is also an untestable
 * one, so the defensive foldMode-0 arm it guards has never once been
 * executed by the battery.  The owner ruled to buy the coverage.
 *
 * Selftest builds only, and set only by the fixture that owns the family —
 * test_fold_round8_window subcase [5], which clears it on every arm out,
 * including its own FIXTURE BUG paths, and again at the fixture's tail.
 * (AUDIT round 8, R8-8: this banner previously named a fixture that does
 * not exist — a reader looking for the owner would not have found it.) */
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

/* C1: parks currentStep on FHIST's own END step (its "last step" — END is
 * always the final numbered step of a program, per getNumberOfSteps()'s
 * own convention), i.e. immediately before it. _insertInProgram's
 * insert-before-currentStep semantics then append new content as FHIST's
 * newest line, immediately preceding that END.  False if FHIST is absent.
 * L1-F1 (the fold) calls this too, to park its transient step there. */
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

  /* R8-2: same bracket as forthFoldLeave's restore — this is the fold's
   * entry-side navigation, reached from the same keypress. */
  { int16_t savedDynamicMenuItem = dynamicMenuItem;
    dynamicMenuItem = -1;
    goToPgmStep(program, localStep);
    dynamicMenuItem = savedDynamicMenuItem;
  }
  return true;
}

/* C3: oldest-first eviction down to FORTH_HISTORY_MAX_BYTES. */
void forthHistoryEvict(void) {
  uint16_t program = forthHistoryProgram();
  if(program == 0) {
    return;
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
    /* Upstream use-after-free guard (binding — STAGE_L_TRACES.md §T7.2b,
     * PACKET_L1_H C3). deleteStepsFromTo calls scanLabelsAndPrograms, which
     * frees labelList/programList up front (manage.c:132-133) and can
     * early-return on ERROR_RAM_FULL without reallocating
     * (src/c47/programming/manage.c:151-163), leaving both NULL. leavePem
     * then dereferences programList via defineCurrentStep
     * (keyboard.c:2404-2409 -> src/c47/programming/nextStep.c:532, a file
     * with no package override) — an upstream defect we do not patch
     * upstream (S1 precedent: UPSTREAM_REPORTS_globalRegister_reset.md).
     * Abandon the loop rather than touch either list again. */
    if(lastErrorCode != ERROR_NONE) {
      return;
    }

    program = forthHistoryProgram();   /* re-resolve against the rebuilt list */
    if(program == 0) {
      return;
    }
  }
}

/* C3: push, cap, evict.  Silent on failure throughout — history is a
 * convenience, never an error that blocks a run. */
void forthHistoryPush(const char *text) {
  uint16_t program;

  if(text[0] == 0) {
    return;
  }
  if(!forthHistoryEnsure()) {
    return;
  }

  /* L2: consecutive duplicates collapse. */
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
  forthHistoryEvict();

  _forthHistRestoreCursor();

  forthCapSetHistoryIndex(FORTH_HIST_BROWSE_NONE);   /* C4: reset on every push */
  _forthHistScratch[0] = 0;                          /* C5: and the browse-local
                                                        stash dies with it */
}

/* C4: f-shifted up/down recall.  Read-only: never creates or modifies
 * FHIST.  The browse index lives in forthCap (forthCapHistoryIndex/
 * forthCapSetHistoryIndex) — FORTH_HIST_BROWSE_NONE resolves against the
 * CURRENT line count at first use, so the reset at open/push needs no
 * knowledge of FHIST's size. */
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

  /* C5: leaving the past-newest slot stashes the line being typed. */
  if(cur == lineCount && (uint16_t)next != lineCount) {
    forthCopyWholeGlyphs(_forthHistScratch, aimBuffer, (int32_t)sizeof(_forthHistScratch));
  }

  if((uint16_t)next == lineCount) {
    /* C5: and arriving back at it restores that line rather than emptying
     * the editor.  With nothing stashed — the ordinary "nothing to recall"
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
       * for step[3] bytes and is NOT NUL-terminated (_forthCapBuildStep;
       * forthStepPayload). */
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
 * PACKET_L1_F1 — the fold context: materialise, arm, sweep, restore.
 *
 * Materialises a real ITM_FORTH capture step in FHIST (L1-H's program),
 * seeded with the live interactive line, so F2's calcMode = CM_PEM bracket
 * around _tamProcessInput lets the landed F6-2/F6-4 PEM step-insert
 * machinery run UNMODIFIED against a real step, giving the interactive line
 * the same text by the same code.  L1-F1 landed this inert; L1-F2 wired it
 * LIVE (ui/tam.c's tamEnterMode seam) — round 6 D7-3 corrected this
 * comment, which still said "inert in production" while every crash of the
 * round lived in this window.  The self-test additionally drives
 * forthFoldEnter/forthFoldLeave directly.
 * ================================================================== */

/* C2: admission — FOLD (bracket armed) vs PARK (materialised and
 * suspended so the line survives, bracket NOT armed, TAM runs live).  PARK
 * is option (c) applied to the minority: it never refuses the key and
 * never loses the line. */
static bool_t _forthFoldAdmits(int16_t func, uint16_t mode) {
  if(func == ITM_GTOP)   { return false; }  /* navigates the program pointer via
                                               unguarded fnGoto/goToPgmStep,
                                               ui/tam.c:888-899 — not an operand */
  if(func == ITM_ASSIGN || func == ITM_USERMODE) { return false; }  /* zeroes
                                               aimBuffer, ui/tam.c:1198-1200 */
  if(func == ITM_DELP)   { return false; }  /* already excluded by the PEM commit's
                                               own guard, ui/tam.c:1102 */
  switch(mode) {
    case TM_NEWMENU:                         /* sets FLAG_ALPHA + zeroes aimBuffer */
    case TM_STRING:                          /* same */
    case TM_KEY:                             /* half-buffer swap */
      return false;
    default: return true;
  }
}

/* C1: the fold context.  One static instance.  savedProgram is
 * currentProgramNumber — NOT a global step number: program boundaries are
 * themselves global step numbers and all shift when FHIST grows or evicts
 * (see L1-H C2).  forthFoldLeave restores via goToPgmStep, which re-reads
 * programList[program - 1] AT RESTORE TIME, after scanLabelsAndPrograms has
 * rebuilt it, so the base is current; a saved GLOBAL step number would be
 * computed before FHIST grew/evicted and be stale by restore time.  Do not
 * "simplify" this back to a saved global number. */
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

/* C3: arm the fold.  Exact — see PACKET_L1_F1_fold_context.md C3 for the
 * rationale behind every step; the comments here are the load-bearing
 * subset. */
void forthFoldEnter(int16_t func, uint16_t mode) {
  if(!forthHistoryEnsure()) {
    forthCapSetFoldModeRaw(0);   /* no program, no fold */
    return;
  }

  if(currentProgramNumber < 1) {
    goToGlobalStep(1);           /* guard programList[-1] below */
  }
  forthFoldCtx.savedProgram        = currentProgramNumber;
  forthFoldCtx.savedLocalStep      = currentLocalStepNumber;
  forthFoldCtx.savedFirstDisplayed = firstDisplayedLocalStepNumber;
  forthFoldCtx.savedZerothStep     = (uint8_t)pemCursorIsZerothStep;
  pemCursorIsZerothStep = false;  /* MUST: a parked capture step is a real
                                      step, never the zeroth-step pseudo-
                                      position.  addStepInProgram's pre-move
                                      (manage.c:2664) is gated on this being
                                      false; left true, the TAM step commits
                                      BEFORE the capture step, resume's
                                      offset-derived pointer (manage.c:
                                      1210-1213) reads the TAM step, the
                                      canary falsifies and the capture is
                                      abandoned.  It is a persistent global
                                      with no reset on leaving PEM. */

  forthHistoryGotoLastStep();     /* L1-H's helper: park on FHIST's last
                                      step, before its END */
  _forthFoldKeptSteps = 0;        /* round 6 (F10): defensive reset — a stale
                                      kept count would blind the next sweep */

  forthFoldCtx.entryStepCount = getNumberOfSteps();  /* AFTER the reposition:
                                      getNumberOfSteps() is keyed entirely on
                                      currentProgramNumber (manage.c:2774-
                                      2787), so sampling it in the CALLER's
                                      program and comparing against FHIST's
                                      count in forthFoldLeave would make the
                                      sweep eat real history whenever FHIST
                                      is longer. */

  /* Materialise the capture step, seeded with the LIVE line.  This is
   * manage.c:941-952's shape verbatim, with aimBuffer instead of "".  It
   * leaves the live line recoverable in FHIST across a crash inside the
   * fold — exactly where a history entry belongs (T7.2a) — so do not
   * "simplify" this back to inserting at the caller's currentStep. */
  forthPkgInsertInProgram((uint8_t *)tmpString, forthCapBuildStep(tmpString, aimBuffer));
  --currentLocalStepNumber;
  currentStep = findPreviousStep(currentStep);  /* park ON the capture step —
                                      the state forthCaptureSuspend documents
                                      at manage.c:1192-1197 */
  forthFoldCtx.capStepOffset = (uint32_t)(currentStep - beginOfProgramMemory);

  forthCapSetFoldModeRaw(_forthFoldAdmits(func, mode) ? 1 : 2);
}

/* C4: unwind the fold.  Exact — see PACKET_L1_F1_fold_context.md C4. */
/* L1-F2 (rev 3): unwind the fold once the TAM session has actually ENDED.
 *
 * Two defects in rev 2 forced this shape, both confirmed by test:
 *
 *  - The epilogue fired after EVERY tamProcessInput call, not only the one
 *    that commits.  "STO 0 5" is two calls; the first digit does not commit,
 *    so the fold was torn down BEFORE the commit — the second digit then ran
 *    with the bracket off, took the live dispatch arm, and actually stored.
 *    Hence the !tam.mode gate: unwind only when TAM is really over.
 *
 *  - The resume must not fire inside ui/tam.c's RAW teardown (_tamLeave)
 *    for an ARMED fold.  That file's leave-then-dispatch sites tear down
 *    and THEN dispatch, so resuming there happens before the dispatch
 *    inserts its step: the F6-4 splice sees n == 0, folds nothing, and the
 *    line is lost while an orphan step stays in FHIST.  So Seam 2 defers
 *    the resume for an armed fold and it happens here instead, after
 *    _tamProcessInput has fully returned.  (D7-1, 2026-08-08: the PUBLIC
 *    leaveTamModeIfEnabled is now a wrapper that calls this function
 *    itself, so every teardown outside ui/tam.c settles the bracket by
 *    construction — the F2/F4 strand class cannot recur through it.)
 *
 * forthCaptureResume() is a no-op unless FCAP_SUSPENDED, so calling it for a
 * PARK that Seam 2 already resumed is harmless. */
void forthFoldUnwindIfDone(void) {
  if(!forthFoldPending() || tam.mode) { return; }
  forthCaptureResume();
  forthFoldLeave();
}

/* AUDIT round 8, out-of-family: where the fold's parked capture step
 * actually is, as opposed to where forthFoldEnter left it.
 *
 * forthFoldCtx.capStepOffset is an offset from beginOfProgramMemory, which
 * is stable against everything that happens ABOVE it and stale against
 * anything that happens below: delete a program that sits BEFORE FHIST and
 * the capture step slides down by that program's size while the context's
 * copy does not move.  forthCaptureResume already recovers from exactly
 * this — its canary falsifies, _forthFoldFindCaptureStep locates the real
 * step and it rewrites the CAPTURE's offset — but the fold context has its
 * own copy and nobody rewrote that one.  Executed (console open, DELP,
 * spell a program that precedes FHIST, ENTER): FHIST came back one step
 * longer, the owner's parked line stranded in it as debris.
 *
 * Two rules, and the second is the one the raw canary was missing: the
 * answer must be INSIDE FHIST, because the capture step is only ever
 * created there.  A stale address that happens to satisfy the opcode
 * canary is not a near-miss — a Forth step inside a user's own program
 * satisfies it exactly, and forthFoldLeave re-anchors the debris sweep
 * onto whatever program the answer lives in.
 *
 * Returns NULL when FHIST is gone or holds no capture step, which is the
 * DELP-of-FHIST door: the caller then does nothing at all. */
static uint8_t *_forthFoldResolveCaptureStep(void) {
  uint16_t hist = forthHistoryProgram();
  uint8_t *cap, *from, *to;

  if(hist == 0) { return NULL; }

  from = programList[hist - 1].instructionPointer;
  to   = (hist < numberOfPrograms) ? programList[hist].instructionPointer
                                   : firstFreeProgramByte;

  cap = beginOfProgramMemory + forthFoldCtx.capStepOffset;
  if(cap >= from && cap < to
     && cap < firstFreeProgramByte
     && checkOpCodeOfStep(cap, ITM_FORTH)
     && cap[2] == (uint8_t)STRING_LABEL_VARIABLE) {
    return cap;
  }

  /* The offset is stale.  Same recovery forthCaptureResume uses: the
   * capture step is the LAST ITM_FORTH step in FHIST — forthFoldEnter
   * appends it after every history line, and a step TAM committed is not
   * ITM_FORTH at all.
   *
   * The assumption this rests on, stated so it can be attacked: while a
   * fold is pending its capture step is still in FHIST, so the last
   * ITM_FORTH step is that step and not a history line.  Deliberately NOT
   * defended with a step-count check, because no door to the contrary
   * exists — this function is the only thing that deletes the capture step,
   * and it clears foldMode in the same breath; the resume's splice deletes
   * only the steps TAM inserted after it; and the one gesture that removes
   * the step from underneath (DELP of FHIST) removes the whole program, so
   * the hist == 0 arm above returns NULL first.  If someone finds a door
   * that deletes the step while FHIST survives, that is a finding with a
   * reaching input, and the guard to add is
   * `FHIST step count > forthFoldCtx.entryStepCount`. */
  return _forthFoldFindCaptureStep();
}

/* AUDIT round 8 (R8-1): the fold's half of upstream's deleter convention.
 * Called from _clearProgram, which is where fnClP renumbers its own saved
 * cursor — and the rule is upstream's, character for character: a deletion
 * BELOW the saved program shifts it down by one; a deletion AT it leaves the
 * index alone (upstream's `programNumberToDelete != savedCurrentProgramNumber`
 * arm does nothing, so the cursor lands on what is now the next program, and
 * the fold follows rather than inventing a different answer).
 *
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

  /* AUDIT round 8 (P-1): BOTH numbers this block consumes were sampled in
   * FHIST at forthFoldEnter — entryStepCount is FHIST's step count and
   * capStepOffset is a FHIST address — and getNumberOfSteps() is keyed
   * entirely on currentProgramNumber.  Neither means anything anywhere
   * else, and the cursor is NOT guaranteed to be in FHIST when we get
   * here: the PARK dispatch runs LIVE after the resume and can navigate
   * (GTOP), and forthCaptureResume's abandon arm returns BEFORE the F1
   * re-anchor whenever the canary falsifies.  EXECUTED door: console open
   * -> DELP -> "FHIST" -> ENTER deletes FHIST from inside its own fold;
   * the sweep then compared FHIST's entry count against a real user
   * program's length and deleted four of its steps (13 -> 9, `111 222 333
   * 444` decoded away).
   *
   * So re-anchor onto the capture step first — F1's own fix, applied at
   * the second consumer of an FHIST-scoped count — and when the capture
   * step is gone, do NOTHING here: no anchor, no sweep, no delete.  The
   * cursor restore and the foldMode clear below still run.
   *
   * AUDIT round 8, OUT-OF-FAMILY: resolving that step through
   * _forthFoldResolveCaptureStep and NOT through the raw offset is the
   * whole of the second half of this fix.  capStepOffset is an offset from
   * beginOfProgramMemory, so deleting a program that sits BEFORE FHIST
   * shifts the capture step DOWN and strands the context's copy — and
   * nothing updated it, because the recovery forthCaptureResume already
   * has for exactly this case fixes the CAPTURE's offset only.  Executed:
   * console open, DELP, spell a program that precedes FHIST, ENTER — FHIST
   * came back one step longer, the owner's parked line left behind as
   * debris.  The reader's other consequence is worse and shares the root:
   * the stale address can land on a Forth step inside a USER program,
   * which satisfies this canary exactly, after which the re-anchor would
   * aim the sweep at that program. */
  { uint8_t *cap = _forthFoldResolveCaptureStep();
    if(cap != NULL) {
      currentStep = cap;
      defineCurrentProgramFromCurrentStep();   /* the sweep's threshold is now
                                                  read in the fold's OWN program
                                                  by construction */

      /* Debris sweep.  Normally zero iterations: forthCaptureResume already
       * deleted the folded step (manage.c:1262).  This covers the PARK case and
       * break paths that keep NOTHING; steps the splice deliberately KEPT
       * (oversize decode, no room — round 6 F10) are counted in
       * _forthFoldKeptSteps and stay, or the committed operation vanishes
       * between the splice's "keep" and this sweep.  BOUNDED and guarded —
       * deleteStepsFromTo is a silent no-op when from == to (manage.c:221-227),
       * so an unbounded while can spin; findNextStep can return NULL
       * (src/c47/programming/nextStep.c:151-157); and lastErrorCode may already
       * be set on entry. */
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
            break;                    /* L1-H's UAF guard */
          }
        }
        if(lastErrorCode == ERROR_NONE) {
          lastErrorCode = savedErr;
        }
      }

      /* The capture step, RE-RESOLVED a second time — the sweep above may
       * have shortened the region, and _insertInProgram rebases every
       * program pointer whenever it grows it (manage.c:723-733).  Through
       * the same resolver, so the second look cannot answer a different
       * question than the first. */
      cap = _forthFoldResolveCaptureStep();
      if(cap != NULL) {
        deleteStepsFromTo(cap, findNextStep(cap));
      }
    }
  }

  /* AUDIT round 8 (R8-1): the cursor restore is the THIRD consumer of a
   * quantity sampled across the PARK dispatch, and the P-1 fix above ruled
   * it out of scope in one clause ("the cursor restore and the foldMode
   * clear below still run").  It is the same class and it was the worst of
   * the three.
   *
   * savedProgram is an INDEX into programList, which every
   * scanLabelsAndPrograms reallocates to exactly numberOfPrograms entries,
   * and the PARK dispatch can DELETE a program: console open, DELP, name a
   * program that precedes the cursor's, ENTER.  Upstream states the rule in
   * the very function that dispatch runs — fnClP renumbers its own saved
   * cursor when the deleted program precedes it (src/c47 manage.c:350-355)
   * and _clearProgram clamps again — and this was a third cache of the same
   * quantity with neither guard.  Ordinary case: the cursor silently landed
   * in a program the owner was not editing, overwriting fnClP's CORRECT
   * restore, with no error.  Boundary case (the cursor's program was the
   * last one): goToPgmStep read programList[numberOfPrograms] out of bounds
   * on the freshly reallocated arena, and goToGlobalStep walked the garbage
   * with no NULL guard and no iteration cap — reproduced as a SIGSEGV.
   *
   * The index is now MAINTAINED by the deleter, which is upstream's own
   * convention and not a workaround: `_clearProgram` calls
   * `_forthFoldNoteProgramDeleted` at the moment it knows which program is
   * going, applying the same rule fnClP applies to its own saved cursor.
   * That is what makes the ordinary case correct — the owner comes back to
   * the program they were editing.
   *
   * The clamp below stays as the crash guard.  It is defence in depth for
   * a shrink no deleter announced: this used to be the only thing between
   * a stale index and `programList[numberOfPrograms]` on a freshly
   * reallocated arena, walked by `goToGlobalStep` with no NULL guard and
   * no iteration cap — reproduced as a SIGSEGV.  A repair at this site
   * alone could never fix the identity half, because nothing HERE knows
   * which program went; that is precisely why the fix belongs in the
   * deleter. */
  { uint16_t p = forthFoldCtx.savedProgram;
    if(numberOfPrograms > 0) {
      if(p < 1)                { p = 1; }
      if(p > numberOfPrograms) { p = numberOfPrograms; }
      /* AUDIT round 8 (R8-2): goToGlobalStep, which goToPgmStep reaches, is
       * not a "go to this step" primitive — with dynamicMenuItem >= 0 it
       * reinterprets the request as the label the dynamic menu names and
       * RETURNS WITHOUT NAVIGATING when that does not resolve
       * (lblGtoXeq.c:102, :114-116; DESIGN.md §3.3.6).  The softkey that
       * commits a console TAM latches exactly that global — press DELP,
       * PROG, then a program-name softkey — and nothing on the commit path
       * clears it, so this restore silently did not happen and the owner
       * was left parked inside FHIST at a step number belonging to another
       * program.  Bracketed here the way this tree already brackets its two
       * other navigations: _insertInProgram (manage.c:721/772/774) and
       * forth_compile.c:1431-1437. */
      { int16_t savedDynamicMenuItem = dynamicMenuItem;
        dynamicMenuItem = -1;
        goToPgmStep(p, forthFoldCtx.savedLocalStep);
        firstDisplayedLocalStepNumber = forthFoldCtx.savedFirstDisplayed;
        defineFirstDisplayedStep();
        pemCursorIsZerothStep = forthFoldCtx.savedZerothStep;
        dynamicMenuItem = savedDynamicMenuItem;
      }
    }
  }

  forthCapSetFoldModeRaw(0);
  _forthFoldKeptSteps = 0;      /* round 6 (F10): consumed by this sweep */
}

bool_t forthFoldArmed(void)   { return forthCapFoldModeRaw() == 1; }
bool_t forthFoldPending(void) { return forthCapFoldModeRaw() != 0; }

/* AUDIT round 8 (C-1): the one way to re-derive fold admission after TAM
 * rewrites tam.function mid-session.  forthFoldEnter decided FOLD vs PARK
 * from the item it was ENTERED with; a rewrite makes that decision stale in
 * exactly the sense the F1 class names — "any decision cached across a
 * state rewrite must be re-derived at the rewrite" — and the two rewrites
 * run in opposite directions, so a hand-written one-way patch at one site
 * (which is what F1 landed) is wrong at the other.
 *
 * No-op unless a fold is pending: a rewrite outside the console's bracket
 * must never ARM one. */
void forthFoldRederiveAdmission(int16_t func, uint16_t mode) {
  if(!forthFoldPending()) {
    return;
  }
  forthCapSetFoldModeRaw(_forthFoldAdmits(func, mode) ? 1 : 2);
}
