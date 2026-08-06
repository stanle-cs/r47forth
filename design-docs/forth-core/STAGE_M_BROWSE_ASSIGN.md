# Stage M — Forth words in the wider UI: browse, execute, ASSIGN

**Status: AUTHORED 2026-08-05 under the owner's delegation ("author stage
M and proceed with implementation", 2026-08-05). The decision sheet below
is architect-decided under that delegation — each ruling is revisitable
by the owner, and none forecloses a later reversal cheaply. Seed:
DESIGN.md §8.10 item 1 (deferred-additive), recorded at Stage L's scope
cut as "browse/assign UX plus a native ASSIGN grammar trace". Branch
`forth-core/stage-m` from the Stage L close (`4bfb4a136`). Not yet
normative — folds into DESIGN.md at landing.**

## Problem

RPN programs appear in catalogs and can be `ASSIGN`ed to a key; Forth
words can do neither. The FWRD catalog (`MNU_FORTH`) exists but is an
ALPHA submenu — reachable only while an alpha surface is up — and a word
softkey outside a capture has no defined meaning. A global word survives
lifetimes and save/restore (§8.3) yet cannot be put on a key, while the
machinery to dispatch exactly that already landed: the 2026-07-27
USER-key hardening gave `_executeItem` and `btnReleased` a
`forthFindColon → reallyRunFunction(ITM_FCALL, widx)` fallback behind
the same `calcMode == CM_PEM` recording guard the native label arm uses.
The asymmetry is now pure surface: nothing can *create* the assignment,
and nothing *lists* the words outside alpha.

## Inventory — what the recon found landed (verified this pass)

- **The record vocabulary already fits.** A program assignment is stored
  as `(ITM_XEQ, argumentName)` — `_assignItem`'s `ASSIGN_LABELS` arm
  (src/c47/assign.c:808-812) copies the label NAME into the user-key
  argument store (`setUserKeyArgument`, assign.c:1043;
  `userKeyLabel` block). The band constant (`ASSIGN_LABELS` 12000,
  defines.h:2242) exists only between pick and `assignToKey`; the
  persisted record carries no trace of it. **A Forth-word assignment as
  `(ITM_XEQ, wordName)` is indistinguishable in kind from a program
  assignment** — same store, same save format, same USER-mode label
  rendering, and the press-time dispatch is the landed H-hook, which
  tries `findNamedLabel` first and falls back to `forthFindColon`.
- **The pick path has one canonical shape to mirror.** In `CM_ASSIGN`,
  the softkey-resolution switch (src/c47/keyboard.c:265-273; the package
  override carries the same function) maps a dynamic-catalog pick to a
  band value: `case MNU_PROG/MNU_PROGS: return findNamedLabel(<picked
  name>, GLOBAL_LABELS) - FIRST_LABEL + ASSIGN_LABELS;`.
- **Execution by pick already exists inside TAM.** The dynamic-menu XEQ
  dispatch (items.c, `dynamicMenuItem >= 0`, the MNU_FORTH-picker path)
  resolves and — outside PEM — live-executes a picked word today; PEM
  records `XEQ 'NAME'` (2026-07-27 guards, test
  `test_pem_xeq_dynmenu_no_live_exec`).
- **The browse accessor exists** (`forthDictBrowseName`, F6-5), and the
  picker's content/paging/pixel behaviour is pinned to standard (G1-G4).
- **Press-time miss is already an error surface.** A name that resolves
  to nothing follows the standard not-found path (§8.7 discipline); no
  new message class is needed for a FORGET-ten word on a key.

## Scope (M-R1) — two thin seams plus their battery

**M1 — browse/execute outside capture.** `MNU_FORTH` becomes reachable
without an alpha surface (a row in the CATALOG tree), and a word softkey
pressed in plain `CM_NORMAL` executes the word — the same resolution the
TAM pick path uses, and the same "press = run" semantics the native
program-function menu gives programs. Inside XEQ-TAM, ASSIGN, and
captures, the landed behaviours stand untouched.

