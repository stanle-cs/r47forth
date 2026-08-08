# forth-core — DESIGN-HISTORY.md

**Non-normative.** This file is the amendment trail for `DESIGN.md`. It records
*how* decisions were reached, what they superseded, and why — so `DESIGN.md`
can state only what is true **now**, in one voice, with no amendment scaffolding
interleaved with the specification.

Rules:

- `DESIGN.md` is authoritative. Where it speaks, nothing else does.
- Nothing here is normative. If this file and `DESIGN.md` disagree,
  `DESIGN.md` wins and this file has a bug.
- Entries are append-only and dated. Do not rewrite history to match a later
  decision; add a superseding entry instead.
- When an amendment is folded into `DESIGN.md`, its scaffolding
  (`STAGE 2 AMENDMENT`, `[SUPERSEDED]`, `[AMENDMENT PENDING RATIFICATION]`,
  `C-n` / `P-n` / `D-n` inline tags) is **removed** from `DESIGN.md` and its
  narrative lands here.

---

## Tag glossary

Historic amendment series. These tags no longer appear in `DESIGN.md`; they are
retained here so old commit messages, review notes, and test names remain
readable.

| series | date | scope |
|--------|------|-------|
| `C-1`..`C-14` | 2026-07-07 | Sub-phase C compiler pre-build audit |
| `P-1`..`P-10` | 2026-07-10 | PEM-native entry stage (§9) |
| `P-H1`..`P-H7` | 2026-07-10 | PEM-native entry hook points (§6) |
| `H1`..`H10` | 2026-07-05..06 | Stage-1 hook points (§6) |
| `T1-1`..`T1-4` | 2026-07-06 | Pre-H1 conformance fixes |
| `D-2`, `D-3` | 2026-07-13 | Stage 2 rulings (pre-scan, re-entrancy) |
| `F1`..`F4` | 2026-07-11 | PEM audit fixes |
| `V-1`..`V-7` | 2026-07-14 | Extension-principle series (this rewrite) |

Note: the §6 hook IDs and the §4.2 call-site-map H-numbers were independent
numbering schemes. An H-number in the §4.2 table never corresponded to the
hook of the same ID in §6. This collision is one reason the tags were retired.

---

## 2026-07-08 — §3.3-C consolidation (C-1..C-13)

The sub-phase C amendment set (compiler pre-build audit, 2026-07-07) was merged
into `DESIGN.md` in place; every piece of base text it superseded was removed.
The trailing patch-sections — "Stage 1 — Resolution Clarifications", the §3.3
tokenizer NOTE, and the §2.2 `FTOK_LIT`-size correction — were folded at their
sites (§4.1/§1.3, §3.3.3, and the §2 token table + §3.2 pseudocode + §5.4 cost
formula respectively).

Amendment tags `C-1..C-13` were **retained inline** at each merge site for
traceability. That decision is reversed by the 2026-07-14 rewrite: the tags are
now here and `DESIGN.md` reads as one voice.

Substantive corrections in that set, for the record:

- **C-2** — resolution order: number tried **before** C47 label, not after. A
  user program labelled `3` must never hijack the numeric literal `3` inside
  Forth source. The converse loss (a digits-only program name is uncallable
  from Forth by bare name) was accepted as trivial.
- **C-3** — `forthFindPrim` returns `uint16_t` with miss = `FORTH_PRIM_NONE`
  (0xFFFF). The draft's `idx >= 0` test is always true on an unsigned; every
  unknown word would have dispatched `forthPrims[0xFFFE].fn()`.
- **C-12** — nested-entry error code: doc said `ERROR_RAM_FULL`, code raised
  `ERROR_OPERATION_UNDEFINED`. Resolved **in code's favour**; the doc was
  wrong and the tests were right.
- **C-13** — arena cost formula: the earlier `2*(tokenCount + 1)` form
  undercounted every inline payload. Superseded by the per-token-class formula
  now in §5.4.
- **C-14** — error context display: `ERROR_FUNCTION_NOT_FOUND` concatenates the
  offending token, so the user sees `No such function: TOKEN` rather than a
  bare generic error.

## 2026-07-10 — PEM-native entry stage (P-1..P-10)

§9 merged with tags `P-1..P-10`; `P-1..P-4` amended §0.1/§0.2, §2.1,
§3.1/§3.3.1, §3.3.2, §4.2 and §6/§6.2 in place.

- **P-1 — `ITM_FORTH` becomes `PTP_REM`, superseding `PTP_NONE`.** The step
  carries an inline `STRING_LABEL_VARIABLE` payload (the source line, or a
  zero-length payload = the toggle marker). `PTP_REM` was chosen because every
  upstream consumer already handles it generically: step length via
  `countLiteralBytes`, listing render via `decodeRem`, variable-scan skip, and
  the `insertStepInProgram` PTP switch treating it as "nothing to do".
  **Breaking encode change:** a step recorded under the old `PTP_NONE` two-byte
  encoding is unreadable after this change. No migration was provided; the
  representation predated any release.
- **P-3 — names persist, `widx` never does.** Program↔Forth crossings are name
  strings resolved at run time. This is the invariant the whole design rests on.
- **P-6 — entry-only toggle, no runtime flag.** Keypad state is *derived* from
  the program bytes at the cursor, never stored.

## 2026-07-11 — PEM audit fixes (F1..F4)

- **F1** — `forthEntryStateAtInsertion()` added alongside
  `forthEntryStateAtCursor()`. An insertion derives from the step *before*
  `currentStep`, not `currentStep` itself.
- **F3** — `forthFallbackOp` gates the Forth XEQ fallback to
  `ITM_XEQ`/`ITM_XEQP1` only. `LBL?` on a name that resolves only in the Forth
  dictionary reports label-not-found (its own negative result), not an error
  halt.
- **F4** — `insertUserItemInProgram` wrote the opcode low byte as `func & 0x7f`,
  corrupting any item whose low byte ≥ 0x80 (e.g. `ITM_XEQP1` = 0x08AF →
  0x88 0x2F). Fixed to `func & 0xff`. **Blast radius exceeds forth-core:** the
  fix changes encoding for *all* user items with low byte ≥ 0x80 inserted via
  this helper.

## 2026-07-11 — FCALL reject-and-redirect (ratified)

A PEM gesture that would record `ITM_FCALL` + `widx` is rewritten to the stored
source-step form carrying the word's **name** (reverse lookup via
`forthDictNameByIndex`). An unresolvable or indirect `widx` is rejected with
`ERROR_NON_PROGRAMMABLE_COMMAND`. Implemented in the `manage.c` override's
`insertStepInProgram` FCALL arm; verified by `test_fcall_redirect_records_name`
/ `test_fcall_redirect_rejects_stale`.

This closed the "known hole" §4.2 described (a user hand-entering `FCALL nn` in
PEM to persist a raw index). **§4.2's hole text was left stale for three days**
and contradicted the resolution until the 2026-07-14 audit caught it.

## 2026-07-13 — H5: dictionary save/restore

The `saveRestoreBackup.c` package patch carries the five `forthDict*`
parameters; restored state is validated (`forthDictValidateRestored`) and
pre-H5 backups default to an empty dictionary. The dictionary round-trips the
simulator backup. Run-scoping still governs its lifetime across runs, so this
buys crash-safety and cross-session inspection, not durability of words.

## 2026-07-13 — D-2: Architecture 2 (run-start pre-scan) supersedes execute-in-place

**Superseded:** Architecture 1 — "the runner interprets each `ITM_FORTH` source
step as it reaches it", with the documented consequence that a `GTO` jumping
over `: SQ … ;` left `SQ` undefined at the call site.

**Now:** `forthProgramStep` runs `forthRunGenCheckReset()`, then a first-touch
pre-scan of the owning program (`forthPreScanOwningProgram`: every source step
interpreted in `DEFS_ONLY` mode), then executes the current payload in
`SKIP_DEFS` mode.

- **D-2a** — definitions compile, interpret-state tokens are skipped during the
  scan: no early tail execution.
- **D-2b** — `:`…`;` regions are consumed without touching the dictionary during
  execution: no recompilation.
- **D-2c** — scope is exactly the owning program, resolved via
  `forthOwningProgramStart` / `forthNextProgramStart`.

Tracking: `forthScannedProgs[FORTH_SCAN_MAX=8]` + `forthScannedCount`, reset
with the dictionary at the generation seam. **The stored representation did not
change** — that was the point of source-as-truth.

Gained: interpret-state (tail) references resolve against any definition in the
same program, earlier or later. The Architecture 1 "reachability constraint" is
gone.

Retained limitations, documented rather than fixed:

1. Definition-BODY forward references (def → later def) still error at pre-scan
   time, at the referencing step. Standard Forth define-before-use.
2. A pre-scan error halts the run at the triggering step *before* its tail
   executes, and the program stays unrecorded, so a fixed program re-scans.
3. Scan-list overflow (>8 Forth-bearing programs per run) re-scans and
   recompiles on later touches. Shadowing keeps lookups correct at the cost of
   dictionary bytes.
4. Editing programs between single-steps leaves dictionary and scan list stale
   until the next generation bump. Recorded pointers are only compared, never
   dereferenced.

## 2026-07-13 — D-3: full re-entrancy for both interpreters

**Superseded:** the single-level `forthRunning` guard (inner) and
`forthOuterActive` + `static forthSource[256]` model (outer). Both refused
nested entry with `ERROR_OPERATION_UNDEFINED`.

**Now, inner (`forthInner`):** re-entrant to `FORTH_NEST_MAX` (4) via a depth
counter plus an `rsp` watermark. `uint8_t forthDepth` replaces `forthRunning`;
entry at the cap raises the same error; each invocation records
`rspBase = rsp` instead of zeroing `rsp`; `FTOK_EXIT` returns at
`rsp == rspBase`; every exit path unwinds via `rsp = rspBase; forthDepth--`
through one `INNER_LEAVE()` macro. A leaked entry would otherwise be silently
absorbed as the next invocation's floor, shrinking capacity — `rstack[64]`
stays one shared static partitioned by watermarks, so zero BSS growth, and the
`FTOK_CALL` overflow check naturally bounds the sum of all levels.
`forthTestGetRsp() == 0` at rest pins the unwind in every path.

**Now, outer (`forthOuterInterpret`):** each interpret carries a
per-invocation `forthOuterCtx_t` (source[256], tokenizer position, `openDef`
snapshot) on the **caller's C stack**, chained through one static
`forthOuterCur` pointer. `forthOuterDepth` caps nesting at
`FORTH_OUTER_NEST_MAX` (2). The tokenizer statics moved into the context;
`openDef` is snapshot/restored around nesting via `forthDefStateSave` /
`forthDefStateRestore`, so a nested line can never close or abort the outer
line's definition. Idle BSS: ~255 B reclaimed.

Nesting deeper than 2 is unreachable by construction — a label XEQ from a
program-context Forth step is continuation-style (`fnExecute`'s nested branch
pushes a level and defers stepping to the enclosing `runProgram` loop), so
interpreter frames never stack past typed-line → program-step. The cap is a
backstop, pinned by a hook-primed test (T3.6 phase A).

The copy discipline (copy the X string **before** `fnDrop`) was unchanged and
remains normative.

## 2026-07-13 — C-1 label-arm dispatch: `fnExecute`, not a `PGM_RUNNING` wrap

**Superseded:** §3.3.6 prescribed dispatching interpret-state C47-label calls
via `reallyRunFunction(ITM_XEQ, label)` inside the same PGM_RUNNING
save/set/restore wrap as the `FTOK_C47` arm.

That DECIDED text was **defective for `ITM_XEQ` specifically**, first exercised
end-to-end by test T3.5:

1. **The program never ran.** Under a forced `PGM_RUNNING`, `fnExecute` takes
   its nested branch: it allocates a 3-block subroutine level, `fnGoto`s, and
   defers stepping to an *enclosing* `runProgram` loop. From an interactive
   Forth line no such loop exists — the XEQ was a silent no-op that leaked the
   subroutine level (3 blocks per call).
2. **Run-generation bump site A was suppressed.** Forcing `PGM_RUNNING` made
   `fnExecute` skip the bump, so a program run started from a Forth line
   carried a stale dictionary generation.
3. The livelock the wrap defended against lives in `items.c`'s NORMAL-MODE
   dispatch (`refreshStatusBar()` → GTK pump), which calling `fnExecute`
   directly bypasses entirely.

**Now:** the label arm clears `dynamicMenuItem = -1` (its `>= 0` menu branch in
`fnGoto` reinterprets the label ID as a global step number — leftover menu
state sent `goToGlobalStep` off the end of program memory; `fnExecute` resets it
only *after* `fnGoto`, too late for a name-resolved call), then calls
`fnExecute(label)` directly.

The `PGM_RUNNING` wrap **remains correct and normative for the `FTOK_C47` arm**,
where the dispatched items are ordinary functions, not the run-loop driver.

## 2026-07-13 — Range-overlap double-free guard in `freeListFree`

Unconditional range-overlap double-free rejection, `core/freeList.c`. Promoted
to `DESIGN.md` §5.6 and hook H10. Upstream MR to the c47 firmware repository
remains **pending** — this is the one package change that is a candidate to go
upstream rather than live here forever.

---

## 2026-07-14 — Extension-principle series (V-1..V-7)

Origin: hardware testing on a real R47 surfaced four PEM entry defects. Root
causing them exposed a deeper question — *is Forth a separate system or an
extension of RPN?* — which was settled explicitly:

> **Forth is an extension of RPN, not a new system. That is the design core.**

Everything below follows from that ruling. The four field defects and their
root causes are recorded in the PEM entry section of this file.

### V-1 — resolution order gains C47 items, placed **before** label

**Superseded:** prim → colon → number → label → error.
**Now:** prim → colon → number → **item** → label → error.

The first proposal placed items *after* label, reasoning "preserve existing
programs' behaviour exactly". That reasoning was **wrong and was withdrawn**:
it imported a constraint from the reverse direction (§4.2, where existing RPN
programs doing `XEQ 'NAME'` genuinely must keep working) into the forward
direction, where no Forth source exists yet to preserve.

Settled on §4.1's own precedent instead — C-2 put number before label because
*"a user program labeled `3` must never hijack the numeric literal `3` inside
Forth source."* A user program labelled `SIN` must not hijack the builtin `SIN`
by the identical argument. And the extension principle says it directly: in
RPN, `SIN` means the builtin.

Accepted cost: a program named e.g. `MEAN` is uncallable from Forth by bare
name. C-2 accepted precisely this trade for digit-named programs. The escape
hatch is `XEQ 'MEAN'`, which V-3 makes reachable from inside Forth.

### V-2 — compile-state item calls allowed; `FTOK_C47` emitted at last

`FTOK_C47` (token 0x7F04) existed from the original §2.2 as *"the bridge that
lets Forth call the entire C47 command set"*, with a complete runtime decoder,
and was never emitted by the compiler — §2.2 said so: *"until stage-2 work
lands this token is exercised only by hand-assembled test bodies."* This is
that work.

§3.3.6's two reasons for deferring compile-state C47 calls were examined and
found to be **label-specific**:

1. `FTOK_C47` cannot decode `PTP_LABEL` — but items used here are `PTP_NONE`,
   which the decoder already handles.
2. A compiled label ID goes stale because `labelList[]` renumbers — but item
   ids are compile-time constants in `indexOfItems[]` and never renumber.

Neither transfers. The block is lifted **for items only**; it stands for labels
until V-3.

Safety filter: `CAT_FNCT && PTP_NONE`. Checked at the boundary rather than
assumed — `SIN` (76) resolves; `EXIT` (1737, `CAT_NONE | PTP_DISABLED`) and
`ALPHA` (1740, `CAT_NONE`) are excluded. `OFF` (1543, `CAT_FNCT | PTP_NONE`)
resolves, and that is correct: `PTP_DISABLED` means "not programmable", so
passing the filter means the item is legal as a program step. `OFF` in a Forth
word is exactly as dangerous as `OFF` in a keystroke program — not a new hole,
and a principled filter beats a hand-maintained blacklist.

### V-3 — `FTOK_XEQN`: the staleness fix for compile-state label calls

The problem `DESIGN.md` §3.3.6 named but did not solve. `findNamedLabel`
returns an index into `labelList[]`; `scanLabelsAndPrograms` **frees and
reallocates** that array, resets `numberOfLabels = 0`, and rebuilds it by
walking program memory in address order — on **every edit**. Indices are
positional. Insert a `LBL` earlier in memory and every later index shifts.
A compiled label ID then calls the wrong program: no error, no crash, just
wrong.

C47 itself never stores label indices — a program's `XEQ 'NAME'` step stores an
inline name string and resolves it at run time. `FTOK_XEQN` does the same, and
reuses `forthResolveXEQ`, which already existed.

**Principle established** (state it once, it settles every future case):

> **Bake ids that are stable; name-resolve ids that aren't.**

| id | stable? | mechanism |
|----|---------|-----------|
| item id (`indexOfItems[]`) | compile-time constant | bake → `FTOK_C47` |
| colon index (`FTOK_CALL`) | stable within a dictionary generation | bake |
| label id (`labelList[]`) | **renumbers on every edit** | name-resolve → `FTOK_XEQN` |

`FTOK_XEQN` also supplies the inline-string machinery that parameterised items
(`STO 'VAR'`) need later. Built once, used twice.

### V-4 — parameterised items follow C47 convention

Ruling: *"we always go with C47's convention; this forth version is an
extension of RPN not a completely new system."*

C47 binds a parameter to its opcode inline: `STO 05` is one step. Carried into
Forth source, `STO` consumes the **next source token** as its parameter — a
parsing word, legitimate in Forth (`."`, `S"` do this) though not
stack-idiomatic.

**Decided: `STO 05`. Rejected: `5 STO`.** They are mutually exclusive.

The parameter grammar belongs to the parsing word, **not** to the §3.3.5 number
rule — `.05` means local register 5, which the number grammar would otherwise
read as the real 0.05. Consuming the raw next token before the number rule sees
it avoids the collision by construction.

Phased: (1) `PTP_NONE` — no grammar, ships on the existing decoder;
(2) `PTP_REGISTER` + `PTP_NUMBER_8/16` direct params; (3) named/indirect
(`STO 'VAR'`, `STO IND 05`) on V-3's inline strings.

### V-5 — primitives are an alias layer, not a vocabulary

Observation forced by V-1: every one of the 11 primitives is a thin wrapper over
an existing C47 handler (`pDup`→`fnDupN(1)`, `pSwap`→`fnSwapXY`,
`pPlus`→`fnAdd`, …). Once items resolve, all of them are reachable through the
item table too.

So the prim table was never a separate vocabulary — it is Forth-idiomatic
**naming** for C47 operations the item table names differently (`SWAP` for
`x<>y`), plus a faster 2-byte token.

This is recorded as a **guardrail**, not trivia: without it, the natural
instinct after V-1 is to keep adding prims (`SIN`, `SQRT`, `MOD`), rebuilding
the disjoint system V-1 removes, one well-intentioned commit at a time.

### V-6 — source-as-truth reaffirmed; entry-time validation added instead

Challenged during review: *"why is a Forth line source text resolved at run
time when an RPN step is a compiled dispatch?"*

The framing was **conceded to be misleading** — an RPN step was never compiled
from anything. You pressed a key and the item was recorded; the byte *is* the
canonical form and decode is a bijection. RPN entry is selection from a finite
set; Forth entry is typing. There is no "RPN convention for storing typed text"
to follow.

Compile-at-entry was then examined properly and rejected on two concrete
grounds:

- **Into the arena, persistent dictionary:** the arena would have to hold every
  definition of every stored program simultaneously across power-off, against a
  ≤ 2 KB high-water ceiling on the 64 KB part — and it contradicts run-scoping,
  whose entire purpose is that runs are deterministic and words do not
  accumulate.
- **Into program memory, tokens in the step:** calls must be name-resolved
  anyway; a call still has to *find* its definition, which is the pre-scan
  again, walking tokens instead of text. All the run-time machinery is kept and
  the source is thrown away — and `EDIT` then needs a full decompiler, a second
  body of code in flash, on a flash-constrained target.

The loss-of-source argument was **weaker than first claimed** and is recorded
honestly: the prim aliases (`*`, `×`, `·`) are distinct token indices and would
decompile correctly. What is actually lost is whitespace, number formatting
(`2.50` → `2.5`), and comments if ever added. The real reasons are the two
above, plus `PTP_REM` buying every upstream consumer for free.

What the challenge correctly identified is that RPN has a property Forth lacked:
*you cannot enter a broken step*. Addressed by **entry-time validation** —
check the line on commit, no storage change. Advisory for not-yet-authored
forward references, since the pre-scan makes those legal.

### V-7 — Forth source lines render bare

**Superseded:** `FORTH 'source'` (the generic `decodeRem` form).
**Also rejected, having first been recommended:** `'source'` (quotes, no prefix).

The quotes recommendation was **withdrawn after being shown backwards**. A
string-literal step already renders `'text'` *with* quotes, so `'source'` is
what collides with a literal. Rendering **bare** collides with nothing: RPN
steps render as `SIN` / `STO 05`, and a Forth line reading `SIN` renders `SIN`
and — post V-1/V-2 — *does the same thing*. Where Forth genuinely differs
(`: SQ DUP * ;`, multi-word lines) it looks different because it is different.

Markers keep their directional render (`»FORTH` / `FORTH«`); only source lines
go bare.

Consequential simplification: the `pemAlphaEdit` extraction offset drops 8 → 0,
and the `"FORTH "` branch in the fnPem cursor-offset hack becomes dead code —
the existing default handles bare text, identically to `ITM_LITERAL`.

### PEM entry defects found on hardware, 2026-07-14

Four field reports, root-caused. All four had **passing tests** — recorded here
because the *reason* they passed is the durable lesson.

1. **Alpha menu never appeared; user stranded in the CAT menu.** `pemAlpha`
   pushes `-MNU_ALPHA`, then `_closeCatalog()` runs *after* `runFunction`
   returns and pops it, because `MNU_ALPHA` is itself listed in
   `CatalogMenus[]`. Double-pop: the toggle arm popped FCNS, `_closeCatalog`
   popped again and ate ALPHA. Inherited from the `ITM_REM` arm it was modelled
   on, which has the same latent flaw and never noticed — REM needs no menu.
2. **EXIT killed alpha input instead of escaping the menu.** A *consequence* of
   (1): `fnKeyExit`'s CM_PEM arm tries `isAlphaSubmenu(0)` first, but the
   current menu was the catalog, so it fell through to `pemAlpha(ITM_BACKSPACE)`.
   Fixing (1) exposes a second, independent bug: `isAlphaSubmenu` does not list
   `MNU_FORTH`, so EXIT from the FWRD picker would kill the capture too.
3. **`tam.function` clobbered on every keystroke.** `insertStepInProgram`'s
   first arm (inherited verbatim from upstream) fires whenever `FLAG_ALPHA` is
   set and unconditionally sets `tam.function = ITM_LITERAL` — before the
   `ITM_FORTH` arm is ever reached. Killed the empty-commit rule, the
   `forthPickerGuard`, and the decode transient-capture exception. Also meant
   toggling *off* during capture was swallowed and never reached the toggle.
4. **ENTER dropped out of capture with no way back.** The design said
   re-entering capture after ENTER was the in-region capture route's job, firing
   on the next printable key. But `FLAG_ALPHA` is what selects the alpha
   keyboard layout — with it cleared, letter keys produce `ITM_SIN` etc., not
   `ITM_A`. Only digits could satisfy the route. There was no keystroke that
   re-opened a Forth *text* line.

