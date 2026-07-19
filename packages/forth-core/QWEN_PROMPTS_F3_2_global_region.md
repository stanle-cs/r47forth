# Stage F3-2 — global region, word refs, persistence swap, validator retarget

Origin: DESIGN §10.3 via the F3 design pass (`QWEN_PROMPTS_F3_core.md` D1/D2).
This packet creates the persistent global dictionary region `gdict`, splits
the FTOK_CALL token space, introduces word REFS (bit 15 = global), teaches
the inner interpreter to execute across both regions, swaps persistence from
fdict to gdict, and retargets the F1-5 restore validator to gdict.  No
production path WRITES gdict yet (`GLOBAL` is F3-4); tests hand-build gdict
content with the helpers this packet defines.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree on `forth-core/pem-entry-fixes`; `git log --oneline -1` shows
   the F3-1 commit `forth-core: F3-1 — dictionary headers carry an owner
   field`.
2. `grep -n "uint16_t owner" packages/forth-core/forth_dict.h` matches
   exactly one line; `grep -n "FORTH_OWNER_GLOBAL" packages/forth-core/forth_dict.h`
   matches exactly one line.
3. `grep -n "PASS V-B7\|PASS V-B8" packages/forth-core/test_dict_reloc.c`
   shows both F3-1 pins registered.
4. `grep -cn "gdict" packages/forth-core/forth_dict.c` prints 0 (no global
   region exists yet).
5. `grep -n "forthDictBase" packages/forth-core/saveRestoreBackup.c` shows
   the five-key save block and the five-key restore block (10 total
   forthDict* key mentions across both).
6. `grep -n "fdict\." packages/forth-core/forth_inner.c | wc -l` — record
   the count N in the todo file; the Change-4 sweep below must account for
   every site (the sweep is enumerated; if the enumeration and N disagree,
   STOP).
7. Pre-gate green; record the arena baseline from F3-1's commit message.

---

## PREAMBLE (paste before the task)

Identical to F3-1's preamble with every path renamed `f3-1` → `f3-2`
(`/tmp/forth-f3-2-todo.md`, `/tmp/forth-f3-2-gate.log`, mutation logs
`/tmp/forth-f3-2-mutN.log`).  All nine rules and the two-attempt debugger
handoff apply verbatim.

---

## F3-2 — a second region, one interpreter

### Authority carried by this packet (no open choices)

1. **gdict.** A second `forthDict_t gdict` (extern in forth_dict.h,
   defined in forth_dict.c with the same static initializer shape as
   fdict).  Its region grows through the same ensure policy as fdict via a
   parameterized internal:

   ```c
   #define FORTH_GDICT_INITIAL_BLOCKS 16
   static bool dictEnsureOn(forthDict_t *d, uint16_t bytes, uint16_t initialBlocks);
   bool forthDictEnsure(uint16_t bytes);    /* wrapper: (&fdict, bytes, FORTH_INITIAL_BLOCKS, + PC test override) */
   bool forthGDictEnsure(uint16_t bytes);   /* wrapper: (&gdict, bytes, FORTH_GDICT_INITIAL_BLOCKS) */
   ```

   `dictEnsureOn` is the EXACT body of today's `forthDictEnsure` with
   `fdict.` → `d->` and the initial-block constant parameterized; the
   PC-build `testInitialBlocks` override applies to the fdict wrapper only.
   The 64 KB wrap guard and the grow policy are unchanged.  New lifecycle:
   `forthGDictInit()` (reset scalars, base NULL — no free) and
   `forthGDictClear()` (free + reset), mirroring the fdict pair.
