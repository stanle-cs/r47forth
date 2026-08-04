# Defects — capture text round-trip, toggle-close, buried catalog menu (found 2026-08-04)

All three surfaced during the Stage K research passes (six-tracer research
sweep + six-tracer pre-work verification, same day). **D-C1 and D-C2 are
CONFIRMED by static trace** (T5/T4 verdicts folded below); D-C3 is suspected
pending reproduction. None has a live reproducer yet — that is each fix
packet's first deliverable, per the bug-fix testing rule
(FSERIES_ROADMAP.md standing discipline, ruled 2026-08-04: reproducer +
named class + class test where the class is enumerable). Class assignments —
D-C1: emit/accept parity; class test = round-trip sweep over every decodable
parameter form. D-C2: capture-close completeness; class test = the
poison-every-close-site sweep (shared with Stage K's E14). D-C3:
softmenu-stack reconciliation across suspend/resume; class test shape set at
repro time. Standard of the 2026-07-25 ruling applies:
anything that behaves differently from R47 is a bug; anything that emits
text its own compiler refuses is one too.

---

## D-C1 — F6-4 fold emits quote glyphs the compiler cannot re-read

**Severity: CONFIRMED bug (trace T5, 2026-08-04). Committed source that
fails its own re-compile — silently, for everything except XEQ.**

Trace verdict (sharper than the original suspicion): the ENTER-time
visibility is decided by an accident of naming. `forthCheckSourceLine`'s
check mode skips the item/parameterized-item/label branches entirely
(forth_compile.c:1057-1062), so a folded `GTO '<glyph>NAME<glyph>'` — or any
non-XEQ named form — passes E9, COMMITS, and fails only when program flow
executes that step (top-level lines are skipped even by pre-scan,
forth_compile.c:844-859). Only the folded native `XEQ` collides with Forth's
structural ASCII `XEQ` keyword, which check mode does fully parse — so that
one case is refused at ENTER. Same bug, opposite user-visible behavior.
Compiler-side glyph acceptance is collision-safe against the number grammar
(any byte ≥0x80 disqualifies a number, forth_compile.c:456-461); the only
residual risk is structural — names that contain the quote glyphs as content
are legal today (no instance found in tests or dictionaries).

`forthCaptureResume`'s F6-4 fold renders a suspended TAM commit to canonical
text with `decodeOneStep` — the shared PEM listing renderer — which spells
quoted parameters with the directional display glyphs
`STD_LEFT_SINGLE_QUOTE`/`STD_RIGHT_SINGLE_QUOTE` (`\xa0\x18`/`\xa0\x19`)
[decode.c:160-248]. The compiler's quoted-name parsers accept **only** ASCII
`0x27` [forth_compile.c:288-310 (parseQuotedName), 1511-1531
(forthParseXeqForm); V4 ruling: no alternate spellings]. No normalization of
the glyph quotes exists anywhere in the package (grepped 2026-08-04).

Consequence by code reading: a capture line that received e.g. `XEQ 'WA'`
via the TAM fold carries glyph-quoted text; when that line is committed and
later compiled (pre-scan or interactive), the quoted form cannot parse as
emitted. The landed F6-4 test pins the glyph-quoted *text* but never
compiles it [test_capture.part.h:3720-3725].

Open (trace T5): whether `forthCheckSourceLine` (E9 tier 1, at ENTER)
already refuses the line — i.e. the user is blocked at commit immediately
after a successful-looking TAM entry — or whether the failure surfaces later
at pre-scan. Either way the flow is broken; the tier determines how loud.

**Fix direction (proposed, not ruled):** accept the directional glyphs
compiler-side — fold `\xa0\x18`/`\xa0\x19` to `0x27` at the tokenizer or in
the two parsers, the same shape as sort.c's arrow folding. Changing the emit
side is wrong: `decodeOneStep` is the global PEM listing renderer and its
output spelling is a display convention. Collision risk assessment is part
of T5.

**Sequencing:** must land before Stage K (keys mode makes parameterized
folds a primary flow). Tag: FIX-7.

---

## D-C2 — ITM_FORTH toggle-close arm leaves forthCap.state == FCAP_OPEN

**Severity: CONFIRMED code defect (trace T4, 2026-08-04); UNREACHABLE today,
ACTIVATED by Stage K.**

Trace verdict: the omission is structural — the F6-1 packet enumerated the
pemAlpha open/close retrofit sites and never listed this arm; its mutation
run covered exactly the two forthCapClose sites. Reachability today is nil:
ITM_FORTH lives only in the FCNS catalog, and during a capture the key
carrying -MNU_CATALOG is invisible (its AIM column is -MNU_AIMCATALOG,
characters only) — matching the existing defensive test's own comment
("not known to be user-reachable today", test_engine.part.h:8628-8651).
Stage K's column swap is precisely what makes -MNU_CATALOG → FCNS → FORTH
reachable with a capture open, so **FIX-8 must land with or before Stage K**.
Confirmed downstream misbehavers on the stale state: the tamEnterMode
suspend seam (destructive recommit + bogus FCAP_SUSPENDED transition) and
`forthCapTextNonEmpty` in fnKeyExit (EXIT-ladder currentStep resync
misroute); pemAlpha's open-block read is masked by its unconditional
aimBuffer clear.

`insertStepInProgram`'s ITM_FORTH arm, wasOn == true branch (region close),
clears FLAG_ALPHA and zeroes tam.function but never calls `forthCapClose()`
and never clears aimBuffer [manage.c:1708-1719]. The only production
`forthCapClose` sites are pemAlpha's backspace-abort and both
`pemCloseAlphaInput` branches [manage.c:960, 1090, 1095].

Reachability hypothesis: during an open capture, physical keys are alpha
letters, but F6-3 makes catalogs live — a catalog pick of FORTH routes
through runFunction → insertStepInProgram, where E0's first arm explicitly
excludes `func == ITM_FORTH` from the alpha divert [manage.c:1643], dropping
it into the toggle arm with the capture still OPEN. Result: FCAP_OPEN with
the alpha UI torn down. Later consumers that would then misfire: the
tamEnterMode suspend seam (`forthCapIsOpen()` true with no live capture
line), `forthCapTextNonEmpty` in fnKeyExit, and pemAlpha's open block.

Two tracers flagged this independently; no test drives FORTH-from-catalog
with FCAP_OPEN. Verification (trace T4): confirm ITM_FORTH is listed in a
catalog reachable during capture, then the repro is: open capture → open
FCNS → pick FORTH → inspect forthCap.state.

**Fix direction (proposed, not ruled):** the toggle-close arm calls
`forthCapClose()` (and clears aimBuffer) when `forthCapIsOpen()`, mirroring
the pemCloseAlphaInput discipline; plus the missing test (open a LIVE
capture, drive the E1 closing branch catalog-shaped, assert
`forthTestCapState()` — none of the three nearest tests does all three).
Tag: FIX-8, lands with or before Stage K.

---

## D-C3 — Catalog-initiated TAM during capture buries a catalog menu that later eats the ALPHA menu

**Severity: suspected bug (trace T2, 2026-08-04 — static only). Reachable
TODAY; independent of Stage K.**

Route: during an open capture, pick a TAM item (STO/RCL/…) from a catalog.
The catalog-family softmenu (e.g. -MNU_FCNS) is necessarily on the stack
when `tamEnterMode` pushes the TAM menu on top of it; `_closeCatalog()`
declines to pop while a TAM menu is current ("TAM menus are processed
elsewhere", keyboard.c:468-483), `leaveTamModeIfEnabled` pops only
`numberOfTamMenusToPop == 1`, and `forthCaptureResume`'s
`showSoftmenu(-MNU_ALPHA)` does no stack-wide check — so the catalog entry
survives, buried. `_closeCatalog()` decides "in a catalog" by scanning the
WHOLE stack (keyboard.c:461-466), and -MNU_ALPHA is itself on the
CatalogMenus[] closable list — so the next softkey dispatch that runs
`_closeCatalog()` finds the buried entry and pops the freshly-restored
ALPHA menu. This is trap #6's exact shape (the E1 arm got the
`_forthCatalogBuriedOnStack` bounded drain for it; the TAM suspend/resume
path has no equivalent).

Not yet reproduced live; existing F6-2/F6-4 tests drive the suspend seam via
direct `runFunction` calls with no catalog stack, and the one real-catalog
test covers the ITM_FORTH-open case, not TAM. Repro sketch: open capture →
CATALOG→FCNS (via the alpha-reachable AIMCATALOG? — no: FCNS requires
-MNU_CATALOG; today's in-capture catalog routes need enumeration, which is
part of the repro work) → pick STO → complete TAM → observe softmenu stack
and next softkey dispatch. If in-capture FCNS turns out unreachable today,
this collapses into a Stage-K-activated defect like D-C2 — the repro
determines which.

**Fix direction (proposed, not ruled):** apply the E1-style bounded drain
(or an equivalent stack reconciliation) at `forthCaptureResume` before
`showSoftmenu(-MNU_ALPHA)`. Tag: FIX-9.
