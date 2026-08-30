# Audit — PP18 round 6 (restarted series), THE WAVE THAT CLOSED ROUND 5, at `aafd38f7d`

Subject: `9de22ac7d..HEAD` on `pretty-print/stage-pp18`, HEAD frozen at `aafd38f7d`
when the round was dispatched. **Three commits**: one docs commit carrying the
round-4 and round-5 reports, one repair commit closing eight findings, one
merge-surface commit closing the ninth. Reading restricted to the upstream-PR
file set — `prettyVisual.c`, `prettyLayout.c`, `prettyFormula.c`,
`prettyEquation.c`, `prettyInternal.h`, `prettyPrint.h`, `prettyTest.c`,
`screen.c`.

The question set was whether the fix-regression rate falls when a wave is asked
to close a round that was itself about fix regressions. Rounds 2–5 found 4 of 7,
4 of 4, 8 of 14 and 6 of 7 of their findings inside the previous wave's own
repairs. **This round: 6 of 7.** The rate has not fallen in five consecutive
waves, and it has now survived every variable that was supposed to move it —
feature content, pure repair, mutation-verified pins, and a wave whose author had
just read a report saying exactly this.

**The single most expensive thing found is not a fix regression at all.**
`ppfFormatStaged` hands `shortIntegerToDisplayString` a 200-byte stack buffer
whose builder's *first write* lands at index 256. It predates the wave by four
stages, it is reached by `10 ENTER 5 +` in BASE mode followed by `PHIST`, and
round 3 cleared it in prose — *"both with ample margin at their producers' digit
counts"* — while a sibling package in this same repository carries a
`_Static_assert` naming the class. The gate is green while the write executes
three times per history row.

**The most expensive fix regression is the one the wave was proudest of.**
`ppfPowBase` is now the single named decision point for whether a power's base
needs brackets, and `prettyInternal.h` was rewritten to say so. It enumerates
shapes rather than testing the class: a capture leaf reports `PPF_PREC_ATOM` for
display text that can be `-5`, so `5` `+/-` `x²` draws `-5²` for a value of 25 —
while the walker, one file away, already demotes a leading minus to
`PPF_PREC_ADD` with the comment *"a signed numeral brackets as a term"* and draws
`(-5)²` for the identical program. Two surfaces, same expression, different
mathematics.

**Seven CONFIRMED, zero PLAUSIBLE, five REFUTED.** Twenty-four findings were
raised across eight in-family dimensions and sixteen went to refutation;
convergence was heavy (four dimensions independently reported the same wrong
theory about the new class oracle, and it was killed four times by four separate
mutations; two reported the signed-leaf divergence from opposite sides), so they
collapse to **twelve distinct claims**. **Six of the seven CONFIRMED are backed by a mutation or an
instrumented probe** applied in an isolated worktree, built through the package's
own gate with presence verified in `build.sim/custom_pkg_shadow/*` or
`packages/pretty-print/files/*`, observed in `build.sim/meson-logs/testlog.txt`,
and reverted.

Nothing was fixed. The tree is clean and the gate is green.

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).

### Subject

