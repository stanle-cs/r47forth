# Stage F3-5 — control-flow words: IF/ELSE/THEN, BEGIN/UNTIL/AGAIN/WHILE/REPEAT

Origin: DESIGN §10.3 (2026-07-16 fold) via the F3 design pass
(`QWEN_PROMPTS_F3_core.md` D5).  The runtime tokens (`FTOK_BR`/`FTOK_0BR`)
landed long ago and the F1-5 validator already covers everything these
words can emit; this packet adds ONLY the compile-time machinery: eight
immediate primitives and the control stack.  **`IF` emits a bare
`FTOK_0BR` and consumes the flag — no hidden `DUP`** (normative, §2.2
branch-token stack effects; a value needed after the test is the
program's job to `DUP`).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F3-4 commit
   `forth-core: F3-4 — GLOBAL/IMMEDIATE/FORGET with same-line mark
   discipline`.
2. `grep -n "PRIM_IMMEDIATE = 13\|PRIM_COUNT     = 14" packages/forth-core/forth_prims.c`
   both match (append point = 14).
3. `grep -rn "forthCstack\|forthCtlIf\|CTL_ORIG" packages/forth-core` →
   ZERO matches.
4. `grep -n "FF_DEFMARK" packages/forth-core/forth_dict.h` matches (F3-4
   landed).
5. Pre-gate green; arena baseline from the F3-4 commit.

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
   `/tmp/forth-f3-5-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f3-5-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f3-5-todo.md`,
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

(No program fixtures are authored here — the fixture-rule block is not
required for this packet.)

---

## F3-5 — compile-time branches over the landed runtime tokens

### Authority carried by this packet (no open choices)

1. **Control stack** (forth_compile.c, near the tokenizer statics):

   ```c
   #define FORTH_CSTACK_DEPTH 8
   #define CTL_ORIG 1        /* a forward branch's delta cell, to patch */
   #define CTL_DEST 2        /* a backward target (BEGIN)               */
   typedef struct { uint16_t pos; uint8_t kind; } forthCtl_t;
   static forthCtl_t forthCstack[FORTH_CSTACK_DEPTH];
   static uint8_t forthCsp = 0;
   ```

   Reset: in the `:` branch, immediately after `startDefinition(name)`
   succeeds, add `forthCsp = 0;`.  Balance check: in the `;` branch,
   BEFORE `finishDefinition()`:

   ```c
   if (forthCsp != 0) {
     abortDefinition();
     displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     forthCsp = 0;
     lineOK = false;
     continue;
   }
   ```

2. **Local helpers** (forth_compile.c, static):

   ```c
   static bool ctlPush(uint16_t pos, uint8_t kind) {
     if (forthCsp >= FORTH_CSTACK_DEPTH) {
       displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return false;
     }
     forthCstack[forthCsp].pos = pos;
     forthCstack[forthCsp].kind = kind;
     forthCsp++;
     return true;
   }
   static bool ctlPop(uint8_t kind, uint16_t *pos) {
     if (forthCsp == 0 || forthCstack[forthCsp - 1].kind != kind) {
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return false;
     }
     forthCsp--;
     *pos = forthCstack[forthCsp].pos;
     return true;
   }
   static bool ctlEmitBranch(ftoken_t brTok, uint16_t *deltaPosOut) {
     if (!forthDictEmit(brTok)) return false;      /* ensure displayed RAM_FULL */
     *deltaPosOut = fdict.here;
     return forthDictEmit((ftoken_t)0);            /* placeholder delta */
   }
   static void ctlPatchTo(uint16_t deltaPos, uint16_t target) {
     int16_t delta = (int16_t)(((int32_t)target - (int32_t)(deltaPos + 2)) / 2);
     memcpy(fdict.base + deltaPos, &delta, 2);     /* base read fresh: emits may
                                                      have moved the region */
   }
   static bool ctlEmitBack(ftoken_t brTok, uint16_t dest) {
     if (!forthDictEmit(brTok)) return false;
     {
       uint16_t deltaPos = fdict.here;
       int16_t delta = (int16_t)(((int32_t)dest - (int32_t)(deltaPos + 2)) / 2);
       return forthDictEmit((ftoken_t)(uint16_t)delta);
     }
   }
   ```

   Deltas are int16 CELLS relative to the cell after the delta field
   (§2.2); positions are region-relative OFFSETS held across emits
   (§3.3.7 offsets-only discipline — never pointers).
3. **Word bodies** (forth_compile.c, public; declared in forth_dict.h;
   every one opens with the RECURSE-pattern compile-only guard):

   ```c
   #define CTL_GUARD() do { if (!isDefinitionOpen()) { \
     displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE); \
     return; } } while (0)

   void forthCtlIf(void)    { CTL_GUARD(); uint16_t p;
                              if (!ctlEmitBranch(FTOK_0BR, &p)) return;
                              ctlPush(p, CTL_ORIG); }
   void forthCtlThen(void)  { CTL_GUARD(); uint16_t p;
                              if (!ctlPop(CTL_ORIG, &p)) return;
                              ctlPatchTo(p, fdict.here); }
   void forthCtlElse(void)  { CTL_GUARD(); uint16_t p1, p2;
                              if (!ctlPop(CTL_ORIG, &p1)) return;
                              if (!ctlEmitBranch(FTOK_BR, &p2)) return;
                              ctlPatchTo(p1, fdict.here);
                              ctlPush(p2, CTL_ORIG); }
   void forthCtlBegin(void) { CTL_GUARD(); ctlPush(fdict.here, CTL_DEST); }
   void forthCtlUntil(void) { CTL_GUARD(); uint16_t d;
                              if (!ctlPop(CTL_DEST, &d)) return;
                              ctlEmitBack(FTOK_0BR, d); }
   void forthCtlAgain(void) { CTL_GUARD(); uint16_t d;
                              if (!ctlPop(CTL_DEST, &d)) return;
                              ctlEmitBack(FTOK_BR, d); }
   void forthCtlWhile(void) { CTL_GUARD(); uint16_t d, p;
                              if (!ctlPop(CTL_DEST, &d)) return;
                              if (!ctlEmitBranch(FTOK_0BR, &p)) return;
                              if (!ctlPush(p, CTL_ORIG)) return;
                              ctlPush(d, CTL_DEST); }
   void forthCtlRepeat(void){ CTL_GUARD(); uint16_t d, o;
                              if (!ctlPop(CTL_DEST, &d)) return;
                              if (!ctlPop(CTL_ORIG, &o)) return;
                              if (!ctlEmitBack(FTOK_BR, d)) return;
                              ctlPatchTo(o, fdict.here); }
   ```

   (forth_compile.c already defines `FTOK_BR`? It does NOT — its local
   token constants stop at `FTOK_C47`.  Add `#define FTOK_BR 0x7F02` /
   `#define FTOK_0BR 0x7F03` beside them, mirroring forth_inner.c.)
4. **Prim rows** (forth_prims.c, append-only; all FF_IMMEDIATE):

   ```c
   PRIM_IF = 14, PRIM_ELSE = 15, PRIM_THEN = 16, PRIM_BEGIN = 17,
   PRIM_UNTIL = 18, PRIM_AGAIN = 19, PRIM_WHILE = 20, PRIM_REPEAT = 21,
   PRIM_COUNT = 22
   ...
   [PRIM_IF]     = { "IF",     FF_IMMEDIATE, forthCtlIf     },
   [PRIM_ELSE]   = { "ELSE",   FF_IMMEDIATE, forthCtlElse   },
   [PRIM_THEN]   = { "THEN",   FF_IMMEDIATE, forthCtlThen   },
   [PRIM_BEGIN]  = { "BEGIN",  FF_IMMEDIATE, forthCtlBegin  },
   [PRIM_UNTIL]  = { "UNTIL",  FF_IMMEDIATE, forthCtlUntil  },
   [PRIM_AGAIN]  = { "AGAIN",  FF_IMMEDIATE, forthCtlAgain  },
   [PRIM_WHILE]  = { "WHILE",  FF_IMMEDIATE, forthCtlWhile  },
   [PRIM_REPEAT] = { "REPEAT", FF_IMMEDIATE, forthCtlRepeat },
   ```

   Error/abort discipline needs NO new code: immediate prims dispatch
   through the existing interpret-arm error gate, which aborts an open
   definition on any `lastErrorCode`.

### Files

Modify only: `forth_dict.h`, `forth_prims.c`, `forth_compile.c`,
`test_dict_reloc.c`.

### Targeted reads

1. forth_prims.c in full.
2. forth_compile.c: the token-constant block, the `:` and `;` branches,
   the prim-dispatch arm (error gate shape).
3. test_dict_reloc.c: `run_word`, `x_is_longint`, the Stage-1-B loop test
   (grep `0BR` and read that one hand-assembled test for the delta
   convention), registration lines.

### New test `test_control_flow` (register after the newest test)

Fresh state (`forthDictClear(); forthGDictClear(); lastErrorCode =
ERROR_NONE;`).  Subcases:

1. **IF/ELSE/THEN, both arms + the no-DUP pin.**
   `forthOuterInterpret(": CF1 IF 10 ELSE 20 THEN ;")` → no error.
   Token pin: CF1 is `fdict.latest`; its body starts at
   `fdict.latest + TO_BLOCKS(6 + 3) * BYTES_PER_BLOCK` (name "CF1", 3
   bytes — encoding assertion); the FIRST body token, read via 2-byte
   memcpy, MUST equal `0x7F03` (`FTOK_0BR` — IF consumed, nothing DUPed).
   Behavior: `forthPushInt32(1); run_word("CF1")` → `x_is_longint(10)`;
   `forthPushInt32(0); run_word("CF1")` → `x_is_longint(20)`.
   `[1] PASS: IF consumes the flag and selects the correct arm`
2. **BEGIN/WHILE/REPEAT countdown.**
   `forthOuterInterpret(": CF2 BEGIN DUP WHILE 1 - REPEAT ;")` → no
   error; `forthPushInt32(5); run_word("CF2")` → `x_is_longint(0)`
   (five iterations, terminates — the Stage-1-B semantics from source).
   `[2] PASS: BEGIN/WHILE/REPEAT loop terminates at zero`
3. **UNTIL loops while false.**
   `forthOuterInterpret(": CF5 BEGIN 1 - DUP UNTIL DROP ;")` with
   `forthPushInt32(1)` → one pass: 1−1=0, DUP, UNTIL pops 0 (false) →
   loops; second pass: 0−1=−1, UNTIL pops −1 (true) → exits; DROP.
   Precondition: seed the stack with `forthPushInt32(99)` BENEATH the 1
   (push 99 first), and require `x_is_longint(99)` after (the loop's
   working value dropped cleanly).
   `[3] PASS: UNTIL branches back on false`
4. **AGAIN is the runaway-bounded infinite loop.**
   `forthOuterInterpret(": CF3 BEGIN AGAIN ;")` → no error;
   `run_word("CF3")` → `lastErrorCode == ERROR_RAM_FULL` (the §2.2
   runaway backstop is the only exit).  Clear the error.
   `[4] PASS: AGAIN loops until the runaway backstop`
5. **Nesting.**  `forthOuterInterpret(": CF4 IF 1 IF 30 THEN THEN ;")`;
   `forthPushInt32(1); run_word("CF4")` → `x_is_longint(30)`;
   `forthPushInt32(7); forthPushInt32(0); run_word("CF4")` →
   `x_is_longint(7)` (both levels skipped).
   `[5] PASS: nested IF pairs resolve independently`
6. **Pairing and placement errors, all atomic.**  Each line below must
   set the named error, leave the named word UNFINDABLE
   (`forthFindColon` false), and be cleared before the next:
   - `": CE1 THEN ;"` → `ERROR_INVALID_NAME`;
   - `": CE2 IF ;"` → `ERROR_INVALID_NAME` (unbalanced at `;`);
   - `": CE3 BEGIN THEN ;"` → `ERROR_INVALID_NAME` (kind mismatch);
   - `": CE4 BEGIN REPEAT ;"` → `ERROR_INVALID_NAME` (missing WHILE);
   - interpret-state `forthOuterInterpret("IF")` →
     `ERROR_OPERATION_UNDEFINED` (compile-only guard);
   - `": CE5 IF IF IF IF IF IF IF IF IF 1 ;"` → `ERROR_RAM_FULL`
     (ninth IF overflows FORTH_CSTACK_DEPTH 8).
   `[6] PASS: unbalanced and misplaced control words reject atomically`
7. **Branches survive GLOBAL + restore.**
   `forthOuterInterpret(": GLW BEGIN DUP WHILE 1 - REPEAT ; GLOBAL")` →
   no error (branch tokens pass the GLOBAL validate walk); saveCalc;
   clear both regions; restoreCalc (T1.1 idiom); `forthPushInt32(3);
   run_word("GLW")` → `x_is_longint(0)` — compiled branches validated by
   the restore walk and executed from gdict.
   `[7] PASS: compiled branches survive GLOBAL and restore validation`

Cleanup as usual (both regions, error state).

### Existing tests

Untouched.  The Stage-1-B hand-assembled loop test must stay green — it
pins the delta convention this packet's emitters must match.

### Non-goals / STOP boundaries

- No XEQN, no XEQ forms (F3-6).  No DO/LOOP, no `0=`-style new prims —
  the vocabulary is exactly the eight words above.
- No validator change (BR/0BR arms already exist).
- No DESIGN edits, no upstream edits.

### Gate and required mutations

Full gate green (seven PASS lines).  Mutations, each separately, verbatim
anchors, manual restore:

1. In `forthCtlIf`, change `FTOK_0BR` to `FTOK_BR`.  Subcase 1 MUST go
   RED (token pin sees 0x7F02, and/or both flag cases take one arm).
2. In `ctlPatchTo`, change `(deltaPos + 2)` to `deltaPos`.  Subcase 1
   MUST go RED (forward branch lands one cell early — wrong X or an
   error; name the observed symptom).
3. In `forthCtlWhile`, swap the final two pushes (push `d` as CTL_DEST
   first, then `p` as CTL_ORIG).  Subcase 2 MUST go RED (`CF2` fails to
   compile: REPEAT's kind-checked pops reject).
4. In the `;` branch, delete the `forthCsp != 0` balance check.  Subcase
   6's CE2 assertion MUST go RED (CE2 becomes findable).
5. In `forthCtlIf`, delete `CTL_GUARD()`.  Subcase 6's interpret-state
   `IF` pin MUST go RED (no ERROR_OPERATION_UNDEFINED raised).

Logs `/tmp/forth-f3-5-mut1..5.log`; residue-free diff; final gate; report
the seven PASS lines, banners, exit 0, arena line vs baseline, `git diff
--check`, mirror equality.  RULE-1: flash grows (eight prim rows +
helpers) — record the `make dmcp5r47` delta in the stage commit.

### Commit

```text
forth-core: F3-5 — compile-time control flow over the landed branch tokens
```
