# Stage N — architect pre-work traces N-T1..N-T5

**Status: complete 2026-08-05. Evidence sheet for STAGE_N_CONSOLE.md's
packet decomposition; not normative. Every file:line was read this pass on
`forth-core/stage-n` at the Stage N authoring commit (`99c6f3914`). Line
numbers cite the PACKAGE copies where an override exists (screen.c,
keyboard.c, softmenus.c, items.c, defines.h, programming/manage.c,
ui/tam.c are package files); `src/c47/...` paths are upstream-only files.**

Six findings below change what STAGE_N_CONSOLE.md said. They are marked
**[CORRECTS THE STAGE DOC]** and collected in the closing section.

## N-T1 — the render seam

### The canvas

| thing | value | site |
|---|---|---|
| register rows T/Z/Y/X | Y 24 / 60 / 96 / 132, `REGISTER_LINE_HEIGHT` 36 | defines.h:1516-1521 |
| softmenu block | `217 - SOFTMENU_HEIGHT*ySoftKey`, 3 rows, top row Y=171 | softmenus.c:1940, defines.h:1524 |
| the stack-area clear rect | Y 20 .. 170 (`topLeftGraphInfoY` = 24-4, `heightGraphInfoBox` = 240-24-69+4 = 151) | src/c47/screen.h:88,90 via screen.c:5686-5690 |
| the listing pitch | **21 px**, rows `Y_POSITION_OF_REGISTER_T_LINE + 21*line`, line 0..6 → 24,45,66,87,108,129,150 | programming/manage.c:541,546-551 |

So the console's canvas is Y 24..170, and seven 21-px rows fit it — the
fnPem listing is the existence proof, not an analogy. `STANDARD_FONT_HEIGHT`
(22, defines.h:733) is the font-browser metric; 21 is the landed pitch and
is what the console uses.

### The seam

`_refreshNormalScreen`'s CM_AIM arm, screen.c:5923-5931:

```c
else {
  if(yMultiLineEdOffset == 3) {
    refreshRegisterLine(REGISTER_T);
    refreshRegisterLine(REGISTER_Z);
    refreshRegisterLine(REGISTER_Y);
  }
  refreshRegisterLine(REGISTER_X);      // :5930 — AIM_REGISTER_LINE is REGISTER_X (defines.h:1495)
}
```

`refreshRegisterLine(REGISTER_X)` **is** the edit-line draw: `_refreshRegisterLine`'s
`regist == AIM_REGISTER_LINE && calcMode == CM_AIM && !tam.mode` arm
(screen.c:3871) calls `showStringEdC47(multiEdLines, displayAIMbufferoffset,
T_cursorPos, aimBuffer, 1, Y_POSITION_OF_NIM_LINE - 3 - checkHPoffset, ...)`
at screen.c:3881. The console arm replaces the three T/Z/Y calls and leaves
:5930 alone — the input band and the yield path's X row are the same call.

`_selectiveClearScreen()` runs at screen.c:5902, **before** the arm, and
clears Y 20..170 whenever `SCRUPD_MANUAL_STACK | SCRUPD_SKIP_STACK_ONE_TIME`
is clear (screen.c:5682-5691) — which `showStringEdC47` itself guarantees for
AIM by clearing `SCRUPD_MANUAL_STACK` on every call (screen.c:1660, :1666).
**The console needs no clear of its own**; it paints onto a just-cleared band.

### The gate — every conjunct justified

```c
calcMode == CM_AIM            /* the enclosing else already establishes it */
&& forthCapIsInteractive()    /* forth_capture.c:89 — PEM captures keep the listing */
&& !tam.mode                  /* see (c) */
&& lastErrorCode == 0         /* see (b) */
&& temporaryInformation == TI_NO_INFO   /* see (a) */
```

**(a) `temporaryInformation == TI_NO_INFO` is load-bearing, not hygiene.
[CORRECTS THE STAGE DOC]** The stage doc asked for the TI arms to be
"audited for the same swallow hazard the error line has". The audit's answer
is stronger than a swallow: `displayTemporaryInformationOnX` (screen.c:2566)
repaints **all four register rows** (screen.c:2573-2576) and is called from
sixteen TI arms inside `_refreshRegisterLine` (screen.c:3307, 3312, 3316,
3389, 3396, 3414, 3420, 3463, 3468, …), each gated `regist == REGISTER_X`.
The X-row call the console *keeps* (screen.c:5930) therefore re-enters the
four-row paint whenever a TI is live, wiping the transcript from inside the
one call the console does not own. Reachable: a console line can execute any
`CAT_FNCT | PTP_NONE` item (§4.1 step 4, forth_dict.c:602-614), and `BATTV`,
`BYTES`, `BITS`, `WHO`, `VERS`, `WOY` and the RESTORED/SAVED family all set
`temporaryInformation`. The conjunct is in the gate from day one and the
whole area yields.

