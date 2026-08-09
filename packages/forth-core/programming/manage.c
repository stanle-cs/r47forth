// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file manage.c
 ***********************************************/

#include "c47.h"
#include "forth_dict.h"
#include "forth_capture.h"
#include "forth_console.h"
#include "forth_menu.h"

// Structure of the program memory.
// In this example the RAM is 16384 blocks (from 0 to 16383) of 4 bytes = 65536 bytes.
// The program memory occupies the end of the RAM area.
//
//  +-----+------+----+----------+
//  |Block| Step |Code|    OP    |
//  |     |loc/gl|    |          |
//  +-----+------+----+----------+
//  |16374| 1/ 1 |   1| LBL 'P1' | <-- beginOfProgramMemory
//  |     |      | 253|          |
//  |     |      |   2|          |
//  |     |      | 'P'|          |
//  |16375|      | '1'|          |  ^
//  |     | 2/ 2 | 114| 1.       |  | 1 block = 4 bytes
//  |     |      |   6|          |  |
//  |     |      |   1|          |  v
//  |16376|      | '1'|          |
//  |     | 3/ 3 |  95| +        |
//  |     | 4/ 4 | 133| END      |
//  |     |      | 168|          |
//  |16377| 1/ 5 |   1| LBL 'P2' | <-- beginOfCurrentProgram  <-- firstDisplayedStep   firstDisplayedStepNumber = 5   firstDisplayedLocalStepNumber = 1
//  |     |      | 253|          |
//  |     |      |   2|          |
//  |     |      | 'P'|          |
//  |16378|      | '2'|          |
//  |     | 2/ 6 | 114| 22.      | <-- currentStep       this inequality is always true: beginOfCurrentProgram ≤ currentStep < endOfCurrentProgram
//  |     |      |   6|          |     currentStepNumber = 6   currentLocalStepNumber = 2
//  |     |      |   2|          |     currentProgramNumber = 2
//  |16379|      | '2'|          |
//  |     |      | '2'|          |
//  |     | 3/ 7 |  95| +        |
//  |     | 4/ 8 | 133| END      |
//  |16380|      | 168|          |
//  |     | 1/ 9 |   2| LBL 'P3' | <-- endOfCurrentProgram
//  |     |      | 253|          |
//  |     |      |   2|          |
//  |16381|      | 'P'|          |
//  |     |      | '3'|          |
//  |     | 2/10 | 114| 3.       |
//  |     |      |   6|          |
//  |16382|      |   1|          |
//  |     |      | '3'|          |
//  |     | 3/11 |  95| +        |
//  |     | 4/12 | 133| END      |
//  |16383| 5/13 | 168|          |
//  |     |      | 255| .END.    | <-- firstFreeProgramByte
//  |     |      | 255|          |
//  |     |      |   ?|          | free byte     This byte is the end of the RAM area
//  +-----+------+----+----------+
//
//  freeProgramBytes = 1

static void _pemCloseAngleInput(int item);
static void _pemCloseDateInput(void);
static void _pemCloseTimeInput(void);

bool_t isAtEndOfPrograms(const uint8_t *step) {
  return (step == NULL) || (*step == 255 && *(step + 1) == 255);
}



bool_t checkOpCodeOfStep(const uint8_t *step, uint16_t op) {
  if(op < 128) {
    return step && *step == op;
  }
  else {
    return step && (*step & 0x7f) == (op >> 8) && *(step + 1) == (op & 0xff);
  }
}



// A label or variable name stored in a program step is preceded by a one-byte
// length that is taken from program memory on trust. A corrupt or crafted program
// - a restored state file or imported program - can claim a name longer than the
// bytes that remain. Clamp the claimed length to the span before
// firstFreeProgramByte so a name read can never run past the program region.
// When the name would start at or past firstFreeProgramByte there are no valid
// bytes left (the scan can register a label in the gap up to the RAM end), so
// return 0 rather than skipping the clamp and leaving the length unbounded.
uint8_t boundProgramNameLength(const uint8_t *nameStart, uint8_t claimedLength) {
  if(nameStart >= firstFreeProgramByte) {
    return 0;
  }
  if(claimedLength > firstFreeProgramByte - nameStart) {
    return (uint8_t)(firstFreeProgramByte - nameStart);
  }
  return claimedLength;
}


// A label name longer than MAX_LABEL_NAME_LENGTH cannot have been produced by the calculator and can only come from a corrupt or crafted file.
// Such a name does not fit the fixed name buffers of its consumers, ASSIGN's argumentName[16] and tamBuffer[32], so the loaders use this walk to reject it.
// The walk runs from the given step to the end of program memory; a step the walker cannot decode ends it, where scanLabelsAndPrograms() truncates too.
bool_t programMemoryHasOverlongLabelName(uint8_t *step) {
  while(programBytesAvailable(step, 2) && !isAtEndOfPrograms(step)) {
    if(checkOpCodeOfStep(step, ITM_LBL)
        && (*(step + 1) == STRING_LABEL_VARIABLE || *(step + 1) == LOCAL_LABEL_VARIABLE)
        && programBytesAvailable(step, 3)
        && *(step + 2) > MAX_LABEL_NAME_LENGTH) {
      return true;
    }
    step = findNextStep(step);
    if(step == NULL) {
      break;
    }
  }
  return false;
}


void scanLabelsAndPrograms(void) {
  uint32_t stepNumber = 0;
  uint8_t *nextStep, *step = beginOfProgramMemory;
  // Hard upper bound of the program region; a step that would advance past it has
  // a corrupt length and must not be walked, or findNextStep reads out of bounds.
  uint8_t * const programRegionEnd = (uint8_t *)(ram + RAM_SIZE_IN_BLOCKS);

  freeC47Blocks(labelList, TO_BLOCKS(sizeof(labelList_t)) * numberOfLabels);
  freeC47Blocks(programList, TO_BLOCKS(sizeof(programList_t)) * numberOfPrograms);

  numberOfLabels = 0;
  numberOfPrograms = 1;
  while(!isAtEndOfPrograms(step)) { // .END.
    if(*step == ITM_LBL) { // LBL
      numberOfLabels++;
    }
    nextStep = findNextStep(step);
    if(nextStep == NULL || nextStep <= step || nextStep >= programRegionEnd) {
      lastErrorCode = ERROR_UNDEFINED_OPCODE; // this step and everything after it are dropped
      break;
    }
    if(isAtEndOfProgram(step)) { // END
      if(!isAtEndOfPrograms(nextStep)) { // .END. following END is not the start of a new program
        numberOfPrograms++;
      }
    }
    step = nextStep;
  }

  labelList = allocC47Blocks(TO_BLOCKS(sizeof(labelList_t)) * numberOfLabels);
  if(labelList == NULL) {
    // unlikely
    lastErrorCode = ERROR_RAM_FULL;
    return;
  }

  programList = allocC47Blocks(TO_BLOCKS(sizeof(programList_t)) * numberOfPrograms);
  if(programList == NULL) {
    // unlikely
    lastErrorCode = ERROR_RAM_FULL;
    return;
  }

  numberOfLabels = 0;
  step = beginOfProgramMemory;
  programList[0].instructionPointer = step;
  programList[0].step = (0 + 1);
  numberOfPrograms = 1;
  stepNumber = 1;
  while(!isAtEndOfPrograms(step)) { // .END.
    nextStep = findNextStep(step);
    if(nextStep == NULL || nextStep <= step || nextStep >= programRegionEnd) {
      lastErrorCode = ERROR_UNDEFINED_OPCODE; // the labels and programs past it are dropped
      break;
    }
    if(checkOpCodeOfStep(step, ITM_LBL)) { // LBL
      labelList[numberOfLabels].program = numberOfPrograms;
      if(*(step + 1) <= LAST_LOCAL_LABEL) { // Local label
        labelList[numberOfLabels].step = -stepNumber;
        labelList[numberOfLabels].labelPointer = step + 1;
      }
      else if(*(step + 1) == LOCAL_LABEL_VARIABLE) { // Local named label
        labelList[numberOfLabels].step = -stepNumber;
        labelList[numberOfLabels].labelPointer = step + 2;
      }
      else { // Global label
        labelList[numberOfLabels].step = stepNumber;
        labelList[numberOfLabels].labelPointer = step + 2;
      }

      labelList[numberOfLabels].instructionPointer = nextStep;
      numberOfLabels++;
    }

    if(isAtEndOfProgram(step)) { // END
      if(!isAtEndOfPrograms(nextStep)) { // .END. following END is not the start of a new program
        programList[numberOfPrograms].instructionPointer = step + 2;
        programList[numberOfPrograms].step = stepNumber + 1;
        numberOfPrograms++;
      }
    }

    step = nextStep;
    stepNumber++;
  }

  //The folowing 2 lines added to address the FFFFFFFF issue in old state files
  firstFreeProgramByte = step;
  freeProgramBytes = (((uint8_t *)(ram + RAM_SIZE_IN_BLOCKS)) - firstFreeProgramByte) - 2;

  defineCurrentProgramFromCurrentStep();
  defineFirstDisplayedStep();
}



void deleteStepsFromTo(uint8_t *from, uint8_t *to) {
  uint16_t opSize = to - from;

  xcopy(from, to, (firstFreeProgramByte - to) + 2);
  firstFreeProgramByte -= opSize;
  freeProgramBytes += opSize;
  scanLabelsAndPrograms();
}



static void _removeLabelsAssignments() {
  int16_t i;
  char label[256]; // a global label name is a 1-byte-length string, so up to 255 bytes
  uint8_t labelLength;
  for(i=0; i<numberOfLabels; i++) {
    if((labelList[i].program == currentProgramNumber) && (labelList[i].step > 0)) {
      labelLength = boundProgramNameLength(labelList[i].labelPointer + 1, labelList[i].labelPointer[0]);
      xcopy(label, labelList[i].labelPointer + 1, labelLength);
      label[labelLength]=0;
      removeUserItemAssignments(ITM_XEQ, label);   // Remove label assignments
    }
  }
}



void fnClPAll(uint16_t confirmation) {
  if(confirmation == NOT_CONFIRMED) {
    setConfirmationMode(fnClPAll);
  }
  else {
    // Remove assignments of all global labels, before deleting all programs
    removeUserItemAssignments(ITM_XEQ, "");   // Remove all labels assignments

    bool_t wasInRam = (programList[currentProgramNumber - 1].step > 0);
    resizeProgramMemory(1); // 1 block for an empty program
    *(beginOfProgramMemory + 0)   = (ITM_END >> 8) | 0x80;
    *(beginOfProgramMemory + 1)   =  ITM_END       & 0xff;
    *(beginOfProgramMemory + 2)   = 255; // .END.
    *(beginOfProgramMemory + 3)   = 255; // .END.
    firstFreeProgramByte          = beginOfProgramMemory + 2;
    freeProgramBytes              = 0;
    temporaryInformation          = TI_NO_INFO;
    programRunStop                = PGM_STOPPED;

    if(wasInRam) { // Not in flash
      currentStep                   = beginOfProgramMemory;
      firstDisplayedStep            = beginOfProgramMemory;
      firstDisplayedLocalStepNumber = 0;
      currentLocalStepNumber        = 1;
      beginOfCurrentProgram         = beginOfProgramMemory;
      endOfCurrentProgram           = firstFreeProgramByte;
    }

    scanLabelsAndPrograms();
    if(programRunStop != PGM_RUNNING) {
      temporaryInformation = TI_DEL_ALL_PRGMS;
    }
    else {
      temporaryInformation = TI_NO_INFO;
    }
    screenUpdatingMode = SCRUPD_AUTO;
  }
}



/* AUDIT round 8 (R8-1): defined with the fold context far below; declared
 * here because the DELETER is where upstream adjusts saved cursors, and the
 * fold holds one. */
static void _forthFoldNoteProgramDeleted(uint16_t deletedProgramNumber);