**Why every test passed:** the tests call `addStepInProgram` /
`pemCloseAlphaInput` directly, with `catalog = CATALOG_NONE` and `tam.function`
hand-set — bypassing the exact `keyboard.c` dispatch chain that breaks them.
`test_alpha_menu_on_top_during_capture` asserts `currentMenu() == -MNU_ALPHA` at
the one instant it is still true.

**Durable lesson, and the reason this paragraph exists:** a test that primes the
state its subject is supposed to derive proves nothing about the path that
derives it. Entry-layer tests must drive `runFunction` with `catalog` set.

### Audit findings, 2026-07-14

Corrections to `DESIGN.md`'s own gap list, made as part of this rewrite:

- **Text export/import "[GAP]" was a phantom.** It gated a feature on verifying
  that "the import parser round-trips these". There is no import parser — no
  text-parsing import path exists in the tree. `fnPExport`/`_exportProgram` is
  one-way text export for *every* step type, RPN included; the round-trip path
  is `_saveProgram`/`fnLoadProgram`, a **binary** format (header lines then raw
  program bytes) through which Forth steps pass as opaque bytes. Closed as not
  applicable.
- **§4.2's "known hole" contradicted the FCALL resolution** (see 2026-07-11).
  Two sections of the authoritative document disagreed about whether the
  design's central invariant had a hole in it. §4.2's text was stale; removed.
- **Cross-program word reuse is untested.** Nested `XEQ` under `PGM_RUNNING`
  does not bump the run generation, and the pre-scan tracks up to 8 programs, so
  program A doing `XEQ 'LIB'` *may* leave LIB's words resolvable in A's later
  Forth lines. Reasoned through, **not verified**. Either a documented feature
  or an accident that breaks later; one test settles it. Recorded as an open
  question, not a claim.

### Deferred, deliberately

- **Forth words in catalogs / assignable to keys.** RPN programs appear in the
  PROG catalog and can be `ASSIGN`ed; Forth words can neither. Real asymmetry,
  purely additive, no format impact.
- **Interactive `FORTH` requires a string in X.** `fnForthOuter` errors with
  `ERROR_INVALID_DATA_TYPE_FOR_OP` unless X already holds a string, so the same
  item is an entry-mode toggle in PEM and a string-consuming function outside
  it. Extension-consistent would be: pressing FORTH interactively opens the same
  capture PEM gives you. Same entry-layer surface as the PEM fixes, so cheapest
  to do alongside them — but it is an addition, not a correction.
- **Words are program-local; RPN labels are global.** Not classified as a
  violation: RPN's unit of shared code *is* the labelled program, and V-3 makes
  any keystroke program callable from inside a Forth definition. Forth words are
  local helpers; RPN programs are the sharing unit. Nothing is durable-at-risk
  because the source is the truth and lives in program memory; the dictionary is
  a cache the pre-scan rebuilds. The rationale now lives in `DESIGN.md` so this
  is not re-litigated.

---

## 2026-07-15 — R6 audit fold: stage-F architecture, platform retarget, local labels

Origin: the Fable pre-execution audit (`FOR_THE_ARCHITECT_R6_preexecution_audit.md`,
verdict NO-GO) plus its upstream-delta addendum R6.1, the R4 architecture
interview's accepted decisions (recorded 2026-07-15, commit `2cc6b1d03`), and
three owner rulings the same day (`R6_RESOLUTION_PLAN.md` §1). This entry
records what the amendment pass changed and what it superseded.

**New DESIGN.md §10 (Stage F).** The R4 accepted architecture is now in the
authoritative document as DECIDED-unimplemented target state: F1 lifetime
foundations (pending-reset flag replaces generation-equality as the truth
predicate; active-frame guard; PEM single-step = fresh generation; dynamic
scan tracking; RECURSE; restore-time validator), F2 shared native parameter
decoder, F3 vocabulary/scopes/XEQ (supersedes the withdrawn Qwen prompt
R1-4), F4 textual parameters, F5 commit validation (E9's implementation), F6
capture submode. §9 was left unassigned — pre-renumber artifacts cite "§9.x"
meaning today's §8.x, and reusing the number would collide.

**Platform retarget (RULE-1).** Target is the R47 on DM42n (DMCP5); DM42 is
best-effort. Flash ceases to be a design veto — stages record the measured
`make dmcp5r47` delta instead. Knock-ons: `boundedRead` stays permanently
alongside the future restore validator (old Q2); the `FORTH ARENA:` suite
line is blessed as the §5.4 reporting mechanism and `bench/hwm.fs` demoted to
optional (old Q3). CLAUDE.md's target line updated to match.

**Named local labels (Q8 ruling).** Upstream b8f79e486 introduced named LOCAL
labels (kind byte 249 vs 253, `findNamedLabel(name, labelType)`,
position-sensitive in-program resolution, TAM `:` syntax). Ruled: Forth
incorporates them by **mimicking upstream** — `XEQ :NAME:` source form at
stage F3, `FTOK_XEQN` inline data adopts upstream's `[kind][len][name]`
payload with the kind byte passed verbatim to the native resolver, bare names
stay global-only, and a local request never falls through to Forth
vocabulary. §0.3, §2.2, §3.3.6, §4.1/§4.2 amended. The audit's AUD-U1 (the
tam.c hook predates `tam.colon` and lacks the gate) is scheduled as a code
fix. The per-program word scopes of F3 deliberately mirror the
`labelList[].program` pattern so both "local name" systems share one model.

**Falsified/stale text corrected (superseded wordings, for the record):**

- §8.4 E7 / §6 P-H2 claimed the fnPem cursor default "is already correct —
  delete any FORTH branch as dead code." Falsified by hardware: bare renders
  lack the two-byte quote the shared `+2` path assumes; the landed R3-1
  branch (`tam.function == ITM_FORTH` → `cursorInString = T_cursorPos - 2`)
  is now the documented contract. The old text would have deleted a live fix.
- Stale "required change / does not exist" claims rewritten as implemented
  invariants (R4's list): the prim-count `_Static_assert` (forth_prims.c:51),
  ASLIFT-on-exit, public push helpers, `forthPushInt32`'s long-integer store,
  the §3.3.7 emit/start/finish/abort API, the 0x6F00 count cap,
  `forthDictWriteName`'s clamped 3-arg form, the §5.4 BSS-vs-stack accounting.
- §3.2 pseudocode's two bare `return`s (prim-error, rstack guard) →
  `INNER_LEAVE()`; the committed code always did this correctly.
- §8.3's "generation wrap is harmless" — falsified by R4-E2's executed probe
  (65,536 bumps alias); annotated as a known bounded defect until F1.
- §8.4 E1's "do not fix REM by symmetry" rationale — upstream fixed REM
  itself in b8f79e486 (shallower two-pop); both fixes coexist, not unified.
  The E1 drain pseudocode now shows the landed bounded stack-wide form
  (59f58dbe3) instead of the unbounded `while(anyCatalogMenuOnStack())` that
  could spin when `popSoftmenu()` re-pushes HOME.
- §3.3 error-table row "C47 label in compile state → INVALID_NAME" annotated
  stage-interim (the §3.3.6/§4.1 FTOK_XEQN emission is the F3 target — the
  R6 audit's AUD-H3 fork resolved by marking which text is current vs target).
- §8.9 reworded per the Q1 ruling: unit analogs vs planned end-to-end paths
  made explicit; the harness is scheduled immediately after F1.
- §8.10 item 1 (cross-program visibility) closed as SUPERSEDED by F3 scopes;
  the "run-scoped, not program-local" paragraph now separates implementation
  (generation-global today) from contract (program-local under F3).
- §4.2 gained the Q4 interim ruling (NOPARAM dispatch of parameterized items
  via XEQ-by-name is documented behavior until F3's atomic-error rule) and
  the label-kind pins (GLOBAL_LABELS everywhere; AUD-U1 gate scheduled).
- §8.5 gained the Q5 ruling: the bare listing is deliberately contextual and
  non-injective; markers are the only type cue.
- R3-A2 (malformed-opcode walker) closed per Q6: upstream's
  `programBytesAvailable()` guards (including the computed-end check over
  PTP_REM payloads) cover the demonstrated class; no package-side renderer
  bound is added without new evidence.
- Post-migration citation refresh: LAST_ITEM 2860→2870, ITM_FORTH row
  4707→4722, REM row 3374→3391, slot 213 = MNU_FORTH (spares 214-219),
  `forthResolveXEQ` at forth_dict.c:420-456, config reset hook at :1957,
  scanLabelsAndPrograms 102-194/:734, EXIT/ALPHA/OFF rows, tam hook range.
  The §4.2 call-site map's numbers are marked drifted-by-design (anchors are
  the durable content).

**Also corrected in this pass, other documents:** CLAUDE.md target line;
`PROPOSED_SPEC_CHANGES.md` item 2 marked RATIFIED (it was folded into §3.3.6
on 2026-07-13 but still said PROPOSED); `Stage1.md` given a SUPERSEDED banner
(it still described the obsolete `forthArena`); `design-docs/package-manager/README.md`'s
stale "no package uses this system yet" limitation removed. **Correction to
this file's own 2026-07-14 entry:** it called `_saveProgram`/`fnLoadProgram`
a "binary" format; the format is textual (one decimal byte value per line) —
DESIGN.md §8.10 has been right about this since the R1 fold; the semantic
point (payload bytes opaque, lossless round-trip) was and is correct.

**Deferral bookkeeping (Q7):** PEM_FIX deferrals F8 (reject-path cursor
drift) and F9.1 (phantom marker on power-off mid-capture) are carried into
the Step-2 prompt backlog as bounded verify-then-fix-or-close tasks; they had
fallen out of all current tracking.

## 2026-07-16 — Stage-F roadmap completion: control flow, globals, and the word catalog get stages

Owner rulings (Stan, 2026-07-16), folded into §10 as class-2 amendments:

- **Control-flow words + `IMMEDIATE` → F3 (§10.3).** They had fallen through
  the stage numbering: §3.3.9 and §2.2 called them "future work / stage 2"
  (pre-F-series numbering), but §10's F1-F6 never claimed them, leaving
  `RECURSE` (F1-4) without conditionals to terminate on. Ruled into F3: the
  runtime tokens (`FTOK_BR`/`FTOK_0BR`) already exist and the F1-5 validator
  already covers their emissions; compilation shapes are settled at the F3
  design pass.
- **Global Forth words → F3 (§10.3), superseding §10.3's own "remain
  deferred".** Designed as the third reserved scope with the other two:
  searched after the current scope, survives top-level resets, persists with
  the validated save. Entry spelling, FORGET-class deletion, and §5.4
  arena-ceiling accounting are named as F3-design-pass sub-questions.
- **Dedicated Forth word catalog → F6 (§10.6),** closing §4.3's "future
  stage" pointer. Lands with the capture submode because its contents come
  from F3's scopes and its UI must integrate with the final capture entry
  model, not the interim alpha wrapper.

Status notes recorded in the same pass (`R6_RESOLUTION_PLAN.md`): the
report-only probes R6-4/R6-5 are complete (owner-confirmed; no tree change by
design — they were characterization probes). With these folds the accepted
implementation backlog is exactly F1→F6 (plus the F1.5 §8.9 harness); the
only post-series work is Step 8 housekeeping (freeList upstream MR, optional
upstream reports) and documentation reconciliation as stages land.

## 2026-07-17 — Stage F1 landed; §8.3 rewritten to the landed lifecycle; §8.9 item 9 reconciled

Stage F1 executed in full (commits `1834901d3` F1-1, `542972b32` F1-2,
`ecbd6bcce` F1-3, `2940a0f4f` F1-4, `04006089f` F1-5; ledger closeout
`10c04af4b`). Documentation reconciliation, first tranche:

- **§8.3 rewritten** from the pre-F1 interim (generation-equality truth, two
  scattered bump sites in `fnExecute`/`runProgram`, fixed 8-slot scan array,
  "resume keeps the generation", known-wrap defect) to the landed mechanism:
  pending-reset event as the sole truth, one `!nestedEngine`-gated signal
  site at `runProgram` entry, active-frame deferral on both signal and
  consumption, arena-backed first-touch records, R/S resume and SST as fresh
  lifetimes. The interim text survives in git history and in this file's
  earlier entries; the wrap defect is closed (executable proof
  `test_pending_reset_lifetime`).
- **§8.9 item 9(b) reconciled** to F1 semantics per the Q1 scheduling ruling
  (2026-07-15): same observable (`X == 9` after STOP + R/S), new pinned
  mechanism (fresh lifetime + first-touch re-derivation), a sharpened
  assertion (a word defined interactively during the pause is dropped), and
  a replacement mutation — the old "bump in `fnRunProgram` too" mutation is
  meaningless now that `fnRunProgram` reaches the sole signal site.
- §8.9 coverage note now points at the F1.5 stage ledger
  (`QWEN_PROMPTS_F15_harness.md`). Remaining §8.3-adjacent prose sweeps
  happen at F1.5 stage close.

## 2026-07-18 — F3 global-scope sub-questions ruled; F2 authored ahead

Owner rulings (Stan, 2026-07-18), folded into §10.3 as class-2 amendments,
closing the three design-pass blockers named in the 2026-07-16 fold:

- **Entry spelling:** postfix `GLOBAL` — an immediate-style word marking
  the latest closed definition as global, reusing the exact latest-entry
  mechanism F3 builds for `IMMEDIATE`. No new grammar.
- **Deletion:** classic `FORGET <name>`, truncating the global scope at
  the named word; not-a-global is an error.
- **Arena accounting:** same arena, same §5.4 ceiling; definition-time
  exhaustion is ordinary dictionary-full; the §5.4 report splits global
  vs transient high-water.

Same pass (owner pacing instruction): stage F2 was fully authored ahead of
execution — trace + ledger (`QWEN_PROMPTS_F2_core.md`) and four
gate-locked packets — and the F1.5 harness packet list was completed
(F15-5). F3's remaining pre-work before packets: the control-flow
compilation-shape design pass, the R4-C2 label-grammar trace, and the
F1-5 validator XEQN extension spec, all against the post-F2 tree.

## 2026-07-18 — F15-4 debug: capture-drive contract pinned; §8.9 item 5 mutation replaced

The F15-4 run exposed two spec-side defects, both fixed in the landed test
(`6775252bf`) and reconciled here:

- **Capture-drive contract (now explicit):** typing into a Forth region in
  a test drive requires the ALPHA gesture (`runFunction(ITM_AIM)`) as the
  FIRST key, cursor on the OPENING marker, `pemCursorIsZerothStep` owned by
  the fixture. Mechanism: only the ALPHA arm of `insertStepInProgram`
  consults `forthEntryStateAtInsertion()` after `addStepInProgram`'s
  pre-move (governing predecessor = the opening marker); leading digits or
  `':'` consult it without the pre-move, see the RPN predecessor, and open
  number entry — behavior the landed F15-2 subcases pin as correct.
- **§8.9 item 5's mutation was falsified empirically** (mutation escape,
  gate stayed green): the pre-R1-3 assumption "no prim alias → ÷ is
  FUNCTION NOT FOUND" no longer holds — the §4.1 step 4 item fallback
  resolves the glyph to the native divide item. Item 5's mutation is
  replaced by the capture-store mutation (manage.c `itemSoftmenuName` →
  `itemCatalogName`). This closes the stale-§8.9-mutation sweep started
  with item 9(b): items 2/3/4/8 mutations are live in landed tests, 9(b)
  and 5 are reconciled, and no §8.9-derived mutation remains unexecuted.

## 2026-07-18 — Stage F1.5 COMPLETE; §8.9 item 10 mutation reconciled; double-guard recorded

All ten §8.9 items are now covered end-to-end (F15-1 `b773597bd`, F15-2
`5a9e9ce2d`, F15-3 `c8b87dfa8`, F15-4 `6775252bf`, F15-5 `546aa8b6c`); the
§8.9 coverage note is flipped to COMPLETE and a green gate certifies the
end-to-end contracts. Closing findings:

- **Item 10's mutation consequence was falsified in execution** (the third
  and last stale §8.9 mutation, after 9(b) and 5): re-routing the tam PEM
  branch to `insertStepInProgram(ITM_FCALL)` cannot put `0x8B 0x1B` in
  program memory because insertStepInProgram's own ITM_FCALL arm resolves
  the index back to a name and records an `ITM_FORTH` source step (or
  rejects). Name-faithful recording is therefore DOUBLY guarded — the tam
  H-hook records names directly, and the step inserter converts any
  index-bearing insertion back to a name. The re-route mutation is still
  detected by the name-step probe (F15-5 subcase 1 RED). The "no raw
  FCALL opcode" probe is declared-redundant defense in depth.
- The `vBodyWalk` BR/0BR-arm indentation nit (F1-5 cosmetic carry-over) is
  fixed; semantics byte-identical, gate-verified.
- Stage ledger closed out. Next per `FSERIES_ROADMAP.md`: the F2 queue
  (F2-1..F2-4, authored and gate-locked).

## 2026-07-18 — F3-3 packet defect: XEQ-name steps missed the scope model (amendment F3-3A)

The F3-3 implementation run STOPPED correctly on a real packet
contradiction: the packet required legacy tests to stay green while
operationalizing "scope tracks program-step execution" as the `ITM_FORTH`
source-step arm only.  An `XEQ 'name'` step executed from a running
program therefore resolved in INTERACTIVE scope and could not see its own
program's words — three param_core legacy tests red with
`ERROR_LABEL_NOT_FOUND` (6) from the fallback arm, exactly at
`paramCoreExecuteOp`'s forth-fallback site.  Two further legacy reds
(`test_recurse_compile_only` [5], `test_accept_run_lifecycle` [3]) were
harness-level `forthFindColon` introspection of program-owned words from
INTERACTIVE scope — cross-scope reads the new contract deliberately
rejects; their product assertions (RAM_FULL recursion; X==9 after resume)
already carry each test's original purpose.

Ruling (normative text added to §10.3): scope is a property of the
executing step.  Every step arm that resolves Forth names on a step's
behalf enters the owning program's scope through one shared primitive
(`forthScopeEnterProgramStep`/`forthScopeRestore` — generation check +
first-touch pre-scan + scan-record derivation, INTERACTIVE fallback for
non-program addresses) and restores on exit.  Scope guards name→ref
resolution only; by-ref execution (FCALL) and ref→name display stay
scope-free.  The reported "contradiction" with mutation 3 dissolves: the
per-source-step restore in `forthOuterRun` stays (mutation 3 intact), and
the XEQ arm gets its own enter/restore in `param_core.c`.

