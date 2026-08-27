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
- **Torn staging left consumable** (undo-history r5, U1-era, found by a
  failure-axis round): a multi-slot write into a shared buffer aborts
  mid-way; the failure path repairs bookkeeping but leaves the
  half-written buffer indistinguishable from a valid one to its
  consumer (an armed thereIsSomethingToUndo + a later plain undo()).
  Hunt: every early return inside a loop that mutates shared state —
  ask who reads that state believing a flag that predates the loop.
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
- **Saved cursor tuple with an unmaintained half** (r9 R9-1/R9-2, one field
  over from r8 R8-1): a remembered `(program, localStep)` pair whose
  program half is maintained by the deleter and whose step half is
  restored raw into a program that may have shrunk. The consequence is not
  a wrong cursor but a NULL one, because the navigation walk it feeds has
  no bound. Close such a class one TUPLE at a time, never one field at a
  time, and drive EVERY mutation possible between save and restore
  (deletion, eviction, insertion) rather than one door per site.
- **Structural rule spelled per-site** (r9 R9-5): the same structural
  invariant written independently at each consumer, over separately stored
  copies of the quantity it constrains. Four confirmed defects across two
  rounds came from the consumer that still had the raw test after the
  others were given the rule. One predicate, plus a pin holding it at one
  definition, so the NEXT consumer inherits the rule instead of restating
  it.

## Predicates and guards

- **Guard placed below the state-consuming read it guards** (r9 R9-3): a
  "this gesture does nothing" contract enforced after the code that
  already read and dropped the state. Fixed by moving the READ, not the
  guard, when the guard has its own ordering constraint. A no-op contract
  is checked against every piece of state on the path, not only the one
  the finding named.

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
- **Success-only coverage of a two-sided contract** (undo-history r5
  R16): a contract with distinct success and failure obligations whose
  failure side no fixture can even reach (here: nothing in the battery
  ever filled the pool, so every RAM_FULL return was unexecuted). The
  first fixture to execute the failure path immediately caught a
  second, functional bug sitting on it. Hunt: grep the subject's error
  returns, then ask which test produces each error.
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
- **Test asserting the absence of one wrong answer** (r9 R9-4, C22's class
  one level up): a fixture that checks `!= theOneWrongThingIThoughtOf`
  passes on every OTHER wrong answer, so it can DRIVE a live defect and
  report success on every run. Assert the positive property the design
  states (here K-R3, "the row is the mode indicator"), not the absence of
  the instance that happened to come to mind.
- **Doc naming a symbol its tree does not have** (r9 R9-8): path checks
  pass while the authoritative doc names a DELETED function as the live
  mechanism, so a maintainer goes looking for a guard that is not there —
  or re-adds a site-local one and forks the funnel the deletion built.
  Check symbol liveness, not only paths; the check found seven more of its
  own class on its first run.
- **Stale load-bearing narration after relocation** (r9 R9-9): a verbatim
  move carries a comment whose premises the same function refutes sixty
  lines above. The code executes correctly and the stated reasons are
  dead, which is worse than no comment in the one place — the highest-
  regression function — where a reviewer most needs to trust one.
- **Consolidation gating a restore on a different field** (r10 R10-1): a
  shared restore merged two sites' unconditional per-field assignments and
  gated one field's restore on a predicate about ANOTHER field of the same
  tuple — "the second navigation is redundant" read as "the tuple is
  invalid". Class test: per-field round-trip identity with no intervening
  mutation, for every field of every consolidated tuple.
- **Range test standing in for an identity test** (r10 R10-OOF-1, third
  turn of the unmaintained-cursor class): a saved index restored on a
  validity check where the tuple promises a position — eviction from the
  front renumbers everything after it, so a number that still FITS names a
  different element. The deleter renumbers the saved index (upstream's
  own convention); the fixture asserts payload identity, not bounds.
