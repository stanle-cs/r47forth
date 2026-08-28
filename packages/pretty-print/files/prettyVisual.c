// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyVisual.c
 * VISUAL (ITM_VISUAL, item row 984): shows a stored RPN program as the
 * mathematics it computes, WITHOUT running it. `XEQ 'DBLINT'` returns
 * 4/3; `VISUAL 'DBLINT'` draws the nested integrals it got that from.
 *
 * The package already had two front-ends to one renderer — the capture
 * engine (live dispatches) and the equation parser (EQN text). This is
 * the third: a symbolic RPN interpreter over stored program steps that
 * transpiles the chain into equation-language TEXT, which the existing
 * ppqShowRender() then parses and paints. Emitting text rather than
 * nodes is what makes the feature small: the renderer, the evaluator,
 * and the EXIT-dismissal protocol are all reused unchanged, and the
 * result is a string the user could have typed into EQN.
 *
 * Faithfulness rests on one fact about how the engines feed a body
 * program. Upstream writes each integration node into BOTH the named
 * d-variable AND every stack level (integrate.c, DEI_xeq_user +
 * fnFillStack); a programmed sum delivers its counter through the
 * filled stack ALONE (sumprod.c). An equation body, by contrast, reads
 * its variable by NAME (solver/equation.c, the XEQ-mode RCL arm). So
 * recursing into a body with the symbolic stack seeded to the variable
 * NAME on every level reproduces both channels at once, and the
 * transpiled text computes what the RPN computed.
 *
 * Everything else is decline-biased, in the package's house style: an
 * opcode this file does not name, an indirect parameter, a local label,
 * flow control, a numbered-register read — none of them are guessed at.
 * The walk fails, an error is raised, and NOTHING is painted. The
 * decline catalog is D1..D18 (ppvDecline callers); DESIGN.md PP17
 * carries the reasoning for each.
 */

#include "c47.h"
#include "prettyInternal.h"

#define PPV_POOL_BYTES   1024   ///< fragment text arena for one whole walk
#define PPV_FRAG_MAX      255   ///< a fragment must fit an evaluator slice
#define PPV_STACK_SLOTS     8   ///< SSIZE8 simulated regardless of the flag
#define PPV_MAX_DEPTH       5   ///< XEQ inlining + construct recursion
#define PPV_STEP_BUDGET   256   ///< decoded steps across the whole walk
#define PPV_DIRTY_MAX       8
#define PPV_NAME_MAX       16   ///< == the evaluator's varName[16]

// precedence lattice, mirroring the equation grammar's own nesting:
// expr(+-) < term(x/) < factor(^) < primary
enum { PPV_PREC_ADD = 0, PPV_PREC_MUL = 1, PPV_PREC_POW = 2, PPV_PREC_ATOM = 3 };

#define PPV_F_OPAQUE 0x01   ///< a string literal: carryable, never printable

// decline reasons; the number reaches the user through moreInfoOnError
enum { PPV_D_OPCODE = 1, PPV_D_INDIRECT, PPV_D_LOCALLABEL, PPV_D_UNRESOLVED,
       PPV_D_DIRTY, PPV_D_NOLATCH, PPV_D_REGISTER, PPV_D_DEPTH, PPV_D_BUDGET,
       PPV_D_UNDERFLOW, PPV_D_OPAQUE, PPV_D_COLLISION, PPV_D_MEMORY,
       PPV_D_NUMERAL, PPV_D_FRAGMENT, PPV_D_POOL, PPV_D_EMPTY, PPV_D_NAME };

typedef struct {
  uint16_t off;     ///< into ctx->pool
  uint16_t len;     ///< 0 for an opaque placeholder
  uint8_t  prec;
  uint8_t  flags;
} ppvFrag_t;

typedef struct {
  ppvFrag_t frag[PPV_STACK_SLOTS];
  uint8_t   depth;
  bool_t    liftDisabled;   ///< ENTER latched: the next lifting read overwrites X
} ppvStack_t;

typedef struct {
  char     pool[PPV_POOL_BYTES];
  uint16_t poolUsed;
  char     scratch[PPV_FRAG_MAX + 1];   ///< construct body, held across the rollback
  uint16_t latchedInt;                  ///< PGMINT target, labelList index; 0xFFFF = none
  uint16_t stepsWalked;
  uint16_t declineStep;
  uint8_t  callDepth;
  uint8_t  declineReason;
  bool_t   failed;
  char     dirty[PPV_DIRTY_MAX][PPV_NAME_MAX];      ///< names a STO wrote during the walk
  uint8_t  dirtyCount;
  char     binding[PPV_MAX_DEPTH][PPV_NAME_MAX];    ///< construct variables in scope
  bool_t   bindingSynth[PPV_MAX_DEPTH];             ///< true = an invented sum counter
  uint8_t  bindingCount;
} ppvCtx_t;

static void ppvDecline(ppvCtx_t *ctx, uint8_t reason) {
  if(!ctx->failed) {
    ctx->failed        = true;
    ctx->declineReason = reason;
    ctx->declineStep   = ctx->stepsWalked;
  }
}


