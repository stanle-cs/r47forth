# Stage 2 — Three Pillars Implementation Master Plan

**Status:** Phase 2 output. Produced after human approval of
`STAGE2_THREE_PILLARS_AUDIT.md` with rulings D-1 (harness bug fixed in this
branch, gate green), D-2 (a: no early tail execution, b: no recompilation,
c: owning-program scope only), D-3 (both interpreters fully re-entrant, no
idle-BSS burn, respect C-stack limits).

**Audience:** the implementing model (Qwen). Every decision is stated; do not
improvise. Where a value is a `#define`, use exactly the value given.

**Nothing here is implemented yet.** This branch contains only the D-1 harness
fix (`test_dict_reloc.c`, `build-test.sh`) and the planning documents.

**Execution vehicle:** `STAGE2_QWEN_PROMPTS.md` (same directory) breaks this
plan into 15 small, ordered, self-contained prompts sized for the
implementer's context window. Where the prompts refine this plan, the prompts
win. Two refinements so far: (1) `forthOuterMode_t`, `forthOuterCtx_t`, and
`forthOuterRun` stay file-static in `forth_compile.c` — every consumer,
including the P2 pre-scan, lives in that file, so the `forth_dict.h` enum
placement mentioned below is superseded; (2) DESIGN.md updates (deliverable
4) are done by the architect after implementation, not by the implementer.

---

## 0. Ground rules for the implementer

1. **Gate.** The only build/test command is
   `./packages/forth-core/build-test.sh`. Run it after every task below. A
   green run ends with `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.` (exit 0). This was verified green at plan
   time. Never invoke meson/ninja by hand; the script injects
   `-DFORTH_DEBUG_SELFTEST` via `meson configure`
   [VERIFIED: packages/forth-core/build-test.sh:54-61].
2. **Package system.** Never edit files under `src/` directly. Package working
   files live flat under `packages/forth-core/`, mirroring the `src/c47/`
   subtree (`packages/forth-core/config.c` overrides `src/c47/config.c`;
   `packages/forth-core/programming/lblGtoXeq.c` overrides
   `src/c47/programming/lblGtoXeq.c`). After **every** edit to a working file,
   regenerate the resolver inputs:
   `python3 tools/pkg_patch_refresh.py packages/forth-core`.
   The refresh tool auto-classifies (modified-upstream → `patches/`, new file
   → `files/`).
3. **Forbidden files.** Do not touch `src/c47/core/freeList.c` or its package
   copy — a known double-free-guard bug is being handled elsewhere and is out
   of scope. Do not modify `DESIGN.md` DECIDED sections.
4. **Arena duty (CLAUDE.md).** Every task that changes dictionary behavior
   must extend or preserve the suite's arena report (currently
   `FORTH ARENA: dict here=.. sizeBlocks=..` printed at suite end) and the
   final report to the human must quote the high-water values.
5. **Implementation order is P1 → P3 → P2.** P1 is independent. P3 replaces
   the static source buffer with stack contexts; P2's pre-scan is specified
   on top of the P3 context model (a pre-scan interleaved with a static
   `forthSource[]` would clobber the in-flight step). Do not reorder.
6. **Commit series** (per project convention, one clean series on this
   branch): one commit per pillar (`forth-core: P1 save/restore`,
   `forth-core: P3 re-entrancy`, `forth-core: P2 run-start pre-scan`), each
   including its tests, each gate-green.
7. **Mutation discipline.** Every test below states the specific mistake it
   must catch. When you finish a test, verify it by temporarily introducing
   that mistake and confirming the suite goes red, then reverting.

---

## Pillar 1 — Save/Restore Integration (Hook H5)

### 1.1 Scope and mechanism

The simulator backup (`saveCalc`/`restoreCalc`, PC_BUILD only) already
serializes the whole managed arena as a hex dump
[VERIFIED: src/c47/saveRestoreBackup.c:562 (save), :819 (restore)] and rebases
arena-resident pointers with the `TO_C47MEMPTR`/`TO_PCMEMPTR` idiom, e.g.
`labelList`/`programList` [VERIFIED: src/c47/saveRestoreBackup.c:523-527
(save), :843-847 (restore)]. The dictionary **contents** therefore already
travel inside the `ram` dump for free. What is missing is the `fdict` control
block (package BSS, not in the arena
[VERIFIED: packages/forth-core/forth_dict.c:14-29]): `base` must be saved as a
c47Ptr and rebased on restore; the four scalars must be saved verbatim.

Binding is done by creating a **package working copy** of
`saveRestoreBackup.c`:

- Copy `src/c47/saveRestoreBackup.c` → `packages/forth-core/saveRestoreBackup.c`.
- Apply the two hunks below.
- Run the refresh tool (it will classify this as a patch).

No parallel serialization path, no format/version bump: parameters are
name-keyed and order-independent
[VERIFIED: src/c47/saveRestoreBackup.c:817-818], and a missing parameter
leaves the caller's buffer untouched and prints "using default"
[VERIFIED: src/c47/saveRestoreBackup.c:585-588] — so old backup files load as
"empty dictionary" provided we pre-seed defaults (Hunk R does).

### 1.2 Hunk S — save side

Location: inside `saveCalc`, immediately after the `programList` save pair
[VERIFIED: src/c47/saveRestoreBackup.c:526-527], i.e. before the
`currentSubroutineLevelData` block at :529. Insert:

