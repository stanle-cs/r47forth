## H1 Integration Commit Series — Ordered Plan

### Spec Ambiguities (flagged, not guessed)

| # | Ambiguity | Impact | Resolution |
|---|-----------|--------|------------|
| A1 | `fnForthOuter` stub behavior: §3.1 says "reads source from alpha reg" but full outer interp is stage 2. Should the stub display `ERROR_OPERATION_UNDEFINED` (consistent with "not yet implemented") or `ERROR_ITEM_TO_BE_CODED` (upstream convention for unimplemented items)? | Bridge test for `ITM_FORTH` XEQ | Plan uses `ERROR_OPERATION_UNDEFINED`; change if you prefer |
| A2 | `forthRentrancySelfTest` needs to test the guard fires on nested entry. Direct `forthInner` calls can't nest (first returns before second enters). The only natural path is FTOK_C47 → Forth item → `forthInner`, which requires H1+bridge wired. Alternative: test-level `#ifdef` to expose `forthRunning` for manual priming. | Test placement | Plan uses FTOK_C47 path in acceptance tests (step 15); adds a direct `forthRunning=true` priming test in step 4b |
| A3 | `forthDictSelfTest` relocation assertion: with `FORTH_INITIAL_BLOCKS=64` (256 bytes), 20 single-char words (12 bytes each = 240 bytes) may or may not trigger realloc. The test currently relies on `FORTH_INITIAL_BLOCKS=8` (32 bytes) to force early realloc. | Step 1 test must still verify relocation | Plan adds a test-local `FORTH_TEST_INITIAL_BLOCKS` override (see step 1) |

---

### Step 1 — FORTH_INITIAL_BLOCKS = 64 + relocation test fix

**DECISION 2**

**Files:** `forth_dict.c`, `test_dict_reloc.c`

**forth_dict.c:5**
```c
// Before:
#define FORTH_INITIAL_BLOCKS 8
// After:
#define FORTH_INITIAL_BLOCKS 64
```

**test_dict_reloc.c:forthDictSelfTest** — the test currently depends on `FORTH_INITIAL_BLOCKS=8` to force realloc after ~3 words. With 64, 20 single-char words (240 bytes) fits in 256 bytes and may not trigger realloc. Fix by adding a test-local override:

At top of `forthDictSelfTest`, before any allocation:
```c
/* Override initial alloc size for this test to force relocation.
   Production uses FORTH_INITIAL_BLOCKS=64; test forces 4 blocks (16 bytes)
   so the first 2-char word (12 bytes header+pad + 2 EXIT = 14 bytes) triggers
   a realloc on the second word. */
#define FORTH_INITIAL_BLOCKS 4
#include "forth_dict.c"  // NO — can't re-include
```

Better approach: add a **test-only helper** in `forth_dict.c`:
```c
#if defined(PC_BUILD)
  static uint16_t testInitialBlocks = 0;  // 0 = use FORTH_INITIAL_BLOCKS
  void forthDictSetTestInitialBlocks(uint16_t blocks) { testInitialBlocks = blocks; }
#endif
```

Modify `forthDictEnsure` first-alloc:
```c
if (fdict.base == NULL) {
    uint16_t initBlocks = FORTH_INITIAL_BLOCKS;
#if defined(PC_BUILD)
    if (testInitialBlocks > 0) initBlocks = testInitialBlocks;
#endif
    fdict.base = allocC47Blocks(initBlocks);
    // ...
    fdict.sizeBlocks = initBlocks;
}
```

In `forthDictSelfTest`, before defining words:
```c
forthDictSetTestInitialBlocks(4);  // 16 bytes — forces realloc after word 1
```

Update the test comment from "FORTH_INITIAL_BLOCKS (=8 blocks, 32 bytes)" to "test-local override: 4 blocks (16 bytes)".

**Build-test:** `./packages/forth-core/build-test.sh` — dict self-test passes, relocation detected.

**Dependencies:** None. Independent fix.

