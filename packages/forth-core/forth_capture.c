#include "forth_capture.h"

static forthCap_t forthCap;   /* zero-initialized: FCAP_CLOSED */

void forthCapOpen(void) {
  if (forthCap.state == FCAP_OPEN) {
    forthCap.buf[0] = 0;                    /* reopen = fresh line */
    return;
  }
  forthCap.sizeBlocks = TO_BLOCKS(FORTH_CAP_BYTES);
  forthCap.buf = allocC47Blocks(forthCap.sizeBlocks);
  if (forthCap.buf == NULL) {
    forthCap.sizeBlocks = 0;
    displayCalcErrorMessage(ERROR_RAM_FULL, ERR_REGISTER_LINE, NIM_REGISTER_LINE);
    return;
  }
  forthCap.buf[0] = 0;
  forthCap.state = FCAP_OPEN;
}

void forthCapClose(void) {
  if (forthCap.buf != NULL) {
    freeC47Blocks(forthCap.buf, forthCap.sizeBlocks);
    forthCap.buf = NULL;
  }
  forthCap.sizeBlocks = 0;
  forthCap.state = FCAP_CLOSED;
}

bool_t forthCapIsOpen(void)  { return forthCap.state == FCAP_OPEN; }
uint8_t *forthCapBuf(void)   { return forthCap.state == FCAP_OPEN ? forthCap.buf : NULL; }
bool_t forthCapTextNonEmpty(void) {
  return forthCap.state == FCAP_OPEN && forthCap.buf[0] != 0;
}

#if defined(FORTH_DEBUG_SELFTEST)
uint8_t forthTestCapState(void) { return forthCap.state; }
const char *forthTestCapText(void) {
  return forthCap.state == FCAP_OPEN ? (const char *)forthCap.buf : "";
}
#endif
