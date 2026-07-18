# Stage F1.5 — §8.9 end-to-end acceptance harness: packet ledger

> Operator sequencing lives in `QWEN_RUNBOOK.md`. This file is the stage
> ledger only; every task lives in its own self-contained packet file (one
> packet per file — the F1 layout, kept per the owner's 2026-07-17
> instruction).

Origin: the Q1 ruling (DESIGN §8.9, 2026-07-15) scheduled the end-to-end
harness immediately after stage F1 so its lifecycle tests pin the landed F1
semantics; F1 completed 2026-07-17 (`1834901d3`..`10c04af4b`), and §8.3/§8.9
were reconciled to the landed mechanism in `6345f6c64` before any packet
here was authored. Items 3 and 8 of §8.9 are already fully covered by the
unit suite and get no packets.

## Authoring rules for this stage (lessons from the F1 cycle, binding)

1. Packets are authored **against the landed tree only** — every anchor
   grepped, every drive modeled on an existing green test, before the packet
   is written. No packet is authored ahead of its own verification pass
   (the F1-5 P0 defect came from authoring against unlanded semantics).
2. Every literal is **machine-verified at authoring time** (`printf '%s' …
   | wc -c` for payload lengths; fixture byte arrays proven against an
   existing in-tree fixture where one exists). Packets carry exact byte
   arrays — no free variables for the implementer to fill.
3. The three F1 preamble rules stay: log-captured gate runs inspected only
   through bounded greps, an on-disk todo file with MUTATION
   APPLIED/RESTORED markers, rule-9 compaction recovery. All `/tmp` paths
   are per packet (`…-f15-N-…`).
4. Mutations must name the packet's own subcase that goes RED and must be
   verified meaningful against the current tree (no obsolete mutation
   targets; see §8.9 item 9's replaced mutation).

## Status and dependency order

| Task | Packet | Status | §8.9 items | Dependency |
|---|---|---|---|---|
| F15-1 end-to-end run lifecycle | `QWEN_PROMPTS_F15_1_run_lifecycle.md` | DONE (`b773597bd`) | 1, 7(a,b), 9(a,b) | F1 complete + `6345f6c64` |
| F15-2 entry state + power-off round-trip | `QWEN_PROMPTS_F15_2_entry_state.md` | DONE (`5a9e9ce2d`) | 2(a-d) | F15-1 committed green (`b773597bd`) |
| F15-3 display parity across surfaces | `QWEN_PROMPTS_F15_3_display_parity.md` | DONE (`c8b87dfa8`) | 4 | F15-2 committed green (`5a9e9ce2d`) |
| F15-4 glyph operators + literal type parity | `QWEN_PROMPTS_F15_4_glyph_type_parity.md` | **READY TO EXECUTE** | 5, 6 | F15-3 committed green (`c8b87dfa8`) |
| F15-5 XEQ-by-name records a name step | — | TO AUTHOR (verification pass pending) | 10 | F15-4 committed green |

Packet F15-5 is deliberately **not** authored yet: it needs its
own architect verification pass against the tree its predecessor leaves
(rule 1 above). F15-4 was authored against landed F15-3 (`c8b87dfa8`) with
exact alpha-item sequences, 38-byte glyph programs, the real RPN NIM-close
drive, and both safe mutation anchors verified. Design notes fixed so far,
from the completed inventory:

- **F15-2 (landed):** `forthEntryStateAtCursor` /
  `forthEntryStateAtInsertion` (forth_bridge.c) are pure probes over
  `currentStep`/program bytes; the packet extends the unit analog with real
  cursor motion (`fnGotoDot`), real digit dispatch (`runFunction(ITM_2)`),
  and the 2(d) power-off round-trip modeled on `test_save_restore_roundtrip`.
  Mutation: the §8.9 item-2 static-bool replacement of
  `forthEntryStateAtInsertion`.
- **F15-3 (landed):** the marker renderer is `decodeRem`
  (programming/decode.c, `forthMarkerTurnsOn` call); the packet compares the
  real PEM listing's three framebuffer rectangles with fixed font-rendered
  tokens, then drives the same program through SST/BST and exact `tmpString`
  probes. Its first mutation pass caught an architect-side blind oracle
  (`screenData` is only the GTK presentation copy); the landed test samples
  the real `lcd_buffer` and the corrected mutation reddens all three surfaces.
- **F15-4 (authored):** glyphs are two-byte sequences — `STD_CROSS` `"\x80\xd7"`,
  `STD_DIVIDE` `"\x80\xf7"` (src/c47/fonts.h:183,216); the prim table
  carries alias rows `PRIM_CROSS`/`PRIM_DOT`/`PRIM_DIVGL`
  (forth_prims.c). Item 5's §8.9 mutation "delete the PRIM_DIVGL row" is
  UNSAFE as written — the table uses designated initializers, so deletion
  leaves a NULL `.name` hole for `forthFindPrim` (forth_dict.c:335) to
  strcmp; the packet uses rename-to-sentinel instead. Item 6's RPN half
  drives the verified `addItemToNimBuffer(ITM_7)` + `closeNim()` path
  (src/c47/bufferize.c:837,2342) before comparing it with a real source step.
- **F15-5:** hardest drive (PEM XEQ + alpha through tam); precedents exist
  (`reallyRunFunction(ITM_FCALL, …)` dispatch tests and direct `tam.*`
  manipulation in the suite around test_dict_reloc.c:5266). Needs its own
  tam-chain verification before authoring.

Execute strictly in order, one packet per session, clean green tree each;
every packet's EXECUTION GATE must pass before its first edit. After each
stage commit, the architect authors the next packet against the new tree,
then hands it to the operator. The §5.4 arena rule applies to every packet:
the acceptance run reports the dictionary high-water mark.
