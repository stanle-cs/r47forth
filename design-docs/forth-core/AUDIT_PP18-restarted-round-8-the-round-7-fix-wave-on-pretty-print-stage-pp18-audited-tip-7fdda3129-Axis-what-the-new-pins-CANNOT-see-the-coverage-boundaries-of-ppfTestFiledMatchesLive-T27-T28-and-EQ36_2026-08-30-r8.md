# Audit — PP18 restarted round 8: the round-7 fix wave, at `7fdda3129`

Subject: `f4421da32..7fdda3129` on `pretty-print/stage-pp18`, three commits.
Axis, as round 7 asked for it: **what the new pins cannot see** — the coverage
boundaries of `ppfTestFiledMatchesLive`, `T27`, `T28` and `EQ36`, and any input
class where the filed and live surfaces can still diverge.

The four repairs are correct at the sites where they were typed. I re-derived
all four rather than accept the red-first record, and the record holds. The
round's answer to its own axis is short and it is not the one the wave expected:
the new equality oracle is structurally blind to every defect the two surfaces
**share**, and three of this round's confirmed findings are exactly that shape —
a base-tagged integer bracketed on both surfaces, a polar complex drawn
rectangular on both, and a glyph the tiny rung silently paints as a blank on
both. The pins compare filed against live; nothing in the suite compares either
of them against what the owner sees on the stack line.

Half of what follows is not this wave's code. Five of the ten confirmed
findings are wave-0 defects the rotated axis reached for the first time —
including an out-of-bounds read in the shipped solo artifact that a design
amendment records as closed.

Two out-of-family readers ran, one on the pin net and one on the four repairs.
Neither produced a new confirmed finding. Both converged, independently and from
different packets, on the same structural point the in-family pass reached: the
equality oracle records neither fonts nor spaces and compares a mode no
production caller uses, so it cannot express the property its name claims. Their
ten items went past the refutation cap and are carried as unverified in §4.

> **Filename note.** The dispatched subject string for this round is about 1000
> bytes and cannot be a filename (255-byte limit). This file keeps the same
> naming convention truncated after the axis clause; the full subject, the
> pre-verified facts and the fenced ids are stated below in §1 and §6.

---

## 1. Subject and coverage

### The three commits

| commit | role in the wave |
|---|---|
| `fd080dee9` | comment-only STE pass over the package, verified code-identical after comment strip; fifteen rewrites that changed meaning corrected |
| `4361b80e9` | the repair wave: `PP18RR7-1`, `-2`, `-3`, `-9`, with the pins landed red-first — 8 red before, 0 after |
| `7fdda3129` | docs: the round-7 report, the amended `DESIGN.md` fallback rule, the `CLAUDE.md` gate order, four new bug classes |

Diffstat over the range: 47 files, 4334 insertions, 4255 deletions. That number
is almost entirely the comment pass and its generated `files/` mirror. Behaviour
changes live in exactly three package sources — `prettyFormula.c`
(`ppfValBuf` 96→200, the two `ppfBuildEntry` leaf arms), `prettyEquation.c`
(`ppqNumber`'s `c->failed` latch), `prettyTest.c` (`ppcTestReset`'s
`lastIntegerBase`, `ppfTestFiledMatchesLive`, T27, T28, EQ36) — plus one header
line in `prettyInternal.h`. **No override file changed behaviour**; the upstream
surface is untouched by the fix commit.

Independently verified for this report: `fd080dee9` is comment-only.
Preprocessed (`gcc -fpreprocessed -dD -E -P`) and whitespace-stripped hashes of
`f4421da32` against `fd080dee9` match for all 19 package C and H files,
`prettyTest.c` included.

### Out-of-family accounting

Both reply files were read. Both are present and complete; neither is empty, so
no timeout or overwrite banner is owed.

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r8/packet-sol-pinnet.md` | `/tmp/pp18-r8/packet-sol-pinnet.sol.reply.md` | `MODEL: GPT-5` | **7** |
| gemini / gemini | `/tmp/pp18-r8/packet-gemini-fixwave.md` | `/tmp/pp18-r8/packet-gemini-fixwave.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | **3** |

Identity, stated plainly because the skill requires the check every pass. Sol's
reply self-reports the family name `GPT-5`; the dispatch transcript beside it
(`packet-sol-pinnet.sol.reply.md.err`, 69,699 B, the `codex` CLI log) records
`model: gpt-5.6-sol`, `provider: openai`, `approval: never`,
`sandbox: read-only`. The self-report is coarser than the dispatched id, not a
substitution — the transcript carries the dispatch. Gemini's `.err` is 0 bytes
with a complete 65-line reply, which is the normal shape for that driver.

Counting note. Sol's reply raises seven numbered findings, then adds five
pin-vacuity verdicts (the helper, the T25 call sites, T27, T28, EQ36) and one
helper-state-contamination verdict. Those six are analyses of the pins the seven
findings already name, not separate claims, so the count is 7. Gemini's reply
has four numbered items; item 4 is a clearance — it multiplies out both length
comparisons on the staging path and concludes the buffer repair is sound — so
the count is 3.

What each was given, and what that shaped. Both packets are self-contained
(37,244 B and 38,492 B), built from a shared `core-context.md` plus `new-code.md`
and a per-reader header and task. Sol was sent at the pin net: the input classes
where the two surfaces can still diverge with every pin green, whether each pin
can pass while its named property is false, and whether the helper's `ppReset()`
pair and `ITM_CLSTK` can change what a later row tests. Gemini was sent at the
four repairs, one question per repair, with an explicit instruction to take the
precedence census from the call graph and not from the diff. Neither reader was
given `ppfBigop`'s body; that omission produced Sol's top-ranked finding and is
recorded as a process observation in §5, D8.

### Coverage, union across the eight in-family dimensions

Read in full by at least one reader: the whole `f4421da32..7fdda3129` diff and
all three commit messages; `prettyFormula.c` (844), `prettyEquation.c` (995),
`prettyInternal.h` (187), `browsers/prettyBrowser.c` (232);
`design-docs/pretty-print/DESIGN.md` (836 lines, including the amendment in this
range), the DESIGN-HISTORY entries this wave added, `TESTING.md`, the round-7
report in full through §8, the 2026-08-27 upstream-minimality review, and
`bug-classes.md`.

Read in the relevant span: `prettyCapture.c` (arena, `ppcClassify`,
`ppcValLeafFromRegister`, `ppcRclLeaf`, `ppcEnsureKnown`, `prettyNoteNimText`,
`prettyNoteNumberCommit`, `ppcSerializeNode`, `ppcEmit`, the history ring and
its eviction, the STAGE/DONE arms); `prettyLayout.c` (pools, `ppNewRun`,
`ppNewBox`, `ppGlyphOf`, `ppRunInk`, `ppMeasure`, `ppSetFontDeep`);
`prettyValue.c` (the rung tables, `prettyTryRegisterLine`, the complex arm);
`prettyVisual.c` (`ppvLiteral`, `ppvAstToNodes`, the paint arms);
`prettyTest.c` (T22–T29, the T25 power block, `ppcTestReset`, `ppfTestSigNode`,
`ppfTestExpect`, `ppfTestFiledMatchesLive`, EQ4, EQ36); the package's
`keyboard.c` containment arms and `screen.c`'s `calcMode` dispatch; all 13
generated patches, hunk by hunk.

Upstream read for reachability, not review: `display.c`
(`shortIntegerToDisplayString`, `real34ToDisplayString`,
`complex34ToDisplayString`, `addBaseNumber`, `insertSepsIntoIntegerText`);
`fonts.c` `findGlyph`/`findGlyphExact` and `fonts.h`'s glyph codes; `screen.c`
`showGlyphCode` and `_refreshRegisterLine`; `registers.c` `reallocateRegister`
and the tag macros; `flagBrowser.c:250-300`; `generateCatalogs.c`; `items.c`
rows 65, 1687, 1857 and the `CAT_SYFL` block; `integers.c` `fnChangeBase`;
`bufferize.c` `closeNim` and its 20 call sites; `config.c`'s reset table;
`error.c` `displayCalcErrorMessage`; `testSuite.c`'s `In:` handling.

Measured, not read: the three glyph tables parsed out of
`build.sim/src/ttf2RasterFonts/rasterFontsData.c` (`standardFont` 711 codes,
`tinyFont` 663, `numericFont` 441; 82 codes are `standardFont`-only), and a
clean solo `meson`/`ninja` build configured with
`-DCUSTOM_PKG=packages/pretty-print` for the `menu_SYSFL` count.

### Not reached, and it matters where

- **`prettyLayout.c`'s measure and paint arithmetic.** `ppGlyphOf`, `ppRunInk`
  and `ppSetFontDeep` were read; nobody re-derived `ppMeasure`'s box arithmetic
  or the `PP_SUP` placement rule.
- **`prettyVisual.c`'s walker body** (about 1486 lines). The comment pass is
  hash-verified identical, so it was read only at its leaf arm and its
  `lastErrorCode` gate. The V-series pins were not swept.
- **`solver/equation.c`'s 578-line inline block.** Read as a patch and as a
  merge surface, not as behaviour. Its one open item is unverified in §4.
- **The generated `files/` mirrors, `patches/` bodies beyond their hunk shapes,
  and `.refresh-manifest.json`.** Declared generated output.
- **`keyboard.c`'s six `CM_PRETTY_BROWSER` arms** beyond the containment
  guards, and `screen.c` beyond the dispatch site.
- **The out-of-family findings.** Ten items, none put through the refutation
  pass — the cap was spent on the in-family set. §4 lists them with what the
  in-family read already establishes about each.
- **The device.** Nothing ran on hardware and no LCD was photographed. Every
  picture claim below is `ppMeasure`/`ppShowRun` arithmetic plus a glyph table,
  or a signature printed by an executed probe in the simulator battery.

---

## 2. Mechanical results

**Gate: GREEN at `7fdda3129`** (pre-verified this session in the refresh-first
order, and re-established at baseline by every verifier that mutated). The
governing gate for this package is `./packages/pretty-print/build-test.sh`
(equivalently `tools/pkg_patch_refresh.py` then
`make pkg_build PKG=packages/pretty-print`), which `7fdda3129` itself added to
`CLAUDE.md`.

**Churn scan: clean.** Re-run here:

```
python3 .claude/skills/upstream-diff-review/references/patch_churn_scan.py \
        packages/pretty-print/patches/*.patch     → exit 0, no findings

patch                                             adds  dels hunks
010-solver__equation.c.patch                       578     0     5
010-keyboard.c.patch                                54     3    13
010-screen.c.patch                                  38     4     5
010-items.c.patch                                   19    10     5
010-softmenus.c.patch                               19     2     4
010-testSuite__testSuite.c.patch                    11     0     1
010-items.h.patch                                    8     6     2
010-testSuite__tests__testSuiteList.txt.patch        8     0     2
010-config.c / defines.h / bufferize.c / c47.h / calcMode.c   14   1   7
                                          total   749    26    44
```

