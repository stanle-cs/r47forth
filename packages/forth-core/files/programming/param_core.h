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

void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode);
void paramCorePutLiteral(uint8_t *literalAddress);

/* F2-3 (§10.2): the semantic tail for DIRECT (non-indirect, non-name)
 * parameters, shared by native _executeOp arms and Forth's FTOK_C47.
 * Validate mirrors the traced native range checks EXACTLY, including
 * their traced silence: an out-of-range direct parameter is a no-op
 * that sets no error (native parity), so validate returning false
 * means "do nothing", not "raise". */
bool paramCoreValidateDirect(uint16_t op, uint16_t ptpClass, uint16_t value);
void paramCoreDispatchDirect(uint16_t op, uint16_t value);

#endif /* PARAM_CORE_H */