---

### Step 2 — T1-2: forthPushInt32 → dtLongInteger + test assertion updates

**DECISION 1**

**Files:** `forth_inner.c`, `test_dict_reloc.c`

**forth_inner.c:29-35** — replace `int32ToReal34` with long integer idiom:
```c
static void forthPushInt32(int32_t val)
{
    setSystemFlag(FLAG_ASLIFT);
    liftStack();
    clearSystemFlag(FLAG_ASLIFT);
    longInteger_t li;
    longIntegerInit(li);
    int32ToLongInteger(val, li);
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X);
    longIntegerFree(li);
}
```

**test_dict_reloc.c** — add a generic X-reading helper (near top, after includes):
```c
static int32_t readXAsInt32(void)
{
    uint32_t xType = getRegisterDataType(REGISTER_X);
    if (xType == dtLongInteger || xType == dtShortInteger) {
        longInteger_t xLi;
        longIntegerInit(xLi);
        int32_t v = 0;
        if (getRegisterAsLongInt(REGISTER_X, xLi, NULL))
            longIntegerToInt32(xLi, v);
        longIntegerFree(xLi);
        return v;
    } else {
        decContext ctx;
        decContextDefault(&ctx, DEC_INIT_DECIMAL128);
        return decQuadToInt32(REGISTER_REAL34_DATA(REGISTER_X), &ctx, DEC_ROUND_DOWN);
    }
}
```

Update all test assertions that read X as `decQuad` after ILIT operations. Affected tests and lines:

| Test | Location | Current | Change |
|------|----------|---------|--------|
| Stack a | ~230-240 | `decQuadToInt32(REGISTER_REAL34_DATA(REGISTER_X))` | `readXAsInt32()` |
| Stack b | ~292-298 | `decQuadToInt32` + `decQuadToString` | `readXAsInt32()` + note type in output |
| Branch test 6 | ~983-1001 | Already generic (checks type) | **No change needed** |
| C47 test 1 | ~1106-1110 | `decQuadToInt32` + `decQuadToString` | `readXAsInt32()` |
| C47 test 2 | ~1197-1206 | `decQuadToInt32` for X, Y, Z | `readXAsInt32()` (all three) |
| CoopBreak test 1 | ~1303-1309 | `decQuadToInt32` for X, Y | `readXAsInt32()` |
| CoopBreak test 2 | ~1340-1343 | `decQuadToInt32` for X | `readXAsInt32()` |
| CoopBreak test 3 | ~1370-1376 | `decQuadToInt32` for X, Y | `readXAsInt32()` |
| Rstack test 1 | ~1849-1851 | `decQuadToInt32` | `readXAsInt32()` |
| Rstack test 2 | ~1904-1906 | `decQuadToInt32` | `readXAsInt32()` |
| Literal test 1 | ~1485-1499 | `real34Compare` against 4.14 | **Special**: after ILIT fix, `fnAdd` of real34+longInteger may produce either type. Replace `real34Compare` with generic numeric comparison using `readXAsInt32()` and compare against 4 (integer part), OR read as real34 if type is real34, or convert long integer to real34 for comparison. **Flag: A4** (see below) |
| Error test 1 | ~1615-1617 | `decQuadToInt32` | `readXAsInt32()` |

**A4 — Literal test 1 comparison:** `FTOK_LIT 3.14 + ILIT 1 +` — after ILIT fix, X after LIT is real34 (3.14), Y after ILIT is long integer (1). `fnAdd` dispatches `addition[dtLongInteger][dtReal34]()` — need to check what that produces. If it converts to real34, `real34Compare` still works. If it produces long integer, need generic comparison. **Recommendation:** use `readXAsInt32()` and compare against 4 (losing fractional precision) OR add a `readXAsReal34Ep()` that converts long integer → real34 for comparison.

Actually, simpler: add a `readXApprox(int32_t expected)` helper that reads X generically and checks `abs(xVal - expected) <= tolerance`. For the literal test, compare against 4.14 by reading X as real34 if type is real34, or converting from long integer.