Zero mechanical findings, against one at the 2026-08-27 review (the
`showEquation` wrap-reindent, since fixed). The comment pass rewrote only
comments that sit on the package's own `+` lines, so no hunk anchor moved and
no context line changed.

`design-docs/forth-core/design-audit.sh` **still does not cover this package**:
line 29 hard-wires `PKG="packages/forth-core"`. Unchanged from round 7, so no
drift result exists for pretty-print this round.

### Mutations run by the refutation pass

Every probe went into the flat working area, was propagated by
`pkg_patch_refresh.py` into `files/`, verified present in the built shadow, run,
then reverted by inverse edit with a second refresh. Every verifier ended with
an empty `git status --porcelain` at `7fdda3129`.

| probe | result | measured line |
|---|---|---|
| wide `eˣ` row through the tiny rung, then check every run's glyphs | row ACCEPTED, five unaudited glyphs | `node 2 paints code 0xa147 as a blank` · `accepted row, width=343, unaudited tiny glyphs=5` |
| `10 →INT 16 ENTER 2 ×`, sign the live tree | bracket present, base 10 too | `livesig='[P(A₁₆) · 2]' atom=0` · `livesig='[P(10₁₀) · 2]' atom=0` |
| sign the live tree inside T28's own five-base loop | gate GREEN with the bracket present | `base=10 sig='[96<A0>474…<A4>i - P(1<A4>i)]'` |
| right-deep `+` chain at 5, 6 and 7 repetitions | filed decode fails at 7, live row still builds | `[5] filed=1 filedRow=1` · `[7] liveRow=1 … filed=0 filedRow=0` |
| one filed row wider than the band, pan then Up/Down | pan does not reset | `width=414 visible=388` · `recovered=0 panIsOneWay=1`, painted pixels bit-identical to the panned frame |
| `ppfValBuf` 200 → 96 (restore `PP18RR7-2`) | RED, sole failure, base 2 only | `T28 the widest word does not decode in this base (expected 2, actual 0)` |
| same defect **plus** `WS=64`→`WS=16` in the driver preamble | **GREEN, 0 failed** | the shipped defect is in the binary and no pin prints anything |
| remove `ppqNumber`'s `c->failed` latch (restore `PP18RR7-3`) | RED, sole failure | `EQ36 a numeral vanished into the text pool and the parse still succeeded` |
| same defect **plus** EQ36's fixture 492 → 491 digits | **GREEN** | the only pin for the fix goes silent on one byte |
| `LEAD.0` set, base 2, WSIZE 64, three value leaves, decode | entry built, result tail gone | `built=1 hasEq=0 siglen=490` · `streamHasTKRES=1` |
| `ppfTestSigNode` instrumented: guard hits and high-water | guard NEVER fires suite-wide | max 43 of cap 192; T28-shaped row 191 of 192, canary intact |
| revert `PP18RR7-1` (both leaf arms back to `ATOM`) | RED, five failures | `T27 filed MULT keeps the signed bracket (live '[3 · P(-5)]', filed '[3 · -5]')` + T27 SUB + three T25 rows |
| `resultRun = ppfRun("Z", …)` | RED, four failures | `FV3 … (expected '[[2 + 3] = 5]', actual '[[2 + 3] = Z]')` + T22, T23b, T27 |
| delete the `withResult` HBOX wrap | RED, six failures | FV3, T23b, T25, T27, T28, T29 |
| delete `ppSetFontDeep(root, PP_FONT_TINY)` (`prettyFormula.c:745`) | **GREEN** | the rung-1 re-font has no pin |
| invert both `T28` guards | RED, six failures, both fixtures self-identifying | `T28 the widest word does not decode…` ×5 + `T28 the panning caller lost the wide row again` |
| solo `meson`/`ninja` build, parse the generated catalog | count and rows disagree | `menu_SYSFL` 114 entries, `nm` size `0x0e4`; `NUMBER_OF_SYSTEM_FLAGS` = 115 |

Two harness facts came out of those runs and belong here, not in a finding.

- **The false green is still being paid for.** `7fdda3129` amended `CLAUDE.md`
  to say that `./packages/forth-core/build-test.sh` "stays green under a broken
  pretty-print" and to name the correct gate. **Three verifiers ran the wrong
  gate anyway this round**, one of them reporting `BUILD + SELF-TEST GREEN` with
  the result tail fully deleted from `ppfBuildEntry`. Writing the trap down did
  not remove it; the two scripts still have the same name in different
  directories. A verifier who had stopped at the first green would have returned
  a false `REFUTED`.
- **Every verifier worktree spawned at `e21af8d28`**, 143 commits off the
  audited tip, with `7fdda3129` not an ancestor. Seventh consecutive round.
  Every reader had to `git checkout 7fdda3129` before reading anything. No
  foreign mutation was found in any worktree this round.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. Ten findings. Two dimensions filed
the same base-subscript defect independently from different call sites; they are
merged as `PP18RR8-6` and both probes are cited there.

---

### PP18RR8-1 — `ppGlyphOf`'s miss test is dead for `tinyFont`, so an unaudited glyph paints as a blank and the row still reports success

**Where.** `packages/pretty-print/prettyLayout.c:47-49`:

```c
int16_t gi = findGlyph(font, charCode);
return gi < 0 ? NULL : &font->glyphs[gi];
```

`findGlyph` (`src/c47/fonts.c:67-80`) returns `-1` on a miss **only** when
`font->id == 1` and `-2` **only** when `font->id == 0`; otherwise it falls
through to `return 0`. `tinyFont.id` is 2. So for the tiny rung a miss is
reported as valid glyph index 0 — `charCode 0x0020`, `colsGlyph 0`,
`colsAfterGlyph 6`, `rowsGlyph 0`. The guard in `ppRunInk` never fires, the run
measures, and `ppShowRun` paints through `showGlyphCode`, which takes the same
fallback (`screen.c:1203-1206`).

**Reaching input, measured.** Natural display on (default). Keys:
`2` `eˣ` `1234567890123456` `×` `2345678901234567` `+` `CLX` `PHIST`. `CLX`
displaces the formula, so it files; `PHIST` opens the CM-20 browser. The row is
far past `SCREEN_WIDTH - 8`, so `ppfBuildRow`'s rung 0 falls through on width
(`prettyFormula.c:760`) and rung 1 does
`ppSetFontDeep(root, PP_FONT_TINY)` (`:745`). `ITM_EXP` is classified `PPC_MO`,
so `ppfBuildOp1`'s default arm builds the operator's name run from
`indexOfItems[ITM_EXP].itemCatalogName` = `STD_EulerE STD_SUP_BOLD_x` =
`0xa147 0x82e3` (items.c item 65; fonts.h:391, :431). Neither code is in
`tinyFont`. The probe, in the built battery:

```
AUDIT-PROBE R8: built=1 exactEulerE=-1 exactSUPx=-1 loose=0
AUDIT-PROBE R8: node 2 paints code 0xa147 as a blank
AUDIT-PROBE R8: node 2 paints code 0x82e3 as a blank
AUDIT-PROBE R8: accepted row, width=343, unaudited tiny glyphs=5
```

`loose=0` is `findGlyph` aliasing to index 0 where `findGlyphExact` returns
`-1`. `built=1` is the row being accepted rather than declined. The same rung is
reached with no filing at all through the `FLAG_PTLINE` T-line
(`prettyValue.c:818-819`).

**What breaks.** The browser row paints
`  (2) · 1234567890123456 + 2345678901234567 = …` — the `eˣ` operator has become
two blank spaces — and `ppfBuildRow` returns true, so nothing declines and
upstream's linear line never runs. 82 codes are in `standardFont` and not in
`tinyFont`. Two of them are formula text a keypress produces: `eˣ`'s catalog
name, and `0x83d5` (`STD_phi_m`), the whole catalog name of the `r.golden`
constant, reachable through `PPC_CONSTCLS`. The group-separator codes `0xa008`
and `0xa00a` alias too; that half is space-to-space and cosmetically invisible.

**Contracts violated.**

- `prettyLayout.c:172-174`, the function's own charter: *"Space-class glyphs (no
  ink rows) contribute nothing. An unknown glyph fails the run: its metrics were
  never audited."* For `PP_FONT_TINY` no glyph is ever unknown.
- `DESIGN.md:129-131`, **BINDING**: *"Any failure — unsupported type, unexpected
  glyph, pool/text/depth overflow, doesn't fit any rung — paints nothing and
  returns false, and upstream's own arm renders unchanged."*
- The package already knows this exact hazard and guarded it once:
  `screen.c:1196` comments that `findGlyphExact` is used for the bold font
  because *"a miss returns -1 so it can never alias glyph index 0."* The guard
  was not carried to `ppGlyphOf`.

**Bug class.** A sentinel whose meaning depends on the object it is asked about.
`gi < 0` is a correct miss test for two of the three fonts and a silent success
for the third; the caller cannot tell which contract it got.

**Class-level test that would pin it.** Enumerable and cheap: for each of the
three fonts, assert `ppGlyphOf(font, code) == NULL` for a code the font lacks —
`tinyFont` with `0xa147` is the row that fails today. The stronger property, and
the one that stops the next instance, is a sweep: every `itemCatalogName`
reachable as leaf or operator text must resolve with `findGlyphExact` in all
three fonts, or the run must fail `ppMeasure` at `PP_FONT_TINY`.

---

### PP18RR8-2 — the solo build declares 115 system flags and supplies 114 `menu_SYSFL` rows, so the A8 fix left the browser one entry past the end rather than zero

**Where.** `packages/pretty-print/defines.h:1021`
(`#define NUMBER_OF_SYSTEM_FLAGS 64+51`) against the generated
`menu_SYSFL`. `src/c47/browsers/flagBrowser.c` is **not** overridden by this
package, so the upstream loop runs: `fOffset = 60`, and the loop breaks only
when `f + fOffset > NUMBER_OF_SYSTEM_FLAGS - 1` = 114, so it evaluates
`menu_SYSFL[114]`.

**Reaching input.** A solo pretty-print build — the gate build, and the shape
shipped as `pkg_dist/pretty-print.zip`. Run `STATUS` (item 1610) or `FLGS`
(1935), then page with UP/DOWN to `SYSTEM_FLAGS_SCREEN_2`. Two keys after the
browser opens.

**Measured, on a clean solo `meson`/`ninja` build at this tip.** The generated
`softmenuCatalogs.h` has exactly 114 entries in `menu_SYSFL`;
`nm --print-size` gives `menu_SYSFL` size `0x0e4` = 228 bytes = 114 × `int16_t`.
Row source: `grep -c CAT_SYFL` over each package's `items.c` gives upstream 112,
forth-core 112, undo-history 113 (adds `UHIST`), pretty-print 114 (adds `PPRTY`
and `PTLINE`). **Root cause, which the A8 note does not state:** `0x8070` is
flag id 112, undo-history's `FLAG_UHIST`. The solo pretty-print build declares a
flag-id space of 0..114 but leaves a hole at 112 with no catalog row. That hole
is the missing entry.

