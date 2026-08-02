# §9 PEM-native Forth — independent adversarial audit

Audited object: commit `cc41b7320` ("PEM mode initial commit") — the squashed C1–C13
series plus the P-3 pending-tree work. Working tree is clean; HEAD == audited code.
All line numbers verified against HEAD on 2026-07-10. Gate used: `ninja -C build.sim`
+ `./build.sim/src/c47-gtk/c47 --headless` + `./build.sim/src/c47-gtk/r47 --headless`
(both print `FORTH SELF-TEST: ALL PASSED` — but see F6).

Severity scale: CRITICAL = silent wrongness or memory unsafety reachable from the
primary user flow; HIGH = silent wrongness on a plausible gesture; MED = robustness/
integrity defect; LOW = cosmetic/documented-behavior note.

---

## Findings, worst first

### F1 — CRITICAL — E1/E2 derive entry state from the WRONG STEP in the real keystroke flow; the natural multi-line authoring flow cannot continue a Forth region

**Mechanism.** Every PEM keypress funnels through `runFunction` → CM_PEM branch →
`addStepInProgram(func)` [VERIFIED: src/c47/items.c:718-753]. `addStepInProgram`
**pre-moves the cursor one step forward** before calling `insertStepInProgram`
[VERIFIED: packages/forth-core/programming/manage.c:1883-1886, gated
`!pemCursorIsZerothStep && !isAtEndOfProgram && !isAtEndOfPrograms`]. The E1 toggle
arm (manage.c:1434 `wasOn = forthEntryStateAtCursor()`) and the E2 in-region route
(manage.c:1458-1460) evaluate `forthEntryStateAtCursor()` **after** that pre-move,
i.e. from the step *following* the insertion point — not from the step the cursor
highlights, which is what DESIGN.md §9.4 specifies ("derived from **that step**",
the step the cursor lands on).