```c
    ramPtr = TO_C47MEMPTR(fdict.base);
    saveStateValue(&ramPtr,            sizeof(ramPtr),            "forthDictBase",       "c47Ptr");
    saveStateValue(&fdict.sizeBlocks,  sizeof(fdict.sizeBlocks),  "forthDictSizeBlocks", "uint16");
    saveStateValue(&fdict.here,        sizeof(fdict.here),        "forthDictHere",       "uint16");
    saveStateValue(&fdict.latest,      sizeof(fdict.latest),      "forthDictLatest",     "uint16");
    saveStateValue(&fdict.count,       sizeof(fdict.count),       "forthDictCount",      "uint16");
```

Also add `#include "forth_dict.h"` near the top of the file (mirror the
config.c override, which includes it the same way
[VERIFIED: packages/forth-core/config.c:5]).

Notes fixed by design, do not deviate:
- `"uint16"` is a supported value type
  [VERIFIED: src/c47/saveRestoreBackup.c:98 (restore dispatch), :302 (existing
  use for `tam.mode`)].
- No `...Offset` companion parameter is needed. `beginOfProgramMemory` saves
  an extra intra-block offset [VERIFIED: src/c47/saveRestoreBackup.c:541-542]
  because that pointer can sit mid-block; `fdict.base` is always the raw
  result of `allocC47Blocks`/`reallocC47Blocks`
  [VERIFIED: packages/forth-core/forth_dict.c:76, :92-96], which is
  block-aligned, so the c47Ptr round-trip is exact.
- `TO_C47MEMPTR(NULL)` yields `C47_NULL` and `TO_PCMEMPTR(C47_NULL)` yields
  `NULL` (defines.h idiom used by every c47Ptr line in this file), so a
  never-initialized dictionary saves and restores as `base == NULL` with no
  special-casing.

### 1.3 Hunk R — restore side

Location: inside `restoreCalc`, immediately after the `programList` rebase
pair [VERIFIED: src/c47/saveRestoreBackup.c:846-847], i.e. before the
`currentSubroutineLevelData` restore at :849. Insert:

```c
    ramPtr = C47_NULL;   /* default: old backup w/o Forth params -> empty dict */
    restoreStateValue(&ramPtr,         sizeof(ramPtr),         "forthDictBase",       "c47Ptr");
    fdict.base = TO_PCMEMPTR(ramPtr);
    {
      uint16_t v;
      v = 0;          restoreStateValue(&v, sizeof(v), "forthDictSizeBlocks", "uint16"); fdict.sizeBlocks = v;
      v = 0;          restoreStateValue(&v, sizeof(v), "forthDictHere",       "uint16"); fdict.here       = v;
      v = FORTH_NULL; restoreStateValue(&v, sizeof(v), "forthDictLatest",     "uint16"); fdict.latest     = v;
      v = 0;          restoreStateValue(&v, sizeof(v), "forthDictCount",      "uint16"); fdict.count      = v;
    }
    forthDictValidateRestored();
```

Ordering guarantees, already satisfied by the anchor position — verify, do not
rearrange:
- `doFnReset(CONFIRMED, loadAutoSav)` runs first
  [VERIFIED: src/c47/saveRestoreBackup.c:755] and calls `forthDictInit()`
  via the package reset hook [VERIFIED: packages/forth-core/config.c:1941] —
  correct there because the arena has just been wiped (this is the sanctioned
  exception to the P-4 init-vs-clear rule, DESIGN.md §6.2).
- The `ram` dump and the free/allocated-region bookkeeping are restored
  earlier [VERIFIED: src/c47/saveRestoreBackup.c:819-823], so the rebase in
  Hunk R lands on the restored arena, exactly like the `labelList` idiom.
- Each `restoreStateValue` call must have its default written into the buffer
  **immediately before** the call, because a missing parameter returns
  without writing [VERIFIED: src/c47/saveRestoreBackup.c:585-588] and the
  local `ramPtr` still holds the previous parameter's value.

### 1.4 New function `forthDictValidateRestored`

File: `packages/forth-core/forth_dict.c`, placed after `forthDictClear`
[VERIFIED: packages/forth-core/forth_dict.c:48-58]. Prototype in
`forth_dict.h` next to the lifecycle prototypes
[VERIFIED: packages/forth-core/forth_dict.h:50-51].

Purpose: a torn, hand-edited, or version-skewed backup must never leave
`fdict` in a state where the next `forthDictEmit`/lookup reads or writes out
of bounds. Exact behavior (pseudocode is normative):

```c
void forthDictValidateRestored(void) {
  if (fdict.base == NULL) {
    /* normalize scalars regardless of what the file said */
    fdict.sizeBlocks = 0; fdict.here = 0; fdict.latest = FORTH_NULL; fdict.count = 0;
    return;
  }
  uint32_t cap = (uint32_t)fdict.sizeBlocks * BYTES_PER_BLOCK;
  bool ok = (fdict.sizeBlocks != 0) && (fdict.here <= cap)
         && (fdict.latest == FORTH_NULL || fdict.latest < fdict.here);
  if (ok) {                       /* walk the header chain */
    uint16_t off = fdict.latest, n = 0;
    while (off != FORTH_NULL) {
      if ((uint32_t)off + 4 > fdict.here) { ok = false; break; }
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      if (hdr->nameLen == 0 || hdr->nameLen > FORTH_NAME_MAX) { ok = false; break; }
      if (hdr->link != FORTH_NULL && hdr->link >= off) { ok = false; break; } /* must strictly decrease */
      off = hdr->link;
      if (++n > fdict.count) { ok = false; break; }
    }
    if (ok && n != fdict.count) ok = false;
  }
  if (!ok) {
    printf("forthDictValidateRestored: inconsistent dictionary in backup, resetting\n"); /* PC-only path */
    forthDictInit();   /* deliberate orphan: region (if any) stays in the restored
                          bookkeeping. Do NOT freeC47Blocks here — the restored
                          allocation tables are exactly what we just failed to
                          trust. Documented exception to P-4 (DESIGN.md §6.2). */
  }
}
```

