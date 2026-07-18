// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file param_core.c
 * §10.2: byte-identical extraction of the native
 * parameter execution core from lblGtoXeq.c.
 ***********************************************/

#include "c47.h"
#include "forth_dict.h"
#include "param_core.h"

/* F2-2 (§10.2): bounded variant of decode.c's getStringLabelOrVariableName.
 * end is EXCLUSIVE. A name that would read past end is clamped to the
 * available bytes; the caller's normal not-found path then reports it.
 * The unbounded decode.c reader remains for display paths only. */
static void paramCoreReadName(const uint8_t *stringAddress, const uint8_t *end) {
  uint8_t stringLength = *(uint8_t *)(stringAddress++);
  if(stringAddress >= end) {
    stringLength = 0;
  }
  else if(stringLength > (uint8_t)(end - stringAddress)) {
    stringLength = (uint8_t)(end - stringAddress);
  }
  xcopy(tmpStringLabelOrVariableName, stringAddress, stringLength);
  tmpStringLabelOrVariableName[stringLength] = 0;
}

static void _executeWithIndirectRegister(uint8_t *paramAddress, uint16_t op) {
  uint8_t opParam = *(uint8_t *)paramAddress;
  bool_t  tryAllocate = isFunctionAllowingNewVariable(op);
  if(opParam <= LAST_SPARE_REGISTERS_IN_KS_CODE) { // Local register from .00 to .98
      int16_t realParam = indirectAddressing(regKStoC(opParam), indirectionType(op), indexOfItems[op].tamMinMax >> TAM_MAX_BITS, indexOfItems[op].tamMinMax & TAM_MAX_MASK, tryAllocate);
      if(realParam != FAILED_INDIRECTION) {
        reallyRunFunction(op, realParam);
      }
  }
  else {
    sprintf(tmpString, "\nIn function _executeWithIndirectRegister: %s " STD_RIGHT_ARROW " %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
  }
}

static void _executeWithIndirectVariable(uint8_t *stringAddress, uint16_t op) {
  calcRegister_t regist;
  bool_t  tryAllocate = isFunctionAllowingNewVariable(op);
  paramCoreReadName(stringAddress, firstFreeProgramByte);
  regist = findNamedVariable(tmpStringLabelOrVariableName);
  if(regist != INVALID_VARIABLE) {
      int16_t realParam = indirectAddressing(regist, indirectionType(op), indexOfItems[op].tamMinMax >> TAM_MAX_BITS, indexOfItems[op].tamMinMax & TAM_MAX_MASK, tryAllocate);
      if(realParam != FAILED_INDIRECTION) {
        reallyRunFunction(op, realParam);
      }
  }
  else {
    displayCalcErrorMessage(ERROR_UNDEF_SOURCE_VAR, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "string '%s' is not a named variable", tmpStringLabelOrVariableName);
      moreInfoOnError("In function _executeWithIndirectVariable:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
  }
}

void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode) {
  uint8_t opParam = *(uint8_t *)(paramAddress++);
  bool_t tryAllocate = isFunctionAllowingNewVariable(op);

  switch(paramMode) {
    case PARAM_DECLARE_LABEL: {
      // nothing to do
      break;
      }

      case PARAM_LABEL: {
      if(opParam <= LAST_LOCAL_LABEL) { // Local label from 00 to 99 or from A to l
        reallyRunFunction(op, opParam);
      }
      else if((opParam == STRING_LABEL_VARIABLE) || (opParam == LOCAL_LABEL_VARIABLE)) {
        paramCoreReadName(paramAddress, firstFreeProgramByte);
        /* Rebase to b8f79e486: upstream added named LOCAL labels, reusing
         * this same opParam byte (STRING_LABEL_VARIABLE == GLOBAL_LABELS,
         * LOCAL_LABEL_VARIABLE == LOCAL_LABELS) as the labelType selector —
         * do the real, opParam-aware label search FIRST, exactly as
         * upstream's fix does, so a step that specifically encodes a local
         * name resolves against local labels, not silently against an
         * unrelated global label/colon word/item of the same name.
         * Forth's XEQ/XEQP1 colon+item fallback only applies when the step
         * encoded a GLOBAL name (opParam == GLOBAL_LABELS): a step asking
         * for a LOCAL label must fail as "not found" if no local label
         * matches, never fall through to Forth vocabulary. */
        calcRegister_t label = findNamedLabel(tmpStringLabelOrVariableName, opParam);
        bool_t forthFallbackEligible = (opParam == GLOBAL_LABELS)
                                     && (op == ITM_XEQ || op == ITM_XEQP1);
        if (label != INVALID_VARIABLE) {
          reallyRunFunction(op, label);
        }
        else if (forthFallbackEligible) {
          uint16_t resolvedParam;
          forthXEQType_t res = forthResolveXEQ(tmpStringLabelOrVariableName, &resolvedParam);
          if (res == FORTH_XEQ_COLON) {
            reallyRunFunction(ITM_FCALL, resolvedParam);
            if(op == ITM_XEQP1 && programRunStop == PGM_RUNNING && lastErrorCode == ERROR_NONE) {
              currentReturnLocalStep++;
            }
          }
          else if (res == FORTH_XEQ_ITEM) {
            reallyRunFunction(resolvedParam, NOPARAM);
          }
          else {
            displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
            #if (EXTRA_INFO_ON_CALC_ERROR == 1)
              sprintf(errorMessage, "string '%s' is not a named label", tmpStringLabelOrVariableName);
              moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
            #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
          }
        }
        else if (op == ITM_LBLQ) {
          reallyRunFunction(op, (uint16_t)INVALID_VARIABLE);
        }
        else {
          displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
          #if (EXTRA_INFO_ON_CALC_ERROR == 1)
            sprintf(errorMessage, "string '%s' is not a named label", tmpStringLabelOrVariableName);
            moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
          #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
        }
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_LABEL, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_FLAG: {
      if(opParam <= LAST_LOCAL_FLAG) { // Global flag from 00 to 99, Lettered flag from X to K, or Local flag from .00 to .31
        reallyRunFunction(op, opParam);
      }
      else if(FLAG_M <= opParam && opParam < FLAG_W) { // Lettered flag from M to W
         reallyRunFunction(op, opParam);
      }
      else if(opParam == SYSTEM_FLAG_NUMBER) {
        if(*paramAddress < 64) { // first 64 system flags
          reallyRunFunction(op, indexOfItems[(*paramAddress) + SFL_TDM24].param);
        }
        else { // other system flags
          reallyRunFunction(op, indexOfItems[((*paramAddress) & 0x3f) + SFL_MONIT].param);
        }
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_FLAG, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_NUMBER_8: {
      if(opParam <= (indexOfItems[op].tamMinMax & TAM_MAX_MASK)) { // Value from 0 to 99
        reallyRunFunction(op, opParam);
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_NUMBER, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_NUMBER_8_16: {
        if(opParam <= 249) { // Value from 0 to 249
          reallyRunFunction(op, opParam);
        }
        else if(opParam == CNST_BEYOND_250) { // Value from 250 to 499
          reallyRunFunction(op, 250 + *(paramAddress));
        }
        else if(opParam == INDIRECT_REGISTER) {
          _executeWithIndirectRegister(paramAddress, op);
        }
        else if(opParam == INDIRECT_VARIABLE) {
          _executeWithIndirectVariable(paramAddress, op);
        }
        else {
          sprintf(tmpString, "\nIn function _executeOp: case PARAM_NUMBER, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
        }
        break;
      }

    case PARAM_NUMBER_16: {
        if(isFunctionOldParam16(op)) {  // original Param16 functions without indirection support (little endian parameter)
          reallyRunFunction(op, opParam + 256 * *(paramAddress));
        }
        else {                        // new Param16 functions with indirection support (big endian parameter)
          if(opParam == INDIRECT_REGISTER) {
            _executeWithIndirectRegister(paramAddress, op);
          }
          else if(opParam == INDIRECT_VARIABLE) {
            _executeWithIndirectVariable(paramAddress, op);
          }
          else {
            reallyRunFunction(op, (opParam * 256) + *(paramAddress));
          }
        }
        break;
      }

    case PARAM_REGISTER:
    case PARAM_COMPARE: {
      if(opParam <= LAST_SPARE_REGISTERS_IN_KS_CODE) { // Global register from 00 to 99, Lettered register from X to K, or Local register from .00 to .98
        if(regInRange(regKStoC(opParam))) {
          reallyRunFunction(op, regKStoC(opParam));
        }
      }
      else if(opParam == STRING_LABEL_VARIABLE) {
        paramCoreReadName(paramAddress, firstFreeProgramByte);
        calcRegister_t regist = findNamedVariable(tmpStringLabelOrVariableName);
        if(tryAllocate) {
          reallyRunFunction(op, findOrAllocateNamedVariable(tmpStringLabelOrVariableName));
        }
        else if(regist != INVALID_VARIABLE) {
          reallyRunFunction(op, regist);
        }
        else {
          displayCalcErrorMessage(ERROR_UNDEF_SOURCE_VAR, ERR_REGISTER_LINE, REGISTER_X);
          #if (EXTRA_INFO_ON_CALC_ERROR == 1)
            sprintf(errorMessage, "string '%s' is not a named variable", tmpStringLabelOrVariableName);
            moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
          #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
        }
      }
      else if(paramMode == PARAM_COMPARE && opParam == VALUE_0) {
        reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
        real34SetZero(REGISTER_REAL34_DATA(TEMP_REGISTER_1));
        reallyRunFunction(op, TEMP_REGISTER_1);
      }
      else if(paramMode == PARAM_COMPARE && opParam == VALUE_1) {
        reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
        real34SetOne(REGISTER_REAL34_DATA(TEMP_REGISTER_1));
        reallyRunFunction(op, TEMP_REGISTER_1);
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_REGISTER / PARAM_COMPARE, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_MENU: {
      if(opParam == STRING_LABEL_VARIABLE) {
        paramCoreReadName(paramAddress, firstFreeProgramByte);
        int16_t menu_id = findMenu(tmpStringLabelOrVariableName);
        if(tryAllocate) {
          reallyRunFunction(op, findOrAllocateNamedVariable(tmpStringLabelOrVariableName));
        }
        else if(menu_id != INVALID_MENU) {
          reallyRunFunction(op, menu_id);
        }
        else {
          displayCalcErrorMessage(ERROR_UNDEF_MENU, ERR_REGISTER_LINE, REGISTER_X);
          #if (EXTRA_INFO_ON_CALC_ERROR == 1)
            sprintf(errorMessage, "string '%s' is not a menu name", tmpStringLabelOrVariableName);
            moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
          #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
        }
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_REGISTER / PARAM_COMPARE, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_SKIP_BACK:
      case PARAM_SHUFFLE: {
      reallyRunFunction(op, opParam);
      break;
      }

      default: {
      sprintf(tmpString, "\nIn function _executeOp: paramMode %u is not valid!\n", paramMode);
  }
}
  }
