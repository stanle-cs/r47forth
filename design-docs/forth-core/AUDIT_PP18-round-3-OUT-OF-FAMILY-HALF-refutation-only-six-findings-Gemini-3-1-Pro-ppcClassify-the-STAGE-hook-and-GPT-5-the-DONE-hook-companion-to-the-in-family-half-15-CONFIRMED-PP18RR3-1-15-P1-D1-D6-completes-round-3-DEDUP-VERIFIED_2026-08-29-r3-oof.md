# PP18 audit — round 3, OUT-OF-FAMILY HALF (refutation only)

**Tip `34ac6e97f`. Range `pretty-print/stage-pp17..HEAD`. 2026-08-29.**
Subject: `ppcClassify` + `prettyNoteFunction` (STAGE) to Gemini 3.1 Pro,
`prettyNoteFunctionDone` (DONE) to GPT-5. Six findings entered the refutation
pass; **none survived**. No new numbers minted. Companion to the in-family
half (15 CONFIRMED `PP18RR3-1`–`-15`, `PP18RR3-P1`, `PP18RR3-D1`–`D6`); with
this file round 3 satisfies the three-family ruling.

*(Filename note: the requested title exceeded the 255-byte limit and was
shortened. Same subject, same date suffix.)*

---

## 1. Subject and coverage

### Out-of-family accounting

Both reply files exist, are non-empty, and were read in full. No timeout, no
overwrite, no banner.

