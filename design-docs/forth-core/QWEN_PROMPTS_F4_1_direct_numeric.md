# Stage F4-1 — parameter classification + direct numeric parameters

Origin: DESIGN §10.4 via the F4 trace/design pass (`QWEN_PROMPTS_F4_core.md`
§1-§2).  This packet replaces F3-6's blanket parameterized-item reject with
the Series-C classification (flow → `ERROR_OPERATION_UNDEFINED`; eligible →
parameter grammar) and lands the DIRECT NUMERIC grammar for
`PTP_NUMBER_8`, `PTP_NUMBER_16`, and `PTP_NUMBER_8_16`.  Register, flag,
shuffle, named, and indirect forms stay rejected until F4-2/F4-3 (interim
`ERROR_INVALID_NAME`, commented).

Traced facts this packet carries (QWEN_PROMPTS_F4_core.md §1, all verified
2026-07-18): the flow set `{ITM_END 1458, ITM_RTN 4, ITM_STOP 70,
ITM_RTNP1 1579}` is upstream's own `funcIsProgramStopControl` predicate
[src/c47/items.c:262] and all four are `CAT_FNCT|PTP_NONE` (they RESOLVE
from Forth source today; no test pins that dispatch); `ITM_CASE` 1418 is
flow inside PTP_REGISTER; `ITM_FCALL`'s param is a Forth dictionary index;
`tamMinMax` packs `(min << 14) | max` [defines.h:1018-1019]; TAM digit
entry is decimal, unsigned, leading-zero-insensitive [ui/tam.c:742-748];
SDL is item 423, `PTP_NUMBER_8`, min 0 max 99 [items.c:2216]; CNST is the
only `PTP_NUMBER_8_16` item, max NOUC−1 = 83 [items.c:1994,
defines.h:1100], so the `[250][ext]` cell is decoder/validator-only.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` prints `forth-core/pem-entry-fixes`;
   `git status --short` empty; `git log -1 --format=%s` is exactly:
   `forth-core: F3-7 — pin XEQ resolution parity and close stage F3`.
2. `grep -n "forthFindItemParameterized" packages/forth-core/forth_dict.c
   packages/forth-core/forth_compile.c` shows the definition and exactly
   one call site whose hit path raises `ERROR_INVALID_NAME` (the F3-6
   interim this packet replaces).
3. `grep -n "PTP_NUMBER_8_16" packages/forth-core/forth_inner.c
   packages/forth-core/forth_dict.c` → ZERO matches (class not yet
   decoded/validated).
4. `grep -rn "forthItemIsFlowReject\|forthParamParse" packages/forth-core`
   → ZERO matches.
5. `grep -n "paramCoreValidateDirect" packages/forth-core/programming/param_core.c`
   shows the landed F2-3/F2-5 function (NUMBER_8/NUMBER_16 arms).
6. `grep -n '"RTN"\|"STOP"' packages/forth-core/test_dict_reloc.c` → ZERO
   matches (no dispatch pins to migrate; any hit is a STOP).
7. Pre-gate green (capture `/tmp/forth-f4-1-pre.log`); record the
   two-region arena baseline from the F3-7 commit message.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`.  You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions.  If a quoted anchor, function, test, branch, literal, or identifier
does not match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`.  The tree must be clean before any edit.  Otherwise
   STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f4-1-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f4-1-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read `items.c`,
   `config.c`, `lblGtoXeq.c`, `forth_inner.c`, or `test_dict_reloc.c` in full.
   Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f4-1-todo.md`,
   `git status --short`, and `git diff` are the durable task state.  After any
   compaction or uncertainty, STOP the current step and re-read those sources;
   never reconstruct packet text from memory.

**Two-attempt debugger handoff.** After the implementation first fails a
required command because of your changes, make at most two distinct repair
attempts, each followed by the relevant rerun.  This does not override any
immediate STOP rule and does not apply to an expected mutation RED.  If the
second repair is not green, STOP and report `[SOL DEBUGGER HANDOFF]` with the
command, bounded failure output, both repairs/results, current status/diff,
and remaining hypotheses.

---

## F4-1 — flow steps reject; numbers become parameters

### Authority carried by this packet (no open choices)

