# forth-core Stage 2 — Three Pillars: Phase 1 Audit
### Upstream Leverage and Footprint Audit (architecture pass, no implementation)

Date: 2026-07-12. Branch: `forth-core/stage2-three-pillars` (branched from
`package-manager/patch-based-overlay` with its in-flight work present, per
instruction). Baselines read in full: `packages/forth-core/DESIGN.md`
(2147 lines, authoritative), `custom_package/README.md`, `BUILD.md`.

Every `[VERIFIED: file:line]` below was re-checked against the working tree
in this session — including re-verification of anchors DESIGN.md already
cites, since several have drifted (drift is called out where found).
`src/c47/...` = upstream (read-only). `packages/forth-core/...` = package.

**Scope lock honored:** the freeList.c double-free-guard bug and the stale
`labelList`/`programList` harness-side precondition violation are in-flight
elsewhere and are NOT analyzed, worked around, or fixed here. Collision
findings per pillar: Pillar 1 — none; Pillar 2 — **one, analysis stopped at
that point** (see §2.4); Pillar 3 — none.

Environment note for Phase 2: this branch carries the partially-migrated
patch-based package layout (`packages/forth-core/patches/` holds 13
generated patches; there is **no** `saveRestoreBackup.c` patch)
[VERIFIED: ls packages/forth-core/patches/]. New hook work would be
authored as flat working-area files and regenerated via `refresh`
(`custom_package/README.md`, Authoring Workflow).

---

## Pillar 1 — Save/Restore Integration (Hook H5)

### 1.1 Upstream architecture matching

**The subsystem.** `saveRestoreBackup.c` is the simulator `backup.cfg`
subsystem — the entire file body is compiled only under
`#if defined(PC_BUILD)` [VERIFIED: src/c47/saveRestoreBackup.c:28, closing
:1472; file header comment "PC_BUILD (simulator) only" :7]. Save is
`saveCalc()` [VERIFIED: :241], restore is `restoreCalc()` [VERIFIED: :747].
Both are flat sequences of `saveStateValue`/`restoreStateValue` calls; both
helpers are `static` to this file [VERIFIED: :70, :572] — a binding hunk
must therefore live inside this file (patch), not in package sources.

**Why the binding is small — the arena rides along wholesale.** `saveCalc`
dumps the entire arena: `saveStateValue(ram, TO_BYTES(RAM_SIZE_IN_BLOCKS),
"ram", "hexDump")` [VERIFIED: :562], and the allocator's bookkeeping with
it: `freeMemoryRegions`/`allocatedMemoryRegions` + counts [VERIFIED:
:278-281]. `restoreCalc` restores all of it symmetrically [VERIFIED: ram
:819; allocator arrays :820-823]. Consequently the Forth dictionary
*region* (an ordinary `allocC47Blocks` block, DESIGN.md §5.2) and its
allocation record are already round-tripped today. The only unsaved state
is the package-BSS control block `fdict` — `{uint8_t *base; uint16_t
sizeBlocks, here, latest, count;}` [VERIFIED: packages/forth-core/
forth_dict.h:37-43] — exactly as DESIGN.md §5.5 assumed.

**The descriptor idiom to mirror (managed-block pointer).** Upstream saves
every arena-resident pointer as a `"c47Ptr"` block offset:

```c
ramPtr = TO_C47MEMPTR(labelList);
saveStateValue(&ramPtr, sizeof(ramPtr), "labelList", "c47Ptr");
```
[VERIFIED: :523-524; programList :526-527] and restores by rebasing:

```c
restoreStateValue(&ramPtr, sizeof(ramPtr), "labelList", "c47Ptr");
labelList = TO_PCMEMPTR(ramPtr);
```
[VERIFIED: :843-844; programList :846-847]. The macros round-trip NULL:
`TO_C47MEMPTR(NULL)` = `C47_NULL` (65535) and `TO_PCMEMPTR(C47_NULL)` =
NULL [VERIFIED: src/c47/defines.h:2182-2184] — so an **unallocated**
dictionary (`fdict.base == NULL`, the common case) serializes cleanly with
no special-casing. Scalars use the plain typed idiom, e.g.
`saveStateValue(&numberOfLabels, ..., "uint16")` [VERIFIED: :399].

