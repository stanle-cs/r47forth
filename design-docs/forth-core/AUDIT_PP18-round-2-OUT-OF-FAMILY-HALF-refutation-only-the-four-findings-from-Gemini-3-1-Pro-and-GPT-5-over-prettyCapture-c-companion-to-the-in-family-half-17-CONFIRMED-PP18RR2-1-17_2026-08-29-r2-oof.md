# Audit — PP18 round 2, OUT-OF-FAMILY HALF (refutation only), at `34ac6e97f`

Companion to the in-family half
(`AUDIT_PP18-round-2-NO-FIX-WAVE-EXISTS-…_2026-08-29-r2.md`, seventeen CONFIRMED,
`PP18RR2-1`–`-17`). Same tip, same tree, no fix wave in between. This half
carries the two out-of-family readers of round 2 and nothing else: **four
findings raised over `prettyCapture.c`, each put through an independent
refutation pass with one assigned lens and a default of REFUTED.**

**Two survived. Only one of them is a new defect.** The other is the same
defect the in-family half filed as `PP18RR2-6`, reached blind by a different
family through a different door, and it is recorded here as corroboration
rather than as a second number. New number this half: **`PP18RR2-OOF-1`**, one
byte of guard arithmetic in `ppcEmit`.

With this half, **round 2 has been read by all three families** (in-family
dimensions, Gemini, GPT/Sol) and is complete under the 2026-08-29 ruling. It is
not clean, so the exit clock does not move.

---

## 1. Subject and coverage

### Out-of-family accounting

Both readers ran. **Both reply files exist and are non-empty**, both open with a
`MODEL:` line, and both were read in full for this report — no timeout, no
overwrite, no banner required.