The chain-walk invariants mirror what `forthFindColon` and
`bodyOffsetOfIndex` assume when they dereference
[VERIFIED: packages/forth-core/forth_dict.c:159-185;
packages/forth-core/forth_inner.c:118-140].

### 1.5 Self-test run-once guard in `config.c`

The self-test runner sits inside `doFnReset`
[VERIFIED: packages/forth-core/config.c:1943-1952]. T1 tests call
`restoreCalc`, and `restoreCalc` calls `doFnReset`
[VERIFIED: src/c47/saveRestoreBackup.c:755] → without a guard the suite
re-enters itself. Wrap the existing block:

```c
      #if defined(PC_BUILD) && defined(FORTH_DEBUG_SELFTEST)
       {
         static bool forthSelfTestRan = false;
         if(!forthSelfTestRan) {
           forthSelfTestRan = true;
           extern int forthDictSelfTest(void);
           ... existing body :1944-1951 unchanged ...
         }
       }
      #endif
```

### 1.6 Pillar 1 tests (in `test_dict_reloc.c`)

Test-side protocol, mandatory for every T1 test that touches the backup file:
`backupFileName` is a compile-time macro
[VERIFIED: src/c47/saveRestoreBackup.c:26], not redirectable. Before the T1
group, read the existing backup file (if any) fully into a heap buffer; after
the group, write it back byte-identical (or delete the file if it did not
exist). Otherwise the suite destroys the developer's simulator state.

Also note: `restoreCalc` replaces the entire arena and its bookkeeping, so the
suite's start/end allocation-count leak gate only balances if each T1 test
ends by returning the arena to the state captured in its own `saveCalc`
snapshot and then `forthDictClear()`s the restored dictionary.

- **T1.1 — round-trip.** Build a dictionary with ≥2 words (one calling the
  other), `saveCalc()`, then `forthDictClear()` and define one *different*
  word, then `restoreCalc()`. Assert: both original words resolve via
  `forthFindColon`, the post-clear word does not, executing the caller word
  via `forthInner` produces the expected X, and `fdict.count`/`here` equal
  the pre-save values. **Must fail if:** any one of the five parameters is
  dropped from Hunk S or Hunk R (e.g. `latest` missing ⇒ lookup finds
  nothing; `here` missing ⇒ count/here assertion), or the rebase line writes
  `ramPtr` into `fdict.base` without `TO_PCMEMPTR` (pointer garbage ⇒ lookup
  crash/miss).
- **T1.2 — old-backup default.** `saveCalc()` with a populated dictionary,
  then strip all five `forthDict*` lines from the backup file (text file:
  filter lines by prefix), `restoreCalc()`. Assert `fdict.base == NULL`,
  `latest == FORTH_NULL`, `count == 0`, and a subsequent `: X 1 ;` via the
  outer interpreter works (lazy alloc intact). **Must fail if:** the
  pre-seeded defaults in Hunk R are removed (stale `ramPtr` from the
  `programList` restore would masquerade as the dict base).
- **T1.3 — validation clamps corruption.** `saveCalc()` with a populated
  dictionary; in the backup file, overwrite the `forthDictHere` value with
  `65534` (exceeds `sizeBlocks*4`); `restoreCalc()`. Assert the dictionary
  came back empty (`base == NULL` per validation reset) and that a fresh
  definition then compiles and runs. Repeat once with `forthDictCount`
  incremented by 1 (chain-walk mismatch). **Must fail if:**
  `forthDictValidateRestored` is not called or any listed invariant check is
  deleted (next `forthDictEmit` would write past the region — under ASAN/host
  this is the observable crash; the count-mismatch variant fails the lookup
  assertions).
- **T1.4 — no recursive self-test.** Implicit in every T1 test: `restoreCalc`
  must return. Add an explicit static counter incremented at suite entry;
  assert it equals 1 at suite end. **Must fail if:** the §1.5 run-once guard
  is removed (suite re-enters via `doFnReset` and either recurses to death or
  the counter reads 2).
- **Arena duty:** after T1.1's restore, print
  `FORTH ARENA (post-restore): here=.. sizeBlocks=..` and keep the existing
  suite-end report intact.

---

## Pillar 3 — Re-entrancy Hardening (implement second)

D-3 ruling: both interpreters become fully re-entrant, with hard depth caps
(no unbounded C-stack growth) and no idle-BSS cost.

### 3.1 Inner interpreter (`forth_inner.c`)

Current state: single-level boolean guard `forthRunning`
[VERIFIED: packages/forth-core/forth_inner.c:28, :160-166], `rsp` zeroed on
every entry [VERIFIED: forth_inner.c:178], `FTOK_EXIT` returns at `rsp == 0`
[VERIFIED: forth_inner.c:209-218], overflow check at `FTOK_CALL`
[VERIFIED: forth_inner.c:241-247]. `rstack[64]`/`rsp` are statics
[VERIFIED: forth_inner.c:26-27] — they stay static (shared across nesting
levels via watermarking; zero extra BSS).

Changes, all in `forth_inner.c`:

1. Delete `static bool forthRunning` (:28). Add:
   ```c
   #define FORTH_NEST_MAX 4
   static uint8_t forthDepth = 0;
   ```
2. Replace the guard block (:160-166) with:
   ```c
   if (forthDepth >= FORTH_NEST_MAX) {
     lastErrorCode = ERROR_OPERATION_UNDEFINED;   /* C-12: error code preserved */
     displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     return;
   }
   forthDepth++;
   ```
