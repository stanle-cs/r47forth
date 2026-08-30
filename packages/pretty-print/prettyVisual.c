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
 * builds a small expression tree, which is then laid out through the
 * SAME node builders the capture engine uses (ppfBuildOp1/ppfBuildOp2)
 * and the same construct assembly the equation parser uses
 * (ppqBuildBigop). Nothing here decides where a bracket goes.
 *
 * PP17 shipped this as a transpiler to equation-language TEXT which was
 * then re-parsed. That worked, but it settled precedence twice — once
 * inserting brackets into a string, once reading them back out — and it
 * made the text grammar a dependency of drawing. PP18 removed the round
 * trip. The text form survives as a test back end, where it serializes
 * the same tree so the pins can assert something readable.
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

#define PPV_POOL_BYTES    512   ///< leaf TEXT only: literals and names
#define PPV_AST_NODES      48   ///< expression nodes for one whole walk
#define PPV_FRAG_MAX      255   ///< a serialized form must fit an evaluator slice
#define PPV_STACK_SLOTS     8   ///< array width; the LIVE depth follows SSIZE4/8
#define PPV_MAX_DEPTH       5   ///< XEQ inlining + construct recursion
#define PPV_STEP_BUDGET   256   ///< decoded steps across the whole walk
#define PPV_DIRTY_MAX       8
#define PPV_NAME_MAX       16   ///< == the evaluator's varName[16]
#define PPV_NIL          0xFF

// AST kinds. The walk produces one of these trees; the back ends consume it.
enum { PPA_FREE = 0, PPA_LIT, PPA_VAR, PPA_OP1, PPA_OP2, PPA_CONSTRUCT };

#define PPV_F_OPAQUE 0x01   ///< a string literal: carryable, never printable
#define PPV_F_SECOND 0x02   ///< CONSTRUCT: a second-order derivative
#define PPV_F_BOUND  0x04   ///< VAR: a seeded construct variable, not a free one

// decline reasons; the number reaches the user through moreInfoOnError
enum { PPV_D_OPCODE = 1, PPV_D_INDIRECT, PPV_D_LOCALLABEL, PPV_D_UNRESOLVED,
       PPV_D_DIRTY, PPV_D_NOLATCH, PPV_D_REGISTER, PPV_D_DEPTH, PPV_D_BUDGET,
       PPV_D_UNDERFLOW, PPV_D_OPAQUE, PPV_D_COLLISION, PPV_D_MEMORY,
       PPV_D_NUMERAL, PPV_D_FRAGMENT, PPV_D_ARENA, PPV_D_EMPTY, PPV_D_NAME,
       PPV_D_TOOBIG };

typedef struct {
  uint8_t  kind;        ///< PPA_*
  uint8_t  child[4];    ///< OP1 [a] · OP2 [a,b] · CONSTRUCT [body,from,to,step]
  uint16_t item;        ///< OP1/OP2: the ITM id · CONSTRUCT: the operator's ITM id
  uint16_t textOff;     ///< LIT/VAR: into ctx->pool
  uint8_t  textLen;
  uint8_t  flags;       ///< PPV_F_*
  uint16_t varOff;      ///< CONSTRUCT: the variable name, also in ctx->pool.
                        ///< uint16_t, not uint8_t: the pool is 512 bytes,
                        ///< so an offset past 255 would wrap into another
                        ///< leaf's text and silently draw the wrong name
  uint8_t  varLen;
} ppvAst_t;

typedef struct {
  uint8_t ast[PPV_STACK_SLOTS];   ///< AST indices, PPV_NIL when empty
  uint8_t depth;
  bool_t  liftDisabled;   ///< ENTER latched: the next lifting read overwrites X
  bool_t  saturated;      ///< the model has held a full stack, so T replicates on every drop
} ppvStack_t;

typedef struct {
  char     pool[PPV_POOL_BYTES];
  ppvAst_t ast[PPV_AST_NODES];
  uint16_t poolUsed;
  uint8_t  astUsed;
  uint16_t latchedInt;                  ///< PGMINT target, labelList index; 0xFFFF = none
  uint16_t latchedDrv;                  ///< PGMDRV target; a SEPARATE slot upstream,
                                        ///< so that taking a derivative does not
                                        ///< repoint what INT will run next
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
  /* ENTER shares a node, so the tree is a DAG and a naive layout visits
   * a shared child 2^k times. ppNewBox returns PP_NONE per call without
   * latching, so layoutFull is the latch; layoutVisits pins the bound. */
  bool_t   layoutFull;
  uint32_t layoutVisits;
} ppvCtx_t;

static void ppvDecline(ppvCtx_t *ctx, uint8_t reason) {
  if(!ctx->failed) {
    ctx->failed        = true;
    ctx->declineReason = reason;
    ctx->declineStep   = ctx->stepsWalked;
  }
}


/* ==== the AST =========================================================
 * The walk builds a small expression tree; ctx->pool holds only LEAF text
 * (a literal as the program spells it, a variable's name). Structure is
 * nodes, not brackets in a string — which is the point of PP18: whether
 * something needs parentheses is decided ONCE, by ppfBuildOp1/ppfBuildOp2
 * when the tree is turned into layout, and not a second time by a parser
 * reading brackets back out of text.
 *
 * Bump-allocated and thrown away per walk, like the layout pool; the
 * capture engine's free list would be machinery for nothing here. */

static uint8_t ppvAlloc(ppvCtx_t *ctx, uint8_t kind) {
  if(ctx->astUsed >= PPV_AST_NODES) {
    ppvDecline(ctx, PPV_D_ARENA);
    return PPV_NIL;
  }
  uint8_t n = ctx->astUsed++;
  ppvAst_t *a = &ctx->ast[n];
  a->kind    = kind;
  a->item    = 0;
  a->textOff = 0;
  a->textLen = 0;
  a->flags   = 0;
  a->varOff  = 0;
  a->varLen  = 0;
  for(uint8_t i = 0; i < 4; i++) {
    a->child[i] = PPV_NIL;
  }
  return n;
}

// leaf text goes in the pool; the node keeps an offset and a length
static bool_t ppvIntern(ppvCtx_t *ctx, const char *bytes, uint16_t len,
                        uint16_t *offOut) {
  if((uint32_t)ctx->poolUsed + len > PPV_POOL_BYTES) {
    ppvDecline(ctx, PPV_D_ARENA);
    return false;
  }
  *offOut = ctx->poolUsed;
  xcopy(ctx->pool + ctx->poolUsed, (void *)bytes, len);
  ctx->poolUsed = (uint16_t)(ctx->poolUsed + len);
  return true;
}

