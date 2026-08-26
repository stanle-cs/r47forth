# undo-history — cross-model audit round 4 (2026-08-26)

Subject: the round-3 deltas (436f3b1c8). Readers GPT-5 + Gemini 3.1
Pro High, identities verified. ONE finding, convergent (both readers,
same site, same shape), fixed same-day red-first.

## Confirmed (1)

- R4-1: E3's gap-abandon ruling was spelled per-site — RestoreLevel
  cleared historyGapPending on success but the OTHER restore sites
  (undoHistoryStepBack, fnRedo) did not, so an undo/redo walk after an
  oversized-live skip left the stale flag and the next capture wore a
  spurious ~. The named bug class from the catalog: structural rule
  spelled per-site. FIX: the clear moved into historyRestoreToIndex's
  success path — the funnel every restore goes through — and the two
  caller-side clears were removed. Pin R15 (red-first: buffer-undo then
  ring-step-back then capture; the stale ~ appeared, now cannot).

## Exit state

Round 4 had a finding: the counter resets. Rounds 5 and 6 must both be
clean (round 5 audits the funnel delta + R15). The delta is one moved
statement; the surface is as small as it has ever been.