2. **Word refs.** In forth_dict.h:

   ```c
   #define FORTH_GCALL_BASE 0x7000u
   #define FORTH_REF_GLOBAL 0x8000u
   static inline ftoken_t forthTokenFromRef(uint16_t ref) {
     return (ref & FORTH_REF_GLOBAL) ? (ftoken_t)(FORTH_GCALL_BASE + (ref & 0x7FFFu))
                                     : (ftoken_t)(0x1000u + ref);
   }
   static inline uint16_t forthRefFromToken(ftoken_t tok) {
     return (tok >= FORTH_GCALL_BASE) ? (uint16_t)(FORTH_REF_GLOBAL | (tok - FORTH_GCALL_BASE))
                                      : (uint16_t)(tok - 0x1000u);
   }
   ```

   FTOK_CALL token space: `0x1000..0x6FFF` transient (index = tok−0x1000),
   `0x7000..0x7EFF` global (index = tok−0x7000).  `startDefinition`'s count
   cap literal changes `0x6F00` → `0x6000` (token 0x1000+0x5FFF stays below
   FORTH_GCALL_BASE).
3. **Lookup returns refs.** `forthFindColon(name, &out)` KEEPS its name and
   signature but `out` becomes a REF: walk fdict newest-first (unfiltered —
   scope filtering is F3-3), then gdict newest-first (`0x8000|gidx`), both
   skipping FF_SMUDGE.  A new
   `bool forthFindColonRef(const char *name, uint16_t *ref, uint8_t *flags)`
   is the same walk additionally reporting `hdr->flags`;
   `forthFindColon` becomes a thin wrapper over it discarding flags.
   `forthDictNameByIndex(idx, ...)` is RENAMED `forthDictNameByRef(ref, ...)`
   and region-dispatches on bit 15 (walk the selected region; the transient
   arm keeps the existing FF_SMUDGE reject; the global arm has no smudged
   entries by validator rule but keeps the same check).  Update its two
   production callers (grep `forthDictNameByIndex` — expected in
   `programming/manage.c` FCALL arm and `test_dict_reloc.c`; a third caller
   is a STOP).
4. **Cross-region execution.** `forthInner`'s entry parameter is now a REF.
   A region bit accompanies every ip:

   ```c
   static uint64_t rstackRegionBits;   /* bit i = region of rstack[i]'s ip (1 = gdict) */
   ```

   Inside `forthInner`: local `bool curG = (wordRef & FORTH_REF_GLOBAL) != 0;`
   set at entry and at every call/exit transition.  Two static inline
   helpers replace direct fdict access in the interpreter ONLY:

   ```c
   static inline uint8_t *innerBase(bool g) { return g ? gdict.base : fdict.base; }
   static inline uint16_t innerHere(bool g) { return g ? gdict.here : fdict.here; }
   ```

   `readToken(ip)` → `readToken(curG, ip)`; `boundedRead(ip, n)` →
   `boundedRead(curG, ip, n)` (bound = `innerHere(g)`; error identical).
   Every `fdict.base + ip`-style operand read in the dispatch arms becomes
   `innerBase(curG) + ip` (LIT, ILIT, BR, 0BR, C47 itemId + params — the
   Change-4 sweep enumerates each).  `bodyOffsetOfIndex(idx)` is RENAMED
   `bodyOffsetOfRef(ref)`: selects the region from bit 15, walks that
   region's chain (transient arm identical to today; global arm mirrors on
   gdict), header prefix 6.
   - FTOK_CALL dispatch: `uint16_t calleeRef = forthRefFromToken(tok);`
     rsp-overflow guard unchanged; push
     `rstack[rsp] = ip; if (curG) rstackRegionBits |= (1ull << rsp); else
     rstackRegionBits &= ~(1ull << rsp); rsp++;` then
     `curG = (calleeRef & FORTH_REF_GLOBAL) != 0; ip = bodyOffsetOfRef(calleeRef);`
     with the existing FORTH_NULL → corrupted-data error.
   - FTOK_EXIT with `rsp > rspBase`: `--rsp; ip = rstack[rsp];
     curG = (rstackRegionBits >> rsp) & 1;`.
   - The entry-point resolve and its FORTH_NULL error are unchanged apart
     from ref decoding.
   `fnForthCall(param)` needs no edit (param is a ref by construction).
   `forthOpenDefinitionIndex` output stays a valid transient ref.
