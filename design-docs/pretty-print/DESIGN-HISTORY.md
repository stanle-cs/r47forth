# Pretty-print package — design history

Non-normative amendment trail. DESIGN.md is authoritative.

## 2026-08-28 — PP18 audit round 3: pin vacuity, and a class with three producers

Round 3 took round 2's fixes as its subject, on the axis neither earlier
round had asked: (a) which pins would still pass if the behaviour they
claim to test were deleted, and (b) round 2 had changed a SHIPPED
surface — `ppfBigop` — to fix a defect found in a different component,
so was that change right everywhere? Seven confirmed, four refuted.

**Both worst findings were round 2's repairs.** `ppvDerivative` passed
the INVENTED name to `ppvBody` with `synthetic = false`, so the shadow
guard that makes invention safe never armed for a derivative and a body
recalling a real `n` was drawn as the counter — the exact confusion V6
and V71 forbid for sums, reintroduced at the one caller that did not say
so. And round 2's own PP18R2-3 repair had added drawability as a
conjunct to the `first` capture, so the mirror began picking the first
DRAWABLE declaration instead of the first: it stopped mirroring the walk
it is named after, and the picture named a variable upstream never
varies. Drawability is a property of the name we END UP with, judged
once, at the end.

**That is three rounds in which the worst finding came from the previous
round's fixes**, and the rate has not fallen — which is what
CODE_AUDIT.md says happens and why the exit criterion refuses to close
on a round containing fixes.

**Axis (b) found the class had THREE producers.** The walker (PP18-4),
the capture engine (PP18R2-2) and the EQN parser (PP18R3-3) all draw a
big operator as an operand, and the parser has no precedence value
anywhere to correct, which is why the one-line repair could not reach
it: `SUM(X;X;1;3)^2` drew the picture of 14 for an equation EQCALC
returns 36. There the node KIND decides. DESIGN-HISTORY's own claim of
"two sites" was wrong and is corrected above. **A class fixed at the
site where it was noticed is a class fixed once.**

**Axis (a) verdict, and it is not the comfortable one.** No individual
pin was vacuous — every deletion was named and checked. The BATTERY was:
V65's five programs miss both of this round's regressions by one letter.
A battery of non-vacuous pins can still be a battery that tests the
wrong five things.

Also fixed: a construct nested in another construct's LIMIT could be
given the same counter name (PP18R2-4 was right that a closed sibling is
reusable and wrong to stop looking at constructs at all — the
distinction is "about to be drawn inside me", not "already built"); the
mirror's name-length bound; and two namespace collisions the wave
introduced — a second pin named `B9`, and three audit tags reused from
an earlier round, now `PP18R2-*`.

## 2026-08-28 — PP18 audit round 2: the fixes audited, and one was a regression

Round 2 took the FIX COMMITS as its subject, with the question axis
rotated off round 1's: (a) the fixes touched code SHARED with the capture
engine and the EQN surfaces — did they disturb the neighbours? and (b)
the fixes ADDED refusals — are those honest? Eight confirmed, four
refuted.

**Axis (a) answered clean and mechanically: the neighbours were not
disturbed — one was left behind.** (Round 3 corrected this: there were
THREE producers of the shape, not two. The EQN parser is the third, and
it has no precedence value to fix, so the node kind decides there.) `ppfBigop`, the capture engine's
big-operator builder, still reported ATOM, so a captured or replayed sum
took no brackets in PSHOW and PHIST while VISUAL bracketed it. PP18-4
had been fixed at one of the two sites that share the defect. **Axis (b)
did not answer clean: two of three new refusals refused programs that
should draw.**

**PP18R2-1 is the one that matters, and it is my fix, not the original
code.** PP18-1's repair declined every derivative over a body declaring
no MVAR, reasoning that upstream varies nothing there. `fnFillStack` is
UNCONDITIONAL — only the `STO` into the named variable is guarded — so
the ordinary stack-consuming RPN function body is differentiated
correctly, returns 6, and PP17 drew it. The fix refused it. The body
reads its argument positionally exactly as a SUM body does, so the
picture now invents a name, which is what SUM has always done.

**And V65 would have caught it in one line.** V65 is the differential
oracle — run the program, evaluate the walker's own drawing, require
agreement — recommended by round 1, recorded as delivered in the commit
message, DESIGN-HISTORY and TESTING.md, and **never written**. It was
lost when a mutation runner reverted the tree mid-fix, and because only
prose referred to it, nothing failed and nobody looked. It is written
now, over five programs, and it caught PP18R2-1 on its first run. **A pin
that exists only in prose is worse than a missing one: it stops anyone
looking for the gap.**

Also fixed: the MVAR mirror aborting the whole picture over the
drawability of a declaration it was not going to use (PP18R2-3); the counter
pool spent on CLOSED sibling scopes, so a fifth disjoint sum declined
with nothing to collide with (PP18R2-4); PP18-5 fixed at three arms and
pinned at one (PP18R2-6); and round 1's PP18-8, skipped at the time, which
turned out to be what made PP18R2-4's fixture exhaust the arena — the body
seeding allocated eight nodes where its own comment said one.

**Two documentation failures worth naming.** DESIGN.md still taught the
retracted seeding rule as a measurement (PP18R2-7) — the authoritative file
teaching the thing the code had already stopped doing. And the PP18-16
correction had been pasted in FRONT of the sentence it was replacing, so
TESTING.md asserted both readings at once (PP18R2-8). **A correction that
does not delete what it corrects is not a correction**, and the stale
sentence reads the more confident of the two.

Exit criterion still NOT met: this round found real defects, so the count
resets again, and the rule against closing on a round that contains fixes
stands.

## 2026-08-28 — PP18 audit round 1: sixteen findings, and the one that lied

A cross-model round over the five PP18 commits: eight blind finder
dimensions, an independent refutation pass per finding, 33 agents.
Sixteen CONFIRMED, one refuted. The refactor's central claim held —
precedence is settled once, the drawing is byte-identical, 45 pins
survived unedited — and the report says so. Everything of consequence
was somewhere else.

**The worst finding was a picture that lied, and it was mine.** I wrote
that DERIV's seeding rule was "a measured claim, not an analogy" and
then measured one call short. `_differentiatorIteration` does store the
sample into `variable`; `calcDeriv` gets `variable` from
`deriv_pgm_variable(label)`, which reads the BODY program's own MVAR
declarations. A body declaring no MVAR returns 0 while VISUAL drew a
picture meaning 6. Both halves were executed in the real binary. The
class name is **measured one call short** — an invariant verified at the
callee and assumed at the caller — and its companion, **the only fixture
satisfies the assumption the code never checks**: my single DERIV body
declared `MVAR 'x'` and was driven by `f' 'x'`, the one configuration in
which the two channels agree, so five pins passed a rule that held only
for them.

**Three more were one sentence.** PP17's text back end had been carrying
three implicit bounds — a rolled-back pool, a 255-byte fragment cap, and
a guaranteed linear line of last resort. PP18 deleted the text and all
three went with it, and I replaced none, because every pin still passed.
The consequences were a hang (the ENTER DAG re-expanded 2^k with no
abort, because a PP_NONE return was a per-call value and not a latch), a
blank framed screen with the answer erased and no error, and one
unchecked construct operand. **Deleting a representation deletes the
bounds it happened to enforce, and a green suite will not mention it.**

**A guard that enumerated its examples instead of its class.** PP18-4:
constructs reported ATOM precedence, so a Σ under a square drew the
exponent on the Σ's body — two programs whose answers differ by 2.6x
drawing the same picture. The guard for stacked powers forty lines
above had found exactly this class and listed the two OP1 members in
front of it. Fixing it also drew the line the fix needed: a construct as
an OPERAND needs brackets because nothing terminates its body; a
construct as a BODY does not, because the outer construct's own " d<var>"
does, which is why nested integrals are written without them.

Also fixed: the lift latch surviving XEQ/PGMINT/PGMDRV (a false decline
at top level, a silent wrong drawing inside a body), `varOff` a uint8_t
indexing a 512-byte pool (observed misfiring during another finding's
verification), and the invented Σ counter colliding with a free variable
of the same name — **a scope rule implemented where it was noticed
rather than over the scope it names**.

**Two process notes.** The hang's fix has two redundant guards, so
neither mutates red alone; only removing both reproduces the shipped
shape. Recorded as redundancy rather than credited as two coverage
holes. And V66 asserts a visit COUNT, because a timing pin passes on a
desktop for a program that hangs the calculator.

The oracle the report recommended is now V65, and it is the shape that
would have caught PP18-1 without anyone thinking of derivatives at all:
run the program, evaluate the walker's own drawing, require them to
agree. No expected string appears in it.

## 2026-08-28 — PP18: the walker stops writing strings, and gains DERIV

PP17 drew by transpiling a program to equation-language text and parsing
it back. Stan asked why the walker needs to emit text at all, and the
answer did not survive the question: it was settling precedence TWICE —
inserting brackets into a string, then having the parser read them back
out to rediscover the same structure — while `ppfCombine1`/`ppfCombine2`
had been doing exactly that job for the capture engine the whole time. I
had rebuilt an existing component in string form without noticing, and
only found it by going to look when asked.

The walker now builds an expression tree and lays it out through the
shared builders. **The drawing is byte-identical to PP17's** — same
DBLINT screenshot, `cmp` clean, produced by a completely different path.
That, plus 45 pins passing unedited through the representation change,
is what makes the refactor verifiable rather than merely plausible.

Three things came free with the round trip gone:

- **A failure class stopped existing.** The emitted alphabet had to be
  byte-exact or the whole formula silently dropped to a linear line —
  BINDING, with MUT-106 as its guard. A node cannot be mis-spelled. The
  rule is recorded as retired rather than deleted, because the reasoning
  is what argues against going back.
- **The linear fallback became unnecessary.** It existed because
  `ppqParse` could decline. Tree-to-nodes cannot.
- **The text grammar left the drawing path.** VISUAL now reaches
  `ppfCombine`, `ppfWrapIf`, `ppqBuildBigop` and the layout primitives,
  and nothing else — which is what makes draw-only a small thing to lift
  out, and was the whole point of the upstream reader's objection.

