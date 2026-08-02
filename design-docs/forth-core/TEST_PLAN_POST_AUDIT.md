# Post-Audit Comprehensive Test Plan
## forth-core Stage 1 (15 must-fix + 7 defer)

Mutation-test standard: **per test, name a bug that SHOULD fail it; confirm it would. No vacuous tests.**

**⚠️ Reviewed against DESIGN.md (authoritative, 2026-07-08). Spec corrections flagged `[SPEC-CORRECTED]`.**

> **Reconciliation note (independent Qwen review).** A parallel Qwen pass over the
> pre-review draft independently converged on **4 of 7** spec corrections
> (ILIT → `forthPushInt32`/`dtLongInteger`; `forthFindColon` two-arg;
> `forthOuterInterpret` as the core; hand-assembled branch tests) but **missed
> three** — a future transcription is at risk of reintroducing them, so watch:
> 1. **BR/0BR deltas are int16 (2 bytes), in cells** (`ip += 2; ip += delta*2`) —
>    NOT int32 / `ip += 4` (§2.2 token table, §3.2 L473-474).
> 2. The 0BR zero-test helper is **`forthPopIsZero`** — type-dispatched, consumes
>    X — NOT `popIsFalse` / raw `real34IsZero` (§8 T1-1, §3.2 L475).
> 3. **No `ERROR_NAME_TOO_LONG` exists** — use the committed generic error path
>    (§3.3.7 L943).
> This document is the reconciled authority; **DESIGN.md wins on any conflict.**

### Spec facts that constrain the tests (from DESIGN.md)

- **FTOK_ILIT** → `dtLongInteger` (long integer), via `forthPushInt32` /
  `convertLongIntegerToLongIntegerRegister` — NOT real34 (§2.2, §3.3.5). Tests
  must assert the register TYPE is `dtLongInteger` and compare via
  `longIntegerCompareInt`, not `real34ToInt`.
- **FTOK_BR / FTOK_0BR** deltas are **int16 (2 bytes), signed, in CELLS**:
  `ip += 2; ip += delta*2` (§2.2 token table, §3.2 lines 473-474). Not int32.
