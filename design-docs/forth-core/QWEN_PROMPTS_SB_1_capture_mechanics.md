# SB-1 — sim bench, capture mechanics and cancel edges (F6 charter rows A2-A6, F1, F2)

Origin: owner ruling 2026-08-02 converting the F6 stage-exit hardware
bench to an automated sim bench. Derivation and row dispositions:
`F6_KEYBOARD_PEM_AUDIT.md` §6. This packet authors the Block A + Block F
subcases as key-driven self-tests; its sibling `QWEN_PROMPTS_SB_2_
nesting_param_menus.md` covers Blocks B/C/D. Rows marked COVERED in §6
are NOT re-implemented here — their gate-greps below machine-check the
coverage claim instead.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` empty.
2. Suite call anchor: `grep -n "fail |= test_capture_acceptance();" packages/forth-core/test_dict_reloc.c`
   → exactly ONE match (new bench calls are inserted after it).
3. COVERED-claim greps (all must match, one each):
   `grep -c "static int test_capture_acceptance(void)$" packages/forth-core/test_dict_reloc.c` → 1.
4. Drive idiom anchors: `grep -n "extern void pemAlpha(int16_t);" packages/forth-core/test_dict_reloc.c`
   matches at least once (inside `test_capture_acceptance`);
   `grep -n "tpInit\|tpWrite\|tpStepAddr" packages/forth-core/test_dict_reloc.c | head -3` matches.
5. EXIT-ladder mechanism: `grep -n "isAlphaSubmenu" packages/forth-core/softmenus.c`
   matches; read the function body and confirm it contains a
   `-MNU_FORTH` disjunct (DESIGN.md §8.4 E8, VERIFIED tag).
6. Capture cap: `grep -n "196" packages/forth-core/programming/manage.c | head -3`
   matches (the glyph cap enforced at insertion sites).
7. Two-byte glyph shuffle: `grep -n "stringLastGlyph" packages/forth-core/programming/manage.c | head -1`
   matches.
8. Pre-gate green; arena baseline from the current HEAD commit message.

## PREAMBLE (paste before the task)

You are implementing one fully specified task in the C47 firmware repo at
`/home/stan/c43`. You are an implementer, not a designer: follow this
packet exactly; if a quoted anchor, function, test, branch, literal, or
identifier does not match the tree, STOP and report the mismatch.

Rules (binding; unchanged from the F-series):

1. Clean tree on the branch named in gate item 1 before any edit.
2. Keep a live todo at `/tmp/forth-sb-1-todo.md`: one item per subcase,
   mutation, final gate, report; append `MUTATION APPLIED/RESTORED: <n>`
   immediately.
3. The only build/test command is `./packages/forth-core/build-test.sh`,
   captured: `./packages/forth-core/build-test.sh > /tmp/forth-sb-1-gate.log 2>&1; echo "gate exit: $?"`.
   Success = exit 0 + `FORTH SELF-TEST: ALL PASSED` + `==> BUILD +
   SELF-TEST GREEN.` Inspect only bounded slices (`tail -n 12`, targeted
   greps). NOTE: the gate now also runs upstream's testSuite (T1); a red
   there is a STOP, not something to repair.
4. Edit ONLY `packages/forth-core/test_dict_reloc.c` (subcases) and, for
   mutations only, the files each mutation names — restore them by
   reversing the mutation hunk by hand. Never edit `src/`, `patches/`,
   `files/`.
5. Never read DESIGN.md/DESIGN-HISTORY.md; never read test_dict_reloc.c
   in full — grep the named anchors, read only local slices.
6. Do not change any test this packet does not name; a red outside your
   diff is an immediate STOP (`[SOL DEBUGGER HANDOFF]`, zero repairs).
7. No `git stash/reset/checkout --/restore`; no `git add -A`.
8. Report every required PASS/RED line, both banners, the arena line,
   `git diff --check`, and surprises.
9. After any compaction: re-read this packet, the todo file,
   `git status --short`, `git diff`; never reconstruct from memory.
10. Two-attempt debugger handoff as in the F-series packets.

**PROGRAM-FIXTURE AUTHORING RULE (mandatory).** `test_dict_reloc.c`
program fixtures are structural, not hand-addressed. Build programs with
`testProg_t` and the `tp*` helpers; capture step handles and resolve with
`tpStepAddr`; abort the subcase if fixture construction, `tpWrite`, or
address lookup fails. Never `beginOfProgramMemory + <literal>`, never a
numeric `tpStepAddr` argument, never payload-length arithmetic.

## Task — one new battery function, seven subcases

Add `static int test_sim_bench_capture(void)` to
`packages/forth-core/test_dict_reloc.c`, declared next to the other
`test_capture_*` forward declarations and called immediately AFTER the
`fail |= test_capture_acceptance();` line as
`fail |= test_sim_bench_capture();` with the same `[DEBUG] running ...`
print convention as its neighbors.

Prologue/epilogue: mirror `test_capture_acceptance`'s save/restore set
exactly (grep its opening `savedCurrentStep` slice and copy the pattern —
targeted read, ~45 lines). Each subcase prints one `PASS:`/`FAIL:` line
naming its charter row.

Subcases, each driving the KEY layer (`pemAlpha`, `fnKeyExit`,
`runFunction`) against a fresh `testProg_t` fixture with a `»FORTH`
region open unless stated:

- **SB-A2 reopen + mid-line edit.** Commit a line `3 4 +` (as
  test_capture_acceptance does), close, reopen it with the edit gesture.
  Reopen places the cursor at line end (T5 trace); assert that first —
  if it does not hold, STOP and report (bench divergence). Then move the
  cursor left twice, insert glyph `2`, ENTER. Locate the source step via
  its role handle and assert its payload text is exactly `"3 42 +"`
  (the literal that results from inserting `2` two glyphs before the
  end of `"3 4 +"`).
- **SB-A3 two-byte glyph backspace.** In an open line type the two-byte
  glyph `×` (ITM_CROSS through the alpha path), then one backspace.
  Assert the line's glyph count returned to its pre-glyph value and the
  byte length dropped by exactly 2 (whole-glyph removal, T1 arm 4).
- **SB-A4 the 196-glyph cap.** Drive a repeated key to exactly 196
  glyphs; assert press 197 leaves length unchanged (silent ignore,
  manage.c cap); ENTER; assert the committed step carries all 196 glyphs
  intact. (This subcase also re-tests the cap premise recorded in
  DESIGN_AUDIT Part 3's rotation list.)
- **SB-A5 save/restore with a half-typed line.** Type a line, do NOT
  commit; run the save/restore round trip the way
  `test_accept_entry_state_roundtrip` does (grep it, read its round-trip
  slice, reuse the same calls). Assert: the committed-per-key text
  survives in the step (T1 arm 7), and after restore capture is CLOSED
  with the cursor on the region (the documented cursor/open-flag loss —
  that loss IS the contract, assert it explicitly).
- **SB-A6 EXIT with a half-typed line.** Type a half line, press EXIT
  once (drops the alpha keypad per the E8 middle row), reopen the line.
  Assert the text present equals what was typed (committed per key) and
  no marker was written by the EXIT.
- **SB-F1 the EXIT ladder.** With the FWRD picker current above an open
  capture: EXIT pops to the ALPHA menu (assert via the softmenu stack
  top, as `test_alpha_menu_on_top_during_capture` reads it — grep for
  its stack-top idiom); EXIT again drops the alpha keypad, region open,
  cursor unmoved, no marker; EXIT again leaves PEM (`calcMode !=
  CM_PEM`). Three presses, three asserted states, in one subcase.
- **SB-F2 empty-line backspace.** Backspace on an EMPTY open capture
  line: (a) in a region with no committed lines — assert the
  marker-delete rule fires (region gone, both markers removed);
  (b) after one committed line — assert the committed line is NOT
  destroyed and only the empty tail closes per the landed rule. Derive
  the exact landed behavior for (b) from T1 arm 4's empty-buffer branch
  (grep `pemAlpha` arm 4 in `packages/forth-core/programming/manage.c`,
  read that slice only) — assert what the code does today; if what it
  does contradicts the §8.4 marker rule, STOP and report (that is a
  bench divergence, the thing this bench exists to catch).

## Gate and required mutations

Full gate first: seven new PASS lines + every existing banner. Then,
separately, logs `/tmp/forth-sb-1-mut{1,2,3}.log`, restore + green gate
between:

1. **Cap mutation.** In `packages/forth-core/programming/manage.c`,
   disable the 196-glyph cap check at the insertion site (make the
   comparison unreachable). SB-A4 MUST go RED (197th glyph appends).
   Restore.
2. **Ladder mutation.** In `packages/forth-core/softmenus.c`, remove the
   `-MNU_FORTH` disjunct from `isAlphaSubmenu`. SB-F1 MUST go RED (first
   EXIT falls through and destroys the capture — either the stack-top or
   the region assert fires). Restore.
3. **Glyph mutation.** In `packages/forth-core/programming/manage.c`,
   change the backspace path's two-byte-glyph handling to remove one
   byte instead of the whole glyph. SB-A3 MUST go RED. Restore.

Report: seven PASS lines, three mutation REDs, both banners, exit 0,
arena line vs baseline, `git diff --check` clean.

## Commit

```text
forth-core: SB-1 — sim bench, capture mechanics + cancel edges (charter A2-A6, F1, F2)
```