**Exact binding points.**
- Save: after the `labelList`/`programList` c47Ptr pair [:523-527] —
  one `ramPtr = TO_C47MEMPTR(fdict.base)` + `"c47Ptr"` line pair, plus four
  `"uint16"` lines for `sizeBlocks/here/latest/count`. Placement is
  cosmetic: "The order in which parameters are saved doesn't matter"
  [VERIFIED: :274-275] — parameters are looked up **by name** from the
  parsed file [VERIFIED: :577-583].
- Restore: after the `labelList`/`programList` restore pair [:843-847] —
  the symmetric c47Ptr rebase into `fdict.base` plus the four scalar
  restores.

**Backward/forward compatibility falls out for free.** A missing parameter
makes `restoreStateValue` print "using default" and return without touching
the buffer [VERIFIED: :585-588]. `restoreCalc` begins with
`doFnReset(CONFIRMED, loadAutoSav)` [VERIFIED: :755], and the package's
reset hook runs `forthDictInit()` inside `doFnReset` [VERIFIED:
packages/forth-core/config.c:1941], leaving `fdict` at the well-formed
empty state `{NULL, 0, 0, FORTH_NULL, 0}` [VERIFIED:
packages/forth-core/forth_dict.c:39-46]. So restoring a **pre-Forth
backup** yields an empty dictionary — correct, since that backup's arena
contains no region. An **old binary** reading a new backup simply never
looks the extra names up. No `BACKUP_VERSION` bump is required by this
mechanism (whether to bump anyway for bookkeeping is a Phase 2 style call).

**No allocator interaction → no bug collision.** The binding is pure
assignment: no `allocC47Blocks`/`freeC47Blocks`/`reallocC47Blocks` on
either path (the region's storage and its allocation record ride the
wholesale ram + allocator-array restore). The in-flight freeList
double-free-guard bug is untouched and untouchable from here.

**Position-independence precondition holds.** Dictionary links and `ip`
values are region-relative (DESIGN.md §5.3); the saved region bytes are
valid wherever the arena image lands, and `fdict.base = TO_PCMEMPTR(saved)`
is the only absolute fix-up — same property `labelList` relies on.