static int _clearProgram(void) {
  if(beginOfCurrentProgram == beginOfProgramMemory && (endOfCurrentProgram >= firstFreeProgramByte || (*endOfCurrentProgram == 255 && *(endOfCurrentProgram + 1) == 255))) { // There is only one program in memory
    fnClPAll(CONFIRMED);
    return 1;
  }
  else {
    // Remove assignments of global labels in the program being deleted, before deleting the program
    _removeLabelsAssignments();

    uint16_t savedCurrentProgramNumber = currentProgramNumber;

    /* AUDIT round 8 (R8-1), following upstream's own convention rather than
     * working around it: the DELETER adjusts every saved cursor.  fnClP
     * renumbers its own `savedCurrentProgramNumber` when the deleted program
     * precedes it (:354-357) and this function clamps its own below; the
     * Forth fold context holds a THIRD copy of the same quantity, and this
     * is the one place that knows which program is going.  Same rule, same
     * shape, same moment.
     *
     * The alternative — repairing it at forthFoldLeave — was tried and
     * cannot work: that site can count that a program went, never WHICH. */
    _forthFoldNoteProgramDeleted(currentProgramNumber);

    goToPgmStep(currentProgramNumber, 1);  // [DL] work around for crash when label deleted is not at the beginning of the program
    firstDisplayedLocalStepNumber = 0;     // ditto

    deleteStepsFromTo(beginOfCurrentProgram, endOfCurrentProgram - ((currentProgramNumber == numberOfPrograms) ? 2 : 0));
    scanLabelsAndPrograms();
    // unlikely fails

    if(savedCurrentProgramNumber >= numberOfPrograms) { // The last program
      goToPgmStep(numberOfPrograms, 1);
    }
    else { // Not the last program
      goToPgmStep(savedCurrentProgramNumber, 1);
    }
    return 0;
  }
}



void fnClP(uint16_t label) {
  uint16_t savedCurrentLocalStepNumber = currentLocalStepNumber;
  uint16_t savedCurrentProgramNumber = currentProgramNumber;

  while(currentSubroutineLevel > 0) { // drop subroutine stack before deleting a program
    fnReturn(0);
  }
  fnReturn(0); // 1 more time to clean local registers

  goToPgmStep(savedCurrentProgramNumber, savedCurrentLocalStepNumber);

  if(label == 0 && !tam.alpha && tam.digitsSoFar == 0) {
    uint16_t savedCurrentProgramNumber = currentProgramNumber;
    const int result = _clearProgram();
    if(result == 1 && savedCurrentProgramNumber <= 1) {
      fnGotoDot(1);
    }
  }
  else if(label >= FIRST_LABEL && label <= LAST_LABEL) {
    fnGoto(label);
    const uint16_t programNumberToDelete = currentProgramNumber;
    const int result = _clearProgram();
    switch(result) {
      case 2: {
        goToPgmStep(savedCurrentProgramNumber, savedCurrentLocalStepNumber);
        break;
      }
      case 1: {
        if(savedCurrentProgramNumber <= 1) { // RAM
          fnGotoDot(1);
        }
        break;
      }
      case 0: {
        if(programNumberToDelete != savedCurrentProgramNumber) {
          if(programNumberToDelete < savedCurrentProgramNumber) {
            --savedCurrentProgramNumber;
          }
          goToPgmStep(savedCurrentProgramNumber, savedCurrentLocalStepNumber);
          break;
        }
        break;
      }
      default: {
        break;
      }
    }
  }
  else {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "label %" PRIu16 " is not a global label", label);
      moreInfoOnError("In function fnClP:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
  }
}



uint32_t _getProgramSize(void) {
  if(currentProgramNumber == numberOfPrograms) {
    uint8_t *step = programList[currentProgramNumber - 1].instructionPointer;
    while(!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) { // END or .END.
      step = findNextStep(step);
    }
    return (uint32_t)(step - programList[currentProgramNumber - 1].instructionPointer + 2);
  }
  else {
    return (uint32_t)(programList[currentProgramNumber].instructionPointer - programList[currentProgramNumber - 1].instructionPointer);
  }
}



void defineCurrentProgramFromGlobalStepNumber(int16_t globalStepNumber) {
  currentProgramNumber = 0;
  while(globalStepNumber >= programList[currentProgramNumber].step) {
    currentProgramNumber++;
    if(currentProgramNumber >= numberOfPrograms) {
      break;
    }
  }

  if(currentProgramNumber == numberOfPrograms) {
    endOfCurrentProgram = programList[currentProgramNumber - 1].instructionPointer + _getProgramSize();
  }
  else {
    endOfCurrentProgram = programList[currentProgramNumber].instructionPointer;
  }
  beginOfCurrentProgram = programList[currentProgramNumber - 1].instructionPointer;
}



void defineCurrentProgramFromCurrentStep(void) {
  if(beginOfProgramMemory <= currentStep && currentStep <= firstFreeProgramByte) {
    currentProgramNumber = 0;
    while(currentStep >= programList[currentProgramNumber].instructionPointer) {
      currentProgramNumber++;
      if(currentProgramNumber >= numberOfPrograms) {
        break;
      }
    }

    if(currentProgramNumber >= numberOfPrograms) {
      endOfCurrentProgram = programList[currentProgramNumber - 1].instructionPointer + _getProgramSize();
    }
    else {
      endOfCurrentProgram = programList[currentProgramNumber].instructionPointer;
    }
    beginOfCurrentProgram = programList[currentProgramNumber - 1].instructionPointer;
  }
}



void scrollPemBackwards(void) {
  if(firstDisplayedLocalStepNumber > 0) {
    --firstDisplayedLocalStepNumber;
  }
  defineFirstDisplayedStep();
}

void scrollPemForwards(void) {
  if(getNumberOfSteps() > 6) {
    if(currentLocalStepNumber > 3) {
      ++firstDisplayedLocalStepNumber;
      firstDisplayedStep = findNextStep(firstDisplayedStep);
    }
    else if(currentLocalStepNumber == 3) {
      firstDisplayedLocalStepNumber = 1;
    }
  }
}


int32_t pemLeftOffset(int32_t y) {
  if(y > Y_POSITION_OF_REGISTER_T_LINE || X_SHIFT == X_SHIFT_R || Y_SHIFT == 0){
    return 0;
  }
  else {
    return 16; //Offset to allow for f/g
  }
}


static bool_t _isAngleType(uint8_t literalType) {
  switch(literalType) {
    case STRING_ANGLE_RADIAN:
    case STRING_ANGLE_GRAD:
    case STRING_ANGLE_DEGREE:
    case STRING_ANGLE_MULTPI: {
      return true;
      break;
    }
    default: {
      return false;
    }
  }
}


void fnPem(uint16_t unusedButMandatoryParameter) {
    ///////////////////////////////////////////////////////////////////////////////////////
    // For this function to work properly we need the following variables set properly:
    //  - currentProgramNumber
    //  - currentLocalStepNumber
    //  - firstDisplayedLocalStepNumber
    //  - firstDisplayedStep
    //
    uint32_t currentStepNumber, firstDisplayedStepNumber;
    uint16_t line, firstLine;
    uint16_t stepsThatWouldBeDisplayed = 7;
    uint8_t *step, *nextStep;
    bool_t lblOrEnd, lblOrEndOrXeq, gto;
    bool_t inTamMode = tam.mode && programList[currentProgramNumber - 1].step > 0;
    uint16_t numberOfSteps = getNumberOfSteps();
    uint16_t linesOfCurrentStep = 1;
    lastIntegerBase = 0;

    if(calcMode != CM_PEM) {
      calcMode = CM_PEM;
      showSoftmenu(-MNU_PFN);
      screenUpdatingMode &= ~SCRUPD_MANUAL_MENU;
      hourGlassIconEnabled = false;
      aimBuffer[0] = 0;
      currentInputVariable = INVALID_VARIABLE;
      refreshScreen(227);
      return;
    }

    if(currentLocalStepNumber < firstDisplayedLocalStepNumber) {
      firstDisplayedLocalStepNumber = currentLocalStepNumber;
      defineFirstDisplayedStep();
    }

    if(currentLocalStepNumber == 0) {
      currentLocalStepNumber = 1;
      pemCursorIsZerothStep = true;
    }
    else if(currentLocalStepNumber > 1) {
      pemCursorIsZerothStep = false;
    }

    currentStepNumber        = currentLocalStepNumber        + abs(programList[currentProgramNumber - 1].step) - 1;
    firstDisplayedStepNumber = firstDisplayedLocalStepNumber + abs(programList[currentProgramNumber - 1].step) - 1;
    step                     = firstDisplayedStep;
    programListEnd           = false;
    lastProgramListEnd       = false;

    if(firstDisplayedLocalStepNumber == 0) {
      showString("0000:" STD_SPACE_4_PER_EM, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE) + 1, Y_POSITION_OF_REGISTER_T_LINE, (pemCursorIsZerothStep && !tam.mode && aimBuffer[0] == 0) ? vmReverse : vmNormal, false, true);
      sprintf(tmpString, "{Prgm #%" PRIu16 "/%" PRIu16 ": %" PRIu32 " bytes / %" PRIu16 " step%s}", currentProgramNumber, numberOfPrograms, _getProgramSize(), numberOfSteps, numberOfSteps == 1 ? "" : "s");
      showString(tmpString, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE) + 42, Y_POSITION_OF_REGISTER_T_LINE, vmNormal, false, false);
      firstLine = 1;
    }
    else {
      firstLine = 0;
    }

    int lineOffset = 0, lineOffsetTam = 0;

    for(line=firstLine; line<7; line++) {
      nextStep = findNextStep(step);
      //uint16_t stepSize = (uint16_t)(nextStep - step);
      sprintf(tmpString, "%04d:" STD_SPACE_4_PER_EM, firstDisplayedLocalStepNumber + line - lineOffset + lineOffsetTam);
      if(firstDisplayedStepNumber + line - lineOffset == currentStepNumber) {
        tamOverPemYPos = Y_POSITION_OF_REGISTER_T_LINE + 21 * line;
        showString(tmpString, &standardFont, pemLeftOffset(tamOverPemYPos) + 1, tamOverPemYPos, ((pemCursorIsZerothStep && !tam.mode && aimBuffer[0] == 0) || (tam.mode && (programList[currentProgramNumber - 1].step > 0))) ? vmNormal : vmReverse, false, true);
        currentStep = step;
      }
      else {
        showString(tmpString, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE + 21 * line) + 1, Y_POSITION_OF_REGISTER_T_LINE + 21 * line, vmNormal,  false, true);
      }

      //Automatically, when on battery (hence low processor), change to skip long processing register printing, recovering the fragmented screen here: See timer.c fnTimerEndOfActivity() , skippedStackLines
      #if defined(DMCP_BUILD)                                                      // vvv
        if( !(!runningOnSimOrUSB && !emptyKeyBuffer() && key_empty() == 1) ||(firstDisplayedStepNumber + line - lineOffset == currentStepNumber)) {
      #endif

      lblOrEndOrXeq = checkOpCodeOfStep(step, ITM_LBL) || isAtEndOfProgram(step) || isAtEndOfPrograms(step) || checkOpCodeOfStep(step, ITM_XEQ);
      lblOrEnd =      checkOpCodeOfStep(step, ITM_LBL) || isAtEndOfProgram(step) || isAtEndOfPrograms(step);
      gto = checkOpCodeOfStep(step, ITM_GTO);
      if(programList[currentProgramNumber - 1].step > 0) {
        if((!pemCursorIsZerothStep && firstDisplayedStepNumber + line - lineOffset == currentStepNumber + 1) || (line == 1 && tam.mode && pemCursorIsZerothStep)) {
          tamOverPemYPos = Y_POSITION_OF_REGISTER_T_LINE + 21 * line;
          if(tam.mode) {
            line += 1;
            lineOffset += 1;
            lineOffsetTam += 1;
            showString(tmpString, &standardFont, pemLeftOffset(tamOverPemYPos) + 1, tamOverPemYPos, vmReverse, false, true);
            if(line >= 7) {
              break;
            }
            sprintf(tmpString, "%04d:" STD_SPACE_4_PER_EM, firstDisplayedLocalStepNumber + line - lineOffset + lineOffsetTam);
            showString(tmpString, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE + 21 * line) + 1, Y_POSITION_OF_REGISTER_T_LINE + 21 * line, vmNormal, false, true);
          }
        }
        else if(firstDisplayedStepNumber + line - lineOffset == currentStepNumber && lblOrEnd && (*step != ITM_LBL)) {
          if(tam.mode) {
            line += 1;
            lineOffset += 1;
            lineOffsetTam += 1;
            showString(tmpString, &standardFont, pemLeftOffset(tamOverPemYPos) + 1, tamOverPemYPos, vmReverse, false, true);
            if(line >= 7) {
              break;
            }
            sprintf(tmpString, "%04d:" STD_SPACE_4_PER_EM, firstDisplayedLocalStepNumber + line - lineOffset + lineOffsetTam);
            showString(tmpString, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE + 21 * line) + 1, Y_POSITION_OF_REGISTER_T_LINE + 21 * line, vmNormal, false, true);
          }
        }
      }
      decodeOneStep(step);
      if(firstDisplayedStepNumber + line - lineOffset == currentStepNumber && !tam.mode) {
        if(getSystemFlag(FLAG_ALPHA)) {
          char tmpChar4 = tmpString[4];
          char tmpChar6 = tmpString[6];
          int16_t cursorInString;
          tmpString[6] = 0;
          tmpString[4] = 0;
          if(tam.function == ITM_FORTH) {
            /* R3-1: a non-empty ITM_FORTH source step is decoded BARE
             * (decodeRem, §8.5) — no two-byte opening quote to skip. The
             * ordinary-literal/REM/42-string branches below all assume that
             * quote and are followed by an unconditional +2; give Forth a
             * zero-byte prefix so cursorInString+2 lands before the first
             * real payload byte instead of on top of the second one. */
            cursorInString = T_cursorPos - 2;
          }
          else {
            cursorInString = (strcmp(tmpString, "REM ") == 0 ? T_cursorPos + 4 : (strcmp(tmpString, "42" STD_alpha) == 0)  || (strcmp(tmpString, "42" STD_RIGHT_TACK) == 0) ? T_cursorPos +5 : T_cursorPos);
          }
          tmpString[4] = tmpChar4;
          tmpString[6] = tmpChar6;
          xcopy(tmpString + 2 + cursorInString + 2, tmpString + 2 + cursorInString, stringByteLength(tmpString + 2 + cursorInString) + 1);
          tmpString[2 + cursorInString    ] = STD_CURSOR[0];
          tmpString[2 + cursorInString + 1] = STD_CURSOR[1];
        }
        else if(aimBuffer[0] != 0) {
          char *tstr = tmpString + stringByteLength(tmpString);
          lastIntegerBase = decodedIntegerBase;
          if((lastIntegerBase != 0) && ((nimNumberPart == NP_INT_10) || (nimNumberPart == NP_INT_16))) {
            tstr -= 2;
            *(tstr++) = STD_CURSOR[0];
            *(tstr++) = STD_CURSOR[1];
            *(tstr++) = baseChars[lastIntegerBase * 2    ];
            *(tstr++) = baseChars[lastIntegerBase * 2 + 1];
          }
          else if(_isAngleType(editingLiteralType)) {
            tstr -= 2;                    // to overwrite angle symbol with cursor
            *(tstr++) = STD_CURSOR[0];
            *(tstr++) = STD_CURSOR[1];
            *(tstr++) = angleChars[(editingLiteralType - STRING_ANGLE_RADIAN) * 2    ];
            *(tstr++) = angleChars[(editingLiteralType - STRING_ANGLE_RADIAN) * 2 + 1];
          }
          else {
            *(tstr++) = STD_CURSOR[0];
            *(tstr++) = STD_CURSOR[1];
          }
          *(tstr++) = 0;
        }
      }
      // Split long lines
      int numberOfExtraLines = 0;
      int offset = 0;
      const char *endStr = NULL;
      while(offset <= 1500 && (*(endStr = stringAfterPixels(tmpString + offset, &standardFont, 337, false, false)) != 0)) {
        int lineByteLength = endStr - (tmpString + offset);
        numberOfExtraLines++;
        xcopy(tmpString + offset + 300, tmpString + offset + lineByteLength, stringByteLength(endStr) + 1);
        tmpString[offset + lineByteLength] = 0;
        offset += 300;
      }
      stepsThatWouldBeDisplayed -= numberOfExtraLines;
      if(firstDisplayedStepNumber + line - lineOffset == currentStepNumber) {
        linesOfCurrentStep += numberOfExtraLines;
      }

      showString(tmpString, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE + 21 * line) + (lblOrEndOrXeq ? 42 : gto ? 82 : 62), Y_POSITION_OF_REGISTER_T_LINE + 21 * line, vmNormal,  false, false);
      offset = 300;
      while(numberOfExtraLines && line <= 5) {
        line++;
        showString(tmpString + offset, &standardFont, pemLeftOffset(Y_POSITION_OF_REGISTER_T_LINE + 21 * (line)) + 62, Y_POSITION_OF_REGISTER_T_LINE + 21 * (line), vmNormal,  false, false);
        numberOfExtraLines--;
        offset += 300;
        lineOffset++;
      }
      if(isAtEndOfProgram(step)) {
        programListEnd = true;
        if(*nextStep == 255 && *(nextStep + 1) == 255) {
          lastProgramListEnd = true;
        }
        break;
      }
      if((*step == 255) && (*(step + 1) == 255)) {
        programListEnd = true;
        lastProgramListEnd = true;
        break;
      }
      step = nextStep;

      #if defined(DMCP_BUILD)   //^^
        }
      #endif //DMCP_BUILD
    }

    if(lastErrorCode != ERROR_NONE) {
      refreshRegisterLine(errorMessageRegisterLine);
    }

    if(aimBuffer[0] != 0 && linesOfCurrentStep > 4) { // Limited to 4 lines so as not to cause crash or freeze
      if(getSystemFlag(FLAG_ALPHA)) {
        pemAlpha(ITM_BACKSPACE);
      }
      else {
        pemAddNumber(ITM_BACKSPACE, true);
      }

      clearScreen(13);
      showSoftmenuCurrentPart();
      fnPem(NOPARAM);
    }
    if((currentLocalStepNumber + (inTamMode ? (currentLocalStepNumber < numberOfSteps ? 2 : 1) : 0)) >= (firstDisplayedLocalStepNumber + stepsThatWouldBeDisplayed)) {
      firstDisplayedLocalStepNumber = currentLocalStepNumber - stepsThatWouldBeDisplayed + 1 + (inTamMode ? (currentLocalStepNumber < numberOfSteps ? 2 : 1) : 0);
      if(inTamMode && (firstDisplayedLocalStepNumber > 1) && (currentLocalStepNumber + 1 >= (firstDisplayedLocalStepNumber + stepsThatWouldBeDisplayed))) {
        ++firstDisplayedLocalStepNumber;
      }

      defineFirstDisplayedStep();
      clearScreen(14);
      showSoftmenuCurrentPart();
      fnPem(NOPARAM);
    }
}



