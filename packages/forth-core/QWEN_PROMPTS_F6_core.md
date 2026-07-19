# Stage F6 — capture as a PEM-shaped submode + word catalog (stage ledger)

Origin: DESIGN §10.6.  Authored 2026-07-18 under the owner ruling of the
same date: **the hardware bench (charter Blocks A-F) is DEFERRED to
stage-exit confirmation** — packets are authored from the T1-T7 architect
traces in `F6_AUDIT_RESULTS.md` alone.  Every fixture below runs on the
PC-build self-test; the DM42n bench session remains queued and re-runs
the charter's Blocks A-F against the LANDED F6 behavior before the stage
may close.  Divergence found there is triaged by the architect (design
amendment vs upstream report vs fix); packets are never adapted by the
implementer.

This ledger is the architect/operator record.  Qwen never reads it; each
task packet is standalone (full preamble + fixture rule inline, per the
32d23fff1 self-containment rule).  Binding authoring rules: the ten rules
of `QWEN_PROMPTS_F3_core.md` §0 apply to every packet verbatim.

## 0. Authoring base and the re-verification obligation

Authored on the F3-2 tree (`e8f1f16cd`, F3-3 amendment in flight) —
FOUR stages ahead of F6-1's execution point (post-F5-2).  Anchor
stability (traced 2026-07-18, `F6_AUDIT_RESULTS.md` §0): of the files F6
touches, the still-unlanded F3-4..F5-2 packets modify ONLY
`programming/manage.c` (F5-2: the E9 gate line in `pemAlpha`'s ITM_ENTER
arm) — `ui/tam.c`, `keyboard.c`, `softmenus.c`, `screen.c`,
`saveRestoreBackup.c` are untouched by the queue.  Consequences:

- F6-1's execution gate greps BOTH today's anchors and F5-2's landed E9
  line; the E9 line is also an EDIT TARGET of F6-1 (argument re-point).
- Standing discipline applies unchanged: before handing any F6 packet to
  Qwen, the architect re-runs its EXECUTION GATE against the then-current
  tree; if a predecessor landed with deviations, the packet is
  RE-AUTHORED, never adapted.
- Packets F6-3..F6-6 reference code F6-1/F6-2 CREATE; their anchors quote
  the predecessor packets' normative text (the F3-4-quotes-F3-3
  precedent) and their gates grep the landed helpers.

## 1. What F6 builds (decomposition, trace-derived)

Six packets, linear gate chain, one commit each:

| Packet | Delivers | Key trace evidence |
|---|---|---|
| F6-1 capture object + managed buffer | `forth_capture.c/h`; FORTH capture text moves off `aimBuffer` into a managed 256-byte allocation held only while capture is open; every FORTH arm of `pemAlpha`/`pemCloseAlphaInput`/render re-pointed; interim TAM guard preserves landed commit-and-close | T1 arm map; T6 sink classes; tam.c:1172-1182; keyboard.c:3779 (aimBuffer zeroed on TAM-cancel — the buffer cannot stay there) |
| F6-2 TAM suspend/resume | entering TAM mid-capture SUSPENDS (snapshot) instead of committing-and-closing; cancel/commit RESUMES via the EDIT-reopen path; buffer freed during suspension (step bytes are the single source of truth) | tam.c:1172 seam; tam.c:1369 FLAG_ALPHA clear; T1 arm 7 incremental re-commit |
| F6-3 catalogs + menus during capture | FCNS/alpha-catalog pick inserts the item's catalog NAME as text at the cursor (P-H7 discipline); EXIT ladder pins; no-leak pins | T3 stack discipline; T4 picker path keyboard.c:1016-1019; F15-5 name-faithful precedent |
| F6-4 parameter entry emits text | TAM-eligible keys during capture run REAL TAM (over F6-2 suspend); commit renders the equivalent step through the decoder and inserts CANONICAL TEXT instead of a step (mimicry = F4 parity by construction) | T2; F4 grammar (landed by then); decode render path |
| F6-5 dictionary-backed word catalog | MNU_FORTH builder = UNION of the landed text-scan (edited program's words — no execution needed) + interactive fdict entries + gdict globals, sectioned; same menu id, same insert path | T4 (text-scan proven dictionary-blind); F3 scopes; rebuild-always softmenus.c:3184 |
| F6-6 acceptance battery | end-to-end: multi-line session, EXIT ladder walk, empty-backspace marker rule, power-off contract, 196/256 cap pins, suspend interplay, arena high-water sweep | whole audit; §8.9 canaries |

## 2. Load-bearing design decisions (ruled at authoring, 2026-07-18)

1. **The capture object** (`forth_capture.c/h`, new package-new files):

   ```c
   typedef enum { FCAP_CLOSED = 0, FCAP_OPEN = 1, FCAP_SUSPENDED = 2 } forthCapState_t;
   typedef struct {
     uint8_t     state;            /* forthCapState_t */
     uint8_t    *buf;              /* allocC47Blocks'd; NULL unless FCAP_OPEN */
     uint16_t    sizeBlocks;       /* TO_BLOCKS(FORTH_CAP_BYTES) while allocated */
     /* suspend snapshot — meaningful only in FCAP_SUSPENDED (F6-2): */
     uint16_t    savedCursor;      /* T_cursorPos at suspend */
     uint16_t    savedLocalStep;   /* currentLocalStepNumber at suspend */
     uint32_t    savedStepOffset;  /* capture step vs beginOfProgramMemory —
                                      an OFFSET because program memory can
                                      relocate/shift on TAM's step insert */
   } forthCap_t;
   #define FORTH_CAP_BYTES 256     /* same contract as the landed aimBuffer cap */

   No tam snapshot: `tamEnterMode` clobbers `tam.mode/function` BEFORE the
   CM_PEM seam fires (tam.c:1143 vs :1172), so a struct copy there is
   corrupt by construction — and the capture-era tam state is
   deterministic anyway ({mode 0, function ITM_FORTH}).  Resume
   reconstructs it; TAM's own entry/exit resets keep `tam.colon` and
   friends from leaking into the resumed capture (pinned by an F6-2
   no-leak test).  No display-offset snapshot: PEM renders the committed
   step, never the live buffer (T5/T6 — no screen.c read of aimBuffer
   under PEM capture), so `displayAIMbufferoffset` is CM_AIM-only state.
   ```

   The buffer is a FIXED-SIZE allocation: allocated at capture open,
   freed at close AND at suspend, never realloc'd while open — so it can
   never move while a pointer to it is live.  (fdict's relocation
   machinery exists because fdict GROWS; the capture buffer does not.)
   Nothing about it persists across power-off: T1 arm 7 proves the
   program step always holds the typed text, so power-on finds
   FCAP_CLOSED and a committed source step — the A5 contract.
2. **Cursor globals stay.**  `T_cursorPos` (byte index) and
   `displayAIMbufferoffset` remain the ACTIVE capture's cursor/render
   state — they are already the render contract (screen.c:3830-3838) and
   are shared with plain alpha.  The capture object snapshots them only
   across suspension.  No parallel cursor is introduced.
3. **The incremental re-commit is preserved bit-for-bit** (T1 arm 7):
   after every key the FORTH source step is deleted and re-inserted from
   the capture buffer exactly as it is today from aimBuffer.  This is
   what makes power-off safe and what F5-2's commit gate builds on;
   F6-1 changes WHERE the text lives, never WHEN the step is written.
4. **aimBuffer users that do NOT move** (T6 classes): REM/LITERAL
   captures, NIM (`pemAddNumber`), TAM name entry, CM_AIM proper.  Only
   the FORTH-typed capture sink moves.  F6-1 carries the
   machine-enumerated inventory gate (aimBuffer grep counts per file)
   so any drift is a STOP.
5. **F6-1 interim TAM guard.**  Moving the text off aimBuffer defeats
   tamEnterMode's `aimBuffer[0] != 0` protective close (tam.c:1172).
   F6-1 inserts an explicit `if (forthCapIsOpen()) forthCapCommitClose();`
   at that seam so the LANDED behavior (commit-and-close) is preserved
   verbatim until F6-2 replaces that exact line with suspend.  The
   interim has its own test pin and its own mutation.
6. **Suspend/resume shape (F6-2).**  Suspend (at the tam.c:1172 seam):
   save {cursor, currentLocalStepNumber, capture-step OFFSET};
   **currentStep does NOT move** — the landed commit-and-close nets to
   currentStep ON the committed line (pemCloseAlphaInput steps forward,
   tamEnterMode's arm steps back), and TAM commits insert through
   `addStepInProgram(tamOperation())` (traced: tam.c:217/552/587/896/918/
   1095), whose pre-move places the insert AFTER the current step — so
   staying on the line reproduces the landed insertion position exactly.
   Suspend also must NOT touch `tam.function`: tamEnterMode assigned the
   INCOMING TAM function before the seam fires, and zeroing it would
   break the TAM session (the landed pemCloseAlphaInput's unconditional
   reset at this seam is exactly what suspend replaces).  Clear
   FLAG_ALPHA, normal GUI, `_closeAlphaMenus`; FREE the buffer (step
   bytes are the single source of truth); state = FCAP_SUSPENDED.
   Resume at ONE choke point — `leaveTamModeIfEnabled`'s CM_PEM tail
   (every TAM exit, commit or cancel, funnels through it): revalidate the
   saved step structurally (in-bounds, opcode ITM_FORTH,
   STRING_LABEL_VARIABLE marker byte) — falsification abandons the
   suspension into plain PEM (defensive canary, unreachable today);
   re-open the buffer and refill from the step payload (len 0 = empty
   line, legal — the uniform suspend condition also covers the landed
   empty-line TAM edge, killing it); restore currentStep/localStep from
   the offset (stable across TAM's append-after insert), clamp+restore
   the cursor, `tam.function = ITM_FORTH`, FLAG_ALPHA, AIM GUI,
   `showSoftmenu(-MNU_ALPHA)`, resetShiftState (fresh-open parity).
   TAM's entry resets + leaveTam keep `tam.colon`/`dot`/`indirect` from
   leaking into the resumed capture (explicit no-leak pin).  A
   `forthCapOpen` on a still-SUSPENDED object abandons the orphan first
   (belt for exotic mode changes that skip the choke point).
7. **Parameter-entry mimicry (F6-4).**  On TAM commit while
   FCAP_SUSPENDED: build the step bytes the commit WOULD have inserted
   (the machinery already produced them), render through the landed
   decoder (`decodeOneStep` canonical text — the same renderer PEM
   displays), insert that text at the resumed capture cursor, and do NOT
   insert the step.  Parity with F4's grammar is by construction: F4's
   acceptance battery already pins text↔step round-trips through this
   same decoder.
8. **Catalog union (F6-5).**  Sections in order: (a) the edited
   program's words from the LANDED text scan (works with zero
   executions — the landed picker's exact builder, now one section);
   (b) interactive-scope fdict entries (owner == FORTH_OWNER_INTERACTIVE,
   unsmudged); (c) gdict globals.  Dedup within section by the landed
   rule; a name appearing in multiple sections appears in each (sections
   answer "where does it come from").  Scope filtering uses the F3-3A
   surface: the catalog is a BROWSE surface reading owners directly —
   it never enters a program-step scope (forthScopeEnterProgramStep is
   for step execution only).
9. **No new escape routes** (T7): SST/BST/step motion remain unreachable
   mid-capture; EXIT/ENTER/BACKSPACE(empty) stay the only structural
   exits.  The EXIT ladder is pinned in F6-3/F6-6 exactly as
   `isAlphaSubmenu` + the §8.4 table define it today.
10. **Arena discipline** (CLAUDE.md): every packet's gate reports the
    arena line; F6-1 and F6-6 additionally report capture-buffer
    high-water (alloc/free cycling leaves zero residue; the self-test
    asserts free-block equality across a full open/type/close cycle).

## 3. Deferred-bench register

See `F6_AUDIT_RESULTS.md` (bottom table): every charter experiment's
interim trace-derived substitute and residual hardware risk.  The bench
re-run happens after F6-6 lands and before stage close.  B1-B4 are
re-run against the NEW (suspend/resume) contract, not the landed one.

## 4. Status

| Packet | File | State |
|---|---|---|
| F6-1 | `QWEN_PROMPTS_F6_1_capture_buffer.md` | AUTHORED, gate-locked on F5-2 |
| F6-2 | `QWEN_PROMPTS_F6_2_tam_suspend.md` | AUTHORED, gate-locked on F6-1 |
| F6-3 | `QWEN_PROMPTS_F6_3_capture_menus.md` | AUTHORED, gate-locked on F6-2 |
| F6-4 | `QWEN_PROMPTS_F6_4_param_text.md` | AUTHORED, gate-locked on F6-3 |
| F6-5 | `QWEN_PROMPTS_F6_5_word_catalog.md` | AUTHORED, gate-locked on F6-4 |
| F6-6 | `QWEN_PROMPTS_F6_6_acceptance.md` | AUTHORED, gate-locked on F6-5 |
| bench | charter Blocks A-F on DM42n | DEFERRED → stage-exit confirmation [S+A] |
