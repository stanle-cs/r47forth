/*
 * forth_menu.c — Forth capture UI: the FWRD word picker, the
 * name-insertion helpers shared by the picker and the catalogs, and the
 * console's softmenu-frame ownership.  Nothing here is upstream logic;
 * the upstream patches are the call sites alone.
 */

#include "c47.h"
#include "forth_dict.h"
#include "forth_capture.h"
#include "forth_menu.h"

/* Name-slot comparator.  softmenus.c's own sortMenu is file-static, so
 * the picker carries its own — same comparison, same 15-byte slot
 * stride used by every other dynamic-menu name list. */
static int forthSortMenu(void const *a, void const *b) {
  return compareString(a, b, CMP_EXTENSIVE);
}

/* Insert name into the open capture line at T_cursorPos as its own
 * token: one trailing space always, plus one LEADING separator space
 * whenever the byte before the cursor is neither a space nor the line
 * start (token-boundary guard).  Same 256-byte/196-glyph cap as typing;
 * false = no room or capture not open. */
bool_t forthCapInsertName(const char *name)
{
  int32_t nameLen = stringByteLength(name);
  if(!forthCapIsOpen()) { return false; }
  int32_t bufLen = stringByteLength(aimBuffer);
  /* A name must land as its own token: without the leading separator the
   * direct picker/keys-mode paths glue digits to names ("42" + SIN ->
   * "42SIN ", an unresolvable token). */
  int32_t lead = (T_cursorPos > 0 && aimBuffer[T_cursorPos - 1] != ' ') ? 1 : 0;
  if(bufLen + nameLen + lead + 1 < 256 && stringGlyphLength(aimBuffer) + nameLen + lead + 1 <= 196) {
    xcopy(aimBuffer + T_cursorPos + nameLen + lead + 1, aimBuffer + T_cursorPos,
          stringByteLength(aimBuffer + T_cursorPos) + 1);
    if(lead) { aimBuffer[T_cursorPos] = ' '; }
    xcopy(aimBuffer + T_cursorPos + lead, name, nameLen);
    aimBuffer[T_cursorPos + lead + nameLen] = ' ';
    T_cursorPos += nameLen + lead + 1;
    return true;
  }
  return false;
}

/* Insert the currently selected dynamic-menu label (§9.6 P-H7). */
bool_t pickerInsertName(void)
{
  return forthCapInsertName(dynmenuGetLabel(dynamicMenuItem));
}

/* forthPickerGuard — guard for MNU_FORTH picker action in executeFunction (§9.6).
 * Returns true only when all Forth-capture conjuncts hold AND the current
 * softmenu is actually MNU_FORTH (menu-identity check, placed before any
 * dynamicSoftmenu[] indexing to prevent OOB access on wrong menu). */
bool_t forthPickerGuard(int16_t item)
{
  if(softmenu[softmenuStack[0].softmenuId].menuItem != -MNU_FORTH) return false;
  return ((calcMode == CM_PEM && getSystemFlag(FLAG_ALPHA) && tam.function == ITM_FORTH)
          || forthCapIsInteractive())
      && item == ITM_NOP
      && dynamicMenuItem >= 0
      && dynamicMenuItem < dynamicSoftmenu[softmenuStack[0].softmenuId].numItems;
}

/* MNU_FORTH content builder (§9.6): the UNION of the edited program's own
 * ': NAME' definitions before the cursor (text scan — no execution
 * needed), the interactive-scope dictionary, and the global dictionary,
 * each section sorted independently.
 *
 * Called from initVariableSoftmenu()'s MNU_FORTH arm, which has already
 * freed the previous menuContent; this function only allocates the new
 * one and sets numItems. */