3. Replace `rsp = 0;` (:178) with `uint8_t rspBase = rsp;` — the watermark.
   (Note: the entry-error path in step 2 and the `ip == FORTH_NULL` path
   below must be checked against where `forthDepth++` sits: the bad-entry
   check at :169-176 runs *after* the increment, so it must decrement.)
4. `FTOK_EXIT` arm: change `if (rsp == 0)` to `if (rsp == rspBase)`; on that
   branch replace `forthRunning = false;` with `forthDepth--;`.
5. Every other exit path currently doing `forthRunning = false; return;` or
   falling out of the loop — the bad-entry path (:170-176), the DMCP key-poll
   return (:188-191), the runaway cap (:195-201), the bad-prim path
   (:223-229), prim error (:232-235), `FTOK_CALL` bad body (:250-257),
   LIT/ILIT/0BR/C47 error checks (:268-271, :282-285, :308-311, :370-373),
   unsupported-PTP (:349-355), default/corrupt token (:377-382), and the
   cooperative-break fallthrough (:386) — becomes:
   ```c
   rsp = rspBase;
   forthDepth--;
   return;
   ```
   Use one local macro to avoid divergence between the ~13 sites:
   ```c
   #define INNER_LEAVE() do { rsp = rspBase; forthDepth--; return; } while (0)
   ```
   Rationale for `rsp = rspBase` on *every* non-EXIT exit: an error or
   suspension abandons this level's partially-built return chain; the outer
   level must find `rsp` exactly where it left it. Errors still propagate to
   outer levels through `lastErrorCode` (the outer level's own
   post-dispatch checks at :232, :268, :282, :308, :370 fire and unwind it in
   turn), so a mid-nest error unwinds the entire nest to depth 0.
6. `RUNAWAY_CAP` note (no code change): `dispatches` is a per-invocation
   local [VERIFIED: forth_inner.c:157], so total work is bounded by
   `FORTH_NEST_MAX × RUNAWAY_CAP` — acceptable.
7. Test hooks: replace `forthTestSetRunning`/`forthTestIsRunning`
   [VERIFIED: forth_inner.c:390-393; forth_dict.h:146-149] with
   ```c
   void    forthTestSetDepth(uint8_t d) { forthDepth = d; }
   uint8_t forthTestGetDepth(void)      { return forthDepth; }
   ```
   and update the existing guard test in `test_dict_reloc.c` (it primes the
   guard with `forthTestSetRunning(true)`) to prime with
   `forthTestSetDepth(FORTH_NEST_MAX)` and to assert depth returns to its
   primed value/0 as appropriate.

### 3.2 Outer interpreter (`forth_compile.c` + small `forth_dict.c` addition)

Current statics to eliminate from idle BSS: `forthSource[256]` +
`forthOuterActive` [VERIFIED: packages/forth-core/forth_compile.c:22-24] and
the tokenizer pair `tokenizerSource`/`tokenizerPos`
[VERIFIED: forth_compile.c:44-45]. Replacement model: a per-invocation
context on the **caller's C stack**, chained through one static pointer.

1. New types/statics in `forth_compile.c`:
   ```c
   typedef struct {
     char            source[FORTH_SOURCE_MAX];
     int16_t         pos;          /* tokenizer position within source */
     forthDefState_t savedDef;     /* outer level's open-definition snapshot */
   } forthOuterCtx_t;

   #define FORTH_OUTER_NEST_MAX 2
   static forthOuterCtx_t *forthOuterCur   = NULL;
   static uint8_t          forthOuterDepth = 0;
   ```
   Idle BSS after P3: one pointer + two uint8 counters versus the removed
   ~267 B (256 + 1 + 8 + 2) + 1 (`forthRunning`) — net idle-BSS reduction
   ≈ 255 B on ARM. Report the exact map-file delta with the P3 commit.
2. `forthTokenizerInit`/`nextToken` (:47-71) lose their statics: they read
   `forthOuterCur->source` and `forthOuterCur->pos`. `forthTokenizerInit`
   reduces to `forthOuterCur->pos = 0;`.
3. Open-definition snapshot helpers in `forth_dict.c` — the `openDef` static
   [VERIFIED: packages/forth-core/forth_dict.c:189] must be saved/restored
   across a nested interpret so a nested line can never finish or abort the
   outer line's definition. Add to `forth_dict.c` (+`forth_dict.h`):
   ```c
   typedef struct { uint16_t here, latest, count, entryOff; bool open; } forthDefState_t;
   void forthDefStateSave(forthDefState_t *out);    /* copy openDef fields out */
   void forthDefStateRestore(const forthDefState_t *in); /* copy back */
   ```
   (Straight field copies; `openDef` itself stays static and private.)
4. Core entry point (new; the mode parameter is used by P2 — until P2 lands,
   only `FORTH_OUTER_FULL` exists):
   ```c
   void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode) {
     if (forthOuterDepth >= FORTH_OUTER_NEST_MAX) {
       displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ...);   /* C-12 */
       return;
     }
     forthOuterCtx_t *prev = forthOuterCur;
     forthOuterCur = ctx;
     forthOuterDepth++;
     forthDefStateSave(&ctx->savedDef);
     ctx->pos = 0;
     ...existing forthOuterInterpret body :217-365, reading source via ctx...
     forthDefStateRestore(&ctx->savedDef);
     forthOuterDepth--;
     forthOuterCur = prev;
   }
   ```
   The body keeps its current structure (loop :224-351, end-of-line handling
   :353-365); `nextToken(buf)` already needs no per-call source argument.
   Restoring `prev` (not `NULL`) is what makes nesting work; restoring at
   *every* exit path matters — the current body has no early `return`s
   (errors set `lineOK = false` and fall through
   [VERIFIED: forth_compile.c:224-351]), keep it that way.
