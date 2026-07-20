# Stage F6-5 — the dictionary-backed word catalog

Origin: DESIGN §10.6 (the §4.3 dynamic-catalog fold, ruled 2026-07-16)
via `QWEN_PROMPTS_F6_core.md` §2 decision 8 and the T4 trace of
`F6_AUDIT_RESULTS.md`.  The landed §8.6 picker is a TEXT SCAN: it lists
`: NAME` definitions found in the edited program's source lines before
the cursor and never reads the dictionary (E1 answered statically).
This packet turns `MNU_FORTH` into the ruled UNION catalog of three
provenance sections: (a) the edited program's words — the landed text
scan, verbatim, kept because it needs zero executions; (b)
interactive-scope dictionary words; (c) global dictionary words.  The
menu id, softmenu machinery, rebuild-always discipline, and the P-H7
insert path are unchanged.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. Clean tree; `git log --oneline -1` shows the F6-4 commit
   `forth-core: F6-4 — parameter entry emits canonical text`.
2. `grep -n "case MNU_FORTH" packages/forth-core/softmenus.c` → exactly
   ONE match (the landed builder this packet extends).
3. `grep -rn "forthDictBrowseName\|forthGDictBrowseName" packages/forth-core`
   → ZERO matches (this packet adds them).
4. `grep -n "FORTH_OWNER_INTERACTIVE" packages/forth-core/forth_dict.h`
   matches; `grep -n "FF_SMUDGE" packages/forth-core/forth_dict.h`
   matches (the filter constants).
5. `grep -n "gend_word" packages/forth-core/test_dict_reloc.c` matches
   (the F3-2 g-helpers the fixture reuses).
6. `grep -n "== -MNU_FORTH" packages/forth-core/softmenus.c` shows the
   rebuild-always disjunction (content freshness is structural — no
   invalidation work in this packet).
7. Pre-gate green; arena baseline from the F6-4 commit message.

---

## PREAMBLE (paste before the task)

You are implementing one small, fully specified task in the C47 calculator
firmware repository at `/home/stan/c43`.  You are an implementer, not a
designer: follow this packet exactly and make no product or architecture
decisions.  If a quoted anchor, function, test, branch, literal, or identifier
does not match the tree, STOP and report the mismatch instead of guessing.

Rules:

1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes` and run
   `git status --short`.  The tree must be clean before any edit.  Otherwise
   STOP.
2. Before reading or editing a task file, write a tight todo list to the FILE
   `/tmp/forth-f6-5-todo.md` (outside the repo): one item per file, helper,
   test subcase, mutation, final gate, parity check, and report.  Keep it
   updated; mark each item in progress and completed as you work, and append
   `MUTATION APPLIED: <n>` / `MUTATION RESTORED: <n>` immediately.  Do not
   report success with an open item.
3. The only build/test command is `./packages/forth-core/build-test.sh`.
   Success requires exit 0 plus both `FORTH SELF-TEST: ALL PASSED` and
   `==> BUILD + SELF-TEST GREEN.`  Never invoke meson or ninja directly.
   Always capture the output:

   `./packages/forth-core/build-test.sh > /tmp/forth-f6-5-gate.log 2>&1; echo "gate exit: $?"`

   Inspect only bounded slices: `tail -n 12`, targeted PASS/FAIL greps, and at
   most one small context window around a failure.  Never read the full log.
4. Edit only the flat files named by this packet under
   `packages/forth-core/`.  Never edit `src/`, generated `patches/`, or
   generated `files/`; the gate refreshes the generated package view.  Never
   touch `src/c47/core/freeList.c` or any copy.
5. Never read `DESIGN.md` or `DESIGN-HISTORY.md`.  Never read
   `softmenus.c`, `forth_dict.c`, `keyboard.c`, or `test_dict_reloc.c` in
   full.  Grep the named anchors and read only the specified local slices.
6. Do not change an old-contract test unless this task names it.  If another
   test reddens, STOP before editing it.
7. Never run `git stash`, `git stash pop`, `git reset`, `git checkout --`, or
   `git restore`.  Restore mutations by manually reversing only the mutation
   hunk.  Never use `git add -A`.
8. Report every required PASS/RED line, both final success banners, the arena
   line, `git diff --check`, generated mirror equality, and surprises.
9. Small-context recovery: this packet, `/tmp/forth-f6-5-todo.md`,
   `git status --short`, and `git diff` are the durable task state.  After any
   compaction or uncertainty, STOP the current step and re-read those sources;
   never reconstruct packet text from memory.

**Two-attempt debugger handoff.** After the implementation first fails a
required command because of your changes, make at most two distinct repair
attempts, each followed by the relevant rerun.  This does not override any
immediate STOP rule and does not apply to an expected mutation RED.  If the
second repair is not green, STOP and report `[SOL DEBUGGER HANDOFF]` with the
command, bounded failure output, both repairs/results, current status/diff,
and remaining hypotheses.

**PROGRAM-FIXTURE AUTHORING RULE (mandatory)**

`test_dict_reloc.c` program fixtures are structural, not hand-addressed.
Build behavior-test programs with `testProg_t` and its `tp*` helpers. Capture
the returned step handle when a test must execute or inspect that step, and
resolve it with `tpStepAddr`; abort the subcase if fixture construction,
`tpWrite`, or address lookup fails.

Never add `beginOfProgramMemory + <numeric literal>`, a numeric argument to
`tpStepAddr`, or arithmetic derived from preceding payload lengths. Packet
authors must identify steps by role and must not publish a calculated byte
offset as a normative literal. If a packet contains such an offset, stop
with `[SOL DEBUGGER HANDOFF]` and report the packet defect; do not repair
its arithmetic locally.

Use a typed builder accessor such as `tpSrcPayload` for an internal field.
If the needed step or field helper does not exist, extend the central fixture
builder first; do not introduce local pointer arithmetic in the test.

Prefer named opcode/parameter constants in builder helpers. An exact byte
array may remain as the expected value of an encoding assertion. Raw bytes
inserted into the program fixture are allowed only for the encoding under
test or a deliberate malformation; they must enter through `tpRaw`, carry an
adjacent comment naming that purpose, and still use the returned handle and
builder-derived logical end. `tpRaw` is never a shortcut for an ordinary
behavior fixture.

This rule is prospective. Do not widen the task by converting untouched
legacy fixtures.

---

## F6-5 — three provenance sections, one menu

### Authority carried by this packet (no open choices)

1. **Browse enumerators** (forth_dict.c; prototypes in forth_dict.h):

   ```c
   /* F6-5 browse surface: copy the name of the n-th listable entry
    * (newest-first) into out (>= 15 bytes, NUL-terminated).  Listable =
    * not FF_SMUDGE, nameLen 1..14, and (fdict variant) owner matches.
    * Returns false when fewer than n+1 listable entries exist.  This is
    * a BROWSE surface: it reads owners directly and never enters a
    * program-step scope (F3-3A: scope entry is for execution only). */
   bool_t forthDictBrowseName(uint16_t n, uint16_t owner, char *out);
   bool_t forthGDictBrowseName(uint16_t n, char *out);
   ```

   Both are the fdict/gdict newest-first link walks of the shared colon
   walk (same traversal shape), counting only listable entries; names
   longer than 14 glyphs are SKIPPED entirely (the landed picker's slot
   rule), never truncated.  Alongside them add the test hook (same file,
   FORTH_DEBUG_SELFTEST only):

   ```c
   #if defined(FORTH_DEBUG_SELFTEST)
   /* F6-5 test hook: newest-first fdict walk, FIRST name match regardless
    * of owner or smudge state; set/clear FF_SMUDGE on that header.  No
    * product surface. */
   void forthTestSmudgeSet(const char *name, bool_t on);
   #endif
   ```
2. **Builder extension** (softmenus.c, `case MNU_FORTH:`): the landed
   text-scan (section a) runs UNCHANGED, filling 15-byte `tmpString`
   slots and `nNames` exactly as today, including its dedup and its
   sort-of-collected-names step and the `TMP_STR_LENGTH / 15` cap.
   AFTER it (before the landed code publishes
   `dynamicSoftmenu[menu].numItems`), append:

   - Section (b): `forthDictBrowseName(i, FORTH_OWNER_INTERACTIVE, slot)`
     for i = 0.. while true and slot room remains (same cap variable);
     dedup WITHIN the section only (compare against section-b slots,
     `CMP_BINARY`, the landed dup-loop shape).
   - Section (c): `forthGDictBrowseName(i, slot)` likewise, dedup within
     section c only.
   - If the landed builder sorts its collected names, apply the SAME
     sort independently to the section-b slot range and the section-c
     slot range (three independently ordered sections, concatenated
     a-then-b-then-c).  If it does not sort, keep all three sections in
     collection order.  Do not re-sort across sections.

   Cross-section duplicates are INTENTIONAL (a name listed in two
   sections tells the user where it comes from — core decision 8); only
   within-section duplicates collapse.  `numItems` = total slots filled.
3. Insertion, guard, rebuild, and menu identity are untouched:
   `forthPickerGuard`, `pickerInsertName`→`forthCapInsertName`, the
   rebuild-always disjunction, and `dynmenuGetLabel*` all keep working
   on the extended content with zero edits.
4. Empty sections are legal (fresh boot: only section a, possibly
   empty).  The menu with zero total items keeps the landed
   empty-picker behavior, whatever the landed builder produces today
   (numItems 0) — unchanged.

### Files

Modify only: `forth_dict.h`, `forth_dict.c`, `softmenus.c`,
`test_dict_reloc.c` — all under `packages/forth-core/`.

### Targeted reads

1. softmenus.c: the whole `case MNU_FORTH:` block (bounded — read from
   the `case` line to its `break;`), nothing else.
2. forth_dict.c: the shared colon walk (grep `forthFindColonRef`) as the
   traversal model; the header layout accessors.
3. test_dict_reloc.c: one landed picker test in full (fixture +
   menuContent assertion idiom + restore block); the F3-2 g-helper block
   (grep `gend_word`, read the three helpers); the F3-3 program-step
   drive discipline (grep `test_scope_isolation`, read subcase 1's drive
   only); registration lines.

### Change A/B — as Authority items 1-2

### Change C — new test `test_word_catalog` (register after the newest test)

Fixture (order matters — the F1 lifetime discipline: build interactive
words AFTER any run-generation signal):

- Program: `tpLbl("F65")`, `tpMarker`, `sDef = tpSrc(": PW 1 ;")`,
  `tpWrite`.  (`printf '%s' ": PW 1 ;" | wc -c` = 9.)
- Drive `sDef` once per the F3-3 drive discipline (programRunStop save /
  `forthRunGenBump()` / PGM_RUNNING / `executeOneStep` / restore) so the
  fdict holds a PROGRAM-owned `PW` entry — section (b) must NOT list it.
- Interactive: `forthOuterInterpret(": WI 7 ;")` (INTERACTIVE owner).
- Global: hand-build `GVIS` with the F3-2 g-helpers (body `FTOK_ILIT`,
  int32 9, `FTOK_EXIT` — the F3-3 subcase-5 fixture shape).
- Position `currentStep` AFTER `sDef` (the landed picker tests' cursor
  idiom) and build the menu the way those tests do
  (`initVariableSoftmenu` idiom for `MNU_FORTH`).

1. **Union content and section order.**  Require `numItems == 3` and
   menuContent = `PW` then `WI` then `GVIS` in that order (positions 0,
   1, 2 — sections a, b, c).  PASS:
   `[1] PASS: catalog lists program, interactive, and global sections`
2. **Program-owned dict entries stay out of section (b).**  `PW` exists
   BOTH as text (section a) and as a program-owned fdict entry — require
   it appears EXACTLY ONCE.  (This is the owner-filter pin.)  PASS:
   `[2] PASS: program-owned dictionary entries are not double-listed`
3. **Smudged entries absent.**  `forthTestSmudgeSet("WI", true);`
   rebuild the menu; require `numItems == 2` and no `WI`.
   `forthTestSmudgeSet("WI", false);` rebuild; require `WI` back.  PASS:
   `[3] PASS: smudged entries never list`
4. **Long names skipped.**  `forthOuterInterpret(": WINTERACTIVELONG 1 ;")`
   (name length 15 > 14 — verify with `printf '%s' "WINTERACTIVELONG" | wc -c`
   = 16, glyphs 16); rebuild; require `numItems` unchanged and no
   truncated entry.  PASS: `[4] PASS: over-long names are skipped, not truncated`
5. **Cross-section duplicate shows per section.**
   `forthOuterInterpret(": PW 2 ;")` (interactive PW alongside program
   PW); rebuild; require `PW` at a section-a position AND at a
   section-b position (`numItems == 4` counting WI and GVIS).  PASS:
   `[5] PASS: cross-section duplicates list once per provenance`

Cleanup: `forthDictClear(); forthGDictClear();` restore softmenu stack
and dynamicSoftmenu[…] per the landed picker tests' restore block,
`lastErrorCode = ERROR_NONE`, `cleanupTestProgram()`.

### Existing tests

All stay green untouched.  The two landed picker tests exercise
section (a) content through the same builder — if either reddens, STOP:
the likely cause is a section-append placed before the landed publish
point, not the tests.

### Non-goals / STOP boundaries

- No scope-variable involvement (browse reads owners directly).
- No insertion/guard changes; no menu keying changes.
- No FORGET/GLOBAL interaction work beyond what the enumerators see in
  the dictionaries (F3-4 landed those semantics).
- No cap redesign: the landed 170-slot cap bounds the UNION, by scan
  order a→b→c.

### Gate and required mutations

Full gate green first (five PASS lines + every legacy banner).
Mutations, each separately, logs `/tmp/forth-f6-5-mut1..3.log`; co-reds
beyond the named RED are expected; post-restore gate green between
mutations.

1. Delete the section-(b) append loop.  Subcase 1 MUST go RED (WI
   missing).
2. In `forthDictBrowseName`, delete the owner comparison (list every
   fdict owner).  Subcase 2 MUST go RED (PW double-listed).
3. Delete the section-(c) append loop.  Subcase 1 MUST go RED (GVIS
   missing).

Report: five PASS lines, both banners, exit 0, arena line vs baseline,
`git diff --check`, generated-mirror equality, three mutation REDs.
RULE-1: negligible flash delta expected; note the measured delta.

### Commit

```text
forth-core: F6-5 — the dictionary-backed word catalog
```

---

## REGRESSION + ENTRY-POINT RULES (binding, added 2026-07-19 after the F5-2 debug)

These cost a full session at F5-2, where a correct six-line change was
blamed for four red tests it never touched. They apply to this packet
whether or not the body above repeats them.

1. **A red outside your diff is an immediate STOP — zero repair attempts.**
   The two-attempt allowance applies only to code or tests THIS packet
   authored. If a test you did not write reddens, stop and report
   `[SOL DEBUGGER HANDOFF]` at once. Do not first try to decide whether your
   change could have caused it: "my change cannot have caused this" is the
   most common wrong conclusion, and deciding it is the debugger's job.
2. **Name the blast radius by diffing PASS sets, not by reading failures.**
   Keep the pre-gate log. Then:
   `diff <(grep -o "PASS: .*" /tmp/<pre>.log | sort) <(grep -o "PASS: .*" /tmp/<gate>.log | sort)`
   Newly-missing PASS lines in untouched tests are the report.
3. **Entry-point contract pre-flight.** Before wiring an existing function
   into a new call site, prove it saves/restores the process-global state its
   siblings do — grep the other entry points for the fields the shared
   epilogue restores and compare. A function correct in isolation can be
   wrong the moment a second caller exists. A mismatch is a packet defect:
   STOP and report, do not patch around it.
4. **Pin the contract, not just the verdict.** If this packet adds an entry
   point whose spec claims state neutrality ("mutates no live state",
   "restores the mode", "leaves the buffer untouched"), pin that claim
   directly: set the state to a NON-default value, drive both the accepting
   and the rejecting path, assert the state came back. See
   `test_check_source_line` subcase 6 and `poisonAutoFrame()` for the landed
   shape — the poison makes an uninitialized restore deterministic instead of
   luck-of-the-stack.

**Working-area files (added 2026-07-19).** Every upstream file this packet
names is already materialized in `packages/forth-core/` — edit it in place.
Never `cp` a file out of `src/c47/`: patches are diffed against upstream **at
the package's recorded base commit**, so a copy from the current tree bakes
any post-base upstream change into your patch, and `refresh` does not detect
that drift. If a file this packet names is missing from the working area,
STOP and report a packet defect rather than materializing or copying on your
own initiative. Brand-new files (no upstream counterpart) are the exception:
create them in the working area and `refresh` classifies them into `files/`.