void forthBuildWordPicker(int16_t menu)
{
  int16_t i, numberOfBytes;
  uint8_t *ptr;
  uint8_t *progStart = NULL;
  uint8_t *step;
  int16_t nNames = 0;
  /* Accepted names are copied into 15-byte slots in this same global
   * tmpString; TMP_STR_LENGTH/15 = 170 complete slots fit.  Policy:
   * truncate by scan order — once the cap is reached, stop RECORDING new
   * names but keep tokenizing normally, so the cap cannot desync the
   * tokenizer's position in the line.  Sorting applies only to the names
   * actually collected. */
  const int16_t forthPickerMaxNames = TMP_STR_LENGTH / 15;

  memset(tmpString, 0, TMP_STR_LENGTH);

  /* The program-scan section is suppressed for the interactive capture
   * and for CM_NORMAL/CM_ASSIGN (the catalog-tree surfaces): there
   * currentStep points at whatever the PEM cursor last was, so scanning
   * would list that program's definitions with false provenance, and a
   * normal-mode press of such a name resolves to nothing and errors.
   * Every other mode keeps the scan — the builder is deliberately
   * mode-blind everywhere else. */
  progStart = (forthCapIsInteractive()
               || calcMode == CM_NORMAL || calcMode == CM_ASSIGN)
                ? NULL : forthOwningProgramStart(currentStep);

  if (progStart) {
    step = progStart;
    int16_t stepCount = 0;
    while (step && step <= currentStep) {
      stepCount++;
      if (stepCount > FORTH_PICKER_MAX_SCAN_STEPS) break; /* §9.6 documented deviation */
      uint8_t *next = findNextStep(step);
      if (checkOpCodeOfStep(step, ITM_FORTH) && step[2] == STRING_LABEL_VARIABLE) {
        uint8_t len = step[3];
        if (len > 0 && step[4] != 0) { /* skip the §8.1 open placeholder (len=1, NUL) — not authored source */
          char line[256];
          xcopy(line, (const char *)(step + 4), len);
          line[len] = 0;
          /* clamp to C-string truth: an embedded NUL must never let the
           * byte-count out-run stringNextGlyph's string walk (hang). */
          len = (uint8_t)stringByteLength(line);
          int16_t pos = 0;
          char tok[FORTH_TOKEN_MAX + 1];

          while (pos < len && line[pos] == ' ')
            pos = stringNextGlyph(line, pos);
          while (pos < len) {
            int16_t start = pos;
            while (pos < len && line[pos] != ' ')
              pos = stringNextGlyph(line, pos);
            int16_t tokLen = pos - start;
            if (tokLen > FORTH_TOKEN_MAX) {
              while (pos < len && line[pos] == ' ')
                pos = stringNextGlyph(line, pos);
              continue;
            }
            xcopy(tok, line + start, tokLen);
            tok[tokLen] = 0;

            if (compareString(tok, ":", CMP_BINARY) == 0) {
              while (pos < len && line[pos] == ' ')
                pos = stringNextGlyph(line, pos);
              if (pos < len) {
                int16_t nameStart = pos;
                while (pos < len && line[pos] != ' ')
                  pos = stringNextGlyph(line, pos);
                int16_t nameLen = pos - nameStart;
                if (nameLen > 0 && nameLen <= 14) {
                  char nameBuf[15];
                  xcopy(nameBuf, line + nameStart, nameLen);
                  nameBuf[nameLen] = 0;
                  int16_t dup = 0;
                  for (int16_t d = 0; d < nNames; d++) {
                    if (compareString(tmpString + 15 * d, nameBuf, CMP_BINARY) == 0) {
                      dup = 1;
                      break;
                    }
                  }
                  if (!dup && nNames < forthPickerMaxNames) {
                    xcopy(tmpString + 15 * nNames, nameBuf, nameLen);
                    nNames++;
                  }
                }
              }
            }
            while (pos < len && line[pos] == ' ')
              pos = stringNextGlyph(line, pos);
          }
        }
      }
      if (!next || next <= step) break;
      step = next;
    }
  }

  if (nNames > 0) {
    qsort(tmpString, nNames, 15, forthSortMenu);
  }

  /* Section (b): interactive-scope dictionary words. */
  { uint16_t bi = 0;
    int16_t nB = 0;
    char slot[15];
    while (nNames < forthPickerMaxNames && forthDictBrowseName(bi, FORTH_OWNER_INTERACTIVE, slot)) {
      int16_t dup = 0;
      for (int16_t d = 0; d < nB; d++) {
        if (compareString(tmpString + 15 * (nNames - nB + d), slot, CMP_BINARY) == 0) {
          dup = 1;
          break;
        }
      }
      if (!dup) {
        xcopy(tmpString + 15 * nNames, slot, stringByteLength(slot));
        nNames++;
        nB++;
      }
      bi++;
    }
    if (nB > 0) {
      qsort(tmpString + 15 * (nNames - nB), nB, 15, forthSortMenu);
    }
  }

  /* Section (c): global dictionary words. */
  { uint16_t ci = 0;
    int16_t nC = 0;
    char slot[15];
    while (nNames < forthPickerMaxNames && forthGDictBrowseName(ci, slot)) {
      int16_t dup = 0;
      for (int16_t d = 0; d < nC; d++) {
        if (compareString(tmpString + 15 * (nNames - nC + d), slot, CMP_BINARY) == 0) {
          dup = 1;
          break;
        }
      }
      if (!dup) {
        xcopy(tmpString + 15 * nNames, slot, stringByteLength(slot));
        nNames++;
        nC++;
      }
      ci++;
    }
    if (nC > 0) {
      qsort(tmpString + 15 * (nNames - nC), nC, 15, forthSortMenu);
    }
  }

  numberOfBytes = 1;
  for (i = 0; i < nNames; i++) {
    numberOfBytes += stringByteLength(tmpString + 15 * i) + 1;
  }

  ptr = calloc(1, numberOfBytes);
  if (ptr == NULL) {
    /* Out of heap: leave the menu empty rather than writing through NULL.
     * An empty picker is a menu with no keys, which showSoftmenuCurrentPart
     * already renders (its numberOfItems == 0 arm blanks all six). */
    dynamicSoftmenu[menu].menuContent = NULL;
    dynamicSoftmenu[menu].numItems = 0;
    return;
  }
  dynamicSoftmenu[menu].menuContent = ptr;
  for (i = 0; i < nNames; i++) {
    int16_t len = stringByteLength(tmpString + 15 * i) + 1;
    xcopy(ptr, tmpString + 15 * i, len);
    ptr += len;
  }

  dynamicSoftmenu[menu].numItems = nNames;
}