**What breaks.** On the second system-flags screen the browser draws a 55th tile
from one `int16_t` past the end. In this link layout the neighbouring bytes are
alignment padding, so `systemFlag = 0` → `indexOfItems[0]` = `ITM_NULL`, an
empty label and `param = NOPARAM = 9876`. `getSystemFlag(9876)` then takes the
`>= 64` branch and shifts by 9812 — a shift count past the width of `uint64_t`,
undefined behaviour; on x86-64 the count masks to an unrelated real flag. When
that bit is set, `flagBrowser.c:284` paints a filled highlight rectangle in the
phantom slot. The read is out of bounds in every layout; the benign symptom
depends on link order and is not guaranteed.

**Contract violated.** `DESIGN.md:782`, the A8 amendment: *"The move was forced:
with the count here and the rows there, a SOLO pretty-print build declared 115
flags and supplied 112 rows, and the (un-overridden) flag browser indexed three
entries past the end of `menu_SYSFL` into the alpha catalog. … Ours is now exact
(115/115) and combined over-SUPPLIES (116 ≥ 115, safe)."* Measured: solo is
115 declared / 114 supplied, and combined is 115 / 115 exactly. Both arithmetic
claims are wrong by one, in the direction that matters — the defect the
amendment records as closed is still open at one entry instead of three. The
`KNOWN, NOT OURS TO FIX ALONE` carve-out in the same paragraph does not cover
it: that carve-out explicitly asserts the pretty-print solo build is the
balanced one.

**Why no pass has seen it.** `build.asan` is the **combined** tree, which is
exact at 115/115. The ASAN run that found the upstream `addons.c` read could not
have found this one.

**Bug class.** A shared constant owned by no package, with its correctness
verified in one build configuration and claimed for another.

**Class-level test that would pin it.** A compile-time assertion in the package,
or one battery row: assert the generated `menu_SYSFL` length equals
`NUMBER_OF_SYSTEM_FLAGS`. It is derivable at build time in every configuration,
which is the point — the count and the rows are generated from the same
`items.c` the assertion can count.

---

### PP18RR8-3 — `ppfBuildEntry` drops the `" = result"` tail and returns true, and this wave's `ppfValBuf` 96→200 is what put the failure in reach

**Where.** `packages/pretty-print/prettyFormula.c:598`,
`resultRun = ppfRun(ppfValBuf, ctxFont);` — no `PP_NONE` test. The consumer at
`:692` is `if(withResult && resultRun != PP_NONE)`, so the tail is skipped and
`:704-705` returns `stackNode[0]` with `true`. Every other allocation failure in
the same function returns false (`:594`, `:639`, `:655`, `:677`, `:695`).

**Reaching input, measured.** Integer mode, base 2, WSIZE 64 (the default), and
`LEAD.0` set (items.c item 1857 — a user-facing display setting that pads a
short integer to its full word). Keys: `0` `→INT 2` `ENTER` `1` `→INT 2` `−`,
then `ENTER` `ENTER`, then `→INT 2` again (unclassified and `US_ENABLED`, so
`ppcClassify` returns `PPC_INVALIDATE` and the slots go UNKNOWN while the
registers keep their values), then `+` and `+` (each `PPC_DY` mints a `PPN_VAL`
leaf through `ppcEnsureKnown`), then `CLSTK` to file, then `PHIST`.

The probe ran that sequence twice, once per setting:

```
PROBE R8: cfg=0 ws=64 lead0=0   spell len=118
PROBE R8: entry 0 built=1 hasEq=1 siglen=493
PROBE R8: cfg=1 ws=64 lead0=1   spell len=160
PROBE R8: entry 0 built=1 hasEq=0 siglen=490
PROBE R8: entry 0 streamHasTKRES=1 totalBytes=68
```

Serialization is postfix, so three value runs at 161 bytes and two `"+"` runs
consume 487 of `PP_TEXT_BYTES` (512) before the `TKRES` run asks for 161. The
threshold is `4L + 8 > 512`, i.e. any operand spelling of 127 bytes or more, so
under `LEAD.0` in base 2 at WSIZE 64 the whole class of three-leaf integer
formulas degrades — wider than one contrived row. `prettyFormula.c:20-21`
already names 160 bytes as "the widest reachable spelling".

**What breaks.** The browser row paints the formula with no `= result`, while
every neighbouring row carries one and while row 0 of the same formula draws
complete. The owner cannot tell this from the legitimate "filed without a
result" state `ppcEmit` produces. Worse: `prettyBrowserEnter` still recalls it.
`pbFindResult` (`browsers/prettyBrowser.c:162-176`) walks the same stream with
no allocation and does find the `TKRES`, so pressing ENTER on a row that showed
no result pushes a number onto X that the row never displayed.

**Contract violated.** `DESIGN.md:129-131`, **BINDING**, quoted in full above.
A text-pool overflow here paints something and returns true. The suite already
states the rule as a pin — `prettyTest.c:1836` fails with *"T27 the filed
formula has no result and can never be recalled"* — but T27 drives
`2 ENTER 3.7 + IP`, whose operands spell one to three bytes.

**Attribution.** Before this wave, `ppfFormatStaged(ppfValBuf, 96)` refused a
160-byte spelling outright and the whole entry declined; that refusal is
`PP18RR7-2`, which the wave correctly fixed. Round 7's own §4 recorded this
degradation and said "nobody could construct an entry that exhausts the pool
exactly there". Raising one budget against an unchanged other one is what
constructs it.

**Bug class.** Two size budgets that must agree, with the relation between them
written nowhere. Enlarging the one that was measured moved the other one's
failure into reach, and the failure arm of the second budget was already the
only degrading arm in the function.

**Class-level test that would pin it.** File a formula whose leaves consume the
text pool to within one run of the cap and assert `ppfBuildEntry` **declines**
rather than degrades. It is enumerable from the formatter's own worst cases:
for each leaf type, the widest spelling the formatter can emit × the number of
leaves the arena allows.

---

### PP18RR8-4 — `ppcSerializeNode` files trees whose postfix replay needs more than `ppfBuildEntry`'s eight operand slots, so an entry that filed can never be drawn again

**Where.** Producer: `packages/pretty-print/prettyCapture.c:270-392`, which
recurses `child[0]`, `child[1]`, operator with no operand-stack bound, no depth
guard and no token cap. Consumer: `prettyFormula.c:562-563`, `uint8_t
stackNode[8]; int stackPrec[8];`, with `sp >= 8` refusing at `:573`, `:601` and
`:615`.

**Reaching input, measured.** `1` `ENTER` `2` `+` builds `1+2`. Then repeat
seven times: a digit, `x<>y` (`ITM_XexY`, classified `PPC_SWAP`), `+`. Each
repetition puts the accumulated tree in X, so the `PPC_DY` arm makes it
`child[1]` and the tree grows right-deep; postfix peak depth is
repetitions + 2. Twenty-five keystrokes, 17 of the 24 arena nodes. Then displace
the formula and press `PHIST`. The probe drove the real capture path:

```
AUDIT-PROBE R8[5]: liveRow=1  hist=1  entryBytes=63 filed=1  filedRow=1
AUDIT-PROBE R8[6]: liveRow=1  hist=1  entryBytes=69 filed=1  filedRow=1
AUDIT-PROBE R8[7]: liveRow=1  hist=1  entryBytes=75 filed=0  filedRow=0
```

`filedRow` is `ppfBuildRow(0, 0, true, …)` — the exact call
`browsers/prettyBrowser.c:71` makes. The boundary is the 8-slot bound and
nothing else: every plausible escape was checked and closed. `PPC_NODES` is 24
against 17 needed; the ring is not the limit (75 bytes against a 320-byte
per-entry cap, so the oversize-drop rule never fires); the layout side is clear
(`PP_MAX_DEPTH` 12 and `PP_POOL_NODES` 72 against ~9 levels and ~28 nodes,
because an equal-precedence `ADD` right operand takes no paren box).

**What breaks.** `ppfBuildRow` returns false, so the CM-20 browser paints
*"(too large to show)"* (`browsers/prettyBrowser.c:84-89`) and `fnPrettyHist`
drops the row from both pager passes — permanently, since the entry stays in the
ring — for a formula the live surface drew correctly one row above a moment
earlier. ENTER-recall still works, because `pbFindResult` scans the same stream
with no operand stack. So the row is unshowable and recallable at the same time.

**Contract violated.** `prettyFormula.c:83-85`: the builders are *"shared by the
tree walker and the token decoder so both paths typeset identically."*
`prettyTest.c:2564-2566`, this wave's own helper header: *"require the same
layout signature on both surfaces."* The sibling decoder in the browser carries
an explicit agreement comment for the token set — *"this decoder must know every
token `ppfBuildEntry` knows, or an entry renders but refuses to recall"*
(`browsers/prettyBrowser.c:186-187`) — and nothing states the stack bound.
`DESIGN.md` §5 describes the ring and its eviction and states no operand-slot
bound.

**Attribution.** Pre-existing, not introduced by this wave. It is in scope
because the wave's new pin would catch it — "the filed entry does not decode" is
exactly what `ppfTestFiledMatchesLive` and T27 assert — if any fixture built a
right-deep chain. Every T25/T27/T28 fixture is depth 2 or 3. The suite stayed
green with the 7-repetition case failing to decode.

**Bug class.** A producer with no bound wired to a consumer with a fixed one,
where the bound is a private array size in the consumer's own frame.

**Class-level test that would pin it.** Serialize-then-decode as a property over
depth: build a right-deep chain up to the arena's node limit, file each, and
assert every filed entry decodes. The suite already has the driver for it
(`ppcTestType` / `ppcTestOp`); the loop is three lines.

---

### PP18RR8-5 — the formula view's complex leaf forces rectangular form, so a polar value drawn on the stack line is redrawn as `a+ib` in PHIST and the browser

**Where.** `packages/pretty-print/prettyFormula.c:50-52`. The `dtComplex34` arm
passes the register tag for `tagAngle` and a literal `false` for `tagPolar`:

```c
complex34ToDisplayString(REGISTER_COMPLEX34_DATA(TEMP_REGISTER_1), buf, &standardFont,
                         180, 8, LIMITEXP, !FRONTSPACE, NOIRFRAC,
                         (uint16_t)getRegisterTag(TEMP_REGISTER_1), false);
```

In `display.c:1486-1510` the `tagAngle` argument is read **only** inside
`if(tagPolar)`, so passing the real tag there is inert and the literal `false`
is the whole decision.

