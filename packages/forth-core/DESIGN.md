# forth-core — DESIGN.md (authoritative)

Stage 1 design for embedding a Forth word engine into C47 firmware **without
modifying upstream `.c`/`.h` files**. All integration is delivered as a
`CUSTOM_PKG` package (see `custom_package/README.md`): new sources under
`packages/forth-core/`, plus whole-file *overrides* of a small set of upstream
`.c` files that add the documented hook lines and nothing else.

This document is written to be implemented by a local model with **zero
unstated decisions**. Every struct layout, constant, and stateful routine is
given exactly. Line numbers reference the upstream tree at the commit this was
authored against (`src/c47/...`); the implementer must re-confirm each hook line
still matches the quoted code before editing (upstream is generated in places).

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

Item IDs run `0 .. LAST_ITEM` (`items.h:2970  #define LAST_ITEM 2860`;
re-verified 2026-07-06 — earlier drafts cited 2850 at items.h:2960, which was
doc drift; the don't-grow constraint below is unaffected).
IDs `1..127` encode as **one** program byte; IDs `128..32767` encode as **two**
bytes (see §2). `indexOfItems[]` is defined in `items.c`, which the package
system can override — so we *may* fill spare slots, but we may **not** grow the
array past `LAST_ITEM` (that would desync generated code that assumes the bound).

Two genuinely free slots exist at the tail (`items.c:4690-4691`, `CAT_FREE`):

```
/* 2842 */ { itemToBeCoded, NOPARAM, "2842", "2842", ... CAT_FREE ... PTP_DISABLED ... },
/* 2843 */ { itemToBeCoded, NOPARAM, "2843", "2843", ... CAT_FREE ... PTP_DISABLED ... },
```

**We claim these two.** They become the only two new C47 items Forth needs:

| new item id            | value | role                                                        |
|------------------------|-------|-------------------------------------------------------------|
| `ITM_FORTH`            | 2842  | outer interpreter entry (run/compile a Forth source line)   |
| `ITM_FCALL`            | 2843  | inner-call bridge: **runtime** param (from program bytes / TAM entry) = dictionary index of a colon def. NOTE: the table row's `param` *field* is the TAM mode tag `TM_VALUE`, NOT a dictionary index — see the full rows in §0.2. |

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
- `PTP_STATUS` 0x1E00 — **program parameter type** (drives `executeOneStep`, §3).
  - `ITM_FORTH`  → `PTP_NONE`   (no inline parameter).
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
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_NONE      | HG_ENABLED },
/* 2843 */ { fnForthCall,  TM_VALUE, "FCALL", "FCALL",
             (0 << TAM_MAX_BITS) | 16383,
             CAT_FNCT | SLS_ENABLED | US_ENABLED | EIM_DISABLED | PTP_NUMBER_16 | HG_ENABLED | RESULT_IN_X },
