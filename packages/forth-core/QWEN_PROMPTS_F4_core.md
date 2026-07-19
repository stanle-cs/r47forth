# Stage F4 — Series C textual parameters: ledger + grammar trace + design pass

> Operator sequencing lives in `QWEN_RUNBOOK.md`; the series-wide plan in
> `FSERIES_ROADMAP.md`.  This file is the stage ledger, the §10.4 grammar
> trace (the R4-C2 obligation: "the future C prompt must carry the exact
> grammar and error table; Qwen must not fill them in from examples"), and
> the F4 design pass.  Authored 2026-07-18 on the post-F3-authoring tree;
> the packets are gate-locked on the F3-7 stage commit
> (`forth-core: F3-7 — pin XEQ resolution parity and close stage F3`) and
> every packet re-verifies its anchors before edit.

The §0 binding authoring rules of `QWEN_PROMPTS_F3_core.md` (the F2 error
record) apply to every F4 packet verbatim.

## 1. The trace (2026-07-18, file:line evidence, tree `18d8c8056`/src upstream)

### 1.1 Classification facts

- **Upstream's own flow predicate** [src/c47/items.c:262]:
  `funcIsProgramStopControl = (func == ITM_END || func == ITM_RTN ||
  func == ITM_STOP || func == ITM_RTNP1)` — END 1458, RTN 4, STOP 70,
  RTN+1 1579.  All four are `CAT_FNCT | PTP_NONE` [items.c:1787, 1853,
  3275; RTNP1 by id], so they RESOLVE from Forth source today through the
  §4.1 step-4 filter.  F4 closes that hole: §10.4's "control/declarative
  steps stay rejected with ERROR_OPERATION_UNDEFINED" becomes true by an
  explicit reject of exactly this set at step 4.
- **CASE is PTP_REGISTER** [items.c:3235] — flow semantics inside a
  register class, so classification cannot be class-only; CASE joins the
  item reject list.
- **FCALL is PTP_NUMBER_16** whose parameter is an index into Forth's own
  per-lifetime dictionary — never expressible as durable source text
  (names-only invariant, §4.2/§8).  Item reject list.
- **KEYG/KEYX and 42KEYG/42KEYX are PTP_DISABLED** [items.c:3315-3316,
  4654-4655] — already unreachable by name (they fail CAT/PTP gates).
- **PTP class ranges** [packages/forth-core/defines.h:1056-1071]: parameter
  classes are `PTP_DECLARE_LABEL (1<<9) .. PTP_MENU (12<<9)`; PTP_NONE /
  PTP_LITERAL / PTP_REM / PTP_DISABLED sit outside (the F3-6
  `forthFindItemParameterized` filter — its INVALID_NAME arm is the seam
  F4-1 replaces).
- **Class rejects (flow/declarative/unmappable):** PTP_DECLARE_LABEL (LBL
  — declares program structure), PTP_LABEL (GTO 2, XEQ+1 2223 — XEQ
  itself is the F3-6 structural bridge), PTP_SKIP_BACK (BACK 1412, SKIP
  1603 — step-relative jumps), PTP_COMPARE (x=? family — native semantics
  are "skip the next program STEP on false" [decode PARAM_COMPARE +
  upstream runner]; inside a token-threaded body there is no next step to
  skip — unmappable), PTP_KEYG_KEYX (key/label declarative).
- **Eligible classes:** PTP_REGISTER, PTP_FLAG, PTP_NUMBER_8,
  PTP_NUMBER_16, PTP_NUMBER_8_16, PTP_SHUFFLE, PTP_MENU — minus the item
  reject list {CASE, FCALL} ∪ funcIsProgramStopControl's set (the latter
  are PTP_NONE and rejected at step 4).

### 1.2 The tamMinMax encoding and entry rules

- `TAM_MAX_BITS = 14`, `TAM_MAX_MASK = 0x3fff` [src/c47/defines.h:1018-1019]:
  an item row's `(min << TAM_MAX_BITS) | max` carries BOTH bounds —
  e.g. KEYG `(1 << TAM_MAX_BITS) | 21` = min 1, max 21 [items.c:3315].
  SDL/SDR: max 99, min 0, PTP_NUMBER_8, TM_VALUE [items.c:2216-2217].
  CNST: max `NOUC-1`, PTP_NUMBER_8_16 [items.c:1994].  STO/RCL and the
  STO×÷↑↓/RCL×÷↑↓ family: PTP_REGISTER max 99 [items.c:1827-1838,
  3247-3362].  SF/CF: PTP_FLAG max 99 [items.c:1893-1894].
