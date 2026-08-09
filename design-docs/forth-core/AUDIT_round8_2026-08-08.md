# Audit — the round-7 fix wave at `db2186cf1` (round 8)

Range `28eb24b93..db2186cf1` — the four fix commits that closed round 7's
nine confirmed findings, plus the DESIGN-HISTORY entry. The regression
record said this round's findings would live in those fixes (r2 4/7, r3
4/4, r5 9/12, r7 2 behavioral of 7). **The record held, and this time the
finder was out-of-family**: both behavioral findings of round 8 are defects
in fixes written the same day, and both came from Gemini rather than from
the eight in-family dimensions.

*Bookkeeping note.* Unlike rounds 6 and 7, this round is **not**
findings-only. The two out-of-family findings were reproduced red-first and
fixed in `58e07a2bd`, because leaving a known data-loss path open across a
session boundary is worse than the tree moving during an audit. The
in-family pass therefore covers `28eb24b93..db2186cf1`; the two fixes that
followed it are covered only by their own class tests, and are named here
so the next round audits them explicitly.

---

## 1. Subject and coverage

**Commits.** `28eb24b93..db2186cf1`, five commits, all landed 2026-08-08:
`a96350e20` (P-1 + C-1), `0aaf53393` (C-2 + OOF-1, new package override of
`c47Extensions/keyboardTweak.c`), `d4a204d66` (C-3 and the P-2 family,
both owner rulings), `1212e4efb` (C-4..C-7, the record and the pins),
`db2186cf1` (DESIGN-HISTORY).

