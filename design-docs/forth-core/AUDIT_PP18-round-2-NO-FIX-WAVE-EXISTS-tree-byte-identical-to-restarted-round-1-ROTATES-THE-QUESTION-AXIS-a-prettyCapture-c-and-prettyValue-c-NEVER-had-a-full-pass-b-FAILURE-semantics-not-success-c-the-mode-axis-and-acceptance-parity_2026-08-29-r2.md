# Audit — PP18 round 2 (restarted series), whole stage, at `34ac6e97f`

Subject: `pretty-print/stage-pp17..HEAD` on `pretty-print/stage-pp18`, tip
`34ac6e97f`. **No fix wave exists.** The tree is byte-identical to the one
restarted round 1 read, so nothing here is a regression audit of somebody's
repairs. What changed is the question. This round runs restarted round 1's own
priority list, inverted into three axes:

- **(a) `prettyCapture.c` and `prettyValue.c`** — the two files round 1
  recorded as never having had a full pass, and through which its own #1
  (`PP18RR1-1`) paints.
- **(b) failure semantics rather than success semantics** — what the code does
  when a pool exhausts, a rung fails, a parse declines, a capture is abandoned
  mid-formula, or a mode changes underneath staged state.
- **(c) the mode axis mechanically** (the producer-by-flag table of
  `PP18RR1-D1`) **and the acceptance-parity question** across
  `ppqParse`/evaluator/walker.

Eight in-family finder dimensions ran blind to each other over that scope;
every raised finding then went to an independent refutation pass with one
assigned lens (reachability, correctness, intent), instructed to default to
REFUTED and to prove coverage claims by mutation.

**Seventeen CONFIRMED findings, one REFUTED, one UNVERIFIED.** Twenty-three
findings were raised; twenty-two survived refutation; deduplicated across
dimensions they are seventeen. **Fourteen of the seventeen are backed by a
probe or mutation that was applied, built through the real gate, observed in
`testlog.txt`, and reverted** — a sharper evidence posture than any previous
PP18 round, and it is what turned four of them from arguments into
measurements.

The axis paid. Both never-audited files carried defects four earlier rounds
could not have found, because they were reading elsewhere: the capture engine
files a **wrong `= result`** on six keystrokes, **deletes** a finished formula
on `FILL`, and goes on describing registers that two upstream `fnRecall` paths
rotated away; and `prettyValue.c`'s exponent parser rejects **every string its
own producer emits**, so PP2's raised-exponent form — a shipped, documented,
history-recorded feature — has never fired on a real register value on this
device.

Axis (c) is half-answered and the report says so: the mode axis was run
mechanically and produced a three-dimension finding in the capture engine
(`FLAG_SSIZE8`), extending `PP18RR1-D1`'s table to a second column. **The
acceptance-parity question was not reached** — no reader built the
accept/refuse oracle across `ppqParse`/evaluator/walker, and no finding here
substitutes for one.

Nothing was fixed. Every probe and mutation was applied, observed and reverted
inside an isolated worktree; the main tree is clean at start and finish
(`git status --porcelain packages/` empty, `grep -rn AUDIT-PROBE packages/`
empty), and the gate is green at `34ac6e97f`.

---

## 1. Subject and coverage

> **This round had NO out-of-family reader** (outOfFamily: 'pending'). It does not count toward the exit criterion's two clean rounds and its verdicts carry one family's blind spots (SKILL.md, Exit criterion).

### Subject

**Tip.** `34ac6e97f` ("docs: skill defect handoff — the out-of-family pass can
be skipped silently"). Range `pretty-print/stage-pp17..HEAD`: fourteen
commits, 26 files, +8,668 / −859 — identical to restarted round 1's subject,
because no commit has landed since. The *reading* scope is different and is
stated per axis above: primary subject `prettyCapture.c` (1,291 lines) and
`prettyValue.c` (890), neither of which the range modifies. That is
deliberate. Both files are load-bearing for the stage's surfaces, both sit
inside every PP18 drawing path, and neither had been read end to end by
anybody.

**KNOWN, excluded from re-reporting** (verified still present, then fenced):
`PP18RR1-1..12` and `PP18RR1-P1`; `PP18R4-1..11` and the round-4 plausible
carry (P1 `MVAR` import, P2 `PP_MAX_DEPTH`, R2-P1, R3-P1..P3); and the
2026-08-29 out-of-family set (`HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md`
§4). Where a new finding sits adjacent to a known one it states the
distinction — `PP18RR2-6` vs `PP18RR1-3` most importantly: the same *flag* on
different *code* (the live capture engine vs the static VISUAL walker), by
different mechanisms, and neither fix touches the other.

**Numbering.** This round's findings are **`PP18RR2-1`–`PP18RR2-17`**, its
design observations **`PP18RR2-D1`–`D6`**. `grep -rn PP18RR2` over the
repository returned nothing before this file was written. The `RR` prefix is
not cosmetic: `PP18R2-*` is already taken by the 2026-08-28 round 2 of the
pre-restart series, and the in-code comment tags `R1-*`/`R2-*`/`R3-*` belong
to a *third* series, the pretty-print package's own audit tags — a collision
already recorded as `PP18R3-7`. Findings below quote code comments tagged
`AUDIT R1-5`, `R2-1`, `R3-3`; those are package tags, not this series.

### Coverage (union across the eight in-family dimensions)

**Read at line level, in full, by more than one dimension:**
`prettyCapture.c` (all 1,291 lines — arena, alloc, free, deep copy, the
value-leaf recipe, serializer, the history ring and its eviction,
`ppcEmit`/`ppcDisplaced`/`ppcInvalidate`, the classifier, both dispatch hooks,
every STAGE and every DONE arm, the three NIM hooks, the viewer API) by six;
`prettyValue.c` (all 890 — `ppParseFraction`, `ppParseExponent`,
`ppParseIrfrac`, `ppParseComplex`, `ppBuildRegister`, both rung ladders,
`prettyTryRegisterLine`, `fnPrettyShow`) by five; `prettyInternal.h` and
`prettyPrint.h` in full.

**Read in part, at the seams the primary two touch:** `prettyFormula.c`
(`ppfStageValFields`/`ppfFormatStaged`, `ppfFromCaptureNode`, the
`ppfBuildEntry` token decoder, `ppfBuildRow`, `fnPrettyHist`, `ppfCombine1/2`,
`ppfBigop`); `prettyLayout.c` (`ppReset`/`ppNewBox`/`ppNewRun`/
`ppAppendChild`/`ppMeasure` head and HBOX arm/`ppRenderRightAligned`/metrics —
the allocation-failure contract the parsers depend on);
`browsers/prettyBrowser.c` in full by two (`pbPaint`, `pbFindResult`,
`prettyBrowserEnter`); `prettyEquation.c` at `ppqBuildBigop`/`ppqBuildCall`/
`ppqShowRender` only; `prettyVisual.c` at its fixed-buffer sites and the
classifier/vocabulary cross-checks only; `prettyTest.c` — the capture drivers
and helper layer end to end, `T1`–`T29`, `B0`–`B11`, `P12`/`P13`, `M1`–`M8`,
`S1`–`S4`, `F1`–`F3`, `FV1`–`FV20` by the tests dimension, targeted elsewhere.

**Upstream verified by execution path, not assumed:** `items.c`
`reallyRunFunction` (the STAGE/dispatch/DONE ordering `PP18RR2-4` turns on) and
the `US_STATUS` rows for `FILL`/`REGS`/`SST`/`SSIZE4`/`SSIZE8`/`EXIT1`/
`REtoCX`; `keyboard.c`'s `SHOWMODE` `RCL` arm (:2830), the register browser's
two `fnRecall` calls (:3024, :3033), the CM_NIM key routing, the deferred
single step (:2289-2292), the error-clearing arm (:2390-2395) and the
`CM_PRETTY_BROWSER` containment guard (:2819); `bufferize.c`'s `closeNim` head,
`closeNim_exit` and all five `goto` exits (which of them run `undo()` and which
do not is `PP18RR2-7`); `calcMode.c`'s latch-then-`liftStack` order; `recall.c`
`fnRecall`'s `FLAG_ASLIFT`/`fnRollUp` branch; `store.c`
`fnStore`/`_storeValue`/`isRegInRange`; `stack.c`
`liftStack`/`fnFillStack`/`_Drop`/`getStackTop`; `registers.c`
`getRegisterFullSizeInBlocks`/`reallocateRegister`; `display.c`
`real34ToDisplayString2`/`exponentToDisplayString`/`supNumberToDisplayString`
(:195 is `PP18RR2-1`) and `complex34ToDisplayString2`'s hair-space clauses;
`flags.c` `SetSetting`/`clearSetPairs`; `nextStep.c` `fnSst`; `lblGtoXeq.c`
`runProgram`; `ui/tam.c`'s dispatch-before-`leaveTamMode` ordering and indirect
resolution; `solver/sumprod.c` `_checkArgument`; `src/testSuite/hal/lcd.c`'s
blitter polarity (load-bearing for `PP18RR2-14`/`-15`).

**Docs read:** `DESIGN.md` in full by three, its §3 BINDING invariant and §4
segmentation rule by all eight; `DESIGN-HISTORY.md`'s PP2/PP3/PP8/PP18 entries
and the 2026-08-27 EXIT-bug entry; `TESTING.md`'s mutation catalog and the
P13/browser-row rulings; `REVIEW_upstream-minimality_2026-08-27.md` in full;
all five prior PP18-family reports at their findings, PLAUSIBLE, cleared and
not-flagged sections, so nothing they killed was unknowingly re-raised.

### Not reached, and it matters where

- **Acceptance parity (axis (c), second half) was not run.** No reader
  compared what `ppqParse`, the evaluator and the walker *accept*; the
  differential oracle `PP18RR1-D3` asks for still does not exist.
- **`prettyLayout.c`'s measure/paint arithmetic** was read only where the
  primary two call into it. A layout defect reachable only from those two
  files could have been missed; `PP18RR1-1` and `-9` own that file this cycle.
- **`prettyVisual.c`** was deliberately not re-read (23 open findings, out of
  scope for this axis) and **`prettyEquation.c`'s parser** was not read at all.
- **`prettyTest.c`'s ~4,300 pre-stage pin bodies** outside the capture, value
  and formula families were spot-checked only.
- **No simulator ran; no LCD photograph backs any finding.** The evidence is
  end-to-end code traces plus fourteen executed in-suite probes.
- **No device build.** The gate builds the simulator only, so a device-only
  break inside a `PC_BUILD`/`TESTSUITE_BUILD` region would not be caught. The
  guard boundaries were read by hand and are sound; that is a read, not a
  build.
- `010-solver__equation.c.patch` (619 adds, 5 hunks) grew during **PP17**,
  before this range opens, and no in-tree review covers its fifth hunk. Stated
  as a coverage gap for whoever owns the PP17 range, not as a finding here.

---

## 2. Mechanical results

**Gate.** `./packages/pretty-print/build-test.sh --solo` is green at
`34ac6e97f`. Ten verifiers ran it to completion in isolated worktrees — clean
baselines recorded at 175–290 s (`PRETTY-PRINT GATE GREEN`, testSuite OK) —
and each re-ran it after reverting its probe. No new compiler warning was
reported; the range's own `10e49e084` closed the one known warning (V77
format-overflow). This is the baseline every mutation below is measured
against, not a discovery.

**The governing gate is the package's own.** The round-3/4 warning stands and
was re-confirmed by two verifiers: `./packages/forth-core/build-test.sh`
returns a meaningless green for pretty-print mutations.

**Probes and mutations this round ran** — all applied, built through the real
refresh (mutation presence verified in `build.sim/custom_pkg_shadow/*` or in
the regenerated `files/` twin), observed in `build.sim/meson-logs/testlog.txt`,
and **reverted**. None numbered; the MUT catalog is the owner's to extend.

| probe / mutation | observed result | finding |
|---|---|---|
| replay `2 ENTER 3 + 9 STO Y`, then inspect `ppcHistoryEntry(0)` | filed signature `[[2 + 3] = 9]` | **PP18RR2-4** |
| `2 ENTER 3 + 9 FILL`, with a `CLSTK` control on identical state | `PROBE-FILL hist (expected 1, actual 0)`; control `hist 1` | **PP18RR2-3** |
| `2 ENTER 3 + x<>y FILL`, plus a root-in-X control | FILL `sig '-' hist 0`; CLSTK control `hist 1`; root-in-X control untouched | **PP18RR2-3** |
| `2 ENTER 3 + ENTER FILL` (current root aliased in slot 1) | `cur=2` → `cur=255 (PPC_NIL)`, `hist 0` | **PP18RR2-3** |
| complex `2+3i` in Y, `4` in X, `ITM_MULT` | `Ydt=2 blocks=8 bytes=32 payloadCap=16`; `rootkind=OP2 c0=OPAQUE c1=VAL`; `shown=255`; `histCount=0`; `err=0` | **PP18RR2-10** |
| T24's shape with `ITM_SST` in place of `ITM_RS` | `FAIL: SST left the shadow describing stale registers (expected '-', actual '2 3 +')`; T24 green | **PP18RR2-5** |
| `1..5` under SSIZE8, `SSIZE4`, `9 STO A`, `SSIZE8`, 4×`R↓`, `2 ×` | `A=9`, `X=18`, drawn signature `1 2 ×` | **PP18RR2-6** |
| `X = 1.5e30`, then the exact builder call `ppBuildRegister` makes | bytes `20 31 2e 35 80 d7 a4 7d a1 63 a1 60 a0 0a`; `ppParseExponent=0`, `ppParseRealAny=0`; with the trailing `a0 0a` stripped, `ppParseExponent=1` | **PP18RR2-1** |
| 17-node formula in Y, digit in X, `RCL+ Y` | arena non-free nodes after a full invalidate `= 1`; with the sibling BIGOP arm's `if(n != PPC_NIL) ppcFreeTree(n);` added, `= 0` | **PP18RR2-11** |
| `8 WSIZE`, `2 ENTER 3 +`, `300#10` (error 14), `×` | after the failed close `X=0 Y=5` with shadow `2 3 +`; after `×`, `sig '# 2 3 + ×'` against `X=0` | **PP18RR2-7** |
| depth-9 right spine (`1 2 x<>y +` ×8), then `CLX` | live build **succeeds** at 151×12 px; entry stores at 75 B; `ppfBuildEntry=0`, `ppfBuildRow=0` | **PP18RR2-12** |
| `2 ENTER 3 + ENTER CLSTK`, and `… ENTER CLX DROP CLX` | history count **2** where the design demands 1, both sequences | **PP18RR2-8** |
| `prettyValue.c:795` → `if(false)` (T-line surface dead) | gate **GREEN** | **PP18RR2-14** |
| `prettyValue.c:795` → `if(regist == REGISTER_T)` (flag ignored) | gate **GREEN** | **PP18RR2-14** |
| row-height gate → `h > 10`, so FV6's row cannot build | FV6 **green**; formula absent from the screen, row painted "(too large to show)" | **PP18RR2-15** |
| B10's `eLbl` forced to `INVALID_VARIABLE` | gate **GREEN**, zero output from B10 | **PP18RR2-16** |
| R1-3's bug reintroduced (`ppcStage.depth != myDepth` dropped) | **exactly one** failure in the whole battery: B10 | **PP18RR2-16** |
| `ppBuildRegister` given a `dtString` arm (accept what it must decline) | S4 **passes**; only F1 and the probe's own `screenHoldsDrawnPixels` check fail | **PP18RR2-17** |
| `screen.c:3940` `const34_1e6` → `const34_1on10` (upstream FRACT gate narrowed, package copy untouched) | gate **RED**, exactly one failure: `F3 displayValueX differs` | **REFUTES** the builder-first finding (§6) |
| FV6's own named mutation (second rung deleted, `prettyFormula.c:651`) | T25/T28/T29 red, **FV6 green and honest** — its row measures 60 px and never reaches the tiny rung | correction to **PP18RR2-15** |

