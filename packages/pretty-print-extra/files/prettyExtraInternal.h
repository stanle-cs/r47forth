// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyExtraInternal.h
 * Pretty-print-extra internals, shared between the package's own
 * translation units (prettyCapture.c, prettyFormula.c,
 * browsers/prettyBrowser.c, prettyExtraTest.c). Include after c47.h
 * and prettyInternal.h: the viewer here decodes capture trees into
 * layout nodes through the core package's builders, and the core
 * engine measures and paints them. Not part of the public surface.
 */

#if !defined(PRETTYEXTRAINTERNAL_H)
#define PRETTYEXTRAINTERNAL_H

/* ==== capture engine (prettyCapture.c) ==================================
 * The shadow expression stack's arena, node kinds, and the postfix token
 * stream finished formulas serialize into. Shared with the viewer and
 * the test drivers only. */

#define PPC_NODES      24
#define PPC_NIL        0xFF
#define PPC_UNKNOWN    0xFE
#define PPC_HIST_BYTES 640
#define PPC_HIST_MAX   12
#define PPA_EMITTED    0x01
// A literal leaf stores its text as a head payload plus one LIT2
// continuation, 15 bytes each. Anything longer degrades to a value leaf.
#define PPC_LIT_CAPACITY 30

enum { PPN_FREE = 0, PPN_OP1, PPN_OP2, PPN_LIT, PPN_LIT2, PPN_VAL,
       PPN_RCL, PPN_CONST, PPN_OPAQUE, PPN_BIGOP, PPN_VAL2 };
// PPN_VAL2: a VAL whose payload exceeds one node continues into a
// continuation on child[0], the shape PPN_LIT/PPN_LIT2 already use.
// A complex34 is 32 bytes, so it fits in 16 + 16.
#define PPC_VAL_CAPACITY 32
// PPN_BIGOP: item = ITM id, pad[0..1] = label param (LE), payload = the
// step real34 (zeros for the integral), child[0]/child[1] = the limits.
// Its VALUE is the result the dispatch left in X, so chains continue.

// postfix stream tokens (history entries: 6-byte header {totalBytes u16,
// seq u16, nTokens u8, flags u8} then tokens; TKRES trails when present)
enum { PPT_TKL = 1,    // literal: len u8, text bytes (as typed)
       PPT_TKV,        // value: dataType u8, tag u8, allocParam u16, len u8, payload
       PPT_TKR,        // register/variable leaf: param u16
       PPT_TKC,        // constant: item u16
       PPT_TKO1,       // monadic op: item u16
       PPT_TKO2,       // dyadic op: item u16
       PPT_TKRES,      // result snapshot: same shape as TKV
       PPT_TKBIG };    // big operator: item u16, label u16, step real34(16B); pops from,to

typedef struct {
  uint8_t  kind;        ///< PPN_*
  uint8_t  aux;         ///< LIT/LIT2: text length; VAL: dataType; OP: PPA_* flags
  uint16_t item;        ///< OP/CONST: item id; RCL: param; VAL: allocParam
  uint8_t  child[2];    ///< arena indices; LIT: child[0] = continuation; free list via child[0]
  uint8_t  pad[2];      ///< VAL: tag, payloadBytes
  uint8_t  payload[16]; ///< LIT text / VAL raw register payload
} ppcNode_t;

// prettyFormula.c: capture tree / token stream -> infix layout
bool_t ppfBuildCurrent(uint8_t ctxFont, uint8_t childFont, uint8_t *rootOut);
bool_t ppfBuildEntry(const uint8_t *entry, uint8_t ctxFont, uint8_t childFont,
                     bool_t withResult, uint8_t *rootOut);

// viewer/test API: all lazy, no formatter runs at capture time
const ppcNode_t *ppcNodeAt(uint8_t n);
uint8_t          ppcCurrentFormulaRoot(void);
uint8_t          ppcHistoryCount(void);
const uint8_t   *ppcHistoryEntry(uint8_t idx, uint16_t *lenOut, uint16_t *seqOut);
void             ppcHistoryClear(void);
uint8_t          ppcTestCurrentRaw(void);   ///< test only: the current root before the opaque screen
uint8_t          ppcTestSlotRaw(uint8_t k);   ///< test only: slot k's raw arena index
void             ppcTestDeinit(void);         ///< test only: re-arm the cold-start path
bool_t           ppfTestStagedSpelling(uint8_t dataType, uint8_t tag, const uint8_t *payload,
                                       uint8_t bytes, char *out, size_t cap);   ///< test only: a staged value's display spelling

// prettyFormula.c: the browser reuses the pager's packed row builder.
// canPan: the browser can scroll a row sideways, so it accepts any
// width. The pager cannot, so it refuses an over-wide row.
bool_t ppfBuildRow(uint8_t row, uint8_t haveCurrent, bool_t canPan, uint8_t *rootOut, int16_t *ascOut, int16_t *hOut);

// prettyFormula.c: the T-line renderer ppcInit registers into the
// core's ppTlineExtension slot
bool_t ppfTlineTry(int16_t baseY, int16_t bandTop, int16_t bandBottom,
                   int16_t *lineWidth);

#endif // !PRETTYEXTRAINTERNAL_H