/* ---- One owner for the console's row; ownership rides the FRAME ----
 *
 * While an interactive capture is open, the console owns AT MOST ONE
 * softmenu frame: FWRD in keys input, ALPHA in the alpha excursion.  This
 * is the ONLY function file that changes the console's row.
 *
 * A menu id is a value TWO DIFFERENT OWNERS can hold (the user's own FWRD
 * frame is indistinguishable from the console's by identity alone), so
 * ownership lives IN the frame: the frame the console relies on carries a
 * sentinel in its userMenuId — FRAME_STAMP for a frame the console
 * CREATED (the close rung pops it), BORROW_STAMP for the USER'S OWN row
 * the console is merely displaying (the close rung releases it) — and
 * every decision (retarget, restore, "is anything stacked above the
 * base", "pop what the open pushed") tests the stamps.
 *
 * THE INVARIANT: while an interactive capture is open the stack carries
 * AT MOST ONE BORROWED base and AT MOST ONE OWNED frame, and when both
 * exist the OWNED one is ABOVE the borrowed one.  Two registrations is
 * the normal, correct state of the alpha excursion opened over the user's
 * own FWRD row.  What must never happen is two OWNED frames, or a stamp
 * of either kind outliving its capture: forthCapClose() is the clearing
 * funnel, with forthCapAbandonSuspended() and the restore sanitizer as
 * the two paths that reach a closed capture without passing through it.
 *
 * The field is safe to borrow: it is meaningful only for -MNU_DYNAMIC
 * frames, native pushes write 0, real user-menu ids are >= 0, and the one
 * native mutator (removeUserMenuFromStack's re-number loop) only
 * decrements values GREATER than a non-negative threshold — a NEGATIVE
 * stamp is inert everywhere upstream.  Because the stamp is in the frame,
 * it shifts with every push/pop/dedup-lift and survives every capture
 * reopen and resume by construction.  Marking the BORROWED base is
 * load-bearing, not bookkeeping: an unmarked base would still match
 * pushSoftmenu's (softmenuId, userMenuId) dedup, so a native re-push of
 * the same menu could lift the user's base out from under the console or
 * early-return against it.
 *
 * The sub-mode swap RETARGETS slot 0 in place rather than popping and
 * pushing, because popSoftmenu() carries its own CM_AIM compensation that
 * re-pushes an alpha row and pushSoftmenu() dedups against a match
 * ANYWHERE in the array by lifting the stack over it.  Two deliberate
 * exceptions, both bounded:
 *
 *   - FOLD-BACK, FWRD only: when the console's own frame sits directly on
 *     a FWRD row that is not also console-created, the keys-mode swap POPS
 *     ours instead of retargeting — returning the user's frame at the
 *     user's page instead of keeping a duplicate above it.  FWRD only:
 *     nothing native ever pops a FWRD row, while folding back onto a
 *     user's ALPHA row would hand it straight to the next line's
 *     calcModeNormal(), which pops -MNU_ALPHA on sight.  Safe from
 *     popSoftmenu's CM_AIM compensation: that fires only when the pop
 *     reveals softmenuId 0/1, and the revealed row here is a real FWRD.
 *
 *   - HAND-ROLLED ALPHA acquisition: re-establishing the ALPHA surface
 *     never goes through showSoftmenu, whose dedup would LIFT a user's
 *     own ALPHA row to slot 0 — where the next calcModeNormal() destroys
 *     it — and whose early-return would leave the user's row where a
 *     console-created one was needed.  A fresh frame is shifted in above
 *     whatever is there; a user ALPHA row deeper stays SHIELDED beneath
 *     it.  ALPHA is a static menu, so no content build is skipped.  FWRD
 *     acquisition, by contrast, does use showSoftmenu (the picker content
 *     must be rebuilt) — if dedup hoists a user's own FWRD row, it is
 *     registered as BORROWED, never claimed. */
