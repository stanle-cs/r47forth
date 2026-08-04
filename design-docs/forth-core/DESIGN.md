# forth-core — DESIGN.md (authoritative)

Stage 1 design for embedding a Forth word engine into C47 firmware **without
modifying upstream `.c`/`.h` files**. All integration is delivered as a
`CUSTOM_PKG` package (see `design-docs/package-manager/README.md`): new sources under
`packages/forth-core/`, plus whole-file *overrides* of a small set of upstream
`.c` files that add the documented hook lines and nothing else.

This document is written to be implemented by a local model with **zero
unstated decisions**. Every struct layout, constant, and stateful routine is
given exactly. Line numbers reference the upstream tree at the commit this was
authored against (`src/c47/...`); the implementer must re-confirm each hook line
still matches the quoted code before editing (upstream is generated in places).

**Core design principle.** Forth is an **extension of RPN**, not a separate
system layered beside it. Where a choice exists, follow the C47 convention.
A bare name that means a builtin in RPN means that builtin in Forth; a
parameter binds to its opcode the way C47 binds it; a Forth error halts the
way an RPN error halts. Divergence requires a reason stated at the point of
divergence.

**Target platform (RULED 2026-07-15).** The package targets the **R47 on
DM42n hardware (DMCP5)**. DM42 compatibility is best-effort, never
design-binding. RAM discipline stays binding — the dictionary lives in the
shared arena (§5) and every dictionary-touching change reports the arena
high-water mark. Flash is **not** a design veto: where a flash increase
simplifies the implementation, take it and record the measured delta on
`make dmcp5r47` in the stage commit (§5.4).

**How to read this document.** It states what is true **now**, in one voice,
with one sanctioned exception: where the committed code and an accepted
future decision diverge, the current behavior is marked *implemented
interim* and the target carries a pointer into the decision records
(§10-§11) — both states are normative, each for its own phase, and
nothing else about the future is interleaved here. No such divergence is
open today; the last interim markers closed with the 2026-08-03
reconciliation pass. Every normative claim
carries a `[VERIFIED: file:line]` citation against the tree, or an explicit
`[GAP]` / `[OPEN]` marker. There are no amendment tags and no narrative
history in the specification: the amendment trail — what was decided when,
what it superseded, and why — lives in `DESIGN-HISTORY.md`, which is
**non-normative**. Where this document speaks, nothing else does; if
`DESIGN-HISTORY.md` disagrees with it, that file has a bug.

---

## 0. How Forth fits into the existing machine (read first)

C47 already *is* a bytecode calculator. Understanding its four existing name
spaces is required before adding a fifth (Forth).

### 0.1 The item table — the built-in "dictionary"
Every built-in command is an **item**, an entry in a flat static table:

```
src/c47/typeDefinitions.h:603   typedef struct { ... } item_t;
src/c47/items.c:1758            TO_QSPI const item_t indexOfItems[] = { ... };
```

`item_t` (typeDefinitions.h:603-615), exact layout:

| field              | type                | bytes | meaning                                      |
|--------------------|---------------------|-------|----------------------------------------------|
| `func`             | `void (*)(uint16_t)` | 4/8  | C handler, called as `func(param)`           |
| `param`            | `uint16_t`          | 2     | default 1st arg / TAM mode tag               |
| `itemCatalogName`  | `char[16]`          | 16    | name in catalogs & program listings          |
| `itemSoftmenuName` | `char[16]`          | 16    | name on soft-keys                            |
| `tamMinMax`        | `uint16_t`          | 2     | TAM arg min (2 bits) / max (14 bits)         |
| `status`           | `uint16_t`          | 2     | packed status word (see §0.2)                |

Item IDs run `0 .. LAST_ITEM` (`items.h:2992  #define LAST_ITEM 2870`;
re-verified 2026-07-15 after the b8f79e486 migration — upstream itself grew
the table for its new CONV items; the don't-grow constraint below binds this
package, not upstream).
IDs `1..127` encode as **one** program byte; IDs `128..32767` encode as **two**
bytes (see §2). `indexOfItems[]` is defined in `items.c`, which the package
system can override — so we *may* fill spare slots, but we may **not** grow the
array past `LAST_ITEM` (that would desync generated code that assumes the bound).

Two genuinely free slots exist at the tail (`items.c:4690-4691`, `CAT_FREE`):

```
/* 2842 */ { itemToBeCoded, NOPARAM, "2842", "2842", ... CAT_FREE ... PTP_DISABLED ... },
/* 2843 */ { itemToBeCoded, NOPARAM, "2843", "2843", ... CAT_FREE ... PTP_DISABLED ... },
```

**We claim these two.** They become the two executable C47 items Forth needs
(plus, since the §8 PEM-entry stage, one **menu** id claimed from a mid-table
`CAT_FREE` slot — see the third row; slot 213 now holds the `MNU_FORTH` row
in the package override and 214-219 remain free [VERIFIED:
packages/forth-core/items.c:2020-2026, post-b8f79e486 positions]):

| new item id            | value | role                                                        |
|------------------------|-------|-------------------------------------------------------------|
| `ITM_FORTH`            | 2842  | outer interpreter entry (run/compile a Forth source line); in PEM: the entry-mode **toggle** and the tag of **source-text program steps** (§8) |
| `ITM_FCALL`            | 2843  | inner-call bridge: **runtime** param (from program bytes / TAM entry) = dictionary index of a colon def. NOTE: the table row's `param` *field* is the TAM mode tag `TM_VALUE`, NOT a dictionary index — see the full rows in §0.2. |
| `MNU_FORTH`            | 213   | dynamic soft-menu id for the in-program `: NAME` picker (§8.6). `CAT_MENU`, `itemToBeCoded`, names `"FWRD"`/`"FWRD"` — the same row shape as `MNU_PROG` (items.c:3196, upstream) [VERIFIED: src/c47/items.c:3196]. |

**Slot assignment (DECIDED, verified 2026-07-06):** the two items claim the
`CAT_FREE`/`itemToBeCoded` tail spares — 2842 at items.c:4690, 2843 at
items.c:4691, confirmed free in the current tree. **Naming warning:** the
bridge item was originally drafted as `ITM_FWORD`, but that identifier is
**already taken by upstream**: `items.h:2056` defines `ITM_FWORD 2003`, the
swap-endian-word item ("W.SWP", `fnSwapEndian(16)`, items.c:3809), and
upstream `softmenus.c:866` references it in the bit-ops soft menu. That define
is upstream property — it must NOT be removed, redefined, or reused. Any Forth
code that used the name `ITM_FWORD` would silently target item 2003 and
byte-swap X. The Forth bridge item is therefore named `ITM_FCALL` (= 2843)
throughout this document; earlier drafts and notes saying `ITM_FWORD` mean
`ITM_FCALL`.

**H1 scope (DECIDED):** H1/H1b land NOW, in the same commit series as two
mandatory companions: the `forthInner` re-entrancy guard (§3.2) and the
`FTOK_C47` PGM_RUNNING execution-context fix (§2.2 resolved issue 2). H1 is
what makes the re-entry path reachable, so the guard may not lag it.

### 0.2 The `status` word (defines.h:1056-1113)
Packed bitfields consulted throughout dispatch. The two Forth items must set
these correctly:

- `SLS_STATUS` 0x0003 — stack-lift class. Use `SLS_ENABLED` on **both** items:
  the dispatcher epilogue (items.c:594-598) sets `FLAG_ASLIFT` after every
  successfully executed `SLS_ENABLED` item, which is the machine-wide
  convention Forth must match (§3.2 "ASLIFT on exit").
- `US_STATUS` 0x000C — undo class. Use `US_ENABLED`.
- `CAT_STATUS` 0x00F0 — which catalog. Use `CAT_FNCT`.
- `EIM_STATUS` 0x0100 — enabled in equation editor (defines.h:1078-1080). Use
  `EIM_DISABLED` on **both** items, stated explicitly like every surrounding
  row (it is the zero value, but the spec must not leave it implied).
- `PTP_STATUS` 0x1E00 — **program parameter type** (drives `executeOneStep()`, §3).
  - `ITM_FORTH`  → `PTP_REM` (**supersedes the original `PTP_NONE`**): the
    step carries an inline `STRING_LABEL_VARIABLE` string payload — the Forth
    source line (or a zero-length payload = the §8.4 toggle marker). `PTP_REM`
    is chosen because every upstream consumer already handles it generically:
    step length via `countLiteralBytes` (`step + *step + 1` for
    `STRING_LABEL_VARIABLE`) [VERIFIED: src/c47/programming/nextStep.c:297-300,
    236-238], listing render via `decodeRem` (`NAME 'payload'`) [VERIFIED:
    src/c47/programming/decode.c:905-908, 828-843], variable-scan skip
    [VERIFIED: src/c47/programming/clcvar.c:273-277], and the
    `insertStepInProgram` PTP switch treats it as "nothing to do" (entry is
    routed earlier, §8.4) [VERIFIED: src/c47/programming/manage.c:1705-1708].
    Run dispatch happens in the `executeOneStep()` `PTP_REM` arm, which the
    package already owns [VERIFIED:
    packages/forth-core/programming/lblGtoXeq.c:838-863]. **Breaking encode
    change:** a program step recorded under the old `PTP_NONE` two-byte
    encoding is unreadable after this change (the decoder would consume the
    following step's first byte as the string-type byte). No migration is
    provided; the representation predates any release.
  - `ITM_FCALL`  → `PTP_NUMBER_16` (a 16-bit inline param = dictionary index).
- `HG_STATUS` 0x6000 — hourglass. Use `HG_ENABLED`.
- `RESULT_IN_X` 0x8000 — set on `ITM_FCALL` (Forth words generally return in X).

**Complete H1 rows (DECIDED, verified against upstream 2026-07-06).** Upstream
convention for `PTP_NUMBER_16` items is: the `param` *field* holds the TAM mode
tag `TM_VALUE` (defines.h:1677, = 10001), and `tamMinMax` packs
`(min << TAM_MAX_BITS) | max` with `TAM_MAX_BITS` = 14 (defines.h:1046). The
runtime argument reaches `func` from the program's inline bytes or interactive
TAM entry, never from the `param` field. Verified against existing
`PTP_NUMBER_16` rows: `BestF` (items.c:3107 — `fnCurveFitting, TM_VALUE, ...
(0 << TAM_MAX_BITS) | 511, CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED
| PTP_NUMBER_16 | HG_ENABLED | RESULT_IN_X`) and `>RNG<` (items.c:3740 —
`fnRange, TM_VALUE, ... (0 << TAM_MAX_BITS) | 6145, ... EIM_DISABLED |
PTP_NUMBER_16 | ... | RESULT_IN_X`). The `PTP_NONE` shape follows `SIN`
(items.c:1844 — `fnSin, NOPARAM, ... (0 << TAM_MAX_BITS) | 0`). The two rows
to write at items.c:4690-4691 are therefore exactly:

```c
/* 2842 */ { fnForthOuter, NOPARAM,  "FORTH", "FORTH",
             (0 << TAM_MAX_BITS) |     0,
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_REM       | HG_ENABLED },
/* 2843 */ { fnForthCall,  TM_VALUE, "FCALL", "FCALL",
             (0 << TAM_MAX_BITS) | 16383,
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_NUMBER_16 | HG_ENABLED | RESULT_IN_X },
```

(the `ITM_FORTH` row above shows `PTP_REM`, superseding the `PTP_NONE`
this section originally specified. `PTP_REM` landed via PEM C2
[VERIFIED: packages/forth-core/items.c:4722 — `PTP_REM` confirmed]. The
`PTP_REM` shape follows `REM` itself: `fnNop, NOPARAM, ... CAT_FNCT |
SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_REM | HG_ENABLED` [VERIFIED:
packages/forth-core/items.c:3391]. `func` stays `fnForthOuter` — interactive
dispatch (catalog/XEQ outside PEM) still reads its source from X, §3.3.2;
the PTP class only governs program encode/step-length/dispatch.)

`ITM_FCALL` tamMinMax note: the max field is 14 bits, so 16383 is the largest
expressible TAM ceiling. The full `FTOK_CALL` index space runs to
`0x7EFF - 0x1000` = 28415, so interactive TAM entry can only address the first
16384 dictionary indices — acceptable (program bytes carry the full 16-bit
runtime param and are unaffected; a stage-1 dictionary is orders of magnitude
smaller). Do NOT try to widen tamMinMax; 14 bits is a machine-wide format.

### 0.3 The label list — the runtime user "dictionary"
User program entry points are **global labels**, resolved by name at runtime:

```
src/c47/typeDefinitions.h:653   typedef struct { ... } labelList_t;
src/c47/c47.c:119               labelList_t *labelList = NULL;      // in ram[]
src/c47/c47.c:278               uint16_t     numberOfLabels;
```

`findNamedLabel()` (manage.c:1863) linear-scans `labelList[]` and returns
`lbl + FIRST_LABEL`. **This is the seam Forth's outer interpreter reuses for
name lookup** (§4). Label IDs occupy `FIRST_LABEL(2044) .. LAST_LABEL(6999)`
(defines.h:1337-1338) — a namespace *disjoint* from item IDs even though the
integer ranges overlap, because labels only ever appear as the `param` of
`ITM_GTO`/`ITM_XEQ`, never as an opcode.

**Two label kinds since upstream b8f79e486 (named LOCAL labels).** Global
label steps carry a `STRING_LABEL_VARIABLE` (253) payload; named local labels
carry `LOCAL_LABEL_VARIABLE` (249) and live in the same `labelList[]`
(locals: `step < 0`, name at `labelPointer`) [VERIFIED:
src/c47/programming/manage.c:159-171 scan arms]. The resolver took a selector:
`findNamedLabel(name, labelType)` with `namedLabels_t` = `GLOBAL_LABELS`
(= 253), `LOCAL_LABELS` (= 249), `ALL_LABELS` (0) — **the selector values are
the payload kind bytes** [VERIFIED: src/c47/typeDefinitions.h:796-800]. Local
resolution is **position-sensitive**: searched only within
`currentProgramNumber`, matching the first occurrence *after*
`currentLocalStepNumber`, else the first in the program [VERIFIED:
src/c47/programming/manage.c:1869-1908]. Entry is the TAM `:` syntax
(`tam.colon`); listings render `XEQ :name:` vs `XEQ 'name'`. **Every current
Forth call site binds `GLOBAL_LABELS`** (§4.1 step 5, §4.2, the tam.c hook);
locals enter Forth only through the explicit stage-F3 `XEQ :NAME:` source
form (§10), mirroring upstream where locals always require the colon
spelling. A local request is kind-faithful: it resolves local labels or
fails — it never falls through to Forth vocabulary, items, or globals.

### 0.4 The three dispatch entry points
1. **Keyboard** → `executeFunction()` (keyboard.c:928) → `runFunction(item)`.
2. **Program**  → `runProgram()` (lblGtoXeq.c:850) → `executeOneStep()` (lblGtoXeq.c:722).
3. **Both funnel to** `runFunction()` (items.c:628) → `reallyRunFunction()`
   (items.c:237) → the actual call `indexOfItems[func].func(param)` (items.c:399).

Forth needs to be reachable from all three, and to run its own *inner*
interpreter (threaded code) that the C47 step VM does not provide.

---

## 1. Dictionary entry struct

Forth words live in a **single contiguous dictionary region** carved from the
C47 RAM arena (§5). The region holds a downward-compatible bump-allocated
sequence of headers, each a colon (`:`) definition. Primitives are **not** in
this region — they live in flash as a static C table (§1.3).

### 1.1 In-RAM header layout (`forthHeader_t`)
All fields little-endian. The struct is **block-aligned** (4-byte, `BPB=2`,
defines.h:2213). Links are **region-relative 16-bit offsets** (see §5.3 for why
relative, not absolute `C47MEMPTR`).

```c
// packages/forth-core/forth_dict.h
#define FORTH_NULL      0xFFFFu     // end-of-chain sentinel (region-relative)
#define FORTH_PRIM_NONE ((uint16_t)0xFFFFu)  // forthFindPrim miss sentinel (§3.3)
#define FORTH_NAME_MAX  31

#define FF_IMMEDIATE    0x01        // execute even in compile state
#define FF_SMUDGE       0x02        // hidden: definition in progress / incomplete
#define FF_RESERVED     0xFC        // must be 0

typedef struct {                    // stored in ram[], NEVER dereferenced as-is
  uint16_t link;                    // region-relative offset (bytes) of previous header, or FORTH_NULL
  uint8_t  flags;                   // FF_* bits
  uint8_t  nameLen;                 // 1..31, byte length of name (C47 glyph bytes)
  // uint8_t  name[nameLen];        // follows immediately; C47 string encoding; NOT NUL-terminated
  // uint8_t  pad[];                // zero bytes up to next 4-byte boundary
  // ftoken_t body[];               // threaded code (§2), terminated by FTOK_EXIT (0x0000)
} forthHeader_t;                    // fixed prefix = 4 bytes
```

Layout of one entry, byte by byte:

```
offset 0  : link      (2 bytes, LE, region-relative)
offset 2  : flags     (1 byte)
offset 3  : nameLen   (1 byte)
offset 4  : name      (nameLen bytes)
offset 4+nameLen : pad (0..3 zero bytes) -> round total-so-far up to *4
then       : body     (ftoken_t[], 2 bytes each, LE) ending in FTOK_EXIT
```

`bodyStart(entry) = 4 + nameLen` rounded up to a multiple of 4.

### 1.2 Dictionary control block
A small fixed struct (in package BSS, *not* in the arena — it holds the arena
handle) tracks the whole region:

```c
// packages/forth-core/forth_dict.h
typedef struct {
  uint8_t *base;        // PCMEMPTR of region start (from allocC47Blocks); may move on grow
  uint16_t sizeBlocks;  // current region size in 4-byte blocks
  uint16_t here;        // region-relative byte offset of next free byte (bump ptr)
  uint16_t latest;      // region-relative byte offset of newest header, or FORTH_NULL
  uint16_t count;       // number of defined words (== next dictionary index)
} forthDict_t;

extern forthDict_t fdict;   // defined in forth_dict.c
```

`base` is refreshed after every (re)alloc. Because links are region-relative,
a region move only rewrites `fdict.base`; header contents are untouched.

### 1.3 Primitives (flash, static)
```c
// packages/forth-core/forth_prims.h
typedef void (*forthPrim_t)(void);          // operates on the C47 stack via helpers
typedef struct {
  const char  *name;                        // ASCII/C47 name, NUL-terminated
  uint8_t      flags;                        // FF_IMMEDIATE etc.
  forthPrim_t  fn;
} forthPrimDef_t;

extern const forthPrimDef_t forthPrims[];   // forth_prims.c, index-stable, append-only
extern const uint16_t       forthPrimCount;
```

**What the primitive table is for (read before adding one).** Every primitive is
a thin wrapper over an existing C47 handler — `pDup`→`fnDupN(1)`,
`pDrop`→`fnDrop`, `pSwap`→`fnSwapXY`, `pOver`→`fnRecall(REGISTER_Y)`,
`pPlus`→`fnAdd`, `pMul`→`fnMultiply` [VERIFIED:
packages/forth-core/forth_prims.c:9-17]. Since §4.1 resolves C47 items by name,
every one of them is reachable through the item table as well. The table is
therefore **an alias layer, not a vocabulary**: it gives Forth-standard *names*
to operations the item table names differently (`SWAP` for `x<>y`, `DUP`,
`DROP`, `OVER`), and it gives a faster 2-byte token than `FTOK_C47`'s 4.

> **A primitive exists only to give a Forth-standard name to an operation the
> item table names differently, or to shadow an item for speed. Never to add
> capability.** If a capability is reachable as a `CAT_FNCT` item, it is not a
> primitive.

This is a guardrail, not trivia. Without it the natural instinct is to keep
appending prims (`SIN`, `SQRT`, `MOD`) until Forth has a second, divergent
vocabulary — rebuilding exactly the split §4.1 exists to remove, one
well-intentioned commit at a time.

**Index stability rule (APPEND-ONLY):** never reorder, insert mid-table, or
delete a primitive; the numeric index is the on-disk/on-arena token (§2).
New entries are appended at the end only. Retire a primitive by pointing it at
a `fnNotAvailable`-style stub, never by removal. Inserting mid-table shifts
every subsequent token and corrupts all compiled bodies that reference those
indices.

**Name encoding (Stage-1 clarification, folded in):** primitive names use C47
glyph encoding (same as stored labels), so a single `compareString` path serves
both. ASCII names are byte-identical single-byte glyphs, verified. Lookup is
case-sensitive via `compareString(CMP_BINARY)` — see §4.1.

**Alpha-mode keyboard aliases (glyph entries):** alpha-mode keyboard entry emits
C47 glyph codes for multiply and divide, not ASCII. The tokenizer's glyph-wise
advance delivers the full two-byte token intact; the prim table provides alias
entries so name lookup succeeds:

| alias entry | `fonts.h` macro | bytes          | delegates to |
|-------------|-----------------|----------------|--------------|
| `PRIM_CROSS`| `STD_CROSS`     | `\x80\xd7` (×) | `pMul` (`*`) |
| `PRIM_DOT`  | `STD_DOT`       | `\x80\xb7` (·) | `pMul` (`*`) |
| `PRIM_DIVGL`| `STD_DIVIDE`    | `\x80\xf7` (÷) | `pDiv` (`/`) |

`STD_PLUS`/`STD_MINUS` are plain ASCII (`\x2b`/`\x2d`) and need no alias.
`PRODUCT_SIGN` (defines.h:2229) is flag-dependent (`FLAG_MULTx` chooses × or ·),
so both `STD_CROSS` and `STD_DOT` must resolve to the multiply handler.

---

## 2. Token encoding table

Two encodings coexist and must not be confused.

### 2.1 C47 program bytecode (unchanged, upstream)
How a C47 program step names an opcode (`executeOneStep()`, lblGtoXeq.c:725-730;
`runProgram()`, lblGtoXeq.c:877-879):

```
byte0 < 0x80          : 1-byte opcode, op = byte0                 (items 1..127)
byte0 >= 0x80         : 2-byte opcode, op = ((byte0 & 0x7F)<<8) | byte1   (items 128..32767)
```

Therefore our new items encode in-program as:

| item      | id   | program bytes            |
|-----------|------|--------------------------|
| `ITM_FORTH` | 2842 | `0x8B 0x1A 0xFD len bytes[len]` (; `0xFD` = `STRING_LABEL_VARIABLE` 253) |
| `ITM_FCALL` | 2843 | `0x8B 0x1B` + param     |

`0x8B = 0x80 | (2842>>8)`, `0x1A = 2842 & 0xFF`; `0x1B = 2843 & 0xFF`.
`ITM_FORTH` is `PTP_REM` (§0.2): the opcode is followed by the string
type byte `STRING_LABEL_VARIABLE` (253, defines.h:1363), a 1-byte length
`len` (0..255), and `len` payload bytes — the same shape `pemAlpha` commits
for `REM`/`42STR` steps [VERIFIED: src/c47/programming/manage.c:953-959].
`len == 0` is the §8.4 toggle marker; `len > 0` is a Forth source line
(§8.1). Step length falls out of upstream `countLiteralBytes` unchanged
[VERIFIED: src/c47/programming/nextStep.c:236-238].
`ITM_FCALL` is `PTP_NUMBER_16`, so **two** parameter bytes follow the opcode,
holding the dictionary index (LE), consumed by `_executeOp(..., PARAM_NUMBER_16)`
(the default arm at lblGtoXeq.c:835).

### 2.2 Forth threaded code (new) — the `ftoken_t`
Each colon-definition body is a stream of 16-bit **tokens** (`ftoken_t`,
little-endian). This is the encoding the Forth *inner* interpreter walks; it is
independent of the C47 bytecode above.

```c
typedef uint16_t ftoken_t;
```

| token value (hex)   | mnemonic    | inline data (bytes, LE)          | meaning                                                    |
|---------------------|-------------|----------------------------------|------------------------------------------------------------|
| `0x0000`            | `FTOK_EXIT` | —                                | return from this word (end of body)                        |
| `0x0001 .. 0x0FFF`  | `FTOK_PRIM` | —                                | call `forthPrims[token - 1].fn()` (primitive index = token - 1) |
| `0x1000 .. 0x7EFF`  | `FTOK_CALL` | —                                | call colon def with dictionary index `(token - 0x1000)`    |
| `0x7F00`            | `FTOK_LIT`  | 16 (a real34_t / decQuad = `REAL34_SIZE_IN_BYTES`, realType.h:13) | push inline real34 literal onto the C47 stack              |
| `0x7F01`            | `FTOK_ILIT` | 4 (int32 LE)                     | push inline integer literal **as a long integer** (`dtLongInteger` — the type keyboard entry produces; §3.3 number-type conformance) |
| `0x7F02`            | `FTOK_BR`   | 2 (int16 signed cell delta)      | unconditional branch                                       |
| `0x7F03`            | `FTOK_0BR`  | 2 (int16 signed cell delta)      | pop X; branch if zero/false                                |
| `0x7F04`            | `FTOK_C47`  | 2 (uint16 item id) + params, padded to 2-byte cells | escape: run a native C47 item via `reallyRunFunction()`      |
| `0x7F05`            | `FTOK_XEQN` | 1 (kind: 253 global / 249 local) + 1 (len) + len name bytes, zero-padded to a whole cell | escape: run a C47 label **by name**, resolved at run time (stage F3, §10) |
| `0x7F06 .. 0x7FFF`  | reserved    | —                                | must not be emitted (reserve for control words)            |
| `0x8000 .. 0xFFFF`  | reserved    | —                                | unused; keeps top bit free for a future long-token scheme  |

Notes:
- **Branch deltas are in *tokens* (2-byte cells), signed, relative to the cell
  *after* the delta field.** `THEN`/`ELSE`/loop back-patching writes here.
- `FTOK_C47` is the bridge that lets Forth call the entire C47 command set (e.g.
  `SIN`, `STO 05`). Its inline `params` follow the same per-`PTP` convention the
  C47 VM uses. The decoder (forth_inner.c:304-360) accepts `PTP_NONE`,
  `PTP_NUMBER_8` (cell-padded) and `PTP_NUMBER_16`; `PTP_REGISTER` joins them
  with §4.4. Any PTP outside that set — including `PTP_LABEL` — raises
  `ERROR_OPERATION_UNDEFINED`; `PTP_LABEL` is not an omission but a rule:
  labels are never baked (§3.3.6), they go through `FTOK_XEQN`.
  **Inline params are padded to a whole 2-byte cell.** A `PTP_NUMBER_8` param
  occupies 2 bytes (value byte + one zero pad byte), so `FTOK_C47`/`PTP_NUMBER_8`
  is always 6 bytes total (token 2 + itemId 2 + param 1 + pad 1) and every token
  and inline datum stays cell-aligned. Emitter writes the pad byte as 0; decoder
  advances `ip` by 2 after reading the param byte.
- `FTOK_XEQN` (landed, F3-6 `2db8af231`) is the bridge that
  lets Forth call a C47 keystroke program by **name**, and it exists because
  `labelList[]` indices renumber on every program edit — baking one calls the
  wrong program silently (§3.3.6). Inline data is `[kind][len][name bytes]`
  zero-padded to a whole cell — **byte-compatible with the RPN step payload
  after the opcode**, where `kind` is 253 (`STRING_LABEL_VARIABLE`, global)
  or 249 (`LOCAL_LABEL_VARIABLE`, local; §0.3) and doubles as the
  `findNamedLabel` selector passed verbatim at run time. Arithmetic:
  `inline = 2 + len`, `padded = (inline + 1) & ~1`, `total = 2 + padded`;
  `len` is 1..`FORTH_NAME_MAX` (31), worst case 2 + 34 = 36 bytes; any other
  kind byte on decode → `ERROR_INVALID_CORRUPTED_DATA`. Resolution is fresh
  on every execution; dispatch follows the B4 matrix (§10): a resolved label
  (either kind) takes the native XEQ path (§3.3.6 — direct `fnExecute`, no
  wrap), a global-kind miss falls back to a callable Forth target
  (prim → same-scope colon → item), a **local-kind miss just fails** —
  kind-faithful, no fallback. The per-execution name scan is exactly what a
  C47 `XEQ 'NAME'` program step already costs; this is not a new tax.
- The dictionary-index space (`FTOK_CALL`) and the primitive-index space
  (`FTOK_PRIM`) are disjoint by construction: primitives ≤ 0x0FFF, colon defs
  offset by 0x1000. `forthPrimCount` MUST stay ≤ 0x0FFF (4095); enforce at
  **compile time** with `_Static_assert(sizeof(forthPrims)/sizeof(forthPrims[0])
  <= 0x0FFF, ...)` in `forth_prims.c` (required change — no assert exists in
  the tree today; see §7 invariants).
- Emit: FTOK_PRIM token = index + 1 (`FTOK_PRIM_BASE` = 1); decode subtracts 1.
  Index 0 must never emit() as `0x0000` (= `FTOK_EXIT`).
- **Branch token stack effects:**
  - `FTOK_0BR` CONSUMES its operand: pops X, branches if zero/false. A value
    needed after the test must be `DUP`'d before `0BR` (this is what `IF`
    compiles around).
  - `FTOK_BR` has no stack effect.
  - Verified by the Stage-1-B backward-loop test:
    `DUP 0BR(+6) ILIT(-1) + BR(-9) EXIT`, counter 5 → 0 over 5 iterations,
    terminates correctly.
