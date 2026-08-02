# §9 PEM audit — per-commit fix prompts (Qwen)

Generated 2026-07-10 against HEAD = cc41b7320 (working tree clean). Every line
number below was verified against that commit. Line numbers may drift as earlier
fixes land — each prompt names its anchors textually; re-locate by anchor text
before editing.

**Reuse the COMMON PREAMBLE from PEM_COMMITS.md verbatim** (scope discipline,
upstream read-only, locked decisions, test/mutation rules, gate, STOP-no-commit).
Gate for every commit below, unchanged from the C1 discovery:
`ninja -C build.sim` then `./build.sim/src/c47-gtk/c47 --headless` and
`./build.sim/src/c47-gtk/r47 --headless`, expect `FORTH SELF-TEST: ALL PASSED`
on both — **plus, from FIX-6 onward, zero freeList diagnostics** (see FIX-6).

## Fix table (dependency-ordered)

| id | finding | files | must-fix class | tests (escaping mutation) |
|----|---------|-------|----------------|---------------------------|
| FIX-1 | F2 picker overflow + byte-wise scan | softmenus.c (override, slice) | before by-hand test (crash landmine once picker shows) | glyph-split probe (STD_ANGLE name) + long-token no-copy probe (mutation: byte-wise scan restored) |
| FIX-2 | F1 derived state at insertion point | forth_bridge.c + manage.c (override) + tests | **before by-hand test** | addStepInProgram-driven continuation + toggle-direction tests (mutation: derive from currentStep again) |
| FIX-3 | F3 PARAM_LABEL fallback over-broad | programming/lblGtoXeq.c (override) | before merge | GTO-'word' errors, XEQ-'word' still calls (mutation: drop the op gate) |
| FIX-4 | F4 XEQP1 opcode corruption | programming/manage.c (override, 1 line) + test | before merge | XEQP1 byte-probe (mutation: revert to & 0x7f) |
| FIX-5 | F5 missing §9.6 presentation | programming/manage.c (override, 1 line) | before by-hand acceptance 3 | manual sim script incl. REM-negative (mutation named in comment: unconditional push) |
| FIX-6 | F6 gate integrity + arena report | test_dict_reloc.c (custom only) | before trusting any gate | free-list consistency walk fails suite (mutation: restore region surgery) |

