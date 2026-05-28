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
// DSL Command Implementations - Jim Tcl wrappers
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
 * xeq <labelname> - Execute a label (like XEQ key)
 */
static int xeq(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *labelName = (argc > 1) ? Jim_String(argv[1]) : "";
    calcRegister_t label = findNamedLabel(labelName);
    
    if(label == INVALID_VARIABLE) {
        Jim_SetResultFormatted(interp, "xeq: Label '%s' not found", labelName);
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
    }

    Jim_SetResultFormatted(interp, "press: Invalid key code '%s' (expected single char, Enter/Return, or 2 digits)", keyCode);
    return JIM_ERR;
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
 * snap [<basename>] - Wrap SNAP, producing basename.bmp and
 *                     basename.REGS.TSV output files.
 */
static int snap(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        const char *baseName = Jim_String(argv[1]);
        char bmpFileName[JIM_PATH_LEN];

        snprintf(bmpFileName, sizeof(bmpFileName), "%s.bmp", baseName);
        strncpy(_ioFileNameOverride, bmpFileName, JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';

        snprintf(filename_csv, FILENAMELEN, "%s.REGS.TSV", baseName);
        
        /* Suppress default timestamp naming in filename_csv */
        mem__32 = getUptimeMs();
        cancelFilename = false;
    }
    
    fnSNAP(0);
    if(argc > 1) cancelFilename = true;

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
    Jim_CreateCommand(interp, "readp",  readp,  NULL, NULL);
    Jim_CreateCommand(interp, "xeq",    xeq,    NULL, NULL);
    Jim_CreateCommand(interp, "press",  press,  NULL, NULL);
    Jim_CreateCommand(interp, "snap",   snap,   NULL, NULL);
    Jim_CreateCommand(interp, "savest", savest, NULL, NULL);
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