The `elecTI` prefixes (screen.c:2378-2412, TI_ABC/TI_ABBCCA/TI_012) and the
3-D vector-component arms (screen.c:2689-2733) paint on Y and Z as well —
covered by the same conjunct.

**(b) `lastErrorCode == 0`** — the error text paints inside
`_refreshRegisterLine` gated `lastErrorCode != 0 && regist ==
errorMessageRegisterLine` (screen.c:3778-3786), default line
`ERR_REGISTER_LINE` = REGISTER_Z (defines.h:1498), set by
`displayCalcErrorMessage` (src/c47/error.c:296-298), which also forces
`screenUpdatingMode = SCRUPD_AUTO`. `CM_ERROR_MESSAGE` routes through
`_refreshNormalScreen` like the normal modes (screen.c:6182-6216). An
unconditional console arm would swallow every error display.

**(c) `!tam.mode` is what protects the fold — not the forged CM_PEM.
[CORRECTS THE STAGE DOC]** The stage doc's N-T1 asked to "confirm the forged
CM_PEM routes to `_refreshPemScreen`, so the arm is unreachable during a
fold". The routing claim is true (`case CM_PEM: _refreshPemScreen()`,
screen.c:6166-6169; the `CM_AIM` case falls to `_refreshNormalScreen`,
screen.c:6183-6216) but it protects nothing, because the fold's forge is
**three statements wide**: `savedMode = calcMode; if(brk) calcMode = CM_PEM;
_tamProcessInput(item); if(brk && calcMode == CM_PEM) calcMode = savedMode;`
(ui/tam.c:1459-1465), and the bracket's own banner records that *nothing
inside it refreshes* (ui/tam.c:1440-1444). No paint happens under the forge
at all. What actually happens over an interactive capture during a TAM
session is that `calcMode` stays **CM_AIM** with `tam.mode != 0`
(ui/tam.c and the L1-F2 note at keyboard.c:1786-1795 both say so), the TAM
prompt paints on the T row (`Y_POSITION_OF_TAM_LINE` = the T line,
defines.h:1503), and refreshes are ordinary. `!tam.mode` is therefore the
real conjunct, and it is the same one `_refreshRegisterLine`'s own AIM arm
carries at screen.c:3871.

**Yield shape.** Every yield falls back to today's code verbatim:

```c
else {
  if(forthConsoleActive()) {             /* the gate above */
    forthConsoleRender();                /* transcript rows only */
  }
  else if(yMultiLineEdOffset == 3) {     /* today's block, untouched */
    refreshRegisterLine(REGISTER_T);
    refreshRegisterLine(REGISTER_Z);
    refreshRegisterLine(REGISTER_Y);
  }
  refreshRegisterLine(REGISTER_X);       /* untouched */
}
```

Pre-existing residue, named so N1-3 does not mistake it for a regression:
in the long-line editor state (`yMultiLineEdOffset != 3`) the yield branch
paints no Z row, so an error message is **already** invisible there today.
Out of scope; the N1-3 test asserts the transcript's error line, not the
native paint.

### Row arithmetic, per editor state

`showStringEdC47` (screen.c:1634) sets the state from the *current* string
before drawing: `yincr = 35` (screen.c:1655 — **not 21; the "ENLARGE 21"
comment is a relic**), and

- width in `numericFont` **> SCREEN_WIDTH-50** → `multiEdLines = 3`,
  `yMultiLineEdOffset = 1` (screen.c:1657-1662);
- else → `multiEdLines = 2`, `yMultiLineEdOffset = 3` (screen.c:1663-1668);
- `checkHP` → `multiEdLines = 1`, `yincr = 1` (screen.c:1670-1676) —
  **unreachable in the console**: `checkHP` requires `calcMode == CM_NORMAL
  || calcMode == CM_NIM` (defines.h:2378), so `checkHPoffset` (screen.c:386)
  is 0 and there is no third editor state to design for.

Then `if(lastline > yMultiLineEdOffset) { x = xMultiLineEdOffset; y =
(yincr-1) + yMultiLineEdOffset*(yincr-1); }` (screen.c:1680-1683), with
`lastline` = the `multiEdLines` the caller read *before* this call. So:

