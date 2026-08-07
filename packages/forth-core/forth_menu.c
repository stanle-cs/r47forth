/*
 * forth_menu.c — Forth capture UI: the FWRD word picker and the
 * name-insertion helpers shared by the picker and the catalogs.
 *
 * S2 (simplification pass): this code previously lived inside the
 * upstream files it is called from — the MNU_FORTH builder inside
 * softmenus.c's initVariableSoftmenu(), the insert/guard helpers at the
 * top of keyboard.c.  Nothing here is upstream logic, so carrying it as
 * ~210 lines of patch against two large, actively-maintained upstream
 * files bought nothing but rebase surface.  Moved verbatim; the upstream
 * patches are now the call sites alone.
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
 * start (K2 token-boundary guard).  Same 256-byte/196-glyph cap as
 * typing; false = no room or capture not open.  §9.6 P-H7 discipline,
 * generalized (F6-3). */
bool_t forthCapInsertName(const char *name)
{
  int32_t nameLen = stringByteLength(name);
  if(!forthCapIsOpen()) { return false; }
  int32_t bufLen = stringByteLength(aimBuffer);
  /* K2: token-boundary guard — a name must land as its own token.  When
   * the byte before the cursor is neither a space nor the line start,
   * insert one leading separator space.  Previously only the F6-4 fold
   * wrapper did this; the direct F6-3/picker/keys-mode paths glued
   * digits to names ("42" + SIN -> "42SIN ", an unresolvable token). */
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

/* MNU_FORTH content builder (§9.6, F6-5): the UNION of the edited
 * program's own ': NAME' definitions before the cursor (text scan — no
 * execution needed), the interactive-scope dictionary, and the global
 * dictionary, each section sorted independently.
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
  /* R2 finding 6, ruled: accepted names are copied into 15-byte slots in
   * this same global tmpString with no capacity check; TMP_STR_LENGTH/15
   * = 170 complete slots fit, and a personal program with more unique
   * definitions before the cursor could write past the workspace.
   * Policy: truncate by scan order. Once the cap is reached, stop
   * RECORDING new names but keep tokenizing normally — nothing else in
   * this loop depends on nNames, and the cap must not desync the
   * tokenizer's position in the line. Sorting applies only to the names
   * actually collected. No error UI: this is ordinary single-user
   * robustness (a reboot/lost-edit risk), not a reportable condition,
   * and 170 unique word definitions before the cursor is far beyond any
   * realistic personal program. */
  const int16_t forthPickerMaxNames = TMP_STR_LENGTH / 15;

  memset(tmpString, 0, TMP_STR_LENGTH);

  /* L1-3 (C5), extended by M1 (Stage M) ADDITIVELY: the interactive gate
   * keeps its L1-3 rationale, and the two surfaces Stage M opens —
   * CM_NORMAL and CM_ASSIGN, the catalog-tree reachability — join it:
   * there currentStep points at whatever the PEM cursor last was, so
   * scanning would list that program's definitions with false provenance,
   * and a normal-mode press of such a name resolves to nothing and
   * errors, so it must not be offered.  Every OTHER mode keeps its landed
   * behaviour byte for byte (an earlier over-wide PEM-only gate turned
   * twelve landed text-scan tests red — the builder is deliberately
   * mode-blind everywhere the pre-M surfaces reach it). */
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

  /* F6-5 section (b): interactive-scope dictionary words. */
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

  /* F6-5 section (c): global dictionary words. */
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

/* ---- AUDIT C2/C3/C4/C8/C9 (2026-08-06): one owner for the console's row ----
 * ---- AUDIT C17 (2026-08-06): ownership rides the FRAME, not the menu id ----
 *
 * While an interactive capture is open, the console owns AT MOST ONE softmenu
 * frame: FWRD in keys input, ALPHA in the alpha excursion.  Stage N pushed
 * that frame at open and then let four other sites manage it independently —
 * the E10/E11 toggle, both EXIT rungs, and the REPL reopen — and every one of
 * them got a different part of it wrong (the C2/C3/C4/C8/C9 family; see the
 * round-1 report).  This is the ONLY function that changes the console's row.
 *
 * C17 is what was still wrong after that consolidation: every ownership
 * decision asked "is the visible menu FWRD or ALPHA?", and a menu id is a
 * value TWO DIFFERENT OWNERS can hold.  The user's own FWRD frame — slot 0
 * precisely when they browsed the CATALOG tree to FWRD before pressing FORTH
 * — answered "ours", was retargeted to ALPHA, and a line's calcModeNormal()
 * then popped it: their frame, consumed, unrecoverable.  A single homePushed
 * bit on the capture object could not fix this: it says whether the console
 * owns A frame, never WHICH frame, and it had to be hand-preserved across
 * every capture reopen (the C3 family — two sites patched, each missed once).
 *
 * So ownership now lives IN the frame.  The frame the console relies on
 * carries a sentinel in its userMenuId — FRAME_STAMP for a frame the console
 * CREATED (the close rung pops it), BORROW_STAMP for the USER'S OWN row the
 * console is merely displaying (the close rung releases it) — and every
 * decision — retarget, restore, "is anything stacked above the base", "pop
 * what the open pushed" — tests the stamps.
 *
 * THE INVARIANT, stated correctly (AUDIT round 3 — an earlier draft of this
 * banner claimed "exactly one frame is registered", which is NOT what the
 * code does and was caught by three independent readers): while an
 * interactive capture is open the stack carries AT MOST ONE BORROWED base
 * and AT MOST ONE OWNED frame, and when both exist the OWNED one is ABOVE
 * the borrowed one.  Two registrations is the normal, correct state of the
 * alpha excursion opened over the user's own FWRD row: the borrow marks
 * their row so dedup cannot lift it and the close cannot pop it, while the
 * owned frame carries the excursion.  What must never happen is two OWNED
 * frames, or a stamp of either kind outliving its capture.  Every close
 * path clears both — forthCapClose() is the funnel, with
 * forthCapAbandonSuspended() and the restore sanitizer as the two paths
 * that reach a closed capture WITHOUT passing through it.
 *
 * The field is safe to borrow: it is meaningful only for -MNU_DYNAMIC frames
 * (pushSoftmenu/popSoftmenu/fnGetMenu all gate on that), native pushes write
 * 0, real user-menu ids are >= 0, and the one native mutator
 * (removeUserMenuFromStack's re-number loop) only decrements values GREATER
 * than a non-negative threshold — a NEGATIVE stamp is inert everywhere
 * upstream.  Because the stamp is in the frame, it shifts with every
 * push/pop/dedup-lift and survives every capture reopen and resume by
 * construction: the whole "preserve homePushed across reopen X" class dies.
 * Marking the BORROWED base is load-bearing, not bookkeeping: an unmarked
 * base no longer matches pushSoftmenu's (softmenuId, userMenuId) dedup, so a
 * native re-push of the same menu can neither lift the user's base out from
 * under the console nor early-return against it — the failure sequence the
 * out-of-family design review (GPT-5 Sol, 2026-08-06) constructed against a
 * single-stamp draft of this fix.
 *
 * The frame count/content discipline is unchanged: the sub-mode swap still
 * RETARGETS slot 0 in place rather than popping and pushing, because
 * popSoftmenu() carries its own CM_AIM compensation that re-pushes an alpha
 * row (softmenus.c:3719-3733) and pushSoftmenu() dedups against a match
 * ANYWHERE in the array by lifting the stack over it (softmenus.c:3671-3683).
 * Two deliberate exceptions, both bounded:
 *
 *   - FOLD-BACK, FWRD only: when the console's own frame sits directly on a
 *     FWRD row that is not also console-created, the keys-mode swap POPS ours
 *     instead of retargeting — returning the user's frame at the user's page
 *     instead of keeping a duplicate above it.  FWRD only, because nothing
 *     native ever pops a FWRD row; folding back onto a user's ALPHA row would
 *     hand it straight to the next line's calcModeNormal(), which pops
 *     -MNU_ALPHA on sight (the failure sequence the OTHER out-of-family
 *     reader, Gemini, constructed against the same draft).  Safe from
 *     popSoftmenu's CM_AIM compensation: that fires only when the pop reveals
 *     softmenuId 0/1, and the revealed row here is a real FWRD.
 *
 *   - HAND-ROLLED ALPHA acquisition: re-establishing the ALPHA surface never
 *     goes through showSoftmenu, whose dedup would LIFT a user's own ALPHA
 *     row to slot 0 — where the next calcModeNormal() destroys it (C17's
 *     class) — and whose early-return would leave the user's row where a
 *     console-created one was needed.  A fresh frame is shifted in above
 *     whatever is there; a user ALPHA row deeper stays SHIELDED beneath it.
 *     ALPHA is a static menu, so no content build is skipped.  FWRD
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

/* Register slot 0 as the console's surface frame.  `created` says whether the
 * open/acquire pushed it (OWNED: the close rung pops it) or it is the user's
 * own row (BORROWED: the close rung releases it).
 *
 * An OWNED registration is refused while an OWNED frame already exists — two
 * owned frames is the state the invariant forbids, and the path that reaches
 * here with one live is FORTH pressed inside an open console (C6, open
 * finding).  A BORROWED registration is refused while ANY stamp exists: the
 * borrow marks the base, and the base is claimed once, at the open. */
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
/* AUDIT round 4: the ownership invariant is stated in the banner and was
 * WRONG for a whole session before three readers caught it, so the battery
 * now asserts it rather than trusting the prose.  Selftest-only, same
 * rationale as forthTestCapOrigin: production has no business counting
 * stamps, but mutation coverage must be able to pin the raw field. */
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
 * AUDIT round 4 — the OWNED-already guard below is why this cannot become a
 * C18.  Both out-of-family readers, independently, attacked this function
 * with the same shape: it PUSHES a frame and then delegates the stamping to
 * a function entitled to DECLINE, so a decline would leave a pushed frame
 * that nothing owns — and EXIT's overlay rung would pop it, the surface
 * owner would re-push it, and the press would cycle without reaching the
 * excursion rung.
 *
 * BOTH TRACES WERE WRONG about how the state is reached (they had the
 * sub-mode toggle over an owned base entering here, when
 * forthConsoleShowSurface retargets that frame IN PLACE and never calls
 * this — verified by probe: after the toggle slot 0 is ALPHA and still
 * stamped).  Enumerating this function's two callers, neither can reach it
 * with an OWNED frame live: ShowSurface calls it only from the BORROWED-base
 * branch, and an owned frame is always created ABOVE the borrow so the
 * borrow cannot be back on top while one exists; RestoreSurface calls it
 * only when NO stamp exists anywhere.
 *
 * The guard is therefore hardening, not a bug fix, and it is worth its four
 * lines: the failure the readers described is exactly the class this
 * codebase already paid for once (C18 — "a state change committed by the
 * caller and the display of that state established by a callee that may
 * decline"), the reachability argument above rests on an invariant that a
 * future caller could break silently, and two independent readers finding
 * the same shape is the agreement CODE_AUDIT.md says to act on.  Retarget
 * the owned frame instead of stacking a second one: same end state, and the
 * push-then-decline window does not exist. */
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

  if(!forthCapIsInteractive() || !forthCapIsOpen()) { return; }

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
    /* C17: the user's own surface row shows the other sub-mode.  Never
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
   * AUDIT C18: a buried OWNED frame is still retargeted IN PLACE, so the
   * base stays truthful beneath the overlay — the REPL reopen legitimately
   * flips to keys while an overlay is up (N-R6: keys-first survives every
   * ENTER), and leaving the covered base on ALPHA would leave the mode
   * indicator wrong the moment the overlay pops.  Possible at all only
   * because the stamp identifies OUR frame at depth (C17); a BORROWED base
   * is the user's frame and is never rewritten — for it, the reopen's flip
   * needs no repair (a borrowed base is FWRD, which is what keys wants).
   * User rows above and below stay untouched. */
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
 * Separate entry point because the precondition differs and the caller is the
 * only one who knows it.  forthConsoleShowSurface() above leaves a foreign top
 * row alone, because a foreign row normally means the user stacked something.
 * After a line has run, a foreign row can instead mean the console's own frame
 * was popped out from under it: calcModeNormal() pops the ALPHA row
 * (src/c47/calcMode.c:44-46) and retargets MyAlpha to MyMenu, and any native
 * item may call it — fnClearStack does, outright.
 *
 * The scan asks "does the REGISTERED frame survive, anywhere?", and if so the
 * normal ownership rules apply.  (Top-row-only inspection was the frame leak
 * the OUT-OF-FAMILY reader found on 2026-08-06 after eight in-family readers
 * missed it: with a menu stacked it concluded "ours is gone" and pushed a
 * SECOND console row.)  Only when the registered frame is gone is the surface
 * re-established, through the acquisition rules — never by adopting whatever
 * the pop revealed, which is the C17 half of the same class. */
void forthConsoleRestoreSurface(void) {
  if(!forthCapIsInteractive() || !forthCapIsOpen()) { return; }

  if(forthConsoleStampOnStack()) {
    forthConsoleShowSurface();                 /* alive somewhere: the
                                                  ownership rules decide */
    return;
  }
  _forthConsoleAcquireRow(forthCapKeysMode() ? -MNU_FORTH : -MNU_ALPHA);
}