5. **Compiler emit site.** In forth_compile.c's colon branch, the emit
   becomes `forthDictEmit(forthTokenFromRef(widx))` (interpret branch
   `forthInner(widx, ...)` is already ref-correct).  `RECURSE`'s
   `FTOK_CALL_BASE + idx` in forth_prims.c stays byte-identical (self-calls
   are transient by construction) — do not edit it.
6. **Persistence swap.** In saveRestoreBackup.c: the five save lines write
   gdict under NEW names — `forthGDictBase` (c47Ptr from `gdict.base`),
   `forthGDictSizeBlocks`, `forthGDictHere`, `forthGDictLatest`,
   `forthGDictCount` — replacing the five `forthDict*` lines at the same
   anchored position.  The restore block similarly restores gdict with the
   same pre-seeded defaults idiom (`C47_NULL` / 0 / 0 / FORTH_NULL / 0),
   then calls `forthGDictValidateRestored();` followed by
   `forthDictInit();` (a restore is a lifetime seam: fdict starts empty,
   scan tracking reset — fdict is NEVER persisted from now on).  The old
   `forthDict*` keys disappear from both sides (the name-keyed mechanism
   tolerates removal; old backups restore as empty gdict).
7. **Validator retarget.** `forthDictValidateRestored` is RENAMED
   `forthGDictValidateRestored` and, together with `vBodyWalk`, operates on
   `gdict` (mechanical `fdict.` → `gdict.` inside these two functions
   only).  Rule changes inside the walk:
   - owner check: `hdr->owner != FORTH_OWNER_GLOBAL` → invalid;
   - flags check: `hdr->flags & (uint8_t)~FF_IMMEDIATE` → invalid
     (FF_IMMEDIATE legal — F3-4 produces it; smudge/reserved forbidden);
   - FTOK_CALL arm: only global-space tokens are legal in a global body —
     `tok < FORTH_GCALL_BASE` → invalid; else
     `(uint16_t)(tok - FORTH_GCALL_BASE) > entryIdx` → invalid (define-
     before-use, self-call allowed at == entryIdx);
   - the failure branch orphan-resets via `forthGDictInit()` (P-4
     exception comment carries over verbatim).
   config.c's reset hook gains `forthGDictInit();` immediately after the
   existing `forthDictInit();` (RESET wipes user RAM; globals are user
   memory).
8. **Arena line.** The suite printer becomes:

   ```c
   printf("  FORTH ARENA: dict here=%u sizeBlocks=%u gdict here=%u sizeBlocks=%u freeRamDelta=%ld\n", ...)
   ```

   (fdict fields, then gdict fields, then the existing delta.)  The two
   auxiliary `FORTH ARENA (post-…)` prints are unchanged.  The §5.4
   ceiling (≤ 2 KB) now covers the SUM of both regions — future commits
   quote the full line.
9. **Test builders (normative).** In test_dict_reloc.c, beside
   `begin_word`:

   ```c
   static uint16_t gbegin_word(const char *name, uint8_t nameLen) {
     uint16_t alignedHdr = (uint16_t)TO_BLOCKS(6 + nameLen) * BYTES_PER_BLOCK;
     if (!forthGDictEnsure(alignedHdr)) return FORTH_NULL;
     uint16_t off = gdict.here;
     forthHeader_t *hdr = (forthHeader_t *)(gdict.base + off);
     hdr->link = gdict.latest; hdr->flags = 0; hdr->nameLen = nameLen;
     hdr->owner = FORTH_OWNER_GLOBAL;
     memcpy(gdict.base + off + 6, name, nameLen);
     for (uint16_t i = off + 6 + nameLen; i < off + alignedHdr; i++) gdict.base[i] = 0;
     gdict.here = (uint16_t)(off + alignedHdr);
     gdict.latest = off; gdict.count++;
     return off;
   }
   static bool gemit(ftoken_t t) {
     if (!forthGDictEnsure(2)) return false;
     memcpy(gdict.base + gdict.here, &t, 2); gdict.here += 2; return true;
   }
   static void gend_word(void) {
     gemit(T_EXIT);
     gdict.here = (uint16_t)TO_BLOCKS(gdict.here) * BYTES_PER_BLOCK;
   }
   ```

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `forth_inner.c`,
`forth_compile.c` (one emit line), `saveRestoreBackup.c`, `config.c`,
`programming/manage.c` (rename call sites only), `test_dict_reloc.c` — all
under `packages/forth-core/`.