The device build earned its place in the loop: it caught `ppvRun` being
swallowed into the PC_BUILD guard around the test back end, which the
simulator build cannot see because both halves compile there. The flash
measurement is a build test as much as a size test.

DERIV landed on top, and the deferral that PP17 recorded turned out to
cost one read: `_differentiatorIteration` fills every stack level with
the sample point AND stores it into the named variable — *"feed both
channels"* — exactly as `DEI_xeq_user` does for an integral. The seeding
rule was the one already in use. PGMDRV gets its own latch because
upstream gives it one on purpose, and V55 pins that a derivative does not
read PGMINT's.

**Two places the node form is not the text form**, and both are
improvements: a fraction bar scopes, so `a/(b+c)` draws with no
parentheses; and a stacked power needs its base bracketed, which the
walker does locally rather than adding a POW level to `ppfCombine` and
changing that contract underneath the capture engine.

**V18 stopped grading its own homework.** It evaluated a string typed in
the test file, which agreed with the expectation typed beside it whether
or not either was right. It now evaluates the walker's own output.

## 2026-08-28 — PP17: VISUAL, an RPN program drawn as its mathematics

Jaymos, replying to the v0.1 announcement, asked for the one thing the
package could not do: draw what R47 itself computes — a chained RPN
program from appnote 22, `VISUAL 'DBLINT'` beside `XEQ 'DBLINT'`. It
lands as a third front-end to the existing renderer (DESIGN.md §6,
PP17), a static walker that transpiles program steps into
equation-language text.

- The design decision that made it small: **emit TEXT, not nodes**. The
  renderer, the evaluator and the dismissal protocol are all reused
  unchanged, and the output is a string the user could have typed into
  EQN — so V18 can assert that a transpiled double integral EVALUATES to
  4/3, which no string comparison could have caught.
- 18 mutations (MUT-77..MUT-94), all red-verified.