- **Two-half gesture with the second half conditional** (r10 R10-2): a
  dismiss-then-land translation whose land half only fires in the state
  the dismiss half establishes at depth 1. Parameterise the fixture over
  depth and assert the landing property at every depth.
- **Gate consuming output its producer never emits** (r10 R10-3/4/5): a
  check grepping a tag the tool does not print, a liveness test satisfied
  by comments, a pin counting one spelling of its subject. A new gate
  lands with a red-first injection of the class it exists to catch, or it
  does not land.
- **Serialisation that claims byte-fidelity and omits a header field**
  (r11 R11-IF-1, the worst of the round): the D3 spill record is
  `[dataType][sizeInBlocks][payload]` and DESIGN.md calls it a
  "byte-faithful register image", but `registerHeader_t` also carries the
  TAG — which for a long integer IS the sign, for a short integer the
  base, and for a real34 the angular mode. Both readers restored `amNone`
  (5), so `LI_NEGATIVE` (1) never matched and a spilled negative came
  back positive, silently, inside `ERROR_NONE`. Sibling: C11's orphan
  lead byte. Class test: round-trip EVERY header field through the
  serialiser for one value of each type that uses the field — and pick
  fixtures the defect can reach, because five rounds missed this one on a
  fixture set of plain reals and positive integers, for which the wrong
  restore is indistinguishable from the right one.
- **A user-facing control expressed as an item id, resolved by upstream's
  key-translation stack** (r11 R11-IF-2/3, and D7-a): the N-R10 controls
  synthesize `ITM_SPACE` into `processAimInput` and match on `ITM_RS`.
  That stack rewrites items by plane, by shift, by NUMLOCK, by CAPS and
  by press duration — so ENTER types `+` with NUMLOCK on, and R/S types
  `?` in the alpha plane where key 84's `primaryAim` is
  `ITM_QUESTION_MARK`. A control is a GESTURE, not an item: pin it
  against every plane and modifier the translator applies, or bind it
  above the translator.