Compounding it: committing a Forth line with ENTER advances the cursor **past** the
committed step (`pemCloseAlphaInput`: `++currentLocalStepNumber; currentStep =
findNextStep(currentStep);` [VERIFIED: manage.c:1001-1002]). The spec's E4
parenthetical ("the cursor now sits on the just-committed source step, so
forthEntryStateAtCursor() is true" — DESIGN.md:1846-1848) is **false** for the
committed code.

**Failure scenario (the by-hand acceptance flow).** Author `»FORTH` + `: SQ2 DUP × ;`
+ ENTER. Cursor now sits on `END`. Press `2` to start the usage line `2 SQ2`:
`addStepInProgram` skips the pre-move (`isAtEndOfProgram(END)` true), E2 evaluates
state at `END` → not an ITM_FORTH step → false → the key falls through to
`pemAddNumber` (manage.c:1467-1472) and opens **RPN number entry**. There is no
cursor position that fixes this: BST back onto the definition line and the pre-move
advances to `END` before E2 evaluates. E2 only ever fires when the step *after* the
insertion point is a Forth step — inverted from spec for every boundary case
(§9.9 acceptance 2a/2b both fail in the real flow; 2a inverts mid-region:
cursor on an RPN step whose successor is a source step opens Forth capture).

E1 mis-derives too: with the cursor mid-program on the last RPN step before an
existing `»FORTH`, pressing FORTH computes `wasOn` from the *marker* (true) →
treated as closing → no capture, and the inserted marker shifts the parity of the
whole downstream chain.

**Why the test suite is green anyway.** Every E1/E2 test calls
`insertStepInProgram` directly with a hand-placed cursor
(test_dict_reloc.c:2311-2430, :2624+), bypassing `addStepInProgram`'s pre-move —
exactly the harness-bypasses-dispatch failure. The tests validate the helper against
the spec's cursor, not the flow's cursor.

**Fix.** Derive entry state from the **predecessor of the insertion point**: a new
read-only helper (forth_bridge.c) that finds the owning program start (programList
walk as in `forthMarkerTurnsOn`), walks to the step whose `findNextStep` ==
`currentStep`, and derives from that step (source → true, marker →
`forthMarkerTurnsOn(prev)`, else false; no predecessor / zeroth → false). E1 and E2
switch to it. This restores every §9.4 case: land-on-RPN→RPN, land-on-source→Forth,
land-on-»→Forth, land-on-«→RPN, and the post-ENTER continuation. Regression test
must drive `addStepInProgram` (see fix prompt FIX-2).

---

### F2 — CRITICAL — picker builder: stack buffer overflow + byte-wise (not glyph-wise) tokenization, reachable from the display refresh path

**Site.** `initVariableSoftmenu` case `MNU_FORTH`
[packages/forth-core/softmenus.c:1839-1930].

**Overflow.** `char tok[FORTH_TOKEN_MAX + 1]` (64 bytes) receives
`xcopy(tok, src + tokStart, tokLen)` with `tokLen` bounded only by the payload
length (≤255) [softmenus.c:1866-1872]. A source step containing a spaceless token
longer than 63 bytes **smashes the stack**. Because `-MNU_FORTH` is in the
rebuild-always disjunction [softmenus.c:3142], the builder re-runs on **every screen
refresh** while the menu is displayed — the overflow fires repeatedly. The
compiler's tokenizer rejects long tokens with `ERROR_INPUT_TOO_LONG`
[forth_compile.c:65-68]; the picker must skip them, not copy them.

**Mis-tokenization.** The scan advances byte-wise (`while (pos < len && src[pos] !=
' ') pos++`) where the spec mandates the §3.3.3 glyph-wise discipline
("advance only via stringNextGlyph, delimiter exactly 0x20" — DESIGN.md §9.6,
PEM_COMMITS C11). This is not hypothetical: `STD_ANGLE` = `"\xa2\x20"`
[src/c47/fonts.h:569] has 0x20 as its **second byte**. A definition `: A∡B … ;`
gets split inside the glyph and the picker lists the garbage name `A\xa2`; picking
it inserts a token that can never resolve.

**Fix.** Tokenize with `stringNextGlyph` exactly as forth_compile.c:58-64; skip
tokens > FORTH_TOKEN_MAX bytes (they cannot be `:` or a pickable ≤14-byte name).
Also: `#define FORTH_TOKEN_MAX 63` is duplicated at softmenus.c:6 (hygiene — hoist
to forth_dict.h or accept the dup with a comment).

**Minor deviations in the same builder:** hard `stepCount > 1000` walk cap
(softmenus.c:1856-1857) is an undocumented behavior change (definitions past step
1000 silently missing); `numberOfBytes = 1` start matches the MNU_PROG model ✓;
alloc/free discipline correct (free at initVariableSoftmenu:1659 covers it) ✓.

---

### F3 — HIGH — the PARAM_LABEL Forth/item fallback applies to all 15 PTP_LABEL ops, turning GTO/PGMSLV/PGMINT/Σn/Πn/f'(x)/… misses into silent execution

**Site.** `_executeOp`, PARAM_LABEL / STRING_LABEL_VARIABLE arm
[packages/forth-core/programming/lblGtoXeq.c:365-391].

**Mechanism.** The arm serves every op whose PTP class is PTP_LABEL: GTO, XEQ, LBL?,
f'(x), f"(x), PGMINT, PGMSLV, VARMNU, Σn, Πn, iΣ, iΠ, XEQ.SKP, PGMPLT, 42VRMNU
[VERIFIED: 15 rows grep'd in packages/forth-core/items.c]. When `findNamedLabel`
misses, the new code resolves through `forthResolveXEQ` — which also scans **every
CAT_FNCT item by catalog name** [forth_dict.c:330-340] — and then:
- `FORTH_XEQ_COLON` → `reallyRunFunction(ITM_FCALL, widx)`: **GTO 'SQ' calls the
  Forth word and falls through to the next step** (a branch becomes a call);
  **PGMSLV 'SQ' executes SQ immediately** instead of binding the solver target.
- `FORTH_XEQ_ITEM` → `reallyRunFunction(item, NOPARAM)`: **GTO 'ABS' executes ABS.**
Upstream behavior for all of these: `ERROR_LABEL_NOT_FOUND` halt. This also
corrupts §9.2's documented reachability story: "GTO over a definition fails as a
clean §9.7 halt" — today `GTO 'SQ2'` (word already compiled) silently *calls* it.

**Spec status.** DESIGN.md §4.2:1223-1224 does say "same fallback before
ERROR_LABEL_NOT_FOUND" for this arm with no op discrimination — the implementation
transcribed an under-specified spec sentence. The section title and all discussion
are XEQ-only; §4.2's own rationale ("no existing keystroke program silently changes
meaning") is violated by the current form. [DECISION NEEDED — recommend restricting
the fallback to `op == ITM_XEQ || op == ITM_XEQP1`, LBLQ keeping its upstream
INVALID path; spec amendment to §4.2 accordingly.]

Also note (perf, DM42-class): the CAT_FNCT scan is a linear pass over ~2800 items
executed on **every** program-step XEQ-by-name and on every label miss.

---

### F4 — HIGH — XEQ.SKP of a Forth word in PEM records a corrupted opcode (silent wrong step)

**Mechanism.** The tam.c colon-hit branch fires for `tam.function == ITM_XEQ ||
ITM_XEQP1` [packages/forth-core/ui/tam.c:943] and in PEM records via
`insertUserItemInProgram(tam.function, buffer)` [tam.c:964]. That helper writes the
opcode low byte as `func & 0x7f` [manage.c:1861] — the §9.10-item-4 upstream oddity.
`ITM_XEQP1` = 2223 = 0x08AF: low byte 0xAF → recorded as 0x88 0x2F = item 0x082F
(2095) — **a different instruction**, silently. For ITM_XEQ (=3, single byte) the
path is safe, which is exactly what §9.10-4 checked; the XEQP1 leg was missed.

**Failure scenario.** In PEM: XEQ.SKP → alpha `SQ2` (a compiled Forth word) →
recorded step decodes/executes as item 2095. Silent program corruption at entry
time; niche gesture but zero diagnostics.

**Fix.** In the package-owned manage.c override, correct the helper
(`& 0x7f` → `& 0xff`) — the write is wrong for *any* func with low byte ≥ 0x80;
document as the in-package resolution of §9.10 item 4. Byte-probe test with
ITM_XEQP1. (Alternative, if Stan prefers zero behavior delta on the helper:
special-case the tam.c call. The helper fix is strictly better.)

---

### F5 — HIGH (feature-absent) — §9.6 "Presentation" was never implemented: the picker menu is never pushed; acceptance item 3 fails end-to-end

**Evidence.** No `showSoftmenu(-MNU_FORTH)` anywhere in
packages/forth-core/programming/manage.c (the C13 edit is missing; only
`showSoftmenu(-MNU_ALPHA)` at :844 runs on capture open). The FWRD menu exists
(items.c:2003 row 213; softmenus.c:1043/:1237; defines.h:1429=23; disjunction
:3142; builder :1839) and its *mechanics* are unit-tested green, but the user has
no route to it during capture: `f`+`+` in alpha mode opens `-MNU_AIMCATALOG`
(kbd aim column, src/c47/assign.c:406), not the MENUS catalog. §9.9 acceptance 3
("picker sees an uncompiled word … picking inserts") cannot be performed on the
calculator.

**Fix.** The one-line C13 edit: in pemAlpha's capture-open branch after
`showSoftmenu(-MNU_ALPHA)` (manage.c:844), `if(tam.function == ITM_FORTH) {
showSoftmenu(-MNU_FORTH); }`, plus the REM-negative manual check (open a REM line,
assert FWRD does NOT appear). Depends on F1's fix to be meaningful (without E2,
capture rarely opens where the picker matters) and on F2 (the builder will then run
on every refresh — it must be memory-safe first).

