# Audit — PP18 restarted round 9, COMPLETE: the round-8 fix wave, the PP18RR8-6 ruling and the slop pass, at `2ae202759`

Subject: `7fdda3129..2ae202759` on `pretty-print/stage-pp18`, five commits.

Axis, as dispatched: **the wave's two semantic changes ripple wider than their
pins.** (a) An unknown glyph now fails a run in every font, so rows that painted
blanks now decline — which surfaces and which catalog names change behaviour,
and is every change the BINDING fallback rule's intent? (b) A based integer is
now an atom everywhere — which consumers assumed the bracket? Plus the standing
fix-regression axis over all six repairs, including the new upstream override of
`browsers/flagBrowser.c`.

The six repairs are correct at the sites where they were typed. Every one was
re-derived rather than accepted from the red-first record, and the record holds.
The round's answer to its own axis is that neither semantic change is wrong, and
both are under-covered in the same way: **the wave changed which alphabet is
fatal and which alphabet is an atom, and neither alphabet has an owner.** The
top two findings are the two halves of that sentence. A polar or angle-tagged
value leaf now carries a code point `tinyFont` lacks, so the row declines and
vanishes from the history pager with no placeholder. A based integer draws bare
only when every digit is 0-9, so `10₁₆·2` and `2A₁₆·2` bracket differently on
one screen.

Seven of the nine findings are the fix-regression axis, and six of those sit in
what the wave built to keep the repairs repaired rather than in the repairs.
Nine of nine findings come from this wave's own five commits.

> **This file supersedes** the in-family-half report
> `AUDIT_PP18-restarted-round-9-…-ripple-wider-than-their-pins_2026-08-31-r9.md`,
> which carried the `outOfFamily: 'pending'` banner. Findings `PP18RR9-1` …
> `PP18RR9-8` keep the ids that file gave them, so the owner's notes stay valid.
> The out-of-family pass adds one id, `PP18RR9-OOF-1`, and one independent
> corroboration of `PP18RR9-2`.

> **Filename note.** The dispatched subject string for this round is 1,397 bytes
> and cannot be a filename (255-byte limit). This file keeps the convention
> truncated after the tip. The full subject, the pre-verified facts and the
> fenced ids are stated in §1 and §8.

---

## 1. Subject and coverage

### The five commits

| commit | role in the wave |
|---|---|
| `5209d4685` | docs: the round-8 reports (in-family 1,424 lines, out-of-family completion half 758 lines) plus the DESIGN-HISTORY entry. 2,212 insertions, no code |
| `81b9969a0` | the repair wave: `PP18RR8-1`, `-2`, `-3`, `-5`, `-8`. Pins red-first — 3 red before (T29, T30, T31), 0 after. Adds the package's fourteenth upstream override, `browsers/flagBrowser.c` |
| `8a2775fa5` | docs: the wave recorded, and the A8 arithmetic corrected in `DESIGN.md` §7 |
| `1f7b337b3` | `PP18RR8-6` ruled option A: the fifteen base subscripts `0xa461..0xa46f` join `ppfTextIsAtom`'s two-byte window. T32 red-first, the sole failure |
| `2ae202759` | comment-only slop pass over the wave's authored comments and documents |

Diffstat over the range: 15 files, 3,056 insertions, 48 deletions. The insertion
count is almost entirely the two report files. Behaviour changes live in exactly
three places — `prettyFormula.c` (the staged complex formatter's two arguments,
the `resultRun` decline, the atom window), `prettyLayout.c` (`findGlyphExact`,
the digit-group-space skip) and the new `browsers/flagBrowser.c` override — plus
`prettyTest.c` (201 changed lines).

Verified for this report, not taken from the commit message: `2ae202759` is
comment-only for code. Comment-stripped, whitespace-stripped hashes of
`1f7b337b3` against `2ae202759` match for all four changed C files
(`prettyFormula.c` `31bb0ff9cf2b5cf4`, `prettyLayout.c` `8df451cf21676f5c`,
`prettyTest.c` `86b8326614639601`, `browsers/flagBrowser.c` `dff5732af03d57dd`).
The commit also edits `DESIGN.md`, `DESIGN-HISTORY.md` and two
`cross-model-audit` reference documents, which are prose.

### Out-of-family accounting

Both replies were read in full for this report. Neither is empty, and both carry
a `MODEL:` line, so neither pass is a timeout or an overwrite.

| reader | packet | reply | `MODEL:` line, verbatim | findings raised |
|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r9b/packet-sol-measure.md` | `/tmp/pp18-r9b/packet-sol-measure.sol.reply.md` | `MODEL: GPT-5` | **0** |
| gemini / gemini | `/tmp/pp18-r9b/packet-gemini-r9.md` | `/tmp/pp18-r9b/packet-gemini-r9.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | **4** |

Sol's reply is 72 lines and opens *"No qualifying functional-correctness finding
can be constructed from the supplied packet. The defect ranking is therefore
empty."* It is a measure-and-paint packet about `ppRunInk`, the carve-out and the
decline ripple. It clears the four questions it was asked and then names three
evidence gaps its packet could not close: whether all sixteen window codes exist
as zero-ink glyphs in `numericFont`, what `stringWidth` and `showGlyphCode` do
after `findGlyph` returns `-2`, and the absence of a complete glyph inventory for
the converse question. Those gaps were closed in-family from the generated font
tables, and all three cleared (§6). Its `.err` file is `codex` transcript noise,
not a failure.