static void _insertInProgram(const uint8_t *dat, uint16_t size) {
  //#define printarr(fmt, dat, len) for (uint16_t i = 0; i < len; i++) printf(fmt, dat[i])
  //printarr("%d ", dat, size);
  //printf("\n");fflush(stdout);
  int16_t _dynamicMenuItem = dynamicMenuItem;
  uint16_t globalStepNumber;

  if(freeProgramBytes < size) {
    uint8_t *oldBeginOfProgramMemory = beginOfProgramMemory;
    uint32_t programSizeInBlocks = RAM_SIZE_IN_BLOCKS - TO_C47MEMPTR(beginOfProgramMemory);
    uint32_t newProgramSizeInBlocks = TO_BLOCKS(TO_BYTES(programSizeInBlocks) - freeProgramBytes + size);
    freeProgramBytes      += TO_BYTES(newProgramSizeInBlocks - programSizeInBlocks);
    resizeProgramMemory(newProgramSizeInBlocks);
    currentStep           = currentStep           - oldBeginOfProgramMemory + beginOfProgramMemory;
    firstDisplayedStep    = firstDisplayedStep    - oldBeginOfProgramMemory + beginOfProgramMemory;
    beginOfCurrentProgram = beginOfCurrentProgram - oldBeginOfProgramMemory + beginOfProgramMemory;
    endOfCurrentProgram   = endOfCurrentProgram   - oldBeginOfProgramMemory + beginOfProgramMemory;
  }

  for(uint8_t *pos = firstFreeProgramByte + 1 + size; pos > currentStep; --pos) {
    *pos = *(pos - size);
  }





  #define tmpA (dat[1]+((dat[0] & 0x7F) << 8))    //convert codes for v3>xyz and xyz>v3 respecting 3D_PHYS
  uint16_t tmpB = 0;
  if(size == 2 && (tmpA == ITM_STKtoV3 || tmpA == ITM_V3toSTK)) {
    switch(tmpA) {
      case ITM_STKtoV3 : tmpB = getSystemFlag(FLAG_3DPHYS) == false ? ITM_STKtoV3_M : ITM_STKtoV3_P; break;
      case ITM_V3toSTK : tmpB = getSystemFlag(FLAG_3DPHYS) == false ? ITM_V3toSTK_M : ITM_V3toSTK_P; break;
      default:;
    }
    *(currentStep++) = (tmpB >> 8) | 0x80;
    *(currentStep++) = tmpB & 0x00FF;
  }
  else if(size == 2 && (tmpA == ITM_toPOL2 || tmpA == ITM_toREC2)) {  // convert >RECT and >POLAR to the relevant ones, respecting RP_HP
    tmpB = ITM_toPOL_HP + (tmpA - ITM_toPOL2) + (getSystemFlag(FLAG_HPRP) ? 0 : 2);
    *(currentStep++) = (tmpB >> 8) | 0x80;
    *(currentStep++) = tmpB & 0x00FF;
  }
  else {    //otherwise use the input data to add a step
    for(uint16_t i = 0; i < size; ++i) {
      *(currentStep++) = *(dat++);
    }
  }

  firstFreeProgramByte    += size;
  freeProgramBytes        -= size;
  currentLocalStepNumber  += 1;
  endOfCurrentProgram     += size;
  globalStepNumber = currentLocalStepNumber + programList[currentProgramNumber - 1].step - 1;
  scanLabelsAndPrograms();
  dynamicMenuItem = -1;
  goToGlobalStep(globalStepNumber);
  dynamicMenuItem = _dynamicMenuItem;
}

static void _closeAlphaMenus(void) {
  for(int i = 0; i < SOFTMENU_STACK_SIZE; ++i) {
    switch(-softmenu[softmenuStack[0].softmenuId].menuItem) {
      case MNU_ALPHAINTL:
      case MNU_ALPHAintl:
      case MNU_ALPHAMATH:
      case MNU_ALPHA_OMEGA:
      case MNU_alpha_omega:
      case MNU_ALPHA:
        popSoftmenu();
        break;

      case MNU_MyAlpha:
        switch(-softmenu[softmenuStack[1].softmenuId].menuItem) {
          case MNU_ALPHAINTL:
          case MNU_ALPHAintl:
          case MNU_ALPHAMATH:
          case MNU_ALPHA_OMEGA:
          case MNU_alpha_omega:
          case MNU_ALPHA:
            popSoftmenu();
            break;
          default:
            softmenuStack[0].softmenuId = 0; // MyMenu
            return;
        }

      default:
        return;
    }
  }
  // Just in case softmenuStack was filled with AIM-related menus
  for(int i = 0; i < SOFTMENU_STACK_SIZE; ++i) {
    softmenuStack[i].softmenuId = 0; // MyMenu
  }
}

void forthCaptureSanitizeRestoredUi(void) {
  /* AUDIT round 3 — the C17 frame stamp MUST be cleared here, and it is
   * deliberately ABOVE the gate below.
   *
   * Frame ownership rides softmenuStack[].userMenuId, and softmenuStack is
   * persisted WHOLESALE as a hex dump (saveRestoreBackup.c:293/:986).  So a
   * backup taken with the console open carries the stamps into the file, and
   * the restore writes them back into the live stack — at :986, which is
   * AFTER the dict-lifecycle seam at :975-976 whose forthCapPowerReset() ->
   * forthCapClose() -> forthConsoleUnstampAll() was supposed to clear them.
   * The unstamp is silently overwritten moments later, exactly as a
   * FLAG_ALPHA clear attempted in that seam would be (the F6-6 finding, and
   * the reason THIS function exists).
   *
   * The consequence is not cosmetic: a stale stamp with no capture open makes
   * the next console open's forthConsoleRegisterSlot0() a no-op (it declines
   * when a stamp already exists), so that session registers nothing and its
   * EXIT reads ownership off a frame belonging to a capture that ended before
   * the restore.
   *
   * This is the defect the old homePushed bit did NOT have — it was capture
   * state, explicitly never persisted.  Moving ownership into the frame
   * (which is right, and is what C17 needed) moved it into a PERSISTED
   * structure, and this is the seam that pays for that.
   *
   * Unconditional, and above the early return: a restore always lands with
   * the capture CLOSED (forthCap.state is process-local), so no live stamp
   * can exist here to protect, and the gate below is CM_PEM-only — the
   * interactive origin would never reach it.  Found by four of seven
   * independent readers in round 3. */
  forthConsoleUnstampAll();

  /* forthCap.state is process-local and is reset before restoreCalc()
   * reloads the persisted UI fields, so a backup taken mid-capture restores
   * CM_PEM + ALPHA + tam.function == ITM_FORTH around a CLOSED capture.
   * Force that to a clean closed state; the source step is already committed
   * (per-keystroke recommit), so nothing is lost — the user reopens the line
   * with EDIT.
   *
   * Premise note: post-S3 every ingredient of the capture IS persisted
   * except this one process-local flag, so this discards recoverable
   * state rather than repairing broken state.  Resuming intact instead
   * would change the §8 A5 power-off contract and awaits an owner
   * ruling — see DESIGN-HISTORY.md 2026-07-25 (design audit). */
  if(calcMode != CM_PEM
     || tam.function != ITM_FORTH
     || forthCapIsOpen()) {
    return;
  }

  aimBuffer[0] = 0;
  T_cursorPos = 0;
  displayAIMbufferoffset = 0;
  clearSystemFlag(FLAG_ALPHA);
  calcModeNormalGui();
  _closeAlphaMenus();
  tam.function = 0;
}

