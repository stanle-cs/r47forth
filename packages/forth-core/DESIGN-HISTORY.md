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
