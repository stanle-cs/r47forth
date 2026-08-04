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

#if defined(FORTH_DEBUG_SELFTEST)
uint32_t paramCoreDebugNameLengthReads = 0;
#endif

/* F2-2 (§10.2): bounded variant of decode.c's getStringLabelOrVariableName.
 * end is EXCLUSIVE. A name that would read past end is clamped to the
 * available bytes; the caller's normal not-found path then reports it.
 * The unbounded decode.c reader remains for display paths only. */
static void paramCoreReadName(const uint8_t *stringAddress, const uint8_t *end) {
  uint8_t stringLength = 0;
  const uint8_t *nameBytes = end;
  if(stringAddress < end) {
    stringLength = *(uint8_t *)(stringAddress++);
    nameBytes = stringAddress;
    #if defined(FORTH_DEBUG_SELFTEST)
      paramCoreDebugNameLengthReads++;
    #endif
    if((end - stringAddress) < stringLength) {
      stringLength = (uint8_t)(end - stringAddress);
    }
  }
  xcopy(tmpStringLabelOrVariableName, nameBytes, stringLength);
  tmpStringLabelOrVariableName[stringLength] = 0;
}

/* A bounded entry must also bound its fixed-width cells.  Name reads already
 * clamp by contract; a missing structural byte is corrupted encoded data. */
static bool paramCoreReadByte(const uint8_t *address, const uint8_t *end,
                              uint8_t *value) {
  if(address >= end) {
    displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return false;
  }
  *value = *address;
  return true;
}

