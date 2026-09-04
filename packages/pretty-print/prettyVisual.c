// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file prettyVisual.c
 * VISUAL (ITM_VISUAL, item row 984): renders a stored RPN program
 * as mathematical notation without program execution.
 *
 * The function accepts a global program label.
 * It builds an expression tree through static analysis of program bytecode.
 * The function paints the result in the Z and T display window.
 * If the drawing is too tall, it falls back to the full-screen band.
 *
 * The walk declines on unsupported opcodes or unmodeled registers.
 * On decline, the function raises an error and paints nothing.
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

// AST kinds. The walk builds the tree, and the back ends consume it.
enum { PPA_FREE = 0, PPA_LIT, PPA_VAR, PPA_OP1, PPA_OP2, PPA_CONSTRUCT };

#define PPV_F_OPAQUE 0x01   ///< a string literal: carryable, never printable
#define PPV_F_SECOND 0x02   ///< CONSTRUCT: a second-order derivative
#define PPV_F_BOUND  0x04   ///< VAR: bound construct variable flag

// decline reasons. The number reaches the user through moreInfoOnError.
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
  uint16_t varOff;      ///< CONSTRUCT: the variable name, also in
                        ///< ctx->pool. uint16_t: the pool is 512 bytes.
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
  uint16_t latchedDrv;                  ///< PGMDRV target, a separate slot
                                        ///< upstream
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
  /* ENTER shares a node, so the tree is a DAG. layoutFull latches the
   * first PP_NONE, and layoutVisits counts the layout recursion. */
  bool_t   layoutFull;
  uint32_t layoutVisits;
} ppvCtx_t;

/* Record a decline reason during bytecode analysis.
 * If no previous failure exists, sets the failure flag.
 * It also records the step index. */
static void ppvDecline(ppvCtx_t *ctx, uint8_t reason) {
  if(!ctx->failed) {
    ctx->failed        = true;
    ctx->declineReason = reason;
    ctx->declineStep   = ctx->stepsWalked;
  }
}

/* Allocate an expression tree node from the context pool.
 * If the pool is full, records an arena decline.
 * Initializes child links and metadata fields before returning the node index. */
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

/* Store literal text bytes in the context pool.
 * If the pool lacks memory, records an arena decline.
 * Writes the byte offset to offOut and advances the pool pointer. */
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

/* Create a leaf node with interned text in the expression tree.
 * Copies text into the pool and initializes a node with the specified kind and flags.
 * Returns the node index, or PPV_NIL if memory allocation fails. */
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

/* Get the number of active stack levels on the calculator.
 * Returns four levels under four-stack mode, or eight levels under normal mode. */
static uint8_t ppvLiveStackSlots(void) {
  int16_t n = (int16_t)(getStackTop() - REGISTER_X + 1);
  if(n < 1) {
    n = 1;
  }
  return (n > PPV_STACK_SLOTS) ? (uint8_t)PPV_STACK_SLOTS : (uint8_t)n;
}

/* Push an expression node onto the simulated stack.
 * If the stack is full, drops the bottom element to mimic hardware behavior.
 * Returns true on success, or false if the node is invalid. */
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

/* Push an expression node onto the stack respecting stack-lift rules.
 * If stack lift is disabled, overwrites the top register.
 * Otherwise, pushes the node onto the stack.
 * Returns true on success, or false if node allocation fails. */
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

/* Pop the top expression node from the simulated stack.
 * If the stack is empty, records an underflow decline.
 * Writes the node index to out and decrements stack depth. */
static bool_t ppvPop(ppvCtx_t *ctx, ppvStack_t *stk, uint8_t *out) {
  if(stk->depth == 0) {
    // the program reads state its caller never provided: decline
    ppvDecline(ctx, PPV_D_UNDERFLOW);
    return false;
  }
  *out = stk->ast[--stk->depth];
  return true;
}

/* Test whether an expression node contains opaque unprintable data.
 * Returns true if the node index is valid and PPV_F_OPAQUE is set. */
static bool_t ppvIsOpaque(const ppvCtx_t *ctx, uint8_t n) {
  return n != PPV_NIL && (ctx->ast[n].flags & PPV_F_OPAQUE) != 0;
}

/* Apply a binary operator to the top two stack operands.
 * Pops right operand b from X and left operand a from Y.
 * Creates an operator node and pushes it onto the stack.
 * Returns true on success, or false if operands underflow or are opaque. */
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

