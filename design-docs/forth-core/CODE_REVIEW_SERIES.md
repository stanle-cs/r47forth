# Correctness review series — C47 forth-core

> **Superseded for anything worth more than one pass by
> `CODE_AUDIT.md`** (2026-08-06), which runs the same hunt with several
> blind readers, an adversarial refutation pass, and a report as its
> output. This series remains the record of R1–R5 and is still the right
> shape when you want one area, one session, and a Qwen prompt list at
> the end rather than a report.

**What this is.** A personal hobby project: firmware for a DM42-class pocket
calculator (a fork of the open-source C47/R47 firmware,
gitlab.com/rpncalculators/c43). Single-user handheld — no network stack, no
untrusted input, no privilege boundary, no multi-user model. The owner types
programs on a rubber keypad and runs them. The worst outcome of any bug
discussed here is that the calculator reboots and the owner loses the program
they were typing. This is ordinary code review of my own project.

**For:** GPT-5.6 Sol (~256K context), repo at `/home/stan/c43`, branch
`forth-core/pem-entry-fixes`.

**Five sessions, R1→R5.** Each is independent and sized to fit. For each: paste
the **SHARED PREAMBLE**, then the one **R block**, into a *fresh* session. Do not
run two in one session — the reading budgets do not compose, and a session that
overflows will compact away the framing and the conventions.

**Deliverable per session:** not a report. A **Qwen prompt list** for that area,
plus an architect section. Format in the preamble; it is mandatory.

Run order: R1 first (it finds the wrong claims everything else is built on).
R2–R5 are independent of each other.

| | area | reads | ~budget |
|---|---|---|---|
| R1 | DESIGN.md ↔ code: are the `[VERIFIED:]` citations true? | DESIGN.md + cited slices | ~75K |
| R2 | The test suite: tests that cannot fail | test_dict_reloc.c + DESIGN.md §8/§9 | ~100K |
| R3 | PEM entry code + robustness | manage.c, decode.c, slices | ~70K |
| R4 | The Forth engine + the unwritten series B/C/D | forth_*.c + design excerpts | ~40K |
| R5 | Package system + tooling | tools/, build-test.sh | ~40K |

**Who may read DESIGN.md:** R1 (whole), R2 (sections, as needed), R4 (§2–§4).
R3 and R5 have their design context quoted inline and do not need it. The
"never read DESIGN.md" rule that appears in the output skeleton is a rule for
**Qwen**, the downstream implementer — never for you.

**Each session must end with `git diff` clean and the gate green.** See
"Experiments" in the preamble. A session that leaves the tree broken has failed
regardless of what it found.

---

# SHARED PREAMBLE (paste at the top of every session)

## The project

C47/R47 firmware for a DM42-class pocket calculator; `packages/forth-core/` adds
a Forth to the RPN calculator. Personal hobby fork. No network, no untrusted
input, no privilege boundary — the only "input" is what the owner types or loads
from their own save file. Bad pointers and early exits discussed here are
ordinary robustness bugs on a single-user handheld. Routine code review.

Core design principle:

> Forth is an **extension of RPN**, not a separate system layered beside it.
> Where a choice exists, follow the C47 convention.

**Flash/RAM is the binding constraint** (it is a pocket calculator). A fix that
costs bytes in the production build must justify them.

**The package system.** `packages/forth-core/` is a flat working area mirroring
upstream paths (`packages/forth-core/programming/manage.c` overrides
`src/c47/programming/manage.c`). `tools/pkg_patch_refresh.py` classifies each
file automatically and regenerates `patches/` + `files/`. **The build reads only
the generated `patches/`+`files/`, never the working area.** Never edit anything
under `src/`. Never edit `patches/`/`files/` by hand.

The only build/test command is `./packages/forth-core/build-test.sh` (it
refreshes first, injects `-DFORTH_DEBUG_SELFTEST`, runs the suite headless).
Success = `FORTH SELF-TEST: ALL PASSED` + `==> BUILD + SELF-TEST GREEN.` + exit 0.
**Never invoke meson/ninja directly** — a hand-rolled build compiles the previous
patch content *and* omits the whole self-test suite, reporting green either way.