### Targeted reads

1. forth_dict.h in full (it is small).
2. forth_dict.c: `forthDictEnsure`, `forthFindColon`,
   `forthDictNameByIndex`, `startDefinition` (cap literal),
   `forthDictValidateRestored` + `vBodyWalk` in full.
3. forth_inner.c: `bodyOffsetOfIndex`, `readToken`, `boundedRead`, then
   `forthInner` one dispatch arm at a time.  Enumerate first:
   `grep -n "fdict\." packages/forth-core/forth_inner.c` — every hit must
   be inside the functions above; list them in the todo and tick each as
   converted.  A hit outside them is a STOP.
4. forth_compile.c: grep `FTOK_CALL_BASE + widx` (exactly one line).
5. saveRestoreBackup.c: the two five-line forthDict blocks only.
6. config.c: 3 lines around `forthDictInit();`.
7. programming/manage.c: grep `forthDictNameByIndex` (call sites only).
8. test_dict_reloc.c: `begin_word` block (place the g-helpers after it);
   the T1.x tests (grep `forthDictBase:` and read those two functions and
   the neighboring T1.3/T1.3b/T1.4 functions); the V-B pin function; the
   arena printer line.

### Test migrations (this packet names them; nothing else may change)

- **T1.1 round-trip** (the test asserting
  `save/restore round-trip preserved the dictionary`): rebuild on gdict.
  Hand-build `GW1` (body: `FTOK_ILIT`, int32 7, EXIT) and `GW2` (body:
  call token `FORTH_GCALL_BASE + 0`, `FTOK_ILIT`, int32 35, EXIT) with the
  g-helpers (name lengths 3 — `printf '%s' "GW1" | wc -c` = 3).  saveCalc;
  require `backupFileContains("forthGDictBase:")` and NOT
  `backupFileContains("forthDictBase:")` (old keys gone).  Save gdict
  scalars; `forthGDictClear()`; define a transient `: SRZZ 1 ;`;
  restoreCalc.  Require: gdict scalars match saved; fdict is EMPTY
  (`fdict.count == 0` and `forthFindColon("SRZZ", &r)` false — fdict is
  never persisted); `forthFindColon("GW2", &ref)` true with
  `ref == (FORTH_REF_GLOBAL | 1)`; execute `forthInner(ref, false)` after
  clearing error state and require `x_is_longint(35)` — this drives the
  global FTOK_CALL arm and cross-region body resolution end-to-end.
  Release the pre-clobber region as the old test released fdict's.  Keep
  the post-restore arena print (extend it with gdict fields).
- **T1.2 missing-params**: strip prefix becomes `"forthGDict"`; the
  emptiness assertions move to gdict scalars; fdict emptiness asserted too.
- **T1.3 / T1.3b / T1.4 and every V-B validator pin**: the fixtures build
  in gdict via the g-helpers and the calls become
  `forthGDictValidateRestored()`.  Mechanical moves, with these semantic
  updates: V-B2's bad call token becomes `FORTH_GCALL_BASE + 5` (global
  space, index above own); a NEW V-G1 subcase pins the transient-token
  rule — a gdict body containing `0x1000` (a transient call) must reset,
  PASS line `PASS V-G1: transient call token rejected in global body`; a
  NEW V-G2 subcase builds a word with `hdr->flags = FF_IMMEDIATE` and
  requires validation to ACCEPT it (base non-NULL afterward), PASS line
  `PASS V-G2: FF_IMMEDIATE survives restore validation`; V-B7's foreign
  owner (0x1234) now reds against FORTH_OWNER_GLOBAL.  V-B8 stays on fdict
  untouched (it pins forthDictAllocate).