**Tip.** `aafd38f7d` ("pkg: the wrapped-x guard stops re-indenting the upstream
lines it encloses"). Range `9de22ac7d..aafd38f7d`: **three commits**, +4,064 /
−121 across 15 paths, of which 3,235 added lines are the two audit reports.

| commit | subject | closes |
|---|---|---|
| `809ebab39` | the round 4 and 5 audit reports the fix commits cite | `PP18RR5-D6` |
| `10cc180bc` | round 5 — the stacked-power rule had a second producer, and a third | `PP18RR5-1`…`-6`, `-P1`, `PP18RR4-4` |
| `aafd38f7d` | the wrapped-x guard stops re-indenting the upstream lines it encloses | `PP18RR5-7` |

**Correction to the brief.** The brief says "all seven of round 5's findings plus
two that were open from round 4". Nine items are closed, but only **one** of them
is a round-4 finding: `PP18RR4-4` (the `ITM_FILL` arm). The ninth closure is
`PP18RR5-P1`, round 5's *plausible*, which the wave retired by construction
rather than by measurement. **`PP18RR4-8` remains open** — nothing in the suite
drives the `ppqShowRender` branch `PP18RR3-5` changed — and `prettyEquation.c`
moved by exactly one line in this range (`ppqScopeOperand` wrapped in
`ppfPowBase`), so `-8` is not merely unfixed, it is unexamined for the second
consecutive wave.

**In-scope file set and its delta over the range** (flat working area; the
generated `files/` twins carry the identical numbers and the gate's refresh left
the tree clean, so the build read what was read here):

| file | +/− | note |
|---|---|---|
| `prettyTest.c` | +311 / −8 | `ppcTestNoteLabel`, three test helpers, T25, EQ4d, V-DECL, V-FILL's second arm, V-XEQ's second mode |
| `prettyFormula.c` | +48 / −12 | `ppfPowBase` extracted; both `PP_SUP` arms rerouted |
| `prettyVisual.c` | +34 / −15 | `PPV_DECLARATION_ITEMS`, the `ITM_FILL` rewrite, the Z/T chrome clear |
| `screen.c` (override) | +11 / −14 | the no-reindent wrap; behaviour-identical |
| `prettyInternal.h` | +3 / −2 | the builder banner's new ownership sentence + the `ppfPowBase` declaration |
| `prettyEquation.c` | +1 / −1 | `ppqFactor`'s `^` arm routed through `ppfPowBase` |

`prettyLayout.c` and `prettyPrint.h` are untouched in the range.

**The tree moved under the audit, and it moved in the predicate this round is
about.** HEAD is now `45d07a4ea`; two commits landed after the frozen tip.
`7ddff2c5f` is a **round-6 out-of-family fix wave** — the first out-of-family
reader in six rounds — which widened `ppfPowBase` a third time to match a
trailing ASCII `[eE][+-]?digits`, because a typed literal keeps the text the
owner typed and never passes the display formatter. That is the *fourth* alphabet
in the same predicate in two waves, and **both of this round's wrong-drawing
findings survive it**: verified at `45d07a4ea`, `-5` falls out of the ASCII arm
at `d > 1` and the degree-tagged run falls out at the digit scan. The two
findings are live at HEAD, not just at the audited tip. `7ddff2c5f` also repaired
`ppqFactor`'s lost `c->failed`, which three of this round's dimensions had
already cleared as unreachable.

**KNOWN, excluded from re-reporting** (verified still open or ruled, then
fenced): `PP18RR4-8`, `PP18RR4-2`'s drawn text, `PP18RR4-11/-12`'s unpinnable
clamp, `PP18RR3-3`, `PP18RR3-4`, `PP18RR2-2`, and every finding from the
`PP18RR5-*`, `PP18RR4-*`, `PP18RR3-*`, `PP18RR2-*`, `PP18RR1-*` and pre-restart
`PP18-*` series with their rulings.

**Numbering.** `PP18RR6-1`–`PP18RR6-7`, design observations `PP18RR6-D1`–`D6`.
There are no plausibles. `grep -rn PP18RR6` over the repository returned nothing
before this file was written.

### Coverage (union across the eight in-family dimensions)

**Read in full by three or more dimensions:** the entire `git log -p
9de22ac7d..HEAD` with all three messages; `prettyFormula.c` (`ppfValBuf`,
`ppfStageValFields`, `ppfFormatStaged`, `ppfParen`/`ppfWrapIf`/`ppfRun`,
**all of `ppfPowBase` glyph by glyph**, both `PP_SUP` arms, `ppfBuildOp1` and
`ppfBuildOp2` every case, `ppfFromCaptureNode` every arm, `ppfBuildEntry`'s whole
token loop, `ppfBuildCurrent`/`ppfBuildRow`/`fnPrettyHist`); `prettyVisual.c`
(`ppvLiveStackSlots`, `ppvPush`/`ppvPushLifting`/`ppvPop`, `ppvRefillFromT`,
`ppvBody`, `ppvLiftNeutral`, `ppvStep` and **every arm** of `ppvStepArm`,
`ppvAstToNodes` including the `PPA_LIT` and `PPA_OP1` arms, both paint surfaces,
`fnPrettyVisual` end to end with both refusal arms); `prettyEquation.c`
(`ppqNumber`, `ppqName`, `ppqPrimary`, `ppqFactor` **both** branches,
`ppqScopeOperand`, `ppqUnwrapParen`, `ppqTerm`, `ppqExpr`, `ppqParse`);
`prettyInternal.h`; `prettyTest.c`'s whole added block (`ppcTestNoteLabel`,
`ppcTestWriteAndLoadPgm`, `ppfTestRunEndsSup`, `ppfTestFirstRunText`,
`ppfTestPowersScoped`, T25's five rows, EQ4d, V-DECL, V-FILL, V-XEQ) plus the
pins they lean on or collide with (S3, T23c, T24c, V19/V20/V27/V51/V67/V72/V36b,
V-MODE4, EQ7); `screen.c`'s changed hunk with its enclosing upstream function.

**Out of the PR file set, read where a call path led in:** `prettyCapture.c`
(`ppcClassify`, `prettyNoteNimText`, `prettyNoteNumberCommit`,
`ppcValLeafFromRegister`, `ppcRclLeaf`, `ppcEmit`'s result snapshot,
`ppcScopeOk`, `ppcHistoryEntry`), `prettyValue.c` (`ppParseExponent`,
`ppParseRealAny`, `ppBuildRegister`, `prettyTryRegisterLine`, and the whole
consumer chain in both directions), `items.c`'s `prettyNoteFunction` hook,
`browsers/prettyBrowser.c` not at all.

**Upstream verified by execution path, not assumed:** `display.c`
`real34ToDisplayString`/`real34ToDisplayString2`, `angle34ToDisplayString2`'s
four unit arms, `exponentToDisplayString`, `supNumberToDisplayString`,
`complex34ToDisplayString2` in both `CPXMULT` states, and
**`shortIntegerToDisplayString` in full**; `fonts.h` glyph blocks `0xa000`–
`0xa00f`, `0xa080`–`0xa089`, `0xa160`–`0xa16b`, `0xa460`–`0xa46f`, `STD_DEGREE`,
`STD_SUP_BOLD_r/g`, `STD_SUP_pir`; `defines.h` `ERROR_MESSAGE_LENGTH`,
`TMP_STR_LENGTH`, `getStackTop`, the `SCRUPD_*` bits; `items.c` SLS/PTP rows for
every op the walker dispatches; `lblGtoXeq.c` `executeOneStep` (the
`PTP_DECLARE_LABEL` and `PTP_REM` early returns), `fnKeyEnter`'s eRPN branch;
`stack.c` `fnFillStack`; `bufferize.c` `closeNim` and the `ITM_CHS` NIM edit;
`arcsin.c`'s angular-mode reallocation; `screen.c` `_refreshNormalScreen`,
`RETURN_NORMAL`, `closeShowMenu`, `_selectiveClearScreen`; `statusBar.c`
`refreshStatusBar`'s calcMode self-guard; `softmenus.c` `createHOME`/`createPFN`
(the only `= ~SCRUPD_AUTO` writers in the tree) and `showSoftmenuCurrentPart`;
`programming/input.c` `fnPause`'s four `refreshScreen` sites; `charString.c`
`stringNextGlyph`.

**Machine-checked rather than eyeballed:** every `ppNewBox(PP_SUP` site in the
package (four); every reader and writer of `SCRUPD_MANUAL_MENU` /
`SCRUPD_MANUAL_SHIFT_STATUS` / `SCRUPD_SKIP_MENU_ONE_TIME` in `src/` and
`packages/`; a parse of all 85 `ppcTestWriteAndLoadPgm` call sites resolved to
their fixture arrays and replayed through `ppcTestNoteLabel`'s recording order;
`patch_churn_scan.py` over all thirteen pretty-print patches.

### Not reached, and it matters where

- **No simulator ran and no LCD photograph backs any finding.** Every picture
  claim is a layout-signature string from the harness or an argument from
  `ppMeasure`/`ppPaint` arithmetic. `PP18RR6-2` and `-3` are both proved as
  *node trees* (`S(-5|2)` with no `P(...)`), not as pixels.
- **`PP18RR6-1` was not observed to crash.** The out-of-bounds write was
  measured — highest byte written = 256 against a buffer ending at 199 — but the
  x86 simulator frame absorbed it and the drawn text came back correct. It is
  undefined behaviour with a proved reaching path, not a reproduced fault, and
  the DM42n ARM frame is not the simulator's.
- **`prettyCapture.c` and `prettyValue.c` are outside the PR file set for the
  third consecutive round.** They were read only along traces from in-scope
  sites. `PP18RR6-2`'s producer half lives in `prettyCapture.c`.
- **`prettyLayout.c` was not audited.** No in-range change, and the `PP_SUP`
  measure/paint arithmetic that makes a missing bracket *invisible* was taken
  from round 5's derivation rather than re-derived.
- **`prettyTest.c`'s ~5,900 lines were sampled, not swept.** The new and changed
  pins and their neighbours were read; the B-series and the P-series past P6 were
  not.
- **`design-audit.sh` is forth-core's.** There is still no pretty-print
  equivalent, so no override-budget check ran; §2's churn scan is the substitute.
- **Flash was not re-measured.** `10cc180bc` records `+144 B` on `make dmcp5r47`
  and `RAM unchanged at 11188`; this round did not build the device target to
  confirm it.

### Out-of-family accounting

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Sixth consecutive in-family round with no out-of-family reader** — with a
wrinkle worth recording. An out-of-family pass over this same subject *did* run,
outside this round's workflow, and landed as `7ddff2c5f` while this round's
verifiers were working against the frozen tip. Its two confirmed findings (the
ASCII exponent alphabet; `ppqFactor`'s lost `c->failed`) are disjoint from this
round's seven, which is a mild independent corroboration that neither family is
covering the other's ground. It does not change the accounting: the reader
answered a different packet, its findings were fixed before this report was
written, and the exit criterion counts rounds, not commits. `PP18RR6-D5` records
the sequencing hazard.

---

## 2. Mechanical results

**Gate: GREEN.** `./packages/pretty-print/build-test.sh --solo`, run by this
synthesis pass on the main tree at `45d07a4ea`: `testSuite EXIT STATUS: 0`,
`1/1 testSuite OK 168.71s`, `Fail: 0`, `PRETTY-PRINT GATE GREEN`,
`grep -c "test FAIL" build.sim/meson-logs/testlog.txt` = **0**. Verifier worktrees ran
the same gate independently at `aafd38f7d` (168–327 s) and each re-ran it after
reverting its probe. `git status --porcelain` is empty; no
`AUDIT-PROBE` marker survives anywhere in `packages/`.

**Warnings: two, both upstream's own text.**
`build.sim/custom_pkg_shadow/testSuite/testSuite.c:5498` and `:5506`,
`'%s' directive writing up to 1999 bytes into a region of size 404`
`[-Wformat-overflow=]`, byte-identical to `src/testSuite/testSuite.c`. Unchanged
from round 5, outside the subject range.

**Upstream churn: closed to the standing baseline.** `patch_churn_scan.py` over
all thirteen pretty-print patches reports **1** mechanical finding, the
pre-existing `010-solver__equation.c.patch` `[WS-ONLY]` line the package's own
minimality review already catalogued. `010-screen.c.patch` is now 49 adds / 4
dels / 5 hunks with **zero** findings, against 5 findings at `9de22ac7d`. That is
`PP18RR5-7` closed exactly as specified, and `aafd38f7d` is behaviour-identical:
`git diff -w -B` over the range shows only a reworded comment.

**The design record moved once, and not where it counts.** `809ebab39` committed
the round-4 and round-5 reports, which closes `PP18RR5-D6` — twelve commits no
longer cite a document that exists nowhere. But
`git log 9de22ac7d..aafd38f7d -- design-docs/pretty-print/` is **empty** for the
third consecutive wave, and `DESIGN.md:565-574` now actively contradicts the code
it describes (`PP18RR6-7`). `grep -rn "ppfPowBase\|saturat\|SSIZE\|PPV_DECLARATION"
design-docs/pretty-print/` still returns nothing.

**Flash and RAM: recorded, for the first time in three waves.** `10cc180bc`
carries *"Flash +144 B measured on make dmcp5r47 against 9de22ac7d with the
package; RAM unchanged at 11188"*. Round 5 reported zero of seven commits
complying; this wave is one of three, and the one that changed code. Credit where
it is due.

**Probes and mutations this round** — all applied in isolated worktrees, built
through the real refresh, presence verified in the built artifact, observed in
`testlog.txt`, and reverted.

| probe / mutation | observed result | finding |
|---|---|---|
| `ppcTestType("5")` + `ITM_CHS` + `ITM_SQUARE` → `ppfBuildCurrent`, print the signature | `aimBuffer='-5'`, `capture sig='-5 x2'`, `layout sig='S(-5|2)'` — a `PP_SUP` over an unbracketed `-5` run | **`PP18RR6-2`** |
| `setRegisterTag(REGISTER_Y, amDegree)` inserted into the wave's own PP18RR5-3 row | `leaf=312e80d7a47da165a160a00a80b0`, `rootkind=PP_SUP`, `basekind=PP_RUN`, **`scoped=1`** | **`PP18RR6-3`** |
| `10 ENTER 5 +` in base 16, filed, then `ppfBuildEntry`, with `buf` enlarged and poisoned | `shortArmHits=3 highestByteWritten=256 (buf[200] would end at 199)` | **`PP18RR6-1`** |
| the same driver against the pristine `char buf[200]` | `ppfBuildEntry=1 root=5` — gate GREEN, drawn text correct, write still out of bounds | **`PP18RR6-1`** (refutes its own "text is garbage" clause) |
| `else { ppTestFail("overflow, name not recorded: %s") }` added to `ppcTestNoteLabel`'s cap | RED with **37 distinct names** dropped, including `VXA`, `VXB`, `VXI`, `VXD`, `VXP`, `VCUB`, `VPOW`, `VPX` | **`PP18RR6-4`** |
| two synthetic fixtures with divergent bytes, one under a recorded name (`VIX`), one under a dropped name (`VXI`) | exactly ONE failure: *"label VIX is defined by two different fixtures"*; `VXI` silent | **`PP18RR6-4`** |
| `ppfPowBase` call deleted from `ppqFactor`'s `^` arm (the third producer) | RED with exactly ONE failure — `EQ4d`, a hand-written row. **No T25 assertion moved.** | **`PP18RR6-5`** |
| the Z/T chrome clear (`prettyVisual.c:1649`) deleted outright | gate **GREEN**, `1/1 testSuite OK 174.37s` | **`PP18RR6-6`** |
| `x = PP_NONE;` injected into the walker's surviving stacked-power branch | RED, sole failure `V51 stacked power brackets its base` — the branch **executes** | **`PP18RR6-7`** |
| the walker's whole stacked-power block (`prettyVisual.c:1137-1142`) deleted | gate **GREEN** — the branch is **inert**; `ppfPowBase` already brackets | **`PP18RR6-7`** |
| `isPower` window narrowed to `0xa160..0xa164`, production only, test copy untouched | RED, sole failure `EQ4d` (raised **by `ppfTestPowersScoped`**) — control | **REFUTES** the "oracle is a copy" claim, four times |
| `isPower = (last == 0xffff)`, i.e. `PP18RR5-3` reinstated exactly | RED: `EQ4d` **and** `T25 a squared scientific value…` | **REFUTES** the same claim again |

Two of those deserve to be read together. The chrome clear — the one repair in
this wave that changes what a shared global holds when the surface returns — can
be deleted with the gate staying green. The walker's stacked-power block can also
be deleted with the gate staying green, but corrupting it turns V51 red. Same
green, opposite meanings: one is uncovered, the other is redundant. Only a
mutation tells them apart, which is why `PP18RR6-6` and `PP18RR6-7` are different
findings with different severities.

**Two runner traps this round earned, both new, both capable of producing a
confident wrong verdict.**

1. **The wrong gate gives a vacuous green.** Two verifiers ran
   `./packages/forth-core/build-test.sh`, which hardcodes
   `PKG="packages/forth-core"`. `pkg_patch_refresh.py` therefore never
   regenerated pretty-print's `patches/`+`files/`, the shadow contains no
   `prettyFormula.c` at all, and a mutation on it produced a green that measured
   code which was never built. Both caught it — one by `find build.sim -name
   prettyFormula.c` coming back empty, one by grepping the probe out of
   `packages/pretty-print/files/` — and re-ran under
   `./packages/pretty-print/build-test.sh`. CLAUDE.md names the forth-core gate;
   for this package the governing gate is the pretty-print one, and a mutation is
   only evidence after its presence in the built artifact is verified.
2. **A shared `/tmp` log was clobbered by a sibling verifier.** One agent wrote
   its mutated-gate output to `/tmp/gate_mut.log`, read back a log whose paths
   pointed at another worktree, and discarded the result rather than reporting
   it. Probe output goes to an in-worktree path.

The stale-ref trap `CODE_AUDIT.md` records is now at **seven consecutive
rounds**: every verifier spawned at `e21af8d28`, `git merge-base --is-ancestor
aafd38f7d HEAD` failed for all of them, and every one checked out the audited tip
before its first read. The mandated first action is doing its job and the runner
defect behind it has still not been fixed.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. An out-of-bounds stack write on a
common gesture outranks a wrong drawing three keystrokes away, which outranks the
same wrong drawing behind a longer gesture, which outranks a harness guard that
covers half its ground, which outranks a coverage claim that will mislead the
next author, which outranks an unpinned repair, which outranks a rule with two
homes and three contradicting descriptions.

Where the refutation pass corrected a finder, the correction is stated **inside**
the finding. Where two dimensions reported the same defect from different sides,
they are merged and the merge is declared. Each carries `file:line` at the
audited tip, what breaks, the concrete reaching input, the violated contract
quoted, the bug class, and the class-level test. **No patches.**

Line numbers are at `aafd38f7d`. `prettyFormula.c` shifted by roughly +25 lines
at `45d07a4ea`; where a finding is live at HEAD it says so.

---

### PP18RR6-1 — `ppfFormatStaged` hands `shortIntegerToDisplayString` a 200-byte buffer, and that builder's first write lands at index 256

`packages/pretty-print/prettyFormula.c:40` (the buffer) against `:56` (the call).
**NOT introduced by this wave** — `git log -L 39,60:packages/pretty-print/prettyFormula.c`
attributes both lines to `f94c21e16` (PP4). It is reported here because it is
live at the audited tip, it is the worst thing in the file set, and **round 3
cleared it by inspection**.

**What breaks.** `src/c47/display.c:2056` is `i = ERROR_MESSAGE_LENGTH / 2;`, and
every subsequent write in that function is `displayString + i`. `defines.h:2513`
defines `ERROR_MESSAGE_LENGTH` as 512, so the builder's scratch region starts at
index **256** and grows upward — up to roughly 80 bytes more for a 64-bit binary
value with `LEAD0` separators — before compacting the digits back to the front.
`prettyFormula.c:40` declares `char buf[200]`. The first byte written is 56 bytes
past the end of a stack array.

**Reaching input**, constructed and executed:

```
BASE menu → #16 (or #10)
10 ENTER 5 +        → the formula is captured; the filed entry carries
                       PPT_TKV dataType 0x08 (dtShortInteger) twice and a
                       PPT_TKRES dataType 0x08 for the result
CLSTK (or start the next calculation)   → the formula is displaced and filed
PHIST               → fnPrettyHist → ppfBuildRow → ppfBuildEntry →
                       the PPT_TKRES arm → ppfStageValFields → ppfFormatStaged
```

Nothing filters the type on the way in. `items.c:413` calls
`prettyNoteFunction(func, param)` from the generic item dispatcher, so `ITM_ADD`
is staged regardless of operand type; `ppcScopeOk` (`prettyCapture.c:666-671`)
gates only on `programRunStop`, `FLAG_SOLVING`, `FLAG_INTING` and `calcMode`;
`ppcValLeafFromRegister` (`:218-262`) rejects only the two matrix types and
oversize payloads, and a `dtShortInteger` is 8 bytes. The same arm is reached for
OPERAND leaves through `prettyFormula.c:469` whenever a short-integer register is
snapshotted (`ppcEnsureKnown`, `PPC_LASTX`, `PPC_XSWAPREG`, `ppcRclLeaf` for a
lettered register).

**Evidence.** Instrumented run in a worktree at `aafd38f7d`: `buf` enlarged to
1000 bytes, memset to `0x5A`, a counter in the `dtShortInteger` arm, and a scan
for the highest non-`0x5A` byte after the call.
`build.sim/meson-logs/testlog.txt`:

```
AUDIT-PROBE R6: after fnChangeBase(16) X dataType=8
AUDIT-PROBE R6: history entry=… 07 08 10 00 00 08 0f 00 …
AUDIT-PROBE R6: ppfBuildEntry=1 shortArmHits=3 highestByteWritten=256
                (buf[200] would end at 199)
```

`0x07` = `PPT_TKRES`, dataType `0x08` = `dtShortInteger`, tag `0x10` = base 16,
`bytes 0x08`, payload `0f` = 15. **Three** out-of-bounds writes per history row —
two operands and the result — with a two-digit hex value; a 64-bit word writes
considerably further. The pristine `buf[200]` control run reported
`ppfBuildEntry=1 root=5`: the drawn text came back **correct** and the gate was
green, which refutes the finder's secondary claim that the output is garbage and
leaves the fault undefined behaviour rather than an observed crash on x86.

**Violated.** This project has already named and paid for this exact class.
`packages/forth-core/forth_bridge.c:290-308`:

> *"`shortIntegerToDisplayString` does NOT write from the front: it builds digits
> from `displayString[ERROR_MESSAGE_LENGTH / 2]` upward as scratch, then compacts
> them to the front, so the buffer it is handed must be at least
> `ERROR_MESSAGE_LENGTH` bytes. The 256-byte local `buf` is too small for that"*

— and it carries a `_Static_assert(TMP_STR_LENGTH >= ERROR_MESSAGE_LENGTH,
"C1/C22: …")` so the constraint cannot silently regress. Every upstream caller
passes `tmpString` (2560) or `errorMessage` (512). `prettyFormula.c` passes 200.
And round 3's own clearance, at
`AUDIT_PP18-round-3-…_2026-08-29-r3.md:1739-1740`: *"`ppfFormatStaged`'s
`buf[200]` and `prettyValue.c`'s `ppLeafScratch[200]`, both with ample margin at
their producers' digit counts."* The margin argument is right for the three arms
that format from the front and irrelevant for the one that does not.

**Bug class.** A buffer sized against the *output* of a formatter that is
specified against its *scratch space* — and a clearance that reasoned about digit
counts without reading the builder. Cross-package: the class is documented, and
the document lives in a sibling package that this file's author had no reason to
open.

**Class-level test.** Enumerate the display builders this package calls and
assert each one's buffer contract at compile time, exactly as `forth_bridge.c`
does: a `_Static_assert(sizeof(buf) >= ERROR_MESSAGE_LENGTH)` beside every call
to a builder that writes from `ERROR_MESSAGE_LENGTH/2`. There are two such
buffers in the package (`prettyFormula.c:40`, `prettyValue.c`'s
`ppLeafScratch[200]`) and four builders behind them; the enumeration is small and
it is checkable without running anything. A behavioural companion is also cheap
and currently missing entirely: `grep dtShortInteger packages/pretty-print/prettyTest.c`
returns nothing, so no pin puts an integer-mode value through `ppfBuildEntry` at
all.

---

### PP18RR6-2 — the capture leaf reports `PPF_PREC_ATOM` for text that reads as a term, so `5` `+/-` `x²` draws `-5²` for a value of 25 — and VISUAL draws `(-5)²` for the same program

`packages/pretty-print/prettyFormula.c:103` (`ppfPowBase`'s enumeration) and
`:431`/`:452` (the leaf's `*outPrec`), against `packages/pretty-print/prettyVisual.c:1121`.
**Two dimensions reported this from opposite sides and both survived; they are
one defect and are merged here.** One framed it as `ppfPowBase`'s rule missing a
member of its own class; the other framed it as the two leaf builders disagreeing
about the same numeral. The second framing is the one that says where the fix
belongs. **Live at HEAD**: the ASCII arm added by `7ddff2c5f` requires
`d > 1` after the sign strip, and `-5` leaves `d == 0`.

**What breaks.** `ppfFromCaptureNode` sets `*outPrec = PPF_PREC_ATOM` on entry
(`:431`) and the `PPN_LIT` arm (`:437-451`) returns `ppfRun(text, ctxFont)`
without ever revising it. The text can be `-5`. `ppfBuildOp1`'s SQUARE arm calls
`ppfPowBase(a, PPF_PREC_ATOM, ctxFont)`; the node is a `PP_RUN`, its last inked
glyph is `'5'` (0x35), not in `0xa160..0xa16b`, so the fallback is
`ppfWrapIf(a, ATOM, PPF_PREC_ATOM)` — and `ATOM < ATOM` is false, so nothing is
inserted. The result is `SUP(RUN"-5", RUN"2")` with no `PP_PAREN`.

**Reaching input**, driven through the real dispatch and measured:

```
5  +/-  x²   then PHIST      → X = 25, drawn  -5²
```

`bufferize.c:1272-1284` — `ITM_CHS` during NIM flips `aimBuffer[0]` between `'+'`
and `'-'`; the NIM buffer is sign-prefixed. `bufferize.c:2345` — `closeNim` calls
`prettyNoteNimText(aimBuffer)` before any mutation. `prettyCapture.c:1180-1182` —
`if(*s == '+') { s++; }`; **only the plus is stripped**, so `ppcNimText` is
`"-5"`. `prettyCapture.c:1226-1236` — `prettyNoteNumberCommit` copies that text
verbatim into a `PPN_LIT` payload. `prettyCapture.c:543` — `ITM_SQUARE` is a
one-operand op, so the tree is `OP1(SQUARE, LIT"-5")`.

Probe output from the worktree gate, `testlog.txt:429-432`:

```
AUDIT-PROBE R6 aimBuffer='-5'
AUDIT-PROBE R6 capture sig='-5 x2'
AUDIT-PROBE R6 layout sig='S(-5|2)'
```

Compare the wave's own pins at `prettyTest.c:1472-1474`, every one of which
expects `"S(P(S(3|2))|2)"`. `S(-5|2)` has no `P(...)`.

**Not browser-only.** `ppfBuildCurrent` has two non-test callers:
`prettyFormula.c:732` (inside `ppfBuildRow`, "row 0 = the current formula when
open", the PHIST pager) and `prettyValue.c:826` (the live T-line formula under
`FLAG_PTLINE`). Both draw the unbracketed picture.

**The same class through the other leaf builders.** `5 +/- STO Y`, `RCL Y`, `x²`
— the T25 scientific row's own recipe — gives a `PPN_VAL` formatted by
`real34ToDisplayString`, whose output for a negative value also begins with `-`.
A complex leaf formatted by `complex34ToDisplayString` carries an infix `+`/`-`
joiner (`display.c:1552-1596`) and reports `ATOM` identically. The `PPN_VAL` arm
(`:448+`) never inspects the sign either, so this is the whole capture leaf set,
not one slipped arm.

**The divergence.** `prettyVisual.c:1118-1122`, the walker's `PPA_LIT` arm:

```c
if(a->textLen > 0 && ctx->pool[a->textOff] == '-') {
  *outPrec = PPF_PREC_ADD;   // a signed numeral brackets as a term
}
```

`PPF_PREC_ADD` is 1, `PPF_PREC_ATOM` is 3, so `ppfWrapIf` parenthesises and
VISUAL draws `(-5)²` for `LBL P: literal "-5", x²`. The codebase already contains
the missing class member, in this codebase's own words, one file away.

**Violated.**
- `prettyInternal.h:120-123`, rewritten by this wave: *"The one shape that level
  would have covered — a power whose base is itself a power — is handled by
  `ppfPowBase`, which every producer of a `PP_SUP` calls"*.
- `prettyFormula.c:99-100`, written by this wave: *"Two shapes count as 'already
  a power' — a `PP_SUP` node, and a run whose text ends in superscript glyphs."*
  The class is not "already a power"; it is "the base run is not a visual atom",
  and `prettyVisual.c:1121` states the missing member.
- `DESIGN-HISTORY.md:1080-1081`: *"One constructor pair (`ppfCombine1/2`) serves
  both the live tree and the token stream, so the two paths cannot drift
  typographically."* The shared constructors cannot stop this drift, because the
  drift is in the `aPrec` the two leaf builders hand them.
- `DESIGN.md:565-574`: *"**Nothing in the walker decides where a bracket goes.**"*
  followed by exactly **two** enumerated node-vs-text exceptions (the scoping
  fraction bar V49, the stacked-power base V51). The signed-numeral rule is an
  unlisted third, and nothing licenses the capture side to disagree with it.

**The wave's contribution, stated precisely.** `ppfPowBase` is new in `10cc180bc`
and the pre-wave `ITM_SQUARE` arm had the identical `ppfWrapIf(a, aPrec,
PPF_PREC_ATOM)` fallback, so the wave did not create the hole. What it did was
consolidate three call sites onto one fallback that **trusts a number one of its
two producers computes wrong**, and write a header sentence saying the shape is
handled. Before the wave this was an unstated gap a reader might find; after it,
it is covered by a written guarantee.

**Searched for a ruling and found none.** `grep` over all of `design-docs/` for
"signed numeral | negative literal | leading minus | negative base | as a term |
typed negative" returns three hits, none a ruling: `DESIGN.md:590` (the retired
emitted-alphabet note), `DESIGN-HISTORY.md:1083` (`ITM_CHS` renders as a prefix
minus — a different question), and an unrelated forth-core prompt.
`git log -S "a signed numeral brackets as a term"` attributes the walker rule to
`55c363ad5`, the PP18 refactor, where it existed to reproduce PP17's text-path
bracketing byte-identically — which is exactly why it exists on one side only.
Round 1's `D18-2` is the only prior look at this class and it compared the walker
against the **equation parser**, never against `ppfFromCaptureNode`.

**Correction to a finder.** One dimension's consequence claimed `2 + -5` draws
`2+-5` only on the capture path. It draws `2+-5` on **both** paths (`ADD < ADD`
is false), so the ADD case is a shared cosmetic wart, not a divergence.
`SQUARE`/`CUBE`, `YX`, `MULT` and `SUB`'s right operand genuinely diverge.

**Bug class.** A predicate that enumerates instances of a class instead of
testing it, sitting downstream of two producers that disagree about the fact it
consults. This project's own `guard-enumerates-examples-not-class` (PP18 audit
r1), compounded by `DESIGN-HISTORY.md:143-146`'s `PP18-4`.

**Class-level test.** Not another row. A **differential** table driven from the
producer side: for each leaf shape the capture engine can build — typed positive,
typed negative, typed `EEX` literal, recalled positive real, recalled negative
real, recalled scientific real, recalled complex, recalled short integer, a
tagged angle — assert that `ppfFromCaptureNode`'s `*outPrec` and the walker's
`ppvAstToNodes` `*outPrec` are **equal** for the same text, and that
`ppfPowBase` of each under `ITM_SQUARE` produces a `PP_PAREN` iff the text is not
a visual atom. That is enumerable from the leaf side, where each builder knows
what it just formatted; it is not enumerable from the glyph side, which is
`PP18RR6-D2`.

---

### PP18RR6-3 — `ppfPowBase`'s glyph scan is defeated by the angular-unit suffix `angle34ToDisplayString2` appends AFTER the exponent, so a tagged angle in scientific form is an unbracketed power base

`packages/pretty-print/prettyFormula.c:135`. **Live at HEAD** — the ASCII arm's
digit scan stops immediately on `0xb0`. Same class as `PP18RR6-2`, one glyph
further out, and reported by a different dimension.

**What breaks.** `ppfFormatStaged`'s `dtReal34` arm (`:46`) passes
`getRegisterTag(TEMP_REGISTER_1)` into `real34ToDisplayString`. `display.c:252-257`
routes any non-`amNone` tag to `angle34ToDisplayString2`, which formats the real
— **exponent included, appended last** by `exponentToDisplayString` →
`supNumberToDisplayString` — and then does `strcat(displayString, STD_DEGREE)`
(`display.c:1899`). So the run's last inked glyph is `0x80b0`, not a superscript
digit, and the `0xa160..0xa16b` test at `:135` is false. Radian, grad and
multiple-of-π tags reach it identically: `STD_SUP_BOLD_r` = `0x82b3`,
`STD_SUP_BOLD_g` = `0x9d4d`, `STD_SUP_pir` = `0xac66`.

**Reaching input** (degrees is the default mode):

```
1 EE 20 +/- ASIN     → arcsin.c:50 reallocates X as dtReal34 tagged
                        currentAngularMode; X = 5.729578e-19 deg
STO A                → REGISTER_A = 104 > 99
RCL A                → ppcRclLeaf takes the param>99 branch and stores
                        pad[0] = getRegisterTag = amDegree
x²                   → ITM_SQUARE, classified PPC_MO
draw (PSHOW / the Z-T window / PHIST)
```

**Evidence.** The wave's own PP18RR5-3 row copy-adapted in a worktree with one
line added — `setRegisterTag(REGISTER_Y, amDegree)` between the STO and the RCL.
Gate green (the probe only prints), `testlog.txt:439`:

```
AUDIT-PROBE R6: tagY=5 leaf=312e80d7a47da165a160a00a80b0
                rootkind=4 basekind=0 PP_PAREN=5 PP_SUP=4 PP_RUN=0 scoped=1
```

Reading the leaf: `31 2e` = `"1."`, `80d7` = `PRODUCT_SIGN`, `a47d` = `STD_SUB_10`,
`a165 a160` = superscript `"50"`, `a00a` = the numeric-font padding space
`ppfPowBase` skips, `80b0` = `STD_DEGREE`. **The padding sits before the unit
glyph**, so the skip logic at `:105-107` does not help. `rootkind=PP_SUP` with
`basekind=PP_RUN`: the base is not wrapped, where the identical untagged row two
blocks earlier asserts `base->kind == PP_PAREN` and passes. One tag byte apart,
opposite outcome.

**Consequence.** The owner sees `5.73×10⁻¹⁹°²` — a raised minus-nineteen, a
raised degree ring and a raised 2 all on one line, reading as an exponent of
"-19 deg 2" — beside a result computed from `(5.73e-19)²`. `ITM_YX`
(`prettyFormula.c:178`) and `ppqFactor`'s `^` arm reach the same predicate.

**Violated.** `prettyFormula.c:95-99`: *"a run whose text ends in superscript
glyphs, which is how a value in scientific form spells its exponent."* A tagged
angle **is** a value in scientific form whose text ends in superscript glyphs —
just not in the enumerated block. And `:105-107` states the reason the scan skips
trailing padding at all: *"testing the very last glyph would never see the
exponent underneath it"*. The unit suffix is that same miss one glyph further
out.

**The bonus defect the finder did not claim, and it is the worse half.**
`scoped=1` in the probe output: `ppfTestPowersScoped` — the class-level property
helper whose comment advertises catching *"a new producer that skips
`ppfPowBase` … without anyone writing its row"* — returns **true** on the
defective tree. Its `ppfTestRunEndsSup` copy shares the same alphabet, so the
wave's class guard covers only the half of the class it already knew about. Any
fix has to widen the helper too, or the next alphabet repeats this exactly.

**Bug class.** Same as `PP18RR6-2`: enumerated alphabet where a class test is
needed. This instance also demonstrates the **shared blind spot** limit of the
copied oracle (§6), which is the one thing the four "the oracle is a copy"
findings got right in the middle of getting the mechanism wrong.

**Class-level test.** Drive the leaf formatter over its own tag enumeration:
`amNone`, `amDegree`, `amRadian`, `amGrad`, `amMultPi`, `amDMS`, each on a value
whose magnitude forces scientific form, each under `ITM_SQUARE`, asserting
`base->kind == PP_PAREN`. Six rows, one loop, and the tags come from upstream's
own enumeration rather than from a list someone typed. The same table run through
`ppfTestPowersScoped` is the fix's own proof that the helper was widened with the
predicate.

---

### PP18RR6-4 — `ppcTestNoteLabel`'s 48-name table is exceeded by the suite's 83 fixture labels, so 37 names — including the `VXA`/`VXB` pair whose collision motivated the check — are never recorded

`packages/pretty-print/prettyTest.c:988` (the cap) with `:957` (the table).
**Introduced by `10cc180bc`**: `git log -S ppcTestNoteLabel` returns exactly that
commit. This is the repair for `PP18RR5-5`, and it is saturated on the day it
shipped.

**What breaks.** The backing store is `static char seenName[48][8]` /
`static uint32_t seenSum[48]` / `static uint8_t seenCount`. The insert is
`if(seenCount < 48) { … }` with **no else** — an overflow returns having neither
matched nor recorded, and produces no diagnostic. The 48th distinct name is
inserted at `seenCount == 47`; the 49th is dropped.

**Reaching input**, static in the current tree: 85 `ppcTestWriteAndLoadPgm` call
sites define 83 distinct labels. The 48th distinct name is `VSIN`; everything
from `VSF` onward is outside the guard. Add or rename any future fixture to a
name already used by one of the dropped set — say a new `LBL VXA` appended to
`prettyTestVisual` — and `ppcTestNoteLabel` scans `seenName[]`, does not find
`VXA`, tries to insert, finds the table full, and returns silently.
`ppcTestWriteAndLoadPgm` appends the program anyway, `findNamedLabel` returns the
**first** `VXA`, and the new pin exercises a different fixture's program.

**Evidence.** Two mutations at the audited tip.

1. An `else` arm calling `ppTestFail("overflow, name not recorded: %s")`. Gate
   RED with exactly **37 distinct names** dropped: `VB1 VB2 VB3 VB4 VB5 VB6 VBIG
   VCOL VCUB VD1 VD2 VD3 VD4 VD5 VD6 VD7 VDB VDIN VDN VDNL VDR2 VDRV VEXP VISN
   VNB VNS VOFF VPOW VPX VSF VSIB VSQ VXA VXB VXD VXI VXP`. **`VXA` and `VXB` —
   the pair the check exists for — are in that list.**
2. The differential: two synthetic fixtures with divergent bytes, one labelled
   `VIX` (distinct name #33, recorded) and one labelled `VXI` (#72, dropped).
   Gate output was exactly ONE failure in the whole suite — *"prettyPrint test
   FAIL: label VIX is defined by two different fixtures"* — and nothing for
   `VXI`. The recorded name is caught; the overflowed name is not. The
   single-failure total doubles as the baseline-green control.

**Why the fix's own validating measurement passed.** `10cc180bc`'s message
records that reintroducing the collision "names it directly". The pair it
reintroduces is `VZA`/`VZB` → `VXA`/`VXB` at `prettyTest.c:4408-4422`, whose
first member is distinct name **#10** — comfortably inside the cap. The proof
fires because it lands in the protected prefix.

Two counting notes, both in the direction of a larger gap. A static parse of the
call sites finds 83 distinct labels and 35 past the cap; the instrumented run
found 37, the difference being the five `PPVDRV(…)` macro-built fixtures at
`prettyTest.c:5013-5017` whose labels a source parse does not see. The measured
number is the one to trust.

**Violated.** `prettyTest.c:947-953`, written by this wave: *"Fixture labels must
be unique across this file. `findNamedLabel` returns the FIRST match in label
order and nothing clears program memory between fixtures, so a duplicate silently
points one pin at another pin's program."* The helper's implicit precondition is
that the suite has at most 48 distinct fixture labels. No caller establishes it;
the suite already violates it by 37.

**Consequence, stated honestly.** The historical incident the guard was written
for (`V72` failing as a lift-latch regression ~960 lines from the fixture that
took its name) did turn the suite red — just far from the cause. So a recurrence
among the 37 would more likely be **misdiagnosed** than invisible. That is a
severity nuance, and the finding is already rated latent.

**Bug class.** A guard whose capacity is a hand-typed constant sized against the
suite as it stood, failing **open** and **silently** on overflow. Adjacent to the
round's headline class: a check that covers examples rather than the population,
in the repair written to close a check that covered examples rather than the
population.

**Class-level test.** Two lines and no new fixture: make the overflow arm call
`ppTestFail` (the mutation above *is* the test), and add a compile-time or
first-call assertion that the table is at least as large as the number of
`ppcTestWriteAndLoadPgm` sites. Failing closed is the whole class — a registry
that silently stops registering is worse than no registry, because it reports the
same green either way.

---

### PP18RR6-5 — T25's comment claims the property check covers new `PP_SUP` producers; the body covers only the shapes its own rows type

`packages/pretty-print/prettyTest.c:1469`. **Introduced by `10cc180bc`**, and it
is the sentence that will decide what the next author does.

**What breaks.** `ppfTestPowersScoped` only ever sees a tree that a row built.
The rows type `3 x² x²`, `3 x³ x³`, `3 x² 2 yˣ`, `2 ENTER 3 yˣ 2 yˣ` (plus the
filed decode of the last), the recalled-1e50 square, and EQ4d's parsed
`1×10⁵^2/X`. A fourth producer of `PP_SUP` added later — an `ITM_10x` or
`ITM_EXP` arm in `ppfBuildOp1`, a `PP_SUP` inside `ppqFrameDerivative` — is
reached by none of those inputs, so nothing in T25 changes colour.

**Evidence.** Baseline green, then the third existing producer made to skip the
rule:

```c
- n = ppfPowBase(ppqScopeOperand(c, n, font), PPF_PREC_ATOM, font);
+ n = ppqScopeOperand(c, n, font);   /* AUDIT-PROBE R6 */
```

(`prettyEquation.c:613`; presence verified in
`build.sim/custom_pkg_shadow/prettyEquation.c`.) Gate RED with **exactly one**
failed assertion, `testlog.txt:346` — *"EQ4d a power over a scientific-form
number draws two exponents as one"*. **Not one T25 assertion moved.** A live
`PP_SUP` producer that skips `ppfPowBase` leaves T25 green; it is caught only
because someone hand-wrote EQ4d at `prettyTest.c:3246-3260` — precisely the row
the comment says is unnecessary.

The wave's own history is the counterexample the comment denies: closing
`PP18RR5-1` required hand-writing the `yˣ` rows into T25, and covering the third
producer required a separate EQ4d. Every producer so far has needed its row
written.

**Violated.** `prettyTest.c:1467-1469`: *"`ppfTestPowersScoped` then checks the
property over the whole tree, so a new producer that skips `ppfPowBase` reddens
here without anyone writing its row."* It reddens only if an existing row's
keystrokes happen to route through the new producer.

**Supporting fact, from the same walk.** `ppNewBox(PP_SUP, …)` has four call
sites: `prettyFormula.c:155` and `:281` (both via `ppfPowBase`),
`prettyEquation.c:608` (base via `ppfPowBase` at `:613`), and
`prettyValue.c:316`, which does **not** call it. The fourth is structurally
exempt and harmless (§6), but it means `prettyFormula.c:101`'s *"Every producer of
`PP_SUP` calls this"* and `prettyInternal.h:122`'s three-item list are already an
inventory rather than a checked invariant. `ppfTestPowersScoped` has three call
sites (`prettyTest.c:1491`, `:1525`, `:3258`), no corpus sweep, and no build-time
check pairs a `ppNewBox(PP_SUP` site with a `ppfPowBase` call.

**Bug class.** `guard-enumerates-examples-not-class` again, this time in the
guard written to end that class — and stated in prose that actively instructs the
next author to skip the work.

**Class-level test.** The enumeration has to come from the code, not from a
table someone typed. A `design-audit`-style check over `packages/pretty-print/*.c`
that greps every `ppNewBox(PP_SUP` site and fails unless the enclosing function
also calls `ppfPowBase` (with `prettyValue.c:316` a named, justified exception)
turns the banner sentence into something that runs. That is cheap, it is the
shape `patch_churn_scan.py` already proves works in this project, and it is what
the comment currently promises without delivering.

---

### PP18RR6-6 — `PP18RR5-P1`'s chrome-bit clear has zero assertion power, and the stated reason it cannot be pinned does not match what the fix claims

`packages/pretty-print/prettyVisual.c:1649`. **Introduced by `10cc180bc`.** This
is the one repair in the wave that changes what a shared global holds when the
surface returns.

**What breaks.** Nothing today. Deleting
`screenUpdatingMode &= (uint8_t)~(SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS);`
outright leaves the whole gate **GREEN** (measured: mutation reached
`packages/pretty-print/files/prettyVisual.c:1649`, ninja recompiled it,
`1/1 testSuite OK 174.37s`, `Fail: 0`). No assertion in `prettyTest.c` reads
either bit on this surface.

**Why V36b does not cover it.** `prettyTest.c:5917-5945` is the fixture that
drives the guard — type `VDBL`, clear the softkey band, `refreshScreen(900)`,
assert ink returns — and it is exactly the symptom. But it never sets either
chrome bit, and its typed path runs through `addItemToBuffer`
(`src/c47/bufferize.c:457`), which wipes `MANUAL_STACK|MANUAL_SHIFT_STATUS`. Both
bits are already 0 when the Z/T arm runs, so the clear is a no-op in the harness
and V36b stays green either way.

**Reaching input for a pin**, two lines inside an existing block. `V27`
(`prettyTest.c:5753`) already calls `fnPrettyVisual((uint16_t)id)` directly on
`VDBL`, takes the Z/T arm, and already asserts two other postconditions of that
arm (`screenHoldsDrawnPixels`, `temporaryInformation == TI_SHOWNOTHING`). Set
`screenUpdatingMode |= SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS` before
the call, assert both bits are clear after it. The idiom already exists for the
sibling bit on the sibling surface: `prettyTest.c:663`,
`if(!(screenUpdatingMode & SCRUPD_MANUAL_STACK)) ppTestFail("S3 manual stack bit
not set")`, and again at `:3362` for EQ7.

**Violated.** The wave classes this as not pinnable because *"no probe drives two
VISUAL surfaces in sequence"*. The fix's own comment states a different contract:
*"The chrome bits are CLEARED rather than left alone, because the guard that
decides the menu repaint reads them: ORing would let a bit an earlier full-screen
surface set decide this surface's chrome."* That is a claim about the **value of
`screenUpdatingMode` when the arm returns**, not about the bit's provenance, and
a test can set the bit itself.

**Searched for a ruling that exempts it, and found the opposite.** `10cc180bc`
touched zero files under `design-docs/`. The strings "chrome",
`SCRUPD_MANUAL_MENU` and `SCRUPD_MANUAL_SHIFT_STATUS` appear nowhere in
`design-docs/pretty-print/DESIGN.md`, `DESIGN-HISTORY.md`, `TESTING.md` or
`prettyTest.c`. `TESTING.md` does carry a doctrine for genuinely unpinnable code
(MUT-76's `pbPaint` "HARDENED but NOT PINNED" arm; P13/MUT-D "unverified code and
is recorded as such") — but that doctrine's requirement is that the gap be
**recorded with the analysis that reaches it**, and this clear is recorded
nowhere. The one `TESTING.md` rule that could kill the proposed pin — *"Do not
'fix' this by writing a fixture that forces the state artificially: a pin that
cannot reach its own state through a real gesture is the exact vacuity class R4-1
was about"* — does not reach, because `SCRUPD_MANUAL_MENU` **is** reachable-as-set
through real gestures (upstream sets it at `screen.c:3341`, `:5743`, `:5749`,
`:5776`, `:5925` and `timer.c:187`), and the structural blocker round 5 used to
kill the general `P1` claim, `bufferize.c:457`, masks only
`MANUAL_STACK|MANUAL_SHIFT_STATUS` — `MANUAL_MENU` is not in it.

**Consequence.** A later edit that drops the clear, or that reorders it before
the `|= SCRUPD_MANUAL_STACK` (which would clear the bit it just set — currently
correct, and nothing checks the order), ships green. The owner sees a VISUAL
drawing whose softkey row still shows the popped TAM menu: the symptom V36b
exists to catch, on the surface V36b does not cover.

**Bug class.** A repair that answers a plausible-but-unmeasured finding by
**declaring** the state rather than measuring the race, and then does not assert
the declaration either. `DESIGN.md:690` supplies the oracle — *"Only the stack
refresh is suspended (`SCRUPD_MANUAL_STACK`), so the menu and status bar keep
working"* — which makes the fix correct and the pin cheap.

**Class-level test.** One table over the two VISUAL surfaces asserting the full
`screenUpdatingMode` postcondition each one declares: the Z/T arm leaves
`MANUAL_STACK` set and the two chrome bits clear; the full-screen arm
(`prettyVisual.c:1573`) leaves all three set. Both surfaces are already driven
directly by existing fixtures (V27, V67), so the table costs no new fixture and
it pins the asymmetry that is the whole point of the repair.

---

### PP18RR6-7 — the stacked-power rule now has two homes and three descriptions, one of them the authoritative document

`packages/pretty-print/prettyVisual.c:1137` against `prettyInternal.h:121-123`
and `design-docs/pretty-print/DESIGN.md:565-574`. **Design-flaw, no runtime
misbehaviour.** This is the finding I would leave alone if the goal were correct
code — except for the one-sentence `DESIGN.md` correction, which is free.

**What is there.** The walker still carries its own, older, narrower copy of the
stacked-power rule:

```c
if((a->item == ITM_SQUARE || a->item == ITM_CUBE)
    && ctx->ast[a->child[0]].kind == PPA_OP1
    && (ctx->ast[a->child[0]].item == ITM_SQUARE
        || ctx->ast[a->child[0]].item == ITM_CUBE)) {
  p = PPF_PREC_MUL;
}
```

**Reached and inert, both measured.** Injecting `x = PP_NONE;` beside the
demotion turns exactly one test red — `V51 stacked power brackets its base`, the
existing `RCL a; x²; x²` fixture — so the branch executes on real input. Deleting
the whole block leaves the gate **GREEN**, because `ppfPowBase`'s
`nd->kind == PP_SUP` test at `prettyFormula.c:106` returns `ppfParen(a)` without
ever consulting `aPrec`. The demoted precedence is discarded.

**Why this is still a finding after round 5 examined the same site.** Round 5
looked at it as a *redundancy* and correctly ruled it deliberate, citing
`24cbf7590`'s message (*"the walker's local guard raises `aPrec`, but for a
stacked base this arm now takes the `ppfParen` branch and never consults
`aPrec`"*) and `DESIGN.md`'s V51 exception, then filed the wording mismatch under
*"doc drift, found and not filed as defects"*. Two things changed in this wave.
The banner was **rewritten** to say `ppfPowBase` owns the rule and *"no caller
carries the rule"* — so a maintainer reading `prettyInternal.h:121-123` or
`prettyFormula.c:101` will not look in `prettyVisual.c`, while a maintainer
reading `DESIGN.md:571-574` (*"a stacked power DOES need its base bracketed,
**which the walker does locally** because `ppfBuildOp` deliberately has no POW
level … (V51)"*) will not look in `prettyFormula.c`. Both are half right. And the
surviving copy is now **strictly narrower than the live rule**: it tests the child
AST node's item for SQUARE/CUBE, so it cannot see the superscript-glyph run tail
`PP18RR5-3` added, nor the leading minus of `PP18RR6-2`. Anyone who copies it as
the pattern under-covers.

**Violated.** `prettyInternal.h:121-123` and `prettyFormula.c:101` against
`DESIGN.md:565-566` and `:571-574`, with `DESIGN-HISTORY.md:217` repeating the
`DESIGN.md` sentence. CLAUDE.md makes `DESIGN.md` authoritative; the wave updated
one of the three statements and left the code and the authoritative document
behind. `git show --stat 10cc180bc` lists no `design-docs/` file.

**Bug class.** A rule that got a new home while the old one stayed, with the
documentation split across the move. `ppqScopeOperand`'s own comment
(`prettyEquation.c:148-154`) narrates the previous two occurrences of this in
this package.

**Class-level test.** None is warranted — this is a documentation and dead-code
question, and inventing a pin for it would be the vacuity class. The correct
close is: delete the walker's block (measured green without it), and make
`DESIGN.md:571-574` say what the code does. If the block is kept as
belt-and-braces, the paragraph that keeps it says why in the same words as the
header.

---

## 4. PLAUSIBLE

**None.** Every claim that survived refutation had a constructed reaching input,
and six of the seven had a probe or a mutation behind them.

Accounting: **twenty-four raised** across eight dimensions, **sixteen verified**,
collapsing to **twelve distinct claims** after convergence (four dimensions
independently reported the "class oracle is a copy of the predicate" theory; two
reported the signed-leaf divergence from opposite sides; two reported the
`prettyVisual.c:1137` copy with different claims). Seven distinct claims
survived, five died, none fell beyond the verification cap. The two `prettyVisual.c:1137` claims split
— the "redundant guard is a contract violation" half is REFUTED on the ruling and
sits in §6; the "three statements, one updated" half is `PP18RR6-7`.

---

## 5. Design observations (D7)

Shape, not defects. Six; the second is the one that will still matter after every
finding above is closed.

**`PP18RR6-D1` — the fix-regression rate has now survived every variable that
was supposed to move it.** Round 2: 4 of 7. Round 3: 4 of 4. Round 4: 8 of 14.
Round 5: 6 of 7. Round 6: **6 of 7** (all but `PP18RR6-1`, which predates the
wave by four stages). The waves have been, in order: mixed feature-and-fix,
fix-only, fix-only-with-mutation-verified-pins, and now fix-only by an author who
had just read a report saying the previous wave's fixes were where the defects
were. None of it moved the rate. The remaining hypothesis worth testing is that
the rate is a property of the *number of decision points a wave adds*, not of the
wave's intent: `10cc180bc` added one new function with a new contract, one new
macro, one new harness registry, one new class oracle and three new pins — five
new decision points, five findings. `aafd38f7d` added none and produced none.

**`PP18RR6-D2` — `ppfPowBase` is enumerating a class from the wrong side, and
the enumeration is growing faster than the audits.** The predicate has had four
alphabets in two waves: `PP_SUP` node (round 4), superscript-glyph tail (round 5,
`PP18RR5-3`), trailing ASCII `[eE][+-]?digits` (round 6 out-of-family,
`7ddff2c5f`). This round adds two more members it still misses — a leading `-`
(`PP18RR6-2`) and an angular-unit suffix past the exponent (`PP18RR6-3`) — and
there is no reason to think that list is finished, because the question
`ppfPowBase` is asking is *"is this text a visual atom?"* and it is asking it of
a string, after the information has been thrown away. **The leaf builder knows.**
`ppfFromCaptureNode` knows it just copied a `PPN_LIT` payload that begins with a
minus; `ppfFormatStaged` knows it just called `angle34ToDisplayString2`; the
walker already acts on exactly this knowledge at `prettyVisual.c:1121`. Every
future alphabet is discovered by an audit round if the question stays on the
glyph side and by nobody if it moves to the producer side. This is the single
highest-value structural change available in this file set.

**`PP18RR6-D3` — four dimensions independently reported the same wrong theory,
and only a mutation killed it.** The theory: `ppfTestRunEndsSup` is a
byte-for-byte copy of `ppfPowBase`'s scan, so the class oracle agrees with the
implementation by construction and can only catch a missing *call*, never a wrong
*definition*. It reads as obviously true and it is false, because **the oracle is
not applied to the same input as the implementation — it is applied to the
implementation's output.** When the rule fires the base is a `PP_PAREN`, which the
oracle rejects immediately; the two copies only ever meet on a base `ppfPowBase`
declined to bracket, so a textually independent second copy disagrees the instant
either copy changes. Four separate mutations (narrow the production window to
`0xa164`; narrow to `0xa16b`→`0xa164` with a discriminating fixture; set
`isPower = (last == 0xffff)`, i.e. reinstate `PP18RR5-3` exactly) each turned the
gate red through `ppfTestPowersScoped` itself. **What survives is much weaker and
is real**: the oracle is blind only to a class *both* definitions miss, which is
`PP18RR6-3`'s tagged angle and was `7ddff2c5f`'s ASCII exponent. The lesson cuts
both ways — a duplicated oracle is a legitimate design when it restates the
intent independently, and "this looks like the pin-agrees-with-the-bug shape" is
a hypothesis, not a finding, until something is broken and watched.

**`PP18RR6-D4` — an audit's "deliberately not flagged" section is load-bearing
state, and it can be wrong.** `PP18RR6-1` was cleared by round 3 in one clause
(*"both with ample margin at their producers' digit counts"*), which is correct
reasoning about three of the four arms and irrelevant to the fourth, and the
clearance then stood for three rounds because subsequent rounds fence off known
items rather than re-deriving them. The class was already documented, with a
`_Static_assert`, in `packages/forth-core/forth_bridge.c` — a sibling package in
the same repository, by the same author, for the same upstream function. Two
process consequences: a clearance that reasons about a *caller's* values rather
than the *callee's* contract should be marked as such and re-examined when the
callee is next touched; and this project's cross-package knowledge (the C1/C22
class, the `TMP_STR_LENGTH` assert) is not reaching the packages that need it.

**`PP18RR6-D5` — the tree moved under the audit, in the exact predicate the audit
was hunting.** `7ddff2c5f` landed a fourth `ppfPowBase` alphabet and repaired
`ppqFactor`'s `c->failed` while this round's verifiers were working against the
frozen tip, and one finder's coverage section reports reading a working tree
"ahead of HEAD" carrying those edits. That did not cost this round a verdict —
both wrong-drawing findings were re-checked at `45d07a4ea` and survive — but it
easily could have: a verifier that reads the working tree instead of the audited
ref produces a verdict about neither. The out-of-family half also proceeded
outside the round's accounting, so this report carries a `pending` banner for a
pass that in fact ran on the same subject. Audits freeze a ref; fix waves should
not land inside the window, and if they must, the round's accounting should say
which findings were re-checked at the new tip.

**`PP18RR6-D6` — the wave's best repair is the one that added nothing.**
`aafd38f7d` is the model: it removes churn, changes no behaviour (`git diff -w -B`
is a comment), takes the package's own sanctioned no-reindent-wrap idiom from the
minimality review rather than inventing one, converts three modified upstream
lines back into context, and drops `010-screen.c.patch` from five churn findings
to zero. It produced no findings in this round from any dimension. The contrast
with `10cc180bc` — five new decision points, five findings — is `PP18RR6-D1`'s
mechanism stated as a positive.

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation pass
disproved. This is the substantive half of the round: the wave closed nine items
and **all nine hold**, several of them re-derived from upstream independently by
three or four dimensions rather than trusted.

### Killed by the refutation pass

**"The class oracle is a copy of the predicate it audits, so it can only detect
an omitted call, never a wrong alphabet"** (`prettyTest.c:2325`/`:2353`/`:2378`,
reported four times by four dimensions). **REFUTED by mutation, four times.** The
mechanism is in `PP18RR6-D3`. Concretely: narrowing only the production window to
`0xa160..0xa164` — a wrong *definition*, not a missing call — turned the gate red
with the sole failure `EQ4d`, whose only assertion is `ppfTestPowersScoped`;
reinstating `PP18RR5-3` exactly (`isPower = (last == 0xffff)`) reddened both
`EQ4d` and T25's scientific row. The oracle is a second, independently compiled
statement of the intent, applied to the output tree, and it fails closed on
divergence. The residual — a shape both definitions miss — is `PP18RR6-3` and is
reported there. One finder's supporting claim, that T25's scientific row "agrees
with the predicate under test", is also false: `prettyTest.c:1558` asserts
`base->kind == PP_PAREN` **structurally**, with the in-code ruling *"structural,
not a glyph test: the row must not agree with the predicate it exists to check"*,
and a reach guard at `:1552` (`strstr(leaf, STD_SUB_10)`) whose failure message
is *"the row never reached scientific form, so the row tests nothing"*.

**"`ppvAstToNodes` still pre-brackets a stacked power's base, the precondition
the new banner says no caller carries"** (`prettyVisual.c:1137`). **REFUTED on
intent.** The retention is stated in writing by the commit that centralised the
rule (`24cbf7590`: *"The two paths compose without double-bracketing: the
walker's local guard raises `aPrec`, but for a stacked base this arm now takes
the `ppfParen` branch and never consults `aPrec`, so the walker's pinned output
is unchanged — verified, its stacked-power pin still passes"*), it is ruled by
`DESIGN.md:571-574` with V51 a live pin, and round 5 closed the site by name
under its verification section and separately filed the wording mismatch as doc
drift. The finding's technical analysis (vacuous, unreached-in-effect) is correct
and matches the ruling; what it adds over the ruling is that the ruling should
have been written differently. Scope reinforces it: the hunted range does not
touch `ppvAstToNodes`' OP1 arm at all (`git log -L 1125,1145:…` last modified at
`da8fa57b4`, a pre-wave rename). **The half of the same site that does survive —
that this wave rewrote the banner into a contradiction with `DESIGN.md` and left
a strictly narrower copy behind it — is `PP18RR6-7`.**

**"'Every producer of a `PP_SUP` calls `ppfPowBase`' is false: `prettyValue.c:316`
is a fourth producer"** (`prettyInternal.h:122`). **REFUTED on reachability, by
construction rather than by absence of a key sequence.** `ppParseExponent`
synthesises its own base three lines above the `PP_SUP`: it copies only the
pre-marker region and appends the literal characters `'1','0','\0'`, and the
pre-marker scan at `prettyValue.c:249-252` returns false on any
`PP_IS_SUP_DIGIT`/`PP_IS_SUB_DIGIT`/`PP_SUP_MINUS_CODE`/`PP_RAD_CODE`/const-name.
So the base run's last inked glyph is ASCII `'0'` for **every possible input**,
both of `ppfPowBase`'s tests fail by construction, and the `ATOM < ATOM` fallback
is inert — inserting the call would return the base unchanged always. Containment
was traced in both directions: `ppParseExponent` ← `ppParseRealAny` ←
`ppBuildRegister`/`ppParseComplex` ← `prettyTryRegisterLine` ← `screen.c:3954`,
and `grep ppParse` over `prettyVisual.c`, `prettyFormula.c` and
`prettyEquation.c` returns nothing — the formula tree never pulls a
`prettyValue` node in; it receives register values as a flat display run, which
is exactly why `ppfPowBase` has its run-tail branch. What is left is a
universal-sounding sentence with a three-item enumeration that a grep can
contradict, in a subsystem the sentence's paragraph is not about. It is cited as
supporting evidence inside `PP18RR6-5` and is not a finding of its own.

**"The Z/T surface's 'I did not take the chrome' claim is written into a global
that its reader re-asserts on the way out"** (`prettyVisual.c:1649`). **REFUTED
on intent.** The mechanic is real and reproduced — `screen.c:6074-6075` is
`RETURN_NORMAL: screenUpdatingMode |= SCRUPD_MANUAL_STATUSBAR |
SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU;`, so the repaint arm fires at most once
per paint — but the project has already ruled on exactly this, twice, in
documents this wave committed. Round 4's report refutes a sibling finding *by
relying on* the re-OR (*"the mode is `0x17` on exit, and the next refresh takes
the same early return"*), and round 5 names it as the stabiliser (*"`RETURN_NORMAL`
re-sets MENU on every pass, so the two bits are never both stripped while the
surface is up"*). The one-shot repaint is the intent of the fix it descends from:
`PP18RR3-2` existed to service the single pending menu-repaint request
`popSoftmenu` raised during TAM teardown. The write-site comment asserts a scoped
invariant — which surface's bits decide *this paint's* chrome — and that holds.
The finder's nominated second-evaluation path was also closed by code it had not
read: every `refreshScreen` in `fnPause` (`input.c:167`) is inside a guard that
either excludes `PGM_RUNNING` or sets `screenUpdatingMode = SCRUPD_AUTO` first,
failing the guard's own `!= SCRUPD_AUTO` conjunct. And the status-bar half is
doubly inert: `statusBar.c:783-802` returns immediately in `CM_NORMAL` when
`SCRUPD_MANUAL_STATUSBAR` is set, which `RETURN_NORMAL` sets on every pass.
Round 5 already filed the shape as `PP18RR5-D5`. **What survives at this site is
the absence of any assertion, which is `PP18RR6-6`.**

**"The package's menu-repaint guard is the only site in `screen.c` that omits
`SCRUPD_SKIP_MENU_ONE_TIME`"** (`screen.c:5898`). **REFUTED on reachability, and
the baseline was mis-identified.** An exhaustive enumeration of every write to
`screenUpdatingMode` in `src/` and `packages/` shows bit `0x40` **is** set today —
the finder missed it because it enters via `screenUpdatingMode = ~SCRUPD_AUTO`
(0xFF) in `createHOME`/`createPFN` (`softmenus.c:3803`, `:3808`, `:3833`,
`:3838`), not via a `|=` — and 0xFF necessarily also sets `MANUAL_MENU` (0x04)
and `MANUAL_SHIFT_STATUS` (0x08). So the package's
`(mode & (MANUAL_MENU|MANUAL_SHIFT_STATUS)) == 0` is false in exactly the states
where upstream's `!(mode & (MANUAL_MENU|SKIP_MENU_ONE_TIME))` is false; the two
spellings are behaviourally identical at every reachable state, and no
composition in the tree yields 0x40 set with 0x04 and 0x08 clear. Separately, the
"only site that drops the term" premise is wrong about the control flow: upstream
at the corresponding line (`src/c47/screen.c:5863-5865`) has **no menu test
here at all** — a bare `goto RETURN_NORMAL` that paints nothing — so the package's
`if` is a narrowing gate inside a branch upstream left silent, not the same test
spelled differently. The finder's own reaching-input section concedes UNREACHED
and pins the consequence to a hypothetical future upstream commit re-enabling
`keyboardTweak.c:125`. That is a merge-hygiene watch item about code that does
not exist.

### The wave's repairs, re-derived rather than trusted

**`PPV_DECLARATION_ITEMS` — the macro and its membership, checked against the
machine rather than the comment.** Six dimensions checked it independently. The
macro expands to `case ITM_NULL: case ITM_LBL: case ITM_MVAR: case ITM_REM:
case ITM_PAUSE: case ITM_SNAP` with the final colon supplied by each use site;
both switches previously carried the identical six-case list; no arm's
fallthrough moved (`ppvLiftNeutral` still falls into `case ITM_ENTER: case
ITM_XEQ: return true;`, `ppvStepArm` still `return`s). The membership claim looks
wrong in `items.c` and is right in the machine: `ITM_LBL` and `ITM_REM` are
`SLS_ENABLED`, which would make them lift-enabling — but `executeOneStep`
(`lblGtoXeq.c:826-867`) returns for `PTP_DECLARE_LABEL` and `PTP_REM` **without
calling `runFunction`**, so the SLS bit is never applied and they are lift-neutral
in effect. `MVAR`, `PAUSE`, `SNAP` and item 0 are `SLS_UNCHANGED`. The restated
rule's exclusion list was also verified against every write to
`stk->liftDisabled`: `LITERAL` and `RCL` route through `ppvPushLifting`, which
sets it false on both branches; `PGMINT` (`:1007`) and `PGMDRV` (`:1031`) clear
it explicitly. All four leave the latch exactly as the epilogue would, so their
absence is a no-op and the comment is accurate. The one design objection
considered — one name fusing two different questions ("no stack, no picture" for
the dispatch arm, "clearing here would destroy what the step left" for the
epilogue exception) — could not be made concrete: nobody could name an item that
belongs in one and not the other, and the comment states the failure it is
trading against.

**The `ITM_FILL` repair.** `stk->depth = ppvLiveStackSlots()` matches upstream
`fnFillStack` (`stack.c:208`, `for(i = REGISTER_Y; i <= getStackTop(); i++)`)
exactly. `slots` cannot change mid-walk (the walker declines SSIZE4/SSIZE8 steps
via `default → D_OPCODE`), `depth` can never exceed `slots` beforehand because
`ppvPush` renormalises against the same accessor, and `ast[depth-1]` is guarded by
the `depth == 0` decline above it. `stk->saturated = true` is not duplicated
state: it is exactly what `ppvPush`'s own `if(stk->depth >= slots)` would
conclude, and FILL is the one arm that cannot go through `ppvPush`. `break`
rather than `return` correctly lets the epilogue run, where `ppvRefillFromT` is a
no-op at `depth == slots`. `ppvBody`'s seeding loop still iterates
`PPV_STACK_SLOTS` times rather than the live count — correct at both stack sizes
only because `ppvPush` renormalises, i.e. by the same accident the new V-FILL
comment documents as the reason the old FILL bug hid. Not a defect; worth
knowing.

**The V-XEQ eRPN expectation.** Checked against `fnKeyEnter`
(`keyboard.c:3417-3440`): under eRPN with `programRunStop == PGM_RUNNING` the
third disjunct is true, so the dup still happens and ASLIFT is then set — the
machine yields `[., 4, 4, 5] → 13`, so `4+(4+5)` is the right assertion and the
walker's `liftDisabled = !getSystemFlag(FLAG_ERPN)` matches. The premise that
makes the whole V-XEQ repair sound — that a callee's trailing ENTER survives the
return — rests on `RTN` and `END` being `SLS_UNCHANGED` in `items.c`, which was
verified directly. (This is the same row `45d07a4ea` records an out-of-family
reader getting wrong because the packet supplied only XEQ's SLS bit.)

**The flag scaffolding in the three new pins.** V-XEQ and V-FILL save/set/restore
both ways; V-DECL's one-sided restore is correct because that block only ever
clears. `ppTestFail` only increments a counter and prints — no `longjmp`, no
abort — so no failed assertion can skip a restore, and the restores also run on
`ppvTestExpect`'s early-return paths. `setSystemFlag(FLAG_SSIZE8)` genuinely
moves the axis rather than poking an inert flag: `defines.h:2284` makes
`getStackTop()` a macro over that flag, which is what `ppvLiveStackSlots` reads.
`setSystemFlag`/`clearSystemFlag` both hit `refreshStateFlags`, leaving
`doRefreshSoftMenu = true` for later blocks, but that cannot false-green V36b —
the `CM_NORMAL` guard calls `showSoftmenuCurrentPart()` directly and never
consults it.

**`aafd38f7d` is whitespace-and-comment only.** `git diff -w` over the range on
`screen.c` shows only the reworded comment; the per-column guards (`x1 <
SCREEN_WIDTH`, the bold twin at `x1 + 1 < SCREEN_WIDTH`, `x2 < SCREEN_WIDTH`, the
`rep_enlarge` arm) and the `x2 = x1 - 1` reasoning are byte-identical in effect.
The result is that three `setPixel` lines and a brace become **context** rather
than modified lines. One prose loss worth the owner's attention rather than a
finding: the deleted sentence *"which the simulator HALs reject and the device
ROM's `bitblt24` does not"* was the recorded reason the clamp cannot be pinned
(`PP18RR4-11/-12`); the replacement keeps only the ROM half, so a future reader
has less reason not to delete an untested guard.

### Guards and conjuncts, falsified or proved load-bearing

**`ppfPowBase`'s false-positive surface — searched, none found that matters.**
Four constants' `itemCatalogName` end in a superscript glyph (`Se²`, `Se'²`,
`Sf⁻¹`, `m_uc²`), and `PPN_CONST` renders exactly that string, so squaring `Se²`
now draws `(Se²)²`. Parentheses never change meaning, and here the extra pair is
the correct drawing anyway. Short integers end in `STD_BASE_*` (`0xa460..0xa46f`,
subscript); fractions end in `STD_SUB_*` and are unreachable from
`real34ToDisplayString`; complex in the default form ends in the `i`/`j`
(`display.c:1613-1618`); DMS ends in the seconds glyph. Skipping the
`0xa000..0xa00f` space block cannot create a meaning-changing false positive
because skipping an inked glyph could only add brackets.

**`ppfPowBase`'s glyph decoding.** The two-byte-when-high-bit-set rule matches
upstream `stringNextGlyph` (`charString.c:379-392`) and the package's own
`ppRunInk` (`prettyLayout.c:180-190`); `s[i+1]` is only read when `s[i] != 0`;
the truncated-glyph `break` cannot loop and fails closed; `i` is `uint16_t`
against a 512-byte pool; `last = 0` for an empty run gives `isPower = false`;
`ppTextAt` returns `""` (not NULL) out of range, so the `s != NULL` guard is
belt-and-braces. `STD_NOCHAR` (0x01) in a complex string sets `last` to 1 — a
false negative, not a false positive.

**`ppqFactor`'s lost `c->failed`.** `ppfPowBase` can return `PP_NONE` without
setting the flag (pool exhaustion inside `ppfParen`), where the
`ppqScopeOperand` it replaced always set it. Cleared as unreachable-as-a-defect
by three dimensions: `ppqParse:790` tests `c.failed || n == PP_NONE`, `ppqTerm`'s
loop guard tests both, and `ppqExpr:724`/`:748` set the flag on a `PP_NONE` from
`ppqTerm`. It is an invariant break with no observable, on an allocation-failure
path nobody can steer. It has since been repaired by `7ddff2c5f` (G3), which
reached the same conclusion.

**The Z/T chrome clear's arithmetic and ordering.** `screenUpdatingMode` is
`uint8_t` and the two bits are `0x04|0x08`, so `(uint8_t)~0x0C` = `0xF3` clears
nothing else. The `|= MANUAL_STACK` precedes the `&= ~chrome`, so the mode can
never land back on `SCRUPD_AUTO` and falsify the guard's second conjunct. In the
VISUAL path the guard's `goto RETURN_NORMAL` fires **before**
`_selectiveClearScreen`, so clearing `MANUAL_MENU` cannot cause a band clear, and
both `showSoftmenuCurrentPart` and `refreshStatusBar` draw outside
`PPV_BAND_TOP..PPV_BAND_BOTTOM` (rows 20..91). The only reader of
`MANUAL_SHIFT_STATUS` that could drop a pending shift (`screen.c:6045`) is
unreachable in this state because the total return at `:5901` fires first.

**`ppcTestNoteLabel`'s "identical bytes are a re-run" allowance.** Considered by
four dimensions and cleared as designed: the label name is inside the hashed
bytes, so identical bytes under one name **is** the same program, both copies
behave identically, and no pin can be mis-aimed. The 31-multiplier rolling hash
can in principle collide, but forging one requires deliberate construction, which
is outside this review's threat model. Its `len == 0 || len > 7 || 3 + len > n`
and `n < 4` guards are correct for `char name[8]`. The check only fires on
`pgm[0] == ITM_LBL`; every label site is at array offset 0, so a
second-label-inside-a-body hole is currently unreachable. Two second-order gaps
recorded rather than flagged: labels created by any path other than
`ppcTestWriteAndLoadPgm` are invisible to the registry (V39 keys `VKEY` in
through PEM), and the registry is a function-local static never reset, so a
second run of the suite in one process would re-load all 85 fixtures while the
registry correctly calls them re-runs.

**The `screen.c` wrapped-x guards.** Each conjunct is falsifiable and each write
is screened by its own column: `x1 + 1 < SCREEN_WIDTH` drops the bold twin at
column 399, `x2 < SCREEN_WIDTH` keeps the doubled twin at 399 when `x1` lands on
400 (deliberate, and the comment argues for it), `if(x2 > 0) x2--;` leaves
`x2 == 0` at `x1 == 0` (upstream's own line, untouched). A wrapped negative `x`
makes both `x1` and `x2` huge `uint32`s and both guards reject them. The
pre-clear's `x < SCREEN_WIDTH` does not screen `x + width`, but upstream calls
`lcd_fill_rect` unguarded for every normal glyph including ones at the right
edge, so the primitive clips its own width.

### Chased hard and killed, worth recording

**The matrix-SHOW hypothesis.** The strongest candidate the upstream dimension
had: that the package's added menu-repaint block fires for an **upstream**
surface. It genuinely does. `temporaryInformation == TI_SHOWNOTHING` is set at
`display.c:3952` by `fnC47Show`'s matrix arm, right after `clearScreenOld`
deliberately blanks the softkey band, and that arm calls `refreshScreen(150)`
when `programRunStop == PGM_RUNNING` with `screenUpdatingMode == 0x10` — not
`SCRUPD_AUTO`, and carrying neither chrome bit. So the package's block runs where
upstream's total return painted nothing. Reaching input: a program with a matrix
in X executing SHOW. **What killed it:** `fnC47Show(NOPARAM)` pushes `MNU_SHOW`,
`showSoftmenuCurrentPart` skips its own `clearScreenOld` for `MNU_SHOW`
(`softmenus.c:3119`, *"the screen owner has already painted over the menu area"*)
and draws nothing because `menu_SHOW` has `numItems == 0`, and `refreshStatusBar`
repaints a bar `clearScreenOld` never cleared. Net pixels unchanged. Recorded
because the safety is upstream's accident and nothing pins it.

**Missing `lcd_refresh_dma()` after the added `showSoftmenuCurrentPart()`.**
Upstream's site at `screen.c:6052-6055` carries it with a load-bearing comment
(*"If this is not here, menu generation is not reliable, and presses are missed.
Not sure why."*). Cleared: upstream itself omits it at 9 of its 10
`showSoftmenuCurrentPart` call sites in this file, so its absence is not a
convention the package broke.

**The complex leaf one level out.** `complex34ToDisplayString2` emits
`re ± i·im`, so `RCL` of a complex register followed by `x²` should draw
`2+i3²` — a compound leaf reported as `ATOM`. It is the same shape as
`PP18RR6-2` and was routed rather than double-filed: it is the same root cause
(the leaf builder does not report what it formatted) and the same fix, and
`PP18RR6-2`'s class-level test enumerates it explicitly. Not reproduced by
execution, so it is named here rather than asserted.

**`ppqFactor`'s second branch** (*"attach an already-superscript exponent run
verbatim"*, `prettyEquation.c:622-641`) does not call `ppfPowBase`, but it builds
a `PP_HBOX`, not a `PP_SUP`, so the banner's enumeration stays literally true. It
cannot be sequenced into a wrong drawing either: `ppqFactor` tests `'^'` first,
`ppqNumber` swallows a `·₁₀ⁿ` tail into the number run, and `X⁵^2` leaves `'^'`
unconsumed so `ppqParse` declines on `c.pos != c.len` rather than drawing
anything.

### Doc drift and below the bar

`DESIGN.md:783`'s `screen.c` hook row still says *"ONE hunk: the §6 inline arm at
:3936"* while the patch has five, and its sibling-adjacency argument
(*"forth-core's hunks … are all far away"*) no longer holds cleanly — the
package's menu-repaint hunk sits at upstream `:5861`, 66 lines from forth-core's
`:5927` hunk in the same function. Not a range finding: the hunk count reached
five before `9de22ac7d`, and the stale inventory is already the package's own
minimality-review finding 2. It is stated because this range's whitespace commit
touched that patch and did not correct the row.

The remaining churn-scan finding (`solver/equation.c`, `[WS-ONLY]`) is the same
wrap-reindent class `aafd38f7d` just fixed in `screen.c`, one file over. Outside
the named file set, predates the range (`9fdf90ce3`), and already catalogued as
an open item in the minimality review.

Four hand-written copies of the same glyph classification now exist
(`prettyValue.c:79-92` named, `prettyEquation.c:35-42` named,
`prettyFormula.c:129-135` raw, `prettyTest.c:2372-2384` raw) and they already
disagree — `PP_IS_SUP_DIGIT` excludes `0xa16a`/`0xa16b`, `PPQ_IS_SUP` includes
both. The disagreement predates the wave and the new copies match the correct
one, so it is folded into `PP18RR6-D2` rather than filed.

T25 row 2 (`3 CUBE CUBE`) duplicates T24c exactly — redundant, not vacuous. The
V-FILL `SS=4` row is green before and after the fix and therefore cannot
distinguish the repair, but its comment says so outright (*"Under SS=4 the old arm
still saturated by accident … which is why this pin has to set SS=8"*): an honest
control, not a defect.

---

## 7. Verdict

**Ship `aafd38f7d`? Not as it stands, and the reason is not a fix regression.**

`PP18RR6-1` is a write 56 bytes past a stack array, three times per history row,
on a gesture as ordinary as doing arithmetic in BASE mode and pressing PHIST. It
did not fault in the x86 simulator and the drawn text came back correct, which is
what undefined behaviour looks like right up until the ARM frame layout differs.
That one is not optional and it is not this wave's; it has been in the tree since
PP4 and an audit round cleared it in a subordinate clause.

After that, the file set draws wrong mathematics for two reachable inputs, both
in the wave's headline repair, both still live at HEAD after a fourth widening of
the same predicate. `5` `+/-` `x²` draws `-5²` beside a value of 25 in the
formula browser and on the live T-line, while VISUAL draws `(-5)²` for the same
program. That divergence is not a slip — the walker's rule is *right* and the
capture builder's is *absent*, and the wave concentrated three call sites onto a
fallback that consults the number the absent rule was supposed to supply.

**Where it breaks first:** integer mode plus PHIST, which is a crash class rather
than a drawing class. Then any typed negative under a power or a product — the
cheapest keystroke sequence in this report. Then the harness, which now certifies
48 of 83 fixture labels and reports the same green either way.

**What I would leave alone if the goal were correct code rather than a clean
audit.** `PP18RR6-7` is inert: the walker's surviving block executes and changes
nothing, and round 5 already ruled the redundancy deliberate. The only part of it
worth a keystroke is the one-sentence `DESIGN.md:571-574` correction, because
CLAUDE.md makes that document authoritative and it now says the opposite of the
code. `PP18RR6-6` is a two-line pin in an existing fixture; if it were expensive
I would leave it and record the gap in `TESTING.md` under the doctrine that file
already has for unpinnable code — the objection is that the gap is recorded
nowhere at all, not that the code is wrong. `PP18RR6-5` is a comment; the honest
minimum is to delete the claim the body does not support, and the better answer
is the code-side enumeration check, which is worth building once for the whole
package.

**What the wave got right, and it is most of it.** Nine closures, all nine hold
under attack. `PPV_DECLARATION_ITEMS`' membership survives a check against
upstream's *executor* rather than its item table — which is where the naive
reading is wrong, and four dimensions checked it independently and all four got
the same answer. `ITM_FILL` matches `fnFillStack` slot for slot. The eRPN and
SSIZE8 arms added to V-XEQ and V-FILL are the two-bit axis round 5 asked for. The
class oracle four readers dismissed as tautological is genuinely load-bearing and
was proved so four times. `aafd38f7d` is the model repair: no behaviour, no new
decision point, five churn findings to zero, and no findings against it from any
dimension.

---

## 8. Round and exit state

**Round 6** of the restarted PP18 series. **In-family only.**

| | |
|---|---|
| readers | eight in-family dimensions (contracts, lifecycle, arithmetic, errorpaths, guards, tests, design, upstream) + a three-lens refutation pass per finding |
| out-of-family | **none — `pending`** (see the §1 banner) |
| raised / verified / distinct / CONFIRMED / PLAUSIBLE / REFUTED | 24 / 16 / 12 / 7 / 0 / 5 |
| findings inside the previous wave's own repairs | **6 of 7** |
| mutation- or probe-backed CONFIRMED | 6 of 7 |
| gate | GREEN at `45d07a4ea` (this pass) and at `aafd38f7d` (seven verifier worktrees) |
| tree at finish | clean; no `AUDIT-PROBE` residue |

**The exit criterion is not met and did not advance.** It requires two
consecutive clean rounds with all three families reading. This round had one
family and seven findings, so it fails on both counts. The running total of clean
rounds is **zero**, and the sequence of confirmed counts since the restart is
16, 17, 15, 14, 7, 7 — it has stopped falling.

**Consecutive rounds without an out-of-family reader: six.** With the caveat
`PP18RR6-D5` records: an out-of-family pass over this same subject did run
outside the round's workflow and landed as `7ddff2c5f` before this report was
written. Its findings were disjoint from these seven, which is worth something,
but it does not satisfy the criterion, which counts a round's readers and not the
branch's commits. If the intent is to close the exit criterion, the next round
needs the out-of-family half **inside** the round, against the same frozen ref,
with the packet path, the reply path and the verbatim `MODEL:` line in this
table.

**Recommended subject for round 7**, if the pattern is to be tested rather than
assumed: the repairs for these seven findings, restricted to the same file set,
with the axis rotated to **the producer side of the leaf** — `ppfFromCaptureNode`
and `ppfFormatStaged` in `prettyFormula.c` against `ppvAstToNodes` in
`prettyVisual.c`, which is where `PP18RR6-D2` says the next alphabet will
otherwise be discovered by an audit instead of by the code.
