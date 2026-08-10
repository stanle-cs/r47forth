/*
 * forth_bridge.c — C47↔Forth bridge
 * fnForthCall: invoked by ITM_FCALL (XEQ) with dictionary index in param
 * Per DESIGN.md §6, §9.4 (derived-state helpers)
 */

#include "c47.h"
#include "forth_dict.h"
#include "forth_console.h"

void fnForthCall(uint16_t param)
{
    forthInner(param, programRunStop == PGM_RUNNING);
}

/* ---- §4.2 executable-name resolution ---- */

/* Dispatch a name that forthFindColon() has already resolved to colon word
 * `widx`: record a step when composing a program, execute it otherwise.
 *
 * PEM must RECORD a step, not execute live — this mirrors the native label
 * arm at every call site and honours DESIGN.md §4.2's "PEM recording of XEQ
 * 'NAME'" contract (the NAME persists in the program, never the dictionary
 * index, which is not stable across edits).
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
 * The arena-membership check is required: without it, any pointer at or past
 * the last program's start would resolve to that program, including a
 * pointer outside program memory entirely (e.g. a stack buffer) — harmless
 * only because every caller today passes a genuinely in-program pointer.
 *
 * The upper bound is deliberately coarse (arena membership), not
 * firstFreeProgramByte: PEM legitimately parks currentStep ==
 * firstFreeProgramByte (cursor on the .END. sentinel, a normal PEM
 * position), and a tighter bound rejects that valid position. */