**Anchor drift vs DESIGN.md §5.5/§6-H5** (re-verify before editing, per
DESIGN.md's own rule):

| DESIGN.md cites | Current tree |
|---|---|
| save `numberOfLabels` :398 | :399 |
| save `labelList` c47Ptr :526 | :523-524 (programList :526-527) |
| restore :815-816 | labelList/programList restore :843-847 |
| restore `numberOfLabels` :988 | :1017 |

**Documented boundary (not a decision — stating scope).** The state-file
subsystem (`SAVEST`/`LOADST`, `saveRestoreCalcState.c`) is a *semantic*
serializer: program memory is written value-by-value [VERIFIED:
src/c47/saveRestoreCalcState.c:799-813] and load re-derives lists via
`scanLabelsAndPrograms()` [VERIFIED: :2007]. It never persists raw managed
blocks, so it cannot carry the dictionary — H5 covers the simulator
`backup.cfg` only. That matches DESIGN.md's DECIDED H5 scope and §9.10
item 1's rationale (dictionary is run-scoped/reconstructible elsewhere).

### 1.2 Override elimination

None available — the opposite: `saveRestoreBackup.c` is today **not**
overridden/patched at all [VERIFIED: packages/forth-core/meson.build:2-4
lists 11 override sources, no saveRestoreBackup.c; no
`patches/*saveRestoreBackup*` exists]. Minimal footprint is achieved by
keeping the hunk to pure value-marshaling lines mirroring the labelList
idiom verbatim (~6 save lines, ~11 restore lines, no logic, no new
functions), because `saveStateValue`/`restoreStateValue` are file-static
[VERIFIED: :70, :572]. Nothing moves into `forth_dict.c`: exporting
accessors would enlarge the surface without shrinking the patch.

### 1.3 Lifecycle intersection

Save: user/simulator triggers `saveCalc()` [:241] → flat value dump
including the whole arena [:562]. Restore: `restoreCalc()` [:747] →
`doFnReset` [:755] (Forth dict re-init via package config.c:1941) →
wholesale ram + allocator-array restore [:819-823] → pointer rebases
[:825-880] → scalar restores → `scanLabelsAndPrograms()` [VERIFIED: :1405]
(labelList/programList are *rebuilt*, not trusted — irrelevant to `fdict`,
which has no equivalent rebuild and genuinely needs the explicit
descriptors). The Forth binding sits entirely inside the existing
restoreStateValue block; it introduces no ordering constraint (name-keyed
lookup) and no multi-step branching.

Acceptance hook (§7.6 "save → restore round-trips the dictionary;
region-relative links intact") binds here; test specification is Phase 2.

---

## Pillar 2 — Architecture 2 (Run-Start Pre-Scan)

### 2.1 Upstream architecture matching

**`scanLabelsAndPrograms` offers NO hook point.** The function
[VERIFIED: src/c47/programming/manage.c:102-180] is straight-line code:
two `findNextStep` walks over program memory (count pass, fill pass) with
`freeC47Blocks`/`allocC47Blocks` of `labelList`/`programList` in between
[VERIFIED: :110-111, :131, :138]. There is no call-out, no registered
callback, no extensible dispatch — piggybacking would mean editing its
body. Under the pillar's own framing that route is a `[DECISION NEEDED]`
per Rule 3 — **but it does not need to be taken**, for two independent
reasons:

1. **DESIGN.md already decided the seam.** §9.2: Architecture 2 "hangs off
   the §9.3 run-generation seam and changes **nothing** in the stored
   representation". The run-generation seam is package-owned and landed.
2. **`scanLabelsAndPrograms` is the wrong lifecycle anyway.** It runs on
   every program edit (insertion path [VERIFIED: manage.c:730 within the
   insert machinery at :720-733], plus :200, :249, :277) and at restore
   [VERIFIED: saveRestoreBackup.c:1405] — edit-time, not run-time. A
   pre-scan attached there would compile at edit time, violating the
   run-scoped dictionary (§9.3) and "no validation at entry" (§9.6).

**The sanctioned seam, as landed.** Fresh-run detection already exists at
exactly two package-owned bump sites:
- Site A — `fnExecute` entry, gated `programRunStop != PGM_RUNNING`
  [VERIFIED: packages/forth-core/programming/lblGtoXeq.c:162];
- Site B — top of `runProgram`, gated `!nestedEngine && !singleStep &&
  menuLabel != INVALID_VARIABLE`, before `programRunStop = PGM_RUNNING`
  [VERIFIED: :904, :907].

Both feed `forthRunGenBump()` [VERIFIED: packages/forth-core/
forth_compile.c:31-33]; the lazy consumer `forthRunGenCheckReset()` fires
**exactly once per generation** — at the first executed `ITM_FORTH` source
step, from `forthProgramStep` [VERIFIED: forth_compile.c:35-40 (clear +
resync), :399 (call site); dispatch arm packages/forth-core/programming/
lblGtoXeq.c:860-867].

**Key timing fact for a pre-scan.** At bump site A the target program is
not yet resolved (`fnExecute` bumps before `fnGoto`; upstream shape
[VERIFIED: src/c47/programming/lblGtoXeq.c:160-201, interactive branch
:191-201, `runProgram(false, INVALID_VARIABLE)` at :199]). At the lazy
seam, by contrast, `currentStep` already sits **inside** the target
program. So the single clean run-once point that (a) exists today, (b) is
package-owned, (c) needs zero new hook lines in any override, and (d) has
the program context in hand, is `forthRunGenCheckReset` — upgrade it from
"clear dictionary" to "clear dictionary, then pre-scan the owning program"
and Architecture 2's forward-reference parity hangs off it exactly as §9.2
prescribed. (Positional alternatives — e.g. an extra call after `fnGoto`
in the `fnExecute` else-branch — would add override lines and still need
the generation check to avoid double-scanning; noted and disfavored,
final selection is Phase 2.)

**Binding representation constraint (DECIDED, restated).** §9 decision 2 +
§9.2: the stored representation does not change; program→Forth references
stay name strings; `widx` never persists (§4.2 P-3). Any pre-scan design
must be a pure run-start compilation pass over the *same* `ITM_FORTH`
steps.

### 2.2 Override elimination

No Stage 1 hook file is eliminated, but the strongest possible footprint
result holds: **zero new override/patch lines are required** for the
trigger — both bump sites already exist in the (already-owned)
`lblGtoXeq.c` override, and the pre-scan body would live in package-owned
`forth_compile.c` (which owns the generation counters, the tokenizer, and
`forthOuterInterpret`, all `static` there [VERIFIED: forth_compile.c:24,
:28-29, :44-45]). This also satisfies the pillar's centralization
constraint: run-dispatch logic remains concentrated in the `lblGtoXeq.c`
override + `forth_compile.c`, with no third file touched.

### 2.3 Lifecycle intersection (cited trace, keypress → execution)

Keypress → `executeFunction` [VERIFIED: src/c47/keyboard.c:928] →
`runFunction` [VERIFIED: src/c47/items.c:630] → `reallyRunFunction`
[VERIFIED: :237] → `indexOfItems[func].func(param)` [VERIFIED: :401]
(drift: DESIGN.md §0.4 cites :628/:399) → for `XEQ`: `fnExecute`
[VERIFIED: src/c47/programming/lblGtoXeq.c:160] → interactive branch
(`programRunStop != PGM_RUNNING`) → `fnGoto` :192 → `runProgram` :199 →
step loop [VERIFIED: :899-964] → `executeOneStep` per step :910, error
break :911/:930-932, per-step DMCP key poll gated `!nestedEngine`
:933-955. Program-internal `XEQ` steps instead push a subroutine level
(`programRunStop == PGM_RUNNING` branch, :161-189) and do **not** re-enter
a fresh run — which is precisely why bump site A's gate makes "fresh run"
detection exact.

Within this trace the pre-scan's run-once point is the generation-change
fire inside the first `ITM_FORTH` step's `forthProgramStep` (§2.1 above):
it executes once, before any Forth source of that run has interpreted,
with no branching side effects on the C47 step machine (it is reached
*inside* one `executeOneStep`, which returns 1 as today [VERIFIED:
packages/forth-core/programming/lblGtoXeq.c:866]).

### 2.4 [DECISION NEEDED — collides with known open bug: stale labelList/programList harness-side precondition violation]

The pre-scan must establish the **owning program's bounds** to walk its
`ITM_FORTH` steps. The landed idiom for this is the `programList` scan
(largest `instructionPointer ≤ step` — the `forthMarkerTurnsOn` approach,
DESIGN.md §9.4), i.e. a direct dependence on `programList`/`labelList`
freshness at run start. Production-side that precondition is maintained
(rebuild on every edit and on restore — citations in §2.1/§1.3). But the
in-flight harness bug is exactly a violation of this precondition in the
self-test environment (cf. the already-documented instance class in
`test_dict_reloc.c` [VERIFIED: comments and rescans at
packages/forth-core/test_dict_reloc.c:1330-1352, :2219-2242]).

Per the scope lock, Pillar 2's analysis **stops here**. Specifically
deferred until that bug's resolution lands (and deliberately NOT designed
around, e.g. by substituting a `beginOfProgramMemory` walk to dodge
`programList` — that would be silently papering over the bug's blast
radius):

- the exact owning-program walk mechanics;
- pre-scan compile semantics for mixed lines (see consolidated list, D-2);
- **all** Phase 2 test specification for this pillar (any pre-scan test
  runs a program under the harness whose precondition is currently
  violated).

---

## Pillar 3 — Re-entrancy Hardening (Stage 2+)

### 3.1 Upstream architecture matching

**`nestedEngine`: real definition and usage.** It is a **local variable**
of `runProgram`, derived per-invocation:
`bool_t nestedEngine = (programRunStop == PGM_RUNNING);`
[VERIFIED: src/c47/programming/lblGtoXeq.c:872]. All five upstream uses
are inside `runProgram` [VERIFIED: :872, :873, :875, :934, :967]. It is
not a global, not a counter, and **cannot be "checked alongside rsp" from
forth_inner.c** — there is nothing to link against. What *is* reusable:

1. **Its derivation.** `programRunStop == PGM_RUNNING` is the machine-wide
   "an engine is running" signal, and the package already threads exactly
   this into the inner interpreter: `fnForthCall` calls
   `forthInner(param, programRunStop == PGM_RUNNING)` [VERIFIED:
   packages/forth-core/forth_bridge.c:11-14].
2. **Its pattern.** Upstream `runProgram` is itself re-entrant not via a
   guard but via a **watermark on a shared depth counter**: it captures
   `startingSubLevel` from `currentSubroutineLevel` at entry [VERIFIED:
   :873] and unwinds until the level returns to that watermark [VERIFIED:
   :917-919]. The depth state is `currentSubroutineLevel` →
   `currentSubroutineLevelData->subroutineLevel` [VERIFIED: macro
   src/c47/defines.h:2320], globally anchored in `allSubroutineLevels`
   [VERIFIED: src/c47/c47.c:76], pushed by `fnExecute` under PGM_RUNNING
   [VERIFIED: lblGtoXeq.c:161-177] and popped by `fnReturn` [VERIFIED:
   :212-256].

**Why upstream's counters cannot substitute for a Forth depth measure.**
`allSubroutineLevels` counts C47 *subroutine* levels (XEQ). A nested
`forthInner` entry arrives via item dispatch (`FTOK_C47` →
`reallyRunFunction` → `ITM_FCALL`/XEQ-fallback), which pushes **no**
subroutine level for the item call itself — so C47's depth counters do not
observe item-level re-entry. The native measure of Forth execution depth
is the token return stack: `rsp` over `rstack[FORTH_RSTACK_DEPTH=64]`
[VERIFIED: packages/forth-core/forth_inner.c:23-27].

**Is `rsp` genuinely accessible at the re-entrancy check point?** Yes —
same translation unit: the statics at forth_inner.c:26-28 and the guard at
forth_inner.c:160-165 are in one file; no export is needed.

**What actually blocks nesting today (audit of the current guard).**
- `forthRunning` (bool) refuses nested entry with
  `ERROR_OPERATION_UNDEFINED` [VERIFIED: forth_inner.c:28, :160-165];
  cleared on every exit path [VERIFIED: :174, :189, :199, :213, :227,
  :233, :245-255, :269, :283, :309, :326, :354, :371, :381, :386].
- `rsp = 0` unconditionally on entry [VERIFIED: :178] — this, not the
  bool, is the state-destruction the guard exists to prevent (§3.2).
- Crucially, `ip` and `dispatches` are already **locals** [VERIFIED: :169,
  :157] — per-invocation state needs no change. Direct Forth-level
  recursion (a word calling itself via `FTOK_CALL`) already works through
  `rstack` push/pop with a depth check [VERIFIED: :239-258]. The only
  blocked shape is re-entry **through C47**: `FTOK_C47`/label-XEQ → item
  or program → `ITM_FCALL`/`XEQ 'name'` → nested `forthInner`.

So the pillar's constraint resolves cleanly: no new independent guard
mechanism is warranted. The upstream-matching design shape (Phase 2 to
specify exactly) is the `runProgram` watermark pattern transplanted onto
the existing shared `rstack`/`rsp`: a nested invocation captures
`base = rsp` at entry (instead of zeroing), runs until `FTOK_EXIT` at
`rsp == base`, and restores `rsp = base` on every error exit; the bool
becomes a small depth counter (the direct analog of upstream deriving
nesting from `programRunStop` + watermarking the shared level counter).

**DECIDED-language check (Rule 1).** §3.2 decides "guard, not nesting"
for Stage 1 with explicit deferral: "Real nesting is deferred until a use
case demands it." This pillar *is* the deferred stage — no conflict. Two
normative constraints carry over into any upgrade: (a) C-12 — the
nested-entry refusal error is `ERROR_OPERATION_UNDEFINED`, deliberately
distinct from the rstack-depth `ERROR_RAM_FULL`, and tests assert it
[VERIFIED: guard raises it at forth_inner.c:161-163; depth guard raises
RAM_FULL at :242-244]; the upgraded design must map its refusal/cap errors
without breaking that distinction. (b) The §9.9 acceptance suite and §3.2
exit-path discipline (guard state restored on *every* path) remain binding.

**Bug-collision check: none.** `forthInner` consults neither
`labelList`/`programList` (body resolution walks `fdict` only [VERIFIED:
forth_inner.c:118-140]) nor the allocator (no alloc/free anywhere in the
interpreter loop). The two in-flight bugs are orthogonal to this pillar's
surface.

### 3.2 Override elimination

None to eliminate and none to add: the entire pillar lives in
package-owned custom sources (`forth_inner.c`, and `forth_bridge.c`'s
`fnForthCall` already passes the correct nesting signal). No override file
carries re-entrancy logic today, and none would after.

### 3.3 Lifecycle intersection

The re-entry trace the upgrade must serve, fully cited:
Forth word executing in `forthInner` → `FTOK_C47` arm sets PGM_RUNNING
around `reallyRunFunction` [VERIFIED: forth_inner.c:363-368] (or
interpret-state label fallback, same protocol, DESIGN.md §3.3.6) → C47
program runs → a step `XEQ 'W'` resolving to a Forth word (name-string
resolution at run time, §4.2 P-3) or an `ITM_FCALL` step → `fnForthCall`
→ nested `forthInner` — today refused at forth_inner.c:160.

**Boundary discovered — the outer interpreter is a second single-level
guard on this same trace.** If the nested C47 program instead contains an
`ITM_FORTH` **source step**, entry is `forthProgramStep`, which refuses on
`forthOuterActive` [VERIFIED: forth_compile.c:392-395; guard set/cleared
:384-386, :400-402], and the underlying state is genuinely singleton:
`forthSource[256]` and tokenizer position are file-statics [VERIFIED:
:22-24, :44-45]. Upgrading the inner guard alone therefore enables
recursion through C47 via `XEQ 'name'`/`FCALL` steps, while nested
*source-step* interpretation stays refused — a user-visible asymmetry.
Whether Pillar 3's scope is inner-only (accepting that asymmetry as
documented behavior) or must include outer-interpreter re-entrancy
(stacking/saving `forthSource` + tokenizer state — a materially larger
change) is not decided anywhere in DESIGN.md → flagged below (D-3).

