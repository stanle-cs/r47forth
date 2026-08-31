# Audit — PP18 restarted round 7: the round-6 fix wave and the ship pass, at `f4421da32`

Subject: `aafd38f7d..f4421da32` on `pretty-print/stage-pp18`, ten commits.
Axis, as round 6 asked for it: **the producer side of the leaf** —
`ppfFromCaptureNode`, `ppfFormatStaged` and `ppfBuildEntry` in
`prettyFormula.c`, `ppvAstToNodes` in `prettyVisual.c`, `ppqFactor` in
`prettyEquation.c` — plus the `PP_NONE`-versus-`c->failed` latch semantics.

The wave's central repair was correct in shape. It stopped `ppfPowBase`
accumulating spellings, named the class (`ppfTextIsAtom`), and moved the
question to the producer that formatted the text. It then converted **three of
the five numeric leaf producers** and deleted the consumer-side scan that had
covered all of them. `ppfBuildEntry`, the history token decoder that draws
every filed PHIST row and every browser row below row 0, still assigns
`PPF_PREC_ATOM` to both of its leaf arms. One formula therefore draws two
different pictures on one screen: `(-5)²` in the live row, `-5²` beside its own
`= 25` in the row under it. For the scientific-form spelling that is a
regression the wave itself introduced.

---

## 1. Subject and coverage

### The ten commits

| commit | role in the wave |
|---|---|
| `7ddff2c5f` | round 6, out of family: the ASCII exponent an owner types (`1 EEX 50`), fixed inside `ppfPowBase` by reading the base run's text |
| `45d07a4ea` | skill: a packet that gives one side's establishing fact must give the other's |
| `b27394db6` | **the in-family repair wave**: the 200-byte buffer for a builder that starts writing at 256; `ppfTextIsAtom`; `ppfRunPrec`; `ppfPowBase` loses both text scans; `ppqFactor` latches |
| `48d35ded7` | three bug classes round 6 paid for |
| `973089e45` | V27's caveat was a neighbour's decline; new `V-FULL` for the arm with no pin |
| `fff40b9be` | the last whitespace-only hunk goes; the churn scan is clean |
| `1a98ac816` | the `screen.c` hook row says five hunks; the V27 record is corrected |
| `1b95fc9bb` | docs: an ASAN build finds an upstream out-of-bounds read on our path |
| `d5dfff4b3` | the paint-arm count says which side of `V-FULL` it was taken on |
| `f4421da32` | docs: how to run the suite under ASAN, and the two results that are not defects |

Diffstat over the range: 20 files, 2411 insertions, 141 deletions. Behaviour
changes live in exactly four package sources (`prettyFormula.c`,
`prettyInternal.h`, `prettyEquation.c`, `prettyVisual.c`) plus `prettyTest.c`;
the only upstream override touched is `solver/equation.c`, and that change is
indentation inside the package's own guard.

### Coverage, union across the eight in-family dimensions

Read in full by at least one reader: `prettyFormula.c` (862 lines),
`prettyEquation.c` (1037), `prettyInternal.h` (194); the whole
`aafd38f7d..f4421da32` diff and every commit message;
`design-docs/pretty-print/DESIGN.md` (829 lines), the DESIGN-HISTORY entries
this wave added, `TESTING.md`'s mutation catalogue, the round-6 report, the
2026-08-27 upstream-minimality review, and `bug-classes.md`.