| reader | packet | reply | `MODEL:` line (verbatim) | findings raised |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r2/arena.md` (18,161 B) | `/tmp/pkt-r2/arena.gemini.reply.md` (3,646 B) | `MODEL: Gemini 3.1 Pro (High)` | **1** (plus 5 items considered and explicitly cleared — merged into §6) |
| sol | `/tmp/pkt-r2/histring.md` (15,152 B) | `/tmp/pkt-r2/histring.sol.reply.md` (5,987 B) | `MODEL: GPT-5` | **3** (two filed as defects, one filed as a contract defect with its own caveat; plus 9 cleared items — merged into §6) |

**Identity, and an asymmetry in it.** `dispatch.sh:98` matches Sol's reply
against `/gpt|codex|sol/`; the reply self-reports `MODEL: GPT-5` and the
dispatcher's own transcript in `/tmp/pkt-r2/histring.sol.reply.md.err`
independently records `model: gpt-5.6-sol` — two sources. `dispatch.sh:79`
matches Gemini against `/gemini/`, but `/tmp/pkt-r2/arena.gemini.reply.md.err`
is **0 bytes**, so Gemini's identity rests on its own self-report alone. That
is the weaker of the two checks and it is a process item (§8), not a reason to
discount the reply.

### Subject

Tip `34ac6e97f`, identical to the in-family half's — no commit has landed, so
the two halves read the same bytes. The out-of-family subject is one file,
`packages/pretty-print/prettyCapture.c`, split into two self-contained packets
at a function boundary.

**`arena.md`** carried the arena and transform half: `ppcTopSlot`, `ppcInit`,
`ppcTestDeinit`, `prettyReset`, `ppcAlloc`, `ppcFreeTree`, `ppcDeepCopy`,
`ppcAllocParamOf`, `ppcValLeafFromRegister`, `ppcRclLeaf`, `ppcEnsureKnown`,
`ppcTreeHasOpaque`, `ppcDisplaced`, `ppcInvalidate`, `ppcShiftUpForLift`,
`ppcShiftDownAfterConsume`, `ppcCurrentRevalidate`, `ppcScopeOk`,
`ppcSupersedeCurrent`.

**`histring.md`** carried the serialization and ring half: `ppcSerializeNode`
(all token arms), `ppcHistEvictOldest`, `ppcEmit`, `ppcHistoryCount`,
`ppcHistoryEntry`, `ppcHistoryClear`, `ppcShadowInvalidate`.

### What was NOT sent out-of-family, and what that cost

Everything else in the file: `ppcClassify` and the two dispatch hooks with all
of their STAGE and DONE arms — roughly six hundred lines, the largest part of
`prettyCapture.c` — the three NIM hooks, and the whole of `prettyValue.c`,
`prettyFormula.c`, `prettyLayout.c`, `prettyEquation.c`, `prettyVisual.c`, the
browsers and `prettyTest.c`.

That boundary is visible in the results. **Neither reader could see a write
path.** Gemini's finding arrives with its central question unresolved — it can
show that slots above the cap survive, but not who moves the registers
underneath them, because `PPC_STO_NOP` was not in its packet. The refutation
had to supply `prettyCapture.c:1004` to settle it. Both of round 2's worst
defects live in the code that was not sent.

### Disclosure: one finding was seeded by its own packet

`arena.md`'s orientation names the hazard: *"`ppcTopSlot()` returns the live
stack height minus `REGISTER_X` — the calculator has a 4-level or 8-level stack
depending on a user mode flag, so slots ABOVE the top are not in use at that
moment but their array entries still exist."* Gemini's finding is downstream of
that sentence. It still had to build the shrink/regrow round trip, the
resurrection, and the invariant argument, and it did — but "independent" is
weaker here than the reply's isolation suggests, and the report says so rather
than banking the confirmation at full value.

The second packet has the mirror-image problem, in the other direction:
`histring.md`'s orientation asserts *"`ppcHistSeq` a monotonically increasing
stamp"* — a property the tree does not state anywhere. One of Sol's three
findings is filed against that sentence (§5, §6).

### Reading budget

**This half ran no find pass.** The four findings are the finders' output; the
work recorded here is refutation. The refuters read `prettyCapture.c` end to
end plus the seams each finding needed: `items.c` rows 1938/1939,
`src/c47/flags.c:799-880` (`SetSetting`/`clearSetPairs`), `src/c47/stack.c`
(`CLSTK`, `FILL`), `src/c47/registers.c:1329` (`fnClearRegisters`),
`src/c47/defines.h:1272-1279` (register ids) and `:2292` (`TO_BYTES`),
`prettyFormula.c` (`ppfBuildEntry`), `browsers/prettyBrowser.c`
(`pbFindResult`, `prettyBrowserEnter`), `prettyInternal.h`, all three
pretty-print design docs, and `prettyTest.c` by targeted grep. **Nothing here
is coverage of any file**, and no finding in this half should be read as
"`prettyFormula.c` was audited by a second family".

---

## 2. Mechanical results

**Gate.** One refutation needed a mutation and ran the package's own gate twice
to completion in an isolated worktree: baseline `PRETTY-PRINT GATE GREEN`,
testSuite OK, 163.27 s, Fail 0; with the mutation applied through the real
refresh, green again, 162.12 s, Fail 0. Reverted by inverse edit followed by
`python3 tools/pkg_patch_refresh.py packages/pretty-print`.

| mutation | why | observed | finding |
|---|---|---|---|
| `prettyCapture.c:436-437` → `buf[2] = 0; buf[3] = 0;` (every history entry stamped seq 0 — strictly worse than one wrap per 65,536 emits) | prove or kill the claim that the sequence stamp has any consumer | mutation verified present in the regenerated `files/` twin and in the rebuilt `custom_pkg_shadow` object; **gate GREEN**, Fail 0 | **REFUTES** OOF-S4 |

The other three refutations needed no build: two rest on arithmetic plus a dead
call path, one on a ruling and an intent search. Nothing was measurable, so
nothing was measured — and that is stated rather than papered over with a
courtesy gate run.

**`design-audit.sh`** is forth-core's; there is still no pretty-print
equivalent, so no override-budget check ran in this half either. No override
file was touched.

**Tree state.** The main tree is clean in `packages/` at the start and at the
finish of this half: `git status --porcelain` shows only untracked audit reports
and the operator's own in-flight skill/workflow edits; `grep -rn AUDIT-PROBE
packages/` is empty, re-verified while writing this file.

**Worktree staleness, sixth consecutive round.** All four refuters spawned at
`e21af8d28` — 111 commits behind the tip, not an ancestor of it, a tree in
which `packages/pretty-print/` does not exist at all. Every one of them
detected it with `git log --oneline -1`, confirmed it with
`git merge-base --is-ancestor` or `git rev-list --count`, and checked out
`34ac6e97f` detached before reading a line. The guard in `audit-workflow.js` is
still absent. Unlike the in-family half, no refuter this half found a sibling's
`build.sim`, a clobbered `/tmp` log, or a foreign edit.

---

## 3. CONFIRMED findings, worst first

### 3.1 — The `SSIZE8 → SSIZE4 → SSIZE8` round trip is `PP18RR2-6`, found blind by a second family. **No new number.**

`packages/pretty-print/prettyCapture.c:594` (`ppcShiftUpForLift`), `:606`
(`ppcShiftDownAfterConsume`), against `ppcTopSlot()` at `:86-88` — and, once
the refutation supplied the missing half, `:1004` (`PPC_STO_NOP`).

**What Gemini filed.** In 8-level mode, fill slots 4–7. `SSIZE4`. Every loop in
the engine that walks the shadow is bounded by `ppcTopSlot()`, which is now 3,
so slots 4–7 are touched by nothing — not the lift shift, not the consume
shift, not `R↑`/`R↓`, not `FILL`. `SSIZE8`. `ppcTopSlot()` is 7 again and those
four trees re-enter the live shadow holding what they held before the flip.

**What the refutation added, and why the finding survives.** The reply's own
open question — *does anything write registers A–D while 4-level mode is
active?* — was deliberately left unjudged, and it resolves against refutation:

- `src/c47/flags.c:799-880` `SetSetting` for `SS_4`/`SS_8` matches
  `clearSetPairs` and does nothing but `fnClearFlag`/`fnSetFlag(FLAG_SSIZE8)` +
  `fnRefreshState()`. No register is cleared, so registers 4–7 keep their
  pre-switch values across the flip: slot and register go stale *together* and
  the invariant would survive — if nobody wrote them.
- `src/c47/defines.h:1272-1279`: `REGISTER_A` = 104 = `REGISTER_X + 4`. Under
  `SSIZE4` those are ordinary lettered registers the owner stores into.
- `prettyCapture.c:1004`: `if(t > REGISTER_X && t <= (uint16_t)getStackTop())`.
  With `t` = 104 and `getStackTop()` = 103, false — the shadow does nothing
  while `fnStore` writes register A. `x<>A` (`:741`) is a second writer.
- `CLSTK` (`src/c47/stack.c:12`) and `FILL` (`:203`) are themselves bounded by
  `getStackTop()`, so they are innocent; `LOAD`/`LOADST` are `US_CANCEL` →
  `PPC_INVALIDATE` by the package's own `R1-6`, also innocent. **`STO A` is the
  survivor.**

Concrete path: `SSIZE8`; `2 ENTER 3 +`; four `ENTER`s walk that tree to slot 4
(register A = 5); `SSIZE4`; `9 STO A`; `SSIZE8`; four `R↓`. Slot 4 claims A
holds `2+3` while A holds 9.

**Why it is not a new number.** That is `PP18RR2-6`, which the in-family half
found through three dimensions and *executed*, printing the drawn signature
`1 2 ×` beside a real result of 18 from the same accessor the screen uses. Same
file, same invariant, same window, same writer. **A finding number is a fix
obligation, and two numbers for one defect is exactly how a class gets fixed at
one site and not its sibling** — this round's own `PP18RR2-3`/`-4`/`-9` shape.
It is filed here as corroboration and as an addendum.

**What the second family adds that `PP18RR2-6` does not carry.**

1. **The latency framing.** `PP18RR2-6` is written as a guard defect: the write
   detector is bounded by a moving cap. Gemini's is written as a *residency*
   defect: slots above the cap leave the reachable set entirely, so nothing —
   not just the write detector — maintains them while they are out. The
   distinction matters for the fix. Widening the four guards to the fixed index
   space closes the writer that is known; it does not establish that the
   out-of-range slots are maintained by anything else, and the engine's two
   index spaces (`ppcInvalidate` loops `i < 8`, absolute; every motion helper
   loops to `ppcTopSlot()`, relative) still disagree with no comment saying
   which is authoritative.
2. **A second fix option nobody had named.** `SSIZE4`/`SSIZE8` (items.c rows
   1938/1939) are `US_UNCHANGED`, so `ppcClassify`'s default rule at
   `prettyCapture.c:576` — *"unknown non-undo items are display/mode chatter —
   ignore"* — files them with the display chatter and `prettyNoteFunction`
   returns before any transform. They are the **only** items that change the
   shadow's index space, and they are classified as the items that change
   nothing. Re-classifying them (invalidate, or re-scope the slots) is the
   other repair, and it is the one that closes the class rather than the writer.
3. **The intent search came back completely empty**, which is the strongest new
   fact in this half. `DESIGN.md` (823 lines), `DESIGN-HISTORY.md` and
   `TESTING.md` have **zero hits** for `ssize`, `stack size`, `getStackTop`,
   `4-level`, `8-level` or `stack depth` in this sense. `prettyCapture.c`'s only
   comment in the neighbourhood is the generic default rule at `:576`. The
   `PP9` entry and the `R1-8`/`R3-2/3/4` block at `:1004` reason at length about
   `STO` targeting "a stack register" — always as a fixed set. **The
   possibility that the set shrinks and regrows is not raised anywhere in the
   package's writing.** There is no ruling to refute the finding with, and no
   evidence the author ever considered the flip.

**Violated.** `DESIGN.md` §3 (`:164-168`), marked BINDING: *"shadow slot k
always holds an expression whose value equals the live contents of register
`REGISTER_X + k`. When a transform cannot maintain that, the slot degrades to a
value leaf snapshotted from its register (truthful by construction) or the whole
shadow invalidates. The display never lies; over-invalidation only costs history
granularity."* Over-invalidation is permitted by name; a surviving wrong slot is
the forbidden direction.

**Bug class.** An invariant stated over a fixed index space, enforced by loops
and guards written against a runtime-variable bound.

**Class-level test — one pin `PP18RR2-6`'s does not already give you.**
`PP18RR2-6` asks for the whole capture battery run a second time with
`FLAG_SSIZE8` cleared, plus a targeted write pin. The round-trip needs its own
pin because it fails for a different reason: fill slots 4–7 under `SSIZE8`,
`SSIZE4`, **do nothing at all except one `STO A`**, `SSIZE8`, then assert slot 4
is `PPC_UNKNOWN` or the shadow is invalid. `grep -ci ssize
packages/pretty-print/prettyTest.c` returns **0** — the package has no
stack-size coverage whatever, in either shape.

---

### 3.2 — `PP18RR2-OOF-1`: the result-snapshot guard reserves 7 bytes for a 6-byte header, so an entry that ends exactly on the cap loses its `= result`

`packages/pretty-print/prettyCapture.c:420` (the guard), `:421-427` (the write).

**What breaks.** The `TKRES` block writes six fixed bytes — token kind,
`dataType`, `tag`, `allocParam` lo/hi, `len` — then `xcopy`s `bytes` of
payload. Its guard demands `off + 7 + bytes <= sizeof(buf)`. One byte more than
it writes, so an entry whose last byte lands exactly on 320 is refused and the
formula is filed **without** its result snapshot.

**Concrete reaching input**, inside every limit in the file. Default 4-level
stack. Type a 15-digit literal (one `PPN_LIT`; `PPC_LIT_CAPACITY` is 30, so no
`PPN_LIT2`), then repeat `RCL A` followed by `+` eleven times. `RCL` of a
lettered register takes the `else` arm at `:1024-1026` because `param` (104) is above
`getStackTop()` (103), and `ppcRclLeaf` (`:234`) sends `param > 99` to
`ppcValLeafFromRegister`, minting a `PPN_VAL` leaf with `pad[1] = 16` for a
`real34`. Nodes: 1 `LIT` + 11 `VAL` + 11 `OP2` = **23** of `PPC_NODES` 24, and
the L slot never allocates. Bytes: 6 header + (2+15) + 11×22 + 11×3 = **298**.
Finish with X holding a non-matrix `real34`: `bytes` = 16, `TKRES` = 6+16 = 22,
total **320 = `sizeof(buf)` exactly**. The shipped guard computes 298+7+16 = 321
and skips it. (Sol's own construction — two 30-character literals at 32 bytes
each, nine value leaves, ten dyadic ops — lands on the same 298 by a different
route and is equally legal.)

**Consequence.** The history row draws as a bare formula with no `= result`, and
the browser's `ENTER` on that row can never recall it:
`browsers/prettyBrowser.c:229-232` takes `payload == NULL` from `pbFindResult`
and leaves the browser with no recall. The formula itself is stored and drawn
correctly; nothing is lost but the snapshot.

**Violated.** `prettyInternal.h:91` states the token layout verbatim —
*"value: dataType u8, tag u8, allocParam u16, len u8, payload"* — and `:96`
defines `PPT_TKRES` as *"result snapshot: same shape as TKV"*. That is 6 bytes
plus payload. `ppcEmit`'s own header comment promises *"`resultReg >= 0`
supplies the `= result` snapshot (the register still holding the value — the
invariant is the proof)"*; here the register holds the value, the entry fits,
and the snapshot is dropped anyway.

**The reported second producer is refuted.** Sol filed this as one class with
two producers, the twin being the identical `off + 7 + bytes > cap` at `:322`
(`PPN_VAL`), with the consequence *"a formula one byte from the cap is lost
entirely"*. That consequence is not caused by the off-by-one. `ppcEmit:395-397`
returns unless the root kind is `PPN_OP1`/`PPN_OP2`/`PPN_BIGOP`, so a value
token can never be the last thing in the stream; the shipped and corrected
guards differ in exactly one case, a `VAL` write ending precisely at `off ==
cap`, and there the enclosing operator still has to serialize — at `off == 320`
every remaining arm fails (`OP1`/`OP2` need 3, `BIGOP` 21, a sibling `LIT` at
least 2) and the formula returns `0xffff` either way. **One producer, and the
severity is a missing snapshot, not a lost formula.** Sol's reply flagged its
own uncertainty here honestly ("I could not construct an otherwise-fitting
complete formula that is rejected solely by that extra byte"); the refutation
settled it negative.

**Bug class.** Guard arithmetic re-derived beside each write instead of shared
with the layout it guards. Seven space tests in this file; six are exact —
`LIT` tests `2 + total` for a 2+total write, `OP1`/`OP2`/`CONST`/`RCL` test 3
for 3, `BIGOP` tests 21 for 21 — and the two that reserve an extra byte are
`PPN_VAL` (harmless) and `TKRES` (not).

**Class-level test.** The class is enumerable by token kind, so pin it that
way rather than by formula: one case per `PPT_*` that builds the maximal
instance of that token such that the entry ends **exactly** at
`PPC_HIST_BYTES/2`, and asserts the entry is stored, its header length is 320,
and its final token is the one under test. The cheapest single pin that would
have caught this: drive the 298-byte formula above, then assert
`ppcHistoryEntry(0, &len, NULL)` gives `len == 320` with a trailing
`PPT_TKRES`. The one-byte fix is `off + 6 + bytes` at `:420`; `:322` only for
class uniformity, tagged no-observable-effect.

**What I would leave alone.** On owner cost alone, this does not justify a
commit: it fires only for entries landing on exactly one value out of 320, and
its worst outcome is a row that draws without `= result`. It is worth fixing
because the edit is one character and it rides along with the class, not
because anybody will meet it. Every one of the seventeen in-family findings
outranks it.

---

## 4. PLAUSIBLE findings

**None.** Both survivors carry a constructed, verified reaching input, and both
refuted findings died on a ruling and on reachability rather than on "nobody
could build the input". Nothing in this half is stuck at "probably".

Two sub-questions stayed open at the end and neither is a finding:

1. **Are there writers to `REGISTER_A`–`D` under `SSIZE4` beyond `STO`,
   `STO±×÷` and `x<>reg`?** `CLSTK` and `FILL` are bounded by `getStackTop()`
   and innocent; `LOAD`/`LOADST` invalidate. `src/c47/registers.c:1329`
   (`fnClearRegisters`) clears A–D specifically when `FLAG_SSIZE8` is clear —
   **settled by checking `ITM_CLREGS`'s `US_STATUS` row**: if it invalidates,
   the guard-widening repair alone closes the window; if it does not, the
   classifier repair (3.1, option 2) is required. That check is one grep and
   belongs to whoever takes `PP18RR2-6`.
2. **Can any input make the `PPN_VAL` guard at `:322` decide alone?** Settled
   NO by the root-kind gate at `:395-397`. It reopens the day `ppcEmit` accepts
   a non-operator root.

---

## 5. Design observations

**D-OOF1 — `ppcHistSeq` is state with no consumer, and its documented property
gets asserted three times outward from a value nothing depends on.** The census
is complete: the only reader of header bytes 2–3 is `ppcHistoryEntry`; both
product call sites (`prettyFormula.c:660`, `browsers/prettyBrowser.c:227`) pass
`NULL` for `seqOut`; the four test sites (`prettyTest.c:1110`, `1254`, `1533`,
`1734`) write `eseq` and never read it; `ppfBuildEntry` starts at offset 6 and
jumps the field; ordering everywhere is positional (`phys = ppcHistCount - 1 -
idx`) and eviction reads only `firstLen`. Nothing compares two stamps anywhere
in the tree. Yet the header documents the field, `DESIGN.md:294` names it in the
accessor signature, and the audit packet upgraded it to "monotonically
increasing". Either give it a consumer — the browser ordering by stamp is the
obvious one, at which point the `uint16_t` wrap becomes a real question — or
delete it and the two bytes of header it occupies.

**D-OOF2 — an enumeration cleared over the wrong set, which is why a second
family was worth running.** The in-family half re-derived the serializer's space
tests arm by arm and cleared them: *"Every token arm's space test is correct or
conservative by one (TKO1/TKO2/TKC/TKR write 3 and test 3; TKV writes 6+bytes
and tests 7+bytes; TKBIG writes and tests 21; TKL writes and tests 2+total)"*.
Every word of that is true. The defective test is the one that is not an arm of
`ppcSerializeNode` — `TKRES` lives in the caller. **When a class is cleared by
enumeration, the enumeration's boundary is a function; the class's boundary is a
contract** — here the `PPT_*` layouts in `prettyInternal.h`, which `TKRES`
shares with `TKV` by explicit reference ("same shape as TKV"). Where the two
boundaries disagree, the clearing is worth exactly the smaller set. The general
form: clear guard classes over the *contract's* member list, not the switch
statement's.

**D-OOF3 — a packet can invent a contract, and the refutation pays for it.**
`histring.md`'s orientation asserts *"`ppcHistSeq` a monotonically increasing
stamp"*; `prettyInternal.h:89` says only *"seq u16"*, and `grep -rn monotonic`
over `packages/pretty-print` and `design-docs/pretty-print` returns nothing.
Sol filed a contract violation against the packet's prose — correctly and
traceably, since the packet is the only specification it was given — and killing
it cost a full gate cycle plus a mutation. The packet template's orientation
section should quote the code or `DESIGN.md`, or mark narration as narration.
That is a change to `references/packet-template.md`, not to the code.

---

## 6. Deliberately not flagged

### 6a. Killed by the refutation pass

**OOF-S2 — "`ppcEmit`'s 320-byte scratch buffer is half the 640-byte ring it
feeds, so a formula that fits history is silently dropped."** REFUTED twice
over.

*It is ruled.* `DESIGN.md:289-291` (§5, History ring): *"Ring: `ppHist[640]` +
12 offsets, oldest-first eviction (undo-history's eviction shape). **Oversized
entries (> half the ring) are dropped, not stored.**"* `git log -S"Oversized
entries"` returns one commit, `05500bae8`, the original package design doc, and
nothing in `DESIGN-HISTORY.md` retracts or narrows it. `prettyCapture.c:410` is
that sentence compiled: `uint8_t buf[PPC_HIST_BYTES / 2];`. The finding's stated
violation — "two places that must agree, with nothing forcing them to" — is
false on its face: `PPC_HIST_BYTES` (`prettyInternal.h:74`) is the single source
and the 320 is derived from it by the `/2` that *is* the rule.

