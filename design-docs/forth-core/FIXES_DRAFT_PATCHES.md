# Concrete Fix Patches (Fable Audit 15 Must-Fix Items)

**Format:** For each fix, exact before/after code with line numbers. Stan applies via Qwen (one file per session).

**⚠️ Reviewed against DESIGN.md (authoritative, 2026-07-08 consolidation). Corrections from the first draft are flagged with `[SPEC-CORRECTED]`.**

> **Reconciliation note.** The earlier uploaded `FIXES_DRAFT_PATCHES.md` was the
> **pre-review draft** and still contained five spec violations (ILIT via
> `int32ToReal34`; int32/4-byte branch deltas; `popIsFalse`; single-arg
> `forthFindColon`; made-up `forthCompile`). **This file supersedes it.** A
> parallel Qwen pass caught some but not all of these — the three it missed
> (int16 branch deltas, `forthPopIsZero`, no `ERROR_NAME_TOO_LONG`) are corrected
> here and called out at their fix sites. DESIGN.md wins on any conflict.

Key spec facts that shaped these patches (all from DESIGN.md):
- **FTOK_ILIT** pushes its int32 payload **as a long integer (`dtLongInteger`)** via `forthPushInt32` — NOT `int32ToReal34` (§2.2 token table line, §3.3.5 number-type conformance). The decode bug is a sign-extension error; the fix corrects the *decode*, and `forthPushInt32` (already the committed helper) does the typed push.
- **FTOK_BR / FTOK_0BR** deltas are **int16 (2 bytes), signed, in CELLS**: `delta = i16 at ip; ip += 2; ip += delta*2` (§2.2 token table, §3.2 pseudocode lines 473-474). NOT a 4-byte int32.
- The 0BR zero-test helper is **`forthPopIsZero`** (forth_inner.c:35-40), and it must be **type-dispatched** (long integer / real34 / complex) like upstream `compareRegisters` — NOT raw `real34IsZero` (T1-1, §3.2). This is the same helper the audit calls `popIsFalse`.
- **`forthFindColon`** signature is `bool forthFindColon(const char *name, uint16_t *widx)` — returns bool, index via out-param, skips `FF_SMUDGE` (§4.1 step 2, C-5). NOT a single-arg call.
- Nested-entry / re-entrancy guard raises **`ERROR_OPERATION_UNDEFINED`** (C-12, forth_inner.c:105) — do NOT touch the working guard.
- §6.2 Reset Hook is **already documented in DESIGN.md** (lines 1239-1252) — fix #11 lands the *code*, not the doc (the doc block exists; verify code matches it).

---

## COMMIT 1: Fix #6 (DMCP Build: Guard test file)

**File:** `packages/forth-core/test_dict_reloc.c`

**Location:** Top of file (after the existing header comment, before includes).

DESIGN.md §6 lists exactly five runtime custom sources (`forth_dict.c`, `forth_prims.c`, `forth_inner.c`, `forth_compile.c`, `forth_bridge.c`); the test harness is not among them and must never reach the cross target.

**BEFORE (line 1-10, typical opening):**
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
```

**AFTER (guard the whole file):**
```c
// FORTH test harness — PC debug build only.
// Cross-target (DMCP) must not include fork/waitpid/printf or the
// PC_BUILD-only test helpers (forthDictSetTestInitialBlocks,
// forthTestSetRunning/IsRunning).
#if defined(PC_BUILD)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
```

**AT END OF FILE:**
```c
#endif  // PC_BUILD
```

Belt-and-suspenders alternative (an explicit #error) is REJECTED here: a plain
`#if defined(PC_BUILD) ... #endif` wrapper lets the file compile to an empty TU
on the cross target, which is what meson's source list expects if the file is
still (harmlessly) listed. But per DESIGN.md §6 the cleaner fix is to ALSO keep
`test_dict_reloc.c` out of the cross source list (see meson note below).

**meson note (defer to fix #20 / packaging pass, but flag now):** DESIGN.md §6
`pkg_custom_sources` lists only the five runtime sources — `test_dict_reloc.c`
is NOT among them. If the current `packages/forth-core/meson.build` adds the
test file to `pkg_custom_sources` unconditionally (audit #6 says it does, at
meson.build:4-5), the real fix is to move it into a PC-only branch. The
`#if PC_BUILD` guard above is the minimum to unbreak the build; the meson
scoping is the correct long-term fix and pairs with fix #15 (self-test define
gating).

**Verify:**
```bash
# PC build should still pass.
./packages/forth-core/build-test.sh
# Should exit 0.

# Cross build should no longer pull test symbols into firmware.
make dmcp CUSTOM_PKG=packages/forth-core 2>&1 | head -20
# Should link (test_dict_reloc.o is now an empty TU) or, if also removed
# from the source list, not compile the file at all.
```

---

## COMMIT 2: Fix #2 + #11 (Init/Reset Lifecycle)

### Fix #2a: Static initialization of fdict