Read in the relevant span: `prettyVisual.c` (`ppvLiteral`, `ppvIntern`,
`ppvLeaf`, `ppvPush`/`ppvPop`/`ppvBody`, `ppvInventName`, `ppvAstToNodes`,
both paint arms, `fnPrettyVisual`); `prettyCapture.c` (leaf constructors,
`prettyNoteNimText`, `ppcSerializeNode`, the classifier arms for CLX/CLSTK);
`prettyLayout.c` (`ppNewRun`, `ppTextAt`, `ppMeasure` guards);
`prettyValue.c` (`prettyTryRegisterLine`'s gate, `fnPrettyShow`);
`browsers/prettyBrowser.c` (`pbPaint`'s row loop and the placeholder arm);
`solver/equation.c` (the package block and the `showEquation` hook);
`prettyTest.c` (T22b–T26, the T25 power block, `ppcTestNoteLabel`,
`ppfTestPowersScoped`, V19/V20/V27/V-CHROME/V-FULL, EQ4c/EQ4d/EQ23).

Upstream read for reachability, not review: `screen.c` `_refreshRegisterLine`
(3244–5505) and the `CM_PRETTY_BROWSER` dispatch; `display.c`
(`shortIntegerToDisplayString`, `real34ToDisplayString`,
`complex34ToDisplayString`, `insertSepsIntoIntegerText`, `addBaseNumber`);
`bufferize.c` `closeNim` and the `ITM_EXPONENT` arm; `integers.c`
`fnChangeBase`; `items.c` row 1687 and the `FPGRP` range; `defines.h` gap
macros; `config.c` defaults; `error.c` `displayCalcErrorMessage`;
`keyboard.c`'s `tmpString` hand-off; `src/testSuite/hal/lcd.c` for the polarity
of `LCD_SET_VALUE`.

### Not reached, and it matters where

- **`prettyLayout.c` measure and paint arithmetic.** Nobody re-derived the
  `PP_SUP` placement rule; the "two exponents at one height" consequence in
  `PP18RR7-1` rests on round 6's measurement of it, not on a fresh one.
- **`prettyCapture.c` beyond the leaf builders and the serializer**, and
  `prettyValue.c` beyond the T-line gate. Both hold the producer half of two
  findings here and have now been outside the file set for four rounds.
- **The browser's key routing and pan state machine** past `pbPaint`.
- **`ppqFrameIntegral` / `ppqFrameDerivative`** (the EQSHW framing arms) and
  the equation-language evaluator in `solver/equation.c`.
- **`keyboard.c`, `items.c`, `softmenus.c` patch bodies** — unchanged in range,
  covered only by their churn metrics and the 2026-08-27 review.
- Three of the eight in-family readers ran **read-only** and produced no probe;
  their findings were executed later by the refutation pass, which is why the
  mutation table in §2 is shorter than the finding list.

### Out-of-family accounting

Both replies exist, both carry a `MODEL:` line, neither is empty.

| family | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r7/packet-sol-latch.md` | `/tmp/pp18-r7/packet-sol-latch.sol.reply.md` | `MODEL: GPT-5` | 1 |
| gemini / gemini | `/tmp/pp18-r7/packet-gemini-leaf.md` | `/tmp/pp18-r7/packet-gemini-leaf.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 2 |

The sol packet asked one question — the `PP_NONE`-versus-`c->failed` latch
semantics of the equation parser. The gemini packet asked for a census of
every leaf producer that stakes a precedence, then for the defects that census
exposes. Both packets were self-contained (the sol dispatch ran in an empty
read-only workdir, `/tmp/tmp.d3cYzWlTXb`, so every line it reasoned about was
inline). Survival counts are in §8.

---

## 2. Mechanical results

**Gate: GREEN at `f4421da32`** (pre-verified this session, and re-run inside
every mutation below; the suite banner printed `13025` assertions in the run
that recorded it). **Churn scan: clean.** Re-run here:

```
python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
        packages/pretty-print/patches/*.patch     → exit 0
13 patches; 0 modified upstream lines
solver/equation.c 609/0/5 · keyboard.c 64/3/13 · screen.c 49/4/5 ·
softmenus.c 21/2/4 · items.c 19/10/5 · items.h 8/6/2 · defines.h 4/1/2 · …
```

`design-docs/forth-core/design-audit.sh` **does not cover this package**: line
29 hard-wires `PKG="packages/forth-core"` and line 36 reads that package's
refresh manifest. The mechanical half for pretty-print is the churn scan above
plus `./packages/pretty-print/build-test.sh`, which refreshes before it builds.

### Mutations run by the refutation pass

Every probe was applied to the flat working area, propagated by
`pkg_patch_refresh.py` into `files/`, verified present in
`build.sim/custom_pkg_shadow/`, run, then reverted; each verifier ended with an
empty `git status --porcelain` at `f4421da32`.

| probe | result | measured line |
|---|---|---|
| file `S(-5)` after CLSTK and decode via `ppfBuildEntry` | RED, sole failure | `AUDIT-PROBE R7 filed row shape (expected 'S(P(-5)|2)', actual 'S(-5|2)')` |
| file the three T25 recipes and decode them | RED, three failures, reach guards passed first | `PROBE-A FILED sci power draws its two exponents as one` / `PROBE-B …typed…` / `PROBE-C FILED signed numeral draws unbracketed` |
| `0 ITM_toINT(2) ENTER 1 −` then `ppfBuildEntry` | measured refusal | `dt=8 tag=2 wsize=64 grpDisabled=0` · `display strlen=160` · `hist=1` · `buildEntry=0` |
| 507-byte equation `("1+"×5)+("7"×492)+"abc/2"` through `ppqParse` | parse reported success, numeral gone | `srclen=507 PARSE OK` · tree `[[[[[1 + 1] + 1] + 1] + 1] + F(abc|2)]` · `w=117 a=21 d=8 wlimit=396` |
| print `lastIntegerBase` after T26 and for the five rows after it | leak measured, gate still green | `after T26 ppcTestReset, lastIntegerBase=16` · `powRow 0/1/2 … xtype=8` · `typed-sci EXP row, lastIntegerBase=0` |
| comment out `screenUpdatingMode |= SCRUPD_MANUAL_STACK` (`prettyVisual.c:1650`) | RED, sole failure | `V-CHROME the Z/T arm did not claim the stack` |
| force `ppfTextIsAtom` to return `true` | RED, four named failures | `EQ4d …two exponents as one` + three T25 rows |
| remove `ppfRunPrec`'s first-character name gate | RED | `EQ23 caret exponent (expected 'F(S(x|2)|[x + 1])', actual 'F(S(P(x)|2)|[x + 1])')` |
| add `ppfTextIsAtom` to `PPA_VAR` + `PPN_CONST` + `PPN_RCL` | RED on the walker half only (8 rows) | `V46 … 'B([B([P(t) d t]|0|x) d x]|0|2)'`; capture half alone: GREEN |

Two harness facts came out of those runs and belong here rather than in a
finding:

- **`./packages/forth-core/build-test.sh` gives a false green for a
  pretty-print edit.** It sets `PKG="packages/forth-core"`, so it never
  refreshes this package and the shadow it builds contains no `prettyFormula.c`.
  Two independent verifiers hit it, one of them twice. `CLAUDE.md` names that
  script as *the* gate. The governing gate for this package is
  `./packages/pretty-print/build-test.sh` (`DESIGN.md:13`).
- **Eight of eight verifier worktrees spawned at `e21af8d28`**, 140 commits off
  the audited tip, with `f4421da32` not an ancestor. Every one had to
  `git checkout f4421da32` before reading. No foreign mutation was found in any
  worktree.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner.

---

### PP18RR7-1 — `ppfBuildEntry` is the third leaf producer and it was not converted, so a filed formula loses the bracket the live one just gained

**Where.** `packages/pretty-print/prettyFormula.c:594` (`PPT_TKL`) and `:618`
(`PPT_TKV` / `PPT_TKRES`). Both arms do `stackPrec[sp++] = PPF_PREC_ATOM;`
unconditionally. The converted producers are `:481` and `:507`
(`ppfFromCaptureNode`), `prettyVisual.c:1122` (`ppvAstToNodes`) and
`prettyEquation.c:614` (`ppqFactor`, through `ppfRunPrec`).

**Reaching input, measured.** Three sequences, all with natural display on
(default):

1. `5` `[+/-]` `[x²]` `[CLSTK]` `[PHIST]`. `ITM_CHS` during NIM sign-prefixes
   `aimBuffer`; `prettyNoteNimText` strips only a leading `+`
   (`prettyCapture.c:1180`), so the capture leaf is `PPN_LIT "-5"`. CLSTK
   displaces the root, which files it; `ppcSerializeNode` writes `PPT_TKL` with
   the text verbatim (`prettyCapture.c:346-350`).
2. `1` `EEX` `50` `STO Y` `RCL Y` `[x²]` `[CLSTK]` `[PHIST]` — a `PPN_VAL` leaf,
   filed as one flat `PPT_TKV` (`prettyCapture.c:363`), formatted by
   `real34ToDisplayString` into a run ending in superscript glyphs.
3. `1` `EEX` `50` `[x²]` `[CLSTK]` `[PHIST]` — the typed ASCII spelling
   `1.e+50`, filed as `PPT_TKL`.

The paint path is not the test harness: `fnPrettyHist` (`:790`) →
`prettyBrowser` → `ppfBuildRow` (`browsers/prettyBrowser.c:64` and `:81`), and
`ppfBuildRow` (`prettyFormula.c:750-751`) routes **every row except the live
row 0** through `ppfBuildEntry`. `PPT_TKO1` then calls
`ppfBuildOp1(ITM_SQUARE, node, ATOM)` (`:649`) → `ppfPowBase`, whose only
surviving test is `nd->kind == PP_SUP` (`:169`); a flat `PP_RUN` fails it and
`ppfWrapIf(a, ATOM, ATOM)` inserts nothing.

**What breaks.** The PHIST pager and the CM-20 browser draw `-5²` next to its
own `= 25` — the picture of −25 beside the result 25 — and draw
`1·10⁵⁰²`-shaped ink for a value of 1e100, both exponents on one baseline. The
same formula, one row above, on the same screen, draws correctly through
`ppfBuildCurrent`; so does the T line under `FLAG_PTLINE`. The same missing
precedence also drops the MULT and SUB brackets on the filed row (`3·-5`,
`7--5`) that the live row now has.

Two probes produced this, not one reader's reading:

```
prettyPrint test FAIL: AUDIT-PROBE R7 filed row shape (expected 'S(P(-5)|2)', actual 'S(-5|2)')
prettyPrint test FAIL: PROBE-A FILED sci power draws its two exponents as one
prettyPrint test FAIL: PROBE-B FILED typed power draws its two exponents as one
prettyPrint test FAIL: PROBE-C FILED signed numeral draws unbracketed
```

In each run those were the only failures, and each probe's reach guard (the
`STD_SUB_10` glyph, the ASCII `e+`, the leading `-`) passed first, so no row
was vacuous.

**Half of it is a regression this wave introduced.** At `7ddff2c5f`
`ppfPowBase` scanned the base run's own text for a trailing `0xa160..0xa16b`
glyph and parenthesised regardless of `aPrec`, so the filed scientific row was
correct. `b27394db6` deleted that scan (`git show b27394db6 --
packages/pretty-print/prettyFormula.c` removes the comment
`// 0xa160..0xa16b: the superscript digits plus the signs` and the
`isPower = …` line) and taught the rule to two leaves. The signed-numeral half
was wrong before and after.

**Contracts violated.**

- `prettyInternal.h:124-125`, written by this wave: *"A base whose TEXT reads as
  a term is a different half of that class and is decided at the leaf, which
  reports `PPF_PREC_ADD` for it."*
- `prettyFormula.c:86-88`: the `ppfBuildOp*` builders are *"shared by the tree
  walker and the token decoder so both paths typeset identically."*
- `DESIGN.md:576-580`, rewritten by this wave: *"both leaf builders ask the same
  `ppfTextIsAtom`."*
- `b27394db6`'s own message: *"the walker's leaf calls the same function so the
  two cannot drift."* There are five numeric leaf producers, not two.

**Bug class.** A rule moved off a consumer chokepoint onto its producers, with
the producer census taken from the file that was open rather than from the
call graph. (Sibling of the catalogued *"a constraint documented in one place
does not travel"*, and the third instance of the project's own
audit-fix-regression shape.)

**Class-level test that would pin it.** For every fixture in the T25 power
block, and for the MULT/SUB rows too, assert the **filed** picture equals the
**live** picture: build with `ppfBuildCurrent`, `ppcTestOp(ITM_CLSTK)`,
`ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), …)`, and compare the two
signatures. The block already knows this shape — it does exactly that for the
structural `yx`-over-`yx` case twelve lines above (`prettyTest.c:1570-1579`).
A surface-equality property over the whole fixture table is the general form
and is what nothing in the suite currently asks.

---

### PP18RR7-2 — the `dtShortInteger` arm's operative length gate is `ppfValBuf`'s 96, not the 200 the wave guarded, and a 64-bit binary value trips it exactly

**Where.** `packages/pretty-print/prettyFormula.c:76` (`if(strlen(buf) >=
destSize) return false;`), against `ppfValBuf[96]` at `:23`. The new guard the
wave added is at `:68` and compares against `sizeof(buf)` = 200.

**Reaching input, measured.** `BASE 2` with the default `WSIZE 64` and the
default digit grouping, then `0` `ENTER` `1` `−` (X = −1 = 64 set bits), then
`CLSTK`, then `PHIST`. Probe output at `f4421da32`:

```
PROBE R7: dt=8 tag=2 wsize=64 grpDisabled=0
PROBE R7: value=ffffffffffffffff
PROBE R7: display strlen=160
PROBE R7: hist=1
PROBE R7: buildEntry=0
```

Both defaults are upstream's: `config.c:35` (`_gapl` = `ITM_SPACE_PUNCTUATION`,
`_gprl` = 3, so grouping is on out of the box) and `config.c:1453`
(`fnSetWordSize(64)`). `display.c:2091-2093` sets gap = 4 for base 2, `:2029`
forces sign = 0, `:1912` appends the two-byte `STD_BASE_2`.

