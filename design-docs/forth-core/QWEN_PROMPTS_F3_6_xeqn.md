# Stage F3-6 — XEQ source forms, FTOK_XEQN, and the B3 parameterized-item reject

Origin: DESIGN §10.3/§2.2 + R6_RESOLUTION_PLAN §2 (B2/B4 rulings) via the F3
design pass (`QWEN_PROMPTS_F3_core.md` D6).  This packet lands the
`XEQ 'NAME'` / `XEQ :NAME:` source forms, the `FTOK_XEQN` token end to end
(emit, runtime dispatch, restore-validator arm, GLOBAL-walk arm), and the
B3 rulings in both directions.  The compile-state label seam this replaces
is the documented §3.3.6 interim (label in compile state →
`ERROR_INVALID_NAME`).

Traced facts this packet relies on (verified 2026-07-18, `b5d794df4`):
- kind bytes ARE the resolver selectors: `findNamedLabel(name, kind)` with
  253 = global scan, 249 = position-sensitive local scan against
  `currentProgramNumber`/`currentLocalStepNumber`
  [src/c47/programming/manage.c:1863-1912];
- `ITM_FORTH` (2842) is `CAT_FNCT | PTP_REM` [packages/forth-core/items.c:4722]
  and `ITM_XEQ` (3) is `CAT_FNCT | PTP_LABEL` [src/c47/items.c:1786] — so
  the B3 reject set is EXACTLY the parameter classes
  `PTP_DECLARE_LABEL(1<<9) .. PTP_MENU(12<<9)`; `PTP_NONE`, `PTP_LITERAL`,
  `PTP_REM`, `PTP_DISABLED` stay outside it [packages/forth-core/defines.h:1056-1071];
- the landed C-1 label dispatch shape (`dynamicMenuItem = -1;
  fnExecute(label);`, no wrap) [packages/forth-core/forth_compile.c:499-500].

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F3-5 commit
   `forth-core: F3-5 — compile-time control flow over the landed branch
   tokens`.
