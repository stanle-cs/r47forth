// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The C47 Authors

/**
 * \file dsl.c
 * \brief Jim Tcl-based DSL for calculator control
 */

#include "c47.h"
#include "c47-gtk.h"

#if defined(PC_BUILD)

#include <ctype.h>
#include <jim.h>
#include <stdio.h>
#include <strings.h>

// Global state - declared in gtkGui.c
extern bool_t scriptingActive;
extern bool_t headlessMode;

// External declaration for _ioFileNameOverride from hal/io.c
extern char _ioFileNameOverride[JIM_PATH_LEN];

// Jim interpreter instance
static Jim_Interp *g_dsl_interpreter = NULL;

/** runCatalogFunctionByName result codes */
enum {
    CATFN_NOT_FOUND = 0,
    CATFN_OK        = 1,
    CATFN_ERROR     = -1
};

// ============================================================================
// Helper functions
// ============================================================================

/**
 * Wait until calculator program execution has fully returned control.
 *
 * DSL contract: xeq must not return while the execution engine is still
 * running (or internally paused as part of run-state handling).
 */
static void waitForEngineReturn(void)
{
    while(programRunStop == PGM_RUNNING ||
          programRunStop == PGM_PAUSED ||
          programRunStop == PGM_KEY_PRESSED_WHILE_PAUSED) {
        g_main_context_iteration(g_main_context_default(), TRUE);
    }

    // Drain remaining UI work queued by the final refresh path.
    refresh_gui();
}

/**
 * Resolve script-provided program filename in the same spirit as UI READP:
 * if a relative path does not exist as-is, also try PROGRAMS/<name>.
 */
static void setReadpFilenameOverride(const char *filename)
{
    if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
        strncpy(_ioFileNameOverride, filename, JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
        return;
    }

    if(strchr(filename, '/') == NULL) {
        char fallback[JIM_PATH_LEN];
        snprintf(fallback, sizeof(fallback), "%s/%s", PROGRAMS_DIR, filename);
        if(g_file_test(fallback, G_FILE_TEST_EXISTS)) {
            strncpy(_ioFileNameOverride, fallback, JIM_PATH_LEN - 1);
            _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
            return;
        }
    }

    // Let fnLoadProgram report the open/read failure with the original value.
    strncpy(_ioFileNameOverride, filename, JIM_PATH_LEN - 1);
    _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
}

/**
 * True when the catalog item expects a script-supplied argument (TAM in UI).
 */
static bool_t itemNeedsScriptArgs(int16_t index)
{
    uint16_t p = indexOfItems[index].param;
    return TM_VALUE <= p && p <= TM_CMP;
}

/**
 * Number of Tcl string args required after the command name, or -1 if unsupported.
 */