---

### F6 — MED-HIGH (test integrity) — the self-test gate declares ALL PASSED while the arena allocator reports 49 overlap warnings and 7 free-accounting errors

**Evidence.** `./r47 --headless` run captured this session:
`grep -c "overlap discovered"` → **49**; `error:---->Memory freeing A/B (regions) …
N blocks at address X, but M were allocated` → **7** (freeList.c:212/:236); yet the
suite ends `FORTH SELF-TEST: ALL PASSED` and exits 0. The diagnostics cluster around
the writeTestProgram-based §9 tests (marker parity, entry-state, toggle, …).

**Mechanism (probable root).** `restoreTestProgram`
[test_dict_reloc.c:~2070-2105] hand-edits `freeMemoryRegions[0].sizeInBlocks` and
prunes regions by address heuristics instead of using the allocator's API; the free
list is left inconsistent, and subsequent `freeC47Blocks` calls (dict clears,
`resizeProgramMemory`) then detect overlaps. Whether or not any *feature-code*
double-free hides among the 56 diagnostics is currently **undecidable** — which is
the point: the gate cannot catch a real double-free introduced by a future commit.

**Fix.** (a) Restore program memory through the production API
(`resizeProgramMemory`) instead of region surgery; (b) make the harness fail on any
freeList diagnostic (end-of-suite free-list consistency walk: regions sorted,
non-overlapping, sum plausible); (c) add the §5.4/§9.9 arena high-water report to
the suite output (currently absent — the §9.9 "arena reporting rule" duty is unmet;
CLAUDE.md requires it with dictionary changes).