```

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

`findNamedLabel()` (manage.c:1864/1870) linear-scans `labelList[]` and returns
`lbl + FIRST_LABEL`. **This is the seam Forth's outer interpreter reuses for
name lookup** (§4). Label IDs occupy `FIRST_LABEL(2044) .. LAST_LABEL(6999)`
(defines.h:1337-1338) — a namespace *disjoint* from item IDs even though the
integer ranges overlap, because labels only ever appear as the `param` of
`ITM_GTO`/`ITM_XEQ`, never as an opcode.

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

**Index stability rule:** never reorder or delete a primitive; the numeric index
is the on-disk/on-arena token (§2). Retire a primitive by pointing it at a
`fnNotAvailable`-style stub, never by removal.

---

## 2. Token encoding table

Two encodings coexist and must not be confused.

### 2.1 C47 program bytecode (unchanged, upstream)
How a C47 program step names an opcode (`executeOneStep`, lblGtoXeq.c:725-730;
`runProgram`, lblGtoXeq.c:877-879):

```
byte0 < 0x80          : 1-byte opcode, op = byte0                 (items 1..127)
byte0 >= 0x80         : 2-byte opcode, op = ((byte0 & 0x7F)<<8) | byte1   (items 128..32767)
```

Therefore our new items encode in-program as:

| item      | id   | program bytes            |
|-----------|------|--------------------------|
| `ITM_FORTH` | 2842 | `0x8B 0x1A`            |
| `ITM_FCALL` | 2843 | `0x8B 0x1B` + param     |

`0x8B = 0x80 | (2842>>8)`, `0x1A = 2842 & 0xFF`; `0x1B = 2843 & 0xFF`.
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
| `0x0001 .. 0x0FFF`  | `FTOK_PRIM` | —                                | call `forthPrims[token - 1].fn` (primitive index = token - 1) |
| `0x1000 .. 0x7EFF`  | `FTOK_CALL` | —                                | call colon def with dictionary index `(token - 0x1000)`    |
| `0x7F00`            | `FTOK_LIT`  | 16 (a real34_t / decQuad)        | push inline real34 literal onto the C47 stack              |
| `0x7F01`            | `FTOK_ILIT` | 4 (int32 LE)                     | push inline integer literal **as a long integer** (`dtLongInteger` — the type keyboard entry produces; §3.3 number-type conformance) |
| `0x7F02`            | `FTOK_BR`   | 2 (int16 signed cell delta)      | unconditional branch                                       |
| `0x7F03`            | `FTOK_0BR`  | 2 (int16 signed cell delta)      | pop X; branch if zero/false                                |
| `0x7F04`            | `FTOK_C47`  | 2 (uint16 item id) + params, padded to 2-byte cells | escape: run a native C47 item via `reallyRunFunction`      |
| `0x7F05 .. 0x7FFF`  | reserved    | —                                | must not be emitted (reserve for control words)            |
| `0x8000 .. 0xFFFF`  | reserved    | —                                | unused; keeps top bit free for a future long-token scheme  |

Notes:
- **Branch deltas are in *tokens* (2-byte cells), signed, relative to the cell
  *after* the delta field.** `THEN`/`ELSE`/loop back-patching writes here.
- `FTOK_C47` is the bridge that lets Forth call the entire C47 command set (e.g.
  `SIN`, `STO 05`). Its inline `params` follow the same per-`PTP` convention the
  C47 VM uses; for stage 1 restrict to `PTP_NONE`/`PTP_NUMBER_8` items and widen
  later. **DECIDED: inline params are padded to a whole 2-byte cell.** A
  `PTP_NUMBER_8` param occupies 2 bytes (value byte + one zero pad byte), so
  `FTOK_C47`/`PTP_NUMBER_8` is always 6 bytes total (token 2 + itemId 2 +
  param 1 + pad 1) and every token and inline datum stays cell-aligned.
  Emitter writes the pad byte as 0; decoder advances ip by 2 after reading the
  param byte. (The current decoder's `ip += 1` and the hand-assembled
  `FTOK_C47` test body predate this and must be changed in the same commit.)
- The dictionary-index space (`FTOK_CALL`) and the primitive-index space
  (`FTOK_PRIM`) are disjoint by construction: primitives ≤ 0x0FFF, colon defs
  offset by 0x1000. `forthPrimCount` MUST stay ≤ 0x0FFF (4095); enforce at
  **compile time** with `_Static_assert(sizeof(forthPrims)/sizeof(forthPrims[0])
  <= 0x0FFF, ...)` in `forth_prims.c` (required change — no assert exists in
  the tree today; see §7 invariants).
- Emit: FTOK_PRIM token = index + 1 (`FTOK_PRIM_BASE` = 1); decode subtracts 1.
  Index 0 must never emit as `0x0000` (= `FTOK_EXIT`).
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

**Open issues (§2.2):**

1. **RESOLVED — FTOK_C47/PTP_NUMBER_8 padding.** The unpadded encoding
   (2+2+1 = 5 bytes) left ip at an odd offset, misaligning subsequent token
   reads. DECIDED: pad the param to a full 2-byte cell — FTOK_C47/PTP_NUMBER_8
   is always 6 bytes (now normative in the token table and notes above). This
   keeps the invariant that every token and inline datum is cell-aligned (as
   FTOK_LIT/ILIT and branch deltas already are); the 1-byte param is the only
   odd-width datum in the encoding. REJECTED: re-aligning ip after dispatch
   (ip=(ip+1)&~1) — makes FTOK_C47 variable-width with hidden state, which
   emit/decode/decompile/save must all special-case. One wasted byte per C47
   escape is worth the uniform encoding. Implementation note: the decoder
   (`forth_inner.c`, currently `ip += 1`) and the hand-assembled FTOK_C47
   self-test body must be updated in the same commit, or the test locks in the
   unpadded encoding. Keep the token fetch as `memcpy` even after padding.

2. **RESOLVED — FTOK_C47 execution context (root cause of the harness hang).**
   *Diagnosis (confirmed):* when `forthInner` calls `reallyRunFunction` with
   `programRunStop != PGM_RUNNING`, dispatch takes the NORMAL-MODE branch
   (items.c:317) and the unconditional `refreshStatusBar()` (items.c:389).
   Both funnel into `force_SBrefresh` (statusBar.c:607) → `lcd_refresh`
   (c47-gtk/hal/lcd.c:93) → — because `FULLUPDATE` is defined for the GTK
   build (defines.h:446) — `refresh_gui` (lcd.c:212):
   `while(gtk_events_pending()) gtk_main_iteration();` — a GTK event pump
   inside the interpreter loop. The pump's only early exit is `ui_is_active`,
   which is true only ~100 ms after a physical UI event (gtkGui.c:5409-5425).
   The self-tests run from the reset path (config.c:1948) with `ui_is_active`
   false, and the queue keeps refilling (periodic `refreshLcd` timer,
   screen.c:482, plus draw events queued by each `LCD_write_line`) → livelock.
   Interactively `ui_is_active` is true and the pump exits at once — hence
   "works by hand, hangs in harness". The interactive branch is additionally
   re-entrancy-unsafe: `gtk_main_iteration` can dispatch a queued keypress
   that runs a C47 function while a Forth word is mid-execution.
   *DECIDED FIX (production):* execute `FTOK_C47` under **program semantics**:
   save `programRunStop`, set `PGM_RUNNING` around the `reallyRunFunction`
   call, and restore the saved value **only if still PGM_RUNNING afterwards**
   (an async R/S stop must not be clobbered). The PGM_RUNNING branch
   (items.c:347-355) skips the hourglass/interactive chain and performs only
   the rate-limited `force_refresh(timed)` — the same exposure normal C47
   programs have. See the FTOK_C47 arm in §3.2.
   *Harness note:* `refresh_gui` early-outs on `headlessMode` (lcd.c:213);
   run the self-test binary headless and the hang disappears independently.
   *Interaction:* PGM_RUNNING semantics make the FTOK_C47 → item dispatch →
   Forth-item re-entry path MORE reachable, so the §3.2 re-entrancy guard is
   a mandatory companion to this fix (same commit series as H1).

---

## 3. Program executor hook (the inner interpreter)

The C47 step VM (`executeOneStep`, lblGtoXeq.c:722) dispatches one item and
returns "steps to advance". It has no concept of threaded code. We add the
inner interpreter as a **new module** invoked when the two Forth items execute;
we do **not** change the outer `runProgram` loop shape.

### 3.1 Dispatch path for `ITM_FCALL` / `ITM_FORTH`
Both items have real `func` pointers in the overridden `indexOfItems[]`, so they
already flow through `executeOneStep` default arm → `runFunction` (lblGtoXeq.c:781)
→ `reallyRunFunction` → `indexOfItems[func].func(param)` (items.c:399). No new
case in `executeOneStep` is required for *execution*. The only executor edits
are the two table entries (§6, hook H1).

- `indexOfItems[ITM_FCALL].func = fnForthCall;` — `param` = dictionary index.
- `indexOfItems[ITM_FORTH].func = fnForthOuter;` — reads source from alpha reg.

### 3.2 The inner interpreter (`forth_inner.c`, pseudocode)
Threaded-code walker. Uses one explicit return stack in package BSS (NOT the C
call stack — depth must be bounded and inspectable).

```
#define FORTH_RSTACK_DEPTH 64            // tune against arena high-water (§5.4)
static uint16_t rstack[FORTH_RSTACK_DEPTH];   // region-relative token offsets
static uint8_t  rsp;
static bool     forthRunning = false;    // re-entrancy guard (see below)

