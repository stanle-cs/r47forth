# Audit — PP18 restarted round 8, COMPLETION HALF: the ten out-of-family findings

Subject: the ten findings raised by round 8's two out-of-family readers — Sol 1–7
and Gemini 1–3 — at `7fdda3129` (`f4421da32..7fdda3129`, three commits), each put
through the standard three lenses (reachability, correctness, intent) with
mutation authority and worktree isolation.

Why this file exists. Round 8's verification cap was spent on the 24 in-family
findings, so the ten out-of-family items went into that report's §4 as *recorded,
not verified*. They are verified now. **Nothing in the in-family half is
re-derived here.** The ten CONFIRMED in-family findings (`PP18RR8-1`..`-10`), the
three PLAUSIBLE items, both packet replies, the coverage union and the mechanical
half stay where they are, in

    AUDIT_PP18-restarted-round-8-the-round-7-fix-wave-…_2026-08-30-r8.md

This half carries verdicts, and only the verdicts change the round's accounting.

The result in one line: **two of ten survive**, both from Sol, both coverage
defects in the pin net this wave built, neither a wrong picture on the owner's
screen. Eight are refuted. Two of the round-8 report's own pre-notes on these
items were wrong, in the same direction, and §1 corrects them.

> **Filename note.** The dispatched subject string for this pass is 565 bytes and
> cannot be a filename (255-byte limit). This file truncates after the refutation
> clause, as the round-8 report did after its axis clause. The full subject is
> recorded in §1.

---

## 1. Subject and coverage

### What was audited

The ten out-of-family findings, nothing else. The code under them is the same tip
the round-8 report audits: `7fdda3129` on `pretty-print/stage-pp18`, three
commits (`fd080dee9` comment-only, `4361b80e9` the four repairs, `7fdda3129`
docs). Every verifier read at that tip after correcting its own worktree; see §2.

Full dispatched subject, for the record, since the filename could not carry it:

> PP18 restarted round 8, COMPLETION HALF — the ten out-of-family findings
> (Sol 1-7, Gemini 1-3) through the standard three-lens refutation. The
> in-family half, both packet replies and the ten CONFIRMED in-family findings
> (PP18RR8-1..10) are already recorded in the round-8 report
> (`AUDIT_PP18-restarted-round-8-*_2026-08-30-r8.md`); this pass exists because
> the 24-finding verification cap left the out-of-family findings unverified
> there. Verdicts from this pass complete round 8's accounting; do not re-derive
> or re-report the in-family findings.

### Out-of-family accounting