**Build-test:** `./packages/forth-core/build-test.sh` — all existing tests pass with new type semantics.

**Dependencies:** None. Independent fix.

---

### Step 3 — C5: forthFindColon skips FF_SMUDGE + C6: forthDictWriteName clamp

**Files:** `forth_dict.c`

**forth_dict.c:152-181 (forthFindColon)** — add smudge check inside the while loop, before name comparison:
```c
while (off != FORTH_NULL) {
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
    if (hdr->flags & FF_SMUDGE) {
        off = hdr->link;
        n++;
        continue;
    }
    // ... existing name comparison ...
}
```

**forth_dict.c:115-120 (forthDictWriteName)** — clamp copy length:
```c
void forthDictWriteName(uint16_t entryOffset, const char *name)
{
    forthHeader_t *hdr = (forthHeader_t *)(fdict.base + entryOffset);
    uint8_t *dst = fdict.base + entryOffset + sizeof(forthHeader_t);
    size_t len = strlen(name);
    if (len > hdr->nameLen)
        len = hdr->nameLen;
    memcpy(dst, name, len);
}
```

**Build-test:** `./packages/forth-core/build-test.sh` — dict self-test passes (smudge skip is exercised if any test defines a word while another is smudged; add a smudge visibility subtest if desired).

**Dependencies:** None.

---

### Step 4 — C7: _Static_assert + C3: Re-entrancy guard

**DECISION 4** — Chosen error code: **`ERROR_OPERATION_UNDEFINED` (13)**. Rationale: best semantic match for "nested Forth entry not supported"; does not collide with `ERROR_RAM_FULL` (11) used by rstack-overflow and runaway guards. Distinguishable in tests by error code value.

**Files:** `forth_prims.c`, `forth_inner.c`, `test_dict_reloc.c`, `forth_dict.h`

**forth_prims.c** — add before `forthPrims[]` array:
```c
_Static_assert(sizeof(forthPrims)/sizeof(forthPrims[0]) <= 0x0FFF,
    "forthPrimCount exceeds FTOK_PRIM address space");
```

Wait — `forthPrims` isn't defined yet at that point. Add after the array and `forthPrimCount`:
```c
const uint16_t forthPrimCount = sizeof(forthPrims) / sizeof(forthPrims[0]);
_Static_assert(forthPrimCount <= 0x0FFF,
    "forthPrimCount exceeds FTOK_PRIM address space (0x0FFF)");
```

**forth_inner.c** — add re-entrancy guard:
```c
static bool_t forthRunning = false;  // after rstack/rsp declarations

void forthInner(uint16_t entryIndex, bool fromProgram)
{
    if (forthRunning) {
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        return;
    }
    forthRunning = true;
    
    uint16_t ip = forthDictBodyByIndex(entryIndex);
    if (ip == FORTH_NULL) {
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,
                                ERR_REGISTER_LINE, NIM_REGISTER_LINE);
        forthRunning = false;
        return;
    }
    // ... existing loop ...
    
    // Every exit path must clear forthRunning:
    // - FTOK_EXIT/rsp==0: forthRunning = false; return;
    // - All error returns: forthRunning = false; return;
    // - Runaway guard: forthRunning = false; return;
    // - Cooperative break: forthRunning = false; return;
}
```

Add `forthRunning = false;` before every `return` in the main loop (error returns, runaway guard return, cooperative break return) and before the normal `rsp==0` return.

**test_dict_reloc.c** — add `forthRentrancySelfTest`:
```c
int forthRentrancySelfTest(void)
{
    // Test: simulate nested entry by priming forthRunning=true (via test-only
    // accessor or by calling forthInner from within a word — see A2).
    // For pre-bridge testing: use a test-only mechanism.
    // For acceptance testing (step 15): use FTOK_C47 → ITM_FCALL path.
}
```