/* §8.1: an ITM_FORTH capture step with empty text is the OPEN-CAPTURE
 * PLACEHOLDER — len=1, single 0x00 payload byte — categorically distinct
 * from a len==0 region marker.  Every ITM_FORTH capture emit goes through
 * here: emitting len==0 instead would alias a marker and flip the parity
 * of every marker after the cursor (the very hazard E3 names). */
static uint16_t _forthCapBuildStep(char *dst, const char *text) {
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

void pemAlpha(int16_t item) {
  bool_t editCommand = false;
  if(item == ITM_EDIT) {
    int16_t aimFunc = currentStep[0];

    if(aimFunc & 0x80) {
      aimFunc &= 0x7f;
      aimFunc <<= 8;
      aimFunc |= currentStep[1];
    }

    tam.function = aimFunc;
    decodeOneStep(currentStep);
    uint16_t ll = stringByteLength(tmpString);
    if(aimFunc == ITM_LITERAL)  { // literal
      xcopy(aimBuffer, tmpString + 2, ll);        //purposely overshoot aimbuffer, as there is sufficient space
      aimBuffer[ll - 2 - 2] = 0;
      T_cursorPos = stringLastGlyph(aimBuffer) + 1;
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
      editCommand = true;
      item = 0;
    }
    else if(aimFunc == ITM_REM)   { // REM
      xcopy(aimBuffer, tmpString + 6, ll);        //purposely overshoot aimbuffer, as there is sufficient space
      aimBuffer[ll - 2 - 6] = 0;
      T_cursorPos = stringLastGlyph(aimBuffer) + 1;
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
      tam.function = aimFunc;
      editCommand = true;
      item = aimFunc;
    }
    else if(aimFunc == ITM_FORTH) { // FORTH
      if(currentStep[3] == 0) {
        aimBuffer[0] = 0;
        return;
      }
      forthCapOpen();                    // cannot fail: nothing is allocated
      xcopy(aimBuffer, tmpString, ll);   // bare render: no name prefix, no quotes
      aimBuffer[ll] = 0;
      /* §8.1: a leaked placeholder decodes to "" — cursor 0, not
       * stringLastGlyph("")+1 == 1, which would insert every glyph behind
       * the terminating NUL and silently eat the keystrokes.  EDIT is the
       * sanctioned recovery gesture for a restore-leaked capture step. */
      T_cursorPos = (ll == 0) ? 0 : stringLastGlyph(aimBuffer) + 1;
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
      tam.function = aimFunc;
      editCommand = true;
      item = aimFunc;
    }
    else {
      aimBuffer[0] = 0;
      return;
    }
  }

  if(!getSystemFlag(FLAG_ALPHA)) {
      if(tam.function == ITM_FORTH && !forthCapIsOpen()) {
        forthCapOpen();
      }
      resetShiftState();
      displayAIMbufferoffset = 0;
      if(!editCommand) {
        T_cursorPos = 0;
        aimBuffer[0] = 0;
      }


      //if(softmenuStack[0].softmenuId == 0) { // MyMenu
      //  softmenuStack[0].softmenuId = 1; // MyAlpha
      //}
      showSoftmenu(-MNU_ALPHA);
      setSystemFlag(FLAG_ALPHA);
      calcModeAimGui();

      if(tam.function < 128) { // literal
        tmpString[0] = tam.function;
        tmpString[1] = (char)STRING_LABEL_VARIABLE;
        tmpString[2] = 0;
        _insertInProgram((uint8_t *)tmpString, 3);
      }
      else if(tam.function == ITM_FORTH) { // forth: §8.1 placeholder, never a marker-aliased len==0
        _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, ""));
      }
      else { // rem or 42str
        tmpString[0] = (tam.function >> 8) | 0x80;
        tmpString[1] =  tam.function       & 0xff;
        tmpString[2] = (char)STRING_LABEL_VARIABLE;
        tmpString[3] = 0;
        _insertInProgram((uint8_t *)tmpString, 4);
      }
      --currentLocalStepNumber;
      currentStep = findPreviousStep(currentStep);
    }
    if(indexOfItems[item].func == addItemToBuffer) {
      int32_t len = stringByteLength(aimBuffer);
      if(forthCapIsOpen() && item == ITM_EXPONENT) {
        /* K2/E12.3: EEX must produce the number grammar's exponent
         * spelling, not the three letters "EEX" (its softmenu name).
         * One byte, inserted under the same cap and cursor advance as any
         * other character, and NOT via forthCapInsertName — no trailing
         * space, because "1e5" has to stay a single token. */
        if(len < (256 - 1) && stringGlyphLength(aimBuffer) < 196) {
          xcopy(aimBuffer + T_cursorPos + 1, aimBuffer + T_cursorPos, stringByteLength(aimBuffer + T_cursorPos) + 1);
          aimBuffer[T_cursorPos] = 'e';
          T_cursorPos += 1;
        }
      }
      else {
      /* K2/E12.3: keys-mode items are normal-column ids; the numlock
       * translation table is aim-column keyed and must not touch them. */
      if(!(forthCapIsOpen() && forthCapKeysMode())) {
      item = numlockReplacements(0, item, getSystemFlag(FLAG_NUMLOCK), shiftF, shiftG);
      }
      if(alphaCase == AC_LOWER) {
          if(ITM_A <= item && item <= ITM_Z) {
            item += (ITM_a - ITM_A);
          }
      }

      if((nextChar == NC_NORMAL) || ((item != ITM_DOWN_ARROW) && (item != ITM_UP_ARROW))) {
        item = convertItemToSubOrSup(item, nextChar);
        int32_t inputCharLength = stringByteLength(indexOfItems[item].itemSoftmenuName);
        if(len < (256 - inputCharLength) && stringGlyphLength(aimBuffer) < 196) {
          xcopy(aimBuffer + T_cursorPos + inputCharLength, aimBuffer + T_cursorPos, stringByteLength(aimBuffer + T_cursorPos) + 1);
          xcopy(aimBuffer + T_cursorPos, indexOfItems[item].itemSoftmenuName, inputCharLength);
          T_cursorPos += inputCharLength;
        }
      }
      }
    }
    else if(item == ITM_BACKSPACE) {
      if(aimBuffer[0] == 0) {
        deleteStepsFromTo(currentStep, findNextStep(currentStep));
        clearSystemFlag(FLAG_ALPHA);
        calcModeNormalGui();
        _closeAlphaMenus();
        // Capture-abort reset: tam.function is set by the Forth capture open
        // paths (the `func == ITM_AIM` and `func == ITM_FORTH` arms in
        // `insertStepInProgram`) and never reset by upstream on
        // this backspace-abort exit. Idle value 0 matches the global `tam`'s
        // zero-initialized boot state [VERIFIED: src/c47/c47.c:190 — no
        // initializer, static storage] and the documented invariant that
        // tam.mode, not tam.function, is the "in TAM" gate [VERIFIED:
        // src/c47/typeDefinitions.h:672-680].
        forthCapClose();
        tam.function = 0;
        return;
      }
      else if(T_cursorPos == 0) {
        return;
      }
      else {
        char cursorByte = aimBuffer[T_cursorPos];
        int16_t lastGlyphPos;
        aimBuffer[T_cursorPos] = 0;
        lastGlyphPos = stringLastGlyph(aimBuffer);
        aimBuffer[T_cursorPos] = cursorByte;
        xcopy(aimBuffer + lastGlyphPos, aimBuffer + T_cursorPos, stringByteLength(aimBuffer + T_cursorPos) + 1);
        T_cursorPos = lastGlyphPos;
      }
    }
    else if(item == ITM_ENTER) {
      bool_t wasForth = (tam.function == ITM_FORTH);
      bool_t hadText  = (aimBuffer[0] != 0);   // E5 locks on a NON-EMPTY line
      /* E9 tier 1: commit refused atomically — the capture stays open,
       * aimBuffer intact for correction, and the error is already displayed.
       * Tier 2 (names) never reaches here: forthCheckSourceLine accepts
       * them. */
      if(wasForth && hadText && !forthCheckSourceLine(aimBuffer)) {
        return;
      }
      /* forth-core: an empty ENTER is the escape hatch — E3 deletes the
       * placeholder and leaves the region open behind it. */
      pemCloseAlphaInput();
      //--firstDisplayedLocalStepNumber;
      defineFirstDisplayedStep();
        _closeAlphaMenus();
      /* forth-core: the multi-line lock.  The state is DERIVED from the
       * program bytes at the cursor — never stored.  ENTER just drops to the
       * next Forth line. */
      if(wasForth && hadText && forthEntryStateAtInsertion()) {
        tam.function = ITM_FORTH;
        pemAlpha(0);
      }
      return;
    }
    else if(item == ITM_USERMODE) {
      fnFlipFlag(FLAG_USER);
      return;
    }
    else if(item == ITM_CLA) { // JM addon
      aimBuffer[0] = 0;
      T_cursorPos = 0;
      nextChar = NC_NORMAL;
    }
    else if(item == CHR_numL && !getSystemFlag(FLAG_NUMLOCK)) { // JM addon
      alphaCase = AC_UPPER;
      SetSetting(indexOfItems[CHR_num].param);
      return;
    }
    else if(item == CHR_numU && getSystemFlag(FLAG_NUMLOCK)) { // JM addon
      alphaCase = AC_UPPER;
      SetSetting(indexOfItems[CHR_num].param);
      return;
    }
    else if(item == CHR_caseUP && alphaCase != AC_UPPER) { // JM addon
      nextChar = NC_NORMAL;
      SetSetting(indexOfItems[CHR_case].param);
      return;
    }
    else if(item == CHR_caseDN && alphaCase != AC_LOWER) { // JM addon
      nextChar = NC_NORMAL;
      SetSetting(indexOfItems[CHR_case].param);
      return;
    }
    else if(item == CHR_num) { // JM addon
      alphaCase = AC_UPPER;
      SetSetting(indexOfItems[item].param);
      return;
    }
    else if(item == CHR_case) { // JM addon
      nextChar = NC_NORMAL;
      clearSystemFlag(FLAG_NUMLOCK);
      SetSetting(indexOfItems[item].param);
      return;
    }
    else if(item == ITM_SCR) { // JM addon
      SetSetting(indexOfItems[item].param);
      return;
    }
    else if(indexOfItems[item].func == fnT_ARROW) { // JM addon
      fnT_ARROW(indexOfItems[item].param);
      return;
    }
    else if(forthCapIsOpen()
            && (indexOfItems[item].status & CAT_STATUS) == CAT_FNCT
            && (indexOfItems[item].status & PTP_STATUS) == PTP_NONE
            && item != ITM_AIM && item != ITM_FORTH) {
      (void)forthCapInsertName(indexOfItems[item].itemCatalogName);
      /* falls through to the re-commit tail: the step tracks the insert */
    }

    int16_t aimFunc = currentStep[0];
    if(aimFunc & 0x80) {
      aimFunc &= 0x7f;
      aimFunc <<= 8;
      aimFunc |= currentStep[1];
    }

    deleteStepsFromTo(currentStep, findNextStep(currentStep));
    if(aimFunc < 128) { // literal
      tmpString[0] = aimFunc;
      tmpString[1] = (char)STRING_LABEL_VARIABLE;
      tmpString[2] = stringByteLength(aimBuffer);
      xcopy(tmpString + 3, aimBuffer, stringByteLength(aimBuffer));
      _insertInProgram((uint8_t *)tmpString, stringByteLength(aimBuffer) + 3);
    }
    else if(aimFunc == ITM_FORTH) { // forth: backspace-to-empty re-emits the §8.1 placeholder, never len==0
      _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, aimBuffer));
    }
    else { // rem or 42str
      tmpString[0] = (aimFunc >> 8) | 0x80;
      tmpString[1] =  aimFunc       & 0xff;
      tmpString[2] = (char)STRING_LABEL_VARIABLE;
      tmpString[3] = stringByteLength(aimBuffer);
      xcopy(tmpString + 4, aimBuffer, stringByteLength(aimBuffer));
      _insertInProgram((uint8_t *)tmpString, stringByteLength(aimBuffer) + 4);
    }
    --currentLocalStepNumber;
    currentStep = findPreviousStep(currentStep);
    if(!programListEnd) {
      scrollPemBackwards();
    }
}