- **Runaway guard (backstop only):** `forthInner` bounds total dispatches with
  a hard counter; a non-terminating loop raises `ERROR_RAM_FULL` rather than
  hanging the calculator. This is the *backstop*: the primary interrupt for a
  long/looping word is the per-dispatch R/S key poll (§3.2 "Cooperative break
  & key poll") — a bare `programRunStop` check cannot fire on hardware because
  nothing mutates `programRunStop` mid-word.

**Cell alignment is an invariant, not a preference.** Every token and every
inline datum is 2-byte aligned. The `PTP_NUMBER_8` param is the only odd-width
datum in the encoding, and it is padded rather than packed. Re-aligning `ip`
after dispatch (`ip = (ip+1) & ~1`) is **rejected**: it makes `FTOK_C47`
variable-width with hidden state, which emit(), decode, decompile and save would
each have to special-case. One wasted byte per C47 escape buys a uniform
encoding. Keep the token fetch as `memcpy` — the region is byte-addressed and
`ftoken_t` reads must not assume alignment of `fdict.base`.

**FTOK_C47 runs under program semantics — normative.** The dispatch is wrapped:

```c
uint8_t saved = programRunStop;
programRunStop = PGM_RUNNING;
reallyRunFunction(itemId, param);
if (programRunStop == PGM_RUNNING) programRunStop = saved;   // an async R/S stop
                                                             // must not be clobbered
```

This is not defensive dressing; without it the interpreter deadlocks the GTK
build. With `programRunStop != PGM_RUNNING`, `reallyRunFunction()` takes its
NORMAL-MODE branch (items.c:317) and the unconditional `refreshStatusBar()`
(items.c:389), which funnels through `force_SBrefresh` (statusBar.c:607) →
`lcd_refresh` (c47-gtk/hal/lcd.c:93) → `refresh_gui` (lcd.c:212):
`while(gtk_events_pending()) gtk_main_iteration();` — a GTK event pump **inside
the interpreter loop**. Its only early exit is `ui_is_active`, true only ~100 ms
after a physical UI event, so a headed self-test run livelocks while the queue
refills. The PGM_RUNNING branch (items.c:347-355) skips the
hourglass/interactive chain and performs only the rate-limited
`force_refresh(timed)` — the same exposure a normal C47 program has.

The interactive branch is additionally re-entrancy-unsafe: `gtk_main_iteration()`
can dispatch a queued keypress that runs a C47 function while a Forth word is
mid-execution. That is also why program semantics make the
`FTOK_C47` → item dispatch → Forth-item re-entry path *more* reachable, and why
the §3.2 nesting discipline is a mandatory companion rather than a nicety.

The wrap is normative for `FTOK_C47` and `FTOK_XEQN`'s **item** arm only. It
is **wrong for a label dispatch** — see §3.3.6, where `ITM_XEQ` needs a direct
`fnExecute` because it is the run-loop driver, not an ordinary item — and
**wrong for the colon arm** too: a nested colon call runs in the current Forth
execution context and must not synthesize `PGM_RUNNING` (B4 accepted ruling,
§10 — forcing RUNNING would give a label inside that colon word continuation
semantics with no enclosing `runProgram()` loop to resume them).

---

## 3. Program executor hook (the inner interpreter)

The C47 step VM (`executeOneStep()`, lblGtoXeq.c:722) dispatches one item and
returns "steps to advance". It has no concept of threaded code. We add the
inner interpreter as a **new module** invoked when the two Forth items execute;
we do **not** change the outer `runProgram()` loop shape.

### 3.1 Dispatch path for `ITM_FCALL` / `ITM_FORTH`
Both items have real `func` pointers in the overridden `indexOfItems[]`, so
interactive dispatch flows through `executeFunction()` → `runFunction()` →
`reallyRunFunction()` → `indexOfItems[func].func(param)` (items.c:399).

- `indexOfItems[ITM_FCALL].func = fnForthCall;` — `param` = dictionary index.
  In-program `ITM_FCALL` steps flow through the `executeOneStep()` default arm
  unchanged (PTP_NUMBER_16).
- `indexOfItems[ITM_FORTH].func = fnForthOuter;` — reads source from the X
  register (§3.3.2). **supersession:** in-program `ITM_FORTH` steps do
  NOT reach `fnForthOuter`; they are `PTP_REM` steps dispatched by the
  `ITM_FORTH` case in `executeOneStep()`'s `PTP_REM` arm to
  `forthProgramStep` (§8.2), which reads source from the step payload. The
  original claim "no new case in `executeOneStep()` is required" holds only
  for `ITM_FCALL`.

### 3.2 The inner interpreter (`forth_inner.c`, pseudocode)
Threaded-code walker. Uses one explicit return stack in package BSS (NOT the C
call stack — depth must be bounded and inspectable).

`forthInner` is **re-entrant to `FORTH_NEST_MAX` (4)**, implemented with a
depth counter plus an `rsp` watermark rather than a single-level guard.
`rstack[64]` is one shared static partitioned by watermarks — zero BSS growth,
and the `FTOK_CALL` overflow check naturally bounds the sum of all levels.

The unwind discipline is the load-bearing part: **every** exit path (normal
`FTOK_EXIT`, every error return, the runaway backstop, key-poll suspension,
cooperative break) must unwind via the single `INNER_LEAVE()` macro
(`rsp = rspBase; forthDepth--`). A leaked entry is not loud — it is silently
absorbed as the next invocation's floor, permanently shrinking capacity.
`forthTestGetRsp() == 0` at rest pins the unwind in every path and is the test
that catches a missed one.

```
#define FORTH_RSTACK_DEPTH 64            // tune against arena high-water (§5.4)
#define FORTH_NEST_MAX      4            // max nested forthInner invocations
static uint16_t rstack[FORTH_RSTACK_DEPTH];   // region-relative token offsets
static uint8_t  rsp;
static uint8_t  forthDepth = 0;          // nested forthInner invocations

// INNER_LEAVE() == { rsp = rspBase; forthDepth--; return; }  -- EVERY exit path

void forthInner(uint16_t entryIndex, bool fromProgram):   // matches forth_dict.h:56
    if forthDepth >= FORTH_NEST_MAX:     // refuse at the cap, don't recurse.
        displayCalcErrorMessage(ERROR_OPERATION_UNDEFINED,...)  // Error code is
        return                           // normative (forth_inner.c:105) and
                                         // deliberately DISTINCT from the rstack-
                                         // depth guard's ERROR_RAM_FULL below
    forthDepth++
    rspBase = rsp                        // watermark: NOT rsp = 0 — zeroing would
                                         // destroy an outer word's return stack
    ip = bodyOffsetOfIndex(entryIndex)   // region-relative byte offset of body[0]
    if ip == FORTH_NULL:                 // bad/stale entry index (e.g. saved program
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,...)  // after CLEAR FORTH)
        INNER_LEAVE()                    // NEVER a silent no-op — same error as the
                                         // mid-word FTOK_CALL bad-index arm below
    loop:
        if pollProgramInterrupt():       // DMCP R/S/EXIT key poll, EVERY dispatch —
            INNER_LEAVE()                // poll sets programRunStop = PGM_WAITING and
                                         // requests stop; fires for interactive AND
                                         // fromProgram entry ("Cooperative break &
                                         // key poll" below)
        if fromProgram && programRunStop != PGM_RUNNING:   // async stop from inside a
            INNER_LEAVE()                // dispatched item (e.g. FTOK_C47 ran R/S-
                                         // sensitive code). Gated on fromProgram:
                                         // interactively programRunStop is NOT
                                         // PGM_RUNNING to begin with.
        if ++dispatches >= RUNAWAY_CAP:  // backstop only (§2.2 notes)
            displayCalcErrorMessage(ERROR_RAM_FULL,...); INNER_LEAVE()
        tok = readToken(ip); ip += 2
        if tok == FTOK_EXIT:
            if rsp == rspBase:               // this invocation's floor, not 0
                setSystemFlag(FLAG_ASLIFT)   // C47 convention: result-producing
                INNER_LEAVE()                // items exit with ASLIFT set
            ip = rstack[--rsp]
            continue
        else if tok <= 0x0FFF:                       // FTOK_PRIM
            forthPrims[tok - 1].fn()                 // decode subtracts FTOK_PRIM_BASE
                                                     // = 1 (§2.2) — NEVER index by raw tok
            clearSystemFlag(FLAG_ASLIFT)             // per-dispatch scrub; committed at
                                                     // forth_inner.c:170-171 (mirrors
                                                     // this in interpret state, §3.3.4)
            if lastErrorCode != ERROR_NONE: INNER_LEAVE()   // honor C47 error protocol
                                                     // (INNER_LEAVE, not bare return —
                                                     // §3.2 unwind discipline)
        else if tok <= 0x7EFF:                       // FTOK_CALL
            if rsp == FORTH_RSTACK_DEPTH:
                displayCalcErrorMessage(ERROR_RAM_FULL,...); INNER_LEAVE()   // deep recursion guard
            rstack[rsp++] = ip
            ip = bodyOffsetOfIndex(tok - 0x1000)
        else switch(tok):
            FTOK_LIT:  push real34 from ram at ip; ip += 16   // real34_t = 16 bytes (§2.2 correction; forth_inner.c matches)
            FTOK_ILIT: push int32 at ip AS A LONG INTEGER; ip += 4
                       // dtLongInteger via the closeNim idiom (longIntegerInit /
                       // int -> longInteger / convertLongIntegerToLongIntegerRegister,
                       // bufferize.c:2434-2437) — NOT int32ToReal34. See §3.3
                       // number-type conformance; forthPushInt32 must change.
            FTOK_BR:   delta = i16 at ip; ip += 2; ip += delta*2
            FTOK_0BR:  delta = i16 at ip; ip += 2; if popIsFalse(): ip += delta*2
                       // popIsFalse() is a TYPE-DISPATCHED zero test (long integer,
                       // real34, complex, ...) like upstream's compareRegisters
                       // machinery (mathematics/compare.c:505 fnXEqualsTo) — NOT a
                       // raw real34IsZero on X's data, which misreads any non-real34
                       // X (e.g. a dtLongInteger left by an FTOK_C47 escape).
            FTOK_C47:  itemId = u16 at ip; ip += 2
                       saved = programRunStop; programRunStop = PGM_RUNNING   // program semantics (§2.2 resolved issue 2)
                       reallyRunFunction(itemId, inlineParam)
                       if programRunStop == PGM_RUNNING: programRunStop = saved   // restore unless an async R/S stop changed it
                       advance ip past params (cell-padded: PTP_NONE +0,
                       PTP_NUMBER_8 +2, PTP_NUMBER_16 +2 — see §2.2); any other
                       PTP (incl. PTP_LABEL) raises ERROR_OPERATION_UNDEFINED
                       and returns (committed decoder, forth_inner.c:240-255; )
            default:   displayCalcErrorMessage(ERROR_INVALID_DATA_...); INNER_LEAVE()
```

(Every error exit in the pseudocode above spells `INNER_LEAVE()` — a bare
`return` would leak `forthDepth`/`rsp` and silently shrink capacity, the
exact defect the unwind discipline paragraph forbids. The committed code has
always done this correctly; earlier revisions of this pseudocode did not.)

**Truncation guards (IMPLEMENTED, R1-2).** Every token fetch and every
inline-operand read (LIT 16, ILIT 4, BR/0BR delta 2, C47 itemId 2, C47 param
cell 2) is preceded by `boundedRead(ip, n)` — `(uint32_t)ip + n ≤ fdict.here`
or `ERROR_INVALID_CORRUPTED_DATA` and unwind via `INNER_LEAVE()` [VERIFIED:
packages/forth-core/forth_inner.c:159-168 and every dispatch arm]. A restored
or truncated body can therefore never read past the logical dictionary end.
**Retention ruling (2026-07-15, RULE-1):** these per-dispatch guards stay as
defense-in-depth alongside the landed stage-F1 restore-time validator (§10) —
the flash argument for removing them is void on the R47 target.

**Cooperative break & key poll (DECIDED, verified against upstream 2026-07-06):**
the old spec relied on `if programRunStop != PGM_RUNNING && calledFromProgram:
return` at the loop bottom. That check is **insufficient on hardware**: nothing
inside `forthInner` ever mutates `programRunStop`, because upstream's R/S
detection lives exclusively in `runProgram()`'s step loop and a whole Forth word
executes as ONE step. `forthInner` must therefore poll the keyboard itself,
using the SAME mechanism and cadence upstream uses:

- *Upstream mechanism (cited):* at the bottom of `runProgram()`'s `while(1)`
  loop, **once per executed program step**, DMCP builds only, outermost engine
  only (`!nestedEngine` gate): `programming/lblGtoXeq.c:906-928`. The call is
  `int key = C47PopKeyNoBuffer(DISPLAY_WAIT_FOR_RELEASE) + 1;` (lblGtoXeq.c:908).
  Key 36 = R/S, key 33 = EXIT (lblGtoXeq.c:911): on hit, upstream sets
  `programRunStop = PGM_WAITING`, refreshes the screen, starts the `TO_KB_ACTV`
  timer, waits for key release and pops it (lblGtoXeq.c:912-921). Any other key
  is buffered for later via `setLastKeyCode(key)` (lblGtoXeq.c:924-925).
  Per-step polling is cheap because `C47PopKeyNoBuffer` early-returns −1 unless
  `anyKeyWaiting()` (c47Extensions/addons.c:1103-1108).
- *Forth mapping:* one token dispatch is the Forth analog of one program step,
  so `pollProgramInterrupt()` runs **once per dispatch**, at the top of the
  loop, under `#if defined(DMCP_BUILD)` (returns `false` on non-DMCP builds).
  It performs the identical sequence:
  `C47PopKeyNoBuffer(DISPLAY_WAIT_FOR_RELEASE) + 1`; on key 36 or 33 it sets
  `programRunStop = PGM_WAITING` (mirroring lblGtoXeq.c:912) and returns
  `true`, which exits the word directly — it must NOT rely on a
  `programRunStop != PGM_RUNNING` comparison, because interactively
  `programRunStop` was never `PGM_RUNNING` to begin with; on any other key,
  `setLastKeyCode(key)` exactly as upstream, returning `false`. The break
  fires for BOTH interactive and program entry (upstream's `!nestedEngine`
  gate maps to the §3.2 re-entrancy guard: `forthInner` nests only up to
  `FORTH_NEST_MAX` [VERIFIED: packages/forth-core/forth_inner.c:159-166], and
  the innermost level is the one that polls while a word runs).
  When entered `fromProgram`, `runProgram()`'s own
  `if(programRunStop != PGM_RUNNING) break` (lblGtoXeq.c:929-931) then stops
  the program after the word returns.
- The runaway dispatch cap is retained purely as a backstop (§2.2 notes); the
  key poll is the primary interrupt. The current code's bare check
  (forth_inner.c:65) is the required-change site.
- PC/GTK builds: no key poll (upstream polls only under `DMCP_BUILD`); the
  existing `programRunStop` check plus the runaway cap remain the PC behavior.

`readToken`/`push`/`pop` operate through the existing C47 stack API
(`liftStack()`, `getRegister…`, `setRegister…`) and `TO_PCMEMPTR(fdict base +
offset)`. Reuse C47 error reporting verbatim so undo/trace behave normally.

**ASLIFT on exit (DECIDED, verified against upstream 2026-07-05):**
`forthInner` sets `FLAG_ASLIFT` immediately before its normal (`rsp == 0`)
return. Convention confirmed in upstream: the dispatcher epilogue in
`reallyRunFunction()` (items.c:594-598) sets `FLAG_ASLIFT` after every
successfully executed item whose status carries `SLS_ENABLED` — "Stack lift
enabled after item execution" (defines.h:1051) — and the result-producing
items all carry it: `fnAdd` "+" (items.c:1863), `fnSin` (items.c:1844),
`fnMultiply` (items.c:1866). So after `3 SQ` leaves 9 in X, the next digit
entry must *lift* onto 9, not overwrite it. Two reasons `forthInner` must set
the flag itself rather than rely on the epilogue: (1) primitives call handlers
like `fnAdd` directly, bypassing `reallyRunFunction()` and its epilogue; (2) the
self-test/harness path enters `forthInner` without going through item dispatch
at all. When entered via `ITM_FCALL`, the item's `SLS_ENABLED` (§0.2) makes
the epilogue agree redundantly. **IMPLEMENTED:** `forthInner` sets
`FLAG_ASLIFT` at the normal `rsp == rspBase` exit [VERIFIED:
packages/forth-core/forth_inner.c:231], and `test_stack_aslift` asserts it.

**CORRECTED 2026-07-25 (D1).** This section previously ended "the *internal*
scrub (each push forcing its own lift, clearing after) is correct and
unchanged." That was wrong, and wrong in the direction this very section
argues against. The scrub was reasoned about only for Forth-internal
sequencing, but it also governed the Forth→native boundary: leaving `ASLIFT`
clear after each push and each primitive made the next native item take
`liftStack()`'s else-branch (`src/c47/stack.c:20`) and **overwrite** X instead
of lifting onto it. `1000 RCL 19` left Y=0 where R47 leaves Y=1000. By this
section's own rule — "after `3 SQ` leaves 9 in X, the next digit entry must
*lift* onto 9, not overwrite it" — the scrub was a defect.

The rule is now uniform: **every dispatch that leaves a value in X sets
`FLAG_ASLIFT`**, mirroring `reallyRunFunction()`'s epilogue, because every
prim-equivalent item upstream (`fnAdd`, `fnDrop`, `fnSwapXY`, `fnMultiply`)
carries `SLS_ENABLED`. Pushes leave it set; primitives set it; the definition
marks (`GLOBAL`/`IMMEDIATE`) touch no stack and so leave it alone
(`SLS_UNCHANGED`). Pinned by `test_native_lift_after_forth`.

**Data-stack overflow guard (DECIDED 2026-07-25, D2).** The Forth data stack
*is* the C47 RPN stack — the primitives are one-line delegations (`pDup` →
`fnDupN(1)`, `pPlus` → `fnAdd`) and pushes go through `liftStack()` into
`REGISTER_X`. Depth is therefore 4 or 8 (`FLAG_SSIZE8`), and a push past the
top silently discards the bottom entry. For a user keying values that is
ordinary RPN behaviour; for a recursive word overrunning its own operands it is
silent corruption — `7 FACT` returned `720*6 = 4320` instead of `5040` with
`lastErrorCode` 0 throughout, and `6 2 NCR` returned 1 instead of 15 because
`FACT` ran with two values already beneath it.

`forthDataDepth` (forth_inner.c) counts values Forth has pushed and not yet
consumed since the current line began, driven by a new `stackEffect` column in
`forthPrims[]` (runtime words only; the compile-time words are 0). Growth past
`getStackTop() - REGISTER_X + 1` raises `ERROR_RAM_FULL`, joining the return-
stack depth guard and the runaway cap as a loud resource failure.

Two deliberate properties, both chosen so the guard can never fire on a correct
program:

- **It is only ever an underestimate.** A native item's stack effect is not
  knowable from the dispatcher, so running one *resyncs* the count to 0 rather
  than abandoning it. 0 is never above the true depth, so the guard can fire
  late but never falsely. (Resync also happens to be exact after the common
  `XEQ 'CLSTK'` prefix.)
- **It applies only while Forth is executing.** `forthPushInt32`/
  `forthPushReal34` are public helpers used to seed the RPN stack outside any
  Forth line; counting those accumulated a stale depth that refused a later
  legitimate push (caught by `test_param_series_c_acceptance` during
  implementation).

Pinned by `test_data_stack_overflow_guard`: `6 FACT` still exact, `7 FACT`
raises, and a long-but-shallow `1 2 + 3 + …` chain is untouched.

**Why nesting must be bounded, not merely allowed.** The path
`FTOK_C47` → `reallyRunFunction()` → item dispatch → a Forth item (`ITM_FCALL`,
or the XEQ fallback of §4.2) → nested `forthInner` is reachable, and the
PGM_RUNNING execution context (§2.2) makes it *more* reachable. `rstack`/`rsp`
are static, so an unbounded nest would exhaust them; a nest that zeroed `rsp`
would destroy the outer word's return stack and the outer loop would then EXIT
into garbage. Hence the watermark discipline above rather than either extreme.

`ERROR_OPERATION_UNDEFINED` at the depth cap is **normative** and is
deliberately DISTINCT from the rstack-depth guard's `ERROR_RAM_FULL`: the first
means "you nested too deep", the second means "one word recursed too deep".
Tests assert both codes; do not merge them.

### 3.3 Compilation (`:` … `;`) — `forth_compile.c`

Consolidated with the §3.3-C sub-phase C amendments (compiler pre-build audit,
2026-07-07, against the committed post-H1 foundation: forth_inner.c /
forth_dict.c/.h / forth_prims.c / forth_bridge.c). Every decision below is
DECIDED; the implementer makes none.

**Emit scope:** the compiler emits `FTOK_PRIM`, `FTOK_CALL`, `FTOK_LIT`,
`FTOK_ILIT`, `FTOK_C47`, `FTOK_XEQN`, `FTOK_EXIT`. It does **not** emit()
`FTOK_BR`/`FTOK_0BR` — control-flow words are future work, and the branch
provision "hold offsets, not pointers" in §3.3.7 binds when they land.

`fnForthOuter` acquires the source line into the per-invocation context
(§3.3.2), then `forthOuterInterpret()` tokenizes it glyph-wise (§3.3.3) and, for
each token, resolves via the **lookup order** in §4.1
(prim → colon → number → item → label) and either executes (interpret state) or
appends `ftoken_t`s to the dictionary bump area (compile state). "stop line"
below = abandon the remainder of the source line; the error has already been
displayed.

```
// forthOuterInterpret(source) — 'state' is a LOCAL variable of this
// invocation, initialized INTERPRET; it does not persist across lines
state = INTERPRET
while nextToken(buf):                          // tokenizer §3.3.3
    word = buf
    if word == ":":
        if state == COMPILE:                   // nested ':' — a second
            error(ERROR_INVALID_NAME)          // snapshot would clobber the
            abortDefinition(); stop line       // single static abort record
        if not nextToken(name):                // ':' with no following word
            error(ERROR_INVALID_NAME); stop line  // nothing allocated yet —
                                                         // no abort needed
        if not startDefinition(name): stop line   // §3.3.7; error already shown
        state = COMPILE; continue
    if word == ";":
        if state == INTERPRET:                 //
            error(ERROR_INVALID_NAME); stop line
        if not finishDefinition(): stop line   // finishDefinition() emits FTOK_EXIT itself
        state = INTERPRET; continue
    idx = forthFindPrim(word)                  // §4.1 step 1
    if idx != FORTH_PRIM_NONE:                 // NEVER 'idx >= 0' — idx is
                                               // uint16_t, that test is always true
        if state == COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE):
            emit(idx + FTOK_PRIM_BASE)         // FTOK_PRIM: token = index + 1.
                                               // NEVER emit(idx): index 0 (DUP) would
                                               // emit() 0x0000 = FTOK_EXIT (see §2.2)
        else:                                  // interpret state, or immediate prim
                                               // (immediacy = prims only, )
            forthPrims[idx].fn()
            clearSystemFlag(FLAG_ASLIFT)       // scrub — mirror forth_inner.c:170-171
            if lastErrorCode != ERROR_NONE:    // error gate
                abortDefinition()-if-open; stop line
        continue
    if forthFindColon(word, &widx):            // §4.1 step 2 — bool return; 0-based
                                               // index via out-param (miss leaves
                                               // widx untouched); skips FF_SMUDGE
        if state == COMPILE: emit(0x1000 + widx)   // FTOK_CALL
        else:
            forthInner(widx, programRunStop == PGM_RUNNING)  // fromProgram source
                                               // (; earlier draft omitted the arg)
            if lastErrorCode != ERROR_NONE: stop line        // error gate
        continue
    if classifiesAsNumber(word):               // §4.1 step 3 — the EXACT
                                               // grammar of §3.3.5 is the gate
        <integer or real path per §3.3.5, identical in both states>
        if lastErrorCode != ERROR_NONE:        // pushes can fail on allocation —
            abortDefinition()-if-open; stop line // error gate
        continue
    if forthFindItem(word, &itemId):           // §4.1 step 4 — CAT_FNCT && PTP_NONE.
                                               // Parameterised items: §4.4 consumes
                                               // the next token here, before emit().
        if state == COMPILE:                   // item ids are flash constants — safe
            emit(FTOK_C47); emit(itemId)       // to bake (§3.3.6)
        else:                                  // PGM_RUNNING protocol (§2.2)
            saved = programRunStop; programRunStop = PGM_RUNNING
            reallyRunFunction(itemId, param)
            if programRunStop == PGM_RUNNING: programRunStop = saved
        if lastErrorCode != ERROR_NONE:
            abortDefinition()-if-open; stop line
        continue
    label = findNamedLabel(word, GLOBAL_LABELS) // §4.1 step 5 — number and item BEFORE
                                               // label; bare names are GLOBAL only
                                               // (§0.3 — locals need :NAME:, stage F3)
    if label != INVALID_VARIABLE:              // see §4.1 order rationale
        if state == COMPILE:                   // label ids RENUMBER on every edit —
            emit(FTOK_XEQN); emitKind(253); emitName(word)  // never bake one; §3.3.6.
        else:                                  // fresh findNamedLabel() per use.
                                               // Direct fnExecute, NOT the PGM_RUNNING
                                               // wrap — ITM_XEQ is the run-loop driver,
                                               // not an ordinary item (§3.3.6)
            dynamicMenuItem = -1               // fnGoto reads >= 0 as a global step no.
            fnExecute(label)
            if lastErrorCode != ERROR_NONE: stop line
        continue
    error(ERROR_FUNCTION_NOT_FOUND, word)      // §4.1 last resort — token in errorMessage
    abortDefinition()-if-open; stop line

end of line:
    if state == COMPILE:                       // unterminated definition —
        abortDefinition()                      // without the abort, the smudged entry
        error(ERROR_INVALID_NAME)              // leaks: permanently invisible yet
                                               // holding a dictionary index + arena bytes
    else if lastErrorCode == ERROR_NONE:
        setSystemFlag(FLAG_ASLIFT)             // mirrors the inner interpreter's
                                               // rsp == 0 exit
```

**Error code mapping:** Forth errors use distinct C47 error codes to
communicate the failure class accurately.  `ERROR_OPERATION_UNDEFINED` (13,
"Operation is undefined in this mode") is reserved for inner-interpreter
anomalies (re-entrancy guard, unsupported PTP classes) where the operation
genuinely cannot execute.  Outer-interpreter errors use codes that match the
failure semantics:

| Condition | Error Code | Text |
|-----------|-----------|------|
| Unknown word (last resort) | `ERROR_FUNCTION_NOT_FOUND` (7) | "No such function" |
| Syntax error (`:` in compile, `;` in interpret, unterminated def) | `ERROR_INVALID_NAME` (48) | "Invalid name" |
| Token exceeds `FORTH_TOKEN_MAX` (63) | `ERROR_INPUT_TOO_LONG` (10) | "Input is too long" |
| Empty or overlong definition name | `ERROR_INVALID_NAME` (48) | "Invalid name" |
| Outer re-entrancy guard | `ERROR_OPERATION_UNDEFINED` (13) | "Operation is undefined in this mode" |
| Inner re-entrancy guard | `ERROR_OPERATION_UNDEFINED` (13) | "Operation is undefined in this mode" |
| Unsupported PTP class in FTOK_C47 | `ERROR_OPERATION_UNDEFINED` (13) | "Operation is undefined in this mode" |

**Context display:** On `ERROR_FUNCTION_NOT_FOUND`, the offending token
is written to `errorMessage` before `displayCalcErrorMessage`.  The upstream
display layer (`screen.c` `_refreshRegisterLine`) concatenates
`errorMessages[code]` with `errorMessage` for select codes.  This is extended
to include `ERROR_FUNCTION_NOT_FOUND` so the display shows
`"No such function: TOKEN"`.  When the error occurs inside an open definition,
the context includes the definition name: `"No such function: TOKEN (in WORD)"`.

#### 3.3.1 State & line discipline

- `state` is a **local variable** of one `forthOuterInterpret()` invocation,
  initialized `INTERPRET`. It does not persist across lines; definitions are
  single-line in stage C — and therefore may not span program steps (§8.1).
  There is no interaction with C47's PRGM mode: in PEM the keypress records a
  program step (a source step or marker per §8.4) and `fnForthOuter` never
  runs; when a *running* program executes `ITM_FORTH`, PEM is not active and
  the entry point is `forthProgramStep` (§3.3.2/§8.2). The only
  shared-state hazard is `aimBuffer`/`tmpString` (§3.3.2).
- `:` while `state == COMPILE` → `ERROR_INVALID_NAME` (48, "Invalid name"),
   `abortDefinition()`, stop the line (a second snapshot would clobber the
   single static abort record).
- `;` while `state == INTERPRET` → `ERROR_INVALID_NAME` (48, "Invalid name"), stop.
- `:` with no following word on the line → `ERROR_INVALID_NAME` (48, "Invalid name"), stop
   (nothing allocated yet — no abort needed).
- End of line with `state == COMPILE` (unterminated definition) →
   `abortDefinition()` then `ERROR_INVALID_NAME` (48, "Invalid name").
- Defining a name that collides with a primitive is **allowed silently**;
  the new word is permanently shadowed (§4.1 prims win). Documented
  behavior, not an error.

#### 3.3.2 Source acquisition, private buffer, outer re-entrancy guard

Each interpret carries a **per-invocation `forthOuterCtx_t`** (source[256],
tokenizer position, `openDef` snapshot) on the **caller's C stack**, chained
through one static `forthOuterCur` pointer. `forthOuterDepth` caps nesting at
`FORTH_OUTER_NEST_MAX` (2), raising the same `ERROR_OPERATION_UNDEFINED` as the
inner cap (§3.2). The tokenizer's position lives in the context, not in statics;
`openDef` is snapshot/restored around nesting via `forthDefStateSave()` /
`forthDefStateRestore()`, so **a nested line can never close or abort the outer
line's definition**. Idle BSS cost: one pointer plus two bytes.

Nesting deeper than 2 is unreachable by construction — a label XEQ from a
program-context Forth step is continuation-style (`fnExecute`'s nested branch
pushes a level and defers stepping to the enclosing `runProgram()` loop), so
interpreter frames never stack past typed-line → program-step. The cap is a
backstop, pinned by a hook-primed test.

```c
#define FORTH_SOURCE_MAX      256             // bytes incl. NUL
#define FORTH_OUTER_NEST_MAX  2

typedef struct forthOuterCtx {                // on the CALLER's C stack
  char     source[FORTH_SOURCE_MAX];          // PRIVATE. Never tmpString,
                                              // never aimBuffer, never errorMessage
  int16_t  pos;                               // tokenizer position (§3.3.3)
  /* openDef snapshot — see forthDefStateSave()/Restore */
  struct forthOuterCtx *prev;
} forthOuterCtx_t;

static forthOuterCtx_t *forthOuterCur = NULL;
static uint8_t          forthOuterDepth = 0;

void fnForthOuter(uint16_t unused) {
  if (getRegisterDataType(REGISTER_X) != dtString) {
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ...); return;
  }
  int32_t len = stringByteLength(REGISTER_STRING_DATA(REGISTER_X));  // register
  if (len + 1 > FORTH_SOURCE_MAX) {           // strings are NUL-terminated in place
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ...); return;  // no silent truncation
  }
  forthOuterCtx_t ctx;
  xcopy(ctx.source, REGISTER_STRING_DATA(REGISTER_X), len + 1);
  fnDrop(NOPARAM);                            // copy MUST precede drop: drop
                                              // invalidates the register string.
                                              // Also consumes the source line first,
                                              // so interpreted words see a clean stack
  forthOuterRun(&ctx, FORTH_OUTER_FULL);      // pushes/pops ctx + depth; core is
}                                             // directly callable from PC tests
```

`forthOuterRun` performs the depth check, links `ctx` onto `forthOuterCur`,
saves the definition state, runs `forthOuterInterpret()`, then restores and
unlinks on **every** exit path.

Why the private buffer is mandatory, not stylistic: the interpret loop
*executes arbitrary C47 code between tokens* (prims call `fnAdd` etc.;
label fallback runs whole programs). `tmpString` is the machine-wide
scratch and `aimBuffer` doubles as the NIM buffer (c47.c:132); either can
be rewritten mid-line by an executed word, corrupting the unread remainder
of the source. The earlier §5.4 wording "tokenizer scratch (reuse
tmpString)" is **stricken** for the source line itself (RAM cost: 256 B BSS
+ 64 B token buffer, §3.3.3; §5.4 updated accordingly).