## Context budget — read this before your first tool call

Your context is ~256K. The repo is far larger. **Never open a file without
checking its size first.**

- **`packages/forth-core/items.c` is ~277K tokens — bigger than your whole
  context.** Opening it ends the session. `grep -a` it, never read it.
- `softmenus.c` ~85K. `test_dict_reloc.c` ~70K. `keyboard.c` ~54K.
  `DESIGN.md` ~42K. `manage.c` ~20K.
- The `forth_*.c` engine files plus their headers are small (~15K total) — those
  you may read whole.

Your R block names your reading budget. Stay inside it. Grep to locate, read a
slice to confirm. If you find yourself wanting a file outside your block's list,
that is a signal the finding belongs to a different session — note it and move on.

## Two traps that have already produced wrong conclusions

> **Use `grep -a`.** `keyboard.c`, `test_dict_reloc.c` and build logs trip grep's
> binary heuristic and are **silently skipped** without it. If a grep returns
> nothing surprising, re-run with `-a` before believing it. This has caused two
> wrong conclusions here.

> **Log adjacency lies.** `errorf` is unbuffered stderr; test `printf` is buffered
> stdout. Diagnostics appear in the merged log *earlier* than the test that
> emitted them, and if the simulator exits early the buffered label of the
> running test is lost with it. **Never attribute a log line to a test by
> adjacency** — use gdb and get a backtrace. This has caused two wrong
> conclusions here, including one confidently reported and later retracted.

## What I need from you

Be skeptical. The suite is green, and that is the problem — it has been green
while being wrong at least four times in the last two days, and each time someone
leaned on the green to conclude the code was right.

Not style. You are looking for:

- claims in the design the code does not honour,
- claims that cite a line which does not say what is claimed,
- tests that cannot fail,
- tests that pass for a reason other than the one stated,
- code correct only because a caller happens to be well-behaved,
- anything where "it works" rests on a fact nobody checked.

**Start from the assumption that the author (Claude) was wrong.** The taxonomy
below is real mistakes from this codebase — a checklist, not history.

## The failure taxonomy — look for more of each

1. **Unverified predicate asserted as fact.** A spec claimed
   `forthEntryStateAtInsertion()` would be false after an empty-ENTER; it is
   true, because deleting the placeholder leaves the opening marker standing.
   DESIGN.md had it right ("non-empty"); the prompt dropped the word and asserted
   the opposite. → *Find every "will be" / "cannot" / "is always" nobody ran.*
2. **A wrong `[VERIFIED: file:line]` citation.** DESIGN.md §8.4 **E4** claims the
   per-key re-insert path keys on "`tam.function` / the step's own opcode" and
   cites manage.c. It keys on **`currentStep[0]` only** (manage.c:970) — never
   `tam.function`. The citation is decoration. ~63 lines carry `[VERIFIED:]`.
3. **Contract change without migrating the tests that encode the old contract.**
   Twice (bare render; the E5 multi-line lock). Both gates were declared green
   while unsatisfiable.
4. **A spec demanding an unreachable chain.** A task said "drive `runFunction`…
   then assert" — but `executeFunction` (keyboard.c:981) and `_closeCatalog`
   (:490) are **file-static**. No test could. The implementer improvised a
   simulation that *fails a correct implementation*.
5. **Priming the state under test.** The deepest one. *All four hardware defects
   shipped with passing tests*, because the tests set `catalog = CATALOG_NONE`
   and hand-assigned `tam.function` — the very state the code derives. One such
   test left a badly-formed step in the simulator's program buffer, and the
   simulator exited when it later walked the listing.
6. **A test that cannot fail.** `test_forth_alpha_gesture_resumes_forth` seeded
   `tam.function = ITM_FORTH`, satisfying the very `else if(tam.function !=
   ITM_FORTH)` guard it was meant to prove — it passed with the feature deleted.
   → *For each test, state the mutation that makes it RED. If you cannot, it is
   decoration.*
