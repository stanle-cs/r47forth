# Stage F5-1 — the check mode: one grammar, side effects gated off

Origin: DESIGN §10.5/§8.4 E9 via `QWEN_PROMPTS_F5_core.md` §2-§3 (the tier
ruling and derivation mechanism are DECIDED there; this packet implements
them).  `forthOuterRun` gains `FORTH_OUTER_CHECK`; the new public
`forthCheckSourceLine` returns whether a source line is committable.
Nothing calls it from the entry layer yet (F5-2).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log -1 --format=%s` is exactly:
   `forth-core: F4-4 — Series C error table and native parity are pinned`.
2. `grep -n "FORTH_OUTER_SKIP_DEFS = 2" packages/forth-core/forth_compile.c`
   matches (the mode enum this packet extends);
   `grep -rn "FORTH_OUTER_CHECK\|forthCheckSourceLine" packages/forth-core`
   → ZERO matches.
3. Side-effect inventory (record each count in the todo; the weave must
   account for every hit, and a count that cannot be reconciled with the
   Change-2 site list is a STOP):
   - `grep -cn "forthDictEmit" packages/forth-core/forth_compile.c`
   - `grep -cn "forthPrims\[" packages/forth-core/forth_compile.c`
   - `grep -cn "forthInner(" packages/forth-core/forth_compile.c`
   - `grep -cn "reallyRunFunction\|fnExecute" packages/forth-core/forth_compile.c`
   - `grep -cn "forthPushInt32\|forthPushReal34" packages/forth-core/forth_compile.c`
   - `grep -cn "startDefinition\|finishDefinition\|abortDefinition" packages/forth-core/forth_compile.c`
   - `grep -cn "setSystemFlag(FLAG_ASLIFT)\|clearSystemFlag(FLAG_ASLIFT)" packages/forth-core/forth_compile.c`
   - `grep -cn "forthGDictForget\|forthDictMakeLatestGlobal\|forthDictSetImmediateByRef\|forthParamMarkerDispatch\|forthXeqnDispatch" packages/forth-core/forth_compile.c`
4. Pre-gate green (`/tmp/forth-f5-1-pre.log`); arena baseline from the
   F4-4 commit.

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
   `/tmp/forth-f5-1-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f5-1-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f5-1-todo.md`,
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

## F5-1 — the tokenizer checks itself

### Authority carried by this packet (no open choices)

1. **Mode + API** (forth_compile.c):

   ```c
   FORTH_OUTER_CHECK = 3            /* joins the forthOuterMode_t enum */

   bool forthCheckSourceLine(const char *source)
   {
     forthOuterCtx_t ctx;
     size_t n = strlen(source);
     if (n >= FORTH_SOURCE_MAX) {
       displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return false;
     }
     memcpy(ctx.source, source, n + 1);
     lastErrorCode = ERROR_NONE;
     forthOuterRun(&ctx, FORTH_OUTER_CHECK);
     return lastErrorCode == ERROR_NONE;
   }
   ```

   (declare in forth_dict.h).  Check mode NEVER touches
   `forthRunGenBump`/lifecycle — `forthOuterRun` itself has no lifecycle
   calls, which is why the entry rides it directly.
2. **Simulation state** — locals of `forthOuterRun`, live only in check
   mode:

   ```c
   bool simOpen = false;            /* definition open (structural sim) */
   bool simClosedThisLine = false;  /* same-line tracker analog */
   uint8_t simCsp = 0;
   uint8_t simKind[FORTH_CSTACK_DEPTH];
   ```

3. **The weave.**  Introduce `const bool checking = (mode ==
   FORTH_OUTER_CHECK);` at the top.  Then, branch by branch (this is the
   complete site list; reconcile it against the gate-item-3 inventory
   before editing):
   - **`:` branch**: in check mode, do NOT call `startDefinition`;
     instead: nested open (`simOpen`) → the same `ERROR_INVALID_NAME` +
     stop; missing name token → same; name length 0 or >
     `FORTH_NAME_MAX` → same; else `simOpen = true; simCsp = 0;` and
     continue.  (The real branch is untouched for other modes.)
   - **`;` branch**: check mode: `!simOpen` → stray-`;` error;
     `simCsp != 0` → unterminated-control error (`ERROR_INVALID_NAME`);
     else `simOpen = false; simClosedThisLine = true;`.
   - **FORGET branch**: check mode: compile-state (`simOpen`) →
     `ERROR_INVALID_NAME` + stop; missing name → same; else consume the
     name and continue (tier 2 — no gdict consult).
   - **XEQ branch**: check mode: missing token / `forthParseXeqForm`
     failure → `ERROR_INVALID_NAME` + stop; else continue (no emit, no
     dispatch — resolution is tier 2).
   - **DEFS_ONLY gate**: unchanged (check mode never passes
     `FORTH_OUTER_DEFS_ONLY`).
   - **prim branch**: check mode replaces BOTH the emit and execute
     halves with a placement/pairing simulation:
     - control prims (match the handler pointer or the PRIM_IF..
       PRIM_REPEAT index range — use the index range; it is
       append-frozen): outside a definition (`!simOpen`) →
       `ERROR_OPERATION_UNDEFINED` + stop; else apply the pairing table
       against `simKind`/`simCsp` — IF push O; THEN pop O; ELSE pop O
       push O; BEGIN push D; UNTIL/AGAIN pop D; WHILE pop D, push O,
       push D; REPEAT pop D, pop O — pop-mismatch/underflow →
       `ERROR_INVALID_NAME`, push overflow → `ERROR_RAM_FULL`, each +
       stop;
     - `PRIM_RECURSE`: `!simOpen` → `ERROR_OPERATION_UNDEFINED` + stop;
     - `PRIM_GLOBAL`/`PRIM_IMMEDIATE`: `!simClosedThisLine` →
       `ERROR_INVALID_NAME` + stop (the same-line law, statically
       checkable);
     - every other prim: continue (legal in both states; nothing runs).
   - **colon branch**: check mode: continue on hit OR miss (tier 2) —
     but the LOOKUP still executes (read-only) because the number branch
     below needs its result for the suppression rule; restructure so
     check mode queries `forthFindColonRef` once into a local
     `colonHit`.
   - **number branch**: check mode: if `classifyNumber(buf) !=
     FORTH_NUM_NONE` and `!colonHit` (the §2(f) suppression): run the
     PARSE into locals — integer path `parseParamDigits`-independent:
     reuse `parseNumberAsInt32`, real path `parseNumberAsReal34` — with
     NO push and NO emit; a parse failure →
     `ERROR_INVALID_NAME` + stop.  Success or suppressed → continue.
   - **item and label branches**: check mode: SKIPPED ENTIRELY (jump to
     the next token; tier 2) — the parameterized-item arm included: its
     parameter token is NOT consumed in check mode (an unshadowed run
     would consume it, a shadowed run would not; consuming in check
     could mask a tier-1 violation in the next token, so check treats it
     as an ordinary token — carry this comment).
   - **last-resort error**: check mode: continue (unknown = tier 2).
   - **end-of-line**: check mode: `simOpen` → unterminated-definition
     `ERROR_INVALID_NAME` (no abort — nothing was opened); NO ASLIFT
     write on either path.
   - **`forthCurrentScope`/tracker snapshots**: already restored by the
     epilogue; check mode changes neither.
4. **Zero side effects, provable**: in check mode the function performs
   NO call to any symbol in the gate-3 inventory lists (emit, prims-fn,
   inner, run/execute, push, start/finish/abort, ASLIFT, marks/forget/
   xeqn/param dispatch).  After implementation, verify by inspection per
   site and say so in the report.

### Files

Modify only: `forth_compile.c`, `forth_dict.h`, `test_dict_reloc.c`.

### Targeted reads

forth_compile.c: `forthOuterRun` in full (it is the subject), the mode
enum, `forthParseXeqForm`, `parseNumberAsInt32/AsReal34`,
`classifyNumber`.  test_dict_reloc.c: `run_word`, registration lines,
one tp-based test for the drive idiom.

### New test `test_check_source_line` (register after the newest test)

Fresh state (`forthDictClear(); forthGDictClear(); lastErrorCode =
ERROR_NONE;`).

1. **Tier-1 rejects, exact codes.**  For each pair below, call
   `forthCheckSourceLine(line)`, require `false` and the exact
   `lastErrorCode`, clear it:
   `": A : B ;"` → INVALID_NAME; `": ;"` → INVALID_NAME; `";"` →
   INVALID_NAME; `": A"` (unterminated) → INVALID_NAME;
   `": TOOLONGNAMETOOLONGNAMETOOLONGNAMEX 1 ;"` (name 32+) →
   INVALID_NAME; `"IF"` → OPERATION_UNDEFINED; `": A THEN ;"` →
   INVALID_NAME; `": A BEGIN THEN ;"` → INVALID_NAME; `": A IF ;"` →
   INVALID_NAME; `"RECURSE"` → OPERATION_UNDEFINED; `"GLOBAL"` →
   INVALID_NAME; `"FORGET"` → INVALID_NAME; `"XEQ"` → INVALID_NAME;
   `"XEQ AB"` → INVALID_NAME.
   `[1] PASS: tier-1 structural violations reject with their runtime codes`
2. **Tier-2 acceptances.**  Each must return `true` with
   `lastErrorCode == ERROR_NONE` after: `"UNKNOWNWORD9"`; `"SDL"`;
   `"SDL 100"`; `"RTN"`; `"STO 'NEVERMADE'"`; `"FORGET NOSUCH"`;
   `"XEQ 'NOLABEL'"`; `"3 4 +"`; `": D2 2 / ; 8 D2"`.
   `[2] PASS: names and item-level conditions stay advisory`
3. **Zero side effects.**  Record `fdict.here/latest/count`,
   `gdict.here/count`, `forthTestGetRsp()`, and X (seed
   `forthPushInt32(123)` first) — run EVERY line from subcases 1 and 2
   through `forthCheckSourceLine` again — require every recorded value
   unchanged and `x_is_longint(123)` (nothing executed, emitted, or
   pushed; the check of `": D2 2 / ; 8 D2"` defined NOTHING).
   `[3] PASS: the check mode mutates nothing`
4. **Soundness battery (the derivation's teeth).**  For every REJECTED
   line of subcase 1: run the SAME line through
   `forthOuterInterpret(line)` on a fresh state and require the SAME
   `lastErrorCode` the check produced (record pairs; clear between).
   This is the check-implies-runtime implication; a mismatch fails the
   subcase naming both codes.
   `[4] PASS: every check reject reproduces at execution with the same code`
5. **The documented suppression edge.**  `forthOuterInterpret(": 12E ;")`…
   `12E` classifies as a number?  It does NOT (no exponent digits ⇒
   classify returns NONE) — so use the classifying-but-unparseable
   form: craft `line1 = "9999999999999999999999999999999999999999E9999"`
   (classifies real; `stringToReal34` yields infinity — VERIFY at
   runtime what `parseNumberAsReal34` returns for it; if it parses
   successfully, print `[5] CONFIG: no parse-failing numeric form found
   — suppression edge untestable, tier-1(f) reachable only via
   conversion failures` and pass the subcase vacuously; this is an
   escape valve, not silent success).  If it DOES fail to parse:
   `forthCheckSourceLine(line1)` → false/INVALID_NAME; then define a
   colon shadow with EXACTLY that name via
   `startDefinition`/`finishDefinition` direct calls (the compiler path
   would itself reject), and require `forthCheckSourceLine(line1)` →
   true (suppressed by the live shadow).
   `[5] PASS: number tier-1 fires and the live-shadow suppression holds`

Cleanup as usual.

### Existing tests

Untouched and green — check mode must not perturb any other mode's
behavior (the weave is additive per branch).

### Non-goals / STOP boundaries

- No entry-layer call site (F5-2).  No new advisory UI.  No item-table
  consultation in check mode.  No parameter-grammar checks at commit
  (tier ruling — do not "improve" this).

### Gate and required mutations

Full gate green (five PASS lines).  Mutations, each separately, verbatim
anchors, manual restore, `/tmp/forth-f5-1-mut1..4.log`:

1. In the check `;` branch, drop the `simCsp != 0` conjunct.  Subcase
   1's `": A IF ;"` row MUST go RED (check accepts; subcase 4's
   implication also breaks — name both).
2. In the check prim branch, remove the control-prim
   outside-definition guard.  Subcase 1's `"IF"` row MUST go RED.
3. In the check number branch, drop the `!colonHit` suppression.
   Subcase 5's shadow half MUST go RED (or its CONFIG line proves the
   subcase vacuous — in that case this mutation is N/A; report it so).
4. In `forthCheckSourceLine`, make it return `true` unconditionally
   (keep the run).  Subcase 1 MUST go RED wholesale (the API's verdict
   is load-bearing, not just the error code).

Residue-free diff; final gate; report the five PASS lines, banners,
exit 0, arena line, `git diff --check`, mirror equality.  RULE-1: the
weave adds flash — record the delta in the stage commit.

### Commit

```text
forth-core: F5-1 — check mode: the tokenizer validates its own grammar
```