*Its reaching input cannot exist.* The finding needs five `PPN_BIGOP` subtrees
co-resident in one tree, obtained by chaining five big-operator dispatches. The
`PPC_BIGOPSUM`/`PPC_BIGOPINT` DONE arm (`:1109-1116`) frees **every** slot and
sets `ppcSlot[0]` to the new `BIGOP` alone, so no prior `BIGOP` survives a
second dispatch; the earlier sum's value re-materialises at the next operator as
a 22-byte `VAL` leaf via `ppcEnsureKnown`, not as a 65-byte `BIGOP` subtree. The
maximum `BIGOP` count in a chained-dispatch tree is one, and the entry is about
93 bytes.

*The honest addendum.* A **different** input does exceed 320 — one sum, then
`ENTER` five times (`PPC_ENTER` deep-copies, `:903-906`), then five `+`: six
`BIGOP` copies + five `OP2` = 23 nodes ≈ 411 bytes. That input lands squarely
inside the ruling and is dropped as specified. It is the only place in this half
where ruled behaviour has a visible owner cost, and the ruling is the owner's to
revisit — not a finding.

**OOF-S4 — "`ppcHistSeq` is a `uint16_t` documented as monotonically increasing
and wraps to zero on the 65,536th commit."** REFUTED. The arithmetic is true and
leads nowhere. The stamp has no consumer (the census is in D-OOF1); the cited
contract does not exist in the tree (only in the packet, D-OOF3); and the
reachability was killed by **mutation rather than by reading** — forcing every
stamp to a constant zero, a strictly worse defect than one wrap per 65,536
emits, left the gate green with Fail 0, mutation verified present in the built
artifact. The precondition is also out of practical reach: `ppcHistSeq` is
zeroed only by `ppcInit` on cold start and `ppcHistoryClear` deliberately does
not touch it, so the wrap needs 65,536 successful emits inside one power-on
session on a hand calculator. A future consumer would inherit the hazard; that
is D-OOF1, not a defect in this tree.

