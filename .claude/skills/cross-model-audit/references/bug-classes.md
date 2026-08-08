# Named bug classes — the catalog the audits paid for

Every class below was named under the standing fix rule (reproducer + named
class + class test where enumerable) or by an audit round. Finders use this
two ways: hunt each class at ALL its sites, not just where it was first
found — the classes recur (`isAlphaSubmenu` was re-widened short twice, the
last-qualifying-entry loop shape appeared at a second site after the first
was fixed and commented) — and when a new bug is fixed, check whether it is
one of these before minting a new name. New classes are appended here AND
recorded in DESIGN-HISTORY.md; the round tag says where the evidence lives.

## Ownership, lifecycle, state

- **Persistence-contract mismatch** (r3 R1): state relocated into a carrier
  that is saved/restored differently than the one it left. The most
  dangerous fix shape on record; diff both homes' contracts (persistence,
  reset seams, other writers) whenever state moves.
- **Ownership inferred from visible identity** (C16/C17): who owns a
  deduplicated/persistable resource must ride WITH the resource (a stamp),
  not be inferred from a value two owners can legitimately hold.
- **Shared-body open, caller-side re-establishment gap** (r1/r2): an open
  that resets N fields and delegates re-establishment to callers; M<N
  re-establish, and nothing counts the call sites.
- **Close path bypassing the funnel** (r3 R2; D-C2): a teardown arm that
  hand-clears some fields but skips the single full-cleanup funnel. Class
  test shape: poison every close site.
- **Push-then-decline window** (C18, reproduced by its own fix in r5):
  caller commits a state flip, then calls a function entitled to do
  nothing; the committed state and the surface disagree.
- **Second-exit bypass** (r5 R1): the fix moved the unwind to the one place
  "it cannot be bypassed"; another early return in the same function still
  bypasses it. Count every exit reachable under the guarded condition.
- **Destructive re-entry** (C6): a state-establishing entry point with no
  already-open guard discards live state when invoked over itself.
- **Durable invisible cross-mode side effect** (r5 R1/R2): an armed flag or
  mis-typed record survives the gesture that should have cleared it and
  corrupts a later, unrelated operation in another mode.
- **Global-flag leakage across a dispatch boundary** (D1, FLAG_ASLIFT): a
  flag toggled for one caller's local effect leaves the steady state wrong
  for the next unrelated caller.
- **Ephemeral allocation vs wholesale restore** (F6-6): an allocation whose
  lifetime is shorter than a save/restore cycle orphans the allocator's
  bookkeeping on every round-trip.
- **Stale mirror** (r5): a live buffer edited without re-syncing its
  persisted/on-disk twin; every consumer of the mirror reads stale data.
- **Buried frame without a drain** (r1 C2 family): every code path that can
  bury a menu/frame needs a matching drain, not just the creation path.

## Predicates and guards

- **Origin-vs-openness confusion** (r5): a predicate answering "what kind
  is this" read as "is this live now"; the answers coincide except exactly
  where the bug lives. Classify every call site by which question it asks.
- **Predicate widened for one consumer, others unchecked** (r1 N-T4, again
  r5 R3): widening a shared predicate requires re-grepping the FULL
  consumer set; the last two re-derivations each came back one short.
- **Predicate re-derived, paired side effect not** (r1 C4): the conditional
  was corrected for new ground truth; the call that only made sense under
  the old truth was left beside it.
- **Scope-mismatched predicate pair** (D-C3, "trap #6's exact shape"):
  top-of-stack test in one place, whole-stack scan in another, same
  resource; they disagree and both have dependents.
- **Single-owner contract with untracked writers** (r5 R4): declaring one
  function the sole writer does not make it so; sites predating the
  declaration keep writing.
- **Safety proof scoped to one caller, reused by others** (r5): the
  conservatism argument held for the rung it was written for; C18's new
  consumers inherited the predicate without the proof.
- **Shared guard for two independent repairs** (r3 R3): the surface repair
  gated on the mode repair's condition; `EXITALL` satisfied one and not the
  other, invisibly.
- **Exemption inherited without its paired clear** (r5): keys exempted from
  a cleanup because they ARE the dismiss gesture in the native context; a
  new caller inherits the exemption list without the clears.
- **Hardened arm, exposed sibling** (r4→r5 P-B/G2): independent readers
  converged on one arm of a two-armed decision; it was hardened; the
  complementary arm four lines below carries the same shape untested.
- **Correct only by callers' good behavior** (forthOwningProgramStart, then
  the picker builder — same shape, second site after the first was fixed
  and commented): correctness resting on an unstated invariant (sortedness,
  in-bounds inputs) that nothing asserts. Grep for the shape everywhere
  when one site is fixed.
- **Guard that can overestimate** (D3): a resource guard must only ever
  underestimate, or it fires falsely on correct programs; and a counter
  scoped to a context must not accumulate outside it (context-leaked
  counter).

## Boundaries and encodings

