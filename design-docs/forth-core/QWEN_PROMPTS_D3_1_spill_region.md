# D3-1 — spill region + accessors (no behavior change) — Part A

Origin: DESIGN.md §11 (owner ruling 2026-08-03); queue QWEN_RUNBOOK §2b.
This packet builds the arena-backed spill region and its API, plus unit
tests. NOTHING calls the API from product paths yet — D3-2 wires it into
`forthDataDepthApply`. Authored per runbook §4a; Part B (mutations) is
directed separately after this lands.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` clean.
2. `grep -n "static int16_t forthDataDepth" packages/forth-core/forth_inner.c`
   → exactly ONE match (the module the spill joins).
3. `grep -n "forthDataDepthLeaveOuter" packages/forth-core/forth_inner.c | head -1`
   matches (the reset seam).
4. `grep -c "getRegisterFullSizeInBlocks" src/c47/registers.h` → at least 1;
   `grep -c "define getStackTop" src/c47/defines.h` → exactly 1.
5. `grep -n "T5 split: forward declarations" packages/forth-core/test_dict_reloc.c`
   → exactly ONE match (new tests go in the MAIN file, core area, before
   this block's neighborhood — see Step 2).

## LAYOUT AND API FACTS (§4a-1 — stated, do not re-derive)

- A spill SLOT is a variable-length record in one arena block, packed
  back-to-back, LIFO via a top byte-offset:
  `[uint32 dataType][uint16 sizeInBlocks][payload sizeInBlocks*4 bytes]`.
- Register images are read/written ONLY with:
  `getRegisterDataType(reg)`, `getRegisterDataPointer(reg)`,
  `getRegisterFullSizeInBlocks(reg)` (src/c47/registers.h),
  `allocC47Blocks` / `reallocC47Blocks` / `freeC47Blocks`, and `xcopy`.
- Setting a register from a slot uses `setRegisterDataType(reg, type, amNone)`
  and `setRegisterDataPointer(reg, allocC47Blocks(sizeInBlocks))` then
  `xcopy` — grep `setRegisterDataType` in src/c47/registers.h first and
  STOP if the three-arg form does not match.

## Task — module + API + five test subcases

**Step 1.** In `packages/forth-core/forth_inner.c`, immediately AFTER the
`forthDataDepth`/`forthOuterActive` statics, add the spill module:

```c
/* D3-1 spill region (DESIGN.md §11): arena-backed, per-execution, LIFO.
 * Unused by product paths until D3-2 wires forthDataDepthApply to it.
 * Slot: [uint32 dataType][uint16 sizeInBlocks][payload]. */
static void    *forthSpillBase   = NULL;   /* arena block, or NULL */
static uint16_t forthSpillBlocks = 0;      /* allocated size in blocks */
static uint32_t forthSpillTop    = 0;      /* byte offset one past last slot */
static uint16_t forthSpillSlots  = 0;      /* live slot count */

uint16_t forthSpillCount(void) { return forthSpillSlots; }

void forthSpillReset(void)
{
  if (forthSpillBase) {
    freeC47Blocks(forthSpillBase, forthSpillBlocks);
  }
  forthSpillBase = NULL; forthSpillBlocks = 0;
  forthSpillTop = 0; forthSpillSlots = 0;
}

bool_t forthSpillCatch(calcRegister_t reg)
{
  uint32_t type   = getRegisterDataType(reg);
  uint16_t blocks = getRegisterFullSizeInBlocks(reg);
  uint32_t need   = forthSpillTop + 6u + (uint32_t)blocks * 4u;
  if (forthSpillBase == NULL || need > (uint32_t)forthSpillBlocks * 4u) {
    uint16_t newBlocks = (uint16_t)((need + 63u) / 4u + 16u);
    void *nb = forthSpillBase
      ? reallocC47Blocks(forthSpillBase, forthSpillBlocks, newBlocks)
      : allocC47Blocks(newBlocks);
    if (nb == NULL) { return false; }          /* arena exhausted: caller errors */
    forthSpillBase = nb; forthSpillBlocks = newBlocks;
  }
  { uint8_t *p = (uint8_t *)forthSpillBase + forthSpillTop;
    xcopy(p, &type, 4);
    xcopy(p + 4, &blocks, 2);
    xcopy(p + 6, getRegisterDataPointer(reg), (uint32_t)blocks * 4u);
  }
  forthSpillTop += 6u + (uint32_t)blocks * 4u;
  forthSpillSlots++;
  return true;
}