/* ==== fragments =========================================================
 * One linear pool plus 6-byte descriptors, not a buffer per stack slot:
 * eight 256-byte slots per frame would cost kilobytes of C stack at
 * depth. ENTER then costs a descriptor copy and no text at all.
 *
 * Reclamation is by construct-boundary rollback (ppvConstruct): every
 * descriptor alive when a body walk starts points below the mark taken
 * at that moment, so rolling the pool back to the mark afterwards frees
 * the whole body and leaves the surviving fragments intact. Within a
 * frame the pool is append-only — a binary op leaks its operands' bytes,
 * which the pool cap bounds with a clean decline. */

static bool_t ppvRoom(ppvCtx_t *ctx, uint16_t n) {
  if((uint32_t)ctx->poolUsed + n > PPV_POOL_BYTES) {
    ppvDecline(ctx, PPV_D_POOL);
    return false;
  }
  return true;
}

static bool_t ppvAppend(ppvCtx_t *ctx, const char *bytes, uint16_t n) {
  if(!ppvRoom(ctx, n)) {
    return false;
  }
  xcopy(ctx->pool + ctx->poolUsed, (void *)bytes, n);
  ctx->poolUsed = (uint16_t)(ctx->poolUsed + n);
  return true;
}

static bool_t ppvAppendStr(ppvCtx_t *ctx, const char *s) {
  return ppvAppend(ctx, s, (uint16_t)strlen(s));
}

static bool_t ppvPush(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t off, uint16_t len,
                      uint8_t prec, uint8_t flags) {
  if(len > PPV_FRAG_MAX) {
    ppvDecline(ctx, PPV_D_FRAGMENT);
    return false;
  }
  if(stk->depth == PPV_STACK_SLOTS) {
    // a full stack drops its bottom, exactly as the hardware one does
    for(uint8_t i = 0; i + 1 < PPV_STACK_SLOTS; i++) {
      stk->frag[i] = stk->frag[i + 1];
    }
    stk->depth--;
  }
  stk->frag[stk->depth].off   = off;
  stk->frag[stk->depth].len   = len;
  stk->frag[stk->depth].prec  = prec;
  stk->frag[stk->depth].flags = flags;
  stk->depth++;
  return true;
}

// A lifting read (literal or recall) after ENTER overwrites X instead of
// pushing — fnRecall consults FLAG_ASLIFT for exactly this, and ENTER
// clears it (recall.c). The latch is one-shot.
static bool_t ppvPushLifting(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t off,
                             uint16_t len, uint8_t prec, uint8_t flags) {
  if(stk->liftDisabled && stk->depth > 0) {
    stk->liftDisabled = false;
    if(len > PPV_FRAG_MAX) {
      ppvDecline(ctx, PPV_D_FRAGMENT);
      return false;
    }
    stk->frag[stk->depth - 1].off   = off;
    stk->frag[stk->depth - 1].len   = len;
    stk->frag[stk->depth - 1].prec  = prec;
    stk->frag[stk->depth - 1].flags = flags;
    return true;
  }
  stk->liftDisabled = false;
  return ppvPush(ctx, stk, off, len, prec, flags);
}

static bool_t ppvPop(ppvCtx_t *ctx, ppvStack_t *stk, ppvFrag_t *out) {
  if(stk->depth == 0) {
    // the program reads state its caller never provided; a seeded body
    // frame provides exactly the construct variable, so this is the
    // honest boundary of what a static walk can claim
    ppvDecline(ctx, PPV_D_UNDERFLOW);
    return false;
  }
  *out = stk->frag[--stk->depth];
  return true;
}

/* Copy a fragment out, wrapping it in parentheses when the context binds
 * tighter than the fragment does. Left operands may keep an equal
 * precedence (the grammar is left-associative); right operands may not,
 * so a-(b+c) and a/(b·c) never flatten into something that computes
 * differently. */
static bool_t ppvAppendOperand(ppvCtx_t *ctx, const ppvFrag_t *f, uint8_t level, bool_t rightSide) {
  bool_t wrap = rightSide ? (f->prec <= level) : (f->prec < level);
  if(wrap && !ppvAppendStr(ctx, "(")) {
    return false;
  }
  if(!ppvAppend(ctx, ctx->pool + f->off, f->len)) {
    return false;
  }
  if(wrap && !ppvAppendStr(ctx, ")")) {
    return false;
  }
  return true;
}

static bool_t ppvEmitBinary(ppvCtx_t *ctx, ppvStack_t *stk, const char *op, uint8_t level) {
  ppvFrag_t b, a;
  if(!ppvPop(ctx, stk, &b) || !ppvPop(ctx, stk, &a)) {   // X is the RIGHT operand
    return false;
  }
  if(((a.flags | b.flags) & PPV_F_OPAQUE) != 0) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  uint16_t off = ctx->poolUsed;
  if(!ppvAppendOperand(ctx, &a, level, false)
      || !ppvAppendStr(ctx, op)
      || !ppvAppendOperand(ctx, &b, level, true)) {
    return false;
  }
  return ppvPush(ctx, stk, off, (uint16_t)(ctx->poolUsed - off), level, 0);
}