**What breaks.** `ppfBuildEntry:606` returns false → `ppfBuildRow` returns
false → the browser paints `(too large to show)`
(`browsers/prettyBrowser.c:94-99`) and the PHIST pager drops the row entirely
(`prettyFormula.c:823`, `:845`). The string is 160 bytes only because the
builder's second try re-spells every digit as a two-byte glyph, and that try
**returns only when it fits `SCREEN_WIDTH`** — so upstream draws this value in
full on one line while the browser row refuses it. Base 4, 8 and 16 all fit
(64, 38, 32 bytes), which is why T26 (base 16, value 15) does not see it.

**Contract violated.** `b27394db6` states the arm's repair as complete — *"The
arm now passes `tmpString`, which is what every upstream caller passes"* — and
T26's comment (`prettyTest.c:1474-1485`) says *"the assertion is that the entry
decodes to text at all."* For a base-2 value the entry does not decode at all,
and the pin cannot see it because it drives base 16.

**Secondary, same arm.** The 200-byte guard added at `:68` can never be the
deciding branch. With `determineFont = false` only two spellings are reachable
and the widest is 160 bytes; everything above 95 has already been refused at
`:76`. It is sound as the `strcpy`'s correctness proof; it is not a reachable
decision, and reading it as one is how the 96 stayed invisible.

**Bug class.** Two size gates on one path, where the guard that was reasoned
about is not the guard that decides. (Adjacent to the wave's own new class,
*"a buffer sized against a formatter's output, not its scratch"*.)

**Class-level test that would pin it.** One row per integer base
(2, 4, 8, 10, 16) at `WSIZE 64` with all bits set, filed and decoded through
`ppfBuildEntry`, asserting the entry decodes — the same assertion T26 already
makes, driven across the base axis instead of at one point. The general
property: for every value upstream will draw on one line, the browser row must
draw or decline for a reason other than its own staging buffer.

---

### PP18RR7-3 — an allocation-failure `PP_NONE` from a leaf is consumed as a probe-no, so `EQSHW` paints a formula with a numeral missing and reports success

Raised out of family (GPT-5). Reaching input reconstructed and executed here.

**Where.** `packages/pretty-print/prettyEquation.c:118` — `ppqNumber` consumes
its digits and then `return ppNewRun(c->s + start, …)` with no `c->failed` on
the `PP_NONE`. `ppqPrimary` (`:578-587`) treats that `PP_NONE` as *"not a
number, try a name"* and calls `ppqName` **at the already-advanced position**.

**Reaching input, measured.** The reader's literal string cannot fail — an
equation is capped at 508 glyphs (`MAX_NUMBER_OF_GLYPHS_IN_STRING`,
`defines.h:2097`) and a 508-byte numeral needs 509 ≤ `PP_TEXT_BYTES` 512. But
the pool is filled by every earlier run at `len + 1` each, so a few leading
one-byte terms lift `ppTextLen` enough that a legal-length numeral overflows
while the short name after it still fits:

```
"1+" ×5  +  "7" ×492  +  "abc/2"        (507 bytes)
AUDIT-PROBE R7: srclen=507 PARSE OK
tree: [[[[[1 + 1] + 1] + 1] + 1] + F(abc|2)]      ← the 492-digit numeral is gone
w=117 a=21 d=8 wlimit=396
```

Arithmetic: ten leading runs cost `ppTextLen` 20, so the numeral needs
20 + 492 + 1 = 513 > 512 and is refused with no node consumed, while `abc`
needs 24 and fits. Node exhaustion cannot produce this — it is monotonic; the
text pool is length-dependent, which is why this is the one allocator that can
fail in the middle and let the parse continue.

**What breaks, and on which surface.** Not the softmenu strip: that path feeds
`showEquation`'s ellipsis-truncated display string, which the strict parser
rejects on the ellipsis, and the probe tree is 29 px against the strip's 23 px
gate. **EQSHW** has no such escape. `ppqShowRender` (`:967-993`) gates on
width ≤ 396 and ascent + descent ≤ 147; 117 and 29 pass, so it paints and sets
`pretty = true`. `setEquation` (`src/c47/solver/equation.c:216`) stores
`aimBuffer` verbatim before validation, so a 507-character stored equation is
storable and showable. The owner sees a 2D formula that is not the stored one,
reported as success instead of declining to the linear fallback.

