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