---

### F7 — MED — picker press guard doesn't verify the active menu is MNU_FORTH

`executeFunction` picker block [packages/forth-core/keyboard.c:992-1001] guards on
`CM_PEM && FLAG_ALPHA && tam.function==ITM_FORTH && item==ITM_NOP &&
dynamicMenuItem in range` but never checks
`softmenu[softmenuStack[0].softmenuId].menuItem == -MNU_FORTH`. Any *dynamic* menu
on top during Forth capture that yields `item == ITM_NOP` with a valid
`dynamicMenuItem` (e.g. an empty MyAlpha/MyMenu slot) routes into
`pickerInsertName()` and inserts that menu's label (or a stray space) into the
Forth line. The `dynamicSoftmenu[softmenuStack[0].softmenuId]` index is only saved
from out-of-bounds reads by the `dynamicMenuItem >= 0` short-circuit — brittle.
Fix: add the menu-identity conjunct. (Case-mapping at keyboard.c:100-104 is a pure
map, single-fire via executeFunction on release — verified correct.)

### F8 — LOW-MED — FCALL reject path leaves a cursor off-by-one at end-of-program

`insertStepInProgram`'s FCALL arm (manage.c:1549-1564) returns after
`displayCalcErrorMessage` without inserting. When invoked through
`addStepInProgram` with the cursor at END (pre-move skipped, post-move still runs —
manage.c:1888-1893), the cursor drifts one step back although nothing was inserted.
Cosmetic; surfaces as the highlight jumping up after a rejected `FCALL nn`.
Same-shape drift for any future rejecting arm — consider signalling "nothing
inserted" to addStepInProgram. DEFER.

### F9 — LOW — notes, accepted behaviors, hygiene

1. **Phantom marker on power-off mid-capture:** an empty open placeholder
   (len==0, byte-identical to a marker) persists in program memory if state is
   saved during capture; E3 never runs; downstream marker parity flips on restore.
   Display-only corruption of directions; runner unaffected. Document or clear on
   restore. DEFER.