/* pre/post wrap a single operand: 1/x, -x, x^2, sqrt(x). `bracketed`
 * says the pre/post pair ALREADY encloses the argument (the radical's
 * own parentheses) — asking for precedence brackets on top of those
 * yields a valid but doubled sqrt((x)). */
static bool_t ppvEmitMonadic(ppvCtx_t *ctx, ppvStack_t *stk, const char *pre,
                             const char *post, uint8_t argLevel, uint8_t resultPrec,
                             bool_t bracketed) {
  ppvFrag_t a;
  if(!ppvPop(ctx, stk, &a)) {
    return false;
  }
  if((a.flags & PPV_F_OPAQUE) != 0) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  uint16_t off = ctx->poolUsed;
  if(!ppvAppendStr(ctx, pre)) {
    return false;
  }
  if(bracketed) {
    if(!ppvAppend(ctx, ctx->pool + a.off, a.len)) {
      return false;
    }
  }
  else if(!ppvAppendOperand(ctx, &a, argLevel, true)) {
    return false;
  }
  if(!ppvAppendStr(ctx, post)) {
    return false;
  }
  return ppvPush(ctx, stk, off, (uint16_t)(ctx->poolUsed - off), resultPrec, 0);
}


/* ==== step and parameter decoding ======================================
 * Mirrors decodeOp's own parameter grammar (programming/decode.c); the
 * step ADVANCE is upstream's findNextStep, never re-derived here. */

static uint16_t ppvOpAt(const uint8_t *step, const uint8_t **paramOut) {
  uint16_t op = *step++;
  if(op & 0x80) {
    op = (uint16_t)(((op & 0x7f) << 8) | *step++);
  }
  *paramOut = step;
  return op;
}

// A name has to survive the renderer's ppqName, which takes ASCII letters
// (and subscript digits after the first). A variable spelled any other
// way would build text that silently declines to draw.
static bool_t ppvNameIsDrawable(const char *s) {
  if(s[0] == 0) {
    return false;
  }
  for(uint8_t i = 0; s[i] != 0; i++) {
    if(i >= PPV_NAME_MAX - 1) {
      return false;
    }
    if(!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z'))) {
      return false;
    }
  }
  return true;
}

// PARAM_LABEL byte -> labelList index. Global names only: a local label
// number means nothing without a running program's context, which is the
// one thing a static walk does not have.
static bool_t ppvLabelIndex(ppvCtx_t *ctx, const uint8_t *pa, uint16_t *idxOut) {
  uint8_t p = pa[0];
  if(p == STRING_LABEL_VARIABLE) {
    getStringLabelOrVariableName((uint8_t *)(pa + 1));
    calcRegister_t id = findNamedLabel(tmpStringLabelOrVariableName, GLOBAL_LABELS);
    if(id == INVALID_VARIABLE || (uint16_t)(id - FIRST_LABEL) >= numberOfLabels) {
      ppvDecline(ctx, PPV_D_UNRESOLVED);
      return false;
    }
    *idxOut = (uint16_t)(id - FIRST_LABEL);
    return true;
  }
  ppvDecline(ctx, (p == INDIRECT_REGISTER || p == INDIRECT_VARIABLE)
                    ? PPV_D_INDIRECT : PPV_D_LOCALLABEL);
  return false;
}

// PARAM_REGISTER byte -> a named variable's name. Numbered, lettered and
// local registers decline: the equation language reads variables by name
// and has no spelling for them at all.
static bool_t ppvVarName(ppvCtx_t *ctx, const uint8_t *pa, char *out) {
  uint8_t p = pa[0];
  if(p == INDIRECT_REGISTER || p == INDIRECT_VARIABLE) {
    ppvDecline(ctx, PPV_D_INDIRECT);
    return false;
  }
  if(p != STRING_LABEL_VARIABLE) {
    ppvDecline(ctx, PPV_D_REGISTER);
    return false;
  }
  getStringLabelOrVariableName((uint8_t *)(pa + 1));
  if(!ppvNameIsDrawable(tmpStringLabelOrVariableName)) {
    ppvDecline(ctx, PPV_D_NAME);
    return false;
  }
  strcpy(out, tmpStringLabelOrVariableName);
  return true;
}

static bool_t ppvNameInList(const char list[][PPV_NAME_MAX], uint8_t n, const char *name) {
  for(uint8_t i = 0; i < n; i++) {
    if(strcmp(list[i], name) == 0) {
      return true;
    }
  }
  return false;
}


/* ==== literals =========================================================
 * The two plain-numeral forms carry their ASCII exactly as typed, which
 * is what an equation string wants. Every other literal — a string, an
 * exponent numeral, a binary payload — becomes an OPAQUE placeholder
 * rather than a decline: the appnote programs push such values only to
 * store or drop them (a plot title, an ACC setting), and the taint rule
 * below guarantees one can never reach the printed mathematics. */