void forthInner(uint16_t entryIndex, bool fromProgram):   // matches forth_dict.h:56
    if forthRunning:                     // nested entry would zero rsp and
        displayCalcErrorMessage(ERROR_RAM_FULL,...)  // destroy the outer word's
        return                           // return stack — refuse, don't recurse
    forthRunning = true                  // cleared on EVERY exit path below
    ip = bodyOffsetOfIndex(entryIndex)   // region-relative byte offset of body[0]
    if ip == FORTH_NULL:                 // bad/stale entry index (e.g. saved program
        displayCalcErrorMessage(ERROR_INVALID_CORRUPTED_DATA,...)  // after CLEAR FORTH)
        forthRunning = false; return     // NEVER a silent no-op — same error as the
                                         // mid-word FTOK_CALL bad-index arm below
    rsp = 0
    loop:
        if pollProgramInterrupt():       // DMCP R/S/EXIT key poll, EVERY dispatch —
            return                       // poll sets programRunStop = PGM_WAITING and
                                         // requests stop; fires for interactive AND
                                         // fromProgram entry ("Cooperative break &
                                         // key poll" below)
        if fromProgram && programRunStop != PGM_RUNNING:   // async stop from inside a
            return                       // dispatched item (e.g. FTOK_C47 ran R/S-
                                         // sensitive code). Gated on fromProgram:
                                         // interactively programRunStop is NOT
                                         // PGM_RUNNING to begin with.
        if ++dispatches >= RUNAWAY_CAP:  // backstop only (§2.2 notes)
            displayCalcErrorMessage(ERROR_RAM_FULL,...); return
        tok = readToken(ip); ip += 2
        if tok == FTOK_EXIT:
            if rsp == 0:
                setSystemFlag(FLAG_ASLIFT)   // C47 convention: result-producing
                forthRunning = false         // items exit with ASLIFT set
                return
            ip = rstack[--rsp]
            continue
        else if tok <= 0x0FFF:                       // FTOK_PRIM
            forthPrims[tok].fn()
            if lastErrorCode != ERROR_NONE: return    // honor C47 error protocol
        else if tok <= 0x7EFF:                       // FTOK_CALL
            if rsp == FORTH_RSTACK_DEPTH:
                displayCalcErrorMessage(ERROR_RAM_FULL,...); return   // deep recursion guard
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
                       // popIsFalse is a TYPE-DISPATCHED zero test (long integer,
                       // real34, complex, ...) like upstream's compareRegisters
                       // machinery (mathematics/compare.c:505 fnXEqualsTo) — NOT a
                       // raw real34IsZero on X's data, which misreads any non-real34
                       // X (e.g. a dtLongInteger left by an FTOK_C47 escape).
            FTOK_C47:  itemId = u16 at ip; ip += 2
                       saved = programRunStop; programRunStop = PGM_RUNNING   // program semantics (§2.2 resolved issue 2)
                       reallyRunFunction(itemId, inlineParam)
                       if programRunStop == PGM_RUNNING: programRunStop = saved   // restore unless an async R/S stop changed it
                       advance ip past params (cell-padded: PTP_NONE +0, PTP_NUMBER_8 +2 — see §2.2)
            default:   displayCalcErrorMessage(ERROR_INVALID_DATA_...); return