---

## Consolidated flags

### [DECISION NEEDED]

- **D-1 [DECISION NEEDED — collides with known open bug: stale
  labelList/programList harness-side precondition violation]** (Pillar 2,
  §2.4). The pre-scan's owning-program walk depends on `programList`
  freshness; the harness violates exactly that precondition today. Pillar
  2's walk mechanics, compile-semantics pinning, and *all* of its Phase 2
  test specification are stopped pending that bug's resolution. Not worked
  around (a `beginOfProgramMemory`-walk dodge was considered and rejected
  as silent papering-over).

- **D-2 [DECISION NEEDED — underdetermined semantics]** (Pillar 2).
  DESIGN.md §9.2 defines Architecture 2 only as "a run-start pre-scan that
  compiles all `:`-lines first". Unstated: (a) treatment of mixed lines
  (`: SQ DUP * ; 3 SQ` — pre-scanning must not execute the non-definition
  tail early); (b) whether reach-time execution of an already-pre-scanned
  `:` line re-compiles (duplicate entries; latest-wins keeps results
  correct but spends arena and muddies §9.9-style count assertions);
  (c) scope of the scan (whole owning program vs entry-point-forward).
  These need human ratification before Phase 2 can spec them with zero
  unstated decisions — they interact with D-1's resolution, so deciding
  them now would be premature anyway.

