// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The C47 Authors

/**
 * \file value.c
 * \brief Data type aware value type processing for T47 DSL
 */

#include "value.h"

#if defined(PC_BUILD)

#include <ctype.h>
#include <jim.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * True when the argument is not supported in scripts.
 */
static bool_t dslUnsupportedArg(Jim_Interp *interp, const char *arg, const char *kind)
{
    if(arg[0] == '.' && arg[1] != '\0') {
        Jim_SetResultFormatted(interp,
            "local %s '.NN' not supported in scripts", kind);
        return TRUE;
    }
    if(arg[0] == '-' && arg[1] == '>') {
        Jim_SetResultFormatted(interp,
            "indirect %s '->...' not supported in scripts", kind);
        return TRUE;
    }
    return FALSE;
}

/**
 * Return the register number for a given letter.
 */
static int16_t dslRegisterFromLetter(char letter)
{
    const char *p = strchr(registerFlagLetters, toupper((unsigned char)letter));
    if(!p) {
        return INVALID_VARIABLE;
    }
    int idx = (int)(p - registerFlagLetters);
    if(idx <= REGISTER_K - FIRST_LETTERED_REGISTER) {
        return (int16_t)(FIRST_LETTERED_REGISTER + idx);
    }
    if(idx <= REGISTER_S - FIRST_STAT_REGISTER + 12) {
        return (int16_t)(FIRST_STAT_REGISTER + (idx - 12));
    }
    return (int16_t)(FIRST_SPARE_REGISTER + (idx - 18));
}

/**
 * Parse a register argument.
 */
int dslParseRegisterArg(Jim_Interp *interp, int16_t op, const char *arg,
        uint16_t *outParam)
{
    char internalName[64];

    if(dslUnsupportedArg(interp, arg, "register")) {
        return JIM_ERR;
    }

    if(arg[0] != '\0' && arg[strspn(arg, "0123456789")] == '\0') {
        long n = strtol(arg, NULL, 10);
        if(n < 0 || n > 99) {
            Jim_SetResultFormatted(interp, "register number out of range: '%s'", arg);
            return JIM_ERR;
        }
        calcRegister_t reg = (calcRegister_t)(FIRST_GLOBAL_REGISTER + n);
        if(!regInRange(reg)) {
            Jim_SetResultFormatted(interp, "invalid register: '%s'", arg);
            return JIM_ERR;
        }
        *outParam = (uint16_t)reg;
        return JIM_OK;
    }

    if(arg[1] == '\0' && isalpha((unsigned char)arg[0])) {
        calcRegister_t reg = dslRegisterFromLetter(arg[0]);
        if(reg == INVALID_VARIABLE || !regInRange(reg)) {
            Jim_SetResultFormatted(interp, "invalid register letter: '%s'", arg);
            return JIM_ERR;
        }
        *outParam = (uint16_t)reg;
        return JIM_OK;
    }

    if(strlen(arg) >= sizeof(internalName)/2) {
        Jim_SetResultFormatted(interp, "register name too long: '%s'", arg);
        return JIM_ERR;
    }
    utf8ToString((const uint8_t *)arg, internalName);
    calcRegister_t reg;
    if(isFunctionAllowingNewVariable((uint16_t)op)) {
        reg = findOrAllocateNamedVariable(internalName);
    } else {
        reg = findNamedVariable(internalName);
        if(reg == INVALID_VARIABLE) {
            Jim_SetResultFormatted(interp, "undefined variable: '%s'", arg);
            return JIM_ERR;
        }
    }
    if(reg == INVALID_VARIABLE) {
        Jim_SetResultFormatted(interp, "could not allocate variable: '%s'", arg);
        return JIM_ERR;
    }
    *outParam = (uint16_t)reg;
    return JIM_OK;
}

/**
 * Parse a label argument.
 */
