Begin your reply with the line `MODEL: <your exact model name>` before
anything else.

## Subject

A personal hobby project: a drawing package (program-graphics) built as an
external package over the open-source C47/R47 firmware for a DM42-class
pocket calculator. A user program draws on the screen, and the new
"canvas view" (calcMode 21) keeps the drawing on the screen after the
program stops. Single-user handheld. No network stack, no untrusted input,
no privilege boundary. The worst outcome of any bug here is that the
calculator reboots and the owner loses the program they were typing.

Audit for FUNCTIONAL correctness: wrong answers, lost work, stuck states,
crashes. A finding whose impact statement needs an attacker is not a
finding. Report findings, not fixes.

## Orientation

- `calcMode` is a global uint8. Values 0 to 18 are upstream modes (CM_NORMAL 0, CM_AIM 1, CM_NIM 2, CM_PEM 3, CM_REGISTER_BROWSER 5, CM_FLAG_BROWSER 6, CM_FONT_BROWSER 7, CM_BUG_ON_SCREEN 10, CM_TIMER 14, CM_GRAPH 15, CM_ASN_BROWSER 17, CM_LISTXY 18). Values 19 to 23 are reserved for packages: undo-history uses 19, pretty-print-extra uses 20, this package uses 21 (`CM_GRAPHICS_CANVAS`).
- The screen is 400 by 240 pixels. Rows 0 to 19 are the status bar. Rows 20 to 170 hold the four register lines. Rows 171 to 239 hold the softmenu (three rows of 23). `PG_TOP_ROW` is 20 and `PG_REGISTER_BOTTOM_ROW` is 170.
- `PVIEW n` is a program step with a parameter n. The item table restricts the typed parameter to 2..6 (`(2 << TAM_MAX_BITS) | 6`); `fnPview` accepts only 2 and 6 and raises `ERROR_OUT_OF_RANGE` for the rest through `displayCalcErrorMessage`, which sets `lastErrorCode` and, inside a running program, stops the program (upstream behaviour for any error).
- `screenUpdatingMode` is a bit mask. `SCRUPD_AUTO` is 0. The MANUAL bits stop the normal repaint of the stack, the menu, the status bar, and the shift status. `_selectiveClearScreen` and `_refreshNormalScreen` consult them. The run loop of a program sets `screenUpdatingMode = SCRUPD_AUTO | SCRUPD_SKIP_STATUSBAR_ONE_TIME` after every step (lblGtoXeq.c:997-998), so the MANUAL bits that `fnPview` sets do not survive the next step. That is intended: while `calcMode` is 21, `refreshScreen` takes the package's case, which paints only the status bar and the softmenu.
- `refreshScreen(source)` dispatches on `calcMode` in a switch. Every upstream full-screen view has a case. The `default` arm paints nothing. `_refreshNormalScreen()` is the only path that clears and repaints the register lines.
- `screenHoldsDrawnPixels` is an upstream flag set by CLLCD, PIXEL, POINT, AGRAPH. `refreshScreen` clears it on entry (screen.c:6066). Its only reader is SNAP, which skips its own pre-refresh when the flag is set.
- `showSoftmenuCurrentPart()` first calls `clearScreenOld(false, false, true)`, which clears rows 171 to 239, then paints the current softmenu. With an empty softmenu stack it still clears the band.
- `refreshStatusBar()` paints rows 0 to 19 only.
- Upstream assigns `calcMode = CM_NORMAL;` directly at 26 sites in 8 files (store.c, screen.c, plotstat.c, calcMode.c, ui/tam.c, keyboard.c, solver/graph.c, c47Extensions/graphs.c), and `calcModeNormal()` (body below) has 18 callers. None of them knows about this package. If one of them runs while the view is open, `calcMode` becomes CM_NORMAL and `canvas.region` stays 2 or 6. `pgCloseView` returns early when `calcMode != 21`. `fnPview` records `prevCalcMode` only when `calcMode != 21`. `fnErase` opens the view when `calcMode != 21`.
- A program runs through `runProgram()` (lblGtoXeq.c). The tail after the `stopProgram:` label is below. `refreshScreen(4)` runs there when `screenUpdatingMode == SCRUPD_AUTO`. The items `STOP`, `RTN`, `END`, `RTNP1` set `screenUpdatingMode = SCRUPD_AUTO` at subroutine level 0 (items.c excerpt below).
- Keys have two paths. Direct keys go through `processKeyAction(item)` at the press; the guard arm below is in its `default:` item case, in an if-chain after the `tam.mode` arm and before the `SNAP` arm. EXIT, ENTER, BACKSPACE, UP, DOWN and .d have their own `case` earlier in the same switch and do not reach the guard arm. Those keys run their key function (`fnKeyExit`, `fnKeyEnter`, ...) on the key RELEASE through the item function (`runFunction(item)`), not on the press. Softkeys go through three other functions (`btnFnPressed`, `btnFnReleased`, `executeFunction`); each has one gate line, quoted below, that skips the normal handling when `calcMode >= 19`.
- The key resolution chain (`determineItem`) maps a physical key to an item by `calcMode`. The range arm below is the package's arm. See the forth-core bullet below for the combined build.
- `showFunctionNameItem` is the item that the release phase runs after the press phase showed its name. The CM_NORMAL arm below sets it to `ITM_RS` at the release when the pressed key was R/S; the package's arm does the same for calcMode 21. `ITM_RS`'s item function is `fnRunProgram`.
- `temporaryInformation` values: `TI_NO_INFO` 0, `TI_VIEW_REGISTER` set by VIEW/AVIEW (display.c:3989 below). The register line painter shows the viewed register while it is set (see the `_refreshNormalScreen` bullet).
- Documented limits, do not report: the canvas does not survive sleep or power off; region code 1 is unsupported; VIEW and AVIEW inside the view show nothing; the DM42 DMA refresh path is untested; text and 2D shapes come in a later stage.
- Bodies not included, with their effect (each is upstream, unchanged): `refreshScreen(source)` is the dispatcher (screen.c:6065), a `switch(calcMode)` whose `default` arm paints nothing; `_refreshNormalScreen()` (screen.c:5853) is the register-line repaint and is reached only from the CM_NORMAL group of cases (screen.c:6187-6222); `refreshStatusBar()` (statusBar.c:779) paints rows 0 to 19 only; `showSoftmenuCurrentPart()` (softmenus.c:3093) calls `clearScreenOld(false, false, true)` at softmenus.c:3120, which clears rows 171 to 239, then paints the current menu; `reallyRunFunction(func, param)` (items.c:243) is the item dispatcher and the items.c excerpt is its post-dispatch block.
- The register line painter runs only inside `_refreshNormalScreen` (screen.c:5914-5953). The calcMode 21 case above does not call it, so the viewed register of VIEW is not painted while the view is open.
- The final `else` of the key resolution chain shows a bug screen (upstream keyboard.c:1691-1694, the four lines after the range arm above). In a combined build with forth-core, forth-core's own patch rewrites the condition above the arm to include `|| (calcMode >= 20 && calcMode <= 23)` (packages/forth-core/patches/010-keyboard.c.patch, hunk `-1674,11`), so the arm is reached only in a build without forth-core.
- Counts, with the command that produces them: `grep -rn 'calcMode *= *CM_NORMAL;' src/c47 --include=*.c | wc -l` gives 26; `grep -rn 'calcModeNormal()' src/c47 --include=*.c | wc -l` gives 18.
- Overrides: this package patches keyboard.c, screen.c, screen.h, items.c, items.h, defines.h, and softmenus.c, and adds pgmGraphics.c and pgmGraphics.h. Every excerpt marked "the subject" is a package line. The siblings undo-history and pretty-print-extra also patch keyboard.c (the same three gate lines, byte-identical, plus arms of their own for modes 19 and 20) and screen.c (their own refreshScreen cases). The excerpts marked "context, upstream" are byte-identical to upstream.
- How each mode named here is established. CM_NORMAL: the idle state, entered by `calcModeNormal()` and by the 26 direct assignments. CM_NIM: a digit or the radix key in CM_NORMAL starts number entry; ENTER or any function key closes it (keyboard.c `closeNim`). CM_AIM: the alpha key. CM_PEM: the PRGM key. CM_GRAPH and CM_PLOT_STAT: a plot command (`fnPlotSQ`, graphs.c:322; `fnPlotStat`, plotstat.c:1992). CM_TIMER: the timer application. CM_LISTXY: the LISTXY item from a plot. CM_BUG_ON_SCREEN: `displayBugScreen` (error.c:359). CM_GRAPHICS_CANVAS: `fnPview` and `fnErase` above, and nothing else.
- The design authority is design-docs/program-graphics/DESIGN.md §3 (the canvas view), §7 (composition), §8 (speed). The pins V1-V8 and K1-K3 in TESTING.md §4 were verified red-first.

