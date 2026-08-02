# Stage F3-4 — GLOBAL, IMMEDIATE, FORGET, and the same-line tracker

Origin: DESIGN §10.3 (owner rulings 2026-07-18) via the F3 design pass
(`QWEN_PROMPTS_F3_core.md` D4).  This packet lands the three vocabulary
marks and the latest-closed-definition tracker that makes them coherent
under the two-pass program model.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F3-3 commit
   `forth-core: F3-3 — definitions are scope-owned and lookup honors the
   owner`.
2. `grep -n "forthCurrentScopeGet" packages/forth-core/forth_dict.h` matches
   (F3-3 landed); `grep -rn "forthLatestClosedRef\|FF_DEFMARK\|forthGDictForget\|forthDictMakeLatestGlobal" packages/forth-core` → ZERO matches.
3. `grep -n "PRIM_RECURSE = 11\|PRIM_COUNT   = 12" packages/forth-core/forth_prims.c`
   both match (append point = index 12).
4. `grep -n "FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET"
   packages/forth-core/forth_compile.c` matches exactly one line (the
   pre-scan skip gate this packet carves).
5. Pre-gate green; arena baseline from the F3-3 commit.

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
   `/tmp/forth-f3-4-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-4-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f3-4-todo.md`,
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

## F3-4 — marks act on the definition closed earlier on the same line

### Authority carried by this packet (no open choices)

