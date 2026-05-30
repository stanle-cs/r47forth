// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The C47 Authors

/**
 * \file dsl.c
 * \brief Jim Tcl-based DSL for calculator control
 */

#include "c47.h"
#include "c47-gtk.h"
#include "value.h"

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

// =====================================================================
// Helper functions not worth extracting to a separate module
// =====================================================================

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
        printf("Calling argless catalog function %s, index %d\n",
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
    const char *argstr = Jim_String(argArgv[0]);
    if(dslParseParam(interp, index, argstr, &param) != JIM_OK) {
        return JIM_ERR;
    }
    printf("Calling catalog function %s(%s), index %d\n",
        item.itemCatalogName, argstr, index);
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
        for(int i = 0; cmdName[i]; ++i) {
            cmdName[i] = tolower((unsigned char)cmdName[i]);
        }
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

// =====================================================================
// DSL command implementations
// =====================================================================

/*
 * flag <name> - Get flag state (1 or 0)
 * flag <name> <value> - Set flag (1=set, 0=clear), return new state
 */
static int flag(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "flag: missing flag argument", -1);
        return JIM_ERR;
    }

    const char *flagArg = Jim_String(argv[1]);
    uint16_t param;

    if(dslParseFlagArg(interp, flagArg, &param) != JIM_OK) {
        return JIM_ERR;
    }
    int32_t flagNum = (int32_t)param;

    if(argc == 2) {
        char resultStr[32];
        snprintf(resultStr, sizeof(resultStr), "%d", (getSystemFlag(flagNum)) ? 1 : 0);
        Jim_SetResultString(interp, resultStr, -1);
        return JIM_OK;
    }

    if(argc != 3) {
        Jim_SetResultFormatted(interp, "flag: wrong # args: expected 1 or 2, got %d", argc - 1);
        return JIM_ERR;
    }

    const char *valueArg = Jim_String(argv[2]);
    long newValue;

    if(valueArg[0] == '\0' || valueArg[strspn(valueArg, "0123456789")] != '\0') {
        Jim_SetResultFormatted(interp, "flag: expected numeric value, got '%s'", valueArg);
        return JIM_ERR;
    }

    newValue = strtol(valueArg, NULL, 10);

    if(newValue != 0 && newValue != 1) {
        Jim_SetResultFormatted(interp, "flag: value must be 0 or 1, got %ld", newValue);
        return JIM_ERR;
    }

    if(newValue == 1) {
        setSystemFlag(flagNum);
    } else {
        clearSystemFlag(flagNum);
    }

    char resultStr[32];
    snprintf(resultStr, sizeof(resultStr), "%d", (getSystemFlag(flagNum)) ? 1 : 0);
    Jim_SetResultString(interp, resultStr, -1);
    return JIM_OK;
}

/**
 * reg <name> <value> - Set register, return new value
 */
static int reg(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "reg: missing register argument", -1);
        return JIM_ERR;
    }

    const char *regArg = Jim_String(argv[1]);
    uint16_t param;

    if(dslParseRegisterArg(interp, ITM_RCL, regArg, &param) != JIM_OK) {
        return JIM_ERR;
    }
    calcRegister_t regist = (calcRegister_t)param;

    if(!regInRange(regist)) {
        Jim_SetResultFormatted(interp, "invalid register: '%s'", regArg);
        return JIM_ERR;
    }

    if(argc == 2) {
        char buffer[1024];
        convertRegisterToString(regist, buffer, sizeof(buffer));
        Jim_SetResultString(interp, buffer, -1);
        return JIM_OK;
    }

    if(argc != 3) {
        Jim_SetResultFormatted(interp, "reg: wrong # args: expected 1 or 2, got %d", argc - 1);
        return JIM_ERR;
    }

    const char *valueArg = Jim_String(argv[2]);

    // Parse value string into temporary register
    if(parseValueToTempRegister(interp, valueArg) != JIM_OK) {
        return JIM_ERR;
    }

    // Copy from temp to target register
    copySourceRegisterToDestRegister(TEMP_REGISTER_1, regist);

    char buffer[1024];
    convertRegisterToString(regist, buffer, sizeof(buffer));
    Jim_SetResultString(interp, buffer, -1);
    return JIM_OK;
}