Deferred (tracked, not prompted): F7 picker-guard menu-identity conjunct
(keyboard.c), F8 reject-path cursor drift, F9.1 phantom marker on power-off,
F9.6 FORTH_TOKEN_MAX dup, builder 1000-step cap documentation, §4.2 spec-text
amendment (do together with FIX-3's landing — one DESIGN.md edit).

Ordering rationale: FIX-1 first (memory safety, independent, smallest risk);
FIX-2 unblocks the by-hand walkthrough; FIX-5 requires FIX-2 (capture must open
in the right places) and FIX-1 (builder runs on every refresh once shown);
FIX-3/FIX-4 independent correctness; FIX-6 last so its stricter gate covers the
re-tested suite. One override file per session throughout.

---

## FIX-1 — softmenus.c: glyph-wise, bounded picker tokenizer

**Goal.** Make the `MNU_FORTH` builder memory-safe and spec-conformant
(DESIGN.md §9.6 "Content build": tokenize with the §3.3.3 glyph-wise
discipline).

**Read first.** DESIGN.md §9.6 (:1904-1952), the compiler tokenizer
`nextToken` at packages/forth-core/forth_compile.c:58-72 (the model), and the
current builder at packages/forth-core/softmenus.c:1839-1930 (anchor: `case
MNU_FORTH: {`).

**File.** `packages/forth-core/softmenus.c` ONLY (existing override).

**The two defects you are fixing (state both in the report):**
1. `char tok[FORTH_TOKEN_MAX + 1]` receives an unchecked
   `xcopy(tok, src + tokStart, tokLen)` where tokLen ≤ 255 → stack overflow on
   any spaceless token > 63 bytes, re-fired on every display refresh via the
   :3142 rebuild-always disjunction.
2. The scan advances byte-wise (`src[pos] != ' '`); C47 glyphs are 1–2 bytes
   and `STD_ANGLE` = `"\xa2\x20"` (src/c47/fonts.h:569) has 0x20 as its second
   byte, so byte-wise splitting cuts tokens inside glyphs.

**Edit.** Rewrite the payload scan inside `case MNU_FORTH:` to walk glyph-wise:
- Copy the payload into a NUL-terminated local (`char line[256]`; len ≤ 255) so
  you can use the string helpers, then advance ONLY via
  `stringNextGlyph(line, pos)` exactly as forth_compile.c:58-64; delimiter is
  the single byte 0x20 **as a glyph position** (a 0x20 that is the second byte
  of a glyph is not a delimiter).
- Token boundaries: [start, pos) as in `nextToken`. If `pos - start >
  FORTH_TOKEN_MAX`, SKIP the token entirely (do not copy, do not truncate — a
  >63-byte token can be neither `:` nor a ≤14-byte name) and continue scanning.
- Keep the rest of the builder logic unchanged: `:`-then-name extraction
  (mid-line occurrences included), nameLen 1..14 else omit, dedupe, qsort with
  sortMenu, pack. Do not touch the 15-byte slot layout.
- Leave the `stepCount > 1000` cap but add a one-line comment stating the
  behavioral limit (documented deviation).

**Tests (test_dict_reloc.c, registered per the :1940-1975 pattern, cleanup per
the existing picker tests).**
1. `test_picker_glyph_tokenize` — source step `: A<0xA2><0x20>B DUP ;` (name
   contains STD_ANGLE; build the payload bytes by hand). Build the menu
   (testInitVariableSoftmenu(22)); assert menuContent contains the 4-byte name
   `A\xa2\x20B` and does NOT contain the 2-byte prefix `A\xa2`.
   Escaping mutation: restore the byte-wise `src[pos] != ' '` advance — the
   name splits and both assertions fail.
2. `test_picker_long_token_skipped` — a source step whose payload is one
   200-byte spaceless token followed by ` : SQ DUP ;` (so the line also
   contains a legitimate definition). Assert numItems == 1 and "SQ" present
   (the long token was skipped, not copied). This is also the overflow probe:
   run it under the existing test_asan configuration and report the ASAN
   result. Escaping mutation: restore the unchecked xcopy — ASAN build fails
   (stack-buffer-overflow); non-ASAN still passes the numItems assert, so name
   the ASAN run as the authoritative mutation check.
Run the mutation checks; report transcripts.

**Gate** per header. **Report and STOP.** One commit, no push.

---

## FIX-2 — derived state at the insertion point (the E1/E2 wrong-step bug)

**Goal.** Restore DESIGN.md §9.4's derived-state semantics in the *real*
keystroke flow: entry state must be derived from the step the new step will
FOLLOW (the step the cursor highlights), not from the step after the
`addStepInProgram` pre-move.

**Read first.** DESIGN.md §9.4 in full (:1732-1868), especially the
land-on-step invariant and E4's continuation note. Then read, in
packages/forth-core/programming/manage.c: `addStepInProgram` :1879-1893 (the
pre-move at :1883-1886), `insertStepInProgram`'s E1 arm :1429-1446 and E2 arm
:1458-1465, and `pemCloseAlphaInput` :990-1008 (the post-ENTER cursor advance
at :1001-1002). The bug: E1/E2 call `forthEntryStateAtCursor()` after the
pre-move, so the state comes from the step *following* the insertion point;
after ENTER the cursor sits on END and E2 never fires (the by-hand flow opens
RPN number entry instead of continuing the Forth region).

**Files.** `packages/forth-core/forth_bridge.c` (custom — new helper),
`packages/forth-core/forth_dict.h` (declaration),
`packages/forth-core/programming/manage.c` (override — two call-site swaps),
`packages/forth-core/test_dict_reloc.c` (tests). manage.c is the only override
touched.

**Edit 1 — helper (forth_bridge.c).** Add, next to `forthEntryStateAtCursor`
(:71-83):

```c
/* §9.4 (audit fix F1): entry state governing an INSERTION at currentStep.
 * The new step will follow the step immediately BEFORE currentStep, so derive
 * from that predecessor. currentStep may sit past the pre-move of
 * addStepInProgram (manage.c) or past the committed line after ENTER
 * (pemCloseAlphaInput) — in both cases the predecessor is the step the spec's
 * "cursor lands on" language means. */
bool forthEntryStateAtInsertion(void)
{
  if (pemCursorIsZerothStep) return false;

  uint8_t *progStart = NULL;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    if (programList[i].instructionPointer <= (uint8_t *)currentStep) {
      progStart = programList[i].instructionPointer;
    }
  }
  if (!progStart || progStart >= currentStep) return false;  /* top of program */

  uint8_t *prev = progStart;                 /* find predecessor of currentStep */
  for (;;) {
    uint8_t *next = findNextStep(prev);
    if (!next || next <= prev) return false; /* defensive: malformed walk */
    if (next >= currentStep) break;          /* prev is the predecessor */
    prev = next;
  }

  uint8_t len;
  if (!forthStepPayload(prev, &len)) return false;  /* RPN step: RPN */
  if (len > 0) return true;                          /* source step: Forth */
  return forthMarkerTurnsOn(prev);                   /* marker: its direction */
}
```

Declare it in forth_dict.h beside the other §9.4 helpers. Do NOT remove
`forthEntryStateAtCursor` (decode.c and existing tests use the at-cursor
semantics legitimately).

**Edit 2 — call sites (manage.c).** In `insertStepInProgram`:
- E1 arm (:1434): `bool_t wasOn = forthEntryStateAtCursor();` →
  `forthEntryStateAtInsertion()`.
- E2 guard (:1460): `&& forthEntryStateAtCursor()` →
  `&& forthEntryStateAtInsertion()`.
Nothing else. The FCALL arm and E3/E5 are untouched.

**Semantics you must verify by hand before writing tests (report each):**
(a) post-ENTER continuation: cursor on END, predecessor = the committed source
line → true → digit opens Forth capture; (b) cursor highlighted on an RPN step
inside a region (pre-move puts currentStep on the next step; predecessor = the
RPN step) → false (spec 2a); (c) cursor on a source step → predecessor is that
source step after pre-move → true (spec 2b); (d) cursor on `»FORTH` → true, on
`FORTH«` → false (spec 2c); (e) zeroth → false. If any of these disagrees with
§9.4, STOP and report [DECISION NEEDED] rather than adjusting the helper.

**Tests.** These MUST drive `addStepInProgram` (the real funnel), not
`insertStepInProgram` — that bypass is how the bug escaped the original suite.
Reuse the writeTestProgram infra + the state save/restore pattern of
test_toggle_inserts_marker.
1. `test_e2_continuation_after_enter` — program bytes: marker,
   `: SQ DUP * ;` source step (hand-built), then rely on writeTestProgram's
   END/.END. Place cursor on the END step (currentStep = the END opcode,
   currentLocalStepNumber accordingly, pemCursorIsZerothStep = false, alpha
   clear, aimBuffer empty, tam.mode 0) — this is exactly where
   pemCloseAlphaInput leaves the cursor after ENTER. Call
   `addStepInProgram(ITM_2)`. Assert: FLAG_ALPHA set, tam.function ==
   ITM_FORTH, aimBuffer == "2". Then clean up via pemAlpha(ITM_ENTER) —
   which also asserts the committed step is an ITM_FORTH source step with
   payload "2" (byte-probe). Escaping mutation: swap the two call sites back
   to forthEntryStateAtCursor — END derives false and the test fails with RPN
   number entry (FLAG_ALPHA clear).
2. `test_e2_not_inside_rpn_gap` — program: marker, source step, RPN step
   (ITM_sin), marker. Cursor ON the RPN step; `addStepInProgram(ITM_2)`; the
   pre-move puts currentStep on the closing marker, predecessor = the RPN step
   → assert NO capture (FLAG_ALPHA clear) and number entry began (aimBuffer[0]
   == '2' via pemAddNumber... verify what pemAddNumber leaves in aimBuffer and
   assert on that, or on FLAG_ALPHA alone if NIM state is awkward — state what
   you asserted). Escaping mutation: a naive "always derive from predecessor of
   the PRE-move cursor" (i.e. two steps back) — this case flips to capture.
3. `test_e1_direction_mid_program` — program: RPN step, marker(»), source,
   marker(«), END. Cursor ON the RPN step (predecessor semantics: insertion
   follows it, before the »). Call `addStepInProgram(ITM_FORTH)`. Assert the
   new marker was inserted between the RPN step and the old », that capture
   OPENED (FLAG_ALPHA set — predecessor is RPN → wasOn false → opening), and
   clean up the capture. Escaping mutation: the at-cursor derivation (state
   from the old » = true) suppresses the capture — assertion fails.
Run all three mutation checks (both directions).

**Gate** per header. **Report and STOP.** Explicitly confirm in the report: no
mode flag added; both call sites now use the insertion-point helper; decode.c
still uses forthEntryStateAtCursor (unchanged).

---

## FIX-3 — lblGtoXeq.c: restrict the PARAM_LABEL fallback to XEQ/XEQ.SKP

**Goal.** Stop GTO/PGMSLV/PGMINT/Σn/Πn/f'(x)/f"(x)/VARMNU/PGMPLT/42VRMNU/
XEQ.SKP-adjacent ops from silently *executing* a Forth word or a same-named
built-in item when their string label does not resolve. Upstream error
behavior returns for every op except XEQ and XEQ.SKP.

**Read first.** DESIGN.md §4.2 (:1214-1275) — note its rationale line "no
existing keystroke program silently changes meaning"; the current arm at
packages/forth-core/programming/lblGtoXeq.c:365-391 (anchor:
`forthXEQType_t res = forthResolveXEQ`); the PTP_LABEL op list (15 items —
grep PTP_LABEL in packages/forth-core/items.c).

**File.** `packages/forth-core/programming/lblGtoXeq.c` ONLY.

**Edit.** Inside the `opParam == STRING_LABEL_VARIABLE` arm, gate the fallback
legs on the op:

```c
getStringLabelOrVariableName(paramAddress);
uint16_t resolvedParam = (uint16_t)INVALID_VARIABLE;
forthXEQType_t res = forthResolveXEQ(tmpStringLabelOrVariableName, &resolvedParam);
bool_t forthFallbackOp = (op == ITM_XEQ || op == ITM_XEQP1);
if (res == FORTH_XEQ_LABEL) {
  reallyRunFunction(op, resolvedParam);
}
else if (res == FORTH_XEQ_COLON && forthFallbackOp) {
  reallyRunFunction(ITM_FCALL, resolvedParam);
  if(op == ITM_XEQP1 && programRunStop == PGM_RUNNING && lastErrorCode == ERROR_NONE) {
    currentReturnLocalStep++;
  }
}
else if (res == FORTH_XEQ_ITEM && forthFallbackOp) {
  reallyRunFunction(resolvedParam, NOPARAM);
}
else if (op == ITM_LBLQ) {
  reallyRunFunction(op, (uint16_t)INVALID_VARIABLE);
}
else {
  displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
  ... (keep the existing EXTRA_INFO block)
}
```

Behavior notes to verify and state: LBL? of a non-label (colon word or item
name) reports not-found exactly as upstream (it previously took the
COLON→INVALID leg only when the name was a colon word; now every non-label
lands in the LBLQ leg — confirm upstream parity for LBL? 'SIN'). GTO 'name'
with a real label is untouched (FORTH_XEQ_LABEL first).

**DESIGN.md amendment (same commit, doc-only):** §4.2's `_executeOp` bullet
gains "…for `op == ITM_XEQ || ITM_XEQP1` only; all other PARAM_LABEL ops keep
the upstream ERROR_LABEL_NOT_FOUND halt (audit fix F3)". Do not restructure
the section.

**Tests.**
1. `test_gto_word_errors` — define SQ via forthProgramStep (`: SQ DUP * ;`),
   hand-build a `GTO 'SQ'` step (opcode ITM_GTO + STRING_LABEL_VARIABLE +
   len + "SQ"), snapshot X, run executeOneStep on it; assert
   `lastErrorCode == ERROR_LABEL_NOT_FOUND` and X unchanged (the word was NOT
   called). Clear the error. Escaping mutation: remove the `forthFallbackOp`
   conjunct — GTO calls SQ, X changes (DUP* on whatever X held), error absent.
2. `test_gto_item_errors` — `GTO 'ABS'` step (no label ABS): assert
   ERROR_LABEL_NOT_FOUND, X unchanged. Same mutation.
3. `test_xeq_word_still_calls` (regression) — `XEQ 'SQ'` step with X=3 →
   executeOneStep → assert X == 9, no error. Escaping mutation: over-tighten
   the gate (drop ITM_XEQ) — this test fails.
Run mutation checks.

**Gate** per header. **Report and STOP.**

---

## FIX-4 — manage.c: fix insertUserItemInProgram's opcode low-byte mask

**Goal.** `insertUserItemInProgram` writes the opcode low byte as
`func & 0x7f` [packages/forth-core/programming/manage.c:1861] — wrong for any
func whose low byte ≥ 0x80. The P-3 tam.c hook routes ITM_XEQP1 (2223 =
0x08AF) through it [packages/forth-core/ui/tam.c:943, :964], recording
0x88 0x2F = item 2095 — a silently wrong program step. This resolves DESIGN.md
§9.10 item 4 in-package.

**Read first.** §9.10 item 4 (:2075-2080); §4.2 P-3 block (:1260-1275);
manage.c:1848-1877 (`insertUserItemInProgram`).

**File.** `packages/forth-core/programming/manage.c` ONLY (one line + comment).

**Edit.** :1861 `tmpString[opBytes++] =  func       & 0x7f;` →
`tmpString[opBytes++] =  func       & 0xff;  // audit F4: 0x7f masked low bytes ≥ 0x80 (e.g. ITM_XEQP1=0x08AF) — §9.10 item 4 resolved`
Then update DESIGN.md §9.10 item 4 (doc-only, same commit): mark RESOLVED with
the fix location.

**Tests.**
1. `test_useritem_xeqp1_opcode` — writeTestProgram minimal program; call
   `insertUserItemInProgram(ITM_XEQP1, "SQ2")` with sane PEM globals (pattern
   of test_fcall_redirect_records_name); byte-probe the inserted step:
   `0x88 0xAF 0xFD 0x03 'S' 'Q' '2'`. Escaping mutation: revert :1861 to
   `& 0x7f` — the probe reads 0x2F and fails.
Run the mutation check.

**Gate** per header. **Report and STOP.**

---

## FIX-5 — manage.c: the missing §9.6 presentation edit (C13)

**Goal.** Push the FWRD picker when Forth capture opens — the one-line C13
edit that was never made; without it §9.9 acceptance 3 is unreachable on the
calculator.

**Read first.** DESIGN.md §9.6 "Presentation" bullet (:1935-1938); manage.c
pemAlpha's capture-open branch (anchor: `showSoftmenu(-MNU_ALPHA);` at :844).
PREREQUISITES: FIX-1 (builder is memory-safe — it will now run on every screen
refresh while displayed) and FIX-2 (capture opens at the right places) must
already be in.

**File.** `packages/forth-core/programming/manage.c` ONLY.

**Edit.** Immediately after `showSoftmenu(-MNU_ALPHA);` (:844), add:
```c
if(tam.function == ITM_FORTH) {   // §9.6 presentation: picker on top of alpha
  showSoftmenu(-MNU_FORTH);       // (audit F5; EXIT pops back to alpha)
  // escaping mutation: pushing -MNU_FORTH unconditionally — a REM capture
  // would show FWRD; the manual script's REM-negative step catches it.
}
```
This branch runs only when capture OPENS (FLAG_ALPHA was clear), which is
exactly E1-opening and E2 — verify tam.function is set by every caller before
pemAlpha (E1 :1441, E2 :1461, REM :1424, LITERAL :1410) and state so.

**Tests.** Keystroke-bound; no honest headless unit test (menu stack + display
path). Provide the MANUAL SIM SCRIPT for Stan on
`./build.sim/src/c47-gtk/r47`:
1. f R/S (PEM) → f + (CATALOG) → FNCS → select FORTH → capture opens →
   ASSERT the FWRD menu is displayed on top (F-keys show previously authored
   words; empty first time).
2. Type `: SQ DUP * ;` ENTER, then press a digit (FIX-2's continuation) →
   capture reopens → ASSERT FWRD now shows SQ; press the SQ softkey → ASSERT
   `SQ ` lands at the cursor (mid-line pick per the C12 script).
3. REM-negative: EXIT capture; author a REM line (catalog → REM) → ASSERT
   FWRD does NOT appear (the named mutation).
4. EXIT pops back to the alpha menu (§9.6).
State explicitly that no automated test was added and why.

**Gate** per header (regression only). **Report and STOP.**

---

## FIX-6 — test_dict_reloc.c: make the gate honest (free-list integrity + arena report)

**Goal.** The current ALL-PASSED run emits 49 "free memory regions overlap"
warnings and 7 freeListFree accounting errors (freeList.c:212/:236) — the gate
cannot detect a real double-free. Repair the test-program memory restore to
use production APIs, make any allocator diagnostic fail the suite, and add the
§5.4/§9.9 arena high-water report.

**Read first.** test_dict_reloc.c: `writeTestProgram` /
`restoreTestProgram` / `cleanupTestProgram` (:2044-2160 region; anchor
`static void restoreTestProgram`). The upstream allocator:
src/c47/core/freeList.c (read `freeListFree`'s two diagnostic sites and
whatever counters/globals exist there — discover, don't assume). CUSTOM
SOURCES ONLY — do not edit freeList.c or any upstream file.

**Edits.**
1. Rewrite `restoreTestProgram` to stop hand-editing `freeMemoryRegions[]`:
   restore by the production path — write the pristine 4-byte program at the
   current beginOfProgramMemory, fix firstFreeProgramByte/freeProgramBytes,
   then call `resizeProgramMemory(1)` (or the minimal legal block count —
   discover what config.c reset uses) and `scanLabelsAndPrograms()`. If
   resizeProgramMemory cannot shrink safely from test state, STOP and report
   [DECISION NEEDED] with the exact obstacle — do not reinstate the surgery.
2. Add a suite-final `test_freelist_consistent`: walk
   `freeMemoryRegions[0..numberOfFreeMemoryRegions)`, assert blockAddress
   strictly increasing and no region overlapping the next
   (`addr + size <= next.addr`), and assert no region overlaps program memory
   (`TO_C47MEMPTR(beginOfProgramMemory)`). Register it LAST in
   forthDictSelfTest.
3. If freeList.c exposes a diagnostic counter (discover in step "Read first"),
   snapshot it at suite start and assert unchanged at suite end; if it does
   not, state so and rely on (2).
4. Arena report (§5.4/§9.9 duty): at suite end print
   `FORTH ARENA: dict here=%u sizeBlocks=%u  freeRam=%d` using fdict and
   `getFreeRamMemory()` before/after the define-and-use tests; assert the dict
   region ≤ 2 KB (the §5.4 budget) and include the numbers in the report.

**Tests / mutation.** The escaping mutation for (1)+(2): temporarily
reinstate the old region surgery (keep it in a `#if 0` block for the check) —
`test_freelist_consistent` must FAIL and the freeList diagnostics must
reappear in the log; then remove it and confirm zero diagnostics
(`./r47 --headless 2>&1 | grep -c "overlap discovered"` → 0) and ALL PASSED.
Report both transcripts. From this commit on, the gate is:
build + both headless runs + **zero freeList diagnostics** + ALL PASSED.

**Gate** per header, upgraded as above. **Report and STOP.**

## FIX-6 addendum — core/freeList.c: double-free / invalid-free guard

**Goal.** FIX-6 made the gate honest about *existing* free-list corruption
but the allocator itself had no defense against a double free: `freeListFree`
had no guard at all upstream, and an earlier interim guard in the override
was defective in three ways — (a) its size-grow branch could extend a free
region over an adjacent allocated block, turning a detected error into
silent corruption; (b) it matched only exact `blockAddress == C47RamPtr`,
missing a double free of an address since coalesced into a larger free
region; (c) it was silent on device builds (all PC-only diagnostics compiled
out under `#if !defined(DMCP_BUILD)`).

**Fix.** `freeListFree` gains an unconditional double-free / invalid-free
guard using range-overlap detection: any free whose
`[address, address+size)` overlaps an existing `freeMemoryRegions[]` entry is
rejected — loud (`errorf` + backtrace) in PC builds, silent-but-safe on
device builds where diagnostics are compiled out, and it **never mutates the
list** on a hit. Upstream's `>=` overlap diagnostics (`freeListReduce`,
`freeListFree`) are RETAINED at full sensitivity: they are deliberate
double-free detectors (upstream's own comment: "This suggests there was
double-free!"), and the guard removes the duplicate-insertion cause rather
than relaxing the detector to `>`. Diff to upstream
(`diff src/c47/core/freeList.c packages/forth-core/core/freeList.c`) is the
guard hunk only.

**Root causes found and fixed in forth-core (Step 4 of the guard task).**
Rerunning the full self-test suite with the corrected (loud) guard surfaced
one real, previously-silent double free: `test_xeq_precedence()`
(test_dict_reloc.c:1284) reallocated `labelList` via `reallocC47Blocks`
(which frees the pre-expansion block internally on success —
`freeListRealloc`, core/freeList.c:90), then on both its success and failure
paths restored the pre-realloc `savedLabelList`/`savedNumLabels` snapshot —
reinstating a pointer to memory the realloc had already freed. The next
`scanLabelsAndPrograms()` call then double-freed it. Fixed by rescanning from
the (unmodified) program memory instead of restoring the stale snapshot
(test_dict_reloc.c:1319-1333, 1339-1353). This root cause was entirely
forth-core's own test helper — no upstream code was implicated. No other
"Memory freeing C" or restored `>=` overlap occurrences were found in the
clean suite run.

**Tests.** `test_freelist_double_free_guarded` (exact-address double free),
`test_freelist_interior_double_free` (double free of an address coalesced
into the interior of a larger region), `test_freelist_no_mutation_on_oversize_free`
(double free with a larger size must not grow the region). Each verified
empirically against its escaping mutation (guard-loop disabled, guard
reverted to exact-match, old size-grow branch re-added respectively): FAIL
under the mutation, PASS on revert.

**Candidate upstream MR** — see `PROPOSED_SPEC_CHANGES.md` (the guard is not
Forth-specific; it protects the shared C47 allocator used by GMP reals,
config.c, register data, and the Forth dictionary alike).