static int dslParseLabelArg(Jim_Interp *interp, const char *arg, uint16_t *outParam)
{
    char internalName[64];

    if(arg[0] != '\0' && arg[strspn(arg, "0123456789")] == '\0') {
        long n = strtol(arg, NULL, 10);
        if(n < 0 || n > 99) {
            Jim_SetResultFormatted(interp, "label number out of range: '%s'", arg);
            return JIM_ERR;
        }
        *outParam = (uint16_t)n;
        return JIM_OK;
    }

    if(arg[1] == '\0') {
        char c = arg[0];
        if(c >= 'A' && c <= 'L') {
            *outParam = (uint16_t)(100 + (c - 'A'));
            return JIM_OK;
        }
        if(c >= 'a' && c <= 'l') {
            *outParam = (uint16_t)(FIRST_LC_LOCAL_LABEL + (c - 'a'));
            return JIM_OK;
        }
    }

    if(strlen(arg) >= sizeof(internalName)/2) {
        Jim_SetResultFormatted(interp, "label name too long: '%s'", arg);
        return JIM_ERR;
    }
    utf8ToString((const uint8_t *)arg, internalName);
    calcRegister_t label = findNamedLabel(internalName);
    if(label == INVALID_VARIABLE) {
        Jim_SetResultFormatted(interp, "label not found: '%s'", arg);
        return JIM_ERR;
    }
    *outParam = (uint16_t)label;
    return JIM_OK;
}

/**
 * Parse a flag argument.
 */
int dslParseFlagArg(Jim_Interp *interp, const char *arg, uint16_t *outParam)
{
    char internalName[64];

    if(dslUnsupportedArg(interp, arg, "flag")) {
        return JIM_ERR;
    }

    if(arg[0] != '\0' && arg[strspn(arg, "0123456789")] == '\0') {
        long n = strtol(arg, NULL, 10);
        if(n < 0 || n > LAST_GLOBAL_FLAG) {
            Jim_SetResultFormatted(interp, "flag number out of range: '%s'", arg);
            return JIM_ERR;
        }
        *outParam = (uint16_t)n;
        return JIM_OK;
    }

    if(arg[1] == '\0' && isalpha((unsigned char)arg[0])) {
        const char *p = strchr(registerFlagLetters, toupper((unsigned char)arg[0]));
        if(!p) {
            Jim_SetResultFormatted(interp, "invalid flag letter: '%s'", arg);
            return JIM_ERR;
        }
        int idx = (int)(p - registerFlagLetters);
        if(idx <= FLAG_K - FLAG_X) {
            *outParam = (uint16_t)(FLAG_X + idx);
        } else {
            *outParam = (uint16_t)(FLAG_M + (idx - 12));
        }
        return JIM_OK;
    }

    if(strlen(arg) >= sizeof(internalName)/2) {
        Jim_SetResultFormatted(interp, "flag name too long: '%s'", arg);
        return JIM_ERR;
    }
    utf8ToString((const uint8_t *)arg, internalName);
    for(int i = 0; i < LAST_ITEM; ++i) {
        if((indexOfItems[i].status & CAT_STATUS) == CAT_SYFL &&
                compareString(internalName, indexOfItems[i].itemCatalogName, CMP_NAME) == 0) {
            *outParam = indexOfItems[i].param;
            return JIM_OK;
        }
    }
    Jim_SetResultFormatted(interp, "system flag not found: '%s'", arg);
    return JIM_ERR;
}

/**
 * Parse a numeric argument.
 */