#define FORTH_CONSOLE_FRAME_STAMP  ((int16_t)-0x4643)   /* console-created  */
#define FORTH_CONSOLE_BORROW_STAMP ((int16_t)-0x4642)   /* user's, on loan  */

static int16_t _softmenuIndexOf(int16_t menuId) {
  int16_t m = 0;
  while(softmenu[m].menuItem != 0) {
    if(softmenu[m].menuItem == menuId) { return m; }
    m++;
  }
  return -1;
}

static bool_t _ownedAt(int i) {
  return softmenuStack[i].userMenuId == FORTH_CONSOLE_FRAME_STAMP;
}
static bool_t _stampedAt(int i) {
  return softmenuStack[i].userMenuId == FORTH_CONSOLE_FRAME_STAMP
      || softmenuStack[i].userMenuId == FORTH_CONSOLE_BORROW_STAMP;
}

bool_t forthConsoleOwnsSlot0(void) {
  return _ownedAt(0);
}

bool_t forthConsoleStampOnStack(void) {
  int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
    if(_stampedAt(i)) { return true; }
  }
  return false;
}

/* Register slot 0 as the console's surface frame.  `created` says whether
 * the open/acquire pushed it (OWNED: the close rung pops it) or it is the
 * user's own row (BORROWED: the close rung releases it).
 *
 * An OWNED registration is refused while an OWNED frame already exists —
 * two owned frames is the state the invariant forbids.  A BORROWED
 * registration is refused while ANY stamp exists: the borrow marks the
 * base, and the base is claimed once, at the open. */
void forthConsoleRegisterSlot0(bool_t created) {
  if(created) {
    int i;
    for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
      if(_ownedAt(i)) { return; }
    }
  }
  else if(forthConsoleStampOnStack()) {
    return;
  }
  softmenuStack[0].userMenuId = created ? FORTH_CONSOLE_FRAME_STAMP
                                        : FORTH_CONSOLE_BORROW_STAMP;
}

/* Every close path funnels through forthCapClose(), which calls this: a
 * stamp must not outlive the capture that minted it, or the next native
 * push of the same menu fails pushSoftmenu's (softmenuId, userMenuId) dedup
 * and duplicates the row.  Clearing to 0 restores exactly what a native
 * push writes for non-dynamic frames, so a released BORROWED base is
 * byte-identical to the frame the user stacked. */
void forthConsoleUnstampAll(void) {
  int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
    if(_stampedAt(i)) { softmenuStack[i].userMenuId = 0; }
  }
}

#if defined(FORTH_DEBUG_SELFTEST)
/* Selftest-only stamp census: the battery asserts the ownership invariant
 * rather than trusting the prose, and mutation coverage must be able to
 * pin the raw field. */
uint8_t forthConsoleTestOwnedCount(void) {
  uint8_t n = 0; int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) { if(_ownedAt(i)) { n++; } }
  return n;
}
uint8_t forthConsoleTestBorrowCount(void) {
  uint8_t n = 0; int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
    if(softmenuStack[i].userMenuId == FORTH_CONSOLE_BORROW_STAMP) { n++; }
  }
  return n;
}
/* Slot index of the single owned/borrowed frame, or -1.  The "owned ABOVE
 * borrowed" half of the invariant needs the positions, not just the counts. */