void pemCloseAlphaInput(void) {
  if(tam.function == ITM_FORTH && !forthCapTextNonEmpty()) {
    deleteStepsFromTo(currentStep, findNextStep(currentStep));
    clearSystemFlag(FLAG_ALPHA);
    calcModeNormalGui();
    _closeAlphaMenus();
    // Capture-close reset: see the identical rationale/citations at the
    // `ITM_BACKSPACE` empty-buffer arm above.
    forthCapClose();
    tam.function = 0;
    return;
  }
  aimBuffer[0] = 0;
  forthCapClose();
  if(tam.function == ITM_FORTH) {
    tam.function = 0;
  }
  clearSystemFlag(FLAG_ALPHA);
  calcModeNormalGui();
  ++currentLocalStepNumber;
  currentStep = findNextStep(currentStep);
  if((getNumberOfSteps() - currentLocalStepNumber) > 4) {
    ++firstDisplayedLocalStepNumber;
    firstDisplayedStep = findNextStep(firstDisplayedStep);
  }
  _closeAlphaMenus();
  // Capture-close reset: see the identical rationale/citations at the
  // `ITM_BACKSPACE` empty-buffer arm above. This branch commits
  // REM/LITERAL/non-empty-FORTH source lines alike, so the reset must be
  // unconditional here too, not just gated on tam.function == ITM_FORTH.
  tam.function = 0;
}

/* FIX-9: the E1 catalog-drain helpers are defined further down (above
 * insertStepInProgram); forthCaptureResume needs them too. */
