# PEM bare rendering and citation repair — Qwen implementation prompts

2 tasks, strictly ordered. Each is sized for a ~100k-token context window: it
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
1. Confirm `git branch --show-current` is `forth-core/pem-entry-fixes`. If not, STOP.
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

## R3-1 — Make live Forth capture obey the bare-render contract

**File(s):** `packages/forth-core/programming/decode.c`,
`packages/forth-core/programming/manage.c`

**Read:** In `decode.c`, `grep -a -n "static void decodeRem"` and read from that
anchor through the closing brace of `decodeRem` (currently about 35 lines). In
`manage.c`, `grep -a -n "cursorInString ="` and read 15 lines before through 10
lines after that anchor. Read no other source for this task.

**The defect.** A non-empty `ITM_FORTH` source step is decoded as its bare
payload, but PEM's live ALPHA cursor insertion still unconditionally skips the
two-byte opening quote used by ordinary string literals. Concrete state:
`tam.function == ITM_FORTH`, `FLAG_ALPHA` set, payload `ABC`, and
`T_cursorPos == 0`. `decodeRem()` produces `ABC`, then `fnPem()` inserts the
cursor at byte offset 2, on top of `C`, rather than before `A`. At end of line it
can write beyond the payload's NUL so the cursor is not displayed at all. The
empty-placeholder exception is also an empty block: it falls through to the
generic REM formatter and produces `FORTH ''` instead of an empty source line.
On the calculator this makes the just-opened Forth line and its edit cursor lie
about what will be committed.

**The change.** Make exactly these two changes and no others:

1. In `decodeRem()`, replace the empty transient-capture comment-only body with
   an empty result and an immediate return:

   ```c
      if (opcodeStart == currentStep && getSystemFlag(FLAG_ALPHA) && tam.function == ITM_FORTH) {
        tmpString[0] = 0;
        return;
      } else {
   ```

2. In `fnPem()`'s `FLAG_ALPHA` cursor block, preserve the existing REM,
   `42`-string, and ordinary-literal offsets exactly, but give live Forth capture
   a zero-byte prefix. The later code always adds 2, so assign
   `T_cursorPos - 2` for Forth:

   ```c
          if(tam.function == ITM_FORTH) {
            cursorInString = T_cursorPos - 2;
          }
          else {
            cursorInString = (strcmp(tmpString, "REM ") == 0 ? T_cursorPos + 4 : (strcmp(tmpString, "42" STD_alpha) == 0)  || (strcmp(tmpString, "42" STD_RIGHT_TACK) == 0) ? T_cursorPos +5 : T_cursorPos);
          }
   ```

   Leave the existing `xcopy(tmpString + 2 + cursorInString + 2, ...)` and cursor
   byte assignments unchanged. Do not replace `tam.function` with catalog state,
   a stored Forth-region flag, or a simulation.

**Tests that encode the old contract.** none — this implements the existing
bare-render contract and changes no contract. The R3 review's permitted file
set excluded `test_dict_reloc.c`, so do not invent or edit a test fixture in
this task.

**Facts the harness forces on you.** `STD_LEFT_SINGLE_QUOTE` and `STD_CURSOR`
are two-byte display glyphs. The ordinary literal path therefore deliberately
uses the existing +2 prefix; only Forth source is bare. `tam.function` is
captured before `pemCloseAlphaInput()` and is `ITM_FORTH` throughout a live
Forth ALPHA capture.

**Gate:** build-test green.
*Mutation: UNVERIFIED — the review was forbidden to edit firmware or read the
self-test file, so no honest RED mutation was available. The exact source-level
failure above was identified, but Task R3-1 must report this limitation rather
than claim mutation coverage.*

**Report:** Paste back the two edited snippets, both required green sentinel
lines, the script exit code, the arena high-water line printed by the gate, and
any surprise. Do not commit in this task.

---

## R3-2 — Replace stale executable-line citations, then commit the series

**File(s):** `packages/forth-core/programming/manage.c`,
`packages/forth-core/keyboard.c`

**Read:** In `manage.c`, `grep -a -n "Capture-abort reset"` and read 15 lines;
then `grep -a -n "Capture-close reset"` and read both matching blocks, no more
than 15 lines around each. In `keyboard.c`, `grep -a -n "real CAT->FORTH chain"`
and read 15 lines, then `grep -a -n "runFunction(item);"` and inspect only the
match whose following statements include `_closeCatalog();` in the
`calcMode == CM_PEM && catalog` arm (no more than 20 lines). Read no other source.

**The defect.** These comments present drifting line numbers as evidence. In
the reviewed tree, `manage.c:1441` is `pemCloseNumberInput()`, `manage.c:1463`
is a `MNU_PROGS` comparison, and `keyboard.c:1213-1216` is the TAM-label arm;
none is the code claimed. The real anchors are the `func == ITM_AIM` and
`func == ITM_FORTH` capture-open arms in `insertStepInProgram()`, and the
`runFunction(item);` / `_closeCatalog();` pair in the PEM catalog arm. A future
implementer following the current citations will inspect unrelated code and can
repeat the unreachable-chain test defect.

**The change.** Change comments only; do not change executable code.

1. In `manage.c`, replace `paths (E1/E2, manage.c:1441/1463)` with
   ``paths (the `func == ITM_AIM` and `func == ITM_FORTH` arms in
   `insertStepInProgram`)``. Reflow only those comment lines.
2. Replace both `manage.c:889-895` back-references in the two
   `Capture-close reset` comments with the stable phrase
   ``the `ITM_BACKSPACE` empty-buffer arm above``. Do not retain a line number.
3. In `keyboard.c`, replace `(see :1213-1216)` with the stable phrase
   `(see the PEM catalog arm's runFunction(item) / _closeCatalog() pair below)`.
   Reflow only that comment.

**Tests that encode the old contract.** none — comments only; executable
behavior and contracts are unchanged.

**Gate:** build-test green.
*Mutation: UNVERIFIED — this is a documentation-only task with no executable
predicate that can honestly turn the suite RED. Verification is the required
anchor grep plus the green build/test gate.*

After the gate, report the arena high-water line even though this series does
not touch the dictionary. Confirm with `git diff --check` and
`git status --short`. Stage only these working files and their generated patch
counterparts (never `git add -A`):

```text
packages/forth-core/programming/decode.c
packages/forth-core/programming/manage.c
packages/forth-core/keyboard.c
packages/forth-core/patches/010-programming__decode.c.patch
packages/forth-core/patches/010-programming__manage.c.patch
packages/forth-core/patches/010-keyboard.c.patch
```

If any listed generated path does not exist after the gate, or any additional
path would need staging, STOP and report rather than guessing. Commit once with
the exact message `forth-core: fix bare Forth PEM rendering`. Do not push.

**Report:** Paste back the exact comment replacements, both green sentinel
lines, exit code, arena high-water line, `git diff --check` result, the staged
path list, commit hash, and anything surprising.

---
