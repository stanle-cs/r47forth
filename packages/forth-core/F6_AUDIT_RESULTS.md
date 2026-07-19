# F6 audit results — architect traces T1-T7 (bench Blocks A-F deferred)

Status: **TRACES FOLDED 2026-07-18; HARDWARE BENCH DEFERRED by owner ruling
2026-07-18** ("author the F6 packet without the hardware test for now").
The charter's Blocks A-F move from authoring PRECONDITION to **stage-exit
confirmation**: every F6 fixture below is derived from code traces and runs
on the PC-build self-test; the bench session remains queued and any
hardware divergence found there triggers packet/design amendment, not
silent adaptation.  DESIGN §10.6's precondition is amended accordingly
(see DESIGN-HISTORY 2026-07-18).

Trace base: branch `forth-core/pem-entry-fixes`, F3-2 landed
(`e8f1f16cd`) with F3-3 in flight.  File:line anchors cite the PACKAGE
copies (`packages/forth-core/...`); packets re-verify every anchor at
execution time per the standing gates.  Anchors in files F4/F5 packets
will touch (manage.c ENTER arm, tam.c, param_core.c) are flagged
[F4/F5-MOVABLE] — the F6 packets' execution gates re-grep them.

## T1 — pemAlpha arm map (programming/manage.c:793-1014)

Entry `pemAlpha(item)`; `tam.function` is the capture-type discriminator
(ITM_LITERAL / ITM_REM / ITM_FORTH), `FLAG_ALPHA` the "capture open" flag.

1. **ITM_EDIT arm** (:795-841). Decodes `currentStep`; per-type sub-arms:
   LITERAL :807, REM :815, **FORTH :824-836** — marker (`currentStep[3]==0`)
   bails; source step renders bare into `aimBuffer`, `T_cursorPos` to end,
   deletes the step, `tam.function = ITM_FORTH`, `editCommand = true`.
2. **Open-capture block** (:843-874, when `!FLAG_ALPHA`): resetShiftState,
   `displayAIMbufferoffset = 0`, buffer/cursor cleared unless editCommand;
   `showSoftmenu(-MNU_ALPHA)` :855; `setSystemFlag(FLAG_ALPHA)`;
   calcModeAimGui; **placeholder step inserted** :859-871 (3-byte literal
   header or 4-byte two-byte-opcode header by `tam.function < 128`), then
   steps back onto it :872-873.
3. **Glyph arm** (`func == addItemToBuffer`, :875-893): numlockReplacements
   → case fold → convertItemToSubOrSup; **CAP :887**:
   `len < (256 - inputCharLength) && stringGlyphLength(aimBuffer) < 196`,
   silently ignores past cap (A4 answered: 197th glyph is a no-op; ENTER of
   a full line commits normally).  Inserts at byte index `T_cursorPos`,
   advances it.  **DOES NOT RETURN — falls through to arm 10.**