**File:** `packages/forth-core/forth_dict.c`

**Location:** The `fdict` definition. DESIGN.md §1.2 (line 217-225) defines the
struct field order as: `base, sizeBlocks, here, latest, count`.

`[SPEC-CORRECTED]` The initializer must match the struct field order in §1.2.
The struct is:
```c
typedef struct {
  uint8_t *base;        // field 0
  uint16_t sizeBlocks;  // field 1
  uint16_t here;        // field 2
  uint16_t latest;      // field 3
  uint16_t count;       // field 4
} forthDict_t;
```

So `latest` (the end-of-chain sentinel field) is field 3, and its "empty"
value is `FORTH_NULL` (0xFFFF). `here`, `count`, `sizeBlocks` are 0 when empty.

**BEFORE:**
```c
forthDict_t fdict;   // note: DESIGN.md §1.2 says `extern forthDict_t fdict;`
                     // in the header — the definition here has no initializer.
```

**AFTER:**
```c
// Static initialization ensures fdict is well-formed even if production never
// calls forthDictInit() before a lookup. On hardware there is no
// FORTH_DEBUG_SELFTEST to trigger the self-test (which is what calls
// forthDictInit on PC), so base must be NULL and latest must be the
// end-of-chain sentinel from the very first instruction.
// Field order per DESIGN.md §1.2: base, sizeBlocks, here, latest, count.
forthDict_t fdict = {
  .base       = NULL,
  .sizeBlocks = 0,
  .here       = 0,
  .latest     = FORTH_NULL,   // 0xFFFF end-of-chain sentinel
  .count      = 0,
};
```

Using designated initializers (`.field = ...`) instead of positional
`{ NULL, 0, 0, FORTH_NULL, 0 }` guards against a future field reorder silently
mis-initializing `latest`.

### Fix #2b: Guard in forthFindColon

**File:** `packages/forth-core/forth_dict.c`

**Location:** Top of `forthFindColon` (forth_dict.c:152-181 per §8 row C5).

`[SPEC-CORRECTED]` The signature is `bool forthFindColon(const char *name,
uint16_t *widx)` (§4.1 step 2), NOT single-arg. The guard returns false (a
clean miss) and must NOT write `*widx`.

**BEFORE:**
```c
bool forthFindColon(const char *name, uint16_t *widx) {
    // ... walks fdict.latest chain, skips FF_SMUDGE ...
    uint16_t off = fdict.latest;
    while (off != FORTH_NULL) {
        forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
        // ...
    }
    return false;
}
```

**AFTER:**
```c
bool forthFindColon(const char *name, uint16_t *widx) {
    // Guard against an uninitialized dict (production hardware path, before
    // any ':' definition has lazily allocated the region). base == NULL means
    // no words exist yet -> clean miss, never a NULL deref.
    if (fdict.base == NULL) return false;

    // ... walks fdict.latest chain, skips FF_SMUDGE ...
    uint16_t off = fdict.latest;
    while (off != FORTH_NULL) {
        forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
        // ...
    }
    return false;
}
```

Note: with fix #2a, `latest == FORTH_NULL` when empty, so the `while` loop
already never executes on an empty dict. The `base == NULL` guard is the
belt-and-suspenders against a partially-initialized state (base NULL but latest
somehow non-sentinel). Both together close the audit #2 hole.

### Fix #11: Reset hook in doFnReset

**File:** `packages/forth-core/config.c` (the `doFnReset` override).

**Location:** End of doFnReset, after the RAM memset + `freeMemoryRegions`
rebuild (audit cites config.c:1520-1534).

`[SPEC-VERIFIED]` DESIGN.md §6.2 (lines 1239-1252) ALREADY documents this hook:
"3. **forthDictInit() is called** (new, as part of the reset sequence)." So the
DOC is landed; this fix lands the CODE to match it. Verify the doc block is
present (it is, in the uploaded DESIGN.md) and does not need re-adding.

**BEFORE:**
```c
void doFnReset(uint16_t confirmation) {
    // ... existing upstream code: memset(ram, ...), freeMemoryRegions(), etc.
    // (no Forth reset)
}
```

**AFTER:**
```c
void doFnReset(uint16_t confirmation) {
    // ... existing upstream code: memset(ram, ...), freeMemoryRegions(), etc.

    // Reset hook (DESIGN.md §6.2): reinitialize the Forth dictionary control
    // block so no stale base/here/latest/count points into the re-zeroed arena.
    forthDictInit();
}
```

**Requires include** (if not already present in the config.c override):
```c
#include "forth_dict.h"  // for forthDictInit()
```

`[SPEC-CORRECTED]` DESIGN.md §6.2 line 1241 says the trigger is "**ON CLEAR** in
C47" — not "Cl CLEAR". Use the correct key name if referenced in comments or
commit messages.

