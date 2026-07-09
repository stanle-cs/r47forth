# DESIGN.md consolidation — verification checklist (2026-07-08)

How to use: each row states where the amendment item now lives in the
consolidated DESIGN.md and quotes the merged text so you can grep for it
verbatim. Line numbers refer to the consolidated file as written today.

## A. Amendment items C-1..C-13

**C-1 — compile-state C47-label calls deferred; emit scope**
Lives in: §3.3.6 (full rationale + both-state behavior), §3.3 intro ("Emit
scope (C-1)"), pseudocode label arm, §2.2 FTOK_C47 note, §3.2 FTOK_C47 arm,
§4.1 step 4, §7.3.
Quotes: §3.3.6 — "The earlier pseudocode line `if state==COMPILE:
emit(FTOK_C47); emit16(ITM_XEQ...)` is **stricken**. Two independent
reasons, both verified in the tree" (both reasons kept verbatim: PTP_LABEL
unsupported by the committed decoder → ERROR_OPERATION_UNDEFINED; stale
label IDs vs upstream's inline name strings). §3.3 intro — "the sub-phase C
compiler emits **only** `FTOK_PRIM`, `FTOK_CALL`, `FTOK_LIT`, `FTOK_ILIT`,
`FTOK_EXIT`. It never emits `FTOK_BR`/`FTOK_0BR` ... and never emits
`FTOK_C47`". Interpret-state protocol kept verbatim: "saved =
programRunStop; programRunStop = PGM_RUNNING; reallyRunFunction(ITM_XEQ,
label); if (programRunStop == PGM_RUNNING) programRunStop = saved;" with
"the GTK refresh-pump livelock applies to *any* `reallyRunFunction` call
from Forth context, not just FTOK_C47". Message intent kept: "cannot
compile a C47 label call (stage 2)".
Superseded base text removed: the `emit(FTOK_C47); emit16(ITM_XEQ...)` /
`execProgram(citem)` pseudocode lines are gone.

**C-2 — lookup order: number BEFORE label**
Lives in: §4.1 (reordered list: 1 prim, 2 colon, 3 number, 4 label) +
"Order rationale (C-2 ...)" paragraph; pseudocode comments "§4.1 step 3
(C-2)" / "step 4 (C-2: number BEFORE label)".
Quote: "a user program labeled `\"3\"` must never hijack the numeric literal
`3` inside Forth source. The converse loss (a digits-only program name is
uncallable from Forth by bare name) is trivial and has an escape hatch
(interpret-state `XEQ`-by-name still works from the keyboard)."
Superseded base text removed: old §4.1 order (label step 3 / number step 4)
and "standard Forth tries numbers last, which we match".

**C-3 — FORTH_PRIM_NONE; `idx >= 0` is a trap**
Lives in: §1.1 forth_dict.h block — "`#define FORTH_PRIM_NONE
((uint16_t)0xFFFFu)  // forthFindPrim miss sentinel (C-3, §3.3)`"; §3.3
pseudocode — "if idx != FORTH_PRIM_NONE: // C-3: NEVER 'idx >= 0' — idx is
uint16_t, that test is always true"; §4.1 step 1 — "every unknown word
would dispatch `forthPrims[0xFFFE].fn()` (C-3)".
Superseded base text removed: `if idx >= 0:` in the pseudocode.

**C-4 — STATE per-line; single-line definitions; `:`/`;` error cases**
Lives in: §3.3.1 (all six bullets verbatim: state local per invocation, no
PRGM-mode interaction, nested `:`, stray `;`, `:` with no name,
unterminated definition, prim-name collision "allowed silently ...
permanently shadowed"), plus all four error paths wired into the
pseudocode.
Quote: "End of line with `state == COMPILE` (unterminated definition) →
`abortDefinition()` then `ERROR_OPERATION_UNDEFINED`" and (pseudocode)
"without the abort, the smudged entry leaks: permanently invisible yet
holding a dictionary index + arena bytes".

**C-5 — source acquisition, private forthSource, forthOuterActive guard**
Lives in: §3.3.2 — complete `fnForthOuter` code block verbatim
(FORTH_SOURCE_MAX 256, "PRIVATE. Never tmpString, never aimBuffer, never
errorMessage", dtString type check, no-silent-truncation length check,
`fnDrop(NOPARAM)` before interpret, guard raising
ERROR_OPERATION_UNDEFINED, "delete that body; the real implementation must
leave `funcOK` true on success"), plus the why-mandatory paragraph
("executes arbitrary C47 code between tokens ... aimBuffer doubles as the
NIM buffer (c47.c:132)"). The `fromProgram` fix "forthInner(widx,
programRunStop == PGM_RUNNING)" is in both §3.3.2 and the pseudocode.
Superseded base text removed: §5.4 "tokenizer scratch (reuse tmpString)"
(now lists forthSource 256 B + token buffer 64 B); pseudocode
one-arg `forthInner(widx)`.

**C-6 — tokenizer, glyph-wise advance**
Lives in: §3.3.3 — `nextToken` code verbatim (FORTH_TOKEN_MAX 63, glyph
citations charString.c:379-395 / sort.c:70, second-byte-may-be-0x20
hazard), delimiter "exactly the single byte 0x20", names capped at 31
**bytes** by startDefinition, ">31 bytes legal only as number literals".
Absorbed the base trailing NOTE's unique sentence: "Never author names as
UTF-8; a UTF-8 lead byte (0xC0+) is misparsed as a C47 two-byte glyph high
byte."

**C-7 — interpret-state execution discipline**
Lives in: §3.3.4 — all three numbered rules verbatim (prim scrub
`clearSystemFlag(FLAG_ASLIFT)` citing forth_inner.c:170-171; lastErrorCode
gate with abortDefinition-if-open; line-end `setSystemFlag(FLAG_ASLIFT)`
with the PC-test rationale), plus "remove `static` from `forthPushInt32`
and `forthPushReal34` (forth_inner.c:17-43) ... Do not reimplement the lift
discipline in the compiler." All gates also appear inline in the
pseudocode.

**C-8 — number grammar (exact)**
Lives in: §3.3.5, second half ("Number grammar (C-8 ...)"), grammar block
verbatim. Preserved with exact nuance: two-byte-glyph disqualification;
NaN/Infinity gate ("`decQuadFromString` ... happily parses `\"NaN\"` and
`\"Infinity\"` — those must never reach it"); radix `'.'` only; e/E only;
base 10 only; leading-`'+'` skip with mpz_set_str citation; defensive
stringToLongInteger failure → non-number; **int32 range check then real34
fallback in BOTH states**: "Range check with `longIntegerCompareInt(li,
INT32_MAX) <= 0 && longIntegerCompareInt(li, INT32_MIN) >= 0` ... **never**
bare `longIntegerToInt32` (`mpz_get_si` truncates silently). In range:
`longIntegerToInt32(li, v)`; compile → `FTOK_ILIT` + 2 cells ... interpret
→ `forthPushInt32(v)`. Out of range: fall to the real34 path *in both
states* ... `longIntegerFree(li)` on every path." Real path: FTOK_LIT + 8
cells / forthPushReal34.
Superseded base text removed: `isNumber`/`isIntegerLiteral`/`fitsInt32`
pseudocode arm (the loop now delegates: "integer or real path per §3.3.5,
identical in both states").

**C-9 — dict-emit API mapped to committed code**
Lives in: §3.3.7 — full C code block verbatim (openDef struct,
forthDictEmit, forthDictEmitBytes, startDefinition with nameLen check and
the **count cap `fdict.count >= 0x6F00` → ERROR_RAM_FULL** ("index 0x6F00
would emit 0x7F00 == FTOK_LIT"), snapshot-BEFORE-any-mutation, pad-byte
zeroing loop with "arena memory is not zeroed", finishDefinition,
abortDefinition). Offsets-only discipline kept with nuance: "The compiler
holds `openDef.entryOff` (an offset) and **no pointers** across emits —
including branch back-patch positions for future `IF`/`THEN` (stage 2); any
cached pointer is invalid after any emit". Smudge/lookup paragraph kept
verbatim ("verified committed ... forth_dict.c:172-176 ... No change
needed"). Base grow-in-place rationale (upstream `_insertInProgram`
pattern) and the REJECTED temp-buffer paragraph retained — not superseded.
Superseded base text removed: the old 4-bullet startDefinition/emit/finish/
abort spec (including "required change — the current walk does not check
flags", now stated as committed).

**C-10 — dict hardening**
Lives in: §3.3.8 (both items verbatim: 64 KB wrap check "`if
((uint32_t)fdict.here + neededBytes > 0xFFFEu) { RAM_FULL; return false;
}`", 0xFFFF stays unused; count cap in startDefinition, Allocate uncapped
for tests, "the compiler must never bypass `startDefinition`"). Echoed in
§5.2 grow policy and §7 invariants.

**C-11 — immediacy scope**
Lives in: §3.3.9 verbatim ("`FF_IMMEDIATE` is honored for **primitives
only** ... a **stated non-goal**, not an oversight ... No stage-C code may
assume colon words are never immediate in the *encoding*"). Cross-refs:
pseudocode immediate-prim comment, §7.4 "(primitives only in stage C —
C-11, §3.3.9)".

**C-12 — nested-entry error code resolved in code's favor**
Lives in: §3.2 pseudocode ("displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,...)
... Error code per C-12: the committed guard (forth_inner.c:105);
deliberately DISTINCT from the rstack-depth guard's ERROR_RAM_FULL below")
and §3.2 guard paragraph ("**Nested-entry error code (C-12 — doc/code drift
resolved in code's favor)** ... tests asserting it ... Do not touch the
working guard"). Also §8 C3 row + §8 status update. The outer guard (C-5)
and the compile-state label error (C-1) use the same code independently.
Superseded base text removed: nested-entry `ERROR_RAM_FULL` in pseudocode,
"(same error class as the rstack-depth guard: return-stack resources
exhausted)" rationale, and the §8 C3 row's old error code.

**C-13 — §5.4 cost formula**
Lives in: §5.4 — "cost(word) = ceil4(4 + nameLen) + 2 * cells" with
"`cells` = 1 (the `FTOK_EXIT`) + per token: PRIM/CALL 1, ILIT 3, LIT 9,
BR/0BR 2, C47 2 (PTP_NONE) or 3 (PTP_NUMBER_8 padded, PTP_NUMBER_16)" and
"the earlier `2*(tokenCount + 1)` form undercounted every inline payload
and is superseded". Worked examples recomputed under the new formula (SQ
still 14 bytes; second example restated as "100 single-cell tokens
(PRIM/CALL)" so it stays true).

## B. Cross-cutting decisions you named (preserved with nuance)

- **C-8 int32-range-check-then-real34-fallback, both states** — §3.3.5
  (quote in C-8 row above; the base "documented stage-1 limitation, applied
  identically in interpret state" paragraph is also retained just above it).
- **C-9 offsets-only, no cached pointers across realloc** — §3.3.7 (quote
  in C-9 row) + §5.3 (base, untouched) + §5.2 grow policy.
- **C-5 private forthSource buffer** — §3.3.2 (never tmpString/aimBuffer/
  errorMessage; §5.4 overheads updated to match).
- **C-1 emit-only PRIM/CALL/LIT/ILIT/EXIT scope** — §3.3 intro + §3.3.6.
- **ASLIFT-set-on-exit + lastErrorCode gate** — §3.2 "ASLIFT on exit"
  paragraph (base, untouched incl. the test-c assertion flip) + pseudocode
  `setSystemFlag(FLAG_ASLIFT)` at rsp==0 + per-prim `lastErrorCode` check;
  interpret-state mirror in §3.3.4/C-7 and pseudocode line-end arm.
- **Re-entrancy guard error = ERROR_OPERATION_UNDEFINED, distinct from
  RAM_FULL** — §3.2 (C-12 row above). The rstack-depth guard and runaway
  cap keep `ERROR_RAM_FULL` — unchanged, and now explicitly contrasted.
- **Managed allocC47Blocks region, NOT a static arena** — §5.2 (base,
  untouched): "one `allocC47Blocks` region ... Rejected option (B): reserve
  a fixed top-of-arena slice".
- **ITM_FORTH 2842 / ITM_FCALL 2843** — §0.1/§0.2 (base, untouched),
  including the ITM_FWORD naming warning and the exact item rows.

## C. Base-spec bugs corrected in place (your item 2)

1. **§3.2 prim dispatch `forthPrims[tok].fn()`** → now
   `forthPrims[tok - 1].fn()` ("decode subtracts FTOK_PRIM_BASE = 1 (§2.2)
   — NEVER index by raw tok"). Also added the per-dispatch
   `clearSystemFlag(FLAG_ASLIFT)` scrub that C-7 cites as committed
   behavior (forth_inner.c:170-171).
2. **§2.2 FTOK_C47 note** "restrict to PTP_NONE/PTP_NUMBER_8 and widen
   later" → committed decoder set per C-1: "accepts exactly `PTP_NONE`,
   `PARAM_NUMBER_8` (cell-padded) and `PARAM_NUMBER_16`; any other PTP ...
   raises `ERROR_OPERATION_UNDEFINED`"; §3.2 FTOK_C47 arm advance updated
   to match (+2 for NUMBER_16 too).
3. **§7.3 acceptance** implied the compiler emits `0BR` (`: ABS DUP 0BR
   ... ;`) — contradicted C-1. Now: branch tokens "exercised by a
   hand-assembled body test (e.g. the Stage-1-B backward-loop test, §2.2
   notes)"; source-level branch tests are stage 2.
4. **§7 invariant `fdict.count ≤ 0x7EFF - 0x1000`** was off by one against
   C-9's cap (which permits indices 0..0x6EFF, i.e. final count up to
   0x6F00). Amendment wins: "`fdict.count ≤ 0x6F00` — equivalently, every
   emitted `FTOK_CALL` token ≤ 0x7EFF (max colon index 0x6EFF)".
5. **FTOK_LIT size**: token table row now carries the citation
   "real34_t / decQuad = `REAL34_SIZE_IN_BYTES`, realType.h:13"; §3.2
   already advanced ip by 16; C-13's formula (LIT = 9 cells) supersedes the
   old cost math. No 8-byte assumption remains (grepped).

## D. Base trailing patch-sections folded (not amendment-file items)

- "Stage 1 — Resolution Clarifications" → §4.1 (forthFindColon bool +
  out-param, case-sensitive compareString(CMP_BINARY)) and §1.3 (glyph
  encoding of primitive names). Section removed from the tail.
- "NOTE (§3.3 tokenizer)" → §3.3.3 (glyph advance + UTF-8 warning).
  Removed from the tail.
- "§2.2 correction" (FTOK_LIT = 16) → token table + C-13 formula. Removed
  from the tail.

## E. FLAGS — judgment calls to eyeball (nothing dropped)

1. **§8 status annotations.** The base §8 said every row was "confirmed
   absent as of 2026-07-06"; the amendment (2026-07-07, post-H1) states
   some are now committed. I annotated ONLY the rows the amendment
   explicitly confirms (C2 decoder, C3 guard, C5 smudge-skip) and said the
   rest "carry no confirmed status from that audit". If you know C1/C4/C6/
   C7/T1-x also landed, update the note — I did not infer beyond the text.
2. **forthPushInt32 dtLongInteger fix status** (§3.3.5 "Required code
   change (H1 series)") — the amendment cites the helpers (C-7,
   forth_inner.c:17-43) but never says the int32ToReal34→longInteger change
   landed, so the base "required change" wording is retained as-is.
3. **Worked example rewording** (§5.4): "100-token body" became "body of
   100 single-cell tokens (PRIM/CALL)" — same arithmetic, restated so the
   example is well-defined under C-13's per-token cell weights.
4. **C-9's "steps 2–4" reference**: the amendment said forthDictAllocate
   performs "§3.3 startDefinition steps 2–4 verbatim", referring to the
   old numbered bullets this consolidation deletes. Reworded to name the
   steps ("header written with FF_SMUDGE, link = latest, latest/count
   updated, here bumped to exactly ceil4(4 + nameLen) = body start") —
   content identical, reference de-dangled.