2. `grep -n "0x7F05..0xFFFF reserved" packages/forth-core/forth_dict.c`
   matches exactly one line (vBodyWalk's reserved arm).
3. `grep -rn "FTOK_XEQN\|forthXeqnDispatch\|forthParseXeqForm" packages/forth-core`
   → ZERO matches.
4. In `forth_compile.c`, the compile-state label arm still raises
   `ERROR_INVALID_NAME` (grep `label ids RENUMBER` or read the step-5
   branch; the interim reject must still be present — it is what this
   packet replaces).
5. `grep -n "expected ITEM.*ITM_FCALL\|FCALL.*expected ITEM" packages/forth-core/test_dict_reloc.c`
   locates `test_xeq_item_lookup`'s Test 2 (the row B3 migrates).
6. Pre-gate green; arena baseline from the F3-5 commit.

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
   `/tmp/forth-f3-6-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-6-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f3-6-todo.md`,
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


**PROGRAM-FIXTURE AUTHORING RULE (mandatory)**

`test_dict_reloc.c` program fixtures are structural, not hand-addressed.
Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
the returned step handle when a test must execute or inspect that step, and
resolve it with `tpStepAddr`; abort the subcase if fixture construction,
`tpWrite`, or address lookup fails.

Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
`tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
authors must identify steps by role (for example `sSource` or `sXeq`) and
must not publish a calculated byte offset as a normative literal. If a
packet contains such an offset, stop with `[SOL DEBUGGER HANDOFF]` and report
the packet defect; do not repair its arithmetic locally.

Use a typed builder accessor such as `tpSrcPayload` for an internal field.
If the needed step or field helper does not exist, extend the central fixture
builder first; do not introduce local pointer arithmetic in the test.

Prefer named opcode/parameter constants in builder helpers. An exact byte
array may remain as the expected value of an encoding assertion. Raw bytes
inserted into the program fixture are allowed only for the encoding under
test or a deliberate malformation; they must enter through `tpRaw`, carry an
adjacent comment naming that purpose, and still use the returned handle and
builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
behavior fixture.

This rule is prospective. Do not widen the task by converting untouched
legacy fixtures.

---

## F3-6 — names cross the boundary as names, kind-faithfully

### Authority carried by this packet (no open choices)

1. **Token define** `#define FTOK_XEQN 0x7F04 + 1` — NO: define it
   literally as `0x7F05` beside the existing token constants in ALL THREE
   files that carry them (forth_compile.c, forth_inner.c top block,
   forth_dict.c validator block).  Inline layout (normative, §2.2):
   `[kind][len][name bytes][pad]`, `inline = 2 + len`,
   `padded = (inline + 1) & ~1` (pad byte present iff len is odd, always
   written 0), kind ∈ {`STRING_LABEL_VARIABLE` 253,
   `LOCAL_LABEL_VARIABLE` 249}, len 1..`FORTH_NAME_MAX` (31).
2. **Form parser** (forth_compile.c, static):

   ```c
   /* 'NAME' -> kind 253; :NAME: -> kind 249.  The closing delimiter must be
    * the LAST GLYPH (a two-byte glyph whose second byte merely equals the
    * delimiter is not a close).  Name bytes pass through raw (C47 glyphs
    * legal; 0x20 is inexpressible in a token by construction). */
   static bool forthParseXeqForm(const char *tok, uint8_t *kind,
                                 char *name, uint8_t *lenOut)
   {
     int16_t len = (int16_t)strlen(tok);
     char delim = tok[0];
     if (len < 3 || (delim != '\'' && delim != ':')) return false;
     {
       int16_t p = 0, last = 0;
       while (tok[p] != 0) { last = p; p = stringNextGlyph((char *)tok, p); }
       if (last != len - 1 || tok[last] != delim) return false;
     }
     {
       int16_t nlen = len - 2;
       if (nlen < 1 || nlen > FORTH_NAME_MAX) return false;
       memcpy(name, tok + 1, (size_t)nlen);
       name[nlen] = 0;
       *lenOut = (uint8_t)nlen;
     }
     *kind = (delim == ':') ? LOCAL_LABEL_VARIABLE : STRING_LABEL_VARIABLE;
     return true;
   }
   ```

3. **Structural `XEQ` branch** (forth_compile.c, after the F3-4 FORGET
   branch, BEFORE the DEFS_ONLY gate — structural like `:`/`;`/`FORGET`,
   deliberately unshadowable; bare `XEQ` resolves as nothing today, so
   this collides with no live meaning):

   ```c
   if (compareString(buf, "XEQ", CMP_BINARY) == 0) {
     char xtok[FORTH_TOKEN_MAX + 1];
     char xname[FORTH_NAME_MAX + 1];
     uint8_t xkind, xlen;
     if (!nextToken(xtok)) {
       if (isDefinitionOpen()) abortDefinition();
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;
       continue;
     }
     if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
       continue;                     /* tail XEQ is execution, not a mark */
     }
     if (!forthParseXeqForm(xtok, &xkind, xname, &xlen)) {
       if (isDefinitionOpen()) abortDefinition();
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;               /* B3: only the canonical spellings exist */
       continue;
     }
     if (state == STATE_COMPILE) {
       if (!emitXeqn(xkind, xname, xlen)) {
         abortDefinition();
         lineOK = false;
       }
     } else {
       uint16_t colonRef;
       forthXeqnResult_t r = forthXeqnDispatch(xname, xkind, &colonRef);
       if (r == FORTH_XEQN_COLON) {
         forthInner(colonRef, programRunStop == PGM_RUNNING);
       }
       if (lastErrorCode != ERROR_NONE) {
         lineOK = false;
       }
     }
     continue;
   }
   ```

   with the emitter:

   ```c
   static bool emitXeqn(uint8_t kind, const char *name, uint8_t len)
   {
     uint8_t buf[2 + FORTH_NAME_MAX + 1];
     uint16_t inlineBytes = (uint16_t)(2 + len);
     uint16_t padded = (uint16_t)((inlineBytes + 1) & ~1u);
     buf[0] = kind;
     buf[1] = len;
     memcpy(buf + 2, name, len);
     if (padded > inlineBytes) buf[inlineBytes] = 0;
     if (!forthDictEmit((ftoken_t)FTOK_XEQN)) return false;
     return forthDictEmitBytes(buf, padded);
   }
   ```

   The old step-5 compile-state reject
   (`ERROR_INVALID_NAME` on a label hit in compile state) REMAINS for
   bare names — bare names stay global-only INTERPRET dispatch, and a
   bare label name in compile state still cannot be baked.  (The §3.3
   error-table row survives for bare names; only the explicit `XEQ` forms
   compile.)
4. **Shared dispatch** (forth_inner.c, public; enum + prototype in
   forth_dict.h):

   ```c
   typedef enum { FORTH_XEQN_DONE, FORTH_XEQN_COLON, FORTH_XEQN_ERR } forthXeqnResult_t;
   forthXeqnResult_t forthXeqnDispatch(const char *name, uint8_t kind, uint16_t *colonRef);
   ```

   Body, in exactly this order (B4 matrix + B2 chain):
   1. `calcRegister_t label = findNamedLabel(name, kind);` — the stored
      kind byte passes VERBATIM; position-sensitivity is inherited, not
      re-implemented.  Hit → `dynamicMenuItem = -1; fnExecute((uint16_t)label);`
      (never the PGM_RUNNING wrap — ITM_XEQ is the run-loop driver;
      program-context calls take fnExecute's nested continuation branch
      unchanged) → return ERR if `lastErrorCode` else DONE.
   2. Miss with kind 249 → `displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, …)`,
      return ERR.  **Kind-faithful: a local request never falls back.**
   3. Miss with kind 253 — the B2 chain:
      prim (`forthFindPrim`; dispatch `fn()` + `clearSystemFlag(FLAG_ASLIFT)`;
      ERR/DONE per `lastErrorCode`) → colon
      (`forthFindColonRef(name, &ref, &fl)`; out via `*colonRef`, return
      COLON — the CALLER dispatches) → item (`forthFindItem` — the §4.1
      step-4 set, CAT_FNCT + PTP_NONE; dispatch `reallyRunFunction(itemId,
      NOPARAM)` under the PGM_RUNNING save/set/restore wrap; ERR/DONE) →
      `ERROR_LABEL_NOT_FOUND`, ERR.
5. **Runtime arm** (forth_inner.c, new `case FTOK_XEQN:` before the
   default): bounded-read 2 via the region-aware helpers; read
   `kind = innerBase(curG)[ip]`, `nlen = innerBase(curG)[ip+1]`; reject
   kind ∉ {253, 249} or nlen ∉ 1..31 with `ERROR_INVALID_CORRUPTED_DATA`
   + `INNER_LEAVE()`; compute `inlineBytes/padded` per authority 1;
   bounded-read `padded`; if a pad byte exists it must be 0 (same error);
   copy the name NUL-terminated into a local `char xname[FORTH_NAME_MAX+1]`;
   `ip += padded;` then `forthXeqnDispatch(xname, kind, &colonRef)`:
   COLON → exactly the FTOK_CALL dispatch shape (rsp overflow guard →
   ERROR_RAM_FULL; push ip + current region bit; `curG` from the ref;
   `ip = bodyOffsetOfRef(colonRef)` with the FORTH_NULL corrupted-data
   reject); otherwise fall through the standard `lastErrorCode` gate.
6. **Validator arm** (forth_dict.c `vBodyWalk`, replacing the reserved
   reject for 0x7F05 ONLY — 0x7F06.. stays reserved): bounds-check 2,
   read kind/len; kind ∈ {253, 249} and len 1..31 or invalid; compute
   padded; bounds-check padded; if len is odd require the trailing pad
   byte to be 0; `pos += padded`.  **Same arithmetic in the F3-4 GLOBAL
   validate walk and rewrite walk** (`forthDictMakeLatestGlobal`): the
   validate walk ACCEPTS the XEQN operand (names resolve fresh at run
   time — a global body may carry any XEQN) and advances by the same
   padded count; the rewrite walk advances identically (nothing to
   rewrite inside XEQN).
7. **B3 forward** (forth_compile.c, immediately after the step-4 item
   miss, before step 5): new helper in forth_dict.c

   ```c
   /* CAT_FNCT items whose PTP class is a parameter class (1<<9 .. 12<<9).
    * PTP_NONE/PTP_LITERAL/PTP_REM/PTP_DISABLED are OUTSIDE the set:
    * ITM_FORTH (PTP_REM) must keep resolving through the reverse path. */
   bool forthFindItemParameterized(const char *name, uint16_t *itemId);
   ```

   (same scan shape as `forthFindItem` with the class-range filter
   `ptp >= PTP_DECLARE_LABEL && ptp <= PTP_MENU`).  On hit in EITHER
   state: `ERROR_INVALID_NAME`, abort-if-open, stop line — a bare
   parameterized item is an atomic syntax error.  Carry a comment naming
   F4: this arm becomes the Series-C parameter-grammar entry.
8. **B3 reverse** (forth_dict.c `forthResolveXEQ` item arm): add the SAME
   class-range reject — the arm's accept condition becomes
   `CAT_FNCT && !(ptp >= PTP_DECLARE_LABEL && ptp <= PTP_MENU)` with ptp
   computed once.  (`FORTH`/PTP_REM keeps resolving; `FCALL`/PTP_NUMBER_16
   stops.)  Migrate `test_xeq_item_lookup` Test 2: expectation flips to
   `FORTH_XEQ_NONE` with an updated FAIL message and the summary PASS line
   reworded (`FCALL->NONE (B3)`).

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_inner.c`,
`forth_compile.c`, `test_dict_reloc.c`.

### Targeted reads

1. forth_compile.c: the FORGET branch (placement model), the DEFS_ONLY
   gate, step-4/step-5 arms, the token-constant block.
2. forth_inner.c: the FTOK_CALL and FTOK_C47 arms (dispatch + region
   shapes to mirror), the constants block.
3. forth_dict.c: `vBodyWalk` in full, `forthDictMakeLatestGlobal`'s two
   walks, `forthFindItem`, `forthResolveXEQ`.
4. test_dict_reloc.c: `test_xeq_item_lookup` in full (small), the
   g-helpers, `begin_word`/`emit_int32`, tp helpers, registration lines.

### New test `test_xeqn` (register after the newest test)

Fresh state.  Subcases:

1. **Compile shape, both kinds.**  `forthOuterInterpret(": X1 XEQ 'AB' ;")`
   (`printf '%s' ": X1 XEQ 'AB' ;" | wc -c` = 15) → no error.  Encoding
   assertion on X1's transient body (name "X1" = 2 bytes; body at
   `fdict.latest + TO_BLOCKS(6+2)*BYTES_PER_BLOCK`): exact bytes
   `05 7F FD 02 41 42 00 00` (FTOK_XEQN LE, kind 253, len 2, 'A', 'B',
   FTOK_EXIT LE).  Then `forthOuterInterpret(": X2 XEQ :CDE: ;")` → body
   bytes `05 7F F9 03 43 44 45 00 00 00` (kind 249, len 3, pad 0, EXIT).
   `[1] PASS: XEQN encodes kind/len/name/pad exactly`
2. **Interpret-state global hit runs the program.**  tp fixture:
   `tpLbl(&tp, "TG")` + `tpSrc(&tp, "77")` + `tpEnd(&tp)`; `tpWrite`.
   `forthOuterInterpret("XEQ 'TG'")` → no error and `x_is_longint(77)`
   (fnExecute ran the program synchronously; its Forth step pushed 77;
   the run start was a fresh lifetime by design — define any transient
   words AFTER this subcase).
   `[2] PASS: XEQ 'NAME' interpret dispatch reaches the native XEQ path`
3. **Local requests never fall back.**  `forthOuterInterpret(": ZZ 8 ;")`
   then seed `forthPushInt32(55)`; `forthOuterInterpret("XEQ :ZZ:")` →
   `lastErrorCode == ERROR_LABEL_NOT_FOUND` and `x_is_longint(55)` (the
   colon word ZZ was NOT dispatched).  Clear the error.
   `[3] PASS: kind 249 miss is terminal — no fallback`
4. **Global-kind fallback chain, prim then colon.**
   `forthPushInt32(5); forthOuterInterpret("XEQ 'DUP'")` → no error; both
   X and Y are 5 (`x_is_longint(5)` + read_reg-style Y check per the
   local test idiom).  Then `forthOuterInterpret(": CW 9 ;")` and
   `forthOuterInterpret("XEQ 'CW'")` → `x_is_longint(9)`.
   `[4] PASS: global-kind miss falls back prim-then-colon`
5. **Compiled XEQN dispatches at run time, from both regions.**
   `forthOuterInterpret(": XR XEQ 'CW' ;")`; `run_word("XR")` →
   `x_is_longint(9)` (runtime arm, colon fallback, rstack jump).  Then
   `forthOuterInterpret(": GX XEQ 'CW' ; GLOBAL")` → no error (the GLOBAL
   walks accept XEQN); `run_word("GX")` → `x_is_longint(9)` (XEQN executed
   from a gdict body resolves the transient CW through the current
   scope).
   `[5] PASS: FTOK_XEQN runs from transient and global bodies`
6. **Corrupted inline data rejects.**  Runtime: hand-build a transient
   word via `begin_word("XC", 2)` emitting raw cells
   `FTOK_XEQN`, then `forthDictEmitBytes` of `{0xAA, 0x02, 'A', 'B'}`,
   then `end_word` — running it must set `ERROR_INVALID_CORRUPTED_DATA`
   (clear after).  Validator: g-build a word whose body is
   `FTOK_XEQN` + `{0xFD, 0x00}` + EXIT (len 0), run
   `forthGDictValidateRestored()` → reset (V-X1 outcome, the V-B idiom).
   `[6] PASS: bad kind and zero length reject at run and restore`
7. **B3 forward.**  `forthOuterInterpret("STO")` →
   `ERROR_INVALID_NAME` (clear).  `forthOuterInterpret(": BX STO ;")` →
   `ERROR_INVALID_NAME` and `forthFindColon("BX", &r)` false (atomic).
   Malformed XEQ forms too: `forthOuterInterpret("XEQ AB")` →
   `ERROR_INVALID_NAME`; `forthOuterInterpret("XEQ")` →
   `ERROR_INVALID_NAME` (clear each).
   `[7] PASS: bare parameterized items and malformed XEQ forms reject atomically`

Cleanup: both regions cleared, program memory restored per neighboring
tp tests, `lastErrorCode = ERROR_NONE`.

### Existing tests

`test_xeq_item_lookup` changes ONLY as authority 8 specifies.  Everything
else stays green untouched; the compile-state bare-label reject keeps its
existing pins.

### Non-goals / STOP boundaries

- No parameter grammar (`STO 05` stays F4) — B3's reject is the ONLY new
  behavior for parameterized items.
- No §2.3 mimicry/two-instance/round-trip acceptance — that is F3-7.
- No change to `insertUserItemInProgram`, the tam hook, or any PEM path.
- 0x7F06+ stays reserved everywhere.

### Gate and required mutations

Full gate green (seven PASS lines + migrated lookup test).  Mutations,
each separately, verbatim anchors, manual restore:

1. In `forthXeqnDispatch`, delete the kind-249 early-error branch (let
   local misses continue into the fallback chain).  Subcase 3 MUST go RED
   (ZZ dispatches: X becomes 8, no error).  Green = STOP.
2. In the runtime arm, delete the kind-byte validation (keep len).
   Subcase 6's runtime half MUST go RED (the 0xAA fixture now reaches
   resolution and raises ERROR_LABEL_NOT_FOUND instead of
   ERROR_INVALID_CORRUPTED_DATA).
3. In `emitXeqn`, swap the first two bytes (`buf[0] = len; buf[1] = kind;`).
   Subcase 1 MUST go RED (byte image mismatch at offsets 2-3).
4. In `vBodyWalk`'s XEQN arm, drop the `len < 1` half of the bounds check.
   Subcase 6's validator half MUST go RED (len-0 fixture survives).
5. Delete the B3 forward check (authority 7's call site).  Subcase 7's
   `"STO"` pin MUST go RED (ERROR_FUNCTION_NOT_FOUND instead of
   ERROR_INVALID_NAME).
6. In `forthResolveXEQ`, delete the class-range reject.  The migrated
   Test 2 MUST go RED (`FCALL` resolves as ITEM again).

Logs `/tmp/forth-f3-6-mut1..6.log`; residue-free diff; final gate; report
all PASS lines, banners, exit 0, arena line vs baseline, `git diff
--check`, mirror equality.  RULE-1: flash grows (parser + dispatch + arm)
— record the `make dmcp5r47` delta in the stage commit.

### Commit

```text
forth-core: F3-6 — XEQ source forms and FTOK_XEQN, kind-faithful end to end
```
