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
(it still described the obsolete `forthArena`); `custom_package/README.md`'s
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