7. **Silent-green tooling.** `build-test.sh` did not run `refresh`, so edits were
   invisible to the compiler and the gate passed on the previous code — while the
   script's own header said reconfiguring sufficed. → *Distrust any doc that
   reassures you.*
8. **Blunt tools causing silent drift.** A regex cleanup destroyed 52 call sites
   (`foo()` → `foo`) in DESIGN.md; the repair over-corrected to 148.

## How to work

- **Verify, do not reason.** A claim you did not execute is a hypothesis. Say
  which is which. Build/run with `./packages/forth-core/build-test.sh`.
- **Show your evidence.** Concrete input/state → wrong output. For a test defect,
  give the change that *should* make it fail and show it does not. Speculation is
  fine if labelled.
- **Rank by what it costs the person holding the calculator**, not tidiness. A
  wrong `[VERIFIED:]` citation outranks a naming nit — it is how the next five
  bugs get written.
- **Do not fix anything. Do not commit.** Never `git stash`/`reset`/`checkout --`
  — the tree carries uncommitted work and `stash@{0}` is a foreign stash from
  another branch.
- If a category is clean, say so plainly. An honest "I could not find a problem
  here, here is what I checked" beats a padded list.

## Experiments: how to verify without wrecking the tree

You are asked to verify empirically, and that means editing files and building.
Do it — but under this protocol, because a previous session left probe code in
`test_dict_reloc.c` that **did not compile**, and the next session found it,
could not tell whose it was, and reported it as "foreign WIP" and a build blocker
rather than doing its own job.

1. **Probe freely.** Add temporary tests, call functions directly, mutate code to
   see a test go red. This is the job.
2. **Mark every probe** with a comment containing `AUDIT-PROBE R<n>` so any later
   reader knows the session and that it is temporary.
3. **Revert every probe before you finish.** `git diff` must be empty for any
   file you touched, and `./packages/forth-core/build-test.sh` must exit 0, before
   you write your output. **A session that ends with a broken or dirty tree has
   failed, whatever it found.**
4. **Never delete an existing test to make room.** One session removed the FIX-6
   free-list block while adding probes.
5. If a probe is worth keeping, do not keep it — **put it in your Qwen prompt as
   a task**, with its verified mutation. Your probe is evidence; the Qwen task is
   the deliverable.
6. Still forbidden: committing, `git stash`/`reset`/`checkout --`. If you have
   dirtied something you cannot cleanly revert, **say so loudly at the top of your
   output** with the exact paths. Do not leave it silent.

Read `git status` at the start. If the tree is already dirty, that is the
previous session's debris or the owner's work — **report it and stop**, do not
adopt it and do not clean it up.

## Output contract

Two files per session.

**(a) `QWEN_PROMPTS_R<n>_<slug>.md`** — the fix list. Qwen is a local implementer
model with a **~100K context window**. It follows specs literally and makes no
design decisions. It cannot see your reasoning. A task it cannot complete exactly
as written will be improvised — improvisation here has already produced a test
fixture that made the simulator exit, and a test that fails a correct
implementation.

> **The skeleton below is a template for the file YOU WRITE. Its rules are
> addressed to Qwen, not to you.** In particular Qwen is told never to read
> DESIGN.md — that restriction is Qwen's, because its prompts must carry their
> own spec slices. It does **not** apply to you. A previous session read this
> template, applied Qwen's rules to itself, and refused to open DESIGN.md at all.
> Your own reading rules are in your R block and nowhere else.

Skeleton, follow exactly:

```markdown
# <title> — Qwen implementation prompts

<N> tasks, strictly ordered. Each is sized for a ~100k-token context window: it
names the only files and line ranges to read, and never requires reading
DESIGN.md.

**How to use:** paste the PREAMBLE, then one task block, into a fresh Qwen
session. Do not run tasks out of order.

---

## PREAMBLE (paste at the top of every task)

You are implementing one small, fully specified task in the C47 calculator
firmware repo at /home/stan/c43. You are an implementer, not a designer: follow
the spec exactly, make zero design decisions. If an anchor (a quoted line,
function, or search string) does not match what you find, STOP immediately and
report the mismatch instead of guessing.

Rules:
1. Confirm `git branch --show-current` is `<branch>`. If not, STOP.
2. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success = `FORTH SELF-TEST: ALL PASSED` and `==> BUILD + SELF-TEST GREEN.`
   and exit 0. Never invoke meson or ninja directly — a hand-rolled build omits
   the self-test suite entirely and reports green having asserted nothing.
3. All edits go in `packages/forth-core/`. Never edit `src/`. The build reads
   only the GENERATED `patches/`+`files/`; build-test.sh refreshes first, so
   using the gate is sufficient. Never hand-edit `patches/`/`files/`.
4. Never touch `src/c47/core/freeList.c` or any copy. Never read DESIGN.md or
   DESIGN-HISTORY.md — your prompt carries every slice you need. Never read
   items.c (it is enormous) or test_dict_reloc.c in full; read only the ranges
   listed. Use `grep -a`.
5. Match surrounding code style. Keep upstream-derived files byte-identical
   except the marked change, so the generated patch stays small.
6. Do not commit unless told. Never `git add -A`. **Never run `git stash`,
   `git stash pop`, `git reset`, `git checkout -- <file>`, or `git restore`** —
   the tree carries uncommitted work and `stash@{0}` is a foreign stash from
   another branch. If you think you need to undo something, STOP and report. A
   red gate is safe; a mangled tree is not.
7. If the gate goes red on a test asserting the OLD behaviour your task was
   written to change, that test is part of your task — but STOP and report
   before touching it. Never make a test pass by weakening the change it caught.
   If a task changes a contract without listing the tests that encode it, the
   spec is wrong: say so and stop.
8. Report what you changed, the gate output, and anything that surprised you.

---

## <ID> — <imperative title>

**File(s):** <paths>

**Read:** <grep anchor, or a line range ≤ 60 lines>. Trust the grep anchor, not
the line number — earlier tasks shift lines.

**The defect.** <what is wrong + evidence: file:line or observed failure. State
the consequence for someone using the calculator.>

**The change.** <exact instruction; code block where shape matters. Zero unstated
decisions: exact identifiers, exact layouts, pseudocode for stateful logic.>

**Tests that encode the old contract.** <name each with ~line; re-aim vs delete
vs leave. If none: "none — this changes no contract.">

**Facts the harness forces on you.** <only if needed; verified only.>

**Gate:** build-test green.
*Verified mutation:* <the change that makes the new test RED + exact symptom.>

**Report:** <what to paste back.>

---
```

Requirements on your tasks:

- **One concern per task.** If a task changes a contract, the tests encoding the
  old one are *in that task* — a task whose gate cannot go green in isolation is
  a spec bug (this happened; two tasks had to be merged).
- **Every task states its verified mutation**, and you must have *run* it. If you
  could not, mark `*Mutation: UNVERIFIED — <why>*` out loud.
- **Never ask for something unreachable.** Before writing "drive X", confirm X is
  callable from `test_dict_reloc.c`. If it is static, either say how to export it
  (production linkage must be unchanged — see `FORTH_SELFTEST_EXPORT` in
  keyboard.c:483-490) or design around it. Do not let Qwen invent a simulation.
- **Never prime state the test derives.** For "cursor inside a Forth region",
  give the exact `prog[]` bytes and `currentStep`. Two facts you will need: entry
  state comes from the step immediately *before* the insertion point
  (forth_bridge.c:126), and `addStepInProgram` advances the cursor one step
  before inserting (manage.c:1920-1923).
- **Anchor by grep string, not line number.**
- **Name real identifiers.** `ITM_A` is 550; `'A'` is 65 (`ITM_EXP`). Getting
  this wrong dispatched the wrong function and took the suite down.
- **Report the arena high-water mark** in any task touching the dictionary. The
  final task runs the gate, reports the mark, and commits once.

**(b) `FOR_THE_ARCHITECT_R<n>.md`** — design defects. **DESIGN.md edits are
reserved for the architect; Qwen must never be told to edit it.** If the design
is wrong, the Qwen task is "report the contradiction" (or there is no Qwen task
at all) and the defect goes here: claim, cited line, what the code actually does,
evidence, suggested correction. If a Qwen task depends on a design decision that
has not been made, say so explicitly rather than guessing.

