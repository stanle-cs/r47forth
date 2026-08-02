# Stage F4-2 — register, flag, and shuffle direct forms

Origin: DESIGN §10.4/§4.4 via `QWEN_PROMPTS_F4_core.md` §1.3-§1.5, §2.
This packet lands the DIRECT (non-named, non-indirect) source forms for
`PTP_REGISTER`, `PTP_FLAG`, and `PTP_SHUFFLE`: `NN`, `.NN`, single
uppercase letters, and the four-letter shuffle spelling.  Named/indirect
forms remain the commented `ERROR_INVALID_NAME` interim until F4-3.

Traced facts this packet carries (QWEN_PROMPTS_F4_core.md §1, verified
2026-07-18): the KS map (numbered 0..99; X Y Z T A B C D L I J K =
100..111; locals `.00`..`.98` = 112..210; M N P Q R S = 211..216;
E F G H O U V W = 217..224) [src/c47/defines.h:1318-1362]; flags share the
letter codes with locals capped at `.31` (112..143; 144..210 ILLEGAL)
[defines.h:792-822, decode.c PARAM_FLAG]; the native FLAG dispatch arm
accepts `<= LAST_LOCAL_FLAG(143)` or `FLAG_M(211) <= v < FLAG_W(224)` —
**flag letter W (224) parses natively but dispatches as a silent no-op;
this quirk is parity, do not fix it**
[packages/forth-core/programming/param_core.c PARAM_FLAG arm]; the
REGISTER arm range-gates `regInRange(regKStoC(ks))` — and (AMENDMENT
F4-2A, traced 2026-07-19) `regInRange` is NOT a pure predicate: on a miss
it raises `ERROR_OUT_OF_RANGE` itself [src/c47/store.c:17-72] and only
then returns false, so the arm declines to DISPATCH but the user sees an
error; there is no silence here [param_core.c PARAM_REGISTER arm]; SHUFFLE dispatches the packed
byte as-is, spelling = 4 lowercase chars of `xyzt`, char i ↔ bits
2i..2i+1 via `shuffleReg[4] = {'x','y','z','t'}`
[src/c47/programming/decode.c:10 + PARAM_SHUFFLE arm]; the shuffle item
is 1694 whose name is the glyph `STD_RIGHT_OVER_LEFT_ARROW`
[items.c:3511].

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log -1 --format=%s` is exactly:
   `forth-core: F4-1 — flow steps reject; direct numeric parameters land`.
2. `grep -n "F4-2/F4-3: register, flag, shuffle" packages/forth-core/forth_compile.c`
   matches exactly one line (the interim arm this packet replaces).
3. `grep -n "paramCoreDispatchDirect" packages/forth-core/programming/param_core.h`
   shows the two-argument form `(uint16_t op, uint16_t value)` (this
   packet widens it).
4. `grep -rn "paramLetterToKS\|PTP_SHUFFLE" packages/forth-core/forth_compile.c`
   → ZERO matches.
5. Pre-gate green (`/tmp/forth-f4-2-pre.log`); arena baseline from the
   F4-1 commit.

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
   `/tmp/forth-f4-2-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f4-2-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f4-2-todo.md`,
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

## F4-2 — registers by number, dot, and letter; flags likewise; shuffle by spelling

### Authority carried by this packet (no open choices)

1. **Letter table** (forth_compile.c, static — exactly these 26 rows):

   ```c
   static const struct { char c; uint8_t ks; } paramLetterKS[26] = {
     {'X',100},{'Y',101},{'Z',102},{'T',103},{'A',104},{'B',105},
     {'C',106},{'D',107},{'L',108},{'I',109},{'J',110},{'K',111},
     {'M',211},{'N',212},{'P',213},{'Q',214},{'R',215},{'S',216},
     {'E',217},{'F',218},{'G',219},{'H',220},{'O',221},{'U',222},
     {'V',223},{'W',224},
   };
   static bool paramLetterToKS(const char *tok, uint8_t *ks)
   {
     int i;
     if (tok[0] == 0 || tok[1] != 0) return false;   /* exactly one byte */
     for (i = 0; i < 26; i++) {
       if (paramLetterKS[i].c == tok[0]) { *ks = paramLetterKS[i].ks; return true; }
     }
     return false;
   }
   ```

   Uppercase only (native `itemSoftmenuName` parity); lowercase falls
   through to the malformed reject.
2. **Register-form parser** (static): for an eligible `PTP_REGISTER`
   item's parameter token, in this order:
   - all digits (`parseParamDigits`) → value ≤ 99 → `ks = value`; value
     100..0x3fff → `ERROR_OUT_OF_RANGE`;
   - leading `'.'` with ≥1 digit after → local index via
     `parseParamDigits(tok+1, ...)` → index ≤ 98 → `ks = 112 + index`;
     else `ERROR_OUT_OF_RANGE`;
   - single letter via `paramLetterToKS` → its ks;
   - anything else (including quotes and the arrow — F4-3) →
     `ERROR_INVALID_NAME` with the comment `/* F4-3: named + indirect */`.
3. **Flag-form parser**: same shapes with flag byte spaces —
   digits ≤ 99 → byte = value; `.NN` with index ≤ 31 → byte = 112 +
   index, index 32..98 → `ERROR_OUT_OF_RANGE`; letter → the SAME table
   byte (the quirk note from the header rides as a comment on the `W`
   row); else `ERROR_INVALID_NAME` (system-flag quotes are F4-3).
4. **Shuffle parser**: token must be EXACTLY 4 bytes, each ∈
   {'x','y','z','t'} (lowercase); packed
   `byte = c0 | c1<<2 | c2<<4 | c3<<6` with ci = index in "xyzt";
   anything else → `ERROR_INVALID_NAME`.
5. **Emit** (compile state): one cell `[byte][0]` after the
   FTOK_C47+itemId cells, all three classes.
6. **Shared-core widening** (programming/param_core.c/.h) — API change:
   `paramCoreDispatchDirect(uint16_t op, uint16_t ptpClass, uint16_t value)`
   (class parameter added).  Body: `PTP_REGISTER` dispatches
   `reallyRunFunction(op, regKStoC((uint8_t)value));` — the ONLY
   converting class; every other class dispatches `value` as today.
   `paramCoreValidateDirect` gains:
   - `PTP_REGISTER`: `value <= LAST_SPARE_REGISTERS_IN_KS_CODE &&
     regInRange(regKStoC((uint8_t)value))` (F4-2A: out-of-range = false
     AND `regInRange` has already raised `ERROR_OUT_OF_RANGE` — this
     class is the documented exception to the "validate sets no error"
     contract in param_core.h; note it there);
   - `PTP_FLAG`: `value <= LAST_LOCAL_FLAG || (FLAG_M <= value && value <
     FLAG_W)` — the native arm's exact condition INCLUDING the `< FLAG_W`
     quirk (mirror, don't fix);
   - `PTP_SHUFFLE`: `true` (native dispatches as-is).
   Migrate BOTH existing `paramCoreDispatchDirect` call sites (grep them:
   the forth_inner FTOK_C47 tail and F4-1's interpret site; a third site
   is a STOP) to pass the class.
7. **Runtime decode** (forth_inner.c FTOK_C47 arm) — three new cases,
   each bounded-reading one cell, `ip += 2`:
   - `PTP_REGISTER`: `b0 <= 224` → `param = b0`; `b0 >= 225` →
     `ERROR_INVALID_CORRUPTED_DATA` (markers join in F4-3; a malformed
     CELL fails loud — the silence parity applies to legal-form
     out-of-range VALUES, not corrupt encodings; carry this comment);
   - `PTP_FLAG`: legal bytes (≤ 143 or 211..224) → `param = b0`; else
     `ERROR_INVALID_CORRUPTED_DATA` (250 joins in F4-3);
   - `PTP_SHUFFLE`: `param = b0` (any byte).
   The shared validate/dispatch tail applies unchanged (now with the
   class argument).
8. **Validator + GLOBAL walks** (forth_dict.c, all three walks): the
   FTOK_C47 arm gains the same three classes — one cell, pad byte must
   be 0, byte legality exactly as the runtime cases above (SHUFFLE: any
   byte; FLAG: ≤143 or 211..224; REGISTER: ≤224); advance +2.

### Files

Modify only: `forth_compile.c`, `forth_inner.c`,
`programming/param_core.c`, `programming/param_core.h`, `forth_dict.c`,
`test_dict_reloc.c`.

### Targeted reads

1. forth_compile.c: the F4-1 parameterized arm (grep the interim
   comment), `parseParamDigits`.
2. param_core.c: `paramCoreValidateDirect`/`paramCoreDispatchDirect` and
   5 lines around each; the PARAM_FLAG arm (for the quoted quirk
   condition only).
3. forth_inner.c: the FTOK_C47 arm.
4. forth_dict.c: the three walk arms (vBodyWalk + the two mark walks).
5. test_dict_reloc.c: `read_reg_int32` and its four-push seeding caller,
   `run_word`, `x_is_longint`, g-helpers, registration lines.

### New test `test_param_register_flag` (register after the newest test)

Fresh state.  Subcases:

1. **STO/RCL numbered round-trip, interpret + compiled.**
   `forthPushInt32(7); forthOuterInterpret("STO 05")` → no error;
   `forthPushInt32(9); forthOuterInterpret("RCL 05")` → no error and
   `x_is_longint(7)`.  Compiled twin: `": PR0 RCL 05 ;"`,
   `forthPushInt32(3); run_word("PR0")` → `x_is_longint(7)` (register 05
   still holds 7).
   `[1] PASS: STO/RCL 05 round-trips in both states`
2. **Letter registers.**  `forthPushInt32(13);
   forthOuterInterpret("STO A")` → no error; `forthPushInt32(2);
   forthOuterInterpret("RCL A")` → `x_is_longint(13)`.  Byte-image pin:
   `": PRA STO A ;"` body cells are `FTOK_C47`, `ITM_STO`, `{104, 0}`,
   `FTOK_EXIT` (encoding assertion with named constants; 104 is the
   traced KS of A).
   `[2] PASS: lettered register A maps to KS 104 and round-trips`
3. **Stat-letter conversion is live.**  `forthPushInt32(21);
   forthOuterInterpret("STO M")`; `forthPushInt32(4);
   forthOuterInterpret("RCL M")` → `x_is_longint(21)` (M rides
   `regKStoC(211)` — the converting dispatch).
   `[3] PASS: stat register M stores through regKStoC`
4. **Local dot form encodes and rejects unallocated.**  Byte-image:
   `": PRL STO .05 ;"` body cell `{117, 0}` (112+5).  Behavior parity
   (F4-2A, corrected): with no local registers allocated, running
   `STO .05` over a pushed 31 → `lastErrorCode == ERROR_OUT_OF_RANGE`
   (raised inside `regInRange`) and X still 31 — no store happens.
   Clear the error afterwards.
   `[4] PASS: .05 encodes KS 117; unallocated locals raise OUT_OF_RANGE and never store`
5. **Flag forms.**  Byte-images: `": PF1 SF 10 ;"` → `{10, 0}`;
   `": PF2 SF .31 ;"` → `{143, 0}`.  Errors: `"SF .32"` →
   `ERROR_OUT_OF_RANGE`; `"SF q"` → `ERROR_INVALID_NAME`; `"CF 100"` →
   `ERROR_OUT_OF_RANGE` (clear each).  Behavior: `"SF 55"` then... flag
   observability is not required here — the byte images plus the
   dispatch-through-shared-core are the pins; do NOT add a flag-read
   helper.
   `[5] PASS: flag forms encode and bound correctly`
6. **Shuffle.**  Build the source with the item's own glyph name:
   `sprintf(sbuf, "%s yxzt", indexOfItems[1694].itemCatalogName);`
   (verify at runtime that item 1694 has `PTP_SHUFFLE` status — CONFIG
   FAIL otherwise).  Byte-image via `": PSH <same> ;"` built with
   sprintf: parameter cell `{0xE1, 0}` (yxzt = 1|0<<2|2<<4|3<<6 = 225).
   Behavior: the seed MUST ride in the source line —
   `"11 22 33 44 <glyph> yxzt"` — because `x_set_string` overwrites
   REGISTER_X with the source string and `fnForthOuter` drops it, which
   shifts anything pre-pushed by one level.  Expect no error and X==33,
   Y==44, Z==22, T==11 via `read_reg_int32` on all four.  Malformed: `"<glyph> yxz"` and
   `"<glyph> yxzq"` → `ERROR_INVALID_NAME` (clear).
   `[6] PASS: shuffle yxzt packs to 0xE1 and swaps X/Y`
7. **Validator arms.**  g-build three one-cell bodies and validate:
   REGISTER cell `{230, 0}` → RESET (illegal byte); FLAG cell `{150, 0}`
   → RESET (the 144..210 hole); SHUFFLE cell `{0xE1, 1}` → RESET (pad
   byte).  A REGISTER cell `{104, 0}` → ACCEPT.  (V-idiom from the F3-2
   pins; `forthGDictClear()` between builds.)
   `[7] PASS: walk arms enforce register/flag/shuffle cell legality`

**Test-authoring facts (F4-2A, learned the hard way).**  `forthFindColon`
returns a REF INDEX, not a byte offset — byte-image pins must walk from
`fdict.latest + TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK`.  (Using the
ref as an offset silently works for the first word after a clear, where
both are 0, and corrupts every later pin.)  Every subcase must open with
`lastErrorCode = ERROR_NONE;`: the preceding F4-1 test ends on subcase 7's
deliberate `ERROR_OPERATION_UNDEFINED` and does not clear it, so a subcase
that reads `lastErrorCode` before setting it reports a phantom error 13.

Cleanup: regions + error state; leave register 05/A/M as the suite's
neighboring tests do (they reseed their own state).

### Existing tests

Untouched and green.  The `paramCoreDispatchDirect` widening must keep
every landed F2 parity test green — those tests call the PUBLIC gate via
engines, not the helper directly; if one calls it directly, STOP and
report (packet defect), do not adapt.

### Non-goals / STOP boundaries

- No named/quoted/indirect/system-flag forms (F4-3).
- No flag-state observation helpers; no local-register allocation
  machinery.
- The FLAG `< FLAG_W` quirk is preserved, never corrected.

### Gate and required mutations

Full gate green (seven PASS lines).  Mutations, each separately,
verbatim anchors, manual restore, `/tmp/forth-f4-2-mut1..5.log`:

1. In `paramCoreDispatchDirect`, drop the REGISTER conversion (dispatch
   raw `value`).  Subcase 3 MUST go RED (M's KS 211 dispatched raw — the
   store lands off-register and RCL M misses; if it somehow stays green,
   STOP and report per the escape-valve rule).
2. In `paramLetterKS`, change A's code 104 to 105.  Subcase 2's
   byte-image pin MUST go RED (`{105, 0}` — note the round-trip half
   alone would stay green, which is WHY the image pin exists; say so in
   the report).
3. In the shuffle packer, reverse the bit order (`c0<<6 | c1<<4 | c2<<2
   | c3`).  Subcase 6 MUST go RED (image 0x4B and the register swap
   misbehaves).
4. In the `.NN` register encode, change `112 + index` to `111 + index`.
   Subcase 4's image MUST go RED (`{116, 0}`).
5. In `vBodyWalk`'s FLAG arm, accept the contiguous range (drop the
   144..210 hole).  Subcase 7's FLAG-cell pin MUST go RED (the `{150,0}`
   body validates).

Residue-free diff; final gate; report PASS lines, mutation symptoms,
banners, exit 0, arena line, `git diff --check`, mirror equality.
RULE-1: modest flash growth — record the delta in the stage commit.

### Commit

```text
forth-core: F4-2 — register, flag, and shuffle direct parameter forms
```
