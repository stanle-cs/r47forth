# PACKET L1-5D — arena and program-memory residue (C2.2 / C2.3)

**Stage L final packet, session D.** Parent: `PACKET_L1_5_acceptance.md`
(C2.2, C2.3). **Prerequisite: session C landed and green** (commit
`L1-5: the interactive close-path sweep — 7 paths, full tuple (C2.1)`).
Adds `test_interactive_residue` and registers it. **Transcribe the code
EXACTLY**; judgment is limited to placement, gate, report. STOP with
`[SOL DEBUGGER HANDOFF]` on any contradiction. NO mutations here.

## Implementer contract

As PACKET_L1_5A rev 2: only `packages/forth-core/test_capture.part.h`
and `packages/forth-core/test_dict_reloc.c`; todo
`/tmp/qwen-l1-5d-todo.md`; gate log `/tmp/qwen-l1-5d-gate.log` (override
any remembered form); green iff `FORTH SELF-TEST: ALL PASSED` and
`BUILD + SELF-TEST GREEN`; quote the `[DEBUG] running
test_interactive_residue...` line, both PASS lines, the REPORT line, and
the `FORTH ARENA` line verbatim; one commit, exact message.

## EXECUTION GATE (STOP on mismatch)

```
grep -c "static int test_interactive_residue" packages/forth-core/test_capture.part.h      # expect 0
grep -c "fail |= test_interactive_close_sweep();" packages/forth-core/test_dict_reloc.c    # expect 1
grep -c "BYTES_PER_BLOCK" packages/forth-core/test_capture.part.h                          # expect >=1
git status --short | wc -l                                                                  # expect 0
git log --oneline -3 | grep -c "7 paths, full tuple"                                       # expect 1
```

## Edit 1 — the test body (APPEND at the very end of `packages/forth-core/test_capture.part.h`)

