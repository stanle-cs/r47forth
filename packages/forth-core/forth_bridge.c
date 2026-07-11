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

/* §9.4: marker occurrence parity.
 * Owning program start = largest programList[i].instructionPointer <= markerStep.
 * Walk findNextStep from there to markerStep, counting ITM_FORTH steps with
 * len==0 strictly before it. Opening iff count is even. */
bool forthMarkerTurnsOn(const uint8_t *markerStep)
{
    uint8_t *progStart = NULL;

    for (uint16_t i = 0; i < numberOfPrograms; i++) {
        if (programList[i].instructionPointer <= markerStep) {
            progStart = programList[i].instructionPointer;
        }
    }

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
 * "cursor lands on" language means. */
bool forthEntryStateAtInsertion(void)
{
  if (pemCursorIsZerothStep) return false;

  uint8_t *progStart = NULL;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    if (programList[i].instructionPointer <= (uint8_t *)currentStep) {
      progStart = programList[i].instructionPointer;
    }
  }
  if (!progStart || progStart >= currentStep) return false;  /* top of program */

  uint8_t *prev = progStart;                 /* find predecessor of currentStep */
  for (;;) {
    uint8_t *next = findNextStep(prev);
    if (!next || next <= prev) return false; /* defensive: malformed walk */
    if (next >= currentStep) break;          /* prev is the predecessor */
    prev = next;
  }

  uint8_t len;
  if (!forthStepPayload(prev, &len)) return false;  /* RPN step: RPN */
  if (len > 0) return true;                          /* source step: Forth */
  return forthMarkerTurnsOn(prev);                   /* marker: its direction */
}