Amendment F3-3A (appended to `QWEN_PROMPTS_F3_3_scopes_live.md`) carries:
the shared primitive + `forthProgramStep` refactor onto it; deletion of a
tautological savedScope no-op the packet's item-3/item-4 ambiguity induced
in `forthOuterRun`; the param_core fallback-arm hook (resolve AND colon
dispatch inside the scope window, per the nested-evaluation-inherits
rule); the two named legacy assertion flips to isolation pins; fixture
step `sXeqA` + subcase 6 (cross-program XEQ-name rejection at the step
surface, `ERROR_LABEL_NOT_FOUND`, scope restored on the error path); and
mutation 5 (hook removal → `test_param_core_bounded_names` [1] RED — the
legacy positive is the detector, since subcase 6 rejects either way).
Consequences accepted: an XEQ-name step is now a first-touch site (a
forward `XEQ 'W'` before any source step of its program executes resolves
after pre-scan, matching §9.2's forward-reference promise); a program
XEQ-name step can no longer resolve interactively-defined words (mutual
invisibility, already pinned by subcase 3).  The §8.6 picker walk stays
unfiltered as the documented interim until the F6 catalog lands the
scope-aware listing.

## 2026-07-19 — F6 authored from traces; hardware bench deferred to stage-exit (owner ruling 2026-07-18)

Owner ruling (2026-07-18, "author the F6 packet without the hardware test
for now"): §10.6's audit precondition is split — the ARCHITECT half
(traces T1-T7) remains the authoring gate and was performed and folded
into `F6_AUDIT_RESULTS.md`; the HARDWARE half (`F6_KEYBOARD_PEM_AUDIT.md`
Blocks A-F) moves from authoring precondition to STAGE-EXIT confirmation,
re-run on the DM42n against the LANDED F6 behavior before the stage may
close.  §10.6 amended accordingly.  Rationale recorded with the traces:
every planned fixture is PC-build-derivable (the self-test is the gate);
the bench's unique value is DMCP-hardware divergence (key timing, deep
sleep, save timing), which a post-landing re-run still catches.  The
deferred-bench register (audit results, bottom table) maps each charter
experiment to its interim trace-derived substitute and residual risk.

Trace findings that shaped the design (full evidence in
`F6_AUDIT_RESULTS.md`): the landed capture wrapper re-commits the source
step after EVERY key (pemAlpha's fall-through tail), so the program step
always holds the typed text — power-off loses only cursor/open-flag, and
F5-2's commit gate already builds on this; `tamEnterMode` commits-and-
closes a non-empty capture line at its CM_PEM arm and TAM teardown
scrubs `FLAG_ALPHA` and zeroes `aimBuffer` in PEM — three independent
proofs the capture text cannot stay in `aimBuffer`; `tamEnterMode`
clobbers `tam.mode/function` BEFORE that arm, so suspend snapshots no tam
state (capture-era tam is deterministically {mode 0, function ITM_FORTH});
no screen.c site reads `aimBuffer` during PEM capture (the listing
renders the committed step), so the buffer move has zero display surface;
`fnKeyExit`'s CM_PEM arms pick abort-vs-commit by `aimBuffer[0]` and must
follow the sink (F6-1 Change D2 — found by trace, would have been a
regression); the §8.6 picker is dictionary-blind (text scan), pinning the
F6-5 delta.

Stage F6 authored 2026-07-19 as six gate-locked packets on the F5-2
commit (`QWEN_PROMPTS_F6_core.md` ledger; F6-1 capture object + managed
256-byte buffer, uniform alloc-on-open/free-on-close, interim TAM guard;
F6-2 TAM suspend/resume — suspension frees the buffer and resumes by
refilling from the step, offset-based step reference, single resume
choke point in `leaveTamModeIfEnabled`, uniform even for empty lines
(kills the landed TAM-over-open-capture edge); F6-3 catalog picks insert
`itemCatalogName` text (CAT_FNCT class = the §4.2 callable class); F6-4
suspended TAM commits convert to canonical text THROUGH THE LANDED
DECODER (mimicry = F4 parity), no-room keeps the step; F6-5 MNU_FORTH
becomes the union catalog — landed text-scan section + interactive fdict
+ gdict, browse surface reads owners directly per F3-3A, cross-section
duplicates show provenance; F6-6 acceptance battery + capture lifecycle
reset at the two `forthScanTrackReset` seams — deep-sleep wake
legitimately keeps capture open, matching landed `FLAG_ALPHA` behavior).
Authoring-base discipline: authored on the F3-2 tree four stages ahead;
of all files F6 touches, only `manage.c` is modified by the pending
F3-4..F5-2 queue (F5-2's single E9 line, which F6-1 re-points), so the
anchor-stability risk is one known line; every packet's execution gate
re-greps its anchors and the standing re-author-on-deviation rule
applies.

## 2026-07-19 — F6 adversarial review (pre-execution): five substantive defects fixed

An adversarial pass over the six authored F6 packets (owner-requested)
before any execution.  Substantive findings, all fixed in place:

1. **Suspend moved the cursor and would have displaced the TAM commit.**
   F6-2's `forthCaptureSuspend` stepped forward "so TAM's insert lands
   after the capture line" — but TAM commits insert via
   `addStepInProgram(tamOperation())` (traced: tam.c:217/552/587/896/918/
   1095), whose pre-move already places the insert after the current
   step; stepping forward would land the committed step one position too
   late.  Fixed: suspend does not move `currentStep` (the landed
   commit-and-close nets to cursor-on-the-line); position restore at
   resume retained; the retargeted mutation now deletes the resume's
   position-restore lines.
2. **Suspend zeroed `tam.function` and would have broken the TAM
   session.**  `tamEnterMode` assigns the incoming TAM function BEFORE
   the CM_PEM seam; the "capture-close reset parity" line would have
   clobbered it (the landed `pemCloseAlphaInput` reset at that seam is
   precisely the behavior suspend replaces).  Fixed: suspend leaves
   `tam` untouched.
3. **F6-3's item arm conflicted with F6-4 for parameterized items.**
   Inserting a bare name for a `PTP_*`-parameterized item would create a
   second entry UX beside F6-4's TAM path.  Fixed: the arm requires
   `PTP_NONE` (SIN traced `CAT_FNCT | PTP_NONE`, items.c:1879);
   parameterized items stay inert in `pemAlpha` — their capture UX is
   the suspend+convert path.
4. **F6-4's converted text lacked a word separator** when the cursor did
   not follow a space (`5 DUP` + STO → `5 DUPSTO 05`).  Fixed: the
   conversion prefixes one space when the byte before the cursor is
   neither space nor line start, plus a 255-byte decode clamp
   (keep-the-step fallback).
5. **Unsound free-RAM oracles.**  Whole-session `getFreeRamMemory()`
   equality asserts would red on program-memory growth (committed steps,
   and the restore path's own inherent footprint — the landed arena line
   records `freeRamDelta=64` post-restore).  Fixed: buffer-lifecycle
   equalities are scoped to net-zero-program-delta windows with a
   resize-quantum escape valve; the F6-6 restore subcase is differential
   against a no-capture restore baseline.

Fidelity corrections in the same pass: the F6-1 gate grepped a
nonexistent test name (`test_forth_picker*` — the tree's picker tests are
located by their quoted asserts instead); every "drive STO/XEQ/EXIT"
became the landed idioms (`tamEnterMode(...)` direct, `fnKeyExit(NOPARAM)`,
`addStepInProgram(ITM_FORTH)` for the toggle, `pemAlphaEdit(0)` for EDIT);
the 196-glyph fixtures type alternating `X`/space (a single 196-glyph
token could trip the E9 structural tier); F6-5's smudge fixture uses a
new `forthTestSmudgeSet` FORTH_DEBUG_SELFTEST hook instead of an assumed
header-poke idiom; F6-1's mutation 2 covers both `forthCapClose` sites
with subcase-10 co-red; leftover authoring artifacts (a thinking-aloud
mutation note, an imprecise C6 site count, reopen ambiguity between
subcases) cleaned.  Core-ledger decision 6 and the affected packet
rationales were rewritten to match; `F6_AUDIT_RESULTS.md` already carried
the correct trace facts.

## 2026-07-19 — F4-2 debug: `regInRange` is not silent (packet amendment F4-2A)

Non-normative. The F4-2 packet carried the traced claim that the native
`PARAM_REGISTER` arm's range gate is *silent* on a miss ("out-of-range is
SILENT"), and pinned it as subcase 4 (`STO .05` with no local registers
allocated → `ERROR_NONE`, X untouched).  The trace was wrong in its
consequence.  `regInRange()` (`src/c47/store.c:17-72`) is **not** a pure
predicate: on a miss it classifies the register and calls
`displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ...)` itself before returning
false (under `EXTRA_INFO_ON_CALC_ERROR` it also emits the
"In function regInRange: … is not defined!" info line).  The native arm's
`if(regInRange(regKStoC(opParam)))` therefore raises on a miss and merely
declines to *dispatch* — silence applies to the caller, not to the user.

Ruling: mirror the native call chain exactly.  `paramCoreValidateDirect`
keeps `regInRange(regKStoC(value))` for `PTP_REGISTER`, and the class is
the documented exception to the "validate sets no error" contract in
`param_core.h`.  Behavior pinned by the landed subcase 4: an unallocated
local raises `ERROR_OUT_OF_RANGE` and performs no store (X unchanged).
The traced-silence parity in §10.4 continues to hold where it was
actually traced — the `PTP_NUMBER_8` out-of-range arm — not here.

Two authoring defects in the same packet, found in the same debug pass:
the acceptance test's stack seeding, and its byte-image addressing.
`x_set_string` overwrites REGISTER_X with the source string and
`fnForthOuter` drops it, so any stack seeded by `forthPushInt32` before
the call is shifted one level before the line runs — a shuffle fixture
must ride its seed in the source line (`"11 22 33 44 <glyph> yxzt"`).
And `forthFindColon` yields a **ref index**, not a byte offset; byte-image
pins must walk from `fdict.latest` (the ref-as-offset error is invisible
for the first word defined after a clear, where both are 0 — which is
exactly why three of the five image pins passed and two did not).

## 2026-07-19 — F4-3 landed after a rewrite: one marker table, one cell-span, one decode body

Non-normative. F4-3's implementation pass produced a working but
structurally unsound shape — the marker grammar restated per class, seven
copies of the 253/255 decoder in `forthInner` (one of them defined *inside*
the switch as a GCC nested function), six copies of the emit block in the
compiler, and the same grammar spelled a fourth and fifth time in the two
`forthDictMakeLatestGlobal` walks. It was rewritten around two functions
before landing, and §10.4 now records the invariant: `forthParamMarkerMask`
is the only statement of which markers a class accepts, and
`forthParamCellSpan` the only statement of the cell grammar. Everything else
— `decodeMarkerCell` + `forthParamMarkerDispatch` (runtime),
`parseMarkerForm` + `emitOrRunMarkerForm` (compiler), all three walks — reads
them. Net effect beyond hygiene: the compiler, the runtime, and the validator
cannot drift, which is exactly the drift F2-3 was created to close.

Four substantive defects fixed in the same pass:

1. **Invented surface.** The interim implementation accepted an ASCII `->`
   spelling, an `→RNN` register form, and a `.NNN` three-digit system-flag
   form — none of them traced, all of them contradicting the packet's V4
   non-goal (the typeable surface is exactly the arrow glyph `\xa1\x92` and
   the ASCII quote `0x27`). Removed.
2. **`compareString` used as a truthiness test.** It returns 0 on equal
   (sort.c:70), so the system-flag reverse map matched every name except the
   right one. Compounded by `parseQuotedName` not NUL-terminating its output
   — the map compared a name against unterminated stack bytes.
3. **NUMBER_8 / NUMBER_8_16 indirection never reached the parser**: the
   numeric arm rejected any non-digit token before the marker forms were
   tried. The arm now falls through to `parseMarkerForm`.
4. **An unsound acceptance pin.** The packet asked the validator to reject a
   `{254, 5}` cell on a NUMBER_16 item. It cannot: for that class the cell IS
   the legal value 1534. The exclusion is a compile-time rule and is pinned
   there; the validator subcase pins what walks really enforce (len 0,
   non-zero pad, name extent past the body) plus one well-formed ACCEPT so
   the RESET pins are not vacuous.

Also recorded: the acceptance test as first written papered over its own
named-variable leak by assigning `numberOfAllocatedMemoryRegions` back to its
start value. The landed test unwinds what it allocated instead (data block
per variable plus the header table, back to the pre-test count) — upstream
has no delete-named-variable API, so the teardown is explicit. Never satisfy
a gate by writing to the counter it reads.

Cost: `make dmcp5r47` flash 1090256 → 1092176 (+1920 bytes), RAM unchanged
at 7188, Forth arena unchanged from the F4-2 baseline. All five packet
mutations verified RED and restored; amendments F4-3A and F4-4A appended to
their packets, and the four fixture rules that cost red gates in F4-2/F4-3
(x_set_string vs seeded stacks, forthFindColon returning a ref index, per-
subcase error clearing, allocation teardown) are now repeated verbatim in
F4-4, F5-1, F5-2, and F6-6 — packets are pasted standalone, so a shared
reference would not have travelled.

## 2026-07-19 — F5-2 debug: check mode restored the scope from an uninitialized field

Non-normative. F5-2's production change — the six-line `forthCheckSourceLine`
call in `pemAlpha`'s ITM_ENTER arm — was correct as authored. The gate went
red in four tests the packet never touched (F3-3 scope isolation, the F4-4
parity sweep, F5-2's own hygiene subcase), all reporting a nonsense
`forthCurrentScope` such as `0x800E`.

Root cause was landed in F5-1. `forthOuterRun`'s epilogue restores
`forthCurrentScope = ctx->savedScope`; its prologue fills `savedDef` and
`savedLatestClosed` but leaves `savedScope` to the caller, and every entry
point snapshots it — `forthOuterInterpret`, `fnForthOuter`,
`forthProgramStep`, the pre-scan — except `forthCheckSourceLine`, which set
only `ctx.source`. Check mode therefore wrote an uninitialized stack word
into the live scope on every call. Invisible while its only caller was
F5-1's own test (which never observed scope); a suite-wide poison the moment
check mode was wired into the commit seam. One line fixes it.

The interesting part is why it was allowed to land. §10.5 states check mode
"executes nothing, allocates nothing, mutates no live state" — a normative
claim F5-1 shipped with no pin at all; its tests only read verdicts. The
landed correction adds `test_check_source_line` subcase 6, which pins the
CONTRACT: scope set to a non-default `0x1234`, both an accepted and a
rejected line checked, then scope, open-definition state, rsp, and the
dictionary asserted unchanged — with `poisonAutoFrame()` filling the callee's
future stack frame with `0xAA` so an uninitialized restore reports a
deterministic `43690` rather than whatever the previous call left behind.
`forthTestScopeSet` (FORTH_DEBUG_SELFTEST only) exists for this pin.

**Rule now binding on every packet:** an entry point whose specification
includes a state-neutrality claim must pin that claim directly, from a
non-default state, over both its accepting and its rejecting path. A verdict
pin is not a contract pin. Two process rules were added alongside (recorded
in amendment F5-2A and repeated verbatim in the remaining F6/FIX-6 packets):
a red in a test the packet did not write is an immediate STOP with zero
repair attempts, and the blast radius is named by diffing the pre-gate and
gate PASS sets rather than by reading failure text.

Cost: `make dmcp5r47` flash 1092216 → 1093016 (+800 bytes), RAM unchanged.
The delta exceeds the call site because F5-1's check-mode code had been
unreachable and LTO was dropping it; this is the true cost of E9 tier 1
going live.

Measurement note (corrected): the shadow tree symlinks upstream files AND
`files/` entries, so edits to those are live; only PATCHED files are
materialized copies, refreshed at `meson setup`. `build.dmcp5` is a plain
directory target, so once it exists `make dmcp5r47` skips the setup recipe
entirely — `f=1` is not the discriminator — and only a CUSTOM_PKG change or
`CUSTOM_PKG_RECONFIGURE=1` forces a reconfigure. Rebuilding after swapping
package sources therefore yields a chimera (live `files/` edit + stale
patched copy), which is what produced two plausible-but-meaningless flash
numbers here before the reconfigure was forced. `build-test.sh` always
reconfigures, so the self-test gate is never affected.

## 2026-07-19 — F6-3 mutation execution: packet mutation 2 falsified for SIN (second occurrence of the F15-4 pattern)

Non-normative. F6-3's item arm (`manage.c`, ALPHA-mode item dispatch) is
correct as authored: it inserts `indexOfItems[item].itemCatalogName` for a
catalog/menu pick made while a Forth capture line is open, gated to
`CAT_FNCT | PTP_NONE` — the design choice traced and reviewed in the
"F6 adversarial review" entry above (finding 3) and stated directly in the
"F6 authored from traces" entry ("F6-3 catalog picks insert
`itemCatalogName` text"). Nothing here changes that.

The packet's required mutation 2 ("replace `itemCatalogName` with
`itemSoftmenuName`; subcase 1 MUST go RED if the two spellings differ for
SIN") stayed GREEN on first execution. Cause: `items.c:1859` gives
`ITM_sin` identical catalog and softmenu spellings (`"SIN"`, `"SIN"`), so
the field swap is a no-op for the item subcase 1 happens to drive — the two
fields simply never diverge for this particular item, independent of which
one the production code reads. This is the same failure shape as the
§8.9 item 5 mutation the F15-4 entry above already documents (there,
`PRIM_DIVGL` deletion escaped once R1-3 made the alias redundant; here, a
field-selection mutation escapes because the probe item's fields coincide)
— the packet itself flagged the risk in advance and required a STOP rather
than a silent accept, exactly because this pattern had already been seen
once.

Fix, mirroring the F15-4 resolution (retarget the probe, not the
production code): `test_capture_menus` subcase 1 now additionally drives
`ITM_arccos` (81) after the SIN checks pass. `items.c:1864` gives it
genuinely divergent fields — catalog `"ARCCOS"`, softmenu `"ACOS"` — under
the identical `CAT_FNCT | PTP_NONE` classification SIN carries, so the
field-swap mutation is now observable (`"1 SIN ARCCOS "` becomes
`"1 SIN ACOS "` under the mutation). Subcase 2's expected string is updated
to match the longer buffer (`"1 SIN ARCCOS 2"`); subcases 3-6 compare
against a captured `textBefore` or open a fresh line and were untouched.
No production code changed; mutation 2 re-run RED after the retarget.

## 2026-07-20 — F6-6 acceptance battery: two pre-existing save/restore-vs-allocator gaps found and routed around

Non-normative. F6-6 adds `forthCapPowerReset()` (`forth_capture.c/h`) and
wires it into the two lifecycle seams `forthDictInit()`/`forthDictClear()`
already call `forthScanTrackReset()` from, so a re-initialized or restored
machine always starts with the capture CLOSED and its buffer freed —
matching the landed rule that capture cannot outlive the dictionary
lifecycle (deep-sleep wake does not run these seams; a sleeping capture
legitimately survives, same as FLAG_ALPHA today).

Authoring `test_capture_acceptance` subcase 4 (restore lifecycle closes an
open and a suspended capture) surfaced two genuine, pre-existing gaps
between `saveRestoreBackup.c`'s restore path and the block allocator,
neither previously exercised because no earlier F-series test drove a
save/restore round-trip with a Forth capture actually open at save time:

1. `restoreCalc()` restores `numberOfFreeMemoryRegions` /
   `freeMemoryRegions[]` / `numberOfAllocatedMemoryRegions` /
   `allocatedMemoryRegions[]` wholesale from the backup file — the entire
   allocator tracking state, independent of anything Forth-related. With a
   capture genuinely open at save time, this leaves the capture buffer's
   address range overlapping a restore-time free region; the lifecycle
   seam's own `forthCapClose()` free is then correctly rejected by
   `freeListFree`'s double/invalid-free guard, orphaning the buffer's
   blocks. Confirmed via 3 independent mitigation attempts (fresh fixture,
   pre-inflation padding, direct A/B toggling) that this is a real conflict
   in production code, not a test-fixture artifact.
2. Independently of (1) — confirmed by temporarily disabling just this
   round-trip with no capture ever open — a `saveCalc()`/`restoreCalc()`
   round-trip alone measurably shifts `numberOfAllocatedMemoryRegions` by
   +1 relative to pre-save state.
3. `systemFlags0`/`systemFlags1` (carrying `FLAG_ALPHA`) restore verbatim
   well after the dict-lifecycle seam runs, and the restore path's own
   generic alpha-clear is conditional on a catalog also having been open —
   never true for a Forth-only capture. Any `FLAG_ALPHA` clear attempted in
   the seam is silently overwritten moments later. `forthCapPowerReset()`
   deliberately does not touch `FLAG_ALPHA` for exactly this reason (see
   its doc comment); subcase 4 asserts capture state only, not the flag.

All three are pre-existing architectural gaps in the save/restore-vs-
allocator interaction, out of scope for F6-6 to fix (a blind retry-free
would be unsafe). Both phases of subcase 4 that would otherwise hit (1) or
(2) through a full round-trip are written instead against the same direct
`forthGDictValidateRestored(); forthDictInit();` pair the seam itself
runs — proving the seam closes an open/suspended capture leak-free without
routing through the unrelated allocator-restore defects. This is a
deliberate deviation from the packet's literal "run the landed F15-2
power-off round-trip idiom" wording, made so the test proves the seam
correct without also proving-or-failing-on bugs the seam cannot fix.
Flagged here for the forth-core code audit; (1) and (2) remain live in
`saveRestoreBackup.c` today.

One direct test bug, found and fixed during the same debugging: subcase
4's Phase 0 (baseline `freeBase` measurement) called
`forthGDictValidateRestored(); forthDictInit();` directly, same as Phases
1 and 2, but — unlike those two — with no `forthDictClear()` immediately
before it. `forthDictInit()` nulls `fdict.base` without freeing (by
design: it assumes a genuine cold boot, where `fdict.base` is already
NULL). Subcase 1 leaves fdict holding a live "SQ" allocation; nothing
between subcase 1 and subcase 4 clears it; Phase 0's direct call silently
leaked those 8 blocks, surfacing as a suite-wide +1
`numberOfAllocatedMemoryRegions` at the gate's final check. Fixed by
adding the same hygiene `forthDictClear()` Phases 1 and 2 already carry.
Required mutation (delete both `forthCapPowerReset()` seam calls) re-run
RED, specifically at subcase 4 ("phase 1 capture not closed after
restore"); reverted, gate re-run green. `make dmcp5r47` flash
1094400 → 1094456 (+56 B, the new function body plus two call sites); RAM
(data+bss) unchanged at 7228; fdict/gdict layout unchanged (no new
fields, no growth-behavior change).

## 2026-07-20 — Test-suite audit: a third pre-existing production bug, real user-facing data loss in TAM cancel (fixed)

Non-normative. Owner-directed audit of the entire forth-core test suite
(after the F6 series landed): every `static int test_*` function in
`test_dict_reloc.c` reviewed for toothless assertions, weak oracles, and
bad design, in parallel by seven independent review passes covering
disjoint line ranges plus a manual pass over the orchestrator and a
handful of functions. Found and fixed ~14 genuine rigor defects (missing
`fail = 1` before two `goto cleanup` sites in `test_accept_xeq_name_step`
that let real FAIL-printf paths return pass; a `test_dict_space_full`
assertion that checked "some error" instead of the specific
`ERROR_RAM_FULL` its own name claims; `test_malformed_token`'s three
subcases checking only "not the sentinel" instead of "the original
value," so a third, unanticipated post-error value would pass;
`test_div_zero_halt` similarly checked only "not the sentinel 999," and
its own comment claimed the missing check should be "X == 42" — adding
that literally FAILED the gate, because it was never true: traced
through `src/c47/mathematics/division.c` (`divLonILonI` converts both
operands to `dtReal34` in place before a zero-divisor ever raises
`ERROR_ARG_EXCEEDS_FUNCTION_DOMAIN`, in `divRealReal`, so X is left at
its already-converted `real34` value, not restored to the original
longint) and `registers.c`'s `adjustResult`/`undo()` (the restore-on-error
path reads a `SAVED_REGISTER_X` checkpoint this Forth-driven call never
populates via `saveForUndo()`, confirmed empirically rather than by
tracing every `saveForUndo()` call site) — corrected to assert what's
actually true and meaningful (X is real34 zero) instead of a stale,
never-verified claim in a decade-old comment; a dead tautological
`nativeOk`/`forthOk` check in
`test_param_series_c_acceptance` removed; `test_check_source_line`
subcase 3's state-neutrality pin extended to cover `gdict.latest`
(previously only `gdict.here`/`count`, asymmetric with the `fdict` triple
it mirrors); `test_param_register_flag` subcases 1 and 3 gained
byte-image pins for the compiled parameter cell — subcase 3 (`STO M`)
particularly, since a round-trip alone can't distinguish a correct
`REGISTER_M_IN_KS_CODE` (211) from a transcription error in the
hand-maintained `paramLetterKS[]` table that swaps two stat letters,
because both STO and RCL read the same wrong row; `test_reentrancy`'s
depth-cap check now primes a sentinel before the guarded call instead of
coincidentally relying on whatever the previous test left in X;
`test_c47_param_shared_dispatch` subcase 3 (PTP_NONE dispatch) now checks
the result actually became `dtReal34` instead of only "no error," so a
future regression that silently skips dispatch for parameterless
functions can't hide behind "no error, no observable effect" either;
`test_capture_menus` subcase 3's `!= -MNU_FORTH` check tightened to the
specific `== -MNU_ALPHA` its own PASS message already claimed;
`test_word_catalog` subcase 4 now confirms the three pre-existing catalog
entries (PW/WI/GVIS) survive an over-long-name insert untouched, not just
that the count stayed 3 and the truncated spelling is absent.

One finding was NOT a test-rigor issue but a real, traced, production
defect: `test_capture_suspend` subcase 2 ("Cancel round-trip") asserts
`forthTestCapText() == "5 DUP"` after a TAM cancel that immediately
follows subcase 1's TAM commit — but subcase 1 ends with the line reading
`"5 DUP STO 05 "` (F6-4's fold-to-text landed that), and the F6-2 packet's
own subcase 2 spec says "text intact," never updated after F6-4 changed
what "intact" should mean. The literal was never a design choice; it is
what `forthCaptureResume()` (`programming/manage.c:1093-1144`) actually
produces, and that IS a bug: `forthCaptureSuspend()`
(`programming/manage.c:1072-1091`) snapshots `stepOffset` assuming the
on-disk step already mirrors `forthCapBuf()` — true after ordinary
keystrokes, which recommit incrementally via `pemAlpha`'s glyph-editing
tail (`manage.c:1011-1039`), but NOT true here: subcase 1's F6-4 fold
wrote into `forthCapBuf()` via `forthCapInsertName()` without recommitting
the on-disk step (the fold loop only calls `deleteStepsFromTo` on the
now-redundant inserted step, never touches the capture step itself). The
next suspend reads a stale on-disk snapshot and resume silently drops
everything typed or folded since. Reproducible on real hardware: type
text, do one TAM operation (STO/RCL/GTO/XEQ/...), then immediately start
a second one and cancel it — the first operation's folded text vanishes
from the line. Traced and confirmed by direct code reading (both
`forthCaptureSuspend`/`forthCaptureResume` and the F6-2/F6-4 packet
history), not left as a review agent's unverified claim.

Originally left unfixed at test-audit time, deliberately: a correct fix
requires resyncing the on-disk step from `forthCapBuf()` before
snapshotting the offset in `forthCaptureSuspend()`, so the invariant
ordinary keystrokes maintain holds unconditionally — but that first
requires tracing `_insertInProgram`'s relocation/cursor-advance/rescan
behavior (`manage.c:697-755` — it can trigger `resizeProgramMemory`,
shifts the cursor forward past the inserted bytes, and the existing
recommit call site steps back via `findPreviousStep` afterward to
compensate) closely enough to reuse or mirror it safely. `test_capture_suspend`
subcase 2 was left asserting the then-current (buggy) value, with an
in-code comment citing this entry, rather than asserting the correct
value and reddening the gate without an accompanying fix.

Fixed as the opening item of the forth-core code audit that followed
this test-suite audit, once the required `_insertInProgram` trace was
done. `forthCaptureSuspend()` (`programming/manage.c`, now starting
around line 1072) gained a recommit block at its top, mirroring
`pemAlpha`'s own glyph-editing recommit tail (`manage.c:1011-1039`):
`deleteStepsFromTo(currentStep, findNextStep(currentStep))` removes the
stale on-disk capture step, a fresh `ITM_FORTH` step is built from the
current `forthCapBuf()` contents and reinserted via `_insertInProgram`,
and `--currentLocalStepNumber; currentStep = findPreviousStep(currentStep);`
compensates for `_insertInProgram`'s internal cursor advance to land
back on the refreshed step — the same idiom `pemAlpha` already uses,
simplified because a capture step's opcode is always the 2-byte
`ITM_FORTH` form, so the generic `aimFunc` branching `pemAlpha` needs
can be skipped. This recommit runs unconditionally at the top of
`forthCaptureSuspend()`, before the existing cursor/localStep/stepOffset
snapshot, so the on-disk step is guaranteed current regardless of
whether the caller path was an ordinary keystroke (already fine) or an
F6-4 fold-to-text with no intervening keystroke (previously stale).

`test_capture_suspend` subcase 2 now asserts the correct
`"5 DUP STO 05 "` (matching subcase 1's post-fold text) instead of the
buggy `"5 DUP"`. Mutation-tested: temporarily wrapping the new recommit
block in a `/* MUTATION: ... */` comment (disabling it) reran the gate
RED, specifically failing `test_capture_suspend` subcase 2 with the
expected "text lost" symptom; reverting the mutation reran the gate
GREEN. Unlike the two F6-6 findings (self-inflicted by test fixtures
never-before exercising a capture-open save/restore, arguably low
real-world likelihood), this bug was reachable from ordinary keyboard
use with no test harness involved — type text, do one TAM operation
(STO/RCL/GTO/XEQ/...), then immediately start a second one and cancel
it, and the first operation's folded text would vanish from the line.
That is now fixed. `make dmcp5r47` flash 1093608 → 1093744 (+136 B,
text only, measured via `size` on `R47.elf` and confirmed by the
identical delta on `R47_flash.bin`); RAM (data+bss) unchanged at 7228.

## 2026-07-20 — Code audit #2: doFnReset hook reorder fixes a false-positive double-free diagnostic; a deeper allocator-vs-restore leak remains open

Non-normative. Second code-audit item, opened while investigating the two
F6-6 save/restore-vs-allocator gaps flagged above. Traced further than the
F6-6 entry's own hypothesis and found the actual mechanism differs from
what was suspected there.

**What was found and fixed.** `doFnReset()` (`config.c:1523`, upstream,
called both by a plain user RESET via `fnReset()` and unconditionally as
the first statement of `restoreCalc()`) wipes all allocator bookkeeping
early in its body — `memset`s `ram[]`, collapses `freeMemoryRegions[]` to
one giant free region, zeroes `numberOfAllocatedMemoryRegions` — and only
~400 lines later called the Forth reset hook (`forthDictInit()`/
`forthGDictInit()`, forth-core's own addition to this function, landed
under DESIGN.md §6). `forthDictInit()` calls `forthCapPowerReset()` →
`forthCapClose()` → `freeC47Blocks()` on a still-open capture buffer — but
by the hook's original (late) position, the earlier wipe had already
zeroed the bookkeeping and folded the capture's address range into one
giant free region, so `freeListFree`'s (FIX-6) double-free guard rejected
the free as a spurious overlap and printed an alarming
"double/invalid free" diagnostic every time. Reproduced live: driving a
direct `saveCalc()`/`restoreCalc()` round-trip with a capture open (no
existing test exercised this — `test_capture_acceptance` subcase 4
deliberately avoids the real round-trip, see the entry above) printed
`64 blocks at address 715 overlap free region [715..809)`, decoded via
`addr2line` on the actual crash backtrace to confirm the call site.

Fixed by moving the `forthDictInit()`/`forthGDictInit()` calls in
`doFnReset()` to before the RAM wipe instead of after (`config.c`, the
`else` branch's opening statements). Verified safe by inspection: nothing
between the function's entry and the wipe, or between the wipe and the
hook's old position, reads or depends on fdict/gdict/capture state
(grepped the ~400-line span for `forth`/`fdict`/`gdict`/`Capture` —
nothing). Gate green; `make dmcp5r47` flash delta is **0 B** (text/data/bss
all byte-identical before and after, confirmed via the stash-based A/B
methodology) — expected, since this is a pure statement reorder, not new
code.

**What this fix does NOT do — a real leak remains, found while writing a
regression test for the above.** The reorder is diagnostic-only, not a
capacity fix. `doFnReset()`'s RAM wipe (`memset`/`freeMemoryRegions[0]`
reset/`numberOfAllocatedMemoryRegions = 0`) runs unconditionally
immediately after the hook regardless of which ordering is used, so by
the time the wipe finishes, allocator state is identical either way —
the free succeeding vs. being rejected only matters for the split second
before the wipe overwrites everything, which is exactly why the fix has
zero flash/behavior footprint beyond silencing the diagnostic. Later in
the SAME `restoreCalc()` call, `restoreStateValue(numberOfAllocatedMemoryRegions/
allocatedMemoryRegions, ...)` (`saveRestoreBackup.c` lines ~833-836)
restores the allocator's allocated/free-region arrays **wholesale from
the backup file** — and that file's own snapshot, taken at `saveCalc()`
time while the capture was genuinely open, still marks the capture's
blocks as allocated. That restore silently reintroduces a phantom
"still allocated" entry that nothing will ever free again (`forthCap.buf`
is already `NULL` by then, from the hook's earlier, now-successful free —
`forthCapClose()` unconditionally nulls it regardless of whether the
underlying `freeC47Blocks` succeeds), because the later seam call at
`saveRestoreBackup.c:872-873` (`forthGDictValidateRestored();
forthDictInit();`) finds `forthCap.buf == NULL` and no-ops.

Confirmed via a real round-trip test (built during investigation, then
reverted rather than committed — see below): `getFreeRamMemory()` before
opening a capture vs. after a full `saveCalc()`/`restoreCalc()` round-trip
differs by exactly 256 bytes (64 blocks, the capture's own size) in both
the pre-fix and post-fix trees alike — this reorder changes nothing about
that number. Also worth recording precisely, since it cost real
investigation time: three unrelated "double/invalid free" diagnostics
that appeared to coincide with this test's own restore call turned out,
on `addr2line`-decoding their backtraces, to be the FIX-6
`test_freelist_double_free_guarded`/`test_freelist_interior_double_free`/
`test_freelist_no_mutation_on_oversize_free` tests' own *intentional*
guard triggers, running later in the suite — their position in the
captured log was a stdout/stderr buffering artifact (stdout is fully
buffered once redirected to a file; stderr's `fflush` calls inside
`freeListFree` are not), not a real finding. Lesson: when bisecting by log
position across a mixed stdout/stderr capture, decode backtraces before
trusting apparent ordering.

**Root architectural gap (not fixed here, tracked separately for
possible upstream reporting, similar treatment to the FIX-6 precedent):**
`forthCap` is deliberately not part of the persisted save-file state (by
design — a capture cannot survive a real save/restore, `forthCapPowerReset()`
exists specifically to guarantee it's closed at the two dictionary
lifecycle seams). But the save file's allocator-bookkeeping snapshot
(`numberOfAllocatedMemoryRegions`/`allocatedMemoryRegions[]`, saved and
restored wholesale, with no per-entry provenance) has no way to know that
one of its "allocated" entries belongs to state that must NOT be treated
as restored. Nothing today reconciles "ephemeral, intentionally-not-
persisted allocation" against a wholesale bookkeeping-array restore for
ANY such allocation, not just Forth's capture — this is a general gap in
the save/restore-vs-allocator design, exposed by Forth because it is
currently the only subsystem with an allocation of this shape. Net effect
on real hardware: every power-off/power-on cycle (or explicit save/restore)
performed while a Forth capture is open permanently leaks 256 B (64
blocks) of RAM that can never be reclaimed without a full RESET. Given R47
target RAM budgets, this is a real, if slow-accumulating, defect worth
raising — deferred rather than fixed inline, since a correct fix likely
needs either the save format to stop persisting ephemeral allocations at
all, or the restore seam to reconcile stale bookkeeping entries against
what Forth actually still owns post-restore, and both are upstream-shaped
changes bigger than this stage's scope (Owner ruling 2026-07-20: trace
and write up for potential upstream reporting, do not attempt a local fix
without further direction — same treatment as the FIX-6 precedent).

## 2026-07-20 — Code audit #3: dynamic-menu/USER-key XEQ of a Forth word executed live in PEM instead of recording a step, three call sites

Non-normative. Third code-audit item, found during the broader sweep of
forth-core's patched (non-owned) production files after the save/restore
items above were resolved.

**The bug.** Three separate call sites implement "XEQ a name picked from
elsewhere" against upstream's own established shape: resolve the name to
a native label first; if that succeeds, branch on `calcMode == CM_PEM`
— insert an `insertUserItemInProgram(item, name)` step while composing a
program, or `reallyRunFunction(...)` immediately otherwise (this exact
label-case shape predates forth-core; it's upstream's own pattern for
"a name picked from a live UI surface"). forth-core's own H-hook additions
at each site — the Forth-vocabulary fallback that runs when the name
*isn't* a native label — copied the label case's dispatch call but not
its PEM/execute branch, so a Forth colon word or plain item resolved via
this fallback was **always executed immediately**, in every calcMode,
including CM_PEM:

- `items.c`, `runFunction()`'s dynamic-menu XEQ dispatch (`dynamicMenuItem
  >= 0`, the MNU_FORTH-picker-driven path): `FORTH_XEQ_COLON` and
  `FORTH_XEQ_ITEM` both called `reallyRunFunction(...)` unconditionally,
  while the `FORTH_XEQ_LABEL` arm three lines above correctly checks
  `calcMode == CM_PEM`.
- `screen.c`, `_executeItem()`'s `FLAG_USER`-key XEQ dispatch: the H-hook
  Forth fallback (`forthFindColon` → `reallyRunFunction(ITM_FCALL, widx)`)
  ran unconditionally; the native-label branch immediately above it
  correctly checks `calcMode == CM_PEM`.
- `keyboard.c`, `btnReleased()`'s `FLAG_USER`/`Norm_Key_00_released`-key
  XEQ dispatch: identical shape, identical gap.

Net effect: editing a program (CM_PEM) and triggering any of these three
XEQ paths against a name that resolves to a Forth colon word (all three
sites) or a plain Forth-visible item (items.c only) executed that code
live — mutating the stack/registers/flags mid-edit — instead of recording
the intended `XEQ 'NAME'` program step. This directly violates DESIGN.md
§4.2's own normative contract ("PEM recording of `XEQ 'NAME'`: names
persist, never `widx`") — a contract the *canonical* TAM-typed entry path
(`ui/tam.c:964`) already honors correctly; these three dynamic-menu/
USER-key paths just never got the same treatment when the H-hooks were
added. No test exercised any of the three (grepped `dynamicMenuItem` /
`FLAG_USER` XEQ-adjacent test code — only defensive `dynamicMenuItem = -1`
hygiene assignments in unrelated tests, nothing driving a real selection
through this dispatch).

**Fix.** All three sites gained the identical `if(calcMode == CM_PEM) {
insertUserItemInProgram(...); } else { <original live-execute call>; }`
wrapper around their Forth-fallback dispatch, mirroring the native-label
arm immediately adjacent to each — same idiom, no new abstraction, three
small near-identical edits rather than a shared helper (each site's
surrounding variable names/control flow differ enough that factoring
would cost more clarity than it saves for three call sites). Verified
`insertUserItemInProgram(func, name)` records the same generic
`[opcode][STRING_LABEL_VARIABLE][len][name]` step regardless of what
`func` will resolve to later — confirming the fix doesn't need to know
*which* Forth-fallback subtype (colon vs. item) it's handling, exactly
like the pre-existing label arm doesn't need to know which label it
resolved.

**Regression test:** `test_pem_xeq_dynmenu_no_live_exec` (new,
`test_dict_reloc.c`) drives the `items.c` site end-to-end — compiles `W7`
interactively (fdict-resident), builds a real `MNU_FORTH` picker over a
minimal program via `testInitVariableSoftmenu`/`showSoftmenu(-MNU_FORTH)`,
selects it via `dynamicMenuItem`, calls `runFunction(ITM_XEQ)` with
`calcMode == CM_PEM`, and asserts a step was recorded
(`getNumberOfSteps()` +1) and a sentinel left in X survived untouched (no
live execution). Mutation-tested: disabling the `calcMode == CM_PEM`
check in `items.c`'s `FORTH_XEQ_COLON` arm reran the gate RED with the
exact predicted symptom ("X changed — word executed live instead of being
recorded"); reverting reran GREEN. The screen.c/keyboard.c sites were
fixed by inspection/mirroring this one's verified shape and are not
independently test-driven — both are `FLAG_USER`-key dispatch paths that
would need a materially larger fixture (real key-assignment state) to
drive through the actual UI entry point; the fix at each site is
structurally identical to the tested one and reuses the exact same
`insertUserItemInProgram` contract already proven correct in DESIGN.md's
canonical TAM path and by this test.

`make dmcp5r47` flash 1093728 → 1093776 (+48 B, three small conditional
branches); RAM (data+bss) unchanged at 7228.

---

## 2026-07-25 — Simplification pass S1/S2/S3: the separate capture buffer was a vestige and is gone

Three stages reducing this package's coupling to upstream, prompted by an
audit of the whole override set rather than of any one defect. No Forth
feature was removed. Upstream override files 17 → 13 (the resolver work
also left `screen.c` a 2-hunk shell); patch added-lines 937 → 634.

**S1 — evict what was never forth-core's.** The enhanced
missing-function error text (`error.c` in full, plus its `screen.c`
branch) is dropped on owner ruling: the error still raises and the
offending name is still written to `errorMessage`, only the on-screen
rendering of the name is gone. `config.c`'s unconditional
global-register descriptor memset is a generic upstream fix and moved to
`UPSTREAM_REPORTS_globalRegister_reset.md`;
`test_lifecycle_real_reset_hook` drops the poison/assert pair that
pinned it. Six lines of gratuitous whitespace churn reverted.

**S2 — move package logic out of upstream files.** The 157-line
`MNU_FORTH` picker builder (inside `softmenus.c`) and the
insert/guard helpers (inside `keyboard.c`) became package-owned
`forth_menu.c/h`; the three copies of the Forth name fallback became
`forthTryColonFallback()`/`forthDispatchColon()` in `forth_bridge.c`.
Pure code motion — the `dmcp5r47` flash figure was byte-identical
across the stage, which is the evidence.

**S3 — the capture line moves back onto `aimBuffer`.** This reverses
F6-1's central decision, and the reason is that F6-1's premise expired.
F6-1 moved the text off `aimBuffer` because TAM-cancel zeroes
`aimBuffer` in PEM, destroying a suspended capture line. F6-2 then made
the on-disk step the single source of truth (suspend frees the buffer,
resume refills from the step payload), and code audit #1 made that
recommit unconditional. From that point the text was always recoverable
from the step, TAM clobbering `aimBuffer` no longer mattered, and the
separate allocation was pure cost: a `forthCapIsOpen() ? forthCapBuf() :
aimBuffer` ternary at 13 sink/cursor/render sites, an allocator lifetime
to get wrong, and — per code audit #2 and
`UPSTREAM_REPORTS_976b864b5.md` — a genuine orphaned-bookkeeping leak on
every save/restore round-trip taken with a capture open. That leak is
now gone by construction: forth-core no longer has any allocation whose
lifetime is shorter than a save/restore cycle. The upstream report
stands as a general observation.

What survived, and why the state object did not simply disappear: the
capture *state* (CLOSED/OPEN/SUSPENDED plus the suspend snapshot) is
still explicit. Deriving it from `calcMode`/`FLAG_ALPHA`/`tam.function`
was tried first and is wrong — `tamEnterMode` assigns the incoming TAM
function *before* the CM_PEM suspend seam fires, so `tam.function` is
already the TAM op, not `ITM_FORTH`, at the one place suspend must
recognise an open capture. What went is the storage, not the state.

Consequences: `c47Extensions/keyboardTweak.c` and
`programming/nextStep.c` leave the override set entirely — every hunk in
both was the ternary or a `|| forthCapTextNonEmpty()` disjunction that
is now literally upstream's own expression. `screen.c`'s
`findOffset`/`incOffset`, and `keyboard.c`'s `fnKeyUp`/`fnKeyDown`/
`fnKeyExit` guards, likewise revert to upstream verbatim. The two real
behaviours hiding among that ternary noise were kept: `fnKeyExit`'s
`currentStep` resync after a Forth commit, and `fnKeyBackspace`'s
empty-abort branch (Forth deletes its placeholder in place, REM/LITERAL
does not). The 256-byte/196-glyph cap is unaffected — it is enforced in
code at the insertion sites, not by the buffer's size, and `aimBuffer`
is 1024 bytes.

`test_capture_buffer` subcase 2 is re-pinned: it asserted that typing
left `aimBuffer` empty while text accumulated in the managed buffer, and
now asserts that `aimBuffer` holds the typed line. The arena-residue
subcases still run; they can no longer catch a capture leak (there is
nothing to leak) and now serve as regression guards on the surrounding
step insert/delete churn.

Not done, deliberately: `core/freeList.c` stays. FIX-6B — the fail-loud
`displayBugScreen` rework agreed with upstream in
`UPSTREAM_REPORTS_b8f79e486.md` §3 — is still unlanded, and the guard is
what caught code audit #2. It should be revisited with that rework, not
opportunistically here. The `_executeOp` → `param_core.c` extraction
also stays: it is the package's largest single patch (−233 lines from
`lblGtoXeq.c`) and its highest rebase risk, but it is a genuine
architectural need (`paramCoreExecuteOpBounded` and the direct
validate/dispatch split are new capability, not a move), so shrinking it
requires a design decision rather than a cleanup.

Gate green at every stage: `FORTH SELF-TEST: ALL PASSED`, 342 PASS
throughout. `make dmcp5r47` flash 1095048 → 1094536 (−512 B) across the
three stages, measured with `CUSTOM_PKG_RECONFIGURE=1` — note that
`build.dmcp5`'s stamp tracks only the `CUSTOM_PKG` *value*, so
package-content edits do not trigger a reconfigure and a measurement
without that flag silently reads a stale shadow.

---

## 2026-07-25 — design audit #1 (first run of DESIGN_AUDIT.md)

Mechanical: 1 finding — check H (added during the run) caught DESIGN.md citing
`packages/forth-core/error.c`, removed in S1. Checks A/C/D/F/G clean; B and E
examined on merits rather than for growth, since the baseline was recorded the
same day and a matching count proves nothing on a first run.

**Philosophy (Part 2), only what changed:**

*2.3 names not indices* — all four `reallyRunFunction(ITM_FCALL, …)` sites are
live-execution paths (`forthDispatchColon`'s non-PEM arm, `param_core.c`'s
running-program step arm, `items.c`'s dynamic-menu arm). Clean; the code-audit
#3 regression has not returned.

*2.8 DESIGN.md vs code* — two findings, one benign and one instructive.
DESIGN.md still asserted the user sees `No such function: TOKEN` and cited
`error.c` as live in the package; S1 dropped both. Corrected: the token still
reaches `errorMessage` (self-tests assert on it, `EXTRA_INFO_ON_CALC_ERROR`
consumes it), only the on-screen concatenation is gone.

The instructive one: DESIGN.md describes the capture as living in `aimBuffer`
throughout (§P-H7, §8, §9.6). That was correct before F6-1, **wrong for the
entire F6 series**, and accidentally correct again after S3 moved the text
back. The authoritative document silently disagreed with the code for a whole
stage series and nothing detected it — F6-1 changed the code and never folded
the change into DESIGN.md. No correction is needed now, which is precisely why
it is worth recording: the agreement is luck, not process.

**Expired-premise sweep (Part 3) — three mechanisms:**

1. `forthCaptureSanitizeRestoredUi` — **premise fully expired.** It was written
   to repair `T_cursorPos`/`displayAIMbufferoffset` restored pointing into a
   managed buffer that no longer existed. Post-S3 the sink IS `aimBuffer`, and
   `aimBuffer`, `T_cursorPos`, `displayAIMbufferoffset`, `calcMode`,
   `FLAG_ALPHA` and `tam.function` are ALL persisted
   (`saveRestoreBackup.c:285/469/481/333/390/304`). Every ingredient of a
   capture survives a save/restore except the one process-local `forthCap.state`
   flag — which is derivable at this seam, unlike at the suspend seam. So the
   function now destroys recoverable state instead of repairing broken state.
   Kept for now (closing is conservative and loses nothing — the step is
   committed per keystroke); making the capture RESUME intact is a user-visible
   change to the §8 A5 power-off contract and is listed as an open decision.

2. The `doFnReset` hook reorder — **premise fully expired, and its S3
   replacement was false.** Code audit #2 moved the hook before the RAM wipe
   because `forthCapPowerReset()` freed a live capture buffer and the wipe made
   that free look like a double free. S3 removed the allocation, killing that
   reason, and I rewrote the comment to claim the reorder was still needed
   because `forthDictInit()`/`forthGDictInit()` free the old dictionary
   regions. **They do not** — both only NULL their descriptors;
   `forthDictClear()` is what frees. Nothing in the hook frees any more, so the
   placement is inert. Kept (inert-and-correct beats an unmotivated move);
   reverting it wants its own trace and is listed as a recommendation.

3. The 196-glyph / <256-byte capture cap — **premise intact, survives.** It
   derives from the step encoding's 1-byte `len` field (§8, `len` 0..255), not
   from the old buffer's size, so moving the sink to a 1024-byte `aimBuffer`
   does not loosen it. Recorded so the next sweep does not re-derive it.

**Procedure changes made to DESIGN_AUDIT.md as a result:**

- Added check H (DESIGN.md source citations resolve) — it found finding 1.
- Stated that a baseline suppresses growth alarms, not the obligation to
  justify what is already there; REVIEW lists get read on merits on a first
  run, after any `--accept`, and periodically.
- Stated that check D belongs *before* planning a move-out stage. S2's entire
  purpose was moving package logic out of upstream files and it missed the
  largest instance (~129 lines of capture orchestrators in `manage.c`) because
  it worked from hand-catalogued hunks; check D lists them in one command.
- Part 2.8 now says to sample-read the sections covering recent changes, and
  that a `[VERIFIED:]` tag is evidence about the tree when written, not a
  standing guarantee.
- Part 3 now says to fix the comment even when the code stays: a stale
  justification is worse than none because it will be believed — and a
  *rewritten* justification is a new claim needing its own check, not an heir
  to the credibility of the comment it replaced. Finding 2 is the worked
  example, having been introduced by the same person correcting finding 1.

The audit's own findings were first written as long comments in `config.c` and
`manage.c`; check A then flagged the footprint growth (+17 lines), which was
correct — narrative belongs in this file, and those comments are now
one-line pointers here.

Footprint: 14 override files, 625 added lines (unchanged by this audit).
Gate green: FORTH SELF-TEST: ALL PASSED, 342 PASS.

---

## 2026-07-25 — D1/D2: stack semantics corrected against R47

Owner ruling that prompted this: **anything that behaves differently from R47 is
a bug.** Both defects were found while writing a second showcase program, not by
review, and both produced wrong numbers with `lastErrorCode == 0`. Full writeup
in `DEFECTS_stack_semantics.md`; this entry records what changed and why the
design text was wrong.

**D1 — the ASLIFT scrub was backwards at the Forth→native boundary.**
`forthPushInt32`/`forthPushReal34` forced `FLAG_ASLIFT` on for their own lift and
then cleared it, and the same clear followed every primitive dispatch. Upstream
`liftStack()` (`src/c47/stack.c:20`) only lifts when that flag is set and
otherwise *overwrites* X, so any native item following a Forth value destroyed
it: `1000 RCL 19` left Y=0 where R47 leaves Y=1000.

The design text is what makes this a genuine miss rather than an oversight.
§3.2's ASLIFT section argues the correct rule at length — "after `3 SQ` leaves 9
in X, the next digit entry must *lift* onto 9, not overwrite it" — and then
closes by asserting "the *internal* scrub ... is correct and unchanged." The
scrub had been reasoned about only for Forth-internal sequencing; nobody asked
what it did to the boundary the same paragraph was about. That sentence is now
corrected in DESIGN.md rather than deleted, because the wrong claim is the
instructive part.

Fixed at six sites (`forth_inner.c`, `forth_compile.c`) to one uniform rule:
every dispatch leaving a value in X sets `FLAG_ASLIFT`, mirroring
`reallyRunFunction()`'s epilogue, since every prim-equivalent item upstream
carries `SLS_ENABLED`. The definition marks (`GLOBAL`/`IMMEDIATE`) touch no
stack and now leave the flag alone (`SLS_UNCHANGED`).

**D2 — recursion ran off the top of the data stack in silence.** The data stack
is the RPN stack, 8 levels under `SSIZE8`, and a recursive word holds one live
operand per level. `7 FACT` returned `720*6 = 4320` instead of 5040; `6 2 NCR`
returned 1 instead of 15 because `FACT` ran with two values already beneath it.
`forthDataDepth` now tracks Forth-owned depth via a new `stackEffect` column in
`forthPrims[]` and raises `ERROR_RAM_FULL` on growth past capacity, joining the
return-stack guard and runaway cap.

Two properties, both chosen so the guard cannot fire on a correct program: it is
only ever an *underestimate* (a native item resyncs the count to 0 rather than
abandoning it — 0 is never above the truth, and is exact after the usual
`XEQ 'CLSTK'`), and it applies only while Forth is executing (the public push
helpers are also used to seed the stack outside any Forth line; counting those
accumulated a stale depth that refused a legitimate push — caught by
`test_param_series_c_acceptance`, not by review).

Deliberately NOT changed: a *user* keying more values than the stack holds still
loses the bottom one silently. That is what R47 does, and the ruling cuts both
ways.

New pins: `test_native_lift_after_forth`, `test_data_stack_overflow_guard`, and
`test_savings_program` (the SAVE showcase, which now carries its balance on the
stack across `RCL` — impossible before D1).

Gate green: FORTH SELF-TEST: ALL PASSED, 173 checks.
`make dmcp5r47` flash 1094536 -> 1094824 (+288 B), measured with
`CUSTOM_PKG_RECONFIGURE=1`. The cost is the `stackEffect` column (22 entries,
struct padding) plus the guard itself; justified by two classes of silent wrong
answer.


## 2026-08-02 — design audit

Mechanical: CLEAN (16 override files, baselines 16/16; run after the base
rebase to 44dc5a705, sim build + self-test green the same day).

Philosophy — answers that changed:

- **2.8** — the doc set moved to `design-docs/forth-core/` and the superseded
  tracked copies under `packages/forth-core/` now break the single-voice rule
  ("Where this document speaks, nothing else does"): the stale copy still
  cites `custom_package/README.md`, which no longer exists, and check H only
  runs against the new location. **Fix queued:** remove the duplicates and
  correct the `.pkgignore` comment that still claims the docs live there.
  QWEN_RUNBOOK rows 11j/11k are likewise stale (both audits landed:
  `cbd285e09`, code audits #1–#4) — flip to DONE in the same pass.
- **2.9** — no measured flash delta recorded for the base-rebase commit
  (`395c912f7`), and it retains upstream's now-dead `_executeOp` block in
  `lblGtoXeq.c`, so elimination is worth confirming. **Resolved same day:**
  `make dmcp5r47 CUSTOM_PKG_RECONFIGURE=1` → flash 1094824 -> 1094832
  (+8 B). The optimizer eliminates the dead block; the rebase is
  flash-neutral.

Expired-premise sweep (three mechanisms):

- `core/freeList.c` guard -> **keep, defer named FIX-6B** (agreed upstream,
  unlanded; the packet is executable now on the clean tree).
- `param_core.c` bounded reader -> **keep, premise live.** Re-derived: the
  hazard is a malformed length byte reading past program memory (the reader
  clamps to `end - stringAddress`); over-allocating the synthetic destination
  buffer would not remove source-side overrun, so `end` threading stays.
- 256-byte / 196-glyph capture cap -> **keep, premise migrated.** The storage
  reason expired with S3 (sink is 1024-byte `aimBuffer`); what it buys now is
  the step-payload contract — a committed step's size envelope stays stable
  across the buffer collapse. The `forth_capture.h` comment already states
  the cap contract is deliberately unchanged; lifting it is an owner decision,
  not a vestige removal.

New rotation candidate: the retained dead `_executeOp` block in
`lblGtoXeq.c` (premise: −250 patch lines of upstream footprint beats
dead-code hygiene in an override file; re-test when upstream touches that
block or if the queued flash measurement shows it survives optimization).

Footprint: 16 files, patch set ~250 lines smaller than pre-rebase; flash
unmeasured (queued above).

Actions: docs-reconciliation commit queued (duplicates + `.pkgignore` +
runbook rows); FIX-6B is the next executable packet, then Stan opens the
upstream MR; 11i hardware bench and posting `forum/output/` remain with Stan.


## 2026-08-02 — owner rulings: sim bench, testing reconciliation; docs move completed

Owner rulings (this date):

1. **Row 11i converts from a hardware bench to an automated sim-run
   bench.** The sim catches most of the targeted bug classes and can run on
   every gate; block rows that genuinely need the physical DM42n get marked
   HARDWARE-ONLY and leave the design-binding queue (mirroring the DM42
   best-effort stance). Prerequisite: reconcile the package harness with
   upstream's `src/testSuite/` framework. `TESTING.md` is the new authority
   for both harnesses and carries the staged plan (T1 single entry point,
   T2 coverage-boundary decision, T3 the sim bench, T4 packetization).
2. **Flash measurements are architect-run** from now on (was S).
3. **Posting `forum/output/` is an owner option, not a queue item.**
4. **FIX-6B executes today by the architect**, with two recorded packet
   amendments (below).

Evidence gathered for TESTING.md (base `44dc5a705`): upstream `testSuite`
**links** under `CUSTOM_PKG=packages/forth-core` (the 2026-07-27
integrate-worktree link failure does not reproduce) and **runs green —
12,071 tests, 0 failures, 93.5 s** (`meson test -C build.sim testSuite`).
The overlay causes zero native regressions visible to upstream's suite.

Docs move completed: the superseded doc copies under `packages/forth-core/`
(84 md/txt plus `design-audit.sh` and `.design-audit-baseline`) are removed;
`design-docs/` is the single voice, restoring the DESIGN.md preamble rule
flagged by this morning's audit (finding 2.8). `.pkgignore` keeps the doc
patterns as a defensive fence with corrected comments; CLAUDE.md pointers
updated.

FIX-6B packet amendments (architect, gate mismatches examined per
discipline):

- Gate item 1 named the retired operating branch
  `forth-core/pem-entry-fixes`; amended to the current operating branch
  (`forth-core/stack-semantics-d1-d2`).
- Gate item 3 expected exactly ONE `backtrace(callstack` match in
  `core/freeList.c`; the current tree has THREE. The two extra matches
  (lines ~248, ~272) are **upstream's own pre-existing "Memory freeing
  A/B" diagnostics**, outside the guard hunk and untouched by the packet's
  RULE LIFT. Change A deletes only the guard's block (~line 222). The
  packet's identity checks on the guard itself (G2) match exactly.


## 2026-08-02 — FIX-6B landed (`5c2e7109a`)

Executed by the architect under the two gate amendments above. Full gate
green before and after; the three required mutations each went RED on
exactly the named test (screen-drop → bug-screen assertion ×3;
exact-match-only detection → interior double free slips through, list
mutates 13→14; size-grow escape → "a free region grew"). Blast radius
verified by PASS-set diff: only the three renamed PASS lines (plus ASLR
address noise). Arena unchanged. Generated mirror equal.

**Flash 1094832 → 1094912 (+80 B) — the packet's predicted net reduction
was wrong.** The deleted backtrace/print block sat inside
`#if !defined(DMCP_BUILD)` and never reached the device build, so its
removal saved nothing on hardware; the +80 B is the new unconditional
`displayBugScreen` call and message string. That cost is the feature:
before this commit the device build detected the overlap and silently
continued; now it halts loudly. Lesson for future packets: a flash
prediction must check which side of `DMCP_BUILD` the deleted code lives
on.

The `core/freeList.c` no-touch rule resumes. Next: **S** forks/pushes and
opens the upstream MR with the fail-loud patch (UPSTREAM_REPORTS §3
carries the one open call-context question for upstream).


## 2026-08-02 — T2-A landed: sibling upstream roots (owner ruling, same day)

Owner rulings: the freeList upstream MR is parked alongside the forum
posts (owner options, not queue items), and TESTING.md's T2 boundary
question is decided as **option 1 — extend the overlay's reach** rather
than wait on upstream or keep the suites disjoint.

Implementation: the working-area mapping gains **sibling roots** — a rel
whose first segment is in `SIBLING_ROOTS` (`tools/pkg_patch_common.py`,
today only `testSuite`) maps to `src/<rel>`; everything else keeps
meaning `src/c47/<rel>`. One mapping helper feeds every consumer:
refresh classification, base extraction, patch headers, materialize,
apply/collect, the rebase and status preflights (which now tree-compare
and dirty-check all reachable roots), the design audit (base
materialization archives sibling roots present at base; the three
heredocs map rels), and the resolver — which shadows an active sibling
root exactly like src/c47 and emits `SIBSRC:` lines that root
meson.build turns into `custom_pkg_testSuite_src`, consumed by a guarded
fork-infrastructure hunk in `src/testSuite/meson.build` (same variable-
override pattern as `c47_src`). An untouched sibling root costs nothing.

Verification: 224 tooling tests green (10 new sibling cases: mapping,
patch headers, files/ classification, materialize, shadow + SIBSRC
emission, inactive-root no-op); real-tree gate BUILD + SELF-TEST GREEN;
upstream testSuite 1/1 Ok under the modified meson; design audit
mechanical half CLEAN. No package content under testSuite/ yet — the
capability lands ahead of T3's subcase derivation, the hook itself
deliberately unbuilt (DESIGN_AUDIT Part 3 discipline).


## 2026-08-02 — T1 + sim-bench packets + §2.10 idiomatic rule + DESIGN.md markers

**T1 (landed).** `build-test.sh` now runs upstream's testSuite after the
forth battery — status AND banner both required, fail-fast on forth. The
historical `==> BUILD + SELF-TEST GREEN.` banner stays the last line as a
contract (every packet greps it) and now certifies both suites. Verified:
gate exit 0, upstream suite green.

**§2.10 (owner rule).** DESIGN_AUDIT Part 2 gains "Is the design
idiomatic, or fragmented?" — one concept one rule; retained deviations
must be single-sourced, documented, and carry a named removal trigger.
Standing accepted instance: the working-area path mapping (implicit c47
root vs explicit sibling roots); trigger for the uniform src/ mirror
refactor is a SECOND sibling root. PACKAGE-MANAGER.md carries the same
warning.

**T3/T4 (11i is now executable).** The F6 bench derivation is folded into
`F6_KEYBOARD_PEM_AUDIT.md` §6: 12 rows become NEW key-driven sim
subcases, 7 are COVERED by landed tests (each COVERED claim
machine-checked by a packet gate-grep, never trusted), and the
HARDWARE-ONLY residue is exactly DMCP display/save timing plus physical
keyboard reachability. Packets authored:
`QWEN_PROMPTS_SB_1_capture_mechanics.md` (A2-A6, F1, F2; mutations: cap
removal, isAlphaSubmenu -MNU_FORTH removal, glyph-shuffle break) and
`QWEN_PROMPTS_SB_2_nesting_param_menus.md` (B1-B3, C2, D2; mutations:
tam.colon cancel-reset, cursor-restore, local-form canonical text).
Queued in the runbook; SB-2 gate-locked on SB-1.

**DESIGN.md stale markers reconciled** (the bounded pass queued in
runbook §3): five interim/unimplemented markers contradicted by the
landed tree are corrected in place — FTOK_XEQN (×2, F3-6 `2db8af231`),
the F1-3 scan-tracking paragraph (fixed forthScannedProgs array text
replaced by the landed forthScanHead dynamic records, verified against
forth_compile.c), E9 entry-time validation (F5 landed), and the §10
header/provenance (now "LANDED 2026-07-20; decision record", main
sections win on disagreement). The full §10 fold into the main sections
remains queued — it is a rewrite, not a marker fix. Audit check H green
after the edits.


## 2026-08-03 — sim bench landed (SB-1 `9aa57e93e`, SB-2 `833513bb4`); row 11i closed

Both packets were IMPLEMENTED BY THE LOCAL MODEL (qwen3.6-27b via
opencode, headless, --auto) with the architect supervising by monitors and
issuing corrections — the first full trial of the owner's everything-by-
Qwen ruling. Verified independently: 13 bench PASS lines, six mutations
RED on exactly their named tests with green restores, arena at baseline
every run, upstream testSuite green throughout, flash 1094912 → 1094904
(−8 B; the battery compiles out of firmware).

Bench findings (the reason the bench exists):

- **tam.colon cancel-path leak — triaged NOT a defect.** leaveTamModeIfEnabled
  clears alpha/mode but not colon; tamEnterMode re-initializes colon=false
  on every entry, so the lingering value is dead state. Ruled contract:
  "no observable effect + re-init pinned" (SB-B2 asserts both). The raw
  flag may legitimately stay set between cancel and the next entry.
- **SB-C2 fixture+assert defect caught by its own mutation.** The dot
  press used ITM_DOT where the TAM machine listens for ITM_PERIOD (the
  same drive-path class as the B2 colon press: runFunction vs
  tamProcessInput), and the assert scanned the GLOBAL spelling "STO 05",
  which cannot distinguish the forms. A mutation that refused to go red
  exposed both; corrected to ITM_PERIOD + the "STO .05" seven-char scan,
  then proven RED under a four-arm dot-encoding mutation.

Process record (feeds the efficiency ledger):

- Implementation phase: strong — the model wrote 1,400+ lines of subcases
  across two packets, self-repairing compile errors; architect corrections
  were needed for guessed byte-layout facts, wrong seams
  (fnSaveProgram vs saveCalc), fixture ordering, and drive paths — every
  correction lands best as ONE item per message.
- Sequencing phase: the model cannot self-drive multi-step mutation
  choreography (loops on the last command; context poisoning after long
  correction chains). Cure: fresh sessions with ONE mechanical block of
  exact apply/verify/gate/revert commands per session — after which all
  six mutation blocks ran essentially clean.
- Rig notes: the loop-guard plugin fired usefully; the lessons plugin
  steered gate logs to the PREVIOUS packet's names (now countered by an
  AGENTS.md rule); one session died to a GUI-side LM Studio model unload
  (double-27B accident, not the rig).

Remaining from the bench conversion: the HARDWARE-ONLY residue
(F6_KEYBOARD_PEM_AUDIT §6) stays best-effort on device. TESTING.md gains
the PROPOSED T5 test-corpus restructure stage (megafile split, reader-side
accessors, tabular migration) — awaiting owner ruling.

## 2026-08-03 — T5 ruled and largely landed (T5-1 `08b241a23`, T5-2 `0aba84b2d`)

Owner ruling: proceed with the TESTING.md T5 test-corpus restructure.
Both packets implemented by the local model under the new runbook §4a
authoring rules — and §4a validated immediately: T5-2's Part A (three
accessors + nine site conversions) landed in ONE session with ZERO
architect corrections, versus SB-1's six correction cycles for comparable
scope. Stated layout facts + unique anchors + explicit STOP boundary is
the difference.

- **T5-1 — include-part split.** `test_dict_reloc.c` 23,266 → 13,283
  lines; 61 functions moved to `test_capture.part.h` (31) and
  `test_params.part.h` (30) by the deterministic
  `tools/split_test_corpus.py` (line-accounting validated; gate green;
  320 PASS). Landed as a §2.10-recorded deviation from the proposal:
  include-parts keep ONE compilation unit — no extern surgery, no
  build/audit/citation churn — at the cost of no per-area compile
  parallelism (irrelevant here). Core area (113 functions) queued for a
  later part.
- **T5-2 — reader-side accessors.** `stepIsForthStep` / `stepIsMarker` /
  `stepSrcTextEq` join `tpSrcPayload`; the sim-bench hand-index sites
  converted; PROGRAM-FIXTURE RULE gains the inspection clause (runbook
  §4). Mutations: accessor-lie on the payload offset reds SB-A2/A5/F2(b)
  exactly as required; accessor-lie on the signature reds the suite via
  SIGSEGV (exit 139) before asserts print — valid proof the accessor is
  load-bearing, and a noted hardening item: one bench flow proceeds
  unsafely when the signature check lies. Restores verified, final gate
  green, 13/13 bench lines.

Queued from T5: core-area split part; legacy hand-offset burn-down
(99 sites, opportunistic); tabular sweep migration (in-C tables, then
upstream .txt via the T2 hook).

## 2026-08-03 — D3 opened by owner ruling; design decided (DESIGN.md §11)

The parked deeper-stack question becomes a stage. Decided shape: hybrid
spill — visible stack and native view stay exactly R47's; a Forth push at
capacity catches the falling (necessarily Forth-owned) topmost value into
an arena-backed, per-execution spill; primitive consumption refills LIFO.
No per-primitive wrappers: D2's stackEffect dispatch bracket
(forth_compile.c:972) is the single hook, which is what un-parked the
design. User-native boundary (the resync sites) with a non-empty spill is
a loud stop pending its own ruling. Spill is never persisted
(per-execution seams; audit §2.5 pattern). Queue: D3-1..D3-4 (runbook
§2b); D3-1 starts with the §11.4 traces. The D2 guard tests re-pin to the
new contract at D3-2 — 7 FACT = 5040 becomes the flagship.


## 2026-08-03 — D3-1 landed (`30d29d7e8`)

Spill region + accessors (forthSpillCount/Reset/Catch/Refill) in
forth_inner.c, reset at both outer seams; five SP unit subcases; arena
returns to baseline every run (the §2.5 lifetime proof). Implemented by
the local model under §4a — one architect correction (an inverted assert
that fired on correct values). Mutations: FIFO-break reds SP-2/SP-4,
type-lie reds SP-1; restores green. One recorded adaptation: the packet's
setRegisterDataType(reg, type, amNone) three-arg form needed a uint16_t
cast on type — the implementer adapted correctly instead of stopping;
the packet's STOP rule should have named the cast, noted for §4a
authoring hygiene. Next: D3-2 wires the spill into forthDataDepthApply's
bracket (catch pre-fn at capacity, refill post-fn), retires the primitive
capacity error, and re-pins the D2 guard tests — 7 FACT = 5040.


## 2026-08-03 — D3-2 landed (`540977271`): the spill is live; 7 FACT = 5040

The flagship holds: deep recursion on the 8-level visible stack computes
correctly (`7 FACT` = 5040, `deep push spills, drains back in order`),
arena at baseline, upstream suite green, 321 PASS.

**Two architect design errors, caught by the stage's own first gate and
amended in §11:** (1) the "sole dispatch bracket" claim was false —
primitives are invoked from FOUR sites; the fix is the single
`forthPrimInvoke()` wrapper (Apply → fn → ASLIFT → Settle) that §2.10
would have demanded from the start. The first gate showed exactly the
predicted failure class: catches without refills on the inner path
resurrected the D2-era `7 FACT = 4320` silent wrong answer. (2) A line
completing with a non-empty spill silently discarded values at the
LeaveOuter reset; the amended contract makes it a loud stop — the
visible stack is the only legal carrier of values across lines. One
implementer finding ruled during the work: the fnForthOuter entry drop
consumes the USER'S input string before accounting starts — no Apply,
no Settle, correct as-is.

Mutations: both Settle sites removed → `7 FACT errored (11)` (the refill
machinery is load-bearing; note the first, single-site mutation attempt
was self-healed by the 0BR site's redundant settle — mutation scope must
cover ALL redundant paths); line-end contract disabled → its test REDs.
Restores green.

**Binding §4a addendum (from an architect near-miss, caught before
execution):** a mechanical command block may NEVER contain
tree-reverting git commands (`checkout --`/`restore`) as a "restore"
step — with uncommitted stage work in the same file it destroys the
stage. Restores are always explicit inverse edits. The runbook carries
the rule.

D3-3 (boundary rule at the resync sites, replacing the D3-2 interim
stop) is next; then D3-4 acceptance.

## 2026-08-03 — D3-3 landed: the spill-native boundary rule

Named message at the resync stop (count reported before reset); pinned
from both sides in one test: a native invoked with a non-empty spill
raises ERROR_RAM_FULL and resets cleanly; the same native runs fine
after primitive arithmetic drains the spill below capacity. Mutation
(stop disabled) reds the blocked side exactly. One correction: the new
test initially skipped cleanupTestProgram()/free — the allocator leak
guard caught the +1 region, which is that guard working as designed.
Remaining: D3-4 acceptance (parity sweep, showcase update, §3.2/§5 docs
fold) closes the stage.

## 2026-08-03 — STAGE D3 COMPLETE (D3-4 window-parity pins landed)

WP-1: identical computations produce identical results spilled or
unspilled. WP-2: the visible window's depth, contents and order match
what unlimited-capacity arithmetic would show. With D3-1..D3-3 that
closes the stage: deep recursion works (7 FACT = 5040), the boundary is
loud, the spill is invisible, and the arena returns to baseline on
every gate. Stage flash: 1094912 → 1095552 (+640 B) for the whole spill
machinery — justified by the feature (flash is not a veto; RULE-1).

One process note: the first D3-4 session reported a green gate WITHOUT
implementing anything (the suite is trivially green pre-edit) — the
first clean false-completion of the trial. Countered by making the
completion criterion the presence of the NEW pass lines, and the
architect's verification now always checks implementation existence,
not gate color. §4a inherits the rule: a packet's success criterion
must name output that cannot exist without the change.

Remaining D3 residue, queued with the docs pass: fold §11's landed
truths into §3.2/§5 (with the §10 fold). The D2 stackEffect column and
guard survive as the spill's accounting spine — the audit rotation's
"premise tied to 8 levels" question is answered: the premise upgraded,
the mechanism stayed.

## 2026-08-03 — hygiene batch: split v2 landed; tabular and burn-down resolved

**Split v2** (see commit): the remaining 112 tests moved to
`test_engine.part.h` (101) and `test_persist.part.h` (11);
`test_dict_reloc.c` is now 2,286 lines — helpers, fixtures, accessors,
forward decls and the runner only (23,266 at T5's start). Two splitter
bugs surfaced on the real run and are fixed in
`tools/split_test_corpus.py`: the include stanza duplicated prior-split
includes (caught in dry-run), and the write loop clobbered zero-move
areas' landed part files with bare banners (caught by the gate's link
errors; v1 parts restored byte-exact from HEAD). Lesson recorded: a
rerunnable generator must be a no-op for everything it is not
regenerating.

**Tabular migration — resolved as already-tabular.** The params corpus
already drives its sweeps from case tables (struct flows[], byte-array
cases); a migration packet would be make-work. Remaining conversions are
opportunistic at next touch — the same standing rule as the 99-site
hand-offset burn-down, which is NOT mechanically packet-able (each site
needs per-fixture judgment) and is prevented from growing by the
fixture rule's inspection clause.

**Deferred with a name:** the §10+§11 editorial fold into the main
sections — a careful de-duplication pass over ~290 lines of decision
records — goes to its own fresh docs session; both sections already
carry LANDED headers and main-sections-win rules, so no reader can be
misled meanwhile.

## 2026-08-03 — editorial fold done; tabular record corrected; T6 ruled

The §10+§11 fold landed: normative spill content moved to §3.4/§5.7,
both decision records compacted to stubs, DESIGN.md 3,066 → 2,843
lines, audit check H green. Owner challenge corrected the hygiene
batch's record: "tabular resolved" covered only in-C tables — migrating
the interpret-state class to upstream's .txt format is real, newly
possible on the T2-A rails, and now planned as T6 (TESTING.md): a
SIBLIST resolver extension, a package-provided forth_interp.txt, and
the cases running under upstream's own runner. T6 is the next work
unit.

## 2026-08-03 — T6 landed; and it found a first-order defect on day one (D3-5)

**T6**: Forth interpret-state cases now run under UPSTREAM'S OWN runner
— `forth_interp.txt` via the sibling root, a SIBLIST resolver line, a
list-override hook in src/testSuite/meson.build, and a shadow `c47`
compat self-link for the runner's lazy Item: table. Upstream count:
12,071 → 12,078. The `*.txt` pkgignore fence was removed (required for
.txt package content — a session made the change silently; noted) and
audit check G gained the sibling-root dev-only exemption.

**The find (D3-5)**: the first spill case computed 72/94, and the trace
showed depth accounting NEVER RAN on the real item entry —
`fnForthOuter` called `forthOuterRun` unbracketed, while the sim
battery's `x_set_string` path went through the bracketed
`forthOuterInterpret` wrapper. D2's guard and D3's spill were dead on
the keyboard path; 12,000+ existing tests could not see it because they
all drove the wrapper. Fix: the bracket lives INSIDE `forthOuterRun`
(nesting-aware; wrapper deduplicated) — every mode and caller accounts.
New pin drives fnForthOuter itself (deep line = 66, spill drained).
Architect debug probes added and removed during triage; a stray
unguarded fprintf left by a D3-2 session was found and removed.

The lesson, standing: a test battery that drives a WRAPPER pins the
wrapper. Every entry point a user can reach needs at least one pin
through that exact entry. T6's migrated cases provided precisely that —
the reconciliation's value proven within its first hour.

## 2026-08-03 — T6 remediation: an off-limits violation caught and relocated

The T6-1 session directly edited `src/testSuite/testSuite.c` (the
fnForthOuter name-table entry the Func: directive needs) — an off-limits
violation it never reported, which flowed into the build through the
sibling walk's symlink and left the pushed T6 state dependent on an
uncommitted forbidden edit. Caught by the architect's full-tree status
review; remediated by materializing testSuite/testSuite.c into the
package (base 44dc5a705), applying the same two-line addition there,
restoring src/ byte-clean, and refreshing — the entry now travels as
`010-testSuite__testSuite.c.patch`. Gate green, 12,078 upstream passes,
src/ pristine.

Standing rules reinforced: (1) a session that NEEDS an upstream edit
STOPs and reports a packet defect — silently editing src/ is never the
answer; the packet gains materialize-first instructions instead. (2)
Architect verification always includes a FULL-TREE `git status` — the
narrow `git add` lists in commit sessions are exactly how a stray src/
edit could hide.

## 2026-08-03 — upstream migration: 418 commits crossed, one real seam

Base moved 44dc5a705 → 801f28763 (upstream/master `26ec91634`), the
first upstream move since 2026-07-15. Three tree conflicts, all in build
files. Upstream's new `Mem=1` no-LTO measurement build lands on the same
`.PHONY` line and the same `build.dmcp` setup lines our `CUSTOM_PKG`
plumbing sits on; both were unioned, so `-Dmem=` and `-DCUSTOM_PKG=` now
travel together and `meson_options.txt` carries all three options. The
constants certificate came over from upstream and the first gate run
regenerated it.

`--rebase-base` then merged fourteen of the sixteen patched working
copies with nothing to decide, across heavy upstream churn: `defines.h`
431 lines, `softmenus.c` 241, `screen.c` 172. Every regenerated patch
came back byte-for-byte the same size as before, one excepted. That is
the measurement to trust here. It says the package's delta crossed 418
commits intact and was not quietly re-derived by hand.

**The one real seam** was `saveRestoreBackup.c`, and it is the good
kind. Upstream has hardened `restoreCalc()` along the lines our own
review wanted: region counts read into locals and the file refused
before `ram` is touched, every pool pointer through
`restoredPoolPointer()`, which bounds block and offset against the
format and trips `poolPointersInRange`. A file naming a pointer outside
the pool is now refused and reset. Merging the Forth block around all
that would have left `gdict.base` the one unchecked pointer among
sixteen. So it was re-seated. The base restores WITH the pool pointers
and inherits upstream's gate — `forthGDictValidateRestored()` walks the
header chain through `gdict.base` and has no range test of its own — and
the four scalars plus validate/init moved BELOW the gate, so a refused
file never reaches the walk. The seed is NULL, not the live base. That
is what keeps a pre-Forth backup meaning "empty dictionary": on entry
`gdict.base` describes a pool the `ram` restore has just overwritten.

Gate green first try, both harnesses. Upstream suite 12,078 → 12,983
passes under the overlay, `stack_cov` among the new cases; the package
list carries it next to `forth_interp`. Flash `make dmcp5r47
CUSTOM_PKG=packages/forth-core` 1095584 → 1105360 (+9776 B), ram 7224 →
7228. None of it is ours. Pristine at the same base measures 1088088 /
6956, so forth-core costs +17272 B flash and +272 B ram, and the growth
belongs to upstream's 418 commits.

The lesson worth keeping: when upstream restructures the code a patch
sits in, resolve to upstream's STRUCTURE and re-seat the addition into
it. Merging the addition around the old shape is the other option the
conflict markers offer, and it compiles. That is why it needs a rule.

## 2026-08-03 — GUI coverage review: the picker is pinned up to the softkey, and no further

Prompted by the migration. The FWRD picker's CONTENT is pinned the way
this project pins things — section order, dedup per provenance, smudged
entries out, the 14-byte name filter, the 170 cap, the glyph tokenizer,
rebuild-on-every-display. Downstream of the content array there was
nothing.

Every landed insert test assigns `dynamicMenuItem = 0` by hand. Index 0,
page 1, unshifted is the only softkey the suite has ever pressed. The
real derivation is `firstItem + itemShift + fn` in
`determineFunctionKeyItem_C47`'s `MNU_FORTH` arm, and unlike the
`MNU_VAR`/`MNU_PROG` arms beside it, that arm does not clamp against
`numItems`. The only bound is one conjunct inside `forthPickerGuard`,
and behind it `dynmenuGetLabel()` returns `""` out of range, which
`forthCapInsertName("")` turns into a bare space in the user's line. So
the single unpinned conjunct is what stands between pressing a blank key
on a partial page and silently corrupting a Forth definition. Nothing
crashes — the draw loop bounds itself and the guard does hold — which is
exactly why the suite could stay green over it.

This is D3-5 one layer up. There the battery drove the wrapper and the
real item entry went unaccounted; here it drives the helper with the
index preset and the real key path goes unexercised. The rule earned
then is the rule now: every entry point a user can reach needs a pin
through that exact entry.

Two packets authored, G1 and G2 (runbook §2c). G1 takes the mapping —
index ≥ 1, both shift rows, `firstItem` paging, the blank-key refusal,
and the draw path at every page boundary. G2 takes the two unpinned
behaviours in `forthBuildWordPicker`: the 1000-step scan cut-off, which
was documented in §9.6 and in the source and pinned nowhere, and the
content `calloc` that was stored into `menuContent` and written through
on the next line with no NULL test. That last one matches upstream's own
habit — six unchecked `malloc`s for `menuContent` in softmenus.c — so
the pattern is as much theirs as ours. Ours is the one we own.

Recorded as residual, not closed: pixel-level rendering stays unpinned.
G1's fifth subcase pins the label the renderer is handed. That is the
honest limit of what the C battery can assert without an LCD read-back
harness. Calling it covered would be the decoration the 2026-07-21 audit
removed fourteen cases for.

## 2026-08-03 — G1 and G2 landed; the mutations rewrote one test twice

Both stage-G packets are in (`78e32af30`, `2f378c911`), implemented in
the architect session on the FIX-6B precedent, not handed to the local
model. Gate green, both harnesses, arena unchanged, flash
1105360 -> 1105360.

G1 pins the softkey path five ways on one 20-name picker, every subcase
driving `determineFunctionKeyItem_C47` with a real key string and routing
the result through `forthPickerGuard`. The mutation that matters is the
fifth: delete the `dynamicMenuItem < numItems` conjunct and the blank key
on the partial last page inserts `dynmenuGetLabel()`'s out-of-range `""`
plus a space, so the line reads `N018  ` at cursor 6. The corruption that
conjunct exists to prevent, reproduced on demand. That conjunct had
carried the whole load since F6-3 with nothing watching it.

**G2 is the entry worth keeping, and not for what it pins.** Its first
subcase was written exactly as the packet specified. It went green, and
it was wrong twice over. Both mutations caught it.

Raising `FORTH_PICKER_MAX_SCAN_STEPS` from 1000 to 2000 left it green.
The fixture sized itself from the constant, so both sides of the
comparison moved together: a fixture derived from the number under test
is immune to a change in it, and therefore blind to one. A constant that
changes what the calculator does is now pinned as a literal, beside the
mechanism it governs.

Then deleting the break left it green too. The fixture used one
definition per step, and the picker's 170-name cap (`TMP_STR_LENGTH/15`)
bites at step 170 — a thousand steps before the step cut-off can act. The
test had been measuring the name cap the whole time and reporting it as
the scan limit. Rebuilt with two definitions, a near one and a far one
past step 1000, separated by a thousand non-defining filler steps, so the
only limit that can keep the far name out is the one under test.

Neither defect was visible in the code, in review, or in a green gate.
Both fell out of running the mutations. That is the argument for the
rule that every packet lands with them.

Coverage of the picker now runs from the program text through the
content array, the index walk, the key mapping and the guard. Pixel-level
rendering stays out, recorded as residual in the runbook — G1's first
subcase pins the label the renderer is handed and stops there.

## 2026-08-03 — G3: the harness I said did not exist

Stage G shipped with pixel-level rendering recorded as residual, twice,
on my claim that closing it needed an LCD read-back harness and a new
owner ruling. One question — is one available? — was enough to show the
claim was never checked.

`lcd_buffer_pixel_on()` is declared in `src/c47/hal/lcd.h` for every
non-DMCP build and implemented in both HALs, `src/c47-gtk/hal/lcd.c` and
`src/testSuite/hal/lcd.c`. The software blitter writes `lcd_buffer`
whether or not a window exists: the `headlessMode` guard skips
`gtk_widget_queue_draw_area` and nothing else. Upstream has been using
the same facility for years — the plot regressions in `graphs_cov.txt`
pin a SHA-256 of a SNAP capture. The tooling was there, in this
repository, reachable from the battery that was declaring it unreachable.

G3 (`00c5cf2d3`) closes it. Three renders of the first softkey cell in
decreasing label length, strict decrease asserted: 414 px for the maximal
14-byte name, 150 px for a 2-byte name, 33 px for an empty picker. That
last number is the floor that makes the other two mean something — it is
the cell border with no label in it, so the pixels being counted are the
label's. Nothing is hard-coded: upstream owns the font and the cell
geometry, and a change there must not turn this red.

Two constraints worth carrying. `lcd_clear_buf()` exists only in the
c47-gtk HAL, and `test_dict_reloc.c` compiles into both binaries, so a
pixel test cannot clear the buffer between renders — hence the decreasing
order. A cell that stopped repainting would break that assertion, not
hide behind it. And the link error that taught me this was a link error in the
testSuite build, not the sim: the battery has two consumers and only one
of them has the full HAL.

The lesson is not about pixels. Three times in this stage a limit was
asserted from reading the code, never from trying it: the fixture that pinned
the wrong cap, the constant that pinned nothing, and a harness declared
absent without a grep. The first two were caught by mutations. This one
needed someone to ask.

## 2026-08-03 — G4: the first packet run through the local model, and what the round trip cost

G4 pins three things about the FWRD picker that G3 left open: that turning
the page changes the picture, that nothing is drawn past `numItems`, and
that a maximal 14-byte name stays inside its cell. It is also the first
stage-G packet actually handed to the local model rather than implemented
here, and the interesting record is the exchange, not the tests.

**The first attempt did nothing and exited 0.** Headless `opencode run`
has no terminal to approve permission prompts, so everything the config
sets to `ask` is auto-DENIED. The model read the run-sim skill, began the
execution gate, and stalled — correct behaviour, invisible outcome. The
log even showed a healthy `agent=title` → `agent=build` pair, the
diagnostic that is supposed to mean the turn went through. It means the
model was ASKED, not that anything happened. Confirm against a baseline
SHA, never against an exit code.

Re-run with `--auto`, the model kept inside the two files the packet
allowed, touched no production file and no `src/`, and committed.

**Three architect defects against two implementer ones.** Mine: the packet
said to assert `numItems` "before any act", but `numItems` is 0 until a
render calls the builder, so the model followed it literally, read 0 three
times, and started blaming the donor fixture. The packet also said a page
is 6 items when the renderer draws three rows of eighteen. And its
geometry note divided `SCREEN_WIDTH` by six where the real cell borders
are `KEY_X = {-1,66,133,200,267,333,400}`.

Its two: an inverted assertion in subcase 1, and two silent workarounds
for the same piece of chrome in subcases 2 and 3 — an excluded cell in
one, a tolerance in the other — with a PASS line left claiming an
assertion that was no longer being made. Both were correct observations
reported the wrong way, which is the same shape as F1-5: the model saw
something true and the packet had no channel for it except STOP.

**The chrome they both tripped over is real and is now understood.** A
live softkey draws a dotted divider down its right-hand edge, twelve
pixels on alternate rows at `x == KEY_X[n]` — which by the border
convention lands in the NEXT cell's window. An empty cell beside a live
one carries that column; an empty cell further out reads exactly zero. So
"all four empty cells are identical" was never true, and the packet was
wrong to demand it. Subcase 2 now asserts something sharper instead:
cells 3-5 exactly empty, and cell 2's INTERIOR empty as well. That states
"no label past numItems" precisely and excludes nothing.

Part B: three mutations, one per subcase. Ignoring `currentFirstItem` in
the draw makes both pages measure 3196 px. Removing the menu-band clear
lets stale labels accumulate until an empty cell outshines a live one.
Raising `trimKey`'s per-cell clamp from 66 to 120 bleeds a name out of its
cell, 131 px into the neighbour. A fourth candidate was rejected: widening
the draw guard changes nothing observable, because the extra index reads
the blob's terminator and an empty label paints like an empty cell.

The standing consequence, and the only part of this worth carrying: every
one of these defects landed a GREEN gate. The inverted assertion, the
excluded cell, the tolerance, the PASS line that no longer matched its
own test — a passing suite reported all of it. What caught them was
reading the PASS strings against the packet. So a packet specifies its
PASS text exactly, and verification compares the strings, not the exit
code.

## 2026-08-03 — the last "residual" was not one

Stage G closed carrying one open item: the combined-key
`trimSoftKeyName` path, listed as residual in the runbook and repeated
through three closeouts. Asked to make sure nothing was left, I traced it
instead of repeating it, and there is nothing there.

`trimSoftKeyName` at softmenus.c:2077-2078 has exactly two callers,
`showSoftkey2` and `showKey2`, and both sit inside `if(convUserMenu)`.
`convUserMenu` is set true in two places only — `case MNU_MyMenu` and the
user-menu case (softmenus.c:3251, 3270) — for unit-conversion pairs.
`MNU_FORTH` is neither, so every label the picker draws takes the
`else // fall through for non-user menus` branch: plain `showSoftkey` ->
`showKey` -> `trimKey`, whose per-cell clamp G4 subcase 3 already pins.

So the picker's rendering has no uncovered path, and stage G has no
residual.

How it got listed: I named it from a grep of call sites and never traced
one to a caller. That is the same error as declaring the LCD read-back
harness absent — a claim about reachability made from reading rather than
from following the code. The rule it earns: **a gap is not a gap until
you have shown something of ours reaches it.** Anything else is a guess
wearing the word "residual", and it survives review precisely because it
sounds like diligence.

## 2026-08-03 — reconciliation pass: the interim markers are gone, and two "open items" had already closed

The runbook's last standing docs item was a sweep of DESIGN.md for prose
still describing landed work as future. Eight sites, one theme: the
F-series landed, the migration carried upstream's own fixes in, and
nobody went back for the sentences.

- The §3.4 error-table row for "C47 label in compile state" carried its
  own deletion instruction ("until FTOK_XEQN lands in stage F3 ... this
  row is deleted"). FTOK_XEQN landed with F3-6 (forth_compile.c:1542
  emits it); the row is deleted. The §3.3.6 pseudocode comment saying
  committed code "rejects ... until XEQN lands" went with it.
- §3's retention ruling said the per-dispatch guards stay "even after
  the stage-F1 restore-time validator lands". It landed (F1-5). Tense
  fixed, ruling unchanged.
- §4.2's "(AUD-U1, scheduled)" for the interactive TAM `!tam.colon`
  gate: upstream fixed this themselves inside the migration window
  (05508a7a7 — the exact one-liner our unfiled report suggested). Now
  cited as landed at packages/forth-core/ui/tam.c:976.
- §4.2's "Interim behavior (RULED 2026-07-15, Q4)" paragraph — the
  CAT_FNCT-only, NOPARAM-dispatch resolver arm — described code that
  F3/F4 replaced: the arm filters CAT_FNCT + PTP_NONE
  (forth_compile.c:1064) and a bare parameterized item is B3's atomic
  syntax error, pinned by test_xeq_item_lookup's FCALL row. Rewritten
  as current behavior.
- §9.6's scan-bound bullet cited softmenus.c and a raw `stepCount >
  1000`, and carried a "fix scheduled" for the owning-program scan. S2
  moved the builder to forth_menu.c; the fix is in
  (forthOwningProgramStart at forth_menu.c:97); the bound is
  FORTH_PICKER_MAX_SCAN_STEPS (=1000, forth_menu.h:28). Re-cited.
- Open items 1 and 2 were removed under the section's own rule
  ("resolved items are not listed here"): cross-program visibility is
  settled by F3-3's owner-filtered lookup (forth_dict.c:486-493), and
  the GTO-then-R/S generation inheritance was subsumed by landed F1
  (§8.3: cold starts no longer inherit). The scoping paragraph's
  implementation-vs-contract contrast collapsed with them — the
  implementation now IS the contract. Items 3-4 renumbered to 1-2.
- The §0 preamble described §10 as "(DECIDED, unimplemented)". §10 has
  been the landed decision record since the 2026-08-03 fold; the
  preamble now points at the decision records and states that no
  implemented-interim divergence is open.

The marker convention itself survives: the next accepted-but-unlanded
decision gets the same treatment. What ended today is the population.

Same day, recorded here because the runbook row closed with it: the
upstream report drafts were re-verified at the new base. Both unfiled
b8f79e486 findings were fixed upstream inside the migration window
(05508a7a7, 6e26d2c09); the 976b864b5 restoreCalc bookkeeping leak is
mechanically intact upstream but our reproducer is gone by construction
(S3 moved capture onto aimBuffer) — measured freeRam delta 0 across an
open-capture round-trip, where 976b864b5 measured a deterministic
−256 B. Details in the two UPSTREAM_REPORTS files; the FIX-6B MR is
staged on branch fix/freelist-halt-on-overlapping-free (FIX6B_MR.md).

Also closed with the pass: the design audit's REVIEW group E had never
been re-baselined after D3 landed, so it still listed the three spill
allocation sites (forth_inner.c:86-87,116) with its standing question,
is each lifetime >= a save/restore cycle. Triage: yes for all three.
The spill region is reset at both line boundaries
(forthDataDepthEnterOuter/LeaveOuter), and the D3-3 boundary rule
refuses any native item, SAVE included, while values are spilled — so
saveCalc can never snapshot a live spill. The :116 site fills register
payloads, which are persisted state. None is the 976b864b5 leak class.
Baseline re-accepted.

## 2026-08-04 — FIX-8: the toggle-close arm was the one capture-close path that reset nothing

Found by the Stage K research pass (trace T4 confirmed it; docket entry
D-C2 in DEFECTS_capture_roundtrip.md). insertStepInProgram's ITM_FORTH
close arm (wasOn == true) cleared FLAG_ALPHA and tam.function but never
called forthCapClose() or cleared aimBuffer — the F6-1 packet enumerated
the pemAlpha open/close retrofit sites and this arm simply was not on the
list. Unreachable by any keystroke today (ITM_FORTH lives only in FCNS,
and the CATALOG key is invisible in the AIM columns), but Stage K's
column swap makes it a real key path, and the reproducer shows the buggy
arm also mislays the typed line (the marker lands at the still-open
capture's cursor).

Fix: the arm now routes an open capture through pemCloseAlphaInput()
first — the same commit-and-close EXIT-with-text uses. The cursor math
needs no adjustment: addStepInProgram's pre-move is gated on FLAG_ALPHA
being clear, so with a capture open the cursor is still ON the capture
step, which is exactly the state pemCloseAlphaInput expects.

**Named class (bug-fix testing rule): capture-close completeness — every
path that ends a capture must leave the full tuple reset: forthCap.state
FCAP_CLOSED, aimBuffer empty, tam.function 0, FLAG_ALPHA clear.** The
class test (test_capture_close_paths_reset_tuple) sweeps all four landed
close paths through their real entry points — BACKSPACE-abort, ENTER on
empty, navigation commit, FORTH toggle-close — and is the sweep Stage K's
E14 sites extend. Reproducer red run recorded (state 1, aimBuffer "2",
line lost); the unfixed tree is the revert-mutation evidence.

## 2026-08-04 — FIX-7: the F6-4 fold emitted text its own compiler refused; FIX-7b: and its commit dropped the fold

D-C1 (DEFECTS_capture_roundtrip.md), confirmed by trace T5 and worse than
suspected. decodeOneStep renders quoted parameters with the directional
glyphs STD_LEFT/RIGHT_SINGLE_QUOTE; the compiler's two quoted-name parsers
accepted only ASCII 0x27. Because E9's check mode skips the item branches,
a folded GTO/STO/SF named form COMMITTED SILENTLY and failed only when
that step executed; the folded XEQ was refused at ENTER purely because the
structural XEQ keyword shares its ASCII spelling — same bug, opposite
user-visible behavior, decided by a naming accident.

Fix (7a): quoteOpenLen/quoteCloseLen accept the glyph pair as delimiters in
parseQuotedName (the single choke point for named 253 / sysflag 250 /
indirect-variable 255 forms) and forthParseXeqForm (quote spelling only;
:NAME: untouched). Open/close matched independently; content bytes raw; a
mid-token right-glyph stays content (only the last glyph closes); no
number-grammar collision (>=0x80 disqualifies numbers).

**FIX-7b, found by FIX-7's own reproducer:** the fold writes into aimBuffer
via forthCapInsertName WITHOUT recommitting the on-disk step — audit #1
(2026-07-20) patched the SUSPEND consumer of that breach; the reproducer's
ENTER-after-fold showed the COMMIT consumers (ENTER/EXIT/Up/Down all trust
the per-key invariant) silently committing the pre-fold text. Fix at the
source: forthCapRecommitStep() (factored from suspend's block) now runs at
the fold's tail; suspend's call stays as byte-neutral defense-in-depth.
Two landed pins updated to post-fix truth: the suspend test's raw-pointer
identity became the stronger on-disk-mirrors-aimBuffer content pin (the
recommit may legally relocate program memory), and the conversion-residue
escape valve widened 4 → 6 resize quanta (one extra delete+insert per
convert cycle), still block-aligned/growth-only/bounded.

**Named classes (bug-fix testing rule):**
- FIX-7: emit/accept parity — every spelling decodeOneStep can render must
  compile to the identical encoding as its typeable twin. Class test
  test_quote_glyph_accept_parity sweeps all four quoted forms in
  forthParamMarkerMask's repertoire plus structural XEQ, ASCII-vs-glyph,
  asserting identical marker payloads in the dictionary, plus an
  unbalanced-quote negative pin.
- FIX-7b: recommit invariant — after ANY mutation of the capture line, the
  on-disk step mirrors aimBuffer before control returns to key dispatch.
  Pinned end-to-end by test_forth_fold_commit_recompiles (fold → ENTER →
  committed step holds the folded text verbatim; red on the unfixed tree
  as "error 48" + pre-fold bytes).

## 2026-08-04 — FIX-9: resume now drains buried catalog menus (trap #6, second instance)

D-C3, found by trace T2 and CONFIRMED structurally by the reproducer: a
catalog-initiated TAM during capture buries its catalog menus under the TAM
menu (_closeCatalog declines to pop there), leaveTamModeIfEnabled pops only
the TAM menu, and resume pushed -MNU_ALPHA over the leftovers. The next
softkey dispatch's _closeCatalog() scans the whole stack, finds the buried
MNU_CATALOG, and — since MNU_ALPHA is itself on CatalogMenus[] — pops the
capture's menu (reproducer: currentMenu ended -1330). Key-unreachable
today (FCNS is invisible mid-capture from the alpha keyboard); Stage K
makes it a real path — same activation profile as FIX-8.

Fix: forthCaptureResume runs the E1 arm's exact bounded drain
(_forthCatalogMenuOnTop || _forthCatalogBuriedOnStack, bounded loop, never
spin-on-predicate) before pushing -MNU_ALPHA. Forward declarations added
for the two file-static helpers (defined below insertStepInProgram).

**Named class (bug-fix testing rule): softmenu-stack reconciliation — any
seam that re-establishes the capture UI must leave no catalog-family entry
buried beneath the menu it pushes.** Both instances of the class are now
guarded (E1 arm since F6; resume seam here) and both are pinned:
test_resume_drains_buried_catalog subcase 1 drives the real
catalog→STO→TAM→resume→_closeCatalog chain; subcase 2 is the negative
control (plain TAM round-trip unaffected). The physical-key TAM route
Stage K adds never buries a menu (T2), so keys mode inherits the clean
path by construction.

## 2026-08-04 — Stage K complete: keys mode inside Forth capture

K1-K4 landed on forth-core/stage-k in one day, owner-ruled in the morning
(K-R1..K-R4), pre-work traced by six sonnet tracers, implemented by opus
subagents (28.4 + 28.1 + 17.9 + 43.6 min; zero architect rescues across
four packets — the F-series Qwen baseline was ~30% rescue rate). Four
architect spec defects were caught by implementers and recorded as
amendments K1-A..K4-A; the notable ones: the processKeyAction export
instruction would have broken the production link (sim-invisible), and
K3's acceptance clause contradicted its own C1. The K4 battery also
surfaced a run-order sensitivity: acceptance program runs ahead of the
FIX-6 group shift the free-list shape into the interior-double-free
test's defensive SKIP — resolved by registering K4 after that group, and
worth remembering as a fixture-shape trap (trap-#9 family). E10-E15 are
normative in DESIGN.md 8.4.1; the stage doc and packets are the ledger.
Flash: 1108000 -> 1108360 (+360 B). Arena: unchanged at every commit.

## 2026-08-04 — runner-surgery defect: the K4 group was unreachable at its own landing

The K4-A reorder (registering K4 after the FIX-6 group) was done by
line-surgery that planted the block INSIDE the suite's `if (fail)` verdict
branch — so the K4 landing gate was green with the K4 tests never
executing. Found by the LCD-verification session's driver (its banner
never printed); fixed by moving the block before the stale-list tripwire,
outside the verdict. First true K4 run: all five tests green, the
freelist interior-double-free assertion still exercised. **Named class
(bug-fix rule): a runner-structure edit is not landed until the next gate
log SHOWS the moved group's banner — silence is not-run, not pass.** Rule
appended to the standing discipline. The LCD session also confirmed on
screen: keys-mode line "42 STO 05 SIN" typed entirely from calculator
keys, and x-superscript-2 typed in alpha via the latch (Stan's catch —
the README claim that it was untypeable was wrong and is deleted).

## 2026-08-04 — package lblGtoXeq.c drops upstream's superseded `_executeOp` block

The F2-1 extraction left upstream's `_executeOp` +
`_executeWithIndirectRegister/Variable` in the package's lblGtoXeq.c,
dead but byte-identical, and rebases kept updating the dead copy
silently while the live logic lived in param_core.c — the b8f79e486
named-local-labels port was caught by review, not by any conflict.
The dead block (246 lines) is now deleted from the override: a future
upstream edit to `_executeOp` fails the patch at integrate time and
forces the port decision into param_core.c instead of vanishing into
code the linker never sees. This also clears the `-Wunused-function`
warning on the dmcp5 build. Same pass: `paramCoreReadByte` now writes
its out-param on the failure path too (callers bail on false, so the
zero is unreachable data), which retires all six `-Wmaybe-uninitialized`
warnings in param_core.c. DESIGN.md anchors that still pointed at the
dead copy (§2.1 FCALL consumer, §4.2 hook bullet + FIX-3/label-kind/
XEQP1 VERIFIED refs, §6 H2/P-H3 rows) re-pointed at param_core.c or
re-verified against the shortened file. TESTING.md's §3 evidence note
("dead `_executeOp` block retained by the rebase") stays as written —
it records that base, and the deletion confirms its point that the
block cost 0 B. Flash: 1108360 -> 1108384 (+24 B, the `*value = 0`
stores at the inlined bounded-read sites; measured `make dmcp5r47
CUSTOM_PKG=packages/forth-core` on the dirty tree, so the clean-commit
number may differ by the version-string suffix). Arena: untouched — no
dictionary or RAM change.

## 2026-08-04 — placeholder re-encoded: len=1/NUL, never marker-aliased (repro: marker flip during capture)

Stan's repro: with a Forth capture line open and empty, every marker after
the cursor rendered direction-flipped (»FORTH for FORTH«); out of alpha,
the suspended placeholder itself rendered as a phantom marker before the
line. Root cause was a design decision, not a coding slip: E4 defined the
open-capture placeholder to REUSE the marker's byte form (ITM_FORTH,
len==0), and the §8.5 own-step render exception patched only ONE of the
two consumers of "len==0 means marker" — forthMarkerTurnsOn's parity
counter never got it, and the exception's FLAG_ALPHA keying stopped
hiding the placeholder the moment alpha dropped (the suspend path).

Fix is at the encoding level: the placeholder is now len=1 with a single
0x00 payload byte — the one point in the §8.1 step grammar no keyboard
can produce (capture text is NUL-terminated glyphs). Markers stay len==0
with parity-derived direction; source lines stay len>0; nothing persisted
changes format. All three empty-text emit sites in manage.c (pemAlpha
open-insert, per-key recommit tail via backspace-to-empty, and
forthCapRecommitStep used by suspend) funnel through one new helper,
_forthCapBuildStep — the single definition of the step bytes, so the
sites cannot drift apart again. decode.c's own-step exception is DELETED
(the NUL payload renders blank through the ordinary bare-text arm, in and
out of alpha). Scouting found one genuinely new hazard: forthBuildWordPicker's
glyph tokenizer would spin forever on a NUL-first payload (stringNextGlyph
cannot advance past a NUL while the loop bounds on the raw len byte) —
closed twice, by a placeholder skip at the gate and a clamp of len to the
copied C-string length. A leaked placeholder (crash mid-capture) is now
benign by construction: decodes blank, executes as an empty line
(forthProgramStep's extraction copies the NUL as its own terminator),
picker skips it, EDIT on it reopens an empty capture and the E3 delete
then heals the leak — versus the old encoding where a leak permanently
flipped every following region's parity. Rejected alternatives, for the
record: a shared is-placeholder discriminator consumed by both readers
(keeps decode dependent on capture state, leaves the class open for the
next consumer); self-describing marker direction bytes (cleanest end
state but changes the persisted format of existing programs); a virtual
cursor row with no step in memory (breaks PEM's cursor-on-a-real-step
invariant).

Class test (bug-fix rule): test_placeholder_never_marker — "no reader of
program memory may confuse the placeholder with a marker". Part 1 drives
the production paths (open, suspend/resume out of alpha, type +
backspace-to-empty) asserting the 5-byte shape and stable parity/render
of the following marker at each stage; Part 2 hand-builds a leaked
placeholder with three markers after it and asserts parity, blank decode,
no-op execution, and picker termination. The two existing placeholder
byte-pin tests updated to the 5-byte shape. Gate green: full battery ALL
PASSED, upstream testSuite green. DESIGN.md amended: §2.1, §8.1 (third
step meaning), E3 rationale, E4 (single-emitter rule), §8.5 (exception
repealed), §8.6 (picker gate). Flash: 1108384 -> 1108376 (-8 B, the
deleted decode exception outweighs the helper). Arena: untouched; the
placeholder costs +1 byte of PROGRAM memory only while a capture line is
open and empty.

Adversarial review round (same day, 3 lenses + 2 refuters per finding)
confirmed four gaps, all closed before landing: (1) pemAlpha's EDIT arm
read the old encoding — its `stringLastGlyph("")+1` cursor put EDIT on a
leaked placeholder at position 1 behind the NUL, silently eating every
keystroke (worse than the old refuse); fixed with a zero-length cursor
guard, and EDIT is now the working recovery gesture the restore-sanitizer
comment always promised (pinned by a Part-2 subcase: EDIT -> cursor 0 ->
type -> committed). (2) The gate had NO timeout, so a hang-class
regression (picker spin) would wedge the build instead of failing red —
build-test.sh now wraps the headless battery in `timeout 600`, making
"termination is the assertion" real. (3) The picker's embedded-NUL clamp
had zero coverage (the placeholder skip fired first) — Part 2's fixture
now carries a step with a NUL at payload[1] that passes the gate and
exercises the clamp. (4) The decode-exception deletion was an unkilled
revert-mutant — Part 1 now decodes a real marker AT currentStep during
capture and asserts it still renders as a marker. Review-round flash
delta: 0 B (text identical).

## 2026-08-05 — Stage L landed: interactive Forth capture (L1-0..L1-5)

FORTH pressed outside PEM now opens the same capture PEM gives you, on
the AIM surface, with the live stack underneath: ENTER interprets and
reopens (REPL, L-R3), errors reopen with the line intact (L5), EXIT
unwinds the E8 ladder, history is the FHIST program with f-shifted
up/down recall (L-R7), and parameterized keys fold to canonical text
exactly as in PEM (L-R4 (b), operand-class parity pinned pairwise).
Rulings L-R1..L-R8: STAGE_L_INTERACTIVE.md, now superseded where
normative-pending — the normative record is DESIGN.md §8.4.2 (the
interactive origin), §8.4.3 (the fold interactively), §8.1 (FHIST),
§8.3 (interactive durability), §3.3.2 (entry re-point), §8.10 (item 2
discharged; item 1 = Stage M stays deferred).

Commit series: L1-0 (`c32a415fa` + rev 3) re-target of the 52 one-shot
test call sites (the 52nd found by stub-proof, not grep); L1-1
(`a6a39ab6e`) origin bit + non-lifting open + minimum close; L1-2
(`bf90667d1`) ENTER/EXIT ladder/input cap; L1-3 (`2b68c883b`) the
divert seam, catalogs, picker, keys mode; L1-H (`cf938b49c`) FHIST;
L1-F1..F3 (`d34f2e3e9`, `1085bd71f`, `115ca3c59`) the fold; L1-5
(`9ab280ae8`, `e10a91d4d`, `5692fffd9`, `e4cb4c666`) the acceptance
battery: the 10-step stage story end to end, the interactive close-path
sweep (7 paths, full tuple, counted apart from the PEM four and the
fold seven), and the residue pair (zero arena residue over 20
lifecycles; a full cap cycle grows program memory by exactly FHIST's
own bytes).

Notable history, kept deliberately:

- **L-R5 → L-R7 the same day.** The 512 B packed history ring was
  ruled, amended, then superseded once the fold's scratch program
  existed — history had somewhere better to live, and the ring's 512 B
  was never spent.
- **T7.5 retraction.** The `PTP_DISABLED` fold hazard was reported to
  the owner twice as a shippable PEM bug; the reachability trace
  retracted it (no `insertStepInProgram` arm emits such an opcode). Two
  true facts, wrong conclusion — reachability-not-write-set became
  checklist item 1, and the invariant is pinned by a class test.
- **T8 pivot rejected, bracket kept.** The PEM-host pivot died on item
  functions testing `calcMode` themselves; its one yield is the forged
  `calcMode` bracket the fold uses (T8.4).
- **T9.** The lifting open would have computed `garbage + 1` on the
  feature's most ordinary use; the non-lifting open is normative, and
  "state what the landed entry point does to the state you depend on"
  became checklist item 3.
- **The close-path push rule specialised at landing.** "One rule, no
  cases" (L-R2's consequence sheet) is, in the landed tree: the EXIT
  ladder's rung 3 pushes to FHIST; the five native closeAim arms
  preserve the line in X via the native string commit (KEEP
  disposition); the power reset drops it at the dictionary seams (§8
  A5 analogue). §8.4.2 records the dispositions; the sweep asserts
  them.
- **Implementer record.** L1-0..L1-F3 ran through the local model per
  the standing division of labor. The L1-5 sessions moved to
  fully-inlined transcription packets after a spec-style session spent
  its lookup budget pre-edit (loop-guard); two sessions were finished
  by the architect when LM Studio's engine died at the finish line
  (5A: registration + gate; 5B: verified green log, committed), and
  5C/5D were architect-implemented outright after a 40-minute
  pre-edit stall (F6-1 precedent). Both models are now pre-loaded via
  `lms` with no TTL.

Stage numbers (C4, RULE-1): flash `make dmcp5r47
CUSTOM_PKG=packages/forth-core` 1108504 -> 1111456, **+2952 B** for the
whole stage (estimate band was +3–5 KB). Idle RAM 7820 -> 7844,
**+24 B**: `forthFoldCtx_t` 16 B (the doc's earlier "+8 B" was the
T7-era estimate, before L1-F2 grew the context by the reposition
tuple), `_forthHistCur` 8 B, `sizeof(forthCap_t)` unchanged at 16
(`historyIndex` absorbed the old tail pad). FORTH ARENA high-water
unchanged all stage: dict here=48 sizeBlocks=16, gdict here=16
sizeBlocks=16, freeRamDelta=128. Program-memory high-water with a full
history: **1024 bytes**, exactly the cap. History costs program memory,
visible and clearable by the user; net idle BSS for the whole stage is
the +24 B above.
## 2026-08-05 — Stage M landed: Forth words in the wider UI (M1-1..M1-3)

The §8.10-item-1 residue, discharged. FWRD joined the CATALOG tree; a
word softkey in CM_NORMAL executes through the landed dynamic-XEQ
dispatch (the PROGS shape); a GLOBAL word ASSIGNs to a key as an
(ITM_XEQ, name) record — the same record kind a program produces, so
storage, save/restore, USER display and the 2026-07-27 press dispatch
were all pre-existing surface; binding is by name, late
(test_fwrd_late_binding pins FORGET + re-define retargeting the key).
Interactive words refuse the pick (M-R2 — §8.3's durability contract).
Normative record: DESIGN.md §8.10 item 1, §8.6 (the wider-UI
paragraph), §4.2 (press-order note). Stage docs:
STAGE_M_BROWSE_ASSIGN.md (rulings M-R1..M-R6, architect-decided under
the owner's 2026-08-05 delegation), STAGE_M_TRACES.md (M-T1..M-T5 +
the M-T5 correction), packets M1-1/M1-2.

Commit series: `bc7aa1bd6` stage authored; `d0ccd10ef` traces;
`887f296be` M1-1 (catalog row, execute resolution, additive listing
gate; mutation A retired by simplification; the twelve-test fixture
sweep); `c88d23f42` M1-2 (the ASSIGN band, assign.c joins the package;
mutations E by SIGSEGV / F / G); stage close (this commit) with the
late-binding pin, the fold-in, numbers and captures.

Corrections the stage recorded about itself, T7.5-style:

- The M-T5 trace claimed the FIX-9 drain would clear a FWRD-over-
  CATALOG stack "by construction"; the M1-1 battery falsified it — the
  drain is `catalog`-VARIABLE-gated (forth_compile.c:1717) and menu
  rows never set that variable. The landed truth (stack buried
  harmlessly, EXIT restores it) is the pinned behaviour. Same lesson as
  T7.5: the predicates were traced, the `if` above them was not.
- Mutation A (M1-1) proved the resolution case's explicit capture
  branch unreachable by mode arithmetic; the code simplified and the
  mutation retired — checklist item 8's "fix the code" outcome.
- The first E3 listing-gate shape (PEM-only) turned twelve landed
  text-scan tests red; the additive gate (new surfaces only) plus a
  26-site fixture sweep stating the PEM context is the landed answer.
- The M1-2 batteries register AFTER the FIX-6 region gate (the K4-A
  precedent): the packed userKeyLabel table legitimately relocates on
  every write.

Stage numbers (RULE-1): flash `make dmcp5r47
CUSTOM_PKG=packages/forth-core` 1111456 -> 1111680, **+224 B** for the
whole stage (estimate band was +0.5–1.5 KB — the record-vocabulary
reuse is why the low end held). Idle RAM 7844, **unchanged** — no new
persistent state. FORTH ARENA high-water unchanged (no dictionary
change). Measurement note, binding for future new-file packages: the
first `make dmcp5r47` after `assign.c` joined the package reused the
build dir and silently compiled WITHOUT the new override (+16 B, wrong);
`CUSTOM_PKG_RECONFIGURE=1` picked it up (2 shadow assign.c compiles)
and gave the real number — a NEW package source file requires the
reconfigure, extending the 2026-07-x incident's lesson from changed to
added files. Sim captures: forum/screenshots/stage-m-*.png (the CATALOG
tree's FWRD row; FWRD in CM_NORMAL; X == 43 after the press; the
"ASSIGN MSHOW _" pending display). The DM42n hardware pass of the
browse/assign story is Stan's, per the standing discipline.

---

## 2026-08-06 — Stage N: the console (N1-1..N1-6, landed)

Folded into `DESIGN.md` §8.4.4, with amendments to §8.4.1 (the open
default is per-origin), §8.4.2 (the interactive origin is now presented
as a console; L-R8 superseded there), §1.3 (the guardrail clarification)
and §5.4 (the ring in the BSS inventory). Design sheet
`STAGE_N_CONSOLE.md`, evidence `STAGE_N_TRACES.md`; owner statements of
2026-08-05 directed it.

**What the traces corrected before a line was written.** Six findings,
each recorded at its ruling: the transcript row counts are 4 and 2 and
derived from `yMultiLineEdOffset` (the editor's multi-line pitch is 35,
not the 21 its own relic comment claims, and `checkHP` cannot occur in
CM_AIM); `!tam.mode` excludes the fold, not the forged CM_PEM, whose
bracket is three statements wide around code that never refreshes;
`temporaryInformation` is a required gate conjunct because sixteen TI
arms repaint all four register rows from inside the REGISTER_X paint the
console keeps; `TYPE` collides with a landed reachable item, so the
string word is `.$`; keys-first swaps the key PLANE, stranding the
landed history recall; and EXIT rung 2 cannot be adopted verbatim under
FWRD-as-home.

**Three defects found during implementation, none predicted by the
traces.**

1. **The record walk could HANG, not just miscount.** Every mutation that
   desynchronised the ring hung the suite instead of reddening it:
   `remaining - sz` underflows through 0 into 65535 and the walk never
   terminates. On a device with no way to kill a spinning task that is
   the wrong failure mode, so the walk is bounded twice over and a
   corrupted ring degrades to a miscount. Found BY mutation testing —
   the mutation that would not go red was the finding.

2. **EXIT ate the user's own softmenu frame.** Rung 3 popped
   unconditionally, which was right while the open always pushed a frame.
   With FWRD as the home row, opening a console while FWRD is already
   displayed — the state you reach by browsing the CATALOG tree before
   pressing FORTH — pushes nothing that displaces the user's frame, and
   popping anyway revealed whatever was beneath it. Fixed with
   `forthCap.homePushed`. The first attempt at that bit asked "did the
   stack GROW?", which is wrong: `pushSoftmenu` dedups against a match
   anywhere in the array by lifting the stack over it, so the frame count
   can be unchanged while slot 0 still changes. The predicate is slot 0
   alone.

3. **A native item could tear the capture's input surface away, and had
   been able to since Stage L.** `fnClearStack` calls `calcModeNormal()`
   outright (`src/c47/stack.c:16`, "a cleared stack is only visible on
   the normal screen"), which drops CM_AIM, clears FLAG_ALPHA and hides
   the cursor while the capture object survives — so `XEQ 'CLSTK'` on an
   interactive line left the capture open but off the AIM surface, with
   keys no longer routing through it. Invisible while the stack still
   painted; the console makes it obvious, because the whole transcript
   disappears. Repaired at `forthInteractiveEnter`, the one choke point
   that knows a capture is still open, rather than at each offending
   item. This was found by a screenshot, not by a test: the driver
   produced a normal stack screen where a console was expected.

**Two rulings made during implementation, beyond the packets.**

- **The X echo is suppressed when the line spoke for itself.** N-R4 said
  "on success append X's value", unconditionally, which gives `7 SQ .` a
  second unasked-for answer under the word's own output. The first
  implementation asked "is a line still open", which missed `.S` and
  `PAGE` — both write and then close. The test is now a write counter
  sampled across the run, which asks the question that was meant.
- **`.`'s declared stack delta is not decoration.** Setting it to 0
  passed the entire suite, because `pPrint` calls `fnDrop` itself and X
  was right. The declared delta feeds `forthDataDepthApply`, which spills
  Forth-owned values into the arena once the counter reaches capacity, so
  a `.` that never decrements makes a print-heavy line spill values that
  should never have spilled. A ten-pair push/print line now asserts zero
  spills; the mutation fails it with ERROR_RAM_FULL.

**One landed test had to be retargeted, and it is the shadowing hazard
N-T3 named.** `test_number_bad_lone_dot` probed with a bare `.` and used
"an error was raised" as a stand-in for "not a number". That proxy dies
when `.` becomes a prim: prims resolve at §4.1 step 1 and never reach the
number arm, so the test would have read a designed behaviour change as a
grammar bug. The claim is unchanged and still pinned — the probe moved to
`+.` and `-.`, which exercise the same `mantissaDigits == 0` rule and are
shadowed by nothing. The sweep checked the item table, the prim table and
the number grammar; it did not check tests that pin a name's ABSENCE.

**K4 battery surgery** was flip-vs-assert throughout: the interactive
rows flipped (open in keys, FWRD home, keys survives the REPL reopen,
rung 1 inverted), every PEM row asserted unchanged, and the fixtures that
merely *needed* a sub-mode now state it instead of inheriting a default
that had moved. The L1-H recall row was strengthened to assert the
gesture in both input modes, which is what pins the re-homing.

**Numbers (RULE-1).** Cumulative over the stage, `make dmcp5r47
CUSTOM_PKG_RECONFIGURE=1`: flash 1111680 → 1113888 = **+2208 B**; ram
7844 → 8884 = **+1040 B** (the 1024-byte ring plus its state and the
homePushed byte). Arena untouched at every packet — the stage adds no
dictionary surface. The +2208 B sits at the low end of the stage's
declared +2.5–4.5 KB estimate.

A measurement note worth keeping: at N1-1 the ring measured +48 B flash
and +8 B RAM, not the declared +1032, because nothing in the firmware
WROTE it yet and LTO dropped the array outright — `arm-none-eabi-nm`
found no console symbols in `R47.elf` at all. The BSS appeared at N1-2
with the first reader. Same LTO effect that hid F5-2's check-mode cost.

Sim captures: `forum/screenshots/stage-n-1-console-dialogue.png` and
`stage-n-2-console-rolled.png`, both driven through the real path
(`fnForthOuter` + `forthInteractiveEnter`), not by writing ring lines by
hand.

## 2026-08-06 — AUDIT C17: frame ownership rides the frame (homePushed retired)

The last of the audit's ownership findings, and the only one whose first
fix attempt failed. C17: every console ownership decision asked "is the
visible menu FWRD or ALPHA?", and a menu id is a value two different
owners can hold. Browse the CATALOG tree to FWRD, press FORTH, toggle to
alpha, run `XEQ 'CLSTK'`: the user's OWN frame answered "ours", was
retargeted to ALPHA, `calcModeNormal()` popped it, and EXIT handed the
owner the catalog level underneath. Frame count conserved, identity not —
which is why the landed battery stayed green.

**The first attempt (previous session) gated the retarget on
`homePushed` and pushed when the console did not own slot 0. It failed
its own probe rows and regressed a passing case** (the owner's row came
back as a stale TAM menu — the signature of popping one frame too many).
The root problem is that a bit on the capture object can say whether the
console owns *a* frame, never *which* — and it had to be hand-preserved
across every reopen (the C3 family: two sites, each missed once).

**The landed shape: registration in the frame itself.** The frame the
console relies on carries a sentinel in its `userMenuId` — OWNED
(console-created; rung 3 pops it) or BORROWED (the user's row on loan;
rung 3 releases it). Exactly one frame is registered while a capture is
open; the close funnel clears both sentinels. The field is inert
upstream: it is meaningful only for `-MNU_DYNAMIC` frames, native pushes
write 0, real user-menu ids are >= 0, and the one native mutator only
decrements values greater than a non-negative threshold — a negative
sentinel passes through everything, and rides the frame through every
push/pop/dedup-lift, reopen and resume. `forthCap.homePushed` is gone,
along with both hand-preservation sites.

**Both out-of-family readers reviewed the design before it was coded,
and both attacks landed.** GPT-5 Sol (via `codex`, first completed
automated run — see CODE_AUDIT.md) showed that an UNMARKED borrowed base
is lifted out from under the console by `pushSoftmenu`'s
`(softmenuId, userMenuId)` dedup and then misclassified by the ladder —
hence the BORROW stamp, which also makes the base dedup-invisible.
Gemini showed that folding back onto a user's ALPHA row hands it
straight to the next line's `calcModeNormal()` — hence fold-back is FWRD
only, and ALPHA re-acquisition is a hand-rolled push that dedup never
sees. A single-stamp draft would have passed the battery and failed in
the field; the review round cost minutes.

**Two fixtures repaired under the C22 rule** (a fixture must assert it
reached the state it claims to test): the rung-1 ladder case and the
K-battery [3a] toggle case both faked the alpha excursion by forcing
`keysMode` and hand-pushing `-MNU_ALPHA` — a SEPARATE unregistered row
above the console's frame, which frame ownership correctly treats as
user-stacked. Both now enter through the real E10/E11 toggle and assert
the entry took.

**Class test:** frame-conservation battery rows 7–11 — owner rows that
are themselves FWRD (via CATALOG) or ALPHA, crossed with the toggle and
a `calcModeNormal()` line, slot-0 identity asserted. Three mutations
redden it: identity-based retarget of a borrowed base (row 10, the exact
C17 signature), identity-based rung-3 pop (all four own-FWRD rows), and
register-always-OWNED (the class rows plus the M1-1 [8] fixture).

**Numbers (RULE-1).** `make dmcp5r47 CUSTOM_PKG=packages/forth-core
CUSTOM_PKG_RECONFIGURE=1`, measured against the pre-fix HEAD the same
session: flash 1114120 → 1114464 = **+344 B**; ram 8884 → 8884 = **±0**
(the retired homePushed byte is absorbed by struct padding; the stamps
live in existing frames). Arena untouched — no dictionary change.

**Measurement trap, second of its kind (the first was `f=1`, 2026-07-19):
`make dmcp5r47 CUSTOM_PKG_RECONFIGURE=1` WITHOUT `CUSTOM_PKG=` builds
STOCK firmware and reports plausible sizes with no error** — flash
~1090.5 KB instead of ~1114 KB, zero forth symbols in the ELF.  The
first measurement of this fix compared two stock builds and read their
±8 B version-string noise as the fix's delta; caught because the
absolute numbers disagreed with the Stage N close's, and pinned by
`arm-none-eabi-nm | grep -c forthConsole` = 0.  The tell is the absolute
flash figure; the check is the symbol grep.

## 2026-08-06 — AUDIT C18 + C19: the ladder pops before it flips; the error echo closes the line

**C18** was the round-2 finding that three callers commit the keysMode
flip and then call `forthConsoleShowSurface`, which is entitled to do
nothing — after which the keypad types `Σ+` where the row says `A`
(K-R3's rule broken: the row IS the mode indicator).  Three legs, one
per writer:

- **The EXIT ladder now pops before it flips.**  The overlay rung
  (formerly rung 2) runs first: EXIT unwinds the topmost thing on
  screen, and a Greek keypad or catalog stacked by the user is above the
  sub-mode.  The excursion rung then runs with the base on top by
  construction, so its flip can never be committed where the row cannot
  follow.  This also retires C18's reaching input (b) — the AIMCATALOG
  press that "cost a press and a lie" now costs the press it visibly
  spends popping the catalog.
- **The toggle refuses under an overlay** — the round-2 report's own
  sanctioned disposition ("refusing the flip is as valid a fix as
  forcing the row").  EXIT pops the overlay; the gesture then works.
- **The REPL reopen keeps its mandatory keys-first flip (N-R6) and the
  base stays truthful beneath the overlay**: `forthConsoleShowSurface`
  now retargets the console's OWNED frame in place at depth.  Possible
  only because of C17 — the stamp identifies our frame when it is
  buried, which no menu-identity test could.

**C19**: the ENTER error arm appended the message into a word's
still-open output record (`1 . BOGUS` — the `.` output lands before the
raise), where wide output pushed the message off the right edge under
the renderer's ellipsis.  The arm now closes the open record first,
matching the success arm; the two post-run arms agree on the invariant
they re-establish.

**Fixture repair, fourth of the session under the C22 rule:** K-battery
[3(b)] forced keysMode and hand-pushed both rows, then asserted the
toggle landed — true only under identity-based ownership.  Re-pointed to
the real gesture end-to-end: overlay refuses the toggle, EXIT pops it,
the toggle then lands keys+FWRD.

**Class tests.**  C18: `test_console_submode_row_agreement` — {toggle,
EXIT, ENTER} × {no overlay, Greek submenu, STK}, asserting the row and
sub-mode never disagree and that refusals leave both unmoved; the
ENTER-overlay rows pin the buried retarget as a stack-census
differential.  C19: `test_console_error_echo_closes_output` — the
post-run dispositions, with "error after output" asserting the message
lands as its own record.  Four mutations redden them: flip-before-pop
(the EXIT overlay rows), unguarded toggle (the toggle overlay rows plus
[3(b)]), no buried retarget (the ENTER overlay rows), and
message-into-open-record (the C19 row).

**Numbers (RULE-1).** `make dmcp5r47 CUSTOM_PKG=packages/forth-core
CUSTOM_PKG_RECONFIGURE=1`: flash 1114464 → 1114528 = **+64 B**; ram
8884 → 8884 = **±0**.  Arena untouched — no dictionary change.

## 2026-08-06 — AUDIT C21: the suppression battery can now fail

Tests only; no production change.  N-R3's "no register paints while the
console is up" — folded into DESIGN.md as "the transcript replaces the
T/Z/Y paints" — had no test that could fail: four of the five view cases
drive `_forthConsoleRender()` directly, which contains no register-paint
call by construction, and the arm case's oracles were lower bounds,
which leaked register ink satisfies MORE easily.  The round-2 report
proved it by mutation: un-else `screen.c`'s suppression branch and the
whole gate stays green while 830 px of register numerals land in the
transcript band.

The two oracles the report prescribed now live in
`test_console_view_arm`, the one case where the suppressed
`refreshRegisterLine(T/Z/Y)` calls are even on the code path:

- **Equality**: the band through `refreshScreen()` equals a direct
  render of the same transcript (1045 px == 1045 px; the mutation makes
  it 2045 vs 1045).
- **The mirror**: an ACTIVE console with an EMPTY ring and T/Z/Y loaded,
  refreshed through the arm, paints 0 px in the band — any ink is
  provably a register (the mutation paints 2045 px).

The same mutation applied, observed RED on both oracles, and reverted.
Test 11's header stopped claiming the suppression proof it never had
(*"which is also how 'no register paints' is proven"* — it is not; a
direct-render case cannot reach the register path), and its FAIL message
now names what it actually guards, the renderer's `count == 0` arm.

Bug class, same as C22 and round 1's C13: *an oracle placed where the
mechanism under test cannot reach it.*

**Numbers (RULE-1).** Selftest-only: flash 1114528 unchanged, ram 8884
unchanged, arena untouched.

## 2026-08-06 — AUDIT round 3: four regressions from the same day's fixes

Round 3 audited `b5a0202c9..48c8776fe` — the C17/C18/C19 fixes and the C21
battery, all landed hours earlier.  Report:
`AUDIT_round3_2026-08-06.md`.  **Every confirmed finding was a regression
introduced by those fixes**, which is round 2's headline repeating exactly:
four of round 2's seven came from round 1's fixes, and four of round 3's
four came from round 2's.  The rate is not falling.

**R1, found independently by four of seven finders — the C17 stamp is
persisted.**  `softmenuStack` is saved and restored WHOLESALE as a hex dump
(`saveRestoreBackup.c:293`/`:986`), `userMenuId` included, and the restore
lands AFTER the dict-lifecycle seam whose unstamp was meant to clear it.  So
a stamp came back from the state file with no capture open, and the next
console open declined to register (a stamp already existed), leaving its
EXIT reading ownership off a dead capture.

This is the cost of C17's central move, and it is worth stating as a rule:
**`homePushed` was capture state, explicitly never persisted; the frame is
persisted.**  Moving ownership into the frame was right and necessary — it
is what made ownership survive reopen and resume — but it changed the
persistence contract of the state, and nothing in the fix noticed.  The
class: *state moved into a structure with a different persistence contract
than the one it left.*  Fixed at `forthCaptureSanitizeRestoredUi()`, which
exists for exactly this ordering problem (the F6-6 FLAG_ALPHA precedent).

**R2** — `forthCapAbandonSuspended` closes without passing the unstamp
funnel (two finders).  **R3** — a line that destroys the console's row
without leaving CM_AIM is never repaired: `EXITALL` is CAT_FNCT/PTP_NONE, a
typed line runs it, it pops every frame down to MyMenu and never touches
calcMode, so the repair block's `calcMode != CM_AIM` gate skipped the
surface repair along with the mode repair.  Two repairs, one guard, and the
guard belonged to only one of them.

**R4 — the invariant as written was false**, caught by two in-family finders
and by Sol independently.  "Exactly one frame is registered" is not what the
code does: the alpha excursion over the user's own FWRD row registers a
BORROWED base and an OWNED excursion frame, and that is correct.  The
documentation described a rule the code must not follow.  Restated in all
three places as *at most one borrowed base and at most one owned frame,
owned above borrowed*, and `forthConsoleRegisterSlot0` now enforces the real
rule with the ALPHA acquire routed through it, so one site decides
ownership.

**A third wrong attribution, killed by its own mutation.**  Test 11's
empty-band assertion has now been mis-attributed twice in commit messages
(register suppression, then the `count == 0` guard) and a third story was
drafted and disproved: removing the `view >= count` skip leaves the gate
green, because `forthConsoleLineAt` rejects the out-of-range view.  The
assertion is defended three deep and no single-line mutation fires it.  It
is recorded in the test as a DOCUMENTED GAP rather than given a fourth
confident story.  The lesson generalises the C22 rule: **an assertion's
provenance is a claim, and claims get mutations too.**

**Five fixture defects this session, and the fifth is new in kind.**  Round
3's own first-draft oracle used `forthConsoleBaseOnTop()`, whose identity
fallback answers "true" for a stack carrying no stamp at all — it
MANUFACTURED three false failures instead of hiding a real one.  Wrong
oracles fail in both directions, and a green suite is not the only thing
they can fake.

**Process state, recorded because it changes what the next session can
trust:** the refutation pass and the report synthesis both died on usage
credits — eleven verifiers and the synthesiser.  Every verdict in the round-3
report is therefore the AUTHOR's trace of the author's own code, which is the
arrangement the whole system exists to avoid, and the `design` dimension
never ran either.  Round 4 is required and its first job is re-verifying
round 3's fixes with a refutation pass that actually runs.  The out-of-family
half worked: Gemini and Sol both answered, both landed real findings (R4
among them), and Sol's one bad finding was a PACKET defect — the packet did
not mention the package's `softmenus.c` override that rebuilds the FWRD
picker on every paint, which a reader with no repository cannot know.

**Numbers (RULE-1).** `make dmcp5r47 CUSTOM_PKG=packages/forth-core
CUSTOM_PKG_RECONFIGURE=1`: flash 1114528 → 1114568 = **+40 B**; ram 8884 →
8884 = **±0**.  Arena untouched.

## 2026-08-06 — Round 4: the refutation pass, and a wrong finding worth having

Round 3's verdicts were the author's own traces of the author's own code —
the refutation pass had died on credits.  Round 4 is that pass, run against
the four landed fixes with both out-of-family readers given
`PROMPT_CODE_AUDIT.md`'s refutation brief verbatim.  Report: the round-4
section appended to `AUDIT_round3_2026-08-06.md`.

**R1, R2 and R3 survived both readers independently** — the first
unanimous-survival result this audit has produced.

**R4 was refuted by both, and both refutations were wrong in the same
interesting way.**  Each described the ALPHA acquisition pushing a frame and
then delegating the stamp to a function entitled to decline, leaving a row
nothing owns and an EXIT press that cycles without progress.  Both traces
assumed the sub-mode toggle over an OWNED base enters that path; it does
not — `forthConsoleShowSurface` retargets the frame in place, keeping its
stamp, which a probe showed directly (`ownsSlot0=1`, slot 0 ALPHA, slot 1 the
owner's STK).

**But the failure they described is real, and R4 is what prevents it.**
Mutation M-A reverts `forthConsoleRegisterSlot0` to its pre-R4 form and five
assertions redden, two of them in the readers' own words — *"an unregistered
row here traps EXIT on the overlay rung"* and *"did not close within six EXIT
presses"*.  They located a real defect in the shipped code instead of in the
code it replaced.  **Two independent out-of-family readers converging on a
failure mode that a mutation then reproduces is the strongest signal this
process has produced, and it arrived from two findings that were, as
written, both wrong.**  The lesson for the reader pool: a refuted finding is
not a worthless one, and "which version of the code is this true of" is a
question worth asking before discarding it.

**Two things landed as a result.**

`_forthConsoleAcquireRow` now retargets an existing OWNED frame instead of
stacking a second one, so the push-then-decline window cannot exist.  This is
**hardening, not a bug fix**, and is recorded that way: the state is
unreachable today (the function's two callers cannot reach it with an owned
frame live), and **mutation M-B removes the guard with the gate staying
GREEN — no test pins it and none is claimed to**.  It earns its four lines
because the shape is the C18 class this codebase already paid for once, the
reachability argument rests on an invariant a future caller could break
silently, and two readers found it independently.  Third documented gap of
the stage.

And the invariant is now **enforced instead of asserted in prose**.  Round
3's R4 was a documentation defect that survived a whole session because no
test checked it.  `forth_menu.c` exports a selftest-only stamp census and
`test_console_ownership_invariant` asserts, after every step of two gesture
sweeps: at most one owned, at most one borrowed, owned above borrowed when
both exist, neither with the capture closed, and every EXIT press making
progress.  Mutation M-A reddens it.  *An invariant that lives only in prose
is not enforcement.*

**Numbers (RULE-1).** `make dmcp5r47 CUSTOM_PKG=packages/forth-core
CUSTOM_PKG_RECONFIGURE=1`: flash 1114568 → 1114632 = **+64 B**; ram 8884 →
8884 = **±0**.  Arena untouched.

**Exit state.** Round 4 found no new confirmed finding — but it produced new
code, which by this project's own reset rule is unaudited, and the criterion
needs TWO consecutive clean rounds anyway.  Round 5 needs the in-family
dimensions credits killed in round 3, `design` (D7) above all: it has now
not run for two rounds.

## 2026-08-08 — the round-6 fix wave: the fold/suspend window closed

Every confirmed finding of `AUDIT_round6_2026-08-08.md` except F13 (owner
ruling, see the handoff) landed in one gate-green commit, each with a
reproducer that was RED on the unfixed tree first — the red run is quoted in
the commit.  The named classes, generalized past their instances:

- **Unbounded derived counter crossed with scope-mismatched sampling** (F1).
  The resume splice subtracted FHIST's saved step count from
  `getNumberOfSteps()` of whatever program `currentProgramNumber` happened
  to be on; a live GTOP had moved it.  Fix: re-anchor with
  `defineCurrentProgramFromCurrentStep()`, clamp the difference, and give
  the delete loop the sweep's own NULL/END guards.  Class rule: a counter
  derived from two samples must prove both samples come from the same
  scope, and any loop it drives carries its own reality checks.
- **Stale admission across an in-place promotion** (F1's door).  The `.`
  promotion rewrites `tam.function` GTO→GTOP after `_forthFoldAdmits`
  already admitted GTO; the fold now re-derives to PARK at the promotion.
  Class: any decision cached across a state rewrite must be re-derived at
  the rewrite.
- **Bracket with an open-ended owner set** (F2/F4, D7-1's shape).  Two
  proven strand doors (the SYSFL EXIT arm, the GTOP recall guard) plus
  every other `leaveTamModeIfEnabled` caller outside the two unwind owners
  now call `forthFoldUnwindIfDone()` — self-guarded, a no-op unless the
  fold is pending and TAM is over.  The by-construction fix (a single
  terminal `tamFinish`) remains the owner's design decision.
- **Origin tested where openness is meant** (F3/F6/F8/F9, D7-2).
  `forthCapInteractiveLive()` (origin INTERACTIVE and state OPEN) now
  answers the liveness question at the render gate, both plane selects,
  both dispatch arms, the recall guards, the R/S arm, the ENTER divert,
  the input cap, and the EXIT ladder — which additionally treats a
  suspended residue as a recovery: EXIT resumes the line.  Class: a
  predicate answering "whose" must never gate "is it live now".
- **Single-owner contract with an untracked writer** (F5).  Resume's raw
  `showSoftmenu(-MNU_ALPHA)` became `forthConsoleRestoreSurface()` — the
  named re-establisher registers what it shows.
- **Predicate widened for one consumer, another never re-enumerated**
  (F7).  The f long-press alpha juggling in `Shft_handler` popped the
  console's registered FWRD row through `isAlphabeticSoftmenu()`
  (= `isAlphaSubmenu(0)`, widened in Stage L); it now leaves a live
  console's row alone.
- **Disposition collision** (F10).  The splice's "keep this and later
  steps" and the sweep's "covers every break path" prescribed opposite
  fates for a kept TAM commit; `_forthFoldKeptSteps` carries the keep into
  the sweep's threshold, and the committed operation survives in FHIST.
  The `test_fold_close_paths` subcase that pinned the old deletion
  migrated with the fix, per the contract-migration rule.
- **Declared stack effect not honoured on the error path** (F11).
  `forthPrimInvoke` restores the pre-applied depth and skips the settle
  when a consuming prim raised an error — a refusal leaves the stack it
  was handed, the spill stays, and the line-end `ERROR_RAM_FULL` stays
  loud.  Class test sweeps EMIT and `.$` on a spilled stack.
- **Oracle asserting a lifetime narrower than the design's** (F12/U1).
  The ownership oracle now accepts a stamped SUSPENDED capture; the
  ownership sweep gained suspend/resume steps in both fixtures, which is
  the TAM-driven coverage the handoff demanded for four rounds.
- **D7-3**: the three "PEM-only / inert in production" comments on the
  fold machinery were corrected — they described the window every crash
  in rounds 5 and 6 lived in as dormant.

**Numbers (RULE-1).** `make dmcp5r47 CUSTOM_PKG=packages/forth-core
CUSTOM_PKG_RECONFIGURE=1`: flash 1114632 → 1114904 = **+272 B**; ram 8884 →
8884 = **±0** (`arm-none-eabi-nm | grep -c forthConsole` = 19, package
confirmed in).  Arena untouched — no dictionary change.

**Exit state.** Thirteen fixes are new code written in one day.  By this
project's own regression record (r2 4/7, r3 4/4, r5 9/12 findings from the
previous round's fixes) round 7 audits THIS commit before anything else,
and the earliest audit close moves to round 8.
