# Stage 2 Three Pillars — Qwen implementation prompts

Fifteen small tasks, strictly ordered Q1→Q15. Each is sized for a ~100k-token
context window: it names the only files/regions to read and never requires
reading `DESIGN.md` (~2100 lines) or all of `test_dict_reloc.c` (~5700 lines).

**How to use:** for each task, paste the PREAMBLE below, then the task block,
into a fresh Qwen session. Do not run tasks out of order — later tasks assume
earlier ones landed. Between tasks, a quick human sanity check of the reported
gate output is enough. Commits happen only in Q5, Q10, Q15.

**Refinement vs. STAGE2_THREE_PILLARS_PLAN.md:** `forthOuterMode_t`,
`forthOuterCtx_t`, and `forthOuterRun` stay file-static in `forth_compile.c`
(every consumer, including the P2 pre-scan, lives in that file). The plan's
mention of a `forth_dict.h` enum placement is superseded by these prompts.

**Reserved for the architect (not Qwen):** all `DESIGN.md` edits (the §3.2,
§5.5, §9.2 status updates in the plan's deliverables checklist).

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at `/home/stan/c43`. You are an implementer, not a designer:
follow the spec exactly, make zero design decisions of your own. If an anchor
(a quoted line, function, or search string) does not match what you find in
the file, STOP immediately and report the mismatch instead of guessing.

Rules:
1. First run `git branch --show-current` and confirm you are on
   `forth-core/stage2-three-pillars`. If not, STOP and report.
2. The only build/test command is `./packages/forth-core/build-test.sh` (run
   from the repo root). Success = it prints `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.` and exits 0. Never invoke meson or ninja
   directly.
3. All edits go in `packages/forth-core/`. Never edit anything under `src/`.
   After EVERY edit to any file under `packages/forth-core/`, run:
   `python3 tools/pkg_patch_refresh.py packages/forth-core`
   (it regenerates `patches/` and `files/`; the build reads only those).
4. Never touch `src/c47/core/freeList.c` or any copy of it. Never read or
   edit `packages/forth-core/DESIGN.md`. Never read `test_dict_reloc.c` in
   full — read only the line ranges each task lists (use `sed -n 'A,Bp'` or
   `grep -n`).
5. Match the surrounding code style (indentation, brace placement, comment
   density). No commits unless the task explicitly says to commit.
   **NEVER run `git checkout`, `git restore`, `git reset`, `git stash`, or
   `git clean` — on anything, ever.** The working tree carries uncommitted
   work from earlier tasks; a file-level revert destroys it (this has
   happened). To revert a mutation you applied for verification, re-apply
   the inverse change with the Edit tool using the exact text you changed.
6. Finish by reporting: (a) files changed, (b) the last ~10 lines of the gate
   output, (c) results of any task-specific verification steps, (d) any
   deviation from the spec (there should be none).

**Two-attempt debugger handoff (mandatory).** This rule applies only when this
task authorizes you to fix the observed error; it never overrides an earlier
immediate-STOP rule. After a command, test, or gate first fails because of your
task changes, you may make at most two distinct repair attempts. A repair
attempt is an edit intended to clear that failure followed by rerunning the
relevant command. The original task implementation is not a repair attempt. If
the required command is still not green after repair attempt 2 — even if the
visible error changes — STOP. Do not make a third repair, broaden scope, or use
git to undo anything. Leave the tree exactly as it stands; read-only inspection
is allowed only to prepare this report:

`[SOL DEBUGGER HANDOFF]`

- task ID and exact failing command;
- original failure and its relevant verbatim output;
- attempt 1: files/hunks changed, rationale, and resulting output;
- attempt 2: files/hunks changed, rationale, and resulting output;
- current `git status --short`, `git diff --stat`, and relevant diff excerpts;
- your best remaining hypotheses and anything that surprised you.

---

## Q1 — Pillar 1: add `forthDictValidateRestored()`

Read: `packages/forth-core/forth_dict.c` lines 1-60 and
`packages/forth-core/forth_dict.h` lines 26-60. Nothing else.

In `forth_dict.c`, immediately after the `forthDictClear` function (it ends
around line 58), add exactly this function:

```c
/* H5 (§5.5): sanity-check fdict after a state restore. A torn or corrupt
 * backup must never leave fdict able to read/write out of bounds. */
void forthDictValidateRestored(void)
{
  if (fdict.base == NULL) {
    /* normalize scalars regardless of what the file said */
    fdict.sizeBlocks = 0;
    fdict.here = 0;
    fdict.latest = FORTH_NULL;
    fdict.count = 0;
    return;
  }

  uint32_t cap = (uint32_t)fdict.sizeBlocks * BYTES_PER_BLOCK;
  bool ok = (fdict.sizeBlocks != 0) && (fdict.here <= cap)
         && (fdict.latest == FORTH_NULL || fdict.latest < fdict.here);

  if (ok) {  /* walk the header chain: offsets must strictly decrease */
    uint16_t off = fdict.latest;
    uint16_t n = 0;
    while (off != FORTH_NULL) {
      if ((uint32_t)off + 4 > fdict.here) { ok = false; break; }
      forthHeader_t *hdr = (forthHeader_t *)(fdict.base + off);
      if (hdr->nameLen == 0 || hdr->nameLen > FORTH_NAME_MAX) { ok = false; break; }
      if (hdr->link != FORTH_NULL && hdr->link >= off) { ok = false; break; }
      off = hdr->link;
      if (++n > fdict.count) { ok = false; break; }
    }
    if (ok && n != fdict.count) {
      ok = false;
    }
  }

  if (!ok) {
#if defined(PC_BUILD)
    printf("forthDictValidateRestored: inconsistent dictionary in backup, resetting\n");
#endif
    /* Deliberate orphan: do NOT freeC47Blocks here — the restored allocation
     * tables are exactly what we just failed to trust (P-4 exception). */
    forthDictInit();
  }
}
```

In `forth_dict.h`, after the `void forthDictClear(void);` prototype (~line
51), add:

```c
/* H5: sanity-check fdict after restoreCalc; resets to empty on corruption. */
void forthDictValidateRestored(void);
```

Run the refresh tool, then the gate. The function is not yet called; the
suite must stay green with the same test count as before.

---

## Q2 — Pillar 1: package patch of `saveRestoreBackup.c`

Read: `src/c47/saveRestoreBackup.c` lines 1-40, 510-570, and 810-860. Nothing
else (the file is ~1470 lines; do not read the rest).

Step 1: `cp src/c47/saveRestoreBackup.c packages/forth-core/saveRestoreBackup.c`.
All edits below go into the **copy**.

Step 2: near the top of the copy, after the last existing `#include` line in
the file's include block, add:

```c
#include "forth_dict.h"
```

Step 3 (save hunk): in `saveCalc`, find this exact pair (~line 526):

```c
    ramPtr = TO_C47MEMPTR(programList);
    saveStateValue(&ramPtr,                         sizeof(ramPtr),                                              "programList",                    "c47Ptr");
```

Immediately AFTER it (before the `currentSubroutineLevelData` pair), insert:

```c
    ramPtr = TO_C47MEMPTR(fdict.base);
    saveStateValue(&ramPtr,                         sizeof(ramPtr),                                              "forthDictBase",                  "c47Ptr");
    saveStateValue(&fdict.sizeBlocks,               sizeof(fdict.sizeBlocks),                                    "forthDictSizeBlocks",            "uint16");
    saveStateValue(&fdict.here,                     sizeof(fdict.here),                                          "forthDictHere",                  "uint16");
    saveStateValue(&fdict.latest,                   sizeof(fdict.latest),                                        "forthDictLatest",                "uint16");
    saveStateValue(&fdict.count,                    sizeof(fdict.count),                                         "forthDictCount",                 "uint16");
```

Step 4 (restore hunk): in `restoreCalc`, find this exact pair (~line 846):

```c
    restoreStateValue(&ramPtr,                         sizeof(ramPtr),                                              "programList",                    "c47Ptr");
    programList = TO_PCMEMPTR(ramPtr);
```

Immediately AFTER it (before the `currentSubroutineLevelData` restore),
insert:

```c
    ramPtr = C47_NULL;   /* default: old backup without Forth params -> empty dict */
    restoreStateValue(&ramPtr,                         sizeof(ramPtr),                                              "forthDictBase",                  "c47Ptr");
    fdict.base = TO_PCMEMPTR(ramPtr);
    {
      uint16_t forthV;
      forthV = 0;          restoreStateValue(&forthV, sizeof(forthV), "forthDictSizeBlocks", "uint16"); fdict.sizeBlocks = forthV;
      forthV = 0;          restoreStateValue(&forthV, sizeof(forthV), "forthDictHere",       "uint16"); fdict.here       = forthV;
      forthV = FORTH_NULL; restoreStateValue(&forthV, sizeof(forthV), "forthDictLatest",     "uint16"); fdict.latest     = forthV;
      forthV = 0;          restoreStateValue(&forthV, sizeof(forthV), "forthDictCount",      "uint16"); fdict.count      = forthV;
    }
    forthDictValidateRestored();
```

The per-call default pre-seeding is load-bearing: a missing parameter leaves
the buffer untouched, and `ramPtr` still holds the previous parameter's value
otherwise. Do not "optimize" it away.

Run the refresh tool, then the gate (green, same test count). Also verify the
refresh classified the file as a patch: `ls packages/forth-core/patches/ |
grep -i saverestore` should show an entry — include that in your report.

---

## Q3 — Pillar 1: self-test run-once guard in `config.c`

Read: `packages/forth-core/config.c` lines 1935-1955. Nothing else (the file
is ~2000 lines).

Find this exact block:

```c
      #if defined(PC_BUILD) && defined(FORTH_DEBUG_SELFTEST)
       extern int forthDictSelfTest(void);
       if(forthDictSelfTest()) {
         fprintf(stderr, "FORTH DICT SELF-TEST FAILED\n");
         exit(1);
       }
       if(headlessMode) {
         exit(0);
       }
     #endif
```

Replace it with:

```c
      #if defined(PC_BUILD) && defined(FORTH_DEBUG_SELFTEST)
       {
         /* Run-once: tests call restoreCalc, which calls doFnReset — without
          * this guard the suite would recursively re-enter itself. */
         static bool forthSelfTestRan = false;
         if(!forthSelfTestRan) {
           forthSelfTestRan = true;
           extern int forthDictSelfTest(void);
           if(forthDictSelfTest()) {
             fprintf(stderr, "FORTH DICT SELF-TEST FAILED\n");
             exit(1);
           }
           if(headlessMode) {
             exit(0);
           }
         }
       }
     #endif
```

Run the refresh tool, then the gate (green, same test count).

---

## Q4 — Pillar 1 tests, part 1: backup-file helpers + round-trip test T1.1

Read: `packages/forth-core/test_dict_reloc.c` lines 1018-1060 (pattern:
`test_outer_simple_expr`, X-register checking via `x_is_longint`), lines
4772-4800 (suite entry `forthDictSelfTest`, `allocRegionsStart` snapshot),
and lines 4870-4920 (the `fail |= test_...()` registration pattern). Also
grep — do not read whole file: `grep -n "x_is_longint\|static bool x_is"
packages/forth-core/test_dict_reloc.c | head` and read that helper's ~20
lines.

Background you need (verified facts, do not re-derive):
- `saveCalc()`/`restoreCalc()` are PC_BUILD state backup functions declared
  via the headers the test file already includes. `restoreCalc()` internally
  calls `doFnReset` (safe now — Q3 guard) and early-returns if the global
  `bool_t loadTestPrograms` is set, so the test must force it to `false`
  around the call.
- The backup file is a text file in the simulator's working directory (the
  repo root when run via the gate): name it with
  `(CALCMODEL == USER_C47 ? "backup.cfg" : "backupR47.cfg")`. Each parameter
  is one line: `name:type:value`.