**forth_dict.h** — add declaration:
```c
int forthRentrancySelfTest(void);
```

**Build-test:** `./packages/forth-core/build-test.sh` — _Static_assert passes (8 ≤ 0x0FFF), re-entrancy test passes.

**Dependencies:** None for C7 (forth_prims.c standalone). C3 depends on nothing external.

---

### Step 5 — C1: FTOK_C47 PGM_RUNNING fix

**Files:** `forth_inner.c`

**forth_inner.c FTOK_C47 arm** — wrap `reallyRunFunction` with program semantics:
```c
if (tok == FTOK_C47) {
    // ... existing itemId/ptp/param decoding ...
    uint8_t saved = programRunStop;
    programRunStop = PGM_RUNNING;
    reallyRunFunction(itemId, param);
    if (programRunStop == PGM_RUNNING)
        programRunStop = saved;
    clearSystemFlag(FLAG_ASLIFT);
    if (lastErrorCode != ERROR_NONE) {
        forthRunning = false;
        return;
    }
    continue;
}
```

**Build-test:** `./packages/forth-core/build-test.sh` — C47 self-test passes (PGM_RUNNING semantics don't change PC build behavior where `programRunStop` isn't `PGM_RUNNING`).

**Dependencies:** Step 4 (C3 re-entrancy guard must be present — PGM_RUNNING fix makes re-entry path more reachable).

---

### Step 6 — DECISION 3: Key-poll (pollProgramInterrupt)

**Files:** `forth_inner.c`

**forth_inner.c** — add `pollProgramInterrupt` function and integrate into loop:

```c
#if defined(DMCP_BUILD)
#include "addons.h"  // C47PopKeyNoBuffer, setLastKeyCode

static bool_t pollProgramInterrupt(void)
{
    int key = C47PopKeyNoBuffer(DISPLAY_WAIT_FOR_RELEASE) + 1;
    if (key == 36 || key == 33) {  // R/S or EXIT
        programRunStop = PGM_WAITING;
        return true;
    } else if (key > 0) {
        setLastKeyCode(key);
    }
    return false;
}
#endif
```

In the main loop, **before** the runaway guard check (top of loop, after cooperative break check):
```c
for (;;) {
    /* Cooperative break — async stop from dispatched item */
    if (fromProgram && programRunStop != PGM_RUNNING) {
        forthRunning = false;
        return;
    }

    /* Key poll — primary interrupt on DMCP hardware */
#if defined(DMCP_BUILD)
    if (pollProgramInterrupt()) {
        forthRunning = false;
        return;
    }
#endif

    if (++dispatchCount >= FORTH_RUNAWAY_CAP) {
        // ...
    }
    // ...
}
```

**Build-test:** `./packages/forth-core/build-test.sh` — PC build unaffected (DMCP_BUILD not defined).

**Dependencies:** Step 4 (re-entrancy guard for `forthRunning = false` on exit).

---

### Step 7 — C2: FTOK_C47/PTP_NUMBER_8 padding to 2-byte cell

**Files:** `forth_inner.c`, `test_dict_reloc.c`

**forth_inner.c** — change `ip += 1` to `ip += 2` in PTP_NUMBER_8 arm:
```c
} else if (ptp == PARAM_NUMBER_8) {
    param = (uint16_t)fdict.base[ip];
    ip += 2;  /* cell-padded: 1 byte param + 1 byte zero pad */
}
```

**test_dict_reloc.c** — C47 test 2 body update:
- Allocate 14 bytes instead of 13
- Insert zero padding byte after param:
```c
/* FTOK_C47 item 2620 param=2 */
tok = FTOK_C47;
memcpy(bp, &tok, 2); bp += 2;
memcpy(bp, &itmDupn, 2); bp += 2;
uint8_t param = 2;
memcpy(bp, &param, 1); bp += 1;
uint8_t pad = 0;
memcpy(bp, &pad, 1); bp += 1;

/* EXIT at byte 12 */
tok = FTOK_EXIT;
memcpy(bp, &tok, 2); bp += 2;
```
- Update body hex dump loop from `i < 13` to `i < 14`
- Update comment from "Total: 13 bytes" to "Total: 14 bytes"
- Update byte offset comments accordingly