- **The harness enters below the layer where the bugs are** (r11, the
  round's most useful observation, reached independently by both legs):
  every console test starts at `processKeyAction(<item>)` or calls the
  orchestrator directly, so no test can see a defect introduced by key
  translation, which happens earlier. A green suite over a control's
  behaviour proves nothing about the control's REACHABILITY by the
  gesture the documentation advertises. When a fixture synthesizes the
  item, name in the test what layer it is skipping.
- **One-door seam** (r11 R11-1, confirmed out-of-family with a
  screenshot): a guard installed at one of two entry doors, where the
  same file already installs a DIFFERENT guard at both and each of its
  comments names the other door. The cap got it right; the opening
  refresh did not. Ask of every new guard: what is the complete list of
  doors into this surface — and keep that list somewhere better than a
  comment per site.

## save-test narrower than the save (pretty-print audit r1, 2026-08-27)

A guard decides whether to snapshot state by asking a question that is
STRICTER than what the snapshot can actually hold, so the state that falls
in the gap is modified with no way back.

Found: `ppEqDelegate`'s caller tested `getRegisterAsRealQuiet(var)` before
`saveRegisterSnapshot(var)`. The snapshot handles `dtComplex34`; the test
refuses a complex with a non-zero imaginary part. A loop variable holding
7+4i therefore took no snapshot, was overwritten by the loop counter, and
was never restored — the owner's value destroyed by a sum that had no
business touching it. The idiom was copied from `differentiate.c`, where
the same narrowness is harmless because that engine needs a real point
anyway; copying an idiom carries its preconditions with it.

Hunt it at: every `if(canConvert(x)) { save(x); } ... modify(x)` pair. Ask
what the SAVE covers, not what the test converts. Where the save covers
nothing, refuse rather than clobber.

## stale global read as this call's verdict (pretty-print audit r1, 2026-08-27)

A status global set by someone else, at some earlier time, is read after a
call as though it described that call.

Found: `ppEqDelegate` read `engineNestingWasRefused` to decide whether the
engine it had just invoked had refused. Only the integrate branch cleared
it first, and NOTHING on the derivative path clears it —
`solve.c:46` holds the tree's only assignment of `false`. So one earlier
refused nesting left the flag standing and every later `DERIV` construct
discarded a correct answer as "refused", until an unrelated integral or
solve happened to clear it. `programRunStop` had the same shape and was
additionally overwritten on the way out.

Hunt it at: every read of a firmware status global after a call. Establish
it before the call or do not read it after. And a global you did not set
is not yours to leave changed — snapshot and restore.

## result snapshot taken on the wrong side of the dispatch (pretty-print audit r1, 2026-08-27)

An idiom whose precondition is "a register still holds this value" is
reused at a site where the dispatch has already run, so the snapshot
records the NEW value as the old operation's result.

Found: every emit-with-register call in `prettyCapture.c` runs at STAGE,
before `indexOfItems[func].func(param)`. `ppcInvalidate(true)` runs at
DONE, after — deliberately, because the dispatch may error. It read
`REGISTER_X + k` anyway, so `2 ENTER 3 . 7 +` then `IP` filed
`2 + 3.7 = 5.` permanently, and the browser recalled that 5. Confirmed by
probe. The comment above it asserted the false premise as fact.

Hunt it at: every reuse of a "read the live register" idiom. Ask which
side of the dispatch THIS call site is on. Where the value has already
left, the truthful form is to record no result at all.

## the capture engine trusts a register to still mean what it meant (pretty-print audit r1, D7-8)

The umbrella class the audit named for five separate findings. The shadow
stack's invariant is about VALUES, and every classifier exception written
in terms of MOTION ("the stack does not move") leaves a value hole.

Instances: `STO Y` overwrites a register the shadow still describes with
its old tree; `US_CANCEL` items (LOAD/LOADST) replace the whole register
file while the default rule waves them through as harmless; `R/S` resumes
a program that rewrites everything with each step out of scope.

Hunt it at: every hand exception in a classifier. Ask "what VALUES can
this item change?", never "does the stack move?". Note that in all three
cases the binding DEFAULT rule would have been safe — the hand exception
is what created the hole.

## unsigned cast swallows a negative coordinate (pretty-print audit r3, 2026-08-27)

A drawing primitive takes unsigned coordinates. A negative value cast to
unsigned becomes enormous, so the shape lands nowhere and is dropped
WHOLE — while a neighbouring primitive that clips properly keeps drawing.
The result is a partially-rendered picture that looks deliberate.

Found: every rule this engine paints (fraction bars, radical vinculums,
|x| strokes, tall parens, the Sigma/Pi/integral glyphs) is an
`lcd_fill_rect` with `uint32_t` coordinates. The browser's horizontal pan
paints from a negative origin, so a panned row lost its fraction bar
while keeping every digit: measured 1243 lit pixels with one solid run
unpanned, 910 and NO solid run after one pan step. Glyph ink survived
because `showString` clips.

Hunt it at: every mixed-signedness drawing call, and anywhere a paint
origin can go negative (scrolling, panning, centring a too-wide object).
Clip once in a wrapper, not at each of the nineteen call sites.

## a containment guard that swallows the feature's own key (pretty-print audit r3)

A guard added to stop stray keys acting under a modal screen must exempt
that screen's OWN keys — and "its own keys" is not the obvious list.

Found: the browser's UP/DOWN are matched by an earlier clause in the same
if-chain, and ENTER/EXIT/BACKSPACE have their own `case ITM_...` upstream
of it, so all five survived a blanket guard. `.d` (the pan key) had
NEITHER, so the guard silently made panning unreachable — turning a
round-2 finding that was conditional into a certainty. The fix that
closed one hole opened another in the same commit.

Hunt it at: every new early-return or swallow in a key path. Enumerate
the feature's keys and prove, per key, which clause handles it.