int16_t forthConsoleTestOwnedSlot(void) {
  int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) { if(_ownedAt(i)) { return (int16_t)i; } }
  return -1;
}
int16_t forthConsoleTestBorrowSlot(void) {
  int i;
  for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
    if(softmenuStack[i].userMenuId == FORTH_CONSOLE_BORROW_STAMP) { return (int16_t)i; }
  }
  return -1;
}
#endif

/* EXIT rung 2's predicate: is the console's BASE the visible row (so EXIT
 * should fall through to rung 3), or is something stacked above it (so EXIT
 * should pop)?  The base is the registered frame when one exists.  With no
 * stamp anywhere (transiently possible mid-line, between a destructive
 * calcModeNormal() and the restore choke point, or after an exotic external
 * unwind) the legacy identity test is the conservative fallback: it can only
 * make rung 3 decline a pop, never over-pop. */
bool_t forthConsoleBaseOnTop(void) {
  if(_stampedAt(0)) { return true; }
  if(forthConsoleStampOnStack()) { return false; }   /* ours is buried */
  return currentMenu() == -MNU_FORTH || currentMenu() == -MNU_ALPHA;
}

/* Bring a `want` row to slot 0 and register it.  See the banner: ALPHA is
 * hand-rolled (dedup must not touch user rows), FWRD goes through
 * showSoftmenu (picker rebuild) and registers a dedup-hoisted user row as
 * BORROWED.
 *
 * The OWNED-already arm retargets the existing owned frame rather than
 * stacking a second one: a push whose stamping is delegated to a function
 * entitled to DECLINE would leave a pushed frame nothing owns — EXIT's
 * overlay rung would pop it, the surface owner would re-push it, and the
 * press would cycle without reaching the excursion rung.  Neither caller
 * can reach here with an OWNED frame live today; the arm is hardening
 * against a future caller breaking that invariant silently. */
static void _forthConsoleAcquireRow(int16_t want) {
  { int i;
    for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
      if(_ownedAt(i)) {
        if(menu((uint8_t)i) != want) {
          int16_t m = _softmenuIndexOf(want);
          if(m >= 0) {
            softmenuStack[i].softmenuId = m;
            softmenuStack[i].firstItem  = 0;
            doRefreshSoftMenu = true;
          }
        }
        return;
      }
    }
  }

  if(want == -MNU_ALPHA) {
    int16_t m = _softmenuIndexOf(-MNU_ALPHA);
    if(m < 0) { return; }
    xcopy(softmenuStack + 1, softmenuStack,
          (SOFTMENU_STACK_SIZE - 1) * sizeof(softmenuStack_t));
    softmenuStack[0].softmenuId = m;
    softmenuStack[0].firstItem  = 0;
    softmenuStack[0].userMenuId = 0;          /* a fresh native-shaped frame... */
    softmenuStack[0].calcMode   = calcMode;
    forthConsoleRegisterSlot0(true);          /* ...claimed through the ONE
                                                 site that decides ownership */
    doRefreshSoftMenu = true;
    return;
  }
  { bool_t theirs = false;
    int i;
    for(i = 0; i < SOFTMENU_STACK_SIZE; i++) {
      if(menu((uint8_t)i) == -MNU_FORTH && !_stampedAt(i)) { theirs = true; break; }
    }
    showSoftmenu(-MNU_FORTH);
    if(currentMenu() == -MNU_FORTH && !_stampedAt(0)) {
      forthConsoleRegisterSlot0(!theirs);
    }
  }
}

