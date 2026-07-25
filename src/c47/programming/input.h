// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/********************************************//**
 * \file input.h
 ***********************************************/
#if !defined(INPUT_H)
  #define INPUT_H

  void fnInput   (uint16_t regist);
  /**
   * Reports whether the program at a global label starts with MVAR, skipping leading REM steps.
   */
  bool_t isVarMenu  (uint16_t label);
  void fnVarMnu  (uint16_t label);
  void fn42VarMnu(uint16_t label);
  void fnPause   (uint16_t duration);
  void fnKey     (uint16_t regist);
  void fnKeyType (uint16_t regist);
  void fnPutKey  (uint16_t regist);
  void fnEntryQ  (uint16_t unusedButMandatoryParameter);
#endif // !INPUT_H