static int expectedScriptArgCount(int16_t index)
{
    switch(indexOfItems[index].status & PTP_STATUS) {
        case PTP_NONE:
            return 0;
        case PTP_REGISTER:
        case PTP_LABEL:
        case PTP_DECLARE_LABEL:
        case PTP_FLAG:
        case PTP_COMPARE:
        case PTP_NUMBER_8:
        case PTP_NUMBER_16:
        case PTP_NUMBER_8_16:
        case PTP_SKIP_BACK:
        case PTP_SHUFFLE:
        case PTP_MENU:
            return 1;
        default:
            return -1;
    }
}

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
static int dslParseRegisterArg(Jim_Interp *interp, int16_t op, const char *arg,
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
static int dslParseFlagArg(Jim_Interp *interp, const char *arg, uint16_t *outParam)
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
static int dslParseParam(Jim_Interp *interp, int16_t index, const char *arg,
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
 * Run one catalog function, with optional script args parsed per
 * items.c metadata.  The item index is validated, and the function
 * is called with the parsed parameter.
 */
static int runCatalogItem(Jim_Interp *interp, int16_t index, int argArgc,
        Jim_Obj *const *argArgv, const char *cmdName)
{
    if(index < 0 || index >= LAST_ITEM) {
        Jim_SetResultFormatted(interp, "%s: invalid catalog index %d", cmdName, index);
        return JIM_ERR;
    }

    if(index == ITM_DELITM) {
        Jim_SetResultFormatted(interp, "%s: DELITM not supported in scripts yet", cmdName);
        return JIM_ERR;
    }

    item_t item = indexOfItems[index];
    bool_t needsArgs = itemNeedsScriptArgs(index);
    int expected = needsArgs ? expectedScriptArgCount(index) : 0;

    if(needsArgs && expected < 0) {
        Jim_SetResultFormatted(interp, "%s: '%s' not scriptable",
            cmdName, item.itemCatalogName);
        return JIM_ERR;
    }

    if(!needsArgs) {
        if(argArgc != 0) {
            Jim_SetResultFormatted(interp, "%s: wrong # args: expected 0, got %d",
                cmdName, argArgc);
            return JIM_ERR;
        }
        printf("Calling catalog function %s, index %d\n",
            item.itemCatalogName, index);
        reallyRunFunction(index, item.param);
        return JIM_OK;
    }

    if(argArgc != expected) {
        Jim_SetResultFormatted(interp, "%s: wrong # args: expected %d, got %d",
            cmdName, expected, argArgc);
        return JIM_ERR;
    }

    uint16_t param;
    if(dslParseParam(interp, index, Jim_String(argArgv[0]), &param) != JIM_OK) {
        return JIM_ERR;
    }
    printf("Calling catalog function %s, index %d\n",
        item.itemCatalogName, index);
    reallyRunFunction(index, param);
    return JIM_OK;
}

/**
 * cmdCatalogFn index - Calls a built-in catalog function by its item index.
 * This binds CAT FCNS to Tcl commands.
 */
static int cmdCatalogFn(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int index = (int)(intptr_t)Jim_CmdPrivData(interp);
    return runCatalogItem(interp, (int16_t)index, argc - 1, argv + 1, argv[0] ?
        Jim_String(argv[0]) : "command");
}

/**
 * Check if a string is a valid Tcl identifier.
 */
static bool_t validTclIdentifier(const char *str) {
    if(str[0] == '\0') {
        return FALSE;
    }
    if(!isalpha(str[0]) && str[0] != '_') {
        return FALSE;
    }
    for(int i = 1; str[i]; ++i) {
        if(!isalnum(str[i]) && str[i] != '_') {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * Register a catalog function directly if its name happens to be a
 * valid Tcl identifier.
 */
static void registerCatFn(Jim_Interp *interp, const char *name, void *idx, char *cmdName, bool_t skipUtf8Identity)
{
    if(!name || !name[0]) {
        return;
    }
    stringToUtf8(name, (uint8_t*)cmdName);
    if(skipUtf8Identity && strcmp(name, cmdName) == 0) {
        return;
    }
    if(compareString(name, name, CMP_NAME) == 0 &&
            validTclIdentifier(cmdName)) {
        Jim_CreateCommand(interp, cmdName, cmdCatalogFn, idx, NULL);
    }
}

/**
 * Look up a catalog function by name and run it.  Core of catfn and
 * the by-name fallback in xeq.
 */
static int runCatalogFunctionByName(Jim_Interp *interp, const char *fnName,
        int argArgc, Jim_Obj *const *argArgv, const char *cmdName)
{
    char internalName[64];
    static const size_t max = sizeof(internalName)/2;

    if(strlen(fnName) >= max) {
        Jim_SetResultFormatted(interp, "%s: '%s' exceeds max length %d",
                cmdName, fnName, max);
        return JIM_ERR;
    }
    utf8ToString((const uint8_t *)fnName, internalName);
    for(int i = 0; i < LAST_ITEM; ++i) {
        item_t item = indexOfItems[i];
        const char* catName = item.itemCatalogName;
        if((item.status & CAT_STATUS) == CAT_FNCT &&
                compareString(internalName, catName, CMP_NAME) == 0) { //change here to slacken the character check for commands: CMP_CLEANED_STRING_ONLY
            return runCatalogItem(interp, (int16_t)i, argArgc, argArgv, cmdName) == JIM_OK ?
                CATFN_OK : CATFN_ERROR;
        }
    }
    return CATFN_NOT_FOUND;
}

// ============================================================================
// DSL Command Implementations - Jim Tcl wrappers
// ============================================================================


/**
 * catfn <name> - Calls a built-in catalog function by its name.
 * This is slow due to a linear lookup over a large table.  Prefer
 * the direct call by index by using the item name as a command,
 * when it is a legal Tcl identifier.
 */
static int catfn(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "catfn: missing function name", -1);
        return JIM_ERR;
    }
    const char *fnName = Jim_String(argv[1]);
    int found = runCatalogFunctionByName(interp, fnName, argc - 2, argv + 2, "catfn");
    if(found == CATFN_OK) {
        return JIM_OK;
    }
    if(found == CATFN_ERROR) {
        return JIM_ERR;
    }
    Jim_SetResultFormatted(interp, "catfn: '%s' not in the function catalog", fnName);
    return JIM_ERR;
}

/**
 * readp <filename> - Load a program from file (like READP menu command)
 */
static int readp(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *filename = (argc > 1) ? Jim_String(argv[1]) : "";
    if(strlen(filename) > 0) setReadpFilenameOverride(filename);
    fnLoadProgram(0);
    return JIM_OK;
}

/**
 * xeq <labelname> - Execute a label, emulating the XEQ key action
 */
static int xeq(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *labelName = (argc > 1) ? Jim_String(argv[1]) : "";
    calcRegister_t label = findNamedLabel(labelName);

    if(label == INVALID_VARIABLE) {
        int found = runCatalogFunctionByName(interp, labelName, argc - 2, argv + 2, "xeq");
        if(found == CATFN_OK) {
            return JIM_OK;
        }
        if(found == CATFN_ERROR) {
            return JIM_ERR;
        }
        Jim_SetResultFormatted(interp, "xeq: '%s' not found as label or function", labelName);
        return JIM_ERR;
    }

    dynamicMenuItem = -1;  // clear stale dynamic menu context
    reallyRunFunction(ITM_XEQ, (uint16_t)label);
    waitForEngineReturn();
    return JIM_OK;
}

/**
 * Helper for pressOne() to handle the common elements for each keypress.
 */
static int injectScriptKey(Jim_Interp *interp, const char *keyCode, uint32_t keyval)
{
    if(!scriptInjectGtkKey(keyval)) {
        Jim_SetResultFormatted(interp, "press: failed to inject key '%s'", keyCode);
        return JIM_ERR;
    }
    return JIM_OK;
}

/**
 * pressOne <keycode> - Fake a Gtk keypress event for a single
 * calculator keyboard key.  At the moment, it only understands
 * events corresponding to ASCII characters, plus a symbolic "ENTER".
 */
static int pressOne(Jim_Interp *interp, const char *keyCode)
{
    if(headlessMode) {
        Jim_SetResultString(interp, "press: unavailable in --headless mode", -1);
        return JIM_ERR;
    }

    if(strlen(keyCode) == 1) {
        /* Presume the ASCII value of the char == the GTK_KEY_* value */
        uint32_t keyval = (uint32_t)(unsigned char)keyCode[0];
        return injectScriptKey(interp, keyCode, keyval);
    } else if(strcasecmp(keyCode, "ENTER") == 0) {
        /* The Gtk "Return" keypress event code is ugly; allow the user
           to give this symbolic name instead. */
        return injectScriptKey(interp, keyCode, GDK_KEY_Return);
    } else if(strcasecmp(keyCode, "R/S") == 0) {
        /* to give this symbolic name instead. */
        return injectScriptKey(interp, keyCode, GDK_KEY_backslash);
    }

    Jim_SetResultFormatted(interp, "press: Invalid key code '%s' (expected single char, Enter/Return, or 2 digits)", keyCode);
    return JIM_ERR;
}

/**
 * nim <string> - Add a string to the NIM buffer
 */
static int nim(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "nim: missing string argument", -1);
        return JIM_ERR;
    }
    for(const char *p = Jim_String(argv[1]); *p != 0; p++) {
        int16_t item;
        if(*p >= '0' && *p <= '9') {
            item = ITM_0 + (*p - '0');
        }
        else if(*p == '.' || *p == ',') {
            item = ITM_PERIOD;
        }
        else if(*p == '-') {
            item = ITM_CHS;
        }
        else if(*p == 'e' || *p == 'E') {
            item = ITM_EXPONENT;
        }
        else if(*p == ' ') {
            continue;
        }
        else {
            Jim_SetResultFormatted(interp, "nim: invalid character '%c' (expected 0-9, . , - e E or space)", *p);
            return JIM_ERR;
        }
        addItemToNimBuffer(item);
        refreshRegisterLine(REGISTER_X);
    }
    closeNim();
    return JIM_OK;
}


/**
 * press <keycode> - Press a keyboard key (like pressing the button)
 */
static int press(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "press: missing key code argument", -1);
        return JIM_ERR;
    }
    
    if(Jim_IsList(argv[1])) {
        int listLen = Jim_ListLength(interp, argv[1]);
        for(int i = 0; i < listLen; i++) {
            Jim_Obj *elemObj;
            if(Jim_ListIndex(interp, argv[1], i, &elemObj, JIM_NONE) != JIM_OK) {
                return JIM_ERR;
            }
            const char *keyCode = Jim_String(elemObj);
            if(pressOne(interp, keyCode) != JIM_OK) {
                return JIM_ERR;
            }
        }
    } else {
        const char *keyCode = Jim_String(argv[1]);
        if(pressOne(interp, keyCode) != JIM_OK) {
            return JIM_ERR;
        }
    }
    
    return JIM_OK;
}