## The code

```c
// packages/program-graphics/pgmGraphics.h (whole file, the subject)
// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.h
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#if !defined(PGMGRAPHICS_H)
  #define PGMGRAPHICS_H

  #include <stdint.h>

  #define PG_TOP_ROW              20   // first row below the status bar
  #define PG_REGISTER_BOTTOM_ROW 170   // last row of the register lines (region 2)
  #define PG_REGION_REGISTERS      2   // PVIEW 2: the register lines
  #define PG_REGION_FULL           6   // PVIEW 6: the register lines and the softmenu

  /** State of the canvas view. All fields are zero at boot (DESIGN.md §3.3). */
  typedef struct {
    uint8_t   region;        // 0 = view closed, else 2 or 6
    uint8_t   prevCalcMode;  // the calcMode to restore on EXIT
    uint8_t   drawMode;      // 0 set, 1 clear, 2 invert
    uint8_t   reserved;
    int16_t   clipX0, clipY0, clipX1, clipY1;   // screen coordinates, top-left origin, inclusive
    uint32_t  lastRefreshMs;
  } pgCanvas_t;

  // The commands and the view hooks are declared in screen.h, next to the
  // upstream PIXEL family, so that items.c, keyboard.c, and screen.c see
  // them through c47.h.

  // Test drivers for the headless suite (TESTING.md §1). Each writes its
  // failure count into X as a long integer.
  void pgTestSmoke   (uint16_t unusedButMandatoryParameter);
  void pgTestBaseline(uint16_t unusedButMandatoryParameter);
  void pgTestView    (uint16_t unusedButMandatoryParameter);
  void pgTestKeys    (uint16_t unusedButMandatoryParameter);

#endif // !PGMGRAPHICS_H

```