static bool_t ppvLiteral(ppvCtx_t *ctx, ppvStack_t *stk, const uint8_t *pa) {
  uint8_t type = pa[0];
  if(type != STRING_LONG_INTEGER && type != STRING_REAL34) {
    return ppvPushLifting(ctx, stk, 0, 0, PPV_PREC_ATOM, PPV_F_OPAQUE);
  }
  getStringLabelOrVariableName((uint8_t *)(pa + 1));
  const char *t = tmpStringLabelOrVariableName;
  if(t[0] == 0) {
    ppvDecline(ctx, PPV_D_NUMERAL);
    return false;
  }
  bool_t exponent = false;
  for(uint16_t i = (t[0] == '-') ? 1 : 0; t[i] != 0; i++) {
    if(t[i] == 'e' || t[i] == 'E') {
      exponent = true;   // legitimate, just not spellable: ppqNumber has
      break;             // no exponent arm, so it becomes opaque
    }
    if(!((t[i] >= '0' && t[i] <= '9') || t[i] == '.')) {
      ppvDecline(ctx, PPV_D_NUMERAL);
      return false;
    }
  }
  if(exponent) {
    return ppvPushLifting(ctx, stk, 0, 0, PPV_PREC_ATOM, PPV_F_OPAQUE);
  }
  uint16_t off = ctx->poolUsed, len = (uint16_t)strlen(t);
  if(!ppvAppend(ctx, t, len)) {
    return false;
  }
  // a negative numeral is an expr-level term: the grammar's primary arm
  // has no sign, so it must parenthesize wherever it is used as one
  return ppvPushLifting(ctx, stk, off, len,
                        (t[0] == '-') ? PPV_PREC_ADD : PPV_PREC_ATOM, 0);
}


/* ==== the walk ========================================================= */

static void ppvWalk(ppvCtx_t *ctx, uint16_t labelIdx, ppvStack_t *stk);

/* Seed a fresh frame with the construct's variable on EVERY level and
 * walk the body. That single seeding covers both channels upstream uses:
 * the integrator writes each node into the named variable AND fills the
 * stack with it, a programmed sum delivers its counter through the
 * filled stack alone. The body's one result is handed back in
 * ctx->scratch, which survives the caller's pool rollback. */
static bool_t ppvBody(ppvCtx_t *ctx, uint16_t bodyIdx, const char *var, bool_t synthetic) {
  if(ctx->bindingCount >= PPV_MAX_DEPTH) {
    ppvDecline(ctx, PPV_D_DEPTH);
    return false;
  }
  strcpy(ctx->binding[ctx->bindingCount], var);
  ctx->bindingSynth[ctx->bindingCount] = synthetic;
  ctx->bindingCount++;

  uint16_t voff = ctx->poolUsed, vlen = (uint16_t)strlen(var);
  if(!ppvAppend(ctx, var, vlen)) {
    return false;
  }
  ppvStack_t sub;
  sub.depth        = 0;
  sub.liftDisabled = false;
  for(uint8_t i = 0; i < PPV_STACK_SLOTS; i++) {
    if(!ppvPush(ctx, &sub, voff, vlen, PPV_PREC_ATOM, 0)) {
      return false;
    }
  }
  ppvWalk(ctx, bodyIdx, &sub);
  if(ctx->failed) {
    return false;
  }
  ppvFrag_t body;
  if(!ppvPop(ctx, &sub, &body)) {
    return false;
  }
  if((body.flags & PPV_F_OPAQUE) != 0) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  xcopy(ctx->scratch, ctx->pool + body.off, body.len);
  ctx->scratch[body.len] = 0;
  ctx->bindingCount--;
  return true;
}

// INTEG: X = upper limit, Y = lower (fnIntegrateYX); the integrand is
// whatever the last PGMINT named. Only a latch set DURING this walk
// counts — the runtime global's leftover value is not something a
// drawing may quietly assume.
static bool_t ppvIntegral(ppvCtx_t *ctx, ppvStack_t *stk, const uint8_t *pa) {
  char v[PPV_NAME_MAX];
  if(!ppvVarName(ctx, pa, v)) {
    return false;
  }
  if(ctx->latchedInt == 0xFFFF) {
    ppvDecline(ctx, PPV_D_NOLATCH);
    return false;
  }
  uint16_t bodyIdx = ctx->latchedInt;
  ppvFrag_t upper, lower;
  if(!ppvPop(ctx, stk, &upper) || !ppvPop(ctx, stk, &lower)) {
    return false;
  }
  if(((upper.flags | lower.flags) & PPV_F_OPAQUE) != 0) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  if(ppvNameInList(ctx->binding, ctx->bindingCount, v)) {
    ppvDecline(ctx, PPV_D_COLLISION);   // an inner d-variable shadowing an outer one
    return false;
  }
  uint16_t mark = ctx->poolUsed;
  if(!ppvBody(ctx, bodyIdx, v, false)) {
    return false;
  }
  ctx->poolUsed = mark;   // the body text lives in scratch now
  uint16_t off = ctx->poolUsed;
  if(!ppvAppendStr(ctx, "INTEG(") || !ppvAppendStr(ctx, ctx->scratch)
      || !ppvAppendStr(ctx, ";") || !ppvAppendStr(ctx, v)
      || !ppvAppendStr(ctx, ";") || !ppvAppend(ctx, ctx->pool + lower.off, lower.len)
      || !ppvAppendStr(ctx, ";") || !ppvAppend(ctx, ctx->pool + upper.off, upper.len)
      || !ppvAppendStr(ctx, ")")) {
    return false;
  }
  return ppvPush(ctx, stk, off, (uint16_t)(ctx->poolUsed - off), PPV_PREC_ATOM, 0);
}

