// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file elec.c
 ***********************************************/

#include "c47.h"

  static void jmBegin(void) {                                                         // common preamble for all delta/star/symmetrical transforms
    saveForUndo();
    setSystemFlag(FLAG_ASLIFT);
  }

  static void jmGetXYZ(cplx_t *x, cplx_t *y, cplx_t *z) {                             // read Z, Y, X registers as complex (ctxtReal39 internals, real34_t in)
    getRegisterAsComplex(REGISTER_X, CPLX(*x));
    getRegisterAsComplex(REGISTER_Y, CPLX(*y));
    getRegisterAsComplex(REGISTER_Z, CPLX(*z));
  }

  static void jmPutResults(const cplx_t *l, const cplx_t *x, const cplx_t *y, const cplx_t *z) {  // store L, X, Y, Z then collapse any pure-real result back to real34_t
    convertComplexToResultRegister(CPLX(*l), REGISTER_L);
    convertComplexToResultRegister(CPLX(*x), REGISTER_X);
    convertComplexToResultRegister(CPLX(*y), REGISTER_Y);
    convertComplexToResultRegister(CPLX(*z), REGISTER_Z);
    convertComplexRegisterToRealIfZeroImag(REGISTER_X);
    convertComplexRegisterToRealIfZeroImag(REGISTER_Y);
    convertComplexRegisterToRealIfZeroImag(REGISTER_Z);
    convertComplexRegisterToRealIfZeroImag(REGISTER_L);
  }

  static void jmSetAOperators(cplx_t *aOp, cplx_t *aaOp) {                            // a = 1 angle 120deg = -1/2 + j*root3/2 ; a^2 = conjugate of a = -1/2 - j*root3/2
    realCopy(const_1on2, &aOp->Real);
    realChangeSign(&aOp->Real);
    realCopy(const39_root3on2, &aOp->Imag);
    realCopy(&aOp->Real, &aaOp->Real);
    realCopy(&aOp->Imag, &aaOp->Imag);
    realChangeSign(&aaOp->Imag);
  }


static bool_t jmDeltaToStar(void) {                                                   // Delta to Star; ZYX to ZYX. Complex math in real_t (ctxtReal39), in/out via real34_t
    cplx_t x, y, z, s, xy, yz, zx, rx, ry, rz;
    jmBegin();
    jmGetXYZ(&x, &y, &z);
    addComplex(CPLX(x), CPLX(y), CPLX(s), &ctxtReal39);                               // s = x + y + z
    addComplex(CPLX(s), CPLX(z), CPLX(s), &ctxtReal39);
    mulComplexComplex(CPLX(z), CPLX(x), CPLX(zx), &ctxtReal39);                       // pairwise products
    mulComplexComplex(CPLX(x), CPLX(y), CPLX(xy), &ctxtReal39);
    mulComplexComplex(CPLX(y), CPLX(z), CPLX(yz), &ctxtReal39);
    divComplexComplex(CPLX(zx), CPLX(s), CPLX(rx), &ctxtReal39);                      // X = (z*x) / s
    divComplexComplex(CPLX(xy), CPLX(s), CPLX(ry), &ctxtReal39);                      // Y = (x*y) / s
    divComplexComplex(CPLX(yz), CPLX(s), CPLX(rz), &ctxtReal39);                      // Z = (y*z) / s
    jmPutResults(&x, &rx, &ry, &rz);                                                  // L = original X
    temporaryInformation = TI_ABC;
    return true;
  }