### 6b. What the finders cleared — verified, not transcribed

**Gemini's five** (`arena.md`, all re-checked against the file at `34ac6e97f`):

- **Arena exhaustion never leaves a half-built tree linked in `ppcSlot[]`.**
  Confirmed: `ppcDeepCopy` (`:161`) returns `PPC_NIL` and unwinds,
  `ppcValLeafFromRegister` and `ppcRclLeaf` propagate `PPC_NIL`, and both
  `ppcEnsureKnown` (`:245`) and `ppcShiftDownAfterConsume` (`:606-615`) downgrade a
  `PPC_NIL` result to `PPC_UNKNOWN` — the truthful degradation §3 permits.
- **`ppcShiftDownAfterConsume` not freeing `ppcSlot[0]` is not a leak.** Correct
  and load-bearing: for a dyadic operation the old X is a *child* of the new
  root, so freeing it there would destroy the operand. Ownership sits with the
  caller, which is what the `PPC_RCLARITH` arm demonstrates by doing it by hand.
- **The transient alias in that loop is resolved.** After the shift,
  `ppcSlot[top-1]` and `ppcSlot[top]` name the same tree for two statements
  until `ppcSlot[top] = ppcDeepCopy(...)` replaces the top with a fresh copy; if
  the copy fails the slot degrades to `PPC_UNKNOWN` while the original stays
  owned by `ppcSlot[top-1]`. No permanent alias, no leak, no double free.