- **Silent narrowing before a range gate** (C20): truncate-then-check
  accepts any wide value whose low bits land in range; the class test needs
  a magnitude whose truncation is in-range.
- **Orphan byte at an atomic boundary** (C10/C11): a multi-byte glyph or
  unit accepted or cut at a byte boundary.
- **Byte-wise idiom on glyph-wise content** (r5 R5/R6): a separator or
  cursor idiom correct for ASCII misreads a two-byte glyph whose second
  byte is 0x20; the idiom propagates by copy-paste.
- **One encoding of the violation guarded, others open** (r1 C10): the
  guard rejects the obvious encoding of the unrepresentable condition;
  equivalent encodings pass.
- **Unbounded derived counter inside a bounded scan** (G2 picker): the
  documented scan cap does not bound the per-distinct-name slots the scan
  feeds; every allocation downstream of a capped loop needs its own cap.
- **Hang instead of miscount** (Stage N ring; found BY a mutation that
  refused to go red): walks over corruptible structures need an iteration
  cap as well as the natural predicate — on this device a hang is strictly
  worse than a wrong answer.
- **Display setting read as model quantity** (C7): `.S` read a UI row
  setting where the engine's own depth accessor was required.
- **Clamp without its viewport** (C12): a bound computed in a module that
  architecturally excludes the consumer-size it clamps for. Layering
  ruling, not a patch.
- **Non-injective rendering claimed unambiguous** (architect R6): the
  injectivity claim was checked against the one case its author had in
  mind; sweep the full payload space.
- **Emit/accept parity violation** (D-C1, FIX-7): the decoder renders a
  spelling the compiler refuses. Class test: round-trip sweep over every
  decodable form. Fix on the ACCEPT side when the emitter is a shared
  rendering convention.

## Tests and oracles

- **Fixture primes what the subject derives** (four PEM defects reached
  hardware green): hand-set catalog/tam state proves nothing about the
  derivation path. Drive the real dispatch.
- **Fixture fakes the state** (four repaired in one session): forcing
  `keysMode` or hand-pushing rows instead of driving the gesture. A fixture
  must assert it REACHED the state it claims to test.
- **Oracle where the mechanism cannot reach** (C21/C22): the canary sat on
  a buffer no producer writes; the real backstop was the simulator's stack
  protector, which the device build does not carry.
- **Assertion after the epilogue reset** (C13): the counter is zeroed by
  the code under test before the assert reads it. Sample inside the run.
- **Lenient-fallback oracle** (r3): an identity fallback answering "true"
  for unstamped state manufactured three false failures. Wrong oracles
  fail in both directions.
- **Upper-bound-only oracle** (r5 R9): "at most N" is satisfied by zero;
  deleting the only registration call left the whole gate green. Assert
  the lower bound wherever the design guarantees one.
- **Message/body mismatch** (r5): the PASS text claims exact tracking; the
  body checks `!=` against one value. When one of two symmetric assertions
  is strengthened, check its twin.
- **Fixture sized from the constant under test** (G2, twice): both sides
  of the comparison move together; and a tighter unrelated limit can bite
  first, silently doing the test's work.
- **Wrapper-only coverage** (D3-5): the battery drove the bracketed
  wrapper; the user-reachable unbracketed entry left the guard and spill
  dead code. Every reachable entry point gets its own pin.
- **Green for the wrong reason** (architect R6): the test's pass is a
  coincidence of unvalidated fixture data, not the guarded property. Ask
  what would make it pass wrongly; prove with a mutation.
- **Enumeration without a count check** (r2, three instances): a human
  list of call sites/arms/consumers not backed by a build-time count is a
  comment, and it comes back short.

## Documentation and citations

- **Decorative citation** (architect R1: 13 of 73 FALSE): a
  `[VERIFIED: file:line]` that proves a narrower fact than its sentence is
  FALSE, and it propagates because it is trusted.
- **Absolute claim never executed** ("never nests" — contradicted by three
  passing tests): every "always/never/cannot" about runtime behavior is a
  hypothesis until someone runs it.
- **Stale "not yet implemented"** (8 instances found in one pass): design
  text describing landed code as future work is an instruction to re-apply
  already-landed work.
- **Invariant living only in prose** (r3 R4): a false invariant written in
  three places survived a whole session; prose is not enforcement — export
  a census and assert after every step of the gesture sweep.
- **Comment that outlived its mechanism** (r5 R13): the cited mechanism was
  deleted in a rebase; the conclusion still holds for an unstated reason,
  and the comment is now false evidence for a true claim.
- **Rule corrected in a subset of its copies** (r5 R12): the fix added the
  corrected clause and left the stale one standing six lines away.
- **Constant copied by value across a module boundary** (C14): hand-copied
  geometry with nothing forcing agreement; pin with `_Static_assert`.
- **Hand-maintained inventory of a machine-derivable set** (C15): the
  override list drifted in both directions; the checker should diff the
  computed truth against the doc.