```c
// packages/program-graphics/pgmGraphics.c, the non-test part (whole functions, the subject)
// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file pgmGraphics.c
 * program-graphics package: drawing commands for user programs.
 * Contract and rules: design-docs/program-graphics/DESIGN.md.
 */
#include "pgmGraphics.h"
#include "c47.h"

static pgCanvas_t canvas;

// Sends the changed rows to the LCD and stamps the time (DESIGN.md §8.4).
static void pgRefreshNow(void) {
  canvas.lastRefreshMs = getUptimeMs();
  #if defined(DMCP_BUILD)
    lcd_refresh_dma();
  #else // !DMCP_BUILD
    lcd_refresh();
  #endif // DMCP_BUILD
}

// Sets the region, the clip rectangle, and clears the region to white.
static void pgSetRegion(uint8_t region) {
  canvas.region = region;
  canvas.clipX0 = 0;
  canvas.clipX1 = SCREEN_WIDTH - 1;
  canvas.clipY0 = PG_TOP_ROW;
  canvas.clipY1 = (region == PG_REGION_REGISTERS) ? PG_REGISTER_BOTTOM_ROW : SCREEN_HEIGHT - 1;
  lcd_fill_rect(0, PG_TOP_ROW, SCREEN_WIDTH, canvas.clipY1 - PG_TOP_ROW + 1, LCD_SET_VALUE);
}

// PVIEW n: opens the canvas view over region n (DESIGN.md §3.5).
void fnPview(uint16_t region) {
  if(region != PG_REGION_REGISTERS && region != PG_REGION_FULL) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }
  if(calcMode != CM_GRAPHICS_CANVAS) {
    canvas.prevCalcMode = calcMode;
  }
  pgSetRegion((uint8_t)region);
  calcMode = CM_GRAPHICS_CANVAS;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
  screenHoldsDrawnPixels = true;
  if(region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}

// ERASE: clears the canvas region. Opens the view over region 2 when closed (DESIGN.md §3.7).
void fnErase(uint16_t unusedButMandatoryParameter) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    fnPview(PG_REGION_REGISTERS);
    return;
  }
  pgSetRegion(canvas.region);
  if(canvas.region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
  pgRefreshNow();
}

// The refreshScreen case for the canvas view: the status bar stays live,
// the softmenu is painted for region 2, nothing else is painted.
void pgRefreshCanvasView(void) {
  refreshStatusBar();
  if(canvas.region == PG_REGION_REGISTERS) {
    showSoftmenuCurrentPart();
  }
}

// EXIT in the canvas view: restore the previous mode and repaint (DESIGN.md §3.6).
void pgCloseView(void) {
  if(calcMode != CM_GRAPHICS_CANVAS) {
    return;
  }
  calcMode = canvas.prevCalcMode;
  canvas.region = 0;
  temporaryInformation = TI_NO_INFO;
  screenUpdatingMode = SCRUPD_AUTO;
  refreshScreen(197);
}
```

