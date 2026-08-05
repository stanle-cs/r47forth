# PACKET L1-0 — re-target the battery's one-shot entry

**Stage L packet 0 of N** (design: STAGE_L_INTERACTIVE.md, owner-ruled
2026-08-04; evidence: STAGE_L_TRACES.md §T2). Prerequisite for every other
Stage L packet.

**Why this exists.** Today `fnForthOuter` (the `ITM_FORTH` item entry) is a
one-shot: it requires a string in X, interprets it, and consumes it
(forth_compile.c:1604-1620). L-R2 rules that FORTH *always opens an
interactive capture* instead, so from L1-1 onward the item entry stops
interpreting. **51 self-test call sites drive `fnForthOuter(NOPARAM)`
expecting the one-shot** — 44 in test_params.part.h, 6 in
test_engine.part.h, 1 in test_persist.part.h. Left alone they would go
silently vacuous under L1-1: no error, no interpretation, assertions
failing against stale state. The battery would then be red for reasons
unrelated to the code under test, and the gate could not distinguish an
L1 regression from L-R2's intended behaviour change.

This packet moves those sites to a self-test entry carrying today's exact
semantics. It is a **pure refactor**: no production behaviour changes, and
the battery's pass/fail set must be identical before and after.

## Implementer contract (Claude-subagent edition)

- You are in an isolated git worktree of the repo. Work ONLY through the
  package working area `packages/forth-core/` — never edit `patches/` or
  `files/` (generated), never edit `src/c47/` (upstream).
- The gate is `./packages/forth-core/build-test.sh` (it refreshes the
  generated outputs first). Run it with output redirected to a log file and
  inspect via bounded greps (`grep -a` — the log contains control bytes).
  A run ends green iff the log shows `FORTH SELF-TEST: ALL PASSED` and
  `BUILD + SELF-TEST GREEN`.
- **STOP conditions (report back, do not adapt):** a red test this packet
  did not write; any anchor in the EXECUTION GATE not matching; any spec
  statement here that contradicts what you find in the tree. On STOP,
  return a report naming the mismatch with file:line evidence.
- Fixture rules (binding, from the F-series ledger): clear `lastErrorCode`
  per subcase; `dynamicMenuItem = -1` and `programRunStop = PGM_STOPPED`
  before `fnGotoDot`; never prime the state under test (drive the real
  entry point); `compareString` returns 0 on equal.
- Every mutation below must be applied, shown RED at the named assertion,
  and reverted (`git diff` clean of mutation residue afterward). Record
  each RED line verbatim in your report.
- Report the `FORTH ARENA` lines from your final green gate log (§5.4
  discipline).

## EXECUTION GATE (verify before any edit; STOP on mismatch)

```
grep -n "define FORTH_SOURCE_MAX" packages/forth-core/forth_compile.c      # expect: 256, file-local
grep -n "void fnForthOuter(uint16_t unused)" packages/forth-core/forth_compile.c
grep -n "static void forthOuterRun" packages/forth-core/forth_compile.c    # file-static — helper MUST live in this file
grep -c "fnForthOuter(NOPARAM)" packages/forth-core/test_params.part.h     # expect 44
grep -c "fnForthOuter(NOPARAM)" packages/forth-core/test_engine.part.h     # expect 6
grep -c "fnForthOuter(NOPARAM)" packages/forth-core/test_persist.part.h    # expect 1
grep -c "forthTestRunFromX" packages/forth-core/forth_compile.c            # expect 0 (not yet implemented)
```

If any count differs, STOP and report the actual number — the count is the
packet's completeness contract, not a convenience.

## C1 — `forth_compile.c`: the self-test entry

`forthOuterRun` is file-static in forth_compile.c, so the helper **must**
live in that file; it cannot be written in the test harness.

Immediately AFTER the closing brace of `fnForthOuter`, add:

```c
#if defined(FORTH_DEBUG_SELFTEST)
/* L1-0: the battery's "interpret the string in X" entry.
 *
 * Until Stage L this WAS fnForthOuter: ITM_FORTH outside PEM required a
 * string in X, interpreted it, and consumed it.  L-R2 rules that FORTH
 * always opens an interactive capture instead, so the item entry stops
 * interpreting and the sites that drove it for its interpret semantics
 * need those semantics under their own name.
 *
 * The body is fnForthOuter's, VERBATIM as of 2026-08-04 — same two error
 * codes, same copy-before-drop ordering (drop invalidates the string),
 * same FORTH_OUTER_FULL run — so every existing assertion keeps its exact
 * stack expectation.  Do not "improve" it: forthOuterInterpret() is NOT a
 * substitute (it never touches X, so the drop that these tests' stack
 * expectations are written against would not happen, and it clears
 * lastErrorCode on entry where this does not).
 *
 * Self-test builds only; production never calls it. */
void forthTestRunFromX(void) {
  if (getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  int32_t len = stringByteLength(REGISTER_STRING_DATA(REGISTER_X));
  if (len + 1 > FORTH_SOURCE_MAX) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  forthOuterCtx_t ctx;
  ctx.savedScope = forthCurrentScope;
  xcopy(ctx.source, REGISTER_STRING_DATA(REGISTER_X), len + 1);
  fnDrop(NOPARAM);   /* copy MUST precede drop: drop invalidates the string */
  forthOuterRun(&ctx, FORTH_OUTER_FULL);
}
#endif
```

**`fnForthOuter` itself is NOT touched by this packet.** It keeps its
current body; L1-1 replaces it. That is deliberate — see C4.

## C2 — `forth_dict.h`: the declaration

Next to the existing `void fnForthOuter(uint16_t param);` (forth_dict.h:262),
add:

```c
#if defined(FORTH_DEBUG_SELFTEST)
void forthTestRunFromX(void);   /* L1-0: battery entry for the one-shot
                                   interpret-the-string-in-X semantics that
                                   fnForthOuter carried before Stage L */
#endif
```

## C3 — the 51 call sites

Substitute, in these three files only:

```
fnForthOuter(NOPARAM)   ->   forthTestRunFromX()
```

- `packages/forth-core/test_params.part.h` — 44 sites
- `packages/forth-core/test_engine.part.h` — 6 sites
- `packages/forth-core/test_persist.part.h` — 1 site

This is a mechanical substitution; the surrounding `x_set_string(...)`
setup and every assertion stay exactly as they are. Do not reflow, retab,
or otherwise touch adjacent lines — the diff must be one changed token per
site so review can read it.

Then update the two comments that describe the pin, because their subject
has moved:

**`packages/forth-core/test_dict_reloc.c:243-245`**, the sub-phase C banner
comment. Replace `Exercise fnForthOuter (the REAL entry point), not
forthOuterInterpret.` with:

```
 * Exercise forthTestRunFromX (the one-shot interpret-from-X core), not
 * forthOuterInterpret: these cases are written against a source line that
 * ARRIVES IN X and is consumed, so their stack expectations depend on the
 * drop.  Before Stage L this core was fnForthOuter itself; L-R2 made that
 * item entry a capture opener (see PACKET_L1_0).
```

**`packages/forth-core/test_persist.part.h:1295-1297`**, the D3-5 comment.
Replace it with:

```
/* D3-5: pin the interpret-from-X core — it must bracket depth/spill
 * exactly like forthOuterInterpret.  Found by the T6 upstream-runner cases
 * (spill dead on the keyboard path).  This pinned fnForthOuter until
 * Stage L; L-R2 made that entry a capture opener, so the pin moved to
 * forthTestRunFromX, which carries the old body verbatim.  L1-1 adds the
 * separate pin for what fnForthOuter does NOW (opens a capture, seeds
 * from a string X, consumes it at seed). */
```

Rename that test function `test_fnforthouter_brackets` →
`test_forth_run_from_x_brackets`, updating its registration site and its
**four** `printf` strings (test_persist.part.h:1308, :1313, :1319, :1324 —
`fnForthOuter` → `forthTestRunFromX` in the message text) so a red line
names the function that actually failed.

