// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The C47 Authors

/**
 * \file dsl.c
 * \brief Jim Tcl-based DSL for calculator control
 */

#include "c47.h"
#include "c47-gtk.h"

#if defined(PC_BUILD)

#include <jim.h>
#include <stdio.h>

// Global state - declared in gtkGui.c
extern bool_t scriptingActive;

// External declaration for _ioFileNameOverride from hal/io.c
extern char _ioFileNameOverride[JIM_PATH_LEN];

// Jim interpreter instance
static Jim_Interp *jim_interp = NULL;

// ============================================================================
// DSL Command Implementations - Jim Tcl wrappers
// ============================================================================

/**
 * readp <filename> - Load a program from file (like READP menu command)
 */
static int readp(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *filename = (argc > 1) ? Jim_String(argv[1]) : "";
    
    if(strlen(filename) > 0) {
        strncpy(_ioFileNameOverride, filename, JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
    }
    
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
    
    reallyRunFunction(ITM_XEQ, (uint16_t)label);
    return JIM_OK;
}

/**
 * helper for press() to handle the common elements for each keypress
 */
static int pressOne(Jim_Interp *interp, const char *keyCode)
{
    if(strlen(keyCode) != 2 || !isdigit((unsigned char)keyCode[0]) || !isdigit((unsigned char)keyCode[1])) {
        Jim_SetResultFormatted(interp, "press: Invalid key code '%s' (expected 2 digits)", keyCode);
        return JIM_ERR;
    }
    
    GtkWidget *dummyWidget = NULL;
    btnClicked(dummyWidget, (gpointer)keyCode);
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
 * snap [<filename>] - Take a screenshot and save to file (like SNAP handler)
 */
static int snap(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if(argc > 1) {
        strncpy(_ioFileNameOverride, Jim_String(argv[1]), JIM_PATH_LEN - 1);
        _ioFileNameOverride[JIM_PATH_LEN - 1] = '\0';
    }
    
    fnSNAP(0);
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
    
    if(strcmp(scriptFile, "-") == 0) {
        ret = Jim_Eval(jim_interp, "eval [info source [stdin read] stdin 1]");
    } else {
        // Read from file
        ret = Jim_EvalFile(jim_interp, scriptFile);
    }
    
    // Handle errors - print to stderr with stack trace if available
    if(ret != JIM_OK) {
        const char *errorMsg = Jim_GetString(Jim_GetResult(jim_interp), NULL);
        fprintf(stderr, "%s\n", errorMsg);
        
        // Try to get and print stack trace
        Jim_Obj *traceObj = Jim_GetVariableStr(jim_interp, "errorInfo", 0);
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
    jim_interp = Jim_CreateInterp();
    
    // Register core Tcl commands
    Jim_RegisterCoreCommands(jim_interp);
    
    // Register DSL commands at global scope
    Jim_CreateCommand(jim_interp, "readp",  readp,  NULL, NULL);
    Jim_CreateCommand(jim_interp, "xeq",    xeq,    NULL, NULL);
    Jim_CreateCommand(jim_interp, "press",  press,  NULL, NULL);
    Jim_CreateCommand(jim_interp, "snap",   snap,   NULL, NULL);
    Jim_CreateCommand(jim_interp, "savest", savest, NULL, NULL);
}

// ============================================================================
// DSL Cleanup
// ============================================================================

void cleanupDSL(void) {
    if(jim_interp) {
        Jim_FreeInterp(jim_interp);
        jim_interp = NULL;
    }
}

#endif