- **The partial-`ppcDeepCopy` failure path is clean.** `:172-179` frees `k0` and
  `k1` (both `PPC_NIL`/`PPC_UNKNOWN`-safe), then returns node `c` to the free
  list by hand without orphaning children. `ppcFreeTree`'s `PPN_FREE` early
  return is a real double-free guard.
- **Degrading X to `PPC_UNKNOWN` in `ppcShiftUpForLift` is design.** `DESIGN.md`
  §3 permits over-invalidation by name; the packet said so and the reader
  respected it. This is the discipline that keeps a third of an audit's output
  from being noise.

**Sol's nine** (`histring.md`):

- **Eviction arithmetic**, worked through a concrete four-entry example with two
  evictions for one insert. Re-derived independently: `ppcHistEvictOldest`
  (`:375`) reads `ppcHistOffset[ppcHistCount]` only after the decrement, so it
  is in bounds even at twelve.
- **`ppcHistOffset[0] == 0` holds inductively**, which is what the `memmove` and
  the `ppcHistUsed - (first + firstLen)` arithmetic depend on; subtracting
  `firstLen` rather than `first + firstLen` in the offset fix-up is right for
  that reason.
- **Every serialized length includes the six-byte header**, because
  serialization starts at offset 6 and `TKRES` is added to the same `off`.