1. **Flow predicate** (forth_dict.c, public; declared in forth_dict.h):

   ```c
   /* §10.4: control/declarative steps are not Forth-callable.  The PTP_NONE
    * subset is upstream's own funcIsProgramStopControl set (items.c);
    * CASE is flow inside PTP_REGISTER; FCALL's parameter is a Forth
    * dictionary index (names-only invariant).  Class rejects: label
    * declaration/target, step-relative jumps, skip-on-compare, key
    * declarations. */
   bool forthItemIsFlowReject(uint16_t itemId)
   {
     uint16_t ptp = (uint16_t)(indexOfItems[itemId].status & PTP_STATUS);
     if (itemId == ITM_END || itemId == ITM_RTN || itemId == ITM_STOP ||
         itemId == ITM_RTNP1 || itemId == ITM_CASE || itemId == ITM_FCALL) {
       return true;
     }
     return ptp == PTP_DECLARE_LABEL || ptp == PTP_LABEL ||
            ptp == PTP_SKIP_BACK || ptp == PTP_COMPARE ||
            ptp == PTP_KEYG_KEYX;
   }
   ```

2. **Step-4 rework** (forth_compile.c).  The item arm becomes, in order:
   1. `forthFindItem(buf, &itemId)` hit (CAT_FNCT+PTP_NONE): if
      `forthItemIsFlowReject(itemId)` → `ERROR_OPERATION_UNDEFINED`,
      abort-if-open, stop line (BEHAVIOR CHANGE: END/RTN/STOP/RTN+1 were
      dispatchable; gate item 6 proved no test pins that).  Otherwise the
      landed PTP_NONE emit/dispatch, unchanged.
   2. `forthFindItemParameterized(buf, &itemId)` hit: if
      `forthItemIsFlowReject(itemId)` → `ERROR_OPERATION_UNDEFINED`,
      atomic.  Else consume EXACTLY ONE token via `nextToken(ptok)`
      (missing → `ERROR_INVALID_NAME`, atomic) and dispatch on the class:
      - `PTP_NUMBER_8`, `PTP_NUMBER_16`, `PTP_NUMBER_8_16` → the digit
        grammar below;
      - every other eligible class → `ERROR_INVALID_NAME`, atomic, with
        the comment `/* F4-2/F4-3: register, flag, shuffle, named,
        indirect forms */`.
      DEFS_ONLY needs no carve-out: in a tail, the interpret-state gate
      already skipped the item token before this arm; inside a definition
      the pre-scan compiles normally (carry this as a comment).
3. **Digit grammar** (static helper in forth_compile.c):

   ```c
   /* Decimal, unsigned, leading-zero-insensitive (TAM parity).  Returns
    * false on any non-digit byte, empty token, or accumulated value
    * above TAM_MAX_MASK. */
   static bool parseParamDigits(const char *tok, uint16_t *out)
   {
     uint32_t v = 0;
     int16_t i = 0;
     if (tok[0] == 0) return false;
     for (i = 0; tok[i] != 0; i++) {
       if (tok[i] < '0' || tok[i] > '9') return false;
       v = v * 10u + (uint32_t)(tok[i] - '0');
       if (v > TAM_MAX_MASK) return false;
     }
     *out = (uint16_t)v;
     return true;
   }
   ```

   Bounds: `min = indexOfItems[itemId].tamMinMax >> TAM_MAX_BITS;
   max = indexOfItems[itemId].tamMinMax & TAM_MAX_MASK;`  Non-digit form →
   `ERROR_INVALID_NAME`; digits with value outside [min, max] →
   `ERROR_OUT_OF_RANGE`.  Both atomic (abort-if-open, stop line).