**Override hygiene warning (audit #18, defer but be aware):** the config.c
override must stay byte-identical to upstream except the marked insertion.
DESIGN.md §6 (line 1197-1199) and the §6 hook table require this so upstream
merges stay reviewable. If earlier work added a `memset(globalRegister, ...)`
or whitespace shifts (audit #18), those violate the rule and pollute merges —
flag for the packaging pass. Also: config.c is NOT in the DESIGN.md §6 hook
table (H1-H6) — audit #18 says to add it (self-test hook + this reset hook) or
relocate the hooks. Note this for the doc-drift fix (#21).

### Add Tests (test_dict_reloc.c)

`[SPEC-CORRECTED]` All `forthFindColon` calls use the two-arg signature with a
`uint16_t widx` out-param.

```c
// Test #1: Pre-init guard
void test_lifecycle_pre_init(void) {
    // Simulate hardware: no forthDictInit() yet. Hand-set fdict to the BSS/
    // static-init state (matches fix #2a).
    fdict = (forthDict_t){ .base = NULL, .sizeBlocks = 0, .here = 0,
                           .latest = FORTH_NULL, .count = 0 };

    // Attempt a lookup (the H2/H3 path that segfaults today). Should return
    // false cleanly, not dereference NULL.
    uint16_t widx = 0xDEAD;                 // sentinel; must stay untouched
    bool found = forthFindColon("DOESNOTEXIST", &widx);
    assert(!found);                          // guard returned a clean miss
    assert(fdict.base == NULL);              // still uninitialized
    assert(widx == 0xDEAD);                  // out-param NOT written on miss
}

// Test #2: Reset lifecycle
void test_lifecycle_reset(void) {
    // Stage a word: SQ
    forthDictInit();
    // (Compile via the outer interpreter, matching the real path.)
    // forthOuterInterpret is the directly-callable core (§3.3.2).
    forthOuterInterpret(": SQ DUP * ;");
    assert(lastErrorCode == ERROR_NONE);

    uint16_t widx = 0;
    assert(forthFindColon("SQ", &widx) == true);

    // Simulate RESET (mimic doFnReset's sequence).
    // NOTE: use the same arena teardown doFnReset uses; if the test can't call
    // the full doFnReset, at minimum re-run forthDictInit() (the reset hook).
    forthDictInit();                         // reset hook (fix #11)

    // After reset, SQ is gone (dict control block cleared).
    assert(forthFindColon("SQ", &widx) == false);

    // And we can define again without corruption.
    forthOuterInterpret(": SQ DUP * ;");
    assert(lastErrorCode == ERROR_NONE);
    assert(forthFindColon("SQ", &widx) == true);
}
```

**Register in the harness** (however the harness runs tests — a RUN_TEST macro,
an array, or direct calls in main):
```c
RUN_TEST(test_lifecycle_pre_init);
RUN_TEST(test_lifecycle_reset);
```

**Escaping mutations (name them in the test comments):**
- Remove the `if (fdict.base == NULL) return false;` guard → `test_lifecycle_pre_init`
  dereferences NULL (crash/ASan).
- Remove `forthDictInit()` from the reset path → `test_lifecycle_reset` finds a
  stale SQ or corrupts on the second define.

**Verify:**
```bash
./packages/forth-core/build-test.sh 2>&1 | grep -E "test_lifecycle"
# Both should PASS.
```

---

## COMMIT 3: Fix #1 + #16 (FTOK_ILIT & BR/0BR Decode)  [SPEC-CORRECTED — major]

**File:** `packages/forth-core/forth_inner.c`

This commit fixes THREE decode sites. The first draft of this patch had two
serious spec errors, both corrected below:

1. **`[SPEC-CORRECTED]` FTOK_ILIT must push a long integer, not a real34.**
   The decode-side fix is the memcpy (kills the sign-extension). The PUSH must
   go through `forthPushInt32`, which DESIGN.md §3.3.5 (line 813-817) requires
   to build a `dtLongInteger` via `convertLongIntegerToLongIntegerRegister` —
   NOT `int32ToReal34`. Do NOT write `pushReal34(int32ToReal34(v))`.

2. **`[SPEC-CORRECTED]` BR/0BR deltas are int16 (2 bytes), in CELLS.**
   DESIGN.md §2.2 (token table) and §3.2 (pseudocode lines 473-474):
   `delta = i16 at ip; ip += 2; ip += delta*2`. The first draft used a 4-byte
   int32 and `ip += 4` — wrong width AND wrong stride. The sign-extension bug
   the audit found (#16) is real, but the fix is to load a proper **int16_t**
   via memcpy, not an int32.

### Fix #1: FTOK_ILIT decode (forth_inner.c ~line 253)

The bug (audit #1): `(int8_t)fdict.base[ip]` sign-extends the low byte before
OR-ing the higher bytes, forcing bits 8-31. Fix: memcpy the 4 bytes into an
int32, then push via the existing typed helper.

**BEFORE:**
```c
case FTOK_ILIT: {
    // BUGGY: sign-extends low byte.
    int32_t v = (int32_t)((int8_t)fdict.base[ip]
                          | ((int32_t)fdict.base[ip+1] << 8)
                          | ((int32_t)fdict.base[ip+2] << 16)
                          | ((int32_t)fdict.base[ip+3] << 24));
    // ... and (per audit) may have pushed via int32ToReal34 — WRONG type.
    ip += 4;
    break;
}
```

**AFTER:**
```c
case FTOK_ILIT: {
    // 4-byte signed int32 payload, LE, via memcpy (no sign-extend).
    // Mirrors the correct emit side.
    int32_t v;
    memcpy(&v, fdict.base + ip, 4);
    ip += 4;
    // Push AS A LONG INTEGER (dtLongInteger) — matches keyboard entry and
    // interpret-state semantics. forthPushInt32 does the typed store via
    // convertLongIntegerToLongIntegerRegister (DESIGN.md §3.3.5); it is NOT
    // int32ToReal34.
    forthPushInt32(v);
    break;
}
```

If the committed code reads the payload directly rather than into a temporary,
the essential change is: replace the `(int8_t)`-cast byte assembly with
`memcpy(&v, fdict.base + ip, 4)`. If `forthPushInt32` still does `int32ToReal34`
internally, that is a SEPARATE required change tracked as audit T1-2 / §3.3.5
(remove `static`, build a `longInteger_t`, store via
`convertLongIntegerToLongIntegerRegister`). Confirm forthPushInt32's body during
this commit — the audit's "what conforms" list says "ILIT→dtLongInteger via the
closeNim idiom (T1-2 landed)", so it may already be correct. If it is, the ILIT
fix is purely the memcpy decode.

### Fix #16a: FTOK_BR delta decode (forth_inner.c ~line 268)

`[SPEC-CORRECTED]` int16, `ip += 2`, delta in cells (`ip += delta*2`).

**BEFORE:**
```c
case FTOK_BR: {
    int8_t delta = (int8_t)fdict.base[ip];   // BUGGY: 8-bit sign-extend.
    ip += 1;                                  // WRONG stride.
    ip += delta;                              // WRONG: delta is in cells.
    break;
}
```

**AFTER:**
```c
case FTOK_BR: {
    // Signed 16-bit cell delta, relative to the cell AFTER the delta field.
    int16_t delta;
    memcpy(&delta, fdict.base + ip, 2);
    ip += 2;
    ip += (int32_t)delta * 2;   // delta counts CELLS; body is byte-addressed.
    break;
}
```

### Fix #16b: FTOK_0BR delta decode (forth_inner.c ~line 277)

`[SPEC-CORRECTED]` Same int16/cell fix, AND the zero-test goes through the
type-dispatched helper `forthPopIsZero` (see fix #3 for that helper's body).

**BEFORE:**
```c
case FTOK_0BR: {
    int8_t delta = (int8_t)fdict.base[ip];   // BUGGY.
    ip += 1;                                  // WRONG stride.
    // ... zero test that may not consume / may be raw real34IsZero ...
    if (/* zero */) ip += delta;              // WRONG stride.
    break;
}
```

**AFTER:**
```c
case FTOK_0BR: {
    // Signed 16-bit cell delta, relative to the cell AFTER the delta field.
    int16_t delta;
    memcpy(&delta, fdict.base + ip, 2);
    ip += 2;
    // forthPopIsZero: type-dispatched zero test that POPS X (see fix #3).
    if (forthPopIsZero()) {
        ip += (int32_t)delta * 2;   // CELLS -> bytes.
    }
    break;
}
```

### Add Tests (test_dict_reloc.c)

```c
// Test #1: ILIT sign-extend fix (the escaping mutation: low byte >= 0x80).
void test_ilit_sign_extend(void) {
    forthDictInit();
    // : TEST128 128 ;  -> ILIT with low byte 0x80.
    forthOuterInterpret(": TEST128 128 ;");
    assert(lastErrorCode == ERROR_NONE);

    // Execute (interactive/test entry is fine; forthPushInt32 sets X).
    // Use the harness's word-run helper; here we call forthInner by index.
    uint16_t widx = 0;
    assert(forthFindColon("TEST128", &widx));
    forthInner(widx, false);
    assert(lastErrorCode == ERROR_NONE);

    // X must be 128, NOT -128. And it must be a long integer (dtLongInteger),
    // matching keyboard entry (§3.3.5).
    assert(getRegisterDataType(REGISTER_X) == dtLongInteger);
    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 128) == 0);   // CRITICAL: not -128.
    longIntegerFree(li);
}

// Test #2: compiled vs interpreted arithmetic agree (128 with high bit set).
void test_ilit_compile_interpret_parity(void) {
    forthDictInit();
    // Compiled: : W 128 + ;
    forthOuterInterpret(": W 128 + ;");
    assert(lastErrorCode == ERROR_NONE);

    // Seed X = 42, run compiled W -> expect 170.
    forthPushInt32(42);
    uint16_t widx = 0;
    assert(forthFindColon("W", &widx));
    forthInner(widx, false);
    assert(lastErrorCode == ERROR_NONE);
    // Read X as long integer, expect 170.
    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 170) == 0);
    longIntegerFree(li);

    // Interpreted: 42 128 +  -> also 170. (Clean stack first.)
    // fnDrop or reset X as the harness allows.
    forthPushInt32(42);
    forthOuterInterpret("128 +");
    assert(lastErrorCode == ERROR_NONE);
    longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 170) == 0);
    longIntegerFree(li);
}
```

`[SPEC-CORRECTED]` BR/0BR delta tests are **deferred**, and DESIGN.md is
explicit about why: the sub-phase C compiler NEVER emits FTOK_BR/FTOK_0BR
(§3.3 "Emit scope (C-1)", §7 acceptance item 3). Branch tokens are exercised
ONLY by hand-assembled bodies (the Stage-1-B backward-loop test). So:
- The #16 delta fix is still correct to land now (it's a latent bug), but its
  test must be a **hand-assembled body**, not compiled source.
- The canonical hand-assembled body per DESIGN.md §2.2 (line 334):
  `DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`, counter 5 → 0 over 5 iterations.
- This overlaps fix #3's test work — land the hand-assembled loop test there
  (Commit 6), where the 0BR consume + delta stride are both exercised together.

**Verify:**
```bash
./packages/forth-core/build-test.sh 2>&1 | grep -E "test_ilit"
# Both should PASS.
```

**Escaping mutations:**
- Revert ILIT decode to `(int8_t)`-cast assembly → `test_ilit_sign_extend` fails (X = -128).
- Change `forthPushInt32` to `int32ToReal34` → `test_ilit_sign_extend`'s
  `dtLongInteger` assertion fails.

---

## COMMIT 4: Fix #4 (fnForthCall fromProgram)

**File:** `packages/forth-core/forth_bridge.c`

**Location:** `fnForthCall` (forth_bridge.c:12).

`[SPEC-VERIFIED]` DESIGN.md §3.3.2 (line 734) and the §3.3 pseudocode (line 634)
both give the exact idiom for the interpret-state colon call:
`forthInner(widx, programRunStop == PGM_RUNNING)`. The bridge entry must use the
same expression (audit #4's fix C-5).

**BEFORE:**
```c
void fnForthCall(uint16_t param) {
    // ... bounds check ...
    forthInner(param, true);   // BUGGY: hardcoded true. Interactive entry
                               // (programRunStop == PGM_STOPPED) exits before
                               // the first token.
}
```

**AFTER:**
```c
void fnForthCall(uint16_t param) {
    // ... bounds check (keep the existing ERROR_INVALID_CORRUPTED_DATA arm) ...
    // fromProgram is true ONLY when a running program invoked us. Interactive
    // XEQ / TAM entry has programRunStop != PGM_RUNNING, so pass the real
    // context (DESIGN.md §3.3 pseudocode line 634 / §3.3.2 line 734).
    forthInner(param, programRunStop == PGM_RUNNING);
}
```

### Correct Stage-H1.md (root of the bug)

**File:** `design-docs/forth-core/Stage-H1.md` (if present in the tree).

Audit #4 notes Stage-H1.md step 11 contains `forthInner(param, true)` — the
stage-doc bug that a future rebuild would re-transcribe. DESIGN.md is the single
authority and already has the correct form, but fix the stage doc too so the
two agree.

**BEFORE:**
```
Step 11: forthInner(param, true);
```

**AFTER:**
```
Step 11: forthInner(param, programRunStop == PGM_RUNNING);
         // Pass true only from a running program, not interactive entry.
         // (Matches DESIGN.md §3.3 pseudocode line 634.)
```

### Add Test (test_dict_reloc.c)

```c
void test_fnforthcall_interactive(void) {
    forthDictInit();
    forthOuterInterpret(": ADDTEN 10 + ;");
    assert(lastErrorCode == ERROR_NONE);

    uint16_t widx = 0;
    assert(forthFindColon("ADDTEN", &widx));

    // Interactive entry: programRunStop = PGM_STOPPED (NOT running).
    programRunStop = PGM_STOPPED;
    forthPushInt32(5);              // X = 5
    fnForthCall(widx);              // Should execute ADDTEN -> X = 15.
    assert(lastErrorCode == ERROR_NONE);

    longInteger_t li; longIntegerInit(li);
    convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
    assert(longIntegerCompareInt(li, 15) == 0);   // executed, not a no-op.
    longIntegerFree(li);
}
```

**Escaping mutation:** revert to `forthInner(param, true)` → the interactive
call exits before the first token, X stays 5, test fails.

**Verify:**
```bash
./packages/forth-core/build-test.sh 2>&1 | grep test_fnforthcall_interactive
# Should PASS.
```

---

## COMMIT 5: Fix #5 (LBL? UB: Uninitialized resolvedParam)

**File:** `packages/forth-core/programming/lblGtoXeq.c`

**Location:** The H2 override (audit cites lines 366-370).

`[SPEC-VERIFIED]` DESIGN.md §4.2 (line 1084-1090) specifies the H2 fallback:
in the `PARAM_LABEL`/`STRING_LABEL_VARIABLE` arm, after `findNamedLabel` misses,
try `forthFindColon`; on hit `reallyRunFunction(ITM_FCALL, widx)`. §4.2 also
fixes the REVERSE-lookup precedence: **C47 label first, Forth colon second**
(opposite of §4.1) — so existing programs never change meaning.

The audit's UB: `resolvedParam` is uninitialized and passed to
`reallyRunFunction` even when nothing resolved. And for `ITM_LBLQ`, a Forth-word
hit would pass the colon index as if it were a label ID (type confusion).

`[SPEC-NOTE]` DESIGN.md's §4.2 describes the fallback for XEQ/GTO execution. The
`LBL?` (ITM_LBLQ) predicate is upstream's "does this label exist" test; when the
name is neither a C47 label nor (per the fix) a Forth word usable as a label,
`LBL?` must report false the upstream way — via `INVALID_VARIABLE`, not garbage.
The forthResolveXEQ return contract (FORTH_XEQ_LABEL / FORTH_XEQ_NONE /
FORTH_XEQ_FORTH) is the committed mechanism (audit "what conforms" confirms
forthResolveXEQ precedence).

**BEFORE:**
```c
uint16_t resolvedParam;                       // UNINITIALIZED
int res = forthResolveXEQ(op, name, &resolvedParam);
if (res == FORTH_XEQ_LABEL) {
    // resolvedParam set here (only)
}
reallyRunFunction(op, resolvedParam);         // UB if not a label hit
```

**AFTER:**
```c
// Initialize to upstream's "not found" marker so a miss reports false the
// normal way instead of reading stack garbage (audit #5).
uint16_t resolvedParam = (uint16_t)INVALID_VARIABLE;
int res = forthResolveXEQ(op, name, &resolvedParam);

if (res == FORTH_XEQ_LABEL) {
    // A real C47 label. Safe to run / test.
    reallyRunFunction(op, resolvedParam);
} else if (res == FORTH_XEQ_FORTH) {
    // Name is a Forth word. For XEQ/GTO this dispatches the word (per §4.2);
    // for LBL? it is a type mismatch — a Forth word is not a C47 label, and
    // passing the colon index as a label ID is the upstream contract
    // violation the audit flagged. Report the predicate false (or the
    // stage-appropriate error) rather than running LBL? on a bad ID.
    if (op == ITM_LBLQ) {
        // LBL? of a Forth word: not a label. Fall through to the upstream
        // "false" path with INVALID_VARIABLE (no garbage index).
        reallyRunFunction(op, (uint16_t)INVALID_VARIABLE);
    } else {
        // XEQ/GTO: dispatch the Forth word via the bridge (§4.2 forward path).
        reallyRunFunction(ITM_FCALL, resolvedParam);
    }
} else {
    // FORTH_XEQ_NONE: neither label nor Forth word. resolvedParam is still
    // INVALID_VARIABLE, so this is the normal upstream "not found" behavior.
    reallyRunFunction(op, resolvedParam);
}
```

`[SPEC-NOTE]` The exact branch shape depends on how the committed override is
structured (audit says H3's copy has no LBLQ path and is safe, so only H2 needs
the LBLQ guard). Preserve the existing upstream control flow; the essential two
changes are (1) initialize `resolvedParam = INVALID_VARIABLE`, (2) never pass a
Forth colon index to an `ITM_LBLQ` dispatch. Confirm against §4.2's precedence
(C47 label first) — do not reorder the resolve.

### Add Test (test_dict_reloc.c)

The audit notes zero tests exercise H2 today, and a proper test needs a program
context. Add a focused unit check where callable; otherwise mark it explicitly
deferred to the §8.5 integration pass (do NOT leave it as a silent gap).

```c
void test_lblq_undefined_no_ub(void) {
    // LBL? 'NONEXISTENT' must report false with no uninitialized read.
    // Best run under ASan/valgrind to catch the UB directly.
    forthDictInit();
    // If the H2 override entry is unit-callable, invoke it here with a name
    // that is neither a label nor a Forth word, and assert the false result.
    // If not unit-callable in the harness, this test is a DEFERRED integration
    // item (§8.5, full program context) — tracked, not forgotten.
}
```

**Escaping mutation:** remove the `= (uint16_t)INVALID_VARIABLE` initializer →
under ASan, the uninitialized read is flagged.

**Verify:**
```bash
./packages/forth-core/build-test.sh          # exit 0
make test_asan CUSTOM_PKG=packages/forth-core 2>&1 | grep -i uninitialized
# Should print nothing (no uninitialized-read report).
```

---

## COMMIT 6: Fix #3 (FTOK_0BR Consumer + forthPopIsZero)  [SPEC-CORRECTED]

**File:** `packages/forth-core/forth_inner.c` (+ test body in test_dict_reloc.c)

`[SPEC-CORRECTED]` Three spec facts the first draft got wrong or vague:

1. The helper is **`forthPopIsZero`** (forth_inner.c:35-40 per T1-1), not
   `popIsFalse`. The audit uses the name "popIsFalse" descriptively; the code
   symbol is `forthPopIsZero`.
2. It must be **type-dispatched** (long integer / real34 / complex …) like
   upstream `compareRegisters` (mathematics/compare.c:505) — NOT raw
   `real34IsZero` on X's data, which misreads a `dtLongInteger` X (T1-1, §3.2
   line 475-479).
3. **0BR CONSUMES its operand** (DESIGN.md §2.2 line 329-331: "FTOK_0BR
   CONSUMES its operand: pops X, branches if zero/false"). So `forthPopIsZero`
   must pop X.

The audit's finding (#3): the committed code does NOT consume, and a rewritten
test with a bogus comment enshrines the wrong behavior. The upstream `_Drop`
DOES have standard semantics (the comment's claim is false). Fix: make
`forthPopIsZero` type-dispatch AND pop, then restore the canonical test body.

### Fix #3: forthPopIsZero — type-dispatch + consume

**BEFORE (forth_inner.c:35-40, approximate):**
```c
static bool forthPopIsZero(void) {
    // BUGGY: raw real34 test, does NOT pop, misreads long integers.
    real34_t *x = REGISTER_REAL34_DATA(REGISTER_X);
    return real34IsZero(x);
}
```

**AFTER:**
```c
static bool forthPopIsZero(void) {
    // Type-dispatched zero/false test, mirroring upstream compareRegisters
    // (mathematics/compare.c:505). Handles dtLongInteger (which FTOK_ILIT and
    // FTOK_C47 escapes leave in X), dtReal34, and others uniformly. Then POPS
    // X, because FTOK_0BR consumes its operand (DESIGN.md §2.2). IF compiles a
    // DUP before 0BR precisely because 0BR pops.
    bool isZero;
    uint32_t t = getRegisterDataType(REGISTER_X);
    if (t == dtLongInteger) {
        longInteger_t li; longIntegerInit(li);
        convertLongIntegerRegisterToLongInteger(REGISTER_X, li);
        isZero = (longIntegerCompareInt(li, 0) == 0);
        longIntegerFree(li);
    } else if (t == dtReal34) {
        real34_t *x = REGISTER_REAL34_DATA(REGISTER_X);
        // Masked zero test: real34IsZero catches +0; also treat -0 as zero
        // (sign-magnitude). Use the same predicate upstream compare uses.
        isZero = real34IsZero(x);
    } else {
        // Other types: route through the upstream comparison machinery, or
        // (conservatively) treat as non-zero. Prefer the compareRegisters
        // path so complex/short-integer X behave like the rest of the machine.
        isZero = forthRegisterXIsZeroViaCompare();   // helper wrapping compareRegisters
    }
    fnDrop(NOPARAM);        // CONSUME the tested value (0BR pops).
    return isZero;
}
```

`[SPEC-NOTE]` If a full `compareRegisters` wrapper is too heavy for stage 1, the
minimum correct behavior per §3.2 is: dtLongInteger and dtReal34 handled
explicitly (the two types stage-1 Forth actually produces), with a documented
fallback for others. The audit's specific escaping mutation is "raw
`real34IsZero` on a long-int" — the dtLongInteger arm above is what kills it.

`[CAUTION]` Confirm the FTOK_0BR arm (fix #16b) calls `forthPopIsZero()` and
does NOT also pop separately (double-drop). The consume lives in
`forthPopIsZero` now.

### Fix #3b: Restore the canonical test body

**File:** `packages/forth-core/test_dict_reloc.c` (test_branch_back, ~169-225)

`[SPEC-VERIFIED]` DESIGN.md §2.2 line 334 gives the canonical body exactly:
`DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`, counter 5 → 0 over 5 iterations.

**BEFORE (buggy body that hangs under a consuming 0BR):**
```c
// DUP ILIT 1 MINUS 0BR BR  -- DUP consumed by MINUS; a spec-conforming
// consuming 0BR then loops forever. The test was rewritten to the bug.
```

**AFTER (hand-assembled canonical body):**
```c
// Canonical backward-loop body (DESIGN.md §2.2 line 334):
//   DUP  0BR(+6)  ILIT(-1)  +  BR(-9)  EXIT
// Counter starts at 5, decremented to 0 over 5 iterations, terminates.
// 0BR CONSUMES the DUP'd copy; the original counter stays for +.
// Deltas are in CELLS, signed, relative to the cell after the delta.
ftoken_t body[] = {
    /* DUP  */ (ftoken_t)(FORTH_PRIM_DUP_INDEX + 1),
    /* 0BR  */ FTOK_0BR, (ftoken_t)(int16_t)+6,
    /* ILIT */ FTOK_ILIT, 0xFFFF, 0xFFFF,      /* int32 -1, LE, 2 cells */
    /* +    */ (ftoken_t)(FORTH_PRIM_PLUS_INDEX + 1),
    /* BR   */ FTOK_BR, (ftoken_t)(int16_t)-9,
    /* EXIT */ FTOK_EXIT,
};
// (Use the real prim indices from forthPrims[]; the +1 is FTOK_PRIM_BASE.)
// Assemble into a dict entry via the test's hand-assembly helper, seed X=5,
// run forthInner, assert X == 0 and stack depth unchanged.
```

`[SPEC-NOTE]` Verify the exact deltas against the assembled cell layout — the
+6/-9 in the DESIGN.md example assume a specific token sequence. Recompute from
the actual `body[]` you assemble (delta = target_cell - cell_after_delta).

### Fix #3c: De-vacuous test_0br_longint (audit #14a)

**File:** `packages/forth-core/test_dict_reloc.c` (~231-261)

**BEFORE:**
```c
// Asserts only "no error"; passes under its own named mutation.
assert(lastErrorCode == ERROR_NONE);
```

**AFTER:**
```c
// Put a sentinel on the not-taken path and assert X, so a mis-branch or a
// raw-real34 zero-test on the long-int is caught.
assert(lastErrorCode == ERROR_NONE);
// e.g. if X was a long-integer 0, 0BR must branch; assert the branch target
// ran (sentinel value) and the not-taken sentinel did NOT run.
assert(/* X equals the taken-path sentinel, a specific non-ambiguous value */);
```

### Add Tests (test_dict_reloc.c)

```c
void test_0br_consumes(void) {
    // Hand-assemble: DUP 0BR(skip) ILIT 999 EXIT  (+ a taken path)
    // With X = 0: 0BR consumes the DUP'd 0 and branches past ILIT 999.
    // Assert X == 0 (the original, DUP'd copy consumed) and 999 NOT pushed.
    // With X != 0: 0BR consumes, does not branch, ILIT 999 executes.
    // (Hand-assembled body; see fix #3b pattern.)
}

void test_0br_longint_sign_magnitude(void) {
    // X = long-integer 0 must be seen as zero by the type-dispatched test.
    // Escaping mutation: raw real34IsZero on the long-int data misreads it.
}
```

**Escaping mutations:**
- Remove `fnDrop(NOPARAM)` from `forthPopIsZero` → `test_0br_consumes` fails
  (value not consumed) or `test_branch_back` hangs (DUP accumulates).
- Revert to raw `real34IsZero` → `test_0br_longint_sign_magnitude` fails on a
  long-integer zero.
- Revert the test body to `DUP ILIT 1 MINUS 0BR BR` → `test_branch_back` hangs
  (infinite loop), which the harness's runaway cap turns into a FAIL.

**Verify:**
```bash
./packages/forth-core/build-test.sh 2>&1 | grep -E "test_0br|test_branch_back"
# All PASS, no hangs (finite time).
```

---

## Summary: Commit Order (unchanged; matches Fable's suggested order)

```
1. #6      — Unbreak the DMCP target build (guard test file).
2. #2+#11  — Init/reset lifecycle (static fdict, guard, reset hook).
3. #1+#16  — Decode fixes (ILIT memcpy + long-int push; BR/0BR int16/cells).
4. #4      — fnForthCall fromProgram.
5. #5      — LBL? uninitialized param (UB).
6. #3      — 0BR consumes (forthPopIsZero type-dispatch + pop) + canonical body.
7. #7-#12  — Errors, grammar, undo, key-poll (Phase 3).
8. #13-#15 — Coverage, vacuous tests, heap (Phase 3/4).
```

---

## Notes for Qwen Integration

- **One file per session** (avoid context overflow/compaction).
- DESIGN.md is the single authority — if any patch here disagrees with DESIGN.md,
  DESIGN.md wins; stop and flag it.
- After each commit: `git log --oneline | head -1`, run
  `./packages/forth-core/build-test.sh` (gate), then `git push origin master`.
- **Mutations:** after a fix lands, verify the named test fails under the named
  mutation (temporarily revert, watch it fail, restore).
- Signatures to respect verbatim: `forthFindColon(name, &widx)` (bool + out-param),
  `forthInner(index, fromProgram)`, `forthPushInt32(int32)` (pushes dtLongInteger),
  `forthPopIsZero()` (type-dispatched, consumes X).

## QA Checklist (per commit)

- [ ] Line numbers re-grepped against the current tree (files may have shifted).
- [ ] Patch matches DESIGN.md (spec wins on any conflict).
- [ ] Mutation named in each test comment; test fails under it.
- [ ] No vacuous assertions (gate on a specific value/type, not just "!error").
- [ ] `build-test.sh` exit 0, no hangs.
- [ ] `git push origin master` succeeds; `git log origin/master` verified.
- [ ] Vanilla build still byte-identical where the file is an override.