/* Apply a unary operator to the top stack operand.
 * Pops operand a from X and allocates a unary operator node.
 * Pushes the operator node back onto the stack.
 * Returns true on success, or false on underflow or opaque input. */
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

/* Decode an operation code from program bytecode.
 * Reads one or two bytes for the opcode and sets paramOut to the following argument.
 * Returns the decoded operation identifier. */
static uint16_t ppvOpAt(const uint8_t *step, const uint8_t **paramOut) {
  uint16_t op = *step++;
  if(op & 0x80) {
    op = (uint16_t)(((op & 0x7f) << 8) | *step++);
  }
  *paramOut = step;
  return op;
}

/* Test whether a variable name consists of printable ASCII letters.
 * Verifies that the string contains only letters and fits within length limits.
 * Returns true if valid, or false otherwise. */
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

/* Resolve a global program label parameter to its catalog index.
 * Declines local labels and indirect registers.
 * Writes the label list index to idxOut and returns true on success. */
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

/* Extract a named variable parameter from bytecode.
 * Declines indirect and numbered registers alongside unprintable names.
 * Copies the valid variable name into out and returns true. */
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

/* Test whether a name string matches any entry in a list.
 * Compares name against each entry up to count n.
 * Returns true if found, or false otherwise. */
static bool_t ppvNameInList(const char list[][PPV_NAME_MAX], uint8_t n, const char *name) {
  for(uint8_t i = 0; i < n; i++) {
    if(strcmp(list[i], name) == 0) {
      return true;
    }
  }
  return false;
}

/* Decode a literal number parameter and push it onto the stack.
 * Extracts integer or real numeral strings from bytecode.
 * Pushes opaque placeholders for non-numeric literal types.
 * Returns true on success, or false if parsing fails. */
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
      exponent = true;   // the numeral grammar has no exponent arm:
      break;             // this literal goes opaque
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

/* Analyze the body of a mathematical construct with a bound variable.
 * Seeds all stack levels with the variable node and walks the target program.
 * Writes the evaluated result node to out and returns true on success. */
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
  // one shared VAR node on every level
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

/* Build a construct node for multi-part mathematical operators.
 * Interns the variable name and links body alongside limit child nodes.
 * Pushes the construct node onto the stack and returns true on success. */
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

/* Process an integral instruction during bytecode analysis.
 * Extracts the variable name and pops integration bounds from the stack.
 * Analyzes the integrand routine and pushes an integral construct node.
 * Returns true on success, or false on parsing errors. */
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

/* Test whether a variable name appears as a free variable in the tree.
 * Scans allocated nodes to prevent collisions with invented variable names.
 * Returns true if the name matches an unbound variable, or false otherwise. */
static bool_t ppvNameUsedInAst(const ppvCtx_t *ctx, const char *name) {
  uint16_t len = (uint16_t)strlen(name);
  for(uint8_t i = 0; i < ctx->astUsed; i++) {
    const ppvAst_t *a = &ctx->ast[i];
    // only free variables collide: a seeded read carries PPV_F_BOUND
    if(a->kind == PPA_VAR && (a->flags & PPV_F_BOUND) == 0
        && a->textLen == len
        && memcmp(ctx->pool + a->textOff, name, len) == 0) {
      return true;
    }
  }
  return false;
}

/* Test whether a variable name occurs within a specific expression subtree.
 * Traverses child nodes recursively to locate matching variable references.
 * Returns true if the name is found, or false otherwise. */
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

/* Invent a unique dummy variable name for a construct without a formal name.
 * Tests candidate letters to prevent collisions with existing variables in scope.
 * Returns a pointer to an unused candidate name, or NULL if all choices collide. */
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

/* Determine the differentiation variable from leading MVAR declarations.
 * Scans MVAR steps in the target program to match parameter names.
 * Copies the resolved variable name to out and returns true on success. */
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
    // drawability is judged once, on the name we end up with
    if(strcmp(nm, param) == 0) {
      if(!ppvNameIsDrawable(nm)) {
        ppvDecline(ctx, PPV_D_NAME);
        return false;
      }
      strcpy(out, nm);      // the match wins over the first, as upstream
      return true;
    }
    // record the first declaration whatever it looks like, as upstream
    // does
    if(first[0] == 0) {
      strcpy(first, nm);
    }
    step = findNextStep(step);
  }
  if(first[0] != 0 && !ppvNameIsDrawable(first)) {
    ppvDecline(ctx, PPV_D_NAME);   // upstream varies this one, and we cannot draw it
    return false;
  }
  strcpy(out, first);   // "" when the body declares nothing: not an error
  return true;
}