- TAM digit entry accumulates `value*10 + digit ≤ max` with a min check at
  the final digit [src/c47/ui/tam.c:742-748] — leading zeros are
  insignificant ("5" ≡ "05"); no sign, no base prefix, decimal only.

### 1.3 The KS-code map (the REGISTER/FLAG byte space) [src/c47/defines.h:1318-1375]

- Numbered global registers: KS 0..99.
- Lettered: KS 100..111 = X Y Z T A B C D L I J K; KS 211..216 =
  M N P Q R S; KS 217..224 = E F G H O U V W (all uppercase, matched via
  `indexOfItems[ITM_REG_X + n].itemSoftmenuName` in decode).
- Local registers `.00`..`.98`: KS 112..210
  (`FIRST_LOCAL_REGISTER_IN_KS_CODE = 112`).
- Flags [defines.h:792-822]: numeric global 0..99; lettered FLAG_X..FLAG_K
  = 100..111 and FLAG_M..FLAG_W = 211..224 (same letters); LOCAL flags
  `.00`..`.31` = 112..143 (`NUMBER_OF_LOCAL_FLAGS 32` — NOT 99; 144..210
  are illegal, decode rejects them).
- Marker bytes [defines.h:1364-1374]: 249 LOCAL_LABEL_VARIABLE,
  250 CNST_BEYOND_250 ≡ SYSTEM_FLAG_NUMBER (context-disjoint), 251
  VALUE_0, 252 VALUE_1, 253 STRING_LABEL_VARIABLE, 254 INDIRECT_REGISTER,
  255 INDIRECT_VARIABLE.
- `regKStoC`/`regCtoKS` are the inline converters [defines.h:1395-1407];
  `regInRange` (store.h:11, body store.c) is the dispatch-time range gate.

### 1.4 Canonical spellings (decode.c:169-400 — the LISTING is the grammar)

- REGISTER: `NN` (two-digit render, entry accepts 1-2 digits), letter,
  `.NN`, `'NAME'`, `→NN` / `→letter` / `→.NN` (INDIRECT_REGISTER),
  `→'NAME'` (INDIRECT_VARIABLE).
- FLAG: `NN`, letter, `.NN` (0..31), `'SYSFLAGNAME'` (marker 250 + byte;
  names from the SFL_ item range: `indexOfItems[b + SFL_TDM24]` for b<64
  else `indexOfItems[b + SFL_MONIT - 64]` [decode.c:279-286]), plus both
  indirect forms.
- NUMBER_8: bare digits, width-rendered by tamMax; both indirect forms
  are legal [decode.c:314-318].
- NUMBER_8_16 (CNST): digits 0..249 direct; 250..499 as marker
  CNST_BEYOND_250 + extension byte [decode.c:341-343]; indirect legal.
  **CNST (207) is the ONLY PTP_NUMBER_8_16 item and its max is NOUC−1 =
  83** [items.c:1994, defines.h:1100] — the extension form is therefore
  UNREACHABLE from the source grammar (range-capped); the Forth decoder
  and validator still accept `[250][ext]` for format completeness (native
  steps may carry it), pinned by hand-built bodies only.
- **No existing test pins RTN/STOP/END/RTN+1 dispatch from Forth source**
  (grep `"RTN"\|"STOP"\|"END"` over test_dict_reloc.c: zero name-dispatch
  hits, 2026-07-18) — the F4-1 flow-reject behavior change migrates no
  tests.
- NUMBER_16: digits; old-form items (`isFunctionOldParam16` — exactly
  {ITM_BESTF_OLD, ITM_RNG_OLD, ITM_YY_DFLT_OLD, ITM_DENMAX2_OLD}
  [items.c]) are little-endian with NO indirection; new-form items are
  BIG-endian in the native step with indirection [decode.c:356-377].
- SHUFFLE: `xyzt`-style 4-character form, LOWERCASE, 2 bits per position
  via `shuffleReg[4] = {'x','y','z','t'}` [decode.c:10, PARAM_SHUFFLE arm];
  the single item is 1694, whose NAME is the glyph
  `STD_RIGHT_OVER_LEFT_ARROW` [items.c:3511].
- MENU (OPENM 2405 [items.c:4229]): NAMED form only —
  `OPENM 'MENUNAME'` (STRING_LABEL_VARIABLE + inline name).