/**
 * loadst [<filename>] - Load state from file
 */
static int loadst(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        strncpy(_ioFileNameOverride, Jim_String(argv[1]), JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
    }
    
    fnLoad(LM_STATE_LOAD);
    return JIM_OK;
}

/**
 * savest [<filename>] - Save state to file
 */
static int savest(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        strncpy(_ioFileNameOverride, Jim_String(argv[1]), JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
    }
    
    fnSave(SM_STATE_SAVE);
    return JIM_OK;
}

/**
 * tsvfnSet <path> - Set the TSV file name override
 */
static void tsvfnSet(const char *path)
{
    strncpy(filename_csv, path, FILENAMELEN - 1);
    filename_csv[FILENAMELEN - 1] = '\0';
    mem__32 = getUptimeMs();
    cancelFilename = false;
    clearSystemFlag(FLAG_PRTACT);
    printf("Overrode TSV file name to %s\n", filename_csv);
}

/**
 * tsvfnClear - Clear the TSV file name override
 */
static void tsvfnClear(void)
{
    cancelFilename = true;
    filename_csv[0] = '\0';
    printf("Cleared TSV file name override\n");
}

/**
 * snap [<basename>] - Wrap SNAP, producing basename.bmp and
 *                     basename.REGS.TSV output files.
 */