static int dslParseNumericArg(Jim_Interp *interp, int16_t index, const char *arg,
        uint16_t *outParam)
{
    item_t item = indexOfItems[index];
    int16_t minVal = item.tamMinMax >> TAM_MAX_BITS;
    int16_t maxVal = item.tamMinMax & TAM_MAX_MASK;

    if(index == ITM_PNORM) {
        if(strcasecmp(arg, "NNZ") == 0) {
            *outParam = pNorm_0_NNZ;
            return JIM_OK;
        }
        if(strcasecmp(arg, "CNORM") == 0) {
            *outParam = pNorm_1_CNORM;
            return JIM_OK;
        }
        if(strcasecmp(arg, "ENORM") == 0) {
            *outParam = pNorm_2_ENORM;
            return JIM_OK;
        }
        if(strcasecmp(arg, "RNORM") == 0 || strcasecmp(arg, "inf") == 0 ||
                strcasecmp(arg, "INFINITY") == 0) {
            *outParam = pNorm_inf_RNORM;
            return JIM_OK;
        }
    }

    if(arg[0] == '\0' || arg[strspn(arg, "0123456789")] != '\0') {
        Jim_SetResultFormatted(interp, "expected numeric argument, got '%s'", arg);
        return JIM_ERR;
    }
    long n = strtol(arg, NULL, 10);
    if(((item.status & PTP_STATUS) == PTP_NUMBER_8_16) && n >= 250 && n <= 499) {
        *outParam = (uint16_t)n;
        return JIM_OK;
    }
    if(n < minVal || n > maxVal) {
        Jim_SetResultFormatted(interp, "value out of range [%d,%d]: '%s'",
            minVal, maxVal, arg);
        return JIM_ERR;
    }
    *outParam = (uint16_t)n;
    return JIM_OK;
}

/**
 * Parse a menu argument.
 */
static int dslParseMenuArg(Jim_Interp *interp, const char *arg, uint16_t *outParam)
{
    char internalName[64];

    if(strlen(arg) >= sizeof(internalName)/2) {
        Jim_SetResultFormatted(interp, "menu name too long: '%s'", arg);
        return JIM_ERR;
    }
    utf8ToString((const uint8_t *)arg, internalName);
    int16_t menu = findMenu(internalName);
    if(menu == INVALID_MENU) {
        Jim_SetResultFormatted(interp, "menu not found: '%s'", arg);
        return JIM_ERR;
    }
    *outParam = (uint16_t)menu;
    return JIM_OK;
}

/**
 * Generic parameter parser for catalog functions.  Dispatches calls
 * to the above type-specific parsers.
 */
int dslParseParam(Jim_Interp *interp, int16_t index, const char *arg,
        uint16_t *outParam)
{
    uint16_t paramMode = (indexOfItems[index].status & PTP_STATUS) >> 9;

    switch(paramMode) {
        case PARAM_REGISTER:
            return dslParseRegisterArg(interp, index, arg, outParam);
        case PARAM_LABEL:
        case PARAM_DECLARE_LABEL:
            return dslParseLabelArg(interp, arg, outParam);
        case PARAM_FLAG:
            return dslParseFlagArg(interp, arg, outParam);
        case PARAM_COMPARE:
            if(strcmp(arg, "0") == 0) {
                reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
                real34SetZero(REGISTER_REAL34_DATA(TEMP_REGISTER_1));
                *outParam = TEMP_REGISTER_1;
                return JIM_OK;
            }
            if(strcmp(arg, "1") == 0) {
                reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
                real34SetOne(REGISTER_REAL34_DATA(TEMP_REGISTER_1));
                *outParam = TEMP_REGISTER_1;
                return JIM_OK;
            }
            return dslParseRegisterArg(interp, index, arg, outParam);
        case PARAM_NUMBER_8:
        case PARAM_NUMBER_16:
        case PARAM_NUMBER_8_16:
        case PARAM_SKIP_BACK:
            return dslParseNumericArg(interp, index, arg, outParam);
        case PARAM_SHUFFLE:
            return dslParseNumericArg(interp, index, arg, outParam);
        case PARAM_MENU:
            return dslParseMenuArg(interp, arg, outParam);
        default:
            Jim_SetResultFormatted(interp,
                "parameter type %u not supported in scripts", paramMode);
            return JIM_ERR;
    }
}

/**
 * Convert register contents to Tcl string representation.
 * Handles: Real34, Complex34, String, ShortInteger, LongInteger
 */
