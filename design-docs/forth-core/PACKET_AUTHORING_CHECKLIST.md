# Packet-authoring checklist

Distilled from Stage L (2026-08-04/05), where an adversarial review of nine
authored packets found 19 defects — three of which would have destroyed
user data — and the implementation of the first two found four more that
review had missed. Every item below is a mistake that was actually made in
this stage, by an architect who was confident at the time.

Run this before handing any packet to an implementer.

## 1. Reachability, not write-set

The most expensive error class of the stage, hit three times.

- **The bug claim.** "Function A does not write buffer B" + "function C
  reads buffer B" is not a bug until you have traced whether A is
  **reachable** from C. In T7.5 it was not: `insertStepInProgram` never
  emits a step carrying the opcode whose decode arm leaves `tmpString`
  unwritten. Two true facts, wrong conclusion, reported to the owner twice.
- **The safety claim.** "This predicate is false everywhere before the
  stage, so nothing existing changes" is true and *irrelevant*. The defect
  class lives in the paths the change **newly opens**. State the new state
  explicitly and what it resolves to. (Written twice, caught twice.)
- **The guard placement.** Before putting an early-out in a shared sink,
  check the sink is reached on the path you care about. L1-3 rev 1 guarded
  `insertUserItemInProgram`, which interactively is never entered — the
  guard would have been dead code no mutation could kill.

## 2. A grep-derived call-site count is a lower bound, not a scope

L1-0 counted 51 call sites of `fnForthOuter(NOPARAM)`. There were 52. The
extra one is `Func: fnForthOuter` in a **test-program text file**,
resolved through a name-string dispatch table. No grep for the C call
spelling could ever have found it.

If a packet's scope is "every caller of X", the acceptance criterion must
be a **proof**, not a count: stub X to a bare `return` and require the full
gate to stay green. That is what found the 52nd site.

## 3. "Call the landed entry point" must say what it does to the state you depend on

T9. The packet said "call `fnAim`" and cited it for the mode and the menu.
Nobody asked what it did to the **stack**. `fnAim` → `calcModeAim` →
`liftStack()`, which unconditionally replaces X with an uninitialised
`dtReal34` — and the feature's whole premise is that the line operates on
the live stack.

For every landed function a packet says to call, enumerate its effects on
the state the feature depends on, not just the state the packet is about.

## 4. `indexOfItems[item]` needs `item > 0`

`determineItem` returns **negative** softmenu ids (e.g. `-MNU_AIMCATALOG`,
src/c47/assign.c:46). They reach `processKeyAction`'s default arm and the
`case CM_AIM:` body. Any new guard of the form
`forthCapIsInteractive() && indexOfItems[item].func == …` short-circuits
after the capture test, so it fires **exactly** when a capture is open.
The landed `keyboard.c:2794` tests `… || item < 0` for this reason; that
shape is a latent wart, not the pattern to copy.

## 5. A new symbol in a file needs that file's include

`items.c` includes only `c47.h` and `forth_dict.h`. `forth_compile.c`
reaches neither `forth_capture.h` nor `forth_menu.h`. The build is
`warning_level=2` with **no `werror`**, so a missing declaration becomes
an implicit `int` return and the gate stays **green** with an
ABI-unspecified value. Check the include list of every file a packet
touches, and never hand-declare (items.c:8-9 does, and it is the pattern
that hides this).

## 6. Two-part edits must be named as such

Escaping a `calcMode` disjunct in `determineItem` without adding the
matching arm to the normal-column branch drops the item off the end of the
mode chain into `displayBugScreen`. A packet that specifies half of an
atomic edit will be implemented as half. Say "this is one of two" and make
the omission a mutation.

## 7. Register new tests outside `if(fail)`

`test_capture.part.h` holds bodies only and is `#include`d *after* the
runner in `test_dict_reloc.c`. An uncalled static test is a compiler
warning and a **green gate**. Precedent: commit `14fecc428`, where K4's
registrations sat inside the suite's `if(fail)` verdict branch and the
landing gate never ran them. Require the implementer to quote the
`[DEBUG] running …` line and every PASS line verbatim from the green log —
the `ALL PASSED` banner alone is not evidence a new test ran.

## 8. A mutation that does not go red is a finding — but design the mutation too

Three genuine coverage holes were found this way in the first two packets:
no test asserted the `fnDrop` happened, none drove a maximum-length line,
and one guard was unfalsifiable by construction. **Instruct the implementer
to report such a mutation, not delete it.**

But two of L1-F1's non-red mutations were **mutation-design errors rather
than holes**: one replicated the real code's own arithmetic (computing the
restore key at restore time, which is what the real code does), and one
probed a property the operation cannot violate (a fold is net-zero on
program memory, so no saved key can go stale). Before reporting a mutation
as unpinned, ask whether it actually differs from the original *in the
dimension the assertion tests*.

Three outcomes, and they are different things: a coverage hole (fix the
test), a mutation-design error (fix the mutation), and unfalsifiable by
construction (record it as a fact about the design). If a mutation is
unkillable, say so in the packet up front with the reason, so the
implementer does not spend hours on it.

## 9. Verify the assertion can be driven before specifying it

L1-1's packet asked for "type `1 +`, ENTER, assert X == 17" in a packet
whose Out-of-scope list forbids ENTER. L1-1 rev 1 also specified a test
that would have called `forthCaptureSuspend` on an interactive capture,
which reaches `forthCapRecommitStep` and would have **deleted whatever
`currentStep` pointed at** — the fixture's `.END.`.

Ask, per assertion: does the harness have the gesture? Does the packet's
own scope permit it? Does driving it touch anything destructive?

**And: does the comparison survive the operation's own legitimate side
effects?** L1-F1's C6.1 said "assert `currentStep` bit-identical", which
reads as if a raw pointer comparison is safe. It is not —
`_insertInProgram` rebases every program pointer when it grows the region,
which is exactly the relocation the packet's own `capStepOffset` design
exists to survive. Compare by **offset**, and pre-create any lazily-built
structure before snapshotting. The implementer's first draft went red
twice on this before diagnosing it.

## 10. Program-memory edits: name the position and the order

`_insertInProgram` writes **before** the step `currentStep` points at, and
`scanLabelsAndPrograms` assigns a label to the program number current at
the label's position. "Position at the end of program memory" was not
specific enough — the natural reading would have spliced a new program's
label **into the user's last program**. Show the byte layout you expect,
and require the test to assert the neighbouring program is unchanged by
byte comparison rather than by prose.

## 11. Do not fix upstream defects in package overrides

`UPSTREAM_REPORTS_globalRegister_reset.md` records a correct fix evicted
from forth-core in the S1 pass because it "has nothing to do with Forth,
so it should not ride in this package's patch set". Carrying an upstream
behaviour change inside a package patch is the divergence class that cost
us the dead `_executeOp` block. If the stage makes an upstream defect
*reachable*, guard it in code the stage writes — do not patch what it
inherited.