5. Public wrapper (tests call it; keep name and signature
   [VERIFIED: forth_dict.h:134]):
   ```c
   void forthOuterInterpret(const char *source) {
     forthOuterCtx_t ctx;
     size_t n = strlen(source);
     if (n >= FORTH_SOURCE_MAX) { displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ...); return; }
     memcpy(ctx.source, source, n + 1);
     forthOuterRun(&ctx, FORTH_OUTER_FULL);
   }
   ```
6. `fnForthOuter` rewrite (:368-387): drop the `forthOuterActive` guard
   (:369-372) — the depth guard in `forthOuterRun` subsumes it; keep the type
   and length checks (:373-381); then:
   ```c
   forthOuterCtx_t ctx;
   xcopy(ctx.source, REGISTER_STRING_DATA(REGISTER_X), len + 1);
   fnDrop(NOPARAM);
   forthOuterRun(&ctx, FORTH_OUTER_FULL);
   ```
   The copy **must** precede `fnDrop` exactly as today (:382-383) — dropping
   X invalidates the string data.
7. `forthProgramStep` rewrite (:391-403), P3 interim form (P2 revises it):
   ```c
   void forthProgramStep(const uint8_t *payload) {
     forthRunGenCheckReset();
     forthOuterCtx_t ctx;
     uint8_t len = *payload;
     xcopy(ctx.source, payload + 1, len);
     ctx.source[len] = 0;
     forthOuterRun(&ctx, FORTH_OUTER_FULL);
   }
   ```
   (The old `forthOuterActive` early-out :392-395 is deleted; nesting is now
   legal up to the cap.)

### 3.3 Depth caps and C-stack accounting

- `FORTH_NEST_MAX 4` (inner), `FORTH_OUTER_NEST_MAX 2` (outer). Both are
  strictly more permissive than today (today any nesting errors out), and
  both fail with the same C-12 error code as the current guards, so no error
  semantics change at the cap.
- Worst-case transient C stack ≈ `FORTH_OUTER_NEST_MAX × sizeof(forthOuterCtx_t)`
  (≈ 2 × 270 B) plus the interpreter frames — well under 1 KB of new
  worst-case stack. `[UNVERIFIED — needs confirmation]` the DMCP-build total
  stack budget; before ever *raising* either cap, measure real headroom on
  hardware. At the values above no measurement is required to proceed.
- `rstack[64]` stays a single shared static: nesting partitions it via
  watermarks instead of duplicating it — zero BSS growth, and the existing
  `FTOK_CALL` overflow check (:241-247) naturally accounts for the sum of
  all levels.

### 3.4 Pillar 3 tests

- **T3.1 — two-level colon nesting works.** Define `: INNER 7 ;`. Create a
  C47 program with a global label whose body is a Forth step `INNER`. Define
  `: OUTER <XEQ that label via compiled FTOK_C47> 35 ;` (use the existing
  test-suite pattern for compiling an XEQ-label token). Execute `OUTER` via
  `forthInner`. Assert X/Y contain 35 and 7 and no error. **Must fail if:**
  the single-level guard survives anywhere (nested entry raises C-12), i.e.
  catches "guard not actually upgraded."
- **T3.2 — watermark protects the outer return chain.** Define
  `: LEAF 1 ; : MID LEAF <nested-XEQ-to-Forth-step> LEAF ; : TOP MID 9 ;` so
  the nested entry fires while `rsp > 0`. Assert the full expected stack
  result (TOP completes: 1, nested result, 1, 9 in order) and
  `forthTestGetDepth() == 0` after. **Must fail if:** entry still executes
  `rsp = 0` (outer chain destroyed ⇒ TOP's tail after MID never runs /
  garbage ip pop) — this is the exact D-3 core bug.
- **T3.3 — depth cap trips and recovers.** Prime
  `forthTestSetDepth(FORTH_NEST_MAX)`, call `forthInner` on a valid word:
  assert C-12 error and that the word did not execute. Reset error, set depth
  0, run the same word: assert success. **Must fail if:** the cap check is
  removed (unbounded recursion becomes possible) or the entry-error path
  decrements a counter it never incremented (recovery run would fail).
- **T3.4 — error unwinds rsp to the watermark.** Build `: BAD <token that
  errors, e.g. corrupt call target> ; : WRAP BAD ;` nested under a caller
  with `rsp > 0` as in T3.2. Trigger, assert error surfaced, then clear the
  error and run a simple word: assert it succeeds and produces exactly one
  stack push. **Must fail if:** any error path forgets `rsp = rspBase`
  (stale rstack entries make the follow-up word's `FTOK_EXIT` pop a garbage
  ip — corrupt behavior or corrupt-data error).
- **T3.5 — outer nesting preserves the tokenizer.** Interpret the line
  `"XEQLBL 5"` where `XEQLBL` is a C47 label whose program contains a Forth
  step `"3"` (nested outer). Assert final stack: 3 then 5 — i.e. the outer
  line's *remaining* token `5` is still consumed after the nested line.
  **Must fail if:** tokenizer state is still static (nested init resets
  `pos` ⇒ outer line re-reads or loses its tail), the exact clobber D-3
  targets.
- **T3.6 — outer depth cap.** Arrange three-deep outer nesting (line → label
  program Forth step → label program Forth step). Assert C-12 error at the
  third level, outer two levels complete their remaining tokens, and a
  fresh interpret afterwards works. **Must fail if:** `forthOuterDepth` is
  not restored on the error path (all subsequent FORTH lines would be
  locked out) or the cap is missing entirely.