int convertRegisterToString(calcRegister_t regist, char *buffer, size_t bufferSize) {
    if(bufferSize < 256) return JIM_ERR;

    switch(getRegisterDataType(regist)) {
        case dtReal34: {
            real34ToString(REGISTER_REAL34_DATA(regist), buffer);
            angularMode_t am = getRegisterAngularMode(regist);
            if(am != amNone) {
                size_t len = strlen(buffer);
                strncat(buffer, " ", bufferSize - len - 1);
                strncat(buffer, getAngularModeName(am), bufferSize - len - 2);
            }
            break;
        }
        case dtComplex34: {
            char realStr[512], imagStr[512];
            real34ToString(REGISTER_REAL34_DATA(regist), realStr);
            real34ToString(REGISTER_IMAG34_DATA(regist), imagStr);

            if(real34IsNegative(REGISTER_IMAG34_DATA(regist))) {
                snprintf(buffer, bufferSize, "%s - ix%s", realStr, imagStr + 1);
            } else {
                snprintf(buffer, bufferSize, "%s + ix%s", realStr, imagStr);
            }
            break;
        }
        case dtString: {
            stringToUtf8(REGISTER_STRING_DATA(regist), (uint8_t *)buffer);
            break;
        }
        case dtShortInteger: {
            uint64_t value = *(REGISTER_SHORT_INTEGER_DATA(regist));
            snprintf(buffer, bufferSize, "%08x-%08x (base %u)",
                     (unsigned int)(value >> 32), (unsigned int)(value & 0xffffffff),
                     getRegisterTag(regist));
            break;
        }
        case dtLongInteger: {
            longInteger_t lgInt;
            convertLongIntegerRegisterToLongInteger(regist, lgInt);
            longIntegerToAllocatedString(lgInt, buffer, bufferSize);
            longIntegerFree(lgInt);
            break;
        }
        case dtDate:
        case dtTime:
        default: {
            strncpy(buffer, "<unsupported>", bufferSize - 1);
            buffer[bufferSize - 1] = '\0';
            break;
        }
    }
    return JIM_OK;
}

/**
 * Parse a value string and store it in a temporary register.
 * Attempts to determine the appropriate data type by trying each type in order:
 * - First tries real34 (numeric) parsing
 * - If that fails, stores as string using utf8ToString conversion
 */
static bool_t isComplexNumber(const char *str) {
    size_t len = strlen(str);
    if(len < 5) {
        return FALSE;
    }
    
    const char *ixPos = NULL;
    for(size_t i = 0; i < len - 2; i++) {
        if((str[i] == 'i' || str[i] == 'I') && (str[i+1] == 'x' || str[i+1] == 'X')) {
            ixPos = str + i;
            break;
        }
    }
    
    if(!ixPos) {
        return FALSE;
    }
    
    const char *plusPos = NULL;
    const char *minusPos = NULL;
    
    for(size_t i = 0; i < len - 2; i++) {
        if(str[i] == '+' && (str[i+1] == ' ' || str[i+1] == '\t')) {
            plusPos = str + i;
        }
        else if(str[i] == '-' && (str[i+1] == ' ' || str[i+1] == '\t')) {
            minusPos = str + i;
        }
    }
    
    return (plusPos && plusPos < ixPos) || (minusPos && minusPos < ixPos);
}