2. **Transient on-screen parity flip during empty capture:** the placeholder counts
   as a marker for *later* markers' `forthMarkerTurnsOn` while capture is open
   (decode-side only, self-heals at first keypress). Cosmetic.
3. **softmenuStack persisted:** saveRestoreBackup.c:290/:890 hex-dumps
   `softmenuStack` (softmenuId indices). Saved states from before this commit point
   one menu off for ids ≥ 22 after the TAMFLAG shift. Inherent to the
   upstream-sanctioned procedure (softmenus.c comment :1023-1028) — accepted, now
   reported (C10's verify-and-report duty was never done).
4. **Interactive XEQ between STOP and R/S** bumps the generation (site A fires for
   any interactive XEQ) → next Forth step after resume clears the dict. Follows
   §9.3's two-site rule as written; sharp edge worth a manual note.
5. **Menu-key start of a paused program's subroutine** (VARMNU soft key while
   PGM_WAITING) bumps via site B (menuLabel path) → words defined before the pause
   are gone at the next source step. Also §9.3-as-written. Note in manual.
6. **`#define FORTH_TOKEN_MAX 63` duplicated** in softmenus.c:6 (also
   forth_compile.c:44). Hoist or comment.
7. **testInitVariableSoftmenu PC_BUILD shim** inside the softmenus.c override
   (:1941-1945) — a test hook in an override file; acceptable (PC-gated), but it is
   an unsanctioned-by-spec insertion; keep it marked.
8. **tam.c PEM item-scan behavior** (pre-series, commit 5122b4ca3): in PEM,
   `XEQ 'SIN'` *executes* the item rather than recording a step (tam.c:946-957).
   Out of this audit's 13-commit scope but adjacent; flag for a future review.
9. **In-PEM XEQ 'FORTH'** works as a toggle route: the item scan runs
   `runFunction(ITM_FORTH)` → CM_PEM funnel → addStepInProgram → E1. Verified.

---

## Axis-by-axis verdicts

### Axis 1 — names-only invariant: HOLDS (with F4 adjacent)
- The FCALL gesture funnels through the single `insertStepInProgram` entry
  (TAM completion → addStepInProgram → insertStepInProgram); the redirect arm
  precedes the PTP_NUMBER_16 switch and early-returns [manage.c:1549-1564]. Not
  compiled yet / stale widx / indirect → reject with
  ERROR_NON_PROGRAMMABLE_COMMAND, nothing written [tested:
  test_fcall_redirect_rejects_stale, real dispatch, byte-probe]. Define-later is
  served by the tam.c degrade path (`addStepInProgram(tamOperation())`,
  tam.c:1073-1074) which records the typed NAME.
- `forthDictNameByIndex` walk matches `forthFindColon`'s `count-1-n` indexing,
  refuses smudged entries [forth_dict.c:292-317]. Correct name for the widx in hand.
- Runner-side: XEQ-name steps resolve **fresh at run time** via `forthResolveXEQ`
  [lblGtoXeq.c:365-380]; widx never persists. Byte-probe test asserts no 0x8B 0x1B
  anywhere in program memory [test_fcall_redirect_records_name].
- No other writer of ITM_FCALL steps found (grep across overrides + upstream
  funnels). PASS — except F4's *opcode* corruption on the XEQP1 leg (a names-only-
  adjacent silent write of a wrong step), and F3 which changes what a persisted
  *name* resolves to for 13 non-XEQ ops.