**Contract violated.** `prettyEquation.c:10`: *"Anything the grammar does not
fully recognize — an ellipsis-truncated string, an unknown glyph, a dangling
operator — declines, and upstream's linear rendering runs unchanged."* Round 6
latched exactly this class one frame up, at `ppqFactor:616` (*"ppfPowBase
allocates and does not set it"*) — at one site only.

**Bug class.** One sentinel, two meanings: an allocator refusal and a grammar
probe-no share `PP_NONE`, and the position has already advanced.

**Class-level test that would pin it.** A parser property, not a fixture: for
every equation string the suite parses, if `ppqParse` returns true then the
concatenated text of the built tree's runs must account for every consumed
byte. Drive it with the text pool pre-loaded to a few chosen high-water marks
so each leaf allocator fails in turn — the same table shape the node-pool
exhaustion rows already use.

---

### PP18RR7-4 — the wave's three new class rows never file, so nothing in the suite drives the third producer

**Where.** `packages/pretty-print/prettyTest.c:1589-1594` and the three blocks
under it: the negated square (`:1596-1612`), the typed scientific square
(`:1619-1657`), the recalled scientific square (`:1659-1686`). All three stop
at `ppfBuildCurrent`. The only filed assertion in the block is the older
`yx`-over-`yx` row (`:1570-1579`), whose base is a `PP_SUP` node and is
therefore caught by the structural test `ppfPowBase` kept.

**Why it is a finding and not a note.** The row's own comment states a property
it does not check: *"Both leaves now ask `ppfTextIsAtom`, so they cannot
disagree."* The block twelve lines above states the pattern the new rows
dropped: *"it reaches the same builder on both capture surfaces — the live tree
here, the filed entry below through the token decoder."* That pattern, applied
to the three new rows, is what turned red in §2's probes. This is the coverage
hole that let `PP18RR7-1` ship inside a wave whose entire purpose was to close
that class.

**What the existing filed assertion could and could not have caught.**
`ppfTestPowersScoped` (`:2476-2486`) fires on a `PP_SUP` base **or** a run
ending in superscript glyphs, so it would have caught the scientific regression
had a scientific row been filed. It is blind to a leading minus, so the
signed-numeral half needed the exact-signature assertion the live rows use.

**Bug class.** A pin written for the surface the fix was typed on, when the
class is defined across surfaces.

**Class-level test.** Same as `PP18RR7-1`: surface equality as a property over
the fixture table, not per-row duplication.

---

### PP18RR7-5 — the renderer now writes `tmpString`, which §1's rule marked BINDING says it never does

**Where.** `packages/pretty-print/prettyFormula.c:67`,
`shortIntegerToDisplayString(TEMP_REGISTER_1, tmpString, false, 0)`, introduced
by `b27394db6` to fix a real 56-byte overflow.

**Status: a documentation-versus-code divergence on a rule marked BINDING, not
a live corruption.** Both the finder and the refutation pass looked for a
caller that holds live data in `tmpString` across either entry and neither
found one. `_refreshRegisterLine`'s own `tmpString` arms are all behind
`temporaryInformation` / `lastErrorCode` / `calcMode` conditions that
`prettyTryRegisterLine` (`prettyValue.c:799-808`) declines on, or rebuild the
string after the hook; `showEquation`'s `tmpString` round trip goes through
`prettyTryEquation`, which reaches `ppq*` only and never `ppfFormatStaged`;
`keyboard.c`'s hand-off is read only in `CM_ASSIGN`, where the pretty arms
decline.

**The rule.** `DESIGN.md:129-134`, intact and unamended at the audited tip:
*"**Fallback rule (BINDING).** … paints nothing and returns false, and
upstream's own arm renders unchanged. The renderer never reads or writes
`tmpString`; on fallback the upstream path is provably untouched."* Its scope
is package-wide (the paragraph opens *"Every pretty path is a `bool_t`
try-function"*, and §2 defers to it). `git diff aafd38f7d..f4421da32` contains
no amendment to it; the same commit that added the write **did** edit
`DESIGN.md` §6, so the document was open and being maintained.

**Two specifics that keep this above the noise floor.**

- The call-site comment argues a strictly narrower proposition (*"nothing runs
  between the call and the copy below"*). It was copy-adapted from
  `packages/forth-core/forth_bridge.c:292-306`, whose version carries the
  reconciling clause — *"That does not weaken the 'never tmpString' rule above
  — that rule is about HOLDING a pointer…"* — and that clause is what the
  transplant dropped. forth-core's ruling governs its own file banner and does
  not travel; this wave catalogued that very class.
- The clause the rule makes is about **fallback**, and `:68-70` can
  `return false` *after* `tmpString` has been overwritten. So "on fallback the
  upstream path is provably untouched" is false by construction on that arm,
  independent of any aliasing argument.

**Bug class.** A binding invariant left unamended by the fix that broke it;
the reasoning that justified the fix was borrowed from a sibling package
without its scoping clause.

**Class-level test.** Not a runtime test — a doc-drift check. The cheap
mechanical form is a grep gate: every `tmpString` write inside
`packages/pretty-print/` must be listed in §1's rule as a named exception. The
alternative, and what the rest of the package does, is a package-owned buffer
(`ppfValBuf[96]`, `ppScratch[200]`, `ppSpanA/B[120]` are all package-owned;
`:67` is the only place render code writes an upstream global).

---

### PP18RR7-6 — `ppfTextIsAtom`'s accepted alphabet is narrower than the display formatter's separator emit-set

**Where.** `packages/pretty-print/prettyFormula.c:125` — below 0x80 the
predicate accepts only `[0-9]`, `.`, `,` and `' '`.

**Reaching input (traced through the code, not executed).**
`MODE → GAP → GAP-L → apostrophe` (`ITM_GAPAPO_L`, one of thirteen choices in
`menu_GAP_L`, `softmenus.c:1019-1021`). `defines.h:2353-2357` maps `'` to the
two-byte descriptor `"'\1\0"`, and because `SEPARATOR_LEFT[1] == 1`,
`insertSepsIntoIntegerText` (`display.c:2521-2523`) writes a bare `0x27` between
groups. Then any long-integer value leaf: `1234567` into a register, recalled
or re-derived, times 2. `ppfTextIsAtom` rejects `0x27`, `:507` reports
`PPF_PREC_ADD`, and `ppfBuildOp2`'s MULT arm brackets it.

**What breaks.** `(1'234'567)·2` — brackets round a bare numeral that needs
none — on PSHOW, the T line and every history row; the same for the right
operand of a subtraction and for a power base. Owners on the default space
separator (`\xa0\x08`, inside the accepted `0xa000..0xa00f` window) see none of
it, which is why the gate is green. Not wrong mathematics: clutter, plus one
`PP_PAREN` node out of the 72-node pool per grouped operand.

**Contract violated.** `prettyEquation.c:92` names this codebase's own class:
*"Class: acceptor alphabet narrower than the producer's emit-set."* And the
predicate's own charter at `prettyFormula.c:109-116` — *"Only digits, the radix
mark and the digit-group spaces are"* atoms — enumerates the space spelling
instead of asking the producer which character it emitted.
`DESIGN-HISTORY.md`'s entry for this wave closes with the lesson the predicate
did not finish applying: *"A guard whose body is a growing disjunction of forms
is a guard that has not found its property yet."*

**Bug class.** Acceptor alphabet narrower than the producer's emit-set, at a
producer whose alphabet is an owner setting.

**Class-level test.** One row per `GAP-L` choice (the menu has thirteen), each
formatting a grouped long integer under MULT and asserting no `PP_PAREN`. The
property behind it: the predicate must accept whatever
`insertSepsIntoIntegerText` can emit for the current configuration, so the
honest form asks the producer (`SEPARATOR_LEFT`/`RIGHT`) rather than listing
glyphs.

---

### PP18RR7-7 — one rule, two exported spellings with different semantics, and no stated rule for choosing

**Where.** `packages/pretty-print/prettyInternal.h:133-134`. `ppfTextIsAtom` is
declared as the class test — *"digits, radix and digit-group spaces only;
anything else reads as a term"* — with **no caller obligation**. `ppfRunPrec`
is declared as *"`PPF_PREC_ATOM` or `_ADD` for a built `PP_RUN`, from its
text"*. The difference between them — a first-character name gate, without
which `ppfTextIsAtom("R05")` and `ppfTextIsAtom("x")` are both false — exists
only in a comment at `prettyFormula.c:144-147`.

**Reached today: no.** Every current call site happens to hold a numeral. The
finding is about the next edit, and the next edit is invited by the design text:
`DESIGN.md:576-580` says *"both leaf builders ask the same `ppfTextIsAtom`"*
without the word "numeric" that `DESIGN-HISTORY.md:1472` uses.

**What makes it more than tidiness — measured.** The obvious next tidy is to
give the three untested leaves the same test: `PPA_VAR` (`prettyVisual.c:1113`,
one line above the arm that now carries it), `PPN_CONST` (`prettyFormula.c:511`)
and `PPN_RCL` (`:518`). Applying it to all three turns the gate RED with eight
walker rows (`V46 … 'B([B([P(t) d t]|0|x) d x]|0|2)'`, V47, V50, V51, V56, V57,
V68, V69). Applying it to the **capture half alone** leaves the gate **GREEN**:
the only `PPN_RCL` row (T17, `:1205-1216`) asserts the capture signature, never
the laid-out node string, and `PPN_CONST` has no layout row at all. So
`(R05)²` and π drawn as a term would ship with nothing objecting. Both arms are
live user paths: `ppcRclLeaf` mints `PPN_RCL` for any RCL of register 0..99,
and `PPC_CONSTCLS` mints `PPN_CONST` for the constants menu.

**Contract violated.** The round's own conclusion in `DESIGN-HISTORY.md`: *"the
rule moved to where the fact is known … only the producer that formatted the
text can answer it."* Two exported answers to one question, and the safe one is
not marked.

**Bug class.** One rule, two exported entry points with different
preconditions, the precondition stated in a comment rather than at the
declaration.

**Class-level test.** A layout-signature row for a `PPN_RCL` leaf and for a
`PPN_CONST` leaf under `x²` and under `·`, asserting **no** bracket — the trap
that currently has no net. The header line for `ppfTextIsAtom` should carry the
caller obligation ("numeric leaves only"), which is the part a test cannot say.

---

### PP18RR7-8 — `DESIGN.md`'s hook table names 8 of the 13 patched upstream files, after this wave edited it

**Where.** `design-docs/pretty-print/DESIGN.md:784-794`, the §7 table introduced
as *"Upstream files hooked, with verified adjacency to sibling packages'
hunks."* It lists `c47.h`, `screen.c`, `items.c`, `bufferize.c`, `calcMode.c`,
`config.c`, `testSuite/*`. The churn scan lists 13 patches.

**Scope, after refutation.** Two of the five missing files are in fact covered
one table up: the whole `defines.h` patch is the flag-count identical-edit row
(`:775`), and all four `softmenus.c` hunks map onto `:779-781`. `items.h` is
half-covered (`:773` gives its adjacency against undo-history for the
`:481-490` hunk; the `:228-232` hunk is not). The residue is **`keyboard.c`
and `solver/equation.c`, and it is worse than "a missing row"**:

- `keyboard.c` — 13 hunks, 64 adds, and the file all three packages patch.
  Three anchors are byte-identical to undo-history's (the sanctioned softkey
  gate range). The rest interleave at 12/11/12/15/7 lines, and the last pair
  **overlaps**: ours `@@ -4931,6` spans 4931-4936, undo-history's `@@ -4935,6`
  spans 4935-4941. That is tighter than the `screen.c` case commit `1a98ac816`
  documented in this very range as *"the tight one: ONE untouched line
  separates them"*. `DESIGN.md:349` calls `keyboard.c` *"the project's riskiest
  three-package composition surface"* and gives it no row.
- `solver/equation.c` — 609 adds, 74% of the package's added upstream lines.
  The adjacency answer is benign (no sibling patches it), but it is stated only
  in the non-normative trail (`DESIGN-HISTORY.md:1063-1066`) and the hunk count
  there is stale: `DESIGN.md:823`'s PP5 row still says "one hunk"; there are 5.

**Not supportable, and dropped from the finding as filed:** that line 796's
*"No patches to `stack.c`, `defines.h`, `keyboard.c`, `softmenus.c`,
`statusBar.c` until PP4"* is false today. It reads as anticipatory, and the
2026-08-27 review read it the same way. It is stale differently: §6:346 rules
PP4 a pager with *"zero keyboard.c/defines.h churn"* while table 1:774 says the
browser has been wired since PP10, so the churn arrived at PP10.

**Contract violated.** The table's own introducing sentence. The same class was
filed on 2026-08-27
(`REVIEW_upstream-minimality_2026-08-27.md` §3.1, *"the most consequential item
in this review"*) and is still open; `1a98ac816`, inside this range, rewrote one
row of this table after finding it stale and left the rest.

**Bug class.** An inventory document maintained row-wise, at the row the
current task touched.

**Class-level test.** Mechanical, and it belongs in the churn scanner: list the
patch set, list the files named in §7, fail on any patch with no row. The
upstream-diff-review method already says *"check it against `DESIGN.md` §6's
hook tables where covered"* — the scanner knows the patch list, so the check
costs nothing.

---

### PP18RR7-9 — T26 leaves `lastIntegerBase = 16` behind it, and `ppcTestReset` does not restore it

**Where.** `packages/pretty-print/prettyTest.c:1490`,
`ppcTestOpParam(ITM_toINT, 16)` → `items.c` row 1687 → `fnChangeBase(16)` →
`lastIntegerBase = 16` (`src/c47/integers.c:19`).

**Measured.** The trailing `ppcTestReset()` (`:1511`) clears `calcMode`,
`temporaryInformation`, `lastErrorCode`, `programRunStop`, four flags,
`aimBuffer` and `nimNumberPart`, and calls `prettyReset()`. None of that touches
the base:

```
AUDIT-PROBE R7: after T26 ppcTestReset, lastIntegerBase=16
AUDIT-PROBE R7: powRow 0/1/2 … lastIntegerBase=16 xtype=8   (dtShortInteger)
AUDIT-PROBE R7: RR6-2 negated row, lastIntegerBase=16 xtype=8
AUDIT-PROBE R7: typed-sci EXP row, lastIntegerBase=0 xtype=1
```

So five rows written against a long integer or a real run as base-16 short
integers, and the leak self-heals only at the next row that types `EEX` or `.`
(`bufferize.c:1221`). The headline "for the rest of the process" is wrong —
the blast radius is those five rows. Note that `closeNim`'s suffix branch
**re-sets** `lastIntegerBase` at `bufferize.c:2575`, so the `+` inside T26
renews the leak rather than clearing it.

**Why nothing is red.** The capture hook snapshots `aimBuffer` at the top of
`closeNim`, before the `#16` suffix is appended (`bufferize.c:2345`), so the
signature pins still see `"3"` and `"-5"`.

**Contract violated.** `ppcTestReset`'s stated job (`prettyTest.c:883-884`):
*"NIM typing residue must not leak into later suite blocks."* T26 introduces a
second kind of residue — a persistent entry **mode** — that the helper does not
cover, and T26's own comment acknowledges the dependency it creates: *"The
operand's spelling depends on the base."*

**Cost.** Latent. The next row anyone inserts after T26 that types a plain
integer and asserts on the **formatted** value gets a hex short integer, and its
reach check will read as a product defect. Same shape as `PP18RR6-4`: a harness
whose state quietly stops meaning what the rows assume.

**Bug class.** A reset helper that covers the state its author had in hand, in
a suite whose rows are order-dependent.

**Class-level test.** Have `ppcTestReset` restore `lastIntegerBase` (and assert
it), then add a self-check row that runs after any block that changes an entry
mode: type a plain integer, assert `getRegisterDataType(REGISTER_X)` is what
the following rows assume. The general property: a reset helper must return
every global the suite's rows read, and the enumerable set is the globals the
harness itself writes.

---

## 4. PLAUSIBLE

**None.** No finding reached the refutation pass in the state that section
describes — survived, but with no constructible reaching input. Every survivor
in §3 carries either an executed probe or a fully traced keystroke path, and
`PP18RR7-5` and `-8` are contract/documentation divergences whose "reaching
input" is a human reading the document.

Recorded instead, so they are findings the day something changes. These were
named by readers, did **not** go through refutation, and none has a constructed
input:

1. **`ppfBuildEntry` silently drops the `" = result"` tail** when the `PPT_TKRES`
   run fails (`prettyFormula.c:704`), while every other allocation failure in
   the function returns false. TKRES is the last and largest run, so it is the
   likeliest to hit the 512-byte text pool. Nobody could construct an entry that
   exhausts the pool exactly there. Degrading may be the better answer; the
   asymmetry is what is unrecorded.
2. **`ppqFrameDerivative` returns the bare equation** when any of its five
   allocations fails (`prettyEquation.c:921`), so EQSHW would paint the
   integrand with no `d/dx` prefix and still report `pretty = true`. Needs a
   parse that used ~68 of 72 nodes and still fits the band. Pre-existing.
3. **`ppfStageValFields` clears `lastErrorCode`** and, on the `RAM_FULL` arm,
   upstream's `displayCalcErrorMessage` also sets `screenUpdatingMode =
   SCRUPD_AUTO` (`error.c:298`), which the package does not undo. Every entry
   point that reaches it guards on `lastErrorCode == 0` first, and `fnPrettyHist`
   re-ORs its bits after both passes. No keypress sequence fills RAM at that
   instant.
4. **`ppfPowBase`'s header claims every producer of a `PP_SUP` calls it**
   (`prettyInternal.h:122-123`). `prettyValue.c:316` builds one without it. Its
   base is a mantissa run the same function just synthesised, so it is an atom
   by construction; the header enumerates its three producers explicitly, so it
   is not claiming that site. Benign, pre-existing, worth a word in the header.
5. **The `dtShortInteger` arm passes `determineFont = false`**, so the builder
   is locked to `standardFont` and skips the tiny rung upstream uses for wide
   integers; a value that fails both width tries yields the sentence
   *"Integer data representation to wide!"* as the operand text. Predates the
   wave (it changed the buffer, not the argument) and `moreInfoOnError` is a
   PC_BUILD printf. Related to `PP18RR7-2`, and the two should be looked at
   together.

---

## 5. Design observations

**D1 — the rule moved off a chokepoint onto its producers, and the producer
census was taken from the open file.** `ppfPowBase` was a consumer: whoever
built the run, it inspected the text and decided. That covered five producers
for free. The wave replaced it with a producer obligation and enumerated the
producers by reading `ppfFromCaptureNode` and `ppvAstToNodes`, which were the
two the round-6 finding named. The other three (`ppfBuildEntry`'s two arms,
`ppqFactor`) were found only because a later reader asked "how many are there?"
The project's memory already records that relocating state is the most
dangerous fix shape; this is the same shape one level up — relocating a
*decision*.

**D2 — the rule now has two exported spellings and five producers, and the safe
one is not marked.** `ppfTextIsAtom` answers "does this text read as a term",
`ppfRunPrec` answers "does this built run read as a term, and is it a name". A
future reader who tidies the leaves toward one predicate breaks the other half,
and the gate catches that only on the walker (see `PP18RR7-7`). The stable
shape is one entry point that takes the producer's own knowledge as an
argument.

**D3 — the suite pins pictures per surface, and the class is defined across
surfaces.** Four of this round's nine findings, and all three of the most
expensive, are a disagreement between two surfaces about one formula: live
versus filed, T line versus browser row, walker versus capture. Nothing in the
suite asks the general question. A surface-equality property over the existing
fixture table would have caught `PP18RR7-1` at zero design cost, and it is the
one test this package still does not have.

**D4 — the harness's own state is now an input the rows depend on.** T26
introduced a persistent entry mode into a suite whose reset helper covers NIM
text only (`PP18RR7-9`). As the fixtures reach further into upstream modes
(base, word size, angular mode, display format), the reset helper has to grow
with them or the rows stop meaning what they say.

**D5 — the gate named in `CLAUDE.md` is the wrong gate for this package, and it
fails green.** `./packages/forth-core/build-test.sh` refreshes only
`packages/forth-core`; a `packages/pretty-print` edit never reaches the shadow
it builds, so a mutation run under it returns exit 0 with the pretty battery
compiled from stale source. Two verifiers were fooled this round, one twice.
The governing gate is `./packages/pretty-print/build-test.sh`. Round 6 already
flagged the `CLAUDE.md` line; it is still there.

**D6 — the 200-byte guard that cannot decide.** `PP18RR7-2`'s secondary half is
worth keeping as shape, not just as a defect: a guard added to prove a `strcpy`
safe, sitting on a path where a tighter guard downstream always fires first,
reads to the next maintainer as *the* size decision. Two gates, one decision,
and the reasoning attached to the wrong one.

**D7 — out-of-family value came from the census question, not from the code.**
The gemini packet asked for an enumeration ("list every producer that stakes a
precedence") before asking for defects; that enumeration is what produced the
top finding. The sol packet asked one narrow semantic question about a sentinel
and produced a wrong-drawing-reported-as-success on a surface nobody in family
had reached. Both packets were structural, not "find bugs here", and both
returned findings that survived. That is the pattern to repeat.

---

## 6. Deliberately not flagged

Two kinds of item are merged here: what the refutation pass disproved, and what
the eight finders cleared during the read.

### Killed by the refutation pass

1. **"The `tmpString` write destroys the proof that a declining renderer leaves
   the display pipeline byte-untouched."** The mechanism is right and survives
   as `PP18RR7-5`; the consequence is wrong. That proof never existed. Since
   PP3/PP4, and mandated by `DESIGN.md:175` and `:345`, every value leaf calls
   `ppfStageValFields`, which does `reallocateRegister(TEMP_REGISTER_1, …)` +
   `xcopy` + `setRegisterTag` with **no save and no restore anywhere in the
   package**, on the decline path as much as the success path. And
   `TEMP_REGISTER_1` is live across the very hook: upstream's own
   `_refreshRegisterLine` does
   `copySourceRegisterToDestRegister(currentViewRegister, TEMP_REGISTER_1);
   regist = TEMP_REGISTER_1;` at `screen.c:3632-3633`, ~320 lines above the
   pretty arm, and every arm below reads `regist`. Both mutations are held off
   by one guard (`prettyValue.c:799-808`), not by ~190 `tmpString` sites — and
   that guard already had to be re-read at every merge. Also checked and
   rejected as an argument in the finding's favour: none of the other builders
   the renderer calls touches `tmpString`, so `:67` is the only write, which is
   what makes it a one-line documentation fix.
2. **"`ppvBody` pushes a binding scope and pops it only on the success path;
   four failure returns leak the slot."** The increment is reachable and so is
   the leaked state, but no input reads it. `ctx->failed` is write-once-true
   within a walk (the only `= false` is `ppvRun`'s initializer at `:1226`), all
   four exits latch it, all three callers propagate immediately, and all four
   readers of `bindingCount` sit downstream of guards. `bindingCount` is reset
   three lines below `failed` in the same initializer, so the leak's lifetime is
   one already-doomed run. The depth guard tests the same counter it protects,
   so a leak cannot produce an out-of-bounds write — only an earlier decline.
3. **"An invented sum counter only avoids names the walk has already seen, so a
   later free variable draws with the counter's letter."** The mechanical path
   is real and was reproduced; the consequence is not what the code draws. A
   `PPA_CONSTRUCT` reports `PPF_PREC_ADD` (`prettyVisual.c:1202`), so the MULT
   arm brackets it: the probe drew `[P(B(n|[n = 1]|3)) . n]`, the sum's scope
   closed before the trailing free `n`. Ordinary notation. The three positions
   where a same-letter collision would make the picture compute a different
   number are all guarded already (body → `D_COLLISION`, limits →
   `ppvNameInSubtree`, built tree → `ppvNameUsedInAst`). The order-dependence is
   real and changes only which letter is chosen (reversing the program yields
   `m`).
4. **"`ppqFrameIntegral`'s 48-byte limit buffers are too small for
   `real34ToDisplayString`'s first, unmeasured write."** True, and already
   filed: this is `PP18RR3-7`, on the prompt's known list, verified still open
   and fenced at round 4, deferred with a stated reason (*"the measured overrun
   is two bytes and the reaching configuration is a specific preset plus FIX 19;
   worth fixing with the sweep"*). The one thing the reader added — that the
   digit-grouping width is a user setting — is written inside `PP18RR3-7`
   itself as its named open sub-question. The declarations are also byte-
   identical to their PP13 original; no commit in this wave touched them.
5. **"V-CHROME's 'did not claim the stack' assertion cannot fire, because
   `SCRUPD_MANUAL_STACK` is already set by V19."** Measurably false. The only
   producer of that bit on this path is the line the reader proposed to delete
   (`prettyVisual.c:1650`): remove it and V19 does not set it either. Running
   exactly the mutation the finding predicts stays green turned the gate RED
   with one failure in 13025, and that failure is the literal string the finding
   says cannot appear. The residue is a catalogue gap, not a defect:
   `TESTING.md` records `MUT-135` for V-FULL and has no row for V-CHROME. This
   run supplies the missing measurement.
6. **"The suite's only class-level power check still implements the glyph
   enumeration the wave deleted from production."** `ppfTestPowersScoped` has
   three call sites and none is ever handed the fixture the reader ran through
   it; the negated-square tree is pinned by an exact signature, which is
   strictly stronger. Forcing `ppfTextIsAtom` to `true` reddens four rows, one
   of them (EQ4d) asserting *through* the property checker — so the class rule
   is gated. `ppfTestRunEndsSup` checks a still-enforced subset of the current
   rule, not a retired one. The prescribed replacement is also wrong:
   production's predicate for a built run is `ppfRunPrec`, and a property test
   built on bare `ppfTextIsAtom` would false-fail `x²`.
7. **"The total-early-return chrome guard tests the wrong bits."**
   (`screen.c:5896`.) The design wants "took the stack **alone**", and
   `SCRUPD_MANUAL_STACK` cannot express it: both paint arms set it
   (`prettyVisual.c:1575`, `:1650`) and `RETURN_NORMAL` re-ORs it at
   `screen.c:6075`, so the proposed conjunct discriminates nothing and sheds no
   caller. The predicate is the outcome of `f69f2749a`, whose message records
   the measurement behind it (`scrupd=10` = `MANUAL_STACK | MANUAL_SHIFT_STATUS`
   surviving), and it is pinned twice (V-CHROME asserts the exact mask, V-FULL
   asserts all three). The `SKIP_MENU_ONE_TIME` half was adjudicated
   REFUTED-on-reachability in round 6 against an exhaustive enumeration.
8. **"`ppfRunPrec`'s name gate classifies a spelling differently from the direct
   `ppfTextIsAtom` door — NaN through the power arm."** (Gemini finding 2.) The
   split is the ruled design, in four places, and the gate is the fix for a
   regression the first version of this repair caused: removing it reddens EQ23
   (`x²` drawn as `(x)²`). The two doors also judge different objects — in an
   equation `ppqName` accepts only letters, so "NaN" there is a variable name
   whose square is correctly unbracketed, while a stack NaN value reaches the
   letters through a different producer and is correctly bracketed. The
   properly-scoped residue is `PP18RR7-7`, filed.

### The wave's repairs, re-derived rather than trusted

- **`ppqFactor`'s new `c->failed = true` (`prettyEquation.c:616`)** is correct
  and needed: `ppqScopeOperand` latches its own failures, `ppfPowBase` does not.
  Checked the inverse direction too — every `PP_NONE` return in
  `ppqFactor`/`ppqTerm`/`ppqExpr` either latches or comes from a callee that
  did, and `ppqParse` tests both the flag and the node. The two sibling sites
  that do not latch (`:637`, `:696`) are guaranteed non-`PP_NONE` input.
  `ppqFunctionCall`'s flagless `PP_NONE` is deliberate ("no match, restore
  pos").
- **`ppfPowBase` losing its glyph scan** is not, by itself, the regression:
  the structural `PP_SUP` test still covers a power over a power, and three of
  the five producers now supply the leaf precedence. Only the fourth and fifth
  are unconverted, which is `PP18RR7-1`.
- **The `shortInteger` buffer arithmetic.** Bounded against `display.c:2056`
  (the builder starts at `ERROR_MESSAGE_LENGTH/2` = 256) and the compaction
  loops: worst-case scratch high-water ~356, compacted output ~110, inside
  `TMP_STR_LENGTH` 2560. The `_Static_assert` is a proxy (it asserts
  `TMP_STR_LENGTH >= ERROR_MESSAGE_LENGTH` rather than the exact requirement)
  but the slack is 2048 bytes, so the proxy holds. The 96-byte gate that
  actually decides is `PP18RR7-2`.
- **`ppvAstToNodes`' widened `PPA_LIT` predicate.** Looked like new spurious
  brackets on the VISUAL surface; it is not. `ppvLiteral` admits a `PPA_LIT`
  text only when it matches `-?[0-9.]*` — anything with an exponent goes OPAQUE
  — so on that alphabet the new predicate is exactly the old
  `pool[textOff] == '-'` test. The text back end's surviving copy of the old
  predicate (`:1297`) is compiled only under PC_BUILD/TESTSUITE_BUILD and sees
  the same restricted alphabet.
- **`ppvAstToNodes`' local stacked-power override** (`p = PPF_PREC_MUL`,
  `:1136-1141`) is now redundant, because `ppfPowBase`'s `PP_SUP` arm
  parenthesises before `aPrec` is consulted. `DESIGN.md:574`, edited by this
  wave, already says so. Redundancy the design names is not a finding.
- **The whitespace-only revert (`fff40b9be`).** The enclosed upstream
  `showString` is byte-identical to `src/c47/solver/equation.c:660` (checked
  with `cat -A`), the short-circuit order is unchanged
  (`cursorAt != EQUATION_NO_CURSOR` first, so `prettyTryEquation` is never
  called while editing), and the odd indentation carries the comment a rebaser
  needs.

### Guards and conjuncts, falsified or proved load-bearing

- **`ppfRunPrec`'s `t[0] == '-'` branch is dead from its only caller** —
  `ppqExpr` peels a leading sign into its own run and `ppqNumber` starts only on
  a digit, `.` or `,`. Dead but defensive, and it is what keeps a name atomic.
- **`ppfTextIsAtom`'s index arithmetic** is clean: a two-byte glyph is read only
  when both bytes are inside `len`, and a truncated trailing glyph returns
  false, which is the conservative side (more brackets, never fewer).
- **`ppfTextIsAtom` accepting ASCII space and `0xa000..0xa00f`** keeps the
  default grouped numeral an atom (`STD_SPACE_PUNCTUATION` is `\xa0\x08`), which
  is why no ordinary product gains brackets today.
- **`ppfBuildEntry`'s `PPT_TKC`/`PPT_TKR` leaves pushing `PPF_PREC_ATOM`
  (`:638`)** is correct, not a fourth instance of `PP18RR7-1`: those runs are
  `R00` or a catalog name, and a name is an atom whatever its glyphs.
- **`ppfBuildOp2`'s `ITM_YX` arm not class-testing the exponent.** An exponent
  needs no bracket to read correctly; `2^(-5)` drawing as `2⁻⁵` is right.
- **V27's added reach check** matches its comment: `lastErrorCode` at `:5947`
  and `screenHoldsDrawnPixels` at `:5964` are both asserted.
- **V-FULL's frame assertion is live, not vacuous.** `LCD_SET_VALUE` is 0 and
  the test HAL maps `val == 0` to `BLT_ANDN` (`src/testSuite/hal/lcd.c:65`), so
  the pre-fill **clears** every pixel and `lcd_buffer_pixel_on` can only be true
  where the arm painted.
- **`ppcTestNoteLabel`'s new overflow arm**: the table is 160 against 83 labels,
  `len > 7` is rejected before the 8-byte `strcpy`, and the else arm fails
  loudly. Every fixture label in the file is 1, 3 or 4 characters, so the silent
  `len > 7` skip has no member.

### Chased hard and killed, worth recording

- **`ppqBuildBigop`'s `PP_NONE` sentinel collision.** `ppvAstToNodes` passes
  `PP_NONE` as a legitimate "this kind does not use it", so an allocation
  failure would be indistinguishable — except the builder validates exactly the
  one each kind needs (`:238`, `:328`, `:356`), so a failure declines rather
  than drawing a big operator with no limit.
- **`ppEqDepth` / `ppEqStackBase` balance** in `solver/equation.c`: `++` and
  `--` have no `return` between them, the base is re-seeded on every depth-0
  entry, and `ppEqTempAppend`/`Delete` pair on every path including the SUM
  loop's `break`s.
- **`ppvStep`'s lift-latch exception list** checked against upstream's own SLS
  bits rather than intuition: `ITM_STO` is `SLS_ENABLED` (`items.c:1862`), so
  clearing `liftDisabled` after STO matches the machine; `ITM_CLX` is a
  documented non-goal (`DESIGN.md:743`).
- **`TEMP_REGISTER_1` left typed `dtShortInteger` while `screen.c`'s
  `TI_DISP_JULIAN` arm writes a 16-byte real34 into it with no realloc.**
  Traced, then cleared as not ours: upstream reaches the same state with no
  package installed (`store.c:174`/`:328`), so the fragility is upstream-native.
- **`showEquation`'s `(*cursorShown || …)` dereference** of a pointer
  `softmenus.c:3388` passes as NULL: byte-identical to upstream; the package's
  hook adds a clause after it. Not ours, and nobody could explain why it does
  not crash, so nothing is asserted about it.
- **`ppfBuildEntry` trusting the token stream** (no per-token check that
  `off + len` stays inside `total`): by construction the stream comes from
  `ppcSerializeNode`, which reserves before it writes and drops oversized
  entries rather than truncating. Predates the wave; no input files a malformed
  entry.
- **`complex34ToDisplayString` into `buf[200]`** — the same class as the fixed
  overflow, since upstream writes the imaginary part at a fixed offset of 100.
  Checked rather than assumed: at 8 digits the highest index touched is ~150.
- **The PHIST pager arithmetic** (`prettyFormula.c:820-855`): `totalRows` ≤ 13,
  `page` cannot wrap, `pages >= 1` by construction.
- **`fnPrettyVisual`'s solver-status save/clear/restore and the three chrome
  bits** on both paint arms match §6's dismissal rule; the new V-FULL fixture
  restores `lastErrorCode`, `temporaryInformation`, `screenHoldsDrawnPixels` and
  `screenUpdatingMode` on exit, so it leaves no residue for later rows.
- **`ppEqFunctionItem` living inside the `equation.c` override** (30 of its 609
  added lines) looks like an extraction candidate, but `functionAlias` is
  `TO_QSPI static const` and file-local, so the function cannot move without a
  second wrapper. Inline is cheaper.
- **The 529-line PP14/PP16 construct block inside `solver/equation.c`** is known
  and already scheduled by the 2026-08-27 review (*"When: rebase-adjacent stage
  work, never mid-audit"*).

### Doc drift and below the bar

- **`prettyTest.c:3356` (EQ4d) still says "`ppfPowBase` reads the run."** It
  does not since `b27394db6`; `ppfRunPrec` does, and EQ4d is now the only pin on
  that branch. The row still fails on the right defect, so this is stale prose.
- **Two rows named T25 and two named T26** (`:1514`/`:1758`, `:1474`/`:1788`),
  and `prettyCapture.c:225` now names two rows at once. The failure strings are
  distinguishable, so this is naming — recorded only because this file already
  paid for a duplicate-name defect once.
- **Base-mode literals over-bracket.** `1F#16` typed in integer mode reaches
  `PPN_LIT` as `"1F#16"` and now draws `(1F#16)·2`. Every consumer of the leaf
  precedence can only **add** a bracket, so the class makes a picture noisier,
  never wrong. Cosmetic; the same family as `PP18RR7-6` and not worth a second
  finding.
- **The packaging-rule clause in the `equation.c` and `screen.c` override
  comments** (*"so the patch carries no whitespace-only hunk"*) arguably breaks
  the contracts-only comment bar, but in an override file it is the fact that
  stops the next reader "fixing" the indentation.

---

## 7. Verdict

**Do not ship this tip as the base for the announcement.** One finding is
enough on its own: `PP18RR7-1` puts a wrong picture next to a correct result on
a gesture an owner reaches by accident (`5 [+/-] [x²]`, then anything that
displaces the formula, then PHIST), it is visible on the two surfaces the
package exists to provide, and half of it is a regression created inside this
wave. It is a small fix — the two leaf arms ask the predicate the other
producers already ask — but it needs the surface-equality pin with it, or the
next wave re-opens it at the sixth producer.

**Where it breaks first.** In order of what an owner will hit: the PHIST row
for anything negated or scientific (`PP18RR7-1`); a base-2 word disappearing
from the browser behind *"too large to show"* (`PP18RR7-2`); then nothing else
for most owners, because `PP18RR7-3` needs a 507-character equation and
`PP18RR7-6` needs a non-default group separator.

**What I would leave alone if the goal were correct code rather than a clean
audit.** `PP18RR7-6` — the apostrophe separator is a setting almost nobody
changes, and the cost is a redundant bracket, not a wrong one; fix it when the
predicate is next touched. `PP18RR7-8` — real, and cheap, but it is a document,
and the rebaser it protects is the person who will notice. `PP18RR7-7` — there
is no defect today; the honest minimum is one sentence of caller obligation on
the header line, not a refactor. `PP18RR7-5`'s code half — the write is safe as
written and every other option costs a buffer; amend the rule to name the
exception and stop there. `PP18RR7-3` — I would fix the latch, because it is
one line and the class is exactly the one round 6 fixed one frame up, but I
would not build the parser property test for a case that needs 507 characters.

**What must be fixed:** `PP18RR7-1` with `PP18RR7-4`'s pin, `PP18RR7-2`, and
`PP18RR7-9` (five rows in the gate currently test something other than what
their comments claim).

**On the wave itself.** The repair was right and the round-6 findings it closed
are closed on the surfaces it touched. It failed the same way the previous two
waves failed: the fix was verified against the finding's reaching input rather
than against the class the fix redefined. The count of confirmed findings since
the restart now reads 16, 17, 15, 14, 7, 7, **9** — it stopped falling at round
5 and turned up on a wave that was pure repair. That is the audit-fix-regression
rate the project already records, and it will not fall while every round's
subject is the previous round's fixes.

---

## 8. Round and exit state

**Round 7 of the restarted series.** Readers: eight in-family dimensions
(contracts, lifecycle, arithmetic, errorpaths, guards, tests, design, upstream),
one independent refutation pass per finding with mutation authority, and **two
out-of-family families**:

| family | packet | reply | `MODEL:` line, verbatim | raised | survived refutation |
|---|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r7/packet-sol-latch.md` | `/tmp/pp18-r7/packet-sol-latch.sol.reply.md` | `MODEL: GPT-5` | 1 | 1 (`PP18RR7-3`) |
| gemini / gemini | `/tmp/pp18-r7/packet-gemini-leaf.md` | `/tmp/pp18-r7/packet-gemini-leaf.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 2 | 1 (folded into `PP18RR7-1`) |

Both replies were read in full; both carry a `MODEL:` line; neither is empty or
truncated. GPT-5's finding needed its reaching input rebuilt (its literal
512-digit string cannot overflow the pool; the pre-loaded-pool variant does) and
survived. Gemini's second finding was refuted against four rulings and a
mutation; its properly-scoped residue is filed as `PP18RR7-7`. Gemini's first
finding is the same defect four in-family dimensions found independently.

**The out-of-family drought is over.** Both families ran **inside this round**,
against the audited ref. The counter that stood at seven consecutive
single-family rounds resets here.

**Exit criterion: NOT MET.** It asks for a round with all three families and no
confirmed findings on the actual subject. This round had two of three families
and nine confirmed findings, so it fails on both counts. Clean rounds to date:
zero.

**What the next round needs, in order.**

1. **The fix wave for `PP18RR7-1`, `-2`, `-4` and `-9`**, and then a round on
   that wave — the series cannot close on a round whose subject contains fixes,
   so it needs a wave to land, a round on the wave, and then a round on nothing
   new. The third of those is the one that can be clean.
2. **The third family** (anthropic-external, or whichever of the three did not
   run here), inside the round, against a frozen ref, with the packet path,
   reply path and verbatim `MODEL:` line recorded in §1 as they are above.
3. **The axis, if the pattern is to be tested rather than assumed:**
   `prettyCapture.c` and `prettyValue.c`, outside the file set for four
   consecutive rounds and holding the producer half of two of this round's
   findings; and the **surface-equality question** — for every formula the
   package can draw, do the live row, the filed row, the T line and the browser
   row agree? `PP18RR7-1` answers "no" at one site, and nothing in the suite
   asks it generally.
4. **Two process items that cost this round real time:** `CLAUDE.md` still names
   `./packages/forth-core/build-test.sh` as the gate, which fails green for this
   package (D5); and eight of eight verifier worktrees spawned 140 commits off
   the audited tip.