4. **ITM_BACKSPACE** (:894-922): empty buffer → delete placeholder, close
   capture (`FLAG_ALPHA` clear, `_closeAlphaMenus`, `tam.function = 0`)
   :895-909 [the F2-experiment marker-delete rule's code side]; cursor at
   0 → no-op :911; else deletes the glyph before the cursor via
   `stringLastGlyph` byte-shuffle :914-921 (A3 answered: one press removes
   one whole multi-byte glyph).  **Falls through to arm 10.**
5. **ITM_ENTER** (:924-936) [F5-MOVABLE: the E9 commit gate lands here]:
   snapshots `wasForth`/`hadText` BEFORE `pemCloseAlphaInput()`;
   `defineFirstDisplayedStep`; `_closeAlphaMenus`; **multi-line lock**
   :931-934 — `wasForth && hadText && forthEntryStateAtInsertion()` →
   `tam.function = ITM_FORTH; pemAlpha(0);` reopens the next placeholder.
6. ITM_USERMODE :937, ITM_CLA :941, CHR_numL/numU/caseUP/caseDN/num/case
   :946-976, ITM_SCR :977, fnT_ARROW items :981 — mode/lock arms, return.
7. **Fall-through incremental re-commit** (:986-1013): re-reads the step's
   opcode, `deleteStepsFromTo(currentStep, findNextStep(currentStep))`,
   re-inserts the step with the full `aimBuffer` payload (literal 3-byte /
   two-byte-opcode 4-byte header with length at [3]), steps back.
   **LOAD-BEARING: the program step ALWAYS holds the typed text after
   every key.** Power-off mid-capture loses only {cursor, open-flag,
   menus} — the text is already a committed source step (A5 answered
   structurally).  F5's commit gate assumes exactly this ("step bytes
   track the buffer; reject = refuse the close").

`pemCloseAlphaInput` (:1016-1042): FORTH+empty → delete placeholder and
close (:1017-1025); else commit: clear buffer, clear FLAG_ALPHA, step
FORWARD :1030-1031, scroll bookkeeping :1032-1035, `_closeAlphaMenus`,
`tam.function = 0` unconditional :1041.
`pemAlphaEdit` (:1045-1061): edit gesture; only `!FLAG_ALPHA && CM_PEM &&
!tam.mode`; dispatches `pemAlpha(ITM_EDIT)` for LITERAL/REM/FORTH steps.
`_closeAlphaMenus` (:756-791): pops MNU_ALPHA-family tops; default arm
resets stack level 0 to MyMenu and returns; fallback wipes the whole
stack to MyMenu.

## T2 — the TAM surface (ui/tam.c; struct src/c47/typeDefinitions.h:675-709)

`tamState_t` has **17 fields**: mode, function, alpha, currentOperation,
dot, colon, indirect, digitsSoFar, value0, value, min, max, key, keyAlpha,
keyDot, keyIndirect, keyInputFinished.  **The F6 suspend snapshot is a
whole-struct copy** (`tamState_t saved = tam;`) — no field enumeration can
rot.  `tam.colon` is in the struct (b8f79e486) ✓.

- `tamEnterMode(func)` (tam.c:1142-1260):
  - :1143-1147 mode/function/min/max from `indexOfItems[].param`/`tamMinMax`.
  - **:1172-1182 THE F6-2 SEAM [F4-MOVABLE]:** `calcMode == CM_PEM &&
    aimBuffer[0] != 0` → `FLAG_ALPHA` ? `pemCloseAlphaInput()` :
    `pemCloseNumberInput()`; buffer zeroed; steps back.  **Entering TAM
    with an open capture line COMMITS AND CLOSES it today** (B1-B3
    answered: line committed early, capture NOT reopened on TAM
    cancel/commit; no state leak because the capture is gone).
  - :1191-1204 resets alpha/currentOperation/digitsSoFar/dot/colon/
    indirect/value/key* — TAM entry scrubs its own state.
  - :1206-1260 pushes the per-mode TAM softmenu (-MNU_TAMNORM/-MNU_TAM/
    -MNU_TAMFLAG/-MNU_TAMSTO…) ON TOP of the current stack.
- `leaveTamModeIfEnabled` (tam.c:1352-1398): `tam.alpha = false;
  tam.mode = 0;` **`clearSystemFlag(FLAG_ALPHA)` :1369 unconditionally**;
  pops `numberOfTamMenusToPop` menus; `catalog = CATALOG_NONE` (except
  MVAR view case :1360-1365).  **A resumed capture must re-establish
  FLAG_ALPHA + its menu + cursor itself.**
- EXIT during TAM (keyboard.c:3749-3787): menus>1 → pop one (clearing a
  pending `tam.colon` :3770-3773); else **`aimBuffer[0] = 0` when
  CM_PEM :3779**, `leaveTamModeIfEnabled()`, `scrollPemBackwards()`.
  **TAM-cancel DESTROYS aimBuffer in PEM — a suspended capture line
  cannot live there.**  This alone justifies the managed buffer (F6-1).

## T3 — softmenu stack during capture (softmenus.c)

- `showSoftmenu(id)` :3972+ — pushes; HOME redirects to -MNU_DYNAMIC.
- `popSoftmenu` :3734-3767 — shifts stack down, refills bottom with
  MyMenu; AIM special cases (MyMenu↔MyAlpha by `calcMode == CM_AIM`
  :3743-3748); **FLAG_BASE_HOME re-pushes HOME when not AIM :3749-3751**
  (why §8.4 E1's teardown loop is bounded-iteration, never
  spin-on-predicate).
- `isAlphaSubmenu` :3902-3911: **-MNU_FORTH is classified an ALPHA
  submenu** (forth-core edit) — EXIT pops it back to ALPHA instead of
  killing capture.
- Capture opens with `showSoftmenu(-MNU_ALPHA)` (manage.c:855); TAM
  pushes its menu above; `_closeAlphaMenus` (manage.c:756) is the
  capture-side teardown.  The §8.4 EXIT ladder is: picker/alpha submenu →
  ALPHA → (close capture only via its own arms, never via menu pop).

## T4 — catalog machinery to reuse (softmenus.c)

- `initVariableSoftmenu` :1676, **`case MNU_FORTH:` :1865-1960**: the
  landed §8.6 picker is a TEXT SCAN — walks steps from
  `forthOwningProgramStart(currentStep)` to `currentStep`, tokenizes
  source lines, collects `: NAME` names into 15-byte `tmpString` slots,
  cap `TMP_STR_LENGTH/15 = 170` by scan order, dedups, sorts.  **It never
  reads the dictionary** (E1 answered statically: interactive FWRD outside
  a program shows nothing).  The F6-5 catalog replaces this builder with a
  dictionary walk (fdict filtered per F3 scopes + gdict), same menu id.
- Rebuild-always: :3184 `menuItem != cachedDynamicMenu || == -MNU_DYNAMIC
  || == -MNU_FORTH` — FWRD rebuilds on every show; content freshness is
  structural, no invalidation hooks needed.
- `dynmenuGetLabel`/`WithDup` :4243-4249 — label fetch for dynamic menus.
- Picker action path (keyboard.c): `forthPickerGuard` :32-38 (top-of-stack
  must BE -MNU_FORTH), insert :1016-1019 — `pickerInsertName()` then
  `pemAlpha(ITM_NOP)` re-commits the step (P-H7).  The F6-5 catalog
  keeps this exact insertion discipline.

## T5 — cursor and scroll (screen.c, manage.c)

- `T_cursorPos` is a BYTE index into aimBuffer; glyph discipline via
  `stringLastGlyph`/`stringNextGlyph` at every move (manage.c:810/818/831,
  :914-921; screen.c:3830-3838 range-clamps before render).
- `displayAIMbufferoffset` (screen.c:1790-1800): render-window offset,
  advanced by glyph until the cursor fits the line width.
- `scrollPemBackwards`/`Forwards` (manage.c:413-427): maintain
  `firstDisplayedLocalStepNumber`/`firstDisplayedStep`;
  `defineFirstDisplayedStep()` recomputes after edits; capture close paths
  call them (manage.c:1012, keyboard.c:3783).
- F6 keeps T_cursorPos/displayAIMbufferoffset AS THE RENDER CONTRACT and
  maps the managed buffer through the same render call
  (`showStringEdC47`, screen.c:3836).

## T6 — aimBuffer lifecycle (machine-enumerated 2026-07-18)

`grep -c aimBuffer` per capture-path file: manage.c **117**, keyboard.c
**77**, ui/tam.c **23**, screen.c **22**, softmenus.c **2**.  (Re-run at
F6-1 execution; counts are the inventory gate.)  Load-bearing classes:
capture sink (T1 arms), TAM name entry (tam.c), NIM sharing
(pemAddNumber, manage.c:1064+), render (screen.c), destructive resets
(keyboard.c:3779; tam.c:1179).  The FORTH capture sink is the ONLY class
F6-1 moves; REM/LITERAL/NIM/TAM users stay on aimBuffer.
- Cap: 256 bytes / 196 glyphs (manage.c:887).  FORTH source-step payload
  cap is 255 by the length byte; §3.3 FORTH_SOURCE_MAX bounds the
  interpreter side — F6-1 pins the managed buffer at the SAME 256/196
  contract (no capacity change; DEFERRED-BENCH A4 note below).
- Allocation model (§5.2, proven in forth_dict.c:262/278):
  `allocC47Blocks`/`reallocC47Blocks` + block-quantized sizes; handle
  pattern per DESIGN §10.6 ("addressed through a relocation-safe handle").

## T7 — key dispatch reachability in capture

- PEM alpha routing (keyboard.c:766-771): `calcMode == CM_PEM && !tam.mode
  && FLAG_ALPHA` → `pemAlpha(item)` for addItemToBuffer-class items; else
  `addStepInProgram(item)`.  `keyStateCode = (FLAG_ALPHA ? 3 : 0) + shift`
  (:1959) selects the alpha remap layer; `numlockReplacements`
  (src/c47/c47Extensions/keyboardTweak.c:1022) applies NUM/CAPS-lock
  substitutions inside the glyph arm (manage.c:877).
- SST/BST in PEM: gated at keyboard.c:1951 (DMCP shift capture) and the
  :2880/:2902/:2935 arms — reachable only when NOT in alpha capture (the
  FLAG_ALPHA branch above wins first).  Mid-capture SST/BST today are
  glyph keys or ignored — no step motion.  F6 keeps that (no new escape
  routes; EXIT/ENTER/BACKSPACE remain the only structural exits).
- `ITM_AIM` first-key contract (F15-4, binding rule 8): only
  `insertStepInProgram`'s ALPHA arm consults `forthEntryStateAtInsertion()`
  after `addStepInProgram`'s pre-move; digits/`':'` consult without the
  pre-move.  All F6 typing fixtures inherit rule 8's drive discipline.
- **The EXIT ladder in CM_PEM** (`fnKeyExit`, keyboard.c:3917-3959),
  load-bearing for F6-1/F6-3: (1) pending error → clear, stay; (2)
  `FLAG_ALPHA && !tam.mode && isAlphaSubmenu(0)` → pop the submenu only
  :3922-3926 (picker/alpha submenus pop toward ALPHA, capture untouched);
  (3) `FLAG_ALPHA && aimBuffer[0]==0` → `pemAlpha(ITM_BACKSPACE)` + BST
  :3928-3931 (EXIT on an EMPTY capture line = the abort path); (4)
  `aimBuffer[0]!=0` → `pemCloseAlphaInput()` + BST :3933-3945 (EXIT with
  TEXT = commit-and-close); (5) else menu pops / leave PEM :3947-3959.
  Arms 3/4 read `aimBuffer[0]` to pick abort-vs-commit — F6-1 re-points
  BOTH reads at the capture sink (its Change D2) or EXIT-with-text would
  regress to a one-glyph delete.

## Deferred-bench register (owner ruling 2026-07-18)

| Charter item | Interim substitute (trace-derived) | Residual hardware risk |
|---|---|---|
| A1-A2 open/commit/edit | T1 arms 1/2/5/7 byte-image goldens on PC build | DMCP display timing only |
| A3 glyph backspace | T1 arm 4 (`stringLastGlyph` shuffle) | none (shared code) |
| A4 196/197th glyph | T1 arm 3 cap :887 (silent ignore) | none |
| A5 power-off mid-capture | T1 arm 7: text already committed per key; loses cursor/open-flag only | DMCP save timing — F6-6 carries the contract; bench confirms |
| A6/F1 EXIT ladder | T3 `isAlphaSubmenu` + §8.4 table | none |
| F2 empty-line backspace | T1 arm 4 empty-buffer branch | none |
| B1-B4 TAM nesting | T2 seams :1172/:1369/:3779 (today: commit-and-close) | none — F6-2 REPLACES this behavior; bench re-run post-F6-2 |
| C1-C3 param entry | T2 + F4 canonical spellings; native step images from PC encoders | none (same encoders) |
| D1-D3 menus | T3/T4 stack discipline | none |
| E1 picker delta | T4: text-scan proven by source | none |

Exit criteria for the DEFERRED bench (unchanged from the charter, retimed):
run Blocks A-F on the DM42n **after F6-6 lands**; any divergence from the
landed behavior is triaged (design amendment vs upstream report vs fix)
before the F6 stage may close.
