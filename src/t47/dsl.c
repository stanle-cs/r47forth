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
 * cmdByIndex index - Calls a built-in catalog function by its item index.
 * This binds CAT FCNS to Tcl commands.
 */
static int cmdByIndex(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    int index = (int)(intptr_t)Jim_CmdPrivData(interp);
    printf("Calling catalog function %s, index %d\n",
        indexOfItems[index].itemCatalogName, index);
    runFunction(index);
    return JIM_OK;
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
        Jim_CreateCommand(interp, cmdName, cmdByIndex, idx, NULL);
    }
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
    } else {
        char internalName[64];
        static const size_t max = sizeof(internalName)/2;
        const char *fnName = (argc > 1) ? Jim_String(argv[1]) : "";
        if(strlen(fnName) >= max) {
            Jim_SetResultFormatted(interp, "catfn: '%s' exceeds max length %d",
                    fnName, max);
            return JIM_ERR;
        }
        utf8ToString((const uint8_t *)fnName, internalName);
        for(int i = 0; i < LAST_ITEM; ++i) {
            item_t item = indexOfItems[i];
            const char* catName = item.itemCatalogName;
            if((item.status & CAT_STATUS) == CAT_FNCT &&
                    compareString(internalName, catName, CMP_NAME) == 0) { //change here to slacken the character check for commands: CMP_CLEANED_STRING_ONLY
                runFunction(i);
                return JIM_OK;
            }
        }
        Jim_SetResultFormatted(interp, "catfn: '%s' not in the function catalog", fnName);
        return JIM_ERR;
    }
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
        Jim_SetResultFormatted(interp, "xeq: '%s' not a known label", labelName);
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


static int push(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc < 2) {
        Jim_SetResultString(interp, "push: missing string argument", -1);
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
            Jim_SetResultFormatted(interp, "push: invalid character '%c' (expected 0-9, . , - e E or space)", *p);
            return JIM_ERR;
        }
        addItemToNimBuffer(item);
        refreshRegisterLine(REGISTER_X);
    }
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
    Jim_CreateCommand(interp, "press",  press,  NULL, NULL);
    Jim_CreateCommand(interp, "push",   push,   NULL, NULL);
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
            const char* catName = item.itemCatalogName;
            const char* smName  = item.itemSoftmenuName;
            void* idx = (void*)(intptr_t)i;
            if((item.status & CAT_STATUS) == CAT_FNCT) {
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
