# Stage L — interactive Forth capture (PROPOSAL, pre-ruling)

> **SUPERSEDED 2026-08-05 where normative-pending.** Stage L landed
> (L1-0..L1-3, L1-H, L1-F1..F3, L1-5); DESIGN.md §8.4.2/§8.4.3, §8.1
> (FHIST), §8.3 (interactive durability), §3.3.2 (entry re-point) and
> §8.10 (item 2 discharged) are now the normative record. This file
> remains the stage's proposal and ruling history.

**Status: FULLY RULED 2026-08-04. Scope is full — L1+L2+L3+L5; L-R5
amended same day (L2 = 512 B packed history ring; terminal-console view
PARKED — see Non-goals). L-R4 closed same day on trace evidence:
**(b) build the fold** — the interactive capture gets the same
parameterized-key behaviour as PEM, over the architect's recommendation
of (a) (recorded below, not relitigated). Traces T1–T6 landed
(STAGE_L_TRACES.md); the fold's own three pieces get their own trace
series before packets, per the house rule. Not yet normative — folds
into DESIGN.md at landing.** Branch `forth-core/stage-l` created
2026-08-04 (L-R6 stands). Seed: DESIGN.md §8.10 item 2 (deferred-additive,
"same entry-layer surface as §8.4, so cheapest to build alongside it") plus
item 1 for the deferred Stage M half. DESIGN.md §8.4 rules E0–E14 and
§8.5–§8.8 stay normative and unchanged; this stage adds an interactive
entry surface over the same machinery. Amendment trail to DESIGN-HISTORY.md
as usual. Stage letter and branch (`forth-core/stage-l`) stand unless
renumbered at branch creation (L-R6).

## Problem

Interactive Forth today is a one-shot function, not a mode. `fnForthOuter`
requires a string in X and raises `ERROR_INVALID_DATA_TYPE_FOR_OP` otherwise
(§3.3.2; item row [VERIFIED: packages/forth-core/items.c:4771]). To run one
line the user opens plain AIM, types the line letter-by-letter, closes alpha
(line lands in X), finds FORTH in the FCNS catalog, executes it — and the
line is consumed. There is no re-run short of retyping (ENTER-dup before
FORTH is the only workaround), no discovery, and none of what F6/K built:

- catalog picks inserting `itemCatalogName` as text (F6-3),
- parameter folding through the landed decoder (F6-2/F6-4),
- the FWRD union catalog (F6-5),
- keys mode (§8.4.1, Stage K).

All of it is PEM-capture-only. The same item is an entry-mode toggle in PEM
and a string-consuming function outside it — the exact asymmetry §8.10
item 2 records against the extension principle.

## Inventory — what transfers, what does not

Transfers as-is (verified this pass):

- **The run engine.** `forthOuterRun(ctx, FORTH_OUTER_FULL)` is already the
  interactive core and is entry-agnostic (§3.3.2). Untouched.
- **Interactive scope + durability.** `FORTH_OWNER_INTERACTIVE` /
  `FORTH_OWNER_GLOBAL` exist [VERIFIED: packages/forth-core/forth_dict.h:23-24];
  `GLOBAL` moves a word to gdict (forth_dict.h:292 comment), which survives
  lifetime resets and save/restore with its own H5 restore validation
  (forth_dict.h:75, 88). The durability story for interactive definitions
  already exists; it needs surfacing, not machinery.
- **The union catalog.** F6-5's `MNU_FORTH` lists text-scan + interactive
  fdict + gdict sections with provenance (DESIGN-HISTORY 2026-07-19 F6
  authoring entry); the text-scan section already null-guards the no-program
  case [VERIFIED: packages/forth-core/forth_menu.c:107-109]. Interactively it
  degrades to exactly the right thing: dictionary sections only.
- **XEQ-by-name.** The §4.2 fallback arms (`FORTH_XEQ_LABEL/COLON/ITEM`,
  [VERIFIED: packages/forth-core/items.c:698-718]) mean interactive
  `XEQ 'NAME'` runs a global/interactive word today.
- **Keys-mode state.** `forthCap.keysMode` + E10–E14 discipline
  (forth_capture.h:39; §8.4.1). The bit and its reset seams are
  capture-scoped, not PEM-scoped.

PEM-anchored — does NOT transfer, and is the real work:

- **The dispatch interception seam.** Every F6/K text sink hangs off E0's
  arm in `insertStepInProgram` (manage.c) — a function only PEM reaches.
  Interactively, a dispatched item goes `determineItem` → `runFunction` →
  live execution. Interactive capture needs its own divert seam (the
  E0-equivalent) at the interactive dispatch site; locating it is trace T1.
- **Suspend/resume's source of truth.** F6-2 suspend FREES the buffer and
  resume refills from the program step (forth_capture.h:14-21). No step
  exists interactively; anything that relies on recommit-from-step (TAM
  folding, F6-4) has no substrate. See L-R4.
- **The picker's text-scan section** keys off `currentStep`
  (forth_menu.c:107) — correctly empty interactively; nothing to do beyond
  the CM gates.
- **CM_PEM gates.** The F6/K conditionals are keyed on
  `calcMode == CM_PEM && tam.function == ITM_FORTH && …`. Every such gate
  must be audited and widened (or left PEM-only) deliberately — this is the
  stage's principal regression surface.

One standing rule bites twice here: the interpret loop must NEVER run out of
`aimBuffer` — it is also the NIM buffer and executed words can rewrite it
mid-line (§3.3.2, c47.c:132). The interactive ENTER path copies the line
into the private `forthOuterCtx_t.source` before interpreting, exactly as
the X-register path copies before `fnDrop`.

## Candidate improvements

### L1 — FORTH interactively opens a capture; ENTER interprets (core)

Pressing FORTH outside PEM opens a Forth capture line on the normal
screen — seeded from X when X holds a string, empty otherwise (L-R2,
ruled): alpha input into `aimBuffer`, the ALPHA menu with FWRD, the
F6-3-style catalog inserts, keys mode via the E10/E11 gesture. ENTER
copies the line out and runs `forthOuterRun(FULL)`; EXIT closes the
capture (E8 ladder, extended). The natural host is the AIM
surface (`ITM_AIM` seam) with `forthCap` OPEN plus a new origin bit
(PEM vs INTERACTIVE) so close/reset seams and sinks can key correctly —
K-R1's precedent (transient resolution state, derived display, no persisted
byte) applies unchanged.

Mechanism notes for the trace to confirm, not rulings:

- Entry: `fnForthOuter` opens the capture on both arms (L-R2, ruled):
  non-string X opens empty; string X seeds the line and is consumed at
  seed (copy before drop, §3.3.2; oversize → today's error, no capture).
- Sinks: character items → `addItemToBuffer` (native AIM path); direct
  CAT_FNCT items and picker picks → `forthCapInsertName` at the T1 divert
  seam; digits/EEX per E12.
- Render: the AIM input line surface, unchanged; no new display code
  expected (trace T5 confirms).
- Reset: E14's choke point (`forthCapClose`) already covers every close
  path since FIX-8; the interactive origin joins the same seams and the
  close-paths class test extends to it (bug-fix class-test rule applies
  from day one).

### L2 — line history (**L-R7: the fold's scratch program**)

> **Superseded 2026-08-04.** Everything below describes the 512 B BSS
> packed ring (L-R5). L-R7 replaced it: history lives as `ITM_FORTH`
> steps in the kept, named `FHIST` program that the fold already needs —
> so the ring, its encoding, its eviction arithmetic and its poison sweep
> are all deleted, and history gains save/restore persistence at zero
> format cost. The *rules* below survive almost unchanged (push on every
> close path with non-empty text; consecutive duplicates collapse;
> in-editor recall, not a softmenu); only the storage changes, and the
> lifetime rule inverts — history now persists rather than dying at the
> `forthCap` reset seams. Growth is a 1024-byte soft cap with oldest-
> first eviction (`deleteStepsFromTo` on the first step). Rationale and
> verified cost: STAGE_L_TRACES.md §T7.2b. Kept below for the record.

After a successful ENTER the line is gone (the REPL loop reopens empty).
History is a packed variable-length ring in one fixed 512-byte BSS buffer:
entries back-to-back as `[len][bytes]`, oldest evicted to fit — ~a dozen
realistic lines (10–40 bytes each), guaranteed minimum two worst-case
256-byte lines. Rules:

- **Push rule (one rule, no cases).** Every interactive close path with
  non-empty text pushes an entry — run, abandon, reset seams (L-R2
  consequences) — so EXIT never loses a line. Consecutive duplicates
  collapse: re-running a line does not eat the ring.
- **Recall.** Readline-style browse in the open capture line: older/newer
  recalls the full text into the editor; editing + ENTER runs the edited
  version and pushes it as newest, leaving the browsed entry untouched.
  The gesture is T4's deliverable — up/down arrows are the natural
  candidates but are NOT free during capture (they page multi-page
  softmenus, and are SST/BST in PEM), so T4 resolves the recall-vs-paging
  collision (empty-line gating or shifted arrows as fallback). A softkey
  menu listing history lines was considered and rejected: 14-byte menu
  slots truncate most lines into ambiguity; in-editor recall shows the
  whole line.
- **Lifetime.** Plain BSS text: survives deep sleep like all RAM (normal
  DM42n on/off usage keeps history), clears at the same seams as
  `forthCap` (RESET, restore validation — one choke-point rule,
  poison-swept with the close-paths class test). NO save-file
  persistence: new save keys + restore validation for a convenience
  buffer is not worth the format surface.
- **Rejected shapes (2026-08-04):** N fixed 256 B slots (worst
  bytes-per-line — 8 slots would equal the entire §5.4 dictionary
  ceiling) and arena allocation (history competing with the dictionary
  couples a convenience feature to compile capacity, and drags in
  allocator-lifetime machinery).

### L3 — discovery + durability surfacing (fold into L1)

No new machinery. (a) The FWRD picker inside an interactive capture lists
interactive + global words via the landed union-catalog sections — expected
to be nearly free once the CM gates open it. (b) DESIGN.md gains a short
§8.10-adjacent paragraph making the durability contract explicit: interactive
definitions die at the next lifetime consumption (§8.3, standing rule);
`GLOBAL` is the durability mechanism (gdict, survives lifetimes and
save/restore). (c) Cross-scope isolation (programs cannot see interactive
words — DESIGN-HISTORY, mutual-isolation ruling) is restated as deliberate,
not relitigated.

### L5 — REPL error ergonomics

On an interpret error, the capture reopens with the line intact so the user
edits instead of retyping; `lastErrorCode`/message surface exactly as today
(§8.7 — the S1 ruling against on-screen token display stands). Optional
half: place the cursor at the failing token (the outer loop knows the
position); costs a small plumb-through, no display override. Bundled with
L1 because the ENTER path decides where the text goes on failure anyway —
separating it would touch the same lines twice.

### Stage M (separate, not this stage) — §8.10 item 1 residue

Forth words visible to the wider UI: catalog/browse surfaces outside
capture and ASSIGN-to-key of a global word. The XEQ-name dispatch arms
already exist (items.c:698-718; the FLAG_USER-key sites share the shape per
DESIGN-HISTORY 2026-07-27ff), so the residue is browse/assign UX plus a
native ASSIGN grammar trace. Separable, bigger native-trace burden, and
worth its own owner statement. Recorded here only so L's scope cut is
explicit.

## Rulings — decision sheet (L-R4 open)

| # | Question | Ruling |
|---|----------|--------|
| L-R1 | Stage L scope | **RULED 2026-08-04: full — L1+L2+L3+L5.** Stage M stays deferred to its own owner statement. |
| L-R2 | String-in-X arm | **RULED 2026-08-04: always capture.** FORTH interactively always opens the capture; when X holds a string it seeds the line (over my recommendation to keep the one-shot arm — recorded, not relitigated). Spec consequences below. |
| L-R3 | ENTER semantics | **RULED 2026-08-04: REPL loop.** Run, reopen empty, EXIT leaves; error path per L5 reopens with the line intact. |
| L-R4 | Parameterized (TAM) items in interactive keys mode | **RULED 2026-08-04 on trace evidence: (b) build the fold.** Pressing STO 0 5 during an interactive capture types `STO 05` into the line, exactly as in PEM. Rationale (owner): one behaviour for one gesture — the fold is what the capture *means*, and an interactive capture that diverged from PEM would be a second thing to learn. Supersedes the architect recommendation of (a) and the owner-raised (c), both recorded below unchanged. Consequence: the three fold pieces (non-executing TAM, interactive suspend store, text synthesis) each get a trace before their packet — STAGE_L_TRACES.md §T7. |
| L-R5 | History RAM | ~~**RULED 2026-08-04, amended same day: 512 B packed variable-length ring**~~ — **SUPERSEDED BY L-R7 the same day.** Kept for the record: the ring was the right answer while history had nowhere else to live. Once the fold's scratch program existed, history had somewhere better. |
| L-R7 | History storage | **RULED 2026-08-04: history IS the fold's scratch program.** Committed lines accumulate as `ITM_FORTH` steps in one kept, named, runnable program; the L2 BSS ring is deleted. Persistence comes free (program memory is already a persisted format with restore validation — the exact cost L-R5 rejected persistence over). **Growth: soft cap with oldest-first eviction**, proposed budget 1024 bytes, tunable pinned by the battery. **Identity: a leading `LBL`**, proposed `FHIST` — NOT `FORTH`, which would shadow the item for `XEQ 'FORTH'` (items.c:698-718 tries labels first). **Running it re-runs the session, deliberately**; no execution guard. Rationale and the verified cost analysis: STAGE_L_TRACES.md §T7.2b. |
| L-R8 | Composing view (long lines) | **RULED 2026-08-04: accept native AIM behaviour; no display code.** The edit line sits on X's row (`AIM_REGISTER_LINE == REGISTER_X`, defines.h:1495), so X is never visible while composing; and once the line passes `SCREEN_WIDTH - 50` = 350 px measured in the **large** font (screen.c:1667), `_refreshNormalScreen` drops T/Z/Y and paints the line alone (screen.c:5933-5941). That threshold — order 18–23 glyphs — is reached by ordinary Forth lines. Consistency with alpha entry elsewhere wins over a Forth-specific layout. The architect recommendation (one-row line + horizontal scroll via `displayAIMbufferoffset`, keeping T/Z/Y at any length) is recorded, not taken. Purely a render decision with no state behind it, so it can be revisited additively later. Detail: STAGE_L_TRACES.md §T5. |
| L-R6 | Letter + branch | `L`, `forth-core/stage-l` — stands unless renumbered at branch creation. |

### L-R2 ruling consequences (architect spec notes, final wording post-trace)

- **Seed consumes X (drop at seed), copy-before-drop per §3.3.2.** The
  one-shot dropped the source string before interpreting "so interpreted
  words see a clean stack"; a seeded line that runs must see the same
  stack. Dropping at ENTER instead would leave the stale original under
  the line's results (and the line may have been edited meanwhile).
- **Abandon preserves the line.** EXIT on a non-empty interactive capture
  pushes the line onto the L2 history ring before closing, so drop-at-seed
  can never lose a line irrecoverably. (This generalizes: every
  interactive close path with non-empty text pushes an entry — run,
  abandon, power reset per the E14-analog seams. One rule, no cases.)
- **Oversize string in X** (≥ FORTH_SOURCE_MAX / the 256-byte-196-glyph
  capture cap): same error as today, no capture opened — the §3.3.2
  no-silent-truncation rule is unchanged.
- **PC-test/scripting entry unchanged in substance:** the documented test
  core is `forthOuterRun` (§3.3.2 "directly callable from PC tests");
  self-tests that drove `fnForthOuter`'s one-shot expectation re-target
  the core. The T2 trace inventories those callers.

### L-R4 background (for ruling) — why PEM's fold cannot just run interactively

What works in PEM today (landed F6-2 + F6-4, Stage K reuses it): during an
open capture, pressing a parameterized key — STO, RCL, GTO, anything that
prompts — enters TAM. In PEM, a committed TAM **inserts a program step**
(inert bytes, no execution). F6 exploits that: the capture *suspends* (its
text is safe in the placeholder step — the step IS the suspend store), TAM
runs natively and commits its real step, then F6-4 renders that freshly
committed step to text **through the landed decoder** (`decodeOneStep` —
so the spelling is canonical by construction, the same mimicry-equals-parity
argument as F4), splices the text into the capture line, deletes the step,
and resumes. Pressing STO 0 5 types `STO 05` into the Forth line.

Interactively, both load-bearing pieces are missing:

1. **TAM outside PEM executes.** Committing `STO 05` immediately stores X
   into R05 — the side effect happens at commit. There is no inert step to
   decode, and the store has already fired. A fold would need a
   **non-executing TAM variant**: every commit site in `tam.c` (six sites
   per the F6-2 trace: 217/552/587/896/918/1095) gated to capture the
   resolved (item, operand) instead of dispatching it, across the full F4
   operand grammar (direct, register, flag, named, indirect, min/max
   packing). That is exactly the kind of native-path surgery that made F6
   the highest-UI-risk stage of the series.
2. **No suspend substrate.** F6-2's suspend frees the buffer and resume
   refills from the step (forth_capture.h:14-21). No step interactively →
   suspension needs a new snapshot buffer with its own lifetime, poison
   sweep, and power-loss story — and it cannot be `aimBuffer`, which is
   precisely what TAM-cancel zeroes (the F6-1 lesson that forced the
   managed buffer PEM later got to delete again).
3. **Text synthesis.** With no committed step, canonical text must come
   from either a scratch-synthesized step record fed to the decoder
   (plausible; needs a trace that `decodeOneStep` is context-free enough)
   or a reimplementation of canonical spelling (rejected on principle —
   duplicating the F4 grammar is the parity bug factory F6-4 was built to
   avoid).

Cost of **(a) reject in v1**: in interactive keys mode a parameterized key
is a diverted no-op (K's E12 numlock-guard precedent). The user types the
same thing in alpha — `STO 05` is six presses — with canonical spelling
per the F4 grammar; direct items, digits, EEX, catalogs and the FWRD
picker all still insert. PEM capture keeps the full fold (landed code
untouched). Nothing in v1's architecture forecloses adding the fold later:
the divert seam L1 builds is exactly where a future fold would hook.

Cost of **(b) build the fold now**: the three pieces above — a
non-executing TAM mode, an interactive suspend store, and the synthesis
path — plus their acceptance battery (per-operand-class fold parity,
suspend/abandon/cancel/power paths). Realistic shape: 3–5 packets and a
tam.c trace series, roughly half an F6 riding on top of L1's already-new
dispatch seam. Doubling the risk surface of the stage that creates the
seam is the part I recommend against — not the feature, the coupling.

**Owner-raised alternative (2026-08-04), option (c) — live pass-through:
"why can't it behave like normal mode, like any other action?"** It can,
and it is *cheaper than the fold*: suspend the capture, let native TAM run
and **execute at commit** exactly as normal mode does (zero tam.c changes,
no decoder, no synthesis — fold pieces 1 and 3 vanish), then resume the
line. The one mandatory piece is the interactive snapshot store (fold
piece 2, ~256 B transient + cursor): TAM interactions write into
`aimBuffer` (named operands type there; TAM-cancel zeroes it), which is
where the capture line lives — without the snapshot, pressing STO destroys
the half-typed line. Resume-on-cancel mirrors E13's abandoned-suspension
shape, minus all step bookkeeping (no offsets, no step counts — text +
cursor only). Realistic shape: 1–2 packets.

Design costs of (c), stated for the ruling:

- **Verb split inside one mode.** Keys mode exists to *type source*:
  direct items insert their name as text (SIN types `SIN`). Under (c) a
  parameterized neighbor *executes instead* (STO fires a store, nothing
  enters the line) — two verbs on adjacent keys, and the surprising one
  is destructive (register overwritten) while the expected one is inert
  text.
- **PEM divergence.** The same STO press during a PEM capture produces
  text (landed F6-4). Same gesture, same capture context, different
  meaning by location.
- Mitigation of the underlying need under (a): the park-and-work flow
  already falls out of the ruled scope — EXIT pushes the line onto the
  history ring (L-R2 consequence), do any normal-mode work including STO,
  then FORTH + recall resumes composition. Two extra presses, no new
  machinery, no verb split.
- If (c) is ruled, it pre-builds fold piece 2: a later fold stage would
  reuse the snapshot store and change only what TAM commit does during a
  suspension. (c) is a stepping stone, not throwaway.

**Recommendation restated: (a)** — reject in v1: one verb per mode, PEM
consistency, and the EXIT/recall flow covers the live-work need. (c) is a
legitimate ruling if "the calculator never stops being a calculator"
outweighs mode purity — the build is honest and small. (b) remains
not-recommended as a rider.

**Owner ruling 2026-08-04, post-trace: (b), build the fold.** The
consistency argument won: the fold is what a capture *means*, and a
gesture that types text in PEM and does nothing interactively is a second
thing to learn for no gain. The traces did not change the shape of (b) —
its three pieces are still non-executing TAM, an interactive suspend
store, and text synthesis — but they did change the arithmetic against
its rivals: T3 finding 4 (`determineItem`'s AIM-first disjunct misroutes
every TAM keystroke to the alpha column when `calcMode == CM_AIM`) and
T3 finding 5 (the AIM line does not render while `tam.mode` is set) are
costs (b) and (c) share, so the gap between them is narrower than the
"half an F6 vs 1–2 packets" framing above. The `determineItem` fix and
the suspend store are now common infrastructure, not (b)-specific
overhead. What remains uniquely (b)'s is the non-executing TAM gate and
the text-synthesis path — and synthesis is the piece with real unknowns,
which is why it is traced before it is specified (T7).

The standing rejection of re-implementing canonical spelling is
unaffected and binding: the fold's text comes from `decodeOneStep` or it
does not ship. Duplicating the F4 grammar is the parity-bug factory F6-4
was built to avoid.

## Mandatory architect pre-work (before any packet is authored)

House rule (FSERIES_ROADMAP standing discipline): trace first; a packet
authored before its trace repeats the F1-5 P0 defect. Predicted traces:

- **T1 — interactive dispatch seam.** `determineItem` → `btnReleased` →
  `runFunction` outside PEM with FLAG_ALPHA set: where character items,
  catalog softkeys, and direct items actually flow, and the single site
  where an open interactive capture can divert direct items to text insert
  (the E0-equivalent). Deliverable: exact file:line + the gate expression.
- **T2 — AIM lifecycle.** Open/close seams (`ITM_AIM`, `fnKeyExit`,
  `fnKeyEnter` CM_AIM arms), `aimBuffer` consumers interactively (NIM
  collision list), what today's AIM ENTER does that the Forth ENTER must
  not (string-to-X commit).
- **T3 — TAM outside PEM.** Closes L-R4 (with T1): confirm the
  live-execution claim per item class; enumerate the divert list (the K
  E12 list re-derived interactively); pin the interactive suspend/resume
  choke points a pass-through or fold would hook (the tamEnterMode
  ordering fact, forth_capture.h:29-33, applies interactively too).
- **T4 — key semantics inside an open interactive capture.** ENTER; EXIT
  ladder (keys→alpha→close, extending E8); up/down — the history-recall
  candidates, but not free: they page multi-page softmenus and are
  SST/BST in PEM, so T4 resolves the recall-vs-paging collision
  (empty-line gating or shifted arrows as fallback); R/S (no step to
  record — propose: run the line; K E12's R/S rule has no interactive
  analog); backspace-on-empty.
- **T5 — render path.** Where the AIM line draws on the normal screen;
  confirm zero new display code; softmenu row behavior in keys mode
  (K-R3's underlying-menus rule interactively).
- **T6 — catalogs outside PEM.** What a catalog softkey does during
  interactive alpha today (native path), and where F6-3's insert-name
  behavior hooks without breaking native AIM catalog use outside a Forth
  capture.

- **T7 — the fold's three pieces (added by the L-R4 ruling).** Runs
  before any TAM-arm packet: (a) every TAM commit/termination site in
  `ui/tam.c` re-derived at current line numbers, and whether one choke
  point covers them; (b) whether the (item, operand) → step-bytes
  encoding can be produced into a caller-supplied scratch buffer without
  touching program memory, and what must be extracted to get there;
  (c) whether `decodeOneStep` is context-free enough to render that
  scratch buffer — the decisive question, since re-implementing the
  spelling grammar is rejected on principle; (d) the landed PEM
  suspend/resume walked statement by statement, separating the
  program-memory-bound half from the substrate-independent
  decode→text→`forthCapInsertName`→recommit chain; (e) everything
  clobbered across an interactive TAM, which is what the suspend store
  must hold. Each finding adversarially refuted before it is built on.

Packet decomposition (post-rulings, T1–T6 applied): **L1-0 re-target the
51 `fnForthOuter` self-test call sites** (T2 — must land first, or the
gate cannot distinguish L1's regressions from L-R2's intended behaviour
change); L1-1 capture object origin bit + open/close seams, incl. the
FIX-9 catalog-drain analog and the restore-sanitizer widen; L1-2
ENTER/EXIT/interpret loop + the 256-byte/196-glyph cap on the interactive
alpha path (T1 — PEM gets it from `pemAlpha`, AIM does not); L1-3 divert
seam (`runFunction`, before items.c:736) + catalog/picker/keys-mode gates
+ picker text-scan gate-off; **L1-F1..L1-F3 the fold** (non-executing TAM
gate, interactive suspend store, text synthesis), authored only after T7
lands; L1-4 history ring (L2, L-R5 as amended) with f-shifted up/down as
the recall gesture (T4 — unshifted arrows are case-change, paging, and a
destructive close, in that order); L1-5 acceptance battery + class tests
(close-path poison sweep extended to the interactive origin; the 17-row
CM-gate audit sweep). Sub-agent prompts inline the binding rules and
reference artifacts per the standing 2026-08-04 rule.

## Cost & risk

Revised 2026-08-04 for the L-R4 (b) ruling — the fold roughly doubles the
stage.

- **Flash:** order +3–5 KB (was +1.5–3 KB before the fold): new entry
  arm, divert seam, loop, gates, ring management, plus the fold's
  non-executing TAM gate, suspend store and synthesis path. Measured
  `make dmcp5r47` delta recorded in the stage commit (RULE-1); fine if
  justified, per standing policy.
- **RAM:** revised again by T7 and L-R7, downward twice. The interactive
  suspend store is **+8 bytes**, not ~260: the fold runs PEM's fold on a
  real materialised step, so `forthCaptureSuspend`/`Resume` are reused
  verbatim and the step is the store (T7.0). The L2 ring's **512 B is
  deleted outright** (L-R7) — history is program steps. **Net idle BSS
  for the whole stage: +8 bytes.** What history costs instead is
  *program memory*, capped at 1024 bytes with oldest-first eviction, in
  a region the user can see and clear; note that region shares its pool
  with the Forth dictionary (src/c47/memory.c:177-188 vs
  forth_dict.c:367), which is why it is capped rather than unbounded.
  Arena high-water reported per packet (§5.4); interpret path unchanged
  (private ctx on the C stack, §3.3.2).
- **Risk register:** (1) the CM_PEM gate audit — every F6/K conditional
  reviewed for widen-vs-keep, swept by a class test, since a missed gate
  either breaks native AIM or leaks capture behavior into plain alpha;
  (2) the T1 seam is new load-bearing dispatch code on the interactive
  keyboard path — the highest-UI-risk element, same class as F6's, with the
  same control (trace + PC-derivable fixtures + sim LCD verification via
  run-sim + DM42n stage-exit bench); (3) NIM/aimBuffer collisions —
  copy-before-interpret is normative, pinned by a test; (4) upstream-merge
  drift on keyboard.c/tam.c anchors — standing re-grep discipline;
  (5) **the fold's non-executing TAM gate is the stage's new worst case**
  — a gate that leaks the wrong way makes a parameterized key *execute*
  during an interactive capture (a register overwritten, silently) or
  makes a parameterized key *inert in normal mode* (native regression).
  Both directions get a class test, and the acceptance battery drives
  every operand class of the F4 grammar through the fold in PEM and
  interactively, asserting identical text; (6) **TAM with
  `calcMode == CM_AIM` is a state no shipped code has ever produced**
  (T3 finding 4) — `determineItem`'s resolution order and the AIM-line
  render gate both assume it cannot happen.

## Non-goals (explicit)

- No `.`-style output words: the stack display IS the output surface
  (extension principle); nothing prints.
- No terminal-style console (scrolling dialogue view, `EMIT`/`TYPE`
  output sink, `ok`/`.S` printing) — **PARKED by owner ruling
  2026-08-04.** A console is a superset of this stage (it still needs
  L1's line entry) plus a scrolling-text display subsystem and an
  output-channel subsystem the firmware does not have — F6-class UI risk
  with no existing surface to reuse (the PEM listing is the nearest
  precedent and is not reusable), ~3–5 extra packets, 1–2 KB idle RAM —
  for thin payoff on a machine whose stack is permanently visible. Its
  two genuine values are covered more cheaply: input scrollback = the L2
  ring; word-authored text output, if ever wanted, = a one-line message
  primitive as a later additive stage. Nothing in L forecloses a console
  later.
- No auto-durable interactive definitions: `GLOBAL` is the durability
  mechanism; no hidden source store, no replay-on-reset.
- No on-screen error-token display (S1 ruling stands).
- No persisted mode state of any kind (§8.4's debt-free invariant and E14
  discipline extend to the interactive origin).
- No change to cross-scope isolation.