bool_t forthSpillRefill(calcRegister_t reg)
{
  if (forthSpillSlots == 0) { return false; }
  { /* walk from the base to find the LAST slot's offset */
    uint32_t off = 0, prev = 0; uint16_t n = forthSpillSlots;
    while (n-- > 0) {
      uint16_t blocks; prev = off;
      xcopy(&blocks, (uint8_t *)forthSpillBase + off + 4, 2);
      off += 6u + (uint32_t)blocks * 4u;
    }
    { uint8_t *p = (uint8_t *)forthSpillBase + prev;
      uint32_t type; uint16_t blocks;
      xcopy(&type, p, 4);
      xcopy(&blocks, p + 4, 2);
      freeRegisterData(reg);
      setRegisterDataPointer(reg, allocC47Blocks(blocks));
      if (getRegisterDataPointer(reg) == NULL) { return false; }
      setRegisterDataType(reg, type, amNone);
      xcopy(getRegisterDataPointer(reg), p + 6, (uint32_t)blocks * 4u);
      forthSpillTop = prev;
      forthSpillSlots--;
    }
  }
  return true;
}
```

If `setRegisterDataType`'s real signature differs (grep it), STOP and
report — do not adapt.

**Step 2.** Wire the reset seam: inside `forthDataDepthLeaveOuter`, add a
single `forthSpillReset();` call (read its body first; place the call
alongside the existing depth reset). Also add `forthSpillReset();` in
`forthDataDepthEnterOuter` the same way (defensive: a crash between
executions must not leak a stale spill).

**Step 3.** Declare the four functions in `packages/forth-core/forth_dict.h`
next to the existing `forthDataDepth*` declarations (grep them).

**Step 4.** Five test subcases in `packages/forth-core/test_dict_reloc.c`
(the MAIN file — core area), one new function
`static int test_spill_region(void)` placed immediately before the
`/* T5 split: forward declarations` block, called from the runner right
after the `fail |= test_data_stack_overflow_guard();` line (grep it;
exactly one match). Each subcase prints one PASS/FAIL line:

- **SP-1 round trip preserves type+payload**: seed REGISTER_X with a
  real34 (use `x_set_real34` if present — grep; otherwise reuse whatever
  the neighbouring stack tests use to seed X, read that slice), catch
  from REGISTER_X, alter X, refill into REGISTER_Y, assert Y's dataType
  and payload bytes equal the original (compare via
  getRegisterFullSizeInBlocks + memcmp of data pointers).
- **SP-2 LIFO over three values**: catch X three times with three
  distinct seeded values; refill three times; assert reverse order.
- **SP-3 reset frees**: record `forthSpillCount()==0` after
  `forthSpillReset()`; catch twice; reset; assert count 0 and a
  subsequent refill returns false.
- **SP-4 growth**: catch 40 values in a loop (forces at least one
  realloc); refill 40; assert count returns to 0 and the last refilled
  value matches the first caught (LIFO end).
- **SP-5 empty refill is false**: on a fresh reset, `forthSpillRefill`
  returns false and the target register is untouched (seed Y, attempt
  refill, assert Y unchanged).

**Step 5.** Gate:
`./packages/forth-core/build-test.sh > /tmp/forth-d3-1-gate.log 2>&1; echo "gate exit: $?"`
(log name per THIS packet). Success = exit 0 + both banners + five
`SP-` PASS lines + the arena line unchanged from baseline
(`FORTH ARENA: dict here=48 sizeBlocks=16 gdict here=16 sizeBlocks=16
freeRamDelta=128`) — the spill must free everything it takes.
Print the five SP lines and the arena line, then STOP. No commit, no
mutations — the architect directs those.