```c
// packages/program-graphics/screen.c, the refreshScreen case (the subject), with two neighbours as context

        _refreshNormalScreen();
        break;

      case CM_GRAPHICS_CANVAS:   // program-graphics package: the canvas keeps its pixels
        last_CM = calcMode;
        pgRefreshCanvasView();
        force_refresh(force);
        break;

```

```c
// packages/program-graphics/keyboard.c, fnKeyExit: the package's case (the subject) ...

      case CM_GRAPHICS_CANVAS: {   // program-graphics package
        pgCloseView();
        break;
      }


// ... and the tail of fnKeyExit after its switch (context, byte-identical to upstream).
// This tail is a deliberate fragment: it shows what runs after the package's case breaks out of the switch.
// <!-- lint: allow-imbalance -->
            assignLeaveAlpha();
          }
        }
        else {
          popSoftmenu();
          if(previousCalcMode == CM_AIM) { //JM
            stayInAIM();                   //JM
          }                                //JM
        }
        break;
      }

      default: {
        sprintf(errorMessage, commonBugScreenMessages[bugMsgCalcModeWhileProcKey], "fnKeyExit", calcMode, "EXIT");
        displayBugScreen(errorMessage);
      }
    }

    last_CM = calcMode; //ignore this method of prioritising refreshes. This method is sunsetting.
    screenUpdatingMode &= ~SCRUPD_MANUAL_STATUSBAR;
//    refreshScreen(127);
    return;

undo_disabled:
    temporaryInformation = TI_UNDO_DISABLED;
//    screenUpdatingMode &= ~SCRUPD_MANUAL_MENU;
//    refreshScreen(128);
    return;
}
```

```c
// packages/program-graphics/keyboard.c, processKeyAction: the guard arm (the subject) between the tam.mode arm and the SNAP arm (context, byte-identical to upstream)
              addItemToBuffer(item + (ITM_a - ITM_A));
              keyActionProcessed = true;
            }
            else if(item == ITM_DOWN_ARROW || item == ITM_UP_ARROW) {
              addItemToBuffer(item);
              keyActionProcessed = true;
            }
            break;
          }
          else if(tam.mode) {
            if(tam.alpha) {
              if(indexOfItems[item].func == addItemToBuffer || item < 0) {
                processAimInput(item); // sets keyActionProcessed
              }
              else {
                keyActionProcessed = true;
              }
            }
            else {
              #if defined(DMCP_BUILD)
                  wait_for_key_release(0);
                  key_pop();
              #endif //DMCP_BUILD
              addItemToBuffer(item);
              #if defined(DMCP_BUILD)
                  key_push(0);
              #endif //DMCP_BUILD
              keyActionProcessed = true;
            }
            break;
          }

          // program-graphics package: inside the canvas view R/S continues
          // the program and every other key does nothing. SNAP falls
          // through to its own arm below.
          else if(calcMode == CM_GRAPHICS_CANVAS && item != ITM_SNAP) {
            if(item == ITM_RS) {
              showFunctionNameItem = 0;
            }
            keyActionProcessed = true;
          }

          else if(item == ITM_SNAP) {
            switch(calcMode) { //place modes here which should not work with SNAP
              //case CM_REGISTER_BROWSER:
              //case CM_FLAG_BROWSER:
              //case CM_FONT_BROWSER:
                break;
              default: {
                runFunction(item);
                keyActionProcessed = true;
                break;
              }
            }
          }

          else {
```

```c
// packages/program-graphics/keyboard.c, the key release path: the CM_NORMAL R/S arm (context, upstream) and the package's arm (the subject)
      //printf("release: showFunctionNameItem=%i calcMode=%i lastItem = %i keyActionProcessed=%i showFunctionNameItem=%i releaseOverride=%i tam.mode=%i tamBuffer=%s tamBuffer[0]=%u\n", showFunctionNameItem, calcMode, lastItem, keyActionProcessed, showFunctionNameItem, releaseOverride, tam.mode, tamBuffer, tamBuffer[0]);

      screenUpdatingMode |= SCRUPD_MANUAL_MENU;
      screenUpdatingMode &= ~SCRUPD_SKIP_MENU_ONE_TIME;

      if(calcMode == CM_NORMAL && showFunctionNameItem == 0 && lastKeyItemDetermined == ITM_RS && !SHOWMODE && calcMode != CM_REGISTER_BROWSER) {
         showFunctionNameItem = ITM_RS;
        temporaryInformation = TI_NO_INFO;
        refreshRegisterLine(REGISTER_T);
      }
      // program-graphics package: R/S in the canvas view runs the program
      // without the register line paint of the arm above.
      if(calcMode == CM_GRAPHICS_CANVAS && showFunctionNameItem == 0 && lastKeyItemDetermined == ITM_RS) {
        showFunctionNameItem = ITM_RS;
        temporaryInformation = TI_NO_INFO;
      }

```