Both reply files were read in full for this pass. Both are present and complete;
neither is empty, so **no `'pending'` or `'none'` banner is owed** and no timeout
or overwrite is being hidden.

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r8/packet-sol-pinnet.md` (37,244 B) | `/tmp/pp18-r8/packet-sol-pinnet.sol.reply.md` (15,963 B) | `MODEL: GPT-5` | **7** |
| gemini / gemini | `/tmp/pp18-r8/packet-gemini-fixwave.md` (38,492 B) | `/tmp/pp18-r8/packet-gemini-fixwave.gemini.reply.md` (5,863 B) | `MODEL: Gemini 3.1 Pro (High)` | **3** |

Identity, checked again this pass because the skill requires it every pass. Sol's
reply self-reports the family name `GPT-5`; the dispatch transcript beside it
(`packet-sol-pinnet.sol.reply.md.err`, 69,699 B, the `codex` CLI log) carries
`model: gpt-5.6-sol`, `provider: openai`, `sandbox: read-only`. The self-report
is coarser than the dispatched id, not a substitution. Gemini's `.err` is 0 bytes
against a complete 65-line reply, which is the normal shape for `agy`.

Counting, unchanged from the round-8 report and re-checked here. Sol's reply
raises seven numbered findings, then five pin-vacuity verdicts (the helper, the
T25 call sites, T27, T28, EQ36) and one helper-state-contamination verdict; those
six analyse pins the seven findings already name, so the count is 7. §6c states
what each of the six was worth after verification. Gemini's reply has four
numbered items; item 4 is a clearance of the buffer repair, so the count is 3.

### What this pass verified, and how

One refutation agent per finding, ten agents, each in its own worktree, each
instructed to kill the finding it was given and to default to refuted when
uncertain. Lenses were assigned per finding: reachability (construct the input or
the finding dies), correctness (grant the path, ask whether the consequence is
what the code does), intent (find the ruling that already decided it). Mutation
authority was granted and used on five of the ten; every mutation was propagated
with `tools/pkg_patch_refresh.py`, **verified present in the built shadow**
(`build.sim/custom_pkg_shadow/…`) rather than only in the working area, run, and
reverted inside the same step. Every agent finished with an empty
`git status --porcelain` at `7fdda3129`.

### What this pass did not reach

- **The in-family half.** Out of scope by instruction and not re-derived: the ten
  CONFIRMED findings, the three PLAUSIBLE items and §5's design observations
  stand as written.
- **The eleventh unverified item.** Round-8 §4 carried one in-family item past
  the cap — the stale comment at `packages/pretty-print/solver/equation.c:1814`
  claiming the `PARSER_*` macros bind to `mvarBuffer`'s name. It is still
  unverified. This pass covered the ten out-of-family items only, so round 8's
  unverified list closes from eleven to one.
- **One composition that the two confirmed findings jointly point at.** The
  measured-geometry probe written for Sol 1 and the `ctxFont` mutation written
  for Sol 2 were built in different worktrees and never run against each other.
  `PP18RR8-OOF-1` names this as its single unexecuted inference.
- **The device.** Nothing ran on hardware. Every picture claim here is
  `ppMeasure` arithmetic or a signature printed by an executed probe in the
  simulator battery.

### Two corrections to the round-8 record

The round-8 report's §4 pre-notes were written to save the next round a
derivation. Two of ten were wrong, both in the same direction — both said the
in-family read had already answered the item, and a mutation said otherwise.

| round-8 text | status after verification |
|---|---|
| §4 oof-2: *"the property is unpinnable in a signature oracle, which is §5's D1 and not a defect in this helper"* | **Withdrawn.** Unpinnable in a *signature* oracle, pinnable in a *measuring* one. The finding stands as `PP18RR8-OOF-1`. |
| §4 oof-6: *"this stands as a coverage gap with no demonstrated producer error"* | **Half stands.** The gap is now demonstrated by mutation and is `PP18RR8-OOF-2`; the finding's operator-token half is refuted. |
| §7: *"Neither reader produced a new confirmed finding"*, and §8's two `0 — none went through the pass` rows | **Withdrawn.** Sol produced two. |
| §7: *"the three items there with a plausible mechanism (Sol 3, Sol 7, Gemini 2) are each smaller than the smallest confirmed finding above"* | All three are refuted. The two that survived were not on that list. |

The pre-note mechanism is not the problem; treating a pre-note as a verdict is.
§5, D-B.

---

## 2. Mechanical results

The round's mechanical half — gate, churn scan, `design-audit.sh` coverage — is
in the round-8 report §2 and is not re-run here; this pass audits findings, not
the tree. What follows is what this pass itself executed.

**Baselines.** Five verifiers established the package gate GREEN at `7fdda3129`
before mutating: `./packages/pretty-print/build-test.sh --solo` (or the
equivalent `tools/pkg_patch_refresh.py` + `make pkg_build PKG=packages/pretty-print`),
testSuite `184.22s`, `172.53s`, `172.56s`, `175.02s` and `190.26s`,
`13023 TESTS PASSED SUCCESSFULLY / 0 TESTS FAILED`.

**Mutations run by this pass.**

| probe | result | measured line |
|---|---|---|
| `ctxFont = PP_FONT_TINY` at the head of `ppfBuildEntry` (`prettyFormula.c:557`) | **GREEN**, 175.02s | every filed row paints tiny beside a standard live row; no pin moves |
| filed leaf arms forward `ctxFont` in place of `childFont` (`prettyFormula.c:637/654/676`) | GREEN, 174.68s | control; inert in production, so it proves nothing alone |
| serializer files `item + 1` on both name leaves (`prettyCapture.c:359-361`, `:368-370`) | **GREEN**, 0 failures | a filed `π` draws as `e`, `R01` as `R02` |
| serializer files `ITM_ADD` where the node holds `ITM_MULT` (`PPN_OP2` arm) | RED, exit 1, one row | `T27 filed MULT keeps the signed bracket (live '[3 · P(-5)]', filed '[3 + -5]')` |
| filed-vs-live **geometry** probe over a big operator with literal limits, at `(STANDARD, TINY)` | GREEN, 172.56s | filed geometry == live geometry |
| the same probe with `ppSetFontDeep` deleted from `ppfBigop` (`prettyFormula.c:392-393`) | RED, exit 1 | `AUDIT-PROBE R8 filed 58/30/18 != live 56/25/13` |
| `resultRun = ppfRun("42", …)` (`prettyFormula.c:598`) | RED, four named + three cascade | `FV3 … (expected '[[2 + 3] = 5]', actual '[[2 + 3] = 42]')`, T22, T23b, T27 |
| widen `ppfTextIsAtom`'s glyph accept window to `0x0000..0xffff` (`prettyFormula.c:129`) | RED, two | `T25 a squared scientific value draws its two exponents as one`; `EQ4d` |
| EQ36 re-run unmutated at the audited tip (Gemini 1's exact input) | GREEN, 4/4 Ok, 190.26s | the parse declines on `1+1+1+1+1+` `492×'7'` `abc/2` |

Three of those nine are the load-bearing evidence for a verdict; the rest are
controls and sensitivity checks, and they are listed because a mutation with no
control is an assertion.

**Process items, all repeats.**

- **Every worktree in this pass spawned at `e21af8d28`**, 143 commits behind the
  audited tip, with `7fdda3129` not an ancestor. Ten more instances of the defect
  the round-8 report recorded for its eight in-family verifiers. Each agent ran
  `git checkout 7fdda3129` before its first read and said so.
- **No foreign mutation in any worktree.** Every agent reported an empty
  `git status --porcelain` on arrival. Round 5's contamination has not recurred.
- **The wrong-gate trap fired once more, and was caught once more.** One verifier
  ran `./packages/forth-core/build-test.sh` first, got `EXIT=0`, `13023 passed`
  and zero `prettyPrint test FAIL` rows, recognised it against `CLAUDE.md`, and
  re-ran the package gate. Four instances in round 8 now — three in-family, one
  here. A verifier that stopped at the first green would have returned a false
  REFUTED.
- One verifier saw a sibling `make pkg_build` running while it worked, resolved
  it with `readlink /proc/<pid>/cwd` to a different worktree, and recorded that
  no cross-tree contamination was possible. That is the right reflex and it is
  cheaper than the doubt it removes.

---

## 3. CONFIRMED findings

Two, both from Sol, both coverage. Ranked by what the hole costs the owner: the
first leaves every filed row's size unpinned, the second leaves two leaf kinds'
identity unpinned.

### `PP18RR8-OOF-1` — the equality oracle is blind to fonts, and `ctxFont` is the uncovered axis (Sol 2)

**Where.** `packages/pretty-print/prettyTest.c:2571` and `:2581` (the helper's two
build calls), with `ppfTestSigNode` (`:2455-2549`) as the reason no other row in
the suite closes the hole.

**What breaks.** Nothing today. What cannot be caught: a context-font regression
on either surface the helper compares. `ppfTestSigNode` emits box-kind letters
(`F(|)`, `S(|)`, `R(;)`, `U(|)`, `A()`, `P()`, `I()`, `B(||)`, `[ ]`) and run
text with ASCII spaces stripped. It never reads `nd->font`. The helper is the
suite's only filed-versus-live oracle, so a tree decoded at the wrong context
font produces a byte-identical signature and passes.

**The reaching input, as executed.** `ctxFont = PP_FONT_TINY;` at the head of
`ppfBuildEntry` (`prettyFormula.c:557`), verified present in the compiled shadow
at `custom_pkg_shadow/prettyFormula.c:558`. **Gate GREEN, 175.02s.** In that
build every PHIST row and every browser row paints tiny next to a standard live
row, at the pager's own rung 0 — a size change over the whole filed surface, on
the pager's primary rung — and not one pin moves: not the B4/B7 decode pins, not
the B8 pixel probe on the big-operator strokes, not the FV5/FV6 pager ink checks.

**Correction to the finding, recorded because the record must be right.** The
pager does **not** decode at `(STANDARD, TINY)`. `ppfBuildRow`
(`prettyFormula.c:728-745`) sets `cf = (rung == 0) ? PP_FONT_STANDARD :
PP_FONT_TINY`, so rung 0 is `(STANDARD, STANDARD)` — byte-identical to the
helper's pair — and rung 1 immediately re-fonts the whole tree with
`ppSetFontDeep(root, PP_FONT_TINY)`. `prettyValue.c:809-820`'s `tRungs` has the
same shape, and `browsers/prettyBrowser.c:54,71` reach the builders only through
`ppfBuildRow`. Those are the production callers. The finding's named examples die
with its premise: exponent, index and big-operator-limit fonts come from
`childFont` inside the shared `ppfBuildOp1`/`ppfBuildOp2`/`ppfBigop`
(`prettyFormula.c:506-529` vs `:637/654/676`), which both surfaces call, and at
every production rung `childFont` either equals `ctxFont` or is erased by the
whole-tree re-font. The control mutation that broke `childFont` on the filed path
only was GREEN because it is inert in production, not because a pin missed it.

What survives is the finding's stated consequence, on the axis the finding did
not name: `ctxFont` is uncovered, completely.

**Contract violated.** `packages/pretty-print/prettyFormula.c:85`:

> shared by the tree walker and the token decoder so **both paths typeset
> identically**.

Typeset — not "have the same node shape". The helper's own header claims only the
narrower thing it does (`prettyTest.c:2564-2566`: *"File the live formula with
CLSTK, decode the filed entry, and require the same layout signature on both
surfaces"*), and the distance between those two sentences is the defect: the
suite leans on this pin for the wide claim, and round 7 and round 8 both cite it
that way.

**Bug class.** *Oracle alphabet narrower than the property it is cited for.* The
same shape as the catalogued **save-test narrower than the save** (pretty-print
r1), moved from a guard onto an oracle: what the pin can express is stricter than
what the design sentence it defends asserts, and everything in the gap changes
silently. Kin to **message/body mismatch**, except that here the mismatch is
between the pin's body and a *design* sentence rather than its own PASS text.

**Class-level test.** A sibling of `ppfTestFiledMatchesLive` that measures instead
of spelling: build both roots at the production font pair, `ppMeasure(root, 0)`
each, and compare `width`/`ascent`/`descent` from `ppNodeAt(root)`. The machinery
is already in the file (`prettyTest.c:78`, `:99`, `:2102`). Drive it at both
rungs — with and without the rung-1 `ppSetFontDeep` — so the re-font is inside
the pinned property rather than outside it. That probe was **written and executed
in this pass** as the sensitivity control for Sol 1: GREEN at the tip, and RED
(`filed 58/30/18 != live 56/25/13`) with `ppfBigop`'s restyle deleted, so it is
not vacuous. It has **not** been run against the `ctxFont` mutation — different
verifier, different worktree. It discriminates by construction, since every
metric is read from `ppMet[nd->fontId]` at measure time and the mutation changes
the filed root's font, but that is an inference and it is the one thing in this
finding that the next round should execute rather than reason about.

**Second demonstration, already on record.** The in-family half measured that
deleting the rung-1 `ppSetFontDeep(root, PP_FONT_TINY)` (`prettyFormula.c:745`)
leaves the whole gate GREEN. Same hole, other direction: the font axis has no pin
anywhere in the suite, on either surface.

### `PP18RR8-OOF-2` — no equality row reaches a name leaf, so a filing-side item-number error survives (Sol 6)

**Where.** `packages/pretty-print/prettyFormula.c:610-627`, the `PPT_TKC` /
`PPT_TKR` decode arm, against `packages/pretty-print/prettyCapture.c:355-371`,
the two serializer arms. The oracle that should cover them is
`ppfTestFiledMatchesLive`, whose six call sites (`prettyTest.c:1491`, `:1555`,
`:1585`, `:1614`, `:1627`, `:1635`) are all power, MULT and SUB rows over
literals and values. None reaches a name leaf.

**The reaching input for the class.** `3 STO 05`, `2`, `RCL 05`, `×` — T17's own
keystrokes (`prettyTest.c:1193-1201`) — mints `PPN_RCL` through `ppcRclLeaf`
(`prettyCapture.c:239-248`, any register 0..99). The constants menu mints
`PPN_CONST` through `PPC_CONSTCLS` (`prettyCapture.c:962-975`). Both are live
owner paths. Neither reaches a filed/live comparison: T17 asserts
`ppcTestExpectSig`, which prints from the arena node (`prettyTest.c:831-837`),
and its companion `ppcTestExpectHist` only counts entries.

**The mutation, as executed.** File `item + 1` in both name arms — the low byte of
the two-byte item at `prettyCapture.c:360` and `:369`. Verified in the generated
mirror (`files/prettyCapture.c:360,369`) and in the compiled shadow
(`custom_pkg_shadow/prettyCapture.c:360-361,369-370`), 773 ninja targets rebuilt.
**Gate GREEN, exit 0, zero failures.** A filed `π` draws as `e`, a filed `R01` as
`R02`, and nothing looks.

**Scope correction — the finding's other half is refuted.** A second mutation
filing `ITM_ADD` where the node holds `ITM_MULT` turned the gate **RED** on
exactly one row:

    prettyPrint test FAIL: T27 filed MULT keeps the signed bracket
                           (live '[3 · P(-5)]', filed '[3 + -5]')

while the capture-signature rows, which read the tree rather than the stream,
stayed green. That discriminator proves T27 (`prettyTest.c:1618-1637`) is real
coverage of operator item transport. Only the name leaves are uncovered, and the
finding should be scoped to them before anybody acts on it.

**Contract violated.** `prettyFormula.c:85` again ("both paths typeset
identically"), and the project's own convention for a hole that genuinely cannot
be driven: `design-docs/pretty-print/TESTING.md` MUT-76 writes such a hole down
as *"UNFALSIFIABLE from the harness — documented gap, not a coverage hole"*. No
such record exists for the name leaves, and none would be honest — the helper
already exists and the gap is two call sites wide.

**Rulings searched and not found.** `DESIGN-HISTORY.md:1079-1080` rules on the
item→infix mapping (*"one constructor pair `ppfCombine1/2` serves both the live
tree and the token stream, so the two paths cannot drift typographically"*) —
that is the operator half, and it says nothing about the item number's transport.
The round-7 report at `:890-892` rules only that TKC/TKR push `PPF_PREC_ATOM`.
`DESIGN.md:291` (*"serialized … by pure byte copies"*) states a mechanism, not a
coverage decision. Searched in full: `DESIGN.md` §5–§6, `TESTING.md` including
the whole MUT-1..123 table, `DESIGN-HISTORY.md` at `:188`, `:246`, `:1068-1091`,
`:1541-1584`, and every `AUDIT_PP18` report from rounds 2 through 7.

**Bug class.** **Wrapper-only coverage** (catalogued, D3-5) in its oracle form:
the equality helper's call-site list is narrower than the decoder's arm list, and
the two arms nobody drives are exactly the ones whose payload is an index into a
foreign table (`indexOfItems[item].itemCatalogName`, and `R%02u` of a register
number). Every reachable arm needs its own row, for the same reason every
reachable entry point needs its own pin.

**Class-level test.** Two more `ppfTestFiledMatchesLive` rows: one over an RCL
leaf (T17's keystrokes), one over a constants-menu leaf. That also closes round
7's open note `PP18RR7-7` (*"`PPN_CONST` has no layout row at all"*). The class is
enumerable and the count check is the token list itself: one equality row per
`PPT_*` arm the decoder implements, asserted against the arm list rather than
against a hand-kept comment.

---

## 4. PLAUSIBLE findings

**None new.** All ten items resolved to CONFIRMED or REFUTED; no finding in this
pass survived refutation without a constructible reaching input. The round's
PLAUSIBLE set is unchanged and remains the three in the round-8 report §4 (the
`ppqExpr` lead-sign sentinel, the `ppfTestSigNode` length reserve, the
`ppqFrameIntegral` double degradation).

One thing this pass adds to that set, and it is worth recording because it is
agreement rather than a new claim. Sol reached round-8 PLAUSIBLE-2 independently,
from the other side. Its pin-vacuity section says of `ppfTestSigNode`:

> A long signature can also become non-discriminating: `ppfTestSigNode` silently
> returns once `strlen(out) + 24 >= cap`, so two sufficiently large trees sharing
> a prefix may compare equal despite a difference in the unrecorded suffix.

That is the same mechanism the in-family reader found by instrumenting the guard
and measuring the high-water at 191 of 192. Two families, no contact, one
mechanism. It stays PLAUSIBLE because neither reader could produce a fixture that
reaches it, and the in-family measurement shows the guard never fires suite-wide.
**What would settle it** is unchanged: a run of 160 bytes or more as a nested
`PPT_TKV` operand, so that two or more closers are pending above it.

---

## 5. Design observations

Additive to the round-8 report's §5, which is not repeated. These are shapes this
pass produced, not defects.

**D-A — the oracle pins one axis of four.** A picture on this device has node
shape, run text, font, and geometry. `ppfTestSigNode` records the first two.
Spacing is deliberately unrecordable in it, because ASCII space is the signature
grammar's own HBOX delimiter (`prettyTest.c:2381-2383`) — a run that kept its
spaces would make one run `"a b"` print identically to an HBOX of `"a"` and
`"b"`. Font and geometry are recordable and are simply not recorded. The round-8
in-family §5 D1 states the orthogonal half — an equality oracle cannot see a
defect the two surfaces *share*. Together the two halves bound the pin exactly:
it sees an unshared *structural* defect, and nothing else. Every sentence in the
design that cites it for more than that is over-claiming, and `PP18RR8-OOF-1` is
what that costs.

**D-B — a pre-note reads like a verdict.** Round 8's §4 recorded, for each
unverified out-of-family item, "what the in-family read already establishes so
the next round does not re-derive it". The intent is right and the mechanism is
cheap. Two of ten were wrong, both by reasoning from build-time structure to an
observability conclusion: *no builder branches on font* (true) therefore *no pin
could catch a font change* (false — measurement happens downstream of the font
byte, so a measuring pin catches it). The lesson is not to stop writing
pre-notes; it is that a pre-note without a mutation is a hypothesis, and it
should be spelled as one. In this series the mutation lever is what separates the
two: prose analysis of these ten items got two verdicts wrong, and every
mutation that ran agreed with its own reading.

**D-C — the packet did not just cost Sol a finding, it scrambled its ranking.**
Sol's #1 died on a function body the packet omitted (`ppfBigop`'s two
`ppSetFontDeep` calls). Its #4 died on a gate the packet omitted
(`prettyNoteNimText`'s `n > PPC_LIT_CAPACITY` refusal). Its two survivors were
ranked #2 and #6. The reader's own confidence field was a better guide to
outcome than its rank: it marked #6 *"medium as a coverage gap, low as a present
defect"* and #6 is confirmed as a coverage gap; it marked #1 *"high"* and #1 is
dead. Round 8's D8 already ruled that a packet quoting two callers of a shared
function must quote the shared function. This is the second data point, and it
adds the corollary: when a reader names its own escape hatch (*"if it
clones/restyles its inputs, that undocumented behavior would be the only
caveat"*), the escape is a packet defect until the packet closes it.

**D-D — the dominant out-of-family failure mode this round was granted mechanism,
wrong consequence.** Four of the eight refutations (Sol 1, Sol 5, Gemini 1, and
Sol 3's grouping half) trace a real divergence at the reported line and then do
not follow the call one or two frames further, where it converges:
`ppfBigop`'s restyle, four older result pins, `ppqFactor:565`'s latch test,
`ppfBuildRow`'s re-font. Not one of the four is a misreading of the line it
names. A packet that carried each reported function's callers and callees one
frame out would have killed all four before dispatch — which is a cheaper edit to
the packet builder than a lens is to the refutation pass.

**D-E — a monotone latch read at every call site, enforced by nothing.**
Gemini 1 is refuted, but the shape under it is worth a line. `c.failed` is
assigned `false` exactly once (`prettyEquation.c:743`) and never cleared;
`ppqPrimary` is `static` with two callers, and both test `c->failed` on the line
immediately after the call. That is what makes the latch sound. Nothing forces a
third caller to do the same, and `ppqPrimary` does its own consumption of a
latched `PP_NONE` (`:551-557`) which a new caller would reasonably read as "the
probe said no". This is the classic D7 shape — two places that must agree with
nothing forcing them — and round 7's `PP18RR7-3` chose the producer-latch /
consumer-check split deliberately. It is not a defect; it is a contract carried
by convention, and it costs one comment to make explicit.

**D-F — the two survivors are both pins, and both were settled only by
mutation.** Neither could be decided by reading, and the in-family prose reading
of the same two got them wrong (D-B). That is the strongest argument in this
round's material for keeping mutation authority on the refutation pass rather
than on the finders: the finders produced ten claims from reading, and reading
resolved eight of them; the two that reading could not resolve are the two that
were real.

---

## 6. Deliberately not flagged

### 6a. Killed by the refutation pass — the eight, ordered by what each claimed

**Gemini 1 — `ppqPrimary` consumes `ppqNumber`'s `PP_NONE` without checking
`c->failed`, so a latched allocation failure still parses garbage and reports
success.** The mechanism is real and granted: after `ppqNumber` latches and
returns `PP_NONE`, line 555 does run `ppqName`, which can succeed and can return
a live node. The consequence is not what the code does. `ppqFactor`'s first
statement after the call is `if(c->failed || n == PP_NONE) { return PP_NONE; }`
(`prettyEquation.c:565`), the `PPQ_RAD` call site tests the same at `:538`, and
`ppqTerm` (`:621`), `ppqExpr` (`:696`, `:711`) and `ppqParse` (`:762`, `:773`)
all gate on the latch, so `*rootOut` is never written after a latched failure and
both production callers (`prettyTryEquation:803`, `ppqShowRender:938`) fall back
to upstream's linear line. Executed, not inferred: EQ36 drives Gemini's exact
input — `1+1+1+1+1+`, 492 `'7'` digits (20+492+1 = 513 > `PP_TEXT_BYTES` 512, so
the run allocation fails), then the short name `abc` which genuinely does fit and
does make `ppqName` succeed — and the gate is GREEN with the parse declining.
What is left is a style point about where the latch is read, which round 7 already
ruled (`DESIGN-HISTORY.md:1564-1566`, `PP18RR7-3`, class *one sentinel, two
meanings*). Recorded as shape in §5, D-E.

**Sol 1 — big-operator limits build in `ctxFont` on the filed surface and
`childFont` on the live one, so the pager's limits render large.** The divergence
is real in the *inputs*: the filed path's `PPT_TKL` arms push limit runs built at
`ctxFont` while the live `PPN_BIGOP` arm builds both children at `childFont`. The
path never reaches the consequence, because both surfaces funnel into the same
`ppfBigop`, whose first act is `ppSetFontDeep(fromN, childFont)` and
`ppSetFontDeep(toN, childFont)` (`prettyFormula.c:392-393`) before either limit
is appended, measured or painted. The restyle is total: `ppNewBox`/`ppNewRun`
record kind, text and `fontId` and compute no geometry; every font-derived metric
is read from `ppMet[nd->fontId]` inside `ppMeasure`/`ppPaint`; and
`grep -n "PP_FONT_" prettyFormula.c` returns only the four `ppfBuildRow` lines,
so no builder branches on font at build time. Proved by mutation in both
directions: the finding's own input measures byte-identical across the two
surfaces at the tip, and RED (`filed 58/30/18 != live 56/25/13` — taller and
wider, exactly the "limits render large" symptom) with `:392-393` deleted. Sol
named this escape itself and ranked the finding first anyway; the escape holds.
See §5, D-C.

**Sol 5 — `withResult=false` excludes the result half of every filed picture, so
the `PPT_TKRES` value, its presence and its spelling have no equality coverage.**
The `false` is forced by the comparison: the helper's left side is
`ppfBuildCurrent`, which has no `" = result"` tail (`ppfBuildCurrent` at
`prettyFormula.c:536-551` never builds an equals run; the only producer is
`ppfRun(" = ", ctxFont)` at `:694`, inside `ppfBuildEntry`'s `withResult` arm).
Asking for the result there makes every row unequal by construction. The result
half is covered by four pins the reader never opened — FV3 (decode at
`prettyTest.c:2652`, assertion at `:2656-2657`: `sprintf(expect, "[[2 %s 3] = 5]",
nADD)` then `ppfTestExpect`, a `strcmp` over the whole signature, so value,
presence and spelling fall to one comparison), T22 (`:1324`), T23b (`:1395`),
T27 (`:1828`) — and the
finding's stated consequence is falsified by mutation: `resultRun = ppfRun("42",
…)` turns the gate RED with all four naming it plus three cascade rows. The
defensible residue is far weaker than the finding: the filed-equals-live helper
alone does not exercise the result tail, and that is by design.

**Gemini 3 — `ppfRunPrec`'s first-character fast exit stakes ATOM for text
`ppfTextIsAtom` would call a term (leading `'+'`, letters).** Refuted on
reachability, enumerated over the whole repo rather than the packet. `ppfRunPrec`
has exactly one caller (`prettyEquation.c:585`, inside `ppqFactor`); the node it
judges is `ppqScopeOperand(ppqPrimary(...))`, and `ppqScopeOperand` only wraps a
`PP_BIGOP`; `ppqPrimary`'s other arms all return boxes, which exit at the earlier
`kind != PP_RUN` test (`prettyFormula.c:143`) and never reach line 147. Only two
producers hand it a `PP_RUN`: `ppqNumber`, whose acceptor sets `any` only on
`0-9`, `.` and `,`, so its slice always starts inside the accept set and is fully
judged by `ppfTextIsAtom`; and `ppqName`, whose slice always starts with an ASCII
letter. A name taking the fast exit is the ruled design
(`prettyInternal.h:128`: *"for NUMERIC leaf text only. A name is always an
atom"*), and round 7 already proved the gate load-bearing by mutation — removing
it draws `x²` as `(x)²` and turns EQ23 RED. Leading `'+'` has no producer at all
on this path. The finding's own reaching-input field concedes both spellings.

**Sol 3 — the signature writer strips spaces, so grouping and spacing divergence
is invisible to every pin.** The strip is the grammar's documented rule
(`prettyTest.c:2381-2383`) and its reason is the HBOX delimiter (D-A). The
grouping half is factually wrong: the digit-group separator this firmware emits
is never ASCII `0x20`. The default is `STD_SPACE_PUNCTUATION` = `"\xa0\x08"`
(`src/c47/items.c:2711`, `src/c47/fonts.h:479`, default set at
`src/c47/config.c:37-40`), and every choice in `menu_GAP_L`
(`packages/pretty-print/softmenus.c:1024-1026`) is either a two-byte `0xa0xx`
glyph or a visible single byte — none is `0x20`. So `1␣234` and `1234` already
produce different signatures and a grouping regression reddens the pins. The
`" = "` half cannot reach the invariant it cites, because
`ppfTestFiledMatchesLive` compares with `withResult=false` and the equals run is a
compile-time literal on one code path, not something a serializer emits. The one
true residue — editing that literal's padding would not redden FV3 or B4 — is a
one-literal cosmetic blind spot, and it is exactly what the strip rule buys the
grammar.

**Sol 4 — the decoder accepts a 31-byte `PPT_TKL` while the live literal leaf
caps at 30, so a boundary literal can differ by one digit across surfaces.** No
input reaches it. The single producer (`ppcSerializeNode`'s `PPN_LIT` arm,
`prettyCapture.c:297-322`) writes `min(nd->aux,15) + min(cont->aux,15)`, a hard
maximum of 30; `ppcHist` is a static buffer written only by `ppcEmit`, never
loaded from a file, so no foreign encoding can inject a 31. Above the boundary
there is no literal leaf at all: `prettyNoteNimText` (`prettyCapture.c:1111-1128`)
clears `ppcNimTextValid` when `n > PPC_LIT_CAPACITY` (30, `prettyInternal.h:78`)
and the commit falls back to a `PPN_VAL`. At exactly 30, the live clamp
(`prettyFormula.c:456-467`) and the filed copy produce the same 30 characters.
The decoder's `text[32]` is one byte of slack over a 30-byte ceiling. Sol marked
this UNREACHED itself and named the two traces to run; both close it.

**Sol 7 — the predicate's spelling classes are sparsely pinned: no equality row
drives comma radix, grouped, angular or complex spellings.** The finding treats
four spellings as four decisions, but `ppfTextIsAtom` has no per-class code — one
ASCII accept/reject (`prettyFormula.c:119-120`) and one glyph accept/reject
(`:129-130`). Angular: `STD_DEGREE` is `"\x80\xb0"`, outside the `0xa000..0xa00f`
window, so it takes the same `:130` reject as `STD_SUB_10` — the branch T25's
scientific rows already type, one of them with a filed-vs-live equality row at
`prettyTest.c:1614`; widening that window turns the gate RED on exactly those
rows (`T25 a squared scientific value draws its two exponents as one`, `EQ4d`).
Complex: `complex34ToDisplayString` leaves the imaginary part at
`displayString[100]` and never joins it, so the predicate sees the real part
alone, a plain numeral — no reaching input. Comma radix: `','` sits in the same
accept disjunction as `'.'` with no dependence on the radix setting, so a
comma-radix row decides what a dot-radix row already decides. Only *grouped
values* is genuinely untyped, and it is already filed as round 7's `PP18RR7-6`
against the same predicate at the same tip, with its class-level test written and
its deferral recorded. A byproduct worth its own line but not this finding's: the
formula leaf for a complex value draws only its real part, which is adjacent to
the in-family `PP18RR8-5`.

**Gemini 2 — T28's `continue` skips `ITM_CLSTK`, leaving the failed iteration's
operands on the stack.** The finding mistakes `ITM_CLSTK` for stack hygiene. In
this suite it is the **filing** step: `grep` returns six call sites in
`prettyTest.c` (`:1260`, `:1377`, `:1445`, `:1515`, `:1656`, `:2578`), two of them
commented *"displacing the formula files it"*, against 76 `ppcTestReset()` blocks
— so the suite leaves the register stack dirty between all 76 blocks by design,
and T28's bailed row leaks nothing the other 75 do not. The residue principle the
finding cites is `PP18RR7-9` (`DESIGN-HISTORY.md:1567`), whose subject is
`lastIntegerBase`; `ppcTestReset` (`prettyTest.c:861-878`) restores exactly what
its two comments scope it to (NIM typing residue, entry MODE) and then calls
`prettyReset()`, which wipes the whole shadow — arena, slots, current, history
counters. Both exits from the `continue` pass through it. The shape the finding
objects to is the suite's established one: `ppfTestFiledMatchesLive` itself
returns before its own `CLSTK` when the live build fails, at a dozen call sites,
and that is the pin `PP18RR7-1` landed. Real but tiny, and only reachable after
`ppTestFailInt` has already reddened the run.

### 6b. What the finders cleared — verified against the tree, not transcribed

Sol's seven and Gemini's two, each checked rather than accepted.

1. **`ppfTextIsAtom`'s classification itself (Sol).** The symmetry claim holds:
   the live `PPN_LIT`/`PPN_VAL` arms (`prettyFormula.c:468`, `:494`) and the filed
   `PPT_TKL`/`PPT_TKV` arms (`:605`) call the same predicate on the same text.
   Scope the clearance to that, though — the in-family half separately confirmed
   `PP18RR8-6` against the predicate's *content* (every short-integer spelling is
   rejected, so every integer value leaf is bracketed on stock defaults). Sol
   cleared the symmetry correctly and did not clear the predicate.
2. **Constant and register names staked as atoms on both surfaces (Sol).** True,
   and by two different mechanisms: live by the function-entry default
   `*outPrec = PPF_PREC_ATOM` (`prettyFormula.c:450`) with the `PPN_CONST` /
   `PPN_RCL` arms (`:497-500`) never overriding it, filed by the explicit
   `stackPrec[sp++] = PPF_PREC_ATOM` (`:626`). Two places that agree because
   nobody has yet written the arm that disagrees — which is why
   `PP18RR8-OOF-2` is an item-number gap and not also a precedence gap.
3. **`PPT_TKRES` precedence (Sol, and Gemini independently).** Verified:
   `resultRun` is a local (`prettyFormula.c:565`), assigned at `:598`, and
   appended as the last child of the root box at `:700` under
   `if(withResult && resultRun != PP_NONE)`. It is never handed to an operator
   builder, so it needs no stake. Two families cleared the same non-defect from
   two different packets.
4. **Ordinary correctly serialized unary and binary operators (Sol).** Cleared
   for the reason Sol gave (shared `ppfBuildOp1`/`ppfBuildOp2`), and this pass can
   now say more than Sol could: the `ITM_MULT`→`ITM_ADD` mutation proves T27
   actually catches an operator item-number error.
5. **Allocation failures inside the production builders and decoder (Sol).**
   Correctly excluded per the packet's instruction. Not a blind spot in the
   record: the in-family half carries two findings of exactly that shape as
   PLAUSIBLE (`ppqExpr`'s lead-sign sentinel, `ppqFrameIntegral`'s double
   degradation).
6. **`ppfBigop`'s body, not speculated about (Sol).** The right call under the
   packet's constraints, and the one place where the reader's discipline and the
   packet's omission collided: it named the restyle as its own escape and ranked
   the finding first regardless. §5, D-C.
7. **T26 treated as decode-only, not an equality pin (Sol).** Correct, and it is
   also the pin that supplies the missing half of Sol 4 — capture withholds the
   formula at 31 characters, which is why no 31-byte `PPT_TKL` is filed.
8. **`aimBuffer` vs `nimBuffer` in the reset helper (Gemini).** Verified at
   `prettyTest.c:870-872`: the comment names `fn42Alpha`, which asserts an empty
   ALPHA buffer, and NIM is reset through `nimNumberPart = NP_EMPTY` on the next
   line. Gemini considered flagging a typo and cleared it for the right reason.
9. **The buffer repair, `PP18RR7-2` (Gemini item 4).** Cleared with both length
   comparisons multiplied out and the deciding gate named (200 in both cases,
   160-byte worst case passing). This is independent confirmation from outside
   the family of a repair the in-family half re-derived and mutation-tested
   (`ppfValBuf` 200→96 goes RED on base 2 only). Counted as a clearance, not a
   finding, which is why Gemini's raised count is 3.

### 6c. Sol's six pin analyses — what each was worth after verification

Sol's five pin-vacuity verdicts and its helper-contamination verdict are not
counted as findings, and this is why each was cleared or absorbed.

- **The helper (fonts, spaces, geometry, `withResult`, the length guard, the
  reach guard).** Split by verification: the font half is `PP18RR8-OOF-1`; the
  spaces half is refuted (6a, Sol 3); the `withResult` half is refuted (6a,
  Sol 5); the length-guard half is the round-8 PLAUSIBLE-2 arrived at
  independently (§4); the geometry half is the class-level test
  `PP18RR8-OOF-1` prescribes. Nothing in it is a separate claim.
- **The T25 call sites — "equality, not picture equality".** True and structural:
  a pin that compares two producers cannot see a defect they share. That is the
  in-family §5 D1, and the in-family half found two members of the class from the
  other direction (`PP18RR8-5`, `PP18RR8-6`). Not a defect in T25.
- **T27 — "can pass while 'keeps the signed bracket' is false".** Same structure,
  and true as stated: both surfaces rendering `3·-5` would compare equal. Cleared
  as a member of the class above rather than a defect in T27, and this pass
  measured what T27 *does* catch (an operator item-number error, RED).
- **T28 — "verifies only successful decoding".** Already CONFIRMED in-family as
  `PP18RR8-9`, with the missing width reach-guard named. Not re-reported.
- **EQ36 — "causally vacuous; it passes for any parse failure".** Already
  CONFIRMED in-family as `PP18RR8-8`, with the margin measured at one byte
  (492 digits pass, 491 go silent). Sol reached it from the pin's causality and
  the in-family reader reached it from the fixture arithmetic — independent
  agreement on the round's most fragile pin, and the strongest single result of
  the out-of-family pass after the two confirmations.
- **Helper state contamination.** The reachable half is Gemini 2, refuted in 6a.
  Sol scoped its own claim correctly (*"does not identify a specific later
  existing row"*) and this pass could not identify one either: `ppcTestReset` runs
  at the head of every block and calls `prettyReset()`, which clears the entire
  shadow including the history ring.

---

## 7. Verdict

**The ship decision does not move.** The round-8 verdict stands unchanged: do not
ship this tip, for `PP18RR8-1` and `PP18RR8-2`. Both findings added here are
coverage holes in the test suite. Neither puts a wrong picture on the owner's
screen today, and neither has a demonstrated producer error behind it.

**What does move is the content of the next fix wave.** Round 8's fix list
(`PP18RR8-1`, `-2`, `-3`, `-5`, `-8`, plus a ruling on `-6`) is all behaviour.
Landing it against the current pin net means landing it against an oracle that
cannot see a context-font regression on either surface it compares, and that has
no row at all for two of the decoder's leaf arms. This series' own history says
what happens next: each round's findings come mostly from the previous round's
fixes, and the fixes are validated by the pins that exist. `PP18RR8-OOF-1`'s
class-level test is about twenty lines and reuses machinery already in the file.

**Where it breaks first.** Nowhere new for the owner. Both findings are escapes,
not defects; their whole cost is paid by the next wave, in the form of a
regression that ships green.

**What I would leave alone if the goal were correct code rather than a clean
audit.**

- **`PP18RR8-OOF-2`, entirely.** Two test rows, no demonstrated producer error,
  and the mutation that exposes it is one I wrote. It should ride along with
  whatever next touches `prettyTest.c` — ideally with `PP18RR7-7`, which it
  closes — and it should not pull a commit of its own.
- **The general half of `PP18RR8-OOF-1`.** Making `ppfTestSigNode` font-aware is
  more machinery than the property is worth, and it would still not catch the
  geometry defects that a measuring probe catches for free. Do the cheap half
  only: the measured-geometry sibling, which already exists in executed form from
  this pass's Sol-1 control.
- **Everything in 6a.** Eight refuted items, and I would not spend a line of code
  on any of them. Two are worth one comment each — `ppfBigop`'s restyle
  (`prettyFormula.c:392-393`) is the sole reason Sol 1's divergence is inert and
  nothing says so at the site, and `ppqPrimary`'s consumption of a latched
  `PP_NONE` (`:551-557`) is safe only because both callers check on the next line
  (§5, D-E). Both are safe today by a fact the next edit can remove without
  noticing, which is the same reason the round-8 report gave for commenting its
  PLAUSIBLE items.

**On the out-of-family pass, restated now that it has been verified.** It paid.
Two confirmed findings out of ten, both against the pin net the wave had just
built and neither reachable from the in-family axis, plus one independent arrival
at the round's most fragile pin (EQ36) and one at its PLAUSIBLE-2. The round-8
report's "neither reader produced a new confirmed finding" was a statement about
an unverified list, and it was wrong — which is itself the argument for never
letting a verification cap end a round.

**On the readers.** Sol's ranking was inverted by its packet and its confidence
field was the better signal (§5, D-C). Gemini answered the four questions it was
given, cleared one repair with a full derivation, and produced three findings
whose mechanisms are all real and whose consequences all die one or two frames
down the call chain. Neither reader made an error a repository would not have
corrected; both made errors a better packet would have prevented.

---

## 8. Round and exit state

**Round 8 of the restarted series, completion half.** This file adds no readers.
It adds the refutation pass the round could not afford in one sitting.

| family | packet | reply | `MODEL:` line, verbatim | raised | survived refutation |
|---|---|---|---|---|---|
| in-family (8 dimensions) | — | — | — | 24 | **13** — 10 CONFIRMED (`PP18RR8-1`..`-10`), 3 PLAUSIBLE *(from the round-8 report, unchanged)* |
| sol / gpt | `/tmp/pp18-r8/packet-sol-pinnet.md` | `/tmp/pp18-r8/packet-sol-pinnet.sol.reply.md` | `MODEL: GPT-5` | 7 | **2** — Sol 2 → `PP18RR8-OOF-1`; Sol 6 → `PP18RR8-OOF-2`, narrowed to name leaves. 5 refuted (Sol 1, 3, 4, 5, 7) |
| gemini / gemini | `/tmp/pp18-r8/packet-gemini-fixwave.md` | `/tmp/pp18-r8/packet-gemini-fixwave.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 3 | **0** — all three refuted (Gemini 1, 2, 3) |