| reader | packet → reply | `MODEL:` line, verbatim | reply size | findings raised |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r3-classify.md` → `/tmp/pkt-r3-classify.gemini.reply.md` | `MODEL: Gemini 3.1 Pro` | 7,562 B | **5** (numbered 1–5) + 3 cleared |
| sol | `/tmp/pkt-r3-done.md` → `/tmp/pkt-r3-done.sol.reply.md` | `MODEL: GPT-5` | 5,015 B | **1** filed + **1** named-but-deliberately-unpromoted hazard, which the operator carried into refutation as the sixth item; + 9 cleared |

Six items went to refutation: `OOF-G1`, `OOF-G2`, `OOF-G4`, `OOF-G5` (Gemini),
`OOF-S1`, `OOF-S2` (GPT-5). Gemini's #3 (mode toggles invalidating) was merged
into `OOF-G4` by the operator, which is the right call — they are one objection
to one rule, and the refuter checked the merged second face separately.

**Identity.** Sol's is corroborated outside its own reply: the codex transcript
header in a 30,731 B `/tmp/pkt-r3-done.sol.reply.md.err` reads `model:
gpt-5.6-sol`. Gemini's `.err` is **0 bytes**, so its identity rests on its own
`MODEL:` line alone. This is the identical asymmetry round 2's §8 recorded and
it is still unfixed in `dispatch.sh`.

### Subject, and what each packet withheld

No fix wave has landed since round 1. The tree is byte-identical to the one
restarted round 1, round 2 (both halves) and this round's in-family half read.

| packet | contents | withheld |
|---|---|---|
| classify | `ppcStage` struct, `ppcScopeOk`, `ppcSupersedeCurrent`, `ppcClassify`, `prettyNoteFunction` — 332 lines, one verbatim block | **every DONE arm** |
| done | same preamble + `prettyNoteFunctionDone` — 342 lines | **`prettyNoteFunction` entirely**, named as such in the Orientation |

**The split fell exactly on the two-phase seam, and four of the six findings
had to cross it to be true.** This is the single most important fact about the
half and it is developed in §5 as `D-OOF-1`. GPT-5 said so itself, in its own
opening line: *"I found one substantive arm defect, contingent only on whether
STAGE can route a shadowed-register target to `PPC_XSWAPREG`. The omitted STAGE
body prevents proving that reachability."* That is a correctly named gap, and
the finding was filed anyway.

### Dedup: verified, not trusted

The operator's claim was that none of the sixteen in-family titles touches
`ppcClassify`, the STAGE/DONE hooks, the dispatch-depth counter or
`PPC_XSWAPREG`. Verified two ways against the in-family file
`AUDIT_PP18-round-3-still-NO-FIX-WAVE-…_2026-08-29-r3.md`:

1. **Title level.** `grep '^### PP18RR3-'` returns fifteen CONFIRMED headings
   plus `PP18RR3-P1`; every one names `prettyVisual.c`, `prettyEquation.c`
   (`ppqParse`, `ppqShowRender`, `ppqFrame*`), `prettyLayout.c`, the browser,
   `prettyFormula.c`'s `ppfBuildEntry`, the FV/V test suite, or `DESIGN.md` §6.
   `PP18RR3-D1`–`D6` are the acceptance-triangle table, the parity direction,
   the metrics contract, solo-build assertions, ink-primitive hardening, and
   the mutation/green ratio.
2. **Body level, which is the part that could have failed silently.** Grepping
   lines 233–1487 (all of §3 CONFIRMED, §4 PLAUSIBLE, §5 design observations)
   for `ppcClassify|ppcDispatchDepth|PPC_XSWAPREG|prettyNoteFunction|
   PPC_CONSTCLS|PPC_RCLCLS|ppcCurrentRevalidate|PPC_IGNORE|ppcStage|
   ppcSupersedeCurrent|ppcScopeOk` returns **zero hits**. `prettyCapture.c`
   appears twice in that range, both inside `PP18RR3-P1`, citing
   `prettyCapture.c:410`'s byte cap and `:391-394`'s doc comment as context for
   a `prettyFormula.c` defect — not the classifier and not the hooks.

The whole-file hits are four and all live in §6 and §7: `prettyNoteFunctionDone`
at :1698 (cleared, "fail-closed paths that are actually closed"),
`ppcClassify`'s `PPC_DY` set at :1856 (cleared, "ruled, known, or below the
bar"), and two `0xffff` hits at :1746-1747 that are `ppqParse`'s leaf-arm
overflow sentinel, unrelated to the depth counter. **The dedup holds.** It is
also moot at the outcome level, since nothing survived to be numbered — but it
had to be checked before that was known, and two of the four refuters could not
check it at all (see §5 `D-OOF-4`).

### Reading budget, and what this half did not reach

- **No mutation ran in this half.** Every one of the six is a behavioural or
  reachability claim, and `CODE_AUDIT.md` reserves mutation as the proof for
  coverage claims. Four refuters instead constructed or exhausted call paths;
  one built a simulation (§2). Nothing about test *coverage* of
  `prettyCapture.c` was probed this half, and coverage is where round 3's
  in-family half found two of its worst items.
- **Neither packet contained the arena allocator, `ppcEmit`, `ppcDisplaced`,
  `ppcInvalidate` or `ppcEnsureKnown` bodies.** Both readers were told the
  contracts in prose. Two findings (`OOF-G1`, `OOF-S2`) turn entirely on those
  bodies. Nobody has yet sent an out-of-family reader the helper layer.
- **Not traced to a leaf:** `addItemToBuffer(ITM_EXIT1)` under `CM_NORMAL`,
  which sits upstream of §4's carry. The carry is stated with that contingency
  named rather than assumed away.
- **Not measured:** the history-granularity cost of the default rule on mode
  toggles (`DEG`, `SF`, `FIX`). It was checked structurally and cleared on the
  ruling; nobody counted how many formulas a mode-heavy session actually loses.

---

## 2. Mechanical results

**No new mechanical work in this half, by construction** — the tree is
byte-identical to the one the in-family half gated, and no refuter mutated it.
The governing results are that half's and are cited, not re-derived:
`./packages/pretty-print/build-test.sh --solo` **green** at `34ac6e97f`
(`PRETTY-PRINT GATE GREEN`, testSuite OK, 183–265 s across seven isolated
worktrees), **no new compiler warning**, `patch_churn_scan.py` standing churn
**1** (`[WS-ONLY]`, the `showString` wrap-reindent in
`010-solver__equation.c.patch`, catalogued and owned).

Two reminders this half re-earned:

- `./packages/forth-core/build-test.sh` refreshes only `packages/forth-core`
  and returns a meaningless green for a pretty-print change. The package's own
  gate is the governing one.
- **Tree state on exit.** All four refuters reported `git status --porcelain`
  empty on arrival and on exit. No foreign edit was present in any worktree this
  round, which is a change from round 2.

**One artifact this half produced:** `/tmp/oofg5/sim.c` — a line-for-line model
of the STAGE and DONE depth arithmetic with the saturation cap scaled
`0xFFFF → 3` so the path is reachable, tagging each stage with its owning
dispatch id and enumerating every dispatch tree for depth 1..7 × branching 1..3,
three waves per session. Result across all 21 configurations: **0 mispairs, and
the counter returns to 0 in every one**; at D=7 B=3, 363 saturation events and
6,318 correctly owner-matched applications after them. It is the disproof of
`OOF-G5` and it is worth keeping — it is the first executable model of the
pairing rule anyone has written.

---

## 3. CONFIRMED findings

**None. Zero of six survived.** No `PP18RR3-OOF-n` number is minted;
`grep -rn PP18RR3-OOF` over the repository returns nothing and this file does
not change that.

What zero means here, stated so it is not over-read:

- It does **not** mean `ppcClassify` and the two hooks are clean. It means
  these six specific claims are wrong, and that the two readers who produced
  them were each holding half of a two-phase mechanism (§5 `D-OOF-1`).
- It does **not** mean the readers were careless. Four of the six are correct
  about mechanism and wrong only about consequence — they read the code they
  were given accurately and mispredicted what the half they were not given
  does. That is the packet's defect, not the reader's.
- The one shape in this half that looks like a live defect was found by a
  **refuter's call-graph enumeration**, not by either reader, and it is in §4
  because it has not been through a refutation pass.

Ranked by what they would have cost the owner had they been true, the six were:
`OOF-S1` and `OOF-G1` (wrong formula filed, or formula lost — the top tier),
`OOF-S2` (open formula pointing at a stranger's tree), `OOF-G2` (shadow
silently stops tracking), `OOF-G4` (lost history granularity), `OOF-G5`
(shadow dead until reboot, at 65,535 nested dispatches). Each is disposed of in
§6a with its disproof.

---

## 4. PLAUSIBLE

One item, and it is labelled precisely because the label matters.

### `OOF-P1` (carry, **not refuted, not confirmed**) — a settings key dispatches `ITM_SQUARE` through the hooks while its own item is `PPC_IGNORE`, so the inner arm applies a squaring to a shadow the outer stack motion already invalidated

**Status.** Raised by the `OOF-G2` refuter as a side shape explicitly *"outside
this finding's claim"* and not carried to a verdict. **It has not been through
the refutation pass**, so it is not CONFIRMED and it is not a `PP18RR3-OOF`
number. It is here so it is not lost, and it is the first packet of round 4.

**Where.** `src/c47/config.c:487-503` (`_fnSetC47`), reached from
`src/c47/config.c:506` (`fnSetC47`, item 2627); the shadow side is
`packages/pretty-print/prettyCapture.c:703-707` (STAGE `PPC_MO`) and `:869-890`
(DONE `PPC_MO`).

**Reaching input, as far as it is established.** Item 2627 (`"47"`, the C47
defaults key) carries `US_UNCHANGED` — verified at `src/c47/items.c:4504` — so
the outer STAGE classifies `PPC_IGNORE` and returns at `prettyCapture.c:680`
having staged nothing. Inside, `_fnSetC47` calls `fnDrop(NOPARAM)` **twice
directly** — not through `runFunction`, so the hooks never see those two stack
motions — and then `runFunction(ITM_SQUARE)`, which does reach
`reallyRunFunction` and therefore both hooks (`packages/pretty-print/items.c:413`
and `:416`). `ITM_SQUARE` classifies `PPC_MO` (`prettyCapture.c:502-508`). The
inner STAGE stages at depth 2 and the inner DONE, pairing correctly with its own
STAGE, wraps `ppcSlot[0]` in an `OP1` squaring node. The generator-only stub
`items.c:1615` is not the linked definition; `config.c:506` is.

**Why it looks wrong.** The two unhooked drops moved register X out from under
`ppcSlot[0]` before the squaring, and `ppcEnsureKnown(0)` will not re-snapshot a
slot that is already non-`PPC_NIL`. The shadow then reads `(owner's old
formula)²` while X holds `(whatever survived two drops)²`. That is the class the
packet itself names as the one that matters most, quoted from `DESIGN.md` §3:
*"shadow slot k always holds an expression whose value equals register
REGISTER_X + k at quiescence"* — a slot that **survives** holding an expression
that no longer equals its register.

**What is not established, and would settle it.** Whether `ppcScopeOk()` is
still true at the inner dispatch after `fnKeyExit(0)`,
`addItemToBuffer(ITM_EXIT1)` and `fnClrMod(0)` have run — `calcMode` at that
point was not traced to a leaf — and whether `ppcSlot[0]` is non-`PPC_NIL` in a
realistic session. The probe is small and decisive: type `2 ENTER 3 +`, press
the `47` key, and dump `ppcSlot[0]`'s serialized form beside `REGISTER_X`. If
they disagree, this is a finding of the top tier and its bug class is **"an
unhooked direct call moves the stack inside a dispatch the classifier
ignored"** — a *class*, not an instance: `_fnSetC47` is simply the first member
anybody enumerated, and `src/c47/config.c:470-483` (`OPTION_DEVPROFILES`) has
the same shape with `fnDrop` + `fnSquare`.

**Class-level test if it confirms.** Enumerate every item-table handler that
calls `runFunction`/`reallyRunFunction` while its own item is `US_UNCHANGED`,
and assert for each that the shadow after the outer dispatch either matches the
registers or is invalidated — the same census shape `PP18RR3-9` used for browser
containment. The enumeration already exists in this round's `OOF-G2` refutation
(23 rows, reverse call graph over `src/c47` at 2 hops); it needs the scope
predicate evaluated per row rather than per handler.

---

## 5. Design observations

Shape, not defects.

**`D-OOF-1` — the packet split fell on the two-phase seam, and one reader's
cleared list is the literal refutation of the other reader's finding.** Gemini
saw STAGE and not DONE; GPT-5 saw DONE and not STAGE. `OOF-G1` claims
`PPC_CONSTCLS`/`PPC_RCLCLS` capture nothing pre-op — refuted by their DONE arms,
which Gemini could not see. `OOF-S1` claims `PPC_XSWAPREG` never repairs the
partner slot — refuted by the STAGE arm, which GPT-5 could not see. And the
proof that this is the packet's fault and not the readers' is in Gemini's own
*deliberately not flagged* list, third bullet: *"`PPC_XSWAPREG` with
`REGISTER_L` destroying the shadow tree: … While this drops the tree from
history, it degrades gracefully to a snapshot (`PPC_UNKNOWN`), satisfying the
binding invariant."* Gemini read, understood and correctly cleared the exact
`else if(xt == REGISTER_L)` branch whose absence GPT-5 was reporting as a defect
in the same round. Two families, opposite halves, and the answer sitting in one
reader's clearing list while the other filed the question as a finding.
**Consequence for the next round: a packet may be cut by file, by subsystem or
by lifetime, but not across a two-phase protocol.** If STAGE and DONE must be
split for length, each packet carries the *other* half's arms verbatim as an
appendix marked read-only.

**`D-OOF-2` — the default rule reads as a defect to every fresh reader, and
this is now the third family to say so.** Three of Gemini's five findings
(#1's framing, #3, #4) are one objection: a finite hand table with an
invalidating default is losing the owner's formula. The project has ruled this
twice — `DESIGN.md:197-207` (*"Default rule (BINDING): an unknown item that is
undo-enabled … invalidates the whole shadow"*) and `DESIGN-HISTORY.md:423`
(PP16), which describes an unenumerated operator *"simply falling to the default
rule and invalidating the shadow — truthful, but nothing shown"* and then treats
joining the enumeration as capability work. The rule is right, and the readers
keep tripping on it because **the rationale lives in `DESIGN.md` while the code
comment at `prettyCapture.c:571-577` states the rule without stating what
happens to the open formula.** One added clause at the classifier's default arm
— that `PPC_INVALIDATE` reaches a STAGE arm which *files* the formula first
(`:819-833`) — would have killed three findings before they were written.

**`D-OOF-3` — "STAGE captures, DONE applies" is not the file's actual rule, and
two readers got it wrong in opposite directions.** `PPC_CLX`/`PPC_DROP`,
`PPC_CLSTK` and `PPC_XSWAPREG` *write* at STAGE; `PPC_CONSTCLS`, `PPC_RCLCLS`
and `PPC_LASTX` do all their work at DONE off one common latch
(`ppcStage.lifted`, `:689`). The real predicate is two-part and unwritten:
**emit at STAGE when the register still holds the value and the dispatch cannot
retract it; defer to DONE when the dispatch can still error.** `PPC_RCLCLS` is
the case that proves it — `RCL` of a bad register errors, so a STAGE-side
`ppcDisplaced(0, true)` would file a formula that is still live, which is the
`AUDIT R1-5`/`R2-1` bug class. Gemini inferred "STAGE must capture everything"
and filed `OOF-G1`; GPT-5 inferred "DONE must repair everything" and filed
`OOF-S1`. Both inferences are reasonable from half the file. The two-part rule
belongs in the file header, next to the two-phase paragraph it qualifies.

**`D-OOF-4` — stale worktrees, seventh consecutive round.** All four refuters
spawned at `e21af8d28`, verified here as a real ancestor **111 commits behind**
`34ac6e97f`. All four detected it — three by an explicit
`git merge-base --is-ancestor` check that failed, one by `git log --oneline -1`
— and checked out the audited tip before their first read, which is the only
reason this half is usable. The guard in `audit-workflow.js` has now been
requested by six consecutive reports and is still absent. **A concrete cost
landed this round for the first time:** two refuters could not verify the
operator's dedup, because the in-family report is untracked at `34ac6e97f`, so
they flagged it unverified and proceeded — which is the honest move, and it
means the dedup was carried by this report rather than by the pass that needed
it.

**`D-OOF-5` — `x<> X` has no self-swap carve-out, where `STO X` was given one.**
`PPC_STO_NOP`'s DONE arm carries `AUDIT R3-2` explicitly: *"STO X writes X with
what X already holds, so it changes no value and must not disturb the shadow at
all."* `PPC_XSWAPREG`'s STAGE arm runs `ppcDisplaced(0, true)` unconditionally
and its DONE arm replaces slot 0 with a value leaf unconditionally, so if TAM
admits `X` as a target then `2 ENTER 3 + x<>X 4 ×` draws `5 × 4` where the
machine's own state changed not at all. Nothing lies — the formula is filed with
a true result and the leaf is truthful — so this is the bottom tier,
granularity only. It is recorded because it is the **same correction one ruling
already made next door**, and because the two arms are eleven lines apart. Not
minted as a finding: no refutation pass has seen it, and TAM's admissibility of
`X` as a target was not confirmed.

---

## 6. Deliberately not flagged

### 6a. Killed by the refutation pass — the six, worst-first by what they claimed

**`OOF-S1` (GPT-5) — `PPC_XSWAPREG`'s DONE arm repairs only slot 0, leaving the
partner stack slot and `ppcSlotL` describing values the swap moved.** *Refuted:
the repair exists and is unconditional, in the STAGE arm the packet withheld.*
`prettyCapture.c:733-751`, tagged `AUDIT R3-5` and dated 2026-08-27 by
`git blame` (commit `59ee0cd1ba`), does `ppcDisplaced(0, true)`, then for a
stack target `ppcDisplaced(k, true)` → `ppcFreeTree` → `ppcSlot[k] =
PPC_UNKNOWN`, and for `xt == REGISTER_L` the same on `ppcSlotL`. Both of the
finding's own reaching inputs (`x<> Z`, `x<> L`) are already handled, and the
degraded slot re-materialises truthfully from the live register via
`ppcEnsureKnown`. The pairing cannot be skipped: `items.c:413/416` are
back-to-back around the dispatch, `ppcStage.valid` is set before the STAGE
switch runs, and DONE refuses to act without a depth match, so no path reaches
the DONE arm without the STAGE arm having run. The refuter then went looking for
the escape hatch the finding invited — a live target the `xt > REGISTER_X &&
xt <= getStackTop()` test misses — and closed it: `fnSwapX`
(`src/c47/stack.c:147`) does no address resolution, TAM resolves indirection to
a concrete register before `runFunction` (`src/c47/ui/tam.c:883`, `:954`,
`:1098`), and every number outside the range has no shadow to repair. The one
uncovered live target is A–D under `SSIZE4`, which is `PP18RR2-6` and was fenced
by the packet.

**`OOF-G1` (Gemini) — `PPC_CONSTCLS`/`PPC_RCLCLS` are staged valid but have no
STAGE arm, so nothing captures the pre-op X they overwrite when stack lift is
disabled.** *Refuted: the premise is right and the consequence is not.* The one
pre-op fact those classes need is latched unconditionally for every non-IGNORE
class at `prettyCapture.c:689` (`ppcStage.lifted = getSystemFlag(FLAG_ASLIFT)`)
— per-class STAGE arms exist only for classes needing *more* than that. Both
DONE arms (`:967`, `:1017`) read the latch and, on the lift-disabled branch,
call `ppcDisplaced(0, false)`, which passes `resultReg = -1`; `ppcEmit`'s
snapshot block is `if(resultReg >= 0)` (`:417`) and its doc comment says so
(`:391-394`, *"pass -1 when the value has already left the stack … which stores
the formula without a result"*). The tree is therefore **filed
truthfully-but-resultless**, not "lost, left dangling, or filed using the
POST-op constant or recalled value". The supporting contrast is backwards too:
`PPC_LASTX`'s STAGE arm (`:721-726`), named by the finding as the correct
sibling, does not displace anything — it only materialises `ppcSlotL` from
`REGISTER_L`, and its DONE arm performs the identical `ppcDisplaced(0, false)`
+ free. And the DONE placement is required, not omitted: `RCL` can error, so a
STAGE-side `ppcDisplaced(0, true)` would file a formula that is still live when
the dispatch aborts — the `AUDIT R1-5`/`R2-1` class, documented in-file at
`:460-474`.

**`OOF-S2` (GPT-5) — `ppcCurrent` is a bare arena index, so a freed formula root
whose index is reused passes `ppcCurrentRevalidate`'s liveness test as an
unrelated tree.** *Refuted: closed at the free site, and closed since the
original design commit.* `ppcFreeTree` ends every node with `if(ppcCurrent == n)
{ ppcCurrent = PPC_NIL; }` (`:156-158`), and because it recurses over both
children first, the clear fires for the root **and every interior node** of the
freed subtree. Both arms the finding names free through that function *before*
they allocate (`PPC_XSWAPREG` DONE `:1066-1075`; `PPC_DROPY` DONE `:1076-1087`),
so by the time the LIFO free list hands the index straight back — which it does,
exactly as the finding predicts — `ppcCurrent` is already `PPC_NIL` and
`ppcCurrentRevalidate` returns at its own first line (`:619`) without executing
the comparison. Free-list order is irrelevant. The invariant was checked rather
than assumed: the four `PPN_FREE` sites are `ppcInit` (`:98`), the double-free
guard (`:148`, read-only), `ppcFreeTree` (`:153`, guarded), and `ppcDeepCopy`'s
failure path (`:175`), which frees a node `ppcAlloc` returned two lines earlier
and therefore by induction cannot be `ppcCurrent`; no other file frees arena
nodes. `git log -S "if(ppcCurrent == n)"` returns one commit, `db495d984` (PP3),
so this is original design implementing `DESIGN.md:251-262` §4 rule 1, not a
later audit patch. The round-2 observation `PP18RR2-D2` stays what it was
recorded as — a smell about *instance* identity under ENTER's deliberate dup —
and does not promote to a live index-reuse defect.

**`OOF-G2` (Gemini) — a nested `PPC_IGNORE` dispatch clears `ppcStage.valid` and
returns, discarding the outer dispatch's staged transform.** *Refuted on
reachability, by exhaustion rather than by assertion.* The order the finding
describes is real: `:678` sets `valid = false` downstream of the scope guard,
before the `PPC_IGNORE` return at `:680`. What does not exist is the pairing it
needs — an inner `PPC_IGNORE` dispatch reaching `:678` in scope while an outer
dispatch has staged a non-IGNORE class. A reverse call graph over all of
`src/c47` (2 hops to `runFunction`/`reallyRunFunction`), intersected with the
item table and filtered to handlers whose `US_STATUS` makes them stageable, gave
23 candidate rows; every one is excluded by one of three things: the outer item
is `US_UNCHANGED` and staged nothing (`fnKeyDotD`, `fnSetC47`, `fnCFGsettings`,
`fnSolveVar`, `fnIntVar`, the TVM vars, `fnXSWAP`); the inner item is not
`PPC_IGNORE`, so its own DONE at its own depth applies its transform
(`fnReset` → `ITM_VERS`, `fnEdit` → `ITM_EQ_EDI`, `showSoftmenu` →
`reallyRunFunction(ITM_STO)`); or the nesting happens in a `calcMode`/flag state
`ppcScopeOk()` already rejects (`CM_AIM`, `CM_MIM`, `CM_PEM`, `FLAG_SOLVING`,
`PGM_RUNNING`). The only other `PPC_IGNORE` inner in the tree,
`bufferize.c:1397`'s `reallyRunFunction(ITM_EXIT1)` under `case ITM_CONSTpi`, was
chased through all 31 `addItemToNimBuffer` call sites: none passes `ITM_CONSTpi`
from inside a dispatch. Cross-package composition was checked in the same pass —
forth-core's interpreter brackets its own arbitrary-item dispatch with
`programRunStop = PGM_RUNNING`
(`packages/forth-core/files/forth_inner.c:427-431`), so `ppcScopeOk()` is false
there by construction. Nested *in-scope* dispatch does occur (`fnKeyDotD` is
live), so the guard is **not** vacuous — but no pairing puts a staged outer
transform at risk. The one shape this enumeration did turn up is §4's carry,
which is the inverse failure (inner applies, outer never staged) and outside the
finding's claim.

**`OOF-G4` (Gemini, merged with its #3) — eight ordinary math operations are
absent from `ppcClassify`'s switch, so each destroys the shadow formula through
the default rule.** *Refuted: mechanism confirmed, harm not produced, and the
behaviour is the documented BINDING rule.* All eight (`ITM_IP`, `ITM_FP`,
`ITM_sinh`, `ITM_cosh`, `ITM_SIGN`, `ITM_DELTAPC`, `ITM_PC`, `ITM_ROUND`) are
genuinely absent from `:496-590`, all are `US_ENABLED` in `src/c47/items.c`
(spot-verified at rows 3452 and 3724), and the default arm at `:586` does return
`PPC_INVALIDATE` for them. Following the finding's own input — `2 ENTER 3 +`
then `IP` — the open formula is **not** destroyed: the STAGE `PPC_INVALIDATE`
arm (`:819-833`, tagged `AUDIT R2-1`) calls `ppcSupersedeCurrent()`, which emits
the formula from the register that still holds its value, so `2 + 3 = 5` is
filed with its true result and stays recallable from the browser; by the time
DONE runs `ppcInvalidate(true)`, `ppcCurrent` is already `PPC_NIL` and `ppcEmit`
refuses an already-`PPA_EMITTED` node anyway. Nothing wrong is computed, stored
or shown. The suite already pins this with this exact item:
`prettyTest.c:1108` (`ppcTestOp(ITM_IP); // unmodelled US_ENABLED -> invalidate`,
T22) and `:1252` (T27, `AUDIT PP18R2-1`), whose failure strings are *"the filed
formula has no result and can never be recalled"* and *"the filed result is not
the value the formula had"* — and `ppcTestOp` runs the real hooks, so `ITM_IP`
is the suite's canonical unmodelled-`US_ENABLED` case. Invalidating rather than
ignoring is also the only correct direction for an unmodelled stack-mover:
ignoring would leave the shadow describing registers the op overwrote. The
second face (mode toggles `DEG`/`GRAD`/`SF`/`CF`, `src/c47/items.c:1928-1936`)
was checked separately and costs history granularity only, on the same filing
path. **This is a coverage/feature-scope request, not a correctness finding** —
and per `DESIGN-HISTORY.md:423` the project already treats joining the
enumeration as capability work, priced per operator.

