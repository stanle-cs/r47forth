# undo-history — cross-model audit round 5 (2026-08-26)

Subject: the round-4 delta (390b994d1) — the funnel-owned gap-abandon
plus pin R15. Readers GPT-5 (Sol) + Gemini 3.1 Pro High, identities
verified. The round's question deliberately rotated to the FAILURE
side of the restore contract; that axis surfaced a U1-era defect all
four feature-side waves had missed.

## The delta itself: CLEAN, twice over

Both readers independently confirmed the round-4 fix. Gemini traced
"gap pending while cursor != NONE" to structural unreachability
(every cursor-setting path clears the gap first or requires it
false); Sol confirmed clear-on-success is right at all four funnel
call sites, that the plain buffer undo correctly BYPASSES the clear
(going back along the live branch is not abandoning it), that R15 is
red under the complete old code, and that R14 still pins the
live-branch side.

## Confirmed (2)

- R5-1 (Sol, LOW, coverage): the funnel's failure-side contract —
  the pending gap SURVIVES a failed restore — was implemented but
  unpinned; no test in the battery could reach the failure returns at
  all (nothing ever fills the pool; only R8 reads it). Verified
  statically, then by the R17 red run (the failure path executed for
  the first time and immediately caught R5-2). Class: success-only
  coverage of a two-sided contract. FIX: pool-hoard helpers + pin R16
  (slot-0 failure keeps gap, cursor, live state, and the armed
  buffer; freed-memory retry succeeds and abandons the gap — giving
  R2-3's trace-only repair its first executable coverage).
- R5-2 (Gemini, operator-completed, Sol cross-refutation CONFIRMED;
  MEDIUM, functional): a restore failing MID-STAGING leaves SAVED_*
  torn (slots below the failure hold entry payloads, the rest pre-op
  state); the browser discards the failure and leaves
  thereIsSomethingToUndo armed, so a later fnUndo consumes the torn
  bank via plain undo() — the user lands on a state that never
  existed. Sol's sharpening: with ERROR_RAM_FULL still pending,
  noteFirstUndo's own error guard returns false immediately, making
  the consuming path MORE direct. The sums-alloc failure is the same
  class with everything staged and nothing torn — still not the armed
  pre-op state. Class: torn staging left consumable. NOT a fix-wave
  regression: the staging loop is U1-original — the fix-trap streak
  ends at four. FIX: funnel-owned retirement — on any failure after
  the first mutation (slot > 0, or the sums step) the buffer is
  retired (thereIsSomethingToUndo = false); a slot-0 failure mutates
  nothing and keeps the user's undo. Transactional staging was
  considered and rejected: upstream has no transactional restore
  anywhere (upstream undo() itself tears the LIVE state on the same
  failure), and rebuild-the-bank is the relocating-state fix shape
  the audit's own regression data warns against. Pin R17 red-first
  (the torn bank was consumed — X took level-0's value; now the press
  step-backs through the ring instead).

## Refuted / noted

- Gemini's original fnUndo-based reaching sequence dies on both
  fnUndo paths (thereIsSomethingToUndo ends false after stepBack);
  the browser variant is what survives — recorded as one more case of
  a real defect behind a wrong trace.
- fnRedo's funnel clear guards a structurally unreachable state —
  harmless belt, ruled correct by r3 regardless.
- The upstream testSuite.c sprintf format-overflow warning is
  pre-existing (56 hits in the clean-gate log) — mechanical half,
  not a finding.

## Exit state

Round 5 had confirmed findings: the counter resets. Rounds 6 and 7
must both be clean (round 6 audits the r5 deltas: the two retirement
sites, the hoard helpers, R16/R17). Process growth: rotate the
QUESTION AXIS across rounds, not just the readers — four rounds asked
"is the semantics right?"; the round that asked "what happens when it
fails?" found a wave-0 bug.
