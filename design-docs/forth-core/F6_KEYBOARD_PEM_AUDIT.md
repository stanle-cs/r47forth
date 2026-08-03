# F6 keyboard/PEM audit — the charter (HARD PRECONDITION of DESIGN §10.6)

Status: **TRACES FOLDED (T1-T7) → `F6_AUDIT_RESULTS.md` 2026-07-19;
BENCH (Blocks A-F) converted from hardware to an AUTOMATED SIM BENCH by
owner ruling 2026-08-02 — derivation in §6 below, packets SB-1/SB-2.**
The F6 packets are AUTHORED from the traces (`QWEN_PROMPTS_F6_core.md`
+ F6-1..F6-6); §10.6 was amended to record the split precondition.  The
bench below remains QUEUED: it runs on the DM42n AFTER F6-6 lands,
against the LANDED behavior (B1-B4 against the NEW suspend/resume
contract, not the wrapper's), and the stage cannot close until every
block has recorded observables and divergences are triaged.  The
original charter text follows unchanged as the bench script.

*(Original status, superseded 2026-07-18/19: OPEN — audit not yet run;
authoring before the full audit was forbidden.  The deferred-bench
register in `F6_AUDIT_RESULTS.md` records, per experiment, the
trace-derived interim substitute and the residual hardware risk that
motivated keeping the bench as the stage-exit gate.)*

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

## 6. Sim-bench derivation (owner ruling 2026-08-02 — replaces the hardware run)

The bench's purpose was to validate, end to end and key by key, what the
T1-T7 traces could only claim from source. The deferred-bench register
(`F6_AUDIT_RESULTS.md`) records the residual hardware risk per row: it is
"none" for every row except DMCP display timing (A1/A2) and DMCP save
timing (A5) — the sim runs the same C on every other axis. The owner
therefore converts the bench to headless self-test subcases that drive the
**key-dispatch layer** (`pemAlpha`, `fnKeyExit`, `tamEnterMode`,
`showSoftmenu`, catalog paths) with `testProg_t` fixtures and byte-image
golden asserts — the same idiom as `test_capture_acceptance`. Overlap with
the landed seam-level batteries is intentional (the bench pins the
*sequence through the keys*, not the seam), never a reason to skip a row.

**Row disposition.** COVERED = a landed test already pins the row's
observable at key level; the packet's EXECUTION GATE greps the named
assertion so the claim is machine-checked at run time, not trusted. NEW =
a bench subcase to author (packet named).

| Row | Disposition |
|---|---|
| A1 open/type/commit/exit | COVERED — `test_capture_acceptance` subcase 1 (gate-grep) |
| A2 reopen, cursor ×2 left, mid-line insert, ENTER | **NEW → SB-1** (byte golden) |
| A3 backspace across a two-byte glyph | **NEW → SB-1** |
| A4 196-glyph cap; 197th press; ENTER of full line | **NEW → SB-1** (also re-tests the cap premise, DESIGN_AUDIT Part 3 rotation) |
| A5 half-typed line, save/restore round trip | **NEW → SB-1**; text survives (committed per key, T1 arm 7), cursor/open-flag loss asserted as the contract. DMCP save timing stays HARDWARE-ONLY |
| A6 EXIT with half-typed line, reopen | **NEW → SB-1** |
| B1 TAM open + cancel back, line/cursor intact | **NEW → SB-2** (thin sequence assert over `test_capture_suspend`'s seam pins) |
| B2 tam.colon set + cancel — no leak into capture keys | **NEW → SB-2** |
| B3 XEQ 'NAME' committed from inside capture — step placement | **NEW → SB-2** (byte golden) |
| B4 FCNS catalog pick during capture | COVERED — `test_capture_menus` + `test_pem_xeq_dynmenu_no_live_exec` (gate-grep) |
| C1 STO 0 5 during capture → canonical text | COVERED — `test_capture_param_text` (gate-grep) |
| C2 STO . 0 5 local form; STO cancel path | **NEW → SB-2** |
| C3 native STO →05 / STO 'VAR' byte images | COVERED — F4 parity tests (`test_param_register_flag`, parity sweep; gate-grep) |
| D1 FWRD pick mid-capture, buffer+cursor after | COVERED — `test_picker_insert_at_cursor` (gate-grep) |
| D2 picker open, up/down navigation — no capture-line leak | **NEW → SB-2** |
| D3 menu transitions, which menu on top | COVERED — `test_alpha_menu_on_top_during_capture`, `test_forth_toggle_from_catalog_leaves_alpha_menu` (gate-grep) |
| E1 FWRD contents dictionary-backed, scope-aware | COVERED — `test_word_catalog` (gate-grep) |
| F1 full EXIT ladder, one level per press (§8.4 E8 table) | **NEW → SB-1** |
| F2 backspace on empty capture line, before/after commits | **NEW → SB-1** |

**HARDWARE-ONLY (leaves the binding queue; best-effort on device, DM42
stance):** DMCP display timing (A1/A2 residual), DMCP power/save timing
(A5 residual), physical keyboard reachability (T7's f-shift CAPS/NUM lock
on real keys). Nothing else.

**Packets:** `QWEN_PROMPTS_SB_1_capture_mechanics.md` (A2-A6, F1, F2 — 7
subcases) and `QWEN_PROMPTS_SB_2_nesting_param_menus.md` (B1-B3, C2, D2 —
5 subcases). Exit criteria for row 11i: both packets landed green with
their mutations RED, and the COVERED gate-greps all matched.