The interpret-state source of `fromProgram` for nested `forthInner` calls
(an earlier draft of the pseudocode wrote `forthInner(widx)` with the second
argument missing): `forthInner(widx, programRunStop == PGM_RUNNING)`.

**Program-step entry point — the second front-end over the same core.** When
the program runner reaches an `ITM_FORTH` source step, source comes from the
step's inline payload, not from X. A sibling entry point in `forth_compile.c`
shares the context machinery (`forthOuterCur`/`forthOuterDepth` are `static`
there [VERIFIED: packages/forth-core/forth_compile.c:23-24], which is why this
function must live in the same file). Full semantics are §8.2:

```c
// forth_compile.c — called only from the executeOneStep() ITM_FORTH arm (§8.2)
// payload points at the step's [len][bytes...] pair (after the type byte).
void forthProgramStep(const uint8_t *payload) {
  forthOuterCtx_t ctx;
  uint8_t len = *payload;                     // 0..255: always < FORTH_SOURCE_MAX(256)
  xcopy(ctx.source, payload + 1, len);        // copy BEFORE interpreting: executed
  ctx.source[len] = 0;                        // words may move/rewrite program memory
  forthRunGenCheckReset();                    // §8.3 lazy dictionary reset (also
                                              // resets the scanned-programs list)
  forthPreScanOwningProgram(payload);         // §8.2 first-touch pre-scan, DEFS_ONLY
  forthOuterRun(&ctx, FORTH_OUTER_SKIP_DEFS); // ':'…';' consumed, not recompiled
}
```

The private-copy rule of this section applies with extra force here: the
payload lives in *program memory*, and an interpreted word may run a C47
program (label fallback, §3.3.6) that edits or clears programs, moving
`beginOfProgramMemory` under the pointer. Never interpret out of program
memory in place. The X-register path (`fnForthOuter` above) is unchanged and
remains the interactive/REPL entry.

#### 3.3.3 Tokenizer (glyph-wise advance is a correctness requirement)

C47 two-byte glyphs are `lead byte & 0x80` + **arbitrary second byte**
(stringNextGlyph, charString.c:379-395; compareString, sort.c:70). The
second byte may itself be 0x20, so a byte-wise space scan can split a glyph.
`pos` therefore only ever moves by `stringNextGlyph`, and delimiter tests
happen only at glyph starts:

```c
#define FORTH_TOKEN_MAX 63     // bytes; > FORTH_NAME_MAX because number
                               // literals (e.g. 34-digit reals with exponent)
                               // legally exceed 31 bytes
static int16_t pos;            // byte offset into forthSource; always at a glyph start

// returns false at end of line; true with token copied NUL-terminated into buf
bool nextToken(char buf[FORTH_TOKEN_MAX + 1]) {
  while (forthSource[pos] == ' ')            // ' ' = 0x20, single-byte glyph;
    pos = stringNextGlyph(forthSource, pos); // test valid: pos is a glyph start
  if (forthSource[pos] == 0) return false;
  int16_t start = pos;
  while (forthSource[pos] != 0 && forthSource[pos] != ' ')
    pos = stringNextGlyph(forthSource, pos); // never tests a glyph's 2nd byte
  int16_t len = pos - start;
  if (len > FORTH_TOKEN_MAX) { displayCalcErrorMessage(ERROR_INPUT_TOO_LONG, ...);
                               return false; /* caller aborts line (and open def) */ }
  xcopy(buf, forthSource + start, len); buf[len] = 0;
  return true;
}
```

Delimiter set: exactly the single byte 0x20. No tab/newline handling —
source is one line. Definition names are additionally capped at
`FORTH_NAME_MAX` (31) **bytes** (not glyphs) by `startDefinition`;
tokens longer than 31 bytes are legal only as number literals. Never author
names as UTF-8; a UTF-8 lead byte (0xC0+) is misparsed as a C47 two-byte
glyph high byte.

**Keyboard-authored source:** alpha-mode keypad entry produces two-byte C47
operator glyphs for multiply (`STD_CROSS` `\x80\xd7` or `STD_DOT` `\x80\xb7`)
and divide (`STD_DIVIDE` `\x80\xf7`). The tokenizer's glyph-wise advance
delivers these as intact two-byte tokens. The prim table alias entries
(§1.3) ensure name lookup resolves them to the correct handler.

#### 3.3.4 Interpret-state execution discipline (mirror the inner interpreter)