static bool_t _forthCatalogBuriedOnStack(void);
static bool_t _forthCatalogMenuOnTop(void);

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
  _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, aimBuffer));
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
  _closeAlphaMenus();
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
                 && (_forthCatalogMenuOnTop() || _forthCatalogBuriedOnStack());
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
  _insertInProgram((uint8_t *)tmpString, 3 + FORTH_HISTORY_NAME_LEN);

  tmpString[0] = (char)((ITM_END >> 8) | 0x80);
  tmpString[1] = (char)(ITM_END & 0xff);
  _insertInProgram((uint8_t *)tmpString, 2);

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
  _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, text));
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
  _insertInProgram((uint8_t *)tmpString, _forthCapBuildStep(tmpString, aimBuffer));
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
static void _forthFoldNoteProgramDeleted(uint16_t deletedProgramNumber) {
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


void pemAlphaEdit (uint16_t unusedButMandatoryParameter) {
  if(getSystemFlag(FLAG_ALPHA) || calcMode != CM_PEM || tam.mode) {
    hourGlassIconEnabled = false;
    return;
  }
  int16_t func = currentStep[0];
  //printf("DDDDDa func[0] [1] = %i %i\n",func, currentStep[1]);
  if(func & 0x80) {
    func &= 0x7f;
    func <<= 8;
    func |= currentStep[1];
  }
  if((func == ITM_LITERAL || func == ITM_REM || func == ITM_FORTH)) {
    pemAlpha(ITM_EDIT);
  }
  hourGlassIconEnabled = false;
}


void pemAddNumber(int16_t item, bool doInsertInProgram) {
  if(aimBuffer[0] == 0) {
    lastIntegerBase = 0;
    editingLiteralType = 0;
    tmpString[0] = ITM_LITERAL;
    tmpString[1] = STRING_LONG_INTEGER;
    tmpString[2] = 0;
    _insertInProgram((uint8_t *)tmpString, 3);
    memset(nimBufferDisplay, 0, NIM_BUFFER_LENGTH);
    --currentLocalStepNumber;
    currentStep = findPreviousStep(currentStep);
    switch(item) {
      case ITM_EXPONENT : {
        aimBuffer[0] = '+';
        aimBuffer[1] = '1';
        aimBuffer[2] = '.';
        aimBuffer[3] = 0;
        nimNumberPart = NP_REAL_FLOAT_PART;
        break;
      }

      case ITM_PERIOD : {
        aimBuffer[0] = '+';
        aimBuffer[1] = '0';
        aimBuffer[2] = 0;
        nimNumberPart = NP_INT_10;
        break;
      }

      case ITM_0 :
      case ITM_1 :
      case ITM_2 :
      case ITM_3 :
      case ITM_4 :
      case ITM_5 :
      case ITM_6 :
      case ITM_7 :
      case ITM_8 :
      case ITM_9 :
      case ITM_A :
      case ITM_B :
      case ITM_C :
      case ITM_D :
      case ITM_E :
      case ITM_F : {
        aimBuffer[0] = '+';
        aimBuffer[1] = 0;
        nimNumberPart = NP_EMPTY;
        break;
      }
    }
  }

  if(item == ITM_BACKSPACE && ((aimBuffer[0] == '+' && aimBuffer[1] != 0 && aimBuffer[2] == 0) || aimBuffer[1] == 0)) {
    aimBuffer[0] = 0;
  }
  else {
    addItemToNimBuffer(item);
    if(stringByteLength(aimBuffer) > 255) {
      addItemToNimBuffer(ITM_BACKSPACE);
    }
  }
  clearSystemFlag(FLAG_ALPHA);

  if(aimBuffer[0] != '!') {
    if(doInsertInProgram) {
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
    }
    if(aimBuffer[0] != 0) {
      char *tmpPtr = tmpString;
      char offset = 3;
      const char *numBuffer = aimBuffer[0] == '+' ? aimBuffer + 1 : aimBuffer;
      *tmpPtr++ = ITM_LITERAL;
      switch(nimNumberPart) {
        case NP_INT_10:
        case NP_INT_16: {
        //case NP_INT_BASE: {
          if(lastIntegerBase != 0) {
            *tmpPtr++ = STRING_SHORT_INTEGER;
            *tmpPtr++ = lastIntegerBase;
            offset++;
          }
          else {
            *tmpPtr++ = STRING_LONG_INTEGER;
          }
          break;
        }
        case NP_REAL_FLOAT_PART:
        case NP_REAL_EXPONENT:
        case NP_HP32SII_DENOMINATOR:
        case NP_FRACTION_DENOMINATOR: {
          *tmpPtr++ = STRING_REAL34;
          break;
        }
        case NP_COMPLEX_INT_PART:
        case NP_COMPLEX_FLOAT_PART:
        case NP_COMPLEX_EXPONENT: {
          *tmpPtr++ = STRING_COMPLEX34;
          break;
        }
        default: {
          *tmpPtr++ = STRING_LONG_INTEGER;
          break;
        }
      }
      if(_isAngleType(editingLiteralType)) {
        *(tmpPtr - 1) = editingLiteralType;  // [DL] force literal type when editing angles
      }
      *tmpPtr++ = stringByteLength(numBuffer);
      xcopy(tmpPtr, numBuffer, stringByteLength(numBuffer));
      if(doInsertInProgram) {
        _insertInProgram((uint8_t *)tmpString, stringByteLength(numBuffer) + offset);
        --currentLocalStepNumber;
        currentStep = findPreviousStep(currentStep);
        if(!programListEnd) {
          scrollPemBackwards();
        }
      }
    }
    calcMode = CM_PEM;
  }
  else {
    aimBuffer[0] = 0;
    nimNumberPart = NP_EMPTY;
  }
}

void pemCloseNumberInput(void) {
  if(editingLiteralType > 0) {     // For EDIT: close number input with the right data or angle type
    switch(editingLiteralType) {
      case STRING_DATE         : _pemCloseDateInput();            break;
      case STRING_TIME         : _pemCloseTimeInput();            break;
      case STRING_ANGLE_RADIAN : _pemCloseAngleInput(ITM_RAD2);   break;
      case STRING_ANGLE_GRAD   : _pemCloseAngleInput(ITM_GRAD2);  break;
      case STRING_ANGLE_DEGREE : _pemCloseAngleInput(ITM_DEG2);   break;
      case STRING_ANGLE_DMS    : _pemCloseAngleInput(ITM_DMS2);   break;
      case STRING_ANGLE_MULTPI : _pemCloseAngleInput(ITM_MULPI2); break;
      default:     ;
    }
    return;
  }
  deleteStepsFromTo(currentStep, findNextStep(currentStep));
  if(aimBuffer[0] != 0) {
    char *numBuffer = aimBuffer[0] == '+' ? aimBuffer + 1 : aimBuffer;
    char *tmpPtr = tmpString;
    uint32_t inputLength = stringByteLength(numBuffer);
    bool_t doneWithBinaryLiteral = false;
    int16_t lastChar = strlen(aimBuffer) - 1;

    if((lastIntegerBase != 0) && (nimNumberPart == NP_INT_10 || nimNumberPart == NP_INT_16)) {
      sprintf(aimBuffer + strlen(aimBuffer), "#%" PRIu16, (int) lastIntegerBase);
      nimNumberPart = NP_INT_BASE;
    }

    *(tmpPtr++) = ITM_LITERAL;
    if((nimNumberPart == NP_COMPLEX_EXPONENT || nimNumberPart == NP_REAL_EXPONENT) && (aimBuffer[lastChar] == '+' || aimBuffer[lastChar] == '-') && aimBuffer[lastChar - 1] == 'e') {
      aimBuffer[--lastChar] = 0;
      lastChar--;
    }
    else if(nimNumberPart == NP_REAL_EXPONENT && aimBuffer[lastChar] == 'e') {
      aimBuffer[lastChar--] = 0;
    }
    switch(nimNumberPart) {
      //case NP_INT_16:
      case NP_INT_BASE: {
        char *basePtr = numBuffer;
        while(*basePtr != '#') {
          ++basePtr;
        }
        *(basePtr++) = 0;
        inputLength = stringByteLength(numBuffer); // we must update here since the content of numBuffer has been truncated
        if(inputLength >= sizeof(uint64_t) && numBuffer[0] != '-') {
          uint8_t base = (uint8_t)atoi(basePtr);
          uint64_t val = 0;
          *(tmpPtr++) = BINARY_SHORT_INTEGER;
          *(tmpPtr++) = (char)atoi(basePtr);
          for(unsigned int i = 0; i < inputLength; ++i) {
            if('0' <= numBuffer[i] && numBuffer[i] <= '9') {
              val *= base;
              val += numBuffer[i] - '0';
            }
            else if('A' <= numBuffer[i] && numBuffer[i] <= 'F') {
              val *= base;
              val += numBuffer[i] - 'A' + 10;
            }
            else if('a' <= numBuffer[i] && numBuffer[i] <= 'f') {
              val *= base;
              val += numBuffer[i] - 'a' + 10;
            }
          }
          for(unsigned int i = 0; i < sizeof(uint64_t); ++i) {
            *(tmpPtr++) = ((uint8_t *)(&val))[i];
          }
          _insertInProgram((uint8_t *)tmpString, (int32_t)(tmpPtr - tmpString));
          doneWithBinaryLiteral = true;
        }
        else {
          *(tmpPtr++) = STRING_SHORT_INTEGER;
          *(tmpPtr++) = (char)atoi(basePtr);
        }
        break;
      }
      case NP_REAL_FLOAT_PART:
      case NP_REAL_EXPONENT:
      case NP_HP32SII_DENOMINATOR:
      case NP_FRACTION_DENOMINATOR: {
        if(inputLength >= REAL34_SIZE_IN_BYTES) {
          real34_t val;
          *(tmpPtr++) = BINARY_REAL34;
          stringToReal34(numBuffer, &val);
          for(unsigned int i = 0; i < REAL34_SIZE_IN_BYTES; ++i) {
            *(tmpPtr++) = ((uint8_t *)(&val))[i];
          }
          _insertInProgram((uint8_t *)tmpString, (int32_t)(tmpPtr - tmpString));
          doneWithBinaryLiteral = true;
        }
        else {
          *(tmpPtr++) = STRING_REAL34;
        }
        break;
      }
      case NP_COMPLEX_INT_PART:
      case NP_COMPLEX_FLOAT_PART:
      case NP_COMPLEX_EXPONENT: {
        if(aimBuffer[stringByteLength(aimBuffer)-1] == 'i') {
          strcat(aimBuffer, "1");
          inputLength++;
        }

        if(inputLength >= TO_BYTES(COMPLEX34_SIZE_IN_BLOCKS)) {
          real34_t re, im;
          char *imag = numBuffer;
          while(*imag != 'i' && *imag != 0) {
            ++imag;
          }
          if(*imag == 'i') {
            if(imag > numBuffer && *(imag - 1) == '-') {
              *imag = '-';
              *(imag - 1) = 0;
            }
            else if(imag > numBuffer && *(imag - 1) == '+') {
              *imag = 0;
              *(imag - 1) = 0;
              ++imag;
            }
            else {
              *imag = 0;
            }
          }

          *(tmpPtr++) = BINARY_COMPLEX34;
          stringToReal34(numBuffer, &re);
          stringToReal34(imag, &im);
          for(unsigned int i = 0; i < REAL34_SIZE_IN_BYTES; ++i) {
            *(tmpPtr++) = ((uint8_t *)(&re))[i];
          }
          for(unsigned int i = 0; i < REAL34_SIZE_IN_BYTES; ++i) {
            *(tmpPtr++) = ((uint8_t *)(&im))[i];
          }
          _insertInProgram((uint8_t *)tmpString, (int32_t)(tmpPtr - tmpString));
          doneWithBinaryLiteral = true;
        }
        else {
          *(tmpPtr++) = STRING_COMPLEX34;
        }
        break;
      }
      default: {
        *(tmpPtr++) = STRING_LONG_INTEGER;
        break;
      }
    }
    if(!doneWithBinaryLiteral) {
      *(tmpPtr++) = inputLength;
      xcopy(tmpPtr, numBuffer, inputLength);
      _insertInProgram((uint8_t *)tmpString, inputLength + (int32_t)(tmpPtr - tmpString));
      pemCursorIsZerothStep = false;
    }
  }

  aimBuffer[0] = '!';
  nimNumberPart = NP_EMPTY;
  lastIntegerBase = 0;
}

static void _pemCloseTimeInput(void) {
  switch(nimNumberPart) {
    case NP_INT_10:
    case NP_REAL_FLOAT_PART: {
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
      if(aimBuffer[0] != 0) {
        char *numBuffer = aimBuffer[0] == '+' ? aimBuffer + 1 : aimBuffer;
        char *tmpPtr = tmpString;
        *(tmpPtr++) = ITM_LITERAL;
        *(tmpPtr++) = STRING_TIME;
        *(tmpPtr++) = stringByteLength(numBuffer);
        xcopy(tmpPtr, numBuffer, stringByteLength(numBuffer));
        _insertInProgram((uint8_t *)tmpString, stringByteLength(numBuffer) + (int32_t)(tmpPtr - tmpString));
      }

      aimBuffer[0] = '!';
      break;
    }
  }
}

static void _pemCloseDateInput(void) {
  if(nimNumberPart == NP_REAL_FLOAT_PART) {
    deleteStepsFromTo(currentStep, findNextStep(currentStep));
    if(aimBuffer[0] != 0) {
      char *numBuffer = aimBuffer[0] == '+' ? aimBuffer + 1 : aimBuffer;
      char *tmpPtr = tmpString;
      *(tmpPtr++) = ITM_LITERAL;
      *(tmpPtr++) = STRING_DATE;

      reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
      stringToReal34(numBuffer, REGISTER_REAL34_DATA(TEMP_REGISTER_1));
      convertReal34RegisterToDateRegister(TEMP_REGISTER_1, TEMP_REGISTER_1, false);  //no !YYsystem needed here
      internalDateToJulianDay(REGISTER_REAL34_DATA(TEMP_REGISTER_1), REGISTER_REAL34_DATA(TEMP_REGISTER_1));

      real34ToString(REGISTER_REAL34_DATA(TEMP_REGISTER_1), tmpPtr + 1);
      *tmpPtr = stringByteLength(tmpPtr + 1);
      ++tmpPtr;

      _insertInProgram((uint8_t *)tmpString, stringByteLength(tmpPtr) + (int32_t)(tmpPtr - tmpString));
    }

    aimBuffer[0] = '!';
  }
}

static void _pemCloseAngleInput(int item) {
  switch(nimNumberPart) {
    case NP_INT_10:
    case NP_REAL_FLOAT_PART: {
      deleteStepsFromTo(currentStep, findNextStep(currentStep));
      if(aimBuffer[0] != 0) {
        char *numBuffer = aimBuffer[0] == '+' ? aimBuffer + 1 : aimBuffer;
        char *tmpPtr = tmpString;
        *(tmpPtr++) = ITM_LITERAL;
        static const int angle_ids[] = {
            [ITM_DEG2]   = STRING_ANGLE_DEGREE,
            [ITM_DMS2]   = STRING_ANGLE_DMS,
            [ITM_GRAD2]  = STRING_ANGLE_GRAD,
            [ITM_MULPI2] = STRING_ANGLE_MULTPI,
            [ITM_RAD2]   = STRING_ANGLE_RADIAN
        };
        int id = -1;
        if(item >= 0 && item < (int)(sizeof(angle_ids)/sizeof(angle_ids[0]))) {
            id = angle_ids[item];
        }
        if(id != -1) {
            *(tmpPtr++) = id;
        }
        *(tmpPtr++) = stringByteLength(numBuffer);
        xcopy(tmpPtr, numBuffer, stringByteLength(numBuffer));
        _insertInProgram((uint8_t *)tmpString, stringByteLength(numBuffer) + (int32_t)(tmpPtr - tmpString));
      }
      editingLiteralType = 0;
      aimBuffer[0] = '!';
      break;
    }
  }
}

/* forth-core: helpers for the ITM_FORTH catalog drain.
 *
 * _closeCatalog() (keyboard.c) runs immediately after runFunction() returns and
 * decides "are we in the catalog" by scanning the ENTIRE softmenu stack for
 * MNU_CATALOG; if it finds one it pops the current menu when that menu is
 * listed in CatalogMenus[] — and MNU_ALPHA is on that list. So any MNU_CATALOG
 * surviving anywhere beneath us costs us the alpha menu pemAlpha() is about to
 * push, even if the top of the stack is already clean. The drain must therefore
 * use the same stack-wide predicate _closeCatalog() uses, not a top-of-stack
 * test. Reachable case: a non-catalog menu opened over a catalog (which leaves
 * `catalog` set, since enterAsmModeIfMenuIsACatalog() only ever sets it) with
 * ITM_FORTH pressed from that menu. */
static bool_t _forthCatalogBuriedOnStack(void) {
  for(int i = 0; i < SOFTMENU_STACK_SIZE; i++) {
    if(softmenu[softmenuStack[i].softmenuId].menuItem == -MNU_CATALOG) {
      return true;
    }
  }
  return false;
}

/* Tidy teardown of the catalog menus themselves, kept from the original drain:
 * without it a catalog submenu reached directly (no MNU_CATALOG beneath it)
 * would stay buried under the alpha menu. */
static bool_t _forthCatalogMenuOnTop(void) {
  const int16_t m = currentMenu();
  return m == -MNU_CATALOG || m == -MNU_FCNS  || m == -MNU_CONST
      || m == -MNU_CHARS   || m == -MNU_PROGS || m == -MNU_VARS
      || m == -MNU_MENUS;
}

/* L1-1 (C2b): public wrappers — fnForthOuter's interactive catalog drain
 * (forth_compile.c) needs the same predicates insertStepInProgram's PEM
 * drain uses below, but the helpers themselves are file-static here. */
bool_t forthCatalogMenuOnTop(void)     { return _forthCatalogMenuOnTop(); }
bool_t forthCatalogBuriedOnStack(void) { return _forthCatalogBuriedOnStack(); }

void insertStepInProgram(const int16_t func) {
                                #if defined(DEBUG_PGM)
                                  print_caller(NULL);
                                #endif
  uint32_t opBytes = (func >= 128) ? 2 : 1;

  if(func == ITM_END) {
    firstDisplayedLocalStepNumber = 0;
  }

  if(func == ITM_AIM || (!tam.mode && getSystemFlag(FLAG_ALPHA) && func != ITM_FORTH)) {
    if(func == ITM_AIM && forthCapIsOpen()) {
      /* K1/E10-E11: the ALPHA gesture toggles alpha<->keys while a capture
       * line is open.  Gated on forthCapIsOpen() so E6 (ITM_AIM re-entry
       * with the capture CLOSED) is untouched.  K-R3: keys mode shows the
       * underlying menus — the visible row swap IS the mode indicator. */
      if(forthCapKeysMode()) {
        forthCapSetKeysMode(false);
        showSoftmenu(-MNU_ALPHA);
      }
      else {
        forthCapSetKeysMode(true);
        _closeAlphaMenus();
      }
      pemCursorIsZerothStep = false;
      return;
    }
    if(aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
      pemCloseNumberInput();
      aimBuffer[0] = 0;
    }
    /* forth-core: ALPHA inside a Forth region resumes the Forth capture, not
     * a string literal, and an already-open capture is preserved.  Upstream
     * forced ITM_LITERAL unconditionally, which killed the empty-commit rule,
     * the FWRD picker guard and the toggle-off gesture. */
    if(func == ITM_AIM && forthEntryStateAtInsertion()) {
      tam.function = ITM_FORTH;
    }
    else if(tam.function != ITM_FORTH) {
    tam.function = ITM_LITERAL;
    }
    pemAlpha(func);
    pemCursorIsZerothStep = false;
    return;
  }
  else if(func == ITM_REM) {
    if(aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
      pemCloseNumberInput();
      aimBuffer[0] = 0;
    }
    if(catalog) {      // If called from a catalog such as FNCS, exit catalog and Asm Mode
      leaveAsmMode();
      popSoftmenu();
      if(currentMenu() == -MNU_CATALOG) {   // drop the CAT menu too
        popSoftmenu();
      }
    }
    tam.function = func;
    pemAlpha(func);
    pemCursorIsZerothStep = false;
    return;
  }
  else if(func == ITM_FORTH) {
    if(aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
      pemCloseNumberInput(); aimBuffer[0] = 0;
    }
    if(forthCapIsOpen()) {
      // FIX-8 (D-C2): FORTH picked while a capture line is OPEN — catalog
      // today, keys mode later. Commit-and-close the line through the same
      // path EXIT-with-text uses, so the toggle below proceeds from the
      // closed-capture state every other tested entry reaches. The cursor is
      // still ON the capture step here: addStepInProgram's pre-move is gated
      // on FLAG_ALPHA being clear, so it did not run — exactly the state
      // pemCloseAlphaInput's cursor math expects. Without this, the close arm
      // left forthCap.state == FCAP_OPEN with the alpha UI torn down: the
      // next tamEnterMode suspend seam destructively recommitted stale
      // state, and fnKeyExit's forthCapTextNonEmpty() misrouted the EXIT
      // ladder's currentStep resync.
      pemCloseAlphaInput();
    }
    if(catalog) {   // forth-core: NOT the REM arm's single popSoftmenu() — see
      leaveAsmMode();                    // _forthCatalogBuriedOnStack() above.
      // Bounded: popSoftmenu() can re-push HOME, so never spin on it.
      for(int i = 0; i < SOFTMENU_STACK_SIZE
                     && (_forthCatalogMenuOnTop() || _forthCatalogBuriedOnStack());
          i++) {
        popSoftmenu();
      }
    }
    bool_t wasOn = forthEntryStateAtInsertion();
    tmpString[0] = (ITM_FORTH >> 8) | 0x80;
    tmpString[1] =  ITM_FORTH       & 0xff;
    tmpString[2] = (char)STRING_LABEL_VARIABLE;
    tmpString[3] = 0;
    _insertInProgram((uint8_t *)tmpString, 4);
    if(!wasOn) {
      /* A new Forth region is a complete bracket from its first line onward.
       * Insert the closing marker now, then step back onto it so pemAlpha()
       * places the editable placeholder immediately before that marker.
       * ENTER commits the current line and uses the same insertion point to
       * add an explicit next line inside the already-balanced bracket. */
      _insertInProgram((uint8_t *)tmpString, 4);
      currentStep = findPreviousStep(currentStep);
      if(currentLocalStepNumber > 1) {
        --currentLocalStepNumber;
      }
      tam.function = ITM_FORTH;
      pemAlpha(ITM_FORTH);
    } else {
      clearSystemFlag(FLAG_ALPHA);
      // Capture-close reset: see the identical rationale/citations at the
      // `ITM_BACKSPACE` empty-buffer arm above. Without this, a stale
      // tam.function == ITM_FORTH survives a normal toggle-close and can
      // mislabel the NEXT, unrelated alpha capture: insertStepInProgram's
      // `func == ITM_AIM` arm only sets tam.function = ITM_LITERAL when
      // `tam.function != ITM_FORTH` — a plain literal entry opened while the
      // sentinel is stale skips that assignment and inherits ITM_FORTH,
      // silently misrouting cursor-offset math keyed on tam.function (R3-1).
      tam.function = 0;
    }
    pemCursorIsZerothStep = false;
    return;
  }
  else if(func == ITM_42STRING || func == ITM_42APPEND) {
    tmpString[0] = (func >> 8) | 0x80;
    tmpString[1] =  func  & 0xff;
    tmpString[2] = (char)STRING_LABEL_VARIABLE;
    tmpString[3] = stringByteLength(aimBuffer);
    xcopy(tmpString + 4, aimBuffer, stringByteLength(aimBuffer));
    _insertInProgram((uint8_t *)tmpString, stringByteLength(aimBuffer) + 4);
    aimBuffer[0] = 0;
    return;
  }

  if(!tam.mode && !getSystemFlag(FLAG_ALPHA) && aimBuffer[0] == 0
      && indexOfItems[func].func == addItemToBuffer
      && forthEntryStateAtInsertion()) {
    tam.function = ITM_FORTH;
    pemAlpha(func);
    pemCursorIsZerothStep = false;
    return;
  }

  if(   indexOfItems[func].func == addItemToBuffer
     || (!tam.mode && aimBuffer[0] != 0 && (   func == ITM_CHS || func == ITM_CC || func == ITM_op_j || func == ITM_op_j_pol || func == ITM_toINT
                                            || (nimNumberPart == NP_INT_BASE && (   ( isR47FAM && (func == ITM_SQUAREROOTX || func == ITM_YX))
                                                                                 || (!isR47FAM && (func == ITM_1ONX        || func == ITM_LOG10))
                                                                                 || func == ITM_RCL || func == ITM_EXPONENT || func == ITM_ENTER))))) {
    pemAddNumber(func, true);
    return;
  }
  else if(nimNumberPart == NP_INT_BASE) {
    return;
  }
  else if(func == ITM_CONSTpi && aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA) && nimNumberPart == NP_COMPLEX_INT_PART && aimBuffer[strlen(aimBuffer) - 1] == 'i') {
    strcat(aimBuffer, "3.141592653589793238462643383279503");
    pemCloseNumberInput();
    aimBuffer[0] = 0;
    return;
  }
  else if((func == ITM_DMS || func == ITM_DMS2 || func == ITM_DEG2 || func == ITM_GRAD2 || func == ITM_RAD2 || func == ITM_MULPI2) && aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA) && (nimNumberPart == NP_INT_10 || nimNumberPart == NP_REAL_FLOAT_PART)) {
    _pemCloseAngleInput(func);
    aimBuffer[0] = 0;
    return;
  }
  else if((func == ITM_dotD) && editingLiteralType != 0 && aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {  // cancel time/date/angle type and close number input
    editingLiteralType = 0;
    pemCloseNumberInput();
    aimBuffer[0] = '!';
  }
  else if((func == ITM_DRG) && aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA) && (nimNumberPart == NP_INT_10 || nimNumberPart == NP_REAL_FLOAT_PART)) {
    switch(currentAngularMode) {
      case amRadian : _pemCloseAngleInput(ITM_RAD2); break;
      case amGrad   : _pemCloseAngleInput(ITM_GRAD2); break;
      case amDegree : _pemCloseAngleInput(ITM_DEG2); break;
      case amDMS    : _pemCloseAngleInput(ITM_DMS2); break;
      case amMultPi : _pemCloseAngleInput(ITM_MULPI2); break;
      default: return;
    }
    aimBuffer[0] = 0;
    return;
  }

  if(!tam.mode && !tam.alpha && aimBuffer[0] != 0 && func != ITM_HMStoTM && func != ITM_EXIT1) {
    if((func == ITM_dotD) || ((editingLiteralType == STRING_DATE) && ( func != ITM_ms)))  {
      _pemCloseDateInput();
      if(aimBuffer[0] == '!') {
        aimBuffer[0] = 0;
        return;
      }
    }
    else if((func == ITM_ms) || (editingLiteralType == STRING_TIME)) {
      _pemCloseTimeInput();
      if(aimBuffer[0] == '!') {
        aimBuffer[0] = 0;
        return;
      }
    }
    else {
      switch(editingLiteralType) {   // For EDIT: close number input with the right angle type
        case STRING_ANGLE_RADIAN : _pemCloseAngleInput(ITM_RAD2); break;
        case STRING_ANGLE_GRAD   : _pemCloseAngleInput(ITM_GRAD2); break;
        case STRING_ANGLE_DEGREE : _pemCloseAngleInput(ITM_DEG2); break;
        case STRING_ANGLE_DMS    : _pemCloseAngleInput(ITM_DMS2); break;
        case STRING_ANGLE_MULTPI : _pemCloseAngleInput(ITM_MULPI2); break;
        default:     pemCloseNumberInput();
      }
      aimBuffer[0] = 0;
      if(func == ITM_ENTER) {  // just close number editing, don't insert ENTER
        return;
      }
    }
  }

  char buffer[16];
  xcopy(buffer, tmpString, 16);    // Save tmpString content for dynamic menus

  if(func < 128) {
    tmpString[0] = func;
  }
  else {
    tmpString[0] = (func >> 8) | 0x80;
    tmpString[1] =  func       & 0xff;
  }

   if(func == ITM_FCALL) {
     char fname[FORTH_NAME_MAX + 1];
      if(!tam.indirect && forthDictNameByRef(tam.value, fname, sizeof(fname))) {
       uint16_t nameLen = stringByteLength(fname);
       tmpString[0] = (ITM_FORTH >> 8) | 0x80;
       tmpString[1] =  ITM_FORTH       & 0xff;
       tmpString[2] = (char)STRING_LABEL_VARIABLE;
       tmpString[3] = (char)nameLen;
       xcopy(tmpString + 4, fname, nameLen);
       _insertInProgram((uint8_t *)tmpString, nameLen + 4);
     }
     else {
       displayCalcErrorMessage(ERROR_NON_PROGRAMMABLE_COMMAND, ERR_REGISTER_LINE, REGISTER_X);
     }
     return;
   }

   switch(indexOfItems[func].status & PTP_STATUS) {
    case PTP_DISABLED: {
      switch(func) {
        case ITM_KEYG:           // 1498
        case ITM_KEYX:           // 1499
        case ITM_42KEYG:         // 2795
        case ITM_42KEYX: {       // 2796
            int opLen;
            uint16_t keyFunc;
            if((func == ITM_42KEYG) || (func == ITM_42KEYX)) {
              keyFunc = ITM_42KEY;
            }
            else {
              keyFunc = ITM_KEY;
            }
            tmpString[0] = (char)((keyFunc >> 8) | 0x80);
            tmpString[1] = (char)( keyFunc       & 0xff);
            if(tam.keyAlpha) {
              uint16_t nameLength = stringByteLength(aimBuffer + AIM_BUFFER_LENGTH / 2);
              tmpString[2] = (char)INDIRECT_VARIABLE;
              tmpString[3] = (char)nameLength;
              xcopy(tmpString + 4, aimBuffer + AIM_BUFFER_LENGTH / 2, nameLength);
              opLen = nameLength + 4;
            }
            else if(tam.keyIndirect) {
              tmpString[2] = (char)INDIRECT_REGISTER;
              tmpString[3] = tam.key;
              opLen = 4;
            }
            else {
              tmpString[2] = tam.key;
              opLen = 3;
            }

            tmpString[opLen + 0] = (((func == ITM_KEYX) || (func == ITM_42KEYX)) ? ITM_XEQ : ITM_GTO);
            if(tam.alpha) {
              uint16_t nameLength = stringByteLength(aimBuffer);
              tmpString[opLen + 1] = (char)(tam.indirect ? INDIRECT_VARIABLE : tam.colon ? LOCAL_LABEL_VARIABLE : STRING_LABEL_VARIABLE);
              tmpString[opLen + 2] = nameLength;
              xcopy(tmpString + opLen + 3, aimBuffer, nameLength);
              _insertInProgram((uint8_t *)tmpString, nameLength + opLen + 3);
            }
            else if(tam.indirect) {
              tmpString[opLen + 1] = (char)INDIRECT_REGISTER;
              tmpString[opLen + 2] = tam.value;
              _insertInProgram((uint8_t *)tmpString, opLen + 3);
            }
            else {
              tmpString[opLen + 1] = tam.value;
              _insertInProgram((uint8_t *)tmpString, opLen + 2);
            }
          break;
        }

        case ITM_GTOP: {         // 1482
          #if !defined(DMCP_BUILD)
            stringToUtf8(indexOfItems[func].itemCatalogName, (uint8_t *)tmpString);
            printf("insertStepInProgram: %s\n", tmpString);
          #endif // DMCP_BUILD
          break;
        }

        //case ITM_DELP: {          // 1425
        //  fnClP(NOPARAM);
        //  break;
        //}

        case ITM_DELPALL: {       // 1426
          fnClPAll(NOT_CONFIRMED);
          break;
        }

        case ITM_BST: {          // 1734
          fnBst(NOPARAM);
          break;
        }

        case ITM_SST: {          // 1736
          fnSst(NOPARAM);
          break;
        }

        case VAR_ACC: {          // 1192
          tmpString[0] = ITM_STO;
          tmpString[1] = (char)STRING_LABEL_VARIABLE;
          tmpString[2] = 3;
          tmpString[3] = 'A';
          tmpString[4] = 'C';
          tmpString[5] = 'C';
          _insertInProgram((uint8_t *)tmpString, 6);
          break;
        }

        case VAR_UEST:           // 2545
        case VAR_LEST: {         // 2546
          tmpString[0] = ITM_STO;
          tmpString[1] = (char)STRING_LABEL_VARIABLE;
          tmpString[2] = 5;
          if(func == VAR_UEST) {
            tmpString[3] = STD_UP_ARROW[0];
            tmpString[4] = STD_UP_ARROW[1];
          }
          else {
            tmpString[3] = STD_DOWN_ARROW[0];
            tmpString[4] = STD_DOWN_ARROW[1];
          }
          tmpString[5] = 'E';
          tmpString[6] = 's';
          tmpString[7] = 't';
          _insertInProgram((uint8_t *)tmpString, 8);
          break;
        }

        case VAR_ULIM:           // 1193
        case VAR_LLIM: {         // 1194
          tmpString[0] = ITM_STO;
          tmpString[1] = (char)STRING_LABEL_VARIABLE;
          tmpString[2] = 5;
          if(func == VAR_ULIM) {
            tmpString[3] = STD_UP_ARROW[0];
            tmpString[4] = STD_UP_ARROW[1];
          }
          else {
            tmpString[3] = STD_DOWN_ARROW[0];
            tmpString[4] = STD_DOWN_ARROW[1];
          }
          tmpString[5] = 'L';
          tmpString[6] = 'i';
          tmpString[7] = 'm';
          _insertInProgram((uint8_t *)tmpString, 8);
          break;
      }

        case VAR_UY:             // 2547
        case VAR_LY:             // 2548
        case VAR_UX:             // 1205
        case VAR_LX: {           // 1206
          tmpString[0] = ITM_STO;
          tmpString[1] = (char)STRING_LABEL_VARIABLE;
          tmpString[2] = 3;
          if(func == VAR_UX || func == VAR_UY) {
            tmpString[3] = STD_UP_ARROW[0];
            tmpString[4] = STD_UP_ARROW[1];
          }
          else {
            tmpString[3] = STD_DOWN_ARROW[0];
            tmpString[4] = STD_DOWN_ARROW[1];
          }
          if(func == VAR_UX || func == VAR_LX) {
            tmpString[5] = 'X';
          }
          else {
            tmpString[5] = 'Y';
          }
          _insertInProgram((uint8_t *)tmpString, 6);
          break;
      }

        case ITM_USERMODE: {     // 1729
          fnFlipFlag(FLAG_USER);
          break;
        }
      }
      break;
    }

    case PTP_NONE: {
      if(func == ITM_HMStoTM  && aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
        _pemCloseTimeInput();
        if(aimBuffer[0] != '!') {
          pemCloseNumberInput();
          aimBuffer[0] = 0;
          tmpString[0] = (func >> 8) | 0x80;
          tmpString[1] =  func       & 0xff;
          _insertInProgram((uint8_t *)tmpString, 2);
        }
        aimBuffer[0] = 0;
      }
      else {
        _insertInProgram((uint8_t *)tmpString, 1 + (func >= 128));
      }
      break;
    }

    case PTP_NUMBER_16: {
      if(isFunctionOldParam16(func)) {  // original Param16 functions without indirection support (little endian parameter)
        tmpString[2] = (char)(tam.value & 0xff); // little endian
        tmpString[3] = (char)(tam.value >> 8);
        _insertInProgram((uint8_t *)tmpString, 4);
      }
      else {                        // new Param16 functions with indirection support (big endian parameter)
        if(tam.alpha && tam.indirect) {
          uint16_t nameLength = stringByteLength(aimBuffer);
          tmpString[opBytes    ] = (char)(INDIRECT_VARIABLE);
          tmpString[opBytes + 1] = nameLength;
          xcopy(tmpString + opBytes + 2, aimBuffer, nameLength);
          _insertInProgram((uint8_t *)tmpString, nameLength + opBytes + 2);
        }
        else if(tam.indirect) {
          tmpString[opBytes    ] = (char)INDIRECT_REGISTER;
          tmpString[opBytes + 1] = tam.value + (tam.dot ? FIRST_LOCAL_REGISTER : 0);
          _insertInProgram((uint8_t *)tmpString, opBytes + 2);
        }
        else {
        tmpString[2] = (char)(tam.value >> 8);   // BIG endian
        tmpString[3] = (char)(tam.value & 0xff);
        _insertInProgram((uint8_t *)tmpString, 4);
        }
      }
      break;
    }

    case PTP_LITERAL:
    case PTP_REM: {
      // nothing to do here
      break;
    }

    case PTP_SKIP_BACK: {
        tmpString[opBytes    ] = (tam.dot ? tam.value + FIRST_LOCAL_REGISTER_IN_KS_CODE : tam.value);
        _insertInProgram((uint8_t *)tmpString, opBytes + 1);
      break;
    }

    default: {
      uint32_t opBytes = 1 + (func >= 128);

      if(tam.mode == TM_VALUE && ((indexOfItems[func].status & PTP_STATUS) == PTP_NUMBER_8_16) && tam.value > 250) {
        tmpString[opBytes    ] = (char)CNST_BEYOND_250;
        tmpString[opBytes + 1] = tam.value - 250;
        _insertInProgram((uint8_t *)tmpString, opBytes + 2);
      }
      else if(tam.mode == TM_CMP && tam.value == TEMP_REGISTER_1) {
        tmpString[opBytes    ] = (char)(real34IsZero(REGISTER_REAL34_DATA(TEMP_REGISTER_1)) ? VALUE_0 : VALUE_1);
        _insertInProgram((uint8_t *)tmpString, opBytes + 1);
      }
      else if((tam.mode == TM_FLAGR || tam.mode == TM_FLAGW) && tam.alpha && !tam.indirect) {
        tmpString[opBytes    ] = (char)SYSTEM_FLAG_NUMBER;
        tmpString[opBytes + 1] = tam.value;
        _insertInProgram((uint8_t *)tmpString, opBytes + 2);
      }
      else if((tam.mode == TM_MENU) && !tam.alpha && !tam.indirect) {
        uint16_t nameLength;
        tmpString[opBytes    ] = (char)STRING_LABEL_VARIABLE;
        if(tam.value == MNU_DYNAMIC) {
          nameLength  = stringByteLength(buffer);
          tmpString[opBytes + 1] = nameLength;
          xcopy(tmpString + opBytes + 2, buffer, nameLength);
          _insertInProgram((uint8_t *)tmpString, nameLength + opBytes + 2);
        }
        else {
          nameLength  = stringByteLength(indexOfItems[tam.value].itemCatalogName);
          tmpString[opBytes + 1] = nameLength;
          xcopy(tmpString + opBytes + 2, indexOfItems[tam.value].itemCatalogName, nameLength);
          _insertInProgram((uint8_t *)tmpString, nameLength + opBytes + 2);
        }
      }
      else if(tam.alpha) {
        uint16_t nameLength = stringByteLength(aimBuffer);
        tmpString[opBytes    ] = (char)(tam.indirect ? INDIRECT_VARIABLE : tam.colon ? LOCAL_LABEL_VARIABLE : STRING_LABEL_VARIABLE);
        tmpString[opBytes + 1] = nameLength;
        xcopy(tmpString + opBytes + 2, aimBuffer, nameLength);
        _insertInProgram((uint8_t *)tmpString, nameLength + opBytes + 2);
      }
      else if(tam.indirect) {
        tmpString[opBytes    ] = (char)INDIRECT_REGISTER;
        tmpString[opBytes + 1] = (tam.dot ? tam.value + FIRST_LOCAL_REGISTER_IN_KS_CODE : regCtoKS(tam.value));
        _insertInProgram((uint8_t *)tmpString, opBytes + 2);
      }
      else {
        tmpString[opBytes    ] = (tam.dot ? tam.value + FIRST_LOCAL_REGISTER_IN_KS_CODE : (tam.mode == TM_LABEL || tam.mode == TM_LBLONLY) ? tam.value : regCtoKS(tam.value));
        _insertInProgram((uint8_t *)tmpString, opBytes + 1);
      }
  }
  }

  if(func != ITM_EDIT) {
    aimBuffer[0] = 0;
  }
}