- `saveCalc()` silently refuses to save under some `calcModel` values; the
  test must therefore assert the file actually contains `forthDictBase:`
  after saving, and FAIL loudly if not.

Step 1 — add three static helpers near the other test-infrastructure helpers
(put them just before the `forthDictSelfTest` function, ~line 4770):

```c
/* ---- Pillar 1 (H5) backup-file helpers ---- */
#define TEST_BACKUP_NAME (CALCMODEL == USER_C47 ? "backup.cfg" : "backupR47.cfg")

static char *savedBackupBytes = NULL;
static long  savedBackupLen   = -1;   /* -1: file did not exist */

static void preserveBackupFile(void)
{
  FILE *f = fopen(TEST_BACKUP_NAME, "rb");
  if (!f) { savedBackupLen = -1; return; }
  fseek(f, 0, SEEK_END);
  savedBackupLen = ftell(f);
  fseek(f, 0, SEEK_SET);
  savedBackupBytes = malloc((size_t)savedBackupLen);
  if (savedBackupBytes) {
    if (fread(savedBackupBytes, 1, (size_t)savedBackupLen, f) != (size_t)savedBackupLen) {
      free(savedBackupBytes); savedBackupBytes = NULL; savedBackupLen = -1;
    }
  }
  fclose(f);
}

static void restoreBackupFile(void)
{
  if (savedBackupLen < 0) { remove(TEST_BACKUP_NAME); return; }
  FILE *f = fopen(TEST_BACKUP_NAME, "wb");
  if (f) {
    fwrite(savedBackupBytes, 1, (size_t)savedBackupLen, f);
    fclose(f);
  }
  free(savedBackupBytes); savedBackupBytes = NULL; savedBackupLen = -1;
}

/* returns 1 if the backup file contains a line starting with `prefix` */
static int backupFileContains(const char *prefix)
{
  FILE *f = fopen(TEST_BACKUP_NAME, "r");
  char line[256];
  int found = 0;
  if (!f) return 0;
  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, prefix, strlen(prefix)) == 0) { found = 1; break; }
  }
  fclose(f);
  return found;
}
```

Step 2 — add test T1.1 right after those helpers:

```c
/* T1.1 (H5 round-trip). Must fail if: any of the five forthDict* params is
 * dropped from the save or restore hunk, or the restore rebases fdict.base
 * without TO_PCMEMPTR. */
static int test_save_restore_roundtrip(void)
{
  int fail = 0;
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": SRW1 7 ;");
  forthOuterInterpret(": SRW2 SRW1 35 ;");
  if (lastErrorCode != ERROR_NONE) {
    printf("    FAIL: setup definitions raised error %d\n", lastErrorCode);
    return 1;
  }
  uint16_t savedHere = fdict.here, savedCount = fdict.count, savedLatest = fdict.latest;

  saveCalc();
  if (!backupFileContains("forthDictBase:")) {
    printf("    FAIL: saveCalc did not write forthDictBase (calcModel guard or missing hunk)\n");
    forthDictClear();
    return 1;
  }

  /* clobber the dictionary so only a real restore can bring it back */
  forthDictClear();
  forthOuterInterpret(": SRZZ 1 ;");

  {
    bool_t savedLoad = loadTestPrograms;
    loadTestPrograms = false;
    restoreCalc();
    loadTestPrograms = savedLoad;
  }

  uint16_t idx;
  if (!forthFindColon("SRW1", &idx)) { printf("    FAIL: SRW1 lost across restore\n"); fail = 1; }
  if (!forthFindColon("SRW2", &idx)) { printf("    FAIL: SRW2 lost across restore\n"); fail = 1; }
  if (forthFindColon("SRZZ", &idx))  { printf("    FAIL: post-save word SRZZ survived restore\n"); fail = 1; }
  if (fdict.here != savedHere || fdict.count != savedCount || fdict.latest != savedLatest) {
    printf("    FAIL: fdict scalars mismatch (here %u/%u count %u/%u latest %u/%u)\n",
           fdict.here, savedHere, fdict.count, savedCount, fdict.latest, savedLatest);
    fail = 1;
  }

  if (!fail && forthFindColon("SRW2", &idx)) {
    lastErrorCode = ERROR_NONE;
    forthInner(idx, false);
    if (lastErrorCode != ERROR_NONE) { printf("    FAIL: SRW2 raised %d\n", lastErrorCode); fail = 1; }
    else if (!x_is_longint(35))      { printf("    FAIL: X != 35 after SRW2\n"); fail = 1; }
  }

  printf("  FORTH ARENA (post-restore): here=%u sizeBlocks=%u\n", fdict.here, fdict.sizeBlocks);
  forthDictClear();   /* balance the suite-level leak gate */
  if (!fail) printf("    PASS: save/restore round-trip preserved the dictionary\n");
  return fail;
}
```

(If `x_is_longint` takes different arguments than shown in the helper you
read, adapt the two call sites to the real signature — nothing else.)

Step 3 — register: inside `forthDictSelfTest`, at the END of the existing
`fail |= ...` sequence (after the last existing test call, before the
suite-end leak-check block), add:

```c
  preserveBackupFile();
  printf("  [DEBUG] running test_save_restore_roundtrip...\n");
  fail |= test_save_restore_roundtrip();
  restoreBackupFile();
```

Run refresh tool + gate. Expect green with ONE more PASS than before.

Mutation verification (mandatory, do all three):
1. In `packages/forth-core/saveRestoreBackup.c`, comment out the
   `"forthDictLatest"` save line → refresh → gate must go RED (SRW1/SRW2
   lookup fails). Revert, refresh.
2. Comment out the `forthDictValidateRestored();` call — gate must stay GREEN
   (T1.1 doesn't cover validation; that's Q5's job — this confirms T1.1 is
   testing the round-trip, not the validator). Revert, refresh.
3. In the restore hunk, change `fdict.base = TO_PCMEMPTR(ramPtr);` to
   `fdict.base = (uint8_t *)(uintptr_t)ramPtr;` → gate must go RED. Revert,
   refresh, final gate GREEN.

Report the red/green result of each mutation.

---

## Q5 — Pillar 1 tests, part 2: T1.2 + T1.3 + T1.4, then commit

Read: your own Q4 additions in `packages/forth-core/test_dict_reloc.c`
(grep for `test_save_restore_roundtrip` and read that region), plus lines
4772-4790 (suite entry).

Step 1 — one more file helper next to `backupFileContains`:

```c
/* Rewrite the backup file: drop lines starting with dropPrefix (may be NULL),
 * and/or replace the whole line starting with replPrefix by replLine (may be
 * NULL). Returns 0 on success. */
static int editBackupFile(const char *dropPrefix, const char *replPrefix, const char *replLine)
{
  FILE *in = fopen(TEST_BACKUP_NAME, "r");
  FILE *out = in ? fopen("backup.tmp.cfg", "w") : NULL;
  char line[4096];
  if (!in || !out) { if (in) fclose(in); return 1; }
  while (fgets(line, sizeof(line), in)) {
    if (dropPrefix && strncmp(line, dropPrefix, strlen(dropPrefix)) == 0) continue;
    if (replPrefix && strncmp(line, replPrefix, strlen(replPrefix)) == 0) {
      fputs(replLine, out);
      continue;
    }
    fputs(line, out);
  }
  fclose(in); fclose(out);
  remove(TEST_BACKUP_NAME);
  return rename("backup.tmp.cfg", TEST_BACKUP_NAME);
}
```