Interpreted words must be observationally identical to their compiled forms.
After **every** dispatched action in interpret state — primitive call, colon
call via `forthInner`, number push, label execution — the loop must:

1. Primitive: `forthPrims[idx].fn(); clearSystemFlag(FLAG_ASLIFT);` —
   the same scrub the inner interpreter performs (forth_inner.c:170-171).
2. Check `lastErrorCode != ERROR_NONE` → stop the line immediately (and
   `abortDefinition()` if a definition is open — reachable via immediate
   words once they exist, and via number-push allocation failures).
3. On successful completion of the whole line: `setSystemFlag(FLAG_ASLIFT)`
   (mirrors the inner interpreter's `rsp == 0` exit; the
   `reallyRunFunction()` epilogue makes this redundant when entered via
   `ITM_FORTH`, but the PC-test path calls `forthOuterInterpret()` directly).

Number pushes reuse the committed helpers `forthPushInt32` /
`forthPushReal34` — public, declared in forth_dict.h [VERIFIED:
packages/forth-core/forth_dict.h:125-126; bodies forth_inner.c:32-54]. Do
not reimplement the lift discipline in the compiler.

#### 3.3.5 Numbers — type conformance and exact grammar (base rule)

**Number-type conformance (DECIDED 2026-07-06, audit — CONFORM to the
machine).** Forth numeric literals MUST produce the same register data type
keyboard entry produces, so `3` typed in Forth is indistinguishable from `3`
typed on the keyboard for every downstream type-dispatched behavior (integer
functions, display format, base conversion). Verified upstream (closeNim,
bufferize.c:2343):

- Integer entry (`NP_INT_10`, normal case) builds a **long integer**:
  `stringToLongInteger` + `convertLongIntegerToLongIntegerRegister(lgInt,
  REGISTER_X)` → `dtLongInteger` (bufferize.c:2434-2437).
- Decimal/exponent entry (`NP_REAL_FLOAT_PART`/`NP_REAL_EXPONENT`, default
  case) builds a **real34**: `reallocateRegister(REGISTER_X, dtReal34, 0,
  xangularMode)` + `stringToReal34` (bufferize.c:2642-2643).

Therefore: `FTOK_ILIT` pushes its int32 payload as a **long integer** using the
same `convertLongIntegerToLongIntegerRegister` idiom — NOT as a real34.
`FTOK_LIT` (explicit decimal/exponent forms) stays real34, matching the
keyboard. **IMPLEMENTED:** `forthPushInt32` builds a `longInteger_t` from the
int32 and stores it via `convertLongIntegerToLongIntegerRegister(lgInt,
REGISTER_X)` after the lift [VERIFIED: packages/forth-core/forth_inner.c:42-54];
`test_ilit_compile_interpret_parity` and acceptance §8.9-6 pin the type. Integer literals wider than int32 (upstream long integers
are arbitrary-precision) do not fit `FTOK_ILIT`'s 4-byte payload; stage 1
compiles them as `FTOK_LIT` real34 — a documented stage-1 limitation, applied
identically in interpret state so compile and interpret semantics never
diverge. The earlier acceptance wording "numbers parsed as real34" (§7.4) is
superseded by this rule.

**Number grammar (exact; the validator is ours, not decNumber's).**
Classification runs on the token **bytes**; any two-byte glyph (byte ≥ 0x80)
anywhere in the token disqualifies it as a number.

```
int  := [+-]? digit+                                   (digit = '0'..'9')
real := [+-]? ( digit+ '.' digit* | '.' digit+ | digit+ ) ( [eE] [+-]? digit+ )?
        and (contains '.' or contains e/E)             (else it classified as int)
```

- Anything not matching falls through to label lookup (§4.1), then
  undefined-word error. This grammar is the gate: `stringToReal34` is
  `decQuadFromString` (realType.h:116), which happily parses `"NaN"` and
  `"Infinity"` — those must never reach it.
- Radix mark is `'.'` **only**, regardless of the decimal-comma display
  flag (`decQuadFromString` accepts only `'.'`). Exponent marks: ASCII
  `e`/`E` only. Base is 10 only (no Forth BASE in stage C).
- Integer path: skip a leading `'+'` before conversion —
  `stringToLongInteger` is `mpz_set_str` (longIntegerType.h:38), which
  rejects `'+'`; upstream does the identical skip (bufferize.c:2434). Then:
  `longIntegerInit(li); if (stringToLongInteger(p, 10, li) != 0) → treat as
  non-number (defensive; grammar should prevent it)`. Range check with
  `longIntegerCompareInt(li, INT32_MAX) <= 0 &&
  longIntegerCompareInt(li, INT32_MIN) >= 0` (longIntegerType.h:99) —
  **never** bare `longIntegerToInt32` (`mpz_get_si` truncates silently).
  In range: `longIntegerToInt32(li, v)`; compile → `FTOK_ILIT` + 2 cells of
  int32 LE; interpret → `forthPushInt32(v)`. Out of range: fall to the
  real34 path *in both states* (the documented stage-1 limitation above).
  `longIntegerFree(li)` on every path.
- Real path: `real34_t r; stringToReal34(buf, &r);` compile → `FTOK_LIT` +
  8 cells; interpret → `forthPushReal34(&r)`.

#### 3.3.6 Calling the rest of the machine — C47 items and labels

This section is what makes Forth an extension of RPN rather than a parallel
system. Both crossings work in **both** states; they differ only in *what gets
baked into the compiled body*, and that difference follows one rule:

> **Bake ids that are stable; name-resolve ids that aren't.**

| id | stable? | mechanism |
|----|---------|-----------|
| item id (`indexOfItems[]`) | compile-time constant, never renumbers | bake → `FTOK_C47` |
| colon index (`FTOK_CALL`) | stable within a dictionary generation (§8.3) | bake |
| label id (`labelList[]`) | **renumbers on every program edit** | name-resolve → `FTOK_XEQN` |

**C47 items (§4.1 step 4).**

- *Compile state:* emit() `FTOK_C47 + itemId` (+ inline param per §4.4). The item
  id is an index into a flash table that never moves, so baking it is safe
  forever. `: F SIN ;` compiles.
- *Interpret state:* dispatch `reallyRunFunction(itemId, param)` under the
  PGM_RUNNING save/set/restore protocol (§2.2):
  `saved = programRunStop; programRunStop = PGM_RUNNING;
  reallyRunFunction(itemId, param);
  if (programRunStop == PGM_RUNNING) programRunStop = saved;`
  The wrap is **normative here** — the GTK refresh-pump livelock applies to any
  `reallyRunFunction()` call from Forth context, and the dispatched items are
  ordinary functions.

**C47 labels (§4.1 step 5).** Bare names resolve **global labels only**
(`findNamedLabel(word, GLOBAL_LABELS)` — named local labels require the
explicit `:NAME:` spelling, stage F3, §0.3/§10; a local request never falls
through to Forth vocabulary). The label id returned by `findNamedLabel()` is an
index into `labelList[]`, which `scanLabelsAndPrograms()` **frees, reallocates and
rebuilds by walking program memory in address order — on every edit** [VERIFIED:
src/c47/programming/manage.c:102-194, insertion-path call at :734]. Indices are positional:
insert a `LBL` earlier in memory and every later index shifts. A baked label id
therefore calls the *wrong program* — no error, no crash, just wrong. Upstream
never stores one: a program's `XEQ 'NAME'` step stores an inline name string
resolved at run time [VERIFIED: src/c47/programming/lblGtoXeq.c:365-368].

- *Compile state (landed, F3-6 `2db8af231`):* emit() `FTOK_XEQN`
  + kind byte 253 + the inline name (§2.2). Resolution happens at run time,
  fresh on every execution — the same guarantee upstream's `PARAM_LABEL` arm
  gives, reusing the same resolver. `: F MYPROG ;` compiles and stays correct
  across edits forever.
- *Interpret state (IMPLEMENTED):* `dynamicMenuItem = -1; fnExecute(label);` —
  a **direct** `fnExecute`, deliberately **not** the PGM_RUNNING wrap used for
  items. (Upstream has since adopted the same `dynamicMenuItem = -1` default
  at its own dispatch sites — b8f79e486 range, `fix/dynamic-menu-item-default`
  — so our clear is defense-in-depth; keep it.)
  `ITM_XEQ` is unlike an ordinary item: under a forced PGM_RUNNING, `fnExecute`
  takes its nested branch (level push + `fnGoto`, stepping deferred to an
  enclosing `runProgram()` loop that does not exist interactively) — the program
  never runs, a 3-block subroutine level leaks per call, and §8.3 bump site A is
  suppressed. Calling `fnExecute` directly bypasses the items.c normal-mode
  dispatch whose `refreshStatusBar()` pump the wrap existed to avoid, so the
  livelock is avoided anyway. `dynamicMenuItem` must be cleared **first**:
  `fnGoto`'s `dynamicMenuItem >= 0` branch reinterprets the label id as a global
  step number, and `fnExecute` resets it only *after* `fnGoto` — too late for a
  name-resolved, non-menu call.

**Emit scope.** The compiler emits `FTOK_PRIM`, `FTOK_CALL`, `FTOK_LIT`,
`FTOK_ILIT`, `FTOK_C47`, `FTOK_XEQN`, `FTOK_EXIT`. It does not emit()
`FTOK_BR`/`FTOK_0BR` — control-flow words are future work, and the branch
provision "hold offsets, not pointers" (§3.3.7) binds when they land.

#### 3.3.7 Dict-emit() API — grow-in-place (base decision 2026-07-05)

**Grow-in-place (DECIDED, verified against upstream 2026-07-05).**
The committed `forthDictAllocate(nameLen, bodyBytes)` requires the body size up
front, but a `:` definition's length is unknown until `;` — so the compiler
appends incrementally into an *open* entry. This is the upstream pattern for
building a variable-length managed region: C47 program editing appends each
step with `_insertInProgram` (programming/manage.c:680), which writes directly
into program memory and, when `freeProgramBytes < size`, grows the region via
`resizeProgramMemory` (memory.c:158) and then rebases every live absolute
pointer after the move (manage.c:687-697: `currentStep`, `firstDisplayedStep`,
`beginOfCurrentProgram`, `endOfCurrentProgram`). Forth's region-relative
offsets (§5.3) make the same pattern strictly simpler: after a move, only
`fdict.base` needs refreshing — which `forthDictEnsure` already does
(forth_dict.c:42-48). No staging buffer exists anywhere in the upstream
mechanism.

The API, mapped to the committed code (fills the earlier gap; exact
semantics, zero unstated decisions). In the pseudocode above,
`emit()`/`emit16()`/`emitBytes()` all denote `forthDictEmit` calls — inline
data wider than one cell is emitted as successive cells.
`forthDictEmit`/`startDefinition`/`finishDefinition()`/`abortDefinition()` are
**committed** exactly as specified below [VERIFIED:
packages/forth-core/forth_dict.c:290-371]. Key fact making this small:
`forthDictAllocate(nameLen, 0)` already performs the header-creation steps
verbatim — header written with `FF_SMUDGE`, `link = latest`,
`latest`/`count` updated, `here` bumped to exactly `ceil4(4 + nameLen)` =
body start (forth_dict.c:62-88).

```c
// forth_dict.c
static struct { uint16_t here, latest, count, entryOff; bool open; } openDef;

bool forthDictEmit(ftoken_t tok) {
  if (!forthDictEnsure(2)) return false;      // may move region; offsets-only
  memcpy(fdict.base + fdict.here, &tok, 2);   // discipline (§5.3) makes that safe
  fdict.here += 2;
  return true;
}
bool forthDictEmitBytes(const void *src, uint16_t nBytes) {  // nBytes even
  for (uint16_t i = 0; i < nBytes; i += 2) {
    ftoken_t c; memcpy(&c, (const uint8_t *)src + i, 2);
    if (!forthDictEmit(c)) return false;
  }
  return true;
}

bool startDefinition(const char *name) {      // forth_compile.c
  size_t nameLen = strlen(name);              // token is NUL-terminated ASCII/C47
  if (nameLen == 0 || nameLen > FORTH_NAME_MAX) { error; return false; }
  if (fdict.count >= 0x6F00) {                // FTOK_CALL space full: index 0x6F00
    displayCalcErrorMessage(ERROR_RAM_FULL, ...);  // would emit() 0x7F00 == FTOK_LIT
    return false;                             // (enforces the §7 invariant;
  }                                           // committed at forth_dict.c:327-330)
  openDef.here = fdict.here; openDef.latest = fdict.latest;
  openDef.count = fdict.count;                // snapshot BEFORE any mutation
  uint16_t off = forthDictAllocate((uint8_t)nameLen, 0);
  if (off == FORTH_NULL) return false;        // error already displayed; nothing to undo
  forthDictWriteName(off, name);
  uint16_t bodyOff = forthDictBodyStart(off); // == off + ceil4(4 + nameLen)
  for (uint16_t i = off + 4 + (uint16_t)nameLen; i < bodyOff; i++)
    fdict.base[i] = 0;                        // zero the 0..3 pad bytes: neither
                                              // Allocate nor WriteName does, and
                                              // arena memory is not zeroed
  openDef.entryOff = off; openDef.open = true;
  return true;
}
bool finishDefinition(void) {                 // at ';'
  if (!forthDictEmit(FTOK_EXIT)) { abortDefinition(); return false; }
  forthDictFinishDef(openDef.entryOff);       // clears FF_SMUDGE, block-rounds here
  openDef.open = false;
  return true;
}
void abortDefinition(void) {
  if (!openDef.open) return;
  fdict.here = openDef.here; fdict.latest = openDef.latest;
  fdict.count = openDef.count;                // region may stay grown — harmless;
  openDef.open = false;                       // the bytes above here are dead
}
```

Every `forthDictEmit*` failure while compiling → `abortDefinition()` and
stop the line (the ensure already displayed `ERROR_RAM_FULL`). The compiler
holds `openDef.entryOff` (an offset) and **no pointers** across emits —
including branch back-patch positions for future `IF`/`THEN` (stage 2); any
cached pointer is invalid after any emit(), because `forthDictEmit` can move
the region on any call (§5.3 discipline, restated).

Smudge/lookup interaction (verified committed): `forthFindColon` skips
`FF_SMUDGE` entries (forth_dict.c:172-176) but still counts them in the
index walk, and `forthDictAllocate` still increments `count` — so indices
of already-compiled `FTOK_CALL` tokens stay valid mid-definition, the open
word is unfindable (redefinition references the *old* meaning, standard
Forth — a half-built definition is invisible to lookup), and
`forthResolveXEQ` from a concurrently-running program cannot see the
half-built entry. No change needed.

REJECTED: temp-buffer + copy-at-`;`. It is not the upstream pattern
(`_insertInProgram` stages nothing), it imposes a hard body-length cap plus a
permanent BSS buffer, and the safety it would buy (no mid-definition moves) is
already guaranteed by the offsets-only discipline §5.3 imposes everywhere else.

**Resolved defect (for the record):** `forthDictWriteName` once copied
`strlen(name)` bytes ignoring the allocated `nameLen`. The committed helper
takes an explicit length and clamps it to the header's `nameLen` [VERIFIED:
packages/forth-core/forth_dict.c:212-223]. (Under the `startDefinition` spec
above the copy length is `nameLen` by construction; the clamp is defensive.)

#### 3.3.8 Dict hardening (land with the compiler)

1. **64 KB offset wrap — IMPLEMENTED.** `forthDictEnsure` rejects a grow whose
   `here + neededBytes` would not fit in 16 bits, before growing: on 256 KB
   hardware `reallocC47Blocks` can otherwise push the region past 64 KB and
   `here` silently wraps, corrupting the dictionary. The guard is
   `if ((uint32_t)fdict.here + bytes > 0xFFFEu) { RAM_FULL; return false; }`
   [VERIFIED: packages/forth-core/forth_dict.c:113-117]; 0xFFFF is the
   FORTH_NULL sentinel and must stay unused.
2. **Count cap:** enforced in `startDefinition` (code above);
   `forthDictAllocate` itself remains uncapped for test use, so the compiler
   must never bypass `startDefinition`.

#### 3.3.9 Immediacy scope (stage C)

`FF_IMMEDIATE` is honored for **primitives only** (`forthPrims[idx].flags`),
exactly as the pseudocode reads. Colon definitions cannot become
immediate in stage C (no `IMMEDIATE` word exists to set the flag, and
`forthFindColon` exposes no flags out-param), so the colon branch compiles
`FTOK_CALL` unconditionally — this is a **stated non-goal**, not an
oversight. Stage 2 (control-flow words) adds: an `IMMEDIATE` primitive
setting `FF_IMMEDIATE` on `fdict.latest`, and a flags out-param (or
`forthDictFlagsByIndex` helper) so the compiler can honor it. No stage-C
code may assume colon words are never immediate in the *encoding* (the
flags bit is already reserved and stored).

---


### 3.4 Primitive invocation and the spill bracket (landed, stage D3)

`forthPrimInvoke(idx)` is the ONLY way a primitive runs, from all four
dispatch sites (outer, inner loop, XEQN chain, compile-state path):
Apply the declared `stackEffect` — catching overflow into the spill
(§5.7), deepest register first via `getStackTop()` — then `fn()`, then
the ASLIFT convention, then `forthSpillSettle()` refills vacated deepest
slots LIFO. The two direct `fnDrop` consumes (0BR; the compile-state
string consume has no accounting by design — it eats the user's input
string) settle explicitly. Depth saturates at `forthStackCapacity()`;
the spill count carries the excess; only arena exhaustion errors.

Boundary: `forthDataDepthResync()` (every user-native seam) with a
non-empty spill is a loud `ERROR_RAM_FULL` stop naming the spilled
count, then reset — an arbitrary-arity native may not run over hidden
values. Acceptance pins: `7 FACT` = 5040; spilled and unspilled
computations produce identical results and window contents (WP-1/2).

## 4. Lookup order change

C47 today resolves an XEQ name through exactly one table: `findNamedLabel()`
(manage.c:1864). Forth introduces a **prepended** resolution order. The change
is realized in the Forth outer interpreter and in an override of the two named-
lookup call sites — it does **not** alter C47's own resolution when Forth is not
engaged.

### 4.1 New resolution order (highest priority first)
For a bare name typed at `ITM_FORTH` / found while compiling:

1. **Forth primitive** — `forthFindPrim(name)`: linear scan of `forthPrims[]`
   (flash). Primitives shadow everything (this is standard Forth and keeps
   `DUP`, `+`, etc. deterministic). Returns `uint16_t` with miss =
   `FORTH_PRIM_NONE` (0xFFFF); **never compare its result `>= 0`** — that
   test is always true on an unsigned, and every unknown word would dispatch
   `forthPrims[0xFFFE].fn()`.
2. **Forth colon definition** — `forthFindColon(name, &widx)`: walk
   `fdict.latest` link chain, newest-first (Forth redefinition semantics:
   latest wins), skipping `FF_SMUDGE` entries. Returns `bool`; the colon
   index is returned via out-param (`uint16_t *widx_out`), 0-based
   (0..count-1), `entries[widx]` indexes directly. A miss returns false;
   `widx_out` is left untouched. No sentinel index.
3. **Number literal** — per the exact grammar of §3.3.5.
4. **C47 item** — `forthFindItem(name, &itemId)`: linear scan of
   `indexOfItems[]` accepting an entry only when
   `(status & CAT_STATUS) == CAT_FNCT && (status & PTP_STATUS) == PTP_NONE`,
   matched with `compareString(name, indexOfItems[i].itemCatalogName, CMP_NAME)`.
   This is the seam that makes Forth an extension rather than a parallel
   system: `SIN`, `LN`, `ENTER` and the rest of the parameterless C47 function
   set resolve inside Forth source with the meaning they have on the keypad.
   Compile state emits `FTOK_C47 + itemId` (§2.2); interpret state dispatches
   directly. Parameterised items (`STO 05`) are §4.4.

   *The filter is a safety boundary, not a convenience.* `PTP_DISABLED` means
   "not programmable", so passing this filter means the item is already legal
   as a program step — a Forth word may do exactly what a keystroke program may
   do, no more. `EXIT` (`CAT_NONE | PTP_DISABLED`, items.c:3574) and `ALPHA`
   (`CAT_MENU`, items.c:3763) are excluded by it. `OFF` (`CAT_FNCT | PTP_NONE`,
   items.c:3380) resolves, and that is correct: `OFF` in a Forth word is
   exactly as dangerous as `OFF` in a keystroke program. The b8f79e486
   migration demonstrates the rule working as intended: upstream flipped
   `X.SWAP`, `X.EDIT` and `cpxSlv` to `PTP_NONE` and reclassified the
   pseudo-menu `PLTFCNS` as `CAT_FNCT`, so all four enter the Forth-callable
   set automatically — including `PLTFCNS`, which opens a menu, exactly as it
   would as a program step. Never replace this rule with a hand-maintained
   blacklist.

