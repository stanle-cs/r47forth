# Stage F2 — shared RPN parameter semantic core: packet ledger + trace record

> Operator sequencing lives in `QWEN_RUNBOOK.md`; the series-wide plan in
> `FSERIES_ROADMAP.md`. This file is the stage ledger AND the §10.2 trace
> record. Every task lives in its own self-contained packet file.

Origin: DESIGN §10.2. Authored 2026-07-17 per the owner's pacing instruction
(author the whole series ahead, gate-locked). The trace below was performed
against the post-F15-3 tree (`c8b87dfa8` + authored F15-4/F15-5); F1.5's
remaining packets touch only `forth_prims.c`, `ui/tam.c` (mutation-only) and
the test file — none of F2's anchors. Every F2 packet still opens with a
hard EXECUTION GATE that fails closed on any drift.

## The trace (§10.2 "traced, not inferred") — 2026-07-17, file:line evidence

- **Native path:** `executeOneStep` (programming/lblGtoXeq.c) special-cases
  a few items, then its default switch maps the item's PTP class to
  `_executeOp(step, op, PARAM_x)` — including a fully general default arm
  `_executeOp(step, op, (indexOfItems[op].status & PTP_STATUS) >> 9)` and
  `PTP_KEYG_KEYX → PARAM_NUMBER_8`. `PTP_NONE → runFunction(op)` directly;
  `PTP_LITERAL → _putLiteral(step)`; `PTP_REM` is handled inline (including
  the §8.2 ITM_FORTH arm); `PTP_DISABLED` errors.
- **`_executeOp(paramAddress, op, paramMode)`** (lblGtoXeq.c:360, static)
  reads `opParam = *paramAddress++` then per PARAM_* arm: direct-range
  checks against `indexOfItems[op].tamMinMax & TAM_MAX_MASK` (0x3fff),
  indirect markers (`INDIRECT_REGISTER`/`INDIRECT_VARIABLE` →
  `_executeWithIndirectRegister/Variable`, lblGtoXeq.c:326/340, static),
  string names (`STRING_LABEL_VARIABLE`/`LOCAL_LABEL_VARIABLE` →
  `getStringLabelOrVariableName` — UNBOUNDED, decode.c:121), the
  package-integrated Forth XEQ fallback (PARAM_LABEL arm: label-kind-aware
  search, then `forthResolveXEQ` → `reallyRunFunction(ITM_FCALL, idx)` /
  item / `ERROR_LABEL_NOT_FOUND`), `PARAM_NUMBER_16` split by
  `isFunctionOldParam16` (little-endian, no indirection) vs new form, and
  `PARAM_NUMBER_8_16`'s `CNST_BEYOND_250` extension byte. Every arm ends at
  `reallyRunFunction(op, value)` or an error surface. NOTE (traced fact):
  the direct out-of-range arms only `sprintf(tmpString, …)` — they set NO
  error code; parity means preserving exactly that.
- **`_executeOp`'s dependency closure** is extraction-clean: the three
  statics call only global APIs (`reallyRunFunction`, `findNamedLabel`,
  `findNamedVariable`, `findOrAllocateNamedVariable`,
  `getStringLabelOrVariableName`, register/longInteger helpers,
  `forthResolveXEQ`, flags) plus `_putLiteral` (stays behind, see F2-1).
- **Forth runtime:** forth_inner.c:349 `FTOK_C47` arm is a PARALLEL private
  decoder: `boundedRead` + itemId `< LAST_ITEM` check, then its own PTP
  switch (NONE; NUMBER_8 = value byte padded to a 2-byte cell; NUMBER_16 =
  little-endian cell), `default → ERROR_OPERATION_UNDEFINED`, then its own
  dispatch. It performs NO tamMinMax range check — a drift F2-3 closes by
  re-routing it through the shared core.
- **Forth compile:** forth_compile.c §4.1 step 4 emits `FTOK_C47 + itemId`
  for CAT_FNCT + PTP_NONE items only (`forthFindItem` gate); parameterized
  textual forms are F4's scope. The NUMBER_8/16 runtime shapes are pinned
  today by hand-assembled tests (e.g. ITM_FCALL PTP_NUMBER_16,
  test_dict_reloc.c:539-557) and by the F1-5 validator's FTOK_C47 arm.
- **Encodings differ by design and stay:** native steps carry marker-byte
  grammar (opParam, indirect markers, extension bytes); Forth carries
  2-byte cells. The SHARED layer is therefore the semantic tail — direct
  range validation + dispatch + error surface, "ending at
  `reallyRunFunction()`" — not the byte grammar. Changing Forth's operand
  encoding would break the F1-5 validator and the save format for nothing.