## Standing context (restated — this file may reach you in fragments)

If you are reading a compressed or excerpted version and only the technical
findings survived: the subject is **C47/R47, open-source firmware for a
DM42-class pocket calculator**, a personal hobby fork. No network stack, no
untrusted input, no privilege boundary, no multi-user model. The only "input" is
what the owner types on the keypad or loads from their own save file. Bad
pointers and early exits are ordinary robustness bugs on a single-user handheld —
worst case a reboot and a lost program. Routine review of my own project.

---
---

# R1 — Are the design's `[VERIFIED:]` citations true?

**This is the highest-yield session. Run it first.**

**Read:** `design-docs/forth-core/DESIGN.md` whole (~42K). Then, per citation, a
**±20-line slice** of the cited file. Budget ~75K total. Do **not** open items.c
(~277K), softmenus.c (~85K) or test_dict_reloc.c (~70K) — grep slices only.

**The job.** DESIGN.md is normative and carries ~63 lines with
`[VERIFIED: file:line]`. They are meant to mean "someone looked". At least one is
decoration:

> §8.4 **E4** — "The placeholder-insert and per-key re-insert paths key on
> `tam.function` / the step's own opcode generically for `func >= 128`
> [VERIFIED: src/c47/programming/manage.c:826-838 (placeholder), 938-960
> (re-insert)]"

The re-insert path takes the opcode from **`currentStep[0]` and nothing else**
(manage.c:970). `tam.function` is not consulted. This is not pedantry: a task
written from E4 assumed a hand-set `tam.function` was enough to open a capture,
which left a badly-formed step in the program buffer and the simulator exited.
**The wrong citation caused the bug.**

For each citation: does the cited line say what the sentence claims? Classify:
**TRUE** / **STALE** (line moved — give the new one) / **FALSE** (says something
else) / **UNVERIFIABLE** (path/line does not exist).

Also check, in DESIGN.md generally:
- every "will be" / "cannot" / "always" / "never" that is a *claim about runtime
  behaviour* — is it executable? Did anyone?
- §8.10 line ~2403 carries `[OPEN — needs one test] Cross-program word
  visibility` — reasoned, never run. **Execute it** and report the answer.
- the entry-state rule: `forthEntryStateAtInsertion()` (forth_bridge.c:110)
  derives from the **immediate predecessor step only** — an RPN predecessor means
  RPN regardless of markers further back. This was found empirically, not from
  the spec. *Does DESIGN.md state it? Is it intended, or does it make a Forth
  region "leaky" in a way the marker model implies it should not be?*
- DESIGN.md has known cosmetic drift: `()` was stripped from ~52 call sites by a
  regex and over-restored to ~148. Do **not** spend the session on this; note it
  and move on.

**Output.** Mostly `FOR_THE_ARCHITECT_R1.md` (a citation table + the
contradictions). `QWEN_PROMPTS_R1_*.md` only for defects where the *code* is
wrong and the design is right.

---
---

# R2 — The test suite: which tests cannot fail?

**Read:** `packages/forth-core/test_dict_reloc.c` (~70K, ~7270 lines). Read it
whole if you must, but prefer grepping `static int test_` and reading each test.
Then ±20-line slices of subjects to confirm.

