# undo-history — cross-model audit round 1 (2026-08-25)

Subject: the five same-day fix commits on undo-history/stage-u2
(0f01790aa label, 03e5fc571 ENTER dispatch, 722915309 live-restore
anchor, 95be96301 numbering + key blanket, 0f48799f4 dispatch-capture
gate) plus the engine/browser/keypath seam they cluster on. The package
is Claude-authored, so the family exclusion removed in-family finders:
this round is out-of-family find + CROSS-refutation (each reader
refutes the other's findings), mutations and probes run by the
operator. Readers, identity-verified every pass: GPT-5 (Sol, engine
state-machine packet, 20.6 KB) and Gemini 3.1 Pro High (keypath packet,
22.6 KB).

Mechanical half: gate green solo+combined (13,025). design-audit.sh
flags 2 WS-ONLY churn lines in forth-core's 010-softmenus.c.patch —
pre-existing (b5c4020af era), forth-core round-12 backlog, not this
subject.

## Confirmed findings (3), ranked by owner cost

### A1 — a failed browser restore corrupts navigation state and
destroys the selected level (Sol; SURVIVES all three lenses, Gemini)
undoHistoryRestoreLevel from live: undoHistoryNoteFirstUndo() COMMITS
(anchor pushed — evicting oldest levels — and historyCursor set to the
last capture) BEFORE the seq re-find. If the mint evicted the selected
target, the restore returns false — but the mint is not unwound: the
machine still holds the live state while historyCursor claims the last
capture, and the selected level is gone BECAUSE the user tried to
restore it. Reachable: full ring + large live state + choosing the
oldest level. The refuse-on-eviction ruling covered the refusal, not
the state left behind. Fix shape (next round's subject): pre-check the
mint's fit against the target before committing, or unwind on refusal.

### A2 — the anchor mint bypasses the gap bookkeeping; a second UNDO
walks FORWARD (Sol; SURVIVES, Gemini)
undoHistoryNoteFirstUndo pushes via historySerializePush directly, not
via undoHistoryCapture — so a pending oversized-skip (historyGapPending)
is neither applied nor cleared, and the zeroed historyLastCaptureSeq
leaves historyCursor NONE after the mint. Sequence: oversized pre-state
skipped -> op -> UNDO (anchor minted, cursor NONE, upstream undo()
restores the oversized state) -> UNDO again: stepBack sees NONE and
targets the ring TOP — the anchor — moving the machine FORWARD.
Numbering also shows no ~ across the skip. Fix shape: route the mint
through the same bookkeeping as a capture (gap flag + cursor from the
push result), or teach stepBack the post-mint state.

### A3 — the browser opens mid-TAM and TAM eats its keys (Gemini;
SURVIVES, Sol)
undoHistoryKeyReroute checks calcMode==CM_NORMAL only; TAM (e.g. STO
pending) runs WITH calcMode CM_NORMAL, so f-UP opens the browser with
tam.mode armed. processKeyAction's tam branches then intercept ENTER
(tamProcessInput) and digits before the browser blanket — restore
impossible, browser un-navigable until the user cancels TAM blind.
Sol: "the implementation defeats the documented intent." Fix shape:
gate the reroute on !tam.mode (upstream-conventional: browsers are
unreachable mid-TAM upstream because TAM swallows their entry items).

## Refuted (3)

- S3 (oversized live state unredoable, no trace): correct by
  construction — a state that cannot fit the ring cannot be a REDO
  target, and GAPBEFORE is carried by a SUBSEQUENT ring state, which a
  timeline-end live state does not have. (Gemini, intent+correctness.)
- G1 (shift locks the browser): empirically dead — the operator probe
  through real keys shows shift cancelled by the state machine on the
  next press; one ignored keypress, then normal. (Sol, on supplied
  probe transcript.)
- G3 (backspace swallowed as ITM_CC): wrong item identity — backspace
  is ITM_BACKSPACE with its own live case; ITM_CC is the complex-pair
  key, ignored deliberately. (Sol.)

## Exit state

NOT closed. Three confirmed findings reset the counter; the next round
audits their fixes (the fix trap: r2-of-fixes rates have never fallen).
Two consecutive clean rounds, at least one out-of-family on the fix
commits, close it.

## Process notes (growth rule)

- Refutation packets at 27.9/26.6 KB (over the 23.9 proven range)
  both answered fully and structurally; range note updated in the
  skill.
- An operator-run empirical probe transcript supplied as a
  pre-verified fact killed G1 in one paragraph — cheaper than a
  refutation trace; pattern recorded in the skill.
- Cross-refutation (each out-of-family reader refutes the other) held
  up for a Claude-authored package where in-family finders are
  excluded; both replies engaged all three lenses per finding.