- **T3.7 — no dangling context.** After a nesting episode completes, assert
  via a new test hook `forthTestOuterCur()` (FORTH_DEBUG_SELFTEST-gated,
  returns the `forthOuterCur` pointer) that the pointer is `NULL` at rest.
  **Must fail if:** an exit path restores depth but not `forthOuterCur`
  (use-after-return into a dead stack frame on the next tokenizer call).
- **Arena/BSS duty:** report the map-file BSS delta for `forth_compile.o` /
  `forth_inner.o` with the P3 commit, plus the unchanged suite-end arena
  report.

---

## Pillar 2 — Architecture 2: Run-Start Pre-Scan (implement last)

D-2 rulings baked in: **(a)** pre-scan must not execute any non-definition
(tail) code early; **(b)** definition steps must not be recompiled when
execution reaches them; **(c)** pre-scan scope is exactly the owning program
of the executing step.

### 2.1 Design summary

The run-generation seam already exists: `forthRunGenBump()` fires at
interactive XEQ start [VERIFIED: packages/forth-core/programming/lblGtoXeq.c:162]
and menu-key run start [VERIFIED: packages/forth-core/programming/lblGtoXeq.c:904];
`forthRunGenCheckReset()` clears the dictionary on the first Forth step of a
new generation [VERIFIED: packages/forth-core/forth_compile.c:35-40]. The
program-step entry is `forthProgramStep`, dispatched from the package-owned
`lblGtoXeq.c` ITM_FORTH arm
[VERIFIED: packages/forth-core/programming/lblGtoXeq.c:860-867].

Architecture 2 adds, entirely inside package files:

1. On each `forthProgramStep`, after the generation check, a **first-touch
   pre-scan** of the owning program: walk its steps in order; every Forth
   source step (payload len > 0) is interpreted in `DEFS_ONLY` mode, which
   compiles `: ... ;` regions and *skips* (does not execute) every
   interpret-state token (D-2a).
2. The current step's payload is then interpreted in `SKIP_DEFS` mode, which
   executes interpret-state code normally but *skips over* `: ... ;` regions
   without touching the dictionary (D-2b — they were compiled during the
   pre-scan).
3. A scanned-programs list, reset on generation change, makes the pre-scan
   run once per program per generation (D-2b), scoped per program (D-2c);
   programs first touched later in the run (e.g. via XEQ into another
   program) get their own first-touch scan.

Forward-reference parity achieved: any interpret-state (tail) reference to a
word defined in **any step of the same program, earlier or later**, now
resolves, because all of the program's definitions exist before any of its
steps execute. Documented non-goal: a definition *body* referencing a word
defined only in a later step still errors at pre-scan (standard Forth
define-before-use inside definitions; unchanged semantics class).

No tokenizer is duplicated: pre-scan runs the one existing outer interpreter
in a restricted mode. `scanLabelsAndPrograms` is not touched (Phase 1
verified it has no hook point; this design needs none — it consumes the
already-built `programList`).

### 2.2 State and API changes

In `forth_dict.h` (near the `forthOuterInterpret` declaration :134):
```c
typedef enum {
  FORTH_OUTER_FULL      = 0,   /* interactive: compile and execute (today's behavior) */
  FORTH_OUTER_DEFS_ONLY = 1,   /* pre-scan: compile definitions, skip all interpret-state tokens */
  FORTH_OUTER_SKIP_DEFS = 2    /* step execution: skip ':'..';' regions, execute the rest */
} forthOuterMode_t;
```
(P3 already introduced the enum name in `forthOuterRun`'s signature; P2 adds
the two new members and their behavior.)

In `forth_compile.c`:
```c
#define FORTH_SCAN_MAX 8
static const uint8_t *forthScannedProgs[FORTH_SCAN_MAX];
static uint8_t        forthScannedCount = 0;
```
BSS cost: 8 pointers + 1 byte (32-33 B on ARM) — report with the commit.
Extend `forthRunGenCheckReset` (:35-40) so the same generation change that
clears the dictionary also does `forthScannedCount = 0;`.

In `forth_bridge.c`: factor the owning-program walk (currently duplicated at
[VERIFIED: packages/forth-core/forth_bridge.c:37-45] and
[VERIFIED: packages/forth-core/forth_bridge.c:93-99]) into
```c
uint8_t *forthOwningProgramStart(const uint8_t *ptr) {
  uint8_t *progStart = NULL;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    if (programList[i].instructionPointer <= ptr) progStart = programList[i].instructionPointer;
  }
  return progStart;
}
```
plus a sibling that P2 needs for the walk bound:
```c
uint8_t *forthNextProgramStart(const uint8_t *progStart) {
  /* smallest programList[i].instructionPointer strictly greater than progStart, or NULL */
}
```
Declare both in `forth_dict.h` next to the §9.4 helpers (:136-140), and
rewrite `forthMarkerTurnsOn` and `forthEntryStateAtInsertion` to call
`forthOwningProgramStart` (behavior-identical refactor).

### 2.3 Mode semantics in `forthOuterRun`

Anchors are against the current loop body
[VERIFIED: packages/forth-core/forth_compile.c:224-351]; after P3 the same
structure lives in `forthOuterRun`. Exactly three localized insertions — do
not scatter per-arm conditionals:

1. **`':'` arm (:226-243).** In `SKIP_DEFS` mode, replace the arm's body
   with: consume the name token (`nextToken(name)`; if it fails → C-4 error
   `ERROR_INVALID_NAME`, `lineOK = false`); then loop `nextToken(buf)` until
   a token equal to `";"` is consumed; if end-of-line arrives without `';'`
   → `ERROR_INVALID_NAME`, `lineOK = false` (defensive; unreachable when the
   pre-scan already validated the step, because a pre-scan error halts the
   run before execution). Dictionary untouched (D-2b). `FULL`/`DEFS_ONLY`
   keep the existing body verbatim — `DEFS_ONLY` *does* compile.