- The suite-level leak gate must keep passing: every test that allocates
  gdict ends with `forthGDictClear()`.

### Sweep completeness checks (run before the first gate)

- `grep -n "bodyOffsetOfIndex\|forthDictNameByIndex" packages/forth-core`
  recursively → ZERO matches (both renamed everywhere).
- `grep -n "0x6F00" packages/forth-core/forth_dict.c` → ZERO (cap moved to
  0x6000).
- `grep -n "forthDictBase" packages/forth-core/saveRestoreBackup.c` → ZERO.
- `grep -n "fdict\." packages/forth-core/forth_inner.c` → matches ONLY
  inside `innerBase`/`innerHere`/`bodyOffsetOfRef`'s transient arm — list
  and justify each remaining hit in the report.

### Non-goals / STOP boundaries

- No scope filtering, no current-scope variable (F3-3).  No GLOBAL /
  IMMEDIATE / FORGET words (F3-4).  No XEQN (F3-6).
- No change to the F1 lifecycle seams, the F2 param core, `freeList.c`,
  upstream `src/`, or the entry layer.
- If a `fdict.` site in forth_inner.c falls outside the enumerated
  functions, or a third `forthDictNameByIndex` caller exists, STOP.

### Gate and required mutations

Full gate green; record the extended arena line (new baseline shape).
Mutations, each separately, full gate, manual restore:

1. In `forthGDictValidateRestored`, delete the
   `hdr->owner != FORTH_OWNER_GLOBAL` check.  The migrated V-B7 MUST go
   RED.  Green = STOP.
2. In `forthInner`'s FTOK_CALL arm, replace the ref decode with the old
   single-space form `uint16_t calleeRef = (uint16_t)(tok - 0x1000);` for
   all tok ≤ 0x7EFF.  T1.1's execution half MUST go RED (GW2's global call
   token misdecodes; expect ERROR_INVALID_CORRUPTED_DATA or a wrong X —
   name the observed symptom).  If it stays green, STOP and report.
3. In the FTOK_EXIT arm, delete the `curG = (rstackRegionBits >> rsp) & 1;`
   restore (leave the pop).  Add-and-run the nesting probe FIRST (it is
   part of this packet's tests, not the mutation): transient `: TW2 GW2 ;`
   compiled AFTER the T1.1 fixtures exist, executed via `run_word("TW2")`,
   requiring X==35 and no error.  Under the mutation the return into GW2's
   gdict body resolves against fdict and MUST go RED (bounded-read
   corruption error or wrong X).  If green, STOP and report (escape-valve
   rule).
4. At the restore seam, delete only the new `forthDictInit();` call.
   T1.1's fdict-emptiness assertions MUST go RED (`SRZZ` survives).
5. In `forthGDictValidateRestored`'s CALL arm, delete the
   `tok < FORTH_GCALL_BASE → invalid` branch.  V-G1 MUST go RED.

Logs `/tmp/forth-f3-2-mut1..5.log`; no mutation residue in `git diff`;
final gate to `/tmp/forth-f3-2-final.log`; record all migrated/new PASS
lines (T1.x, V-B*, V-G1, V-G2, the nesting probe), both banners, exit 0,
the extended arena line, `git diff --check`, generated-mirror byte
equality.

RULE-1: this packet adds flash (new region code + save keys).  Record the
`make dmcp5r47` delta in the stage commit (owner runs it; PENDING in the
report if unavailable).

### Commit

Only the eight named flat files + generated counterparts +
`.refresh-manifest.json` may be staged.

```text
forth-core: F3-2 — global dictionary region, word refs, gdict persistence
```

Report commit id, required output, mutation symptoms, arena line, flash
status, surprises.