**Upstream churn.** `patch_churn_scan.py` over all 13 pretty-print patches at
HEAD, re-run for this report: standing churn count **1** — `[WS-ONLY]`, the
`showString` wrap-reindent in `010-solver__equation.c.patch` — pre-existing,
catalogued, owned by `REVIEW_upstream-minimality_2026-08-27.md`, not
re-reported under rule 6. Totals: `010-keyboard.c.patch` 76/3/11,
`010-solver__equation.c.patch` 619/1/5, every other patch ≤ 21 adds. The
in-range `patches/` delta is +13 purely additive lines in the two test-harness
patches. **Zero growth of the eleven firmware overrides across the whole
stage.** Refresh sync verified: working area vs `files/` clean, manifest
hashes matching — the build reads the code this audit read.

**`design-audit.sh`** is forth-core's; there is still no pretty-print
equivalent, so no override-budget check ran. The substitute is the patch
surface audit above, clean apart from the documentary defect in §4.

**One mechanical fact worth stating plainly.** Of the mutations above, **five
left the gate fully green while deleting or corrupting the behaviour they
targeted** (T-line surface dead; T-line flag ignored; FV6's row unbuildable;
B10 skipped entirely; `ppBuildRegister` accepting a type it must decline). One
went red with exactly one useful failure (`F3`), and one went red with exactly
one useful failure after a *bug* was reintroduced (`B10`). That ratio is §3's
four test findings, and it is `PP18RR2-D5`.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. A feature that is silently absent
for a whole class of values on the always-on surface outranks a wrong drawn or
filed formula on a specific gesture, which outranks lost work, which outranks
a stuck state that needs repetition to reach, which outranks a pin that cannot
go red. Where the refutation pass corrected a finding's own claim, the
correction is stated inside the finding — three of them are corrections
against the finder, not for it.

For each: file:line, what breaks, the concrete reaching input, the violated
contract quoted, the bug class, and the class-level test. **No patches.**

---

### PP18RR2-1 — `ppParseExponent` rejects every string `real34ToDisplayString` actually produces, so PP2's raised-exponent form has never fired on a register value

`packages/pretty-print/prettyValue.c:263` (the post-marker `else { return
false; }`), against the producer's last statement,
`src/c47/display.c:195` (`strcat(displayString, STD_SPACE_HAIR);` at the tail
of `supNumberToDisplayString`). Reached by two dimensions' worth of tracing
and settled by execution.

**What breaks.** Every upstream exponent string ends with a trailing hair
space, unconditionally: `real34ToDisplayString2` → `emitSciDigits`
(display.c:392/395) or display.c:1350/1353 → `exponentToDisplayString`
(:127) → `supNumberToDisplayString`, whose final act is the `strcat`. Nothing
in `src/c47` ever strips one; every occurrence appends. `ppParseExponent`'s
post-marker scan admits only superscript digits (`PP_IS_SUP_DIGIT`,
0xa160..0xa169) and a leading `PP_SUP_MINUS_CODE`; `STD_SPACE_HAIR` (0xa00a)
is neither, so the scan falls to line 263 and declines. `ppParseRealAny` then
declines, `ppBuildRegister` returns false, and `prettyTryRegisterLine` falls
through to upstream's inline rendering.

**Reaching input.** Any real with a displayed ten-exponent, from the default
screen with `FLAG_PRETTYP` on: `1 . 5 EEX 3 0 ENTER`. Also every value in SCI
or ENG display mode, and every magnitude outside the FIX band. PSHOW takes the
same builder and falls through to `fnC47Show` identically.

**Evidence (executed).** A probe inside `prettyTestMeasure` set X = 1.5e30 and
made the exact `real34ToDisplayString(...)` call `ppBuildRegister` makes:

```
AUDIT-PROBE R2 builder bytes: 20 31 2e 35 80 d7 a4 7d a1 63 a1 60 a0 0a
AUDIT-PROBE R2 ppParseExponent=0 ppParseRealAny=0
AUDIT-PROBE R2 hairspace-stripped ppParseExponent=1
```

`20` is the FRONTSPACE (tolerated), `80 d7` the product glyph, `a4 7d`
`STD_SUB_10`, `a1 63 a1 60` the superscript "30" — and `a0 0a` the hair space.
Removing only those two bytes flips the parser from decline to accept in the
same run. That isolates the terminator as the sole cause.

**Violated.** `DESIGN.md:150-152`, PP2's own specification: *"split
`real34ToDisplayString` output at the `PRODUCT_SIGN STD_SUB_10` marker
(emitted by `exponentToDisplayString`, display.c:127); sup-digit tail becomes
a `PP_SUP` exponent run. **No marker → plain real → false** (upstream
renders)."* The design's decline condition is *no marker*; the code declines
on the marker's own builder-supplied terminator. It also violates the §2
builder-first premise that the parser reads *the builder's output* — this
parser reads an alphabet the builder does not emit.

**Why five rounds read past it.** The pin that should have caught it is fed by
hand. `prettyTest.c:158` (M6) supplies `"1.5" "\x80\xb7" "\xa4\x7d" "\xa1\x64"
"\xa1\x60"` — the real alphabet with the trailing hair space omitted, i.e.
exactly the "hairspace-stripped" shape the probe showed parses at 1. The
sibling parser's fixture does it correctly: `ppParseIrfrac`'s decline fixture
at `prettyTest.c:215` carries `\xa0\x0a\xa0\x0a`, copied from the real
display.c template, and `ppParseIrfrac` models trailing spaces explicitly
(`S_TAIL`, prettyValue.c:340-347). The author read the builder's alphabet for
one parser and assumed it for its neighbour.

**Bug class.** Parser accept-set narrower than its producer's emit-set, with
the divergence hidden by a hand-written fixture instead of producer output.

**Class-level test.** A producer-fed table pin: for a value list spanning each
parser's domain (a fraction, an exponent real in FIX/SCI/ENG, an IRFRAC form,
a complex), call the same upstream builder `ppBuildRegister` calls and assert
the parse verdict — accept where the design promises a form, decline where it
promises fallthrough. No byte fixture is typed by hand. The same pin retires
M6's fixture as the authority on what the builder emits.

---

### PP18RR2-2 — recall-to-X from SHOW mode and from the register browser bypasses the dispatch hooks, so the shadow keeps describing registers the recall rotated away

`packages/pretty-print/prettyCapture.c:1285` (`ppcShadowInvalidate`, the
facility that exists for exactly this class, with one production caller),
against `packages/pretty-print/keyboard.c:2830-2838`, `:3024` and `:3033`.

**What breaks.** Three upstream call sites reach `fnRecall` *directly*, not
through `reallyRunFunction`, so neither `prettyNoteFunction` nor
`prettyNoteFunctionDone` runs and the shadow is told nothing. In SHOW mode:
`if(SHOWMODE) { if(item == ITM_RCL) { keyActionProcessed = true;
fnRecall(showRegis); … } }`. In the register browser: `fnRecall(currentRegister
BrowserScreen)` in both the global/local and named arms, with `calcMode =
previousCalcMode` restored *before* the recall. With `FLAG_ASLIFT` set,
`recall.c:36-39` runs `fnRollUp(NOPARAM)` — the whole register stack rotates
and the shadow does not.

**Reaching input.** `2 ENTER 3 +` (X=5, current formula `2+3`, ASLIFT set by
the `+`). `SHOW`, `0`, `1` to point `showRegis` at R01 (say R01 = 9), then
`RCL`. Machine: X=9, Y=5. Shadow: slot 0 still says `2+3`, slot 1 UNKNOWN.
Then `×`: STAGE materialises slot 1 truthfully as VAL(5) and DONE builds
`5 × (2+3)` = 25, while the machine computed 5 × 9 = 45. `ppcEmit` files the
entry with the *true* register result, so the history reads `5×(2+3) = 45` and
the browser's ENTER recalls 45 for an expression worth 25. Second, independent
route: `REGS` is `US_UNCHANGED` → `PPC_IGNORE`, so the shadow survives into
`CM_REGISTER_BROWSER`; `RCL` there is the same bypass.

