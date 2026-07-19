# F6 keyboard/PEM audit — the charter (HARD PRECONDITION of DESIGN §10.6)

Status: **OPEN — audit not yet run.**  Authored 2026-07-18 by the
architect as the enabling artifact for stage F6.  DESIGN §10.6 forbids
authoring ANY F6 implementation packet before a dedicated keyboard/PEM
audit with hardware-derived tests exists; this document is that audit's
charter: the architect's trace plan, the owner's hardware experiment
checklist, and the fixture-derivation map that turns recorded
observations into packet fixtures.  When every experiment below has
recorded observables and the architect has folded them into
`F6_AUDIT_RESULTS.md` (created at audit time), the F6 packets may be
authored — not before.  Authoring F6 packets from this charter alone
would repeat the F1-5 P0 defect at stage scale.

Division of labor (CLAUDE.md): **A** = architect (code traces, fixture
derivation, packet authoring), **S** = Stan (hardware runs on the
DM42n/R47 target; simulator cross-checks where stated).

## 1. What F6 will build (scope recap, DESIGN §10.6)

1. Capture as a REAL PEM-style submode: real key paths, catalogs,
   parameter entry, cancel, cursor, softmenus, alpha transitions; the
   sole semantic difference is the sink (source text, not an RPN step).
2. A managed, relocation-safe source buffer held only while capture is
   active (replacing the `aimBuffer` wrapper model).
3. Nested ordinary alpha capture that suspends/restores the FULL capture
   state — including `tam.colon` (in the `tam` struct since b8f79e486).
4. The dedicated Forth word catalog (2026-07-16 fold): callable colon
   words per the F3 scopes (current program → interactive → global) on
   the §8.6 softmenu machinery, for browsing/insertion during capture
   and FCALL/XEQ access outside it.

Regression canaries already in place: the F15-2 entry-state and F15-3
display-parity suites, the F15-4 capture-drive contract, and (post-F5)
the E9 commit-gate tests.  Every F6 packet will inherit them.

## 2. Architect trace plan (A, against the post-F5 tree, before the bench day)

Each trace lands as a section of `F6_AUDIT_RESULTS.md` with file:line
evidence, in the F2/F4 trace style:

- T1. The complete `pemAlpha` arm map: every `item ==`/`func ==` branch,
  which arms are Forth-aware today, which consult
  `forthEntryStateAtInsertion()`, and the exact
  placeholder-insert/edit/close seams (anchors already known:
  manage.c:824-836, 859-871, 924-936, 1000-1042).
- T2. The TAM state machine surface: every `tam` field written during
  parameter entry (`mode/function/alpha/colon/dot/indirect/value/
  digitsSoFar/keyInputFinished/value0`), entry/exit points
  (`tamEnterMode`/`leaveTamModeIfEnabled`), and which fields the F6
  suspend/restore must snapshot (the full set, enumerated, with the
  b8f79e486 `tam.colon` note).
- T3. Softmenu stack behavior during AIM/capture: `softmenuStack`
  push/pop discipline, `-MNU_ALPHA`/`-MNU_FORTH` interplay
  (§8.6/§8.4 rows), `_closeAlphaMenus`, and the EXIT semantics table of
  §8.4 (which level pops vs closes capture).
- T4. The catalog machinery to be reused: `initVariableSoftmenu`
  MNU_FORTH case (P-H5), `dynmenuGetLabel`, the rebuild-always
  disjunction (softmenus.c:3039 class), the FCNS catalog path a user
  takes to reach an item during alpha, and `menu_alphaMisc`
  (quote/colon reachability — F4 traced ITM_QUOTE there).
- T5. Cursor/scroll: `T_cursorPos` glyph discipline,
  `displayAIMbufferoffset`, `scrollPemBackwards`,
  `firstDisplayedStep` maintenance around capture open/close.
- T6. The buffer lifecycle to be replaced: every `aimBuffer` read/write
  in the capture paths (machine-enumerated), the 196-glyph/<256-byte
  cap (manage.c:887), and the allocation seam where a managed handle
  would live (allocC47Blocks + TO_PCMEMPTR discipline, §5.2 model).
- T7. Key dispatch reachability in AIM: assign.c's alpha remaps (the
  f-shift → CAPS/NUM lock fact of §8.4), which physical keys can reach
  SST/BST/parameter entry mid-capture today, and the `ITM_AIM`
  first-key contract (F15-4).

## 3. Hardware experiment checklist (S, on the DM42n/R47; one bench session)

Record for EVERY experiment: the exact keystroke sequence executed, the
display after each press (photo or transcription of the relevant line),
the resulting program listing (PEM view), and — where marked [DUMP] — a
`backup.bin` save immediately after, so A can extract program bytes.
Simulator cross-checks marked [SIM] are run by A afterward for
divergence notes; hardware is authoritative.