static uint8_t ppvLeaf(ppvCtx_t *ctx, uint8_t kind, const char *text, uint16_t len,
                       uint8_t flags) {
  uint16_t off = 0;
  if(len > 0 && !ppvIntern(ctx, text, len, &off)) {
    return PPV_NIL;
  }
  uint8_t n = ppvAlloc(ctx, kind);
  if(n != PPV_NIL) {
    ctx->ast[n].textOff = off;
    ctx->ast[n].textLen = (uint8_t)len;
    ctx->ast[n].flags   = flags;
  }
  return n;
}

/* How many stack levels the machine currently has: four under SSIZE4,
 * eight otherwise. The array is always eight wide; only the effective
 * depth follows the flag, so the mirror drops its bottom entry where the
 * hardware does. */
static uint8_t ppvLiveStackSlots(void) {
  int16_t n = (int16_t)(getStackTop() - REGISTER_X + 1);
  if(n < 1) {
    n = 1;
  }
  return (n > PPV_STACK_SLOTS) ? (uint8_t)PPV_STACK_SLOTS : (uint8_t)n;
}

static bool_t ppvPush(ppvCtx_t *ctx, ppvStack_t *stk, uint8_t node) {
  if(node == PPV_NIL) {
    ppvDecline(ctx, PPV_D_ARENA);
    return false;
  }
  uint8_t slots = ppvLiveStackSlots();
  if(stk->depth >= slots) {
    // a full stack drops its bottom, exactly as the hardware one does
    for(uint8_t i = 0; i + 1 < slots; i++) {
      stk->ast[i] = stk->ast[i + 1];
    }
    stk->depth = (uint8_t)(slots - 1);
  }
  stk->ast[stk->depth++] = node;
  if(stk->depth >= slots) {
    stk->saturated = true;
  }
  return true;
}

// A lifting read (literal or recall) after ENTER overwrites X instead of
// pushing — fnRecall consults FLAG_ASLIFT for exactly this, and ENTER
// clears it (recall.c). The latch is one-shot.
static bool_t ppvPushLifting(ppvCtx_t *ctx, ppvStack_t *stk, uint8_t node) {
  if(node == PPV_NIL) {
    ppvDecline(ctx, PPV_D_ARENA);
    return false;
  }
  if(stk->liftDisabled && stk->depth > 0) {
    stk->liftDisabled = false;
    stk->ast[stk->depth - 1] = node;
    return true;
  }
  stk->liftDisabled = false;
  return ppvPush(ctx, stk, node);
}

static bool_t ppvPop(ppvCtx_t *ctx, ppvStack_t *stk, uint8_t *out) {
  if(stk->depth == 0) {
    // the program reads state its caller never provided; a seeded body
    // frame provides exactly the construct variable, so this is the
    // honest boundary of what a static walk can claim
    ppvDecline(ctx, PPV_D_UNDERFLOW);
    return false;
  }
  *out = stk->ast[--stk->depth];
  return true;
}

static bool_t ppvIsOpaque(const ppvCtx_t *ctx, uint8_t n) {
  return n != PPV_NIL && (ctx->ast[n].flags & PPV_F_OPAQUE) != 0;
}

static bool_t ppvOp2(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t item) {
  uint8_t b, a;
  if(!ppvPop(ctx, stk, &b) || !ppvPop(ctx, stk, &a)) {   // X is the RIGHT operand
    return false;
  }
  if(ppvIsOpaque(ctx, a) || ppvIsOpaque(ctx, b)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  uint8_t n = ppvAlloc(ctx, PPA_OP2);
  if(n == PPV_NIL) {
    return false;
  }
  ctx->ast[n].item     = item;
  ctx->ast[n].child[0] = a;
  ctx->ast[n].child[1] = b;
  return ppvPush(ctx, stk, n);
}

static bool_t ppvOp1(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t item) {
  uint8_t a;
  if(!ppvPop(ctx, stk, &a)) {
    return false;
  }
  if(ppvIsOpaque(ctx, a)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  uint8_t n = ppvAlloc(ctx, PPA_OP1);
  if(n == PPV_NIL) {
    return false;
  }
  ctx->ast[n].item     = item;
  ctx->ast[n].child[0] = a;
  return ppvPush(ctx, stk, n);
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
    return ppvPushLifting(ctx, stk, ppvLeaf(ctx, PPA_LIT, "", 0, PPV_F_OPAQUE));
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
      exponent = true;   // legitimate, just not spellable: the numeral
      break;             // grammar has no exponent arm, so it goes opaque
    }
    if(!((t[i] >= '0' && t[i] <= '9') || t[i] == '.')) {
      ppvDecline(ctx, PPV_D_NUMERAL);
      return false;
    }
  }
  if(exponent) {
    return ppvPushLifting(ctx, stk, ppvLeaf(ctx, PPA_LIT, "", 0, PPV_F_OPAQUE));
  }
  return ppvPushLifting(ctx, stk,
                        ppvLeaf(ctx, PPA_LIT, t, (uint16_t)strlen(t), 0));
}


/* ==== the walk ========================================================= */

static void ppvWalk(ppvCtx_t *ctx, uint16_t labelIdx, ppvStack_t *stk);

/* Seed a fresh frame with the construct's variable on EVERY level and
 * walk the body. That single seeding covers both channels upstream uses:
 * the integrator writes each node into the named variable AND fills the
 * stack with it, a programmed sum delivers its counter through the
 * filled stack alone. The body's one result is handed back as an AST
 * index; the scratch buffer and the construct-boundary pool rollback
 * this comment used to describe went with the text back end. */