### Axis 2 — entry-only toggle / derived state: FAILS in the real flow (F1)
- No persisted mode flag exists: no new system flag, no tam field, no static
  (verified by reading every §9 hunk; `tam.function` reuse mirrors the upstream REM
  idiom and is cleared/reset by every capture-opening path — REM :1424, LITERAL
  :1410, FORTH :1441/:1461 — so the decode transient exception at decode.c:834 and
  E3's guard at manage.c:991 cannot see a stale value during an active capture).
- Land-on-step derivation is correct **as a function** (forth_bridge.c:71-83, tested)
  and wrong **as wired** (F1: evaluated post-pre-move). Power-off/resume,
  scroll-away, GTO-into-region, SST: nothing persisted, all recomputed — PASS
  (modulo F9.1 placeholder-on-poweroff).
- SST boundary: one whole source line per SST press (runProgram(true,…) breaks after
  one executeOneStep) — PASS.

### Axis 3 — run-generation lifecycle: PASS
- Exactly two bump sites [lblGtoXeq.c:162, :904], R/S (`fnRunProgram` :294-305 →
  runProgram(false, INVALID_VARIABLE)) and SST excluded. Menu-start bump runs
  before `programRunStop = PGM_RUNNING` (:907) so the nested `fnExecute` (:917)
  cannot double-bump. STOP → R/S keeps the dictionary (same generation; lazy check
  only at forthProgramStep [forth_compile.c:35-40, :391]).
- Reset action is `forthDictClear` (frees the region — forth_dict.c:48-58), NOT
  forthDictInit; config.c:1945's `forthDictInit()` is the cold-boot §6.2 hook —
  correct division. No region leak. Wrap-safe equality check.
- Residual sharp edges: F9.4/F9.5 (spec-as-written).

### Axis 4 — §9.2 execute-in-place: PASS
- Arm matches the spec byte-for-byte [lblGtoXeq.c:860-867]; pointer math verified
  (executeOneStep consumes the 2-byte opcode :759-763; `*step++` lands on len;
  forthProgramStep gets `[len][bytes…]`). `return 1` == the 42STRING contract
  (fnSkip(0)); error halt is runProgram's `lastErrorCode` check before any advance
  [:936-957] — halts *at* the step; the arm never touches lastErrorCode.
- `forthProgramStep` is the §3.3.2 P-2 function verbatim (guard → copy → NUL →
  gen-check → interpret) [forth_compile.c:383-396]; private-copy rule honored.
- Marker len==0 is a run-time no-op ✓ (tested with X/dict/error probes).
- Compile-state lines are stack-neutral (emit-only paths in forthOuterInterpret
  [forth_compile.c:264-300]).
- Reachability leak: with F3 unfixed, `GTO 'WORD'` over a compiled definition
  *calls* it instead of the documented clean §9.7 halt — F3 must land for the
  documented failure mode to be true.

### Axis 5 — picker + menu: registration PASS, feature FAILS (F5, F2)
- defines.h 22→23 single-line override, end-comment present [defines.h:1429].
- Rows appended in matching order; TAMFLAG now index 23; `_Static_assert` +
  `test_static_menu_integrity` guard the off-by-one; fnOpenMenu/display boundary
  checks classify 22 as dynamic correctly.
- `-MNU_FORTH` **is** in the rebuild-always disjunction [softmenus.c:3142] — the
  acceptance-3 refresh line exists; irrelevant until F5 makes the menu reachable.
- 14-byte omit-don't-truncate ✓; dedupe ✓; qsort/pack per MNU_PROG ✓ (all
  unit-tested); »FORTH/FORTH« computed from scanner state at render time
  [decode.c:830-845], caller passes step+2 so `literalAddress-2` is the opcode ✓;
  capture-transient exception scoped to currentStep+FLAG_ALPHA+tam.function ✓.
- F2 (overflow + byte-wise scan) and F5 (never shown) above. F7 guard gap.

### Axis 6 — glyph path: PASS
- R47 alpha capture: `g`+`×` feeds ITM_CROSS whose itemSoftmenuName is STD_CROSS
  "\x80\xd7" [items.c row 855; assign.c:396]; `g`+`÷` feeds ITM_OBELUS whose
  itemSoftmenuName is STD_DIVIDE "\x80\xf7" [items.c row 857; assign.c:391] — both
  present as prim aliases [forth_prims.c:44-46: STD_CROSS→pMul, STD_DOT→pMul,
  STD_DIVIDE→pDiv]. Compiler tokenizer advances via stringNextGlyph, delimiter
  exactly 0x20 [forth_compile.c:58-64]. `: D2 2 ÷ ;` chain is sound end-to-end.