```c
// packages/program-graphics/keyboard.c, determineItem: the range arm (the subject, identical bytes in pretty-print-extra) before the final else
      if(calcMode == CM_REGISTER_BROWSER) {
        if(shiftF && key->primaryAim >= ITM_A && key->primaryAim <= ITM_Z) {
          result = key->primaryAim;
        }
      }
    }
    // Package browsers 20 to 23 resolve their own keys here (claims
    // registry; 19 has its own branch). The arm is separate because
    // forth-core rewrites the condition above, and two packages cannot
    // edit one line. pretty-print-extra and program-graphics carry this
    // arm byte for byte.
    else if(calcMode >= 20 && calcMode <= 23) {
      result = shiftF ? key->fShifted :
               shiftG ? key->gShifted :
                        key->primary;
    }
    else {
      displayBugScreen(bugScreenItemNotDetermined);
      result = 0;
    }
```

```c
// packages/program-graphics/keyboard.c, determineItem: the whole if-chain that maps a key to an item, from the shift comment to the final else
// (context, byte-identical to upstream except the range arm, which is the subject). A deliberate fragment of a 200-line function:
// the code before the chain computes key_no and handles the catalogs, and the code after it runs Check_MultiPresses and returns result.
// <!-- lint: allow-imbalance -->
      shiftKeyClearsError = true;                                                                                         //JM shifts
    }
    if(programRunStop == PGM_WAITING) {
      programRunStop = PGM_STOPPED;
    }
    lastErrorCode = 0;

    switch(shiftkey) {
      case KEY_fg:
        fg_processing_jm();
        break;
      case ITM_SHIFTf:
        shiftF = !shiftF;
        shiftG = false;
        break;
      case ITM_SHIFTg:
        shiftF = false;
        shiftG = !shiftG;
        break;
      default:;
    }
    lastshiftF = shiftF;
    lastshiftG = shiftG;

    if(temporaryInformation != TI_NO_INFO && !shiftG && !shiftF) {
      screenUpdatingMode &= ~(SCRUPD_MANUAL_SHIFT_STATUS | SCRUPD_MANUAL_STACK);
      temporaryInformation = TI_NO_INFO;
      refreshScreen(1201);
    }

    if(SHOWMODE || currentMenu() == -MNU_SHOW) {
      closeShowMenu();
    }

    showShiftState();
    refreshModeGui();
    screenUpdatingMode &= ~SCRUPD_MANUAL_SHIFT_STATUS;
  }



  static int16_t determineItem(const char *data) {
    delayCloseNim = false;
    int16_t result;
    const calcKey_t *key;

    dynamicMenuItem = -1;

    int8_t key_no = stringToKeyNumber(data);

                    #if defined(PC_BUILD)
                      char tmp[200];
                      sprintf(tmp, "^^^^^^^keyboard.c: determineitem: key_no: %d:", key_no);
                      jm_show_comment(tmp);
                    #endif //PC_BUILD

    //if(kbd_usr[36].primaryTam == ITM_EXIT1) { //opposite keyboard V43 LT, 43S, V43 RT
    key = getSystemFlag(FLAG_USER) ? (kbd_usr + key_no) : (kbd_std + key_no);
    //}
    //else {
    //  key = getSystemFlag(FLAG_USER) && ((calcMode == CM_NORMAL) || (calcMode == CM_AIM) || (calcMode == CM_NIM) || (calcMode == CM_EIM) || (calcMode == CM_PLOT_STAT) || (calcMode == CM_GRAPH) || (calcMode == CM_LISTXY)) ? (kbd_usr + key_no) : (kbd_std + key_no);    //JM Added (calcMode == CM_NORMAL) to prevent user substitution in AIM and TAM
    //y

    fnTimerExec(TO_FN_EXEC);                                  //dr execute queued fn

                    #if defined(PC_BUILD)
                      sprintf(tmp, "^^^^^^^keyboard.c: determineitem: key_no: %u, key->primary1: %d:", key_no, key->primary);
                      jm_show_comment(tmp);
                    #endif //PC_BUILD

    if((key->primary != ITM_SHIFTf) && (key->primary != KEY_fg) && ( !SHOWMODE || !(
                           key->primary == ITM_RCL
                           || key->primary == ITM_RS
                           || key->primary == ITM_UP1
                           || key->primary == ITM_DOWN1
                           || (allowShowDigits && key->primary >= ITM_0 && key->primary <= ITM_9))
                         ) ) {
      showRegis = 9999;                                      //clear showmode register
    }

    int16_t ShiftOverride = 0;
    result = Norm_Key_00_item_in_layout;
    ShiftOverride = Check_Norm_Key_00_Assigned(&result, key_no);
    #if defined(PC_BUILD) && defined(VERBOSE_DETERMINEITEM)
      printf("**[DL]** determineItem = %d\n", result);
    #endif //VERBOSE_DETERMINEITEM


    if(ShiftOverride == 0) {                              //disable long and double press if Sigma+ is shift g
      Setup_MultiPresses( key->primary );
    }

                    #if defined(PC_BUILD)
                      sprintf(tmp, "^^^^^^^keyboard.c: determineitem: key->primary2: %d:", key->primary);
                      jm_show_comment(tmp);
                    #endif //PC_BUILD


    //before going into shift handling, send EXIT over to the key release
    if(SHOWMODE && (key->primary == KEY_fg || key->primary == ITM_SHIFTf)) {
      shiftF = true;
      shiftG = false;
      lastItem = key->primary;
      resetKeytimers();
      screenUpdatingMode = SCRUPD_MANUAL_STATUSBAR | SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
      return ITM_NOP;
    }

    //handle the shift button processing
    if((calcMode == CM_NORMAL || calcMode == CM_AIM || calcMode == CM_NIM  || calcMode == CM_MIM || calcMode == CM_EIM || calcMode == CM_PEM || calcMode == CM_PLOT_STAT || calcMode == CM_GRAPH || calcMode == CM_ASSIGN || calcMode == CM_ASN_BROWSER || calcMode == CM_REGISTER_BROWSER || calcMode == CM_FLAG_BROWSER || calcMode == CM_FONT_BROWSER || calcMode == CM_TIMER)) {
      if(((key->primary == ITM_SHIFTf || ShiftOverride == ITM_SHIFTf))) {
        commonShiftProcessing(ITM_SHIFTf);
        return ITM_NOP;
      }
      else if(((key->primary == ITM_SHIFTg || ShiftOverride == ITM_SHIFTg))) {
        commonShiftProcessing(ITM_SHIFTg);
        return ITM_NOP;
      }
      else if(((key->primary == KEY_fg     || ShiftOverride == KEY_fg))) {
        commonShiftProcessing(KEY_fg);
        return ITM_NOP;
      }
    }

    //handle the shifts in graph mode
    else if((key->primary == KEY_fg || key->primary == ITM_SHIFTf || key->primary == ITM_SHIFTg) && (calcMode == CM_PLOT_STAT || calcMode == CM_LISTXY)) {
      if(lastErrorCode != 0) {
        shiftKeyClearsError = true;
      }
      if(programRunStop == PGM_WAITING) {
        programRunStop = PGM_STOPPED;
      }
      lastErrorCode = 0;
      return ITM_NOP;
    }


                    #if defined(PC_BUILD)
                      sprintf(tmp, "^^^^^^^keyboard.c: determineitem: key->primary3: %d:", key->primary);
                      jm_show_comment(tmp);
                    #endif //PC_BUILD
                                                                                                                         //JM shifts
    if( !tam.mode && (calcMode == CM_NIM || calcMode == CM_NORMAL) && (lastIntegerBase >= 2 && getSystemFlag(FLAG_TOPHEX)) && (key_no >= 0 && key_no <= 5 )) {               //JMNIM vv Added direct A-F for hex entry
      result = shiftF ? key->fShifted :
               shiftG ? key->gShifted :
                        key->primaryAim;
      switch(result){
        case ITM_SHIFTf:
        case ITM_SHIFTg:
        case KEY_fg:
          result = ITM_NOP;
          break;
        default:break;
      }
      //printf(">>> ±±±§§§ keys key:%d result:%d Calmode:%d, nimbuffer:%s, lastbase:%d, nimnumberpart:%d\n", key_no, result, calcMode, nimBuffer, lastIntegerBase, nimNumberPart);
      Check_MultiPresses(&result, key_no);        //JM
      return result;
    }
    else if(calcMode == CM_AIM || (catalog && catalog != CATALOG_MVAR && calcMode != CM_NIM) || calcMode == CM_EIM || tam.alpha || (calcMode == CM_ASSIGN && (previousCalcMode == CM_AIM || previousCalcMode == CM_EIM)) || (calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA))) {
      result = shiftF ? key->fShiftedAim :
               shiftG ? key->gShiftedAim :
                        key->primaryAim;
      if(calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA)) {
        if(result == ITM_DOWN_ARROW || scrLock == NC_SUBSCRIPT) {
          nextChar = NC_SUBSCRIPT;
        }
        else if(result == ITM_UP_ARROW || scrLock == NC_SUPERSCRIPT) {
          nextChar = NC_SUPERSCRIPT;
        }
      }
      else if((result == ITM_COMMA || result == ITM_PERIOD) && (calcMode == CM_EIM || calcMode == CM_AIM) && getSystemFlag(FLAG_ALPHA) ) {
        switch((shiftG ? 2 : 0) + (getSystemFlag(FLAG_NUMLOCK) ? 1 : 0)) {                // gSHIFTED  numLock
        //case 0: result = key->primaryAim;break;           //                                   0        0      key->primaryAim
          case 1: result = RADIX34_MARK_DEC_ITM; break;     //                                   0        1      decimal
        //case 2: result = RADIX34_MARK_DEC_ITM; break;     //                                   2        0      decimal
          case 3: result = RADIX34_MARK_NOT_DEC_ITM; break; //                                   2        1      not the decimal
          default:;
        }
      }
      if((calcMode == CM_EIM) && (result == -MNU_AIMCATALOG)) {
        result = -MNU_EIMCATALOG;
      }
    }
    else if(tam.mode) {
      result = key->primaryTam; // No shifted function in TAM
    }
    else if(calcMode == CM_NORMAL || calcMode == CM_NIM || calcMode == CM_MIM || calcMode == CM_FONT_BROWSER || calcMode == CM_FLAG_BROWSER || calcMode == CM_ASN_BROWSER || calcMode == CM_REGISTER_BROWSER || calcMode == CM_BUG_ON_SCREEN || calcMode == CM_CONFIRMATION || calcMode == CM_PEM || GRAPHMODE || calcMode == CM_ASSIGN || calcMode == CM_TIMER  || calcMode == CM_LISTXY) {
      result = shiftF ? key->fShifted :
               shiftG ? key->gShifted :
                        key->primary;
      if(calcMode == CM_REGISTER_BROWSER) {
        if(shiftF && key->primaryAim >= ITM_A && key->primaryAim <= ITM_Z) {
          result = key->primaryAim;
        }
      }
    }
    // Package browsers 20 to 23 resolve their own keys here (claims
    // registry; 19 has its own branch). The arm is separate because
    // forth-core rewrites the condition above, and two packages cannot
    // edit one line. pretty-print-extra and program-graphics carry this
    // arm byte for byte.
    else if(calcMode >= 20 && calcMode <= 23) {
      result = shiftF ? key->fShifted :
               shiftG ? key->gShifted :
                        key->primary;
    }
    else {
      displayBugScreen(bugScreenItemNotDetermined);
      result = 0;
    }
```