// SUM/PROD: X = step, Y = to, Z = from (_programmableSumProd). The
// counter has no name in RPN — it arrives in a filled stack — so the
// text has to invent one, and any body that recalls a variable spelled
// the same way declines rather than let the invented name shadow it.
static bool_t ppvSumProd(ppvCtx_t *ctx, ppvStack_t *stk, const uint8_t *pa, bool_t isSum) {
  uint16_t bodyIdx;
  if(!ppvLabelIndex(ctx, pa, &bodyIdx)) {
    return false;
  }
  ppvFrag_t stepF, toF, fromF;
  if(!ppvPop(ctx, stk, &stepF) || !ppvPop(ctx, stk, &toF) || !ppvPop(ctx, stk, &fromF)) {
    return false;
  }
  if(((stepF.flags | toF.flags | fromF.flags) & PPV_F_OPAQUE) != 0) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  static const char *const cands[] = { "n", "m", "k", "j" };
  const char *v = NULL;
  for(uint8_t i = 0; i < 4 && v == NULL; i++) {
    if(!ppvNameInList(ctx->binding, ctx->bindingCount, cands[i])) {
      v = cands[i];
    }
  }
  if(v == NULL) {
    ppvDecline(ctx, PPV_D_COLLISION);
    return false;
  }
  // a unit step is the evaluator's default and draws without the ,delta
  // tail: omitting it gives the cleaner picture and identical arithmetic
  bool_t unitStep = (stepF.len == 1 && ctx->pool[stepF.off] == '1');

  uint16_t mark = ctx->poolUsed;
  if(!ppvBody(ctx, bodyIdx, v, true)) {
    return false;
  }
  ctx->poolUsed = mark;
  uint16_t off = ctx->poolUsed;
  if(!ppvAppendStr(ctx, isSum ? "SUM(" : "PROD(") || !ppvAppendStr(ctx, ctx->scratch)
      || !ppvAppendStr(ctx, ";") || !ppvAppendStr(ctx, v)
      || !ppvAppendStr(ctx, ";") || !ppvAppend(ctx, ctx->pool + fromF.off, fromF.len)
      || !ppvAppendStr(ctx, ";") || !ppvAppend(ctx, ctx->pool + toF.off, toF.len)) {
    return false;
  }
  if(!unitStep) {
    if(!ppvAppendStr(ctx, ";") || !ppvAppend(ctx, ctx->pool + stepF.off, stepF.len)) {
      return false;
    }
  }
  if(!ppvAppendStr(ctx, ")")) {
    return false;
  }
  return ppvPush(ctx, stk, off, (uint16_t)(ctx->poolUsed - off), PPV_PREC_ATOM, 0);
}


/* A monadic with no 2D spelling but a name the evaluator resolves.
 *
 * Arity comes from the capture engine's own PPC_MO set, because upstream
 * has none to offer — EIM_DY shares its bit with RESULT_IN_X and is
 * vestigial. The NAME is then required to round-trip: the item's catalog
 * spelling has to resolve back through ppEqFunctionItem to this same
 * item. That check is what makes the emitter safe without a hand table
 * to drift — a name the evaluator would not parse back is never emitted,
 * so text that draws but will not compute cannot be produced here. */
static bool_t ppvMonadicName(uint16_t op, char *out) {
  switch(op) {
    case ITM_LN:     case ITM_LOG10:  case ITM_EXP:    case ITM_10x:
    case ITM_EX1:    case ITM_LN1X:   case ITM_ABS:    case ITM_MAGNITUDE:
    case ITM_sin:    case ITM_cos:    case ITM_tan:
    case ITM_arcsin: case ITM_arccos: case ITM_arctan:
      break;
    default:
      return false;
  }
  const char *nm = indexOfItems[op].itemCatalogName;
  if(!ppvNameIsDrawable(nm) || ppEqFunctionItem(nm) != (int16_t)op) {
    return false;
  }
  strcpy(out, nm);
  return true;
}


/* ==== step dispatch =====================================================
 * Fail-closed: an opcode not named here declines. There is no inferred
 * "harmless" rule, because the item table carries no stack-effect
 * metadata to infer from — TICKS is the counterexample that would break
 * one, looking inert and pushing a value nobody can predict. */