void insertUserItemInProgram(int16_t func, char *funcParam) {
  uint32_t opBytes=0;
  uint16_t nameLength = stringByteLength(funcParam);

  if((!pemCursorIsZerothStep) && ((aimBuffer[0] == 0 && !getSystemFlag(FLAG_ALPHA)) || tam.mode) && !isAtEndOfProgram(currentStep) && !isAtEndOfPrograms(currentStep)) {
    currentStep = findNextStep(currentStep);
    ++currentLocalStepNumber;
  }
  if(func < 128) {
    tmpString[opBytes++] = func;
  }
  else {
    tmpString[opBytes++] = (func >> 8) | 0x80;
    tmpString[opBytes++] =  func       & 0xff;  // audit F4: 0x7f masked low bytes >= 0x80 (e.g. ITM_XEQP1=0x08AF) — §9.10 item 4 resolved
  }

  tmpString[opBytes    ] = (char)STRING_LABEL_VARIABLE;
  tmpString[opBytes + 1] = nameLength;
  xcopy(tmpString + opBytes + 2, funcParam, nameLength);
  _insertInProgram((uint8_t *)tmpString, nameLength + opBytes + 2);

  currentStep = findPreviousStep(currentStep);
  if(currentLocalStepNumber > 1) {
    --currentLocalStepNumber;
  }
  pemCursorIsZerothStep = false;
  if(!programListEnd) {
    scrollPemBackwards();
  }
}