- **The sub-phase C compiler NEVER emits FTOK_BR / FTOK_0BR** (§3.3 "Emit scope
  C-1", §7 acceptance item 3). Branch behavior is testable ONLY via
  **hand-assembled bodies** (the Stage-1-B backward-loop). Any test that tries
  to compile `IF`/loops from source is a stage-2 test.
- **`forthFindColon(name, &widx)`** returns `bool`, index via `uint16_t *widx`
  out-param, skips `FF_SMUDGE`; miss leaves `widx` untouched (§4.1 step 2, C-5).
- **`forthOuterInterpret(source)`** is the directly-callable core (§3.3.2) — PC
  tests call it, not a made-up `forthCompile`.
- Nested/re-entrancy guard raises **`ERROR_OPERATION_UNDEFINED`** (C-12).
- Zero-test helper is **`forthPopIsZero`**, type-dispatched, consumes X (§3.2,
  T1-1).
- The number grammar **accepts a sign after e/E**: `( [eE] [+-]? digit+ )?`
  (§3.3.5 line 831). So `1e-5` is a VALID real; the bug (#9) is code rejecting
  it. Mantissa-less forms (`e5`, `.e5`, `3e`) must be REJECTED (no NaN reaches
  `decQuadFromString`).

---

## CRITICAL FIXES (#6 → #2+#11 → #1+#16 → #4, #5, #3)

### FIX #6: DMCP Build Breakage (test harness in cross target)

**Spec:** §6 lists five runtime custom sources; the harness is not one.

**Bug:** `test_dict_reloc.c` has no `#if PC_BUILD` guard; contains fork/waitpid/
printf and PC-only helpers.

**Fix:** Wrap the whole file in `#if defined(PC_BUILD) ... #endif`, and (correct
long-term fix) keep it out of `pkg_custom_sources` on the cross build.

**Test:** none (build-gate). Escaping mutation: remove the guard → cross build
pulls in POSIX/printf symbols.

**Verify:**
```bash
./packages/forth-core/build-test.sh        # exit 0
make dmcp CUSTOM_PKG=packages/forth-core   # links without the test TU
```

---

### FIX #2+#11: Init/Reset Lifecycle

**Spec:** §1.2 (struct field order: base, sizeBlocks, here, latest, count);
§6.2 (reset hook — ALREADY DOCUMENTED, lines 1239-1252).

**Bugs:** (#2) `fdict` has no initializer → base=NULL, but `latest` must be
`FORTH_NULL` when empty; a lookup on the H2/H3 path derefs NULL. (#11)
`doFnReset` never calls `forthDictInit()`.

**Fixes:**
- Static-init `fdict = { .base=NULL, .sizeBlocks=0, .here=0, .latest=FORTH_NULL, .count=0 }`.
- Guard `forthFindColon`: `if (fdict.base == NULL) return false;` (no `*widx` write).
- Add `forthDictInit();` at the end of `doFnReset` (code to match the §6.2 doc).

**Tests:**

```c
// [SPEC-CORRECTED] two-arg forthFindColon; out-param untouched on miss.
void test_lifecycle_pre_init(void) {
    fdict = (forthDict_t){ .base=NULL, .sizeBlocks=0, .here=0,
                           .latest=FORTH_NULL, .count=0 };
    uint16_t widx = 0xDEAD;
    assert(forthFindColon("DOESNOTEXIST", &widx) == false);  // clean miss
    assert(fdict.base == NULL);
    assert(widx == 0xDEAD);                                   // NOT written
    // Escaping mutation: remove the base==NULL guard -> NULL deref/crash.
}

void test_lifecycle_reset(void) {
    forthDictInit();
    forthOuterInterpret(": SQ DUP * ;");
    assert(lastErrorCode == ERROR_NONE);
    uint16_t widx = 0;
    assert(forthFindColon("SQ", &widx) == true);

    forthDictInit();                                 // reset hook (fix #11)
    assert(forthFindColon("SQ", &widx) == false);    // dict cleared

    forthOuterInterpret(": SQ DUP * ;");
    assert(lastErrorCode == ERROR_NONE);
    assert(forthFindColon("SQ", &widx) == true);
    // Escaping mutation: drop forthDictInit() from reset -> stale SQ or corruption.
}
```

**Verify:** `build-test.sh` GREEN; both tests PASS.

---

### FIX #1+#16: Decode Bugs (ILIT type + BR/0BR width)  [SPEC-CORRECTED]

**Spec:** §2.2 token table; §3.2 lines 467-474; §3.3.5 number-type conformance.

**Bugs:** (#1) `(int8_t)` sign-extend on the ILIT low byte → wrong value for
low byte ≥ 0x80. (#16) same 8-bit sign-extend on BR/0BR deltas.

**`[SPEC-CORRECTED]` Fixes:**
- FTOK_ILIT: `memcpy(&v, base+ip, 4); ip += 4; forthPushInt32(v);` — push as
  **long integer**, not `int32ToReal34`.
- FTOK_BR / FTOK_0BR: `int16_t delta; memcpy(&delta, base+ip, 2); ip += 2;
  ip += (int32_t)delta * 2;` — **int16, cells**, not int32/bytes.

**Tests (ILIT — compiled source is fine, since the compiler DOES emit ILIT):**

```c
void test_ilit_sign_extend(void) {
    forthDictInit();
    forthOuterInterpret(": TEST128 128 ;");
    assert(lastErrorCode == ERROR_NONE);
    uint16_t widx = 0;
    assert(forthFindColon("TEST128", &widx));
    forthInner(widx, false);
    // [SPEC-CORRECTED] assert TYPE is dtLongInteger and value 128.
    assert(getRegisterDataType(REGISTER_X) == dtLongInteger);
    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 128) == 0);   // NOT -128
    longIntegerFree(li);
    // Escaping mutation: (int8_t) sign-extend -> value -128 (fails).
    //                    int32ToReal34 push  -> type dtReal34 (fails).
}

void test_ilit_compile_interpret_parity(void) {
    forthDictInit();
    forthOuterInterpret(": W 128 + ;");
    forthPushInt32(42);
    uint16_t widx = 0; assert(forthFindColon("W", &widx));
    forthInner(widx, false);
    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 170) == 0);
    longIntegerFree(li);
}
```

**`[SPEC-CORRECTED]` BR/0BR delta tests are HAND-ASSEMBLED, not compiled** (the
compiler never emits branches). They belong with fix #3's canonical loop body
(Commit 6), where 0BR-consume and delta-stride are exercised together:
`DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`, counter 5 → 0, terminates.

**Verify:** `build-test.sh` GREEN; ILIT tests PASS.

---

### FIX #4: fnForthCall fromProgram

**Spec:** §3.3 pseudocode line 634; §3.3.2 line 734 —
`forthInner(widx, programRunStop == PGM_RUNNING)`.

**Bug:** hardcoded `true` → interactive entry exits before the first token.

**Fix:** `forthInner(param, programRunStop == PGM_RUNNING);` in `fnForthCall`;
correct Stage-H1.md step 11 too.

**Test:**
```c
void test_fnforthcall_interactive(void) {
    forthDictInit();
    forthOuterInterpret(": ADDTEN 10 + ;");
    uint16_t widx = 0; assert(forthFindColon("ADDTEN", &widx));
    programRunStop = PGM_STOPPED;          // interactive
    forthPushInt32(5);
    fnForthCall(widx);
    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 15) == 0);   // executed, not a no-op
    longIntegerFree(li);
    // Escaping mutation: forthInner(param, true) -> X stays 5.
}
```

---

### FIX #5: H2 LBL? UB (uninitialized resolvedParam)

**Spec:** §4.2 — reverse-lookup fallback (C47 label first, Forth second);
`forthResolveXEQ` contract (FORTH_XEQ_LABEL / _NONE / _FORTH).

**Bug:** `resolvedParam` uninitialized, passed to `reallyRunFunction` on a miss;
Forth-word hit passes colon index as a label ID (type confusion).

**Fix:** init `resolvedParam = (uint16_t)INVALID_VARIABLE`; gate on the resolve
result; for `ITM_LBLQ` never pass a Forth index as a label.

**Test:** `test_lblq_undefined_no_ub` — best under ASan (catches the
uninitialized read). If the H2 entry isn't unit-callable, mark explicitly
DEFERRED to §8.5 integration (tracked, not dropped).

**Verify:** `make test_asan CUSTOM_PKG=packages/forth-core` → no
uninitialized-read report.

---

### FIX #3: FTOK_0BR Consumer + forthPopIsZero  [SPEC-CORRECTED]

**Spec:** §2.2 line 329-331 (0BR consumes); §3.2 line 475-479 + T1-1
(type-dispatched zero test, `forthPopIsZero`, NOT raw real34IsZero).

**Bugs:** 0BR doesn't consume; the zero test is raw real34 (misreads
long-int X); a rewritten test with a false comment enshrines the wrong behavior.

**Fixes:**
- `forthPopIsZero`: type-dispatch (dtLongInteger / dtReal34 / else via
  compareRegisters), then `fnDrop(NOPARAM)` (consume).
- Restore canonical hand-assembled body in `test_branch_back`:
  `DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`.
- De-vacuous `test_0br_longint` (sentinel + X assert).

**Tests (all HAND-ASSEMBLED bodies):**

```c
void test_0br_consumes(void) {
    // Hand-assemble: DUP 0BR(skip) ILIT 999 EXIT
    // X=0: 0BR consumes DUP'd 0, branches past 999 -> X==0, 999 not pushed.
    // X!=0: 0BR consumes, no branch, 999 executes.
    // Escaping mutation: no fnDrop -> value not consumed / stack grows.
}

void test_0br_longint_sign_magnitude(void) {
    // X = long-integer 0 must read as zero.
    // Escaping mutation: raw real34IsZero on the long-int misreads it.
}

// test_branch_back (fixed body): counter 5 -> 0 over 5 iterations, terminates.
// Escaping mutation: revert to "DUP ILIT 1 MINUS 0BR BR" -> infinite loop
// (runaway cap turns the hang into a FAIL).
```

**Verify:** `build-test.sh` GREEN, no hangs.

---

## MEDIUM-HIGH & MEDIUM FIXES (#7-#12)

### FIX #7: Silent-failure family (dict-space / name errors)

**Spec:** §3.3.7 (`startDefinition`), §3.3.8 (`forthDictEnsure` wrap + count
cap). `[SPEC-CORRECTED]` The errors DESIGN.md specifies are:
- Over-long / empty name in `startDefinition` (line 943): generic `error` (no
  specific `ERROR_NAME_TOO_LONG` — do NOT invent one; use what the committed
  `error(...)` path uses, likely `ERROR_OPERATION_UNDEFINED` or the C47
  name-error code the override already references).
- Count cap `fdict.count >= 0x6F00` → `ERROR_RAM_FULL` (line 944-945).
- `forthDictEnsure` 64 KB wrap (`here + need > 0xFFFE`) → `ERROR_RAM_FULL`
  (§3.3.8 item 1).

**Tests:**
- `test_dict_name_too_long`: `: <32+ byte name> ...` → definition abandoned,
  error displayed, ASLIFT **not** set as success.
- `test_dict_space_full`: fill to `0x6F00` count (or force a small region and
  overflow `forthDictEnsure`) → `ERROR_RAM_FULL`, no silent success.
- `test_number_then_no_label_fallthrough`: a classified number that fails to
  push must NOT fall through to label lookup (C-8 classify-gate).

---

### FIX #8: Prefix-match bug (SQ vs SQUARE)

**Spec:** §4.1 uses `compareString(CMP_BINARY)` = whole-string, case-sensitive.

**Bug:** `forthFindColon` memcmp's only `hdr->nameLen` bytes → SQUARE matches SQ.

**Fix:** require `strlen(query) == hdr->nameLen` before the memcmp (or use the
`compareString` whole-string path per §4.1).

**Test:** `test_prefix_no_match`: define `SQ`, look up `SQUARE` →
`forthFindColon` returns false (undefined-word path), does not run SQ.

---

### FIX #9: Number grammar (signed exponents; mantissa-less e)

**Spec:** §3.3.5 grammar (line 831): `real := [+-]? ( digit+ '.' digit* |
'.' digit+ | digit+ ) ( [eE] [+-]? digit+ )?` and must contain `.` or `e/E` to
be real. `[SPEC-VERIFIED]` A sign after `e` is VALID.

**Bugs:** (a) code has no arm for `+/-` after `e` → `1e-5` errors as undefined;
(b) exponent digits counted as mantissa → `e5`, `.e5`, `3e` classify as REAL and
push NaN.

**Fix:** track mantissa digits and exponent digits separately (require ≥1 each);
accept one sign immediately after `e/E`.

**Tests:**
- `test_number_1e_minus_5`: `1e-5` → real 0.00001, dtReal34, no error.
- `test_number_bad_e5`: `e5` → undefined-word (not NaN).
- `test_number_bad_dot_e5`: `.e5` → undefined-word (not NaN).
- `test_number_bad_3e`: `3e` → undefined-word (not NaN).

`[SPEC-NOTE]` "undefined-word" is the correct outcome per §4.1: a token that
fails number classification falls through to label lookup, then the
undefined-word error. Do NOT expect a dedicated "invalid number" error unless
the committed code raises one.

---

### FIX #10: UNDO rows (US_UNCHANGED → US_ENABLED)

**Spec:** §0.2 line 98 ("Use `US_ENABLED`") and the exact H1 rows (§0.2 lines
124-129) — both `ITM_FORTH` and `ITM_FCALL` carry `US_ENABLED`.

**Bug:** committed items.c rows use `US_UNCHANGED` → no undo snapshot → UNDO
after a Forth word restores stale state.

**Fix:** one token per row: `US_UNCHANGED` → `US_ENABLED` at items.c:4704-4705.

**Test:** `test_undo_after_forth` — requires full UNDO integration; DEFER to
§8.5 (tracked). Verify the two item rows literally read `US_ENABLED` (a static
grep/assert on the override is a cheap stand-in now).

---

### FIX #11: Reset hook — covered under Fix #2+#11 above.

---

### FIX #12: DMCP key poll (pollProgramInterrupt)

**Spec:** §3.2 "Cooperative break & key poll" (lines 491-529) — exact recipe:
`C47PopKeyNoBuffer(DISPLAY_WAIT_FOR_RELEASE)+1`; key 36 (R/S) / 33 (EXIT) set
`programRunStop = PGM_WAITING` and return true; other keys `setLastKeyCode`,
return false; all under `#if defined(DMCP_BUILD)`. Runaway cap stays as backstop.

**Bug:** forth_inner.c:166 has a bare `programRunStop` check; no key poll → R/S
can't interrupt a word on hardware.

**Fix:** implement `pollProgramInterrupt()` per the §3.2 recipe; call once per
dispatch at the loop top under `DMCP_BUILD`.

**Test:** `test_key_poll_dmcp` — hardware/emulation only; DEFER to hardware
acceptance. PC path unchanged (poll compiles out).

---

## TEST INTEGRITY FIXES (#13-#15)

### FIX #13: Coverage for FTOK_LIT and FTOK_C47

**Spec:** §2.2 (FTOK_LIT = 16 bytes; FTOK_C47 cell-padded, PTP_NONE/
PARAM_NUMBER_8/PARAM_NUMBER_16 only; other PTP → ERROR_OPERATION_UNDEFINED);
§8 rows C2, C-1.

**Tests (hand-assembled):**
```c
void test_lit_roundtrip(void) {
    // Hand-assemble FTOK_LIT + 16-byte real34 payload + a live token after.
    // Assert the real34 value pushed AND ip advanced by 16 (the live token runs).
    // Escaping mutation: ip += 8 -> the trailing token is misread.
}
void test_c47_ptp_none(void) {
    // FTOK_C47 + item(PTP_NONE); assert dispatch + ip += 2 (no param).
}
void test_c47_ptp_number8_padded(void) {
    // FTOK_C47 + item(PARAM_NUMBER_8) + value + pad; assert ip += 2 past param.
}
void test_c47_bad_ptp(void) {
    // FTOK_C47 + item(PTP_LABEL) -> ERROR_OPERATION_UNDEFINED, returns.
}
void test_c47_nested_reentry(void) {
    // FTOK_C47 -> ITM_FCALL -> forthInner re-entry -> guard fires
    // (ERROR_OPERATION_UNDEFINED). The REAL path, not a test-only setter.
}
void test_outer_real_literal(void) {
    // forthOuterInterpret("2.5 2 *") -> X dtReal34 == 5.
}
```

---

### FIX #14: Vacuous tests (add assertions)

- `test_0br_longint` (~231-261): add sentinel + X assert (covered in #3).
- `test_div_zero_halt` (~294-333): assert `lastErrorCode == ERROR_DIVIDE_BY_ZERO`
  AND `X == 42` (original) AND `X != 999` (sentinel not reached),
  unconditionally — not the current early-pass on `!error`.
- `test_dict_reloc` (~846-851): `assert(relocObserved)` instead of WARN, so §7.2
  ("grow across a move") is actually gated.

---

### FIX #15: Heap corruption & harness hygiene

**Spec:** §7 acceptance; harness must gate via exit(0)/exit(1).

**Action:**
```bash
make test_asan CUSTOM_PKG=packages/forth-core
# Find the double-free (prime suspect: test_xeq_precedence grows labelList via
# realloc, restores numberOfLabels but leaves the region grown; next
# scanLabelsAndPrograms frees with the old count-based size -> mismatched
# freeC47Blocks, manage.c:110).
```
- Fix the real double-free; remove the fork guards and the skipped final free.
- Gate `FORTH_DEBUG_SELFTEST` behind a meson OPTION (not always-on project-wide),
  so a GUI session doesn't run the suite (and a corrupt heap) on every boot.

---

## MUTATION-TEST CHECKLIST (final gate)

For each fix, before DONE:
- [ ] Test exists, named for its escaping mutation.
- [ ] Mutation named in the test comment.
- [ ] Test fails under the mutation (verified by temporary revert).
- [ ] No vacuous assertions (specific value/type, not just `!error`).
- [ ] Fix matches DESIGN.md (spec wins on any conflict).
- [ ] Commit cites the Fable finding + the test.

---

## Acceptance Sequence

```
Commit 1: #6      DMCP guard        -> build-test GREEN; make dmcp links clean.
Commit 2: #2+#11  lifecycle         -> test_lifecycle_{pre_init,reset} PASS.
Commit 3: #1+#16  decode            -> test_ilit_{sign_extend,parity} PASS
                                       (dtLongInteger asserted).
Commit 4: #4      fnForthCall       -> test_fnforthcall_interactive PASS.
Commit 5: #5      LBL? UB           -> ASan clean; test (or deferred) noted.
Commit 6: #3      0BR consume       -> hand-assembled loop PASS, no hangs;
                                       BR/0BR int16/cell delta exercised here.
Commit 7: #7-#10  errors/grammar/undo -> tests PASS (undo row grep-asserted).
Commit 8: #12     key poll          -> #ifdef guard; PC unaffected.
Commit 9: #13-#15 coverage/vacuous/heap -> LIT/C47 tests, ASan clean.

RE-AUDIT (Fable spot-check on commits 1-6) -> then full re-audit after 9.
§8.5 SIGN-OFF -> GUI user test (: SQ2 DUP * ; 3 SQ2 -> 9) -> forum post.
```

`[SPEC-NOTE]` The GUI acceptance value: DESIGN.md §7 item 3 uses `: SQ DUP * ;
3 SQ` → **9**. The handoff's `SQ2` is the same test with a different name; both
leave 9 in X. Either is fine; match whichever the forum post/user test uses.

---

**Total scope:** ~800-1000 lines of test code + ~200 lines of fixes. Order is
dependency-bound (the 0BR loop test needs the #3 fix, or it hangs). **Push after
every commit; verify `git log origin/master`.**