| state | `yMultiLineEdOffset` | editor draw origin | first ink / cursor top |
|---|---|---|---|
| short line | 3 | `y = Y_POSITION_OF_NIM_LINE - 3` = **129**, x=1 (the `lastline > 3` test is false for every reachable `lastline`) | cursor block runs `y-1 .. y+yincr` (screen.c:1712-1721) → **128** |
| long line | 1 | `y = 34 + 1*34` = **68**, x=0; wraps at +35 → 68, 103, 138 | **67** |

Transcript band = Y 24 .. (editorTop − 1); rows at 21 px, bottom-anchored to
the editor so the newest line sits directly above the input:

```
N       = (editorTop - 24) / 21          /* integer division; 0 is the graceful floor */
firstY  = editorTop - 21*N
row i   = firstY + 21*i                  /* i = 0 (oldest visible) .. N-1 (newest) */
```

| state | editorTop | N | rows |
|---|---|---|---|
| short line | 128 | **4** | 44, 65, 86, 107 |
| long line | 67 | **2** | 25, 46 |

**[CORRECTS THE STAGE DOC]** the authored text predicted "~4–5 rows,
possibly zero under the long-line editor". The measured answer is **4 and 2**
— the long-line state keeps two transcript rows, and no reachable state
yields zero. The floor stays in the formula as a guard, not as a case.

**Derive N from `yMultiLineEdOffset`, never by re-measuring the string.**
Both quantities above are functions of that one global, and the landed AIM
arm already trusts its value at exactly this point in the frame
(screen.c:5925). Re-measuring would put the console a frame out of step with
the editor on the call where the line crosses the long/short boundary; reading
the same global inherits the landed one-frame lag and nothing more.

### Paint-call inventory (what the console suppresses)

- Suppressed: `refreshRegisterLine(REGISTER_T/Z/Y)` at screen.c:5926-5928 — the
  only T/Z/Y paints reachable in CM_AIM.
- Not suppressed, and correct: `refreshRegisterLine(REGISTER_X)` :5930.
- Not reachable from CM_AIM, checked: the `LongpressKey_handler` T paints
  (screen.c:1035, 1044, 1052 — all `calcMode == CM_NORMAL`); the CM_NIM block
  (screen.c:5942-5948); `refreshRegisterLineRestoreT` (screen.c:5485, the
  `RESTORE_T` path).
- `clearRegisterLine(regist, clearTop, clearBottom)` (screen.c:2201-2218)
  clears `Y_POSITION_OF_REGISTER_X_LINE - 36*(regist-REGISTER_X)` for 32 px,
  −4/+4 for the flags, +3 more for X → X's full rect is **128..170**. The
  console never calls it (the area is already clear) but the rect is why
  `editorTop` is 128 and not 132 in the short state.

## N-T2 — the value formatter

**There is no single landed register→text entry.** `_refreshRegisterLine`
dispatches on `getRegisterDataType(regist)` and calls a different display.c
producer per type; the ones a console line can hit:

| type | producer | call site |
|---|---|---|
| dtReal34 | `real34ToDisplayString` | screen.c:4791 |
| dtComplex34 | `complex34ToDisplayString` | screen.c:4879 |
| dtShortInteger | `shortIntegerToDisplayString` | screen.c:5022, 5040, 5075 |
| dtLongInteger | `longIntegerRegisterToDisplayString` | screen.c:5190 |
| dtTime | `timeToDisplayString` | screen.c:5249 |
| dtDate | `dateToDisplayString` | screen.c:5284 |
| dtReal34Matrix / dtComplex34Matrix | `real34MatrixToDisplayString` / `vectorToDisplayString` / `complex34MatrixToDisplayString` | screen.c:5381-5384, 5449 |
| dtString | copied, not formatted | `COPY_REGISTER_STRING_TO` |
| fraction display | `fractionToDisplayString` | screen.c:3948 |