2. **`';'` arm, interpret-state branch (:247-249).** `DEFS_ONLY`: skip
   silently (`continue`) instead of erroring — the error belongs to
   execution time, and `SKIP_DEFS`/`FULL` keep today's error there.
3. **Interpret-state skip gate.** Immediately before the primitive lookup
   (:260-261), insert:
   ```c
   if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
     continue;   /* D-2a: pre-scan must not execute tail code */
   }
   ```
   This one line suppresses, in pre-scan only: primitive execution (:270),
   colon-word execution (:291), number pushes (:302-315), label XEQ
   (:326-329), and the undefined-word error (:339-350) — undefined
   interpret-state words are *not* pre-scan errors (they may be
   runtime-resolvable labels or genuinely erroneous; either way the error
   surfaces at execution, same step, same message as today).
   Compile-state behavior (token emission :264-268, :285-289, number
   compilation, immediate-primitive execution at :270 when
   `FF_IMMEDIATE`) is identical in `FULL` and `DEFS_ONLY` — immediates must
   run during pre-scan because they build control-flow structures inside the
   definition being compiled.

End-of-line handling (:353-365) is unchanged in all modes (an unterminated
definition aborts + errors in `DEFS_ONLY` too — that's the pre-scan catching
a malformed step).

### 2.4 Pre-scan function and final `forthProgramStep`

In `forth_compile.c`:

```c
static void forthPreScanOwningProgram(const uint8_t *anyPtrInProgram) {
  uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
  if (!progStart) return;                          /* no program list: nothing to scan */
  for (uint8_t i = 0; i < forthScannedCount; i++) {
    if (forthScannedProgs[i] == progStart) return; /* first-touch already done */
  }
  uint8_t *nextStart = forthNextProgramStart(progStart);
  forthOuterCtx_t ctx;                             /* one ctx reused for all steps */
  uint8_t *step = progStart;
  while (step && (nextStart == NULL || step < nextStart)) {
    uint8_t len;
    if (forthStepPayload(step, &len) && len > 0) { /* markers (len==0) skipped */
      xcopy(ctx.source, step + 4, len);            /* payload bytes follow [op][STRING_LABEL_VARIABLE][len] — see layout note */
      ctx.source[len] = 0;
      forthOuterRun(&ctx, FORTH_OUTER_DEFS_ONLY);
      if (lastErrorCode != ERROR_NONE) return;     /* halt: error already displayed; program not recorded as scanned */
    }
    uint8_t *next = findNextStep(step);
    if (!next || next <= step) break;              /* defensive, mirrors forthMarkerTurnsOn */
    step = next;
  }
  if (forthScannedCount < FORTH_SCAN_MAX) {
    forthScannedProgs[forthScannedCount++] = progStart;
  }
  /* If the list is full the program is scanned but unrecorded: a later touch
     re-scans and recompiles. Shadowing keeps lookups correct (forthFindColon
     walks latest-first) at the cost of dictionary bytes — bounded, documented
     exception to D-2b. FORTH_SCAN_MAX=8 distinct Forth-bearing programs per
     run is beyond any realistic calculator session. */
}
```

**Payload layout note (must be re-derived, not assumed):** `forthStepPayload`
takes the *step start* and reads `step[2] == STRING_LABEL_VARIABLE`,
`step[3] == len` [VERIFIED: packages/forth-core/forth_bridge.c:21-29], i.e.
a two-byte opcode precedes it, and the ITM_FORTH dispatch arm passes
`forthProgramStep` a pointer already advanced to the `[len]` byte
[VERIFIED: packages/forth-core/programming/lblGtoXeq.c:861-863: `*step++`
consumes `STRING_LABEL_VARIABLE`, then `forthProgramStep(step)` with `*step`
= len]. So inside the pre-scan, payload bytes start at `step + 4`; inside
`forthProgramStep`, at `payload + 1`. The implementer must add a static
assertion of consistency in the test suite (T2.7 covers the boundary).

Final `forthProgramStep` (supersedes the P3 interim form):

```c
void forthProgramStep(const uint8_t *payload) {
  forthRunGenCheckReset();                 /* generation first: may clear dict + scan list */
  forthPreScanOwningProgram(payload);      /* payload lies inside the step, inside the program:
                                              the <= comparison in forthOwningProgramStart holds */
  if (lastErrorCode != ERROR_NONE) return; /* pre-scan error halts before executing this step */
  forthOuterCtx_t ctx;
  uint8_t len = *payload;
  xcopy(ctx.source, payload + 1, len);
  ctx.source[len] = 0;
  forthOuterRun(&ctx, FORTH_OUTER_SKIP_DEFS);
}
```

Ordering constraint carried over from the audit: the generation check and the
pre-scan run **before** the payload copy — `payload` points into program
memory and stays valid because the pre-scan never edits programs, but any
future reordering that copies first must not be "simplified" back, because
`forthRunGenCheckReset` conceptually invalidates dictionary-derived state,
not program memory.

`fnForthOuter` keeps `FORTH_OUTER_FULL` — interactive lines compile and
execute in place, exactly today's semantics; there is no owning program to
scan.

### 2.5 Documented limitations (state these in DESIGN.md §9.2 as part of the P2 commit)

- Definition-body forward references (def → later def) error at pre-scan, at
  the step containing the referencing definition.
- Editing programs between single-steps of a paused run leaves both the
  dictionary and the scanned-programs list stale until the next generation
  bump (run restart). This is the pre-existing §9.3 single-step semantic;
  the scanned list only ever *compares* stored pointers (never dereferences
  them), so staleness is memory-safe. A moved program re-scans (harmless,
  costs dict bytes); the theoretical collision of a *different* program
  landing exactly on a recorded start address after an edit is accepted and
  documented (same class of staleness §9.3 already accepts).