- Note: STD_DOT aliases to *multiply* (dot-product convention) — matches the
  earlier alias commit's intent; confirm that's wanted for `·`.

### Axis 7 — package/upstream hygiene: PASS with notes
- Working tree clean at HEAD; `src/c47/` byte-untouched.
- All §9 edits live in registered overrides/custom sources; meson.build registers
  manage.c, decode.c, softmenus.c (+defines.h header) [meson.build:2-3].
- Override diffs vs upstream contain *only* the §9 edits, except: the PC_BUILD test
  shim in softmenus.c (F9.7), the FORTH_TOKEN_MAX dup (F9.6), and a brace/indent
  reflow around keyboard.c:987-1002 (semantics-preserving, verified).
- items.h one-line MNU_FORTH define — the documented exception ✓.
- Vanilla (package-less) build not exercised this session — the defines.h override
  is additive-only so risk is low; re-verify on the next `make sim` without
  CUSTOM_PKG if wanted.

### Axis 8 — test integrity: MIXED
Strengths: E1/E2/E3/E5/FCALL/decode/picker-builder tests drive the real functions
with byte-level probes and named escaping mutations; runner tests go through
`executeOneStep` (real dispatch); registration tests pin the exact off-by-one.

Failures/gaps, in order:
1. **F6** — the gate ignores 56 allocator diagnostics; ALL PASSED is not currently
   trustworthy evidence of memory correctness.
2. **F1's escape** — E1/E2 tests bypass `addStepInProgram`; no test drives the
   keystroke funnel. Acceptance 2 has *no* real coverage; the suite green-lights a
   broken primary flow.
3. **Bump-site wiring untested** — test_program_step_gen_reset calls
   `forthRunGenBump()` by hand; nothing exercises fnExecute/runProgram gating
   (acknowledged in C4's prompt; still a gap — acceptance 9 is manual-only).
4. **No runProgram-level end-to-end test** for acceptance 1 (define-and-use across
   *steps* under the real run loop with halt semantics).
5. **Acceptance 10** (XEQ-by-name records a name) has no automated probe of the
   tam.c path (test_fcall_redirect_records_name covers the FCALL gesture only).
6. **Arena high-water report absent** (§5.4/§9.9 duty; CLAUDE.md requirement).
7. Picker press-path guard (F7 surface) untested — `pickerInsertName` is tested
   directly; the executeFunction guard conditions are not.
8. `make test` cleans CUSTOM_PKG (no Forth coverage); `ninja test` reportedly
   SIGSEGVs pre-existing in test_outer_glyph_divide [per PEM_COMMITS.md:63-65,
   C1's discovery; not re-verified this session]. Gate remains: build + two
   headless self-tests.

### §9.9 acceptance map (current truth)
| item | status |
|---|---|
| 1 define-and-use | unit-level only (executeOneStep/forthProgramStep); by-hand flow blocked by F1 unless single-line or double-toggle workaround |
| 2 derived keypad | **FAILS in real flow** (F1); helper-level tests green (bypass) |
| 3 picker | **FAILS** (F5 unreachable; F2 unsafe); builder/insert unit tests green |
| 4 marker display | unit-tested ✓ (decode probes); sim check pending |
| 5 glyph operators | chain verified ✓ (entry mapping + aliases + tokenizer) |
| 6 literal type parity | covered by pre-existing tests ✓ |
| 7 halt semantics | unit ✓ (error-set + no-advance logic verified in code) |
| 8 marker no-op / empty line | unit ✓ (E3 via real ENTER path) |
| 9 lifecycle | logic ✓, wiring untested; manual script required |
| 10 XEQ records name | mechanism verified in code; automated probe missing |
| arena report | **absent** |