**Reaching input.** Set polar complex display (`FLAG_POLAR`). Key a complex —
`3` `CC` `4`, so X shows `5 ∡ 53.13`. `STO A`, `RCL A` (param > 99 →
`ppcRclLeaf` → `ppcValLeafFromRegister`; the leaf stores `aux = dtComplex34` and
`pad[0] = getRegisterTag` = `amPolar|am`, and 32 bytes fits `PPC_VAL_CAPACITY`
so it is a VAL+VAL2 leaf, not OPAQUE). Then `2` and `×`. Then `PHIST`.
`ppfStageValFields` calls `setRegisterTag` and restores the `amPolar` bit one
line before `ppfFormatStaged` throws it away. The leaf is also reachable through
`ppcEnsureKnown` on the ordinary op path, LASTx, the STO staging pair and the
as-typed-unavailable arm, so `RCL` is one route of six.

**What breaks.** The browser row and the PHIST row show the operand as `3+i4`
while the same value on the stack line, on PSHOW and in the owner's own entry
reads `5 ∡ 53.13`. The number is right; its representation silently changes
between two surfaces of the same feature. The filed entry carries the same tag
byte and takes the same formatter, so the new equality pin cannot see it — both
sides are wrong together.

**Contract violated.** `prettyValue.c:585-589` states the package's rule for
this type: *"complex (rectangular only) … Polar forms (∠) decline."*
`DESIGN.md:165`: *"Complex (PP2): … polar stays linear"* — which means the polar
spelling is kept and not laid out in 2D, not that it is converted.
`DESIGN.md:181-182` requires value leaves be *"formatted only at display time by
staging into `TEMP_REGISTER_1` and calling the standard display builders"*; the
leaf carries the tag precisely so the builder reproduces the owner's display.
`DESIGN.md:584` anticipates a leaf whose text is *"a tagged angle, a complex"*
and rules only on its precedence. The sibling call at `prettyValue.c:764-767`
passes `getComplexRegisterAngularMode(regist)` and
`getComplexRegisterPolarMode(regist) == amPolar`. All eleven upstream call sites
of `complex34ToDisplayString` pass the register's real mode or `FLAG_POLAR`;
none passes a literal `false`. No comment explains the difference, and
`grep -rni polar` over `design-docs/pretty-print/` returns two hits, neither of
which sanctions it.

**Confidence.** Traced end to end and refuted on the intent lens; no probe was
built. The mechanism is a source-level identity (`false` is a literal), so what
is untested is the keystroke path, not the arithmetic.

**Bug class.** A value's tag captured, carried through filing, restored at
display time, and then discarded by a literal in the formatter.

**Class-level test that would pin it.** For each register tag a value leaf can
carry, assert the formula-view spelling equals the register-line spelling for
the same register. That is the cross-surface oracle the suite does not have, and
it is the one this round's axis keeps asking for.

---

### PP18RR8-6 — `ppfTextIsAtom` rejects every short-integer spelling, because the base subscript sits outside its accepted glyph window, so every integer value leaf is bracketed on stock defaults

Filed independently by two dimensions from two call sites; merged here.

**Where.** `packages/pretty-print/prettyFormula.c:129` — above `0x80` the
predicate accepts a two-byte glyph only in `0xa000..0xa00f`. Reached from
`:493` (`ppfFromCaptureNode`'s `PPN_VAL` arm) and from `:605`
(`ppfBuildEntry`'s `PPT_TKV` arm, which **this wave added**).

`ppfFormatStaged` → `shortIntegerToDisplayString` ends in `addBaseNumber`
(`src/c47/display.c:1912-1916`) on every one of its return paths:
`strcat(displayString, STD_BASE_2); displayString[strlen-1] += base - 2;`.
`STD_BASE_2` is `"\xa4\x61"` (fonts.h:642), so the appended code is
`0xa461..0xa46f` for bases 2 through 16 — outside the window, for every base
including 10.

**Reaching input, measured, on shipped defaults.** `10` `→INT 10` `ENTER` `2`
`×`. `→INT` is unclassified and `US_ENABLED`, so `ppcClassify` returns
`PPC_INVALIDATE`; the `MULT`'s `ppcEnsureKnown(1)` then mints a `PPN_VAL` leaf
from Y. Two probes:

```
AUDIT-PROBE R8 dtX=8 capsig='# 2 ×' livesig='[P(10₁₀) · 2]' firstrun='10₁₀' atom=0
AUDIT-PROBE R8 base=10 sig='[96<A0>474<A0>491<A0>253<A0>315<A4>i - P(1<A4>i)]'
```

The base-10 row kills the obvious refutations: the digits are plain ASCII, so
the rejection comes from the base glyph alone — not from a hex letter, and not
from `PP18RR7-6`'s non-default separator (the `<A0>` group separators in the
signature are inside the accepted window). The second probe ran inside **T28's
own five-base loop**: every T28 row files a picture that reads `0₂ − (1₂)` while
the gate stays green.

**What breaks.** Every integer-mode operand that reaches the formula view as a
value leaf draws bracketed — `(10₁₆)·2`, `0₂ − (1₂)`, `(FF₁₆)²`. Where the other
operand of the same expression arrived as typed NIM text it stays a `PPN_LIT`
and draws bare, so one product shows the same value bracketed on one side and
bare on the other. The extra `PP_PAREN` also widens the row, and `ppfBuildRow`'s
width test with `canPan == false` (`:760`) drops an over-wide row from the PHIST
pager entirely. Never a wrong number.

**Contracts violated.** `prettyFormula.c:107-111`, the predicate's own charter:
*"Is this run text a visual atom: something a raised exponent or a neighboring
operator can sit against without brackets? Only digits, the radix mark and the
digit-group spaces are."* A base-tagged integer can. `DESIGN.md:583-585`
enumerates the class the predicate is meant to catch — *"a signed numeral, a
value in scientific form, a tagged angle, a complex"* — and a based integer is
none of them. `prettyInternal.h:128`, the header line this wave added, states
the caller obligation both call sites satisfy: *"for NUMERIC leaf text only"* —
the text **is** a numeral, and the predicate still answers no.

**Why this is not `PP18RR7-6`, which is ruled deferred.** `PP18RR7-6` needs a
non-default `GAP-L` separator emitting a bare `0x27`; its deferral rests on a
stated reachability claim: *"Owners on the default space separator (`\xa0\x08`,
inside the accepted `0xa000..0xa00f` window) see none of it, which is why the
gate is green."* That sentence is false. This defect needs no setting, fires on
the whole `dtShortInteger` type in every base, and arrives through a different
producer. Round 7 also carries an unnumbered minor note on the sibling `PPN_LIT`
path (`(1F#16)·2`), declined as cosmetic; that note names a different call site
and predates `4361b80e9`'s addition of the predicate call at `:605`.

**Why no pin sees it.** T28 asserts only that the entry decodes.
`ppfTestFiledMatchesLive` compares filed against live, and both bracket
identically — the oracle is satisfied by the defect.

**Bug class.** A predicate that enumerates an accepted alphabet against a
producer whose emit set nobody enumerated. The accepted set is 16 codes; the
formatter's reachable set includes at least the 15 base subscripts, the
superscript digits and three BCD marks.

**Class-level test that would pin it.** For every leaf data type crossed with
every display mode the formatter can be in, assert `ppfTextIsAtom` agrees with a
written list of non-atoms. The enumeration is small and it is the artefact the
predicate is missing: today the accepted alphabet is 16 codes chosen by hand and
the emit set is unbounded.

---

### PP18RR8-7 — browser pan is one-way, and on a single-row browser nothing resets it

**Where.** `packages/pretty-print/browsers/prettyBrowser.c:137`,
`pbPan = (int16_t)(pbPan + 60);`, clamped at `:96`. `pbPan` occurs only in this
file (checked across the working area, the generated `files/` and the patches).
The three assignments to 0 are `:113` (mode entry), `:125` (Up) and `:132`
(Down).

**Reaching input, measured.** `PCLR`, then a wide formula, then `CLx` — which
displaces it, so `ppcEmit` files it and `ppcCurrentFormulaRoot()` is `PPC_NIL`.
`pbTotalRows()` is then 1. `PHIST` enters with `pbSelection = 0`, `pbPan = 0`.
Press `.d` twice. With `totalRows == 1`, `prettyBrowserUp`'s `pbSelection > 0`
and `prettyBrowserDown`'s `pbSelection + 1 < pbTotalRows()` are both false, so
neither reset can run. This is the fixture T29 already builds:

```
PROBE hist=1 haveCurrent=0 totalRows=1
PROBE row0 built=1 width=414 visible=388        (maxPan = 26)
PROBE sig  fresh=1061055198 panned=2850367209 afterUpDown=2850367209
PROBE recovered=0 panIsOneWay=1
```

Up then Down leaves the painted pixels bit-identical to the panned frame, not
the fresh one.

**What breaks.** The owner has panned to the right end of the only row and
cannot get back to the left end while the browser is open. Every further `.d` is
a no-op and there is no pan-left key. `ITM_PHIST` has no earlier clause in
`keyboard.c`, so a second `PHIST` press is swallowed by the containment arm at
`:2793` and cannot re-enter. Recovery is EXIT then PHIST.

**Contract violated.** `browsers/prettyBrowser.h`'s key contract: *"UP/DOWN
select, `.d` pans a too-wide selected row"*. The pan is documented as the way to
read a row that does not fit, and half of that row becomes unreachable.

**Attribution, corrected.** The wrap→clamp change landed in `09c1d4b87` (audit
r3, R3-10/R3-11); its removed comment block reads *"a 60 px step wrapped it to 0
on the next paint. Rows are now accepted at any width, so this is the real
arm."* `fd080dee9` only deleted the header's stale *"(wraps)"* to match code
that had already changed. The comment pass did not cause this — it removed the
last written trace of the affordance the r3 fix had dropped. No ruling covering
the clamp tradeoff exists in `DESIGN.md`; `DESIGN-HISTORY.md:924` still describes
the old wrapping behaviour.

**Bug class.** An affordance removed by a fix, with the replacement's reset path
living on a sibling operation the degenerate case cannot reach.

**Class-level test that would pin it.** With `totalRows == 1` and a row wider
than the band, assert some key sequence inside the browser returns the paint to
`pbPan == 0`. The suite can already sign the LCD, so the assertion is a
signature comparison against the fresh frame.

---

### PP18RR8-8 — EQ36 passes on trailing-text rejection, so it stops testing `PP18RR7-3` the moment the numeral fits, and today's margin is one byte

**Where.** `packages/pretty-print/prettyTest.c:3268` — the fixture is
`"1+1+1+1+1+"` + 492 × `'7'` + `"abc/2"`, and the only assertion is the negative
`if(ppqParse(big, …)) ppTestFail(…)`.

**Measured, by two mutations.** Removing the `PP18RR7-3` latch alone turns the
gate RED with EQ36 the **sole** failure — so the pin is live today, and it is
the only pin guarding the fix:

```
prettyPrint test FAIL: EQ36 a numeral vanished into the text pool and the parse still succeeded
```

Removing the latch **and** shortening the fixture numeral from 492 to 491
returns the whole gate to GREEN. The arithmetic: `ppNewRun`'s gate is
`ppTextLen + len + 1 > PP_TEXT_BYTES` (`prettyLayout.c:125`, `PP_TEXT_BYTES` 512
at `prettyInternal.h:31`); the five leading `"1+"` terms cost exactly 20 bytes,
so 20 + 492 + 1 = 513 overflows and 20 + 491 + 1 = 512 does not.

**What breaks.** In the fits case `ppqPrimary` returns the number,
`ppqTerm`/`ppqExpr` stop at `'a'`, and `ppqParse` still returns false at
`prettyEquation.c:784` on the unconsumed `"abc/2"` — the negative assertion is
satisfied by an unrelated rejection, with no diagnostic. Raise `PP_TEXT_BYTES`,
or change how `ppqExpr`/`ppqNumber` size their leading runs, and the only pin
for the fix goes silent while the shipped defect walks back in: an equation
draws with its numeral missing and reports success.

**Contract violated.** The file's own stated idiom for exactly this, at
`prettyTest.c:1535`: *"The SUB-10 glyph is the reach check: without it the row
passes while testing nothing."* EQ36 carries no reach check, and its own comment
at `:3255` asserts the mechanism it never verifies: *"The leading terms lift
`ppTextLen` so the legal-length numeral overflows while the short name after it
still fits."*