**`OOF-G5` (Gemini) — the dispatch-depth counter's saturation asymmetry was
widened rather than removed, so a saturating STAGE still desynchronises every
later STAGE/DONE pair.** *Refuted twice over: the comment was misread, and the
consequence is false even granting 65,535 nested dispatches.* The `AUDIT R2-4`
comment (`:653-659`) and the commit that wrote it (`e84e9a1db`) state a two-part
remedy — *"a wider counter, and a depth we cannot represent invalidates rather
than guesses"* — and both shipped; the `else` branch **is** the surviving
asymmetry, ruled deliberate. The comment claims the *harm* it names was removed,
not that the arithmetic was made symmetric. And the harm is removed, because
pairing is **relative**: STAGE stores the post-increment counter (`:687`), DONE
compares the pre-decrement counter (`:840-845`), so a uniform offset cancels, and
DONE's `if(ppcDispatchDepth > 0)` clamp absorbs the deficit during unwind so the
counter returns to 0 rather than persisting "until reboot". `/tmp/oofg5/sim.c`
(§2) enumerated it: **0 mispairs in 21 configurations, `C_final = 0` in every
one**, thousands of correct owner-matched applications *after* saturation events
in the same session. Corroborating: `ppcInvalidate(false)` is the file's ordinary
recovery path (9 call sites, including DONE's error arm at `:854`), not a
terminal state. The residual is not a latent defect; it is a no-op. **Dedup note
recorded by that refuter:** the only in-family hit for this subject is
`PP18R3-7`'s tag-collision table naming `R2-4` as a reused *tag*, not the
counter.

### 6b. What the finders cleared — verified against the tree, not transcribed

**Gemini's three.**

1. *BIGOP decline arms skipping `ppcSupersedeCurrent()`.* Cleared because the
   packet's KNOWN list fenced it. Correct, and worth stating plainly: the fenced
   item is **real and still open** — the decline arms rewrite `ppcStage.cls`
   after the switch has branched. Gemini re-derived a live defect independently
   and stopped at the fence, which is the fence working as designed.
2. *`PPC_STO_NOP`/`PPC_SWAP`/`PPC_RUP`/`PPC_RDOWN` missing from STAGE's switch.*
   Right conclusion, wrong reasoning, and the difference matters. Gemini cleared
   them on "these do not destructively overwrite `REGISTER_X`". `STO Y`
   destructively overwrites register **Y**, and the reason its DONE arm is a
   problem is precisely that `ppcDisplaced(k, true)` at `:1007` reads a register
   the store has already overwritten — which is the fenced known finding
   *"`STO`-to-a-stack-register files a post-store result snapshot"*. The clear
   is correct only because the defect is already filed under another number.
   `SWAP`/`RUP`/`RDOWN` are genuinely safe: their DONE arms (`:911-934`) permute
   unique arena indices and duplicate nothing.
3. *`PPC_XSWAPREG` with `REGISTER_L` degrading `ppcSlotL` to `PPC_UNKNOWN`.*
   Correct, and it is the refutation of `OOF-S1`. See `D-OOF-1`.

**GPT-5's nine.**

4. *Error exit.* Verified: `ppcDispatchDepth` is decremented at `:841-843`,
   ahead of every return; `ppcStage.valid = false` at `:846` precedes the
   `lastErrorCode` test at `:847`; BIGOP's detached limit trees are freed
   (`:849-851`); then `ppcInvalidate(false)`. No half-application path exists.
   This is also the answer to the classic stale-shadow shape — DONE runs ~175
   lines before `reallyRunFunction`'s `undo()`.
5. *Depth mismatch tolerated deliberately.* Verified, and it is the same
   relative-pairing property that kills `OOF-G5`.
6. *DY/MO/RCLARITH/BIGOP root ownership.* Agreed for `PPC_DY`/`PPC_MO` on a read
   of the arms; `PPC_RCLARITH`'s failure-path node leak is the fenced known item
   and GPT-5 excluded it separately and correctly. Not re-derived line by line
   here — the in-family halves of rounds 2 and 3 own those arms.
7. *ENTER and RCL copying before creating two live references.* Verified:
   `:902-904` (`ppcDeepCopy(ppcSlot[1])` after the shift) and `:1019-1025`
   (copy-then-lift, source read before the shift). Both degrade to
   `PPC_UNKNOWN` on exhaustion.
8. *Rotations.* Verified as in item 2 above.
9. *DROPY's shift-then-top-deep-copy.* Verified; the arena-reuse caveat it
   attached is `OOF-S2` and is refuted.
10. *Allocation failures.* Verified as a uniform idiom: `(d == PPC_NIL) ?
    PPC_UNKNOWN : d` at every alloc site in the DONE switch.
11. *`ppcDeepCopy(PPC_UNKNOWN)` — the named gap.* **Resolved here, in GPT-5's
    favour:** `prettyCapture.c:161-164` returns `n` unchanged for both
    `PPC_NIL` and `PPC_UNKNOWN`, so the sentinel propagates and every caller's
    `== PPC_NIL` re-test still behaves. No crash, no guess needed. This is the
    model of a well-named gap and it cost the round nothing.
12. *FILL, BIGOP decline handling, stack-size changes, the listed bypass/error
    issues.* Correctly deferred to the fenced set.

### 6c. Cleared while writing this half

- **`ppcFreeTree` cannot dangle `ppcCurrent` from the `PPC_XSWAPREG` STAGE arm**
  even though — unlike its `PPC_STO_NOP` twin at `:1004-1012` — that arm does
  not call `ppcCurrentRevalidate()` afterwards. The recursive `ppcCurrent` clear
  at `:156-158` covers it. Checked because an asymmetry between two adjacent
  arms is exactly the shape that has produced findings twice in this package; it
  is not one here.
- **`ppcEmit`'s refusal of non-formula roots** (`:401`, `kind != PPN_OP1/OP2/
  BIGOP → return`) means a slot holding a bare value leaf cannot be filed as a
  formula by any displacement path. That is what makes `ppcDisplaced` safe to
  call unconditionally, which several arms do.
- **The classifier's `fnConstant` fallback** (`:570-572`) catches every constant
  by function pointer rather than by item id, so `PPC_CONSTCLS` is not the
  one-item family the explicit `case ITM_CONSTpi` makes it look like. Checked
  because `OOF-G4`'s "incomplete family" objection would otherwise apply here
  with more force than where it was aimed.
- **`ppcScopeOk()`'s five-way predicate** (`:633`) re-read against both packets'
  Orientation paraphrase and found to match it: not `PGM_RUNNING`, not
  `FLAG_SOLVING`, not `FLAG_INTING`, `calcMode ∈ {CM_NORMAL, CM_NIM}`. No reader
  was misled by the paraphrase.

**What I would leave alone if the goal were correct code rather than a passing
audit.** All six refuted items, without qualification — `OOF-G4` most
emphatically, since "enumerate eight more operators" is a feature request priced
per operator, and the current behaviour files the owner's formula correctly
before invalidating. `D-OOF-5` (`x<> X`) too: it is eleven lines from a ruling
that says the opposite, which makes it tempting, and it costs the owner nothing
but richness on a gesture nobody makes. The only item here worth spending code
on is §4's carry, and only after the probe that decides whether it is real.

---

## 7. Verdict

**Ship the classifier and the hooks as they stand at `34ac6e97f`** — but this
half is weak evidence for that, and the weakness is worth naming precisely.

Six findings from two families over ~330 lines each, and zero survived. That is
not a clean bill: four of the six were refuted by code the reader was not shown,
so what this half actually measured is the packet, not the file. The parts of
`prettyCapture.c` that have now genuinely been read out-of-family are
`ppcClassify`'s bet, the depth counter, the scope guard, and the two hook
bodies — and all four held under a deliberate attack, which is worth something.
The helper layer beneath them — `ppcEmit`, `ppcDisplaced`, `ppcInvalidate`,
`ppcEnsureKnown`, the allocator — has never left the family, and two of this
round's six findings died on facts that live there.

**Where it breaks first.** Not in the classifier's table and not in the depth
arithmetic. It breaks at the boundary where upstream code moves the stack
*without* going through `reallyRunFunction` while an item the classifier ignored
is executing — §4's carry, `_fnSetC47`'s two bare `fnDrop(NOPARAM)` calls. Every
hook-based mirror has this exposure by construction, the fenced known list
already contains three instances of it (SHOW-mode recall, the register browser,
`SST`), and the carry would be the fourth found by a different door. The
classifier is only as good as the assumption that item dispatch is the single
funnel, and that assumption is upstream's to break at any time.

---

## 8. Round and exit state

**Round: PP18 round 3 of the restarted series, out-of-family half.** Subject
`pretty-print/stage-pp17..34ac6e97f`, tip `34ac6e97f`, no fix wave — the same
tree restarted round 1, round 2 (both halves) and this round's in-family half
all read.

### Readers

| reader | packet → reply | `MODEL:` line (verbatim) | raised | survived refutation |
|---|---|---|---|---|
| gemini | `/tmp/pkt-r3-classify.md` → `/tmp/pkt-r3-classify.gemini.reply.md` | `MODEL: Gemini 3.1 Pro` | 5 filed, 4 carried (#3 merged into `OOF-G4`) | **0** — `OOF-G1` refuted by the DONE arms it was not shown; `OOF-G2` refuted by call-graph exhaustion (23 rows); `OOF-G4` refuted on the ruling plus the T22/T27 pins; `OOF-G5` refuted by relative pairing plus `/tmp/oofg5/sim.c` |
| sol | `/tmp/pkt-r3-done.md` → `/tmp/pkt-r3-done.sol.reply.md` | `MODEL: GPT-5` | 1 filed + 1 unpromoted hazard, 2 carried | **0** — `OOF-S1` refuted by the STAGE arm it was not shown (`AUDIT R3-5`, `59ee0cd1ba`); `OOF-S2` refuted by `ppcFreeTree`'s recursive `ppcCurrent` clear, original to PP3 |

Every finding was refuted independently under one assigned lens (reachability,
correctness, intent), default REFUTED. No refuter saw another's verdict. No
refuter mutated the tree; all four reported it clean on entry and on exit.

**Counts.** Six raised, **zero survived (0%)**, **zero new numbers**. Round 3
across both halves: **fifteen CONFIRMED** (`PP18RR3-1`–`-15`), one PLAUSIBLE
(`PP18RR3-P1`), six design observations (`PP18RR3-D1`–`D6`), plus this half's
one unrefuted carry (`OOF-P1`) and five process/shape observations
(`D-OOF-1`–`D-OOF-5`). `grep -rn PP18RR3-OOF` over the repository returns
nothing, and this file does not change that.

**Dedup: verified, holds.** Method and result in §1. Zero hits for any
out-of-family subject symbol anywhere in the in-family report's §3–§5.

**Three-family status: SATISFIED.** In-family dimensions, Gemini 3.1 Pro and
GPT-5 have all read this subject at this tip. Round 3 is the third consecutive
PP18 round to satisfy the 2026-08-29 ruling.

**Exit criterion: NOT MET.** The round is complete but not clean — the
in-family half's fifteen CONFIRMED findings reset the count on their own,
regardless of this half's zero. The criterion's two consecutive clean rounds,
at least one out-of-family, stands where round 2 left it.

### Process items

1. **Never split a packet across a two-phase protocol.** `D-OOF-1`. This half
   spent two out-of-family reader budgets and returned nothing, and the direct
   cause is that STAGE and DONE went to different readers with each half hidden
   from the other. Whichever way the next packet is cut, the counterpart arms
   ride along as a read-only appendix.
2. **Stale worktrees, seventh consecutive round; the `git merge-base
   --is-ancestor` guard in `audit-workflow.js` is still absent** after six
   reports asked for it. It cost something real this time (`D-OOF-4`): two
   refuters could not verify the dedup because the in-family report is untracked
   at the audited tip.
3. **Gemini's identity check is still one-sided.** 0-byte `.err`; identity rests
   on the reply's own `MODEL:` line, while Sol's is corroborated by the codex
   header. Unchanged from round 2's §8 item 2.
4. **Two comments would have prevented three findings.** The classifier's
   default arm should say that `PPC_INVALIDATE` *files* the open formula at
   STAGE before tearing down (`D-OOF-2`), and the file header's two-phase
   paragraph should state the real STAGE-vs-DONE predicate (`D-OOF-3`). Both are
   comment-budget-compatible: they state invariants, not narration.

### Round 4's out-of-family packet, from this half

In priority order:

1. **The helper layer** — `ppcAlloc`/`ppcFreeTree`/`ppcDeepCopy`/`ppcEmit`/
   `ppcDisplaced`/`ppcInvalidate`/`ppcEnsureKnown` as one packet. It has never
   left the family, it is where two of this round's six findings died, and it is
   small enough to send whole, which is what the last two rounds' failures argue
   for.
2. **`OOF-P1`'s class** — the census of `US_UNCHANGED` handlers that dispatch
   through `runFunction` while moving the stack by direct calls. Send the
   23-row enumeration with the packet; the question for the reader is the scope
   predicate per row, not the call graph.
