# PACKET L1-5B — the stage story, part 2 (C1 steps 8–10)

**Stage L final packet, session B.** Parent: `PACKET_L1_5_acceptance.md`
(C1). **Prerequisite: session A landed and green** (commit
`L1-5: the stage story, part 1 — open to EXIT (C1 steps 1-7)`). This
session replaces the seam comment inside `test_interactive_acceptance`
with steps 8–10, inlined below. **Transcribe EXACTLY.** Your judgment is
limited to placement, running the gate, and reporting discrepancies. If
the code contradicts the tree, STOP and report `[SOL DEBUGGER HANDOFF]` —
do not adapt. NO mutations in this session.

## Implementer contract

- Work ONLY in `packages/forth-core/test_capture.part.h`.
- Todo file: `/tmp/qwen-l1-5b-todo.md`. Gate log: `/tmp/qwen-l1-5b-gate.log`
  (these names OVERRIDE any remembered form).
- Gate: `./packages/forth-core/build-test.sh > /tmp/qwen-l1-5b-gate.log 2>&1`;
  green iff `FORTH SELF-TEST: ALL PASSED` **and** `BUILD + SELF-TEST GREEN`.
- Literals are law. **Report requirement:** quote the
  `[DEBUG] running test_interactive_acceptance...` line and ALL TEN
  `    [N] PASS:` lines verbatim from the green log, plus the
  `FORTH ARENA` line.
- Finish with the commit step. One commit, exact message.

## EXECUTION GATE (run these, compare, STOP on mismatch)

```
grep -c "L1-5B: steps 8-10 are appended here by session B." packages/forth-core/test_capture.part.h  # expect 1
grep -c "static int test_interactive_acceptance" packages/forth-core/test_capture.part.h             # expect 1
grep -c "fail |= test_interactive_acceptance();" packages/forth-core/test_dict_reloc.c               # expect 1
grep -c "static bool_t stepSrcTextEq" packages/forth-core/test_dict_reloc.c                          # expect 1
git status --short | wc -l                                                                            # expect 0
git log --oneline -3 | grep -c "the stage story, part 1"                                             # expect 1
```

## The edit — replace the seam line

In `packages/forth-core/test_capture.part.h`, replace the single line

```c
  /* L1-5B: steps 8-10 are appended here by session B. */
```

with everything in the block below, verbatim:

```c
  /* ---- [8] FORTH again; f-up recalls FHIST's newest; the four-line
   * record.  This pins push-before-run (steps 2,3,4,6), the fold's text
   * as pushed ("STO 05 "), and the recall gesture, end to end. ---- */
  scFail = 0;
  lastErrorCode = ERROR_NONE;
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive() || aimBuffer[0] != 0) {
    printf("    [8] FIXTURE FAIL: reopen did not take (state=%d, line=\"%s\")\n",
           forthTestCapState(), aimBuffer);
    scFail = 1;
  }
  if (!scFail) {
    extern void processKeyAction(int16_t);
    int upRow = -1, i;
    char kbUp[3];
    int16_t itUp;
    for (i = 0; i < 37; i++) {
      if (kbd_std[i].primary == ITM_UP1) { upRow = i; }
    }
    if (upRow < 0) {
      printf("    [8] FIXTURE FAIL: ITM_UP1 not on kbd_std\n");
      scFail = 1;
    } else {
      sprintf(kbUp, "%02d", upRow);
      /* shiftF is one-shot: determineItem's own resetShiftState() clears
       * it after the call (landed recall idiom, L1-H C5.6). */
      shiftF = true;
      itUp = determineItem(kbUp);
      shiftF = false;
      processKeyAction(itUp);
    }
  }
  if (!scFail) {
    uint16_t prog = forthHistoryProgram();
    if (prog == 0) {
      printf("    [8] FAIL: FHIST does not exist after the session\n");
      scFail = 1;
    } else {
      uint8_t *lbl = programList[prog - 1].instructionPointer;
      uint8_t *s1 = findNextStep(lbl);
      uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
      uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
      uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
      uint8_t *s5 = s4 ? findNextStep(s4) : NULL;
      if (!s1 || !s2 || !s3 || !s4 || !s5) {
        printf("    [8] FAIL: FHIST walk broke before five steps\n");
        scFail = 1;
      }
      if (!scFail && (!stepSrcTextEq(s1, "1 2 +") ||
                      !stepSrcTextEq(s2, ": SQ DUP * ;") ||
                      !stepSrcTextEq(s3, "4 SQ") ||
                      !stepSrcTextEq(s4, "STO 05 ") ||
                      !isAtEndOfProgram(s5))) {
        printf("    [8] FAIL: FHIST is not the session's four lines in order\n");
        scFail = 1;
      }
      /* The recall matches FHIST's newest by DIRECT comparison against
       * the recalled buffer — one comparison, no second literal to
       * drift (the L1-F3 parity discipline). */
      if (!scFail && !stepSrcTextEq(s4, aimBuffer)) {
        printf("    [8] FAIL: recalled line \"%s\" is not FHIST's newest\n", aimBuffer);
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [8] PASS: f-up recalls FHIST's newest; history holds the session's four lines in order\n");
  fail |= scFail;

  /* ---- [9] EXIT collapses the duplicate; XEQ 'FHIST' re-runs the
   * session (L-R7: deliberately runnable). ---- */
  scFail = 0;
  fnKeyExit(NOPARAM);
  if (forthTestCapState() != FCAP_CLOSED) {
    printf("    [9] FAIL: state %d after EXIT, expected FCAP_CLOSED\n", forthTestCapState());
    scFail = 1;
  }
  if (!scFail) {
    /* The EXIT pushed the recalled text — a consecutive duplicate of the
     * newest entry, so it must COLLAPSE: still exactly four lines. */
    uint16_t prog = forthHistoryProgram();
    uint8_t *lbl = prog ? programList[prog - 1].instructionPointer : NULL;
    uint8_t *s1 = lbl ? findNextStep(lbl) : NULL;
    uint8_t *s2 = s1 ? findNextStep(s1) : NULL;
    uint8_t *s3 = s2 ? findNextStep(s2) : NULL;
    uint8_t *s4 = s3 ? findNextStep(s3) : NULL;
    uint8_t *s5 = s4 ? findNextStep(s4) : NULL;
    if (!s4 || !s5 || !stepSrcTextEq(s4, "STO 05 ") || !isAtEndOfProgram(s5)) {
      printf("    [9] FAIL: EXIT's push did not collapse as a consecutive duplicate\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    /* Discriminators: zero R05, put 5 in X — the run must RECOMPUTE both. */
    longIntegerInit(li); int32ToLongInteger(0, li);
    convertLongIntegerToLongIntegerRegister(li, 5); longIntegerFree(li);
    longIntegerInit(li); int32ToLongInteger(5, li);
    convertLongIntegerToLongIntegerRegister(li, REGISTER_X); longIntegerFree(li);
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    lastErrorCode = ERROR_NONE;
    {
      extern void fnExecute(uint16_t);
      calcRegister_t lblF = findNamedLabel("FHIST", GLOBAL_LABELS);
      if (lblF == INVALID_VARIABLE) {
        printf("    [9] FAIL: FHIST label not findable by name — XEQ 'FHIST' (L-R7) would not work\n");
        scFail = 1;
      } else {
        fnExecute(lblF);
        if (lastErrorCode != ERROR_NONE) {
          printf("    [9] FAIL: FHIST run errored (%d)\n", lastErrorCode);
          scFail = 1;
        }
        if (!scFail && !x_is_longint(16)) {
          printf("    [9] FAIL: X != 16 after the FHIST re-run\n");
          scFail = 1;
        }
        if (!scFail) {
          read_reg_int32(5, &rType, &rVal);
          if (rType != dtLongInteger || rVal != 16) {
            printf("    [9] FAIL: register 05 = %ld type %u, expected 16 (the replay re-ran \"STO 05 \")\n",
                   (long)rVal, rType);
            scFail = 1;
          }
        }
      }
    }
  }
  if (!scFail) printf("    [9] PASS: XEQ 'FHIST' re-runs the session's lines (X and R05 recomputed)\n");
  fail |= scFail;

  /* ---- [10] Durability: GLOBAL survives the next lifetime; the
   * interactive-scope word does not (§8.3, L3's contract). ---- */
  scFail = 0;
  lastErrorCode = ERROR_NONE;
  calcMode = CM_NORMAL;
  fnForthOuter(NOPARAM);
  if (!forthCapIsOpen() || !forthCapIsInteractive()) {
    printf("    [10] FIXTURE FAIL: reopen did not take\n");
    scFail = 1;
  }
  if (!scFail) {
    /* ": TGLO 6 ; GLOBAL" through the key path. */
    runFunction(ITM_COLON); runFunction(ITM_SPACE);
    runFunction(ITM_T); runFunction(ITM_G); runFunction(ITM_L); runFunction(ITM_O);
    runFunction(ITM_SPACE); runFunction(ITM_6); runFunction(ITM_SPACE);
    runFunction(ITM_SEMICOLON); runFunction(ITM_SPACE);
    runFunction(ITM_G); runFunction(ITM_L); runFunction(ITM_O);
    runFunction(ITM_B); runFunction(ITM_A); runFunction(ITM_L);
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [10] FAIL: \": TGLO 6 ; GLOBAL\" errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
  }
  if (!scFail) {
    /* ": TDUR 5 ;" — stays interactive-scope. */
    runFunction(ITM_COLON); runFunction(ITM_SPACE);
    runFunction(ITM_T); runFunction(ITM_D); runFunction(ITM_U); runFunction(ITM_R);
    runFunction(ITM_SPACE); runFunction(ITM_5); runFunction(ITM_SPACE);
    runFunction(ITM_SEMICOLON);
    fnKeyEnter(NOPARAM);
    if (lastErrorCode != ERROR_NONE) {
      printf("    [10] FAIL: \": TDUR 5 ;\" errored (%d)\n", lastErrorCode);
      scFail = 1;
    }
    fnKeyExit(NOPARAM);
  }
  if (!scFail && (!forthFindColon("TGLO", &idx) || !forthFindColon("TDUR", &idx))) {
    printf("    [10] FIXTURE FAIL: TGLO/TDUR not both visible before the reset\n");
    scFail = 1;
  }
  if (!scFail) {
    /* The lifetime-reset program: one ITM_FORTH step.  This tpWrite
     * REPLACES program memory and destroys FHIST — deliberate; every
     * FHIST assertion is behind us (steps 8-9). */
    testProg_t p;
    tpInit(&p);
    tpLbl(&p, "TLIF");
    tpSrc(&p, "1");
    tpEnd(&p);
    if (!tpWrite(&p)) {
      printf("    [10] FIXTURE FAIL: TLIF build/write\n");
      scFail = 1;
    }
  }
  if (!scFail) {
    extern void fnExecute(uint16_t);
    calcRegister_t lblT;
    programRunStop = PGM_STOPPED;
    dynamicMenuItem = -1;
    lastErrorCode = ERROR_NONE;
    lblT = findNamedLabel("TLIF", GLOBAL_LABELS);
    if (lblT == INVALID_VARIABLE) {
      printf("    [10] FIXTURE FAIL: findNamedLabel(\"TLIF\") returned INVALID_VARIABLE\n");
      scFail = 1;
    } else {
      fnExecute(lblT);
      if (lastErrorCode != ERROR_NONE) {
        printf("    [10] FAIL: TLIF run errored (%d)\n", lastErrorCode);
        scFail = 1;
      }
      if (!scFail && !x_is_longint(1)) {
        printf("    [10] FAIL: X != 1 (the ITM_FORTH step did not run)\n");
        scFail = 1;
      }
      if (!scFail && forthFindColon("TDUR", &idx)) {
        printf("    [10] FAIL: TDUR survived the lifetime reset\n");
        scFail = 1;
      }
      if (!scFail && !forthFindColon("TGLO", &idx)) {
        printf("    [10] FAIL: TGLO did not survive (GLOBAL is the durability mechanism)\n");
        scFail = 1;
      }
    }
  }
  if (!scFail) printf("    [10] PASS: GLOBAL survives the lifetime reset; the interactive-scope word does not\n");
  fail |= scFail;
```

## Gate, report, commit

1. Run the gate. Green required. Any red in a test this session did not
   write, or any `FIXTURE FAIL`: STOP, report `[SOL DEBUGGER HANDOFF]`
   with the log excerpt.
2. Report: quote the `[DEBUG]` line, all TEN PASS lines, and the
   `FORTH ARENA` line verbatim.
3. Commit exactly:

```
git add packages/forth-core/test_capture.part.h
git commit -m "L1-5: the stage story, part 2 — recall, FHIST re-run, durability (C1 steps 8-10)"
```