1. **Tracker** (forth_compile.c):

   ```c
   static uint16_t forthLatestClosedRef = FORTH_NULL;   /* a word REF, or FORTH_NULL */
   uint16_t forthLatestClosedRefGet(void) { return forthLatestClosedRef; }
   void     forthLatestClosedRefSet(uint16_t ref) { forthLatestClosedRef = ref; }
   ```

   (getter/setter declared in forth_dict.h — forth_prims.c consumes them.)
   Lifecycle: add `uint16_t savedLatestClosed;` to `forthOuterCtx_t`;
   `forthOuterRun` saves the tracker beside the defState save, RESETS it to
   `FORTH_NULL` for the new line, and restores it beside the defState
   restore.  The tracker is strictly per-line by construction.
   Set sites:
   - in the `;` branch, after `finishDefinition()` returns true (FULL and
     DEFS_ONLY modes): `forthLatestClosedRef = (uint16_t)(fdict.count - 1);`
     (the just-closed transient word's ref);
   - in the SKIP_DEFS `:`-consumer, after the closing `;` is found:
     resolve the consumed `name` via `forthFindColonRef(name, &ref, &fl)`
     and set the tracker to `ref` on hit (pre-scan may already have moved
     the word to gdict — the ref is then global, which is exactly what
     makes the marks idempotent on the execution pass); on miss leave the
     tracker unchanged (defensive; the pre-scan compiled or errored).
2. **New flag + prims** (forth_prims.h/.c): `#define FF_DEFMARK 0x04`
   (narrow `FF_RESERVED` to `0xF8` in forth_dict.h — FF_DEFMARK is a
   PRIM-TABLE flag only and never appears in dictionary headers, so the
   restore validator's header-flag rule is untouched).  Append (order and
   indices normative):

   ```c
   PRIM_GLOBAL    = 12,
   PRIM_IMMEDIATE = 13,
   PRIM_COUNT     = 14
   ...
   [PRIM_GLOBAL]    = { "GLOBAL",    FF_DEFMARK, pGlobal },
   [PRIM_IMMEDIATE] = { "IMMEDIATE", FF_DEFMARK, pImmediate },
   ```

   Bodies:

   ```c
   static void pGlobal(void)
   {
     uint16_t ref = forthLatestClosedRefGet();
     if (ref == FORTH_NULL) {
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return;
     }
     if (ref & FORTH_REF_GLOBAL) {
       return;                       /* already global: idempotent no-op */
     }
     if (forthInnerIsActive()) {
       displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return;
     }
     {
       uint16_t gref;
       if (forthDictMakeLatestGlobal(ref, &gref)) {
         forthLatestClosedRefSet(gref);
       }
     }
   }
   static void pImmediate(void)
   {
     uint16_t ref = forthLatestClosedRefGet();
     if (ref == FORTH_NULL) {
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return;
     }
     if (!forthDictSetImmediateByRef(ref)) {
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     }
   }
   ```

3. **Pre-scan carve-out** (forth_compile.c): the DEFS_ONLY skip gate
   becomes:

   ```c
   if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
     uint16_t pidx = forthFindPrim(buf);
     if (pidx != FORTH_PRIM_NONE && (forthPrims[pidx].flags & FF_DEFMARK)) {
       forthPrims[pidx].fn();          /* definition-completing marker */
       clearSystemFlag(FLAG_ASLIFT);
       if (lastErrorCode != ERROR_NONE) {
         lineOK = false;
       }
       continue;
     }
     continue;   /* D-2a: pre-scan must not execute tail code */
   }
   ```

   Rationale (carry as a comment): a later definition compiled in the SAME
   pre-scan must already see the mark (`IMMEDIATE` changes how subsequent
   definitions COMPILE; `GLOBAL` changes which region a subsequent lookup
   hits), so marks apply on the definition pass, and re-apply as no-ops on
   the execution pass.
4. **The move** (forth_dict.c):

   ```c
   bool forthDictMakeLatestGlobal(uint16_t tref, uint16_t *grefOut);
   ```

   Exact algorithm (implement faithfully; local static helper allowed):
   1. `idx = tref`; require `fdict.base && fdict.latest != FORTH_NULL &&
      idx == (uint16_t)(fdict.count - 1)`; else
      `ERROR_INVALID_NAME`, return false (defensive: the tracker
      construction makes this unreachable).
   2. `off = fdict.latest; hdr = (forthHeader_t *)(fdict.base + off);`
      require `!(hdr->flags & FF_SMUDGE)` (defensive).
      `bodyStart = off + (uint16_t)TO_BLOCKS(6 + hdr->nameLen) * BYTES_PER_BLOCK;`
      `selfTok = (ftoken_t)(0x1000 + idx);`
   3. **Validate walk** over `fdict.base` from `bodyStart`, bound
      `fdict.here`: advance per token exactly like `vBodyWalk`'s
      arms (PRIM/CALL 0, LIT 16, ILIT 4, BR/0BR 2, C47 2 + PTP-dependent
      2/0) until `FTOK_EXIT`, recording `end = pos` (offset AFTER the EXIT
      cell).  Legality inside the walk: a transient-space FTOK_CALL other
      than `selfTok` → `ERROR_INVALID_NAME`, false (a global word may only
      call global words); a global-space token with index `>= gdict.count`
      → same error (defensive); token ≥ 0x7F05 → same error (reserved —
      F3-6 extends BOTH this walk and vBodyWalk with the XEQN arm); any
      bound violation → same error.
   4. `entryBytes = end - off;` `if (!forthGDictEnsure(entryBytes)) return false;`
      (gdict may move; fdict cannot — the source pointers stay valid, but
      re-derive `hdr` AFTER the ensure anyway via `fdict.base + off` for
      hygiene).
   5. `goff = gdict.here;`
      `memcpy(gdict.base + goff, fdict.base + off, entryBytes);`
      patch the copy: `link = gdict.latest`, `owner = FORTH_OWNER_GLOBAL`
      (flags and name bytes ride along — IMMEDIATE marked before GLOBAL is
      preserved).
   6. **Rewrite walk** over the COPY (`gdict.base`, from
      `goff + <same aligned header>` to `goff + entryBytes`): same
      advancement switch; every token equal to `selfTok` is overwritten
      with `(ftoken_t)(FORTH_GCALL_BASE + gdict.count)` via a 2-byte
      memcpy.
   7. Commit gdict: `gdict.latest = goff;`
      `gdict.here = (uint16_t)TO_BLOCKS(goff + entryBytes) * BYTES_PER_BLOCK;`
      `gdict.count++;`
   8. Roll the transient copy off fdict (read `hdr->link` first):
      `fdict.here = off; fdict.latest = savedLink; fdict.count--;`
   9. `*grefOut = FORTH_REF_GLOBAL | (uint16_t)(gdict.count - 1);` true.
5. **Immediate setter** (forth_dict.c):
   `bool forthDictSetImmediateByRef(uint16_t ref);` — region-dispatch on
   bit 15, walk to the entry exactly like `forthDictNameByRef`, then
   `hdr->flags |= FF_IMMEDIATE; return true;` (out-of-range → false; do
   not touch smudged entries → false).
6. **Compiler honors colon immediacy** (§3.3.9 machinery, forth_compile.c
   colon branch): replace the compile-state emit with

   ```c
   uint16_t widx; uint8_t wflags;
   if (forthFindColonRef(buf, &widx, &wflags)) {
     if (state == STATE_COMPILE && !(wflags & FF_IMMEDIATE)) {
       ...emit forthTokenFromRef(widx) as today...
     } else {
       ...the existing interpret-state dispatch (forthInner + error gate),
          now also reached for an immediate word in compile state...
     }
     continue;
   }
   ```

7. **FORGET** is STRUCTURAL (forth_compile.c, new branch AFTER the `;`
   branch and BEFORE the DEFS_ONLY gate):

   ```c
   if (compareString(buf, "FORGET", CMP_BINARY) == 0) {
     char fname[FORTH_TOKEN_MAX + 1];
     if (state == STATE_COMPILE) {
       abortDefinition();
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;
       continue;
     }
     if (!nextToken(fname)) {
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;
       continue;
     }
     if (mode == FORTH_OUTER_DEFS_ONLY) {
       continue;                     /* behavior, not a mark: skipped in pre-scan */
     }
     if (forthInnerIsActive()) {
       displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;
       continue;
     }
     if (!forthGDictForget(fname)) {
       lineOK = false;               /* error already displayed */
     }
     continue;
   }
   ```

   `forthGDictForget` (forth_dict.c): walk gdict newest-first matching
   name (length + memcmp at `off + 6`); miss → copy the name into
   `errorMessage`, `ERROR_FUNCTION_NOT_FOUND`, false; hit at `off` after
   skipping `n` newer entries → `gdict.count -= (uint16_t)(n + 1);
   gdict.latest = hdr->link; gdict.here = off;` true.  (Define-before-use
   + top-truncation ⇒ survivors only reference survivors; stale global
   refs in transient bodies fail later through
   `bodyOffsetOfRef → FORTH_NULL → ERROR_INVALID_CORRUPTED_DATA`.)

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_prims.h` (only if the
FF_DEFMARK define belongs there per the existing FF_* placement — check:
FF_* live in forth_dict.h; then forth_prims.h is untouched),
`forth_prims.c`, `forth_compile.c`, `test_dict_reloc.c`.

### Targeted reads

1. forth_prims.c in full (72 lines).
2. forth_compile.c: the `;` branch, the SKIP_DEFS `:`-consumer, the
   DEFS_ONLY gate, the colon-lookup branch, `forthOuterCtx_t` +
   `forthOuterRun` prologue/epilogue.
3. forth_dict.c: `forthDictNameByRef` (walk model), `vBodyWalk`'s
   advancement arms (the walk model for step 3/6), `startDefinition`,
   `abortDefinition`.
4. test_dict_reloc.c: g-helpers, tp helpers, `run_word`,
   `x_is_longint`, registration lines.

### New test `test_global_marks` (register after the newest test)

Reset state first (`forthDictClear(); forthGDictClear();
lastErrorCode = ERROR_NONE;`).  Subcases (each with its own PASS line,
each clearing errors it expects):

1. **GLOBAL moves the same-line definition.**
   `forthOuterInterpret(": GA 5 ; GLOBAL")` → no error; require
   `forthFindColon("GA", &ref)` with `ref == (FORTH_REF_GLOBAL | 0)`;
   `fdict.count == 0` (transient copy rolled off); `gdict.count == 1`;
   `run_word("GA")` leaves `x_is_longint(5)`.  Then
   `forthOuterInterpret(": TB GA 1 + ;")` + `run_word("TB")` →
   `x_is_longint(6)`.
   `[1] PASS: same-line GLOBAL moved the definition to gdict`
2. **Same-line discipline.**  `forthOuterInterpret("GLOBAL")` alone →
   `ERROR_INVALID_NAME` (clear).  Then `forthOuterInterpret(": GC 1 ;")`
   followed by a SEPARATE `forthOuterInterpret("GLOBAL")` →
   `ERROR_INVALID_NAME` again, and `forthFindColon("GC", &ref)` still
   yields a TRANSIENT ref.
   `[2] PASS: GLOBAL requires a definition closed on the same line`
3. **A global may not call a transient.**
   `forthOuterInterpret(": TD 3 ;")` then
   `forthOuterInterpret(": GE TD ; GLOBAL")` → `ERROR_INVALID_NAME`
   (clear); `forthFindColon("GE", &ref)` yields a TRANSIENT ref (move
   refused, definition intact).
   `[3] PASS: transient-calling body refused GLOBAL`
4. **Self-call rewrite.**  `forthOuterInterpret(": GR 1 RECURSE ; GLOBAL")`
   → no error.  Locate GR's gdict body: `off = gdict.latest`,
   `bodyStart = off + TO_BLOCKS(6 + 2) * BYTES_PER_BLOCK` (name "GR", 2
   bytes — this arithmetic is a gdict ENCODING assertion, allowed);
   read the token after the ILIT payload (`bodyStart + 2 + 4`) via memcpy
   and require it to equal `FORTH_GCALL_BASE + 0`.  Do NOT run GR.
   `[4] PASS: RECURSE self-call rewritten to the global index`
5. **IMMEDIATE honored by the compiler.**
   `forthOuterInterpret(": GI 2 ; IMMEDIATE")` → no error;
   `forthFindColonRef("GI", &ref, &fl)` with `fl & FF_IMMEDIATE`.
   Seed X: `forthPushInt32(0)`.  `forthOuterInterpret(": TU GI ;")` → no
   error and `x_is_longint(2)` (GI EXECUTED during compilation).  Verify
   TU's body is empty: resolve TU's transient entry (it is `fdict.latest`),
   compute its bodyStart (name "TU" = 2 bytes; same encoding-assertion
   arithmetic), require the first body token to be `FTOK_EXIT` (0x0000).
   `[5] PASS: immediate colon word executed at compile time`
6. **FORGET truncates from the named word.**  Build via marks:
   `": G1 1 ; GLOBAL"`, `": G2 2 ; GLOBAL"`, `": G3 3 ; GLOBAL"`
   (three lines; gdict already holds GA from subcase 1 and GR from
   subcase 4, so assert `gdict.count == 5` here).
   `forthOuterInterpret("FORGET G2")` → no error; `gdict.count == 3`
   (GA, GR, G1); `forthFindColon("G2", &r)` false; `forthFindColon("G3", &r)`
   false; `forthFindColon("G1", &r)` true.  Then `forthOuterInterpret("FORGET TD")`
   (TD is transient from subcase 3) → `ERROR_FUNCTION_NOT_FOUND` (clear) —
   FORGET is gdict-only.  Then `forthOuterInterpret("FORGET ZZQQ")` →
   `ERROR_FUNCTION_NOT_FOUND` (clear).
   `[6] PASS: FORGET truncated the global scope at the named word`
7. **Marks apply on the pre-scan (program context).**  tp fixture:

   ```c
   testProg_t tp; tpInit(&tp);
   int sL   = tpLbl(&tp, "PM");
   int sMi  = tpSrc(&tp, ": MI 3 ; IMMEDIATE");
   int sMu  = tpSrc(&tp, ": MU MI ;");
   int sTail= tpSrc(&tp, "MU");
   ```

   (`printf '%s' ": MI 3 ; IMMEDIATE" | wc -c` = 19; `": MU MI ;"` = 10;
   `"MU"` = 2.)  `tpWrite`; `forthRunGenBump()`; drive `sMi` once with the
   F15-1 discipline (this first touch pre-scans ALL of PM's source steps:
   MI compiled and MARKED immediate by the carve-out, then MU compiled —
   during which MI EXECUTES, leaving X == 3 from the pre-scan's nested
   dispatch).  Assertions after the single drive:
   `forthFindColonRef("MI", &ref, &fl)` — wait: interactive scope cannot
   see MI.  Perform the assertions THROUGH the program path instead:
   drive `sTail` and require no error and `x_is_longint(3)` — MU's body
   is EMPTY (the mark took during pre-scan) so running MU leaves X from
   MI's compile-time execution earlier… no: running an empty MU leaves X
   UNCHANGED.  Therefore seed X with `forthPushInt32(55)` immediately
   before driving `sTail`, then require `x_is_longint(55)` — an empty MU
   must not touch it.  The discriminating oracle for the carve-out is the
   EMPTINESS of MU, proven behaviorally: X stays 55.  (Without the
   carve-out MU compiles a CALL to MI and X becomes 3.)
   `[7] PASS: IMMEDIATE applied during the pre-scan pass`
8. **Global persistence with flags.**
   `forthOuterInterpret(": GJ 4 ; IMMEDIATE GLOBAL")` → no error;
   saveCalc; `forthGDictClear(); forthDictClear();` restoreCalc (the T1.1
   idiom including loadTestPrograms handling); require
   `forthFindColonRef("GJ", &ref, &fl)` global with `fl & FF_IMMEDIATE`,
   and `run_word("GJ")` → `x_is_longint(4)`.
   `[8] PASS: global word and its IMMEDIATE flag survive restore`

Cleanup: clear both regions, reset program memory per neighboring tests,
`lastErrorCode = ERROR_NONE`.

### Existing tests

Untouched and green.  The colon-branch rework (authority 6) must be
behavior-identical for non-immediate words — if any legacy test reddens,
STOP.

### Non-goals / STOP boundaries

- No control-flow words (F3-5), no XEQN (F3-6).
- The validator gains nothing here (FF_IMMEDIATE acceptance landed in
  F3-2; XEQN arms land in F3-6).
- No DESIGN edits, no upstream edits, no entry-layer changes.

### Gate and required mutations

Full gate green (eight PASS lines).  Mutations, each separately, verbatim
anchors, manual restore:

1. In the DEFS_ONLY gate, delete the FF_DEFMARK carve-out (restore the
   plain `continue`).  Subcase 7 MUST go RED (X becomes 3 — MU compiled a
   call).  Green = STOP.
2. In `forthDictMakeLatestGlobal`, skip the rewrite walk (delete step 6).
   Subcase 4 MUST go RED (token still `0x1000 + idx`).
3. In the validate walk, delete the transient-call reject (treat any
   transient token like `selfTok` without recording).  Subcase 3 MUST go
   RED (GE becomes global / no error).
4. In `forthOuterRun`, delete the per-line tracker RESET (keep
   save/restore).  Subcase 2's separate-line `GLOBAL` MUST go RED (stale
   tracker accepted).
5. In the colon branch, revert the immediacy honor (always emit in compile
   state).  Subcase 5 MUST go RED (X != 2 and/or body not empty).
6. In `forthGDictForget`, change `gdict.count -= (uint16_t)(n + 1);` to
   `-= n;`.  Subcase 6 MUST go RED (its `gdict.count == 3` assertion sees
   4).

Logs `/tmp/forth-f3-4-mut1..6.log`; residue-free diff; final gate; report
the eight PASS lines, banners, exit 0, arena line (both regions — the
gdict figures move in this packet; quote against baseline), `git diff
--check`, mirror equality.  RULE-1: flash grows (three words + move
machinery) — record the `make dmcp5r47` delta in the stage commit
(PENDING if the owner is unavailable).

### Commit

```text
forth-core: F3-4 — GLOBAL/IMMEDIATE/FORGET with same-line mark discipline
```