Note: the `ram` hexDump parameter spans very long lines; 4096 may be shorter
than one line. That is fine for this helper ONLY because partial reads of a
non-matching line are copied through verbatim in chunks — `fgets` chunks
never match the short prefixes we filter. Do not shrink the buffer.

Step 2 — T1.2:

```c
/* T1.2 (old-backup defaults). Must fail if: the pre-seeded defaults before
 * each restoreStateValue call are removed (stale ramPtr from the programList
 * restore would masquerade as the dict base). */
static int test_restore_missing_params_defaults(void)
{
  int fail = 0;
  forthDictClear();
  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": OLDW 2 ;");
  if (lastErrorCode != ERROR_NONE) { printf("    FAIL: setup error %d\n", lastErrorCode); return 1; }
  uint8_t *preBase = fdict.base;
  uint16_t preBlocks = fdict.sizeBlocks;

  saveCalc();
  if (!backupFileContains("forthDictBase:")) { printf("    FAIL: save missing params\n"); forthDictClear(); return 1; }
  if (editBackupFile("forthDict", NULL, NULL)) { printf("    FAIL: file edit failed\n"); forthDictClear(); return 1; }

  { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

  if (fdict.base != NULL || fdict.latest != FORTH_NULL || fdict.count != 0
      || fdict.sizeBlocks != 0 || fdict.here != 0) {
    printf("    FAIL: missing params did not default to empty dict\n");
    fail = 1;
  }
  /* The restored arena still carries the pre-save dict region, now orphaned
   * by design (params stripped). Release it to balance the leak gate. */
  if (preBase) freeC47Blocks(preBase, preBlocks);

  lastErrorCode = ERROR_NONE;
  forthOuterInterpret(": NEWW 3 ;");
  { uint16_t idx; if (lastErrorCode != ERROR_NONE || !forthFindColon("NEWW", &idx)) {
      printf("    FAIL: lazy alloc broken after defaulted restore\n"); fail = 1; } }
  forthDictClear();
  if (!fail) printf("    PASS: stripped params default to empty dict\n");
  return fail;
}
```

Step 3 — T1.3 (two corruption variants; note each run of `saveCalc` rewrites
the file, so corrupt fresh each time):

```c
/* T1.3 (validation clamps corruption). Must fail if:
 * forthDictValidateRestored is not called from the restore hunk, or its
 * here-bound / chain-count checks are deleted (next dict write would land
 * out of bounds). */
static int test_restore_validation_clamps(void)
{
  int fail = 0;
  int variant;
  for (variant = 0; variant < 2; variant++) {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VALW 4 ;");
    if (lastErrorCode != ERROR_NONE) { printf("    FAIL: setup error\n"); return 1; }
    uint8_t *preBase = fdict.base;
    uint16_t preBlocks = fdict.sizeBlocks;

    saveCalc();
    if (variant == 0) {
      if (editBackupFile(NULL, "forthDictHere:", "forthDictHere:uint16:65534\n")) { printf("    FAIL: edit\n"); return 1; }
    } else {
      char repl[64];
      sprintf(repl, "forthDictCount:uint16:%u\n", (unsigned)fdict.count + 1);
      if (editBackupFile(NULL, "forthDictCount:", repl)) { printf("    FAIL: edit\n"); return 1; }
    }

    { bool_t s = loadTestPrograms; loadTestPrograms = false; restoreCalc(); loadTestPrograms = s; }

    if (fdict.base != NULL) {
      printf("    FAIL: variant %d: corrupt scalars survived validation\n", variant);
      fail = 1;
      forthDictClear();     /* free whatever it points at, best effort */
    }
    else if (preBase) {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }

    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VOKW 5 ;");
    { uint16_t idx; if (lastErrorCode != ERROR_NONE || !forthFindColon("VOKW", &idx)) {
        printf("    FAIL: variant %d: dict unusable after validation reset\n", variant); fail = 1; } }
    forthDictClear();
  }
  if (!fail) printf("    PASS: corrupt here/count both clamped to empty dict\n");
  return fail;
}
```

Step 4 — T1.4: at the very top of `forthDictSelfTest` (first statements),
add:

```c
  static int suiteEntryCount = 0;
  suiteEntryCount++;
```

and in the suite-end summary region (right where the existing leak/tripwire
checks aggregate into `fail`), add:

```c
  if (suiteEntryCount != 1) {
    printf("  FAIL: suite entered %d times (run-once guard in config.c broken)\n", suiteEntryCount);
    fail = 1;
  }
```

Step 5 — registration: extend the Q4 block so it reads:

```c
  preserveBackupFile();
  printf("  [DEBUG] running test_save_restore_roundtrip...\n");
  fail |= test_save_restore_roundtrip();
  printf("  [DEBUG] running test_restore_missing_params_defaults...\n");
  fail |= test_restore_missing_params_defaults();
  printf("  [DEBUG] running test_restore_validation_clamps...\n");
  fail |= test_restore_validation_clamps();
  restoreBackupFile();
```

Run refresh + gate: green, three more PASS lines than after Q4... (T1.2,
T1.3; T1.4 has no PASS line of its own).

Mutation verification (mandatory):
1. Remove `forthDictValidateRestored();` from the restore hunk in
   `packages/forth-core/saveRestoreBackup.c` → gate RED (T1.3). Revert.
2. In `forthDictValidateRestored`, delete the `if ((uint32_t)off + 4 >
   fdict.here)` check → gate must go RED on the count variant (or, if it
   stays green, report that — do not silently accept). Revert.
3. In the restore hunk, delete the `ramPtr = C47_NULL;` pre-seed line → gate
   RED (T1.2). Revert. Final gate GREEN.

Step 6 — commit (exactly this, nothing else staged):

```
git add packages/forth-core/forth_dict.c packages/forth-core/forth_dict.h \
        packages/forth-core/saveRestoreBackup.c packages/forth-core/config.c \
        packages/forth-core/test_dict_reloc.c \
        packages/forth-core/patches packages/forth-core/files \
        packages/forth-core/.refresh-manifest.json
git commit -m "forth-core: P1 save/restore integration (H5)

Serialize fdict via the backup.cfg name-keyed parameter list, rebase
fdict.base with the c47Ptr idiom, validate on restore, guard the
self-test against recursive doFnReset. Tests T1.1-T1.4."
```

Report `git show --stat HEAD`.

---

## Q5b — Pillar 1 follow-up: pin the unpinned validator checks

