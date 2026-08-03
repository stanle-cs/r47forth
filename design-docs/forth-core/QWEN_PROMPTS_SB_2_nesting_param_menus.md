# SB-2 — sim bench, alpha nesting, parameter entry, menus (F6 charter rows B1-B3, C2, D2)

Origin: owner ruling 2026-08-02 (see `F6_KEYBOARD_PEM_AUDIT.md` §6 for
the derivation and row dispositions). Sibling packet of
`QWEN_PROMPTS_SB_1_capture_mechanics.md`; run SB-1 first (its battery
function is this packet's insertion anchor). Rows marked COVERED in §6
are machine-checked by the gate-greps below, not re-implemented.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` empty; SB-1's commit is in `git log --oneline -3`.
2. Insertion anchor: `grep -n "fail |= test_sim_bench_capture();" packages/forth-core/test_dict_reloc.c`
   → exactly ONE match (this packet's battery call goes after it).
3. COVERED-claim greps (each → exactly 1):
   `grep -c "static int test_capture_menus(void)$" packages/forth-core/test_dict_reloc.c`
   `grep -c "static int test_pem_xeq_dynmenu_no_live_exec(void)$" packages/forth-core/test_dict_reloc.c`
   `grep -c "static int test_capture_param_text(void)$" packages/forth-core/test_dict_reloc.c`
   `grep -c "static int test_param_register_flag(void)$" packages/forth-core/test_dict_reloc.c`
   `grep -c "static int test_picker_insert_at_cursor(void)$" packages/forth-core/test_dict_reloc.c`
   `grep -c "static int test_word_catalog(void)$" packages/forth-core/test_dict_reloc.c`
4. TAM cancel reset: `grep -n "tam.colon = false" packages/forth-core/ui/tam.c`
   matches (the cancel-path reset this packet's mutation 1 targets;
   note every match line in the todo before editing anything).
5. Suspend/restore seam: `grep -n "forthCapSuspendState" packages/forth-core/forth_capture.c | head -2`
   matches.
6. Pre-gate green; arena baseline from the current HEAD commit message.

## PREAMBLE

Identical to SB-1's preamble with these substitutions: todo file
`/tmp/forth-sb-2-todo.md`, gate log `/tmp/forth-sb-2-gate.log`, mutation
logs `/tmp/forth-sb-2-mut{1,2,3}.log`. All ten rules, the two-attempt
handoff, and the PROGRAM-FIXTURE AUTHORING RULE apply verbatim.

## Task — one new battery function, five subcases

Add `static int test_sim_bench_nesting(void)` next to
`test_sim_bench_capture`, called immediately after it in the suite
runner. Prologue/epilogue: same saved-globals pattern (copy from
`test_capture_acceptance`'s opening slice). One `PASS:`/`FAIL:` line per
subcase, naming the charter row.

Subcases (fresh `testProg_t` fixture with an open capture region each,
unless stated):

- **SB-B1 TAM cancel keeps the line.** With `text` typed in an open
  capture line, open TAM via the XEQ path (`tamEnterMode` as
  `test_capture_suspend` drives it — grep it, read its drive slice),
  type two name glyphs WITHOUT completing, cancel back (EXIT). Assert:
  the capture line text is byte-identical to before TAM, capture is
  OPEN, and the cursor is where it was (record `T_cursorPos` before,
  compare after).
- **SB-B2 no tam.colon leak.** Same flow but press the TAM `:` key so
  `tam.colon` becomes true, then cancel. Assert `tam.colon == false`
  after the cancel AND the next capture keypress inserts a plain glyph
  (no colon artifact in the line).
- **SB-B3 committed XEQ from capture.** From an open capture line,
  complete a full `XEQ 'NAME'` TAM entry for a label that exists in the
  fixture (build the labelled step with `tpLbl` and use its role
  handle). Assert where the XEQ step lands relative to the open region
  per the LANDED behavior: derive it from the F6-2 suspend contract by
  running the flow and asserting the resulting step order via role
  handles — the XEQ step must be a well-formed native step, the capture
  line must survive intact, and the region must stay balanced. If the
  step lands INSIDE the region as a native step, that is the §8.4
  invariant (RPN steps mid-region are permitted) — assert that
  placement; if the program is corrupted or the capture line is lost,
  STOP and report (bench divergence).
- **SB-C2 local-register form and cancel.** During capture press STO
  then `.` `0` `5` (the local form): assert the canonical text landed in
  the capture line (same canonical-spelling source as
  `test_capture_param_text` — grep its expected-string idiom and use
  the same helper). Then press STO and cancel out (EXIT before any
  digit): assert the line is unchanged and no partial parameter text
  leaked.
- **SB-D2 picker navigation leaks nothing.** Open the FWRD picker over
  an open capture line containing `text`; press the softmenu row
  up/down navigation keys twice each (the keys
  `test_picker_rebuilds_same_menu` uses — grep its drive slice); EXIT
  back. Assert the capture line is byte-identical and the cursor
  unmoved.

## Gate and required mutations

Full gate first: five new PASS lines + all existing banners green.
Mutations, separately, restore + green between:

1. **Colon-leak mutation.** In `packages/forth-core/ui/tam.c`, disable
   the cancel-path `tam.colon = false` reset (the gate-item-4 line; if
   several matched, the one on the cancel/abort path — identify it from
   the surrounding `case`/branch, note the line in the todo). SB-B2
   MUST go RED. Restore.
2. **Cursor-restore mutation.** In `packages/forth-core/forth_capture.c`,
   make the resume path restore a cursor of 0 instead of the suspended
   value (the `forthCapSuspendState` counterpart read slice names the
   field). SB-B1 MUST go RED on its cursor assert. Restore.
3. **Canonical-text mutation.** Break the local-form emission so the
   leading `.`/arrow marker of the local register spelling is dropped
   (in the parameter-text sink `test_capture_param_text` pins — grep
   `paramCore`/capture text emission in
   `packages/forth-core/programming/param_core.c` or the capture sink
   it names; read that slice). SB-C2 MUST go RED. Restore.

Report: five PASS lines, three mutation REDs, both banners, exit 0,
arena line vs baseline, `git diff --check` clean.

## Commit

```text
forth-core: SB-2 — sim bench, nesting + parameter entry + menus (charter B1-B3, C2, D2)
```