static bool_t ppvBody(ppvCtx_t *ctx, uint16_t bodyIdx, const char *var,
                      bool_t synthetic, uint8_t *out) {
  if(ctx->bindingCount >= PPV_MAX_DEPTH) {
    ppvDecline(ctx, PPV_D_DEPTH);
    return false;
  }
  strcpy(ctx->binding[ctx->bindingCount], var);
  ctx->bindingSynth[ctx->bindingCount] = synthetic;
  ctx->bindingCount++;

  ppvStack_t sub;
  sub.depth        = 0;
  sub.liftDisabled = false;
  sub.saturated    = false;
  /* One shared VAR node on every level: the body reads the same variable
   * from every level, so sharing is the accurate model, not a saving. */
  uint8_t seed = ppvLeaf(ctx, PPA_VAR, var, (uint16_t)strlen(var), PPV_F_BOUND);
  for(uint8_t i = 0; i < PPV_STACK_SLOTS; i++) {
    if(!ppvPush(ctx, &sub, seed)) {
      return false;
    }
  }
  ppvWalk(ctx, bodyIdx, &sub);
  if(ctx->failed) {
    return false;
  }
  if(!ppvPop(ctx, &sub, out)) {
    return false;
  }
  if(ppvIsOpaque(ctx, *out)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  ctx->bindingCount--;
  return true;
}

// Build the construct node once its parts are known. The variable name
// is interned alongside the leaves; the layout back end turns it into
// the runs each shape needs.
static bool_t ppvConstruct(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t item,
                           const char *var, uint8_t body, uint8_t from,
                           uint8_t to, uint8_t step, bool_t second) {
  uint16_t voff = 0;
  uint16_t vlen = (uint16_t)strlen(var);
  if(!ppvIntern(ctx, var, vlen, &voff)) {
    return false;
  }
  uint8_t n = ppvAlloc(ctx, PPA_CONSTRUCT);
  if(n == PPV_NIL) {
    return false;
  }
  ctx->ast[n].item     = item;
  ctx->ast[n].child[0] = body;
  ctx->ast[n].child[1] = from;
  ctx->ast[n].child[2] = to;
  ctx->ast[n].child[3] = step;
  ctx->ast[n].varOff   = voff;
  ctx->ast[n].varLen   = (uint8_t)vlen;
  ctx->ast[n].flags    = second ? PPV_F_SECOND : 0;
  return ppvPush(ctx, stk, n);
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
  uint8_t upper, lower;
  if(!ppvPop(ctx, stk, &upper) || !ppvPop(ctx, stk, &lower)) {
    return false;
  }
  if(ppvIsOpaque(ctx, upper) || ppvIsOpaque(ctx, lower)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  if(ppvNameInList(ctx->binding, ctx->bindingCount, v)) {
    ppvDecline(ctx, PPV_D_COLLISION);   // an inner d-variable shadowing an outer one
    return false;
  }
  uint8_t body;
  if(!ppvBody(ctx, bodyIdx, v, false, &body)) {
    return false;
  }
  return ppvConstruct(ctx, stk, ITM_INTEGRAL_YX, v, body, lower, upper,
                      PPV_NIL, false);
}

/* Is this name already a free variable somewhere in the tree built so far?
 *
 * An invented counter must not collide with a name the program itself
 * uses, or the drawing shows the same letter for two different things
 * with nothing to tell them apart. Enclosing constructs are checked
 * separately, through ctx->binding. */
static bool_t ppvNameUsedInAst(const ppvCtx_t *ctx, const char *name) {
  uint16_t len = (uint16_t)strlen(name);
  for(uint8_t i = 0; i < ctx->astUsed; i++) {
    const ppvAst_t *a = &ctx->ast[i];
    /* Only FREE variables collide. A seeded read carries PPV_F_BOUND, and
     * a closed sibling's counter is out of scope by now; counting either
     * would spend the four-name pool on scopes nobody can confuse. */
    if(a->kind == PPA_VAR && (a->flags & PPV_F_BOUND) == 0
        && a->textLen == len
        && memcmp(ctx->pool + a->textOff, name, len) == 0) {
      return true;
    }
  }
  return false;
}

/* Does this name occur inside ONE subtree — free, or as another
 * construct's bound variable?
 *
 * A closed sibling's counter is reusable, but a construct that ends up
 * inside the next operator's LIMITS is not closed in any sense that
 * matters: it will be drawn within that operator's visual scope. The test
 * is "about to be drawn inside me", not "already built", and the limit
 * operands are exactly the subtrees that will be. */
static bool_t ppvNameInSubtree(const ppvCtx_t *ctx, uint8_t n, const char *name) {
  if(n == PPV_NIL || n >= PPV_AST_NODES) {
    return false;
  }
  const ppvAst_t *a = &ctx->ast[n];
  uint16_t len = (uint16_t)strlen(name);
  if(a->kind == PPA_VAR && (a->flags & PPV_F_BOUND) == 0
      && a->textLen == len && memcmp(ctx->pool + a->textOff, name, len) == 0) {
    return true;
  }
  if(a->kind == PPA_CONSTRUCT && a->varLen == len
      && memcmp(ctx->pool + a->varOff, name, len) == 0) {
    return true;
  }
  for(uint8_t c = 0; c < 4; c++) {
    if(ppvNameInSubtree(ctx, a->child[c], name)) {
      return true;
    }
  }
  return false;
}

/* A construct whose variable has no name in the program needs one
 * invented. Used by SUM/PROD, whose counter arrives in a filled stack,
 * and by a derivative over a body that declares no MVAR — the same
 * situation. NULL if all four candidates are genuinely taken. */
static const char *ppvInventName(const ppvCtx_t *ctx,
                                 uint8_t r1, uint8_t r2, uint8_t r3) {
  static const char *const cands[] = { "n", "m", "k", "j" };
  for(uint8_t i = 0; i < 4; i++) {
    if(ppvNameInList(ctx->binding, ctx->bindingCount, cands[i])
        || ppvNameUsedInAst(ctx, cands[i])
        || ppvNameInSubtree(ctx, r1, cands[i])
        || ppvNameInSubtree(ctx, r2, cands[i])
        || ppvNameInSubtree(ctx, r3, cands[i])) {
      continue;
    }
    return cands[i];
  }
  return NULL;
}

/* Which variable does the derivative actually vary?
 *
 * NOT the f' parameter. calcDeriv asks deriv_pgm_variable(label), which
 * walks the BODY program's own leading MVAR declarations and returns the
 * one matching the f' parameter, else the first declared, else
 * INVALID_VARIABLE. This mirrors that walk, including REM transparency.
 *
 * With no MVAR declared it returns "none" rather than an error, and the
 * caller invents a name. Declining instead would be wrong: fnFillStack
 * is unconditional in _differentiatorIteration and only the STO into the
 * named variable is guarded, so a body that takes its argument off the
 * stack differentiates correctly. Such a body is in the same position as
 * a SUM body — no name in the program — so the picture invents one.
 *
 * The REM arm is not covered by a test: a REM step's tail is sized
 * through literalTailBytes, which a hand-built fixture cannot reach
 * without reproducing that encoding. */
static bool_t ppvDerivVariable(ppvCtx_t *ctx, uint16_t bodyIdx,
                               const char *param, char *out) {
  if(bodyIdx >= numberOfLabels) {
    ppvDecline(ctx, PPV_D_UNRESOLVED);
    return false;
  }
  uint8_t *step = labelList[bodyIdx].instructionPointer;
  char first[PPV_NAME_MAX];
  first[0] = 0;
  for(uint16_t d = 0; d < MAX_MVAR_DECLARATIONS && step != NULL; d++) {
    while(step != NULL && checkOpCodeOfStep(step, ITM_REM)) {
      step = findNextStep(step);
    }
    if(step == NULL || !checkOpCodeOfStep(step, ITM_MVAR)
        || *(step + 2) != STRING_LABEL_VARIABLE) {
      break;
    }
    uint8_t len = boundProgramNameLength(step + 4, *(step + 3));
    if(len == 0 || len >= PPV_NAME_MAX) {
      break;
    }
    char nm[PPV_NAME_MAX];
    xcopy(nm, step + 4, len);
    nm[len] = 0;
    /* Drawability is a property of the name we END UP with, not of every
     * declaration walked past — upstream's walk has no such notion and
     * does not stop. It is judged once, below. */
    if(strcmp(nm, param) == 0) {
      if(!ppvNameIsDrawable(nm)) {
        ppvDecline(ctx, PPV_D_NAME);
        return false;
      }
      strcpy(out, nm);      // the match wins over the first, as upstream
      return true;
    }
    /* Record the first declaration whatever it looks like. Skipping an
     * undrawable one here would pick the second, while upstream still
     * differentiates the first — the mirror would stop mirroring. */
    if(first[0] == 0) {
      strcpy(first, nm);
    }
    step = findNextStep(step);
  }
  if(first[0] != 0 && !ppvNameIsDrawable(first)) {
    ppvDecline(ctx, PPV_D_NAME);   // upstream will vary THIS one; we cannot draw it
    return false;
  }
  strcpy(out, first);   // "" when the body declares nothing: not an error
  return true;
}

/* DERIV: the point comes off the stack, the program from PGMDRV.
 *
 * _differentiatorIteration fills every stack level with the sample point
 * AND stores it into `variable` — but `variable` is NOT the f'
 * parameter. It comes from deriv_pgm_variable(label), which reads the
 * BODY's own MVAR declarations. That is where PP18 first got this wrong:
 * the claim was checked at _differentiatorIteration and assumed at its
 * caller, so the drawn subscript named the parameter while upstream
 * varied something else, and the picture meant a different number from
 * the one XEQ returns. ppvDerivVariable now answers the question
 * upstream answers, and the body is seeded with THAT name.
 *
 * The integral is genuinely different and genuinely simpler:
 * DEI_xeq_user writes into `regist`, and _fnIntegrate sets
 * regist = labelOrVariable — the integral's own parameter. So INTEG's
 * seeding by parameter name is exact, and DERIV's cannot be.
 *
 * PGMDRV is a slot of its own upstream, so a derivative does not
 * repoint what INT would run; the walker keeps the two latches apart
 * for the same reason.
 */
static bool_t ppvDerivative(ppvCtx_t *ctx, ppvStack_t *stk, const uint8_t *pa,
                            bool_t second) {
  char v[PPV_NAME_MAX];
  if(!ppvVarName(ctx, pa, v)) {
    return false;
  }
  if(ctx->latchedDrv == 0xFFFF) {
    ppvDecline(ctx, PPV_D_NOLATCH);
    return false;
  }
  uint16_t bodyIdx = ctx->latchedDrv;
  // the point comes off the stack first: an invented name must not
  // collide with anything drawn inside this operator, and the point is
  // drawn inside it
  uint8_t at;
  if(!ppvPop(ctx, stk, &at)) {
    return false;
  }
  if(ppvIsOpaque(ctx, at)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  char sampled[PPV_NAME_MAX];
  if(!ppvDerivVariable(ctx, bodyIdx, v, sampled)) {
    return false;
  }
  bool_t invented = false;
  if(sampled[0] == 0) {
    // no MVAR: the body reads its argument off the filled stack, as a
    // sum body does, so the picture invents a name rather than refuse
    const char *inv = ppvInventName(ctx, at, PPV_NIL, PPV_NIL);
    if(inv == NULL) {
      ppvDecline(ctx, PPV_D_COLLISION);
      return false;
    }
    strcpy(sampled, inv);
    invented = true;   // arms the shadow guard at the ppvBody call below
  }
  if(ppvNameInList(ctx->binding, ctx->bindingCount, sampled)) {
    ppvDecline(ctx, PPV_D_COLLISION);
    return false;
  }
  uint8_t body;
  /* `synthetic` arms the shadow guard in the RCL arm, and an INVENTED
   * name is exactly what it exists for: without it, a body recalling a
   * real variable spelled like the counter draws as the counter. A name
   * that came from the body's own MVAR is real and must not arm it. */
  if(!ppvBody(ctx, bodyIdx, sampled, invented, &body)) {
    return false;
  }
  return ppvConstruct(ctx, stk, ITM_F1DRV, sampled, body, at, PPV_NIL, PPV_NIL, second);
}

// SUM/PROD: X = step, Y = to, Z = from (_programmableSumProd). The
// counter has no name in RPN — it arrives in a filled stack — so the
// tree has to invent one, and any body that recalls a variable spelled
// the same way declines rather than let the invented name shadow it.
static bool_t ppvSumProd(ppvCtx_t *ctx, ppvStack_t *stk, const uint8_t *pa, bool_t isSum) {
  uint16_t bodyIdx;
  if(!ppvLabelIndex(ctx, pa, &bodyIdx)) {
    return false;
  }
  uint8_t stepN, toN, fromN;
  if(!ppvPop(ctx, stk, &stepN) || !ppvPop(ctx, stk, &toN) || !ppvPop(ctx, stk, &fromN)) {
    return false;
  }
  if(ppvIsOpaque(ctx, stepN) || ppvIsOpaque(ctx, toN) || ppvIsOpaque(ctx, fromN)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return false;
  }
  const char *v = ppvInventName(ctx, fromN, toN, stepN);
  if(v == NULL) {
    ppvDecline(ctx, PPV_D_COLLISION);
    return false;
  }
  // a unit step is the evaluator's default and draws without the ,delta
  // tail: dropping it gives the cleaner picture and identical arithmetic
  const ppvAst_t *st = &ctx->ast[stepN];
  bool_t unitStep = (st->kind == PPA_LIT && st->textLen == 1
                     && ctx->pool[st->textOff] == '1');
  uint8_t body;
  if(!ppvBody(ctx, bodyIdx, v, true, &body)) {
    return false;
  }
  return ppvConstruct(ctx, stk, isSum ? ITM_SIGMAn : ITM_PIn, v, body,
                      fromN, toN, unitStep ? PPV_NIL : stepN, false);
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

/* The declaration and display items: no stack, no picture, and no effect
 * on a pending lift. One list, used by the dispatch arm and by the
 * exception test below, because keeping two in sync by hand is how the
 * exception test gets half-updated. */
#define PPV_DECLARATION_ITEMS \
  case ITM_NULL: case ITM_LBL: case ITM_MVAR: case ITM_REM: \
  case ITM_PAUSE: case ITM_SNAP

/* Items the epilogue's lift clear must NOT run after.
 *
 * Upstream clears stack lift in a dispatch epilogue no item can skip, and
 * this mirrors that: the arms live in ppvStepArm and may return freely,
 * ppvStep applies the epilogue afterwards. A new arm gets the clear
 * whether it breaks or returns, and the exceptions are this list rather
 * than a property of control flow.
 *
 * Membership is "clearing here would DESTROY what the step left", not
 * upstream's SLS_UNCHANGED (XEQ is SLS_ENABLED and is here anyway) and
 * not "the arm touches the latch" — LITERAL, RCL, PGMINT and PGMDRV all
 * decide the latch and are absent, because each leaves it exactly as the
 * epilogue would set it, so the clear is a no-op for them. Three reasons,
 * one per group:
 *   - the declaration items change no stack and must leave a pending
 *     lift alone;
 *   - ENTER is the item that ARMS the latch;
 *   - XEQ does not execute a step, it walks the whole callee on the
 *     shared stack. Its arm clears the latch BEFORE that walk so the
 *     callee cannot inherit one, and whatever the callee leaves is the
 *     caller's to see: a subroutine ending in ENTER arms the caller's
 *     next read. An epilogue firing after the walk would erase that. */
static bool_t ppvLiftNeutral(uint16_t op) {
  switch(op) {
    PPV_DECLARATION_ITEMS:
    case ITM_ENTER:
    case ITM_XEQ:
      return true;
    default:
      return false;
  }
}

static void ppvStepArm(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa);

/* T replication. The hardware stack is always exactly its configured
 * depth: an operation that consumes operands pulls the rest down and T
 * duplicates itself into the vacated slot, so a long chain keeps
 * computing with copies of T rather than running out of operands.
 *
 * This only applies once the model has actually held a full stack. Before
 * that the deeper registers hold whatever preceded the program, which a
 * static walk cannot know, so the underflow decline is the honest answer.
 *
 * It lives in the epilogue for the same reason the lift clear does: the
 * arms that drop are many and the rule is one. */
static void ppvRefillFromT(ppvStack_t *stk) {
  if(!stk->saturated) {
    return;
  }
  uint8_t slots = ppvLiveStackSlots();
  while(stk->depth < slots) {
    for(uint8_t i = stk->depth; i > 0; i--) {
      stk->ast[i] = stk->ast[i - 1];
    }
    stk->depth++;   // ast[0] is unchanged, so T now appears twice
  }
}

static void ppvStep(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa) {
  ppvStepArm(ctx, stk, op, pa);
  if(!ppvLiftNeutral(op)) {
    stk->liftDisabled = false;
  }
  if(!ctx->failed) {
    ppvRefillFromT(stk);
  }
}

static void ppvStepArm(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa) {
  switch(op) {
    PPV_DECLARATION_ITEMS:
      return;

    case ITM_ENTER: {
      if(stk->depth == 0) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      // the dup shares the operand node; the tree is a DAG here and
      // both back ends only ever read it
      if(ppvPush(ctx, stk, stk->ast[stk->depth - 1])) {
        /* Classic ENTER clears FLAG_ASLIFT, so the next lifting read
         * OVERWRITES X. Under eRPN a running program's ENTER dups AND
         * re-sets the latch, so the next read LIFTS instead. */
        stk->liftDisabled = !getSystemFlag(FLAG_ERPN);
      }
      return;
    }

    case ITM_XexY: {
      if(stk->depth < 2) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      uint8_t t = stk->ast[stk->depth - 1];
      stk->ast[stk->depth - 1] = stk->ast[stk->depth - 2];
      stk->ast[stk->depth - 2] = t;
      break;
    }

    case ITM_DROP: {
      uint8_t d;
      ppvPop(ctx, stk, &d);
      break;
    }

    case ITM_DROPY: {
      if(stk->depth < 2) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      stk->ast[stk->depth - 2] = stk->ast[stk->depth - 1];
      stk->depth--;
      break;
    }

    case ITM_FILL: {
      if(stk->depth == 0) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      /* FILL is the one arm that fills the stack without ppvPush, so it
       * has to arm the saturation latch itself or T never replicates and
       * a long chain declines a program the machine runs. It fills the
       * LIVE slots: writing all eight under SSIZE4 leaves depth past the
       * configured depth. */
      uint8_t t = stk->ast[stk->depth - 1];
      uint8_t slots = ppvLiveStackSlots();
      for(uint8_t i = 0; i < slots; i++) {
        stk->ast[i] = t;
      }
      stk->depth = slots;
      stk->saturated = true;
      break;
    }

    case ITM_ADD: case ITM_SUB: case ITM_MULT: case ITM_DIV:
      ppvOp2(ctx, stk, op);
      break;

    case ITM_SQUARE: case ITM_CUBE: case ITM_SQUAREROOTX:
    case ITM_1ONX:   case ITM_CHS:
      ppvOp1(ctx, stk, op);
      break;

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
      ppvPushLifting(ctx, stk,
                     ppvLeaf(ctx, PPA_VAR, nm, (uint16_t)strlen(nm), 0));
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
      stk->liftDisabled = false;   // clear BEFORE the nested walk, see ITM_XEQ
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
      /* Order, not reach: this runs BEFORE the nested walk so a callee
       * never inherits an ENTER latch armed in its caller. The epilogue
       * runs after the arm returns and so cannot do it. */
      stk->liftDisabled = false;
      if(ppvLabelIndex(ctx, pa, &idx)) {
        ppvWalk(ctx, idx, stk);   // a subroutine shares its caller's stack
      }
      return;
    }

    case ITM_PGMDRV: {
      uint16_t idx;
      stk->liftDisabled = false;   // clear BEFORE the nested walk, see ITM_XEQ
      if(ppvLabelIndex(ctx, pa, &idx)) {
        ctx->latchedDrv = idx;
      }
      return;
    }

    case ITM_INTEGRAL_YX: ppvIntegral(ctx, stk, pa);        break;
    case ITM_F1DRV:       ppvDerivative(ctx, stk, pa, false); break;
    case ITM_F2DRV:       ppvDerivative(ctx, stk, pa, true);  break;
    case ITM_SIGMAn:      ppvSumProd(ctx, stk, pa, true);   break;
    case ITM_PIn:         ppvSumProd(ctx, stk, pa, false);  break;

    default: {
      char fname[PPV_NAME_MAX];
      if(ppvMonadicName(op, fname)) {
        ppvOp1(ctx, stk, op);   // the NAME comes from the item at build time
        break;
      }
      ppvDecline(ctx, PPV_D_OPCODE);
      return;
    }
  }
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

/* ==== the layout back end ==============================================
 * AST -> 2D nodes, and the reason PP18 exists. Bracketing is decided
 * here ONCE, by the same ppfBuildOp1/ppfBuildOp2 the capture engine has
 * always used; the walker no longer has an opinion about parentheses.
 *
 * Shaped after ppfFromCaptureNode: default the out-precedence to ATOM,
 * validate, switch on kind, leaves return runs, operator arms recurse
 * into locals and then delegate. */

static uint8_t ppvAstToNodes(ppvCtx_t *ctx, uint8_t n,
                             uint8_t ctxFont, uint8_t childFont, int *outPrec) {
  *outPrec = PPF_PREC_ATOM;
  if(n == PPV_NIL || n >= PPV_AST_NODES || ctx->layoutFull) {
    return PP_NONE;   // once the pool is spent, every path returns at once
  }
  ctx->layoutVisits++;
  const ppvAst_t *a = &ctx->ast[n];
  switch(a->kind) {
    case PPA_VAR:
      return ppNewRun(ctx->pool + a->textOff, a->textLen, ctxFont);

    case PPA_LIT:
      if((a->flags & PPV_F_OPAQUE) != 0) {
        return PP_NONE;   // never reaches here: the walk refuses it first
      }
      if(a->textLen > 0 && ctx->pool[a->textOff] == '-') {
        *outPrec = PPF_PREC_ADD;   // a signed numeral brackets as a term
      }
      return ppNewRun(ctx->pool + a->textOff, a->textLen, ctxFont);

    case PPA_OP1: {
      int p;
      uint8_t x = ppvAstToNodes(ctx, a->child[0], ctxFont, childFont, &p);
      if(x == PP_NONE) {
        ctx->layoutFull = true;
        return PP_NONE;
      }
      // ppfBuildOp has no POW level — a PP_SUP scopes itself, so it
      // reports ATOM and a stacked power would come out unbracketed and
      // read as a^(b^c). Bracket the base ourselves; the alternative,
      // adding a level, would change ppfBuildOp's contract under the
      // capture engine.
      if((a->item == ITM_SQUARE || a->item == ITM_CUBE)
          && ctx->ast[a->child[0]].kind == PPA_OP1
          && (ctx->ast[a->child[0]].item == ITM_SQUARE
              || ctx->ast[a->child[0]].item == ITM_CUBE)) {
        p = PPF_PREC_MUL;
      }
      return ppfBuildOp1(a->item, x, p, ctxFont, childFont, outPrec);
    }

    case PPA_OP2: {
      int pa, pb;
      uint8_t x = ppvAstToNodes(ctx, a->child[0], ctxFont, childFont, &pa);
      if(x == PP_NONE) {
        ctx->layoutFull = true;   // check BEFORE the second recursion, or
        return PP_NONE;           // the doubling survives the latch
      }
      uint8_t y = ppvAstToNodes(ctx, a->child[1], ctxFont, childFont, &pb);
      if(y == PP_NONE) {
        ctx->layoutFull = true;
        return PP_NONE;
      }
      return ppfBuildOp2(a->item, x, pa, y, pb, ctxFont, childFont, outPrec);
    }

    case PPA_CONSTRUCT: {
      int pBody, pFrom, pTo, pStep;
      uint8_t body = ppvAstToNodes(ctx, a->child[0], ctxFont, childFont, &pBody);
      uint8_t from = ppvAstToNodes(ctx, a->child[1], childFont, childFont, &pFrom);
      uint8_t to   = ppvAstToNodes(ctx, a->child[2], childFont, childFont, &pTo);
      uint8_t step = (a->child[3] == PPV_NIL)
                       ? PP_NONE
                       : ppvAstToNodes(ctx, a->child[3], childFont, childFont, &pStep);
      if(body == PP_NONE || from == PP_NONE
          || (to == PP_NONE && a->item != ITM_F1DRV)
          || (a->child[3] != PPV_NIL && step == PP_NONE)) {
        ctx->layoutFull = true;   // every operand is checked, `step` included
        return PP_NONE;
      }
      // A nested construct body needs no bracket: the inner integral is
      // terminated by the outer's own " d<var>". What needs one is a
      // construct with an operator beside it, where nothing terminates it.
      body = ppfWrapIf(body,
                       (ctx->ast[a->child[0]].kind == PPA_CONSTRUCT)
                         ? PPF_PREC_ATOM : pBody,
                       PPF_PREC_MUL, ctxFont);
      if(body == PP_NONE) {
        return PP_NONE;
      }
      bool_t isInt = (a->item == ITM_INTEGRAL_YX);
      bool_t isDrv = (a->item == ITM_F1DRV);
      // INTEG needs only the context-font run, SUM/PROD only the tiny
      // one, DERIV both
      uint8_t varTiny = isInt ? PP_NONE
                              : ppNewRun(ctx->pool + a->varOff, a->varLen, PP_FONT_TINY);
      uint8_t varCtx  = (isInt || isDrv)
                          ? ppNewRun(ctx->pool + a->varOff, a->varLen, ctxFont)
                          : PP_NONE;
      uint8_t kind = isInt ? PPQ_BIG_INTEG
                   : isDrv ? PPQ_BIG_DERIV
                   : ((a->item == ITM_SIGMAn) ? PPQ_BIG_SUM : PPQ_BIG_PROD);
      /* A big operator is not an atom: its body extends right of the
       * stroke, so a neighbour would bind into it. ADD brackets it under
       * x, / and ^, and leaves it bare left of a +. */
      *outPrec = PPF_PREC_ADD;
      return ppqBuildBigop(kind, a->item, body, varTiny, varCtx,
                           from, to, step,
                           (a->flags & PPV_F_SECOND) != 0, ctxFont);
    }

    default:
      return PP_NONE;
  }
}


/* Walk a program and hand back the root of the tree it built, or
 * PPV_NIL with ctx->declineReason set. The product entry point: both the
 * drawing path and the test seam start here. */
static uint8_t ppvRun(ppvCtx_t *ctx, uint16_t labelIdx) {
  ctx->poolUsed      = 0;
  ctx->astUsed       = 0;
  ctx->latchedInt    = 0xFFFF;
  ctx->latchedDrv    = 0xFFFF;
  ctx->stepsWalked   = 0;
  ctx->declineStep   = 0;
  ctx->callDepth     = 0;
  ctx->declineReason = 0;
  ctx->failed        = false;
  ctx->dirtyCount    = 0;
  ctx->bindingCount  = 0;
  ctx->layoutFull    = false;
  ctx->layoutVisits  = 0;

  ppvStack_t stk;
  stk.depth        = 0;
  stk.saturated    = false;
  stk.liftDisabled = false;

  ppvWalk(ctx, labelIdx, &stk);
  if(ctx->failed) {
    return PPV_NIL;
  }
  if(stk.depth == 0) {
    ppvDecline(ctx, PPV_D_EMPTY);
    return PPV_NIL;
  }
  uint8_t root = stk.ast[stk.depth - 1];
  if(ppvIsOpaque(ctx, root)) {
    ppvDecline(ctx, PPV_D_OPAQUE);
    return PPV_NIL;
  }
  return root;
}

/* ==== the text back end (PC_BUILD only) ================================
 * Serializes the same tree to equation-language text. Nothing in the
 * product reads it: XEQ computes a program, VISUAL draws it, and the
 * drawing is built from nodes. It exists so the pins can assert
 * something a person can read, and so one pin can hand the walker's own
 * output to fnEqCalc and check the mathematics against a number nobody
 * chose — which no comparison against my own expected string can do.
 *
 * Compiled out of the device build entirely, which is what removes the
 * text grammar from VISUAL's dependencies.
 *
 * Precedence appears here, and ONLY here, because text has to carry it
 * in brackets. The drawing path does not consult it.
 */
#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)

enum { PPV_PREC_ADD = 0, PPV_PREC_MUL = 1, PPV_PREC_POW = 2, PPV_PREC_ATOM = 3 };

typedef struct {
  char    *buf;
  uint16_t cap, len;
  bool_t   ovf;
} ppvOut_t;

static void ppvOutRaw(ppvOut_t *o, const char *b, uint16_t n) {
  if((uint32_t)o->len + n + 1 > o->cap) {
    o->ovf = true;
    return;
  }
  xcopy(o->buf + o->len, (void *)b, n);
  o->len = (uint16_t)(o->len + n);
  o->buf[o->len] = 0;
}

static void ppvOutStr(ppvOut_t *o, const char *s) {
  ppvOutRaw(o, s, (uint16_t)strlen(s));
}

static uint8_t ppvAstPrec(const ppvCtx_t *ctx, uint8_t n) {
  const ppvAst_t *a = &ctx->ast[n];
  switch(a->kind) {
    case PPA_LIT:
      // a negative numeral is an expr-level term: the grammar's primary
      // arm has no sign, so it brackets wherever it is used as one
      return (a->textLen > 0 && ctx->pool[a->textOff] == '-')
               ? PPV_PREC_ADD : PPV_PREC_ATOM;
    case PPA_OP2:
      return (a->item == ITM_MULT || a->item == ITM_DIV)
               ? PPV_PREC_MUL : PPV_PREC_ADD;
    case PPA_OP1:
      switch(a->item) {
        case ITM_SQUARE: case ITM_CUBE: return PPV_PREC_POW;
        case ITM_1ONX:                  return PPV_PREC_MUL;
        case ITM_CHS:                   return PPV_PREC_ADD;
        default:                        return PPV_PREC_ATOM;
      }
    default:
      return PPV_PREC_ATOM;   // VAR, CONSTRUCT, and the radical/call forms
  }
}

static void ppvSerialize(const ppvCtx_t *ctx, uint8_t n, ppvOut_t *o);

static void ppvOperand(const ppvCtx_t *ctx, uint8_t n, ppvOut_t *o,
                       uint8_t level, bool_t rightSide) {
  uint8_t p = ppvAstPrec(ctx, n);
  bool_t wrap = rightSide ? (p <= level) : (p < level);
  if(wrap) {
    ppvOutStr(o, "(");
  }
  ppvSerialize(ctx, n, o);
  if(wrap) {
    ppvOutStr(o, ")");
  }
}

static void ppvSerialize(const ppvCtx_t *ctx, uint8_t n, ppvOut_t *o) {
  if(n == PPV_NIL) {
    o->ovf = true;
    return;
  }
  const ppvAst_t *a = &ctx->ast[n];
  switch(a->kind) {
    case PPA_LIT:
    case PPA_VAR:
      ppvOutRaw(o, ctx->pool + a->textOff, a->textLen);
      return;

    case PPA_OP2: {
      uint8_t level = (a->item == ITM_MULT || a->item == ITM_DIV)
                        ? PPV_PREC_MUL : PPV_PREC_ADD;
      // multiplication is the cross the evaluator and the renderer BOTH
      // accept; '*' is accepted by neither
      const char *sym = (a->item == ITM_ADD)  ? "+"
                      : (a->item == ITM_SUB)  ? "-"
                      : (a->item == ITM_MULT) ? STD_CROSS : "/";
      ppvOperand(ctx, a->child[0], o, level, false);
      ppvOutStr(o, sym);
      ppvOperand(ctx, a->child[1], o, level, true);
      return;
    }

    case PPA_OP1:
      switch(a->item) {
        case ITM_SQUARE:
          ppvOperand(ctx, a->child[0], o, PPV_PREC_POW, true);
          ppvOutStr(o, "^2");
          return;
        case ITM_CUBE:
          ppvOperand(ctx, a->child[0], o, PPV_PREC_POW, true);
          ppvOutStr(o, "^3");
          return;
        case ITM_SQUAREROOTX:
          // the radical brings its own parentheses: asking for
          // precedence brackets on top would give sqrt((x))
          ppvOutStr(o, "\xa2\x1a" "(");
          ppvSerialize(ctx, a->child[0], o);
          ppvOutStr(o, ")");
          return;
        case ITM_1ONX:
          ppvOutStr(o, "1/");
          ppvOperand(ctx, a->child[0], o, PPV_PREC_MUL, true);
          return;
        case ITM_CHS:
          // negation binds looser than x and /, tighter than + and -:
          // the grammar's leading sign takes a whole TERM, so -a/b needs
          // no brackets while -(a+b) does
          ppvOutStr(o, "-");
          ppvOperand(ctx, a->child[0], o, PPV_PREC_ADD, true);
          return;
        default: {
          char fname[PPV_NAME_MAX];
          if(!ppvMonadicName(a->item, fname)) {
            o->ovf = true;
            return;
          }
          ppvOutStr(o, fname);
          ppvOutStr(o, "(");
          ppvSerialize(ctx, a->child[0], o);
          ppvOutStr(o, ")");
          return;
        }
      }

    case PPA_CONSTRUCT:
      ppvOutStr(o, (a->item == ITM_INTEGRAL_YX) ? "INTEG("
                 : (a->item == ITM_F1DRV)       ? "DERIV("
                 : (a->item == ITM_SIGMAn)      ? "SUM(" : "PROD(");
      ppvSerialize(ctx, a->child[0], o);
      ppvOutStr(o, ";");
      ppvOutRaw(o, ctx->pool + a->varOff, a->varLen);
      ppvOutStr(o, ";");
      ppvSerialize(ctx, a->child[1], o);
      if(a->item == ITM_F1DRV) {
        // DERIV(body;var;at[;order]) — no upper limit, and the order is
        // a literal the renderer accepts only as 1 or 2
        if((a->flags & PPV_F_SECOND) != 0) {
          ppvOutStr(o, ";2");
        }
        ppvOutStr(o, ")");
        return;
      }
      ppvOutStr(o, ";");
      ppvSerialize(ctx, a->child[2], o);
      if(a->child[3] != PPV_NIL) {
        ppvOutStr(o, ";");
        ppvSerialize(ctx, a->child[3], o);
      }
      ppvOutStr(o, ")");
      return;

    default:
      o->ovf = true;
      return;
  }
}

/* Walk a program and lay it out, so a pin can assert the NODE TREE the
 * product actually paints rather than an intermediate. Resets the layout
 * pool itself, as every builder's caller must. */
bool_t ppvTestBuildNodes(uint16_t labelIdx, uint8_t ctxFont, uint8_t childFont,
                         uint8_t *rootOut, uint32_t *visitsOut) {
  ppvCtx_t ctx;
  uint8_t root = ppvRun(&ctx, labelIdx);
  if(visitsOut != NULL) {
    *visitsOut = 0;
  }
  if(root == PPV_NIL) {
    return false;
  }
  int prec;
  ppReset();
  uint8_t node = ppvAstToNodes(&ctx, root, ctxFont, childFont, &prec);
  if(visitsOut != NULL) {
    *visitsOut = ctx.layoutVisits;   // so a pin can assert the BOUND
  }
  if(node == PP_NONE) {
    return false;
  }
  *rootOut = node;
  return true;
}

bool_t ppvTranspile(uint16_t labelIdx, char *out, uint16_t cap,
                    uint8_t *reasonOut, uint16_t *stepOut) {
  ppvCtx_t ctx;
  uint8_t root = ppvRun(&ctx, labelIdx);
  if(root != PPV_NIL) {
    ppvOut_t o;
    o.buf = out;
    o.cap = cap;
    o.len = 0;
    o.ovf = false;
    out[0] = 0;
    ppvSerialize(&ctx, root, &o);
    if(o.ovf) {
      ppvDecline(&ctx, PPV_D_FRAGMENT);
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

#endif // PC_BUILD || TESTSUITE_BUILD


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

static bool_t ppvPaintStackWindow(ppvCtx_t *ctx, uint8_t root) {
  // the T-line ladder: full size first, then a whole-tree shrink
  for(int rung = 0; rung < 2; rung++) {
    int prec;
    ppReset();
    ctx->layoutFull = false;   // a fresh pool deserves a fresh verdict
    uint8_t node = ppvAstToNodes(ctx, root, PP_FONT_STANDARD,
                                 (rung == 0) ? PP_FONT_STANDARD : PP_FONT_TINY, &prec);
    if(node == PP_NONE) {
      return false;   // the layout pool ran out; the tree itself is sound
    }
    if(rung == 1) {
      ppSetFontDeep(node, PP_FONT_TINY);
    }
    if(!ppMeasure(node, 0)) {
      continue;
    }
    const ppNode_t *m = ppNodeAt(node);
    if(m->width > SCREEN_WIDTH - 4 || m->ascent + m->descent > PPV_BAND_ROWS) {
      continue;
    }
    ppvClearBand();
    int16_t base = (int16_t)(PPV_BAND_TOP
                             + (PPV_BAND_ROWS - (m->ascent + m->descent)) / 2
                             + m->ascent);
    ppPaintAt(node, (int16_t)(SCREEN_WIDTH - 2 - m->width), base);
    return true;
  }
  return false;
}


/* Taller than the two stack rows: the full band, 21..167. Same shape as
 * the stack window, only a bigger box and a centred x — and no linear
 * fallback beneath it, because a tree that failed to lay out here failed
 * on SIZE, and there is nothing smaller left to try. */
static bool_t ppvPaintFullScreen(ppvCtx_t *ctx, uint8_t root) {
  for(int rung = 0; rung < 2; rung++) {
    int prec;
    ppReset();
    ctx->layoutFull = false;
    uint8_t node = ppvAstToNodes(ctx, root, PP_FONT_STANDARD,
                                 (rung == 0) ? PP_FONT_STANDARD : PP_FONT_TINY, &prec);
    if(node == PP_NONE) {
      return false;
    }
    if(rung == 1) {
      ppSetFontDeep(node, PP_FONT_TINY);
    }
    if(!ppMeasure(node, 0)) {
      continue;
    }
    const ppNode_t *m = ppNodeAt(node);
    if(m->width > SCREEN_WIDTH - 4 || m->ascent + m->descent > 167 - 21 + 1) {
      continue;
    }
    // The clear happens HERE, once the fit is known: nothing is erased
    // until there is something to put in its place.
    lcd_fill_rect(0, 16, SCREEN_WIDTH, SCREEN_HEIGHT - 16, LCD_SET_VALUE);
    drawSinglePixelFullWidthLine(20);
    drawSinglePixelFullWidthLine(168);
    int16_t base = (int16_t)((21 + 167 - (m->ascent + m->descent)) / 2 + m->ascent);
    ppPaintAt(node, (int16_t)((SCREEN_WIDTH - m->width) / 2), base);
    screenUpdatingMode |= SCRUPD_MANUAL_STACK | SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS;
    screenHoldsDrawnPixels = true;
    // a self-painted screen declares itself one, or EXIT cannot dismiss it
    temporaryInformation = TI_SHOWNOTHING;
    return true;
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

  ppvCtx_t ctx;
  uint8_t root = ppvRun(&ctx, idx);
  if(root == PPV_NIL) {
    // nothing has been painted: the tree is built entire before any
    // pixel is touched, so a decline leaves the screen alone
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "step %u: cannot be drawn (D%u)",
              (unsigned)ctx.declineStep, (unsigned)ctx.declineReason);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    return;
  }

  // A surface leaves session state as it found it, whether or not
  // anything downstream currently reads it. V19 pins this.
  uint16_t saved = currentSolverStatus;
  currentSolverStatus &= (uint16_t)~(SOLVER_STATUS_EQUATION_MODE | SOLVER_STATUS_INTERACTIVE);
  if(ppvPaintStackWindow(&ctx, root)) {
    /* Only the stack refresh is suspended: the menu and the status bar
     * keep working, and X keeps whatever the program left there. The
     * chrome bits are CLEARED rather than left alone, because the guard
     * that decides the menu repaint reads them: ORing would let a bit an
     * earlier full-screen surface set decide this surface's chrome. */
    screenUpdatingMode |= SCRUPD_MANUAL_STACK;
    screenUpdatingMode &= (uint8_t)~(SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS);
    screenHoldsDrawnPixels = true;
    // a self-painted screen declares itself one, or EXIT cannot dismiss
    // it (DESIGN.md §6, the binding rule)
    temporaryInformation = TI_SHOWNOTHING;
  }
  else if(!ppvPaintFullScreen(&ctx, root)) {
    // Neither surface can hold it. An error is the honest answer, not a
    // blank held screen: nothing has been painted, so X still shows what
    // the program returned.
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "too large to draw (D%u)", (unsigned)PPV_D_TOOBIG);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
  }
  currentSolverStatus = saved;
}