static bool_t jmStarToDelta(void) {                                                   // Star to Delta; ZYX to ZYX. Complex math in real_t (ctxtReal39), in/out via real34_t
    cplx_t x, y, z, p, t, rx, ry, rz;
    jmBegin();
    jmGetXYZ(&x, &y, &z);
    mulComplexComplex(CPLX(x), CPLX(y), CPLX(p), &ctxtReal39);                        // p = x*y + y*z + z*x
    mulComplexComplex(CPLX(y), CPLX(z), CPLX(t), &ctxtReal39);
    addComplex(CPLX(p), CPLX(t), CPLX(p), &ctxtReal39);
    mulComplexComplex(CPLX(z), CPLX(x), CPLX(t), &ctxtReal39);
    addComplex(CPLX(p), CPLX(t), CPLX(p), &ctxtReal39);
    divComplexComplex(CPLX(p), CPLX(z), CPLX(rx), &ctxtReal39);                       // X = p / z
    divComplexComplex(CPLX(p), CPLX(x), CPLX(ry), &ctxtReal39);                       // Y = p / x
    divComplexComplex(CPLX(p), CPLX(y), CPLX(rz), &ctxtReal39);                       // Z = p / y
    jmPutResults(&x, &rx, &ry, &rz);                                                  // L = original X
    temporaryInformation = TI_ABBCCA;
    return true;
  }

  static bool_t jmSymToAbc(void) {                                                    // Symmetrical components -> ABC; ZYX to ZYX. Complex math in real_t (ctxtReal39), in/out via real34_t
    cplx_t a2, a1, a0, va, vb, vc, aOp, aaOp, t;
    jmBegin();
    jmGetXYZ(&a2, &a1, &a0);                                                          // X = A2 (negative), Y = A1 (positive), Z = A0 (zero) sequence
    jmSetAOperators(&aOp, &aaOp);
    addComplex(CPLX(a0), CPLX(a1), CPLX(va), &ctxtReal39);                            // Va = A0 + A1 + A2
    addComplex(CPLX(va), CPLX(a2), CPLX(va), &ctxtReal39);
    mulComplexComplex(CPLX(aaOp), CPLX(a1), CPLX(vb), &ctxtReal39);                   // Vb = A0 + a^2*A1 + a*A2
    mulComplexComplex(CPLX(aOp),  CPLX(a2), CPLX(t),  &ctxtReal39);
    addComplex(CPLX(vb), CPLX(t),  CPLX(vb), &ctxtReal39);
    addComplex(CPLX(vb), CPLX(a0), CPLX(vb), &ctxtReal39);
    mulComplexComplex(CPLX(aOp),  CPLX(a1), CPLX(vc), &ctxtReal39);                   // Vc = A0 + a*A1 + a^2*A2
    mulComplexComplex(CPLX(aaOp), CPLX(a2), CPLX(t),  &ctxtReal39);
    addComplex(CPLX(vc), CPLX(t),  CPLX(vc), &ctxtReal39);
    addComplex(CPLX(vc), CPLX(a0), CPLX(vc), &ctxtReal39);
    jmPutResults(&a2, &vc, &vb, &va);                                                 // L = original X (A2)
    temporaryInformation = TI_ABC;
    return true;
  }

  static bool_t jmAbcToSym(void) {                                                    // ABC -> symmetrical components; ZYX to ZYX. Complex math in real_t (ctxtReal39), in/out via real34_t
    cplx_t vc, vb, va, s0, s1, s2, aOp, aaOp, t;
    jmBegin();
    jmGetXYZ(&vc, &vb, &va);                                                          // X = Vc, Y = Vb, Z = Va
    jmSetAOperators(&aOp, &aaOp);
    addComplex(CPLX(va), CPLX(vb), CPLX(s0), &ctxtReal39);                            // A0 = (Va + Vb + Vc) / 3
    addComplex(CPLX(s0), CPLX(vc), CPLX(s0), &ctxtReal39);
    realDivide(&s0.Real, const_3, &s0.Real, &ctxtReal39);
    realDivide(&s0.Imag, const_3, &s0.Imag, &ctxtReal39);
    mulComplexComplex(CPLX(aOp),  CPLX(vb), CPLX(s1), &ctxtReal39);                   // A1 = (Va + a*Vb + a^2*Vc) / 3
    mulComplexComplex(CPLX(aaOp), CPLX(vc), CPLX(t),  &ctxtReal39);
    addComplex(CPLX(s1), CPLX(t),  CPLX(s1), &ctxtReal39);
    addComplex(CPLX(s1), CPLX(va), CPLX(s1), &ctxtReal39);
    realDivide(&s1.Real, const_3, &s1.Real, &ctxtReal39);
    realDivide(&s1.Imag, const_3, &s1.Imag, &ctxtReal39);
    mulComplexComplex(CPLX(aaOp), CPLX(vb), CPLX(s2), &ctxtReal39);                   // A2 = (Va + a^2*Vb + a*Vc) / 3
    mulComplexComplex(CPLX(aOp),  CPLX(vc), CPLX(t),  &ctxtReal39);
    addComplex(CPLX(s2), CPLX(t),  CPLX(s2), &ctxtReal39);
    addComplex(CPLX(s2), CPLX(va), CPLX(s2), &ctxtReal39);
    realDivide(&s2.Real, const_3, &s2.Real, &ctxtReal39);
    realDivide(&s2.Imag, const_3, &s2.Imag, &ctxtReal39);
    jmPutResults(&vc, &s2, &s1, &s0);                                                 // L = original X (Vc)
    temporaryInformation = TI_012;
    return true;
  }


  static bool_t jmCopyXtoAbc(void) {                                                  // Copy/create X -> ABC balanced set; X in, ZYX out, L = original X. Complex math in real_t (ctxtReal39), in/out via real34_t
    cplx_t x, rx, ry, aOp, aaOp;
    jmBegin();
    getRegisterAsComplex(REGISTER_X, CPLX(x));                                        // X = source phasor
    jmSetAOperators(&aOp, &aaOp);                                                     // a = 1 angle 120deg ; a^2 = 1 angle 240deg
    mulComplexComplex(CPLX(aOp),  CPLX(x), CPLX(ry), &ctxtReal39);                    // Y = a*x
    mulComplexComplex(CPLX(aaOp), CPLX(x), CPLX(rx), &ctxtReal39);                    // X = a^2*x
    jmPutResults(&x, &rx, &ry, &x);                                                   // L = original X, Z = original X
    temporaryInformation = TI_ABC;
    return true;
  }


bool_t fnJM1(uint16_t JM_OPCODE) {
  #if defined(OPTION_ELEC)
    if(JM_OPCODE == 6) {
      return jmDeltaToStar();
    }
    else if(JM_OPCODE == 7) {
      return jmStarToDelta();
    }
    else if(JM_OPCODE == 8) {
      return jmSymToAbc();
    }
    else if(JM_OPCODE == 9) {
      return jmAbcToSym();
    }
    else if(JM_OPCODE == 20) {
      return jmCopyXtoAbc();
    }
    return false;
  #endif // OPTION_ELEC
}