**Context-freeness: proven, by precedent.** `copyRegisterToClipboardString`
(screen.c:193-…, `#if PC_BUILD || DMCP_BUILD`) is a landed, complete,
**paint-free** all-types register→string function built from exactly this
family (screen.c:203-361). It is the wrong *semantics* for the console —
it produces full-precision CSV text, not the current display mode — but it
settles the question the stage doc asked ("whether it is callable outside a
paint"): the producers are pure string writers taking `(value, font,
maxWidth, digits, …)`, and the type switch has already been lifted out of
the paint once.

**Consequences for N1-3/N1-4, stated so the packet has no decisions left:**

- The console gets its own small dispatcher — call it
  `forthConsoleFormatX(char *out, int16_t outSize)` — modelled on the
  display-mode arms above, **not** on `copyRegisterToClipboardString`.
  `maxWidth` is `SCREEN_WIDTH - 1` at `&standardFont`; digits are
  `NUMBER_OF_DISPLAY_DIGITS` as the register paints use.
- **Format into a bounded local, never into `tmpString`.** display.c writes
  `tmpString` in 192 places; the console's caller (`.`, the ENTER echo)
  cannot know which producer aliases it. `copyRegisterToClipboardString`
  makes the same choice and records the reason at screen.c:197 — its buffer
  is off the stack "because the pair overran the DM42 stack grant", so the
  console's local is **128 B**, sized to a transcript line, not to
  `TMP_STR_LENGTH`.

**Truncation.** The landed idiom is at screen.c:4982-4983 (and :4995-4996):

```c
tmpStrW = stringAfterPixels(str, &standardFont, SCREEN_WIDTH - prefixWidth - 14 - 1, false, true);
xcopy(tmpStrW, STD_ELLIPSIS, 3);          /* 14 px is the ellipsis width */
```

`stringAfterPixels` is src/c47/charString.c:348, `stringWidth` is :341. The
console truncates transcript rows with this, unchanged.

**`EMIT`'s code space.** Glyph codes are one byte below 0x80 and two bytes
`0x80..0xFF` + low byte above it — the decode loop is screen.c:1725-1728
(`charCode = (charCode<<8) | (uint8_t)string[ch++]` when the MSB is set), and
fonts.h encodes them exactly so (`STD_DOLLAR "\x24"`, `STD_DOT "\x80\xb7"`,
`STD_UP_ARROW "\xa1\x91"`). So `EMIT` accepts **0x20..0x7E** (written as one
byte, ASCII-faithful per N-R5) and **0x8000..0xFFFF** (written high byte
first); everything else — including a bare 0x80..0xFF, which would be a
truncated glyph — is `ERROR_OUT_OF_RANGE`. Unknown-but-well-formed codes are
not the prim's problem: `showStringEdC47` already routes them through
`generateNotFoundGlyph` (screen.c:1734-1741).

**`.S` width reality.** The Forth data stack *is* the calculator stack plus
the D3 spill region (`forthSpillCount`, `forthDataDepthResync`,
forth_inner.c:186-200). A depth-prefixed four-level picture of real34s at
`standardFont` overruns 400 px on any two full-precision values, so `.S`
is specified as "depth first, then levels until the width runs out, then the
N-T2 ellipsis" — the depth is the part that must never be truncated away.

## N-T3 — prim mechanics and the collision sweep

**Mechanics confirmed.** `forthPrims[]` is a flash table of
`{name, flags, fn, stackEffect}` indexed by an append-only enum ending in
`PRIM_COUNT` (forth_prims.c:85-127); `_Static_assert(PRIM_COUNT <= 0x0FFF)`
bounds the `FTOK_PRIM` token. Lookup is `forthFindPrim` → linear
`compareString(forthPrims[i].name, name, CMP_BINARY)` (forth_dict.c:475);
invoke applies the stack delta first (`forthDataDepthApply`,
forth_inner.c:152-155). Appending seven entries is flash-only.

**Resolution order, confirmed at the source** (forth_compile.c): step 1 prims
:888, step 2 colon defs :1010, step 3 number :1036, step 4 C47 item :1090,
step 5 label :1404, undefined :1448. So a prim shadows the number grammar and
every item — which is what makes the sweep a gate.

### The sweep

Method: every one of the 2601 rows of the item table was parsed and its
`itemCatalogName` (field 3, src/c47/typeDefinitions.h:606) compared to the
seven candidates. 203 rows name themselves with glyph macros
(`STD_SQUARE_ROOT`, `STD_CROSS`, …) and were inspected separately — none can
equal an ASCII candidate. `forthFindItem`'s filter is
`CAT_FNCT && PTP_NONE && compareString(..., CMP_NAME)`
(forth_dict.c:602-614), and `CMP_NAME` is **case-sensitive** (it compares
glyph codes, folding only sup/sub/struck — src/c47/sort.c, the
`CMP_BINARY || CMP_NAME || CMP_COMMAND` branch).

| name | verdict |
|---|---|
| `.` | **clear** — no item, and not a number: `classifyNumber` returns `FORTH_NUM_NONE` when `mantissaDigits == 0` (forth_compile.c, the classify tail), so a bare `.` is an undefined word today and becomes the prim cleanly |
| `.S` | **clear** — the only dot-leading catalog names in the table are `.d` and `.ms` |
| `CR` | **clear** as a catalog name. Note `ITM_CR` exists as a *key-plane* item (the AIM f-plane of key 41, src/c47/assign.c:22) — a keyboard action, not a resolvable item name; no conflict |
| `EMIT` | clear |
| `SPACE` | clear |
| `PAGE` | clear |
| `TYPE` | **COLLIDES** |

**`TYPE` collides and must be renamed. [CORRECTS THE STAGE DOC]** Item 2402
is `{ fnGetType, NOPARAM, "TYPE", "TYPE", …, CAT_FNCT | … | PTP_NONE | … }`
(items.c:4368). It passes `forthFindItem`'s filter exactly, so **`TYPE`
resolves from a Forth line today** and runs `fnGetType`. A prim of that name
would silently change a landed, reachable meaning — precisely the §1.3
guardrail case, and the sweep's whole reason to exist.

**Ruling: the string-output word is `.$`.** It is free (the dot-leading scan
above), it reads as the `.`-family member it is, and it is typeable: `$` is
`STD_DOLLAR "\x24"` (src/c47/fonts.h:53), plain ASCII, reachable as
`ITM_DOLLAR` (items.c item 810). The Forth-83 name `TYPE` is recorded in the
fold-in as deliberately not taken, with the collision as the reason.

**Enum identifiers.** `PRIM_DOT` is **already taken** — by `STD_DOT` (`·`,
the multiplication dot, forth_prims.c:94), not by anything printing. The
seven new identifiers must not reuse it:

| word | identifier | flags | stack delta |
|---|---|---|---|
| `.` | `PRIM_PRINT` | 0 | −1 |
| `.S` | `PRIM_PRINTS` | 0 | 0 |
| `CR` | `PRIM_CR` | 0 | 0 |
| `EMIT` | `PRIM_EMIT` | 0 | −1 |
| `SPACE` | `PRIM_SPACE` | 0 | 0 |
| `.$` | `PRIM_PRINTSTR` | 0 | −1 |
| `PAGE` | `PRIM_PAGE` | 0 | 0 |

Indices 22..28, `PRIM_COUNT` → 29. No `FF_IMMEDIATE`, no `FF_DEFMARK`.

**Glyph-alias sweep:** none of the seven collides with the landed prim names
(`DUP DROP SWAP OVER + - * / × · ÷ RECURSE GLOBAL IMMEDIATE IF ELSE THEN
BEGIN UNTIL AGAIN WHILE REPEAT`).

**User-shadowing hazard, unchanged and documented:** a user colon definition
named `.` or `PAGE` in a restored dictionary loses to the new prim at
§4.1 step 1, silently. This is the upgrade note the fold-in records; it is
not testable against a user's dictionary and not fixable without a rule
change.

## N-T4 — the entry-state ladder and the home menu

### The two facts that drive everything else

**(1) Keys mode swaps the whole key plane.** `determineItem`'s AIM disjunct
carries `!(forthCapIsInteractive() && forthCapKeysMode())`
(keyboard.c:1785), and the normal-columns disjunct carries
`|| (calcMode == CM_AIM && forthCapIsInteractive() && forthCapKeysMode())`
(keyboard.c:1842). So in keys mode a key resolves through
`primary / fShifted / gShifted`, and in alpha mode through
`primaryAim / fShiftedAim / gShiftedAim` (keyboard.c:1770-1772, :1811-1813).

**(2) [CORRECTS THE STAGE DOC] Keys-first breaks the landed history
gesture.** L1-H's recall lives in `case CHR_caseUP:` / `case CHR_caseDN:`
(keyboard.c:2782-2825, `forthHistoryRecall(∓1)`), and `CHR_caseUP` is
reachable **only from the AIM f-plane** — key 51 is
`{51, ITM_UP1, ITM_BST, ITM_RBR, ITM_UP1, ITM_UP1, CHR_caseUP,
ITM_UP_ARROW, ITM_UP1}` and key 61 the same shape with `ITM_SST` /
`CHR_caseDN` / `ITM_DOWN_ARROW` (src/c47/assign.c:27, :32). In keys mode
f-up resolves to `ITM_BST` and f-down to `ITM_SST`. Today that is a footnote
because keys mode is an excursion. **After N3 it is the ground state, and
the console would open with its own history unreachable.** N1-5 must
re-home the gesture, not merely flip the bit.

### The gesture table for the interactive capture

Both keys must be identified **layout-independently** — the landed K1/E10
toggle sets the precedent by keying on `key->fShifted == ITM_AIM` rather
than a key number (keyboard.c:1798-1809). The console's rows are the ones
whose AIM f-column is `CHR_caseUP` / `CHR_caseDN`.

| chord | alpha input (today) | keys input (today) | ruling for an interactive capture |
|---|---|---|---|
| up / down | `ITM_UP1`/`ITM_DOWN1` → `fnKeyUp`/`fnKeyDown`, menu paging, else insert `↑`/`↓` (keyboard.c:2619-2668) | `ITM_UP1`/`ITM_DOWN1`, same | **KEEP** — menu paging is what a scrolling FWRD home row needs |
| f-up / f-down | `CHR_caseUP`/`CHR_caseDN` → FHIST recall | `ITM_BST`/`ITM_SST` (meaningless in a capture) | **NEW** — resolve to `CHR_caseUP`/`CHR_caseDN` in **both** modes, so recall survives keys-first |
| g-up / g-down | `ITM_UP_ARROW`/`ITM_DOWN_ARROW` → insert the arrow glyph | `ITM_RBR` / `ITM_FLGSV` (meaningless in a capture) | **NEW — this is the roll gesture**, both modes |

**The roll gesture is g-shifted up/down.** The runner-up (unshifted arrows)
is spent on menu paging, and f-shifted is spent on recall — recall and roll
are different gestures with different targets (the N1-6 battery asserts
exactly that). The cost is that `↑`/`↓` can no longer be typed with g-up
inside the console. That cost is bounded and was checked: the glyphs stay
reachable on the alpha MISC softmenu (`ITM_UP_ARROW, ITM_DOWN_ARROW`,
softmenus.c:711), and the item names that contain them — `R↑`, `R↓`
(items.c items 39/40), `↑Lim`, `↓Lim`, `STO↑`, `RCL↑` and 14 more — are
*typed by pressing their keys* in keys mode via the landed name-insert
divert (items.c:794-800), which is the mode the console now opens in. Nothing
outside an interactive capture changes.

### The EXIT ladder, re-derived row by row

The landed interactive arm is keyboard.c:4020-4093. Rung by rung, for
keys-as-ground with `-MNU_FORTH` as the home row:

| rung | landed | disposition |
|---|---|---|
| 1 — keys → alpha (keyboard.c:4029-4033: `forthCapSetKeysMode(false); showSoftmenu(-MNU_ALPHA);`) | first press leaves keys mode | **INVERT.** Keys is now the ground, so the first EXIT press must leave **alpha** and return to keys — and it must restore the FWRD home row, not push ALPHA. The rung becomes `if(!forthCapKeysMode()) { forthCapSetKeysMode(true); showSoftmenu(-MNU_FORTH); break; }` |
| 2 — pop anything stacked above the base (keyboard.c:4043-4047) | pre-normalises `currentMenu() == -MNU_ALPHA` → `softmenuStack[0].softmenuId = 1` (:4043), then tests `!(softmenuStack[0].softmenuId <= 1 && menu(1) != -MNU_ALPHA)` (:4044) | **NEW predicate.** Under FWRD-as-home the pre-normalisation never fires (`currentMenu()` is `-MNU_FORTH`), `softmenuStack[0].softmenuId` is FWRD's index — which is `> 1` — so the test pops **the console's own home row** instead of falling through to rung 3. The base test must read "slot 0 is the console's home" as well: `currentMenu() == -MNU_FORTH || currentMenu() == -MNU_ALPHA` normalises, everything else pops. This is the rung that silently breaks if the flip ships without it |
| 3 — close (keyboard.c:4084-4091: history push :4084, `forthCapClose()` :4086, buffer/cursor reset, `calcModeNormal()` :4090, `popSoftmenu()` :4091) | never `closeAim()` | **KEEP**, with one check: the `popSoftmenu()` at :4091 exists to remove the frame rung 2's rename did not pop. FWRD is pushed by `pushSoftmenu` exactly like ALPHA (softmenus.c:4161, via `showSoftmenu`), so the pop count is unchanged — one frame in, one frame out. The N1-5 test asserts the pre-FORTH menu is revealed |

**The open site.** `forthEnterAimSurfaceNoLift` (forth_compile.c:1672-1690)
is the only interactive open path; `showSoftmenu(-MNU_ALPHA)` at :1686 and
the `softmenuStack[0].softmenuId == 0 → 1` fixup at :1687 become
`showSoftmenu(-MNU_FORTH)` with the fixup **dropped** — it is the native
"MyMenu → MyAlpha in AIM" idiom (`popSoftmenu` does the same at
softmenus.c:3719-3721) and has no meaning for a non-alpha home row.
`keysMode = 1` is set *after* `forthCapOpenInteractive()`
(forth_compile.c:1726) and after the REPL reopen
(programming/manage.c:1405-1410), because `_forthCapOpenAs` zeroes the bit
unconditionally (forth_capture.c:11). The universal reset stays, so PEM
inherits alpha-first untouched.

**`isAlphaSubmenu` already counts `-MNU_FORTH`** (softmenus.c:3880-3891,
the forth-core row). Consumers: `isAlphabeticSoftmenu()` (softmenus.c:4191)
→ screen.c:913 (a status-bar/shift-display test), keyboard.c:1309, :1313,
:1380, :1489, :1535, :3910, :4162, and test_persist.part.h:538. The one that
matters is **keyboard.c:4162**, the CM_PEM EXIT arm — PEM-side, and PEM
still opens in alpha, so its behaviour is unchanged by N3. Disposition:
**KEEP** the row; the N1-5 battery asserts the PEM ladder rung-for-rung
against the K4 table.

**Catalog-drain predicates — re-audited for a console-pushed FWRD.**
`_forthCatalogMenuOnTop` matches CATALOG/FCNS/CONST/CHARS/PROGS/VARS/MENUS
and *not* FWRD; `_forthCatalogBuriedOnStack` scans for `-MNU_CATALOG` only
(M-T5, unchanged this pass). The drain in `fnForthOuter`
(forth_compile.c:1717) runs **only under `if(catalog)`** — the M-T5
correction of 2026-08-05 — and `catalog` is set by the FCNS/ASM machinery,
never by a plain menu row. The console's own `showSoftmenu(-MNU_FORTH)` at
:1686 happens *after* that loop and is therefore never a drain candidate.
The E13 resume drain (programming/manage.c:1336-1341) runs the same
predicates, and FWRD is not in either set. **Disposition: KEEP all three,
unchanged**; N1-5 adds the row "open the console over a CATALOG stack →
CATALOG drains, FWRD home is pushed and survives".

**The E13 resume guard.** `if(!forthCapKeysMode()) { showSoftmenu(-MNU_ALPHA); }`
(programming/manage.c:1342) restores the input surface after a TAM
suspension. It is correct for PEM as written. For an interactive capture it
would restore ALPHA over a console whose home is FWRD — but only on a path
where keys mode is off, i.e. an alpha excursion, where ALPHA *is* the right
row. **Disposition: KEEP, with an asserted row** rather than an edit: the
fold suspends an interactive capture (ui/tam.c:1181 → `forthFoldEnter`) and
resumes through this site, so N1-5 pins "fold from keys mode → resume in
keys mode with FWRD up" and "fold from an alpha excursion → resume in alpha
with ALPHA up". `keysMode` already rides the suspension
(programming/manage.c:1269, :1283), so both rows should pass unmodified;
if the second fails, the fix is here.

**K4 battery rows, flip vs assert.** The K4 pin ("a fresh capture always
opens in alpha input") is a battery default, not a ruling (§8.4.1). Rows
keyed on `FCAP_ORIGIN_INTERACTIVE` **flip** to expect `keysMode == 1` at
open and after every REPL reopen; rows keyed on `FCAP_ORIGIN_PEM` are
**asserted unchanged** and gain an explicit "still alpha" comment, because
they now pass for a reason (`_forthCapOpenAs`'s reset) that the interactive
rows deliberately override two lines later. Both leak directions get a class
test — a PEM capture opening in keys is a regression, an interactive one
opening in alpha is the feature dead.

## N-T5 — the channel's writers and seams

**Write contexts, all bounded, none painting.**

- The ENTER dialogue: `forthInteractiveEnter` (programming/manage.c:1363).
  The echo co-sites with `forthHistoryPush(aimBuffer)` at :1381 — after the
  empty no-op (:1364) and after the E9 refusal (:1373), so a refused line
  neither echoes nor enters history, which is the one-act property N-R4
  rests on. `forthOuterInterpret` runs at :1393; the error echo goes on the
  error arm (:1395-1403, which restores the line from the pre-run copy) and
  the result echo on the REPL arm (:1405-1410, `forthCapOpenInteractive()`
  at :1408). Both are view-only and neither touches FHIST.
- Prims under `forthOuterRun` (§3.3.2, private ctx, nesting ≤ 2) — a ring
  append is a bounded BSS write; `displayCalcErrorMessage` from prim context
  is landed precedent for strictly more (forth_prims.c:27).
- Program-step runs under `runProgram` — same call, same absence of paint.

**Reset seams — the complete list.** `forthCapPowerReset()` (forth_capture.c:73)
has exactly **two** production callers: `forthDictInit()` (forth_dict.c:57)
and `forthDictClear()` (forth_dict.c:71). The ring joins those two, and no
others: the ring must **not** be cleared at `forthCapClose`
(forth_capture.c:21) or `forthCapAbandonSuspended` (:51), because N-R2 rules
the dialogue survives capture close and reopen. `PAGE` is the third clear
site and is user-driven.

**The refresh path is a checked property, not a hope.** A completed ENTER
returns into the key-release path; `fnKeyUp` (keyboard.c:4811) ends in
`refreshScreen(131)` (keyboard.c:4825), which dispatches
`case CM_AIM: … _refreshNormalScreen()` (screen.c:6183-6216) → the console
arm. Nothing between the ring append and that call paints, so "no display
calls outside the view" holds by construction: the ring module includes no
screen.h include, and N1-1's test asserts the module's symbol table is free
of `lcd_`, `showString` and `refreshScreen`.

**RAM.** +1024 B BSS for the ring plus head/length/view-offset state. No
arena, no dictionary change, no program memory (FHIST is untouched). BSS
survives deep sleep, which is the whole lifetime story — the same one
`FLAG_ALPHA` has and `forthCapPowerReset` bounds.

## Adversarial check note (the T7.5 lesson applied)

Three claims in the authored stage doc would have shipped wrong if taken
from the write-set alone:

1. **"the forged CM_PEM makes the console arm unreachable during a fold."**
   True as routing, worthless as protection — the forge is three statements
   wide and brackets code that never refreshes (ui/tam.c:1440-1444,
   :1459-1465). The protection is `!tam.mode`, and a TAM session over the
   capture keeps `calcMode == CM_AIM` throughout. Read the bracket, not the
   switch.
2. **"audit `temporaryInformation` for the same swallow hazard the error
   line has."** The hazard is not symmetrical. The error line paints on a
   row the console suppresses; a live TI makes the X-row call the console
   *keeps* repaint all four rows from inside `_refreshRegisterLine`
   (screen.c:2573-2576). The conjunct is mandatory, and it was found by
   following the call the arm does **not** own.
3. **"the seven names, swept."** Six were clear and the seventh —
   `TYPE` — is a landed `CAT_FNCT | PTP_NONE` item, reachable from Forth
   today (items.c:4368). A sweep that had stopped at the prim table and the
   number grammar would have shipped a silent meaning change on the one name
   that had an owner.

And one from the ladder: keys-first does not merely flip a default, it swaps
the key plane (keyboard.c:1785 vs :1842), which strands the landed f-up
history recall on a plane the console no longer uses. The write-set of
`keysMode` — all 20 sites — says nothing about that; only the plane
selection does.

## What this sheet changes in STAGE_N_CONSOLE.md

| # | authored | traced |
|---|---|---|
| 1 | transcript rows "~4–5, possibly zero" | **4** (short line) / **2** (long line), derived from `yMultiLineEdOffset`; zero is a guard, not a reachable case |
| 2 | the fold is excluded by the forged CM_PEM | the fold is excluded by `!tam.mode`; the forge is too narrow to matter |
| 3 | TI arms "audited for the swallow hazard" | `temporaryInformation == TI_NO_INFO` is a **required gate conjunct** — the kept X-row call re-enters a four-row paint |
| 4 | `TYPE` is one of the seven words | `TYPE` collides with item 2402 (`fnGetType`); the word is **`.$`**. `PRIM_DOT` is also taken (`·`) — the identifiers are `PRIM_PRINT`/`PRINTS`/`CR`/`EMIT`/`SPACE`/`PRINTSTR`/`PAGE` |
| 5 | N3 flips a default | N3 also swaps the key plane, stranding the landed FHIST recall gesture; N1-5 must re-home `CHR_caseUP`/`CHR_caseDN` into keys mode or the console opens with no history |
| 6 | rung 2 "adopted verbatim" | rung 2's base predicate **must be re-derived** for `-MNU_FORTH`, or the first EXIT pops the console's own home row; rung 1 inverts; rung 3 keeps |

Packet order is unchanged (N1-1 ring → N1-2 view → N1-3 dialogue → N1-4
words → N1-5 keys-first → N1-6 acceptance): the ring has no dependency, the
view needs only N-T1, and N1-5's ladder work is the largest single risk and
benefits from the console being visible first.