static int parseComplexToTempRegister(Jim_Interp *interp, const char *valueArg) {
    char buffer[1024];
    strncpy(buffer, valueArg, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    size_t len = strlen(buffer);
    char *plusPos = NULL;
    char *minusPos = NULL;
    char *ixPos = NULL;
    
    for(size_t i = 0; i < len - 2; i++) {
        if((buffer[i] == 'i' || buffer[i] == 'I') && (buffer[i+1] == 'x' || buffer[i+1] == 'X')) {
            ixPos = buffer + i;
            break;
        }
    }
    
    if(!ixPos) {
        return JIM_ERR;
    }
    
    for(size_t i = 0; i < len - 2; i++) {
        if(buffer[i] == '+' && (buffer[i+1] == ' ' || buffer[i+1] == '\t')) {
            plusPos = buffer + i;
        }
        else if(buffer[i] == '-' && (buffer[i+1] == ' ' || buffer[i+1] == '\t')) {
            minusPos = buffer + i;
        }
    }
    
    char realPart[512], imagPart[512];
    
    if(plusPos && plusPos < ixPos) {
        *plusPos = '\0';
        strncpy(realPart, buffer, sizeof(realPart) - 1);
        realPart[sizeof(realPart) - 1] = '\0';
        
        char *startImag = plusPos + 2;
        while(*startImag == ' ' || *startImag == '\t') startImag++;
        strncpy(imagPart, startImag, sizeof(imagPart) - 1);
        imagPart[sizeof(imagPart) - 1] = '\0';
        
        size_t imagLen = strlen(imagPart);
        if(imagLen > 2 && (imagPart[imagLen-2] == 'i' || imagPart[imagLen-2] == 'I') &&
                          (imagPart[imagLen-1] == 'x' || imagPart[imagLen-1] == 'X')) {
            imagPart[imagLen-2] = '\0';
        }
    }
    else if(minusPos && minusPos < ixPos) {
        *minusPos = '\0';
        strncpy(realPart, buffer, sizeof(realPart) - 1);
        realPart[sizeof(realPart) - 1] = '\0';
        
        char *startImag = minusPos + 2;
        while(*startImag == ' ' || *startImag == '\t') startImag++;
        strncpy(imagPart, startImag, sizeof(imagPart) - 1);
        imagPart[sizeof(imagPart) - 1] = '\0';
        
        size_t imagLen = strlen(imagPart);
        if(imagLen > 2 && (imagPart[imagLen-2] == 'i' || imagPart[imagLen-2] == 'I') &&
                          (imagPart[imagLen-1] == 'x' || imagPart[imagLen-1] == 'X')) {
            imagPart[imagLen-2] = '\0';
        }
        
        if(strlen(imagPart) > 0 && imagPart[0] != '-') {
            size_t len = strlen(imagPart);
            memmove(imagPart + 1, imagPart, len + 1);
            imagPart[0] = '-';
        }
    }
    else {
        return JIM_ERR;
    }
    
    reallocateRegister(TEMP_REGISTER_1, dtComplex34, 0, amNone);
    
    real34_t realVal, imagVal;
    stringToReal34(realPart, &realVal);
    stringToReal34(imagPart, &imagVal);
    
    if(real34IsNaN(&realVal) || real34IsNaN(&imagVal)) {
        return JIM_ERR;
    }
    
    real34Copy(&realVal, REGISTER_REAL34_DATA(TEMP_REGISTER_1));
    real34Copy(&imagVal, REGISTER_IMAG34_DATA(TEMP_REGISTER_1));
    
    return JIM_OK;
}

int parseValueToTempRegister(Jim_Interp *interp, const char *valueArg) {
    // Try to parse as real number first
    reallocateRegister(TEMP_REGISTER_1, dtReal34, 0, amNone);
    
    // Parse the value
    stringToReal34(valueArg, REGISTER_REAL34_DATA(TEMP_REGISTER_1));
    
    if(!real34IsNaN(REGISTER_REAL34_DATA(TEMP_REGISTER_1))) {
        return JIM_OK;
    }
    
    if(isComplexNumber(valueArg)) {
        if(parseComplexToTempRegister(interp, valueArg) == JIM_OK) {
            return JIM_OK;
        }
    }
    
    // If parsing produced NaN and not complex, treat as string
    int stringLen = stringByteLength(valueArg) + 1;
    char *internalStr = malloc(stringLen);
    utf8ToString((const uint8_t *)valueArg, (uint8_t *)internalStr);
    
    reallocateRegister(TEMP_REGISTER_1, dtString, TO_BLOCKS(stringLen), amNone);
    xcopy(REGISTER_STRING_DATA(TEMP_REGISTER_1), internalStr, stringLen);
    
    free(internalStr);
    
    return JIM_OK;
}

#endif