Context (architect's ruling on the Q5 mutation report): mutations 2 and 3
stayed GREEN because of overlapping defenses, and that is ACCEPTED for the
checks that are structurally shadowed — but two validator checks turned out
to be reachable only through in-memory corruption, which no current test
exercises. This task adds one direct unit test to pin them, and documents
the declared-redundant checks so nobody "cleans them up" later.

Read: `packages/forth-core/forth_dict.c` (the `forthDictValidateRestored`
function only) and your T1.3 test in `packages/forth-core/test_dict_reloc.c`
(grep `test_restore_validation_clamps`).

Step 1 — in `forthDictValidateRestored`, update two comments (no logic
changes):
- On the line `bool ok = (fdict.sizeBlocks != 0) && ...` add above it:
  ```c
  /* Pinned by tests: sizeBlocks!=0 (T1.3b V1), here<=cap (T1.3 v1),
   * nameLen bounds (T1.3b V2), n==count (T1.3 v2).
   * Declared redundant (termination/robustness, shadowed by the checks
   * above — do not remove without re-running the mutation analysis):
   * latest<here (shadowed by walk's off+4 bound), off+4>here vs OOB reads,
   * link strictly-decreasing (cycles are bounded by the n>count cap). */
  ```

Step 2 — add test T1.3b next to `test_restore_validation_clamps` (no file
I/O; direct in-memory corruption; does NOT call restoreCalc):

```c
/* T1.3b (validator direct pins). V1 must fail if the sizeBlocks!=0 check is
 * removed (stale base + zeroed scalars passes every other check: cap=0,
 * here=0<=0, latest=FORTH_NULL skips the walk, n=0==count).
 * V2 must fail if the nameLen bounds check is removed (a zeroed nameLen
 * header walks clean through the off/link/count checks). */
static int test_validate_direct_corruption(void)
{
  int fail = 0;

  /* V1: stale base with zeroed scalars */
  {
    uint8_t *region = allocC47Blocks(4);
    if (!region) { printf("    SKIP: alloc failed\n"); return 0; }
    forthDictClear();
    fdict.base = region;             /* simulate stale-pointer restore */
    fdict.sizeBlocks = 0;
    fdict.here = 0;
    fdict.latest = FORTH_NULL;
    fdict.count = 0;
    forthDictValidateRestored();
    if (fdict.base != NULL) {
      printf("    FAIL: V1 stale base with zeroed scalars survived validation\n");
      fail = 1;
      forthDictClear();              /* best effort */
    }
    freeC47Blocks(region, 4);        /* release the deliberate orphan */
  }

  /* V2: corrupt nameLen on a real header */
  {
    forthDictClear();
    lastErrorCode = ERROR_NONE;
    forthOuterInterpret(": VD2 1 ;");
    if (lastErrorCode != ERROR_NONE || !fdict.base) {
      printf("    SKIP: V2 setup failed\n");
      return fail;
    }
    uint8_t *preBase = fdict.base;
    uint16_t preBlocks = fdict.sizeBlocks;
    ((forthHeader_t *)(fdict.base + fdict.latest))->nameLen = 0;
    forthDictValidateRestored();
    if (fdict.base != NULL) {
      printf("    FAIL: V2 zero-nameLen header survived validation\n");
      fail = 1;
      forthDictClear();
    }
    else {
      freeC47Blocks(preBase, preBlocks);  /* release the deliberate orphan */
    }
  }

  forthDictClear();
  if (!fail) printf("    PASS: validator direct pins (sizeBlocks, nameLen)\n");
  return fail;
}
```

Step 3 — register it inside the existing Pillar-1 block (after
`test_restore_validation_clamps`, still between `preserveBackupFile()` and
`restoreBackupFile()` — it needs no file, but keep the group contiguous):

```c
  printf("  [DEBUG] running test_validate_direct_corruption...\n");
  fail |= test_validate_direct_corruption();
```

Refresh + gate: green, +1 PASS.

Mutation verification (mandatory):
1. Delete `(fdict.sizeBlocks != 0) &&` from the validator → gate RED via V1.
   Revert.
2. Delete the `hdr->nameLen == 0 || hdr->nameLen > FORTH_NAME_MAX` check →
   gate RED via V2. Revert. Final gate GREEN.

Step 4 — commit (a separate small commit; also carries the gate script fix
that predates this series and must be in history for the suite to be
non-vacuous on a fresh checkout):

```
git add packages/forth-core/forth_dict.c packages/forth-core/test_dict_reloc.c \
        packages/forth-core/build-test.sh \
        packages/forth-core/patches packages/forth-core/files \
        packages/forth-core/.refresh-manifest.json
git commit -m "forth-core: P1 validator hardening + commit gate selftest fix

Pin the two validator checks unreachable via backup-file corruption
(sizeBlocks!=0, nameLen bounds) with a direct in-memory test; document
the declared-redundant checks. Commit the build-test.sh change that
injects FORTH_DEBUG_SELFTEST (previously uncommitted: a fresh checkout
would run a vacuous-green gate)."
```

Report `git show --stat HEAD` and the mutation results.

---

## Q6 — Pillar 3: open-definition snapshot helpers

Read: `packages/forth-core/forth_dict.c` lines 185-200 (the `openDef` static)
and `packages/forth-core/forth_dict.h` lines 74-100.

In `forth_dict.c`, immediately after the `static struct { ... } openDef;`
line (~189), add:

```c
/* §3.2 re-entrancy (D-3): snapshot/restore openDef so a nested interpret can
 * never finish or abort the outer line's definition. */
void forthDefStateSave(forthDefState_t *out)
{
  out->here = openDef.here;
  out->latest = openDef.latest;
  out->count = openDef.count;
  out->entryOff = openDef.entryOff;
  out->open = openDef.open;
}

void forthDefStateRestore(const forthDefState_t *in)
{
  openDef.here = in->here;
  openDef.latest = in->latest;
  openDef.count = in->count;
  openDef.entryOff = in->entryOff;
  openDef.open = in->open;
}
```

In `forth_dict.h`, after the `abortDefinition` prototype (~line 84), add:

```c
/* Open-definition snapshot for nested interprets (§3.2 re-entrancy) */
typedef struct { uint16_t here, latest, count, entryOff; bool open; } forthDefState_t;
void forthDefStateSave(forthDefState_t *out);
void forthDefStateRestore(const forthDefState_t *in);
```

Refresh + gate: green, same test count.

---

## Q7 — Pillar 3: inner interpreter depth + rsp watermark

Read: `packages/forth-core/forth_inner.c` IN FULL (394 lines). Also
`packages/forth-core/forth_dict.h` lines 142-151, and
`packages/forth-core/test_dict_reloc.c` lines 525-565
(`test_c47_nested_reentry`) and 1230-1290 (`test_reentrancy`).

### 7a — forth_inner.c

1. Replace `static bool forthRunning = false;` (line 28) with:
   ```c
   #define FORTH_NEST_MAX 4
   static uint8_t forthDepth = 0;   /* nested forthInner invocations */
   ```
2. Replace the re-entrancy guard block (lines 160-166):
   ```c
   /* Re-entrancy (§3.2, D-3): bounded nesting; rstack shared via watermark */
   if (forthDepth >= FORTH_NEST_MAX) {
     lastErrorCode = ERROR_OPERATION_UNDEFINED;   /* C-12: same code as old guard */
     displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,
                             ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     return;
   }
   forthDepth++;
   ```
3. Replace `rsp = 0;` (line 178) with nothing there; instead, immediately
   after the `forthDepth++;` line add:
   ```c
   uint8_t rspBase = rsp;   /* watermark: this level's rstack floor */
   ```
   (It must be set before the `bodyOffsetOfIndex` bad-entry return below can
   use it.)
4. Define, right after the `rspBase` line:
   ```c
   #define INNER_LEAVE() do { rsp = rspBase; forthDepth--; return; } while (0)
   ```
   and `#undef INNER_LEAVE` at the very end of the function body.
5. Replace EVERY `forthRunning = false; return;` pair in the function with
   `INNER_LEAVE();` — there are these sites: bad-entry (~174), DMCP key-poll
   (~189), runaway cap (~199), bad prim (~227), prim error (~233), FTOK_CALL
   bad body (~255), FTOK_LIT error (~269), FTOK_ILIT error (~283), FTOK_0BR
   error (~309), FTOK_C47 unsupported-PTP (~354), FTOK_C47 error (~371),
   default/corrupt token (~381). Count them: if you do not find exactly 12,
   STOP and report.
6. `FTOK_EXIT` arm (lines 209-218): change `if (rsp == 0)` to
   `if (rsp == rspBase)`, and inside that branch replace
   `forthRunning = false;` with `forthDepth--;` (keep the ASLIFT set and the
   `return`).
7. The final `forthRunning = false;` after the loop (line 386, reached by
   the cooperative break) becomes:
   ```c
   rsp = rspBase;
   forthDepth--;
   ```
8. Test hooks (lines 389-393): replace with:
   ```c
   /* Test-only: prime/read nesting depth for guard tests (§3.2) */
   #ifdef FORTH_DEBUG_SELFTEST
   void forthTestSetDepth(uint8_t d) { forthDepth = d; }
   uint8_t forthTestGetDepth(void) { return forthDepth; }
   #endif
   ```

### 7b — forth_dict.h

Replace the two hook prototypes (lines 146-149) with:

```c
/* Test-only: prime/read forthInner nesting depth (§3.2) */
#ifdef FORTH_DEBUG_SELFTEST
void forthTestSetDepth(uint8_t d);
uint8_t forthTestGetDepth(void);
#endif
```

### 7c — update the two existing tests (behavior legitimately changed)

`test_reentrancy` (~1239): this test primed the old boolean guard. Rework it
to prime the depth cap — replace `forthTestSetRunning(true);` with
`forthTestSetDepth(FORTH_NEST_MAX);`, replace `forthTestSetRunning(false);`
with `forthTestSetDepth(0);`, and replace the `if (forthTestIsRunning())`
check with `if (forthTestGetDepth() != 0)`. Keep all its error-code
assertions (the cap still raises ERROR_OPERATION_UNDEFINED). Update the
test's comment header: it now verifies the DEPTH CAP fires at
FORTH_NEST_MAX and recovers. Also fix the stale mention at file line ~17.

`test_c47_nested_reentry` (~525): nesting is now LEGAL at depth 2, so invert
the expectations. After `run_word("NR")`: expect NO error
(`err == false && lastErrorCode == ERROR_NONE`), expect `x_is_longint(999)`
true (the sentinel ILIT after the nested call DID execute), and add a check
that the nested word's 42 landed beneath it (use the suite's existing
pattern for checking Y if one exists near `x_is_longint`; if none exists,
drop the Y check and say so in your report). Rename the test
`test_c47_nested_call_succeeds`, update its `[DEBUG] running` line and
`fail |=` call site (~grep for `test_c47_nested_reentry` to find both), and
rewrite its comment: "P3: nested forthInner via ITM_FCALL now succeeds
(depth 2 <= FORTH_NEST_MAX). Must fail if the depth upgrade regresses to a
single-level guard, or if the nested return corrupts the outer ip."

Refresh + gate: green, same PASS count (two tests changed, none added).

Mutation verification (mandatory):
1. Reintroduce `rsp = 0;` in place of `uint8_t rspBase = rsp;` (and make
   `rspBase` a constant 0) → gate must go RED (nested-call test corrupts the
   outer return chain... if it stays green, report it — Q9's T3.2 will pin
   this; do not skip the check). Revert.
2. Change `FORTH_NEST_MAX` to 1 → gate RED (`test_c47_nested_call_succeeds`
   errors). Revert. Final gate GREEN.

---

## Q8 — Pillar 3: outer interpreter context model

Read: `packages/forth-core/forth_compile.c` IN FULL (405 lines) and
`packages/forth-core/forth_dict.h` lines 113-151.

Rewrite `forth_compile.c` state handling as follows. Everything else in the
file (number grammar, the interpret-loop arm logic) stays byte-identical
except where named.