static void ppvStep(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa) {
  switch(op) {
    // declarations, comments, display and timing: no stack, no picture,
    // and no effect on the pending lift either
    case ITM_NULL: case ITM_LBL: case ITM_MVAR: case ITM_REM:
    case ITM_PAUSE: case ITM_SNAP:
      return;

    case ITM_ENTER: {
      if(stk->depth == 0) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      ppvFrag_t top = stk->frag[stk->depth - 1];
      if(ppvPush(ctx, stk, top.off, top.len, top.prec, top.flags)) {
        stk->liftDisabled = true;   // the next lifting read overwrites X
      }
      return;
    }

    case ITM_XexY: {
      if(stk->depth < 2) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      ppvFrag_t t = stk->frag[stk->depth - 1];
      stk->frag[stk->depth - 1] = stk->frag[stk->depth - 2];
      stk->frag[stk->depth - 2] = t;
      break;
    }

    case ITM_DROP: {
      ppvFrag_t d;
      ppvPop(ctx, stk, &d);
      break;
    }

    case ITM_DROPY: {
      if(stk->depth < 2) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      stk->frag[stk->depth - 2] = stk->frag[stk->depth - 1];
      stk->depth--;
      break;
    }

    case ITM_FILL: {
      if(stk->depth == 0) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      ppvFrag_t t = stk->frag[stk->depth - 1];
      for(uint8_t i = 0; i < PPV_STACK_SLOTS; i++) {
        stk->frag[i] = t;
      }
      stk->depth = PPV_STACK_SLOTS;
      break;
    }

    case ITM_ADD:  ppvEmitBinary(ctx, stk, "+",       PPV_PREC_ADD); break;
    case ITM_SUB:  ppvEmitBinary(ctx, stk, "-",       PPV_PREC_ADD); break;
    // multiplication typesets as the cross the evaluator and the renderer
    // BOTH accept; '*' is accepted by neither
    case ITM_MULT: ppvEmitBinary(ctx, stk, STD_CROSS, PPV_PREC_MUL); break;
    case ITM_DIV:  ppvEmitBinary(ctx, stk, "/",       PPV_PREC_MUL); break;

    case ITM_SQUARE:      ppvEmitMonadic(ctx, stk, "", "^2", PPV_PREC_POW, PPV_PREC_POW, false); break;
    case ITM_CUBE:        ppvEmitMonadic(ctx, stk, "", "^3", PPV_PREC_POW, PPV_PREC_POW, false); break;
    case ITM_SQUAREROOTX: ppvEmitMonadic(ctx, stk, "\xa2\x1a" "(", ")", 0, PPV_PREC_ATOM, true);  break;
    case ITM_1ONX:        ppvEmitMonadic(ctx, stk, "1/", "", PPV_PREC_MUL, PPV_PREC_MUL, false); break;
    // negation binds looser than x and /, tighter than + and -: the
    // grammar's leading sign takes a whole TERM, so -a/b needs no
    // brackets while -(a+b) does
    case ITM_CHS:         ppvEmitMonadic(ctx, stk, "-",  "", PPV_PREC_ADD, PPV_PREC_ADD, false); break;

    case ITM_LITERAL:
      ppvLiteral(ctx, stk, pa);
      return;   // ppvPushLifting owns the latch

    case ITM_RCL: {
      char nm[PPV_NAME_MAX];
      if(!ppvVarName(ctx, pa, nm)) {
        return;
      }
      if(ppvNameInList(ctx->dirty, ctx->dirtyCount, nm)) {
        // the emitted text would read the variable's ORIGINAL meaning and
        // silently ignore the store that changed it
        ppvDecline(ctx, PPV_D_DIRTY);
        return;
      }
      for(uint8_t i = 0; i < ctx->bindingCount; i++) {
        if(ctx->bindingSynth[i] && strcmp(ctx->binding[i], nm) == 0) {
          // an invented counter name must never shadow a real variable
          ppvDecline(ctx, PPV_D_COLLISION);
          return;
        }
      }
      uint16_t off = ctx->poolUsed, len = (uint16_t)strlen(nm);
      if(ppvAppend(ctx, nm, len)) {
        ppvPushLifting(ctx, stk, off, len, PPV_PREC_ATOM, 0);
      }
      return;
    }

    case ITM_STO: {
      // STO copies X: the stack picture is unchanged. The NAME, though,
      // now means something the emitted text cannot express, so later
      // reads of it decline.
      uint8_t p = pa[0];
      if(p == INDIRECT_REGISTER || p == INDIRECT_VARIABLE) {
        ppvDecline(ctx, PPV_D_INDIRECT);
        return;
      }
      if(p == STRING_LABEL_VARIABLE) {
        getStringLabelOrVariableName((uint8_t *)(pa + 1));
        if(!ppvNameInList(ctx->dirty, ctx->dirtyCount, tmpStringLabelOrVariableName)) {
          if(ctx->dirtyCount >= PPV_DIRTY_MAX
              || strlen(tmpStringLabelOrVariableName) >= PPV_NAME_MAX) {
            ppvDecline(ctx, PPV_D_DIRTY);
            return;
          }
          strcpy(ctx->dirty[ctx->dirtyCount++], tmpStringLabelOrVariableName);
        }
      }
      break;
    }

    case ITM_PGMINT: {
      uint16_t idx;
      if(ppvLabelIndex(ctx, pa, &idx)) {
        // NOT restored when a construct returns: currentSolverProgram is
        // a persistent global upstream, so a callee's relatch is exactly
        // what a second integral would run
        ctx->latchedInt = idx;
      }
      return;
    }

    case ITM_XEQ: {
      uint16_t idx;
      if(ppvLabelIndex(ctx, pa, &idx)) {
        ppvWalk(ctx, idx, stk);   // a subroutine shares its caller's stack
      }
      return;
    }

    case ITM_INTEGRAL_YX: ppvIntegral(ctx, stk, pa);        break;
    case ITM_SIGMAn:      ppvSumProd(ctx, stk, pa, true);   break;
    case ITM_PIn:         ppvSumProd(ctx, stk, pa, false);  break;

    default: {
      char fname[PPV_NAME_MAX];
      if(ppvMonadicName(op, fname)) {
        char pre[PPV_NAME_MAX + 2];
        snprintf(pre, sizeof(pre), "%s(", fname);
        ppvEmitMonadic(ctx, stk, pre, ")", 0, PPV_PREC_ATOM, true);
        break;
      }
      ppvDecline(ctx, PPV_D_OPCODE);
      return;
    }
  }
  stk->liftDisabled = false;   // every op above finishes with lift enabled
}

