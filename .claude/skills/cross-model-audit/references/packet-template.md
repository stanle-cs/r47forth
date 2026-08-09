<!--
  Packet template — cross-model audit.
  COPY this file and fill it in; never assemble a packet freehand. Five audit
  rounds produced five packet defects, and every one looked exactly like a
  good finding. The packet IS the audit: a defect in it audits a codebase
  that does not exist.

  After filling it in, run references/packet_lint.py over the result and
  judge every flag before sending. Sizes 3-11 KB are proven to answer in
  minutes; keep packets small for depth, not from fear of 10 KB.

  Delete this comment block before dispatch. Everything below the line goes
  to the reader verbatim.
-->

Begin your reply with the line `MODEL: <your exact model name>` before
anything else.

<!-- Leave that instruction first and check the answer. agy with -p before
     --model silently serves Claude, and a silent fallback is
     indistinguishable from a good audit. references/dispatch.sh checks it
     mechanically.

     IF THIS PACKET GRANTS REPOSITORY ACCESS (a repo-hybrid census, not a
     self-contained excerpt), also demand the MODEL line in the OUTPUT
     section — see the note there. Round 6: a repo-exploring reader ran
     tools between this instruction and its answer, opened the final
     message with prose, and dispatch.sh correctly discarded the whole
     reply. The lead-line instruction alone does not survive exploration. -->


## Subject

A personal hobby project: a Forth interpreter built as an external package
over the open-source C47/R47 firmware for a DM42-class pocket calculator.
Single-user handheld. No network stack, no untrusted input, no privilege
boundary. The worst outcome of any bug here is that the calculator reboots
and the owner loses the program they were typing.

Audit for FUNCTIONAL correctness: wrong answers, lost work, stuck states,
crashes. A finding whose impact statement needs an attacker is not a
finding.

<!-- Adapt the paragraph above if the subject is not forth-core. The scope
     sentence stays: audit it as what it is. -->

## Orientation

<!-- The section five rounds paid for. One line per fact the reader cannot
     infer from the excerpt. Four classes, all mandatory where they apply:

     1. Shared-structure orientation. "softmenuStack slot 0 is the TOP;
        currentMenu() is menu(0)." Round 2's inverted-stack finding came
        from omitting exactly this line.
     2. What ESTABLISHES each state, not only what it means. "A BORROWED
        frame is always FWRD: the open stamps BORROWED only when
        currentMenu() == -MNU_FORTH already." Round 5 lost two runs to
        stating the meaning without the establishing gate.
     3. Every package override the excerpt depends on. "This package's
        softmenus.c override rebuilds the FWRD picker on every paint."
        Round 3's bad Sol finding was this omission.
     4. If a comment in the code names a function, that function's body is
        in the packet below. Say so here; the linter cross-checks.
-->

## The code

<!-- WHOLE functions, verbatim, comments verbatim. Condensing a load-bearing
     comment is truncation (round 5: a paraphrase dropped the clause naming
     forthConsoleRestoreSurface and the reader's whole finding rested on the
     gap). No `...`, no /* snip */, no elided tails — round 2's sed-cut
     packet produced "this never writes its output", true of the packet and
     false of the code.

     LABEL context-only functions as context. Round 7: a packet whose
     Subject said "every function below is same-day fix code" also carried
     two upstream-verbatim helpers included only for reference; the reader
     audited them as the subject and both findings were out of scope. If a
     body is included so other code can be read, say so at its fence:
     "context, not the subject — byte-identical to upstream". -->

```c
```

## Your task

<!-- Copy-adapt the auditor brief or the refutation brief from
     design-docs/forth-core/PROMPT_CODE_AUDIT.md — never re-derive either
     from memory. Then the specific question for THIS packet: the defect
     under review, the fix design to cross-examine, or the dimension to
     read under. One packet, one question. -->

## Budget and output

<!-- REPO-HYBRID PACKETS ONLY (the packet grants repository access for a
     census the excerpts cannot carry): uncomment the line below. A reader
     that runs tools forgets the lead-line MODEL instruction, and
     dispatch.sh discards a reply that does not open with it (round 6). -->
<!-- YOUR FINAL MESSAGE MUST BEGIN WITH `MODEL: <your exact model name>` —
     even after tool use or exploration, that line comes first in the final
     answer, before any prose. A reply without it is discarded unread. -->

Answer from this packet alone; you have no repository, and everything you
need is above. If something you need is missing, name the gap instead of
guessing — a named gap is worth more than a confident wrong finding.

Report findings, not fixes. For each: where, the concrete reaching input
(keypress sequence, Forth line, or call path), the observable consequence,
the violated contract quoted, and your confidence. Rank by what the defect
costs the owner. End with what you considered and deliberately did not
flag, and why.