/* Process a derivative instruction during bytecode analysis.
 * Extracts differentiation target and pops the evaluation point from the stack.
 * Analyzes the derivative body and pushes a derivative construct node.
 * Returns true on success, or false on parsing or collision errors. */
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
  // the point pops first: an invented name must not collide with
  // anything drawn inside this operator, and the point is drawn inside
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
    // no MVAR: the body reads its argument off the filled stack, so
    // invent a name
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
  /* `synthetic` arms the shadow guard in the RCL arm. A name from the
   * body's own MVAR is real. It must not arm the guard. */
  if(!ppvBody(ctx, bodyIdx, sampled, invented, &body)) {
    return false;
  }
  return ppvConstruct(ctx, stk, ITM_F1DRV, sampled, body, at, PPV_NIL, PPV_NIL, second);
}

/* Process a summation or product instruction during bytecode analysis.
 * Pops step and range limit nodes from the stack.
 * Invents an unused loop variable and analyzes the repeated expression body.
 * Builds and pushes a summation or product construct node. */
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
  // a unit step is the evaluator's default: drop the ,delta tail
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

/* Resolve a monadic function opcode to its textual function name.
 * Verifies that the catalog name resolves back to the original opcode.
 * Copies the valid function name to out and returns true. */
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

#define PPV_DECLARATION_ITEMS \
  case ITM_NULL: case ITM_LBL: case ITM_MVAR: case ITM_REM: \
  case ITM_PAUSE: case ITM_SNAP

/* Test whether an opcode preserves the stack-lift disabled state.
 * Identifies declaration items alongside ENTER and subroutine calls.
 * Returns true if the operation preserves lift state, or false otherwise. */
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

/* Replicate the top T register when operands drop down the stack.
 * If the stack was previously saturated, fills vacated top slots with copies of T. */
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

/* Execute a single program instruction step during static analysis.
 * Dispatches the opcode arm and clears the lift latch if the operation is not lift-neutral.
 * Refills empty stack levels from the top T register on success. */
static void ppvStep(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa) {
  ppvStepArm(ctx, stk, op, pa);
  if(!ppvLiftNeutral(op)) {
    stk->liftDisabled = false;
  }
  if(!ctx->failed) {
    ppvRefillFromT(stk);
  }
}

/* Dispatch an operation code to its simulated stack execution handler.
 * Handles arithmetic operators and stack changes alongside subroutine calls.
 * Updates simulated register state or records decline reasons on unmodeled operations. */
static void ppvStepArm(ppvCtx_t *ctx, ppvStack_t *stk, uint16_t op, const uint8_t *pa) {
  switch(op) {
    PPV_DECLARATION_ITEMS:
      return;

    case ITM_ENTER: {
      if(stk->depth == 0) {
        ppvDecline(ctx, PPV_D_UNDERFLOW);
        return;
      }
      // the dup shares the operand node (the tree is a read-only DAG)
      if(ppvPush(ctx, stk, stk->ast[stk->depth - 1])) {
        /* Classic ENTER clears FLAG_ASLIFT, so the next lifting read
         * overwrites X. Under eRPN the next read lifts instead. */
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
      /* FILL fills the stack without ppvPush, so it must arm the
       * saturation latch itself. It fills the live slots only. */
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
        // the emitted text cannot show the store that changed this
        // variable
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
      // STO copies X: the stack picture is unchanged. Later reads of
      // the name decline (see the RCL arm).
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
        // not restored when a construct returns: the latch is a
        // persistent global upstream too
        ctx->latchedInt = idx;
      }
      return;
    }

    case ITM_XEQ: {
      uint16_t idx;
      // clear before the nested walk: a callee must not inherit an
      // ENTER latch armed in its caller
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

/* Traverse program instructions starting from a global label.
 * Decodes steps sequentially up to return statements or step budget limits.
 * Advances simulated stack state for each instruction. */
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

/* Convert an abstract syntax tree into a 2D layout node hierarchy.
 * Translates AST nodes into text runs alongside mathematical operator boxes.
 * Sets outPrec to the expression precedence and returns the root layout index. */
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
      // the same atom predicate the capture leaves use
      if(!ppfTextIsAtom(ctx->pool + a->textOff, a->textLen)) {
        *outPrec = PPF_PREC_ADD;
      }
      return ppNewRun(ctx->pool + a->textOff, a->textLen, ctxFont);

    case PPA_OP1: {
      int p;
      uint8_t x = ppvAstToNodes(ctx, a->child[0], ctxFont, childFont, &p);
      if(x == PP_NONE) {
        ctx->layoutFull = true;
        return PP_NONE;
      }
      // a stacked power (x²)³ needs its base bracketed: ppfBuildOp has
      // no POW level, so force the wrap here
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
        ctx->layoutFull = true;   // test memory capacity before the second recursion
        return PP_NONE;
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
        ctx->layoutFull = true;   // validate all operands, including `step`
        return PP_NONE;
      }
      // a nested construct body needs no bracket: the outer " d<var>"
      // terminates it
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
      // a big operator is not an atom: ADD brackets it under x, / and
      // ^, and leaves it bare beside a +
      *outPrec = PPF_PREC_ADD;
      return ppqBuildBigop(kind, a->item, body, varTiny, varCtx,
                           from, to, step,
                           (a->flags & PPV_F_SECOND) != 0, ctxFont);
    }

    default:
      return PP_NONE;
  }
}