static void ppvWalk(ppvCtx_t *ctx, uint16_t labelIdx, ppvStack_t *stk) {
  if(ctx->failed) {
    return;
  }
  if(ctx->callDepth >= PPV_MAX_DEPTH) {
    ppvDecline(ctx, PPV_D_DEPTH);
    return;
  }
  if(labelIdx >= numberOfLabels) {
    ppvDecline(ctx, PPV_D_UNRESOLVED);
    return;
  }
  ctx->callDepth++;
  uint8_t *step = labelList[labelIdx].instructionPointer;
  while(step != NULL && !ctx->failed) {
    if(isAtEndOfPrograms(step) || isAtEndOfProgram(step)
        || checkOpCodeOfStep(step, ITM_RTN)) {
      break;
    }
    if(++ctx->stepsWalked > PPV_STEP_BUDGET) {
      ppvDecline(ctx, PPV_D_BUDGET);
      break;
    }
    const uint8_t *pa;
    uint16_t op = ppvOpAt(step, &pa);
    ppvStep(ctx, stk, op, pa);
    if(ctx->failed) {
      break;
    }
    uint8_t *next = findNextStep(step);
    if(next == NULL || next <= step) {
      ppvDecline(ctx, PPV_D_MEMORY);   // scanLabelsAndPrograms' own guard
      break;
    }
    step = next;
  }
  ctx->callDepth--;
}

bool_t ppvTranspile(uint16_t labelIdx, char *out, uint16_t cap,
                    uint8_t *reasonOut, uint16_t *stepOut) {
  ppvCtx_t ctx;
  ctx.poolUsed      = 0;
  ctx.latchedInt    = 0xFFFF;
  ctx.stepsWalked   = 0;
  ctx.declineStep   = 0;
  ctx.callDepth     = 0;
  ctx.declineReason = 0;
  ctx.failed        = false;
  ctx.dirtyCount    = 0;
  ctx.bindingCount  = 0;

  ppvStack_t stk;
  stk.depth        = 0;
  stk.liftDisabled = false;

  ppvWalk(&ctx, labelIdx, &stk);

  if(!ctx.failed) {
    if(stk.depth == 0) {
      ppvDecline(&ctx, PPV_D_EMPTY);
    }
    else {
      ppvFrag_t r = stk.frag[stk.depth - 1];
      if((r.flags & PPV_F_OPAQUE) != 0) {
        ppvDecline(&ctx, PPV_D_OPAQUE);
      }
      else if((uint32_t)r.len + 1 > cap) {
        ppvDecline(&ctx, PPV_D_FRAGMENT);
      }
      else {
        xcopy(out, ctx.pool + r.off, r.len);
        out[r.len] = 0;
      }
    }
  }
  if(reasonOut != NULL) {
    *reasonOut = ctx.declineReason;
  }
  if(stepOut != NULL) {
    *stepOut = ctx.declineStep;
  }
  return !ctx.failed;
}


/* ==== the Z/T window ====================================================
 * Where the drawing goes was part of the request, not a detail: the
 * formula belongs in the two upper stack rows so the ANSWER stays
 * visible in X underneath — XEQ 'DBLINT' gives 4/3, VISUAL 'DBLINT'
 * draws the integrals above it.
 *
 * Measured 2026-08-28 (heights of the transpiled text through
 * ppMeasure): ONE stack line is 36 px and holds only a single integral,
 * and then only at the tiny rung (38 standard, 31 tiny). The T and Z
 * bands TOGETHER are rows 20..91 — 72 px — which holds every chain in
 * appnote 22: a single integral at 38, the double at 58, the coupled
 * triple at 71 once shrunk. That is why this paints across the pair
 * rather than into one line.
 *
 * Anything taller falls through to the full-screen view, which has 147.
 * A formula the 2D grammar declines (plain arithmetic gains nothing from
 * stacking) still shows here, linear, on the Z line — dropping to a
 * full screen for `x·x-x·p-2` would be a worse answer than the one the
 * stack rows already give. */