Both out-of-family replies are present, non-empty and complete, so no `'pending'`
or `'none'` banner is owed here or in §1. The round is out of family on the
actual subject.

**Round 8 totals, restated.** 34 findings raised, **12 CONFIRMED** (10 in-family
plus two here), 3 PLAUSIBLE, 19 refuted. The confirmed count since the restart now
reads 16, 17, 15, 14, 7, 7, 9, **12**.

**Exit criterion: NOT MET**, and this half strengthens the reason rather than
weakening it. The criterion asks for two consecutive rounds with no new CONFIRMED
finding, at least one of them out of family on the actual subject. Round 8 is out
of family and has twelve confirmed findings — and two of the twelve came from
outside the family, which is the case the criterion exists to catch. Clean rounds
to date: zero.

**Unverified items outstanding after this half: one.** The stale comment at
`packages/pretty-print/solver/equation.c:1814`, in-family, carried past the
round-8 cap. Round 8's unverified list closes from eleven to one.

**What the next round needs, amended from the round-8 list.**

1. **The fix wave**, unchanged: `PP18RR8-1`, `-2`, `-3`, `-5`, `-8`, and a ruling
   on `-6`. Add `PP18RR8-OOF-1`'s measured-geometry probe to it, because the wave
   will be validated by whatever pins exist when it lands.
2. **Execute the one composition this pass did not.** Run the measured-geometry
   probe against the `ctxFont = PP_FONT_TINY` mutation in a single worktree.
   It is one build and it converts `PP18RR8-OOF-1`'s prescribed test from
   reasoned to demonstrated.
3. **Refute or rule the last unverified item** (`equation.c:1814`).
4. **The packet rule from round 8's D8, now with a second data point:** a packet
   that quotes two callers of a shared function must quote that function, and a
   packet that lets a reader name its own escape hatch has already failed. One
   omission cost round 8 its highest-ranked out-of-family finding; a second put
   Sol 4 into UNREACHED.
5. **The process items, both repeats.** Ten more worktrees spawned 143 commits
   off the audited tip; a fourth verifier this round ran the forth-core gate and
   got a false green. Neither was fixed by writing it down last round.