```

**Cooperative break & key poll (DECIDED, verified against upstream 2026-07-06):**
the old spec relied on `if programRunStop != PGM_RUNNING && calledFromProgram:
return` at the loop bottom. That check is **insufficient on hardware**: nothing
inside `forthInner` ever mutates `programRunStop`, because upstream's R/S
detection lives exclusively in `runProgram`'s step loop and a whole Forth word
executes as ONE step. `forthInner` must therefore poll the keyboard itself,
using the SAME mechanism and cadence upstream uses:

- *Upstream mechanism (cited):* at the bottom of `runProgram`'s `while(1)`
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
  gate maps to the §3.2 re-entrancy guard: `forthInner` never nests, so it is
  always the innermost — and only — engine that can poll while a word runs).
  When entered `fromProgram`, `runProgram`'s own
  `if(programRunStop != PGM_RUNNING) break` (lblGtoXeq.c:929-931) then stops
  the program after the word returns.
- The runaway dispatch cap is retained purely as a backstop (§2.2 notes); the
  key poll is the primary interrupt. The current code's bare check
  (forth_inner.c:65) is the required-change site.
- PC/GTK builds: no key poll (upstream polls only under `DMCP_BUILD`); the
  existing `programRunStop` check plus the runaway cap remain the PC behavior.

`readToken`/`push`/`pop` operate through the existing C47 stack API
(`liftStack`, `getRegister…`, `setRegister…`) and `TO_PCMEMPTR(fdict base +
offset)`. Reuse C47 error reporting verbatim so undo/trace behave normally.

**ASLIFT on exit (DECIDED, verified against upstream 2026-07-05):**
`forthInner` sets `FLAG_ASLIFT` immediately before its normal (`rsp == 0`)
return. Convention confirmed in upstream: the dispatcher epilogue in
`reallyRunFunction` (items.c:594-598) sets `FLAG_ASLIFT` after every
successfully executed item whose status carries `SLS_ENABLED` — "Stack lift
enabled after item execution" (defines.h:1051) — and the result-producing
items all carry it: `fnAdd` "+" (items.c:1863), `fnSin` (items.c:1844),
`fnMultiply` (items.c:1866). So after `3 SQ` leaves 9 in X, the next digit
entry must *lift* onto 9, not overwrite it. Two reasons `forthInner` must set
the flag itself rather than rely on the epilogue: (1) primitives call handlers
like `fnAdd` directly, bypassing `reallyRunFunction` and its epilogue; (2) the
self-test/harness path enters `forthInner` without going through item dispatch
at all. When entered via `ITM_FCALL`, the item's `SLS_ENABLED` (§0.2) makes
the epilogue agree redundantly. **Required code change (build session, NOT
applied here):** add `setSystemFlag(FLAG_ASLIFT)` before the `rsp == 0` return
in `forth_inner.c`. **Required test change (same commit):** stack test c in
`test_dict_reloc.c` (~310-325) currently asserts clear-on-exit — that
assertion enshrines the wrong behavior and must be flipped. The *internal*
scrub (each push forcing its own lift, clearing after) is correct and
unchanged; only the final exit state changes.

**Re-entrancy guard (DECIDED: guard, not nesting):** `rstack`/`rsp` are static
and `rsp = 0` on every entry, so a nested `forthInner` destroys the outer
word's return stack and the outer loop then EXITs into garbage. Once H1 lands,
the path FTOK_C47 → `reallyRunFunction` → item dispatch → a Forth item
(`ITM_FCALL`, or the H2/H3 XEQ fallback) → nested `forthInner` is reachable —
and the PGM_RUNNING fix (§2.2 resolved issue 2) makes it *more* reachable.
Spec: a `static bool forthRunning`; on nested entry raise
`displayCalcErrorMessage(ERROR_RAM_FULL, ...)` and return without touching
`rstack`/`rsp`/`ip` (same error class as the rstack-depth guard: return-stack
resources exhausted). `forthRunning` is cleared on **every** exit path (normal
EXIT, every error return, runaway guard, cooperative break). Real nesting is
deferred until a use case demands it. **This guard MUST land in the same
commit as H1** — H1 is what makes the re-entry path reachable.

### 3.3 Compilation (`:` … `;`) — `forth_compile.c`, pseudocode
`fnForthOuter` tokenizes the source line (whitespace-delimited), then for each
token resolves via the **lookup order** in §4 and either executes (interpret
state) or appends an `ftoken_t` to the dictionary bump area (compile state).

```
state = INTERPRET
for word in tokenize(sourceLine):
    if word == ":":  startDefinition(nextWord()); state = COMPILE; continue
    if word == ";":  finishDefinition(); state = INTERPRET; continue   // finishDefinition emits FTOK_EXIT itself
    idx  = forthFindPrim(word)                 // §4 step 1
    if idx >= 0:
        if state==COMPILE && !(forthPrims[idx].flags & FF_IMMEDIATE):
            emit(idx + FTOK_PRIM_BASE)           // FTOK_PRIM: token = index + 1.
                                                 // NEVER emit(idx): index 0 (DUP) would
                                                 // emit 0x0000 = FTOK_EXIT (see §2.2)
        else: forthPrims[idx].fn()
        continue
    widx = forthFindColon(word)                // §4 step 2
    if widx >= 0:
        if state==COMPILE: emit(0x1000 + widx)  // FTOK_CALL
        else: forthInner(widx)
        continue
    if isNumber(word):
        if isIntegerLiteral(word) && fitsInt32(word):   // no '.'/exponent — the same
                                                        // syntactic split NIM applies
            if state==COMPILE: emit(FTOK_ILIT); emitBytes(int32(word))
            else: pushLongInteger(word)                 // dtLongInteger — see number-
                                                        // type conformance below
        else:                                           // decimal/exponent form, or
                                                        // integer too wide for int32
            if state==COMPILE: emit(FTOK_LIT); emitBytes(real34(word))
            else: pushReal34(word)                      // dtReal34
        continue
    citem = findNamedLabel(word)               // §4 step 3 (C47 XEQ fallback)
    if citem != INVALID_VARIABLE:
        if state==COMPILE: emit(FTOK_C47); emit16(ITM_XEQ...) ...
        else: execProgram(citem)
        continue
    error("undefined word: %s", word)          // §4 step 4