- **`0xffff` poison propagates** through the unary, binary and big-operator
  recursion, and `ppcEmit` commits nothing partial.
- **The eviction `while` cannot spin**: a committed entry is at most 320 and the
  ring is 640, so at count 0 both predicates are false. Same argument the
  in-family half made independently.
- **Indexing is correct**: index 0 newest via `phys = count-1-idx`, index
  `count` and any index at count 0 return `NULL`, and `lenOut` comes from the
  selected entry's own header rather than an adjacent-offset difference.
- **`ppcHistoryClear` leaving stale offsets is harmless** — it resets both
  `ppcHistCount` and `ppcHistUsed`, and the next append writes offset 0.
- **Endianness** — explicit little-endian writes (`buf[0]`, `buf[1]`) read back
  with native `xcopy` — cleared as target-bound, and I agree: R47 on DM42n is
  ARM LE, the ring never leaves RAM or crosses a device, and the package follows
  upstream's own convention here. The reader flagged its own uncertainty rather
  than filing it, which is the right call on a packet with no target statement.
- **Pointer lifetime** cleared as unspecified — and it can be settled, which the
  packet did not let the reader do: `ppcHist` is `static uint8_t
  ppcHist[PPC_HIST_BYTES]` (`:72`), plain BSS, so upstream's moving register
  heap cannot relocate it; the only consumer that holds the pointer across calls
  (`prettyBrowser.c:226-243`) does its `xcopy` before `ppcShadowInvalidate()`,
  and that function does not touch the ring in any case. No stale-pointer bug.

