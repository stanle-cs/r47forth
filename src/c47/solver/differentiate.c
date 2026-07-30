// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file differentiate.c
 ***********************************************/

#include "c47.h"

#define DERIV_FIRST_SHIFT       1   // h starts at x/10, the coarsest step a 15 point stencil is worth taking
#define DERIV_LAST_SHIFT       16   // and stops at x*1e-16, the step this engine used for every stencil before the ladder
#define DERIV_TOLERANCE_DIGITS 32   // digits a sample carries, less one for the coefficient sum, which is what two estimates are compared against


#if 0
// All of the above finite differences combined into a single array */
TO_QSPI static const FINITE_DIFF_COEFF *const all_first_derivatives[] = {
    &der_1_central_10,      &der_1_central_8,       &der_1_central_6,
    &der_1_central_4,       &der_1_central_2,
    &der_1_upper_5,         &der_1_upper_4,         &der_1_strict_upper_5,
    &der_1_upper_3,         &der_1_strict_upper_4,  &der_1_upper_2,
    &der_1_strict_upper_3,  &der_1_upper_1,         &der_1_strict_upper_2,
    &der_1_lower_5,         &der_1_lower_4,         &der_1_strict_lower_5,
    &der_1_lower_3,         &der_1_strict_lower_4,  &der_1_lower_2,
    &der_1_strict_lower_3,  &der_1_lower_1,         &der_1_strict_lower_2,
    NULL
};
TO_QSPI static const FINITE_DIFF_COEFF *const all_second_derivatives[] = {
    // &der_2_central_10
    &der_2_central_8,       &der_2_central_6,       &der_2_central_4,
    &der_2_central_2,
    &der_2_upper_5,         &der_2_upper_4,         &der_2_strict_upper_5,
    &der_2_upper_3,         &der_2_strict_upper_4,  &der_2_upper_2,
    &der_2_strict_upper_3,
    &der_2_lower_5,         &der_2_lower_4,         &der_2_strict_lower_5,
    &der_2_lower_3,         &der_2_strict_lower_4,  &der_2_lower_2,
    &der_2_strict_lower_3,
    NULL
};
#endif

static void calcDeriv(calcRegister_t label, const FINITE_DIFF_COEFF *const *finDiff);
static calcRegister_t deriv_pgm_variable(calcRegister_t label);

static void calcDerivOfOrder(uint16_t label, int order) {
  calcDeriv(label, finite_difference_table[order]);
}


// A program that declares MVARs has more than one thing it could be differentiated with respect to, and the answer depends on which, so the MVAR menu is opened for
// the user to say, the way the solver and the integrator do. The variable key stores the point and takes the selection, and f' on the last softkey runs it. Inside a
// running program, or under another engine, there is nobody to press a key: the derivative is taken there and then, with respect to the selected variable.
static bool_t deriv_open_mvar_menu(uint16_t label, uint16_t order, bool_t solving) {
  if(programRunStop == PGM_RUNNING || solving || getSystemFlag(FLAG_INTING)) {
    return false;
  }
  if(deriv_pgm_variable(label) == INVALID_VARIABLE) {
    return false;
  }
  currentSolverProgram = label - FIRST_LABEL;
  currentMvarLabel = INVALID_VARIABLE;   // the menu builds from currentSolverProgram, and its variable key acts for the solver rather than for VARMNU
  currentSolverStatus &= ~SOLVER_STATUS_EQUATION_MODE;
  currentSolverStatus |= (order == DERIVATIVE_FIRST_CENTRAL ? SOLVER_STATUS_EQUATION_1ST_DERIVATIVE : SOLVER_STATUS_EQUATION_2ND_DERIVATIVE);
  currentSolverStatus |= SOLVER_STATUS_INTERACTIVE;
  showSoftmenu(-MNU_MVAR);
  return true;
}