```

**Number-type conformance (DECIDED 2026-07-06, audit T1-2 — CONFORM to the
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
keyboard. **Required code change (H1 series):** `forthPushInt32`
(forth_inner.c:27-33) currently does `int32ToReal34` — it must build a
`longInteger_t` from the int32 and store it via
`convertLongIntegerToLongIntegerRegister(lgInt, REGISTER_X)` after the lift
(the lift discipline — set ASLIFT / `liftStack()` / clear — is unchanged; only
the store changes). Integer literals wider than int32 (upstream long integers
are arbitrary-precision) do not fit `FTOK_ILIT`'s 4-byte payload; stage 1
compiles them as `FTOK_LIT` real34 — a documented stage-1 limitation, applied
identically in interpret state so compile and interpret semantics never
diverge. The earlier acceptance wording "numbers parsed as real34" (§7.4) is
superseded by this rule.

**Dict-emit API (DECIDED, verified against upstream 2026-07-05): grow-in-place.**
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

The API (`forth_dict.c`; exact semantics, zero unstated decisions). In the
pseudocode above, `emit()`/`emit16()`/`emitBytes()` all denote `forthDictEmit`
calls — inline data wider than one cell is emitted as successive cells:

- `startDefinition(name, nameLen)`:
  1. Snapshot `{here, latest, count}` into a static abort record.
  2. `forthDictEnsure(ceil4(4 + nameLen))`; on failure, error out (nothing
     written).
  3. Write the header at `fdict.here`: `link = fdict.latest`,
     `flags = FF_SMUDGE`, `nameLen`; copy exactly `nameLen` bytes of name;
     zero the 0..3 pad bytes.
  4. `fdict.latest = entryOffset`, `fdict.count++`,
     `fdict.here = entryOffset + ceil4(4 + nameLen)` (= body start).
  The entry is linked immediately but smudged; **`forthFindColon` must skip
  entries with `FF_SMUDGE` set** (required change — the current walk does not
  check flags), so a half-built definition is invisible to lookup and a word
  can reference its previous definition while being redefined.
- `forthDictEmit(ftoken_t tok)`: `forthDictEnsure(2)` — this may
  `reallocC47Blocks`-move the region mid-definition, refreshing `fdict.base`;
  then `memcpy(fdict.base + fdict.here, &tok, 2); fdict.here += 2`. Inline
  data (LIT/ILIT payloads, branch deltas, C47 params+pad) is emitted as
  successive cells through the same call. **The compiler must hold only
  region-relative offsets across emits — never pointers** — including branch
  back-patch positions for future `IF`/`THEN`; any cached pointer is invalid
  after any emit.
- `finishDefinition()` (at `;`): `forthDictEmit(FTOK_EXIT)`; clear
  `FF_SMUDGE`; block-round `fdict.here` (existing `forthDictFinishDef`
  behavior); discard the abort record.
- `abortDefinition()` (error mid-compile): restore the snapshot
  `{here, latest, count}`. The region may stay grown — harmless; the bytes
  above `here` are dead.

REJECTED: temp-buffer + copy-at-`;`. It is not the upstream pattern
(`_insertInProgram` stages nothing), it imposes a hard body-length cap plus a
permanent BSS buffer, and the safety it would buy (no mid-definition moves) is
already guaranteed by the offsets-only discipline §5.3 imposes everywhere else.

**Known defect to fix alongside (forth_dict.c:118):** `forthDictWriteName`
copies `strlen(name)` bytes and ignores the `nameLen` the entry was allocated
with — a longer name overruns into the next entry's header. Clamp the copy to
`hdr->nameLen`. (Under the `startDefinition` spec above the copy length is
`nameLen` by construction, but the helper must still clamp defensively.)

---

## 4. Lookup order change

C47 today resolves an XEQ name through exactly one table: `findNamedLabel`
(manage.c:1864). Forth introduces a **prepended** resolution order. The change
is realized in the Forth outer interpreter and in an override of the two named-
lookup call sites — it does **not** alter C47's own resolution when Forth is not
engaged.

### 4.1 New resolution order (highest priority first)
For a bare name typed at `ITM_FORTH` / found while compiling:

1. **Forth primitive** — `forthFindPrim(name)`: linear scan of `forthPrims[]`
   (flash). Primitives shadow everything (this is standard Forth and keeps
   `DUP`, `+`, etc. deterministic).
2. **Forth colon definition** — `forthFindColon(name)`: walk `fdict.latest`
   link chain, newest-first (Forth redefinition semantics: latest wins).
3. **C47 named label** — `findNamedLabel(name)` (unchanged upstream code). Lets
   Forth call existing keystroke programs by name.
4. **Number literal**, else **undefined-word error**.

Numbers are tried *after* the dictionary so a word may legally be named like a
number-looking token only if explicitly defined; standard Forth tries numbers
last, which we match.

### 4.2 Reverse direction — C47 `XEQ 'NAME'` finding a Forth word
So that a Forth word is reachable from the normal keyboard/`XEQ` and from
existing programs, the `XEQ` name-resolution path gains a Forth fallback. The
touch points are the two dynamic-menu `XEQ` branches and the program `PARAM_LABEL`
resolver:

- `runFunction`, XEQ-by-menu branch (items.c:664-685): after
  `findNamedLabel` returns `INVALID_VARIABLE`, call `forthFindColon`; on hit,
  `reallyRunFunction(ITM_FCALL, widx)` instead of erroring.
- `_executeOp`, `PARAM_LABEL`/`STRING_LABEL_VARIABLE` arm (lblGtoXeq.c:345-357):
  same fallback before `ERROR_LABEL_NOT_FOUND`.

**Order for reverse lookup:** C47 label first (preserve existing programs'
behavior exactly), Forth colon def second. This is the *opposite* precedence of
§4.1 on purpose — inside Forth, Forth wins; from the C47 side, C47 wins, so no
existing keystroke program silently changes meaning.

### 4.3 Why not synthesize label IDs
Rejected alternative: register Forth words into `labelList[]` with synthetic IDs
in a reserved slice of `FIRST_LABEL..LAST_LABEL`. That would auto-populate the
PROG catalog (softmenus.c:1673) but couples Forth lifetime to label GC and risks
ID collisions with user programs. Instead Forth keeps its own dictionary and
exposes words to the UI via a dedicated dynamic catalog (future stage; the
`ITM_FCALL` bridge already makes them executable).

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
  with the existing allocator, undo save/restore, and `getFreeRamMemory`
  (memory.c:6).

Initial allocation: lazily on first `:` definition — `allocC47Blocks(
FORTH_INITIAL_BLOCKS)` with `FORTH_INITIAL_BLOCKS = 64` (256 bytes). Grow policy:
when `here + need > sizeBlocks*4`, `reallocC47Blocks` to
`max(sizeBlocks*2, TO_BLOCKS(here+need))`, refresh `fdict.base`, done.

### 5.3 Why region-relative links
`reallocC47Blocks` may relocate the region. Absolute `C47MEMPTR` links stored
inside headers would all break on a move. **Links and `ip`/`rstack` entries are
byte offsets relative to `fdict.base`.** On relocation only `fdict.base` changes;
no header rewriting, no pointer fix-up pass. Convert to a usable pointer exactly
at deref time: `ptr = fdict.base + offset`.

### 5.4 Budget & high-water reporting (required by CLAUDE.md)
Per-word arena cost, in bytes:

```
cost(word) = ceil4(4 + nameLen) + 2*(tokenCount + 1)   // +1 for FTOK_EXIT
```

Worked examples:
- `: SQ DUP * ;`  → name 2, body `DUP`(prim) `*`(prim) `EXIT` = 3 tokens →
  `ceil4(6)=8` + `2*3=6` = **14 bytes** (4 blocks).
- 31-char name, 100-token body → `ceil4(35)=36 + 2*101=202` = **238 bytes** (60 blocks).

Fixed overheads: `forthDict_t` (12 bytes BSS), `rstack` (128 bytes BSS),
tokenizer scratch (reuse `tmpString`, no new arena). Flash: `forthPrims[]` table
≈ `primCount * (4+1+3+pad)` plus primitive code.

**Reporting rule:** every change that adds/edits primitives or alters the header
layout MUST print, in the PC build, `getFreeRamMemory()` before/after a fixed
benchmark script (`packages/forth-core/bench/hwm.fs`, to be added) and record the
delta in the stage commit message. Target ceiling for stage 1: dictionary region
high-water ≤ **2 KB** (512 blocks) on the 64 KB part under the benchmark.

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

---

## 6. Exact hook points (file:line)

Each hook is a whole-file override placed in `packages/forth-core/` with
`pkg_override_sources`/`pkg_override_headers` in the package `meson.build`. Keep
every override byte-identical to upstream except the marked insertion; this keeps
future upstream merges reviewable.

| id  | file (override)                     | upstream anchor (re-verify before editing)                                   | edit                                                                                          |
|-----|-------------------------------------|------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| H1  | `src/c47/items.c`                   | `indexOfItems[]` def at **items.c:1758**; spare rows **items.c:4690-4691**    | Replace slots 2842/2843 with the exact `ITM_FORTH`/`ITM_FCALL` rows given in §0.2 (param field, tamMinMax, EIM stated there).      |
| H1b | `src/c47/items.h`                   | `#define ITM_2842 2842`, `#define ITM_2843 2843` (items.h ~2949-2950)         | Add `#define ITM_FORTH 2842` / `#define ITM_FCALL 2843` aliases (keep the numeric `ITM_2842`/`ITM_2843` names too). Do NOT touch upstream's `ITM_FWORD 2003` (items.h:2056) — that is the swap-endian item, referenced by softmenus.c:866 (§0.1 naming warning). |
| H2  | `src/c47/programming/lblGtoXeq.c`   | `_executeOp` `PARAM_LABEL` arm **lblGtoXeq.c:341-357**                        | Before `ERROR_LABEL_NOT_FOUND`, add Forth colon-def fallback → `reallyRunFunction(ITM_FCALL,widx)`. |
| H3  | `src/c47/items.c`                   | `runFunction` XEQ-by-menu branch **items.c:664-685**                          | Same fallback as H2 for interactive `XEQ 'name'`.                                              |
| H4  | `src/c47/keyboard.c`                | `executeFunction` **keyboard.c:928** (near runFunction call **:1164/:1429**) | *(Optional stage-2)* route a dedicated Forth soft-key / alpha-entry to `ITM_FORTH`. No stage-1 edit. |
| H5  | `src/c47/saveRestoreBackup.c`       | label save **:398/:526**, restore **:815-816/:988**                          | Add symmetric save/restore of the Forth region ptr + `fdict` scalars (§5.5).                    |
| H6  | `src/c47/softmenus.c`               | dynamic-catalog switch **softmenus.c:1657**, PROG build **:1673-1704**       | *(Stage 2)* add `MNU_FORTH` case enumerating `fdict` names, mirroring the PROG label loop.      |

