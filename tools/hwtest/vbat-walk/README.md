# vbat sampling schedule hardware proof (MR !1611)

Proves on DM42/DM42n hardware that the `vbatSampleDelay[]` escalating
schedule (src/c47/config.c) is walked entry by entry: each ADC conversion is
gated by the table entry at the current schedule index, the index advances
per conversion and saturates on the last entry (2500 ms), and a top level
program run start resets the index so the first dispatches sample undelayed.

This code is `DMCP_BUILD` only: the simulator never compiles it, so hardware
is the only place it runs. The proof rig is a firmware patch plus a six step
program; the firmware drives the whole experiment itself by injecting R/S
key presses through the DMCP key buffer, so the injected presses traverse
the same physical key path as a finger.

## Contents

- `vbat-walk-monitor.patch` - the `MONITOR_VBAT_SCHEDULE` instrumentation:
  a 700 event capture buffer recording every ADC conversion (schedule index,
  ms since the previous conversion, vbat) and every schedule reset (tagged
  run start vs USB changeover), a driver that injects R/S after each program
  stop (idle gaps cycling 0.5 s / 2 s / 5 s) for 20 run cycles, and a dump
  of `VBATWALK.CSV` to the FAT root when cycle 20 stops ("BWALK DONE" in X).
- `genbwalk.py` - generates `PROGRAMS/BWALK.p47` (encoder reused from
  tools/bench/genbench.py). BWALK is one run cycle: a 10000 iteration
  DSZ/GTO loop (~14 s on a battery powered DM42n, comfortably past the ~7 s
  the schedule needs to saturate), then STOP; the injected R/S resumes at
  the trailing GTO 'BWALK' so every cycle is a genuine top level run start.
- `analyze_vbatwalk.py` - verifies the CSV cycle by cycle (index marches
  1..16 and holds, every delta at or above its gate) and prints the
  aggregate per position table. Exit 0 only if every cycle proves the walk.
- `results/` - captured CSVs and their verdicts.

## Base

Written against MR !1611 head plus the BAT_MINIMUM sync commit
(`cb73d6c0a`, branch battery/per-dispatch-housekeeping). The patch touches
config.c, config.h, defines.h, c47.c and programming/lblGtoXeq.c and is
fully `#if` gated: with `MONITOR_VBAT_SCHEDULE` undefined the object code
is unchanged.

## Run it

```
git apply tools/hwtest/vbat-walk/vbat-walk-monitor.patch
export PATH="$HOME/.local/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
make dmcp      # DM42:  build.dmcp.p4/src/c47-dmcp/C47.pgm + C47_qspi.bin
make dmcp5     # DM42n: build.dmcp5/src/c47-dmcp5/C47.pg5
```

1. Copy the firmware to the calculator (USB disk mode) and flash via the
   DMCP loader. On the DM42 both `C47.pgm` and `C47_qspi.bin` are needed:
   the QSPI image changed in this MR (the delay table is TO_QSPI). The
   DM42n build keeps everything in flash, so `C47.pg5` alone suffices.
2. Copy `PROGRAMS/BWALK.p47` into the calculator's PROGRAMS directory and
   load it (LOADP). Run on battery power: the schedule is identical on USB,
   but battery reproduces the MR's scenario and arms the low battery stop.
   Optional: start plugged into USB and unplug while idle before starting -
   the capture preamble then shows the USB changeover reset as well.
3. `XEQ 'BWALK'` once and walk away. The calculator runs 20 cycles by
   itself (~6 to 10 minutes depending on model and supply) and shows
   "BWALK DONE" in X when finished.
4. USB disk mode again, copy `VBATWALK.CSV` off the FAT root, then:

```
python3 tools/hwtest/vbat-walk/analyze_vbatwalk.py VBATWALK.CSV
```

## What a pass looks like

Per cycle the delta column reproduces the table entry by entry: samples 2
and 3 arrive undelayed (the 0 ms gates), then two at ~50 ms, ten at
~100 ms, one at ~1000 ms, then ~2500 ms repeating with the index pegged at
16. Cycle 1 is the manual XEQ; cycles 2..20 are injected R/S resumes, each
a genuine top level run start through fnRunProgram(), so the reset at
lblGtoXeq.c is exercised twenty times through the real key path. Deltas sit
a few ms above their gate during a run (dispatch granularity) and up to
~160 ms above it for samples that land in the idle gaps (main loop wake
cadence). Minute pulse conversions bypass the gate by design and are tagged
`sample_minute`, excluded from the gate check.

## Results

- (pending hardware run)

## Notes

- The proof run doubles as a check that the TO_QSPI move of the table
  (0b88661d) reads correctly from QSPI: a wrong table would show up
  directly as wrong gates.
- If key injection ever misbehaves on a future DMCP release, the AUTXEQ
  precedent in c47.c (`runFunction(ITM_RS)`) is the sanctioned programmatic
  fallback; swap it into monitorVbatDriverTick().
- The capture buffer caps at 700 events (~450 expected); the CSV trailer
  line reports `dropped=` so truncation is visible, never silent.