4. **Encode (compile state)** — after `forthDictEmit(FTOK_C47)` and
   `forthDictEmit(itemId)`:
   - `PTP_NUMBER_8`: one cell, low byte = value, high byte 0
     (`forthDictEmit((ftoken_t)value)` — value ≤ 255 by max ≤ 0x3fff and
     the class's one-byte nature; ADD a defensive `value > 0xFF → 
     ERROR_OUT_OF_RANGE` before emit);
   - `PTP_NUMBER_16`: one cell, LE value (`forthDictEmit((ftoken_t)value)`)
     — the LANDED encoding, unchanged;
   - `PTP_NUMBER_8_16`: value ≤ 249 → one cell `[value][0]`; 250..505 →
     one cell `[250][value-250]` (unreachable for CNST's max 83 but the
     encoder is written complete; the range check against max already
     capped the value).
5. **Interpret state** — dispatch through the SHARED direct core exactly
   like the FTOK_C47 arm (F2 parity discipline):

   ```c
   if (paramCoreValidateDirect(itemId, ptpClass, value)) {
     uint8_t savedRunStop = programRunStop;
     programRunStop = PGM_RUNNING;
     paramCoreDispatchDirect(itemId, value);
     if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
   }
   /* else: native-parity silence */
   ```

   plus the standard `lastErrorCode` gate (abort-if-open, stop).
6. **Shared core extension** (programming/param_core.c):
   `paramCoreValidateDirect` gains a `PTP_NUMBER_8_16` arm — value must be
   `<= (indexOfItems[op].tamMinMax & TAM_MAX_MASK)` (same shape as the
   NUMBER_8 arm; out-of-range = false = traced silence).
   `paramCoreDispatchDirect` needs NO change (it dispatches
   `reallyRunFunction(op, value)` for every direct class).
7. **Runtime decode** (forth_inner.c FTOK_C47 arm): add
   `case PTP_NUMBER_8_16:` — bounded-read one cell; `b0 = byte0`;
   `b0 <= 249` → `param = b0`; `b0 == 250` → `param = (uint16_t)(250 +
   byte1)`; `b0 >= 251` → `ERROR_INVALID_CORRUPTED_DATA` + INNER_LEAVE
   (254/255 join in F4-3); `ip += 2`.  The existing shared
   validate/dispatch tail then applies unchanged.
8. **Validator + GLOBAL walks** (forth_dict.c): `vBodyWalk`'s FTOK_C47 arm
   and BOTH F3-4 mark walks gain `PTP_NUMBER_8_16`: one cell; legal iff
   (`byte0 <= 249 && byte1 == 0`) or `byte0 == 250`; advance +2; anything
   else invalid.

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_compile.c`,
`forth_inner.c`, `programming/param_core.c`, `programming/param_core.h`
(only if a prototype is needed), `test_dict_reloc.c`.

### Targeted reads

1. forth_compile.c: the step-4 item arm and the F3-6 parameterized reject
   (grep `forthFindItemParameterized`), `nextToken`'s signature.
2. forth_inner.c: the FTOK_C47 arm only.
3. forth_dict.c: `vBodyWalk`'s FTOK_C47 arm; `forthDictMakeLatestGlobal`'s
   two walks (the same arm shape).
4. param_core.c: `paramCoreValidateDirect` and `paramCoreDispatchDirect`
   only.
5. test_dict_reloc.c: `run_word`, `x_is_longint`, the F2-5
   `seedParamParityState` helper and `read_reg_int32`, the g-helpers,
   `begin_word`/`emit_int16`, registration lines.

### New test `test_param_textual_numeric` (register after the newest test)

Fresh state.  Subcases (PASS lines exactly as quoted):

1. **NUMBER_8 interpret + leading zeros.**  Seed
   `seedParamParityState()`-equivalent state (X=44 on top), then
   `forthOuterInterpret("SDL 03")` → no error; record X.  Reseed
   identically; `forthOuterInterpret("SDL 3")` → identical X (the two
   spellings are one parameter).  Independent oracle:
   `paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, 99)` must be true and
   `paramCoreValidateDirect(ITM_SDL, PTP_NUMBER_8, 100)` false (direct
   core assertions, not parity).
   `[1] PASS: SDL 03 == SDL 3 with core-validated bounds`
2. **NUMBER_8 compile shape.**  `forthOuterInterpret(": PD1 SDL 02 ;")` →
   no error; byte-image assertion on PD1's transient body (name "PD1", 3
   bytes; body at `fdict.latest + TO_BLOCKS(6+3)*BYTES_PER_BLOCK` — a
   dictionary ENCODING assertion): cells `FTOK_C47`, `ITM_SDL` (build the
   expected array with the named constants, LE), `0x02 0x00`, `FTOK_EXIT`.
   `[2] PASS: SDL 02 compiles to the padded NUMBER_8 cell`
3. **Compiled/interpret parity.**  Reseed the full state; `run_word("PD1")`;
   reseed identically; `forthOuterInterpret("SDL 02")`; the resulting X and
   `lastErrorCode` must match (both halves reseeded — F2-5 rule).
   `[3] PASS: compiled and interpreted SDL 02 agree`
4. **NUMBER_16 first-match discovery + parity.**  Discover the FIRST
   ascending item id with `CAT_FNCT`, `PTP_NUMBER_16`, NOT
   `forthItemIsFlowReject(id)`, and NOT old-form (`!isFunctionOldParam16`);
   `break` on match, then independently rescan `1..id-1` and CONFIG-FAIL
   on any earlier match (F2-5 first-match proof).  Build the source line
   with `sprintf(srcBuf, "%s 5", indexOfItems[id].itemCatalogName)`.
   Interpret it and run its compiled twin (`: PD2 <same> ;` built the same
   way) with identical reseeding; X and `lastErrorCode` must match.
   `[4] PASS: first NUMBER_16 textual parameter agrees (item=%u)`