/**
 * var <name> - Get variable contents
 * var <name> <value> - Set variable, return new value
 */
static int var(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "var: missing variable argument", -1);
        return JIM_ERR;
    }

    const char *varArg = Jim_String(argv[1]);
    uint16_t param;

    if(dslParseRegisterArg(interp, ITM_INPUT, varArg, &param) != JIM_OK) {
        return JIM_ERR;
    }
    calcRegister_t regist = (calcRegister_t)param;

    if(!regInRange(regist)) {
        Jim_SetResultFormatted(interp, "invalid variable: '%s'", varArg);
        return JIM_ERR;
    }

    if(argc == 2) {
        char buffer[1024];
        convertRegisterToString(regist, buffer, sizeof(buffer));
        Jim_SetResultString(interp, buffer, -1);
        return JIM_OK;
    }

    if(argc != 3) {
        Jim_SetResultFormatted(interp, "var: wrong # args: expected 1 or 2, got %d", argc - 1);
        return JIM_ERR;
    }

    const char *valueArg = Jim_String(argv[2]);

    // Parse value string into temporary register
    if(parseValueToTempRegister(interp, valueArg) != JIM_OK) {
        return JIM_ERR;
    }

    // Copy from temp to target register
    copySourceRegisterToDestRegister(TEMP_REGISTER_1, regist);

    char buffer[1024];
    convertRegisterToString(regist, buffer, sizeof(buffer));
    Jim_SetResultString(interp, buffer, -1);
    return JIM_OK;
}

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
    if(argc < 2) {
        Jim_SetResultString(interp, "xeq: missing label name", -1);
        return JIM_ERR;
    }
    const char *labelName = Jim_String(argv[1]);
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

// =====================================================================
// DSL Execution - Reads a line at a time from the script and passes it
// to Jim Tcl for evaluation.
// =====================================================================

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

// =====================================================================
// DSL initialization
// =====================================================================

void initDSL(void) {
    scriptingActive = TRUE;
    
    // Create Jim interpreter
    Jim_Interp *interp = g_dsl_interpreter = Jim_CreateInterp();
    
    // Register core Tcl commands
    Jim_RegisterCoreCommands(interp);
    Jim_InitStaticExtensions(interp);
    
    // Register DSL commands at global scope
    Jim_CreateCommand(interp, "catfn",  catfn,  NULL, NULL);
    Jim_CreateCommand(interp, "flag",   flag,   NULL, NULL);
    Jim_CreateCommand(interp, "loadst", loadst, NULL, NULL);
    Jim_CreateCommand(interp, "nim",    nim,    NULL, NULL);
    Jim_CreateCommand(interp, "press",  press,  NULL, NULL);
    Jim_CreateCommand(interp, "reg",    reg,    NULL, NULL);
    Jim_CreateCommand(interp, "readp",  readp,  NULL, NULL);
    Jim_CreateCommand(interp, "savest", savest, NULL, NULL);
    Jim_CreateCommand(interp, "snap",   snap,   NULL, NULL);
    Jim_CreateCommand(interp, "tsvfn",  tsvfn,  NULL, NULL);
    Jim_CreateCommand(interp, "var",    var,    NULL, NULL);
    Jim_CreateCommand(interp, "xeq",    xeq,    NULL, NULL);
    
    // Register all 🟧 CAT FCNS entries as commands, too
    {
        char cmdName[64];
        for(int i = 0; i < LAST_ITEM; ++i) {
            switch(i) {
                case ITM_LOADST: 
                case ITM_READP: 
                case ITM_SAVEST: 
                case ITM_SNAP: 
                case ITM_XEQ: 
                    continue; // skip op that shadow a command above
            }
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

// =====================================================================
// DSL cleanup
// =====================================================================

void cleanupDSL(void) {
    if(g_dsl_interpreter) {
        Jim_FreeInterp(g_dsl_interpreter);
        g_dsl_interpreter = NULL;
    }
}

#endif