Gemini's four findings: (1) hex numerals wrongly rejected as atoms, (2) FB1 is a
false positive that leaks state and skips screen 1, (3) EQ36's fit control has a
2-3 byte slack window, (4) the carve-out unconditionally accepts unknown
space-class glyphs. It also named two gaps its packet could not answer
(`ppfBuildEntry`'s other decode arms, and `ppfStageValFields`), and listed three
items it cleared. Dispositions are in §8.

### In-family coverage

Eight dimensions, blind to each other: contracts, lifecycle, arithmetic,
error/refusal paths, guard reachability, tests-that-cannot-fail, design flaws,
upstream discipline. Every filing then went to an independent refutation pass
with a distinct lens (reachability, correctness, intent).

Read in full by at least one reader: `git log -p 7fdda3129..2ae202759`;
`prettyFormula.c`; `prettyLayout.c`; `browsers/prettyBrowser.c`; the new
`browsers/flagBrowser.c` and its generated patch, byte-diffed against
`src/c47/browsers/flagBrowser.c`; the new `prettyTest.c` rows (T29, T30, T31,
T32, FB1) and the EQ36 rewrite; `DESIGN.md` §1, §2, §3, §5, §6, §7 and the leaf
precedence paragraph; both new `DESIGN-HISTORY.md` entries; `TESTING.md`'s pin
and mutation rules; the `PP18RR8-1/-2/-3/-6/-8` sections of the round-8 report.

Read upstream for the ripple: `fonts.c` (`findGlyph`, `findGlyphExact`),
`charString.c` `_calculateStringWidth`, `screen.c` `showGlyphCode`, `display.c`
(`shortIntegerToDisplayString` all four tries, `addBaseNumber`,
`complex34ToDisplayString2`, `angle34ToDisplayString2`, `real34ToDisplayString`),
`registers.c` `reallocateRegister`, `registers.h` tag macros, `flags.c`
`getSystemFlag`, `error.c`, `softmenus.c` (the `softmenu[]` table and its
terminator), `generateCatalogs.c`, `keyboard.c`'s `CM_FLAG_BROWSER` arms.

Measured rather than assumed: per-font glyph inventories and metrics extracted
from `build.sim/src/ttf2RasterFonts/rasterFontsData.c` (font ids 0/1/2, the 82
codes `standardFont` has and `tinyFont` lacks, the whole `0xa000..0xa00f`
window, every `0xa46x` subscript, the four `ppMetricsInit` probe codes); the
generated `menu_SYSFL` row counts in both build shadows (solo 114, combined 115);
`CAT_SYFL` row counts across upstream and all three packages (112 / 112 / 113 /
114); standard-font pixel widths for every row claimed to exceed a rung gate.

**Not reached by the budget.** No simulator run and no ASAN run this round —
every claim is static reading, generated-artifact measurement, or a gate
mutation. `prettyCapture.c`'s classify and transform tables were traced only on
the tag-storage path. `prettyValue.c` was read at its rung tables and its
`ppMeasure` call sites, not end to end. `prettyEquation.c` has no change in this
range and was read only at `ppfRunPrec`'s caller and the big-operator framing.
The two round-8 report files (2,182 lines together) were read only at the
sections this wave fixed. `design-audit.sh` exists for forth-core only, so no
drift script covers `design-docs/pretty-print/`.

**Worktree hazard, seventh consecutive round.** Every isolated reader spawned at
`e21af8d28`, 148 commits behind the audited tip, where none of the audited code
exists. All of them ran `git log --oneline -1`, detected it, and checked out
`2ae202759` before their first read, as `CODE_AUDIT.md` requires. No reader found
a foreign mutation in its tree, and every tree was verified clean after its
probes.

---

## 2. Mechanical results

**Gate: GREEN at `2ae202759`.** Pre-verified in the dispatch and re-measured
independently five times as mutation baselines
(`./packages/pretty-print/build-test.sh --solo`, 172 s to 310 s, exit 0,
`PRETTY-PRINT GATE GREEN`). One reader also ran the combined pass. No compiler
warning was surfaced by any run.

**Churn scan: clean.** `patch_churn_scan.py` over all fourteen
`packages/pretty-print/patches/*.patch`, re-run for this report:

| patch | adds | dels | hunks |
|---|---|---|---|
| `010-browsers__flagBrowser.c.patch` | 12 | 1 | 1 |
| `010-keyboard.c.patch` | 54 | 3 | 13 |
| `010-solver__equation.c.patch` | 578 | 0 | 5 |
| (eleven others) | 2-38 | 0-10 | 1-5 |

No whitespace-only and no comment-only tier hits. The refresh manifest was
re-hashed against the flat working area by the upstream reader: all 13 files and
14 patches in sync, so there is no stale-patch hazard in this wave.

**Two gate traps, both paid for in reader time this round.**

1. `./packages/forth-core/build-test.sh` configures `CUSTOM_PKG=packages/forth-core`
   and does not compile pretty-print at all. Two readers ran their first
   mutations there and got a meaningless GREEN. The correct gate for this package
   is `./packages/pretty-print/build-test.sh` (`TESTING.md`:12). Every mutation
   reported below was confirmed present in the generated mirror
   (`packages/pretty-print/files/…`) or the build shadow before its result was
   accepted.
2. Worktrees spawn at a stale ref (§1). This is the same runner defect
   `CODE_AUDIT.md` recorded after round 6, and it has now recurred seven rounds
   running.

---

## 3. CONFIRMED findings

Ranked by what the finding costs the owner. Ids `PP18RR9-1` … `-8` are carried
from the in-family half unchanged.

---

### PP18RR9-1 — a value leaf's alphabet was never enumerated against `tinyFont`, and `PP18RR8-5` added two glyphs `tinyFont` lacks

**Where.** `packages/pretty-print/prettyFormula.c:50-55` (the staged complex
arm), against `packages/pretty-print/prettyLayout.c:176-177` and `:201-204`.

**What breaks.** `PP18RR8-5` routes a polar-tagged complex into upstream's polar
spelling. That spelling ends with the angular-unit marker for
`tagAngle == amNone ? currentAngularMode : tagAngle` (`display.c:1889-1893`) — in
RAD, `STD_SUP_BOLD_r` `0x82b3`, in GRAD `STD_SUP_BOLD_g` `0x9d4d`. Both are in
`numericFont` and `standardFont` and **absent from `tinyFont`**. `PP18RR8-1` made
a `tinyFont` miss fatal. So a row that needs the tiny rung now fails
`ppMeasure`, and the whole row declines.

**Reaching input.** POLAR display and RAD mode. `3` `ENTER` `4` `→CPX` (`→CPX` is
unclassified, so the slot goes UNKNOWN — T31's own fixture), then a product wide
enough to fall through rung 0, then `PHIST`. Measured on the running binary at
this tip:

```
PROBE R9 : sig='[P(5. ∡ 0.927 295 22ʳ) · 2]' hasR=1 stdOk=1 stdW=150 tinyOk=0 tinyW=-1
PROBE R9 : tiny glyph 0x82b3 index=-1   0xa221 index=456   0x80b0 index=110
PROBE R9b[mode=RAD]: hasR=1 stdOk=1 stdW=416 limit=392 ; ppfBuildRow canPan=0 noPan=0
PROBE R9b[mode=DEG]: hasR=0 stdOk=1 stdW=415 limit=392 ; ppfBuildRow canPan=1 noPan=1
```

The DEG line is the control that rules out "the row was only too wide". 415 px
against 416 px, both over the 392 px rung-0 limit, and DEG builds at the tiny
rung while RAD does not. The single variable is the suffix glyph.

The same glyph reaches the same place through a plain angle-tagged `real34`:
`ppfFormatStaged`'s `dtReal34` arm passes the tag straight to
`real34ToDisplayString` (`prettyFormula.c:46-48`). That route predates the wave.
`PP18RR8-5` added the first one in this commit series.

**Consequence.** In the CM-20 browser the row prints `(too large to show)`
(`browsers/prettyBrowser.c:84-86`), which is false — the row is not too large, it
contains a glyph the font lacks. In the PHIST pager there is no placeholder at
all: `ppfBuildRow` returns false and the pager `continue`s
(`prettyFormula.c:816` and `:830`), so a history entry disappears from the page.
The value is still recallable, so the browser offers a row the owner cannot read.
Before the wave the same row drew, wrongly, in rectangular form — which is what
`PP18RR8-5` fixed. The two repairs together turn a mis-spelled row into a missing
one.

**Violated contract.** `prettyLayout.c:176-177`, `ppRunInk`'s charter: *"An
unknown glyph fails the run: its metrics were never audited."* The precondition —
every code point a run can contain exists in the run's font — is established for
the VISUAL surface by the package's own sibling walker (`ppvNameIsDrawable`,
ASCII letters only, `prettyVisual.c:270-283`) and by nobody for the formula
surface's value leaves. `PP18RR8-1`'s own remediation proposal covered *"every
`itemCatalogName` reachable as leaf or operator text"*. Value-leaf spellings sit
outside even that sweep.

**Bug class.** *Enumeration without a count check* (catalog), one level up: an
alphabet that a producer can emit, with no artifact that lists it and no owner.
Candidate new class — **a fatal alphabet widened without enumerating its
producers**.

**Class-level test.** Enumerable and small: for every `dataType` a value leaf can
carry, crossed with every register tag (`amNone`, the four angular modes, each
with and without `amPolar`), build the leaf through `ppfFormatStaged` and assert
that every code point in the result resolves with `findGlyphExact` in all three
fonts — or that the design accepts the decline for that cell, stated per cell.
T31 does not catch this: it asserts only that `0xa221` appears, it never measures
at `PP_FONT_TINY`, and it runs in the harness's angular mode, whose `STD_DEGREE`
suffix is in `tinyFont`.

**Verification.** Reachability lens, three refutation attempts, all failed (glyph
present; tiny rung unreachable; decline caused by width). Confirmed with an
instrumented run at the audited tip plus the DEG control above. Two numbers in
the original filing were corrected: the rung-0 limit is `SCREEN_WIDTH - 8` = 392,
and a T31-sized row alone is not wide enough — the class needs a genuinely long
row, which the verifier constructed.

---

### PP18RR9-2 — option A reaches only numerals whose digits are 0-9, so `2A₁₆·2` still brackets while `21₁₆·2` draws bare

**Where.** `packages/pretty-print/prettyFormula.c:123` (the ASCII arm) and
`:133-134` (the widened two-byte window).

**What breaks.** `1f7b337b3` widened `ppfTextIsAtom`'s two-byte window to the
fifteen base subscripts. It did not touch the single-byte arm, which accepts only
`0-9 . , space`. A hexadecimal numeral containing `A`-`F` therefore fails on its
own digit, before the subscript is ever read, and reports `PPF_PREC_ADD`.
`ppfBuildOp2`'s MULT arm then calls `ppfWrapIf(a, 1, 2)` (`prettyFormula.c:226`)
and brackets it.

**Reaching input.** T32's exact fixture with different numbers: `26` `→INT 16`
`ENTER` `2` `×`, then `PHIST`. `shortIntegerToDisplayString` spells the value
from `baseDigits = "0123456789ABCDEF…"` (`c47.c:29`, used at `display.c:2162`)
and `addBaseNumber` appends `0xa461 + base - 2`. The run text is
`'1','A',0xa4,0x6f`. Measured on the running binary at this tip, inside a GREEN
gate run:

```
AUDIT-PROBE R9 sig=[5b 50 28 31 41 a4 6f 29 20 80 b7 20 32 5d ] paren=1
```

which decodes to `[P(1A₁₆) · 2]`. In the same run, T32 asserts that `10₁₆ · 2`
has no `P(`. The two spellings disagree at one tip.

Second instance, same hole. With `determineFont == false` and a spelling wider
than `SCREEN_WIDTH`, the "2nd and last try" substitutes `STD_BINARY_ZERO`
`0xa20e` and `STD_BINARY_ONE` `0xa027` for `'0'` and `'1'`
(`display.c:2360-2377`). Neither is in either accepted window. The plain-digit
try crosses 400 px at about 45 base-2 digits, so any base-2 value of 45 or more
significant bits reaches the fallback alphabet on stock settings. That is the
160-byte spelling the package names as *"the widest reachable spelling"*
(`prettyFormula.c:20-21`) and the exact fixture T30 builds — 160 bytes is only
expressible as 80 two-byte glyphs, so the package's own figure comes from this
branch.

**Consequence.** On one screen, in one base, bracketing depends on which digits
the number happens to contain. `21₁₆·2` draws bare, `2A₁₆·2` draws `(2A₁₆)·2`.
Powers differ the same way: `10₁₆²` against `(2A₁₆)²`. In base 2 the bracketing
flips with WSIZE. The extra `PP_PAREN` also widens the row, so an affected row
can cross `SCREEN_WIDTH - 8` and drop out of the pager entirely.

**Violated contract.** `design-docs/pretty-print/DESIGN.md:587-591`, added by this
wave: ***"A based integer IS a visual atom (ruled, PP18RR8-6, option A): the base
subscript is part of the numeral's spelling, like its digit-group spaces, so
`10₁₆ · 2` draws bare."*** The sentence is unconditional and its example base is
hexadecimal. The code implements it only for numerals whose digits are all 0-9.
The code comment repeats it unqualified (`prettyFormula.c:110-114`, *"a based
integer is one numeral (ruled, PP18RR8-6)"*).

**This is not a re-report of the ruling.** Option A stands. The claim is that the
code covers part of option A's stated domain.

**Bug class.** *A predicate that accumulates alphabets instead of naming its
class* (catalog, PP18 r5/r6), recurring at its own site. The catalog's own hunt
instruction applies: *"Ask which producer KNOWS the fact — usually the one that
formatted the text."* `ppfFormatStaged` knows the leaf is `dtShortInteger` and
therefore a bare based numeral. The predicate re-derives it by scanning glyphs,
and the builder's digit alphabet is width-dependent.

**Class-level test.** For each base 2..16, build a value leaf whose digits include
that base's highest digit and assert no `P(` on both surfaces — fifteen rows,
fully enumerable. Add one wide base-2 row at WSIZE 64 for the substituted
alphabet. T32 drives `0xa469` only, which is the branch where the digits stay
ASCII.

**Verification.** Four readers reached this independently — three in-family
dimensions and Gemini out-of-family, which stated it as its own finding 1. Two
verifiers built probes: one printed the bracketed hex signature above, one built
a base-16 probe row that failed while every other row in the suite stayed green.
Refutation attempts that failed: upstream might not emit a bare ASCII letter on
this path (it does, `display.c:2162`); the leaf might be judged by `ppfRunPrec`,
which exempts names (it is not — `prettyFormula.c:473`, `:498`, `:585`, `:614`
call `ppfTextIsAtom` directly); the MULT arm might not bracket a `PPF_PREC_ADD`
child (it does, `:223-239`). The wide-binary instance is confirmed by arithmetic
over the generated font table and is carried at PLAUSIBLE for its on-screen half
(§4).

---

### PP18RR9-3 — the corrected A8 row reads as closed, and only pretty-print carries the repair for a count both packages carry

**Where.** `design-docs/pretty-print/DESIGN.md:786`, §7 *"Composition claims
(BINDING for other packages)"*.

**What breaks.** `8a2775fa5` corrected A8's arithmetic and, in the same edit,
deleted the sentence that handed the sibling's exposure to its owner. The deleted
text: *"**KNOWN, NOT OURS TO FIX ALONE:** … undo-history's solo build
over-declares, which is a property of the shared-count agreement and needs its
owner."* The replacement: *"That bound is correct in every combination — solo
pretty (114), combined (115), solo undo-history (113)."* The bound expression is
correct in all three. The override that carries it ships only in this package.

**Reaching input.** Build undo-history solo — one of its two gated configurations
(`packages/undo-history/build-test.sh:7-9`). Its `defines.h:1020` carries the
byte-identical `64+51` = 115, and its `items.c` supplies 113 `CAT_SYFL` rows.
`generateCatalogs.c:41-74` emits one `int16_t` per row with the pad-to-six loop
commented out, so `menu_SYSFL` is exactly 113 entries.
`packages/undo-history/patches/` has no `browsers__flagBrowser.c.patch`, so the
upstream loop runs: `fOffset = 60`, `f` up to 54, breaking only when
`f+fOffset > 114`. Press the flag browser key and page to
`SYSTEM_FLAGS_SCREEN_2` (`keyboard.c:4596` steps `currentFlgScr`). The loop reads
`menu_SYSFL[113]` and `[114]` — two entries past the array — and feeds both to
`indexOfItems[]`.

**Consequence.** Two softkey cells painted from whatever follows `menu_SYSFL`,
with `getSystemFlag` masking the garbage silently (`flags.c:283-290`, no error
path). The read is pre-existing and is not pretty-print's alone to fix. What this
wave changed is the record: §7 is the document other packages read, and the only
place that named this now reads as closed. The wave record in DESIGN-HISTORY
enumerates what stays open and does not name it. A grep over `design-docs/` finds
the handoff sentence surviving nowhere except inside the round-8 report's
quotation of the text that was deleted.

**Violated contract.** `DESIGN.md:786`: *"The single `NUMBER_OF_SYSTEM_FLAGS` line
cannot be edited by two packages independently, so BOTH packages carry the
byte-identical `64+51` line and 3-way unifies them (identical-edit claim)."* The
edit that creates the over-read is a shared obligation. The edit that repairs it
is one package's private override, and the row no longer says so. The cause is
also this package's: `design-docs/undo-history/DESIGN.md:181` records the sibling
bumping the count *"reserving one flag for pretty-print's FLAG_PRETTYP"*.
Undo-history supplies 113 rows and would be balanced at 113 alone.

**Bug class.** *Rule corrected in a subset of its copies* (catalog), inverted — a
correction that deleted the true clause together with the false one. Candidate
new class: **the arithmetic was fixed and the open item went out with it.**

**Class-level test.** Not a unit pin. The enumerable check is a build-time
assertion in every package that edits the shared count: assert the walk's bound
equals the generated `menu_SYSFL` length in that configuration. The paste-ready
form is an `UPSTREAM_REPORTS`-style note to undo-history's owner, which is what
the deleted sentence was.

**Verification.** Two readers filed it (design, upstream). The intent lens found
no ruling anywhere that makes the deletion deliberate, and found the project's own
contrary practice of filing sibling defects in this file
(`DESIGN-HISTORY.md:943`, *"FOUND, PRE-EXISTING, NOT OURS … Filed here for the
forth-core/undo-history owners"*). **One half of the filing is refuted and is not
reported:** the closing sentence *"No package supplies a row for a reserved id"*
is not false. "Reserved" there is build-relative, and the sentence states the
`rows <= count` invariant that makes a `numItems` bound safe. Only the *"correct
in every combination"* clause and the deleted handoff are defective.

---

### PP18RR9-4 — FB1, the only test guarding the new override, cannot go red for the defect it names

**Where.** `packages/pretty-print/prettyTest.c:1802-1828`, against
`packages/pretty-print/browsers/flagBrowser.c:281-289`.

**What breaks.** FB1 re-derives the row count with a verbatim copy of the
production scan, asserts `rows >= 61`, drives both system-flag screens, and
checks `lastErrorCode`. The defect it exists to catch sets no error code, so the
row is green before the fix and after it.

**Reaching input (the mutation).** Restore the pre-fix bound
`if(f+fOffset > NUMBER_OF_SYSTEM_FLAGS - 1)` and run the suite. Two verifiers did
this by two different doors — the bound directly, and the fallback arm
`int16_t sysflRows = NUMBER_OF_SYSTEM_FLAGS;` reached by neutering the scan. Both
runs: `testSuite OK`, `Fail: 0`, `PRETTY-PRINT GATE GREEN`. Both confirmed the
mutation reached the built artifact
(`build.sim/custom_pkg_shadow/browsers/flagBrowser.c`).

Positive controls prove the row is live and reaches the walk. With the mutation
in place, an injected `lastErrorCode = 1` at exactly index 114 turns the gate red
with FB1's own text: `prettyPrint test FAIL: FB1 the flag browser raised an error
(expected 0, actual 1)`. A threshold probe gives `FB1 the SYSFL catalog is too
short to fill two screens (expected 61, actual 114)`, which independently
measures `rows` = 114 against `NUMBER_OF_SYSTEM_FLAGS` = 115.

**Consequence.** The one new upstream override in the package is unprotected. A
later refresh, rebase or revert that restores the count-based bound reintroduces
the A8 out-of-bounds read — on a solo build the last tile of system-flags screen
2 shows a row taken from the neighbouring alpha catalog — and the gate stays
green. The documented ASAN configuration cannot cover it either: ASAN runs the
combined build, where 115/115 is exact and there is no overflow to read.

**Violated contract.** The standing bug-fix rule: every fix lands with a
reproducer, a named bug class, and a class-level test where the class is
enumerable (Stan, 2026-08-04). FB1's own failure text claims the browser: *"FB1
the flag browser raised an error."*

**Dissent, recorded.** One refutation pass killed Gemini's version of this finding
on a ruling: the wave record states *"The A8 class test cannot be a
count-equality pin: the count covers RESERVED ids by design … The bound itself
moved to the generated array instead"*, and `81b9969a0` classifies FB1 as
hardening rather than as red-first evidence. That ruling is correct and is a
pre-verified fact of this round. It disclaims **one pin shape**, not every pin.
Two independent verifiers, on reachability, ruled the gap a defect against the
standing rule. The report follows them, and the owner should read the dissent
before acting.

**Bug class.** *Fixture sized from the constant under test* (G2 — both sides move
together) plus *oracle where the mechanism cannot reach* (`getSystemFlag` masks
with `0x3fff` and cannot fail).

**Class-level test.** Assert the derived bound, not the count: require the
browser's `sysflRows` to equal the generated `menu_SYSFL` length (114 solo, 115
combined), or equivalently that the walk's highest index stays inside the array.
That pin goes red under the revert and does not need the impossible count
equality.

---

### PP18RR9-5 — T31 pins the `tagPolar` half of a two-argument fix, and a regression of the other half prints a wrong number under a right-looking unit

**Where.** `packages/pretty-print/prettyTest.c:1794`, against the fix at
`packages/pretty-print/prettyFormula.c:52-54`.

**What breaks.** `81b9969a0` changed two arguments on one call:
`getComplexRegisterAngularMode(TEMP_REGISTER_1)` replaced
`(uint16_t)getRegisterTag(TEMP_REGISTER_1)`, and
`getComplexRegisterPolarMode(...) == amPolar` replaced a literal `false`. T31
asserts that `0xa221` (∡) appears in the signature. `display.c:1543` emits that
glyph inside `if(tagPolar)`, before the angle text is formatted at `:1546`, so
the assertion is a function of `tagPolar` alone.

**Reaching input (the mutation).** Half-revert `tagAngle` only and run the gate:
`testSuite OK`, `Fail: 0`, gate GREEN, T31 passes. The probe captured both
spellings of the same leaf:

```
baseline    : tag=18 sig='[P(5. ∡ 53.130 102°) · 2]'      err=0
half-revert : tag=18 sig='[P(5. ∡ 0.927 295 22°) · 2]'    err=0
```

`0.927295` is the RADIAN magnitude printed with `STD_DEGREE`, because
`display.c:1501` receives the unmasked tag 18 and skips the conversion while
`angle34ToDisplayString2` masks `18 & amAngleMask` = `amDegree` and appends the
degree sign anyway.

**Consequence.** A regression in the argument that carries the angle prints a
silently wrong number under a correct-looking unit, on a PHIST row, with the
suite green.

**Violated contract.** The row's own comment claims the class: *"T31 (PP18RR8-5):
a polar-tagged complex keeps its polar spelling on the formula view."* The
spelling includes the angle and its unit. `TESTING.md:324`: *"A pin that stays
green under its mutation is decoration and gets deleted or fixed."*
`TESTING.md:125-129` already ruled this vacuity shape once, converting T23 and
T26 *"from absence checks to exact-signature assertions"*.

**Bug class.** Presence-check oracle on a glyph that the untested argument does
not control. Sibling of the catalog's *test asserting the absence of one wrong
answer*.

**Class-level test.** The round-8 report's own prescription, unwritten: for each
register tag a value leaf can carry, assert the formula-view spelling EQUALS the
register-line spelling. That pin is the same enumeration `PP18RR9-1` needs.

**Correction to the filing.** The finder traced the fixture's tag as 21
(`amNone|amPolar`) and predicted a `?` plus `displayBugScreen`. The measured tag
is 18 (`amDegree|amPolar`), so there is no bug screen — the regression is quieter
than the filing claimed, which makes it worse rather than better.

---

### PP18RR9-6 — T30 has no positive control and cannot tell its intended decline from a fixture that never built

**Where.** `packages/pretty-print/prettyTest.c:1759` (the assertion), block at
`:1732`.

**What breaks.** T30's whole assertion is
`if(ppfBuildEntry(ppcHistoryEntry(0, NULL, NULL), …)) ppTestFail(…)`.
`ppfBuildEntry`'s first statement is `if(entry == NULL) { return false; }`
(`prettyFormula.c:559-562`), and it also returns false for a short-literal
overflow, a full node stack, a failed `ppfStageValFields` and a failed
`ppfFormatStaged`. Any of those makes the row green.

**Reaching input (the mutation).** Insert `ppcTestReset()` after T30's `CLSTK`, so
the history ring is empty and `ppcHistoryEntry(0, …)` returns NULL. Gate GREEN in
both configurations, zero failures. T30 passed while its fixture built nothing. A
control mutation — deleting the `CLSTK` — turns the row red, which proves the row
is live and that edits reach the artifact through this gate.

**Consequence.** The pin for `PP18RR8-3` stops testing silently after any fixture
drift, and the owner learns nothing until the degrade-instead-of-decline
behaviour ships again.

**Violated contract.** `TESTING.md:555`, the decline catalog: each row asserts its
own D-number, *"so a decline for the WRONG reason is a failure."* The R4-1 record
repaired T23 and T26 for exactly this shape: *"`if(strstr(sig, "7")) fail` is
satisfied by an EMPTY signature, so it passes when capture has stopped working."*
The same commit wrote the opposite standard three hundred lines later, in EQ36:
*"a fit-exactly control must PARSE, so a drift in the pool size or the preamble
cost breaks this block loudly."*

**Bug class.** Negative-only pin with no positive control — the R4-1 class,
recurring.

**Class-level test.** T28 (`prettyTest.c:1643`) is T30's fixture minus
`FLAG_LEAD0` and minus the extra gestures, and it already asserts that
`ppfBuildEntry` SUCCEEDS. T30 needs the same positive half — a
`ppcTestExpectHist(…, 1)` after the `CLSTK`, or a non-LEAD0 build of the
identical gesture chain asserted true.

**Corrections to the filing.** T30 does carry one guard (the `dtShortInteger`
check), and T25/T27/T28 would redden on a systemic filing drift. Neither closes
the hole, because none of them drives T30's extra gestures (double ENTER, the
unclassified `→INT`, two ADDs). The one ruling that could have excused the shape
(`TESTING.md:485`, do not force a state artificially) does not reach it: T30's
state comes from real gestures and was red before the fix.

---

### PP18RR9-7 — T32 sits in the interior of the new window, so an off-by-one at either edge ships green

**Where.** `packages/pretty-print/prettyTest.c:1692`, against
`packages/pretty-print/prettyFormula.c:134`.

**What breaks.** The ruling opened a fifteen-code window, `0xa461..0xa46f`. T32
drives one code, `STD_BASE_10` = `0xa469`, five codes inside each edge. The edges
are base 2 and base 16 — the two bases an integer-mode user lives in.

**Reaching input (the mutation).** `code >= 0xa462` and, separately,
`code <= 0xa46e`. Both shipped GREEN through the full solo gate plus the upstream
suite, against a baseline measured GREEN at the same tip. Both probes were
confirmed present in the regenerated `files/prettyFormula.c` before the run.

**Consequence.** With either edge off by one, `10₂ · 2` or `10₁₆ · 2` keeps the
bracket the ruling just removed while `10₁₀ · 2` draws bare — the
two-surfaces-disagree shape PP18RR6-2 already paid for, with no red row.
`addBaseNumber` (`display.c:1911-1914`) emits `STD_BASE_2` with its low byte
bumped by `base - 2`, so base 2 lands exactly on `0xa461` and base 16 exactly on
`0xa46f`.

**Violated contract.** `DESIGN.md:591`: *"The predicate's accepted two-byte window
is the digit-group spaces plus the fifteen base subscripts."* Fifteen claimed,
one driven, and it is neither boundary. The pin's own comment claims the class:
*"a based integer is one numeral … so an integer value leaf draws without
brackets, on both surfaces."*

**Bug class.** Interior-only coverage of a range. Sibling of *enumeration without
a count check*.

**Class-level test.** Loop bases 2..16, build `10<base> × 2`, assert no `P(` —
fifteen rows. T28 already proves that bases 2 and 16 produce a `dtShortInteger`
leaf, so the input is live. T28 itself cannot see the edge, because its only
assertion is that the entry builds, which holds bracketed or bare.

**Correction to the filing.** Its `FF₁₆` example is a poor witness — `F` fails the
predicate before the subscript is read (that is `PP18RR9-2`). `10₁₆` is the
reaching input at the high edge.

---

### PP18RR9-OOF-1 — EQ36's fit control leaves one byte of preamble slack, and a silenced overflow row passes through trailing-text rejection

**Where.** `packages/pretty-print/prettyTest.c:3427` (the EQ36 block rewritten by
`81b9969a0`).

**Raised by.** Gemini 3.1 Pro (High), out-of-family, its finding 3. Its arithmetic
is corrected here from "2-3 bytes" to exactly one byte, and the drift shape it
imagined is not the reachable one.

**What breaks.** EQ36 brackets the text pool only on the numeral-length axis.
`fit` totals 20 + 490 + 2 = 512 exactly. `over`'s numeral totals 20 + 493 = 513,
one past. The fit-exactly control can only go red when the preamble gets DEARER.
A relative drift of one or two bytes the other way leaves all three rows green
while the `PP18RR7-3` latch goes untested — the `over` row then fails through
trailing-text rejection instead of the latch, which is precisely the silencing
`PP18RR8-8` named.

**Reaching input (the mutation matrix).**

- M1, latch removed (`ppqNumber`'s `c->failed = true`, restoring PP18RR7-3): gate
  RED, sole failure *"EQ36 a numeral vanished into the text pool and the parse
  still succeeded"*. The pin works today.
- M2, latch removed AND `ppNewRun`'s bound loosened by one byte
  (`> PP_TEXT_BYTES + 1`): whole suite GREEN. The shipped defect walks back in
  with every row green.

The reachable drift is not the run-count change Gemini imagined — the preamble is
five identical `1+` pairs, so a tokenisation change moves the count by a multiple
of five, and preamble 15 or 10 makes the `over` row PARSE and redden loudly. The
reachable drift is an off-by-one in `ppNewRun`'s own bound, the function the
fixture's arithmetic is derived from. Silence needs a relative drift of exactly
−1 or −2.

**Consequence.** A one-byte accounting drift silences the only pins on the
`PP18RR7-3` latch while every row stays green. The same M2 edit also lets
`ppText[512]` overrun a `static char ppText[PP_TEXT_BYTES]` by one byte, and the
gate stayed green through that too, because `build.sim` is not
ASAN-instrumented.

**Violated contract.** The block's own comment: *"a drift in the pool size or the
preamble cost breaks this block loudly instead of silencing the overflow rows"*,
repeated in `81b9969a0`'s message. Pool-size drift is absorbed correctly and
upward cost drift is loud. A −1 or −2 accounting drift is silent.

**Bug class.** *Fixture sized from the constant under test* (G2), second
instance — the fixture derives from `PP_TEXT_BYTES` through the same accounting
the subject uses.

**Class-level test.** Assert the bound directly rather than through the parser:
one row that fills `ppText` to exactly `PP_TEXT_BYTES` and one that asks for one
byte more, each asserting `ppNewRun`'s verdict. That pin does not depend on the
preamble's cost. Neither the round-8 report nor DESIGN-HISTORY records the
remaining one-byte margin, so the finding's ask — hardening or a recorded
bound — is unmet anywhere in the tree.

---

### PP18RR9-8 — the new override copies an upstream idiom and the commit names no upstream site

**Where.** `packages/pretty-print/browsers/flagBrowser.c:282`, and the commit
message of `81b9969a0`.

**What breaks.** Nothing at runtime. The hunk adds a linear scan of `softmenu[]`
for `-MNU_SYSFL` to take `numItems`. Upstream solves the same problem the same
way in three places — `softmenus.c:3019-3024`, `:4125-4134`, and, closest,
`fnOpenMenu` at `:1283-1295`, which scans for `-menu` and then takes
`softmenu[i].numItems`, the package's exact use. No commit, doc row,
DESIGN-HISTORY entry or code comment names any of them. A grep for `src/c47/*.c:`
citations over the last sixty commit bodies returns nothing.

**Reaching input.** Not a runtime path. `git log -1 --format=%B 81b9969a0`, plus
`DESIGN.md:804` and `:786`.

**Consequence.** At the next upstream rebase the reviewer of this hunk has no
recorded precedent and must re-derive whether the inline scan was right. The
shape IS right — there is no id-keyed helper in `softmenus.h` (`findMenu` takes a
name), and `MNU_SYSFL` sits at `softmenu[]` index 23, past
`NUMBER_OF_DYNAMIC_SOFTMENUS` (22), so `.numItems` is the static generated row
count. The cost is one reader repeating one search.

**Violated contract.** `.claude/skills/upstream-diff-review/SKILL.md:46-49`,
ground rule 4: *"**Upstream-convention-first (owner standing rule, 2026-08-08).**
When a change to an upstream file is unavoidable, its *shape* follows how
upstream handles the same class at its own sites, **and the commit names that
site**."* The project's own established form is `deliberate-exceptions.md` entry
22, *"Upstream's own convention at `showSoftkey`/`showSoftkey2`"*.

**Bug class.** Missing provenance citation — the inverse of the catalog's
*decorative citation*.

**Class-level test.** Not enumerable in code. The check is the churn scanner's
sibling: every hunk in `patches/` has a named upstream precedent or an entry in
`deliberate-exceptions.md`.

---

## 4. PLAUSIBLE

These survived refutation as claims, and nobody built the input that shows them
on a screen.

1. **The wide base-2 member of `PP18RR9-2` paints rather than declines.** The
   substituted alphabet is reached by arithmetic over the generated
   `standardFont` table: 64 base-2 digits measure 577 px plain (over
   `SCREEN_WIDTH`) and 386 px in binary glyphs. Two such leaves plus the dot
   exceed `prettyFormula.c:769`, so the finder's own 64×64 example is refused on
   width. A 46-bit value measures about 280 px, so `(x₂) · 2` is about 314 px and
   paints WITH brackets. Nobody built that fixture. **Settles it:** a base-2
   fixture at WSIZE 48 whose signature is read for `P(`.
2. **GRAD, `amMultPi` and the D.MS spellings as further members of
   `PP18RR9-1`'s class.** `0x9d4d` is absent from `tinyFont` by the same table
   read, so GRAD behaves like RAD. `STD_SUP_pir` `0xac66` and the D.MS marks were
   not checked at all. **Settles it:** the enumeration `PP18RR9-1`'s class test
   describes.
3. **The pager's two passes can disagree under RAM pressure.** `fnPrettyHist` and
   `pbPaint` build every row twice, and `ppfStageValFields` leaves
   `TEMP_REGISTER_1` at the last row's type. A `reallocateRegister` failure in one
   pass and not the other reserves one height and paints another, so the page
   packing diverges. The precondition is a `RAM_FULL` that the same build
   survives on the second try. **Settles it:** fault injection on
   `reallocateRegister`. Pre-existing, not this wave's.

---

## 5. Design observations

**The value-leaf alphabet has no owner, and now neither does the fatal one.** Two
independent questions have no artifact that answers them: what code points can
`ppfFormatStaged` emit, and which of those does each font carry? The VISUAL
surface answers the first with `ppvNameIsDrawable`. The formula surface answers
neither. `PP18RR9-1` and `PP18RR9-2` are what that costs. One enumeration closes
both.

**The tiny rung is now closed to 151 catalog names.** 82 code points exist in
`standardFont` and not in `tinyFont`, and 151 `itemCatalogName` strings contain
one of them — `eˣ`, the `yˣ` family, every distribution inverse. Those rows now
decline at the shrink rung instead of painting blanks. That is the fallback
rule's intent (§6). It is worth the owner knowing the size of the set, because
the browser's `(too large to show)` placeholder is the message they will see.

**`ppfTextIsAtom` is a predicate with five clauses and no class.** ASCII digits,
radix marks, space, a sixteen-code space window, a fifteen-code subscript window.
Three waves have added clauses. The producer that knows the answer —
`ppfFormatStaged`, which knows the leaf is `dtShortInteger` — reports nothing.

**A composition claim whose edit is shared and whose repair is private.** The
count line is an identical-edit obligation across two packages. The bound that
makes the count safe is one package's override. Nothing forces the second half to
follow the first.

**FB1 derives its expected value from the subject.** The row copies the production
scan verbatim, so both sides move together. The same shape produced G2 twice
before.

**Duplicate pin labels.** `prettyTest.c:1706` (T29, glyph alias) and `:2033` (T29,
browser pan) share a label, extending an existing collision on T23-T28. The
failure strings differ, so the owner can still tell which fired. It does make the
wave's red-first record ambiguous by label alone, and it is the same shape as the
FHIST duplicate label from forth-core round 10.

**Documentation drift, known and new.** `DESIGN.md:50` still names `findGlyph` as
the measure probe, which is the exact function `PP18RR8-1` replaced — a stale
symbol in a parenthetical, fenced by a red-first pin (§6). `DESIGN.md:37` still
says `ppNode_t ppPool[48]` and "max nesting depth 6" against `PP_POOL_NODES` 72
and `PP_MAX_DEPTH` 12 — raised in rounds 1 and 3, KNOWN.

**Hardcoded glyph windows against upstream's macro-derived test.** Upstream's own
base-subscript range test derives its bytes from the `STD_BASE_*` macros
(`charString.c:1135`). The package writes `0xa461..0xa46f` and `0xa000..0xa00f`
as numbers at five sites. The macros are string literals, so the derivation is
ugly at a numeric-code site, and a font renumbering would go wrong with no
compile error.

**The LIT/VAL spelling split.** A literal leaf holds the as-typed `FF#16` and a
value leaf holds `FF₁₆`, so the same number can bracket differently across the two
leaf kinds. The strings genuinely differ and the as-typed rule is §3's own choice.
It is the thing to watch if option A is ever restated as "the leaf's TYPE
decides".

**Two host-side residues.** `generateTestPgms.c:2675` emits
`NUMBER_OF_SYSTEM_FLAGS - 1` as a live flag id, which in a pretty build is 114 =
`FLAG_PTLINE` rather than upstream's last flag. `prettyBrowserEnter`
(`browsers/prettyBrowser.c:216-222`) lifts the stack before `reallocateRegister`,
so a RAM failure leaves the stack lifted and the copy skipped. Both predate this
wave. The second is unruled.

---

## 6. Deliberately not flagged

This section merges what the eight finders cleared with what the refutation pass
killed. Eleven filings were killed. Every one is stated with the reason.

### Axis (a), answered: which surfaces change, and why the change is intent

`findGlyph` returns `-1` for `standardFont` (id 1) and `-2` for `numericFont`
(id 0), and `ppGlyphOf` already mapped both to NULL. Only `tinyFont` (id 2) fell
through to the index-0 alias, which is the `0x0020` space with `rowsGlyph` 0 — it
measured as no ink and painted as a blank. **So the change is tiny-font only**,
and the affected values are those spelled with one of the 82 codes `standardFont`
has and `tinyFont` lacks.

Declining is the ruled outcome. `DESIGN.md:129-131`, BINDING: *"Every pretty path
is a `bool_t` try-function. Any failure — unsupported type, **unexpected glyph**,
pool/text/depth overflow, doesn't fit any rung — paints nothing and returns false,
and upstream's own arm renders unchanged."* Every consumer was traced to a real
fallback: `prettyTryEquation` returns before any paint, `ppqShowRender` always
paints the linear fallback, the VISUAL ladders paint nothing until the fit is
known and `fnPrettyVisual` restores `currentSolverStatus` on every exit, and the
browser reserves `PB_UNSHOWN_H` and prints `(too large to show)`. No stuck state
was constructible. Sol reached the same conclusion independently on the `eˣ` and
`r.golden` cases: *"both changes are intentional … They are not findings."*

The only place the decline is NOT clean is the PHIST pager, which has no
placeholder — and that is `PP18RR9-1`.

### The digit-group-space carve-out — cleared three times, including a mutation

Two in-family findings and Gemini's finding 4 all said the same thing: the
`0xa000..0xa00f` skip in `ppRunInk` exempts codes without asking whether the font
has them, so an unknown glyph is accepted. All three are refuted.

- **The carve-out is the repair, not a hole in it.** `tinyFont` lacks `0xa003`,
  `0xa006`, `0xa008` and `0xa00a`, and `0xa008` is the separator
  `shortIntegerToDisplayString` emits. Without the skip, every grouped numeral
  would decline at the tiny rung. The wave states the rule three times — the
  commit message, the T29 comment (*"Digit-group spaces stay measurable: they are
  space-class in every font"*), and a tiny-font assertion inside T29.
- **Mutation.** Narrowing the window to `… && findGlyphExact(m->font, code) >= 0`
  turned the gate RED with five failures in two batteries, including *"T29 a
  digit-group space must stay measurable in the tiny font"* and three panning rows
  that lose their fixture. Deleting the carve-out outright reproduces the same
  red. The remedy these findings imply reinstates the `PP18RR8-1` regression.
- **The window has no non-space occupant.** `fonts.h:474-480` names seven codes in
  the range and every one is a `STD_SPACE_*`. The other nine have no symbol and no
  producer. `standardFont` carries all seven, so its `-1` box arm cannot miss.
  `numericFont` lacks only `0xa004` and `0xa006`, and neither can enter a
  numeric-font run: numeric-font runs exist only on `prettyValue.c`'s rungs, the
  selectable separators are `0xa008`/`0xa005`/`0xa003`
  (`radioButtonCatalog.c:187-189`), `gapItemFrac` is hard-wired to `0xa005`, and
  the `0xa004`/`0xa006` producers are catalog names, which never reach a numeric
  run.
- **Measure and paint still agree.** `stringWidth` takes the same `findGlyph`
  fallback, and the tiny fallback is glyph 0 with zero ink.

### The other refuted filings

**"The value-staging refusal leaves two globals the error wrote."** Refuted on
reachability, and the finder conceded it was unreached.
`errorMessageRegisterLine` is dead state — every read in the tree sits behind
`lastErrorCode != 0`, and the refusal clears exactly that. `screenUpdatingMode` is
re-armed unconditionally by both live callers before any refresh
(`prettyBrowser.c:142` sets `SCRUPD_AUTO`, the pager ORs the manual bits at
`prettyFormula.c:849-851`). The cited authority is also scoped to `tmpString`, not
to the error globals.

**"T30 clears `FLAG_LEAD0` instead of restoring it."** Refuted by probe. An
inverted assertion proved the flag is CLEAR when T30 is entered, so the
unconditional clear restores the entry value exactly. The only writer anywhere in
`src/` or all three packages is T30 itself, `fnReset` zeroes both flag words, and
no test `.txt` mentions LEAD. The "violated idiom" is also wrong: the file's own
reset helper and T11, T15 and FV16 all normalise unconditionally.

**"`DESIGN.md:50` still names `findGlyph`."** The stale symbol is real and is
recorded in §5. The stated consequence is refuted: restoring the documented call
turns the gate RED with a single failure, *"T29 a glyph tinyFont lacks still
measures"*, whose comment names `PP18RR8-1`. §1's own BINDING fallback rule states
the repaired semantics normatively, so the document does not describe the defect.
The filing's affected-code list was also wrong — `0xa008` never reaches
`ppGlyphOf`, and "82 codes" is the standard-minus-tiny count, not the set
`tinyFont` lacks (106).

**"`ppfBuildEntry` declines on a result tail the caller told it not to build."**
Refuted on intent. The BINDING fallback rule is unconditional, a `PP_NONE` at the
TKRES token is a text-pool overflow, and the fix removed what `PP18RR8-3` named as
the only degrading arm. `withResult` is permission to ATTACH a tail, ruled in
round 3, not permission to build one — and the round-8 refutation pass already
corrected this as a fact of record, because the identical-node-production property
is what makes `ppfTestFiledMatchesLive`'s oracle valid.

**Gemini finding 2's ancillary claims.** Screen 1 is not ambient: `flagBrowser`
assigns `currentFlgScr = init` whenever `calcMode != CM_FLAG_BROWSER`, no earlier
row leaves that mode, and FB1 sets screen 2 explicitly and restores `calcMode` on
the way out. `tmpString` has no restore contract in this codebase — it is a shared
scratch pointer that upstream writes in about 190 places, and no `prettyTest.c`
row reads it across a call. The coverage half of that finding survives as
`PP18RR9-4`.

**Gemini finding 4 and Sol's question (c).** Covered by the carve-out block above.
Sol's three named evidence gaps were closed from the generated font tables:
`numericFont` carries `0xa003`, `0xa005`, `0xa007`, `0xa008`, `0xa00a`;
`findGlyph`'s `-2` reaches `generateNotFoundGlyph`, which cannot be hit because
the two codes `numericFont` lacks have no numeric-run producer; and the full
inventory shows no reachable zero-ink spelling outside the window.

### Cleared by the finders, with the reasoning

**`ppMetricsInit` cannot newly fail.** Its four probe codes (`'0'`, `'-'`, `'('`,
`0xa21a`) are present in all three fonts, so the single global `ppMetOk` latch
cannot flip and disable the feature.

**Big operators survive the tiny rung.** `STD_SUM` `0xa211` is missing from
`tinyFont`, which looked fatal for every sum, product and integral. It is not: the
signs are stroke-drawn (`prettyLayout.c:681-724`), never glyph runs.

**The equation surface is untouched by both changes.** Its accepted classes —
superscripts `0xa160-0xa16b`, `0xa47d`, ASCII letters and digits — are all in
`tinyFont`, and its name alphabet is ASCII, so `SUM(θ;…)` does not even parse.
`prettyVisual`'s `PPA_LIT` text is restricted to `[-]digits[.digits]`, so the
VISUAL surface sees neither semantic change.

**The `PP18RR8-6` ripple into every precedence consumer is the ruling's intent.**
`ppfWrapIf` in the ADD/SUB/MULT arm, `ppfPowBase`, and ITM_CHS were all traced.
`-(10₁₆)` becoming `-10₁₆` and `(2₁₀)³` becoming `2₁₀³` are what option A asks
for. `ppfRunPrec` is called only from `prettyEquation.c:585` on parsed ASCII
source, which can never contain a base subscript.

**The flag-browser hunk itself is correct in every combination.** `softmenu[]` is
zero-terminated at index 186, so the scan cannot run off. `numItems` for
`-MNU_SYSFL` is `sizeof(menu_SYSFL)/sizeof(int16_t)` with the generator's padding
loop commented out, so the bound is the array's true length. `f + fOffset` and
`sysflRows - 1` both promote to `int`, so there is no unsigned underflow.
`menu_SYSFL` is indexed at exactly one site, and the browser is read-only, so no
second consumer holds the old bound. The `sizeof/sizeof` row is entry 023 of a
const flash table, so the `NUMBER_OF_SYSTEM_FLAGS` fallback is unreachable —
which is why it appears in `PP18RR9-4` as an uncovered arm, not as a live defect.

**The complex tag round-trips.** `prettyCapture.c:213` stores the full tag byte,
the filed stream writes the same byte, and `amAngleMask | amPolar` fits five bits.
`getComplexRegisterAngularMode` masks the polar bit off, which the old
`(uint16_t)getRegisterTag(...)` did not, so the fix silently corrected a second
argument as well. The replacement spelling is verbatim upstream's own at
`display.c:3204`.

**EQ36's rewritten arithmetic is exact.** `fit = PP_TEXT_BYTES-23` gives
`20 + (fit+1) + 2` = 512, and `ppNewRun` rejects only on `>`.
`over = PP_TEXT_BYTES-20` overflows by exactly one byte at the numeral, before the
denominator is allocated. `big[]` is `PP_TEXT_BYTES+24` and the longest write ends
at index 507. Three readers checked it to the byte. The third block is redundant
with the second once `c->failed` latches, which is redundancy and not vacuity.
What is missing is the margin record — `PP18RR9-OOF-1`.

**T29's own arms each falsify for their stated reason.** `tinyFont` genuinely
lacks `0xa147` and `0xa008`, `standardFont` genuinely lacks `0xfffe`, and the
middle arm requires `ppMeasure` to SUCCEED, so a dead metrics table fails the
block loudly. The comment says "EVERY font" while the body drives two of three,
and the third (`numericFont`, id 0) was never affected.

**The `eˣ` and `r.golden` enumeration behind `PP18RR8-1` is complete for catalog
names.** A scan of all 84 `fnConstant` names found exactly two with tiny-missing
glyphs, which is what the round-8 report named. `ppcClassify` does reach every
constant (`prettyCapture.c:566` catches `fnConstant` generically, not just
`ITM_CONSTpi`), so the report was right and the first objection to it was wrong.

**Unchecked sibling `ppfRun` pushes in `ppfBuildEntry`** (`:584`, `:613`, `:627`)
are the mirror of the hole `PP18RR8-3` just closed. Every operator arm tests its
operands for `PP_NONE`, so a failed leaf becomes a decline as soon as an operator
consumes it. The only surviving shape is a single-leaf entry, which `ppcEmit`
refuses (*"a bare value is not a formula"*), and `ppMeasure(PP_NONE)` declines the
row anyway. UNREACHED, twice guarded.

**`softmenu_t`'s "numItems must be a multiple of 6" comment** against 114 or 115
SYSFL rows. Upstream is already 112, also not a multiple of six, and the paint
bounds itself explicitly. Stale comment, not a contract.

**The BCD base-10 spelling** (`STD_SUB_b/c/d`, `0xa49d-0xa49f`, and `STD_BASE_1`
`0xa460` one below the window floor) is unreachable: `bcdFlag` needs
`baseOverride == 1` and `ppfFormatStaged` always passes 0.

**`fnPrettyShow` breaks the rung ladder on a measure failure where `ppfBuildRow`
continues.** Every code point in the value alphabet is present in `numericFont`,
so no rung-0 miss is constructible, and the behaviour predates this wave.

**FB1's state teardown** leaves `previousCalcMode`, `lastFlgScr`,
`hourGlassIconEnabled` and `FLAG_ALPHA` dirty and hard-sets `calcMode`, against
the file's save-and-restore idiom elsewhere. No later row reads any of them
without setting it first. Harness hygiene, below the bar.

**Known ids, not re-reported.** `PP18RR8-6`'s ruling itself, `-7`, `-9`, `-10`,
`PP18RR8-OOF-2`, `OOF-1`'s general half and the `PP18RR7-5/-6/-7/-8` dispositions
were treated as ruled. One line to the owner even so: `PP18RR7-6` was deferred to
*"the predicate's next touch"*, `1f7b337b3` IS that touch, and the separator
alphabet was carried into the new code unchanged.

---

## 7. Verdict

The six repairs are correct. Both semantic changes are faithful to the rulings
they cite, the decline is the fallback rule's intent at every surface reached this
round, and the new override is close to ideal on the upstream-diff dimension — one
hunk, one modified line, a virgin file, a bound derived from the generated array,
an idiom upstream uses three times. The churn scan is clean across all fourteen
patches, the refresh manifest is in sync, and the slop pass is comment-only under
an independent strip.

I would not ship this tip as closing round 8.

Where it breaks first is the history pager, in RAD or GRAD mode. A polar or
angle-tagged value leaf now carries a code point `tinyFont` lacks, so a row that
needs the tiny rung declines, and the pager drops it with no placeholder while the
browser still offers it for recall. That is a row the owner loses, created by two
repairs that are each correct.

Second is the hex or binary integer user, who now gets brackets that depend on
which digits the value contains. The ruling's own example base is one of the bases
where it half works.

Third is the sibling: a solo undo-history build still reads two entries past
`menu_SYSFL`, and this wave deleted the only sentence that said so.

**What I would leave alone if the goal were correct code rather than code that
passes an audit.** `PP18RR9-8` entirely — the shape is right, `fnOpenMenu` proves
it, and the cost is one reader repeating one search. `PP18RR9-7` nearly so: both
edge codes come from one constant plus `base - 2`, so a slip at one edge without
the other is unlikely, and the fifteen-row loop is cheaper to write than to argue
about. `PP18RR9-6` is pin hygiene that only matters while T30 is load-bearing for
the exit criterion. `PP18RR9-OOF-1` is a margin the owner may simply record rather
than engineer away. `PP18RR9-3`'s cost inside this package is a paragraph. Its
cost outside it is a real out-of-bounds read with no owner, and that is the half
worth acting on.

The two I would act on today are `PP18RR9-1` and `PP18RR9-2`, and they are one
task: name the alphabets. One enumeration answers both — what the value formatter
can emit, and which of those code points each font has. `PP18RR9-4` is third,
because it is the only repair in the wave with no oracle at all, and its class is
memory safety.

---

## 8. Round and exit state

**Round 9, complete.** Subject `7fdda3129..2ae202759`, five commits, audited tip
`2ae202759`. Readers: eight in-family dimensions (contracts, lifecycle,
arithmetic, error paths, guards, tests, design, upstream), blind to each other,
plus **both out-of-family families**, followed by an independent refutation pass
over every filing.

| reader | packet | reply | `MODEL:` line, verbatim | raised | survived |
|---|---|---|---|---|---|
| sol / gpt | `/tmp/pp18-r9b/packet-sol-measure.md` | `/tmp/pp18-r9b/packet-sol-measure.sol.reply.md` | `MODEL: GPT-5` | 0 | 0 |
| gemini / gemini | `/tmp/pp18-r9b/packet-gemini-r9.md` | `/tmp/pp18-r9b/packet-gemini-r9.gemini.reply.md` | `MODEL: Gemini 3.1 Pro (High)` | 4 | 2 |
| in-family, 8 dimensions | — | — | — | 16 | 11 |

Gemini's two survivors: its finding 1 is the same defect as `PP18RR9-2`, found
independently across two families, and its finding 3 became `PP18RR9-OOF-1` with
its arithmetic corrected from 2-3 bytes to one. Its findings 2 and 4 were refuted
(§6). Sol raised nothing and its pass is not a clean bill — it named three
evidence gaps its packet could not close, all in the font-inventory question where
`PP18RR9-1` lives. Those gaps were closed in-family and cleared.

**Result.** 16 in-family filings reduced to 11 by the refutation pass and merged
to **eight ids** (`PP18RR9-1` … `-8`), plus 4 out-of-family filings reduced to 2,
of which one is new (`PP18RR9-OOF-1`). **Nine CONFIRMED findings.** Three
PLAUSIBLE residues in §4. Ten design observations in §5. Eleven killed filings and
about twenty-five cleared items in §6.

**Fenced ids, unchanged from the dispatch.** `PP18RR8-6` (option A is the owner's
ruling), `PP18RR8-7`, `-9`, `-10`, `PP18RR8-OOF-2` and `OOF-1`'s general half, and
the `PP18RR7-5/-6/-7/-8` dispositions are ruled and are not re-reported. All
`PP18RR1..RR8` and `PP18R4-1..11` ids are KNOWN.

**Pre-verified facts, carried and used.** The gate is GREEN at this tip. The
red-first evidence lives in the two fix commits' messages and was re-derived
rather than assumed. `menu_SYSFL` is sorted by name by the generator, so it is
positionless — which is why `PP18RR9-4` does not claim a wrong flag id, only an
out-of-bounds row. A count-equality pin is impossible by design, because the count
covers reserved ids, which is why `PP18RR9-4`'s class test asserts the derived
bound instead.

**Exit criterion: NOT met.** The rule is two consecutive rounds with no new
CONFIRMED finding, at least one of them out-of-family. This round has a full
three-family reader set and nine CONFIRMED findings, so **a real finding resets
the count**. The counter stays at zero. The base is still unpushed.

**Runner defects to fix before round 10.** Worktrees spawned 148 commits behind
the audited tip for the seventh consecutive round, and the brief must keep naming
`./packages/pretty-print/build-test.sh` — the forth-core gate does not compile
this package, and it returned two vacuous GREEN runs this round.