5. **CNST direct + extension-cell format.**  `forthOuterInterpret("CNST 07")`
   → no error (constant 7 loads; do not assert its value — assert
   `lastErrorCode == ERROR_NONE` and that X CHANGED from the 44 seed).
   `forthOuterInterpret("CNST 260")` → `ERROR_OUT_OF_RANGE` (max is 83;
   clear).  Extension-cell format is decoder/validator-only: g-build a
   global word whose body is `FTOK_C47`, `ITM_CNST`, raw cell
   `{0xFA, 0x0A}` (250, ext 10), `FTOK_EXIT` — run
   `forthGDictValidateRestored()` and require it to ACCEPT (base
   non-NULL), then FORGET-free cleanup via `forthGDictClear()`.
   `[5] PASS: CNST bounds enforced; [250][ext] cell validates`
6. **Flow rejects, both states.**  Each interpret line sets
   `ERROR_OPERATION_UNDEFINED` (clear after each): `"RTN"`, `"STOP"`,
   `"GTO"`, `"CASE"`, `"FCALL"`, `"BACK"`.  Compile:
   `forthOuterInterpret(": FR RTN ;")` → same error and
   `forthFindColon("FR", &r)` false (atomic).
   `[6] PASS: flow and declarative steps reject with OPERATION UNDEFINED`
7. **Malformed and missing parameters.**  `"SDL"` alone →
   `ERROR_INVALID_NAME`; `"SDL 1X"` → `ERROR_INVALID_NAME`; `"SDL 100"` →
   `ERROR_OUT_OF_RANGE`; `": FQ SDL ;"` → `ERROR_INVALID_NAME` + FQ
   unfindable.  One-token consume rule: seed X, `"SDL 02 7"` → no error
   and `x_is_longint(7)` (the 7 was ordinary source after the parameter).
   `[7] PASS: parameter errors are atomic; exactly one token is consumed`

Cleanup: both regions cleared, error state cleared.

### Existing tests

Untouched and green (gate item 6 proved the flow-reject changes pin
nothing existing).  If any legacy test reddens, STOP.

### Non-goals / STOP boundaries

- No register/flag/shuffle/named/indirect forms (F4-2/F4-3) — their arm
  is the commented INVALID_NAME interim.
- No change to XEQ (structural), no PTP_NONE dispatch changes beyond the
  flow reject, no entry-layer or save-format work.
- If an eligible NUMBER_16 item cannot be discovered in subcase 4, print
  a CONFIG FAIL and stop — do not substitute a hardcoded id.

### Gate and required mutations

Full gate green (seven PASS lines).  Mutations, each separately, verbatim
anchors, manual restore, `/tmp/forth-f4-1-mut1..5.log`:

1. In `forthItemIsFlowReject`, delete the four-id
   `funcIsProgramStopControl` clause (keep CASE/FCALL/classes).  Subcase
   6's `"RTN"` pin MUST go RED (RTN resolves and dispatches).  Green =
   STOP.
2. In the NUMBER_8 emit, write the pad byte as 1 (emit
   `(ftoken_t)(value | 0x0100)`).  Subcase 2's byte image MUST go RED.
3. In `parseParamDigits`'s caller, drop the `min` half of the bounds
   check (compare against max only).  Discover at runtime an eligible
   direct-numeric item with `(tamMinMax >> TAM_MAX_BITS) > 0`; if none
   exists, report `MUTATION 3: NOT DISCRIMINABLE (no min>0 item)` and
   restore — an escape valve, not a silent pass.  If one exists, add the
   corresponding probe line to subcase 7 BEFORE running mutations (value
   = min−1 → ERROR_OUT_OF_RANGE) and require it RED under the mutation.
4. In `vBodyWalk`'s NUMBER_8_16 arm, delete the `byte0 == 250` disjunct.
   Subcase 5's validator-accepts pin MUST go RED (the hand-built body
   resets).
5. In `paramCoreValidateDirect`'s NUMBER_8_16 arm, change `<=` to `<`.
   Subcase 5's `"CNST 07"`… does not discriminate; instead require the
   NEW direct assertion added to subcase 5:
   `paramCoreValidateDirect(ITM_CNST, PTP_NUMBER_8_16, 83)` must be true
   (boundary).  Under the mutation it MUST go RED.

Residue-free diff; final gate `/tmp/forth-f4-1-final.log`; report all
PASS lines, mutation symptoms, banners, exit 0, the two-region arena
line vs baseline, `git diff --check`, mirror equality.  RULE-1: flash
grows (classifier + parser) — record the `make dmcp5r47` delta in the
stage commit (PENDING if the owner is unavailable).

### Commit

```text
forth-core: F4-1 — flow steps reject; direct numeric parameters land
```