- **Quotes in SOURCE are ASCII 0x27.**  The typeable quote is `ITM_QUOTE`
  (813), which inserts `STD_QUOTE = "\x27"` [items.c:2616, fonts.h:56] and
  sits on `menu_alphaMisc` [softmenus.c:695].  The directional glyphs
  `\xa0\x18`/`\xa0\x19` are DISPLAY-side only (decode/TAM rendering);
  Forth source lines render bare (§8.5), so source shows exactly the 0x27
  the user typed.  This confirms F3-6's delimiter choice and binds F4's
  named forms to the same 0x27.  The indirection arrow is the two-byte
  glyph `STD_RIGHT_ARROW` (fonts.h) and passes through the glyph-wise
  tokenizer intact.

### 1.5 The dispatch tail (already shared — F2's investment)

- `paramCoreExecuteOp`'s PARAM_REGISTER/PARAM_COMPARE arm
  [packages/forth-core/programming/param_core.c]: direct KS ≤ 224 →
  `regInRange(regKStoC(ks))` then dispatch — out-of-range is the traced
  SILENT no-op; 253 → bounded name read + `findNamedVariable`, with
  `tryAllocate` → `findOrAllocateNamedVariable` (CREATE) else
  `ERROR_UNDEF_SOURCE_VAR`; 254/255 → the indirect helpers.
- `tryAllocate` = `isFunctionAllowingNewVariable(op)`
  [src/c47/registers.c:2387]: exactly {INPUT, STO, STO+ − × ÷, KEYQ,
  M_DIM, MVAR, SOLVE, PLTf, STOCFG, STOMAX, STOMIN, X→ALPHA, Xex, Yex,
  Zex, Tex, INTEGRAL, INTEGRAL_YX} — create semantics are INHERITED by
  dispatching through the shared arm, never re-implemented.
- `paramCoreValidateDirect`/`DispatchDirect` (F2-3/F2-5) carry the direct
  NUMBER_8/NUMBER_16 semantics both engines share.

## 2. The design pass (DECIDED, no open choices)

1. **One parsing-word seam.**  F3-6's `forthFindItemParameterized` hit
   (currently `ERROR_INVALID_NAME`) becomes the Series-C entry:
   - id ∈ {ITM_END, ITM_RTN, ITM_STOP, ITM_RTNP1} — checked at step 4
     BEFORE the PTP_NONE dispatch — or id ∈ {ITM_CASE, ITM_FCALL} or
     class ∈ {DECLARE_LABEL, LABEL, SKIP_BACK, COMPARE, KEYG_KEYX} →
     `ERROR_OPERATION_UNDEFINED`, atomic (abort-if-open, stop line).
     This CHANGES landed behavior for END/RTN/STOP/RTN+1 (they dispatch
     today); any test pinning that dispatch migrates in F4-1.
   - eligible class → consume EXACTLY ONE next token and parse per class
     grammar (§1.4).  No lookahead beyond one token; the token after the
     parameter is ordinary source.  Missing next token or malformed form
     → `ERROR_INVALID_NAME`, atomic.  Value outside [min, max] (from
     `tamMinMax`) or an out-of-range local (`.99`, flag `.32`) →
     `ERROR_OUT_OF_RANGE`, atomic.  Compile-state failures abort the open
     definition (C2 question answered: YES, atomic).
   - DEFS_ONLY needs NO carve-out: in a tail, both the item token and its
     parameter token are independently skipped by the interpret-state
     gate; inside a definition the pre-scan compiles normally.
2. **FTOK_C47 extended param cells** (§2.2-compatible, all cell-aligned):
   - direct REGISTER/FLAG/NUMBER_8/SHUFFLE: `[value][0]` (one cell; value
     = KS byte, flag byte, N8 value, or packed shuffle byte);
   - direct NUMBER_16: `[lo][hi]` LE (LANDED, unchanged — the validator
     and save format pin it; old/new native endianness is a STEP-format
     concern only);
   - CNST 250..499: `[250][ext]` (one cell);
   - named variable / menu name: `[253][len]` + name bytes zero-padded to
     cells;
   - system flag: `[250][sysByte]` (one cell; class FLAG context);
   - indirect register: `[254][ks]` (one cell);
   - indirect variable: `[255][len]` + name bytes zero-padded to cells;
   - **indirect NUMBER_16 is EXCLUDED from source** — a `[254][ks]` cell
     is indistinguishable from a legal LE direct value whose low byte is
     254 (e.g. 510), so the marker grammar cannot ride the landed LE
     encoding; native steps remain the only carrier.  Documented
     limitation, pinned by a reject test.
   Decoder disambiguation per class: markers 250/253/254/255 live above
   every legal direct byte for their class (REGISTER/FLAG direct ≤ 224 KS,
   N8 direct ≤ tamMax ≤ 0x3fff&mask but stored as one byte ≤ 249, CNST
   direct ≤ 249), so byte0 classifies the cell.  NUMBER_16 alone reads
   the whole cell as LE value (no markers, per the exclusion).