**Block A — capture mechanics (baseline behavior to preserve)**
- A1. Open capture on an empty program (»FORTH marker), type `3 4 +`,
  ENTER, EXIT.  [DUMP]  (Pins: placeholder timing, commit, lock, exit.)
- A2. Reopen the committed line (cursor on it, the edit gesture), move
  the cursor left twice, insert a glyph mid-line, ENTER.  [DUMP]
- A3. Backspace across a two-byte glyph (type `×` from the alpha keypad,
  then backspace once) — does one press remove the whole glyph?
- A4. Type to exactly 196 glyphs (script provided by A with a repeated
  key) — what happens on the 197th press?  On ENTER of the full line?
  [DUMP]
- A5. Power off mid-capture with a half-typed line; power on.  Where is
  the cursor, what is in the buffer, is capture open?  [DUMP]
  (Extends §8.9 item 2(d) to the un-committed case.)
- A6. EXIT with a half-typed line (per the §8.4 EXIT table), then
  reopen.  What survived?

**Block B — alpha nesting and tam.colon (the suspend/restore contract)**
- B1. During capture, invoke an ordinary-alpha flow: press XEQ (opens
  TAM), type a name WITHOUT the colon, EXIT/cancel back to capture.
  Is the capture line intact?  Cursor position?
- B2. Same, but with the TAM `:` (local-label) entry — set `tam.colon`,
  cancel back.  Does the capture line survive; does any colon state
  leak into subsequent capture keys?
- B3. XEQ 'NAME' COMMITTED from inside capture (complete the TAM):
  what lands in the program relative to the open capture line?  [DUMP]
- B4. From capture, open the FCNS catalog, pick a parameterless item
  (e.g. SIN): today's behavior — text inserted? step recorded? catalog
  cancelled?  [DUMP]

**Block C — parameter entry during capture (F4 interplay)**
- C1. During capture press STO, then digits 0 5: what does today's
  wrapper do key by key?  [DUMP]
- C2. Same with STO . 0 5 (local form) and STO ENTER-cancel paths.
- C3. Outside capture (plain PEM), record `STO →05` and `STO 'VAR'`
  steps natively.  [DUMP]  (A extracts the byte images as F6 fixture
  gold and cross-pins the F4 encodings.)

**Block D — catalogs and menus during capture**
- D1. Open the FWRD picker (§8.6) mid-capture with two authored words;
  pick one; EXIT.  Buffer content and cursor after?
- D2. With the picker OPEN, define nothing and press each softkey row
  navigation key (up/down): any state leak into the capture line?
- D3. MyMenu / MNU_ALPHA transitions during capture: open, navigate,
  EXIT back.  Which menu is on top after each press? (Feeds T3.)

**Block E — the word catalog's future contents (F3 interplay)**
- E1. After running a program that defines words and marks one GLOBAL
  (script from A once F3 lands), open FWRD interactively OUTSIDE any
  program: which names appear today (expected: none — the picker scans
  authored text, not the dictionary)?  This pins the DELTA the F6
  catalog adds (dictionary-backed, scope-aware) versus the landed
  text-scan picker.

**Block F — cancel/undo edges**
- F1. During capture, press EXIT at each §8.4 table level in sequence
  and record which level each press pops (the roach-motel escape).
- F2. Backspace on an EMPTY capture line (the marker-delete rule) —
  before and after committing earlier lines.  [DUMP]

## 4. Fixture derivation map (A, after the bench day)

| Experiments | Derived fixtures | Consuming packet (predicted) |
|---|---|---|
| A1-A4, F2 | key-sequence → byte-image golden pairs for the submode's sink; cap behavior pins | F6-2 (submode key dispatch) |
| A5-A6 | suspend/power-off state-machine pins | F6-3 (suspend/restore) |
| B1-B4 | tam snapshot set + nesting drive fixtures (extends the F15-4 contract) | F6-3 |
| C1-C3 | parameter-entry-to-text goldens; native step images cross-pinning F4 | F6-4 (parameter entry sink) |
| D1-D3, E1 | softmenu stack pins; catalog content/refresh fixtures | F6-5 (word catalog) |
| T6 + A4 | managed-buffer contract (size, relocation seams, handle) | F6-1 (source buffer) |
| all | end-to-end capture acceptance battery | F6-6 |

Predicted packet count stays 5-6 (`FSERIES_ROADMAP.md`); the real
decomposition is decided from the results, not this prediction.

## 5. Exit criteria

The audit is COMPLETE when: every Block A-F experiment has recorded
observables (with dumps where marked); every T1-T7 trace section exists
with file:line evidence; A has written `F6_AUDIT_RESULTS.md` folding
both; and any behavior that contradicts a landed design assumption has
been triaged (design amendment vs upstream report vs F6 scope).  Then —
and only then — the F6 packets are authored against the post-F5 tree,
under the same binding rules (`QWEN_PROMPTS_F3_core.md` §0) as every
other stage.
