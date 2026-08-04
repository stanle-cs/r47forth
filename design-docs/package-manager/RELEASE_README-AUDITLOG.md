# RELEASE_README audit log — "Typing a Forth line" section (2026-08-04)

Round 1 (mechanical): aiaudit 0 flags. framescan clean (1 colon-list,
judged legitimate enumeration of three key behaviors, kept). voicematch:
comma-but chain fixed to front-But (his signature position); CV 0.48 vs
0.72 noted, NOT chased per the metric-chasing trap; attest flags all
domain artifacts (calculator vocabulary absent from the reddit corpus),
ignored per the artifact rule.

Round 2 (gemini-3.1-pro-high): one finding — the "Why bother when you
can spell the names?" transition question. RULED KEEP: question-then-
answer is the corpus-measured construct at ~this exact rate (1 question
in 13 sentences ~ his 11%), same shape as his "Why not just copy the
file out of your checked-out tree?". Gemini itself endorsed every other
construct as hardware-factual.

Round 3 (codex, read-only): zero actionable findings; independently
endorsed the question, the ellipsis "Some you can't.", and the key-list
colon. Convergence reached; stopped.

Facts verified against source: 16 patches / 19 files (unchanged by
Stage K), upstream pin unchanged, keys-mode behaviors against the K1-K4
landed tests (ALPHA toggle, name+separator inserts, TAM fold text, R/S
STOP step, EXIT one level per press, x-squared glyph untypeable in
alpha per the T6 trace).

GATE: Stan's read. Model-clean is not done.

Round 4 (STAN'S READ — the gate): "very good, but doesnt sound like me."
Verdict: middle-register failure exactly as the skill names it — the
scanners and both model lanes had converged on crafted prose (the
"digits stay digits" symmetry, the "Some you can't." fragment closer,
the "key knows its own spelling" flourish). Full rewrite against the
2026 corpus register (the LocalLLM $200/month comment is the anchor
shape): plain verbs, direct you-instructions, sequences instead of
parallels, no closers. One "so" dropped to land the connector rate.
aiaudit 0 flags on the rewrite; model lanes deliberately NOT re-run
(seesaw risk — they endorsed the failed version). Awaiting his re-read.

Round 5 (STAN'S READ, fact challenge): "is it true that you can't type
superscript? sounds wrong." He was right. convertItemToSubOrSup
(bufferize.c) maps digits to SUP digits under the up-arrow latch and
pemAlpha applies it — x² IS typeable in alpha (numlock + superscript
arrow + 2). The T6 trace had only grepped item/softmenu rows and marked
the conversion path an open question; the README asserted past the open
question. Facts-first violation, caught only by his read. Claim replaced
with the true one: keys mode saves presses. Scanners stay at 0 flags.

Round 6 (Stan's ruling: sim-verify, then delete if nothing impossible):
LCD captures confirmed every section claim on screen — the keys-typed
"42 STO 05 SIN" line and an alpha-typed x-superscript-2 (latch + 2), and
"3 x²" interprets clean. Nothing is alpha-impossible, so the closing
justification paragraph is deleted per the ruling. Side finding: the
capture driver exposed that the K4 test group had been unreachable since
the K4-A reorder (see DESIGN-HISTORY 2026-08-04, runner-surgery defect).