/* Execute static program analysis to construct an expression tree.
 * Initializes analysis context and walks the instruction stream from a label.
 * Returns the root AST node index, or PPV_NIL if analysis fails. */
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

#if defined(PC_BUILD) || defined(TESTSUITE_BUILD)

enum { PPV_PREC_ADD = 0, PPV_PREC_MUL = 1, PPV_PREC_POW = 2, PPV_PREC_ATOM = 3 };

typedef struct {
  char    *buf;
  uint16_t cap, len;
  bool_t   ovf;
} ppvOut_t;

/* Append a raw byte sequence to a serialization buffer.
 * If the buffer capacity is exceeded, sets the overflow flag.
 * Copies bytes and null-terminates the buffer on success. */
static void ppvOutRaw(ppvOut_t *o, const char *b, uint16_t n) {
  if((uint32_t)o->len + n + 1 > o->cap) {
    o->ovf = true;
    return;
  }
  xcopy(o->buf + o->len, (void *)b, n);
  o->len = (uint16_t)(o->len + n);
  o->buf[o->len] = 0;
}

/* Append a null-terminated string to a serialization buffer.
 * Calls ppvOutRaw with the measured string length. */
static void ppvOutStr(ppvOut_t *o, const char *s) {
  ppvOutRaw(o, s, (uint16_t)strlen(s));
}

/* Determine the mathematical operator precedence of an AST node.
 * Evaluates node kind and operation type to assign relative binding power.
 * Returns the precedence level constant. */
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

/* Serialize an operand node with parentheses if precedence requires grouping.
 * Evaluates relative precedence against the parent operator level.
 * Wraps lower-precedence expressions in parentheses. */
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

/* Serialize an expression AST node into mathematical formula text.
 * Traverses operator trees and literals recursively into a text buffer.
 * Sets the overflow flag if the output exceeds buffer capacity. */
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
      // the cross is the multiplication sign both the evaluator and
      // the renderer accept
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
          // the radical brings its own parentheses
          ppvOutStr(o, "\xa2\x1a" "(");
          ppvSerialize(ctx, a->child[0], o);
          ppvOutStr(o, ")");
          return;
        case ITM_1ONX:
          ppvOutStr(o, "1/");
          ppvOperand(ctx, a->child[0], o, PPV_PREC_MUL, true);
          return;
        case ITM_CHS:
          // the grammar's leading sign takes a whole term: -a/b needs
          // no brackets, -(a+b) does
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
        // DERIV syntax accepts body, variable, eval point, plus optional order.
        // There is no upper limit parameter. The order is 1 or 2.
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

/* Build and measure 2D layout nodes for a program label for testing.
 * Runs static analysis and converts the resulting AST into layout boxes.
 * Returns true on success, or false if analysis or layout fails. */
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
    *visitsOut = ctx.layoutVisits;   // the layout visit count, for tests
  }
  if(node == PP_NONE) {
    return false;
  }
  *rootOut = node;
  return true;
}

