# Stage N — the console: a transcript over the stack area, keys-first entry

**Status: AUTHORED 2026-08-05 under the owner's statement of the same day
("full console/terminal for interactive forth taking over the whole area
used for the stack on screen; change interactive to start in non-alpha
mode instead of alpha capture"). That statement un-parks the
terminal-console view Stage L's Non-goals parked on 2026-08-04 and
directs the entry-state flip; the decision sheet below is
architect-decided under it — each ruling revisitable by the owner, none
foreclosing a cheap reversal. Seeds: STAGE_L_INTERACTIVE.md Non-goals
(the park record), L-R8's closing clause ("purely a render decision with
no state behind it — nothing in L forecloses revisiting it additively"),
and §8.4.1's note that the alpha-open default is pinned by the K4
battery, not by a ruling. Branch `forth-core/stage-n` from the Stage M
close (`adaa12b8a`). Not yet normative — folds into DESIGN.md at
landing.**

## Problem

The interactive capture landed (§8.4.2) with a recorded ergonomic debt,
accepted eyes-open at L-R8: the edit line sits on X's row, so **X — the
register a REPL user most wants — is never visible while composing**,
and past ~350 px the whole stack display degrades to nothing. There is
no output channel of any kind: a word cannot say anything (the park's
"one-line message primitive" was never built), a session has no visible
past — the dialogue a REPL is *made of* exists only in FHIST, readable
one recalled line at a time. And the capture opens in alpha, so the
first gesture of every session is typing letters on a machine whose keys
already name the words a line is made of: `1 2 +` is three keypresses in
keys mode and a mode-switch plus letter-hunting in alpha.

The 2026-08-04 park priced a console at "a scrolling-text display
subsystem and an output-channel subsystem the firmware does not have —
F6-class UI risk with no existing surface to reuse, ~3–5 extra packets,
1–2 KB idle RAM". Two things changed. The owner directed the build. And
Stage L landed the expensive half: line entry, ENTER/REPL, history,
close-path sweeps and the fold all exist — this pass's recon (below)
reduces the "display subsystem" to one paint arm reusing the PEM
listing's own row idiom, and the "output channel" to a BSS byte ring
plus seven primitives. The park's cost estimate was made against a
pre-L world; the residual genuinely-new surface is one `screen.c` arm
and one ring module.

## Inventory — what the recon found (verified this pass)

- **The stack area is 4 × 36 px rows starting at Y=24** — T/Z/Y/X at
  Y 24/60/96/132 (`Y_POSITION_OF_REGISTER_*`, defines.h:1516-1521),
  softmenu rows from Y=171 (softmenus.c:1939-1941, `SOFTMENU_HEIGHT` 23,
  3 rows). The T/Z/Y sub-area is 108 px; X's row (132..167) is where the
  AIM edit line already draws.
- **The row-paint precedent exists and is copy-adaptable.** `fnPem`
  (programming/manage.c:481) paints the register area as **7 rows of
  `standardFont` text at 21 px spacing from Y=24**
  (manage.c:541, 546-551) — exactly the "text rows replace registers"
  idiom the park said did not exist. Its pagination/step logic is not
  reusable; its row metric is. Five 21 px rows fit the T/Z/Y area
  (24 + 5×21 = 129 < 132) above an untouched X-row edit line.
  (`STANDARD_FONT_HEIGHT` 22 is the glyph height; 21 is the landed
  listing pitch — the trace pins which the console uses.)
- **The render seam is the landed AIM arm, current line numbers.** The
  edit line draws via `showStringEdC47` (screen.c:3881-3900 area); the
  long-line split is screen.c:1657 (`> SCREEN_WIDTH - 50` →
  `yMultiLineEdOffset = 1`); `_refreshNormalScreen`'s CM_AIM arm
  (screen.c:5923-5931) paints T/Z/Y only when `yMultiLineEdOffset == 3`,
  then X. The console arm slots exactly where those three
  `refreshRegisterLine(REGISTER_T/Z/Y)` calls sit, behind the same
  conjuncts the AIM arm already trusts.
- **Errors paint *inside* `_refreshRegisterLine`,** gated
  `lastErrorCode != 0 && regist == errorMessageRegisterLine`
  (screen.c:3778-3786), default line REGISTER_Z (defines.h:1498/1505);
  `CM_ERROR_MESSAGE` routes through `_refreshNormalScreen` like normal
  modes (screen.c:6189-6216). **Consequence, load-bearing:** a console
  arm that unconditionally replaced the T/Z/Y paints would silently
  swallow every error display. The console yields to the native
  register path while `lastErrorCode != 0`; the gate carries that
  conjunct from day one.
- **Refresh cadence is already wired:** key release ends in
  `refreshScreen(131)` (keyboard.c:4825, fnKeyUp), and the CM_AIM arm
  refreshes the edit line per keystroke (keyboard.c:2896). A transcript
  append during a run needs no paint call of its own; the view is
  repainted at the seam that already repaints the stack.
- **The primitive table takes capability-class words today.** `IF`,
  `BEGIN`, `GLOBAL`, `IMMEDIATE`, `RECURSE` are landed prims
  (forth_prims.c:101-124) — Forth-*language* machinery with no item-table
  equivalent. §1.3's guardrail ("never to add capability") bans
  duplicating *calculator* operations reachable as `CAT_FNCT` items; the
  console channel is language surface in the same class as control flow.
  The fold-in records that clarification rather than leaving the
  guardrail to be argued at every output word. Mechanics: append-only
  indices after `PRIM_REPEAT` (21), 2-byte `FTOK_PRIM` tokens, flash-only
  cost, the 4th struct field (stack delta) supplied per word.
- **Prims shadow everything (§4.1 order 1)** — including `CAT_FNCT`
  items and *existing user colon definitions*. A new prim name is a
  silent meaning change wherever that name already resolves; the
  collision sweep (N-T3) is therefore mandatory, not hygiene.
- **No string-literal token exists** (§2.2's inventory: LIT/ILIT/BR/0BR/
  C47/XEQN) — `."` would need a grammar and token extension. Out of
  scope; `TYPE` consumes a native `dtString` in X instead.
- **The open path is one five-statement helper.**
  `forthEnterAimSurfaceNoLift` (forth_compile.c:1672-1690):
  `alphaCase = CAPS_AIM_DEFAULT; calcMode = CM_AIM; cursorEnabled =
  true; showSoftmenu(-MNU_ALPHA)` (:1686, plus the
  `softmenuStack[0].softmenuId` 0→1 fixup at :1687);
  `setSystemFlag(FLAG_ALPHA)`. `fnForthOuter`'s interactive arm
  (forth_compile.c:1710-1738) calls it, then `forthCapOpenInteractive()`,
  then seeds/places the cursor. The keys-first flip and the FWRD-home
  menu re-point exactly this helper plus the REPL reopen
  (manage.c:1405-1410) — no other open path exists.
- **Every capture open resets `keysMode` to 0** (`_forthCapOpenAs`,
  forth_capture.c:11) — the complete production write-set of the bit is
  20 sites, enumerated this pass: the resets (open/close/abandon/power,
  forth_capture.c:11/23/51/76), the interactive toggle (items.c:747-793,
  `runFunction`'s ITM_AIM arm — with the alpha-menu drain), the PEM
  toggle (manage.c:2367-2379), the `determineItem` routing gates
  (keyboard.c:1785/1797/1842), the EXIT rungs (keyboard.c:4029, :4152),
  the E13 resume bracket (manage.c:1269/1283/1342), and the pemAlpha
  numlock skip (manage.c:969). Keys-first therefore *sets the bit after
  open* at the two interactive open sites and leaves the universal
  reset — and the K4 pin ("a fresh capture always opens in alpha
  input") is **a battery default, not a ruling** (§8.4.1), so the flip
  is battery-row surgery on the interactive rows with the PEM rows
  asserted unchanged.
- **Keys-first typing needs no new dispatch code.** With the bit set,
  `determineItem` already routes through the normal item columns
  (keyboard.c:1841-1842) and the name-insert divert is landed
  (items.c:794-800); digits/EEX are `CAT_NONE`/`addItemToBuffer` items
  consumed by `processAimInput` (keyboard.c:636-637) on the same path
  both modes share — `1 2 +` types today in keys mode, three presses.
- **The ENTER orchestrator has the echo's exact hook points.**
  `forthInteractiveEnter` (manage.c:1363-1411): empty no-op (:1364), the
  E9 check-line refusal (:1373-1375, capture stays open),
  `forthHistoryPush` **before** the run (:1381 — a word can rewrite
  `aimBuffer`), the pre-run copy (:1387), the run (:1393), error →
  restore line, capture open (:1395-1403), else REPL reopen empty
  (:1405-1410). The line echo co-sits with the history push; the
  result/error echoes sit on the two post-run arms. The EXIT ladder
  rungs are keyboard.c:4029-4033 (keys→alpha), :4043-4048 (pop stacked
  menu), :4083-4093 (close; explicitly not `closeAim()`).
- **Output words run inside `forthOuterRun`** (§3.3.2: private ctx,
  nesting ≤ 2). A ring append is a bounded BSS write with no display
  call — legal from interpret, program-step and nested contexts alike.
  `displayCalcErrorMessage` from prim context is landed precedent for
  more than that (forth_prims.c:27).
- **The transcript's lifetime class already exists.**
  `forthCapPowerReset()` runs at the dictionary init/restore seams
  (F6-6); BSS survives deep sleep like `FLAG_ALPHA` state. The ring
  joins those seams — no new lifetime story, no save-format surface.
- **FHIST is untouched.** Input history (recall, eviction, persistence)
  is a landed, separate concern; the transcript is *output*, transient
  by design, and echoing into FHIST-as-display would burn program
  memory for scrollback. The two do not merge.

## Scope (N-R1) — three seams plus their battery

**N1 — the console view.** While an interactive capture is open, the
T/Z/Y register area paints the transcript tail (≈5 `standardFont` rows)
instead of registers; the edit line stays the native AIM draw on X's
row, untouched. Yields: to the native path while `lastErrorCode != 0`,
during TAM/the fold (`!tam.mode`, forged-CM_PEM never reaches the arm),
and in the long-line case (`yMultiLineEdOffset != 3` — L-R8's long-line
arm stands). Overwide transcript rows truncate at the right edge with
the native ellipsis. Scrollback browse: g-shifted up/down when N-T4
clears the gesture; else v1 pins to newest.

**N2 — the output channel.** One BSS byte ring (proposed 1024 B, one
constant) is *the* channel; the console is its view. Writers: the ENTER
dialogue (line echo, X result echo, error echo) and the word set —
`.` `.S` `CR` `EMIT` `SPACE` `TYPE` `PAGE`, appended prims. Words write
the ring wherever they run (interactive, key-press, program step); no
console need be open; nothing repaints mid-run.

**N3 — keys-first entry.** The interactive capture opens and REPL-reopens
with `keysMode = 1`; alpha becomes the excursion you toggle into (E10/E11
gesture unchanged), and the EXIT ladder re-derives for keys-as-ground.
PEM captures keep the K4 alpha default untouched.

Out of scope, deliberately (see Non-goals): wrapping, string literals,
input words, scroll-region/ANSI semantics, transcript persistence,
printer/serial channels, any PEM-side view change.

## Rulings — decision sheet (architect-decided under the owner's statement, 2026-08-05)

| # | Question | Ruling |
|---|----------|--------|
| N-R1 | Stage scope | **N1 + N2 + N3 as above.** Each is separable; N1 without N2 is an empty pane, N2 without N1 is a channel with no view — they land together, N3 rides the same seams. |
| N-R2 | Transcript storage | **One BSS byte ring, 1024 B (`FORTH_CONSOLE_RING_BYTES`), glyph-encoded text, `\n`-terminated lines, oldest-line eviction by tail advance.** Not arena (a convenience buffer must not compete with compile capacity — the L-R5 argument), not program memory (output is transient display state; FHIST stays input-only). Survives capture close/reopen and deep sleep (BSS, the FLAG_ALPHA parallel — reopening the console restores the dialogue); cleared at the `forthCapPowerReset` seams and by `PAGE`; never persisted. ~25–40 realistic lines of scrollback. One constant, battery-pinned, cheap to retune. |
| N-R3 | View geometry | **Transcript tail in the T/Z/Y area (~5 rows, `standardFont`, the fnPem row idiom); X's row stays the native AIM edit line.** The gate extends the landed CM_AIM arm's own conjuncts: interactive capture open, `!tam.mode`, `lastErrorCode == 0`, `yMultiLineEdOffset == 3`. Every yield case falls back to today's paint — the arm is additive by construction. Truncate-with-ellipsis, no wrap (render-only; revisitable). Status bar and softmenus untouched. |
| N-R4 | The dialogue (what ENTER writes) | **Echo the committed line prompt-prefixed (`»`, the marker glyph); on success append X's value rendered by the landed register formatter; on interpret error append the §8.7 message text (generic form — S1 stands, no token).** The X echo is the calculator's "ok": the stack is hidden, so the console answers with where X landed. Echo belongs to `forthInteractiveEnter`, not the engine — program-run Forth steps echo nothing (only explicit output words write). The line echo sits beside the FHIST push (manage.c:1381), which is already after the E9 refusal — a refused line stays in the editor and never echoes; the result/error echoes sit on the two post-run arms (:1395/:1405). The error *display* protocol is unchanged (native paint over the area until the next key); the transcript line is the record that keeps the dialogue readable afterwards. |
| N-R5 | The word set | **`.` (format X per current display mode via the landed formatter, append + trailing space, DROP), `.S` (one-line depth-prefixed picture of the live stack, non-destructive), `CR`, `EMIT` (X as C47 glyph code, ASCII subset ASCII-faithful, DROP), `SPACE`, `TYPE` (dtString in X → append text, DROP; else the standard type error), `PAGE` (clear ring + view).** All plain prims (no FF_IMMEDIATE), appended after PRIM_REPEAT, names subject to the N-T3 collision sweep (renames are the sweep's deliverable, not a relitigation). Formatting comes from the landed display code or it does not ship — the decodeOneStep argument, transplanted to values. Where no console is open the words still write the ring (one rule, no cases; PC tests assert ring bytes). |
| N-R6 | Keys-first entry | **`keysMode = 1` set immediately after `forthCapOpenInteractive()` at both interactive open sites** — `fnForthOuter`'s arm (forth_compile.c:1726 area) and the REPL reopen (manage.c:1405-1410); the universal open-reset (forth_capture.c:11) stays, so PEM inherits alpha-first untouched. The E10/E11 gesture keeps toggling both ways (items.c:747-793 unchanged); **the EXIT ladder re-derives for keys-as-ground: alpha unwinds to keys (rung 1 inverts, keyboard.c:4029-4033), stacked menus pop, keys-ground closes** (rung table is N-T4's deliverable). At open the console pushes **`MNU_FORTH` (FWRD) as its home row** in place of the `-MNU_ALPHA` push at forth_compile.c:1686 (the :1687 `softmenuStack[0]` fixup re-derived alongside) — discovery on the softkeys, and a word softkey *types* the word (the landed capture-gated F6-3 sink; the Stage M CM_NORMAL execute arm is a different mode and untouched). The E8 row-1 disposition (FWRD → pop to ALPHA menu) and `isAlphaSubmenu`'s `-MNU_FORTH` row are re-derived for FWRD-as-home (N-T4). PEM: K4's alpha default stands, asserted by the same battery that pins the flip. |
| N-R7 | Stack visibility | **The console replaces the stack display while open — deliberately** (the owner's ask; the N-R4 X-echo is the running feedback). Any native close path or EXIT rung 3 returns to the normal stack screen; reopening restores the dialogue (N-R2). L-R8 is superseded for the short-line interactive case by this stage's owner statement; its long-line arm and its PEM-side reasoning stand. |
| N-R8 | Letter + branch | `N`, `forth-core/stage-n`, branched from the Stage M close. |

## Mandatory architect pre-work (traces before any packet)

- **N-T1 — the render seam.** Current line numbers for: the CM_AIM arm's
  T/Z/Y paints (screen.c:5923-5931), the AIM edit-line draw
  (screen.c:3881 area), the long-line split (screen.c:1657), the error
  paint inside `_refreshRegisterLine` (screen.c:3778-3786) and every
  `errorMessageRegisterLine` writer, `_refreshPemScreen`'s fnPem calls,
  and `clearRegisterLine`'s exact clear rects (screen.c:2201-2218).
  Deliverable: the console arm's gate expression with every conjunct
  justified; the row arithmetic (21 vs 22 px pitch, row count, first-row
  Y); the paint-call inventory (what is suppressed, what is cleared);
  the TAM/fold interplay proof (forged CM_PEM routes to
  `_refreshPemScreen`, so the arm is unreachable during a fold — confirm
  at the `refreshScreen` switch, screen.c:6166-6216); and the
  `temporaryInformation` arms audited for the same swallow hazard the
  error line has.
- **N-T2 — the value formatter.** The landed register→display-text path
  the X echo and `.` reuse: candidate call chain inside
  `_refreshRegisterLine` for a non-error line; its buffer, font and
  context requirements; whether it is callable outside a paint (the
  context-freeness question — the F15-3 display-parity test knows the
  answer's shape). Also: the glyph code space `EMIT` maps (fonts.h),
  `stringWidth`/ellipsis mechanics for truncation, and the `.S` width
  reality (what a 4-level picture costs in px at standardFont).
- **N-T3 — prim mechanics + the collision sweep.** The seven names
  swept against (a) `indexOfItems[]` catalog names under the §4.1 item
  filter, (b) the §3.3.5 number grammar (bare `.` must resolve as a
  prim before the number arm ever sees it — order 1 vs 3, confirm), and
  (c) existing prim names and glyph aliases. Deliverable: the cleared
  name list (with renames where the sweep demands), each new entry's
  stack delta, and the flash cost estimate. The user-shadowing hazard
  (a landed colon def named `TYPE` silently loses to the new prim) is
  named in the fold-in as a documented upgrade note.
- **N-T4 — the entry-state ladder and the home menu.** The seam *sites*
  are inventoried above; the trace's work is the dispositions. The
  E8/K-R3 rung table re-derived row by row for keys-as-ground with
  FWRD-as-home (each row KEEP / INVERT / NEW with its reason — the
  M-T5 discipline), including: the rung-2 "base menu" definition when
  the base is `-MNU_FORTH`; the E13 resume's `-MNU_ALPHA`-only-for-alpha
  guard (manage.c:1342) against a keys-ground session; the
  `isAlphaSubmenu` `-MNU_FORTH` row's interactive disposition; and —
  the M-T5 lesson applied forward — **every catalog-drain predicate
  (`forthCatalogMenuOnTop`/`forthCatalogBuriedOnStack`, the E1 drain,
  fnForthOuter's own drain at forth_compile.c:1717 area) re-audited for
  a FWRD pushed by the console's open** rather than by an alpha catalog
  or the CATALOG tree: the console must not drain its own home row, and
  a later PEM entry must not inherit a stale one. Also: the K4 battery
  rows enumerated flip-vs-assert-unchanged, and the g-shifted up/down
  gesture's availability in CM_AIM (scrollback candidate) against
  native meanings.
- **N-T5 — the channel's writers and seams.** The ring's write contexts
  proven bounded (interactive ENTER, prim under `forthOuterRun` at
  nesting depth 2, program-step run under `runProgram`); the reset
  seams enumerated (`forthCapPowerReset` call sites — the ring joins
  them); the refresh path from a completed ENTER to the repaint
  (fnKeyUp → refreshScreen(131) → `_refreshNormalScreen`), so "no paint
  calls outside the view" is a checked property, not a hope.

Each finding adversarially checked before it is built on (the T7.5
lesson: reachability, not write-set).

## Packet decomposition (predicted; final after traces)

- **N1-1 — the ring.** `forth_console.c/h`: append/append-line/clear/
  iterate-tail, eviction, partial-line tail; reset-seam wiring; unit
  tests (eviction arithmetic, iteration, clear, nested-append hammer).
  No display code. *(New source file: the close packet carries the
  `CUSTOM_PKG_RECONFIGURE=1` measurement note from M1-3.)*
- **N1-2 — the view.** The screen.c arm per N-T1: suppression gate, row
  paints, truncation; the lcd_buffer display test (F15-3 precedent);
  sim LCD verification via run-sim, capture driver copy-adapted from the
  skill's references, never hand-rolled (standing 2026-08-04 rule).
- **N1-3 — the dialogue.** Echo/result/error lines in
  `forthInteractiveEnter` per N-R4 with the N-T2 formatter; L5
  interplay (error reopens line intact *and* transcript carries the
  message); tests assert ring bytes end-to-end.
- **N1-4 — the words.** Seven prims per the swept name list; per-word
  tests (type errors, stack deltas, ring bytes, PAGE, program-context
  writes with no console open).
- **N1-5 — keys-first.** The default flip at both opens, the re-derived
  ladder, FWRD-as-home; K4 interactive rows flipped, PEM rows asserted
  unchanged; class tests both leak directions (PEM must still open
  alpha; interactive must open keys; E13 round-trip preserved).
- **N1-6 — acceptance + close.** The story battery (open → keys-typed
  arithmetic → echo + X → alpha excursion to define → `GLOBAL` → `.S` →
  `PAGE` → EXIT → reopen with dialogue intact → power-reset seam clears
  the ring); the close-sweep extension (ring untouched at capture
  close, cleared at power reset — asserted in the same sweep that owns
  the close tuple); DESIGN.md fold-in (§8.4.4 the console; §8.4.2
  amendments for keys-first; the §1.3 guardrail clarification; §5.4 BSS
  inventory; the L-R8 supersession note); DESIGN-HISTORY; RULE-1
  numbers; sim captures for the forum.

Implementation follows the Stage L1-5 pattern: fully-inlined
transcription packets offered to the local model first, architect
fallback per the F6-1/L1-5C precedent without ceremony.

## Cost & risk

- **Flash:** order +2.5–4.5 KB (ring module, one render arm, seven
  prims, dialogue wiring; tests are sim-only). Measured `make dmcp5r47`
  delta recorded at close (RULE-1), with the new-file reconfigure note.
- **RAM:** **+1024 B BSS** (the ring) plus ~8 B state (head/length/view
  offset) — the stage's declared price, directly against the park's
  1–2 KB estimate, at the low end by ruling. Arena untouched (prims are
  flash; no dictionary change); high-water reported per packet
  regardless (§5.4). Program memory untouched (FHIST unchanged).
- **Risk register:** (1) the render arm lives inside
  `_refreshNormalScreen` — a wrong gate blanks registers in ordinary
  use; controlled by inheriting the landed AIM arm's own conjuncts,
  the lcd_buffer test, and sim verification (F6-class control set).
  (2) The error/temporaryInformation swallow — the yield conjunct is in
  the gate from day one and N-T1 audits every writer. (3) Keys-first
  flips a battery-pinned default — both leak directions tested (a PEM
  capture opening in keys is a regression; an interactive one opening
  in alpha is the feature dead). (4) The ladder re-derivation — a wrong
  rung strands alpha or destructively closes (the T4 arrow lesson);
  rung-per-rung class test. (5) Prim-name shadowing is silent by
  construction (§4.1 order 1) — the N-T3 sweep is a gate, and the
  upgrade note is recorded. (6) Ring appends from nested/program
  contexts — bounded-write proof plus a hammer test; no display calls
  outside the view by checked property (N-T5). (7) Upstream drift on
  screen.c, the largest override — markers around the arm, standing
  re-grep discipline.

## Non-goals (explicit)

- No line wrapping (truncate-with-ellipsis is the v1 contract;
  render-only, additive later).
- No `."` / string literals — there is no string token in §2.2 and this
  stage does not add one. `TYPE` covers string output from values.
- No input words (`KEY`, `ACCEPT`, `EXPECT`): input remains the capture
  line; the console never blocks a run on a keypress.
- No cursor addressing, scroll regions, colors, or font selection — the
  transcript is an append-only tail view.
- No transcript persistence: no save keys, no restore validation, no
  format surface. The ring dies at the power-reset seams, deliberately.
- No printer/IR/serial output channel; the ring is the only sink this
  stage defines.
- No PEM-side view change: the PEM capture keeps the listing and the
  landed composing behaviour; the console is the interactive origin's
  surface only (the words still write the ring from any context).
- No change to §8.7's error protocol or the S1 ruling (the transcript
  echoes the generic message text, nothing more).
- No change to FHIST, the fold (§8.4.3), or the Stage M dispatch arms.
