# D7-1 — closing the TAM teardown owner set: `tamFinish`

Design for owner review, 2026-08-08. No code. Sol's round-6 verdict
(self-contained packet, GPT-5.6) is the starting point: *the fold bracket
cannot be correct by construction while teardown and fold-finalisation have
separate owners.* F2 proved the owner set open; F4 proved it a second time;
the round-6 fix wave closed every **known** door with a manual
`forthFoldUnwindIfDone()` after each external `leaveTamModeIfEnabled` call.
That is enumeration. The next teardown site anyone adds starts unguarded,
and nothing but review catches it.

## What the wave left standing

Two populations of teardown call sites, with different constraints:

1. **Internal** — the eleven `ui/tam.c` leave-then-dispatch sites inside
   `_tamProcessInput`'s call tree (ui/tam.c:303, 494, 508, 520, 536, 572,
   913, 934, 980, 996, 1130). Their resume MUST stay deferred to the
   `tamProcessInput` epilogue: the L1-F2 rev-2 failure is on record —
   unwinding at the leave site fires before the dispatch inserts its step,
   the splice sees n == 0, and the line is lost. These sites are already
   correct by the epilogue's construction.
2. **External** — keyboard.c (eight sites after the wave), screen.c,
   assign.c, lblGtoXeq.c. Each now carries the manual unwind. These are
   the F2/F4 class: any new one is a strand door until someone remembers
   the second call.

## The design

**Rev 1 of this design (static + a new `tamFinish` name) is DEAD, and the
owner's question killed it.** Twelve upstream files call
`leaveTamModeIfEnabled`; six of them are NOT in the package's override set
(`flags.c`, `ui/matrixEditor.c`, `c47Extensions/keyboardTweak.c`,
`printing/print.c`, `programming/input.c`, plus the `ui/tam.h`
declaration). A static would break their link, and overriding six upstream
files for an enforcement mechanism is D8 footprint growth the discipline
forbids — group A is already one file over budget.

**Rev 2 inverts the enforcement, and covers more for less.** The package
already overrides `ui/tam.c`, so every caller in the whole tree — package
and un-overridden upstream alike — links the package's version of the
function. So keep the public name and make it settle the bracket itself.
Three moves, ALL confined to the existing `ui/tam.c` patch:

1. **The current body moves to a file-static `_tamLeave(void)`** —
   byte-identical, including the PEM/PARK resume gate and the rev-3
   armed-fold deferral.
2. **`leaveTamModeIfEnabled` becomes the public wrapper**:

   ```c
   /* The one teardown every caller outside this file gets.  Ends the TAM
    * session and settles the fold bracket in one act:
    *   _tamLeave();                teardown — unchanged semantics
    *   forthFoldUnwindIfDone();    the bracket: resume + foldLeave, only
    *                               if the fold is pending and TAM is over
    * Name and signature are upstream's, so every upstream caller —
    * including the six files the package does not override — inherits the
    * unwind through the override, with no header patch and no new
    * overrides.  The eleven leave-then-dispatch sites in THIS file call
    * _tamLeave() directly: their resume must stay deferred to the
    * tamProcessInput epilogue (the L1-F2 rev-2 loss is the record). */
   void leaveTamModeIfEnabled(void) {
     _tamLeave();
     forthFoldUnwindIfDone();
   }
   ```
3. **The eleven internal sites and the epilogue swap to `_tamLeave()`** —
   mechanical, same file. The epilogue keeps its own
   `forthFoldUnwindIfDone()` after `_tamProcessInput` returns, unchanged.

Then the round-6 wave's ten manual `forthFoldUnwindIfDone()` calls after
external leaves become redundant and are REMOVED (keyboard.c ×7, screen.c,
assign.c, lblGtoXeq.c) — the external patches shrink back to upstream's
own shape, which is the direction D8 wants.

Enforcement is now correctness-by-default rather than refusal: a NEW
external teardown site that calls the public name is right without knowing
the fold exists, and `_tamLeave` is unreachable outside the file. Both
directions closed.

**The census gap this uncovered (round-7 item).** Every armed-fold census
so far — the audit's and the fix wave's — grepped the PACKAGE working
area. The six un-overridden upstream callers were never checked for
reachability with a pending fold. Rev 2 guards them regardless (they link
the wrapper), but round 7 should still trace whether any is reachable with
the console's fold pending, because each such path was an unguarded strand
door until this lands. Census rule, generalized: *a package-tree grep is
not an upstream census — enumerate callers in `src/` and diff against the
override set.*

## What this deliberately does not do

Sol's fullest shape — one terminal transition also owning the **dispatch**
(commit as a parameter/thunk, raw leave unreachable even inside tam.c) —
would fold the eleven internal sites into the same construction. The cost
sits exactly there: eleven call sites re-sequenced around a C-style
deferred-dispatch parameter, in the most re-entrancy-sensitive file in the
package, to defend against a class (an internal site that dispatches after
leave but outside the epilogue's reach) that has never produced a finding.
The epilogue already covers the internal population, and it is one screen
of code in the module that owns the invariant. If an internal-site strand
ever appears, that is the trigger to revisit; until then the smaller form
buys the by-construction property where the bugs actually were.

## Enforcement and tests

- Correctness-by-default IS the class guard: a new external teardown that
  calls the public name settles the bracket without knowing it exists,
  and `_tamLeave` cannot be reached from outside the file.
- The existing census class tests (fnKeyExit returns, the F4 doors) keep
  pinning behaviour unchanged — the wrapper is behaviourally identical to
  the pairs it replaces. The one new mutation: revert the wrapper to
  `_tamLeave` alone and the F2/F4 reproducers must go red.
- `PROMPT_CODE_AUDIT.md`'s D1 lens gains one line: any NEW direct caller
  of `_tamLeave`, or any teardown path that bypasses the wrapper, is a
  finding by definition.

## Risks

- **Relocation risk is the project's most dangerous fix shape** — but
  nothing here relocates state: the same two calls run in the same order
  at the same sites. The diff is one rename inside ui/tam.c, a three-line
  wrapper, eleven same-file swaps, and ten deletions of now-redundant
  unwind calls.
- **The six un-overridden upstream callers change behaviour**: their
  teardown now settles a pending fold. That is the point — but each is a
  path no fixture has driven, so the round-7 census above must precede or
  accompany the landing, and the class test asserts the wrapper's unwind
  fires (mutation: revert the wrapper to `_tamLeave` alone → the F2/F4
  reproducers go red again).
- The PC_BUILD `forceTamAlpha` re-entrancy hazard (documented dead in the
  bracket comment) is unchanged by this design.
- `fnKeyExit`'s TAM branch calls leave then unwind with PEM scroll logic
  between them; its explicit unwind becomes redundant under the wrapper
  and is removed with the other ten.

## Sequencing

New code resets the audit clock, so this lands AFTER round 7 has read the
round-6 fix wave — as its own small commit, with the round-7 upstream
caller census (above) preceding or accompanying it. Implementation is
within the local implementer's range (a mechanical packet: the rename,
the three-line wrapper, eleven same-file swaps, eleven redundant-unwind
deletions, gate green, class tests unchanged, the wrapper-revert mutation
red), with the packet citing this document.