**Override-content status (as of 2026-07-05):** the four override files in
`packages/forth-core/` (`items.c`, `items.h`, `programming/lblGtoXeq.c`,
`saveRestoreBackup.c`) are currently **byte-identical to upstream** — they are
placeholder shadows carrying none of the hooks above. Landing H1/H1b (decided,
§0.1) means the `items.c` and `items.h` overrides must genuinely diverge from
upstream: `items.c` rows 4690/4691 replaced with the `ITM_FORTH`/`ITM_FCALL`
entries, `items.h` gaining the two new defines. Keep every other line
byte-identical to upstream so merges stay reviewable.

New (non-override) package sources:
```
packages/forth-core/forth_dict.c/.h      dictionary region mgmt, find*, grow, save hooks
packages/forth-core/forth_prims.c/.h     static primitive table (index-stable)
packages/forth-core/forth_inner.c        threaded-code interpreter (§3.2)
packages/forth-core/forth_compile.c      tokenizer + : ; compiler (§3.3)
packages/forth-core/forth_bridge.c       fnForthOuter (ITM_FORTH), fnForthCall (ITM_FCALL)
packages/forth-core/meson.build          pkg_override_sources / pkg_custom_sources / pkg_override_headers
```

`meson.build` shape (per README §"Create Your Own Package"):
```meson
pkg_override_sources = ['items.c', 'programming/lblGtoXeq.c', 'saveRestoreBackup.c']
pkg_override_headers = ['items.h']
pkg_custom_sources   = files('forth_dict.c','forth_prims.c','forth_inner.c',
                             'forth_compile.c','forth_bridge.c')
```
(Stage 2 adds `softmenus.c`, `keyboard.c` to `pkg_override_sources`.)