1. Delete lines 22-24 (`forthSource`, `forthOuterActive`) and lines 44-45
   (`tokenizerSource`, `tokenizerPos`). Keep the `FORTH_SOURCE_MAX` define.
2. Add in their place:
   ```c
   /* ---- §3.3.2 / D-3: per-invocation context; idle BSS = one ptr + 2 bytes ---- */
   typedef enum {
     FORTH_OUTER_FULL = 0          /* compile and execute (interactive semantics) */
     /* P2 adds DEFS_ONLY / SKIP_DEFS */
   } forthOuterMode_t;

   typedef struct {
     char            source[FORTH_SOURCE_MAX];
     int16_t         pos;          /* tokenizer position */
     forthDefState_t savedDef;     /* outer level's open-definition snapshot */
   } forthOuterCtx_t;

   #define FORTH_OUTER_NEST_MAX 2
   static forthOuterCtx_t *forthOuterCur   = NULL;
   static uint8_t          forthOuterDepth = 0;
   ```
3. Tokenizer: `forthTokenizerInit` becomes `{ forthOuterCur->pos = 0; }`
   (no argument). In `nextToken`, replace every `tokenizerSource` with
   `forthOuterCur->source` and every `tokenizerPos` with
   `forthOuterCur->pos`.
4. Rename the current `forthOuterInterpret` function to
   ```c
   static void forthOuterRun(forthOuterCtx_t *ctx, forthOuterMode_t mode)
   ```
   with this prologue replacing the old first lines (`state`,
   `lineOK`, `buf` declarations stay):
   ```c
   (void)mode;   /* P2 uses it */
   if (forthOuterDepth >= FORTH_OUTER_NEST_MAX) {
     displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     return;
   }
   forthOuterCtx_t *prevCtx = forthOuterCur;
   forthOuterCur = ctx;
   forthOuterDepth++;
   forthDefStateSave(&ctx->savedDef);
   forthTokenizerInit();
   ```
   and this epilogue as the function's last lines (after the existing
   end-of-line handling):
   ```c
   forthDefStateRestore(&ctx->savedDef);
   forthOuterDepth--;
   forthOuterCur = prevCtx;
   ```
   Verify the body has no `return` statements between prologue and epilogue
   (it uses `lineOK` fall-through) — if you find one, STOP and report.
   Where the old body called `forthTokenizerInit(source)`, it now calls
   `forthTokenizerInit()`; there is no `source` parameter anymore.
5. New public wrapper (same name/signature as the old public function, so
   `forth_dict.h:134` and all tests keep working):
   ```c
   void forthOuterInterpret(const char *source)
   {
     forthOuterCtx_t ctx;
     size_t n = strlen(source);
     if (n >= FORTH_SOURCE_MAX) {
       displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       return;
     }
     memcpy(ctx.source, source, n + 1);
     forthOuterRun(&ctx, FORTH_OUTER_FULL);
   }
   ```
6. `fnForthOuter` rewrite: delete the `forthOuterActive` guard (lines
   369-372); keep the dtString check and the length check exactly as they
   are; replace the tail (old lines 382-386) with:
   ```c
   forthOuterCtx_t ctx;
   xcopy(ctx.source, REGISTER_STRING_DATA(REGISTER_X), len + 1);
   fnDrop(NOPARAM);   /* copy MUST precede drop: drop invalidates the string */
   forthOuterRun(&ctx, FORTH_OUTER_FULL);
   ```
7. `forthProgramStep` rewrite (interim; P2 revises it):
   ```c
   void forthProgramStep(const uint8_t *payload)
   {
     forthRunGenCheckReset();
     forthOuterCtx_t ctx;
     uint8_t len = *payload;
     xcopy(ctx.source, payload + 1, len);
     ctx.source[len] = 0;
     forthOuterRun(&ctx, FORTH_OUTER_FULL);
   }
   ```
   (The old `forthOuterActive` early-out is gone: nesting is legal to the
   cap. Note the generation check moved BEFORE the copy — keep that order.)
8. Test hooks at the end of the file:
   ```c
   /* Test-only: outer-interpreter nesting introspection (D-3) */
   #ifdef FORTH_DEBUG_SELFTEST
   void *forthTestOuterCur(void) { return (void *)forthOuterCur; }
   uint8_t forthTestOuterDepth(void) { return forthOuterDepth; }
   #endif
   ```
   And in `forth_dict.h` next to the Q7 hooks:
   ```c
   #ifdef FORTH_DEBUG_SELFTEST
   void *forthTestOuterCur(void);
   uint8_t forthTestOuterDepth(void);
   #endif
   ```

Refresh + gate: green, same PASS count. Report the BSS delta:
`nm --size-sort -S build.sim/src/c47-gtk/c47 2>/dev/null | grep -i
"forthsource\|forthouter\|tokenizer"` should show the old statics gone; also
report `size` of the binary before/after if easy, else skip.

---

## Q9 — Pillar 3 tests, part 1 (T3.2, T3.4)

