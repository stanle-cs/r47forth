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

Make the compiler close the external set. Three moves, all in `ui/tam.c`
and the header:

1. **`leaveTamModeIfEnabled` becomes file-static** (its declaration leaves
   the header). The eleven internal sites and the epilogue keep calling it
   unchanged. Any external caller becomes a link error — the enforcement
   is the toolchain, not a review rule.
2. **New public `tamFinish(void)`**, the ONLY external way to end a TAM
   session:

   ```c
   /* The one external teardown.  Ends the TAM session and settles the
    * fold bracket in one act, in this order and no other:
    *   leaveTamModeIfEnabled();     teardown (menus, tam.mode, resume for
    *                                PEM/PARK — unchanged semantics)
    *   forthFoldUnwindIfDone();     the bracket: resume + foldLeave, only
    *                                if the fold is pending and TAM is over
    * External callers that dispatch a cancelling item afterwards get the
    * unwound, restored console first — the EXIT-cancel semantic the
    * round-6 wave established at every door. */
   void tamFinish(void);
   ```
3. **The ten external sites swap** `leaveTamModeIfEnabled();` +
   `forthFoldUnwindIfDone();` for `tamFinish();` — a mechanical
   substitution of exactly the pairs the wave created.

The `tamProcessInput` epilogue is untouched: it already runs after
`_tamProcessInput` fully returns, which is the one ordering the internal
sites need. `forthFoldUnwindIfDone` stays public (manage.c owns it; the
epilogue and `fnKeyExit`'s TAM branch call it directly).

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

- The link error IS the class guard for new external sites.
- The existing census class tests (fnKeyExit returns, the F4 doors) keep
  pinning behaviour; they need no change — `tamFinish` is behaviourally
  identical to the pairs it replaces.
- `PROMPT_CODE_AUDIT.md`'s D1 lens gains one line: an external
  `leaveTamModeIfEnabled` caller is a finding by definition (it cannot
  compile, so finding one means the static was reverted).

## Risks

- **Relocation risk is the project's most dangerous fix shape** — but
  nothing here relocates state: the same two calls run in the same order
  at the same sites, renamed. The diff is a static keyword, a two-line
  function, and ten mechanical swaps.
- The PC_BUILD `forceTamAlpha` re-entrancy hazard (documented dead in the
  bracket comment) is unchanged by this design.
- `fnKeyExit`'s TAM branch calls leave then unwind with PEM scroll logic
  between them; it keeps its explicit pair (it is upstream-shaped code) or
  swaps to `tamFinish` + scroll — decided at implementation, either is
  correct.

## Sequencing

New code resets the audit clock, so this lands AFTER round 7 has read the
round-6 fix wave — as its own small commit with the swap enumerated in the
message. Implementation is within the local implementer's range (a
mechanical packet: one static, one function, ten swaps, gate green, class
tests unchanged), with the packet citing this document.