void addStepInProgram(int16_t func) {
                                #if defined(DEBUG_PGM)
                                  print_caller(NULL);
                                #endif
  if((!pemCursorIsZerothStep) && ((aimBuffer[0] == 0 && !getSystemFlag(FLAG_ALPHA)) || tam.mode) && !isAtEndOfProgram(currentStep) && !isAtEndOfPrograms(currentStep)) {
    currentStep = findNextStep(currentStep);
    ++currentLocalStepNumber;
  }
  insertStepInProgram(func);
  if((aimBuffer[0] == 0 && !getSystemFlag(FLAG_ALPHA)) || tam.mode) {
    currentStep = findPreviousStep(currentStep);
    if(currentLocalStepNumber > 1) {
      --currentLocalStepNumber;
    }
    pemCursorIsZerothStep = false;
    if((indexOfItems[func].status & PTP_STATUS) == PTP_DISABLED) {
      switch(func) {
        case VAR_ACC:            // 1192
        case VAR_ULIM:           // 1193
        case VAR_LLIM:           // 1194
        case VAR_UX:             // 1205
        case VAR_LX:             // 1206
        case VAR_UEST:           // 2545
        case VAR_LEST:           // 2546
        case VAR_UY:             // 2547
        case VAR_LY:             // 2548
        case ITM_DELP:           // 1425
        case ITM_DELPALL:        // 1426
        case ITM_GTOP:           // 1482
        case ITM_KEYG:           // 1498
        case ITM_KEYX:           // 1499
        case ITM_42KEYG:         // 2795
        case ITM_42KEYX:         // 2796
        case ITM_BST:            // 1734
        case ITM_SST: {          // 1736
          break;
        }
        default: {
          return;
        }
      }
    }
    if(!programListEnd) {
      scrollPemBackwards();
    }
  }
}



calcRegister_t findNamedLabel(const char *labelName, uint8_t labelType) {
  return findNamedLabelWithDuplicate(labelName, 0, labelType);
}



calcRegister_t findNamedLabelWithDuplicate(const char *labelName, int16_t dupNum, uint8_t labelType) {
  if((labelType == ALL_LABELS) || (labelType == LOCAL_LABELS)) {       // Start searching for local named labels
    bool_t labelFound = false;
    uint16_t firstLabel = 0;
    uint16_t nextLabel = 0;
    uint16_t lbl;

    for(lbl=0; lbl<numberOfLabels; lbl++) {
      if(labelList[lbl].program > currentProgramNumber) {   // After the current program
        break;
      }
      if(labelList[lbl].program == currentProgramNumber) { // Within the current progrm
        if(labelList[lbl].step < 0 &&  *(labelList[lbl].labelPointer - 1) == LOCAL_LABEL_VARIABLE) { // Is a named local label
          uint8_t lblNameLen = boundProgramNameLength(labelList[lbl].labelPointer + 1, *(labelList[lbl].labelPointer));
          xcopy(tmpString, labelList[lbl].labelPointer + 1, lblNameLen);
          tmpString[lblNameLen] = 0;
          if(compareString(tmpString, labelName, CMP_BINARY) == 0) {   // Label name match
            if(!labelFound) {    // First label occurence in the current program
              firstLabel = lbl;
              labelFound = true;
            }
            uint16_t labelLocalStepNumber = (-labelList[lbl].step) - programList[currentProgramNumber - 1].step + 1;
            if(labelLocalStepNumber > currentLocalStepNumber) {
              nextLabel = lbl;  // First label occurence after the current program step
              break;
            }
          }
        }
      }
    }
    // return local label found, if any
    if(labelFound) {   // If a local label found in the program
      lbl = (nextLabel != 0 ? nextLabel : firstLabel); // First found label label after current program step or the first found label in the program
      return lbl + FIRST_LABEL;
    }
  }
  if((labelType == ALL_LABELS) || (labelType == GLOBAL_LABELS)) {      // then search global labels
    for(uint16_t lbl = 0; lbl < numberOfLabels; lbl++) {
      if(labelList[lbl].step > 0) {
        uint8_t lblNameLen = boundProgramNameLength(labelList[lbl].labelPointer + 1, *(labelList[lbl].labelPointer));
        xcopy(tmpString, labelList[lbl].labelPointer + 1, lblNameLen);
        tmpString[lblNameLen] = 0;
        if(compareString(tmpString, labelName, CMP_BINARY) == 0) {
          if(dupNum <= 0) {
            return lbl + FIRST_LABEL;
          }
          else {
            --dupNum;
          }
        }
      }
    }
  }
  return INVALID_VARIABLE;
}



uint16_t getNumberOfSteps(void) {
  if(currentProgramNumber == numberOfPrograms) {
    uint16_t numberOfSteps = 1;
    uint8_t *step = programList[currentProgramNumber - 1].instructionPointer;
    while(!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) { // END or .END.
      ++numberOfSteps;
      step = findNextStep(step);
    }
    return numberOfSteps;
  }
  else {
    return abs(programList[currentProgramNumber].step - programList[currentProgramNumber - 1].step);
  }
}