```c
// packages/program-graphics/keyboard.c, the three softkey gate lines (the subject; byte-identical in undo-history and pretty-print-extra)
      else if(calcMode != CM_REGISTER_BROWSER && calcMode != CM_FLAG_BROWSER && calcMode != CM_ASN_BROWSER && calcMode != CM_FONT_BROWSER && calcMode < 19 /* package browsers 19-23, claims registry */) {
    if(calcMode != CM_REGISTER_BROWSER && calcMode != CM_FLAG_BROWSER && calcMode != CM_ASN_BROWSER && calcMode != CM_FONT_BROWSER && calcMode < 19 /* package browsers 19-23, claims registry */) {
    if(calcMode != CM_REGISTER_BROWSER && calcMode != CM_FLAG_BROWSER && calcMode != CM_ASN_BROWSER && calcMode != CM_FONT_BROWSER && calcMode < 19 /* package browsers 19-23, claims registry */) {
```

```c
// src/c47/programming/lblGtoXeq.c:994-1027, the end of runProgram (context, upstream)
    if(singleStep) {
      break;
    }
    screenUpdatingMode = SCRUPD_AUTO;
    screenUpdatingMode |= SCRUPD_SKIP_STATUSBAR_ONE_TIME;
  }

stopProgram:
  if(programRunStop == PGM_RUNNING && !nestedEngine) {
    programRunStop = PGM_STOPPED;
  }
  if(programRunStop != PGM_RUNNING) {
    entryStatus &= 0xfe;
  }
  if(!nestedEngine) {
    stackWatermarkAfterDispatch();                               // the run's own end, ahead of the statusbar repaint so that repaint's frames are not counted
    // Force a full statusbar repaint on every halt path and clear the one-time skip bit so the bar is current despite the cadence throttling in reallyRunFunction().
    // A program-set manual statusbar mode is left as is.
    forceSBupdate();
    screenUpdatingMode &= ~SCRUPD_SKIP_STATUSBAR_ONE_TIME;
  }
  if(!getSystemFlag(FLAG_INTING) && !getSystemFlag(FLAG_SOLVING) && !graphAccActive) {
    showHideHourGlass();
    if(temporaryInformation == TI_VIEW_REGISTER) {
      screenUpdatingMode |= SCRUPD_SKIP_STACK_ONE_TIME;
    }
    if(graphToRemainOnScreen && calcMode == CM_NORMAL) {
      calcMode = CM_GRAPH;
    }
    if(screenUpdatingMode == SCRUPD_AUTO && !singleStep) {
      refreshScreen(4);
    }
  }
  return;
```