---

## 7. Stage-1 acceptance (what the local model must deliver)

1. H1/H1b: two items live; `XEQ` of neither crashes; `indexOfItems` size
   unchanged (still ≤ `LAST_ITEM`).
2. `forth_dict.c`: create region lazily, define/find/redefine words, grow across
   a `reallocC47Blocks` move without corruption (unit-test on PC build with a
   forced small initial size to trigger a move).
3. `forth_inner.c`: `: SQ DUP * ; 3 SQ` leaves 9 in X. `FTOK_0BR`/`FTOK_BR`
   exercised by `: ABS DUP 0BR ... ;`-style test.
4. `forth_compile.c`: interpret vs compile state; `FF_IMMEDIATE` respected;
   integer literals parsed as long integers (`FTOK_ILIT` → `dtLongInteger`),
   decimal/exponent literals as real34 (`FTOK_LIT`), matching keyboard entry
   (§3.3 number-type conformance).
5. H2/H3: `findNamedLabel` miss falls through to Forth; existing keystroke
   programs unaffected (regression: a program named the same as a Forth word
   still runs the *program* from the C47 side).
6. H5: save → restore round-trips the dictionary; region-relative links intact.
7. Arena high-water printed for `bench/hwm.fs`; report the mark in the commit,
   ≤ 2 KB region ceiling on 64 KB HW.