uint8_t *forthOwningProgramStart(const uint8_t *ptr)
{
    uint8_t *arenaBase = (uint8_t *)ram;
    uint8_t *arenaEnd = arenaBase + (size_t)RAM_SIZE_IN_BLOCKS * BYTES_PER_BLOCK;
    if (!arenaBase || ptr < arenaBase || ptr >= arenaEnd) {
        return NULL;
    }

    /* Compute the greatest qualifying start explicitly, matching
     * forthNextProgramStart's own min-tracking below — do not rely on
     * programList being address-ascending; that is scanLabelsAndPrograms's
     * current behavior, not a documented contract this function may depend
     * on. */
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

/* §9.4: entry state governing an INSERTION at currentStep.
 * The new step will follow the step immediately BEFORE currentStep, so derive
 * from that predecessor. currentStep may sit past the pre-move of
 * addStepInProgram or past the committed line after ENTER
 * (pemCloseAlphaInput) — in both cases the predecessor is the step the spec's
 * "cursor lands on" language means.
 *
 * RTN does not itself close an open-ended (no closing »FORTH marker) region
 * — a routine can be entirely Forth source with RTN as its sole terminator.
 * When the predecessor is RTN, it is transparent: look through it to ITS
 * predecessor instead of concluding RPN, so a capture that empties all the
 * way back to RTN and reopens still resolves Forth. Bounded by progStart
 * like the outer walk; only RTN specifically is looked through, so a
 * genuine RPN predecessor is unaffected. */
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

/* ---- the console's value formatter ----
 *
 * There is no single landed register->text entry: _refreshRegisterLine
 * dispatches on getRegisterDataType() and calls a different display.c
 * producer per type. This mirrors that switch for the types a console line
 * can leave in X, using the CURRENT display mode — which is what makes `.`
 * and the ENTER echo agree with what the stack would have shown.
 *
 * copyRegisterToClipboardString is a landed, paint-free, all-types
 * register->string function built from the same family, but it is the
 * wrong SEMANTICS here: it produces full-precision CSV text, not the
 * display mode. So the shape is borrowed and the producers are not.
 *
 * Formats into the CALLER's buffer and never into tmpString: display.c
 * writes tmpString in ~190 places and the caller cannot know which producer
 * aliases it.
 *
 * standardFont, not numericFont: the transcript is a text row at the fnPem
 * pitch, so the width budget the formatter fits into is the one the
 * console actually paints with. */

/* Copy text into a fixed buffer, cutting only on a GLYPH boundary.
 *
 * The C47 string encoding is one byte below 0x80 and two bytes (high first)
 * at 0x80 and above — the same encoding screen.c decodes and the console
 * ring stores. A plain byte cut can therefore end a record with a lone lead
 * byte, which the painter and forthConsoleLineAt both re-pair with whatever
 * follows.
 *
 * Walks with upstream's own boundary primitive (stringNextGlyph) rather
 * than re-deriving the encoding here, so a change to the encoding cannot
 * leave this behind. Returns the bytes written. */
int32_t forthCopyWholeGlyphs(char *dst, const char *src, int32_t cap) {
  int32_t out = 0;
  int16_t at  = 0;
  int16_t next;

  if(cap <= 1) { if(cap == 1) { dst[0] = 0; } return 0; }

  while(src[at] != 0) {
    next = stringNextGlyph(src, at);
    if(next <= at)          { break; }          /* defensive: no progress */
    if(next > cap - 1)      { break; }          /* the glyph does not fit */
    at = next;
  }
  out = at;
  xcopy(dst, src, (uint32_t)out);
  dst[out] = 0;
  return out;
}

void forthConsoleFormatRegister(calcRegister_t regist, char *out, int16_t outSize)
{
  char buf[FORTH_CONSOLE_FMT_MAX];
  const int16_t maxWidth = SCREEN_WIDTH - 1;

  if(out == NULL || outSize <= 0) { return; }
  out[0] = 0;
  buf[0] = 0;

  switch(getRegisterDataType(regist)) {
    case dtReal34:
      real34ToDisplayString(REGISTER_REAL34_DATA(regist), getRegisterAngularMode(regist),
                            buf, &standardFont, maxWidth, NUMBER_OF_DISPLAY_DIGITS,
                            LIMITEXP, FRONTSPACE, FULLIRFRAC);
      break;

    case dtComplex34:
      complex34ToDisplayString(REGISTER_COMPLEX34_DATA(regist), buf, &standardFont, maxWidth,
                               NUMBER_OF_DISPLAY_DIGITS, LIMITEXP, FRONTSPACE, FULLIRFRAC,
                               getComplexRegisterAngularMode(regist),
                               getComplexRegisterPolarMode(regist) == amPolar);
      break;

    case dtLongInteger:
      longIntegerRegisterToDisplayString(regist, buf, (int32_t)sizeof(buf), maxWidth, 50,
                                         getSystemFlag(FLAG_LARGELI));
      break;

    case dtShortInteger:
      /* shortIntegerToDisplayString does NOT write from the front: it builds
       * digits from displayString[ERROR_MESSAGE_LENGTH / 2] upward as
       * scratch, then compacts them to the front, so the buffer it is
       * handed must be at least ERROR_MESSAGE_LENGTH bytes. The 256-byte
       * local `buf` is too small for that; this arm uses tmpString instead,
       * which is what every upstream caller passes and what the producer's
       * arithmetic requires. That does not weaken the "never tmpString"
       * rule above — that rule is about HOLDING a pointer into tmpString
       * across other producers that also write it; nothing runs between
       * this call and the copy-out below.
       *
       * The static asserts below pin this at BUILD time: no producer is
       * ever handed `out` directly (it is written once, by the clamped
       * copy at the end of this function), so a runtime canary on `out`
       * could never catch a regression here. */
      _Static_assert(TMP_STR_LENGTH >= ERROR_MESSAGE_LENGTH,
                     "C1/C22: shortIntegerToDisplayString writes from "
                     "ERROR_MESSAGE_LENGTH/2 upward, so its buffer must be at "
                     "least ERROR_MESSAGE_LENGTH bytes");
      _Static_assert(FORTH_CONSOLE_FMT_MAX < ERROR_MESSAGE_LENGTH,
                     "C1/C22: if the format buffer ever grows past "
                     "ERROR_MESSAGE_LENGTH, the comment above and the tmpString "
                     "detour must both be re-derived, not assumed");
      shortIntegerToDisplayString(regist, tmpString, false, noBaseOverride);
      /* The same glyph-boundary cut — shortIntegerToDisplayString can emit
       * two-byte glyphs (the base prefix and the sign). */
      forthCopyWholeGlyphs(buf, tmpString, (int32_t)sizeof(buf));
      break;

    case dtTime:
      timeToDisplayString(regist, buf, false);
      break;

    case dtDate:
      dateToDisplayString(regist, buf);
      break;

    case dtString:
      /* The string's own glyphs, not a quoted rendering: TYPE-class output
       * is the text itself. Glyph boundary, not byte boundary. */
      forthCopyWholeGlyphs(buf, REGISTER_STRING_DATA(regist), (int32_t)sizeof(buf));
      break;

    case dtReal34Matrix:
      if(!vectorToDisplayString(regist, buf)) { real34MatrixToDisplayString(regist, buf); }
      break;

    case dtComplex34Matrix:
      complex34MatrixToDisplayString(regist, buf);
      break;

    default:
      /* dtConfig and anything a later stage adds: name the type rather than
       * printing nothing, so the transcript never lies by omission. */
      snprintf(buf, sizeof(buf), "{type %u}", (unsigned)getRegisterDataType(regist));
      break;
  }

  { int32_t n = stringByteLength(buf);
    if(n > outSize - 1) { n = outSize - 1; }
    xcopy(out, buf, (uint32_t)n);
    out[n] = 0;
  }
}
