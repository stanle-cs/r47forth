# Stage F4-3 — named, system-flag, and indirect parameter forms

Origin: DESIGN §10.4/§4.4 phase 3 via `QWEN_PROMPTS_F4_core.md` §1.4-§2.
This packet lands the marker-cell forms — `'NAME'` (253), `→register`
(254), `→'NAME'` (255), system flags (250) — the bounded entry into the
shared native tail (`paramCoreExecuteOpBounded`), and the documented
indirect-NUMBER_16 exclusion.  Create semantics, `ERROR_UNDEF_SOURCE_VAR`,
`ERROR_UNDEF_MENU`, and indirection resolution are INHERITED from the
extracted native arms — never re-implemented.

Traced facts this packet carries (verified 2026-07-18): the typeable quote
is ASCII 0x27 (`ITM_QUOTE` 813 inserts `STD_QUOTE "\x27"` [items.c:2616,
fonts.h:56], reachable from `menu_alphaMisc` [softmenus.c:695]); the
indirection arrow is the two-byte glyph `STD_RIGHT_ARROW "\xa1\x92"`
[fonts.h:534]; the native named-variable arm gates creation on
`tryAllocate = isFunctionAllowingNewVariable(op)` (exact item list at
src/c47/registers.c:2387) and errors `ERROR_UNDEF_SOURCE_VAR` otherwise;
the MENU named arm resolves `findMenu` → `ERROR_UNDEF_MENU`
[packages/forth-core/programming/param_core.c PARAM_MENU arm]; system
flags encode `[250][b]` with the reverse name map over
`indexOfItems[b + SFL_TDM24]` (b<64) / `indexOfItems[(b&0x3f) + SFL_MONIT]`
[param_core.c PARAM_FLAG arm; SFL_TDM24 463, SFL_MONIT 2251,
items.h:491/2306]; indirect NUMBER_16 cannot ride the landed LE cell (a
`[254][ks]` cell aliases legal direct values with low byte 254) — the
exclusion is design (`QWEN_PROMPTS_F4_core.md` §2.2).

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log -1 --format=%s` is exactly:
   `forth-core: F4-2 — register, flag, and shuffle direct parameter forms`.
2. `grep -n "F4-3: named + indirect" packages/forth-core/forth_compile.c`
   matches (the interim arm this packet replaces);
   `grep -n "paramCoreExecuteOpBounded" packages/forth-core -r` → ZERO.
3. `grep -c "firstFreeProgramByte" packages/forth-core/programming/param_core.c`
   — record the count N in the todo; the Change-1 threading below must
   convert EVERY site (final grep must show exactly ONE remaining, inside
   the compatibility wrapper).
4. `grep -n "paramCoreDispatchDirect(uint16_t op, uint16_t ptpClass"
   packages/forth-core/programming/param_core.h` matches (F4-2's widened
   API).
5. Pre-gate green (`/tmp/forth-f4-3-pre.log`); arena baseline from the
   F4-2 commit.

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
   `/tmp/forth-f4-3-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f4-3-gate.log 2>&1; echo "gate exit: $?"`

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
9. Small-context recovery: this packet, `/tmp/forth-f4-3-todo.md`,
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

## F4-3 — marker cells feed the native tail through an explicit bound

### Authority carried by this packet (no open choices)

1. **Bounded entry** (programming/param_core.c/.h):

   ```c
   void paramCoreExecuteOpBounded(uint8_t *paramAddress, const uint8_t *end,
                                  uint16_t op, uint16_t paramMode);
   void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode);
   /* wrapper: paramCoreExecuteOpBounded(paramAddress, firstFreeProgramByte, op, paramMode) */
   ```

   Mechanics: rename the current body to the Bounded form adding the
   `end` parameter; thread `end` into EVERY internal
   `paramCoreReadName(..., firstFreeProgramByte)` call and into
   `_executeWithIndirectVariable` (which gains the same `end` parameter —
   `_executeWithIndirectRegister` reads no name and is unchanged).  After
   the threading, `grep -n firstFreeProgramByte programming/param_core.c`
   must match EXACTLY ONCE — inside the wrapper.  Behavior through the
   wrapper is byte-identical (every landed caller sees the same bound).
2. **Quoted-name parser** (forth_compile.c, static — the quote arm of the
   F3-6 shape, duplicated deliberately to avoid churning the landed XEQ
   parser; comment the twin):

   ```c
   /* 'NAME' with ASCII 0x27 delimiters; the closing quote must be the
    * LAST GLYPH (glyph-wise walk — a two-byte glyph's second byte that
    * equals 0x27 is not a close).  Twin of forthParseXeqForm's quote arm
    * (F3-6); kept separate so the landed XEQ parser stays untouched. */
   static bool parseQuotedName(const char *tok, char *name, uint8_t *lenOut);
   ```

   Same algorithm as F3-6's parser restricted to the quote delimiter;
   len 1..FORTH_NAME_MAX.
3. **Form dispatch by class** — the F4-2 parser arms extend; each eligible
   class accepts these ADDITIONAL token shapes:
   - `'NAME'` → REGISTER and MENU classes: marker 253; FLAG class:
     system-flag reverse lookup (below); other classes →
     `ERROR_INVALID_NAME`;
   - token beginning with the two bytes `"\xa1\x92"` (STD_RIGHT_ARROW):
     the REMAINDER after those two bytes is re-parsed as
     `'NAME'` → marker 255, or a direct register form
     (digits/`.NN`/letter via the F4-2 parsers) → marker 254 with that
     KS byte.  Classes accepting indirection: REGISTER, FLAG, NUMBER_8,
     NUMBER_8_16, MENU (the native decode arms all show 254/255).
     `PTP_NUMBER_16` does NOT — the arrow there is
     `ERROR_INVALID_NAME` with the exclusion comment;
   - system-flag reverse map (FLAG + quoted name): scan b = 0..63
     comparing `name` to `indexOfItems[b + SFL_TDM24].itemSoftmenuName`
     (compareString CMP_BINARY), then b = 64..127 against
     `indexOfItems[(b & 0x3f) + SFL_MONIT].itemSoftmenuName`; hit →
     bytes `[250][b]`; miss → `ERROR_INVALID_NAME`.
4. **Encoding** (compile state), after FTOK_C47+itemId:
   - 253/255 forms: cell `[marker][len]`, then the name bytes zero-padded
     to whole cells (`ceil2(len)`; odd len ⇒ one pad byte 0) — emitted
     via a local byte buffer + `forthDictEmitBytes` (the F3-6 `emitXeqn`
     shape);
   - 254 form: one cell `[254][ks]`;
   - 250 form: one cell `[250][b]`.
5. **Interpret dispatch** — assemble the native-shaped parameter and call
   the bounded core under the item wrap:

   ```c
   uint8_t nbuf[2 + FORTH_NAME_MAX];   /* marker + len + name, or marker + byte */
   /* fill nbuf per form; used = 2 + len for 253/255, 2 for 254/250 */
   {
     uint8_t savedRunStop = programRunStop;
     programRunStop = PGM_RUNNING;
     paramCoreExecuteOpBounded(nbuf, nbuf + used, itemId, paramModeOf(ptpClass));
     if (programRunStop == PGM_RUNNING) programRunStop = savedRunStop;
   }
   ```

   plus the standard error gate.  `paramModeOf` maps PTP→PARAM_* exactly
   as the F2 trace recorded (`(status & PTP_STATUS) >> 9`).
6. **Runtime decode** (forth_inner.c FTOK_C47 arm): the class cases from
   F4-1/F4-2 widen — when byte0 of the first parameter cell is a marker
   LEGAL FOR THAT CLASS (253: REGISTER/MENU; 250: FLAG; 254/255: the five
   indirect-capable classes), decode instead of erroring:
   - 254/250: `nbuf = {b0, byte1}`, used = 2, `ip += 2`;
   - 253/255: `len = byte1` (1..31 else corrupted-data), bounded-read
     `ceil2(len)` further bytes, odd-len pad must be 0 (corrupted-data),
     copy into `nbuf = {b0, len, name…}`, used = 2 + len,
     `ip += 2 + ceil2(len)`;
   then the SAME bounded-core dispatch as authority 5 (share the code:
   implement a file-static helper in forth_inner.c used by the arm, and
   export `forthParamMarkerDispatch(uint16_t op, uint16_t ptpClass,
   uint8_t *nbuf, uint16_t used)` in forth_dict.h for forth_compile.c's
   interpret path — ONE dispatch body total).
   `PTP_NUMBER_16` decode is UNTOUCHED (plain LE cell — the exclusion).
7. **Validator + GLOBAL walks** (three walks): the class arms widen with
   the same marker grammar — 254/250 one cell; 253/255 `[m][len]` with
   len 1..31 + `ceil2(len)` bytes + odd-len pad 0; markers only where the
   class allows them (as authority 6); everything else stays invalid.
   NUMBER_16 stays plain-cell.

### Files

Modify only: `forth_compile.c`, `forth_inner.c`, `forth_dict.h`,
`forth_dict.c`, `programming/param_core.c`, `programming/param_core.h`,
`test_dict_reloc.c`.

### Targeted reads

1. param_core.c: every `paramCoreReadName` call site (grep), the
   PARAM_MENU and PARAM_FLAG arms, `_executeWithIndirectVariable`.
2. forth_compile.c: the F4-2 parser arms, `emitXeqn` (the byte-buffer
   emit model).
3. forth_inner.c: the FTOK_C47 class cases, `forthXeqnDispatch` (the
   exported-helper model).
4. forth_dict.c: the three walk arms.
5. test_dict_reloc.c: helpers as before + the F3-6 `test_xeqn` encoding
   subcase (image-assertion style).

### New test `test_param_named_indirect` (register after the newest test)

Fresh state.  Subcases:

1. **Named variable create + recall round-trip.**
   `forthPushInt32(42); forthOuterInterpret("STO 'VZ'")` → no error (STO
   is in the traced create list — the variable is ALLOCATED by the shared
   arm); `forthPushInt32(1); forthOuterInterpret("RCL 'VZ'")` → no error,
   `x_is_longint(42)` (RCL is NOT in the create list — it found the
   variable).  Then `forthOuterInterpret("RCL 'VMISSING'")` →
   `ERROR_UNDEF_SOURCE_VAR` (clear) — inherited, not re-implemented.
   `[1] PASS: named variable creation and UNDEF_SOURCE_VAR inherit from the core`
2. **Named encode image.**  `": PN1 STO 'VZ' ;"` body cells:
   `FTOK_C47`, `ITM_STO`, `{253, 2}`, `{'V','Z'}`, `FTOK_EXIT`; and
   `": PN2 RCL 'ABC' ;"` parameter bytes `{253, 3, 'A','B','C', 0}` (odd
   len ⇒ pad 0).
   `[2] PASS: 253 cells carry len, name, and pad exactly`
3. **Compiled named dispatch.**  `run_word("PN1")` after
   `forthPushInt32(77)` → no error; `forthPushInt32(0);
   forthOuterInterpret("RCL 'VZ'")` → `x_is_longint(77)` (the runtime
   marker decode fed the same bounded core).
   `[3] PASS: compiled 253 form dispatches through the bounded core`
4. **Indirect register chain.**  `forthPushInt32(7);
   forthOuterInterpret("STO 05")` (register 05 := 7);
   `forthPushInt32(99);` then interpret the arrow form built as
   `sprintf(sbuf, "STO %s05", STD_RIGHT_ARROW)` → no error (stores 99
   into the register NAMED BY 05, i.e. register 07);
   `forthPushInt32(0); forthOuterInterpret("RCL 07")` → `x_is_longint(99)`.
   Encode image via `": PN3 <same sbuf> ;"`: parameter cell `{254, 5}`.
   `[4] PASS: →05 resolves through the native indirection helper`
5. **Indirect variable.**  `forthPushInt32(3);
   forthOuterInterpret("STO 'VP'")` (VP := 3 — a register NUMBER target);
   `forthPushInt32(55);` interpret `sprintf(sbuf, "STO %s'VP'",
   STD_RIGHT_ARROW)` → no error (stores 55 into register 03);
   `forthPushInt32(0); forthOuterInterpret("RCL 03")` → `x_is_longint(55)`.
   Encode image: parameter cells `{255, 2}`, `{'V','P'}`.
   `[5] PASS: →'VP' resolves through the native indirect-variable helper`
6. **System flag reverse map.**  Discover name0 =
   `indexOfItems[SFL_TDM24].itemSoftmenuName` at runtime; interpret
   `sprintf(sbuf, ": PF3 SF '%s' ;", name0)` → no error; PF3's parameter
   cell must be `{250, 0}`.  A missing flag name `"SF 'ZZQQ'"` →
   `ERROR_INVALID_NAME` (clear).
   `[6] PASS: system-flag names map to [250][index] cells`
7. **Menu named form.**  `"OPENM 'ZZQQ'"` → `ERROR_UNDEF_MENU` (clear) —
   the inherited miss surface; encode image via `": PM1 OPENM 'ZZQQ' ;"`
   parameter cells `{253, 4}`, `{'Z','Z','Q','Q'}` (compile does not
   resolve — names resolve at dispatch, source-as-truth).
   `[7] PASS: OPENM names encode unresolved and miss with UNDEF_MENU`
8. **NUMBER_16 exclusion + validator teeth.**  Using the F4-1 discovery
   (first eligible new-form NUMBER_16 item), interpret
   `sprintf(sbuf, "%s %s05", name, STD_RIGHT_ARROW)` →
   `ERROR_INVALID_NAME` (clear).  Validator: g-build a REGISTER-class
   body with cells `{253, 0}` (len 0) → `forthGDictValidateRestored`
   RESETS; another with `{253, 3, 'A','B','C', 7}` (bad pad) → RESETS;
   a NUMBER_16-class body whose parameter cell is `{254, 5}` → RESETS
   (markers stay illegal for N16).
   `[8] PASS: N16 indirection excluded; marker-cell malformations reject`

Cleanup: regions, error state; named variables VZ/VP persist in the
calculator variable space — delete them the way neighboring tests handle
allocated variables IF such cleanup exists (grep `deleteNamedVariable`
usage in the test file; if none exists, leave them and note it in the
report — do not invent cleanup machinery).

### Existing tests

Untouched and green.  The Bounded refactor is wrapper-compatible: every
landed caller goes through `paramCoreExecuteOp` unchanged.

### Non-goals / STOP boundaries

- No new PTP classes; COMPARE/LABEL/etc. stay rejected (flow).
- No `IND` spelling, no directional-glyph quotes — 0x27 and `\xa1\x92`
  only (V4).
- No local-register allocation; no menu-open positive-path assertion.

### Gate and required mutations

Full gate green (eight PASS lines).  Mutations, each separately, verbatim
anchors, manual restore, `/tmp/forth-f4-3-mut1..5.log`:

1. In the runtime 254 assembly, drop the marker byte (`nbuf = {byte1}`
   with used = 1 and mode unchanged).  Subcase 4's compiled twin
   (`run_word("PN3")` — ADD that drive to subcase 4: after the interpret
   chain, reseed registers 05:=7, X:=99, run PN3, RCL 07 must be 99)
   MUST go RED (the direct-store path hits register 05 instead of 07).
   If green, STOP and report.
2. In `paramCoreExecuteOpBounded`'s threading, revert ONE
   `paramCoreReadName(..., end)` back to
   `paramCoreReadName(..., firstFreeProgramByte)` (the PARAM_REGISTER
   arm's site).  Subcase 1 MUST go RED (the stack-buffer name reads
   against a program-memory bound — expect an empty/garbled name and a
   failed round-trip; if it accidentally stays green, STOP and report —
   escape valve).
3. In the 253 emitter, write the odd-len pad byte as 1.  Subcase 2's PN2
   image MUST go RED.
4. In the system-flag reverse scan, start the second range at
   `SFL_MONIT + 1`.  Subcase 6 stays green (index 0 is in range one) —
   therefore ALSO add to subcase 6 a range-two probe: discover
   `name64 = indexOfItems[SFL_MONIT].itemSoftmenuName`, compile
   `"SF '<name64>'"`, require cell `{250, 64}`.  Under the mutation that
   probe MUST go RED.
5. In `vBodyWalk`'s 253 arm, drop the len lower bound.  Subcase 8's
   len-0 pin MUST go RED.

Residue-free diff; final gate; report PASS lines, mutation symptoms,
banners, exit 0, arena line, `git diff --check`, mirror equality.
RULE-1: flash grows (marker machinery) — record the delta in the stage
commit.

### Commit

```text
forth-core: F4-3 — named, system-flag, and indirect parameters through the bounded core
```
