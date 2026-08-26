# undo-history — cross-model audit round 6 (2026-08-26)

Subject: the round-5 deltas (ea2a7ced8) + the sums addendum
(a0f10a4c2) — the two retirement sites, the pool-hoard helpers, pins
R16/R17/R18. Readers GPT-5 (Sol) + Gemini 3.1 Pro High, identities
verified. Packets carried the sizing constants multiplied out
(template rule 5, paid for by the addendum).

## Confirmed (1, documentation)

- R6-1 (Sol): the funnel comment's absolute claim — "the bank no
  longer describes any state that ever existed" — is falsified by a
  reachable coherent-by-luck case: a staged slot can write bytes
  equal to the pre-op bank (entry slot byte-identical to SAVED), and
  Sol constructed a sequence where the retired-but-coherent buffer
  was the only path back to an oversized, never-ringed pre-op state.
  Class: absolute claim contradicted by a reachable case. RULED, not
  reworked: the behavior stays conservative — exact-change tracking
  would be new failure-path code whose own defect points the
  dangerous way (silently KEEPING a torn bank); over-retiring loses
  at most one undo level in a RAM-full corner, under-retiring
  re-opens R5-2. The asymmetry is the ruling. FIX: the comment now
  states the conservatism and the ruling; DESIGN.md carries the
  asymmetry argument; TESTING.md's R18 paragraph now demands BOTH
  a genuine-change pin and a coherent-by-luck ruling pin the day the
  sums constants change. No functional delta.

## Refuted

- Gemini's "post-retirement UNDO lands on a future state": version
  misattribution (the round-9 lesson — ask which version the finding
  is true of). Every constructible variant of the sequence has the
  buffer already SPENT at the failed restore (their own step 1
  consumes it), so the retirement is a no-op and pre-r5 code walks
  the identical path. The conjunction the trace needs — cursor NONE,
  live historically old, buffer ARMED — collapses pairwise: an armed
  buffer implies a fresh saveForUndo, which makes the minted anchor
  mirror live and anchor-1 the most recent PAST capture; the
  step-back lands one step back, correctly. The underlying
  anchor-on-top layout is r1-era ruled design (it is what makes
  browser jumps redoable), and the walk-from-NONE-targets-newest
  fallback is documented cursor algebra.

## Independently re-confirmed by both readers

Retirement unreachable in ordinary (non-RAM-full) use; fail-retry
chains idempotent (i == 0 on a retry safely leaves a retired buffer
retired and a rebuilt buffer armed); each retirement site's mutation
caught by exactly one pin (i>0 -> R17, gap-clear-on-failure -> R16,
retire-at-0 -> R16's armed assert, sums-site -> dormant per the R10
precedent + R18 tripwire); hoard helpers terminate, cannot overrun,
cannot corrupt the pool, and a passing pin cannot lie about an
incomplete hoard (the RAM_FULL assert is the reached-state proof);
R18's direct sums allocation supplies exactly the state the capture
reads; the sums-session walk (skip -> plain undo -> browser restore
dropping live sums through upstream undo()) is coherent.

## Exit state

Round 6 is FUNCTIONALLY clean — the first round with zero functional
findings on its subject. R6-1 is documentation-class and its fix is
comment+docs only, so round 7 audits a prose-only delta. By the
letter of the exit criterion the counter reset; rounds 7 and 8 close
the audit if clean. Process growth: nothing new this round — the two
judgment calls (version attribution, dormant-code treatment) applied
rules earlier rounds already encoded; Sol's future-pin demand is
captured in TESTING.md.