static int snap(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        const char *baseName = Jim_String(argv[1]);
        char bmpFileName[JIM_PATH_LEN];
        char regsPath[FILENAMELEN];

        snprintf(bmpFileName, sizeof(bmpFileName), "%s.bmp", baseName);
        strncpy(_ioFileNameOverride, bmpFileName, JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';

        snprintf(regsPath, sizeof(regsPath), "%s.REGS.TSV", baseName);
        tsvfnSet(regsPath);
    }

    fnSNAP(0);
    if(argc > 1) tsvfnClear();

    return JIM_OK;
}

/**
 * tsvfn [<path>] - Set the TSV output file name.  Affects virtual
 * printing, stats, graphs…  With no argument: clear the override.
 */
static int tsvfn(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        tsvfnSet(Jim_String(argv[1]));
    } else {
        tsvfnClear();
    }
    return JIM_OK;
}

// ============================================================================
// DSL Execution - Reads a line at a time from the script and passes it
// to Jim Tcl for evaluation.
// ============================================================================

int executeScript(const char *scriptFile) {
    int ret;
    
    Jim_Interp *interp = g_dsl_interpreter;
    if(strcmp(scriptFile, "-") == 0) {
        ret = Jim_Eval(interp, 
            "package require aio;"
            "eval [info source [stdin read] stdin 1]");
    } else {
        // Read from file
        ret = Jim_EvalFile(interp, scriptFile);
    }
    
    // Handle errors - print to stderr with stack trace if available
    if(ret != JIM_OK) {
        const char *errorMsg = Jim_GetString(Jim_GetResult(interp), NULL);
        fprintf(stderr, "%s\n", errorMsg);
        
        // Try to get and print stack trace
        Jim_Obj *traceObj = Jim_GetVariableStr(interp, "errorInfo", 0);
        if(traceObj) {
            const char *trace = Jim_GetString(traceObj, NULL);
            fprintf(stderr, "%s\n", trace);
        }
    }
    
    return ret;
}

// ============================================================================
// DSL Initialization
// ============================================================================

void initDSL(void) {
    scriptingActive = TRUE;
    
    // Create Jim interpreter
    Jim_Interp *interp = g_dsl_interpreter = Jim_CreateInterp();
    
    // Register core Tcl commands
    Jim_RegisterCoreCommands(interp);
    Jim_InitStaticExtensions(interp);
    
    // Register DSL commands at global scope
    Jim_CreateCommand(interp, "catfn",  catfn,  NULL, NULL);
    Jim_CreateCommand(interp, "loadst", loadst, NULL, NULL);
    Jim_CreateCommand(interp, "nim",    nim,    NULL, NULL);
    Jim_CreateCommand(interp, "press",  press,  NULL, NULL);
    Jim_CreateCommand(interp, "readp",  readp,  NULL, NULL);
    Jim_CreateCommand(interp, "savest", savest, NULL, NULL);
    Jim_CreateCommand(interp, "snap",   snap,   NULL, NULL);
    Jim_CreateCommand(interp, "tsvfn",  tsvfn,  NULL, NULL);
    Jim_CreateCommand(interp, "xeq",    xeq,    NULL, NULL);
    
    // Register all 🟧 CAT FCNS entries as commands, too
    {
        char cmdName[64];
        for(int i = 0; i < LAST_ITEM; ++i) {
            item_t item = indexOfItems[i];
            if((item.status & CAT_STATUS) == CAT_FNCT) {
                void* idx = (void*)(intptr_t)i;
                const char* catName = item.itemCatalogName;
                const char* smName  = item.itemSoftmenuName;
                registerCatFn(interp, catName, idx, cmdName, FALSE);
                registerCatFn(interp, smName,  idx, cmdName, TRUE);
            }
        }
    }
}

// ============================================================================
// DSL Cleanup
// ============================================================================

void cleanupDSL(void) {
    if(g_dsl_interpreter) {
        Jim_FreeInterp(g_dsl_interpreter);
        g_dsl_interpreter = NULL;
    }
}

#endif