## Decided architecture (architect, from the trace — no open choices)

New package-new file pair `programming/param_core.h` / `param_core.c`
(auto-included by the package system; no build registration exists or is
needed):

1. **F2-1** moves `_executeOp` (→ public `paramCoreExecuteOp`) and the two
   `_executeWith*` helpers (stay static) into `param_core.c`, byte-identical
   behavior. `_putLiteral` stays in lblGtoXeq.c; `param_core.c` reaches it
   through a one-line public wrapper `paramCorePutLiteral` implemented in
   lblGtoXeq.c and declared in `param_core.h`.
2. **F2-2** adds a BOUNDED string-name reader (static inside `param_core.c`,
   contract copied from decode.c's reader, explicit exclusive end bound =
   `firstFreeProgramByte` on program-memory paths) and switches every name
   path inside `param_core.c` onto it. The unbounded decode.c reader remains
   for display paths (out of F2 scope; §10.2's reader requirement is about
   the execution core).
3. **F2-3** factors the direct-parameter semantic tail into
   `paramCoreValidateDirect(op, ptpClass, value)` +
   `paramCoreDispatchDirect(op, ptpClass, value)` used by BOTH
   `paramCoreExecuteOp`'s direct branches and forth_inner's `FTOK_C47` arm
   (which keeps its byte decode + `boundedRead`, drops its private checks
   and dispatch). Forth gains the native range semantics it lacked.
4. **F2-4** is the parity acceptance sweep: same item + same parameter
   driven through a native step and through a hand-assembled Forth call
   must produce identical X and identical `lastErrorCode` (including the
   traced silent out-of-range behavior), plus corrupted-itemId and arena
   reporting. Flash delta recorded per RULE-1.

## Status and dependency order

| Task | Packet | Status | Dependency |
|---|---|---|---|
| F2-1 extract the native core | `QWEN_PROMPTS_F2_1_extraction.md` | DONE (`6f0ffca4b`) | F1.5 complete (F15-5 committed green) |
| F2-2 bounded name reader | `QWEN_PROMPTS_F2_2_bounded_names.md` | DONE (`69e594c71`) | F2-1 committed green (`6f0ffca4b`) |
| F2-3 shared direct dispatch + FTOK_C47 re-route | `QWEN_PROMPTS_F2_3_shared_dispatch.md` | DONE (`06ce84b5a`) | F2-2 committed green (`69e594c71`) |
| F2-4 parity acceptance sweep | `QWEN_PROMPTS_F2_4_parity_acceptance.md` | LANDED (`176e0be0f`) — REVIEW FOUND ESCAPES | F2-3 committed green (`06ce84b5a`) |
| F2-5 acceptance correction | `QWEN_PROMPTS_F2_5_acceptance_correction.md` | **READY TO EXECUTE** | F2-4 landed; review correction required before F3 |

F2-1 landed after three authoring corrections: its full-engine XEQ fixture
repeated forever because legacy `executeOneStep(ITM_XEQ)` returns `-1`
after the synchronous Forth fallback leaves `currentStep` unchanged; the
acceptance now drives the real source and XEQ decoders exactly once. The
call-site inventory is four, and the PARAM_REGISTER mutation anchor is
`reallyRunFunction(op, regKStoC(opParam))`.

F2-2's execution gate was re-verified against `6f0ffca4b`: four unbounded
name reads remain (indirect-variable helper, PARAM_LABEL,
PARAM_REGISTER/PARAM_COMPARE, PARAM_MENU). Its two XEQ fixtures use the
same corrected one-step drive. The malformed-name differential is pinned
by `writeTestProgram`'s `0xFF 0xFF` sentinel, not an ASan assumption.

F2-4's final gate was green, but its required NUMBER_8 boundary mutation
also stayed green: both native and Forth paths compared through the same
mutated validator. Post-stage review additionally found read-before-bound
and available-count narrowing in the F2-2 reader, last-match NUMBER_16
discovery, and non-isolated NUMBER_16 input state. F2-5 is the bounded
correction packet; F2 and the F3 dependency remain open until it lands green.

Authoring rules as in `QWEN_PROMPTS_F15_harness.md` (landed-tree
verification, machine-verified literals, per-packet `/tmp/forth-f2-N-*`
paths, verified mutations). Execute strictly in order, one packet per
session, clean green tree each. After each stage commit the operator runs
the successor's gate; any mismatch returns to the architect.