**M2 — ASSIGN of a global word.** In `CM_ASSIGN`, a FWRD pick of a
**global** word produces `itemToBeAssigned` in a new band; `_assignItem`
derefs it to `(ITM_XEQ, name)` exactly as the label arm does. Everything
downstream — storage, persistence, USER-mode display, press-time
dispatch, the PEM recording guard — is landed and untouched.

Out of scope, deliberately (see Non-goals): interactive-word ASSIGN, any
new record format or save key, any change to the 2026-07-27 dispatch
guards, MyMenu/user-menu authoring surfaces beyond what `(ITM_XEQ, name)`
already gives them, and any §4.3 label-ID synthesis.

## Rulings — decision sheet (architect-decided under delegation, 2026-08-05)

| # | Question | Ruling |
|---|----------|--------|
| M-R1 | Stage scope | **M1 + M2 as above.** The asymmetry §8.10 item 1 records is discharged for GLOBAL words; transient words stay capture/XEQ-name-only surfaces. |
| M-R2 | Which words can be ASSIGNed | **Global (gdict) only.** An assignment must not outlive its referent by design: interactive words die at the next lifetime consumption (§8.3), so a key naming one would dangle by construction the moment it is used as intended. A FWRD pick of a non-global word during ASSIGN **refuses** (the G1 blank-key-refusal precedent — no beep machinery, simply no state change). Revisitable: late binding means allowing it later costs one filter line. |
| M-R3 | Record + binding | **`(ITM_XEQ, name)`, late-bound at press time.** New band `ASSIGN_FORTH_WORDS` in the pick channel only, deref to the name at `_assignItem` time via the landed browse accessor. A missing word at press time is the standard not-found surface — no validation at restore (the record kind is the same one program names use; natively they restore unvalidated too). |
| M-R4 | Browse-execute semantics | **Press = run, PFN-parity.** In `CM_NORMAL` a word softkey resolves via `forthFindColonRef` and dispatches `reallyRunFunction(ITM_FCALL, widx)` — the identical call the landed TAM pick makes outside PEM. Listing = the picker's current sections (interactive + global; text-scan keys off `currentStep` as landed). Execution inherits landed XEQ-name lifetime semantics unchanged (no fresh-lifetime signal — same as typed `XEQ 'NAME'`). |
| M-R5 | The catalog entry point | **A `-MNU_FORTH` row in the CATALOG menu table** (package softmenus.c, one table edit). The FIX-9/L1-1 catalog-drain predicates and the E-rule close paths are re-audited against the new reachability and swept by a class test — the drain must treat a CATALOG-opened FWRD exactly as an alpha-opened one. |
| M-R6 | Letter + branch | `M`, `forth-core/stage-m`, branched from the Stage L close. |

## Mandatory architect pre-work (traces before any packet)

- **M-T1 — the pick dispatch, per mode.** For a `MNU_FORTH` softkey
  press: the exact chain in `CM_NORMAL` (today: what fires, where it
  dies), in XEQ-TAM (landed), in `CM_ASSIGN` (today: which switch arm
  eats it), and in native AIM without a capture (F6-5's insert surface —
  must stay untouched). Deliverable: file:line for the one site where
  M1's execute arm and M2's assign arm each hook, plus the PFN parity
  target's own dispatch (what exactly happens when a program is picked
  from the native program menu in `CM_NORMAL` — the behaviour M-R4
  mirrors).
- **M-T2 — the ASSIGN grammar.** The full `CM_ASSIGN` flow for a
  program-by-name assignment at current line numbers: source pick →
  `itemToBeAssigned` → destination key → `assignToKey` →
  `_assignItem` → `setUserKeyArgument`; the pending-assignment display
  path (the assign.c:744-area consumer); the numeric room above
  `ASSIGN_LABELS` (labelList max vs the int16 channel) pinning the
  `ASSIGN_FORTH_WORDS` value; and the gdict index's stability window
  between pick and deref.
- **M-T3 — persistence and display, zero-new-surface proof.**
  `userKeyLabel`/`kbd_usr` save/restore sites for named assignments;
  USER-mode softkey label rendering of an `(ITM_XEQ, name)` key; confirm
  a Forth-word record exercises not one new byte of format. Deliverable:
  the citation list DESIGN.md's fold-in will carry.
