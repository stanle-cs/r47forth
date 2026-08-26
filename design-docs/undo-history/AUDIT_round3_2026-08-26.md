# undo-history — cross-model audit round 3 (2026-08-26)

Subject: the round-2 fix deltas (47f0c37a3). Readers GPT-5 + Gemini 3.1
Pro High, identities verified. The operator posed the merged-top gap
mark's direction as an explicit open question; BOTH readers ruled it a
defect, convergent with each other and with two further findings each.

## Confirmed (3 composites), all fixed same-day red-first

- R3-1 (convergent, the big one): merging a gap-separated equal state
  collapsed two distinct temporal occurrences into one level — the ~
  pointed at the contiguous P->Q boundary while the real loss sat
  above, and the first undo landed one level too deep. R13 v1 had
  PINNED the buggy behavior. RULING: a state equal to the top but
  separated from it by a gap is NOT a duplicate — the push now
  suppresses dedupe when GAPBEFORE is incoming, the anchor pushes
  distinctly with the ~ pointing the right way, (now) survives, undo
  lands on the last recorded state. R13 rewritten to pin the SEMANTICS
  (red x4 against the merged representation, green after); the round-2
  merge-keeps-gap line and the result-2 gap branch removed as
  unreachable.
- R3-2 (Sol): the restore-failure cursor repair inferred "pushed" from
  entry-count growth — an evict-plus-push keeps the count equal (or
  shrinks it), so the cursor fell to live and a retry re-minted and
  merged with the stranded anchor. FIX: push detection by TOP SEQ
  change. Trace-verified (failure injection not enumerable).
- R3-3 (Gemini): a retry after a failed live restore enters the
  mid-trail path and skipped the gap-abandon clear. RULING extended:
  ANY successful restore abandons the un-captured live continuation.
  FIX: the mid-trail path clears historyGapPending on success too.

## Process lesson (growth rule)

R13 v1 enshrined its fix's behavior as the expected outcome — a pin
must encode ruled SEMANTICS, not echo the implementation it was written
beside. Posing the operator's own doubt as an explicit packet question
converted a hunch into two independent confirmations in one pass.

## Exit state

NOT closed: round 3 confirmed findings (in round 2's fixes — the trap
is now 3 for 3 across waves). Round 4 audits the round-3 deltas
(no-dedupe-across-gap, seq-based push detection, universal
gap-abandon, R13 v2). Two consecutive clean rounds close.
