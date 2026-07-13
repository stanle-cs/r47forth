/*
 * forth_prims.h -- Forth primitive table (flash, static)
 * Per DESIGN.md §1.3
 */

#ifndef FORTH_PRIMS_H
#define FORTH_PRIMS_H

#include <stdint.h>

typedef void (*forthPrim_t)(void);          /* operates on the C47 stack via helpers */

typedef struct {
  const char  *name;                        /* ASCII/C47 name, NUL-terminated */
  uint8_t      flags;                        /* FF_IMMEDIATE etc. */
  forthPrim_t  fn;
} forthPrimDef_t;

extern const forthPrimDef_t forthPrims[];   /* forth_prims.c, index-stable, append-only */
extern const uint16_t       forthPrimCount;

#endif /* FORTH_PRIMS_H */