### Invariants (must hold at all times)
- `forthPrimCount ≤ 0x0FFF`; `fdict.count ≤ 0x7EFF - 0x1000`. The prim bound
  MUST be enforced at compile time by a
  `_Static_assert(sizeof(forthPrims)/sizeof(forthPrims[0]) <= 0x0FFF, ...)`
  in `forth_prims.c` — the spec previously said "assert at init" and no assert
  of any kind exists in the tree (required change; this is the one invariant
  that breaks silently when primitives are appended).
- Every header is 4-byte aligned; `fdict.here` is always block-rounded.
- Links & `ip` are region-relative; the only absolute pointer is `fdict.base`,
  refreshed immediately after any (re)alloc.
- Forth never grows `indexOfItems[]` and never invents item IDs beyond 2843.
- Error handling routes through C47 (`displayCalcErrorMessage`, `lastErrorCode`)
  so undo/trace/hourglass stay consistent with the rest of the machine.

## 8. H1 commit-series checklist (conformance audit 2026-07-06)

Every item below is DECIDED elsewhere in this document and **confirmed absent
from the code** as of 2026-07-06. H1 makes most of these paths reachable, so
they land in the H1 commit series. None is optional.

Two small fixes land **before** H1 (independent, immediately testable):

| id   | fix | code site | spec |
|------|-----|-----------|------|
| T1-1 | `FTOK_0BR` truth test must type-dispatch (long integer / real34 / complex …) like upstream `compareRegisters` (mathematics/compare.c:505), not raw `real34IsZero` on X's data | forth_inner.c:35-40 (`forthPopIsZero`) | §3.2 FTOK_0BR note |
| T1-3 | Bad entry index at `forthInner` entry raises `ERROR_INVALID_CORRUPTED_DATA` (same as the mid-word FTOK_CALL arm) instead of silently returning | forth_inner.c:51-53 | §3.2 pseudocode entry check |

With H1 itself:

| id | change | code site | spec |
|----|--------|-----------|------|
| C1 | `FTOK_C47` runs under program semantics: save `programRunStop`, set `PGM_RUNNING`, restore only if still `PGM_RUNNING` after | forth_inner.c:169 (bare `reallyRunFunction` call) | §2.2 resolved issue 2, §3.2 FTOK_C47 arm |
| C2 | `FTOK_C47`/`PTP_NUMBER_8` param padded to a full cell (6-byte encoding); decoder `ip += 2` not `+= 1`; hand-assembled self-test body updated in the same commit | forth_inner.c:162-163; test_dict_reloc.c:958-980 | §2.2 resolved issue 1 |
| C3 | `forthRunning` re-entrancy guard (refuse nested entry, `ERROR_RAM_FULL`, cleared on every exit path) | forth_inner.c:44-55 (static `rstack`/`rsp`, no guard) | §3.2 re-entrancy guard |
| C4 | `setSystemFlag(FLAG_ASLIFT)` before the normal `rsp == 0` return; flip the clear-on-exit assertion in stack test c | forth_inner.c:78-82; test_dict_reloc.c:310-325 | §3.2 ASLIFT on exit |
| C5 | `forthFindColon` skips entries with `FF_SMUDGE` set | forth_dict.c:152-181 (no flags check) | §3.3 startDefinition |
| C6 | `forthDictWriteName` clamps the copy to `hdr->nameLen` (currently copies `strlen(name)` — overrun) | forth_dict.c:115-120 | §3.3 known defect |
| C7 | `_Static_assert(sizeof(forthPrims)/sizeof(forthPrims[0]) <= 0x0FFF, ...)` in forth_prims.c | forth_prims.c (no assert exists) | §7 invariants |

Also in the H1 series (decided 2026-07-06, this document):
- **Key poll** (§3.2 "Cooperative break & key poll", audit T1-4):
  `pollProgramInterrupt()` per dispatch on DMCP; runaway cap demoted to
  backstop. Required-change site: forth_inner.c:65.
- **Integer literal type** (§3.3 number-type conformance, audit T1-2):
  `forthPushInt32` (forth_inner.c:27-33) stores `dtLongInteger`, not real34.

## Stage 1 — Resolution Clarifications (amendments)
- forthFindColon returns bool; the colon index is returned via out-param
  (uint16_t *widx_out). Index is 0-based (0..count-1); entries[widx] indexes
  directly. A miss returns false; widx_out is left untouched. No sentinel index.
- Name resolution is CASE-SENSITIVE and uses compareString(CMP_BINARY), identical
  to C47 findNamedLabel. User words and promoted labels resolve under one rule.
- Primitive names use C47 glyph encoding (same as stored labels), so a single
  compareString path serves both. ASCII names are byte-identical single-byte glyphs, verified.

##NOTE (§3.3 tokenizer):
word-split must advance by glyph (stringNextGlyph), not
by byte — a name may contain 0x80+ two-byte glyphs. ASCII-only primitives defer
this, but non-ASCII words require it. Never author names as UTF-8; a UTF-8 lead
byte (0xC0+) is misparsed as a C47 two-byte glyph high byte.

§2.2 correction: FTOK_LIT inline data is 16 bytes (real34_t / decQuad =
REAL34_SIZE_IN_BYTES, realType.h:13), NOT 8. ip advances 16. The §5.4 cost
formula and any 8-byte assumption elsewhere must use 16.