**Build-test:** `./packages/forth-core/build-test.sh` — C47 test 2 passes with padded body.

**Dependencies:** Step 2 (test already uses `readXAsInt32()`), Step 5 (PGM_RUNNING fix ensures clean dispatch).

---

### Step 8 — C4: ASLIFT on exit

**Files:** `forth_inner.c`, `test_dict_reloc.c`

**forth_inner.c** — add `setSystemFlag(FLAG_ASLIFT)` before `rsp==0` return:
```c
if (tok == FTOK_EXIT) {
    if (rsp == 0) {
        setSystemFlag(FLAG_ASLIFT);
        forthRunning = false;
        return;
    }
    ip = rstack[--rsp];
    continue;
}
```

**test_dict_reloc.c** — stack test c (~319-326), flip assertion:
```c
// Before:
if (asliftAfter) {
    printf("FAIL test c: FLAG_ASLIFT is SET after forthInner returns (should be clear)\n");
    fail = 1;
} else {
    printf("PASS test c: FLAG_ASLIFT is clear after word return\n");
}

// After:
if (!asliftAfter) {
    printf("FAIL test c: FLAG_ASLIFT is CLEAR after forthInner returns (should be SET per C4)\n");
    fail = 1;
} else {
    printf("PASS test c: FLAG_ASLIFT is SET after word return (ASLIFT on exit)\n");
}
```

**Build-test:** `./packages/forth-core/build-test.sh` — stack test c passes with flipped assertion.

