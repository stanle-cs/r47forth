# T5-2 — reader-side step accessors; sim-bench hand-walks converted (Part A)

Origin: T5 owner ruling (TESTING.md §5 T5 item 2; DESIGN-HISTORY
2026-08-03). Layout facts must be READ through one accessor, never
re-derived per test. This packet adds three accessors to the main test
file and converts the sim-bench subcases' hand-indexed byte checks to
them. Authored per QWEN_RUNBOOK §4a: Part A only — the architect runs
the mutation blocks separately after this lands.

## EXECUTION GATE (verify before any edit; STOP on any mismatch)

1. `git branch --show-current` is `forth-core/stack-semantics-d1-d2`;
   `git status --short` clean.
2. `grep -c "tpSrcPayload" packages/forth-core/test_dict_reloc.c` → at
   least 1 (the existing helper this packet builds on).
3. `grep -c "sSource\[0\] != 0x8B" packages/forth-core/test_capture.part.h`
   → at least 1 (the hand-index sites this packet removes).
4. `grep -n "T5 split: forward declarations" packages/forth-core/test_dict_reloc.c`
   → exactly ONE match (the accessors go immediately BEFORE this block).

## LAYOUT FACTS (stated per §4a rule 1 — do not re-derive)

- A source/marker step's signature is the three bytes `0x8B 0x1A 0xFD`
  followed by one LENGTH byte at offset 3.
- LENGTH 0 = the region marker. LENGTH > 0 = a source line whose payload
  is exactly LENGTH bytes at offset 4, NO terminator: `"3 4 +"` has
  length 5 and five payload bytes.

## Task — three accessors, then convert the bench sites

**Step 1.** Immediately BEFORE the `/* T5 split: forward declarations`
block in `packages/forth-core/test_dict_reloc.c`, insert exactly:

```c
/* T5-2 reader-side step accessors (PROGRAM-FIXTURE RULE, inspection
 * clause): tests never hand-index step bytes — layout facts live here
 * and in tpSrcPayload only. */
static bool_t stepIsForthStep(const uint8_t *step) {
  return step && step[0] == 0x8B && step[1] == 0x1A && step[2] == 0xFD;
}

static bool_t stepIsMarker(const uint8_t *step) {
  return stepIsForthStep(step) && step[3] == 0;
}

static bool_t stepSrcTextEq(const uint8_t *step, const char *expected) {
  uint8_t len = (uint8_t)strlen(expected);
  return stepIsForthStep(step) && step[3] == len
      && memcmp(step + 4, expected, len) == 0;
}
```

**Step 2.** In `packages/forth-core/test_capture.part.h`, convert every
hand-indexed step check to the accessors. Find each site by its unique
FAIL text (anchor rule §4a-5), read only that slice, and replace ONLY the
condition — never the FAIL printf or surrounding logic:

- SB-A2 (`FAIL: committed text wrong` in the reopen subcase): the
  `sSource[0] != 0x8B || ...` signature check becomes
  `!stepIsForthStep(sSource)`; the length+memcmp comparison against
  `"3 42 +"` becomes `!stepSrcTextEq(sSource, "3 42 +")`.
- SB-A5 (both its `committed source step not found` and
  `committed text wrong`/`half-line text wrong` checks): same two
  conversions, expected strings `"3 4 +"` and `"78"` as they are today.
- SB-F1 (`opening marker gone after second EXIT`): the
  `mkr[0] != 0x8B || mkr[1] != 0x1A || mkr[2] != 0xFD || mkr[3] != 0x00`
  check becomes `!stepIsMarker(mkr)`.
- SB-F2 (any `mkr`/signature hand-index checks in its two sub-parts):
  same conversions — marker checks to `stepIsMarker`, text checks to
  `stepSrcTextEq` with the strings already present.

Do NOT convert `test_params.part.h` or the main file's legacy tests in
this packet — that burn-down is queued separately.

**Step 3.** Run the gate:
`./packages/forth-core/build-test.sh > /tmp/forth-t5-2-gate.log 2>&1; echo "gate exit: $?"`
(log name per THIS packet, overriding any remembered form). Success =
exit 0 + `FORTH SELF-TEST: ALL PASSED` + `==> BUILD + SELF-TEST GREEN.`
and all 13 `SB-` PASS lines unchanged:
`grep '    SB-' /tmp/forth-t5-2-gate.log`

STOP after printing those lines. Do not commit; do not run mutations —
the architect directs those separately. A red anywhere outside the
converted conditions is an immediate STOP with a
`[SOL DEBUGGER HANDOFF]` report.
