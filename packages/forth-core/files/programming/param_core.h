/*
 * param_core.h -- Native parameter execution core
 *
 * §10.2: byte-identical extraction of _executeOp, _executeWithIndirectRegister,
 * _executeWithIndirectVariable from lblGtoXeq.c.  Behaviour is unchanged;
 * only the location and visibility of the entry point (_executeOp ->
 * paramCoreExecuteOp) differ.  _putLiteral stays in lblGtoXeq.c; the
 * forwarder paramCorePutLiteral is declared here for internal use.
 */

#ifndef PARAM_CORE_H
#define PARAM_CORE_H

#include <stdint.h>
#include <stdbool.h>

void paramCoreExecuteOpBounded(uint8_t *paramAddress, const uint8_t *end,
                               uint16_t op, uint16_t paramMode);
/* `end` is exclusive. Missing fixed-width structural bytes raise
 * ERROR_INVALID_CORRUPTED_DATA; bounded names retain their documented
 * clamp-to-available behavior. */
void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode);
void paramCorePutLiteral(uint8_t *literalAddress);

/* F2-3 (§10.2): the semantic tail for DIRECT (non-indirect, non-name)
 * parameters, shared by native _executeOp arms and Forth's FTOK_C47.
 * Validate mirrors the traced native range checks EXACTLY, including
 * their traced silence: an out-of-range direct parameter is a no-op
 * that sets no error (native parity), so validate returning false
 * means "do nothing", not "raise".
 * EXCEPTION (F4-2A) — PTP_REGISTER: the native gate is regInRange(), which
 * is not a pure predicate (store.c:17-72): on a miss it raises
 * ERROR_OUT_OF_RANGE itself and then returns false. Mirroring the native
 * arm therefore means this one class DOES raise on a rejected value. */
bool paramCoreValidateDirect(uint16_t op, uint16_t ptpClass, uint16_t value);
void paramCoreDispatchDirect(uint16_t op, uint16_t ptpClass, uint16_t value);

#if defined(FORTH_DEBUG_SELFTEST)
extern uint32_t paramCoreDebugNameLengthReads;
#endif

#endif /* PARAM_CORE_H */