**Dependencies:** Step 4 (re-entrancy guard's `forthRunning = false` on exit path).

---

### Step 9 — H1b: items.h override (ITM_FORTH/ITM_FCALL defines)

**Files:** `items.h` (override)

Add after `#define ITM_2843 2843`:
```c
#define ITM_FORTH 2842
#define ITM_FCALL 2843
```

Keep `ITM_2842`/`ITM_2843` numeric defines. Do NOT touch `ITM_FWORD 2003`.

**Build-test:** `./packages/forth-core/build-test.sh` — compiles; no behavioral change yet (defines only).

**Dependencies:** None.

---

### Step 10 — H1: items.c override (item rows 2842/2843)

**Files:** `items.c` (override)

Replace rows 4689-4690 (0-indexed 2842/2843):
```c
/* 2842 */ { fnForthOuter, NOPARAM,  "FORTH", "FORTH",
             (0 << TAM_MAX_BITS) |     0,
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_NONE      | HG_ENABLED },
/* 2843 */ { fnForthCall,  TM_VALUE, "FCALL", "FCALL",
             (0 << TAM_MAX_BITS) | 16383,
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_NUMBER_16 | HG_ENABLED | RESULT_IN_X },
```

Add forward declarations at top of file (or include `forth_dict.h`):
```c
extern void fnForthCall(uint16_t param);
extern void fnForthOuter(uint16_t param);
```

**Build-test:** `./packages/forth-core/build-test.sh` — links against `forth_bridge.c` (step 11 provides the symbols, but they're declared `extern` here).

**Dependencies:** Step 9 (ITM_FORTH/ITM_FCALL defines). Step 11 (bridge functions must exist for link).

---

### Step 11 — Bridge functions (fnForthCall, fnForthOuter)

**Files:** `forth_bridge.c`, `forth_dict.h`

**forth_bridge.c:**
```c
#include "c47.h"
#include "forth_dict.h"
#include "forth_prims.h"
#include "items.h"

void fnForthCall(uint16_t param)
{
    forthInner(param, programRunStop == PGM_RUNNING);
    // (Pass true only if called from a running program, not interactively.)
}

void fnForthOuter(uint16_t unused)
{
    /* Stage 1: outer interpreter is stage 2.
       Display error to signal "not yet implemented". */
    displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                            ERR_REGISTER_LINE, NIM_REGISTER_LINE);
}
```

**forth_dict.h** — add declarations:
```c
void fnForthCall(uint16_t param);
void fnForthOuter(uint16_t param);
```

**Build-test:** `./packages/forth-core/build-test.sh` — links cleanly; `XEQ 'FCALL'` with valid index runs a word; `XEQ 'FORTH'` displays error 13.

**Dependencies:** Step 9 (ITM_FORTH/ITM_FCALL), Step 10 (item rows reference these functions), Steps 4-8 (safety/correctness fixes in forthInner).

---

### Step 12 — H2: lblGtoXeq.c PARAM_LABEL arm Forth fallback

**Files:** `programming/lblGtoXeq.c` (override)

**lblGtoXeq.c:345-357** — after `findNamedLabel` returns `INVALID_VARIABLE`, before `ERROR_LABEL_NOT_FOUND`:
```c
case PARAM_LABEL: {
    if(opParam <= LAST_LOCAL_LABEL) {
        reallyRunFunction(op, opParam);
    }
    else if(opParam == STRING_LABEL_VARIABLE) {
        getStringLabelOrVariableName(paramAddress);
        calcRegister_t label = findNamedLabel(tmpStringLabelOrVariableName);
        if(label != INVALID_VARIABLE || op == ITM_LBLQ) {
            reallyRunFunction(op, label);
        }
        else {
            /* H2: Forth colon-def fallback (C47-label-first, Forth-second) */
            uint16_t widx;
            if (forthFindColon(tmpStringLabelOrVariableName, &widx)) {
                reallyRunFunction(ITM_FCALL, widx);
            }
            else {
                displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
                // ... existing EXTRA_INFO block ...
            }
        }
    }
    // ... rest unchanged ...
}
```

Add includes at top:
```c
#include "forth_dict.h"
#include "items.h"  /* ITM_FCALL */
```

**Build-test:** `./packages/forth-core/build-test.sh` — compiles; existing programs unaffected (C47 label still tried first).

**Dependencies:** Step 11 (ITM_FCALL defined, forthInner working).

---

### Step 13 — H3: items.c XEQ-by-menu Forth fallback

**Files:** `items.c` (override)

**items.c:664-685** — after `findNamedLabel` returns `INVALID_VARIABLE`, before `ERROR_LABEL_NOT_FOUND`:
```c
if(func == ITM_XEQ && dynamicMenuItem > -1) {
    char *varCatalogItem = dynmenuGetLabel(dynamicMenuItem);
    if(strcmp(varCatalogItem, "XEQ") != 0) {
        calcRegister_t label = findNamedLabel(varCatalogItem);
        if(label != INVALID_VARIABLE) {
            // ... existing PEM/runFunction ...
        }
        else {
            /* H3: Forth colon-def fallback */
            uint16_t widx;
            if (forthFindColon(varCatalogItem, &widx)) {
                reallyRunFunction(ITM_FCALL, widx);
            }
            else {
                displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
                // ... existing EXTRA_INFO block ...
            }
        }
    }
    return;
}
```

Add includes:
```c
#include "forth_dict.h"
```

**Build-test:** `./packages/forth-core/build-test.sh` — compiles; interactive XEQ of Forth word works.

**Dependencies:** Step 12 (same pattern, already verified).

---

### Step 14 — Acceptance tests (§7.1, §7.5)

**Files:** `test_dict_reloc.c`, `forth_dict.h`, `config.c`

**test_dict_reloc.c** — add acceptance test functions:

**14a. XEQ end-to-end test:** Define a word, call via `fnForthCall` (simulating XEQ path):
```c
int forthXEQSelfTest(void)
{
    // Define a word: ILIT(42) EXIT
    // Call fnForthCall(0) — simulates ITM_FCALL dispatch
    // Assert X = 42, no crash
    // Verify indexOfItems size: LAST_ITEM == 2860
}
```

**14b. Re-entrancy test (FTOK_C47 → Forth item):**
```c
// Define word A: FTOK_C47(ITM_FCALL, index_of_word_B) EXIT
// Define word B: ILIT(99) EXIT
// Run word A — FTOK_C47 dispatches ITM_FCALL → forthInner for B
// Re-entrancy guard fires: ERROR_OPERATION_UNDEFINED, B does NOT execute
// Assert: error = ERROR_OPERATION_UNDEFINED (13), X != 99
```

**14c. §7.5 regression test (C47 program shadows Forth word):**
```c
// This tests the H2/H3 lookup order: C47 label first, Forth second.
// Verify that findNamedLabel takes precedence over forthFindColon.
// (Mostly verified by code inspection of steps 12-13, but add a
//  unit test that calls the lookup sequence directly.)
```

**forth_dict.h:**
```c
int forthXEQSelfTest(void);
int forthRentrancySelfTest(void);
```

**config.c** — add declarations and runner invocations:
```c
extern int forthXEQSelfTest(void);
extern int forthRentrancySelfTest(void);
// In runner block:
if(forthXEQSelfTest()) { /* ... */ }
if(forthRentrancySelfTest()) { /* ... */ }
```

**Build-test:** `./packages/forth-core/build-test.sh` — all acceptance tests pass.

**Dependencies:** Steps 9-13 (all hooks and bridge wired).

---

### Step 15 — Hygiene: config.c override inventory

**Files:** `config.c` (override — already in pkg_override_sources)

Verify `config.c` override contains the complete self-test declaration/runner block (already present from prior work). Add any missing test declarations (`forthBadEntrySelfTest` at line 1959 — confirm it's invoked in the runner).

Check: `config.c` runner block at ~1960 invokes all declared tests. If `forthBadEntrySelfTest` is declared but not invoked, add the invocation.

**Build-test:** `./packages/forth-core/build-test.sh` — all tests run, no new warnings.

**Dependencies:** Step 14 (all tests declared and implemented).

---

### Summary Table

| Step | Category | File(s) | Decision/Checklist | Build-Test |
|------|----------|---------|--------------------|------------|
| 1 | Correctness | forth_dict.c, test_dict_reloc.c | DECISION 2 | ✓ |
| 2 | Correctness | forth_inner.c, test_dict_reloc.c | DECISION 1 (T1-2) | ✓ |
| 3 | Correctness | forth_dict.c | C5 + C6 | ✓ |
| 4 | Correctness + Safety | forth_prims.c, forth_inner.c, test_dict_reloc.c, forth_dict.h | C7 + C3 (DECISION 4) | ✓ |
| 5 | Safety | forth_inner.c | C1 | ✓ |
| 6 | Safety | forth_inner.c | DECISION 3 (key-poll) | ✓ |
| 7 | Correctness | forth_inner.c, test_dict_reloc.c | C2 | ✓ |
| 8 | Correctness | forth_inner.c, test_dict_reloc.c | C4 | ✓ |
| 9 | Hook | items.h | H1b | ✓ |
| 10 | Hook | items.c | H1 | ✓ |
| 11 | Hook | forth_bridge.c, forth_dict.h | bridge fn | ✓ |
| 12 | Reverse lookup | programming/lblGtoXeq.c | H2 | ✓ |
| 13 | Reverse lookup | items.c | H3 | ✓ |
| 14 | Acceptance | test_dict_reloc.c, forth_dict.h, config.c | §7.1, §7.5 | ✓ |
| 15 | Hygiene | config.c | override inventory | ✓ |

### Parallelization Opportunities

Steps 1-8 are independent of each other (except step 4's C3 which steps 5-8 reference for `forthRunning = false`). They could theoretically be done in any order. Steps 9-11 must precede 12-13. Step 14 depends on everything.

The critical path is: {1-8} → {9,10,11} → {12,13} → 14 → 15.

---