**A reported bug that did not exist, and the pin that proved it.** This
stage was planned with a rider: "PSHOW/PHIST render a recalled named
variable as `R256`", read out of `prettyFormula.c`'s two `R%02u` arms.
The pin written to red-first it came back
`expected '2 q ×', actual '2 # ×'` — a VALUE leaf, not a mis-named RCL
leaf. `ppcRclLeaf` (prettyCapture.c:234) only builds a `PPN_RCL` leaf for
`param <= 99`; everything else, named variables included, degrades to a
value snapshot by the PP9 ruling ("their display names are not item
ids"). So the `R%02u` arms only ever see numbered registers and are
correct as written, and the "fix" was unreachable code. Reverted.

What survives is a smaller, real observation for whoever wants it: PP9's
stated REASON has expired — `ppfVariableName` now exists and does exactly
that decode for the big operators' d-variable. Whether a named recall
should show `q` or its value at capture time is a design question (the
value is truthful too), not a bug, and it is left open rather than
decided in passing. This is the same shape as R5-1, where a binding
rule's justification was invalidated without the rule itself becoming
wrong.

**Two harness false-greens in one session**, both written up in
TESTING.md: the suite's failure banner is `1 TEST  FAILED` in the
singular, and only package-OWN files are symlinked into the build shadow
— mutations in patched upstream files need
`meson setup --reconfigure` or they test yesterday's code. MUT-87
"survived" against a stale `softmenus.c`. Both were caught by disbelieving
a green, which is the only way this class ever is.

**Scope widened, and the renderer with it.** The first cut emitted only
the four monadics with a 2D spelling, on the reasoning that `ppqPrimary`
had no function-application arm so `sin(x)` could not be drawn. Right
about `sin(x)`, wrong about everything around it: the strict parser
failed on the trailing `(`, so one unrecognised name cost the whole
formula its 2D form — `sin(x)/2` lost its stacked fraction. The renderer
gained an f(x) arm for the CONTEXT, not the function, and deliberately
does not set `fracSeen` for it, since a drawn `SIN(x)` is the same shape
as a linear one.

The emitter needed a name source that cannot drift from the evaluator,
and got one without a hand table: `ppEqFunctionItem` mirrors
`_parseWord`'s own resolution, and an item is emitted only if its
catalog spelling ROUND-TRIPS back to that item. Admitted: LN, LOG, SIN,
COS, TAN, ARCSIN, ARCCOS, ARCTAN. Refused, correctly and without being
listed anywhere: `e^x`, `10ˣ`, `LN(1+x)`, `>ABS<`, `|x|`. The scope had
been narrow partly for a real reason and partly because a decision made
before the linear fallback existed was never revisited — the same shape
as the placement miss below.

**Coverage went from "fixtures I wrote" to "input the unit produces."**
Asked for more test functions and for driving through the keyboard, the
battery gained nine fixtures (a constructed function under an integral,
a serial XEQ chain, the PROD arm, a computed limit, the lift latch, the
stack motions) and two axes it had not had at all. V36/V37 drive VISUAL
through the transient-alpha path its `TM_LBLONLY` parameter exists for —
the command, ALPHA, the label typed a letter at a time, ENTER — which
every other pin skipped, and which is the axis R5-3 was found on. V39
goes further and KEYS THE PROGRAM IN through PEM rather than loading a
byte array, which answered a question the suite had been assuming: PEM's
literal encoding came out `72 08 01 32`, byte-for-byte what the
hand-encoded fixtures spell. The guess was right; it is now verified.

Two harness facts fell out. `getNumberOfSteps()` is the CURRENT
PROGRAM's count, so keying from there splices new steps into whatever
program is current — the first attempt built its program inside another
one, and the walk correctly ran on into that program's `XEQ 09` and
declined. And `fnGotoDot` does not clamp: a step number past the end
walks `currentStep` to NULL and cores the suite. Both are in TESTING.md.

**The placement was in the request and I nearly shipped without it.**
Jaymos wrote "draw the integrals in the Z/T window". The first
implementation used the existing full-screen surface instead, because it
was already there — and the earlier analysis had even flagged Z/T as an
open question to measure, then let the design quietly resolve it by
convenience. Asked to re-read the request, the gap was obvious.
Measuring settled it in one run: one stack line is 36 px and the
transpiled forms need 38/58/78 (31/51/71 shrunk), so a single line
cannot hold the double integral he named — but the T and Z bands
TOGETHER are 72 px and hold every chain in appnote 22. "Z/T window" was
a region, not a line, and reading it as a line was what made it look
impossible. The lesson is not about integrals: when a request names a
place, the place is part of the requirement, and substituting a
different one needs saying out loud, not deciding silently.

**MUT-79 is the one worth remembering for coverage.** Dropping the
right-operand parenthesization rule survived V16 (`a/(b+c)`), because a
LOWER-precedence operand parenthesizes under either rule. Only the
EQUAL-precedence case distinguishes them — `a-(b+c)` becoming `a-b+c` —
and V21/V22 had to be written for it. A precedence pin that does not
include a same-level right operand is not testing the rule it looks like
it is testing.

## 2026-08-27 — two owner-reported bugs: EXIT, and lowercase constructs

Stan, using the build: "in eqshw pressing exit did not exit correctly,
had to press <- key for it" and "lowercase integ/deriv etc. does not
work." Both real, both fixed red-first, gate green solo and combined.
Flash 1,146,336 → 1,146,432 (+96 bytes); BSS unchanged, device RAM
12,908 of 16,384.

**EXIT.** The full-screen surfaces paint their own pixels with the
fnPixel protocol (`screenHoldsDrawnPixels` + the SCRUPD manual bits) and
left `temporaryInformation` alone. Upstream's EXIT arm
(keyboard.c:2533) dismisses a held screen only when
`temporaryInformation != TI_NO_INFO || showScreenDismissed`, and
`showScreenDismissed` is latched at btnPressed (keyboard.c:1808) from
SHOWMODE, which is itself a temporaryInformation test. Both terms were
false, so EXIT fell through to the menu arm and the picture stayed up.
BACKSPACE worked only because its own arm refreshes unconditionally,
which is exactly why the defect presented as EQSHW-specific.

The remedy is upstream's own: its matrix SHOW paints its screen and then
sets `TI_SHOWNOTHING` — "then tell the system it is in show nothing
mode" (display.c:3952). One assignment at each surface. It also settles
the dismissal semantics for free, because the whole machine already
agrees on what a show screen is: any key dismisses (processKeyAction's
TI clear), EXIT dismisses without popping the menu underneath, and
`clearTamBuffer` leaves a self-painted screen alone (screen.c:5579).

PSHOW carried the SAME defect and nobody reported it, because its
fallback arm calls the real `fnC47Show`, which sets temporaryInformation
properly — so PSHOW dismissed correctly on exactly the inputs it could
not pretty, and stuck on the ones it could. S4 had pinned the fallback
half since PP2 (`"S4 fallback did not reach SHOW"`), which is the pin
that should have made the asymmetry visible and did not, because it only
ever asserted the fallback.

**Lowercase.** Two defects, not one. Both name matchers used a
case-sensitive `strncmp`, and the renderer had a SECOND gate — it
attempted a construct only when the first character was `A`-`Z`
(`ppqPrimary`), so fixing the comparison alone left the renderer
declining input the evaluator computed. Verified by the gate: the eval
pins went green while `EQ34 lowercase construct does not render` stayed
red.

Upstream convention decides the spelling rule. `functionAlias[]` carries
both spellings of any name a user types — "sinh" beside "SINH", "asinh"
beside "ASINH" (solver/equation.c:41-44) — because CMP_NAME folds
superscript, subscript and struck forms but never case (sort.c:137).
So the constructs answer to their all-upper and all-lower spellings and
nothing else; mixed case is upstream's own no, and EQ34 pins that
`Sum(` is declined so the rule stays a ruling rather than an artefact of
how the comparison happens to be written. The test extends to
`ppEqConstructIs` in prettyPrint.h, one function both parsers call, so
the two cannot drift again.

Both classes are in the audit catalog: "a self-painted screen that never
declared itself one" and "one grammar, two matchers, drifted".

**What the process missed, and why.** Five audit rounds, a screenshot
review and two out-of-family readers never asked how a screen ENDS —
every question was whether it drew correctly, and a capture cannot show
that a key does nothing. The lowercase pair is worse: FACTS.md recorded
"variable names may be lowercase, and lowercase evaluates identically"
from a verification that only ever exercised variables, and that
sentence then read as coverage of a case rule nobody had tested. A pin
covering the neighbouring half is how both survived.

FV20 is deliberately a CONTRACT pin. Upstream's EXIT arm lives in
keyboard.c's static `processKeyAction`, reached only from GTK button
events the testSuite binary cannot raise, so the pin asserts the state
the surface must leave behind for that arm to fire, which is the exact
term that was missing. The keypress itself remains unexercised by any
automated test.

## 2026-08-26 — PP16: the three deferred items, closed

Stan asked why anything was still deferred and told me to finish them.
All three were real, and one of them was a genuine capability gap
rather than a cosmetic one.

- **Complex results in the constructs.** `SUM`/`PROD` accumulated in a
  single real, so a complex term was refused. Upstream's own
  `_programmableSumProd` latches over to complex the moment a term has
  an imaginary part — gated on FL_CPXRES, and a domain error when that
  is clear. The package now does the same, with complex multiply
  written out for PROD. DERIV/INTEG still refuse complex, and that is
  CORRECT rather than deferred: upstream's differentiator and
  integrator have no complex handling whatsoever (zero `dtComplex34`
  references in either file), so refusing matches the built-in
  behaviour exactly.
- **The early-stop sum.** `fnProgrammableSumInf` IS in the shipped ARM
  binary (checked the map file), so Σ∞ exists on the target and was
  simply falling to the default rule and invalidating the shadow —
  truthful, but nothing shown. It reads the same three stack levels,
  so it joins the existing class in one line. Guarded on
  OPTION_INFSUMS: without the option, item 2755 is an unimplemented
  stub that moves no stack, and capturing it would mint a node for an
  operation that never happened.
- **The softkey state indicators.** The generic checkbox path reads the
  flag out of the item's `param` but only fires for `fnGetSystemFlag`
  items inside `-MNU_TAMFLAG`. Rather than repurpose our toggle items'
  param to suit it — which would have put a value above NOPARAM into a
  field the dispatcher reads — the package carries its own branch that
  reads the two flags explicitly. Contained, and nothing in dispatch
  is perturbed.

Two process notes worth keeping. MUT-60 came back GREEN on its first
run: the fix had been verified by screenshot only, which is precisely
the "a pin green under its mutation is decoration" rule catching an
unpinned change — FV17 exists because of it. And I then misread my own
failure message and briefly believed FV17's margin was 1 pixel; it is
33 (291 filled vs 258 outline). Measure before concluding, including
when the thing being measured is your own test.

## 2026-08-26 — PP15: both toggles are flags, and the menus exist

Stan ruled that PPON and PTLIN should both be flags, asked for a home
for PSHOW/PHIST/PCLR, and asked whether EQSHW was in the EQN menu (it
was not — the package had never touched a menu).

Built as one stage because all three touch the same files:

- **FLAG_PTLINE (51, 0x8072)** replaces the `ppTlineActive` bool. The
  count line goes to `64+51` byte-identically in BOTH packages (the
  identical-edit claim), and the `PTLINE` SYSFL catalog row lands at
  free row 2301 in UNDO-HISTORY's items.c beside `PPRTY` — the
  touching-line rule, same as PP11.
- **MNU_PP** claims free item row 217, adjacent to our own 215/216, as
  a `CAT_MENU` row (forth-core's precedent at row 213). Its six-key
  menu is `PSHOW PHIST PCLR EQSHW PPON PTLIN`, hung off the earliest
  free slot in `menu_DISP` (row 5) and registered at the softmenu
  table's TAIL per that table's own instruction — ~160 lines clear of
  forth-core's mid-table insertion. `EQSHW` also goes into `menu_EQN`'s
  first free slot. Both parent menus are untouched by both siblings
  (verified by diff); the STACK menu was deliberately avoided because
  undo-history took three of its four free slots and edited that exact
  line.
- **The stage found a real latent bug in PP11.** FV14 failed on its
  first run — a reset was not leaving the T line off — and the cause
  generalised: `prettyReset()` was wired to five LAZY-INIT sites as
  well as to `doFnReset`, so it conflated "initialise my data" with
  "restore factory defaults". The first dispatch after a cold start
  therefore force-set FLAG_PRETTYP, silently overwriting a preference
  the user had saved — destroying the persistence PP11 was built to
  provide. Split into `ppcInit()` (data only) and `prettyReset()`
  (data + both flag defaults); FV16 and MUT-58 pin it.
- Deliberately NOT done, with the mechanism recorded so it is cheap
  later: the PPON/PTLIN softkeys do not show their flag state the way
  their DISP neighbours do. The checkbox render reads
  `getSystemFlag(indexOfItems[item].param)` but only for items whose
  func is `fnGetSystemFlag` AND only inside `-MNU_TAMFLAG`
  (softmenus.c:3471). Wiring it would mean carrying the flag number as
  our toggle items' param and widening that condition — real surgery
  in a shared file for a cosmetic gain, so it waits for a ruling.

Also ruled and now documented: EQSHW and PSHOW deliberately IGNORE
FLAG_PRETTYP. The flag governs what the calculator does by itself; an
explicit "show me this" should work regardless. That was implicit
before and is now written down.

## 2026-08-26 — the separator, challenged and verified (round 8)

Stan challenged the `;` separator: had I checked it was right, why not
`,`, and was the shape even in upstream's convention? Two of three
answers held up; the third exposed a risk I had not considered and an
untested gap.

- **`,` is genuinely unavailable.** The parser rewrites every comma
  inside a number to `.` (equation.c:1191) — comma IS the radix mark.
  A comma-separated list would be silently ambiguous, not merely ugly.
  This was the one thing I had reasoned correctly at design time.
- **There is no upstream convention, because the feature does not
  exist.** `MAX`, `MIN` and `atan2` sit in the equation alias table,
  but all eight plausible call shapes fail, most with
  ERROR_ITEM_TO_BE_CODED. So I did not deviate from house style; there
  was no house style. What I had NOT done at design time was check —
  I verified `;` was free and stopped there, which is half the
  upstream-convention rule.
- **The risk that follows:** "to be coded" is intent. Upstream will
  choose a separator eventually, and if it is not `;` this package
  speaks a dialect on their own machine. That question now belongs in
  the upstream conversation alongside the derivative report.
- **Typeability, which I had never tested at all.** Every EQ test
  builds its equation from C. Nobody had shown a user could enter one.
  Three probe rounds failed before the cause surfaced:
  `reallyRunFunction` passes the CALLER's param, so driving a character
  item through it calls `addItemToBuffer(NOPARAM)` — a bug-screen path
  that inserts nothing. keyboard.c calls `addItemToBuffer(item)`
  directly. Driven that way it works: `;` lives in the ALPHA
  punctuation menu and inserts fine, and EQ29 now types
  `SUM(X;X;1;3)` key by key, commits it the way ENTER does, and
  evaluates it to 6.
- **Bug 1 was worse than round 5 recorded.** The commit path ENTER runs
  is `setEquation` followed by the MVAR variable-hunting parse, and a
  failure there bounces the user back into the editor with the old text
  restored. So before the fix, a typed construct could not be SAVED at
  all — not merely mis-evaluated under a derivative. MUT-56 reproduces
  it and turns EQ29 red with syntax error 45.

Lesson, and it is the same one as rounds 4-7 wearing another hat: the
half of a rule that gets skipped is the half that needs a tool run
against it. "Is `;` free?" was answerable by reading and I read it.
"Does upstream have a convention?" and "can a user type this?" were
answerable only by running something, and I did neither until asked.

## 2026-08-26 — the appnotes, and the remedy that was there all along (round 7)

Stan asked whether the project's application notes covered the
derivative behaviour. Reading them (docs/appnotes, 30 notes; sources
are .docx/.odt zips, so the prose extracts without a PDF tool)
answered a different and better question.

What the notes say: **AN0011** documents `f'`/`f''` as EQN-menu
features — "evaluates the current expression's derivative using the
value in X" — with no accuracy guidance at all. **AN0022** (RPN solve,
integration, plot; 2026-07-13) carries a substantial "Accuracy, in
plain terms" section and an "Accuracy when you nest" subsection
covering solve-in-solve, integral-in-integral, solve-in-plot and
integral-in-plot, with the SDIGS and ACC dials explained — and does
not mention derivatives once in that prose, though its own test
program nests derivative engines THREE deep (PLOT(DERIV(DERIV(DERIV)))
and SOLVE(DERIV(DERIV))). So: deep engine nesting is explicitly an
intended, exercised use case (which vindicates round 5's parity work),
while derivative ACCURACY is simply undocumented.

Two things fell out of reading them:

- **The failure band is narrower than round 6 recorded.** Sweeping the
  same function: x = 0.1, 0.05, 0.025 and 0.01 are all EXACT; 0.005
  returns 4.28 for a true 1.4888; 0.001 returns −1511. Upstream's own
  derivative plots sample −5..5 at ~0.025 intervals, which is why
  their tests never met it.
- **The remedy already exists in the firmware.** `deriv_user_step`
  (differentiate.c:240) honours a step the user puts in the named
  variable `δ_d`, exposed as the Δ softkey in the derivative menu.
  Set it, and the relative-step collapse cannot occur. The case this
  package cares about — INTEG(DERIV(...)) over [0,1], true 5/6 — goes
  from −2.947e23 to 0.8333333333333333333333333332992391 in 266 ms.
  EQ28 pins it.

So the verdict sharpens rather than softens: still a genuine silent
defect at default settings (the built-in key at X = 0.005 answers
confidently with nonsense), but fully mitigable with a control that
already ships. The real gap is documentation — the notes explain the
integrator's ACC and the solver's SDIGS at length and never connect
the Δ step to the failure it prevents. That is the shape the upstream
report should take.

Method note: the appnotes were the right place to look and I had not
looked. The answer to "is this a known limitation?" lives in the
project's own documentation, and reading it cost minutes.

## 2026-08-26 — the zero-limit caveat was wrong (Stan asked; round 6)

Stan asked whether the numeric quirk recorded in round 5 was a genuine
bug. Answering it honestly meant testing the claim instead of asserting
it — the round-5 text said "an INTEG limit exactly at 0 collapses the
stencil, use nonzero limits", which was an INFERENCE from where the
garbage appeared, never a measurement.

Measured directly, driving upstream's own `fn2ndDerivEq` on the plain
formula `6/(X+2)` with no construct or package code in the path, the
inference was backwards:

- x = 0 exactly: **correct** (1.50000000000000000000000000025).
- x = 1: correct. x = 0.001: **−1511.79** where the truth is 1.4978.
- x = 1e−8: −9.28e7. x = 1e−16: 1.51e29. x = 1e−24: 0E+17.
- The FIRST derivative is correct at every one of those points, only
  failing at 1e−40 — exactly what an h² division predicts.

Mechanism, from upstream's own comment (differentiate.c:478): the step
is sized as a fraction of x, with an absolute fallback ONLY at exactly
zero. Small nonzero x therefore gets a step that has underflowed
against the function's scale; the second difference cancels to noise
and h² amplifies it. Erratic rather than monotone (1e−12 comes out
right) because it is noise, not bias.

So: a genuine, silent, user-reachable upstream defect — press the
built-in d²/dx² on any formula at X = 0.001 and it answers confidently
with nonsense. It is not the package's, and per the upstream-convention
rule it is not the package's to patch either. DESIGN.md now carries the
measured table and the corrected guidance (avoid ranges NEAR zero, not
"zero exactly"), TESTING.md records that EQ26's [1,2] limits dodge it
deliberately, and it is a candidate for an UPSTREAM_REPORT.

Standing lesson, third time in this package: a claim inferred from
where a symptom appeared is not a finding. Round 4's MUT-41 was green
under a stale binary; round 5 blamed "corruption" for what was
numerics; round 6 wrote a caveat pointing at the one input that
actually works.

## 2026-08-26 — render/eval parity (Stan's ruling, round 5)

Stan asked whether the stack risk could be reduced so the full tower
EVALUATES — render and eval limits must match, waiting is fine. It can,
and the guessed numbers were all wrong in the safe direction:

- **Measured, the fear evaporates.** The full tower high-waters 5.3 KB
  of stack on the 64-bit sim (ARM frames are smaller) and evaluates in
  27.8 s — not the estimated 8-12 KB and minutes-to-hours. The fixed
  depth cap of 2 is replaced by a stack-consumption guard: the
  outermost construct records the SP, every deeper level refuses
  cleanly past an 8 KB allowance. MUT-55 (allowance zeroed) reds every
  nested pin, proving the guard live and correctly placed.
- **The differentiator's entry parse runs MVAR mode and errored on
  every construct** — ';' is a hard error in the base grammar and the
  MVAR intercept only hid the NAME. It now consumes the whole span.
  Construct-internal variables are not enumerated (they bind their own).
- **Temp-slot appends refused to be blamed:** the append rollback fired
  on PENDING errors it did not cause, corrupting the formula-list
  bookkeeping under the differentiator's transient sample errors.
  Appends and slice evals now refuse under a pending error.
- **The garbage was numerics, not corruption** (a day of probes said
  so): tanh-sinh clusters nodes at x ≈ 1E-24 off a zero limit, where
  the second-derivative's RELATIVE-step stencil collapses
  catastrophically — upstream's interactive d²/dx² at that abscissa
  produces the same E+39. Documented caveat: nonzero limits. Over
  [1,2] the tower matches the analytic value to 16+ digits.
- **Nested ∫-in-∫ simply works** — the new double-exponential path
  never increments the engine counter, so upstream refuses nothing
  (EQ27 pins ∫∫x = 0.5). The refusal-hardening in the delegate stays
  as defensive code for the old path: a refused engine errors instead
  of returning a stale X.
- Probe hygiene lesson: two debugging rounds were spent on artifacts of
  the probes themselves (a display helper scratching tmpString
  mid-parse; a probe block reordered by its own patch anchor). Probes
  print AFTER evals complete, or not at all.

## 2026-08-26 — typography follow-ups (Stan's review, round 4)

- Multiplication typesets as the raised dot (STD_DOT) in both pretty
  renderers — the x glyph reads heavy beside fractions and big
  operators. The linear/edit views keep the true text; FV1 pins the
  dot (MUT-53's red shows only in the pass/fail counts — the expected
  and actual strings differ only in glyph bytes, which grep's binary
  mode suppresses).
- The vinculum now carries the radical sign's stroke weight: standard
  font strokes are 2 px but vincThick was 1 there, so the bar read
  thinner than the sign it joins. Only tiny stays 1 px. P6 pins the
  weight — its first probe sat beside the raised glyph's own top hook,
  which lit the row and kept MUT-54 green; the probe moved to the
  vinculum's right end (a pin green under its mutation is decoration,
  applied once more).

## 2026-08-26 — ultimate nesting (Stan's stress request, round 3)

- Stan asked for the full tower: Σ over a multiplication, times ∏,
  inside d²/dx², inside ∫. Three capacity walls fell in order:
  PP_MAX_DEPTH 6 (the tower nests 9 boxes; now 12 — the pool is the
  real bound), PP_POOL_NODES 48 then 64 (the DERIV layer alone is 58
  nodes; the ∫ wrapper landed exactly ON the 64 cap — now 72, +384 B
  BSS), and construct limits typeset at full body size in EQSHW
  (limits/scripts are now always TINY, and the '/' deep-refont is
  guarded when the caller's fonts are equal so it cannot flatten a
  construct's tiny limits back to body size).
- **The demo declined through the real surface and exposed a general
  cap: EQSHW could never show an equation longer than the strip.**
  fnPrettyEqShow fed ppqShowRender from showEquation's display string,
  which is built for the 400 px strip and truncates with an ellipsis —
  the strict parser then rightly declined (the EQ4 rule doing its
  job). EQSHW now reads the STORED text; the stored alphabet needed a
  '^' arm (which builds a real 2D superscript, better than the
  display's glyph run) and the NAME: label-prefix skip parseEquation
  itself uses.
- An additive body under a big operator misreads without parens
  (∏ 1+x could be (∏ 1)+x): HBOX bodies carrying a +/- joiner now
  scope in parens (EQ25, MUT-52). Fractions, powers and radicals
  already scope visually.
- Evaluation depth stays capped at 2 (the ruled trade-off: nested
  delegate engines on the device stack), so the full tower RENDERS but
  declines to evaluate below depth 2 — the render and eval limits are
  independent by design.

## 2026-08-26 — big-operator letterforms (Stan's review, round 2)

- Stan asked whether the hand-drawn operators could match the real
  symbols. The right references were already on the machine: the
  FONT'S OWN Σ/∏/∫ glyphs, rendered and read back as bitmaps. The
  numeric Σ is 14x25 (w/h 9/16) with 3-row bars, thick diagonals and
  its apex at ~40% width — while ours was a fixed 16 px box with 2 px
  bars, which left tall operators pinched and chevron-like. Σ and ∏
  now scale from ppBigopBox (one function, called by measure and
  paint) with the font's proportions; the ∏ gains its overhanging bar
  and inset legs. The ∫ keeps the curved textbook S from the previous
  round — the font's ∫ is a straight stem with blob terminals, which
  at operator scale reads as a bar.
- **Paid the stale-binary trap in full, from the harness side.** The
  helper landed below ppMeasure's use of it: implicit declaration, a
  compile ERROR — and pp-iter.sh's ninja pipe ended in `|| true` with
  the output greppped away, so the OLD binary ran and reported 7
  green, and MUT-41's "red check" ran the same stale binary green. A
  pin that stays green under its mutation was the tell (the rule
  caught the harness, not the code). pp-iter.sh now fails loudly on a
  build error; "silence is not-run" applies to one's own tooling.

## 2026-08-26 — visual polish (Stan's review of the PP12-PP14 shots)

Three findings from human review of the capture sheets — all three were
invisible to the structural pins and two of them now have pixel pins.

- **The fraction bar sat one row tighter below than above** (2 clear
  rows over the bar, 1 under — measured off the shot, then explained:
  descent counts rows at/below a baseline while ascent counts strictly
  above, so `+ fracGap + den.ascent` lands the denominator one row
  closer than the numerator's mirror formula). Fixed with an explicit
  +1 on the denominator side; the radical's vinculum had the same
  asymmetry over its radicand and got the same +1. Fractions are one
  row taller: M1/M2/M5 metric pins updated, P4/S2 row pins shifted
  (fixed-baseline vinculum up 1; the centered PSHOW fraction re-centers
  up 1). MUT-48 pins the clearance.
- **The stroke-drawn ∫ read as a slash**: straight-line hooks. Replaced
  with `ppDrawIntegralSign` — a 2 px spine whose hooks bend along a
  quadratic per-row offset with terminal dots, shared by PP_INT and the
  PP_BIGOP ∫ arm (the old shape lived in two copies; now one). P5 pins
  the hooks' sideways reach; MUT-50 red.
- **The variable X typeset as a capital**. The fonts carry no italic
  alphabet (probed x, x̄ 0x837f, χ 0x83c7 as candidates — the plain
  lowercase x is the closest form to the textbook convention), so the
  canonical variable X now renders as lowercase x across the pretty
  surfaces: ppfVariableName (PP12 ∫ bodies, PP13 frames), ppqName and
  the construct var runs (PP14 bodies and decorations — body and
  under-limit stay consistent). Scoped to exactly 'X': the first draft
  lowercased every single capital and renamed A+B to a+b — EQ2's
  pinned caps caught it. Other names keep their letters; the
  linear/edit views always show the true text. MUT-49 red.

## 2026-08-26 — PP14 (equation-language SUM/PROD/DERIV/INTEG)

- The design pass paid for itself three times: (1) parseEquation's whole
  state lives in its caller's mvarBuffer, so nested slice evaluation is
  re-entrant by construction with private buffers — no engine refactor;
  (2) `;` is a hard error in the base grammar, so the separator space
  was free and radix-proof; (3) every XEQ caller passes
  tmpString/tmpString+AIM_BUFFER_LENGTH, which fixed exactly what the
  DERIV/INTEG delegate must snapshot.
- **END_OF_FORMULA pops the numeric stack after writing REGISTER_X** —
  the first slice reader took the value from the slice's own stack and
  read an empty cell (all four constructs dead on first run, err-free).
  X is where a slice's value survives; the differentiator reads it the
  same way.
- **The derivative engine reads its point from the solver VARIABLE, not
  X** (covDerivEq STOs X into it before calling): the delegate feeds
  both channels with direct register writes. DERIV then matches
  deriv_cov's exactness pins digit for digit.
- The temp formula slot has its own appender and tail-deleter: fnEqNew
  opens the editor and moves currentFormula; deleteEquation resets
  currentSolverVariable. Both side effects were found by reading, not
  debugging.
- The bound variable binds by DIRECT register write (no dispatch runs
  inside the evaluation) and restores via the differentiate.c probe
  idiom; MUT-45 pins the restore.
- All construct buffers are transient pool blocks — zero resident BSS
  for the whole stage; the free-list allocator never relocates live
  blocks, so the outer parse's string pointer survives the formula-list
  appends.
- Stack-churn ruling: the evaluator's own operators lift/drop through
  the machine stack (T is junked by any operator chain), so the
  constructs' nested evaluations add the same CLASS of churn upstream
  already produces — no snapshot, documented instead.
- Solve framing skipped in PP13 stays skipped here; ∪/∩/lim stay
  excluded (no machine semantics — dead code violates discipline).

## 2026-08-26 — PP13 (solver-surface templates)

- EQSHW's integrate frame graduated from the bare PP7 stroke ∫ to a
  PP_BIGOP carrying the session's REAL limits (RESERVED_VARIABLE_LLIM/
  ULIM formatted through the standard builder) and the d-variable name;
  the PP12 layout arm needed nothing new — the same node kind serves
  capture decode and the solver surface.
- The d-variable decode was refactored out of PP12's ppfBigop into the
  exported `ppfVariableName` so both TUs share one best-effort rule.
- **Solve framing (f(x)=0) skipped, with the reason ruled into DESIGN.md:**
  `SOLVER_STATUS_EQUATION_SOLVER` is the ZERO value of the mode field —
  a stale INTERACTIVE bit after any past session is indistinguishable
  from a live solve, and the frame would decorate a plain view with an
  `= 0` the user never asked for. Un-determinable state stays unframed.
- The limits render with the real marker (`0.`, `1.`) exactly as the
  reserved registers hold them — same acceptance as PP12's step.
- EQ8's PP7-era fixture (mode bits only, no INTERACTIVE) keeps passing
  by design: the frame builder's fallback IS the old behaviour, so the
  fixture now pins the fallback rather than needing an update.
- MUT-42's red run is only visible in the pass/fail COUNTS — the EQ13
  FAIL line carries superscript-2 glyph bytes and grep's binary mode
  suppresses it (the TESTING.md trap, now hit from the harness side).

## 2026-08-26 — PP12 (captured Σ/Π/∫ big-operator nodes)

- **The capture hooks were not nesting-safe — latent since PP3, load-bearing
  from PP12 on.** `prettyNoteFunction` cleared `ppcStage.valid` BEFORE its
  scope check, and a BIGOP dispatch runs its label program's every step
  through `runFunction` (executeOneStep → runFunction, under
  FLAG_SOLVING/PGM_RUNNING): the first inner step destroyed the outer
  stage and the BIGOP DONE never applied. The same clobber was reachable
  pre-PP12 by any function whose body nests `reallyRunFunction` in scope
  (`.d` executing →REAL is upstream's own example). Fix: scope-check
  first — `valid` is only ever true strictly inside a dispatch, so a
  top-level STAGE out of scope has nothing to clear.
- **The screenshot caught a display lie the sig pins could not.** The first
  ∫ trace called `fnIntegrateYX(label)` — which is the interactive SETUP
  form: it harvests X,Y into ULIM/LLIM, drops them, retargets the solver
  and integrates NOTHING. The capture minted a BIGOP whose "result" was
  whatever X held (0), and the composed sheet read `∫₀¹P(x)dx = 0.` —
  truthful-looking, wrong. Ruling: capture only the dispatch that actually
  integrates (named-variable param over a preselected label program, the
  `covIntegratePgm` currency); every other param form invalidates (B6b
  pins it). The variable id rides in the payload so the body shows the
  real d-variable (`P(X)dX`).
- **After a BIGOP dispatch, only X can be vouched for.** The label program
  ran with the machine's full keyboard between STAGE and DONE — every
  register but the result is somebody else's writing. Slot 0 = the BIGOP,
  all other slots and slot L go UNKNOWN (lazy VAL re-materialization).
  The first draft slid-and-top-dupped the surviving trees; that claimed
  register values a user program can falsify. Reverted before it ever ran.
- **The ∫/Σ dispatches leak solver state into later suite files.** The
  full gate failed 4 upstream tests in deriv_cov: `fnPgmInt` (inside the
  ∫ path) clears `SOLVER_STATUS_USES_FORMULA` and retargets
  `currentSolverProgram` at label P, and `covDerivEq` — unlike
  `covSolveRoot` — does not reset the status it inherits, so it
  differentiated my x² program instead of its X³ formula (the observed
  2x/constant-2 values matched exactly). The driver now saves and
  restores `currentSolverStatus/Program/Variable` + `currentMvarLabel` —
  the aimBuffer restore rule, extended to solver globals.
- **MUT-41 stayed green on its first run** — the B8 probe rect reached
  column 35, and the body run (`P(n)`, starting at colW+3 = 29) shares
  the probe rows, so body ink masked the deleted strokes. Probe tightened
  to the operator column (x..x+26). Same lesson as FV7/FV9: pin the
  pixels only the mutated code can light.
- The sum step travels as real34 (`fnToReal`'s own currency, longint
  converted at STAGE via `convertLongIntegerRegisterToReal34`), so a
  non-unit step renders with the real marker: `n=1,Δ2.` — accepted as
  truthful rather than re-formatted.
- Fixture notes: program installed through a copy-adapted
  `covWriteAndLoadPgm` (label `P`, `x²`, the upstream pgmT shape); the
  history-order in `ppcHistoryEntry` is newest-first; live key replays
  repaint the register lines, so the capture sheet builds all entries
  first and paints after (the first composed shot lost its under-limits
  to exactly that).
- No new upstream hunks: PP12 is entirely package-internal (capture,
  layout, formula, tests). §7 claims unchanged.

## 2026-08-26 — post-PP11 capture polish

- The PP6-PP11 showcase captures found one cosmetic defect: the ⁿ√
  index kerned into the RAISED-GLYPH sign's own hook. Fixed: an indexed
  root always synthesizes its stroke sign (measure and paint recompute
  the same test). Also a capture-driver lesson worth keeping: CHS during
  NIM is a buffer EDIT, not a dispatch — a fixture that dispatched it
  left the NIM open and the following digits appended, producing
  ³√(-527) on screen. The engine rendered that truthfully, which is the
  invariant doing its job; the fixture now closes NIM through the real
  keypress pattern.

## 2026-08-26 — PP11 (the toggle becomes a persisted system flag)

- FLAG_PRETTYP (0x8071, bit 50) replaces the package bool: PPON now
  persists across power cycles with the ordinary flag machinery. The
  count-line conflict that made a flag impossible in v1 is resolved by
  the identical-edit claim: BOTH packages' defines.h carry
  NUMBER_OF_SYSTEM_FLAGS 64+50 verbatim, and the 3-way merge unifies.
- **Adjacency lesson sharpened: diff3 conflicts on TOUCHING-line edits,
  not just same-line ones.** My row-2300 items.c edit sat one line below
  undo-history's row-2299 edit and conflicted (my hunk's context carries
  upstream's 2299, their patch had changed it); the identical-edit trick
  cannot save a hunk whose CONTEXT mismatches. Resolution follows the
  case-20 pattern: the SYSFL row 2300 ("PPRTY", literal 0x8071) lives in
  UNDO-HISTORY's patch alongside its own row-2299 edit; solo
  pretty-print keeps the fully-working flag but shows no catalog row for
  it — a documented solo-config gap, not a behavior change.
- Ordering trap paid for: the config.c prettyReset hook originally sat
  BEFORE doFnReset's `systemFlags0 = 0` wipe — the default-ON would have
  been erased moments after being set. The hook moved below
  `Sett(_Reset)`; FV13 pins reset-restores-default-ON.
- DESIGN.md §7's "no system flag in v1" entry is superseded by this
  stage; the claims registry rows there stay authoritative.

## 2026-08-26 — PP10 (the browser lands on calcMode 20)

- **The composition wall was condition LINES, not insertions.**
  Undo-history's browser edits ~7 shared condition lines (fn-key/TAM
  guards, shift inclusion, keyActionProcessed, underline, keyboardTweak)
  — a third browser editing the same lines is a guaranteed 3-way
  conflict. Resolution: ONE undo-history amendment (Stan pre-authorized
  the mechanism class) converts those lines to the range form
  `calcMode 19..23 = package browsers` and adds literal `case 20:` to
  its case-list insertions — solo-safe, and browsers 21-23 now compose
  for free. Recorded in undo-history DESIGN.md §6.
- Pretty-print's own keyboard surface is insertions only: the
  head-of-chain resolution branch (both packages independently prepend
  `if(mode){...} else` blocks at different lines — the 3-way merge
  CHAINS them, which is the elegant part), and one case block per
  fnKey handler, each anchored after a complete case body well away
  from undo-history's insertion points (fallthrough groups must never
  be split — a braced case inside one captures the group).
- PHIST now opens the browser; UP/DOWN select (clamped), .d pans a
  too-wide selected row (wraps), ENTER recalls the selected entry's
  TKRES into X — saveForUndo first, lift, then `ppcShadowInvalidate()`:
  the recall bypasses item dispatch, so the shadow wipes to UNKNOWN
  rather than pretending it followed. The live (now) row recalls
  nothing — its value IS X. The manual-paint pager body remains as the
  non-browser fallback surface.
- Keyboard-case reachability is proven structurally (the handlers are
  3-line breaks); the handlers themselves are pinned via direct calls
  (FV12). A B9-style real-keypress harness remains open work.
- The head-of-chain branch was ABANDONED after conflicting at
  undo-history's exact insertion point (and the earlier anchor would
  have left a bare `else` binding into a PC_BUILD preprocessor block):
  instead forth-core's REWRITTEN key-resolution list line gained
  `|| (calcMode >= 20 && calcMode <= 23)` — package browsers without
  their own resolution branch resolve plainly through the standard
  branch. Solo-pretty-print (no forth-core) resolves nav keys through
  the final else (primaryAim == primary for them); only .d-pan may
  differ solo — documented quirk, direct-call pins unaffected.
- **FOUND, PRE-EXISTING, NOT OURS: the forth-core + undo-history
  HEADLESS BATTERY segfaults** right after fold test [9] (R8-P1) — with
  or without the amendment, with or without pretty-print. No gate ever
  ran the battery under a combined shadow (forth's battery gate is
  solo; combined passes run only the meson testSuite). Reproducer:
  configure CUSTOM_PKG=packages/forth-core,packages/undo-history with
  -DFORTH_DEBUG_SELFTEST, run `c47 --headless`. Filed here for the
  forth-core/undo-history owners; the forth+pretty-print battery is
  GREEN at this tip.

## 2026-08-26 — PP9 (RCL/STO capture classes)

- Register operations now chain instead of invalidating: RCL pushes a
  NAMED leaf for numbered registers (R05 stays R05 — a name is truthful
  even if the register is later overwritten), deep-copies the shadow
  tree for stack-register recalls (copy-then-lift, matching upstream's
  read-before-shift), and degrades lettered/named registers to value
  leaves (their display names are not item ids). RCL±×÷ builds a dyadic
  node with the PLAIN operator item so the renderer needs no new cases.
  x<>reg emits the departing tree (its value still sits in X at STAGE)
  and leaves a value leaf for the register's old content — naming the
  register would lie, since it now holds the swapped-in value. DROPY
  mirrors upstream's shift-with-top-dup from slot 1.
- T18 documents a subtle correct behavior: using a stack-recalled COPY
  of the open formula supersedes (emits) the original still sitting
  higher up — the copy continues visually, the original is finished.
- STO and STO±×÷ stay stack-silent (PPC_STO_NOP), as before.

## 2026-08-26 — PP8 (T-line live formula, opt-in)

- PTLIN (row 215) shows the OPEN formula on the T register line while
  chaining — DEFAULT OFF, exactly as Stan specified when selecting the
  gap: T's value is hidden while the toggle is on, so the user opts in.
  Implementation is a T-only branch at the top of the inline surface's
  ladder: standard rung, then the whole-tree tiny re-font, and any
  no-formula/no-fit case falls through to the ordinary value rendering.
- FV11 pins all three properties: OFF-by-default (band identity between
  the fresh state and forced-off), the formula appearing when toggled,
  and the X line staying a value under the toggle. The first FV11
  landed with a stubbed default-off comparison — caught in review
  before commit; the capture helper grew a band parameter to close it.

## 2026-08-26 — PP7 (EQN full view; the Σ finding)

- **The equation language has NO Σ/∏/∫ constructs** — its function
  vocabulary is trig/hyperbolic aliases plus the glyph→item map. A Σ
  template would be dead code with zero possible inputs (capture
  excludes solver items by standing ruling; EQN cannot express a sum),
  so Σ was SKIPPED, narrowing the purchased scope honestly. The one
  legitimate big-operator surface: the interactive integrate solver,
  where the stored equation IS the integrand — EQSHW frames it with a
  stroke-drawn big ∫ (PP_INT) when
  `(currentSolverStatus & SOLVER_STATUS_EQUATION_MODE) == INTEGRATE`.
- EQSHW (row 216 — the 214-216 spare run; forth-core edits row 213's
  line, different lines merge cleanly): full-screen equation view on the
  manual-paint protocol. `ppqParse` gained font parameters; the full
  view parses standard/standard, so the nested fractions the 23 px strip
  declines render at full size; unparseable equations still show their
  linear line (the always-show-something fallback, pinned by EQ9).
- Ellipsis caveat stands: the view renders showEquation's display
  string, which upstream truncates at screen width — very long equations
  fall back to the (truncated) linear line rather than mis-typesetting.

## 2026-08-26 — PP6 (typographic trio; gap-closure wave, Stan's selection)

- PP_RAD grew a synthesized stroke sign (Bresenham over setBlackPixel,
  2 px in numeric contexts) for radicands taller than the font glyph —
  √(fraction) now renders instead of declining — and an optional second
  child: the ⁿ√ index tucked above-left with ~half overlapping the sign.
  XTHROOT and CUBEROOT render as indexed radicals (index = X operand for
  ˣ√y, verified against fnXthRoot's register use).
- New node kinds: PP_SUB (log_b subscripts; LOGXY verified as log base X
  of Y from WP34S_Logxy's argument order — also had to JOIN the capture
  dyadic table, which is how FV9 found it missing) and PP_BARS (|x| for
  ABS/MAGNITUDE, two 2 px strokes one row proud of the child).
- Two pins were born decorative and sharpened same-day: FV7's sign probe
  included the vinculum's first column (any-ink satisfied without
  strokes), and FV9's root-descent assert was satisfied by "log"'s own
  descender — the pin now reads the script node's relBase directly. The
  mutation sweep exists precisely to catch pins like these.

## 2026-08-26 — post-PP5 stress test (continued fractions)

Stan asked for a complex-expression trial. `1/(2+3/(4+5/6))` keyed through
the real paths (6 ENTER 5 x<>y ÷ 4 x<>y + 3 x<>y ÷ 2 x<>y + 1 x<>y ÷):

- The capture engine held ONE open formula through all eleven operations
  (history stayed empty — every op consumed the previous root), and the
  layout engine rendered the full three-level nest with correctly nested
  bars. Depth headroom: a run at nesting level 6 hits PP_MAX_DEPTH — one
  more fraction level than this test would decline.
- **Found: the pager's fixed 36 px rows silently dropped tall formulas**
  (the rung ladder only re-fonted leaf runs, and even all-tiny the nest
  measures 37 px). Fixed red-first (FV6): `ppSetFontDeep` on the tiny
  rung AND the pager rewritten to variable-height packing — rows pack
  until the band fills, pages fall out of the packing walk.
- **Found: rows packed flush against the frame lines wipe them** — glyph
  boxes overhang their ink by the font padding and the pre-clear erases
  the frame (FV5 went red the moment packing started at the band edge).
  The packing band is inset 4 px from both frames.
- Σ+ after a chain: the shadow invalidated truthfully and emitted both
  finished formulas first — integration/summation internals stay outside
  capture scope by the standing ruling (solver/integrator storms), and
  the unknown-item default handles the items themselves.
- EQN strip: `(A+B)/C+1/D` renders as two stacked fractions in the 23 px
  row; nested `1/(2+3/4)` declines to the linear line as designed.

## 2026-08-26 — PP5 (EQN strip 2D)

- The EQN prettifier parses showEquation's DISPLAY string, not the
  stored source — superscript exponents are already glyphs there, so the
  grammar only owns what it improves: '/' terms stack as
  standard-context/tiny-child fractions (17 px, inside the 23 px strip
  row) and √ gets its vinculum. `ppSetFontDeep` re-fonts a built subtree
  instead of re-parsing per rung.
- The grammar is deliberately strict: numbers (with verbatim ·₁₀ⁿ
  tails), ASCII names (+ subscript digits), + − × · / ( ) √ = and
  attached sup-runs. Ellipsis truncation, unknown glyphs, or any
  unconsumed tail decline to the linear line. A parse with no fraction
  and no radical also declines — nothing 2D is gained.
- Hook: one hunk at the solver/equation.c paint site, gated to the
  no-cursor path — editing always shows the linear form the cursor
  logic was built for. solver/equation.c was a virgin file (no sibling
  package touches it).

## 2026-08-26 — PP4 (formula view)

- **The browser became a pager (RULED).** The plan's CM-mode browser
  costs ~20 keyboard.c sites in the one upstream file where forth-core
  rewrites the determineItem chain and undo-history already had to
  squeeze its branch into a hunk gap — the project's riskiest
  three-package composition surface. PHIST on the PSHOW manual-paint
  protocol delivers the user goal (seeing the chained operations
  naturally) with zero keyboard/defines churn: repeated presses page,
  any key releases. calcMode 20 stays reserved; the full browser is an
  explicitly possible later upgrade, not a closed door.
- **One constructor pair (`ppfCombine1/2`) serves both the live tree and
  the token stream**, so the two paths cannot drift typographically.
  DIV → FRAC (children never parenthesized — the bar scopes), YX → SUP
  with the base keeping its parens, √ → RAD (the vinculum scopes),
  1/x → FRAC(1,x), x²/x³ → SUP, CHS → leading minus, everything else →
  function form name(…) — which also future-proofs unknown dyadics that
  later become classified.
- **PP_PAREN has two modes chosen at measure time**: glyph parens when
  the child fits the font's '(' ink, synthesized 5 px stroke parens for
  tall children (fraction inside parens). The paint recomputes the same
  test instead of storing a mode bit.
- All six drivers green on the first PP4 build — the PP3 trace helpers
  (real key paths) carried the formula tests for free.

## 2026-08-26 — PP3 (capture engine)

- **Thirteen of sixteen traces passed on the first real-path run** — the
  two-phase STAGE/DONE design and the deferred NIM lift survived contact
  with the actual choreography. The three findings:
  (1) `ppcDeepCopy` masked `PPA_EMITTED` out of `aux` for every node
  kind, but LIT/VAL store a LENGTH there — a copied `"2"` truncated to
  the empty string (T4). Flags now strip for op nodes only.
  (2) A trace script bug, not an engine bug: backspacing an
  already-aborted NIM lands in CM_NORMAL and leaves an error that the
  next DONE treats as invalidation (T8's stray second backspace).
  (3) Arena exhaustion recovers BETTER than designed: after the
  invalidate, ensureKnown rebuilds truthfully from value leaves and the
  chain continues as `# 1 + 1 +` — the pin now asserts the recovery.
- **`nimWhenButtonPressed` is keyboard-owned** and false under a test
  driver, which flips fnKeyEnter's eRPN condition; both the shadow's
  mirror and the driver read/mimic the real global (T15).
- **L degrades to UNKNOWN on every op** rather than deep-copying the
  consumed operand (the design's "move" would alias a child node):
  LASTx returns as a truthful value leaf — `(2+3)×3` renders with the 3
  as a value, which is what the registers say (T13).
- **Deferred lift earned its keep twice**: T8 (abort after ENTER) and
  T16 (abort with ASLIFT set), the latter added when MUT-14 survived T8
  — lift-at-open only mis-fires when the abort follows a lift-armed
  open, and T8's abort follows ENTER (ASLIFT clear).
- **Top-of-stack falloff emits without a result** — by the time the
  shadow applies a lift, the real register is gone; the token stream
  simply omits TKRES for those formulas.
- **Driver hygiene:** the capture driver's real NIM typing left
  `aimBuffer` residue that broke `fn42Alpha` in string_cov.txt ("aimBuffer
  is empty headless") — a one-test blast radius the full-suite gate
  caught. Rule reaffirmed: a driver leaves the machine as it found it,
  including input buffers, not just flags and registers.

## 2026-08-26 — PP2 (radicals, exponents, IRFRAC, complex, PSHOW)

- **Radical sign strategy narrowed for PP2:** the raised-glyph approach
  only (sign painted so its ink top meets the vinculum; sign font = the
  node's font), with radicands taller than `radInk + 3` declining. The
  synthesized-DDA sign for tall radicands is deferred to PP4, where
  expression trees can put fractions under radicals.
- **IRFRAC parser scoped to the common template**
  `[sign] [multiple: n×|n|supN] name [/den]` with name ∈ {√d, √π, π, e, φ}.
  The exotic checkForAndChange outputs — mixed-number constant forms
  (`e+…`), the paren-power family `(π²)`, `(e⁻¹)` — decline to upstream's
  linear rendering by the fallback rule. Bare names with no structural
  win (lone `π`) also decline: no visual change, so no reason to own the
  paint.
- **The real path gained a third alternative:** IRFRAC's pure-fraction
  output (constant = 1) uses the same sup-num/`/`/sub-den alphabet as the
  FRACT builder, so `ppParseRealAny` = exponent → irfrac-template →
  fraction. A value like 0.75 with IRFRAC on but FRACT off now stacks.
- **Angular-tagged reals decline** (`getRegisterAngularMode != amNone`):
  the upstream angle path draws its mode suffix glyph in a separate pass
  the package arm would skip.
- **Exponent textbook form:** `mantissa·₁₀ⁿ` becomes base run
  `mantissa·10` (plain-size 10, original product glyph kept verbatim)
  with the exponent as a true raised run — PP_SUP with supDrop 10/6/4 px.
- **P4's probe lesson:** the √ glyph's own diagonal legitimately crosses
  the "gap row" in the sign's columns — gap/ink pixel pins must probe the
  radicand's columns only.

## 2026-08-26 — PP1 initial design and first findings

- Package created per the approved plan: natural display of calculations,
  undo-history-shaped package, five-stage roadmap (PP1 fractions inline →
  PP2 radicals/exponents/PSHOW → PP3 capture engine → PP4 formula view →
  PP5 EQN 2D). Segmentation ruled: liveness + new-root supersession; ENTER
  never terminates (DESIGN.md §4).
- **Slot claims verified against the live tree, two adjustments over the
  planning draft:** (1) the c47.h include anchors after the
  `#endif //TESTSUITE_BUILD` line, not the ui/ include list — undo-history's
  insertion after `ui/tone.h` sits inside the 3-line context window of any
  ui/-list anchor; (2) the package include enters via c47.h only, because
  forth-core patches screen.c's include block at line 3 and a second
  insertion there cannot compose. screen.c carries exactly one hunk (the
  render arm).
- **Non-X register lines have a 31-row descent budget, not 35.** Adjacent
  clear bands overlap: line N+1's `clearRegisterLine` starts at its own
  baseY−4 = line N's baseY+32, erasing anything painted there after line N
  rendered (refreshScreen order T,Z,Y,X). The planning draft assumed each
  line's own clear extent was usable. Consequence: an improper fraction
  (descent 5) floats its baseline up 1 px on Y/Z/T; the X line (band to
  baseY+38) does not float.
- **Paint-order finding (pin P1, red on first run): glyph-box pre-clears
  eat rules painted first.** The fraction bar was painted before the digit
  runs; `showGlyphCode`'s per-glyph box pre-clear (padding rows included)
  wiped it under both digits, leaving exactly the 2+2 overhang pixels lit —
  the pixel pin's "bar full-width at exact rows" assert caught it precisely
  as designed. Ruled into DESIGN.md §1 as the binding paint-order rule;
  PP2's vinculum inherits it.
- Measured constant worth remembering: `stringWidth(…, false, true)` drops
  the first glyph's leading empty columns, so a numericFont `"1"` run
  measures 14 px, not the 16 px advance (M2 pins 26 for the mixed-number
  HBOX, not 28).
- **First combined pass conflicted twice — both anchors moved, and a
  sharper rule emerged.** (1) items.c catalog stub: all three packages
  append stubs to the same list; undo-history inserts after :1677,
  forth-core after :1685 — an anchor at the list TAIL is contended by
  construction, so pretty-print's stub sits mid-list after :1670.
  (2) testSuite.c driver declarations: the declaration block (:83-:94) is
  contended the same way; eliminated entirely — testSuite.c includes c47.h,
  which already carries prettyPrint.h, so only the table-row hunk remains
  (anchored after `fnGetNDEC` :707, above both siblings' row hunks).
  The refined rule for future hooks: "≥4 lines from a sibling hunk" must be
  checked against EVERY package's hunks in that file, and natural append
  points (list tails, section ends) are exactly where everyone lands —
  prefer mid-list anchors next to entries no package will move.

## Audit round 3, wave 2 — the pre-clear box is not the ink box (R3-13)

Round 3's axis was "what does the owner actually SEE?", and it kept
paying at the paint layer. Wave 1 fixed the `.d` pan key (R3-10) and
clipped fill-drawn rules at a negative origin (R3-11/R3-12). Wave 2
closed the finding that was hardest to see, because the obvious fix for
it does nothing.

**The defect.** The engine measures tight INK extents — `ppRunInk`
derives ascent/descent per glyph from `boxAscent - rowsAboveGlyph` — and
every placement decision in `ppMeasure` uses them. Paint went through
`showString`, which paints each glyph via `showGlyphCode` with
`noPreClear` false, and that clear covers the whole FONT box:
`rowsAboveGlyph + rowsGlyph + rowsBelowGlyph` (screen.c:1239). Measure
and paint therefore used two different rectangles for the same node.

It bites hardest where a node's baseline is placed FROM its ink. A
fraction denominator gets
`relBase = barTopRel + barThick + fracGap + 1 + den.ascent`: the shorter
its ink, the higher its baseline must sit for the ink to clear the bar
by `fracGap` — and the higher its font box reaches, by
`fracGap + (boxAscent - ascent)` rows, across the bar and into the
numerator, which PP_FRAC paints first. Measured over the numerator's own
columns, an '8' numerator kept **52** lit rows over an '8' denominator,
**38** over an 'x', **20** over a '.'. The same overshoot let a run
packed against a band edge clear frame rows its measured ink never
touches (round 3's separate frame-line finding — same root, same fix).

**The near-miss worth recording.** Reordering PP_FRAC to paint the
denominator first reads like the fix and changed the measurement by
nothing at all. The erasure is not an ordering problem: the two
rectangles disagree, and no permutation of the paint order makes them
agree. That dead end cost a full measure/rebuild cycle and is why the
class entry says *a fix that does not move the measurement is not a fix*.

**The fix.** `ppShowRun()` clears the MEASURED box with `ppFillVal(…,
LCD_SET_VALUE)` and then paints the glyphs with upstream's own
`noPreClear` flag, so paint covers exactly what measure promised. The
erase-before-draw contract is unchanged — only its extent is. The
denominator's clear now stops at `barTopRel + barThick + fracGap + 1`,
below the bar for every denominator by construction. `ppFill` splits
into `ppFillVal(…, val)` with `ppFill` as the ink wrapper. PP_PAREN's
glyph parens go through the same helper (they paint AFTER their child,
so a font-box clear there could eat the child's ink); the radical sign's
glyph keeps `showString` — it is the only thing painted in its columns,
and the vinculum follows it under the paint-order rule.

`slc`/`sec` in the glyph loop reproduce `_doShowString`
(screen.c:1365-1380) for the `showLeadingCols=false, showEndingCols=true`
call it replaces, so the advance stays the one `stringWidth()` measured.
Negative x and y are upstream's own convention at this call: it recovers
a wrapped y at screen.c:1226 and column positions wrap back into range
unsigned — verified against both simulator HALs, which reject an
out-of-range x in `bitblt24` and never index the buffer with it.

**Pin P12**, written from the semantics rather than the fix: nothing in
FRAC layout gives the denominator any say over the rows above the bar, so
one numerator over three denominators of very different ink height must
light exactly the same pixels, counted over the numerator's own measured
box. Red under MUT-A (`noPreClear` → false) with the original 52/38/20;
green after.

## Audit round 4, wave 1 — the instrument, not the engine (R4-1, R4-2)

Round 4 rotated the axis to failure semantics: what these subsystems
leave behind when they fail, rather than whether they are right when they
succeed. The out-of-family pass on the equation constructs (the one
question: does a partway failure leave the owner's equation list exactly
as it was?) came back with NO findings and a seven-path
considered-and-rejected list. The load-bearing claim in it — that
`restoreRegisterSnapshot` is gated on `restoreVar` and not on `ok`, so
the owner's loop variable comes back even when the body fails — was
verified here independently at equation.c:2230.

What the round did find was in the test suite. Three defects, all in the
same family, none of them in the product:

- **T21 was vacuous for three rounds (R4-1).** It claimed to pin the
  designed degradation for RCL-arithmetic against an UNKNOWN slot: a tree
  IS built with the sentinel as a child, and the display path withholds
  it. Every assertion sat behind `if(ppcCurrentFormulaRoot() != PPC_NIL)`
  — and that accessor withholds precisely the trees T21 was about, so the
  guard was false by construction and the body never ran. The test would
  have passed with ITM_RCLADD removed from the classifier. Seeing through
  the screen needed `ppcTestCurrentRaw()`; with it, T21 now asserts the
  four separate things its comment always claimed, in order. The engine
  was correct all along — MUT-B (disabling the opaque screen) reds the
  third assertion.
- **T23 and T26 were absence-only (R4-1).** `if(strstr(sig, "7")) fail`
  is satisfied by an EMPTY signature, so it passes when capture has
  stopped working. Both now assert the exact expected signature: T23 the
  whole truthful shape (`# 2 3 + ×` — the overwritten Y degraded to a
  value leaf, times the live sum), T26 the ruled outcome that a
  31-character literal WITHHOLDS the formula (`-`).
- **Two states shared one signature character (R4-2).** `PPN_VAL` and the
  `PPC_UNKNOWN` sentinel both printed `#`, which is exactly the
  distinction the binding invariant turns on. No signature pin could tell
  a truthful value leaf from an unknown. `~` now marks the sentinel. All
  three pre-existing `#` expectations stayed green through the split,
  which is the evidence that none of them had been silently accepting an
  UNKNOWN where they meant a value.

A false lead worth recording: T23's signature `# 2 3 + ×` read as a tree
with an UNKNOWN operand that the display had failed to withhold, which
would have been a real defect. Dumping the node showed `child[0]` was
node 0 of kind PPN_VAL — the ambiguity above, misleading its own author
inside the same hour it was found. That is why R4-2 is a fix and not a
note.

## Audit round 4, wave 2 — a formula displayed with an operand missing (R4-3)

The in-family failure-axis pass over the renderer found the round's one
product defect, and it is the same shape as R3-13 in a different layer: a
check the container cannot make, in the one container that cannot make it.

**R4-3.** `ppfCombine2` builds two of its forms — `LOGₓy` and the generic
`name(a, b)` function form — by appending `ppfParen(...)` straight into a
PP_HBOX without binding the result. `ppfParen` ALLOCATES, so it returns
`PP_NONE` when the 72-node pool is exhausted. Every other operand in both
forms is checked before use; these two were not, and `ppAppendChild`
silently no-ops on a `PP_NONE` child.

Nothing downstream catches it. `ppMeasure` checks arity for PP_FRAC,
PP_SUP, PP_SUB, PP_RAD, PP_BARS, PP_PAREN and PP_BIGOP — but PP_HBOX is
variadic by construction, so its measure walks `firstChild`/`nextSibling`
with no arity check at all, and paint does the same. The tree measures
and paints as a finished formula with an operand simply absent:
`log₂(8)` renders as `log₂`, `atan2(3, 4)` as `atan2`. The builder
returns TRUE. It reaches all three surfaces that funnel through
`ppfCombine2` — the T line, the PHIST pager and the browser.

The pool is the only way in, which makes the window narrow but real: for
any allocation, some input size is the one that exhausts the pool exactly
there. **FV18 calibrates that input at runtime rather than hardcoding
it** — it builds the formula once with a full pool, counts the nodes used
by scanning `ppNodeAt`, then rebuilds with the pool starved by exactly
one node, which lands the failure on `ppfParen` because it is the last
allocation in both forms. It asserts both halves, so the calibration
itself is pinned: at the measured count the build must succeed, one node
short it must FAIL rather than yield a partial tree. MUT-C (disabling the
new guard) reds it.

A sweep for the same shape found four sites in the package. The two in
`prettyEquation.c` pass `ppqUnwrapParen`, which never allocates, and feed
PP_RAD and PP_SUP, whose measure DOES check arity. So the two fixed here
are the whole exposure.

### R4-4 — hardened, not pinned

`pbPaint` used to `continue` an unbuildable row out of both passes, which
also skipped `selPage = page` when it was the SELECTED row: `selPage` kept
its initialiser, pass 2 painted page 0, and no selection marker appeared
anywhere, so pressing DOWN onto such a row looked like the browser had
reset itself. Both passes now reserve a fixed-height placeholder reading
"(too large to show)", so the row pages, selects and marks like any other
— the same reasoning that made an empty browser say "no formulas".

No reaching input could be constructed, and that is recorded rather than
papered over: see TESTING.md's documented-gap section for the two
ceilings that sit below the layout engine's (the 24-node capture arena,
and the renderer's depth guard dropping nested division to an inline
slash — a 25-level tower measured h=31, one fraction level). A T30
written for this branch was removed the same hour for failing the very
rule R4-1 established: a fixture that cannot reach its own state is not a
pin.

## Audit round 5 — the fix review (R5-1, R5-2, R5-3)

Round 5 audited rounds 3 and 4's FIXES rather than the feature, because
this project's measured pattern is that most of a round's findings come
from the previous round's repairs. Three readers: an out-of-family pass on
`ppShowRun` and every paint call site, and two in-family passes on the
browser/formula fixes and the keyboard/capture/test fixes.

**Out-of-family: no functional defects in R3-13**, with seven reasoned
exclusions whose arithmetic on the fraction bounds independently matched
ours. One exclusion was WRONG and is recorded because it nearly became a
false finding: it claimed the surviving `showString` in the radical arm
wipes the root's index. It cannot — `synth` is forced true whenever an
index exists (`|| (index != PP_NONE)`), paint recomputes the identical
test, and the `showString` sits inside `if(!synth)`. Round 3's reader had
this right and round 5's did not; the code comment states the collision and
the guard explicitly, which is why "argue with a comment, never ignore
one" is a rule.

**In-family on the browser/formula fixes: no findings**, with the
four-site sweep independently re-run and both browser passes confirmed to
paginate bit-for-bit identically.

**R5-1 — the fix invalidated a BINDING rule's justification.** The
fraction-bar comment and DESIGN.md's paint-order rule both said the bar
goes last because `showGlyphCode` pre-clears each glyph's full box. True
when written; false since R3-13 gave the runs `noPreClear`. The rule still
binds — but now only for the one remaining `showString`, the radical sign.
For a fraction the clears provably cannot reach the bar (numerator's stops
`fracGap+1` rows above, denominator's starts `fracGap+2` below), so the
ordering there is uniformity, not necessity. A sweep for the same class
found a third instance in the pager's band-inset comment. All three
corrected. Nothing functional; the hazard is the next person reasoning
from a stale mechanism.

**R5-2 — half of R3-13 is unverified, and that is now written down.** P13
was added to hold the dependency the fix rests on: bounding the clear is
safe only while something else clears the band, and upstream's
`clearRegisterLine()` calls are commented out at their call sites.
Measured, a 35-digit value repainted with a 3-glyph one leaves 467 lit
pixels, exactly a clean paint. Then MUT-D deleted the measured-box clear
outright and the suite stayed GREEN — P12 and P13 both. That is not a
coverage hole to close with an invented fixture; it is the truth about the
fix. Only `noPreClear` is load-bearing; the explicit clear is redundant on
every surface, because each clears its band and measure lays siblings out
non-overlapping. Kept as defence, recorded as unverified code. P13 is
therefore an upstream-drift pin, like P1's exact bar rows.

**R5-3 — the containment guard covered only half the dispatch path.**
R3-7's guard lives in `processKeyAction`, the direct-key half of the
driver. The SOFTKEY half is three separate upstream functions, each with
its own browser list this package never touched — so F-keys ran their
items underneath the modal browser, including our own `PCLR`, which wipes
the history being browsed while the browser repaints over the evidence.

The reason four rounds missed it: undo-history had already generalised
those three lines to `calcMode < 19 /* package browsers 19-23 */`, so the
COMBINED build was never vulnerable and the hole existed only in SOLO —
a gated configuration whose gate is green because no test drives a
softkey. The reader that found it flagged honestly that it could not tell
whether a sibling closed it; checking that caveat is what located the real
scope. This package now carries the byte-identical range clause itself
(the identical-edit claim, as with `NUMBER_OF_SYSTEM_FLAGS`), both gates
stay green, and FV19 pins `CM_PRETTY_BROWSER` inside 19..23 — renumbering
it out of that range would silently reopen the hole with nothing else
going red. DESIGN.md's calcMode row still said "20 reserved (not wired)",
stale since PP10; corrected in the same pass.

## 2026-08-29 — `ppfCombine1`/`ppfCombine2` renamed to `ppfBuildOp1`/`ppfBuildOp2`

Mechanical rename, no behaviour change; the gate is the proof. The arity
digit was never the problem — it is the package's own convention, shared
by `ppvOp1`/`ppvOp2`, `PPA_OP1`/`PPA_OP2` and `PPN_OP1`/`PPN_OP2`, and a
reader who has met any of those pairs reads it correctly on sight. The
verb was. These functions do not merely combine two nodes: they choose
the construct kind for the operator (`PP_FRAC` for divide, `PP_RAD` for
the square root, `PP_SUP` for the power), settle bracketing against the
operands' precedences, and report the result's precedence back through
`outPrec`. "Combine" said none of that, and it was the only verb of its
kind in a file whose every other name is `ppfBuild*`. So the name broke
its own file's convention while the digit followed the package's.

`ppfBuildOp1`/`ppfBuildOp2` satisfy both, and line up with the
`ppvOp1`/`ppvOp2` in the walker that calls them — which matters because
these two functions plus `ppfWrapIf` ARE the public seam between VISUAL
and the drawing engine, and are the first thing an upstream reviewer
meets.

Entries above this one keep the old name deliberately: they record what
was true when they were written, and so do the `AUDIT_*` reports. Anyone
grepping `ppfCombine` in the trail should read it as `ppfBuildOp`.

## The bracket rule's home, r5–r6 (2026-08-30)

The stacked-power bracket moved three times before it landed. Round 4 put
it in `ppfBuildOp1`'s SQUARE/CUBE arm, "the one place that can answer it",
and the header said no caller carried the precondition. Round 5 found
`ppfBuildOp2`'s `ITM_YX` arm building the same node kind twelve lines
above, and a value in scientific form whose exponent is already spelled
into its glyphs. Round 6 found two more spellings — a typed negative and a
tagged angle — and made the point the three rounds were circling: the
question is not "is the base already a power", it is "does the base read
as an atom", and only the producer that formatted the text can answer it.

So `ppfTextIsAtom` names the class and both numeric leaf builders report
`PPF_PREC_ADD` when their text is not one. The walker calls the same
function — it had its own narrower version ("a signed numeral brackets as
a term") since the PP18 refactor, which is exactly why the two surfaces
drew `5 +/- x²` differently. `ppfPowBase` keeps only the structural
`PP_SUP` test, because that one genuinely is invisible to precedence.

The lesson is the clause count. Each round added a spelling to the same
predicate and each fix looked like it closed the class. A guard whose body
is a growing disjunction of forms is a guard that has not found its
property yet.
