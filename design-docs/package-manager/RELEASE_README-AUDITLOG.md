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
