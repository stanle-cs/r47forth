# Upstream report — `doFnReset` leaves reserved global-register descriptor bits uninitialized

Status: **not yet submitted.**  Carried here after being evicted from
forth-core in the S1 simplification pass (the fix was correct but has
nothing to do with Forth, so it should not ride in this package's patch
set — see DESIGN-HISTORY.md's S1 entry).

## The finding

`doFnReset()` (src/c47/config.c) re-initializes every global register with

```c
    // initialize the global registers
    #if defined(DMCP_BUILD) && defined(OLD_HW)
      memset(globalRegister, 0, sizeof(globalRegister));
    #endif // DMCP_BUILD && OLD_HW
    for(calcRegister_t regist=FIRST_GLOBAL_REGISTER; regist<=LAST_GLOBAL_REGISTER; regist++) {
      setRegisterDataType(regist, dtReal34, amNone);
      ...
    }
```

The `for` loop assigns each descriptor's *defined* fields through the
field setters (`setRegisterDataType`, the data pointer, …).  It never
touches the descriptor's **reserved/undefined bits**.  Those are only
cleared by the `memset`, which is compiled in for `DMCP_BUILD && OLD_HW`
alone — so on every other target (PC simulator, DM42n/DMCP5, DM50) a
reset leaves whatever those bits previously held.

## Why it matters

`saveCalc()` serializes the *whole* `registerHeader_t` descriptor word,
not just the fields the setters define.  So stale reserved bits survive
a reset, get written to the state file, and are restored verbatim on the
next `restoreCalc()`.  Any future upstream use of those bits inherits
undefined values on hardware that has been reset rather than freshly
flashed — and the discrepancy is target-dependent, which is the hard
kind to reproduce.

Today nothing reads them, so this is latent, not user-visible.  That is
exactly why it is worth fixing before something does.

## Suggested fix

Make the clear unconditional and size it to the header, not the whole
array (the register *data* is reallocated by the loop that follows, so
only the headers need zeroing):

```c
    // initialize the global registers, including reserved header bits
    memset(globalRegister, 0, sizeof(registerHeader_t) * NUMBER_OF_GLOBAL_REGISTERS);
```

This subsumes the `OLD_HW` case, so the `#if` guard goes away.

## Reproduction

Poison the reserved bits, reset, and read them back:

```c
    for (uint16_t i = 0; i < NUMBER_OF_GLOBAL_REGISTERS; i++) {
      globalRegister[i].descriptor |= 0xFE000000u;
    }
    doFnReset(CONFIRMED, doNotLoadAutoSav);
    for (uint16_t i = 0; i < NUMBER_OF_GLOBAL_REGISTERS; i++) {
      assert((globalRegister[i].descriptor & 0xFE000000u) == 0);   /* fails today */
    }
```

This ran as part of `test_lifecycle_real_reset_hook` in forth-core's
suite (child exit code 80) until S1 removed it along with the fix.
