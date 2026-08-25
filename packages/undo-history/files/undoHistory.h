// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: Copyright The WP43 and C47 Authors

/**
 * \file undoHistory.h
 * Multi-level undo: a ring of complete pre-operation states layered on top
 * of the existing single-level UNDO buffer. saveForUndo() feeds the ring,
 * fnUndo() walks it when the single-level buffer is spent, REDO walks
 * forward again. Entries are complete states, so a missing (oversized)
 * capture only removes granularity, never correctness.
 */
#if !defined(UNDOHISTORY_H)
  #define UNDOHISTORY_H

  // Ring sizing. The ceiling is upstream's, not ours: matrix.txt RCL58's
  // QR workspace requests one contiguous chunk within ~1400 blocks of the
  // pool's ENTIRE free space (measured: 29820 requested, 31236 available
  // vanilla), so the firmware-wide budget for resident pool allocations is
  // about 5 KiB — this ring takes 4 KiB of it on the new hardware and
  // scales down with the quarter-size old pool.
  #if RAM_SIZE_IN_BLOCKS == RAM_SIZE_IN_BLOCKS_NEW_HW
    #define HISTORY_RING_SIZE_IN_BLOCKS  1024  // 4 KiB
  #else
    #define HISTORY_RING_SIZE_IN_BLOCKS   256  // 1 KiB
  #endif
  #define HISTORY_MAX_ENTRIES              48

  #define HISTORY_ENTRY_LIVEANCHOR       0x01  // pushed at first UNDO so REDO can return to "now"
  #define HISTORY_ENTRY_GAPBEFORE        0x02  // one or more captures before this one were skipped
  #define HISTORY_ENTRY_HAS_SUMS         0x04  // statistical sums payload appended

  /**
   * Feeds the ring from the SAVED_* area. Called from saveForUndo() once
   * every part of the single-level buffer is committed; must never read the
   * live registers.
   */
  void   undoHistoryCapture      (void);

  /**
   * Labels the next capture with the item about to run. Called from
   * reallyRunFunction() next to its saveForUndo() call; every other
   * saveForUndo() caller leaves the label at 0 (rendered generically).
   *
   * \param[in] item Item number about to be executed
   */
  void   undoHistoryNoteFunction (int16_t item);

  /**
   * Pushes the live state as a LIVEANCHOR entry so REDO can come back to
   * the pre-undo point. Called from fnUndo() before the single-level undo()
   * runs; gated on user context and a clean error state, because upstream
   * also uses saveForUndo()/fnUndo() as an internal error-rollback
   * transaction (e.g. RANI#).
   */
  void   undoHistoryNoteFirstUndo(void);

  /**
   * True when a deeper state than the single-level buffer is available.
   */
  bool_t undoHistoryCanStepBack  (void);

  /**
   * Restores the next older ring entry. Called from fnUndo() when
   * thereIsSomethingToUndo is false.
   */
  void   undoHistoryStepBack     (void);

  /**
   * True in direct user context only: not while a program runs and not
   * inside SLV/INTEGRAL, which call fnUndo(0) internally with nothing to
   * undo — stepping into the ring there would restore an ancient state
   * mid-computation.
   */
  bool_t undoHistoryUserContext  (void);

  /**
   * Forgets the ring without freeing (the pool it lived in was rebuilt or
   * replaced) and re-arms it from the current pool. Runs at RESET — right
   * after doFnReset rebuilds the free list, which pins the block to the
   * pool's low edge — and at the end of a state restore.
   */
  void   undoHistoryReset        (void);

  void   fnRedo                  (uint16_t unusedButMandatoryParameter);
  void   fnHistoryClear          (uint16_t unusedButMandatoryParameter);
#endif // !UNDOHISTORY_H