**You MAY and SHOULD read `DESIGN.md` (~42K) here** — grep to the sections a test
claims to pin (`§8.4`, `§8.9` acceptance, `§9`) and read those. You cannot judge
whether a test asserts the *right* thing without the contract it is supposed to
encode; a test can be perfectly self-consistent and still pin the wrong
behaviour. Read sections, not the whole file, and only for tests whose intent is
unclear. (The "never read DESIGN.md" rule in the output-contract skeleton is
**Qwen's**, not yours.)

Budget ~100K. **Do not** open items.c (~277K — it will end your session),
softmenus.c or keyboard.c whole.

**The job.** All four hardware defects shipped with *passing* tests. The suite's
credibility is the thing under review.

If a test's intent contradicts DESIGN.md, that is an architect finding, not a
Qwen task — the design may be right and the test wrong, or the reverse, and you
should say which you think it is and why.

For every `test_*`:

1. **State the mutation that makes it RED.** If you cannot name one, the test is
   decoration — list it.
2. **Does it prime the state its subject derives?** Look for direct assignment of
   `tam.function`, `catalog`, `FLAG_ALPHA`, `currentStep`, `calcMode`,
   `fnKeyInCatalog` where the code under test is supposed to *compute* them.
   Precedent: `test_forth_alpha_gesture_resumes_forth` seeded
   `tam.function = ITM_FORTH` and passed with the feature deleted.
3. **Does it pass for the stated reason?** Several docstrings name an "escaping
   mutation" — spot-check the newest ones by actually applying the mutation.
4. **Fixture correctness.** `writeTestProgram` appends `.END.`; some `prog[]`
   include an explicit `ITM_END` and some do not. `addStepInProgram` advances the
   cursor one step before inserting (manage.c:1920-1923), so a cursor parked on
   an RPN step makes *that step* the predecessor. Are any fixtures asserting
   something other than what they set up?
5. **Leaked globals between tests.** `fnKeyInCatalog` was set by one test and not
   cleared (`showSoftmenu` also clears it — ordering matters). What else leaks?

Three tests deliberately provoke the free-list guard
(`test_freelist_double_free_guarded`, `test_freelist_interior_double_free`,
`test_freelist_no_mutation_on_oversize_free`) and print `errorf` diagnostics on
purpose. That output is expected — it was already misread once as a real bug via
log adjacency. Do not repeat that; see the buffering trap.

**Output.** `QWEN_PROMPTS_R2_tests.md` — one task per test or coherent group.
Every task must give the mutation and its symptom.

---
---

# R3 — PEM entry code + robustness

**Read:** `packages/forth-core/programming/manage.c` (~20K, read whole),
`programming/decode.c` (~9K, whole), `forth_bridge.c` (~1K, whole),
`src/c47/programming/nextStep.c` (small), plus **grep slices only** of
`keyboard.c` (~54K) and `softmenus.c` (~85K). Design context is quoted below —
do **not** open DESIGN.md. Budget ~70K.

**The job.** The just-landed PEM entry series (commit `dcc8d6594`): the FORTH
toggle from Catalog, the multi-line lock, bare rendering, the ALPHA gesture.

The design rules it implements (quoted so you need not open DESIGN.md):

> **E3 (empty-commit).** When capture ends with `tam.function == ITM_FORTH` and
> `aimBuffer[0] == 0`, delete the placeholder step instead of committing — an
> empty committed line would be byte-identical to a marker and flip every
> subsequent marker's parity.
>
> **E5 (multi-line lock).** In `pemAlpha`'s `ITM_ENTER` arm, after
> `pemCloseAlphaInput()` commits a **non-empty** Forth line, re-derive
> `forthEntryStateAtInsertion()` and, if true, re-open capture:
> `tam.function = ITM_FORTH; pemAlpha(0);`. The state is recomputed from the
> program bytes at the cursor, never stored.
>
> **E6 (ALPHA gesture).** `ITM_AIM` inside a Forth region must open a **Forth**
> capture, not a string-literal one.

Specific leads:

- **`findKey2ndParam` (src/c47/programming/nextStep.c:291)** does
  `indexOfItems[op].status` without range-checking `op`, which is read straight
  from the program bytes. `LAST_ITEM` is 2870; a badly-formed step gave `op =
  32001`, well past the end of the table, and the desktop simulator exited.
  Upstream code, so a fix goes through the package system. *Can a listing reach
  that state in ordinary use — a half-finished edit, a truncated save file,
  `saveRestoreBackup` loading an older format? If it is only reachable from a
  broken test fixture, say so and I will drop it.*
- **`scanLabelsAndPrograms` (manage.c:103-190)** counts labels with
  `*step == ITM_LBL` in the first loop and `checkOpCodeOfStep(step, ITM_LBL)` in
  the second, and the two loops `break` at different points. The first sizes the
  `labelList` allocation; the second fills it. Convince yourself the second can
  never write more entries than the first counted — or show the program shape
  where it does.
- **`_closeCatalog` is exported under `FORTH_DEBUG_SELFTEST`**
  (`FORTH_SELFTEST_EXPORT`, keyboard.c:483-490), so the tested binary and the
  shipped binary differ in linkage. *Is anything else static-in-production that
  tests reach? Does the macro name collide? Does losing `static` cost flash — the
  compiler can no longer prove it has no external callers?*
- **CONFIRMED BUG, already found by an earlier session — do not re-derive it,
  but do check for siblings.** The catalog drain in the `ITM_FORTH` arm
  (manage.c, `while(currentMenu() == -MNU_CATALOG || ... ) popSoftmenu();`)
  only drains while the **top** of the softmenu stack is a catalog menu, but
  `_closeCatalog` (keyboard.c:490) decides `inCatalog` by scanning the **entire**
  stack. Put any menu that is not in the drain list on top of a buried
  `MNU_CATALOG` (e.g. `showSoftmenu(-MNU_CATALOG); showSoftmenu(-MNU_CLK);`) and
  the drain stops immediately, `pemAlpha` pushes `MNU_ALPHA`, `_closeCatalog`
  finds the buried `MNU_CATALOG`, falls to its `default:` branch (keyboard.c:514)
  → `closeAllCatalogMenus()` (:517) → and pops `MNU_ALPHA`, because `MNU_ALPHA`
  is itself listed in `CatalogMenus[]` (keyboard.c:443). That is the original
  hardware defect, still live. The two predicates disagree: one is top-of-stack,
  the other is stack-wide. *Is the buried-catalog state reachable in normal use,
  or only constructible in a test? Are there other places where a top-of-stack
  test is paired with a stack-wide one?*
- **The `hadText` gate** in the `ITM_ENTER` arm distinguishes "commit this line,
  give me the next" from "I'm done". Both snapshots must be taken *before*
  `pemCloseAlphaInput()` (it clears `tam.function` and `aimBuffer`). *Is there
  any path where `aimBuffer[0] != 0` but the line is not committed, or vice
  versa?*
- **Bare rendering** (decode.c `decodeRem`): a Forth source step renders its
  payload with no name prefix and no quotes, because a string-literal step
  already renders `'text'` with quotes and the two would be indistinguishable.
  *Is any other step type now ambiguous with a bare Forth line? What about
  `STD_LEFT_SINGLE_QUOTE` appearing inside a payload?*

**Output.** `QWEN_PROMPTS_R3_pem.md` + `FOR_THE_ARCHITECT_R3.md` for anything
where the design, not the code, is wrong.

---
---

# R4 — The Forth engine, and whether the unwritten design holds

**Read:** `forth_bridge.c`, `forth_dict.c`, `forth_inner.c`, `forth_compile.c`,
`forth_prims.c`, `forth_dict.h`, `forth_prims.h` — **all small, ~15K total, read
them whole.** Plus `DESIGN.md` §2–§4 only (token encoding, the interpreters,
vocabulary) — locate with grep, read the sections, do not read the file whole.
Budget ~40K.

**The job.** Two halves.

**(a) The engine as built.** Dictionary, token encoding, the inner and outer
interpreters, re-entrancy, the pre-scan. Read for real defects.

**(b) Does the *unwritten* design hold together?** Three series are specified in
DESIGN.md but not implemented. Read their specs and find the contradictions
*before* anyone writes them — this is the cheapest bug-fixing in the whole review:

- **B — vocabulary.** `forthFindItem` (factored out of `forthResolveXEQ`), items
  resolving *before* labels in §4.1's order, `FTOK_C47` emission, and a new
  `FTOK_XEQN 0x7F05` token (`1 (len) + len name bytes, zero-padded to a whole
  cell`) for name-resolved label calls. The stated principle: *bake ids that are
  stable; name-resolve ids that aren't* — item ids are flash constants (bake),
  colon indices are stable within a generation (bake), label ids renumber on
  every edit (name-resolve).
- **C — parameterised items.** `STO 05` under the C47 convention (decided);
  `5 STO` stack convention (rejected). `PTP_REGISTER` decoder widening.
- **D — entry-time validation.** Check-only tokenize+resolve on commit.

For each: does the design contradict the code as it exists? Is the token space
consistent (`FTOK_XEQN` at 0x7F05, reserved `0x7F06..0x7FFF`)? Does cell
alignment work? Does `FTOK_XEQN` interact correctly with the run-generation
counter and the `PGM_RUNNING` wrap? Is the §4.1 resolution order (prim → colon →
number → item → label) actually implementable given `forthResolveXEQ`'s current
shape?

**Output.** `FOR_THE_ARCHITECT_R4.md` is likely the bigger half here.
`QWEN_PROMPTS_R4_engine.md` for defects in the code as it stands. Do **not**
write Qwen tasks implementing B/C/D — they are unspecified until the architect
resolves what you find.

---
---

# R5 — Package system and tooling

**Read:** `tools/pkg_patch_refresh.py` (~9K), `tools/test_pkg_patch_refresh.py`,
`tools/pkg_patch_common.py`, `tools/resolve_c47_src.py`,
`packages/forth-core/build-test.sh`, `packages/forth-core/.pkgignore`,
`packages/.gitignore`, root `meson.build` (grep for CUSTOM_PKG),
`design-docs/package-manager/PROPOSED_SPEC_CHANGES.md`. Budget ~40K.

**The job.** This tooling decides *what the compiler sees*. It has already lied
once: `build-test.sh` did not run `refresh`, so the build compiled the previous
patch content while reporting green — and the script's own header said
reconfiguring was sufficient. Assume it is still lying somewhere.

Specific leads:

- **`.pkgignore` sharp edge (accepted, not fixed).** Package `.c` files compile
  from the `files/` copy, so ignoring one **silently removes it from the build**
  rather than erroring. Currently ignores `*.md`, `*.txt`, `build-test.sh`. *Is
  that acceptable? Is there a cheap check that does not reintroduce a
  declaration to keep in sync (which the design explicitly abolishes)?*
- **Base pinning (BP-1..BP-7)** in `pkg_patch_refresh.py` — `--materialize`,
  `--rebase-base`, `base_commit` in the manifest, the conflict-marker guard.
  This landed in commit `e3e1e8224` **unreviewed by its committer**, who said so
  in the commit message. Review it properly. Does `--rebase-base` lose work? Is
  the pre-scan/mutation split actually atomic? What happens on a shallow clone?
- **The manifest** (`.refresh-manifest.json`) is a single generated file
  recording base commit, patch hashes and files/ hashes. It could not be split
  cleanly across two commits, so commit `e3e1e8224` is transiently inconsistent
  with its own manifest. *Does that self-heal as claimed, or does the drift
  warning fire forever?*
- **Mode noise.** Refresh now bakes `100644 → 100755` mode changes into patches
  (`010-defines.h.patch`, `010-programming__lblGtoXeq.c.patch`) because the
  working area has executable bits on `.h`/`.c` files. Harmless or not?
- **The self-test define.** `build-test.sh` injects
  `-Dc_args=-DFORTH_DEBUG_SELFTEST` via `meson configure` because the resolver
  ignores `packages/*/meson.build`. *Is that injection robust? Does it survive a
  reconfigure? Can the suite silently vanish again?*
- **`AGENTS.md`** is auto-loaded into every Qwen session. It previously
  documented a build recipe that silently disabled the tests. *Is it accurate
  now?*

**Output.** `design-docs/package-manager/QWEN_PROMPTS_R5_tooling.md`. Note: tooling lives in `tools/`, not
`packages/forth-core/`, so tasks here are exempt from the "all edits go in
packages/forth-core/" rule — say so explicitly in any task that touches `tools/`,
and give the test command (`python3 tools/test_pkg_patch_refresh.py`).

---
---

# If a session finds little

Say so, and hand back a short list. Do not manufacture tasks. But before
concluding that: re-read the failure taxonomy and confirm you actually checked
the `[VERIFIED:]` citations (R1) or ran a mutation against the newest tests (R2).
Those two are where the bodies have been.