```c
// src/c47/items.c:462-470, inside reallyRunFunction, after the item ran (context, upstream)
    }

    if(funcIsProgramStopControl) {
      screenUpdatingMode &= ~SCRUPD_MANUAL_STATUSBAR;
      if(currentSubroutineLevel == 0) {
        forceSBupdate();
        screenUpdatingMode = SCRUPD_AUTO;
      }
    }
```

```c
// src/c47/display.c:3986-3996, VIEW (context, upstream)
void _view(uint16_t regist) {
  if(regInRange(regist)) {
    currentViewRegister = regist;
    temporaryInformation = TI_VIEW_REGISTER;
    if(programRunStop == PGM_RUNNING) {
      screenUpdatingMode &= ~(SCRUPD_MANUAL_STATUSBAR | SCRUPD_SKIP_STATUSBAR_ONE_TIME);
      refreshScreen(151);
//      temporaryInformation = TI_NO_INFO;  //JM removed to signal to STOP, so that STOP does not clear the screen after VIEW
    }
  }
}
```

```c
// src/c47/programming/input.c:281-294, the end of PAUSE (context, upstream)
    if(programRunStop == PGM_WAITING) {
      previousProgramRunStop = PGM_WAITING;
    }
    programRunStop = previousProgramRunStop;
    if(programRunStop != PGM_RUNNING) {
      screenUpdatingMode &= ~SCRUPD_MANUAL_STATUSBAR;
      refreshScreen(13);
      #if defined(DMCP_BUILD)
        lcd_refresh();
      #else // !DMCP_BUILD
        refreshLcd(NULL);
      #endif // DMCP_BUILD
        }
}
```