3. **Dispatch reuse, not re-implementation.**
   - Direct REGISTER/FLAG join `paramCoreValidateDirect` /
     `paramCoreDispatchDirect` with arms that mirror the extracted native
     ones: REGISTER validates `regInRange(regKStoC(ks))` and dispatches
     `reallyRunFunction(op, regKStoC(ks))`; out-of-range keeps the traced
     SILENCE (validate=false, no error).  FLAG validates the traced legal
     byte ranges; SHUFFLE dispatches the packed byte; NUMBER_8_16 direct
     ≤ 249 dispatches the byte, `[250][ext]` dispatches `250 + ext`.
   - Marker forms (253/254/255, FLAG 250): the runtime and interpret
     paths assemble a small NATIVE-SHAPED buffer `[marker][payload…]` and
     call **`paramCoreExecuteOpBounded(buf, bufEnd, op, PARAM_mode)`** —
     a NEW explicit-end variant; the landed `paramCoreExecuteOp(addr, op,
     mode)` becomes a wrapper passing `firstFreeProgramByte` (F2-2's
     bounded-name contract, now with a caller-owned end).  This inherits
     range checks, create semantics, `ERROR_UNDEF_SOURCE_VAR`, and
     indirection resolution byte-for-byte from the shared core.
4. **Validator/GLOBAL-walk growth.**  `vBodyWalk`'s FTOK_C47 arm and the
   F3-4 GLOBAL validate/rewrite walks gain the same per-class cell
   grammar (advance = 2 for one-cell forms; `2 + ceil2(len)` for
   253/255 forms with len 1..31 and zero pad checks; byte0 legality per
   class; NUMBER_16 stays plain-cell).  Reject anything else.
5. **Errors — the table the packets carry verbatim:**

   | Condition | Error |
   |---|---|
   | Flow/declarative item name (class or item reject, incl. END/RTN/STOP/RTN+1) | `ERROR_OPERATION_UNDEFINED`, atomic |
   | Eligible item, missing/malformed parameter token | `ERROR_INVALID_NAME`, atomic |
   | Well-formed value outside [min,max] / illegal local index / illegal letter | `ERROR_OUT_OF_RANGE`, atomic |
   | Named form on a class without a 253 arm (e.g. `SDL 'X'`) | `ERROR_INVALID_NAME`, atomic |
   | Indirect NUMBER_16 spelling | `ERROR_INVALID_NAME`, atomic (documented exclusion) |
   | Missing variable at DISPATCH, non-creating item | `ERROR_UNDEF_SOURCE_VAR` (inherited from the shared arm) |
   | Direct register/flag out-of-range at DISPATCH (corrupt body) | traced native SILENCE (no error) — parity-pinned |

6. **Non-goals for the stage:** no stack-idiomatic forms (`5 STO` stays
   rejected — §4.4), no indirection arrow aliases (`IND` is NOT accepted;
   the glyph is the only spelling — V4), no new save format, no PEM/entry
   changes (F6), no check-only mode (F5).

## 3. Packets — status and dependency order

| Task | Packet | Status | Dependency |
|---|---|---|---|
| F4-1 classification + direct numeric params | `QWEN_PROMPTS_F4_1_direct_numeric.md` | AUTHORED, gate-locked | F3-7 stage commit green |
| F4-2 register/flag/shuffle direct forms | `QWEN_PROMPTS_F4_2_register_flag.md` | AUTHORED, gate-locked | F4-1 committed green |
| F4-3 named + indirect forms, bounded core entry | `QWEN_PROMPTS_F4_3_named_indirect.md` | AUTHORED, gate-locked | F4-2 committed green |
| F4-4 error-table + parity acceptance sweep | `QWEN_PROMPTS_F4_4_acceptance.md` | AUTHORED, gate-locked | F4-3 committed green |

Per-packet `/tmp/forth-f4-N-*` paths; one packet per session; strict
order; successor gates re-verified after every stage commit.  RULE-1
flash deltas recorded at F4-1 (parser core) and F4-3 (marker machinery).
Arena: quote the two-region line every commit.  Stage close (architect,
docs-only): DESIGN §4.4 phasing note flipped, §10.4 marked landed,
ledger closeout.