### 6c. Cleared by the refutation pass beyond the four findings

- **The `ppcEmit` root-kind gate and `PPA_EMITTED`** were re-read while killing
  the `:322` twin. The ENTER-duplicate defect in that neighbourhood is the
  in-family half's `PP18RR2-8` and is not re-reported here.
- **Decoding a 320-byte entry** is fine: `ppfBuildEntry` (`prettyFormula.c:471`)
  walks `while(off < total)` over a 640-byte ring, and a left-leaning chain
  keeps its operand stack at 2 — well under the fixed limit of 8 that the
  in-family half filed as `PP18RR2-12`. The exact-fit entry does not
  additionally trip that finding.
- **`TKRES` decode symmetry** between `ppfBuildEntry:502-514` and
  `pbFindResult:178-186`: both parse `TKV` and `TKRES` with the same 6-byte
  head, which is the corroboration that `:420`'s 7 is the outlier and not a
  deliberate reserve.
- **The KNOWN set was fenced, not re-reported**: `PP18RR1-1..12` and `-P1`,
  `PP18R4-1..11` with the round-4 plausible carry, and the 2026-08-29
  out-of-family set in `HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md` §4.
  Neither survivor here overlaps any of them.

---

## 7. Verdict

**Would I ship on the strength of this half? No — and this half does not move
the answer on its own.** It contributes one byte-level defect nobody will meet
and a second family's signature on the one defect in round 2 that puts a formula
on the screen the calculator never computed.

**Where it breaks first: the `SSIZE4` window (`PP18RR2-6`).** That is now two
families and four dimensions, blind to each other, converging on one window; an
executed probe that printed the wrong drawn signature; a design corpus that
never mentions the mode; and a test suite with zero occurrences of `SSIZE`. It
is also the only defect here whose failure mode is the one `DESIGN.md` forbids
outright rather than the one it permits.

**If the goal is correct code rather than an audit-clean tree**, the work items
from this half are: fix `PP18RR2-6`, checking `ITM_CLREGS`'s `US_STATUS` row
first to decide whether widening the four guards is sufficient or whether the
classifier must stop filing the stack-size items as chatter; take
`PP18RR2-OOF-1` as a one-character rider on that commit; and leave `:322` alone
except for uniformity. Nothing else in this half earns a commit — the
oversized-entry drop is ruled, and the sequence wrap is a property of a value no
shipped code reads.