```c
// src/c47/calcMode.c, calcModeNormal (context, upstream; 18 callers)
  void calcModeNormal(void) {
    #if defined(PC_BUILD)
      char tmp[200];
      sprintf(tmp, "^^^^### calcModeNormal");
      jm_show_comment(tmp);
    #endif // PC_BUILD
    calcMode = CM_NORMAL;
    if(softmenu[softmenuStack[0].softmenuId].menuItem == -MNU_ALPHA) {  //JM
      popSoftmenu();
    }                                                                   //JM

    if(softmenuStack[0].softmenuId == 1) { // MyAlpha
      softmenuStack[0].softmenuId = 0; // MyMenu
    }

    clearSystemFlag(FLAG_ALPHA);
    hideCursor();
    cursorEnabled = false;

    calcModeNormalGui();
  }


```

## Your task

You are auditing firmware code for bugs and design flaws. Report what you
find. Do not fix anything. The design intent is stated in the Orientation; code that contradicts the stated intent is a finding, and code that contradicts your expectations but matches the intent is not.

Bugs: wrong results, lost or corrupted state, states no path handles, an early return that skips something a later line assumes was done, a contract broken at one call site. Design flaws: two places that must agree with nothing forcing them to; state stored that could be derived; a guard whose conjuncts cannot all be falsified.

Your one question: **can a key act underneath the canvas view, or fail to reach the view when it must?** Concretely: (1) any direct key or softkey whose item runs while calcMode is 21, other than EXIT, R/S and SNAP; (2) any path by which EXIT does not close the view, or R/S does not continue a stopped program, or SNAP does not capture; (3) any path by which the key resolution chain shows a bug screen or returns a wrong item in calcMode 21, in the solo build (no forth-core) and in the combined build; (4) the shift keys f and g, the alpha key, the long-press timers, and the catalog key in this mode. You have no repository. The chain fragment carries every arm of the key resolution; the guard excerpt carries the package arm with its two neighbours; the release excerpt carries both R/S arms. If a body you need is missing, name the gap instead of guessing.

## Budget and output

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing — a named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where, the concrete reaching input
(keypress sequence, program steps, or call path), the observable consequence,
the violated contract quoted, and your confidence. Rank by what the defect
costs the owner. End with what you considered and deliberately did not
flag, and why.