static void derivativeCommon(uint16_t label, uint16_t order, uint8_t ti) {
  currentSolverStatus &= ~SOLVER_STATUS_USES_FORMULA;
  bool_t solving = getSystemFlag(FLAG_SOLVING);
  char buf[2];

  if(label >= FIRST_LABEL && label <= LAST_LABEL && deriv_open_mvar_menu(label, order, solving)) {
    return;
  }
  setSystemFlag(FLAG_SOLVING);
  if(label >= FIRST_LABEL && label <= LAST_LABEL) {
    calcDerivOfOrder(label, order);
    temporaryInformation = ti;
  }
  else if(REGISTER_X <= label && label <= REGISTER_T) {
    // Interactive mode
    buf[0] = letteredRegisterName((calcRegister_t)label);
    buf[1] = 0;
    label = findNamedLabel(buf, GLOBAL_LABELS);
    if(label == INVALID_VARIABLE) {
      displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
      #if (EXTRA_INFO_ON_CALC_ERROR == 1)
        sprintf(errorMessage, "string '%s' is not a named label", buf);
        moreInfoOnError("In function derivativeCommon:", errorMessage, NULL, NULL);
      #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    }
    else if(!deriv_open_mvar_menu(label, order, solving)) {
      calcDerivOfOrder(label, order);
      temporaryInformation = ti;
    }
  }
  else {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "unexpected parameter %u", label);
      moreInfoOnError("In function derivativeCommon:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
  }
  if(!solving) {
    clearSystemFlag(FLAG_SOLVING);
  }
}

void fn1stDeriv(uint16_t label) {
  derivativeCommon(label, DERIVATIVE_FIRST_CENTRAL, TI_1ST_DERIVATIVE);
}

void fn2ndDeriv(uint16_t label) {
  derivativeCommon(label, DERIVATIVE_SECOND_CENTRAL, TI_2ND_DERIVATIVE);
}

static void derivativeEquation(uint16_t order, uint8_t ti) {
  // FLAG_SOLVING suppresses the per-item undo snapshot, so the one calcDeriv takes before sampling survives to be restored, and it is what lets execProgram run a
  // body at all, which a user delta-x label needs.
  bool_t solving = getSystemFlag(FLAG_SOLVING);
  real_t probeValue;
  snap_t savedRegister;
  bool_t restore;

  setSystemFlag(FLAG_SOLVING);
  //new method to maintain solver variable
  reallyRunFunction(ITM_RCL, currentSolverVariable);
  // The sampling stores each point in the variable, so its own value is kept here and put back after. A register cannot hold it: for a program the user's code runs
  // in between and reaches every temporary register, which is what used to hand the variable back holding a number out of that program.
  restore = (currentSolverVariable != INVALID_VARIABLE) && getRegisterAsRealQuiet(currentSolverVariable, &probeValue);
  if(restore) {
    saveRegisterSnapshot(currentSolverVariable, &savedRegister);
  }
  if(!(currentSolverStatus & SOLVER_STATUS_USES_FORMULA) && currentSolverProgram < numberOfLabels) {
    calcDerivOfOrder(currentSolverProgram + FIRST_LABEL, order);   // the MVAR menu was opened on a program, so that is what this key differentiates
  }
  else {
    currentSolverStatus |= SOLVER_STATUS_USES_FORMULA;
    calcDerivOfOrder(INVALID_VARIABLE, order);
  }
  if(restore) {
    restoreRegisterSnapshot(currentSolverVariable, &savedRegister);
  }
  temporaryInformation = ti;
  if(!solving) {
    clearSystemFlag(FLAG_SOLVING);
  }
}

void fn1stDerivEq(uint16_t unusedButMandatoryParameter) {
  derivativeEquation(DERIVATIVE_FIRST_CENTRAL, TI_1ST_DERIVATIVE);
}

void fn2ndDerivEq(uint16_t unusedButMandatoryParameter) {
  derivativeEquation(DERIVATIVE_SECOND_CENTRAL, TI_2ND_DERIVATIVE);
}

/* The following routines are ported from WP34s. */

static void deriv_found_lbl(calcRegister_t deltaX, real_t *h) {
  execProgram(deltaX);
  fnToReal(NOPARAM);
  if(getRegisterDataType(REGISTER_X) == dtReal34) {
    real34ToReal(REGISTER_REAL34_DATA(REGISTER_X), h);
  }
  else {
    lastErrorCode = ERROR_NONE;
    realCopy(const_1on10, h);
  }
}

static bool_t deriv_default_h(real_t *h, int shift) {
  calcRegister_t deltaX;
  unsigned int i;
  TO_QSPI static const char *const lbls[] = {
    STD_delta "x",  STD_delta "X",
    STD_DELTA "x",  STD_DELTA "X",
  };

  {   // the step the user set on the menu key or stored in the variable itself: it is used as it stands, and no ladder is walked
    real_t given;

    if((deltaX = findNamedVariable(STD_delta STD_SUB_d)) != INVALID_VARIABLE && getRegisterAsRealQuiet(deltaX, &given) && !realIsZero(&given) && !realIsSpecial(&given)) {
      realCopy(&given, h);
      return true;
    }
  }

  saveForUndo();
  reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
  realToReal34(h, REGISTER_REAL34_DATA(REGISTER_X));
  fnFillStack(NOPARAM);

  dynamicMenuItem = -1;
  for(i=0; i<nbrOfElements(lbls); i++) {
    if((deltaX = findNamedLabel(lbls[i], ALL_LABELS)) != INVALID_VARIABLE) {
      deriv_found_lbl(deltaX, h);
      undo();
      return true;
    }
  }
  undo();
  if(realIsZero(h)) {   // the step is relative to x, so at x=0 it collapses and the weighted sum is divided by zero
    realCopy(const_1, h);
  }
  h->exponent -= shift;
  return false;
}


// Do two estimates of the same derivative, taken a factor of ten apart in h, agree to better than the cancellation the finer one suffers? A sample carries 34 digits
// and the points are h apart, so differencing them loses the digits they share and the error is about 1e-DERIV_TOLERANCE_DIGITS times ten to the shift, for each
// order of the derivative. Agreement means the truncation of the coarser estimate is already below that, so the coarser one is the better of the two. The gap
// between the pair is handed back for the caller to rank the pairs by, whether they agreed or not.
static bool_t deriv_agrees(const real_t *coarse, const real_t *fine, int shift, uint8_t order, real_t *difference) {
  real_t tolerance;

  realSubtract(fine, coarse, difference, &ctxtReal39);
  if(realIsZero(difference)) {
    return true;
  }
  realCopyAbs(fine, &tolerance);
  tolerance.exponent += shift * order - DERIV_TOLERANCE_DIGITS;
  return realCompareAbsLessThan(difference, &tolerance);
}


// A program that declares MVARs takes its argument from named storage (RCL 'x'), not from the stack, so the sample point has to be stored where the program will
// recall it. Return the variable to perturb, or INVALID_VARIABLE for a program that declares none and therefore reads the stack. Among several MVARs the caller's
// selection wins whenever the program declares it, matching what the MVAR softmenu and the equation derivative differentiate with respect to; otherwise the first
// declaration, which is the argument by convention and the leftmost key of the MVAR menu.
static calcRegister_t deriv_pgm_variable(calcRegister_t label) {
  uint8_t *step;
  char name[MAX_LABEL_NAME_LENGTH + 1];
  calcRegister_t first = INVALID_VARIABLE;
  uint16_t declared;

  if(label < FIRST_LABEL || label > LAST_LABEL || (uint16_t)(label - FIRST_LABEL) >= numberOfLabels) {
    return INVALID_VARIABLE;
  }
  step = labelList[label - FIRST_LABEL].instructionPointer;

  for(declared = 0; declared < MAX_MVAR_DECLARATIONS; declared++) {
    while(checkOpCodeOfStep(step, ITM_REM)) {   // a REM ahead of an MVAR is transparent, as in the MVAR softmenu
      step = findNextStep(step);
    }
    if(!(checkOpCodeOfStep(step, ITM_MVAR) && *(step + 2) == STRING_LABEL_VARIABLE)) {
      break;
    }
    uint8_t nameLength = boundProgramNameLength(step + 4, *(step + 3));
    if(nameLength == 0 || nameLength > MAX_LABEL_NAME_LENGTH) {
      break;
    }
    xcopy(name, step + 4, nameLength);
    name[nameLength] = 0;
    calcRegister_t variable = findOrAllocateNamedVariable(name);
    if(variable != INVALID_VARIABLE) {
      if((uint16_t)variable == currentSolverVariable) {
        return variable;
      }
      if(first == INVALID_VARIABLE) {
        first = variable;
      }
    }
    step = findNextStep(step);
  }
  return first;
}


static void _differentiatorIteration(calcRegister_t label, calcRegister_t variable, real_t *r0) {
  reallocateRegister(REGISTER_X, dtReal34, 0, amNone);
  realToReal34(r0, REGISTER_REAL34_DATA(REGISTER_X));
  fnFillStack(NOPARAM);

  if(currentSolverStatus & SOLVER_STATUS_USES_FORMULA) {
    reallyRunFunction(ITM_STO, currentSolverVariable);
    parseEquation(currentFormula, EQUATION_PARSER_XEQ, tmpString, tmpString + AIM_BUFFER_LENGTH);
  }
  else {
    if(variable != INVALID_VARIABLE) {   // feed both channels: the stack for a program that consumes X, the variable for one that recalls its MVAR
      reallyRunFunction(ITM_STO, variable);
    }
    dynamicMenuItem = -1;
    execProgram(label);
    fnToReal(NOPARAM);
  }

  if(getRegisterDataType(REGISTER_X) == dtReal34) {
    real34ToReal(REGISTER_REAL34_DATA(REGISTER_X), r0);
  }
  else {
    lastErrorCode = ERROR_NONE;
    realSetNaN(r0);
  }
}

// Try to compute a single derivative estimate from a stencil
static bool_t calcOneDeriv(const FINITE_DIFF_COEFF *stencil, const real_t fxIn[],
                           const real_t *h, real_t *r, realContext_t *realContext) {
  uint16_t i, maxi = 2*stencil->n+1;
  real_t t, s;
  const real_t * const fx = fxIn + MAX_ORDER - stencil->n;

  // Check if all f(x) are defined or not
  for(i=0; i<maxi; i++) {
    if(stencil->coeff[i] != 0 && realIsSpecial(fx + i)) {
      return false;
    }
  }

  // All values are defined where required so calculate the weighted sum
  realSetZero(&s);
  for(i=0; i<maxi; i++) {
    if(stencil->coeff[i] != 0) {
      int32ToReal(fdValues[stencil->coeff[i]], &t);
      realFMA(fx+i, &t, &s, &s, realContext);
    }
  }
  // Inefficiently factor in the derivative order
  // It's not a problem since the order can only be 1 or 2 currently
  // For larger orders we need to divide the result by h^order
  uInt32ToReal(fdValues[stencil->denom], &t);
  for(i=0; i<stencil->order; i++) {
    realMultiply(&t, h, &t, realContext);
  }
  realDivide(&s, &t, r, realContext);
  return true;
}

// Compute the function values f(x + k h), k = -MAX_ORDER .. MAX_ORDER
static void calcFuncValues(calcRegister_t label, calcRegister_t variable, const real_t *x, real_t fx[MAX_F_EVAL], real_t *h, realContext_t *realContext) {
  int i;
  real_t t;

  for(i=0; i < MAX_F_EVAL; i++) {
    if(lastErrorCode == ERROR_SOLVER_ABORT || programRunStop == PGM_WAITING || exitKeyWaiting()) {
      // calcOneDeriv rejects a stencil only on the samples that stencil reads, so a narrow one would still succeed on the points already taken and hand back a
      // value beside the abort. Poison them all to make the abort the only outcome.
      lastErrorCode = ERROR_SOLVER_ABORT;
      if(programRunStop == PGM_RUNNING) {   // halt the outer program too, as every other abort point does
        programRunStop = PGM_WAITING;
      }
      for(i = 0; i < MAX_F_EVAL; i++) {
        realSetNaN(fx + i);
      }
      return;
    }
    int32ToReal(i - MAX_ORDER, &t);
    realFMA(&t, h, x, fx + i, realContext);
    _differentiatorIteration(label, variable, fx + i);
  }
}


// Evaluate the function at stencil points and compute "best" estimate
static void calcDeriv(calcRegister_t label, const FINITE_DIFF_COEFF *const *finDiff) {
  real_t x, h, probeValue, estimate, coarse, gap, best, bestGap, fx[MAX_F_EVAL];
  snap_t savedRegister;
  calcRegister_t variable = INVALID_VARIABLE;
  bool_t userStep = false;
  int i, shift, stencil, coarseStencil = -1, coarseShift = 0, bestShift = 0;

  if(!getRegisterAsReal(REGISTER_X, &x)) {
    return;
  }

  if(!realIsSpecial(&x)) {
    if(!(currentSolverStatus & SOLVER_STATUS_USES_FORMULA)) {
      uint8_t probeError = lastErrorCode;   // an MVAR name the variable allocator rejects raises here, before any sampling the caller asked for

      lastErrorCode = ERROR_NONE;
      variable = deriv_pgm_variable(label);
      if(lastErrorCode != ERROR_NONE) {   // no room for the MVAR: the user is told, rather than given the wrong answer a fall back to the stack would return
        return;
      }
      lastErrorCode = probeError;
      if(variable != INVALID_VARIABLE && !getRegisterAsRealQuiet(variable, &probeValue)) {
        variable = INVALID_VARIABLE;   // differentiate only with respect to something numeric
      }
    }

#if 0
    // This block of code prints out all the function evaluations and
    // all the various derivative estimates.
    {
      char buf[1000];

      for(i=0; i<MAX_F_EVAL; i++) {
        printf("f[x+%dh] = %s\n", i - MAX_ORDER, decNumberToString(fx+i, buf));
      }
      for(i=0; finDiff[i] != NULL; i++) {
        if(calcOneDeriv(finDiff[i], fx, &h, &x, &ctxtReal39)) {
          printf("df/dx = %s\t(%s)\n", decNumberToString(&x, buf), finDiff[i]->desc);
        }
      }
    }
#endif
    // Walk the step down a decade at a time. Each step gives one estimate, and two estimates from the same stencil that agree say the coarser step's truncation is
    // already lost in the noise, so the coarser one is taken: it is the one that threw away the fewest digits. A user delta-x label sets the step itself and is
    // taken as it stands.
    for(shift = DERIV_FIRST_SHIFT; shift <= DERIV_LAST_SHIFT; shift++) {
      if(variable != INVALID_VARIABLE) {
        // Kept here rather than in a register: the user program runs between the save and the restore and every temporary register is scratch to something it can
        // call, RCL of a stack register among them. The snapshot carries the type and the tag, so the value comes back as itself and not as the real34 the sampling
        // stored. It is taken again for each step, because the restore hands back the long integer it holds. getRegisterAsRealQuiet has already turned away
        // everything the snapshot does not cover.
        saveRegisterSnapshot(variable, &savedRegister);
      }
      realCopy(&x, &h);   // Pass X into the h determination code to allow relative steps
      userStep = deriv_default_h(&h, shift);

      // Compute the function at the finite difference points
      saveForUndo();
      calcFuncValues(label, variable, &x, fx, &h, &ctxtReal39);
      undo();
      if(variable != INVALID_VARIABLE) {   // undo() rolls back the stack only, so the sampled variable is put back here
        restoreRegisterSnapshot(variable, &savedRegister);
      }
      if(lastErrorCode == ERROR_SOLVER_ABORT) {
        break;
      }

      // Try finite differences until we get a result
      stencil = -1;
      for(i=0; finDiff[i] != NULL; i++) {
        if(calcOneDeriv(finDiff[i], fx, &h, &estimate, &ctxtReal39)) {
          stencil = i;
          break;
        }
      }
      if(stencil < 0) {   // every stencil rejected this step's samples, so take the points closer in
        continue;
      }
      if(userStep) {
        realCopy(&estimate, &x);
        goto found;
      }
      if(stencil == coarseStencil) {
        if(deriv_agrees(&coarse, &estimate, shift, finDiff[stencil]->order, &gap)) {
          goto settled;
        }
        // The two are a decade apart, so the gap between them is smallest where the truncation of the coarser one and the cancellation of the finer one balance.
        // The coarser member of the closest pair is therefore the best the ladder saw, and it is what the answer falls back to when no pair ever agrees.
        if(bestShift == 0 || realCompareAbsLessThan(&gap, &bestGap)) {
          realCopy(&coarse, &best);
          realCopy(&gap, &bestGap);
          bestShift = coarseShift;
        }
      }
      realCopy(&estimate, &coarse);
      coarseStencil = stencil;
      coarseShift = shift;
    }
    if(coarseStencil < 0) {   // no step gave a usable set of samples
      goto noResult;
    }
    if(bestShift != 0) {   // the ladder ran out without a pair ever agreeing, so the closest pair is as near as this function gets
      realCopy(&best, &coarse);
      coarseShift = bestShift;
    }

settled:                      // the coarser of the two estimates is the answer, and its own step is what the display reports
    realCopy(&x, &h);
    if(realIsZero(&h)) {
      realCopy(const_1, &h);
    }
    h.exponent -= coarseShift;
    realCopy(&coarse, &x);
    goto found;
  }
  goto noResult;

found:
  {
    //Add string, for display at TI
    decContext c = ctxtReal4;
    c.digits = 2;
    real_t hh;
    realPlus(&h, &hh, &c);
    strcpy(errorMessage, STD_delta "=");
    decNumberToString(&hh, errorMessage + stringByteLength(errorMessage));
    strcat(errorMessage, "; ");
    goto finish;
  }

noResult:;
  // No estimate possible
  realSetNaN(&x);
  //Add string, for display at TI
  errorMessage[0] = 0;

finish:
  convertRealToResultRegister(&x, REGISTER_X, amNone);
}