**One aggravating detail.** `ppqPrimary` does not check `c->failed` after
`ppqNumber` returns `PP_NONE`; it falls through to `ppqName`, consumes `"abc"`
and allocates a run for it. The latch is the only thing between the overflow and
a name silently substituted for the number. (The caller one frame up does test
it — `prettyEquation.c:565`, `if(c->failed || n == PP_NONE)` — which is why the
parse still declines. That is the fact that answers Gemini's item 1 in §4.)

**Bug class.** A pin whose reach is an arithmetic coincidence between a
hand-computed literal in one file and a `#define` in another, asserted by
neither.

**Class-level test that would pin it.** Assert the overflow happened, not only
that the parse declined — for example that `ppqParse` failed with `c.pos` still
at the numeral, or add a positive control at one digit less that must parse. The
general form is the same one the T25 rows already use.

---

### PP18RR8-9 — T28 asserts only a boolean and has no width reach-guard, though width is the whole point of the row

**Where.** `packages/pretty-print/prettyTest.c:1659` — the row's only assertion
is `if(!ppfBuildEntry(…))`. It carries a type reach-guard at `:1651`
(`getRegisterDataType(REGISTER_X) != dtShortInteger`) and nothing about the
spelling's length.

**Measured, by two mutations.** Restoring the `PP18RR7-2` defect
(`ppfValBuf` 200 → 96) turns the gate RED, with base 2 the only failing base —
so the row's whole reach is one knife-edge:

```
prettyPrint test FAIL: T28 the widest word does not decode in this base (expected 2, actual 0)
```

Keeping that defect and changing `WS=64` to `WS=16` on the driver preamble line
(`testSuite/tests/pretty_print.txt` line 8, six drivers earlier) returns the
whole suite to **GREEN, 0 failed**. At WSIZE 16 the widest base-2 word spells
about 24 bytes and fits the 96-byte buffer.

**What breaks.** The pin that certifies the 200-byte `ppfValBuf` budget can stop
certifying it with no failure printed, and the shipped defect returns: a base-2
WSIZE-64 word shows as *"too complex to pretty-print"* in PHIST and the browser
while upstream draws it on one line. The width comes from two ambient globals
the row neither sets nor asserts — `shortIntegerWordSize`, and the digit-group
settings. `ppcTestReset` restores nine pieces of state, including
`lastIntegerBase` added by `PP18RR7-9` in this very wave for precisely this
residue class, and not the word size.

**Contract violated.** The row's own comment at `prettyTest.c:1639`: *"for every
base, the widest word must decode on the filed surface"* — the body never
establishes that the word was wide. The sibling rows in the same block all state
their reach explicitly ("so the row tests nothing", at `:1552`, `:1580`,
`:1605`, `:1651`).

**Bug class.** A row whose precondition lives in another file's ambient state,
with the assertion written as if the precondition were local.

**Class-level test that would pin it.** Assert the spelling's length before
asserting the decode — the sibling idiom, one line. The general form is that any
row whose name contains a superlative ("the widest", "too wide", "the deepest")
must assert the superlative.

---

### PP18RR8-10 — the amended BINDING fallback rule proves the `tmpString` write safe with gates that only one of the three surfaces reaching the formatter has

**Where.** `design-docs/pretty-print/DESIGN.md:136-139`, amended in this range:
*"On every surface that reaches the formatter, upstream holds no live
`tmpString` data across the call — the try-function declines first (the
`temporaryInformation` / `lastErrorCode` / `calcMode` gates in
`prettyTryRegisterLine`)."*

**Reaching input.** A static call path with no gate anywhere on it:
`screen.c:6190` `case CM_PRETTY_BROWSER: prettyBrowser(NOPARAM);` →
`prettyBrowser.c:108` (calcMode is already 20, so `pbPaint` runs) → `pbPaint`
`:54`/`:71` → `ppfBuildRow` `:739` → `ppfBuildEntry` `:592` → `ppfFormatStaged`
`:38` → `shortIntegerToDisplayString(TEMP_REGISTER_1, tmpString, false, 0)` at
`:65`. `pbPaint` tests neither `temporaryInformation` nor `lastErrorCode` nor
`calcMode`. `fnPrettyHist` (`:773`) tests `lastErrorCode` only. Only
`prettyTryRegisterLine` (`prettyValue.c:787-796`) carries the three gates the
rule cites. The concrete entry shape is T26's own fixture — a base-16 integer,
ENTER, 5, `+`, `CLSTK` — which is byte-for-byte what `ppfBuildRow` hands
`ppfBuildEntry` for browser row 0.

**What breaks.** Nothing today. No upstream arm was found holding live
`tmpString` across a browser repaint, and under calcMode 20 upstream's
register-line rendering is not on the stack. The cost is to the next maintainer:
the rule reads as if `prettyTryRegisterLine`'s gates protect every path, so a
surface added to the browser or the pager inherits a safety argument that was
never made for it, and a rule marked BINDING cannot be checked against
two thirds of its own scope. The nearby sentence *"each upstream arm writes
`tmpString` before it reads it"* does not cover the gap — it addresses the
fallback ordering, not the re-entrancy hazard where upstream writes the buffer,
a repaint clobbers it, and upstream reads its own stale bytes.

**Contract violated.** Its own words, and the code half's ruling. This is not
`PP18RR7-5`: that ruling is that the write stays and the rule names it as the
exception. The exception is named; the proof attached to it covers one surface.

**Bug class.** A safety argument written for the surface the author had open,
generalised in the sentence and not in the reasoning.

**Class-level test that would pin it.** None mechanical. The rule should state
the invariant per surface, or state the calcMode-20 argument the browser
actually relies on — that upstream's register-line renderer is not on the stack.

---

## 4. PLAUSIBLE, and what went unverified

Three findings survived refutation with no constructible reaching input. Each
says what would settle it.

1. **`ppqExpr` swallows a `PP_NONE` from the leading-sign run without latching
   `c->failed`** (`prettyEquation.c:691`). `lead = ppNewRun(c->s + c->pos, 1,
   font); c->pos = next;` advances past the sign unconditionally, and `:700`
   (`if(lead != PP_NONE)`) then drops it and returns a successful subtree — the
   exact class `PP18RR7-3` just fixed one frame down. Unreachable today for an
   arithmetic reason, not because of a check: the lead is a one-byte run and the
   first allocation in the call, so any pool state that refuses it refuses every
   later run too, and `ppqPrimary` bottoms out in `ppqNumber` (which now
   latches) or `ppqName` (latched by `ppqPrimary:559`). The invariant holds by
   accident of the grammar. Note why the wave's sweep could not see it: round 7
   checked *"every `PP_NONE` **return** in `ppqFactor`/`ppqTerm`/`ppqExpr`"*,
   and this sentinel is assigned to a local and tested away. **What would settle
   it:** any future `ppqPrimary` path that needs no fresh run — a cached run, a
   shared `'('` node, text-pool reuse. Then EQSHW paints `3+x` for a stored
   `-3+x` and reports success.

2. **`ppfTestSigNode`'s separators and closers are appended with no length
   re-check** (`prettyTest.c:2481`, `:2485`, and the same closers in the
   FRAC/SUP/RAD/SUB/BARS/PAREN/BIGOP arms). The entry guard at `:2456`
   (`if(strlen(out) + 24 >= cap) return;`) reserves 24 bytes and is only ever
   charged for the frame's opening bracket. Measurement corroborated the
   mechanism and corrected the reasoning: across the whole suite the guard
   **never fires once**, and the high-water is 43 of cap 192 — but a fixture in
   the class T28 already builds (the widest base-2 word, filed with its result)
   produced a signature of **191 characters in a 192-byte buffer**, filled
   entirely by the `PP_RUN` arm's own independent clamp at `:2468`. The reserve
   is not consumed on unwind; it is never consulted, and the current margin is
   one byte. The new `ppfTestFiledMatchesLive` puts `liveSig[192]` and
   `filedSig[192]` adjacent on the stack. **What would settle it:** a fixture
   with a run of 160 bytes or more as a nested `PPT_TKV` operand — producible in
   principle through a typed literal over `PPC_LIT_CAPACITY` or through
   `ppcEnsureKnown` minting a value leaf — so that two or more closers are
   pending above it. The `PPT_TKRES` route cannot do it: `ppfBuildEntry` makes
   the result the last child of the root, so exactly one closer follows, and
   `ppfTestFiledMatchesLive` passes `withResult = false` anyway.

3. **`ppqFrameIntegral` degrades twice on allocation failure, and the second
   degradation drops the integral sign itself while EQSHW reports success**
   (`prettyEquation.c:862`, `:867`). If any of the five allocations fails,
   control falls to the bare-stroke arm — which is also the arm for "no live
   limits at all", so allocation failure is indistinguishable from the
   condition case; if `ppNewBox(PP_INT)` then fails, the function returns the
   bare integrand and `ppqShowRender:955` still sets `pretty = true`. Round 7
   recorded the same shape at the twin `ppqFrameDerivative` and did not name
   this one, which has the extra silent level. Nothing rules it deliberate: the
   header contract (`prettyInternal.h:114`) enumerates two outcomes and no
   third, `DESIGN.md:361-366` scopes the fallback to a **condition**
   (`SOLVER_STATUS_INTERACTIVE`, non-real limits), and the BINDING rule's
   failure list names "pool/text/depth overflow". **What would settle it:** a
   parse that consumed nearly all 72 pool nodes and still measures inside the
   21..167 band.

### Recorded, not verified — beyond the refutation cap

These eleven items had no independent refutation pass. They are not findings
until one runs. Where the in-family read already establishes a relevant fact,
it is stated so the next round does not re-derive it.

**In-family, one item.**

- **The comment-only pass left the one comment a prior review had already
  declared false.** `packages/pretty-print/solver/equation.c:1814` still says the
  `PARSER_*` macros bind to `mvarBuffer`'s name.
  `REVIEW_upstream-minimality_2026-08-27.md` §3.2 records it as stale:
  *"`equation.c:1799` comment stale — 'the `PARSER_*` macros bind to this name'
  describes a coupling the block does not have."* `fd080dee9` rewrote both
  neighbours (the `ppEqEvalSlot` header two lines above, the `END_OF_FORMULA`
  note four lines below) and left this line untouched. In `ppEqEvalSlot`,
  `mvarBuffer` is a plain local passed as an argument to `parseEquation`; the
  only `PARSER_*` uses in the block are the two stack-size constants inside
  `PPEQ_STATE_BYTES`. The comment is the block's only in-file statement about its
  coupling to `equation.c`'s file-local macros, and it is the fact the extraction
  decision turns on — a reader who trusts it concludes the block cannot leave
  `equation.c`, when the real seam is two integer constants plus one exported
  static. `PP18RR7-8`-adjacent but distinct: `-8` is the hook table, this is a
  comment inside the override.

**Out of family — sol / gpt, seven items** (`packet-sol-pinnet.md`).

1. **Big-operator limits build in `ctxFont` on the filed surface and `childFont`
   on the live one**, so filed rows would draw the limits large. Sol's own
   caveat names the escape: *"if it clones/restyles its inputs, that undocumented
   behavior would be the only caveat."* It does. `ppfBigop` calls
   `ppSetFontDeep(fromN, childFont)` and `ppSetFontDeep(toN, childFont)`
   unconditionally (`prettyFormula.c:392-393`) on both surfaces, which the
   in-family parity sweep in §6 records. The packet did not carry that body —
   see §5, D8. Ranked first by its author; answered by code the reader was not
   given.
2. **The equality oracle is font-blind**: the helper builds both surfaces at
   `(STANDARD, STANDARD)` while the pager decodes at `(STANDARD, TINY)`, and
   `ppfTestSigNode` records no font. True as stated, and the in-family
   refutation established the sharper form of it: no builder in the package
   branches on font at all, so no change to the helper's font arguments could
   catch anything — the property is unpinnable in a signature oracle, which is
   §5's D1 and not a defect in this helper.
3. **The signature writer strips ASCII spaces** (`prettyTest.c:2469`), so a
   grouping or spacing divergence compares equal. Correct and unreached: the
   default group separator is the two-byte `0xa008` glyph, which survives, and
   no current producer emits an ASCII-space difference. Sol says so itself
   (*"No current producer difference is demonstrated"*).
4. **The decoder accepts a 31-byte `PPT_TKL` while the live literal leaf reads at
   most 15+15 = 30**, so a boundary-length literal could differ by one digit
   across surfaces. Sol marks it UNREACHED for want of the serializer. The
   in-family read supplies the missing half: T26 pins that capture withholds the
   formula at 31 characters, and `prettyNoteNimText` gates on
   `n > PPC_LIT_CAPACITY` before the leaf is built, so no 31-byte literal is
   filed. Recorded so the boundary is not re-derived.
5. **`withResult = false` excludes the result half of every filed picture.**
   True of the helper, and forced: `ppfBuildCurrent` has no result node, so
   `true` makes every row unequal by construction (measured — all eight rows fail
   with the structural reason printed). The tail is held by four older pins
   (FV3, T22, T23b, T27), demonstrated by mutation. The *reachable* defect in
   this area is `PP18RR8-3`, a different mechanism.
6. **Name leaves and operator-token mappings have no live/filed equality
   coverage**, so a filing-side item-number error would survive. Sol marks it
   UNREACHED for want of the serializer. The in-family parity sweep read the
   serializer and found `PPN_CONST`/`TKC` and `PPN_RCL`/`TKR` emitting the same
   text at the same precedence, so this stands as a coverage gap with no
   demonstrated producer error.
7. **The predicate's spelling classes are sparsely pinned** — no equality row
   drives comma radix, grouped, angular or complex spellings. A coverage claim,
   and this round's `PP18RR8-5` and `PP18RR8-6` are two members of exactly that
   unpinned set, found from the other direction.

**Out of family — gemini / gemini, three items** (`packet-gemini-fixwave.md`).

1. **`ppqPrimary` consumes `ppqNumber`'s `PP_NONE` without checking `c->failed`,
   so the latched failure still parses garbage and reports success.** The first
   clause is true and is recorded inside `PP18RR8-8`. The consequence is not:
   `ppqFactor`'s first line after `ppqPrimary` is
   `if(c->failed || n == PP_NONE) return PP_NONE;` (`prettyEquation.c:565`), and
   `ppqExpr` tests the same at `:696`, so the latch is honoured one frame up —
   and EQ36 drives Gemini's exact input and passes post-fix, i.e. the parse
   declines. What survives is a style point about where the latch is read.
2. **T28's `continue` skips `ITM_CLSTK`, leaving the failed iteration's operands
   on the stack.** Only reachable after the reach guard has already failed the
   suite loudly (`ppTestFailInt` fires before the `continue`), so it can only
   confuse the diagnosis of an already-red run. Real, small, unverified.
3. **`ppfRunPrec`'s first-character fast-exit stakes ATOM for text
   `ppfTextIsAtom` would call a term** (leading `'+'`, letters). This is round
   7's refuted NaN finding through another door: the name-versus-value split is
   the documented design (*"a name is an atom whatever its glyphs"*),
   `ppfRunPrec`'s only caller is `ppqFactor`, `ppqNumber` never emits a leading
   `'+'`, and the capture hook strips a typed one. Both example spellings are
   unreachable through the door named.

Gemini's item 4 is not a finding: it multiplies out both length comparisons on
the staging path, states that the deciding gate is 200 in both cases and that
the 160-byte worst case passes, and clears the buffer repair. That is an
independent confirmation of `PP18RR7-2` from outside the family, and it agrees
with the in-family re-derivation in §6.

---

## 5. Design observations

**D1 — an equality oracle cannot see a defect the two surfaces share, and that
is where this package's defects now live.** `ppfTestFiledMatchesLive` is the
right pin for `PP18RR7-1` and it worked: reverting that fix turns both T27 rows
red with the missing bracket printed. But three of this round's ten confirmed
findings are invisible to it **by construction** — the base subscript
(`PP18RR8-6`), polar complex (`PP18RR8-5`), and the tiny-rung blank glyph
(`PP18RR8-1`) are all wrong identically on both sides. Round 7's own D3 said the
class is defined across surfaces and asked for surface equality; the wave built
the filed-versus-live half. The half still missing is the one that would have
caught all three: **formula view against the register line**, for the same
value. The out-of-family pass reached the same conclusion from the opposite
direction and without the findings — Sol's items 2, 3 and 7 are all "the oracle
cannot express this" — which is two families agreeing independently, and that is
evidence rather than one reader's opinion.

**D2 — the predicate's alphabet is an emit set, and nobody owns the emit set.**
`ppfTextIsAtom` accepts 16 hand-chosen codes. The display builders emit base
subscripts (15 codes), superscript digits and signs (12), BCD marks (3), group
separators (2 used, more available) and the binary digit glyphs. Two deferrals
(`PP18RR7-6`) and one confirmed finding (`PP18RR8-6`) come from that one gap,
and the deferral's stated reachability argument was falsified by the confirmed
one. The stable shape is either to ask the producer what it wrote, or to
enumerate what the formatter can emit and test the predicate against that list.

**D3 — the pins bind to arithmetic coincidences in other files.** EQ36's reach
is `20 + 492 + 1 > 512` — a literal here against a `#define` there, with one
byte of margin and no assertion connecting them. T28's reach is a `WS=64` on a
driver preamble six drivers upstream. Both were demonstrated to go silently
vacuous under a one-token change. The file already owns the cure and states it
in a comment (`prettyTest.c:1535`); it is applied on the T25 rows and not on the
new ones. A pin that names a superlative must assert the superlative.

**D4 — two budgets that must agree, with the relation written nowhere.**
`ppfValBuf` 96→200 was correct and the measured 160-byte worst case is real. It
was raised against an unchanged `PP_TEXT_BYTES` 512, which is what moved
`ppfBuildEntry`'s one degrading arm into reach (`PP18RR8-3`). Round 7 recorded
that arm as unreachable in §4; the wave that recorded it made it reachable in
the same commit. This is the project's fix-regression shape at one level up from
relocating state: **enlarging a budget is a change to every other budget on the
path**, and no document states which budgets share a path.

**D5 — a guard whose miss contract depends on the object it is given.**
`findGlyph` returns `-1`, `-2` or `0` depending on `font->id`. Every predicate
built on "negative means miss" is therefore font-dependent, and `ppGlyphOf` is
one (`PP18RR8-1`). The package already discovered this for the bold font and
wrote the reason into `screen.c:1196` — and then wrote a second wrapper without
it. A comment recording a hazard is not a guard against the hazard.

**D6 — the solo and combined builds are two configurations, and no run covers
both claims.** `menu_SYSFL` is exact in the combined tree and one short in the
solo one (`PP18RR8-2`). The gate builds solo. The ASAN pass built combined. The
A8 amendment states both numbers and got both wrong. Any claim of the form "ours
is exact and combined is safe" needs the arithmetic asserted in the build, not
recorded in a table.

**D7 — writing a trap down does not remove it.** `7fdda3129` amended
`CLAUDE.md` to name the correct gate and to say the forth-core gate stays green
under a broken pretty-print. Three verifiers ran the wrong gate anyway in the
round that audited that amendment, one of them reporting a green build with the
result tail deleted from `ppfBuildEntry`. The two scripts have the same file
name in different directories, and the wrong one exits 0. That is a naming
problem, not a documentation problem.

**D8 — a packet that omits a body invents a finding in it.** Sol's top-ranked
finding is a font divergence in `ppfBigop`, and it is wrong for one reason: the
packet quoted both call sites and not the function they call, which restyles
both limit subtrees on entry. The reader flagged the gap itself
(*"if it clones/restyles its inputs, that undocumented behavior would be the only
caveat"*) and ranked the finding first anyway, which is the correct behaviour
for a blind reader and the wrong outcome for the round. The skill already
carries the rule this breaks — `45d07a4ea`, round 7: *a packet that gives one
side's establishing fact must give the other's*. The rule was written for
establishing facts and needs one word more: **if a packet quotes two callers of
a shared function, it must quote the shared function.** The same omission is
what put three more of Sol's seven items into UNREACHED for want of the
serializer.

---

## 6. Deliberately not flagged

Two kinds of item are merged here: what the refutation pass disproved, and what
the eight finders cleared during the read. The out-of-family claims are **not**
in this section — none of them was refuted, and they are listed as unverified in
§4 with whatever the in-family read already establishes.

### Killed by the refutation pass

1. **"A duplicated formula files twice: `ppcDeepCopy` clears `PPA_EMITTED`."**
   The mechanism is real and still open — but it is `PP18RR2-8`, a fenced KNOWN
   id, filed on 2026-08-29 with the same file, same root cause, same violated
   `DESIGN.md` §4 clause, and two shorter reaching inputs backed by executed
   probes. `git log -L` over the statement shows the only commits that touched
   it are `db495d984` (PP3, which introduced it), `31ca8821e` (PP12) and this
   wave's comment pass, which moved the line number. Carried, not new.
2. **"`ppfTestFiledMatchesLive` compares signatures that both truncate, drives
   one font rung and `withResult=false`."** Two of its three regressions land
   red, not green: mutating the `PPT_TKRES` arm turns the gate RED with FV3,
   T22, T23b and T27 all failing. `withResult=false` is forced, not a dodge —
   `ppfBuildCurrent` has no result node, so `true` makes every row unequal by
   construction (measured: all eight rows fail, with the structural reason
   printed). The font claim rests on an inverted premise:
   `prettyFormula.c:730` sets `cf = PP_FONT_STANDARD` for rung 0, so the helper
   reproduces rung 0 exactly — and a font-driven signature divergence is
   impossible anyway, because no builder branches on font and `ppfTestSigNode`
   records only kind and text. The truncation residue survives as PLAUSIBLE 2.
3. **"`ppfStageValFields` clears a `lastErrorCode` it did not set, so every
   value-bearing browser row refuses for the rest of the paint."** The premise
   holds; the consequence does not. `prettyFormula.c:29-32` **zeroes** the code,
   so the stale value is consumed by the first value leaf and every later leaf
   sees `ERROR_NONE`. `pbPaint` runs its full pass-1 packing loop before pass 2
   paints anything, so the single spurious refusal is always absorbed in pass 1;
   the *"(too large to show)"* string is printed only from pass 2, which can
   never see a non-zero code from this mechanism. The residue is a one-row
   pass-1/pass-2 height disagreement and the loss of a pending code — a
   different and much smaller defect, on a path the reader could not reach.
4. **"The wave mints duplicate T27 and T28 ids."** Factually right — both labels
   are used twice inside `prettyTestCapture`, and `4361b80e9` minted the second
   pair. The consequence is not: `ppTestFail` prints the whole `what` string,
   every `T*`/`V*`/`EQ*` message literal in the file is unique
   (`sort | uniq -d` returns nothing), and a mutation that broke both T28
   fixtures printed six plainly different red lines. Round 7 already ruled the
   class at the previous two numbers, on this same premise.
5. **"T27's two rows are a parity oracle only — neither can fail for the bracket
   property they are named for."** Refuted by mutation: reverting `PP18RR7-1`
   turns both rows red and prints the exact missing brackets
   (`filed '[3 · -5]'`, `filed '[7 - -5]'`). The finding's chosen mutation
   deletes `+1` in `ppfBuildOp2`, which both surfaces call, so it cannot break
   parity by construction — a different rule with a different pin of record. The
   rows are also documented as parity by design: round 7 prescribed exactly this
   shape, and DESIGN-HISTORY records it landing. (Sol's pin-vacuity verdict on
   T27 is the same claim from outside the family, and the same mutation answers
   it.)
6. **"`ppfFormatStaged`'s destination gate became unreachable when `ppfValBuf`
   grew to the size of its own scratch."** This is the intended end state of
   `PP18RR7-2`, not an accident of it: the fix collapsed two gates onto one
   budget so the reasoned-about gate is the deciding one, and
   `prettyFormula.c:20` says so. The stated consequence is also inverted — the
   guard is a runtime test against the caller's `destSize`, so a future caller
   with a smaller destination is checked exactly as advertised. The function is
   file-static with both call sites in the same file. Gemini's item 4 reached the
   same reading independently.
7. **"Neither new pin can see the result tail."** The failure mode it names —
   `ppfBuildEntry` returns true with the result dropped — was applied as a
   mutation and six pins went red, two of them asserting the tail by exact text
   (FV3, T27). Two supporting facts are wrong on a read: the `TKRES` run **is**
   allocated in the `withResult=false` mode (only the 3-node wrap is skipped),
   and that mode returns the identical node production passes as the wrap's
   first child. What survives is that two new pins do not duplicate coverage
   four older pins carry — economy, not a hole. (The *reachable* version of the
   underlying defect is `PP18RR8-3`, which is a different mechanism: the run
   fails to allocate rather than being skipped.)
8. **"DESIGN.md's amended rule mis-scopes its justification."** Refuted on the
   intent lens and then re-filed on reachability. Round 7 ruled the amendment's
   depth explicitly (*"amend the rule to name the exception and stop there"*)
   and the wave executed it exactly, and the normative sentence is true on the
   browser path. The residue — that the named mechanism exists on one of three
   surfaces — survives as `PP18RR8-10`, which is the smaller claim.
9. **"The new pin compares 192-byte truncated signatures, so two different
   pictures compare equal."** Instrumented and measured: zero guard bails, zero
   run cuts, high-water 41 of 192 across the whole suite, and the eight pin rows
   produce 10 to 20 byte signatures. `ppfTestSigNode` is static test-only code
   fed by hard-coded fixtures, so reaching truncation needs a future edit to the
   test source, not an input. The measured near-miss in a **different** fixture
   class survives as PLAUSIBLE 2.
10. **"DESIGN.md §6's hook table denies patching `keyboard.c`/`softmenus.c`/
    `defines.h` and omits `solver/equation.c`."** This is `PP18RR7-8`, ruled and
    deferred inside the audited range itself (DESIGN-HISTORY.md:1573, *"the hook
    table is the rebaser's document"*). Its two load-bearing sub-claims are also
    wrong: the "denies patching" sentence was examined and explicitly dropped by
    the original reader as anticipatory (§9's own PP4 row calls PP4 *"the
    `keyboard.c` stage"*), and the `solver/equation.c` adjacency question is
    answered in DESIGN-HISTORY.md:1063-1066.

### Cleared during the read

**The four repairs, re-derived rather than trusted.**
`ppfValBuf[200]` is correctly sized and correctly gated: the 160-byte worst case
was re-derived independently (base 2, WSIZE 64 → 64 digits + 15 separators, both
re-spelled as 2-byte glyphs, + the 2-byte base subscript) with 39 bytes of
slack, and both size gates now agree. The `ppqNumber` latch closes its site, and
`ppqName` does **not** need the same treatment — both its callers latch the
sentinel themselves, and `ppqNumber` needed its own only because `ppqPrimary`
falls through to `ppqName` at the advanced position. `ppcTestReset`'s new
`lastIntegerBase = 0` is upstream's own "no base pending" value
(`config.c:544`), tested as such at three upstream sites. The `PP18RR7-1`
conversion makes the filed leaf arms byte-for-byte equivalent to the live ones,
and a fifth leaf class where they still disagree could not be found.

**Live-versus-filed structural parity, checked kind by kind.** All seven capture
kinds against all eight tokens: `PPN_LIT`/`TKL` both cap at 30 bytes and both
now ask the predicate; `PPN_VAL`/`TKV` share `ppfStageValFields`;
`PPN_CONST`/`TKC` and `PPN_RCL`/`TKR` emit the same text at `PPF_PREC_ATOM`;
OP1/OP2 share the builders; `BIGOP`/`TKBIG` reach the same `ppfBigop`, and the
one font asymmetry (live builds the limits at `childFont`, filed at `ctxFont`)
is erased by `ppfBigop`'s two unconditional `ppSetFontDeep` calls at `:392-393`.
That last item is the one an out-of-family reader filed first, from a packet
that did not carry the function. The 31-byte `PPT_TKL` boundary against the
30-byte `PPN_LIT` cap is unreachable, because T26 pins that capture withholds
the formula at 31 characters.

**Bounds and index arithmetic.** `ppfBuildEntry`'s `sp` is bounded at every push
and every pop is guarded; `TKBIG`'s `sp--` then `stackNode[sp-1]` is correct at
`sp == 2`. Token payloads are not bounds-checked against `total`, but
`ppcEmit` is the only writer and checks `off + N > cap` before every write, and
history entries are never persisted or imported. `ppcHistEvictOldest`'s uint16
subtraction cannot underflow (the `ppcHistOffset[0] == 0` invariant holds from
init and survives the shift). The browser's two `(uint8_t)(x - haveCurrent)`
expressions are both preceded by the branch that makes them safe, and
`ppcHistoryEntry` refuses an out-of-range index with NULL. `ppqFitWithEllipsis`
reserves exactly the 3 bytes it needs. `ppqMatchName`'s `pos + l >= len` looks
off by one and is right, because `ppEqConstructIs` reads `s[len]` to require the
`'('`.

**Split payloads and capacities.** `PPC_VAL_CAPACITY` is 32 = exactly two
16-byte payloads, so `bytes - head` is never more than 16 on either the
reassembly or the serializer; only `complex34` gets the continuation. A NIM
literal longer than the leaf cannot be truncated into a wrong number, because
`prettyNoteNimText` gates on `n > PPC_LIT_CAPACITY` first and falls back to a
value leaf. A formula too big for the serializer is dropped, not stored, and
`DESIGN.md` §5 rules that.

**Lifecycle routes that looked open and are not.** Nested in-scope dispatch
cannot strand an outer `ppcStage`: every `runFunction` reachable from inside an
item's own `func` runs under `FLAG_SOLVING`, `PGM_RUNNING`, `FLAG_INTING`,
`CM_MIM` or `CM_PEM`, all of which `ppcScopeOk()` declines. `closeNim` cannot
fire the commit hook outside a real commit — all 20 call sites are guarded by
`calcMode == CM_NIM`, and the backspace-to-empty abort takes
`calcModeNormal()` + `undo()` with no `closeNim`. A stale `ppcPendingLift` after
`fnEdit` opens a NIM without the latch is neutralised by `ITM_EDIT` being
`PPC_INVALIDATE`, which wipes every slot before the commit hook runs — worth a
comment there, not a finding. The eviction loop cannot spin. The history ring's
handoff and the `checkHP` gating asymmetry are both correct as written.

**Containment, with one correction that is not ours to re-file.**
`ITM_op_j` / `ITM_op_j_pol` / `ITM_CC` escape the browser guard, because
`keyboard.c:2793` sits in the `default:` arm and those items have their own case
label whose calcMode list stops at `CM_TIMER`. Confirmed still open — but it is
`PP18RR3-9`, a KNOWN id. The correction worth recording: RR3-9 calls USER-mode
assignment the open route; on the R47 default layout `ITM_op_j` is a **stock**
f-shifted key, which would make it two keystrokes, except that the same sibling
patch owns the shift list at `keyboard.c:1604`, so in the solo build f/g never
resolve inside calcMode 20, and in the combined build undo-history's own range
test closes the case list. Both configurations are safe today, by the sibling —
which is exactly the standing violation RR3-9 records.

**Guards that cannot be falsified, and are correct to keep.**
`ppfBuildEntry`'s `sp >= 8` is defensive rather than dead-by-mistake (see
`PP18RR8-4` for the case where the producer disagrees). `ppfTextIsAtom`'s
truncated-glyph conjunct cannot fire, because all four callers pass `strlen` of
a NUL-terminated buffer — and it errs toward more brackets, never fewer.
`*rootOut = stackNode[0]` cannot hand back `PP_NONE` for a multi-token entry,
because `ppcEmit` refuses to file a bare value and all thirteen builder arms
test their children; a single-token failure ends as "row not shown", the correct
fallback. `pbPan`'s clamp sits inside `if(n->width > visible)` so a narrow row
grows it without bound, which is unreachable for the reasons in `PP18RR8-7`'s
neighbourhood and, on a wide row, self-correcting.

**Rewritten comments checked against their code.** `ppEqFunctionItem`'s new
banner (*"Takes a bare NUL-terminated name, no `'('` check"*) is true — both
callers do their own check. `ppqBuildBigop`'s *"`body` arrives already scoped by
the caller"* is true on both paths. The `PPEQ_STACK_ALLOWANCE` rewrite that
dropped the 5.3 KB measurement for a `§6` pointer is correct: `DESIGN.md:379-380`
carries the number verbatim. The `keyboard.c` arm that lost *"Unreachable in the
combined build, correct in both"* lost a true sentence —
`packages/forth-core/keyboard.c:1847` does swallow calcMode 20 first — and the
loss costs a rebaser one grep, not behaviour.

**Upstream shapes that are catalogued, not defects.** `calcMode < 19` in three
softkey gates (upstream's highest calcMode is 18, and the line is byte-identical
in undo-history so 3-way unifies it). The four `screen.c` x-clipping conjuncts
(appended-conjunct and no-reindent-wrap idioms; the scanner is clean and
DESIGN-HISTORY carries the original measurement). `items.h`'s out-of-order
`ITM_VISUAL` define (diff-minimal by choice; nothing is shadowed). The
`defines.h` insertion anchors (both ≥4 context lines from the sibling's).
`softmenus.c`'s two spare-slot edits. The 578-line `solver/equation.c` block
remains the package's dominant merge exposure and remains correctly deferred as
rebase-adjacent stage work — its coupling evidence needed one correction, in §4.

**Known ids, not re-reported.** `PP18RR7-5`'s code half (the `tmpString` scratch
write, which `DESIGN.md` §1 now names), `-6` (the separator alphabet, deferred),
`-7` (header obligation landed, refactor declined), `-8` (the hook table,
deferred), and every `PP18RR1..RR7` and `PP18R4-1..11` id. `PP18RR8-6` is argued
as distinct from `-6` rather than assumed so, and its argument is that `-6`'s
written deferral premise is false.

---

## 7. Verdict

**Do not ship this tip.** Two findings carry that on their own, and neither is
this wave's code.

`PP18RR8-1` puts a wrong picture on the package's own headline surface and
reports success, so upstream's correct linear line never runs: any browser or
T-line row wide enough to fall to the tiny rung draws `eˣ` as two blanks. The
BINDING fallback rule says an unexpected glyph paints nothing; for one of the
three fonts there is no such thing as an unexpected glyph. `PP18RR8-2` is an
out-of-bounds read in the shipped solo artifact, two keys from `STATUS`, on a
path a design amendment records as closed with arithmetic that is wrong by one
in both of its numbers.

**Where it breaks first, in the order an owner will hit it.** Any integer-mode
formula, on stock defaults, draws its value operands bracketed (`PP18RR8-6`) —
cosmetic, but it is every row. `STATUS` plus one page down reads past the end of
`menu_SYSFL` (`PP18RR8-2`). A polar complex in a formula flips to rectangular
between the stack line and PHIST (`PP18RR8-5`). A wide formula containing `eˣ`
or `r.golden` loses the symbol (`PP18RR8-1`). Then the narrow classes: `LEAD.0`
in base 2 drops the `= result` while ENTER still recalls it (`PP18RR8-3`), a
25-keystroke right-deep chain files a row that can never be drawn again
(`PP18RR8-4`), and the browser's pan sticks on a one-row history
(`PP18RR8-7`).

**What I would leave alone if the goal were correct code rather than a clean
audit.** `PP18RR8-10` — the rule's conclusion is true on all three surfaces; only
the reason attached to it is register-line-specific, and the fix is one
sentence, so it should ride along with whatever next touches §1 rather than pull
a commit of its own. `PP18RR8-7` — the pan is recoverable with EXIT then PHIST,
and it needs a browser holding exactly one row; if the choice is between fixing
the clamp and fixing anything above it, fix the thing above it. All three
PLAUSIBLE items — none reaches the owner, and two of them (the `ppqExpr` lead,
the signature reserve) are worth one comment each stating **why** they are safe,
because both are safe by an arithmetic accident that the next edit can remove
without noticing. `PP18RR8-9` — T28's reach guard is one line and should be
written, but the row does work today, and the more valuable version of that work
is `PP18RR8-8`, whose pin is the sole guard for a shipped fix and whose margin is
one byte. Everything in §4's unverified list, until somebody refutes it: the
three items there with a plausible mechanism (Sol 3, Sol 7, Gemini 2) are each
smaller than the smallest confirmed finding above.

**What must be fixed:** `PP18RR8-1`, `PP18RR8-2`, `PP18RR8-3`, `PP18RR8-5` and
`PP18RR8-8`. `PP18RR8-6` must be either fixed or ruled — and if it is ruled,
`PP18RR7-6`'s deferral has to be rewritten, because the sentence it rests on is
false.

**On the out-of-family pass.** Neither reader produced a new confirmed finding,
and that is a real result rather than an empty one: both were sent at the wave
and the pins, both worked the questions they were given, and the ten items they
raised are coverage claims, unreached boundaries, or facts the packet withheld
from them. The strongest signal from the pass is the agreement — Sol arrived at
the oracle's blindness from the pin net while the in-family dimensions arrived at
it from three defects, which is the same conclusion by two roads. The strongest
warning is D8: the one finding an out-of-family reader ranked first is wrong
because the packet omitted a called function, and that is the packet builder's
defect, not the reader's.

**On the wave itself.** The four repairs are right, the red-first evidence is
real and re-derivable, and the comment pass is genuinely comment-only (verified
independently here by preprocessed hash). This is the best-executed wave of the
series. It still produced findings the same way every previous wave did: five of
this round's ten confirmed findings land on code or documents the wave created,
moved or made reachable — the new pins' blind spots (`-8`, `-9`), the budget it
enlarged (`-3`), the rule it amended (`-10`), and the second call site it added
to a predicate that was already wrong (`-6`). **The other five are wave-0
defects — `PP18RR8-1`, `-2`, `-4`, `-5`, `-7` — and every one of them was
reached by the rotated axis, not by reading the diff.** That is the pattern the
project's own memory records, arriving again on schedule: an axis the audit has
never asked about is where wave-0 bugs survive seven rounds. The confirmed count
since the restart now reads 16, 17, 15, 14, 7, 7, 9, **10**. It has not fallen
since round 5, and it will not fall while every round's subject is the previous
round's fixes.

---

## 8. Round and exit state

**Round 8 of the restarted series.** Readers: eight in-family dimensions
(contracts, lifecycle, arithmetic, errorpaths, guards, tests, design, upstream),
one independent refutation pass per finding with mutation authority, and two
out-of-family readers.

| family | packet | reply | `MODEL:` line, verbatim | raised | survived refutation |
|---|---|---|---|---|---|
| in-family (8 dimensions) | — | — | — | 24 | **13** — 10 CONFIRMED, 3 PLAUSIBLE |
| sol / gpt | `/tmp/pp18-r8/packet-sol-pinnet.md` | `/tmp/pp18-r8/packet-sol-pinnet.sol.reply.md` | `MODEL: GPT-5` | 7 | **0 — none went through the pass** (cap; all seven carried as unverified, §4) |
| gemini / gemini | `/tmp/pp18-r8/packet-gemini-fixwave.md` | `/tmp/pp18-r8/packet-gemini-fixwave.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 3 | **0 — none went through the pass** (cap; all three carried as unverified, §4) |

Both out-of-family replies are present, non-empty and complete, so no
`'pending'` or `'none'` banner is owed. **The round is out of family**, which
the previous seven rounds mostly were not. Zero out-of-family items were
refuted, and zero were confirmed: they were not verified at all, and §4 says so
rather than treating an unread finding as a clean bill.

**Exit criterion: NOT MET.** It asks for two consecutive rounds with no new
CONFIRMED finding, at least one of them out of family on the actual subject.
This round is out of family and has ten confirmed findings, so it fails on the
clean-round half. Clean rounds to date: zero.

**What the next round needs, in order.**

1. **A fix wave for `PP18RR8-1`, `-2`, `-3`, `-5` and `-8`, and a ruling on
   `-6`.** Then a round on that wave, and only after that a round on nothing
   new — the series still cannot close on a round whose subject contains fixes.
   Three of the five fixes are two lines each; `-2` is a build-time assertion;
   `-8` is one assertion in a fixture.
2. **Refutation for the eleven unverified items in §4**, or an explicit ruling
   that closes them. Ten of the eleven came from outside the family, and leaving
   them unread is the same failure mode as not running the pass at all.
3. **The packet rule from D8, before the next out-of-family dispatch:** if a
   packet quotes two callers of a shared function, it must quote the shared
   function. One omission cost this round its highest-ranked out-of-family
   finding and put three more into UNREACHED.
4. **The axis, if the pattern is to be tested rather than assumed:** the
   **cross-surface oracle the suite still does not have** — for one value, does
   the stack line, the T line, the PHIST row and the browser row agree? Three of
   this round's findings answer "no" at three different sites, and every one of
   them is invisible to the filed-versus-live pin the wave just built. The
   second axis worth naming is **the shipped configuration**: the solo build and
   the combined build are two products, the gate tests one and the ASAN pass
   tested the other, and `PP18RR8-2` lived in the gap for two rounds.
5. **Two process items that cost this round real time again:** three verifiers
   ran `./packages/forth-core/build-test.sh` and got a false green in the very
   round that audited the `CLAUDE.md` amendment naming that trap; and eight of
   eight worktrees spawned 143 commits off the audited tip, for the seventh
   consecutive round.