```c

/* ==================================================================
 * PACKET_L1_5 (C2.2/C2.3) — test_interactive_residue.
 * [1] the capture lifecycle itself allocates nothing: 20 open/close
 *     cycles return getFreeRamMemory() to baseline (escape valve per
 *     the landed F6-2 [6] precedent: bounded, block-aligned,
 *     growth-only allocator quantization is reported, not failed).
 * [2] a full history cap cycle grows program memory by exactly FHIST's
 *     own bytes — nothing else leaks; the cap holds; eviction is
 *     oldest-first.  The plateau is the C4 "program-memory high-water"
 *     number, printed as a REPORT line.
 * ================================================================== */
static int test_interactive_residue(void)
{
  extern void fnForthOuter(uint16_t);
  extern void fnKeyExit(uint16_t);

  int fail = 0, scFail;
  int i;

  bool_t savedAlpha = getSystemFlag(FLAG_ALPHA);
  uint8_t savedCalcMode = calcMode;
  int16_t savedCatalog = catalog;
  int16_t savedTamFunction = tam.function;
  int16_t savedTamMode = tam.mode;
  uint8_t savedProgramRunStop = programRunStop;
  int16_t savedDynamicMenu = dynamicMenuItem;
  int16_t savedCursorPos = T_cursorPos;
  bool_t savedShiftF = shiftF;
  bool_t savedShiftG = shiftG;
  softmenuStack_t savedStack[SOFTMENU_STACK_SIZE];
  xcopy(savedStack, softmenuStack, sizeof(savedStack));

  #define L15D_RESET() do { \
    calcMode = CM_NORMAL; catalog = CATALOG_NONE; tam.mode = 0; tam.function = 0; \
    programRunStop = PGM_STOPPED; dynamicMenuItem = -1; alphaCase = AC_UPPER; \
    nextChar = NC_NORMAL; shiftF = false; shiftG = false; \
    clearSystemFlag(FLAG_ALPHA); lastErrorCode = ERROR_NONE; forthCapClose(); \
  } while (0)

  cleanupTestProgram();
  {
    testProg_t base;
    tpInit(&base);
    tpLbl(&base, "BASED");
    tpEnd(&base);
    if (!tpWrite(&base)) {
      printf("    FIXTURE FAIL: baseline build/write\n");
      cleanupTestProgram();
      return 1;
    }
  }

  /* ---- [1] 20 empty open/close cycles: zero arena residue. ---- */
  scFail = 0;
  L15D_RESET();
  /* Warmup: absorb any first-open, first-menu effects before measuring. */
  fnForthOuter(NOPARAM);
  fnKeyExit(NOPARAM);
  L15D_RESET();
  {
    uint32_t freeBefore = getFreeRamMemory();
    for (i = 0; i < 20; i++) {
      fnForthOuter(NOPARAM);
      if (!forthCapIsOpen() || !forthCapIsInteractive()) {
        printf("    [1] FIXTURE FAIL: open %d did not take\n", i);
        scFail = 1;
        break;
      }
      fnKeyExit(NOPARAM);
      if (forthTestCapState() != FCAP_CLOSED) {
        printf("    [1] FIXTURE FAIL: close %d did not take\n", i);
        scFail = 1;
        break;
      }
    }
    if (!scFail) {
      uint32_t freeAfter = getFreeRamMemory();
      if (freeAfter != freeBefore) {
        uint32_t delta = (freeBefore > freeAfter) ? (freeBefore - freeAfter)
                                                  : (freeAfter - freeBefore);
        /* Escape valve, landed F6-2 [6] shape: bounded, block-aligned,
         * growth-only allocator quantization is a report, not a leak. */
        if (delta % BYTES_PER_BLOCK == 0 && delta <= 6 * BYTES_PER_BLOCK
            && freeBefore > freeAfter) {
          printf("    [1] PASS (escape valve): freeRam %u -> %u is %u resize quantum(s), not a lifecycle leak\n",
                 (unsigned)freeBefore, (unsigned)freeAfter,
                 (unsigned)(delta / BYTES_PER_BLOCK));
        } else {
          printf("    [1] FAIL: freeRam %u -> %u across 20 empty open/close cycles\n",
                 (unsigned)freeBefore, (unsigned)freeAfter);
          scFail = 1;
        }
      } else {
        printf("    [1] PASS: 20 open/close cycles leave getFreeRamMemory() at baseline (no arena residue)\n");
      }
    }
  }
  fail |= scFail;

  /* ---- [2] Full cap cycle: growth == FHIST's own bytes, cap holds,
   * eviction is oldest-first. ---- */
  scFail = 0;
  L15D_RESET();
  {
    uint32_t pgmOffBefore = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);
    char line[16];
    uint16_t prog;

    if (forthHistoryProgram() != 0) {
      printf("    [2] FIXTURE FAIL: FHIST already exists before the cap cycle\n");
      scFail = 1;
    }
    if (!scFail) {
      for (i = 0; i < 200; i++) {
        sprintf(line, "RLINE%04d", i);
        forthHistoryPush(line);
      }
      prog = forthHistoryProgram();
      if (prog == 0) {
        printf("    [2] FAIL: FHIST not created by 200 pushes\n");
        scFail = 1;
      } else {
        uint8_t *begin = programList[prog - 1].instructionPointer;
        uint8_t *step = begin;
        uint8_t *firstContent = NULL;
        uint32_t totalBytes;
        uint32_t pgmOffAfter;
        while (!(isAtEndOfProgram(step) || isAtEndOfPrograms(step))) {
          if (firstContent == NULL && step != begin) { firstContent = step; }
          step = findNextStep(step);
        }
        totalBytes = (uint32_t)(step - begin) + 2;
        pgmOffAfter = (uint32_t)(firstFreeProgramByte - beginOfProgramMemory);

        printf("    [2] REPORT: program-memory high-water with a full history: %u bytes (cap %u)\n",
               (unsigned)totalBytes, (unsigned)FORTH_HISTORY_MAX_BYTES);

        /* The cap is asserted as the LITERAL 1024, not the macro: a
         * fixture sized from the constant is immune to a change in the
         * constant, and therefore blind to one (the G2 cut-off lesson,
         * QWEN_RUNBOOK §2c).  The L1-H cap subcase uses the macro and is
         * legacy evidence; this is the literal pin beside the mechanism. */
        if (totalBytes > 1024) {
          printf("    [2] FAIL: FHIST is %u bytes, over the 1024-byte cap\n",
                 (unsigned)totalBytes);
          scFail = 1;
        }
        if (!scFail && (pgmOffAfter - pgmOffBefore) != totalBytes) {
          printf("    [2] FAIL: program memory grew %u bytes but FHIST is %u — residue outside FHIST\n",
                 (unsigned)(pgmOffAfter - pgmOffBefore), (unsigned)totalBytes);
          scFail = 1;
        }
        if (!scFail && (firstContent == NULL || stepSrcTextEq(firstContent, "RLINE0000"))) {
          printf("    [2] FAIL: oldest line survived a full cap cycle (eviction not oldest-first)\n");
          scFail = 1;
        }
        if (!scFail) {
          printf("    [2] PASS: a full cap cycle grows program memory by exactly FHIST's own bytes (cap respected, oldest evicted)\n");
        }
      }
    }
  }
  fail |= scFail;

  forthCapClose();
  cleanupTestProgram();
  #undef L15D_RESET
  clearSystemFlag(FLAG_ALPHA);
  calcMode = savedCalcMode;
  catalog = savedCatalog;
  tam.function = savedTamFunction;
  tam.mode = savedTamMode;
  programRunStop = savedProgramRunStop;
  dynamicMenuItem = savedDynamicMenu;
  T_cursorPos = savedCursorPos;
  shiftF = savedShiftF;
  shiftG = savedShiftG;
  xcopy(softmenuStack, savedStack, sizeof(savedStack));
  if (savedAlpha) setSystemFlag(FLAG_ALPHA); else clearSystemFlag(FLAG_ALPHA);
  lastErrorCode = ERROR_NONE;

  return fail;
}
```

## Edit 2 — forward declaration (`test_dict_reloc.c`)

Insert immediately after
`static int test_interactive_close_sweep(void);                /* L1-5 */`:

```c
static int test_interactive_residue(void);                    /* L1-5 */
```

## Edit 3 — invocation (`test_dict_reloc.c`)

Find the block

```c
  forthDictInit();
  printf("  [DEBUG] running test_interactive_close_sweep...\n");
  fail |= test_interactive_close_sweep();
  forthDictClear();
  forthGDictClear();
```

and insert immediately AFTER it:

```c

  forthDictInit();
  printf("  [DEBUG] running test_interactive_residue...\n");
  fail |= test_interactive_residue();
  forthDictClear();
  forthGDictClear();
```

## Gate, report, commit

Gate green required; STOP on any red outside this session's writes or
any FIXTURE FAIL. Report the `[DEBUG]` line, both PASS lines, the REPORT
line, and the `FORTH ARENA` line verbatim. Commit exactly:

```
git add packages/forth-core/test_capture.part.h packages/forth-core/test_dict_reloc.c
git commit -m "L1-5: arena and program-memory residue — the lifecycle leaks nothing (C2.2/C2.3)"
```
