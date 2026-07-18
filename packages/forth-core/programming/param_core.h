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

void paramCoreExecuteOp(uint8_t *paramAddress, uint16_t op, uint16_t paramMode);
void paramCorePutLiteral(uint8_t *literalAddress);

#endif /* PARAM_CORE_H */