#define PPV_BAND_TOP    (Y_POSITION_OF_REGISTER_T_LINE - 4)
#define PPV_BAND_BOTTOM (Y_POSITION_OF_REGISTER_Z_LINE + 31)
#define PPV_BAND_ROWS   (PPV_BAND_BOTTOM - PPV_BAND_TOP + 1)

static void ppvClearBand(void) {
  lcd_fill_rect(0, PPV_BAND_TOP, SCREEN_WIDTH, PPV_BAND_ROWS, LCD_SET_VALUE);
}

static bool_t ppvPaintStackWindow(const char *text) {
  // the T-line ladder: full size first, then a whole-tree shrink
  for(int rung = 0; rung < 2; rung++) {
    uint8_t root;
    ppReset();
    if(!ppqParse(text, PP_FONT_STANDARD, PP_FONT_STANDARD, &root)) {
      break;   // not the height that failed — try the linear form below
    }
    if(rung == 1) {
      ppSetFontDeep(root, PP_FONT_TINY);
    }
    if(!ppMeasure(root, 0)) {
      continue;
    }
    const ppNode_t *n = ppNodeAt(root);
    if(n->width > SCREEN_WIDTH - 4 || n->ascent + n->descent > PPV_BAND_ROWS) {
      continue;
    }
    ppvClearBand();
    int16_t base = (int16_t)(PPV_BAND_TOP
                             + (PPV_BAND_ROWS - (n->ascent + n->descent)) / 2
                             + n->ascent);
    ppPaintAt(root, (int16_t)(SCREEN_WIDTH - 2 - n->width), base);
    return true;
  }
  // linear, on the Z line, right-aligned as a stack value is
  {
    int16_t w = stringWidth(text, &standardFont, false, true);
    int16_t h = STANDARD_FONT_HEIGHT;
    if(w > 0 && w <= SCREEN_WIDTH - 4 && h <= PPV_BAND_ROWS) {
      ppvClearBand();
      // centred in the band, not pinned to the Z line: the placement has
      // to follow the band it claims, or a wrong band still looks right
      showString(text, &standardFont, (int16_t)(SCREEN_WIDTH - 2 - w),
                 (int16_t)(PPV_BAND_TOP + (PPV_BAND_ROWS - h) / 2),
                 vmNormal, false, true);
      return true;
    }
  }
  return false;
}


/* ==== the command ======================================================= */

void fnPrettyVisual(uint16_t label) {
  if(lastErrorCode != ERROR_NONE) {
    return;
  }
  // label resolution follows fnPgmInt's own ladder (solver/integrate.c),
  // minus its latch: VISUAL names a program to READ, and must not
  // repoint what INT would run next
  uint16_t idx;
  if(FIRST_LABEL <= label && label <= LAST_LABEL) {
    idx = (uint16_t)(label - FIRST_LABEL);
  }
  else if(REGISTER_X <= label && label <= REGISTER_T) {
    char name[2];
    name[0] = letteredRegisterName((calcRegister_t)label);
    name[1] = 0;
    calcRegister_t r = findNamedLabel(name, GLOBAL_LABELS);
    if(r == INVALID_VARIABLE) {
      displayCalcErrorMessage(ERROR_LABEL_NOT_FOUND, ERR_REGISTER_LINE, REGISTER_X);
      #if (EXTRA_INFO_ON_CALC_ERROR == 1)
        sprintf(errorMessage, "label %s not found", name);
        moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
      #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
      return;
    }
    idx = (uint16_t)(r - FIRST_LABEL);
  }
  else {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "unexpected parameter %u", label);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    return;
  }
  if(idx >= numberOfLabels) {
    displayCalcErrorMessage(ERROR_OUT_OF_RANGE, ERR_REGISTER_LINE, REGISTER_X);
    return;
  }

  char text[PPV_FRAG_MAX + 1];
  uint8_t reason = 0;
  uint16_t atStep = 0;
  if(!ppvTranspile(idx, text, sizeof(text), &reason, &atStep)) {
    // nothing has been painted: the whole text is composed before any
    // pixel is touched, so a decline leaves the screen alone
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "step %u: cannot be drawn (D%u)", atStep, reason);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    return;
  }

  // ppqShowRender frames its result from the live solver session, and a
  // STALE integrate or derivative bit would wrap a program's drawing in
  // an integral sign it never asked for. This text is the whole picture;
  // clear the framing for the call and put the session back.
  uint16_t saved = currentSolverStatus;
  currentSolverStatus &= (uint16_t)~(SOLVER_STATUS_EQUATION_MODE | SOLVER_STATUS_INTERACTIVE);
  if(ppvPaintStackWindow(text)) {
    // only the stack refresh is suspended: the menu and the status bar
    // keep working, and X keeps whatever the program left there
    screenUpdatingMode |= SCRUPD_MANUAL_STACK;
    screenHoldsDrawnPixels = true;
    // a self-painted screen declares itself one, or EXIT cannot dismiss
    // it (DESIGN.md §6, the binding rule)
    temporaryInformation = TI_SHOWNOTHING;
  }
  else {
    ppqShowRender(text);   // taller than the two rows: the full screen has 147
  }
  currentSolverStatus = saved;
}