- **D-3 [DECISION NEEDED — pillar scope boundary]** (Pillar 3, §3.3).
  Inner-guard upgrade alone yields recursion through C47 via
  `XEQ 'name'`/`FCALL` steps but still refuses nested `ITM_FORTH`
  source-step interpretation (`forthOuterActive`, singleton
  `forthSource`/tokenizer). Choose: inner-only (asymmetry documented) vs
  inner+outer (larger change: outer source/tokenizer state stacking).
  DESIGN.md decides neither.

### [UNVERIFIED — needs confirmation]

- **U-1** (Pillar 1). The exact simulator triggers of
  `saveCalc`/`restoreCalc` (GTK quit/start wiring) were not traced; the
  audit relies only on the functions' internal structure, which is
  sufficient for the binding-point question. Confirm triggers in Phase 2
  only if test specification needs to drive them end-to-end.
- **U-2** (Pillar 3). "Tests assert the C-12 error code" is taken from
  DESIGN.md §3.2/C-12; the specific asserting test in `test_dict_reloc.c`
  was not re-located in this session. Phase 2 must locate it before
  specifying guard changes (the constraint itself is verified in code:
  forth_inner.c:161-163 vs :242-244).

### No-collision statements (scope lock)

- Pillar 1: no dependence on `freeListFree`/allocator behavior (§1.1) and
  none on `labelList`/`programList` correctness (restore rebuilds them
  independently of the Forth binding).
- Pillar 3: no dependence on either bug (§3.1).
- Pillar 2: collides as flagged in D-1; analysis stopped at that point.

---

**Phase 1 ends here. STOP per Rule 4 — no Phase 2 content follows.**
Awaiting explicit human approval (and rulings on D-1's sequencing, D-2,
D-3) before producing `STAGE2_THREE_PILLARS_PLAN.md`.
