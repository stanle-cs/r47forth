#ifndef FORTH_CAPTURE_H
#define FORTH_CAPTURE_H

#include "c47.h"

#define FORTH_CAP_BYTES 256   /* same 256-byte/196-glyph contract as the
                                 landed aimBuffer capture (manage.c cap) */

typedef enum { FCAP_CLOSED = 0, FCAP_OPEN = 1, FCAP_SUSPENDED = 2 } forthCapState_t;

typedef struct {
  uint8_t     state;          /* forthCapState_t */
  uint8_t    *buf;            /* allocC47Blocks'd; NULL unless FCAP_OPEN */
  uint16_t    sizeBlocks;     /* TO_BLOCKS(FORTH_CAP_BYTES) while allocated */
  /* Suspend snapshot — dormant this stage, wired by F6-2: */
  uint16_t    savedCursor;    /* T_cursorPos at suspend */
  uint16_t    savedLocalStep; /* currentLocalStepNumber at suspend */
  uint32_t    savedStepOffset;/* capture step vs beginOfProgramMemory
                                 (offset: program memory may relocate) */
} forthCap_t;

void        forthCapOpen(void);       /* alloc+zero; on alloc failure:
                                         displays ERROR_RAM_FULL, state
                                         stays FCAP_CLOSED */
void        forthCapClose(void);      /* free; state FCAP_CLOSED; safe if
                                         already closed */
bool_t      forthCapIsOpen(void);     /* state == FCAP_OPEN */
uint8_t    *forthCapBuf(void);        /* NULL unless FCAP_OPEN */
bool_t      forthCapTextNonEmpty(void); /* open && buf[0] != 0 */

/* F6-2: suspend/resume state ops */
void     forthCapSuspendState(uint16_t cursor, uint16_t localStep, uint32_t stepOffset);
bool_t   forthCapIsSuspended(void);
uint16_t forthCapSavedCursor(void);
uint16_t forthCapSavedLocalStep(void);
uint32_t forthCapSavedStepOffset(void);
void     forthCapAbandonSuspended(void);

/* F6-2: orchestrators (programming/manage.c — need the file-static
 * _closeAlphaMenus) */
void     forthCaptureSuspend(void);
void     forthCaptureResume(void);

/* F6-3: shared inserter (implementation stays in keyboard.c — it owns
 * the cap constants' original site). */
bool_t   forthCapInsertName(const char *name);

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t     forthTestCapState(void);
const char *forthTestCapText(void);   /* "" when not open */
#endif

#endif