**Comment sweep (rev 2).** Beyond the two comments named above, the three
battery files carry prose that names `fnForthOuter` while describing the
call being retargeted. **One rule, applied everywhere in
`test_params.part.h`, `test_engine.part.h` and `test_persist.part.h`:
a comment describing the call under test says `forthTestRunFromX`.**
The seven known sites in test_params.part.h are :166, :184, :357, :375,
:393 ("via fnForthOuter" → "via forthTestRunFromX"), :2924 ("source string
and fnForthOuter drops it" → "…and forthTestRunFromX drops it") and :3135
("Note forthOuterInterpret (not fnForthOuter)" → "(not
forthTestRunFromX)"). Sweep the other two files the same way and report
the total count you changed — the list above is what one grep found, not a
guarantee of completeness.

Comments that describe *history* rather than the call — e.g. the two
rewritten above, which explain why the pin moved — keep the name
`fnForthOuter` deliberately, because they are about that function.

## C4 — the completeness proof (this is the acceptance criterion)

Because `fnForthOuter` is unchanged in this packet, a **missed call site is
invisible to the gate** — the old entry still interprets, so the test still
passes. The count in the EXECUTION GATE is therefore not sufficient on its
own. Prove completeness directly:

1. Temporarily replace `fnForthOuter`'s body with `{ (void)unused; return; }`.
2. Run the full gate.
3. **It must be GREEN.** A green run proves no self-test depends on
   `fnForthOuter` any more, which is exactly the property L1-1 needs.
   A red run names the sites you missed — fix them and repeat.
4. Revert the stub. Confirm `git diff` shows no residue.

Record in your report: the stubbed-gate result, and the list of any sites
step 3 caught.

## Acceptance

- **Zero call sites:**
  `grep -c "fnForthOuter(NOPARAM)" packages/forth-core/test_params.part.h
  packages/forth-core/test_engine.part.h packages/forth-core/test_persist.part.h`
  → 0, 0, 0. (The earlier form of this criterion counted *all* occurrences
  and was unsatisfiable — prose comments legitimately name the function.
  The call-site spelling is what matters.)
- **Every surviving `fnForthOuter` mention is deliberate:** list them with
  `grep -n "fnForthOuter"` across the three files and state, one line
  each, why it stays. Anything you cannot justify is a missed C3 sweep.
- Gate green.
- **The battery's result set is unchanged.** Capture the sorted list of
  `PASS:`/`FAIL:` lines from a gate log taken BEFORE your first edit and
  from your final green log; they must be identical except for the three
  renamed `printf` strings in C3. Include the diff of those two lists in
  your report — an empty diff (modulo the rename) is the packet's proof
  that it changed nothing.
- The C4 stubbed-gate run was green.

## Mutations (each must be shown RED, then reverted)

1. **Drop the drop.** Delete `fnDrop(NOPARAM);` from `forthTestRunFromX`.
   Expect RED in `test_forth_run_from_x_brackets` (X is the source string,
   not 66) and in the test_params sites that assert a result in X.
2. **Invert the ordering.** Move `fnDrop(NOPARAM);` above the `xcopy`.
   Expect RED broadly — the copy reads freed register data. (If this
   happens to pass on your machine, report it: it means the fixture is not
   exercising the freed-memory window and the ordering comment is
   unpinned. Do NOT treat a pass as acceptable.)
3. **Weaken the oversize guard.** Change `len + 1 > FORTH_SOURCE_MAX` to
   `len > FORTH_SOURCE_MAX`. Expect RED at whichever test drives a
   maximum-length line; if nothing goes red, report it — that is a coverage
   hole this packet should not paper over, and the fix is a new subcase
   driving a 256-byte line, not a silent pass.

## Out of scope

- Any change to `fnForthOuter`'s behaviour (that is L1-1).
- Any new interactive-capture test (L1-1 adds the pin for the new entry).
- Any change to `forthOuterInterpret` or its several hundred call sites.

## Flash / RAM

Zero production delta: `forthTestRunFromX` is inside
`#if defined(FORTH_DEBUG_SELFTEST)` and the dmcp5r47 build does not define
it. Report the measured `make dmcp5r47 CUSTOM_PKG=packages/forth-core` size
anyway (RULE-1) — it must be unchanged, and a non-zero delta means the
guard is wrong.