- **M-T4 — press-time resolution order.** The H-hook's order (native
  label first, then `forthFindColon`) restated against §4.1/§4.2's
  rationale; confirm a program and a global word sharing a name behave
  at a key press exactly as `XEQ 'NAME'` behaves typed. No change
  expected; the trace exists so the fold-in can cite it.
- **M-T5 — catalog-drain and close-path audit.** Every predicate that
  recognises "a Forth catalog is up" (`forthCatalogMenuOnTop`,
  `_forthCatalogBuriedOnStack`, the FIX-9 drain, L1-1's open-drain) and
  every CM-gate row from the L1-F3 audit that mentions catalogs,
  re-derived for CATALOG-tree reachability. Deliverable: the widen/keep
  disposition per site, each with its reason.

Each finding adversarially checked before it is built on (the T7.5
lesson: reachability, not write-set).

## Packet decomposition (predicted; final after traces)

- **M1-1 — the catalog row and the CM_NORMAL execute arm.** The
  `-MNU_FORTH` CATALOG row; the pick-site execute arm (M-R4) with the
  PEM guard mirrored from the landed TAM pick; the M-T5 dispositions
  applied; tests: press-runs-word (X changes as the word dictates),
  PEM-records (existing guard extended coverage), native-AIM insert
  untouched, drain class test.
- **M1-2 — the ASSIGN band.** `ASSIGN_FORTH_WORDS`; the `CM_ASSIGN`
  pick arm with the global-only filter (M-R2 refusal); the
  `_assignItem` arm and the pending-display consumer; tests: end-to-end
  assign → USER-mode press runs the word; non-global pick refuses;
  save/restore round-trip of the assigned key; FORGET-then-press hits
  the standard error surface; PEM-press records `XEQ 'NAME'`.
- **M1-3 — acceptance + close.** The stage story (assign a word, power
  cycle, press it, FORGET it, press it, reassign); DESIGN.md fold-in
  (§8.10 item 1 discharged; §4.2 gains the key-press column; §8.4-area
  cross-references); DESIGN-HISTORY; RULE-1 numbers; sim captures.

Implementation follows the Stage L1-5 pattern: fully-inlined
transcription packets offered to the local model first (loop-guard
thresholds raised, models pre-loaded), architect fallback per the
F6-1/L1-5C precedent without ceremony.

## Cost & risk

- **Flash:** order +0.5–1.5 KB (two dispatch arms, one menu row, one
  `_assignItem` arm, tests are sim-only). Measured `make dmcp5r47`
  delta recorded at close (RULE-1).
- **RAM:** zero expected. No new persistent state, no new BSS object;
  the band is a pick-time integer in an existing channel. Arena
  untouched (no dictionary change). Report per packet regardless.
- **Risk register:** (1) the CM_ASSIGN pick switch and the CM_NORMAL
  dynamic dispatch are native keyboard paths — the F6-class UI risk,
  controlled the same way (trace first, PC-driven fixtures, sim LCD
  verification, the CM-gate audit sweep); (2) the catalog-drain
  predicates were written for alpha-opened FWRD — a missed disposition
  either strands a menu or drains a native catalog (M-T5 owns it, class
  test swept); (3) the global-only filter leaking the wrong way makes a
  transient word assignable (dangling key) or a global word refusable
  (feature dead) — both directions get a test; (4) upstream-merge drift
  on assign.c/keyboard.c anchors — standing re-grep discipline.

## Non-goals (explicit)

- No ASSIGN of interactive or program-scoped words (M-R2; the durability
  contract §8.3 is the reason, not implementation cost).
- No new record format, save key, or restore validation — the stage's
  claim is precisely that `(ITM_XEQ, name)` already suffices; if a trace
  falsifies that, the stage STOPS and comes back to the owner.
- No §4.3 label-ID synthesis, no item-table entries for words.
- No change to the 2026-07-27 PEM recording guards or to capture
  behaviour (§8.4.x stands untouched).
- No MyMenu/user-menu authoring UI beyond what the record kind already
  gives those surfaces natively.
- No Forth-side ASSIGN word (assignments are a keyboard UI act, not a
  language feature; a program-text `ASSIGN` stage would need its own
  owner statement).