---

## 8. Round and exit state

**Round: PP18 round 2 of the restarted series, out-of-family half.** Subject
`pretty-print/stage-pp17..34ac6e97f`, tip `34ac6e97f`, no fix wave — the same
tree the in-family half and restarted round 1 both read.

### Readers

| reader | packet → reply | `MODEL:` line (verbatim) | raised | survived refutation |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r2/arena.md` → `/tmp/pkt-r2/arena.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 1 | **1** → folded into **`PP18RR2-6`** (not a new number; adds the residency framing, the classifier fix option, and an empty intent search) |
| sol | `/tmp/pkt-r2/histring.md` → `/tmp/pkt-r2/histring.sol.reply.md` | `MODEL: GPT-5` | 3 | **1** → **`PP18RR2-OOF-1`** (re-filed as one producer, not two; severity reduced from lost formula to missing snapshot). 2 refuted: the oversized-entry drop (ruled + dead input), the sequence wrap (no consumer, killed by mutation) |

Every finding was refuted independently under one assigned lens (reachability,
correctness, intent), default REFUTED, with coverage claims proven by mutation.
No refuter saw another's verdict.

**Counts.** Four raised, **two survived (50%)**, **one new number**. Round 2
across both halves: **eighteen distinct CONFIRMED defects** — `PP18RR2-1`–`-17`
plus `PP18RR2-OOF-1`. `grep -rn PP18RR2-OOF` over the repository returned
nothing before this file was written.

**Three-family status: SATISFIED.** In-family dimensions, Gemini 3.1 Pro and
GPT-5 have all read this subject. Round 2 is the second PP18 round to satisfy
the 2026-08-29 ruling (restarted round 1 was the first), and the first in which
the out-of-family packets took `prettyCapture.c` — which is what restarted round
1's §8 asked for by name ("the next packet should be the capture staging
machine").

**Exit criterion: NOT MET.** The round is complete but not clean: eighteen
CONFIRMED findings reset the count on their own. The criterion's two consecutive
clean rounds, at least one out-of-family, stands where restarted round 1 left
it.

### Process items

1. **Stale worktrees, sixth consecutive round.** All four refuters spawned at
   `e21af8d28`, 111 commits behind, not an ancestor, in a tree where
   `packages/pretty-print/` does not exist. All four detected it and checked out
   `34ac6e97f` before reading, which is the only reason this half is usable. The
   `git merge-base --is-ancestor` guard in `audit-workflow.js` is **still
   absent** after being requested by five previous reports.
2. **Gemini's identity check is one-sided.** `agy` wrote nothing to
   `arena.gemini.reply.md.err` (0 bytes), so the reader's identity rests on its
   own `MODEL:` line; Sol's is corroborated by the codex transcript header
   (`model: gpt-5.6-sol`) in a 27,655-byte `.err`. If the Gemini driver can be
   made to echo the resolved model to stderr, the check becomes two-sided for
   both families.
3. **The packet invented a contract and the refutation paid for it** (D-OOF3).
   One of four findings, one gate cycle, one mutation — spent killing a property
   that exists only in the packet's orientation prose.
4. **A refutation pass is not scoped to the packet, and should be told so.**
   Two of the four findings turned on code the packet did not contain
   (`PPC_STO_NOP` for one, the `BIGOP` DONE arm for the other); both refutations
   had to leave the packet to settle them, and both were right to. The
   refutation brief should say this explicitly, because a refuter who assumed
   packet scope would have returned SURVIVES on the oversized-entry finding and
   REFUTED-as-unreachable on the mode finding — both wrong.
5. **Cross-half deduplication has no owner.** The in-family half is numbered by
   the workflow; this half was numbered by hand, and nothing in the tooling
   would have caught that Gemini's finding is `PP18RR2-6`. The rule that follows
   from this round: **the out-of-family half is written after the in-family half
   and greps its numbers before minting one.** Had the halves run in the other
   order, the round would have shipped two numbers for one defect.

### Round 3's axis, from this half

1. **An out-of-family packet over `ppcClassify` and the dispatch arms** — the
   ~600 lines of `prettyCapture.c` that have never been sent out-of-family, and
   where both of round 2's worst defects actually live. This half is the
   argument's evidence: Gemini could see the symptom and not the cause, because
   the cause was not in its packet.
2. **The fix wave, when it lands** — eighteen findings, seven-for-seven on the
   fix-regression pattern, and the shape to hunt in `PP18RR2-6`'s repair is the
   one 3.1 names: a fix that closes the known writer without re-establishing who
   maintains a slot that is out of range.
3. **The acceptance-parity oracle**, still unreached by either half of round 2.