void forthConsoleShowSurface(void) {
  int16_t want, cur, m;

  if(!forthCapInteractiveLive()) { return; }

  want = forthCapKeysMode() ? -MNU_FORTH : -MNU_ALPHA;
  cur  = currentMenu();

  if(_ownedAt(0)) {
    if(cur == want) { return; }                /* already right */
    /* Fold back onto the FWRD row beneath ours instead of keeping a
     * duplicate above it — FWRD only; see the banner. */
    if(want == -MNU_FORTH && menu(1) == -MNU_FORTH && !_ownedAt(1)) {
      popSoftmenu();
      if(!_stampedAt(0)) {
        softmenuStack[0].userMenuId = FORTH_CONSOLE_BORROW_STAMP;
      }
      return;
    }
    /* Our own frame, wrong row — swap it in place.  userMenuId is untouched:
     * the stamp rides the frame. */
    m = _softmenuIndexOf(want);
    if(m >= 0) {
      softmenuStack[0].softmenuId = m;
      softmenuStack[0].firstItem  = 0;
      doRefreshSoftMenu = true;
    }
    return;
  }

  if(_stampedAt(0)) {                          /* BORROWED base on top */
    if(cur == want) { return; }
    /* The user's own surface row shows the other sub-mode.  Never
     * retarget their frame — acquire our own over it; theirs stays put
     * (and, for ALPHA, shielded) beneath. */
    _forthConsoleAcquireRow(want);
    return;
  }

  /* Slot 0 is not registered: rows the USER stacked sit above the base (the
   * EXIT ladder's overlay rung unwinds them one press at a time), or nothing
   * is registered at all — a line just destroyed the surface and
   * forthConsoleRestoreSurface() is the re-establisher, not this function.
   *
   * A buried OWNED frame is still retargeted IN PLACE, so the base stays
   * truthful beneath the overlay — the REPL reopen legitimately flips to
   * keys while an overlay is up, and leaving the covered base on ALPHA
   * would leave the mode indicator wrong the moment the overlay pops.
   * Possible at all only because the stamp identifies OUR frame at depth;
   * a BORROWED base is the user's frame and is never rewritten (a
   * borrowed base is FWRD, which is what keys wants).  User rows above
   * and below stay untouched. */
  { int i;
    for(i = 1; i < SOFTMENU_STACK_SIZE; i++) {
      if(_ownedAt(i)) {
        if(menu((uint8_t)i) != want) {
          m = _softmenuIndexOf(want);
          if(m >= 0) {
            softmenuStack[i].softmenuId = m;
            softmenuStack[i].firstItem  = 0;
          }
        }
        return;
      }
    }
  }
}

/* Re-establish the console's row after something may have DESTROYED it.
 *
 * Separate entry point because the precondition differs and the caller is
 * the only one who knows it.  forthConsoleShowSurface() leaves a foreign
 * top row alone, because a foreign row normally means the user stacked
 * something.  After a line has run, a foreign row can instead mean the
 * console's own frame was popped out from under it: calcModeNormal() pops
 * the ALPHA row and retargets MyAlpha to MyMenu, and any native item may
 * call it.
 *
 * The scan asks "does the REGISTERED frame survive, anywhere?" — a
 * top-row-only inspection would conclude "ours is gone" under any stacked
 * menu and push a SECOND console row.  Only when the registered frame is
 * gone is the surface re-established, through the acquisition rules —
 * never by adopting whatever the pop revealed. */
void forthConsoleRestoreSurface(void) {
  if(!forthCapInteractiveLive()) { return; }

  if(forthConsoleStampOnStack()) {
    forthConsoleShowSurface();                 /* alive somewhere: the
                                                  ownership rules decide */
    return;
  }
  _forthConsoleAcquireRow(forthCapKeysMode() ? -MNU_FORTH : -MNU_ALPHA);
}


/* HOME.3 over a live console, both halves, in ONE place — each upstream
 * site is a two-line seam.
 *
 * THE RULING, taken from what the gesture NATIVELY does: upstream's own
 * arm (openHOMEorMyM's FLAG_ALPHA branch) dismisses the alphabetic
 * overlay if one is on top, then LANDS on the row that matches the
 * current input context (TAMALPHA vs ALPHA by tam.alpha).  Two halves,
 * and the second is the one the gesture is named for: openHOMEorMyM
 * OPENS a home row.  The console adds a sub-mode upstream does not have,
 * so the faithful translation keeps both halves and evaluates the same
 * conditional against the state the console actually has:
 * forthConsoleShowSurface picks FWRD or ALPHA from keysMode exactly as
 * upstream picks TAMALPHA or ALPHA from tam.alpha.  That keeps K-R3 —
 * the row IS the mode indicator.
 *
 * tam.alpha still reaches the native arm: during a TAM session the
 * capture is SUSPENDED, not live, so this returns false.
 *
 * Returns true when the gesture was handled here and the caller must skip
 * its native arm entirely. */
bool_t forthConsoleHomeRow(void) {
  if(!forthCapInteractiveLive()) {
    return false;
  }
  if(!forthConsoleBaseOnTop()) {
    popSoftmenu();          /* the dismiss half — ONE frame per press, which
                               is both upstream's shape here and the EXIT
                               ladder's rung-1 idiom */
  }
  forthConsoleShowSurface();/* the land half, row chosen by sub-mode */
  return true;
}