static void _executeWithIndirectRegister(uint8_t *paramAddress,
                                         const uint8_t *end, uint16_t op) {
  uint8_t opParam;
  if(!paramCoreReadByte(paramAddress, end, &opParam)) return;
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

static void _executeWithIndirectVariable(uint8_t *stringAddress, const uint8_t *end, uint16_t op) {
  calcRegister_t regist;
  bool_t  tryAllocate = isFunctionAllowingNewVariable(op);
  paramCoreReadName(stringAddress, end);
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

void paramCoreExecuteOpBounded(uint8_t *paramAddress, const uint8_t *end, uint16_t op, uint16_t paramMode) {
  uint8_t opParam;
  if(!paramCoreReadByte(paramAddress, end, &opParam)) return;
  paramAddress++;
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
        paramCoreReadName(paramAddress, end);
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
          /* F3-3A: this resolution acts for the step being executed — enter
           * the owning program's scope (first touch included) so same-
           * program words resolve and cross-scope words do not.  paramAddress
           * points into the step; a non-program address (defensive) falls
           * back to INTERACTIVE inside the helper.  Scope guards name
           * resolution only: the FCALL dispatch below runs by ref and needs
           * no scope of its own. */
          uint16_t prevScope = forthScopeEnterProgramStep(paramAddress);
          if(lastErrorCode != ERROR_NONE) {
            forthScopeRestore(prevScope);   /* first-touch pre-scan failed: halt this step */
          }
          else {
            uint16_t resolvedParam;
            forthXEQType_t res = forthResolveXEQ(tmpStringLabelOrVariableName, &resolvedParam);
            if(res == FORTH_XEQ_COLON) {
              reallyRunFunction(ITM_FCALL, resolvedParam);
              if(op == ITM_XEQP1 && programRunStop == PGM_RUNNING && lastErrorCode == ERROR_NONE) {
                currentReturnLocalStep++;
              }
            }
            else if(res == FORTH_XEQ_ITEM) {
              reallyRunFunction(resolvedParam, NOPARAM);
            }
            else {
              displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
              #if (EXTRA_INFO_ON_CALC_ERROR == 1)
                sprintf(errorMessage, "string '%s' is not a named label", tmpStringLabelOrVariableName);
                moreInfoOnError("In function _executeOp:", errorMessage, NULL, NULL);
              #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
            }
            forthScopeRestore(prevScope);
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
        _executeWithIndirectRegister(paramAddress, end, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, end, op);
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
        uint8_t systemFlag;
        if(!paramCoreReadByte(paramAddress, end, &systemFlag)) break;
        if(systemFlag < 64) { // first 64 system flags
          reallyRunFunction(op, indexOfItems[systemFlag + SFL_TDM24].param);
        }
        else { // other system flags
          reallyRunFunction(op, indexOfItems[(systemFlag & 0x3f) + SFL_MONIT].param);
        }
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, end, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, end, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_FLAG, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_NUMBER_8: {
      if (paramCoreValidateDirect(op, PTP_NUMBER_8, opParam)) {
        paramCoreDispatchDirect(op, PTP_NUMBER_8, opParam);
      }
      else if(opParam == INDIRECT_REGISTER) {
        _executeWithIndirectRegister(paramAddress, end, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, end, op);
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
          uint8_t extension;
          if(!paramCoreReadByte(paramAddress, end, &extension)) break;
          reallyRunFunction(op, 250 + extension);
        }
        else if(opParam == INDIRECT_REGISTER) {
          _executeWithIndirectRegister(paramAddress, end, op);
        }
        else if(opParam == INDIRECT_VARIABLE) {
          _executeWithIndirectVariable(paramAddress, end, op);
        }
        else {
          sprintf(tmpString, "\nIn function _executeOp: case PARAM_NUMBER, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
        }
        break;
      }

    case PARAM_NUMBER_16: {
        if(isFunctionOldParam16(op)) {  // original Param16 functions without indirection support (little endian parameter)
          uint8_t byte1;
          if(!paramCoreReadByte(paramAddress, end, &byte1)) break;
          uint16_t val = opParam + 256 * byte1;
          if (paramCoreValidateDirect(op, PTP_NUMBER_16, val)) {
            paramCoreDispatchDirect(op, PTP_NUMBER_16, val);
          }
        }
        else {                        // new Param16 functions with indirection support (big endian parameter)
          if(opParam == INDIRECT_REGISTER) {
            _executeWithIndirectRegister(paramAddress, end, op);
          }
          else if(opParam == INDIRECT_VARIABLE) {
            _executeWithIndirectVariable(paramAddress, end, op);
          }
          else {
            uint8_t byte1;
            if(!paramCoreReadByte(paramAddress, end, &byte1)) break;
            uint16_t val = (opParam * 256) + byte1;
            if (paramCoreValidateDirect(op, PTP_NUMBER_16, val)) {
              paramCoreDispatchDirect(op, PTP_NUMBER_16, val);
            }
            else {
              sprintf(tmpString, "\nIn function _executeOp: case PARAM_NUMBER, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
            }
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
        paramCoreReadName(paramAddress, end);
        calcRegister_t regist = findNamedVariable(tmpStringLabelOrVariableName);
        if(tryAllocate) {
          // Reuse the lookup above; failed allocation remains INVALID_VARIABLE.
          if(regist == INVALID_VARIABLE) {
            regist = findOrAllocateNamedVariable(tmpStringLabelOrVariableName);
          }
          reallyRunFunction(op, regist);
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
        _executeWithIndirectRegister(paramAddress, end, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, end, op);
      }
      else {
        sprintf(tmpString, "\nIn function _executeOp: case PARAM_REGISTER / PARAM_COMPARE, %s  %u is not a valid parameter!", indexOfItems[op].itemCatalogName, opParam);
      }
      break;
      }

    case PARAM_MENU: {
      if(opParam == STRING_LABEL_VARIABLE) {
        paramCoreReadName(paramAddress, end);
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
        _executeWithIndirectRegister(paramAddress, end, op);
      }
      else if(opParam == INDIRECT_VARIABLE) {
        _executeWithIndirectVariable(paramAddress, end, op);
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

/* F2-3 (§10.2): shared direct-parameter validation.
 * Mirrors the traced native range checks exactly.
 * PTP_NUMBER_16: native arm applies no range check in either the
 * isFunctionOldParam16 or the new-param16 path — always true. */
bool paramCoreValidateDirect(uint16_t op, uint16_t ptpClass, uint16_t value) {
  if (ptpClass == PTP_NONE) {
    return true;
  }
  else if (ptpClass == PTP_NUMBER_8 || ptpClass == PTP_KEYG_KEYX) {
    return value <= (indexOfItems[op].tamMinMax & TAM_MAX_MASK);
  }
  else if (ptpClass == PTP_NUMBER_16) {
    return true;
  }
  else if (ptpClass == PTP_NUMBER_8_16) {
    return value <= (indexOfItems[op].tamMinMax & TAM_MAX_MASK);
  }
  else if (ptpClass == PTP_REGISTER) {
    return value <= LAST_SPARE_REGISTERS_IN_KS_CODE && regInRange(regKStoC((uint8_t)value));
  }
  else if (ptpClass == PTP_FLAG) {
    return value <= LAST_LOCAL_FLAG || (FLAG_M <= value && value < FLAG_W);
  }
  else if (ptpClass == PTP_SHUFFLE) {
    return true;
  }
  return false;
}

/* F2-3/F4-2: shared direct-parameter dispatch.
 * PTP_REGISTER converts KS code to C register; all other classes pass value as-is. */
void paramCoreDispatchDirect(uint16_t op, uint16_t ptpClass, uint16_t value) {
  if (ptpClass == PTP_REGISTER) {
    reallyRunFunction((int16_t)op, regKStoC((uint8_t)value));
  } else {
    reallyRunFunction((int16_t)op, value);
  }
}

/* F4-3: unbounded wrapper — delegates to Bounded with program-memory end.
 * All landed native callers go through this unchanged. */
void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode) {
  paramCoreExecuteOpBounded(paramAddress, firstFreeProgramByte, op, paramMode);
}