5. **C47 named label** — `findNamedLabel(name, GLOBAL_LABELS)` (unchanged
   upstream resolver; **global labels only** — named local labels require the
   explicit `:NAME:` spelling of stage F3 and never resolve from a bare name,
   §0.3/§10). Lets Forth call existing keystroke programs by name, in **both**
   states: compile state emits `FTOK_XEQN` + kind + the inline name (§2.2,
   §3.3.6), interpret state dispatches per §3.3.6. Labels resolve *after* items, so a keystroke program named `SIN`
   does not shadow the builtin inside Forth source; it stays reachable as
   `XEQ 'SIN'`.

   Else **undefined-word error**: `ERROR_FUNCTION_NOT_FOUND` (7, "No such
   function") with the offending token in `errorMessage` for context display.
   When the error occurs inside an open definition, the definition name is
   appended as `"(in WORDNAME)"` and the definition is aborted.

**Order rationale.** Two placements carry the whole design, and both follow the
same rule — *a user-chosen name must never hijack a meaning the machine already
owns*:

- **Number before label:** a user program labelled `"3"` must not hijack the
  numeric literal `3`. Numbers are still tried *after* the Forth dictionary, so
  a word may legally be named like a number-looking token if explicitly defined.
- **Item before label:** a user program labelled `"SIN"` must not hijack the
  builtin `SIN`. This is the same argument, and it is the extension principle:
  in RPN, `SIN` means the builtin.

Both cost the same thing — a program whose name collides with a literal or a
builtin is uncallable from Forth by bare name — and both keep the same escape
hatch: `XEQ 'NAME'`, which reaches the program in either state (§4.4, `FTOK_XEQN`).

Primitives and colon definitions sit ahead of all of it because that is standard
Forth: builtin words are deterministic, and a user's own definition shadows them
by redefinition. The §3.3 pseudocode order (prim → colon → number → item →
label) is normative.

**Name resolution is CASE-SENSITIVE** and uses `compareString(CMP_BINARY)`,
identical to C47 `findNamedLabel()` — user words and promoted labels resolve
under one rule. Primitive names use C47 glyph encoding (same as stored
labels), so a single `compareString` path serves both; ASCII names are
byte-identical single-byte glyphs, verified (§1.3).

### 4.2 Reverse direction — C47 `XEQ 'NAME'` finding a Forth word
So that a Forth word is reachable from the normal keyboard/`XEQ` and from
existing programs, the `XEQ` name-resolution path gains a Forth fallback. The
touch points are the two dynamic-menu `XEQ` branches and the program `PARAM_LABEL`
resolver:

- `runFunction()`, XEQ-by-menu branch (items.c:664-685): after
  `findNamedLabel()` returns `INVALID_VARIABLE`, call `forthFindColon`; on hit,
  `reallyRunFunction(ITM_FCALL, widx)` instead of erroring.
- `_executeOp`, `PARAM_LABEL`/`STRING_LABEL_VARIABLE` arm (lblGtoXeq.c:345-357):
   same fallback before `ERROR_LABEL_NOT_FOUND`, for `op == ITM_XEQ || ITM_XEQP1` only;
   all other PARAM_LABEL ops keep the upstream ERROR_LABEL_NOT_FOUND halt (audit fix F3).

**LBLQ semantics.** A Forth colon definition is not an RPN label. `LBL?` on a
name that resolves only in the Forth dictionary reports label-not-found
(`LBLQ`'s own negative result, not an error halt): `forthFallbackOp` gates the
Forth fallback to `ITM_XEQ`/`ITM_XEQP1` only, so a `FORTH_XEQ_COLON` result
for `op == ITM_LBLQ` falls through to the `op == ITM_LBLQ` arm
(`reallyRunFunction(op, INVALID_VARIABLE)`) instead of `ITM_FCALL` or
`displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ...)`. Verified by the FIX-3
gating in packages/forth-core/programming/lblGtoXeq.c:382-383 (`_executeOp`,
`PARAM_LABEL` arm).

**Complete `findNamedLabel()` call-site map** (20 sites, 6 hooked, 14 excluded):

| # | File | Line (upstream) | Context | Hooked? | Rationale |
|---|------|----------------|---------|---------|-----------|
| 1 | `items.c` | ~664 | XEQ-by-menu, `runFunction()` | **YES** (H1) | Primary user-facing XEQ path |
| 2 | `lblGtoXeq.c` | ~345 | `PARAM_LABEL`/`STRING_LABEL_VARIABLE` | **YES** (H2) | Program execution XEQ |
| 3 | `ui/tam.c` | ~917 | `_tamProcessInput`, XEQ alpha entry | **YES** (H3) | Interactive TAM XEQ `'NAME'` |
| 4 | `keyboard.c` | ~2229 | keyboard shortcut XEQ path | **YES** (H4) | Keyboard-driven XEQ |
| 5 | `screen.c` | ~813 | long-press/config execution | **YES** (H5) | Screen-driven XEQ |
| 6 | `forth_dict.c` | ~295 | `forthResolveXEQ` | **YES** (H6) | Central resolver (DESIGN.md §4.2) |
| 7 | `assign.c` | ~ | assignment target resolution | NO | Undesigned; re-entrancy TBD |
| 8 | `saveRestorePrograms.c` | ~ | program serialization | NO | Sub-phase E (save/restore) |
| 9 | `solver/*.c` | ~ | solver equation parsing | NO | Re-entrancy design stage 2+ |
| 10 | `registers.c` | ~ | register name resolution | NO | Re-entrancy design stage 2+ |
| 11 | `keyboard.c` | ~272 | dynamic softmenu label lookup | NO | Menu display, not execution |
| 12 | `keyboard.c` | ~1333 | label variable assignment | NO | Assignment path, not XEQ |
| 13 | `keyboard.c` | ~3181 | label resolution for catalog | NO | Catalog display, not execution |
| 14 | `lblGtoXeq.c` | ~1019 | dynamic menu label resolution | NO | Menu display, not execution |
| 15 | `forth_compile.c` | ~298 | compile-time label resolution | NO | Compile path, not runtime XEQ |
| 16-20 | `ui/tam.c` | ~ | TAM variable/label display | NO | Display-only, not execution |

**Exclusion rationale:** Sites 7-10 require re-entrancy analysis (stage 2+).
Sites 11-20 are display/assignment/compile paths — they do not execute user
code, so a Forth fallback would be incorrect or unnecessary.

*(Line numbers in the map above are pre-b8f79e486 and have drifted a few
lines; the Context and Hooked columns are the durable content — re-locate by
anchor, not number, and note upstream added `labelType` arguments at every
`findNamedLabel` site in the migration range.)*

**Order for reverse lookup:** C47 label first (preserve existing programs'
behavior exactly), Forth colon def second. This is the *opposite* precedence of
§4.1 on purpose — inside Forth, Forth wins; from the C47 side, C47 wins, so no
existing keystroke program silently changes meaning. The committed resolver
`forthResolveXEQ` implements label → C47 item name → colon, in that order
[VERIFIED: packages/forth-core/forth_dict.c:420-456], and the interactive TAM
hook applies the same order (label handled upstream of the hook; item-name
scan then colon fallback) [VERIFIED: packages/forth-core/ui/tam.c:964-991].

Label-kind pins (b8f79e486, named local labels — §0.3): `forthResolveXEQ`'s
label step and every Forth-side lookup bind `GLOBAL_LABELS`; a program step
that encodes a LOCAL name resolves against local labels or fails — the Forth
fallback in `_executeOp` is gated `opParam == GLOBAL_LABELS` and a local miss
**never** reaches Forth vocabulary [VERIFIED:
packages/forth-core/programming/lblGtoXeq.c, `forthFallbackEligible`]. The
interactive TAM hook carries the matching `!tam.colon` gate — upstream's own
fix, arrived with the migration base (AUD-U1 closed) [VERIFIED:
packages/forth-core/ui/tam.c:976].

**Item dispatch is unparameterized by construction.** The resolver's item arm
filters `CAT_FNCT + PTP_NONE` [VERIFIED: packages/forth-core/forth_compile.c:1064],
so a bare name never matches a parameterized item and the arm's `NOPARAM`
dispatch is always correct for what it can match. Parameterized items are
reachable only through their canonical spellings (B3, §10); a spelling that
fails to parse is an atomic syntax error, `ERROR_INVALID_NAME`, aborting any
open definition [VERIFIED: packages/forth-core/forth_compile.c:820-825].
Pinned by `test_xeq_item_lookup`, whose FCALL row asserts the B3-reverse
rejection of a bare parameterized item [VERIFIED:
packages/forth-core/test_engine.part.h:1223-1229].

**PEM recording of `XEQ 'NAME'` (names persist, never `widx`).** When
`XEQ 'NAME'` resolves to a Forth colon word while **recording in PEM**, the
step recorded is the ordinary name-string `XEQ` step (opcode +
`STRING_LABEL_VARIABLE` + name) via `insertUserItemInProgram` [VERIFIED:
src/c47/programming/manage.c:1775-1804; hook call at
packages/forth-core/ui/tam.c:964, working-tree change of 2026-07-10] — NOT an
`ITM_FCALL` step with a baked-in index. Resolution then happens at *run* time
in the `_executeOp` `STRING_LABEL_VARIABLE` arm through `forthResolveXEQ`
[VERIFIED: packages/forth-core/programming/lblGtoXeq.c:364-390], fresh on
every execution. This keeps the §8 invariant that program↔Forth crossings are
name strings; dictionary indices never persist in program memory. (The
`ITM_XEQP1` return-step adjustment when the target is a colon word is part of
the same change [VERIFIED:
packages/forth-core/programming/lblGtoXeq.c:376-378].)

**The names-only invariant is enforced at entry, not merely hoped for.**
`ITM_FCALL` is a keyable `PTP_NUMBER_16` item, so a user *can* reach
`FCALL nn` from the FCNS catalog in PEM. That gesture does not persist an
index: the `insertStepInProgram` FCALL arm reverse-looks-up the name via
`forthDictNameByIndex` and records an ordinary `ITM_FORTH` **name** step
instead; an unresolvable or indirect `widx` is rejected with
`ERROR_NON_PROGRAMMABLE_COMMAND` [VERIFIED:
packages/forth-core/programming/manage.c:1567-1582]. Verified by
`test_fcall_redirect_records_name` / `test_fcall_redirect_rejects_stale`.

### 4.3 Why not synthesize label IDs
Rejected alternative: register Forth words into `labelList[]` with synthetic IDs
in a reserved slice of `FIRST_LABEL..LAST_LABEL`. That would auto-populate the
PROG catalog (softmenus.c:1673) but couples Forth lifetime to label GC and risks
ID collisions with user programs. Instead Forth keeps its own dictionary and
exposes words to the UI via a dedicated dynamic catalog (future stage; the
`ITM_FCALL` bridge already makes them executable).

### 4.4 Parameterised items — C47 convention, not stack convention

C47 binds a parameter to its opcode **inline**: `STO 05` is one step, one
opcode plus one param byte. Forth follows that convention, because Forth is an
extension of RPN and not a new language:

```
STO 05        ← DECIDED. The parsing word consumes the next source token.
5 STO         ← REJECTED. Stack-idiomatic Forth, but not what C47 means by STO.
```

The two are mutually exclusive; this design picks C47's. In Forth terms a
parameterised item is a **parsing word** — it consumes source text at
compile/interpret time. That is legitimate Forth (`."`, `S"` do exactly this),
merely not stack-idiomatic.

**The param grammar belongs to the parsing word, never to §3.3.5.** This is a
correctness requirement, not a style note: `.05` means *local register 5* in C47,
and §3.3.5's number grammar would read it as the real `0.05`. Because the
parsing word takes the raw next token *before* the number rule ever sees it,
there is no collision — but only if implemented in that order.

**Encoding.** `FTOK_C47 + itemId + param`, param cell-padded (§2.2). The decoder
switches on the item's PTP class and mirrors the C47 VM's own arm for that class
— the VM is the reference implementation, so read it rather than re-deriving:

| PTP class | source form | inline param | decode |
|---|---|---|---|
| `PTP_NONE` | `SIN` | — | `reallyRunFunction(itemId, NOPARAM)` |
| `PTP_NUMBER_8` | `item nn` | 1 byte + 1 pad | value as-is |
| `PTP_NUMBER_16` | `item nnnn` | 2 bytes LE | value as-is |
| `PTP_REGISTER` | `STO 05`, `STO .05`, `STO X` | 1 KS-code byte + 1 pad | `regInRange(regKStoC(p))` then `reallyRunFunction(itemId, regKStoC(p))` — mirrors [VERIFIED: packages/forth-core/programming/lblGtoXeq.c:485-491] |

`regKStoC`/`regCtoKS` are existing inline helpers [VERIFIED: src/c47/defines.h:1386-1398].
Any PTP class outside this table raises `ERROR_OPERATION_UNDEFINED`.

**Phasing.** Phase 1 is `PTP_NONE` only — no grammar, no decoder change, ships on
what already exists (§3.3.6). Phase 2 adds `PTP_REGISTER` +
`PTP_NUMBER_8/16` with the direct-param grammar above. Phase 3 adds named and
indirect forms (`STO 'VAR'`, `STO IND 05`), which need the inline-string
machinery `FTOK_XEQN` introduces (§2.2) — build it once, use it twice.

---

## 5. Memory arena plan

### 5.1 The arena
```
src/c47/c47.c:90       uint32_t *ram = NULL;
src/c47/config.c:1517  ram = (uint32_t *)malloc(TO_BYTES(RAM_SIZE_IN_BLOCKS));
src/c47/defines.h:2213 #define BPB 2                 // 4 bytes / block
src/c47/defines.h:2052 RAM_SIZE_IN_BLOCKS_OLD_HW 16384   //  64 KB (DM42-class)
src/c47/defines.h:2053 RAM_SIZE_IN_BLOCKS_NEW_HW 65534   // 256 KB
src/c47/defines.h:2218 TO_PCMEMPTR / TO_C47MEMPTR         // 16-bit block-offset <-> pointer
src/c47/memory.c:76    void *allocC47Blocks(size_t blocks)
src/c47/memory.c:91    reallocC47Blocks(...)  / :  freeC47Blocks(...)
```

One contiguous `malloc`. Everything (registers, named vars, programs,
subroutine levels, the free-list regions themselves) is block-addressed with
16-bit offsets. `C47_NULL = 65535` (defines.h) is the null offset — this is why
`RAM_SIZE_IN_BLOCKS_NEW_HW` is 65534, not 65536.

### 5.2 Where the dictionary goes — option (A), chosen
The Forth dictionary is **one `allocC47Blocks` region**, treated like any other
managed block (same family as a program region). Grow with `reallocC47Blocks`;
free (on `CLEAR FORTH` / reset) with `freeC47Blocks`.

- Rejected option (B): reserve a fixed top-of-arena slice → requires editing the
  RAM map in `config.c`/`defines.h` (upstream, unconditional RAM theft). No.
- Option (A) costs **zero** RAM when no Forth words are defined, and cooperates
  with the existing allocator, undo save/restore, and `getFreeRamMemory()`
  (memory.c:6).

Initial allocation: lazily on first `:` definition — `allocC47Blocks(
FORTH_INITIAL_BLOCKS)` with `FORTH_INITIAL_BLOCKS = 64` (256 bytes). Grow policy:
when `here + need > sizeBlocks*4`, `reallocC47Blocks` to
`max(sizeBlocks*2, TO_BLOCKS(here+need))`, refresh `fdict.base`, done — subject
to the 16-bit offset cap (§3.3.8): `forthDictEnsure` rejects
`here + neededBytes > 0xFFFE` with `ERROR_RAM_FULL` before growing, because on
256 KB hardware the region could otherwise pass 64 KB and silently wrap `here`
(0xFFFF is the `FORTH_NULL` sentinel and must stay unused).

### 5.3 Why region-relative links
`reallocC47Blocks` may relocate the region. Absolute `C47MEMPTR` links stored
inside headers would all break on a move. **Links and `ip`/`rstack` entries are
byte offsets relative to `fdict.base`.** On relocation only `fdict.base` changes;
no header rewriting, no pointer fix-up pass. Convert to a usable pointer exactly
at deref time: `ptr = fdict.base + offset`.

### 5.4 Budget & high-water reporting (required by CLAUDE.md)
Per-word arena cost, in bytes (the earlier `2*(tokenCount + 1)` form
undercounted every inline payload and is superseded):

```
cost(word) = ceil4(4 + nameLen) + 2 * cells
```

where `cells` = 1 (the `FTOK_EXIT`) + per token: PRIM/CALL 1, ILIT 3, LIT 9,
BR/0BR 2, C47 2 (PTP_NONE) or 3 (PTP_NUMBER_8 padded, PTP_NUMBER_16).

Worked examples:
- `: SQ DUP * ;`  → name 2, cells = 1(EXIT) + 1(`DUP`) + 1(`*`) = 3 →
  `ceil4(6)=8` + `2*3=6` = **14 bytes** (4 blocks).
- 31-char name, body of 100 single-cell tokens (PRIM/CALL) →
  `ceil4(35)=36 + 2*101=202` = **238 bytes** (60 blocks).

Fixed overheads: `forthDict_t` (12 bytes BSS), `rstack` (128 bytes BSS), the
per-invocation `forthOuterCtx_t` (256-byte source buffer + tokenizer position,
on the **caller's C stack** since D-3 — idle BSS is one pointer plus a depth
byte, §3.3.2; the earlier "tokenizer scratch (reuse tmpString)" and the
256-byte-BSS accounting are both **stricken**), the token buffer (64 bytes,
stack, §3.3.3), and the `openDef` abort record (BSS, §3.3.7). Flash:
`forthPrims[]` table ≈ `primCount * (4+1+3+pad)` plus primitive code.

**Reporting rule (RULED 2026-07-15, Q3 + RULE-1).** The self-test suite's
`FORTH ARENA: dict here=.. sizeBlocks=.. freeRamDelta=..` line, printed by
every gate run, **is** the arena reporting mechanism: quote it in every
dictionary-touching commit message (current baseline: here=36 sizeBlocks=16
freeRamDelta=64). Dictionary region high-water ceiling stays ≤ **2 KB**
(512 blocks). A scripted benchmark (`bench/hwm.fs`) is optional future work,
not a requirement. **Flash:** per the platform ruling (§0 header), flash is
not a design veto on the R47/DM42n target — a stage that adds flash records
the measured binary delta from `make dmcp5r47` in its commit message instead
of justifying bytes away.

### 5.5 Save / restore
The dictionary region participates in state save exactly like other managed
blocks. `fdict.{here,latest,count,sizeBlocks}` and the region's `C47MEMPTR` must
be added to the save/restore descriptor set alongside the existing
`labelList`/`numberOfLabels` entries:

```
src/c47/saveRestoreBackup.c:398  saveStateValue(&numberOfLabels, ...)
src/c47/saveRestoreBackup.c:526  ramPtr = TO_C47MEMPTR(labelList); saveStateValue(&ramPtr, ...)
```
Mirror these for the Forth region (hook H5, §6). Because links are
region-relative, the saved bytes are position-independent; on restore, set
`fdict.base = TO_PCMEMPTR(savedRamPtr)`.

**IMPLEMENTED (H5, 2026-07-13 — commits "P1 save/restore integration (H5)"
and "P1 validator hardening"):** five name-keyed parameters in the package
patch of `saveRestoreBackup.c`, anchored after the `programList` pair on
both the save and restore sides: `forthDictBase` (c47Ptr) plus
`forthDictSizeBlocks`/`forthDictHere`/`forthDictLatest`/`forthDictCount`
(uint16). No `...Offset` companion is needed — `fdict.base` is always the
block-aligned raw allocC47Blocks result. Defaults are pre-seeded before
every `restoreStateValue` call, so pre-H5 backup files load as an empty
dictionary. `forthDictValidateRestored()` (forth_dict.c) clamps
inconsistent restored state to empty; on failure it deliberately ORPHANS
the region rather than freeing through the very allocation tables it just
failed to trust (documented exception). The config.c self-test hook is
run-once guarded because restoreCalc re-enters doFnReset. Tests
T1.1–T1.4 + T1.3b (validator direct pins), all mutation-verified.

### 5.6 Allocator double-free guard (override: core/freeList.c)

upstream `freeListFree` has no defense against a double free or an invalid
free. On PC-simulator builds a double free of an exact address already
produced misleading diagnostics because the address had been removed from
`allocatedMemoryRegions[]` by the first free — but the function still
inserted the region into the free list a second time, corrupting it. On DMCP
(device) builds the diagnostic code is compiled out entirely, so the same
call silently corrupts the free list.

The override `packages/forth-core/core/freeList.c` inserts a
range-overlap guard in `freeListFree`, immediately after
`C47RamPtr = TO_C47MEMPTR(pcMemPtr);` and before the existing
`#if !defined(DMCP_BUILD)` diagnostic block:

1. Runs **unconditionally** (before any `#if !defined(DMCP_BUILD)` gate),
   so device builds get the same protection as the simulator.
2. Checks `[C47RamPtr, C47RamPtr+sizeInBlocks)` against every existing
   `freeMemoryRegions()[]` entry for interval overlap (not exact-address
   match), so a double free of an address that has since coalesced into a
   larger region is still caught.
3. Never mutates `freeMemoryRegions()[]` on a hit — logs (PC builds only, via
   `errorf`/`fprintf(stderr, ...)` plus a backtrace) and returns. A double
   free is always a caller bug; the free list must survive it unchanged.
4. Restores the overlap detector at the bottom of `freeListFree` and
   `freeListReduce` to `>=` (its original upstream form), since
   adjacent-but-not-yet-coalesced free regions can no longer occur once the
   guard above prevents the duplicate insertion.

The override is byte-identical to upstream except this single hunk. It is
an upstream-MR candidate: the guard is not Forth-specific; `freeListFree` is
the shared C47 allocator used by GMP reals, config.c, register data, and
the Forth dictionary/label-list machinery alike.

Test coverage (`packages/forth-core/test_dict_reloc.c`, FIX-6 section):
`test_freelist_double_free_guarded` (exact-address double free),
`test_freelist_interior_double_free` (double free of an address coalesced
into the interior of a larger region),
`test_freelist_no_mutation_on_oversize_free` (double free with a larger
size than originally allocated must not grow the region). All three assert
the free list is byte-for-byte unchanged and that `test_freelist_consistent()`
still passes afterward.

---


### 5.7 The spill region (landed, stage D3)

One arena block (`allocC47Blocks`/`reallocC47Blocks`), LIFO records
`[uint32 dataType][uint16 sizeInBlocks][payload]` — byte-faithful
register images, no second numeric representation. Per-execution
lifetime: reset at both `forthDataDepthEnterOuter/LeaveOuter` seams,
NEVER persisted (power-off abandons execution state; restoreCalc sees
no spill). A line that completes with a non-empty spill is a loud
`ERROR_RAM_FULL` stop before the reset — the visible stack is the only
legal carrier of values across lines. The arena high-water discipline
(§5.4) applies: every gate proves the spill frees everything it took.

## 6. Exact hook points (file:line)

Each hook is an edited copy of an upstream file, placed in the package's **flat
working area** (`packages/forth-core/<upstream-rel-path>`). There is nothing to
declare: `tools/pkg_patch_refresh.py` scans the working area and classifies each
file automatically — a file mirroring a real upstream path becomes a generated
`patches/<NNN>-<rel>.patch`; a file with no upstream counterpart is copied to
`files/<rel>`. `patches/` and `files/` are **generated output, never edited by
hand**, and every `*.patch` found there is applied automatically. Run
`python3 tools/pkg_patch_refresh.py packages/forth-core` (or `make pkg_build
PKG=packages/forth-core`, which calls it) after any edit.

Keep every override byte-identical to upstream except the marked insertion; that
is what keeps the generated diff small and future upstream merges reviewable.

| id  | file (override)                     | upstream anchor (re-verify before editing)                                   | edit                                                                                          |
|-----|-------------------------------------|------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| H1  | `src/c47/items.c`                   | `indexOfItems[]` def at **items.c:1758**; spare rows **items.c:4690-4691**    | Replace slots 2842/2843 with the exact `ITM_FORTH`/`ITM_FCALL` rows given in §0.2 (param field, tamMinMax, EIM stated there).      |
| H1b | `src/c47/items.h`                   | `#define ITM_2842 2842`, `#define ITM_2843 2843` (items.h ~2949-2950)         | Add `#define ITM_FORTH 2842` / `#define ITM_FCALL 2843` aliases (keep the numeric `ITM_2842`/`ITM_2843` names too). Do NOT touch upstream's `ITM_FWORD 2003` (items.h:2056) — that is the swap-endian item, referenced by softmenus.c:866 (§0.1 naming warning). |
| H2  | `src/c47/programming/lblGtoXeq.c`   | `_executeOp` `PARAM_LABEL` arm **lblGtoXeq.c:341-357**                        | Before `ERROR_LABEL_NOT_FOUND`, add Forth colon-def fallback → `reallyRunFunction(ITM_FCALL,widx)`. |
| H3  | `src/c47/items.c`                   | `runFunction()` XEQ-by-menu branch **items.c:664-685**                          | Same fallback as H2 for interactive `XEQ 'name'`.                                              |
| H4  | `src/c47/keyboard.c`                | `executeFunction()` **keyboard.c:928** (near runFunction() call **:1164/:1429**) | **[LANDED]** §4.2 site 4: Forth fallback after label miss (`forthFindColon` → `reallyRunFunction(ITM_FCALL, widx)` at **keyboard.c:2293-2300**); P-H7: `MNU_FORTH` picker (`forthPickerGuard` + `pickerInsertName` at **keyboard.c:13-46**, dispatch **:1001-1008**, softmenu case **:113-118**). |
| H5  | `src/c47/saveRestoreBackup.c`       | label save **:398/:526**, restore **:815-816/:988**                          | Add symmetric save/restore of the Forth region ptr + `fdict` scalars (§5.5).                    |
| H6  | `src/c47/softmenus.c`               | dynamic-catalog switch **softmenus.c:1657**, PROG build **:1673-1704**       | `MNU_FORTH` dynamic-menu case — **re-scoped by §8.6**: it enumerates `: NAME` text-scan results from the current program's `ITM_FORTH` steps (not `fdict` names), mirroring the PROG label loop [VERIFIED: src/c47/softmenus.c:1673-1704]. Also add `MNU_FORTH` to the rebuild-always condition at softmenus.c:3039 (§8.6). |
| H7  | `src/c47/config.c`                  | `doFnReset` **config.c:1506**; `memset(ram, 0, ...)` **:1519**               | **[LANDED]** §6.2 reset hook: `#include "forth_dict.h"`; `forthDictInit()` after RAM clear (**config.c:1957**); PC self-test runner: `forthDictSelfTest()` + `exit(0)` on headless (**config.c:1959-1974**). |
| H8  | `src/c47/error.c`                   | `fnErrorMessage` tmpString formatting **error.c:282-289**                     | **[LANDED]** concatenation: `ERROR_FUNCTION_NOT_FOUND` with `errorMessage[0]` → `sprintf(tmpString, "%s: %s", errorMessages[lastErrorCode], errorMessage)` (**error.c:288-289**). |
| H9  | `src/c47/screen.c`                  | `_executeItem` label-not-found **screen.c:822**; error display **screen.c:3725** | **[LANDED]** §4.2 site 5: Forth fallback after label miss (`forthFindColon` → `reallyRunFunction(ITM_FCALL, widx)` at **screen.c:823-830**); display: `ERROR_FUNCTION_NOT_FOUND` with `errorMessage[0]` → concatenated message with width guard (**screen.c:3734-3741**). |
| H10 | `src/c47/core/freeList.c`           | `freeListFree` after `C47RamPtr = TO_C47MEMPTR(pcMemPtr);` **freeList.c:205** | **[LANDED]** core/freeList.c — freeListFree guard hunk only — unconditional range-overlap double-free rejection; upstream-MR candidate (§5.6). |

§8 (PEM-native entry) adds the following hooks. Same override discipline:
byte-identical to upstream except the marked insertions. **All rows below are
[LANDED]** — one generated patch per entry (see Override status).

| id  | file (override)                     | upstream anchor (re-verify before editing)                                   | edit                                                                                          |
|-----|-------------------------------------|------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| P-H1 | `src/c47/items.c` *(existing override)* | `ITM_FORTH` row, package items.c:4722                                    | `PTP_NONE` → `PTP_REM` (§0.2). Claim slot 213 for the `MNU_FORTH` `CAT_MENU` row (§0.1, §8.6). |
| P-H2 | `src/c47/programming/manage.c` **(new override)** | REM route **manage.c:1386-1399**; addItemToBuffer route **:1411**; `pemAlpha` **:773-966**; `pemAlphaEdit` **:982-998**; fnPem cursor hack **:566-575** | `ITM_FORTH` toggle arm + in-region capture route in `insertStepInProgram`; `ITM_FORTH` support in `pemAlpha` (EDIT-extraction: bare, no prefix and no quotes; empty-commit rule) and `pemAlphaEdit`. The fnPem cursor-offset hack **does** need a Forth branch keyed on `tam.function == ITM_FORTH` (`cursorInString = T_cursorPos - 2`, R3-1 — see E7). All specified exactly in §8.4. |
| P-H3 | `src/c47/programming/lblGtoXeq.c` *(existing override)* | `executeOneStep()` `PTP_REM` arm, package lblGtoXeq.c:838-863; `fnExecute` :~270-303; `runProgram()` :886-897 | `ITM_FORTH` case in the `PTP_REM` arm → `forthProgramStep` (§8.2); run-generation bump sites (§8.3). |
| P-H4 | `src/c47/programming/decode.c` **(new override)** | `decodeRem` **decode.c:828-843**                                          | `ITM_FORTH` marker arm: zero-length payload renders `»FORTH`/`FORTH«` from scan parity (§8.5); non-empty payloads render the source text **bare** — no item-name prefix, no quotes — NOT the generic `FORTH '…'` form (§8.5, E7). |
| P-H5 | `src/c47/softmenus.c` **(new override — same file as H6)** | `softmenu[]` dynamic area **:1017-1029**, `dynamicSoftmenu[]` **:1211-1234**, `initVariableSoftmenu` **:1648+**, cached rebuild **:3039** | `MNU_FORTH` rows appended to BOTH arrays (order must match — upstream comment softmenus.c:1021-1028); `initVariableSoftmenu` case building the `: NAME` scan content (§8.6); `MNU_FORTH` added to the rebuild-always disjunction. |
| P-H6 | `src/c47/defines.h` **(new header override)** | `NUMBER_OF_DYNAMIC_SOFTMENUS 22` **defines.h:1429**                        | 22 → 23. This is the upstream-documented procedure for adding a dynamic menu ("don't forget to adjust NUMBER_OF_DYNAMIC_SOFTMENUS in defines.h", softmenus.c:1025-1028). defines.h is machine-wide: keep the override byte-identical except this one line, and re-diff it on every upstream merge. |
| P-H7 | `src/c47/keyboard.c` *(existing override)* | dynamic-menu dispatch; `dynmenuGetLabel` idiom **keyboard.c:1153-1156**    | `MNU_FORTH` picker press → insert name text + one space into `aimBuffer` at `T_cursorPos` during Forth capture (§8.6). |

† The §6 hook IDs (H1–H9, P-H1–P-H7) and the §4.2 call-site-map H-numbers
are independent numbering schemes; an H-number in §4.2 does not correspond to
a hook of the same ID in §6.

**Override status.** Every file in the working area that mirrors an upstream
path is live by construction — classification is automatic, so there is no
registration step to forget and no list to keep in sync. Current overrides:
`items.c`/`items.h`, `defines.h`, `config.c`, `error.c`, `screen.c`,
`keyboard.c`, `softmenus.c`, `saveRestoreBackup.c`, `core/freeList.c`,
`programming/lblGtoXeq.c`, `programming/manage.c`, `programming/decode.c`,
`ui/tam.c` [VERIFIED: packages/forth-core/patches/ — one generated patch per
entry].

Genuinely-new package sources (no upstream counterpart — auto-copied to
`files/`):
```
packages/forth-core/forth_dict.c/.h      dictionary region mgmt, find*, grow, save hooks
packages/forth-core/forth_prims.c/.h     static primitive table (index-stable)
packages/forth-core/forth_inner.c        threaded-code interpreter (§3.2)
packages/forth-core/forth_compile.c      tokenizer + : ; compiler (§3.3)
packages/forth-core/forth_bridge.c       fnForthCall (ITM_FCALL), §8.4 derived-state helpers
packages/forth-core/test_dict_reloc.c    package self-tests
```
The package declares no build file. `.refresh-manifest.json` records the hash of
every generated entry plus the `base_commit` the package was authored against;
`refresh` warns and self-heals if a `patches/`/`files/` entry was hand-edited.

### §6.2 Reset Hook

When the user performs a RESET (ON CLEAR in C47, or programmatic doFnReset):

1. RAM is zeroed (config.c, memset by existing code).
2. freeMemoryRegions() rebuilds the free-list metadata.
3. **forthDictInit() is called** (new, as part of the reset sequence).

This call:
- Resets fdict.base, fdict.here, fdict.latest, fdict.count to zero.
- Clears the managed block arena, preparing for new Forth definitions.
- Ensures no stale pointers into reused RAM (hardware lifecycle safety).

See fix #11 (Fable audit, hardware reset safety).

**Two reset primitives — do not confuse them.** `forthDictInit()`
(forth_dict.c:39-46) only zeroes the control block; it is correct **only**
when the arena itself has just been rebuilt (the RESET path above,
config.c:1957 in the package override [VERIFIED:
packages/forth-core/config.c:1957]). Calling it while the arena is live
*leaks the dictionary region* (base is dropped without `freeC47Blocks`).
`forthDictClear()` (forth_dict.c:48-58) frees the region first and is the
correct primitive for any live-arena reset — in particular the §8.3
run-scoped dictionary reset [VERIFIED: packages/forth-core/forth_dict.c:39-58].

---

## 7. Stage-1 acceptance (what the local model must deliver)

1. H1/H1b: two items live; `XEQ` of neither crashes; `indexOfItems` size
   unchanged (still ≤ `LAST_ITEM`).
2. `forth_dict.c`: create region lazily, define/find/redefine words, grow across
   a `reallocC47Blocks` move without corruption (unit-test on PC build with a
   forced small initial size to trigger a move).
3. `forth_inner.c`: `: SQ DUP * ; 3 SQ` leaves 9 in X. `FTOK_0BR`/`FTOK_BR`
   exercised by a hand-assembled body test (e.g. the Stage-1-B backward-loop
   test, §2.2 notes) — the sub-phase C compiler does not emit() branch tokens
   (§3.3), so `: ABS DUP 0BR ... ;`-style source tests are stage 2.
4. `forth_compile.c`: interpret vs compile state; `FF_IMMEDIATE` respected
   (primitives only — §3.3.9);
   integer literals parsed as long integers (`FTOK_ILIT` → `dtLongInteger`),
   decimal/exponent literals as real34 (`FTOK_LIT`), matching keyboard entry
   (§3.3.5 number-type conformance).
5. H2/H3: `findNamedLabel()` miss falls through to Forth; existing keystroke
   programs unaffected (regression: a program named the same as a Forth word
   still runs the *program* from the C47 side).
6. H5: save → restore round-trips the dictionary; region-relative links intact.
7. Arena high-water printed for `bench/hwm.fs`; report the mark in the commit,
   ≤ 2 KB region ceiling on 64 KB HW.

### Invariants (must hold at all times)
- `forthPrimCount ≤ 0x0FFF`; `fdict.count ≤ 0x6F00` — equivalently, every
  emitted `FTOK_CALL` token ≤ 0x7EFF (max colon index 0x6EFF; index 0x6F00
  would emit() 0x7F00 == `FTOK_LIT`). The count cap is enforced at run time by
  `startDefinition` (§3.3.7); `forthDictAllocate` remains uncapped for
  test use, so the compiler must never bypass `startDefinition`. The prim
  bound is enforced at compile time: `_Static_assert(PRIM_COUNT <= 0x0FFF,
  ...)` [VERIFIED: packages/forth-core/forth_prims.c:51] — this is the one
  invariant that would break silently when primitives are appended, which is
  why it is a build-time assert and not a runtime check.
- `fdict.here + neededBytes ≤ 0xFFFE` checked at the top of `forthDictEnsure`
  (§3.3.8); 0xFFFF is the `FORTH_NULL` sentinel and must stay unused.
- Every header is 4-byte aligned; `fdict.here` is always block-rounded.
- Links & `ip` are region-relative; the only absolute pointer is `fdict.base`,
  refreshed immediately after any (re)alloc.
- Forth never grows `indexOfItems[]` and never invents item IDs beyond 2843.
- Error handling routes through C47 (`displayCalcErrorMessage`, `lastErrorCode`)
  so undo/trace/hourglass stay consistent with the rest of the machine.

## 8. PEM-native Forth entry — source-as-truth program steps

Goal: in PEM, define a Forth word and use it later in the *same* program with
no separate trip through the FORTH catalog item — the Forth analog of
`LBL … RTN/END` defining and calling a subroutine inline.

Decisions in force:

1. **Source-as-truth.** A Forth line is a self-identifying `ITM_FORTH` program
   step whose payload is the source text (§8.1). The compiler
   (`forthOuterInterpret()`) stays the single authority on token layout;
   program→Forth references are name strings resolved at run time; a dictionary
   index never persists in program memory (§4.2).
2. **Run-start pre-scan.** The first touch of a program's Forth steps compiles
   every definition in that program, then the current step executes (§8.2).
   Forward references work; the stored representation is unaffected.
3. **Program-scoped dictionary lifecycle** via a run-generation counter and
   lazy reset (§8.3).
4. **Entry-only toggle** — one item (`ITM_FORTH`), no runtime mode flag;
   keypad state is *derived*, never stored (§8.4); symmetric `»FORTH`/`FORTH«`
   display computed at render time (§8.5); in-region `: NAME` picker (§8.6).
5. **Errors** halt the program at the offending step with Forth-meaningful
   messages (§8.7).

**Why the source is the truth, and not compiled tokens.** The question recurs,
so it is settled here. An RPN step was never compiled *from* anything — you
pressed a key and the item was recorded; the byte **is** the canonical form and
decode is a bijection. RPN entry is selection from a finite set; Forth entry is
typing. There is no "RPN convention for storing typed text" to inherit. Storing
compiled tokens instead was examined and rejected on two grounds:

- *Compile into the arena, keep the dictionary persistent:* the arena would have
  to hold every definition of every stored program simultaneously, across
  power-off, against the ≤ 2 KB high-water ceiling on the 64 KB part (§5.4) —
  and it contradicts §8.3, whose whole purpose is that runs are deterministic
  and words do not accumulate.
- *Compile into program memory, tokens in the step:* calls must be
  name-resolved anyway, and a call still has to *find* its definition — which is
  the §8.2 pre-scan again, walking tokens instead of text. Every piece of
  run-time machinery is kept, the source is thrown away, and `EDIT` (§8.4) then
  needs a full decompiler: a second body of code in flash, on a target where
  flash is the binding constraint.

`PTP_REM` then buys every upstream consumer for free (§0.2). What the compiled
form *would* have bought — errors at entry rather than at run — is obtained
instead by validating the line on commit (§8.4), with no storage change.

### 8.1 Stored representation

One step shape, two meanings (encoding normative in §2.1, ):

```
0x8B 0x1A  STRING_LABEL_VARIABLE(0xFD)  len  bytes[len]
```

- `len > 0`  → **source step**: `bytes` is one Forth source line in C47 glyph
  encoding (same encoding `pemAlpha` writes into `aimBuffer`,
  §3.3.3 "keyboard-authored source").
- `len == 0` → **toggle marker**: a run-time no-op recording where the author
  flipped Forth entry on/off (§8.4). Marker vs. source is decided by `len`
  alone; there is no second item id.
- **Empty source lines are not representable** — the `len == 0` encoding is
  reserved for markers. The entry layer enforces this (§8.4 rule E3).
- Payload capacity: 255 bytes (1-byte len). The capture buffer caps entry at
  196 glyphs / <256 bytes [VERIFIED: src/c47/programming/manage.c:854], and
  every payload fits `FORTH_SOURCE_MAX` (256 incl. NUL) [VERIFIED:
  packages/forth-core/forth_compile.c:22] — `len ≤ 255` ⇒ `len + 1 ≤ 256`,
  no truncation path exists.
- Step length, listing decode, CLCVAR scan, and PEM insertion all ride the
  existing `PTP_REM` machinery with zero upstream edits (citations in §0.2
).
- A **definition may not span steps**: `forthOuterInterpret()`'s compile state
  is local to one line (§3.3.1), so `: NAME … ;` must be complete within
  one source step. An unterminated definition is aborted and reported at that
  step (§8.7).

### 8.2 Execution semantics — run-start pre-scan

**The model.** `forthProgramStep` runs `forthRunGenCheckReset()` (§8.3 — which
also resets the scanned-programs list), then a **first-touch pre-scan of the
owning program** (`forthPreScanOwningProgram`: every `ITM_FORTH` source step is
interpreted in `DEFS_ONLY` mode — definitions compile, interpret-state tokens
are skipped, so nothing in the tail executes early), then executes the current
payload in `SKIP_DEFS` mode (`:`…`;` regions are consumed without touching the
dictionary, so nothing recompiles). Scope is **exactly the owning program**,
resolved via `forthOwningProgramStart` / `forthNextProgramStart`
(forth_bridge.c). Tracking (landed, F1-3 `ecbd6bcce`): dynamic 8-byte records
inside the dictionary region, newest at `forthScanHead`
(`forthScanTrackReset` / `forthScanIsRecorded` [VERIFIED:
packages/forth-core/forth_compile.c]) — capacity failure is ordinary
dictionary exhaustion, never a program-count cliff (R4-E1).
Interactive `fnForthOuter` keeps `FULL` (compile-and-execute-in-place)
semantics.

**Why a pre-scan at all.** Without it, a definition exists only after execution
has fallen through its source step, so a `GTO` jumping over `: SQ … ;` leaves
`SQ` undefined at the call site — while `LBL`s, being pre-scanned by
`scanLabelsAndPrograms()`, do not have that hazard. That asymmetry is exactly the
kind of divergence-from-RPN this design does not accept. The pre-scan buys
forward-reference parity: interpret-state (tail) references resolve against any
definition in the same program, earlier or later.

**The stored representation is unchanged by any of this** — that is the payoff
of source-as-truth (§8.1). The pre-scan re-derives the dictionary from program
text; it is a cache-fill, not a format.

Documented limitations, each a consequence rather than a defect:

1. Definition-BODY forward references (def → later def) error at pre-scan time,
   at the referencing step. This is standard Forth define-before-use.
2. A pre-scan error halts the run at the triggering step **before** its tail
   executes, and the program stays unrecorded — so a fixed program re-scans.
3. Scan-list overflow (>8 Forth-bearing programs per run) re-scans and
   recompiles on later touches. Shadowing keeps lookups correct (§4.1
   newest-first); the cost is dictionary bytes.
4. Editing programs between single-steps leaves dictionary and scan list stale
   until the next generation bump (§8.3 boundary). Recorded pointers are only
   ever compared, never dereferenced.

**Runner dispatch site.** `executeOneStep()`'s `PTP_REM` arm — package-owned
[VERIFIED: packages/forth-core/programming/lblGtoXeq.c:838-863] — gains an
`ITM_FORTH` case, modeled byte-for-byte on the `ITM_42STRING` case:

```c
// packages/forth-core/programming/lblGtoXeq.c, inside case PTP_REM:
else if(op == ITM_FORTH) {
  if(*step++ == STRING_LABEL_VARIABLE) {
    if(*step != 0) {              // len > 0: source step
      forthProgramStep(step);     // §3.3.2; step -> [len][bytes...]
    }                             // len == 0: marker — run-time no-op
  }
  return 1;
}
```

- **Halting on error:** `forthProgramStep` reports through
  `displayCalcErrorMessage`/`lastErrorCode` (§3.3, §8.7). `runProgram()` checks
  `lastErrorCode` after every step and breaks *without advancing*
  [VERIFIED: packages/forth-core/programming/lblGtoXeq.c:925-947], so the
  program halts showing the offending `ITM_FORTH` step. `return 1` is
  correct for the success path (advance one step), same as `42STRING`.
- **`:`-lines are stack-neutral.** In compile state the outer interpreter
  only emits tokens — prims/colon-calls/numbers all take the `emit()` branch
  [VERIFIED: packages/forth-core/forth_compile.c:250-254, 271-275, 156-158];
  nothing touches the C47 stack except an immediate primitive (none carry
  `FF_IMMEDIATE` in stage C) or an error path. Any other line executes
  against the shared C47 stack exactly as the interactive REPL does.
- **SST parity.** Single-stepping executes one whole source line per SST
  press — one `executeOneStep()` call [VERIFIED:
  packages/forth-core/programming/lblGtoXeq.c:925 — one call per loop
  iteration; `runProgram(true, …)` breaks after it at :974-976]. No special
  SST handling: the step is self-identifying.
- **Interactive dispatch unchanged.** Outside a program, `ITM_FORTH` still
  funnels to `fnForthOuter` (X-register REPL) via
  `runFunction()`/`reallyRunFunction()`; the PTP class is not consulted on that
  path [VERIFIED: §0.4 chain; packages/forth-core/forth_compile.c:344-363].
  In PEM the item never reaches dispatch — `insertStepInProgram` routes it
  first (§8.4).

### 8.3 Program-scoped dictionary lifecycle

> **Status (2026-07-17).** Stage F1 landed (commits `1834901d3`..`04006089f`;
> §10.1 and DESIGN-HISTORY hold the stage record). This section now describes
> the **landed mechanism**. The pre-F1 interim scheme (generation-equality as
> truth, two scattered bump sites, fixed 8-slot scan array) survives only in
> git history and DESIGN-HISTORY.

A run that executes any `ITM_FORTH` source step gets a fresh dictionary
lifetime, so runs are deterministic, redefinition is clean, and words do not
accumulate across runs. The landed mechanism (F1-1/F1-2/F1-3):

- **Truth is a Forth-private pending-reset event** (`forthResetPending`,
  forth_compile.c). The 16-bit generation counters survive as diagnostics
  only and are never compared for correctness — the R4-E2 wrap alias is
  structurally impossible (executable proof: `test_pending_reset_lifetime`
  subcase 1, 65,536 bumps).
- **One production signal site**: the top of `runProgram()`
  (programming/lblGtoXeq.c), gated `!nestedEngine` and nothing else. Every
  non-nested engine entry — interactive `XEQ`, R/S start, **R/S resume**,
  run-mode **SST**, programmable-menu start, solver-driven start — requests
  a fresh lifetime. `forthRunGenBump()` itself additionally defers the
  request while a `forthInner` frame is active (`forthInnerIsActive()`,
  F1-1): a launch made from a live Forth frame belongs to that lifetime.
- **Consumption at the first safe Forth program-step entry**
  (`forthRunGenCheckReset` inside `forthProgramStep`): clears the dictionary
  and the first-touch scan state, samples the diagnostic counters, and
  consumes the event. Consumption is deferred while a Forth frame is
  active; the event stays pending.
- **First-touch pre-scan tracking** (§8.2) lives in 8-byte records inside
  the dictionary region itself (F1-3): capacity failure is ordinary
  dictionary exhaustion, never a program-count cliff, and records die with
  the region (clear / init / restore seams).

Consequences, all deliberate (R4 lifetime rulings 1-4):

- `XEQ 'PRG'` / menu / solver start → the first `ITM_FORTH` step of the run
  sees a fresh dictionary. Deterministic.
- `STOP` mid-program, then R/S → the **resume is a fresh lifetime**,
  superseding the pre-F1 "resume keeps the generation". The first-touch
  pre-scan re-derives every program-defined word, so a self-contained
  program resumes correctly; only words defined *interactively during the
  pause* are dropped — the standing "programs are self-contained" rule.
- `GTO`-then-R/S cold starts no longer inherit stale generations.
- Run-mode SST is a fresh lifetime (R4 lifetime ruling 3); the pre-F1
  `!singleStep` exclusion is retired.
- Interactive REPL definitions share the dictionary and are wiped at the
  next lifetime's consumption point — programs are **self-contained** in
  this stage: a program that uses a Forth word defines it. *(Under F3's
  accepted scopes, interactive definitions get a reserved interactive-local
  scope instead — §10.3.)*

### 8.4 Entry-only toggle — no runtime flag, keypad state derived

**What the toggle is.** `ITM_FORTH`, pressed in PEM, is an entry-mode
toggle. It commits a **marker step** (§8.1) and, when opening a region,
drops the keypad into Forth text capture. It sets **no runtime flag**:
committed steps are self-identifying, so the runner (§8.2), SST, the
program scanner and branch logic never ask "am I in Forth mode?". The only
transient state is the same state ordinary alpha capture already uses
(`FLAG_ALPHA`, `aimBuffer`, `tam.function`) — cleared when capture closes,
exactly as `REM` entry behaves today [VERIFIED REM model:
src/c47/programming/manage.c:1386-1399 → pemAlpha:773-966].

**The debt-free invariant (normative).** When the cursor lands on an
existing step, the keypad's Forth-vs-RPN behavior for *new forward entry* is
derived from **that step**, never from a persisted flag:

```c
// forth_bridge.c
bool forthEntryStateAtCursor(void) {
  if (pemCursorIsZerothStep) return false;              // top of program: RPN
  const uint8_t *S = currentStep;
  if (!checkOpCodeOfStep((uint8_t *)S, ITM_FORTH)) return false;  // RPN step: RPN
  uint8_t len = forthStepPayloadLen(S);                 // byte S[3] (§8.1)
  if (len > 0) return true;                             // source step: Forth
  return forthMarkerTurnsOn(S);                         // marker: » = Forth, « = RPN
}
```

- Land on an RPN step → RPN keypad, even between markers (self-identifying
  steps make mid-region RPN insertion legal; the runner does not care).
- Land on a Forth source step → Forth keypad (the next line continues the
  region).
- Land on a marker → the direction that marker itself establishes: after an
  opening (`»FORTH`) occurrence, Forth; after a closing (`FORTH«`) one, RPN.
- Power-off anywhere, scroll away and back, insert into the middle — the
  answer is recomputed from the program bytes at the cursor every time.
  There is nothing to go stale.

`forthMarkerTurnsOn(step)` (forth_bridge.c) computes occurrence parity by a
left-to-right walk: find the owning program's start (largest
`programList[i].instructionPointer ≤ step` — `programList` is rebuilt by
`scanLabelsAndPrograms()` on every edit [VERIFIED:
src/c47/programming/manage.c:730, 102-160]), then walk `findNextStep` from
there to `step`, counting `ITM_FORTH` steps with `len == 0`; the occurrence
is *opening* iff the count of markers strictly before it is even. Cost is
one program walk per call — the same order as `scanLabelsAndPrograms()`,
which upstream already runs on every single step insertion [VERIFIED:
src/c47/programming/manage.c:730]. Unbalanced regions (odd marker count to
end-of-program) are legal; the region simply extends to the program's end.

**Entry routing (manage.c override). Exact edits:**

E0. *Ordering precondition — the arm must be reachable at all.*
    `insertStepInProgram`'s first arm is
    `if(func == ITM_AIM || (!tam.mode && getSystemFlag(FLAG_ALPHA)))`, and it
    sets `tam.function = ITM_LITERAL` unconditionally [VERIFIED:
    src/c47/programming/manage.c:1376-1383 — inherited verbatim from upstream].
    Once capture is open `FLAG_ALPHA` is set, so **every** subsequent key —
    including ENTER and a second `ITM_FORTH` press — enters through that arm and
    `tam.function == ITM_FORTH` is destroyed before the Forth arm is reached.
    That single line silently disables rule E3, `forthPickerGuard` (§8.6) and the
    §8.5 transient-capture exception, and swallows the toggle-off gesture.
    **Required:** exclude `func == ITM_FORTH` from that condition, and preserve
    `tam.function` when it already holds `ITM_FORTH` rather than forcing
    `ITM_LITERAL`. Anchor the edit with a comment; it is an upstream-inherited
    line and reads correct in isolation.

E1. *Toggle arm* in `insertStepInProgram`, inserted after the `ITM_REM` arm
    (manage.c:1386-1399) and modeled on it — **except** for the catalog
    teardown, where the REM model is actively wrong:

```c
else if(func == ITM_FORTH) {
  if(aimBuffer[0] != 0 && !getSystemFlag(FLAG_ALPHA)) {
    pemCloseNumberInput(); aimBuffer[0] = 0;            // as the REM arm
  }
  if(catalog) {                                         // NOT the REM arm's shallow
    leaveAsmMode();                                     //   teardown — see below.
    // Bounded: popSoftmenu() can re-push HOME, so never spin on the predicate.
    for(int i = 0; i < SOFTMENU_STACK_SIZE
                   && (_forthCatalogMenuOnTop() || _forthCatalogBuriedOnStack());
        i++) {
      popSoftmenu();
    }
  }
  bool_t wasOn = forthEntryStateAtInsertion();          // derive from the step
                                                        // BEFORE currentStep
  tmpString[0] = (ITM_FORTH >> 8) | 0x80;               // commit the marker:
  tmpString[1] =  ITM_FORTH       & 0xff;               // 0x8B 0x1A 0xFD 0x00
  tmpString[2] = (char)STRING_LABEL_VARIABLE;
  tmpString[3] = 0;
  _insertInProgram((uint8_t *)tmpString, 4);
  if(!wasOn) {                                          // opening »FORTH:
    tam.function = ITM_FORTH;                           // enter Forth capture
    pemAlpha(ITM_FORTH);                                // opens the line placeholder;
  } else {                                              //   ITM_FORTH is not an
    clearSystemFlag(FLAG_ALPHA);                        //   addItemToBuffer item, so
    tam.function = 0;                                   //   no character is fed.
  }                                                      // Closing also clears
  pemCursorIsZerothStep = false;                         // tam.function (see below).
  return;
}
```

    **Why the closing arm clears `tam.function` too (R2 finding 5, ruled).**
    The transient alpha state this section's introduction promises is cleared
    on close is `FLAG_ALPHA`, `aimBuffer`, and `tam.function` together; the
    closing arm originally cleared only `FLAG_ALPHA`. Confirmed live, not just
    theoretical: probed a normal open-then-close, then a completely unrelated
    plain alpha capture at a structurally non-Forth location. The stale
    `tam.function == ITM_FORTH` survived the close and was then inherited by
    that unrelated capture, because `insertStepInProgram`'s `func == ITM_AIM`
    arm only assigns `tam.function = ITM_LITERAL` when
    `tam.function != ITM_FORTH` — the guard reads the stale value as "still in
    a Forth capture" and skips the assignment. That mislabeling then misroutes
    E7's cursor-offset math, which is keyed on `tam.function`, not the step's
    real type.

    **Why the full teardown, and not the REM arm's single `popSoftmenu()`.**
    `pemAlpha` pushes `-MNU_ALPHA` (manage.c:844), and control then returns to
    `keyboard.c`, which calls `_closeCatalog()` *after* `runFunction` [VERIFIED:
    packages/forth-core/keyboard.c:1213, 1216]. `_closeCatalog` scans the whole
    stack for `-MNU_CATALOG`; the REM-style single pop removed only `MNU_FCNS`,
    so the CAT menu is still there, `inCatalog` is true, and
    `closeAllCatalogMenus()` pops the **current** menu if it appears in
    `CatalogMenus[]` — which lists `MNU_ALPHA` [VERIFIED:
    packages/forth-core/keyboard.c:443, 466-475]. The freshly-pushed alpha menu
    is eaten and the user is left in alpha *input* with the *catalog* displayed.
    Popping the catalog stack to empty before `pemAlpha` makes `_closeCatalog`
    a no-op. The drain must use the same **stack-wide** predicate
    `_closeCatalog` uses (`_forthCatalogBuriedOnStack`), not a top-of-stack
    test — a `MNU_CATALOG` buried under a non-catalog menu still costs the
    alpha menu [VERIFIED: packages/forth-core/programming/manage.c,
    `_forthCatalogBuriedOnStack`/`_forthCatalogMenuOnTop` helpers]. Do **not**
    instead remove `MNU_ALPHA` from `CatalogMenus[]`: that array exists to
    serve the ordinary CAT→ALPHA path and removing it regresses that.

    The REM arm shipped with the same latent flaw; upstream has since fixed
    it **itself** (b8f79e486, `fix-REM-in-PEM-from-FCNS-menu`) with a
    shallower two-pop teardown (drop FCNS, then CAT if it is directly on
    top). Ours stays the stack-wide drain — the FORTH arm must also survive a
    catalog buried under a non-catalog menu — and the two fixes coexist in
    different arms. Do not unify them in either direction.

E2. *In-region capture route*, inserted immediately before the
    `addItemToBuffer`/number check (manage.c:1411):

```c
if(!tam.mode && !getSystemFlag(FLAG_ALPHA) && aimBuffer[0] == 0
   && indexOfItems[func].func == addItemToBuffer
   && forthEntryStateAtInsertion()) {
  tam.function = ITM_FORTH;
  pemAlpha(func);            // opens a new source-line capture, feeds first key
  pemCursorIsZerothStep = false;
  return;
}
```

    **`AtInsertion`, not `AtCursor` (R2 finding 3, ruled).** This route derives
    from the step immediately BEFORE the insertion point, which is what
    `forthEntryStateAtInsertion()` computes — production has always called it
    here, not `forthEntryStateAtCursor()`. `forthEntryStateAtCursor()` answers a
    different, landing-on-an-existing-step question (§8.4 intro, "the debt-free
    invariant"); it has no production caller today. They are not
    interchangeable — do not rename one to the other by symmetry — and §8.9
    item 2's mutation below must target the function this route actually calls.

    Only printable items (`func == addItemToBuffer` — digits included) open
    a capture; every other key (navigation, ENTER, function keys, R/S)
    keeps its normal PEM meaning. That is the precise sense in which the
    toggle "flips the keypad": text keys type Forth source, everything else
    is unchanged; a function keystroke in-region records an ordinary RPN
    step (legal — see the invariant above).

E3. *Empty-commit rule* in `pemAlpha`: when capture ends (`ITM_ENTER` arm,
    manage.c:882-888, and the `fnSst`/`fnBst`-triggered
    `pemCloseAlphaInput` path) with `tam.function == ITM_FORTH` **and**
    `aimBuffer[0] == 0`, delete the placeholder step (the exact deletion the
    empty-`ITM_BACKSPACE` arm already performs, manage.c:861-868) instead of
    committing. Rationale: an empty committed line would be byte-identical
    to a marker and flip every subsequent occurrence's parity (§8.1).

E4. *Capture machinery — two different sources of opcode truth.* Both paths
    are generic for `func >= 128` (they emit the two-byte form
    `[(op >> 8) | 0x80][op & 0xff]`), but they do **not** read the opcode from
    the same place, and the difference is normative:

    - *Placeholder insert* keys on **`tam.function`**
      [VERIFIED: src/c47/programming/manage.c:826-838].
    - *Per-key re-insert* keys on **the step's own opcode**, decoded from
      `currentStep[0..1]` — it never consults `tam.function`
      [VERIFIED: src/c47/programming/manage.c:938-943 reads it, :953-959
      re-emits it].

    So `tam.function` decides the opcode exactly once, when the placeholder is
    written; from then on the step carries its own truth and re-insert is
    self-sufficient. Two consequences: setting `tam.function` alone **cannot**
    open a valid high-opcode capture — without a placeholder step to read back,
    there is no opcode for re-insert to find — and E0's "stop `tam.function`
    being clobbered" requirement therefore binds only on the placeholder write,
    not on the per-key path.

E5. *The multi-line lock — ENTER stays in capture.* In `pemAlpha`'s `ITM_ENTER`
    arm (manage.c:912-918), after `pemCloseAlphaInput()` commits a **non-empty**
    Forth line, re-derive `forthEntryStateAtInsertion()` and, if true, re-open a
    Forth capture on the new line: `tam.function = ITM_FORTH; pemAlpha(0);`,
    leaving `FLAG_ALPHA` set and the ALPHA menu up. The editor stays locked in
    the region across lines; ENTER simply drops to the next one.

    This **respects the derived-state invariant** — the state is recomputed from
    the program bytes at the cursor, not stored. No flag, no `tam` field.

    *Why the lazy model could not work.* The original design left re-entry to E2,
    firing on the next printable key. But `FLAG_ALPHA` is what selects the alpha
    keyboard layout [VERIFIED: packages/forth-core/keyboard.c:1698-1702]; with it
    cleared, letter keys produce `ITM_SIN` etc., never `ITM_A`. Only digits could
    ever satisfy E2's `addItemToBuffer` gate, so no keystroke re-opened a Forth
    *text* line and the region was unreachable after the first ENTER.

    *ENTER on an empty capture line is a no-op.* There is nothing to commit, and
    an empty source line is unrepresentable by construction (§8.1 — `len == 0` is
    the marker encoding). Note that `ENTER`-the-instruction is not lost: it is a
    `CAT_FNCT | PTP_NONE` item, so it is reachable as an ordinary Forth word
    (`3 ENTER 4 +`) via §4.1. The ENTER *key* is therefore free to mean newline.

E6. *Re-entering capture from the RPN keypad — the ALPHA gesture.*
    `ITM_AIM` currently opens a string-literal capture unconditionally
    (manage.c:1421-1430 sets `tam.function = ITM_LITERAL`). When the cursor is
    inside a Forth region it must open a **Forth** capture instead: consult
    `forthEntryStateAtInsertion()` and set `tam.function = ITM_FORTH` when true.

    This is what makes EXIT's middle level survivable (§8.4 EXIT ladder): you
    drop the alpha keypad to reach a function key, press it, then press ALPHA to
    resume typing Forth. Without it, dropping the keypad inside a region is
    one-way.

E7. *Editing an existing line.* `pemAlphaEdit` gains `|| func == ITM_FORTH`
    [VERIFIED gate today keys on `ITM_LITERAL || ITM_REM`:
    src/c47/programming/manage.c:994], and the `pemAlpha(ITM_EDIT)` extraction
    arm gains an `ITM_FORTH` case. Because §8.5 renders source lines **bare**,
    the decoded form has no prefix and no quotes, so the extraction is
    `xcopy(aimBuffer, tmpString, ll); aimBuffer[ll] = 0;` — offset **0**, not the
    REM case's 6.

    The fnPem cursor-offset hack **does need a Forth branch — keyed on
    `tam.function == ITM_FORTH`, never on rendered text** (R3-1, landed;
    supersedes this section's earlier claim that the default was already
    correct, which was empirically false). The shared cursor-insert path
    below the offset selection unconditionally adds 2 for the two-byte
    opening quote that quoted renders carry; a bare Forth render has none, so
    without compensation the cursor lands two bytes into the payload (or
    past the NUL at end of line). The landed branch assigns
    `cursorInString = T_cursorPos - 2` so the downstream `+2` cancels
    [VERIFIED: packages/forth-core/programming/manage.c, fnPem
    `tam.function == ITM_FORTH` branch]. The *old* `"FORTH "` string-compare
    branch from the pre-bare-render revision remains wrong and must not be
    resurrected — it keyed on display text and compensated in the wrong
    direction. The empty open placeholder renders as an empty string during
    live capture, not `FORTH ''` (decode.c transient-capture arm, §8.5).

    Markers are **not** editable this way (`len == 0` has no text); EDIT on a
    marker is a no-op.

E8. *The EXIT ladder.* EXIT unwinds exactly one level per press:

    | state | EXIT does |
    |---|---|
    | an alpha submenu or `MNU_FORTH` (FWRD) is current | pop back to the ALPHA menu |
    | ALPHA menu current, capture open | drop the alpha keypad — **region stays open, cursor unmoved, no marker written** |
    | capture closed, still in PEM | leave PEM |

    The first row requires `MNU_FORTH` in `isAlphaSubmenu` — **landed**
    [VERIFIED: packages/forth-core/softmenus.c, `isAlphaSubmenu` gains
    `-MNU_FORTH` disjunct]; without it, EXIT from the FWRD picker would fall
    through to `pemAlpha(ITM_BACKSPACE)` and destroy the capture.

    The middle row is why capture is not sticky-forever. Inside a region the
    alpha layout is live, so function keys are unreachable — and SST/BST are too,
    since AIM remaps f-shift to CAPS/NUM lock [VERIFIED:
    src/c47/assign.c:32 — key 61's `fShiftedAim` is `CHR_caseDN`, not `ITM_SST`].
    Without a level that drops the keypad, a Forth region would be a roach motel.
    §8.4's invariant already permits RPN steps mid-region; this is the gesture
    that reaches them.

E9. *Entry-time validation (landed, stage F5 — F5-1 `ba304a3cf` check
    mode, F5-2 commit gate; per the accepted D ruling).* RPN's strongest
    property is that a malformed step
    cannot be entered — you select from a menu. Forth entry gets the nearest
    equivalent, in two tiers with **different strengths**:

    - **Lexical/structural malformation is a hard, atomic reject** (accepted
      D ruling, supersedes this section's earlier blanket "advisory, must not
      block" wording): malformed numbers, malformed quote/colon parameter
      syntax, broken `:`/`;` structure, overlong tokens — the commit is
      rejected atomically and the prior step is preserved unchanged.
    - **Name resolution stays advisory and never blocks**: §8.2's pre-scan
      makes a forward reference to a `: NAME …` written *later* legal, so an
      unresolved name is not necessarily wrong. §8.6's picker already scans
      authored `: NAME` in the region and supplies the "plausibly coming" set.

    Check-only means for both tiers: no dictionary mutation, no execution, no
    `FTOK_*` emission, no stack/catalog/program state change. Final
    resolution remains the pre-scan/run's job. **Nothing of E9 is implemented
    today** — no check-only mode exists in `forth_compile.c`; it lands as
    stage F5 with its own prompt, traced grammar, and tests.

**Explicit non-mechanism (guardrail):** there is no `FLAG_FORTHMODE`, no
field in `tam`, no persisted byte anywhere recording "entry is in Forth
mode". If a future edit appears to need one, STOP: it violates this
section's invariant and must come back as a design change.

### 8.5 Symmetric display — `»FORTH` / `FORTH«` at render time

The same single item renders directionally, computed from scan parity at
display time — never stored as two items and never cached:

- Glyphs exist in the standard font: `STD_RIGHT_DOUBLE_ANGLE` = `"\x80\xbb"`
  (»), `STD_LEFT_DOUBLE_ANGLE` = `"\x80\xab"` («) [VERIFIED:
  src/c47/fonts.h:156, 150]. Tokens: `»FORTH` = `STD_RIGHT_DOUBLE_ANGLE
  "FORTH"` (7 bytes, 6 glyphs); `FORTH«` = `"FORTH" STD_LEFT_DOUBLE_ANGLE`.
  Both fit every surface (item name fields are `char[16]`, §0.1; listing
  lines wrap at 337 px [VERIFIED: src/c47/programming/manage.c:604]).
- **Render site:** `decodeRem` (decode.c override, P-H4). For
  `op == ITM_FORTH && len == 0`, write the marker token chosen by
  `forthMarkerTurnsOn(step)` (§8.4) into `tmpString`. For `len > 0`, write the
  payload text **bare** — no item-name prefix, no quotes — instead of the
  generic `decodeRem` form `FORTH '…source…'` [VERIFIED generic path:
  src/c47/programming/decode.c:828-843].

  **Why bare, and why not quotes.** A Forth source line that reads `SIN` must
  render exactly like an RPN `SIN` step, because under §4.1 it *does the same
  thing* — that is the extension principle at the display layer. Where Forth
  genuinely differs (`: SQ DUP * ;`, multi-token lines) it renders differently
  because it *is* different: no RPN step is more than one operation. Quoting
  the payload (`'…source…'`) was considered and rejected — a string-literal
  step already renders `'text'` *with* quotes [VERIFIED:
  src/c47/programming/decode.c:707-713], so quotes are precisely what would
  make a Forth line indistinguishable from a literal. Bare collides with
  nothing. Because `decodeOneStep` is the
  single decode funnel, the PEM listing, SST/trace display and the program
  browser all show the same tokens [VERIFIED callers:
  src/c47/programming/manage.c:565, 785; src/c47/programming/nextStep.c:364;
  src/c47/programming/decode.c:42, 65, 112].

  **The listing is deliberately contextual, not injective (RULED 2026-07-15,
  R3-A1).** A single-token Forth line reading `SIN` displays exactly like the
  RPN step `SIN` — and under §4.1 it *does the same thing*, which is the
  point. The `»FORTH`/`FORTH«` markers are the type cue, and in a seven-line
  window they may be scrolled off-screen; that ambiguity is accepted. No
  per-line visual tag is added.
- *Transient during capture:* the open placeholder is byte-identical to a
  marker until the first key lands (E4). The decode arm therefore renders
  `len == 0` as an (empty) source line, not as a marker, when
  `step == currentStep && getSystemFlag(FLAG_ALPHA) && tam.function ==
  ITM_FORTH` — all globals visible to decode.c. E3 guarantees no empty line
  survives capture, so the exception is display-only.
- The renderer *can* see the running state cheaply (one program walk per
  marker rendered, ≤ a handful per screen, §8.4 cost note) — so the
  documented fallback (two display-distinct items) is **not needed** and
  must not be built.
- Text export (`decodeOneStep_XPORT` / MODE_RTF) inherits the same
  rendering; import-side handling is unverified — see §8.10 open item 3.

### 8.6 Name discovery — the in-region `: NAME` picker

The entry-time analog of `scanLabelsAndPrograms()` → `MNU_PROG` [VERIFIED
model: src/c47/programming/manage.c:102 (scan);
src/c47/softmenus.c:1673-1704 (menu build)]. While Forth capture is active,
previously *authored* words in this program are pickable — no compilation,
no catalog, no dictionary lookup:

- **Menu id:** `MNU_FORTH` = item 213 (§0.1). Registration requires all
  three upstream pieces, in matching order (upstream's own comment:
  softmenus.c:1021-1028): a `softmenu[]` row and a `dynamicSoftmenu[]` row
  appended to the dynamic area [VERIFIED: src/c47/softmenus.c:1017-1029,
  1211-1234 — all 22 current slots occupied], and
  `NUMBER_OF_DYNAMIC_SOFTMENUS` 22 → 23 (defines.h:1429, header override
  P-H6 — the procedure upstream's comment explicitly prescribes).
- **Content build** (`initVariableSoftmenu` case `MNU_FORTH`, P-H5): walk
  `findNextStep` from the owning program's start to `currentStep`
  (inclusive); for each `ITM_FORTH` step with `len > 0`, scan the payload
  text with the §3.3.3 glyph-wise tokenizer for the pattern
  `":" <name>` (token `:` followed by a name token — mid-line occurrences
  after `;` included); collect names into fixed 15-byte slots in
  `tmpString`, qsort, pack — the exact `MNU_PROG` builder pattern
  [VERIFIED: src/c47/softmenus.c:1675-1698]. **Names longer than 14 bytes
  are omitted** (not truncated — a truncated pick would insert a *wrong*
  name; deliberate deviation from `MNU_PROG`, which truncates labels).
  Duplicates (redefinitions) collapse to one entry.
- **Item-count cap (R2 finding 6, ruled): `TMP_STR_LENGTH / 15` = 170
  names.** `tmpString` is a fixed, shared global buffer; accepting names
  without a count cap can write past it. Policy is **truncate by scan
  order**: once 170 unique names are collected, further `:` names stop
  being recorded, but the tokenizer keeps running normally — nothing else
  in the scan depends on the count, and the cap must not desync token
  position within the line. Sorting applies only to the names actually
  collected, so the omitted names are whichever were encountered *last* in
  program order, not necessarily the alphabetically-last ones. No error UI:
  this is ordinary single-user robustness (a reboot or lost-edit risk), not
  a reportable condition, and 170 unique word definitions before the cursor
  is far beyond any realistic personal program.
- **Scan bound (documented deviation):** the builder walks at most **1000
  steps** (`FORTH_PICKER_MAX_SCAN_STEPS`) from the owning program's start,
  found via `forthOwningProgramStart(currentStep)` per the R4-E5 ruling
  [VERIFIED: packages/forth-core/forth_menu.c:97,104]. A program longer than
  that is not fully scanned, so definitions past step 1000 do not appear in
  the picker; they still compile and run normally (§8.2). The cut-off and
  its literal are pinned by the G2 scan-cut-off test.
- **Registration note (softmenu numbering):** `-MNU_FORTH`'s `softmenu[]` row
  sits at the end of the *dynamic area* (slot 022) — mid-table overall,
  deliberately deviating from upstream's "add new menus at the end" rule
  (which serves its Wiki numbering) because the dynamic-area rows must stay
  contiguous and position-matched with `dynamicSoftmenu[]`. Every static menu
  index ≥ 022 is therefore shifted by one against upstream's Wiki numbering.
  Benign at runtime; re-check on every upstream merge that adds a menu
  (upstream added its static `MNU_TAMLOCALLABEL` at the table end, slot 185,
  in b8f79e486 — no collision).
- **No validation at entry.** The scan is text-only; picking a name never
  resolves it against the dictionary or checks reachability. Resolution is
  deferred to run time (§8.2) — a picked name can still fail at run with
  the §8.7 unresolved-word halt (e.g. its definition line was later
  deleted). This is by design.
- **Presentation (reworked, commit 097c7e3bd):** the picker is a static submenu entry — `-MNU_FORTH` occupies a row appended to `menu_ALPHA` (softmenus.c override), so during any alpha capture the user opens FWRD from the alpha menu; EXIT pops back. The earlier design (push `-MNU_FORTH` on top of `-MNU_ALPHA` at capture open) is superseded — no `showSoftmenu(-MNU_FORTH)` call exists. Verified by `test_alpha_menu_on_top_during_capture` / `test_alpha_menu_contains_fwrd`.
- **Refresh:** `fnOpenMenu` rebuilds on open [VERIFIED:
  src/c47/softmenus.c:1261-1270], but the display path caches by menu id and
  rebuilds only `-MNU_DYNAMIC` unconditionally [VERIFIED:
  src/c47/softmenus.c:3039-3042]. `MNU_FORTH` must join that rebuild-always
  disjunction (`|| softmenu[m].menuItem == -MNU_FORTH`), or a word defined
  on the previous line would not appear while the menu stays displayed —
  this is the acceptance-critical line (§8.9 item 3).
- **Pick action** (keyboard.c override, P-H7): during Forth capture, a
  `MNU_FORTH` softkey press inserts `dynmenuGetLabel(dynamicMenuItem)` plus
  one trailing space into `aimBuffer` at `T_cursorPos`, using the same
  insert-at-cursor idiom as `pemAlpha`'s `addItemToBuffer` arm [VERIFIED
  idioms: src/c47/keyboard.c:1153-1156 (dynmenuGetLabel→aimBuffer);
  src/c47/programming/manage.c:854-857 (insert at cursor)].

### 8.7 Error surface

All Forth errors surface through the existing C47 protocol at the
`ITM_FORTH` step, with the /mappings unchanged:

- `forthProgramStep` → `forthOuterInterpret()` displays via
  `displayCalcErrorMessage` and sets `lastErrorCode` (§3.3 pseudocode;
  committed paths [VERIFIED: packages/forth-core/forth_compile.c:210-341]).
- `runProgram()` halts *at* the step, cursor on it, because the step pointer
  only advances when `lastErrorCode == ERROR_NONE` [VERIFIED:
  packages/forth-core/programming/lblGtoXeq.c:925-947].
- Unknown word: `ERROR_FUNCTION_NOT_FOUND` with the offending token (and
  `(in WORD)` when a definition was open) in `errorMessage` [VERIFIED:
  packages/forth-core/forth_dict.c:1079, 1103]. The on-screen line shows
  the generic message text only; the token reaches PC diagnostics
  (`EXTRA_INFO_ON_CALC_ERROR`) and the self-tests, which assert on the
  buffer. An earlier package extension concatenated the token into the
  displayed line — that was dropped in S1 (owner ruling: not worth two
  upstream overrides for it), which is why `error.c` is no longer an
  override and `screen.c` carries no display change.
- Malformed definition (nested `:`, stray `;`, `:` with no name, unterminated
  line): `ERROR_INVALID_NAME` + `abortDefinition()` (; [VERIFIED:
  packages/forth-core/forth_compile.c:212-244, 333-336]) — the smudged
  entry never leaks (§3.3.7).
- Marker steps cannot error (run-time no-op, §8.2).

### 8.8 Naming & width

- `ITM_FORTH` stays the internal C identifier; no new executable item ids.
- Catalog/soft-menu label of the toggle item: `"FORTH"` (5 glyphs — fits the
  `char[16]` fields and the 14-byte dynamic-menu display slot [VERIFIED cap:
  src/c47/softmenus.c:1680-1682]).
- Listing tokens: `»FORTH` / `FORTH«` (§8.5) for markers; source lines render
  **bare** — no item-name prefix, no quotes (§8.5, E7, R2 finding 2, ruled).
  A Forth `SIN` displays like the RPN `SIN` it extends. Not the generic
  `FORTH '…'` form other REM-family payloads use.
- Picker menu label: `"FWRD"` (`MNU_FORTH` row, §0.1). Picker entries: word
  names ≤ 14 bytes (§8.6).

### 8.9 Acceptance — target suite; per-item status below (RULED 2026-07-15, Q1)

> **Coverage status: COMPLETE (stage F1.5, closed 2026-07-18).** Every item
> is covered end-to-end through the real engine paths, pinning the landed
> F1 semantics (§8.3): 1/7/9 by `test_accept_run_lifecycle` (`b773597bd`),
> 2(a-d) by `test_accept_entry_state_roundtrip` (`5a9e9ce2d`), 4 by the
> F15-3 display-parity test against the real `lcd_buffer` (`c8b87dfa8`),
> 5/6 by `test_accept_glyph_type_parity` (`6775252bf`), 10 by
> `test_accept_xeq_name_step` (`546aa8b6c`); 3 and 8 were already fully
> covered at unit level (`test_picker_rebuilds_same_menu`, marker no-op +
> empty-ENTER). A green gate now certifies the end-to-end contracts. Three
> of the mutations stated in the items below were falsified during
> execution and replaced (items 5, 9(b), 10 — see the per-item notes and
> DESIGN-HISTORY); the landed tests are normative over the item prose.

1. **Define-and-use in one program.** Program: `»FORTH`, `: SQ DUP * ;`,
   `3 SQ`, `FORTH«`, `END`. `XEQ` it → `X == 9`, `dtLongInteger`.
   *Mutation:* drop the `forthProgramStep` call from the §8.2 arm (make
   source steps no-ops) — X stays unchanged and the test fails.
2. **Derived keypad state.** (a) Cursor on an RPN step → digit key opens
   number entry (RPN); (b) cursor on a Forth source step → digit key opens
   Forth capture and types text; (c) cursor on `»FORTH` → capture; on
   `FORTH«` → RPN; (d) power-off mid-region (simulator save/load), land on
   the same step → identical behavior to (a)-(c).
   *Mutation:* replace `forthEntryStateAtInsertion()` (the function E2's route
   actually calls, R2 finding 3) with a static bool set by the toggle —
   case (d) and scroll-away-and-back regress.
3. **Picker sees an uncompiled word.** Author `: SQ DUP * ;` as a source
   step; **without any run**, on the next capture line the `MNU_FORTH`
   picker lists `SQ`; picking inserts `SQ ` into the buffer.
   *Mutation:* remove `MNU_FORTH` from the softmenus.c:3039 rebuild-always
   disjunction — the just-defined word is missing until the menu is
   re-opened, failing the test.
4. **Symmetric marker display.** Two markers around a source line render
   `»FORTH` … `FORTH«`; adding a third marker after them renders `»FORTH`
   again. BST/SST display and the PEM listing agree.
   *Mutation:* invert `forthMarkerTurnsOn`'s parity (odd instead of even) —
   every marker renders with the wrong direction.
5. **Glyph operators in program lines.** A source step authored on the
   alpha keypad containing `×` (`STD_CROSS`) and `÷` (`STD_DIVIDE`)
   compiles: `: D2 2 ÷ ;` runs (`8 D2` → 4). Chain verified in the tree:
   alpha capture stores `itemSoftmenuName` bytes [VERIFIED: the aimBuffer
   append in packages/forth-core/programming/manage.c], the tokenizer
   advances glyph-wise [VERIFIED: `nextToken` (C-6),
   packages/forth-core/forth_compile.c], and the prim table carries the
   alias entries [VERIFIED: packages/forth-core/forth_prims.c].
   *Mutation (replaced 2026-07-18 — the original escaped empirically):*
   deleting/disabling the `PRIM_DIVGL` alias row no longer fails the test:
   since R1-3 landed, §4.1 step 4's item fallback resolves `÷` to the
   native divide item and the program still runs — the resolution
   redundancy is landed design, not a defect. The valid mutation is the
   capture store: switch the manage.c aimBuffer append from
   `itemSoftmenuName` to `itemCatalogName` — glyphs vanish from the
   captured line and the byte-image comparison fails (F15-4, `6775252bf`).
6. **Integer literal type parity.** `7` typed on the RPN keypad and `7`
   executed from a Forth source step both leave `dtLongInteger` in X
   [VERIFIED path: packages/forth-core/forth_inner.c:43-56
   (`forthPushInt32` → `convertLongIntegerToLongIntegerRegister`)].
   *Mutation:* revert `forthPushInt32` to `int32ToReal34` — type probe
   fails.
7. **Forth-meaningful halt.** (a) `»FORTH`, `: SQ DUP *` (no `;`) → run
   halts at that step with `ERROR_INVALID_NAME`, definition aborted (no
   smudged leak: defining `SQ` correctly afterwards works). (b) `3 SQX`
   with `SQX` undefined → halts at that step showing
   `No such function: SQX`.
   *Mutation:* delete the `lastErrorCode == ERROR_NONE` guard in `runProgram`
   [packages/forth-core/programming/lblGtoXeq.c:936] — the program runs past
   the bad step and (a)/(b) fail.
   *Not a mutation:* "return 1 unconditionally from the §8.2 arm ignoring
   `lastErrorCode`" — the arm (lblGtoXeq.c:860-867) **already** returns 1
   unconditionally and never reads `lastErrorCode`. The halt is `runProgram`'s
   separate check, so that edit changes nothing and cannot demonstrate the
   property.
8. **Marker no-op & empty-line rule.** A lone `»FORTH`/`FORTH«` pair with
   nothing between them runs to completion with stack untouched; ENTER on an
   empty capture line commits **no source step** — only the opening marker
   already inserted when capture began remains (R2 finding 4, ruled: total
   step count is `before + 1` for that marker, not unchanged — starting from
   RPN and opening capture adds it before ENTER is ever pressed;
   `test_forth_empty_enter_leaves_no_step` correctly expects the marker to
   remain).
   *Mutation:* drop rule E3 — the empty commit becomes a third marker and
   test 4's parity assertions fail downstream.
9. **Run-scoped lifecycle (F1 semantics, reconciled 2026-07-17).** (a) Run
   the test-1 program twice → second run identical (redefinition clean,
   `fdict.count` after run 2 equals after run 1 — no accumulation). (b)
   Insert `STOP` between the definition and `3 SQ`; run, then R/S → still
   `X == 9`: the resume is a **fresh lifetime** whose first-touch pre-scan
   re-derives `SQ` (§8.3). A word defined interactively during the pause is
   dropped by the resume — assert both halves. *(The pre-F1 wording "resume
   keeps the dictionary" and its `fnRunProgram`-bump mutation are obsolete:
   `fnRunProgram` reaches the sole `runProgram` signal site by design.)*
   *Mutation:* restore the old menu-key gate at the sole signal site
   (`!nestedEngine && menuLabel != INVALID_VARIABLE`) — the pause-defined
   word survives R/S and (b) fails.
10. **XEQ-by-name from RPN records a name.** In PEM, `XEQ` + alpha `SQ`
    (a Forth word) records a name-string `XEQ` step, and the program bytes
    contain the glyphs `SQ` — no `ITM_FCALL` opcode, no index [VERIFIED
    mechanism: §4.2].
    *Mutation (consequence corrected 2026-07-18 — F15-5 execution):*
    re-routing the tam.c PEM branch to `insertStepInProgram(ITM_FCALL)`
    with `tam.value = widx` does NOT put `0x8B 0x1B` in program memory:
    `insertStepInProgram`'s own `ITM_FCALL` arm (programming/manage.c) is
    a SECOND name-faithfulness guard — it resolves the index back to the
    name via `forthDictNameByIndex` and records an `ITM_FORTH` source step
    (or rejects with `ERROR_NON_PROGRAMMABLE_COMMAND` when unresolvable).
    The re-route is still detected: the XEQ-name-step probe goes RED
    (`0x03 0xFD len glyphs` absent). No reachable insertion path can
    record a raw index; the "no `0x8B 0x1B`" probe is defense-in-depth
    documentation, declared redundant on every production path
    (F1-5 annotation convention).

Arena reporting rule (§5.4) applies: the acceptance run must report the
dictionary high-water mark; region ceiling unchanged (≤ 2 KB, §5.4).

### 8.10 Open questions and gaps

Resolved items are not listed here; their history is in `DESIGN-HISTORY.md`.
This list carries only what is genuinely unsettled.

1. **[Deferred — additive] Forth words are invisible to the rest of the UI.**
   RPN programs appear in the PROG catalog and can be `ASSIGN`ed to a key; Forth
   words can do neither (§4.3). Real asymmetry against the extension principle,
   but purely additive and with no format impact.

2. **[Deferred — additive] Interactive `FORTH` requires a string in X.**
   `fnForthOuter` (§3.3.2) raises `ERROR_INVALID_DATA_TYPE_FOR_OP` unless X
   already holds a string, so the same item is an entry-mode toggle in PEM and a
   string-consuming function outside it. Extension-consistent would be: pressing
   FORTH interactively opens the same capture PEM gives you (§8.4), with ENTER
   running the line. Same entry-layer surface as §8.4, so cheapest to build
   alongside it.

**Scoping.** Colon definitions are local to their owning RPN program,
interactive definitions live in a reserved interactive scope, and a name
lookup cannot see another owner's definitions — the dictionary walk is
owner-filtered (F3, §10) [VERIFIED:
packages/forth-core/forth_dict.c:486-493]. RPN's unit
of shared code *is* the labelled program, and `FTOK_XEQN` (§3.3.6) makes any
keystroke program callable from inside a Forth definition. Forth words are
local helpers; RPN programs are the sharing unit. Nothing is durable-at-risk:
the source is the truth (§8.1) and lives in program memory permanently — the
dictionary is a cache the pre-scan rebuilds.

**Not a gap: program text export.** There is no text *import* parser anywhere in
the tree. `fnPExport`/`_exportProgram` [VERIFIED:
src/c47/saveRestorePrograms.c:162, 362] is one-way text export for every step
type, RPN included; the round-trip path is `_saveProgram`/`fnLoadProgram`
[VERIFIED: src/c47/saveRestorePrograms.c:396, 505], a **textual** format —
header lines followed by one decimal byte value per line
(`sprintf(tmpString, "%" PRIu8 "\n", beginOfCurrentProgram[i])`, :444-447) —
through which `ITM_FORTH` steps pass as opaque byte values. The representation
is text; what matters for Forth is only that the payload bytes are never
interpreted and round-trip losslessly. There is nothing for Forth to round-trip through and nothing to
verify.

---

## 10. Stage F — landed architecture (decision record, folded 2026-08-03)

*(§9 intentionally unassigned — pre-2026-07-14 artifacts cite "§9.x"
meaning today's §8.x; reusing the number would collide.)*

The full F series is LANDED (F1..F6 + F1.5, closed 2026-07-20; commit
table in QWEN_RUNBOOK §2). Normative content lives in the main sections;
the decision rationale lives in DESIGN-HISTORY and the per-stage packet
ledgers. Stage summaries:

- **F1 engine lifetime** — pending-reset truth, runProgram as the sole
  lifetime signal, dynamic arena-backed scan tracking (§3.2), compile-only
  RECURSE, restore-time threaded-code validator.
- **F1.5 §8.9 acceptance harness** — the end-to-end battery every later
  stage inherits.
- **F2 shared parameter core** — `param_core.c` extraction, bounded name
  reader, shared direct dispatch (§0.2/§3.3).
- **F3 vocabulary & scopes** — owner-tagged headers, global region,
  filtered lookup, GLOBAL/IMMEDIATE/FORGET, compile-time control flow,
  FTOK_XEQN (§1, §2.2, §4).
- **F4 textual parameters** — canonical spellings, full parameter-form
  grammar, error table (§8).
- **F5 commit validation** — check mode + commit gate implementing §8's E9.
- **F6 capture submode** — real key paths, managed→aimBuffer capture
  (S3), suspend/restore incl. tam snapshot, word catalog (§8.4-8.6).

## 11. Stage D3 — hybrid spill stack (decision record, folded 2026-08-03)

LANDED 2026-08-03 (D3-1..D3-4; DESIGN-HISTORY entries of that date).
Normative content now lives in §3.4 (invocation bracket, boundary rule,
acceptance pins) and §5.7 (spill region, lifetime, line-end contract).
Sanctioned divergence from R47 recorded here: depth beyond the visible
stack, bounded so the visible window and every native's view remain
exactly R47's. Two design errors were caught by the stage's own gates
and amended (four invocation sites, not one; line-end discard made
loud) — the ledger carries both.
