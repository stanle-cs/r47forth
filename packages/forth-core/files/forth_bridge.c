/*
 * forth_bridge.c — C47↔Forth bridge
 * fnForthCall: invoked by ITM_FCALL (XEQ) with dictionary index in param
 * Per DESIGN.md §6, Stage-H1 Step 11
 * §9.4 derived-state helpers
 */

#include "c47.h"
#include "forth_dict.h"

void fnForthCall(uint16_t param)
{
    forthInner(param, programRunStop == PGM_RUNNING);
}

/* ---- §4.2 executable-name resolution ---- */

/* Dispatch a name that forthFindColon() has already resolved to colon word
 * `widx`: record a step when composing a program, execute it otherwise.
 *
 * code-audit 2026-07-20: PEM must RECORD a step, not execute live — this
 * mirrors the native label arm at every call site and honours DESIGN.md
 * §4.2's "PEM recording of XEQ 'NAME'" contract (the NAME persists in the
 * program, never the dictionary index, which is not stable across edits).
 *
 * Split from forthTryColonFallback() because ui/tam.c must run
 * leaveTamModeIfEnabled() between the lookup and the dispatch. */
void forthDispatchColon(int16_t item, char *name, uint16_t widx)
{
    forthUserItemDispatch(item, name, ITM_FCALL, widx);
}

/* Forth fallback after a native label miss (DESIGN.md §4.2): resolve `name`
 * as a colon word and dispatch it.  Returns false without side effects when
 * it is not one, so the caller falls through to its own not-found handling.
 *
 * Shared by the two identical call sites in keyboard.c (determineItem's XEQ
 * arm) and screen.c (execTimerApp); ui/tam.c uses the two halves separately
 * because of its TAM-exit ordering and its !tam.colon guard. */
bool_t forthTryColonFallback(int16_t item, char *name)
{
    uint16_t widx;
    if (!forthFindColon(name, &widx)) {
        return false;
    }
    forthDispatchColon(item, name, widx);
    return true;
}

/* ---- §9.4 derived-state helpers ---- */

/* Returns true if step is an ITM_FORTH step with STRING_LABEL_VARIABLE param.
 * On success, *lenOut receives step[3] (payload length). Distinguishes
 * "valid Forth step, len==0" (marker) from "not a Forth step" (false). */
bool forthStepPayload(const uint8_t *step, uint8_t *lenOut)
{
    if (step && checkOpCodeOfStep(step, ITM_FORTH) &&
        step[2] == STRING_LABEL_VARIABLE) {
        *lenOut = step[3];
        return true;
    }
    return false;
}

/* §9.2: start of the program containing ptr (largest instructionPointer <= ptr),
 * or NULL if the program list is empty, ptr precedes all programs, ptr lies
 * outside the C47 RAM arena entirely, or ptr lies at/after the start of the
 * genuinely next program.
 *
 * Bounds found during R2-T2: this scan had none, so ANY pointer at or past
 * the last program's start resolved to that program — including a pointer
 * outside program memory entirely (a stack buffer), which is what made
 * test_exec_step_halts_on_error's original stack-local fixture silently
 * pre-scan a real, unrelated program. Harmless in production only because
 * every caller today passes a genuinely in-program pointer; this closes the
 * gap for the first one that doesn't.
 *
 * The upper bound is deliberately coarse (arena membership), not
 * firstFreeProgramByte: several PEM fixtures legitimately set currentStep ==
 * firstFreeProgramByte (cursor parked ON the .END. sentinel, a normal PEM
 * position), and a tighter bound rejected that — regression caught by the
 * full gate before this landed. A stack address fails the arena check by a
 * huge margin, so it alone closes T2's actual gap without that fragility. */
uint8_t *forthOwningProgramStart(const uint8_t *ptr)
{
    uint8_t *arenaBase = (uint8_t *)ram;
    uint8_t *arenaEnd = arenaBase + (size_t)RAM_SIZE_IN_BLOCKS * BYTES_PER_BLOCK;
    if (!arenaBase || ptr < arenaBase || ptr >= arenaEnd) {
        return NULL;
    }

    /* R4 accepted ruling (E5): compute the greatest qualifying start
     * explicitly, matching forthNextProgramStart's own min-tracking below —
     * do not rely on or prove programList ordering. Verified:
     * scanLabelsAndPrograms (manage.c:102-129) walks program memory
     * sequentially and appends in that order, so programList IS built
     * address-ascending today — but that is the builder's behavior, not a
     * documented contract this function may depend on. The old loop just
     * overwrote progStart on every qualifying i (last-in-iteration-order,
     * not largest), which happens to coincide with the max only because of
     * that unstated invariant. */
    uint8_t *progStart = NULL;
    for (uint16_t i = 0; i < numberOfPrograms; i++) {
        uint8_t *ip = programList[i].instructionPointer;
        if (ip <= ptr && (progStart == NULL || ip > progStart)) {
            progStart = ip;
        }
    }
    if (progStart) {
        uint8_t *nextStart = forthNextProgramStart(progStart);
        if (nextStart && ptr >= nextStart) {
            return NULL;  /* belongs to a later program, not this one */
        }
    }
    return progStart;
}