(T3.1's success path and T3.3's cap/recovery landed in Q7's reworked tests.)

Read: `packages/forth-core/test_dict_reloc.c` lines 525-565 (the Q7-reworked
nested-call test — copy its FCALL-embedding pattern), plus the ~30 lines
around `begin_word`/`end_word`/`run_word`/`emit_int32` definitions (grep
`-n "static uint16_t begin_word\|static void end_word\|static bool
run_word\|static void emit_int32"` and read those).

Add two tests after the nested-call test:

T3.2 — watermark protects the outer return chain:

```c
/* T3.2 (D-3 core): nested forthInner fires while the OUTER level has rsp > 0.
 * Must fail if forthInner still zeroes rsp on entry (outer return chain
 * destroyed: TOP's tail after MID never runs). */
static int test_nested_preserves_outer_rstack(void)
```

Structure (use the suite's emit helpers exactly like the nested-call test):
- Word `WMLEAF`: ILIT(1), EXIT.
- Word `WMNEST`: ILIT(42), EXIT — its index `nestIdx`.
- Word `WMMID`: CALL(WMLEAF) — emit token `(uint16_t)(0x1000 + leafIdx)` —
  then FTOK_C47 + itemId 2843 (ITM_FCALL) + `nestIdx`, then CALL(WMLEAF)
  again, then EXIT.
- Word `WMTOP`: CALL(WMMID), ILIT(9), EXIT.
- `run_word("WMTOP")` with `programRunStop` forced to `PGM_RUNNING` around it
  (copy the pattern from `test_xeq_end_to_end` ~line 1170).
- Assert: no error; `x_is_longint(9)` (TOP's tail ran — the exact thing a
  zeroed rsp kills); `forthTestGetDepth() == 0` after.

T3.4 — error unwind restores the watermark:

```c
/* T3.4: an error deep in a nest must unwind rsp to each level's watermark.
 * Must fail if any error path in forthInner forgets rsp = rspBase (a stale
 * rstack entry makes the follow-up word's EXIT pop a garbage ip). */
static int test_nested_error_unwinds_rsp(void)
```

Structure:
- Word `UWBAD`: emit token `(uint16_t)(0x1000 + 200)` (a CALL to a
  nonexistent index → ERROR_INVALID_CORRUPTED_DATA at dispatch), EXIT.
- Word `UWMID`: CALL(UWBAD) wrapped so `rsp > 0` when the bad call fires,
  EXIT.
- Run `UWMID`; assert an error was raised.
- Then: `lastErrorCode = ERROR_NONE;` and run a fresh trivial word
  `UWOK`: ILIT(6), EXIT. Assert no error, `x_is_longint(6)`, and
  `forthTestGetDepth() == 0`.

Register both with the standard `[DEBUG] running` + `fail |=` pattern at the
end of the run list (before the Pillar-1 backup block is fine — order among
test groups is not significant; just keep each group contiguous).

Refresh + gate: green, two more PASS lines.

Mutation verification (mandatory):
1. In `forth_inner.c`, change the FTOK_CALL bad-body error path's
   `INNER_LEAVE()` to `do { forthDepth--; return; } while(0)` (drop the rsp
   restore) → gate RED via T3.4. Revert.
2. Change `uint8_t rspBase = rsp;` to `uint8_t rspBase = 0;` → gate RED via
   T3.2. Revert. Final gate GREEN.

---

## Q9b — Pillar 3 follow-up: make rstack leaks observable

Context (architect's ruling on the Q9 mutation report): mutation 1 survived
because the watermark design silently ABSORBS a leaked rstack entry — the
next invocation's `rspBase = rsp` just treats it as floor, and the only
behavioral consequence (capacity loss, one entry per leak) is invisible to
T3.4. The report's root-cause analysis was correct. The fix is direct
observability: a test hook for `rsp`, plus asserting `rsp == 0` at rest,
which pins the restore in EVERY error path at once.

Read: `packages/forth-core/forth_inner.c` lines 155-260 and 375-385, and
your Q9 tests in `packages/forth-core/test_dict_reloc.c` (grep
`test_nested_preserves_outer_rstack` and `test_nested_error_unwinds_rsp`).

Step 1 — `forth_inner.c`: in the FTOK_CALL bad-body error path there is a
lone `rsp--;` (line ~245) immediately before the error handling that ends in
`INNER_LEAVE();`. Delete that `rsp--;` line — `INNER_LEAVE()`'s
`rsp = rspBase` subsumes it, and keeping two restore mechanisms in one path
is what masked the Q9 mutation. (Verify the path still ends in
`INNER_LEAVE();` — if the structure differs from this description, STOP and
report.)

Step 2 — `forth_inner.c`, in the `FORTH_DEBUG_SELFTEST` hook block at the
end of the file, add:

```c
uint8_t forthTestGetRsp(void) { return rsp; }
```

and in `forth_dict.h`, next to `forthTestGetDepth`:

```c
uint8_t forthTestGetRsp(void);
```

Step 3 — extend the two Q9 tests:
- In `test_nested_error_unwinds_rsp` (T3.4), immediately after the assertion
  that running `UWMID` raised an error (and BEFORE running `UWOK`), add:
  ```c
  if (forthTestGetRsp() != 0) {
    printf("    FAIL: rsp=%u leaked after error unwind (watermark restore missing)\n",
           forthTestGetRsp());
    fail = 1;
  }
  ```
  Update the test's header comment: the rsp-at-rest assertion is the direct
  pin; the UWOK follow-up run remains as the behavioral smoke check.
- In `test_nested_preserves_outer_rstack` (T3.2), add the same
  `forthTestGetRsp() == 0` assertion at the end (success paths must balance
  too).

Refresh + gate: green, same PASS count.

Mutation verification (mandatory — this re-runs Q9's failed mutation with
the new pin):
1. In the FTOK_CALL bad-body error path, replace `INNER_LEAVE();` with
   `do { forthDepth--; return; } while (0);` → gate must now go RED via
   T3.4's rsp assertion. Revert.
2. Change the `INNER_LEAVE` macro itself to drop the `rsp = rspBase;` →
   gate RED (same assertion, proving the pin covers every error path).
   Revert. Final gate GREEN.

No commit — Q10's Pillar 3 commit picks these files up.

---

## Q10 — Pillar 3 tests, part 2 (T3.5–T3.7), then commit

> **STATUS: COMPLETED BY ARCHITECT (2026-07-13) — do not re-run.** Qwen's
> Q10 attempt hit three distinct defects, debugged and fixed directly:
> (1) test encoding: RTN steps were written as `0x04, 0xFD` — ITM_RTN is
> PTP_NONE, single byte; the stray 0xFD desynced the step walk and crashed
> `scanLabelsAndPrograms` (layout-dependent segfault). (2) engine: the
> §3.3.6 label arm's DECIDED PGM_RUNNING wrap made interactive label XEQ a
> silent no-op leaking a subroutine level — replaced with `dynamicMenuItem
> = -1` + direct `fnExecute(label)`; amendment in PROPOSED_SPEC_CHANGES.md.
> (3) harness: `dynamicMenuItem` was 0 at reset, hijacking `fnGoto` into
> menu-step semantics. T3.6 was rewritten (outer depth >2 is unreachable by
> construction — continuation semantics — so the cap is pinned via a new
> `forthTestSetOuterDepth` hook; the two-program construct now tests
> continuation XEQ). Gate green (113 PASS), both mutations verified RED,
> committed as `forth-core: P3 full re-entrancy for both interpreters`.

Read: `packages/forth-core/test_dict_reloc.c` lines 2280-2340
(`writeTestProgram` + `cleanupTestProgram` — read the actual helper bodies),
lines 2405-2470 (a marker/source-step program byte-array example, including
the `0x8B, 0x1A, 0xFD, len, ...` ITM_FORTH encoding), and lines 1288-1370
(`test_xeq_precedence` — check whether it builds a NAMED LABEL program; if it
does, copy that encoding; if it does not, grep the file for `ITM_LBL` and
read the first hit's context).

Facts: an ITM_FORTH source step is `0x8B, 0x1A, 0xFD, <len>, <len bytes>`.
A step opcode with item id < 128 is a single byte (e.g. `0x4C` = ITM_sin).
Find `ITM_LBL`'s numeric id via `grep -n "ITM_LBL \|ITM_LBL=" src/c47/items.h
| head` (or the generated items header the tests use — follow how the test
file resolves other `ITM_` ids). A global named label step is
`[opcode][0xFD][nameLen][name bytes]`. `ITM_RTN` likewise via grep.

T3.5 — outer nesting preserves the tokenizer:

```c
/* T3.5 (D-3): a Forth line XEQs a label whose program contains a Forth
 * source step (outer-in-outer). The OUTER line's remaining tokens must still
 * be consumed after the nested line. Must fail if tokenizer state is shared
 * statics (nested init clobbers the outer position). */
static int test_outer_nesting_tokenizer(void)
```

- Build with `writeTestProgram`: `LBL 'NLB'` + ITM_FORTH source step `"3"`
  (bytes `0x8B,0x1A,0xFD,0x01,'3'`) + `RTN`.
- `forthRunGenBump();` then `lastErrorCode = ERROR_NONE;`
- `forthOuterInterpret("NLB 5");` — the label arm dispatches
  `reallyRunFunction(ITM_XEQ, label)`, runs the program, which nests the
  outer interpreter for `"3"`.
- Assert: no error; `x_is_longint(5)` (outer tail token consumed AFTER the
  nested run) and the `3` beneath it if a Y-check helper exists.
- `cleanupTestProgram();`
- If the label-XEQ dispatch does not work in the harness (e.g. label not
  found after writeTestProgram), STOP and report exactly what
  `findNamedLabel("NLB")` returned — do not work around it.

T3.6 — outer depth cap:

```c
/* T3.6: third-level outer nesting must fail with C-12 and unwind cleanly.
 * Must fail if forthOuterDepth is unbounded or not restored on the error
 * path (every later FORTH line would be locked out). */
static int test_outer_depth_cap(void)
```

- Reuse the T3.5 program (label `NLB`). Interpret the line `"NLB"` from a
  context that is ALREADY two levels deep — simplest faithful construction:
  temporarily prime with the test hook pattern: there is no outer SetDepth
  hook, so instead build program `LBL 'NL2'` + Forth step `"NLB"` + RTN in
  the same program memory write (two labels, one program is fine), and run
  `forthOuterInterpret("NL2");` → level 1 = the typed line, level 2 = NL2's
  Forth step `"NLB"`, level 3 = NLB's Forth step `"3"` → the third level
  must raise ERROR_OPERATION_UNDEFINED.
- Assert: `lastErrorCode == ERROR_OPERATION_UNDEFINED`,
  `forthTestOuterDepth() == 0` afterwards, and a follow-up
  `forthOuterInterpret("11")` succeeds with `x_is_longint(11)`.

T3.7 — no dangling context:

```c
/* T3.7: after any nesting episode, forthOuterCur must be NULL at rest.
 * Must fail if an exit path restores depth but not the ctx pointer
 * (use-after-return into a dead stack frame on the next line). */
static int test_outer_ctx_at_rest(void)
```

- Run `forthOuterInterpret("1 2 +");` then assert
  `forthTestOuterCur() == NULL` and `forthTestOuterDepth() == 0`.
- Re-run the T3.5 nested scenario, then assert both again.

Register all three (contiguous group). Refresh + gate: green, +3 PASS.

Mutation verification (mandatory):
1. In `forthOuterRun`'s epilogue, change `forthOuterCur = prevCtx;` to
   `forthOuterCur = NULL;` → T3.5 must go RED (outer line loses its context
   mid-line after the nested return) — if it stays green, report why before
   reverting. Revert.
2. Delete the depth-cap check in `forthOuterRun` → T3.6 goes RED (no C-12).
   Revert. Final gate GREEN.

Commit:

```
git add packages/forth-core/forth_inner.c packages/forth-core/forth_compile.c \
        packages/forth-core/forth_dict.c packages/forth-core/forth_dict.h \
        packages/forth-core/test_dict_reloc.c \
        packages/forth-core/patches packages/forth-core/files \
        packages/forth-core/.refresh-manifest.json
git commit -m "forth-core: P3 full re-entrancy for both interpreters

Inner: depth counter (cap 4) + rsp watermark, unwind on all exits.
Outer: per-invocation stack context chained through one static pointer
(cap 2), openDef snapshot, ~255B idle BSS reclaimed. C-12 error code
preserved at both caps. Tests T3.1-T3.7."
```

Report `git show --stat HEAD` and the gate tail.

---

## Q11 — Pillar 2: owning-program helpers in the bridge

Read: `packages/forth-core/forth_bridge.c` IN FULL (114 lines) and
`packages/forth-core/forth_dict.h` lines 130-145.

1. In `forth_bridge.c`, above `forthMarkerTurnsOn`, add:

```c
/* §9.2: start of the program containing ptr (largest instructionPointer <= ptr),
 * or NULL if the program list is empty / ptr precedes all programs. */
uint8_t *forthOwningProgramStart(const uint8_t *ptr)
{
  uint8_t *progStart = NULL;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    if (programList[i].instructionPointer <= ptr) {
      progStart = programList[i].instructionPointer;
    }
  }
  return progStart;
}

/* §9.2: start of the next program after progStart (smallest
 * instructionPointer strictly greater), or NULL if progStart is last. */
uint8_t *forthNextProgramStart(const uint8_t *progStart)
{
  uint8_t *nextStart = NULL;
  for (uint16_t i = 0; i < numberOfPrograms; i++) {
    uint8_t *ip = programList[i].instructionPointer;
    if (ip > progStart && (nextStart == NULL || ip < nextStart)) {
      nextStart = ip;
    }
  }
  return nextStart;
}
```

2. In `forthMarkerTurnsOn`, replace its inline walk (the `progStart` loop,
   lines 37-43) with `uint8_t *progStart = forthOwningProgramStart(markerStep);`.
3. In `forthEntryStateAtInsertion`, replace its identical inline walk (lines
   93-98) with `uint8_t *progStart = forthOwningProgramStart((const uint8_t *)currentStep);`
   keeping the following `if (!progStart || progStart >= currentStep)` line
   unchanged.
4. In `forth_dict.h`, next to the §9.4 helper prototypes (~line 137), add:

```c
uint8_t *forthOwningProgramStart(const uint8_t *ptr);
uint8_t *forthNextProgramStart(const uint8_t *progStart);
```

Refresh + gate: green, same PASS count (behavior-identical refactor — the
existing §9.4 marker tests are the regression net).

---

## Q12 — Pillar 2: interpreter modes DEFS_ONLY / SKIP_DEFS + scan list

Read: `packages/forth-core/forth_compile.c` IN FULL.

1. Extend the mode enum:
   ```c
   typedef enum {
     FORTH_OUTER_FULL      = 0,  /* compile and execute (interactive semantics) */
     FORTH_OUTER_DEFS_ONLY = 1,  /* pre-scan: compile definitions, skip ALL interpret-state tokens */
     FORTH_OUTER_SKIP_DEFS = 2   /* step execution: skip ':'..';' regions, execute the rest */
   } forthOuterMode_t;
   ```
   Remove the `(void)mode;` line in `forthOuterRun`.
2. Scanned-programs list, next to the generation counters:
   ```c
   /* §9.2 first-touch pre-scan tracking (reset with the dictionary) */
   #define FORTH_SCAN_MAX 8
   static const uint8_t *forthScannedProgs[FORTH_SCAN_MAX];
   static uint8_t        forthScannedCount = 0;
   ```
   In `forthRunGenCheckReset`, inside the `if`, after `forthDictClear();`,
   add `forthScannedCount = 0;`.
3. Mode behavior — exactly three insertions in `forthOuterRun`'s loop:

   (a) In the `':'` arm: the arm currently starts with
   `if (compareString(buf, ":", CMP_BINARY) == 0) {`. Make its body
   mode-dependent: when `mode == FORTH_OUTER_SKIP_DEFS`, instead of the
   existing body run:
   ```c
   /* SKIP_DEFS (D-2b): definition was compiled by the pre-scan — consume
    * ':' <name> ... ';' without touching the dictionary. */
   char name[FORTH_TOKEN_MAX + 1];
   if (!nextToken(name)) {
     displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
     lineOK = false;
   } else {
     bool closed = false;
     while (nextToken(buf)) {
       if (strcmp(buf, ";") == 0) { closed = true; break; }
     }
     if (!closed) {   /* defensive: pre-scan already errored such a step */
       displayCalcErrorMessage(ERROR_INVALID_NAME, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
       lineOK = false;
     }
   }
   continue;
   ```
   `FULL` and `DEFS_ONLY` keep the existing body verbatim (DEFS_ONLY *does*
   compile).

   (b) In the `';'` arm's interpret-state branch (currently: error +
   `lineOK = false`): when `mode == FORTH_OUTER_DEFS_ONLY`, `continue;`
   silently instead (the error belongs to execution time).

   (c) Immediately BEFORE the primitive-lookup block (the comment
   `/* ---- §4.1 step 1: primitive lookup ---- */`), insert:
   ```c
   if (mode == FORTH_OUTER_DEFS_ONLY && state == STATE_INTERPRET) {
     continue;   /* D-2a: pre-scan must not execute tail code */
   }
   ```
   This single gate suppresses primitive execution, colon-word execution,
   number pushes, label XEQ, and the undefined-word error during pre-scan.
   Compile-state behavior (including immediate primitives, which must run
   while compiling) is untouched.

No caller passes the new modes yet. Refresh + gate: green, same PASS count.

---

## Q13 — Pillar 2: the pre-scan + final forthProgramStep

Read: `packages/forth-core/forth_compile.c` (your Q12 state: the mode enum,
scan list, `forthOuterRun`, `forthProgramStep`) and
`packages/forth-core/forth_bridge.c` lines 18-30 (`forthStepPayload`).

Byte-layout facts (verified; a step `S` is an ITM_FORTH source step):
`S[0]=0x8B, S[1]=0x1A` (opcode), `S[2]=0xFD` (STRING_LABEL_VARIABLE),
`S[3]=len`, source bytes at `S+4`. The dispatcher hands `forthProgramStep` a
pointer to `S+3` (so `*payload == len`, bytes at `payload+1`).

1. Add above `forthProgramStep`:

```c
/* §9.2 Architecture 2: first-touch pre-scan of the owning program.
 * DEFS_ONLY-compiles every Forth source step so forward references from any
 * step's tail resolve. D-2: (a) no tail execution, (b) no recompile,
 * (c) owning program only. */
static void forthPreScanOwningProgram(const uint8_t *anyPtrInProgram)
{
  uint8_t *progStart = forthOwningProgramStart(anyPtrInProgram);
  if (!progStart) {
    return;
  }
  for (uint8_t i = 0; i < forthScannedCount; i++) {
    if (forthScannedProgs[i] == progStart) {
      return;   /* first touch already done this generation */
    }
  }

  uint8_t *nextStart = forthNextProgramStart(progStart);
  forthOuterCtx_t ctx;
  uint8_t *step = progStart;
  while (step && (nextStart == NULL || step < nextStart)) {
    uint8_t len;
    if (forthStepPayload(step, &len) && len > 0) {   /* markers (len==0) skipped */
      xcopy(ctx.source, step + 4, len);
      ctx.source[len] = 0;
      forthOuterRun(&ctx, FORTH_OUTER_DEFS_ONLY);
      if (lastErrorCode != ERROR_NONE) {
        return;   /* halt at the failing step; program stays unrecorded */
      }
    }
    uint8_t *next = findNextStep(step);
    if (!next || next <= step) {
      break;      /* defensive, mirrors forthMarkerTurnsOn */
    }
    step = next;
  }

  if (forthScannedCount < FORTH_SCAN_MAX) {
    forthScannedProgs[forthScannedCount++] = progStart;
  }
  /* List full: program scanned but unrecorded — a later touch re-scans and
   * recompiles. Shadowing keeps lookups correct (forthFindColon walks
   * latest-first) at the cost of dict bytes. Bounded, documented D-2b
   * exception; 8 distinct Forth-bearing programs per run is beyond any
   * realistic session. */
}
```

2. Replace `forthProgramStep` with its final form:

```c
void forthProgramStep(const uint8_t *payload)
{
  forthRunGenCheckReset();                  /* generation first: may clear dict + scan list */
  forthPreScanOwningProgram(payload);       /* payload sits inside the step, inside the program */
  if (lastErrorCode != ERROR_NONE) {
    return;                                 /* pre-scan error halts before executing this step */
  }
  forthOuterCtx_t ctx;
  uint8_t len = *payload;
  xcopy(ctx.source, payload + 1, len);
  ctx.source[len] = 0;
  forthOuterRun(&ctx, FORTH_OUTER_SKIP_DEFS);
}
```

`fnForthOuter` stays `FORTH_OUTER_FULL` — interactive lines keep today's
compile-and-execute-in-place semantics.

Refresh + gate. NOTE: existing tests that drive `forthProgramStep` on
programs whose steps contain definitions may legitimately change behavior
(definitions now compile at first touch, and `SKIP_DEFS` skips them at
execution). If any existing test goes red, do NOT patch the engine — read
that one test, decide whether its expectation encodes the OLD execute-in-
place semantics, and report the test name + the exact assertion that fails.
STOP there; the architect will rule. If the gate is green, you are done.

---

> **Q13 STATUS: COMPLETED (2026-07-13).** Qwen's engine code was
> spec-verbatim and is kept as-is; the STOP clause fired on three existing
> tests encoding the retired execute-in-place contract (stack-buffer
> payloads, definitions compiled at execution). Architect ruling: contract
> migration, not engine change. All three were rewritten against real
> programs (`writeTestProgram` + payloads at `beginOfProgramMemory + N`):
> `test_program_step_define_and_use` and `test_exec_step_source_runs` keep
> their original mutation targets; `test_program_step_gen_reset` needed a
> NEW observable because its old expectation (FUNCTION_NOT_FOUND after a
> generation bump) is exactly what the pre-scan eliminates — it now pins
> `forthRunGenCheckReset` via an interactive word (GENX) that must NOT
> survive the bump, plus `fdict.count == 1` after re-scan. The retired
> `build_payload` helper was removed. checkReset-deletion mutation
> verified RED; gate green (113 PASS). No commit (P2 commits at Q15).

## Q14 — Pillar 2 tests, part 1 (T2.1–T2.4)

Read: `packages/forth-core/test_dict_reloc.c` lines 2280-2340
(`writeTestProgram`/`cleanupTestProgram`) and lines 2405-2470 (program
byte-array example with ITM_FORTH steps). Also your Q13 code in
`forth_compile.c` (grep `forthPreScanOwningProgram`).

Harness pattern for all four tests (the runner-level dispatch is simulated by
calling `forthProgramStep` directly on each step's payload pointer, exactly
as the ITM_FORTH arm does — payload = step address + 3):

```c
forthRunGenBump();               /* fresh generation, like a run start */
lastErrorCode = ERROR_NONE;
uint8_t savedRS = programRunStop;
programRunStop = PGM_RUNNING;    /* forthInner(idx, fromProgram) must not break */
... forthProgramStep(stepN + 3); ...
programRunStop = savedRS;
```

Encode a Forth step of source `SRC` (length L) as bytes
`0x8B, 0x1A, 0xFD, L, <SRC bytes>`. Step addresses inside the written
program: first step at `beginOfProgramMemory`, each next at previous + 4 + L.
End every test with `forthDictClear(); cleanupTestProgram();`.

- **T2.1 `test_prescan_forward_reference`** — steps: `"FWD"` then
  `": FWD 42 ;"`. Call `forthProgramStep` on step 1 only. Assert no error
  and `x_is_longint(42)`. Comment: "Must fail if the pre-scan is skipped or
  only covers steps before the current one (execute-in-place raises
  FUNCTION_NOT_FOUND)."
- **T2.2 `test_prescan_no_early_tail`** — steps: `": A2 1 ; 99"` then
  `"A2"`. Record a stack-depth proxy before (push a sentinel via
  `forthPushInt32(777)` and count on it): execute step 1 then step 2.
  Assert the values above the sentinel are exactly 99 then 1 (X==1, next
  down 99, next down 777 — use the suite's X/Y check helpers; if only X is
  checkable, execute `+` via `forthOuterInterpret("+")`... do NOT — keep it
  simple: assert X==1 after step 2, then `fnDrop` twice and assert
  X==777). Comment: "Must fail if DEFS_ONLY executes interpret-state tokens
  (99 pushed during pre-scan too — sentinel lands one deeper)."
- **T2.3 `test_prescan_no_recompile`** — steps: `": B3 5 ;"` then `"B3"`.
  Execute step 1; record `fdict.count` and `fdict.here`; execute step 2;
  assert `x_is_longint(5)`, `fdict.count` unchanged and equal to 1, and
  `fdict.here` unchanged. Comment: "Must fail if SKIP_DEFS recompiles the
  definition when execution passes the defining step (count 2 / here
  grows)."
- **T2.4 `test_prescan_owning_scope`** — build ONE memory write containing
  TWO programs: program 1 = Forth step `": ONLY1 8 ;"`, then an `ITM_END`
  step (`0x85, 0xB2` — copy the exact END encoding from the
  `writeTestProgram` doc comment you read), then program 2 = Forth step
  `"ONLY1"`. After the write, verify `numberOfPrograms >= 2` (report and
  SKIP-fail if the harness didn't split — do not fake it). Execute program
  2's step only. Assert `lastErrorCode == ERROR_FUNCTION_NOT_FOUND` and
  `fdict.count == 0`. Comment: "Must fail if the pre-scan walks all
  programs instead of only the owning one (D-2c)."

Register the four contiguously. Refresh + gate: green, +4 PASS.

Mutation verification (mandatory):
1. In `forthPreScanOwningProgram`, replace the DEFS_ONLY call's mode with
   `FORTH_OUTER_FULL` → T2.2 RED (early tail execution). Revert.
2. In `forthProgramStep`, change `FORTH_OUTER_SKIP_DEFS` to
   `FORTH_OUTER_FULL` → T2.3 RED (recompile). Revert.
3. In `forthPreScanOwningProgram`, replace `nextStart` computation with
   `NULL` (walk to end of program memory) → T2.4 RED (scope violation).
   Revert. Final gate GREEN.

---

> **Q14 STATUS: COMPLETED BY ARCHITECT (2026-07-13) — do not re-run.**
> Qwen's four tests were structurally correct but two had miscounted
> payload length bytes (T2.1: len 9 for 10 bytes; T2.2: len 12 for 11) —
> same defect class as Q10's RTN byte. The desynced streams made the
> engine interpret garbage (hang on Qwen's run, segfault on re-run).
> Worse: the stuck session ran a git file-revert that wiped ALL uncommitted
> Q11–Q13 engine work (bridge helpers, modes, pre-scan) back to HEAD —
> only the test file survived. The engine was reconstructed verbatim from
> the Q11–Q13 specs, verified by the Architecture-2-only tests, and
> everything is now COMMITTED (see preamble rule 5's new git prohibition).
> All three Q14 mutations verified RED — note mutation 3 as originally
> written (nextStart→NULL) is inert for T2.4 (program 2 is last, its
> nextStart is already NULL); the effective scope mutation is forcing
> forthOwningProgramStart to return the first program.

## Q15 — Pillar 2 tests, part 2 (T2.5–T2.8) + arena report, then commit

> **STATUS: COMPLETED (2026-07-13) — SERIES FINISHED.** Qwen's four tests
> were structurally sound; two carried the recurring payload-length
> miscount (T2.6: len 19 for 20 bytes; T2.8: len 11 for 9 bytes plus the
> derived use-step offset). The T2.8 desync was the reported segfault
> (again mis-attributed by stdout buffering to test_outer_ctx_at_rest;
> gdb placed it in T2.8's writeTestProgram → scanLabelsAndPrograms).
> Fixed by architect; gate green at 121 PASS; all four mutations verified
> RED; committed as "forth-core: P2 tests T2.5-T2.8 + arena report".
> Arena high-water (post-prescan, T2.8): here=16 sizeBlocks=4; suite-end:
> here=36 sizeBlocks=16, freeRamDelta=64.

Read: your Q14 tests in `test_dict_reloc.c` (grep `test_prescan_` and read
that region) — reuse their harness pattern verbatim.

- **T2.5 `test_prescan_generation_rearm`** — run the T2.1 program shape to
  completion (both steps). Record `fdict.count`. Then `forthRunGenBump();`
  and execute step 1 again. Assert success (X==42 again) and `fdict.count`
  equals the recorded value (fresh compile, not doubled, not missing).
  Comment: "Must fail if forthRunGenCheckReset clears the dictionary but
  not forthScannedCount (scan skipped after dict clear → FUNCTION_NOT_FOUND)."
- **T2.6 `test_prescan_error_halts`** — steps: `"77 : C6 NOSUCHWORD ;"` then
  `"1"`. Push sentinel 555 first. Execute step 1. Assert:
  `lastErrorCode == ERROR_FUNCTION_NOT_FOUND`; X is still the 555 sentinel
  (the tail `77` never executed — pre-scan halted the step);
  `fdict.count == 0`. Comment: "Must fail if pre-scan errors are swallowed
  and the step executes against a partial dictionary, or if the halt lands
  after the tail ran."
- **T2.7 `test_prescan_last_step_visible`** — steps: `"LAST7"` then
  `": LAST7 3 ;"` where the defining step is the FINAL step of the FINAL
  program in memory (single program, two steps — `forthNextProgramStart`
  returns NULL). Execute step 1. Assert X==3. Comment: "Must fail if the
  walk bound drops the final step (exclusive-bound bug) or mishandles the
  NULL next-program sentinel."
- **T2.8 `test_prescan_two_programs_first_touch`** — two programs in one
  write (T2.4 pattern): program 1 = Forth step `"9"`, program 2 = Forth step
  `": P2W 4 ; P2W"`. Execute program 1's step, then program 2's step (same
  generation, no bump between). Assert X==4 and, after one `fnDrop`, X==9;
  assert `fdict.count == 1` (only P2W). Comment: "Must fail if the scanned
  list is a single pointer instead of an array (P2's touch would evict P1's
  record and a third touch of P1 would re-scan/recompile), or if nested
  bookkeeping broke sequential multi-program stepping."
- Arena duty: at the end of T2.8, print
  `  FORTH ARENA (post-prescan): here=%u sizeBlocks=%u\n` from `fdict`.

Register the four contiguously after the Q14 group. Refresh + gate: green,
+4 PASS.

Mutation verification (mandatory):
1. Remove `forthScannedCount = 0;` from `forthRunGenCheckReset` → T2.5 RED.
   Revert.
2. In `forthPreScanOwningProgram`, change the error check after the
   DEFS_ONLY call to `lastErrorCode = ERROR_NONE;` (swallow) → T2.6 RED.
   Revert.
3. Change the walk condition `step < nextStart` to also stop one step early
   for the NULL case (e.g. bound by the step BEFORE program end — simplest:
   `while (step && nextStart != NULL && step < nextStart)`) → T2.7 RED.
   Revert.
4. Shrink `FORTH_SCAN_MAX` to 1 → T2.8 must stay GREEN (both programs still
   first-touch-scan; eviction only costs a re-scan on a THIRD touch) — then
   additionally change the recording to `forthScannedProgs[0] = progStart;
   forthScannedCount = 1;` while ALSO breaking the duplicate check to
   compare only `forthScannedProgs[0]`... if this compound mutation is
   awkward, run mutation 4 as: make `forthPreScanOwningProgram` return
   immediately when `forthScannedCount > 0` → T2.8 RED (program 2 never
   scanned). Revert. Final gate GREEN.

Commit (NOTE — updated after the Q14 incident: the P2 engine and T2.1-T2.4
are already committed as "forth-core: P2 engine (pre-scan) + T2.1-T2.4";
this commit carries only Q15's additions):

```
git add packages/forth-core/test_dict_reloc.c \
        packages/forth-core/patches packages/forth-core/files \
        packages/forth-core/.refresh-manifest.json
git commit -m "forth-core: P2 tests T2.5-T2.8 + arena report

Generation re-arm, pre-scan error halt, last-step walk bound, two-program
first-touch. Completes the Architecture 2 test matrix (D-2 a/b/c all
pinned)."
```

Report `git show --stat HEAD`, the gate tail, and the
`FORTH ARENA (post-prescan)` line from the run.