/* Transpile an RPN program into mathematical equation text for testing.
 * Analyzes bytecode from a label and serializes the AST into out.
 * Returns true on success, or false if the program cannot be analyzed. */
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

#define PPV_BAND_TOP    (Y_POSITION_OF_REGISTER_T_LINE - 4)
#define PPV_BAND_BOTTOM (Y_POSITION_OF_REGISTER_Z_LINE + 31)
#define PPV_BAND_ROWS   (PPV_BAND_BOTTOM - PPV_BAND_TOP + 1)

/* Clear the display rows spanning the Z and T register lines.
 * Fills the stack window rectangle with blank background pixels. */
static void ppvClearBand(void) {
  lcd_fill_rect(0, PPV_BAND_TOP, SCREEN_WIDTH, PPV_BAND_ROWS, LCD_SET_VALUE);
}

/* Attempt to paint an equation into the Z and T stack line window.
 * Tests multiple font scale levels to fit within the two-line display band.
 * Clears the band and paints right-aligned if the equation fits.
 * Returns true if painted, or false if the equation is too large. */
static bool_t ppvPaintStackWindow(ppvCtx_t *ctx, uint8_t root) {
  // the T-line ladder: full size first, then a whole-tree shrink
  for(int rung = 0; rung < 2; rung++) {
    int prec;
    ppReset();
    ctx->layoutFull = false;   // fresh pool, fresh latch
    uint8_t node = ppvAstToNodes(ctx, root, PP_FONT_STANDARD,
                                 (rung == 0) ? PP_FONT_STANDARD : PP_FONT_TINY, &prec);
    if(node == PP_NONE) {
      return false;   // the layout pool ran out, the tree itself is sound
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

/* Paint an equation across the full screen display band.
 * Centers the equation between horizontal frame lines across rows 21 to 167.
 * Sets manual screen update flags so pixels persist until dismissed.
 * Returns true if painted, or false if the equation exceeds screen bounds. */
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
    // clear only once the fit is known
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

/* Display an RPN program as formatted mathematical notation without execution.
 * Resolves the program label and performs static analysis to build an equation.
 * Paints into the stack window or falls back to full-screen display.
 * Displays an error message if analysis declines or fails. */
void fnPrettyVisual(uint16_t label) {
  if(lastErrorCode != ERROR_NONE) {
    return;
  }
  // label resolution follows fnPgmInt's ladder (solver/integrate.c),
  // minus its latch: VISUAL must not repoint what INT runs next
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
    // a decline leaves the screen alone: the tree is built before any
    // pixel is touched
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "step %u: cannot be drawn (D%u)",
              (unsigned)ctx.declineStep, (unsigned)ctx.declineReason);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
    return;
  }

  // leave session state as found
  uint16_t saved = currentSolverStatus;
  currentSolverStatus &= (uint16_t)~(SOLVER_STATUS_EQUATION_MODE | SOLVER_STATUS_INTERACTIVE);
  if(ppvPaintStackWindow(&ctx, root)) {
    /* Only the stack refresh is suspended: the menu and the status bar
     * keep working. The chrome bits are cleared: a bit an earlier
     * full-screen surface set must not decide this surface's chrome. */
    screenUpdatingMode |= SCRUPD_MANUAL_STACK;
    screenUpdatingMode &= (uint8_t)~(SCRUPD_MANUAL_MENU | SCRUPD_MANUAL_SHIFT_STATUS);
    screenHoldsDrawnPixels = true;
    // a self-painted screen declares itself one, or EXIT cannot
    // dismiss it (DESIGN.md §4)
    temporaryInformation = TI_SHOWNOTHING;
  }
  else if(!ppvPaintFullScreen(&ctx, root)) {
    // neither surface can hold it: raise an error, nothing was painted
    displayCalcErrorMessage(ERROR_INVALID_DATA_TYPE_FOR_OP, ERR_REGISTER_LINE, REGISTER_X);
    #if (EXTRA_INFO_ON_CALC_ERROR == 1)
      sprintf(errorMessage, "too large to draw (D%u)", (unsigned)PPV_D_TOOBIG);
      moreInfoOnError("In function fnPrettyVisual:", errorMessage, NULL, NULL);
    #endif // (EXTRA_INFO_ON_CALC_ERROR == 1)
  }
  currentSolverStatus = saved;
}