/* §9.2: start of the next program after progStart (smallest
 * instructionPointer strictly greater), or NULL if progStart is last. */
uint8_t *forthNextProgramStart(const uint8_t *progStart)
{
    uint8_t *nextStart = NULL;
    for (uint16_t i = 0; i < numberOfPrograms; i++) {
        uint8_t *ip = programList[i].instructionPointer;
        if (ip > progStart && (nextStart == NULL || ip < nextStart)) {
            nextStart = ip;
        }
    }
    return nextStart;
}

/* §9.4: marker occurrence parity.
 * Owning program start = largest programList[i].instructionPointer <= markerStep.
 * Walk findNextStep from there to markerStep, counting ITM_FORTH steps with
 * len==0 strictly before it. Opening iff count is even. */
bool forthMarkerTurnsOn(const uint8_t *markerStep)
{
    uint8_t *progStart = forthOwningProgramStart(markerStep);

    if (!progStart) return true;

    int markerCount = 0;
    uint8_t *step = progStart;

    while (step && step < markerStep) {
        uint8_t *next = findNextStep(step);
        if (!next || next <= step) break;

        uint8_t len;
        if (checkOpCodeOfStep(step, ITM_FORTH) &&
            step[2] == STRING_LABEL_VARIABLE) {
            len = step[3];
            if (len == 0) {
                markerCount++;
            }
        }

        step = next;
    }

    return (markerCount % 2) == 0;
}

/* §9.4 entry-only toggle: keypad state derived from program bytes at cursor.
 * No persisted flag — recomputed from currentStep every time. */
bool forthEntryStateAtCursor(void)
{
    if (pemCursorIsZerothStep) return false;

    uint8_t len;
    if (!forthStepPayload(currentStep, &len)) return false;

    if (len > 0) return true;

    return forthMarkerTurnsOn(currentStep);
}

/* §9.4 (audit fix F1): entry state governing an INSERTION at currentStep.
 * The new step will follow the step immediately BEFORE currentStep, so derive
 * from that predecessor. currentStep may sit past the pre-move of
 * addStepInProgram (manage.c) or past the committed line after ENTER
 * (pemCloseAlphaInput) — in both cases the predecessor is the step the spec's
 * "cursor lands on" language means.
 *
 * forth-core (F6-1): RTN does not itself close an open-ended (no closing
 * »FORTH marker) region — a routine can be entirely Forth source with RTN
 * as its sole terminator. When the predecessor is RTN, it is transparent:
 * look through it to ITS predecessor instead of concluding RPN, so a
 * capture that empties all the way back to RTN and reopens still resolves
 * Forth. Bounded by progStart like the outer walk; only RTN specifically is
 * looked through, so a genuine RPN predecessor is unaffected. */
bool forthEntryStateAtInsertion(void)
{
  if (pemCursorIsZerothStep) return false;

  uint8_t *progStart = forthOwningProgramStart((const uint8_t *)currentStep);
  if (!progStart || progStart >= currentStep) return false;  /* top of program */

  uint8_t *scanEnd = (uint8_t *)currentStep;
  for (;;) {
    uint8_t *prev = progStart;               /* find predecessor of scanEnd */
    for (;;) {
      uint8_t *next = findNextStep(prev);
      if (!next || next <= prev) return false; /* defensive: malformed walk */
      if (next >= scanEnd) break;            /* prev is the predecessor */
      prev = next;
    }

    uint8_t len;
    if (!forthStepPayload(prev, &len)) {
      if (checkOpCodeOfStep(prev, ITM_RTN) && prev > progStart) {
        scanEnd = prev;
        continue;
      }
      return false;                                     /* RPN step: RPN */
    }
    if (len > 0) return true;                            /* source step: Forth */
    return forthMarkerTurnsOn(prev);                     /* marker: its direction */
  }
}