**Violated.** `prettyCapture.c:11-16`, the BINDING invariant (DESIGN.md §3):
*"shadow slot k always holds an expression whose value equals register
REGISTER_X + k at quiescence… The display never lies."* And the package's own
statement of this exact class, `prettyCapture.c:1283-1284`: *"For mutations
that bypass item dispatch (the browser's recall-to-X): wipe the shadow to
UNKNOWN without touching the history ring."* `ppcShadowInvalidate()` has
exactly one production caller — `browsers/prettyBrowser.c:249`, the package's
*own* browser. Upstream's three recall-to-X bypasses have none.

**Correction against the finder, recorded.** The finding claims the
intermediate frame shows `2+3` over an X of 9. It does not:
`prettyTryRegisterLine` bails on `temporaryInformation != TI_NO_INFO`, and
both bypass sites set one (`TI_COPY_FROM_SHOW`, `TI_STORCL`). That hides the
lie for exactly one keypress. The corrupt shadow is real and the wrong drawn
and filed formula lands on the next operator as described.

**Refutations that failed.** `OPTION_SHOW` and `OPTION_REGBROWSER` are both
defined in the "common to all hardware packages" block of `defines.h`, so both
sites are live on the target; the generated `010-keyboard.c.patch` contains
zero occurrences of `ppcShadowInvalidate`; `SHOW` (row 1742) and `REGS` (row
1560) are both `US_UNCHANGED`, so `ppcClassify` returns `PPC_IGNORE` and never
even sets `ppcStage.valid`; `fnC47Show` saves and restores `systemFlags0/1`
and never touches `FLAG_ASLIFT`, so the `+`-set lift survives; and
`ppcEnsureKnown` only fills a slot that is *already* UNKNOWN — nothing
re-checks a slot that still holds a tree.

**Bug class.** Mirror bypass: a state mutation that does not pass the
instrumented dispatch. Same class as `PP18RR2-5` (SST) and, one layer down,
`PP18RR2-7` (`closeNim`'s error exits).

**Class-level test.** Enumerate every upstream call site that writes
`REGISTER_X` or rolls the stack outside `reallyRunFunction`
(`grep -n "fnRecall\|fnStore\|fnRollUp\|liftStack" src/c47/keyboard.c` is the
whole surface today: three `fnRecall`s) and assert each is followed by
`ppcShadowInvalidate()`. The executable form is the quiescence pin of
`PP18RR2-D1`: after each key, for every slot k that is not UNKNOWN, the slot's
value must equal register X+k.

---

### PP18RR2-3 — `PPC_FILL` frees slots 1..top without displacing them, so `FILL` deletes a finished formula instead of filing it

`packages/pretty-print/prettyCapture.c:946-952` (the DONE arm), with no
`case PPC_FILL` anywhere in the STAGE switch. **Found independently by four
dimensions** (contracts, lifecycle, arithmetic, design) through four different
reaching inputs, and executed by three of them.

**What breaks.** `ITM_FILL` is classified (`prettyCapture.c:517`), so the
default rule never sees it, and `prettyNoteFunction` has no arm for it — it
falls to `default: break`, so nothing is displaced while the registers are
still pre-op. The DONE arm is the whole of its handling:

```
    case PPC_FILL: {
      for(uint8_t k = 1; k <= ppcTopSlot(); k++) {
        ppcFreeTree(ppcSlot[k] == PPC_UNKNOWN ? PPC_NIL : ppcSlot[k]);
        uint8_t d = ppcDeepCopy(ppcSlot[0]);
```

Free, with no `ppcDisplaced`. `ppcFreeTree` ends `if(ppcCurrent == n) {
ppcCurrent = PPC_NIL; }`, so when the freed node is the live root the formula
closes silently: the T-line goes blank and PHIST never gains a row.
Meanwhile upstream `fnFillStack` (stack.c:203-218) really has overwritten
Y..`getStackTop()`, which is displacement by the §4 definition.

**Reaching input** (any of four; the precondition is only that the current
root sits in a slot ≥ 1): `2 ENTER 3 + 9 FILL`; `2 ENTER 3 + x<>y FILL`;
`2 ENTER 3 + ENTER FILL`; `2 ENTER 3 + 7 FILL`. `FILL` is `menu_STK` slot 5,
two keystrokes from `menu_BLUE_C47`'s `-MNU_STK`.

**Evidence (executed, three independent probes).**

```
PROBE-FILL hist (expected 1, actual 0)        <- FILL loses it
control CLSTK: sig '-', hist 1                <- identical state, filed
after FILL  : sig='-' cur=255 hist=0
ctrl FILL (root still in X): sig='2 3 +' cur=2 hist=0   <- correctly untouched
```

The CLSTK control is decisive: same shadow state, and the wipe site that
*does* call `ppcDisplaced` (STAGE, `:716-719`) files `2+3 = 5`. The
root-in-X control bounds the defect to roots in slots 1..top.

**Violated.** `DESIGN.md:250-256`, the ruled segmentation rule: *"A formula
(an op-rooted tree) is emitted to history when either 1. **displacement** —
its root leaves the shadow stack unconsumed: overwritten in X, dropped, wiped
(CLX/CLSTK/invalidation), swapped out to a register, or pushed off the stack
top by lifts."* And the code's own `AUDIT R3-3` note at
`prettyCapture.c:996-1000`, whose words apply verbatim to the arm the sweep
did not visit: *"The tree being dropped may be a FINISHED formula. Every other
wipe site in this file displaces it (emits it with the register that still
holds its value) before freeing; this one freed it, so the owner's formula
vanished from the history instead of being filed."*

**Not exempted anywhere.** `FILL` appears twice in `DESIGN.md` (:201, the
classifier list; :609, the walker's opaque-taint rule — a different
subsystem), and once in `git log -S PPC_FILL` (db495d984, PP3), none of which
sanctions discarding. `grep FILL packages/pretty-print/prettyTest.c` returns
one hit, a comment in a DERIV body: the interactive path has **no pin at all**,
which is why the hole is silent.

**Severity note.** This is a lost record, not a wrong displayed value —
nothing on screen states a falsehood; a true formula simply never gets filed,
and the live one disappears.

**Bug class.** Unenumerated member of a ruled class (wipe site without
displacement) — the same shape as `PP18RR2-4` and `-9`.

**Class-level test.** A table over every item that overwrites or discards a
shadow slot — CLX, DROP, DROPY, CLSTK, **FILL**, STO-to-stack, `x<>reg`, the
lift push-off, a big operator's consumed limits — driven from one fixture
with the finished root in slot 0 and again with it in slot 1, asserting
history count 1 and the correct `= result` in both placements. That single
table pins `PP18RR2-3` and `-4` at once, and would have gone red on this arm
the day it was written.

---

### PP18RR2-4 — `PPC_STO_NOP`'s displacement emit runs at DONE, so the filed formula's `= result` is the value `STO` just wrote

`packages/pretty-print/prettyCapture.c:1006` (`ppcDisplaced(k, true);  // file
it before it is lost`), inside the only `ppcDisplaced(..., true)` in the whole
of `prettyNoteFunctionDone`.

**What breaks.** `PPC_STO_NOP` has **no STAGE arm** — `prettyNoteFunction`'s
switch falls to `default: break`, so nothing is read before the dispatch. The
hook order in `reallyRunFunction` is STAGE, then
`indexOfItems[func].func(param)`, then DONE; by DONE, `fnStore` → `_storeValue`
(store.c:219) has already copied X into the target register. The arm then
computes `k` and calls `ppcDisplaced(k, true)` → `ppcEmit(tree, REGISTER_Y)`,
reading the register **after** the store.

**Reaching input.** Six keystrokes on default settings: `2 ENTER 3 + 9 STO Y`,
then `PHIST`. The `+` builds `OP2(ADD, "2", "3")` into slot 0 and sets
`ppcCurrent` (X=5); `9` opens NIM with ASLIFT set, so the commit takes the
`ppcShiftUpForLift` branch and the tree moves to slot 1 (Y=5, X=9);
`STO Y` writes Y=9 and *then* the shadow files the formula against Y.

**Evidence (executed).** Replaying exactly that through the harness's real
dispatch helpers and inspecting the filed entry:

```
AUDIT-PROBE R2: hist count = 1
AUDIT-PROBE R2: filed sig '[[2 + 3] = 9]'
```

The ring permanently records `2+3 = 9`. PHIST draws that row; pressing ENTER
on it (`prettyBrowserEnter` → `pbFindResult` → the TKRES payload) recalls 9
into X for a formula whose value is 5. The entry is marked `PPA_EMITTED` and
the tree is freed, so nothing later can correct it.

**Violated.** `ppcEmit`'s own banner, `prettyCapture.c:404`: *"resultReg >= 0
supplies the '= result' snapshot (the register still holding the value — the
invariant is the proof)."* And the `AUDIT R1-5` comment inside
`ppcInvalidate`, `prettyCapture.c:461`: *"Every OTHER emit-with-register site
in this file runs at STAGE, before the dispatch, where the register genuinely
still holds the formula's value."* That sentence is now false, and the site
that falsifies it was added by the `R3-3` correction, whose own comment
assumes the STAGE-side timing it did not use.

**Why T23 misses it.** T23 is the only pin that reaches the branch, and its
displaced slot holds a bare literal (`"7"`); `ppcEmit` returns early on a
non-OP root (*"a bare value is not a formula"*, `:401`), so the
emit-with-register at `:1006` never executes under it.

**Reachability checked at every joint.** `menu_TamSto` lists `ITM_REG_Y`;
`tam.c:710` sets `tam.value = indexOfItems[item].param` and `:1123` calls
`reallyRunFunction(tamOperation(), value)` *before* `leaveTamModeIfEnabled()`;
there is no `CM_TAM` calcMode, so `ppcScopeOk()` holds; and
`ppcShiftUpForLift` displaces only the *top* slot, so the `2+3` root moves to
slot 1 unemitted and `ppcCurrentRevalidate` keeps it live.

**Bug class.** Snapshot read on the wrong side of the dispatch — the `R1-5`
class, recurring at the site added by `R1-5`'s successor fix.

**Class-level test.** Two shapes, both cheap. Structural: assert that no
`ppcDisplaced(k, true)` executes inside `prettyNoteFunctionDone` (the file's
own stated invariant, currently violated exactly once). Behavioural: the
`PP18RR2-3` wipe table, extended to compare the filed `= result` against the
register value captured *before* the dispatch, for every stack-register target
including `STO L`.

---

### PP18RR2-5 — `SST` executes a whole program step outside the hooks and is `US_UNCHANGED`, so the shadow survives a stack the step rewrote

`packages/pretty-print/prettyCapture.c:560` (the `AUDIT R1-7` exception block,
which enumerates `ITM_UNDO` and `ITM_RS` and not `ITM_SST`) and `:588` (the
default rule's `return PPC_IGNORE`).

**What breaks.** `ITM_SST` (items.c row 1736) is `US_UNCHANGED` and absent
from `ppcClassify`'s switch, so it classifies `PPC_IGNORE` and STAGE returns
with nothing staged. `fnSst` (nextStep.c:414-450) only sets
`programRunStop = PGM_SINGLE_STEP`; DONE then runs with `ppcStage.valid`
false. Control returns to `keyboard.c:2289-2292`, which runs
`runProgram(true, INVALID_VARIABLE)` — one program step executes with
`programRunStop = PGM_RUNNING` (lblGtoXeq.c:902), so every nested
`prettyNoteFunction` fails `ppcScopeOk()` and returns without mirroring
*or* invalidating. The step's stack motion is recorded nowhere.

**Reaching input.** With a program in memory, from CM_NORMAL: `2 ENTER 3 +`
(current formula `2+3`, X=5), then `SST`. That CM_NORMAL single-step state is
upstream-supported (screen.c:1047 handles `calcMode == CM_NORMAL &&
programRunStop == PGM_SINGLE_STEP`). If the stepped instruction pushes or
replaces X, the T line goes on showing `2+3` for an X that is no longer 5, and
the next terminator files `2+3 = <the step's result>`. Repeated `SST`
compounds it. Nothing on screen says the mirror stopped tracking.

**Evidence (executed).** A pin shaped exactly like T24 — the R1-7 pin for R/S
— with `ITM_SST` in place of `ITM_RS`:

```
prettyPrint test FAIL: AUDIT-PROBE R2 SST left the shadow describing
  stale registers (expected '-', actual '2 3 +')
```

T24 one block above, identical but for the item, is green at `-`. R/S
invalidates; SST does not.

**Violated.** The `AUDIT R1-7` ruling itself, `prettyCapture.c:554-559`:
*"R/S resumes a stopped program, which then rewrites the stack with every step
out of scope — so nothing tells the shadow. XEQ is the same operation reached
by the other key and is US_ENABLED, so the default rule already covers it; R/S
is US_UNCHANGED and was not."* `SST` is the third member of that set and was
not enumerated.

**Refutations that failed.** SST does reach `runFunction` in CM_NORMAL
(`keyboard.c:2860`; the direct `fnSst(NOPARAM)` at `:3175` is the CM_PEM arm);
`itemNotAvail` has no `ITM_SST` case, so the hook line is reached; the empty
`fnSst` stub in `src/c47/items.c` is inside a `GENERATE_CATALOGS` block and is
not what firmware links; and the package's complete hook set (calcMode.c:269,
config.c:1732, bufferize.c:2345/2690, items.c:413/416) has nothing in
`runProgram`, `nextStep.c` or the single-step epilogue.

**Bug class.** `US_UNCHANGED` items that move the stack — the second member
of the class this file already carves out by hand.

**Class-level test.** Enumerate the `US_UNCHANGED` rows that can move the
stack (`UNDO`, `R/S`, **`SST`**, `BST`, `SSIZE4`/`SSIZE8`, `REGS`) and assert,
per item, that after dispatch the shadow is either invalid or still
quiescence-correct. The same enumeration answers `PP18RR2-6`.

---

### PP18RR2-6 — every stack-target guard in the capture engine is bounded by the live `getStackTop()`, so a write to A–D under SSIZE4 leaves slots 4–7 stale and the lie reappears under SSIZE8

`packages/pretty-print/prettyCapture.c:1004` (`STO`), `:741` (`x<>reg`),
`:1021` and `:1045` (`RCL` / RCL-arith stack sources), all against
`ppcTopSlot()` at `:86-88`. **Found independently by three dimensions**
(lifecycle, arithmetic, guards) and executed.

**What breaks.** `ppcSlot[8]` is indexed by *absolute* register offset — slot
k is `REGISTER_X + k` in both stack sizes, and `ppcInvalidate` loops
`for(i < 8)` — but every write-detection guard is written against
`getStackTop()`, which is `REGISTER_D` or `REGISTER_T` depending on
`FLAG_SSIZE8`. Under SSIZE4 the guards stop covering A–D while the slots stay
live, and upstream keeps A–D perfectly writable (`isRegInRange` has no
stack-size term). `ITM_SSIZE4`/`ITM_SSIZE8` are `SLS_UNCHANGED | US_UNCHANGED`
(items.c rows 1938/1939) dispatching `SetSetting`, so `ppcClassify` returns
`PPC_IGNORE` and nothing re-scopes the slots. `SetSetting`/`clearSetPairs`
only flips the flag; the sole upstream site that clears A–D is
`fnClearRegisters`, which the mode switch does not call.

**Reaching input.** Under SSIZE8: `1 ENTER 2 ENTER 3 ENTER 4 ENTER 5` (slots
0..4 hold 5,4,3,2,1 against X..A). `SSIZE4` — shadow untouched,
`ppcTopSlot()` now 3. `9 STO A` (param 104): the guard tests `t <=
getStackTop()` = 103 and does nothing, while `fnStore` writes A = 9. Slot 4
still says `1`. `SSIZE8` — slot 4 is inside the cap again. Four `R↓`, then
`2 ×`.

**Evidence (executed).** Driving exactly that through the real dispatch:

```
AUDIT-PROBE R2: after SSIZE4, stackTop=103
AUDIT-PROBE R2: register A after STO = 9
AUDIT-PROBE R2: register X after 4 Rdown = 9
AUDIT-PROBE R2: register X after MULT = 18
AUDIT-PROBE R2: drawn formula signature = '1 2 ×'
```

The signature comes from the same accessor (`ppcCurrentFormulaRoot`) that
`prettyFormula.c:455` uses to draw the on-screen formula, and the tree holds no
UNKNOWN, so it is not withheld: the screen shows `1 × 2` beside a result of 18.
A second dimension reproduced the same divergence through `RCL A` instead of
`R↓`, filing `2+3 = 9` on the supersession and drawing `2+3+1 = 10`.

**Violated.** `DESIGN.md:164-168`, marked BINDING: *"shadow slot k always
holds an expression whose value equals the live contents of register
`REGISTER_X + k`. When a transform cannot maintain that, the slot degrades to
a value leaf … or the whole shadow invalidates. The display never lies;
over-invalidation only costs history granularity."* The invariant is stated
over k, without reference to stack size; the guards are not. The failure is in
the forbidden direction — the display lying rather than over-invalidating.

**Distinct from `PP18RR1-3`.** That finding is the *static VISUAL walker*
hardcoding eight levels (`PPV_STACK_SLOTS 8 ///< SSIZE8 simulated regardless
of the flag`). This is the *live shadow* reading a moving cap and then
trusting the slots it stopped maintaining. Neither fix touches the other, and
the walker's inline comment is the contrast that makes this one a defect: the
author documents the choice where he makes it, and there is no counterpart at
`ppcTopSlot()` or at any of the four guards.

**Coverage.** `grep -n SSIZE packages/pretty-print/prettyTest.c` returns
nothing. The package has no stack-size coverage at all, and the gate stayed
green with the lie live in the log.

**Bug class.** An invariant stated over a fixed index space, enforced by
guards written against a moving bound — the mode axis, at its second producer.

**Class-level test.** Run the whole capture battery twice, once with
`FLAG_SSIZE8` cleared (the mode-looped oracle `PP18RR1-D1` already asks for,
one loop around an existing instrument). Plus one targeted pin for the window
itself: write A under SSIZE4, restore SSIZE8, assert slot 4 is UNKNOWN or the
shadow invalid.

---

### PP18RR2-7 — a `closeNim` that ERRORS leaves the registers lifted and the shadow unlifted, and no compensating invalidate runs

`packages/pretty-print/prettyCapture.c:1176` (`if(calcMode == CM_NIM ||
lastErrorCode != 0) { return; }` — the early return that skips
`ppcShiftUpForLift()`), against `bufferize.c:2529-2542` (the
`ERROR_WORD_SIZE_TOO_SMALL` arm, which reaches `closeNim_exit` with **no**
`undo()`).

**What breaks.** The lift is two-phase by design: `calcMode.c:269` latches
`ppcPendingLift = getSystemFlag(FLAG_ASLIFT)` and then upstream `liftStack()`
lifts the **registers immediately**, while the shadow's lift is deferred to
the commit hook. `closeNim` runs `calcModeNormal()` *before* its range checks,
so an erroring close returns to CM_NORMAL with the registers lifted; the
commit hook then declines on `lastErrorCode != 0` and the shadow never lifts.
The two are one slot out of step until something invalidates. Nothing does:
digit keys in CM_NIM are handled inside `addItemToNimBuffer` with
`keyActionProcessed = true`, so there is no item dispatch and therefore no
`prettyNoteFunctionDone` — DONE's `lastErrorCode != ERROR_NONE →
ppcInvalidate(false)` guard at `:849-857` never runs. The same hole exists on
the EXIT route: `fnKeyExit` calls `addItemToNimBuffer(ITM_EXIT1)`, and
`ITM_EXIT1` is `US_UNCHANGED` → `PPC_IGNORE` → DONE bails at `:845` before
reaching the error check.

**Reaching input.** `8 WSIZE`, then `2 ENTER 3 +`, then `3 0 0 # 1 0`, then
any key to clear the error, then `×`. (The base digit that makes the base ≥ 2
auto-closes NIM; the word size rejects the value.) A second reacher: `1 # 1`
then `EXIT`, where the base < 2 error at bufferize.c:2470-2473 likewise runs
no `undo()`.

**Evidence (executed).**

```
AUDITPROBE A after 2 ENTER 3 +  sig='2 3 +'    X = 5
AUDITPROBE B lastErrorCode=14 calcMode=0 aslift=1
AUDITPROBE B after failed closeNim sig='2 3 +'  X = 0   Y = 5
AUDITPROBE C after MULT sig='# 2 3 + ×'         X = 0
```

At B the registers have lifted and calcMode is back to CM_NORMAL while the
shadow is byte-for-byte the pre-NIM `2 3 +` in slot 0. At C the drawn and
filed formula is `5×(2+3)` (25) against a register X of 0.

**Violated.** `DESIGN.md:164` (BINDING) as above, and the design's own
justification for deferring the lift, which enumerates exactly one abort:
`DESIGN.md:191-194` and the file banner at `prettyCapture.c:21-23` — *"A NIM
aborted by backspace-to-empty runs upstream `undo()` and needs no shadow
rollback, because nothing was applied."* `closeNim`'s error exits are a second
abort shape in which the register lift **has** been applied and nothing undoes
it. The asymmetry is inside upstream's own function: the digit-not-in-base
exit at `bufferize.c:2481-2489` calls `undo()` and is therefore safe; the
word-size and base-range exits do not.

**Refutations that failed.** The error-clearing keypress does not repair it —
`keyboard.c:2390-2395` clears `lastErrorCode` and falls through to dispatch
the same item, with no `undo()`, no `restoreStack()`, no package hook. And
nothing downstream compares a slot to its register: `ppcCurrentRevalidate`
only checks reachability, `ppcCurrentFormulaRoot` only rejects OPAQUE.

**Bug class.** Two-phase state applied on one side only when phase two fails
— torn staging on an error path, the class `undo-history`'s U1-era finding
already paid for once in a sibling package.

**Class-level test.** For each `goto closeNim_exit` in `bufferize.c` (five
today), drive the erroring input from a state with a live formula and assert
quiescence — every non-UNKNOWN slot k equals register X+k — or that the shadow
invalidated. The same pin shape covers the EXIT route.

---

### PP18RR2-8 — `ppcDeepCopy` clears `PPA_EMITTED`, so ENTER's dup makes two unfiled instances of one formula and the history takes it twice

`packages/pretty-print/prettyCapture.c:185`
(`ppcArena[c].aux &= (uint8_t)~PPA_EMITTED;` for OP1/OP2/BIGOP kinds).

**What breaks.** ENTER's dup leaves the original root in slot 1 and a deep
copy in slot 0. The copy is minted *unemitted*, so each instance emits
independently when displaced, and one calculation is filed twice — two
distinct sequence numbers, two of the twelve ring slots. `ppcEmit` has no
content or sequence de-duplication; the per-node flag is the only guard.

**Reaching input.** `2 ENTER 3 + ENTER CLSTK` (the shortest: `PPC_CLSTK`'s
STAGE loop displaces every slot, so both instances emit within one keystroke),
or `2 ENTER 3 + ENTER CLX DROP CLX`.

**Evidence (executed).**

```
PROBE-A enter-dup then CLSTK      (expected 1, actual 2)
PROBE-B enter-dup CLX DROP CLX    (expected 1, actual 2)
```

**Violated.** `DESIGN.md:259-261`: the emitted root *"stays on the shadow
stack flagged EMITTED, and can still be consumed later to continue a larger
formula (**flag cleared on consumption, so nothing emits twice**)"*, and §4's
closing sentence, which names the **one** accepted duplication source:
*"Consequence accepted: undo-then-redo can eventually duplicate a history
entry."* ENTER-dup is not that source — and §4 goes out of its way to protect
the ENTER-dup idiom (*"pressing ENTER on a result to duplicate and continue
(`2 ENTER 3 + ENTER ×` = (2+3)²) is a continuation idiom"*).

**Why the clear cannot be defensive.** Every consumer of `ppcDeepCopy` — the
ENTER dup, `PPC_FILL`, `LASTX`, `RCL` of a stack slot — replicates an
already-filed or about-to-be-filed formula, so clearing the flag can only ever
*add* emissions, never rescue a lost one. `FILL` is a worse instance of the
same statement: it can mint up to four copies. The only recorded ruling on
this line (DESIGN-HISTORY, 2026-08-26 PP3, finding 1) is about `aux` being a
LENGTH for LIT/VAL nodes and says nothing about op nodes.

**Consequence.** PHIST lists `2+3 = 5` twice for one calculation and evicts a
real earlier formula sooner than it should. Both entries are truthful, so it
reads as a browsing glitch rather than a bug — which is why it survived five
rounds.

**Bug class.** Identity-by-instance for a fact that is about the formula, not
the node. Shares its root with `PP18RR2-3`: `ppcCurrent` and `PPA_EMITTED`
both name one arena node, and the continuation idiom deliberately makes two
nodes for one formula.

**Class-level test.** An emit-once property pin: for a table of terminator
sequences applied after an ENTER dup (CLSTK, CLX, DROP+CLX, FILL, STO-to-stack,
a superseding operator), assert `ppcHistoryCount() == 1` and that the surviving
entry's `= result` is right.

---

### PP18RR2-9 — the BIGOP decline arms rewrite `ppcStage.cls` after the STAGE switch has already branched, so the class's own supersede never runs and the formula files with no result

`packages/pretty-print/prettyCapture.c:760` and `:773` (`PPC_BIGOPSUM`) and
`:795` (`PPC_BIGOPINT`) — each `ppcStage.cls = PPC_INVALIDATE; break;` —
against `case PPC_INVALIDATE:` at `:822-834`, a **later case of the same
switch**, which cannot be entered by assigning `cls` inside an earlier one.

**What breaks.** At DONE the class is now `PPC_INVALIDATE`, so
`ppcInvalidate(true)` runs `ppcEmit(ppcCurrent, (calcRegister_t)-1)` — an
entry with no TKRES token. PHIST draws the row as a bare `2+3` with no `= 5`
tail, and `pbFindResult` returns NULL, so ENTER on that row closes the browser
and puts nothing in X. Any *other* unmodelled item, which classifies
`PPC_INVALIDATE` up front, files the same formula correctly as `2+3 = 5`.

**Reaching input.** With a global label named `X` in program memory:
`2 ENTER 3 + 1 ENTER 3 ENTER 1`, then the sum key (`ITM_SIGMAn`) with the
lettered-register parameter `X` — upstream's interactive form,
`solver/sumprod.c:308-327`. The param falls outside `FIRST_LABEL..LAST_LABEL`,
so the arm declines at `:760`. Same shape from `:773` (a step that is neither
`dtReal34` nor `dtLongInteger`) and `:795` (the INTEG setup form).

**Violated.** The `AUDIT R2-1` comment that states the ruling this breaks,
`prettyCapture.c:441-451`: *"Emitting with -1 is truthful but records NO
result, and the browser's ENTER can then never recall the entry — truthful and
useless. The classifier's INVALIDATE arm now emits at STAGE instead, where the
register genuinely still holds this formula's value."* The fix was applied to
`case PPC_INVALIDATE:` only; the three arms that *become* `PPC_INVALIDATE` are
the unenumerated members of the same class.

**The -1 site is exclusively theirs.** `ppcSupersedeCurrent` ends with
`ppcCurrent = PPC_NIL`, so an item classified `PPC_INVALIDATE` up front leaves
`ppcCurrent` NIL and `ppcInvalidate(true)`'s guard is false. The three decline
breaks are the only way to reach DONE with `cls == PPC_INVALIDATE` and
`ppcCurrent` still live — so the comment's "truthful last resort for a caller
that reaches invalidation without having staged" describes nothing: these arms
*do* stage.

**Not exempted.** `DESIGN.md:219-226` says only that the register-letter sum
form and the INTEG label form *"invalidate rather than mint a node that would
display a lie"* — a ruling about node minting, silent on the pending formula's
result. `DESIGN-HISTORY.md:1118`'s one documented no-TKRES case is top-of-stack
falloff, *"the real register is gone"* — a different path; here the register is
present at STAGE. B6b, the only pin on this path, drives `5 ENTER 7
INTEGRAL_YX` from a state with no open formula, so it pins "no node is minted"
and never exercises a pending root.

**Not executed.** This finding is a static trace with every hop named; it is
one of three in this round that were not reproduced in a build.

**Bug class.** Class reassignment after the switch has dispatched on the
original class — the unenumerated-member shape again, at the fix site of
`R2-1`.

**Class-level test.** Structural and cheap: for every arm that assigns
`ppcStage.cls`, assert that the assigned class's own STAGE-side work has run
(today: assert `ppcCurrent == PPC_NIL` on entry to `ppcInvalidate(true)`, which
makes the -1 branch provably dead). Behaviourally: for each BIGOP decline
input, assert the pending formula files **with** its result.

---

### PP18RR2-10 — a complex value can never become a capture VAL leaf, so any formula that must snapshot a complex operand is silently withheld; DESIGN.md promises a two-child header that does not exist

`packages/pretty-print/prettyCapture.c:215-217`
(`if(bytes > sizeof(((ppcNode_t *)0)->payload)) return ppcAlloc(PPN_OPAQUE);`).

**What breaks.** `dtComplex34` reports 8 blocks = 32 bytes against a 16-byte
node payload, so `ppcValLeafFromRegister` returns `PPN_OPAQUE`. The containing
tree is then poisoned for both surfaces: `ppcCurrentFormulaRoot` returns
`PPC_NIL` (T line blank) and `ppcEmit` refuses it (no history row). There is
**no `PPN_VAL2` kind** anywhere in `prettyInternal.h` — the enumeration is
FREE, OP1, OP2, LIT, LIT2, VAL, RCL, CONST, OPAQUE, BIGOP.

**Reaching input.** Put a complex in X (`2 ENTER 3` then `→CPX`; `ITM_REtoCX`
is `US_ENABLED` and unclassified, so it invalidates and every slot goes
UNKNOWN), then `4`, then `×`. STAGE's `ppcEnsureKnown(1)` hits line 215 for
the complex in Y. The same leaf is minted by `ppcRclLeaf` (RCL of a named
register holding a complex), the `LASTX` arm, the BIGOP limit snapshots, and
the NIM fallback — and by `ppcEmit`'s own `= result` snapshot at `:419`.

**Evidence (executed).**

```
AUDIT-PROBE R2: Ydt=2 blocks=8 bytes=32 payloadCap=16
AUDIT-PROBE R2: err=0 xdt=2 raw=2 shown=255
AUDIT-PROBE R2: rootkind=2 c0=8 c1=5     (OP2=2 VAL=5 OPAQUE=8)
AUDIT-PROBE R2: histCount=0
```

The multiply succeeded, the shadow *did* build a formula, its Y operand is
OPAQUE while its X operand is a proper VAL, the public root is withheld, and
the ring stayed empty — with `lastErrorCode 0` and no decline shown anywhere.

**Violated.** `DESIGN.md:174-177`, verbatim: *"value leaves store raw register
payloads ≤16 B (complex via a two-child header), formatted only at display
time"* — the two-child header is not implemented — and the next sentence
scopes OPAQUE to *"Matrix/string/oversized payloads"*, not to ordinary complex
registers. No Non-goals section excludes complex capture; §2 explicitly
supports complex value *rendering*, and `prettyFormula.c:50` already carries a
`dtComplex34` formatting arm this path can never reach.

**Correction against the finder, recorded.** The headline overstates it: a
formula that *computes* a complex through modelled ops keeps an all-real tree
and displays fine. What is withheld is any formula that must snapshot a
complex operand out of an UNKNOWN slot — and the `= result` snapshot, which is
the same screen.

**Bug class.** A documented representation that was never implemented, whose
absence is expressed as a silent withholding rather than a decline.

**Class-level test.** A data-type coverage table: for each register data type
× each capture path that snapshots a register (`ensureKnown`, `RclLeaf`,
`LASTX`, BIGOP limits, the result snapshot), assert the intended outcome —
VAL leaf, OPAQUE, or invalidate — against a written table rather than against
whatever the size test happens to do. That table is also where the ruling
belongs: implement `PPN_VAL2` or amend §3.

---

### PP18RR2-11 — `PPC_RCLARITH` allocates the OP2 node before validating the operand copy and never returns it on the failure path, leaking one of the 24 arena nodes per occurrence

`packages/pretty-print/prettyCapture.c:1050` (the `ppcAlloc(PPN_OP2)`) and
`:1052` (the guard that frees only `r`). **Found by two dimensions**
(arithmetic, guards) and executed.

**What breaks.** On the in-stack branch, `r = ppcDeepCopy(ppcSlot[k])` runs
first. `ppcDeepCopy` unwinds *completely* on failure — it frees both children
and hand-returns its own root to `ppcFreeHead` — so a failed copy always
leaves the free list non-empty, which means the following `ppcAlloc(PPN_OP2)`
does not merely happen to succeed, it is **guaranteed** to. The compound guard
`if(n == PPC_NIL || r == PPC_NIL)` then frees only `r` (already NIL, a no-op)
and calls `ppcInvalidate(false)`, which reclaims by walking
`ppcSlot[]`/`ppcSlotL` — and `n` is in neither. The node stays kind `PPN_OP2`,
off the free list, unreferenced, for the rest of the session: `ppcInit` is the
only thing that rebuilds the free list, and it runs only on cold start or
`RESET`.

**Reaching input.** Build a large tree, then RCL-arith a stack register whose
slot holds it: `1 ENTER 2 +` then `3 +` seven times (a 17-node tree), type a
digit to lift it into Y, then `RCL` `+` `Y`.

**Evidence (executed).**

```
AUDITPROBE build sig=1 2 + 3 + 3 + 3 + 3 + 3 + 3 + 3 +  nonfree=17
AUDITPROBE after RCLADD-Y  nonfree=1  err=0  sig=-
   … with the sibling arm's `if(n != PPC_NIL) { ppcFreeTree(n); }` added:
AUDITPROBE after RCLADD-Y  nonfree=0  err=0  sig=-
```

`sig=-` means the shadow fully invalidated and every slot is empty — and one
arena node is still off the free list. The gate was GREEN in both runs, so
nothing currently pins it.

**Violated.** The file's stated degradation contract, `prettyCapture.c:11-17`
and `DESIGN.md:164-168`: *"the whole shadow invalidates … over-invalidation
only costs history granularity."* Here it also costs arena capacity that never
returns, so the cost is not bounded by the invalidation. The project's only
ruling on arena exhaustion (DESIGN-HISTORY, 2026-08-26 PP3, finding 3) states
the opposite expectation — *"Arena exhaustion recovers BETTER than designed:
after the invalidate, ensureKnown rebuilds truthfully from value leaves and
the chain continues"* — a recovery model that presupposes invalidation returns
nodes to the free list.

**The intended shape is eleven lines below.** The BIGOP failure path at
`:1091-1100` does exactly `if(n != PPC_NIL) { ppcFreeTree(n); }` for the
identical partial-construction failure. The two simple arms (`PPC_DY` `:856`,
`PPC_MO` `:878`) guard on `n == PPC_NIL` alone and cannot leak, which leaves
`PPC_RCLARITH` as the single unhandled instance rather than a house style.

**Severity is gradual, and stated as such.** Each occurrence costs 1 of 24
nodes. The endpoint — every `ppcAlloc` failing, so the T line and the history
show nothing at all for the rest of the session with no message — needs the
state repeated many times; the intermediate effect is a steadily lower ceiling
on formula size, surfacing as ordinary invalidation.

**Bug class.** Allocate-before-validate with a compound guard that frees only
one of the two allocations.

**Class-level test.** An arena-conservation pin: after any sequence that ends
in a full invalidate, assert the count of non-`PPN_FREE` nodes is zero. Run it
over a table of failure arms (deep-copy exhaustion in `PPC_DY`, `PPC_MO`,
`PPC_RCLARITH`, both BIGOPs, `PPC_ENTER`'s dup, `PPC_FILL`). It is four lines
of harness and it would have gone red on the day this arm was written.

---

### PP18RR2-12 — `ppfBuildEntry`'s postfix operand stack is fixed at 8, so a right-nested formula the T line draws is refused when it comes back out of the history ring

`packages/pretty-print/prettyFormula.c:480` (`uint8_t stackNode[8];`), against
`ppcSerializeNode` (`prettyCapture.c:265-365`), which is recursive and has no
depth bound.

**What breaks.** The producer stores any tree the 24-node arena can hold; the
consumer's postfix walk can hold only eight operands. A right spine pushes all
its operands before the first combine, so a depth-9 right-nested formula
stores and then cannot be rebuilt — `ppfBuildEntry` returns false at `:491`/
`:518`/`:531`, `ppfBuildRow` returns false outright, and `pbPaint` falls to
`PB_UNSHOWN_H` and prints "(too large to show)".

**Reaching input.** `1 ENTER 2 +`, then `n x<>y +` seven times. `ITM_XexY` →
`PPC_SWAP` puts the accumulated root back in X, so the next dyadic op takes it
as `child[1]` instead of `child[0]`; no supersession fires because
`ppcSlot[0] == ppcCurrent` at every STAGE. Then `CLX` displaces it and
`ppcEmit` files it.

**Evidence (executed).**

```
AUDITPROBE sig='9 8 7 6 5 4 3 1 2 + + + + + + + +'
AUDITPROBE buildCurrent=1   liveW=151  liveH=12
AUDITPROBE histCount=1      entry=1  len=75
AUDITPROBE buildEntry=0     buildRow=0
```

151 × 12 px against a 392 px pan threshold and a 139 px band; 75 bytes against
the 320-byte serialize cap and the 640-byte ring. The row is nowhere near any
limit the message names.

**Violated.** The project's ruling on absent rows, `prettyBrowser.c:48-57`
(`AUDIT R4-4`): *"A row that cannot be drawn still EXISTS, and the owner is
entitled to see that it does"*, and `prettyFormula.c:681-691` (`AUDIT R1-10`),
which rejected exactly this outcome (*"not panned, not truncated, not marked —
it was ABSENT"*). Nothing in `DESIGN.md` §5 bounds a formula's postfix depth;
the one documented drop rule is *"Oversized entries (> half the ring) are
dropped, not stored"* (`DESIGN.md:290`), which this entry is four times under.

**Correction against the finder, recorded.** The consequence's second half —
that the PHIST pager silently skips the row — is real code but is **not**
reachable by pressing PHIST inside `CM_PRETTY_BROWSER`: the `AUDIT R3-7`
containment guard at `keyboard.c:2819` marks every resolved item except
`ITM_dotD` as processed in calcMode 20, so `fnPrettyHist`'s manual-pager body
never runs from the keyboard there. The blast radius is the browser row alone.

**Bug class.** Producer and consumer of one serialized form bounded
differently, with the mismatch surfacing as a false explanation to the owner.

**Class-level test.** A producer/consumer agreement pin: build the deepest and
widest trees the arena can hold in both spine directions, file them, and assert
that everything `ppcSerializeNode` stored, `ppfBuildEntry` rebuilds. The same
pin retires the hand-chosen `8`.

---

### PP18RR2-13 — `ppParseExponent` writes into `ppSpanA`, which `ppParseComplex` hands it as `src`, and then reads the exponent digits back out of the buffer it just clobbered

`packages/pretty-print/prettyValue.c:277-280` (the writes) against `:285-290`
(the read), reached through `:686` (`ppParseRealAny(ppSpanA, …)` with `ppSpanA`
filled at `:676-677`).

**What breaks, and why it does not break today.** `ppParseExponent` computes
`baseLen = markOff + 2` and writes `ppSpanA[markOff+2..markOff+4]`, but
`expOff` is `markOff + 4` (product glyph 2 bytes + `STD_SUB_10` 2 bytes), so
the NUL at `:280` overwrites the high byte of the first exponent glyph
**before** the extraction loop reads `src` from `pos = expOff`. The loop then
emits a spurious leading `'0'`, reads the low byte `0x60+d` as a standalone
code, and realigns; for a superscript minus (0xa16b) the low byte yields
`'0' + (0x6b & 0xF)` = `';'` — the sign becomes a semicolon. It is UNREACHED
**only because of `PP18RR2-1`**: `complex34ToDisplayString2`
(display.c:1552-1554) unconditionally appends `STD_SPACE_HAIR` to the real
part, and the post-marker branch declines on it at `:263` before reaching the
writes.

**Reaching input.** None today, and that is the finding. `1.5e-50 + 2i` would
render its exponent as `0;50` — sign lost, magnitude misread, no decline, no
error — the moment either the hair space stops being appended or a future
caller passes `ppSpanA` a bare exponent form. **This is why it is reported:
fixing `PP18RR2-1` as written un-blocks it.** The two must land together.

**Violated.** `prettyValue.c:9-14`, the builder-first invariant: *"the upstream
display builder runs first and its OUTPUT is parsed into the tree. The pretty
form can never disagree with what upstream would have shown."* A parser that
mutates the string it is parsing cannot make that guarantee, and `DESIGN.md`
§1's fallback rule (*"Any failure … paints nothing and returns false"*) is
unavailable once the input has been destroyed mid-parse.

**Ruling search, negative.** No comment, no `DESIGN.md`/`DESIGN-HISTORY.md`
entry, and no stage sheet documents the write into the file-static as
deliberate, or records that the buffer may be the caller's own `src`; the
declarations at `:20-22` carry no comment at all. `ppParseComplex`'s own header
documents the assembly it parses (`re ± [i·im | im␣␣i]`) and does not even
mention the hair space that is keeping this path shut. The nearest prior
disposition — the PP1–PP16 audit's §6c, *"`ppSpanA` re-entrancy in
`ppParseComplex` … UNREACHED"* — clears a **different** mechanism (its stated
reason is that the 512 B text pool cannot be near-full, i.e. the
decline-after-`ppNewRun`-fails path) and never mentions the `expOff` byte
clobber. `git diff` confirms both functions are byte-identical to the tip that
audit read, so its disposition covers this code but not this mechanism.

**Dimension disagreement, recorded.** One dimension listed this in its own
*cleared* list ("UNREACHED, blocked by the hair space"); another filed it; the
refutation pass upheld the filing on intent. Both readings are defensible; the
tie-breaker is that the blocker is upstream's, is conditional, and is
documented nowhere.

**Bug class.** A parser that mutates its input buffer, aliased through a
file-static, with the aliasing invisible at the call site.

**Class-level test.** An aliasing pin: call each parser with `src` pointing at
its own scratch buffer and assert the parse result equals the result from a
private copy. Three lines per parser, and it is the pin that has to exist
before `PP18RR2-1` is fixed.

---

### PP18RR2-14 — FV11's "toggled ON" assertion probes the exact band where T's own value is drawn, so the whole PP8 T-line surface has no pin that fails when it stops running

`packages/pretty-print/prettyTest.c:2231`
(`ppTestRectAnyLit(PPT_T_TOP + 1, PPT_T_TOP + PPT_T_ROWS - 1, 300,
SCREEN_WIDTH - 1)`).

**What breaks.** FV11 is the **only** render pin for the entire PP8 surface
(`grep -n REGISTER_T prettyTest.c` returns lines 2216/2220/2229 and nothing
else), and all three of its assertions survive the surface being deleted:
(1) snap0 vs snap1 are both taken with the surface in the same state, so they
match either way; (2) the "some ink in the T band" probe is satisfied by T's
**own** right-aligned value glyphs — the fixture never sets T, never captures
the value render for comparison, and the T-line value is drawn in exactly that
band; (3) the X-band identity is unaffected because X never takes the T branch.

**Evidence (executed, both directions).** Mutating `prettyValue.c:795`:

| mutation | meaning | gate |
|---|---|---|
| `if(false)` | the PP8 surface is dead | **GREEN** |
| `if(regist == REGISTER_T)` | the opt-in flag is ignored | **GREEN** |

Mutation presence was verified in the compiled shadow tree
(`build.sim/custom_pkg_shadow/prettyValue.c:795`), so neither green is a stale
`files/` artifact. Because mutation A leaves only the ordinary value render to
paint, the green also directly confirms the mechanism: T's own glyphs satisfy
the probe.

**Violated.** The pin's own comment two lines above: *"toggled ON: the T line
must DIFFER from the value render (the formula \"2+3\" paints instead of T's
value)"* — the body never compares against the value render, although
`ppTestSnap[1]` already holds it. And `DESIGN.md:527`: *"FLAG_PTLINE is a
second, independent opt-in for the T-line live formula, default OFF."* Either
half can break silently: a T line that shows the formula for an owner who
never set PTLINE (T's value becomes unreachable), or a PTLINE the owner sets
that draws nothing.

**What is genuinely covered, so the finding is scoped.** `TESTING.md:220-221`
catalogues MUT-30 (default flipped to ON) and MUT-31 (branch loses its
`regist` check); both are really caught by FV11's two identity assertions. The
gap is precisely the middle assertion — the toggled-ON half.

**No ruling sanctions it.** `DESIGN-HISTORY.md:979` asserts the opposite
(*"FV11 pins all three properties … the formula appearing when toggled"*), and
the PP16 audit's §6e asserts that every pixel probe was checked for a region
something other than the feature could light.

**Bug class.** A probe satisfied by a region the feature does not own.

**Class-level test.** Differential, and the data is already in hand: compare
the T band against `ppTestSnap[1]` (the value render) and assert they DIFFER,
which is exactly what the comment says the pin does. The general rule this
instantiates: an "any ink here" probe needs a stated exclusion argument, and
where a snapshot of the same band without the feature exists, the comparison
is strictly better.

---

### PP18RR2-15 — FV6's ink probe is satisfied by the browser's 3-pixel selection marker, so the fallback it exists for cannot turn it red

`packages/pretty-print/prettyTest.c:2058`
(`ppTestRectAnyLit(21, 56, 0, SCREEN_WIDTH - 1)`), against
`browsers/prettyBrowser.c:95` (`lcd_fill_rect(0, y, 3, h,
LCD_EMPTY_VALUE)` for the selected row, drawn **before** the build result is
consulted).

**What breaks.** `fnPrettyHist` from CM_NORMAL hands off to `prettyBrowser`,
whose pass 2 paints the selection marker for row 0 at y=25 with
`LCD_EMPTY_VALUE` — which the test HAL maps to `BLT_OR`, i.e. pixels ON
(`src/testSuite/hal/lcd.c:68`; the same polarity FV5 relies on). Columns 0–2
across rows 25.. are inside FV6's rectangle, so the probe passes whether or not
the row was built.

**Evidence (executed).** Forcing FV6's row to fail both rungs (row-height gate
→ `h > 10`):

```
AUDIT-PROBE R2 totalRows/haveCurrent (expected 1, actual 1)
AUDIT-PROBE R2 ink cols0-2 rows25-44 / ink rows60-160 (expected 1, actual 0)
AUDIT-PROBE R2 FV6 row DOES NOT BUILD
```

— and **no** `FV6 tall formula missing from the pager`. The formula was
entirely absent from the browser, the row painted "(too large to show)", and
FV6 reported green.

**Correction against the finder, recorded, and it makes the picture worse.**
The finding's own named mutation — deleting the second rung at
`prettyFormula.c:651` — does **not** make FV6's row fail: the continued
fraction `1/(2+3/(4+5/6))` measures 60 px at `PP_FONT_STANDARD`, well under
the 139 px band, so FV6 never reaches the tiny rung at all. That mutation
reddens T25/T28/T29 (the width half of the ladder) and leaves FV6 honest. So
the "whole-tree tiny re-font is unverified by FV6" half of the consequence is
not latent — it is **already true today**: FV6's fixture never exercises the
rung it was written for.

**Violated.** The pin's stated purpose at `prettyTest.c:2033-2035`: *"a formula
too tall for the pager's standard rung must still show — the tiny rung
re-fonts the whole tree"*, and `TESTING.md:78-81` (*"pins the variable-height
packing and the whole-tree tiny re-font"*). Also the file's own standing
fixture rule, quoted at `prettyTest.c:1665` (`AUDIT R1-11`): *"A fixture must
prove it reached the state it claims to test."*

**Bug class.** Same as `PP18RR2-14` — a probe satisfied by a region the
feature does not own — compounded by a fixture that does not reach the state
it names.

**Class-level test.** Two parts, both small: exclude the marker columns from
the probe (or assert the built row's height directly, which the harness can
read), and give the pin a fixture whose measured height actually exceeds the
standard rung, asserted as a precondition. The documented instruction not to
force the unbuildable-row branch artificially (`TESTING.md:459-488`) is not in
tension with either.

---

### PP18RR2-16 — B10's dispatch-depth pin, the only pin for `R1-3`, is wrapped in a label guard with no `else`, so a failed fixture load skips it silently

`packages/pretty-print/prettyTest.c:1592`
(`calcRegister_t eLbl = findNamedLabel("E", GLOBAL_LABELS); if(eLbl !=
INVALID_VARIABLE) {` … no `else`).

**What breaks.** If label `E` does not register, the entire pin body
disappears with no output and a green suite. `ppcTestWriteAndLoadPgm` reports a
failed `fopen` but ignores `fnLoadProgram`'s outcome, so a load failure is
silent on both sides.

**Evidence (executed, both halves).**

```
probe B: eLbl forced to INVALID_VARIABLE   -> gate GREEN, zero B10 output
probe C: R1-3's bug reintroduced           -> exactly ONE failure in the
         (`ppcStage.depth != myDepth` dropped)   whole battery:
         prettyPrint test FAIL: B10 failed sum left a formula behind
                                (expected '-', actual '{#,#}...')
```

Composed: B10 is the sole detector for the dispatch-depth pairing, and it is
deletable without a sound. The bug it guards is a shadow that keeps claiming a
finished sum after a big operator's program failed mid-loop — a formula filed
against a register value that never existed.

**Violated.** The fixture rule the same block quotes four lines below, at
`prettyTest.c:1600-1604` (`AUDIT R3-9`): *"this is the ONLY pin for R1-3's
dispatch-depth pairing, and it used to assert only INSIDE `if(lastErrorCode !=
ERROR_NONE)` — so if the program ever stopped failing mid-loop the pin
vanished silently and the suite still passed. Assert that we REACHED the
failure."* The inner vacuity was fixed; the outer one, one line above it, was
not. The file's own precedents do it correctly: B0 (`:1484`) fails loudly on a
missing label P, as do `ppvTestExpect` (`:3424`) and `ppvTestDecline`
(`:3446`).

**Correction against the finder, recorded.** Both named reaching inputs are
overstated. A loader-path failure strands `pgmP` too, and B0 — which encloses
B10 — fails loudly on that, so the suite goes red rather than green; and
`pretty_print` runs at position 270 of `testSuiteList.txt` while `programs` is
at 443, so the driver-appended program bulk cannot fill program memory ahead of
B10. The trigger narrows to *any change that makes the `E` lookup miss*. It
does not vanish: a probe accident cut the other way — `findNamedLabel("Z",
GLOBAL_LABELS)` at that point in the suite returned a **valid** register
although nothing in the package or upstream declares a global label `Z`, so
label state there is suite-global, not hermetic, and the guard's outcome is not
statically guaranteed.

**Bug class.** A fixture guard with no `else`: a skipped pin is a green pin.

**Class-level test.** A grep-level rule with an executable form: every
`if(<fixture available>)` in `prettyTest.c` has an `else ppTestFail(...)`.
Three sites in the file already do it; this one and any future one should be
checked mechanically rather than by review.

---

### PP18RR2-17 — S4's oracle cannot tell the SHOW fallback from the pretty surface having painted the unsupported type itself

`packages/pretty-print/prettyTest.c:528`
(`if(temporaryInformation == TI_NO_INFO) ppTestFail("S4 fallback did not reach
SHOW");`).

**What breaks.** `fnPrettyShow`'s **success** path also leaves
`temporaryInformation != TI_NO_INFO` — it sets `TI_SHOWNOTHING` at
`prettyValue.c:884`. So any change that lets `ppBuildRegister` accept a type it
should decline satisfies S4 while the ordinary SHOW is never reached.
`screenHoldsDrawnPixels`, which does discriminate, is reset immediately before
the call and never read.

**Evidence (executed).** With a `dtString` arm added to `ppBuildRegister`:

```
prettyPrint test FAIL: AUDIT-PROBE R2 S4 painted the pretty surface
prettyPrint test FAIL: F1 string band differs
```

`S4 fallback did not reach SHOW` did **not** appear. S4 passed while the
pretty surface painted the string and `fnC47Show` was never called.

**How it went weak, which is the useful part.** S4 was written at PP2, when the
success path left `temporaryInformation` alone and `!= TI_NO_INFO` genuinely
discriminated. The 2026-08-27 EXIT fix added `TI_SHOWNOTHING` to the success
path and destroyed that discrimination without touching the pin — the
project's own audit-fix-regression shape, in a test rather than in code. The
history entry that records the fix even criticises S4 (*"the pin that should
have made the asymmetry visible and did not"*) without noticing that the same
commit had just disarmed its remaining half.

**Violated.** The driver's stated case in `tests/pretty_print.txt`
(*"unsupported type falls back to the ordinary SHOW"*, echoed by
`DESIGN.md`/`TESTING.md:64`, *"temporaryInformation leaves TI_NO_INFO"*) and
the assertion's own message, which the body does not establish.

**Honest mitigation.** F1 in `prettyTestFallback` also went red on that
mutation, so the shared builder is not wholly unpinned — but F1 pins the
**inline register line**, not PSHOW, and the two are coupled only because they
share `ppBuildRegister` today (the `R1-13` checkHP note records these surfaces
diverging once already). A PSHOW-side-only acceptance would leave nothing red.

**Bug class.** An oracle both branches satisfy.

**Class-level test.** Use the idiom the file already has: FV20 (`:2513`)
checks `screenHoldsDrawnPixels` for exactly this discrimination. Assert it
false for the fallback case, and true for the pretty case, for a table of
register data types spanning the decline list (string, long integer, matrix,
short integer, date, time, config).

---

## 4. PLAUSIBLE, and one UNVERIFIED finding the cap did not reach

**No new PLAUSIBLE finding this round.** Every finding raised either
constructed its reaching input or was refuted. `PP18RR2-13` is the closest
call and is deliberately **not** filed here: its input is not merely
unconstructed, it is *provably blocked today* by an upstream guarantee, and
the mechanism is fully traced — which is a different statement from "nobody
could reach it", and it earns a place in §3 because the fix for `PP18RR2-1`
removes the blocker.

### UNVERIFIED — no independent refutation pass ran on this one

**`DESIGN.md`'s adjacency table still claims a separation from undo-history's
testSuiteList hunk that this range removed.** `design-docs/pretty-print/
DESIGN.md:788` (the `testSuite/testSuite.c` + `testSuiteList.txt` row) asserts
two things that are no longer true of the tree: that pretty-print contributes
*"ONE hunk (three `funcTestNoParam` rows …) + one list line after `matrix`"*,
and that the *"List line [is] far from `nested_cov` (undo-history) and EOF
(forth-core)"*. The mechanical facts were re-derived in the main tree while
writing this report: the testSuite.c patch adds **nine** driver rows (+12
lines, one hunk), and the list patch has **two** hunks — the second,
`@@ -501,6 +502,13 @@ shortint_restore_cov`, carries `graphs_cov` and
`nested_cov` as context, and undo-history's single hunk is
`@@ -505,5 +505,7 @@ graphs_cov`. Lines 505-506 are context in **both** patches
and the two insertion points are four lines apart.

Nothing breaks today — `pkg_patch_apply.py` uses `git apply --3way` against the
pristine blob, both sides are pure insertions, and the combined gate is green.
The cost is paid at the next upstream merge by whoever reads the table to know
the surface: if upstream edits the `serialize_cov`..`config_cov` band, the two
packages conflict in a region the authoritative doc says was checked clear.
The new §7 row this same range added (`DESIGN.md:772`) compounds it — it
reasons explicitly about forth-core's EOF hunk and never mentions
undo-history's, which is the nearer of the two siblings to the anchor actually
chosen. This is the `C-4` class `REVIEW_upstream-minimality_2026-08-27.md` §3
already named (*"the §6 hook table is stale and incomplete … it is what a
future rebaser reads to know the surface"*), recurring at a new site: there the
defect was files missing from the table, here it is an existing row whose
safety claim this range falsified. *What would settle it:* nothing to
reproduce — the fix is an edit to two doc rows. It is listed here rather than
in §3 only because no independent reader tried to knock it down.

### Carried forward, still open, not re-examined

Listed for the ledger only: `PP18RR1-P1` (the 147-px centering boundary),
round 4's P1 (8–14-glyph `MVAR` through the import channel) and P2
(`PP_MAX_DEPTH` composition), round 2's P1 (non-numeric first `MVAR`), round
3's P1/P2 (V65 ordering) and P3 (`ppvAstPrec`'s missing NIL guard).

---

## 5. Design observations (D7)

Shape, not defects. Six observations; the first two organise most of this
round's findings, and the last two are about why the round found them at all.

**PP18RR2-D1 — the capture engine can only repair what the dispatch tells it
about, and four things move the stack without telling it.** The engine has
exactly one place that knows how to recover from a failed operation
(`ppcInvalidate` at DONE), and it is reachable only through a stage that a
classified dispatch created. Everything else is outside its reach: upstream's
three direct `fnRecall` calls (`PP18RR2-2`), `SST`'s deferred program step
(`-5`), `closeNim`'s error exits (`-7`), and `fnStore`'s post-dispatch write
(`-4`). Four of this round's top seven findings are one sentence — *the mirror
is complete for what goes through the dispatch and blind to what goes around
it* — and the instrument that pins all four is not a picture oracle but a
**quiescence assertion**: after every key, for every slot k that is not
UNKNOWN, the slot's value must equal register `REGISTER_X + k`. That is the
§3 invariant written as code. It is the single most valuable thing this report
proposes.

**PP18RR2-D2 — formula identity is arena-node identity, and the design's own
continuation idiom makes two nodes for one formula.** `ppcCurrent` names an
instance; `PPA_EMITTED` is stored on an instance; ENTER's dup deliberately
creates a second instance and `ppcDeepCopy` clears the flag on it. Free the
wrong one and the formula vanishes (`PP18RR2-3`); file both and the history
doubles it (`-8`). `R3-3` already paid for half of this once. The shape of the
answer is a formula-level identity (a sequence number minted at root creation
and copied, not cleared) rather than another site-by-site patch.

**PP18RR2-D3 — the mode axis has a second column, and the capture engine fills
one cell of it and not the other.** `PP18RR1-D1` named the walker as a
simulator of one configuration. The capture engine is the file that report held
up as the good example — it *does* read `FLAG_ERPN` and is pinned for it
(T15) — and it does not read `FLAG_SSIZE8` at all (`grep SSIZE
prettyCapture.c` is empty) while its slot array is 8 wide and four of its
guards use `getStackTop()`. The producer-by-flag table wants both columns:
for each flag that changes the semantics a surface mirrors, the surface reads
it, models it as a documented constant, or declines. The package already
demonstrates the third option's discipline — `prettyVisual.c:49`,
`#define PPV_STACK_SLOTS 8   ///< SSIZE8 simulated regardless of the flag` —
and the contrast is exactly what makes `PP18RR2-6` a defect rather than a
choice: the author documents the choice where he makes it, and there is no
counterpart in the engine that mirrors the *live* stack.

**PP18RR2-D4 — the classifier's default rule delegates to upstream a predicate
upstream does not maintain, and the carve-out list is the evidence.**
`prettyCapture.c:576-586` states the rule as binding and justifies it: *"Upstream
maintains US_STATUS for its own undo correctness, so it maintains our predicate
too."* Directly above it sits a block headed *"stack mutators that are
US_UNCHANGED and would otherwise be ignored"* naming `ITM_UNDO`, `ITM_RS` and
rows 427/428. The delegation is known to admit counterexamples and is patched
by name each time one is found; `SST` (`PP18RR2-5`) and `SSIZE4`/`SSIZE8`
(`-6`) are the next two. `US_UNCHANGED` means *upstream's undo data stays
valid*, which is a weaker claim than *the shadow's predicate stays valid* —
those two coincide for value writes and diverge for stack-shape changes and
for deferred execution. Either the list is enumerated once mechanically (all
`US_UNCHANGED` rows whose implementation touches the stack) or the rule is
restated as what it actually is: a heuristic with a hand-maintained exception
list.

**PP18RR2-D5 — five of this round's mutations left the gate green, and the
idioms that would have caught them are already in the file.** FV11 and FV6 are
"any ink in a band" probes over bands other things paint; S4 is an oracle both
branches satisfy; B10 is a guard with no else. Meanwhile F3 — a *differential*
pin comparing `displayValueX` with the feature on and off — went red on a
one-token upstream mutation with exactly one useful failure, and FV20 uses
`screenHoldsDrawnPixels` to tell "we painted" from "we fell back". The working
idiom is comparison against the same surface with the feature disabled; the
failing idiom is a positive existence probe with no exclusion argument. This
is not a call to rewrite the suite: it is a rule for new pixel pins, and a
short list of four existing ones to convert.

**PP18RR2-D6 — a parser pin fed by a hand-typed fixture is a pin against your
own assumption, and this round has the receipt.** `PP18RR2-1` is one missing
2-byte glyph. The fixture that should have caught it (M6) was typed by hand
with the trailing hair space omitted; the sibling fixture for the *other*
parser (`prettyTest.c:215`) was copied from the real `display.c` template and
carries it. Five audit rounds read past the difference, because reading a
fixture tells you what the author believed the builder emits. The rule that
falls out is narrow and cheap: **any pin over a parser whose input is another
component's output must obtain that input by calling the producer.**

---

## 6. Deliberately not flagged

Merged from what the eight finders reported clearing and what the refutation
pass disproved. Mandatory section, and it is most of what this round bought:
`prettyCapture.c` and `prettyValue.c` were read end to end for the first time,
and the honest headline is that their arithmetic, their bounds and their
decline paths hold almost everywhere — the seventeen findings cluster on four
axes (bypassed dispatch, node identity, the mode flag, and probe oracles),
not across the files.

### Killed by the refutation pass

**"The builder-first invariant rests on a hand copy of upstream's arm
predicate, in a file the package overrides wholesale, with nothing relating
the two"** (raised against `prettyValue.c:731`). **REFUTED**, and by
measurement rather than argument. The design ruled on exactly this and did not
leave the ruling as prose: `TESTING.md:48` names F3 — *"`displayValueX`
parity: fraction rendered pretty vs upstream — `displayValueX` strings
byte-identical (pins the builder-first rule)"* — and because upstream's arm and
`ppBuildRegister` write `displayValueX` through *different* builders, that
comparison **is** a differential test of the two predicates against each other.
Narrowing the upstream FRACT gate (`screen.c:3940`, `const34_1e6` →
`const34_1on10`) while leaving the package copy alone turned the gate RED with
exactly one failure: `F3 displayValueX differs`. The divergence is neither
silent nor invisible; whoever resolves an upstream refresh of that arm gets a
loud, named failure. The finding also overstated "nothing relating the two":
`prettyValue.c:726-727` says it in terms.

*What survives is smaller and is not a design flaw:* F3 exercises one fixture
value (1.234567) and one flag state, so a divergence confined to a magnitude
band no fixture crosses would slip, and the `prefixWidth`/`SCREEN_WIDTH`
argument half has no pin at all. That is a class-widening of F3 — a small value
table, one below and one above the 1e6 boundary — not a finding.

### Arena, ring and serialization arithmetic, re-derived rather than assumed

- **The eviction loop cannot spin.** `while(ppcHistCount >= PPC_HIST_MAX ||
  ppcHistUsed + off > PPC_HIST_BYTES)` looks unbounded at count 0, but `buf` is
  `PPC_HIST_BYTES/2` and is also the serializer's cap, so `off <= 320 < 640`
  and the second predicate is false at count 0. This is `DESIGN.md`'s
  *"Oversized entries (> half the ring) are dropped"* implemented as a
  serialize failure.
- **`ppcHistEvictOldest`** reads `ppcHistOffset[ppcHistCount]` only after the
  decrement — in bounds even at the full count of twelve — and preserves
  `offset[0] == 0` inductively, which is what its `memmove` depends on.
- **Every token arm's space test** is correct or conservative by one
  (TKO1/TKO2/TKC/TKR write 3 and test 3; TKV writes 6+bytes and tests 7+bytes;
  TKBIG writes and tests 21; TKL writes and tests 2+total), and the `off ==
  0xffff` poison propagates correctly through the recursive arms.
- **The literal-leaf size chain is exact and closed**: `PPC_LIT_CAPACITY` 30 →
  `ppcNimText[32]` → 15/15 across `payload[16]` + LIT2 → re-joined into
  `text[32]` in both the serializer and `ppfFromCaptureNode` → survives
  `ppfBuildEntry`'s `len >= sizeof(text)` gate. The `R1-14` comment explaining
  why the gate is 30 and not 32 is correct.
- **The value-leaf recipe is byte-exact against upstream, not merely
  bounded**: `TO_BYTES(getRegisterFullSizeInBlocks())` includes the
  `strLgIntHeader` for `dtLongInteger`/`dtString` and `getRegisterDataPointer`
  points *at* that header, so the copy takes the whole allocation and no more;
  `ppcAllocParamOf` returns exactly what `reallocateRegister` re-derives.
- **`ppcDeepCopy` returns every partially-allocated node on failure** (both
  children via `ppcFreeTree`, its own root by hand). `PP18RR2-11` depends on
  this being true, and it is.
- **No slot aliasing survives a transform.** `ppcShiftDownAfterConsume`,
  `PPC_DROPY`, `PPC_FILL`, `PPC_ENTER` and `PPC_LASTX` each leave two slots
  transiently pointing at one node and each resolves it with a deep copy whose
  failure degrades to UNKNOWN rather than sharing; `ppcFreeTree`'s `PPN_FREE`
  guard covers the rest. Verified against upstream `_Drop`, which copies at
  top-1 rather than top — value-identical.
- **`ppcFreeTree` nulling `ppcCurrent`** was checked on every free path,
  including `ppcDeepCopy`'s hand-return, which can only be freeing the node it
  just allocated.
- **Bounds in `prettyValue.c`**: `ppMapDigits`'s `o + 2 >= outSize` reserves
  room for a 2-byte copy *and* the NUL; `ppParseComplex`'s `sepOff` and
  `imEnd - imStart` guards leave room for the terminator; `ppParseIrfrac`'s
  `mapped[32]` writes leave index 31 for the NUL; every 2-byte glyph read is
  preceded by a torn-glyph check; `expd[24]` and the 30-char multiple
  truncation are unreachable (a real34 exponent is at most four digits and a
  sign; a longer multiple would already have declined on a separator glyph).
- **`ppScratch[200]`** against the upstream builders: both are called with
  `maxWidth = SCREEN_WIDTH` and shrink digits until the string fits 400 px of
  `numericFont`, which bounds the byte count far below 200 even with two-byte
  glyphs.

### Fail-closed paths that are actually closed

- **All four parsers decline before painting.** `ppReset()` runs per rung, so a
  half-built tree is discarded whole; `ppAppendChild` no-ops on `PP_NONE`, so
  `ppParseComplex`'s append-before-check at `:690` cannot corrupt the tree it
  abandons three lines later; partial pool consumption on a declining
  alternative (up to ~24 nodes) is harmless against a 72-node pool cleared per
  rung.
- **The rung ladders are correct in both spellings.** `prettyTryRegisterLine`
  returns false on the first build failure while `fnPrettyShow` breaks — same
  semantics, because a parse failure is font-independent — and
  `fnPrettyShow`'s `break`-then-`fnC47Show` honours §6's *"the user always gets
  a SHOW"*. `ppRenderRightAligned` measures and bounds-checks before its single
  paint, so no rung can paint and then fail.
- **Degradation to UNKNOWN on arena exhaustion is the design, not a bug.**
  `ppcTreeHasOpaque` deliberately returns true for UNKNOWN, so a starved tree
  is withheld rather than shown wrong; T12 pins the recovery.
- **`prettyBrowserEnter`'s RAM-full path** leaves `lastErrorCode` set and
  invalidates immediately after — fail-loud, and the same shape upstream's own
  recall paths use. The silent close on `ERROR_RAM_FULL` is a refusal, not a
  wrong result; below the bar.
- **`prettyTryRegisterLine` not writing `*lineWidth` on the false path** is
  safe: `lineWidth` is initialised at `screen.c:3231` and every upstream arm
  the false return falls through to assigns it before `lineTWidth = lineWidth`.
- **`ppfBuildEntry` pushing a `PP_NONE` run** cannot escape: `ppcEmit` refuses
  any root that is not OP1/OP2/BIGOP, so every entry contains a combine, and
  all three combiners test their operands.

### Guards whose conjuncts were falsified, or proved load-bearing

- **`ppcScopeOk`**: all four conjuncts falsifiable and all four needed.
  `PGM_SINGLE_STEP` passes it, and that is correct — an SST'd step moves the
  stack exactly as an interactive press would. (What is *not* covered is the
  step itself, which is `PP18RR2-5`.)
- **`case ITM_CONSTpi`** looks subsumed by the `func == fnConstant` test twelve
  lines below and is not: row 109 dispatches `fnPi`. Load-bearing.
- **The `OPTION_INFSUMS` guard** around `ITM_SIGMAnINF` states its falsifying
  case (an unimplemented stub moves no stack) and is correct for it.
- **`prettyNoteNumberCommit`'s mode test** has no `CM_NORMAL` conjunct;
  neither PEM nor MIM can reach it, because `closeNim`'s PEM branch returns
  before the `closeNim_exit` label the hook hangs off, and `calcModeNim` is
  only entered from CM_NORMAL. Fail-closed by construction.
- **The `PPC_ENTER` dup expression** is byte-for-byte upstream's own
  `fnKeyEnter` predicate — this project's stated convention — and its CM_NIM
  arm's missing `lastErrorCode == 0` conjunct is subsumed by DONE's error
  branch.
- **`prettyTryRegisterLine`'s eight-conjunct gate** against the arm it
  preempts: every `prefixWidth =` in `_refreshRegisterLine` sits inside a
  `temporaryInformation` branch below the hook, and the gate declines on
  `TI != TI_NO_INFO`, so the `SCREEN_WIDTH` vs `SCREEN_WIDTH - prefixWidth`
  argument difference is vacuous rather than a divergence.
- **The FRACT arm's missing angular-mode conjunct**: `fractionToDisplayString`
  never emits an angle suffix, so upstream's own arm shows the same
  suffix-free string. Builder-first holds.
- **`ppcRclLeaf`'s `param <= 99`**: the else branch hands a named-variable id
  to `getRegisterDataType`, which range-checks named/reserved/local ids, and
  TAM resolves indirection *before* `reallyRunFunction` sees the param — so
  `RCL IND nn` cannot mint a node against the wrong register.
- **`ppcDispatchDepth`'s 0xFFFF arm** does desynchronise (increment skipped,
  DONE still decrements) and is unreachable at 65,535 nested dispatches; the
  comment says so, and the arm it guards already invalidates.

### Unreached shapes, named so they are findings the day something changes

- **`ENTER` dispatched while `calcMode == CM_NIM`.** `prettyCapture.c:895` has
  a `stagedMode == CM_NIM` branch, and `PPC_ENTER`'s STAGE arm can materialise
  slot 0 from `REGISTER_X` — which during NIM holds the zeroed placeholder, one
  level below the real value, because the shadow lift is deferred. That would
  mint `VAL(0)` where an UNKNOWN would have re-materialised truthfully. No
  reaching input exists today: `ENTER` in CM_NIM is intercepted by
  `addItemToNimBuffer`, whose catch-all closes NIM *before* `ITM_ENTER`
  dispatches, and the harness deliberately does the same. Reported here, not as
  a finding, because the branch is currently unexercised belt-and-braces — and
  because the day something does dispatch in CM_NIM it is finding-grade.
- **A nested *in-scope* dispatch clobbering an outer transform's stage.**
  `prettyNoteFunction` sets `ppcStage.valid = false` before the `PPC_IGNORE`
  early return, so any in-scope nesting strands the outer STAGE's half-applied
  work and the outer DONE bails silently. In-scope nesting exists
  (`fnKeyDotD` → `runFunction(ITM_toREAL)`, `fnChangeBaseMNU` →
  `runFunction(ITM_toINT)`, the matrix editor, `conversionUnits.c`), but in
  every case the *outer* item is itself unmodelled and invalidates anyway, and
  the BIGOP label programs — the only deep nesting — run under
  `PGM_RUNNING`/`FLAG_SOLVING`/`FLAG_INTING` and are filtered by `ppcScopeOk`
  exactly as the comment claims. Two dimensions hunted this independently and
  neither could name an outer item with a modelled transform that nests an
  in-scope dispatch. It is the strongest remaining latent shape in the file and
  is worth an owner's ruling rather than another reading pass.
- **`ppcStage.stagedFrom`/`stagedTo` leaking or dangling.** Same clearance:
  the only live window is between a BIGOP STAGE and its DONE, no in-scope
  nesting can reach it, the DONE error path frees both, and the three decline
  breaks happen before the allocations.
- **`fnPrettyShow`'s vertical centring** is one row high for odd heights and at
  exactly `h = 147` would place the top ink row on the frame line. No value
  tree this converter can build comes within ~80 rows of the limit, so there is
  no reaching input; the sibling case at `ppqShowRender` is the KNOWN
  `PP18RR1-P1`.
- **PHIST raised from CM_AIM** (which would leave `FLAG_ALPHA` cleared) — the
  item is unreachable from `processAimInput`.

### Doc drift, found and not filed as defects

`DESIGN.md` §8 records `prettyValue` BSS as 200 B; the file holds
`ppScratch[200] + ppSpanA[120] + ppSpanB[120]` = 440 B, stale by 240 B since
PP2 added the complex spans — but the binding claim is the measured device
total, so this is drift on a derived figure. §6's inline band is stated as
`baseY+35` where the code uses `+31` for non-X lines (the code is the stricter
one and explains itself); §6 still shows the two-argument hook signature; §2
names `ppLeafScratch` where the code has a private `ppScratch`. All four are
worth a doc pass and none is a defect. The one documentary item that *is*
filed is in §4, because a rebaser acts on it.

### Ruled, known, or below the bar

- **P13** (`prettyTest.c:1369`) measures upstream's repaint rather than the
  package's painter, because `FLAG_FRACT` is clear when `prettyTestCapture`
  runs and both its values decline every parser. Not flagged:
  `TESTING.md:490-521` already rules it — *"P13 is therefore an UPSTREAM-DRIFT
  pin … our own code cannot break it, and that is the point."*
- **T4's** "deep copy" assertion would pass under aliasing (the signature
  printer walks a DAG), but `ppcFreeTree`'s `PPN_FREE` guard makes aliasing
  non-destructive here, so there is no consequence.
- **F1/F2/F3** are enabled-vs-disabled identity pins that a globally dead
  surface would satisfy — but `prettyTestPixels` asserts positively in the same
  process, and F1/F2 are legitimately negative pins over decline paths.
- **The unguarded `findNamedLabel` results** passed to `fnPrettyVisual`
  (V19/V20/V27/V67) look like B10's shape and are not: `INVALID_VARIABLE` =
  `FIRST_LABEL - 1`, so the call takes its `ERROR_OUT_OF_RANGE` arm and the
  pins fail loudly.
- **T24** (R/S) discriminates correctly: an unclassified item leaves
  `ppcStage.valid` false and the shadow survives, which is what the pin
  detects.
- **Duplicate pin names** (two live `P5` pins in `prettyTestPixels`) — log
  ambiguity only, and the same class as the KNOWN `PP18R4-10`.
- **`ppvDerivVariable`'s hand mirror** of upstream's file-static — ruled by
  restarted round 1 (§6) and by `DESIGN.md:628-631`; its two real drift defects
  are the KNOWN `PP18R4-4`/`-5`.
- **The `docs/appnotes/sources/AN0022/func.p47` path** and the hardcoded
  `"c47programTest.bin"`: both argued in-code, both fail loudly on an upstream
  rename, and the second follows upstream's own testSuite convention.
- **`fnPrettyShow`'s error-then-`checkHP` ordering** (`R3-6`) and
  `TI_SHOWNOTHING` being declared *after* the paint: both are load-bearing as
  written; setting TI before the paint would suppress `numDouble` and mask the
  very garbling `R1-13` found.
- **`checkHP` existing only at the two `prettyValue` surfaces** is currently
  airtight — `numDouble` requires `font == &numericFont`, and `PP_FONT_NUMERIC`
  is requested only inside the two guarded functions. Recorded because it is
  the same shape as `PP18RR2-D5`: an engine precondition enforced by each
  surface by hand, already paid for twice, with nothing that would go red if a
  future surface picks a numeric rung. No input reaches it today.
- **`ppfStageValFields` clearing `lastErrorCode` unconditionally**: every
  reaching caller is gated on `lastErrorCode == 0` upstream of it, so the only
  error it can clear is one `reallocateRegister` just raised.
- **The DONE error path discarding the current formula** (`ppcInvalidate(false)`
  rather than emitting) loses `2+3` on `2 ENTER 3 + 0 ÷`. Arguable under §4,
  but the code states its reason (*"a failed function may have partially moved
  the stack"*) and emitting with a register it cannot trust is precisely the
  `R1-5` defect. Conservative-by-design until someone establishes whether
  upstream restores the stack on every error path — which nobody did this
  round.
- **`ppcInvalidate(false)` from the package browser's recall** dropping the
  open formula unfiled: the same shape, at a site whose own comment scopes it,
  and §3 accepts over-invalidation. (`PP18RR2-9` is flagged and this is not,
  because there the design's own fix comment says the opposite.)
- **`ppcDispatchDepth` not being reset by `prettyReset()`** is right, not
  wrong: `prettyReset` runs *inside* the `ITM_RESET` dispatch, and zeroing the
  depth there would desynchronise the paired DONE.
- **`prettyNoteNimText` on the CM_PEM path** only sets a snapshot that every
  route to `closeNim_exit` overwrites first.
- **The exception-not-taken cases in `PPC_STO_NOP`**: `STO X` correctly
  excluded (`R3-2` uses `>` not `>=`), `STO L` correctly wipes `ppcSlotL`
  without emitting (it only ever holds a VAL leaf, which `ppcEmit` refuses),
  numbered targets need no action. Only the result-register timing is wrong,
  and that is `PP18RR2-4`.
- **`ppqBuildBigop`'s unchecked `varTiny`/`varCtx`**: the callee checks both,
  per kind.

### Cross-package composition

- **The raw item numbers `case 427: case 428:`** are two places that must
  agree, and a renumbering in undo-history would silently stop the shadow
  invalidating on a stack-rewriting item. Not flagged: `DESIGN.md` §7's
  composition-claims table is a written forcing mechanism on both sides, in the
  solo build both rows are inert `CAT_FREE` stubs so the over-invalidation is
  harmless, and no reaching input exists that does not begin with someone
  editing the sibling package. Worth one grep in any future undo-history
  renumbering review; there is no pin.
- **`pretty_visual_real` clearing program memory inside a shared upstream test
  list** is contract-documented and ordered correctly against every driver that
  runs after it, all of which load their own programs.
- **The testSuite.c hunk adjacency** still holds for the table-row half (the
  block grew from three rows to nine but stayed contiguous above
  `fnGetREALDF`); only the list-line half is falsified, which is §4's
  unverified item.

---

## 7. Verdict

**Would I ship this? No** — and for a different reason than restarted round 1
gave. That round's blocker was ink painted outside a measured box. This
round's is that the surface whose binding invariant is *"the display never
lies"* has four ways to keep describing registers it no longer mirrors, and
one way to file a `= result` that was never the formula's value. None of them
needs an unusual machine, a program, or a mode nobody uses.

**Where would it break first?** In an owner's hands, in this order:

1. **On any number with an exponent, today, for everybody** — not as a
   misdraw but as an absence. `PP18RR2-1` means PP2's raised exponent has never
   rendered on this device. It is the cheapest thing on the list to confirm (put
   `1.5e30` in X with the flag on) and it invalidates a claim `DESIGN.md`,
   `TESTING.md` and the release history all make.
2. **`REGS`, arrow to a register, `RCL`, then any operator** — the T line and
   the filed entry describe a formula the machine did not compute
   (`PP18RR2-2`). Three unhooked call sites, one of them in SHOW.
3. **`FILL` after any completed calculation** — the formula is deleted rather
   than filed, on both surfaces, with no message (`PP18RR2-3`).
4. **`STO` to a stack register while a formula is open** — a permanently wrong
   `= result` in the ring that the browser's ENTER will recall as a number
   (`PP18RR2-4`).
5. **Single-stepping a program with a formula open** (`PP18RR2-5`), and
   **`SSIZE4` for anyone who prefers four levels** (`PP18RR2-6`).

**What I would leave alone if the goal were correct code rather than a clean
audit.** `PP18RR2-13` (the `ppSpanA` self-clobber) is unreachable today and
would stay unreachable forever if `PP18RR2-1` were never fixed — but the two
are coupled, so it costs nothing to fix them together and is wrong to fix
either alone. `PP18RR2-11` (the arena leak) needs the same rare failure many
times in one session to reach its stated endpoint; the one-line shape is
already in the sibling arm, so it is worth taking when someone is in the file,
not worth a wave. `PP18RR2-12` (the postfix-8 ceiling) needs a nine-deep right
spine, which an owner builds only by chaining `x<>y`; the honest cost is a
wrong explanation in the browser, not a lost formula. `PP18RR2-9` (the BIGOP
decline arms) is narrow — a global label named `X` and the interactive sum
form — and its consequence is an entry that cannot be recalled rather than one
that lies. `PP18RR2-10` (complex) wants a **ruling** more than a fix: either
implement `PPN_VAL2` or amend §3's sentence; what is not defensible is the
current state, where the design promises a representation the code does not
have and the failure is expressed as silence. And `PP18RR2-16`/`-17` are
insurance: real vacuities, but their subjects are correct today.

**What should not wait.** `PP18RR2-1` (a shipped feature that has never run,
and a two-byte fix window that must not be taken without `-13`);
`PP18RR2-2`/`-5` (one class, two sites, and `ppcShadowInvalidate` already
exists for it); `PP18RR2-3` and `-4` (the same wipe/displacement table, both at
the sites the previous fix wave named and did not visit); `PP18RR2-6` (a
ruling first — read the flag, model it, or decline — then the mode-looped
oracle); `PP18RR2-14` (the whole PP8 surface can be deleted and the gate stays
green, which is worth fixing before anyone edits that file again).

**What is genuinely solid, verified rather than assumed.** The history ring's
arithmetic is closed end to end — the eviction loop terminates, every token's
space test is right, the literal-length chain is exact at both ends, and the
value-leaf recipe is byte-exact against upstream's own block sizing. The
parsers fail closed: their alphabets are tight, their bounds are correct
including every two-byte glyph read, and no rung can paint and then fail. The
arena's aliasing discipline holds at all five duplication sites. `ppcScopeOk`
really does keep nested program execution out of the mirror, which four
dimensions tried to break. The `R1-14`, `R3-2`, `R3-6` and `R1-13` fixes are
all correct as written and correctly explained. And where the suite is
differential rather than existential — F3 — it caught a one-token upstream
mutation with exactly one useful failure.

**The pattern to carry.** Three of the top seven findings are last wave's
fixes applied to a subset of their sites (`R1-5` → `PP18RR2-4`, `R3-3` →
`-3`, `R2-1` → `-9`, `R1-7` → `-5`). That is now seven audit rounds of the
same statistic, and the remedy this report can defend is not another
site-by-site sweep: it is the two enumerations named in `PP18RR2-D1` and
`PP18RR2-D2` — a quiescence assertion, and a wipe/displacement table over
every arm — each of which turns a class of finding into a pin.

---

## 8. Round and exit state

**Round: PP18 round 2 of the restarted series**, same subject
(`pretty-print/stage-pp17..34ac6e97f`), **no fix wave in between** — the tree
is byte-identical to the one restarted round 1 read. This round is therefore
not a fix-wave audit; it is the same code under a rotated question, per that
report's own §8 priority list.

**Readers.** Eight in-family finder dimensions (contracts, lifecycle,
arithmetic, error paths, guards, tests, design, upstream), blind to each other,
scoped to `prettyCapture.c` and `prettyValue.c` with their seams; every
finding then refuted independently under one assigned lens (reachability,
correctness, intent), default REFUTED, coverage claims proven by mutation.

**Out-of-family accounting: `pending`.** No packet was built, no reply exists,
no `MODEL:` line can be quoted. The §1 banner states it; this section repeats
it because the exit criterion turns on it:

| reader | packet | reply | `MODEL:` line | findings |
|---|---|---|---|---|
| — | none | none | — | **pass not run (`pending`)** |

**Counts.** Twenty-three findings raised; **one refuted** (the builder-first
parity claim, killed by an executed F3 mutation); one filed **unverified** (the
`DESIGN.md` adjacency row, no refutation pass); twenty-two survived.
Deduplicated across dimensions: **seventeen CONFIRMED**, `PP18RR2-1`–`-17`,
**no new PLAUSIBLE**.

**Independent agreement — the strongest of any PP18 round.** `PPC_FILL` was
found by **four** dimensions (contracts, lifecycle, arithmetic, design) through
four different reaching inputs and three independent probes. The `SSIZE4`
window was found by **three** (lifecycle, arithmetic, guards) at three
different guard sites. The `PPC_RCLARITH` leak was found by **two**. That is
nine of the twenty-three raised findings converging on three defects without
any reader seeing another's notes, which is the evidence the fan-out is real.

**Evidence discipline.** **Fourteen of the seventeen** are backed by a probe or
mutation applied, built through the package gate, observed in
`build.sim/meson-logs/testlog.txt`, and reverted — including every one of the
top eight except `PP18RR2-2`. The three static traces are `PP18RR2-2` (recall
bypass), `-9` (BIGOP decline arms) and `-13` (`ppSpanA`), each constructed
hop-by-hop against upstream with every line number verified. No simulator ran;
no finding rests on an LCD photograph. Main tree clean at start and finish
(`git status --porcelain packages/` empty, no `AUDIT-PROBE` marker anywhere in
`packages/`).

**Exit criterion: NOT MET, and this round cannot advance it.** Seventeen new
CONFIRMED findings would reset the count on their own; separately, the round
had no out-of-family reader, and the criterion requires two consecutive clean
rounds with at least one of them out-of-family. The clock stands where
restarted round 1 left it.

**Process items.**

1. **Stale worktrees, now requested by five consecutive rounds.** Every
   verifier worktree spawned at `e21af8d28` — 111 commits behind, not an
   ancestor of the tip, a tree in which `prettyValue.c` does not exist. All of
   them detected it and checked out `34ac6e97f` before reading, which is the
   only reason the round is usable. The `git merge-base --is-ancestor` guard in
   `audit-workflow.js` is **still absent**.
2. **Shared `/tmp` paths across verifier worktrees, and one inherited
   `build.sim`.** Two verifiers wrote gate output to `/tmp/probe_run.log`
   concurrently and clobbered each other (one log opened with a *different*
   worktree's banner and interleaved 742 foreign lines). One verifier found its
   `build.sim` had been copied from a sibling worktree — ninja object paths and
   `meson test`'s "Entering directory" both named `wf_…-25` inside `wf_…-22` —
   and had to prove its own run by re-`setup --reconfigure` and grepping the
   shadow tree for its probe. A third observed live `AUDIT-PROBE R2` markers in
   a sibling's `prettyTest.c` and correctly did not touch them. **Fan-out needs
   per-agent scratch paths and per-worktree build directories**; this is the
   same serialization defect restarted round 1 recorded as its item 2, one turn
   worse.
3. **Two probes were inert and were nearly scored as clean.** One fed
   `ppParseFraction` an ASCII `"3/4"` (that parser requires superscript glyphs)
   and read the decline as agreement; another used
   `findNamedLabel("Z", GLOBAL_LABELS)` to simulate a missing label and got a
   **valid** register back, because label state at that point in the suite is
   suite-global rather than hermetic. Both were caught by the verifier's own
   liveness check — assert the probe fails for the reason you think it does,
   *before* trusting a green. That check should be part of the mutation
   protocol, not an individual habit.
4. **The out-of-family pass was skipped silently again**, which is the exact
   defect the tip commit's own handoff describes
   (`HANDOFF_SKILL-DEFECT_out-of-family_2026-08-29.md`). The workflow still
   does not throw on a missing out-of-family argument and still does not
   enforce the §1 accounting. This round complied by writing the banner by
   hand — which is what the handoff says is not enough.
5. **The governing gate matters.** `./packages/forth-core/build-test.sh`
   returns a meaningless green for pretty-print mutations; the package's own
   `build-test.sh --solo` is the only trustworthy runner, and its refresh step
   is what carries a working-area edit into the built artifact.
6. **Report filename truncated.** The requested filename is 613 bytes and the
   filesystem limit is 255; this file's name is the requested one truncated
   after the axis list, with the date and `-r2` suffix preserved.

**Round 3's axis, in priority order.** (1) **An out-of-family reader over
`prettyCapture.c`'s staging machine** — restarted round 1 already argued for
this packet, and this round is the argument's evidence: every finding above
came from one family. (2) **The fix wave for this report, when it lands** —
seven-for-seven on the fix-regression pattern, and the two shapes to hunt are
this report's own: a class fixed at the sites it was found and not at its
siblings (`PP18RR2-3`/`-4`/`-9`), and state whose identity is an instance
(`-D2`). (3) **The acceptance-parity oracle** across `ppqParse`/evaluator/
walker, which this round did not reach and which `PP18RR1-D3` still wants.
(4) **`prettyFormula.c` and `prettyLayout.c` internals** — the remaining files
without a full pass, and the pair `PP18RR2-12` sits between.