- `FORTH_SCAN_MAX` overflow ⇒ re-scan/recompile fallback (see code comment).

### 2.6 Pillar 2 tests

Test programs are written through the existing suite helpers that build
program memory (`writeTestProgram`-family in `test_dict_reloc.c`), each test
bumping the generation (`forthRunGenBump()`) before its run to start clean.

- **T2.1 — forward reference, tail → later definition.** Program: step 1
  Forth `"FWD"`, step 2 Forth `": FWD 42 ;"`. Run the program. Assert X = 42
  and no error. **Must fail if:** the pre-scan is skipped or scoped to steps
  before the current one (execute-in-place would raise
  `ERROR_FUNCTION_NOT_FOUND` at step 1) — this is the pillar's reason to
  exist.
- **T2.2 — D-2a: no early tail execution.** Program: step 1 Forth
  `": A 1 ; 99"`, step 2 Forth `"A"`. Run. Assert the stack holds exactly
  two pushes: 99 (step 1 tail) then 1 (step 2), and stack depth grew by
  exactly 2. **Must fail if:** `DEFS_ONLY` executes interpret-state tokens
  (the pre-scan would push 99 a second time ⇒ depth grows by 3).
- **T2.3 — D-2b: no recompilation.** Program: step 1 Forth `": B 5 ;"`,
  step 2 Forth `"B"`. Run; capture `fdict.count` and `fdict.here`; run the
  *same generation* path again by executing step 2's payload once more.
  Assert `fdict.count == 1` after the full run and `here` unchanged by the
  second touch. **Must fail if:** `SKIP_DEFS` falls back to compiling
  (count becomes 2 / `here` grows when execution passes the defining step).
- **T2.4 — D-2c: owning-program scope.** Two separate programs: P1 contains
  Forth `": ONLY1 8 ;"`; P2 contains Forth `"ONLY1"`. Run P2 (never running
  P1). Assert `ERROR_FUNCTION_NOT_FOUND` and `fdict.count == 0`. **Must fail
  if:** the pre-scan walks all programs (a global scan would compile ONLY1
  and the run would succeed) — the exact behavior D-2c forbids.
- **T2.5 — generation reset re-arms the scan.** Run the T2.1 program to
  completion. Call `forthRunGenBump()` (simulating a new interactive run),
  then run it again. Assert it succeeds again and `fdict.count` equals the
  fresh-compile count (not doubled). **Must fail if:**
  `forthRunGenCheckReset` clears the dictionary but not `forthScannedCount`
  (scan skipped after dict clear ⇒ FUNCTION_NOT_FOUND on the second run).
- **T2.6 — pre-scan error halts before the triggering step executes.**
  Program: step 1 Forth `"77 : C NOSUCHWORD ;"`, step 2 anything. Run.
  Assert `ERROR_FUNCTION_NOT_FOUND` (compile-state undefined word), assert
  stack depth grew by **zero** (step 1's tail `77` never executed), and
  assert the program is *not* in the scanned list (a retry after fixing
  would re-scan): observable by asserting `fdict.count == 0`. **Must fail
  if:** pre-scan errors are swallowed (`lineOK` reset) and execution
  proceeds with a partial dictionary, or the halt happens after the step
  tail ran.
- **T2.7 — last-step definitions are visible (walk bound).** Program: step 1
  Forth `"LAST"`, step 2 Forth `": LAST 3 ;"` where step 2 is the **final**
  step of the final program in memory (`forthNextProgramStart` returns
  NULL). Run. Assert X = 3. **Must fail if:** the pre-scan walk uses an
  exclusive bound that drops the last step (e.g. reuses
  `forthMarkerTurnsOn`'s `step < markerStep` pattern) or mishandles the
  NULL next-program sentinel.
- **T2.8 — first-touch across XEQ into a second program.** P1: step 1 Forth
  `"9"`, step 2 XEQ label of P2; P2: Forth `": P2W 4 ; P2W"`. Run P1.
  Assert final stack contains 9 then 4. **Must fail if:** the scanned list
  holds only one entry (P2's first touch evicts/ignores P1 — e.g. a single
  static pointer instead of the array) or nested `forthProgramStep` is
  blocked (regression guard on the P3 depth work).
- **Arena duty:** after T2.8, print
  `FORTH ARENA (post-prescan): here=.. sizeBlocks=..`; the human report must
  quote it (pre-scan compiles all definitions up front, so this is the new
  dictionary high-water profile).

---

## Consolidated deliverables checklist (for the final report to the human)

1. Gate green after each pillar commit (`ALL PASSED`, exit 0), with the
   mutation-verification of each new test noted (bug injected → red →
   reverted → green).
2. Arena high-water values: suite-end, post-restore (T1), post-prescan
   (T2.8).
3. BSS deltas for P3 (expected ≈ −255 B idle) and P2 (expected ≈ +33 B ARM).
4. DESIGN.md updates: §5.5/H5 marked implemented (P1), §3.2 guard section
   rewritten for depth/watermark model (P3), §9.2 Architecture 2 marked
   implemented with the §2.5 limitations recorded (P2).
5. No changes outside `packages/forth-core/` working files + regenerated
   `patches/`/`files/`; `core/freeList.c` untouched.

## Open items carried out of Phase 2

- `[UNVERIFIED — needs confirmation]` DMCP-build C-stack budget; required
  only before raising `FORTH_NEST_MAX`/`FORTH_OUTER_NEST_MAX`, not for this
  implementation.
