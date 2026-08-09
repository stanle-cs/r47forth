# Design input — the PEM sibling of the resume-canary door (Sol, 2026-08-09)

GPT-5 Sol's reply to the round-9 self-contained design packet
(packet: the round-9 OOF report §1; question recorded in 88703343f's
comment in forth_capture.c). Verified in-repo the same day: fnClP(label)
CAN delete the suspended program; defineCurrentProgramFromCurrentStep
(src/c47/programming/manage.c:410) never writes currentLocalStepNumber.
Input to the eventual PEM-sibling design; not itself a ruling.

MODEL: GPT-5

Verdict: abandon-on-canary is not sufficient for PEM. A correctly aligned wrong `ITM_FORTH` step passes the only PEM validation, after which resume can edit one user step and delete subsequent real steps from an unrelated program.

### 1. Worst PEM-side consequence

Construction:

1. Suspend a PEM capture in program A:
   - Save A’s capture-step byte offset.
   - Save A’s total step count `S`.
2. During TAM, DELP removes a preceding program, shifting A and later programs downward while leaving the saved offset unchanged.
3. Arrange program B so the stale address now lands on an early, legitimate variable-string `ITM_FORTH` step in B.
4. Give B a total count `S + K` and at least `K` real steps following that Forth step. Short decoded steps maximize how many fit in `aimBuffer`.

No shown line blocks this. The global shape test passes, and:

```c
if (pValid && forthFoldPending())
```

skips the FHIST bound because this is PEM.

Resume then does the following:

- `forthCapOpen()` clears the real suspended line from `aimBuffer`.
- `xcopy(aimBuffer, p + 4, len)` replaces it with B’s Forth-step text.
- The saved cursor from A is merely clamped to that unrelated text.
- `currentLocalStepNumber` is restored from A even though `currentStep` is in B.
- `defineCurrentProgramFromCurrentStep()` selects B.
- `getNumberOfSteps()` therefore returns B’s count, while `saved` remains A’s count.
- The splice computes `n = K` and repeatedly takes the step following the false capture step in B.
- Each successfully decoded step is appended to the reconstructed Forth line and immediately deleted from B.
- If any folding occurred, `forthCapRecommitStep()` rewrites B’s false capture step with that reconstructed line.

The maximum supported consequence is therefore:

1. B’s original Forth step is overwritten.
2. Up to `K` subsequent real steps in B are deleted.
3. A’s suspended line is displaced from the live capture state.
4. The session is now anchored to the wrong program and step.

The deleted steps’ canonical text may survive concatenated into B’s rewritten Forth step, but that is not equivalent to preserving the original program structure or behavior. If the line buffer fills, earlier deletions remain committed; there is no rollback.

The END/NULL guard limits deletion to steps actually following `p`. It does not protect those steps from deletion.

A still stronger construction deletes A itself, making the captured line physically disappear before aligning onto B. The packet does not say whether DELP permits deleting the suspended program itself, so that variant is a named gap rather than a premise.

Another gap: the supplied contract says `defineCurrentProgramFromCurrentStep()` re-derives only the program. It does not say it corrects `currentLocalStepNumber`; therefore the consequences of retaining A’s local number while operating in B cannot be bounded from this excerpt.

### 2. Required PEM structural rule

The correct invariant is stronger than a program bound:

> A PEM capture must resolve to the same materialized capture step in the same logical program in which PEM opened it. If that step or program was deleted, resolution must fail.

Merely requiring “some Forth step inside the original program” still permits alignment onto another Forth step within that program.

#### (a) Deleter-adjusts convention

For every program-memory relocation:

- Deletion wholly before the capture subtracts the deleted byte count from its saved offset.
- Insertion wholly before it adds the inserted byte count.
- A deletion containing the capture step or its program invalidates the suspension.
- Mutations after it leave the offset unchanged.

This directly follows the stated upstream `fnClP` convention and is my primary recommendation.

What breaks it:

- A memory-moving path that omits the adjustment.
- Reordering or rewriting operations whose relocation semantics are not reported.
- Deleting the capture step without marking the anchor invalid.

Cost:

- The suspended-capture state must be registered with the central mutation/deletion machinery.
- Every relocation path must use that machinery.
- Tests must cover deletion before, after, and across the capture.

Refactor robustness:

- Highest if implemented at the common program-memory mutation layer.
- Poor if patched into DELP alone, because the next compaction or insertion path recreates the class.

This solves identity at the relevant level: the saved anchor moves with the exact step, rather than being rediscovered from a shape.

#### (b) Saved program identity

A program ordinal does not survive deletion of an earlier program. An instruction pointer does not survive compaction. A name is unsuitable unless uniqueness and immutability are guaranteed.

The appropriate identity would be an immutable program ID that survives renumbering and relocation. No such ID is described in the packet.

Even with such an ID, program identity alone is insufficient: mutations within that program can move the capture offset onto a different `ITM_FORTH` step. It must be paired with a step locator—such as a relocation-maintained offset/handle or an unambiguous step identity.

Cost:

- Low if firmware already has immutable IDs and stable step handles.
- Potentially high if IDs or hidden capture markers must be introduced.

Refactor robustness:

- Strong with genuine immutable program and step identities.
- Weak with saved ordinals, pointers, names, or content fingerprints.

Whether such an identity already exists is a packet gap.

#### (c) Recomputed suspending-program `[from, to)` bound

This catches cross-program alignment only if resume can first identify the same logical program.

What breaks it:

- Saving the program number: deletion before it renumbers programs, so the recomputed window may belong to another program.
- Saving its start pointer: compaction invalidates it.
- Movement within the correct program: the stale offset can align with another Forth step inside the same window.
- Deletion and recreation of a program at the same ordinal.

Cost:

- Small once a stable/adjusted program identity is available.

Refactor robustness:

- Useful as defense-in-depth.
- Insufficient as the primary identity mechanism.

Therefore the best design is:

1. Adopt upstream’s relocation-adjustment convention for the exact capture-step anchor.
2. Invalidate that anchor when its step/program is deleted.
3. At resume, verify that the resolved pointer is within the tracked logical program and has the required opcode shape.
4. Treat both the program bound and canary as validation, not as identity recovery.

### 3. Fallback and existing test

Yes: abandon-on-validation-failure remains the right fallback. Once exact identity cannot be established, PEM has no FOLD-like uniqueness rule with which to recover safely. Losing the suspended line is cheaper than guessing and modifying a user program.

The behavioral invariant pinned by the test should remain:

> An invalid, deleted, or unresolvable PEM capture anchor causes abandonment with no splice or other program-memory write.

Whether the existing test itself survives unchanged depends on its setup:

- If it corrupts/removes the capture step or otherwise creates an unresolvable anchor, it should continue to pass unchanged.
- If it merely deletes a preceding program and expects the resulting stale canary to cause abandonment, its expected result should change: the relocation convention should adjust the anchor and resume the real capture.
- If it asserts only “false canary means abandon,” retain it and add a separate passing-canary/wrong-step regression.

Tests needed for this door are deletion before the suspended program, deletion of the suspended program, cross-program aligned `ITM_FORTH`, and same-program aligned `ITM_FORTH`. The key assertion is that no unresolved identity ever reaches `forthCapOpen()`, `xcopy`, or the splice.