**Files.** `programming/manage.c` (the re-anchored sweep,
`forthFoldRederiveAdmission`, the `forthHistoryEnsure` injection hook, the
Live conversion at :1512), `ui/tam.c` (the top-of-function refusal, both
`tam.function` rewrite sites), `c47Extensions/keyboardTweak.c` (new
override), `screen.c` (`_forthConsoleMaxView` / `_forthConsoleClampView`
and the renderer's per-frame clamp), `forth_menu.c` + `forth_capture.h`
(the C-6 conversions and the corrected header comment),
`test_capture.part.h` + `test_console.part.h` (the new fixtures),
`design-audit.sh` (group I).

**Readers.** Eight in-family dimension finders (D1–D8) via
`audit-workflow.js`, blind to each other, findings through the three-lens
refutation in isolated worktrees. Out-of-family: **Gemini 3.1 Pro** on two
code packets (the fold sweep; the row-destroying calls) and **Sol/GPT-5**
on one design packet (the P-2 refusal). `dispatch.sh probe all` ran clean
before the round; identity was verified on every dispatch, and **one reply
was discarded for failing the MODEL check** and re-dispatched (§6).

**Deliberately not audited.** The standing open findings C5, C6, C7, C10,
C11, C13, C14, C15, C20, C22; the ruled items P1 (round 3), P2 (the push
ruling), F13/U5; round 6's and round 7's refuted items. None re-reported.

---

## 2. Mechanical results

Measured at the audited tip and re-measured at HEAD. **Gate GREEN** both
times, Forth self-test ALL PASSED including the new
`test_fold_round8_window` [1]–[8] and `test_console_render_view_clamp`;
upstream `meson test testSuite` 1/1 OK, 0 fail.

`design-audit.sh`: **4 chronic finding groups**, all of them the standing
overlay cost and none a finding here — A (override files 17 → **18**,
budget 16; added lines 2384 → **2656**, the new 1580-line upstream
override carrying two guards), B (no-Forth hunks 3 → 29, saturated by
D7-1's rename and now this override's brace hunk), D (inline blocks 16 →
37), E (the standing allocation prompt). **Group I — the new
enumerated-site pins — reports all nine ok at HEAD**, after three of them
were repaired for blindness (§4).

RULE-1 across the whole round: flash 1115008 → **1115344 = +336 B**; ram
8884 unchanged; arena untouched — no commit in this range touches the
dictionary.

Nothing the mechanical half reports is counted as a finding below.

---

## 3. Out-of-family findings

Gemini 3.1 Pro, two code packets. Both findings are against code committed
earlier the same day, both reproduced red-first, both FIXED in `58e07a2bd`.

**OOF-A — the P-1 fix resolved the capture step through a stale offset.**
`forthFoldCtx.capStepOffset` is an offset from `beginOfProgramMemory`:
stable against everything above it, stale against anything below. Deleting
a program that sits BEFORE FHIST slides the parked capture step down and
leaves the context's copy pointing elsewhere — and `forthCaptureResume`
recovers from exactly this, rewriting the CAPTURE's offset, while the fold
context's copy goes unrewritten. **Executed**: console open, DELP, spell a
program that precedes FHIST, ENTER — FHIST came back one step longer, the
owner's parked line stranded in it as debris. The unexecuted half is worse
and shares the root: the stale address can land on an ITM_FORTH string
step inside a USER program, which satisfies the opcode canary exactly,
after which the P-1 re-anchor aims the debris sweep at that program. The
re-anchor made the canary load-bearing for a decision it was never strong
enough to make. Fixed by `_forthFoldResolveCaptureStep` — offset first,
but the answer must be inside FHIST, else the same scan the resume uses.

**OOF-B — the C-2 guard was broader than the thing it protects.** It
skipped the pop whenever a console line was live, but the frame that pop
would destroy is only sometimes the console's. Push any other alphabetic
row over the console — which this same function's MyM.3 arm does, and
which is ruled benign — and the gesture that used to dismiss that row did
nothing: the overlay stuck. Fixed by pairing the Live predicate with
`forthConsoleBaseOnTop()` at both sites of the shape (the keyboardTweak
guard and its round-6 F7 twin in `screen.c`).

**Refuted or out of scope from the same pass.** `closeNim` as a third
frame destroyer — REFUTED on reachability: it needs `CM_NIM` with a live
capture, and the console is an AIM editor whose suspension leaves
CM_NORMAL. The unconditional cursor restore undoing GTOP navigation — the
PARK borrow-and-restore semantics, ruled in round 7 §6. The 4-iteration
sweep bound — round 6's F10 contract. One genuine UPSTREAM defect worth
filing separately: `keyboardTweak.c` reads `indexOfItems[item]` before the
`item > 0` check, so a menu id assigned to the long-press key is a
negative array index.

**Sol (GPT-5) on the P-2 refusal design: no defect.** The refusal is "the
only demonstrated choice that preserves the console line and avoids an
invalid capture state"; the placement is "the strongest available one";
the predicate "matches the dangerous seam precisely". Two named
dependencies rather than findings: `tamEnterMode` is now fallible without
communicating failure to callers (no concrete broken caller constructible
from the packet), and error dismissal must restore the editor
presentation. Both are round-9 checks, named in §8.

---

## 4. What the in-family pass found, and what was done about it

Full detail in `AUDIT_round8_in-family_2026-08-08.md` (R8-1..R8-10,
R8-P1). The headline: **four in-family readers independently found OOF-A's
root**, which the out-of-family pass had already fixed — and the round's
real news sits beside it.

- **R8-1 and R8-2, both at ONE call** — `forthFoldLeave`'s cursor restore,
  the third consumer of a quantity sampled across the PARK dispatch, and
  the one the P-1 fix exempted in a single clause. R8-1: a program NUMBER
  cached across a dispatch that deletes programs, executed to a wrong
  cursor and, at the boundary, to a **SIGSEGV**. R8-2: `goToGlobalStep`
  silently does not navigate while `dynamicMenuItem` is latched by the
  softkey that committed the TAM. Both FIXED in `c106008de` — the crash by
  clamping, R8-2 by the bracket this tree already applies at two other
  navigations. **R8-1's wrong-program half is also CLOSED** — the owner
  ruled the standing rule rather than the case: *follow upstream's
  conventions when possible for fix decisions*. Upstream's convention was
  in the same file all along (the deleter adjusts every saved cursor;
  `fnClP` renumbers its own), so `_clearProgram` now maintains the fold's
  cursor by the same rule at the same moment, and test [8] tightened from
  printing the mismatch to asserting identity (`c17c0f3`-series).
- **R8-3/R8-4/R8-5 — D7-a recurring inside its own countermeasure.** Three
  of group I's pins were mutation-proven blind: the frame-destroyer pin
  was anchored to `^ +`, and a third destroyer left the pin AND the full
  gate green; the upstream-consumer pin counted FILES, so a new consumer
  in any of the five already counted never moved it. Both now count call
  sites. R8-5's justification was false on the nested-TAM arm, where
  nothing re-derives admission for the new function — inert today, now
  recorded at the pin. FIXED in `c106008de`.
- **R8-6..R8-9 — four defects in this round's own tests and records**,
  fixed in `59147c93e`. The one to keep: the C-7 guard rewrite reproduced
  C-7's own vacuity one commit later.

---

## 5. The honest negatives

Three, because a round that reports only what it fixed is not a report.

**The P-1 fix is half mutation-provable.** A subcase written to pin its
second door passed with the re-anchor MUTATED OUT. Two gestures were
tried; a probe showed the drive never reaches the state it claims. The
subcase was DELETED rather than shipped. The canary gate is what closes
the executed door; the re-anchor is defensive, and no mutation available
today reddens it.

**R8-1's wrong-cursor half was written off too early, and the owner's
standing rule caught it.** I fixed the crash at the restore site, proved
that site cannot know which program was deleted, and recorded the rest as
an unfixable gap — without checking how upstream solves the same problem.
It solves it in the same file: the DELETER adjusts every saved cursor.
Applying that convention closed the gap the same day. The general lesson is
now a standing rule: a workaround is only correct when no upstream
convention covers the case, and "the site I happened to be editing cannot
do it" is not evidence that none exists.

**This round is not findings-only.** The tree moved seven times during it.
Every fix carries a red-first reproducer, and `58e07a2bd`, `bdbfffeb1`,
`c106008de` and `59147c93e` are UNAUDITED — they are round 9's explicit
subject.

---

## 6. Process — what this round taught, and where it is encoded

The growth rule: a new packet-defect class becomes a linter check before
the next round; a new reader-pool trap goes into `dispatch.sh` or the
skill.

**Seventh packet-defect class: a fence glued to its content.** Packets
assembled by concatenating extracted function bodies got their closing
fence appended to the last brace — ` }``` ` on one line, which Markdown
does not treat as a fence at all. With ONE code block the linter said "no
code fence" and blocked dispatch; with FIVE, the openers paired with each
other, the check passed, and the packet that went out had its task section
inside a code block. That packet was killed mid-flight and re-sent.
`packet_lint.py` now HARD-fails a fence marker that does not own its line,
and an odd number of fence markers.

**The MODEL check earned its keep again.** Gemini's first reply to the
second packet opened with prose instead of the `MODEL:` line;
`dispatch.sh` discarded it unread and the packet was re-dispatched, at
which point the same reader produced three findings, one of them a real
defect. The rule cost one re-run and bought a verified-identity finding —
the alternative was discarding good work or trusting an unverified reply.

**A fixture that could not reach its state, caught by its own mutation.**
A subcase written to pin P-1's second door (a live navigation inside a PARK
fold aiming the sweep at another program) passed with the fix MUTATED OUT.
Two gestures were tried; a probe showed the drive never reaches the state
it claims — `GTO . 3` errors in `fnGoto` before GTOP ever navigates. The
subcase was DELETED rather than shipped, and the honest residue is recorded
in §5 as a documented gap: the P-1 fix has two halves, and only the canary
half is mutation-provable.

---

## 7. Verdict

**Would I ship it? Closer than any round so far, with one caveat that is
about process, not code.** Round 7's worst was a silent wrong-result on
five keypresses; round 8's worst was a SIGSEGV, and it is fixed, as is the
wrong-cursor half beside it. The caveat: five commits of this round are
unaudited (§8), and they are fixes to fixes.

**Where it breaks first.** Nothing known in the fold window. The last open
behaviour — `FORTH → DELP → <a program before the cursor's> → ENTER`
returning the PEM cursor one program off — closed when the fix moved to
the deleter, where upstream keeps its own.

**The pattern, eighth round running, and it has moved.** For seven rounds
the shape was "a fix that enforced its own rule on an enumerated subset of
the rule's sites". Round 8 is the same shape one level up: the
countermeasure for that class — group I's counted pins — was itself
enumerated by hand, and three of its nine pins were blind. The lesson is
not that pinning fails; the navigation pin caught its own author's
miscount on its first run. It is that **a pin is code, and code written at
peak confidence in exactly the spot the blind spot is widest.**

---

## 8. Round and exit state

**Exit criterion: NOT met, and reset.** Round 8 produced new CONFIRMED
findings — two out-of-family, two behavioral in-family including a crash —
so the count resets. The earliest close is **round 10**, and the round
that closes it must include an out-of-family pass on the actual fix
commits.

**Round 9's subject, in order:**

1. **The four unaudited commits** — `58e07a2bd`, `bdbfffeb1`,
   `c106008de`, `59147c93e`. They are fixes to fixes, which is the highest
   prior this project has.
2. **Sol's two named dependencies** (§3): every caller of `tamEnterMode`
   is either a terminal dispatch or tolerates it returning without
   entering TAM; and ERROR_RAM_FULL's dismissal restores the live
   console's editor presentation. Both are repo-checkable.
3. **R8-P1**, PLAUSIBLE and unconstructed: the same `dynamicMenuItem`
   divert defeating `forthFoldEnter`'s park onto FHIST, which would
   materialise the capture step in the caller's program.

**Owner rulings owed:** none new. R8-1's remaining half was ruled the same
day, as a standing rule rather than a one-off — *follow upstream's
conventions when possible for fix decisions* — and closed under it.
Carried unchanged: P1 (round 3), P2's push ruling, C22-vs-C1, F13/U5.

**Standing items unchanged:** the DM42n hardware pass for stages L/M/N,
the merge to main (110+ commits), the FIX-6B MR push, the leak-report
filing, and the pre-round-6 open findings C5, C6, C7, C10, C11, C13, C14,
C15, C20, C22.
